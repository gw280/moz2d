/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawTargetSkia.h"
#include "SourceSurfaceSkia.h"

#include "ScaledFontBase.h"
#include "ScaledFontCairo.h"
#include "FilterNodeSoftware.h"

#include "core/SkDevice.h"
#include "core/SkTypeface.h"
#include "core/SkBitmapDevice.h"
#include "gpu/SkGpuDevice.h"
#include "effects/SkGradientShader.h"
#include "effects/SkBlurDrawLooper.h"
#include "effects/SkBlurMaskFilter.h"
#include "core/SkColorFilter.h"
#include "effects/SkLayerRasterizer.h"
#include "effects/SkLayerDrawLooper.h"
#include "effects/SkDashPathEffect.h"
#include "Logging.h"
#include "HelpersSkia.h"
#include "Tools.h"
#include "DataSurfaceHelpers.h"
#include <algorithm>

#ifdef USE_SKIA_GPU
#include "gpu/SkGpuDevice.h"
#include "gpu/gl/GrGLInterface.h"
#endif

using namespace std;

namespace mozilla {
namespace gfx {

class GradientStopsSkia : public GradientStops
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(GradientStopsSkia)
  GradientStopsSkia(const std::vector<GradientStop>& aStops, uint32_t aNumStops, ExtendMode aExtendMode)
    : mCount(aNumStops)
    , mExtendMode(aExtendMode)
  {
    if (mCount == 0) {
      return;
    }

    // Skia gradients always require a stop at 0.0 and 1.0, insert these if
    // we don't have them.
    uint32_t shift = 0;
    if (aStops[0].offset != 0) {
      mCount++;
      shift = 1;
    }
    if (aStops[aNumStops-1].offset != 1) {
      mCount++;
    }
    mColors.resize(mCount);
    mPositions.resize(mCount);
    if (aStops[0].offset != 0) {
      mColors[0] = ColorToSkColor(aStops[0].color, 1.0);
      mPositions[0] = 0;
    }
    for (uint32_t i = 0; i < aNumStops; i++) {
      mColors[i + shift] = ColorToSkColor(aStops[i].color, 1.0);
      mPositions[i + shift] = SkFloatToScalar(aStops[i].offset);
    }
    if (aStops[aNumStops-1].offset != 1) {
      mColors[mCount-1] = ColorToSkColor(aStops[aNumStops-1].color, 1.0);
      mPositions[mCount-1] = SK_Scalar1;
    }
  }

  BackendType GetBackendType() const { return BackendType::SKIA; }

  std::vector<SkColor> mColors;
  std::vector<SkScalar> mPositions;
  int mCount;
  ExtendMode mExtendMode;
};

inline static ostream&
operator <<(ostream& aStream, const DrawTargetSkia& aDrawTarget)
{
  aStream << "DrawTargetSkia (" << &aDrawTarget << ")";
  return aStream;
}

/**
 * When constructing a temporary SkBitmap via GetBitmapForSurface, we may also
 * have to construct a temporary DataSourceSurface, which must live as long as
 * the SkBitmap. So we return a pair of the SkBitmap and the (optional)
 * temporary surface.
 */
struct TempBitmap
{
  SkBitmap mBitmap;
  RefPtr<SourceSurface> mTmpSurface;
};

static TempBitmap
GetBitmapForSurface(SourceSurface* aSurface)
{
  TempBitmap result;

  if (aSurface->GetType() == SurfaceType::SKIA) {
    result.mBitmap = static_cast<SourceSurfaceSkia*>(aSurface)->GetBitmap();
    return result;
  }

  RefPtr<DataSourceSurface> surf = aSurface->GetDataSurface();
  if (!surf) {
    MOZ_CRASH("Non-skia SourceSurfaces need to be DataSourceSurfaces");
  }

  result.mBitmap.setConfig(GfxFormatToSkiaConfig(surf->GetFormat()),
                                 surf->GetSize().width, surf->GetSize().height,
                                 surf->Stride());
  result.mBitmap.setPixels(surf->GetData());
  result.mTmpSurface = surf.forget();
  return result;
}

DrawTargetSkia::DrawTargetSkia()
  : mTexture(0), mSnapshot(nullptr)
{
}

DrawTargetSkia::~DrawTargetSkia()
{
}

TemporaryRef<SourceSurface>
DrawTargetSkia::Snapshot()
{
  RefPtr<SourceSurfaceSkia> snapshot = mSnapshot;
  if (!snapshot) {
    snapshot = new SourceSurfaceSkia();
    mSnapshot = snapshot;
    if (!snapshot->InitFromCanvas(mCanvas.get(), mFormat, this))
      return nullptr;
  }

  return snapshot;
}

static void
SetPaintPattern(SkPaint& aPaint, const Pattern& aPattern, TempBitmap& aTmpBitmap,
                Float aAlpha = 1.0)
{
  switch (aPattern.GetType()) {
    case PatternType::COLOR: {
      Color color = static_cast<const ColorPattern&>(aPattern).mColor;
      aPaint.setColor(ColorToSkColor(color, aAlpha));
      break;
    }
    case PatternType::LINEAR_GRADIENT: {
      const LinearGradientPattern& pat = static_cast<const LinearGradientPattern&>(aPattern);
      GradientStopsSkia *stops = static_cast<GradientStopsSkia*>(pat.mStops.get());
      SkShader::TileMode mode = ExtendModeToTileMode(stops->mExtendMode);

      if (stops->mCount >= 2) {
        SkPoint points[2];
        points[0] = SkPoint::Make(SkFloatToScalar(pat.mBegin.x), SkFloatToScalar(pat.mBegin.y));
        points[1] = SkPoint::Make(SkFloatToScalar(pat.mEnd.x), SkFloatToScalar(pat.mEnd.y));

        SkShader* shader = SkGradientShader::CreateLinear(points, 
                                                          &stops->mColors.front(), 
                                                          &stops->mPositions.front(), 
                                                          stops->mCount, 
                                                          mode);

        if (shader) {
            SkMatrix mat;
            GfxMatrixToSkiaMatrix(pat.mMatrix, mat);
            shader->setLocalMatrix(mat);
            SkSafeUnref(aPaint.setShader(shader));
        }

      } else {
        aPaint.setColor(SkColorSetARGB(0, 0, 0, 0));
      }
      break;
    }
    case PatternType::RADIAL_GRADIENT: {
      const RadialGradientPattern& pat = static_cast<const RadialGradientPattern&>(aPattern);
      GradientStopsSkia *stops = static_cast<GradientStopsSkia*>(pat.mStops.get());
      SkShader::TileMode mode = ExtendModeToTileMode(stops->mExtendMode);

      if (stops->mCount >= 2) {
        SkPoint points[2];
        points[0] = SkPoint::Make(SkFloatToScalar(pat.mCenter1.x), SkFloatToScalar(pat.mCenter1.y));
        points[1] = SkPoint::Make(SkFloatToScalar(pat.mCenter2.x), SkFloatToScalar(pat.mCenter2.y));

        SkShader* shader = SkGradientShader::CreateTwoPointConical(points[0], 
                                                                   SkFloatToScalar(pat.mRadius1),
                                                                   points[1], 
                                                                   SkFloatToScalar(pat.mRadius2),
                                                                   &stops->mColors.front(), 
                                                                   &stops->mPositions.front(), 
                                                                   stops->mCount, 
                                                                   mode);
        if (shader) {
            SkMatrix mat;
            GfxMatrixToSkiaMatrix(pat.mMatrix, mat);
            shader->setLocalMatrix(mat);
            SkSafeUnref(aPaint.setShader(shader));
        }

      } else {
        aPaint.setColor(SkColorSetARGB(0, 0, 0, 0));
      }
      break;
    }
    case PatternType::SURFACE: {
      const SurfacePattern& pat = static_cast<const SurfacePattern&>(aPattern);
      aTmpBitmap = GetBitmapForSurface(pat.mSurface);
      const SkBitmap& bitmap = aTmpBitmap.mBitmap;

      SkShader::TileMode mode = ExtendModeToTileMode(pat.mExtendMode);
      SkShader* shader = SkShader::CreateBitmapShader(bitmap, mode, mode);
      SkMatrix mat;
      GfxMatrixToSkiaMatrix(pat.mMatrix, mat);
      shader->setLocalMatrix(mat);
      SkSafeUnref(aPaint.setShader(shader));
      if (pat.mFilter == Filter::POINT) {
        aPaint.setFilterBitmap(false);
      }
      break;
    }
  }
}

struct AutoPaintSetup {
  AutoPaintSetup(SkCanvas *aCanvas, const DrawOptions& aOptions, const Pattern& aPattern)
    : mNeedsRestore(false), mAlpha(1.0)
  {
    Init(aCanvas, aOptions);
    SetPaintPattern(mPaint, aPattern, mTmpBitmap, mAlpha);
  }

  AutoPaintSetup(SkCanvas *aCanvas, const DrawOptions& aOptions)
    : mNeedsRestore(false), mAlpha(1.0)
  {
    Init(aCanvas, aOptions);
  }

  ~AutoPaintSetup()
  {
    if (mNeedsRestore) {
      mCanvas->restore();
    }
  }

  void Init(SkCanvas *aCanvas, const DrawOptions& aOptions)
  {
    mPaint.setXfermodeMode(GfxOpToSkiaOp(aOptions.mCompositionOp));
    mCanvas = aCanvas;

    //TODO: Can we set greyscale somehow?
    if (aOptions.mAntialiasMode != AntialiasMode::NONE) {
      mPaint.setAntiAlias(true);
    } else {
      mPaint.setAntiAlias(false);
    }

    // TODO: We could skip the temporary for operator_source and just
    // clear the clip rect. The other operators would be harder
    // but could be worth it to skip pushing a group.
    if (!IsOperatorBoundByMask(aOptions.mCompositionOp)) {
      mPaint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
      SkPaint temp;
      temp.setXfermodeMode(GfxOpToSkiaOp(aOptions.mCompositionOp));
      temp.setAlpha(U8CPU(aOptions.mAlpha*255));
      //TODO: Get a rect here
      mCanvas->saveLayer(nullptr, &temp);
      mNeedsRestore = true;
    } else {
      mPaint.setAlpha(U8CPU(aOptions.mAlpha*255.0));
      mAlpha = aOptions.mAlpha;
    }
    mPaint.setFilterBitmap(true);
  }

  // TODO: Maybe add an operator overload to access this easier?
  SkPaint mPaint;
  TempBitmap mTmpBitmap;
  bool mNeedsRestore;
  SkCanvas* mCanvas;
  Float mAlpha;
};

void
DrawTargetSkia::Flush()
{
  mCanvas->flush();
}

void
DrawTargetSkia::DrawSurface(SourceSurface *aSurface,
                            const Rect &aDest,
                            const Rect &aSource,
                            const DrawSurfaceOptions &aSurfOptions,
                            const DrawOptions &aOptions)
{
  if (aSource.IsEmpty()) {
    return;
  }

  MarkChanged();

  SkRect destRect = RectToSkRect(aDest);
  SkRect sourceRect = RectToSkRect(aSource);

  TempBitmap bitmap = GetBitmapForSurface(aSurface);
 
  AutoPaintSetup paint(mCanvas.get(), aOptions);
  if (aSurfOptions.mFilter == Filter::POINT) {
    paint.mPaint.setFilterBitmap(false);
  }

  mCanvas->drawBitmapRectToRect(bitmap.mBitmap, &sourceRect, destRect, &paint.mPaint);
}

void
DrawTargetSkia::DrawFilter(FilterNode *aNode,
                           const Rect &aSourceRect,
                           const Point &aDestPoint,
                           const DrawOptions &aOptions)
{
  FilterNodeSoftware* filter = static_cast<FilterNodeSoftware*>(aNode);
  filter->Draw(this, aSourceRect, aDestPoint, aOptions);
}

void
DrawTargetSkia::DrawSurfaceWithShadow(SourceSurface *aSurface,
                                      const Point &aDest,
                                      const Color &aColor,
                                      const Point &aOffset,
                                      Float aSigma,
                                      CompositionOp aOperator)
{
  MarkChanged();
  mCanvas->save(SkCanvas::kMatrix_SaveFlag);
  mCanvas->resetMatrix();

  uint32_t blurFlags = SkBlurMaskFilter::kHighQuality_BlurFlag |
                       SkBlurMaskFilter::kIgnoreTransform_BlurFlag;
  TempBitmap bitmap = GetBitmapForSurface(aSurface);
  SkShader* shader = SkShader::CreateBitmapShader(bitmap.mBitmap,
    SkShader::kClamp_TileMode, SkShader::kClamp_TileMode);
  SkMatrix matrix;
  matrix.reset();
  matrix.setTranslateX(SkFloatToScalar(aDest.x));
  matrix.setTranslateY(SkFloatToScalar(aDest.y));
  shader->setLocalMatrix(matrix);
  SkLayerDrawLooper* dl = new SkLayerDrawLooper;
  SkLayerDrawLooper::LayerInfo info;
  info.fPaintBits |= SkLayerDrawLooper::kShader_Bit;
  SkPaint *layerPaint = dl->addLayer(info);
  layerPaint->setShader(shader);

  info.fPaintBits = 0;
  info.fPaintBits |= SkLayerDrawLooper::kMaskFilter_Bit;
  info.fPaintBits |= SkLayerDrawLooper::kColorFilter_Bit;
  info.fColorMode = SkXfermode::kDst_Mode;
  info.fOffset.set(SkFloatToScalar(aOffset.x), SkFloatToScalar(aOffset.y));
  info.fPostTranslate = true;

  SkMaskFilter* mf = SkBlurMaskFilter::Create(aSigma, SkBlurMaskFilter::kNormal_BlurStyle, blurFlags);
  SkColor color = ColorToSkColor(aColor, 1);
  SkColorFilter* cf = SkColorFilter::CreateModeFilter(color, SkXfermode::kSrcIn_Mode);


  layerPaint = dl->addLayer(info);
  SkSafeUnref(layerPaint->setMaskFilter(mf));
  SkSafeUnref(layerPaint->setColorFilter(cf));
  layerPaint->setColor(color);
  
  // TODO: This is using the rasterizer to calculate an alpha mask
  // on both the shadow and normal layers. We should fix this
  // properly so it only happens for the shadow layer
  SkLayerRasterizer *raster = new SkLayerRasterizer();
  SkPaint maskPaint;
  SkSafeUnref(maskPaint.setShader(shader));
  raster->addLayer(maskPaint, 0, 0);
  
  SkPaint paint;
  paint.setAntiAlias(true);
  SkSafeUnref(paint.setRasterizer(raster));
  paint.setXfermodeMode(GfxOpToSkiaOp(aOperator));
  SkSafeUnref(paint.setLooper(dl));

  SkRect rect = RectToSkRect(Rect(Float(aDest.x), Float(aDest.y),
                                  Float(bitmap.mBitmap.width()), Float(bitmap.mBitmap.height())));
  mCanvas->drawRect(rect, paint);
  mCanvas->restore();
}

void
DrawTargetSkia::FillRect(const Rect &aRect,
                         const Pattern &aPattern,
                         const DrawOptions &aOptions)
{
  MarkChanged();
  SkRect rect = RectToSkRect(aRect);
  AutoPaintSetup paint(mCanvas.get(), aOptions, aPattern);

  mCanvas->drawRect(rect, paint.mPaint);
}

void
DrawTargetSkia::Stroke(const Path *aPath,
                       const Pattern &aPattern,
                       const StrokeOptions &aStrokeOptions,
                       const DrawOptions &aOptions)
{
  MarkChanged();
  MOZ_ASSERT(aPath, "Null path");
  if (aPath->GetBackendType() != BackendType::SKIA) {
    return;
  }

  const PathSkia *skiaPath = static_cast<const PathSkia*>(aPath);


  AutoPaintSetup paint(mCanvas.get(), aOptions, aPattern);
  if (!StrokeOptionsToPaint(paint.mPaint, aStrokeOptions)) {
    return;
  }

  mCanvas->drawPath(skiaPath->GetPath(), paint.mPaint);
}

void
DrawTargetSkia::StrokeRect(const Rect &aRect,
                           const Pattern &aPattern,
                           const StrokeOptions &aStrokeOptions,
                           const DrawOptions &aOptions)
{
  MarkChanged();
  AutoPaintSetup paint(mCanvas.get(), aOptions, aPattern);
  if (!StrokeOptionsToPaint(paint.mPaint, aStrokeOptions)) {
    return;
  }

  mCanvas->drawRect(RectToSkRect(aRect), paint.mPaint);
}

void 
DrawTargetSkia::StrokeLine(const Point &aStart,
                           const Point &aEnd,
                           const Pattern &aPattern,
                           const StrokeOptions &aStrokeOptions,
                           const DrawOptions &aOptions)
{
  MarkChanged();
  AutoPaintSetup paint(mCanvas.get(), aOptions, aPattern);
  if (!StrokeOptionsToPaint(paint.mPaint, aStrokeOptions)) {
    return;
  }

  mCanvas->drawLine(SkFloatToScalar(aStart.x), SkFloatToScalar(aStart.y), 
                    SkFloatToScalar(aEnd.x), SkFloatToScalar(aEnd.y), 
                    paint.mPaint);
}

void
DrawTargetSkia::Fill(const Path *aPath,
                    const Pattern &aPattern,
                    const DrawOptions &aOptions)
{
  MarkChanged();
  if (aPath->GetBackendType() != BackendType::SKIA) {
    return;
  }

  const PathSkia *skiaPath = static_cast<const PathSkia*>(aPath);

  AutoPaintSetup paint(mCanvas.get(), aOptions, aPattern);

  mCanvas->drawPath(skiaPath->GetPath(), paint.mPaint);
}

void
DrawTargetSkia::FillGlyphs(ScaledFont *aFont,
                           const GlyphBuffer &aBuffer,
                           const Pattern &aPattern,
                           const DrawOptions &aOptions,
                           const GlyphRenderingOptions *aRenderingOptions)
{
  if (aFont->GetType() != FontType::MAC &&
      aFont->GetType() != FontType::SKIA &&
      aFont->GetType() != FontType::GDI) {
    return;
  }

  MarkChanged();

  ScaledFontBase* skiaFont = static_cast<ScaledFontBase*>(aFont);

  AutoPaintSetup paint(mCanvas.get(), aOptions, aPattern);
  paint.mPaint.setTypeface(skiaFont->GetSkTypeface());
  paint.mPaint.setTextSize(SkFloatToScalar(skiaFont->mSize));
  paint.mPaint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);

  if (aRenderingOptions && aRenderingOptions->GetType() == FontType::CAIRO) {
    switch (static_cast<const GlyphRenderingOptionsCairo*>(aRenderingOptions)->GetHinting()) {
      case FontHinting::NONE:
        paint.mPaint.setHinting(SkPaint::kNo_Hinting);
        break;
      case FontHinting::LIGHT:
        paint.mPaint.setHinting(SkPaint::kSlight_Hinting);
        break;
      case FontHinting::NORMAL:
        paint.mPaint.setHinting(SkPaint::kNormal_Hinting);
        break;
      case FontHinting::FULL:
        paint.mPaint.setHinting(SkPaint::kFull_Hinting);
        break;
    }

    if (static_cast<const GlyphRenderingOptionsCairo*>(aRenderingOptions)->GetAutoHinting()) {
      paint.mPaint.setAutohinted(true);
    }
  } else {
    paint.mPaint.setHinting(SkPaint::kNormal_Hinting);
  }

  std::vector<uint16_t> indices;
  std::vector<SkPoint> offsets;
  indices.resize(aBuffer.mNumGlyphs);
  offsets.resize(aBuffer.mNumGlyphs);

  for (unsigned int i = 0; i < aBuffer.mNumGlyphs; i++) {
    indices[i] = aBuffer.mGlyphs[i].mIndex;
    offsets[i].fX = SkFloatToScalar(aBuffer.mGlyphs[i].mPosition.x);
    offsets[i].fY = SkFloatToScalar(aBuffer.mGlyphs[i].mPosition.y);
  }

  mCanvas->drawPosText(&indices.front(), aBuffer.mNumGlyphs*2, &offsets.front(), paint.mPaint);
}

void
DrawTargetSkia::Mask(const Pattern &aSource,
                     const Pattern &aMask,
                     const DrawOptions &aOptions)
{
  MarkChanged();
  AutoPaintSetup paint(mCanvas.get(), aOptions, aSource);

  SkPaint maskPaint;
  TempBitmap tmpBitmap;
  SetPaintPattern(maskPaint, aMask, tmpBitmap);
  
  SkLayerRasterizer *raster = new SkLayerRasterizer();
  raster->addLayer(maskPaint);
  SkSafeUnref(paint.mPaint.setRasterizer(raster));

  // Skia only uses the mask rasterizer when we are drawing a path/rect.
  // Take our destination bounds and convert them into user space to use
  // as the path to draw.
  SkPath path;
  path.addRect(SkRect::MakeWH(SkScalar(mSize.width), SkScalar(mSize.height)));
 
  Matrix temp = mTransform;
  temp.Invert();
  SkMatrix mat;
  GfxMatrixToSkiaMatrix(temp, mat);
  path.transform(mat);

  mCanvas->drawPath(path, paint.mPaint);
}

void
DrawTargetSkia::MaskSurface(const Pattern &aSource,
                            SourceSurface *aMask,
                            Point aOffset,
                            const DrawOptions &aOptions)
{
  MarkChanged();
  AutoPaintSetup paint(mCanvas.get(), aOptions, aSource);

  SkPaint maskPaint;
  TempBitmap tmpBitmap;
  SetPaintPattern(maskPaint, SurfacePattern(aMask, ExtendMode::CLAMP), tmpBitmap);

  SkMatrix transform = maskPaint.getShader()->getLocalMatrix();
  transform.postTranslate(SkFloatToScalar(aOffset.x), SkFloatToScalar(aOffset.y));
  maskPaint.getShader()->setLocalMatrix(transform);

  SkLayerRasterizer *raster = new SkLayerRasterizer();
  raster->addLayer(maskPaint);
  SkSafeUnref(paint.mPaint.setRasterizer(raster));

  IntSize size = aMask->GetSize();
  Rect rect = Rect(aOffset.x, aOffset.y, size.width, size.height);
  mCanvas->drawRect(RectToSkRect(rect), paint.mPaint);
}

TemporaryRef<SourceSurface>
DrawTargetSkia::CreateSourceSurfaceFromData(unsigned char *aData,
                                            const IntSize &aSize,
                                            int32_t aStride,
                                            SurfaceFormat aFormat) const
{
  RefPtr<SourceSurfaceSkia> newSurf = new SourceSurfaceSkia();

  if (!newSurf->InitFromData(aData, aSize, aStride, aFormat)) {
    gfxDebug() << *this << ": Failure to create source surface from data. Size: " << aSize;
    return nullptr;
  }
    
  return newSurf;
}

TemporaryRef<DrawTarget>
DrawTargetSkia::CreateSimilarDrawTarget(const IntSize &aSize, SurfaceFormat aFormat) const
{
  RefPtr<DrawTargetSkia> target = new DrawTargetSkia();
  if (!target->Init(aSize, aFormat)) {
    return nullptr;
  }
  return target;
}

TemporaryRef<SourceSurface>
DrawTargetSkia::OptimizeSourceSurface(SourceSurface *aSurface) const
{
  if (aSurface->GetType() == SurfaceType::SKIA) {
    return aSurface;
  }

  if (aSurface->GetType() != SurfaceType::DATA) {
    return nullptr;
  }

  RefPtr<DataSourceSurface> data = aSurface->GetDataSurface();
  RefPtr<SourceSurface> surface = CreateSourceSurfaceFromData(data->GetData(),
                                                              data->GetSize(),
                                                              data->Stride(),
                                                              data->GetFormat());
  return data.forget();
}

TemporaryRef<SourceSurface>
DrawTargetSkia::CreateSourceSurfaceFromNativeSurface(const NativeSurface &aSurface) const
{
  return nullptr;
}

void
DrawTargetSkia::CopySurface(SourceSurface *aSurface,
                            const IntRect& aSourceRect,
                            const IntPoint &aDestination)
{
  //TODO: We could just use writePixels() here if the sourceRect is the entire source

  if (aSurface->GetType() != SurfaceType::SKIA) {
    return;
  }

  MarkChanged();

  TempBitmap bitmap = GetBitmapForSurface(aSurface);

  mCanvas->save();
  mCanvas->resetMatrix();
  SkRect dest = IntRectToSkRect(IntRect(aDestination.x, aDestination.y, aSourceRect.width, aSourceRect.height)); 
  SkIRect source = IntRectToSkIRect(aSourceRect);
  mCanvas->clipRect(dest, SkRegion::kReplace_Op);
  SkPaint paint;

  if (mCanvas->getDevice()->config() == SkBitmap::kRGB_565_Config) {
    // Set the xfermode to SOURCE_OVER to workaround
    // http://code.google.com/p/skia/issues/detail?id=628
    // RGB565 is opaque so they're equivalent anyway
    paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
  } else {
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  }

  mCanvas->drawBitmapRect(bitmap.mBitmap, &source, dest, &paint);
  mCanvas->restore();
}

bool
DrawTargetSkia::Init(const IntSize &aSize, SurfaceFormat aFormat)
{
  SkAutoTUnref<SkBaseDevice> device(new SkBitmapDevice(GfxFormatToSkiaConfig(aFormat),
                                                       aSize.width, aSize.height,
                                                       aFormat == SurfaceFormat::B8G8R8X8));

  SkBitmap bitmap = device->accessBitmap(true);
  if (!bitmap.allocPixels()) {
    return false;
  }

  bitmap.eraseARGB(0, 0, 0, 0);

  SkAutoTUnref<SkCanvas> canvas(new SkCanvas(device.get()));
  mSize = aSize;

  mCanvas = canvas.get();
  mFormat = aFormat;
  return true;
}

#ifdef USE_SKIA_GPU
void
DrawTargetSkia::InitWithGrContext(GrContext* aGrContext,
                                  const IntSize &aSize,
                                  SurfaceFormat aFormat)
{
  mGrContext = aGrContext;

  mSize = aSize;
  mFormat = aFormat;

  GrTextureDesc targetDescriptor;

  targetDescriptor.fFlags = kRenderTarget_GrTextureFlagBit;
  targetDescriptor.fWidth = mSize.width;
  targetDescriptor.fHeight = mSize.height;
  targetDescriptor.fConfig = GfxFormatToGrConfig(mFormat);
  targetDescriptor.fOrigin = kBottomLeft_GrSurfaceOrigin;
  targetDescriptor.fSampleCnt = 0;

  SkAutoTUnref<GrTexture> skiaTexture(mGrContext->createUncachedTexture(targetDescriptor, NULL, 0));

  mTexture = (uint32_t)skiaTexture->getTextureHandle();

  SkAutoTUnref<SkBaseDevice> device(new SkGpuDevice(mGrContext.get(), skiaTexture->asRenderTarget()));
  SkAutoTUnref<SkCanvas> canvas(new SkCanvas(device.get()));
  mCanvas = canvas.get();
}

#endif

void
DrawTargetSkia::Init(unsigned char* aData, const IntSize &aSize, int32_t aStride, SurfaceFormat aFormat)
{
  SkAlphaType alphaType = kPremul_SkAlphaType;
  if (aFormat == SurfaceFormat::B8G8R8X8) {
    // We have to manually set the A channel to be 255 as Skia doesn't understand BGRX
    ConvertBGRXToBGRA(aData, aSize, aStride);
    alphaType = kOpaque_SkAlphaType;
  }

  SkBitmap bitmap;
  bitmap.setConfig(GfxFormatToSkiaConfig(aFormat), aSize.width, aSize.height, aStride, alphaType);
  bitmap.setPixels(aData);
  SkAutoTUnref<SkCanvas> canvas(new SkCanvas(new SkBitmapDevice(bitmap)));

  mSize = aSize;
  mCanvas = canvas.get();
  mFormat = aFormat;
}

void
DrawTargetSkia::SetTransform(const Matrix& aTransform)
{
  SkMatrix mat;
  GfxMatrixToSkiaMatrix(aTransform, mat);
  mCanvas->setMatrix(mat);
  mTransform = aTransform;
}

void*
DrawTargetSkia::GetNativeSurface(NativeSurfaceType aType)
{
  if (aType == NativeSurfaceType::OPENGL_TEXTURE) {
    return (void*)((uintptr_t)mTexture);
  }

  return nullptr;  
}


TemporaryRef<PathBuilder> 
DrawTargetSkia::CreatePathBuilder(FillRule aFillRule) const
{
  RefPtr<PathBuilderSkia> pb = new PathBuilderSkia(aFillRule);
  return pb;
}

void
DrawTargetSkia::ClearRect(const Rect &aRect)
{
  MarkChanged();
  SkPaint paint;
  mCanvas->save();
  mCanvas->clipRect(RectToSkRect(aRect), SkRegion::kIntersect_Op, true);
  paint.setColor(SkColorSetARGB(0, 0, 0, 0));
  paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  mCanvas->drawPaint(paint);
  mCanvas->restore();
}

void
DrawTargetSkia::PushClip(const Path *aPath)
{
  if (aPath->GetBackendType() != BackendType::SKIA) {
    return;
  }

  const PathSkia *skiaPath = static_cast<const PathSkia*>(aPath);
  mCanvas->save(SkCanvas::kClip_SaveFlag);
  mCanvas->clipPath(skiaPath->GetPath(), SkRegion::kIntersect_Op, true);
}

void
DrawTargetSkia::PushClipRect(const Rect& aRect)
{
  SkRect rect = RectToSkRect(aRect);

  mCanvas->save(SkCanvas::kClip_SaveFlag);
  mCanvas->clipRect(rect, SkRegion::kIntersect_Op, true);
}

void
DrawTargetSkia::PopClip()
{
  mCanvas->restore();
}

TemporaryRef<GradientStops>
DrawTargetSkia::CreateGradientStops(GradientStop *aStops, uint32_t aNumStops, ExtendMode aExtendMode) const
{
  std::vector<GradientStop> stops;
  stops.resize(aNumStops);
  for (uint32_t i = 0; i < aNumStops; i++) {
    stops[i] = aStops[i];
  }
  std::stable_sort(stops.begin(), stops.end());
  
  return new GradientStopsSkia(stops, aNumStops, aExtendMode);
}

TemporaryRef<FilterNode>
DrawTargetSkia::CreateFilter(FilterType aType)
{
  return FilterNodeSoftware::Create(aType);
}

void
DrawTargetSkia::MarkChanged()
{
  if (mSnapshot) {
    mSnapshot->DrawTargetWillChange();
    mSnapshot = nullptr;
  }
}

void
DrawTargetSkia::SnapshotDestroyed()
{
  mSnapshot = nullptr;
}

}
}
