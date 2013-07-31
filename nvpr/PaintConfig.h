/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_NVPR_PAINTCONFIG_H_
#define MOZILLA_GFX_NVPR_PAINTCONFIG_H_

#ifdef WIN32
#define NOMINMAX
#include <Windows.h>
#undef NOMINMAX
#endif

#include "2D.h"
#include <GL/gl.h>

namespace mozilla {
namespace gfx {

class GradientStopsNVpr;
class SourceSurfaceNVpr;

namespace nvpr {

struct PaintConfig {
  enum PaintMode {
    MODE_NONE,
    MODE_SOLID_COLOR,
    MODE_TEXTURE_1D,
    MODE_TEXTURE_2D,
    MODE_TEXTURE_2D_CLAMPED,
    MODE_FOCAL_GRAD_CENTERED,
    MODE_FOCAL_GRAD_INSIDE,
    MODE_FOCAL_GRAD_OUTSIDE,
    MODE_FOCAL_GRAD_TOUCHING,
    MODE_RADIAL_GRAD_INSIDE_ADD_SQRT,
    MODE_RADIAL_GRAD_INSIDE_SUBTRACT_SQRT,
    MODE_RADIAL_GRAD_OUTSIDE_DISCARD_HIGH,
    MODE_RADIAL_GRAD_OUTSIDE_DISCARD_LOW,
    MODE_COUNT
  };

  enum TexgenComponents {
    TEXGEN_NONE,
    TEXGEN_S,
    TEXGEN_ST
  };

  PaintConfig();
  void SetToPattern(const Pattern& aPattern);
  void SetToColor(const Color& aColor);
  void SetToSurface(SourceSurfaceNVpr* aSurface, Filter aFilter = FILTER_LINEAR,
                    ExtendMode aExtendMode = EXTEND_CLAMP);
  void SetToSurface(SourceSurfaceNVpr* aSurface, const Rect& aSamplingBounds,
                    Filter aFilter = FILTER_LINEAR,
                    ExtendMode aExtendMode = EXTEND_CLAMP);
  void SetToSurface(SourceSurfaceNVpr* aSurface, const Matrix& aTexCoordMatrix,
                    Filter aFilter = FILTER_LINEAR,
                    ExtendMode aExtendMode = EXTEND_CLAMP);
  void SetToGradient(GradientStopsNVpr* aStops,
                     const Point& aBeginPoint, const Point& aEndPoint);
  void SetToGradient(GradientStopsNVpr* aStops, const Point& aFocalPoint,
                     const Point& aEndCenter, float aEndRadius);
  void SetToGradient(GradientStopsNVpr* aStops,
                     const Point& aBeginCenter, float aBeginRadius,
                     const Point& aEndCenter, float aEndRadius);
  void SetTexgenCoefficients(const Matrix& aTransform);

  PaintMode mPaintMode;
  GLuint mTextureId;
  TexgenComponents mTexgenComponents;
  GLfloat mTexgenCoefficients[6];
  union {
    struct {
      GLfloat uColor[4];
    };
    struct {
      GLfloat uClampRect[4];
    };
    struct {
      GLfloat uFocalX;
      GLfloat u1MinuxFx_2;
    };
    struct {
      GLfloat uEndCenter[2];
      GLfloat uA;
      GLfloat uB;
      GLfloat uC;
      GLfloat uOffsetLimit;
    };
  };
};

}
}
}

#endif /* MOZILLA_GFX_NVPR_PAINTCONFIG_H_ */
