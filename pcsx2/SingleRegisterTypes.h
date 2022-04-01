/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

// --------------------------------------------------------------------------------------
//  r64 / r128 - Types that are guaranteed to fit in one register
// --------------------------------------------------------------------------------------
// Note: Recompilers rely on some of these types and the registers they allocate to,
// so be careful if you want to change them

#pragma once

#include <cstring>
#include <immintrin.h>
#include <emmintrin.h>

// Can't stick them in structs because it breaks calling convention things, yay
using r64 = u64;
using r128 = __m128i;

// Calling convention setting, yay
#define RETURNS_R64  r64
#define RETURNS_R128 r128 __vectorcall
#define TAKES_R64
#define TAKES_R128 __vectorcall

// And since we can't stick them in structs, we get lots of static methods, yay!

__forceinline static r64 r64_load(const void* ptr)
{
	r64 ret;
	std::memcpy(&ret, ptr, sizeof(ret));
	return ret;
}

__forceinline static void r64_store(void* ptr, r64 val)
{
	std::memcpy(ptr, &val, sizeof(val));
}

__forceinline static r64 r64_zero()
{
	return 0;
}

__forceinline static r64 r64_from_u32(u32 val)
{
	return static_cast<u64>(val);
}

__forceinline static r64 r64_from_u32x2(u32 lo, u32 hi)
{
	return (static_cast<u64>(hi) << 32) | static_cast<u64>(lo);
}

__forceinline static r64 r64_from_u64(u64 val)
{
	return val;
}

__forceinline static u32 r64_to_u32(r64 val)
{
	return static_cast<u32>(val);
}

__forceinline static u32 r64_to_u32_hi(r64 val)
{
	return static_cast<u32>(val >> 32);
}

__forceinline static u64 r64_to_u64(r64 val)
{
	return val;
}

__forceinline static r128 r128_load(const void* ptr)
{
	return _mm_load_si128(reinterpret_cast<const r128*>(ptr));
}

__forceinline static void r128_store(void* ptr, r128 val)
{
	return _mm_store_si128(reinterpret_cast<r128*>(ptr), val);
}

__forceinline static void r128_store_unaligned(void* ptr, r128 val)
{
	return _mm_storeu_si128(reinterpret_cast<r128*>(ptr), val);
}

__forceinline static r128 r128_zero()
{
	return _mm_setzero_si128();
}

/// Expects that r64 came from r64-handling code, and not from a recompiler or something
__forceinline static r128 r128_from_r64_clean(r64 val)
{
	return _mm_set1_epi64x(val);
}

__forceinline static r128 r128_from_u32x4(u32 lo0, u32 lo1, u32 hi0, u32 hi1)
{
	return _mm_setr_epi32(lo0, lo1, hi0, hi1);
}

__forceinline static r128 r128_from_u128(const u128& u)
{
	return _mm_loadu_si128(reinterpret_cast<const __m128i*>(&u));
}

__forceinline static u32 r128_to_u32(r128 val)
{
	return _mm_cvtsi128_si32(val);
}

__forceinline static u64 r128_to_u64(r128 val)
{
	return _mm_cvtsi128_si64(val);
}

__forceinline static u128 r128_to_u128(r128 val)
{
	alignas(16) u128 ret;
	_mm_store_si128(reinterpret_cast<r128*>(&ret), val);
	return ret;
}

template <typename u>
struct rhelper;

template <>
struct rhelper<u64>
{
	using r = r64;
	__forceinline static r load(void* ptr) { return r64_load(ptr); }
	__forceinline static r zero() { return r64_zero(); }
};

template <>
struct rhelper<u128>
{
	using r = r128;
	__forceinline static r load(void* ptr) { return r128_load(ptr); }
	__forceinline static r zero() { return r128_zero(); }
};

template <typename u>
using u_to_r = typename rhelper<u>::r;
