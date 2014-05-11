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

#include "PrecompiledHeader.h"
#include "IopCommon.h"
#include "Rom/Iop/Tools.h"

// Nice but verbose
#ifdef PCSX2_DEBUG
#define SELF_CHECK
#endif

namespace R3000A {

namespace sysmem {


	std::list< AllocInfo > allocated_pool;
	std::list< AllocInfo > free_pool;

	u32 memsize = 0;

	__fi int return_hle(u32 ret) {
		v0 = ret;
		pc = ra;
		SYSMEM_LOG("return 0x%x", ret);
		return 1;
	}

#ifdef SELF_CHECK
	void debug_check_free_pool()
	{
		for (auto it = free_pool.begin(); ; it++) {
			auto next_it = std::next(it);
			if (next_it == free_pool.end())
				break;

			// bad order
			assert(it->addr <= next_it->addr);
			// bad merge
			assert((it->addr + it->size) != next_it->addr);
			// overflow
			assert(it->size <= memsize);
			assert(next_it->size <= memsize);
		}
	}
#endif

	void init_memory(u32 size)
	{
		SYSMEM_LOG("init_memory size:%d", size);
		memsize = std::min(8u*1024u*1024u, size & ~0xFF);

		allocated_pool.clear();
		free_pool.clear();
		// Technically on reset the sysmem module is loadeded
		// sysmem : 0x800 -> 0x1500
		const u32 reserved_low_mem = 0x1600;
		AllocInfo free_chunk((memsize - reserved_low_mem) >> 8u, reserved_low_mem >> 8u);
		AllocInfo rsvd_chunk(reserved_low_mem >> 8u, 0);

		free_pool.push_front(free_chunk);
		allocated_pool.push_front(rsvd_chunk);
	}

	int AllocSysMemory_HLE()
	{
		u16 size = (a1 + 255u) >> 8u;
		u16 addr = a2 >> 8u;
		SYSMEM_LOG("AllocSysMemory %d, size:%d, ideal addr:0x%x", a0, a1, a2);

		if (free_pool.empty()) {
			DevCon.Warning("No more memory chunk available");
			return return_hle(0);
		}

		std::list< AllocInfo >::iterator it;
#ifdef SELF_CHECK
		QueryTotalFreeMemSize_HLE();
		u32 total = v0 >> 8u;
#endif

		switch (a0) {
			default:
			case SMEM_low:
				it = free_pool.begin();
				// Search first block big enough
				while (it->size < size && it != free_pool.end()) it++;
				if (it->size < size) {
					DevCon.Warning("No big enough memory chunk available (LOW)");
					return return_hle(0);
				}
				break;
			case SMEM_high:
				it = free_pool.end();
				// Search latest block big enough
				while (it->size < size && it != free_pool.begin()) it--;
				if (it->size < size) {
					DevCon.Warning("No big enough memory chunk available (HIGH)");
					return return_hle(0);
				}

				// Then split it into 2 blocks
				if (it->addr != addr) {
					// with a size of "size" for the 2nd blocks
					free_pool.insert(std::next(it), AllocInfo(size, it->addr + it->size - size));
					// Reduce the size of initial block
					it->size -= size;
					it++;
				}
				break;
			case SMEM_addr:
				it = free_pool.begin();
				// Search first block that contain the addr
				while (((it->addr + it->size - 1u) < addr) && it != free_pool.end()) it++;
				if ((it->addr + it->size - 1u) < addr) {
					DevCon.Warning("No big enough memory chunk available (ADDR)");
					return return_hle(0);
				}

				if (addr < it->addr) {
					// no block contains the address, just take the first one
				} else if (it->addr != addr) {
					// Then split it into 2 blocks. 2nd blocks will start at addr
					u16 new_size = addr - it->addr;
					free_pool.insert(std::next(it), AllocInfo(it->size - new_size, addr));
					it->size = new_size;
					it++;
				}
				break;
		}

		assert(it-> size >= size);

		AllocInfo new_chunk(size, it->addr);
		allocated_pool.push_front(new_chunk);
		it->addr += size;
		it->size -= size;
		if (it->size == 0)
			free_pool.erase(it);

#ifdef SELF_CHECK
		QueryTotalFreeMemSize_HLE();
		assert(total == ((v0 >> 8u)+size));
		debug_check_free_pool();
#endif

		return return_hle(new_chunk.addr << 8u);
	}

	int FreeSysMemory_HLE()
	{
#ifdef SELF_CHECK
		QueryTotalFreeMemSize_HLE();
		u32 total = v0 >> 8u;
#endif
		SYSMEM_LOG("FreeSysMemory_HLE addr:0x%x", a0);

		u32 addr = a0 >> 8u;
		auto it = allocated_pool.begin();
		while (it->addr != addr && it != allocated_pool.end()) it++;

		if (it == allocated_pool.end()) {
			DevCon.Warning("FreeSysMemory address 0x%x was never allocated", addr);
			return return_hle(-1);
		}
		u32 size = it->size;
		allocated_pool.erase(it);

		AllocInfo free_chunk(size, addr);

		if (free_pool.empty()) {
			free_pool.push_back(free_chunk);
			return return_hle(0);
		}

		it = free_pool.begin();
		while ( (it != free_pool.end()) && (it->addr < addr) )
			it++;

		if (it == free_pool.end()) {
			// prev_it < free_chunk
			auto prev_it = std::prev(it);
			// After the last packet
			if ((prev_it->addr + prev_it->size) == addr) {
			 	// merge with existing (left) block if possible
				prev_it->size += size;
			} else {
				free_pool.push_back(free_chunk);
			}
		} else if (it == free_pool.begin()) {
			// free_chunk < it
			if ((addr+size) ==	it->addr) {
				// merge with existing (right) block if possible
				it->addr  = addr;
				it->size += size;
			} else {
				free_pool.insert(it, free_chunk);
			}
		} else {
			// prev_it < free_chunk < it
			auto prev_it = std::prev(it);
			if ((prev_it->addr + prev_it->size) ==  addr) {
				// merge with existing (left) block
				prev_it->size += size;
				if ((prev_it->addr + prev_it->size) == it->addr) {
					// double merge (aka fill a hole)
					prev_it->size += it->size;
					free_pool.erase(it);
				}
			} else if ((addr+size) == it->addr) {
				// merge with existing (right) block
				it->addr  = addr;
				it->size += size;
			} else {
				// no merge, create a new block
				free_pool.insert(it, free_chunk);
			}
		}

#ifdef SELF_CHECK
		QueryTotalFreeMemSize_HLE();
		assert(total == ((v0>>8u) -size));
		debug_check_free_pool();
#endif

		return return_hle(0);
	}

	int QueryMemSize_HLE()
	{
		return return_hle(memsize);
	}

	int QueryTotalFreeMemSize_HLE()
	{
		SYSMEM_LOG("QueryTotalFreeMemSize");
		u32 free_mem = 0;
		for (auto chunk : free_pool) {
			free_mem += chunk.size;
		}
		return return_hle(free_mem << 8u);
	}

	int query_param(u16 addr, enum Query p)
	{
		for (auto chunk : free_pool)
			if (addr >= chunk.addr && addr < (chunk.addr + chunk.size)) {
				u32 val = (p == QUERY_SIZE) ? chunk.size : chunk.addr;
				return return_hle(0x80000000 | (val << 8u));
			}

		for (auto chunk : allocated_pool)
			if (addr >= chunk.addr && addr < (chunk.addr + chunk.size)) {
				u32 val = (p == QUERY_SIZE) ? chunk.size : chunk.addr;
				return return_hle(val << 8u);
			}


		DevCon.Warning("Query Param invalid address!");
		return return_hle(0xFFFFFFFF);
	}

	int QueryBlockSize_HLE()
	{
		return query_param(a0 >> 8u, QUERY_SIZE);
	}

	int QueryBlockTopAddress_HLE()
	{
		return query_param(a0 >> 8u, QUERY_ADDR);
	}

	int QueryMaxFreeMemSize_HLE()
	{
		u32 max = 0;
		for (auto chunk : free_pool)
			max = std::max(max, (u32)chunk.size);

		return return_hle(max << 8u);
	}

	void Reset()
	{
		// Note: maybe we can increase the memory size :p
		init_memory(2*1024*1024);
	}
}

}
