/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2015  PCSX2 Dev Team
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

#include "PrecompiledHeader.h"

#include "Perf.h"

//#define ProfileWithPerf
#define MERGE_BLOCK_RESULT


namespace Perf
{
	// Warning object aren't thread safe
	InfoVector any("");
	InfoVector ee("EE");
	InfoVector iop("IOP");
	InfoVector vu("VU");

// Perf is only supported on linux
#if defined(__linux__) && defined(ProfileWithPerf)

	////////////////////////////////////////////////////////////////////////////////
	// Implementation of the Info object
	////////////////////////////////////////////////////////////////////////////////

	Info::Info(uptr x86, u32 size, const char* symbol) : m_x86(x86), m_size(size), m_dynamic(false)
	{
		strncpy(m_symbol, symbol, sizeof(m_symbol));
	}

	Info::Info(uptr x86, u32 size, const char* symbol, u32 pc) : m_x86(x86), m_size(size), m_dynamic(true)
	{
		snprintf(m_symbol, sizeof(m_symbol), "%s_0x%08x", symbol, pc);
	}

	void Info::Print(FILE* fp)
	{
		fprintf(fp, "%x %x %s\n", m_x86, m_size, m_symbol);
	}

	////////////////////////////////////////////////////////////////////////////////
	// Implementation of the InfoVector object
	////////////////////////////////////////////////////////////////////////////////

	InfoVector::InfoVector(const char* prefix)
	{
		strncpy(m_prefix, prefix, sizeof(m_prefix));
	}

	void InfoVector::print(FILE* fp)
	{
		for(auto&& it : m_v) it.Print(fp);
	}

	void InfoVector::map(uptr x86, u32 size, const char* symbol)
	{
		// This function is typically used for dispatcher and recompiler.
		// Dispatchers are on a page and must always be kept.
		// Recompilers are much bigger (TODO check VIF) and are only
		// useful when MERGE_BLOCK_RESULT is defined

#ifdef MERGE_BLOCK_RESULT
		m_v.emplace_back(x86, size, symbol);
#else
		if (size < 8 * _1kb) m_v.emplace_back(x86, size, symbol);
#endif
	}

	void InfoVector::map(uptr x86, u32 size, u32 pc)
	{
#ifndef MERGE_BLOCK_RESULT
		m_v.emplace_back(x86, size, m_prefix, pc);
#endif
	}

	void InfoVector::reset()
	{
		auto dynamic = std::remove_if(m_v.begin(), m_v.end(), [](Info i) { return i.m_dynamic; });
		m_v.erase(dynamic, m_v.end());
	}

	////////////////////////////////////////////////////////////////////////////////
	// Global function
	////////////////////////////////////////////////////////////////////////////////

	void dump()
	{
		char file[256];
		snprintf(file, 250, "/tmp/perf-%d.map", getpid());
		FILE* fp = fopen(file, "w");

		any.print(fp);
		ee.print(fp);
		iop.print(fp);
		vu.print(fp);

		if (fp)
			fclose(fp);
	}

	void dump_and_reset()
	{
		dump();

		any.reset();
		ee.reset();
		iop.reset();
		vu.reset();
	}

#else

	////////////////////////////////////////////////////////////////////////////////
	// Dummy implementation
	////////////////////////////////////////////////////////////////////////////////

	InfoVector::InfoVector(const char* prefix) {}
	void InfoVector::map(uptr x86, u32 size, const char* symbol) {}
	void InfoVector::map(uptr x86, u32 size, u32 pc) {}
	void InfoVector::reset() {}

	void dump() {}
	void dump_and_reset() {}

#endif

}
