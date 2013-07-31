/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GL.h"
#include "ConvexPolygon.h"
#include "Line.h"
#include "Logging.h"
#include "Shader.h"
#include <iterator>
#include <iostream>
#include <sstream>
#include <string>

using namespace std;

#ifdef WIN32
#define STDCALL __stdcall
#else
#define STDCALL
#endif

namespace mozilla {
namespace gfx {

static void STDCALL GLDebugCallback(GLenum aSource, GLenum aType, GLuint aId,
                                    GLenum aSeverity, GLsizei aLength,
                                    const GLchar* aMessage,
                                    const void* aUserParam)
{
  if (aSeverity == GL_DEBUG_SEVERITY_LOW) {
    return;
  }

  gfxWarning() << "OpenGL Debug Callback:";
  gfxWarning() << "  Source: 0x" << hex << aSource;
  gfxWarning() << "  Type: 0x" << hex << aType;
  gfxWarning() << "  Id: " << aId;
  gfxWarning() << "  Severity: 0x" << hex << aSeverity;
  gfxWarning() << "  Message: " << aMessage;
  gfxWarning() << "";
}


namespace nvpr {

GL* gl = nullptr;

GL::GL()
  : mIsValid(false)
  , mNextUniqueId(1)
  , mReadFramebuffer(0)
  , mDrawFramebuffer(0)
  , mColorWritesEnabled(true)
  , mNumClipPlanes(0)
  , mClipPolygonId(0)
  , mColor(1, 1, 1, 1)
  , mScissorTestEnabled(false)
  , mStencilTestEnabled(false)
  , mStencilTest(ALWAYS_PASS)
  , mStencilComparand(0)
  , mStencilTestMask(~0)
  , mStencilOp(LEAVE_UNCHANGED)
  , mStencilWriteMask(~0)
  , mPathStencilFuncBits(0)
  , mBlendingEnabled(false)
  , mSourceBlendFactorRGB(GL_ONE)
  , mDestBlendFactorRGB(GL_ZERO)
  , mSourceBlendFactorAlpha(GL_ONE)
  , mDestBlendFactorAlpha(GL_ZERO)
  , mShaderProgram(0)
{
  mTransformIdStack.push(0);
  memset(mTexgenComponents, 0, sizeof(mTexgenComponents));
  memset(mTexgenCoefficients, 0, sizeof(mTexgenCoefficients));
  memset(mActiveTextureTargets, 0, sizeof(mActiveTextureTargets));
  memset(mBoundTextures, 0, sizeof(mBoundTextures));
  memset(mTexCoordArraysEnabled, 0, sizeof(mTexCoordArraysEnabled));
}

void GL::Initialize()
{
  MOZ_ASSERT(IsCurrent());

  memset(mSupportedExtensions, 0, sizeof(mSupportedExtensions));
  stringstream extensions(reinterpret_cast<const char*>(GetString(GL_EXTENSIONS)));
  istream_iterator<string> iter(extensions);
  istream_iterator<string> end;

  for (; iter != end; iter++) {
    const string& extension = *iter;

    if (*iter == "GL_EXT_direct_state_access") {
      mSupportedExtensions[EXT_direct_state_access] = true;
      continue;
    }

    if (*iter == "GL_NV_path_rendering") {
      mSupportedExtensions[NV_path_rendering] = true;
      continue;
    }

    if (*iter == "GL_EXT_framebuffer_multisample") {
      mSupportedExtensions[EXT_framebuffer_multisample] = true;
      continue;
    }

    if (*iter == "GL_EXT_framebuffer_blit") {
      mSupportedExtensions[EXT_framebuffer_blit] = true;
      continue;
    }

    if (*iter == "GL_EXT_texture_filter_anisotropic") {
      mSupportedExtensions[EXT_texture_filter_anisotropic] = true;
      continue;
    }
  }

  GetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &mMaxRenderbufferSize);
  GetIntegerv(GL_MAX_TEXTURE_SIZE, &mMaxTextureSize);
  GetIntegerv(GL_MAX_CLIP_PLANES, &mMaxClipPlanes);

  if (HasExtension(EXT_texture_filter_anisotropic)) {
    GetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &mMaxAnisotropy);
  } else {
    mMaxAnisotropy = 1;
  }

  GenFramebuffers(1, &mTextureFramebuffer1D);
  GenFramebuffers(1, &mTextureFramebuffer2D);

  for (size_t i = 0; i < TEXTURE_UNIT_COUNT; i++) {
    MultiTexGeniEXT(GL_TEXTURE0 + i, GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    MultiTexGeniEXT(GL_TEXTURE0 + i, GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
  }

  EnableClientState(GL_VERTEX_ARRAY);

  DebugMessageCallback(GLDebugCallback, nullptr);
  DebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
  Enable(GL_DEBUG_OUTPUT);

  mIsValid = true;
}

GL::~GL()
{
  // No need to delete the GL objects. They automatically went away when the
  // context was destroyed.
}

void
GL::SetTargetSize(const IntSize& aSize)
{
  MOZ_ASSERT(IsCurrent());

  if (mTargetSize == aSize) {
    return;
  }

  Viewport(0, 0, aSize.width, aSize.height);
  MatrixLoadIdentityEXT(GL_PROJECTION);
  MatrixOrthoEXT(GL_PROJECTION, 0, aSize.width, 0, aSize.height, -1, 1);
  mTargetSize = aSize;
}

void
GL::SetFramebuffer(GLenum aFramebufferTarget, GLuint aFramebuffer)
{
  MOZ_ASSERT(IsCurrent());

  bool clearTextureFramebuffer1D;
  bool clearTextureFramebuffer2D;

  switch (aFramebufferTarget) {
    case GL_FRAMEBUFFER:
      if (mReadFramebuffer == aFramebuffer && mDrawFramebuffer == aFramebuffer) {
        return;
      }

      BindFramebuffer(GL_FRAMEBUFFER, aFramebuffer);

      clearTextureFramebuffer1D = (aFramebuffer != mTextureFramebuffer1D)
                                  && (mDrawFramebuffer == mTextureFramebuffer1D
                                      || mReadFramebuffer == mTextureFramebuffer1D);
      clearTextureFramebuffer2D = (aFramebuffer != mTextureFramebuffer2D)
                                  && (mDrawFramebuffer == mTextureFramebuffer2D
                                      || mReadFramebuffer == mTextureFramebuffer2D);
      mReadFramebuffer = mDrawFramebuffer = aFramebuffer;
      break;

    case GL_READ_FRAMEBUFFER:
      if (mReadFramebuffer == aFramebuffer) {
        return;
      }

      BindFramebuffer(GL_READ_FRAMEBUFFER, aFramebuffer);

      clearTextureFramebuffer1D = (mReadFramebuffer == mTextureFramebuffer1D);
      clearTextureFramebuffer2D = (mReadFramebuffer == mTextureFramebuffer2D);
      mReadFramebuffer = aFramebuffer;
      break;

    case GL_DRAW_FRAMEBUFFER:
      if (mDrawFramebuffer == aFramebuffer) {
        return;
      }

      BindFramebuffer(GL_DRAW_FRAMEBUFFER, aFramebuffer);

      clearTextureFramebuffer1D = (mDrawFramebuffer == mTextureFramebuffer1D);
      clearTextureFramebuffer2D = (mDrawFramebuffer == mTextureFramebuffer2D);
      mDrawFramebuffer = aFramebuffer;
      break;

    default:
      MOZ_ASSERT(!"Invalid framebuffer target.");
      break;
  }

  if (clearTextureFramebuffer1D) {
    NamedFramebufferTexture1DEXT(mTextureFramebuffer1D, GL_COLOR_ATTACHMENT0,
                                 GL_TEXTURE_1D, 0, 0);
  }

  if (clearTextureFramebuffer2D) {
    NamedFramebufferTexture2DEXT(mTextureFramebuffer2D, GL_COLOR_ATTACHMENT0,
                                 GL_TEXTURE_2D, 0, 0);
  }
}

void
GL::SetFramebufferToTexture(GLenum aFramebufferTarget,
                            GLenum aTextureTarget, GLuint aTextureId)
{
  MOZ_ASSERT(IsCurrent());

  switch (aTextureTarget) {
    case GL_TEXTURE_1D:
      NamedFramebufferTexture1DEXT(mTextureFramebuffer1D, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_1D, aTextureId, 0);
      SetFramebuffer(aFramebufferTarget, mTextureFramebuffer1D);
      break;

    case GL_TEXTURE_2D:
      NamedFramebufferTexture2DEXT(mTextureFramebuffer2D, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, aTextureId, 0);
      SetFramebuffer(aFramebufferTarget, mTextureFramebuffer2D);
      break;

    default:
      MOZ_ASSERT(!"Invalid texture target.");
      break;
  }
}

void
GL::SetTransform(const Matrix& aTransform, UniqueId aTransformId)
{
  MOZ_ASSERT(IsCurrent());

  if (mTransformIdStack.top() == aTransformId) {
    return;
  }

  const GLfloat matrix[] = {
    aTransform._11,  aTransform._12,  0,  0,
    aTransform._21,  aTransform._22,  0,  0,
    0,               0,               1,  0,
    aTransform._31,  aTransform._32,  0,  1
  };

  MatrixLoadfEXT(GL_MODELVIEW, matrix);

  mTransformIdStack.top() = aTransformId;
}

void
GL::SetTransformToIdentity()
{
  MOZ_ASSERT(IsCurrent());

  MatrixLoadIdentityEXT(GL_MODELVIEW);
  mTransformIdStack.top() = 0;
}

void
GL::PushTransform(const Matrix& aTransform)
{
  MOZ_ASSERT(IsCurrent());

  MatrixPushEXT(GL_MODELVIEW);
  mTransformIdStack.push(mTransformIdStack.top());
  SetTransform(aTransform, GetUniqueId());
}

void
GL::PopTransform()
{
  mTransformIdStack.pop();
  MatrixPopEXT(GL_MODELVIEW);
}

void
GL::EnableColorWrites()
{
  MOZ_ASSERT(IsCurrent());

  if (mColorWritesEnabled) {
    return;
  }

  ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  mColorWritesEnabled = true;
}

void
GL::DisableColorWrites()
{
  MOZ_ASSERT(IsCurrent());

  if (!mColorWritesEnabled) {
    return;
  }

  ColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  mColorWritesEnabled = false;
}

void
GL::SetClearColor(const Color& aColor)
{
  MOZ_ASSERT(IsCurrent());

  if (!memcmp(&mClearColor, &aColor, sizeof(Color))) {
    return;
  }

  if (aColor.a == 1) {
    ClearColor(aColor.r, aColor.g, aColor.b, 1);
  } else {
    const float a = aColor.a;
    ClearColor(a * aColor.r, a * aColor.g, a * aColor.b, a);
  }

  mClearColor = aColor;
}

void
GL::SetClearColor(const Color& aColor, GLfloat aAlpha)
{
  SetClearColor(Color(aColor.r, aColor.g, aColor.b, aAlpha * aColor.a));
}

void
GL::SetColor(const Color& aColor)
{
  MOZ_ASSERT(IsCurrent());

  if (!memcmp(&mColor, &aColor, sizeof(Color))) {
    return;
  }

  if (aColor.a == 1) {
    Color4f(aColor.r, aColor.g, aColor.b, 1);
  } else {
    const float a = aColor.a;
    Color4f(a * aColor.r, a * aColor.g, a * aColor.b, a);
  }

  mColor = aColor;
}

void
GL::SetColor(const Color& aColor, GLfloat aAlpha)
{
  SetColor(Color(aColor.r, aColor.g, aColor.b, aAlpha * aColor.a));
}

void
GL::SetColorToAlpha(GLfloat aAlpha)
{
  SetColor(Color(1, 1, 1, aAlpha));
}

void
GL::EnableScissorTest(const IntRect& aScissorRect)
{
  MOZ_ASSERT(IsCurrent());

  if (!mScissorTestEnabled) {
    Enable(GL_SCISSOR_TEST);
    mScissorTestEnabled = true;
  }

  if (!mScissorRect.IsEqualInterior(aScissorRect)) {
    Scissor(aScissorRect.x, aScissorRect.y, aScissorRect.width, aScissorRect.height);
    mScissorRect = aScissorRect;
  }
}

void
GL::DisableScissorTest()
{
  MOZ_ASSERT(IsCurrent());

  if (!mScissorTestEnabled) {
    return;
  }

  Disable(GL_SCISSOR_TEST);
  mScissorTestEnabled = false;
}

void
GL::EnableClipPlanes(const ConvexPolygon& aPolygon, UniqueId aPolygonId)
{
  MOZ_ASSERT(IsCurrent());
  MOZ_ASSERT(aPolygon.NumSides() <= mMaxClipPlanes);

  if (mClipPolygonId == aPolygonId) {
    return;
  }

  if (aPolygon.IsEmpty()) {
    if (!mNumClipPlanes) {
      Enable(GL_CLIP_PLANE0);
    } else {
      for (size_t i = 1; i < mNumClipPlanes; i++) {
        Disable(GL_CLIP_PLANE0 + i);
      }
    }

    mNumClipPlanes = 1;

    // We specify a single clip plane equation that fails for all vertices.
    const double planeEquation[] = {0, 0, 0, -1};
    ClipPlane(GL_CLIP_PLANE0, planeEquation);

    mClipPolygonId = aPolygonId;

    return;
  }

  for (size_t i = mNumClipPlanes; i < aPolygon.NumSides(); i++) {
    Enable(GL_CLIP_PLANE0 + i);
  }
  for (size_t i = aPolygon.NumSides(); i < mNumClipPlanes; i++) {
    Disable(GL_CLIP_PLANE0 + i);
  }

  mNumClipPlanes = aPolygon.NumSides();

  for (size_t i = 0; i < aPolygon.NumSides(); i++) {
    const Line& line = aPolygon.Sides()[i];
    const double planeEquation[] = {line.A, line.B, 0, -line.C};
    ClipPlane(GL_CLIP_PLANE0 + i, planeEquation);
  }

  mClipPolygonId = aPolygonId;
}

void
GL::DisableClipPlanes()
{
  MOZ_ASSERT(IsCurrent());

  for (size_t i = 0; i < mNumClipPlanes; i++) {
    Disable(GL_CLIP_PLANE0 + i);
  }

  mNumClipPlanes = 0;
  mClipPolygonId = 0;
}

void
GL::EnableStencilTest(UnaryStencilTest aTest, GLuint aTestMask,
                      StencilOperation aOp, GLuint aWriteMask)
{
  switch (aTest) {
    case PASS_IF_NOT_ZERO:
      EnableStencilTest(PASS_IF_NOT_EQUAL, 0, aTestMask, aOp, aWriteMask);
      return;
    case PASS_IF_ALL_SET:
      EnableStencilTest(PASS_IF_EQUAL, aTestMask, aTestMask, aOp, aWriteMask);
      return;
  }
}

void
GL::EnableStencilTest(BinaryStencilTest aTest,
                      GLint aComparand, GLuint aTestMask,
                      StencilOperation aOp, GLuint aWriteMask)
{
  MOZ_ASSERT(IsCurrent());

  if (!mStencilTestEnabled) {
    Enable(GL_STENCIL_TEST);
    mStencilTestEnabled = true;
  }

  if (mStencilTest != aTest || mStencilComparand != aComparand
      || mStencilTestMask != aTestMask) {
    GLenum func;
    switch (aTest) {
      default:
        MOZ_ASSERT(!"Invalid stencil test");
      case ALWAYS_PASS:
        func = GL_ALWAYS;
        break;
      case PASS_IF_EQUAL:
        func = GL_EQUAL;
        break;
      case PASS_IF_NOT_EQUAL:
        func = GL_NOTEQUAL;
        break;
    }

    StencilFunc(func, aComparand, aTestMask);

    mStencilTest = aTest;
    mStencilComparand = aComparand;
    mStencilTestMask = aTestMask;
  }

  if (mStencilOp != aOp) {
    switch (aOp) {
      case LEAVE_UNCHANGED:
        StencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        break;
      case CLEAR_PASSING_VALUES:
        StencilOp(GL_KEEP, GL_ZERO, GL_ZERO);
        break;
      case REPLACE_PASSING_WITH_COMPARAND:
        StencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);
        break;
      case REPLACE_PASSING_CLEAR_FAILING:
        StencilOp(GL_ZERO, GL_REPLACE, GL_REPLACE);
        break;
    }

    mStencilOp = aOp;
  }

  if (mStencilWriteMask != aWriteMask) {
    StencilMask(aWriteMask);
    mStencilWriteMask = aWriteMask;
  }
}

void
GL::DisableStencilTest()
{
  MOZ_ASSERT(IsCurrent());

  if (!mStencilTestEnabled) {
    return;
  }

  Disable(GL_STENCIL_TEST);
  mStencilTestEnabled = false;
}

void
GL::ConfigurePathStencilTest(GLubyte aClipBits)
{
  MOZ_ASSERT(IsCurrent());

  if (mPathStencilFuncBits == aClipBits) {
    return;
  }

  if (!aClipBits) {
    PathStencilFuncNV(GL_ALWAYS, 0, 0);
  } else {
    PathStencilFuncNV(GL_EQUAL, aClipBits, aClipBits);
  }

  mPathStencilFuncBits = aClipBits;
}

void
GL::EnableBlending(GLenum aSourceFactorRGB, GLenum aDestFactorRGB,
                   GLenum aSourceFactorAlpha, GLenum aDestFactorAlpha)
{
  MOZ_ASSERT(IsCurrent());

  if (!mBlendingEnabled) {
    Enable(GL_BLEND);
    mBlendingEnabled = true;
  }

  if (mSourceBlendFactorRGB != aSourceFactorRGB
      || mDestBlendFactorRGB != aDestFactorRGB
      || mSourceBlendFactorAlpha != aSourceFactorAlpha
      || mDestBlendFactorAlpha != aDestFactorAlpha) {

    if (aSourceFactorRGB == aSourceFactorAlpha
        && aDestFactorRGB == aDestFactorAlpha) {
      BlendFunc(aSourceFactorRGB, aDestFactorRGB);
    } else {
      BlendFuncSeparate(aSourceFactorRGB, aDestFactorRGB,
                        aSourceFactorAlpha, aDestFactorAlpha);
    }

    mSourceBlendFactorRGB = aSourceFactorRGB;
    mDestBlendFactorRGB = aDestFactorRGB;
    mSourceBlendFactorAlpha = aSourceFactorAlpha;
    mDestBlendFactorAlpha = aDestFactorAlpha;
  }
}

void
GL::DisableBlending()
{
  MOZ_ASSERT(IsCurrent());

  if (!mBlendingEnabled) {
    return;
  }

  Disable(GL_BLEND);
  mBlendingEnabled = false;
}

void
GL::EnableShading(const ShaderConfig& aShaderConfig)
{
  MOZ_ASSERT(IsCurrent());

  const bool hasAlpha = aShaderConfig.mGlobalAlpha != 1;
  const PaintConfig::PaintMode paintMode = aShaderConfig.mPaintConfig.mPaintMode;
  const PaintConfig::PaintMode maskMode = aShaderConfig.mMaskConfig.mPaintMode;

  ConfigureTexgen(PAINT_UNIT, aShaderConfig.mPaintConfig.mTexgenComponents,
                  aShaderConfig.mPaintConfig.mTexgenCoefficients);
  ConfigureTexgen(MASK_UNIT, aShaderConfig.mMaskConfig.mTexgenComponents,
                  aShaderConfig.mMaskConfig.mTexgenCoefficients);

  RefPtr<Shader>& shader = mShaders[hasAlpha][paintMode][maskMode];
  if (!shader) {
    shader = Shader::Create(hasAlpha, paintMode, maskMode);
  }

  if (mShaderProgram != *shader) {
    UseProgram(*shader);
    mShaderProgram = *shader;
  }

  shader->ApplyFragmentUniforms(aShaderConfig);
}

void
GL::DisableShading()
{
  MOZ_ASSERT(IsCurrent());

  if (mShaderProgram == 0) {
    return;
  }

  UseProgram(0);
  mShaderProgram = 0;
}

void
GL::DeleteShaderProgram(GLuint aShaderProgram)
{
  MOZ_ASSERT(IsCurrent());

  if (mShaderProgram == aShaderProgram) {
    DisableShading();
  }

  DeleteProgram(aShaderProgram);
}

void
GL::ConfigureTexgen(TextureUnit aTextureUnit, unsigned aComponents,
                    const GLfloat* aTexgenCoefficients)
{
  MOZ_ASSERT(IsCurrent());
  MOZ_ASSERT(aComponents <= 2);

  if (mTexgenComponents[aTextureUnit] == aComponents
      && !memcmp(mTexgenCoefficients[aTextureUnit], aTexgenCoefficients,
          aComponents * 3 * sizeof(GLfloat))) {
    return;
  }

  if (mTexgenComponents[aTextureUnit] > aComponents) {
    if (aComponents < 2 && mTexgenComponents[aTextureUnit] >= 2) {
      Disablei(GL_TEXTURE_GEN_T, aTextureUnit);
    }
    if (aComponents < 1 && mTexgenComponents[aTextureUnit] >= 1) {
      Disablei(GL_TEXTURE_GEN_S, aTextureUnit);
    }
  } else if (mTexgenComponents[aTextureUnit] < aComponents) {
    if (aComponents >= 1 && mTexgenComponents[aTextureUnit] < 1) {
      Enablei(GL_TEXTURE_GEN_S, aTextureUnit);
    }
    if (aComponents >= 2 && mTexgenComponents[aTextureUnit] < 2) {
      Enablei(GL_TEXTURE_GEN_T, aTextureUnit);
    }
  }

  if (aComponents >= 1) {
    const GLfloat plane[] = {aTexgenCoefficients[0], aTexgenCoefficients[1],
                             0, aTexgenCoefficients[2]};
    MultiTexGenfvEXT(GL_TEXTURE0 + aTextureUnit, GL_S, GL_OBJECT_PLANE, plane);
  }
  if (aComponents >= 2) {
    const GLfloat plane[] = {aTexgenCoefficients[3], aTexgenCoefficients[4],
                             0, aTexgenCoefficients[5]};
    MultiTexGenfvEXT(GL_TEXTURE0 + aTextureUnit, GL_T, GL_OBJECT_PLANE, plane);
  }

  if (!aComponents) {
    PathTexGenNV(GL_TEXTURE0 + aTextureUnit, GL_NONE, 0, 0);
  } else {
    PathTexGenNV(GL_TEXTURE0 + aTextureUnit, GL_OBJECT_LINEAR,
                 aComponents, aTexgenCoefficients);
  }

  mTexgenComponents[aTextureUnit] = aComponents;
  memcpy(mTexgenCoefficients[aTextureUnit], aTexgenCoefficients,
         aComponents * 3 * sizeof(GLfloat));
}

void
GL::SetTexture(TextureUnit aTextureUnit, GLenum aTextureTarget,
               GLuint aTextureId)
{
  MOZ_ASSERT(IsCurrent());

  if (mActiveTextureTargets[aTextureUnit] == aTextureTarget
      && mBoundTextures[aTextureUnit] == aTextureId) {
    return;
  }

  if (mActiveTextureTargets[aTextureUnit]
      && mActiveTextureTargets[aTextureUnit] != aTextureTarget) {
    BindMultiTextureEXT(GL_TEXTURE0 + aTextureUnit,
                        mActiveTextureTargets[aTextureUnit], 0);
  }

  BindMultiTextureEXT(GL_TEXTURE0 + aTextureUnit, aTextureTarget, aTextureId);
}

void
GL::DeleteTexture(GLuint aTextureId)
{
  MOZ_ASSERT(IsCurrent());

  DeleteTextures(1, &aTextureId);

  for (size_t i = 0; i < TEXTURE_UNIT_COUNT; i++) {
    if (mBoundTextures[i] == aTextureId) {
      mActiveTextureTargets[i] = 0;
      mBoundTextures[i] = 0;
    }
  }
}

void
GL::EnableTexCoordArray(TextureUnit aTextureUnit, const GLfloat* aTexCoords)
{
  if (!mTexCoordArraysEnabled[aTextureUnit]) {
    EnableClientStateIndexedEXT(GL_TEXTURE0 + aTextureUnit,
                                GL_TEXTURE_COORD_ARRAY);
    mTexCoordArraysEnabled[aTextureUnit] = true;
  }

  MultiTexCoordPointerEXT(GL_TEXTURE0 + aTextureUnit, 2, GL_FLOAT, 0, aTexCoords);
}

void
GL::DisableTexCoordArray(TextureUnit aTextureUnit)
{
  if (!mTexCoordArraysEnabled[aTextureUnit]) {
    return;
  }

  DisableClientStateIndexedEXT(GL_TEXTURE0 + aTextureUnit,
                               GL_TEXTURE_COORD_ARRAY);
  mTexCoordArraysEnabled[aTextureUnit] = false;
}

void
GL::SetVertexArray(const GLfloat* aVertices)
{
  VertexPointer(2, GL_FLOAT, 0, aVertices);
}

}
}
}
