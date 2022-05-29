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

#ifndef __OBJC__
	#error This header is for use with Objective-C++ only.
#endif

#if __has_feature(objc_arc)
	#error This file is for manual reference counting!  Compile without -fobjc-arc
#endif

#pragma once

#include <cstddef>
#include <utility>

/// Managed Obj-C pointer
template <typename T>
class MRCOwned
{
	T ptr;
	MRCOwned(T ptr): ptr(ptr) {}
public:
	MRCOwned(): ptr(nullptr) {}
	MRCOwned(std::nullptr_t): ptr(nullptr) {}
	MRCOwned(MRCOwned&& other)
		: ptr(other.ptr)
	{
		other.ptr = nullptr;
	}
	MRCOwned(const MRCOwned& other)
		: ptr(other.ptr)
	{
		[ptr retain];
	}
	~MRCOwned()
	{
		if (ptr)
			[ptr release];
	}
	operator T() const { return ptr; }
	MRCOwned& operator=(const MRCOwned& other)
	{
		[other.ptr retain];
		if (ptr)
			[ptr release];
		ptr = other.ptr;
		return *this;
	}
	MRCOwned& operator=(MRCOwned&& other)
	{
		std::swap(ptr, other.ptr);
		return *this;
	}
	void Reset()
	{
		[ptr release];
		ptr = nullptr;
	}
	T Get() const { return ptr; }
	static MRCOwned Transfer(T ptr)
	{
		return MRCOwned(ptr);
	}
	static MRCOwned Retain(T ptr)
	{
		[ptr retain];
		return MRCOwned(ptr);
	}
};

/// Take ownership of an Obj-C pointer (equivalent to __bridge_transfer)
template<typename T>
static inline MRCOwned<T> MRCTransfer(T ptr)
{
	return MRCOwned<T>::Transfer(ptr);
}

/// Retain an Obj-C pointer (equivalent to __bridge)
template<typename T>
static inline MRCOwned<T> MRCRetain(T ptr)
{
	return MRCOwned<T>::Retain(ptr);
}

