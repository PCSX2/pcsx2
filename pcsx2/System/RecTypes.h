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

#include "common/PageFaultSource.h"

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
	std::string	m_profiler_name;

public:
	RecompiledCodeReserve( std::string name, uint defCommit = 0 );
	virtual ~RecompiledCodeReserve();

	virtual void* Assign( VirtualMemoryManagerPtr allocator, void *baseptr, size_t size ) override;
	virtual void Reset() override;
	virtual bool Commit() override;

	virtual RecompiledCodeReserve& SetProfilerName( std::string shortname );

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
