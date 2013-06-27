
#include "2D.h"
#include "RecordedEvent.h"
#include <map>
#include <vector>

class RawTranslator : public mozilla::gfx::Translator
{
public:
  RawTranslator(mozilla::gfx::DrawTarget *aBaseDT)
    : mBaseDT(aBaseDT)
  {
  }

  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::Path Path;
  typedef mozilla::gfx::SourceSurface SourceSurface;
  typedef mozilla::gfx::GradientStops GradientStops;
  typedef mozilla::gfx::ScaledFont ScaledFont;

  struct StoredScaledFont {
    uint32_t startEvent;
    uint32_t endEvent;
    mozilla::RefPtr<mozilla::gfx::ScaledFont> scaledFont;
  };

  void SetEventNumber(uint32_t aEventNumber) { mEventNumber = aEventNumber; }

  // Translator
  virtual DrawTarget *LookupDrawTarget(mozilla::gfx::ReferencePtr aRefPtr);
  virtual Path *LookupPath(mozilla::gfx::ReferencePtr aRefPtr);
  virtual SourceSurface *LookupSourceSurface(mozilla::gfx::ReferencePtr aRefPtr);
  virtual GradientStops *LookupGradientStops(mozilla::gfx::ReferencePtr aRefPtr);
  virtual ScaledFont *LookupScaledFont(mozilla::gfx::ReferencePtr aRefPtr);
  virtual DrawTarget *GetReferenceDrawTarget() { return mBaseDT; }
  virtual mozilla::gfx::FontType GetDesiredFontType();
  virtual void AddDrawTarget(mozilla::gfx::ReferencePtr aRefPtr, DrawTarget *aDT) { mDrawTargets[aRefPtr] = aDT; }
  virtual void RemoveDrawTarget(mozilla::gfx::ReferencePtr aRefPtr) { mDrawTargets.erase(aRefPtr); }
  virtual void AddPath(mozilla::gfx::ReferencePtr aRefPtr, Path *aPath) { mPaths[aRefPtr] = aPath; }
  virtual void AddSourceSurface(mozilla::gfx::ReferencePtr aRefPtr, SourceSurface *aSurface) { mSourceSurfaces[aRefPtr] = aSurface; }
  virtual void RemoveSourceSurface(mozilla::gfx::ReferencePtr aRefPtr) { mSourceSurfaces.erase(aRefPtr); }
  virtual void RemovePath(mozilla::gfx::ReferencePtr aRefPtr) { mPaths.erase(aRefPtr); }
  virtual void AddGradientStops(mozilla::gfx::ReferencePtr aRefPtr, GradientStops *aStops) { mGradientStops[aRefPtr] = aStops; }
  virtual void RemoveGradientStops(mozilla::gfx::ReferencePtr aRefPtr) { mGradientStops.erase(aRefPtr); }
  virtual void AddScaledFont(mozilla::gfx::ReferencePtr aRefPtr, ScaledFont *aStops);
  virtual void RemoveScaledFont(mozilla::gfx::ReferencePtr aRefPtr);

  typedef std::map<void*, mozilla::RefPtr<DrawTarget> > DTMap;
  typedef std::map<void*, mozilla::RefPtr<Path> > PathMap;
  typedef std::map<void*, mozilla::RefPtr<SourceSurface> > SourceSurfaceMap;
  typedef std::map<void*, mozilla::RefPtr<GradientStops> > GradientStopsMap;
  typedef std::map<void*, std::vector<StoredScaledFont> > ScaledFontMap;

  uint32_t mEventNumber;
  DTMap mDrawTargets;
  PathMap mPaths;
  SourceSurfaceMap mSourceSurfaces;
  GradientStopsMap mGradientStops;
  ScaledFontMap mScaledFonts;
  
  mozilla::RefPtr<mozilla::gfx::DrawTarget> mBaseDT;
};
