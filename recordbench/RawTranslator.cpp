#include "RawTranslator.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace std;

DrawTarget*
RawTranslator::LookupDrawTarget(ReferencePtr aRefPtr)
{
  DTMap::iterator iter = mDrawTargets.find(aRefPtr);

  if (iter != mDrawTargets.end()) {
    return iter->second;
  }

  return NULL;
}

Path*
RawTranslator::LookupPath(ReferencePtr aRefPtr)
{
  PathMap::iterator iter = mPaths.find(aRefPtr);

  if (iter != mPaths.end()) {
    return iter->second;
  }

  return NULL;
}

SourceSurface*
RawTranslator::LookupSourceSurface(ReferencePtr aRefPtr)
{
  SourceSurfaceMap::iterator iter = mSourceSurfaces.find(aRefPtr);

  if (iter != mSourceSurfaces.end()) {
    return iter->second;
  }

  return NULL;
}

FilterNode*
RawTranslator::LookupFilterNode(ReferencePtr aRefPtr)
{
  FilterNodeMap::iterator iter = mFilterNodes.find(aRefPtr);

  if (iter != mFilterNodes.end()) {
    return iter->second;
  }

  return NULL;
}

GradientStops*
RawTranslator::LookupGradientStops(ReferencePtr aRefPtr)
{
  GradientStopsMap::iterator iter = mGradientStops.find(aRefPtr);

  if (iter != mGradientStops.end()) {
    return iter->second;
  }

  return NULL;
}

void
RawTranslator::AddScaledFont(mozilla::gfx::ReferencePtr aRefPtr, ScaledFont *aFont)
{
  ScaledFontMap::iterator iter = mScaledFonts.find(aRefPtr);

  StoredScaledFont newFont;
  newFont.startEvent = mEventNumber;
  newFont.scaledFont = aFont;

  if (iter != mScaledFonts.end()) {
    vector<StoredScaledFont> fonts;
    fonts.push_back(newFont);
    mScaledFonts[aRefPtr] = fonts;
  } else {
    mScaledFonts[aRefPtr].push_back(newFont);
  }
}

void
RawTranslator::RemoveScaledFont(mozilla::gfx::ReferencePtr aRefPtr)
{
  mScaledFonts[aRefPtr][mScaledFonts[aRefPtr].size() - 1].endEvent = mEventNumber;
}

ScaledFont*
RawTranslator::LookupScaledFont(ReferencePtr aRefPtr)
{
  ScaledFontMap::iterator iter = mScaledFonts.find(aRefPtr);

  if (iter != mScaledFonts.end()) {
    for (int i = 0; i < iter->second.size(); i++) {
      if (iter->second[i].startEvent < mEventNumber &&
          iter->second[i].endEvent > mEventNumber) {
        return iter->second[i].scaledFont;
      }
    }
  }

  return NULL;
}

FontType
RawTranslator::GetDesiredFontType()
{
  switch (mBaseDT->GetType()) {
    case BACKEND_DIRECT2D:
      return FONT_DWRITE;
    case BACKEND_CAIRO:
      return FONT_CAIRO;
    case BACKEND_SKIA:
      return FONT_SKIA;
    case BACKEND_NVPR:
      return FONT_NVPR;
    default:
      MOZ_ASSERT(false);
      return FONT_DWRITE;
  }
}
