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

#include "PrecompiledHeader.h"
#include "Common.h"

#include "Hardware.h"
#include "newVif.h"

using namespace R5900;
using namespace EE_DMAC;

const int rdram_devices = 2;	// put 8 for TOOL and 2 for PS2 and PSX
int rdram_sdevid = 0;

static bool hwInitialized = false;

void hwInit()
{
	// [TODO] / FIXME:  PCSX2 no longer works on an Init system.  It assumes that the
	// static global vars for the process will be initialized when the process is created, and
	// then issues *resets only* from then on. (reset code for various S2 components should do
	// NULL checks and allocate memory and such if the pointers are NULL only).

	if( hwInitialized ) return;

	VifUnpackSSE_Init();

	gsInit();
	ipuInit();

	hwInitialized = true;
}

void hwReset()
{
	hwInit();

	memzero( eeHw );
	memzero( g_fifo );

	psHu32(SBUS_F260) = 0x1D000060;

	// i guess this is kinda a version, it's used by some bioses
	psHu32(DMAC_ENABLEW) = 0x1201;
	psHu32(DMAC_ENABLER) = 0x1201;

	SPU2reset();

	gsReset();
	ipuReset();
	vifReset();
	sifInit();
}

__fi uint intcInterrupt()
{
	if ((psHu32(INTC_STAT)) == 0) {
		//DevCon.Warning("*PCSX2*: intcInterrupt already cleared");
        return 0;
	}
	if ((psHu32(INTC_STAT) & psHu32(INTC_MASK)) == 0) 
	{
		//DevCon.Warning("*PCSX2*: No valid interrupt INTC_MASK: %x INTC_STAT: %x", psHu32(INTC_MASK), psHu32(INTC_STAT));
		return 0;
	}

	HW_LOG("intcInterrupt %x", psHu32(INTC_STAT) & psHu32(INTC_MASK));
	if(psHu32(INTC_STAT) & 0x2){
		counters[0].hold = rcntRcount(0);
		counters[1].hold = rcntRcount(1);
	}

	//cpuException(0x400, cpuRegs.branch);
	return 0x400;
}

__fi uint dmacInterrupt()
{
	if( ((psHu16(DMAC_STAT + 2) & psHu16(DMAC_STAT)) == 0 ) &&
		( psHu16(DMAC_STAT) & 0x8000) == 0 ) 
	{
		//DevCon.Warning("No valid DMAC interrupt MASK %x STAT %x", psHu16(DMAC_STAT+2), psHu16(DMAC_STAT));
		return 0;
	}

	if (!dmacRegs.ctrl.DMAE || psHu8(DMAC_ENABLER+2) == 1) 
	{
		//DevCon.Warning("DMAC Suspended or Disabled on interrupt");
		return 0;
	}
	HW_LOG("dmacInterrupt %x", (psHu16(DMAC_STAT + 2) & psHu16(DMAC_STAT) |
								  psHu16(DMAC_STAT) & 0x8000));

	//cpuException(0x800, cpuRegs.branch);
	return 0x800;
}

void hwIntcIrq(int n)
{
	psHu32(INTC_STAT) |= 1<<n;
	if(psHu32(INTC_MASK) & (1<<n)) cpuTestINTCInts();
}
