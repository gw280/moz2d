/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScaledFontCairo.h"
#include "Logging.h"

#include <string>

#ifdef MOZ_ENABLE_FREETTYPE
#include "ft2build.h"
#include FT_FREETYPE_H
#endif

using namespace std;

namespace mozilla {
namespace gfx {

#ifdef USE_SKIA
static SkTypeface::Style
fontStyleToSkia(FontStyle aStyle)
{
  switch (aStyle) {
  case FONT_STYLE_NORMAL:
    return SkTypeface::kNormal;
  case FONT_STYLE_ITALIC:
    return SkTypeface::kItalic;
  case FONT_STYLE_BOLD:
    return SkTypeface::kBold;
  case FONT_STYLE_BOLD_ITALIC:
    return SkTypeface::kBoldItalic;
   }

  gfxWarning() << "Unknown font style";
  return SkTypeface::kNormal;
}
#endif

ScaledFontCairo::ScaledFontCairo(cairo_scaled_font_t* font, Float aSize)
  : ScaledFontBase(aSize)
{
  mScaledFont = font;
  cairo_scaled_font_reference(mScaledFont);
}

ScaledFontCairo::ScaledFontCairo(const uint8_t* aData, uint32_t aFileSize, uint32_t aIndex, Float aSize)
  : ScaledFontBase(aSize)
{
#ifdef MOZ_ENABLE_FREETYPE
  FT_Error error = FT_New_Memory_Face(Factory::GetFreetypeLibrary(), aData, aFileSize, aIndex, &mFTFace);

  cairo_font_face_t *face = cairo_ft_font_face_create_for_ft_face(mFTFace, FT_LOAD_DEFAULT);
  
  InitScaledFontFromFace(face);
  
  cairo_font_face_destroy(face);
#else
  // Implement me!
  MOZ_ASSERT(false);
#endif
}

ScaledFontCairo::~ScaledFontCairo()
{
#ifdef MOZ_ENABLE_FREETYPE
  if (mFTFace) {
    FT_Done_Face(mFTFace);
  }
#endif
}

}
}
