/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_FILTERS_H_
#define MOZILLA_GFX_FILTERS_H_

#include "Types.h"
#include "mozilla/RefPtr.h"

#include "Point.h"
#include "Matrix.h"

namespace mozilla {
namespace gfx {

class SourceSurface;
struct Point;

enum FilterBackend {
  FILTER_BACKEND_SOFTWARE = 0,
  FILTER_BACKEND_DIRECT2D1_1
};

enum FilterType {
  FILTER_BLEND = 0,
  FILTER_MORPHOLOGY,
  FILTER_COLOR_MATRIX,
  FILTER_POINT_LIGHT,
  FILTER_FLOOD,
  FILTER_IMAGE,
  FILTER_MERGE,
  FILTER_MERGE_NODE,
  FILTER_TILE,
  FILTER_COMPONENT_TRANSFER,
  FILTER_DISTANCE_LIGHT,
  FILTER_SPECULAR_LIGHTING,
  FILTER_CONVOLVE_MATRIX,
  FILTER_OFFSET,
  FILTER_SPOT_LIGHT,
  FILTER_DIFFUSE_LIGHTING,
  FILTER_DISPLACEMENT_MAP,
  FILTER_TURBULENCE,
  FILTER_COMPOSITE,
  FILTER_GAUSSIAN_BLUR
};

enum BlendFilterAtts
{
  ATT_BLEND_BLENDMODE = 0
};

enum BlendMode
{
  BLEND_MODE_NORMAL = 0,
  BLEND_MODE_MULTIPLY,
  BLEND_MODE_SCREEN,
  BLEND_MODE_DARKEN,
  BLEND_MODE_LIGHTEN
};

enum BlendFilterInputs
{
  IN_BLEND_IN = 0,
  IN_BLEND_IN2
};

enum MorphologyFilterAtts
{
  ATT_MORPHOLOGY_RADII = 0,
  ATT_MORPHOLOGY_MORPHOLOGY
};

enum MorphologyFilterInputs
{
  IN_MORPHOLOGY_IN = 0
};

enum ColorMatrixFilterAtts
{
  ATT_COLOR_MATRIX_MATRIX = 0
};

enum ColorMatrixFilterInputs
{
  IN_COLOR_MATRIX_IN = 0
};

enum PointLightFilterAtts
{
  ATT_POINT_LIGHT_POINT = 0
};

enum PointLightFilterInputs
{
  IN_POINT_LIGHT_IN = 0
};

class FilterNode : public RefCounted<FilterNode>
{
public:
  virtual FilterBackend GetBackendType() = 0;

  virtual void SetInput(uint32_t aIndex, SourceSurface *aSurface) { MOZ_CRASH(); }
  virtual void SetInput(uint32_t aIndex, FilterNode *aFilter) { MOZ_CRASH(); }

  virtual void SetAttribute(uint32_t aIndex, uint32_t) { MOZ_CRASH(); }
  virtual void SetAttribute(uint32_t aIndex, Float) { MOZ_CRASH(); }
  virtual void SetAttribute(uint32_t aIndex, const Point &) { MOZ_CRASH(); }
  virtual void SetAttribute(uint32_t aIndex, const Matrix5x4 &) { MOZ_CRASH(); }
  virtual void SetAttribute(uint32_t aIndex, const Point3D &) { MOZ_CRASH(); }

protected:
  friend class Factory;

  FilterNode() {}
};

}
}

#endif
