/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

// This module contains implementations of _aligned_malloc for platforms that don't have
// it built into their CRT/libc.

#include "PrecompiledHeader.h"

struct AlignedMallocHeader
{
	size_t size;		// size of the allocated buffer (minus alignment and header)
	void* baseptr;	// offset of the original allocated pointer
};

static const uint headsize = sizeof(AlignedMallocHeader);

void* __fastcall pcsx2_aligned_malloc(size_t size, size_t align)
{
	pxAssert( align < 0x10000 );
#ifdef _WIN32
	return _aligned_malloc(size, align);
#elif defined(__USE_ISOC11) && !defined(ASAN_WORKAROUND) // not supported yet on gcc 4.9
	return aligned_alloc(align, size);
#else
	void *result=0;
	posix_memalign(&result, alignment, size);
	return result;
#endif
}

void* __fastcall pcsx2_aligned_realloc(void* handle, size_t size, size_t align)
{
	pxAssert( align < 0x10000 );

	void* newbuf = pcsx2_aligned_malloc( size, align );

	if( handle != NULL )
	{
		memcpy_fast( newbuf, handle, size );
		pcsx2_aligned_free(handle);
	}
	return newbuf;
}

__fi void pcsx2_aligned_free(void* pmem)
{
#ifdef _WIN32
	_aligned_free(pmem);
#else
	free(pmem);
#endif
}
