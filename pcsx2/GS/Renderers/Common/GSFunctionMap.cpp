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

namespace GSCodeReserve
{
	static u8* s_memory_base;
	static u8* s_memory_end;
	static u8* s_memory_ptr;
}

void GSCodeReserve::ResetMemory()
{
	s_memory_base = SysMemory::GetSWRec();
	s_memory_end = SysMemory::GetSWRecEnd();
	s_memory_ptr = s_memory_base;
}

size_t GSCodeReserve::GetMemoryUsed()
{
	return s_memory_ptr - s_memory_base;
}

u8* GSCodeReserve::ReserveMemory(size_t size)
{
	pxAssert((s_memory_ptr + size) <= s_memory_end);
	return s_memory_ptr;
}

void GSCodeReserve::CommitMemory(size_t size)
{
	pxAssert((s_memory_ptr + size) <= s_memory_end);
	s_memory_ptr += size;
}
