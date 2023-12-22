// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "ChunksCache.h"

void ChunksCache::SetLimit(uint megabytes)
{
	m_limit = (s64)megabytes * 1024 * 1024;
	MatchLimit();
}

void ChunksCache::MatchLimit(bool removeAll)
{
	std::list<CacheEntry*>::reverse_iterator rit;
	while (!m_entries.empty() && (removeAll || m_size > m_limit))
	{
		rit = m_entries.rbegin();
		m_size -= (*rit)->size;
		delete (*rit);
		m_entries.pop_back();
	}
}

void ChunksCache::Take(void* pMallocedSrc, s64 offset, int length, int coverage)
{
	m_entries.push_front(new CacheEntry(pMallocedSrc, offset, length, coverage));
	m_size += length;
	MatchLimit();
}

// By design, succeed only if the entire request is in a single cached chunk
int ChunksCache::Read(void* pDest, s64 offset, int length)
{
	for (auto it = m_entries.begin(); it != m_entries.end(); it++)
	{
		CacheEntry* e = *it;
		if (e && offset >= e->offset && (offset + length) <= (e->offset + e->coverage))
		{
			if (it != m_entries.begin())
				m_entries.splice(m_entries.begin(), m_entries, it); // Move to top (MRU)
			return CopyAvailable(e->data, e->offset, e->size, pDest, offset, length);
		}
	}
	return -1;
}
