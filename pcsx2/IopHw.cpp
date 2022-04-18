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
#include "R3000A.h"
#include "Common.h"

#include "iR5900.h"
#include "Sio.h"
#include "Mdec.h"
#include "IopSio2.h"
#include "IopCounters.h"
#include "IopHw.h"
#include "IopDma.h"
#include "CDVD/Ps1CD.h"
#include "CDVD/CDVD.h"


// NOTE: Any modifications to read/write fns should also go into their const counterparts
// found in iPsxHw.cpp.

void psxHwReset() {
/*	if (Config.Sio) psxHu32(0x1070) |= 0x80;
	if (Config.SpuIrq) psxHu32(0x1070) |= 0x200;*/

	memset(iopHw, 0, 0x10000);

	mdecInit(); //initialize mdec decoder
	cdrReset();
	cdvdReset();
	psxRcntInit();
	sioInit();
	//sio2Reset();
}

__fi u8 psxHw4Read8(u32 add)
{
	u16 mem = add & 0xFF;
	u8 ret = cdvdRead(mem);
	PSXHW_LOG("HwRead8 from Cdvd [segment 0x1f40], addr 0x%02x = 0x%02x", mem, ret);
	return ret;
}

__fi void psxHw4Write8(u32 add, u8 value)
{
	u8 mem = (u8)add;	// only lower 8 bits are relevant (cdvd regs mirror across the page)
	cdvdWrite(mem, value);
	PSXHW_LOG("HwWrite8 to Cdvd [segment 0x1f40], addr 0x%02x = 0x%02x", mem, value);
}

void psxDmaInterrupt(int n)
{
	if(n == 33) {
		for (int i = 0; i < 6; i++) {
			if (HW_DMA_ICR & (1 << (16 + i))) {
				if (HW_DMA_ICR & (1 << (24 + i))) {
					if (HW_DMA_ICR & (1 << 23)) {
						HW_DMA_ICR |= 0x80000000; //Set master IRQ condition met
					}
					psxRegs.CP0.n.Cause &= ~0x7C;
					iopIntcIrq(3);
					break;
				}
			}
		}
	} else if (HW_DMA_ICR & (1 << (16 + n)))
	{
		HW_DMA_ICR |= (1 << (24 + n));
		if (HW_DMA_ICR & (1 << 23)) {
			HW_DMA_ICR |= 0x80000000; //Set master IRQ condition met
		}
		iopIntcIrq(3);
	}
}

void psxDmaInterrupt2(int n)
{
	// SIF0 and SIF1 DMA IRQ's cannot be supressed due to a mask flag for "tag" interrupts being available which cannot be disabled.
	// The hardware can't disinguish between the DMA End and Tag Interrupt flags on these channels so interrupts always fire
	bool fire_interrupt = n == 2 || n == 3;

	if (n == 33) {
		for (int i = 0; i < 6; i++) {
			if (HW_DMA_ICR2 & (1 << (24 + i))) {
				if (HW_DMA_ICR2 & (1 << (16 + i)) || i == 2 || i == 3) {
					fire_interrupt = true;
					break;
				}
			}
		}
	}
	else if (HW_DMA_ICR2 & (1 << (16 + n)))
	{
		/*
		if (HW_DMA_ICR2 & (1 << (24 + n))) {
			Console.WriteLn("*PCSX2*: HW_DMA_ICR2 n=%d already set", n);
		}
		if (psxHu32(0x1070) & 8) {
			Console.WriteLn("*PCSX2*: psxHu32(0x1070) 8 already set (n=%d)", n);
		}*/
		fire_interrupt = true;
	}

	if (fire_interrupt)
	{
		if(n != 33)
			HW_DMA_ICR2 |= (1 << (24 + n));

		if (HW_DMA_ICR2 & (1 << 23)) {
			HW_DMA_ICR2 |= 0x80000000; //Set master IRQ condition met
		}
		iopIntcIrq(3);
	}
}
