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
	EE physical map :
	[0000 0000,1000 0000) -> Ram (mirrored ?)
	[1000 0000,1400 0000) -> Registers
	[1400 0000,1fc0 0000) -> Reserved (ingored writes, 'random' reads)
	[1fc0 0000,2000 0000) -> Boot ROM

	[2000 0000,4000 0000) -> Unmapped (BUS ERROR)
	[4000 0000,8000 0000) -> "Extended memory", probably unmapped (BUS ERROR) on retail ps2's :)
	[8000 0000,FFFF FFFF] -> Unmapped (BUS ERROR)

	vtlb/phy only supports the [0000 0000,2000 0000) region, with 4k pages.
	vtlb/vmap supports mapping to either of these locations, or some other (externaly) specified address.
*/

#include "PrecompiledHeader.h"

#include "Common.h"
#include "vtlb.h"
#include "COP0.h"
#include "Cache.h"
#include "R5900Exceptions.h"

#include "common/MemsetFast.inl"

#include "fmt/core.h"

using namespace R5900;
using namespace vtlb_private;

#define verify pxAssert

namespace vtlb_private
{
	alignas(64) MapData vtlbdata;
}

static vtlbHandler vtlbHandlerCount = 0;

static vtlbHandler DefaultPhyHandler;
static vtlbHandler UnmappedVirtHandler0;
static vtlbHandler UnmappedVirtHandler1;
static vtlbHandler UnmappedPhyHandler0;
static vtlbHandler UnmappedPhyHandler1;

vtlb_private::VTLBPhysical vtlb_private::VTLBPhysical::fromPointer(sptr ptr) {
	pxAssertMsg(ptr >= 0, "Address too high");
	return VTLBPhysical(ptr);
}

vtlb_private::VTLBPhysical vtlb_private::VTLBPhysical::fromHandler(vtlbHandler handler) {
	return VTLBPhysical(handler | POINTER_SIGN_BIT);
}

vtlb_private::VTLBVirtual::VTLBVirtual(VTLBPhysical phys, u32 paddr, u32 vaddr) {
	pxAssertMsg(0 == (paddr & VTLB_PAGE_MASK), "Should be page aligned");
	pxAssertMsg(0 == (vaddr & VTLB_PAGE_MASK), "Should be page aligned");
	pxAssertMsg((uptr)paddr < POINTER_SIGN_BIT, "Address too high");
	if (phys.isHandler()) {
		value = phys.raw() + paddr - vaddr;
	} else {
		value = phys.raw() - vaddr;
	}
}

__inline int CheckCache(u32 addr)
{
	u32 mask;

	if(((cpuRegs.CP0.n.Config >> 16) & 0x1) == 0) 
	{
		//DevCon.Warning("Data Cache Disabled! %x", cpuRegs.CP0.n.Config);
		return false;//
	}

	for(int i = 1; i < 48; i++)
	{
		if (((tlb[i].EntryLo1 & 0x38) >> 3) == 0x3) {
			mask  = tlb[i].PageMask;
			
			if ((addr >= tlb[i].PFN1) && (addr <= tlb[i].PFN1 + mask)) {
				//DevCon.Warning("Yay! Cache check cache addr=%x, mask=%x, addr+mask=%x, VPN2=%x PFN0=%x", addr, mask, (addr & mask), tlb[i].VPN2, tlb[i].PFN0); 
				return true;
			}
		}
		if (((tlb[i].EntryLo0 & 0x38) >> 3) == 0x3) {
			mask  = tlb[i].PageMask;
			
			if ((addr >= tlb[i].PFN0) && (addr <= tlb[i].PFN0 + mask)) {
				//DevCon.Warning("Yay! Cache check cache addr=%x, mask=%x, addr+mask=%x, VPN2=%x PFN0=%x", addr, mask, (addr & mask), tlb[i].VPN2, tlb[i].PFN0); 
				return true;
			}
		}
	}
	return false;
}
// --------------------------------------------------------------------------------------
// Interpreter Implementations of VTLB Memory Operations.
// --------------------------------------------------------------------------------------
// See recVTLB.cpp for the dynarec versions.

template< typename DataType >
DataType vtlb_memRead(u32 addr)
{
	static const uint DataSize = sizeof(DataType) * 8;
	auto vmv = vtlbdata.vmap[addr>>VTLB_PAGE_BITS];

	if (!vmv.isHandler(addr))
	{
		if (!CHECK_EEREC) 
		{
			if(CHECK_CACHE && CheckCache(addr)) 
			{
				switch( DataSize )
				{
					case 8: 
						return readCache8(addr);
						break;
					case 16: 
						return readCache16(addr);
						break;
					case 32: 
						return readCache32(addr);
						break;

					jNO_DEFAULT;
				}
			}
		}

		return *reinterpret_cast<DataType*>(vmv.assumePtr(addr));
	}

	//has to: translate, find function, call function
	u32 paddr=vmv.assumeHandlerGetPAddr(addr);
	//Console.WriteLn("Translated 0x%08X to 0x%08X", addr,paddr);
	//return reinterpret_cast<TemplateHelper<DataSize,false>::HandlerType*>(vtlbdata.RWFT[TemplateHelper<DataSize,false>::sidx][0][hand])(paddr,data);

	switch( DataSize )
	{
		case 8:
			return vmv.assumeHandler< 8, false>()(paddr);
		case 16:
			return vmv.assumeHandler<16, false>()(paddr);
		case 32:
			return vmv.assumeHandler<32, false>()(paddr);

		jNO_DEFAULT;
	}

	return 0;		// technically unreachable, but suppresses warnings.
}

RETURNS_R64 vtlb_memRead64(u32 mem)
{
	auto vmv = vtlbdata.vmap[mem>>VTLB_PAGE_BITS];

	if (!vmv.isHandler(mem))
	{
		if (!CHECK_EEREC) {
			if(CHECK_CACHE && CheckCache(mem)) 
			{
				return readCache64(mem);
			}
		}

		return r64_load(reinterpret_cast<const void*>(vmv.assumePtr(mem)));
	}
	else
	{
		//has to: translate, find function, call function
		u32 paddr = vmv.assumeHandlerGetPAddr(mem);
		//Console.WriteLn("Translated 0x%08X to 0x%08X", addr,paddr);
		return vmv.assumeHandler<64, false>()(paddr);
	}
}

RETURNS_R128 vtlb_memRead128(u32 mem)
{
	auto vmv = vtlbdata.vmap[mem>>VTLB_PAGE_BITS];

	if (!vmv.isHandler(mem))
	{
		if (!CHECK_EEREC) 
		{
			if(CHECK_CACHE && CheckCache(mem)) 
			{
				return readCache128(mem);
			}
		}

		return r128_load(reinterpret_cast<const void*>(vmv.assumePtr(mem)));
	}
	else
	{
		//has to: translate, find function, call function
		u32 paddr = vmv.assumeHandlerGetPAddr(mem);
		//Console.WriteLn("Translated 0x%08X to 0x%08X", addr,paddr);
		return vmv.assumeHandler<128, false>()(paddr);
	}
}

template< typename DataType >
void vtlb_memWrite(u32 addr, DataType data)
{
	static const uint DataSize = sizeof(DataType) * 8;

	auto vmv = vtlbdata.vmap[addr>>VTLB_PAGE_BITS];

	if (!vmv.isHandler(addr))
	{		
		if (!CHECK_EEREC) 
		{
			if(CHECK_CACHE && CheckCache(addr)) 
			{
				switch( DataSize )
				{
				case 8: 
					writeCache8(addr, data);
					return;
				case 16:
					writeCache16(addr, data);
					return;
				case 32:
					writeCache32(addr, data);
					return;
				}
			}
		}

		*reinterpret_cast<DataType*>(vmv.assumePtr(addr))=data;
	}
	else
	{
		//has to: translate, find function, call function
		u32 paddr = vmv.assumeHandlerGetPAddr(addr);
		//Console.WriteLn("Translated 0x%08X to 0x%08X", addr,paddr);
		return vmv.assumeHandler<sizeof(DataType)*8, true>()(paddr, data);
	}
}

void vtlb_memWrite64(u32 mem, const mem64_t* value)
{
	auto vmv = vtlbdata.vmap[mem>>VTLB_PAGE_BITS];

	if (!vmv.isHandler(mem))
	{		
		if (!CHECK_EEREC) 
		{
			if(CHECK_CACHE && CheckCache(mem)) 
			{
				writeCache64(mem, *value);
				return;
			}
		}

		*(mem64_t*)vmv.assumePtr(mem) = *value;
	}
	else
	{
		//has to: translate, find function, call function
		u32 paddr = vmv.assumeHandlerGetPAddr(mem);
		//Console.WriteLn("Translated 0x%08X to 0x%08X", addr,paddr);

		vmv.assumeHandler<64, true>()(paddr, value);
	}
}

void vtlb_memWrite128(u32 mem, const mem128_t *value)
{
	auto vmv = vtlbdata.vmap[mem>>VTLB_PAGE_BITS];

	if (!vmv.isHandler(mem))
	{
		if (!CHECK_EEREC) 
		{
			if(CHECK_CACHE && CheckCache(mem)) 
			{
				writeCache128(mem, value);
				return;
			}
		}

		CopyQWC((void*)vmv.assumePtr(mem), value);
	}
	else
	{
		//has to: translate, find function, call function
		u32 paddr = vmv.assumeHandlerGetPAddr(mem);
		//Console.WriteLn("Translated 0x%08X to 0x%08X", addr,paddr);

		vmv.assumeHandler<128, true>()(paddr, value);
	}
}

template mem8_t vtlb_memRead<mem8_t>(u32 mem);
template mem16_t vtlb_memRead<mem16_t>(u32 mem);
template mem32_t vtlb_memRead<mem32_t>(u32 mem);
template void vtlb_memWrite<mem8_t>(u32 mem, mem8_t data);
template void vtlb_memWrite<mem16_t>(u32 mem, mem16_t data);
template void vtlb_memWrite<mem32_t>(u32 mem, mem32_t data);

// --------------------------------------------------------------------------------------
//  TLB Miss / BusError Handlers
// --------------------------------------------------------------------------------------
// These are valid VM memory errors that should typically be handled by the VM itself via
// its own cpu exception system.
//
// [TODO]  Add first-chance debugging hooks to these exceptions!
//
// Important recompiler note: Mid-block Exception handling isn't reliable *yet* because
// memory ops don't flush the PC prior to invoking the indirect handlers.


static void GoemonTlbMissDebug()
{
	// 0x3d5580 is the address of the TLB cache
	GoemonTlb* tlb = (GoemonTlb*)&eeMem->Main[0x3d5580];

	for (u32 i = 0; i < 150; i++) {
		if (tlb[i].valid == 0x1 && tlb[i].low_add != tlb[i].high_add)
			DevCon.WriteLn("GoemonTlbMissDebug: Entry %d is valid. Key %x. From V:0x%8.8x to V:0x%8.8x (P:0x%8.8x)", i, tlb[i].key, tlb[i].low_add, tlb[i].high_add, tlb[i].physical_add);
		else if (tlb[i].low_add != tlb[i].high_add)
			DevCon.WriteLn("GoemonTlbMissDebug: Entry %d is invalid. Key %x. From V:0x%8.8x to V:0x%8.8x (P:0x%8.8x)", i, tlb[i].key, tlb[i].low_add, tlb[i].high_add, tlb[i].physical_add);
	}
}

void GoemonPreloadTlb()
{
	// 0x3d5580 is the address of the TLB cache table
	GoemonTlb* tlb = (GoemonTlb*)&eeMem->Main[0x3d5580];

	for (u32 i = 0; i < 150; i++) {
		if (tlb[i].valid == 0x1 && tlb[i].low_add != tlb[i].high_add) {

			u32 size  = tlb[i].high_add - tlb[i].low_add;
			u32 vaddr = tlb[i].low_add;
			u32 paddr = tlb[i].physical_add;

			// TODO: The old code (commented below) seems to check specifically for handler 0.  Is this really correct?
			//if ((uptr)vtlbdata.vmap[vaddr>>VTLB_PAGE_BITS] == POINTER_SIGN_BIT) {
			auto vmv = vtlbdata.vmap[vaddr>>VTLB_PAGE_BITS];
			if (vmv.isHandler(vaddr) && vmv.assumeHandlerGetID() == 0) {
				DevCon.WriteLn("GoemonPreloadTlb: Entry %d. Key %x. From V:0x%8.8x to P:0x%8.8x (%d pages)", i, tlb[i].key, vaddr, paddr, size >> VTLB_PAGE_BITS);
				vtlb_VMap(           vaddr , paddr, size);
				vtlb_VMap(0x20000000|vaddr , paddr, size);
			}
		}
	}
}

void GoemonUnloadTlb(u32 key)
{
	// 0x3d5580 is the address of the TLB cache table
	GoemonTlb* tlb = (GoemonTlb*)&eeMem->Main[0x3d5580];
	for (u32 i = 0; i < 150; i++) {
		if (tlb[i].key == key) {
			if (tlb[i].valid == 0x1) {
				u32 size  = tlb[i].high_add - tlb[i].low_add;
				u32 vaddr = tlb[i].low_add;
				DevCon.WriteLn("GoemonUnloadTlb: Entry %d. Key %x. From V:0x%8.8x to V:0x%8.8x (%d pages)", i, tlb[i].key, vaddr, vaddr+size, size >> VTLB_PAGE_BITS);

				vtlb_VMapUnmap(           vaddr , size);
				vtlb_VMapUnmap(0x20000000|vaddr , size);

				// Unmap the tlb in game cache table
				// Note: Game copy FEFEFEFE for others data
				tlb[i].valid    = 0;
				tlb[i].key      = 0xFEFEFEFE;
				tlb[i].low_add  = 0xFEFEFEFE;
				tlb[i].high_add = 0xFEFEFEFE;
			} else {
				DevCon.Error("GoemonUnloadTlb: Entry %d is not valid. Key %x", i, tlb[i].key);
			}
		}
	}
}

// Generates a tlbMiss Exception
static __ri void vtlb_Miss(u32 addr,u32 mode)
{
	if (EmuConfig.Gamefixes.GoemonTlbHack)
		GoemonTlbMissDebug();

	// Hack to handle expected tlb miss by some games.
	if (Cpu == &intCpu) {
		if (mode)
			cpuTlbMissW(addr, cpuRegs.branch);
		else
			cpuTlbMissR(addr, cpuRegs.branch);

		// Exception handled. Current instruction need to be stopped
		throw Exception::CancelInstruction();
	}

	if( IsDevBuild )
		Cpu->ThrowCpuException( R5900Exception::TLBMiss( addr, !!mode ) );
	else
	{
		static int spamStop = 0;
		if ( spamStop++ < 50 )
			Console.Error( R5900Exception::TLBMiss( addr, !!mode ).FormatMessage() );
	}
}

// BusError exception: more serious than a TLB miss.  If properly emulated the PS2 kernel
// itself would invoke a diagnostic/assertion screen that displays the cpu state at the
// time of the exception.
static __ri void vtlb_BusError(u32 addr,u32 mode)
{
	// The exception terminate the program on linux which is very annoying
	// Just disable it for the moment
#ifdef __linux__
	if (0)
#else
	if( IsDevBuild )
#endif
		Cpu->ThrowCpuException( R5900Exception::BusError( addr, !!mode ) );
	else
		Console.Error( R5900Exception::TLBMiss( addr, !!mode ).FormatMessage() );
}

template<typename OperandType, u32 saddr>
OperandType vtlbUnmappedVReadSm(u32 addr)                   { vtlb_Miss(addr|saddr,0); return 0; }

template<typename OperandType, u32 saddr>
u_to_r<OperandType> __vectorcall vtlbUnmappedVReadLg(u32 addr)         { vtlb_Miss(addr|saddr,0); return rhelper<OperandType>::zero(); }

template<typename OperandType, u32 saddr>
void vtlbUnmappedVWriteSm(u32 addr,OperandType data)        { vtlb_Miss(addr|saddr,1); }

template<typename OperandType, u32 saddr>
void vtlbUnmappedVWriteLg(u32 addr,const OperandType* data) { vtlb_Miss(addr|saddr,1); }

template<typename OperandType, u32 saddr>
OperandType vtlbUnmappedPReadSm(u32 addr)                   { vtlb_BusError(addr|saddr,0); return 0; }

template<typename OperandType, u32 saddr>
u_to_r<OperandType> __vectorcall vtlbUnmappedPReadLg(u32 addr)         { vtlb_BusError(addr|saddr,0); return rhelper<OperandType>::zero(); }

template<typename OperandType, u32 saddr>
void vtlbUnmappedPWriteSm(u32 addr,OperandType data)        { vtlb_BusError(addr|saddr,1); }

template<typename OperandType, u32 saddr>
void vtlbUnmappedPWriteLg(u32 addr,const OperandType* data) { vtlb_BusError(addr|saddr,1); }

// --------------------------------------------------------------------------------------
//  VTLB mapping errors
// --------------------------------------------------------------------------------------
// These errors are assertion/logic errors that should never occur if PCSX2 has been initialized
// properly.  All addressable physical memory should be configured as TLBMiss or Bus Error.
//

static mem8_t vtlbDefaultPhyRead8(u32 addr)
{
	pxFailDev(fmt::format("(VTLB) Attempted read8 from unmapped physical address @ 0x{:08X}.", addr).c_str());
	return 0;
}

static mem16_t vtlbDefaultPhyRead16(u32 addr)
{
	pxFailDev(fmt::format("(VTLB) Attempted read16 from unmapped physical address @ 0x{:08X}.", addr).c_str());
	return 0;
}

static mem32_t vtlbDefaultPhyRead32(u32 addr)
{
	pxFailDev(fmt::format("(VTLB) Attempted read32 from unmapped physical address @ 0x{:08X}.", addr).c_str());
	return 0;
}

static __m128i __vectorcall vtlbDefaultPhyRead64(u32 addr)
{
	pxFailDev(fmt::format("(VTLB) Attempted read64 from unmapped physical address @ 0x{:08X}.", addr).c_str());
	return r64_zero();
}

static __m128i __vectorcall vtlbDefaultPhyRead128(u32 addr)
{
	pxFailDev(fmt::format("(VTLB) Attempted read128 from unmapped physical address @ 0x{:08X}.", addr).c_str());
	return r128_zero();
}

static void vtlbDefaultPhyWrite8(u32 addr, mem8_t data)
{
	pxFailDev(fmt::format("(VTLB) Attempted write8 to unmapped physical address @ 0x{:08X}.", addr).c_str());
}

static void vtlbDefaultPhyWrite16(u32 addr, mem16_t data)
{
	pxFailDev(fmt::format("(VTLB) Attempted write16 to unmapped physical address @ 0x{:08X}.", addr).c_str());
}

static void vtlbDefaultPhyWrite32(u32 addr, mem32_t data)
{
	pxFailDev(fmt::format("(VTLB) Attempted write32 to unmapped physical address @ 0x{:08X}.", addr).c_str());
}

static void vtlbDefaultPhyWrite64(u32 addr,const mem64_t* data)
{
	pxFailDev(fmt::format("(VTLB) Attempted write64 to unmapped physical address @ 0x{:08X}.", addr).c_str());
}

static void vtlbDefaultPhyWrite128(u32 addr,const mem128_t* data)
{
	pxFailDev(fmt::format("(VTLB) Attempted write128 to unmapped physical address @ 0x{:08X}.", addr).c_str());
}

// ===========================================================================================
//  VTLB Public API -- Init/Term/RegisterHandler stuff 
// ===========================================================================================
//

// Assigns or re-assigns the callbacks for a VTLB memory handler.  The handler defines specific behavior
// for how memory pages bound to the handler are read from / written to.  If any of the handler pointers
// are NULL, the memory operations will be mapped to the BusError handler (thus generating BusError
// exceptions if the emulated app attempts to access them).
//
// Note: All handlers persist across calls to vtlb_Reset(), but are wiped/invalidated by calls to vtlb_Init()
//
__ri void vtlb_ReassignHandler( vtlbHandler rv,
							   vtlbMemR8FP* r8,vtlbMemR16FP* r16,vtlbMemR32FP* r32,vtlbMemR64FP* r64,vtlbMemR128FP* r128,
							   vtlbMemW8FP* w8,vtlbMemW16FP* w16,vtlbMemW32FP* w32,vtlbMemW64FP* w64,vtlbMemW128FP* w128 )
{
	pxAssume(rv < VTLB_HANDLER_ITEMS);

	vtlbdata.RWFT[0][0][rv] = (void*)((r8!=0)   ? r8	: vtlbDefaultPhyRead8);
	vtlbdata.RWFT[1][0][rv] = (void*)((r16!=0)  ? r16	: vtlbDefaultPhyRead16);
	vtlbdata.RWFT[2][0][rv] = (void*)((r32!=0)  ? r32	: vtlbDefaultPhyRead32);
	vtlbdata.RWFT[3][0][rv] = (void*)((r64!=0)  ? r64	: vtlbDefaultPhyRead64);
	vtlbdata.RWFT[4][0][rv] = (void*)((r128!=0) ? r128	: vtlbDefaultPhyRead128);

	vtlbdata.RWFT[0][1][rv] = (void*)((w8!=0)   ? w8	: vtlbDefaultPhyWrite8);
	vtlbdata.RWFT[1][1][rv] = (void*)((w16!=0)  ? w16	: vtlbDefaultPhyWrite16);
	vtlbdata.RWFT[2][1][rv] = (void*)((w32!=0)  ? w32	: vtlbDefaultPhyWrite32);
	vtlbdata.RWFT[3][1][rv] = (void*)((w64!=0)  ? w64	: vtlbDefaultPhyWrite64);
	vtlbdata.RWFT[4][1][rv] = (void*)((w128!=0) ? w128	: vtlbDefaultPhyWrite128);
}

vtlbHandler vtlb_NewHandler()
{
	pxAssertDev( vtlbHandlerCount < VTLB_HANDLER_ITEMS, "VTLB handler count overflow!" );
	return vtlbHandlerCount++;
}

// Registers a handler into the VTLB's internal handler array.  The handler defines specific behavior
// for how memory pages bound to the handler are read from / written to.  If any of the handler pointers
// are NULL, the memory operations will be mapped to the BusError handler (thus generating BusError
// exceptions if the emulated app attempts to access them).
//
// Note: All handlers persist across calls to vtlb_Reset(), but are wiped/invalidated by calls to vtlb_Init()
//
// Returns a handle for the newly created handler  See vtlb_MapHandler for use of the return value.
//
__ri vtlbHandler vtlb_RegisterHandler(	vtlbMemR8FP* r8,vtlbMemR16FP* r16,vtlbMemR32FP* r32,vtlbMemR64FP* r64,vtlbMemR128FP* r128,
										vtlbMemW8FP* w8,vtlbMemW16FP* w16,vtlbMemW32FP* w32,vtlbMemW64FP* w64,vtlbMemW128FP* w128)
{
	vtlbHandler rv = vtlb_NewHandler();
	vtlb_ReassignHandler( rv, r8, r16, r32, r64, r128, w8, w16, w32, w64, w128 );
	return rv;
}


// Maps the given hander (created with vtlb_RegisterHandler) to the specified memory region.
// New mappings always assume priority over previous mappings, so place "generic" mappings for
// large areas of memory first, and then specialize specific small regions of memory afterward.
// A single handler can be mapped to many different regions by using multiple calls to this
// function.
//
// The memory region start and size parameters must be pagesize aligned.
void vtlb_MapHandler(vtlbHandler handler, u32 start, u32 size)
{
	verify(0==(start&VTLB_PAGE_MASK));
	verify(0==(size&VTLB_PAGE_MASK) && size>0);

	u32 end = start + (size - VTLB_PAGE_SIZE);
	pxAssume( (end>>VTLB_PAGE_BITS) < std::size(vtlbdata.pmap) );

	while (start <= end)
	{
		vtlbdata.pmap[start>>VTLB_PAGE_BITS] = VTLBPhysical::fromHandler(handler);
		start += VTLB_PAGE_SIZE;
	}
}

void vtlb_MapBlock(void* base, u32 start, u32 size, u32 blocksize)
{
	verify(0==(start&VTLB_PAGE_MASK));
	verify(0==(size&VTLB_PAGE_MASK) && size>0);
	if(!blocksize)
		blocksize = size;
	verify(0==(blocksize&VTLB_PAGE_MASK) && blocksize>0);
	verify(0==(size%blocksize));

	sptr baseint = (sptr)base;
	u32 end = start + (size - VTLB_PAGE_SIZE);
	verify((end>>VTLB_PAGE_BITS) < std::size(vtlbdata.pmap));

	while (start <= end)
	{
		u32 loopsz = blocksize;
		sptr ptr = baseint;

		while (loopsz > 0)
		{
			vtlbdata.pmap[start>>VTLB_PAGE_BITS] = VTLBPhysical::fromPointer(ptr);

			start	+= VTLB_PAGE_SIZE;
			ptr		+= VTLB_PAGE_SIZE;
			loopsz	-= VTLB_PAGE_SIZE;
		}
	}
}

void vtlb_Mirror(u32 new_region,u32 start,u32 size)
{
	verify(0==(new_region&VTLB_PAGE_MASK));
	verify(0==(start&VTLB_PAGE_MASK));
	verify(0==(size&VTLB_PAGE_MASK) && size>0);

	u32 end = start + (size-VTLB_PAGE_SIZE);
	verify((end>>VTLB_PAGE_BITS) < std::size(vtlbdata.pmap));

	while(start <= end)
	{
		vtlbdata.pmap[start>>VTLB_PAGE_BITS] = vtlbdata.pmap[new_region>>VTLB_PAGE_BITS];

		start		+= VTLB_PAGE_SIZE;
		new_region	+= VTLB_PAGE_SIZE;
	}
}

__fi void* vtlb_GetPhyPtr(u32 paddr)
{
	if (paddr>=VTLB_PMAP_SZ || vtlbdata.pmap[paddr>>VTLB_PAGE_BITS].isHandler())
		return NULL;
	else
		return reinterpret_cast<void*>(vtlbdata.pmap[paddr>>VTLB_PAGE_BITS].assumePtr()+(paddr&VTLB_PAGE_MASK));
}

__fi u32 vtlb_V2P(u32 vaddr)
{
	u32 paddr = vtlbdata.ppmap[vaddr>>VTLB_PAGE_BITS];
	paddr    |= vaddr & VTLB_PAGE_MASK;
	return paddr;
}

//virtual mappings
//TODO: Add invalid paddr checks
void vtlb_VMap(u32 vaddr,u32 paddr,u32 size)
{
	verify(0==(vaddr&VTLB_PAGE_MASK));
	verify(0==(paddr&VTLB_PAGE_MASK));
	verify(0==(size&VTLB_PAGE_MASK) && size>0);

	while (size > 0)
	{
		VTLBVirtual vmv;
		if (paddr >= VTLB_PMAP_SZ) {
			if ((s32)paddr >= 0) {
				vmv = VTLBVirtual(VTLBPhysical::fromHandler(UnmappedPhyHandler0), paddr, vaddr);
			} else {
				vmv = VTLBVirtual(VTLBPhysical::fromHandler(UnmappedPhyHandler1), paddr & ~(1<<31), vaddr);
			}
		} else {
			vmv = VTLBVirtual(vtlbdata.pmap[paddr>>VTLB_PAGE_BITS], paddr, vaddr);
		}

		vtlbdata.vmap[vaddr>>VTLB_PAGE_BITS] = vmv;
		if (vtlbdata.ppmap)
			if (!(vaddr & 0x80000000)) // those address are already physical don't change them
				vtlbdata.ppmap[vaddr>>VTLB_PAGE_BITS] = paddr & ~VTLB_PAGE_MASK;

		vaddr += VTLB_PAGE_SIZE;
		paddr += VTLB_PAGE_SIZE;
		size -= VTLB_PAGE_SIZE;
	}
}

void vtlb_VMapBuffer(u32 vaddr,void* buffer,u32 size)
{
	verify(0==(vaddr&VTLB_PAGE_MASK));
	verify(0==(size&VTLB_PAGE_MASK) && size>0);

	uptr bu8 = (uptr)buffer;
	while (size > 0)
	{
		vtlbdata.vmap[vaddr>>VTLB_PAGE_BITS] = VTLBVirtual::fromPointer(bu8, vaddr);
		vaddr += VTLB_PAGE_SIZE;
		bu8 += VTLB_PAGE_SIZE;
		size -= VTLB_PAGE_SIZE;
	}
}

void vtlb_VMapUnmap(u32 vaddr,u32 size)
{
	verify(0==(vaddr&VTLB_PAGE_MASK));
	verify(0==(size&VTLB_PAGE_MASK) && size>0);

	while (size > 0)
	{

		VTLBVirtual handl;
		if ((s32)vaddr >= 0) {
			handl = VTLBVirtual(VTLBPhysical::fromHandler(UnmappedVirtHandler0), vaddr, vaddr);
		} else {
			handl = VTLBVirtual(VTLBPhysical::fromHandler(UnmappedVirtHandler1), vaddr & ~(1<<31), vaddr);
		}

		vtlbdata.vmap[vaddr>>VTLB_PAGE_BITS] = handl;
		vaddr += VTLB_PAGE_SIZE;
		size -= VTLB_PAGE_SIZE;
	}
}

// vtlb_Init -- Clears vtlb handlers and memory mappings.
void vtlb_Init()
{
	vtlbHandlerCount=0;
	memzero(vtlbdata.RWFT);

#define VTLB_BuildUnmappedHandler(baseName, highBit) \
	baseName##ReadSm<mem8_t,0>,		baseName##ReadSm<mem16_t,0>,	baseName##ReadSm<mem32_t,0>, \
	baseName##ReadLg<mem64_t,0>,	baseName##ReadLg<mem128_t,0>, \
	baseName##WriteSm<mem8_t,0>,	baseName##WriteSm<mem16_t,0>,	baseName##WriteSm<mem32_t,0>, \
	baseName##WriteLg<mem64_t,0>,	baseName##WriteLg<mem128_t,0>

	//Register default handlers
	//Unmapped Virt handlers _MUST_ be registered first.
	//On address translation the top bit cannot be preserved.This is not normaly a problem since
	//the physical address space can be 'compressed' to just 29 bits.However, to properly handle exceptions
	//there must be a way to get the full address back.Thats why i use these 2 functions and encode the hi bit directly into em :)

	UnmappedVirtHandler0 = vtlb_RegisterHandler( VTLB_BuildUnmappedHandler(vtlbUnmappedV, 0) );
	UnmappedVirtHandler1 = vtlb_RegisterHandler( VTLB_BuildUnmappedHandler(vtlbUnmappedV, 0x80000000) );

	UnmappedPhyHandler0 = vtlb_RegisterHandler( VTLB_BuildUnmappedHandler(vtlbUnmappedP, 0) );
	UnmappedPhyHandler1 = vtlb_RegisterHandler( VTLB_BuildUnmappedHandler(vtlbUnmappedP, 0x80000000) );

	DefaultPhyHandler = vtlb_RegisterHandler(0,0,0,0,0,0,0,0,0,0);

	//done !

	//Setup the initial mappings
	vtlb_MapHandler(DefaultPhyHandler,0,VTLB_PMAP_SZ);

	//Set the V space as unmapped
	vtlb_VMapUnmap(0,(VTLB_VMAP_ITEMS-1)*VTLB_PAGE_SIZE);
	//yeah i know, its stupid .. but this code has to be here for now ;p
	vtlb_VMapUnmap((VTLB_VMAP_ITEMS-1)*VTLB_PAGE_SIZE,VTLB_PAGE_SIZE);

	// The LUT is only used for 1 game so we allocate it only when the gamefix is enabled (save 4MB)
	if (EmuConfig.Gamefixes.GoemonTlbHack)
		vtlb_Alloc_Ppmap();

	extern void vtlb_dynarec_init();
	vtlb_dynarec_init();
}

// vtlb_Reset -- Performs a COP0-level reset of the PS2's TLB.
// This function should probably be part of the COP0 rather than here in VTLB.
void vtlb_Reset()
{
	for(int i=0; i<48; i++) UnmapTLB(i);
}

void vtlb_Term()
{
	//nothing to do for now
}

static constexpr size_t VMAP_SIZE = sizeof(VTLBVirtual) * VTLB_VMAP_ITEMS;

// Reserves the vtlb core allocation used by various emulation components!
// [TODO] basemem - request allocating memory at the specified virtual location, which can allow
//    for easier debugging and/or 3rd party cheat programs.  If 0, the operating system
//    default is used.
void vtlb_Core_Alloc()
{
	// Can't return regions to the bump allocator
	static VTLBVirtual* vmap = nullptr;
	if (!vmap)
		vmap = (VTLBVirtual*)GetVmMemory().BumpAllocator().Alloc(VMAP_SIZE);
	if (!vtlbdata.vmap)
	{
		bool okay = HostSys::MmapCommitPtr(vmap, VMAP_SIZE, PageProtectionMode().Read().Write());
		if (okay) {
			vtlbdata.vmap = vmap;
		} else {
			throw Exception::OutOfMemory( "VTLB Virtual Address Translation LUT" )
				.SetDiagMsg(fmt::format("({} megs)", VTLB_VMAP_ITEMS * sizeof(*vtlbdata.vmap) / _1mb)
			);
		}
	}
}

static constexpr size_t PPMAP_SIZE = sizeof(*vtlbdata.ppmap) * VTLB_VMAP_ITEMS;

// The LUT is only used for 1 game so we allocate it only when the gamefix is enabled (save 4MB)
// However automatic gamefix is done after the standard init so a new init function was done.
void vtlb_Alloc_Ppmap()
{
	if (vtlbdata.ppmap)
		return;

	static u32* ppmap = nullptr;

	if (!ppmap)
		ppmap = (u32*)GetVmMemory().BumpAllocator().Alloc(PPMAP_SIZE);

	bool okay = HostSys::MmapCommitPtr(ppmap, PPMAP_SIZE, PageProtectionMode().Read().Write());
	if (okay)
		vtlbdata.ppmap = ppmap;
	else
		throw Exception::OutOfMemory("VTLB PS2 Virtual Address Translation LUT")
			.SetDiagMsg(fmt::format("({} megs)", PPMAP_SIZE / _1mb));

	// By default a 1:1 virtual to physical mapping
	for (u32 i = 0; i < VTLB_VMAP_ITEMS; i++)
		vtlbdata.ppmap[i] = i<<VTLB_PAGE_BITS;
}

void vtlb_Core_Free()
{
	if (vtlbdata.vmap)
	{
		HostSys::MmapResetPtr(vtlbdata.vmap, VMAP_SIZE);
		vtlbdata.vmap = nullptr;
	}
	if (vtlbdata.ppmap)
	{
		HostSys::MmapResetPtr(vtlbdata.ppmap, PPMAP_SIZE);
		vtlbdata.ppmap = nullptr;
	}
}

static std::string GetHostVmErrorMsg()
{
	return "Your system is too low on virtual resources for PCSX2 to run. This can be caused by having a small or disabled swapfile, or by other programs that are hogging resources.";
}
// --------------------------------------------------------------------------------------
//  VtlbMemoryReserve  (implementations)
// --------------------------------------------------------------------------------------
VtlbMemoryReserve::VtlbMemoryReserve( std::string name, size_t size )
	: m_reserve( std::move(name), size )
{
	m_reserve.SetPageAccessOnCommit( PageAccess_ReadWrite() );
}

void VtlbMemoryReserve::Reserve( VirtualMemoryManagerPtr allocator, sptr offset )
{
	if (!m_reserve.Reserve( std::move(allocator), offset ))
	{
		throw Exception::OutOfMemory( m_reserve.GetName() )
			.SetDiagMsg("Vtlb memory could not be reserved.")
			.SetUserMsg(GetHostVmErrorMsg());
	}
}

void VtlbMemoryReserve::Commit()
{
	if (IsCommitted()) return;
	if (!m_reserve.Commit())
	{
		throw Exception::OutOfMemory( m_reserve.GetName() )
			.SetDiagMsg("Vtlb memory could not be committed.")
			.SetUserMsg(GetHostVmErrorMsg());
	}
}

void VtlbMemoryReserve::Reset()
{
	Commit();
	memzero_sse_a(m_reserve.GetPtr(), m_reserve.GetCommittedBytes());
}

void VtlbMemoryReserve::Decommit()
{
	m_reserve.Reset();
}

bool VtlbMemoryReserve::IsCommitted() const
{
	return !!m_reserve.GetCommittedPageCount();
}
