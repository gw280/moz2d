/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define _USE_MATH_DEFINES

#include <cmath>
#include "FilterNodeSoftware.h"
#include "2D.h"
#include "Tools.h"
#include "Blur.h"
#include <set>
#include "SVGTurbulenceRenderer.h"
#include "SIMD.h"

#ifdef DEBUG_DUMP_SURFACES
#include "gfxImageSurface.h"
#include "gfx2DGlue.h"
namespace mozilla {
namespace gfx {
static void
DumpAsPNG(SourceSurface* aSurface)
{
  RefPtr<DataSourceSurface> dataSource = aSurface->GetDataSurface();
  IntSize size = dataSource->GetSize();
  nsRefPtr<gfxImageSurface> imageSurface =
    new gfxImageSurface(dataSource->GetData(), gfxIntSize(size.width, size.height),
                        dataSource->Stride(), SurfaceFormatToImageFormat(aSurface->GetFormat()));
  imageSurface->PrintAsDataURL();
}
} // namespace gfx
} // namespace mozilla
#endif

const ptrdiff_t B8G8R8A8_COMPONENT_BYTEOFFSET_B = 0;
const ptrdiff_t B8G8R8A8_COMPONENT_BYTEOFFSET_G = 1;
const ptrdiff_t B8G8R8A8_COMPONENT_BYTEOFFSET_R = 2;
const ptrdiff_t B8G8R8A8_COMPONENT_BYTEOFFSET_A = 3;

namespace mozilla {
namespace gfx {

namespace {

class PointLightSoftware
{
public:
  bool SetAttribute(uint32_t aIndex, Float) { return false; }
  bool SetAttribute(uint32_t aIndex, const Point3D &);
  void Prepare() {}
  Point3D GetInverseRayDirection(const Point3D &aTargetPoint);
  uint32_t GetColor(uint32_t aLightColor, const Point3D &aInverseRayDirection);

private:
  Point3D mPosition;
};

class SpotLightSoftware
{
public:
  SpotLightSoftware();
  bool SetAttribute(uint32_t aIndex, Float);
  bool SetAttribute(uint32_t aIndex, const Point3D &);
  void Prepare();
  Point3D GetInverseRayDirection(const Point3D &aTargetPoint);
  uint32_t GetColor(uint32_t aLightColor, const Point3D &aInverseRayDirection);

private:
  Point3D mPosition;
  Point3D mPointsAt;
  Point3D mInverseCoreRayDirection;
  Float mSpecularFocus;
  Float mLimitingConeAngle;
  Float mLimitingConeCos;
};

class DistantLightSoftware
{
public:
  DistantLightSoftware();
  bool SetAttribute(uint32_t aIndex, Float);
  bool SetAttribute(uint32_t aIndex, const Point3D &) { return false; }
  void Prepare();
  Point3D GetInverseRayDirection(const Point3D &aTargetPoint);
  uint32_t GetColor(uint32_t aLightColor, const Point3D &aInverseRayDirection);

private:
  Float mAzimuth;
  Float mElevation;
  Point3D mInverseRayDirection;
};

class DiffuseLightingSoftware
{
public:
  DiffuseLightingSoftware();
  bool SetAttribute(uint32_t aIndex, Float);
  uint32_t LightPixel(const Point3D &aNormal, const Point3D &aInverseRayDirection,
                      uint32_t aColor);

private:
  Float mDiffuseConstant;
};

class SpecularLightingSoftware
{
public:
  SpecularLightingSoftware();
  bool SetAttribute(uint32_t aIndex, Float);
  uint32_t LightPixel(const Point3D &aNormal, const Point3D &aInverseRayDirection,
                      uint32_t aColor);

private:
  Float mSpecularConstant;
  Float mSpecularExponent;
};

} // unnamed namespace

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

// from xpcom/ds/nsMathUtils.h
static int32_t
NS_lround(double x)
{
  return x >= 0.0 ? int32_t(x + 0.5) : int32_t(x - 0.5);
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
  MOZ_ASSERT(aSrc->GetFormat() == aDest->GetFormat(), "different surface formats");
  uint8_t* sourceData = aSrc->GetData();
  uint32_t sourceStride = aSrc->Stride();
  uint8_t* destData = aDest->GetData();
  uint32_t destStride = aDest->Stride();

  sourceData += DataOffset(aSrc, aSrcRect.TopLeft());
  destData += DataOffset(aDest, aDestPoint);

  if (BytesPerPixel(aSrc->GetFormat()) == 4) {
    for (int32_t y = 0; y < aSrcRect.height; y++) {
      for (int32_t x = 0; x < aSrcRect.width; x++) {
        *((int32_t*)destData + x) = *((int32_t*)sourceData + x);
      }
      sourceData += sourceStride;
      destData += destStride;
    }
  } else if (BytesPerPixel(aSrc->GetFormat()) == 1) {
    for (int32_t y = 0; y < aSrcRect.height; y++) {
      for (int32_t x = 0; x < aSrcRect.width; x++) {
        destData[x] = sourceData[x];
      }
      sourceData += sourceStride;
      destData += destStride;
    }
  }
}

TemporaryRef<DataSourceSurface>
CloneAligned(DataSourceSurface* aSource)
{
  RefPtr<DataSourceSurface> copy =
    Factory::CreateDataSourceSurface(aSource->GetSize(), aSource->GetFormat());
  CopyRect(aSource, copy, IntRect(IntPoint(), aSource->GetSize()), IntPoint());
  return copy;
}

static void
FillRectWithPixel(DataSourceSurface *aSurface, const IntRect &aFillRect, IntPoint aPixelPos)
{
  uint8_t* data = aSurface->GetData();
  uint8_t* sourcePixelData = data + DataOffset(aSurface, aPixelPos);
  int32_t stride = aSurface->Stride();
  data += DataOffset(aSurface, aFillRect.TopLeft());
  if (BytesPerPixel(aSurface->GetFormat()) == 4) {
    uint32_t sourcePixel = *(uint32_t*)sourcePixelData;
    for (int32_t y = 0; y < aFillRect.height; y++) {
      for (int32_t x = 0; x < aFillRect.width; x++) {
        *((uint32_t*)data + x) = sourcePixel;
      }
      data += stride;
    }
  } else if (BytesPerPixel(aSurface->GetFormat()) == 1) {
    uint8_t sourcePixel = *sourcePixelData;
    for (int32_t y = 0; y < aFillRect.height; y++) {
      for (int32_t x = 0; x < aFillRect.width; x++) {
        data[x] = sourcePixel;
      }
      data += stride;
    }
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
  if (BytesPerPixel(aSurface->GetFormat()) == 4) {
    for (int32_t y = 0; y < aFillRect.height; y++) {
      for (int32_t x = 0; x < aFillRect.width; x++) {
        *((uint32_t*)data + x) = *((uint32_t*)sampleData + x);
      }
      data += stride;
    }
  } else if (BytesPerPixel(aSurface->GetFormat()) == 1) {
    for (int32_t y = 0; y < aFillRect.height; y++) {
      for (int32_t x = 0; x < aFillRect.width; x++) {
        data[x] = sampleData[x];
      }
      data += stride;
    }
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
  if (BytesPerPixel(aSurface->GetFormat()) == 4) {
    for (int32_t y = 0; y < aFillRect.height; y++) {
      int32_t sampleColor = *((uint32_t*)sampleData);
      for (int32_t x = 0; x < aFillRect.width; x++) {
        *((uint32_t*)data + x) = sampleColor;
      }
      data += stride;
      sampleData += stride;
    }
  } else if (BytesPerPixel(aSurface->GetFormat()) == 1) {
    for (int32_t y = 0; y < aFillRect.height; y++) {
      int8_t sampleColor = *sampleData;
      for (int32_t x = 0; x < aFillRect.width; x++) {
        data[x] = sampleColor;
      }
      data += stride;
      sampleData += stride;
    }
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
        FillRectWithPixel(aSurface, fill, sampleRect.TopLeft());
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
    Factory::CreateDataSourceSurface(aDestRect.Size(), aSurface->GetFormat());

  if (!target || !dataSource) {
    return nullptr;
  }

  if (aEdgeMode == EDGE_MODE_WRAP) {
    TileSurface(dataSource, target, intersectInDestSpace.TopLeft());
    return target;
  }

  if (aEdgeMode == EDGE_MODE_NONE && !aSurfaceRect.Contains(aDestRect)) {
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
      filter = new FilterNodeLightingSoftware<PointLightSoftware, DiffuseLightingSoftware>();
      break;
    case FILTER_POINT_SPECULAR:
      filter = new FilterNodeLightingSoftware<PointLightSoftware, SpecularLightingSoftware>();
      break;
    case FILTER_SPOT_DIFFUSE:
      filter = new FilterNodeLightingSoftware<SpotLightSoftware, DiffuseLightingSoftware>();
      break;
    case FILTER_SPOT_SPECULAR:
      filter = new FilterNodeLightingSoftware<SpotLightSoftware, SpecularLightingSoftware>();
      break;
    case FILTER_DISTANT_DIFFUSE:
      filter = new FilterNodeLightingSoftware<DistantLightSoftware, DiffuseLightingSoftware>();
      break;
    case FILTER_DISTANT_SPECULAR:
      filter = new FilterNodeLightingSoftware<DistantLightSoftware, SpecularLightingSoftware>();
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
#ifdef DEBUG_DUMP_SURFACES
  printf("<pre>\nRendering...\n");
#endif

  Rect renderRect = aSourceRect;
  renderRect.RoundOut();
  IntRect renderIntRect(int32_t(renderRect.x), int32_t(renderRect.y),
                        int32_t(renderRect.width), int32_t(renderRect.height));
  IntRect outputRect = renderIntRect.Intersect(GetOutputRectInRect(renderIntRect));

  // Render.
  RefPtr<DataSourceSurface> result = GetOutput(outputRect);
  if (!result) {
    return;
  }

  // Add transparency around outputRect in renderIntRect.
  result = GetDataSurfaceInRect(result, outputRect, renderIntRect, EDGE_MODE_NONE);
  if (!result) {
    return;
  }

#ifdef DEBUG_DUMP_SURFACES
  printf("output:\n");
  printf("<img src='"); DumpAsPNG(result); printf("'>\n");
  printf("</pre>\n");
#endif

  // Draw.
  aDrawTarget->DrawSurface(result, Rect(aDestPoint, aSourceRect.Size()),
                           aSourceRect - renderIntRect.TopLeft(),
                           DrawSurfaceOptions(), aOptions);
}

TemporaryRef<DataSourceSurface>
FilterNodeSoftware::GetOutput(const IntRect &aRect)
{
  if (aRect.IsEmpty()) {
    return nullptr;
  }

  MOZ_ASSERT(GetOutputRectInRect(aRect).Contains(aRect));
  if (!mCachedRect.Contains(aRect)) {
    RequestRect(aRect);
    mCachedOutput = Render(mRequestedRect);
    if (!mCachedOutput) {
      mCachedRect = IntRect();
      mRequestedRect = IntRect();
      return nullptr;
    }
    mCachedRect = mRequestedRect;
    mRequestedRect = IntRect();
  } else {
    MOZ_ASSERT(mCachedOutput, "cached rect but no cached output?");
  }
  return GetDataSurfaceInRect(mCachedOutput, mCachedRect, aRect, EDGE_MODE_NONE);
}

void
FilterNodeSoftware::RequestRect(const IntRect &aRect)
{
  mRequestedRect = mRequestedRect.Union(aRect);
  RequestFromInputsForRect(aRect);
}

void
FilterNodeSoftware::RequestInputRect(uint32_t aInputEnumIndex, const IntRect &aRect)
{
  int32_t inputIndex = InputIndex(aInputEnumIndex);
  if (inputIndex < 0 || (uint32_t)inputIndex >= NumberOfSetInputs()) {
    MOZ_CRASH();
  }
  if (mInputSurfaces[inputIndex]) {
    return;
  }
  RefPtr<FilterNodeSoftware> filter = mInputFilters[inputIndex];
  MOZ_ASSERT(filter, "missing input");
  filter->RequestRect(filter->GetOutputRectInRect(aRect));
}

template<typename m128i_t>
TemporaryRef<DataSourceSurface>
ConvertToB8G8R8A8(SourceSurface* aSurface)
{
  IntSize size = aSurface->GetSize();
  RefPtr<DataSourceSurface> input = aSurface->GetDataSurface();
  RefPtr<DataSourceSurface> output =
    Factory::CreateDataSourceSurface(size, FORMAT_B8G8R8A8);
  uint8_t *inputData = input->GetData();
  uint8_t *outputData = output->GetData();
  int32_t inputStride = input->Stride();
  int32_t outputStride = output->Stride();
  switch (input->GetFormat()) {
    case FORMAT_B8G8R8A8:
      output = input;
      break;
    case FORMAT_B8G8R8X8:
      for (int32_t y = 0; y < size.height; y++) {
        for (int32_t x = 0; x < size.width; x++) {
          int32_t inputIndex = y * inputStride + 4 * x;
          int32_t outputIndex = y * outputStride + 4 * x;
          outputData[outputIndex + 0] = inputData[inputIndex + 0];
          outputData[outputIndex + 1] = inputData[inputIndex + 1];
          outputData[outputIndex + 2] = inputData[inputIndex + 2];
          outputData[outputIndex + 3] = 255;
        }
      }
      break;
    case FORMAT_R8G8B8A8:
      for (int32_t y = 0; y < size.height; y++) {
        for (int32_t x = 0; x < size.width; x++) {
          int32_t inputIndex = y * inputStride + 4 * x;
          int32_t outputIndex = y * outputStride + 4 * x;
          outputData[outputIndex + 2] = inputData[inputIndex + 0];
          outputData[outputIndex + 1] = inputData[inputIndex + 1];
          outputData[outputIndex + 0] = inputData[inputIndex + 2];
          outputData[outputIndex + 3] = inputData[inputIndex + 3];
        }
      }
      break;
    case FORMAT_R8G8B8X8:
      for (int32_t y = 0; y < size.height; y++) {
        for (int32_t x = 0; x < size.width; x++) {
          int32_t inputIndex = y * inputStride + 4 * x;
          int32_t outputIndex = y * outputStride + 4 * x;
          outputData[outputIndex + 2] = inputData[inputIndex + 0];
          outputData[outputIndex + 1] = inputData[inputIndex + 1];
          outputData[outputIndex + 0] = inputData[inputIndex + 2];
          outputData[outputIndex + 3] = 255;
        }
      }
      break;
    case FORMAT_A8:
      for (int32_t y = 0; y < size.height; y++) {
        for (int32_t x = 0; x < size.width; x += 16) {
          int32_t inputIndex = y * inputStride + x;
          int32_t outputIndex = y * outputStride + 4 * x;
          m128i_t p1To16 = simd::LoadFrom<m128i_t>((m128i_t*)&inputData[inputIndex]);
          m128i_t zero = simd::FromZero8<m128i_t>();
          m128i_t p1To8 = simd::InterleaveLo8(zero, p1To16);
          m128i_t p9To16 = simd::InterleaveHi8(zero, p1To16);
          m128i_t p1To4 = simd::InterleaveLo8(zero, p1To8);
          m128i_t p5To8 = simd::InterleaveHi8(zero, p1To8);
          m128i_t p9To12 = simd::InterleaveLo8(zero, p9To16);
          m128i_t p13To16 = simd::InterleaveHi8(zero, p9To16);
          simd::StoreTo((m128i_t*)&outputData[outputIndex], p1To4);
          if (outputStride > (x + 4) * 4) {
            simd::StoreTo((m128i_t*)&outputData[outputIndex+16], p5To8);
          }
          if (outputStride > (x + 8) * 4) {
            simd::StoreTo((m128i_t*)&outputData[outputIndex+32], p9To12);
          }
          if (outputStride > (x + 12) * 4) {
            simd::StoreTo((m128i_t*)&outputData[outputIndex+48], p13To16);
          }
        }
      }
      break;
    default:
      output = nullptr;
      break;
  }
  return output;
}

SurfaceFormat
FilterNodeSoftware::DesiredFormat(SurfaceFormat aCurrentFormat,
                                  FormatHint aFormatHint)
{
  if (aCurrentFormat == FORMAT_A8 && aFormatHint == CAN_HANDLE_A8) {
    return FORMAT_A8;
  }
  return FORMAT_B8G8R8A8;
}

TemporaryRef<DataSourceSurface>
FilterNodeSoftware::GetInputDataSourceSurface(uint32_t aInputEnumIndex,
                                              const IntRect& aRect,
                                              FormatHint aFormatHint,
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
    // XXXmstange This is bad, we need to handle requested rects that are
    // completely outside the input filter's output rect more gracefully than
    // just returning nullptr.
    if (!inputFilterOutput.IsEmpty()) {
      surface = filter->GetOutput(inputFilterOutput);
      surfaceRect = inputFilterOutput;
      MOZ_ASSERT(!surface || surfaceRect.Size() == surface->GetSize());
    }
  }

  if (!surface || surface->GetFormat() == FORMAT_UNKNOWN) {
    return nullptr;
  }

  RefPtr<DataSourceSurface> result =
    GetDataSurfaceInRect(surface, surfaceRect, aRect, aEdgeMode);

  if (result->Stride() != GetAlignedStride<16>(result->Stride()) ||
      reinterpret_cast<uintptr_t>(result->GetData()) % 16 != 0) {
    // Align unaligned surface.
    result = CloneAligned(result);
  }

  SurfaceFormat currentFormat = result->GetFormat();
  if (DesiredFormat(currentFormat, aFormatHint) == FORMAT_B8G8R8A8 &&
      currentFormat != FORMAT_B8G8R8A8) {
    if (Factory::HasSSE2()) {
#ifdef COMPILE_WITH_SSE2
      result = ConvertToB8G8R8A8<__m128i>(result);
#endif
    } else {
      result = ConvertToB8G8R8A8<simd::ScalarM128i>(result);
    }
  }

#ifdef DEBUG_DUMP_SURFACES
  printf("input:\n");
  printf("<img src='"); DumpAsPNG(result); printf("'>\n");
#endif

  MOZ_ASSERT(!result || result->GetSize() == aRect.Size(), "wrong surface size");

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
FilterNodeSoftware::AddInvalidationListener(FilterInvalidationListener* aListener)
{
  MOZ_ASSERT(aListener, "null listener");
  mInvalidationListeners.push_back(aListener);
}

void
FilterNodeSoftware::RemoveInvalidationListener(FilterInvalidationListener* aListener)
{
  MOZ_ASSERT(aListener, "null listener");
  std::vector<FilterInvalidationListener*>::iterator it =
    std::find(mInvalidationListeners.begin(), mInvalidationListeners.end(), aListener);
  mInvalidationListeners.erase(it);
}

void
FilterNodeSoftware::FilterInvalidated(FilterNodeSoftware* aFilter)
{
  Invalidate();
}

void
FilterNodeSoftware::Invalidate()
{
  mCachedOutput = nullptr;
  mCachedRect = IntRect();
  for (std::vector<FilterInvalidationListener*>::iterator it = mInvalidationListeners.begin();
       it != mInvalidationListeners.end(); it++) {
    (*it)->FilterInvalidated(this);
  }
}

FilterNodeSoftware::~FilterNodeSoftware()
{
  MOZ_ASSERT(!mInvalidationListeners.size(),
             "All invalidation listeners should have unsubscribed themselves by now!");

  for (std::vector<RefPtr<FilterNodeSoftware> >::iterator it = mInputFilters.begin();
       it != mInputFilters.end(); it++) {
    if (*it) {
      (*it)->RemoveInvalidationListener(this);
    }
  }
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
  if ((uint32_t)inputIndex >= mInputSurfaces.size()) {
    mInputSurfaces.resize(inputIndex + 1);
  }
  if ((uint32_t)inputIndex >= mInputFilters.size()) {
    mInputFilters.resize(inputIndex + 1);
  }
  mInputSurfaces[inputIndex] = aSurface;
  if (mInputFilters[inputIndex]) {
    mInputFilters[inputIndex]->RemoveInvalidationListener(this);
  }
  if (aFilter) {
    aFilter->AddInvalidationListener(this);
  }
  mInputFilters[inputIndex] = aFilter;
  Invalidate();
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
  Invalidate();
}

template<typename m128i_t, uint32_t aBlendMode>
static void
BlendTwoComponentsOfFourPixels(m128i_t source, m128i_t sourceAlpha,
                               m128i_t dest, m128i_t destAlpha,
                               m128i_t& blendedComponent1, m128i_t& blendedComponent2)
{
  m128i_t x255 = simd::From16<m128i_t>(255);

  switch (aBlendMode) {

    case BLEND_MODE_MULTIPLY:
    {
      // val = ((255 - destAlpha) * source + (255 - sourceAlpha + source) * dest);
      m128i_t twoFiftyFiveMinusDestAlpha = simd::Sub16(x255, destAlpha);
      m128i_t twoFiftyFiveMinusSourceAlpha = simd::Sub16(x255, sourceAlpha);
      m128i_t twoFiftyFiveMinusSourceAlphaPlusSource = simd::Add16(twoFiftyFiveMinusSourceAlpha, source);

      m128i_t sourceInterleavedWithDest1 = simd::InterleaveLo16(source, dest);
      m128i_t leftFactor1 = simd::InterleaveLo16(twoFiftyFiveMinusDestAlpha, twoFiftyFiveMinusSourceAlphaPlusSource);
      blendedComponent1 = simd::MulAdd2x8x16To4x32(sourceInterleavedWithDest1, leftFactor1);
      blendedComponent1 = simd::FastDivideBy255(blendedComponent1);

      m128i_t sourceInterleavedWithDest2 = simd::InterleaveHi16(source, dest);
      m128i_t leftFactor2 = simd::InterleaveHi16(twoFiftyFiveMinusDestAlpha, twoFiftyFiveMinusSourceAlphaPlusSource);
      blendedComponent2 = simd::MulAdd2x8x16To4x32(sourceInterleavedWithDest2, leftFactor2);
      blendedComponent2 = simd::FastDivideBy255(blendedComponent2);

      break;
    }

    case BLEND_MODE_SCREEN:
    {
      // val = 255 * (source + dest) + (0 - dest) * source;
      m128i_t sourcePlusDest = simd::Add16(source, dest);
      m128i_t zeroMinusDest = simd::Sub16(simd::From16<m128i_t>(0), dest);

      m128i_t twoFiftyFiveInterleavedWithZeroMinusDest1 = simd::InterleaveLo16(x255, zeroMinusDest);
      m128i_t sourcePlusDestInterleavedWithSource1 = simd::InterleaveLo16(sourcePlusDest, source);
      blendedComponent1 = simd::MulAdd2x8x16To4x32(twoFiftyFiveInterleavedWithZeroMinusDest1, sourcePlusDestInterleavedWithSource1);
      blendedComponent1 = simd::FastDivideBy255(blendedComponent1);

      m128i_t twoFiftyFiveInterleavedWithZeroMinusDest2 = simd::InterleaveHi16(x255, zeroMinusDest);
      m128i_t sourcePlusDestInterleavedWithSource2 = simd::InterleaveHi16(sourcePlusDest, source);
      blendedComponent2 = simd::MulAdd2x8x16To4x32(twoFiftyFiveInterleavedWithZeroMinusDest2, sourcePlusDestInterleavedWithSource2);
      blendedComponent2 = simd::FastDivideBy255(blendedComponent2);

      break;
    }

    case BLEND_MODE_DARKEN:
    case BLEND_MODE_LIGHTEN:
    {
      // Darken:
      // val = min((255 - destAlpha) * source + 255                 * dest,
      //           255               * source + (255 - sourceAlpha) * dest);
      //
      // Lighten:
      // val = max((255 - destAlpha) * source + 255                 * dest,
      //           255               * source + (255 - sourceAlpha) * dest);

      m128i_t twoFiftyFiveMinusDestAlpha = simd::Sub16(x255, destAlpha);
      m128i_t twoFiftyFiveMinusSourceAlpha = simd::Sub16(x255, sourceAlpha);

      m128i_t twoFiftyFiveMinusDestAlphaInterleavedWithTwoFiftyFive1 = simd::InterleaveLo16(twoFiftyFiveMinusDestAlpha, x255);
      m128i_t twoFiftyFiveInterleavedWithTwoFiftyFiveMinusSourceAlpha1 = simd::InterleaveLo16(x255, twoFiftyFiveMinusSourceAlpha);
      m128i_t sourceInterleavedWithDest1 = simd::InterleaveLo16(source, dest);
      m128i_t product1_1 = simd::MulAdd2x8x16To4x32(twoFiftyFiveMinusDestAlphaInterleavedWithTwoFiftyFive1, sourceInterleavedWithDest1);
      m128i_t product1_2 = simd::MulAdd2x8x16To4x32(twoFiftyFiveInterleavedWithTwoFiftyFiveMinusSourceAlpha1, sourceInterleavedWithDest1);
      blendedComponent1 = aBlendMode == BLEND_MODE_DARKEN ? simd::Min32(product1_1, product1_2) : simd::Max32(product1_1, product1_2);
      blendedComponent1 = simd::FastDivideBy255(blendedComponent1);

      m128i_t twoFiftyFiveMinusDestAlphaInterleavedWithTwoFiftyFive2 = simd::InterleaveHi16(twoFiftyFiveMinusDestAlpha, x255);
      m128i_t twoFiftyFiveInterleavedWithTwoFiftyFiveMinusSourceAlpha2 = simd::InterleaveHi16(x255, twoFiftyFiveMinusSourceAlpha);
      m128i_t sourceInterleavedWithDest2 = simd::InterleaveHi16(source, dest);
      m128i_t product2_1 = simd::MulAdd2x8x16To4x32(twoFiftyFiveMinusDestAlphaInterleavedWithTwoFiftyFive2, sourceInterleavedWithDest2);
      m128i_t product2_2 = simd::MulAdd2x8x16To4x32(twoFiftyFiveInterleavedWithTwoFiftyFiveMinusSourceAlpha2, sourceInterleavedWithDest2);
      blendedComponent2 = aBlendMode == BLEND_MODE_DARKEN ? simd::Min32(product2_1, product2_2) : simd::Max32(product2_1, product2_2);
      blendedComponent2 = simd::FastDivideBy255(blendedComponent2);

      break;
    }

  }
}

template<typename m128i_t>
static m128i_t
BlendAlphaOfFourPixels(m128i_t s_rrrraaaa1234, m128i_t d_rrrraaaa1234)
{
  // uint32_t alpha = 255 * 255 + (destAlpha - 255) * (255 - sourceAlpha);
  m128i_t destAlpha = simd::InterleaveHi16(d_rrrraaaa1234, simd::From16<m128i_t>(2 * 255));
  m128i_t sourceAlpha = simd::InterleaveHi16(s_rrrraaaa1234, simd::From16<m128i_t>(0));
  m128i_t f1 = simd::Sub16(destAlpha, simd::From16<m128i_t>(255));
  m128i_t f2 = simd::Sub16(simd::From16<m128i_t>(255), sourceAlpha);
  return simd::FastDivideBy255(simd::MulAdd2x8x16To4x32(f1, f2));
}

template<typename m128i_t>
static void
UnpackAndShuffleComponents(m128i_t bgrabgrabgrabgra1234,
                           m128i_t& bbbbgggg1234, m128i_t& rrrraaaa1234)
{
  m128i_t bgrabgra12 = simd::UnpackLo8x8To8x16(bgrabgrabgrabgra1234);
  m128i_t bgrabgra34 = simd::UnpackHi8x8To8x16(bgrabgrabgrabgra1234);
  m128i_t bbggrraa13 = simd::InterleaveLo16(bgrabgra12, bgrabgra34);
  m128i_t bbggrraa24 = simd::InterleaveHi16(bgrabgra12, bgrabgra34);
  bbbbgggg1234 = simd::InterleaveLo16(bbggrraa13, bbggrraa24);
  rrrraaaa1234 = simd::InterleaveHi16(bbggrraa13, bbggrraa24);
}

template<typename m128i_t>
static m128i_t
ShuffleAndPackComponents(m128i_t bbbb1234, m128i_t gggg1234,
                         m128i_t rrrr1234, m128i_t aaaa1234)
{
  m128i_t bbbbgggg1234 = simd::PackAndSaturate32To16(bbbb1234, gggg1234);
  m128i_t rrrraaaa1234 = simd::PackAndSaturate32To16(rrrr1234, aaaa1234);
  m128i_t brbrbrbr1234 = simd::InterleaveLo16(bbbbgggg1234, rrrraaaa1234);
  m128i_t gagagaga1234 = simd::InterleaveHi16(bbbbgggg1234, rrrraaaa1234);
  m128i_t bgrabgra12 = simd::InterleaveLo16(brbrbrbr1234, gagagaga1234);
  m128i_t bgrabgra34 = simd::InterleaveHi16(brbrbrbr1234, gagagaga1234);
  return simd::PackAndSaturate16To8(bgrabgra12, bgrabgra34);
}

template<typename m128i_t, BlendMode mode>
static TemporaryRef<DataSourceSurface>
ApplyBlending_SIMD(DataSourceSurface* aInput1, DataSourceSurface* aInput2)
{
  IntSize size = aInput1->GetSize();
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(size, FORMAT_B8G8R8A8);
  if (!target) {
    return nullptr;
  }

  uint8_t* source1Data = aInput1->GetData();
  uint8_t* source2Data = aInput2->GetData();
  uint8_t* targetData = target->GetData();
  int32_t targetStride = target->Stride();
  int32_t source1Stride = aInput1->Stride();
  int32_t source2Stride = aInput2->Stride();

  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x += 4) {
      int32_t targetIndex = y * targetStride + 4 * x;
      int32_t source1Index = y * source1Stride + 4 * x;
      int32_t source2Index = y * source2Stride + 4 * x;

      m128i_t s1234 = simd::LoadFrom((m128i_t*)&source2Data[source2Index]);
      m128i_t d1234 = simd::LoadFrom((m128i_t*)&source1Data[source1Index]);

      m128i_t s_bbbbgggg1234, s_rrrraaaa1234;
      m128i_t d_bbbbgggg1234, d_rrrraaaa1234;
      UnpackAndShuffleComponents(s1234, s_bbbbgggg1234, s_rrrraaaa1234);
      UnpackAndShuffleComponents(d1234, d_bbbbgggg1234, d_rrrraaaa1234);
      m128i_t s_aaaaaaaa1234 = simd::Shuffle32<3,2,3,2>(s_rrrraaaa1234);
      m128i_t d_aaaaaaaa1234 = simd::Shuffle32<3,2,3,2>(d_rrrraaaa1234);
      m128i_t blendedB, blendedG, blendedR, blendedA;
      BlendTwoComponentsOfFourPixels<m128i_t,mode>(s_bbbbgggg1234, s_aaaaaaaa1234, d_bbbbgggg1234, d_aaaaaaaa1234, blendedB, blendedG);
      BlendTwoComponentsOfFourPixels<m128i_t,mode>(s_rrrraaaa1234, s_aaaaaaaa1234, d_rrrraaaa1234, d_aaaaaaaa1234, blendedR, blendedA);
      blendedA = BlendAlphaOfFourPixels(s_rrrraaaa1234, d_rrrraaaa1234);

      m128i_t result1234 = ShuffleAndPackComponents(blendedB, blendedG, blendedR, blendedA);
      simd::StoreTo((m128i_t*)&targetData[targetIndex], result1234);
    }
  }

  return target;
}

template<BlendMode aBlendMode>
static TemporaryRef<DataSourceSurface>
ApplyBlending_Scalar(DataSourceSurface* aInput1, DataSourceSurface* aInput2)
{
  IntSize size = aInput1->GetSize();
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(size, FORMAT_B8G8R8A8);
  if (!target) {
    return nullptr;
  }

  uint8_t* source1Data = aInput1->GetData();
  uint8_t* source2Data = aInput2->GetData();
  uint8_t* targetData = target->GetData();
  uint32_t targetStride = target->Stride();
  uint32_t source1Stride = aInput1->Stride();
  uint32_t source2Stride = aInput2->Stride();

  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x++) {
      uint32_t targetIndex = y * targetStride + 4 * x;
      uint32_t source1Index = y * source1Stride + 4 * x;
      uint32_t source2Index = y * source2Stride + 4 * x;
      uint32_t qa = source1Data[source1Index + B8G8R8A8_COMPONENT_BYTEOFFSET_A];
      uint32_t qb = source2Data[source2Index + B8G8R8A8_COMPONENT_BYTEOFFSET_A];
      for (int32_t i = std::min(B8G8R8A8_COMPONENT_BYTEOFFSET_B, B8G8R8A8_COMPONENT_BYTEOFFSET_R);
           i <= std::max(B8G8R8A8_COMPONENT_BYTEOFFSET_B, B8G8R8A8_COMPONENT_BYTEOFFSET_R); i++) {
        uint32_t ca = source1Data[source1Index + i];
        uint32_t cb = source2Data[source2Index + i];
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
        targetData[targetIndex + i] = static_cast<uint8_t>(val);
      }
      uint32_t alpha = 255 * 255 - (255 - qa) * (255 - qb);
      targetData[targetIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_A] =
        FastDivideBy255<uint8_t>(alpha);
    }
  }

  return target;
}

template<typename m128i_t>
static TemporaryRef<DataSourceSurface>
ApplyBlending_SIMD(DataSourceSurface* aInput1, DataSourceSurface* aInput2,
                      BlendMode aBlendMode)
{
  switch (aBlendMode) {
    case BLEND_MODE_MULTIPLY:
      return ApplyBlending_SIMD<m128i_t, BLEND_MODE_MULTIPLY>(aInput1, aInput2);
    case BLEND_MODE_SCREEN:
      return ApplyBlending_SIMD<m128i_t, BLEND_MODE_SCREEN>(aInput1, aInput2);
    case BLEND_MODE_DARKEN:
      return ApplyBlending_SIMD<m128i_t, BLEND_MODE_DARKEN>(aInput1, aInput2);
    case BLEND_MODE_LIGHTEN:
      return ApplyBlending_SIMD<m128i_t, BLEND_MODE_LIGHTEN>(aInput1, aInput2);
  }
}

static TemporaryRef<DataSourceSurface>
ApplyBlending_Scalar(DataSourceSurface* aInput1, DataSourceSurface* aInput2,
                        BlendMode aBlendMode)
{
  switch (aBlendMode) {
    case BLEND_MODE_MULTIPLY:
      return ApplyBlending_Scalar<BLEND_MODE_MULTIPLY>(aInput1, aInput2);
    case BLEND_MODE_SCREEN:
      return ApplyBlending_Scalar<BLEND_MODE_SCREEN>(aInput1, aInput2);
    case BLEND_MODE_DARKEN:
      return ApplyBlending_Scalar<BLEND_MODE_DARKEN>(aInput1, aInput2);
    case BLEND_MODE_LIGHTEN:
      return ApplyBlending_Scalar<BLEND_MODE_LIGHTEN>(aInput1, aInput2);
  }
}

TemporaryRef<DataSourceSurface>
FilterNodeBlendSoftware::Render(const IntRect& aRect)
{
  RefPtr<DataSourceSurface> input1 =
    GetInputDataSourceSurface(IN_BLEND_IN, aRect, NEED_COLOR_CHANNELS);
  RefPtr<DataSourceSurface> input2 =
    GetInputDataSourceSurface(IN_BLEND_IN2, aRect, NEED_COLOR_CHANNELS);
  if (!input1 || !input2) {
    return nullptr;
  }

#ifdef COMPILE_WITH_SSE2
  if (Factory::HasSSE2()) {
    return ApplyBlending_SIMD<__m128i>(input1, input2, mBlendMode);
  }
#endif
  return ApplyBlending_Scalar(input1, input2, mBlendMode);
}

void
FilterNodeBlendSoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  RequestInputRect(IN_BLEND_IN, aRect);
  RequestInputRect(IN_BLEND_IN2, aRect);
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
  Invalidate();
}

void
FilterNodeMorphologySoftware::SetAttribute(uint32_t aIndex,
                                           uint32_t aOperator)
{
  MOZ_ASSERT(aIndex == ATT_MORPHOLOGY_OPERATOR);
  mOperator = static_cast<MorphologyOperator>(aOperator);
  Invalidate();
}

template<MorphologyOperator Operator, typename m128i_t>
static m128i_t
Morph16(m128i_t a, m128i_t b)
{
  return Operator == MORPHOLOGY_OPERATOR_ERODE ?
    simd::Min16(a, b) : simd::Max16(a, b);
}

template<MorphologyOperator op, typename m128i_t>
static void ApplyMorphologyHorizontal_SIMD(uint8_t* aSourceData, int32_t aSourceStride,
                                      uint8_t* aDestData, int32_t aDestStride,
                                      const IntRect& aDestRect, int32_t aRadius)
{
  int32_t kernelSize = aRadius + 1 + aRadius;
  MOZ_ASSERT(kernelSize >= 3, "don't call this with aRadius <= 0");
  MOZ_ASSERT(kernelSize % 4 == 1 || kernelSize % 4 == 3);
  int32_t completeKernelSizeForFourPixels = kernelSize + 3;
  MOZ_ASSERT(completeKernelSizeForFourPixels % 4 == 0 ||
             completeKernelSizeForFourPixels % 4 == 2);
  for (int32_t y = aDestRect.y; y < aDestRect.YMost(); y++) {
    int32_t kernelStartX = aDestRect.x - aRadius;
    for (int32_t x = aDestRect.x; x < aDestRect.XMost(); x += 4, kernelStartX += 4) {
      int32_t sourceIndex = y * aSourceStride + 4 * kernelStartX;
      m128i_t p1234 = simd::LoadFrom<m128i_t>((m128i_t*)&aSourceData[sourceIndex]);
      m128i_t m12 = simd::UnpackLo8x8To8x16(p1234);
      m128i_t m34 = simd::UnpackHi8x8To8x16(p1234);

      for (int32_t i = 4; i < completeKernelSizeForFourPixels; i += 4) {
        m128i_t p5678 = simd::LoadFrom<m128i_t>((m128i_t*)&aSourceData[sourceIndex + 4 * i]);
        m128i_t p4444 = simd::Splat32<3>(p1234);
        m128i_t p4546 = simd::InterleaveLo32(p4444, p5678);
        m128i_t p2341 = simd::Shuffle32<0,3,2,1>(p1234);
        m128i_t p23 = simd::UnpackLo8x8To8x16(p2341);
        m128i_t p34 = simd::UnpackHi8x8To8x16(p1234);
        m128i_t p45 = simd::UnpackLo8x8To8x16(p4546);
        m128i_t p56 = simd::UnpackLo8x8To8x16(p5678);
        m12 = Morph16<op,m128i_t>(m12, p23);
        m12 = Morph16<op,m128i_t>(m12, p34);
        m34 = Morph16<op,m128i_t>(m34, p45);
        m34 = Morph16<op,m128i_t>(m34, p56);
        if (completeKernelSizeForFourPixels % 4 == 0) {
          m128i_t p6785 = simd::Shuffle32<0,3,2,1>(p5678);
          m128i_t p67 = simd::UnpackLo8x8To8x16(p6785);
          m128i_t p78 = simd::UnpackHi8x8To8x16(p5678);
          m12 = Morph16<op,m128i_t>(m12, p45);
          m12 = Morph16<op,m128i_t>(m12, p56);
          m34 = Morph16<op,m128i_t>(m34, p67);
          m34 = Morph16<op,m128i_t>(m34, p78);
        }
        p1234 = p5678;
      }

      m128i_t m1234 = simd::PackAndSaturate16To8(m12, m34);
      int32_t destIndex = y * aDestStride + 4 * x;
      simd::StoreTo((m128i_t*)&aDestData[destIndex], m1234);
    }
  }
}

template<MorphologyOperator Operator>
static void ApplyMorphologyHorizontal_Scalar(uint8_t* aSourceData, int32_t aSourceStride,
                                             uint8_t* aDestData, int32_t aDestStride,
                                             const IntRect& aDestRect, int32_t aRadius)
{
  for (int32_t y = aDestRect.y; y < aDestRect.YMost(); y++) {
    int32_t startX = aDestRect.x - aRadius;
    int32_t endX = aDestRect.x + aRadius;
    for (int32_t x = aDestRect.x; x < aDestRect.XMost(); x++, startX++, endX++) {
      int32_t sourceIndex = y * aSourceStride + 4 * startX;
      uint8_t u[4];
      for (size_t i = 0; i < 4; i++) {
        u[i] = aSourceData[sourceIndex + i];
      }
      sourceIndex += 4;
      for (int32_t ix = startX + 1; ix <= endX; ix++, sourceIndex += 4) {
        for (size_t i = 0; i < 4; i++) {
          if (Operator == MORPHOLOGY_OPERATOR_ERODE) {
            u[i] = umin(u[i], aSourceData[sourceIndex + i]);
          } else {
            u[i] = umax(u[i], aSourceData[sourceIndex + i]);
          }
        }
      }

      int32_t destIndex = y * aDestStride + 4 * x;
      for (size_t i = 0; i < 4; i++) {
        aDestData[destIndex+i] = u[i];
      }
    }
  }
}

template<MorphologyOperator Operator, typename m128i_t>
static void ApplyMorphologyVertical_SIMD(uint8_t* aSourceData, int32_t aSourceStride,
                                         uint8_t* aDestData, int32_t aDestStride,
                                         const IntRect& aDestRect, int32_t aRadius)
{
  int32_t startY = aDestRect.y - aRadius;
  int32_t endY = aDestRect.y + aRadius;
  for (int32_t y = aDestRect.y; y < aDestRect.YMost(); y++, startY++, endY++) {
    for (int32_t x = aDestRect.x; x < aDestRect.XMost(); x += 4) {
      int32_t sourceIndex = startY * aSourceStride + 4 * x;
      m128i_t u = simd::LoadFrom<m128i_t>((m128i_t*)&aSourceData[sourceIndex]);
      m128i_t u_lo = simd::UnpackLo8x8To8x16(u);
      m128i_t u_hi = simd::UnpackHi8x8To8x16(u);
      sourceIndex += aSourceStride;
      for (int32_t iy = startY + 1; iy <= endY; iy++, sourceIndex += aSourceStride) {
        m128i_t u2 = simd::LoadFrom<m128i_t>((m128i_t*)&aSourceData[sourceIndex]);
        m128i_t u2_lo = simd::UnpackLo8x8To8x16(u2);
        m128i_t u2_hi = simd::UnpackHi8x8To8x16(u2);
        if (Operator == MORPHOLOGY_OPERATOR_ERODE) {
          u_lo = simd::Min16(u_lo, u2_lo);
          u_hi = simd::Min16(u_hi, u2_hi);
        } else {
          u_lo = simd::Max16(u_lo, u2_lo);
          u_hi = simd::Max16(u_hi, u2_hi);
        }
      }

      u = simd::PackAndSaturate16To8(u_lo, u_hi);
      int32_t destIndex = y * aDestStride + 4 * x;
      simd::StoreTo((m128i_t*)&aDestData[destIndex], u);
    }
  }
}

template<MorphologyOperator Operator>
static void ApplyMorphologyVertical_Scalar(uint8_t* aSourceData, int32_t aSourceStride,
                                           uint8_t* aDestData, int32_t aDestStride,
                                           const IntRect& aDestRect, int32_t aRadius)
{
  int32_t startY = aDestRect.y - aRadius;
  int32_t endY = aDestRect.y + aRadius;
  for (int32_t y = aDestRect.y; y < aDestRect.YMost(); y++, startY++, endY++) {
    for (int32_t x = aDestRect.x; x < aDestRect.XMost(); x += 4) {
      int32_t sourceIndex = startY * aSourceStride + 4 * x;
      uint8_t u[4];
      for (size_t i = 0; i < 4; i++) {
        u[i] = aSourceData[sourceIndex + i];
      }
      sourceIndex += aSourceStride;
      for (int32_t iy = startY + 1; iy <= endY; iy++, sourceIndex += aSourceStride) {
        for (size_t i = 0; i < 4; i++) {
          if (Operator == MORPHOLOGY_OPERATOR_ERODE) {
            u[i] = umin(u[i], aSourceData[sourceIndex + i]);
          } else {
            u[i] = umax(u[i], aSourceData[sourceIndex + i]);
          }
        }
      }

      int32_t destIndex = y * aDestStride + 4 * x;
      for (size_t i = 0; i < 4; i++) {
        aDestData[destIndex+i] = u[i];
      }
    }
  }
}

template<MorphologyOperator Operator>
static TemporaryRef<DataSourceSurface>
ApplyMorphology(const IntRect& aSourceRect, DataSourceSurface* aInput,
                const IntRect& aDestRect, int32_t rx, int32_t ry)
{
  static_assert(Operator == MORPHOLOGY_OPERATOR_ERODE ||
                Operator == MORPHOLOGY_OPERATOR_DILATE,
                "unexpected morphology operator");

  IntRect srcRect = aSourceRect - aDestRect.TopLeft();
  IntRect destRect = aDestRect - aDestRect.TopLeft();
  IntRect tmpRect(destRect.x, srcRect.y, destRect.width, srcRect.height);
#ifdef DEBUG
  IntMargin margin = srcRect - destRect;
  MOZ_ASSERT(margin.top >= ry && margin.right >= rx &&
             margin.bottom >= ry && margin.left >= rx, "insufficient margin");
#endif

  RefPtr<DataSourceSurface> tmp;
  if (rx == 0) {
    tmp = aInput;
  } else {
    tmp = Factory::CreateDataSourceSurface(tmpRect.Size(), FORMAT_B8G8R8A8);
    if (!tmp) {
      return nullptr;
    }

    int32_t sourceStride = aInput->Stride();
    uint8_t* sourceData = aInput->GetData();
    sourceData += DataOffset(aInput, destRect.TopLeft() - srcRect.TopLeft());

    int32_t tmpStride = tmp->Stride();
    uint8_t* tmpData = tmp->GetData();
    tmpData += DataOffset(tmp, destRect.TopLeft() - tmpRect.TopLeft());

    if (Factory::HasSSE2()) {
#ifdef COMPILE_WITH_SSE2
      ApplyMorphologyHorizontal_SIMD<Operator,__m128i>(
        sourceData, sourceStride, tmpData, tmpStride, tmpRect, rx);
#endif
    } else {
      ApplyMorphologyHorizontal_Scalar<Operator>(
        sourceData, sourceStride, tmpData, tmpStride, tmpRect, rx);
    }
  }

  RefPtr<DataSourceSurface> dest;
  if (ry == 0) {
    dest = tmp;
  } else {
    dest = Factory::CreateDataSourceSurface(destRect.Size(), FORMAT_B8G8R8A8);
    if (!dest) {
      return nullptr;
    }

    int32_t tmpStride = tmp->Stride();
    uint8_t* tmpData = tmp->GetData();
    tmpData += DataOffset(tmp, destRect.TopLeft() - tmpRect.TopLeft());

    int32_t destStride = dest->Stride();
    uint8_t* destData = dest->GetData();

    if (Factory::HasSSE2()) {
#ifdef COMPILE_WITH_SSE2
      ApplyMorphologyVertical_SIMD<Operator,__m128i>(
        tmpData, tmpStride, destData, destStride, destRect, ry);
 #endif
    } else {
      ApplyMorphologyVertical_Scalar<Operator>(
        tmpData, tmpStride, destData, destStride, destRect, ry);
    }
  }

  return dest;
}

TemporaryRef<DataSourceSurface>
FilterNodeMorphologySoftware::Render(const IntRect& aRect)
{
  IntRect srcRect = aRect;
  srcRect.Inflate(mRadii);

  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_MORPHOLOGY_IN, srcRect, NEED_COLOR_CHANNELS);
  if (!input) {
    return nullptr;
  }

  int32_t rx = mRadii.width;
  int32_t ry = mRadii.height;

  if (rx == 0 && ry == 0) {
    return input;
  }

  if (mOperator == MORPHOLOGY_OPERATOR_ERODE) {
    return ApplyMorphology<MORPHOLOGY_OPERATOR_ERODE>(srcRect, input, aRect, rx, ry);
  } else {
    return ApplyMorphology<MORPHOLOGY_OPERATOR_DILATE>(srcRect, input, aRect, rx, ry);
  }
}

void
FilterNodeMorphologySoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  IntRect srcRect = aRect;
  srcRect.Inflate(mRadii);
  RequestInputRect(IN_MORPHOLOGY_IN, srcRect);
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
  Invalidate();
}

static int32_t
ClampToNonZero(int32_t a)
{
  return a * (a >= 0);
}

template<typename m128i_t>
static m128i_t
ColorMatrixMultiply(m128i_t p, m128i_t rows_bg, m128i_t rows_ra, m128i_t bias)
{
  // int16_t p[8] == { b, g, r, a, b, g, r, a }.
  // int16_t rows_bg[8] == { bB, bG, bR, bA, gB, gG, gR, gA }.
  // int16_t rows_ra[8] == { rB, rG, rR, rA, aB, aG, aR, aA }.
  // int32_t bias[4] == { _B, _G, _R, _A }.

  m128i_t sum = bias;

  // int16_t bg[8] = { b, g, b, g, b, g, b, g };
  m128i_t bg = simd::ShuffleHi16<1,0,1,0>(simd::ShuffleLo16<1,0,1,0>(p));
  // int32_t prodsum_bg[4] = { b * bB + g * gB, b * bG + g * gG, b * bR + g * gR, b * bA + g * gA }
  m128i_t prodsum_bg = simd::MulAdd2x8x16To4x32(bg, rows_bg);
  sum = simd::Add32(sum, prodsum_bg);

  // uint16_t ra[8] = { r, a, r, a, r, a, r, a };
  m128i_t ra = simd::ShuffleHi16<3,2,3,2>(simd::ShuffleLo16<3,2,3,2>(p));
  // int32_t prodsum_ra[4] = { r * rB + a * aB, r * rG + a * aG, r * rR + a * aR, r * rA + a * aA }
  m128i_t prodsum_ra = simd::MulAdd2x8x16To4x32(ra, rows_ra);
  sum = simd::Add32(sum, prodsum_ra);

  // int32_t sum[4] == { b * bB + g * gB + r * rB + a * aB + _B, ... }.
  return sum;
}

template<typename m128i_t>
static TemporaryRef<DataSourceSurface>
ApplyColorMatrixFilter(DataSourceSurface* aInput, const Matrix5x4 &aMatrix)
{
  IntSize size = aInput->GetSize();
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(size, FORMAT_B8G8R8A8);
  if (!target) {
    return nullptr;
  }

  uint8_t* sourceData = aInput->GetData();
  uint8_t* targetData = target->GetData();
  int32_t sourceStride = aInput->Stride();
  int32_t targetStride = target->Stride();

  const int16_t factor = 128;
  const int16_t floatElementMax = INT16_MAX / factor; // 255
  static_assert((floatElementMax * factor) <= INT16_MAX, "badly chosen float-to-int scale");

  const Float *floats = &aMatrix._11;
  union {
    int16_t rows_bgra[2][8];
    m128i_t rows_bgra_v[2];
  };
  union {
    int32_t rowBias[4];
    m128i_t rowsBias_v;
  };

  ptrdiff_t componentOffsets[4] = {
    B8G8R8A8_COMPONENT_BYTEOFFSET_R,
    B8G8R8A8_COMPONENT_BYTEOFFSET_G,
    B8G8R8A8_COMPONENT_BYTEOFFSET_B,
    B8G8R8A8_COMPONENT_BYTEOFFSET_A
  };

  // { bB, bG, bR, bA, gB, gG, gR, gA }.
  // { bB, gB, bG, gG, bR, gR, bA, gA }
  for (size_t rowIndex = 0; rowIndex < 4; rowIndex++) {
    for (size_t colIndex = 0; colIndex < 4; colIndex++) {
      const Float& floatMatrixElement = floats[rowIndex * 4 + colIndex];
      Float clampedFloatMatrixElement = clamped<Float>(floatMatrixElement, -floatElementMax, floatElementMax);
      int16_t scaledIntMatrixElement = int16_t(clampedFloatMatrixElement * factor + 0.5);
      int8_t bg_or_ra = componentOffsets[rowIndex] / 2;
      int8_t g_or_a = componentOffsets[rowIndex] % 2;
      int8_t B_or_G_or_R_or_A = componentOffsets[colIndex];
      rows_bgra[bg_or_ra][B_or_G_or_R_or_A * 2 + g_or_a] = scaledIntMatrixElement;
    }
  }

  Float biasMax = (INT32_MAX - 4 * 255 * INT16_MAX) / (factor * 255);
  for (size_t colIndex = 0; colIndex < 4; colIndex++) {
    size_t rowIndex = 4;
    const Float& floatMatrixElement = floats[rowIndex * 4 + colIndex];
    Float clampedFloatMatrixElement = clamped<Float>(floatMatrixElement, -biasMax, biasMax);
    int32_t scaledIntMatrixElement = int32_t(clampedFloatMatrixElement * factor * 255 + 0.5);
    rowBias[componentOffsets[colIndex]] = scaledIntMatrixElement;
  }

  m128i_t row_bg_v = rows_bgra_v[0];
  m128i_t row_ra_v = rows_bgra_v[1];

  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x += 4) {
      MOZ_ASSERT(sourceStride >= 4 * (x + 4), "need to be able to read 4 pixels at this position");
      MOZ_ASSERT(targetStride >= 4 * (x + 4), "need to be able to write 4 pixels at this position");
      int32_t sourceIndex = y * sourceStride + 4 * x;
      int32_t targetIndex = y * targetStride + 4 * x;

      // We load 4 pixels, unpack them, process them 1 pixel at a time, and
      // finally pack and store the 4 result pixels.

      m128i_t p1234 = simd::LoadFrom((m128i_t*)&sourceData[sourceIndex]);

      m128i_t p1 = simd::UnpackLo8x8To8x16(simd::Splat32<0>(p1234));
      m128i_t p2 = simd::UnpackLo8x8To8x16(simd::Splat32<1>(p1234));
      m128i_t p3 = simd::UnpackLo8x8To8x16(simd::Splat32<2>(p1234));
      m128i_t p4 = simd::UnpackLo8x8To8x16(simd::Splat32<3>(p1234));

      m128i_t result_p1 = ColorMatrixMultiply(p1, row_bg_v, row_ra_v, rowsBias_v);
      m128i_t result_p2 = ColorMatrixMultiply(p2, row_bg_v, row_ra_v, rowsBias_v);
      m128i_t result_p3 = ColorMatrixMultiply(p3, row_bg_v, row_ra_v, rowsBias_v);
      m128i_t result_p4 = ColorMatrixMultiply(p4, row_bg_v, row_ra_v, rowsBias_v);

      static_assert(factor == 1 << 7, "Please adapt the calculation in the lines below for a different factor.");
      m128i_t result_p1234 = simd::PackAndSaturate32To8(simd::ShiftRight32(result_p1, 7),
                                                        simd::ShiftRight32(result_p2, 7),
                                                        simd::ShiftRight32(result_p3, 7),
                                                        simd::ShiftRight32(result_p4, 7));
      simd::StoreTo((m128i_t*)&targetData[targetIndex], result_p1234);
    }
  }

  return target;
}

TemporaryRef<DataSourceSurface>
FilterNodeColorMatrixSoftware::Render(const IntRect& aRect)
{
  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_COLOR_MATRIX_IN, aRect, NEED_COLOR_CHANNELS);
  if (!input) {
    return nullptr;
  }
#ifdef COMPILE_WITH_SSE2
  if (Factory::HasSSE2()) {
    return ApplyColorMatrixFilter<__m128i>(input, mMatrix);
  }
#endif
  return ApplyColorMatrixFilter<simd::ScalarM128i>(input, mMatrix);
}

void
FilterNodeColorMatrixSoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  RequestInputRect(IN_COLOR_MATRIX_IN, aRect);
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
  Invalidate();
}

static uint32_t
ColorToBGRA(const Color& aColor)
{
  union {
    uint32_t color;
    uint8_t components[4];
  };
  components[B8G8R8A8_COMPONENT_BYTEOFFSET_R] = NS_lround(aColor.r * aColor.a * 255.0f);
  components[B8G8R8A8_COMPONENT_BYTEOFFSET_G] = NS_lround(aColor.g * aColor.a * 255.0f);
  components[B8G8R8A8_COMPONENT_BYTEOFFSET_B] = NS_lround(aColor.b * aColor.a * 255.0f);
  components[B8G8R8A8_COMPONENT_BYTEOFFSET_A] = NS_lround(aColor.a * 255.0f);
  return color;
}

static SurfaceFormat
FormatForColor(Color aColor)
{
  if (aColor.r == 0 && aColor.g == 0 && aColor.b == 0) {
    return FORMAT_A8;
  }
  return FORMAT_B8G8R8A8;
}

TemporaryRef<DataSourceSurface>
FilterNodeFloodSoftware::Render(const IntRect& aRect)
{
  SurfaceFormat format = FormatForColor(mColor);
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(aRect.Size(), format);
  if (!target) {
    return nullptr;
  }

  uint8_t* targetData = target->GetData();
  uint32_t stride = target->Stride();

  if (format == FORMAT_B8G8R8A8) {
    uint32_t color = ColorToBGRA(mColor);
    for (int32_t y = 0; y < aRect.height; y++) {
      for (int32_t x = 0; x < aRect.width; x++) {
        *((uint32_t*)targetData + x) = color;
      }
      targetData += stride;
    }
  } else if (format == FORMAT_A8) {
    uint8_t alpha = NS_lround(mColor.a * 255.0f);
    for (int32_t y = 0; y < aRect.height; y++) {
      for (int32_t x = 0; x < aRect.width; x++) {
        targetData[x] = alpha;
      }
      targetData += stride;
    }
  } else {
    MOZ_CRASH();
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
                                     const IntRect &aSourceRect)
{
  MOZ_ASSERT(aIndex == ATT_TILE_SOURCE_RECT);
  mSourceRect = IntRect(int32_t(aSourceRect.x), int32_t(aSourceRect.y),
                        int32_t(aSourceRect.width), int32_t(aSourceRect.height));
  Invalidate();
}

TemporaryRef<DataSourceSurface>
FilterNodeTileSoftware::Render(const IntRect& aRect)
{
  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_TILE_IN, mSourceRect);
  if (!input) {
    return nullptr;
  }

  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(aRect.Size(), input->GetFormat());
  if (!target) {
    return nullptr;
  }

  TileSurface(input, target, mSourceRect.TopLeft() - aRect.TopLeft());

  return target;
}

void
FilterNodeTileSoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  RequestInputRect(IN_TILE_IN, mSourceRect);
}

IntRect
FilterNodeTileSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  return aRect;
}

FilterNodeComponentTransferSoftware::FilterNodeComponentTransferSoftware()
 : mDisableR(true)
 , mDisableG(true)
 , mDisableB(true)
 , mDisableA(true)
{}

void
FilterNodeComponentTransferSoftware::SetAttribute(uint32_t aIndex,
                                                  bool aDisable)
{
  switch (aIndex) {
    case ATT_TRANSFER_DISABLE_R:
      mDisableR = aDisable;
      break;
    case ATT_TRANSFER_DISABLE_G:
      mDisableG = aDisable;
      break;
    case ATT_TRANSFER_DISABLE_B:
      mDisableB = aDisable;
      break;
    case ATT_TRANSFER_DISABLE_A:
      mDisableA = aDisable;
      break;
    default:
      MOZ_CRASH();
  }
  Invalidate();
}

static const uint8_t kIdentityLookupTable[256] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
  0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
  0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
  0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
  0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
  0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
  0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
  0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
  0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
  0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
  0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
  0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
  0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

void
FilterNodeComponentTransferSoftware::GenerateLookupTable(ptrdiff_t aComponent,
                                                         uint8_t aTables[4][256],
                                                         bool aDisabled)
{
  if (aDisabled) {
    memcpy(aTables[aComponent], kIdentityLookupTable, 256);
  } else {
    FillLookupTable(aComponent, aTables[aComponent]);
  }
}

template<uint32_t BytesPerPixel>
static void TransferComponents(DataSourceSurface* aInput,
                               DataSourceSurface* aTarget,
                               const uint8_t aLookupTables[BytesPerPixel][256])
{
  MOZ_ASSERT(aInput->GetFormat() == aTarget->GetFormat(), "different formats");
  IntSize size = aInput->GetSize();

  uint8_t* sourceData = aInput->GetData();
  uint8_t* targetData = aTarget->GetData();
  uint32_t sourceStride = aInput->Stride();
  uint32_t targetStride = aTarget->Stride();

  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x++) {
      uint32_t sourceIndex = y * sourceStride + x * BytesPerPixel;
      uint32_t targetIndex = y * targetStride + x * BytesPerPixel;
      for (uint32_t i = 0; i < BytesPerPixel; i++) {
        targetData[targetIndex + i] = aLookupTables[i][sourceData[sourceIndex + i]];
      }
    }
  }
}

TemporaryRef<DataSourceSurface>
FilterNodeComponentTransferSoftware::Render(const IntRect& aRect)
{
  if (mDisableR && mDisableG && mDisableB && mDisableA) {
    return GetInputDataSourceSurface(IN_TRANSFER_IN, aRect);
  }

  uint8_t lookupTables[4][256];
  GenerateLookupTable(B8G8R8A8_COMPONENT_BYTEOFFSET_R, lookupTables, mDisableR);
  GenerateLookupTable(B8G8R8A8_COMPONENT_BYTEOFFSET_G, lookupTables, mDisableG);
  GenerateLookupTable(B8G8R8A8_COMPONENT_BYTEOFFSET_B, lookupTables, mDisableB);
  GenerateLookupTable(B8G8R8A8_COMPONENT_BYTEOFFSET_A, lookupTables, mDisableA);

  bool needColorChannels =
    lookupTables[B8G8R8A8_COMPONENT_BYTEOFFSET_R][0] != 0 ||
    lookupTables[B8G8R8A8_COMPONENT_BYTEOFFSET_G][0] != 0 ||
    lookupTables[B8G8R8A8_COMPONENT_BYTEOFFSET_B][0] != 0;

  FormatHint pref = needColorChannels ? NEED_COLOR_CHANNELS : CAN_HANDLE_A8;

  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_TRANSFER_IN, aRect, pref);
  if (!input) {
    return nullptr;
  }

  SurfaceFormat format = input->GetFormat();
  if (format == FORMAT_A8 && mDisableA) {
    return input;
  }

  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(aRect.Size(), format);
  if (!target) {
    return nullptr;
  }

  if (format == FORMAT_A8) {
    TransferComponents<1>(input, target, &lookupTables[B8G8R8A8_COMPONENT_BYTEOFFSET_A]);
  } else {
    TransferComponents<4>(input, target, lookupTables);
  }

  return target;
}

void
FilterNodeComponentTransferSoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  RequestInputRect(IN_TRANSFER_IN, aRect);
}

IntRect
FilterNodeComponentTransferSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  return GetInputRectInRect(IN_TRANSFER_IN, aRect);
}

int32_t
FilterNodeComponentTransferSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_TRANSFER_IN: return 0;
    default: return -1;
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
  Invalidate();
}

void
FilterNodeTableTransferSoftware::FillLookupTable(ptrdiff_t aComponent,
                                                 uint8_t aTable[256])
{
  switch (aComponent) {
    case B8G8R8A8_COMPONENT_BYTEOFFSET_R:
      FillLookupTableImpl(mTableR, aTable);
      break;
    case B8G8R8A8_COMPONENT_BYTEOFFSET_G:
      FillLookupTableImpl(mTableG, aTable);
      break;
    case B8G8R8A8_COMPONENT_BYTEOFFSET_B:
      FillLookupTableImpl(mTableB, aTable);
      break;
    case B8G8R8A8_COMPONENT_BYTEOFFSET_A:
      FillLookupTableImpl(mTableA, aTable);
      break;
    default:
      MOZ_ASSERT(false, "unknown component");
      break;
  }
}

void
FilterNodeTableTransferSoftware::FillLookupTableImpl(std::vector<Float>& aTableValues,
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
  Invalidate();
}

void
FilterNodeDiscreteTransferSoftware::FillLookupTable(ptrdiff_t aComponent,
                                                    uint8_t aTable[256])
{
  switch (aComponent) {
    case B8G8R8A8_COMPONENT_BYTEOFFSET_R:
      FillLookupTableImpl(mTableR, aTable);
      break;
    case B8G8R8A8_COMPONENT_BYTEOFFSET_G:
      FillLookupTableImpl(mTableG, aTable);
      break;
    case B8G8R8A8_COMPONENT_BYTEOFFSET_B:
      FillLookupTableImpl(mTableB, aTable);
      break;
    case B8G8R8A8_COMPONENT_BYTEOFFSET_A:
      FillLookupTableImpl(mTableA, aTable);
      break;
    default:
      MOZ_ASSERT(false, "unknown component");
      break;
  }
}

void
FilterNodeDiscreteTransferSoftware::FillLookupTableImpl(std::vector<Float>& aTableValues,
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
    int32_t val = NS_lround(255 * v);
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
  Invalidate();
}

void
FilterNodeLinearTransferSoftware::FillLookupTable(ptrdiff_t aComponent,
                                                  uint8_t aTable[256])
{
  switch (aComponent) {
    case B8G8R8A8_COMPONENT_BYTEOFFSET_R:
      FillLookupTableImpl(mSlopeR, mInterceptR, aTable);
      break;
    case B8G8R8A8_COMPONENT_BYTEOFFSET_G:
      FillLookupTableImpl(mSlopeG, mInterceptG, aTable);
      break;
    case B8G8R8A8_COMPONENT_BYTEOFFSET_B:
      FillLookupTableImpl(mSlopeB, mInterceptB, aTable);
      break;
    case B8G8R8A8_COMPONENT_BYTEOFFSET_A:
      FillLookupTableImpl(mSlopeA, mInterceptA, aTable);
      break;
    default:
      MOZ_ASSERT(false, "unknown component");
      break;
  }
}

void
FilterNodeLinearTransferSoftware::FillLookupTableImpl(Float aSlope,
                                                      Float aIntercept,
                                                      uint8_t aTable[256])
{
  for (size_t i = 0; i < 256; i++) {
    int32_t val = NS_lround(aSlope * i + 255 * aIntercept);
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
  Invalidate();
}

void
FilterNodeGammaTransferSoftware::FillLookupTable(ptrdiff_t aComponent,
                                                 uint8_t aTable[256])
{
  switch (aComponent) {
    case B8G8R8A8_COMPONENT_BYTEOFFSET_R:
      FillLookupTableImpl(mAmplitudeR, mExponentR, mOffsetR, aTable);
      break;
    case B8G8R8A8_COMPONENT_BYTEOFFSET_G:
      FillLookupTableImpl(mAmplitudeG, mExponentG, mOffsetG, aTable);
      break;
    case B8G8R8A8_COMPONENT_BYTEOFFSET_B:
      FillLookupTableImpl(mAmplitudeB, mExponentB, mOffsetB, aTable);
      break;
    case B8G8R8A8_COMPONENT_BYTEOFFSET_A:
      FillLookupTableImpl(mAmplitudeA, mExponentA, mOffsetA, aTable);
      break;
    default:
      MOZ_ASSERT(false, "unknown component");
      break;
  }
}

void
FilterNodeGammaTransferSoftware::FillLookupTableImpl(Float aAmplitude,
                                                     Float aExponent,
                                                     Float aOffset,
                                                     uint8_t aTable[256])
{
  for (size_t i = 0; i < 256; i++) {
    int32_t val = NS_lround(255 * (aAmplitude * pow(i / 255.0f, aExponent) + aOffset));
    val = std::min(255, val);
    val = std::max(0, val);
    aTable[i] = val;
  }
}

FilterNodeConvolveMatrixSoftware::FilterNodeConvolveMatrixSoftware()
 : mDivisor(0)
 , mBias(0)
 , mEdgeMode(EDGE_MODE_DUPLICATE)
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
  Invalidate();
}

void
FilterNodeConvolveMatrixSoftware::SetAttribute(uint32_t aIndex,
                                               const Float *aMatrix,
                                               uint32_t aSize)
{
  MOZ_ASSERT(aIndex == ATT_CONVOLVE_MATRIX_KERNEL_MATRIX);
  mKernelMatrix = std::vector<Float>(aMatrix, aMatrix + aSize);
  Invalidate();
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
    default:
      MOZ_CRASH();
  }
  Invalidate();
}

void
FilterNodeConvolveMatrixSoftware::SetAttribute(uint32_t aIndex, const Size &aKernelUnitLength)
{
  switch (aIndex) {
    case ATT_CONVOLVE_MATRIX_KERNEL_UNIT_LENGTH:
      mKernelUnitLength = aKernelUnitLength;
      break;
    default:
      MOZ_CRASH();
  }
  Invalidate();
}

void
FilterNodeConvolveMatrixSoftware::SetAttribute(uint32_t aIndex,
                                               const IntPoint &aTarget)
{
  MOZ_ASSERT(aIndex == ATT_CONVOLVE_MATRIX_TARGET);
  mTarget = aTarget;
  Invalidate();
}

void
FilterNodeConvolveMatrixSoftware::SetAttribute(uint32_t aIndex,
                                               uint32_t aEdgeMode)
{
  MOZ_ASSERT(aIndex == ATT_CONVOLVE_MATRIX_EDGE_MODE);
  mEdgeMode = static_cast<ConvolveMatrixEdgeMode>(aEdgeMode);
  Invalidate();
}

void
FilterNodeConvolveMatrixSoftware::SetAttribute(uint32_t aIndex,
                                               bool aPreserveAlpha)
{
  MOZ_ASSERT(aIndex == ATT_CONVOLVE_MATRIX_PRESERVE_ALPHA);
  mPreserveAlpha = aPreserveAlpha;
  Invalidate();
}

static uint8_t
ColorComponentAtPoint(const uint8_t *aData, int32_t aStride, int32_t x, int32_t y, ptrdiff_t c)
{
  return aData[y * aStride + 4 * x + c];
}

// Accepts fractional x & y and does bilinear interpolation.
// Only call this if the pixel (floor(x)+1, floor(y)+1) is accessible.
static uint8_t
ColorComponentAtPoint(const uint8_t *aData, int32_t aStride, Float x, Float y, ptrdiff_t c)
{
  const uint32_t f = 256;
  const int32_t lx = floor(x);
  const int32_t ly = floor(y);
  const int32_t tux = uint32_t((x - lx) * f);
  const int32_t tlx = f - tux;
  const int32_t tuy = uint32_t((y - ly) * f);
  const int32_t tly = f - tuy;
  const uint8_t &cll = ColorComponentAtPoint(aData, aStride, lx,     ly,     c);
  const uint8_t &cul = ColorComponentAtPoint(aData, aStride, lx + 1, ly,     c);
  const uint8_t &clu = ColorComponentAtPoint(aData, aStride, lx,     ly + 1, c);
  const uint8_t &cuu = ColorComponentAtPoint(aData, aStride, lx + 1, ly + 1, c);
  return ((cll * tlx + cul * tux) * tly +
          (clu * tlx + cuu * tux) * tuy + f * f / 2) / (f * f);
}

template<typename CoordType>
static void
ConvolvePixel(const uint8_t *aSourceData,
              uint8_t *aTargetData,
              int32_t aWidth, int32_t aHeight,
              int32_t aSourceStride, int32_t aTargetStride,
              int32_t aX, int32_t aY,
              const int32_t *aKernel,
              int32_t aBias, int32_t shiftL, int32_t shiftR,
              bool aPreserveAlpha,
              int32_t aOrderX, int32_t aOrderY,
              int32_t aTargetX, int32_t aTargetY,
              CoordType aKernelUnitLengthX,
              CoordType aKernelUnitLengthY)
{
  int32_t sum[4] = {0, 0, 0, 0};
  int32_t offsets[4] = { B8G8R8A8_COMPONENT_BYTEOFFSET_R,
                         B8G8R8A8_COMPONENT_BYTEOFFSET_G,
                         B8G8R8A8_COMPONENT_BYTEOFFSET_B,
                         B8G8R8A8_COMPONENT_BYTEOFFSET_A };
  int32_t channels = aPreserveAlpha ? 3 : 4;
  int32_t roundingAddition = shiftL == 0 ? 0 : 1 << (shiftL - 1);

  for (int32_t y = 0; y < aOrderY; y++) {
    CoordType sampleY = aY + (y - aTargetY) * aKernelUnitLengthY;
    for (int32_t x = 0; x < aOrderX; x++) {
      CoordType sampleX = aX + (x - aTargetX) * aKernelUnitLengthX;
      for (int32_t i = 0; i < channels; i++) {
        sum[i] += aKernel[aOrderX * y + x] *
          ColorComponentAtPoint(aSourceData, aSourceStride,
                                sampleX, sampleY, offsets[i]);
      }
    }
  }
  for (int32_t i = 0; i < channels; i++) {
    int32_t clamped = umin(ClampToNonZero(sum[i] + aBias), 255 << shiftL >> shiftR);
    aTargetData[aY * aTargetStride + 4 * aX + offsets[i]] =
      (clamped + roundingAddition) << shiftR >> shiftL;
  }
  if (aPreserveAlpha) {
    aTargetData[aY * aTargetStride + 4 * aX + B8G8R8A8_COMPONENT_BYTEOFFSET_A] =
      aSourceData[aY * aSourceStride + 4 * aX + B8G8R8A8_COMPONENT_BYTEOFFSET_A];
  }
}

TemporaryRef<DataSourceSurface>
FilterNodeConvolveMatrixSoftware::Render(const IntRect& aRect)
{
  if (mKernelUnitLength.width == floor(mKernelUnitLength.width) &&
      mKernelUnitLength.height == floor(mKernelUnitLength.height)) {
    return DoRender(aRect, (int32_t)mKernelUnitLength.width, (int32_t)mKernelUnitLength.height);
  }
  return DoRender(aRect, mKernelUnitLength.width, mKernelUnitLength.height);
}

static std::vector<Float>
ReversedVector(const std::vector<Float> &aVector)
{
  size_t length = aVector.size();
  std::vector<Float> result(length, 0);
  for (size_t i = 0; i < length; i++) {
    result[length - 1 - i] = aVector[i];
  }
  return result;
}

static std::vector<Float>
ScaledVector(const std::vector<Float> &aVector, Float aDivisor)
{
  size_t length = aVector.size();
  std::vector<Float> result(length, 0);
  for (size_t i = 0; i < length; i++) {
    result[i] = aVector[i] / aDivisor;
  }
  return result;
}

static Float
MaxVectorSum(const std::vector<Float> &aVector)
{
  Float sum = 0;
  size_t length = aVector.size();
  for (size_t i = 0; i < length; i++) {
    if (aVector[i] > 0) {
      sum += aVector[i];
    }
  }
  return sum;
}

// Returns shiftL and shiftR in such a way that
// a << shiftL >> shiftR is roughly a * aFloat.
static void
TranslateFloatToShifts(Float aFloat, int32_t &aShiftL, int32_t &aShiftR)
{
  aShiftL = 0;
  aShiftR = 0;
  if (aFloat <= 0) {
    MOZ_CRASH();
  }
  if (aFloat < 1) {
    while (1 << (aShiftR + 1) < 1 / aFloat) {
      aShiftR++;
    }
  } else {
    while (1 << (aShiftL + 1) < aFloat) {
      aShiftL++;
    }
  }
}

template<typename CoordType>
TemporaryRef<DataSourceSurface>
FilterNodeConvolveMatrixSoftware::DoRender(const IntRect& aRect,
                                           CoordType aKernelUnitLengthX,
                                           CoordType aKernelUnitLengthY)
{
  if (mKernelSize.width <= 0 || mKernelSize.height <= 0 ||
      mKernelMatrix.size() != uint32_t(mKernelSize.width * mKernelSize.height) ||
      !IntRect(IntPoint(0, 0), mKernelSize).Contains(mTarget) ||
      mDivisor == 0) {
    return Factory::CreateDataSourceSurface(aRect.Size(), FORMAT_B8G8R8A8);
  }

  IntRect srcRect = InflatedSourceRect(aRect);
  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_CONVOLVE_MATRIX_IN, srcRect, NEED_COLOR_CHANNELS, mEdgeMode);
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(aRect.Size(), FORMAT_B8G8R8A8);
  if (!input || !target) {
    return nullptr;
  }
  ClearDataSourceSurface(target);

  uint8_t* sourceData = input->GetData();
  int32_t sourceStride = input->Stride();
  uint8_t* targetData = target->GetData();
  int32_t targetStride = target->Stride();

  IntPoint offset = aRect.TopLeft() - srcRect.TopLeft();
  sourceData += DataOffset(input, offset);

  // Why exactly are we reversing the kernel?
  std::vector<Float> kernel = ReversedVector(mKernelMatrix);
  kernel = ScaledVector(kernel, mDivisor);
  Float maxResultAbs = std::max(MaxVectorSum(kernel) + mBias,
                                MaxVectorSum(ScaledVector(kernel, -1)) - mBias);
  maxResultAbs = std::max(maxResultAbs, 1.0f);

  Float idealFactor = INT32_MAX / 2.0f / maxResultAbs / 255.0f;
  MOZ_ASSERT(255 * (maxResultAbs * idealFactor) <= INT32_MAX / 2.0f, "badly chosen float-to-int scale");
  int32_t shiftL, shiftR;
  TranslateFloatToShifts(idealFactor, shiftL, shiftR);
  Float factorFromShifts = Float(1 << shiftL) / Float(1 << shiftR);
  MOZ_ASSERT(255 * (maxResultAbs * factorFromShifts) <= INT32_MAX / 2.0f, "badly chosen float-to-int scale");

  std::vector<int32_t> intKernel(kernel.size(), 0);
  for (size_t i = 0; i < kernel.size(); i++) {
    intKernel[i] = NS_lround(kernel[i] * factorFromShifts);
  }
  int32_t bias = NS_lround(mBias * 255 * factorFromShifts);

  for (int32_t y = 0; y < aRect.height; y++) {
    for (int32_t x = 0; x < aRect.width; x++) {
      ConvolvePixel(sourceData, targetData,
                    aRect.width, aRect.height, sourceStride, targetStride,
                    x, y, intKernel.data(), bias, shiftL, shiftR, mPreserveAlpha,
                    mKernelSize.width, mKernelSize.height, mTarget.x, mTarget.y,
                    aKernelUnitLengthX, aKernelUnitLengthY);
    }
  }

  return target;
}

void
FilterNodeConvolveMatrixSoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  RequestInputRect(IN_CONVOLVE_MATRIX_IN, InflatedSourceRect(aRect));
}

IntRect
FilterNodeConvolveMatrixSoftware::InflatedSourceRect(const IntRect &aDestRect)
{
  IntMargin margin;
  margin.left = ceil(mTarget.x * mKernelUnitLength.width);
  margin.top = ceil(mTarget.y * mKernelUnitLength.height);
  margin.right = ceil((mKernelSize.width - mTarget.x - 1) * mKernelUnitLength.width);
  margin.bottom = ceil((mKernelSize.height - mTarget.y - 1) * mKernelUnitLength.height);

  IntRect srcRect = aDestRect;
  srcRect.Inflate(margin);
  return srcRect;
}

IntRect
FilterNodeConvolveMatrixSoftware::InflatedDestRect(const IntRect &aSourceRect)
{
  IntMargin margin;
  margin.left = ceil((mKernelSize.width - mTarget.x - 1) * mKernelUnitLength.width);
  margin.top = ceil((mKernelSize.height - mTarget.y - 1) * mKernelUnitLength.height);
  margin.right = ceil(mTarget.x * mKernelUnitLength.width);
  margin.bottom = ceil(mTarget.y * mKernelUnitLength.height);

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
  Invalidate();
}

TemporaryRef<DataSourceSurface>
FilterNodeOffsetSoftware::Render(const IntRect& aRect)
{
  return GetInputDataSourceSurface(IN_OFFSET_IN, aRect - mOffset);
}

void
FilterNodeOffsetSoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  RequestInputRect(IN_OFFSET_IN, aRect - mOffset);
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
  Invalidate();
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
  Invalidate();
}

TemporaryRef<DataSourceSurface>
FilterNodeDisplacementMapSoftware::Render(const IntRect& aRect)
{
  IntRect srcRect = InflatedSourceOrDestRect(aRect);
  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_DISPLACEMENT_MAP_IN, srcRect, NEED_COLOR_CHANNELS);
  RefPtr<DataSourceSurface> map =
    GetInputDataSourceSurface(IN_DISPLACEMENT_MAP_IN2, aRect, NEED_COLOR_CHANNELS);
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(aRect.Size(), FORMAT_B8G8R8A8);
  if (!input || !map || !target) {
    return nullptr;
  }

  uint8_t* sourceData = input->GetData();
  int32_t sourceStride = input->Stride();
  uint8_t* mapData = map->GetData();
  int32_t mapStride = map->Stride();
  uint8_t* targetData = target->GetData();
  int32_t targetStride = target->Stride();

  IntPoint offset = aRect.TopLeft() - srcRect.TopLeft();
  sourceData += DataOffset(input, offset);

  static const ptrdiff_t channelMap[4] = {
                             B8G8R8A8_COMPONENT_BYTEOFFSET_R,
                             B8G8R8A8_COMPONENT_BYTEOFFSET_G,
                             B8G8R8A8_COMPONENT_BYTEOFFSET_B,
                             B8G8R8A8_COMPONENT_BYTEOFFSET_A };
  uint16_t xChannel = channelMap[mChannelX];
  uint16_t yChannel = channelMap[mChannelY];

  double scaleOver255 = mScale / 255.0;
  double scaleAdjustment = -0.5 * mScale;

  for (int32_t y = 0; y < aRect.height; y++) {
    for (int32_t x = 0; x < aRect.width; x++) {
      uint32_t mapIndex = y * mapStride + 4 * x;
      uint32_t targIndex = y * targetStride + 4 * x;
      Float sourceX = x +
        scaleOver255 * mapData[mapIndex + xChannel] + scaleAdjustment;
      Float sourceY = y +
        scaleOver255 * mapData[mapIndex + yChannel] + scaleAdjustment;
      for (int32_t i = 0; i < 4; i++) {
        targetData[targIndex + i] =
          ColorComponentAtPoint(sourceData, sourceStride, sourceX, sourceY, i);
      }
    }
  }

  return target;
}

void
FilterNodeDisplacementMapSoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  RequestInputRect(IN_DISPLACEMENT_MAP_IN, InflatedSourceOrDestRect(aRect));
  RequestInputRect(IN_DISPLACEMENT_MAP_IN2, aRect);
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
  Invalidate();
}

void
FilterNodeTurbulenceSoftware::SetAttribute(uint32_t aIndex, bool aStitchable)
{
  MOZ_ASSERT(aIndex == ATT_TURBULENCE_STITCHABLE);
  mStitchable = aStitchable;
  Invalidate();
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
  Invalidate();
}

template<TurbulenceType aType, bool aStitchable>
TemporaryRef<DataSourceSurface>
FilterNodeTurbulenceSoftware::DoRender(const IntRect& aRect)
{
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(aRect.Size(), FORMAT_B8G8R8A8);
  if (!target) {
    return nullptr;
  }

  uint8_t* targetData = target->GetData();
  uint32_t stride = target->Stride();

  SVGTurbulenceRenderer<aType,aStitchable,double> renderer(mBaseFrequency, mSeed, mNumOctaves, aRect);

  for (int32_t y = 0; y < aRect.height; y++) {
    for (int32_t x = 0; x < aRect.width; x++) {
      int32_t targIndex = y * stride + x * 4;
      *(uint32_t*)(targetData + targIndex) =
        renderer.ColorAtPoint(aRect.TopLeft() + IntPoint(x, y));
    }
  }

  return target;
}

TemporaryRef<DataSourceSurface>
FilterNodeTurbulenceSoftware::Render(const IntRect& aRect)
{
  switch (mType) {
    case TURBULENCE_TYPE_TURBULENCE:
      if (mStitchable) {
        return DoRender<TURBULENCE_TYPE_TURBULENCE, true>(aRect);
      }
      return DoRender<TURBULENCE_TYPE_TURBULENCE, false>(aRect);
    case TURBULENCE_TYPE_FRACTAL_NOISE:
      if (mStitchable) {
        return DoRender<TURBULENCE_TYPE_FRACTAL_NOISE, true>(aRect);
      }
      return DoRender<TURBULENCE_TYPE_FRACTAL_NOISE, false>(aRect);
  }
  return nullptr;
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

  Invalidate();
}

TemporaryRef<DataSourceSurface>
FilterNodeArithmeticCombineSoftware::Render(const IntRect& aRect)
{
  RefPtr<DataSourceSurface> input1 =
    GetInputDataSourceSurface(IN_ARITHMETIC_COMBINE_IN, aRect, NEED_COLOR_CHANNELS);
  RefPtr<DataSourceSurface> input2 =
    GetInputDataSourceSurface(IN_ARITHMETIC_COMBINE_IN2, aRect, NEED_COLOR_CHANNELS);
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(aRect.Size(), FORMAT_B8G8R8A8);
  if (!input1 || !input2 || !target) {
    return nullptr;
  }

  uint8_t* source1Data = input1->GetData();
  uint8_t* source2Data = input2->GetData();
  uint8_t* targetData = target->GetData();
  uint32_t source1Stride = input1->Stride();
  uint32_t source2Stride = input2->Stride();
  uint32_t targetStride = target->Stride();

  int32_t k1 = int32_t(clamped(mK1, -255.0f, 255.0f)             * 32);
  int32_t k2 = int32_t(clamped(mK2, -255.0f, 255.0f) * 255       * 32);
  int32_t k3 = int32_t(clamped(mK3, -255.0f, 255.0f) * 255       * 32);
  int32_t k4 = int32_t(clamped(mK4, -255.0f, 255.0f) * 255 * 255 * 32);

  for (int32_t y = 0; y < aRect.height; y++) {
    for (int32_t x = 0; x < aRect.width; x++) {
      uint32_t source1Index = y * source1Stride + 4 * x;
      uint32_t source2Index = y * source2Stride + 4 * x;
      uint32_t targetIndex = y * targetStride + 4 * x;
      for (int32_t i = 0; i < 4; i++) {
        uint8_t i1 = source1Data[source1Index + i];
        uint8_t i2 = source2Data[source2Index + i];
        int32_t result = umin(ClampToNonZero(k1*i1*i2 + k2*i1 + k3*i2 + k4), 255 * 255 * 32);
        targetData[targetIndex + i] =
                   static_cast<uint8_t>(FastDivideBy255<uint32_t>(result / 32));
      }
    }
  }

  return target;
}

void
FilterNodeArithmeticCombineSoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  RequestInputRect(IN_ARITHMETIC_COMBINE_IN, aRect);
  RequestInputRect(IN_ARITHMETIC_COMBINE_IN2, aRect);
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
  Invalidate();
}

template<typename m128i_t, uint32_t aCompositeOperator>
static m128i_t
CompositeTwoPixels(m128i_t source, m128i_t sourceAlpha, m128i_t dest, m128i_t destAlpha)
{
  m128i_t x255 = simd::From16<m128i_t>(255);

  switch (aCompositeOperator) {

    case COMPOSITE_OPERATOR_OVER:
    {
      // val = dest * (255 - sourceAlpha) + source * 255;
      m128i_t twoFiftyFiveMinusSourceAlpha = simd::Sub16(x255, sourceAlpha);

      m128i_t destSourceInterleaved1 = simd::InterleaveLo16(dest, source);
      m128i_t rightFactor1 = simd::InterleaveLo16(twoFiftyFiveMinusSourceAlpha, x255);
      m128i_t result1 = simd::MulAdd2x8x16To4x32(destSourceInterleaved1, rightFactor1);

      m128i_t destSourceInterleaved2 = simd::InterleaveHi16(dest, source);
      m128i_t rightFactor2 = simd::InterleaveHi16(twoFiftyFiveMinusSourceAlpha, x255);
      m128i_t result2 = simd::MulAdd2x8x16To4x32(destSourceInterleaved2, rightFactor2);

      return simd::PackAndSaturate32To16(simd::FastDivideBy255(result1),
                                         simd::FastDivideBy255(result2));
    }

    case COMPOSITE_OPERATOR_IN:
    {
      // val = source * destAlpha;
      return simd::FastDivideBy255_16(simd::Mul16(source, destAlpha));
    }

    case COMPOSITE_OPERATOR_OUT:
    {
      // val = source * (255 - destAlpha);
      m128i_t prod = simd::Mul16(source, simd::Sub16(x255, destAlpha));
      return simd::FastDivideBy255_16(prod);
    }

    case COMPOSITE_OPERATOR_ATOP:
    {
      // val = dest * (255 - sourceAlpha) + source * destAlpha;
      m128i_t twoFiftyFiveMinusSourceAlpha = simd::Sub16(x255, sourceAlpha);

      m128i_t destSourceInterleaved1 = simd::InterleaveLo16(dest, source);
      m128i_t rightFactor1 = simd::InterleaveLo16(twoFiftyFiveMinusSourceAlpha, destAlpha);
      m128i_t result1 = simd::MulAdd2x8x16To4x32(destSourceInterleaved1, rightFactor1);

      m128i_t destSourceInterleaved2 = simd::InterleaveHi16(dest, source);
      m128i_t rightFactor2 = simd::InterleaveHi16(twoFiftyFiveMinusSourceAlpha, destAlpha);
      m128i_t result2 = simd::MulAdd2x8x16To4x32(destSourceInterleaved2, rightFactor2);

      return simd::PackAndSaturate32To16(simd::FastDivideBy255(result1),
                                         simd::FastDivideBy255(result2));
    }

    case COMPOSITE_OPERATOR_XOR:
    {
      // val = dest * (255 - sourceAlpha) + source * (255 - destAlpha);
      m128i_t twoFiftyFiveMinusSourceAlpha = simd::Sub16(x255, sourceAlpha);
      m128i_t twoFiftyFiveMinusDestAlpha = simd::Sub16(x255, destAlpha);

      m128i_t destSourceInterleaved1 = simd::InterleaveLo16(dest, source);
      m128i_t rightFactor1 = simd::InterleaveLo16(twoFiftyFiveMinusSourceAlpha,
                                                  twoFiftyFiveMinusDestAlpha);
      m128i_t result1 = simd::MulAdd2x8x16To4x32(destSourceInterleaved1, rightFactor1);

      m128i_t destSourceInterleaved2 = simd::InterleaveHi16(dest, source);
      m128i_t rightFactor2 = simd::InterleaveHi16(twoFiftyFiveMinusSourceAlpha,
                                                  twoFiftyFiveMinusDestAlpha);
      m128i_t result2 = simd::MulAdd2x8x16To4x32(destSourceInterleaved2, rightFactor2);

      return simd::PackAndSaturate32To16(simd::FastDivideBy255(result1),
                                         simd::FastDivideBy255(result2));
    }

  }
}

template<typename m128i_t, uint32_t op>
static void
ApplyComposition(DataSourceSurface* aSource, DataSourceSurface* aDest)
{
  IntSize size = aDest->GetSize();

  uint8_t* sourceData = aSource->GetData();
  uint8_t* destData = aDest->GetData();
  uint32_t sourceStride = aSource->Stride();
  uint32_t destStride = aDest->Stride();

  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x += 4) {
      uint32_t sourceIndex = y * sourceStride + 4 * x;
      uint32_t destIndex = y * destStride + 4 * x;

      m128i_t s1234 = simd::LoadFrom((m128i_t*)&sourceData[sourceIndex]);
      m128i_t d1234 = simd::LoadFrom((m128i_t*)&destData[destIndex]);

      m128i_t s12 = simd::UnpackLo8x8To8x16(s1234);
      m128i_t d12 = simd::UnpackLo8x8To8x16(d1234);
      m128i_t sa12 = simd::SplatHi16<3>(simd::SplatLo16<3>(s12));
      m128i_t da12 = simd::SplatHi16<3>(simd::SplatLo16<3>(d12));
      m128i_t result12 = CompositeTwoPixels<m128i_t,op>(s12, sa12, d12, da12);

      m128i_t s34 = simd::UnpackHi8x8To8x16(s1234);
      m128i_t d34 = simd::UnpackHi8x8To8x16(d1234);
      m128i_t sa34 = simd::SplatHi16<3>(simd::SplatLo16<3>(s34));
      m128i_t da34 = simd::SplatHi16<3>(simd::SplatLo16<3>(d34));
      m128i_t result34 = CompositeTwoPixels<m128i_t,op>(s34, sa34, d34, da34);

      m128i_t result1234 = simd::PackAndSaturate16To8(result12, result34);
      simd::StoreTo((m128i_t*)&destData[destIndex], result1234);
    }
  }
}

template<typename m128i_t>
static void
ApplyComposition(DataSourceSurface* aSource, DataSourceSurface* aDest,
                 CompositeOperator aOperator)
{
  switch (aOperator) {
    case COMPOSITE_OPERATOR_OVER:
      ApplyComposition<m128i_t, COMPOSITE_OPERATOR_OVER>(aSource, aDest);
      break;
    case COMPOSITE_OPERATOR_IN:
      ApplyComposition<m128i_t, COMPOSITE_OPERATOR_IN>(aSource, aDest);
      break;
    case COMPOSITE_OPERATOR_OUT:
      ApplyComposition<m128i_t, COMPOSITE_OPERATOR_OUT>(aSource, aDest);
      break;
    case COMPOSITE_OPERATOR_ATOP:
      ApplyComposition<m128i_t, COMPOSITE_OPERATOR_ATOP>(aSource, aDest);
      break;
    case COMPOSITE_OPERATOR_XOR:
      ApplyComposition<m128i_t, COMPOSITE_OPERATOR_XOR>(aSource, aDest);
      break;
    default:
      MOZ_CRASH();
  }
}

TemporaryRef<DataSourceSurface>
FilterNodeCompositeSoftware::Render(const IntRect& aRect)
{
  RefPtr<DataSourceSurface> start =
    GetInputDataSourceSurface(IN_COMPOSITE_IN_START, aRect, NEED_COLOR_CHANNELS);
  RefPtr<DataSourceSurface> dest =
    Factory::CreateDataSourceSurface(aRect.Size(), FORMAT_B8G8R8A8);
  if (!start || !dest) {
    return nullptr;
  }
  CopyRect(start, dest, aRect - aRect.TopLeft(), IntPoint());
  for (size_t inputIndex = 1; inputIndex < NumberOfSetInputs(); inputIndex++) {
    RefPtr<DataSourceSurface> input =
      GetInputDataSourceSurface(IN_COMPOSITE_IN_START + inputIndex, aRect, NEED_COLOR_CHANNELS);
    if (!input) {
      return nullptr;
    }
#ifdef COMPILE_WITH_SSE2
    if (Factory::HasSSE2()) {
      ApplyComposition<__m128i>(input, dest, mOperator);
      continue;
    }
#endif
    ApplyComposition<simd::ScalarM128i>(input, dest, mOperator);
  }
  return dest;
}

void
FilterNodeCompositeSoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  for (size_t inputIndex = 0; inputIndex < NumberOfSetInputs(); inputIndex++) {
    RequestInputRect(IN_COMPOSITE_IN_START + inputIndex, aRect);
  }
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

int32_t
FilterNodeBlurXYSoftware::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_GAUSSIAN_BLUR_IN: return 0;
    default: return -1;
  }
}

static void
SeparateColorChannels(DataSourceSurface* aSource,
                      RefPtr<DataSourceSurface>& aChannel0,
                      RefPtr<DataSourceSurface>& aChannel1,
                      RefPtr<DataSourceSurface>& aChannel2,
                      RefPtr<DataSourceSurface>& aChannel3)
{
  IntSize size = aSource->GetSize();
  aChannel0 = Factory::CreateDataSourceSurface(size, FORMAT_A8);
  aChannel1 = Factory::CreateDataSourceSurface(size, FORMAT_A8);
  aChannel2 = Factory::CreateDataSourceSurface(size, FORMAT_A8);
  aChannel3 = Factory::CreateDataSourceSurface(size, FORMAT_A8);
  int32_t sourceStride = aSource->Stride();
  uint8_t* sourceData = aSource->GetData();
  int32_t channelStride = aChannel0->Stride();
  uint8_t* channel0Data = aChannel0->GetData();
  uint8_t* channel1Data = aChannel1->GetData();
  uint8_t* channel2Data = aChannel2->GetData();
  uint8_t* channel3Data = aChannel3->GetData();

#ifdef COMPILE_WITH_SSE2
  if (Factory::HasSSE2()) {
    for (int32_t y = 0; y < size.height; y++) {
      for (int32_t x = 0; x < size.width; x += 16) {
        // Process 16 pixels at a time.
        int32_t sourceIndex = y * sourceStride + 4 * x;
        int32_t targetIndex = y * channelStride + x;

        __m128i bgrabgrabgrabgra1 = simd::From16<__m128i>(0);
        __m128i bgrabgrabgrabgra2 = simd::From16<__m128i>(0);
        __m128i bgrabgrabgrabgra3 = simd::From16<__m128i>(0);
        __m128i bgrabgrabgrabgra4 = simd::From16<__m128i>(0);

        bgrabgrabgrabgra1 = simd::LoadFrom<__m128i>((__m128i*)&sourceData[sourceIndex]);
        if (4 * (x + 4) <= sourceStride) {
          bgrabgrabgrabgra2 = simd::LoadFrom<__m128i>((__m128i*)&sourceData[sourceIndex + 4 * 4]);
        }
        if (4 * (x + 8) <= sourceStride) {
          bgrabgrabgrabgra3 = simd::LoadFrom<__m128i>((__m128i*)&sourceData[sourceIndex + 4 * 8]);
        }
        if (4 * (x + 12) <= sourceStride) {
          bgrabgrabgrabgra4 = simd::LoadFrom<__m128i>((__m128i*)&sourceData[sourceIndex + 4 * 12]);
        }

        __m128i bbggrraabbggrraa1 = _mm_unpacklo_epi8(bgrabgrabgrabgra1, bgrabgrabgrabgra3);
        __m128i bbggrraabbggrraa2 = _mm_unpackhi_epi8(bgrabgrabgrabgra1, bgrabgrabgrabgra3);
        __m128i bbggrraabbggrraa3 = _mm_unpacklo_epi8(bgrabgrabgrabgra2, bgrabgrabgrabgra4);
        __m128i bbggrraabbggrraa4 = _mm_unpackhi_epi8(bgrabgrabgrabgra2, bgrabgrabgrabgra4);
        __m128i bbbbggggrrrraaaa1 = _mm_unpacklo_epi8(bbggrraabbggrraa1, bbggrraabbggrraa3);
        __m128i bbbbggggrrrraaaa2 = _mm_unpackhi_epi8(bbggrraabbggrraa1, bbggrraabbggrraa3);
        __m128i bbbbggggrrrraaaa3 = _mm_unpacklo_epi8(bbggrraabbggrraa2, bbggrraabbggrraa4);
        __m128i bbbbggggrrrraaaa4 = _mm_unpackhi_epi8(bbggrraabbggrraa2, bbggrraabbggrraa4);
        __m128i bbbbbbbbgggggggg1 = _mm_unpacklo_epi8(bbbbggggrrrraaaa1, bbbbggggrrrraaaa3);
        __m128i rrrrrrrraaaaaaaa1 = _mm_unpackhi_epi8(bbbbggggrrrraaaa1, bbbbggggrrrraaaa3);
        __m128i bbbbbbbbgggggggg2 = _mm_unpacklo_epi8(bbbbggggrrrraaaa2, bbbbggggrrrraaaa4);
        __m128i rrrrrrrraaaaaaaa2 = _mm_unpackhi_epi8(bbbbggggrrrraaaa2, bbbbggggrrrraaaa4);
        __m128i bbbbbbbbbbbbbbbb = _mm_unpacklo_epi8(bbbbbbbbgggggggg1, bbbbbbbbgggggggg2);
        __m128i gggggggggggggggg = _mm_unpackhi_epi8(bbbbbbbbgggggggg1, bbbbbbbbgggggggg2);
        __m128i rrrrrrrrrrrrrrrr = _mm_unpacklo_epi8(rrrrrrrraaaaaaaa1, rrrrrrrraaaaaaaa2);
        __m128i aaaaaaaaaaaaaaaa = _mm_unpackhi_epi8(rrrrrrrraaaaaaaa1, rrrrrrrraaaaaaaa2);

        simd::StoreTo((__m128i*)&channel0Data[targetIndex], bbbbbbbbbbbbbbbb);
        simd::StoreTo((__m128i*)&channel1Data[targetIndex], gggggggggggggggg);
        simd::StoreTo((__m128i*)&channel2Data[targetIndex], rrrrrrrrrrrrrrrr);
        simd::StoreTo((__m128i*)&channel3Data[targetIndex], aaaaaaaaaaaaaaaa);
      }
    }
    return;
  }
#endif

  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x++) {
      int32_t sourceIndex = y * sourceStride + 4 * x;
      int32_t targetIndex = y * channelStride + x;
      channel0Data[targetIndex] = sourceData[sourceIndex];
      channel1Data[targetIndex] = sourceData[sourceIndex+1];
      channel2Data[targetIndex] = sourceData[sourceIndex+2];
      channel3Data[targetIndex] = sourceData[sourceIndex+3];
    }
  }
}

static TemporaryRef<DataSourceSurface>
CombineColorChannels(DataSourceSurface* aChannel0, DataSourceSurface* aChannel1,
                     DataSourceSurface* aChannel2, DataSourceSurface* aChannel3)
{
  IntSize size = aChannel0->GetSize();
  RefPtr<DataSourceSurface> result =
    Factory::CreateDataSourceSurface(size, FORMAT_B8G8R8A8);
  int32_t resultStride = result->Stride();
  uint8_t* resultData = result->GetData();
  int32_t channelStride = aChannel0->Stride();
  uint8_t* channel0Data = aChannel0->GetData();
  uint8_t* channel1Data = aChannel1->GetData();
  uint8_t* channel2Data = aChannel2->GetData();
  uint8_t* channel3Data = aChannel3->GetData();

#ifdef COMPILE_WITH_SSE2
  if (Factory::HasSSE2()) {
    for (int32_t y = 0; y < size.height; y++) {
      for (int32_t x = 0; x < size.width; x += 16) {
        // Process 16 pixels at a time.
        int32_t resultIndex = y * resultStride + 4 * x;
        int32_t channelIndex = y * channelStride + x;

        __m128i bbbbbbbbbbbbbbbb = simd::LoadFrom<__m128i>((__m128i*)&channel0Data[channelIndex]);
        __m128i gggggggggggggggg = simd::LoadFrom<__m128i>((__m128i*)&channel1Data[channelIndex]);
        __m128i rrrrrrrrrrrrrrrr = simd::LoadFrom<__m128i>((__m128i*)&channel2Data[channelIndex]);
        __m128i aaaaaaaaaaaaaaaa = simd::LoadFrom<__m128i>((__m128i*)&channel3Data[channelIndex]);

        __m128i brbrbrbrbrbrbrbr1 = _mm_unpacklo_epi8(bbbbbbbbbbbbbbbb, rrrrrrrrrrrrrrrr);
        __m128i brbrbrbrbrbrbrbr2 = _mm_unpackhi_epi8(bbbbbbbbbbbbbbbb, rrrrrrrrrrrrrrrr);
        __m128i gagagagagagagaga1 = _mm_unpacklo_epi8(gggggggggggggggg, aaaaaaaaaaaaaaaa);
        __m128i gagagagagagagaga2 = _mm_unpackhi_epi8(gggggggggggggggg, aaaaaaaaaaaaaaaa);

        __m128i bgrabgrabgrabgra1 = _mm_unpacklo_epi8(brbrbrbrbrbrbrbr1, gagagagagagagaga1);
        __m128i bgrabgrabgrabgra2 = _mm_unpackhi_epi8(brbrbrbrbrbrbrbr1, gagagagagagagaga1);
        __m128i bgrabgrabgrabgra3 = _mm_unpacklo_epi8(brbrbrbrbrbrbrbr2, gagagagagagagaga2);
        __m128i bgrabgrabgrabgra4 = _mm_unpackhi_epi8(brbrbrbrbrbrbrbr2, gagagagagagagaga2);

        simd::StoreTo((__m128i*)&resultData[resultIndex], bgrabgrabgrabgra1);
        if (4 * (x + 4) <= resultStride) {
          simd::StoreTo((__m128i*)&resultData[resultIndex + 4 * 4], bgrabgrabgrabgra2);
        }
        if (4 * (x + 8) <= resultStride) {
          simd::StoreTo((__m128i*)&resultData[resultIndex + 8 * 4], bgrabgrabgrabgra3);
        }
        if (4 * (x + 12) <= resultStride) {
          simd::StoreTo((__m128i*)&resultData[resultIndex + 12 * 4], bgrabgrabgrabgra4);
        }
      }
    }
    return result;
  }
#endif

  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x++) {
      int32_t resultIndex = y * resultStride + 4 * x;
      int32_t channelIndex = y * channelStride + x;
      resultData[resultIndex] = channel0Data[channelIndex];
      resultData[resultIndex+1] = channel1Data[channelIndex];
      resultData[resultIndex+2] = channel2Data[channelIndex];
      resultData[resultIndex+3] = channel3Data[channelIndex];
    }
  }
  return result;
}

TemporaryRef<DataSourceSurface>
FilterNodeBlurXYSoftware::Render(const IntRect& aRect)
{
  Size sigmaXY = StdDeviationXY();
  uint32_t dx = GetBlurBoxSize(sigmaXY.width);
  uint32_t dy = GetBlurBoxSize(sigmaXY.height);

  if (dx == 0 && dy == 0) {
    return GetInputDataSourceSurface(IN_GAUSSIAN_BLUR_IN, aRect);
  }

  IntRect srcRect = InflatedSourceOrDestRect(aRect);
  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_GAUSSIAN_BLUR_IN, srcRect);
  if (!input) {
    return nullptr;
  }

  RefPtr<DataSourceSurface> target;
  Rect r(0, 0, srcRect.width, srcRect.height);

  if (input->GetFormat() == FORMAT_A8) {
    target = Factory::CreateDataSourceSurface(srcRect.Size(), FORMAT_A8);
    CopyRect(input, target, IntRect(IntPoint(), input->GetSize()), IntPoint());
    AlphaBoxBlur blur(r, target->Stride(), sigmaXY.width, sigmaXY.height);
    blur.Blur(target->GetData());
  } else {
    RefPtr<DataSourceSurface> channel0, channel1, channel2, channel3;
    SeparateColorChannels(input, channel0, channel1, channel2, channel3);
    AlphaBoxBlur blur(r, channel0->Stride(), sigmaXY.width, sigmaXY.height);
    blur.Blur(channel0->GetData());
    blur.Blur(channel1->GetData());
    blur.Blur(channel2->GetData());
    blur.Blur(channel3->GetData());
    target = CombineColorChannels(channel0, channel1, channel2, channel3);
  }

  return GetDataSurfaceInRect(target, srcRect, aRect, EDGE_MODE_NONE);
}

void
FilterNodeBlurXYSoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  RequestInputRect(IN_GAUSSIAN_BLUR_IN, InflatedSourceOrDestRect(aRect));
}

IntRect
FilterNodeBlurXYSoftware::InflatedSourceOrDestRect(const IntRect &aDestRect)
{
  Size sigmaXY = StdDeviationXY();
  uint32_t dx = GetBlurBoxSize(sigmaXY.width);
  uint32_t dy = GetBlurBoxSize(sigmaXY.height);
  IntRect srcRect = aDestRect;
  InflateRectForBlurDXY(&srcRect, dx, dy);
  return srcRect;
}

IntRect
FilterNodeBlurXYSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  IntRect srcRequest = InflatedSourceOrDestRect(aRect);
  IntRect srcOutput = GetInputRectInRect(IN_GAUSSIAN_BLUR_IN, srcRequest);
  return InflatedSourceOrDestRect(srcOutput).Intersect(aRect);
}

FilterNodeGaussianBlurSoftware::FilterNodeGaussianBlurSoftware()
 : mStdDeviation(0)
{}

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
  Invalidate();
}

Size
FilterNodeGaussianBlurSoftware::StdDeviationXY()
{
  return Size(mStdDeviation, mStdDeviation);
}

FilterNodeDirectionalBlurSoftware::FilterNodeDirectionalBlurSoftware()
 : mBlurDirection(BLUR_DIRECTION_X)
{}

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
  Invalidate();
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
  Invalidate();
}

Size
FilterNodeDirectionalBlurSoftware::StdDeviationXY()
{
  float sigmaX = mBlurDirection == BLUR_DIRECTION_X ? mStdDeviation : 0;
  float sigmaY = mBlurDirection == BLUR_DIRECTION_Y ? mStdDeviation : 0;
  return Size(sigmaX, sigmaY);
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
  Invalidate();
}

TemporaryRef<DataSourceSurface>
FilterNodeCropSoftware::Render(const IntRect& aRect)
{
  IntRect sourceRect = aRect.Intersect(mCropRect);
  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_CROP_IN, sourceRect);
  if (!input) {
    return nullptr;
  }
  return GetDataSurfaceInRect(input, sourceRect, aRect, EDGE_MODE_NONE);
}

void
FilterNodeCropSoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  RequestInputRect(IN_CROP_IN, aRect.Intersect(mCropRect));
}

IntRect
FilterNodeCropSoftware::GetOutputRectInRect(const IntRect& aRect)
{
  return GetInputRectInRect(IN_CROP_IN, aRect).Intersect(mCropRect);
}

template<typename m128i_t>
static void
DoPremultiplicationCalculation(const IntSize& aSize,
                               uint8_t* aTargetData, int32_t aTargetStride,
                               uint8_t* aSourceData, int32_t aSourceStride)
{
  for (int32_t y = 0; y < aSize.height; y++) {
    for (int32_t x = 0; x < aSize.width; x += 4) {
      int32_t inputIndex = y * aSourceStride + 4 * x;
      int32_t targetIndex = y * aTargetStride + 4 * x;
      m128i_t p1234 = simd::LoadFrom((m128i_t*)&aSourceData[inputIndex]);
      m128i_t p12 = simd::UnpackLo8x8To8x16(p1234);
      m128i_t p34 = simd::UnpackHi8x8To8x16(p1234);
      m128i_t a12 = simd::SplatHi16<3>(simd::SplatLo16<3>(p12));
      a12 = simd::SetComponent16<7>(simd::SetComponent16<3>(a12, 255), 255);
      m128i_t a34 = simd::SplatHi16<3>(simd::SplatLo16<3>(p34));
      a34 = simd::SetComponent16<7>(simd::SetComponent16<3>(a34, 255), 255);
      m128i_t p1, p2, p3, p4;
      simd::Mul2x2x4x16To2x4x32(p12, a12, p1, p2);
      simd::Mul2x2x4x16To2x4x32(p34, a34, p3, p4);
      m128i_t result = simd::PackAndSaturate32To8(simd::FastDivideBy255(p1),
                                                  simd::FastDivideBy255(p2),
                                                  simd::FastDivideBy255(p3),
                                                  simd::FastDivideBy255(p4));
      simd::StoreTo((m128i_t*)&aTargetData[targetIndex], result);
    }
  }
}

template<>
void
DoPremultiplicationCalculation<simd::ScalarM128i>
                              (const IntSize& aSize,
                               uint8_t* aTargetData, int32_t aTargetStride,
                               uint8_t* aSourceData, int32_t aSourceStride)
{
  for (int32_t y = 0; y < aSize.height; y++) {
    for (int32_t x = 0; x < aSize.width; x++) {
      int32_t inputIndex = y * aSourceStride + 4 * x;
      int32_t targetIndex = y * aTargetStride + 4 * x;
      uint8_t alpha = aSourceData[inputIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_A];
      aTargetData[targetIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_R] =
        FastDivideBy255<uint8_t>(aSourceData[inputIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_R] * alpha);
      aTargetData[targetIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_G] =
        FastDivideBy255<uint8_t>(aSourceData[inputIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_G] * alpha);
      aTargetData[targetIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_B] =
        FastDivideBy255<uint8_t>(aSourceData[inputIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_B] * alpha);
      aTargetData[targetIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_A] = alpha;
    }
  }
}

static TemporaryRef<DataSourceSurface>
Premultiply(DataSourceSurface* aSurface)
{
  if (aSurface->GetFormat() == FORMAT_A8) {
    return aSurface;
  }

  IntSize size = aSurface->GetSize();
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(size, FORMAT_B8G8R8A8);
  if (!target) {
    return nullptr;
  }

  uint8_t* inputData = aSurface->GetData();
  int32_t inputStride = aSurface->Stride();
  uint8_t* targetData = target->GetData();
  int32_t targetStride = target->Stride();

#ifdef COMPILE_WITH_SSE2
  if (Factory::HasSSE2()) {
    DoPremultiplicationCalculation<__m128i>(
      size, targetData, targetStride, inputData, inputStride);
    return target;
  }
#endif
  DoPremultiplicationCalculation<simd::ScalarM128i>(
    size, targetData, targetStride, inputData, inputStride);

  return target;
}

// We use a table of precomputed factors for unpremultiplying.
// We want to compute round(r / (alpha / 255.0f)) for arbitrary values of
// r and alpha in constant time. This table of factors has the property that
// (r * sAlphaFactors[alpha] + 128) >> 8 roughly gives the result we want (with
// a maximum deviation of 1).
//
// sAlphaFactors[alpha] == round(255.0 * (1 << 8) / alpha)
//
// This table has been created using the python code
// ", ".join("%d" % (round(255.0 * 256 / alpha) if alpha > 0 else 0) for alpha in range(256))
static const uint16_t sAlphaFactors[256] = {
  0, 65280, 32640, 21760, 16320, 13056, 10880, 9326, 8160, 7253, 6528, 5935,
  5440, 5022, 4663, 4352, 4080, 3840, 3627, 3436, 3264, 3109, 2967, 2838, 2720,
  2611, 2511, 2418, 2331, 2251, 2176, 2106, 2040, 1978, 1920, 1865, 1813, 1764,
  1718, 1674, 1632, 1592, 1554, 1518, 1484, 1451, 1419, 1389, 1360, 1332, 1306,
  1280, 1255, 1232, 1209, 1187, 1166, 1145, 1126, 1106, 1088, 1070, 1053, 1036,
  1020, 1004, 989, 974, 960, 946, 933, 919, 907, 894, 882, 870, 859, 848, 837,
  826, 816, 806, 796, 787, 777, 768, 759, 750, 742, 733, 725, 717, 710, 702,
  694, 687, 680, 673, 666, 659, 653, 646, 640, 634, 628, 622, 616, 610, 604,
  599, 593, 588, 583, 578, 573, 568, 563, 558, 553, 549, 544, 540, 535, 531,
  526, 522, 518, 514, 510, 506, 502, 498, 495, 491, 487, 484, 480, 476, 473,
  470, 466, 463, 460, 457, 453, 450, 447, 444, 441, 438, 435, 432, 429, 427,
  424, 421, 418, 416, 413, 411, 408, 405, 403, 400, 398, 396, 393, 391, 389,
  386, 384, 382, 380, 377, 375, 373, 371, 369, 367, 365, 363, 361, 359, 357,
  355, 353, 351, 349, 347, 345, 344, 342, 340, 338, 336, 335, 333, 331, 330,
  328, 326, 325, 323, 322, 320, 318, 317, 315, 314, 312, 311, 309, 308, 306,
  305, 304, 302, 301, 299, 298, 297, 295, 294, 293, 291, 290, 289, 288, 286,
  285, 284, 283, 281, 280, 279, 278, 277, 275, 274, 273, 272, 271, 270, 269,
  268, 266, 265, 264, 263, 262, 261, 260, 259, 258, 257, 256
};

template<typename m128i_t>
static void
DoUnpremultiplicationCalculation(const IntSize& aSize,
                                 uint8_t* aTargetData, int32_t aTargetStride,
                                 uint8_t* aSourceData, int32_t aSourceStride)
{
  for (int32_t y = 0; y < aSize.height; y++) {
    for (int32_t x = 0; x < aSize.width; x += 4) {
      int32_t inputIndex = y * aSourceStride + 4 * x;
      int32_t targetIndex = y * aTargetStride + 4 * x;
      union {
        m128i_t p1234;
        uint8_t u8[4][4];
      };
      p1234 = simd::LoadFrom((m128i_t*)&aSourceData[inputIndex]);
      // We interpret the alpha factors as signed even though they're unsigned,
      // because the From16 call below expects signed ints. The conversion does
      // not lose any information, and the multiplication works as if they were
      // unsigned (since we only take the lower 16 bits of each 32-bit result),
      // so everything works as if the factors were unsigned all along.
      int16_t aF1 = (int16_t)sAlphaFactors[u8[0][B8G8R8A8_COMPONENT_BYTEOFFSET_A]];
      int16_t aF2 = (int16_t)sAlphaFactors[u8[1][B8G8R8A8_COMPONENT_BYTEOFFSET_A]];
      int16_t aF3 = (int16_t)sAlphaFactors[u8[2][B8G8R8A8_COMPONENT_BYTEOFFSET_A]];
      int16_t aF4 = (int16_t)sAlphaFactors[u8[3][B8G8R8A8_COMPONENT_BYTEOFFSET_A]];
      m128i_t p12 = simd::UnpackLo8x8To8x16(p1234);
      m128i_t p34 = simd::UnpackHi8x8To8x16(p1234);
      m128i_t aF12 = simd::From16<m128i_t>(aF1, aF1, aF1, 1 << 8, aF2, aF2, aF2, 1 << 8);
      m128i_t aF34 = simd::From16<m128i_t>(aF3, aF3, aF3, 1 << 8, aF4, aF4, aF4, 1 << 8);
      p12 = simd::ShiftRight16(simd::Add16(simd::Mul16(p12, aF12), simd::From16<m128i_t>(128)), 8);
      p34 = simd::ShiftRight16(simd::Add16(simd::Mul16(p34, aF34), simd::From16<m128i_t>(128)), 8);
      m128i_t result = simd::PackAndSaturate16To8(p12, p34);
      simd::StoreTo((m128i_t*)&aTargetData[targetIndex], result);
    }
  }
}

template<>
void
DoUnpremultiplicationCalculation<simd::ScalarM128i>(
                                 const IntSize& aSize,
                                 uint8_t* aTargetData, int32_t aTargetStride,
                                 uint8_t* aSourceData, int32_t aSourceStride)
{
  for (int32_t y = 0; y < aSize.height; y++) {
    for (int32_t x = 0; x < aSize.width; x++) {
      int32_t inputIndex = y * aSourceStride + 4 * x;
      int32_t targetIndex = y * aTargetStride + 4 * x;
      uint8_t alpha = aSourceData[inputIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_A];
      uint16_t alphaFactor = sAlphaFactors[alpha];
      // inputColor * alphaFactor + 128 is guaranteed to fit into uint16_t
      // because the input is premultiplied and thus inputColor <= inputAlpha.
      // The maximum value this can attain is 65520 (which is less than 65535)
      // for color == alpha == 244:
      // 244 * sAlphaFactors[244] + 128 == 244 * 268 + 128 == 65520
      aTargetData[targetIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_R] =
        (aSourceData[inputIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_R] * alphaFactor + 128) >> 8;
      aTargetData[targetIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_G] =
        (aSourceData[inputIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_G] * alphaFactor + 128) >> 8;
      aTargetData[targetIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_B] =
        (aSourceData[inputIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_B] * alphaFactor + 128) >> 8;
      aTargetData[targetIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_A] = alpha;
    }
  }
}

static TemporaryRef<DataSourceSurface>
Unpremultiply(DataSourceSurface* aSurface)
{
  if (aSurface->GetFormat() == FORMAT_A8) {
    return aSurface;
  }

  IntSize size = aSurface->GetSize();
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(size, FORMAT_B8G8R8A8);
  if (!target) {
    return nullptr;
  }

  uint8_t* inputData = aSurface->GetData();
  int32_t inputStride = aSurface->Stride();
  uint8_t* targetData = target->GetData();
  int32_t targetStride = target->Stride();

#ifdef COMPILE_WITH_SSE2
  if (Factory::HasSSE2()) {
    DoUnpremultiplicationCalculation<__m128i>(
      size, targetData, targetStride, inputData, inputStride);
    return target;
  }
#endif
  DoUnpremultiplicationCalculation<simd::ScalarM128i>(
    size, targetData, targetStride, inputData, inputStride);

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
  return input ? Premultiply(input) : nullptr;
}

void
FilterNodePremultiplySoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  RequestInputRect(IN_PREMULTIPLY_IN, aRect);
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
  return input ? Unpremultiply(input) : nullptr;
}

void
FilterNodeUnpremultiplySoftware::RequestFromInputsForRect(const IntRect &aRect)
{
  RequestInputRect(IN_UNPREMULTIPLY_IN, aRect);
}

IntRect
FilterNodeUnpremultiplySoftware::GetOutputRectInRect(const IntRect& aRect)
{
  return GetInputRectInRect(IN_UNPREMULTIPLY_IN, aRect);
}

bool
PointLightSoftware::SetAttribute(uint32_t aIndex, const Point3D &aPoint)
{
  switch (aIndex) {
    case ATT_POINT_LIGHT_POSITION:
      mPosition = aPoint;
      break;
    default:
      return false;
  }
  return true;
}

SpotLightSoftware::SpotLightSoftware()
 : mSpecularFocus(0)
 , mLimitingConeAngle(0)
 , mLimitingConeCos(1)
{
}

bool
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
      return false;
  }
  return true;
}

bool
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
      return false;
  }
  return true;
}

DistantLightSoftware::DistantLightSoftware()
 : mAzimuth(0)
 , mElevation(0)
{
}

bool
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
      return false;
  }
  return true;
}

static inline Point3D NORMALIZE(const Point3D &vec) {
  Point3D copy(vec);
  copy.Normalize();
  return copy;
}

template<typename LightType, typename LightingType>
FilterNodeLightingSoftware<LightType, LightingType>::FilterNodeLightingSoftware()
 : mSurfaceScale(0)
{}

template<typename LightType, typename LightingType>
int32_t
FilterNodeLightingSoftware<LightType, LightingType>::InputIndex(uint32_t aInputEnumIndex)
{
  switch (aInputEnumIndex) {
    case IN_LIGHTING_IN: return 0;
    default: return -1;
  }
}

template<typename LightType, typename LightingType>
void
FilterNodeLightingSoftware<LightType, LightingType>::SetAttribute(uint32_t aIndex, const Point3D &aPoint)
{
  if (mLight.SetAttribute(aIndex, aPoint)) {
    Invalidate();
    return;
  }
  MOZ_CRASH();
}

template<typename LightType, typename LightingType>
void
FilterNodeLightingSoftware<LightType, LightingType>::SetAttribute(uint32_t aIndex, Float aValue)
{
  if (mLight.SetAttribute(aIndex, aValue) ||
      mLighting.SetAttribute(aIndex, aValue)) {
    Invalidate();
    return;
  }
  switch (aIndex) {
    case ATT_LIGHTING_SURFACE_SCALE:
      mSurfaceScale = aValue;
      break;
    default:
      MOZ_CRASH();
  }
  Invalidate();
}

template<typename LightType, typename LightingType>
void
FilterNodeLightingSoftware<LightType, LightingType>::SetAttribute(uint32_t aIndex, const Size &aKernelUnitLength)
{
  switch (aIndex) {
    case ATT_LIGHTING_KERNEL_UNIT_LENGTH:
      mKernelUnitLength = aKernelUnitLength;
      break;
    default:
      MOZ_CRASH();
  }
  Invalidate();
}

template<typename LightType, typename LightingType>
void
FilterNodeLightingSoftware<LightType, LightingType>::SetAttribute(uint32_t aIndex, const Color &aColor)
{
  MOZ_ASSERT(aIndex == ATT_LIGHTING_COLOR);
  mColor = aColor;
  Invalidate();
}

template<typename LightType, typename LightingType>
IntRect
FilterNodeLightingSoftware<LightType, LightingType>::GetOutputRectInRect(const IntRect& aRect)
{
  return GetInputRectInRect(IN_LIGHTING_IN, aRect);
}

Point3D
PointLightSoftware::GetInverseRayDirection(const Point3D &aTargetPoint)
{
  return NORMALIZE(mPosition - aTargetPoint);
}

uint32_t
PointLightSoftware::GetColor(uint32_t aLightColor, const Point3D &aInverseRayDirection)
{
  return aLightColor;
}

void
SpotLightSoftware::Prepare()
{
  mInverseCoreRayDirection = NORMALIZE(mPointsAt - mPosition);
  const float radPerDeg = static_cast<float>(M_PI/180.0);
  mLimitingConeCos = std::max<double>(cos(mLimitingConeAngle * radPerDeg), 0.0);
}

Point3D
SpotLightSoftware::GetInverseRayDirection(const Point3D &aTargetPoint)
{
  return NORMALIZE(mPosition - aTargetPoint);
}

uint32_t
SpotLightSoftware::GetColor(uint32_t aLightColor, const Point3D &aInverseRayDirection)
{
  union {
    uint32_t color;
    uint8_t colorC[4];
  };
  color = aLightColor;
  Float dot = -aInverseRayDirection.DotProduct(mInverseCoreRayDirection);
  Float tmp = dot < mLimitingConeCos ? 0 : pow(dot, mSpecularFocus);
  colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_R] = uint8_t(colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_R] * tmp);
  colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_G] = uint8_t(colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_G] * tmp);
  colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_B] = uint8_t(colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_B] * tmp);
  colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_A] = 255;
  return color;
}

void
DistantLightSoftware::Prepare()
{
  const float radPerDeg = static_cast<float>(M_PI/180.0);
  mInverseRayDirection.x = cos(mAzimuth * radPerDeg) * cos(mElevation * radPerDeg);
  mInverseRayDirection.y = sin(mAzimuth * radPerDeg) * cos(mElevation * radPerDeg);
  mInverseRayDirection.z = sin(mElevation * radPerDeg);
}

Point3D
DistantLightSoftware::GetInverseRayDirection(const Point3D &aTargetPoint)
{
  return mInverseRayDirection;
}

uint32_t
DistantLightSoftware::GetColor(uint32_t aLightColor, const Point3D &aInverseRayDirection)
{
  return aLightColor;
}

template<typename CoordType>
static int32_t
Convolve3x3(const uint8_t *index, int32_t stride,
            const int8_t kernel[3][3],
            CoordType kernelUnitLengthX, CoordType kernelUnitLengthY)
{
  int32_t sum = 0;
  for (int32_t y = 0; y < 3; y++) {
    for (int32_t x = 0; x < 3; x++) {
      sum += kernel[y][x] *
        ColorComponentAtPoint(index, stride,
                              (x - 1) * kernelUnitLengthX,
                              (y - 1) * kernelUnitLengthY, 0);
    }
  }
  return sum;
}

template<typename CoordType>
static Point3D
GenerateNormal(const uint8_t *data, int32_t stride,
               int32_t surfaceWidth, int32_t surfaceHeight,
               int32_t x, int32_t y, float surfaceScale,
               CoordType kernelUnitLengthX, CoordType kernelUnitLengthY)
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
        { {  0,  0,  0}, {-1, -2,  0}, { 1,  2,  0} } },
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
    return Point3D(0, 0, 1);
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

  const uint8_t *index = data + y * stride + 4 * x + B8G8R8A8_COMPONENT_BYTEOFFSET_A;

  Point3D normal;
  normal.x = -surfaceScale * FACTORx[yflag][xflag] *
    Convolve3x3(index, stride, Kx[yflag][xflag], kernelUnitLengthX, kernelUnitLengthY);
  normal.y = -surfaceScale * FACTORy[yflag][xflag] *
    Convolve3x3(index, stride, Ky[yflag][xflag], kernelUnitLengthX, kernelUnitLengthY);
  normal.z = 255;
  return NORMALIZE(normal);
}

template<typename LightType, typename LightingType>
TemporaryRef<DataSourceSurface>
FilterNodeLightingSoftware<LightType, LightingType>::Render(const IntRect& aRect)
{
  if (mKernelUnitLength.width == floor(mKernelUnitLength.width) &&
      mKernelUnitLength.height == floor(mKernelUnitLength.height)) {
    return DoRender(aRect, (int32_t)mKernelUnitLength.width, (int32_t)mKernelUnitLength.height);
  }
  return DoRender(aRect, mKernelUnitLength.width, mKernelUnitLength.height);
}

template<typename LightType, typename LightingType>
void
FilterNodeLightingSoftware<LightType, LightingType>::RequestFromInputsForRect(const IntRect &aRect)
{
  IntRect srcRect = aRect;
  srcRect.Inflate(ceil(mKernelUnitLength.width),
                  ceil(mKernelUnitLength.height));
  RequestInputRect(IN_LIGHTING_IN, srcRect);
}

template<typename LightType, typename LightingType> template<typename CoordType>
TemporaryRef<DataSourceSurface>
FilterNodeLightingSoftware<LightType, LightingType>::DoRender(const IntRect& aRect,
                                                              CoordType aKernelUnitLengthX,
                                                              CoordType aKernelUnitLengthY)
{
  IntRect srcRect = aRect;
  IntSize size = aRect.Size();
  srcRect.Inflate(ceil(aKernelUnitLengthX),
                  ceil(aKernelUnitLengthY));
  RefPtr<DataSourceSurface> input =
    GetInputDataSourceSurface(IN_LIGHTING_IN, srcRect, NEED_COLOR_CHANNELS);
  RefPtr<DataSourceSurface> target =
    Factory::CreateDataSourceSurface(size, FORMAT_B8G8R8A8);
  if (!input || !target) {
    return nullptr;
  }

  uint8_t* sourceData = input->GetData();
  int32_t sourceStride = input->Stride();
  uint8_t* targetData = target->GetData();
  int32_t targetStride = target->Stride();

  IntPoint offset = aRect.TopLeft() - srcRect.TopLeft();
  sourceData += DataOffset(input, offset);

  uint32_t lightColor = ColorToBGRA(mColor);
  mLight.Prepare();

  for (int32_t y = 0; y < size.height; y++) {
    for (int32_t x = 0; x < size.width; x++) {
      int32_t sourceIndex = y * sourceStride + 4 * x;
      int32_t targetIndex = y * targetStride + 4 * x;

      Point3D normal = GenerateNormal(sourceData, sourceStride, size.width, size.height,
                                      x, y, mSurfaceScale,
                                      aKernelUnitLengthX, aKernelUnitLengthY);

      IntPoint pointInFilterSpace(aRect.x + x, aRect.y + y);
      Float Z = mSurfaceScale * sourceData[sourceIndex + B8G8R8A8_COMPONENT_BYTEOFFSET_A] / 255.0f;
      Point3D pt(pointInFilterSpace.x, pointInFilterSpace.y, Z);
      Point3D rayDir = mLight.GetInverseRayDirection(pt);
      uint32_t color = mLight.GetColor(lightColor, rayDir);

      *(uint32_t*)(targetData + targetIndex) = mLighting.LightPixel(normal, rayDir, color);
    }
  }

  return target;
}

DiffuseLightingSoftware::DiffuseLightingSoftware()
 : mDiffuseConstant(0)
{
}

bool
DiffuseLightingSoftware::SetAttribute(uint32_t aIndex, Float aValue)
{
  switch (aIndex) {
    case ATT_DIFFUSE_LIGHTING_DIFFUSE_CONSTANT:
      mDiffuseConstant = aValue;
      break;
    default:
      return false;
  }
  return true;
}

uint32_t
DiffuseLightingSoftware::LightPixel(const Point3D &aNormal,
                                    const Point3D &aInverseRayDirection,
                                    uint32_t aColor)
{
  float diffuseNL = mDiffuseConstant * aNormal.DotProduct(aInverseRayDirection);

  if (diffuseNL < 0) diffuseNL = 0;

  union {
    uint32_t color;
    uint8_t colorC[4];
  };
  color = aColor;
  colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_B] =
    std::min(uint32_t(diffuseNL * colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_B]), 255U);
  colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_G] =
    std::min(uint32_t(diffuseNL * colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_G]), 255U);
  colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_R] =
    std::min(uint32_t(diffuseNL * colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_R]), 255U);
  colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_A] = 255;
  return color;
}

SpecularLightingSoftware::SpecularLightingSoftware()
 : mSpecularConstant(0)
 , mSpecularExponent(0)
{
}

bool
SpecularLightingSoftware::SetAttribute(uint32_t aIndex, Float aValue)
{
  switch (aIndex) {
    case ATT_SPECULAR_LIGHTING_SPECULAR_CONSTANT:
      mSpecularConstant = aValue;
      break;
    case ATT_SPECULAR_LIGHTING_SPECULAR_EXPONENT:
      mSpecularExponent = aValue;
      break;
    default:
      return false;
  }
  return true;
}

uint32_t
SpecularLightingSoftware::LightPixel(const Point3D &aNormal,
                                     const Point3D &aInverseRayDirection,
                                     uint32_t aColor)
{
  Point3D H = aInverseRayDirection;
  H.z += 1;
  H.Normalize();

  Float kS = mSpecularConstant;
  Float dotNH = aNormal.DotProduct(H);

  bool invalid = dotNH <= 0 || kS <= 0;
  kS *= invalid ? 0 : 1;
  uint8_t minAlpha = invalid ? 255 : 0;

  Float specularNH = kS * pow(dotNH, mSpecularExponent);

  union {
    uint32_t color;
    uint8_t colorC[4];
  };
  color = aColor;
  colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_B] =
    std::min(uint32_t(specularNH * colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_B]), 255U);
  colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_G] =
    std::min(uint32_t(specularNH * colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_G]), 255U);
  colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_R] =
    std::min(uint32_t(specularNH * colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_R]), 255U);

  colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_A] =
    std::max(minAlpha, std::max(colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_B],
                            std::max(colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_G],
                                   colorC[B8G8R8A8_COMPONENT_BYTEOFFSET_R])));
  return color;
}

} // namespace gfx
} // namespace mozilla
