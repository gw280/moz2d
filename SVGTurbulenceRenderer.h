/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_GFX_SVG_TURBULENCE_RENDERER_H_
#define _MOZILLA_GFX_SVG_TURBULENCE_RENDERER_H_

#include "2D.h"

namespace mozilla {
namespace gfx {

template<typename T>
struct vec2
{
  vec2() : _1(0), _2(0) {}
  vec2(T a) : _1(a), _2(a) {}
  vec2(T x, T y) : _1(x), _2(y) {}
  vec2(const vec2 &aOther) : _1(aOther._1), _2(aOther._2) {}

  template<typename S>
  vec2(const vec2<S> &aOther)
   : _1(T(aOther._1)), _2(T(aOther._2)) {}

  vec2& operator=(const vec2 &aOther)
  {
    if (&aOther != this) {
      _1 = aOther._1;
      _2 = aOther._2;
    }
    return *this;
  }

  template<typename S>
  vec2& operator=(const vec2<S> &aOther)
  { *this = vec2<T>(aOther); return *this; }

  vec2 operator*(T aFactor) const
  { return vec2(_1 * aFactor, _2 * aFactor); }
  vec2 operator+(const vec2<T> &aOther) const
  { return vec2(_1 + aOther._1, _2 + aOther._2); }
  vec2 operator/(T aFactor) const
  { return vec2(_1 / aFactor, _2 / aFactor); }
  vec2 operator-(const vec2<T> &aOther) const
  { return vec2(_1 - aOther._1, _2 - aOther._2); }
  vec2& operator*=(T aFactor)
  { *this = *this * aFactor; return *this; }
  vec2& operator+=(const vec2<T> &aOther)
  { *this = *this + aOther; return *this; }
  vec2& operator/=(T aFactor)
  { *this = *this / aFactor; return *this; }
  vec2& operator-=(const vec2<T> &aOther)
  { *this = *this - aOther; return *this; }

  T& x() { return _1; }
  T& y() { return _2; }
  const T& x() const { return _1; }
  const T& y() const { return _2; }

  T& u() { return _1; }
  T& v() { return _2; }
  const T& u() const { return _1; }
  const T& v() const { return _2; }

  T _1;
  T _2;
};

template<typename T>
struct vec4
{
  vec4() : _1(0), _2(0), _3(0), _4(0) {}
  vec4(T a) : _1(a), _2(a), _3(a), _4(a) {}
  vec4(T a1, T a2, T a3, T a4) : _1(a1), _2(a2), _3(a3), _4(a4) {}
  vec4(const vec4 &aOther)
   : _1(aOther._1), _2(aOther._2), _3(aOther._3), _4(aOther._4) {}

  // Allow implicit conversion from vec4<T> to vec4<int32_t>, for example.
  template<typename S>
  vec4(const vec4<S> &aOther)
   : _1(T(aOther._1)), _2(T(aOther._2)), _3(T(aOther._3)), _4(T(aOther._4)) {}

  vec4& operator=(const vec4 &aOther)
  {
    if (&aOther != this) {
      _1 = aOther._1;
      _2 = aOther._2;
      _3 = aOther._3;
      _4 = aOther._4;
    }
    return *this;
  }

  template<typename S>
  vec4& operator=(const vec4<S> &aOther)
  { *this = vec4<T>(aOther); return *this; }

  vec4 operator*(T aFactor) const
  { return vec4(_1 * aFactor, _2 * aFactor, _3 * aFactor, _4 * aFactor); }
  vec4 operator+(const vec4<T> &aOther) const
  { return vec4(_1 + aOther._1, _2 + aOther._2, _3 + aOther._3, _4 + aOther._4); }
  vec4 operator/(T aFactor) const
  { return vec4(_1 / aFactor, _2 / aFactor, _3 / aFactor, _4 / aFactor); }
  vec4 operator-(const vec4<T> &aOther) const
  { return vec4(_1 - aOther._1, _2 - aOther._2, _3 - aOther._3, _4 - aOther._4); }

  vec4& operator*=(T aFactor) { *this = *this * aFactor; return *this; }
  vec4& operator+=(const vec4<T> &aOther) { *this = *this + aOther; return *this; }
  vec4& operator/=(T aFactor) { *this = *this / aFactor; return *this; }
  vec4& operator-=(const vec4<T> &aOther) { *this = *this - aOther; return *this; }

  T& r() { return _1; }
  T& g() { return _2; }
  T& b() { return _3; }
  T& a() { return _4; }
  const T& r() const { return _1; }
  const T& g() const { return _2; }
  const T& b() const { return _3; }
  const T& a() const { return _4; }

  T _1;
  T _2;
  T _3;
  T _4;
};

template<TurbulenceType Type, bool Stitch, typename T>
class SVGTurbulenceRenderer
{
public:
  SVGTurbulenceRenderer(const Size &aBaseFrequency, int32_t aSeed,
                        int aNumOctaves, const IntRect &aTileRect);

  uint32_t ColorAtPoint(const IntPoint &aPoint) const;

private:
  /* The turbulence calculation code is an adapted version of what
     appears in the SVG 1.1 specification:
         http://www.w3.org/TR/SVG11/filters.html#feTurbulence
  */

  const static int sBSize = 0x100;
  const static int sPerlinN = 0x1000;

  struct StitchInfo {
    int mWidth;     // How much to subtract to wrap for stitching.
    int mHeight;
    int mWrapX;     // Minimum value to wrap.
    int mWrapY;
  };

  void InitFromSeed(int32_t aSeed);
  void AdjustBaseFrequencyForStitch(const IntRect &aTileRect);
  StitchInfo CreateStitchInfo(const IntRect &aTileRect);
  vec4<T> Interpolate(vec2<uint8_t> b0, vec2<uint8_t> b1,
                     vec2<T> r0, vec2<T> r1) const;
  vec4<T> Noise2(int aColorChannel, vec2<T> aVec, const StitchInfo& aStitchInfo) const;
  vec4<T> Turbulence(int aColorChannel, const IntPoint &aPoint) const;

  Size mBaseFrequency;
  int32_t mNumOctaves;
  StitchInfo mStitchInfo;
  bool mStitchable;
  TurbulenceType mType;
  int32_t mLatticeSelector[sBSize];
  vec2<vec4<T> > mGradient[sBSize];
};

} // namespace gfx
} // namespace mozilla

#endif // _MOZILLA_GFX_SVG_TURBULENCE_RENDERER_H_
