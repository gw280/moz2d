/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_GRADIENTSTOPSNVPR_H_
#define MOZILLA_GFX_GRADIENTSTOPSNVPR_H_

#include "2D.h"
#include <GL/gl.h>
#include <mozilla/RefPtr.h>

namespace mozilla {
namespace gfx {

class GradientShadersNVpr;

class GradientStopsNVpr : public GradientStops {
public:
  static TemporaryRef<GradientStopsNVpr>
  create(GradientStop* aRawStops, uint32_t aNumStops, ExtendMode aExtendMode)
  {
    return new GradientStopsNVpr(aRawStops, aNumStops, aExtendMode);
  }

  ~GradientStopsNVpr();

  virtual BackendType GetBackendType() const { return BACKEND_NVPR; }

  void ApplyLinearGradient(const Point& aBegin, const Point& aEnd,
                           float aAlpha) const;

  void ApplyFocalGradient(const Point& aCenter, float aRadius,
                          const Point& aFocalPoint, float aAlpha) const;

  void ApplyRadialGradient(const Point& aBeginCenter, float aBeginRadius,
                           const Point& aEndCenter, float aEndRadius,
                           float aAlpha) const;

private:
  GradientStopsNVpr(GradientStop* aRawStops, uint32_t aNumStops,
                    ExtendMode aExtendMode);

  GradientShadersNVpr& Shaders() const;

  GLuint mRampTextureId;
  Color mInitialColor;
  Color mFinalColor;
};

}
}

#endif /* MOZILLA_GFX_GRADIENTSTOPSNVPR_H_ */
