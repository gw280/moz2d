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
#include <vector>

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
  FILTER_FLOOD,
  FILTER_TILE,
  FILTER_TABLE_TRANSFER,
  FILTER_DISCRETE_TRANSFER,
  FILTER_LINEAR_TRANSFER,
  FILTER_GAMMA_TRANSFER,
  FILTER_CONVOLVE_MATRIX,
  FILTER_OFFSET,
  FILTER_DISPLACEMENT_MAP,
  FILTER_TURBULENCE,
  FILTER_ARITHMETIC_COMBINE,
  FILTER_COMPOSITE,
  FILTER_GAUSSIAN_BLUR,
  FILTER_POINT_DIFFUSE,
  FILTER_POINT_SPECULAR,
  FILTER_SPOT_DIFFUSE,
  FILTER_SPOT_SPECULAR,
  FILTER_DISTANT_DIFFUSE,
  FILTER_DISTANT_SPECULAR,
  FILTER_CROP,
  FILTER_PREMULTIPLY,
  FILTER_UNPREMULTIPLY
};

enum BlendFilterAtts
{
  ATT_BLEND_BLENDMODE = 0                   // uint32_t
};

enum BlendMode
{
  BLEND_MODE_MULTIPLY = 0,
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
  ATT_MORPHOLOGY_RADII = 0,                 // IntSize
  ATT_MORPHOLOGY_OPERATOR                   // MorphologyOperator
};

enum MorphologyOperator
{
  MORPHOLOGY_OPERATOR_ERODE = 0,
  MORPHOLOGY_OPERATOR_DILATE
};

enum MorphologyFilterInputs
{
  IN_MORPHOLOGY_IN = 0
};

enum ColorMatrixFilterAtts
{
  ATT_COLOR_MATRIX_MATRIX = 0               // Matrix5x4
};

enum ColorMatrixFilterInputs
{
  IN_COLOR_MATRIX_IN = 0
};

enum FloodFilterAtts
{
  ATT_FLOOD_COLOR = 0                       // Color
};

enum FloodFilterInputs
{
  IN_FLOOD_IN = 0
};

enum TileFilterAtts
{
  ATT_TILE_SOURCE_RECT = 0                  // IntRect
};

enum TileFilterInputs
{
  IN_TILE_IN = 0
};

enum TableTransferAtts
{
  ATT_TABLE_TRANSFER_TABLE_R = 0,           // Float[]
  ATT_TABLE_TRANSFER_DISABLE_R,             // bool
  ATT_TABLE_TRANSFER_TABLE_G,               // Float[]
  ATT_TABLE_TRANSFER_DISABLE_G,             // bool
  ATT_TABLE_TRANSFER_TABLE_B,               // Float[]
  ATT_TABLE_TRANSFER_DISABLE_B,             // bool
  ATT_TABLE_TRANSFER_TABLE_A,               // Float[]
  ATT_TABLE_TRANSFER_DISABLE_A              // bool
};

enum TableTransferInputs
{
  IN_TABLE_TRANSFER_IN = 0
};

enum DiscreteTransferAtts
{
  ATT_DISCRETE_TRANSFER_TABLE_R = 0,        // Float[]
  ATT_DISCRETE_TRANSFER_DISABLE_R,          // bool
  ATT_DISCRETE_TRANSFER_TABLE_G,            // Float[]
  ATT_DISCRETE_TRANSFER_DISABLE_G,          // bool
  ATT_DISCRETE_TRANSFER_TABLE_B,            // Float[]
  ATT_DISCRETE_TRANSFER_DISABLE_B,          // bool
  ATT_DISCRETE_TRANSFER_TABLE_A,            // Float[]
  ATT_DISCRETE_TRANSFER_DISABLE_A           // bool
};

enum DiscreteTransferInputs
{
  IN_DISCRETE_TRANSFER_IN = 0
};

enum LinearTransferAtts
{
  ATT_LINEAR_TRANSFER_SLOPE_R = 0,          // Float
  ATT_LINEAR_TRANSFER_INTERCEPT_R,          // Float
  ATT_LINEAR_TRANSFER_DISABLE_R,            // bool
  ATT_LINEAR_TRANSFER_SLOPE_G,              // Float
  ATT_LINEAR_TRANSFER_INTERCEPT_G,          // Float
  ATT_LINEAR_TRANSFER_DISABLE_G,            // bool
  ATT_LINEAR_TRANSFER_SLOPE_B,              // Float
  ATT_LINEAR_TRANSFER_INTERCEPT_B,          // Float
  ATT_LINEAR_TRANSFER_DISABLE_B,            // bool
  ATT_LINEAR_TRANSFER_SLOPE_A,              // Float
  ATT_LINEAR_TRANSFER_INTERCEPT_A,          // Float
  ATT_LINEAR_TRANSFER_DISABLE_A             // bool
};

enum LinearTransferInputs
{
  IN_LINEAR_TRANSFER_IN = 0
};

enum GammaTransferAtts
{
  ATT_GAMMA_TRANSFER_AMPLITUDE_R = 0,         // Float
  ATT_GAMMA_TRANSFER_EXPONENT_R,              // Float
  ATT_GAMMA_TRANSFER_OFFSET_R,                // Float
  ATT_GAMMA_TRANSFER_DISABLE_R,               // bool
  ATT_GAMMA_TRANSFER_AMPLITUDE_G,             // Float
  ATT_GAMMA_TRANSFER_EXPONENT_G,              // Float
  ATT_GAMMA_TRANSFER_OFFSET_G,                // Float
  ATT_GAMMA_TRANSFER_DISABLE_G,               // bool
  ATT_GAMMA_TRANSFER_AMPLITUDE_B,             // Float
  ATT_GAMMA_TRANSFER_EXPONENT_B,              // Float
  ATT_GAMMA_TRANSFER_OFFSET_B,                // Float
  ATT_GAMMA_TRANSFER_DISABLE_B,               // bool
  ATT_GAMMA_TRANSFER_AMPLITUDE_A,             // Float
  ATT_GAMMA_TRANSFER_EXPONENT_A,              // Float
  ATT_GAMMA_TRANSFER_OFFSET_A,                // Float
  ATT_GAMMA_TRANSFER_DISABLE_A                // bool
};

enum GammaTransferInputs
{
  IN_GAMMA_TRANSFER_IN = 0
};

enum ConvolveMatrixAtts
{
  ATT_CONVOLVE_MATRIX_KERNEL_SIZE = 0,      // IntSize
  ATT_CONVOLVE_MATRIX_KERNEL_MATRIX,        // Float[]
  ATT_CONVOLVE_MATRIX_DIVISOR,              // Float
  ATT_CONVOLVE_MATRIX_BIAS,                 // Float
  ATT_CONVOLVE_MATRIX_TARGET,               // IntPoint
  ATT_CONVOLVE_MATRIX_EDGE_MODE,            // ConvolveMatrixEdgeMode
  ATT_CONVOLVE_MATRIX_KERNEL_UNIT_LENGTH,   // Float
  ATT_CONVOLVE_MATRIX_PRESERVE_ALPHA,       // bool
};

enum ConvolveMatrixEdgeMode
{
  EDGE_MODE_DUPLICATE = 0,
  EDGE_MODE_WRAP,
  EDGE_MODE_NONE
};

enum ConvolveMatrixInputs
{
  IN_CONVOLVE_MATRIX_IN = 0
};

enum OffsetAtts
{
  ATT_OFFSET_OFFSET = 0                     // IntPoint
};

enum OffsetInputs
{
  IN_OFFSET_IN = 0
};

enum DisplacementMapAtts
{
  ATT_DISPLACEMENT_MAP_SCALE = 0,           // Float
  ATT_DISPLACEMENT_MAP_X_CHANNEL,           // ColorChannel
  ATT_DISPLACEMENT_MAP_Y_CHANNEL            // ColorChannel
};

enum ColorChannel
{
  COLOR_CHANNEL_R = 0,
  COLOR_CHANNEL_G,
  COLOR_CHANNEL_B,
  COLOR_CHANNEL_A
};

enum DisplacementMapInputs
{
  IN_DISPLACEMENT_MAP_IN = 0,
  IN_DISPLACEMENT_MAP_IN2
};

enum TurbulenceAtts
{
  ATT_TURBULENCE_BASE_FREQUENCY = 0,        // Float
  ATT_TURBULENCE_NUM_OCTAVES,               // uint32_t
  ATT_TURBULENCE_SEED,                      // uint32_t
  ATT_TURBULENCE_STITCHABLE,                // bool
  ATT_TURBULENCE_TYPE                       // TurbulenceType
};

enum TurbulenceType
{
  TURBULENCE_TYPE_TURBULENCE = 0,
  TURBULENCE_TYPE_FRACTAL_NOISE
};

enum ArithmeticCombineAtts
{
  ATT_ARITHMETIC_COMBINE_K1 = 0,            // Float
  ATT_ARITHMETIC_COMBINE_K2,                // Float
  ATT_ARITHMETIC_COMBINE_K3,                // Float
  ATT_ARITHMETIC_COMBINE_K4                 // Float
};

enum ArithmeticCombineInputs
{
  IN_ARITHMETIC_COMBINE_IN = 0,
  IN_ARITHMETIC_COMBINE_IN2
};

enum CompositeAtts
{
  ATT_COMPOSITE_OPERATOR = 0                // CompositeOperator
};

enum CompositeOperator
{
  COMPOSITE_OPERATOR_OVER = 0,
  COMPOSITE_OPERATOR_IN,
  COMPOSITE_OPERATOR_OUT,
  COMPOSITE_OPERATOR_ATOP,
  COMPOSITE_OPERATOR_XOR
};

enum CompositeInputs
{
  // arbitrary number of inputs
  IN_COMPOSITE_IN_START = 0
};

enum GaussianBlurAtts
{
  ATT_GAUSSIAN_BLUR_STD_DEVIATION = 0       // Float
};

enum GaussianBlurInputs
{
  IN_GAUSSIAN_BLUR_IN = 0
};

enum PointDiffuseAtts
{
  ATT_POINT_DIFFUSE_POSITION = 0,           // Point3D
  ATT_POINT_DIFFUSE_SURFACE_SCALE,          // Float
  ATT_POINT_DIFFUSE_DIFFUSE_CONSTANT,       // Float
  ATT_POINT_DIFFUSE_KERNEL_UNIT_LENGTH      // Float
};

enum PointDiffuseInputs
{
  IN_POINT_DIFFUSE_IN = 0
};

enum SpotDiffuseAtts
{
  ATT_SPOT_DIFFUSE_POSITION = 0,            // Point3D
  ATT_SPOT_DIFFUSE_POINTS_AT,               // Point3D
  ATT_SPOT_DIFFUSE_SPECULAR_FOCUS,          // Float
  ATT_SPOT_DIFFUSE_LIMITING_CONE_ANGLE,     // Float
  ATT_SPOT_DIFFUSE_SURFACE_SCALE,           // Float
  ATT_SPOT_DIFFUSE_DIFFUSE_CONSTANT,        // Float
  ATT_SPOT_DIFFUSE_KERNEL_UNIT_LENGTH       // Float
};

enum SpotDiffuseInputs
{
  IN_SPOT_DIFFUSE_IN = 0
};

enum DistantDiffuseAtts
{
  ATT_DISTANT_DIFFUSE_AZIMUTH = 0,          // Float
  ATT_DISTANT_DIFFUSE_ELEVATION,            // Float
  ATT_DISTANT_DIFFUSE_SURFACE_SCALE,        // Float
  ATT_DISTANT_DIFFUSE_DIFFUSE_CONSTANT,     // Float
  ATT_DISTANT_DIFFUSE_KERNEL_UNIT_LENGTH    // Float
};

enum DistantDiffuseInputs
{
  IN_DISTANT_DIFFUSE_IN = 0
};

enum PointSpecularAtts
{
  ATT_POINT_SPECULAR_POSITION = 0,          // Point3D
  ATT_POINT_SPECULAR_SURFACE_SCALE,         // Float
  ATT_POINT_SPECULAR_SPECULAR_CONSTANT,     // Float
  ATT_POINT_SPECULAR_SPECULAR_EXPONENT,     // Float
  ATT_POINT_SPECULAR_KERNEL_UNIT_LENGTH     // Float
};

enum PointSpecularInputs
{
  IN_POINT_SPECULAR_IN = 0
};

enum SpotSpecularAtts
{
  ATT_SPOT_SPECULAR_POSITION = 0,           // Point3D
  ATT_SPOT_SPECULAR_POINTS_AT,              // Point3D
  ATT_SPOT_SPECULAR_SPECULAR_FOCUS,         // Float
  ATT_SPOT_SPECULAR_LIMITING_CONE_ANGLE,    // Float
  ATT_SPOT_SPECULAR_SURFACE_SCALE,          // Float
  ATT_SPOT_SPECULAR_SPECULAR_CONSTANT,      // Float
  ATT_SPOT_SPECULAR_SPECULAR_EXPONENT,      // Float
  ATT_SPOT_SPECULAR_KERNEL_UNIT_LENGTH      // Float
};

enum SpotSpecularInputs
{
  IN_SPOT_SPECULAR_IN = 0
};

enum DistantSpecularAtts
{
  ATT_DISTANT_SPECULAR_AZIMUTH = 0,         // Float
  ATT_DISTANT_SPECULAR_ELEVATION,           // Float
  ATT_DISTANT_SPECULAR_SURFACE_SCALE,       // Float
  ATT_DISTANT_SPECULAR_SPECULAR_CONSTANT,   // Float
  ATT_DISTANT_SPECULAR_SPECULAR_EXPONENT,   // Float
  ATT_DISTANT_SPECULAR_KERNEL_UNIT_LENGTH   // Float
};

enum DistantSpecularInputs
{
  IN_DISTANT_SPECULAR_IN = 0
};

enum CropAtts
{
  ATT_CROP_RECT = 0                         // IntRect
};

enum CropInputs
{
  IN_CROP_IN = 0
};

enum PremultiplyInputs
{
  IN_PREMULTIPLY_IN = 0
};

enum UnpremultiplyInputs
{
  IN_UNPREMULTIPLY_IN = 0
};

class FilterNode : public RefCounted<FilterNode>
{
public:
  virtual ~FilterNode() {}

  virtual FilterBackend GetBackendType() = 0;

  virtual void SetInput(uint32_t aIndex, SourceSurface *aSurface) { MOZ_CRASH(); }
  virtual void SetInput(uint32_t aIndex, FilterNode *aFilter) { MOZ_CRASH(); }

  virtual void SetAttribute(uint32_t aIndex, bool) { MOZ_CRASH(); }
  virtual void SetAttribute(uint32_t aIndex, uint32_t) { MOZ_CRASH(); }
  virtual void SetAttribute(uint32_t aIndex, Float) { MOZ_CRASH(); }
  virtual void SetAttribute(uint32_t aIndex, const IntSize &) { MOZ_CRASH(); }
  virtual void SetAttribute(uint32_t aIndex, const IntPoint &) { MOZ_CRASH(); }
  virtual void SetAttribute(uint32_t aIndex, const Rect &) { MOZ_CRASH(); }
  virtual void SetAttribute(uint32_t aIndex, const IntRect &) { MOZ_CRASH(); }
  virtual void SetAttribute(uint32_t aIndex, const Point &) { MOZ_CRASH(); }
  virtual void SetAttribute(uint32_t aIndex, const Matrix5x4 &) { MOZ_CRASH(); }
  virtual void SetAttribute(uint32_t aIndex, const Point3D &) { MOZ_CRASH(); }
  virtual void SetAttribute(uint32_t aIndex, const Color &) { MOZ_CRASH(); }
  virtual void SetAttribute(uint32_t aIndex, const Float* aFloat, uint32_t aSize) { MOZ_CRASH(); }

protected:
  friend class Factory;

  FilterNode() {}
};

}
}

#endif
