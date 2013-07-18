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

  GL::InitializeIfNeeded();
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
DrawTargetNVpr::BlitToForeignTexture(PlatformGLContext aForeignContext,
                                     GLuint aForeignTextureId)
{
  Snapshot();
  return gl->BlitTextureToForeignTexture(mSize, *mSnapshot,
                                         aForeignContext, aForeignTextureId);
}

void
DrawTargetNVpr::DrawSurface(SourceSurface* aSurface,
                            const Rect& aDest,
                            const Rect& aSource,
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

  gl->SetColorToAlpha(aOptions.mAlpha);
  gl->EnableTexturing(GL_TEXTURE_2D, *surface, GL::TEXGEN_NONE);
  gl->DisableShading();
  ApplyDrawOptions(aOptions.mCompositionOp, aOptions.mAntialiasMode,
                   aOptions.mSnapping);

  surface->ApplyTexturingOptions(aSurfOptions.mFilter, EXTEND_CLAMP,
                                 aSurfOptions.mSamplingBounds);

  Rect textureRect = aSource;
  textureRect.ScaleInverse(surface->GetSize().width, surface->GetSize().height);
  const GLfloat texCoords[] = {
    textureRect.x, textureRect.y,
    textureRect.x + textureRect.width, textureRect.y,
    textureRect.x + textureRect.width, textureRect.y + textureRect.height,
    textureRect.x, textureRect.y + textureRect.height
  };
  gl->TexCoordPointer(2, GL_FLOAT, 0, texCoords);

  const GLfloat vertices[] = {
    aDest.x, aDest.y,
    aDest.x + aDest.width, aDest.y,
    aDest.x + aDest.width, aDest.y + aDest.height,
    aDest.x, aDest.y + aDest.height
  };
  gl->VertexPointer(2, GL_FLOAT, 0, vertices);

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

  ApplyPattern(aPattern, aOptions);
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
  ApplyPattern(aPattern, aOptions);
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

  const GLubyte countingMask
    = path->GetFillRule() == FILL_WINDING ? (~mStencilClipBits & 0xff) : 0x1;

  gl->ConfigurePathStencilTest(mStencilClipBits);
  gl->StencilFillPathNV(*path, GL_COUNT_UP_NV, countingMask);

  gl->EnableStencilTest(GL::PASS_IF_NOT_ZERO, countingMask,
                        GL::CLEAR_PASSING_VALUES, countingMask);
  ApplyPattern(aPattern, aOptions);
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
  ApplyPattern(aPattern, aOptions);
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
  gfxWarning() << *this << ": Mask not implemented";

  MarkChanged();
}

void
DrawTargetNVpr::MaskSurface(const Pattern &aSource,
                            SourceSurface *aMask,
                            Point aOffset,
                            const DrawOptions &aOptions)
{
  gl->MakeCurrent();

  Validate();
  gfxWarning() << *this << ": MaskSurface not implemented";

  MarkChanged();
}

void
DrawTargetNVpr::PushClip(const Path* aPath)
{
  MOZ_ASSERT(aPath->GetBackendType() == BACKEND_NVPR);

  const PathNVpr* const path = static_cast<const PathNVpr*>(aPath);

  if (!path->Polygon().IsEmpty()) {
    if (RefPtr<PlanesClip> planesClip
          = PlanesClip::Create(this, mTopPlanesClip, GetTransform(),
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
  if (RefPtr<ScissorClip> scissorClip
        = ScissorClip::Create(this, mTopScissorClip, GetTransform(), aRect)) {
    mTopScissorClip = scissorClip.forget();
    mClipTypeStack.push(SCISSOR_CLIP_TYPE);
    return;
  }

  if (RefPtr<PlanesClip> planesClip
        = PlanesClip::Create(this, mTopPlanesClip, GetTransform(),
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
 RefPtr<TextureObjectNVpr> texture
   = TextureObjectNVpr::Create(aFormat, aSize, aData, aStride);

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
DrawTargetNVpr::ApplyPattern(const Pattern& aPattern,
                             const DrawOptions& aOptions)
{
  MOZ_ASSERT(IsGLCurrent());

  switch (aPattern.GetType()) {
    default:
      MOZ_ASSERT(!"Invalid pattern type");
      break;
    case PATTERN_COLOR:
      ApplyPattern(static_cast<const ColorPattern&>(aPattern),
                   aOptions.mAlpha);
      break;
    case PATTERN_SURFACE:
      ApplyPattern(static_cast<const SurfacePattern&>(aPattern),
                   aOptions.mAlpha);
      break;
    case PATTERN_LINEAR_GRADIENT:
      ApplyPattern(static_cast<const LinearGradientPattern&>(aPattern),
                   aOptions.mAlpha);
      break;
    case PATTERN_RADIAL_GRADIENT:
      ApplyPattern(static_cast<const RadialGradientPattern&>(aPattern),
                   aOptions.mAlpha);
      break;
  }

  ApplyDrawOptions(aOptions.mCompositionOp, aOptions.mAntialiasMode,
                   aOptions.mSnapping);
}

void
DrawTargetNVpr::ApplyPattern(const ColorPattern& aPattern, float aAlpha)
{
  MOZ_ASSERT(gl->IsCurrent());

  gl->SetColor(aPattern.mColor, aAlpha);
  gl->DisableTexturing();
  gl->DisableShading();
}

void
DrawTargetNVpr::ApplyPattern(const SurfacePattern& aPattern, float aAlpha)
{
  MOZ_ASSERT(aSurface->GetType() == SURFACE_NVPR_TEXTURE);

  SourceSurfaceNVpr* const surface
    = static_cast<SourceSurfaceNVpr*>(aPattern.mSurface.get());

  MOZ_ASSERT(gl->IsCurrent());

  Matrix textureCoords = aPattern.mMatrix;
  textureCoords.Invert();
  textureCoords.PostScale(1.0f / surface->GetSize().width,
                          1.0f / surface->GetSize().height);

  gl->SetColorToAlpha(aAlpha);
  gl->EnableTexturing(GL_TEXTURE_2D, *surface, GL::TEXGEN_ST, textureCoords);
  gl->DisableShading();

  surface->ApplyTexturingOptions(aPattern.mFilter, aPattern.mExtendMode);
}

void
DrawTargetNVpr::ApplyPattern(const LinearGradientPattern& aPattern, float aAlpha)
{
  MOZ_ASSERT(IsGLCurrent());
  MOZ_ASSERT(aPattern.mStops->GetBackendType() == BACKEND_NVPR);

  const GradientStopsNVpr* const stops
    = static_cast<const GradientStopsNVpr*>(aPattern.mStops.get());

  // TODO: Are mBegin/mEnd not necessarily in user space?

  stops->ApplyLinearGradient(aPattern.mBegin, aPattern.mEnd, aAlpha);
}

void
DrawTargetNVpr::ApplyPattern(const RadialGradientPattern& aPattern, float aAlpha)
{
  MOZ_ASSERT(IsGLCurrent());
  MOZ_ASSERT(aPattern.mStops->GetBackendType() == BACKEND_NVPR);

  const GradientStopsNVpr* const stops
    = static_cast<const GradientStopsNVpr*>(aPattern.mStops.get());

  // TODO: Are mBegin/mEnd not necessarily in user space?

  if (aPattern.mRadius1 == 0) {
    stops->ApplyFocalGradient(aPattern.mCenter2, aPattern.mRadius2,
                              aPattern.mCenter1, aAlpha);
    return;
  }

  stops->ApplyRadialGradient(aPattern.mCenter1, aPattern.mRadius1,
                             aPattern.mCenter2, aPattern.mRadius2, aAlpha);
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
