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

// rewritten by zerofrog to add multithreading/gs caching to GS and VU1

#include "PS2Etypes.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#include <assert.h>
#include <vector>
#include <list>
#include <cstring> 
#include <cstdlib>

using namespace std;

#ifndef _WIN32
// fixme - Linux needs a proper implementation of locks using pthreads.
// a set of placebo types and functions might do for now though.

typedef int CRITICAL_SECTION;

void EnterCriticalSection( CRITICAL_SECTION* handle ) {}
void LeaveCriticalSection( CRITICAL_SECTION* handle ) {}

void InitializeCriticalSection( CRITICAL_SECTION* handle ) {}
void DeleteCriticalSection( CRITICAL_SECTION* handle ) {}
#endif


extern "C" {

#define PLUGINtypedefs // for GSgifTransfer1

#include "PS2Edefs.h"
#include "zlib.h"
#include "Elfheader.h"
#include "Misc.h"
#include "System.h"
#include "R5900.h"
#include "Vif.h"
#include "VU.h"
#include "VifDma.h"
#include "Memory.h"
#include "Hw.h"
#include "DebugTools/Debug.h"

#include "ix86/ix86.h"
#include "iR5900.h"

#include "Counters.h"
#include "GS.h"

extern _GSinit            GSinit;
extern _GSopen            GSopen;
extern _GSclose           GSclose;
extern _GSshutdown        GSshutdown;
extern _GSvsync           GSvsync;
extern _GSgifTransfer1    GSgifTransfer1;
extern _GSgifTransfer2    GSgifTransfer2;
extern _GSgifTransfer3    GSgifTransfer3;
extern _GSgetLastTag    GSgetLastTag;
extern _GSgifSoftReset    GSgifSoftReset;
extern _GSreadFIFO        GSreadFIFO;
extern _GSreadFIFO2       GSreadFIFO2;
extern _GSfreeze GSfreeze;

extern _GSkeyEvent        GSkeyEvent;
extern _GSchangeSaveState GSchangeSaveState;
extern _GSmakeSnapshot	   GSmakeSnapshot;
extern _GSmakeSnapshot2   GSmakeSnapshot2;
extern _GSirqCallback 	   GSirqCallback;
extern _GSprintf      	   GSprintf;
extern _GSsetBaseMem 	   GSsetBaseMem;
extern _GSsetGameCRC		GSsetGameCRC;
extern _GSsetFrameSkip 	   GSsetFrameSkip;
extern _GSreset		   GSreset;
extern _GSwriteCSR		   GSwriteCSR;

extern _PADupdate PAD1update, PAD2update;

extern _GSsetupRecording GSsetupRecording;
extern _SPU2setupRecording SPU2setupRecording;

// could convert to pthreads only easily, just don't have the time	
#if defined(_WIN32) && !defined(WIN32_PTHREADS)
HANDLE g_hGsEvent = NULL, // set when path3 is ready to be processed
	g_hVuGSExit = NULL;		// set when thread needs to exit
HANDLE g_hGSOpen = NULL, g_hGSDone = NULL;
HANDLE g_hVuGsThread = NULL;

DWORD WINAPI GSThreadProc(LPVOID lpParam);

#else
pthread_cond_t g_condGsEvent = PTHREAD_COND_INITIALIZER;
sem_t g_semGsThread;
pthread_mutex_t g_mutexGsThread = PTHREAD_MUTEX_INITIALIZER;
int g_nGsThreadExit = 0;
pthread_t g_VuGsThread;

void* GSThreadProc(void* idp);

#endif

int g_FFXHack=0;

static bool gsHasToExit=false;
static LONG g_pGSvSyncCount = 0;

#ifdef PCSX2_DEVBUILD

// GS Playback
int g_SaveGSStream = 0; // save GS stream; 1 - prepare, 2 - save
int g_nLeftGSFrames = 0; // when saving, number of frames left
gzFile g_fGSSave;

// MTGS recording
FILE* g_fMTGSWrite = NULL, *g_fMTGSRead = NULL;
u32 g_MTGSDebug = 0, g_MTGSId = 0;
#endif

u32 CSRw;

extern uptr pDsp;
typedef u8* PU8;

PCSX2_ALIGNED16(u8 g_MTGSMem[0x2000]); // mtgs has to have its own memory

} // extern "C"

#ifdef PCSX2_VIRTUAL_MEM
#define gif ((DMACh*)&PS2MEM_HW[0xA000])
#else
#define gif ((DMACh*)&psH[0xA000])
#endif

#ifdef PCSX2_VIRTUAL_MEM
#define PS2GS_BASE(mem) ((PS2MEM_BASE+0x12000000)+(mem&0x13ff))
#else
u8 g_RealGSMem[0x2000];
#define PS2GS_BASE(mem) (g_RealGSMem+(mem&0x13ff))
#endif

// dummy GS for processing SIGNAL, FINISH, and LABEL commands
typedef struct
{
	u32 nloop : 15;
	u32 eop : 1;
	u32 dummy0 : 16;
	u32 dummy1 : 14;
	u32 pre : 1;
	u32 prim : 11;
	u32 flg : 2;
	u32 nreg : 4;
	u32 regs[2];
	u32 curreg;
} GIFTAG;

static GIFTAG g_path[3];
static PCSX2_ALIGNED16(u8 s_byRegs[3][16]);

// g_pGSRingPos == g_pGSWritePos => fifo is empty
static u8* g_pGSRingPos = NULL, // cur pos ring is at
	*g_pGSWritePos = NULL; // cur pos ee thread is at

CRITICAL_SECTION gsRestartLock;

extern int g_nCounters[];

#ifdef RINGBUF_DEBUG_STACK
#include <list>
std::list<long> ringposStack;
CRITICAL_SECTION stackLock;
#endif

void gsInit()
{
	if( CHECK_MULTIGS ) {

#ifdef _WIN32
        g_pGSRingPos = (u8*)VirtualAlloc(GS_RINGBUFFERBASE, GS_RINGBUFFERSIZE, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
#else
        // setup linux vm
        g_pGSRingPos = (u8*)SysMmap((uptr)GS_RINGBUFFERBASE, GS_RINGBUFFERSIZE);
#endif
        if( g_pGSRingPos != GS_RINGBUFFERBASE ) {
			SysMessage("Cannot alloc GS ring buffer\n");
			exit(0);
		}

		memcpy(g_MTGSMem, PS2MEM_GS, sizeof(g_MTGSMem));

		g_pGSWritePos = GS_RINGBUFFERBASE;
        //InterlockedExchangePointer((volatile PVOID*)&g_pGSWritePos, GS_RINGBUFFERBASE);

        if( GSsetBaseMem != NULL )
			GSsetBaseMem(g_MTGSMem);

//#if defined(_DEBUG) && defined(PCSX2_DEVBUILD)
//		assert( g_fMTGSWrite == NULL && g_fMTGSRead == NULL );
//		g_fMTGSWrite = fopen("mtgswrite.txt", "w");
//		g_fMTGSRead = fopen("mtgsread.txt", "w");
//#endif

        gsHasToExit=false;

#if defined(_WIN32) && !defined(WIN32_PTHREADS)
		g_hGsEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

		g_hVuGSExit = CreateEvent(NULL, FALSE, FALSE, NULL);
		g_hGSOpen = CreateEvent(NULL, FALSE, FALSE, NULL);
		g_hGSDone = CreateEvent(NULL, FALSE, FALSE, NULL);

		InitializeCriticalSection( &gsRestartLock );

#ifdef RINGBUF_DEBUG_STACK
		InitializeCriticalSection( &stackLock );
#endif

		SysPrintf("gsInit\n");

		g_hVuGsThread = CreateThread(NULL, 0, GSThreadProc, NULL, 0, NULL);
#else
        SysPrintf("gsInit\n");
        sem_init(&g_semGsThread, 0, 0);

        pthread_mutex_lock(&g_mutexGsThread);
        if( pthread_create(&g_VuGsThread, NULL, GSThreadProc, NULL) != 0 ) {
            SysMessage("Failed to create gsthread\n");
            exit(0);
        }

        pthread_mutex_lock(&g_mutexGsThread);
        pthread_mutex_unlock(&g_mutexGsThread);
#endif
	}
}

__forceinline void gsWaitGS()
{
    // [Air] : I'm pretty sure there's no harm in doing doing timeslices
	// from the main thread in dual-core mode now, thanks to Sleep(0).

	// fixme - This may not be true under linux, which uses usleep(500), who's
	// behavior likely does not mimic Sleep(0).  Ideally the usleep(500) should
	// be replaced with something that matches Sleep(0) behavior.

	/*if( CHECK_DUALCORE ) {
        while( *(volatile PU8*)&g_pGSRingPos != *(volatile PU8*)&g_pGSWritePos );
    }
    else {*/

	while( *(volatile PU8*)&g_pGSRingPos != *(volatile PU8*)&g_pGSWritePos )
		_TIMESLICE();
}

// Sets the gsEvent flag and releases a timeslice.
// For use in loops that wait on the GS thread to do certain things.
static __forceinline void gsSetEventWait()
{
	if( !CHECK_DUALCORE ) {
		GS_SETEVENT();
		_TIMESLICE();
	}
}

// mem and size are the ones from GSRingBufCopy
void GSRINGBUF_DONECOPY(const u8 *mem, u32 size)
{
	u8* temp = (u8*)(mem) + (size); 

	assert( temp <= GS_RINGBUFFEREND); 

	if( temp == GS_RINGBUFFEREND ) temp = GS_RINGBUFFERBASE; 
	InterlockedExchangePointer((void**)&g_pGSWritePos, temp);
	if( !CHECK_DUALCORE ) GS_SETEVENT();
}

void gsShutdown()
{
	if( CHECK_MULTIGS ) {

        gsHasToExit=true;
		
#if defined(_WIN32) && !defined(WIN32_PTHREADS)
		SetEvent(g_hVuGSExit);
		SysPrintf("Closing gs thread\n");
		WaitForSingleObject(g_hVuGsThread, INFINITE);
		CloseHandle(g_hVuGsThread);
		CloseHandle(g_hGsEvent);
		CloseHandle(g_hVuGSExit);
		CloseHandle(g_hGSOpen);
		CloseHandle(g_hGSDone);

		DeleteCriticalSection(&gsRestartLock);
#ifdef RINGBUF_DEBUG_STACK
		DeleteCriticalSection(&stackLock);
#endif

#else
        InterlockedExchange((long*)&g_nGsThreadExit, 1);
        sem_post(&g_semGsThread);
        pthread_cond_signal(&g_condGsEvent);
        SysPrintf("waiting for thread to terminate\n");
        pthread_join(g_VuGsThread, NULL);

        sem_destroy(&g_semGsThread);

        SysPrintf("thread terminated\n");
#endif
        gsHasToExit=false;

#ifdef _WIN32
		VirtualFree(GS_RINGBUFFERBASE, GS_RINGBUFFERSIZE, MEM_DECOMMIT|MEM_RELEASE);
#else
        SysMunmap((uptr)GS_RINGBUFFERBASE, GS_RINGBUFFERSIZE);
#endif

//#if defined(_DEBUG) && defined(PCSX2_DEVBUILD)
//		if( g_fMTGSWrite != NULL ) {
//			fclose(g_fMTGSWrite);
//			g_fMTGSWrite = NULL;
//		}
//		if( g_fMTGSRead != NULL ) {
//			fclose(g_fMTGSRead);
//			g_fMTGSRead = NULL;
//		}
//#endif
	}
	else
		GSclose();
}


u8* GSRingBufCopy(void* mem, u32 size, u32 type)
{
	// Note on volatiles: g_pGSWritePos is not modified by the GS thread,
	// so there's no need to use volatile reads here.  We still have to use
	// interlocked exchanges when we modify it, however, since the GS thread
	// is reading it.

	u8* writepos = g_pGSWritePos;
	assert( size < GS_RINGBUFFERSIZE );
	assert( writepos < GS_RINGBUFFEREND );
	assert( ((uptr)writepos & 15) == 0 );
	assert( (size&15) == 0);

	size += 16;
	if( writepos + size > GS_RINGBUFFEREND )
	{
		// If the incoming packet doesn't fit, then start over from
		// the start of the ring buffer (it's a lot easier than trying
		// to wrap the packet around the end of the buffer).

		// We have to be careful not to leapfrog our read-position.  If it's 
		// greater than the current write position then we need to stall
		// until it loops around to the beginning of the buffer

		while( *(volatile PU8*)&g_pGSRingPos > writepos )
			gsSetEventWait();

		// Wait for the readpos to go past the start of the buffer
		// Otherwise it'll stop dead in its tracks when we set the new write
		// position below (bad!)
		while( *(volatile PU8*)&g_pGSRingPos == GS_RINGBUFFERBASE)
			gsSetEventWait();

		EnterCriticalSection( &gsRestartLock );
		GSRingBufSimplePacket( GS_RINGTYPE_RESTART, 0, 0, 0 );
		g_pGSWritePos = writepos = GS_RINGBUFFERBASE;
		LeaveCriticalSection( &gsRestartLock );

		// two conditionals in the following while() loop, so precache
		// the readpos for more efficient behavior:
		const u8* readpos = *(volatile PU8*)&g_pGSRingPos;

		// stall until the read position is past the end of our incoming block,
		// or until it reaches the new write position (signals an empty buffer)
		// (the second part should never happen actually, but safe is safe!)
		while( writepos+size >= readpos && readpos != writepos )
		{
			gsSetEventWait();
			readpos = *(volatile PU8*)&g_pGSRingPos;
		}
	}
    else if( writepos + size == GS_RINGBUFFEREND )
	{
		// Yay.  Perfect fit.  What are the odds?
		// ... apparently very slim because the old code just performed the equivalent
		// of a gsWaitGS (stalling the GS until the ring buffer emptied completely) and
		// no one noticed enough to fix it. :)

		//SysPrintf( "MTGS > Perfect Fit!\n");
		while( writepos < *(volatile PU8*)&g_pGSRingPos )
			gsSetEventWait();
    }
	else
	{
		// generic gs wait/stall.
		// Waits until the readpos is outside the scope of the write area.
		while( true )
		{
			// three conditionals in the following while() loop, so precache
			// the readpos for more efficient behavior:
			const u8* readpos = *(volatile PU8*)&g_pGSRingPos;

			if( writepos >= readpos ) break;
			if( writepos+size < readpos ) break;

			gsSetEventWait();
		}
	}
#ifdef RINGBUF_DEBUG_STACK
	EnterCriticalSection( &stackLock );
	ringposStack.push_front( (long)writepos );
	LeaveCriticalSection( &stackLock );
#endif

	// just copy
	*(u32*)writepos = type | (((size-16)>>4)<<16);
	return writepos+16;
}

void GSRingBufSimplePacket(int type, int data0, int data1, int data2)
{
	u8* writepos = g_pGSWritePos;
	u8* future_writepos = writepos+16;

	assert( future_writepos <= GS_RINGBUFFEREND );

    if( future_writepos == GS_RINGBUFFEREND )
        future_writepos = GS_RINGBUFFERBASE;

	while( future_writepos == *(volatile PU8*)&g_pGSRingPos )
		gsSetEventWait();

#ifdef RINGBUF_DEBUG_STACK
	EnterCriticalSection( &stackLock );
	ringposStack.push_front( (long)writepos );
	LeaveCriticalSection( &stackLock );
#endif

	*(u32*)writepos = type;
	*(u32*)(writepos+4) = data0;
	*(u32*)(writepos+8) = data1;
	*(u32*)(writepos+12) = data2;

	assert( future_writepos != *(volatile PU8*)&g_pGSRingPos );
	InterlockedExchangePointer((void**)&g_pGSWritePos, future_writepos);

	if( !CHECK_DUALCORE )
		GS_SETEVENT();
}

void gsReset()
{
	SysPrintf("GIF reset\n");

	// GSDX crashes
	//if( GSreset ) GSreset();

	if( CHECK_MULTIGS ) {

#if defined(_WIN32) && !defined(WIN32_PTHREADS)
		ResetEvent(g_hGsEvent);
		ResetEvent(g_hVuGSExit);
#else
        //TODO
#endif
        gsHasToExit=false;
		g_pGSRingPos = g_pGSWritePos;
		g_pGSvSyncCount = 0;
	}

	memset(g_path, 0, sizeof(g_path));
	memset(s_byRegs, 0, sizeof(s_byRegs));

#ifndef PCSX2_VIRTUAL_MEM
	memset(g_RealGSMem, 0, 0x2000);
#endif

	GSCSRr = 0x551B400F;   // Set the FINISH bit to 1 for now
	GSIMR = 0x7f00;
	psHu32(GIF_STAT) = 0;
	psHu32(GIF_CTRL) = 0;
	psHu32(GIF_MODE) = 0;
}

void gsGIFReset()
{
	memset(g_path, 0, sizeof(g_path));

#ifndef PCSX2_VIRTUAL_MEM
	memset(g_RealGSMem, 0, 0x2000);
#endif

	if( GSgifSoftReset != NULL )
		GSgifSoftReset(7);
	if( CHECK_MULTIGS )
		memset(g_path, 0, sizeof(g_path));

//	else
//		GSreset();

	GSCSRr = 0x551B400F;   // Set the FINISH bit to 1 for now
	GSIMR = 0x7f00;
	psHu32(GIF_STAT) = 0;
	psHu32(GIF_CTRL) = 0;
	psHu32(GIF_MODE) = 0;
}

void CSRwrite(u32 value)
{
	CSRw |= value & ~0x60;
	GSwriteCSR(CSRw);

	GSCSRr = ((GSCSRr&~value)&0x1f)|(GSCSRr&~0x1f);
	if( value & 0x100 ) { // FLUSH
		//SysPrintf("GS_CSR FLUSH GS fifo: %x (CSRr=%x)\n", value, GSCSRr);
	}

	if (value & 0x200) { // resetGS
		//GSCSRr = 0x400E; // The host FIFO needs to be empty too or GSsync will fail (saqib)
		//GSIMR = 0xff00;
		if( GSgifSoftReset != NULL )  {
			GSgifSoftReset(7);
			if( CHECK_MULTIGS ) {
				memset(g_path, 0, sizeof(g_path));
				memset(s_byRegs, 0, sizeof(s_byRegs));
			}
		}
		else GSreset();

		GSCSRr = 0x551B400F;   // Set the FINISH bit to 1 - GS is always at a finish state as we don't have a FIFO(saqib)
	             //Since when!! Refraction, since 4/21/06 (zerofrog) ok ill let you off, looks like theyre all set (ref)
		GSIMR = 0x7F00; //This is bits 14-8 thats all that should be 1

		// and this too (fixed megaman ac)
		//CSRw = (u32)GSCSRr;
		GSwriteCSR(CSRw);
	}
}

static void IMRwrite(u32 value) {
	GSIMR = (value & 0x1f00)|0x6000;

	// don't update mtgs mem
}

void gsWrite8(u32 mem, u8 value) {
	switch (mem) {
		case 0x12001000: // GS_CSR
			CSRwrite((CSRw & ~0x000000ff) | value); break;
		case 0x12001001: // GS_CSR
			CSRwrite((CSRw & ~0x0000ff00) | (value <<  8)); break;
		case 0x12001002: // GS_CSR
			CSRwrite((CSRw & ~0x00ff0000) | (value << 16)); break;
		case 0x12001003: // GS_CSR
			CSRwrite((CSRw & ~0xff000000) | (value << 24)); break;
		default:
			*PS2GS_BASE(mem) = value;

			if( CHECK_MULTIGS ) {
				GSRingBufSimplePacket(GS_RINGTYPE_MEMWRITE8, mem&0x13ff, value, 0);
			}
	}
	GIF_LOG("GS write 8 at %8.8lx with data %8.8lx\n", mem, value);
}

extern void UpdateVSyncRate();

void gsWrite16(u32 mem, u16 value) {
	
	GIF_LOG("GS write 16 at %8.8lx with data %8.8lx\n", mem, value);

	switch (mem) {
		case 0x12000010: // GS_SMODE1
			if((value & 0x6000) == 0x6000) Config.PsxType |= 1; // PAL
			else Config.PsxType &= ~1;	// NTSC

			UpdateVSyncRate();
			break;
			
		case 0x12000020: // GS_SMODE2
			if(value & 0x1) Config.PsxType |= 2; // Interlaced
			else Config.PsxType &= ~2;	// Non-Interlaced

			break;
			
		case 0x12001000: // GS_CSR
			CSRwrite( (CSRw&0xffff0000) | value);
			return; // do not write to MTGS memory
		case 0x12001002: // GS_CSR
			CSRwrite( (CSRw&0xffff) | ((u32)value<<16));
			return; // do not write to MTGS memory
		case 0x12001010: // GS_IMR
			//SysPrintf("writing to IMR 16\n");
			IMRwrite(value);
			return; // do not write to MTGS memory
	}

	*(u16*)PS2GS_BASE(mem) = value;

	if( CHECK_MULTIGS ) {
		GSRingBufSimplePacket(GS_RINGTYPE_MEMWRITE16, mem&0x13ff, value, 0);
	}
}

void gsWrite32(u32 mem, u32 value)
{
	assert( !(mem&3));
	GIF_LOG("GS write 32 at %8.8lx with data %8.8lx\n", mem, value);

	switch (mem) {
		case 0x12000010: // GS_SMODE1
			if((value & 0x6000) == 0x6000) Config.PsxType |= 1; // PAL
			else Config.PsxType &= ~1;	// NTSC
			*(u32*)PS2GS_BASE(mem) = value;
			
			UpdateVSyncRate();

			break;
		case 0x12000020: // GS_SMODE2
			if(value & 0x1) Config.PsxType |= 2; // Interlaced
			else Config.PsxType &= ~2;	// Non-Interlaced

			break;
			
		case 0x12001000: // GS_CSR
			CSRwrite(value);
			return;

		case 0x12001010: // GS_IMR
			IMRwrite(value);
			return;
	}

	*(u32*)PS2GS_BASE(mem) = value;

	if( CHECK_MULTIGS ) {
		GSRingBufSimplePacket(GS_RINGTYPE_MEMWRITE32, mem&0x13ff, value, 0);
	}
}

void gsWrite64(u32 mem, u64 value) {

	GIF_LOG("GS write 64 at %8.8lx with data %8.8lx_%8.8lx\n", mem, ((u32*)&value)[1], (u32)value);

	switch (mem) {
		case 0x12000010: // GS_SMODE1
			if((value & 0x6000) == 0x6000) Config.PsxType |= 1; // PAL
			else Config.PsxType &= ~1;	// NTSC
			UpdateVSyncRate();

			break;

		case 0x12000020: // GS_SMODE2
			if(value & 0x1) Config.PsxType |= 2; // Interlaced
			else Config.PsxType &= ~2;	// Non-Interlaced

			break;
		case 0x12001000: // GS_CSR
			CSRwrite((u32)value);
			return;

		case 0x12001010: // GS_IMR
			IMRwrite((u32)value);
			return;
	}

	*(u64*)PS2GS_BASE(mem) = value;

	if( CHECK_MULTIGS ) {
		GSRingBufSimplePacket(GS_RINGTYPE_MEMWRITE64, mem&0x13ff, (u32)value, (u32)(value>>32));
	}
}

u8 gsRead8(u32 mem)
{
	GIF_LOG("GS read 8 %8.8lx, at %8.8lx\n", *(u8*)PS2GS_BASE(mem), mem);

	return *(u8*)PS2GS_BASE(mem);
}

u16 gsRead16(u32 mem)
{
	GIF_LOG("GS read 16 %8.8lx, at %8.8lx\n", *(u16*)PS2GS_BASE(mem), mem);

	return *(u16*)PS2GS_BASE(mem);
}

u32 gsRead32(u32 mem) 
{
	GIF_LOG("GS read 32 %8.8lx, at %8.8lx\n", *(u32*)PS2GS_BASE(mem), mem);
	return *(u32*)PS2GS_BASE(mem);
}

u64 gsRead64(u32 mem)
{
	GIF_LOG("GS read 64 %8.8lx, at %8.8lx\n", *(u32*)PS2GS_BASE(mem), mem);
	return *(u64*)PS2GS_BASE(mem);
}

void gsIrq() {
	hwIntcIrq(0);
}

static void GSRegHandlerSIGNAL(u32* data)
{
	GIF_LOG("GS SIGNAL data %x_%x CSRw %x\n",data[0], data[1], CSRw);

	GSSIGLBLID->SIGID = (GSSIGLBLID->SIGID&~data[1])|(data[0]&data[1]);
	
	if ((CSRw & 0x1)) 
		GSCSRr |= 1; // signal
		
	
	if (!(GSIMR&0x100) ) 
		gsIrq();
	
	
}

static void GSRegHandlerFINISH(u32* data)
{
	GIF_LOG("GS FINISH data %x_%x CSRw %x\n",data[0], data[1], CSRw);

	if ((CSRw & 0x2)) 
		GSCSRr |= 2; // finish
		
	if (!(GSIMR&0x200) )
			gsIrq();
	
}

static void GSRegHandlerLABEL(u32* data)
{
	GSSIGLBLID->LBLID = (GSSIGLBLID->LBLID&~data[1])|(data[0]&data[1]);
}

typedef void (*GIFRegHandler)(u32* data);
static GIFRegHandler s_GSHandlers[3] = { GSRegHandlerSIGNAL, GSRegHandlerFINISH, GSRegHandlerLABEL };
extern "C" int Path3transfer;

/*midnight madness cares because the tag is 5 dwords*/ \
static __forceinline void TagPathTransfer( GIFTAG* ptag, GIFTAG *path )
{
	u32* psrc = (u32*)ptag;
	u32* pdst = (u32*)path;
	pdst[0] = psrc[0];
	pdst[1] = psrc[1];
	pdst[2] = psrc[2];
	pdst[3] = psrc[3];
}


// simulates a GIF tag
u32 GSgifTransferDummy(int path, u32 *pMem, u32 size)
{
	int nreg, i, nloop;
	u32 curreg;
	u32 tempreg;
	GIFTAG* ptag = &g_path[path];

	if( path == 0 ) {
		nloop = 0;
	}
	else {
		nloop = ptag->nloop;
		curreg = ptag->curreg;
		nreg = ptag->nreg == 0 ? 16 : ptag->nreg;
	}

	while(size > 0)
	{
		if(nloop == 0) 
		{
			ptag = (GIFTAG*)pMem;
			nreg = ptag->nreg == 0 ? 16 : ptag->nreg;

			pMem+= 4;
			size--;

			if( path == 2 && ptag->eop) Path3transfer = 0; //fixes SRS and others

			if( path == 0 ) 
			{ 				
				// if too much data for VU1, just ignore
				if((ptag->nloop * nreg) > (size * (ptag->flg == 1 ? 2 : 1))) {
					g_path[path].nloop = 0;
					return ++size; // have to increment or else the GS plugin will process this packet
				}
			}

			if (ptag->nloop == 0 ) {
				if (path == 0 ) {
					if ((!ptag->eop) && (g_FFXHack))
						continue;
					else
						return size;
				}

				g_path[path].nloop = 0;

				// motogp graphics show
				if (!ptag->eop )
					continue;
				else
					return size;
			}

			tempreg = ptag->regs[0];
			for(i = 0; i < nreg; ++i, tempreg >>= 4) {
					if( i == 8 ) tempreg = ptag->regs[1];
					s_byRegs[path][i] = tempreg&0xf;
			}

			nloop = ptag->nloop;
			curreg = 0;
		}

		switch(ptag->flg)
		{
			case 0: // PACKED
			{
				for(; size > 0; size--, pMem += 4)
				{
					if( s_byRegs[path][curreg] == 0xe  && (pMem[2]&0xff) >= 0x60 ) {
						if( (pMem[2]&0xff) < 0x63 )
							s_GSHandlers[pMem[2]&0x3](pMem);
					}

					curreg++;
					if (nreg == curreg) {
						curreg = 0;
						if( nloop-- <= 1 ) {
							size--;
							pMem += 4;
							break;
						}
					}
				}

				if( nloop > 0 ) {
					assert(size == 0);
					TagPathTransfer( ptag, &g_path[path] );
					g_path[path].nloop = nloop;
					g_path[path].curreg = curreg;
					return 0;
				}
				break;
			}
			case 1: // REGLIST
			{
				size *= 2;
	
				tempreg = ptag->regs[0];
				for(i = 0; i < nreg; ++i, tempreg >>= 4) {
					if( i == 8 ) tempreg = ptag->regs[1];
					assert( (tempreg&0xf) < 0x64 );
					s_byRegs[path][i] = tempreg&0xf;
				}

				for(; size > 0; pMem+= 2, size--) {
					if( s_byRegs[path][curreg] >= 0x60 && s_byRegs[path][curreg] < 0x63 )
						s_GSHandlers[s_byRegs[path][curreg]&3](pMem);

					curreg++;
					if (nreg == curreg) {
						curreg = 0;
						if( nloop-- <= 1 ) {
							size--;
							pMem += 2;
							break;
						}
					}
				}

				if( size & 1 ) pMem += 2;
				size /= 2;

				if( nloop > 0 ) {
					assert(size == 0);
					TagPathTransfer( ptag, &g_path[path] );
					g_path[path].nloop = nloop;
					g_path[path].curreg = curreg;
					return 0;
				}

				break;
			}
			case 2: // GIF_IMAGE (FROM_VFRAM)
			case 3:
			{
				// simulate
				if( (int)size < nloop ) {
					TagPathTransfer( ptag, &g_path[path] );
					g_path[path].nloop = nloop-size;
					return 0;
				}
				else {
					pMem += nloop*4;
					size -= nloop;
					nloop = 0;
				}
				break;
			}
		}
		
		if( path == 0 && ptag->eop ) {
			g_path[0].nloop = 0;
			return size;
		}
	}
	
	g_path[path] = *ptag;
	g_path[path].curreg = curreg;
	g_path[path].nloop = nloop;
	return size;
}
static int gspath3done=0;
int gscycles = 0;

__forceinline void gsInterrupt() {
	GIF_LOG("gsInterrupt: %8.8x\n", cpuRegs.cycle);

	if((gif->chcr & 0x100) == 0){
		//SysPrintf("Eh? why are you still interrupting! chcr %x, qwc %x, done = %x\n", gif->chcr, gif->qwc, done);
		cpuRegs.interrupt &= ~(1 << 2);
		return;
	}
	if(gif->qwc > 0 || gspath3done == 0) {
		if( !(psHu32(DMAC_CTRL) & 0x1) ) {
			SysPrintf("gs dma masked\n");
			return;
		}

		GIFdma();
#ifdef GSPATH3FIX
		if ((vif1Regs->mskpath3 && (vif1ch->chcr & 0x100)) || (psHu32(GIF_MODE) & 0x1)) cpuRegs.interrupt &= ~(1 << 2);
#endif
			return;
	}
	
	gspath3done = 0;
	gscycles = 0;
	Path3transfer = 0;
	gif->chcr &= ~0x100;
	GSCSRr &= ~0xC000; //Clear FIFO stuff
	GSCSRr |= 0x4000;  //FIFO empty
	//psHu32(GIF_MODE)&= ~0x4;
	psHu32(GIF_STAT)&= ~0xE00; // OPH=0 | APATH=0
	psHu32(GIF_STAT)&= ~0x1F000000; // QFC=0
	hwDmacIrq(DMAC_GIF);

	cpuRegs.interrupt &= ~(1 << 2);
}

static u64 s_gstag=0; // used for querying the last tag

static __forceinline void WRITERING_DMA(u32 *pMem, u32 qwc) { 
	psHu32(GIF_STAT) |= 0xE00;         
	Path3transfer = 1; 
	if( CHECK_MULTIGS) { 
		u8* pgsmem = GSRingBufCopy(pMem, (qwc)<<4, GS_RINGTYPE_P3); 
		if( pgsmem != NULL ) { 
			int sizetoread = (qwc)<<4; 
			u32 pendmem = (u32)gif->madr + sizetoread; 
			/* check if page of endmem is valid (dark cloud2) */ 
			if( dmaGetAddr(pendmem-16) == NULL ) { 
				pendmem = ((pendmem-16)&~0xfff)-16; 
				while(dmaGetAddr(pendmem) == NULL) { 
					pendmem = (pendmem&~0xfff)-16; 
				} 
				memcpy_fast(pgsmem, pMem, pendmem-(u32)gif->madr+16); 
			} 
			else memcpy_fast(pgsmem, pMem, sizetoread); 
			
			GSgifTransferDummy(2, pMem, qwc); 
			GSRINGBUF_DONECOPY(pgsmem, sizetoread);
		} 
	} 
	else { 
		GSGIFTRANSFER3(pMem, qwc); 
        if( GSgetLastTag != NULL ) { 
            GSgetLastTag(&s_gstag); 
            if( (s_gstag) == 1 ) {        
                Path3transfer = 0; /* fixes SRS and others */ 
            } 
        } 
	} 
} 

int  _GIFchain() {
#ifdef GSPATH3FIX
	u32 qwc = (psHu32(GIF_MODE) & 0x4 && vif1Regs->mskpath3) ? min(8, (int)gif->qwc) : gif->qwc;
#else
	u32 qwc = gif->qwc;
#endif
	u32 *pMem;

	//if (gif->qwc == 0) return 0;

	pMem = (u32*)dmaGetAddr(gif->madr);
	if (pMem == NULL) {
		// reset path3, fixes dark cloud 2
		if( GSgifSoftReset != NULL )
			GSgifSoftReset(4);
		if( CHECK_MULTIGS ) {
			memset(&g_path[2], 0, sizeof(g_path[2]));
		}

		//must increment madr and clear qwc, else it loops
		gif->madr+= gif->qwc*16;
		gif->qwc = 0;
		SysPrintf("NULL GIFchain\n");
		return -1;
	}
	WRITERING_DMA(pMem, qwc);
	
	//if((psHu32(GIF_MODE) & 0x4)) amount -= qwc;
	gif->madr+= qwc*16;
	gif->qwc -= qwc;
	return (qwc)*2;
}

#define GIFchain() \
	if (gif->qwc) { \
		gscycles+= _GIFchain(); /* guessing */ \
	}

int gscount = 0;
static int prevcycles = 0;
static u32* prevtag = NULL;

void GIFdma() 
{
	u32 *ptag;
	u32 id;

	gscycles= prevcycles ? prevcycles: gscycles;

	if( (psHu32(GIF_CTRL) & 8) ) { // temporarily stop
		SysPrintf("Gif dma temp paused?\n");
		return;
	}

	GIF_LOG("dmaGIFstart chcr = %lx, madr = %lx, qwc  = %lx\n tadr = %lx, asr0 = %lx, asr1 = %lx\n", gif->chcr, gif->madr, gif->qwc, gif->tadr, gif->asr0, gif->asr1);

#ifndef GSPATH3FIX
	if ( !(psHu32(GIF_MODE) & 0x4) ) {
		if (vif1Regs->mskpath3 || psHu32(GIF_MODE) & 0x1) {
			gif->chcr &= ~0x100;
			psHu32(GIF_STAT)&= ~0xE00; // OPH=0 | APATH=0
			hwDmacIrq(2);
			return;
		}
	}
#endif

	if ((psHu32(DMAC_CTRL) & 0xC0) == 0x80 && prevcycles != 0) { // STD == GIF
		SysPrintf("GS Stall Control Source = %x, Drain = %x\n MADR = %x, STADR = %x", (psHu32(0xe000) >> 4) & 0x3, (psHu32(0xe000) >> 6) & 0x3, gif->madr, psHu32(DMAC_STADR));

		if( gif->madr + (gif->qwc * 16) > psHu32(DMAC_STADR) ) {
			CPU_INT(2, gscycles);
			gscycles = 0;
			return;
		}
		prevcycles = 0;
		gif->qwc = 0;
	}

	GSCSRr &= ~0xC000;  //Clear FIFO stuff
	GSCSRr |= 0x8000;   //FIFO full
	//psHu32(GIF_STAT)|= 0xE00; // OPH=1 | APATH=3
	psHu32(GIF_STAT)|= 0x10000000; // FQC=31, hack ;)

#ifdef GSPATH3FIX
	if (vif1Regs->mskpath3 || psHu32(GIF_MODE) & 0x1) {
		if(gif->qwc == 0) {
			if((gif->chcr & 0x10e) == 0x104) {
				ptag = (u32*)dmaGetAddr(gif->tadr);  //Set memory pointer to TADR

				if (ptag == NULL) {					 //Is ptag empty?
					psHu32(DMAC_STAT)|= 1<<15;		 //If yes, set BEIS (BUSERR) in DMAC_STAT register 
					return;
				}	
				gscycles += 2;
				gif->chcr = ( gif->chcr & 0xFFFF ) | ( (*ptag) & 0xFFFF0000 );  //Transfer upper part of tag to CHCR bits 31-15
				id        = (ptag[0] >> 28) & 0x7;		//ID for DmaChain copied from bit 28 of the tag
				gif->qwc  = (u16)ptag[0];			    //QWC set to lower 16bits of the tag
				gif->madr = ptag[1];				    //MADR = ADDR field	
				gspath3done = hwDmacSrcChainWithStack(gif, id);
				GIF_LOG("PTH3 MASK gifdmaChain %8.8x_%8.8x size=%d, id=%d, addr=%lx\n", ptag[1], ptag[0], gif->qwc, id, gif->madr);

				if ((gif->chcr & 0x80) && ptag[0] >> 31) {			 //Check TIE bit of CHCR and IRQ bit of tag
					GIF_LOG("PATH3 MSK dmaIrq Set\n");
					SysPrintf("GIF TIE\n");
					gspath3done |= 1;
				}
			}
		}

		FreezeXMMRegs(1); 
		FreezeMMXRegs(1);
		GIFchain(); 
		FreezeXMMRegs(0); // Theres a comment below that says not to unfreeze the xmm regs, so not sure about this.
		FreezeMMXRegs(0);

		if((gspath3done == 1 || (gif->chcr & 0xc) == 0) && gif->qwc == 0){ 
			if(gif->qwc > 0) SysPrintf("Horray\n");
			gspath3done = 0;
			gif->chcr &= ~0x100;
			//psHu32(GIF_MODE)&= ~0x4;
			GSCSRr &= ~0xC000;
			GSCSRr |= 0x4000;  
			Path3transfer = 0;
			psHu32(GIF_STAT)&= ~0x1F000E00; // OPH=0 | APATH=0 | QFC=0
			hwDmacIrq(DMAC_GIF);
		}
		//Dont unfreeze xmm regs here, Masked PATH3 can only be called by VIF, which is already handling it.
		return;
	}
#endif
	//gscycles = 0;
	// Transfer Dn_QWC from Dn_MADR to GIF
	if ((gif->chcr & 0xc) == 0 || gif->qwc > 0) { // Normal Mode
		//gscount++;
		if ((psHu32(DMAC_CTRL) & 0xC0) == 0x80 && (gif->chcr & 0xc) == 0) { 
			SysPrintf("DMA Stall Control on GIF normal\n");
		}
		FreezeXMMRegs(1);  
		FreezeMMXRegs(1);  
		GIFchain();	//Transfers the data set by the switch
		FreezeXMMRegs(0); 
		FreezeMMXRegs(0);	 
		if(gif->qwc == 0 && (gif->chcr & 0xc) == 0) gspath3done = 1;
	}
	else {
		// Chain Mode
		while (gspath3done == 0 && gif->qwc == 0) {		//Loop if the transfers aren't intermittent
			ptag = (u32*)dmaGetAddr(gif->tadr);  //Set memory pointer to TADR
			if (ptag == NULL) {					 //Is ptag empty?
				psHu32(DMAC_STAT)|= 1<<15;		 //If yes, set BEIS (BUSERR) in DMAC_STAT register
				return;
			}
			gscycles+=2; // Add 1 cycles from the QW read for the tag

			// Transfer dma tag if tte is set
			if (gif->chcr & 0x40) {
				//u32 temptag[4] = {0};
				//SysPrintf("GIF TTE: %x_%x\n", ptag[3], ptag[2]);

				//temptag[0] = ptag[2];
				//temptag[1] = ptag[3];
				//GSGIFTRANSFER3(ptag, 1); 
			}

			gif->chcr = ( gif->chcr & 0xFFFF ) | ( (*ptag) & 0xFFFF0000 );  //Transfer upper part of tag to CHCR bits 31-15
		
			id        = (ptag[0] >> 28) & 0x7;		//ID for DmaChain copied from bit 28 of the tag
			gif->qwc  = (u16)ptag[0];			    //QWC set to lower 16bits of the tag
			gif->madr = ptag[1];				    //MADR = ADDR field
			
			gspath3done = hwDmacSrcChainWithStack(gif, id);
			GIF_LOG("gifdmaChain %8.8x_%8.8x size=%d, id=%d, addr=%lx\n", ptag[1], ptag[0], gif->qwc, id, gif->madr);

			if ((psHu32(DMAC_CTRL) & 0xC0) == 0x80) { // STD == GIF
				// there are still bugs, need to also check if gif->madr +16*qwc >= stadr, if not, stall
				if(!gspath3done && gif->madr + (gif->qwc * 16) > psHu32(DMAC_STADR) && id == 4) {
					// stalled
					SysPrintf("GS Stall Control Source = %x, Drain = %x\n MADR = %x, STADR = %x", (psHu32(0xe000) >> 4) & 0x3, (psHu32(0xe000) >> 6) & 0x3,gif->madr, psHu32(DMAC_STADR));
					prevcycles = gscycles;
					gif->tadr -= 16;
					hwDmacIrq(13);
					CPU_INT(2, gscycles);
					gscycles = 0;
					return;
				}
			}

			FreezeXMMRegs(1);  
			FreezeMMXRegs(1);  
			GIFchain();	//Transfers the data set by the switch
			FreezeXMMRegs(0); 
			FreezeMMXRegs(0); 

			if ((gif->chcr & 0x80) && ptag[0] >> 31) { //Check TIE bit of CHCR and IRQ bit of tag
				GIF_LOG("dmaIrq Set\n");
				gspath3done = 1;
				//gif->qwc = 0;
			}
		}
	}
	prevtag = NULL;
	prevcycles = 0;
	if (!(vif1Regs->mskpath3 || (psHu32(GIF_MODE) & 0x1)))	{
		CPU_INT(2, gscycles);
		gscycles = 0;
	}
}

void dmaGIF() {
	//if(vif1Regs->mskpath3 || (psHu32(GIF_MODE) & 0x1)){
	//	CPU_INT(2, 48); //Wait time for the buffer to fill, fixes some timing problems in path 3 masking
	//}				//It takes the time of 24 QW for the BUS to become ready - The Punisher, And1 Streetball
	//else
	gspath3done = 0; // For some reason this doesnt clear? So when the system starts the thread, we will clear it :)

	if(gif->qwc > 0 && (gif->chcr & 0x4) == 0x4)
        gspath3done = 1; //Halflife sets a QWC amount in chain mode, no tadr set.

	if ((psHu32(DMAC_CTRL) & 0xC) == 0xC ) { // GIF MFIFO
		//SysPrintf("GIF MFIFO\n");
		gifMFIFOInterrupt();
		return;
	}

	GIFdma();
}

#define spr0 ((DMACh*)&PS2MEM_HW[0xD000])

static unsigned int mfifocycles;
static unsigned int gifqwc = 0;
static unsigned int gifdone = 0;

int mfifoGIFrbTransfer() {
	u32 qwc = (psHu32(GIF_MODE) & 0x4 && vif1Regs->mskpath3) ? min(8, (int)gif->qwc) : gif->qwc;
	int mfifoqwc = min(gifqwc, qwc);
	u32 *src;


	/* Check if the transfer should wrap around the ring buffer */
	if ((gif->madr+mfifoqwc*16) > (psHu32(DMAC_RBOR) + psHu32(DMAC_RBSR)+16)) {
		int s1 = ((psHu32(DMAC_RBOR) + psHu32(DMAC_RBSR)+16) - gif->madr) >> 4;
		
		/* it does, so first copy 's1' bytes from 'addr' to 'data' */
		src = (u32*)PSM(gif->madr);
		if (src == NULL) return -1;
		WRITERING_DMA(src, s1);

		/* and second copy 's2' bytes from 'maddr' to '&data[s1]' */
		src = (u32*)PSM(psHu32(DMAC_RBOR));
		if (src == NULL) return -1;
		WRITERING_DMA(src, (mfifoqwc - s1));
		
	} else {
		/* it doesn't, so just transfer 'qwc*16' words 
		   from 'gif->madr' to GS */
		src = (u32*)PSM(gif->madr);
		if (src == NULL) return -1;
		
		WRITERING_DMA(src, mfifoqwc);
		gif->madr = psHu32(DMAC_RBOR) + (gif->madr & psHu32(DMAC_RBSR));
	}

	gifqwc -= mfifoqwc;
	gif->qwc -= mfifoqwc;
	gif->madr+= mfifoqwc*16;
	mfifocycles+= (mfifoqwc) * 2; /* guessing */
	

	return 0;
}


int mfifoGIFchain() {	 
	/* Is QWC = 0? if so there is nothing to transfer */

	if (gif->qwc == 0) return 0;
	
	if (gif->madr >= psHu32(DMAC_RBOR) &&
		gif->madr <= (psHu32(DMAC_RBOR)+psHu32(DMAC_RBSR))) {
		if (mfifoGIFrbTransfer() == -1) return -1;
	} else {
		int mfifoqwc = (psHu32(GIF_MODE) & 0x4 && vif1Regs->mskpath3) ? min(8, (int)gif->qwc) : gif->qwc;
		u32 *pMem = (u32*)dmaGetAddr(gif->madr);
		if (pMem == NULL) return -1;

		WRITERING_DMA(pMem, mfifoqwc);
		gif->madr+= mfifoqwc*16;
		gif->qwc -= mfifoqwc;
		mfifocycles+= (mfifoqwc) * 2; /* guessing */
	}

	return 0;
}



void mfifoGIFtransfer(int qwc) {
	u32 *ptag;
	int id;
	u32 temp = 0;
	mfifocycles = 0;
	
	if(qwc > 0 ) {
				gifqwc += qwc;
				if(!(gif->chcr & 0x100))return;
			}
	SPR_LOG("mfifoGIFtransfer %x madr %x, tadr %x\n", gif->chcr, gif->madr, gif->tadr);
		
	if(gif->qwc == 0){
			if(gif->tadr == spr0->madr) {
	#ifdef PCSX2_DEVBUILD
				/*if( gifqwc > 1 )
					SysPrintf("gif mfifo tadr==madr but qwc = %d\n", gifqwc);*/
	#endif
				//hwDmacIrq(14);

				return;
			}
			gif->tadr = psHu32(DMAC_RBOR) + (gif->tadr & psHu32(DMAC_RBSR));
			ptag = (u32*)dmaGetAddr(gif->tadr);
			
			id        = (ptag[0] >> 28) & 0x7;
			gif->qwc  = (ptag[0] & 0xffff);
			gif->madr = ptag[1];
			mfifocycles += 2;
			
			gif->chcr = ( gif->chcr & 0xFFFF ) | ( (*ptag) & 0xFFFF0000 );
			SPR_LOG("dmaChain %8.8x_%8.8x size=%d, id=%d, madr=%lx, tadr=%lx mfifo qwc = %x spr0 madr = %x\n",
					ptag[1], ptag[0], gif->qwc, id, gif->madr, gif->tadr, gifqwc, spr0->madr);

			gifqwc--;
			switch (id) {
				case 0: // Refe - Transfer Packet According to ADDR field
					gif->tadr = psHu32(DMAC_RBOR) + ((gif->tadr + 16) & psHu32(DMAC_RBSR));
					gifdone = 2;										//End Transfer
					break;

				case 1: // CNT - Transfer QWC following the tag.
					gif->madr = psHu32(DMAC_RBOR) + ((gif->tadr + 16) & psHu32(DMAC_RBSR));						//Set MADR to QW after Tag            
					gif->tadr = psHu32(DMAC_RBOR) + ((gif->madr + (gif->qwc << 4)) & psHu32(DMAC_RBSR));			//Set TADR to QW following the data
					gifdone = 0;
					break;

				case 2: // Next - Transfer QWC following tag. TADR = ADDR
					temp = gif->madr;								//Temporarily Store ADDR
					gif->madr = psHu32(DMAC_RBOR) + ((gif->tadr + 16) & psHu32(DMAC_RBSR)); 					  //Set MADR to QW following the tag
					gif->tadr = temp;								//Copy temporarily stored ADDR to Tag
					gifdone = 0;
					break;

				case 3: // Ref - Transfer QWC from ADDR field
				case 4: // Refs - Transfer QWC from ADDR field (Stall Control) 
					gif->tadr = psHu32(DMAC_RBOR) + ((gif->tadr + 16) & psHu32(DMAC_RBSR));							//Set TADR to next tag
					gifdone = 0;
					break;

				case 7: // End - Transfer QWC following the tag
					gif->madr = psHu32(DMAC_RBOR) + ((gif->tadr + 16) & psHu32(DMAC_RBSR));		//Set MADR to data following the tag
					gif->tadr = psHu32(DMAC_RBOR) + ((gif->madr + (gif->qwc << 4)) & psHu32(DMAC_RBSR));			//Set TADR to QW following the data
					gifdone = 2;										//End Transfer
					break;
				}
				if ((gif->chcr & 0x80) && (ptag[0] >> 31)) {
				SPR_LOG("dmaIrq Set\n");
				gifdone = 2;
			}
	 }
	FreezeXMMRegs(1); 
	FreezeMMXRegs(1);
		if (mfifoGIFchain() == -1) {
			SysPrintf("GIF dmaChain error size=%d, madr=%lx, tadr=%lx\n",
					gif->qwc, gif->madr, gif->tadr);
			gifdone = 1;
		}
	FreezeXMMRegs(0); 
	FreezeMMXRegs(0);
		
	if(gif->qwc == 0 && gifdone == 2) gifdone = 1;
	CPU_INT(11,mfifocycles);
		
	SPR_LOG("mfifoGIFtransfer end %x madr %x, tadr %x\n", gif->chcr, gif->madr, gif->tadr);	
}

void gifMFIFOInterrupt()
{
	
	
	if(!(gif->chcr & 0x100)) { SysPrintf("WTF GIFMFIFO\n");cpuRegs.interrupt &= ~(1 << 11); return ; }
	
	if(gifdone != 1) {
		if(gifqwc <= 0) {
		//SysPrintf("Empty\n");
			psHu32(GIF_STAT)&= ~0xE00; // OPH=0 | APATH=0
			hwDmacIrq(14);
			cpuRegs.interrupt &= ~(1 << 11); 
			return;
		}
		mfifoGIFtransfer(0);
		return;
	}
#ifndef PCSX2_PUBLIC
	if(gifdone == 0 || gif->qwc > 0) {
		SysPrintf("gifMFIFO Panic > Shouldnt go here!\n");
		cpuRegs.interrupt &= ~(1 << 11);
		return;
	}
#endif
	//if(gifqwc > 0)SysPrintf("GIF MFIFO ending with stuff in it %x\n", gifqwc);
	gifqwc = 0;
	gifdone = 0;
	gif->chcr &= ~0x100;
	hwDmacIrq(DMAC_GIF);
	GSCSRr &= ~0xC000; //Clear FIFO stuff
	GSCSRr |= 0x4000;  //FIFO empty
	//psHu32(GIF_MODE)&= ~0x4;
	psHu32(GIF_STAT)&= ~0xE00; // OPH=0 | APATH=0
	psHu32(GIF_STAT)&= ~0x1F000000; // QFC=0
	cpuRegs.interrupt &= ~(1 << 11);
}

int HasToExit()
{
	return (gsHasToExit!=0);
}

extern "C" void GSPostVsyncEnd()
{
	*(u32*)(PS2MEM_GS+0x1000) ^= 0x2000; // swap the vsync field

	if( CHECK_MULTIGS ) 
	{
		//while( *(volatile LONG*)&g_pGSvSyncCount >= 8 )
		//	gsSetEventWait();

		//InterlockedIncrement( (volatile LONG*)&g_pGSvSyncCount );
		//SysPrintf( " Sending VSync : %d \n", *(volatile LONG*)&g_pGSvSyncCount );
		GSRingBufSimplePacket(GS_RINGTYPE_VSYNC, (*(u32*)(PS2MEM_GS+0x1000)&0x2000), 0, 0);
	}
	else {
		GSvsync((*(u32*)(PS2MEM_GS+0x1000)&0x2000));
        // update here on single thread mode *OBSOLETE*
        if( PAD1update != NULL ) PAD1update(0);
        if( PAD2update != NULL ) PAD2update(1);
	}
}

#if defined(_WIN32) && !defined(WIN32_PTHREADS)
DWORD WINAPI GSThreadProc(LPVOID lpParam)
{
	u32 prevCmd=0;
	HANDLE handles[2] = { g_hGsEvent, g_hVuGSExit };
	//SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

	{
		int ret;
		HANDLE openhandles[2] = { g_hGSOpen, g_hVuGSExit };
		if( WaitForMultipleObjects(2, openhandles, FALSE, INFINITE) == WAIT_OBJECT_0+1 ) {
			return 0;
		}
		ret = GSopen((void *)&pDsp, "PCSX2", 1);
		GSCSRr = 0x551B400F; // 0x55190000
		SysPrintf("gsOpen done\n");
		if (ret != 0) { SysMessage ("Error Opening GS Plugin"); return (DWORD)-1; }
		SetEvent(g_hGSDone);
	}
#else
void* GSThreadProc(void* lpParam)
{
    // g_mutexGSThread already locked
    SysPrintf("waiting for gsOpen\n");
    pthread_cond_wait(&g_condGsEvent, &g_mutexGsThread);
    pthread_mutex_unlock(&g_mutexGsThread);
    pthread_testcancel();
    if( g_nGsThreadExit )
        return NULL;

    int ret = GSopen((void *)&pDsp, "PCSX2", 1);
	GSCSRr = 0x551B400F; // 0x55190000
	SysPrintf("gsOpen done\n");
	if (ret != 0) {
        SysMessage ("Error Opening GS Plugin");
        return NULL;
    }

    sem_post(&g_semGsThread);
    pthread_mutex_unlock(&g_mutexGsThread);
#endif

	SysPrintf("Starting GS thread\n");
	u32 counter = 0;

	while(!gsHasToExit) {

#if defined(_WIN32) && !defined(WIN32_PTHREADS)
		if( !CHECK_DUALCORE ) 
		{
			if( WaitForMultipleObjects(2, handles, FALSE, INFINITE) == WAIT_OBJECT_0+1 ) 
			{
				break; //exit thread and close gs
			}
		}
#else
        if( !CHECK_DUALCORE ) {
            sem_wait(&g_semGsThread);
            if( g_nGsThreadExit ) {
                GSclose();
                return 0;
            }
		}
		else if( !(counter++ & 0xffff) ) {
			if( g_nGsThreadExit ) {
                GSclose();
                return 0;
            }
		}
#endif

		// note: gsRingPos is intentionally not volatile, because it should only
		// ever be modified by this thread.
		while( g_pGSRingPos != *(volatile PU8*)&g_pGSWritePos)
		{
			assert( g_pGSRingPos < GS_RINGBUFFEREND );

			u32 tag = *(u32*)g_pGSRingPos;
			u32 ringposinc = 16;

#ifdef RINGBUF_DEBUG_STACK
			// pop a ringpos off the stack.  It should match this one!

			EnterCriticalSection( &stackLock );
			long stackpos = ringposStack.back();
			assert( stackpos == (long)g_pGSRingPos );
			if( stackpos != (long)g_pGSRingPos )
			{
				SysPrintf( "Holy Fuck ---------------> %x to %x\n", stackpos, (long)g_pGSRingPos );
				SysPrintf( "    Prev Command : %x\n", prevCmd );
			}
			prevCmd = tag;
			ringposStack.pop_back();
			LeaveCriticalSection( &stackLock );
#endif

			switch( tag&0xffff )
			{
				case GS_RINGTYPE_RESTART:
					InterlockedExchangePointer((volatile PVOID*)&g_pGSRingPos, GS_RINGBUFFERBASE);
					
					// stall for a bit to let the MainThread have time to update the g_pGSWritePos. 
					EnterCriticalSection( &gsRestartLock );
					LeaveCriticalSection( &gsRestartLock );
					continue;

				case GS_RINGTYPE_P1:
                {
                    int qsize = (tag>>16);
//							MTGS_RECREAD(g_pGSRingPos+16, (qsize<<4));
                    // make sure that tag>>16 is the MAX size readable
					GSgifTransfer1((u32*)(g_pGSRingPos+16) - 0x1000 + 4*qsize, 0x4000-qsize*16);
					ringposinc += qsize<<4;
					break;
                }
				case GS_RINGTYPE_P2:
//							MTGS_RECREAD(g_pGSRingPos+16, ((tag>>16)<<4));
					GSgifTransfer2((u32*)(g_pGSRingPos+16), tag>>16);
					ringposinc += (tag>>16)<<4;
					break;
				case GS_RINGTYPE_P3:
//							MTGS_RECREAD(g_pGSRingPos+16, ((tag>>16)<<4));
					GSgifTransfer3((u32*)(g_pGSRingPos+16), tag>>16);
					ringposinc += (tag>>16)<<4;
					break;
				case GS_RINGTYPE_VSYNC:
					GSvsync(*(int*)(g_pGSRingPos+4));
                    if( PAD1update != NULL ) PAD1update(0);
                    if( PAD2update != NULL ) PAD2update(1);

					//SysPrintf( " Receiving VSync : %d \n", *(volatile LONG*)&g_pGSvSyncCount );
					//InterlockedDecrement( (volatile LONG*)&g_pGSvSyncCount );

					// vSyncCount should never dip below zero.
					assert( *(volatile LONG*)&g_pGSvSyncCount >= 0 );
					break;

				case GS_RINGTYPE_FRAMESKIP:
					if( GSsetFrameSkip != NULL )
						GSsetFrameSkip(*(int*)(g_pGSRingPos+4));
					break;
				case GS_RINGTYPE_MEMWRITE8:
					g_MTGSMem[*(int*)(g_pGSRingPos+4)] = *(u8*)(g_pGSRingPos+8);
					break;
				case GS_RINGTYPE_MEMWRITE16:
					*(u16*)(g_MTGSMem+*(int*)(g_pGSRingPos+4)) = *(u16*)(g_pGSRingPos+8);
					break;
				case GS_RINGTYPE_MEMWRITE32:
					*(u32*)(g_MTGSMem+*(int*)(g_pGSRingPos+4)) = *(u32*)(g_pGSRingPos+8);
					break;
				case GS_RINGTYPE_MEMWRITE64:
					*(u64*)(g_MTGSMem+*(int*)(g_pGSRingPos+4)) = *(u64*)(g_pGSRingPos+8);
					break;

				case GS_RINGTYPE_VIFFIFO:
				{
					u64* pMem;
					assert( vif1ch->madr == *(u32*)(g_pGSRingPos+4) );
					assert( vif1ch->qwc == (tag>>16) );

					assert( vif1ch->madr == *(u32*)(g_pGSRingPos+4) );
					pMem = (u64*)dmaGetAddr(vif1ch->madr);

					if (pMem == NULL) {
						psHu32(DMAC_STAT)|= 1<<15;
						continue;	// don't increment gsRingPos
					}

					if( GSreadFIFO2 == NULL ) {
						int size;
						for (size=(tag>>16); size>0; size--) {
							GSreadFIFO((u64*)&PS2MEM_HW[0x5000]);
							pMem[0] = psHu64(0x5000);
							pMem[1] = psHu64(0x5008); pMem+= 2;
						}
					}
					else {
						GSreadFIFO2(pMem, tag>>16); 
						
						// set incase read
						psHu64(0x5000) = pMem[2*(tag>>16)-2];
						psHu64(0x5008) = pMem[2*(tag>>16)-1];
					}

					assert( vif1ch->madr == *(u32*)(g_pGSRingPos+4) );
					assert( vif1ch->qwc == (tag>>16) );

//							tag = (tag>>16) + (cpuRegs.cycle- *(u32*)(g_pGSRingPos+8));
//							if( tag & 0x80000000 ) tag = 0;
					vif1ch->madr += vif1ch->qwc * 16;
					if(vif1Regs->mskpath3 == 0)vif1Regs->stat&= ~0x1f000000;

					// fixme : calling CPU_INT could create a race condition if it runs parallel
					// with the EE/IOP code also calling CPU_INT.

					CPU_INT(1, 0); // since gs thread always lags real thread
                    vif1ch->qwc = 0;

					break;
				}

                case GS_RINGTYPE_SAVE:
                {
                    gzFile f = *(gzFile*)(g_pGSRingPos+4);
                    freezeData fP;

                    if (GSfreeze(FREEZE_SIZE, &fP) == -1) {
                        gzclose(f);
                        break;
                    }
                    fP.data = (s8*)malloc(fP.size);
                    if (fP.data == NULL) {
                        break;
                    }
                    
                    if (GSfreeze(FREEZE_SAVE, &fP) == -1) {
                        gzclose(f);
                        break;
                    }
                    
                    gzwrite(f, &fP.size, sizeof(fP.size));
                    if (fP.size) {
                        gzwrite(f, fP.data, fP.size);
                        free(fP.data);
                    }
                    break;
                }
                case GS_RINGTYPE_LOAD:
                {
                    gzFile f = *(gzFile*)(g_pGSRingPos+4);
                    freezeData fP;

                    gzread(f, &fP.size, sizeof(fP.size));
                    if (fP.size) {
                        fP.data = (s8*)malloc(fP.size);
                        if (fP.data == NULL)
                            break;

                        gzread(f, fP.data, fP.size);
                    }
                    if (GSfreeze(FREEZE_LOAD, &fP) == -1) {
                        // failed
                    }
                    if (fP.size)
                        free(fP.data);

                    break;
                }
                case GS_RINGTYPE_RECORD:
                {
                    int record = *(int*)(g_pGSRingPos+4);
                    if( GSsetupRecording != NULL ) GSsetupRecording(record, NULL);
                    if( SPU2setupRecording != NULL ) SPU2setupRecording(record, NULL);
                    break;
                }
				default:

					SysPrintf("GSThreadProc, bad packet (%x) at g_pGSRingPos: %x, g_pGSWritePos: %x\n", tag, g_pGSRingPos, g_pGSWritePos);
					assert(0);
					g_pGSRingPos = g_pGSWritePos;
					continue;
					//flushall();
			}

			InterlockedExchangeAdd( (long*)&g_pGSRingPos, ringposinc );

			assert( g_pGSRingPos <= GS_RINGBUFFEREND );
#ifdef _WIN32
			InterlockedCompareExchangePointer( (volatile PVOID*)&g_pGSRingPos, GS_RINGBUFFERBASE, GS_RINGBUFFEREND );
#else
			// fixme - [TODO] - InterlockedCompareExchangePointer needs a linux implementation!
			if( g_pGSRingPos == GS_RINGBUFFEREND )
				InterlockedExchangePointer((volatile PVOID*)&g_pGSRingPos, GS_RINGBUFFERBASE);
#endif
		}

		// buffer is empty so our vsync must be zero.

		//if( *(volatile LONG*)&g_pGSvSyncCount != 0 )
		//	SysPrintf( "MTGS > vSync count mismatch: %d\n", g_pGSvSyncCount );

		//InterlockedExchange( (volatile LONG*)&g_pGSvSyncCount, 0 );
		// process vu1
	}

	GSclose();
	return 0;
}

int gsFreeze(gzFile f, int Mode) {

	gzfreeze(PS2MEM_GS, 0x2000);
	gzfreeze(&CSRw, sizeof(CSRw));
	gzfreeze(g_path, sizeof(g_path));
	gzfreeze(s_byRegs, sizeof(s_byRegs));

	return 0;
}

#ifdef PCSX2_DEVBUILD

struct GSStatePacket
{
	u32 type;
	vector<u8> mem;
};

// runs the GS
void RunGSState(gzFile f)
{
	u32 newfield;
	list< GSStatePacket > packets;

	while(!gzeof(f)) {
		int type, size;
		gzread(f, &type, sizeof(type));

		if( type != GSRUN_VSYNC ) gzread(f, &size, 4);

		packets.push_back(GSStatePacket());
		GSStatePacket& p = packets.back();

		p.type = type;

		if( type != GSRUN_VSYNC ) {
			p.mem.resize(size*16);
			gzread(f, &p.mem[0], size*16);
		}
	}

	list<GSStatePacket>::iterator it = packets.begin();
	g_SaveGSStream = 3;

	int skipfirst = 1;

	// first extract the data
	while(1) {

		switch(it->type) {
			case GSRUN_TRANS1:
				GSgifTransfer1((u32*)&it->mem[0], 0);
				break;
			case GSRUN_TRANS2:
				GSgifTransfer2((u32*)&it->mem[0], it->mem.size()/16);
				break;
			case GSRUN_TRANS3:
				GSgifTransfer3((u32*)&it->mem[0], it->mem.size()/16);
				break;
			case GSRUN_VSYNC:
				// flip
				newfield = (*(u32*)(PS2MEM_GS+0x1000)&0x2000) ? 0 : 0x2000;
				*(u32*)(PS2MEM_GS+0x1000) = (*(u32*)(PS2MEM_GS+0x1000) & ~(1<<13)) | newfield;

				GSvsync(newfield);
				SysUpdate();

				if( g_SaveGSStream != 3 )
					return;
				break;
			default:
				assert(0);
		}

		++it;
		if( it == packets.end() )
			it = packets.begin();
	}
}

#endif

#undef GIFchain
