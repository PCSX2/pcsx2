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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "PsxCommon.h"
#include "Misc.h"

R3000Acpu *psxCpu;

// used for constant propagation
u32 g_psxConstRegs[32];
u32 g_psxHasConstReg, g_psxFlushedConstReg;

// Controls when branch tests are performed.
u32 g_psxNextBranchCycle = 0;

// This value is used when the IOP execution is broken to return contorl to the EE.
// (which happens when the IOP throws EE-bound interrupts).  It holds the value of
// psxCycleEE (which is set to zero to facilitate the code break), so that the unrun
// cycles can be accounted for later.
s32 psxBreak = 0;

// tracks the IOP's current sync status with the EE.  When it dips below zero,
// control is returned to the EE.
s32 psxCycleEE = -1;

int iopBranchAction = 0;


PCSX2_ALIGNED16(psxRegisters psxRegs);

int psxInit()
{
	psxCpu = CHECK_EEREC ? &psxRec : &psxInt;

	g_psxNextBranchCycle = 8;
	psxBreak = 0;
	psxCycleEE = -1;

#ifdef PCSX2_DEVBUILD
	Log=0;
#endif

	if (psxMemInit() == -1) return -1;

	return psxCpu->Init();
}

void psxReset() {

	psxCpu->Reset();

	psxMemReset();

	memset(&psxRegs, 0, sizeof(psxRegs));

	psxRegs.pc = 0xbfc00000; // Start in bootstrap

	psxRegs.CP0.n.Status = 0x10900000; // COP0 enabled | BEV = 1 | TS = 1
	psxRegs.CP0.n.PRid   = 0x0000001f; // PRevID = Revision ID, same as the IOP R3000A

	psxBreak = 0;
	psxCycleEE = -1;
	g_psxNextBranchCycle = psxRegs.cycle + 2;

	psxHwReset();
	psxBiosInit();
	psxExecuteBios();
}

void psxShutdown() {
	psxMemShutdown();
	psxBiosShutdown();
	psxSIOShutdown();
	psxCpu->Shutdown();
}

void psxException(u32 code, u32 bd) {
//	PSXCPU_LOG("psxException %x: %x, %x\n", code, psxHu32(0x1070), psxHu32(0x1074));
	//SysPrintf("!! psxException %x: %x, %x\n", code, psxHu32(0x1070), psxHu32(0x1074));
	// Set the Cause
	psxRegs.CP0.n.Cause &= ~0x7f;
	psxRegs.CP0.n.Cause |= code;

#ifdef PSXCPU_LOG
	if (bd) { PSXCPU_LOG("bd set\n"); }
#endif
	// Set the EPC & PC
	if (bd) {
		psxRegs.CP0.n.Cause|= 0x80000000;
		psxRegs.CP0.n.EPC = (psxRegs.pc - 4);
	} else
		psxRegs.CP0.n.EPC = (psxRegs.pc);

	if (psxRegs.CP0.n.Status & 0x400000)
		psxRegs.pc = 0xbfc00180;
	else
		psxRegs.pc = 0x80000080;

	// Set the Status
	psxRegs.CP0.n.Status = (psxRegs.CP0.n.Status &~0x3f) |
						  ((psxRegs.CP0.n.Status & 0xf) << 2);

	/*if ((((PSXMu32(psxRegs.CP0.n.EPC) >> 24) & 0xfe) == 0x4a)) {
		// "hokuto no ken" / "Crash Bandicot 2" ... fix
		PSXMu32(psxRegs.CP0.n.EPC)&= ~0x02000000;
	}*/

	if (Config.PsxOut && !CHECK_EEREC) {
		u32 call = psxRegs.GPR.n.t1 & 0xff;
		switch (psxRegs.pc & 0x1fffff) {
			case 0xa0:
#ifdef PSXBIOS_LOG
				if (call != 0x28 && call != 0xe) {
					PSXBIOS_LOG("Bios call a0: %s (%x) %x,%x,%x,%x\n", biosA0n[call], call, psxRegs.GPR.n.a0, psxRegs.GPR.n.a1, psxRegs.GPR.n.a2, psxRegs.GPR.n.a3); }
#endif
				if (biosA0[call])
			   		biosA0[call]();
				break;
			case 0xb0:
#ifdef PSXBIOS_LOG
				if (call != 0x17 && call != 0xb) {
					PSXBIOS_LOG("Bios call b0: %s (%x) %x,%x,%x,%x\n", biosB0n[call], call, psxRegs.GPR.n.a0, psxRegs.GPR.n.a1, psxRegs.GPR.n.a2, psxRegs.GPR.n.a3); }
#endif
				if (biosB0[call])
			   		biosB0[call]();
				break;
			case 0xc0:
				PSXBIOS_LOG("Bios call c0: %s (%x) %x,%x,%x,%x\n", biosC0n[call], call, psxRegs.GPR.n.a0, psxRegs.GPR.n.a1, psxRegs.GPR.n.a2, psxRegs.GPR.n.a3);
			
				if (biosC0[call])
			   		biosC0[call]();
				break;
		}
	}

	/*if (psxRegs.CP0.n.Cause == 0x400 && (!(psxHu32(0x1450) & 0x8))) {
		hwIntcIrq(1);
	}*/
}

__forceinline void psxSetNextBranch( u32 startCycle, s32 delta )
{
	// typecast the conditional to signed so that things don't blow up
	// if startCycle is greater than our next branch cycle.

	if( (int)(g_psxNextBranchCycle - startCycle) > delta )
		g_psxNextBranchCycle = startCycle + delta;
}

__forceinline void psxSetNextBranchDelta( s32 delta )
{
	psxSetNextBranch( psxRegs.cycle, delta );
}

__forceinline int psxTestCycle( u32 startCycle, s32 delta )
{
	// typecast the conditional to signed so that things don't explode
	// if the startCycle is ahead of our current cpu cycle.

	return (int)(psxRegs.cycle - startCycle) >= delta;
}

__forceinline void PSX_INT( int n, s32 ecycle )
{
	psxRegs.interrupt |= 1 << n;

	psxRegs.sCycle[n] = psxRegs.cycle;
	psxRegs.eCycle[n] = ecycle;

	psxSetNextBranchDelta( ecycle );

	if( psxCycleEE < 0 )
	{
		// The EE called this int, so inform it to branch as needed:

		s32 iopDelta = (g_psxNextBranchCycle-psxRegs.cycle)*8;
		cpuSetNextBranchDelta( iopDelta );
	}
}

static __forceinline void PSX_TESTINT( u32 n, void (*callback)(), int runIOPcode )
{
	if( !(psxRegs.interrupt & (1 << n)) ) return;

	if( psxTestCycle( psxRegs.sCycle[n], psxRegs.eCycle[n] ) )
	{
		callback();
		//if( runIOPcode ) iopBranchAction = 1;
	}
	else
		psxSetNextBranch( psxRegs.sCycle[n], psxRegs.eCycle[n] );
}

static __forceinline void _psxTestInterrupts()
{
	PSX_TESTINT(9, sif0Interrupt, 1);	// SIF0
	PSX_TESTINT(10, sif1Interrupt, 1);	// SIF1
	PSX_TESTINT(16, sioInterrupt, 0);
	PSX_TESTINT(19, cdvdReadInterrupt, 1);

	// Profile-guided Optimization (sorta)
	// The following ints are rarely called.  Encasing them in a conditional
	// as follows helps speed up most games.

	if( psxRegs.interrupt & ( (3ul<<11) | (3ul<<20) | (3ul<<17) ) )
	{
		PSX_TESTINT(11, psxDMA11Interrupt,0);	// SIO2
		PSX_TESTINT(12, psxDMA12Interrupt,0);	// SIO2
		PSX_TESTINT(17, cdrInterrupt,0);
		PSX_TESTINT(18, cdrReadInterrupt,0);
		PSX_TESTINT(20, dev9Interrupt,1);
		PSX_TESTINT(21, usbInterrupt,1);
	}
}

void psxBranchTest()
{
	if( psxTestCycle( psxNextsCounter, psxNextCounter ) )
	{
		psxRcntUpdate();
		iopBranchAction = 1;
	}

	// start the next branch at the next counter event by default
	// the interrupt code below will assign nearer branches if needed.
	g_psxNextBranchCycle = psxNextsCounter+psxNextCounter;
	
	if (psxRegs.interrupt) _psxTestInterrupts();

	if (psxHu32(0x1078)) {
		if(psxHu32(0x1070) & psxHu32(0x1074)){
			if ((psxRegs.CP0.n.Status & 0xFE01) >= 0x401)
			{
//				PSXCPU_LOG("Interrupt: %x  %x\n", HWMu32(0x1070), HWMu32(0x1074));
				psxException(0, 0);
				iopBranchAction = 1;
			}
		}
	}
}

void psxExecuteBios() {
/*	while (psxRegs.pc != 0x80030000)
		psxCpu->ExecuteBlock();
	PSX_LOG("*BIOS END*\n");
*/
}

void psxRestartCPU()
{
	psxCpu->Shutdown();
	psxCpu = CHECK_EEREC ? &psxRec : &psxInt;

	if (psxCpu->Init() == -1) {
		SysClose();
		exit(1);
	}
	psxCpu->Reset();
}
