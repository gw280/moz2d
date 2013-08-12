/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ShadowShaders.h"
#include "ShaderProgram.h"
#include "SourceSurfaceNVpr.h"
#include <iostream>
#include <sstream>

using namespace std;

namespace mozilla {
namespace gfx {
namespace nvpr {

class ShadowShaders::HorizontalConvolutionShader : public ShaderProgram {
public:
  static TemporaryRef<HorizontalConvolutionShader>
  Create(ConvolutionChannel aConvolutionChannel, size_t aRadius)
  {
    MOZ_ASSERT(gl->IsCurrent());

    const char channel = aConvolutionChannel == RED ? 'r' : 'a';
    RefPtr<HorizontalConvolutionShader> shader = new HorizontalConvolutionShader();

    ostringstream vertSrc;
    vertSrc << "uniform vec4 uSampleRect;" << endl;
    vertSrc << "uniform vec4 uShadowRect;" << endl;
    vertSrc << "uniform float uTexelWidth;" << endl;
    vertSrc << "varying vec4 vSampleCoords[" << (2 + aRadius) / 2 << "];" << endl;
    vertSrc << "void main()" << endl;
    vertSrc << "{" << endl;
    vertSrc << "  vec2 sampleLocation = (1 - gl_Vertex.xy) * uSampleRect.xy" << endl;
    vertSrc << "                        + gl_Vertex.xy * uSampleRect.zw;" << endl;
    vertSrc << "  vSampleCoords[0].st = sampleLocation;" << endl;
    for (size_t i = 1; i <= aRadius; i++) {
      vertSrc << "  vSampleCoords[" << (i / 2) << "]." << (i % 2 ? "zw" : "xy")
              << " = sampleLocation.s + vec2(" << i << ", -" << i
              << ") * uTexelWidth;" << endl;
    }
    vertSrc << "  vec2 vertexPosition = (1 - gl_Vertex.xy) * uShadowRect.xy" << endl;
    vertSrc << "                        + gl_Vertex.xy * uShadowRect.zw;" << endl;
    vertSrc << "  gl_Position = vec4(vertexPosition * 2 - 1, 0, 1);" << endl;
    vertSrc << "}" << endl;

    ostringstream fragSrc;
    fragSrc << "uniform float uWeights[" << (1 + aRadius) << "];" << endl;
    fragSrc << "uniform sampler2D uImage;" << endl;
    fragSrc << "varying vec4 vSampleCoords[" << (2 + aRadius) / 2 << "];" << endl;
    fragSrc << "void main()" << endl;
    fragSrc << "{" << endl;
    fragSrc << "  float convolution = uWeights[0] * texture2D(uImage, vSampleCoords[0].st).a;" << endl;
    for (size_t i = 1; i <= aRadius; i++) {
      fragSrc << "  convolution += uWeights[" << i << "]" << endl;
      fragSrc << "    * (texture2D(uImage, vec2(vSampleCoords["
              << (i / 2) << "][" << (2 * (i % 2)) << "], vSampleCoords[0].t)).a" << endl;
      fragSrc << "       + texture2D(uImage, vec2(vSampleCoords["
              << (i / 2) << "][" << (2 * (i % 2) + 1) << "], vSampleCoords[0].t)).a);" << endl;
    }
    fragSrc << "  gl_FragColor." << channel << " = convolution;" << endl;
    fragSrc << "}" << endl;

    shader->Initialize(vertSrc.str().c_str(), fragSrc.str().c_str());

    return shader.forget();
  }

  UniformVec4 uSampleRect;
  UniformVec4 uShadowRect;
  UniformFloat uTexelWidth;
  UniformFloatArray uWeights;
  UniformSampler uImage;

private:
  HorizontalConvolutionShader()
    : uSampleRect("uSampleRect")
    , uShadowRect("uShadowRect")
    , uTexelWidth("uTexelWidth")
    , uWeights("uWeights")
    , uImage("uImage", GL::UNIT_0)
  {}

  void Initialize(const GLchar* aVertexSource, const GLchar* aFragmentSource)
  {
    ShaderProgram::Initialize(aVertexSource, aFragmentSource);
    uSampleRect.Initialize(*this);
    uShadowRect.Initialize(*this);
    uTexelWidth.Initialize(*this);
    uWeights.Initialize(*this);
    uImage.Initialize(*this);
  }
};

class ShadowShaders::ShadowShader : public ShaderProgram {
public:
  static TemporaryRef<ShadowShader>
  Create(ConvolutionChannel aConvolutionChannel, size_t aRadius)
  {
    MOZ_ASSERT(gl->IsCurrent());

    const char channel = aConvolutionChannel == RED ? 'r' : 'a';
    RefPtr<ShadowShader> shader = new ShadowShader();

    ostringstream vertSrc;
    vertSrc << "uniform vec4 uShadowRect;" << endl;
    vertSrc << "uniform float uTexelHeight;" << endl;
    vertSrc << "varying vec4 vSampleCoords[" << (2 + aRadius) / 2 << "];" << endl;
    vertSrc << "void main()" << endl;
    vertSrc << "{" << endl;
    vertSrc << "  vec2 shadowLocation = (1 - gl_Vertex.xy) * uShadowRect.xy" << endl;
    vertSrc << "                        + gl_Vertex.xy * uShadowRect.zw;" << endl;
    vertSrc << "  vSampleCoords[0].st = shadowLocation;" << endl;
    for (size_t i = 1; i <= aRadius; i++) {
      vertSrc << "  vSampleCoords[" << (i / 2) << "]." << (i % 2 ? "zw" : "xy")
              << " = shadowLocation.t + vec2(" << i << ", -" << i
              << ") * uTexelHeight;" << endl;
    }
    vertSrc << "  gl_Position = vec4(shadowLocation * 2 - 1, 0, 1);" << endl;
    vertSrc << "}" << endl;

    ostringstream fragSrc;
    fragSrc << "uniform float uWeights[" << (1 + aRadius) << "];" << endl;
    fragSrc << "uniform vec4 uShadowColor;" << endl;
    fragSrc << "uniform sampler2D uHorizontalConvolution;" << endl;
    fragSrc << "varying vec4 vSampleCoords[" << (2 + aRadius) / 2 << "];" << endl;
    fragSrc << "void main()" << endl;
    fragSrc << "{" << endl;
    fragSrc << "  float alpha = uWeights[0] * texture2D(uHorizontalConvolution, vSampleCoords[0].st)."
            << channel << ";" << endl;
    for (size_t i = 1; i <= aRadius; i++) {
      fragSrc << "  alpha += uWeights[" << i << "]" << endl;
      fragSrc << "    * (texture2D(uHorizontalConvolution, vec2(vSampleCoords[0].s, vSampleCoords["
              << (i / 2) << "][" << (2 * (i % 2)) << "]))." << channel << "" << endl;
      fragSrc << "       + texture2D(uHorizontalConvolution, vec2(vSampleCoords[0].s, vSampleCoords["
              << (i / 2) << "][" << (2 * (i % 2) + 1) << "]))." << channel << ");" << endl;
    }
    fragSrc << "  gl_FragColor = alpha * uShadowColor;" << endl;
    fragSrc << "}" << endl;

    shader->Initialize(vertSrc.str().c_str(), fragSrc.str().c_str());

    return shader.forget();
  }

  UniformVec4 uShadowRect;
  UniformFloat uTexelHeight;
  UniformFloatArray uWeights;
  UniformVec4 uShadowColor;
  UniformSampler uHorizontalConvolution;

private:
  ShadowShader()
    : uShadowRect("uShadowRect")
    , uTexelHeight("uTexelHeight")
    , uWeights("uWeights")
    , uShadowColor("uShadowColor")
    , uHorizontalConvolution("uHorizontalConvolution", GL::UNIT_0)
  {}

  void Initialize(const GLchar* aVertexSource, const GLchar* aFragmentSource)
  {
    ShaderProgram::Initialize(aVertexSource, aFragmentSource);
    uShadowRect.Initialize(*this);
    uTexelHeight.Initialize(*this);
    uWeights.Initialize(*this);
    uShadowColor.Initialize(*this);
    uHorizontalConvolution.Initialize(*this);
  }
};

ShadowShaders::ShadowShaders()
  : mRadius(0)
  , mScale(1)
  , mWeightsId(0)
{
  gl->MakeCurrent();

  GLint maxFloats;
  gl->GetIntegerv(GL_MAX_VARYING_FLOATS, &maxFloats);

  mMaxRadius = (maxFloats - 2) / 2;

  mWeights.resize(1 + mMaxRadius);
  mWeights[0] = 1;
  for (size_t i = 0; i < CONVOLUTION_CHANNEL_COUNT; i++) {
    mHorizontalConvolutionShaders[i].resize(1 + mMaxRadius);
    mShadowShaders[i].resize(1 + mMaxRadius);
  }
}

ShadowShaders::~ShadowShaders()
{
}

void
ShadowShaders::ConfigureShaders(const IntSize& aFramebufferSize,
                                ConvolutionChannel aConvolutionChannel,
                                const Rect& aShadowRect, const Color& aShadowColor,
                                Float aSigma, GLuint* aHorizontalConvolutionShader,
                                GLuint* aShadowShader)
{
  MOZ_ASSERT(gl->IsCurrent());
  MOZ_ASSERT(aSigma > 0);

  size_t radius = static_cast<size_t>(3 * aSigma + 0.5f);
  float scale = 1;
  if (radius > mMaxRadius) {
    scale = static_cast<float>(radius) / mMaxRadius;
    radius = mMaxRadius;
  }

  if (mRadius != radius || mScale != scale) {
    static const float oneOverSqrt2Pi = 0.39894228f;
    const float oneOverSigma = 1 / aSigma;
    const float a = oneOverSqrt2Pi * oneOverSigma;
    const float b = -2 * oneOverSigma * oneOverSigma * mScale * mScale;

    mWeights[0] = a;
    float weightSum = mWeights[0];
    for (size_t x = 1; x <= radius; x++) {
      mWeights[x] = a * exp(x * x * b);
      weightSum += 2 * mWeights[x];
    }

    const float weightAdjust = 1 / weightSum;
    for (size_t x = 0; x <= radius; x++) {
      mWeights[x] *= weightAdjust;
    }

    mRadius = radius;
    mScale = scale;
    mWeightsId = gl->GetUniqueId();
  }

  const Size inverseFramebufferSize(1.0f / aFramebufferSize.width,
                                    1.0f / aFramebufferSize.height);
  const Size inverseShadowSize(1.0f / aShadowRect.width,
                               1.0f / aShadowRect.height);
  const Float padding = mRadius * mScale;

  if (aHorizontalConvolutionShader) {
    RefPtr<HorizontalConvolutionShader>& shader =
      mHorizontalConvolutionShaders[aConvolutionChannel][mRadius];
    if (!shader) {
      shader = HorizontalConvolutionShader::Create(aConvolutionChannel, mRadius);
    }

    Rect sampleRect(Point(), aShadowRect.Size());
    sampleRect.Inflate(padding, 2 * padding);
    sampleRect.Scale(inverseShadowSize.width, inverseShadowSize.height);

    Rect shadowRect(aShadowRect);
    shadowRect.Inflate(padding, 2 * padding);
    shadowRect.Scale(inverseFramebufferSize.width, inverseFramebufferSize.height);

    shader->uSampleRect.Set(sampleRect.TopLeft(), sampleRect.BottomRight());
    shader->uShadowRect.Set(shadowRect.TopLeft(), shadowRect.BottomRight());
    shader->uTexelWidth.Set(inverseShadowSize.width * mScale);
    shader->uWeights.Set(mWeights.data(), 1 + mRadius, mWeightsId);

    *aHorizontalConvolutionShader = *shader;
  }

  if (aShadowShader) {
    RefPtr<ShadowShader>& shader = mShadowShaders[aConvolutionChannel][mRadius];
    if (!shader) {
      shader = ShadowShader::Create(aConvolutionChannel, mRadius);
    }

    Rect shadowRect(aShadowRect);
    shadowRect.Inflate(padding);
    shadowRect.Scale(inverseFramebufferSize.width, inverseFramebufferSize.height);

    shader->uShadowRect.Set(shadowRect.TopLeft(), shadowRect.BottomRight());
    shader->uTexelHeight.Set(inverseFramebufferSize.height * mScale);
    shader->uWeights.Set(mWeights.data(), 1 + mRadius, mWeightsId);
    shader->uShadowColor.Set(aShadowColor.a * aShadowColor.r,
                             aShadowColor.a * aShadowColor.g,
                             aShadowColor.a * aShadowColor.b,
                             aShadowColor.a);

    *aShadowShader = *shader;
  }
}

}
}
}
