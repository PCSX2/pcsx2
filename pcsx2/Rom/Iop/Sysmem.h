/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2014  PCSX2 Dev Team
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

//#define HLE_SYSMEM_MODULE

namespace R3000A {

	namespace sysmem {
		struct AllocInfo {
			u16 size;
			u16 addr;
			AllocInfo(u16 s, u16 a) : size(s), addr(a) {};
		};

		enum SMEM_location {
			SMEM_low = 0,
			SMEM_high = 1,
			SMEM_addr = 2,
		};

		enum Query {
			QUERY_SIZE,
			QUERY_ADDR,
		};

		int AllocSysMemory_HLE();
		int FreeSysMemory_HLE();
		int QueryMemSize_HLE();
		int QueryTotalFreeMemSize_HLE();
		int QueryBlockSize_HLE();
		int QueryBlockTopAddress_HLE();
		int QueryMaxFreeMemSize_HLE();

		void Reset();
	}
}
