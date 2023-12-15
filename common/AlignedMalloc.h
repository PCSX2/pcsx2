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

#pragma once

#include "Pcsx2Defs.h"
#include <cstring>
#include <cstdlib>
#include <new> // std::bad_alloc
#include <memory>
#include <type_traits>
#include <utility>

#ifdef _MSC_VER
#include <malloc.h>
#endif

// Implementation note: all known implementations of _aligned_free check the pointer for
// NULL status (our implementation under GCC, and microsoft's under MSVC), so no need to
// do it here.
#define safe_aligned_free(ptr) \
	((void)(_aligned_free(ptr), (ptr) = NULL))

// aligned_malloc: Implement/declare linux equivalents here!
#if !defined(_MSC_VER)
extern void* _aligned_malloc(size_t size, size_t align);
extern void* pcsx2_aligned_realloc(void* handle, size_t new_size, size_t align, size_t old_size);
extern void _aligned_free(void* pmem);
#else
#define pcsx2_aligned_realloc(handle, new_size, align, old_size) \
	_aligned_realloc(handle, new_size, align)
#endif
