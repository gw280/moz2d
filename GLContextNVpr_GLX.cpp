/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextNVpr.h"
#include <GL/glx.h>

namespace mozilla {
namespace gfx {

struct GLContextNVpr::PlatformContextData {
  Display* mDisplay;
  Pixmap mPixmap;
  GLXPixmap mGLXPixmap;
  GLXContext mContext;
};

void GLContextNVpr::InitGLContext()
{
  mContextData = new PlatformContextData();
  PlatformContextData& ctx = *mContextData;

  ctx.mDisplay = XOpenDisplay(0);

  int nelements;
  GLXFBConfig *fbc = glXChooseFBConfig(ctx.mDisplay, DefaultScreen(ctx.mDisplay),
                                       0, &nelements);
  XVisualInfo *vi = glXGetVisualFromFBConfig(ctx.mDisplay, fbc[0]);

  ctx.mPixmap = XCreatePixmap(ctx.mDisplay, RootWindow(ctx.mDisplay, vi->screen),
                              10, 10, vi->depth);
  ctx.mGLXPixmap = glXCreateGLXPixmap(ctx.mDisplay, vi, ctx.mPixmap);

  ctx.mContext = glXCreateContext(ctx.mDisplay, vi, 0, true);
}

void GLContextNVpr::DestroyGLContext()
{
  PlatformContextData& ctx = *mContextData;

  glXMakeCurrent(ctx.mDisplay, 0, 0);

  if (ctx.mContext) {
    glXDestroyContext(ctx.mDisplay, ctx.mContext);
  }

  if (ctx.mGLXPixmap) {
    glXDestroyGLXPixmap(ctx.mDisplay, ctx.mGLXPixmap);
  }

  if (ctx.mPixmap) {
    XFreePixmap(ctx.mDisplay, ctx.mPixmap);
  }

  XCloseDisplay(ctx.mDisplay);
  ctx.mDisplay = NULL;
}

bool GLContextNVpr::IsCurrent() const
{
  PlatformContextData& ctx = *mContextData;

  return glXGetCurrentContext() == ctx.mContext;
}

void GLContextNVpr::MakeCurrent() const
{
  PlatformContextData& ctx = *mContextData;

  if (IsCurrent()) {
    return;
  }

  glXMakeCurrent(ctx.mDisplay, ctx.mGLXPixmap, ctx.mContext);
}

}
}
