/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PaintConfig.h"
#include "GradientStopsNVpr.h"
#include "SourceSurfaceNVpr.h"

using namespace std;

namespace mozilla {
namespace gfx {
namespace nvpr {

PaintConfig::PaintConfig()
  : mPaintMode(MODE_NONE)
  , mTexgenComponents(TEXGEN_NONE)
{
}

void
PaintConfig::SetToPattern(const Pattern& aPattern)
{
  switch (aPattern.GetType()) {
    default:
      MOZ_ASSERT(!"Invalid pattern type");
    case PATTERN_COLOR:
      SetToColor(static_cast<const ColorPattern&>(aPattern).mColor);
      return;
    case PATTERN_SURFACE: {
      const SurfacePattern& pat(static_cast<const SurfacePattern&>(aPattern));
      MOZ_ASSERT(pat.mSurface->GetType() == SURFACE_NVPR_TEXTURE);

      SetToSurface(static_cast<SourceSurfaceNVpr*>(pat.mSurface.get()),
                   pat.mMatrix, pat.mFilter, pat.mExtendMode);
      return;
    } case PATTERN_LINEAR_GRADIENT: {
      const LinearGradientPattern& pat(
        static_cast<const LinearGradientPattern&>(aPattern));
      MOZ_ASSERT(pat.mStops->GetBackendType() == BACKEND_NVPR);

      SetToGradient(static_cast<GradientStopsNVpr*>(pat.mStops.get()),
                    pat.mBegin, pat.mEnd);
      return;
    } case PATTERN_RADIAL_GRADIENT: {
      const RadialGradientPattern& pat(
        static_cast<const RadialGradientPattern&>(aPattern));
      MOZ_ASSERT(pat.mStops->GetBackendType() == BACKEND_NVPR);

      if (pat.mRadius1 == 0) {
        SetToGradient(static_cast<GradientStopsNVpr*>(pat.mStops.get()),
                      pat.mCenter1, pat.mCenter2, pat.mRadius2);
        return;
      }

      SetToGradient(static_cast<GradientStopsNVpr*>(pat.mStops.get()),
                    pat.mCenter1, pat.mRadius1, pat.mCenter2, pat.mRadius2);
      return;
    }
  }
}

void
PaintConfig::SetToColor(const Color& aColor)
{
  mPaintMode = MODE_SOLID_COLOR;
  uColor[0] = aColor.r;
  uColor[1] = aColor.g;
  uColor[2] = aColor.b;
  uColor[3] = aColor.a;
}

void
PaintConfig::SetToSurface(SourceSurfaceNVpr* aSurface, Filter aFilter,
                          ExtendMode aExtendMode)
{
  mPaintMode = MODE_TEXTURE_2D;
  mTextureId = *aSurface;

  aSurface->ApplyTexturingOptions(aFilter, aExtendMode);
}

void
PaintConfig::SetToSurface(SourceSurfaceNVpr* aSurface,
                          const Rect& aSamplingBounds,
                          Filter aFilter, ExtendMode aExtendMode)
{
  Rect clampRect = aSamplingBounds;
  clampRect.Deflate(0.5f);
  clampRect.ScaleInverse(aSurface->GetSize().width, aSurface->GetSize().height);

  mPaintMode = MODE_TEXTURE_2D_CLAMPED;
  mTextureId = *aSurface;
  uClampRect[0] = clampRect.x;
  uClampRect[1] = clampRect.y;
  uClampRect[2] = clampRect.XMost();
  uClampRect[3] = clampRect.YMost();

  aSurface->ApplyTexturingOptions(aFilter, aExtendMode);
}

void
PaintConfig::SetToSurface(SourceSurfaceNVpr* aSurface,
                          const Matrix& aTexCoordMatrix,
                          Filter aFilter, ExtendMode aExtendMode)
{
  Matrix textureCoords = aTexCoordMatrix;
  textureCoords.Invert();
  textureCoords.PostScale(1.0f / aSurface->GetSize().width,
                          1.0f / aSurface->GetSize().height);

  mPaintMode = MODE_TEXTURE_2D;
  mTextureId = *aSurface;
  mTexgenComponents = TEXGEN_ST;
  SetTexgenCoefficients(textureCoords);

  aSurface->ApplyTexturingOptions(aFilter, aExtendMode);
}

void
PaintConfig::SetToGradient(GradientStopsNVpr* aStops,
                           const Point& aBeginPoint, const Point& aEndPoint)
{
  const Point vector = aEndPoint - aBeginPoint;
  const float lengthSquared = (vector.x * vector.x + vector.y * vector.y);

  if (!lengthSquared || !*aStops) {
    SetToColor(aStops->FinalColor());
    return;
  }

  mPaintMode = MODE_TEXTURE_1D;
  mTextureId = *aStops;
  mTexgenComponents = TEXGEN_S;
  mTexgenCoefficients[0] = vector.x / lengthSquared;
  mTexgenCoefficients[1] = vector.y / lengthSquared;
  mTexgenCoefficients[2] =
    -(aBeginPoint.x * vector.x + aBeginPoint.y * vector.y) / lengthSquared;
}

void
PaintConfig::SetToGradient(GradientStopsNVpr* aStops, const Point& aFocalPoint,
                           const Point& aEndCenter, float aEndRadius)
{
  if (!aEndRadius) {
    mPaintMode = MODE_NONE;
    return;
  }

  if (!*aStops) {
    // TODO: This doesn't exclude regions not in the gradient from being drawn.
    SetToColor(aStops->FinalColor());
    return;
  }

  mTextureId = *aStops;
  mTexgenComponents = TEXGEN_ST;

  // Setup a transformation where the gradient is the unit-circle.
  Matrix gradientCoords;
  gradientCoords.Scale(1 / aEndRadius, 1 / aEndRadius);
  gradientCoords.Translate(-aEndCenter.x, -aEndCenter.y);

  Point focalPoint = gradientCoords * aFocalPoint;
  const float focalOffsetSquared = focalPoint.x * focalPoint.x
                                   + focalPoint.y * focalPoint.y;

  if (fabs(focalOffsetSquared) < 1e-5f) { // The focal point is at [0, 0].
    mPaintMode = MODE_FOCAL_GRAD_CENTERED;
    SetTexgenCoefficients(gradientCoords);
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
    mPaintMode = MODE_FOCAL_GRAD_TOUCHING;
    SetTexgenCoefficients(qCoords);

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

    mPaintMode = MODE_FOCAL_GRAD_OUTSIDE;
    SetTexgenCoefficients(qCoords);
    uFocalX = focalPoint.x;
    u1MinuxFx_2 = 1 - focalPoint.x * focalPoint.x;

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

  mPaintMode = MODE_FOCAL_GRAD_INSIDE;
  SetTexgenCoefficients(qCoords);
  uFocalX = focalPoint.x;
}

void
PaintConfig::SetToGradient(GradientStopsNVpr* aStops,
                           const Point& aBeginCenter, float aBeginRadius,
                           const Point& aEndCenter, float aEndRadius)
{
  if (aBeginCenter == aEndCenter && aBeginRadius == aEndRadius) {
    mPaintMode = MODE_NONE;
    return;
  }

  if (!*aStops) {
    // TODO: This doesn't exclude regions not in the gradient from being drawn.
    SetToColor(aStops->FinalColor());
    return;
  }

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

  mTextureId = *aStops;
  mTexgenComponents = TEXGEN_ST;
  SetTexgenCoefficients(qCoords);
  uEndCenter[0] = endCenter.x;
  uEndCenter[1] = endCenter.y;
  uA = A;
  uB = B;
  uC = C;

  if (A >= 0) {
    mPaintMode = (aEndRadius - aBeginRadius > 1e-5f)
      ? MODE_RADIAL_GRAD_OUTSIDE_DISCARD_LOW
      : MODE_RADIAL_GRAD_OUTSIDE_DISCARD_HIGH;
    uOffsetLimit = aBeginRadius / (aBeginRadius - aEndRadius);
    return;
  }

  mPaintMode = (aEndRadius > aBeginRadius)
    ? MODE_RADIAL_GRAD_INSIDE_ADD_SQRT
    : MODE_RADIAL_GRAD_INSIDE_SUBTRACT_SQRT;
}

void
PaintConfig::SetTexgenCoefficients(const Matrix& aTransform)
{
  mTexgenCoefficients[0] = aTransform._11;
  mTexgenCoefficients[1] = aTransform._21;
  mTexgenCoefficients[2] = aTransform._31;
  mTexgenCoefficients[3] = aTransform._12;
  mTexgenCoefficients[4] = aTransform._22;
  mTexgenCoefficients[5] = aTransform._32;
}

}
}
}
