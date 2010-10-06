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


// This module contains code shared by both the dynarec and interpreter versions
// of the VU0 micro.

#include "PrecompiledHeader.h"
#include "Common.h"

#include <cmath>

#include "VUmicro.h"
#include "Gif.h"
#include "ps2\NewDmac.h"

using namespace EE_DMAC;

#ifdef PCSX2_DEBUG
u32 vudump = 0;
#endif

#define VF_VAL(x) ((x==0x80000000)?0:(x))

__fi bool vu1Running()
{
	return !!(VU0.VI[REG_VPU_STAT].UL & 0x100);
}

// This is called by the COP2 as per FBRST being written via the CTC2 instruction.
void vu1ResetRegs()
{
	VU0.VI[REG_VPU_STAT].UL	&= ~0xff00; // stop vu1
	VU0.VI[REG_FBRST].UL	&= ~0xff00; // stop vu1

	// In case the VIF1 is waiting on the VU1 to end:
	if (vif1Regs.stat.VEW)
		dmacRequestSlice(ChanId_VIF1);
}

// Returns TRUE if the VU1 finished, and is either in STOP or READY state.
// Returns FALSE if execution was stalled for some reason (typically an XGKICK).
bool vu1Finish() {
	if (!vu1Running()) return true;

	// if the VU1 is waiting on an XGKICK transfer, then there's really nothing we can
	// do.  The XGKICK will be completed in due time when the blocking condition (either
	// SIGNAL/IMR or a pending PATH2/PATH3 transfer) is cleared.
	
	if (gifRegs.stat.P1Q)
	{
		VUM_LOG("VU1 stalled due to XGKICK.");
		return false;
	}
	
	CpuVU1->Execute(vu1RunCycles);
	return !vu1Running();
}

bool vu1ResumeXGKICK()
{
	if (!vu1Running()) return true;
	CpuVU1->ResumeXGkick();

	if (!vu1Running())
	{
		// In case the VIF1 is waiting on the VU1 to end:
		if (vif1Regs.stat.VEW)
			dmacRequestSlice(ChanId_VIF1);
		return true;
	}
	return false;
}

void __fastcall vu1ExecMicro(u32 addr)
{
	pxAssumeDev(!vu1Running(), "vu1ExecMicro: VU1 already running!" );

	VUM_LOG("vu1ExecMicro %x", addr);

	VU0.VI[REG_VPU_STAT].UL &= ~0xFF00;
	VU0.VI[REG_VPU_STAT].UL |=  0x0100;

	if ((s32)addr != -1) VU1.VI[REG_TPC].UL = addr;
	_vuExecMicroDebug(VU1);

	CpuVU1->Execute(vu1RunCycles);
}
