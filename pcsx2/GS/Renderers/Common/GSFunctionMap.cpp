// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GS/Renderers/Common/GSFunctionMap.h"
#include "Memory.h"

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
