/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2003  Pcsx2 Team
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

#ifndef __GS_H__
#define __GS_H__

#include "Common.h"

typedef struct
{
	u32 SIGID;
	u32 LBLID;
} GSRegSIGBLID;

#ifdef WIN32_VIRTUAL_MEM
#define GSCSRr *((u64*)(PS2MEM_GS+0x1000))
#define GSIMR *((u32*)(PS2MEM_GS+0x1010))
#define GSSIGLBLID ((GSRegSIGBLID*)(PS2MEM_GS+0x1080))
#else
extern u8 g_RealGSMem[0x2000];
#define GSCSRr *((u64*)(g_RealGSMem+0x1000))
#define GSIMR *((u32*)(g_RealGSMem+0x1010))
#define GSSIGLBLID ((GSRegSIGBLID*)(g_RealGSMem+0x1080))
#endif

#define GS_RINGBUFFERBASE	(u8*)(0x10200000)
#define GS_RINGBUFFERSIZE	0x00200000 // 2Mb
#define GS_RINGBUFFEREND	(u8*)(GS_RINGBUFFERBASE+GS_RINGBUFFERSIZE)

#define GS_RINGTEMPBASE	(u8*)(0x10400000)
#define GS_RINGTEMPSIZE	0x00100000 // 1Mb
#define GS_RINGTEMPEND	(u8*)(GS_RINGTEMPBASE+GS_RINGTEMPSIZE)

#define GS_SHIFT 12
#define ENABLE_GS_CACHING 0 // set to 0 to disable

// mem addrs to support
#define GSPAGES_MEMADDRS	4
#define GSPAGES_STRIDE		((1+GSPAGES_MEMADDRS)*4)
#define GS_PAGEADDRS_		0x10500000
#define GS_PAGEADDRS		(u32*)(GS_PAGEADDRS_)

#define GS_RINGTYPE_RESTART 0
#define GS_RINGTYPE_P1		1
#define GS_RINGTYPE_P2		2
#define GS_RINGTYPE_P3		3
#define GS_RINGTYPE_VSYNC	4
#define GS_RINGTYPE_VIFFIFO	5 // GSreadFIFO2
#define GS_RINGTYPE_MEMREF	0x10 // if bit set, memory is preserved (only valid for p2/p3)
#define GS_RINGTYPE_MEMNOFREE 0x20 // if bit set, don't free or mem

// if returns NULL, don't copy (memory is preserved)
u8* GSRingBufCopy(void* mem, u32 size, u32 type);
void GSRingBufVSync(int field);
void GSFreePage(u32* pGSPage);

extern u8* g_pGSWritePos;

// mem and size are the ones from GSRingBufCopy
#define GSRINGBUF_DONECOPY(mem, size) { \
	u8* temp = (u8*)(mem) + (size); \
	if( temp == GS_RINGBUFFEREND ) temp = GS_RINGBUFFERBASE; \
	InterlockedExchangePointer(&g_pGSWritePos, temp); \
}

u32 GSgifTransferDummy(int path, u32 *pMem, u32 size);

void gsInit();
void gsShutdown();
void gsReset();

// used for resetting GIF fifo
void gsGIFReset();

void gsWrite8(u32 mem, u8 value);
void gsConstWrite8(u32 mem, int mmreg);

void gsWrite16(u32 mem, u16 value);
void gsConstWrite16(u32 mem, int mmreg);

void gsWrite32(u32 mem, u32 value);
void gsConstWrite32(u32 mem, int mmreg);

void gsWrite64(u32 mem, u64 value);
void gsConstWrite64(u32 mem, int mmreg);

void  gsConstWrite128(u32 mem, int mmreg);

u8   gsRead8(u32 mem);
int gsConstRead8(u32 x86reg, u32 mem, u32 sign);

u16  gsRead16(u32 mem);
int gsConstRead16(u32 x86reg, u32 mem, u32 sign);

u32  gsRead32(u32 mem);
int gsConstRead32(u32 x86reg, u32 mem);

u64  gsRead64(u32 mem);
void  gsConstRead64(u32 mem, int mmreg);

void  gsConstRead128(u32 mem, int xmmreg);

void gsIrq();
int  gsInterrupt();
void dmaGIF();
void mfifoGIFtransfer(int qwc);
int  gsFreeze(gzFile f, int Mode);
int _GIFchain();
int  gifMFIFOInterrupt();

#endif
