/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_GLCONTEXTNVPR_H_
#define MOZILLA_GFX_GLCONTEXTNVPR_H_

#include "2D.h"
#include <GL/gl.h>
#include <memory>
#include <stack>

namespace mozilla {
namespace gfx {

class ClipNVpr;
class PathNVpr;

struct UserDataNVpr {
  class Object { public: virtual ~Object() {} };

  std::unique_ptr<Object> mPathCache;
  std::unique_ptr<Object> mGradientShaders;
  std::unique_ptr<Object> mFonts;
};

class GLContextNVpr
{
public:
  static GLContextNVpr* Instance();

  bool IsCurrent() const;
  void MakeCurrent() const;

  enum Extension {
    EXT_direct_state_access,
    NV_path_rendering,
    EXT_framebuffer_multisample,
    EXT_framebuffer_blit,
    EXT_texture_filter_anisotropic,
    EXTENSION_COUNT
  };
  bool HasExtension(Extension aExtension) const
  {
    return mSupportedExtensions[aExtension];
  }

  GLint MaxRenderbufferSize() const { return mMaxRenderbufferSize; }
  GLint MaxTextureSize() const { return mMaxTextureSize; }
  GLint MaxClipPlanes() const { return mMaxClipPlanes; }
  GLint MaxAnisotropy() const { return mMaxAnisotropy; }

  UserDataNVpr& UserData() { return mUserData; }

  void SetTargetSize(const IntSize& aSize);

  void BindFramebuffer(GLenum aFramebufferTarget, GLuint aFramebuffer);
  void BindTexture1DAsFramebuffer(GLenum aFramebufferTarget, GLuint aTextureId);
  void BindTexture2DAsFramebuffer(GLenum aFramebufferTarget, GLuint aTextureId);

  void SetTransform(const Matrix& aTransform);
  void PushTransform(const Matrix& aTransform);
  void PopTransform();

  class ScopedPushTransform {
  public:
    ScopedPushTransform(const Matrix& aTransform)
    {
      Instance()->PushTransform(aTransform);
    }
    ~ScopedPushTransform()
    {
      Instance()->PopTransform();
    }
  };

  void EnableColorWrites();
  void DisableColorWrites();

  enum UnaryStencilTest { PASS_IF_NOT_ZERO, PASS_IF_ALL_SET };
  enum BinaryStencilTest { ALWAYS_PASS, PASS_IF_EQUAL, PASS_IF_NOT_EQUAL };
  enum StencilOperation { LEAVE_UNCHANGED, CLEAR_PASSING_VALUES,
                          REPLACE_PASSING_WITH_COMPARAND,
                          REPLACE_PASSING_CLEAR_FAILING };
  void EnableStencilTest(UnaryStencilTest aTest, GLuint aTestMask,
                         StencilOperation aOp, GLuint aWriteMask = ~0);
  void EnableStencilTest(BinaryStencilTest aTest,
                         GLint aComparand, GLuint aTestMask,
                         StencilOperation aOp, GLuint aWriteMask = ~0);
  void DisableStencilTest();

  void ConfigurePathStencilTest(GLubyte aClipBits);

  void SetColor(const Color& aColor);
  void SetColor(const Color& aColor, GLfloat aAlpha);
  void SetColorToAlpha(GLfloat aAlpha);

  enum TexgenComponents { TEXGEN_NONE = 0, TEXGEN_S = 1, TEXGEN_ST = 2 };
  void EnableTexturing(GLenum aTextureTarget, GLenum aTextureId,
                       TexgenComponents aTexgenComponents,
                       const GLfloat* aTexgenCoefficients = nullptr);
  void EnableTexturing(GLenum aTextureTarget, GLenum aTextureId,
                       TexgenComponents aTexgenComponents,
                       const Matrix& aTransform);
  void DisableTexturing();

  void DeleteTexture(GLuint aTextureId);

  void EnableShading(GLuint aShaderProgram);
  void DisableShading() { EnableShading(0); }

  void EnableBlending(GLenum aSourceFactorRGB, GLenum aDestFactorRGB,
                      GLenum aSourceFactorAlpha, GLenum aDestFactorAlpha);
  void EnableBlending(GLenum aSourceFactor, GLenum aDestFactor)
  {
    EnableBlending(aSourceFactor, aDestFactor, aSourceFactor, aDestFactor);
  }
  void DisableBlending();

//  GLuint ReserveClipPlanes(size_t count);
//  void ReleaseClipPlanes(GLuint aIndex);

private:
  struct PlatformContextData;

  GLContextNVpr();
  ~GLContextNVpr();
  GLContextNVpr(const GLContextNVpr&) = delete;
  GLContextNVpr& operator =(const GLContextNVpr&) = delete;

  void InitGLContext();
  void DestroyGLContext();

  PlatformContextData* mContextData;

  bool mSupportedExtensions[EXTENSION_COUNT];
  GLint mMaxRenderbufferSize;
  GLint mMaxTextureSize;
  GLint mMaxClipPlanes;
  GLint mMaxAnisotropy;
  UserDataNVpr mUserData;
  std::stack<Matrix> mTransformStack;
  GLuint mTexture1DFBO;
  GLuint mTexture2DFBO;

  // GL state.
  IntSize mTargetSize;
  GLuint mReadFramebuffer;
  GLuint mDrawFramebuffer;
  Matrix mTransform;
  bool mColorWritesEnabled;
  Color mColor;
  bool mStencilTestEnabled;
  BinaryStencilTest mStencilTest;
  GLint mStencilComparand;
  GLuint mStencilTestMask;
  StencilOperation mStencilOp;
  GLuint mStencilWriteMask;
  GLubyte mPathStencilFuncBits;
  GLenum mBoundTextureId;
  TexgenComponents mTexgenComponents;
  GLfloat mTexgenCoefficients[6];
  GLuint mShaderProgram;
  bool mBlendingEnabled;
  GLenum mSourceBlendFactorRGB;
  GLenum mDestBlendFactorRGB;
  GLenum mSourceBlendFactorAlpha;
  GLenum mDestBlendFactorAlpha;
  GLenum mActiveTextureTarget;

};

}
}

#endif /* MOZILLA_GFX_GLCONTEXTNVPR_H_ */
