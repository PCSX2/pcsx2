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

#include "PrecompiledHeader.h"
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
