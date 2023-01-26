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
using r128 = __m128i;

// Calling convention setting, yay
#define RETURNS_R128 r128 __vectorcall
#define TAKES_R128 __vectorcall

// And since we can't stick them in structs, we get lots of static methods, yay!
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
__forceinline static r128 r128_from_u64_dup(u64 val)
{
	return _mm_set1_epi64x(val);
}
__forceinline static r128 r128_from_u64_zext(u64 val)
{
	return _mm_set_epi64x(0, val);
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
