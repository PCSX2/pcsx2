/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

#include <cstring> // memset
#include "common/Pcsx2Types.h"
#include "common/Pcsx2Defs.h"

// This method can clear any object-like entity -- which is anything that is not a pointer.
// Structures, static arrays, etc.  No need to include sizeof() crap, this does it automatically
// for you!
template <typename T>
static __fi void memzero(T& object)
{
	memset(&object, 0, sizeof(T));
}

// This method clears an object with the given 8 bit value.
template <u8 data, typename T>
static __fi void memset8(T& object)
{
	memset(&object, data, sizeof(T));
}
