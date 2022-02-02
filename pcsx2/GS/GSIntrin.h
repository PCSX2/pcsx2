/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#pragma once

#include <xmmintrin.h>
#include <emmintrin.h>

#include <tmmintrin.h>
#include <smmintrin.h>

#if _M_SSE >= 0x500
	#include <immintrin.h>
#endif

#if !defined(_MSC_VER)
// http://svn.reactos.org/svn/reactos/trunk/reactos/include/crt/mingw32/intrin_x86.h?view=markup

static inline int _BitScanForward(unsigned long* const Index, const unsigned long Mask)
{
	if (Mask == 0)
		return 0;
#if __has_builtin(__builtin_ctz)
	*Index = __builtin_ctz(Mask);
#else
	__asm__("bsfl %k[Mask], %k[Index]" : [Index] "=r" (*Index) : [Mask] "mr" (Mask) : "cc");
#endif
	return 1;
}

static inline int _BitScanReverse(unsigned long* const Index, const unsigned long Mask)
{
	if (Mask == 0)
		return 0;
#if __has_builtin(__builtin_clz)
	*Index = 31 - __builtin_clz(Mask);
#else
	__asm__("bsrl %k[Mask], %k[Index]" : [Index] "=r" (*Index) : [Mask] "mr" (Mask) : "cc");
#endif
	return 1;
}

#endif
