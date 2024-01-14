// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

// --------------------------------------------------------------------------------------
//  r64 / r128 - Types that are guaranteed to fit in one register
// --------------------------------------------------------------------------------------
// Note: Recompilers rely on some of these types and the registers they allocate to,
// so be careful if you want to change them

#pragma once

#include "Pcsx2Defs.h"
#include "Pcsx2Types.h"
#include "VectorIntrin.h"

#include <cstring>

#if defined(_M_X86)

// Can't stick them in structs because it breaks calling convention things, yay
using r128 = __m128i;

// Calling convention setting, yay
#define RETURNS_R128 r128 __vectorcall
#define TAKES_R128 __vectorcall

// And since we can't stick them in structs, we get lots of static methods, yay!
[[maybe_unused]] __fi static r128 r128_load(const void* ptr)
{
	return _mm_load_si128(reinterpret_cast<const r128*>(ptr));
}

[[maybe_unused]] __fi static void r128_store(void* ptr, r128 val)
{
	return _mm_store_si128(reinterpret_cast<r128*>(ptr), val);
}

[[maybe_unused]] __fi static void r128_store_unaligned(void* ptr, r128 val)
{
	return _mm_storeu_si128(reinterpret_cast<r128*>(ptr), val);
}

[[maybe_unused]] __fi static r128 r128_zero()
{
	return _mm_setzero_si128();
}

/// Expects that r64 came from r64-handling code, and not from a recompiler or something
[[maybe_unused]] __fi static r128 r128_from_u64_dup(u64 val)
{
	return _mm_set1_epi64x(val);
}
[[maybe_unused]] __fi static r128 r128_from_u64_zext(u64 val)
{
	return _mm_set_epi64x(0, val);
}

[[maybe_unused]] __fi static r128 r128_from_u32_dup(u32 val)
{
	return _mm_set1_epi32(val);
}

[[maybe_unused]] __fi static r128 r128_from_u32x4(u32 lo0, u32 lo1, u32 hi0, u32 hi1)
{
	return _mm_setr_epi32(lo0, lo1, hi0, hi1);
}

[[maybe_unused]] __fi static r128 r128_from_u128(const u128& u)
{
	return _mm_loadu_si128(reinterpret_cast<const __m128i*>(&u));
}

[[maybe_unused]] __fi static u32 r128_to_u32(r128 val)
{
	return _mm_cvtsi128_si32(val);
}

[[maybe_unused]] __fi static u64 r128_to_u64(r128 val)
{
	return _mm_cvtsi128_si64(val);
}

[[maybe_unused]] __fi static u128 r128_to_u128(r128 val)
{
	alignas(16) u128 ret;
	_mm_store_si128(reinterpret_cast<r128*>(&ret), val);
	return ret;
}

[[maybe_unused]] __fi static void CopyQWC(void* dest, const void* src)
{
	_mm_store_ps((float*)dest, _mm_load_ps((const float*)src));
}

[[maybe_unused]] __fi static void ZeroQWC(void* dest)
{
	_mm_store_ps((float*)dest, _mm_setzero_ps());
}

[[maybe_unused]] __fi static void ZeroQWC(u128& dest)
{
	_mm_store_ps((float*)&dest, _mm_setzero_ps());
}

#elif defined(_M_ARM64)

using r128 = uint32x4_t;

#define RETURNS_R128 r128 __vectorcall
#define TAKES_R128 __vectorcall

[[maybe_unused]] __fi static void CopyQWC(void* dest, const void* src)
{
	vst1q_u8(static_cast<u8*>(dest), vld1q_u8(static_cast<const u8*>(src)));
}

[[maybe_unused]] __fi static void ZeroQWC(void* dest)
{
	vst1q_u8(static_cast<u8*>(dest), vmovq_n_u8(0));
}

[[maybe_unused]] __fi static void ZeroQWC(u128& dest)
{
	vst1q_u8(&dest._u8[0], vmovq_n_u8(0));
}


[[maybe_unused]] __fi static r128 r128_load(const void* ptr)
{
	return vld1q_u32(reinterpret_cast<const uint32_t*>(ptr));
}

[[maybe_unused]] __fi static void r128_store(void* ptr, r128 value)
{
	return vst1q_u32(reinterpret_cast<uint32_t*>(ptr), value);
}

[[maybe_unused]] __fi static void r128_store_unaligned(void* ptr, r128 value)
{
	return vst1q_u32(reinterpret_cast<uint32_t*>(ptr), value);
}

[[maybe_unused]] __fi static r128 r128_zero()
{
	return vmovq_n_u32(0);
}

/// Expects that r64 came from r64-handling code, and not from a recompiler or something
[[maybe_unused]] __fi static r128 r128_from_u64_dup(u64 val)
{
	return vreinterpretq_u32_u64(vdupq_n_u64(val));
}
[[maybe_unused]] __fi static r128 r128_from_u64_zext(u64 val)
{
	return vreinterpretq_u32_u64(vcombine_u64(vcreate_u64(val), vcreate_u64(0)));
}

[[maybe_unused]] __fi static r128 r128_from_u32_dup(u32 val)
{
	return vdupq_n_u32(val);
}

[[maybe_unused]] __fi static r128 r128_from_u32x4(u32 lo0, u32 lo1, u32 hi0, u32 hi1)
{
	const u32 values[4] = {lo0, lo1, hi0, hi1};
	return vld1q_u32(values);
}

[[maybe_unused]] __fi static r128 r128_from_u128(const u128& u)
{
	return vld1q_u32(reinterpret_cast<const uint32_t*>(u._u32));
}

[[maybe_unused]] __fi static u32 r128_to_u32(r128 val)
{
	return vgetq_lane_u32(val, 0);
}

[[maybe_unused]] __fi static u64 r128_to_u64(r128 val)
{
	return vgetq_lane_u64(vreinterpretq_u64_u32(val), 0);
}

[[maybe_unused]] __fi static u128 r128_to_u128(r128 val)
{
	alignas(16) u128 ret;
	vst1q_u32(ret._u32, val);
	return ret;
}

#else

#error Unknown architecture.

#endif
