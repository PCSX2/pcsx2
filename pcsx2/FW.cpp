/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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
#include "IopDma.h"
#include "R3000A.h"
#include <stdlib.h>
#include <string>
using namespace std;

#include "FW.h"

u8 phyregs[16];
s8* fwregs;

s32 FWopen()
{
	memset(phyregs, 0, sizeof(phyregs));
	// Initializing our registers.
	fwregs = (s8*)calloc(0x10000, 1);
	if (fwregs == NULL)
	{
		DevCon.WriteLn("FW: Error allocating Memory");
		return -1;
	}
	return 0;
}

void FWclose()
{
	// Freeing the registers.
	free(fwregs);
	fwregs = NULL;
}

void PHYWrite()
{
	u8 reg = (PHYACC >> 8) & 0xf;
	u8 data = PHYACC & 0xff;

	phyregs[reg] = data;

	PHYACC &= ~0x4000ffff;
}

void PHYRead()
{
	u8 reg = (PHYACC >> 24) & 0xf;

	PHYACC &= ~0x80000000;

	PHYACC |= phyregs[reg] | (reg << 8);

	if (fwRu32(0x8424) & 0x40000000) //RRx interrupt mask
	{
		fwRu32(0x8420) |= 0x40000000;
		fwIrq();
	}
}

u32 FWread32(u32 addr)
{
	u32 ret = 0;

	switch (addr)
	{
		//Node ID Register the top part is default, bottom part i got from my ps2
		case 0x1f808400:
			ret = /*(0x3ff << 22) | 1;*/ 0xffc00001;
			break;
		// Control Register 2
		case 0x1f808410:
			ret = fwRu32(addr); //SCLK OK (Needs to be set when FW is "Ready"
			break;
		//Interrupt 0 Register
		case 0x1f808420:
			ret = fwRu32(addr);
			break;

		//Dunno what this is, but my home console always returns this value 0x10000001
		//Seems to be related to the Node ID however (does some sort of compare/check)
		case 0x1f80847c:
			ret = 0x10000001;
			break;

		// Include other relevant 32 bit addresses we need to catch here.
		default:
			// By default, read fwregs.
			ret = fwRu32(addr);
			break;
	}

	DevCon.WriteLn("FW: read mem 0x%x: 0x%x", addr, ret);

	return ret;
}

void FWwrite32(u32 addr, u32 value)
{
	switch (addr)
	{
		//		Include other memory locations we want to catch here.
		//		For example:
		//
		//		case 0x1f808400:
		//		case 0x1f808414:
		//		case 0x1f808420:
		//		case 0x1f808428:
		//		case 0x1f808430:
		//

		//PHY access
		case 0x1f808414:
			//If in read mode (top bit set) we read the PHY register requested then set the RRx interrupt if it's enabled
			//Im presuming we send that back to pcsx2 then. This register stores the result, plus whatever was written (minus the read/write flag
			fwRu32(addr) = value;   //R/W Bit cleaned in underneath function
			if (value & 0x40000000) //Writing to PHY
			{
				PHYWrite();
			}
			else if (value & 0x80000000) //Reading from PHY
			{
				PHYRead();
			}
			break;

		//Control Register 0
		case 0x1f808408:
			//This enables different functions of the link interface
			//Just straight writes, should brobably struct these later.
			//Default written settings (on unreal tournament) are
			//Urcv M = 1
			//RSP 0 = 1
			//Retlim = 0xF
			//Cyc Tmr En = 1
			//Bus ID Rst = 1
			//Rcv Self ID = 1
			fwRu32(addr) = value;
			//	if((value & 0x800000) && (fwRu32(0x842C) & 0x2))
			//	{
			//		fwRu32(0x8428) |= 0x2;
			//		FWirq();
			//	}
			fwRu32(addr) &= ~0x800000;
			break;
		//Control Register 2
		case 0x1f808410: // fwRu32(addr) = value; break;
			//Ignore writes to this for now, apart from 0x2 which is Link Power Enable
			//0x8 is SCLK OK (Ready) which should be set for emulation
			fwRu32(addr) = 0x8 /*| value & 0x2*/;
			break;
		//Interrupt 0 Register
		case 0x1f808420:
		//Interrupt 1 Register
		case 0x1f808428:
		//Interrupt 2 Register
		case 0x1f808430:
			//Writes of 1 clear the corresponding bits
			fwRu32(addr) &= ~value;
			break;
		//Interrupt 0 Register Mask
		case 0x1f808424:
		//Interrupt 1 Register Mask
		case 0x1f80842C:
		//Interrupt 2 Register Mask
		case 0x1f808434:
			//These are direct writes (as it's a mask!)
			fwRu32(addr) = value;
			break;
		//DMA Control and Status Register 0
		case 0x1f8084B8:
			fwRu32(addr) = value;
			break;
		//DMA Control and Status Register 1
		case 0x1f808538:
			fwRu32(addr) = value;
			break;
		default:
			// By default, just write it to fwregs.
			fwRu32(addr) = value;
			break;
	}
	DevCon.WriteLn("FW: write mem 0x%x: 0x%x", addr, value);
}
