/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScaledFontFreetype.h"
#include "Logging.h"

#ifdef USE_SKIA
#include "skia/SkTypeface.h"
#endif
#ifdef USE_CAIRO
#include "cairo-ft.h"
#endif

#include <string>

#include "ft2build.h"
#include FT_FREETYPE_H

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

// Ideally we want to use FT_Face here but as there is currently no way to get
// an SkTypeface from an FT_Face we do this.
ScaledFontFreetype::ScaledFontFreetype(FontOptions* aFont, Float aSize)
  : ScaledFontBase(aSize)
{
#ifdef USE_SKIA
  mTypeface = SkTypeface::CreateFromName(aFont->mName.c_str(), fontStyleToSkia(aFont->mStyle));
#endif
}

ScaledFontFreetype::ScaledFontFreetype(const uint8_t* aData, uint32_t aFileSize, uint32_t aIndex, Float aSize)
  : ScaledFontBase(aSize)
{
  FT_Error error = FT_New_Memory_Face(Factory::GetFreetypeLibrary(), aData, aFileSize, aIndex, &mFTFace);
  
#ifdef USE_CAIRO
  cairo_font_face_t *face = cairo_ft_font_face_create_for_ft_face(mFTFace, FT_LOAD_DEFAULT);
  
  InitScaledFontFromFace(face);
  
  cairo_font_face_destroy(face);
#else
  // Implement me!
  MOZ_ASSERT(false);
#endif
}

ScaledFontFreetype::~ScaledFontFreetype()
{
  if (mFTFace) {
    FT_Done_Face(mFTFace);
  }
}

}
}
