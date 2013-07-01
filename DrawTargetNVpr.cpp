/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DrawTargetNVpr.h"
#include "ClipNVpr.h"
#include "GLContextNVpr.h"
#include "GradientStopsNVpr.h"
#include "PathBuilderNVpr.h"
#include "PathNVpr.h"
#include "ScaledFontNVpr.h"
#include "SourceSurfaceNVpr.h"
#include <sstream>
#include <vector>

using namespace std;

namespace mozilla {
namespace gfx {

DrawTargetNVpr::DrawTargetNVpr(const IntSize& aSize, SurfaceFormat aFormat,
                               bool& aSuccess)
  : mSize(aSize)
  , mFormat(aFormat)
  , mColorBuffer(0)
  , mStencilBuffer(0)
  , mFramebuffer(0)
  , mPoppedClips(0)
  , mStencilClipBits(0)
  , mActiveClipPlanes(0)
{
  aSuccess = false;

  MOZ_ASSERT(mSize.width >= 0 && mSize.height >= 0);

  GLContextNVpr* const gl = GLContextNVpr::Instance();
  if (!gl->IsValid()) {
    return;
  }

  gl->MakeCurrent();

  if (!gl->HasExtension(GLContextNVpr::EXT_direct_state_access)
      || !gl->HasExtension(GLContextNVpr::NV_path_rendering)
      || !gl->HasExtension(GLContextNVpr::EXT_framebuffer_multisample)
      || !gl->HasExtension(GLContextNVpr::EXT_framebuffer_blit)) {
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

  gl->SetTargetSize(mSize);
  gl->SetFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
  gl->Clear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

  aSuccess = true;
}

DrawTargetNVpr::~DrawTargetNVpr()
{
}

DrawTargetNVpr::operator string() const
{
  stringstream stream;
  stream << "DrawTargetNVpr(" << this << ")";
  return stream.str();
}

TemporaryRef<SourceSurface>
DrawTargetNVpr::Snapshot()
{
  if (!mSnapshot) {
    GLContextNVpr* const gl = GLContextNVpr::Instance();
    gl->MakeCurrent();

    gl->SetFramebuffer(GL_READ_FRAMEBUFFER, mFramebuffer);
    mSnapshot = SourceSurfaceNVpr::CreateFromFramebuffer(mFormat, mSize);
  }
  return mSnapshot;
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

  Rect textureRect = aSource;
  textureRect.ScaleInverse(surface->GetSize().width, surface->GetSize().height);

  GLContextNVpr* const gl = GLContextNVpr::Instance();
  gl->MakeCurrent();

  Validate();

  if (mStencilClipBits) {
    gl->EnableStencilTest(GLContextNVpr::PASS_IF_ALL_SET, mStencilClipBits,
                          GLContextNVpr::LEAVE_UNCHANGED);
  } else {
    gl->DisableStencilTest();
  }

  gl->SetColorToAlpha(aOptions.mAlpha);
  gl->EnableTexturing(GL_TEXTURE_2D, *surface, GLContextNVpr::TEXGEN_NONE);
  gl->DisableShading();
  ApplyDrawOptions(aOptions.mCompositionOp, aOptions.mAntialiasMode,
                   aOptions.mSnapping);

  surface->ApplyTexturingOptions(aSurfOptions.mFilter, EXTEND_CLAMP,
                                 aSurfOptions.mSamplingBounds);

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

  GLContextNVpr* const gl = GLContextNVpr::Instance();
  gl->MakeCurrent();

  Validate();

  fprintf(stderr, "@@@@> TODO: implement DrawTargetNVpr::DrawSurfaceWithShadow [%u]\n", GLuint(*surface));

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

  GLContextNVpr* const gl = GLContextNVpr::Instance();
  gl->MakeCurrent();

  // TODO: Consider using NV_draw_texture instead.

  gl->AttachTexture2DToFramebuffer(GL_READ_FRAMEBUFFER, *surface);
  gl->SetFramebuffer(GL_DRAW_FRAMEBUFFER, mFramebuffer);

  gl->BlitFramebuffer(aSourceRect.x, aSourceRect.YMost(), aSourceRect.XMost(),
                      aSourceRect.y, aDestination.x, aDestination.y, aDestination.x
                      + aSourceRect.width, aDestination.y + aSourceRect.height,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
}

void
DrawTargetNVpr::FillRect(const Rect& aRect,
                         const Pattern& aPattern,
                         const DrawOptions& aOptions)
{
  GLContextNVpr* const gl = GLContextNVpr::Instance();
  gl->MakeCurrent();

  Validate();

  if (mStencilClipBits) {
    gl->EnableStencilTest(GLContextNVpr::PASS_IF_ALL_SET, mStencilClipBits,
                          GLContextNVpr::LEAVE_UNCHANGED);
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

  GLContextNVpr* const gl = GLContextNVpr::Instance();
  gl->MakeCurrent();

  Validate();

  gl->ConfigurePathStencilTest(mStencilClipBits);
  path->ApplyStrokeOptions(aStrokeOptions);
  gl->StencilStrokePathNV(*path, 0x1, 0x1);

  gl->EnableStencilTest(GLContextNVpr::PASS_IF_NOT_ZERO, 0x1,
                        GLContextNVpr::CLEAR_PASSING_VALUES, 0x1);
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

  GLContextNVpr* const gl = GLContextNVpr::Instance();
  gl->MakeCurrent();

  Validate();

  const GLubyte countingMask
    = path->GetFillRule() == FILL_WINDING ? (~mStencilClipBits & 0xff) : 0x1;

  gl->ConfigurePathStencilTest(mStencilClipBits);
  gl->StencilFillPathNV(*path, GL_COUNT_UP_NV, countingMask);

  gl->EnableStencilTest(GLContextNVpr::PASS_IF_NOT_ZERO, countingMask,
                        GLContextNVpr::CLEAR_PASSING_VALUES, countingMask);
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

  GLContextNVpr* const gl = GLContextNVpr::Instance();
  gl->MakeCurrent();

  Validate();

  const GLubyte countingMask = (~mStencilClipBits & 0xff);

  {
    Matrix transform = GetTransform();
    transform.Scale(font->Size(), -font->Size());
    GLContextNVpr::ScopedPushTransform pushTransform(transform);

    vector<GLuint> characters(aBuffer.mNumGlyphs);
    struct position {GLfloat x, y;};
    vector<position> positions(aBuffer.mNumGlyphs);

    for (size_t i = 0; i < aBuffer.mNumGlyphs; i++) {
      // TODO: How can we know the real mapping index -> unicode?
      characters[i] = aBuffer.mGlyphs[i].mIndex + 29;
      positions[i].x = aBuffer.mGlyphs[i].mPosition.x * font->InverseSize();
      positions[i].y = aBuffer.mGlyphs[i].mPosition.y * -font->InverseSize();
    }

    gl->ConfigurePathStencilTest(mStencilClipBits);
    gl->StencilFillPathInstancedNV(aBuffer.mNumGlyphs, GL_UNSIGNED_INT, &characters.front(),
                                 *font, GL_COUNT_UP_NV, countingMask,
                                 GL_TRANSLATE_2D_NV, &positions[0].x);
  }

  const Rect& glyphBounds = font->GlyphsBoundingBox();
  Point minPoint = aBuffer.mGlyphs[0].mPosition;
  Point maxPoint = aBuffer.mGlyphs[0].mPosition;

  for (size_t i = 1; i < aBuffer.mNumGlyphs; i++) {
    const Point& pt = aBuffer.mGlyphs[i].mPosition;
    minPoint = Point(min(minPoint.x, pt.x), min(minPoint.y, pt.y));
    maxPoint = Point(max(maxPoint.x, pt.x), max(maxPoint.y, pt.y));
  }

  gl->EnableStencilTest(GLContextNVpr::PASS_IF_NOT_ZERO, countingMask,
                        GLContextNVpr::CLEAR_PASSING_VALUES, countingMask);
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
  GLContextNVpr* const gl = GLContextNVpr::Instance();
  gl->MakeCurrent();

  Validate();
  fprintf(stderr, "@@@@> TODO: implement DrawTargetNVpr::Mask\n");

  MarkChanged();
}

void
DrawTargetNVpr::MaskSurface(const Pattern &aSource,
                            SourceSurface *aMask,
                            Point aOffset,
                            const DrawOptions &aOptions)
{
  GLContextNVpr* const gl = GLContextNVpr::Instance();
  gl->MakeCurrent();

  Validate();
  fprintf(stderr, "@@@@> TODO: implement DrawTargetNVpr::MaskSurface\n");

  MarkChanged();
}

void
DrawTargetNVpr::PushClip(const Path* aPath)
{
  MOZ_ASSERT(aPath->GetBackendType() == BACKEND_NVPR);

  const PathNVpr* const path = static_cast<const PathNVpr*>(aPath);

  PushClip(GetTransform(), path);
}

void
DrawTargetNVpr::PushClipRect(const Rect& aRect)
{
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

  PushClip(transform, mUnitSquarePath.get());
}

void
DrawTargetNVpr::PushClip(const Matrix& aTransform, const PathNVpr* aPath)
{
  if (mPoppedClips && mPoppedClips->IsForPath(aTransform, aPath)) {
    mPoppedClips = mPoppedClips->GetNext();
    return;
  }

  GLContextNVpr* const gl = GLContextNVpr::Instance();
  gl->MakeCurrent();

  Validate();

  RefPtr<ClipNVpr> clip;
#if 0
  if (!aPath->ConvexOutline().empty()
      && mActiveClipPlanes + aPath->ConvexOutline().size() <= mMaxClipPlanes) {
    clip = new PlanesClipNVpr(this, aTransform, aPath->Clone());
  } else {
    clip = new StencilClipNVpr(this, aTransform, aPath->Clone());
  }
#else
  // Driver clipping planes + instanced bug.
  clip = new StencilClipNVpr(this, aTransform, aPath->Clone());
#endif

  clip->Prepend(mClip.forget());
  mClip = clip.forget();

  mClip->Apply();
}

void
DrawTargetNVpr::PopClip()
{
  mPoppedClips = !mPoppedClips ? mClip.get() : mPoppedClips->GetPrevious();
  MOZ_ASSERT(mPoppedClips);
}

TemporaryRef<SourceSurface> 
DrawTargetNVpr::CreateSourceSurfaceFromData(unsigned char* aData,
                                            const IntSize& aSize,
                                            int32_t aStride,
                                            SurfaceFormat aFormat) const
{
  // TODO: Is alpha premultiplied?
  return SourceSurfaceNVpr::CreateFromData(aFormat, aSize, aData, aStride);
}

TemporaryRef<SourceSurface> 
DrawTargetNVpr::OptimizeSourceSurface(SourceSurface* aSurface) const
{
  if (aSurface->GetType() == SURFACE_NVPR_TEXTURE) {
    return aSurface;
  }

  RefPtr<DataSourceSurface> data = aSurface->GetDataSurface();
  // TODO: Is alpha premultiplied?
  return SourceSurfaceNVpr::CreateFromData(data.get());
}

TemporaryRef<SourceSurface>
DrawTargetNVpr::CreateSourceSurfaceFromNativeSurface(const NativeSurface& aSurface) const
{
  fprintf(stderr, "@@@@@> CreateSourceSurfaceFromNativeSurface\n");
  return 0;
}

TemporaryRef<DrawTarget>
DrawTargetNVpr::CreateSimilarDrawTarget(const IntSize& aSize, SurfaceFormat aFormat) const
{
  return Create(aSize, aFormat);
}

TemporaryRef<PathBuilder>
DrawTargetNVpr::CreatePathBuilder(FillRule aFillRule) const
{
  return new PathBuilderNVpr(aFillRule);
}

TemporaryRef<GradientStops>
DrawTargetNVpr::CreateGradientStops(GradientStop* rawStops, uint32_t aNumStops, ExtendMode aExtendMode) const
{
  return GradientStopsNVpr::create(rawStops, aNumStops, aExtendMode);
}

void*
DrawTargetNVpr::GetNativeSurface(NativeSurfaceType aType)
{
  fprintf(stderr, "@@@@@> GetNativeSurface\n");
  return 0;
}

void
DrawTargetNVpr::SetTransform(const Matrix& aTransform)
{
  GLContextNVpr* const gl = GLContextNVpr::Instance();
  gl->MakeCurrent();

  gl->SetTransform(aTransform);

  DrawTarget::SetTransform(aTransform);
}

void
DrawTargetNVpr::Validate()
{
  GLContextNVpr* const gl = GLContextNVpr::Instance();
  MOZ_ASSERT(gl->IsCurrent());

  gl->SetTargetSize(mSize);
  gl->SetFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
  gl->SetTransform(GetTransform());
  gl->EnableColorWrites();

  if (mPoppedClips) {
    mPoppedClips->RestorePreviousClipState();
    mClip = mPoppedClips->DetachFromPrevious();
    mPoppedClips = 0;
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
  GLContextNVpr* const gl = GLContextNVpr::Instance();
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

  GLContextNVpr* const gl = GLContextNVpr::Instance();
  MOZ_ASSERT(gl->IsCurrent());

  Matrix textureCoords = aPattern.mMatrix;
  textureCoords.Invert();
  textureCoords.PostScale(1.0f / surface->GetSize().width,
                          1.0f / surface->GetSize().height);

  gl->SetColorToAlpha(aAlpha);
  gl->EnableTexturing(GL_TEXTURE_2D, *surface,
                      GLContextNVpr::TEXGEN_ST, textureCoords);
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
  GLContextNVpr* const gl = GLContextNVpr::Instance();
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

GLuint
DrawTargetNVpr::ReserveClipPlanes(size_t count)
{
  GLContextNVpr* const gl = GLContextNVpr::Instance();
  MOZ_ASSERT(GLContextNVpr::Instance()->IsCurrent());
  MOZ_ASSERT(mActiveClipPlanes + count <= mMaxClipPlanes);

  const GLuint planes = mActiveClipPlanes;
  mActiveClipPlanes += count;

  for (GLuint i = planes; i < mActiveClipPlanes; i++) {
    gl->Enable(GL_CLIP_PLANE0 + i);
  }

  return planes;
}

void
DrawTargetNVpr::ReleaseClipPlanes(GLuint aIndex)
{
  GLContextNVpr* const gl = GLContextNVpr::Instance();
  MOZ_ASSERT(GLContextNVpr::Instance()->IsCurrent());

  for (GLuint i = aIndex; i < mActiveClipPlanes; i++) {
    gl->Disable(GL_CLIP_PLANE0 + i);
  }

  mActiveClipPlanes = aIndex;
}

}
}
