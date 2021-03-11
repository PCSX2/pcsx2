/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

template <class T>
static void __forceinline set_bit(T& value, int bit)
{
	value |= (1 << bit);
}

template <class T>
static void __forceinline clear_bit(T& value, int bit)
{
	value &= ~(1 << bit);
}

template <class T>
static void __forceinline toggle_bit(T& value, int bit)
{
	value ^= (1 << bit);
}

template <class T>
static bool __forceinline test_bit(T& value, int bit)
{
	return (value & (1 << bit));
}
