// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <functional>
#include <utility>

template <typename T, typename... Rest>
static inline void HashCombine(std::size_t& seed, const T& v, Rest&&... rest)
{
	seed ^= std::hash<T>{}(v) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
	(HashCombine(seed, std::forward<Rest>(rest)), ...);
}
