/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SCALEDFONTCAIRO_H_
#define MOZILLA_GFX_SCALEDFONTCAIRO_H_

#include "ScaledFontBase.h"

namespace mozilla {
namespace gfx {

class ScaledFontCairo : public ScaledFontBase
{
public:

  ScaledFontCairo(cairo_scaled_font_t* aScaledFont, Float aSize);
  ScaledFontCairo(const uint8_t* aData, uint32_t aFileSize, uint32_t aIndex, Float aSize);
  ~ScaledFontCairo();

#if defined(USE_SKIA) && defined(MOZ_ENABLE_FREETYPE)
  virtual SkTypeface* GetSkTypeface();
#endif

private:
#ifdef MOZ_ENABLE_FREETYPE
  FT_Face mFTFace;
#endif
};

// We need to be able to tell Skia whether or not to use
// hinting when rendering text, so that the glyphs it renders
// are the same as what layout is expecting. At the moment, only
// Skia uses this class when rendering with FreeType, as gfxFT2Font
// is the only gfxFont that honours gfxPlatform::FontHintingEnabled().
class GlyphRenderingOptionsCairo : public GlyphRenderingOptions
{
public:
  GlyphRenderingOptionsCairo()
    : mHinting(FONT_HINTING_NORMAL)
    , mAutoHinting(false)
  {
  }

  void SetHinting(FontHinting aHinting) { mHinting = aHinting; }
  void SetAutoHinting(bool aAutoHinting) { mAutoHinting = aAutoHinting; }
  FontHinting GetHinting() const { return mHinting; }
  bool GetAutoHinting() const { return mAutoHinting; }
  virtual FontType GetType() const { return FONT_CAIRO; }
private:
  FontHinting mHinting;
  bool mAutoHinting;
};

}
}

#endif /* MOZILLA_GFX_SCALEDFONTCAIRO_H_ */
