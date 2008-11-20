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

typedef struct
{
	u32 SIGID;
	u32 LBLID;
} GSRegSIGBLID;

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

#define GS_RINGBUFFERBASE	(u8*)(0x10200000)
#define GS_RINGBUFFERSIZE	0x00300000 // 3Mb
#define GS_RINGBUFFEREND	(u8*)(GS_RINGBUFFERBASE+GS_RINGBUFFERSIZE)

#define GS_RINGTYPE_RESTART 0
#define GS_RINGTYPE_P1		1
#define GS_RINGTYPE_P2		2
#define GS_RINGTYPE_P3		3
#define GS_RINGTYPE_VSYNC	4
#define GS_RINGTYPE_VIFFIFO	5 // GSreadFIFO2
#define GS_RINGTYPE_FRAMESKIP	6
#define GS_RINGTYPE_MEMWRITE8	7
#define GS_RINGTYPE_MEMWRITE16	8
#define GS_RINGTYPE_MEMWRITE32	9
#define GS_RINGTYPE_MEMWRITE64	10
#define GS_RINGTYPE_SAVE 11
#define GS_RINGTYPE_LOAD 12
#define GS_RINGTYPE_RECORD 13

// if returns NULL, don't copy (memory is preserved)
u8* GSRingBufCopy(void* mem, u32 size, u32 type);
void GSRingBufSimplePacket(int type, int data0, int data1, int data2);

//#ifdef PCSX2_DEVBUILD

// use for debugging MTGS
//extern FILE* g_fMTGSWrite, *g_fMTGSRead;
//extern u32 g_MTGSDebug, g_MTGSId;

//static __forceinline void MTGS_RECWRITE(const u8 *start, u32 size) { 
//	if( g_MTGSDebug & 1 ) { 
//		const u32* pstart = (u32*)(start); 
//		u32 cursize = (size); 
//		fprintf(g_fMTGSWrite, "*%x-%x (%d)\n", (u32)(uptr)(start), (u32)(size), ++g_MTGSId); 
//		if( g_MTGSDebug & 2 ) fflush(g_fMTGSWrite); 
//	} 
//} 

//static __forceinline void MTGS_RECREAD( const u8* start, u32 size) { 
//	if( g_MTGSDebug & 1 ) { 
//		const u32* pstart = (u32*)(start); 
//		u32 cursize = (size); 
//		fprintf(g_fMTGSRead, "*%x-%x (%d)\n", (u32)(uptr)(start), (u32)(size), ++g_MTGSId); 
//		if( g_MTGSDebug & 4 ) fflush(g_fMTGSRead); 
//	} 
//} 
//#else

//#define MTGS_RECWRITE 0&&
//#define MTGS_RECREAD 0&&

//#endif

#if defined(_WIN32) && !defined(WIN32_PTHREADS)

#define GS_SETEVENT() SetEvent(g_hGsEvent)

#else

#include <pthread.h>
#include <semaphore.h>
extern sem_t g_semGsThread;
#define GS_SETEVENT() sem_post(&g_semGsThread)

#endif

u32 GSgifTransferDummy(int path, u32 *pMem, u32 size);

void gsInit();
void gsShutdown();
void gsReset();

// mem and size are the ones from GSRingBufCopy
extern void GSRINGBUF_DONECOPY(const u8 *mem, u32 size);
extern void gsWaitGS();

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
