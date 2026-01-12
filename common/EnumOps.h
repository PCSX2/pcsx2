// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <type_traits>

// Template function for casting enumerations to their underlying type
template <typename Enumeration>
std::underlying_type_t<Enumeration> enum_cast(Enumeration E)
{
	return static_cast<typename std::underlying_type_t<Enumeration>>(E);
}

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
		constexpr operator bool() const { return static_cast<bool>(static_cast<std::underlying_type_t<Enum>>(value)); }
	};
};

#define MARK_ENUM_AS_FLAGS(T) template<> struct detail::enum_is_flags<T> : public std::true_type {}

template <typename Enum>
	requires detail::enum_is_flags<Enum>::value
constexpr Enum operator|(Enum lhs, Enum rhs) noexcept
{
	using underlying = std::underlying_type_t<Enum>;
	return static_cast<Enum>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

template <typename Enum>
	requires detail::enum_is_flags<Enum>::value
constexpr detail::enum_bool_helper<Enum> operator&(Enum lhs, Enum rhs) noexcept
{
	using underlying = std::underlying_type_t<Enum>;
	return static_cast<Enum>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}

template <typename Enum>
	requires detail::enum_is_flags<Enum>::value
constexpr Enum operator^(Enum lhs, Enum rhs) noexcept
{
	using underlying = std::underlying_type_t<Enum>;
	return static_cast<Enum>(static_cast<underlying>(lhs) ^ static_cast<underlying>(rhs));
}

template <typename Enum>
	requires detail::enum_is_flags<Enum>::value
constexpr Enum& operator|=(Enum& lhs, Enum rhs) noexcept
{
	return lhs = lhs | rhs;
}

template <typename Enum>
	requires detail::enum_is_flags<Enum>::value
constexpr Enum& operator&=(Enum& lhs, Enum rhs) noexcept
{
	return lhs = lhs & rhs;
}

template <typename Enum>
	requires detail::enum_is_flags<Enum>::value
constexpr Enum& operator^=(Enum& lhs, Enum rhs) noexcept
{
	return lhs = lhs ^ rhs;
}

template <typename Enum>
	requires detail::enum_is_flags<Enum>::value
constexpr bool operator!(Enum e) noexcept
{
	return !static_cast<std::underlying_type_t<Enum>>(e);
}

template <typename Enum>
	requires detail::enum_is_flags<Enum>::value
constexpr Enum operator~(Enum e) noexcept
{
	return static_cast<Enum>(~static_cast<std::underlying_type_t<Enum>>(e));
}
