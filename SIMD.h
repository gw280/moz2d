/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_GFX_SIMD_H_
#define _MOZILLA_GFX_SIMD_H_

/**
 * Consumers of this file need to #define SIMD_COMPILE_SSE2 before including it
 * if they want access to the SSE2 functions.
 */

#ifdef SIMD_COMPILE_SSE2
#include <xmmintrin.h>
#endif

namespace mozilla {
namespace gfx {

namespace simd {

template<typename u8x16_t>
u8x16_t Load8(const uint8_t* aSource);

template<typename u8x16_t>
u8x16_t From8(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f, uint8_t g, uint8_t h,
              uint8_t i, uint8_t j, uint8_t k, uint8_t l, uint8_t m, uint8_t n, uint8_t o, uint8_t p);

template<typename u8x16_t>
u8x16_t FromZero8();

template<typename i16x8_t>
i16x8_t From16(int16_t a, int16_t b, int16_t c, int16_t d, int16_t e, int16_t f, int16_t g, int16_t h);

template<typename i16x8_t>
i16x8_t From16(int16_t a);

template<typename i32x4_t>
i32x4_t From32(int32_t a, int32_t b, int32_t c, int32_t d);

template<typename i32x4_t>
i32x4_t From32(int32_t a);

// All SIMD backends overload these functions for their SIMD types:

#if 0

// Store 16 bytes to a 16-byte aligned address
void Store8(uint8_t* aTarget, u8x16_t aM);

// Fixed shifts
template<int32_t aNumberOfBits> i16x8_t ShiftRight16(i16x8_t aM);
template<int32_t aNumberOfBits> i32x4_t ShiftRight32(i32x4_t aM);

i16x8_t Add16(i16x8_t aM1, i16x8_t aM2);
i32x4_t Add32(i32x4_t aM1, i32x4_t aM2);
i16x8_t Sub16(i16x8_t aM1, i16x8_t aM2);
i32x4_t Sub32(i32x4_t aM1, i32x4_t aM2);
i16x8_t Min16(i16x8_t aM1, i16x8_t aM2);
i16x8_t Max16(i16x8_t aM1, i16x8_t aM2);
i32x4_t Min32(i32x4_t aM1, i32x4_t aM2);
i32x4_t Max32(i32x4_t aM1, i32x4_t aM2);

// Truncating i16 -> i16 multiplication
i16x8_t Mul16(i16x8_t aM1, i16x8_t aM2);

// Long multiplication i16 -> i32
// aFactorsA1B1 = (a1[4] b1[4])
// aFactorsA2B2 = (a2[4] b2[4])
// aProductA = a1 * a2, aProductB = b1 * b2
void Mul16x4x2x2To32x4x2(i16x8_t aFactorsA1B1, i16x8_t aFactorsA2B2,
                         i32x4_t& aProductA, i32x4_t& aProductB);

// Long multiplication + pairwise addition i16 -> i32
// See the scalar implementation for specifics.
i32x4_t MulAdd16x8x2To32x4(i16x8_t aFactorsA, i16x8_t aFactorsB);

// Set all four 32-bit components to the value of the component at aIndex.
template<int8_t aIndex>
i32x4_t Splat32(i32x4_t aM);

// Interpret the input as four 32-bit values, apply Splat32<aIndex> on them,
// re-interpret the result as sixteen 8-bit values.
template<int8_t aIndex>
u8x16_t Splat32On8(u8x16_t aM);

template<int8_t i0, int8_t i1, int8_t i2, int8_t i3> i32x4 Shuffle32(i32x4 aM);
template<int8_t i0, int8_t i1, int8_t i2, int8_t i3> i16x8 ShuffleLo16(i16x8 aM);
template<int8_t i0, int8_t i1, int8_t i2, int8_t i3> i16x8 ShuffleHi16(i16x8 aM);

u8x16_t InterleaveLo8(u8x16_t m1, u8x16_t m2);
u8x16_t InterleaveHi8(u8x16_t m1, u8x16_t m2);
i16x8_t InterleaveLo16(i16x8_t m1, i16x8_t m2);
i16x8_t InterleaveHi16(i16x8_t m1, i16x8_t m2);
i32x4_t InterleaveLo32(i32x4_t m1, i32x4_t m2);

i16x8_t UnpackLo8x8To16x8(u8x16_t m);
i16x8_t UnpackHi8x8To16x8(u8x16_t m);

i16x8_t PackAndSaturate32To16(i32x4_t m1, i32x4_t m2);
u8x16_t PackAndSaturate16To8(i16x8_t m1, i16x8_t m2);
u8x16_t PackAndSaturate32To8(i32x4_t m1, i32x4_t m2, i32x4_t m3, const i32x4_t& m4);

template<int8_t aIndex> i16x8 SetComponent16(i16x8 aM, int32_t aValue);

i32x4 FastDivideBy255(i32x4 m);
i16x8 FastDivideBy255_16(i16x8 m);

#endif

// Scalar

struct Scalaru8x16_t {
  uint8_t u8[16];
};

struct Scalari16x8_t {
  int16_t i16[8];
};

struct Scalari32x4_t {
  int32_t i32[4];
};

template<>
inline Scalaru8x16_t
Load8<Scalaru8x16_t>(const uint8_t* aSource)
{
  return *(Scalaru8x16_t*)aSource;
}

inline void Store8(uint8_t* aTarget, Scalaru8x16_t aM)
{
  *(Scalaru8x16_t*)aTarget = aM;
}

template<>
inline Scalaru8x16_t From8<Scalaru8x16_t>(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f, uint8_t g, uint8_t h,
                                          uint8_t i, uint8_t j, uint8_t k, uint8_t l, uint8_t m, uint8_t n, uint8_t o, uint8_t p)
{
  Scalaru8x16_t _m;
  _m.u8[0] = a;
  _m.u8[1] = b;
  _m.u8[2] = c;
  _m.u8[3] = d;
  _m.u8[4] = e;
  _m.u8[5] = f;
  _m.u8[6] = g;
  _m.u8[7] = h;
  _m.u8[8+0] = i;
  _m.u8[8+1] = j;
  _m.u8[8+2] = k;
  _m.u8[8+3] = l;
  _m.u8[8+4] = m;
  _m.u8[8+5] = n;
  _m.u8[8+6] = o;
  _m.u8[8+7] = p;
  return _m;
}

template<>
inline Scalaru8x16_t FromZero8<Scalaru8x16_t>()
{
  return From8<Scalaru8x16_t>(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
}

template<>
inline Scalari16x8_t From16<Scalari16x8_t>(int16_t a, int16_t b, int16_t c, int16_t d, int16_t e, int16_t f, int16_t g, int16_t h)
{
  Scalari16x8_t m;
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
inline Scalari16x8_t From16<Scalari16x8_t>(int16_t a)
{
  return From16<Scalari16x8_t>(a, a, a, a, a, a, a, a);
}

template<>
inline Scalari32x4_t From32<Scalari32x4_t>(int32_t a, int32_t b, int32_t c, int32_t d)
{
  Scalari32x4_t m;
  m.i32[0] = a;
  m.i32[1] = b;
  m.i32[2] = c;
  m.i32[3] = d;
  return m;
}

template<>
inline Scalari32x4_t From32<Scalari32x4_t>(int32_t a)
{
  return From32<Scalari32x4_t>(a, a, a, a);
}

template<int32_t aNumberOfBits>
inline Scalari16x8_t ShiftRight16(Scalari16x8_t aM)
{
  return From16<Scalari16x8_t>(uint16_t(aM.i16[0]) >> aNumberOfBits, uint16_t(aM.i16[1]) >> aNumberOfBits,
                               uint16_t(aM.i16[2]) >> aNumberOfBits, uint16_t(aM.i16[3]) >> aNumberOfBits,
                               uint16_t(aM.i16[4]) >> aNumberOfBits, uint16_t(aM.i16[5]) >> aNumberOfBits,
                               uint16_t(aM.i16[6]) >> aNumberOfBits, uint16_t(aM.i16[7]) >> aNumberOfBits);
}

template<int32_t aNumberOfBits>
inline Scalari32x4_t ShiftRight32(Scalari32x4_t aM)
{
  return From32<Scalari32x4_t>(aM.i32[0] >> aNumberOfBits, aM.i32[1] >> aNumberOfBits,
                               aM.i32[2] >> aNumberOfBits, aM.i32[3] >> aNumberOfBits);
}

inline Scalari16x8_t Add16(Scalari16x8_t aM1, Scalari16x8_t aM2)
{
  return From16<Scalari16x8_t>(aM1.i16[0] + aM2.i16[0], aM1.i16[1] + aM2.i16[1],
                               aM1.i16[2] + aM2.i16[2], aM1.i16[3] + aM2.i16[3],
                               aM1.i16[4] + aM2.i16[4], aM1.i16[5] + aM2.i16[5],
                               aM1.i16[6] + aM2.i16[6], aM1.i16[7] + aM2.i16[7]);
}

inline Scalari32x4_t Add32(Scalari32x4_t aM1, Scalari32x4_t aM2)
{
  return From32<Scalari32x4_t>(aM1.i32[0] + aM2.i32[0], aM1.i32[1] + aM2.i32[1],
                               aM1.i32[2] + aM2.i32[2], aM1.i32[3] + aM2.i32[3]);
}

inline Scalari16x8_t Sub16(Scalari16x8_t aM1, Scalari16x8_t aM2)
{
  return From16<Scalari16x8_t>(aM1.i16[0] - aM2.i16[0], aM1.i16[1] - aM2.i16[1],
                               aM1.i16[2] - aM2.i16[2], aM1.i16[3] - aM2.i16[3],
                               aM1.i16[4] - aM2.i16[4], aM1.i16[5] - aM2.i16[5],
                               aM1.i16[6] - aM2.i16[6], aM1.i16[7] - aM2.i16[7]);
}

inline Scalari32x4_t Sub32(Scalari32x4_t aM1, Scalari32x4_t aM2)
{
  return From32<Scalari32x4_t>(aM1.i32[0] - aM2.i32[0], aM1.i32[1] - aM2.i32[1],
                               aM1.i32[2] - aM2.i32[2], aM1.i32[3] - aM2.i32[3]);
}

inline int32_t
umin(int32_t a, int32_t b)
{
  return a - ((a - b) & -(a > b));
}

inline int32_t
umax(int32_t a, int32_t b)
{
  return a - ((a - b) & -(a < b));
}

inline Scalari16x8_t Min16(Scalari16x8_t aM1, Scalari16x8_t aM2)
{
  return From16<Scalari16x8_t>(umin(aM1.i16[0], aM2.i16[0]), umin(aM1.i16[1], aM2.i16[1]),
                               umin(aM1.i16[2], aM2.i16[2]), umin(aM1.i16[3], aM2.i16[3]),
                               umin(aM1.i16[4], aM2.i16[4]), umin(aM1.i16[5], aM2.i16[5]),
                               umin(aM1.i16[6], aM2.i16[6]), umin(aM1.i16[7], aM2.i16[7]));
}

inline Scalari16x8_t Max16(Scalari16x8_t aM1, Scalari16x8_t aM2)
{
  return From16<Scalari16x8_t>(umax(aM1.i16[0], aM2.i16[0]), umax(aM1.i16[1], aM2.i16[1]),
                               umax(aM1.i16[2], aM2.i16[2]), umax(aM1.i16[3], aM2.i16[3]),
                               umax(aM1.i16[4], aM2.i16[4]), umax(aM1.i16[5], aM2.i16[5]),
                               umax(aM1.i16[6], aM2.i16[6]), umax(aM1.i16[7], aM2.i16[7]));
}

inline Scalari32x4_t Min32(Scalari32x4_t aM1, Scalari32x4_t aM2)
{
  return From32<Scalari32x4_t>(umin(aM1.i32[0], aM2.i32[0]), umin(aM1.i32[1], aM2.i32[1]),
                               umin(aM1.i32[2], aM2.i32[2]), umin(aM1.i32[3], aM2.i32[3]));
}

inline Scalari32x4_t Max32(Scalari32x4_t aM1, Scalari32x4_t aM2)
{
  return From32<Scalari32x4_t>(umax(aM1.i32[0], aM2.i32[0]), umax(aM1.i32[1], aM2.i32[1]),
                               umax(aM1.i32[2], aM2.i32[2]), umax(aM1.i32[3], aM2.i32[3]));
}

inline Scalari16x8_t Mul16(Scalari16x8_t aM1, Scalari16x8_t aM2)
{
  // We only want the lower 16 bits of each 32-bit result.
  return From16<Scalari16x8_t>(aM1.i16[0] * aM2.i16[0], aM1.i16[1] * aM2.i16[1],
                               aM1.i16[2] * aM2.i16[2], aM1.i16[3] * aM2.i16[3],
                               aM1.i16[4] * aM2.i16[4], aM1.i16[5] * aM2.i16[5],
                               aM1.i16[6] * aM2.i16[6], aM1.i16[7] * aM2.i16[7]);
}

inline void Mul16x4x2x2To32x4x2(Scalari16x8_t aFactorsA1B1,
                                Scalari16x8_t aFactorsA2B2,
                                Scalari32x4_t& aProductA,
                                Scalari32x4_t& aProductB)
{
  aProductA = From32<Scalari32x4_t>(aFactorsA1B1.i16[0] * aFactorsA2B2.i16[0],
                                    aFactorsA1B1.i16[1] * aFactorsA2B2.i16[1],
                                    aFactorsA1B1.i16[2] * aFactorsA2B2.i16[2],
                                    aFactorsA1B1.i16[3] * aFactorsA2B2.i16[3]);
  aProductB = From32<Scalari32x4_t>(aFactorsA1B1.i16[4] * aFactorsA2B2.i16[4],
                                    aFactorsA1B1.i16[5] * aFactorsA2B2.i16[5],
                                    aFactorsA1B1.i16[6] * aFactorsA2B2.i16[6],
                                    aFactorsA1B1.i16[7] * aFactorsA2B2.i16[7]);
}

inline Scalari32x4_t MulAdd16x8x2To32x4(Scalari16x8_t aFactorsA,
                                        Scalari16x8_t aFactorsB)
{
  return From32<Scalari32x4_t>(aFactorsA.i16[0] * aFactorsB.i16[0] + aFactorsA.i16[1] * aFactorsB.i16[1],
                               aFactorsA.i16[2] * aFactorsB.i16[2] + aFactorsA.i16[3] * aFactorsB.i16[3],
                               aFactorsA.i16[4] * aFactorsB.i16[4] + aFactorsA.i16[5] * aFactorsB.i16[5],
                               aFactorsA.i16[6] * aFactorsB.i16[6] + aFactorsA.i16[7] * aFactorsB.i16[7]);
}

template<int8_t aIndex>
inline void AssertIndex()
{
  static_assert(aIndex == 0 || aIndex == 1 || aIndex == 2 || aIndex == 3,
                "Invalid splat index");
}

template<int8_t aIndex>
inline Scalari32x4_t Splat32(Scalari32x4_t aM)
{
  AssertIndex<aIndex>();
  return From32<Scalari32x4_t>(aM.i32[aIndex], aM.i32[aIndex],
                               aM.i32[aIndex], aM.i32[aIndex]);
}

template<int8_t i>
inline Scalaru8x16_t Splat32On8(Scalaru8x16_t aM)
{
  AssertIndex<i>();
  return From8<Scalaru8x16_t>(aM.u8[i*4], aM.u8[i*4+1], aM.u8[i*4+2], aM.u8[i*4+3],
                              aM.u8[i*4], aM.u8[i*4+1], aM.u8[i*4+2], aM.u8[i*4+3],
                              aM.u8[i*4], aM.u8[i*4+1], aM.u8[i*4+2], aM.u8[i*4+3],
                              aM.u8[i*4], aM.u8[i*4+1], aM.u8[i*4+2], aM.u8[i*4+3]);
}

template<int8_t i0, int8_t i1, int8_t i2, int8_t i3>
inline Scalari32x4_t Shuffle32(Scalari32x4_t aM)
{
  AssertIndex<i0>();
  AssertIndex<i1>();
  AssertIndex<i2>();
  AssertIndex<i3>();
  Scalari32x4_t m = aM;
  m.i32[0] = aM.i32[i3];
  m.i32[1] = aM.i32[i2];
  m.i32[2] = aM.i32[i1];
  m.i32[3] = aM.i32[i0];
  return m;
}

template<int8_t i0, int8_t i1, int8_t i2, int8_t i3>
inline Scalari16x8_t ShuffleLo16(Scalari16x8_t aM)
{
  AssertIndex<i0>();
  AssertIndex<i1>();
  AssertIndex<i2>();
  AssertIndex<i3>();
  Scalari16x8_t m = aM;
  m.i16[0] = aM.i16[i3];
  m.i16[1] = aM.i16[i2];
  m.i16[2] = aM.i16[i1];
  m.i16[3] = aM.i16[i0];
  return m;
}

template<int8_t i0, int8_t i1, int8_t i2, int8_t i3>
inline Scalari16x8_t ShuffleHi16(Scalari16x8_t aM)
{
  AssertIndex<i0>();
  AssertIndex<i1>();
  AssertIndex<i2>();
  AssertIndex<i3>();
  Scalari16x8_t m = aM;
  m.i16[4 + 0] = aM.i16[4 + i3];
  m.i16[4 + 1] = aM.i16[4 + i2];
  m.i16[4 + 2] = aM.i16[4 + i1];
  m.i16[4 + 3] = aM.i16[4 + i0];
  return m;
}

template<int8_t aIndexLo, int8_t aIndexHi>
inline Scalari16x8_t Splat16(Scalari16x8_t aM)
{
  AssertIndex<aIndexLo>();
  AssertIndex<aIndexHi>();
  Scalari16x8_t m;
  int16_t chosenValueLo = aM.i16[aIndexLo];
  m.i16[0] = chosenValueLo;
  m.i16[1] = chosenValueLo;
  m.i16[2] = chosenValueLo;
  m.i16[3] = chosenValueLo;
  int16_t chosenValueHi = aM.i16[4 + aIndexHi];
  m.i16[4] = chosenValueHi;
  m.i16[5] = chosenValueHi;
  m.i16[6] = chosenValueHi;
  m.i16[7] = chosenValueHi;
  return m;
}

inline Scalaru8x16_t
InterleaveLo8(Scalaru8x16_t m1, Scalaru8x16_t m2)
{
  return From8<Scalaru8x16_t>(m1.u8[0], m2.u8[0], m1.u8[1], m2.u8[1],
                              m1.u8[2], m2.u8[2], m1.u8[3], m2.u8[3],
                              m1.u8[4], m2.u8[4], m1.u8[5], m2.u8[5],
                              m1.u8[6], m2.u8[6], m1.u8[7], m2.u8[7]);
}

inline Scalaru8x16_t
InterleaveHi8(Scalaru8x16_t m1, Scalaru8x16_t m2)
{
  return From8<Scalaru8x16_t>(m1.u8[8+0], m2.u8[8+0], m1.u8[8+1], m2.u8[8+1],
                              m1.u8[8+2], m2.u8[8+2], m1.u8[8+3], m2.u8[8+3],
                              m1.u8[8+4], m2.u8[8+4], m1.u8[8+5], m2.u8[8+5],
                              m1.u8[8+6], m2.u8[8+6], m1.u8[8+7], m2.u8[8+7]);
}

inline Scalari16x8_t
InterleaveLo16(Scalari16x8_t m1, Scalari16x8_t m2)
{
  return From16<Scalari16x8_t>(m1.i16[0], m2.i16[0], m1.i16[1], m2.i16[1],
                               m1.i16[2], m2.i16[2], m1.i16[3], m2.i16[3]);
}

inline Scalari16x8_t
InterleaveHi16(Scalari16x8_t m1, Scalari16x8_t m2)
{
  return From16<Scalari16x8_t>(m1.i16[4], m2.i16[4], m1.i16[5], m2.i16[5],
                               m1.i16[6], m2.i16[6], m1.i16[7], m2.i16[7]);
}

inline Scalari32x4_t
InterleaveLo32(Scalari32x4_t m1, Scalari32x4_t m2)
{
  return From32<Scalari32x4_t>(m1.i32[0], m2.i32[0], m1.i32[1], m2.i32[1]);
}

inline Scalari16x8_t
UnpackLo8x8To16x8(Scalaru8x16_t aM)
{
  Scalari16x8_t m;
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

inline Scalari16x8_t
UnpackHi8x8To16x8(Scalaru8x16_t aM)
{
  Scalari16x8_t m;
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

inline Scalari16x8_t
GetMiddleTwo16From8(Scalaru8x16_t aM)
{
  Scalari16x8_t m;
  m.i16[0] = aM.u8[4];
  m.i16[1] = aM.u8[5];
  m.i16[2] = aM.u8[6];
  m.i16[3] = aM.u8[7];
  m.i16[4] = aM.u8[8];
  m.i16[5] = aM.u8[9];
  m.i16[6] = aM.u8[10];
  m.i16[7] = aM.u8[11];
  return m;
}

inline Scalari16x8_t
GetOverlappingTwo16From8(Scalaru8x16_t aM1, Scalaru8x16_t aM2)
{
  Scalari16x8_t m;
  m.i16[0] = aM1.u8[12];
  m.i16[1] = aM1.u8[13];
  m.i16[2] = aM1.u8[14];
  m.i16[3] = aM1.u8[15];
  m.i16[4] = aM2.u8[0];
  m.i16[5] = aM2.u8[1];
  m.i16[6] = aM2.u8[2];
  m.i16[7] = aM2.u8[3];
  return m;
}

template<typename T>
inline int16_t
SaturateTo16(T a)
{
  return int16_t(a >= INT16_MIN ? (a <= INT16_MAX ? a : INT16_MAX) : INT16_MIN);
}

inline Scalari16x8_t
PackAndSaturate32To16(Scalari32x4_t m1, Scalari32x4_t m2)
{
  Scalari16x8_t m;
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
inline uint8_t
SaturateTo8(T a)
{
  return uint8_t(a >= 0 ? (a <= 255 ? a : 255) : 0);
}

inline Scalaru8x16_t
PackAndSaturate32To8(Scalari32x4_t m1, Scalari32x4_t m2, Scalari32x4_t m3, const Scalari32x4_t& m4)
{
  Scalaru8x16_t m;
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

inline Scalaru8x16_t
PackAndSaturate16To8(Scalari16x8_t m1, Scalari16x8_t m2)
{
  Scalaru8x16_t m;
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
inline Scalari16x8_t
SetComponent16(Scalari16x8_t aM, int16_t aValue)
{
  Scalari16x8_t m = aM;
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
inline B FastDivideBy255(A v)
{
  return ((v << 8) + v + 255) >> 16;
}

inline Scalari16x8_t
FastDivideBy255_16(Scalari16x8_t m)
{
  return From16<Scalari16x8_t>(FastDivideBy255<uint16_t>(uint16_t(m.i16[0])),
                               FastDivideBy255<uint16_t>(uint16_t(m.i16[1])),
                               FastDivideBy255<uint16_t>(uint16_t(m.i16[2])),
                               FastDivideBy255<uint16_t>(uint16_t(m.i16[3])),
                               FastDivideBy255<uint16_t>(uint16_t(m.i16[4])),
                               FastDivideBy255<uint16_t>(uint16_t(m.i16[5])),
                               FastDivideBy255<uint16_t>(uint16_t(m.i16[6])),
                               FastDivideBy255<uint16_t>(uint16_t(m.i16[7])));
}

inline Scalari32x4_t
FastDivideBy255(Scalari32x4_t m)
{
  return From32<Scalari32x4_t>(FastDivideBy255<int32_t>(m.i32[0]),
                               FastDivideBy255<int32_t>(m.i32[1]),
                               FastDivideBy255<int32_t>(m.i32[2]),
                               FastDivideBy255<int32_t>(m.i32[3]));
}

#ifdef SIMD_COMPILE_SSE2

// SSE2

template<>
inline __m128i
Load8<__m128i>(const uint8_t* aSource)
{
  return _mm_load_si128((const __m128i*)aSource);
}

inline void Store8(uint8_t* aTarget, __m128i aM)
{
  _mm_store_si128((__m128i*)aTarget, aM);
}

template<>
inline __m128i FromZero8<__m128i>()
{
  return _mm_setzero_si128();
}

template<>
inline __m128i From16<__m128i>(int16_t a, int16_t b, int16_t c, int16_t d, int16_t e, int16_t f, int16_t g, int16_t h)
{
  return _mm_setr_epi16(a, b, c, d, e, f, g, h);
}

template<>
inline __m128i From16<__m128i>(int16_t a)
{
  return _mm_set1_epi16(a);
}

template<>
inline __m128i From32<__m128i>(int32_t a, int32_t b, int32_t c, int32_t d)
{
  return _mm_setr_epi32(a, b, c, d);
}

template<>
inline __m128i From32<__m128i>(int32_t a)
{
  return _mm_set1_epi32(a);
}

template<int32_t aNumberOfBits>
inline __m128i ShiftRight16(__m128i aM)
{
  return _mm_srli_epi16(aM, aNumberOfBits);
}

template<int32_t aNumberOfBits>
inline __m128i ShiftRight32(__m128i aM)
{
  return _mm_srai_epi32(aM, aNumberOfBits);
}

inline __m128i Add16(__m128i aM1, __m128i aM2)
{
  return _mm_add_epi16(aM1, aM2);
}

inline __m128i Add32(__m128i aM1, __m128i aM2)
{
  return _mm_add_epi32(aM1, aM2);
}

inline __m128i Sub16(__m128i aM1, __m128i aM2)
{
  return _mm_sub_epi16(aM1, aM2);
}

inline __m128i Sub32(__m128i aM1, __m128i aM2)
{
  return _mm_sub_epi32(aM1, aM2);
}

inline __m128i Min16(__m128i aM1, __m128i aM2)
{
  __m128i m1_minus_m2 = _mm_sub_epi16(aM1, aM2);
  __m128i m1_greater_than_m2 = _mm_cmpgt_epi16(aM1, aM2);
  return _mm_sub_epi16(aM1, _mm_and_si128(m1_minus_m2, m1_greater_than_m2));
}

inline __m128i Max16(__m128i aM1, __m128i aM2)
{
  __m128i m1_minus_m2 = _mm_sub_epi16(aM1, aM2);
  __m128i m2_greater_than_m1 = _mm_cmpgt_epi16(aM2, aM1);
  return _mm_sub_epi16(aM1, _mm_and_si128(m1_minus_m2, m2_greater_than_m1));
}

inline __m128i Min32(__m128i aM1, __m128i aM2)
{
  __m128i m1_minus_m2 = _mm_sub_epi32(aM1, aM2);
  __m128i m1_greater_than_m2 = _mm_cmpgt_epi32(aM1, aM2);
  return _mm_sub_epi32(aM1, _mm_and_si128(m1_minus_m2, m1_greater_than_m2));
}

inline __m128i Max32(__m128i aM1, __m128i aM2)
{
  __m128i m1_minus_m2 = _mm_sub_epi32(aM1, aM2);
  __m128i m2_greater_than_m1 = _mm_cmpgt_epi32(aM2, aM1);
  return _mm_sub_epi32(aM1, _mm_and_si128(m1_minus_m2, m2_greater_than_m1));
}

inline __m128i Mul16(__m128i aM1, __m128i aM2)
{
  return _mm_mullo_epi16(aM1, aM2);
}

inline void Mul16x4x2x2To32x4x2(__m128i aFactorsA1B1,
                                __m128i aFactorsA2B2,
                                __m128i& aProductA,
                                __m128i& aProductB)
{
  __m128i prodAB_lo = _mm_mullo_epi16(aFactorsA1B1, aFactorsA2B2);
  __m128i prodAB_hi = _mm_mulhi_epi16(aFactorsA1B1, aFactorsA2B2);
  aProductA = _mm_unpacklo_epi16(prodAB_lo, prodAB_hi);
  aProductB = _mm_unpackhi_epi16(prodAB_lo, prodAB_hi);
}

inline __m128i MulAdd16x8x2To32x4(__m128i aFactorsA,
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
inline __m128i Splat32On8(__m128i aM)
{
  return Shuffle32<aIndex,aIndex,aIndex,aIndex>(aM);
}

template<int8_t aIndexLo, int8_t aIndexHi>
inline __m128i Splat16(__m128i aM)
{
  AssertIndex<aIndexLo>();
  AssertIndex<aIndexHi>();
  return ShuffleHi16<aIndexHi,aIndexHi,aIndexHi,aIndexHi>(
           ShuffleLo16<aIndexLo,aIndexLo,aIndexLo,aIndexLo>(aM));
}

inline __m128i
UnpackLo8x8To16x8(__m128i m)
{
  __m128i zero = _mm_set1_epi8(0);
  return _mm_unpacklo_epi8(m, zero);
}

inline __m128i
UnpackHi8x8To16x8(__m128i m)
{
  __m128i zero = _mm_set1_epi8(0);
  return _mm_unpackhi_epi8(m, zero);
}

inline __m128i
InterleaveLo8(__m128i m1, __m128i m2)
{
  return _mm_unpacklo_epi8(m1, m2);
}

inline __m128i
InterleaveHi8(__m128i m1, __m128i m2)
{
  return _mm_unpackhi_epi8(m1, m2);
}

inline __m128i
InterleaveLo16(__m128i m1, __m128i m2)
{
  return _mm_unpacklo_epi16(m1, m2);
}

inline __m128i
InterleaveHi16(__m128i m1, __m128i m2)
{
  return _mm_unpackhi_epi16(m1, m2);
}

inline __m128i
InterleaveLo32(__m128i m1, __m128i m2)
{
  return _mm_unpacklo_epi32(m1, m2);
}

inline __m128i
GetMiddleTwo16From8(__m128i aM)
{
  return UnpackLo8x8To16x8(Shuffle32<0,3,2,1>(aM));
}

inline __m128i
GetOverlappingTwo16From8(__m128i aM1, __m128i aM2)
{
  __m128i p4123 = Shuffle32<2,1,0,3>(aM1);
  __m128i p4516 = InterleaveLo32(p4123, aM2);
  return UnpackLo8x8To16x8(p4516);
}

inline __m128i
PackAndSaturate32To16(__m128i m1, __m128i m2)
{
  return _mm_packs_epi32(m1, m2);
}

inline __m128i
PackAndSaturate32To8(__m128i m1, __m128i m2, __m128i m3, const __m128i& m4)
{
  // Pack into 8 16bit signed integers (saturating).
  __m128i m12 = _mm_packs_epi32(m1, m2);
  __m128i m34 = _mm_packs_epi32(m3, m4);

  // Pack into 16 8bit unsigned integers (saturating).
  return _mm_packus_epi16(m12, m34);
}

inline __m128i
PackAndSaturate16To8(__m128i m1, __m128i m2)
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

#endif // SIMD_COMPILE_SSE2

} // namespace simd

} // namespace gfx
} // namespace mozilla

#endif // _MOZILLA_GFX_SIMD_H_
