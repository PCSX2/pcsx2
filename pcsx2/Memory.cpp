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

/*

RAM
---
0x00100000-0x01ffffff this is the physical address for the ram.its cached there
0x20100000-0x21ffffff uncached
0x30100000-0x31ffffff uncached & accelerated
0xa0000000-0xa1ffffff MIRROR might...???
0x80000000-0x81ffffff MIRROR might... ????

scratch pad
----------
0x70000000-0x70003fff scratch pad

BIOS
----
0x1FC00000 - 0x1FFFFFFF un-cached
0x9FC00000 - 0x9FFFFFFF cached
0xBFC00000 - 0xBFFFFFFF un-cached
*/

#include "PrecompiledHeader.h"

#include "IopHw.h"
#include "GS.h"
#include "VUmicro.h"
#include "MTVU.h"
#include "DEV9/DEV9.h"

#include "ps2/HwInternal.h"
#include "ps2/BiosTools.h"
#include "SPU2/spu2.h"

#include "common/AlignedMalloc.h"

#ifdef ENABLECACHE
#include "Cache.h"
#endif

int MemMode = 0;		// 0 is Kernel Mode, 1 is Supervisor Mode, 2 is User Mode

void memSetKernelMode() {
	//Do something here
	MemMode = 0;
}

void memSetSupervisorMode() {
}

void memSetUserMode() {

}

u16 ba0R16(u32 mem)
{
	//MEM_LOG("ba00000 Memory read16 address %x", mem);

	if (mem == 0x1a000006) {
		static int ba6;
		ba6++;
		if (ba6 == 3) ba6 = 0;
		return ba6;
	}
	return 0;
}

#define CHECK_MEM(mem) //MyMemCheck(mem)

void MyMemCheck(u32 mem)
{
    if( mem == 0x1c02f2a0 )
        Console.WriteLn("yo; (mem == 0x1c02f2a0) in MyMemCheck...");
}

/////////////////////////////
// REGULAR MEM START
/////////////////////////////
static vtlbHandler
	null_handler,

	tlb_fallback_0,
	tlb_fallback_2,
	tlb_fallback_3,
	tlb_fallback_4,
	tlb_fallback_5,
	tlb_fallback_6,
	tlb_fallback_7,
	tlb_fallback_8,

	vu0_micro_mem,
	vu1_micro_mem,
	vu1_data_mem,

	hw_by_page[0x10] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},

	gs_page_0,
	gs_page_1,

	iopHw_by_page_01,
	iopHw_by_page_03,
	iopHw_by_page_08;


void memMapVUmicro()
{
	// VU0/VU1 micro mem (instructions)
	// (Like IOP memory, these are generally only used by the EE Bios kernel during
	//  boot-up.  Applications/games are "supposed" to use the thread-safe VIF instead;
	//  or must ensure all VIF/GIF transfers are finished and all VUmicro execution stopped
	//  prior to accessing VU memory directly).

	// The VU0 mapping actually repeats 4 times across the mapped range, but we don't bother
	// to manually mirror it here because the indirect memory handler for it (see vuMicroRead*
	// functions below) automatically mask and wrap the address for us.

	vtlb_MapHandler(vu0_micro_mem,0x11000000,0x00004000);
	vtlb_MapHandler(vu1_micro_mem,0x11008000,0x00004000);

	// VU0/VU1 memory (data)
	// VU0 is 4k, mirrored 4 times across a 16k area.
	vtlb_MapBlock(VU0.Mem,0x11004000,0x00004000,0x1000);
	// Note: In order for the below conditional to work correctly
	// support needs to be coded to reset the memMappings when MTVU is
	// turned off/on. For now we just always use the vu data handlers...
	if (1||THREAD_VU1) vtlb_MapHandler(vu1_data_mem,0x1100c000,0x00004000);
	else               vtlb_MapBlock  (VU1.Mem,     0x1100c000,0x00004000);
}

void memMapPhy()
{
	// Main memory
	vtlb_MapBlock(eeMem->Main,	0x00000000,Ps2MemSize::MainRam);//mirrored on first 256 mb ?
	// High memory, uninstalled on the configuration we emulate
	vtlb_MapHandler(null_handler, Ps2MemSize::MainRam, 0x10000000 - Ps2MemSize::MainRam);

	// Various ROMs (all read-only)
	vtlb_MapBlock(eeMem->ROM,	0x1fc00000, Ps2MemSize::Rom);
	vtlb_MapBlock(eeMem->ROM1,	0x1e000000, Ps2MemSize::Rom1);
	vtlb_MapBlock(eeMem->ROM2,	0x1e400000, Ps2MemSize::Rom2);

	// IOP memory
	// (used by the EE Bios Kernel during initial hardware initialization, Apps/Games
	//  are "supposed" to use the thread-safe SIF instead.)
	vtlb_MapBlock(iopMem->Main,0x1c000000,0x00800000);

	// Generic Handlers; These fallback to mem* stuff...
	vtlb_MapHandler(tlb_fallback_7,0x14000000, _64kb);
	vtlb_MapHandler(tlb_fallback_4,0x18000000, _64kb);
	vtlb_MapHandler(tlb_fallback_5,0x1a000000, _64kb);
	vtlb_MapHandler(tlb_fallback_6,0x12000000, _64kb);
	vtlb_MapHandler(tlb_fallback_8,0x1f000000, _64kb);
	vtlb_MapHandler(tlb_fallback_3,0x1f400000, _64kb);
	vtlb_MapHandler(tlb_fallback_2,0x1f800000, _64kb);
	vtlb_MapHandler(tlb_fallback_8,0x1f900000, _64kb);

	// Hardware Register Handlers : specialized/optimized per-page handling of HW register accesses
	// (note that hw_by_page handles are assigned in memReset prior to calling this function)

	for( uint i=0; i<16; ++i)
		vtlb_MapHandler(hw_by_page[i], 0x10000000 + (0x01000 * i), 0x01000);

	vtlb_MapHandler(gs_page_0, 0x12000000, 0x01000);
	vtlb_MapHandler(gs_page_1, 0x12001000, 0x01000);

	// "Secret" IOP HW mappings - Used by EE Bios Kernel during boot and generally
	// left untouched after that, as per EE/IOP thread safety rules.

	vtlb_MapHandler(iopHw_by_page_01, 0x1f801000, 0x01000);
	vtlb_MapHandler(iopHw_by_page_03, 0x1f803000, 0x01000);
	vtlb_MapHandler(iopHw_by_page_08, 0x1f808000, 0x01000);

}

//Why is this required ?
void memMapKernelMem()
{
	//lower 512 mb: direct map
	//vtlb_VMap(0x00000000,0x00000000,0x20000000);
	//0x8* mirror
	vtlb_VMap(0x80000000, 0x00000000, _1mb*512);
	//0xa* mirror
	vtlb_VMap(0xA0000000, 0x00000000, _1mb*512);
}

//what do do with these ?
void memMapSupervisorMem()
{
}

void memMapUserMem()
{
}

static mem8_t nullRead8(u32 mem) {
	MEM_LOG("Read uninstalled memory at address %08x", mem);
	return 0;
}
static mem16_t nullRead16(u32 mem) {
	MEM_LOG("Read uninstalled memory at address %08x", mem);
	return 0;
}
static mem32_t nullRead32(u32 mem) {
	MEM_LOG("Read uninstalled memory at address %08x", mem);
	return 0;
}
static mem64_t nullRead64(u32 mem) {
	MEM_LOG("Read uninstalled memory at address %08x", mem);
	return 0;
}
static RETURNS_R128 nullRead128(u32 mem) {
	MEM_LOG("Read uninstalled memory at address %08x", mem);
	return r128_zero();
}
static void nullWrite8(u32 mem, mem8_t value)
{
	MEM_LOG("Write uninstalled memory at address %08x", mem);
}
static void nullWrite16(u32 mem, mem16_t value)
{
	MEM_LOG("Write uninstalled memory at address %08x", mem);
}
static void nullWrite32(u32 mem, mem32_t value)
{
	MEM_LOG("Write uninstalled memory at address %08x", mem);
}
static void nullWrite64(u32 mem, mem64_t value)
{
	MEM_LOG("Write uninstalled memory at address %08x", mem);
}
static void TAKES_R128 nullWrite128(u32 mem, r128 value)
{
	MEM_LOG("Write uninstalled memory at address %08x", mem);
}

template<int p>
static mem8_t _ext_memRead8 (u32 mem)
{
	switch (p)
	{
		case 3: // psh4
			return psxHw4Read8(mem);
		case 6: // gsm
			return gsRead8(mem);
		case 7: // dev9
		{
			mem8_t retval = DEV9read8(mem & ~0xa4000000);
			Console.WriteLn("DEV9 read8 %8.8lx: %2.2lx", mem & ~0xa4000000, retval);
			return retval;
		}
		default: break;
	}

	MEM_LOG("Unknown Memory Read8   from address %8.8x", mem);
	cpuTlbMissR(mem, cpuRegs.branch);
	return 0;
}

template<int p>
static mem16_t _ext_memRead16(u32 mem)
{
	switch (p)
	{
		case 4: // b80
			MEM_LOG("b800000 Memory read16 address %x", mem);
			return 0;
		case 5: // ba0
			return ba0R16(mem);
		case 6: // gsm
			return gsRead16(mem);

		case 7: // dev9
		{
			mem16_t retval = DEV9read16(mem & ~0xa4000000);
			Console.WriteLn("DEV9 read16 %8.8lx: %4.4lx", mem & ~0xa4000000, retval);
			return retval;
		}

		case 8: // spu2
			return SPU2read(mem);

		default: break;
	}
	MEM_LOG("Unknown Memory read16  from address %8.8x", mem);
	cpuTlbMissR(mem, cpuRegs.branch);
	return 0;
}

template<int p>
static mem32_t _ext_memRead32(u32 mem)
{
	switch (p)
	{
		case 6: // gsm
			return gsRead32(mem);
		case 7: // dev9
		{
			mem32_t retval = DEV9read32(mem & ~0xa4000000);
			Console.WriteLn("DEV9 read32 %8.8lx: %8.8lx", mem & ~0xa4000000, retval);
			return retval;
		}
		default: break;
	}

	MEM_LOG("Unknown Memory read32  from address %8.8x (Status=%8.8x)", mem, cpuRegs.CP0.n.Status.val);
	cpuTlbMissR(mem, cpuRegs.branch);
	return 0;
}

template<int p>
static u64 _ext_memRead64(u32 mem)
{
	switch (p)
	{
		case 6: // gsm
			return gsRead64(mem);
		default: break;
	}

	MEM_LOG("Unknown Memory read64  from address %8.8x", mem);
	cpuTlbMissR(mem, cpuRegs.branch);
	return 0;
}

template<int p>
static RETURNS_R128 _ext_memRead128(u32 mem)
{
	switch (p)
	{
		//case 1: // hwm
		//	return hwRead128(mem & ~0xa0000000);
		case 6: // gsm
			return r128_load(PS2GS_BASE(mem));
		default: break;
	}

	MEM_LOG("Unknown Memory read128 from address %8.8x", mem);
	cpuTlbMissR(mem, cpuRegs.branch);
	return r128_zero();
}

template<int p>
static void _ext_memWrite8 (u32 mem, mem8_t  value)
{
	switch (p) {
		case 3: // psh4
			psxHw4Write8(mem, value); return;
		case 6: // gsm
			gsWrite8(mem, value); return;
		case 7: // dev9
			DEV9write8(mem & ~0xa4000000, value);
			Console.WriteLn("DEV9 write8 %8.8lx: %2.2lx", mem & ~0xa4000000, value);
			return;
		default: break;
	}

	MEM_LOG("Unknown Memory write8   to  address %x with data %2.2x", mem, value);
	cpuTlbMissW(mem, cpuRegs.branch);
}

template<int p>
static void _ext_memWrite16(u32 mem, mem16_t value)
{
	switch (p) {
		case 5: // ba0
			MEM_LOG("ba00000 Memory write16 to  address %x with data %x", mem, value);
			return;
		case 6: // gsm
			gsWrite16(mem, value); return;
		case 7: // dev9
			DEV9write16(mem & ~0xa4000000, value);
			Console.WriteLn("DEV9 write16 %8.8lx: %4.4lx", mem & ~0xa4000000, value);
			return;
		case 8: // spu2
			SPU2write(mem, value); return;
		default: break;
	}
	MEM_LOG("Unknown Memory write16  to  address %x with data %4.4x", mem, value);
	cpuTlbMissW(mem, cpuRegs.branch);
}

template<int p>
static void _ext_memWrite32(u32 mem, mem32_t value)
{
	switch (p) {
		case 6: // gsm
			gsWrite32(mem, value); return;
		case 7: // dev9
			DEV9write32(mem & ~0xa4000000, value);
			Console.WriteLn("DEV9 write32 %8.8lx: %8.8lx", mem & ~0xa4000000, value);
			return;
		default: break;
	}
	MEM_LOG("Unknown Memory write32  to  address %x with data %8.8x", mem, value);
	cpuTlbMissW(mem, cpuRegs.branch);
}

template<int p>
static void _ext_memWrite64(u32 mem, mem64_t value)
{

	/*switch (p) {
		//case 1: // hwm
		//	hwWrite64(mem & ~0xa0000000, *value);
		//	return;
		//case 6: // gsm
		//	gsWrite64(mem & ~0xa0000000, *value); return;
	}*/

	MEM_LOG("Unknown Memory write64  to  address %x with data %8.8x_%8.8x", mem, (u32)(value>>32), (u32)value);
	cpuTlbMissW(mem, cpuRegs.branch);
}

template<int p>
static void TAKES_R128 _ext_memWrite128(u32 mem, r128 value)
{
	/*switch (p) {
		//case 1: // hwm
		//	hwWrite128(mem & ~0xa0000000, value);
		//	return;
		//case 6: // gsm
		//	mem &= ~0xa0000000;
		//	gsWrite64(mem,   value[0]);
		//	gsWrite64(mem+8, value[1]); return;
	}*/

	alignas(16) const u128 uvalue = r128_to_u128(value);
	MEM_LOG("Unknown Memory write128 to  address %x with data %8.8x_%8.8x_%8.8x_%8.8x", mem, uvalue._u32[3], uvalue._u32[2], uvalue._u32[1], uvalue._u32[0]);
	cpuTlbMissW(mem, cpuRegs.branch);
}

#define vtlb_RegisterHandlerTempl1(nam,t) vtlb_RegisterHandler(nam##Read8<t>,nam##Read16<t>,nam##Read32<t>,nam##Read64<t>,nam##Read128<t>, \
															   nam##Write8<t>,nam##Write16<t>,nam##Write32<t>,nam##Write64<t>,nam##Write128<t>)

typedef void ClearFunc_t( u32 addr, u32 qwc );

template<int vunum> static __fi void ClearVuFunc(u32 addr, u32 size) {
	if (vunum) CpuVU1->Clear(addr, size);
	else       CpuVU0->Clear(addr, size);
}

// VU Micro Memory Reads...
template<int vunum> static mem8_t vuMicroRead8(u32 addr) {
	VURegs* vu = vunum ?  &VU1 :  &VU0;
	addr      &= vunum ? 0x3fff: 0xfff;

	if (vunum && THREAD_VU1) vu1Thread.WaitVU();
	return vu->Micro[addr];
}
template<int vunum> static mem16_t vuMicroRead16(u32 addr) {
	VURegs* vu = vunum ?  &VU1 :  &VU0;
	addr      &= vunum ? 0x3fff: 0xfff;

	if (vunum && THREAD_VU1) vu1Thread.WaitVU();
	return *(u16*)&vu->Micro[addr];
}
template<int vunum> static mem32_t vuMicroRead32(u32 addr) {
	VURegs* vu = vunum ?  &VU1 :  &VU0;
	addr      &= vunum ? 0x3fff: 0xfff;

	if (vunum && THREAD_VU1) vu1Thread.WaitVU();
	return *(u32*)&vu->Micro[addr];
}
template<int vunum> static mem64_t vuMicroRead64(u32 addr) {
	VURegs* vu = vunum ?  &VU1 :  &VU0;
	addr      &= vunum ? 0x3fff: 0xfff;

	if (vunum && THREAD_VU1) vu1Thread.WaitVU();
	return *(u64*)&vu->Micro[addr];
}
template<int vunum> static RETURNS_R128 vuMicroRead128(u32 addr) {
	VURegs* vu = vunum ?  &VU1 :  &VU0;
	addr      &= vunum ? 0x3fff: 0xfff;
	if (vunum && THREAD_VU1) vu1Thread.WaitVU();

	return r128_load(&vu->Micro[addr]);
}

// Profiled VU writes: Happen very infrequently, with exception of BIOS initialization (at most twice per
//   frame in-game, and usually none at all after BIOS), so cpu clears aren't much of a big deal.
template<int vunum> static void vuMicroWrite8(u32 addr,mem8_t data) {
	VURegs* vu = vunum ?  &VU1 :  &VU0;
	addr      &= vunum ? 0x3fff: 0xfff;

	if (vunum && THREAD_VU1) {
		vu1Thread.WriteMicroMem(addr, &data, sizeof(u8));
		return;
	}
	if (vu->Micro[addr]!=data) {     // Clear before writing new data
		ClearVuFunc<vunum>(addr, 8); //(clearing 8 bytes because an instruction is 8 bytes) (cottonvibes)
		vu->Micro[addr] =data;
	}
}
template<int vunum> static void vuMicroWrite16(u32 addr, mem16_t data) {
	VURegs* vu = vunum ?  &VU1 :  &VU0;
	addr      &= vunum ? 0x3fff: 0xfff;

	if (vunum && THREAD_VU1) {
		vu1Thread.WriteMicroMem(addr, &data, sizeof(u16));
		return;
	}
	if (*(u16*)&vu->Micro[addr]!=data) {
		ClearVuFunc<vunum>(addr, 8);
		*(u16*)&vu->Micro[addr] =data;
	}
}
template<int vunum> static void vuMicroWrite32(u32 addr, mem32_t data) {
	VURegs* vu = vunum ?  &VU1 :  &VU0;
	addr      &= vunum ? 0x3fff: 0xfff;

	if (vunum && THREAD_VU1) {
		vu1Thread.WriteMicroMem(addr, &data, sizeof(u32));
		return;
	}
	if (*(u32*)&vu->Micro[addr]!=data) {
		ClearVuFunc<vunum>(addr, 8);
		*(u32*)&vu->Micro[addr] =data;
	}
}
template<int vunum> static void vuMicroWrite64(u32 addr, mem64_t data) {
	VURegs* vu = vunum ?  &VU1 :  &VU0;
	addr      &= vunum ? 0x3fff: 0xfff;

	if (vunum && THREAD_VU1) {
		vu1Thread.WriteMicroMem(addr, &data, sizeof(u64));
		return;
	}

	if (*(u64*)&vu->Micro[addr]!=data) {
		ClearVuFunc<vunum>(addr, 8);
		*(u64*)&vu->Micro[addr] =data;
	}
}
template<int vunum> static void TAKES_R128 vuMicroWrite128(u32 addr, r128 data) {
	VURegs* vu = vunum ?  &VU1 :  &VU0;
	addr      &= vunum ? 0x3fff: 0xfff;

	const u128 udata = r128_to_u128(data);

	if (vunum && THREAD_VU1) {
		vu1Thread.WriteMicroMem(addr, &udata, sizeof(u128));
		return;
	}
	if ((u128&)vu->Micro[addr]!=udata) {
		ClearVuFunc<vunum>(addr, 16);
		r128_store_unaligned(&vu->Micro[addr],data);
	}
}

// VU Data Memory Reads...
template<int vunum> static mem8_t vuDataRead8(u32 addr) {
	VURegs* vu = vunum ?  &VU1 :  &VU0;
	addr      &= vunum ? 0x3fff: 0xfff;
	if (vunum && THREAD_VU1) vu1Thread.WaitVU();
	return vu->Mem[addr];
}
template<int vunum> static mem16_t vuDataRead16(u32 addr) {
	VURegs* vu = vunum ?  &VU1 :  &VU0;
	addr      &= vunum ? 0x3fff: 0xfff;
	if (vunum && THREAD_VU1) vu1Thread.WaitVU();
	return *(u16*)&vu->Mem[addr];
}
template<int vunum> static mem32_t vuDataRead32(u32 addr) {
	VURegs* vu = vunum ?  &VU1 :  &VU0;
	addr      &= vunum ? 0x3fff: 0xfff;
	if (vunum && THREAD_VU1) vu1Thread.WaitVU();
	return *(u32*)&vu->Mem[addr];
}
template<int vunum> static mem64_t vuDataRead64(u32 addr) {
	VURegs* vu = vunum ?  &VU1 :  &VU0;
	addr      &= vunum ? 0x3fff: 0xfff;
	if (vunum && THREAD_VU1) vu1Thread.WaitVU();
	return *(u64*)&vu->Mem[addr];
}
template<int vunum> static RETURNS_R128 vuDataRead128(u32 addr) {
	VURegs* vu = vunum ?  &VU1 :  &VU0;
	addr      &= vunum ? 0x3fff: 0xfff;
	if (vunum && THREAD_VU1) vu1Thread.WaitVU();
	return r128_load(&vu->Mem[addr]);
}

// VU Data Memory Writes...
template<int vunum> static void vuDataWrite8(u32 addr, mem8_t data) {
	VURegs* vu = vunum ?  &VU1 :  &VU0;
	addr      &= vunum ? 0x3fff: 0xfff;
	if (vunum && THREAD_VU1) {
		vu1Thread.WriteDataMem(addr, &data, sizeof(u8));
		return;
	}
	vu->Mem[addr] = data;
}
template<int vunum> static void vuDataWrite16(u32 addr, mem16_t data) {
	VURegs* vu = vunum ?  &VU1 :  &VU0;
	addr      &= vunum ? 0x3fff: 0xfff;
	if (vunum && THREAD_VU1) {
		vu1Thread.WriteDataMem(addr, &data, sizeof(u16));
		return;
	}
	*(u16*)&vu->Mem[addr] = data;
}
template<int vunum> static void vuDataWrite32(u32 addr, mem32_t data) {
	VURegs* vu = vunum ?  &VU1 :  &VU0;
	addr      &= vunum ? 0x3fff: 0xfff;
	if (vunum && THREAD_VU1) {
		vu1Thread.WriteDataMem(addr, &data, sizeof(u32));
		return;
	}
	*(u32*)&vu->Mem[addr] = data;
}
template<int vunum> static void vuDataWrite64(u32 addr, mem64_t data) {
	VURegs* vu = vunum ?  &VU1 :  &VU0;
	addr      &= vunum ? 0x3fff: 0xfff;
	if (vunum && THREAD_VU1) {
		vu1Thread.WriteDataMem(addr, &data, sizeof(u64));
		return;
	}
	*(u64*)&vu->Mem[addr] = data;
}
template<int vunum> static void TAKES_R128 vuDataWrite128(u32 addr, r128 data) {
	VURegs* vu = vunum ?  &VU1 :  &VU0;
	addr      &= vunum ? 0x3fff: 0xfff;
	if (vunum && THREAD_VU1) {
		alignas(16) const u128 udata = r128_to_u128(data);
		vu1Thread.WriteDataMem(addr, &udata, sizeof(u128));
		return;
	}
	r128_store_unaligned(&vu->Mem[addr], data);
}


void memSetPageAddr(u32 vaddr, u32 paddr)
{
	//Console.WriteLn("memSetPageAddr: %8.8x -> %8.8x", vaddr, paddr);

	vtlb_VMap(vaddr,paddr,0x1000);

}

void memClearPageAddr(u32 vaddr)
{
	//Console.WriteLn("memClearPageAddr: %8.8x", vaddr);

	vtlb_VMapUnmap(vaddr,0x1000); // -> whut ?

#ifdef FULLTLB
//	memLUTRK[vaddr >> 12] = 0;
//	memLUTWK[vaddr >> 12] = 0;
#endif
}

///////////////////////////////////////////////////////////////////////////
// PS2 Memory Init / Reset / Shutdown

EEVM_MemoryAllocMess* eeMem = NULL;
alignas(__pagesize) u8 eeHw[Ps2MemSize::Hardware];


void memBindConditionalHandlers()
{
	if( hw_by_page[0xf] == 0xFFFFFFFF ) return;

	if (EmuConfig.Speedhacks.IntcStat)
	{
		vtlbMemR16FP* page0F16(hwRead16_page_0F_INTC_HACK);
		vtlbMemR32FP* page0F32(hwRead32_page_0F_INTC_HACK);
		//vtlbMemR64FP* page0F64(hwRead64_generic_INTC_HACK);

		vtlb_ReassignHandler( hw_by_page[0xf],
			hwRead8<0x0f>,	page0F16,			page0F32,			hwRead64<0x0f>,		hwRead128<0x0f>,
			hwWrite8<0x0f>,	hwWrite16<0x0f>,	hwWrite32<0x0f>,	hwWrite64<0x0f>,	hwWrite128<0x0f>
		);
	}
	else
	{
		vtlbMemR16FP* page0F16(hwRead16<0x0f>);
		vtlbMemR32FP* page0F32(hwRead32<0x0f>);
		//vtlbMemR64FP* page0F64(hwRead64<0x0f>);

		vtlb_ReassignHandler( hw_by_page[0xf],
			hwRead8<0x0f>,	page0F16,			page0F32,			hwRead64<0x0f>,		hwRead128<0x0f>,
			hwWrite8<0x0f>,	hwWrite16<0x0f>,	hwWrite32<0x0f>,	hwWrite64<0x0f>,	hwWrite128<0x0f>
		);
	}
}


// --------------------------------------------------------------------------------------
//  eeMemoryReserve  (implementations)
// --------------------------------------------------------------------------------------
eeMemoryReserve::eeMemoryReserve()
	: _parent("EE Main Memory")
{
}

eeMemoryReserve::~eeMemoryReserve()
{
	Release();
}

void eeMemoryReserve::Assign(VirtualMemoryManagerPtr allocator)
{
	_parent::Assign(std::move(allocator), HostMemoryMap::EEmemOffset, sizeof(*eeMem));
	eeMem = reinterpret_cast<EEVM_MemoryAllocMess*>(GetPtr());
}


// Resets memory mappings, unmaps TLBs, reloads bios roms, etc.
void eeMemoryReserve::Reset()
{
	_parent::Reset();

	// Note!!  Ideally the vtlb should only be initialized once, and then subsequent
	// resets of the system hardware would only clear vtlb mappings, but since the
	// rest of the emu is not really set up to support a "soft" reset of that sort
	// we opt for the hard/safe version.

	pxAssume( eeMem );

#ifdef ENABLECACHE
	memset(pCache,0,sizeof(_cacheS)*64);
#endif

	vtlb_Init();

	null_handler = vtlb_RegisterHandler(nullRead8, nullRead16, nullRead32, nullRead64, nullRead128,
		nullWrite8, nullWrite16, nullWrite32, nullWrite64, nullWrite128);

	tlb_fallback_0 = vtlb_RegisterHandlerTempl1(_ext_mem,0);
	tlb_fallback_3 = vtlb_RegisterHandlerTempl1(_ext_mem,3);
	tlb_fallback_4 = vtlb_RegisterHandlerTempl1(_ext_mem,4);
	tlb_fallback_5 = vtlb_RegisterHandlerTempl1(_ext_mem,5);
	tlb_fallback_7 = vtlb_RegisterHandlerTempl1(_ext_mem,7);
	tlb_fallback_8 = vtlb_RegisterHandlerTempl1(_ext_mem,8);

	// Dynarec versions of VUs
	vu0_micro_mem = vtlb_RegisterHandlerTempl1(vuMicro,0);
	vu1_micro_mem = vtlb_RegisterHandlerTempl1(vuMicro,1);
	vu1_data_mem  = (1||THREAD_VU1) ? vtlb_RegisterHandlerTempl1(vuData,1) : 0;

	//////////////////////////////////////////////////////////////////////////////////////////
	// IOP's "secret" Hardware Register mapping, accessible from the EE (and meant for use
	// by debugging or BIOS only).  The IOP's hw regs are divided into three main pages in
	// the 0x1f80 segment, and then another oddball page for CDVD in the 0x1f40 segment.
	//

	using namespace IopMemory;

	tlb_fallback_2 = vtlb_RegisterHandler(
		iopHwRead8_generic, iopHwRead16_generic, iopHwRead32_generic, _ext_memRead64<2>, _ext_memRead128<2>,
		iopHwWrite8_generic, iopHwWrite16_generic, iopHwWrite32_generic, _ext_memWrite64<2>, _ext_memWrite128<2>
	);

	iopHw_by_page_01 = vtlb_RegisterHandler(
		iopHwRead8_Page1, iopHwRead16_Page1, iopHwRead32_Page1, _ext_memRead64<2>, _ext_memRead128<2>,
		iopHwWrite8_Page1, iopHwWrite16_Page1, iopHwWrite32_Page1, _ext_memWrite64<2>, _ext_memWrite128<2>
	);

	iopHw_by_page_03 = vtlb_RegisterHandler(
		iopHwRead8_Page3, iopHwRead16_Page3, iopHwRead32_Page3, _ext_memRead64<2>, _ext_memRead128<2>,
		iopHwWrite8_Page3, iopHwWrite16_Page3, iopHwWrite32_Page3, _ext_memWrite64<2>, _ext_memWrite128<2>
	);

	iopHw_by_page_08 = vtlb_RegisterHandler(
		iopHwRead8_Page8, iopHwRead16_Page8, iopHwRead32_Page8, _ext_memRead64<2>, _ext_memRead128<2>,
		iopHwWrite8_Page8, iopHwWrite16_Page8, iopHwWrite32_Page8, _ext_memWrite64<2>, _ext_memWrite128<2>
	);


	// psHw Optimized Mappings
	// The HW Registers have been split into pages to improve optimization.

#define hwHandlerTmpl(page) \
	hwRead8<page>,	hwRead16<page>,	hwRead32<page>,	hwRead64<page>,	hwRead128<page>, \
	hwWrite8<page>,	hwWrite16<page>,hwWrite32<page>,hwWrite64<page>,hwWrite128<page>

	hw_by_page[0x0] = vtlb_RegisterHandler( hwHandlerTmpl(0x00) );
	hw_by_page[0x1] = vtlb_RegisterHandler( hwHandlerTmpl(0x01) );
	hw_by_page[0x2] = vtlb_RegisterHandler( hwHandlerTmpl(0x02) );
	hw_by_page[0x3] = vtlb_RegisterHandler( hwHandlerTmpl(0x03) );
	hw_by_page[0x4] = vtlb_RegisterHandler( hwHandlerTmpl(0x04) );
	hw_by_page[0x5] = vtlb_RegisterHandler( hwHandlerTmpl(0x05) );
	hw_by_page[0x6] = vtlb_RegisterHandler( hwHandlerTmpl(0x06) );
	hw_by_page[0x7] = vtlb_RegisterHandler( hwHandlerTmpl(0x07) );
	hw_by_page[0x8] = vtlb_RegisterHandler( hwHandlerTmpl(0x08) );
	hw_by_page[0x9] = vtlb_RegisterHandler( hwHandlerTmpl(0x09) );
	hw_by_page[0xa] = vtlb_RegisterHandler( hwHandlerTmpl(0x0a) );
	hw_by_page[0xb] = vtlb_RegisterHandler( hwHandlerTmpl(0x0b) );
	hw_by_page[0xc] = vtlb_RegisterHandler( hwHandlerTmpl(0x0c) );
	hw_by_page[0xd] = vtlb_RegisterHandler( hwHandlerTmpl(0x0d) );
	hw_by_page[0xe] = vtlb_RegisterHandler( hwHandlerTmpl(0x0e) );
	hw_by_page[0xf] = vtlb_NewHandler();		// redefined later based on speedhacking prefs
	memBindConditionalHandlers();

	//////////////////////////////////////////////////////////////////////
	// GS Optimized Mappings

	tlb_fallback_6 = vtlb_RegisterHandler(
		_ext_memRead8<6>, _ext_memRead16<6>, _ext_memRead32<6>, _ext_memRead64<6>, _ext_memRead128<6>,
		_ext_memWrite8<6>, _ext_memWrite16<6>, _ext_memWrite32<6>, gsWrite64_generic, gsWrite128_generic
	);

	gs_page_0 = vtlb_RegisterHandler(
		_ext_memRead8<6>, _ext_memRead16<6>, _ext_memRead32<6>, _ext_memRead64<6>, _ext_memRead128<6>,
		_ext_memWrite8<6>, _ext_memWrite16<6>, _ext_memWrite32<6>, gsWrite64_page_00, gsWrite128_page_00
	);

	gs_page_1 = vtlb_RegisterHandler(
		_ext_memRead8<6>, _ext_memRead16<6>, _ext_memRead32<6>, _ext_memRead64<6>, _ext_memRead128<6>,
		_ext_memWrite8<6>, _ext_memWrite16<6>, _ext_memWrite32<6>, gsWrite64_page_01, gsWrite128_page_01
	);

	//vtlb_Reset();

	// reset memLUT (?)
	//vtlb_VMap(0x00000000,0x00000000,0x20000000);
	//vtlb_VMapUnmap(0x20000000,0x60000000);

	memMapPhy();
	memMapVUmicro();
	memMapKernelMem();
	memMapSupervisorMem();
	memMapUserMem();
	memSetKernelMode();

	vtlb_VMap(0x00000000,0x00000000,0x20000000);
	vtlb_VMapUnmap(0x20000000,0x60000000);

	CopyBIOSToMemory();
}

void eeMemoryReserve::Release()
{
	eeMem = nullptr;
	_parent::Release();
}
