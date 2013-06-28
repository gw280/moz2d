/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NativeGLContext.h"
#include <GL/wgl.h>

namespace mozilla {
namespace gfx {

NativeGLContext::NativeGLContext(const NativeGLContext* shareGroup)
{
  mHDC = GetDC(0);
  mContext = wglCreateContext(mHDC);
}

NativeGLContext::~NativeGLContext()
{
  wglMakeCurrent (NULL, NULL) ; 

  if (mContext) {
    wglDeleteContext(mContext);
  }
}

bool NativeGLContext::IsCurrent() const
{
  return wglGetCurrentContext() == mContext;
}

void NativeGLContext::MakeCurrent() const
{
  wglMakeCurrent(mHDC, mContext);
}

}
}
