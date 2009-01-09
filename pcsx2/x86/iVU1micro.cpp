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

#include <float.h>

#include "Common.h"
#include "InterTables.h"
#include "ix86/ix86.h"
#include "iR5900.h"
#include "iMMI.h"
#include "iFPU.h"
#include "iCP0.h"
#include "VU.h"
#include "VUmicro.h"
#include "iVUzerorec.h"
#include "iVUops.h"
#include "VUops.h"

#include "iVUzerorec.h"

#ifdef _WIN32
#pragma warning(disable:4244)
#pragma warning(disable:4761)
#endif

// fixme - having the VUs share the branch/pc values of the R5900 is bad on so many levels... >_< (air)

using ::Dynarec::R5900::pc;
using ::Dynarec::R5900::branch;

namespace Dynarec
{

#define VU ((VURegs*)&VU1)

//Lower/Upper instructions can use that..
#define _Ft_ ((VU1.code >> 16) & 0x1F)  // The rt part of the instruction register 
#define _Fs_ ((VU1.code >> 11) & 0x1F)  // The rd part of the instruction register 
#define _Fd_ ((VU1.code >>  6) & 0x1F)  // The sa part of the instruction register

#define _X ((VU1.code>>24) & 0x1)
#define _Y ((VU1.code>>23) & 0x1)
#define _Z ((VU1.code>>22) & 0x1)
#define _W ((VU1.code>>21) & 0x1)

#define _Fsf_ ((VU1.code >> 21) & 0x03)
#define _Ftf_ ((VU1.code >> 23) & 0x03)


#define VU1_VFx_ADDR(x) (uptr)&VU1.VF[x].UL[0]
#define VU1_VFy_ADDR(x) (uptr)&VU1.VF[x].UL[1]
#define VU1_VFz_ADDR(x) (uptr)&VU1.VF[x].UL[2]
#define VU1_VFw_ADDR(x) (uptr)&VU1.VF[x].UL[3]

#define VU1_REGR_ADDR (uptr)&VU1.VI[REG_R]
#define VU1_REGI_ADDR (uptr)&VU1.VI[REG_I]
#define VU1_REGQ_ADDR (uptr)&VU1.VI[REG_Q]
#define VU1_REGMAC_ADDR (uptr)&VU1.VI[REG_MAC_FLAG]

#define VU1_VI_ADDR(x)	(uptr)&VU1.VI[x].UL

#define VU1_ACCx_ADDR (uptr)&VU1.ACC.UL[0]
#define VU1_ACCy_ADDR (uptr)&VU1.ACC.UL[1]
#define VU1_ACCz_ADDR (uptr)&VU1.ACC.UL[2]
#define VU1_ACCw_ADDR (uptr)&VU1.ACC.UL[3]


void recVU1Init()
{
	SuperVUInit(1);
}

void recVU1Shutdown()
{
	SuperVUDestroy(1);
}

void recResetVU1( void ) {
	if( CHECK_VU1REC ) SuperVUReset(1);

	x86FpuState = FPU_STATE;
	iCWstate = 0;
	branch = 0;
}

static void iVU1DumpBlock()
{
	FILE *f;
	char filename[ g_MaxPath ];
	u32 *mem;
	u32 i;

#ifdef _WIN32
	CreateDirectory("dumps", NULL);
	sprintf_s( filename, g_MaxPath, "dumps\\vu%.4X.txt", VU1.VI[ REG_TPC ].UL );
#else
	mkdir("dumps", 0755);
	sprintf( filename, "dumps/vu%.4X.txt", VU1.VI[ REG_TPC ].UL );
#endif
	SysPrintf( "dump1 %x => %x (%s)\n", VU1.VI[ REG_TPC ].UL, pc, filename );

	f = fopen( filename, "wb" );
	for ( i = VU1.VI[REG_TPC].UL; i < pc; i += 8 ) {
		char* pstr;
		mem = (u32*)&VU1.Micro[i];

		pstr = disVU1MicroUF( mem[1], i+4 );
		fprintf(f, "%x: %-40s ",  i, pstr);
		
		pstr = disVU1MicroLF( mem[0], i );
		fprintf(f, "%s\n",  pstr);
	}
	fclose( f );
}

#ifdef _DEBUG
static u32 vuprogcount = 0;
extern u32 vudump;
#endif

void recExecuteVU1Block(void)
{
#ifdef _DEBUG
	vuprogcount++;
	if( vudump & 8 ) __Log("start vu1: %x %x\n", VU1.VI[ REG_TPC ].UL, vuprogcount);
#endif

	if((VU0.VI[REG_VPU_STAT].UL & 0x100) == 0){
		//SysPrintf("Execute block VU1, VU1 not busy\n");
		return;
	}
	
	if (CHECK_VU1REC)
	{
		if (VU1.VI[REG_TPC].UL >= VU1.maxmicro) { 
			SysPrintf("VU1 memory overflow!!: %x\n", VU1.VI[REG_TPC].UL);
			/*VU0.VI[REG_VPU_STAT].UL&= ~0x100;
			VU1.cycle++;
			return;*/
		}

		assert( (VU1.VI[ REG_TPC ].UL&7) == 0 );

		FreezeXMMRegs(1);
		do { // while loop needed since not always will return finished
			SuperVUExecuteProgram(VU1.VI[ REG_TPC ].UL & 0x3fff, 1);
		} while( VU0.VI[ REG_VPU_STAT ].UL&0x100 );
		FreezeXMMRegs(0);
	}
	else {
#ifdef _DEBUG
		if( (vudump&8) ) {
			__Log("tVU: %x\n", VU1.VI[ REG_TPC ].UL);
			iDumpVU1Registers();
		}
#endif
		while (VU0.VI[ REG_VPU_STAT ].UL&0x100) {
			::R5900::Interpreter::intExecuteVU1Block();
		}
	}
}

void recClearVU1( u32 Addr, u32 Size ) {
	assert( (Addr&7) == 0 );
	if( CHECK_VU1REC ) SuperVUClear(Addr, Size*4, 1);
}
}
