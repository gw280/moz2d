/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_FILTERNODED2D1_H_
#define MOZILLA_GFX_FILTERNODED2D1_H_

#include "Filters.h"
#include <d2d1_1.h>
#include <cguid.h>

namespace mozilla {
namespace gfx {

static inline REFCLSID GetCLDIDForFilterType(FilterType aType)
{
  switch (aType) {
  case FILTER_COLOR_MATRIX:
    return CLSID_D2D1ColorMatrix;
  case FILTER_BLEND:
    return CLSID_D2D1Blend;
  case FILTER_MORPHOLOGY:
    return CLSID_D2D1Morphology;
  case FILTER_FLOOD:
    return CLSID_D2D1Flood;
  case FILTER_TILE:
    return CLSID_D2D1Tile;
  case FILTER_TABLE_TRANSFER:
    return CLSID_D2D1TableTransfer;
  case FILTER_LINEAR_TRANSFER:
    return CLSID_D2D1LinearTransfer;
  case FILTER_DISCRETE_TRANSFER:
    return CLSID_D2D1DiscreteTransfer;
  case FILTER_GAMMA_TRANSFER:
    return CLSID_D2D1GammaTransfer;
  case FILTER_OFFSET:
    return CLSID_D2D12DAffineTransform;
  case FILTER_DISPLACEMENT_MAP:
    return CLSID_D2D1DisplacementMap;
  }
  return GUID_NULL;
}

class FilterNodeD2D1 : public FilterNode
{
public:
  FilterNodeD2D1(ID2D1Effect *aEffect, FilterType aType)
    : mEffect(aEffect)
    , mType(aType)
  {}

  virtual FilterBackend GetBackendType() { return FILTER_BACKEND_DIRECT2D1_1; }

  virtual void SetInput(uint32_t aIndex, SourceSurface *aSurface);
  virtual void SetInput(uint32_t aIndex, FilterNode *aFilter);

  virtual void SetAttribute(uint32_t aIndex, uint32_t aValue);
  virtual void SetAttribute(uint32_t aIndex, Float aValue);
  virtual void SetAttribute(uint32_t aIndex, const Point &aValue);
  virtual void SetAttribute(uint32_t aIndex, const Matrix5x4 &aValue);
  virtual void SetAttribute(uint32_t aIndex, const Point3D &aValue);
  virtual void SetAttribute(uint32_t aIndex, const IntSize &aValue);
  virtual void SetAttribute(uint32_t aIndex, const Color &aValue);
  virtual void SetAttribute(uint32_t aIndex, const Rect &aValue);
  virtual void SetAttribute(uint32_t aIndex, bool aValue);
  virtual void SetAttribute(uint32_t aIndex, const Float *aValues, uint32_t aSize);
  virtual void SetAttribute(uint32_t aIndex, const IntPoint &aValue);

protected:
  friend class DrawTargetD2D1;
  friend class FilterNodeConvolveD2D1;

  RefPtr<ID2D1Effect> mEffect;
  FilterType mType;
};

class FilterNodeConvolveD2D1 : public FilterNodeD2D1
{
public:
  FilterNodeConvolveD2D1(ID2D1DeviceContext *aDC);

  virtual void SetInput(uint32_t aIndex, SourceSurface *aSurface);
  virtual void SetInput(uint32_t aIndex, FilterNode *aFilter);

  virtual void SetAttribute(uint32_t aIndex, uint32_t aValue);
  virtual void SetAttribute(uint32_t aIndex, const IntSize &aValue);
  virtual void SetAttribute(uint32_t aIndex, const IntPoint &aValue);

private:
  void UpdateChain();
  void UpdateOffset();

  RefPtr<ID2D1Effect> mBorderEffect;
  RefPtr<ID2D1Image> mInput;
  RefPtr<ID2D1Effect> mInputEffect;
  ConvolveMatrixEdgeMode mEdgeMode;
  IntPoint mTarget;
  IntSize mKernelSize;
};

}
}

#endif
