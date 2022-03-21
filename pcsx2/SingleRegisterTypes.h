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

#include <immintrin.h>
// Can't stick them in structs because it breaks calling convention things, yay
using r64  = __m128i;
using r128 = __m128i;

// Calling convention setting, yay
#define RETURNS_R64  r64  __vectorcall
#define RETURNS_R128 r128 __vectorcall
#define TAKES_R64  __vectorcall
#define TAKES_R128 __vectorcall

// And since we can't stick them in structs, we get lots of static methods, yay!

__forceinline static r64 r64_load(const void* ptr)
{
	return _mm_loadl_epi64(reinterpret_cast<const r64*>(ptr));
}

__forceinline static r64 r64_zero()
{
	return _mm_setzero_si128();
}

__forceinline static r64 r64_from_u32(u32 val)
{
	return _mm_cvtsi32_si128(val);
}

__forceinline static r64 r64_from_u32x2(u32 lo, u32 hi)
{
	return _mm_unpacklo_epi32(_mm_cvtsi32_si128(lo), _mm_cvtsi32_si128(hi));
}

__forceinline static r64 r64_from_u64(u64 val)
{
	return _mm_cvtsi64_si128(val);
}

__forceinline static r128 r128_load(const void* ptr)
{
	return _mm_load_si128(reinterpret_cast<const r128*>(ptr));
}

__forceinline static r128 r128_zero()
{
	return _mm_setzero_si128();
}

/// Expects that r64 came from r64-handling code, and not from a recompiler or something
__forceinline static r128 r128_from_r64_clean(r64 val)
{
	return val;
}

__forceinline static r128 r128_from_u32x4(u32 lo0, u32 lo1, u32 hi0, u32 hi1)
{
	return _mm_setr_epi32(lo0, lo1, hi0, hi1);
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
