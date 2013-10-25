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

template<typename T>
static void
Swap(T& a, T& b) {
  T c = a;
  a = b;
  b = c;
}

template<TurbulenceType Type, bool Stitch, typename T>
void
SVGTurbulenceRenderer<Type,Stitch,T>::InitFromSeed(int32_t aSeed)
{
  RandomNumberSource rand(aSeed);

  T gradient[4][sBSize][2];
  for (int32_t k = 0; k < 4; k++) {
    for (int32_t i = 0; i < sBSize; i++) {
      T a = T((rand.Next() % (sBSize + sBSize)) - sBSize) / sBSize;
      T b = T((rand.Next() % (sBSize + sBSize)) - sBSize) / sBSize;
      T s = sqrt(a * a + b * b);
      gradient[k][i][0] = a / s;
      gradient[k][i][1] = b / s;
    }
  }

  for (int32_t i = 0; i < sBSize; i++) {
    mLatticeSelector[i] = i;
  }
  for (int32_t i1 = sBSize - 1; i1 > 0; i1--) {
    int32_t i2 = rand.Next() % sBSize;
    Swap(mLatticeSelector[i1], mLatticeSelector[i2]);
  }

  for (int32_t i = 0; i < sBSize; i++) {
    // Contrary to the code in the spec, we build the first lattice selector
    // lookup into mGradient so that we don't need to do it again for every
    // pixel.
    // We also change the order of the gradient indexing so that we can process
    // all four color channels at the same time.
    uint8_t j = mLatticeSelector[i];
    mGradient[i][0] = vec4<T>(gradient[0][j][0], gradient[1][j][0],
                              gradient[2][j][0], gradient[3][j][0]);
    mGradient[i][1] = vec4<T>(gradient[0][j][1], gradient[1][j][1],
                              gradient[2][j][1], gradient[3][j][1]);
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

template<TurbulenceType Type, bool Stitch, typename T>
typename SVGTurbulenceRenderer<Type,Stitch,T>::StitchInfo
SVGTurbulenceRenderer<Type,Stitch,T>::CreateStitchInfo(const IntRect &aTileRect) const
{
  StitchInfo stitch;
  stitch.width = int32_t(floorf(aTileRect.width * mBaseFrequency.width + 0.5f));
  stitch.height = int32_t(floorf(aTileRect.height * mBaseFrequency.height + 0.5f));
  stitch.wrapX = int32_t(aTileRect.x * mBaseFrequency.width) + stitch.width;
  stitch.wrapY = int32_t(aTileRect.y * mBaseFrequency.height) + stitch.height;
  return stitch;
}

template<typename T>
static inline T
SCurve(T t)
{
  return t * t * (3 - 2 * t);
}

static inline Point
SCurve(Point t)
{
  return Point(SCurve(t.x), SCurve(t.y));
}

template<typename S, typename T>
static inline S
Mix(S a, S b, T t)
{
  return a + (b - a) * t;
}

template<typename S>
static S
BiMix(S aa, S ab, S ba, S bb, Point s)
{
  const S xa = Mix(aa, ab, s.x);
  const S xb = Mix(ba, bb, s.x);
  return Mix(xa, xb, s.y);
}

template<typename T>
static inline vec4<T>
Interpolate(vec4<T> qua0, vec4<T> qua1, vec4<T> qub0, vec4<T> qub1,
            vec4<T> qva0, vec4<T> qva1, vec4<T> qvb0, vec4<T> qvb1,
            Point r)
{
  return BiMix(qua0 * r.x + qua1 * r.y,
               qva0 * (r.x - 1) + qva1 * r.y,
               qub0 * r.x + qub1 * (r.y - 1),
               qvb0 * (r.x - 1) + qvb1 * (r.y - 1),
               SCurve(r));
}

template<TurbulenceType Type, bool Stitch, typename T>
IntPoint
SVGTurbulenceRenderer<Type,Stitch,T>::AdjustForStitch(IntPoint aLatticePoint,
                                                      const StitchInfo& aStitchInfo) const
{
  if (Stitch) {
    if (aLatticePoint.x >= aStitchInfo.wrapX) {
      aLatticePoint.x -= aStitchInfo.width;
    }
    if (aLatticePoint.y >= aStitchInfo.wrapY) {
      aLatticePoint.y -= aStitchInfo.height;
    }
  }
  return aLatticePoint;
}

template<TurbulenceType Type, bool Stitch, typename T>
vec4<T>
SVGTurbulenceRenderer<Type,Stitch,T>::Noise2(Point aVec, const StitchInfo& aStitchInfo) const
{
  Point nearestLatticePoint(floorf(aVec.x), floorf(aVec.y));
  Point fractionalOffset = aVec - nearestLatticePoint;

  IntPoint nearestLatticePointInt(nearestLatticePoint.x, nearestLatticePoint.y);

  IntPoint b0 = AdjustForStitch(nearestLatticePointInt, aStitchInfo);
  IntPoint b1 = AdjustForStitch(b0 + IntPoint(1, 1), aStitchInfo);

  uint8_t i = mLatticeSelector[b0.x % sBSize];
  uint8_t j = mLatticeSelector[b1.x % sBSize];

  const vec4<T>* qua = mGradient[(i + b0.y) % sBSize];
  const vec4<T>* qub = mGradient[(i + b1.y) % sBSize];
  const vec4<T>* qva = mGradient[(j + b0.y) % sBSize];
  const vec4<T>* qvb = mGradient[(j + b1.y) % sBSize];

  return Interpolate(qua[0], qua[1], qub[0], qub[1],
                     qva[0], qva[1], qvb[0], qvb[1], fractionalOffset);
}

template<typename T>
static vec4<T>
vabs(const vec4<T> &v)
{
  return vec4<T>(fabs(v._1), fabs(v._2), fabs(v._3), fabs(v._4));
}

template<TurbulenceType Type, bool Stitch, typename T>
vec4<T>
SVGTurbulenceRenderer<Type,Stitch,T>::Turbulence(const IntPoint &aPoint) const
{
  StitchInfo stitchInfo = mStitchInfo;
  vec4<T> sum;
  Point vec(aPoint.x * mBaseFrequency.width, aPoint.y * mBaseFrequency.height);
  T ratio = 1;
  for (int octave = 0; octave < mNumOctaves; octave++) {
    if (Type == TURBULENCE_TYPE_FRACTAL_NOISE) {
      sum += Noise2(vec, stitchInfo) / ratio;
    } else {
      sum += vabs(Noise2(vec, stitchInfo)) / ratio;
    }
    vec = vec * 2;
    ratio *= 2;

    if (Stitch) {
      stitchInfo.width *= 2;
      stitchInfo.wrapX *= 2;
      stitchInfo.height *= 2;
      stitchInfo.wrapY *= 2;
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
    col = Turbulence(aPoint);
  } else {
    col = (Turbulence(aPoint) + vec4<T>(1,1,1,1)) / 2;
  }
  return ColorToBGRA(vec4<T>(clamped<T>(col.r(), 0, 1), clamped<T>(col.g(), 0, 1),
                             clamped<T>(col.b(), 0, 1), clamped<T>(col.a(), 0, 1)));
}

template class SVGTurbulenceRenderer<TURBULENCE_TYPE_TURBULENCE, false, float>;
template class SVGTurbulenceRenderer<TURBULENCE_TYPE_TURBULENCE, true, float>;
template class SVGTurbulenceRenderer<TURBULENCE_TYPE_FRACTAL_NOISE, false, float>;
template class SVGTurbulenceRenderer<TURBULENCE_TYPE_FRACTAL_NOISE, true, float>;

} // namespace gfx
} // namespace mozilla
