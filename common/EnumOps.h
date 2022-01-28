/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022 PCSX2 Dev Team
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

#include <type_traits>

namespace detail
{
	/// Marks an enum as supporting boolean operators
	template <typename T>
	struct enum_is_flags : public std::false_type {};

	/// For return types that should be convertible to bool
	template <typename Enum>
	struct enum_bool_helper
	{
		Enum value;
		constexpr enum_bool_helper(Enum value): value(value) {}
		constexpr operator Enum() const { return value; }
		constexpr operator bool() const { return static_cast<bool>(static_cast<typename std::underlying_type<Enum>::type>(value)); }
	};
};

#define MARK_ENUM_AS_FLAGS(T) template<> struct detail::enum_is_flags<T> : public std::true_type {}

template <typename Enum>
constexpr typename std::enable_if<detail::enum_is_flags<Enum>::value, Enum>::type
operator|(Enum lhs, Enum rhs) noexcept
{
	using underlying = typename std::underlying_type<Enum>::type;
	return static_cast<Enum>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

template <typename Enum>
constexpr typename std::enable_if<detail::enum_is_flags<Enum>::value, detail::enum_bool_helper<Enum>>::type
operator&(Enum lhs, Enum rhs) noexcept
{
	using underlying = typename std::underlying_type<Enum>::type;
	return static_cast<Enum>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}

template <typename Enum>
constexpr typename std::enable_if<detail::enum_is_flags<Enum>::value, Enum>::type
operator^(Enum lhs, Enum rhs) noexcept
{
	using underlying = typename std::underlying_type<Enum>::type;
	return static_cast<Enum>(static_cast<underlying>(lhs) ^ static_cast<underlying>(rhs));
}

template <typename Enum>
constexpr typename std::enable_if<detail::enum_is_flags<Enum>::value, Enum&>::type
operator|=(Enum& lhs, Enum rhs) noexcept
{
	return lhs = lhs | rhs;
}

template <typename Enum>
constexpr typename std::enable_if<detail::enum_is_flags<Enum>::value, Enum&>::type
operator&=(Enum& lhs, Enum rhs) noexcept
{
	return lhs = lhs & rhs;
}

template <typename Enum>
constexpr typename std::enable_if<detail::enum_is_flags<Enum>::value, Enum&>::type
operator^=(Enum& lhs, Enum rhs) noexcept
{
	return lhs = lhs ^ rhs;
}

template<typename Enum>
constexpr typename std::enable_if<detail::enum_is_flags<Enum>::value, bool>::type
operator!(Enum e) noexcept
{
	return !static_cast<typename std::underlying_type<Enum>::type>(e);
}

template<typename Enum>
constexpr typename std::enable_if<detail::enum_is_flags<Enum>::value, Enum>::type
operator~(Enum e) noexcept
{
	return static_cast<Enum>(~static_cast<typename std::underlying_type<Enum>::type>(e));
}
