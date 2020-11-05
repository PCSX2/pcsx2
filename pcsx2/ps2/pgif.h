/*  PCSX2 - PS2 Emulator for PCs
*  Copyright (C) 2016-2016  PCSX2 Dev Team
*  Copyright (C) 2016 Wisi
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

void pgifInit(void);
void pgifReset(void);

extern void psxGPUw(int, u32);
extern u32 psxGPUr(int);

extern void PGIFw(int, u32);
extern u32 PGIFr(int);

extern void PGIFwQword(u32 addr, void *);
extern void PGIFrQword(u32 addr, void *);

extern u32 psxDma2GpuR(u32 addr);
extern void psxDma2GpuW(u32 addr, u32 data);


extern void ps12PostOut(u32 mem, u8 value);
extern void psDuartW(u32 mem, u8 value);
extern u8 psExp2R8(u32 mem);
extern void kernelTTYFileDescrWrite(u32 mem, u32 data);
extern u32 getIntTmrKReg(u32 mem, u32 data);
extern void testInt(void);
extern void HPCoS_print(u32 mem, u32 data);
extern void anyIopLS(u32 addr, u32 data, int Wr);
extern void dma6_OTClear(void);