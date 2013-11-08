/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_GFX_FILTERPROCESSING_H_
#define _MOZILLA_GFX_FILTERPROCESSING_H_

#include "2D.h"
#include "Filters.h"

namespace mozilla {
namespace gfx {

const ptrdiff_t B8G8R8A8_COMPONENT_BYTEOFFSET_B = 0;
const ptrdiff_t B8G8R8A8_COMPONENT_BYTEOFFSET_G = 1;
const ptrdiff_t B8G8R8A8_COMPONENT_BYTEOFFSET_R = 2;
const ptrdiff_t B8G8R8A8_COMPONENT_BYTEOFFSET_A = 3;

class FilterProcessing
{
public:
  static TemporaryRef<DataSourceSurface> ExtractAlpha(DataSourceSurface* aSource);
  static TemporaryRef<DataSourceSurface> ConvertToB8G8R8A8(SourceSurface* aSurface);
  static TemporaryRef<DataSourceSurface> ApplyBlending(DataSourceSurface* aInput1, DataSourceSurface* aInput2, BlendMode aBlendMode);
  static void ApplyMorphologyHorizontal(uint8_t* aSourceData, int32_t aSourceStride,
                                          uint8_t* aDestData, int32_t aDestStride,
                                          const IntRect& aDestRect, int32_t aRadius,
                                          MorphologyOperator aOperator);
  static void ApplyMorphologyVertical(uint8_t* aSourceData, int32_t aSourceStride,
                                          uint8_t* aDestData, int32_t aDestStride,
                                          const IntRect& aDestRect, int32_t aRadius,
                                          MorphologyOperator aOperator);
  static TemporaryRef<DataSourceSurface> ApplyColorMatrix(DataSourceSurface* aInput, const Matrix5x4 &aMatrix);
  static void ApplyComposition(DataSourceSurface* aSource, DataSourceSurface* aDest, CompositeOperator aOperator);
  static void SeparateColorChannels(DataSourceSurface* aSource,
                                    RefPtr<DataSourceSurface>& aChannel0,
                                    RefPtr<DataSourceSurface>& aChannel1,
                                    RefPtr<DataSourceSurface>& aChannel2,
                                    RefPtr<DataSourceSurface>& aChannel3);
  static TemporaryRef<DataSourceSurface>
    CombineColorChannels(DataSourceSurface* aChannel0, DataSourceSurface* aChannel1,
                         DataSourceSurface* aChannel2, DataSourceSurface* aChannel3);
  static void DoPremultiplicationCalculation(const IntSize& aSize,
                                        uint8_t* aTargetData, int32_t aTargetStride,
                                        uint8_t* aSourceData, int32_t aSourceStride);
  static void DoUnpremultiplicationCalculation(const IntSize& aSize,
                                               uint8_t* aTargetData, int32_t aTargetStride,
                                               uint8_t* aSourceData, int32_t aSourceStride);

protected:
  static void ExtractAlpha_Scalar(const IntSize& size, uint8_t* sourceData, int32_t sourceStride, uint8_t* alphaData, int32_t alphaStride);
  static TemporaryRef<DataSourceSurface> ConvertToB8G8R8A8_Scalar(SourceSurface* aSurface);
  static TemporaryRef<DataSourceSurface> ApplyBlending_Scalar(DataSourceSurface* aInput1, DataSourceSurface* aInput2, BlendMode aBlendMode);
  static void ApplyMorphologyHorizontal_Scalar(uint8_t* aSourceData, int32_t aSourceStride,
                                               uint8_t* aDestData, int32_t aDestStride,
                                               const IntRect& aDestRect, int32_t aRadius,
                                               MorphologyOperator aOperator);
  static void ApplyMorphologyVertical_Scalar(uint8_t* aSourceData, int32_t aSourceStride,
                                               uint8_t* aDestData, int32_t aDestStride,
                                               const IntRect& aDestRect, int32_t aRadius,
                                               MorphologyOperator aOperator);
  static TemporaryRef<DataSourceSurface> ApplyColorMatrix_Scalar(DataSourceSurface* aInput, const Matrix5x4 &aMatrix);
  static void ApplyComposition_Scalar(DataSourceSurface* aSource, DataSourceSurface* aDest, CompositeOperator aOperator);

  static void SeparateColorChannels_Scalar(const IntSize &size, uint8_t* sourceData, int32_t sourceStride, uint8_t* channel0Data, uint8_t* channel1Data, uint8_t* channel2Data, uint8_t* channel3Data, int32_t channelStride);
  static void CombineColorChannels_Scalar(const IntSize &size, int32_t resultStride, uint8_t* resultData, int32_t channelStride, uint8_t* channel0Data, uint8_t* channel1Data, uint8_t* channel2Data, uint8_t* channel3Data);
  static void DoPremultiplicationCalculation_Scalar(const IntSize& aSize,
                                        uint8_t* aTargetData, int32_t aTargetStride,
                                        uint8_t* aSourceData, int32_t aSourceStride);
  static void DoUnpremultiplicationCalculation_Scalar(const IntSize& aSize,
                                               uint8_t* aTargetData, int32_t aTargetStride,
                                               uint8_t* aSourceData, int32_t aSourceStride);

#ifdef USE_SSE2
  static void ExtractAlpha_SSE2(const IntSize& size, uint8_t* sourceData, int32_t sourceStride, uint8_t* alphaData, int32_t alphaStride);
  static TemporaryRef<DataSourceSurface> ConvertToB8G8R8A8_SSE2(SourceSurface* aSurface);
  static TemporaryRef<DataSourceSurface> ApplyBlending_SSE2(DataSourceSurface* aInput1, DataSourceSurface* aInput2, BlendMode aBlendMode);
  static void ApplyMorphologyHorizontal_SSE2(uint8_t* aSourceData, int32_t aSourceStride,
                                             uint8_t* aDestData, int32_t aDestStride,
                                             const IntRect& aDestRect, int32_t aRadius,
                                             MorphologyOperator aOperator);
  static void ApplyMorphologyVertical_SSE2(uint8_t* aSourceData, int32_t aSourceStride,
                                             uint8_t* aDestData, int32_t aDestStride,
                                             const IntRect& aDestRect, int32_t aRadius,
                                             MorphologyOperator aOperator);
  static TemporaryRef<DataSourceSurface> ApplyColorMatrix_SSE2(DataSourceSurface* aInput, const Matrix5x4 &aMatrix);
  static void ApplyComposition_SSE2(DataSourceSurface* aSource, DataSourceSurface* aDest, CompositeOperator aOperator);
  static void SeparateColorChannels_SSE2(const IntSize &size, uint8_t* sourceData, int32_t sourceStride, uint8_t* channel0Data, uint8_t* channel1Data, uint8_t* channel2Data, uint8_t* channel3Data, int32_t channelStride);
  static void CombineColorChannels_SSE2(const IntSize &size, int32_t resultStride, uint8_t* resultData, int32_t channelStride, uint8_t* channel0Data, uint8_t* channel1Data, uint8_t* channel2Data, uint8_t* channel3Data);
  static void DoPremultiplicationCalculation_SSE2(const IntSize& aSize,
                                        uint8_t* aTargetData, int32_t aTargetStride,
                                        uint8_t* aSourceData, int32_t aSourceStride);
  static void DoUnpremultiplicationCalculation_SSE2(const IntSize& aSize,
                                               uint8_t* aTargetData, int32_t aTargetStride,
                                               uint8_t* aSourceData, int32_t aSourceStride);
#endif
};

} // namespace gfx
} // namespace mozilla

#endif // _MOZILLA_GFX_FILTERPROCESSING_H_
