/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FilterNodeSoftware.h"
#include "2D.h"
#include "Tools.h"
#include <set>

const ptrdiff_t ARGB32_COMPONENT_BYTEOFFSET_B = 0;
const ptrdiff_t ARGB32_COMPONENT_BYTEOFFSET_G = 1;
const ptrdiff_t ARGB32_COMPONENT_BYTEOFFSET_R = 2;
const ptrdiff_t ARGB32_COMPONENT_BYTEOFFSET_A = 3;

namespace mozilla {
namespace gfx {

// Fast approximate division by 255. It has the property that
// for all 0 <= n <= 255*255, FAST_DIVIDE_BY_255(n) == n/255.
// But it only uses two adds and two shifts instead of an
// integer division (which is expensive on many processors).
//
// equivalent to v/255
template<class B, class A>
static B FastDivideBy255(A v)
{
  return ((v << 8) + v + 255) >> 16;
}

static unsigned
umax(unsigned a, unsigned b)
{
  return a - ((a - b) & -(a < b));
}

static unsigned
umin(unsigned a, unsigned b)
{
  return a - ((a - b) & -(a > b));
}

// from xpcom/string/public/nsAlgorithm.h
template <class T>
inline const T&
clamped(const T& a, const T& min, const T& max)
{
  MOZ_ASSERT(max >= min, "clamped(): max must be greater than or equal to min");
  return std::min(std::max(a, min), max);
}

void
ClearDataSourceSurface(DataSourceSurface *aSurface)
{
  size_t numBytes = aSurface->GetSize().height * aSurface->Stride();
  uint8_t* data = aSurface->GetData();
  for (size_t i = 0; i < numBytes; i++) {
    data[i] = 0;
  }
}

static ptrdiff_t
DataOffset(DataSourceSurface* aSurface, IntPoint aPoint)
{
  return aPoint.y * aSurface->Stride() +
         aPoint.x * BytesPerPixel(aSurface->GetFormat());
}

static void
CopyRect(DataSourceSurface* aSrc, DataSourceSurface* aDest,
         IntRect aSrcRect, IntPoint aDestPoint)
{
  uint8_t* sourceData = aSrc->GetData();
  uint32_t sourceStride = aSrc->Stride();
  uint8_t* destData = aDest->GetData();
  uint32_t destStride = aDest->Stride();

  sourceData += DataOffset(aSrc, aSrcRect.TopLeft());
  destData += DataOffset(aDest, aDestPoint);

  for (int32_t y = 0; y < aSrcRect.height; y++) {
    for (int32_t x = 0; x < aSrcRect.width; x++) {
      *((int32_t*)destData + x) = *((int32_t*)sourceData + x);
    }
    sourceData += sourceStride;
    destData += destStride;
  }
}

static uint32_t
ColorAtPoint(DataSourceSurface* aSurface, const IntPoint &aPoint)
{
  return *(uint32_t*)(aSurface->GetData() + DataOffset(aSurface, aPoint));
}

static void
FillRectWithColor(DataSourceSurface *aSurface, const IntRect &aFillRect, uint32_t aColor)
{
  uint8_t* data = aSurface->GetData();
  int32_t stride = aSurface->Stride();
  data += DataOffset(aSurface, aFillRect.TopLeft());
  for (int32_t y = 0; y < aFillRect.height; y++) {
    for (int32_t x = 0; x < aFillRect.width; x++) {
      *((uint32_t*)data + x) = aColor;
    }
    data += stride;
  }
}

static void
FillRectWithVerticallyRepeatingHorizontalStrip(DataSourceSurface *aSurface,
                                               const IntRect &aFillRect,
                                               const IntRect &aSampleRect)
{
  uint8_t* data = aSurface->GetData();
  int32_t stride = aSurface->Stride();
  uint8_t* sampleData = data + DataOffset(aSurface, aSampleRect.TopLeft());
  data += DataOffset(aSurface, aFillRect.TopLeft());
  for (int32_t y = 0; y < aFillRect.height; y++) {
    for (int32_t x = 0; x < aFillRect.width; x++) {
      *((uint32_t*)data + x) = *((uint32_t*)sampleData + x);
    }
    data += stride;
  }
}

static void
FillRectWithHorizontallyRepeatingVerticalStrip(DataSourceSurface *aSurface,
                                               const IntRect &aFillRect,
                                               const IntRect &aSampleRect)
{
  uint8_t* data = aSurface->GetData();
  int32_t stride = aSurface->Stride();
  uint8_t* sampleData = data + DataOffset(aSurface, aSampleRect.TopLeft());
  data += DataOffset(aSurface, aFillRect.TopLeft());
  for (int32_t y = 0; y < aFillRect.height; y++) {
    int32_t sampleColor = *((uint32_t*)sampleData);
    for (int32_t x = 0; x < aFillRect.width; x++) {
      *((uint32_t*)data + x) = sampleColor;
    }
    data += stride;
    sampleData += stride;
  }
}

static void
DuplicateEdges(DataSourceSurface* aSurface, const IntRect &aFromRect)
{
  IntSize size = aSurface->GetSize();
  IntRect fill;
  IntRect sampleRect;
  for (int32_t ix = 0; ix < 3; ix++) {
    switch (ix) {
      case 0:
        fill.x = 0;
        fill.width = aFromRect.x;
        sampleRect.x = fill.XMost();
        sampleRect.width = 1;
        break;
      case 1:
        fill.x = aFromRect.x;
        fill.width = aFromRect.width;
        sampleRect.x = fill.x;
        sampleRect.width = fill.width;
        break;
      case 2:
        fill.x = aFromRect.XMost();
        fill.width = size.width - fill.x;
        sampleRect.x = fill.x - 1;
        sampleRect.width = 1;
        break;
    }
    if (fill.width <= 0) {
      continue;
    }
    bool xIsMiddle = (ix == 1);
    for (int32_t iy = 0; iy < 3; iy++) {
      switch (iy) {
        case 0:
          fill.y = 0;
          fill.height = aFromRect.y;
          sampleRect.y = fill.YMost();
          sampleRect.height = 1;
          break;
        case 1:
          fill.y = aFromRect.y;
          fill.height = aFromRect.height;
          sampleRect.y = fill.y;
          sampleRect.height = fill.height;
          break;
        case 2:
          fill.y = aFromRect.YMost();
          fill.height = size.height - fill.y;
          sampleRect.y = fill.y - 1;
          sampleRect.height = 1;
          break;
      }
      if (fill.height <= 0) {
        continue;
      }
      bool yIsMiddle = (iy == 1);
      if (!xIsMiddle && !yIsMiddle) {
        // Corner
        FillRectWithColor(aSurface, fill, ColorAtPoint(aSurface, sampleRect.TopLeft()));
      }
      if (xIsMiddle && !yIsMiddle) {
        // Top middle or bottom middle
        FillRectWithVerticallyRepeatingHorizontalStrip(aSurface, fill, sampleRect);
      }
      if (!xIsMiddle && yIsMiddle) {
        // Left middle or right middle
        FillRectWithHorizontallyRepeatingVerticalStrip(aSurface, fill, sampleRect);
      }
    }
  }
}

static IntPoint
TileIndex(const IntRect &aFirstTileRect, const IntPoint &aPoint)
{
  return IntPoint(int32_t(floor(double(aPoint.x - aFirstTileRect.x) / aFirstTileRect.width)),
                  int32_t(floor(double(aPoint.y - aFirstTileRect.y) / aFirstTileRect.height)));
}

static void
TileSurface(DataSourceSurface* aSource, DataSourceSurface* aTarget, const IntPoint &aOffset)
{
  IntRect sourceRect(aOffset, aSource->GetSize());
  IntRect targetRect(IntPoint(0, 0), aTarget->GetSize());
  IntPoint startIndex = TileIndex(sourceRect, targetRect.TopLeft());
  IntPoint endIndex = TileIndex(sourceRect, targetRect.BottomRight());

  for (int32_t ix = startIndex.x; ix <= endIndex.x; ix++) {
    for (int32_t iy = startIndex.y; iy <= endIndex.y; iy++) {
      IntPoint destPoint(sourceRect.x + ix * sourceRect.width,
                         sourceRect.y + iy * sourceRect.height);
      IntRect destRect(destPoint, sourceRect.Size());
      destRect = destRect.Intersect(targetRect);
      IntRect srcRect = destRect - destPoint;
      CopyRect(aSource, aTarget, srcRect, destRect.TopLeft());
    }
  }
}

static TemporaryRef<DataSourceSurface>
GetDataSurfaceInRect(SourceSurface *aSurface,
                     const IntRect &aSurfaceRect,
                     const IntRect &aDestRect,
                     ConvolveMatrixEdgeMode aEdgeMode)
{
  MOZ_ASSERT(aSurfaceRect.Size() == aSurface->GetSize());
  RefPtr<DataSourceSurface> dataSource = aSurface->GetDataSurface();
  IntRect sourceRect = aSurfaceRect;

  if (sourceRect.IsEqualEdges(aDestRect)) {
    return dataSource;
  }

  IntRect intersect = sourceRect.Intersect(aDestRect);
  IntRect intersectInSourceSpace = intersect - sourceRect.TopLeft();
  IntRect intersectInDestSpace = intersect - aDestRect.TopLeft();

  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(aDestRect.Size(), FORMAT_B8G8R8A8);

  if (aEdgeMode == EDGE_MODE_WRAP) {
    TileSurface(dataSource, target, intersectInDestSpace.TopLeft());
    return target;
  }

  if (aEdgeMode == EDGE_MODE_NONE) {
    ClearDataSourceSurface(target);
  }

  CopyRect(dataSource, target, intersectInSourceSpace,
           intersectInDestSpace.TopLeft());

  if (aEdgeMode == EDGE_MODE_DUPLICATE) {
    DuplicateEdges(target, intersectInDestSpace);
  }

  return target;
}

/* static */ TemporaryRef<FilterNode>
FilterNodeSoftware::Create(FilterType aType)
{
  RefPtr<FilterNodeSoftware> filter;
  switch (aType) {
    case FILTER_BLEND:
      filter = new FilterNodeBlendSoftware();
      break;
    case FILTER_MORPHOLOGY:
      filter = new FilterNodeMorphologySoftware();
      break;
    case FILTER_COLOR_MATRIX:
      filter = new FilterNodeColorMatrixSoftware();
      break;
    case FILTER_FLOOD:
      filter = new FilterNodeFloodSoftware();
      break;
    case FILTER_TILE:
      filter = new FilterNodeTileSoftware();
      break;
    case FILTER_TABLE_TRANSFER:
      filter = new FilterNodeTableTransferSoftware();
      break;
    case FILTER_DISCRETE_TRANSFER:
      filter = new FilterNodeDiscreteTransferSoftware();
      break;
    case FILTER_LINEAR_TRANSFER:
      filter = new FilterNodeLinearTransferSoftware();
      break;
    case FILTER_GAMMA_TRANSFER:
      filter = new FilterNodeGammaTransferSoftware();
      break;
    case FILTER_CONVOLVE_MATRIX:
      filter = new FilterNodeConvolveMatrixSoftware();
      break;
    case FILTER_OFFSET:
      filter = new FilterNodeOffsetSoftware();
      break;
    case FILTER_DISPLACEMENT_MAP:
      filter = new FilterNodeDisplacementMapSoftware();
      break;
    case FILTER_TURBULENCE:
      filter = new FilterNodeTurbulenceSoftware();
      break;
    case FILTER_ARITHMETIC_COMBINE:
      filter = new FilterNodeArithmeticCombineSoftware();
      break;
    case FILTER_COMPOSITE:
      filter = new FilterNodeCompositeSoftware();
      break;
    case FILTER_GAUSSIAN_BLUR:
      filter = new FilterNodeGaussianBlurSoftware();
      break;
    case FILTER_DIRECTIONAL_BLUR:
      filter = new FilterNodeDirectionalBlurSoftware();
      break;
    case FILTER_CROP:
      filter = new FilterNodeCropSoftware();
      break;
    case FILTER_PREMULTIPLY:
      filter = new FilterNodePremultiplySoftware();
      break;
    case FILTER_UNPREMULTIPLY:
      filter = new FilterNodeUnpremultiplySoftware();
      break;
    case FILTER_POINT_DIFFUSE:
      filter = new FilterNodeDiffuseSoftware(new PointLightSoftware());
      break;
    case FILTER_POINT_SPECULAR:
      filter = new FilterNodeSpecularSoftware(new PointLightSoftware());
      break;
    case FILTER_SPOT_DIFFUSE:
      filter = new FilterNodeDiffuseSoftware(new SpotLightSoftware());
      break;
    case FILTER_SPOT_SPECULAR:
      filter = new FilterNodeSpecularSoftware(new SpotLightSoftware());
      break;
    case FILTER_DISTANT_DIFFUSE:
      filter = new FilterNodeDiffuseSoftware(new DistantLightSoftware());
      break;
    case FILTER_DISTANT_SPECULAR:
      filter = new FilterNodeSpecularSoftware(new DistantLightSoftware());
      break;
  }
  return filter;
}

void
FilterNodeSoftware::Draw(DrawTarget* aDrawTarget,
                         const Rect &aSourceRect,
                         const Point &aDestPoint,
                         const DrawOptions &aOptions)
{
  Rect renderRect = aSourceRect;
  renderRect.RoundOut();
  IntRect renderIntRect(int32_t(renderRect.x), int32_t(renderRect.y),
                        int32_t(renderRect.width), int32_t(renderRect.height));
  RefPtr<DataSourceSurface> result = Render(renderIntRect);
  aDrawTarget->DrawSurface(result, Rect(aDestPoint, aSourceRect.Size()),
                           aSourceRect - renderRect.TopLeft(),
                           DrawSurfaceOptions(), aOptions);
}

TemporaryRef<DataSourceSurface>
FilterNodeSoftware::GetInputDataSourceSurface(uint32_t aInputEnumIndex,
                                              const IntRect& aRect,
                                              ConvolveMatrixEdgeMode aEdgeMode)
{
  int32_t inputIndex = InputIndex(aInputEnumIndex);
  if (inputIndex < 0 || (uint32_t)inputIndex >= NumberOfSetInputs()) {
    MOZ_CRASH();
    return nullptr;
  }
  RefPtr<SourceSurface> surface;
  IntRect surfaceRect;
  if (mInputSurfaces[inputIndex]) {
    surface = mInputSurfaces[inputIndex];
    surfaceRect = IntRect(IntPoint(0, 0), surface->GetSize());
  } else {
    RefPtr<FilterNodeSoftware> filter = mInputFilters[inputIndex];
    MOZ_ASSERT(filter, "missing input");
    IntRect inputFilterOutput = filter->GetOutputRectInRect(aRect);
    surface = filter->Render(inputFilterOutput);
    surfaceRect = inputFilterOutput;
    MOZ_ASSERT(surfaceRect.Size() == surface->GetSize());
  }
  RefPtr<DataSourceSurface> result = GetDataSurfaceInRect(surface, surfaceRect, aRect, aEdgeMode);
  MOZ_ASSERT(result->GetSize() == aRect.Size(), "wrong surface size");
  return result;
}

IntRect
FilterNodeSoftware::GetInputRectInRect(uint32_t aInputEnumIndex,
                                       const IntRect &aInRect)
{
  int32_t inputIndex = InputIndex(aInputEnumIndex);
  if (inputIndex < 0 || (uint32_t)inputIndex >= NumberOfSetInputs()) {
    MOZ_CRASH();
    return IntRect();
  }
  if (mInputSurfaces[inputIndex]) {
    return aInRect.Intersect(IntRect(IntPoint(0, 0),
                                     mInputSurfaces[inputIndex]->GetSize()));
  }
  RefPtr<FilterNodeSoftware> filter = mInputFilters[inputIndex];
  MOZ_ASSERT(filter, "missing input");
  return filter->GetOutputRectInRect(aInRect);
}

size_t
FilterNodeSoftware::NumberOfSetInputs()
{
  return std::max(mInputSurfaces.size(), mInputFilters.size());
}

void
FilterNodeSoftware::SetInput(uint32_t aIndex, FilterNode *aFilter)
{
  if (aFilter->GetBackendType() != FILTER_BACKEND_SOFTWARE) {
    MOZ_ASSERT(false, "can only take software filters as inputs");
    return;
  }
  SetInput(aIndex, nullptr, static_cast<FilterNodeSoftware*>(aFilter));
}

void
FilterNodeSoftware::SetInput(uint32_t aIndex, SourceSurface *aSurface)
{
  SetInput(aIndex, aSurface, nullptr);
}

void
FilterNodeSoftware::SetInput(uint32_t aInputEnumIndex,
                             SourceSurface *aSurface,
                             FilterNodeSoftware *aFilter)
{
  int32_t inputIndex = InputIndex(aInputEnumIndex);
  if (inputIndex < 0) {
    MOZ_CRASH();
    return;
  }
  if ((uint32_t)inputIndex <= mInputSurfaces.size()) {
    mInputSurfaces.resize(inputIndex + 1);
  }
  if ((uint32_t)inputIndex <= mInputFilters.size()) {
    mInputFilters.resize(inputIndex + 1);
  }
  mInputSurfaces[inputIndex] = aSurface;
  mInputFilters[inputIndex] = aFilter;
}

FilterNodeBlendSoftware::FilterNodeBlendSoftware()
 : mBlendMode(BLEND_MODE_MULTIPLY)
{}

int32_t
FilterNodeBlendSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_BLEND_IN: return 0;
    case IN_BLEND_IN2: return 1;
    default: return -1;
  }
}

void
FilterNodeBlendSoftware::SetAttribute(uint32_t aIndex, uint32_t aBlendMode)
{
  MOZ_ASSERT(aIndex == ATT_BLEND_BLENDMODE);
  mBlendMode = static_cast<BlendMode>(aBlendMode);
}

static TemporaryRef<DataSourceSurface>
ApplyBlendFilter(DataSourceSurface* aInput1, DataSourceSurface* aInput2, uint32_t aBlendMode)
{
  IntSize size = aInput1->GetSize();
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(size, FORMAT_B8G8R8A8);

  uint8_t* sourceData1 = aInput1->GetData();
  uint8_t* sourceData2 = aInput2->GetData();
  uint8_t* targetData = target->GetData();
  uint32_t stride = target->Stride();

  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x++) {
      uint32_t targIndex = y * stride + 4 * x;
      uint32_t qa = sourceData1[targIndex + ARGB32_COMPONENT_BYTEOFFSET_A];
      uint32_t qb = sourceData2[targIndex + ARGB32_COMPONENT_BYTEOFFSET_A];
      for (int32_t i = std::min(ARGB32_COMPONENT_BYTEOFFSET_B, ARGB32_COMPONENT_BYTEOFFSET_R);
           i <= std::max(ARGB32_COMPONENT_BYTEOFFSET_B, ARGB32_COMPONENT_BYTEOFFSET_R); i++) {
        uint32_t ca = sourceData1[targIndex + i];
        uint32_t cb = sourceData2[targIndex + i];
        uint32_t val;
        switch (aBlendMode) {
          case BLEND_MODE_MULTIPLY:
            val = ((255 - qa) * cb + (255 - qb + cb) * ca);
            break;
          case BLEND_MODE_SCREEN:
            val = 255 * (cb + ca) - ca * cb;
            break;
          case BLEND_MODE_DARKEN:
            val = umin((255 - qa) * cb + 255 * ca,
                       (255 - qb) * ca + 255 * cb);
            break;
          case BLEND_MODE_LIGHTEN:
            val = umax((255 - qa) * cb + 255 * ca,
                       (255 - qb) * ca + 255 * cb);
            break;
          default:
            MOZ_CRASH();
        }
        val = umin(FastDivideBy255<unsigned>(val), 255U);
        targetData[targIndex + i] = static_cast<uint8_t>(val);
      }
      uint32_t alpha = 255 * 255 - (255 - qa) * (255 - qb);
      targetData[targIndex + ARGB32_COMPONENT_BYTEOFFSET_A] =
        FastDivideBy255<uint8_t>(alpha);
    }
  }

  return target;
}

TemporaryRef<DataSourceSurface>
FilterNodeBlendSoftware::Render(const IntRect& aRect)
{
  RefPtr<DataSourceSurface> input1 =
    GetInputDataSourceSurface(IN_BLEND_IN, aRect);
  RefPtr<DataSourceSurface> input2 =
    GetInputDataSourceSurface(IN_BLEND_IN2, aRect);
  return ApplyBlendFilter(input1, input2, mBlendMode);
}

IntRect
FilterNodeBlendSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  return GetInputRectInRect(IN_BLEND_IN, aRect).Union(
    GetInputRectInRect(IN_BLEND_IN2, aRect)).Intersect(aRect);
}

FilterNodeMorphologySoftware::FilterNodeMorphologySoftware()
 : mOperator(MORPHOLOGY_OPERATOR_ERODE)
{}

int32_t
FilterNodeMorphologySoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_MORPHOLOGY_IN: return 0;
    default: return -1;
  }
}

void
FilterNodeMorphologySoftware::SetAttribute(uint32_t aIndex,
                                           const IntSize &aRadii)
{
  MOZ_ASSERT(aIndex == ATT_MORPHOLOGY_RADII);
  mRadii.width = clamped(aRadii.width, 0, 100000);
  mRadii.height = clamped(aRadii.height, 0, 100000);
}

void
FilterNodeMorphologySoftware::SetAttribute(uint32_t aIndex,
                                           uint32_t aOperator)
{
  MOZ_ASSERT(aIndex == ATT_MORPHOLOGY_OPERATOR);
  mOperator = static_cast<MorphologyOperator>(aOperator);
}

template<MorphologyOperator Operator>
static TemporaryRef<DataSourceSurface>
DoMorphologyWithRepeatedKernelTraversal(const IntRect& aSourceRect,
                                        DataSourceSurface* aInput,
                                        const IntRect& aDestRect,
                                        int32_t rx,
                                        int32_t ry)
{
  MOZ_STATIC_ASSERT(Operator == MORPHOLOGY_OPERATOR_ERODE ||
                    Operator == MORPHOLOGY_OPERATOR_DILATE,
                    "unexpected morphology operator");

  IntRect srcRect = aSourceRect - aDestRect.TopLeft();
  IntRect destRect = aDestRect - aDestRect.TopLeft();
#ifdef DEBUG
  Margin margin = srcRect - destRect;
  MOZ_ASSERT(margin.top >= ry && margin.right >= rx &&
             margin.bottom >= ry && margin.left >= rx, "insufficient margin");
#endif

  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(destRect.Size(), FORMAT_B8G8R8A8);

  uint8_t* sourceData = aInput->GetData();
  int32_t sourceStride = aInput->Stride();
  uint8_t* targetData = target->GetData();
  int32_t targetStride = target->Stride();

  IntPoint offset = destRect.TopLeft() - srcRect.TopLeft();
  sourceData += DataOffset(aInput, offset);

  // Scan the kernel for each pixel to determine max/min RGBA values.
  int32_t startY = destRect.y - ry;
  int32_t endY = destRect.y + ry;
  for (int32_t y = destRect.y; y < destRect.YMost(); y++, startY++, endY++) {
    int32_t startX = destRect.x - rx;
    int32_t endX = destRect.x + rx;
    for (int32_t x = destRect.x; x < destRect.XMost(); x++, startX++, endX++) {
      uint8_t u[4];
      for (size_t i = 0; i < 4; i++) {
        u[i] = Operator == MORPHOLOGY_OPERATOR_ERODE ? 255 : 0;
      }
      for (int32_t iy = startY; iy <= endY; iy++) {
        for (int32_t ix = startX; ix <= endX; ix++) {
          int32_t sourceIndex = iy * sourceStride + 4 * ix;
          for (size_t i = 0; i < 4; i++) {
            if (Operator == MORPHOLOGY_OPERATOR_ERODE) {
              u[i] = umin(u[i], sourceData[sourceIndex + i]);
            } else {
              u[i] = umax(u[i], sourceData[sourceIndex + i]);
            }
          }
        }
      }

      int32_t targIndex = y * targetStride + 4 * x;
      for (size_t i = 0; i < 4; i++) {
        targetData[targIndex+i] = u[i];
      }
    }
  }

  return target;
}

// Calculates, in constant time, the lowest value between 0 and 255
// for which aValueCounts[value] != 0.
static uint8_t
FindMinNonZero(uint32_t aValueCounts[256])
{
  bool found = false;
  uint8_t foundValue = 0;
  for (int32_t value = 0; value < 256; value++) {
    bool valueCountIsNonZero = aValueCounts[value];
    foundValue += !found * valueCountIsNonZero * value;
    found = found || valueCountIsNonZero;
  }
  return foundValue;
}

// Calculates, in constant time, the highest value between 0 and 255
// for which aValueCounts[value] != 0.
static uint8_t
FindMaxNonZero(uint32_t aValueCounts[256])
{
  bool found = false;
  uint8_t foundValue = 0;
  for (int32_t value = 255; value >= 0; value--) {
    bool valueCountIsNonZero = aValueCounts[value];
    foundValue += !found * valueCountIsNonZero * value;
    found = found || valueCountIsNonZero;
  }
  return foundValue;
}

template<MorphologyOperator Operator>
static TemporaryRef<DataSourceSurface>
DoMorphologyWithCachedKernel(const IntRect& aSourceRect,
                             DataSourceSurface* aInput,
                             const IntRect& aDestRect,
                             int32_t rx,
                             int32_t ry)
{
  MOZ_STATIC_ASSERT(Operator == MORPHOLOGY_OPERATOR_ERODE ||
                    Operator == MORPHOLOGY_OPERATOR_DILATE,
                    "unexpected morphology operator");

  IntRect srcRect = aSourceRect - aDestRect.TopLeft();
  IntRect destRect = aDestRect - aDestRect.TopLeft();
#ifdef DEBUG
  Margin margin = srcRect - destRect;
  MOZ_ASSERT(margin.top >= ry && margin.right >= rx &&
             margin.bottom >= ry && margin.left >= rx, "insufficient margin");
#endif

  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(destRect.Size(), FORMAT_B8G8R8A8);

  uint8_t* sourceData = aInput->GetData();
  int32_t sourceStride = aInput->Stride();
  uint8_t* targetData = target->GetData();
  int32_t targetStride = target->Stride();

  IntPoint offset = destRect.TopLeft() - srcRect.TopLeft();
  sourceData += DataOffset(aInput, offset);

  int32_t kernelStartY = destRect.y - ry;
  int32_t kernelEndY = destRect.y + ry;

  for (int32_t y = destRect.y; y < destRect.YMost(); y++, kernelStartY++, kernelEndY++) {

    int32_t kernelStartX = destRect.x - rx;
    int32_t kernelEndX = destRect.x + rx;

    // For target pixel (x,y) the kernel spans
    // [kernelStartX, kernelEndX] x [kernelStartY, kernelEndY].

    // valueCounts[i][value] is the number of occurrences of value
    // in the kernel for component i.
    uint32_t valueCounts[4][256];

    // Initialize to zero.
    for (int32_t i = 0; i < 4; i++) {
      for (int32_t c = 0; c < 256; c++) {
        valueCounts[i][c] = 0;
      }
    }

    // For each target pixel row, traverse the whole kernel once for the
    // first target pixel in the row. Later, when moving through the row,
    // only the columns which enter and exit the kernel will be processed.
    for (int32_t ky = kernelStartY; ky <= kernelEndY; ky++) {
      for (int32_t kx = kernelStartX; kx <= kernelEndX; kx++) {
        for (int32_t i = 0; i < 4; i++) {
          uint8_t valueToAdd = sourceData[ky * sourceStride + 4 * kx + i];
          valueCounts[i][valueToAdd]++;
        }
      }
    }

    for (int32_t x = destRect.x; x < destRect.XMost(); x++, kernelStartX++, kernelEndX++) {

      int32_t targIndex = y * targetStride + 4 * x;

      // Calculate the values for the four components of the target pixel.
      for (size_t i = 0; i < 4; i++) {
        if (Operator == MORPHOLOGY_OPERATOR_ERODE) {
          targetData[targIndex+i] = FindMinNonZero(valueCounts[i]);
        } else {
          targetData[targIndex+i] = FindMaxNonZero(valueCounts[i]);
        }
      }

      // For subsequent pixels in this row, only process the values at the
      // left and right edges of the kernel.
      if (x + 1 < destRect.XMost()) {
        for (int32_t ky = kernelStartY; ky <= kernelEndY; ky++) {
          for (int32_t i = 0; i < 4; i++) {
            // Add the new value from column kernelEndX + 1, which is entering the kernel.
            uint8_t valueToAdd = sourceData[ky * sourceStride + 4 * (kernelEndX + 1) + i];
            valueCounts[i][valueToAdd]++;

            // Remove the old value from column kernelStartX, which is leaving the kernel.
            uint8_t valueToRemove = sourceData[ky * sourceStride + 4 * kernelStartX + i];
            valueCounts[i][valueToRemove]--;
          }
        }
      }

    }
  }

  return target;
}

TemporaryRef<DataSourceSurface>
FilterNodeMorphologySoftware::Render(const IntRect& aRect)
{
  IntRect srcRect = aRect;
  srcRect.Inflate(mRadii);

  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_MORPHOLOGY_IN, srcRect);

  int32_t rx = mRadii.width;
  int32_t ry = mRadii.height;

  if (rx == 0 && ry == 0) {
    return input;
  }

  int32_t kernelSize = (2 * rx + 1) * (2 * ry + 1);

  // TODO: Find the right number
  if (kernelSize < 16) {
    if (mOperator == MORPHOLOGY_OPERATOR_ERODE) {
      return DoMorphologyWithRepeatedKernelTraversal<MORPHOLOGY_OPERATOR_ERODE>(srcRect, input, aRect, rx, ry);
    } else {
      return DoMorphologyWithRepeatedKernelTraversal<MORPHOLOGY_OPERATOR_DILATE>(srcRect, input, aRect, rx, ry);
    }
  } else {
    if (mOperator == MORPHOLOGY_OPERATOR_ERODE) {
      return DoMorphologyWithCachedKernel<MORPHOLOGY_OPERATOR_ERODE>(srcRect, input, aRect, rx, ry);
    } else {
      return DoMorphologyWithCachedKernel<MORPHOLOGY_OPERATOR_DILATE>(srcRect, input, aRect, rx, ry);
    }
  }
}

IntRect
FilterNodeMorphologySoftware::GetOutputRectInRect(const IntRect& aRect)
{
  IntRect inflatedSourceRect = aRect;
  inflatedSourceRect.Inflate(mRadii);
  IntRect inputRect = GetInputRectInRect(IN_MORPHOLOGY_IN, inflatedSourceRect);
  if (mOperator == MORPHOLOGY_OPERATOR_ERODE) {
    inputRect.Deflate(mRadii);
  } else {
    inputRect.Inflate(mRadii);
  }
  return inputRect.Intersect(aRect);
}

int32_t
FilterNodeColorMatrixSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_COLOR_MATRIX_IN: return 0;
    default: return -1;
  }
}

void
FilterNodeColorMatrixSoftware::SetAttribute(uint32_t aIndex,
                                            const Matrix5x4 &aMatrix)
{
  MOZ_ASSERT(aIndex == ATT_COLOR_MATRIX_MATRIX);
  mMatrix = aMatrix;
}

static TemporaryRef<DataSourceSurface>
ApplyColorMatrixFilter(DataSourceSurface* aInput, const Matrix5x4 &aMatrix)
{
  const Float *row1 = &aMatrix._11;
  const Float *row2 = &aMatrix._21;
  const Float *row3 = &aMatrix._31;
  const Float *row4 = &aMatrix._41;
  const Float *row5 = &aMatrix._51;

  IntSize size = aInput->GetSize();
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(size, FORMAT_B8G8R8A8);

  uint8_t* sourceData = aInput->GetData();
  uint8_t* targetData = target->GetData();
  uint32_t stride = target->Stride();

  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x++) {
      uint32_t targIndex = y * stride + 4 * x;

      Float col[4];
      for (int i = 0; i < 4; i++) {
        col[i] =
          sourceData[targIndex + ARGB32_COMPONENT_BYTEOFFSET_R] * row1[i] +
          sourceData[targIndex + ARGB32_COMPONENT_BYTEOFFSET_G] * row2[i] +
          sourceData[targIndex + ARGB32_COMPONENT_BYTEOFFSET_B] * row3[i] +
          sourceData[targIndex + ARGB32_COMPONENT_BYTEOFFSET_A] * row4[i] +
          255 *                                                   row5[i];
        col[i] = clamped(col[i], 0.f, 255.f);
      }
      targetData[targIndex + ARGB32_COMPONENT_BYTEOFFSET_R] =
        static_cast<uint8_t>(col[0]);
      targetData[targIndex + ARGB32_COMPONENT_BYTEOFFSET_G] =
        static_cast<uint8_t>(col[1]);
      targetData[targIndex + ARGB32_COMPONENT_BYTEOFFSET_B] =
        static_cast<uint8_t>(col[2]);
      targetData[targIndex + ARGB32_COMPONENT_BYTEOFFSET_A] =
        static_cast<uint8_t>(col[3]);
    }
  }

  return target;
}

TemporaryRef<DataSourceSurface>
FilterNodeColorMatrixSoftware::Render(const IntRect& aRect)
{
  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_COLOR_MATRIX_IN, aRect);
  return ApplyColorMatrixFilter(input, mMatrix);
}

IntRect
FilterNodeColorMatrixSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  return GetInputRectInRect(IN_COLOR_MATRIX_IN, aRect);
}

void
FilterNodeFloodSoftware::SetAttribute(uint32_t aIndex, const Color &aColor)
{
  MOZ_ASSERT(aIndex == ATT_FLOOD_COLOR);
  mColor = aColor;
}

static uint32_t
ColorToBGRA(const Color& aColor)
{
  union {
    uint32_t color;
    uint8_t components[4];
  };
  components[ARGB32_COMPONENT_BYTEOFFSET_R] = uint8_t(aColor.r * 255.0f);
  components[ARGB32_COMPONENT_BYTEOFFSET_G] = uint8_t(aColor.g * 255.0f);
  components[ARGB32_COMPONENT_BYTEOFFSET_B] = uint8_t(aColor.b * 255.0f);
  components[ARGB32_COMPONENT_BYTEOFFSET_A] = uint8_t(aColor.a * 255.0f);
  return color;
}

TemporaryRef<DataSourceSurface>
FilterNodeFloodSoftware::Render(const IntRect& aRect)
{
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(aRect.Size(), FORMAT_B8G8R8A8);

  uint32_t color = ColorToBGRA(mColor);
  uint8_t* targetData = target->GetData();
  uint32_t stride = target->Stride();

  for (int32_t y = 0; y < aRect.height; y++) {
    for (int32_t x = 0; x < aRect.width; x++) {
      *((uint32_t*)targetData + x) = color;
    }
    targetData += stride;
  }

  return target;
}

IntRect
FilterNodeFloodSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  return aRect;
}

int32_t
FilterNodeTileSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_TILE_IN: return 0;
    default: return -1;
  }
}

void
FilterNodeTileSoftware::SetAttribute(uint32_t aIndex,
                                     const Rect &aSourceRect)
{
  MOZ_ASSERT(aIndex == ATT_TILE_SOURCE_RECT);
  Rect srcRect = aSourceRect;
  srcRect.Round();
  mSourceRect = IntRect(int32_t(srcRect.x), int32_t(srcRect.y),
                        int32_t(srcRect.width), int32_t(srcRect.height));
}

TemporaryRef<DataSourceSurface>
FilterNodeTileSoftware::Render(const IntRect& aRect)
{
  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_TILE_IN, mSourceRect);
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(aRect.Size(), FORMAT_B8G8R8A8);

  TileSurface(input, target, mSourceRect.TopLeft() - aRect.TopLeft());

  return target;
}

IntRect
FilterNodeTileSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  return aRect;
}

template<ptrdiff_t ComponentOffset>
static void CopyComponent(DataSourceSurface* aInput, DataSourceSurface* aTarget)
{
  IntSize size = aInput->GetSize();

  uint8_t* sourceData = aInput->GetData();
  uint8_t* targetData = aTarget->GetData();
  uint32_t stride = aTarget->Stride();

  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x++) {
      uint32_t index = y * stride + x * 4 + ComponentOffset;
      targetData[index] = sourceData[index];
    }
  }
}

template<ptrdiff_t ComponentOffset>
static void TransferComponent(DataSourceSurface* aInput,
                              DataSourceSurface* aTarget,
                              uint8_t aLookupTable[256])
{
  IntSize size = aInput->GetSize();

  uint8_t* sourceData = aInput->GetData();
  uint8_t* targetData = aTarget->GetData();
  uint32_t stride = aTarget->Stride();

  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x++) {
      uint32_t index = y * stride + x * 4 + ComponentOffset;
      targetData[index] = aLookupTable[sourceData[index]];
    }
  }
}

FilterNodeComponentTransferSoftware::FilterNodeComponentTransferSoftware()
 : mDisableR(true)
 , mDisableG(true)
 , mDisableB(true)
 , mDisableA(true)
{}

template<ptrdiff_t ComponentOffset>
void
FilterNodeComponentTransferSoftware::ApplyComponentTransfer(DataSourceSurface* aInput,
                                                            DataSourceSurface* aTarget,
                                                            bool aDisabled)
{
  if (aDisabled) {
    CopyComponent<ComponentOffset>(aInput, aTarget);
  } else {
    uint8_t lookupTable[256];
    GenerateLookupTable(ComponentOffset, lookupTable);
    TransferComponent<ComponentOffset>(aInput, aTarget, lookupTable);
  }
}

TemporaryRef<DataSourceSurface>
FilterNodeComponentTransferSoftware::Render(const IntRect& aRect)
{
  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_TABLE_TRANSFER_IN, aRect);
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(aRect.Size(), FORMAT_B8G8R8A8);

  ApplyComponentTransfer<ARGB32_COMPONENT_BYTEOFFSET_R>(input, target, mDisableR);
  ApplyComponentTransfer<ARGB32_COMPONENT_BYTEOFFSET_G>(input, target, mDisableG);
  ApplyComponentTransfer<ARGB32_COMPONENT_BYTEOFFSET_B>(input, target, mDisableB);
  ApplyComponentTransfer<ARGB32_COMPONENT_BYTEOFFSET_A>(input, target, mDisableA);

  return target;
}

IntRect
FilterNodeComponentTransferSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  return GetInputRectInRect(IN_COLOR_MATRIX_IN, aRect);
}

int32_t
FilterNodeTableTransferSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_TABLE_TRANSFER_IN: return 0;
    default: return -1;
  }
}

void
FilterNodeTableTransferSoftware::SetAttribute(uint32_t aIndex,
                                              bool aDisable)
{
  switch (aIndex) {
    case ATT_TABLE_TRANSFER_DISABLE_R:
      mDisableR = aDisable;
      break;
    case ATT_TABLE_TRANSFER_DISABLE_G:
      mDisableG = aDisable;
      break;
    case ATT_TABLE_TRANSFER_DISABLE_B:
      mDisableB = aDisable;
      break;
    case ATT_TABLE_TRANSFER_DISABLE_A:
      mDisableA = aDisable;
      break;
    default:
      MOZ_CRASH();
  }
}

void
FilterNodeTableTransferSoftware::SetAttribute(uint32_t aIndex,
                                              const Float* aFloat,
                                              uint32_t aSize)
{
  std::vector<Float> table(aFloat, aFloat + aSize);
  switch (aIndex) {
    case ATT_TABLE_TRANSFER_TABLE_R:
      mTableR = table;
      break;
    case ATT_TABLE_TRANSFER_TABLE_G:
      mTableG = table;
      break;
    case ATT_TABLE_TRANSFER_TABLE_B:
      mTableB = table;
      break;
    case ATT_TABLE_TRANSFER_TABLE_A:
      mTableA = table;
      break;
    default:
      MOZ_CRASH();
  }
}

void
FilterNodeTableTransferSoftware::GenerateLookupTable(ptrdiff_t aComponent,
                                                     uint8_t aTable[256])
{
  switch (aComponent) {
    case ARGB32_COMPONENT_BYTEOFFSET_R:
      GenerateLookupTable(mTableR, aTable);
      break;
    case ARGB32_COMPONENT_BYTEOFFSET_G:
      GenerateLookupTable(mTableG, aTable);
      break;
    case ARGB32_COMPONENT_BYTEOFFSET_B:
      GenerateLookupTable(mTableB, aTable);
      break;
    case ARGB32_COMPONENT_BYTEOFFSET_A:
      GenerateLookupTable(mTableA, aTable);
      break;
    default:
      MOZ_ASSERT(false, "unknown component");
      break;
  }
}

void
FilterNodeTableTransferSoftware::GenerateLookupTable(std::vector<Float>& aTableValues,
                                                     uint8_t aTable[256])
{
  uint32_t tvLength = aTableValues.size();
  if (tvLength < 2) {
    return;
  }

  for (size_t i = 0; i < 256; i++) {
    uint32_t k = (i * (tvLength - 1)) / 255;
    Float v1 = aTableValues[k];
    Float v2 = aTableValues[std::min(k + 1, tvLength - 1)];
    int32_t val =
      int32_t(255 * (v1 + (i/255.0f - k/float(tvLength-1))*(tvLength - 1)*(v2 - v1)));
    val = std::min(255, val);
    val = std::max(0, val);
    aTable[i] = val;
  }
}

int32_t
FilterNodeDiscreteTransferSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_DISCRETE_TRANSFER_IN: return 0;
    default: return -1;
  }
}

void
FilterNodeDiscreteTransferSoftware::SetAttribute(uint32_t aIndex,
                                               bool aDisable)
{
  switch (aIndex) {
    case ATT_DISCRETE_TRANSFER_DISABLE_R:
      mDisableR = aDisable;
      break;
    case ATT_DISCRETE_TRANSFER_DISABLE_G:
      mDisableG = aDisable;
      break;
    case ATT_DISCRETE_TRANSFER_DISABLE_B:
      mDisableB = aDisable;
      break;
    case ATT_DISCRETE_TRANSFER_DISABLE_A:
      mDisableA = aDisable;
      break;
    default:
      MOZ_CRASH();
  }
}

void
FilterNodeDiscreteTransferSoftware::SetAttribute(uint32_t aIndex,
                                              const Float* aFloat,
                                              uint32_t aSize)
{
  std::vector<Float> discrete(aFloat, aFloat + aSize);
  switch (aIndex) {
    case ATT_DISCRETE_TRANSFER_TABLE_R:
      mTableR = discrete;
      break;
    case ATT_DISCRETE_TRANSFER_TABLE_G:
      mTableG = discrete;
      break;
    case ATT_DISCRETE_TRANSFER_TABLE_B:
      mTableB = discrete;
      break;
    case ATT_DISCRETE_TRANSFER_TABLE_A:
      mTableA = discrete;
      break;
    default:
      MOZ_CRASH();
  }
}

void
FilterNodeDiscreteTransferSoftware::GenerateLookupTable(ptrdiff_t aComponent,
                                                        uint8_t aTable[256])
{
  switch (aComponent) {
    case ARGB32_COMPONENT_BYTEOFFSET_R:
      GenerateLookupTable(mTableR, aTable);
      break;
    case ARGB32_COMPONENT_BYTEOFFSET_G:
      GenerateLookupTable(mTableG, aTable);
      break;
    case ARGB32_COMPONENT_BYTEOFFSET_B:
      GenerateLookupTable(mTableB, aTable);
      break;
    case ARGB32_COMPONENT_BYTEOFFSET_A:
      GenerateLookupTable(mTableA, aTable);
      break;
    default:
      MOZ_ASSERT(false, "unknown component");
      break;
  }
}

void
FilterNodeDiscreteTransferSoftware::GenerateLookupTable(std::vector<Float>& aTableValues,
                                                        uint8_t aTable[256])
{
  uint32_t tvLength = aTableValues.size();
  if (tvLength < 1) {
    return;
  }

  for (size_t i = 0; i < 256; i++) {
    uint32_t k = (i * tvLength) / 255;
    k = std::min(k, tvLength - 1);
    Float v = aTableValues[k];
    int32_t val = int32_t(255 * v);
    val = std::min(255, val);
    val = std::max(0, val);
    aTable[i] = val;
  }
}

FilterNodeLinearTransferSoftware::FilterNodeLinearTransferSoftware()
 : mSlopeR(0)
 , mSlopeG(0)
 , mSlopeB(0)
 , mSlopeA(0)
 , mInterceptR(0)
 , mInterceptG(0)
 , mInterceptB(0)
 , mInterceptA(0)
{}

int32_t
FilterNodeLinearTransferSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_LINEAR_TRANSFER_IN: return 0;
    default: return -1;
  }
}

void
FilterNodeLinearTransferSoftware::SetAttribute(uint32_t aIndex,
                                               bool aDisable)
{
  switch (aIndex) {
    case ATT_LINEAR_TRANSFER_DISABLE_R:
      mDisableR = aDisable;
      break;
    case ATT_LINEAR_TRANSFER_DISABLE_G:
      mDisableG = aDisable;
      break;
    case ATT_LINEAR_TRANSFER_DISABLE_B:
      mDisableB = aDisable;
      break;
    case ATT_LINEAR_TRANSFER_DISABLE_A:
      mDisableA = aDisable;
      break;
    default:
      MOZ_CRASH();
  }
}

void
FilterNodeLinearTransferSoftware::SetAttribute(uint32_t aIndex,
                                               Float aValue)
{
  switch (aIndex) {
    case ATT_LINEAR_TRANSFER_SLOPE_R:
      mSlopeR = aValue;
      break;
    case ATT_LINEAR_TRANSFER_INTERCEPT_R:
      mInterceptR = aValue;
      break;
    case ATT_LINEAR_TRANSFER_SLOPE_G:
      mSlopeG = aValue;
      break;
    case ATT_LINEAR_TRANSFER_INTERCEPT_G:
      mInterceptG = aValue;
      break;
    case ATT_LINEAR_TRANSFER_SLOPE_B:
      mSlopeB = aValue;
      break;
    case ATT_LINEAR_TRANSFER_INTERCEPT_B:
      mInterceptB = aValue;
      break;
    case ATT_LINEAR_TRANSFER_SLOPE_A:
      mSlopeA = aValue;
      break;
    case ATT_LINEAR_TRANSFER_INTERCEPT_A:
      mInterceptA = aValue;
      break;
    default:
      MOZ_CRASH();
  }
}

void
FilterNodeLinearTransferSoftware::GenerateLookupTable(ptrdiff_t aComponent,
                                                      uint8_t aTable[256])
{
  switch (aComponent) {
    case ARGB32_COMPONENT_BYTEOFFSET_R:
      GenerateLookupTable(mSlopeR, mInterceptR, aTable);
      break;
    case ARGB32_COMPONENT_BYTEOFFSET_G:
      GenerateLookupTable(mSlopeG, mInterceptG, aTable);
      break;
    case ARGB32_COMPONENT_BYTEOFFSET_B:
      GenerateLookupTable(mSlopeB, mInterceptB, aTable);
      break;
    case ARGB32_COMPONENT_BYTEOFFSET_A:
      GenerateLookupTable(mSlopeA, mInterceptA, aTable);
      break;
    default:
      MOZ_ASSERT(false, "unknown component");
      break;
  }
}

void
FilterNodeLinearTransferSoftware::GenerateLookupTable(Float aSlope,
                                                      Float aIntercept,
                                                      uint8_t aTable[256])
{
  for (size_t i = 0; i < 256; i++) {
    int32_t val = int32_t(aSlope * i + 255 * aIntercept);
    val = std::min(255, val);
    val = std::max(0, val);
    aTable[i] = val;
  }
}

FilterNodeGammaTransferSoftware::FilterNodeGammaTransferSoftware()
 : mAmplitudeR(0)
 , mAmplitudeG(0)
 , mAmplitudeB(0)
 , mAmplitudeA(0)
 , mExponentR(0)
 , mExponentG(0)
 , mExponentB(0)
 , mExponentA(0)
{}

int32_t
FilterNodeGammaTransferSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_GAMMA_TRANSFER_IN: return 0;
    default: return -1;
  }
}

void
FilterNodeGammaTransferSoftware::SetAttribute(uint32_t aIndex,
                                               bool aDisable)
{
  switch (aIndex) {
    case ATT_GAMMA_TRANSFER_DISABLE_R:
      mDisableR = aDisable;
      break;
    case ATT_GAMMA_TRANSFER_DISABLE_G:
      mDisableG = aDisable;
      break;
    case ATT_GAMMA_TRANSFER_DISABLE_B:
      mDisableB = aDisable;
      break;
    case ATT_GAMMA_TRANSFER_DISABLE_A:
      mDisableA = aDisable;
      break;
    default:
      MOZ_CRASH();
  }
}

void
FilterNodeGammaTransferSoftware::SetAttribute(uint32_t aIndex,
                                              Float aValue)
{
  switch (aIndex) {
    case ATT_GAMMA_TRANSFER_AMPLITUDE_R:
      mAmplitudeR = aValue;
      break;
    case ATT_GAMMA_TRANSFER_EXPONENT_R:
      mExponentR = aValue;
      break;
    case ATT_GAMMA_TRANSFER_OFFSET_R:
      mOffsetR = aValue;
      break;
    case ATT_GAMMA_TRANSFER_AMPLITUDE_G:
      mAmplitudeG = aValue;
      break;
    case ATT_GAMMA_TRANSFER_EXPONENT_G:
      mExponentG = aValue;
      break;
    case ATT_GAMMA_TRANSFER_OFFSET_G:
      mOffsetG = aValue;
      break;
    case ATT_GAMMA_TRANSFER_AMPLITUDE_B:
      mAmplitudeB = aValue;
      break;
    case ATT_GAMMA_TRANSFER_EXPONENT_B:
      mExponentB = aValue;
      break;
    case ATT_GAMMA_TRANSFER_OFFSET_B:
      mOffsetB = aValue;
      break;
    case ATT_GAMMA_TRANSFER_AMPLITUDE_A:
      mAmplitudeA = aValue;
      break;
    case ATT_GAMMA_TRANSFER_EXPONENT_A:
      mExponentA = aValue;
      break;
    case ATT_GAMMA_TRANSFER_OFFSET_A:
      mOffsetA = aValue;
      break;
    default:
      MOZ_CRASH();
  }
}

void
FilterNodeGammaTransferSoftware::GenerateLookupTable(ptrdiff_t aComponent,
                                                     uint8_t aTable[256])
{
  switch (aComponent) {
    case ARGB32_COMPONENT_BYTEOFFSET_R:
      GenerateLookupTable(mAmplitudeR, mExponentR, mOffsetR, aTable);
      break;
    case ARGB32_COMPONENT_BYTEOFFSET_G:
      GenerateLookupTable(mAmplitudeG, mExponentG, mOffsetG, aTable);
      break;
    case ARGB32_COMPONENT_BYTEOFFSET_B:
      GenerateLookupTable(mAmplitudeB, mExponentB, mOffsetB, aTable);
      break;
    case ARGB32_COMPONENT_BYTEOFFSET_A:
      GenerateLookupTable(mAmplitudeA, mExponentA, mOffsetA, aTable);
      break;
    default:
      MOZ_ASSERT(false, "unknown component");
      break;
  }
}

void
FilterNodeGammaTransferSoftware::GenerateLookupTable(Float aAmplitude, Float aExponent, Float aOffset,
                                                                      uint8_t aTable[256])
{
  for (size_t i = 0; i < 256; i++) {
    int32_t val = int32_t(255 * (aAmplitude * pow(i / 255.0f, aExponent) + aOffset));
    val = std::min(255, val);
    val = std::max(0, val);
    aTable[i] = val;
  }
}

FilterNodeConvolveMatrixSoftware::FilterNodeConvolveMatrixSoftware()
 : mDivisor(0)
 , mBias(0)
 , mEdgeMode(EDGE_MODE_DUPLICATE)
 , mKernelUnitLength(0)
 , mPreserveAlpha(false)
{}

int32_t
FilterNodeConvolveMatrixSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_CONVOLVE_MATRIX_IN: return 0;
    default: return -1;
  }
}

void
FilterNodeConvolveMatrixSoftware::SetAttribute(uint32_t aIndex,
                                               const IntSize &aKernelSize)
{
  MOZ_ASSERT(aIndex == ATT_CONVOLVE_MATRIX_KERNEL_SIZE);
  mKernelSize = aKernelSize;
}

void
FilterNodeConvolveMatrixSoftware::SetAttribute(uint32_t aIndex,
                                               const Float *aMatrix,
                                               uint32_t aSize)
{
  MOZ_ASSERT(aIndex == ATT_CONVOLVE_MATRIX_KERNEL_MATRIX);
  mKernelMatrix = std::vector<Float>(aMatrix, aMatrix + aSize);
}

void
FilterNodeConvolveMatrixSoftware::SetAttribute(uint32_t aIndex, Float aValue)
{
  switch (aIndex) {
    case ATT_CONVOLVE_MATRIX_DIVISOR:
      mDivisor = aValue;
      break;
    case ATT_CONVOLVE_MATRIX_BIAS:
      mBias = aValue;
      break;
    case ATT_CONVOLVE_MATRIX_KERNEL_UNIT_LENGTH:
      mKernelUnitLength = aValue;
      break;
    default:
      MOZ_CRASH();
  }
}

void
FilterNodeConvolveMatrixSoftware::SetAttribute(uint32_t aIndex,
                                               const IntPoint &aTarget)
{
  MOZ_ASSERT(aIndex == ATT_CONVOLVE_MATRIX_TARGET);
  mTarget = aTarget;
}

void
FilterNodeConvolveMatrixSoftware::SetAttribute(uint32_t aIndex,
                                               uint32_t aEdgeMode)
{
  MOZ_ASSERT(aIndex == ATT_CONVOLVE_MATRIX_EDGE_MODE);
  mEdgeMode = static_cast<ConvolveMatrixEdgeMode>(aEdgeMode);
}

void
FilterNodeConvolveMatrixSoftware::SetAttribute(uint32_t aIndex,
                                               bool aPreserveAlpha)
{
  MOZ_ASSERT(aIndex == ATT_CONVOLVE_MATRIX_PRESERVE_ALPHA);
  mPreserveAlpha = aPreserveAlpha;
}

static void
ConvolvePixel(const uint8_t *aSourceData,
              uint8_t *aTargetData,
              int32_t aWidth, int32_t aHeight,
              int32_t aSourceStride, int32_t aTargetStride,
              int32_t aX, int32_t aY,
              const Float *aKernel,
              Float aDivisor, Float aBias,
              bool aPreserveAlpha,
              int32_t aOrderX, int32_t aOrderY,
              int32_t aTargetX, int32_t aTargetY)
{
  Float sum[4] = {0, 0, 0, 0};
  aBias *= 255;
  int32_t offsets[4] = { ARGB32_COMPONENT_BYTEOFFSET_R,
                         ARGB32_COMPONENT_BYTEOFFSET_G,
                         ARGB32_COMPONENT_BYTEOFFSET_B,
                         ARGB32_COMPONENT_BYTEOFFSET_A };
  int32_t channels = aPreserveAlpha ? 3 : 4;

  for (int32_t y = 0; y < aOrderY; y++) {
    int32_t sampleY = aY + y - aTargetY;
    for (int32_t x = 0; x < aOrderX; x++) {
      int32_t sampleX = aX + x - aTargetX;
      for (int32_t i = 0; i < channels; i++) {
        sum[i] +=
          aSourceData[sampleY * aSourceStride + 4 * sampleX + offsets[i]] *
          aKernel[aOrderX * y + x];
      }
    }
  }
  for (int32_t i = 0; i < channels; i++) {
    aTargetData[aY * aTargetStride + 4 * aX + offsets[i]] =
      clamped(static_cast<int32_t>(sum[i] / aDivisor + aBias), 0, 255);
  }
  if (aPreserveAlpha) {
    aTargetData[aY * aTargetStride + 4 * aX + ARGB32_COMPONENT_BYTEOFFSET_A] =
      aSourceData[aY * aSourceStride + 4 * aX + ARGB32_COMPONENT_BYTEOFFSET_A];
  }
}

TemporaryRef<DataSourceSurface>
FilterNodeConvolveMatrixSoftware::Render(const IntRect& aRect)
{
  if (mKernelSize.width <= 0 || mKernelSize.height <= 0 ||
      mKernelMatrix.size() != uint32_t(mKernelSize.width * mKernelSize.height) ||
      !IntRect(IntPoint(0, 0), mKernelSize).Contains(mTarget) ||
      mDivisor == 0) {
    return Factory::CreateDataSourceSurface(aRect.Size(), FORMAT_B8G8R8A8);
  }

  IntRect srcRect = InflatedSourceRect(aRect);
  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_CONVOLVE_MATRIX_IN, srcRect, mEdgeMode);
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(aRect.Size(), FORMAT_B8G8R8A8);

  uint8_t* sourceData = input->GetData();
  int32_t sourceStride = input->Stride();
  uint8_t* targetData = target->GetData();
  int32_t targetStride = target->Stride();

  IntPoint offset = aRect.TopLeft() - srcRect.TopLeft();
  sourceData += DataOffset(input, offset);

  uint32_t kmLength = mKernelMatrix.size();

  std::vector<Float> kernel(kmLength, 0);
  for (uint32_t i = 0; i < kmLength; i++) {
    kernel[kmLength - 1 - i] = mKernelMatrix[i];
  }

  for (int32_t y = 0; y < aRect.height; y++) {
    for (int32_t x = 0; x < aRect.width; x++) {
      ConvolvePixel(sourceData, targetData,
                    aRect.width, aRect.height, sourceStride, targetStride,
                    x, y, kernel.data(), mDivisor, mBias, mPreserveAlpha,
                    mKernelSize.width, mKernelSize.width, mTarget.x, mTarget.y);
    }
  }

  return target;
}

IntRect
FilterNodeConvolveMatrixSoftware::InflatedSourceRect(const IntRect &aDestRect)
{
  Margin margin;
  margin.left = mTarget.x;
  margin.top = mTarget.y;
  margin.right = mKernelSize.width - mTarget.x - 1;
  margin.bottom = mKernelSize.height - mTarget.y - 1;

  IntRect srcRect = aDestRect;
  srcRect.Inflate(margin);
  return srcRect;
}

IntRect
FilterNodeConvolveMatrixSoftware::InflatedDestRect(const IntRect &aSourceRect)
{
  Margin margin;
  margin.left = mKernelSize.width - mTarget.x - 1;
  margin.top = mKernelSize.height - mTarget.y - 1;
  margin.right = mTarget.x;
  margin.bottom = mTarget.y;

  IntRect destRect = aSourceRect;
  destRect.Inflate(margin);
  return destRect;
}

IntRect
FilterNodeConvolveMatrixSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  IntRect srcRequest = InflatedSourceRect(aRect);
  IntRect srcOutput = GetInputRectInRect(IN_COLOR_MATRIX_IN, srcRequest);
  return InflatedDestRect(srcOutput).Intersect(aRect);
}

int32_t
FilterNodeOffsetSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_OFFSET_IN: return 0;
    default: return -1;
  }
}

void
FilterNodeOffsetSoftware::SetAttribute(uint32_t aIndex,
                                            const IntPoint &aOffset)
{
  MOZ_ASSERT(aIndex == ATT_OFFSET_OFFSET);
  mOffset = aOffset;
}

TemporaryRef<DataSourceSurface>
FilterNodeOffsetSoftware::Render(const IntRect& aRect)
{
  return GetInputDataSourceSurface(IN_OFFSET_IN, aRect - mOffset);
}

IntRect
FilterNodeOffsetSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  return GetInputRectInRect(IN_OFFSET_IN, aRect - mOffset) + mOffset;
}

FilterNodeDisplacementMapSoftware::FilterNodeDisplacementMapSoftware()
 : mScale(0.0f)
 , mChannelX(COLOR_CHANNEL_R)
 , mChannelY(COLOR_CHANNEL_G)
{}

int32_t
FilterNodeDisplacementMapSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_DISPLACEMENT_MAP_IN: return 0;
    case IN_DISPLACEMENT_MAP_IN2: return 1;
    default: return -1;
  }
}

void
FilterNodeDisplacementMapSoftware::SetAttribute(uint32_t aIndex,
                                                Float aScale)
{
  MOZ_ASSERT(aIndex == ATT_DISPLACEMENT_MAP_SCALE);
  mScale = aScale;
}

void
FilterNodeDisplacementMapSoftware::SetAttribute(uint32_t aIndex, uint32_t aValue)
{
  switch (aIndex) {
    case ATT_DISPLACEMENT_MAP_X_CHANNEL:
      mChannelX = static_cast<ColorChannel>(aValue);
      break;
    case ATT_DISPLACEMENT_MAP_Y_CHANNEL:
      mChannelY = static_cast<ColorChannel>(aValue);
      break;
    default:
      MOZ_CRASH();
  }
}

TemporaryRef<DataSourceSurface>
FilterNodeDisplacementMapSoftware::Render(const IntRect& aRect)
{
  IntRect srcRect = InflatedSourceOrDestRect(aRect);
  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_DISPLACEMENT_MAP_IN, srcRect);
  RefPtr<DataSourceSurface> map =
    GetInputDataSourceSurface(IN_DISPLACEMENT_MAP_IN2, aRect);
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(aRect.Size(), FORMAT_B8G8R8A8);

  uint8_t* sourceData = input->GetData();
  int32_t sourceStride = input->Stride();
  uint8_t* mapData = map->GetData();
  int32_t mapStride = map->Stride();
  uint8_t* targetData = target->GetData();
  int32_t targetStride = target->Stride();

  IntPoint offset = aRect.TopLeft() - srcRect.TopLeft();
  sourceData += DataOffset(input, offset);

  static const ptrdiff_t channelMap[4] = {
                             ARGB32_COMPONENT_BYTEOFFSET_R,
                             ARGB32_COMPONENT_BYTEOFFSET_G,
                             ARGB32_COMPONENT_BYTEOFFSET_B,
                             ARGB32_COMPONENT_BYTEOFFSET_A };
  uint16_t xChannel = channelMap[mChannelX];
  uint16_t yChannel = channelMap[mChannelY];

  double scaleOver255 = mScale / 255.0;
  double scaleAdjustment = 0.5 - 0.5 * mScale;

  for (int32_t y = 0; y < aRect.height; y++) {
    for (int32_t x = 0; x < aRect.width; x++) {
      uint32_t mapIndex = y * mapStride + 4 * x;
      uint32_t targIndex = y * targetStride + 4 * x;
      // At some point we might want to replace this with a bilinear sample.
      int32_t sourceX = x +
        floor(scaleOver255 * mapData[mapIndex + xChannel] +
                scaleAdjustment);
      int32_t sourceY = y +
        floor(scaleOver255 * mapData[mapIndex + yChannel] +
                scaleAdjustment);
      int32_t sourceIndex = sourceY * sourceStride + 4 * sourceX;

      *(uint32_t*)(targetData + targIndex) =
        *(uint32_t*)(sourceData + sourceIndex);
    }
  }

  return target;
}

IntRect
FilterNodeDisplacementMapSoftware::InflatedSourceOrDestRect(const IntRect &aDestOrSourceRect)
{
  IntRect sourceOrDestRect = aDestOrSourceRect;
  sourceOrDestRect.Inflate(ceil(fabs(mScale) / 2));
  return sourceOrDestRect;
}

IntRect
FilterNodeDisplacementMapSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  IntRect srcRequest = InflatedSourceOrDestRect(aRect);
  IntRect srcOutput = GetInputRectInRect(IN_DISPLACEMENT_MAP_IN, srcRequest);
  return InflatedSourceOrDestRect(srcOutput).Intersect(aRect);
}

FilterNodeTurbulenceSoftware::FilterNodeTurbulenceSoftware()
 : mNumOctaves(0)
 , mSeed(0)
 , mStitchable(false)
 , mType(TURBULENCE_TYPE_TURBULENCE)
{}

int32_t
FilterNodeTurbulenceSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  return -1;
}

void
FilterNodeTurbulenceSoftware::SetAttribute(uint32_t aIndex, const Size &aBaseFrequency)
{
  switch (aIndex) {
    case ATT_TURBULENCE_BASE_FREQUENCY:
      mBaseFrequency = aBaseFrequency;
      break;
    default:
      MOZ_CRASH();
      break;
  }
}

void
FilterNodeTurbulenceSoftware::SetAttribute(uint32_t aIndex, bool aStitchable)
{
  MOZ_ASSERT(aIndex == ATT_TURBULENCE_STITCHABLE);
  mStitchable = aStitchable;
}

void
FilterNodeTurbulenceSoftware::SetAttribute(uint32_t aIndex, uint32_t aValue)
{
  switch (aIndex) {
    case ATT_TURBULENCE_NUM_OCTAVES:
      mNumOctaves = aValue;
      break;
    case ATT_TURBULENCE_SEED:
      mSeed = aValue;
      break;
    case ATT_TURBULENCE_TYPE:
      mType = static_cast<TurbulenceType>(aValue);
      break;
    default:
      MOZ_CRASH();
      break;
  }
}

void
FilterNodeTurbulenceSoftware::InitSeed(int32_t aSeed)
{
  double s;
  int i, j, k;
  aSeed = SetupSeed(aSeed);
  for (k = 0; k < 4; k++) {
    for (i = 0; i < sBSize; i++) {
      mLatticeSelector[i] = i;
      for (j = 0; j < 2; j++) {
        mGradient[k][i][j] =
          (double) (((aSeed =
                      Random(aSeed)) % (sBSize + sBSize)) - sBSize) / sBSize;
      }
      s = double (sqrt
                  (mGradient[k][i][0] * mGradient[k][i][0] +
                   mGradient[k][i][1] * mGradient[k][i][1]));
      mGradient[k][i][0] /= s;
      mGradient[k][i][1] /= s;
    }
  }
  while (--i) {
    k = mLatticeSelector[i];
    mLatticeSelector[i] = mLatticeSelector[j =
                                           (aSeed =
                                            Random(aSeed)) % sBSize];
    mLatticeSelector[j] = k;
  }
  for (i = 0; i < sBSize + 2; i++) {
    mLatticeSelector[sBSize + i] = mLatticeSelector[i];
    for (k = 0; k < 4; k++)
      for (j = 0; j < 2; j++)
        mGradient[k][sBSize + i][j] = mGradient[k][i][j];
  }
}

#define S_CURVE(t) ( t * t * (3. - 2. * t) )
#define LERP(t, a, b) ( a + t * (b - a) )
double
FilterNodeTurbulenceSoftware::Noise2(int aColorChannel, double aVec[2],
                                     StitchInfo *aStitchInfo)
{
  int bx0, bx1, by0, by1, b00, b10, b01, b11;
  double rx0, rx1, ry0, ry1, *q, sx, sy, a, b, t, u, v;
  long i, j;
  t = aVec[0] + sPerlinN;
  bx0 = (int) t;
  bx1 = bx0 + 1;
  rx0 = t - (int) t;
  rx1 = rx0 - 1.0f;
  t = aVec[1] + sPerlinN;
  by0 = (int) t;
  by1 = by0 + 1;
  ry0 = t - (int) t;
  ry1 = ry0 - 1.0f;
  // If stitching, adjust lattice points accordingly.
  if (aStitchInfo != NULL) {
    if (bx0 >= aStitchInfo->mWrapX)
      bx0 -= aStitchInfo->mWidth;
    if (bx1 >= aStitchInfo->mWrapX)
      bx1 -= aStitchInfo->mWidth;
    if (by0 >= aStitchInfo->mWrapY)
      by0 -= aStitchInfo->mHeight;
    if (by1 >= aStitchInfo->mWrapY)
      by1 -= aStitchInfo->mHeight;
  }
  bx0 &= sBM;
  bx1 &= sBM;
  by0 &= sBM;
  by1 &= sBM;
  i = mLatticeSelector[bx0];
  j = mLatticeSelector[bx1];
  b00 = mLatticeSelector[i + by0];
  b10 = mLatticeSelector[j + by0];
  b01 = mLatticeSelector[i + by1];
  b11 = mLatticeSelector[j + by1];
  sx = double (S_CURVE(rx0));
  sy = double (S_CURVE(ry0));
  q = mGradient[aColorChannel][b00];
  u = rx0 * q[0] + ry0 * q[1];
  q = mGradient[aColorChannel][b10];
  v = rx1 * q[0] + ry0 * q[1];
  a = LERP(sx, u, v);
  q = mGradient[aColorChannel][b01];
  u = rx0 * q[0] + ry1 * q[1];
  q = mGradient[aColorChannel][b11];
  v = rx1 * q[0] + ry1 * q[1];
  b = LERP(sx, u, v);
  return LERP(sy, a, b);
}
#undef S_CURVE
#undef LERP

double
FilterNodeTurbulenceSoftware::Turbulence(int aColorChannel, double* aPoint,
                                         double aBaseFreqX, double aBaseFreqY,
                                         int aNumOctaves, bool aFractalSum,
                                         bool aDoStitching,
                                         double aTileX, double aTileY,
                                         double aTileWidth, double aTileHeight)
{
  StitchInfo stitch;
  StitchInfo *stitchInfo = NULL; // Not stitching when NULL.
  // Adjust the base frequencies if necessary for stitching.
  if (aDoStitching) {
    // When stitching tiled turbulence, the frequencies must be adjusted
    // so that the tile borders will be continuous.
    if (aBaseFreqX != 0.0) {
      double loFreq = double (floor(aTileWidth * aBaseFreqX)) / aTileWidth;
      double hiFreq = double (ceil(aTileWidth * aBaseFreqX)) / aTileWidth;
      if (aBaseFreqX / loFreq < hiFreq / aBaseFreqX)
        aBaseFreqX = loFreq;
      else
        aBaseFreqX = hiFreq;
    }
    if (aBaseFreqY != 0.0) {
      double loFreq = double (floor(aTileHeight * aBaseFreqY)) / aTileHeight;
      double hiFreq = double (ceil(aTileHeight * aBaseFreqY)) / aTileHeight;
      if (aBaseFreqY / loFreq < hiFreq / aBaseFreqY)
        aBaseFreqY = loFreq;
      else
        aBaseFreqY = hiFreq;
    }
    // Set up initial stitch values.
    stitchInfo = &stitch;
    stitch.mWidth = int (aTileWidth * aBaseFreqX + 0.5f);
    stitch.mWrapX = int (aTileX * aBaseFreqX + sPerlinN + stitch.mWidth);
    stitch.mHeight = int (aTileHeight * aBaseFreqY + 0.5f);
    stitch.mWrapY = int (aTileY * aBaseFreqY + sPerlinN + stitch.mHeight);
  }
  double sum = 0.0f;
  double vec[2];
  vec[0] = aPoint[0] * aBaseFreqX;
  vec[1] = aPoint[1] * aBaseFreqY;
  double ratio = 1;
  for (int octave = 0; octave < aNumOctaves; octave++) {
    if (aFractalSum)
      sum += double (Noise2(aColorChannel, vec, stitchInfo) / ratio);
    else
      sum += double (fabs(Noise2(aColorChannel, vec, stitchInfo)) / ratio);
    vec[0] *= 2;
    vec[1] *= 2;
    ratio *= 2;
    if (stitchInfo != NULL) {
      // Update stitch values. Subtracting sPerlinN before the multiplication
      // and adding it afterward simplifies to subtracting it once.
      stitch.mWidth *= 2;
      stitch.mWrapX = 2 * stitch.mWrapX - sPerlinN;
      stitch.mHeight *= 2;
      stitch.mWrapY = 2 * stitch.mWrapY - sPerlinN;
    }
  }
  return sum;
}

TemporaryRef<DataSourceSurface>
FilterNodeTurbulenceSoftware::Render(const IntRect& aRect)
{
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(aRect.Size(), FORMAT_B8G8R8A8);

  uint8_t* targetData = target->GetData();
  uint32_t stride = target->Stride();

  IntRect rect = aRect;

  float fX = mBaseFrequency.width;
  float fY = mBaseFrequency.height;
  int32_t octaves = mNumOctaves;
  TurbulenceType type = mType;
  bool stitch = mStitchable;

  InitSeed(mSeed);

  // XXXroc this makes absolutely no sense to me.
  float filterX = 0;
  float filterY = 0;
  float filterWidth = rect.width;
  float filterHeight = rect.height;

  bool doStitch = stitch;
  if (doStitch) {
    float lowFreq, hiFreq;

    lowFreq = floor(filterWidth * fX) / filterWidth;
    hiFreq = ceil(filterWidth * fX) / filterWidth;
    if (fX / lowFreq < hiFreq / fX)
      fX = lowFreq;
    else
      fX = hiFreq;

    lowFreq = floor(filterHeight * fY) / filterHeight;
    hiFreq = ceil(filterHeight * fY) / filterHeight;
    if (fY / lowFreq < hiFreq / fY)
      fY = lowFreq;
    else
      fY = hiFreq;
  }
  for (int32_t y = 0; y < rect.height; y++) {
    for (int32_t x = 0; x < rect.width; x++) {
      int32_t targIndex = y * stride + x * 4;
      double point[2];
      point[0] = filterX + (filterWidth * x) / (rect.width - 1);
      point[1] = filterY + (filterHeight * y) / (rect.height - 1);

      float col[4];
      if (type == TURBULENCE_TYPE_TURBULENCE) {
        for (int i = 0; i < 4; i++)
          col[i] = Turbulence(i, point, fX, fY, octaves, false,
                              doStitch, filterX, filterY, filterWidth, filterHeight) * 255;
      } else {
        for (int i = 0; i < 4; i++)
          col[i] = (Turbulence(i, point, fX, fY, octaves, true,
                               doStitch, filterX, filterY, filterWidth, filterHeight) * 255 + 255) / 2;
      }
      for (int i = 0; i < 4; i++) {
        col[i] = clamped(col[i], 0.f, 255.f);
      }

      uint8_t a = uint8_t(col[3]);
      targetData[targIndex + ARGB32_COMPONENT_BYTEOFFSET_R] = FastDivideBy255<uint8_t>(unsigned(col[0]) * a);
      targetData[targIndex + ARGB32_COMPONENT_BYTEOFFSET_G] = FastDivideBy255<uint8_t>(unsigned(col[1]) * a);
      targetData[targIndex + ARGB32_COMPONENT_BYTEOFFSET_B] = FastDivideBy255<uint8_t>(unsigned(col[2]) * a);
      targetData[targIndex + ARGB32_COMPONENT_BYTEOFFSET_A] = a;
    }
  }

  return target;
}

IntRect
FilterNodeTurbulenceSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  return aRect;
}

FilterNodeArithmeticCombineSoftware::FilterNodeArithmeticCombineSoftware()
 : mK1(0), mK2(0), mK3(0), mK4(0)
{
}

int32_t
FilterNodeArithmeticCombineSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_ARITHMETIC_COMBINE_IN: return 0;
    case IN_ARITHMETIC_COMBINE_IN2: return 1;
    default: return -1;
  }
}

void
FilterNodeArithmeticCombineSoftware::SetAttribute(uint32_t aIndex,
                                                  const Float* aFloat,
                                                  uint32_t aSize)
{
  MOZ_ASSERT(aIndex == ATT_ARITHMETIC_COMBINE_COEFFICIENTS);
  MOZ_ASSERT(aSize == 4);

  mK1 = aFloat[0];
  mK2 = aFloat[1];
  mK3 = aFloat[2];
  mK4 = aFloat[3];
}

TemporaryRef<DataSourceSurface>
FilterNodeArithmeticCombineSoftware::Render(const IntRect& aRect)
{
  RefPtr<DataSourceSurface> input1 =
    GetInputDataSourceSurface(IN_ARITHMETIC_COMBINE_IN, aRect);
  RefPtr<DataSourceSurface> input2 =
    GetInputDataSourceSurface(IN_ARITHMETIC_COMBINE_IN2, aRect);
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(aRect.Size(), FORMAT_B8G8R8A8);

  uint8_t* sourceData1 = input1->GetData();
  uint8_t* sourceData2 = input2->GetData();
  uint8_t* targetData = target->GetData();
  uint32_t stride = target->Stride();

  float k1Scaled = mK1 / 255.0f;
  float k4Scaled = mK4*255.0f;
  for (int32_t y = 0; y < aRect.height; y++) {
    for (int32_t x = 0; x < aRect.width; x++) {
      uint32_t targIndex = y * stride + 4 * x;
      for (int32_t i = 0; i < 4; i++) {
        uint8_t i1 = sourceData1[targIndex + i];
        uint8_t i2 = sourceData2[targIndex + i];
        float result = k1Scaled*i1*i2 + mK2*i1 + mK3*i2 + k4Scaled;
        targetData[targIndex + i] =
                     static_cast<uint8_t>(clamped(result, 0.f, 255.f));
      }
    }
  }

  return target;
}

IntRect
FilterNodeArithmeticCombineSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  return GetInputRectInRect(IN_ARITHMETIC_COMBINE_IN, aRect).Union(
    GetInputRectInRect(IN_ARITHMETIC_COMBINE_IN2, aRect)).Intersect(aRect);
}

FilterNodeCompositeSoftware::FilterNodeCompositeSoftware()
 : mOperator(COMPOSITE_OPERATOR_OVER)
{}

int32_t
FilterNodeCompositeSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  return aInputEnumIndex - IN_COMPOSITE_IN_START;
}

void
FilterNodeCompositeSoftware::SetAttribute(uint32_t aIndex, uint32_t aCompositeOperator)
{
  MOZ_ASSERT(aIndex == ATT_COMPOSITE_OPERATOR);
  mOperator = static_cast<CompositeOperator>(aCompositeOperator);
}

static void
ApplyComposition(DataSourceSurface* aSource, DataSourceSurface* aDest, uint32_t aCompositeOperator)
{
  IntSize size = aDest->GetSize();

  uint8_t* sourceData = aSource->GetData();
  uint8_t* destData = aDest->GetData();
  uint32_t stride = aDest->Stride();

  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x++) {
      uint32_t targIndex = y * stride + 4 * x;
      uint32_t qa = destData[targIndex + ARGB32_COMPONENT_BYTEOFFSET_A];
      uint32_t qb = sourceData[targIndex + ARGB32_COMPONENT_BYTEOFFSET_A];
      for (int32_t i = 0; i < 4; i++) {
        uint32_t ca = destData[targIndex + i];
        uint32_t cb = sourceData[targIndex + i];
        uint32_t val;
        switch (aCompositeOperator) {
          case COMPOSITE_OPERATOR_OVER:
            val = ca * (255 - qb) + cb * 255;
            break;
          case COMPOSITE_OPERATOR_IN:
            val = cb * qa;
            break;
          case COMPOSITE_OPERATOR_OUT:
            val = cb * (255 - qa);
            break;
          case COMPOSITE_OPERATOR_ATOP:
            val = cb * qa + ca * (255 - qb);
            break;
          case COMPOSITE_OPERATOR_XOR:
            val = cb * (255 - qa) + ca * (255 - qb);
            break;
          default:
            MOZ_CRASH();
        }
        destData[targIndex + i] =
          static_cast<uint8_t>(umin(FastDivideBy255<unsigned>(val), 255U));
      }
    }
  }
}

TemporaryRef<DataSourceSurface>
FilterNodeCompositeSoftware::Render(const IntRect& aRect)
{
  RefPtr<DataSourceSurface> start =
    GetInputDataSourceSurface(IN_COMPOSITE_IN_START, aRect);
  RefPtr<DataSourceSurface> dest =
    Factory::CreateDataSourceSurface(aRect.Size(), FORMAT_B8G8R8A8);
  CopyRect(start, dest, aRect - aRect.TopLeft(), IntPoint());
  for (size_t inputIndex = 1; inputIndex < NumberOfSetInputs(); inputIndex++) {
    RefPtr<DataSourceSurface> input =
      GetInputDataSourceSurface(IN_COMPOSITE_IN_START + inputIndex, aRect);
    ApplyComposition(input, dest, mOperator);
  }
  return dest;
}

IntRect
FilterNodeCompositeSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  IntRect rect;
  for (size_t inputIndex = 0; inputIndex < NumberOfSetInputs(); inputIndex++) {
    rect = rect.Union(GetInputRectInRect(IN_COMPOSITE_IN_START + inputIndex, aRect));
  }
  return rect;
}


/**
 * We want to speed up 1/N integer divisions --- integer division is
 * often rather slow.
 * We know that our input numerators V are constrained to be <= 255*N,
 * so the result of dividing by N always fits in 8 bits.
 * So we can try approximating the division V/N as V*K/(2^24) (integer
 * division, 32-bit multiply). Dividing by 2^24 is a simple shift so it's
 * fast. The main problem is choosing a value for K; this function returns
 * K's value.
 *
 * If the result is correct for the extrema, V=0 and V=255*N, then we'll
 * be in good shape since both the original function and our approximation
 * are linear. V=0 always gives 0 in both cases, no problem there.
 * For V=255*N, let's choose the largest K that doesn't cause overflow
 * and ensure that it gives the right answer. The constraints are
 *     (1)   255*N*K < 2^32
 * and (2)   255*N*K >= 255*(2^24)
 *
 * From (1) we find the best value of K is floor((2^32 - 1)/(255*N)).
 * (2) tells us when this will be valid:
 *    N*floor((2^32 - 1)/(255*N)) >= 2^24
 * Now, floor(X) > X - 1, so (2) holds if
 *    N*((2^32 - 1)/(255*N) - 1) >= 2^24
 *         (2^32 - 1)/255 - 2^24 >= N
 *                             N <= 65793
 *
 * If all that math confuses you, this should convince you:
 * > perl -e 'for($N=1;(255*$N*int(0xFFFFFFFF/(255*$N)))>>24==255;++$N){}print"$N\n"'
 * 66052
 *
 * So this is fine for all reasonable values of N. For larger values of N
 * we may as well just use the same approximation and accept the fact that
 * the output channel values will be a little low.
 */
static uint32_t ComputeScaledDivisor(uint32_t aDivisor)
{
  return UINT32_MAX/(255*aDivisor);
}

static void
BoxBlur(const uint8_t *aInput, uint8_t *aOutput,
        int32_t aStrideMinor, int32_t aStartMinor, int32_t aEndMinor,
        int32_t aLeftLobe, int32_t aRightLobe, bool aAlphaOnly)
{
  int32_t boxSize = aLeftLobe + aRightLobe + 1;
  int32_t scaledDivisor = ComputeScaledDivisor(boxSize);
  int32_t sums[4] = {0, 0, 0, 0};

  for (int32_t i=0; i < boxSize; i++) {
    int32_t pos = aStartMinor - aLeftLobe + i;
    pos = std::max(pos, aStartMinor);
    pos = std::min(pos, aEndMinor - 1);
#define SUM(j)     sums[j] += aInput[aStrideMinor*pos + j];
    SUM(0); SUM(1); SUM(2); SUM(3);
#undef SUM
  }

  aOutput += aStrideMinor*aStartMinor;
  if (aStartMinor + int32_t(boxSize) <= aEndMinor) {
    const uint8_t *lastInput = aInput + aStartMinor*aStrideMinor;
    const uint8_t *nextInput = aInput + (aStartMinor + aRightLobe + 1)*aStrideMinor;
#define OUTPUT(j)     aOutput[j] = (sums[j]*scaledDivisor) >> 24;
#define SUM(j)        sums[j] += nextInput[j] - lastInput[j];
    // process pixels in B, G, R, A order because that's 0, 1, 2, 3 for x86
#define OUTPUT_PIXEL() \
        if (!aAlphaOnly) { OUTPUT(ARGB32_COMPONENT_BYTEOFFSET_B); \
                           OUTPUT(ARGB32_COMPONENT_BYTEOFFSET_G); \
                           OUTPUT(ARGB32_COMPONENT_BYTEOFFSET_R); } \
        OUTPUT(ARGB32_COMPONENT_BYTEOFFSET_A);
#define SUM_PIXEL() \
        if (!aAlphaOnly) { SUM(ARGB32_COMPONENT_BYTEOFFSET_B); \
                           SUM(ARGB32_COMPONENT_BYTEOFFSET_G); \
                           SUM(ARGB32_COMPONENT_BYTEOFFSET_R); } \
        SUM(ARGB32_COMPONENT_BYTEOFFSET_A);
    for (int32_t minor = aStartMinor;
         minor < aStartMinor + aLeftLobe;
         minor++) {
      OUTPUT_PIXEL();
      SUM_PIXEL();
      nextInput += aStrideMinor;
      aOutput += aStrideMinor;
    }
    for (int32_t minor = aStartMinor + aLeftLobe;
         minor < aEndMinor - aRightLobe - 1;
         minor++) {
      OUTPUT_PIXEL();
      SUM_PIXEL();
      lastInput += aStrideMinor;
      nextInput += aStrideMinor;
      aOutput += aStrideMinor;
    }
    // nextInput is now aInput + aEndMinor*aStrideMinor. Set it back to
    // aInput + (aEndMinor - 1)*aStrideMinor so we read the last pixel in every
    // iteration of the next loop.
    nextInput -= aStrideMinor;
    for (int32_t minor = aEndMinor - aRightLobe - 1; minor < aEndMinor; minor++) {
      OUTPUT_PIXEL();
      SUM_PIXEL();
      lastInput += aStrideMinor;
      aOutput += aStrideMinor;
#undef SUM_PIXEL
#undef SUM
    }
  } else {
    for (int32_t minor = aStartMinor; minor < aEndMinor; minor++) {
      int32_t tmp = minor - aLeftLobe;
      int32_t last = std::max(tmp, aStartMinor);
      int32_t next = std::min(tmp + int32_t(boxSize), aEndMinor - 1);

      OUTPUT_PIXEL();
#define SUM(j)     sums[j] += aInput[aStrideMinor*next + j] - \
                              aInput[aStrideMinor*last + j];
      if (!aAlphaOnly) { SUM(ARGB32_COMPONENT_BYTEOFFSET_B);
                         SUM(ARGB32_COMPONENT_BYTEOFFSET_G);
                         SUM(ARGB32_COMPONENT_BYTEOFFSET_R); }
      SUM(ARGB32_COMPONENT_BYTEOFFSET_A);
      aOutput += aStrideMinor;
#undef SUM
#undef OUTPUT_PIXEL
#undef OUTPUT
    }
  }
}

static uint32_t
GetBlurBoxSize(double aStdDev)
{
  MOZ_ASSERT(aStdDev >= 0, "Negative standard deviations not allowed");

  double size = aStdDev*3*sqrt(2*M_PI)/4;
  // Doing super-large blurs accurately isn't very important.
  uint32_t max = 1024;
  if (size > max)
    return max;
  return uint32_t(floor(size + 0.5));
}

static void
InflateRectForBlurDXY(IntRect* aRect, uint32_t aDX, uint32_t aDY)
{
  aRect->Inflate(3*(aDX/2), 3*(aDY/2));
}

FilterNodeGaussianBlurSoftware::FilterNodeGaussianBlurSoftware()
 : mStdDeviation(0)
{}

int32_t
FilterNodeGaussianBlurSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_GAUSSIAN_BLUR_IN: return 0;
    default: return -1;
  }
}

void
FilterNodeGaussianBlurSoftware::SetAttribute(uint32_t aIndex,
                                             float aStdDeviation)
{
  switch (aIndex) {
    case ATT_GAUSSIAN_BLUR_STD_DEVIATION:
      mStdDeviation = std::max(0.0f, aStdDeviation);
      break;
    default:
      MOZ_CRASH();
  }
}

static void
CopyDataRect(uint8_t *aDest, const uint8_t *aSrc, uint32_t aStride,
             const IntRect& aDataRect)
{
  for (int32_t y = aDataRect.y; y < aDataRect.YMost(); y++) {
    memcpy(aDest + y * aStride + 4 * aDataRect.x,
           aSrc + y * aStride + 4 * aDataRect.x,
           4 * aDataRect.width);
  }
}

TemporaryRef<DataSourceSurface>
FilterNodeGaussianBlurSoftware::Render(const IntRect& aRect)
{
  uint32_t d = GetBlurBoxSize(mStdDeviation);

  if (d == 0) {
    return GetInputDataSourceSurface(IN_GAUSSIAN_BLUR_IN, aRect);
  }

  IntRect srcRect = InflatedSourceOrDestRect(aRect);
  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_GAUSSIAN_BLUR_IN, srcRect);
  RefPtr<DataSourceSurface> intermediateBuffer =
    Factory::CreateDataSourceSurface(srcRect.Size(), FORMAT_B8G8R8A8);
  ClearDataSourceSurface(intermediateBuffer);
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(srcRect.Size(), FORMAT_B8G8R8A8);
  ClearDataSourceSurface(target);

  // TODO: use this
  bool alphaOnly = false;

  uint8_t* sourceData = input->GetData();
  uint8_t* tmp = intermediateBuffer->GetData();
  uint8_t* targetData = target->GetData();
  uint32_t stride = target->Stride();

  IntRect dataRect = aRect - srcRect.TopLeft();

  int32_t longLobe = d/2;
  int32_t shortLobe = (d & 1) ? longLobe : longLobe - 1;
  for (int32_t major = dataRect.y; major < dataRect.YMost(); ++major) {
    int32_t ms = major*stride;
    BoxBlur(sourceData + ms, tmp + ms, 4, dataRect.x, dataRect.XMost(), longLobe, shortLobe, alphaOnly);
    BoxBlur(tmp + ms, targetData + ms, 4, dataRect.x, dataRect.XMost(), shortLobe, longLobe, alphaOnly);
    BoxBlur(targetData + ms, tmp + ms, 4, dataRect.x, dataRect.XMost(), longLobe, longLobe, alphaOnly);
  }
  for (int32_t major = dataRect.x; major < dataRect.XMost(); ++major) {
    int32_t ms = major*4;
    BoxBlur(tmp + ms, targetData + ms, stride, dataRect.y, dataRect.YMost(), longLobe, shortLobe, alphaOnly);
    BoxBlur(targetData + ms, tmp + ms, stride, dataRect.y, dataRect.YMost(), shortLobe, longLobe, alphaOnly);
    BoxBlur(tmp + ms, targetData + ms, stride, dataRect.y, dataRect.YMost(), longLobe, longLobe, alphaOnly);
  }

  return GetDataSurfaceInRect(target, srcRect, aRect, EDGE_MODE_NONE);
}

IntRect
FilterNodeGaussianBlurSoftware::InflatedSourceOrDestRect(const IntRect &aDestRect)
{
  uint32_t d = GetBlurBoxSize(mStdDeviation);
  IntRect srcRect = aDestRect;
  InflateRectForBlurDXY(&srcRect, d, d);
  return srcRect;
}

IntRect
FilterNodeGaussianBlurSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  IntRect srcRequest = InflatedSourceOrDestRect(aRect);
  IntRect srcOutput = GetInputRectInRect(IN_GAUSSIAN_BLUR_IN, srcRequest);
  return InflatedSourceOrDestRect(srcOutput).Intersect(aRect);
}

FilterNodeDirectionalBlurSoftware::FilterNodeDirectionalBlurSoftware()
 : mBlurDirection(BLUR_DIRECTION_X)
{}

int32_t
FilterNodeDirectionalBlurSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_DIRECTIONAL_BLUR_IN: return 0;
    default: return -1;
  }
}

void
FilterNodeDirectionalBlurSoftware::SetAttribute(uint32_t aIndex,
                                                Float aStdDeviation)
{
  switch (aIndex) {
    case ATT_DIRECTIONAL_BLUR_STD_DEVIATION:
      mStdDeviation = std::max(0.0f, aStdDeviation);
      break;
    default:
      MOZ_CRASH();
  }
}

void
FilterNodeDirectionalBlurSoftware::SetAttribute(uint32_t aIndex,
                                                uint32_t aBlurDirection)
{
  switch (aIndex) {
    case ATT_DIRECTIONAL_BLUR_DIRECTION:
      mBlurDirection = (BlurDirection)aBlurDirection;
      break;
    default:
      MOZ_CRASH();
  }
}

TemporaryRef<DataSourceSurface>
FilterNodeDirectionalBlurSoftware::Render(const IntRect& aRect)
{
  IntRect srcRect = InflatedSourceOrDestRect(aRect);
  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_DIRECTIONAL_BLUR_IN, srcRect);
  RefPtr<DataSourceSurface> intermediateBuffer =
    Factory::CreateDataSourceSurface(srcRect.Size(), FORMAT_B8G8R8A8);
  ClearDataSourceSurface(intermediateBuffer);
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(srcRect.Size(), FORMAT_B8G8R8A8);
  ClearDataSourceSurface(target);

  // TODO: use this
  bool alphaOnly = false;

  uint8_t* sourceData = input->GetData();
  uint8_t* tmp = intermediateBuffer->GetData();
  uint8_t* targetData = target->GetData();
  uint32_t stride = target->Stride();

  // XXX This needs to be fixed.
  uint32_t dx = mBlurDirection == BLUR_DIRECTION_X ? GetBlurBoxSize(mStdDeviation) : 0;
  uint32_t dy = mBlurDirection == BLUR_DIRECTION_Y ? GetBlurBoxSize(mStdDeviation) : 0;

  IntRect dataRect = aRect - srcRect.TopLeft();

  if (dx == 0) {
    CopyDataRect(tmp, sourceData, stride, dataRect);
  } else {
    int32_t longLobe = dx/2;
    int32_t shortLobe = (dx & 1) ? longLobe : longLobe - 1;
    for (int32_t major = dataRect.y; major < dataRect.YMost(); ++major) {
      int32_t ms = major*stride;
      BoxBlur(sourceData + ms, tmp + ms, 4, dataRect.x, dataRect.XMost(), longLobe, shortLobe, alphaOnly);
      BoxBlur(tmp + ms, targetData + ms, 4, dataRect.x, dataRect.XMost(), shortLobe, longLobe, alphaOnly);
      BoxBlur(targetData + ms, tmp + ms, 4, dataRect.x, dataRect.XMost(), longLobe, longLobe, alphaOnly);
    }
  }

  if (dy == 0) {
    CopyDataRect(targetData, tmp, stride, dataRect);
  } else {
    int32_t longLobe = dy/2;
    int32_t shortLobe = (dy & 1) ? longLobe : longLobe - 1;
    for (int32_t major = dataRect.x; major < dataRect.XMost(); ++major) {
      int32_t ms = major*4;
      BoxBlur(tmp + ms, targetData + ms, stride, dataRect.y, dataRect.YMost(), longLobe, shortLobe, alphaOnly);
      BoxBlur(targetData + ms, tmp + ms, stride, dataRect.y, dataRect.YMost(), shortLobe, longLobe, alphaOnly);
      BoxBlur(tmp + ms, targetData + ms, stride, dataRect.y, dataRect.YMost(), longLobe, longLobe, alphaOnly);
    }
  }

  return GetDataSurfaceInRect(target, srcRect, aRect, EDGE_MODE_NONE);
}

IntRect
FilterNodeDirectionalBlurSoftware::InflatedSourceOrDestRect(const IntRect &aDestRect)
{
  uint32_t dx = mBlurDirection == BLUR_DIRECTION_X ? GetBlurBoxSize(mStdDeviation) : 0;
  uint32_t dy = mBlurDirection == BLUR_DIRECTION_Y ? GetBlurBoxSize(mStdDeviation) : 0;
  IntRect srcRect = aDestRect;
  InflateRectForBlurDXY(&srcRect, dx, dy);
  return srcRect;
}

IntRect
FilterNodeDirectionalBlurSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  IntRect srcRequest = InflatedSourceOrDestRect(aRect);
  IntRect srcOutput = GetInputRectInRect(IN_DIRECTIONAL_BLUR_IN, srcRequest);
  return InflatedSourceOrDestRect(srcOutput).Intersect(aRect);
}

int32_t
FilterNodeCropSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_CROP_IN: return 0;
    default: return -1;
  }
}

void
FilterNodeCropSoftware::SetAttribute(uint32_t aIndex,
                                     const Rect &aSourceRect)
{
  MOZ_ASSERT(aIndex == ATT_CROP_RECT);
  Rect srcRect = aSourceRect;
  srcRect.Round();
  mCropRect = IntRect(int32_t(srcRect.x), int32_t(srcRect.y),
                      int32_t(srcRect.width), int32_t(srcRect.height));
}

TemporaryRef<DataSourceSurface>
FilterNodeCropSoftware::Render(const IntRect& aRect)
{
  IntRect sourceRect = aRect.Intersect(mCropRect);
  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_CROP_IN, sourceRect);
  return GetDataSurfaceInRect(input, sourceRect, aRect, EDGE_MODE_NONE);
}

IntRect
FilterNodeCropSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  return GetInputRectInRect(IN_CROP_IN, aRect).Intersect(mCropRect);
}

#include "PremultiplyTables.h"

template<const uint8_t* aTable>
static TemporaryRef<DataSourceSurface>
PremultiplyOrUnpremultiplyWithLookupTable(DataSourceSurface* aSurface)
{
  IntSize size = aSurface->GetSize();
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(size, FORMAT_B8G8R8A8);

  uint8_t* inputData = aSurface->GetData();
  int32_t inputStride = aSurface->Stride();
  uint8_t* targetData = target->GetData();
  int32_t targetStride = target->Stride();

  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x++) {
      int32_t inputIndex = y * inputStride + 4 * x;
      int32_t targetIndex = y * targetStride + 4 * x;
      uint8_t alpha = inputData[inputIndex + ARGB32_COMPONENT_BYTEOFFSET_A];
      targetData[targetIndex + ARGB32_COMPONENT_BYTEOFFSET_R] =
        aTable[alpha * 256 + inputData[inputIndex + ARGB32_COMPONENT_BYTEOFFSET_R]];
      targetData[targetIndex + ARGB32_COMPONENT_BYTEOFFSET_G] =
        aTable[alpha * 256 + inputData[inputIndex + ARGB32_COMPONENT_BYTEOFFSET_G]];
      targetData[targetIndex + ARGB32_COMPONENT_BYTEOFFSET_B] =
        aTable[alpha * 256 + inputData[inputIndex + ARGB32_COMPONENT_BYTEOFFSET_B]];
      targetData[targetIndex + ARGB32_COMPONENT_BYTEOFFSET_A] = alpha;
    }
  }

  return target;
}

static TemporaryRef<DataSourceSurface>
Premultiply(DataSourceSurface* aSurface)
{
  IntSize size = aSurface->GetSize();
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(size, FORMAT_B8G8R8A8);

  uint8_t* inputData = aSurface->GetData();
  int32_t inputStride = aSurface->Stride();
  uint8_t* targetData = target->GetData();
  int32_t targetStride = target->Stride();

  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x++) {
      int32_t inputIndex = y * inputStride + 4 * x;
      int32_t targetIndex = y * targetStride + 4 * x;
      uint8_t alpha = inputData[inputIndex + ARGB32_COMPONENT_BYTEOFFSET_A];
      targetData[targetIndex + ARGB32_COMPONENT_BYTEOFFSET_R] =
        FastDivideBy255<uint8_t>(inputData[inputIndex + ARGB32_COMPONENT_BYTEOFFSET_R] * alpha);
      targetData[targetIndex + ARGB32_COMPONENT_BYTEOFFSET_G] =
        FastDivideBy255<uint8_t>(inputData[inputIndex + ARGB32_COMPONENT_BYTEOFFSET_G] * alpha);
      targetData[targetIndex + ARGB32_COMPONENT_BYTEOFFSET_B] =
        FastDivideBy255<uint8_t>(inputData[inputIndex + ARGB32_COMPONENT_BYTEOFFSET_B] * alpha);
      targetData[targetIndex + ARGB32_COMPONENT_BYTEOFFSET_A] = alpha;
    }
  }

  return target;
}

static TemporaryRef<DataSourceSurface>
Unpremultiply(DataSourceSurface* aSurface)
{
  IntSize size = aSurface->GetSize();
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(size, FORMAT_B8G8R8A8);

  uint8_t* inputData = aSurface->GetData();
  int32_t inputStride = aSurface->Stride();
  uint8_t* targetData = target->GetData();
  int32_t targetStride = target->Stride();

  uint16_t alphaFactors[256];
  for (int32_t i = 0; i < 256; i++) {
    alphaFactors[i] = 255 * 255 / i;
  }

  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x++) {
      int32_t inputIndex = y * inputStride + 4 * x;
      int32_t targetIndex = y * targetStride + 4 * x;
      uint8_t alpha = inputData[inputIndex + ARGB32_COMPONENT_BYTEOFFSET_A];
      uint8_t alphaFactor = alphaFactors[alpha];
      targetData[targetIndex + ARGB32_COMPONENT_BYTEOFFSET_R] =
        FastDivideBy255<uint8_t>(inputData[inputIndex + ARGB32_COMPONENT_BYTEOFFSET_R] * alphaFactor);
      targetData[targetIndex + ARGB32_COMPONENT_BYTEOFFSET_G] =
        FastDivideBy255<uint8_t>(inputData[inputIndex + ARGB32_COMPONENT_BYTEOFFSET_G] * alphaFactor);
      targetData[targetIndex + ARGB32_COMPONENT_BYTEOFFSET_B] =
        FastDivideBy255<uint8_t>(inputData[inputIndex + ARGB32_COMPONENT_BYTEOFFSET_B] * alphaFactor);
      targetData[targetIndex + ARGB32_COMPONENT_BYTEOFFSET_A] = alpha;
    }
  }

  return target;
}

int32_t
FilterNodePremultiplySoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_PREMULTIPLY_IN: return 0;
    default: return -1;
  }
}

TemporaryRef<DataSourceSurface>
FilterNodePremultiplySoftware::Render(const IntRect& aRect)
{
  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_PREMULTIPLY_IN, aRect);
  return Premultiply(input);
}

IntRect
FilterNodePremultiplySoftware::GetOutputRectInRect(const IntRect& aRect)
{
  return GetInputRectInRect(IN_PREMULTIPLY_IN, aRect);
}

int32_t
FilterNodeUnpremultiplySoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_UNPREMULTIPLY_IN: return 0;
    default: return -1;
  }
}

TemporaryRef<DataSourceSurface>
FilterNodeUnpremultiplySoftware::Render(const IntRect& aRect)
{
  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_UNPREMULTIPLY_IN, aRect);
  return Unpremultiply(input);
}

IntRect
FilterNodeUnpremultiplySoftware::GetOutputRectInRect(const IntRect& aRect)
{
  return GetInputRectInRect(IN_UNPREMULTIPLY_IN, aRect);
}

void
PointLightSoftware::SetAttribute(uint32_t aIndex, const Point3D &aPoint)
{
  MOZ_ASSERT(aIndex == ATT_POINT_LIGHT_POSITION);
  mPosition = aPoint;
}

SpotLightSoftware::SpotLightSoftware()
 : mSpecularFocus(0)
 , mLimitingConeAngle(0)
{
}

void
SpotLightSoftware::SetAttribute(uint32_t aIndex, const Point3D &aPoint)
{
  switch (aIndex) {
    case ATT_SPOT_LIGHT_POSITION:
      mPosition = aPoint;
      break;
    case ATT_SPOT_LIGHT_POINTS_AT:
      mPointsAt = aPoint;
      break;
    default:
      MOZ_CRASH();
  }
}

void
SpotLightSoftware::SetAttribute(uint32_t aIndex, Float aValue)
{
  switch (aIndex) {
    case ATT_SPOT_LIGHT_LIMITING_CONE_ANGLE:
      mLimitingConeAngle = aValue;
      break;
    case ATT_SPOT_LIGHT_FOCUS:
      mSpecularFocus = aValue;
      break;
    default:
      MOZ_CRASH();
  }
}

DistantLightSoftware::DistantLightSoftware()
 : mAzimuth(0)
 , mElevation(0)
{
}

void
DistantLightSoftware::SetAttribute(uint32_t aIndex, Float aValue)
{
  switch (aIndex) {
    case ATT_DISTANT_LIGHT_AZIMUTH:
      mAzimuth = aValue;
      break;
    case ATT_DISTANT_LIGHT_ELEVATION:
      mElevation = aValue;
      break;
    default:
      MOZ_CRASH();
  }
}

inline float DOT(const float* a, const float* b) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

inline void NORMALIZE(float* vec) {
  float norm = sqrt(DOT(vec, vec));
  vec[0] /= norm;
  vec[1] /= norm;
  vec[2] /= norm;
}


FilterNodeLightingSoftware::FilterNodeLightingSoftware(LightSoftware *aLight)
 : mLight(aLight)
 , mSurfaceScale(0)
 , mKernelUnitLength(0)
{
  MOZ_ASSERT(aLight);
}

int32_t
FilterNodeLightingSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_LIGHTING_IN: return 0;
    default: return -1;
  }
}

void
FilterNodeLightingSoftware::SetAttribute(uint32_t aIndex, const Point3D &aPoint)
{
  mLight->SetAttribute(aIndex, aPoint);
}

void
FilterNodeLightingSoftware::SetAttribute(uint32_t aIndex, Float aValue)
{
  switch (aIndex) {
    case ATT_LIGHTING_SURFACE_SCALE:
      mSurfaceScale = aValue;
      break;
    case ATT_LIGHTING_KERNEL_UNIT_LENGTH:
      mKernelUnitLength = aValue;
      break;
    default:
      mLight->SetAttribute(aIndex, aValue);
      break;
  }
}

void
FilterNodeLightingSoftware::SetAttribute(uint32_t aIndex, const Color &aColor)
{
  MOZ_ASSERT(aIndex == ATT_LIGHTING_COLOR);
  mColor = aColor;
}

IntRect
FilterNodeLightingSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  return GetInputRectInRect(IN_LIGHTING_IN, aRect);
}

void
PointLightSoftware::GetLAndColor(uint8_t lightColor[4], const Point3D &pt, Float L[3], uint8_t color[4])
{
  L[0] = mPosition.x - pt.x;
  L[1] = mPosition.y - pt.y;
  L[2] = mPosition.z - pt.z;
  NORMALIZE(L);

  color[0] = lightColor[0];
  color[1] = lightColor[1];
  color[2] = lightColor[2];
  color[3] = lightColor[3];
}

void
SpotLightSoftware::GetLAndColor(uint8_t lightColor[4], const Point3D &pt, Float L[3], uint8_t color[4])
{
  L[0] = mPosition.x - pt.x;
  L[1] = mPosition.y - pt.y;
  L[2] = mPosition.z - pt.z;
  NORMALIZE(L);

  float S[3];
  S[0] = mPointsAt.x - pt.x;
  S[1] = mPointsAt.y - pt.y;
  S[2] = mPointsAt.z - pt.z;
  NORMALIZE(S);

  float dot = -DOT(L, S);
  const float radPerDeg = static_cast<float>(M_PI/180.0);
  float cosConeAngle = std::max<double>(cos(mLimitingConeAngle * radPerDeg), 0.0);
  float tmp = dot < cosConeAngle ? 0 : pow(dot, mSpecularFocus);
  color[0] = uint8_t(lightColor[0] * tmp);
  color[1] = uint8_t(lightColor[1] * tmp);
  color[2] = uint8_t(lightColor[2] * tmp);
  color[3] = 255;
}

void
DistantLightSoftware::GetLAndColor(uint8_t lightColor[4], const Point3D &pt, Float L[3], uint8_t color[4])
{
  const float radPerDeg = static_cast<float>(M_PI/180.0);
  L[0] = cos(mAzimuth * radPerDeg) * cos(mElevation * radPerDeg);
  L[1] = sin(mAzimuth * radPerDeg) * cos(mElevation * radPerDeg);
  L[2] = sin(mElevation * radPerDeg);
  color[0] = lightColor[0];
  color[1] = lightColor[1];
  color[2] = lightColor[2];
  color[3] = lightColor[3];
}

static int32_t
Convolve3x3(const uint8_t *index, int32_t stride,
            const int8_t kernel[3][3]
#ifdef DEBUG
            , const uint8_t *minData, const uint8_t *maxData
#endif // DEBUG
)
{
  int32_t sum = 0;
  for (int32_t y = 0; y < 3; y++) {
    for (int32_t x = 0; x < 3; x++) {
      int8_t k = kernel[y][x];
      if (k) {
        const uint8_t *valPtr = index + (4 * (x - 1) + stride * (y - 1));
        MOZ_ASSERT(valPtr >= minData, "out of bounds read (before buffer)");
        MOZ_ASSERT(valPtr < maxData,  "out of bounds read (after buffer)");
        sum += k * (*valPtr);
      }
    }
  }
  return sum;
}

static void
GenerateNormal(float *N, const uint8_t *data, int32_t stride,
               int32_t surfaceWidth, int32_t surfaceHeight,
               int32_t x, int32_t y, float surfaceScale)
{
  // See this for source of constants:
  //   http://www.w3.org/TR/SVG11/filters.html#feDiffuseLightingElement
  static const int8_t Kx[3][3][3][3] =
    { { { {  0,  0,  0}, { 0, -2,  2}, { 0, -1,  1} },
        { {  0,  0,  0}, {-2,  0,  2}, {-1,  0,  1} },
        { {  0,  0,  0}, {-2,  2,  0}, {-1,  1,  0} } },
      { { {  0, -1,  1}, { 0, -2,  2}, { 0, -1,  1} },
        { { -1,  0,  1}, {-2,  0,  2}, {-1,  0,  1} },
        { { -1,  1,  0}, {-2,  2,  0}, {-1,  1,  0} } },
      { { {  0, -1,  1}, { 0, -2,  2}, { 0,  0,  0} },
        { { -1,  0,  1}, {-2,  0,  2}, { 0,  0,  0} },
        { { -1,  1,  0}, {-2,  2,  0}, { 0,  0,  0} } } };
  static const int8_t Ky[3][3][3][3] =
    { { { {  0,  0,  0}, { 0, -2, -1}, { 0,  2,  1} },
        { {  0,  0,  0}, {-1, -2, -1}, { 1,  2,  1} },
        { {  0,  0,  0}, {-1, -2,  1}, { 1,  2,  0} } },
      { { {  0, -2, -1}, { 0,  0,  0}, { 0,  2,  1} },
        { { -1, -2, -1}, { 0,  0,  0}, { 1,  2,  1} },
        { { -1, -2,  0}, { 0,  0,  0}, { 1,  2,  0} } },
      { { {  0, -2, -1}, { 0,  2,  1}, { 0,  0,  0} },
        { { -1, -2, -1}, { 1,  2,  1}, { 0,  0,  0} },
        { { -1, -2,  0}, { 1,  2,  0}, { 0,  0,  0} } } };
  static const float FACTORx[3][3] =
    { { 2.0f / 3.0f, 1.0f / 3.0f, 2.0f / 3.0f },
      { 1.0f / 2.0f, 1.0f / 4.0f, 1.0f / 2.0f },
      { 2.0f / 3.0f, 1.0f / 3.0f, 2.0f / 3.0f } };
  static const float FACTORy[3][3] =
    { { 2.0f / 3.0f, 1.0f / 2.0f, 2.0f / 3.0f },
      { 1.0f / 3.0f, 1.0f / 4.0f, 1.0f / 3.0f },
      { 2.0f / 3.0f, 1.0f / 2.0f, 2.0f / 3.0f } };

  // degenerate cases
  if (surfaceWidth == 1 || surfaceHeight == 1) {
    // just return a unit vector pointing towards the viewer
    N[0] = 0;
    N[1] = 0;
    N[2] = 255;
    return;
  }

  int8_t xflag, yflag;
  if (x == 0) {
    xflag = 0;
  } else if (x == surfaceWidth - 1) {
    xflag = 2;
  } else {
    xflag = 1;
  }
  if (y == 0) {
    yflag = 0;
  } else if (y == surfaceHeight - 1) {
    yflag = 2;
  } else {
    yflag = 1;
  }

  const uint8_t *index = data + y * stride + 4 * x + ARGB32_COMPONENT_BYTEOFFSET_A;

#ifdef DEBUG
  // For sanity-checking, to be sure we're not reading outside source buffer:
  const uint8_t* minData = data;
  const uint8_t* maxData = minData + (surfaceHeight * surfaceWidth * stride);

  // We'll sanity-check each value we read inside of Convolve3x3, but we
  // might as well ensure we're passing it a valid pointer to start with, too:
  MOZ_ASSERT(index >= minData, "index points before buffer start");
  MOZ_ASSERT(index < maxData, "index points after buffer end");
#endif // DEBUG

  N[0] = -surfaceScale * FACTORx[yflag][xflag] *
    Convolve3x3(index, stride, Kx[yflag][xflag]
#ifdef DEBUG
                , minData, maxData
#endif // DEBUG
                );

  N[1] = -surfaceScale * FACTORy[yflag][xflag] *
    Convolve3x3(index, stride, Ky[yflag][xflag]
#ifdef DEBUG
                , minData, maxData
#endif // DEBUG
                );
  N[2] = 255;
  NORMALIZE(N);
}

TemporaryRef<DataSourceSurface>
FilterNodeLightingSoftware::Render(const IntRect& aRect)
{
  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_LIGHTING_IN, aRect);

  //info = SetupScalingFilter(...);

  IntSize size = aRect.Size();
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(size, FORMAT_B8G8R8A8);

  uint8_t* sourceData = input->GetData();
  int32_t sourceStride = input->Stride();
  uint8_t* targetData = target->GetData();
  int32_t targetStride = target->Stride();

  uint32_t lightColor = mColor.ToABGR();

  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x++) {
      int32_t sourceIndex = y * sourceStride + 4 * x;
      int32_t targetIndex = y * targetStride + 4 * x;

      float N[3];
      GenerateNormal(N, sourceData, sourceStride, size.width, size.height, x, y, mSurfaceScale);

      Float Z = mSurfaceScale * sourceData[sourceIndex + ARGB32_COMPONENT_BYTEOFFSET_A] / 255;
      Float L[3];
      uint8_t color[4];
      mLight->GetLAndColor((uint8_t*)&lightColor, Point3D(x, y, Z), L, color);

      LightPixel(N, L, color, targetData + targetIndex);
    }
  }

  //FinishScalingFilter(&info);

  return nullptr;
}

FilterNodeDiffuseSoftware::FilterNodeDiffuseSoftware(LightSoftware *aLight)
 : FilterNodeLightingSoftware(aLight)
 , mDiffuseConstant(0)
{
}

void
FilterNodeDiffuseSoftware::SetAttribute(uint32_t aIndex, Float aValue)
{
  switch (aIndex) {
    case ATT_DIFFUSE_LIGHTING_DIFFUSE_CONSTANT:
      mDiffuseConstant = aValue;
      break;
    default:
      MOZ_CRASH();
  }
}

void
FilterNodeDiffuseSoftware::LightPixel(const Float *N, const Float *L,
                                      uint8_t *color, uint8_t *targetData)
{
  float diffuseNL = mDiffuseConstant * DOT(N, L);

  if (diffuseNL < 0) diffuseNL = 0;

  targetData[ARGB32_COMPONENT_BYTEOFFSET_B] =
    std::min(uint32_t(diffuseNL * color[ARGB32_COMPONENT_BYTEOFFSET_B]), 255U);
  targetData[ARGB32_COMPONENT_BYTEOFFSET_G] =
    std::min(uint32_t(diffuseNL * color[ARGB32_COMPONENT_BYTEOFFSET_G]), 255U);
  targetData[ARGB32_COMPONENT_BYTEOFFSET_R] =
    std::min(uint32_t(diffuseNL * color[ARGB32_COMPONENT_BYTEOFFSET_R]), 255U);
  targetData[ARGB32_COMPONENT_BYTEOFFSET_A] = 255;
}

FilterNodeSpecularSoftware::FilterNodeSpecularSoftware(LightSoftware *aLight)
 : FilterNodeLightingSoftware(aLight)
 , mSpecularConstant(0)
 , mSpecularExponent(0)
{
}

void
FilterNodeSpecularSoftware::SetAttribute(uint32_t aIndex, Float aValue)
{
  switch (aIndex) {
    case ATT_SPECULAR_LIGHTING_SPECULAR_CONSTANT:
      mSpecularConstant = aValue;
      break;
    case ATT_SPECULAR_LIGHTING_SPECULAR_EXPONENT:
      mSpecularExponent = aValue;
      break;
    default:
      MOZ_CRASH();
  }
}

void
FilterNodeSpecularSoftware::LightPixel(const Float *N, const Float *L,
                                       uint8_t *color, uint8_t *targetData)
{
  float H[3];
  H[0] = L[0];
  H[1] = L[1];
  H[2] = L[2] + 1;
  NORMALIZE(H);

  Float kS = mSpecularConstant;
  Float dotNH = DOT(N, H);

  bool invalid = dotNH <= 0 || kS <= 0;
  kS *= invalid ? 0 : 1;
  uint8_t minAlpha = invalid ? 255 : 0;

  Float specularNH = kS * pow(dotNH, mSpecularExponent);

  targetData[ARGB32_COMPONENT_BYTEOFFSET_B] =
    std::min(uint32_t(specularNH * color[ARGB32_COMPONENT_BYTEOFFSET_B]), 255U);
  targetData[ARGB32_COMPONENT_BYTEOFFSET_G] =
    std::min(uint32_t(specularNH * color[ARGB32_COMPONENT_BYTEOFFSET_G]), 255U);
  targetData[ARGB32_COMPONENT_BYTEOFFSET_R] =
    std::min(uint32_t(specularNH * color[ARGB32_COMPONENT_BYTEOFFSET_R]), 255U);

  targetData[ARGB32_COMPONENT_BYTEOFFSET_A] =
    std::max(minAlpha, std::max(targetData[ARGB32_COMPONENT_BYTEOFFSET_B],
                            std::max(targetData[ARGB32_COMPONENT_BYTEOFFSET_G],
                                   targetData[ARGB32_COMPONENT_BYTEOFFSET_R])));
}

} // namespace gfx
} // namespace mozilla
