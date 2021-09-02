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

#include "PrecompiledHeader.h"
#include "GSCodeBuffer.h"
#include "GSExtra.h"

GSCodeBuffer::GSCodeBuffer(size_t blocksize)
	: m_blocksize(blocksize)
	, m_pos(0)
	, m_reserved(0)
	, m_ptr(NULL)
{
}

GSCodeBuffer::~GSCodeBuffer()
{
	for (auto buffer : m_buffers)
	{
		vmfree(buffer, m_blocksize);
	}
}

void* GSCodeBuffer::GetBuffer(size_t size)
{
	pxAssert(size < m_blocksize);
	pxAssert(m_reserved == 0);

	size = (size + 15) & ~15;

	if (m_ptr == NULL || m_pos + size > m_blocksize)
	{
		m_ptr = (u8*)vmalloc(m_blocksize, true);

		m_pos = 0;

		m_buffers.push_back(m_ptr);
	}

	u8* ptr = &m_ptr[m_pos];

	m_reserved = size;

	return ptr;
}

void GSCodeBuffer::ReleaseBuffer(size_t size)
{
	pxAssert(size <= m_reserved);

	m_pos = ((m_pos + size) + 15) & ~15;

	pxAssert(m_pos < m_blocksize);

	m_reserved = 0;
}
