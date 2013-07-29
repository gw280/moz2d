/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "2D.h"
#include "TestBase.h"

#define DT_WIDTH 1000
#define DT_HEIGHT 1000

typedef void(*FlushFunc)(void*);

/* This general DrawTarget test class can be reimplemented by a child class
 * with optional additional drawtarget-specific tests. And is intended to run
 * on a 1000x1000 32 BPP drawtarget.
 */
class TestDrawTargetBase : public TestBase
{
public:
  void Initialized();

  void FillRect50x50x500();
  void FillRect50x50x2000();
  void FillRect200x200x500();
  void FillRect200x200x2000();
  void FillRect800x800x2000();
  void FillRect50x50x500Add();
  void FillRect50x50x2000Add();
  void FillRect200x200x500Add();
  void FillRect200x200x2000Add();
  void CreateGradientStops();
  void CreateSourceSurfaceForData100x100();
  void CreateSourceSurfaceForData200x200();
  void CreateSourceSurfaceForData500x500();
  void FillRadialSimple();
  void FillRadialComplex();
  void FillRadialSimpleUncached();
  void FillRadialComplexUncached();
  void DrawTransparentSurfaceUnscaledAligned();
  void DrawTransparentSurfaceUnscaled();
  void DrawTransparentSurfaceScaled();
  void DrawOpaqueSurfaceUnscaledAligned();
  void DrawOpaqueSurfaceUnscaled();
  void DrawOpaqueSurfaceScaled();
  void StrokeRectThin();
  void StrokeRectThick();
  void StrokeCurveThin();
  void StrokeCurveThinUncached();
  void StrokeCurveThick();
  void MaskSurface100x100();
  void MaskSurface500x500();
  void DrawShadow10x10SmallRadius();
  void DrawShadow200x200SmallRadius();
  void DrawShadow10x10LargeRadius();
  void DrawShadow200x200LargeRadius();
  void DrawMorphologyFilter100x100Radius40();

protected:
  FlushFunc mFlush;

  TestDrawTargetBase();

  void Flush() {
    if (mFlush) mFlush(this);
  }

  void FillSquare(int aSize, int aRepeat, mozilla::gfx::CompositionOp aOp = mozilla::gfx::OP_OVER);
  mozilla::TemporaryRef<mozilla::gfx::SourceSurface> CreateSquareRandomSourceSurface(int aSize, mozilla::gfx::SurfaceFormat aFormat);
  mozilla::TemporaryRef<mozilla::gfx::GradientStops> CreateSimpleGradientStops();

  mozilla::RefPtr<mozilla::gfx::DrawTarget> mDT;
};
