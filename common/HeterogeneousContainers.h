// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

/**
 * Provides a map template which doesn't require heap allocations for lookups.
 */

#pragma once

#include "Pcsx2Defs.h"
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace detail
{
	struct transparent_string_hash
	{
		using is_transparent = void;

		std::size_t operator()(const std::string_view& v) const { return std::hash<std::string_view>{}(v); }
		std::size_t operator()(const std::string& s) const { return std::hash<std::string>{}(s); }
		std::size_t operator()(const char* s) const { return operator()(std::string_view(s)); }
	};

	struct transparent_string_equal
	{
		using is_transparent = void;

		bool operator()(const std::string& lhs, const std::string_view& rhs) const { return lhs == rhs; }
		bool operator()(const std::string& lhs, const std::string& rhs) const { return lhs == rhs; }
		bool operator()(const std::string& lhs, const char* rhs) const { return lhs == rhs; }
		bool operator()(const std::string_view& lhs, const std::string& rhs) const { return lhs == rhs; }
		bool operator()(const char* lhs, const std::string& rhs) const { return lhs == rhs; }
	};

	struct transparent_string_less
	{
		using is_transparent = void;

		bool operator()(const std::string& lhs, const std::string_view& rhs) const { return lhs < rhs; }
		bool operator()(const std::string& lhs, const std::string& rhs) const { return lhs < rhs; }
		bool operator()(const std::string& lhs, const char* rhs) const { return lhs < rhs; }
		bool operator()(const std::string_view& lhs, const std::string& rhs) const { return lhs < rhs; }
		bool operator()(const char* lhs, const std::string& rhs) const { return lhs < rhs; }
	};
} // namespace detail

template <typename ValueType>
using UnorderedStringMap =
	std::unordered_map<std::string, ValueType, detail::transparent_string_hash, detail::transparent_string_equal>;
template <typename ValueType>
using UnorderedStringMultimap =
	std::unordered_multimap<std::string, ValueType, detail::transparent_string_hash, detail::transparent_string_equal>;
using UnorderedStringSet =
	std::unordered_set<std::string, detail::transparent_string_hash, detail::transparent_string_equal>;
using UnorderedStringMultiSet =
	std::unordered_multiset<std::string, detail::transparent_string_hash, detail::transparent_string_equal>;

template <typename ValueType>
using StringMap = std::map<std::string, ValueType, detail::transparent_string_less>;
template <typename ValueType>
using StringMultiMap = std::multimap<std::string, ValueType, detail::transparent_string_less>;
using StringSet = std::set<std::string, detail::transparent_string_less>;
using StringMultiSet = std::multiset<std::string, detail::transparent_string_less>;
