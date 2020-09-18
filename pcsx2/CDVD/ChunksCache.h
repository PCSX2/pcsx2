/*  PCSX2 - PS2 Emulator for PCs
*  Copyright (C) 2002-2014  PCSX2 Dev Team
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

#include "zlib_indexed.h"

#define CLAMP(val, minval, maxval) (std::min(maxval, std::max(minval, val)))

class ChunksCache
{
public:
	ChunksCache(uint initialLimitMb)
		: m_entries(0)
		, m_size(0)
		, m_limit(initialLimitMb * 1024 * 1024){};
	~ChunksCache() { Clear(); };
	void SetLimit(uint megabytes);
	void Clear() { MatchLimit(true); };

	void Take(void* pMallocedSrc, PX_off_t offset, int length, int coverage);
	int Read(void* pDest, PX_off_t offset, int length);

	static int CopyAvailable(void* pSrc, PX_off_t srcOffset, int srcSize,
							 void* pDst, PX_off_t dstOffset, int maxCopySize)
	{
		int available = CLAMP(maxCopySize, 0, (int)(srcOffset + srcSize - dstOffset));
		memcpy(pDst, (char*)pSrc + (dstOffset - srcOffset), available);
		return available;
	};

private:
	class CacheEntry
	{
	public:
		CacheEntry(void* pMallocedSrc, PX_off_t offset, int length, int coverage)
			: data(pMallocedSrc)
			, offset(offset)
			, coverage(coverage)
			, size(length){};

		~CacheEntry()
		{
			if (data)
				free(data);
		};

		void* data;
		PX_off_t offset;
		int coverage;
		int size;
	};

	std::list<CacheEntry*> m_entries;
	void MatchLimit(bool removeAll = false);
	PX_off_t m_size;
	PX_off_t m_limit;
};

#undef CLAMP
