// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

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
	};

	struct CacheTag
	{
		uptr rawValue;
		// You are able to configure a TLB entry with non-existant physical address without causing a bus error.
		// When this happens, the cache still fills with the data and when it gets evicted the data is lost.
		// We don't emulate memory access on a logic level, so we need to ensure that we don't try to load/store to a non-existant physical address.
		// This fixes the Find My Own Way demo.
		bool validPFN = true;
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

		bool isValid() const { return rawValue & VALID_FLAG; }
		bool isDirty() const { return rawValue & DIRTY_FLAG; }
		bool lrf() const { return rawValue & LRF_FLAG; }
		bool isLocked() const { return rawValue & LOCK_FLAG; }

		bool isDirtyAndValid() const
		{
			return (rawValue & (DIRTY_FLAG | VALID_FLAG)) == (DIRTY_FLAG | VALID_FLAG);
		}

		void setValid() { rawValue |= VALID_FLAG; }
		void setDirty() { rawValue |= DIRTY_FLAG; }
		void setLocked() { rawValue |= LOCK_FLAG; }
		void clearValid() { rawValue &= ~VALID_FLAG; }
		void clearDirty() { rawValue &= ~DIRTY_FLAG; }
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
			if (tag.validPFN)
				*reinterpret_cast<CacheData*>(target) = data;
			tag.clearDirty();
		}

		void load(uptr ppf)
		{
			pxAssertMsg(!tag.isDirtyAndValid(), "Loaded a value into cache without writing back the old one!");

			tag.setAddr(ppf);
			if (!tag.validPFN)
			{
				// Reading from invalid physical addresses seems to return 0 on hardware
				std::memset(&data, 0, sizeof(data));
			}
			else
			{
				std::memcpy(&data, reinterpret_cast<void*>(ppf & ~0x3FULL), sizeof(data));
			}

			tag.setValid();
			tag.clearDirty();
		}

		void clear()
		{
			tag.clear();
			std::memset(&data, 0, sizeof(data));
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
			return {sets[idx].tags[way], sets[idx].data[way], idx};
		}
	};

	static Cache cache = {};
} // namespace

void resetCache()
{
	std::memset(&cache, 0, sizeof(cache));
}

static bool findInCache(const CacheSet& set, uptr ppf, int* way)
{
	auto check = [&](int checkWay) -> bool {
		if (!set.tags[checkWay].matches(ppf))
			return false;

		*way = checkWay;
		return true;
	};

	return check(0) || check(1);
}

static int getFreeCache(u32 mem, int* way, bool validPFN)
{
	const int setIdx = cache.setIdxFor(mem);
	CacheSet& set = cache.sets[setIdx];
	VTLBVirtual vmv = vtlbdata.vmap[mem >> VTLB_PAGE_BITS];

	*way = set.tags[0].lrf() ^ set.tags[1].lrf();
	if (validPFN)
		pxAssertMsg(!vmv.isHandler(mem), "Cache currently only supports non-handler addresses!");

	uptr ppf = vmv.assumePtr(mem);

	[[unlikely]]
	if ((cpuRegs.CP0.n.Config & 0x10000) == 0)
		CACHE_LOG("Cache off!");

	if (findInCache(set, ppf, way))
	{
		[[unlikely]]
		if (set.tags[*way].isLocked())
		{
			// Check the other way
			if (set.tags[*way ^ 1].isLocked())
			{
				Console.Error("CACHE: SECOND WAY IS LOCKED.", setIdx, *way);
			}
			else
			{
				// Force the unlocked way
				*way ^= 1;
			}
		}
	}
	else
	{
		int newWay = set.tags[0].lrf() ^ set.tags[1].lrf();
		[[unlikely]]
		if (set.tags[newWay].isLocked())
		{
			// If the new way is locked, we force the unlocked way, ignoring the lrf bits.
			newWay = newWay ^ 1;
			[[unlikely]]
			if (set.tags[newWay].isLocked())
			{
				Console.Warning("CACHE: SECOND WAY IS LOCKED.", setIdx, *way);
			}
		}
		*way = newWay;

		CacheLine line = cache.lineAt(setIdx, newWay);
		line.writeBackIfNeeded();
		line.tag.validPFN = validPFN;
		line.load(ppf);
		line.tag.toggleLRF();
	}

	return setIdx;
}

template <bool Write, int Bytes>
void* prepareCacheAccess(u32 mem, int* way, int* idx, bool validPFN = true)
{
	*way = 0;
	*idx = getFreeCache(mem, way, validPFN);
	CacheLine line = cache.lineAt(*idx, *way);
	if (Write)
		line.tag.setDirty();
	u32 aligned = mem & ~(Bytes - 1);
	return &line.data.bytes[aligned & 0x3f];
}

template <typename Int>
void writeCache(u32 mem, Int value, bool validPFN)
{
	int way, idx;
	void* addr = prepareCacheAccess<true, sizeof(Int)>(mem, &way, &idx, validPFN);

	CACHE_LOG("writeCache%d %8.8x adding to %d, way %d, value %llx", 8 * sizeof(value), mem, idx, way, value);
	*reinterpret_cast<Int*>(addr) = value;
}

void writeCache8(u32 mem, u8 value, bool validPFN)
{
	writeCache<u8>(mem, value, validPFN);
}

void writeCache16(u32 mem, u16 value, bool validPFN)
{
	writeCache<u16>(mem, value, validPFN);
}

void writeCache32(u32 mem, u32 value, bool validPFN)
{
	writeCache<u32>(mem, value, validPFN);
}

void writeCache64(u32 mem, const u64 value, bool validPFN)
{
	writeCache<u64>(mem, value, validPFN);
}

void writeCache128(u32 mem, const mem128_t* value, bool validPFN)
{
	int way, idx;
	void* addr = prepareCacheAccess<true, sizeof(mem128_t)>(mem, &way, &idx, validPFN);

	CACHE_LOG("writeCache128 %8.8x adding to %d, way %x, lo %llx, hi %llx", mem, idx, way, value->lo, value->hi);
	*reinterpret_cast<mem128_t*>(addr) = *value;
}

template <typename Int>
Int readCache(u32 mem, bool validPFN)
{
	int way, idx;
	void* addr = prepareCacheAccess<false, sizeof(Int)>(mem, &way, &idx, validPFN);

	Int value = *reinterpret_cast<Int*>(addr);
	CACHE_LOG("readCache%d %8.8x from %d, way %d, value %llx", 8 * sizeof(value), mem, idx, way, value);
	return value;
}


u8 readCache8(u32 mem, bool validPFN)
{
	return readCache<u8>(mem, validPFN);
}

u16 readCache16(u32 mem, bool validPFN)
{
	return readCache<u16>(mem, validPFN);
}

u32 readCache32(u32 mem, bool validPFN)
{
	return readCache<u32>(mem, validPFN);
}

u64 readCache64(u32 mem, bool validPFN)
{
	return readCache<u64>(mem, validPFN);
}

RETURNS_R128 readCache128(u32 mem, bool validPFN)
{
	int way, idx;
	void* addr = prepareCacheAccess<false, sizeof(mem128_t)>(mem, &way, &idx, validPFN);
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

namespace R5900
{
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
						doCacheHitOp(addr, "DHIN", [](CacheLine line) {
							line.clear();
						});
						break;

					case 0x18: //DHWBIN (Data Cache Hit WriteBack with Invalidate)
						doCacheHitOp(addr, "DHWBIN", [](CacheLine line) {
							line.writeBackIfNeeded();
							line.clear();
						});
						break;

					case 0x1c: //DHWOIN (Data Cache Hit WriteBack Without Invalidate)
						doCacheHitOp(addr, "DHWOIN", [](CacheLine line) {
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

                        line.tag.setAddr(cpuRegs.CP0.n.TagLo);
						line.tag.rawValue &= ~CacheTag::ALL_FLAGS;
						line.tag.rawValue |= (cpuRegs.CP0.n.TagLo & CacheTag::ALL_FLAGS);

						CACHE_LOG("CACHE DXSTG addr %x, index %d, way %d, DATA %x OP %x", addr, index, way, cpuRegs.CP0.n.TagLo, cpuRegs.code);
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
		} // end namespace OpcodeImpl

	} // namespace Interpreter
} // namespace R5900
