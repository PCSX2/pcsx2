/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2008  Pcsx2 Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "PrecompiledHeader.h"

#include <cmath>

#include "Common.h"
#include "R5900.h"
#include "VUmicro.h"

#include "iVUzerorec.h"

#ifdef PCSX2_VIRTUAL_MEM
extern PSMEMORYBLOCK s_psVuMem;
extern PSMEMORYMAP *memLUT;
#endif

// The following CpuVU objects are value types instead of handles or pointers because they are
// modified on the fly to implement VU1 Skip.

VUmicroCpu CpuVU0;		// contains a working copy of the VU0 cpu functions/API
VUmicroCpu CpuVU1;		// contains a working copy of the VU1 cpu functions/API

static void DummyExecuteVU1Block(void)
{
	VU0.VI[ REG_VPU_STAT ].UL &= ~0x100;
	VU1.vifRegs->stat &= ~4; // also reset the bit (grandia 3 works)
}

void vu1MicroEnableSkip()
{
	CpuVU1.ExecuteBlock = DummyExecuteVU1Block;
}

void vu1MicroDisableSkip()
{
	CpuVU1.ExecuteBlock = CHECK_VU1REC ? recVU1.ExecuteBlock : intVU1.ExecuteBlock;
}

bool vu1MicroIsSkipping()
{
	return CpuVU1.ExecuteBlock == DummyExecuteVU1Block;
}

void vuMicroCpuReset()
{
	CpuVU0 = CHECK_VU0REC ? recVU0 : intVU0;
	CpuVU1 = CHECK_VU1REC ? recVU1 : intVU1;
	CpuVU0.Reset();
	CpuVU1.Reset();

	// SuperVUreset will do nothing is none of the recs are initialized.
	// But it's needed if one or the other is initialized.
	SuperVUReset(-1);
}

void vuMicroMemAlloc()
{
#ifdef PCSX2_VIRTUAL_MEM
	jASSUME( memLUT != NULL );		// memAlloc() must always be called first, thanks.

	// unmap all vu0 pages
	SysMapUserPhysicalPages(PS2MEM_VU0MICRO, 16, NULL, 0);

	// mirror 4 times
	VU0.Micro = PS2MEM_VU0MICRO;

	// since vuregisters are mapped in vumem0, go to diff addr, but mapping to same physical addr
    //VirtualFree((void*)0x11000000, 0x10000, MEM_RELEASE); // free just in case
	bool vu1_reassign = false;
	if( VU0.Mem == NULL )
	{
		VU0.Mem = (u8*)VirtualAlloc((void*)0x11000000, 0x10000, MEM_RESERVE|MEM_PHYSICAL, PAGE_READWRITE);
		vu1_reassign = true;
	}

	if( VU0.Mem != (void*)0x11000000 )
	{
		VU0.Mem = NULL;
		throw Exception::OutOfMemory(
			fmt_string( "Failed to alloc vu0mem 0x11000000 %d", GetLastError() )
		);
	}

	memLUT[0x11004].aPFNs = &s_psVuMem.aPFNs[1]; memLUT[0x11004].aVFNs = &s_psVuMem.aVFNs[1];
	memLUT[0x11005].aPFNs = &s_psVuMem.aPFNs[1]; memLUT[0x11005].aVFNs = &s_psVuMem.aVFNs[1];
	memLUT[0x11006].aPFNs = &s_psVuMem.aPFNs[1]; memLUT[0x11006].aVFNs = &s_psVuMem.aVFNs[1];
	memLUT[0x11007].aPFNs = &s_psVuMem.aPFNs[1]; memLUT[0x11007].aVFNs = &s_psVuMem.aVFNs[1];

	// map only registers
	SysMapUserPhysicalPages(VU0.Mem+0x4000, 1, s_psVuMem.aPFNs, 2);

	if( vu1_reassign )
	{
		// Initialize VU1 memory using VU0's allocations:
		// Important!  VU1 is actually a macro to g_pVU1 (yes, awkward!)  so we need to assign it first.
		g_pVU1 = (VURegs*)(VU0.Mem + 0x4000);

		VU1.Mem = PS2MEM_VU1MEM;
		VU1.Micro = PS2MEM_VU1MICRO;
	}

#else

	// -- VTLB Memory Allocation --

	if( VU0.Mem == NULL )
	{
		VU0.Mem = (u8*)_aligned_malloc(0x4000+sizeof(VURegs), 16); // for VU1

		if( VU0.Mem == NULL )
			throw Exception::OutOfMemory( "vu0init > Failed to allocate VUmicro memory." );

		// Initialize VU1 memory using VU0's allocations:
		// Important!  VU1 is actually a macro to g_pVU1 (yes, awkward!)  so we need to assign it first.

		g_pVU1 = (VURegs*)(VU0.Mem + 0x4000);

		// HACKFIX!  (Air)
		// The VIFdma1 has a nasty habit of transferring data into the 4k page of memory above
		// the VU1. (oops!!)  This happens to be recLUT most of the time, which causes rapid death
		// of our emulator.  So we allocate some extra space here to keep VIF1 a little happier.

		// fixme - When the VIF is fixed, change the *3 below back to an *2.

		VU1.Mem   = (u8*)_aligned_malloc(0x4000*3, 16);
		if (VU1.Mem == NULL )
			throw Exception::OutOfMemory( "vu1Init > Failed to allocate memory for the VU1micro." );
		VU1.Micro = VU1.Mem + 0x4000; //(u8*)_aligned_malloc(16*1024, 16);
	}

	if( VU0.Micro == NULL )
		VU0.Micro = (u8*)_aligned_malloc(4*1024, 16);

	if( VU0.Micro == NULL )
		throw Exception::OutOfMemory( "vu0init > Failed to allocate VUmicro memory." );

#endif
}

void vuMicroMemShutdown()
{
#ifdef PCSX2_VIRTUAL_MEM

	if( VU0.Mem != NULL )
	{
		if( !SysMapUserPhysicalPages(VU0.Mem, 16, NULL, 0) )
			Console::Error("Error releasing vu0 memory %d", params GetLastError());

		if( VirtualFree(VU0.Mem, 0, MEM_RELEASE) == 0 )
			Console::Error("Error freeing vu0 memory %d", params GetLastError());
	}
	VU0.Mem = NULL;
	VU0.Micro = NULL;

#else

	// -- VTLB Memory Allocation --

	safe_aligned_free( VU1.Mem );
	safe_aligned_free( VU0.Mem );
	safe_aligned_free( VU0.Micro );

#endif
}

void vuMicroMemReset()
{
	jASSUME( VU0.Mem != NULL );
	jASSUME( VU1.Mem != NULL );

	memMapVUmicro();

	// === VU0 Initialization ===
	memzero_obj(VU0.ACC);
	memzero_obj(VU0.VF);
	memzero_obj(VU0.VI);
    VU0.VF[0].f.x = 0.0f;
	VU0.VF[0].f.y = 0.0f;
	VU0.VF[0].f.z = 0.0f;
	VU0.VF[0].f.w = 1.0f;
	VU0.VI[0].UL = 0;
	memzero_ptr<4*1024>(VU0.Mem);
	memzero_ptr<4*1024>(VU0.Micro);

	/* this is kinda tricky, maxmem is set to 0x4400 here,
	   tho it's not 100% accurate, since the mem goes from
	   0x0000 - 0x1000 (Mem) and 0x4000 - 0x4400 (VU1 Regs),
	   i guess it shouldn't be a problem,
	   at least hope so :) (linuz)
	*/
	VU0.maxmem = 0x4400-4;
	VU0.maxmicro = 4*1024-4;
	VU0.vuExec = vu0Exec;
	VU0.vifRegs = vif0Regs;

	// === VU1 Initialization ===
	memzero_obj(VU1.ACC);
	memzero_obj(VU1.VF);
	memzero_obj(VU1.VI);
	VU1.VF[0].f.x = 0.0f;
	VU1.VF[0].f.y = 0.0f;
	VU1.VF[0].f.z = 0.0f;
	VU1.VF[0].f.w = 1.0f;
	VU1.VI[0].UL = 0;
	memzero_ptr<16*1024>(VU1.Mem);
	memzero_ptr<16*1024>(VU1.Micro);

	VU1.maxmem   = -1;//16*1024-4;
	VU1.maxmicro = 16*1024-4;
//	VU1.VF       = (VECTOR*)(VU0.Mem + 0x4000);
//	VU1.VI       = (REG_VI*)(VU0.Mem + 0x4200);
	VU1.vuExec   = vu1Exec;
	VU1.vifRegs  = vif1Regs;
}

void SaveState::vuMicroFreeze()
{
	jASSUME( VU0.Mem != NULL );
	jASSUME( VU1.Mem != NULL );

	Freeze(VU0.ACC);
	Freeze(VU0.code);
	FreezeMem(VU0.Mem,   4*1024);
	FreezeMem(VU0.Micro, 4*1024);

	Freeze(VU0.VF);
	if( GetVersion() >= 0x0012 )
		Freeze(VU0.VI);
	else
	{
		// Old versions stored the VIregs as 32 bit values...
		memzero_obj( VU0.VI );
		for(int i=0; i<32; i++ )
			Freeze( VU0.VI[i].UL );
	}

	Freeze(VU1.ACC);
	Freeze(VU1.code);
	FreezeMem(VU1.Mem,   16*1024);
	FreezeMem(VU1.Micro, 16*1024);

	Freeze(VU1.VF);
	if( GetVersion() >= 0x0012 )
		Freeze(VU1.VI);
	else
	{
		// Old versions stored the VIregs as 32 bit values...
		memzero_obj( VU1.VI );
		for(int i=0; i<32; i++ )
			Freeze( VU1.VI[i].UL );
	}

}
