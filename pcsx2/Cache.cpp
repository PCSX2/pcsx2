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

_cacheS pCache[64];

// The lower parts of a cache tags structure is as follows:
// 31 - 12: The physical address cache tag.
// 11 - 7: Unused.
// 6: Dirty flag.
// 5: Valid flag.
// 4: LRF flag - least recently filled flag.
// 3: Lock flag.
// 2-0: Unused.

// 0xFFF - 12 bits, so x & ~0xFFF = the physical address cache tag.

const u32 DIRTY_FLAG = 0x40;
const u32 VALID_FLAG = 0x20;
const u32 LRF_FLAG = 0x10;
const u32 LOCK_FLAG = 0x8;

// TODO: You can't assume the vtlb entry is in both states at once!

int getFreeCache(u32 mem, int mode, int * way)
{
	int number = 0;
	const int i = (mem >> 6) & 0x3F;
	const auto vmv = vtlbdata.vmap[mem >> VTLB_PAGE_BITS];
	uptr ppf = vmv.assumePtr(mem);
	const u32 paddr = vmv.assumeHandlerGetPAddr(mem);

	if((cpuRegs.CP0.n.Config & 0x10000)  == 0) CACHE_LOG("Cache off!");
	
	if ((pCache[i].tag[0] & ~0xFFF) == (paddr & ~0xFFF) && (pCache[i].tag[0] & VALID_FLAG))
	{
		*way = 0;
		if (pCache[i].tag[0] & LOCK_FLAG) CACHE_LOG("Index %x Way %x Locked!!", i, 0);
		return i;
	}
	else if((pCache[i].tag[1] & ~0xFFF) == (paddr & ~0xFFF) && (pCache[i].tag[1] & VALID_FLAG))
	{
		*way = 1;
		if (pCache[i].tag[1] & LOCK_FLAG) CACHE_LOG("Index %x Way %x Locked!!", i, 1);
		return i;
	}

	number = (((pCache[i].tag[0]) & LRF_FLAG) ^ ((pCache[i].tag[1]) & LRF_FLAG)) >> 4;
	ppf = (ppf & ~0x3F); 

	if ((pCache[i].tag[number] & (DIRTY_FLAG | VALID_FLAG)) == (DIRTY_FLAG | VALID_FLAG))	// Dirty Write
	{		
		s32 oldppf = (pCache[i].tag[number] & ~0x80000fff) + (mem & 0xFC0);
		
		CACHE_LOG("Dirty cache fill! PPF %x", oldppf);
		*reinterpret_cast<mem64_t*>(oldppf) = pCache[i].data[number][0].b8._u64[0];
		*reinterpret_cast<mem64_t*>(oldppf+8) =  pCache[i].data[number][0].b8._u64[1];
		*reinterpret_cast<mem64_t*>(oldppf+16) = pCache[i].data[number][1].b8._u64[0];
		*reinterpret_cast<mem64_t*>(oldppf+24) = pCache[i].data[number][1].b8._u64[1];
		*reinterpret_cast<mem64_t*>(oldppf+32) = pCache[i].data[number][2].b8._u64[0];
		*reinterpret_cast<mem64_t*>(oldppf+40) = pCache[i].data[number][2].b8._u64[1];
		*reinterpret_cast<mem64_t*>(oldppf+48) = pCache[i].data[number][3].b8._u64[0];
		*reinterpret_cast<mem64_t*>(oldppf+56) = pCache[i].data[number][3].b8._u64[1];
		pCache[i].tag[number] &= ~DIRTY_FLAG;
	}
	
	pCache[i].data[number][0].b8._u64[0] = *reinterpret_cast<mem64_t*>(ppf);
	pCache[i].data[number][0].b8._u64[1] = *reinterpret_cast<mem64_t*>(ppf+8);
	pCache[i].data[number][1].b8._u64[0] = *reinterpret_cast<mem64_t*>(ppf+16);
	pCache[i].data[number][1].b8._u64[1] = *reinterpret_cast<mem64_t*>(ppf+24);
	pCache[i].data[number][2].b8._u64[0] = *reinterpret_cast<mem64_t*>(ppf+32);
	pCache[i].data[number][2].b8._u64[1] = *reinterpret_cast<mem64_t*>(ppf+40);
	pCache[i].data[number][3].b8._u64[0] = *reinterpret_cast<mem64_t*>(ppf+48);
	pCache[i].data[number][3].b8._u64[1] = *reinterpret_cast<mem64_t*>(ppf+56);

	*way = number;
	pCache[i].tag[number] |= VALID_FLAG;
	pCache[i].tag[number] &= 0xFFF;
	pCache[i].tag[number] |= paddr & ~0xFFF;

	if(pCache[i].tag[number] & LRF_FLAG)
		pCache[i].tag[number] &= ~LRF_FLAG;
	else
		pCache[i].tag[number] |= LRF_FLAG;

	return i;
}

void writeCache8(u32 mem, u8 value)
{
	int number = 0;
	const int i = getFreeCache(mem, 1, &number);

	if (i == -1)
	{
		auto vmv = vtlbdata.vmap[mem >> VTLB_PAGE_BITS];
		*reinterpret_cast<mem8_t*>(vmv.assumePtr(mem)) = value;
		return;
	}

	CACHE_LOG("writeCache8 %8.8x adding to %d, way %d, value %x", mem, i, number, value);
	pCache[i].tag[number] |= DIRTY_FLAG;	// Set Dirty Bit if mode == write
	pCache[i].data[number][(mem >> 4) & 0x3].b8._u8[(mem & 0xf)] = value;
}

void writeCache16(u32 mem, u16 value)
{
	int number = 0;
	const int i = getFreeCache(mem, 1, &number);

	if (i == -1)
	{
		auto vmv = vtlbdata.vmap[mem >> VTLB_PAGE_BITS];
		*reinterpret_cast<mem16_t*>(vmv.assumePtr(mem)) = value;
		return;
	}

	CACHE_LOG("writeCache16 %8.8x adding to %d, way %d, value %x", mem, i, number, value);
	pCache[i].tag[number] |= DIRTY_FLAG;	// Set Dirty Bit if mode == write
	pCache[i].data[number][(mem >> 4) & 0x3].b8._u16[(mem & 0xf) >> 1] = value;
}

void writeCache32(u32 mem, u32 value)
{
	int number = 0;
	const int i = getFreeCache(mem, 1, &number);

	if (i == -1)
	{
		auto vmv = vtlbdata.vmap[mem >> VTLB_PAGE_BITS];
		*reinterpret_cast<mem32_t*>(vmv.assumePtr(mem)) = value;
		return;
	}

	CACHE_LOG("writeCache32 %8.8x adding to %d, way %d, value %x", mem, i, number, value);
	pCache[i].tag[number] |= DIRTY_FLAG;	// Set Dirty Bit if mode == write
	pCache[i].data[number][(mem >> 4) & 0x3].b8._u32[(mem & 0xf) >> 2] = value;
}

void writeCache64(u32 mem, const u64 value)
{
	int number = 0;
	const int i = getFreeCache(mem, 1, &number);

	if (i == -1)
	{
		auto vmv = vtlbdata.vmap[mem >> VTLB_PAGE_BITS];
		*reinterpret_cast<mem64_t*>(vmv.assumePtr(mem)) = value;
		return;
	}

	CACHE_LOG("writeCache64 %8.8x adding to %d, way %d, value %x", mem, i,number,value);
	pCache[i].tag[number] |= DIRTY_FLAG;	// Set Dirty Bit if mode == write
	pCache[i].data[number][(mem >> 4) & 0x3].b8._u64[(mem & 0xf) >> 3] = value;
}

void writeCache128(u32 mem, const mem128_t* value)
{
	int number = 0;
	const int i = getFreeCache(mem, 1, &number);

	if (i == -1)
	{
		auto vmv = vtlbdata.vmap[mem >> VTLB_PAGE_BITS];
		auto ptr = reinterpret_cast<mem64_t*>(vmv.assumePtr(mem));
		ptr[0] = value->lo;
		ptr[1] = value->hi;
		return;
	}

	CACHE_LOG("writeCache128 %8.8x adding to %d way %x tag %x vallo = %x_%x valhi = %x_%x", mem, i, number, pCache[i].tag[number], value->lo, value->hi);
	pCache[i].tag[number] |= DIRTY_FLAG;	// Set Dirty Bit if mode == write
	pCache[i].data[number][(mem>>4) & 0x3].b8._u64[0] = value->lo;
	pCache[i].data[number][(mem>>4) & 0x3].b8._u64[1] = value->hi;
}

u8 readCache8(u32 mem)
{
	int number = 0;
	const int i = getFreeCache(mem, 0, &number);

	if (i == -1)
	{
		auto vmv = vtlbdata.vmap[mem >> VTLB_PAGE_BITS];
		return *reinterpret_cast<u8*>(vmv.assumePtr(mem));
	}

	CACHE_LOG("readCache %8.8x from %d, way %d QW %x u8 part %x Really Reading %x", mem, i, number, (mem >> 4) & 0x3, (mem & 0xf) >> 2, (u32)pCache[i].data[number][(mem >> 4) & 0x3].b8._u8[(mem & 0xf)]);
	return pCache[i].data[number][(mem >> 4) & 0x3].b8._u8[(mem & 0xf)];
}

u16 readCache16(u32 mem)
{
	int number = 0;
	const int i = getFreeCache(mem, 0, &number);

	if (i == -1)
	{
		auto vmv = vtlbdata.vmap[mem >> VTLB_PAGE_BITS];
		return *reinterpret_cast<u16*>(vmv.assumePtr(mem));
	}

	CACHE_LOG("readCache %8.8x from %d, way %d QW %x u16 part %x Really Reading %x", mem, i, number, (mem >> 4) & 0x3, (mem & 0xf) >> 2, (u32)pCache[i].data[number][(mem >> 4) & 0x3].b8._u16[(mem & 0xf) >> 1]);
	return pCache[i].data[number][(mem >> 4) & 0x3].b8._u16[(mem & 0xf) >> 1];
}

u32 readCache32(u32 mem)
{
	int number = 0;
	const int i = getFreeCache(mem, 0, &number);

	if (i == -1)
	{
		auto vmv = vtlbdata.vmap[mem >> VTLB_PAGE_BITS];
		return *reinterpret_cast<u32*>(vmv.assumePtr(mem));
	}

	CACHE_LOG("readCache %8.8x from %d, way %d QW %x u32 part %x Really Reading %x", mem, i, number, (mem >> 4) & 0x3, (mem & 0xf) >> 2, (u32)pCache[i].data[number][(mem >> 4) & 0x3].b8._u32[(mem & 0xf) >> 2]);
	return pCache[i].data[number][(mem >> 4) & 0x3].b8._u32[(mem & 0xf) >> 2];
}

u64 readCache64(u32 mem)
{
	int number = 0;
	int i = getFreeCache(mem, 0, &number);

	if (i == -1)
	{
		auto vmv = vtlbdata.vmap[mem >> VTLB_PAGE_BITS];
		return *reinterpret_cast<u64*>(vmv.assumePtr(mem));
	}

	CACHE_LOG("readCache %8.8x from %d, way %d QW %x u64 part %x Really Reading %x_%x", mem, i, number, (mem >> 4) & 0x3, (mem & 0xf) >> 2, pCache[i].data[number][(mem >> 4) & 0x3].b8._u64[(mem & 0xf) >> 3]);
	return pCache[i].data[number][(mem >> 4) & 0x3].b8._u64[(mem & 0xf) >> 3];
}

__forceinline void clear_cache(int index, int way)
{
	pCache[index].tag[way] &= LRF_FLAG;

	pCache[index].data[way][0].b8._u64[0] = 0;
	pCache[index].data[way][0].b8._u64[1] = 0;

	pCache[index].data[way][1].b8._u64[0] = 0;
	pCache[index].data[way][1].b8._u64[1] = 0;

	pCache[index].data[way][2].b8._u64[0] = 0;
	pCache[index].data[way][2].b8._u64[1] = 0;

	pCache[index].data[way][3].b8._u64[0] = 0;
	pCache[index].data[way][3].b8._u64[1] = 0;
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
		{
			const int index = (addr >> 6) & 0x3F;
			int way = 0;
			const u32 pfnaddr = addr;
			const auto vmv = vtlbdata.vmap[pfnaddr >> VTLB_PAGE_BITS];
			const u32 paddr = vmv.assumeHandlerGetPAddr(pfnaddr);

			if ((paddr & ~0xFFF) == (pCache[index].tag[0] & ~0xfff) && (pCache[index].tag[0] & VALID_FLAG))
			{
				way = 0;
			}
			else if ((paddr & ~0xFFF) == (pCache[index].tag[1] & ~0xfff) && (pCache[index].tag[1] & VALID_FLAG))
			{
				way = 1;
			}
			else
			{
				CACHE_LOG("CACHE DHIN NO HIT addr %x, index %d, phys %x tag0 %x tag1 %x", addr, index, paddr, pCache[index].tag[0], pCache[index].tag[1]);
				return;
			}

			CACHE_LOG("CACHE DHIN addr %x, index %d, way %d, Flags %x OP %x", addr, index, way, pCache[index].tag[way] & 0x78, cpuRegs.code);

			clear_cache(index, way);
			break;
		}

		case 0x18: //DHWBIN (Data Cache Hit WriteBack with Invalidate)
		{
			const int index = (addr >> 6) & 0x3F;
			int way = 0;
			const u32 pfnaddr = addr;
			const auto vmv = vtlbdata.vmap[pfnaddr >> VTLB_PAGE_BITS];
			uptr ppf = vmv.assumePtr(pfnaddr) & ~0x3F;
			const u32 paddr = vmv.assumeHandlerGetPAddr(pfnaddr) & ~0x3f;

			if ((pCache[index].tag[0] & ~0xFFF) == (paddr & ~0xFFF) && (pCache[index].tag[0] & VALID_FLAG))
			{
				way = 0;
			}
			else if ((pCache[index].tag[1] & ~0xFFF) == (paddr & ~0xFFF) && (pCache[index].tag[1] & VALID_FLAG))
			{
				way = 1;
			}
			else
			{
				CACHE_LOG("CACHE DHWBIN NO HIT addr %x, index %d, phys %x tag0 %x tag1 %x", addr, index, paddr, pCache[index].tag[0], pCache[index].tag[1]);
				return;
			}
			
			CACHE_LOG("CACHE DHWBIN addr %x, index %d, phys %x tag0 %x tag1 %x way %x", addr, index, paddr, pCache[index].tag[0], pCache[index].tag[1], way );

			if ((pCache[index].tag[way] & (DIRTY_FLAG|VALID_FLAG)) == (DIRTY_FLAG|VALID_FLAG))	// Dirty
			{
				CACHE_LOG("DHWBIN Dirty WriteBack PPF %x", ppf);

				*reinterpret_cast<mem64_t*>(ppf) = pCache[index].data[way][0].b8._u64[0];
				*reinterpret_cast<mem64_t*>(ppf+8) =  pCache[index].data[way][0].b8._u64[1];
				*reinterpret_cast<mem64_t*>(ppf+16) = pCache[index].data[way][1].b8._u64[0];
				*reinterpret_cast<mem64_t*>(ppf+24) = pCache[index].data[way][1].b8._u64[1];
				*reinterpret_cast<mem64_t*>(ppf+32) = pCache[index].data[way][2].b8._u64[0];
				*reinterpret_cast<mem64_t*>(ppf+40) = pCache[index].data[way][2].b8._u64[1];
				*reinterpret_cast<mem64_t*>(ppf+48) = pCache[index].data[way][3].b8._u64[0];
				*reinterpret_cast<mem64_t*>(ppf+56) = pCache[index].data[way][3].b8._u64[1];
			}

			clear_cache(index, way);
			break;
		}

		case 0x1c: //DHWOIN (Data Cache Hit WriteBack Without Invalidate)
		{
			const int index = (addr >> 6) & 0x3F;
			int way = 0;
			const u32 pfnaddr = (pCache[index].tag[way] & ~0x80000fff) | (addr & 0xfc0);
			const auto vmv = vtlbdata.vmap[pfnaddr >> VTLB_PAGE_BITS];
			uptr ppf = vmv.assumePtr(pfnaddr);
			const u32 paddr = vmv.assumeHandlerGetPAddr(pfnaddr);
			
			CACHE_LOG("CACHE DHWOIN addr %x, index %d, way %d, Flags %x OP %x", addr, index, way, pCache[index].tag[way] & 0x78, cpuRegs.code);

			if ((pCache[index].tag[0] & ~0xFFF) == (paddr & ~0xFFF) && (pCache[index].tag[0] & VALID_FLAG))
			{
				way = 0;
			}
			else if ((pCache[index].tag[1] & ~0xFFF) == (paddr & ~0xFFF) && (pCache[index].tag[1] & VALID_FLAG))
			{
				way = 1;
			}
			else
			{
				CACHE_LOG("CACHE DHWOIN NO HIT addr %x, index %d, phys %x tag0 %x tag1 %x", addr, index, paddr, pCache[index].tag[0], pCache[index].tag[1]);
				return;
			}

			if ((pCache[index].tag[way] & (DIRTY_FLAG|VALID_FLAG)) == (DIRTY_FLAG|VALID_FLAG))	// Dirty
			{
				CACHE_LOG("DHWOIN Dirty WriteBack! PPF %x", ppf);
				*reinterpret_cast<mem64_t*>(ppf) = pCache[index].data[way][0].b8._u64[0];
				*reinterpret_cast<mem64_t*>(ppf+8) =  pCache[index].data[way][0].b8._u64[1];
				*reinterpret_cast<mem64_t*>(ppf+16) = pCache[index].data[way][1].b8._u64[0];
				*reinterpret_cast<mem64_t*>(ppf+24) = pCache[index].data[way][1].b8._u64[1];
				*reinterpret_cast<mem64_t*>(ppf+32) = pCache[index].data[way][2].b8._u64[0];
				*reinterpret_cast<mem64_t*>(ppf+40) = pCache[index].data[way][2].b8._u64[1];
				*reinterpret_cast<mem64_t*>(ppf+48) = pCache[index].data[way][3].b8._u64[0];
				*reinterpret_cast<mem64_t*>(ppf+56) = pCache[index].data[way][3].b8._u64[1];

				pCache[index].tag[way] &= ~DIRTY_FLAG;
			}
			break;
		}

		case 0x16: //DXIN (Data Cache Index Invalidate)
		{
			const int index = (addr >> 6) & 0x3F;
			const int way = addr & 0x1;

			CACHE_LOG("CACHE DXIN addr %x, index %d, way %d, flag %x\n", addr, index, way, pCache[index].tag[way] & 0x78);

			clear_cache(index, way);
			break;
		}

		case 0x11: //DXLDT (Data Cache Load Data into TagLo)
		{
			const int index = (addr >> 6) & 0x3F;
			const int way = addr & 0x1;

			cpuRegs.CP0.n.TagLo = pCache[index].data[way][(addr >> 4) & 0x3].b8._u32[(addr & 0xf) >> 2];

			CACHE_LOG("CACHE DXLDT addr %x, index %d, way %d, DATA %x OP %x", addr, index, way, cpuRegs.CP0.r[28], cpuRegs.code);
			break;
		}

		case 0x10: //DXLTG (Data Cache Load Tag into TagLo)
		{
			const int index = (addr >> 6) & 0x3F;
			const int way = addr & 0x1;

			//DXLTG demands that SYNC.L is called before this command, which forces the cache to write back, so presumably games are checking the cache has updated the memory
			//For speed, we will do it here.
			const u32 pfnaddr = (pCache[index].tag[way] & ~0x80000fff) | (addr & 0xfc0);
			const auto vmv = vtlbdata.vmap[pfnaddr >> VTLB_PAGE_BITS];
			s32 ppf = vmv.assumePtr(pfnaddr);

			if ((pCache[index].tag[way] & (DIRTY_FLAG | VALID_FLAG)) == (DIRTY_FLAG | VALID_FLAG))	// Dirty
			{
				CACHE_LOG("DXLTG Dirty WriteBack! PPF %x", ppf);
				ppf = (ppf & 0x7fffffff);
				*reinterpret_cast<mem64_t*>(ppf) = pCache[index].data[way][0].b8._u64[0];
				*reinterpret_cast<mem64_t*>(ppf + 8) = pCache[index].data[way][0].b8._u64[1];
				*reinterpret_cast<mem64_t*>(ppf + 16) = pCache[index].data[way][1].b8._u64[0];
				*reinterpret_cast<mem64_t*>(ppf + 24) = pCache[index].data[way][1].b8._u64[1];
				*reinterpret_cast<mem64_t*>(ppf + 32) = pCache[index].data[way][2].b8._u64[0];
				*reinterpret_cast<mem64_t*>(ppf + 40) = pCache[index].data[way][2].b8._u64[1];
				*reinterpret_cast<mem64_t*>(ppf + 48) = pCache[index].data[way][3].b8._u64[0];
				*reinterpret_cast<mem64_t*>(ppf + 56) = pCache[index].data[way][3].b8._u64[1];

				pCache[index].tag[way] &= ~DIRTY_FLAG;
			}
			//DevCon.Warning("DXLTG way %x index %x addr %x tagdata=%x", way, index, addr, pCache[index].tag[way]);
			cpuRegs.CP0.n.TagLo = pCache[index].tag[way];

			CACHE_LOG("CACHE DXLTG addr %x, index %d, way %d, DATA %x OP %x ", addr, index, way, cpuRegs.CP0.r[28], cpuRegs.code);
			break;
		}

		case 0x13: //DXSDT (Data Cache Store 32bits from TagLo)
		{
			const int index = (addr >> 6) & 0x3F;
			const int way = addr & 0x1;

			pCache[index].data[way][(addr >> 4) & 0x3].b8._u32[(addr & 0xf) >> 2] = cpuRegs.CP0.n.TagLo;

			CACHE_LOG("CACHE DXSDT addr %x, index %d, way %d, DATA %x OP %x", addr, index, way, cpuRegs.CP0.r[28], cpuRegs.code);
			break;
		}

		case 0x12: //DXSTG (Data Cache Store Tag from TagLo)
		{
			const int index = (addr >> 6) & 0x3F;
			const int way = addr & 0x1;
			pCache[index].tag[way] = cpuRegs.CP0.n.TagLo; 

			CACHE_LOG("CACHE DXSTG addr %x, index %d, way %d, DATA %x OP %x", addr, index, way, cpuRegs.CP0.r[28] & 0x6F, cpuRegs.code);
			break;
		}

		case 0x14: //DXWBIN (Data Cache Index WriteBack Invalidate)
		{
			const int index = (addr >> 6) & 0x3F;
			const int way = addr & 0x1;
			const u32 pfnaddr = (pCache[index].tag[way] & ~0x80000fff) + (addr & 0xFC0);
			const auto vmv = vtlbdata.vmap[pfnaddr >> VTLB_PAGE_BITS];
			s32 ppf = vmv.assumePtr(pfnaddr);
			const u32 paddr = vmv.assumeHandlerGetPAddr(pfnaddr);

			CACHE_LOG("CACHE DXWBIN addr %x, index %d, way %d, Flags %x Paddr %x tag %x", addr, index, way, pCache[index].tag[way] & 0x78, paddr, pCache[index].tag[way]);
			if ((pCache[index].tag[way] & (DIRTY_FLAG|VALID_FLAG)) == (DIRTY_FLAG|VALID_FLAG))	// Dirty
			{		
				ppf = (ppf & 0x7fffffff);
				CACHE_LOG("DXWBIN Dirty WriteBack! PPF %x", ppf);

				*reinterpret_cast<mem64_t*>(ppf) = pCache[index].data[way][0].b8._u64[0];
				*reinterpret_cast<mem64_t*>(ppf+8) =  pCache[index].data[way][0].b8._u64[1];
				*reinterpret_cast<mem64_t*>(ppf+16) = pCache[index].data[way][1].b8._u64[0];
				*reinterpret_cast<mem64_t*>(ppf+24) = pCache[index].data[way][1].b8._u64[1];
				*reinterpret_cast<mem64_t*>(ppf+32) = pCache[index].data[way][2].b8._u64[0];
				*reinterpret_cast<mem64_t*>(ppf+40) = pCache[index].data[way][2].b8._u64[1];
				*reinterpret_cast<mem64_t*>(ppf+48) = pCache[index].data[way][3].b8._u64[0];
				*reinterpret_cast<mem64_t*>(ppf+56) = pCache[index].data[way][3].b8._u64[1];
			}

			clear_cache(index, way);
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
