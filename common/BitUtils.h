// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#include <bit>
#include <cstring>

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

template <typename T>
[[maybe_unused]] __fi static T GetBufferT(u8* buffer, u32 offset)
{
	T value;
	std::memcpy(&value, buffer + offset, sizeof(value));
	return value;
}

[[maybe_unused]] __fi static u8 GetBufferU8(u8* buffer, u32 offset) { return GetBufferT<u8>(buffer, offset); }
[[maybe_unused]] __fi static u16 GetBufferU16(u8* buffer, u32 offset) { return GetBufferT<u16>(buffer, offset); }
[[maybe_unused]] __fi static u32 GetBufferU32(u8* buffer, u32 offset) { return GetBufferT<u32>(buffer, offset); }
[[maybe_unused]] __fi static u64 GetBufferU64(u8* buffer, u32 offset) { return GetBufferT<u64>(buffer, offset); }
