/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextNVpr.h"
#include <iterator>
#include <sstream>
#include <string>

#include "Logging.h"

using namespace std;

namespace mozilla {
namespace gfx {
#ifdef WIN32
static void __stdcall GLDebugCallback(GLenum aSource, GLenum aType, GLuint aId,
#else
static void GLDebugCallback(GLenum aSource, GLenum aType, GLuint aId,
#endif
                            GLenum aSeverity, GLsizei aLength,
                            const GLchar* aMessage, const void* aUserParam)
{
  if (aSeverity == GL_DEBUG_SEVERITY_LOW) {
    return;
  }
  gfxWarning() << "===> Debug callback: source=0x%x, type=0x%x, id=%u, severity=0x%x\n" <<
    aSource << aType << aId << aSeverity;
  gfxWarning() <<  "===> message: %s\n" << aMessage;
}

GLContextNVpr* GLContextNVpr::Instance()
{
  static GLContextNVpr* context;

  if (!context) {
    context = new GLContextNVpr();
  }

  return context;
}

GLContextNVpr::GLContextNVpr()
  : mReadFramebuffer(0)
  , mDrawFramebuffer(0)
  , mColorWritesEnabled(true)
  , mColor(1, 1, 1, 1)
  , mStencilTestEnabled(false)
  , mStencilTest(ALWAYS_PASS)
  , mStencilComparand(0)
  , mStencilTestMask(~0)
  , mStencilOp(LEAVE_UNCHANGED)
  , mStencilWriteMask(~0)
  , mPathStencilFuncBits(0)
  , mActiveTextureTarget(0)
  , mBoundTextureId(0)
  , mTexgenComponents(TEXGEN_NONE)
  , mShaderProgram(0)
  , mBlendingEnabled(false)
  , mSourceBlendFactorRGB(GL_ONE)
  , mDestBlendFactorRGB(GL_ZERO)
  , mSourceBlendFactorAlpha(GL_ONE)
  , mDestBlendFactorAlpha(GL_ZERO)
{
  memset(mTexgenCoefficients, 0, sizeof(mTexgenCoefficients));
  mTexgenCoefficients[0] = 1;
  mTexgenCoefficients[4] = 1;
  mIsValid = false;

  if (!InitGLContext()) {
    return;
  }

  MakeCurrent();

  memset(mSupportedExtensions, 0, sizeof(mSupportedExtensions));
  stringstream extensions(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)));
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

  GenFramebuffers(1, &mTexture1DFBO);
  GenFramebuffers(1, &mTexture2DFBO);

  TexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
  TexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);

  EnableClientState(GL_VERTEX_ARRAY);
  EnableClientState(GL_TEXTURE_COORD_ARRAY);

  DebugMessageCallback(GLDebugCallback, nullptr);
  DebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
  Enable(GL_DEBUG_OUTPUT);

  mIsValid = true;
}

GLContextNVpr::~GLContextNVpr()
{
  DestroyGLContext();
  // No need to delete the GL objects. They automatically went away when the
  // context was destroyed.
}

void
GLContextNVpr::SetTransform(const Matrix& aTransform)
{
  MOZ_ASSERT(IsCurrent());

  if (!memcmp(&mTransform, &aTransform, sizeof(Matrix))) {
    return;
  }

  const GLfloat matrix[] = {
    aTransform._11,  aTransform._12,  0,  0,
    aTransform._21,  aTransform._22,  0,  0,
    0,               0,               1,  0,
    aTransform._31,  aTransform._32,  0,  1
  };

  MatrixLoadfEXT(GL_MODELVIEW, matrix);

  mTransform = aTransform;
}

void
GLContextNVpr::PushTransform(const Matrix& aTransform)
{
  MOZ_ASSERT(IsCurrent());

  MatrixPushEXT(GL_MODELVIEW);

  mTransformStack.push(mTransform);
  SetTransform(aTransform);
}

void
GLContextNVpr::PopTransform()
{
  MOZ_ASSERT(IsCurrent());

  MatrixPopEXT(GL_MODELVIEW);

  mTransform = mTransformStack.top();
  mTransformStack.pop();
}

void
GLContextNVpr::SetTargetSize(const IntSize& aSize)
{
  MOZ_ASSERT(IsCurrent());

  if (mTargetSize == aSize) {
    return;
  }

  Viewport(0, 0, aSize.width, aSize.height);
  MatrixLoadIdentityEXT(GL_PROJECTION);
  MatrixOrthoEXT(GL_PROJECTION, 0, aSize.width, aSize.height, 0, -1, 1);
  mTargetSize = aSize;
}

void
GLContextNVpr::SetFramebuffer(GLenum aFramebufferTarget, GLuint aFramebuffer)
{
  MOZ_ASSERT(IsCurrent());

  const bool texture1DFBOWasBound = mReadFramebuffer == mTexture1DFBO
                                    || mDrawFramebuffer == mTexture1DFBO;
  const bool texture2DFBOWasBound = mReadFramebuffer == mTexture2DFBO
                                    || mDrawFramebuffer == mTexture2DFBO;

  if (aFramebufferTarget == GL_FRAMEBUFFER) {
    if (mReadFramebuffer == aFramebuffer && mDrawFramebuffer == aFramebuffer) {
      return;
    }
    BindFramebuffer(GL_FRAMEBUFFER, aFramebuffer);
    mReadFramebuffer = mDrawFramebuffer = aFramebuffer;
  } else if (aFramebufferTarget == GL_READ_FRAMEBUFFER) {
    if (mReadFramebuffer == aFramebuffer) {
      return;
    }
    BindFramebuffer(GL_READ_FRAMEBUFFER, aFramebuffer);
    mReadFramebuffer = aFramebuffer;
  } else if (aFramebufferTarget == GL_DRAW_FRAMEBUFFER) {
    if (mDrawFramebuffer == aFramebuffer) {
      return;
    }
    BindFramebuffer(GL_DRAW_FRAMEBUFFER, aFramebuffer);
    mDrawFramebuffer = aFramebuffer;
  }

  if (texture1DFBOWasBound && mReadFramebuffer != mTexture1DFBO
      && mDrawFramebuffer != mTexture1DFBO) {
    NamedFramebufferTexture1DEXT(mTexture1DFBO, GL_COLOR_ATTACHMENT0,
                                 GL_TEXTURE_1D, 0, 0);  
  }

  if (texture2DFBOWasBound && mReadFramebuffer != mTexture2DFBO
      && mDrawFramebuffer != mTexture2DFBO) {
    NamedFramebufferTexture1DEXT(mTexture2DFBO, GL_COLOR_ATTACHMENT0,
                                 GL_TEXTURE_2D, 0, 0);  
  }
}

void
GLContextNVpr::AttachTexture1DToFramebuffer(GLenum aFramebufferTarget, GLuint aTextureId)
{
  MOZ_ASSERT(IsCurrent());

  NamedFramebufferTexture1DEXT(mTexture1DFBO, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_1D, aTextureId, 0);
  SetFramebuffer(aFramebufferTarget, mTexture1DFBO);  
}

void
GLContextNVpr::AttachTexture2DToFramebuffer(GLenum aFramebufferTarget, GLuint aTextureId)
{
  MOZ_ASSERT(IsCurrent());

  NamedFramebufferTexture2DEXT(mTexture2DFBO, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, aTextureId, 0);
  SetFramebuffer(aFramebufferTarget, mTexture2DFBO);  
}

void
GLContextNVpr::EnableColorWrites()
{
  MOZ_ASSERT(IsCurrent());

  if (mColorWritesEnabled) {
    return;
  }

  ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  mColorWritesEnabled = true;
}

void
GLContextNVpr::DisableColorWrites()
{
  MOZ_ASSERT(IsCurrent());

  if (!mColorWritesEnabled) {
    return;
  }

  ColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  mColorWritesEnabled = false;
}

void
GLContextNVpr::EnableStencilTest(UnaryStencilTest aTest, GLuint aTestMask,
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
GLContextNVpr::EnableStencilTest(BinaryStencilTest aTest,
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
GLContextNVpr::DisableStencilTest()
{
  MOZ_ASSERT(IsCurrent());

  if (!mStencilTestEnabled) {
    return;
  }

  Disable(GL_STENCIL_TEST);
  mStencilTestEnabled = false;
}

void
GLContextNVpr::ConfigurePathStencilTest(GLubyte aClipBits)
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
GLContextNVpr::SetColor(const Color& aColor)
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
GLContextNVpr::SetColor(const Color& aColor, GLfloat aAlpha)
{
  SetColor(Color(aColor.r, aColor.g, aColor.b, aAlpha * aColor.a));
}

void
GLContextNVpr::SetColorToAlpha(GLfloat aAlpha)
{
  SetColor(Color(1, 1, 1, aAlpha));
}

void
GLContextNVpr::EnableTexturing(GLenum aTextureTarget, GLenum aTextureId,
                               TexgenComponents aTexgenComponents,
                               const GLfloat* aTexgenCoefficients)
{
  MOZ_ASSERT(IsCurrent());

  if (mActiveTextureTarget == aTextureTarget) {
    if (mBoundTextureId != aTextureId) {
      BindTexture(aTextureTarget, aTextureId);
      mBoundTextureId = aTextureId;
    }
  } else {
    if (mBoundTextureId) {
      BindTexture(mActiveTextureTarget, 0);
    }
    if (mActiveTextureTarget) {
      Disable(mActiveTextureTarget);
    }

    Enable(aTextureTarget);
    mActiveTextureTarget = aTextureTarget;

    BindTexture(aTextureTarget, aTextureId);
    mBoundTextureId = aTextureId;
  }

  if (mTexgenComponents == aTexgenComponents
      && !memcmp(mTexgenCoefficients, aTexgenCoefficients,
                 aTexgenComponents * 3 * sizeof(GLfloat))) {
    return;
  }

  if (mTexgenComponents > aTexgenComponents) {
    if (aTexgenComponents < TEXGEN_ST && mTexgenComponents >= TEXGEN_ST) {
      Disable(GL_TEXTURE_GEN_T);
    }
    if (aTexgenComponents < TEXGEN_S && mTexgenComponents >= TEXGEN_S) {
      Disable(GL_TEXTURE_GEN_S);
    }
  } else if (mTexgenComponents < aTexgenComponents) {
    if (aTexgenComponents >= TEXGEN_S && mTexgenComponents < TEXGEN_S) {
      Enable(GL_TEXTURE_GEN_S);
    }
    if (aTexgenComponents >= TEXGEN_ST && mTexgenComponents < TEXGEN_ST) {
      Enable(GL_TEXTURE_GEN_T);
    }
  }

  if (aTexgenComponents >= TEXGEN_S) {
    const GLfloat plane[] = {aTexgenCoefficients[0], aTexgenCoefficients[1],
                             0, aTexgenCoefficients[2]};
    TexGenfv(GL_S, GL_OBJECT_PLANE, plane);
  }
  if (aTexgenComponents >= TEXGEN_ST) {
    const GLfloat plane[] = {aTexgenCoefficients[3], aTexgenCoefficients[4],
                             0, aTexgenCoefficients[5]};
    TexGenfv(GL_T, GL_OBJECT_PLANE, plane);
  }

  if (aTexgenComponents == TEXGEN_NONE) {
    PathTexGenNV(GL_TEXTURE0, GL_NONE, 0, 0);
  } else {
    PathTexGenNV(GL_TEXTURE0, GL_OBJECT_LINEAR,
                 aTexgenComponents, aTexgenCoefficients);
  }

  mTexgenComponents = aTexgenComponents;
  memcpy(mTexgenCoefficients, aTexgenCoefficients,
         aTexgenComponents * 3 * sizeof(GLfloat));
}

void
GLContextNVpr::EnableTexturing(GLenum aTextureTarget, GLenum aTextureId,
                               TexgenComponents aTexgenComponents,
                               const Matrix& aTransform)
{
  GLfloat coefficients[] = {
    aTransform._11, aTransform._21, aTransform._31,
    aTransform._12, aTransform._22, aTransform._32
  };
  EnableTexturing(aTextureTarget, aTextureId, aTexgenComponents, coefficients);
}

void
GLContextNVpr::DisableTexturing()
{
  MOZ_ASSERT(IsCurrent());

  if (mBoundTextureId) {
    BindTexture(mActiveTextureTarget, 0);
    mBoundTextureId = 0;
  }

  if (mActiveTextureTarget) {
    Disable(mActiveTextureTarget);
    mActiveTextureTarget = 0;
  }

  if (mTexgenComponents >= TEXGEN_S) {
    Disable(GL_TEXTURE_GEN_S);
  }
  if (mTexgenComponents >= TEXGEN_ST) {
    Disable(GL_TEXTURE_GEN_T);
  }
  if (mTexgenComponents != TEXGEN_NONE) {
    PathTexGenNV(GL_TEXTURE0, GL_NONE, 0, 0);
    mTexgenComponents = TEXGEN_NONE;
  }
}

void
GLContextNVpr::DeleteTexture(GLuint aTextureId)
{
  MOZ_ASSERT(IsCurrent());

  DeleteTextures(1, &aTextureId);

  if (mBoundTextureId == aTextureId) {
    mBoundTextureId = 0;
  }
}

void
GLContextNVpr::EnableShading(GLuint aShaderProgram)
{
  MOZ_ASSERT(IsCurrent());

  if (mShaderProgram == aShaderProgram) {
    return;
  }

  UseProgram(aShaderProgram);
  mShaderProgram = aShaderProgram;
}

void
GLContextNVpr::EnableBlending(GLenum aSourceFactorRGB, GLenum aDestFactorRGB,
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
GLContextNVpr::DisableBlending()
{
  MOZ_ASSERT(IsCurrent());

  if (!mBlendingEnabled) {
    return;
  }

  Disable(GL_BLEND);
  mBlendingEnabled = false;
}

}
}
