/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_CLIPNVPR_H_
#define MOZILLA_GFX_CLIPNVPR_H_

#include "2D.h"
#include "Line.h"
#include "PathNVpr.h"
#include <mozilla/RefPtr.h>

namespace mozilla {
namespace gfx {

class DrawTargetNVpr;
class PathNVpr;
class PlanesClipNVpr;
class StencilClipNVpr;

/**
 * A clip state in the NV_path_rendering implementation consists of stencil
 * buffer state and OpenGL clipping planes. 'ClipNVpr' is an abstraction of
 * classes that know how to append a path to the clip state, as well as to
 * restore the clip state to how it was before their changes (i.e. undoing their
 * modifications *as well* as those done by future clips). The stack of clips
 * that make up the current clip state are chained together in a linked list.
 */
class ClipNVpr : public RefCounted<ClipNVpr>
{
  friend class PlanesClipNVpr;
  friend class StencilClipNVpr;

public:
  ClipNVpr(DrawTargetNVpr* aDrawTarget, const Matrix& aTransform,
           TemporaryRef<PathNVpr> aPath)
    : mDrawTarget(aDrawTarget)
    , mTransform(aTransform)
    , mPath(aPath)
    , mNext(nullptr)
  {}
  virtual ~ClipNVpr() {}

  virtual void Apply() = 0;

  virtual void RestorePreviousClipState() = 0;

  bool IsForPath(const Matrix& aTransform, const PathNVpr* aPath) const;

  void Prepend(TemporaryRef<ClipNVpr> aPrevious);
  TemporaryRef<ClipNVpr> DetachFromPrevious();

  ClipNVpr* GetPrevious() const { return mPrevious.get(); }
  ClipNVpr* GetNext() const { return mNext; }

private:
  virtual const StencilClipNVpr* GetLastClipBitOwner() const = 0;

  /**
   * In some cases, a clip can't directly undo its modifications to the clip
   * state (i.e. a stencil clip that destructively intersects its path into an
   * existing clip bit in the stencil buffer). This method is the brute-force
   * way of undoing it: Revert the clip state to the most recent location
   * possible, then re-apply the necessary clips.
   */
  virtual void RestorePreviousStateAndReapply() = 0;

  DrawTargetNVpr* const mDrawTarget;
  const Matrix mTransform;
  RefPtr<PathNVpr> mPath;
  RefPtr<ClipNVpr> mPrevious;
  ClipNVpr* mNext;
};

/**
 * A 'stencil clip' etches its path into a bit plane of the stencil buffer. When
 * a stencil bit clip is active, we configure NV_path_rendering to discard
 * samples not in the clip path (samples where the clip bit is not set). When
 * there are two stencil bit clips, each gets its own bit plane, but with three
 * or more they start sharing a clip bit (by etching in just the intersection of
 * paths). That way there are always at least 6 bits left for winding numbers.
 */
class StencilClipNVpr : public ClipNVpr {
public:
  StencilClipNVpr(DrawTargetNVpr* aDrawTarget, const Matrix& aTransform,
                  TemporaryRef<PathNVpr> aPath)
    : ClipNVpr(aDrawTarget, aTransform, aPath)
    , mOwnClipBit(0)
  {}

  virtual void Apply();

  virtual void RestorePreviousClipState();

private:
  virtual const StencilClipNVpr* GetLastClipBitOwner() const
  {
    MOZ_ASSERT(mOwnClipBit || mPrevious);
    return mOwnClipBit ? this : mPrevious->GetLastClipBitOwner();
  }
  virtual void RestorePreviousStateAndReapply();

  GLubyte mOwnClipBit;
  GLuint mInitialClipPlanesIndex;
};

/**
 * A 'planes clip' uses OpenGL clipping planes instead of the stencil buffer.
 * It only works for convex polygons (i.e. clip rects).
 */
class PlanesClipNVpr : public ClipNVpr {
public:
  PlanesClipNVpr(DrawTargetNVpr* aDrawTarget, const Matrix& aTransform,
                 TemporaryRef<PathNVpr> aPath)
    : ClipNVpr(aDrawTarget, aTransform, aPath)
    , mClipPlanesIndex(0)
  {}

  virtual void Apply();

  virtual void RestorePreviousClipState();

private:
  virtual const StencilClipNVpr* GetLastClipBitOwner() const
  {
    MOZ_ASSERT(mPrevious);
    return mPrevious->GetLastClipBitOwner();
  }
  virtual void RestorePreviousStateAndReapply();

  GLuint mClipPlanesIndex;
};

}
}

#endif /* MOZILLA_GFX_CLIPNVPR_H_ */
