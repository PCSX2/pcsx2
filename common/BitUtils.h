/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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

#include "common/Pcsx2Defs.h"

#include <bit>

#ifdef _MSC_VER

#include <intrin.h>

#else

static inline int _BitScanReverse(unsigned long* const Index, const unsigned long Mask)
{
	if (Mask == 0)
		return 0;

	// For some reason, clang won't emit bsr if we use std::countl_zeros()...
	*Index = 31 - __builtin_clz(Mask);
	return 1;
}

#endif

namespace Common
{
	template <typename T>
	static constexpr __fi bool IsAligned(T value, unsigned int alignment)
	{
		return (value % static_cast<T>(alignment)) == 0;
	}

	template <typename T>
	static constexpr __fi T AlignUp(T value, unsigned int alignment)
	{
		return (value + static_cast<T>(alignment - 1)) / static_cast<T>(alignment) * static_cast<T>(alignment);
	}

	template <typename T>
	static constexpr __fi T AlignDown(T value, unsigned int alignment)
	{
		return value / static_cast<T>(alignment) * static_cast<T>(alignment);
	}

	template <typename T>
	static constexpr __fi bool IsAlignedPow2(T value, unsigned int alignment)
	{
		return (value & static_cast<T>(alignment - 1)) == 0;
	}

	template <typename T>
	static constexpr __fi T AlignUpPow2(T value, unsigned int alignment)
	{
		return (value + static_cast<T>(alignment - 1)) & static_cast<T>(~static_cast<T>(alignment - 1));
	}

	template <typename T>
	static constexpr __fi T AlignDownPow2(T value, unsigned int alignment)
	{
		return value & static_cast<T>(~static_cast<T>(alignment - 1));
	}

	template <typename T>
	static constexpr T PageAlign(T size)
	{
		static_assert(std::has_single_bit(__pagesize), "Page size is a power of 2");
		return Common::AlignUpPow2(size, __pagesize);
	}

	__fi static u32 CountLeadingSignBits(s32 n)
	{
		// If the sign bit is 1, we invert the bits to 0 for count-leading-zero.
		if (n < 0)
			n = ~n;

		// If BSR is used directly, it would have an undefined value for 0.
		if (n == 0)
			return 32;

		// Perform our count leading zero.
		return std::countl_zero(static_cast<u32>(n));
	}
} // namespace Common
