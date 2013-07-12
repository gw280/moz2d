/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FilterNodeD2D1.h"

#include "2D.h"
#include "Logging.h"

#include "SourceSurfaceD2D1.h"

namespace mozilla {
namespace gfx {

D2D1_BLEND_MODE D2DBlendMode(uint32_t aMode)
{
  switch (aMode) {
  case BLEND_MODE_DARKEN:
    return D2D1_BLEND_MODE_DARKEN;
  case BLEND_MODE_LIGHTEN:
    return D2D1_BLEND_MODE_LIGHTEN;
  case BLEND_MODE_MULTIPLY:
    return D2D1_BLEND_MODE_MULTIPLY;
  case BLEND_MODE_SCREEN:
    return D2D1_BLEND_MODE_SCREEN;
  default:
    MOZ_NOT_REACHED("Unknown enum value!");
  }

  return D2D1_BLEND_MODE_DARKEN;
}

D2D1_MORPHOLOGY_MODE D2DMorphologyMode(uint32_t aMode)
{
  switch (aMode) {
  case MORPHOLOGY_OPERATOR_DILATE:
    return D2D1_MORPHOLOGY_MODE_DILATE;
  case MORPHOLOGY_OPERATOR_ERODE:
    return D2D1_MORPHOLOGY_MODE_ERODE;
  }

  MOZ_NOT_REACHED("Unknown enum value!");
  return D2D1_MORPHOLOGY_MODE_DILATE;
}

D2D1_CHANNEL_SELECTOR D2DChannelSelector(uint32_t aMode)
{
  switch (aMode) {
  case COLOR_CHANNEL_R:
    return D2D1_CHANNEL_SELECTOR_R;
  case COLOR_CHANNEL_G:
    return D2D1_CHANNEL_SELECTOR_G;
  case COLOR_CHANNEL_B:
    return D2D1_CHANNEL_SELECTOR_B;
  case COLOR_CHANNEL_A:
    return D2D1_CHANNEL_SELECTOR_A;
  }

  MOZ_NOT_REACHED("Unknown enum value!");
  return D2D1_CHANNEL_SELECTOR_R;
}

uint32_t ConvertValue(uint32_t aType, uint32_t aAttribute, uint32_t aValue)
{
  switch (aType) {
  case FILTER_BLEND:
    if (aAttribute == ATT_BLEND_BLENDMODE) {
      aValue = D2DBlendMode(aValue);
    }
    break;
  case FILTER_MORPHOLOGY:
    if (aAttribute == ATT_MORPHOLOGY_OPERATOR) {
      aValue = D2DMorphologyMode(aValue);
    }
    break;
  case FILTER_DISPLACEMENT_MAP:
    if (aAttribute == ATT_DISPLACEMENT_MAP_X_CHANNEL ||
        aAttribute == ATT_DISPLACEMENT_MAP_Y_CHANNEL) {
      aValue = D2DChannelSelector(aValue);
    }
  }
  return aValue;
}

void ConvertValue(uint32_t aType, uint32_t aAttribute, IntSize &aValue)
{
  switch (aType) {
  case FILTER_MORPHOLOGY:
    if (aAttribute == ATT_MORPHOLOGY_RADII) {
      aValue.width *= 2;
      aValue.width += 1;
      aValue.height *= 2;
      aValue.height += 1;
    }
    break;
  }
}

UINT32
GetD2D1InputForInput(uint32_t aType, uint32_t aIndex)
{
  return aIndex;
}

#define CONVERT_PROP(moz2dname, d2dname) \
  case ATT_##moz2dname: \
  return D2D1_##d2dname

UINT32
GetD2D1PropForAttribute(uint32_t aType, uint32_t aIndex)
{
  switch (aType) {
  case FILTER_COLOR_MATRIX:
    switch (aIndex) {
      CONVERT_PROP(COLOR_MATRIX_MATRIX, COLORMATRIX_PROP_COLOR_MATRIX);
    }
    break;
  case FILTER_BLEND:
    switch (aIndex) {
      CONVERT_PROP(BLEND_BLENDMODE, BLEND_PROP_MODE);
    }
    break;
  case FILTER_MORPHOLOGY:
    switch (aIndex) {
      CONVERT_PROP(MORPHOLOGY_OPERATOR, MORPHOLOGY_PROP_MODE);
    }
    break;
  case FILTER_FLOOD:
    switch (aIndex) {
      CONVERT_PROP(FLOOD_COLOR, FLOOD_PROP_COLOR);
    }
    break;
  case FILTER_TILE:
    switch (aIndex) {
      CONVERT_PROP(TILE_SOURCE_RECT, TILE_PROP_RECT);
    }
    break;
  case FILTER_TABLE_TRANSFER:
    switch (aIndex) {
      CONVERT_PROP(TABLE_TRANSFER_DISABLE_R, TABLETRANSFER_PROP_RED_DISABLE);
      CONVERT_PROP(TABLE_TRANSFER_DISABLE_G, TABLETRANSFER_PROP_GREEN_DISABLE);
      CONVERT_PROP(TABLE_TRANSFER_DISABLE_B, TABLETRANSFER_PROP_BLUE_DISABLE);
      CONVERT_PROP(TABLE_TRANSFER_DISABLE_A, TABLETRANSFER_PROP_ALPHA_DISABLE);
      CONVERT_PROP(TABLE_TRANSFER_TABLE_R, TABLETRANSFER_PROP_RED_TABLE);
      CONVERT_PROP(TABLE_TRANSFER_TABLE_G, TABLETRANSFER_PROP_GREEN_TABLE);
      CONVERT_PROP(TABLE_TRANSFER_TABLE_B, TABLETRANSFER_PROP_BLUE_TABLE);
      CONVERT_PROP(TABLE_TRANSFER_TABLE_A, TABLETRANSFER_PROP_ALPHA_TABLE);
    }
    break;
  case FILTER_DISCRETE_TRANSFER:
    switch (aIndex) {
      CONVERT_PROP(DISCRETE_TRANSFER_DISABLE_R, DISCRETETRANSFER_PROP_RED_DISABLE);
      CONVERT_PROP(DISCRETE_TRANSFER_DISABLE_G, DISCRETETRANSFER_PROP_GREEN_DISABLE);
      CONVERT_PROP(DISCRETE_TRANSFER_DISABLE_B, DISCRETETRANSFER_PROP_BLUE_DISABLE);
      CONVERT_PROP(DISCRETE_TRANSFER_DISABLE_A, DISCRETETRANSFER_PROP_ALPHA_DISABLE);
      CONVERT_PROP(DISCRETE_TRANSFER_TABLE_R, DISCRETETRANSFER_PROP_RED_TABLE);
      CONVERT_PROP(DISCRETE_TRANSFER_TABLE_G, DISCRETETRANSFER_PROP_GREEN_TABLE);
      CONVERT_PROP(DISCRETE_TRANSFER_TABLE_B, DISCRETETRANSFER_PROP_BLUE_TABLE);
      CONVERT_PROP(DISCRETE_TRANSFER_TABLE_A, DISCRETETRANSFER_PROP_ALPHA_TABLE);
    }
    break;
  case FILTER_LINEAR_TRANSFER:
    switch (aIndex) {
      CONVERT_PROP(LINEAR_TRANSFER_DISABLE_R, LINEARTRANSFER_PROP_RED_DISABLE);
      CONVERT_PROP(LINEAR_TRANSFER_DISABLE_G, LINEARTRANSFER_PROP_GREEN_DISABLE);
      CONVERT_PROP(LINEAR_TRANSFER_DISABLE_B, LINEARTRANSFER_PROP_BLUE_DISABLE);
      CONVERT_PROP(LINEAR_TRANSFER_DISABLE_A, LINEARTRANSFER_PROP_ALPHA_DISABLE);
      CONVERT_PROP(LINEAR_TRANSFER_INTERCEPT_R, LINEARTRANSFER_PROP_RED_Y_INTERCEPT);
      CONVERT_PROP(LINEAR_TRANSFER_INTERCEPT_G, LINEARTRANSFER_PROP_GREEN_Y_INTERCEPT);
      CONVERT_PROP(LINEAR_TRANSFER_INTERCEPT_B, LINEARTRANSFER_PROP_BLUE_Y_INTERCEPT);
      CONVERT_PROP(LINEAR_TRANSFER_INTERCEPT_A, LINEARTRANSFER_PROP_ALPHA_Y_INTERCEPT);
      CONVERT_PROP(LINEAR_TRANSFER_SLOPE_R, LINEARTRANSFER_PROP_RED_SLOPE);
      CONVERT_PROP(LINEAR_TRANSFER_SLOPE_G, LINEARTRANSFER_PROP_GREEN_SLOPE);
      CONVERT_PROP(LINEAR_TRANSFER_SLOPE_B, LINEARTRANSFER_PROP_BLUE_SLOPE);
      CONVERT_PROP(LINEAR_TRANSFER_SLOPE_A, LINEARTRANSFER_PROP_ALPHA_SLOPE);
    }
    break;
  case FILTER_GAMMA_TRANSFER:
    switch (aIndex) {
      CONVERT_PROP(GAMMA_TRANSFER_DISABLE_R, GAMMATRANSFER_PROP_RED_DISABLE);
      CONVERT_PROP(GAMMA_TRANSFER_DISABLE_G, GAMMATRANSFER_PROP_GREEN_DISABLE);
      CONVERT_PROP(GAMMA_TRANSFER_DISABLE_B, GAMMATRANSFER_PROP_BLUE_DISABLE);
      CONVERT_PROP(GAMMA_TRANSFER_DISABLE_A, GAMMATRANSFER_PROP_ALPHA_DISABLE);
      CONVERT_PROP(GAMMA_TRANSFER_AMPLITUDE_R, GAMMATRANSFER_PROP_RED_AMPLITUDE);
      CONVERT_PROP(GAMMA_TRANSFER_AMPLITUDE_G, GAMMATRANSFER_PROP_GREEN_AMPLITUDE);
      CONVERT_PROP(GAMMA_TRANSFER_AMPLITUDE_B, GAMMATRANSFER_PROP_BLUE_AMPLITUDE);
      CONVERT_PROP(GAMMA_TRANSFER_AMPLITUDE_A, GAMMATRANSFER_PROP_ALPHA_AMPLITUDE);
      CONVERT_PROP(GAMMA_TRANSFER_EXPONENT_R, GAMMATRANSFER_PROP_RED_EXPONENT);
      CONVERT_PROP(GAMMA_TRANSFER_EXPONENT_G, GAMMATRANSFER_PROP_GREEN_EXPONENT);
      CONVERT_PROP(GAMMA_TRANSFER_EXPONENT_B, GAMMATRANSFER_PROP_BLUE_EXPONENT);
      CONVERT_PROP(GAMMA_TRANSFER_EXPONENT_A, GAMMATRANSFER_PROP_ALPHA_EXPONENT);
      CONVERT_PROP(GAMMA_TRANSFER_OFFSET_R, GAMMATRANSFER_PROP_RED_OFFSET);
      CONVERT_PROP(GAMMA_TRANSFER_OFFSET_G, GAMMATRANSFER_PROP_GREEN_OFFSET);
      CONVERT_PROP(GAMMA_TRANSFER_OFFSET_B, GAMMATRANSFER_PROP_BLUE_OFFSET);
      CONVERT_PROP(GAMMA_TRANSFER_OFFSET_A, GAMMATRANSFER_PROP_ALPHA_OFFSET);
    }
    break;
  case FILTER_CONVOLVE_MATRIX:
    switch (aIndex) {
      CONVERT_PROP(CONVOLVE_MATRIX_BIAS, CONVOLVEMATRIX_PROP_BIAS);
      CONVERT_PROP(CONVOLVE_MATRIX_KERNEL_MATRIX, CONVOLVEMATRIX_PROP_KERNEL_MATRIX);
      CONVERT_PROP(CONVOLVE_MATRIX_DIVISOR, CONVOLVEMATRIX_PROP_DIVISOR);
      CONVERT_PROP(CONVOLVE_MATRIX_KERNEL_UNIT_LENGTH, CONVOLVEMATRIX_PROP_KERNEL_UNIT_LENGTH);
      CONVERT_PROP(CONVOLVE_MATRIX_PRESERVE_ALPHA, CONVOLVEMATRIX_PROP_PRESERVE_ALPHA);
    }
  case FILTER_DISPLACEMENT_MAP:
    switch (aIndex) {
      CONVERT_PROP(DISPLACEMENT_MAP_SCALE, DISPLACEMENTMAP_PROP_SCALE);
      CONVERT_PROP(DISPLACEMENT_MAP_X_CHANNEL, DISPLACEMENTMAP_PROP_X_CHANNEL_SELECT);
      CONVERT_PROP(DISPLACEMENT_MAP_Y_CHANNEL, DISPLACEMENTMAP_PROP_Y_CHANNEL_SELECT);
    }
  }

  return UINT32_MAX;
}

bool
GetD2D1PropsForIntSize(uint32_t aType, uint32_t aIndex, UINT32 *aPropWidth, UINT32 *aPropHeight)
{
  switch (aType) {
  case FILTER_MORPHOLOGY:
    if (aIndex == ATT_MORPHOLOGY_RADII) {
      *aPropWidth = D2D1_MORPHOLOGY_PROP_WIDTH;
      *aPropHeight = D2D1_MORPHOLOGY_PROP_HEIGHT;
      return true;
    }
    break;
  }
  return false;
}

void
FilterNodeD2D1::SetInput(uint32_t aIndex, SourceSurface *aSurface)
{
  UINT32 input = GetD2D1InputForInput(mType, aIndex);
  MOZ_ASSERT(input < mEffect->GetInputCount());

  if (aSurface->GetType() != SURFACE_D2D1_1_IMAGE) {
    gfxWarning() << "Unknown input SourceSurface set on effect.";
    MOZ_ASSERT(0);
    return;
  }

  static_cast<SourceSurfaceD2D1*>(aSurface)->EnsureIndependent();
  mEffect->SetInput(input, static_cast<SourceSurfaceD2D1*>(aSurface)->GetImage());
}

void
FilterNodeD2D1::SetInput(uint32_t aIndex, FilterNode *aFilter)
{
  UINT32 input = GetD2D1InputForInput(mType, aIndex);
  MOZ_ASSERT(input < mEffect->GetInputCount());

  if (aFilter->GetBackendType() != FILTER_BACKEND_DIRECT2D1_1) {
    gfxWarning() << "Unknown input SourceSurface set on effect.";
    MOZ_ASSERT(0);
    return;
  }

  mEffect->SetInputEffect(input, static_cast<FilterNodeD2D1*>(aFilter)->mEffect.get());
}

void
FilterNodeD2D1::SetAttribute(uint32_t aIndex, uint32_t aValue)
{
  UINT32 input = GetD2D1PropForAttribute(mType, aIndex);
  MOZ_ASSERT(input < mEffect->GetPropertyCount());

  mEffect->SetValue(input, ConvertValue(mType, aIndex, aValue));
}

void
FilterNodeD2D1::SetAttribute(uint32_t aIndex, Float aValue)
{
  UINT32 input = GetD2D1PropForAttribute(mType, aIndex);
  MOZ_ASSERT(input < mEffect->GetPropertyCount());

  mEffect->SetValue(input, aValue);
}

void
FilterNodeD2D1::SetAttribute(uint32_t aIndex, const Point &aValue)
{
  UINT32 input = GetD2D1PropForAttribute(mType, aIndex);
  MOZ_ASSERT(input < mEffect->GetPropertyCount());

  mEffect->SetValue(input, D2DPoint(aValue));
}

void
FilterNodeD2D1::SetAttribute(uint32_t aIndex, const Matrix5x4 &aValue)
{
  UINT32 input = GetD2D1PropForAttribute(mType, aIndex);
  MOZ_ASSERT(input < mEffect->GetPropertyCount());

  mEffect->SetValue(input, D2DMatrix5x4(aValue));
}

void
FilterNodeD2D1::SetAttribute(uint32_t aIndex, const Point3D &aValue)
{
  UINT32 input = GetD2D1PropForAttribute(mType, aIndex);
  MOZ_ASSERT(input < mEffect->GetPropertyCount());

  mEffect->SetValue(input, D2DVector3D(aValue));
}

void
FilterNodeD2D1::SetAttribute(uint32_t aIndex, const IntSize &aValue)
{
  UINT32 widthProp, heightProp;

  if (!GetD2D1PropsForIntSize(mType, aIndex, &widthProp, &heightProp)) {
    return;
  }

  IntSize value = aValue;
  ConvertValue(mType, aIndex, value);

  mEffect->SetValue(widthProp, (UINT)value.width);
  mEffect->SetValue(heightProp, (UINT)value.height);
}

void
FilterNodeD2D1::SetAttribute(uint32_t aIndex, const Color &aValue)
{
  UINT32 input = GetD2D1PropForAttribute(mType, aIndex);
  MOZ_ASSERT(input < mEffect->GetPropertyCount());

  mEffect->SetValue(input, D2D1::Vector4F(aValue.r, aValue.g, aValue.b, aValue.a));
}

void
FilterNodeD2D1::SetAttribute(uint32_t aIndex, const Rect &aValue)
{
  UINT32 input = GetD2D1PropForAttribute(mType, aIndex);
  MOZ_ASSERT(input < mEffect->GetPropertyCount());

  mEffect->SetValue(input, D2DRect(aValue));
}

void
FilterNodeD2D1::SetAttribute(uint32_t aIndex, bool aValue)
{
  UINT32 input = GetD2D1PropForAttribute(mType, aIndex);
  MOZ_ASSERT(input < mEffect->GetPropertyCount());

  mEffect->SetValue(input, (BOOL)aValue);
}

void
FilterNodeD2D1::SetAttribute(uint32_t aIndex, const Float *aValues, uint32_t aSize)
{
  UINT32 input = GetD2D1PropForAttribute(mType, aIndex);
  MOZ_ASSERT(input < mEffect->GetPropertyCount());

  mEffect->SetValue(input, (BYTE*)aValues, sizeof(Float) * aSize);
}

void
FilterNodeD2D1::SetAttribute(uint32_t aIndex, const IntPoint &aValue)
{
  if (mType == FILTER_OFFSET) {
    MOZ_ASSERT(aIndex == ATT_OFFSET_OFFSET);

    Matrix mat;
    mat.Translate(Float(aValue.x), Float(aValue.y));
    mEffect->SetValue(D2D1_2DAFFINETRANSFORM_PROP_TRANSFORM_MATRIX, D2DMatrix(mat));
    return;
  }

  UINT32 input = GetD2D1PropForAttribute(mType, aIndex);
  MOZ_ASSERT(input < mEffect->GetPropertyCount());

  mEffect->SetValue(input, D2DPoint(aValue));
}

FilterNodeConvolveD2D1::FilterNodeConvolveD2D1(ID2D1DeviceContext *aDC)
  : FilterNodeD2D1(nullptr, FILTER_CONVOLVE_MATRIX)
  , mEdgeMode(EDGE_MODE_DUPLICATE)
{
  HRESULT hr;
  
  hr = aDC->CreateEffect(CLSID_D2D1ConvolveMatrix, byRef(mEffect));

  if (FAILED(hr)) {
    gfxWarning() << "Failed to create ConvolveMatrix filter!";
    return;
  }

  mEffect->SetValue(D2D1_CONVOLVEMATRIX_PROP_BORDER_MODE, D2D1_BORDER_MODE_SOFT);

  hr = aDC->CreateEffect(CLSID_D2D1Border, byRef(mBorderEffect));

  if (FAILED(hr)) {
    gfxWarning() << "Failed to create ConvolveMatrix filter!";
    return;
  }

  UpdateChain();
}

void
FilterNodeConvolveD2D1::SetInput(uint32_t aIndex, SourceSurface *aSurface)
{
  MOZ_ASSERT(aIndex == 0);

  if (aSurface->GetType() != SURFACE_D2D1_1_IMAGE) {
    gfxWarning() << "Unknown input SourceSurface set on effect.";
    MOZ_ASSERT(0);
    return;
  }

  mInput = static_cast<SourceSurfaceD2D1*>(aSurface)->GetImage();
  mInputEffect = nullptr;

  static_cast<SourceSurfaceD2D1*>(aSurface)->EnsureIndependent();

  UpdateChain();
}

void
FilterNodeConvolveD2D1::SetInput(uint32_t aIndex, FilterNode *aFilter)
{
  MOZ_ASSERT(aIndex == 0);

  if (aFilter->GetBackendType() != FILTER_BACKEND_DIRECT2D1_1) {
    gfxWarning() << "Unknown input SourceSurface set on effect.";
    MOZ_ASSERT(0);
    return;
  }

  mInput = nullptr;
  mInputEffect = static_cast<FilterNodeD2D1*>(aFilter)->mEffect;

  UpdateChain();
}

void
FilterNodeConvolveD2D1::SetAttribute(uint32_t aIndex, uint32_t aValue)
{
  if (aIndex != ATT_CONVOLVE_MATRIX_EDGE_MODE) {
    return FilterNodeD2D1::SetAttribute(aIndex, aValue);
  }

  mEdgeMode = (ConvolveMatrixEdgeMode)aValue;

  UpdateChain();
}

void
FilterNodeConvolveD2D1::UpdateChain()
{
  ID2D1Effect *firstEffect = mBorderEffect;
  if (mEdgeMode == EDGE_MODE_NONE) {
    firstEffect = mEffect;
  } else {
    mEffect->SetInputEffect(0, mBorderEffect.get());
  }

  if (mInputEffect) {
    firstEffect->SetInputEffect(0, mInputEffect);
  } else {
    firstEffect->SetInput(0, mInput);
  }

  if (mEdgeMode == EDGE_MODE_DUPLICATE) {
    mBorderEffect->SetValue(D2D1_BORDER_PROP_EDGE_MODE_X, D2D1_BORDER_EDGE_MODE_CLAMP);
    mBorderEffect->SetValue(D2D1_BORDER_PROP_EDGE_MODE_Y, D2D1_BORDER_EDGE_MODE_CLAMP);
  } else if (mEdgeMode == EDGE_MODE_WRAP) {
    mBorderEffect->SetValue(D2D1_BORDER_PROP_EDGE_MODE_X, D2D1_BORDER_EDGE_MODE_WRAP);
    mBorderEffect->SetValue(D2D1_BORDER_PROP_EDGE_MODE_Y, D2D1_BORDER_EDGE_MODE_WRAP);
  }
}

void
FilterNodeConvolveD2D1::SetAttribute(uint32_t aIndex, const IntSize &aValue)
{
  if (aIndex != ATT_CONVOLVE_MATRIX_KERNEL_SIZE) {
    MOZ_ASSERT(false);
    return;
  }

  mKernelSize = aValue;

  mEffect->SetValue(D2D1_CONVOLVEMATRIX_PROP_KERNEL_SIZE_X, aValue.width);
  mEffect->SetValue(D2D1_CONVOLVEMATRIX_PROP_KERNEL_SIZE_X, aValue.height);

  UpdateOffset();
}

void
FilterNodeConvolveD2D1::SetAttribute(uint32_t aIndex, const IntPoint &aValue)
{
  if (aIndex != ATT_CONVOLVE_MATRIX_TARGET) {
    MOZ_ASSERT(false);
    return;
  }

  mTarget = aValue;

  UpdateOffset();
}

void
FilterNodeConvolveD2D1::UpdateOffset()
{
  D2D1_VECTOR_2F vector =
    D2D1::Vector2F((Float(mKernelSize.width) - 1.0f) / 2.0f - Float(mTarget.x),
                   (Float(mKernelSize.height) - 1.0f) / 2.0f - Float(mTarget.y));

  mEffect->SetValue(D2D1_CONVOLVEMATRIX_PROP_KERNEL_OFFSET, vector);
}

}
}
