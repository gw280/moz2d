/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GL.h"
#include "Logging.h"
#include <dlfcn.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <iterator>
#include <iostream>
#include <sstream>
#include <string>

using namespace std;

#define FOR_ALL_GLX_ENTRY_POINTS(MACRO) \
  MACRO(ChooseFBConfig) \
  MACRO(GetVisualFromFBConfig) \
  MACRO(CreateGLXPixmap) \
  MACRO(DestroyGLXPixmap) \
  MACRO(CreateContext) \
  MACRO(DestroyContext) \
  MACRO(GetProcAddress) \
  MACRO(GetCurrentContext) \
  MACRO(MakeCurrent) \
  MACRO(QueryExtensionsString)

namespace mozilla {
namespace gfx {
namespace nvpr {

struct GL::PlatformContextData {
  void* mLibGL;

#define DECLARE_GLX_METHOD(NAME) \
  decltype(&glX##NAME) NAME;

  FOR_ALL_GLX_ENTRY_POINTS(DECLARE_GLX_METHOD);

#undef DECLARE_GLX_METHOD

  PFNGLXCOPYIMAGESUBDATANVPROC CopyImageSubDataNV;

  Display* mDisplay;
  Pixmap mPixmap;
  GLXPixmap mGLXPixmap;
  GLXContext mContext;

  template<typename T> void LoadProcAddress(T& aPtr, const char* aName)
  {
    void (*ptr)() = GetProcAddress(reinterpret_cast<const GLubyte*>(aName));
    if (!ptr) {
      gfxWarning() << "Failed to load GL function " << aName << ".";
      aPtr = nullptr;
      return;
    }
    aPtr = reinterpret_cast<T>(ptr);
  }
};

bool
GL::InitGLContext()
{
  mContextData = new PlatformContextData();
  PlatformContextData& ctx = *mContextData;

  ctx.mLibGL = dlopen("libGL.so", RTLD_LAZY);
  if (!ctx.mLibGL) {
    return false;
  }

#define LOAD_GLX_METHOD(NAME) \
  ctx.NAME = reinterpret_cast<decltype(ctx.NAME)>(dlsym(ctx.mLibGL, "glX"#NAME)); \
  if (!ctx.NAME) { \
    gfxWarning() << "Failed to find GLX function " #NAME "."; \
    return false; \
  }

  FOR_ALL_GLX_ENTRY_POINTS(LOAD_GLX_METHOD);

#undef LOAD_GLX_METHOD

  ctx.mDisplay = XOpenDisplay(0);

  int nelements;
  int screen = DefaultScreen(ctx.mDisplay);
  GLXFBConfig *fbc = ctx.ChooseFBConfig(ctx.mDisplay, screen, 0, &nelements);
  XVisualInfo *vi = ctx.GetVisualFromFBConfig(ctx.mDisplay, fbc[0]);

  ctx.mPixmap = XCreatePixmap(ctx.mDisplay, RootWindow(ctx.mDisplay, vi->screen),
                              10, 10, vi->depth);
  ctx.mGLXPixmap = ctx.CreateGLXPixmap(ctx.mDisplay, vi, ctx.mPixmap);

  ctx.mContext = ctx.CreateContext(ctx.mDisplay, vi, 0, true);

  MakeCurrent();

  stringstream extensions(ctx.QueryExtensionsString(ctx.mDisplay, screen));
  istream_iterator<string> iter(extensions);
  istream_iterator<string> end;

  ctx.CopyImageSubDataNV = nullptr;

  for (; iter != end; iter++) {
    const string& extension = *iter;

    if (*iter == "GLX_NV_copy_image") {
      ctx.LoadProcAddress(ctx.CopyImageSubDataNV, "glXCopyImageSubDataNV");
      break;
    }
  }

#define LOAD_GL_METHOD(NAME) \
  ctx.LoadProcAddress(NAME, "gl"#NAME); \
  if (!NAME) { \
    return false; \
  }

  FOR_ALL_PUBLIC_GL_ENTRY_POINTS(LOAD_GL_METHOD);
  FOR_ALL_PRIVATE_GL_ENTRY_POINTS(LOAD_GL_METHOD);

#undef LOAD_GL_METHOD

  return true;
}

void
GL::DestroyGLContext()
{
  PlatformContextData& ctx = *mContextData;
  if (!ctx.mLibGL) {
    return;
  }

  ctx.MakeCurrent(ctx.mDisplay, 0, 0);

  if (ctx.mContext) {
    ctx.DestroyContext(ctx.mDisplay, ctx.mContext);
  }

  if (ctx.mGLXPixmap) {
    ctx.DestroyGLXPixmap(ctx.mDisplay, ctx.mGLXPixmap);
  }

  if (ctx.mPixmap) {
    XFreePixmap(ctx.mDisplay, ctx.mPixmap);
  }

  XCloseDisplay(ctx.mDisplay);

  dlclose(ctx.mLibGL);
}

bool
GL::IsCurrent() const
{
  PlatformContextData& ctx = *mContextData;

  return ctx.GetCurrentContext() == ctx.mContext;
}

void
GL::MakeCurrent() const
{
  PlatformContextData& ctx = *mContextData;

  if (IsCurrent()) {
    return;
  }

  ctx.MakeCurrent(ctx.mDisplay, ctx.mGLXPixmap, ctx.mContext);
}

bool
GL::BlitTextureToForeignTexture(const IntSize& aSize, GLuint aSourceTextureId,
                                PlatformGLContext aForeignContext,
                                GLuint aForeignTextureId)
{
  PlatformContextData& ctx = *mContextData;

  if (!ctx.CopyImageSubDataNV) {
    return false;
  }

  ctx.CopyImageSubDataNV(ctx.mDisplay, ctx.mContext, aSourceTextureId,
                         GL_TEXTURE_2D, 0, 0, 0, 0,
                         static_cast<GLXContext>(aForeignContext),
                         aForeignTextureId, GL_TEXTURE_2D, 0, 0, 0, 0,
                         aSize.width, aSize.height, 1);

  return true;
}

}
}
}
