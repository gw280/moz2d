/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Shader.h"
#include "Logging.h"
#include <iostream>
#include <sstream>
#include <vector>

using namespace std;

namespace mozilla {
namespace gfx {
namespace nvpr {

static string
AppendTextureUnit(const char* aName, GL::TextureUnit aUnit)
{
  stringstream fullName;
  fullName << aName << '_' << aUnit;
  return fullName.str();
}


class Uniform {
public:
  friend ostream& operator<<(ostream& aOut, const Uniform& aUniform);

  void Initialize(GLuint aShaderProgram)
  {
    mShaderProgram = aShaderProgram;
    mLocation = gl->GetUniformLocation(aShaderProgram, mName.c_str());
  }

protected:
  Uniform(string& aPassName)
    : mLocation(0)
  {
    swap(mName, aPassName);
  }
  Uniform(string&& aPassName)
    : mLocation(0)
  {
    swap(mName, aPassName);
  }

  string mName;
  GLuint mShaderProgram;
  GLint mLocation;
};

ostream& operator<<(ostream& aOut, const Uniform& aUniform)
{
  return aOut << aUniform.mName;
}

class UniformFloat : public Uniform {
public:
  UniformFloat(string&& aName)
    : Uniform(aName)
    , mValue(0)
  {}

  void WriteDeclaration(ostream& aOut) const
  {
    aOut << "uniform float " << *this << ";" << endl;
  }

  void Set(GLfloat aValue)
  {
    if (mValue == aValue) {
      return;
    }

    gl->ProgramUniform1fEXT(mShaderProgram, mLocation, aValue);
    mValue = aValue;
  }

private:
  GLfloat mValue;
};

template<size_t N> struct SetUniformFloat;
template<> struct SetUniformFloat<2> {
  static void Set(GLuint aProg, GLint aLoc, const GLfloat* aVal)
  {
    gl->ProgramUniform2fvEXT(aProg, aLoc, 1, aVal);
  }
};
template<> struct SetUniformFloat<4> {
  static void Set(GLuint aProg, GLint aLoc, const GLfloat* aVal)
  {
    gl->ProgramUniform4fvEXT(aProg, aLoc, 1, aVal);
  }
};
template<size_t N> class UniformVec : public Uniform {
public:
  UniformVec(string&& aName)
    : Uniform(aName)
  {
    memset(mValues, 0, sizeof(mValues));
  }

  void WriteDeclaration(ostream& aOut) const
  {
    aOut << "uniform vec" << N << " " << *this << ";" << endl;
  }

  void Set(const GLfloat* aValues)
  {
    if (!memcmp(mValues, aValues, sizeof(mValues))) {
      return;
    }

    SetUniformFloat<N>::Set(mShaderProgram, mLocation, aValues);
    memcpy(mValues, aValues, sizeof(mValues));
  }

private:
  GLfloat mValues[N];
};

template<size_t D>
class UniformSampler : public Uniform {
public:
  UniformSampler(GL::TextureUnit aTextureUnit)
    : Uniform(AppendTextureUnit("uTexture", aTextureUnit))
    , mTextureUnit(aTextureUnit)
  {}

  void WriteDeclaration(ostream& aOut) const
  {
    aOut << "uniform sampler" << D << "D " << *this << ";" << endl;
  }

  void Initialize(GLuint aShaderProgram)
  {
    Uniform::Initialize(aShaderProgram);
    gl->ProgramUniform1iEXT(aShaderProgram, mLocation, mTextureUnit);
  }

protected:
  GL::TextureUnit mTextureUnit;
};


class Paint : public RefCounted<Paint> {
public:
  static TemporaryRef<Paint> Create(PaintConfig::PaintMode aPaintMode,
                                    GL::TextureUnit aTextureUnit);
  virtual ~Paint() {}

  virtual bool IsEmpty() const { return false; }

  virtual void WriteDeclarations(std::ostream& aOut) const {}
  virtual void WritePaintFunction(std::ostream& aOut) const = 0;
  virtual void Initialize(GLuint aShaderProgram) {}

  virtual void ApplyFragmentUniforms(const PaintConfig& aConfig) {}

protected:
  Paint() {}
};

class EmptyPaint : public Paint {
public:
  virtual bool IsEmpty() const { return true; }

  virtual void WritePaintFunction(std::ostream& aOut) const
  {
    aOut << "return vec4(1);" << endl;
  }
};

class SolidColorPaint : public Paint {
public:
  SolidColorPaint(GL::TextureUnit aTextureUnit)
    : uColor(AppendTextureUnit("uColor", aTextureUnit))
  {}

  virtual void WriteDeclarations(std::ostream& aOut) const
  {
    uColor.WriteDeclaration(aOut);
  }

  virtual void WritePaintFunction(std::ostream& aOut) const
  {
    aOut << "return " << uColor << ";" << endl;
  }

  virtual void Initialize(GLuint aShaderProgram)
  {
    uColor.Initialize(aShaderProgram);
  }

  virtual void ApplyFragmentUniforms(const PaintConfig& aConfig)
  {
    const GLfloat* color = aConfig.uColor;

    if (color[3] == 1) {
      uColor.Set(color);
    } else {
      const float a = color[3];
      const float premultiplied[] = {a * color[0], a * color[1], a * color[2], a};
      uColor.Set(premultiplied);
    }
  }

protected:
  UniformVec<4> uColor;
};

class Texture1DPaint : public Paint {
public:
  Texture1DPaint(GL::TextureUnit aTextureUnit)
    : mTextureUnit(aTextureUnit)
    , uTexture(aTextureUnit)
  {}

  virtual void WriteDeclarations(std::ostream& aOut) const
  {
    uTexture.WriteDeclaration(aOut);
  }

  virtual void WritePaintFunction(std::ostream& aOut) const
  {
    aOut << "float texCoord = gl_TexCoord[" << mTextureUnit << "].s;" << endl;
    aOut << "return texture1D(" << uTexture << ", texCoord);" << endl;
  }

  virtual void Initialize(GLuint aShaderProgram)
  {
    uTexture.Initialize(aShaderProgram);
  }

  virtual void ApplyFragmentUniforms(const PaintConfig& aConfig)
  {
    gl->SetTexture(mTextureUnit, GL_TEXTURE_1D, aConfig.mTextureId);
  }

protected:
  GL::TextureUnit mTextureUnit;
  UniformSampler<1> uTexture;
};

class Texture2DPaint : public Paint {
public:
  Texture2DPaint(GL::TextureUnit aTextureUnit)
    : mTextureUnit(aTextureUnit)
    , uTexture(aTextureUnit)
  {}

  virtual void WriteDeclarations(std::ostream& aOut) const
  {
    uTexture.WriteDeclaration(aOut);
  }

  virtual void WritePaintFunction(std::ostream& aOut) const
  {
    aOut << "vec2 texCoords = gl_TexCoord[" << mTextureUnit << "].st;" << endl;
    aOut << "return texture2D(" << uTexture << ", texCoords);" << endl;
  }

  virtual void Initialize(GLuint aShaderProgram)
  {
    uTexture.Initialize(aShaderProgram);
  }

  virtual void ApplyFragmentUniforms(const PaintConfig& aConfig)
  {
    gl->SetTexture(mTextureUnit, GL_TEXTURE_2D, aConfig.mTextureId);
  }

protected:
  GL::TextureUnit mTextureUnit;
  UniformSampler<2> uTexture;
};

class Texture2DClampedPaint : public Texture2DPaint {
public:
  Texture2DClampedPaint(GL::TextureUnit aTextureUnit)
    : Texture2DPaint(aTextureUnit)
    , uClampRect(AppendTextureUnit("uClampRect", aTextureUnit))
  {}

  virtual void WriteDeclarations(std::ostream& aOut) const
  {
    Texture2DPaint::WriteDeclarations(aOut);
    uClampRect.WriteDeclaration(aOut);
  }

  virtual void WritePaintFunction(std::ostream& aOut) const
  {
    aOut << "vec2 texCoords = clamp(gl_TexCoord[" << mTextureUnit << "].st, "
                                    << uClampRect << ".xy, "
                                    << uClampRect << ".zw);" << endl;
    aOut << "return texture2D(" << uTexture << ", texCoords);" << endl;
  }

  virtual void Initialize(GLuint aShaderProgram)
  {
    uClampRect.Initialize(aShaderProgram);
  }

  virtual void ApplyFragmentUniforms(const PaintConfig& aConfig)
  {
    Texture2DPaint::ApplyFragmentUniforms(aConfig);
    uClampRect.Set(aConfig.uClampRect);
  }

protected:
  UniformVec<4> uClampRect;
};

class FocalGradCenteredPaint : public Texture1DPaint {
public:
  FocalGradCenteredPaint(GL::TextureUnit aTextureUnit)
    : Texture1DPaint(aTextureUnit)
  {}

  virtual void WritePaintFunction(std::ostream& aOut) const
  {
    aOut << "vec2 p = gl_TexCoord[" << mTextureUnit << "].st;" << endl;
    aOut << "float offset = length(p);" << endl;
    aOut << "return texture1D(" << uTexture << ", offset);" << endl;
  }
};

class FocalGradInsidePaint : public Texture1DPaint {
public:
  FocalGradInsidePaint(GL::TextureUnit aTextureUnit)
    : Texture1DPaint(aTextureUnit)
    , uFocalX(AppendTextureUnit("uFocalX", aTextureUnit))
  {}

  virtual void WriteDeclarations(std::ostream& aOut) const
  {
    Texture1DPaint::WriteDeclarations(aOut);
    uFocalX.WriteDeclaration(aOut);
  }

  virtual void WritePaintFunction(std::ostream& aOut) const
  {
    aOut << "vec2 q = gl_TexCoord[" << mTextureUnit << "].st;" << endl;
    aOut << "float offset = q.x * " << uFocalX << " + length(q);" << endl;
    aOut << "return texture1D(" << uTexture << ", offset);" << endl;
  }

  virtual void Initialize(GLuint aShaderProgram)
  {
    uFocalX.Initialize(aShaderProgram);
  }

  virtual void ApplyFragmentUniforms(const PaintConfig& aConfig)
  {
    Texture1DPaint::ApplyFragmentUniforms(aConfig);
    uFocalX.Set(aConfig.uFocalX);
  }

protected:
  UniformFloat uFocalX;
};

class FocalGradOutsidePaint : public Texture1DPaint {
public:
  FocalGradOutsidePaint(GL::TextureUnit aTextureUnit)
    : Texture1DPaint(aTextureUnit)
    , uFocalX(AppendTextureUnit("uFocalX", aTextureUnit))
    , u1MinusFx_2(AppendTextureUnit("u1MinusFx_2", aTextureUnit))
  {}

  virtual void WriteDeclarations(std::ostream& aOut) const
  {
    Texture1DPaint::WriteDeclarations(aOut);
    uFocalX.WriteDeclaration(aOut);
    u1MinusFx_2.WriteDeclaration(aOut);
  }

  virtual void WritePaintFunction(std::ostream& aOut) const
  {
    aOut << "vec2 q = gl_TexCoord[" << mTextureUnit << "].st;" << endl;
    aOut << "float offset = q.x * " << uFocalX << " + sqrt(q.x * q.x + " << u1MinusFx_2 << " * q.y * q.y);" << endl;
    aOut << "return offset >= 0 ? texture1D(" << uTexture << ", offset) : 0;" << endl;
  }

  virtual void Initialize(GLuint aShaderProgram)
  {
    uFocalX.Initialize(aShaderProgram);
    u1MinusFx_2.Initialize(aShaderProgram);
  }

  virtual void ApplyFragmentUniforms(const PaintConfig& aConfig)
  {
    Texture1DPaint::ApplyFragmentUniforms(aConfig);
    uFocalX.Set(aConfig.uFocalX);
    u1MinusFx_2.Set(aConfig.u1MinuxFx_2);
  }

protected:
  UniformFloat uFocalX;
  UniformFloat u1MinusFx_2;
};

class FocalGradTouchingPaint : public Texture1DPaint {
public:
  FocalGradTouchingPaint(GL::TextureUnit aTextureUnit)
    : Texture1DPaint(aTextureUnit)
  {}

  virtual void WritePaintFunction(std::ostream& aOut) const
  {
    aOut << "vec2 p = gl_TexCoord[" << mTextureUnit << "].st;" << endl;
    aOut << "float offset = dot(q, q) / (-2 * q.x);" << endl;
    aOut << "return offset >= 0 ? texture1D(" << uTexture << ", offset) : 0;" << endl;
  }
};

class RadialGradInsidePaint : public Texture1DPaint {
public:
  RadialGradInsidePaint(GL::TextureUnit aTextureUnit,
                        PaintConfig::PaintMode aPaintMode)
    : Texture1DPaint(aTextureUnit)
    , mPaintMode(aPaintMode)
    , uEndCenter(AppendTextureUnit("uEndCenter", aTextureUnit))
    , uA(AppendTextureUnit("uA", aTextureUnit))
    , uB(AppendTextureUnit("uB", aTextureUnit))
    , uC(AppendTextureUnit("uC", aTextureUnit))
  {}

  virtual void WriteDeclarations(std::ostream& aOut) const
  {
    Texture1DPaint::WriteDeclarations(aOut);
    uEndCenter.WriteDeclaration(aOut);
    uA.WriteDeclaration(aOut);
    uB.WriteDeclaration(aOut);
    uC.WriteDeclaration(aOut);
  }

  virtual void WritePaintFunction(std::ostream& aOut) const
  {
    const char sign =
      mPaintMode == PaintConfig::MODE_RADIAL_GRAD_INSIDE_SUBTRACT_SQRT ? '-' : '+';
    aOut << "vec2 q = gl_TexCoord[" << mTextureUnit << "].st;" << endl;
    aOut << "float d = dot(" << uEndCenter << ", q) + " << uB << ";" << endl;
    aOut << "float offset = d " << sign << " sqrt(d * d - " << uA << " * dot(q, q) + " << uC << ");" << endl;
    aOut << "return texture1D(" << uTexture << ", offset);" << endl;
  }

  virtual void Initialize(GLuint aShaderProgram)
  {
    uEndCenter.Initialize(aShaderProgram);
    uA.Initialize(aShaderProgram);
    uC.Initialize(aShaderProgram);
    uB.Initialize(aShaderProgram);
  }

  virtual void ApplyFragmentUniforms(const PaintConfig& aConfig)
  {
    Texture1DPaint::ApplyFragmentUniforms(aConfig);
    uEndCenter.Set(aConfig.uEndCenter);
    uA.Set(aConfig.uA);
    uC.Set(aConfig.uB);
    uB.Set(aConfig.uC);
  }

protected:
  PaintConfig::PaintMode mPaintMode;
  UniformVec<2> uEndCenter;
  UniformFloat uA;
  UniformFloat uB;
  UniformFloat uC;
};

class RadialGradOutsidePaint : public Texture1DPaint {
public:
  RadialGradOutsidePaint(GL::TextureUnit aTextureUnit,
                         PaintConfig::PaintMode aPaintMode)
    : Texture1DPaint(aTextureUnit)
    , mPaintMode(aPaintMode)
    , uEndCenter(AppendTextureUnit("uEndCenter", aTextureUnit))
    , uA(AppendTextureUnit("uA", aTextureUnit))
    , uB(AppendTextureUnit("uB", aTextureUnit))
    , uC(AppendTextureUnit("uC", aTextureUnit))
    , uOffsetLimit(AppendTextureUnit("uOffsetLimit", aTextureUnit))
  {}

  virtual void WriteDeclarations(std::ostream& aOut) const
  {
    Texture1DPaint::WriteDeclarations(aOut);
    uEndCenter.WriteDeclaration(aOut);
    uA.WriteDeclaration(aOut);
    uB.WriteDeclaration(aOut);
    uC.WriteDeclaration(aOut);
    uOffsetLimit.WriteDeclaration(aOut);
  }

  virtual void WritePaintFunction(std::ostream& aOut) const
  {
    const char* op =
      mPaintMode == PaintConfig::MODE_RADIAL_GRAD_OUTSIDE_DISCARD_HIGH ? "<=" : ">=";
    aOut << "vec2 q = gl_TexCoord[" << mTextureUnit << "].st;" << endl;
    aOut << "float d = dot(" << uEndCenter << ", q) + " << uB << ";" << endl;
    aOut << "float offset = d + sqrt(d * d - " << uA << " * dot(q, q) + " << uC << ");" << endl;
    aOut << "return offset " << op << uOffsetLimit << " ? texture1D(" << uTexture << ", offset) : 0;" << endl;
  }

  virtual void Initialize(GLuint aShaderProgram)
  {
    uEndCenter.Initialize(aShaderProgram);
    uA.Initialize(aShaderProgram);
    uC.Initialize(aShaderProgram);
    uB.Initialize(aShaderProgram);
    uOffsetLimit.Initialize(aShaderProgram);
  }

  virtual void ApplyFragmentUniforms(const PaintConfig& aConfig)
  {
    Texture1DPaint::ApplyFragmentUniforms(aConfig);
    uEndCenter.Set(aConfig.uEndCenter);
    uA.Set(aConfig.uA);
    uC.Set(aConfig.uB);
    uB.Set(aConfig.uC);
    uOffsetLimit.Set(aConfig.uOffsetLimit);
  }

protected:
  PaintConfig::PaintMode mPaintMode;
  UniformVec<2> uEndCenter;
  UniformFloat uA;
  UniformFloat uB;
  UniformFloat uC;
  UniformFloat uOffsetLimit;
};

TemporaryRef<Paint>
Paint::Create(PaintConfig::PaintMode aPaintMode, GL::TextureUnit aTextureUnit)
{
  switch (aPaintMode) {
    default:
    case PaintConfig::MODE_NONE:
      return new EmptyPaint();
    case PaintConfig::MODE_SOLID_COLOR:
      return new SolidColorPaint(aTextureUnit);
    case PaintConfig::MODE_TEXTURE_1D:
      return new Texture1DPaint(aTextureUnit);
    case PaintConfig::MODE_TEXTURE_2D:
      return new Texture2DPaint(aTextureUnit);
    case PaintConfig::MODE_TEXTURE_2D_CLAMPED:
      return new Texture2DClampedPaint(aTextureUnit);
    case PaintConfig::MODE_FOCAL_GRAD_CENTERED:
      return new FocalGradCenteredPaint(aTextureUnit);
    case PaintConfig::MODE_FOCAL_GRAD_INSIDE:
      return new FocalGradInsidePaint(aTextureUnit);
    case PaintConfig::MODE_FOCAL_GRAD_OUTSIDE:
      return new FocalGradOutsidePaint(aTextureUnit);
    case PaintConfig::MODE_FOCAL_GRAD_TOUCHING:
      return new FocalGradTouchingPaint(aTextureUnit);
    case PaintConfig::MODE_RADIAL_GRAD_INSIDE_ADD_SQRT:
    case PaintConfig::MODE_RADIAL_GRAD_INSIDE_SUBTRACT_SQRT:
      return new RadialGradInsidePaint(aTextureUnit, aPaintMode);
    case PaintConfig::MODE_RADIAL_GRAD_OUTSIDE_DISCARD_HIGH:
    case PaintConfig::MODE_RADIAL_GRAD_OUTSIDE_DISCARD_LOW:
      return new RadialGradOutsidePaint(aTextureUnit, aPaintMode);
  }
}


class AlphaShader : public Shader {
public:
  AlphaShader(TemporaryRef<Paint> aPaint, TemporaryRef<Paint> aMask)
    : Shader(aPaint, aMask)
    , uGlobalAlpha("uGlobalAlpha")
  {}

  virtual void ApplyFragmentUniforms(const GL::ShaderConfig& aConfig)
  {
    Shader::ApplyFragmentUniforms(aConfig);
    uGlobalAlpha.Set(aConfig.mGlobalAlpha);
  }

protected:
  virtual void WriteDeclarations(std::ostream& aOut) const
  {
    Shader::WriteDeclarations(aOut);
    uGlobalAlpha.WriteDeclaration(aOut);
  }

  virtual void WriteMainFunction(std::ostream& aOut) const
  {
    Shader::WriteMainFunction(aOut);
    aOut << "gl_FragColor *= " << uGlobalAlpha << ";" << endl;
  }

  virtual void Initialize()
  {
    Shader::Initialize();
    uGlobalAlpha.Initialize(mShaderProgram);
  }

  UniformFloat uGlobalAlpha;
};

Shader::Shader(TemporaryRef<Paint> aPaint, TemporaryRef<Paint> aMask)
  : mFragShader(0)
  , mShaderProgram(0)
  , mPaint(aPaint)
  , mMask(aMask)
{}

Shader::~Shader()
{
  gl->MakeCurrent();

  if (mFragShader) {
    gl->DeleteShader(mFragShader);
  }

  if (mShaderProgram) {
    gl->DeleteShaderProgram(mShaderProgram);
  }
}

void
Shader::WriteDeclarations(ostream& aOut) const
{
  mPaint->WriteDeclarations(aOut);
  mMask->WriteDeclarations(aOut);
}

void
Shader::WriteMainFunction(ostream& aOut) const
{
  aOut << "gl_FragColor = GetPaintColor();" << endl;
  if (!mMask->IsEmpty()) {
    aOut << "gl_FragColor *= GetMaskColor().a;" << endl;
  }
}

void
Shader::ApplyFragmentUniforms(const GL::ShaderConfig& aConfig)
{
  MOZ_ASSERT(gl->IsCurrent());

  mPaint->ApplyFragmentUniforms(aConfig.mPaintConfig);
  mMask->ApplyFragmentUniforms(aConfig.mMaskConfig);
}

void
Shader::Initialize()
{
  MOZ_ASSERT(gl->IsCurrent());

  ostringstream fragSource;
  WriteDeclarations(fragSource);
  fragSource << endl;

  fragSource << "vec4 GetPaintColor() {" << endl;
  mPaint->WritePaintFunction(fragSource);
  fragSource << "}" << endl;
  fragSource << endl;

  if (!mMask->IsEmpty()) {
    fragSource << "vec4 GetMaskColor() {" << endl;
    mMask->WritePaintFunction(fragSource);
    fragSource << "}" << endl;
    fragSource << endl;
  }

  fragSource << "void main(void) {" << endl;
  WriteMainFunction(fragSource);
  fragSource << "}" << endl;

  const string& sourceString = fragSource.str();
  const GLchar* sourceCString = sourceString.c_str();
  mFragShader = gl->CreateShader(GL_FRAGMENT_SHADER);
  gl->ShaderSource(mFragShader, 1, &sourceCString, nullptr);
  gl->CompileShader(mFragShader);

  GLint status = 0;
  gl->GetShaderiv(mFragShader, GL_COMPILE_STATUS, &status);
  if (status != GL_TRUE) {
    gfxWarning() << "Failed to compile nvpr fragment shader.";
    gfxWarning() << "----------------------- Shader Source -----------------------";
    gfxWarning() << sourceString;

    GLint length = 0;
    gl->GetShaderiv(mFragShader, GL_INFO_LOG_LENGTH, &length);
    if (!length) {
      gfxWarning() << "No shader info log.";
      return;
    }

    vector<GLchar> infoLog(length);
    gl->GetShaderInfoLog(mFragShader, length, nullptr, infoLog.data());

    gfxWarning() << "---------------------------- Log ----------------------------";
    gfxWarning() << infoLog.data();
    return;
  }

  mShaderProgram = gl->CreateProgram();
  gl->AttachShader(mShaderProgram, mFragShader);
  gl->LinkProgram(mShaderProgram);

  // Link should always succeed since we use the fixed-function vertex shader.
  gl->GetProgramiv(mShaderProgram, GL_LINK_STATUS, &status);
  MOZ_ASSERT(status == GL_TRUE);

  mPaint->Initialize(mShaderProgram);
  mMask->Initialize(mShaderProgram);
}

TemporaryRef<Shader>
Shader::Create(bool aHasAlpha, PaintConfig::PaintMode aPaintMode,
               PaintConfig::PaintMode aMaskMode)
{
  RefPtr<Paint> paint = Paint::Create(aPaintMode, GL::PAINT_UNIT);
  RefPtr<Paint> mask = Paint::Create(aMaskMode, GL::MASK_UNIT);

  RefPtr<Shader> shader = aHasAlpha
    ? new AlphaShader(paint.forget(), mask.forget())
    : new Shader(paint.forget(), mask.forget());

  shader->Initialize();

  return shader.forget();
}

}
}
}
