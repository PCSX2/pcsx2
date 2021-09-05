/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2014-  PCSX2 Dev Team
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
// Hopefully this file will be used for cross-source math utilities.
// Currently these are strewn across the code base. Please collect them all!

#include "common/Pcsx2Defs.h"

// On GCC >= 4.7, this is equivalent to __builtin_clrsb(n);
inline u32 count_leading_sign_bits(s32 n)
{
	// If the sign bit is 1, we invert the bits to 0 for count-leading-zero.
	if (n < 0)
		n = ~n;

	// If BSR is used directly, it would have an undefined value for 0.
	if (n == 0)
		return 32;

// Perform our count leading zero.
#ifdef _MSC_VER
	unsigned long ret;
	_BitScanReverse(&ret, n);
	return 31 - (u32)ret;
#else
	return __builtin_clz(n);
#endif
}
