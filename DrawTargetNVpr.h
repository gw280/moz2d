/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_DRAWTARGETNVPR_H_
#define MOZILLA_GFX_DRAWTARGETNVPR_H_

#include "2D.h"
#include "GLContextNVpr.h"

namespace mozilla {
namespace gfx {

class ClipNVpr;
class GLContextNVpr;
class PathNVpr;

class DrawTargetNVpr : public DrawTarget
{
public:
  static TemporaryRef<DrawTargetNVpr> Create(const IntSize& aSize,
                                             SurfaceFormat aFormat)
  {
    bool success;
    RefPtr<DrawTargetNVpr> drawTarget = new DrawTargetNVpr(aSize, aFormat, success);
    if (!success) {
      return nullptr;
    }
    return drawTarget.forget();
  }
  virtual ~DrawTargetNVpr();

  virtual BackendType GetType() const { return BACKEND_NVPR; }

  operator std::string() const;

  virtual TemporaryRef<SourceSurface> Snapshot();
  virtual IntSize GetSize() { return mSize; }

  virtual void Flush() {}

  virtual void DrawSurface(SourceSurface* aSurface,
                           const Rect& aDest,
                           const Rect& aSource,
                           const DrawSurfaceOptions& aSurfOptions = DrawSurfaceOptions(),
                           const DrawOptions& aOptions = DrawOptions());

  virtual void DrawSurfaceWithShadow(SourceSurface* aSurface,
                                     const Point& aDest,
                                     const Color& aColor,
                                     const Point& aOffset,
                                     Float aSigma,
                                     CompositionOp aOperator);

  virtual void ClearRect(const Rect& aRect);

  virtual void CopySurface(SourceSurface* aSurface,
                           const IntRect& aSourceRect,
                           const IntPoint& aDestination);

  virtual void FillRect(const Rect& aRect,
                        const Pattern& aPattern,
                        const DrawOptions& aOptions = DrawOptions());

  virtual void StrokeRect(const Rect& aRect,
                          const Pattern& aPattern,
                          const StrokeOptions& aStrokeOptions = StrokeOptions(),
                          const DrawOptions& aOptions = DrawOptions());

  virtual void StrokeLine(const Point& aStart,
                          const Point& aEnd,
                          const Pattern& aPattern,
                          const StrokeOptions& aStrokeOptions = StrokeOptions(),
                          const DrawOptions& aOptions = DrawOptions());

  virtual void Stroke(const Path* aPath,
                      const Pattern& aPattern,
                      const StrokeOptions& aStrokeOptions = StrokeOptions(),
                      const DrawOptions& aOptions = DrawOptions());

  virtual void Fill(const Path* aPath,
                    const Pattern& aPattern,
                    const DrawOptions& aOptions = DrawOptions());

  virtual void FillGlyphs(ScaledFont* aFont,
                          const GlyphBuffer& aBuffer,
                          const Pattern& aPattern,
                          const DrawOptions& aOptions = DrawOptions(),
                          const GlyphRenderingOptions* aRenderingOptions = nullptr);

  virtual void Mask(const Pattern& aSource,
                    const Pattern& aMask,
                    const DrawOptions& aOptions = DrawOptions());

  virtual void MaskSurface(const Pattern &aSource,
                           SourceSurface *aMask,
                           Point aOffset,
                           const DrawOptions &aOptions = DrawOptions());

  virtual void PushClip(const Path* aPath);

  virtual void PushClipRect(const Rect& aRect);

  virtual void PopClip();

  virtual TemporaryRef<SourceSurface>
    CreateSourceSurfaceFromData(unsigned char* aData, const IntSize& aSize,
                                int32_t aStride, SurfaceFormat aFormat) const;

  virtual TemporaryRef<SourceSurface>
    OptimizeSourceSurface(SourceSurface* aSurface) const;

  virtual TemporaryRef<SourceSurface>
    CreateSourceSurfaceFromNativeSurface(const NativeSurface& aSurface) const;
  
  virtual TemporaryRef<DrawTarget>
    CreateSimilarDrawTarget(const IntSize& aSize, SurfaceFormat aFormat) const;

  virtual TemporaryRef<PathBuilder>
    CreatePathBuilder(FillRule aFillRule = FILL_WINDING) const;

  virtual TemporaryRef<GradientStops>
    CreateGradientStops(GradientStop* aStops, uint32_t aNumStops,
                        ExtendMode aExtendMode = EXTEND_CLAMP) const;

  virtual void* GetNativeSurface(NativeSurfaceType aType);

  virtual void SetTransform(const Matrix& aTransform);

  GLubyte ReserveStencilClipBit();
  void ReleaseStencilClipBits(GLubyte aBits);

  GLuint ReserveClipPlanes(size_t count);
  void ReleaseClipPlanes(GLuint aIndex);

private:
  DrawTargetNVpr(const IntSize& aSize, SurfaceFormat aFormat, bool& aSuccess);

  void PushClip(const Matrix& aTransform, const PathNVpr* aPath);
  void Validate();

  void ApplyPattern(const Pattern& aPattern, const DrawOptions& aOptions);
  void ApplyPattern(const ColorPattern& aPattern, float aAlpha);
  void ApplyPattern(const SurfacePattern& aPattern, float aAlpha);
  void ApplyPattern(const LinearGradientPattern& aPattern, float aAlpha);
  void ApplyPattern(const RadialGradientPattern& aPattern, float aAlpha);

  void ApplyDrawOptions(CompositionOp aCompositionOp,
                        AntialiasMode aAntialiasMode, Snapping aSnapping);

  void MarkChanged();

  const IntSize mSize;
  const SurfaceFormat mFormat;
  GLuint mColorBuffer;
  GLuint mStencilBuffer;
  GLuint mFramebuffer;
  RefPtr<SourceSurface> mSnapshot;
  RefPtr<PathNVpr> mUnitSquarePath;
  RefPtr<ClipNVpr> mClip;
  ClipNVpr* mPoppedClips;
  GLubyte mStencilClipBits;
  GLuint mActiveClipPlanes;
};

}
}

#endif /* MOZILLA_GFX_DRAWTARGETNVPR_H_ */
