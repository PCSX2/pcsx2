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

// This file defines functions that are linked to by files used in swizzle tests but not actually used in swizzle tests, in order to make linkers happy

#include "PrecompiledHeader.h"
#include "GSBlock.h"
#include "GSClut.h"
#include "GSLocalMemory.h"

GSLocalMemory::psm_t GSLocalMemory::m_psm[64];

void* vmalloc(size_t size, bool code)
{
	abort();
}

void vmfree(void* ptr, size_t size)
{
	abort();
}
