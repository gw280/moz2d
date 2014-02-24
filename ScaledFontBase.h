/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SCALEDFONTBASE_H_
#define MOZILLA_GFX_SCALEDFONTBASE_H_

#include "2D.h"

#ifdef USE_SKIA
#include "core/SkPath.h"
#include "core/SkTypeface.h"
#endif
#ifdef USE_CAIRO
#include "cairo.h"
#endif

class gfxFont;

namespace mozilla {
namespace gfx {

class ScaledFontBase : public ScaledFont
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(ScaledFontBase)
  ScaledFontBase(Float aSize);
  virtual ~ScaledFontBase();

  virtual TemporaryRef<Path> GetPathForGlyphs(const GlyphBuffer &aBuffer, const DrawTarget *aTarget);

  virtual void CopyGlyphsToBuilder(const GlyphBuffer &aBuffer, PathBuilder *aBuilder, BackendType aBackendType, const Matrix *aTransformHint);

  float GetSize() { return mSize; }

#ifdef USE_SKIA
  virtual SkTypeface* GetSkTypeface() { return mTypeface; }
#endif

  // Not true, but required to instantiate a ScaledFontBase.
  virtual FontType GetType() const { return FontType::SKIA; }

#ifdef USE_CAIRO
  cairo_scaled_font_t* GetCairoScaledFont() { return mScaledFont; }
  void SetCairoScaledFont(cairo_scaled_font_t* font);
  void InitScaledFontFromFace(cairo_font_face_t *aFace);
#endif

protected:
  friend class DrawTargetSkia;
#ifdef USE_SKIA
  SkTypeface* mTypeface;
  SkPath GetSkiaPathForGlyphs(const GlyphBuffer &aBuffer);
#endif
#ifdef USE_CAIRO
  cairo_scaled_font_t* mScaledFont;
#endif
  Float mSize;
};

}
}

#endif /* MOZILLA_GFX_SCALEDFONTBASE_H_ */
