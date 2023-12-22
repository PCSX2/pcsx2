// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/AlignedMalloc.h"

template <int i>
class GSAlignedClass
{
protected:
	GSAlignedClass() = default;
	~GSAlignedClass() = default;

public:
	void* operator new(size_t size)
	{
		return _aligned_malloc(size, i);
	}

	void operator delete(void* p)
	{
		_aligned_free(p);
	}

	void* operator new(size_t size, void* ptr)
	{
		return ptr;
	}

	void operator delete(void* ptr, void* placement_ptr)
	{
		// Just here to satisfy compilers
		// Person who calls in-place placement new must handle error case
	}

	void* operator new[](size_t size)
	{
		return _aligned_malloc(size, i);
	}

	void operator delete[](void* p)
	{
		_aligned_free(p);
	}
};

/// GSAlignedClass with a virtual destructor
template <int i>
class GSVirtualAlignedClass : public GSAlignedClass<i>
{
public:
	virtual ~GSVirtualAlignedClass() {}
};
