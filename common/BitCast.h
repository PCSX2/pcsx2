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

// An implementation of bit_cast until we get c++20

template<class T2, class T1>
T2 bit_cast(const T1& src)
{
	static_assert(sizeof(T2) == sizeof(T1), "bit_cast: types must be equal size");
	static_assert(std::is_pod<T1>::value, "bit_cast: source must be POD");
	static_assert(std::is_pod<T2>::value, "bit_cast: destination must be POD");
	T2 dst;
	memcpy(&dst, &src, sizeof(T2));
	return dst;
}
