/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#include "common/Perf.h"
#include "common/Pcsx2Defs.h"
#ifdef __unix__
#include <unistd.h>
#endif
#ifdef ENABLE_VTUNE
#include "jitprofiling.h"
#endif

//#define ProfileWithPerf
#define MERGE_BLOCK_RESULT

#ifdef ENABLE_VTUNE
#ifdef _WIN32
#pragma comment(lib, "jitprofiling.lib")
#endif
#endif

namespace Perf
{
	// Warning object aren't thread safe
	InfoVector any("");
	InfoVector ee("EE");
	InfoVector iop("IOP");
	InfoVector vu("VU");
	InfoVector vif("VIF");

// Perf is only supported on linux
#if defined(__linux__) && (defined(ProfileWithPerf) || defined(ENABLE_VTUNE))

	////////////////////////////////////////////////////////////////////////////////
	// Implementation of the Info object
	////////////////////////////////////////////////////////////////////////////////

	Info::Info(uptr x86, u32 size, const char* symbol)
		: m_x86(x86)
		, m_size(size)
		, m_dynamic(false)
	{
		strncpy(m_symbol, symbol, sizeof(m_symbol));
	}

	Info::Info(uptr x86, u32 size, const char* symbol, u32 pc)
		: m_x86(x86)
		, m_size(size)
		, m_dynamic(true)
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
#ifdef ENABLE_VTUNE
		m_vtune_id = iJIT_GetNewMethodID();
#else
		m_vtune_id = 0;
#endif
	}

	void InfoVector::print(FILE* fp)
	{
		for (auto&& it : m_v)
			it.Print(fp);
	}

	void InfoVector::map(uptr x86, u32 size, const char* symbol)
	{
// This function is typically used for dispatcher and recompiler.
// Dispatchers are on a page and must always be kept.
// Recompilers are much bigger (TODO check VIF) and are only
// useful when MERGE_BLOCK_RESULT is defined
#if defined(ENABLE_VTUNE) || !defined(MERGE_BLOCK_RESULT)
		u32 max_code_size = 16 * _1kb;
#else
		u32 max_code_size = _1gb;
#endif

		if (size < max_code_size)
		{
			m_v.emplace_back(x86, size, symbol);

#ifdef ENABLE_VTUNE
			std::string name = std::string(symbol);

			iJIT_Method_Load ml;

			memset(&ml, 0, sizeof(ml));

			ml.method_id = iJIT_GetNewMethodID();
			ml.method_name = (char*)name.c_str();
			ml.method_load_address = (void*)x86;
			ml.method_size = size;

			iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, &ml);

//fprintf(stderr, "mapF %s: %p size %dKB\n", ml.method_name, ml.method_load_address, ml.method_size / 1024u);
#endif
		}
	}

	void InfoVector::map(uptr x86, u32 size, u32 pc)
	{
#ifndef MERGE_BLOCK_RESULT
		m_v.emplace_back(x86, size, m_prefix, pc);
#endif

#ifdef ENABLE_VTUNE
		iJIT_Method_Load_V2 ml;

		memset(&ml, 0, sizeof(ml));

#ifdef MERGE_BLOCK_RESULT
		ml.method_id = m_vtune_id;
		ml.method_name = m_prefix;
#else
		std::string name = std::string(m_prefix) + "_" + std::to_string(pc);
		ml.method_id = iJIT_GetNewMethodID();
		ml.method_name = (char*)name.c_str();
#endif
		ml.method_load_address = (void*)x86;
		ml.method_size = size;

		iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED_V2, &ml);

//fprintf(stderr, "mapB %s: %p size %d\n", ml.method_name, ml.method_load_address, ml.method_size);
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

	InfoVector::InfoVector(const char* prefix)
		: m_vtune_id(0)
	{
	}
	void InfoVector::map(uptr x86, u32 size, const char* symbol) {}
	void InfoVector::map(uptr x86, u32 size, u32 pc) {}
	void InfoVector::reset() {}

	void dump() {}
	void dump_and_reset() {}

#endif
} // namespace Perf
