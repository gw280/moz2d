/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_CLIPNVPR_H_
#define MOZILLA_GFX_CLIPNVPR_H_

#include "2D.h"
#include "ConvexPolygon.h"
#include "Line.h"
#include "PathNVpr.h"
#include <mozilla/RefPtr.h>

namespace mozilla {
namespace gfx {

class DrawTargetNVpr;
class PathNVpr;

template<typename SubclassType>
class ClipNVpr : public RefCounted<SubclassType> {
public:
  ClipNVpr(DrawTargetNVpr* aDrawTarget, TemporaryRef<SubclassType> aPrevious)
    : mPrevious(aPrevious)
    , mDrawTarget(aDrawTarget)
  {}

  SubclassType* GetPrevious() const { return mPrevious.get(); }
  TemporaryRef<SubclassType> Pop() { return mPrevious.forget(); }

protected:
  DrawTargetNVpr* const mDrawTarget;
  RefPtr<SubclassType> mPrevious;
};

/**
 * 'Planes clips' are a stack of convex polygons stored in device space. We
 * compute the intersection of all polygons in the stack and then use OpenGL
 * clipping planes to clip to that intersection.
 */
class PlanesClipNVpr : public ClipNVpr<PlanesClipNVpr> {
public:
  static TemporaryRef<PlanesClipNVpr>
  Create(DrawTargetNVpr* aDrawTarget, TemporaryRef<PlanesClipNVpr> aPrevious,
         const Matrix& aTransform, ConvexPolygon&& aPassPolygon)
  {
    bool success;
    RefPtr<PlanesClipNVpr> clip = new PlanesClipNVpr(aDrawTarget, aPrevious,
                                                     aTransform, aPassPolygon,
                                                     success);
    return success ? clip.forget() : nullptr;
  }

  const ConvexPolygon& Polygon() const { return mPolygon; }
  GLContextNVpr::UniqueId PolygonId() const { return mPolygonId; }

private:
  PlanesClipNVpr(DrawTargetNVpr* aDrawTarget,
                 TemporaryRef<PlanesClipNVpr> aPrevious,
                 const Matrix& aTransform, ConvexPolygon& aPassPolygon,
                 bool& aSuccess);

  ConvexPolygon mPolygon;
  GLContextNVpr::UniqueId mPolygonId;
};

/**
 * A 'stencil clip' etches its path into a bit plane of the stencil buffer. When
 * a stencil bit clip is active, we configure NV_path_rendering to discard
 * samples not in the clip path (samples where the clip bit is not set). When
 * there are two stencil bit clips, each gets its own bit plane, but with three
 * or more they start sharing a clip bit (by etching in just the intersection of
 * paths). That way there are always at least 6 bits left for winding numbers.
 */
class StencilClipNVpr : public ClipNVpr<StencilClipNVpr> {
public:
  static TemporaryRef<StencilClipNVpr>
  Create(DrawTargetNVpr* aDrawTarget, TemporaryRef<StencilClipNVpr> aPrevious,
         const Matrix& aTransform, GLContextNVpr::UniqueId aTransformId,
         TemporaryRef<PathNVpr> aPath)
  {
    return new StencilClipNVpr(aDrawTarget, aPrevious, aTransform, aTransformId, aPath);
  }

  void ApplyToStencilBuffer();

  void RestoreStencilBuffer();

private:
  StencilClipNVpr* GetLastClipBitOwner()
  {
    return mOwnClipBit ? this : mPrevious->GetLastClipBitOwner();
  }

  StencilClipNVpr(DrawTargetNVpr* aDrawTarget,
                  TemporaryRef<StencilClipNVpr> aPrevious,
                  const Matrix& aTransform, GLContextNVpr::UniqueId aTransformId,
                  TemporaryRef<PathNVpr> aPath)
    : ClipNVpr(aDrawTarget, aPrevious)
    , mTransform(aTransform)
    , mTransformId(aTransformId)
    , mPath(aPath)
    , mOwnClipBit(0)
  {}

  const Matrix mTransform;
  const GLContextNVpr::UniqueId mTransformId;
  RefPtr<PathNVpr> mPath;
  GLubyte mOwnClipBit;
};

}
}

#endif /* MOZILLA_GFX_CLIPNVPR_H_ */
