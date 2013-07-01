/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextNVpr.h"

#include "Logging.h"
#include <Windows.h>

namespace mozilla {
namespace gfx {
  
#define FOR_ALL_WGL_ENTRY_POINTS(MACRO) \
  MACRO(CreateContext) \
  MACRO(MakeCurrent) \
  MACRO(GetProcAddress) \
  MACRO(DeleteContext) \
  MACRO(GetCurrentContext)

struct GLContextNVpr::PlatformContextData {
  void* mLibGL;

#define DECLARE_WGL_METHOD(NAME) \
  decltype(&wgl##NAME) NAME;

  FOR_ALL_WGL_ENTRY_POINTS(DECLARE_WGL_METHOD);

#undef DECLARE_GLX_METHOD

  HDC mDC;
  HGLRC mGLContext;
  HMODULE mGLLibrary;
};

bool
GLContextNVpr::InitGLContext()
{
  mContextData = new PlatformContextData();
  PlatformContextData& ctx = *mContextData;

  ctx.mGLLibrary = ::LoadLibrary("opengl32.dll");
  if (!ctx.mGLLibrary) {
    return false;
  }

#define LOAD_WGL_METHOD(NAME) \
  ctx.NAME = reinterpret_cast<decltype(ctx.NAME)>(::GetProcAddress(ctx.mGLLibrary, "wgl"#NAME)); \
  if (!ctx.NAME) { \
    return false; \
  }
  FOR_ALL_WGL_ENTRY_POINTS(LOAD_WGL_METHOD);

#undef LOAD_GLX_METHOD
  
  HINSTANCE inst = (HINSTANCE)GetModuleHandle(NULL);

  ATOM wclass = 0;
  HWND hwnd = 0;

  WNDCLASS wc;
  memset(&wc, 0, sizeof(wc));
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  wc.hInstance = inst;
  wc.lpfnWndProc = (WNDPROC) DefWindowProc;
  wc.lpszClassName = TEXT("DummyWindow");
  wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;

  wclass = RegisterClass(&wc);
  if (!wclass) {
    return false;
  }

  if (!(hwnd = CreateWindow(TEXT("DummyWindow"),
                            TEXT("Dummy OGL Window"),
                            WS_OVERLAPPEDWINDOW,
                            0, 0, 1, 1,
                            NULL, NULL,
                            inst, NULL))) {
    return false;
  }

  ctx.mDC = ::GetDC(hwnd);

  PIXELFORMATDESCRIPTOR pfd;
  
  memset(&pfd, 0, sizeof(pfd));
  pfd.nSize = sizeof(pfd);
  pfd.dwFlags = PFD_SUPPORT_OPENGL;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 32;
  pfd.cDepthBits = 0;
  pfd.cStencilBits = 0;
  pfd.iLayerType = PFD_MAIN_PLANE;

  // get the best available match of pixel format for the device context   
  int iPixelFormat = ChoosePixelFormat(ctx.mDC, &pfd); 
 
  // make that the pixel format of the device context  
  SetPixelFormat(ctx.mDC, iPixelFormat, &pfd);

  ctx.mGLContext = ctx.CreateContext(ctx.mDC);

  DWORD lastError = ::GetLastError();

  ctx.MakeCurrent(ctx.mDC, ctx.mGLContext);

#define LOAD_GL_METHOD(NAME) \
  NAME = reinterpret_cast<decltype(NAME)>(ctx.GetProcAddress("gl"#NAME)); \
  if (!NAME) { \
    NAME = reinterpret_cast<decltype(NAME)>(::GetProcAddress(ctx.mGLLibrary, "gl"#NAME)); \
    if (!NAME) { \
    gfxWarning() << "Failed to find function " #NAME "."; \
      return false; \
    } \
  }

  FOR_ALL_PUBLIC_GL_ENTRY_POINTS(LOAD_GL_METHOD);
  FOR_ALL_PRIVATE_GL_ENTRY_POINTS(LOAD_GL_METHOD);

#undef LOAD_GL_METHOD

  return true;
}

void GLContextNVpr::DestroyGLContext()
{
  PlatformContextData& ctx = *mContextData;

  ctx.MakeCurrent(ctx.mDC, ctx.mGLContext);

  if (ctx.mGLContext) {
    ctx.DeleteContext(ctx.mGLContext);
  }
}

bool GLContextNVpr::IsCurrent() const
{
  PlatformContextData& ctx = *mContextData;

  return ctx.GetCurrentContext() == ctx.mGLContext;
}

void GLContextNVpr::MakeCurrent() const
{
  PlatformContextData& ctx = *mContextData;

  if (IsCurrent()) {
    return;
  }

  ctx.MakeCurrent(ctx.mDC, ctx.mGLContext);
}


}
}
