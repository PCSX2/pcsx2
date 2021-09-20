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


#include "PrecompiledHeader.h"
#include "Common.h"
#include "Cache.h"
#include "vtlb.h"

using namespace R5900;
using namespace vtlb_private;

namespace
{

	union alignas(64) CacheData
	{
		u8 bytes[64];

		constexpr CacheData(): bytes{0} {}
	};

	struct CacheTag
	{
		uptr rawValue = 0;

		CacheTag() = default;

		// The lower parts of a cache tags structure is as follows:
		// 31 - 12: The physical address cache tag.
		// 11 - 7: Unused.
		// 6: Dirty flag.
		// 5: Valid flag.
		// 4: LRF flag - least recently filled flag.
		// 3: Lock flag.
		// 2-0: Unused.

		enum Flags : decltype(rawValue)
		{
			DIRTY_FLAG = 0x40,
			VALID_FLAG = 0x20,
			LRF_FLAG = 0x10,
			LOCK_FLAG = 0x8,
			ALL_FLAGS = 0xFFF
		};

		int flags() const
		{
			return rawValue & ALL_FLAGS;
		}

		bool isValid() const  { return rawValue & VALID_FLAG; }
		bool isDirty() const  { return rawValue & DIRTY_FLAG; }
		bool lrf() const      { return rawValue & LRF_FLAG; }
		bool isLocked() const { return rawValue & LOCK_FLAG; }

		bool isDirtyAndValid() const
		{
			return (rawValue & (DIRTY_FLAG | VALID_FLAG)) == (DIRTY_FLAG | VALID_FLAG);
		}

		void setValid()  { rawValue |= VALID_FLAG; }
		void setDirty()  { rawValue |= DIRTY_FLAG; }
		void setLocked() { rawValue |= LOCK_FLAG; }
		void clearValid()  { rawValue &= ~VALID_FLAG; }
		void clearDirty()  { rawValue &= ~DIRTY_FLAG; }
		void clearLocked() { rawValue &= ~LOCK_FLAG; }
		void toggleLRF() { rawValue ^= LRF_FLAG; }

		uptr addr() const { return rawValue & ~ALL_FLAGS; }

		void setAddr(uptr addr)
		{
			rawValue &= ALL_FLAGS;
			rawValue |= (addr & ~ALL_FLAGS);
		}

		bool matches(uptr other) const
		{
			return isValid() && addr() == (other & ~ALL_FLAGS);
		}

		void clear()
		{
			rawValue &= LRF_FLAG;
		}
	};

	struct CacheLine
	{
		CacheTag& tag;
		CacheData& data;
		int set;

		uptr addr()
		{
			return tag.addr() | (set << 6);
		}

		void writeBackIfNeeded()
		{
			if (!tag.isDirtyAndValid())
				return;

			uptr target = addr();

			CACHE_LOG("Write back at %zx", target);
			*reinterpret_cast<CacheData*>(target) = data;
			tag.clearDirty();
		}

		void load(uptr ppf)
		{
			pxAssertMsg(!tag.isDirtyAndValid(), "Loaded a value into cache without writing back the old one!");

			tag.setAddr(ppf);
			data = *reinterpret_cast<CacheData*>(ppf & ~0x3FULL);
			tag.setValid();
			tag.clearDirty();
		}

		void clear()
		{
			tag.clear();
			data = CacheData();
		}
	};

	struct CacheSet
	{
		CacheTag tags[2];
		CacheData data[2];
	};

	struct Cache
	{
		CacheSet sets[64];

		int setIdxFor(u32 vaddr) const
		{
			return (vaddr >> 6) & 0x3F;
		}

		CacheLine lineAt(int idx, int way)
		{
			return { sets[idx].tags[way], sets[idx].data[way], idx };
		}
	};

	static Cache cache;

}

void resetCache()
{
	memzero(cache);
}

static bool findInCache(const CacheSet& set, uptr ppf, int* way)
{
	auto check = [&](int checkWay) -> bool
	{
		if (!set.tags[checkWay].matches(ppf))
			return false;

		*way = checkWay;
		return true;
	};

	return check(0) || check(1);
}

static int getFreeCache(u32 mem, int* way)
{
	const int setIdx = cache.setIdxFor(mem);
	CacheSet& set = cache.sets[setIdx];
	VTLBVirtual vmv = vtlbdata.vmap[mem >> VTLB_PAGE_BITS];
	pxAssertMsg(!vmv.isHandler(mem), "Cache currently only supports non-handler addresses!");
	uptr ppf = vmv.assumePtr(mem);

	if((cpuRegs.CP0.n.Config & 0x10000) == 0)
		CACHE_LOG("Cache off!");

	if (findInCache(set, ppf, way))
	{
		if (set.tags[*way].isLocked())
			CACHE_LOG("Index %x Way %x Locked!!", setIdx, *way);
	}
	else
	{
		int newWay = set.tags[0].lrf() ^ set.tags[1].lrf();
		*way = newWay;
		CacheLine line = cache.lineAt(setIdx, newWay);

		line.writeBackIfNeeded();
		line.load(ppf);
		line.tag.toggleLRF();
	}

	return setIdx;
}

template <bool Write, int Bytes>
void* prepareCacheAccess(u32 mem, int* way, int* idx)
{
	*way = 0;
	*idx = getFreeCache(mem, way);
	CacheLine line = cache.lineAt(*idx, *way);
	if (Write)
		line.tag.setDirty();
	u32 aligned = mem & ~(Bytes - 1);
	return &line.data.bytes[aligned & 0x3f];
}

template <typename Int>
void writeCache(u32 mem, Int value)
{
	int way, idx;
	void* addr = prepareCacheAccess<true, sizeof(Int)>(mem, &way, &idx);

	CACHE_LOG("writeCache%d %8.8x adding to %d, way %d, value %llx", 8 * sizeof(value), mem, idx, way, value);
	*reinterpret_cast<Int*>(addr) = value;
}

void writeCache8(u32 mem, u8 value)
{
	writeCache<u8>(mem, value);
}

void writeCache16(u32 mem, u16 value)
{
	writeCache<u16>(mem, value);
}

void writeCache32(u32 mem, u32 value)
{
	writeCache<u32>(mem, value);
}

void writeCache64(u32 mem, const u64 value)
{
	writeCache<u64>(mem, value);
}

void writeCache128(u32 mem, const mem128_t* value)
{
	int way, idx;
	void* addr = prepareCacheAccess<true, sizeof(mem128_t)>(mem, &way, &idx);

	CACHE_LOG("writeCache128 %8.8x adding to %d, way %x, lo %llx, hi %llx", mem, idx, way, value->lo, value->hi);
	*reinterpret_cast<mem128_t*>(addr) = *value;
}

template <typename Int>
Int readCache(u32 mem)
{
	int way, idx;
	void* addr = prepareCacheAccess<false, sizeof(Int)>(mem, &way, &idx);

	Int value = *reinterpret_cast<Int*>(addr);
	CACHE_LOG("readCache%d %8.8x from %d, way %d, value %llx", 8 * sizeof(value), mem, idx, way, value);
	return value;
}


u8 readCache8(u32 mem)
{
	return readCache<u8>(mem);
}

u16 readCache16(u32 mem)
{
	return readCache<u16>(mem);
}

u32 readCache32(u32 mem)
{
	return readCache<u32>(mem);
}

RETURNS_R64 readCache64(u32 mem)
{
	int way, idx;
	void* addr = prepareCacheAccess<false, sizeof(u64)>(mem, &way, &idx);
	r64 value = r64_load(addr);
	CACHE_LOG("readCache64 %8.8x from %d, way %d, value %llx", mem, idx, way, *(u64*)&value);
	return value;
}

RETURNS_R128 readCache128(u32 mem)
{
	int way, idx;
	void* addr = prepareCacheAccess<false, sizeof(mem128_t)>(mem, &way, &idx);
	r128 value = r128_load(addr);
	u64* vptr = reinterpret_cast<u64*>(&value);
	CACHE_LOG("readCache128 %8.8x from %d, way %d, lo %llx, hi %llx", mem, idx, way, vptr[0], vptr[1]);
	return value;
}

template <typename Op>
void doCacheHitOp(u32 addr, const char* name, Op op)
{
	const int index = cache.setIdxFor(addr);
	CacheSet& set = cache.sets[index];
	VTLBVirtual vmv = vtlbdata.vmap[addr >> VTLB_PAGE_BITS];
	uptr ppf = vmv.assumePtr(addr);
	int way;

	if (!findInCache(set, ppf, &way))
	{
		CACHE_LOG("CACHE %s NO HIT addr %x, index %d, tag0 %zx tag1 %zx", name, addr, index, set.tags[0].rawValue, set.tags[1].rawValue);
		return;
	}

	CACHE_LOG("CACHE %s addr %x, index %d, way %d, flags %x OP %x", name, addr, index, way, set.tags[way].flags(), cpuRegs.code);

	op(cache.lineAt(index, way));
}

namespace R5900 {
namespace Interpreter
{
namespace OpcodeImpl
{

extern int Dcache;
void CACHE()
{
	u32 addr = cpuRegs.GPR.r[_Rs_].UL[0] + _Imm_;
	// CACHE_LOG("cpuRegs.GPR.r[_Rs_].UL[0] = %x, IMM = %x RT = %x", cpuRegs.GPR.r[_Rs_].UL[0], _Imm_, _Rt_);

	switch (_Rt_) 
	{
		case 0x1a: //DHIN (Data Cache Hit Invalidate)
			doCacheHitOp(addr, "DHIN", [](CacheLine line)
			{
				line.clear();
			});
			break;

		case 0x18: //DHWBIN (Data Cache Hit WriteBack with Invalidate)
			doCacheHitOp(addr, "DHWBIN", [](CacheLine line)
			{
				line.writeBackIfNeeded();
				line.clear();
			});
			break;

		case 0x1c: //DHWOIN (Data Cache Hit WriteBack Without Invalidate)
			doCacheHitOp(addr, "DHWOIN", [](CacheLine line)
			{
				line.writeBackIfNeeded();
			});
			break;

		case 0x16: //DXIN (Data Cache Index Invalidate)
		{
			const int index = cache.setIdxFor(addr);
			const int way = addr & 0x1;
			CacheLine line = cache.lineAt(index, way);

			CACHE_LOG("CACHE DXIN addr %x, index %d, way %d, flag %x", addr, index, way, line.tag.flags());

			line.clear();
			break;
		}

		case 0x11: //DXLDT (Data Cache Load Data into TagLo)
		{
			const int index = cache.setIdxFor(addr);
			const int way = addr & 0x1;
			CacheLine line = cache.lineAt(index, way);

			cpuRegs.CP0.n.TagLo = *reinterpret_cast<u32*>(&line.data.bytes[addr & 0x3C]);

			CACHE_LOG("CACHE DXLDT addr %x, index %d, way %d, DATA %x OP %x", addr, index, way, cpuRegs.CP0.n.TagLo, cpuRegs.code);
			break;
		}

		case 0x10: //DXLTG (Data Cache Load Tag into TagLo)
		{
			const int index = (addr >> 6) & 0x3F;
			const int way = addr & 0x1;
			CacheLine line = cache.lineAt(index, way);

			// DXLTG demands that SYNC.L is called before this command, which forces the cache to write back, so presumably games are checking the cache has updated the memory
			// For speed, we will do it here.
			line.writeBackIfNeeded();

			// Our tags don't contain PS2 paddrs (instead they contain x86 addrs)
			cpuRegs.CP0.n.TagLo = line.tag.flags();

			CACHE_LOG("CACHE DXLTG addr %x, index %d, way %d, DATA %x OP %x ", addr, index, way, cpuRegs.CP0.n.TagLo, cpuRegs.code);
			CACHE_LOG("WARNING: DXLTG emulation supports flags only, things could break");
			break;
		}

		case 0x13: //DXSDT (Data Cache Store 32bits from TagLo)
		{
			const int index = (addr >> 6) & 0x3F;
			const int way = addr & 0x1;
			CacheLine line = cache.lineAt(index, way);

			*reinterpret_cast<u32*>(&line.data.bytes[addr & 0x3C]) = cpuRegs.CP0.n.TagLo;

			CACHE_LOG("CACHE DXSDT addr %x, index %d, way %d, DATA %x OP %x", addr, index, way, cpuRegs.CP0.n.TagLo, cpuRegs.code);
			break;
		}

		case 0x12: //DXSTG (Data Cache Store Tag from TagLo)
		{
			const int index = (addr >> 6) & 0x3F;
			const int way = addr & 0x1;
			CacheLine line = cache.lineAt(index, way);

			line.tag.rawValue &= ~CacheTag::ALL_FLAGS;
			line.tag.rawValue |= (cpuRegs.CP0.n.TagLo & CacheTag::ALL_FLAGS);

			CACHE_LOG("CACHE DXSTG addr %x, index %d, way %d, DATA %x OP %x", addr, index, way, cpuRegs.CP0.n.TagLo, cpuRegs.code);
			CACHE_LOG("WARNING: DXSTG emulation supports flags only, things will probably break");
			break;
		}

		case 0x14: //DXWBIN (Data Cache Index WriteBack Invalidate)
		{
			const int index = (addr >> 6) & 0x3F;
			const int way = addr & 0x1;
			CacheLine line = cache.lineAt(index, way);

			CACHE_LOG("CACHE DXWBIN addr %x, index %d, way %d, flags %x paddr %zx", addr, index, way, line.tag.flags(), line.addr());
			line.writeBackIfNeeded();
			line.clear();
			break;
		}

		case 0x7: //IXIN (Instruction Cache Index Invalidate)
		{
			//Not Implemented as we do not have instruction cache
			break;
		}

		case 0xC: //BFH (BTAC Flush)
		{
			//Not Implemented as we do not cache Branch Target Addresses.
			break;
		}

		default:
			DevCon.Warning("Cache mode %x not implemented", _Rt_);
			break;
	}
}
}		// end namespace OpcodeImpl

}}
