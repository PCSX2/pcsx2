/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2005  Pcsx2 Team
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <malloc.h>

#include "Common.h"
#include "InterTables.h"
#include "ix86/ix86.h"
#include "iR5900.h"
#include "iMMI.h"
#include "iFPU.h"
#include "iCP0.h"
#include "VU.h"
#include "VUmicro.h"
#include "iVUmicro.h"
#include "iVU1micro.h"
#include "iVUops.h"
#include "VUops.h"

#include "iVUzerorec.h"

// TODO: there's a bug in spyro start menu where release vurec works but debug vurec breaks

#ifdef __MSCW32__
#pragma warning(disable:4244)
#pragma warning(disable:4761)
#endif

#define VU ((VURegs*)&VU1)

u32 vu1recpcold = -1;
u32 vu1reccountold = -1;

static _vuopinfo _opinfo[256];

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


#define VU1_VFx_ADDR(x) (u32)&VU1.VF[x].UL[0]
#define VU1_VFy_ADDR(x) (u32)&VU1.VF[x].UL[1]
#define VU1_VFz_ADDR(x) (u32)&VU1.VF[x].UL[2]
#define VU1_VFw_ADDR(x) (u32)&VU1.VF[x].UL[3]

#define VU1_REGR_ADDR (u32)&VU1.VI[REG_R]
#define VU1_REGI_ADDR (u32)&VU1.VI[REG_I]
#define VU1_REGQ_ADDR (u32)&VU1.VI[REG_Q]
#define VU1_REGMAC_ADDR (u32)&VU1.VI[REG_MAC_FLAG]

#define VU1_VI_ADDR(x)	(u32)&VU1.VI[x].UL

#define VU1_ACCx_ADDR (u32)&VU1.ACC.UL[0]
#define VU1_ACCy_ADDR (u32)&VU1.ACC.UL[1]
#define VU1_ACCz_ADDR (u32)&VU1.ACC.UL[2]
#define VU1_ACCw_ADDR (u32)&VU1.ACC.UL[3]

static void VU1RecompileBlock(void);

void recVU1Init()
{
	SuperVUInit(1);
}

void recVU1Shutdown()
{
	SuperVUDestroy(1);
}

void recResetVU1( void ) {

	if( CHECK_VU1REC ) {
		SuperVUReset(1);
	}

	vu1recpcold = 0;
	x86FpuState = FPU_STATE;
	iCWstate = 0;

	branch = 0;
}

static void iDumpBlock()
{
	FILE *f;
	char filename[ 256 ];
	u32 *mem;
	u32 i;

#ifdef __WIN32__
	CreateDirectory("dumps", NULL);
	sprintf( filename, "dumps\\vu%.4X.txt", VU1.VI[ REG_TPC ].UL );
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

#define VF_VAL(x) ((x==0x80000000)?0:(x))

u32 g_VUProgramId = 0;
void iDumpVU1Registers()
{
	int i;
//	static int icount = 0;
//	__Log("%x\n", icount);
	for(i = 1; i < 32; ++i) {
//		__Log("v%d: w%f(%x) z%f(%x) y%f(%x) x%f(%x), vi: ", i, VU1.VF[i].F[3], VU1.VF[i].UL[3], VU1.VF[i].F[2], VU1.VF[i].UL[2],
//			VU1.VF[i].F[1], VU1.VF[i].UL[1], VU1.VF[i].F[0], VU1.VF[i].UL[0]);
		//__Log("v%d: %f %f %f %f, vi: ", i, VU1.VF[i].F[3], VU1.VF[i].F[2], VU1.VF[i].F[1], VU1.VF[i].F[0]);
		__Log("v%d: %x %x %x %x, vi: ", i, VF_VAL(VU1.VF[i].UL[3]), VF_VAL(VU1.VF[i].UL[2]), VF_VAL(VU1.VF[i].UL[1]), VF_VAL(VU1.VF[i].UL[0]));
		if( i == REG_Q || i == REG_P ) __Log("%f\n", VU1.VI[i].F);
		//else __Log("%x\n", VU1.VI[i].UL);
		else __Log("%x\n", (i==REG_STATUS_FLAG||i==REG_MAC_FLAG||i==REG_CLIP_FLAG)?0:VU1.VI[i].UL);
	}
	__Log("vfACC: %f %f %f %f\n", VU1.ACC.F[3], VU1.ACC.F[2], VU1.ACC.F[1], VU1.ACC.F[0]);
}

#ifdef PCSX2_DEVBUILD
u32 vuprogcount = 0;
u32 vudump = 0;
#endif

void DummyExecuteVU1Block(void)
{
	VU0.VI[ REG_VPU_STAT ].UL &= ~0x100;
}

void recExecuteVU1Block(void)
{
#ifdef _DEBUG
	vuprogcount++;

	if( vudump & 8 ) {
		__Log("start vu1: %x %x\n", VU1.VI[ REG_TPC ].UL, vuprogcount);
	}
#endif

	if (CHECK_VU1REC)
	{
		if (VU1.VI[REG_TPC].UL >= VU1.maxmicro) { 
#ifdef CPU_LOG
			SysPrintf("VU1 memory overflow!!: %x\n", VU->VI[REG_TPC].UL);
#endif
			VU0.VI[REG_VPU_STAT].UL&= ~0x100;
			VU->cycle++;
			return;
		}

		assert( (VU1.VI[ REG_TPC ].UL&7) == 0 );

		//__Log("prog: %x %x %x\n", vuprogcount, *(int*)0x1883a740, *(int*)0x18fe5fe0);
		//for(i = 1; i < 32; ++i) __Log("vf%d: %x %x %x %x, vi: %x\n", i, VU0.VF[i].UL[3], VU0.VF[i].UL[2], VU0.VF[i].UL[1], VU0.VF[i].UL[0], VU0.VI[i].UL);

		//if( VU1.VI[ REG_TPC ].UL == 0x670 ) {
//		__Log("VU: %x %x\n", VU1.VI[ REG_TPC ].UL, vuprogcount);
//		iDumpVU1Registers();
//		vudump |= 8;

		// while loop needed since not always will return finished
		do {
			SuperVUExecuteProgram(VU1.VI[ REG_TPC ].UL, 1);
		}
		while( VU0.VI[ REG_VPU_STAT ].UL&0x100 );

//		__Log("eVU: %x\n", VU1.VI[ REG_TPC ].UL);
//		iDumpVU1Registers();
	}
	else {
#ifdef _DEBUG
		if( (vudump&8) ) {
			__Log("tVU: %x\n", VU1.VI[ REG_TPC ].UL);
			iDumpVU1Registers();
		}
#endif

		while(VU0.VI[ REG_VPU_STAT ].UL&0x100)
			intExecuteVU1Block();
	}
}

void recClearVU1( u32 Addr, u32 Size ) {
	assert( (Addr&7) == 0 );

	if( CHECK_VU1REC ) {
		SuperVUClear(Addr, Size*4, 1);
	}
}
