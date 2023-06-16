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
#include "IopMem.h"
#include "Host.h"
#include "VMManager.h"

#include "common/Align.h"

#include "fmt/core.h"

#include <map>
#include <unordered_set>
#include <unordered_map>

#define FASTMEM_LOG(...)
//#define FASTMEM_LOG(...) Console.WriteLn(__VA_ARGS__)

using namespace R5900;
using namespace vtlb_private;

#define verify pxAssert

namespace vtlb_private
{
	alignas(64) MapData vtlbdata;

	static bool PageFaultHandler(const PageFaultInfo& info);
} // namespace vtlb_private

static vtlbHandler vtlbHandlerCount = 0;

static vtlbHandler DefaultPhyHandler;
static vtlbHandler UnmappedVirtHandler;
static vtlbHandler UnmappedPhyHandler;

struct FastmemVirtualMapping
{
	u32 offset;
	u32 size;
};

struct LoadstoreBackpatchInfo
{
	u32 guest_pc;
	u32 gpr_bitmask;
	u32 fpr_bitmask;
	u8 code_size;
	u8 address_register;
	u8 data_register;
	u8 size_in_bits;
	bool is_signed;
	bool is_load;
	bool is_fpr;
};

static constexpr size_t FASTMEM_AREA_SIZE = 0x100000000ULL;
static constexpr u32 FASTMEM_PAGE_COUNT = FASTMEM_AREA_SIZE / VTLB_PAGE_SIZE;
static constexpr u32 NO_FASTMEM_MAPPING = 0xFFFFFFFFu;

static std::unique_ptr<SharedMemoryMappingArea> s_fastmem_area;
static std::vector<u32> s_fastmem_virtual_mapping; // maps vaddr -> mainmem offset
static std::unordered_multimap<u32, u32> s_fastmem_physical_mapping; // maps mainmem offset -> vaddr
static std::unordered_map<uptr, LoadstoreBackpatchInfo> s_fastmem_backpatch_info;
static std::unordered_set<u32> s_fastmem_faulting_pcs;

vtlb_private::VTLBPhysical vtlb_private::VTLBPhysical::fromPointer(sptr ptr)
{
	pxAssertMsg(ptr >= 0, "Address too high");
	return VTLBPhysical(ptr);
}

vtlb_private::VTLBPhysical vtlb_private::VTLBPhysical::fromHandler(vtlbHandler handler)
{
	return VTLBPhysical(handler | POINTER_SIGN_BIT);
}

vtlb_private::VTLBVirtual::VTLBVirtual(VTLBPhysical phys, u32 paddr, u32 vaddr)
{
	pxAssertMsg(0 == (paddr & VTLB_PAGE_MASK), "Should be page aligned");
	pxAssertMsg(0 == (vaddr & VTLB_PAGE_MASK), "Should be page aligned");
	pxAssertMsg((uptr)paddr < POINTER_SIGN_BIT, "Address too high");
	if (phys.isHandler())
	{
		value = phys.raw() + paddr - vaddr;
	}
	else
	{
		value = phys.raw() - vaddr;
	}
}

__inline int CheckCache(u32 addr)
{
	u32 mask;

	if (((cpuRegs.CP0.n.Config >> 16) & 0x1) == 0)
	{
		//DevCon.Warning("Data Cache Disabled! %x", cpuRegs.CP0.n.Config);
		return false; //
	}

	for (int i = 1; i < 48; i++)
	{
		if (((tlb[i].EntryLo1 & 0x38) >> 3) == 0x3)
		{
			mask = tlb[i].PageMask;

			if ((addr >= tlb[i].PFN1) && (addr <= tlb[i].PFN1 + mask))
			{
				//DevCon.Warning("Yay! Cache check cache addr=%x, mask=%x, addr+mask=%x, VPN2=%x PFN0=%x", addr, mask, (addr & mask), tlb[i].VPN2, tlb[i].PFN0);
				return true;
			}
		}
		if (((tlb[i].EntryLo0 & 0x38) >> 3) == 0x3)
		{
			mask = tlb[i].PageMask;

			if ((addr >= tlb[i].PFN0) && (addr <= tlb[i].PFN0 + mask))
			{
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

template <typename DataType>
DataType vtlb_memRead(u32 addr)
{
	static const uint DataSize = sizeof(DataType) * 8;
	auto vmv = vtlbdata.vmap[addr >> VTLB_PAGE_BITS];

	if (!vmv.isHandler(addr))
	{
		if (!CHECK_EEREC)
		{
			if (CHECK_CACHE && CheckCache(addr))
			{
				switch (DataSize)
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
					case 64:
						return readCache64(addr);
						break;

						jNO_DEFAULT;
				}
			}
		}

		return *reinterpret_cast<DataType*>(vmv.assumePtr(addr));
	}

	//has to: translate, find function, call function
	u32 paddr = vmv.assumeHandlerGetPAddr(addr);
	//Console.WriteLn("Translated 0x%08X to 0x%08X", addr,paddr);
	//return reinterpret_cast<TemplateHelper<DataSize,false>::HandlerType*>(vtlbdata.RWFT[TemplateHelper<DataSize,false>::sidx][0][hand])(paddr,data);

	switch (DataSize)
	{
		case 8:
			return vmv.assumeHandler<8, false>()(paddr);
		case 16:
			return vmv.assumeHandler<16, false>()(paddr);
		case 32:
			return vmv.assumeHandler<32, false>()(paddr);
		case 64:
			return vmv.assumeHandler<64, false>()(paddr);

			jNO_DEFAULT;
	}

	return 0; // technically unreachable, but suppresses warnings.
}

RETURNS_R128 vtlb_memRead128(u32 mem)
{
	auto vmv = vtlbdata.vmap[mem >> VTLB_PAGE_BITS];

	if (!vmv.isHandler(mem))
	{
		if (!CHECK_EEREC)
		{
			if (CHECK_CACHE && CheckCache(mem))
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

template <typename DataType>
void vtlb_memWrite(u32 addr, DataType data)
{
	static const uint DataSize = sizeof(DataType) * 8;

	auto vmv = vtlbdata.vmap[addr >> VTLB_PAGE_BITS];

	if (!vmv.isHandler(addr))
	{
		if (!CHECK_EEREC)
		{
			if (CHECK_CACHE && CheckCache(addr))
			{
				switch (DataSize)
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
					case 64:
						writeCache64(addr, data);
						return;
				}
			}
		}

		*reinterpret_cast<DataType*>(vmv.assumePtr(addr)) = data;
	}
	else
	{
		//has to: translate, find function, call function
		u32 paddr = vmv.assumeHandlerGetPAddr(addr);
		//Console.WriteLn("Translated 0x%08X to 0x%08X", addr,paddr);
		return vmv.assumeHandler<sizeof(DataType) * 8, true>()(paddr, data);
	}
}

void TAKES_R128 vtlb_memWrite128(u32 mem, r128 value)
{
	auto vmv = vtlbdata.vmap[mem >> VTLB_PAGE_BITS];

	if (!vmv.isHandler(mem))
	{
		if (!CHECK_EEREC)
		{
			if (CHECK_CACHE && CheckCache(mem))
			{
				alignas(16) const u128 r = r128_to_u128(value);
				writeCache128(mem, &r);
				return;
			}
		}

		r128_store_unaligned((void*)vmv.assumePtr(mem), value);
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
template mem64_t vtlb_memRead<mem64_t>(u32 mem);
template void vtlb_memWrite<mem8_t>(u32 mem, mem8_t data);
template void vtlb_memWrite<mem16_t>(u32 mem, mem16_t data);
template void vtlb_memWrite<mem32_t>(u32 mem, mem32_t data);
template void vtlb_memWrite<mem64_t>(u32 mem, mem64_t data);

template <typename DataType>
bool vtlb_ramRead(u32 addr, DataType* value)
{
	const auto vmv = vtlbdata.vmap[addr >> VTLB_PAGE_BITS];
	if (vmv.isHandler(addr))
	{
		std::memset(value, 0, sizeof(DataType));
		return false;
	}

	std::memcpy(value, reinterpret_cast<DataType*>(vmv.assumePtr(addr)), sizeof(DataType));
	return true;
}

template <typename DataType>
bool vtlb_ramWrite(u32 addr, const DataType& data)
{
	const auto vmv = vtlbdata.vmap[addr >> VTLB_PAGE_BITS];
	if (vmv.isHandler(addr))
		return false;

	std::memcpy(reinterpret_cast<DataType*>(vmv.assumePtr(addr)), &data, sizeof(DataType));
	return true;
}


template bool vtlb_ramRead<mem8_t>(u32 mem, mem8_t* value);
template bool vtlb_ramRead<mem16_t>(u32 mem, mem16_t* value);
template bool vtlb_ramRead<mem32_t>(u32 mem, mem32_t* value);
template bool vtlb_ramRead<mem64_t>(u32 mem, mem64_t* value);
template bool vtlb_ramRead<mem128_t>(u32 mem, mem128_t* value);
template bool vtlb_ramWrite<mem8_t>(u32 mem, const mem8_t& data);
template bool vtlb_ramWrite<mem16_t>(u32 mem, const mem16_t& data);
template bool vtlb_ramWrite<mem32_t>(u32 mem, const mem32_t& data);
template bool vtlb_ramWrite<mem64_t>(u32 mem, const mem64_t& data);
template bool vtlb_ramWrite<mem128_t>(u32 mem, const mem128_t& data);

int vtlb_memSafeCmpBytes(u32 mem, const void* src, u32 size)
{
	// can memcpy so long as pages aren't crossed
	const u8* sptr = static_cast<const u8*>(src);
	const u8* const sptr_end = sptr + size;
	while (sptr != sptr_end)
	{
		auto vmv = vtlbdata.vmap[mem >> VTLB_PAGE_BITS];
		if (vmv.isHandler(mem))
			return -1;

		const size_t remaining_in_page =
			std::min(VTLB_PAGE_SIZE - (mem & VTLB_PAGE_MASK), static_cast<u32>(sptr_end - sptr));
		const int res = std::memcmp(sptr, reinterpret_cast<void*>(vmv.assumePtr(mem)), remaining_in_page);
		if (res != 0)
			return res;

		sptr += remaining_in_page;
		mem += remaining_in_page;
	}

	return 0;
}

bool vtlb_memSafeReadBytes(u32 mem, void* dst, u32 size)
{
	// can memcpy so long as pages aren't crossed
	u8* dptr = static_cast<u8*>(dst);
	u8* const dptr_end = dptr + size;
	while (dptr != dptr_end)
	{
		auto vmv = vtlbdata.vmap[mem >> VTLB_PAGE_BITS];
		if (vmv.isHandler(mem))
			return false;

		const u32 remaining_in_page =
			std::min(VTLB_PAGE_SIZE - (mem & VTLB_PAGE_MASK), static_cast<u32>(dptr_end - dptr));
		std::memcpy(dptr, reinterpret_cast<void*>(vmv.assumePtr(mem)), remaining_in_page);
		dptr += remaining_in_page;
		mem += remaining_in_page;
	}

	return true;
}

bool vtlb_memSafeWriteBytes(u32 mem, const void* src, u32 size)
{
	// can memcpy so long as pages aren't crossed
	const u8* sptr = static_cast<const u8*>(src);
	const u8* const sptr_end = sptr + size;
	while (sptr != sptr_end)
	{
		auto vmv = vtlbdata.vmap[mem >> VTLB_PAGE_BITS];
		if (vmv.isHandler(mem))
			return false;

		const size_t remaining_in_page =
			std::min(VTLB_PAGE_SIZE - (mem & VTLB_PAGE_MASK), static_cast<u32>(sptr_end - sptr));
		std::memcpy(reinterpret_cast<void*>(vmv.assumePtr(mem)), sptr, remaining_in_page);
		sptr += remaining_in_page;
		mem += remaining_in_page;
	}

	return true;
}

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

	for (u32 i = 0; i < 150; i++)
	{
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

	for (u32 i = 0; i < 150; i++)
	{
		if (tlb[i].valid == 0x1 && tlb[i].low_add != tlb[i].high_add)
		{

			u32 size = tlb[i].high_add - tlb[i].low_add;
			u32 vaddr = tlb[i].low_add;
			u32 paddr = tlb[i].physical_add;

			// TODO: The old code (commented below) seems to check specifically for handler 0.  Is this really correct?
			//if ((uptr)vtlbdata.vmap[vaddr>>VTLB_PAGE_BITS] == POINTER_SIGN_BIT) {
			auto vmv = vtlbdata.vmap[vaddr >> VTLB_PAGE_BITS];
			if (vmv.isHandler(vaddr) && vmv.assumeHandlerGetID() == 0)
			{
				DevCon.WriteLn("GoemonPreloadTlb: Entry %d. Key %x. From V:0x%8.8x to P:0x%8.8x (%d pages)", i, tlb[i].key, vaddr, paddr, size >> VTLB_PAGE_BITS);
				vtlb_VMap(vaddr, paddr, size);
				vtlb_VMap(0x20000000 | vaddr, paddr, size);
			}
		}
	}
}

void GoemonUnloadTlb(u32 key)
{
	// 0x3d5580 is the address of the TLB cache table
	GoemonTlb* tlb = (GoemonTlb*)&eeMem->Main[0x3d5580];
	for (u32 i = 0; i < 150; i++)
	{
		if (tlb[i].key == key)
		{
			if (tlb[i].valid == 0x1)
			{
				u32 size = tlb[i].high_add - tlb[i].low_add;
				u32 vaddr = tlb[i].low_add;
				DevCon.WriteLn("GoemonUnloadTlb: Entry %d. Key %x. From V:0x%8.8x to V:0x%8.8x (%d pages)", i, tlb[i].key, vaddr, vaddr + size, size >> VTLB_PAGE_BITS);

				vtlb_VMapUnmap(vaddr, size);
				vtlb_VMapUnmap(0x20000000 | vaddr, size);

				// Unmap the tlb in game cache table
				// Note: Game copy FEFEFEFE for others data
				tlb[i].valid = 0;
				tlb[i].key = 0xFEFEFEFE;
				tlb[i].low_add = 0xFEFEFEFE;
				tlb[i].high_add = 0xFEFEFEFE;
			}
			else
			{
				DevCon.Error("GoemonUnloadTlb: Entry %d is not valid. Key %x", i, tlb[i].key);
			}
		}
	}
}

// Generates a tlbMiss Exception
static __ri void vtlb_Miss(u32 addr, u32 mode)
{
	if (EmuConfig.Gamefixes.GoemonTlbHack)
		GoemonTlbMissDebug();

	// Hack to handle expected tlb miss by some games.
	if (Cpu == &intCpu)
	{
		if (mode)
			cpuTlbMissW(addr, cpuRegs.branch);
		else
			cpuTlbMissR(addr, cpuRegs.branch);

		// Exception handled. Current instruction need to be stopped
		Cpu->CancelInstruction();
		return;
	}

	const std::string message(fmt::format("TLB Miss, pc=0x{:x} addr=0x{:x} [{}]", cpuRegs.pc, addr, mode ? "store" : "load"));
	if (EmuConfig.Cpu.Recompiler.PauseOnTLBMiss)
	{
		// Pause, let the user try to figure out what went wrong in the debugger.
		Host::ReportErrorAsync("R5900 Exception", message);
		VMManager::SetPaused(true);
		Cpu->ExitExecution();
		return;
	}

	static int spamStop = 0;
	if (spamStop++ < 50 || IsDevBuild)
		Console.Error(message);
}

// BusError exception: more serious than a TLB miss.  If properly emulated the PS2 kernel
// itself would invoke a diagnostic/assertion screen that displays the cpu state at the
// time of the exception.
static __ri void vtlb_BusError(u32 addr, u32 mode)
{
	const std::string message(fmt::format("Bus Error, addr=0x{:x} [{}]", addr, mode ? "store" : "load"));
	if (EmuConfig.Cpu.Recompiler.PauseOnTLBMiss)
	{
		// Pause, let the user try to figure out what went wrong in the debugger.
		Host::ReportErrorAsync("R5900 Exception", message);
		VMManager::SetPaused(true);
		Cpu->ExitExecution();
		return;
	}

	Console.Error(message);
}

// clang-format off
template <typename OperandType>
static OperandType vtlbUnmappedVReadSm(u32 addr) { vtlb_Miss(addr, 0); return 0; }
static RETURNS_R128 vtlbUnmappedVReadLg(u32 addr) { vtlb_Miss(addr, 0); return r128_zero(); }

template <typename OperandType>
static void vtlbUnmappedVWriteSm(u32 addr, OperandType data) { vtlb_Miss(addr, 1); }
static void TAKES_R128 vtlbUnmappedVWriteLg(u32 addr, r128 data) { vtlb_Miss(addr, 1); }

template <typename OperandType>
static OperandType vtlbUnmappedPReadSm(u32 addr) { vtlb_BusError(addr, 0); return 0; }
static RETURNS_R128 vtlbUnmappedPReadLg(u32 addr) { vtlb_BusError(addr, 0); return r128_zero(); }

template <typename OperandType>
static void vtlbUnmappedPWriteSm(u32 addr, OperandType data) { vtlb_BusError(addr, 1); }
static void TAKES_R128 vtlbUnmappedPWriteLg(u32 addr, r128 data) { vtlb_BusError(addr, 1); }
// clang-format on

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

static mem64_t vtlbDefaultPhyRead64(u32 addr)
{
	pxFailDev(fmt::format("(VTLB) Attempted read64 from unmapped physical address @ 0x{:08X}.", addr).c_str());
	return 0;
}

static RETURNS_R128 vtlbDefaultPhyRead128(u32 addr)
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

static void vtlbDefaultPhyWrite64(u32 addr, mem64_t data)
{
	pxFailDev(fmt::format("(VTLB) Attempted write64 to unmapped physical address @ 0x{:08X}.", addr).c_str());
}

static void TAKES_R128 vtlbDefaultPhyWrite128(u32 addr, r128 data)
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
__ri void vtlb_ReassignHandler(vtlbHandler rv,
	vtlbMemR8FP* r8, vtlbMemR16FP* r16, vtlbMemR32FP* r32, vtlbMemR64FP* r64, vtlbMemR128FP* r128,
	vtlbMemW8FP* w8, vtlbMemW16FP* w16, vtlbMemW32FP* w32, vtlbMemW64FP* w64, vtlbMemW128FP* w128)
{
	pxAssume(rv < VTLB_HANDLER_ITEMS);

	vtlbdata.RWFT[0][0][rv] = (void*)((r8 != 0) ? r8 : vtlbDefaultPhyRead8);
	vtlbdata.RWFT[1][0][rv] = (void*)((r16 != 0) ? r16 : vtlbDefaultPhyRead16);
	vtlbdata.RWFT[2][0][rv] = (void*)((r32 != 0) ? r32 : vtlbDefaultPhyRead32);
	vtlbdata.RWFT[3][0][rv] = (void*)((r64 != 0) ? r64 : vtlbDefaultPhyRead64);
	vtlbdata.RWFT[4][0][rv] = (void*)((r128 != 0) ? r128 : vtlbDefaultPhyRead128);

	vtlbdata.RWFT[0][1][rv] = (void*)((w8 != 0) ? w8 : vtlbDefaultPhyWrite8);
	vtlbdata.RWFT[1][1][rv] = (void*)((w16 != 0) ? w16 : vtlbDefaultPhyWrite16);
	vtlbdata.RWFT[2][1][rv] = (void*)((w32 != 0) ? w32 : vtlbDefaultPhyWrite32);
	vtlbdata.RWFT[3][1][rv] = (void*)((w64 != 0) ? w64 : vtlbDefaultPhyWrite64);
	vtlbdata.RWFT[4][1][rv] = (void*)((w128 != 0) ? w128 : vtlbDefaultPhyWrite128);
}

vtlbHandler vtlb_NewHandler()
{
	pxAssertDev(vtlbHandlerCount < VTLB_HANDLER_ITEMS, "VTLB handler count overflow!");
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
__ri vtlbHandler vtlb_RegisterHandler(vtlbMemR8FP* r8, vtlbMemR16FP* r16, vtlbMemR32FP* r32, vtlbMemR64FP* r64, vtlbMemR128FP* r128,
	vtlbMemW8FP* w8, vtlbMemW16FP* w16, vtlbMemW32FP* w32, vtlbMemW64FP* w64, vtlbMemW128FP* w128)
{
	vtlbHandler rv = vtlb_NewHandler();
	vtlb_ReassignHandler(rv, r8, r16, r32, r64, r128, w8, w16, w32, w64, w128);
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
	verify(0 == (start & VTLB_PAGE_MASK));
	verify(0 == (size & VTLB_PAGE_MASK) && size > 0);

	u32 end = start + (size - VTLB_PAGE_SIZE);
	pxAssume((end >> VTLB_PAGE_BITS) < (sizeof(vtlbdata.pmap) / sizeof(vtlbdata.pmap[0])));

	while (start <= end)
	{
		vtlbdata.pmap[start >> VTLB_PAGE_BITS] = VTLBPhysical::fromHandler(handler);
		start += VTLB_PAGE_SIZE;
	}
}

void vtlb_MapBlock(void* base, u32 start, u32 size, u32 blocksize)
{
	verify(0 == (start & VTLB_PAGE_MASK));
	verify(0 == (size & VTLB_PAGE_MASK) && size > 0);
	if (!blocksize)
		blocksize = size;
	verify(0 == (blocksize & VTLB_PAGE_MASK) && blocksize > 0);
	verify(0 == (size % blocksize));

	sptr baseint = (sptr)base;
	u32 end = start + (size - VTLB_PAGE_SIZE);
	verify((end >> VTLB_PAGE_BITS) < std::size(vtlbdata.pmap));

	while (start <= end)
	{
		u32 loopsz = blocksize;
		sptr ptr = baseint;

		while (loopsz > 0)
		{
			vtlbdata.pmap[start >> VTLB_PAGE_BITS] = VTLBPhysical::fromPointer(ptr);

			start += VTLB_PAGE_SIZE;
			ptr += VTLB_PAGE_SIZE;
			loopsz -= VTLB_PAGE_SIZE;
		}
	}
}

void vtlb_Mirror(u32 new_region, u32 start, u32 size)
{
	verify(0 == (new_region & VTLB_PAGE_MASK));
	verify(0 == (start & VTLB_PAGE_MASK));
	verify(0 == (size & VTLB_PAGE_MASK) && size > 0);

	u32 end = start + (size - VTLB_PAGE_SIZE);
	verify((end >> VTLB_PAGE_BITS) < std::size(vtlbdata.pmap));

	while (start <= end)
	{
		vtlbdata.pmap[start >> VTLB_PAGE_BITS] = vtlbdata.pmap[new_region >> VTLB_PAGE_BITS];

		start += VTLB_PAGE_SIZE;
		new_region += VTLB_PAGE_SIZE;
	}
}

__fi void* vtlb_GetPhyPtr(u32 paddr)
{
	if (paddr >= VTLB_PMAP_SZ || vtlbdata.pmap[paddr >> VTLB_PAGE_BITS].isHandler())
		return NULL;
	else
		return reinterpret_cast<void*>(vtlbdata.pmap[paddr >> VTLB_PAGE_BITS].assumePtr() + (paddr & VTLB_PAGE_MASK));
}

__fi u32 vtlb_V2P(u32 vaddr)
{
	u32 paddr = vtlbdata.ppmap[vaddr >> VTLB_PAGE_BITS];
	paddr |= vaddr & VTLB_PAGE_MASK;
	return paddr;
}

static constexpr bool vtlb_MismatchedHostPageSize()
{
	return (__pagesize != VTLB_PAGE_SIZE);
}

static bool vtlb_IsHostAligned(u32 paddr)
{
	if constexpr (!vtlb_MismatchedHostPageSize())
		return true;

	return ((paddr & __pagemask) == 0);
}

static u32 vtlb_HostPage(u32 page)
{
	if constexpr (!vtlb_MismatchedHostPageSize())
		return page;

	return page >> (__pageshift - VTLB_PAGE_BITS);
}

static u32 vtlb_HostAlignOffset(u32 offset)
{
	if constexpr (!vtlb_MismatchedHostPageSize())
		return offset;

	return offset & ~__pagemask;
}

static bool vtlb_IsHostCoalesced(u32 page)
{
	if constexpr (__pagesize == VTLB_PAGE_SIZE)
	{
		return true;
	}
	else
	{
		static constexpr u32 shift = __pageshift - VTLB_PAGE_BITS;
		static constexpr u32 count = (1u << shift);
		static constexpr u32 mask = count - 1;

		const u32 base = page & ~mask;
		const u32 base_offset = s_fastmem_virtual_mapping[base];
		if ((base_offset & __pagemask) != 0)
			return false;

		for (u32 i = 0, expected_offset = base_offset; i < count; i++, expected_offset += VTLB_PAGE_SIZE)
		{
			if (s_fastmem_virtual_mapping[base + i] != expected_offset)
				return false;
		}

		return true;
	}
}

static bool vtlb_GetMainMemoryOffsetFromPtr(uptr ptr, u32* mainmem_offset, u32* mainmem_size, PageProtectionMode* prot)
{
	const uptr page_end = ptr + VTLB_PAGE_SIZE;
	SysMainMemory& vmmem = GetVmMemory();

	// EE memory and ROMs.
	if (ptr >= (uptr)eeMem->Main && page_end <= (uptr)eeMem->ZeroRead)
	{
		const u32 eemem_offset = static_cast<u32>(ptr - (uptr)eeMem->Main);
		const bool writeable = ((eemem_offset < Ps2MemSize::MainRam) ? (mmap_GetRamPageInfo(eemem_offset) != ProtMode_Write) : true);
		*mainmem_offset = (eemem_offset + HostMemoryMap::EEmemOffset);
		*mainmem_size = (offsetof(EEVM_MemoryAllocMess, ZeroRead) - eemem_offset);
		*prot = PageProtectionMode().Read().Write(writeable);
		return true;
	}

	// IOP memory.
	if (ptr >= (uptr)iopMem->Main && page_end <= (uptr)iopMem->P)
	{
		const u32 iopmem_offset = static_cast<u32>(ptr - (uptr)iopMem->Main);
		*mainmem_offset = iopmem_offset + HostMemoryMap::IOPmemOffset;
		*mainmem_size = (offsetof(IopVM_MemoryAllocMess, P) - iopmem_offset);
		*prot = PageProtectionMode().Read().Write();
		return true;
	}

	// VU memory - this includes both data and code for VU0/VU1.
	// Practically speaking, this is only data, because the code goes through a handler.
	if (ptr >= (uptr)vmmem.VUMemory().GetPtr() && page_end <= (uptr)vmmem.VUMemory().GetPtrEnd())
	{
		const u32 vumem_offset = static_cast<u32>(ptr - (uptr)vmmem.VUMemory().GetPtr());
		*mainmem_offset = vumem_offset + HostMemoryMap::VUmemOffset;
		*mainmem_size = vmmem.VUMemory().GetSize() - vumem_offset;
		*prot = PageProtectionMode().Read().Write();
		return true;
	}

	// We end up with some unknown mappings here; currently the IOP memory, instead of being physically mapped
	// as 2MB, ends up being mapped as 8MB. But this shouldn't be virtual mapped anyway, so fallback to slowmem
	// in such cases.
	return false;
}

static bool vtlb_GetMainMemoryOffset(u32 paddr, u32* mainmem_offset, u32* mainmem_size, PageProtectionMode* prot)
{
	if (paddr >= VTLB_PMAP_SZ)
		return false;

	// Handlers aren't in our shared memory, obviously.
	const VTLBPhysical& vm = vtlbdata.pmap[paddr >> VTLB_PAGE_BITS];
	if (vm.isHandler())
		return false;

	return vtlb_GetMainMemoryOffsetFromPtr(vm.raw(), mainmem_offset, mainmem_size, prot);
}

static void vtlb_CreateFastmemMapping(u32 vaddr, u32 mainmem_offset, const PageProtectionMode& mode)
{
	FASTMEM_LOG("Create fastmem mapping @ vaddr %08X mainmem %08X", vaddr, mainmem_offset);

	const u32 page = vaddr / VTLB_PAGE_SIZE;

	if (s_fastmem_virtual_mapping[page] == mainmem_offset)
	{
		// current mapping is fine
		return;
	}

	if (s_fastmem_virtual_mapping[page] != NO_FASTMEM_MAPPING)
	{
		// current mapping needs to be removed
		const bool was_coalesced = vtlb_IsHostCoalesced(page);

		s_fastmem_virtual_mapping[page] = NO_FASTMEM_MAPPING;
		if (was_coalesced && !s_fastmem_area->Unmap(s_fastmem_area->PagePointer(vtlb_HostPage(page)), __pagesize))
			Console.Error("Failed to unmap vaddr %08X", vaddr);

		// remove reverse mapping
		auto range = s_fastmem_physical_mapping.equal_range(mainmem_offset);
		for (auto it = range.first; it != range.second;)
		{
			auto this_it = it++;
			if (this_it->second == vaddr)
				s_fastmem_physical_mapping.erase(this_it);
		}
	}

	s_fastmem_virtual_mapping[page] = mainmem_offset;
	if (vtlb_IsHostCoalesced(page))
	{
		const u32 host_page = vtlb_HostPage(page);
		const u32 host_offset = vtlb_HostAlignOffset(mainmem_offset);

		if (!s_fastmem_area->Map(GetVmMemory().MainMemory()->GetFileHandle(), host_offset,
				s_fastmem_area->PagePointer(host_page), __pagesize, mode))
		{
			Console.Error("Failed to map vaddr %08X to mainmem offset %08X", vtlb_HostAlignOffset(vaddr), host_offset);
			s_fastmem_virtual_mapping[page] = NO_FASTMEM_MAPPING;
			return;
		}
	}

	s_fastmem_physical_mapping.emplace(mainmem_offset, vaddr);
}

static void vtlb_RemoveFastmemMapping(u32 vaddr)
{
	const u32 page = vaddr / VTLB_PAGE_SIZE;
	if (s_fastmem_virtual_mapping[page] == NO_FASTMEM_MAPPING)
		return;

	const u32 mainmem_offset = s_fastmem_virtual_mapping[page];
	const bool was_coalesced = vtlb_IsHostCoalesced(page);
	FASTMEM_LOG("Remove fastmem mapping @ vaddr %08X mainmem %08X", vaddr, mainmem_offset);
	s_fastmem_virtual_mapping[page] = NO_FASTMEM_MAPPING;

	if (was_coalesced && !s_fastmem_area->Unmap(s_fastmem_area->PagePointer(vtlb_HostPage(page)), __pagesize))
		Console.Error("Failed to unmap vaddr %08X", vtlb_HostAlignOffset(vaddr));

	// remove from reverse map
	auto range = s_fastmem_physical_mapping.equal_range(mainmem_offset);
	for (auto it = range.first; it != range.second;)
	{
		auto this_it = it++;
		if (this_it->second == vaddr)
			s_fastmem_physical_mapping.erase(this_it);
	}
}

static void vtlb_RemoveFastmemMappings(u32 vaddr, u32 size)
{
	pxAssert((vaddr & VTLB_PAGE_MASK) == 0);
	pxAssert(size > 0 && (size & VTLB_PAGE_MASK) == 0);

	const u32 num_pages = size / VTLB_PAGE_SIZE;
	for (u32 i = 0; i < num_pages; i++, vaddr += VTLB_PAGE_SIZE)
		vtlb_RemoveFastmemMapping(vaddr);
}

static void vtlb_RemoveFastmemMappings()
{
	if (s_fastmem_virtual_mapping.empty())
	{
		// not initialized yet
		return;
	}

	for (u32 page = 0; page < FASTMEM_PAGE_COUNT; page++)
	{
		if (s_fastmem_virtual_mapping[page] == NO_FASTMEM_MAPPING)
			continue;

		s_fastmem_virtual_mapping[page] = NO_FASTMEM_MAPPING;

		if (!vtlb_IsHostAligned(page << VTLB_PAGE_BITS))
			continue;

		if (!s_fastmem_area->Unmap(s_fastmem_area->PagePointer(vtlb_HostPage(page)), __pagesize))
			Console.Error("Failed to unmap vaddr %08X", page * __pagesize);
	}

	s_fastmem_physical_mapping.clear();
}

bool vtlb_ResolveFastmemMapping(uptr* addr)
{
	uptr uaddr = *addr;
	uptr fastmem_start = (uptr)vtlbdata.fastmem_base;
	uptr fastmem_end = fastmem_start + 0xFFFFFFFFu;
	if (uaddr < fastmem_start || uaddr > fastmem_end)
		return false;

	const u32 vaddr = static_cast<u32>(uaddr - fastmem_start);
	FASTMEM_LOG("Trying to resolve %p (vaddr %08X)", (void*)uaddr, vaddr);

	const u32 vpage = vaddr / VTLB_PAGE_SIZE;
	if (s_fastmem_virtual_mapping[vpage] == NO_FASTMEM_MAPPING)
	{
		FASTMEM_LOG("%08X is not virtual mapped", vaddr);
		return false;
	}

	const u32 mainmem_offset = s_fastmem_virtual_mapping[vpage] + (vaddr & VTLB_PAGE_MASK);
	FASTMEM_LOG("Resolved %p (vaddr %08X) to mainmem offset %08X", uaddr, vaddr, mainmem_offset);
	*addr = ((uptr)GetVmMemory().MainMemory()->GetBase()) + mainmem_offset;
	return true;
}

bool vtlb_GetGuestAddress(uptr host_addr, u32* guest_addr)
{
	uptr fastmem_start = (uptr)vtlbdata.fastmem_base;
	uptr fastmem_end = fastmem_start + 0xFFFFFFFFu;
	if (host_addr < fastmem_start || host_addr > fastmem_end)
		return false;

	*guest_addr = static_cast<u32>(host_addr - fastmem_start);
	return true;
}

void vtlb_UpdateFastmemProtection(u32 paddr, u32 size, const PageProtectionMode& prot)
{
	if (!CHECK_FASTMEM)
		return;

	pxAssert((paddr & VTLB_PAGE_MASK) == 0);
	pxAssert(size > 0 && (size & VTLB_PAGE_MASK) == 0);

	u32 mainmem_start, mainmem_size;
	PageProtectionMode old_prot;
	if (!vtlb_GetMainMemoryOffset(paddr, &mainmem_start, &mainmem_size, &old_prot))
		return;

	FASTMEM_LOG("UpdateFastmemProtection %08X mmoffset %08X %08X", paddr, mainmem_start, size);

	u32 current_mainmem = mainmem_start;
	const u32 num_pages = std::min(size, mainmem_size) / VTLB_PAGE_SIZE;
	for (u32 i = 0; i < num_pages; i++, current_mainmem += VTLB_PAGE_SIZE)
	{
		// update virtual mapping mapping
		auto range = s_fastmem_physical_mapping.equal_range(current_mainmem);
		for (auto it = range.first; it != range.second; ++it)
		{
			FASTMEM_LOG("  valias %08X (size %u)", it->second, VTLB_PAGE_SIZE);

			if (vtlb_IsHostAligned(it->second))
				HostSys::MemProtect(s_fastmem_area->OffsetPointer(it->second), __pagesize, prot);
		}
	}
}

void vtlb_ClearLoadStoreInfo()
{
	s_fastmem_backpatch_info.clear();
	s_fastmem_faulting_pcs.clear();
}

void vtlb_AddLoadStoreInfo(uptr code_address, u32 code_size, u32 guest_pc, u32 gpr_bitmask, u32 fpr_bitmask, u8 address_register, u8 data_register, u8 size_in_bits, bool is_signed, bool is_load, bool is_fpr)
{
	pxAssert(code_size < std::numeric_limits<u8>::max());

	auto iter = s_fastmem_backpatch_info.find(code_address);
	if (iter != s_fastmem_backpatch_info.end())
		s_fastmem_backpatch_info.erase(iter);

	LoadstoreBackpatchInfo info{guest_pc, gpr_bitmask, fpr_bitmask, static_cast<u8>(code_size), address_register, data_register, size_in_bits, is_signed, is_load, is_fpr};
	s_fastmem_backpatch_info.emplace(code_address, info);
}

bool vtlb_BackpatchLoadStore(uptr code_address, uptr fault_address)
{
	uptr fastmem_start = (uptr)vtlbdata.fastmem_base;
	uptr fastmem_end = fastmem_start + 0xFFFFFFFFu;
	if (fault_address < fastmem_start || fault_address > fastmem_end)
		return false;

	auto iter = s_fastmem_backpatch_info.find(code_address);
	if (iter == s_fastmem_backpatch_info.end())
		return false;

	const LoadstoreBackpatchInfo& info = iter->second;
	const u32 guest_addr = static_cast<u32>(fault_address - fastmem_start);
	vtlb_DynBackpatchLoadStore(code_address, info.code_size, info.guest_pc, guest_addr,
		info.gpr_bitmask, info.fpr_bitmask, info.address_register, info.data_register,
		info.size_in_bits, info.is_signed, info.is_load, info.is_fpr);

	// queue block for recompilation later
	Cpu->Clear(info.guest_pc, 1);

	// and store the pc in the faulting list, so that we don't emit another fastmem loadstore
	s_fastmem_faulting_pcs.insert(info.guest_pc);
	s_fastmem_backpatch_info.erase(iter);
	return true;
}

bool vtlb_IsFaultingPC(u32 guest_pc)
{
	return (s_fastmem_faulting_pcs.find(guest_pc) != s_fastmem_faulting_pcs.end());
}

//virtual mappings
//TODO: Add invalid paddr checks
void vtlb_VMap(u32 vaddr, u32 paddr, u32 size)
{
	verify(0 == (vaddr & VTLB_PAGE_MASK));
	verify(0 == (paddr & VTLB_PAGE_MASK));
	verify(0 == (size & VTLB_PAGE_MASK) && size > 0);

	if (CHECK_FASTMEM)
	{
		const u32 num_pages = size / VTLB_PAGE_SIZE;
		u32 current_vaddr = vaddr;
		u32 current_paddr = paddr;

		for (u32 i = 0; i < num_pages; i++, current_vaddr += VTLB_PAGE_SIZE, current_paddr += VTLB_PAGE_SIZE)
		{
			u32 hoffset, hsize;
			PageProtectionMode mode;
			if (vtlb_GetMainMemoryOffset(current_paddr, &hoffset, &hsize, &mode))
				vtlb_CreateFastmemMapping(current_vaddr, hoffset, mode);
			else
				vtlb_RemoveFastmemMapping(current_vaddr);
		}
	}

	while (size > 0)
	{
		VTLBVirtual vmv;
		if (paddr >= VTLB_PMAP_SZ)
			vmv = VTLBVirtual(VTLBPhysical::fromHandler(UnmappedPhyHandler), paddr, vaddr);
		else
			vmv = VTLBVirtual(vtlbdata.pmap[paddr >> VTLB_PAGE_BITS], paddr, vaddr);

		vtlbdata.vmap[vaddr >> VTLB_PAGE_BITS] = vmv;
		if (vtlbdata.ppmap)
		{
			if (!(vaddr & 0x80000000)) // those address are already physical don't change them
				vtlbdata.ppmap[vaddr >> VTLB_PAGE_BITS] = paddr & ~VTLB_PAGE_MASK;
		}

		vaddr += VTLB_PAGE_SIZE;
		paddr += VTLB_PAGE_SIZE;
		size -= VTLB_PAGE_SIZE;
	}
}

void vtlb_VMapBuffer(u32 vaddr, void* buffer, u32 size)
{
	verify(0 == (vaddr & VTLB_PAGE_MASK));
	verify(0 == (size & VTLB_PAGE_MASK) && size > 0);

	if (CHECK_FASTMEM)
	{
		if (buffer == eeMem->Scratch && size == Ps2MemSize::Scratch)
		{
			u32 fm_vaddr = vaddr;
			u32 fm_hostoffset = HostMemoryMap::EEmemOffset + offsetof(EEVM_MemoryAllocMess, Scratch);
			PageProtectionMode mode = PageProtectionMode().Read().Write();
			for (u32 i = 0; i < (Ps2MemSize::Scratch / VTLB_PAGE_SIZE); i++, fm_vaddr += VTLB_PAGE_SIZE, fm_hostoffset += VTLB_PAGE_SIZE)
				vtlb_CreateFastmemMapping(fm_vaddr, fm_hostoffset, mode);
		}
		else
		{
			vtlb_RemoveFastmemMappings(vaddr, size);
		}
	}

	uptr bu8 = (uptr)buffer;
	while (size > 0)
	{
		vtlbdata.vmap[vaddr >> VTLB_PAGE_BITS] = VTLBVirtual::fromPointer(bu8, vaddr);
		vaddr += VTLB_PAGE_SIZE;
		bu8 += VTLB_PAGE_SIZE;
		size -= VTLB_PAGE_SIZE;
	}
}

void vtlb_VMapUnmap(u32 vaddr, u32 size)
{
	verify(0 == (vaddr & VTLB_PAGE_MASK));
	verify(0 == (size & VTLB_PAGE_MASK) && size > 0);

	vtlb_RemoveFastmemMappings(vaddr, size);

	while (size > 0)
	{
		vtlbdata.vmap[vaddr >> VTLB_PAGE_BITS] = VTLBVirtual(VTLBPhysical::fromHandler(UnmappedVirtHandler), vaddr, vaddr);
		vaddr += VTLB_PAGE_SIZE;
		size -= VTLB_PAGE_SIZE;
	}
}

// vtlb_Init -- Clears vtlb handlers and memory mappings.
void vtlb_Init()
{
	vtlbHandlerCount = 0;
	std::memset(vtlbdata.RWFT, 0, sizeof(vtlbdata.RWFT));

#define VTLB_BuildUnmappedHandler(baseName) \
	baseName##ReadSm<mem8_t>, baseName##ReadSm<mem16_t>, baseName##ReadSm<mem32_t>, \
		baseName##ReadSm<mem64_t>, baseName##ReadLg, \
		baseName##WriteSm<mem8_t>, baseName##WriteSm<mem16_t>, baseName##WriteSm<mem32_t>, \
		baseName##WriteSm<mem64_t>, baseName##WriteLg

	//Register default handlers
	//Unmapped Virt handlers _MUST_ be registered first.
	//On address translation the top bit cannot be preserved.This is not normaly a problem since
	//the physical address space can be 'compressed' to just 29 bits.However, to properly handle exceptions
	//there must be a way to get the full address back.Thats why i use these 2 functions and encode the hi bit directly into em :)

	UnmappedVirtHandler = vtlb_RegisterHandler(VTLB_BuildUnmappedHandler(vtlbUnmappedV));
	UnmappedPhyHandler = vtlb_RegisterHandler(VTLB_BuildUnmappedHandler(vtlbUnmappedP));
	DefaultPhyHandler = vtlb_RegisterHandler(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

	//done !

	//Setup the initial mappings
	vtlb_MapHandler(DefaultPhyHandler, 0, VTLB_PMAP_SZ);

	//Set the V space as unmapped
	vtlb_VMapUnmap(0, (VTLB_VMAP_ITEMS - 1) * VTLB_PAGE_SIZE);
	//yeah i know, its stupid .. but this code has to be here for now ;p
	vtlb_VMapUnmap((VTLB_VMAP_ITEMS - 1) * VTLB_PAGE_SIZE, VTLB_PAGE_SIZE);

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
	vtlb_RemoveFastmemMappings();
	for (int i = 0; i < 48; i++)
		UnmapTLB(tlb[i], i);
}

void vtlb_Shutdown()
{
	vtlb_RemoveFastmemMappings();
	s_fastmem_backpatch_info.clear();
	s_fastmem_faulting_pcs.clear();
}

void vtlb_ResetFastmem()
{
	DevCon.WriteLn("Resetting fastmem mappings...");

	vtlb_RemoveFastmemMappings();
	s_fastmem_backpatch_info.clear();
	s_fastmem_faulting_pcs.clear();

	if (!CHECK_FASTMEM || !CHECK_EEREC || !vtlbdata.vmap)
		return;

	// we need to go through and look at the vtlb pointers, to remap the host area
	for (size_t i = 0; i < VTLB_VMAP_ITEMS; i++)
	{
		const VTLBVirtual& vm = vtlbdata.vmap[i];
		const u32 vaddr = static_cast<u32>(i) << VTLB_PAGE_BITS;
		if (vm.isHandler(vaddr))
		{
			// Handlers should be unmapped.
			continue;
		}

		// Check if it's a physical mapping to our main memory area.
		u32 mainmem_offset, mainmem_size;
		PageProtectionMode prot;
		if (vtlb_GetMainMemoryOffsetFromPtr(vm.assumePtr(vaddr), &mainmem_offset, &mainmem_size, &prot))
			vtlb_CreateFastmemMapping(vaddr, mainmem_offset, prot);
	}
}

static constexpr size_t VMAP_SIZE = sizeof(VTLBVirtual) * VTLB_VMAP_ITEMS;

// Reserves the vtlb core allocation used by various emulation components!
// [TODO] basemem - request allocating memory at the specified virtual location, which can allow
//    for easier debugging and/or 3rd party cheat programs.  If 0, the operating system
//    default is used.
bool vtlb_Core_Alloc()
{
	// Can't return regions to the bump allocator
	static VTLBVirtual* vmap = nullptr;
	if (!vmap)
	{
		vmap = (VTLBVirtual*)GetVmMemory().BumpAllocator().Alloc(VMAP_SIZE);
		if (!vmap)
		{
			Host::ReportErrorAsync("Error", "Failed to allocate vtlb vmap");
			return false;
		}
	}

	if (!vtlbdata.vmap)
	{
		HostSys::MemProtect(vmap, VMAP_SIZE, PageProtectionMode().Read().Write());
		vtlbdata.vmap = vmap;
	}

	if (!vtlbdata.fastmem_base)
	{
		pxAssert(!s_fastmem_area);
		s_fastmem_area = SharedMemoryMappingArea::Create(FASTMEM_AREA_SIZE);
		if (!s_fastmem_area)
		{
			Host::ReportErrorAsync("Error", "Failed to allocate fastmem area");
			return false;
		}

		s_fastmem_virtual_mapping.resize(FASTMEM_PAGE_COUNT, NO_FASTMEM_MAPPING);
		vtlbdata.fastmem_base = (uptr)s_fastmem_area->BasePointer();
		Console.WriteLn(Color_StrongGreen, "Fastmem area: %p - %p",
			vtlbdata.fastmem_base, vtlbdata.fastmem_base + (FASTMEM_AREA_SIZE - 1));
	}

	if (!HostSys::InstallPageFaultHandler(&vtlb_private::PageFaultHandler))
	{
		Host::ReportErrorAsync("Error", "Failed to install page fault handler.");
		return false;
	}

	return true;
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

	HostSys::MemProtect(ppmap, PPMAP_SIZE, PageProtectionMode().Read().Write());
	vtlbdata.ppmap = ppmap;

	// By default a 1:1 virtual to physical mapping
	for (u32 i = 0; i < VTLB_VMAP_ITEMS; i++)
		vtlbdata.ppmap[i] = i << VTLB_PAGE_BITS;
}

void vtlb_Core_Free()
{
	HostSys::RemovePageFaultHandler(&vtlb_private::PageFaultHandler);

	if (vtlbdata.vmap)
	{
		HostSys::MemProtect(vtlbdata.vmap, VMAP_SIZE, PageProtectionMode());
		vtlbdata.vmap = nullptr;
	}
	if (vtlbdata.ppmap)
	{
		HostSys::MemProtect(vtlbdata.ppmap, PPMAP_SIZE, PageProtectionMode());
		vtlbdata.ppmap = nullptr;
	}

	vtlb_RemoveFastmemMappings();
	vtlb_ClearLoadStoreInfo();

	vtlbdata.fastmem_base = 0;
	decltype(s_fastmem_physical_mapping)().swap(s_fastmem_physical_mapping);
	decltype(s_fastmem_virtual_mapping)().swap(s_fastmem_virtual_mapping);
	s_fastmem_area.reset();
}

// --------------------------------------------------------------------------------------
//  VtlbMemoryReserve  (implementations)
// --------------------------------------------------------------------------------------
VtlbMemoryReserve::VtlbMemoryReserve(std::string name)
	: VirtualMemoryReserve(std::move(name))
{
}

void VtlbMemoryReserve::Assign(VirtualMemoryManagerPtr allocator, size_t offset, size_t size)
{
	// Anything passed to the memory allocator must be page aligned.
	size = Common::PageAlign(size);

	// Since the memory has already been allocated as part of the main memory map, this should never fail.
	u8* base = allocator->Alloc(offset, size);
	if (!base)
	{
		Console.WriteLn("(VtlbMemoryReserve) Failed to allocate %zu bytes for %s at offset %zu", size, m_name.c_str(), offset);
		pxFailRel("VtlbMemoryReserve allocation failed.");
	}

	VirtualMemoryReserve::Assign(std::move(allocator), base, size);
}

void VtlbMemoryReserve::Reset()
{
	std::memset(GetPtr(), 0, GetSize());
}


// ===========================================================================================
//  Memory Protection and Block Checking, vtlb Style!
// ===========================================================================================
// For the first time code is recompiled (executed), the PS2 ram page for that code is
// protected using Virtual Memory (mprotect).  If the game modifies its own code then this
// protection causes an *exception* to be raised (signal in Linux), which is handled by
// unprotecting the page and switching the recompiled block to "manual" protection.
//
// Manual protection uses a simple brute-force memcmp of the recompiled code to the code
// currently in RAM for *each time* the block is executed.  Fool-proof, but slow, which
// is why we default to using the exception-based protection scheme described above.
//
// Why manual blocks?  Because many games contain code and data in the same 4k page, so
// we *cannot* automatically recompile and reprotect pages, lest we end up recompiling and
// reprotecting them constantly (Which would be very slow).  As a counter, the R5900 side
// of the block checking code does try to periodically re-protect blocks [going from manual
// back to protected], so that blocks which underwent a single invalidation don't need to
// incur a permanent performance penalty.
//
// Page Granularity:
// Fortunately for us MIPS and x86 use the same page granularity for TLB and memory
// protection, so we can use a 1:1 correspondence when protecting pages.  Page granularity
// is 4096 (4k), which is why you'll see a lot of 0xfff's, >><< 12's, and 0x1000's in the
// code below.
//

struct vtlb_PageProtectionInfo
{
	// Ram De-mapping -- used to convert fully translated/mapped offsets (which reside with
	// in the eeMem->Main block) back into their originating ps2 physical ram address.
	// Values are assigned when pages are marked for protection.  since pages are automatically
	// cleared and reset when TLB-remapped, stale values in this table (due to on-the-fly TLB
	// changes) will be re-assigned the next time the page is accessed.
	u32 ReverseRamMap;

	vtlb_ProtectionMode Mode;
};

alignas(16) static vtlb_PageProtectionInfo m_PageProtectInfo[Ps2MemSize::MainRam >> __pageshift];


// returns:
//  ProtMode_NotRequired - unchecked block (resides in ROM, thus is integrity is constant)
//  Or the current mode
//
vtlb_ProtectionMode mmap_GetRamPageInfo(u32 paddr)
{
	pxAssert(eeMem);

	paddr &= ~0xfff;

	uptr ptr = (uptr)PSM(paddr);
	uptr rampage = ptr - (uptr)eeMem->Main;

	if (!ptr || rampage >= Ps2MemSize::MainRam)
		return ProtMode_NotRequired; //not in ram, no tracking done ...

	rampage >>= __pageshift;

	return m_PageProtectInfo[rampage].Mode;
}

// paddr - physically mapped PS2 address
void mmap_MarkCountedRamPage(u32 paddr)
{
	pxAssert(eeMem);

	paddr &= ~__pagemask;

	uptr ptr = (uptr)PSM(paddr);
	int rampage = (ptr - (uptr)eeMem->Main) >> __pageshift;

	// Important: Update the ReverseRamMap here because TLB changes could alter the paddr
	// mapping into eeMem->Main.

	m_PageProtectInfo[rampage].ReverseRamMap = paddr;

	if (m_PageProtectInfo[rampage].Mode == ProtMode_Write)
		return; // skip town if we're already protected.

	eeRecPerfLog.Write((m_PageProtectInfo[rampage].Mode == ProtMode_Manual) ?
						   "Re-protecting page @ 0x%05x" :
						   "Protected page @ 0x%05x",
		paddr >> __pageshift);

	m_PageProtectInfo[rampage].Mode = ProtMode_Write;
	HostSys::MemProtect(&eeMem->Main[rampage << __pageshift], __pagesize, PageAccess_ReadOnly());
	vtlb_UpdateFastmemProtection(rampage << __pageshift, __pagesize, PageAccess_ReadOnly());
}

// offset - offset of address relative to psM.
// All recompiled blocks belonging to the page are cleared, and any new blocks recompiled
// from code residing in this page will use manual protection.
static __fi void mmap_ClearCpuBlock(uint offset)
{
	pxAssert(eeMem);

	int rampage = offset >> __pageshift;

	// Assertion: This function should never be run on a block that's already under
	// manual protection.  Indicates a logic error in the recompiler or protection code.
	pxAssertMsg(m_PageProtectInfo[rampage].Mode != ProtMode_Manual,
		"Attempted to clear a block that is already under manual protection.");

	HostSys::MemProtect(&eeMem->Main[rampage << __pageshift], __pagesize, PageAccess_ReadWrite());
	vtlb_UpdateFastmemProtection(rampage << __pageshift, __pagesize, PageAccess_ReadWrite());
	m_PageProtectInfo[rampage].Mode = ProtMode_Manual;
	Cpu->Clear(m_PageProtectInfo[rampage].ReverseRamMap, __pagesize);
}

bool vtlb_private::PageFaultHandler(const PageFaultInfo& info)
{
	pxAssert(eeMem);

	u32 vaddr;
	if (CHECK_FASTMEM && vtlb_GetGuestAddress(info.addr, &vaddr))
	{
		// this was inside the fastmem area. check if it's a code page
		// fprintf(stderr, "Fault on fastmem %p vaddr %08X\n", info.addr, vaddr);

		uptr ptr = (uptr)PSM(vaddr);
		uptr offset = (ptr - (uptr)eeMem->Main);
		if (ptr && m_PageProtectInfo[offset >> __pageshift].Mode == ProtMode_Write)
		{
			// fprintf(stderr, "Not backpatching code write at %08X\n", vaddr);
			mmap_ClearCpuBlock(offset);
			return true;
		}
		else
		{
			// fprintf(stderr, "Trying backpatching vaddr %08X\n", vaddr);
			return vtlb_BackpatchLoadStore(info.pc, info.addr);
		}
	}
	else
	{
		// get bad virtual address
		uptr offset = info.addr - (uptr)eeMem->Main;
		if (offset >= Ps2MemSize::MainRam)
			return false;

		mmap_ClearCpuBlock(offset);
		return true;
	}
}

// Clears all block tracking statuses, manual protection flags, and write protection.
// This does not clear any recompiler blocks.  It is assumed (and necessary) for the caller
// to ensure the EErec is also reset in conjunction with calling this function.
//  (this function is called by default from the eerecReset).
void mmap_ResetBlockTracking()
{
	//DbgCon.WriteLn( "vtlb/mmap: Block Tracking reset..." );
	std::memset(m_PageProtectInfo, 0, sizeof(m_PageProtectInfo));
	if (eeMem)
		HostSys::MemProtect(eeMem->Main, Ps2MemSize::MainRam, PageAccess_ReadWrite());
	vtlb_UpdateFastmemProtection(0, Ps2MemSize::MainRam, PageAccess_ReadWrite());
}
