/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_NVPR_GL_H_
#define MOZILLA_GFX_NVPR_GL_H_

#ifdef WIN32
#define NOMINMAX
#include <Windows.h>
#undef NOMINMAX
#endif

#include "2D.h"
#include "PaintConfig.h"
#include <GL/gl.h>
#include "GL/glext.h"
#include <memory>
#include <stack>

namespace mozilla {
namespace gfx {
namespace nvpr {

typedef uint64_t UniqueId;

class ConvexPolygon;
class Shader;

struct UserData {
  class Object { public: virtual ~Object() {} };

  std::unique_ptr<Object> mPathCache;
  std::unique_ptr<Object> mColorRampData;
  std::unique_ptr<Object> mFonts;
};

class GL
{
public:
  bool IsValid() const { return mIsValid; }

  bool IsCurrent() const;
  void MakeCurrent() const;

  bool BlitTextureToForeignTexture(const IntSize& aSize, GLuint aSourceTextureId,
                                   void* aForeignContext, GLuint aForeignTextureId);

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

  template<typename T> T&
  GetUserObject(std::unique_ptr<UserData::Object> UserData::*aObject)
  {
    std::unique_ptr<UserData::Object>& object = mUserData.*aObject;
    if (!object) {
      object.reset(new T());
    }
    return static_cast<T&>(*object.get());
  }

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

  void EnableBlending(GLenum aSourceFactorRGB, GLenum aDestFactorRGB,
                      GLenum aSourceFactorAlpha, GLenum aDestFactorAlpha);
  void EnableBlending(GLenum aSourceFactor, GLenum aDestFactor)
  {
    EnableBlending(aSourceFactor, aDestFactor, aSourceFactor, aDestFactor);
  }
  void DisableBlending();

  struct ShaderConfig {
    ShaderConfig()
      : mGlobalAlpha(1)
    {}
    GLfloat mGlobalAlpha;
    PaintConfig mPaintConfig;
    PaintConfig mMaskConfig;
  };
  void EnableShading(const ShaderConfig& aShaderConfig);
  void DisableShading();
  void DeleteShaderProgram(GLuint aShaderProgram);

  enum TextureUnit { PAINT_UNIT, MASK_UNIT, TEXTURE_UNIT_COUNT };
  void SetTexture(TextureUnit aTextureUnit, GLenum aTextureTarget,
                  GLuint aTextureId);
  void DeleteTexture(GLuint aTextureId);

  void EnableTexCoordArray(TextureUnit aTextureUnit, const GLfloat* aTexCoords);
  void DisableTexCoordArray(TextureUnit aTextureUnit);

  void SetVertexArray(const GLfloat* aVertices);

protected:
  GL();
  virtual ~GL();

  void Initialize();

private:
  void ConfigureTexgen(TextureUnit aTextureUnit, unsigned aComponents,
                       const GLfloat* aTexgenCoefficients);

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
  RefPtr<Shader> mShaders[2][PaintConfig::MODE_COUNT][PaintConfig::MODE_COUNT];

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
  bool mBlendingEnabled;
  GLenum mSourceBlendFactorRGB;
  GLenum mDestBlendFactorRGB;
  GLenum mSourceBlendFactorAlpha;
  GLenum mDestBlendFactorAlpha;
  GLuint mShaderProgram;
  unsigned mTexgenComponents[TEXTURE_UNIT_COUNT];
  GLfloat mTexgenCoefficients[TEXTURE_UNIT_COUNT][6];
  GLenum mActiveTextureTargets[TEXTURE_UNIT_COUNT];
  GLenum mBoundTextures[TEXTURE_UNIT_COUNT];
  bool mTexCoordArraysEnabled[TEXTURE_UNIT_COUNT];

#define FOR_ALL_PUBLIC_GL_ENTRY_POINTS(MACRO) \
  MACRO(GenTextures) \
  MACRO(CreateShader) \
  MACRO(ShaderSource) \
  MACRO(CompileShader) \
  MACRO(GetShaderiv) \
  MACRO(GetShaderInfoLog) \
  MACRO(GetProgramiv) \
  MACRO(CreateProgram) \
  MACRO(AttachShader) \
  MACRO(LinkProgram) \
  MACRO(DeleteShader) \
  MACRO(GetUniformLocation) \
  MACRO(ProgramUniform1iEXT) \
  MACRO(ProgramUniform1fEXT) \
  MACRO(ProgramUniform2fvEXT) \
  MACRO(ProgramUniform4fvEXT) \
  MACRO(GenRenderbuffers) \
  MACRO(DeleteRenderbuffers) \
  MACRO(Clear) \
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
  MACRO(DeleteProgram) \
  MACRO(GetIntegerv) \
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
  MACRO(UseProgram) \
  MACRO(BlendFunc) \
  MACRO(BlendFuncSeparate) \
  MACRO(Enablei) \
  MACRO(Disablei) \
  MACRO(VertexPointer) \
  MACRO(MatrixOrthoEXT) \
  MACRO(MatrixLoadfEXT) \
  MACRO(MatrixPushEXT) \
  MACRO(MatrixPopEXT) \
  MACRO(MatrixLoadIdentityEXT) \
  MACRO(NamedFramebufferTexture1DEXT) \
  MACRO(NamedFramebufferTexture2DEXT) \
  MACRO(MultiTexGenivEXT) \
  MACRO(MultiTexGenfvEXT) \
  MACRO(BindMultiTextureEXT) \
  MACRO(EnableClientStateIndexedEXT) \
  MACRO(DisableClientStateIndexedEXT) \
  MACRO(MultiTexCoordPointerEXT) \
  MACRO(PathStencilFuncNV) \
  MACRO(PathTexGenNV)

#define DECLARE_GL_METHOD(NAME) \
  decltype(&gl##NAME) NAME;

public:
  FOR_ALL_PUBLIC_GL_ENTRY_POINTS(DECLARE_GL_METHOD);

protected:
  FOR_ALL_PRIVATE_GL_ENTRY_POINTS(DECLARE_GL_METHOD);

#undef DECLARE_GL_METHOD

  void MultiTexGeniEXT(GLenum texunit, GLenum coord, GLenum pname, GLint param)
  {
    // WAR for driver bug with glMultiTexGeniEXT.
    MultiTexGenivEXT(texunit, coord, pname, &param);
  }

  GL(const GL&);
  GL& operator =(const GL&);
};

extern GL* gl;
void InitializeGLIfNeeded();

}
}
}

#endif /* MOZILLA_GFX_NVPR_GL_H_ */
