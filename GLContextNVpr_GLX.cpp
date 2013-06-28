/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextNVpr.h"
#include <dlfcn.h>
#include <GL/glx.h>

#define FOR_ALL_GLX_ENTRY_POINTS(MACRO) \
  MACRO(ChooseFBConfig) \
  MACRO(GetVisualFromFBConfig) \
  MACRO(CreateGLXPixmap) \
  MACRO(DestroyGLXPixmap) \
  MACRO(CreateContext) \
  MACRO(DestroyContext) \
  MACRO(GetProcAddress) \
  MACRO(GetCurrentContext) \
  MACRO(MakeCurrent)

namespace mozilla {
namespace gfx {

struct GLContextNVpr::PlatformContextData {
  void* mLibGL;

#define DECLARE_GLX_METHOD(NAME) \
  decltype(&glX##NAME) NAME;

  FOR_ALL_GLX_ENTRY_POINTS(DECLARE_GLX_METHOD);

#undef DECLARE_GLX_METHOD

  Display* mDisplay;
  Pixmap mPixmap;
  GLXPixmap mGLXPixmap;
  GLXContext mContext;
};

bool GLContextNVpr::InitGLContext()
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
    return false; \
  }

  FOR_ALL_GLX_ENTRY_POINTS(LOAD_GLX_METHOD);

#undef LOAD_GLX_METHOD

  ctx.mDisplay = XOpenDisplay(0);

  int nelements;
  GLXFBConfig *fbc = ctx.ChooseFBConfig(ctx.mDisplay, DefaultScreen(ctx.mDisplay),
                                       0, &nelements);
  XVisualInfo *vi = ctx.GetVisualFromFBConfig(ctx.mDisplay, fbc[0]);

  ctx.mPixmap = XCreatePixmap(ctx.mDisplay, RootWindow(ctx.mDisplay, vi->screen),
                              10, 10, vi->depth);
  ctx.mGLXPixmap = ctx.CreateGLXPixmap(ctx.mDisplay, vi, ctx.mPixmap);

  ctx.mContext = ctx.CreateContext(ctx.mDisplay, vi, 0, true);

#define LOAD_GL_METHOD(NAME) \
  NAME = reinterpret_cast<decltype(NAME)>(ctx.GetProcAddress(reinterpret_cast<const GLubyte*>("gl"#NAME))); \
  if (!NAME) { \
    return false; \
  }

  FOR_ALL_PUBLIC_GL_ENTRY_POINTS(LOAD_GL_METHOD);
  FOR_ALL_PRIVATE_GL_ENTRY_POINTS(LOAD_GL_METHOD);

#undef LOAD_GL_METHOD

  return true;
}

void GLContextNVpr::DestroyGLContext()
{
  PlatformContextData& ctx = *mContextData;

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

bool GLContextNVpr::IsCurrent() const
{
  PlatformContextData& ctx = *mContextData;

  return ctx.GetCurrentContext() == ctx.mContext;
}

void GLContextNVpr::MakeCurrent() const
{
  PlatformContextData& ctx = *mContextData;

  if (IsCurrent()) {
    return;
  }

  ctx.MakeCurrent(ctx.mDisplay, ctx.mGLXPixmap, ctx.mContext);
}

}
}
