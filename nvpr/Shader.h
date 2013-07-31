/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_NVPR_SHADER_H_
#define MOZILLA_GFX_NVPR_SHADER_H_

#include "2D.h"
#include "GL.h"

namespace mozilla {
namespace gfx {
namespace nvpr {

class Paint;

class Shader : public RefCounted<Shader> {
public:
 static TemporaryRef<Shader> Create(bool aHasAlpha,
                                    PaintConfig::PaintMode aPaintMode,
                                    PaintConfig::PaintMode aMaskMode);
  virtual ~Shader();

  virtual void ApplyFragmentUniforms(const GL::ShaderConfig& aConfig);

  operator GLuint() { return mShaderProgram; }

protected:
  Shader(TemporaryRef<Paint> aPaint, TemporaryRef<Paint> aMask);
  virtual void WriteDeclarations(std::ostream& aOut) const;
  virtual void WriteMainFunction(std::ostream& aOut) const;
  virtual void Initialize();

  GLuint mFragShader;
  GLuint mShaderProgram;
  RefPtr<Paint> mPaint;
  RefPtr<Paint> mMask;
};

}
}
}

#endif /* MOZILLA_GFX_NVPR_SHADER_H_ */
