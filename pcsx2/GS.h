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

#ifndef __GS_H__
#define __GS_H__

// GCC needs these includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zlib.h"

struct GSRegSIGBLID
{
	u32 SIGID;
	u32 LBLID;
};

#define GSPATH3FIX

#ifdef PCSX2_VIRTUAL_MEM
#define GSCSRr *((u64*)(PS2MEM_GS+0x1000))
#define GSIMR *((u32*)(PS2MEM_GS+0x1010))
#define GSSIGLBLID ((GSRegSIGBLID*)(PS2MEM_GS+0x1080))
#else
extern u8 g_RealGSMem[0x2000];
#define GSCSRr *((u64*)(g_RealGSMem+0x1000))
#define GSIMR *((u32*)(g_RealGSMem+0x1010))
#define GSSIGLBLID ((GSRegSIGBLID*)(g_RealGSMem+0x1080))
#endif

#define GS_RINGBUFFERBASE	GS_RINGBUFF_STOREAGE
#define GS_RINGBUFFERSIZE	0x00300000 // 3Mb
#define GS_RINGBUFFEREND	(u8*)(GS_RINGBUFFERBASE+GS_RINGBUFFERSIZE)

__declspec(align(4096)) extern u8 GS_RINGBUFF_STOREAGE[GS_RINGBUFFERSIZE];
enum GS_RINGTYPE
{
	GS_RINGTYPE_RESTART = 0
,	GS_RINGTYPE_P1
,	GS_RINGTYPE_P2
,	GS_RINGTYPE_P3
,	GS_RINGTYPE_VSYNC
,	GS_RINGTYPE_FRAMESKIP
,	GS_RINGTYPE_MEMWRITE8
,	GS_RINGTYPE_MEMWRITE16
,	GS_RINGTYPE_MEMWRITE32
,	GS_RINGTYPE_MEMWRITE64
,	GS_RINGTYPE_SAVE
,	GS_RINGTYPE_LOAD
,	GS_RINGTYPE_RECORD
,	GS_RINGTYPE_RESET		// issues a GSreset() command.
,	GS_RINGTYPE_SOFTRESET	// issues a soft reset for the GIF
,	GS_RINGTYPE_WRITECSR
,	GS_RINGTYPE_MODECHANGE	// for issued mode changes.
,	GS_RINGTYPE_STARTTIME	// special case for min==max fps frameskip settings
};

// if returns NULL, don't copy (memory is preserved)
u8* GSRingBufCopy(u32 size, u32 type);
void GSRingBufSimplePacket(int type, int data0, int data1, int data2);

u32 GSgifTransferDummy(int path, u32 *pMem, u32 size);

void gsInit();
s32 gsOpen();
void gsShutdown();
void gsReset();
void gsSetVideoRegionType( u32 isPal );
void gsResetFrameSkip();
void gsSyncLimiterLostTime( s32 deltaTime );
void gsDynamicSkipEnable();

// mem and size are the ones from GSRingBufCopy
extern void GSRINGBUF_DONECOPY(const u8 *mem, u32 size);
extern void gsWaitGS();

// used for resetting GIF fifo
void gsGIFReset();
void gsCSRwrite(u32 value);

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
extern void gsInterrupt();
void dmaGIF();
void GIFdma();
void mfifoGIFtransfer(int qwc);
int  gsFreeze(gzFile f, int Mode);
int _GIFchain();
void  gifMFIFOInterrupt();

// GS Playback
#define GSRUN_TRANS1 1
#define GSRUN_TRANS2 2
#define GSRUN_TRANS3 3
#define GSRUN_VSYNC 4

#ifdef PCSX2_DEVBUILD

extern int g_SaveGSStream;
extern int g_nLeftGSFrames;
extern gzFile g_fGSSave;

#endif

void RunGSState(gzFile f);

extern void GSGIFTRANSFER1(u32 *pMem, u32 addr); 
extern void GSGIFTRANSFER2(u32 *pMem, u32 addr); 
extern void GSGIFTRANSFER3(u32 *pMem, u32 addr);
extern void GSVSYNC();

#endif
