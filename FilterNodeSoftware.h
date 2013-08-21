/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_GFX_FILTERNODE_CAIRO_H_
#define _MOZILLA_GFX_FILTERNODE_CAIRO_H_

#include "Filters.h"
#include <vector>

namespace mozilla {
namespace gfx {

class DataSourceSurface;
class DrawTarget;
struct DrawOptions;

class FilterNodeSoftware : public FilterNode
{
public:
  static TemporaryRef<FilterNode> Create(FilterType aType);

  void Draw(DrawTarget* aDrawTarget, const Rect &aSourceRect,
            const Point &aDestPoint, const DrawOptions &aOptions);

  virtual FilterBackend GetBackendType() MOZ_OVERRIDE { return FILTER_BACKEND_SOFTWARE; }
  virtual TemporaryRef<DataSourceSurface> Render(const IntRect& aRect) = 0;
  virtual IntRect GetOutputRectInRect(const IntRect& aInRect) = 0;
  virtual void SetInput(uint32_t aIndex, SourceSurface *aSurface) MOZ_OVERRIDE;
  virtual void SetInput(uint32_t aIndex, FilterNode *aFilter) MOZ_OVERRIDE;

protected:
  virtual void SetInput(uint32_t aIndex, SourceSurface *aSurface, FilterNodeSoftware *aFilter);
  virtual int32_t InputIndex(uint32_t aInputEnumIndex) { return -1; }


  /**
   * Format hints for GetInputDataSourceSurface. Some callers of
   * GetInputDataSourceSurface can handle both B8G8R8A8 and A8 surfaces, these
   * should pass CAN_HANDLE_A8 in order to avoid unnecessary conversions.
   * Callers that can only handle B8G8R8A8 surfaces pass NEED_COLOR_CHANNELS.
   */
  enum FormatHint {
    CAN_HANDLE_A8,
    NEED_COLOR_CHANNELS
  };

  /**
   * Returns FORMAT_B8G8R8A8 or FORMAT_A8, depending on the current surface
   * format and the format hint.
   */
  SurfaceFormat DesiredFormat(SurfaceFormat aCurrentFormat,
                              FormatHint aFormatHint);

  /**
   * Intended to be called by FilterNodeSoftware::Render implementations.
   * Returns a surface of size aRect.Size() or nullptr in error conditions. The
   * returned surface contains the output of the specified input filter or
   * input surface in aRect. If aRect extends beyond the input filter's output
   * rect (or the input surface's dimensions), the remaining area is filled
   * according to aEdgeMode: The default, EDGE_MODE_NONE, simply pads with
   * transparent black.
   * If non-null, the returned surface is guaranteed to be of FORMAT_A8 or
   * FORMAT_B8G8R8A8. If aFormatHint is NEED_COLOR_CHANNELS, the returned
   * surface is guaranteed to be of FORMAT_B8G8R8A8 always.
   */
  TemporaryRef<DataSourceSurface>
    GetInputDataSourceSurface(uint32_t aInputEnumIndex, const IntRect& aRect,
                              FormatHint aFormatHint = CAN_HANDLE_A8,
                              ConvolveMatrixEdgeMode aEdgeMode = EDGE_MODE_NONE);
  IntRect GetInputRectInRect(uint32_t aInputEnumIndex, const IntRect& aInRect);
  size_t NumberOfSetInputs();

private:
  std::vector<RefPtr<SourceSurface> > mInputSurfaces;
  std::vector<RefPtr<FilterNodeSoftware> > mInputFilters;
};

class FilterNodeBlendSoftware : public FilterNodeSoftware
{
public:
  FilterNodeBlendSoftware();
  virtual void SetAttribute(uint32_t aIndex, uint32_t aBlendMode) MOZ_OVERRIDE;
  virtual TemporaryRef<DataSourceSurface> Render(const IntRect& aRect) MOZ_OVERRIDE;
  virtual IntRect GetOutputRectInRect(const IntRect& aRect) MOZ_OVERRIDE;

protected:
  virtual int32_t InputIndex(uint32_t aInputEnumIndex) MOZ_OVERRIDE;

private:
  BlendMode mBlendMode;
};

class FilterNodeMorphologySoftware : public FilterNodeSoftware
{
public:
  FilterNodeMorphologySoftware();
  virtual void SetAttribute(uint32_t aIndex, const IntSize &aRadii) MOZ_OVERRIDE;
  virtual void SetAttribute(uint32_t aIndex, uint32_t aOperator) MOZ_OVERRIDE;
  virtual TemporaryRef<DataSourceSurface> Render(const IntRect& aRect) MOZ_OVERRIDE;
  virtual IntRect GetOutputRectInRect(const IntRect& aRect) MOZ_OVERRIDE;

protected:
  virtual int32_t InputIndex(uint32_t aInputEnumIndex) MOZ_OVERRIDE;

private:
  IntSize mRadii;
  MorphologyOperator mOperator;
};

class FilterNodeColorMatrixSoftware : public FilterNodeSoftware
{
public:
  virtual void SetAttribute(uint32_t aIndex, const Matrix5x4 &aMatrix) MOZ_OVERRIDE;
  virtual TemporaryRef<DataSourceSurface> Render(const IntRect& aRect) MOZ_OVERRIDE;
  virtual IntRect GetOutputRectInRect(const IntRect& aRect) MOZ_OVERRIDE;

protected:
  virtual int32_t InputIndex(uint32_t aInputEnumIndex) MOZ_OVERRIDE;

private:
  Matrix5x4 mMatrix;
};

class FilterNodeFloodSoftware : public FilterNodeSoftware
{
public:
  virtual void SetAttribute(uint32_t aIndex, const Color &aColor) MOZ_OVERRIDE;
  virtual TemporaryRef<DataSourceSurface> Render(const IntRect& aRect) MOZ_OVERRIDE;
  virtual IntRect GetOutputRectInRect(const IntRect& aRect) MOZ_OVERRIDE;

private:
  Color mColor;
};

class FilterNodeTileSoftware : public FilterNodeSoftware
{
public:
  virtual void SetAttribute(uint32_t aIndex, const IntRect &aSourceRect) MOZ_OVERRIDE;
  virtual TemporaryRef<DataSourceSurface> Render(const IntRect& aRect) MOZ_OVERRIDE;
  virtual IntRect GetOutputRectInRect(const IntRect& aRect) MOZ_OVERRIDE;

protected:
  virtual int32_t InputIndex(uint32_t aInputEnumIndex) MOZ_OVERRIDE;

private:
  IntRect mSourceRect;
};

/**
 * Baseclass for the four different component transfer filters.
 */
class FilterNodeComponentTransferSoftware : public FilterNodeSoftware
{
public:
  FilterNodeComponentTransferSoftware();

  virtual void SetAttribute(uint32_t aIndex, bool aDisable) MOZ_OVERRIDE;
  virtual TemporaryRef<DataSourceSurface> Render(const IntRect& aRect) MOZ_OVERRIDE;
  virtual IntRect GetOutputRectInRect(const IntRect& aRect) MOZ_OVERRIDE;

protected:
  virtual int32_t InputIndex(uint32_t aInputEnumIndex) MOZ_OVERRIDE;
  virtual void RequestFromInputsForRect(const IntRect &aRect) MOZ_OVERRIDE;
  template<ptrdiff_t ComponentOffset, uint32_t BytesPerPixel>
                            void ApplyComponentTransfer(DataSourceSurface* aInput,
                                                        DataSourceSurface* aTarget,
                                                        uint8_t aLookupTable[256],
                                                        bool aDisabled);
  virtual void MaybeGenerateLookupTable(ptrdiff_t aComponent, uint8_t aTable[256],
                                        bool aDisabled);
  virtual void GenerateLookupTable(ptrdiff_t aComponent, uint8_t aTable[256]) = 0;

  bool mDisableR;
  bool mDisableG;
  bool mDisableB;
  bool mDisableA;
};

class FilterNodeTableTransferSoftware : public FilterNodeComponentTransferSoftware
{
public:
  using FilterNodeComponentTransferSoftware::SetAttribute;
  virtual void SetAttribute(uint32_t aIndex, const Float* aFloat, uint32_t aSize) MOZ_OVERRIDE;

protected:
  virtual void GenerateLookupTable(ptrdiff_t aComponent, uint8_t aTable[256]) MOZ_OVERRIDE;

private:
  void GenerateLookupTable(std::vector<Float>& aTableValues, uint8_t aTable[256]);

  std::vector<Float> mTableR;
  std::vector<Float> mTableG;
  std::vector<Float> mTableB;
  std::vector<Float> mTableA;
};

class FilterNodeDiscreteTransferSoftware : public FilterNodeComponentTransferSoftware
{
public:
  using FilterNodeComponentTransferSoftware::SetAttribute;
  virtual void SetAttribute(uint32_t aIndex, const Float* aFloat, uint32_t aSize) MOZ_OVERRIDE;

protected:
  virtual void GenerateLookupTable(ptrdiff_t aComponent, uint8_t aTable[256]) MOZ_OVERRIDE;

private:
  void GenerateLookupTable(std::vector<Float>& aTableValues, uint8_t aTable[256]);

  std::vector<Float> mTableR;
  std::vector<Float> mTableG;
  std::vector<Float> mTableB;
  std::vector<Float> mTableA;
};

class FilterNodeLinearTransferSoftware : public FilterNodeComponentTransferSoftware
{
public:
  FilterNodeLinearTransferSoftware();
  using FilterNodeComponentTransferSoftware::SetAttribute;
  virtual void SetAttribute(uint32_t aIndex, Float aValue) MOZ_OVERRIDE;

protected:
  virtual void GenerateLookupTable(ptrdiff_t aComponent, uint8_t aTable[256]) MOZ_OVERRIDE;

private:
  void GenerateLookupTable(Float aSlope, Float aIntercept, uint8_t aTable[256]);

  Float mSlopeR;
  Float mSlopeG;
  Float mSlopeB;
  Float mSlopeA;
  Float mInterceptR;
  Float mInterceptG;
  Float mInterceptB;
  Float mInterceptA;
};

class FilterNodeGammaTransferSoftware : public FilterNodeComponentTransferSoftware
{
public:
  FilterNodeGammaTransferSoftware();
  using FilterNodeComponentTransferSoftware::SetAttribute;
  virtual void SetAttribute(uint32_t aIndex, Float aValue) MOZ_OVERRIDE;

protected:
  virtual void GenerateLookupTable(ptrdiff_t aComponent, uint8_t aTable[256]) MOZ_OVERRIDE;

private:
  void GenerateLookupTable(Float aAmplitude, Float aExponent, Float aOffset, uint8_t aTable[256]);

  Float mAmplitudeR;
  Float mAmplitudeG;
  Float mAmplitudeB;
  Float mAmplitudeA;
  Float mExponentR;
  Float mExponentG;
  Float mExponentB;
  Float mExponentA;
  Float mOffsetR;
  Float mOffsetG;
  Float mOffsetB;
  Float mOffsetA;
};

class FilterNodeConvolveMatrixSoftware : public FilterNodeSoftware
{
public:
  FilterNodeConvolveMatrixSoftware();
  virtual void SetAttribute(uint32_t aIndex, const IntSize &aKernelSize) MOZ_OVERRIDE;
  virtual void SetAttribute(uint32_t aIndex, const Float* aMatrix, uint32_t aSize) MOZ_OVERRIDE;
  virtual void SetAttribute(uint32_t aIndex, Float aValue) MOZ_OVERRIDE;
  virtual void SetAttribute(uint32_t aIndex, const Size &aKernelUnitLength) MOZ_OVERRIDE;
  virtual void SetAttribute(uint32_t aIndex, const IntPoint &aTarget) MOZ_OVERRIDE;
  virtual void SetAttribute(uint32_t aIndex, uint32_t aEdgeMode) MOZ_OVERRIDE;
  virtual void SetAttribute(uint32_t aIndex, bool aPreserveAlpha) MOZ_OVERRIDE;
  virtual TemporaryRef<DataSourceSurface> Render(const IntRect& aRect) MOZ_OVERRIDE;
  virtual IntRect GetOutputRectInRect(const IntRect& aRect) MOZ_OVERRIDE;

protected:
  virtual int32_t InputIndex(uint32_t aInputEnumIndex) MOZ_OVERRIDE;

private:
  template<typename CoordType>
  TemporaryRef<DataSourceSurface> DoRender(const IntRect& aRect,
                                           CoordType aKernelUnitLengthX,
                                           CoordType aKernelUnitLengthY);

  IntRect InflatedSourceRect(const IntRect &aDestRect);
  IntRect InflatedDestRect(const IntRect &aSourceRect);

  IntSize mKernelSize;
  std::vector<Float> mKernelMatrix;
  Float mDivisor;
  Float mBias;
  IntPoint mTarget;
  ConvolveMatrixEdgeMode mEdgeMode;
  Size mKernelUnitLength;
  bool mPreserveAlpha;
};

class FilterNodeOffsetSoftware : public FilterNodeSoftware
{
public:
  virtual void SetAttribute(uint32_t aIndex, const IntPoint &aOffset) MOZ_OVERRIDE;
  virtual TemporaryRef<DataSourceSurface> Render(const IntRect& aRect) MOZ_OVERRIDE;
  virtual IntRect GetOutputRectInRect(const IntRect& aRect) MOZ_OVERRIDE;

protected:
  virtual int32_t InputIndex(uint32_t aInputEnumIndex) MOZ_OVERRIDE;

private:
  IntPoint mOffset;
};

class FilterNodeDisplacementMapSoftware : public FilterNodeSoftware
{
public:
  FilterNodeDisplacementMapSoftware();
  virtual void SetAttribute(uint32_t aIndex, Float aScale) MOZ_OVERRIDE;
  virtual void SetAttribute(uint32_t aIndex, uint32_t aValue) MOZ_OVERRIDE;
  virtual TemporaryRef<DataSourceSurface> Render(const IntRect& aRect) MOZ_OVERRIDE;
  virtual IntRect GetOutputRectInRect(const IntRect& aRect) MOZ_OVERRIDE;

protected:
  virtual int32_t InputIndex(uint32_t aInputEnumIndex) MOZ_OVERRIDE;

private:
  IntRect InflatedSourceOrDestRect(const IntRect &aDestOrSourceRect);

  Float mScale;
  ColorChannel mChannelX;
  ColorChannel mChannelY;
};

class FilterNodeTurbulenceSoftware : public FilterNodeSoftware
{
public:
  FilterNodeTurbulenceSoftware();
  virtual void SetAttribute(uint32_t aIndex, const Size &aSize) MOZ_OVERRIDE;
  virtual void SetAttribute(uint32_t aIndex, bool aStitchable) MOZ_OVERRIDE;
  virtual void SetAttribute(uint32_t aIndex, uint32_t aValue) MOZ_OVERRIDE;
  virtual TemporaryRef<DataSourceSurface> Render(const IntRect& aRect) MOZ_OVERRIDE;
  virtual IntRect GetOutputRectInRect(const IntRect& aRect) MOZ_OVERRIDE;

protected:
  virtual int32_t InputIndex(uint32_t aInputEnumIndex) MOZ_OVERRIDE;

  template<TurbulenceType aType, bool aStitchable>
    TemporaryRef<DataSourceSurface> DoRender(const IntRect& aRect);

private:
  Size mBaseFrequency;
  uint32_t mNumOctaves;
  uint32_t mSeed;
  bool mStitchable;
  TurbulenceType mType;
};

class FilterNodeArithmeticCombineSoftware : public FilterNodeSoftware
{
public:
  FilterNodeArithmeticCombineSoftware();
  virtual void SetAttribute(uint32_t aIndex, const Float* aFloat, uint32_t aSize) MOZ_OVERRIDE;
  virtual TemporaryRef<DataSourceSurface> Render(const IntRect& aRect) MOZ_OVERRIDE;
  virtual IntRect GetOutputRectInRect(const IntRect& aRect) MOZ_OVERRIDE;

protected:
  virtual int32_t InputIndex(uint32_t aInputEnumIndex) MOZ_OVERRIDE;

private:
  Float mK1;
  Float mK2;
  Float mK3;
  Float mK4;
};

class FilterNodeCompositeSoftware : public FilterNodeSoftware
{
public:
  FilterNodeCompositeSoftware();
  virtual void SetAttribute(uint32_t aIndex, uint32_t aOperator) MOZ_OVERRIDE;
  virtual TemporaryRef<DataSourceSurface> Render(const IntRect& aRect) MOZ_OVERRIDE;
  virtual IntRect GetOutputRectInRect(const IntRect& aRect) MOZ_OVERRIDE;

protected:
  virtual int32_t InputIndex(uint32_t aInputEnumIndex) MOZ_OVERRIDE;

private:
  CompositeOperator mOperator;
};

class FilterNodeGaussianBlurSoftware : public FilterNodeSoftware
{
public:
  FilterNodeGaussianBlurSoftware();
  virtual void SetAttribute(uint32_t aIndex, Float aStdDeviation) MOZ_OVERRIDE;
  virtual TemporaryRef<DataSourceSurface> Render(const IntRect& aRect) MOZ_OVERRIDE;
  virtual IntRect GetOutputRectInRect(const IntRect& aRect) MOZ_OVERRIDE;

protected:
  virtual int32_t InputIndex(uint32_t aInputEnumIndex) MOZ_OVERRIDE;
  IntRect InflatedSourceOrDestRect(const IntRect &aDestRect);

private:
  Float mStdDeviation;
};

class FilterNodeDirectionalBlurSoftware : public FilterNodeSoftware
{
public:
  FilterNodeDirectionalBlurSoftware();
  virtual void SetAttribute(uint32_t aIndex, Float aStdDeviation) MOZ_OVERRIDE;
  virtual void SetAttribute(uint32_t aIndex, uint32_t aBlurDirection) MOZ_OVERRIDE;
  virtual TemporaryRef<DataSourceSurface> Render(const IntRect& aRect) MOZ_OVERRIDE;
  virtual IntRect GetOutputRectInRect(const IntRect& aRect) MOZ_OVERRIDE;

protected:
  virtual int32_t InputIndex(uint32_t aInputEnumIndex) MOZ_OVERRIDE;
  IntRect InflatedSourceOrDestRect(const IntRect &aDestRect);

private:
  Float mStdDeviation;
  BlurDirection mBlurDirection;
};

class FilterNodeCropSoftware : public FilterNodeSoftware
{
public:
  virtual void SetAttribute(uint32_t aIndex, const Rect &aSourceRect) MOZ_OVERRIDE;
  virtual TemporaryRef<DataSourceSurface> Render(const IntRect& aRect) MOZ_OVERRIDE;
  virtual IntRect GetOutputRectInRect(const IntRect& aRect) MOZ_OVERRIDE;

protected:
  virtual int32_t InputIndex(uint32_t aInputEnumIndex) MOZ_OVERRIDE;

private:
  IntRect mCropRect;
};

class FilterNodePremultiplySoftware : public FilterNodeSoftware
{
public:
  virtual TemporaryRef<DataSourceSurface> Render(const IntRect& aRect) MOZ_OVERRIDE;
  virtual IntRect GetOutputRectInRect(const IntRect& aRect) MOZ_OVERRIDE;

protected:
  virtual int32_t InputIndex(uint32_t aInputEnumIndex) MOZ_OVERRIDE;
};

class FilterNodeUnpremultiplySoftware : public FilterNodeSoftware
{
public:
  virtual TemporaryRef<DataSourceSurface> Render(const IntRect& aRect) MOZ_OVERRIDE;
  virtual IntRect GetOutputRectInRect(const IntRect& aRect) MOZ_OVERRIDE;

protected:
  virtual int32_t InputIndex(uint32_t aInputEnumIndex) MOZ_OVERRIDE;
};

template<typename LightType, typename LightingType>
class FilterNodeLightingSoftware : public FilterNodeSoftware
{
public:
  FilterNodeLightingSoftware();
  virtual void SetAttribute(uint32_t aIndex, Float) MOZ_OVERRIDE;
  virtual void SetAttribute(uint32_t aIndex, const Size &) MOZ_OVERRIDE;
  virtual void SetAttribute(uint32_t aIndex, const Point3D &) MOZ_OVERRIDE;
  virtual void SetAttribute(uint32_t aIndex, const Color &) MOZ_OVERRIDE;
  virtual TemporaryRef<DataSourceSurface> Render(const IntRect& aRect) MOZ_OVERRIDE;
  virtual IntRect GetOutputRectInRect(const IntRect& aRect) MOZ_OVERRIDE;

protected:
  virtual int32_t InputIndex(uint32_t aInputEnumIndex) MOZ_OVERRIDE;

private:
  template<typename CoordType>
  TemporaryRef<DataSourceSurface> DoRender(const IntRect& aRect,
                                           CoordType aKernelUnitLengthX,
                                           CoordType aKernelUnitLengthY);

  LightType mLight;
  LightingType mLighting;
  Float mSurfaceScale;
  Size mKernelUnitLength;
  Color mColor;
};

}
}

#endif // _MOZILLA_GFX_FILTERNODE_CAIRO_H_
