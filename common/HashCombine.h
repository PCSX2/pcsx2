/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#include <functional>
#include <utility>

template <typename T, typename... Rest>
static inline void HashCombine(std::size_t& seed, const T& v, Rest&&... rest)
{
	seed ^= std::hash<T>{}(v) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
	(HashCombine(seed, std::forward<Rest>(rest)), ...);
}
