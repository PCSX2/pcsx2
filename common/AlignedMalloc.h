// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
