/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_NVPR_SHADOWSHADERS_H_
#define MOZILLA_GFX_NVPR_SHADOWSHADERS_H_

#include "2D.h"
#include "GL.h"
#include <vector>

namespace mozilla {
namespace gfx {

class SourceSurfaceNVpr;

namespace nvpr {

class ShadowShaders : public nvpr::UserData::Object {
  class HorizontalConvolutionShader;
  class ShadowShader;

public:
  ShadowShaders();
  ~ShadowShaders();

  enum ConvolutionChannel { RED, ALPHA, CONVOLUTION_CHANNEL_COUNT };
  void ConfigureShaders(const IntSize& aFramebufferSize,
                        ConvolutionChannel aConvolutionChannel,
                        const Rect& aShadowRect, const Color& aShadowColor,
                        Float aSigma, GLuint* aHorizontalConvolutionShader,
                        GLuint* aShadowShader);

private:
  size_t mRadius;
  Float mScale;
  size_t mMaxRadius;
  std::vector<GLfloat> mWeights;
  UniqueId mWeightsId;
  std::vector<RefPtr<HorizontalConvolutionShader> >
    mHorizontalConvolutionShaders[CONVOLUTION_CHANNEL_COUNT];
  std::vector<RefPtr<ShadowShader> > mShadowShaders[CONVOLUTION_CHANNEL_COUNT];
};

}
}
}

#endif /* MOZILLA_GFX_NVPR_SHADOWSHADERS_H_ */
