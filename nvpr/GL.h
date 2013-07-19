/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_NVPR_GL_H_
#define MOZILLA_GFX_NVPR_GL_H_

#ifdef WIN32
#include <Windows.h>
#endif

#include "2D.h"
#include <GL/gl.h>
#include "GL/glext.h"
#include <memory>
#include <stack>

namespace mozilla {
namespace gfx {

class ConvexPolygon;

typedef void* PlatformGLContext;

namespace nvpr {

typedef uint64_t UniqueId;

struct UserData {
  class Object { public: virtual ~Object() {} };

  std::unique_ptr<Object> mPathCache;
  std::unique_ptr<Object> mColorRampData;
  std::unique_ptr<Object> mGradientShaders;
  std::unique_ptr<Object> mFonts;
};

class GL
{
public:
  static void InitializeIfNeeded();

  bool IsValid() const { return mIsValid; }

  bool IsCurrent() const;
  void MakeCurrent() const;

  bool BlitTextureToForeignTexture(const IntSize& aSize, GLuint aSourceTextureId,
                                   PlatformGLContext aForeignContext,
                                   GLuint aForeignTextureId);

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

  UserData& GetUserData() { return mUserData; }

  UniqueId GetUniqueId() { return mNextUniqueId++; }
  UniqueId TransformId() const { return mTransformIdStack.top(); }
  UniqueId ClipPolygonId() const { return mClipPolygonId; }

  void SetTargetSize(const IntSize& aSize);

  void SetFramebuffer(GLenum aFramebufferTarget, GLuint aFramebuffer);
  void SetFramebufferToTexture(GLenum aFramebufferTarget, GLenum aTextureTarget,
                               GLuint aTextureId);

  void SetTransform(const Matrix& aTransform, UniqueId aTransformId);
  void SetTransformToIdentity();

  void PushTransform(const Matrix& aTransform);
  void PopTransform();

  class ScopedPushTransform {
  public:
    ScopedPushTransform(GL* aGL, const Matrix& aTransform)
      : mGL(aGL)
    {
      mGL->PushTransform(aTransform);
    }
    ~ScopedPushTransform()
    {
      mGL->PopTransform();
    }
  private:
    GL* mGL;
  };

  void EnableColorWrites();
  void DisableColorWrites();

  void SetClearColor(const Color& aColor);
  void SetClearColor(const Color& aColor, GLfloat aAlpha);

  void SetColor(const Color& aColor);
  void SetColor(const Color& aColor, GLfloat aAlpha);
  void SetColorToAlpha(GLfloat aAlpha);

  void EnableScissorTest(const IntRect& aScissorRect);
  void DisableScissorTest();

  void EnableClipPlanes(const ConvexPolygon& aPolygon, UniqueId aPolygonId);
  void DisableClipPlanes();

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

private:
  struct PlatformContextData;

  GL();
  ~GL();

  bool InitGLContext();
  void DestroyGLContext();

  PlatformContextData* mContextData;

  bool mIsValid;
  bool mSupportedExtensions[EXTENSION_COUNT];
  GLint mMaxRenderbufferSize;
  GLint mMaxTextureSize;
  GLint mMaxClipPlanes;
  GLint mMaxAnisotropy;
  UserData mUserData;
  UniqueId mNextUniqueId;
  GLuint mTextureFramebuffer1D;
  GLuint mTextureFramebuffer2D;

  // GL state.
  IntSize mTargetSize;
  GLuint mReadFramebuffer;
  GLuint mDrawFramebuffer;
  std::stack<UniqueId> mTransformIdStack;
  size_t mNumClipPlanes;
  UniqueId mClipPolygonId;
  bool mColorWritesEnabled;
  Color mClearColor;
  Color mColor;
  bool mScissorTestEnabled;
  IntRect mScissorRect;
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

#define FOR_ALL_PUBLIC_GL_ENTRY_POINTS(MACRO) \
  MACRO(GenTextures) \
  MACRO(CreateShader) \
  MACRO(ShaderSource) \
  MACRO(CompileShader) \
  MACRO(CreateProgram) \
  MACRO(AttachShader) \
  MACRO(LinkProgram) \
  MACRO(GetUniformLocation) \
  MACRO(Uniform1f) \
  MACRO(Uniform2f) \
  MACRO(GenRenderbuffers) \
  MACRO(DeleteRenderbuffers) \
  MACRO(Clear) \
  MACRO(TexCoordPointer) \
  MACRO(VertexPointer) \
  MACRO(DrawArrays) \
  MACRO(BlitFramebuffer) \
  MACRO(Rectf) \
  MACRO(Enable) \
  MACRO(Disable) \
  MACRO(GenFramebuffers) \
  MACRO(DeleteFramebuffers) \
  MACRO(PixelStorei) \
  MACRO(ClipPlane) \
  MACRO(GetString) \
  MACRO(Flush) \
  MACRO(Finish) \
  MACRO(TextureStorage1DEXT) \
  MACRO(TextureSubImage1DEXT) \
  MACRO(GenerateTextureMipmapEXT) \
  MACRO(TextureParameteriEXT) \
  MACRO(NamedRenderbufferStorageMultisampleEXT) \
  MACRO(NamedFramebufferRenderbufferEXT) \
  MACRO(TextureImage2DEXT) \
  MACRO(TextureSubImage2DEXT) \
  MACRO(GetTextureImageEXT) \
  MACRO(GenPathsNV) \
  MACRO(PathCommandsNV) \
  MACRO(PathGlyphRangeNV) \
  MACRO(GetPathMetricRangeNV) \
  MACRO(StencilStrokePathNV) \
  MACRO(CoverStrokePathNV) \
  MACRO(StencilFillPathInstancedNV) \
  MACRO(StencilFillPathNV) \
  MACRO(CoverFillPathNV) \
  MACRO(DeletePathsNV) \
  MACRO(PathParameterfNV) \
  MACRO(PathParameteriNV) \
  MACRO(PathDashArrayNV) \
  MACRO(IsPointInFillPathNV) \
  MACRO(IsPointInStrokePathNV) \
  MACRO(GetPathParameterfvNV) \
  MACRO(TransformPathNV) \
  MACRO(GetPathParameterivNV) \
  MACRO(GetPathCommandsNV) \
  MACRO(GetPathCoordsNV)

#define FOR_ALL_PRIVATE_GL_ENTRY_POINTS(MACRO) \
  MACRO(DeleteTextures) \
  MACRO(GetIntegerv) \
  MACRO(TexGeni) \
  MACRO(EnableClientState) \
  MACRO(DebugMessageCallback) \
  MACRO(DebugMessageControl) \
  MACRO(Viewport) \
  MACRO(BindFramebuffer) \
  MACRO(ColorMask) \
  MACRO(Scissor) \
  MACRO(StencilFunc) \
  MACRO(StencilOp) \
  MACRO(StencilMask) \
  MACRO(ClearColor) \
  MACRO(Color4f) \
  MACRO(TexGenfv) \
  MACRO(BindTexture) \
  MACRO(UseProgram) \
  MACRO(BlendFunc) \
  MACRO(BlendFuncSeparate) \
  MACRO(MatrixOrthoEXT) \
  MACRO(MatrixLoadfEXT) \
  MACRO(MatrixPushEXT) \
  MACRO(MatrixPopEXT) \
  MACRO(MatrixLoadIdentityEXT) \
  MACRO(NamedFramebufferTexture1DEXT) \
  MACRO(NamedFramebufferTexture2DEXT) \
  MACRO(PathStencilFuncNV) \
  MACRO(PathTexGenNV)

#define DECLARE_GL_METHOD(NAME) \
  decltype(&gl##NAME) NAME;

public:
  FOR_ALL_PUBLIC_GL_ENTRY_POINTS(DECLARE_GL_METHOD);

private:
  FOR_ALL_PRIVATE_GL_ENTRY_POINTS(DECLARE_GL_METHOD);

#undef DECLARE_GL_METHOD

  GL(const GL&);
  GL& operator =(const GL&);
};

extern GL* gl;

}
}
}

#endif /* MOZILLA_GFX_NVPR_GL_H_ */
