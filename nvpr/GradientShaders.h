/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_NVPR_GRADIENTSHADERS_H_
#define MOZILLA_GFX_NVPR_GRADIENTSHADERS_H_

#include "2D.h"
#include "GL.h"
#include <GL/gl.h>

static const GLchar kFocalGradCenteredShaderSource[] = "\
uniform sampler1D uColorRamp;                             \n\
                                                          \n\
void main(void) {                                         \n\
  vec2 p = gl_TexCoord[0].st;                             \n\
  float offset = length(p);                               \n\
  gl_FragColor = gl_Color * texture(uColorRamp, offset);  \n\
}                                                         \n\
";

static const GLchar kFocalGradInsideShaderSource[] = "\
uniform float uFocalX;                                    \n\
uniform sampler1D uColorRamp;                             \n\
                                                          \n\
void main(void) {                                         \n\
  vec2 q = gl_TexCoord[0].st;                             \n\
  float offset = q.x * uFocalX + length(q);               \n\
  gl_FragColor = gl_Color * texture(uColorRamp, offset);  \n\
}                                                         \n\
";

static const GLchar kFocalGradOutsideShaderSource[] = "\
uniform float uFocalX;                                                       \n\
uniform float u1MinusFx_2;                                                   \n\
uniform sampler1D uColorRamp;                                                \n\
                                                                             \n\
void main(void) {                                                            \n\
  vec2 q = gl_TexCoord[0].st;                                                \n\
  float offset = q.x * uFocalX + sqrt(q.x * q.x + u1MinusFx_2 * q.y * q.y);  \n\                            \n\
  gl_FragColor = offset >= 0 ? gl_Color * texture(uColorRamp, offset) : 0;   \n\
}                                                                            \n\
";

static const GLchar kFocalGradTouchingShaderSource[] = "\
uniform sampler1D uColorRamp;                                                   \n\
                                                                                \n\
void main(void) {                                                               \n\
  vec2 q = gl_TexCoord[0].st;                                                   \n\
  float offset = dot(q, q) / (-2 * q.x);                                        \n\
  gl_FragColor = (offset >= 0) ? (gl_Color * texture(uColorRamp, offset)) : 0;  \n\
}                                                                               \n\
";

static const GLchar kRadialGradInsideShaderSource[] = "\
uniform sampler1D uColorRamp;                             \n\
uniform vec2 uEndCenter;                                  \n\
uniform float uA;                                         \n\
uniform float uB;                                         \n\
uniform float uC;                                         \n\
                                                          \n\
void main(void) {                                         \n\
  vec2 q = gl_TexCoord[0].st;                             \n\
  float d = dot(uEndCenter, q) + uB;                      \n\
#ifdef SUBTRACT_SQRT                                      \n\
  float offset = d - sqrt(d * d - uA * dot(q, q) + uC);   \n\
#else                                                     \n\
  float offset = d + sqrt(d * d - uA * dot(q, q) + uC);   \n\
#endif                                                    \n\
  gl_FragColor = gl_Color * texture(uColorRamp, offset);  \n\
}                                                         \n\
";

static const GLchar kRadialGradOutsideShaderSource[] = "\
uniform sampler1D uColorRamp;                                                              \n\
uniform vec2 uEndCenter;                                                                   \n\
uniform float uA;                                                                          \n\
uniform float uB;                                                                          \n\
uniform float uC;                                                                          \n\
uniform float uOffsetBound;                                                                \n\
                                                                                           \n\
void main(void) {                                                                          \n\
  vec2 q = gl_TexCoord[0].st;                                                              \n\
  float d = dot(uEndCenter, q) + uB;                                                       \n\
  float offset = d + sqrt(d * d - uA * dot(q, q) + uC);                                    \n\
#ifdef DISCARD_HIGH                                                                        \n\
  gl_FragColor = (offset <= uOffsetBound) ? (gl_Color * texture(uColorRamp, offset)) : 0;  \n\
#else                                                                                      \n\
  gl_FragColor = (offset >= uOffsetBound) ? (gl_Color * texture(uColorRamp, offset)) : 0;  \n\
#endif                                                                                     \n\
}                                                                                          \n\
";

namespace mozilla {
namespace gfx {
namespace nvpr {

class GradientShader {
public:
  GradientShader(const GLchar* aFragShaderSource,
                 const GLchar* aDefines = "")
    : mShaderProgram(0)
    , mFragShader(0)
  {
    mFragShaderSources[0] = aDefines;
    mFragShaderSources[1] = aFragShaderSource;
  }
  virtual ~GradientShader() {}

  operator GLuint()
  {
    MOZ_ASSERT(gl->IsCurrent());

    if (mShaderProgram) {
      return mShaderProgram;
    }

    mFragShader = gl->CreateShader(GL_FRAGMENT_SHADER);
    gl->ShaderSource(mFragShader, 2, mFragShaderSources, nullptr);
    gl->CompileShader(mFragShader);

    mShaderProgram = gl->CreateProgram();
    gl->AttachShader(mShaderProgram, mFragShader);
    gl->LinkProgram(mShaderProgram);

    return mShaderProgram;
  }

private:
  GradientShader(const GradientShader&) {}
  GradientShader& operator=(const GradientShader&) {}

  const GLchar* mFragShaderSources[2];
  GLuint mShaderProgram;
  GLuint mFragShader;
};

class Uniform {
public:
  Uniform(GradientShader* aShader, const GLchar* aName)
    : mShader(aShader)
    , mName(aName)
    , mLocation(-1)
  {}

protected:
  GradientShader* Shader() const { return mShader; }

  GLint Location()
  {
    if (mLocation == -1) {
      mLocation = gl->GetUniformLocation(*mShader, mName);
    }

    return mLocation;
  }

private:
  GradientShader* const mShader;
  const GLchar* const mName;
  GLint mLocation;
};

class UniformFloat : public Uniform {
public:
  UniformFloat(GradientShader* aShader, const GLchar* aName)
    : Uniform(aShader, aName)
    , mValue(0)
  {}

  void operator =(GLfloat aValue)
  {
    if (mValue == aValue) {
      return;
    }

    gl->Uniform1f(Location(), aValue);
    mValue = aValue;
  }

private:
  GLfloat mValue;
};

class UniformVec2 : public Uniform {
public:
  UniformVec2(GradientShader* aShader, const GLchar* aName)
    : Uniform(aShader, aName)
  {}

  void operator =(const Point& aValue)
  {
    if (mValue == aValue) {
      return;
    }

    gl->Uniform2f(Location(), aValue.x, aValue.y);
    mValue = aValue;
  }

private:
  Point mValue;
};

class FocalGradInsideShader : public GradientShader {
public:
  FocalGradInsideShader(const GLchar* aFragShaderSource)
    : GradientShader(aFragShaderSource)
    , uFocalX(this, "uFocalX")
  {}

  UniformFloat uFocalX;
};

class FocalGradOutsideShader : public GradientShader {
public:
  FocalGradOutsideShader(const GLchar* aFragShaderSource)
    : GradientShader(aFragShaderSource)
    , uFocalX(this, "uFocalX")
    , u1MinusFx_2(this, "u1MinusFx_2")
  {}

  UniformFloat uFocalX;
  UniformFloat u1MinusFx_2;
};

class RadialGradInsideShader : public GradientShader {
public:
  RadialGradInsideShader(const GLchar* aFragShaderSource,
                         const GLchar* aDefines = "")
    : GradientShader(aFragShaderSource, aDefines)
    , uEndCenter(this, "uEndCenter")
    , uA(this, "uA")
    , uB(this, "uB")
    , uC(this, "uC")
  {}

  UniformVec2 uEndCenter;
  UniformFloat uA;
  UniformFloat uB;
  UniformFloat uC;
};

class RadialGradOutsideShader : public GradientShader {
public:
  RadialGradOutsideShader(const GLchar* aFragShaderSource,
                          const GLchar* aDefines = "")
    : GradientShader(aFragShaderSource, aDefines)
    , uEndCenter(this, "uEndCenter")
    , uA(this, "uA")
    , uB(this, "uB")
    , uC(this, "uC")
    , uOffsetBound(this, "uOffsetBound")
  {}

  UniformVec2 uEndCenter;
  UniformFloat uA;
  UniformFloat uB;
  UniformFloat uC;
  UniformFloat uOffsetBound;
};

struct GradientShaders : public UserData::Object {
  GradientShaders()
    : mFocalGradCenteredShader(kFocalGradCenteredShaderSource)
    , mFocalGradInsideShader(kFocalGradInsideShaderSource)
    , mFocalGradOutsideShader(kFocalGradOutsideShaderSource)
    , mFocalGradTouchingShader(kFocalGradTouchingShaderSource)
    , mRadialGradInsideShaderAddSqrt(kRadialGradInsideShaderSource)
    , mRadialGradInsideShaderSubSqrt(kRadialGradInsideShaderSource, "#define SUBTRACT_SQRT\n")
    , mRadialGradOutsideShaderDiscardLo(kRadialGradOutsideShaderSource)
    , mRadialGradOutsideShaderDiscardHi(kRadialGradOutsideShaderSource, "#define DISCARD_HIGH\n")
  {}

  GradientShader mFocalGradCenteredShader;
  FocalGradInsideShader mFocalGradInsideShader;
  FocalGradOutsideShader mFocalGradOutsideShader;
  GradientShader mFocalGradTouchingShader;
  RadialGradInsideShader mRadialGradInsideShaderAddSqrt;
  RadialGradInsideShader mRadialGradInsideShaderSubSqrt;
  RadialGradOutsideShader mRadialGradOutsideShaderDiscardLo;
  RadialGradOutsideShader mRadialGradOutsideShaderDiscardHi;
};

}
}
}

#endif /* MOZILLA_GFX_NVPR_GRADIENTSHADERS_H_ */
