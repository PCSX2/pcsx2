/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2023  PCSX2 Dev Team
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
