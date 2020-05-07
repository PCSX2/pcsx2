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

#include "Utilities/PageFaultSource.h"

// --------------------------------------------------------------------------------------
//  RecompiledCodeReserve
// --------------------------------------------------------------------------------------
// A recompiled code reserve is a simple sequential-growth block of memory which is auto-
// cleared to INT 3 (0xcc) as needed.
//
class RecompiledCodeReserve : public VirtualMemoryReserve
{
	typedef VirtualMemoryReserve _parent;

protected:
	wxString	m_profiler_name;

public:
	RecompiledCodeReserve( const wxString& name=wxEmptyString, uint defCommit = 0 );
	virtual ~RecompiledCodeReserve();

	virtual void* Reserve( size_t size, uptr base=0, uptr upper_bounds=0 );
	virtual void Reset();
	virtual bool Commit();

	virtual RecompiledCodeReserve& SetProfilerName( const wxString& shortname );
	virtual RecompiledCodeReserve& SetProfilerName( const char* shortname )
	{
		return SetProfilerName( fromUTF8(shortname) );
	}

	void ThrowIfNotOk() const;

	operator void*()				{ return m_baseptr; }
	operator const void*() const	{ return m_baseptr; }

	operator u8*()				{ return (u8*)m_baseptr; }
	operator const u8*() const	{ return (u8*)m_baseptr; }

protected:
	void ResetProcessReserves() const;

	void _registerProfiler();
	void _termProfiler();
};

// --------------------------------------------------------------------------------------
//  CodegenAccessible
// --------------------------------------------------------------------------------------
// In x86-64, codegen can't reference any other address, only addresses within the lower
// 2GB of address space or within 2GB of the current instruction pointer.
// Because of this, static variables that need to be read by generated code will need to
// be copied to a referencable place.
//
#ifndef __M_X86_64
template <typename T> using CodegenAccessible = T&;
namespace HostSys {
	// On 32-bit all addresses are codegen accessible, so we don't need to do anything
	template <typename T>
	void MakeCodegenAccessible(CodegenAccessible<T> item) {}
}
#else
template <typename T> using CodegenAccessible = std::reference_wrapper<T>;

namespace HostSys {
	namespace detail {
		extern void *MakeCodegenAccessible(void *ptr, sptr size);
	}

	/// Moves the given CodegenAccessible item somewhere that's codegen-accessible
	/// It's okay to call this on the same object multiple times, only the first one will actually do anything
	template <typename T>
	void MakeCodegenAccessible(CodegenAccessible<T>& item)
	{
		T* ptr = &item.get();
		item = *(T*)detail::MakeCodegenAccessible((void *)ptr, sizeof(T));
	}
}
#endif
