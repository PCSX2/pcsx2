// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

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
	static constexpr s8 msb[256] = {
		-1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
		5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};

	static constexpr s32 debruijn32[64] = {
		32, 8, 17, -1, -1, 14, -1, -1, -1, 20, -1, -1, -1, 28, -1, 18,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 26, 25, 24,
		4, 11, 23, 31, 3, 7, 10, 16, 22, 30, -1, -1, 2, 6, 13, 9,
		-1, 15, -1, 21, -1, 29, 19, -1, -1, -1, -1, -1, 1, 27, 5, 12};

	static constexpr s32 normalizeAmounts[] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 8, 8, 8, 8, 8, 8, 8, 16, 16, 16, 16, 16, 16, 16, 16, 24, 24, 24, 24, 24, 24, 24};

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

	__fi static s32 clz(s32 x)
	{
		x |= x >> 1;
		x |= x >> 2;
		x |= x >> 4;
		x |= x >> 8;
		x |= x >> 16;

		return debruijn32[(u32)x * 0x8c0b2891u >> 26];
	}

	__fi static s32 BitScanReverse8(s32 b)
	{
		return msb[b];
	}

	__fi static s32 GetMostSignificantBitPosition(u32 value)
	{
		for (s32 i = 31; i >= 0; i--)
		{
			if (((value >> i) & 1) != 0)
				return i;
		}
		return -1;
	}
} // namespace Common

template <typename T>
[[maybe_unused]] __fi static T GetBufferT(const u8* buffer, u32 offset)
{
	T value;
	std::memcpy(&value, buffer + offset, sizeof(value));
	return value;
}

[[maybe_unused]] __fi static u8 GetBufferU8(const u8* buffer, u32 offset) { return GetBufferT<u8>(buffer, offset); }
[[maybe_unused]] __fi static u16 GetBufferU16(const u8* buffer, u32 offset) { return GetBufferT<u16>(buffer, offset); }
[[maybe_unused]] __fi static u32 GetBufferU32(const u8* buffer, u32 offset) { return GetBufferT<u32>(buffer, offset); }
[[maybe_unused]] __fi static u64 GetBufferU64(const u8* buffer, u32 offset) { return GetBufferT<u64>(buffer, offset); }
