/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawTargetNVpr.h"
#include "GradientStopsNVpr.h"
#include "Logging.h"
#include "PathBuilderNVpr.h"
#include "PathNVpr.h"
#include "ScaledFontNVpr.h"
#include "SourceSurfaceNVpr.h"
#include "nvpr/Clip.h"
#include <sstream>
#include <vector>

#ifdef WIN32
#include "DXTextureInteropNVpr.h"
#endif

static const size_t sMaxSnapshotTexturePoolSize = 2;

using namespace mozilla::gfx::nvpr;
using namespace std;

namespace mozilla {
namespace gfx {

inline static ostream&
operator <<(ostream& aStream, const DrawTargetNVpr& aDrawTarget)
{
  aStream << "DrawTargetNVpr(" << &aDrawTarget << ")";
  return aStream;
}

class SnapshotNVpr : public SourceSurfaceNVpr {
public:
  SnapshotNVpr(WeakPtr<DrawTargetNVpr> aDrawTarget,
               TemporaryRef<TextureObjectNVpr> aTexture)
    : SourceSurfaceNVpr(aTexture)
    , mDrawTarget(aDrawTarget)
  {}

  virtual ~SnapshotNVpr()
  {
    if (mDrawTarget) {
      mDrawTarget->OnSnapshotDeleted(Texture());
    }
  }

private:
  WeakPtr<DrawTargetNVpr> mDrawTarget;
};

DrawTargetNVpr::DrawTargetNVpr(const IntSize& aSize, SurfaceFormat aFormat,
                               bool& aSuccess)
  : mSize(aSize)
  , mFormat(aFormat)
  , mColorBuffer(0)
  , mStencilBuffer(0)
  , mFramebuffer(0)
  , mSnapshotTextureCount(0)
  , mPoppedStencilClips(nullptr)
  , mTransformId(0)
  , mStencilClipBits(0)
{
  aSuccess = false;

  MOZ_ASSERT(mSize.width >= 0 && mSize.height >= 0);

  InitializeGLIfNeeded();
  if (!gl->IsValid()) {
    return;
  }

  gl->MakeCurrent();

  if (!gl->HasExtension(GL::EXT_direct_state_access)
      || !gl->HasExtension(GL::NV_path_rendering)
      || !gl->HasExtension(GL::EXT_framebuffer_multisample)
      || !gl->HasExtension(GL::EXT_framebuffer_blit)) {
    return;
  }

  if (max(mSize.width, mSize.height) > gl->MaxRenderbufferSize()
      || max(mSize.width, mSize.height) > gl->MaxTextureSize()) {
    return;
  }

  GLenum colorBufferFormat;
  switch (mFormat) {
    case FORMAT_A8:
    case FORMAT_YUV:
    case FORMAT_UNKNOWN:
    default:
      return;
    case FORMAT_B8G8R8A8:
    case FORMAT_R8G8B8A8:
      colorBufferFormat = GL_RGBA8;
      break;
    case FORMAT_B8G8R8X8:
    case FORMAT_R8G8B8X8:
      colorBufferFormat = GL_RGB8;
      break;
    case FORMAT_R5G6B5:
      colorBufferFormat = GL_RGB565;
      break;
  }
  gl->GenRenderbuffers(1, &mColorBuffer);
  gl->NamedRenderbufferStorageMultisampleEXT(mColorBuffer, 16, colorBufferFormat,
                                             mSize.width, mSize.height);

  gl->GenRenderbuffers(1, &mStencilBuffer);
  gl->NamedRenderbufferStorageMultisampleEXT(mStencilBuffer, 16, GL_STENCIL_INDEX8,
                                             mSize.width, mSize.height);

  gl->GenFramebuffers(1, &mFramebuffer);
  gl->NamedFramebufferRenderbufferEXT(mFramebuffer, GL_COLOR_ATTACHMENT0,
                                      GL_RENDERBUFFER, mColorBuffer);
  gl->NamedFramebufferRenderbufferEXT(mFramebuffer, GL_STENCIL_ATTACHMENT,
                                      GL_RENDERBUFFER, mStencilBuffer);

  Validate(FRAMEBUFFER | COLOR_WRITES_ENABLED);

  gl->DisableScissorTest();
  gl->SetClearColor(Color());
  gl->Clear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  aSuccess = true;
}

DrawTargetNVpr::~DrawTargetNVpr()
{
  gl->MakeCurrent();
  gl->DeleteRenderbuffers(1, &mColorBuffer);
  gl->DeleteRenderbuffers(1, &mStencilBuffer);
  gl->DeleteFramebuffers(1, &mFramebuffer);
}

TemporaryRef<SourceSurface>
DrawTargetNVpr::Snapshot()
{
  if (!mSnapshot) {
    RefPtr<TextureObjectNVpr> texture;
    if (!mSnapshotTexturePool.empty()) {
      texture = mSnapshotTexturePool.front();
      mSnapshotTexturePool.pop_front();
    } else {
      texture = TextureObjectNVpr::Create(mFormat, mSize);
      mSnapshotTextureCount++;
    }

    gl->MakeCurrent();

    gl->SetFramebuffer(GL_READ_FRAMEBUFFER, mFramebuffer);
    gl->SetFramebufferToTexture(GL_DRAW_FRAMEBUFFER, GL_TEXTURE_2D, *texture);
    gl->DisableScissorTest();
    gl->EnableColorWrites();

    gl->BlitFramebuffer(0, 0, mSize.width, mSize.height,
                        0, 0, mSize.width, mSize.height,
                        GL_COLOR_BUFFER_BIT, GL_NEAREST);

    mSnapshot = new SnapshotNVpr(this->asWeakPtr(), texture.forget());
  }

  return mSnapshot;
}
void
DrawTargetNVpr::OnSnapshotDeleted(TemporaryRef<TextureObjectNVpr> aTexture)
{
  if (mSnapshotTextureCount > sMaxSnapshotTexturePoolSize) {
    mSnapshotTextureCount--;
    return;
  }

  mSnapshotTexturePool.push_back(aTexture);
}

bool
DrawTargetNVpr::BlitToForeignTexture(void* aForeignContext,
                                     GLuint aForeignTextureId)
{
  Snapshot();
  return gl->BlitTextureToForeignTexture(mSize, *mSnapshot,
                                         aForeignContext, aForeignTextureId);
}

#ifdef WIN32

TemporaryRef<DXTextureInteropNVpr>
DrawTargetNVpr::OpenDXTextureInterop(void* aDX, void* aDXTexture)
{
  return DXTextureInteropNVpr::Create(aDX, aDXTexture);
}

void
DrawTargetNVpr::BlitToDXTexture(DXTextureInteropNVpr* aDXTexture)
{
  gl->MakeCurrent();

  GLuint dxTextureId = aDXTexture->Lock();

  gl->SetFramebuffer(GL_READ_FRAMEBUFFER, mFramebuffer);
  gl->SetFramebufferToTexture(GL_DRAW_FRAMEBUFFER, GL_TEXTURE_2D, dxTextureId);
  gl->DisableScissorTest();
  gl->EnableColorWrites();

  gl->BlitFramebuffer(0, 0, mSize.width, mSize.height,
                      0, 0, mSize.width, mSize.height,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);

  aDXTexture->Unlock();
}

#endif

void
DrawTargetNVpr::Flush()
{
  gl->MakeCurrent();
  gl->Flush();
}

void
DrawTargetNVpr::DrawSurface(SourceSurface* aSurface,
                            const Rect& aDestRect,
                            const Rect& aSourceRect,
                            const DrawSurfaceOptions& aSurfOptions,
                            const DrawOptions& aOptions)
{
  MOZ_ASSERT(aSurface->GetType() == SURFACE_NVPR_TEXTURE);

  SourceSurfaceNVpr* const surface = static_cast<SourceSurfaceNVpr*>(aSurface);

  gl->MakeCurrent();

  Validate();

  if (mStencilClipBits) {
    gl->EnableStencilTest(GL::PASS_IF_ALL_SET, mStencilClipBits,
                          GL::LEAVE_UNCHANGED);
  } else {
    gl->DisableStencilTest();
  }

  GL::ShaderConfig shaderConfig;
  shaderConfig.mGlobalAlpha = aOptions.mAlpha;
  if (aSurfOptions.mSamplingBounds == SAMPLING_UNBOUNDED) {
    shaderConfig.mPaintConfig.SetToSurface(surface, aSurfOptions.mFilter);
  } else {
    shaderConfig.mPaintConfig.SetToSurface(surface, aSourceRect,
                                           aSurfOptions.mFilter);
  }
  gl->EnableShading(shaderConfig);

  ApplyDrawOptions(aOptions.mCompositionOp, aOptions.mAntialiasMode,
                   aOptions.mSnapping);

  Rect textureRect = aSourceRect;
  textureRect.ScaleInverse(surface->GetSize().width, surface->GetSize().height);
  const GLfloat texCoords[] = {textureRect.x, textureRect.y,
                               textureRect.XMost(), textureRect.y,
                               textureRect.XMost(), textureRect.YMost(),
                               textureRect.x, textureRect.YMost()};
  gl->EnableTexCoordArray(GL::PAINT_UNIT, texCoords);
  gl->DisableTexCoordArray(GL::MASK_UNIT);

  const GLfloat vertices[] = {aDestRect.x, aDestRect.y,
                              aDestRect.XMost(), aDestRect.y,
                              aDestRect.XMost(), aDestRect.YMost(),
                              aDestRect.x, aDestRect.YMost()};
  gl->SetVertexArray(vertices);

  gl->DrawArrays(GL_QUADS, 0, 4);

  MarkChanged();
}

void
DrawTargetNVpr::DrawSurfaceWithShadow(SourceSurface* aSurface,
                                      const Point& aDest,
                                      const Color& aColor,
                                      const Point& aOffset,
                                      Float aSigma,
                                      CompositionOp aOperator)
{
  MOZ_ASSERT(aSurface->GetType() == SURFACE_NVPR_TEXTURE);

  SourceSurfaceNVpr* const surface = static_cast<SourceSurfaceNVpr*>(aSurface);

  gl->MakeCurrent();

  Validate();

  gfxWarning() << *this << ": DrawSurfaceWithShadow not implemented";

  MarkChanged();
}

void
DrawTargetNVpr::ClearRect(const Rect& aRect)
{
  FillRect(aRect, ColorPattern(Color()), DrawOptions(1, OP_SOURCE));
}

void
DrawTargetNVpr::CopySurface(SourceSurface* aSurface,
                            const IntRect& aSourceRect,
                            const IntPoint& aDestination)
{
  MOZ_ASSERT(aSurface->GetType() == SURFACE_NVPR_TEXTURE);

  SourceSurfaceNVpr* const surface = static_cast<SourceSurfaceNVpr*>(aSurface);

  gl->MakeCurrent();

  // TODO: Consider using NV_draw_texture instead.

  gl->SetFramebufferToTexture(GL_READ_FRAMEBUFFER, GL_TEXTURE_2D, *surface);
  gl->SetFramebuffer(GL_DRAW_FRAMEBUFFER, mFramebuffer);
  gl->DisableScissorTest();
  gl->EnableColorWrites();

  gl->BlitFramebuffer(aSourceRect.x, aSourceRect.y, aSourceRect.XMost(),
                      aSourceRect.YMost(), aDestination.x, aDestination.y,
                      aDestination.x + aSourceRect.width, aDestination.y
                      + aSourceRect.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
}

void
DrawTargetNVpr::FillRect(const Rect& aRect,
                         const Pattern& aPattern,
                         const DrawOptions& aOptions)
{
  gl->MakeCurrent();

  if (aPattern.GetType() == PATTERN_COLOR) {
    const Color& color = static_cast<const ColorPattern&>(aPattern).mColor;
    const bool needsBlending = aOptions.mCompositionOp != OP_SOURCE
                               && (aOptions.mCompositionOp != OP_OVER
                                   || aOptions.mAlpha != 1 || color.a != 1);
    const bool hasComplexClips = mTopPlanesClip || (mTopStencilClip
                                   && (!mPoppedStencilClips
                                       || mPoppedStencilClips->GetPrevious()));

    IntRect scissorRect;
    if (!needsBlending && !hasComplexClips && GetTransform().IsRectilinear()
        && GetTransform().TransformBounds(aRect).ToIntRect(&scissorRect)) {

      Validate(FRAMEBUFFER | COLOR_WRITES_ENABLED);

      if (mTopScissorClip) {
        scissorRect.IntersectRect(scissorRect, mTopScissorClip->ScissorRect());
      }
      gl->EnableScissorTest(scissorRect);
      gl->SetClearColor(color, aOptions.mAlpha);
      gl->Clear(GL_COLOR_BUFFER_BIT);

      MarkChanged();
      return;
    }
  }

  Validate();

  if (mStencilClipBits) {
    gl->EnableStencilTest(GL::PASS_IF_ALL_SET, mStencilClipBits,
                          GL::LEAVE_UNCHANGED);
  } else {
    gl->DisableStencilTest();
  }

  GL::ShaderConfig shaderConfig;
  shaderConfig.mGlobalAlpha = aOptions.mAlpha;
  shaderConfig.mPaintConfig.SetToPattern(aPattern);
  gl->EnableShading(shaderConfig);

  ApplyDrawOptions(aOptions.mCompositionOp, aOptions.mAntialiasMode,
                   aOptions.mSnapping);

  gl->Rectf(aRect.x, aRect.y, aRect.x + aRect.width, aRect.y + aRect.height);

  MarkChanged();
}

void
DrawTargetNVpr::StrokeRect(const Rect& aRect,
                           const Pattern& aPattern,
                           const StrokeOptions& aStrokeOptions,
                           const DrawOptions& aOptions)
{
  PathBuilderNVpr pathBuilder(FILL_WINDING);
  pathBuilder.MoveTo(aRect.BottomRight());
  pathBuilder.LineTo(aRect.TopRight());
  pathBuilder.LineTo(aRect.TopLeft());
  pathBuilder.LineTo(aRect.BottomLeft());
  pathBuilder.Close();
  RefPtr<Path> path = pathBuilder.Finish();

  Stroke(path.get(), aPattern, aStrokeOptions, aOptions);
}

void
DrawTargetNVpr::StrokeLine(const Point& aStart,
                           const Point& aEnd,
                           const Pattern& aPattern,
                           const StrokeOptions& aStrokeOptions,
                           const DrawOptions& aOptions)
{
  PathBuilderNVpr pathBuilder(FILL_WINDING);
  pathBuilder.MoveTo(aStart);
  pathBuilder.LineTo(aEnd);
  RefPtr<Path> path = pathBuilder.Finish();

  Stroke(path.get(), aPattern, aStrokeOptions, aOptions);
}

void
DrawTargetNVpr::Stroke(const Path* aPath,
                       const Pattern& aPattern,
                       const StrokeOptions& aStrokeOptions,
                       const DrawOptions& aOptions)
{
  MOZ_ASSERT(aPath->GetBackendType() == BACKEND_NVPR);

  const PathNVpr* const path = static_cast<const PathNVpr*>(aPath);

  gl->MakeCurrent();

  Validate();

  gl->ConfigurePathStencilTest(mStencilClipBits);
  path->ApplyStrokeOptions(aStrokeOptions);
  gl->StencilStrokePathNV(*path, 0x1, 0x1);

  gl->EnableStencilTest(GL::PASS_IF_NOT_ZERO, 1, GL::CLEAR_PASSING_VALUES, 1);

  GL::ShaderConfig shaderConfig;
  shaderConfig.mGlobalAlpha = aOptions.mAlpha;
  shaderConfig.mPaintConfig.SetToPattern(aPattern);
  gl->EnableShading(shaderConfig);

  ApplyDrawOptions(aOptions.mCompositionOp, aOptions.mAntialiasMode,
                   aOptions.mSnapping);

  gl->CoverStrokePathNV(*path, GL_BOUNDING_BOX_NV);

  MarkChanged();
}

void
DrawTargetNVpr::Fill(const Path* aPath,
                     const Pattern& aPattern,
                     const DrawOptions& aOptions)
{
  MOZ_ASSERT(aPath->GetBackendType() == BACKEND_NVPR);

  const PathNVpr* const path = static_cast<const PathNVpr*>(aPath);

  gl->MakeCurrent();

  Validate();

  const GLubyte countingMask =
    path->GetFillRule() == FILL_WINDING ? (~mStencilClipBits & 0xff) : 0x1;

  gl->ConfigurePathStencilTest(mStencilClipBits);
  gl->StencilFillPathNV(*path, GL_COUNT_UP_NV, countingMask);

  gl->EnableStencilTest(GL::PASS_IF_NOT_ZERO, countingMask,
                        GL::CLEAR_PASSING_VALUES, countingMask);

  GL::ShaderConfig shaderConfig;
  shaderConfig.mGlobalAlpha = aOptions.mAlpha;
  shaderConfig.mPaintConfig.SetToPattern(aPattern);
  gl->EnableShading(shaderConfig);

  ApplyDrawOptions(aOptions.mCompositionOp, aOptions.mAntialiasMode,
                   aOptions.mSnapping);

  gl->CoverFillPathNV(*path, GL_BOUNDING_BOX_NV);

  MarkChanged();
}

void
DrawTargetNVpr::FillGlyphs(ScaledFont* aFont,
                           const GlyphBuffer& aBuffer,
                           const Pattern& aPattern,
                           const DrawOptions& aOptions,
                           const GlyphRenderingOptions* aRenderOptions)
{
  MOZ_ASSERT(aFont->GetType() == FONT_NVPR);

  if (!aBuffer.mNumGlyphs) {
    return;
  }

  const ScaledFontNVpr* const font = static_cast<const ScaledFontNVpr*>(aFont);

  gl->MakeCurrent();

  Validate();

  const GLubyte countingMask = (~mStencilClipBits & 0xff);

  {
    Matrix transform = GetTransform();
    transform.Scale(font->Size(), -font->Size());
    GL::ScopedPushTransform pushTransform(gl, transform);

    struct Position {GLfloat x, y;};
    vector<GLuint> characters(aBuffer.mNumGlyphs);
    vector<Position> positions(aBuffer.mNumGlyphs);

    for (size_t i = 0; i < aBuffer.mNumGlyphs; i++) {
      // TODO: How can we know the real mapping index -> unicode?
      characters[i] = aBuffer.mGlyphs[i].mIndex + 29;
      positions[i].x = aBuffer.mGlyphs[i].mPosition.x * font->InverseSize();
      positions[i].y = aBuffer.mGlyphs[i].mPosition.y * -font->InverseSize();
    }

    gl->ConfigurePathStencilTest(mStencilClipBits);
    gl->StencilFillPathInstancedNV(aBuffer.mNumGlyphs, GL_UNSIGNED_INT,
                                   &characters.front(), *font, GL_COUNT_UP_NV,
                                   countingMask, GL_TRANSLATE_2D_NV,
                                   &positions[0].x);
  }

  const Rect& glyphBounds = font->GlyphsBoundingBox();
  Point minPoint = aBuffer.mGlyphs[0].mPosition;
  Point maxPoint = aBuffer.mGlyphs[0].mPosition;

  for (size_t i = 1; i < aBuffer.mNumGlyphs; i++) {
    const Point& pt = aBuffer.mGlyphs[i].mPosition;
    minPoint = Point(min(minPoint.x, pt.x), min(minPoint.y, pt.y));
    maxPoint = Point(max(maxPoint.x, pt.x), max(maxPoint.y, pt.y));
  }

  gl->EnableStencilTest(GL::PASS_IF_NOT_ZERO, countingMask,
                        GL::CLEAR_PASSING_VALUES, countingMask);

  GL::ShaderConfig shaderConfig;
  shaderConfig.mGlobalAlpha = aOptions.mAlpha;
  shaderConfig.mPaintConfig.SetToPattern(aPattern);
  gl->EnableShading(shaderConfig);

  ApplyDrawOptions(aOptions.mCompositionOp, aOptions.mAntialiasMode,
                   aOptions.mSnapping);

  gl->Rectf(minPoint.x + glyphBounds.x, minPoint.y + glyphBounds.y,
            maxPoint.x + glyphBounds.XMost(), maxPoint.y + glyphBounds.YMost());

  MarkChanged();
}

void
DrawTargetNVpr::Mask(const Pattern& aSource,
                     const Pattern& aMask,
                     const DrawOptions& aOptions)
{
  gl->MakeCurrent();

  Validate();

  if (mStencilClipBits) {
    gl->EnableStencilTest(GL::PASS_IF_ALL_SET, mStencilClipBits,
                          GL::LEAVE_UNCHANGED);
  } else {
    gl->DisableStencilTest();
  }

  GL::ShaderConfig shaderConfig;
  shaderConfig.mGlobalAlpha = aOptions.mAlpha;
  shaderConfig.mPaintConfig.SetToPattern(aSource);
  shaderConfig.mMaskConfig.SetToPattern(aMask);
  gl->EnableShading(shaderConfig);

  ApplyDrawOptions(aOptions.mCompositionOp, aOptions.mAntialiasMode,
                   aOptions.mSnapping);

  Matrix inverse = GetTransform();
  inverse.Invert();
  Point topLeft = inverse * Point(0, 0);
  Point bottomRight = inverse * Point(mSize.width, mSize.height);

  gl->Rectf(topLeft.x, topLeft.y, bottomRight.x, bottomRight.y);

  MarkChanged();
}

void
DrawTargetNVpr::MaskSurface(const Pattern& aSource,
                            SourceSurface* aMask,
                            Point aOffset,
                            const DrawOptions& aOptions)
{
  MOZ_ASSERT(aMask->GetType() == SURFACE_NVPR_TEXTURE);

  SourceSurfaceNVpr* const mask = static_cast<SourceSurfaceNVpr*>(aMask);
  const Rect maskRect(aOffset, Size(mask->GetSize()));

  gl->MakeCurrent();

  Validate();

  if (mStencilClipBits) {
    gl->EnableStencilTest(GL::PASS_IF_ALL_SET, mStencilClipBits,
                          GL::LEAVE_UNCHANGED);
  } else {
    gl->DisableStencilTest();
  }

  GL::ShaderConfig shaderConfig;
  shaderConfig.mGlobalAlpha = aOptions.mAlpha;
  shaderConfig.mPaintConfig.SetToPattern(aSource);
  shaderConfig.mMaskConfig.SetToSurface(mask);
  gl->EnableShading(shaderConfig);

  ApplyDrawOptions(aOptions.mCompositionOp, aOptions.mAntialiasMode,
                   aOptions.mSnapping);

  const GLfloat maskCoords[] = {0, 0, 1, 0, 1, 1, 0, 1};
  gl->DisableTexCoordArray(GL::PAINT_UNIT);
  gl->EnableTexCoordArray(GL::MASK_UNIT, maskCoords);

  const GLfloat vertices[] = {maskRect.x, maskRect.y,
                              maskRect.XMost(), maskRect.y,
                              maskRect.XMost(), maskRect.YMost(),
                              maskRect.x, maskRect.YMost()};
  gl->SetVertexArray(vertices);

  gl->DrawArrays(GL_QUADS, 0, 4);

  MarkChanged();
}

void
DrawTargetNVpr::PushClip(const Path* aPath)
{
  MOZ_ASSERT(aPath->GetBackendType() == BACKEND_NVPR);

  const PathNVpr* const path = static_cast<const PathNVpr*>(aPath);

  if (!path->Polygon().IsEmpty()) {
    if (RefPtr<PlanesClip> planesClip =
          PlanesClip::Create(this, mTopPlanesClip, GetTransform(),
                             ConvexPolygon(path->Polygon()))) {
      mTopPlanesClip = planesClip.forget();
      mClipTypeStack.push(PLANES_CLIP_TYPE);
      return;
    }
  }

  Validate(FRAMEBUFFER | CLIPPING);

  mTopStencilClip = StencilClip::Create(this, mTopStencilClip.forget(),
                                        GetTransform(), mTransformId,
                                        path->Clone());

  mTopStencilClip->ApplyToStencilBuffer();

  mClipTypeStack.push(STENCIL_CLIP_TYPE);
}

void
DrawTargetNVpr::PushClipRect(const Rect& aRect)
{
  if (RefPtr<ScissorClip> scissorClip =
        ScissorClip::Create(this, mTopScissorClip, GetTransform(), aRect)) {
    mTopScissorClip = scissorClip.forget();
    mClipTypeStack.push(SCISSOR_CLIP_TYPE);
    return;
  }

  if (RefPtr<PlanesClip> planesClip =
        PlanesClip::Create(this, mTopPlanesClip, GetTransform(),
                           ConvexPolygon(aRect))) {
    mTopPlanesClip = planesClip.forget();
    mClipTypeStack.push(PLANES_CLIP_TYPE);
    return;
  }

  if (!mUnitSquarePath) {
    PathBuilderNVpr pathBuilder(FILL_WINDING);
    pathBuilder.MoveTo(Point(0, 0));
    pathBuilder.LineTo(Point(1, 0));
    pathBuilder.LineTo(Point(1, 1));
    pathBuilder.LineTo(Point(0, 1));
    RefPtr<Path> path = pathBuilder.Finish();

    mUnitSquarePath = static_cast<PathNVpr*>(path.get());
  }

  Matrix transform = GetTransform();
  transform.Translate(aRect.x, aRect.y);
  transform.Scale(aRect.width, aRect.height);

  Validate(FRAMEBUFFER | CLIPPING);

  mTopStencilClip = StencilClip::Create(this, mTopStencilClip.forget(),
                                        transform, gl->GetUniqueId(),
                                        mUnitSquarePath->Clone());

  mTopStencilClip->ApplyToStencilBuffer();

  mClipTypeStack.push(STENCIL_CLIP_TYPE);
}

void
DrawTargetNVpr::PopClip()
{
  switch (mClipTypeStack.top()) {
    case SCISSOR_CLIP_TYPE:
      mTopScissorClip = mTopScissorClip->Pop();
      break;
    case PLANES_CLIP_TYPE:
      mTopPlanesClip = mTopPlanesClip->Pop();
      break;
    case STENCIL_CLIP_TYPE:
      mPoppedStencilClips = !mPoppedStencilClips ? mTopStencilClip.get()
                                                 : mPoppedStencilClips->GetPrevious();
      break;
    default:
      MOZ_ASSERT(!"Invalid clip type.");
  }

  mClipTypeStack.pop();
}

TemporaryRef<SourceSurface> 
DrawTargetNVpr::CreateSourceSurfaceFromData(unsigned char* aData,
                                            const IntSize& aSize,
                                            int32_t aStride,
                                            SurfaceFormat aFormat) const
{
 RefPtr<TextureObjectNVpr> texture =
   TextureObjectNVpr::Create(aFormat, aSize, aData, aStride);

  return texture ? new SourceSurfaceNVpr(texture.forget()) : nullptr;
}

TemporaryRef<SourceSurface> 
DrawTargetNVpr::OptimizeSourceSurface(SourceSurface* aSurface) const
{
  if (aSurface->GetType() == SURFACE_NVPR_TEXTURE) {
    return aSurface;
  }

  RefPtr<DataSourceSurface> data = aSurface->GetDataSurface();
  RefPtr<TextureObjectNVpr> texture = TextureObjectNVpr::Create(data);

  return texture ? new SourceSurfaceNVpr(texture.forget()) : nullptr;
}

TemporaryRef<SourceSurface>
DrawTargetNVpr::CreateSourceSurfaceFromNativeSurface(const NativeSurface& aSurface) const
{
  gfxWarning() << *this << ": CreateSourceSurfaceFromNativeSurface not implemented";
  return 0;
}

TemporaryRef<DrawTarget>
DrawTargetNVpr::CreateSimilarDrawTarget(const IntSize& aSize,
                                        SurfaceFormat aFormat) const
{
  return Create(aSize, aFormat);
}

TemporaryRef<PathBuilder>
DrawTargetNVpr::CreatePathBuilder(FillRule aFillRule) const
{
  return new PathBuilderNVpr(aFillRule);
}

TemporaryRef<GradientStops>
DrawTargetNVpr::CreateGradientStops(GradientStop* rawStops, uint32_t aNumStops,
                                    ExtendMode aExtendMode) const
{
  return GradientStopsNVpr::create(rawStops, aNumStops, aExtendMode);
}

void*
DrawTargetNVpr::GetNativeSurface(NativeSurfaceType aType)
{
  gfxWarning() << *this << ": GetNativeSurface not implemented";
  return 0;
}

void
DrawTargetNVpr::SetTransform(const Matrix& aTransform)
{
  DrawTarget::SetTransform(aTransform);
  mTransformId = gl->GetUniqueId();
}

void
DrawTargetNVpr::Validate(ValidationFlags aFlags)
{
  MOZ_ASSERT(gl->IsCurrent());

  if (aFlags & FRAMEBUFFER) {
    gl->SetTargetSize(mSize);
    gl->SetFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
  }

  if (aFlags & CLIPPING) {
    if (mTopScissorClip) {
      gl->EnableScissorTest(mTopScissorClip->ScissorRect());
    } else {
      gl->DisableScissorTest();
    }

    if (mTopPlanesClip) {
      if (gl->ClipPolygonId() != mTopPlanesClip->PolygonId()) {
        gl->SetTransformToIdentity();
        gl->EnableClipPlanes(mTopPlanesClip->Polygon(),
                             mTopPlanesClip->PolygonId());
      }
    } else {
      gl->DisableClipPlanes();
    }

    if (mPoppedStencilClips) {
      mPoppedStencilClips->RestoreStencilBuffer();
      mTopStencilClip = mPoppedStencilClips->Pop();
      mPoppedStencilClips = nullptr;
    }
  }

  if (aFlags & TRANSFORM) {
    gl->SetTransform(GetTransform(), mTransformId);
  }

  if (aFlags & COLOR_WRITES_ENABLED) {
    gl->EnableColorWrites();
  }
}

void
DrawTargetNVpr::ApplyDrawOptions(CompositionOp aCompositionOp,
                                 AntialiasMode aAntialiasMode,
                                 Snapping aSnapping)
{
  MOZ_ASSERT(gl->IsCurrent());

  switch (aCompositionOp) {
    case OP_MULTIPLY:
    case OP_SCREEN:
    case OP_OVERLAY:
    case OP_DARKEN:
    case OP_LIGHTEN:
    case OP_COLOR_DODGE:
    case OP_COLOR_BURN:
    case OP_HARD_LIGHT:
    case OP_SOFT_LIGHT:
    case OP_DIFFERENCE:
    case OP_EXCLUSION:
    case OP_HUE:
    case OP_SATURATION:
    case OP_COLOR:
    case OP_LUMINOSITY:
    default:
      // TODO: Use NV_blend_equation_advanced
      MOZ_ASSERT(!"Unsupported composition operation");
    case OP_SOURCE:
      gl->DisableBlending();
      break;
    case OP_OVER:
      gl->EnableBlending(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      break;
    case OP_ADD:
      gl->EnableBlending(GL_ONE, GL_ONE);
      break;
    case OP_ATOP:
      gl->EnableBlending(GL_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                         GL_ZERO, GL_ONE);
      break;
    case OP_OUT:
      gl->EnableBlending(GL_ONE_MINUS_DST_ALPHA, GL_ZERO);
      break;
    case OP_IN:
      gl->EnableBlending(GL_DST_ALPHA, GL_ZERO);
      break;
    case OP_DEST_IN:
      gl->EnableBlending(GL_ZERO, GL_SRC_ALPHA);
      break;
    case OP_DEST_OUT:
      gl->EnableBlending(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
      break;
    case OP_DEST_OVER:
      gl->EnableBlending(GL_ONE_MINUS_DST_ALPHA, GL_ONE);
      break;
    case OP_DEST_ATOP:
      gl->EnableBlending(GL_ONE_MINUS_DST_ALPHA, GL_SRC_ALPHA,
                         GL_ONE, GL_ZERO);
      break;
    case OP_XOR:
      gl->EnableBlending(GL_ONE_MINUS_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      break;
  }
}

void
DrawTargetNVpr::MarkChanged()
{
  mSnapshot = nullptr;
}

GLubyte
DrawTargetNVpr::ReserveStencilClipBit()
{
  // Don't reserve more than two bit planes for clipping.
  if (mStencilClipBits >= 0xc0) {
    return 0;
  }

  mStencilClipBits = 0x80 | (mStencilClipBits >> 1);

  return (~mStencilClipBits & 0xff) + 1;
}

void
DrawTargetNVpr::ReleaseStencilClipBits(GLubyte aBits)
{
  mStencilClipBits &= (~aBits & 0xff);

  // The clip bits need to be a consecutive run of most-significant bits (in
  // other words, they need to be released in reverse order).
  MOZ_ASSERT((~mStencilClipBits & 0xff) & ((~mStencilClipBits & 0xff) - 1));
}

}
}
