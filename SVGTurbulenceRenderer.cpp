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

template<TurbulenceType Type, bool Stitch>
SVGTurbulenceRenderer<Type,Stitch>::SVGTurbulenceRenderer(const Size &aBaseFrequency, int32_t aSeed,
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

template<TurbulenceType Type, bool Stitch>
void
SVGTurbulenceRenderer<Type,Stitch>::InitFromSeed(int32_t aSeed)
{
  RandomNumberSource rand(aSeed);

  vec2<double> gradient[4][sBSize];
  for (int32_t k = 0; k < 4; k++) {
    for (int32_t i = 0; i < sBSize; i++) {
      double a = double((rand.Next() % (sBSize + sBSize)) - sBSize) / sBSize;
      double b = double((rand.Next() % (sBSize + sBSize)) - sBSize) / sBSize;
      double s = sqrt(a * a + b * b);
      gradient[k][i].x() = a / s;
      gradient[k][i].y() = b / s;
    }
  }

  for (int32_t i = 0; i < sBSize; i++) {
    mGradient[i].x() = vec4<double>(gradient[0][i].x(), gradient[1][i].x(),
                                    gradient[2][i].x(), gradient[3][i].x());
    mGradient[i].y() = vec4<double>(gradient[0][i].y(), gradient[1][i].y(),
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

template<TurbulenceType Type, bool Stitch>
void
SVGTurbulenceRenderer<Type,Stitch>::AdjustBaseFrequencyForStitch(const IntRect &aTileRect)
{
  mBaseFrequency = Size(AdjustForLength(mBaseFrequency.width, aTileRect.width),
                        AdjustForLength(mBaseFrequency.height, aTileRect.height));
}

// from xpcom/ds/nsMathUtils.h
static int32_t
NS_lround(double x)
{
  return x >= 0.0 ? int32_t(x + 0.5) : int32_t(x - 0.5);
}

template<TurbulenceType Type, bool Stitch>
typename SVGTurbulenceRenderer<Type,Stitch>::StitchInfo
SVGTurbulenceRenderer<Type,Stitch>::CreateStitchInfo(const IntRect &aTileRect)
{
  StitchInfo stitch;
  stitch.mWidth = NS_lround(aTileRect.width * mBaseFrequency.width);
  stitch.mWrapX = int(aTileRect.x * mBaseFrequency.width + sPerlinN + stitch.mWidth);
  stitch.mHeight = NS_lround(aTileRect.height * mBaseFrequency.height);
  stitch.mWrapY = int(aTileRect.y * mBaseFrequency.height + sPerlinN + stitch.mHeight);
  return stitch;
}

template<typename T>
static vec2<T>
Mix(double ux, double vx, double y,
    const vec2<T> &qu,
    const vec2<T> &qv)
{
  T u = qu.x() * ux + qu.y() * y;
  T v = qv.x() * vx + qv.y() * y;
  return vec2<T>(u, v);
}

static double
SCurve(double t)
{
  return t * t * (3. - 2. * t);
}

static vec2<double>
SCurve(const vec2<double> &t)
{
  return vec2<double>(SCurve(t.x()), SCurve(t.y()));
}

template<typename T>
static T
Lerp(double t, T a, T b)
{
  return a + (b - a) * t;
}

template<typename T>
static T
BiLerp(const vec2<double> &s, const vec2<T> &a, const vec2<T> &b)
{
  const T xa = Lerp(s.x(), a.u(), a.v());
  const T xb = Lerp(s.x(), b.u(), b.v());
  return Lerp(s.y(), xa, xb);
}

template<TurbulenceType Type, bool Stitch>
vec4<double>
SVGTurbulenceRenderer<Type,Stitch>::Interpolate(vec2<uint8_t> b0, vec2<uint8_t> b1,
                                                      vec2<double> r0, vec2<double> r1)
{
  static_assert(1 << (sizeof(b0.x()) * 8) <= sBSize, "mLatticeSelector is too small");

  int32_t i = mLatticeSelector[b0.x()];
  int32_t j = mLatticeSelector[b1.x()];

  vec2<vec4<double> > qua = mGradient[mLatticeSelector[i + b0.y()]];
  vec2<vec4<double> > qva = mGradient[mLatticeSelector[j + b0.y()]];
  vec2<vec4<double> > qub = mGradient[mLatticeSelector[i + b1.y()]];
  vec2<vec4<double> > qvb = mGradient[mLatticeSelector[j + b1.y()]];
  return BiLerp(SCurve(r0),
                Mix(r0.x(), r1.x(), r0.y(), qua, qva),
                Mix(r0.x(), r1.x(), r1.y(), qub, qvb));
}

template<TurbulenceType Type, bool Stitch>
vec4<double>
SVGTurbulenceRenderer<Type,Stitch>::Noise2(int aColorChannel, vec2<double> aVec)
{
  vec2<double> t = aVec + vec2<double>(sPerlinN, sPerlinN);
  vec2<int32_t> lt = t;

  vec2<int32_t> b0 = lt;
  vec2<int32_t> b1 = lt + vec2<int32_t>(1, 1);

  // If stitching, adjust lattice points accordingly.
  if (Stitch) {
    if (b0.x() >= mStitchInfo.mWrapX)
      b0.x() -= mStitchInfo.mWidth;
    if (b1.x() >= mStitchInfo.mWrapX)
      b1.x() -= mStitchInfo.mWidth;
    if (b0.y() >= mStitchInfo.mWrapY)
      b0.y() -= mStitchInfo.mHeight;
    if (b1.y() >= mStitchInfo.mWrapY)
      b1.y() -= mStitchInfo.mHeight;
  }

  vec2<double> r0 = t - lt;
  vec2<double> r1 = r0 - vec2<double>(1.0f, 1.0f);

  return Interpolate(b0, b1, r0, r1);
}

static vec4<double>
vabs(const vec4<double> &v)
{
  return vec4<double>(fabs(v._1), fabs(v._2), fabs(v._3), fabs(v._4));
}

template<TurbulenceType Type, bool Stitch>
vec4<double>
SVGTurbulenceRenderer<Type,Stitch>::Turbulence(int aColorChannel, const IntPoint &aPoint)
{
  vec4<double> sum;
  vec2<double> vec(aPoint.x * mBaseFrequency.width, aPoint.y * mBaseFrequency.height);
  double ratio = 1;
  for (int octave = 0; octave < mNumOctaves; octave++) {
    if (Type == TURBULENCE_TYPE_FRACTAL_NOISE) {
      sum += Noise2(aColorChannel, vec) / ratio;
    } else {
      sum += vabs(Noise2(aColorChannel, vec)) / ratio;
    }
    vec *= 2;
    ratio *= 2;

    if (Stitch) {
      // Update stitch values. Subtracting sPerlinN before the multiplication
      // and adding it afterward simplifies to subtracting it once.
      mStitchInfo.mWidth *= 2;
      mStitchInfo.mWrapX = 2 * mStitchInfo.mWrapX - sPerlinN;
      mStitchInfo.mHeight *= 2;
      mStitchInfo.mWrapY = 2 * mStitchInfo.mWrapY - sPerlinN;
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

template<TurbulenceType Type, bool Stitch>
Color
SVGTurbulenceRenderer<Type,Stitch>::ColorAtPoint(const IntPoint &aPoint)
{
  vec4<double> col;
  if (Type == TURBULENCE_TYPE_TURBULENCE) {
    col = Turbulence(0, aPoint);
  } else {
    col = (Turbulence(0, aPoint) + vec4<double>(1,1,1,1)) / 2;
  }
  return Color(clamped(col.r(), 0.0, 1.0), clamped(col.g(), 0.0, 1.0),
               clamped(col.b(), 0.0, 1.0), clamped(col.a(), 0.0, 1.0));
}

template class SVGTurbulenceRenderer<TURBULENCE_TYPE_TURBULENCE, false>;
template class SVGTurbulenceRenderer<TURBULENCE_TYPE_TURBULENCE, true>;
template class SVGTurbulenceRenderer<TURBULENCE_TYPE_FRACTAL_NOISE, false>;
template class SVGTurbulenceRenderer<TURBULENCE_TYPE_FRACTAL_NOISE, true>;

} // namespace gfx
} // namespace mozilla
