// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Pcsx2Types.h"

#include <algorithm>
#include <cstring>
#include <list>

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

	void Take(void* pMallocedSrc, s64 offset, int length, int coverage);
	int Read(void* pDest, s64 offset, int length);

	static int CopyAvailable(void* pSrc, s64 srcOffset, int srcSize,
							 void* pDst, s64 dstOffset, int maxCopySize)
	{
		int available = std::clamp(maxCopySize, 0, std::max((int)(srcOffset + srcSize - dstOffset), 0));
		std::memcpy(pDst, (char*)pSrc + (dstOffset - srcOffset), available);
		return available;
	};

private:
	class CacheEntry
	{
	public:
		CacheEntry(void* pMallocedSrc, s64 offset, int length, int coverage)
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
		s64 offset;
		int coverage;
		int size;
	};

	std::list<CacheEntry*> m_entries;
	void MatchLimit(bool removeAll = false);
	s64 m_size;
	s64 m_limit;
};
