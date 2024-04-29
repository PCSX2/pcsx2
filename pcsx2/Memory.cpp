// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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

#include "DEV9/DEV9.h"
#include "IopHw.h"
#include "GS/Renderers/Common/GSFunctionMap.h"
#include "GS.h"
#include "Host.h"
#include "MTVU.h"
#include "SPU2/spu2.h"
#include "SaveState.h"
#include "VUmicro.h"

#include "ps2/HwInternal.h"
#include "ps2/BiosTools.h"

#include "common/AlignedMalloc.h"
#include "common/Error.h"

#ifdef ENABLECACHE
#include "Cache.h"
#endif

namespace Ps2MemSize
{
	u32 ExposedRam = MainRam;
} // namespace Ps2MemSize

namespace SysMemory
{
	static u8* TryAllocateVirtualMemory(const char* name, void* file_handle, uptr base, size_t size);
	static u8* AllocateVirtualMemory(const char* name, void* file_handle, size_t size, size_t offset_from_base);

	static bool AllocateMemoryMap();
	static void DumpMemoryMap();
	static void ReleaseMemoryMap();

	static u8* s_data_memory;
	static void* s_data_memory_file_handle;
	static u8* s_code_memory;
} // namespace SysMemory

static void memAllocate();
static void memReset();
static void memRelease();

int MemMode = 0;		// 0 is Kernel Mode, 1 is Supervisor Mode, 2 is User Mode

static u16 s_ba[0xff];
static u16 s_dve_regs[0xff];
static bool s_ba_command_executing = false;
static bool s_ba_error_detected = false;
static u16 s_ba_current_reg = 0;
static bool s_extra_memory = false;

namespace HostMemoryMap
{
	// For debuggers
	extern "C" {
#ifdef _WIN32
	_declspec(dllexport) uptr EEmem, IOPmem, VUmem;
#else
	__attribute__((visibility("default"), used)) uptr EEmem, IOPmem, VUmem;
#endif
	}
} // namespace HostMemoryMap

u8* SysMemory::TryAllocateVirtualMemory(const char* name, void* file_handle, uptr base, size_t size)
{
	u8* baseptr;

	if (file_handle)
		baseptr = static_cast<u8*>(HostSys::MapSharedMemory(file_handle, 0, (void*)base, size, PageAccess_ReadWrite()));
	else
		baseptr = static_cast<u8*>(HostSys::Mmap((void*)base, size, PageAccess_Any()));

	if (!baseptr)
		return nullptr;

	if ((uptr)baseptr != base)
	{
		if (file_handle)
		{
			if (baseptr)
				HostSys::UnmapSharedMemory(baseptr, size);
		}
		else
		{
			if (baseptr)
				HostSys::Munmap(baseptr, size);
		}

		return nullptr;
	}

	DevCon.WriteLn(Color_Gray, "%-32s @ 0x%016" PRIXPTR " -> 0x%016" PRIXPTR " %s", name,
		baseptr, (uptr)baseptr + size, fmt::format("[{}mb]", size / _1mb).c_str());

	return baseptr;
}

u8* SysMemory::AllocateVirtualMemory(const char* name, void* file_handle, size_t size, size_t offset_from_base)
{
	pxAssertRel(Common::IsAlignedPow2(size, __pagesize), "Virtual memory size is page aligned");

	// Everything looks nicer when the start of all the sections is a nice round looking number.
	// Also reduces the variation in the address due to small changes in code.
	// Breaks ASLR but so does anything else that tries to make addresses constant for our debugging pleasure
	uptr codeBase = (uptr)(void*)AllocateVirtualMemory / (1 << 28) * (1 << 28);

	// The allocation is ~640mb in size, slighly under 3*2^28.
	// We'll hope that the code generated for the PCSX2 executable stays under 512mb (which is likely)
	// On x86-64, code can reach 8*2^28 from its address [-6*2^28, 4*2^28] is the region that allows for code in the 640mb allocation to reach 512mb of code that either starts at codeBase or 256mb before it.
	// We start high and count down because on macOS code starts at the beginning of useable address space, so starting as far ahead as possible reduces address variations due to code size.  Not sure about other platforms.  Obviously this only actually affects what shows up in a debugger and won't affect performance or correctness of anything.
	for (int offset = 4; offset >= -6; offset--)
	{
		uptr base = codeBase + (offset << 28) + offset_from_base;
		if ((sptr)base < 0 || (sptr)(base + size - 1) < 0)
		{
			// VTLB will throw a fit if we try to put EE main memory here
			continue;
		}

		if (u8* ret = TryAllocateVirtualMemory(name, file_handle, base, size))
			return ret;

		DevCon.Warning("%s: host memory @ 0x%016" PRIXPTR " -> 0x%016" PRIXPTR " is unavailable; attempting to map elsewhere...", name,
			base, base + size);
	}

	return nullptr;
}

bool SysMemory::AllocateMemoryMap()
{
	s_data_memory_file_handle = HostSys::CreateSharedMemory(HostSys::GetFileMappingName("pcsx2").c_str(), HostMemoryMap::MainSize);
	if (!s_data_memory_file_handle)
	{
		Host::ReportErrorAsync("Error", "Failed to create shared memory file.");
		ReleaseMemoryMap();
		return false;
	}

	if ((s_data_memory = AllocateVirtualMemory("Data Memory", s_data_memory_file_handle, HostMemoryMap::MainSize, 0)) == nullptr)
	{
		Host::ReportErrorAsync("Error", "Failed to map data memory at an acceptable location.");
		ReleaseMemoryMap();
		return false;
	}

	if ((s_code_memory = AllocateVirtualMemory("Code Memory", nullptr, HostMemoryMap::CodeSize, HostMemoryMap::MainSize)) == nullptr)
	{
		Host::ReportErrorAsync("Error", "Failed to allocate code memory at an acceptable location.");
		ReleaseMemoryMap();
		return false;
	}

	HostMemoryMap::EEmem = (uptr)(s_data_memory + HostMemoryMap::EEmemOffset);
	HostMemoryMap::IOPmem = (uptr)(s_data_memory + HostMemoryMap::IOPmemOffset);
	HostMemoryMap::VUmem = (uptr)(s_data_memory + HostMemoryMap::VUmemSize);

	DumpMemoryMap();
	return true;
}

void SysMemory::DumpMemoryMap()
{
#define DUMP_REGION(name, base, offset, size) \
	DevCon.WriteLn(Color_Gray, "  %-32s @ 0x%016" PRIXPTR " -> 0x%016" PRIXPTR " %s", name, \
		(uptr)(base + offset), (uptr)(base + offset + size), fmt::format("[{}mb]", size / _1mb).c_str());

	DUMP_REGION("EE Main Memory", s_data_memory, HostMemoryMap::EEmemOffset, HostMemoryMap::EEmemSize);
	DUMP_REGION("IOP Main Memory", s_data_memory, HostMemoryMap::IOPmemOffset, HostMemoryMap::IOPmemSize);
	DUMP_REGION("VU0/1 On-Chip Memory", s_data_memory, HostMemoryMap::VUmemOffset, HostMemoryMap::VUmemSize);
	DUMP_REGION("VTLB Virtual Map", s_data_memory, HostMemoryMap::VTLBAddressMapOffset, HostMemoryMap::VTLBVirtualMapSize);
	DUMP_REGION("VTLB Address Map", s_data_memory, HostMemoryMap::VTLBAddressMapSize, HostMemoryMap::VTLBAddressMapSize);

	DUMP_REGION("R5900 Recompiler Cache", s_code_memory, HostMemoryMap::EErecOffset, HostMemoryMap::EErecSize);
	DUMP_REGION("R3000A Recompiler Cache", s_code_memory, HostMemoryMap::IOPrecOffset, HostMemoryMap::IOPrecSize);
	DUMP_REGION("Micro VU0 Recompiler Cache", s_code_memory, HostMemoryMap::mVU0recOffset, HostMemoryMap::mVU0recSize);
	DUMP_REGION("Micro VU0 Recompiler Cache", s_code_memory, HostMemoryMap::mVU1recOffset, HostMemoryMap::mVU1recSize);
	DUMP_REGION("VIF0 Unpack Recompiler Cache", s_code_memory, HostMemoryMap::VIF0recOffset, HostMemoryMap::VIF0recSize);
	DUMP_REGION("VIF1 Unpack Recompiler Cache", s_code_memory, HostMemoryMap::VIF1recOffset, HostMemoryMap::VIF1recSize);
	DUMP_REGION("VIF Unpack Recompiler Cache", s_code_memory, HostMemoryMap::VIFUnpackRecOffset, HostMemoryMap::VIFUnpackRecSize);
	DUMP_REGION("GS Software Renderer", s_code_memory, HostMemoryMap::SWrecOffset, HostMemoryMap::SWrecSize);


#undef DUMP_REGION
}

void SysMemory::ReleaseMemoryMap()
{
	if (s_code_memory)
	{
		HostSys::Munmap(s_code_memory, HostMemoryMap::CodeSize);
		s_code_memory = nullptr;
	}

	if (s_data_memory)
	{
		HostSys::UnmapSharedMemory(s_data_memory, HostMemoryMap::MainSize);
		s_data_memory = nullptr;
	}

	if (s_data_memory_file_handle)
	{
		HostSys::DestroySharedMemory(s_data_memory_file_handle);
		s_data_memory_file_handle = nullptr;
	}
}

bool SysMemory::Allocate()
{
	DevCon.WriteLn(Color_StrongBlue, "Allocating host memory for virtual systems...");

	if (!AllocateMemoryMap())
		return false;

	memAllocate();
	iopMemAlloc();
	vuMemAllocate();

	if (!vtlb_Core_Alloc())
		return false;

	return true;
}

void SysMemory::Reset()
{
	DevCon.WriteLn(Color_StrongBlue, "Resetting host memory for virtual systems...");

	memReset();
	iopMemReset();
	vuMemReset();

	// Note: newVif is reset as part of other VIF structures.
	// Software is reset on the GS thread.
}

void SysMemory::Release()
{
	Console.WriteLn(Color_Blue, "Releasing host memory for virtual systems...");

	vtlb_Core_Free(); // Just to be sure... (calling order could result in it getting missed during Decommit).

	vuMemRelease();
	iopMemRelease();
	memRelease();

	ReleaseMemoryMap();
}

u8* SysMemory::GetDataPtr(size_t offset)
{
	pxAssert(offset <= HostMemoryMap::MainSize);
	return s_data_memory + offset;
}

u8* SysMemory::GetCodePtr(size_t offset)
{
	pxAssert(offset <= HostMemoryMap::CodeSize);
	return s_code_memory + offset;
}

void* SysMemory::GetDataFileHandle()
{
	return s_data_memory_file_handle;
}

bool memGetExtraMemMode()
{
	return s_extra_memory;
}

void memSetExtraMemMode(bool mode)
{
	s_extra_memory = mode;

	// update the amount of RAM exposed to the VM
	Ps2MemSize::ExposedRam = mode ? Ps2MemSize::TotalRam : Ps2MemSize::MainRam;
}

void memSetKernelMode() {
	//Do something here
	MemMode = 0;
}

void memSetSupervisorMode() {
}

void memSetUserMode() {

}

// These regs are related to DEV9 and DVE stuff, we don't have to go crazy with this, but this sucks less than the original code
void ba0W16(u32 mem, u16 value)
{
	//MEM_LOG("ba000000 Memory write16 address %x value %x", mem, value);
	u32 masked_mem = (mem & 0xFF);

	if (masked_mem == 0x6) // Status Reg
	{
		s_ba[0x6] &= ~3;
	}
	else
		s_ba[masked_mem] = value;

	if (masked_mem == 0x00) // Command Execute Reg
	{
		if (s_ba[0x2] == 0x4F || s_ba[0x2] == 0x41)
		{
			DevCon.Warning("Error running DVE command, Control Reg value set to %x", value);
			s_ba_error_detected = true;
		}
		else if (s_ba[masked_mem] & 0x80) // Start executing
		{
			if (s_ba[0x2] == 0x43) // Write Mode
			{
				int size = (s_ba[masked_mem] & 0xF);
				s_ba_current_reg = s_ba[0x10];
				size--;

				// 0x10->0x22 seems to be some sort of FIFO, with 0x10 generally being the register to read/write
				for (int i = 0; i < size; i++)
				{
					s_dve_regs[s_ba_current_reg] = s_ba[0x12 + i];
				}

				s_ba_command_executing = true;
				s_ba_error_detected = false;
			}
			else if(s_ba[0x2] == 0x42) // Read Mode
			{
				int size = (s_ba[masked_mem] & 0xF);

				for (int i = 0; i < size; i++)
					s_ba[0x10 + i] = s_dve_regs[s_ba_current_reg]; // Probably not right but we don't access the real regs, will be enough for now.
				s_ba_command_executing = true;
				s_ba_error_detected = false;
			}
		}
	}
	else if (masked_mem == 0xA) // Power/Standby (?) Reg
	{
		if (value == 0)
			s_ba_error_detected = true;
		else
			s_ba_error_detected = false;

		DevCon.Warning("DVE powered %s", value == 0 ? "off" : "on");
	}
}

u16 ba0R16(u32 mem)
{
	//MEM_LOG("ba000000 Memory read16 address %x", mem);

	if (mem == 0x1a000006)
	{
		// 0xba00000A bit 0 is kind of an "on" switch. bit 0 of ba000006 seems to be the powered off/error bit.
		// bit 1 in ba000006 seems to be "ready".
		u16 return_val = (s_ba[0x6] & 2);

		if (s_ba_error_detected)
			return_val |= 1;

		if (s_ba[0x6] < 3 && s_ba_command_executing)
			s_ba[0x6]++;
		else
			s_ba_command_executing = false;

		return return_val;
	}

	return s_ba[mem & 0x1F];
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
	vtlb_MapBlock(eeMem->Main,	0x00000000,Ps2MemSize::ExposedRam);//mirrored on first 256 mb ?

	// High memory, uninstalled on the configuration we emulate
	vtlb_MapHandler(null_handler, Ps2MemSize::ExposedRam, 0x10000000 - Ps2MemSize::ExposedRam);

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
			MEM_LOG("ba000000 Memory read16 address %x", mem);
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
			MEM_LOG("ba000000 Memory write16 address %x value %x", mem, value);
			ba0W16(mem, value);
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
void memAllocate()
{
	eeMem = reinterpret_cast<EEVM_MemoryAllocMess*>(SysMemory::GetEEMem());
}

void memReset()
{
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

	std::memset(s_ba, 0, sizeof(s_ba));

	s_ba[0xA] = 1; // Power on
	s_ba_command_executing = false;
	s_ba_error_detected = false;
	s_ba_current_reg = 0;

	std::memset(s_dve_regs, 0, sizeof(s_dve_regs));

	s_dve_regs[0x7e] = 0x1C; // Status register. 0x1C seems to be the value it's expecting for everything being OK.

	// BIOS is included in eeMem, so it needs to be copied after zeroing.
	std::memset(eeMem, 0, sizeof(*eeMem));
	CopyBIOSToMemory();
}

void memRelease()
{
	eeMem = nullptr;
}

bool SaveStateBase::memFreeze(Error* error)
{
	Freeze(s_ba);
	Freeze(s_dve_regs);
	Freeze(s_ba_command_executing);
	Freeze(s_ba_error_detected);
	Freeze(s_ba_current_reg);

	bool extra_memory = s_extra_memory;
	Freeze(extra_memory);

	if (extra_memory != s_extra_memory)
	{
		Error::SetStringFmt(error, "Memory size mismatch, save state requires {}, but VM currently has {}.",
			extra_memory ? "128MB" : "32MB", s_extra_memory ? "128MB" : "32MB");
		return false;
	}

	return IsOkay();
}
