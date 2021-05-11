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

#pragma once 

extern void psxDma2(u32 madr, u32 bcr, u32 chcr);
extern void psxDma3(u32 madr, u32 bcr, u32 chcr);
extern void psxDma6(u32 madr, u32 bcr, u32 chcr);
extern void psxDma4(u32 madr, u32 bcr, u32 chcr);
extern void psxDma7(u32 madr, u32 bcr, u32 chcr);
extern void psxDma8(u32 madr, u32 bcr, u32 chcr);
extern void psxDma9(u32 madr, u32 bcr, u32 chcr);
extern void psxDma10(u32 madr, u32 bcr, u32 chcr);

extern int  psxDma4Interrupt();
extern int  psxDma7Interrupt();
extern void dev9Interrupt();
extern void dev9Irq(int cycles);
extern void usbInterrupt();
extern void usbIrq(int cycles);
extern void fwIrq();
extern void spu2Irq();
extern void spu2DMA4Irq();
extern void spu2DMA7Irq();

extern void iopIntcIrq( uint irqType );
extern void iopTestIntc();
