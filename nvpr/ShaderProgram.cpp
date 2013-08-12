/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ShaderProgram.h"
#include "Logging.h"
#include <iostream>
#include <vector>

using namespace std;

namespace mozilla {
namespace gfx {
namespace nvpr {

static GLuint compileShader(const GLchar* aSource, GLenum aType)
{
  if (!aSource) {
    return 0;
  }

  GLuint shader = gl->CreateShader(aType);
  gl->ShaderSource(shader, 1, &aSource, nullptr);
  gl->CompileShader(shader);

  GLint status = 0;
  gl->GetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (status != GL_TRUE) {
    cout << "Failed to compile nvpr shader." << endl;
    cout << "----------------------- Shader Source -----------------------" << endl;
    cout << aSource << endl;

    GLint length = 0;
    gl->GetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length) {
      vector<GLchar> infoLog(length);
      gl->GetShaderInfoLog(shader, length, nullptr, infoLog.data());

      cout << "---------------------------- Log ----------------------------" << endl;
      cout << infoLog.data() << endl;
    } else {
      cout << "No shader info log." << endl;
    }

    gl->DeleteShader(shader);
    return 0;
  }

  return shader;
}

ShaderProgram::ShaderProgram()
  : mVertexShader(0)
  , mFragmentShader(0)
  , mProgramObject(0)
{
}

ShaderProgram::~ShaderProgram()
{
  gl->MakeCurrent();

  if (mProgramObject) {
    gl->DeleteShaderProgram(mProgramObject);
  }

  if (mFragmentShader) {
    gl->DeleteShader(mFragmentShader);
  }

  if (mVertexShader) {
    gl->DeleteShader(mVertexShader);
  }
}

void
ShaderProgram::Initialize(const GLchar* aVertexSource,
                          const GLchar* aFragmentSource)
{
  MOZ_ASSERT(gl->IsCurrent());

  mVertexShader = compileShader(aVertexSource, GL_VERTEX_SHADER);
  mFragmentShader = compileShader(aFragmentSource, GL_FRAGMENT_SHADER);
  mProgramObject = gl->CreateProgram();

  if (mVertexShader) {
    gl->AttachShader(mProgramObject, mVertexShader);
  }
  if (mFragmentShader) {
    gl->AttachShader(mProgramObject, mFragmentShader);
  }
  gl->LinkProgram(mProgramObject);

  GLint status = 0;
  gl->GetProgramiv(mProgramObject, GL_LINK_STATUS, &status);
  if (status != GL_TRUE) {
    cout << "Failed to link nvpr program." << endl;
    cout << "----------------------- Vertex Source -----------------------" << endl;
    cout << aVertexSource << endl;
    cout << "----------------------- Fragment Source -----------------------" << endl;
    cout << aFragmentSource << endl;

    GLint length = 0;
    gl->GetProgramiv(mProgramObject, GL_INFO_LOG_LENGTH, &length);
    if (length) {
      vector<GLchar> infoLog(length);
      gl->GetProgramInfoLog(mProgramObject, length, nullptr, infoLog.data());

      cout << "---------------------------- Log ----------------------------" << endl;
      cout << infoLog.data() << endl;
    } else {
      cout << "No shader info log." << endl;
    }

    gl->DeleteShaderProgram(mProgramObject);
    mProgramObject = 0;
  }
}

Uniform::Uniform(const char* aName)
  : mName(aName)
  , mLocation(0)
{
}

void
Uniform::Initialize(GLuint aShaderProgram)
{
  mShaderProgram = aShaderProgram;
  mLocation = gl->GetUniformLocation(aShaderProgram, mName.c_str());
}

std::ostream& operator<<(std::ostream& aOut, const Uniform& aUniform)
{
  return aOut << aUniform.mName;
}

UniformFloat::UniformFloat(const char* aName)
  : Uniform(aName)
  , mValue(0)
{
}

void
UniformFloat::Set(GLfloat aValue)
{
  if (mValue == aValue) {
    return;
  }

  gl->ProgramUniform1fEXT(mShaderProgram, mLocation, aValue);
  mValue = aValue;
}

UniformFloatArray::UniformFloatArray(const char* aName)
  : Uniform(aName)
  , mArrayId(0)
{
}

void
UniformFloatArray::Set(GLfloat* aValues, GLsizei aCount, UniqueId aArrayId)
{
  if (mArrayId == aArrayId) {
    return;
  }

  gl->ProgramUniform1fvEXT(mShaderProgram, mLocation, aCount, aValues);
  mArrayId = aArrayId;
}

UniformVec2::UniformVec2(const char* aName)
  : Uniform(aName)
{
  memset(mValues, 0, sizeof(mValues));
}

void
UniformVec2::Set(GLfloat aX, GLfloat aY)
{
  const GLfloat values[] = {aX, aY};
  Set(values);
}

void
UniformVec2::Set(const Point& aXY)
{
  const GLfloat values[] = {aXY.x, aXY.y};
  Set(values);
}

void
UniformVec2::Set(const GLfloat* aValues)
{
  if (!memcmp(mValues, aValues, sizeof(mValues))) {
    return;
  }

  gl->ProgramUniform2fvEXT(mShaderProgram, mLocation, 1, aValues);
  memcpy(mValues, aValues, sizeof(mValues));
}

UniformVec4::UniformVec4(const char* aName)
  : Uniform(aName)
{
  memset(mValues, 0, sizeof(mValues));
}

void
UniformVec4::Set(GLfloat aX, GLfloat aY, GLfloat aZ, GLfloat aW)
{
  const GLfloat values[] = {aX, aY, aZ, aW};
  Set(values);
}

void
UniformVec4::Set(const Point& aXY, const Point& aZW)
{
  const GLfloat values[] = {aXY.x, aXY.y, aZW.x, aZW.y};
  Set(values);
}

void
UniformVec4::Set(const GLfloat* aValues)
{
  if (!memcmp(mValues, aValues, sizeof(mValues))) {
    return;
  }

  gl->ProgramUniform4fvEXT(mShaderProgram, mLocation, 1, aValues);
  memcpy(mValues, aValues, sizeof(mValues));
}

UniformSampler::UniformSampler(const char* aName, GL::TextureUnit aTextureUnit)
  : Uniform(aName)
  , mTextureUnit(aTextureUnit)
{
}

void
UniformSampler::Initialize(GLuint aShaderProgram)
{
  Uniform::Initialize(aShaderProgram);
  gl->ProgramUniform1iEXT(aShaderProgram, mLocation, mTextureUnit);
}

}
}
}
