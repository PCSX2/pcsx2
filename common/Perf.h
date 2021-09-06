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

#pragma once

#include <vector>
#include <cstdio>
#include "common/Pcsx2Types.h"

namespace Perf
{

	struct Info
	{
		uptr m_x86;
		u32 m_size;
		char m_symbol[20];
		// The idea is to keep static zones that are set only
		// once.
		bool m_dynamic;

		Info(uptr x86, u32 size, const char* symbol);
		Info(uptr x86, u32 size, const char* symbol, u32 pc);
		void Print(FILE* fp);
	};

	class InfoVector
	{
		std::vector<Info> m_v;
		char m_prefix[20];
		unsigned int m_vtune_id;

	public:
		InfoVector(const char* prefix);

		void print(FILE* fp);
		void map(uptr x86, u32 size, const char* symbol);
		void map(uptr x86, u32 size, u32 pc);
		void reset();
	};

	void dump();
	void dump_and_reset();

	extern InfoVector any;
	extern InfoVector ee;
	extern InfoVector iop;
	extern InfoVector vu;
	extern InfoVector vif;
} // namespace Perf
