/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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

#include "common/Pcsx2Defs.h"

__ri static void MemCopy_WrappedDest(const u128* src, u128* destBase, uint& destStart, uint destSize, uint len)
{
	uint endpos = destStart + len;
	if (endpos < destSize)
	{
		memcpy(&destBase[destStart], src, len * 16);
		destStart += len;
	}
	else
	{
		uint firstcopylen = destSize - destStart;
		memcpy(&destBase[destStart], src, firstcopylen * 16);
		destStart = endpos % destSize;
		memcpy(destBase, src + firstcopylen, destStart * 16);
	}
}

__ri static void MemCopy_WrappedSrc(const u128* srcBase, uint& srcStart, uint srcSize, u128* dest, uint len)
{
	uint endpos = srcStart + len;
	if (endpos < srcSize)
	{
		memcpy(dest, &srcBase[srcStart], len * 16);
		srcStart += len;
	}
	else
	{
		uint firstcopylen = srcSize - srcStart;
		memcpy(dest, &srcBase[srcStart], firstcopylen * 16);
		srcStart = endpos % srcSize;
		memcpy(dest + firstcopylen, srcBase, srcStart * 16);
	}
}
