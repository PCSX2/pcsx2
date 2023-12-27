// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <cstdint>
#include <type_traits>

#ifdef _MSC_VER
#include <stdlib.h>
#endif

template <typename T>
T ByteSwap(T value)
{
	if constexpr (std::is_signed_v<T>)
	{
		return static_cast<T>(ByteSwap(std::make_unsigned_t<T>(value)));
	}
	else if constexpr (std::is_same_v<T, std::uint16_t>)
	{
#ifdef _MSC_VER
		return _byteswap_ushort(value);
#else
		return __builtin_bswap16(value);
#endif
	}
	else if constexpr (std::is_same_v<T, std::uint32_t>)
	{
#ifdef _MSC_VER
		return _byteswap_ulong(value);
#else
		return __builtin_bswap32(value);
#endif
	}
	else if constexpr (std::is_same_v<T, std::uint64_t>)
	{
#ifdef _MSC_VER
		return _byteswap_uint64(value);
#else
		return __builtin_bswap64(value);
#endif
	}
}
