/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GL.h"

#include "Logging.h"
#include <Windows.h>
#include "GL/wglext.h"
#include <iterator>
#include <iostream>
#include <sstream>
#include <string>

using namespace std;

namespace mozilla {
namespace gfx {
namespace nvpr {
  
#define FOR_ALL_WGL_ENTRY_POINTS(MACRO) \
  MACRO(CreateContext) \
  MACRO(MakeCurrent) \
  MACRO(GetProcAddress) \
  MACRO(DeleteContext) \
  MACRO(GetCurrentContext)

struct GL::PlatformContextData {
  void* mLibGL;

#define DECLARE_WGL_METHOD(NAME) \
  decltype(&wgl##NAME) NAME;

  FOR_ALL_WGL_ENTRY_POINTS(DECLARE_WGL_METHOD);

#undef DECLARE_GLX_METHOD

  PFNWGLGETEXTENSIONSSTRINGARBPROC GetExtensionsStringARB;
  PFNWGLCOPYIMAGESUBDATANVPROC CopyImageSubDataNV;

  HDC mDC;
  HGLRC mGLContext;
  HMODULE mGLLibrary;

  template<typename T> void LoadProcAddress(T& aPtr, const char* aName)
  {
    PROC ptr = GetProcAddress(aName);
    if (!ptr) {
      // The GL 1.1 functions have to be loaded directly from the dll.
      ptr = ::GetProcAddress(mGLLibrary, aName);
    }
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

  ctx.mGLLibrary = ::LoadLibrary("opengl32.dll");
  if (!ctx.mGLLibrary) {
    return false;
  }

#define LOAD_WGL_METHOD(NAME) \
  ctx.NAME = reinterpret_cast<decltype(ctx.NAME)>(::GetProcAddress(ctx.mGLLibrary, "wgl"#NAME)); \
  if (!ctx.NAME) { \
    gfxWarning() << "Failed to find WGL function " #NAME "."; \
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

  ctx.LoadProcAddress(ctx.GetExtensionsStringARB, "wglGetExtensionsStringARB");
  if (!ctx.GetExtensionsStringARB) {
    return false;
  }

  stringstream extensions(ctx.GetExtensionsStringARB(ctx.mDC));
  istream_iterator<string> iter(extensions);
  istream_iterator<string> end;

  ctx.CopyImageSubDataNV = nullptr;

  for (; iter != end; iter++) {
    const string& extension = *iter;

    if (*iter == "WGL_NV_copy_image") {
      ctx.LoadProcAddress(ctx.CopyImageSubDataNV, "wglCopyImageSubDataNV");
      break;
    }
  }

#define LOAD_GL_METHOD(NAME) \
  NAME = reinterpret_cast<decltype(NAME)>(ctx.GetProcAddress("gl"#NAME)); \
  if (!NAME) { \
    ctx.LoadProcAddress(NAME, "gl"#NAME); \
    if (!NAME) { \
      return false; \
    } \
  }

  FOR_ALL_PUBLIC_GL_ENTRY_POINTS(LOAD_GL_METHOD);
  FOR_ALL_PRIVATE_GL_ENTRY_POINTS(LOAD_GL_METHOD);

#undef LOAD_GL_METHOD

  return true;
}

void GL::DestroyGLContext()
{
  PlatformContextData& ctx = *mContextData;

  ctx.MakeCurrent(ctx.mDC, ctx.mGLContext);

  if (ctx.mGLContext) {
    ctx.DeleteContext(ctx.mGLContext);
  }
}

bool GL::IsCurrent() const
{
  PlatformContextData& ctx = *mContextData;

  return ctx.GetCurrentContext() == ctx.mGLContext;
}

void GL::MakeCurrent() const
{
  PlatformContextData& ctx = *mContextData;

  if (IsCurrent()) {
    return;
  }

  ctx.MakeCurrent(ctx.mDC, ctx.mGLContext);
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

  ctx.CopyImageSubDataNV(ctx.mGLContext, aSourceTextureId, GL_TEXTURE_2D, 0, 0, 0, 0,
                         static_cast<HGLRC>(aForeignContext), aForeignTextureId,
                         GL_TEXTURE_2D, 0, 0, 0, 0, aSize.width, aSize.height, 1);

  return true;
}

}
}
}
