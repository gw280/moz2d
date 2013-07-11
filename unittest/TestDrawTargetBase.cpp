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
  REGISTER_TEST(TestDrawTargetBase, ColorMatrix);
  REGISTER_TEST(TestDrawTargetBase, Blend);
  REGISTER_TEST(TestDrawTargetBase, Morphology);
  REGISTER_TEST(TestDrawTargetBase, Flood);
  REGISTER_TEST(TestDrawTargetBase, Tile);
  REGISTER_TEST(TestDrawTargetBase, TableTransfer);
  REGISTER_TEST(TestDrawTargetBase, DiscreteTransfer);
  REGISTER_TEST(TestDrawTargetBase, LinearTransfer);
  REGISTER_TEST(TestDrawTargetBase, GammaTransfer);
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
  mDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), ColorPattern(Color(0, 0.502f, 0, 1.0f)));

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.502f, 0, 1.0f));
}

void
TestDrawTargetBase::FillRect()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));
  mDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), ColorPattern(Color(0, 0.502f, 0, 1.0f)));
  mDT->FillRect(Rect(50, 50, 50, 50), ColorPattern(Color(0.502f, 0, 0, 1.0f)));

  RefreshSnapshot();

  VerifyPixel(IntPoint(49, 49), Color(0, 0.502f, 0, 1.0f));
  VerifyPixel(IntPoint(50, 50), Color(0.502f, 0, 0, 1.0f));
  VerifyPixel(IntPoint(99, 99), Color(0.502f, 0, 0, 1.0f));
  VerifyPixel(IntPoint(100, 100), Color(0, 0.502f, 0, 1.0f));
}

void
TestDrawTargetBase::StrokeRect()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));
  mDT->StrokeRect(Rect(DT_WIDTH / 4, DT_WIDTH / 4, DT_WIDTH / 2, DT_HEIGHT / 2),
                  ColorPattern(Color(0, 0.502f, 0, 1.0f)),
                  StrokeOptions(max(DT_WIDTH / 2, DT_HEIGHT / 2)));

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.502f, 0, 1.0f));
}

void
TestDrawTargetBase::StrokeLine()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));
  mDT->StrokeLine(Point(DT_WIDTH / 2, 0), Point(DT_WIDTH / 2, DT_HEIGHT),
                  ColorPattern(Color(0, 0.502f, 0, 1.0f)),
                  StrokeOptions(DT_WIDTH));

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.502f, 0, 1.0f));
}

void
TestDrawTargetBase::Translate()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));
  mDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), ColorPattern(Color(0, 0.502f, 0, 1.0f)));
  Matrix mat;
  mat.Translate(100, 100);
  mDT->SetTransform(mat);
  mDT->FillRect(Rect(50, 50, 50, 50), ColorPattern(Color(0.502f, 0, 0, 1.0f)));
  mDT->SetTransform(Matrix());

  RefreshSnapshot();

  VerifyPixel(IntPoint(149, 149), Color(0, 0.502f, 0, 1.0f));
  VerifyPixel(IntPoint(150, 150), Color(0.502f, 0, 0, 1.0f));
  VerifyPixel(IntPoint(199, 199), Color(0.502f, 0, 0, 1.0f));
  VerifyPixel(IntPoint(200, 200), Color(0, 0.502f, 0, 1.0f));
}

void
TestDrawTargetBase::ClipRect()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));
  mDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), ColorPattern(Color(0, 0.502f, 0, 1.0f)));
  mDT->PushClipRect(Rect(0, 0, 0, 0));
  mDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), ColorPattern(Color(1.0f, 0, 0, 1.0f)));
  mDT->PopClip();

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.502f, 0, 1.0f));
}

void
TestDrawTargetBase::Clip()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));
  mDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), ColorPattern(Color(0, 0.502f, 0, 1.0f)));

  RefPtr<PathBuilder> builder = mDT->CreatePathBuilder();
  builder->MoveTo(Point(0, 0));
  builder->LineTo(Point(0, 0));
  builder->Close();
  RefPtr<Path> path = builder->Finish();

  mDT->PushClip(path);
  mDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), ColorPattern(Color(1.0f, 0, 0, 1.0f)));
  mDT->PopClip();

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.502f, 0, 1.0f));
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

  mDT->Fill(path, ColorPattern(Color(0, 0.502f, 0, 1.0f)));

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.502f, 0, 1.0f));
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

  mDT->Stroke(path, ColorPattern(Color(0, 0.502f, 0, 1.0f)), StrokeOptions(500.0f));

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.502f, 0, 1.0f));
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

  VerifyAllPixels(Color(0, 0.502f, 0, 1.0f));
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

  VerifyAllPixels(Color(0, 0.502f, 0, 1.0f));
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

  VerifyAllPixels(Color(0, 0.502f, 0, 1.0f));
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

  VerifyAllPixels(Color(0, 0.502f, 0, 1.0f));
}

void
TestDrawTargetBase::FillGradient()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  GradientStop rawStops[2];
  rawStops[0].color = Color(0, 0.502f, 0, 1.0f);
  rawStops[0].offset = 0;
  rawStops[1].color = Color(0, 0.502f, 0, 1.0f);
  rawStops[1].offset = 1.0f;
  
  RefPtr<GradientStops> stops = mDT->CreateGradientStops(rawStops, 2);

  mDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), LinearGradientPattern(Point(0, 0), Point(0, DT_HEIGHT), stops));

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.502f, 0, 1.0f));
}

void
TestDrawTargetBase::FillRadialGradient()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  GradientStop rawStops[2];
  rawStops[0].color = Color(0, 0.502f, 0, 1.0f);
  rawStops[0].offset = 0;
  rawStops[1].color = Color(0, 0.502f, 0, 1.0f);
  rawStops[1].offset = 1.0f;
  
  RefPtr<GradientStops> stops = mDT->CreateGradientStops(rawStops, 2);

  mDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), RadialGradientPattern(Point(0, 0), Point(0, 0), 0, 1000, stops));

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.502f, 0, 1.0f));
}

void
TestDrawTargetBase::FillWithSnapshot()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  RefPtr<DrawTarget> tempDT = mDT->CreateSimilarDrawTarget(IntSize(20, 20), FORMAT_B8G8R8X8);
  tempDT->FillRect(Rect(0, 0, 20, 20), ColorPattern(Color(0, 0.502f, 0, 1.0f)));
  RefPtr<SourceSurface> src = tempDT->Snapshot();

  mDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), SurfacePattern(src, EXTEND_REPEAT));

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.502f, 0, 1.0f));
}

void
TestDrawTargetBase::Mask()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  RefPtr<DrawTarget> tempDT = mDT->CreateSimilarDrawTarget(IntSize(20, 20), FORMAT_A8);
  tempDT->FillRect(Rect(0, 0, 20, 20), ColorPattern(Color(1.0f, 1.0f, 1.0f, 1.0f)));
  RefPtr<SourceSurface> src = tempDT->Snapshot();

  mDT->Mask(ColorPattern(Color(0, 0.502f, 0, 1.0f)), SurfacePattern(src, EXTEND_REPEAT));

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.502f, 0, 1.0f));
}

void
TestDrawTargetBase::CopySurface()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  RefPtr<DrawTarget> tempDT = mDT->CreateSimilarDrawTarget(IntSize(DT_WIDTH, DT_HEIGHT), FORMAT_B8G8R8A8);
  tempDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), ColorPattern(Color(0, 0.502f, 0, 1.0f)));
  RefPtr<SourceSurface> src = tempDT->Snapshot();

  mDT->CopySurface(src, IntRect(0, 0, DT_WIDTH, DT_HEIGHT), IntPoint());

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.502f, 0, 1.0f));
}

void
TestDrawTargetBase::Shadow()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  RefPtr<DrawTarget> tempDT = mDT->CreateSimilarDrawTarget(IntSize(DT_WIDTH, DT_HEIGHT), FORMAT_B8G8R8A8);
  tempDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), ColorPattern(Color(1.0f, 0, 0, 1.0f)));
  RefPtr<SourceSurface> src = tempDT->Snapshot();

  mDT->DrawSurfaceWithShadow(src, Point(-DT_WIDTH, -DT_HEIGHT), Color(0, 0.502f, 0, 1.0f), Point(DT_WIDTH, DT_HEIGHT), 0, OP_OVER);

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.502f, 0, 1.0f));
}

void
TestDrawTargetBase::ColorMatrix()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  RefPtr<FilterNode> filter = mDT->CreateFilter(FILTER_COLOR_MATRIX);

  Matrix5x4 mat;
  mat._52 = 127.0f / 255.0f;
  mat._54 = 1.0f;

  filter->SetAttribute(0, mat);

  uint32_t *data = new uint32_t[DT_WIDTH * DT_HEIGHT * 4];

  memset(data, 0, DT_WIDTH * DT_HEIGHT * 4);
  {
    RefPtr<SourceSurface> src =
      mDT->CreateSourceSurfaceFromData((uint8_t*)data, IntSize(DT_WIDTH, DT_HEIGHT), DT_WIDTH * 4, FORMAT_B8G8R8A8);

    filter->SetInput(0, src);

    mDT->DrawFilter(filter, Rect(0, 0, DT_WIDTH, DT_HEIGHT), Point());
  }

  delete [] data;

  RefreshSnapshot();

  VerifyAllPixels(Color(0, mat._52, 0, mat._54));
}

void
TestDrawTargetBase::Blend()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  RefPtr<FilterNode> filter = mDT->CreateFilter(FILTER_BLEND);

  filter->SetAttribute(ATT_BLEND_BLENDMODE, (uint32_t)BLEND_MODE_MULTIPLY);

  uint32_t *data = new uint32_t[DT_WIDTH * DT_HEIGHT * 4];
  for (int i = 0; i < DT_WIDTH * DT_HEIGHT; i++) {
    data[i] = 0xff008000;
  }

  {
    RefPtr<SourceSurface> src =
      mDT->CreateSourceSurfaceFromData((uint8_t*)data, IntSize(DT_WIDTH, DT_HEIGHT), DT_WIDTH * 4, FORMAT_B8G8R8A8);
    RefPtr<SourceSurface> src2 =
      mDT->CreateSourceSurfaceFromData((uint8_t*)data, IntSize(DT_WIDTH, DT_HEIGHT), DT_WIDTH * 4, FORMAT_B8G8R8A8);

    filter->SetInput(0, src);
    filter->SetInput(1, src2);

    mDT->DrawFilter(filter, Rect(0, 0, DT_WIDTH, DT_HEIGHT), Point());
  }

  delete [] data;

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.25f, 0, 1.0f));
}

void
TestDrawTargetBase::Morphology()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  RefPtr<FilterNode> filter = mDT->CreateFilter(FILTER_MORPHOLOGY);

  filter->SetAttribute(ATT_MORPHOLOGY_RADII, IntSize(10, 10));
  filter->SetAttribute(ATT_MORPHOLOGY_OPERATOR, (uint32_t)MORPHOLOGY_OPERATOR_DILATE);

  RefPtr<DrawTarget> dt = mDT->CreateSimilarDrawTarget(IntSize(500, 500), FORMAT_B8G8R8A8);
  dt->FillRect(Rect(10, 10, 480, 480), ColorPattern(Color(0, 0.502f, 0, 1.0f)));

  RefPtr<SourceSurface> src = dt->Snapshot();
  filter->SetInput(0, src);

  mDT->DrawFilter(filter, Rect(0, 0, DT_WIDTH, DT_HEIGHT), Point());

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.502f, 0, 1.0f));
}

void
TestDrawTargetBase::Flood()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  RefPtr<FilterNode> filter = mDT->CreateFilter(FILTER_FLOOD);

  filter->SetAttribute(ATT_FLOOD_COLOR, Color(0, 0.502f, 0, 1.0f));

  mDT->DrawFilter(filter, Rect(0, 0, DT_WIDTH, DT_HEIGHT), Point());

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.502f, 0, 1.0f));
}

void
TestDrawTargetBase::Tile()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  RefPtr<FilterNode> filter = mDT->CreateFilter(FILTER_TILE);

  RefPtr<DrawTarget> dt = mDT->CreateSimilarDrawTarget(IntSize(500, 500), FORMAT_B8G8R8A8);
  dt->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), ColorPattern(Color(1.0f, 0, 0, 1.0f)));
  dt->FillRect(Rect(100, 100, 300, 300), ColorPattern(Color(0, 0.502f, 0, 1.0f)));

  RefPtr<SourceSurface> src = dt->Snapshot();
  filter->SetInput(0, src);

  filter->SetAttribute(ATT_TILE_SOURCE_RECT, Rect(100, 100, 300, 300));

  mDT->DrawFilter(filter, Rect(0, 0, DT_WIDTH, DT_HEIGHT), Point());

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.502f, 0, 1.0f));
}

void
TestDrawTargetBase::TableTransfer()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  RefPtr<FilterNode> filter = mDT->CreateFilter(FILTER_TABLE_TRANSFER);

  filter->SetAttribute(ATT_TABLE_TRANSFER_DISABLE_R, false);
  filter->SetAttribute(ATT_TABLE_TRANSFER_DISABLE_G, false);
  filter->SetAttribute(ATT_TABLE_TRANSFER_DISABLE_B, false);
  filter->SetAttribute(ATT_TABLE_TRANSFER_DISABLE_A, true);

  Float coeffs[] = { 0, 0, 1.0f };

  filter->SetAttribute(ATT_TABLE_TRANSFER_TABLE_R, coeffs, 3);
  filter->SetAttribute(ATT_TABLE_TRANSFER_TABLE_G, coeffs, 3);
  filter->SetAttribute(ATT_TABLE_TRANSFER_TABLE_B, coeffs, 3);

  uint32_t *data = new uint32_t[DT_WIDTH * DT_HEIGHT * 4];
  uint32_t pixelVal = Color(0.498f, 0.75f, 0.498f, 1.0f).ToABGR();
  for (int i = 0; i < DT_WIDTH * DT_HEIGHT; i++) {
    data[i] = pixelVal;
  }

  {
    RefPtr<SourceSurface> src =
      mDT->CreateSourceSurfaceFromData((uint8_t*)data, IntSize(DT_WIDTH, DT_HEIGHT), DT_WIDTH * 4, FORMAT_B8G8R8A8);

    filter->SetInput(0, src);

    mDT->DrawFilter(filter, Rect(0, 0, DT_WIDTH, DT_HEIGHT), Point());
  }

  delete [] data;

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.498f, 0, 1.0f));
}

void
TestDrawTargetBase::DiscreteTransfer()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  RefPtr<FilterNode> filter = mDT->CreateFilter(FILTER_DISCRETE_TRANSFER);

  filter->SetAttribute(ATT_DISCRETE_TRANSFER_DISABLE_R, false);
  filter->SetAttribute(ATT_DISCRETE_TRANSFER_DISABLE_G, false);
  filter->SetAttribute(ATT_DISCRETE_TRANSFER_DISABLE_B, false);
  filter->SetAttribute(ATT_DISCRETE_TRANSFER_DISABLE_A, true);

  Float coeffs[] = { 0, 0.502f, 0, 1.0f };

  filter->SetAttribute(ATT_DISCRETE_TRANSFER_TABLE_R, coeffs, 3);
  filter->SetAttribute(ATT_DISCRETE_TRANSFER_TABLE_G, coeffs, 3);
  filter->SetAttribute(ATT_DISCRETE_TRANSFER_TABLE_B, coeffs, 3);

  uint32_t *data = new uint32_t[DT_WIDTH * DT_HEIGHT * 4];
  uint32_t pixelVal = Color(0.7f, 0.4f, 0.2f, 1.0f).ToABGR();
  for (int i = 0; i < DT_WIDTH * DT_HEIGHT; i++) {
    data[i] = pixelVal;
  }

  {
    RefPtr<SourceSurface> src =
      mDT->CreateSourceSurfaceFromData((uint8_t*)data, IntSize(DT_WIDTH, DT_HEIGHT), DT_WIDTH * 4, FORMAT_B8G8R8A8);

    filter->SetInput(0, src);

    mDT->DrawFilter(filter, Rect(0, 0, DT_WIDTH, DT_HEIGHT), Point());
  }

  delete [] data;

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.502f, 0, 1.0f));
}

void
TestDrawTargetBase::LinearTransfer()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  RefPtr<FilterNode> filter = mDT->CreateFilter(FILTER_LINEAR_TRANSFER);

  filter->SetAttribute(ATT_LINEAR_TRANSFER_DISABLE_R, false);
  filter->SetAttribute(ATT_LINEAR_TRANSFER_DISABLE_G, false);
  filter->SetAttribute(ATT_LINEAR_TRANSFER_DISABLE_B, false);
  filter->SetAttribute(ATT_LINEAR_TRANSFER_DISABLE_A, true);

  filter->SetAttribute(ATT_LINEAR_TRANSFER_INTERCEPT_R, 0.502f);
  filter->SetAttribute(ATT_LINEAR_TRANSFER_SLOPE_R, -5.0f);
  filter->SetAttribute(ATT_LINEAR_TRANSFER_INTERCEPT_G, 0.f);
  filter->SetAttribute(ATT_LINEAR_TRANSFER_SLOPE_G, 1.0f);
  filter->SetAttribute(ATT_LINEAR_TRANSFER_INTERCEPT_B, 0.502f);
  filter->SetAttribute(ATT_LINEAR_TRANSFER_SLOPE_B, -5.0f);

  uint32_t *data = new uint32_t[DT_WIDTH * DT_HEIGHT * 4];
  for (int i = 0; i < DT_WIDTH * DT_HEIGHT; i++) {
    data[i] = 0xff808080;
  }

  {
    RefPtr<SourceSurface> src =
      mDT->CreateSourceSurfaceFromData((uint8_t*)data, IntSize(DT_WIDTH, DT_HEIGHT), DT_WIDTH * 4, FORMAT_B8G8R8A8);

    filter->SetInput(0, src);

    mDT->DrawFilter(filter, Rect(0, 0, DT_WIDTH, DT_HEIGHT), Point());
  }

  delete [] data;

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.502f, 0, 1.0f));
}

void
TestDrawTargetBase::GammaTransfer()
{
  mDT->ClearRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT));

  RefPtr<FilterNode> filter = mDT->CreateFilter(FILTER_GAMMA_TRANSFER);

  filter->SetAttribute(ATT_GAMMA_TRANSFER_DISABLE_R, false);
  filter->SetAttribute(ATT_GAMMA_TRANSFER_DISABLE_G, false);
  filter->SetAttribute(ATT_GAMMA_TRANSFER_DISABLE_B, false);
  filter->SetAttribute(ATT_GAMMA_TRANSFER_DISABLE_A, true);

  filter->SetAttribute(ATT_GAMMA_TRANSFER_AMPLITUDE_R, 0.f);
  filter->SetAttribute(ATT_GAMMA_TRANSFER_EXPONENT_R, 1.f);
  filter->SetAttribute(ATT_GAMMA_TRANSFER_OFFSET_R, 0.f);
  filter->SetAttribute(ATT_GAMMA_TRANSFER_AMPLITUDE_G, 1.0f);
  filter->SetAttribute(ATT_GAMMA_TRANSFER_EXPONENT_G, 2.0f);
  filter->SetAttribute(ATT_GAMMA_TRANSFER_OFFSET_G, 0.25f);
  filter->SetAttribute(ATT_GAMMA_TRANSFER_AMPLITUDE_B, 2.0f);
  filter->SetAttribute(ATT_GAMMA_TRANSFER_EXPONENT_B, 2.0f);
  filter->SetAttribute(ATT_GAMMA_TRANSFER_OFFSET_B, -0.502f);

  uint32_t *data = new uint32_t[DT_WIDTH * DT_HEIGHT * 4];
  for (int i = 0; i < DT_WIDTH * DT_HEIGHT; i++) {
    data[i] = 0xff808080;
  }

  {
    RefPtr<SourceSurface> src =
      mDT->CreateSourceSurfaceFromData((uint8_t*)data, IntSize(DT_WIDTH, DT_HEIGHT), DT_WIDTH * 4, FORMAT_B8G8R8A8);

    filter->SetInput(0, src);

    mDT->DrawFilter(filter, Rect(0, 0, DT_WIDTH, DT_HEIGHT), Point());
  }

  delete [] data;

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.502f, 0, 1.0f));
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
        stringstream message;
        uint32_t rawActual = colVal[y * (mDataSnapshot->Stride() / 4) + x];
        uint32_t actb = rawActual & 0xFF;
        uint32_t actg = (rawActual & 0xFF00) >> 8;
        uint32_t actr = (rawActual & 0xFF0000) >> 16;
        uint32_t acta = (rawActual & 0xFF000000) >> 24;
        uint32_t expb = expected & 0xFF;
        uint32_t expg = (expected & 0xFF00) >> 8;
        uint32_t expr = (expected & 0xFF0000) >> 16;
        uint32_t expa = (expected & 0xFF000000) >> 24;

        message << "Verify Pixel (" << x << "x" << y << ") Failed."
          " Expected (" << expr << "," << expg << "," << expb << "," << expa << ") "
          " Got (" << actr << "," << actg << "," << actb << "," << acta << ")\n";

        LogMessage(message.str());
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
