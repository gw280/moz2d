/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GradientStopsNVpr.h"
#include "GradientShadersNVpr.h"
#include <vector>

using namespace std;

struct TextureColor {
  TextureColor(const mozilla::gfx::Color& color)
    : r(static_cast<GLubyte>(color.a * color.r * 255))
    , g(static_cast<GLubyte>(color.a * color.g * 255))
    , b(static_cast<GLubyte>(color.a * color.b * 255))
    , a(static_cast<GLubyte>(color.a * 255))
  {}
  GLubyte r, g, b, a;
};

namespace mozilla {
namespace gfx {

GradientStopsNVpr::GradientStopsNVpr(GradientStop* aRawStops, uint32_t aNumStops,
                                     ExtendMode aExtendMode)
  : mRampTextureId(0)
  , mInitialColor(1, 1, 1, 1)
  , mFinalColor(1, 1, 1, 1)
{
  if (!aRawStops || !aNumStops) {
    return;
  }

  if (aNumStops == 1) {
    mInitialColor = mFinalColor = aRawStops[0].color;
    return;
  }

  vector<GradientStop> sortedStops;
  sortedStops.resize(aNumStops);
  memcpy(sortedStops.data(), aRawStops, aNumStops * sizeof(GradientStop));
  sort(sortedStops.begin(), sortedStops.end());

  mInitialColor = sortedStops.front().color;
  mFinalColor = sortedStops.back().color;

  GLContextNVpr* const gl = GLContextNVpr::Instance();
  gl->MakeCurrent();

  // Create a 1D texture for the color ramp.
  GLsizei rampSize = min(4096, gl->MaxTextureSize());

  gl->GenTextures(1, &mRampTextureId);
  gl->TextureImage1DEXT(mRampTextureId, GL_TEXTURE_1D, 0, GL_RGBA, rampSize, 0,
                        GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

  // Render the gradient stops into the color ramp texture.
  gl->SetTargetSize(IntSize(rampSize, 1));
  gl->SetFramebufferToTexture(GL_FRAMEBUFFER, GL_TEXTURE_1D, mRampTextureId);

  Matrix colorRampCoords;
  colorRampCoords.Scale(rampSize, 1);
  colorRampCoords.Translate(0, 0.5f);
  gl->SetTransform(colorRampCoords, gl->GetUniqueId());

  gl->EnableColorWrites();
  gl->SetColor(sortedStops.front().color);
  gl->DisableClipPlanes();
  gl->DisableTexturing();
  gl->DisableShading();

  gl->Begin(GL_LINE_STRIP); {

    if (sortedStops.front().offset > 0) {
      gl->Vertex2f(0, 0);
    }

    for (size_t i = 0; i < sortedStops.size(); i++) {
      gl->SetColor(sortedStops[i].color);
      gl->Vertex2f(sortedStops[i].offset, 0);
    }

    if (sortedStops.back().offset < 1) {
      gl->Vertex2f(1, 0);
    }

  } gl->End();

  gl->GenerateTextureMipmapEXT(mRampTextureId, GL_TEXTURE_1D);

  // Configure texturing parameters.
  gl->TextureParameteriEXT(mRampTextureId, GL_TEXTURE_1D,
                           GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  gl->TextureParameteriEXT(mRampTextureId, GL_TEXTURE_1D,
                           GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  GLenum wrapMode;
  switch (aExtendMode) {
    default:
      MOZ_ASSERT(!"Invalid gradient extend mode");
    case EXTEND_CLAMP:
      wrapMode = GL_CLAMP_TO_EDGE;
      break;
    case EXTEND_REPEAT:
      wrapMode = GL_REPEAT;
      break;
    case EXTEND_REFLECT:
      wrapMode = GL_MIRRORED_REPEAT;
      break;
  }
  gl->TextureParameteriEXT(mRampTextureId, GL_TEXTURE_1D,
                           GL_TEXTURE_WRAP_S, wrapMode);

  if (aExtendMode == EXTEND_CLAMP) {
    // Ensure the left-most and right-most pixels are the color of the initial
    // and final stops of the ramp. That way other image data won't bleed into
    // the clamped colors.
    const TextureColor initialColor(mInitialColor);
    const TextureColor finalColor(mFinalColor);
    GLsizei levelSize = rampSize;
    GLint level = 0;
    while (levelSize >= 2) {
      gl->TextureSubImage1DEXT(mRampTextureId, GL_TEXTURE_1D, level, 0, 1,
                               GL_RGBA, GL_UNSIGNED_BYTE, &initialColor);
      gl->TextureSubImage1DEXT(mRampTextureId, GL_TEXTURE_1D, level, levelSize - 1,
                               1, GL_RGBA, GL_UNSIGNED_BYTE, &finalColor);
      levelSize >>= 1;
      level++;
    }
    gl->TextureParameteriEXT(mRampTextureId, GL_TEXTURE_1D,
                             GL_TEXTURE_MAX_LEVEL, level - 1);
  }
}

GradientStopsNVpr::~GradientStopsNVpr()
{
  GLContextNVpr* const gl = GLContextNVpr::Instance();
  gl->MakeCurrent();

  gl->DeleteTexture(mRampTextureId);
}


void
GradientStopsNVpr::ApplyLinearGradient(const Point& aBegin, const Point& aEnd,
                                       float aAlpha) const
{
  const Point vector = aEnd - aBegin;
  const float lengthSquared = (vector.x * vector.x + vector.y * vector.y);

  GLContextNVpr* const gl = GLContextNVpr::Instance();
  MOZ_ASSERT(gl->IsCurrent());

  if (!lengthSquared || !mRampTextureId) {
    gl->SetColor(mFinalColor, aAlpha);
    gl->DisableTexturing();
    gl->DisableShading();
    return;
  }

  const GLfloat texgenCoefficients[] = {
    vector.x / lengthSquared,
    vector.y / lengthSquared,
    -(aBegin.x * vector.x + aBegin.y * vector.y) / lengthSquared
  };

  gl->SetColorToAlpha(aAlpha);
  gl->EnableTexturing(GL_TEXTURE_1D, mRampTextureId,
                      GLContextNVpr::TEXGEN_S, texgenCoefficients);
  gl->DisableShading();
}

void
GradientStopsNVpr::ApplyFocalGradient(const Point& aCenter, float aRadius,
                                      const Point& aFocalPoint,
                                      float aAlpha) const
{
  GLContextNVpr* const gl = GLContextNVpr::Instance();
  MOZ_ASSERT(gl->IsCurrent());

  if (!aRadius) {
    gl->SetColor(Color(1, 1, 1, 1), aAlpha);
    gl->DisableTexturing();
    gl->DisableShading();
    return;
  }

  if (!mRampTextureId) {
    // TODO: This doesn't exclude regions not in the gradient from being drawn.
    gl->SetColor(mFinalColor, aAlpha);
    gl->DisableTexturing();
    gl->DisableShading();
    return;
  }

  gl->SetColorToAlpha(aAlpha);

  // Setup a transformation where the gradient is the unit-circle.
  Matrix gradientCoords;
  gradientCoords.Scale(1 / aRadius, 1 / aRadius);
  gradientCoords.Translate(-aCenter.x, -aCenter.y);

  Point focalPoint = gradientCoords * aFocalPoint;
  const float focalOffsetSquared = focalPoint.x * focalPoint.x
                                   + focalPoint.y * focalPoint.y;

  if (fabs(focalOffsetSquared) < 1e-5f) { // The focal point is at [0, 0].
    gl->EnableTexturing(GL_TEXTURE_1D, mRampTextureId,
                        GLContextNVpr::TEXGEN_ST, gradientCoords);
    gl->EnableShading(Shaders().mFocalGradCenteredShader);
    return;
  }

  // With the following variables inside the unit circle:
  //
  //   f = focal point, normalized to a unit-circle gradient
  //   p = sample's [x,y] location, normalized to a unit-circle gradient
  //
  // A shader program can find the sample's gradient offset using the general
  // radial gradient equation:
  //
  //   offset = (dot(p - f, f) +/- sqrt(dot(p - f, p - f) - cross(p - f, f)^2))
  //            / (1 - dot(f, f))
  //
  // Below we massage this equation to make the math more efficient.

  // 1) Rotate the gradient so the focal point is on the x-axis (i.e. f.y == 0):
  //
  //   Now offset = ((p - f).x * f.x +/- sqrt((p - f).x^2 + (p - f).y^2
  //                                          - (p - f).y^2 * f.x^2))
  //                / (1 - dot(f, f))
  //
  //              = ((p - f).x * f.x +/- sqrt((p - f).x^2
  //                                          + (1 - f.x^2) * (p - f).y^2))
  //                / (1 - dot(f, f))
  //
  Matrix rotation = Matrix::Rotation(-atan2(focalPoint.y, focalPoint.x));
  gradientCoords = gradientCoords * rotation;
  focalPoint = Point(sqrt(focalOffsetSquared), 0);

  // 2) Let q = p - f
  //
  // Now offset = (q.x * f.x +/- sqrt(q.x^2 + (1 - f.x^2) * q.y^2))
  //              / (1 - dot(f, f))
  //
  Matrix qCoords = gradientCoords;
  qCoords.PostTranslate(-focalPoint.x, -focalPoint.y);

  if (fabs(1 - focalOffsetSquared) < 1e-5f) {
    // The focal point is touching the circle. We can't use the general equation
    // because it would divide by zero. Instead we use a special-case formula
    // knowing that f = [0, 1]:
    //
    // offset = dot(p - f, p - f) / (-2 * (p - f).x)
    //
    //        = dot(q, q) / (-2 * q.x)
    //
    gl->EnableTexturing(GL_TEXTURE_1D, mRampTextureId,
                          GLContextNVpr::TEXGEN_ST, qCoords);
    gl->EnableShading(Shaders().mFocalGradTouchingShader);

    return;
  }

  // 3) Let a = 1 / (1 - dot(f, f)):
  //
  // Now offset = a * q.x * f.x + sqrt(a^2 * q.x^2 + a^2 * (1 - f.x^2) * q.y^2))
  //
  // (Note that this reverses the sign of the sqrt when a < 0, and that's exacly
  //  what we want since it allows us to just always use + with it)
  //
  float a = 1 / (1 - focalOffsetSquared);

  if (a < 0) { // The focal point is outside the circle.
    // 4) q.x *= a
    //    q.y *= a
    //
    // Now offset = q.x * f.x + sqrt(q.x^2 + (1 - f.x^2) * q.y^2))
    //
    qCoords.PostScale(a, a);

    gl->EnableTexturing(GL_TEXTURE_1D, mRampTextureId,
                        GLContextNVpr::TEXGEN_ST, qCoords);

    gl->EnableShading(Shaders().mFocalGradOutsideShader);
    Shaders().mFocalGradOutsideShader.uFocalX = focalPoint.x;
    Shaders().mFocalGradOutsideShader.u1MinusFx_2 = 1 - focalPoint.x
                                                        * focalPoint.x;

    return;
  }

  // 4) q.x *= a
  //    q.y *= a * sqrt(1 - f.x^2)
  //
  // Now offset = q.x * f.x + sqrt(q.x^2 + q.y^2)
  //
  //            = q.x * f.x + length(q)
  //
  qCoords.PostScale(a, a * sqrt(1 - focalOffsetSquared));

  gl->EnableTexturing(GL_TEXTURE_1D, mRampTextureId,
                      GLContextNVpr::TEXGEN_ST, qCoords);

  gl->EnableShading(Shaders().mFocalGradInsideShader);
  Shaders().mFocalGradInsideShader.uFocalX = focalPoint.x;
}

void GradientStopsNVpr::ApplyRadialGradient(const Point& aBeginCenter,
                                            float aBeginRadius,
                                            const Point& aEndCenter,
                                            float aEndRadius,
                                            float aAlpha) const
{
  GLContextNVpr* const gl = GLContextNVpr::Instance();
  MOZ_ASSERT(gl->IsCurrent());

  if (aBeginCenter == aEndCenter && aBeginRadius == aEndRadius) {
    // TODO: ApplyColorPattern
    gl->SetColor(Color(1, 1, 1, 1), aAlpha);
    gl->DisableTexturing();
    gl->DisableShading();
    return;
  }

  if (!mRampTextureId) {
    // TODO: This doesn't exclude regions not in the gradient from being drawn.
    gl->SetColor(mFinalColor, aAlpha);
    gl->DisableTexturing();
    gl->DisableShading();
    return;
  }

  gl->SetColorToAlpha(aAlpha);

  // Setup a transformation where the begin circle is the unit-circle.
  Matrix gradientCoords;
  gradientCoords.Scale(1 / aBeginRadius, 1 / aBeginRadius);
  gradientCoords.Translate(-aBeginCenter.x, -aBeginCenter.y);

  // At this point, the begin circle is the unit-circle and we define the
  // following variables:
  //
  //   c = end circle's center
  //   r = end circle's radius
  //   p = sample's [x,y] location
  //   A = dot(c, c) - r^2 + 2 * r - 1
  //
  // A shader program can use the this equation to find the gradient offset:
  //
  //   offset = (dot(c, p) + r - 1 +/- sqrt((dot(c, p) + r - 1)^2
  //                                        - 4 * A * (dot(p, p) - 1))) / A
  Point endCenter = gradientCoords * aEndCenter;
  float endRadius = aEndRadius / aBeginRadius;
  float A = endCenter.x * endCenter.x + endCenter.y * endCenter.y
            - endRadius * endRadius + 2 * endRadius - 1;

  // TODO: Make a special case for A ~= 0.

  // Let q = (1 / A) * p, B = (r - 1) / A, C = 1 / A
  //
  // Now      d = dot(c, q) + B
  //     offset = d +/- sqrt(d^2 - A * dot(q, q) + C)
  //
  // (Note that this reverses the sign of the sqrt when A < 0)
  float C = 1 / A;
  float B = (endRadius - 1) * C;
  Matrix qCoords = gradientCoords;
  qCoords.PostScale(C, C);

  gl->EnableTexturing(GL_TEXTURE_1D, mRampTextureId,
                      GLContextNVpr::TEXGEN_ST, qCoords);

  if (A >= 0) {
    RadialGradOutsideShader& shader = (aEndRadius - aBeginRadius > 1e-5f)
      ? Shaders().mRadialGradOutsideShaderDiscardLo
      : Shaders().mRadialGradOutsideShaderDiscardHi;
    gl->EnableShading(shader);
    shader.uEndCenter = endCenter;
    shader.uA = A;
    shader.uB = B;
    shader.uC = C;
    shader.uOffsetBound = aBeginRadius / (aBeginRadius - aEndRadius);
    return;
  }

  RadialGradInsideShader& shader = (aEndRadius > aBeginRadius)
    ? Shaders().mRadialGradInsideShaderAddSqrt
    : Shaders().mRadialGradInsideShaderSubSqrt;
  gl->EnableShading(shader);
  shader.uEndCenter = endCenter;
  shader.uA = A;
  shader.uB = B;
  shader.uC = C;
}

GradientShadersNVpr& GradientStopsNVpr::Shaders() const
{
  UserDataNVpr& userData = GLContextNVpr::Instance()->UserData();
  if (!userData.mGradientShaders) {
    userData.mGradientShaders.reset(new GradientShadersNVpr());
  }

  return static_cast<GradientShadersNVpr&>(*userData.mGradientShaders.get());
}

}
}
