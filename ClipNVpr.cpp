/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClipNVpr.h"
#include "DrawTargetNVpr.h"
#include "GLContextNVpr.h"
#include <mozilla/DebugOnly.h>

namespace mozilla {
namespace gfx {

void StencilClipNVpr::Apply()
{
  MOZ_ASSERT(!mOwnClipBit);

  GLContextNVpr* const gl = GLContextNVpr::Instance();
  MOZ_ASSERT(gl->IsCurrent());

  GLContextNVpr::ScopedPushTransform pushTransform(mTransform);

  gl->DisableTexturing();
  gl->DisableShading();
  gl->DisableColorWrites();

  mOwnClipBit = mDrawTarget->ReserveStencilClipBit();
  if (mOwnClipBit) {
    // We own a stencil bit plane for clipping. Now we etch in our path.
    const GLubyte existingClipBits = ~(mOwnClipBit | (mOwnClipBit - 1)) & 0xff;

    gl->ConfigurePathStencilTest(existingClipBits);
    gl->StencilFillPathNV(*mPath, GL_COUNT_UP_NV,
                        (mPath->GetFillRule() == FILL_WINDING)
                        ? mOwnClipBit - 1 : 0x1);

    gl->EnableStencilTest(GLContextNVpr::PASS_IF_NOT_EQUAL,
                          mOwnClipBit, mOwnClipBit - 1,
                          GLContextNVpr::REPLACE_PASSING_WITH_COMPARAND,
                          mOwnClipBit | (mOwnClipBit - 1));
    gl->CoverFillPathNV(*mPath, GL_BOUNDING_BOX_NV);

    // Also note the current clip planes for restoring the previous clip state.
    mInitialClipPlanesIndex = mDrawTarget->ReserveClipPlanes(0);

    return;
  }

  // There aren't enough stencil bit planes left for us to get our own. We have
  // to destructively intersect our path into an existing clip bit.
  const StencilClipNVpr* lastClipBitOwner = GetLastClipBitOwner();
  MOZ_ASSERT(lastClipBitOwner);

  const GLubyte sharedClipBit = lastClipBitOwner->mOwnClipBit;
  const GLubyte existingClipBits = ~(sharedClipBit - 1) & 0xff;

  gl->ConfigurePathStencilTest(existingClipBits);
  gl->StencilFillPathNV(*mPath, GL_COUNT_UP_NV,
                      (mPath->GetFillRule() == FILL_WINDING)
                      ? sharedClipBit - 1 : 0x1);

  gl->SetTransform(lastClipBitOwner->mTransform);

  gl->EnableStencilTest(GLContextNVpr::PASS_IF_NOT_EQUAL,
                        sharedClipBit, sharedClipBit - 1,
                        GLContextNVpr::REPLACE_PASSING_CLEAR_FAILING,
                        sharedClipBit | (sharedClipBit - 1));
  gl->CoverFillPathNV(*lastClipBitOwner->mPath, GL_BOUNDING_BOX_NV);
}

void StencilClipNVpr::RestorePreviousClipState()
{
  if (!mOwnClipBit) {
    // We destroyed the previous clip state when we intersected our path into an
    // existing clip bit in the stencil buffer. We have to clear that bit plane
    // and then etch the previous path(s) back into it again.
    MOZ_ASSERT(mPrevious);
    mPrevious->RestorePreviousStateAndReapply();
    return;
  }

  // A clip state also includes clipping planes, so we need to restore them first.
  mDrawTarget->ReleaseClipPlanes(mInitialClipPlanesIndex);

  GLContextNVpr* const gl = GLContextNVpr::Instance();
  MOZ_ASSERT(gl->IsCurrent());

  GLContextNVpr::ScopedPushTransform pushTransform(mTransform);

  gl->DisableColorWrites();
  gl->DisableTexturing();
  gl->DisableShading();

  // In order to reset the stencil buffer to the previous clipping state, we
  // need to clear our bit plane as well as any stencil data from future clips.
  const GLuint newFreeBits = mOwnClipBit | (mOwnClipBit - 1);
  gl->EnableStencilTest(GLContextNVpr::PASS_IF_NOT_ZERO, newFreeBits,
                        GLContextNVpr::CLEAR_PASSING_VALUES,
                        newFreeBits);
  gl->CoverFillPathNV(*mPath, GL_BOUNDING_BOX_NV);

  mDrawTarget->ReleaseStencilClipBits(newFreeBits);
  mOwnClipBit = 0;
}

void StencilClipNVpr::RestorePreviousStateAndReapply()
{
  RestorePreviousClipState();
  Apply();
}


void PlanesClipNVpr::Apply()
{
  MOZ_ASSERT(!mClipPlanesIndex);

  GLContextNVpr* const gl = GLContextNVpr::Instance();
  MOZ_ASSERT(gl->IsCurrent());

  GLContextNVpr::ScopedPushTransform pushTransform(mTransform);

  mClipPlanesIndex = mDrawTarget->ReserveClipPlanes(mPath->ConvexOutline().size());

  for (size_t i = 0; i < mPath->ConvexOutline().size(); i++) {
    const Line& line = mPath->ConvexOutline()[i];
    const double planeEquation[] = {line.A, line.B, 0, -line.C};
    gl->ClipPlane(GL_CLIP_PLANE0 + mClipPlanesIndex + i, planeEquation);
  }
}

void PlanesClipNVpr::RestorePreviousClipState()
{
  if (mNext) {
    // A clip state consists of GL clip planes *and* stencil clip bits. We don't
    // know how to restore the stencil buffer after future modifications, so we
    // rely on mNext to do that part.
    mNext->RestorePreviousClipState();
  }

  mDrawTarget->ReleaseClipPlanes(mClipPlanesIndex);
  mClipPlanesIndex = 0;
}

void PlanesClipNVpr::RestorePreviousStateAndReapply()
{
  mDrawTarget->ReleaseClipPlanes(mClipPlanesIndex);

  // This method gets called when a future clip can't restore the stencil
  // buffer, so we need mPrevious to restore it.
  MOZ_ASSERT(mPrevious);
  mPrevious->RestorePreviousStateAndReapply();

  DebugOnly<GLuint> index
    = mDrawTarget->ReserveClipPlanes(mPath->ConvexOutline().size());
  MOZ_ASSERT(index == mClipPlanesIndex);
  // No need to re-specify the clipping planes, they stayed the same.
}


bool ClipNVpr::IsForPath(const Matrix& aTransform, const PathNVpr* aPath) const
{
  if (memcmp(&mTransform, &aTransform, sizeof(Matrix))) {
    return false;
  }
  return mPath->IsSamePath(aPath);
}

void ClipNVpr::Prepend(TemporaryRef<ClipNVpr> aPrevious)
{
  MOZ_ASSERT(!mPrevious);
  mPrevious = aPrevious;
  if (!mPrevious) {
    return;
  }

  MOZ_ASSERT(!mPrevious->mNext);
  mPrevious->mNext = this;
}

TemporaryRef<ClipNVpr> ClipNVpr::DetachFromPrevious()
{
  if (!mPrevious) {
    return nullptr;
  }

  mPrevious->mNext = nullptr;
  return mPrevious.forget();
}

}
}
