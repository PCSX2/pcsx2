/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#include "GS_types.h"

/// Table for storing swizzling of blocks within a page
struct alignas(64) GSBlockSwizzleTable
{
	// Some swizzles are 4x8 and others are 8x4.  An 8x8 table can store either at the cost of 2x size
	uint8 value[8][8];

	constexpr uint8 lookup(int x, int y) const
	{
		return value[y & 7][x & 7];
	}
};

extern const GSBlockSwizzleTable blockTable32;
extern const GSBlockSwizzleTable blockTable32Z;
extern const GSBlockSwizzleTable blockTable16;
extern const GSBlockSwizzleTable blockTable16S;
extern const GSBlockSwizzleTable blockTable16Z;
extern const GSBlockSwizzleTable blockTable16SZ;
extern const GSBlockSwizzleTable blockTable8;
extern const GSBlockSwizzleTable blockTable4;
extern const uint8 columnTable32[8][8];
extern const uint8 columnTable16[8][16];
extern const uint8 columnTable8[16][16];
extern const uint16 columnTable4[16][32];
extern const uint8 clutTableT32I8[128];
extern const uint8 clutTableT32I4[16];
extern const uint8 clutTableT16I8[32];
extern const uint8 clutTableT16I4[16];
