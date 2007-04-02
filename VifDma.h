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
#ifndef __VIFDMA_H__
#define __VIFDMA_H__

typedef struct {
   int addr;
   int size;
   int cmd;
   u16 wl;
   u16 cl;
} vifCode;

// NOTE, if debugging vif stalls, use sega classics, spyro, gt4, and taito
typedef struct {
	vifCode tag;
	int cmd;
	int irq;
	int cl;
	int wl;
	u8 usn;
	u8 done;
	u8 vifstalled;
	u8 irqoffset; // 32bit offset where next vif code is
	u32 savedtag;
} vifStruct;

extern vifStruct vif0, vif1;

#define vif0ch ((DMACh*)&PS2MEM_HW[0x8000])
#define vif1ch ((DMACh*)&PS2MEM_HW[0x9000])

void UNPACK_S_32( u32 *dest, u32 *data );
int  UNPACK_S_32part( u32 *dest, u32 *data, int size );

void UNPACK_S_16u( u32 *dest, u32 *data );
int  UNPACK_S_16upart( u32 *dest, u32 *data, int size );
void UNPACK_S_16s( u32 *dest, u32 *data );
int  UNPACK_S_16spart( u32 *dest, u32 *data, int size );

void UNPACK_S_8u( u32 *dest, u32 *data );
int  UNPACK_S_8upart( u32 *dest, u32 *data, int size );
void UNPACK_S_8s( u32 *dest, u32 *data );
int  UNPACK_S_8spart( u32 *dest, u32 *data, int size );

void UNPACK_V2_32( u32 *dest, u32 *data );
int  UNPACK_V2_32part( u32 *dest, u32 *data, int size );

void UNPACK_V2_16u( u32 *dest, u32 *data );
int  UNPACK_V2_16upart( u32 *dest, u32 *data, int size );
void UNPACK_V2_16s( u32 *dest, u32 *data );
int  UNPACK_V2_16spart( u32 *dest, u32 *data, int size );

void UNPACK_V2_8u( u32 *dest, u32 *data );
int  UNPACK_V2_8upart( u32 *dest, u32 *data, int size );
void UNPACK_V2_8s( u32 *dest, u32 *data );
int  UNPACK_V2_8spart( u32 *dest, u32 *data, int size );

void UNPACK_V3_32( u32 *dest, u32 *data );
int  UNPACK_V3_32part( u32 *dest, u32 *data, int size );

void UNPACK_V3_16u( u32 *dest, u32 *data );
int  UNPACK_V3_16upart( u32 *dest, u32 *data, int size );
void UNPACK_V3_16s( u32 *dest, u32 *data );
int  UNPACK_V3_16spart( u32 *dest, u32 *data, int size );

void UNPACK_V3_8u( u32 *dest, u32 *data );
int  UNPACK_V3_8upart( u32 *dest, u32 *data, int size );
void UNPACK_V3_8s( u32 *dest, u32 *data );
int  UNPACK_V3_8spart( u32 *dest, u32 *data, int size );

void UNPACK_V4_32( u32 *dest, u32 *data );
int  UNPACK_V4_32part( u32 *dest, u32 *data, int size );

void UNPACK_V4_16u( u32 *dest, u32 *data );
int  UNPACK_V4_16upart( u32 *dest, u32 *data, int size );
void UNPACK_V4_16s( u32 *dest, u32 *data );
int  UNPACK_V4_16spart( u32 *dest, u32 *data, int size );

void UNPACK_V4_8u( u32 *dest, u32 *data );
int  UNPACK_V4_8upart( u32 *dest, u32 *data, int size );
void UNPACK_V4_8s( u32 *dest, u32 *data );
int  UNPACK_V4_8spart( u32 *dest, u32 *data, int size );

void UNPACK_V4_5( u32 *dest, u32 *data );
int  UNPACK_V4_5part( u32 *dest, u32 *data, int size );
extern void (*Vif0CMDTLB[75])();  // VIF0 CMD Table
extern int  (*Vif0TransTLB[128])(); // VIF0 Transfer Data Table
int Vif0TransNull(u32 *data, int size); //Shouldnt Go Here
int Vif0TransSTMask(u32 *data, int size); //STCOL
int Vif0TransSTRow(u32 *data, int size); //STCOL
int Vif0TransSTCol(u32 *data, int size); //STCOL
int Vif0TransMPG(u32 *data, int size); // MPG
int Vif0TransUnpack(u32 *data, int size); // UNPACK
void Vif0CMDNop(); // NOP
void Vif0CMDSTCycl(); // STCYCL
void Vif0CMDITop(); // ITOP
void Vif0CMDSTMod(); // STMOD
void Vif0CMDMark(); // MARK
void Vif0CMDFlushE(); // FLUSHE
void Vif0CMDMSCALF(); //MSCAL/F
void Vif0CMDMSCNT(); // MSCNT
void Vif0CMDSTMask(); // STMASK 
void Vif0CMDSTRowCol();// STROW / STCOL
void Vif0CMDMPGTransfer(); // MPG
void Vif0CMDNull(); // invalid opcode


extern void (*Vif1CMDTLB[82])(); // VIF1 CMD Table
extern int  (*Vif1TransTLB[128])(); // VIF1 Transfer Data Table
int Vif1TransNull(u32 *data, int size); //Shouldnt Go Here
int Vif1TransSTMask(u32 *data, int size); //STCOL
int Vif1TransSTRow(u32 *data, int size); //STCOL
int Vif1TransSTCol(u32 *data, int size); //STCOL
int Vif1TransMPG(u32 *data, int size); // MPG
int Vif1TransDirectHL(u32 *data, int size); // DIRECT/HL
int Vif1TransUnpack(u32 *data, int size); // UNPACK
void Vif1CMDNop(); // NOP
void Vif1CMDSTCycl(); // STCYCL
void Vif1CMDOffset(); // OFFSET
void Vif1CMDBase(); // BASE
void Vif1CMDITop(); // ITOP
void Vif1CMDSTMod(); // STMOD
void Vif1CMDMskPath3(); // MSKPATH3
void Vif1CMDMark(); // MARK
void Vif1CMDFlush(); // FLUSH/E/A
void Vif1CMDMSCALF(); //MSCAL/F
void Vif1CMDMSCNT(); // MSCNT
void Vif1CMDSTMask(); // STMASK 
void Vif1CMDSTRowCol();// STROW / STCOL
void Vif1CMDMPGTransfer(); // MPG
void Vif1CMDDirectHL(); // DIRECT/HL
void Vif1CMDNull(); // invalid opcode

void vifDmaInit();
void vif0Init();
void vif1Init();
int  vif0Interrupt();
int  vif1Interrupt();

void vif0Write32(u32 mem, u32 value);
void vif1Write32(u32 mem, u32 value);

void vif0Reset();
void vif1Reset();
int  vif0Freeze(gzFile f, int Mode);
int  vif1Freeze(gzFile f, int Mode);

#endif
