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
{
  REGISTER_TEST(TestDrawTargetBase, Initialized);
  REGISTER_TEST(TestDrawTargetBase, FillCompletely);
  REGISTER_TEST(TestDrawTargetBase, FillRect);
  REGISTER_TEST(TestDrawTargetBase, StrokeRect);
  REGISTER_TEST(TestDrawTargetBase, StrokeLine);
  REGISTER_TEST(TestDrawTargetBase, Translate);
  REGISTER_TEST(TestDrawTargetBase, ClipRect);
  REGISTER_TEST(TestDrawTargetBase, Clip);
  REGISTER_TEST(TestDrawTargetBase, FillTriangle);
  REGISTER_TEST(TestDrawTargetBase, StrokeTriangle);
  REGISTER_TEST(TestDrawTargetBase, DrawSurface);
  REGISTER_TEST(TestDrawTargetBase, FillWithSurface);
  REGISTER_TEST(TestDrawTargetBase, FillWithPartialLargeSurface);
  REGISTER_TEST(TestDrawTargetBase, FillWithScaledLargeSurface);
  REGISTER_TEST(TestDrawTargetBase, FillGradient);
  REGISTER_TEST(TestDrawTargetBase, FillRadialGradient);
  REGISTER_TEST(TestDrawTargetBase, FillWithSnapshot);
  REGISTER_TEST(TestDrawTargetBase, Mask);
  REGISTER_TEST(TestDrawTargetBase, CopySurface);
  REGISTER_TEST(TestDrawTargetBase, Shadow);
}

void
TestDrawTargetBase::Initialized()
{
  VERIFY(mDT);
}

void
TestDrawTargetBase::FillCompletely()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));
  mDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), ColorPattern(Color(0, 0.5f, 0, 1.0f)));

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.5f, 0, 1.0f));
}

void
TestDrawTargetBase::FillRect()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));
  mDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), ColorPattern(Color(0, 0.5f, 0, 1.0f)));
  mDT->FillRect(Rect(50, 50, 50, 50), ColorPattern(Color(0.5f, 0, 0, 1.0f)));

  RefreshSnapshot();

  VerifyPixel(IntPoint(49, 49), Color(0, 0.5f, 0, 1.0f));
  VerifyPixel(IntPoint(50, 50), Color(0.5f, 0, 0, 1.0f));
  VerifyPixel(IntPoint(99, 99), Color(0.5f, 0, 0, 1.0f));
  VerifyPixel(IntPoint(100, 100), Color(0, 0.5f, 0, 1.0f));
}

void
TestDrawTargetBase::StrokeRect()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));
  mDT->StrokeRect(Rect(DT_WIDTH / 4, DT_WIDTH / 4, DT_WIDTH / 2, DT_HEIGHT / 2),
                  ColorPattern(Color(0, 0.5f, 0, 1.0f)),
                  StrokeOptions(max(DT_WIDTH / 2, DT_HEIGHT / 2)));

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.5f, 0, 1.0f));
}

void
TestDrawTargetBase::StrokeLine()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));
  mDT->StrokeLine(Point(DT_WIDTH / 2, 0), Point(DT_WIDTH / 2, DT_HEIGHT),
                  ColorPattern(Color(0, 0.5f, 0, 1.0f)),
                  StrokeOptions(DT_WIDTH));

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.5f, 0, 1.0f));
}

void
TestDrawTargetBase::Translate()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));
  mDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), ColorPattern(Color(0, 0.5f, 0, 1.0f)));
  Matrix mat;
  mat.Translate(100, 100);
  mDT->SetTransform(mat);
  mDT->FillRect(Rect(50, 50, 50, 50), ColorPattern(Color(0.5f, 0, 0, 1.0f)));
  mDT->SetTransform(Matrix());

  RefreshSnapshot();

  VerifyPixel(IntPoint(149, 149), Color(0, 0.5f, 0, 1.0f));
  VerifyPixel(IntPoint(150, 150), Color(0.5f, 0, 0, 1.0f));
  VerifyPixel(IntPoint(199, 199), Color(0.5f, 0, 0, 1.0f));
  VerifyPixel(IntPoint(200, 200), Color(0, 0.5f, 0, 1.0f));
}

void
TestDrawTargetBase::ClipRect()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));
  mDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), ColorPattern(Color(0, 0.5f, 0, 1.0f)));
  mDT->PushClipRect(Rect(0, 0, 0, 0));
  mDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), ColorPattern(Color(1.0f, 0, 0, 1.0f)));
  mDT->PopClip();

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.5f, 0, 1.0f));
}

void
TestDrawTargetBase::Clip()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));
  mDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), ColorPattern(Color(0, 0.5f, 0, 1.0f)));

  RefPtr<PathBuilder> builder = mDT->CreatePathBuilder();
  builder->MoveTo(Point(0, 0));
  builder->LineTo(Point(0, 0));
  builder->Close();
  RefPtr<Path> path = builder->Finish();

  mDT->PushClip(path);
  mDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), ColorPattern(Color(1.0f, 0, 0, 1.0f)));
  mDT->PopClip();

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.5f, 0, 1.0f));
}
void
TestDrawTargetBase::FillTriangle()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  RefPtr<PathBuilder> builder = mDT->CreatePathBuilder();
  builder->MoveTo(Point(-10000, -10000));
  builder->LineTo(Point(10000, -10000));
  builder->LineTo(Point(0, 10000));
  builder->Close();
  RefPtr<Path> path = builder->Finish();

  mDT->Fill(path, ColorPattern(Color(0, 0.5f, 0, 1.0f)));

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.5f, 0, 1.0f));
}

void
TestDrawTargetBase::StrokeTriangle()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  RefPtr<PathBuilder> builder = mDT->CreatePathBuilder();
  builder->MoveTo(Point(0, 0));
  builder->LineTo(Point(250, 500));
  builder->LineTo(Point(500, 0));
  builder->Close();
  RefPtr<Path> path = builder->Finish();

  mDT->Stroke(path, ColorPattern(Color(0, 0.5f, 0, 1.0f)), StrokeOptions(500.0f));

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.5f, 0, 1.0f));
}

void
TestDrawTargetBase::DrawSurface()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  uint32_t pixel = 0xff008000;

  RefPtr<SourceSurface> src =
    mDT->CreateSourceSurfaceFromData((uint8_t*)&pixel, IntSize(1, 1), 4, FORMAT_B8G8R8A8);

  mDT->DrawSurface(src, Rect(0, 0, DT_WIDTH, DT_HEIGHT), Rect(0, 0, 1, 1));

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.5f, 0, 1.0f));
}

void
TestDrawTargetBase::FillWithPartialLargeSurface()
{
  // This test will test if a DrawTarget correctly displays an extremely
  // large image when only part of it is shown.
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  uint32_t *data = new uint32_t[18000 * DT_HEIGHT];

  for (int i = 0; i < 18000 * DT_HEIGHT; i++) {
    data[i] = 0xff008000;
  }

  {
    RefPtr<DataSourceSurface> src =
      Factory::CreateWrappingDataSourceSurface((uint8_t*)data, 18000 * 4, IntSize(18000, DT_HEIGHT), FORMAT_B8G8R8A8);

    mDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), SurfacePattern(src, EXTEND_REPEAT));
  }

  delete [] data;

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.5f, 0, 1.0f));
}

void
TestDrawTargetBase::FillWithScaledLargeSurface()
{
  // This test will test if a DrawTarget correctly displays an extremely
  // large image when it is scaled down to be completely visible.
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  uint32_t *data = new uint32_t[18000 * DT_HEIGHT];

  for (int i = 0; i < 18000 * DT_HEIGHT; i++) {
    data[i] = 0xff008000;
  }

  {
    RefPtr<DataSourceSurface> src =
      Factory::CreateWrappingDataSourceSurface((uint8_t*)data, 18000 * 4, IntSize(18000, 18000), FORMAT_B8G8R8A8);

    Matrix mat;
    mat.Scale(Float(DT_WIDTH) / 18000, Float(DT_HEIGHT));
    mDT->FillRect(Rect(0, 0, 18000, DT_HEIGHT), SurfacePattern(src, EXTEND_REPEAT));
  }

  delete [] data;

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.5f, 0, 1.0f));
}

void
TestDrawTargetBase::FillWithSurface()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  uint32_t pixel = 0xff008000;

  RefPtr<SourceSurface> src =
    mDT->CreateSourceSurfaceFromData((uint8_t*)&pixel, IntSize(1, 1), 4, FORMAT_B8G8R8A8);

  mDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), SurfacePattern(src, EXTEND_REPEAT));

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.5f, 0, 1.0f));
}

void
TestDrawTargetBase::FillGradient()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  GradientStop rawStops[2];
  rawStops[0].color = Color(0, 0.5f, 0, 1.0f);
  rawStops[0].offset = 0;
  rawStops[1].color = Color(0, 0.5f, 0, 1.0f);
  rawStops[1].offset = 1.0f;
  
  RefPtr<GradientStops> stops = mDT->CreateGradientStops(rawStops, 2);

  mDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), LinearGradientPattern(Point(0, 0), Point(0, DT_HEIGHT), stops));

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.5f, 0, 1.0f));
}

void
TestDrawTargetBase::FillRadialGradient()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  GradientStop rawStops[2];
  rawStops[0].color = Color(0, 0.5f, 0, 1.0f);
  rawStops[0].offset = 0;
  rawStops[1].color = Color(0, 0.5f, 0, 1.0f);
  rawStops[1].offset = 1.0f;
  
  RefPtr<GradientStops> stops = mDT->CreateGradientStops(rawStops, 2);

  mDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), RadialGradientPattern(Point(0, 0), Point(0, 0), 0, 1000, stops));

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.5f, 0, 1.0f));
}

void
TestDrawTargetBase::FillWithSnapshot()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  RefPtr<DrawTarget> tempDT = mDT->CreateSimilarDrawTarget(IntSize(20, 20), FORMAT_B8G8R8X8);
  tempDT->FillRect(Rect(0, 0, 20, 20), ColorPattern(Color(0, 0.5f, 0, 1.0f)));
  RefPtr<SourceSurface> src = tempDT->Snapshot();

  mDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), SurfacePattern(src, EXTEND_REPEAT));

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.5f, 0, 1.0f));
}

void
TestDrawTargetBase::Mask()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  RefPtr<DrawTarget> tempDT = mDT->CreateSimilarDrawTarget(IntSize(20, 20), FORMAT_A8);
  tempDT->FillRect(Rect(0, 0, 20, 20), ColorPattern(Color(1.0f, 1.0f, 1.0f, 1.0f)));
  RefPtr<SourceSurface> src = tempDT->Snapshot();

  mDT->Mask(ColorPattern(Color(0, 0.5f, 0, 1.0f)), SurfacePattern(src, EXTEND_REPEAT));

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.5f, 0, 1.0f));
}

void
TestDrawTargetBase::CopySurface()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  RefPtr<DrawTarget> tempDT = mDT->CreateSimilarDrawTarget(IntSize(DT_WIDTH, DT_HEIGHT), FORMAT_B8G8R8A8);
  tempDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), ColorPattern(Color(0, 0.5f, 0, 1.0f)));
  RefPtr<SourceSurface> src = tempDT->Snapshot();

  mDT->CopySurface(src, IntRect(0, 0, DT_WIDTH, DT_HEIGHT), IntPoint());

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.5f, 0, 1.0f));
}

void
TestDrawTargetBase::Shadow()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  RefPtr<DrawTarget> tempDT = mDT->CreateSimilarDrawTarget(IntSize(DT_WIDTH, DT_HEIGHT), FORMAT_B8G8R8A8);
  tempDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), ColorPattern(Color(1.0f, 0, 0, 1.0f)));
  RefPtr<SourceSurface> src = tempDT->Snapshot();

  mDT->DrawSurfaceWithShadow(src, Point(-DT_WIDTH, -DT_HEIGHT), Color(0, 0.5f, 0, 1.0f), Point(DT_WIDTH, DT_HEIGHT), 0, OP_OVER);

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.5f, 0, 1.0f));
}

void
TestDrawTargetBase::RefreshSnapshot()
{
  RefPtr<SourceSurface> snapshot = mDT->Snapshot();
  mDataSnapshot = snapshot->GetDataSurface();
}

void
TestDrawTargetBase::VerifyAllPixels(const Color &aColor)
{
  uint32_t *colVal = (uint32_t*)mDataSnapshot->GetData();

  uint32_t expected = RGBAPixelFromColor(aColor);

  for (int y = 0; y < DT_HEIGHT; y++) {
    for (int x = 0; x < DT_WIDTH; x++) {
      if (colVal[y * (mDataSnapshot->Stride() / 4) + x] != expected) {
        LogMessage("VerifyAllPixels Failed\n");
        mTestFailed = true;
        return;
      }
    }
  }
}

void
TestDrawTargetBase::VerifyPixel(const IntPoint &aPoint, const mozilla::gfx::Color &aColor)
{
  uint32_t *colVal = (uint32_t*)mDataSnapshot->GetData();

  uint32_t expected = RGBAPixelFromColor(aColor);
  uint32_t rawActual = colVal[aPoint.y * (mDataSnapshot->Stride() / 4) + aPoint.x];

  if (rawActual != expected) {
    stringstream message;
    uint32_t actb = rawActual & 0xFF;
    uint32_t actg = (rawActual & 0xFF00) >> 8;
    uint32_t actr = (rawActual & 0xFF0000) >> 16;
    uint32_t acta = (rawActual & 0xFF000000) >> 24;
    uint32_t expb = expected & 0xFF;
    uint32_t expg = (expected & 0xFF00) >> 8;
    uint32_t expr = (expected & 0xFF0000) >> 16;
    uint32_t expa = (expected & 0xFF000000) >> 24;

    message << "Verify Pixel (" << aPoint.x << "x" << aPoint.y << ") Failed."
      " Expected (" << expr << "," << expg << "," << expb << "," << expa << ") "
      " Got (" << actr << "," << actg << "," << actb << "," << acta << ")\n";

    LogMessage(message.str());
    mTestFailed = true;
    return;
  }
}

uint32_t
TestDrawTargetBase::RGBAPixelFromColor(const Color &aColor)
{
  return uint8_t((aColor.b * 255) + 0.5f) | uint8_t((aColor.g * 255) + 0.5f) << 8 |
         uint8_t((aColor.r * 255) + 0.5f) << 16 | uint8_t((aColor.a * 255) + 0.5f) << 24;
}
