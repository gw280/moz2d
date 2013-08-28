/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_GFX_SIMD_H_
#define _MOZILLA_GFX_SIMD_H_

#if defined(__i386) || defined(__x86_64__)
#define COMPILE_WITH_SSE2
#endif

#ifdef COMPILE_WITH_SSE2
#include <xmmintrin.h>
#endif

namespace mozilla {
namespace gfx {

namespace simd {


template<typename m128i_t>
m128i_t LoadFrom(const m128i_t* aSource);

template<typename m128i_t>
void StoreTo(m128i_t* aTarget, m128i_t aM);

template<typename m128i_t>
m128i_t From16(int16_t a, int16_t b, int16_t c, int16_t d, int16_t e, int16_t f, int16_t g, int16_t h);

template<typename m128i_t>
m128i_t From16(int16_t a);

template<typename m128i_t>
m128i_t From32(int32_t a, int32_t b, int32_t c, int32_t d);

template<typename m128i_t>
m128i_t ShiftRight16(m128i_t aM, int32_t aNumberOfBits);

template<typename m128i_t>
m128i_t ShiftRight32(m128i_t aM, int32_t aNumberOfBits);

template<typename m128i_t>
m128i_t Add16(m128i_t aM1, m128i_t aM2);

template<typename m128i_t>
m128i_t Add32(m128i_t aM1, m128i_t aM2);

template<typename m128i_t>
m128i_t Sub16(m128i_t aM1, m128i_t aM2);

template<typename m128i_t>
m128i_t Mul16(m128i_t aM1, m128i_t aM2);

template<typename m128i_t>
void Mul2x2x4x16To2x4x32(m128i_t aFactorsA1B1, m128i_t aFactorsA2B2,
                         m128i_t& aProductA, m128i_t& aProductB);

template<typename m128i_t>
m128i_t MulAdd2x8x16To4x32(m128i_t aFactorsA, m128i_t aFactorsB);

template<int8_t aIndex, typename m128i_t>
inline m128i_t Splat32(m128i_t aM);

template<typename m128i_t>
inline m128i_t InterleaveLo16(m128i_t m1, m128i_t m2);

template<typename m128i_t>
inline m128i_t InterleaveHi16(m128i_t m1, m128i_t m2);

template<typename m128i_t>
m128i_t UnpackLo8x8To8x16(m128i_t m);

template<typename m128i_t>
m128i_t UnpackHi8x8To8x16(m128i_t m);

template<typename m128i_t>
inline m128i_t PackAndSaturate32To16(m128i_t m1, m128i_t m2);

template<typename m128i_t>
m128i_t PackAndSaturate(m128i_t m1, m128i_t m2, m128i_t m3, m128i_t m4);

template<typename m128i_t>
m128i_t PackAndSaturate(m128i_t m1, m128i_t m2);


// Scalar

union ScalarM128i {
  uint8_t u8[16];
  int16_t i16[8];
  int32_t i32[4];
};

template<>
ScalarM128i
LoadFrom<ScalarM128i>(const ScalarM128i* aSource)
{
  return *aSource;
}

template<>
void StoreTo<ScalarM128i>(ScalarM128i* aTarget, ScalarM128i aM)
{
  *aTarget = aM;
}

template<>
ScalarM128i From16<ScalarM128i>(int16_t a, int16_t b, int16_t c, int16_t d, int16_t e, int16_t f, int16_t g, int16_t h)
{
  ScalarM128i m;
  m.i16[0] = a;
  m.i16[1] = b;
  m.i16[2] = c;
  m.i16[3] = d;
  m.i16[4] = e;
  m.i16[5] = f;
  m.i16[6] = g;
  m.i16[7] = h;
  return m;
}

template<>
ScalarM128i From16<ScalarM128i>(int16_t a)
{
  return From16<ScalarM128i>(a, a, a, a, a, a, a, a);
}

template<>
ScalarM128i From32<ScalarM128i>(int32_t a, int32_t b, int32_t c, int32_t d)
{
  ScalarM128i m;
  m.i32[0] = a;
  m.i32[1] = b;
  m.i32[2] = c;
  m.i32[3] = d;
  return m;
}

template<>
ScalarM128i ShiftRight16<ScalarM128i>(ScalarM128i aM, int32_t aNumberOfBits)
{
  return From16<ScalarM128i>(uint16_t(aM.i16[0]) >> aNumberOfBits, uint16_t(aM.i16[1]) >> aNumberOfBits,
                             uint16_t(aM.i16[2]) >> aNumberOfBits, uint16_t(aM.i16[3]) >> aNumberOfBits,
                             uint16_t(aM.i16[4]) >> aNumberOfBits, uint16_t(aM.i16[5]) >> aNumberOfBits,
                             uint16_t(aM.i16[6]) >> aNumberOfBits, uint16_t(aM.i16[7]) >> aNumberOfBits);
}

template<>
ScalarM128i ShiftRight32<ScalarM128i>(ScalarM128i aM, int32_t aNumberOfBits)
{
  return From32<ScalarM128i>(aM.i32[0] >> aNumberOfBits, aM.i32[1] >> aNumberOfBits,
                             aM.i32[2] >> aNumberOfBits, aM.i32[3] >> aNumberOfBits);
}

template<>
ScalarM128i Add16<ScalarM128i>(ScalarM128i aM1, ScalarM128i aM2)
{
  return From16<ScalarM128i>(aM1.i16[0] + aM2.i16[0], aM1.i16[1] + aM2.i16[1],
                             aM1.i16[2] + aM2.i16[2], aM1.i16[3] + aM2.i16[3],
                             aM1.i16[4] + aM2.i16[4], aM1.i16[5] + aM2.i16[5],
                             aM1.i16[6] + aM2.i16[6], aM1.i16[7] + aM2.i16[7]);
}

template<>
ScalarM128i Sub16<ScalarM128i>(ScalarM128i aM1, ScalarM128i aM2)
{
  return From16<ScalarM128i>(aM1.i16[0] - aM2.i16[0], aM1.i16[1] - aM2.i16[1],
                             aM1.i16[2] - aM2.i16[2], aM1.i16[3] - aM2.i16[3],
                             aM1.i16[4] - aM2.i16[4], aM1.i16[5] - aM2.i16[5],
                             aM1.i16[6] - aM2.i16[6], aM1.i16[7] - aM2.i16[7]);
}

template<>
ScalarM128i Add32<ScalarM128i>(ScalarM128i aM1, ScalarM128i aM2)
{
  return From32<ScalarM128i>(aM1.i32[0] + aM2.i32[0], aM1.i32[1] + aM2.i32[1],
                             aM1.i32[2] + aM2.i32[2], aM1.i32[3] + aM2.i32[3]);
}

template<>
ScalarM128i Mul16<ScalarM128i>(ScalarM128i aM1, ScalarM128i aM2)
{
  // We only want the lower 16 bits of each 32-bit result.
  return From16<ScalarM128i>(aM1.i16[0] * aM2.i16[0], aM1.i16[1] * aM2.i16[1],
                             aM1.i16[2] * aM2.i16[2], aM1.i16[3] * aM2.i16[3],
                             aM1.i16[4] * aM2.i16[4], aM1.i16[5] * aM2.i16[5],
                             aM1.i16[6] * aM2.i16[6], aM1.i16[7] * aM2.i16[7]);
}

template<>
void Mul2x2x4x16To2x4x32<ScalarM128i>(ScalarM128i aFactorsA1B1,
                                      ScalarM128i aFactorsA2B2,
                                      ScalarM128i& aProductA,
                                      ScalarM128i& aProductB)
{
  aProductA = From32<ScalarM128i>(aFactorsA1B1.i16[0] * aFactorsA2B2.i16[0],
                                  aFactorsA1B1.i16[1] * aFactorsA2B2.i16[1],
                                  aFactorsA1B1.i16[2] * aFactorsA2B2.i16[2],
                                  aFactorsA1B1.i16[3] * aFactorsA2B2.i16[3]);
  aProductB = From32<ScalarM128i>(aFactorsA1B1.i16[4] * aFactorsA2B2.i16[4],
                                  aFactorsA1B1.i16[5] * aFactorsA2B2.i16[5],
                                  aFactorsA1B1.i16[6] * aFactorsA2B2.i16[6],
                                  aFactorsA1B1.i16[7] * aFactorsA2B2.i16[7]);
}

template<>
ScalarM128i MulAdd2x8x16To4x32<ScalarM128i>(ScalarM128i aFactorsA,
                                            ScalarM128i aFactorsB)
{
  return From32<ScalarM128i>(aFactorsA.i16[0] * aFactorsB.i16[0] + aFactorsA.i16[1] * aFactorsB.i16[1],
                             aFactorsA.i16[2] * aFactorsB.i16[2] + aFactorsA.i16[3] * aFactorsB.i16[3],
                             aFactorsA.i16[4] * aFactorsB.i16[4] + aFactorsA.i16[5] * aFactorsB.i16[5],
                             aFactorsA.i16[6] * aFactorsB.i16[6] + aFactorsA.i16[7] * aFactorsB.i16[7]);
}

template<int8_t aIndex>
void AssertIndex()
{
  static_assert(aIndex == 0 || aIndex == 1 || aIndex == 2 || aIndex == 3,
                "Invalid splat index");
}

template<int8_t aIndex>
inline ScalarM128i Splat32(ScalarM128i aM)
{
  AssertIndex<aIndex>();
  return From32<ScalarM128i>(aM.i32[aIndex], aM.i32[aIndex],
                             aM.i32[aIndex], aM.i32[aIndex]);
}

template<int8_t i0, int8_t i1, int8_t i2, int8_t i3>
inline ScalarM128i ShuffleLo16(ScalarM128i aM)
{
  AssertIndex<i0>();
  AssertIndex<i1>();
  AssertIndex<i2>();
  AssertIndex<i3>();
  ScalarM128i m = aM;
  m.i16[0] = aM.i16[i3];
  m.i16[1] = aM.i16[i2];
  m.i16[2] = aM.i16[i1];
  m.i16[3] = aM.i16[i0];
  return m;
}

template<int8_t i0, int8_t i1, int8_t i2, int8_t i3>
inline ScalarM128i ShuffleHi16(ScalarM128i aM)
{
  AssertIndex<i0>();
  AssertIndex<i1>();
  AssertIndex<i2>();
  AssertIndex<i3>();
  ScalarM128i m = aM;
  m.i16[4 + 0] = aM.i16[4 + i3];
  m.i16[4 + 1] = aM.i16[4 + i2];
  m.i16[4 + 2] = aM.i16[4 + i1];
  m.i16[4 + 3] = aM.i16[4 + i0];
  return m;
}

template<int8_t aIndex>
inline ScalarM128i SplatLo16(ScalarM128i aM)
{
  AssertIndex<aIndex>();
  ScalarM128i m = aM;
  int16_t chosenValue = aM.i16[aIndex];
  m.i16[0] = chosenValue;
  m.i16[1] = chosenValue;
  m.i16[2] = chosenValue;
  m.i16[3] = chosenValue;
  return m;
}

template<int8_t aIndex>
inline ScalarM128i SplatHi16(ScalarM128i aM)
{
  AssertIndex<aIndex>();
  ScalarM128i m = aM;
  int16_t chosenValue = aM.i16[4 + aIndex];
  m.i16[4] = chosenValue;
  m.i16[5] = chosenValue;
  m.i16[6] = chosenValue;
  m.i16[7] = chosenValue;
  return m;
}

template<>
inline ScalarM128i
InterleaveLo16<ScalarM128i>(ScalarM128i m1, ScalarM128i m2)
{
  return From16<ScalarM128i>(m1.i16[0], m2.i16[0], m1.i16[1], m2.i16[1],
                             m1.i16[2], m2.i16[2], m1.i16[3], m2.i16[3]);
}

template<>
inline ScalarM128i
InterleaveHi16<ScalarM128i>(ScalarM128i m1, ScalarM128i m2)
{
  return From16<ScalarM128i>(m1.i16[4], m2.i16[4], m1.i16[5], m2.i16[5],
                             m1.i16[6], m2.i16[6], m1.i16[7], m2.i16[7]);
}

template<>
ScalarM128i
UnpackLo8x8To8x16<ScalarM128i>(ScalarM128i aM)
{
  ScalarM128i m;
  m.i16[0] = aM.u8[0];
  m.i16[1] = aM.u8[1];
  m.i16[2] = aM.u8[2];
  m.i16[3] = aM.u8[3];
  m.i16[4] = aM.u8[4];
  m.i16[5] = aM.u8[5];
  m.i16[6] = aM.u8[6];
  m.i16[7] = aM.u8[7];
  return m;
}

template<>
ScalarM128i
UnpackHi8x8To8x16<ScalarM128i>(ScalarM128i aM)
{
  ScalarM128i m;
  m.i16[0] = aM.u8[8+0];
  m.i16[1] = aM.u8[8+1];
  m.i16[2] = aM.u8[8+2];
  m.i16[3] = aM.u8[8+3];
  m.i16[4] = aM.u8[8+4];
  m.i16[5] = aM.u8[8+5];
  m.i16[6] = aM.u8[8+6];
  m.i16[7] = aM.u8[8+7];
  return m;
}

template<typename T>
static int16_t
SaturateTo16(T a)
{
  return int16_t(a >= INT16_MIN ? (a <= INT16_MAX ? a : INT16_MAX) : INT16_MIN);
}

template<>
ScalarM128i
PackAndSaturate32To16<ScalarM128i>(ScalarM128i m1, ScalarM128i m2)
{
  ScalarM128i m;
  m.i16[0] = SaturateTo16(m1.i32[0]);
  m.i16[1] = SaturateTo16(m1.i32[1]);
  m.i16[2] = SaturateTo16(m1.i32[2]);
  m.i16[3] = SaturateTo16(m1.i32[3]);
  m.i16[4] = SaturateTo16(m2.i32[0]);
  m.i16[5] = SaturateTo16(m2.i32[1]);
  m.i16[6] = SaturateTo16(m2.i32[2]);
  m.i16[7] = SaturateTo16(m2.i32[3]);
  return m;
}

template<typename T>
static uint8_t
SaturateTo8(T a)
{
  return uint8_t(a >= 0 ? (a <= 255 ? a : 255) : 0);
}

template<>
ScalarM128i
PackAndSaturate<ScalarM128i>(ScalarM128i m1, ScalarM128i m2, ScalarM128i m3, ScalarM128i m4)
{
  ScalarM128i m;
  m.u8[0]  = SaturateTo8(m1.i32[0]);
  m.u8[1]  = SaturateTo8(m1.i32[1]);
  m.u8[2]  = SaturateTo8(m1.i32[2]);
  m.u8[3]  = SaturateTo8(m1.i32[3]);
  m.u8[4]  = SaturateTo8(m2.i32[0]);
  m.u8[5]  = SaturateTo8(m2.i32[1]);
  m.u8[6]  = SaturateTo8(m2.i32[2]);
  m.u8[7]  = SaturateTo8(m2.i32[3]);
  m.u8[8]  = SaturateTo8(m3.i32[0]);
  m.u8[9]  = SaturateTo8(m3.i32[1]);
  m.u8[10] = SaturateTo8(m3.i32[2]);
  m.u8[11] = SaturateTo8(m3.i32[3]);
  m.u8[12] = SaturateTo8(m4.i32[0]);
  m.u8[13] = SaturateTo8(m4.i32[1]);
  m.u8[14] = SaturateTo8(m4.i32[2]);
  m.u8[15] = SaturateTo8(m4.i32[3]);
  return m;
}

template<>
ScalarM128i
PackAndSaturate<ScalarM128i>(ScalarM128i m1, ScalarM128i m2)
{
  ScalarM128i m;
  m.u8[0]  = SaturateTo8(m1.i16[0]);
  m.u8[1]  = SaturateTo8(m1.i16[1]);
  m.u8[2]  = SaturateTo8(m1.i16[2]);
  m.u8[3]  = SaturateTo8(m1.i16[3]);
  m.u8[4]  = SaturateTo8(m1.i16[4]);
  m.u8[5]  = SaturateTo8(m1.i16[5]);
  m.u8[6]  = SaturateTo8(m1.i16[6]);
  m.u8[7]  = SaturateTo8(m1.i16[7]);
  m.u8[8]  = SaturateTo8(m2.i16[0]);
  m.u8[9]  = SaturateTo8(m2.i16[1]);
  m.u8[10] = SaturateTo8(m2.i16[2]);
  m.u8[11] = SaturateTo8(m2.i16[3]);
  m.u8[12] = SaturateTo8(m2.i16[4]);
  m.u8[13] = SaturateTo8(m2.i16[5]);
  m.u8[14] = SaturateTo8(m2.i16[6]);
  m.u8[15] = SaturateTo8(m2.i16[7]);
  return m;
}

template<int8_t aIndex>
ScalarM128i
SetComponent16(ScalarM128i aM, int16_t aValue)
{
  ScalarM128i m = aM;
  m.i16[aIndex] = aValue;
  return m;
}

// Fast approximate division by 255. It has the property that
// for all 0 <= n <= 255*255, FAST_DIVIDE_BY_255(n) == n/255.
// But it only uses two adds and two shifts instead of an
// integer division (which is expensive on many processors).
//
// equivalent to v/255
template<class B, class A>
static B FastDivideBy255(A v)
{
  return ((v << 8) + v + 255) >> 16;
}

inline ScalarM128i
FastDivideBy255_16(ScalarM128i m)
{
  return From16<ScalarM128i>(FastDivideBy255<uint16_t>(uint16_t(m.i16[0])),
                             FastDivideBy255<uint16_t>(uint16_t(m.i16[1])),
                             FastDivideBy255<uint16_t>(uint16_t(m.i16[2])),
                             FastDivideBy255<uint16_t>(uint16_t(m.i16[3])),
                             FastDivideBy255<uint16_t>(uint16_t(m.i16[4])),
                             FastDivideBy255<uint16_t>(uint16_t(m.i16[5])),
                             FastDivideBy255<uint16_t>(uint16_t(m.i16[6])),
                             FastDivideBy255<uint16_t>(uint16_t(m.i16[7])));
}

inline ScalarM128i
FastDivideBy255(ScalarM128i m)
{
  return From32<ScalarM128i>(FastDivideBy255<int32_t>(m.i32[0]),
                             FastDivideBy255<int32_t>(m.i32[1]),
                             FastDivideBy255<int32_t>(m.i32[2]),
                             FastDivideBy255<int32_t>(m.i32[3]));
}

#ifdef COMPILE_WITH_SSE2

// SSE2

template<>
__m128i
LoadFrom<__m128i>(const __m128i* aSource)
{
  return _mm_loadu_si128(aSource);
}

template<>
void StoreTo<__m128i>(__m128i* aTarget, __m128i aM)
{
  _mm_store_si128(aTarget, aM);
}

template<>
__m128i From16<__m128i>(int16_t a, int16_t b, int16_t c, int16_t d, int16_t e, int16_t f, int16_t g, int16_t h)
{
  return _mm_setr_epi16(a, b, c, d, e, f, g, h);
}

template<>
__m128i From16<__m128i>(int16_t a)
{
  return _mm_set1_epi16(a);
}

template<>
__m128i From32<__m128i>(int32_t a, int32_t b, int32_t c, int32_t d); // XXX

template<>
__m128i ShiftRight16<__m128i>(__m128i aM, int32_t aNumberOfBits)
{
  return _mm_srli_epi16(aM, aNumberOfBits);
}

template<>
__m128i ShiftRight32<__m128i>(__m128i aM, int32_t aNumberOfBits)
{
  return _mm_srai_epi32(aM, aNumberOfBits);
}

template<>
__m128i Add16<__m128i>(__m128i aM1, __m128i aM2)
{
  return _mm_add_epi16(aM1, aM2);
}

template<>
__m128i Add32<__m128i>(__m128i aM1, __m128i aM2)
{
  return _mm_add_epi32(aM1, aM2);
}

template<>
__m128i Sub16<__m128i>(__m128i aM1, __m128i aM2)
{
  return _mm_sub_epi16(aM1, aM2);
}

template<>
__m128i Mul16<__m128i>(__m128i aM1, __m128i aM2)
{
  return _mm_mullo_epi16(aM1, aM2);
}

template<>
void Mul2x2x4x16To2x4x32<__m128i>(__m128i aFactorsA1B1,
                                  __m128i aFactorsA2B2,
                                  __m128i& aProductA,
                                  __m128i& aProductB)
{
  __m128i prodAB_lo = _mm_mullo_epi16(aFactorsA1B1, aFactorsA2B2);
  __m128i prodAB_hi = _mm_mulhi_epi16(aFactorsA1B1, aFactorsA2B2);
  aProductA = _mm_unpacklo_epi16(prodAB_lo, prodAB_hi);
  aProductB = _mm_unpackhi_epi16(prodAB_lo, prodAB_hi);
}

template<>
__m128i MulAdd2x8x16To4x32<__m128i>(__m128i aFactorsA,
                                    __m128i aFactorsB)
{
  return _mm_madd_epi16(aFactorsA, aFactorsB);
}

template<int8_t i0, int8_t i1, int8_t i2, int8_t i3>
inline __m128i Shuffle32(__m128i aM)
{
  AssertIndex<i0>();
  AssertIndex<i1>();
  AssertIndex<i2>();
  AssertIndex<i3>();
  return _mm_shuffle_epi32(aM, _MM_SHUFFLE(i0, i1, i2, i3));
}

template<int8_t i0, int8_t i1, int8_t i2, int8_t i3>
inline __m128i ShuffleLo16(__m128i aM)
{
  AssertIndex<i0>();
  AssertIndex<i1>();
  AssertIndex<i2>();
  AssertIndex<i3>();
  return _mm_shufflelo_epi16(aM, _MM_SHUFFLE(i0, i1, i2, i3));
}

template<int8_t i0, int8_t i1, int8_t i2, int8_t i3>
inline __m128i ShuffleHi16(__m128i aM)
{
  AssertIndex<i0>();
  AssertIndex<i1>();
  AssertIndex<i2>();
  AssertIndex<i3>();
  return _mm_shufflehi_epi16(aM, _MM_SHUFFLE(i0, i1, i2, i3));
}

template<int8_t aIndex>
inline __m128i Splat32(__m128i aM)
{
  return Shuffle32<aIndex,aIndex,aIndex,aIndex>(aM);
}

template<int8_t aIndex>
inline __m128i SplatLo16(__m128i aM)
{
  AssertIndex<aIndex>();
  return ShuffleLo16<aIndex,aIndex,aIndex,aIndex>(aM);
}

template<int8_t aIndex>
inline __m128i SplatHi16(__m128i aM)
{
  AssertIndex<aIndex>();
  return ShuffleHi16<aIndex,aIndex,aIndex,aIndex>(aM);
}

template<>
__m128i
UnpackLo8x8To8x16<__m128i>(__m128i m)
{
  __m128i zero = _mm_set1_epi8(0);
  return _mm_unpacklo_epi8(m, zero);
}

template<>
__m128i
UnpackHi8x8To8x16<__m128i>(__m128i m)
{
  __m128i zero = _mm_set1_epi8(0);
  return _mm_unpackhi_epi8(m, zero);
}

__m128i
InterleaveLo16(__m128i m1, __m128i m2)
{
  return _mm_unpacklo_epi16(m1, m2);
}

__m128i
InterleaveHi16(__m128i m1, __m128i m2)
{
  return _mm_unpackhi_epi16(m1, m2);
}

template<>
inline __m128i
PackAndSaturate32To16<__m128i>(__m128i m1, __m128i m2)
{
  return _mm_packs_epi32(m1, m2);
}

template<>
__m128i
PackAndSaturate<__m128i>(__m128i m1, __m128i m2, __m128i m3, __m128i m4)
{
  // Pack into 8 16bit signed integers (saturating).
  __m128i m12 = _mm_packs_epi32(m1, m2);
  __m128i m34 = _mm_packs_epi32(m3, m4);

  // Pack into 16 8bit unsigned integers (saturating).
  return _mm_packus_epi16(m12, m34);
}

template<>
__m128i
PackAndSaturate<__m128i>(__m128i m1, __m128i m2)
{
  // Pack into 16 8bit unsigned integers (saturating).
  return _mm_packus_epi16(m1, m2);
}

template<int8_t aIndex>
inline __m128i
SetComponent16(__m128i aM, int32_t aValue)
{
  return _mm_insert_epi16(aM, aValue, aIndex);
}

inline __m128i
FastDivideBy255(__m128i m)
{
  // v = m << 8
  __m128i v = _mm_slli_epi32(m, 8);
  // v = v + (m + (255,255,255,255))
  v = _mm_add_epi32(v, _mm_add_epi32(m, _mm_set1_epi32(255)));
  // v = v >> 16
  return _mm_srai_epi32(v, 16);
}

inline __m128i
FastDivideBy255_16(__m128i m)
{
  __m128i zero = _mm_set1_epi16(0);
  __m128i lo = _mm_unpacklo_epi16(m, zero);
  __m128i hi = _mm_unpackhi_epi16(m, zero);
  return _mm_packs_epi32(FastDivideBy255(lo), FastDivideBy255(hi));
}

template<typename m128i_t>
inline void
print__m128i(const char* name, m128i_t m)
{
  union {
    m128i_t v;
    uint8_t u8[16];
    int16_t i16[8];
    int32_t i32[4];
  };
  v = m;
  printf("%s:\n", name);
  printf("  u8: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n", u8[0], u8[1], u8[2], u8[3], u8[4], u8[5], u8[6], u8[7], u8[8], u8[9], u8[10], u8[11], u8[12], u8[13], u8[14], u8[15]);
  printf(" i16: %d, %d, %d, %d, %d, %d, %d, %d\n", i16[0], i16[1], i16[2], i16[3], i16[4], i16[5], i16[6], i16[7]);
  printf(" i32: %d, %d, %d, %d\n", i32[0], i32[1], i32[2], i32[3]);
}

#define PRINT__M128I(what) print__m128i(#what, what)

#endif // COMPILE_WITH_SSE2

} // namespace simd

} // namespace gfx
} // namespace mozilla

#endif // _MOZILLA_GFX_SIMD_H_
