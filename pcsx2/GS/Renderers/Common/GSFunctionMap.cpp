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
#include "GS/Renderers/Common/GSFunctionMap.h"
#include "System.h"

static GSCodeReserve s_instance;

GSCodeReserve::GSCodeReserve()
	: RecompiledCodeReserve("GS Software Renderer")
{
}

GSCodeReserve::~GSCodeReserve() = default;

GSCodeReserve& GSCodeReserve::GetInstance()
{
	return s_instance;
}

void GSCodeReserve::Assign(VirtualMemoryManagerPtr allocator)
{
	RecompiledCodeReserve::Assign(std::move(allocator), HostMemoryMap::SWrecOffset, HostMemoryMap::SWrecSize);
}

void GSCodeReserve::Reset()
{
	RecompiledCodeReserve::Reset();
	m_memory_used = 0;
}

u8* GSCodeReserve::Reserve(size_t size)
{
	pxAssert((m_memory_used + size) <= m_size);
	return m_baseptr + m_memory_used;
}

void GSCodeReserve::Commit(size_t size)
{
	pxAssert((m_memory_used + size) <= m_size);
	m_memory_used += size;
}
