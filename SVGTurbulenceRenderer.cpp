/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGTurbulenceRenderer.h"

namespace mozilla {
namespace gfx {

namespace {

struct RandomNumberSource
{
  RandomNumberSource(int32_t aSeed) : mLast(SetupSeed(aSeed)) {}
  int32_t Next() { mLast = Random(mLast); return mLast; }

private:
  static const int32_t RAND_M = 2147483647; /* 2**31 - 1 */
  static const int32_t RAND_A = 16807;      /* 7**5; primitive root of m */
  static const int32_t RAND_Q = 127773;     /* m / a */
  static const int32_t RAND_R = 2836;       /* m % a */

  /* Produces results in the range [1, 2**31 - 2].
     Algorithm is: r = (a * r) mod m
     where a = 16807 and m = 2**31 - 1 = 2147483647
     See [Park & Miller], CACM vol. 31 no. 10 p. 1195, Oct. 1988
     To test: the algorithm should produce the result 1043618065
     as the 10,000th generated number if the original seed is 1.
  */

  static int32_t
  SetupSeed(int32_t aSeed) {
    if (aSeed <= 0)
      aSeed = -(aSeed % (RAND_M - 1)) + 1;
    if (aSeed > RAND_M - 1)
      aSeed = RAND_M - 1;
    return aSeed;
  }

  static int32_t
  Random(int32_t aSeed)
  {
    int32_t result = RAND_A * (aSeed % RAND_Q) - RAND_R * (aSeed / RAND_Q);
    if (result <= 0)
      result += RAND_M;
    return result;
  }

  int32_t mLast;
};

} // unnamed namespace

template<TurbulenceType Type, bool Stitch, typename T>
SVGTurbulenceRenderer<Type,Stitch,T>::SVGTurbulenceRenderer(const Size &aBaseFrequency, int32_t aSeed,
                                                            int aNumOctaves, const IntRect &aTileRect)
 : mBaseFrequency(aBaseFrequency)
 , mNumOctaves(aNumOctaves)
{
  InitFromSeed(aSeed);
  if (Stitch) {
    AdjustBaseFrequencyForStitch(aTileRect);
    mStitchInfo = CreateStitchInfo(aTileRect);
  }
}

static void
Swap(int32_t& a, int32_t& b) {
  int32_t c = a;
  a = b;
  b = c;
}

template<TurbulenceType Type, bool Stitch, typename T>
void
SVGTurbulenceRenderer<Type,Stitch,T>::InitFromSeed(int32_t aSeed)
{
  RandomNumberSource rand(aSeed);

  vec2<T> gradient[4][sBSize];
  for (int32_t k = 0; k < 4; k++) {
    for (int32_t i = 0; i < sBSize; i++) {
      T a = T((rand.Next() % (sBSize + sBSize)) - sBSize) / sBSize;
      T b = T((rand.Next() % (sBSize + sBSize)) - sBSize) / sBSize;
      T s = sqrt(a * a + b * b);
      gradient[k][i].x() = a / s;
      gradient[k][i].y() = b / s;
    }
  }

  for (int32_t i = 0; i < sBSize; i++) {
    mGradient[i].x() = vec4<T>(gradient[0][i].x(), gradient[1][i].x(),
                                    gradient[2][i].x(), gradient[3][i].x());
    mGradient[i].y() = vec4<T>(gradient[0][i].y(), gradient[1][i].y(),
                                    gradient[2][i].y(), gradient[3][i].y());
  }

  for (int32_t i = 0; i < sBSize; i++) {
    mLatticeSelector[i] = i;
  }
  for (int32_t i1 = sBSize - 1; i1 > 0; i1--) {
    int32_t i2 = rand.Next() % sBSize;
    Swap(mLatticeSelector[i1], mLatticeSelector[i2]);
  }

  for (int32_t i = 0; i < sBSize + 2; i++) {
    mLatticeSelector[sBSize + i] = mLatticeSelector[i];
  }
}

// Adjust aFreq such that aLength * AdjustForLength(aFreq, aLength) is integer
// and as close to aLength * aFreq as possible.
static float
AdjustForLength(float aFreq, float aLength)
{
  float lowFreq = floor(aLength * aFreq) / aLength;
  float hiFreq = ceil(aLength * aFreq) / aLength;
  if (aFreq / lowFreq < hiFreq / aFreq) {
    return lowFreq;
  }
  return hiFreq;
}

template<TurbulenceType Type, bool Stitch, typename T>
void
SVGTurbulenceRenderer<Type,Stitch,T>::AdjustBaseFrequencyForStitch(const IntRect &aTileRect)
{
  mBaseFrequency = Size(AdjustForLength(mBaseFrequency.width, aTileRect.width),
                        AdjustForLength(mBaseFrequency.height, aTileRect.height));
}

// from xpcom/ds/nsMathUtils.h
template<typename T>
static int32_t
NS_lround(T x)
{
  return x >= 0.0f ? int32_t(x + 0.5f) : int32_t(x - 0.5f);
}

template<TurbulenceType Type, bool Stitch, typename T>
typename SVGTurbulenceRenderer<Type,Stitch,T>::StitchInfo
SVGTurbulenceRenderer<Type,Stitch,T>::CreateStitchInfo(const IntRect &aTileRect)
{
  StitchInfo stitch;
  stitch.mWidth = NS_lround(aTileRect.width * mBaseFrequency.width);
  stitch.mWrapX = int(aTileRect.x * mBaseFrequency.width + sPerlinN + stitch.mWidth);
  stitch.mHeight = NS_lround(aTileRect.height * mBaseFrequency.height);
  stitch.mWrapY = int(aTileRect.y * mBaseFrequency.height + sPerlinN + stitch.mHeight);
  return stitch;
}

template<typename T, typename S>
static vec2<S>
Mix(T ux, T vx, T y,
    const vec2<S> &qu,
    const vec2<S> &qv)
{
  S u = qu.x() * ux + qu.y() * y;
  S v = qv.x() * vx + qv.y() * y;
  return vec2<S>(u, v);
}

template<typename T>
static T
SCurve(T t)
{
  return t * t * (3 - 2 * t);
}

template<typename T>
static vec2<T>
SCurve(const vec2<T> &t)
{
  return vec2<T>(SCurve(t.x()), SCurve(t.y()));
}

template<typename T, typename S>
static S
Lerp(T t, S a, S b)
{
  return a + (b - a) * t;
}

template<typename T, typename S>
static S
BiLerp(const vec2<T> &s, const vec2<S> &a, const vec2<S> &b)
{
  const S xa = Lerp(s.x(), a.u(), a.v());
  const S xb = Lerp(s.x(), b.u(), b.v());
  return Lerp(s.y(), xa, xb);
}

template<TurbulenceType Type, bool Stitch, typename T>
vec4<T>
SVGTurbulenceRenderer<Type,Stitch,T>::Interpolate(vec2<uint8_t> b0, vec2<uint8_t> b1,
                                                      vec2<T> r0, vec2<T> r1) const
{
  static_assert(1 << (sizeof(b0.x()) * 8) <= sBSize, "mLatticeSelector is too small");

  int32_t i = mLatticeSelector[b0.x()];
  int32_t j = mLatticeSelector[b1.x()];

  vec2<vec4<T> > qua = mGradient[mLatticeSelector[i + b0.y()]];
  vec2<vec4<T> > qva = mGradient[mLatticeSelector[j + b0.y()]];
  vec2<vec4<T> > qub = mGradient[mLatticeSelector[i + b1.y()]];
  vec2<vec4<T> > qvb = mGradient[mLatticeSelector[j + b1.y()]];
  return BiLerp(SCurve(r0),
                Mix(r0.x(), r1.x(), r0.y(), qua, qva),
                Mix(r0.x(), r1.x(), r1.y(), qub, qvb));
}

template<TurbulenceType Type, bool Stitch, typename T>
vec4<T>
SVGTurbulenceRenderer<Type,Stitch,T>::Noise2(int aColorChannel, vec2<T> aVec, const StitchInfo& aStitchInfo) const
{
  vec2<T> t = aVec + vec2<T>(sPerlinN, sPerlinN);
  vec2<int32_t> lt = t;

  vec2<int32_t> b0 = lt;
  vec2<int32_t> b1 = lt + vec2<int32_t>(1, 1);

  // If stitching, adjust lattice points accordingly.
  if (Stitch) {
    if (b0.x() >= aStitchInfo.mWrapX)
      b0.x() -= aStitchInfo.mWidth;
    if (b1.x() >= aStitchInfo.mWrapX)
      b1.x() -= aStitchInfo.mWidth;
    if (b0.y() >= aStitchInfo.mWrapY)
      b0.y() -= aStitchInfo.mHeight;
    if (b1.y() >= aStitchInfo.mWrapY)
      b1.y() -= aStitchInfo.mHeight;
  }

  vec2<T> r0 = t - lt;
  vec2<T> r1 = r0 - vec2<T>(1, 1);

  return Interpolate(b0, b1, r0, r1);
}

template<typename T>
static vec4<T>
vabs(const vec4<T> &v)
{
  return vec4<T>(fabs(v._1), fabs(v._2), fabs(v._3), fabs(v._4));
}

template<TurbulenceType Type, bool Stitch, typename T>
vec4<T>
SVGTurbulenceRenderer<Type,Stitch,T>::Turbulence(int aColorChannel, const IntPoint &aPoint) const
{
  StitchInfo stitchInfo = mStitchInfo;
  vec4<T> sum;
  vec2<T> vec(aPoint.x * mBaseFrequency.width, aPoint.y * mBaseFrequency.height);
  T ratio = 1;
  for (int octave = 0; octave < mNumOctaves; octave++) {
    if (Type == TURBULENCE_TYPE_FRACTAL_NOISE) {
      sum += Noise2(aColorChannel, vec, stitchInfo) / ratio;
    } else {
      sum += vabs(Noise2(aColorChannel, vec, stitchInfo)) / ratio;
    }
    vec *= 2;
    ratio *= 2;

    if (Stitch) {
      // Update stitch values. Subtracting sPerlinN before the multiplication
      // and adding it afterward simplifies to subtracting it once.
      stitchInfo.mWidth *= 2;
      stitchInfo.mWrapX = 2 * stitchInfo.mWrapX - sPerlinN;
      stitchInfo.mHeight *= 2;
      stitchInfo.mWrapY = 2 * stitchInfo.mWrapY - sPerlinN;
    }
  }
  return sum;
}

// from xpcom/string/public/nsAlgorithm.h
template <class T>
inline const T&
clamped(const T& a, const T& min, const T& max)
{
  MOZ_ASSERT(max >= min, "clamped(): max must be greater than or equal to min");
  return std::min(std::max(a, min), max);
}

template<typename S>
static uint32_t
ColorToBGRA(const S& aColor)
{
  union {
    uint32_t color;
    uint8_t components[4];
  };
  components[2] = uint8_t(aColor.r() * aColor.a() * 255.0f + 0.5f);
  components[1] = uint8_t(aColor.g() * aColor.a() * 255.0f + 0.5f);
  components[0] = uint8_t(aColor.b() * aColor.a() * 255.0f + 0.5f);
  components[3] = uint8_t(aColor.a() * 255.0f + 0.5f);
  return color;
}

template<TurbulenceType Type, bool Stitch, typename T>
uint32_t
SVGTurbulenceRenderer<Type,Stitch,T>::ColorAtPoint(const IntPoint &aPoint) const
{
  vec4<T> col;
  if (Type == TURBULENCE_TYPE_TURBULENCE) {
    col = Turbulence(0, aPoint);
  } else {
    col = (Turbulence(0, aPoint) + vec4<T>(1,1,1,1)) / 2;
  }
  return ColorToBGRA(vec4<T>(clamped<T>(col.r(), 0, 1), clamped<T>(col.g(), 0, 1),
                             clamped<T>(col.b(), 0, 1), clamped<T>(col.a(), 0, 1)));
}

template class SVGTurbulenceRenderer<TURBULENCE_TYPE_TURBULENCE, false, double>;
template class SVGTurbulenceRenderer<TURBULENCE_TYPE_TURBULENCE, true, double>;
template class SVGTurbulenceRenderer<TURBULENCE_TYPE_FRACTAL_NOISE, false, double>;
template class SVGTurbulenceRenderer<TURBULENCE_TYPE_FRACTAL_NOISE, true, double>;
template class SVGTurbulenceRenderer<TURBULENCE_TYPE_TURBULENCE, false, float>;
template class SVGTurbulenceRenderer<TURBULENCE_TYPE_TURBULENCE, true, float>;
template class SVGTurbulenceRenderer<TURBULENCE_TYPE_FRACTAL_NOISE, false, float>;
template class SVGTurbulenceRenderer<TURBULENCE_TYPE_FRACTAL_NOISE, true, float>;

} // namespace gfx
} // namespace mozilla
