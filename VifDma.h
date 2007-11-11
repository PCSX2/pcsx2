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
	u8 stallontag;
	u8 irqoffset; // 32bit offset where next vif code is
    u32 savedtag; // need this for backwards compat with save states
} vifStruct;

extern vifStruct vif0, vif1;

#define vif0ch ((DMACh*)&PS2MEM_HW[0x8000])
#define vif1ch ((DMACh*)&PS2MEM_HW[0x9000])

void UNPACK_S_32( u32 *dest, u32 *data, int size );

void UNPACK_S_16u( u32 *dest, u32 *data, int size );
void UNPACK_S_16s( u32 *dest, u32 *data, int size );

void UNPACK_S_8u( u32 *dest, u32 *data, int size );
void UNPACK_S_8s( u32 *dest, u32 *data, int size );

void UNPACK_V2_32( u32 *dest, u32 *data, int size );

void UNPACK_V2_16u( u32 *dest, u32 *data, int size );
void UNPACK_V2_16s( u32 *dest, u32 *data, int size );

void UNPACK_V2_8u( u32 *dest, u32 *data, int size );
void UNPACK_V2_8s( u32 *dest, u32 *data, int size );

void UNPACK_V3_32( u32 *dest, u32 *data, int size );

void UNPACK_V3_16u( u32 *dest, u32 *data, int size );
void UNPACK_V3_16s( u32 *dest, u32 *data, int size );

void UNPACK_V3_8u( u32 *dest, u32 *data, int size );
void UNPACK_V3_8s( u32 *dest, u32 *data, int size );

void UNPACK_V4_32( u32 *dest, u32 *data, int size );

void UNPACK_V4_16u( u32 *dest, u32 *data, int size );
void UNPACK_V4_16s( u32 *dest, u32 *data, int size );

void UNPACK_V4_8u( u32 *dest, u32 *data, int size );
void UNPACK_V4_8s( u32 *dest, u32 *data, int size );

void UNPACK_V4_5( u32 *dest, u32 *data, int size );

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
