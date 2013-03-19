/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestDrawTargetBase.h"
#include <sstream>

using namespace mozilla;
using namespace mozilla::gfx;
using namespace std;

TestDrawTargetBase::TestDrawTargetBase()
  : mFlush(nullptr)
{
  REGISTER_TEST(TestDrawTargetBase, FillRect50x50x500);
  REGISTER_TEST(TestDrawTargetBase, FillRect200x200x500);
  REGISTER_TEST(TestDrawTargetBase, FillRect50x50x2000);
  REGISTER_TEST(TestDrawTargetBase, FillRect200x200x2000);
  REGISTER_TEST(TestDrawTargetBase, FillRect50x50x500Add);
  REGISTER_TEST(TestDrawTargetBase, FillRect200x200x500Add);
  REGISTER_TEST(TestDrawTargetBase, FillRect50x50x2000Add);
  REGISTER_TEST(TestDrawTargetBase, FillRect200x200x2000Add);
  REGISTER_TEST(TestDrawTargetBase, CreateGradientStops);
  REGISTER_TEST(TestDrawTargetBase, CreateSourceSurfaceForData100x100);
  REGISTER_TEST(TestDrawTargetBase, CreateSourceSurfaceForData200x200);
  REGISTER_TEST(TestDrawTargetBase, CreateSourceSurfaceForData500x500);
  REGISTER_TEST(TestDrawTargetBase, FillRadialSimple);
  REGISTER_TEST(TestDrawTargetBase, FillRadialComplex);
  REGISTER_TEST(TestDrawTargetBase, FillRadialSimpleUncached);
  REGISTER_TEST(TestDrawTargetBase, FillRadialComplexUncached);
  REGISTER_TEST(TestDrawTargetBase, DrawTransparentSurfaceUnscaled);
  REGISTER_TEST(TestDrawTargetBase, DrawTransparentSurfaceScaled);
  REGISTER_TEST(TestDrawTargetBase, DrawOpaqueSurfaceUnscaled);
  REGISTER_TEST(TestDrawTargetBase, DrawOpaqueSurfaceScaled);
}

void
TestDrawTargetBase::FillSquare(int aSize, int aRepeat, CompositionOp aOp)
{
  for (int i = 0; i < aRepeat; i++) {
    mDT->FillRect(Rect(i / 6, i / 4, aSize, aSize), ColorPattern(Color(1.0f, 0, 0, 1.0f)), DrawOptions(0.5f, aOp));
  }
  Flush();
}

void
TestDrawTargetBase::FillRect50x50x500()
{
  FillSquare(50, 500);
}

void
TestDrawTargetBase::FillRect200x200x500()
{
  FillSquare(200, 500);
}

void
TestDrawTargetBase::FillRect50x50x2000()
{
  FillSquare(50, 2000);
}

void
TestDrawTargetBase::FillRect200x200x2000()
{
  FillSquare(200, 2000);
}

void
TestDrawTargetBase::FillRect50x50x500Add()
{
  FillSquare(50, 500, OP_ADD);
}

void
TestDrawTargetBase::FillRect200x200x500Add()
{
  FillSquare(200, 500, OP_ADD);
}

void
TestDrawTargetBase::FillRect50x50x2000Add()
{
  FillSquare(50, 2000, OP_ADD);
}

void
TestDrawTargetBase::FillRect200x200x2000Add()
{
  FillSquare(200, 2000, OP_ADD);
}

void
TestDrawTargetBase::CreateGradientStops()
{
  GradientStop stops[2];
  stops[0].color = Color(1.0f, 0, 0, 1.0f);
  stops[0].offset = 0;
  stops[1].color = Color(0, 1.0f, 0, 1.0f);
  stops[1].offset = 1.0f;

  for (int i = 0; i < 500; i++) {
    RefPtr<GradientStops> dtStops = mDT->CreateGradientStops(stops, 2);
  }
}

void
TestDrawTargetBase::CreateSourceSurfaceForData100x100()
{
  unsigned char *surfData = new unsigned char[100 * 100 * 4];

  for (int i = 0; i < 200; i++) {
    RefPtr<SourceSurface> surf = mDT->CreateSourceSurfaceFromData(surfData, IntSize(100, 100), 400, FORMAT_B8G8R8A8);
  }

  delete [] surfData;
}

void
TestDrawTargetBase::CreateSourceSurfaceForData200x200()
{
  unsigned char *surfData = new unsigned char[200 * 200 * 4];

  for (int i = 0; i < 200; i++) {
    RefPtr<SourceSurface> surf = mDT->CreateSourceSurfaceFromData(surfData, IntSize(200, 200), 400, FORMAT_B8G8R8A8);
  }

  delete [] surfData;
}

void
TestDrawTargetBase::CreateSourceSurfaceForData500x500()
{
  unsigned char *surfData = new unsigned char[500 * 500 * 4];

  for (int i = 0; i < 200; i++) {
    RefPtr<SourceSurface> surf = mDT->CreateSourceSurfaceFromData(surfData, IntSize(500, 500), 2000, FORMAT_B8G8R8A8);
  }

  delete [] surfData;
}

void
TestDrawTargetBase::FillRadialSimple()
{
  RefPtr<GradientStops> stops = CreateSimpleGradientStops();
  for (int i = 0; i < 200; i++) {
    mDT->FillRect(Rect(i / 6, i / 4, 500, 500), RadialGradientPattern(Point(250, 250), Point(250, 250), 0, 500, stops));
  }
  Flush();
}

void
TestDrawTargetBase::FillRadialComplex()
{
  RefPtr<GradientStops> stops = CreateSimpleGradientStops();
  for (int i = 0; i < 200; i++) {
    mDT->FillRect(Rect(i / 6, i / 4, 500, 500), RadialGradientPattern(Point(250, 250), Point(300, 300), 40, 500, stops));
  }
  Flush();
}

void
TestDrawTargetBase::FillRadialSimpleUncached()
{
  for (int i = 0; i < 200; i++) {
    RefPtr<GradientStops> stops = CreateSimpleGradientStops();
    mDT->FillRect(Rect(float(i) / 6, float(i) / 4, 500, 500), RadialGradientPattern(Point(250, 250), Point(250, 250), 0, 500, stops));
  }
  Flush();
}

void
TestDrawTargetBase::FillRadialComplexUncached()
{
  for (int i = 0; i < 200; i++) {
    RefPtr<GradientStops> stops = CreateSimpleGradientStops();
    mDT->FillRect(Rect(float(i) / 6, float(i) / 4, 500, 500), RadialGradientPattern(Point(250, 250), Point(300, 300), 40, 500, stops));
  }
  Flush();
}

void
TestDrawTargetBase::DrawTransparentSurfaceUnscaled()
{
  RefPtr<SourceSurface> surf = CreateSquareRandomSourceSurface(400, true);
  for (int i = 0; i < 200; i++) {
    mDT->DrawSurface(surf, Rect(float(i) / 6, float(i) / 4, 400, 400), Rect(0, 0, 400, 400));
  }
  Flush();
}

void
TestDrawTargetBase::DrawTransparentSurfaceScaled()
{
  RefPtr<SourceSurface> surf = CreateSquareRandomSourceSurface(400, true);
  for (int i = 0; i < 200; i++) {
    mDT->DrawSurface(surf, Rect(float(i) / 6, float(i) / 4, 500, 500), Rect(0, 0, 400, 400));
  }
  Flush();
}

void
TestDrawTargetBase::DrawOpaqueSurfaceUnscaled()
{
  RefPtr<SourceSurface> surf = CreateSquareRandomSourceSurface(400, false);
  for (int i = 0; i < 200; i++) {
    mDT->DrawSurface(surf, Rect(float(i) / 6, float(i) / 4, 400, 400), Rect(0, 0, 400, 400));
  }
  Flush();
}

void
TestDrawTargetBase::DrawOpaqueSurfaceScaled()
{
  RefPtr<SourceSurface> surf = CreateSquareRandomSourceSurface(400, false);
  for (int i = 0; i < 200; i++) {
    mDT->DrawSurface(surf, Rect(float(i) / 6, float(i) / 4, 500, 500), Rect(0, 0, 400, 400));
  }
  Flush();
}

TemporaryRef<SourceSurface>
TestDrawTargetBase::CreateSquareRandomSourceSurface(int aSize, bool aAlpha)
{
  unsigned char *surfData = new unsigned char[aSize * aSize * 4];

  RefPtr<SourceSurface> surf = mDT->CreateSourceSurfaceFromData(surfData, IntSize(aSize, aSize), aSize * 4, aAlpha ? FORMAT_B8G8R8A8 : FORMAT_B8G8R8X8);

  delete [] surfData;

  return surf;
}

TemporaryRef<GradientStops>
TestDrawTargetBase::CreateSimpleGradientStops()
{
  GradientStop stops[2];
  stops[0].color = Color(1.0f, 0, 0, 1.0f);
  stops[0].offset = 0;
  stops[1].color = Color(0, 1.0f, 0, 1.0f);
  stops[1].offset = 1.0f;

  return mDT->CreateGradientStops(stops, 2);
}
