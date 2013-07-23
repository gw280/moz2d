/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_DRAWTARGETNVPR_H_
#define MOZILLA_GFX_DRAWTARGETNVPR_H_

#include "2D.h"
#include "nvpr/GL.h"
#include <mozilla/RefPtr.h>
#include <mozilla/WeakPtr.h>
#include <deque>
#include <string>
#include <stack>

namespace mozilla {
namespace gfx {

class PathNVpr;
class SourceSurfaceNVpr;
class TextureObjectNVpr;

#ifdef WIN32
class DXTextureInteropNVpr;
#endif

namespace nvpr {
class ScissorClip;
class PlanesClip;
class StencilClip;
}

class DrawTargetNVpr
  : public DrawTarget
  , public SupportsWeakPtr<DrawTargetNVpr>
{
public:
  static TemporaryRef<DrawTargetNVpr> Create(const IntSize& aSize,
                                             SurfaceFormat aFormat)
  {
    bool success;
    RefPtr<DrawTargetNVpr> drawTarget = new DrawTargetNVpr(aSize, aFormat, success);
    return success ? drawTarget.forget() : nullptr;
  }
  virtual ~DrawTargetNVpr();

  virtual BackendType GetType() const { return BACKEND_NVPR; }

  virtual IntSize GetSize() { return mSize; }

  virtual TemporaryRef<SourceSurface> Snapshot();
  void OnSnapshotDeleted(TemporaryRef<TextureObjectNVpr> aTexture);

  bool BlitToForeignTexture(void* aForeignContext, GLuint aForeignTextureId);

#ifdef WIN32
  TemporaryRef<DXTextureInteropNVpr>
    OpenDXTextureInterop(void* aDX, void* aDXTexture);
  void BlitToDXTexture(DXTextureInteropNVpr* aDXTexture);
#endif

  virtual void Flush();

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

private:
  DrawTargetNVpr(const IntSize& aSize, SurfaceFormat aFormat, bool& aSuccess);

  enum ValidationFlag {
    FRAMEBUFFER = 1 << 0,
    CLIPPING = 1 << 1,
    TRANSFORM = 1 << 2,
    COLOR_WRITES_ENABLED = 1 << 3
  };
  typedef unsigned ValidationFlags;
  void Validate(ValidationFlags aFlags = ~0);

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
  size_t mSnapshotTextureCount;
  std::deque<RefPtr<TextureObjectNVpr> > mSnapshotTexturePool;
  RefPtr<SourceSurfaceNVpr> mSnapshot;
  RefPtr<PathNVpr> mUnitSquarePath;
  enum ClipType { SCISSOR_CLIP_TYPE, PLANES_CLIP_TYPE, STENCIL_CLIP_TYPE };
  std::stack<ClipType> mClipTypeStack;
  RefPtr<nvpr::ScissorClip> mTopScissorClip;
  RefPtr<nvpr::PlanesClip> mTopPlanesClip;
  RefPtr<nvpr::StencilClip> mTopStencilClip;
  nvpr::StencilClip* mPoppedStencilClips;
  nvpr::UniqueId mTransformId;
  GLubyte mStencilClipBits;
};

}
}

#endif /* MOZILLA_GFX_DRAWTARGETNVPR_H_ */
