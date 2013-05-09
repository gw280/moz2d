#include "playbackmanager.h"

using namespace mozilla;
using namespace mozilla::gfx;

PlaybackManager::PlaybackManager()
  : mCurrentEvent(0)
{
}

PlaybackManager::~PlaybackManager()
{
}

DrawTarget*
PlaybackManager::LookupDrawTarget(ReferencePtr aRefPtr)
{
  DTMap::iterator iter = mDrawTargets.find(aRefPtr);

  if (iter != mDrawTargets.end()) {
    return iter->second;
  }

  return NULL;
}

Path*
PlaybackManager::LookupPath(ReferencePtr aRefPtr)
{
  PathMap::iterator iter = mPaths.find(aRefPtr);

  if (iter != mPaths.end()) {
    return iter->second;
  }

  return NULL;
}

SourceSurface*
PlaybackManager::LookupSourceSurface(ReferencePtr aRefPtr)
{
  SourceSurfaceMap::iterator iter = mSourceSurfaces.find(aRefPtr);

  if (iter != mSourceSurfaces.end()) {
    return iter->second;
  }

  return NULL;
}

GradientStops*
PlaybackManager::LookupGradientStops(ReferencePtr aRefPtr)
{
  GradientStopsMap::iterator iter = mGradientStops.find(aRefPtr);

  if (iter != mGradientStops.end()) {
    return iter->second;
  }

  return NULL;
}

ScaledFont*
PlaybackManager::LookupScaledFont(ReferencePtr aRefPtr)
{
  ScaledFontMap::iterator iter = mScaledFonts.find(aRefPtr);

  if (iter != mScaledFonts.end()) {
    return iter->second;
  }

  return NULL;
}

FontType
PlaybackManager::GetDesiredFontType()
{
  switch (mBaseDT->GetType()) {
    case BACKEND_DIRECT2D:
      return FONT_DWRITE;
    case BACKEND_CAIRO:
      return FONT_CAIRO;
    default:
      MOZ_ASSERT(false);
      return FONT_DWRITE;
  }
}

void
PlaybackManager::PlaybackToEvent(int aID)
{
  PlayToEvent(aID);
}

bool
PlaybackManager::IsClipPush(uint32_t aID, int32_t aRefID)
{
  if (aRefID != -1 && mRecordedEvents[aID]->GetObject() != mRecordedEvents[aRefID]->GetObject()) {
    return false;
  }
  return mRecordedEvents[aID]->GetType() == RecordedEvent::PUSHCLIP ||
    mRecordedEvents[aID]->GetType() == RecordedEvent::PUSHCLIPRECT;
}

bool
PlaybackManager::IsClipPop(uint32_t aID, int32_t aRefID)
{
  if (aRefID != -1 && mRecordedEvents[aID]->GetObject() != mRecordedEvents[aRefID]->GetObject()) {
    return false;
  }
  return mRecordedEvents[aID]->GetType() == RecordedEvent::POPCLIP;
}

void
PlaybackManager::DisableEvent(uint32_t aID)
{
  mDisabledEvents.insert(aID);
  EventDisablingUpdated(int32_t(aID));

  if (!IsClipPush(aID) && !IsClipPop(aID)) {
    return;
  }

  if (IsClipPush(aID)) {
    int32_t clipRecord = 1;
    uint32_t id = aID;

    while (++id < mRecordedEvents.size()) {
      if (IsClipPush(id, aID)) {
        clipRecord++;
      }
      if (IsClipPop(id, aID)) {
        clipRecord--;
      }
      if (!clipRecord) {
        mDisabledEvents.insert(id);
        EventDisablingUpdated(int32_t(id));
        return;
      }
    }
  }
  if (IsClipPop(aID)) {
    int32_t clipRecord = 1;
    uint32_t id = aID;

    while (--id >= 0) {
      if (IsClipPush(id, aID)) {
        clipRecord--;
      }
      if (IsClipPop(id, aID)) {
        clipRecord++;
      }
      if (!clipRecord) {
        mDisabledEvents.insert(id);
        EventDisablingUpdated(int32_t(id));
        return;
      }
    }
  }

}

void
PlaybackManager::EnableEvent(uint32_t aID)
{
  mDisabledEvents.erase(aID);
  EventDisablingUpdated(int32_t(aID));
  if (!IsClipPush(aID) && !IsClipPop(aID)) {
    return;
  }

  if (IsClipPush(aID)) {
    int32_t clipRecord = 1;
    uint32_t id = aID;

    while (++id < mRecordedEvents.size()) {
      if (IsClipPush(id, aID)) {
        clipRecord++;
      }
      if (IsClipPop(id, aID)) {
        clipRecord--;
      }
      if (!clipRecord) {
        mDisabledEvents.erase(id);
        EventDisablingUpdated(int32_t(id));
        return;
      }
    }
  }
  if (IsClipPop(aID)) {
    int32_t clipRecord = 1;
    uint32_t id = aID;

    while (--id >= 0) {
      if (IsClipPush(id, aID)) {
        clipRecord--;
      }
      if (IsClipPop(id, aID)) {
        clipRecord++;
      }
      if (!clipRecord) {
        mDisabledEvents.erase(id);
        EventDisablingUpdated(int32_t(id));
        return;
      }
    }
  }
}

void
PlaybackManager::EnableAllEvents()
{
  mDisabledEvents.clear();
  EventDisablingUpdated(-1);
}

bool
PlaybackManager::IsEventDisabled(uint32_t aID)
{
  return mDisabledEvents.find(aID) != mDisabledEvents.end() && CanDisableEvent(mRecordedEvents[aID]);
}

void
PlaybackManager::PlayToEvent(uint32_t aID)
{
  if (mCurrentEvent > aID) {
    mDrawTargets.clear();
    mSourceSurfaces.clear();
    mPaths.clear();
    mGradientStops.clear();
    mCurrentEvent = 0;
  }
  for (int i = mCurrentEvent; i < aID; i++) {
    if (!IsEventDisabled(i) || !CanDisableEvent(mRecordedEvents[i])) {
      PlaybackEvent(mRecordedEvents[i]);
    }
  }

  mCurrentEvent = aID;
}

void
PlaybackManager::PlaybackEvent(RecordedEvent *aEvent)
{
  aEvent->PlayEvent(this);
}

bool
PlaybackManager::CanDisableEvent(RecordedEvent *aEvent)
{
  switch (aEvent->GetType()) {
  case RecordedEvent::CLEARRECT:
  case RecordedEvent::COPYSURFACE:
  case RecordedEvent::DRAWSURFACE:
  case RecordedEvent::DRAWSURFACEWITHSHADOW:
  case RecordedEvent::FILL:
  case RecordedEvent::FILLGLYPHS:
  case RecordedEvent::FILLRECT:
  case RecordedEvent::STROKE:
  case RecordedEvent::SETTRANSFORM:
  case RecordedEvent::STROKERECT:
  case RecordedEvent::STROKELINE:
  case RecordedEvent::MASK:
  case RecordedEvent::PUSHCLIP:
  case RecordedEvent::PUSHCLIPRECT:
  case RecordedEvent::POPCLIP:
    return true;
  default:
    return false;
  }
}