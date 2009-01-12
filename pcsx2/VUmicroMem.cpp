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

VUmicroCpu *CpuVU0;
VUmicroCpu *CpuVU1;

void vuMicroMemAlloc()
{
#ifdef PCSX2_VIRTUAL_MEM
	jASSUME( memLUT != NULL );		// memAlloc() must always be called first, thanks.

	// unmap all vu0 pages
	SysMapUserPhysicalPages(PS2MEM_VU0MICRO, 16, NULL, 0);

	// mirror 4 times
	VU0.Micro = PS2MEM_VU0MICRO;
	memLUT[0x11000].aPFNs = &s_psVuMem.aPFNs[0]; memLUT[0x11000].aVFNs = &s_psVuMem.aVFNs[0];
	memLUT[0x11001].aPFNs = &s_psVuMem.aPFNs[0]; memLUT[0x11001].aVFNs = &s_psVuMem.aVFNs[0];
	memLUT[0x11002].aPFNs = &s_psVuMem.aPFNs[0]; memLUT[0x11002].aVFNs = &s_psVuMem.aVFNs[0];
	memLUT[0x11003].aPFNs = &s_psVuMem.aPFNs[0]; memLUT[0x11003].aVFNs = &s_psVuMem.aVFNs[0];

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
			fmt_string( "Failed to alloc vu0mem 0x11000000 %d", params GetLastError() )
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

		VU1.Mem   = (u8*)_aligned_malloc(16*1024*2, 16);
		if (VU1.Mem == NULL )
			throw Exception::OutOfMemory( "vu1Init > Failed to allocate memory for the VU1micro." );
		VU1.Micro = VU1.Mem + 16*1024; //(u8*)_aligned_malloc(16*1024, 16);
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

	PCSX2_MEM_PROTECT_BEGIN();

	// === VU0 Initialization ===
	memset(&VU0.ACC, 0, sizeof(VECTOR));
	memset(VU0.VF, 0, sizeof(VECTOR)*32);
	memset(VU0.VI, 0, sizeof(REG_VI)*32);
    VU0.VF[0].f.x = 0.0f;
	VU0.VF[0].f.y = 0.0f;
	VU0.VF[0].f.z = 0.0f;
	VU0.VF[0].f.w = 1.0f;
	VU0.VI[0].UL = 0;
	memset(VU0.Mem, 0, 4*1024);
	memset(VU0.Micro, 0, 4*1024);

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
	memset(&VU1.ACC, 0, sizeof(VECTOR));
	memset(VU1.VF, 0, sizeof(VECTOR)*32);
	memset(VU1.VI, 0, sizeof(REG_VI)*32);
	VU1.VF[0].f.x = 0.0f;
	VU1.VF[0].f.y = 0.0f;
	VU1.VF[0].f.z = 0.0f;
	VU1.VF[0].f.w = 1.0f;
	VU1.VI[0].UL = 0;
	memset(VU1.Mem, 0, 16*1024);
	memset(VU1.Micro, 0, 16*1024);

	VU1.maxmem   = -1;//16*1024-4;
	VU1.maxmicro = 16*1024-4;
//	VU1.VF       = (VECTOR*)(VU0.Mem + 0x4000);
//	VU1.VI       = (REG_VI*)(VU0.Mem + 0x4200);
	VU1.vuExec   = vu1Exec;
	VU1.vifRegs  = vif1Regs;

	PCSX2_MEM_PROTECT_END();
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
		memset( VU0.VI, 0, sizeof( VU0.VI ) );
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
		memset( VU1.VI, 0, sizeof( VU1.VI ) );
		for(int i=0; i<32; i++ )
			Freeze( VU1.VI[i].UL );
	}

}
