// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "common/Pcsx2Defs.h"

[[maybe_unused]]
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

[[maybe_unused]]
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
