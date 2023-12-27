// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <type_traits>

// Template function for casting enumerations to their underlying type
template <typename Enumeration>
typename std::underlying_type<Enumeration>::type enum_cast(Enumeration E)
{
	return static_cast<typename std::underlying_type<Enumeration>::type>(E);
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
