/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
  * This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
  
#pragma once
  
#include "2D.h"
#include "TestBase.h"
#include "TestHelpers.h"
  
#define DT_WIDTH 500
#define DT_HEIGHT 500
  
/* This general DrawTarget test class can be reimplemented by a child class
  * with optional additional drawtarget-specific tests. And is intended to run
  * on a 500x500 32 BPP drawtarget.
  */
class TestDrawTargetBase : public TestBase
{
public:
  void Initialized();
  void FillCompletely();
  void FillRect();
  void StrokeRect();
  void StrokeLine();
  void Translate();
  void FillMultiRect();
  void FillMultiRectTransform1();
  void FillMultiRectTransform2();
  void FillMultiRectTransform3();
  void ClipRect();
  void Clip();
  void FillTriangle();
  void StrokeTriangle();
  void DrawSurface();
  void FillWithSurface();
  void FillWithPartialLargeSurface();
  void FillWithScaledLargeSurface();
  void FillGradient();
  void FillRadialGradient();
  void FillWithSnapshot();
  void Mask();
  void CopySurface();
  void Shadow();
  void StreamToSink();
  
protected:
  TestDrawTargetBase();
  
  void RefreshSnapshot();
  
  void VerifyAllPixels(const mozilla::gfx::Color &aColor);
  void VerifyPixel(const mozilla::gfx::IntPoint &aPoint,
                   const mozilla::gfx::Color &aColor);
  
  uint32_t RGBAPixelFromColor(const mozilla::gfx::Color &aColor);
  
  mozilla::RefPtr<mozilla::gfx::DrawTarget> mDT;
  mozilla::RefPtr<mozilla::gfx::DataSourceSurface> mDataSnapshot;
}; 

#ifdef WIN32
IMPLEMENT_DT_TESTS(D2D, DIRECT2D, TestDrawTargetBase);
#endif
#ifdef USE_SKIA
IMPLEMENT_DT_TESTS(SkiaSoftware, SKIA, TestDrawTargetBase);
#endif
#ifdef USE_CAIRO
IMPLEMENT_DT_TESTS(CairoImage, CAIRO, TestDrawTargetBase);
#endif