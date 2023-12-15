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
	class Group
	{
		const char* m_prefix;

	public:
		constexpr Group(const char* prefix) : m_prefix(prefix) {}
		bool HasPrefix() const { return (m_prefix && m_prefix[0]); }

		void Register(const void* ptr, size_t size, const char* symbol);
		void RegisterPC(const void* ptr, size_t size, u32 pc);
		void RegisterKey(const void* ptr, size_t size, const char* prefix, u64 key);
	};

	extern Group any;
	extern Group ee;
	extern Group iop;
	extern Group vu0;
	extern Group vu1;
	extern Group vif;
} // namespace Perf
