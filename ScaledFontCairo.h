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

  ScaledFontCairo(cairo_scaled_font_t* font, Float aSize);
  ScaledFontCairo(const uint8_t* aData, uint32_t aFileSize, uint32_t aIndex, Float aSize);
  ~ScaledFontCairo();

private:
#ifdef MOZ_ENABLE_FREETYPE
  FT_Face mFTFace;
#endif
};

}
}

#endif /* MOZILLA_GFX_SCALEDFONTCAIRO_H_ */
