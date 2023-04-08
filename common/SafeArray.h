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

#include "common/Pcsx2Defs.h"

// Microsoft Windows only macro, useful for freeing out COM objects:
#define safe_release(ptr) \
	((void)((((ptr) != NULL) && ((ptr)->Release(), !!0)), (ptr) = NULL))

// --------------------------------------------------------------------------------------
//  SafeArray
// --------------------------------------------------------------------------------------
// Handy little class for allocating a resizable memory block, complete with exception
// error handling and automatic cleanup.  A lightweight alternative to std::vector.
//
template <typename T>
class SafeArray
{
	DeclareNoncopyableObject(SafeArray);

public:
	static const int DefaultChunkSize = 0x1000 * sizeof(T);

public:
	std::string Name; // user-assigned block name
	int ChunkSize;

protected:
	T* m_ptr;
	int m_size; // size of the allocation of memory

protected:
	SafeArray(std::string name, T* allocated_mem, int initSize);
	virtual T* _virtual_realloc(int newsize);

	// A safe array index fetcher.  Asserts if the index is out of bounds (dev and debug
	// builds only -- no bounds checking is done in release builds).
	T* _getPtr(uint i) const;

public:
	virtual ~SafeArray();

	explicit SafeArray(std::string name = "Unnamed");
	explicit SafeArray(int initialSize, std::string name = "Unnamed");

	void Dispose();
	void ExactAlloc(int newsize);
	void MakeRoomFor(int newsize)
	{
		if (newsize > m_size)
			ExactAlloc(newsize);
	}

	bool IsDisposed() const { return (m_ptr == NULL); }

	// Returns the size of the memory allocation, as according to the array type.
	int GetLength() const { return m_size; }
	// Returns the size of the memory allocation in bytes.
	int GetSizeInBytes() const { return m_size * sizeof(T); }

	// Extends the containment area of the array.  Extensions are performed
	// in chunks.
	void GrowBy(int items)
	{
		MakeRoomFor(m_size + ChunkSize + items + 1);
	}

	// Gets a pointer to the requested allocation index.
	// DevBuilds : Generates assertion if the index is invalid.
	T* GetPtr(uint idx = 0) { return _getPtr(idx); }
	const T* GetPtr(uint idx = 0) const { return _getPtr(idx); }

	// Gets a pointer to the element directly after the last element in the array.
	// This is equivalent to doing GetPtr(GetLength()), except that this call *avoids*
	// the out-of-bounds assertion check that typically occurs when you do that. :)
	T* GetPtrEnd() { return &m_ptr[m_size]; }
	const T* GetPtrEnd() const { return &m_ptr[m_size]; }

	// Gets an element of this memory allocation much as if it were an array.
	// DevBuilds : Generates assertion if the index is invalid.
	T& operator[](int idx) { return *_getPtr((uint)idx); }
	const T& operator[](int idx) const { return *_getPtr((uint)idx); }

	virtual SafeArray<T>* Clone() const;
};
