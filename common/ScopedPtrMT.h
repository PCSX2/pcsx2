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

#include "Threading.h"
using Threading::ScopedLock;

// --------------------------------------------------------------------------------------
//  ScopedPtrMT
// --------------------------------------------------------------------------------------

template <typename T>
class ScopedPtrMT
{
	DeclareNoncopyableObject(ScopedPtrMT);

protected:
	std::atomic<T*> m_ptr;
	Threading::Mutex m_mtx;

public:
	typedef T element_type;

	wxEXPLICIT ScopedPtrMT(T* ptr = nullptr)
	{
		m_ptr = ptr;
	}

	~ScopedPtrMT() { _Delete_unlocked(); }

	ScopedPtrMT& Reassign(T* ptr = nullptr)
	{
		T* doh = m_ptr.exchange(ptr);
		if (ptr != doh)
			delete doh;
		return *this;
	}

	ScopedPtrMT& Delete() noexcept
	{
		ScopedLock lock(m_mtx);
		_Delete_unlocked();
	}

	// Removes the pointer from scoped management, but does not delete!
	// (ScopedPtr will be nullptr after this method)
	T* DetachPtr()
	{
		ScopedLock lock(m_mtx);

		return m_ptr.exchange(nullptr);
	}

	// Returns the managed pointer.  Can return nullptr as a valid result if the ScopedPtrMT
	// has no object in management.
	T* GetPtr() const
	{
		return m_ptr;
	}

	void SwapPtr(ScopedPtrMT& other)
	{
		ScopedLock lock(m_mtx);
		m_ptr.exchange(other.m_ptr.exchange(m_ptr.load()));
		T* const tmp = other.m_ptr;
		other.m_ptr = m_ptr;
		m_ptr = tmp;
	}

	// ----------------------------------------------------------------------------
	//  ScopedPtrMT Operators
	// ----------------------------------------------------------------------------
	// I've decided to use the ATL's approach to pointer validity tests, opposed to
	// the wx/boost approach (which uses some bizarre member method pointer crap, and can't
	// allow the T* implicit casting.

	bool operator!() const noexcept
	{
		return m_ptr.load() == nullptr;
	}

	// Equality
	bool operator==(T* pT) const noexcept
	{
		return m_ptr == pT;
	}

	// Inequality
	bool operator!=(T* pT) const noexcept
	{
		return !operator==(pT);
	}

	// Convenient assignment operator.  ScopedPtrMT = nullptr will issue an automatic deletion
	// of the managed pointer.
	ScopedPtrMT& operator=(T* src)
	{
		return Reassign(src);
	}

#if 0
	operator T*() const
	{
		return m_ptr;
	}

	// Dereference operator, returns a handle to the managed pointer.
	// Generates a debug assertion if the object is nullptr!
	T& operator*() const
	{
		pxAssert(m_ptr != nullptr);
		return *m_ptr;
	}

	T* operator->() const
	{
		pxAssert(m_ptr != nullptr);
		return m_ptr;
	}
#endif

protected:
	void _Delete_unlocked() noexcept
	{
		delete m_ptr.exchange(nullptr);
	}
};
