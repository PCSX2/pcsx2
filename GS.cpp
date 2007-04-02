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

// rewritten by zerofrog to add multithreading/gs caching to GS and VU1


#if defined(__WIN32__)
#include <windows.h>
#endif

#include <assert.h>

#include <vector>
#include <list>

using namespace std;

extern "C" {

#define PLUGINtypedefs // for GSgifTransfer1

#include "PS2Etypes.h"
#include "PS2EDefs.h"
#include "zlib.h"
#include "ElfHeader.h"
#include "Misc.h"
#include "System.h"
#include "R5900.h"
#include "Vif.h"
#include "VU.h"
#include "vifdma.h"
#include "memory.h"
#include "Hw.h"

#include "ix86/ix86.h"
#include "iR5900.h"

#include "counters.h"
#include "GS.h"

extern _GSinit            GSinit;
extern _GSopen            GSopen;
extern _GSclose           GSclose;
extern _GSshutdown        GSshutdown;
extern _GSvsync           GSvsync;
extern _GSgifTransfer1    GSgifTransfer1;
extern _GSgifTransfer2    GSgifTransfer2;
extern _GSgifTransfer3    GSgifTransfer3;
extern _GSgifSoftReset    GSgifSoftReset;
extern _GSreadFIFO        GSreadFIFO;
extern _GSreadFIFO2       GSreadFIFO2;

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

// could convert to pthreads easily, just don't have the time	
HANDLE g_hGsEvent = NULL, // set when path3 is ready to be processed
	g_hVuGSExit = NULL;		// set when thread needs to exit
HANDLE g_hGSOpen = NULL, g_hGSDone = NULL;
HANDLE g_hVuGSThread = NULL;

int g_FFXHack=0;

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
void gsWaitGS();

extern long pDsp;

PCSX2_ALIGNED16(u8 g_MTGSMem[0x2000]); // mtgs has to have its own memory

} // extern "C"

#ifdef WIN32_VIRTUAL_MEM
#define gif ((DMACh*)&PS2MEM_HW[0xA000])
#else
#define gif ((DMACh*)&psH[0xA000])
#endif

#ifdef WIN32_VIRTUAL_MEM
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
static PCSX2_ALIGNED16(BYTE s_byRegs[3][16]);

HANDLE g_hAllGsReady[3] = {NULL};

DWORD WINAPI GSThreadProc(LPVOID lpParam);

// g_pGSRingPos == g_pGSWritePos => fifo is empty
u8* g_pGSRingPos = NULL, // cur pos ring is at
	*g_pGSWritePos = NULL; // cur pos ee thread is at

extern int g_nCounters[];

extern void * memcpy_amd(void *dest, const void *src, size_t n);

void gsInit()
{
	if( CHECK_MULTIGS ) {
		g_hGsEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

		g_hVuGSExit = CreateEvent(NULL, FALSE, FALSE, NULL);
		g_hGSOpen = CreateEvent(NULL, FALSE, FALSE, NULL);
		g_hGSDone = CreateEvent(NULL, FALSE, FALSE, NULL);

		SysPrintf("gsInit\n");

#ifdef _DEBUG
		assert( g_fMTGSWrite == NULL && g_fMTGSRead == NULL );
		g_fMTGSWrite = fopen("mtgswrite.txt", "w");
		g_fMTGSRead = fopen("mtgsread.txt", "w");
#endif

		g_pGSRingPos = (u8*)VirtualAlloc(GS_RINGBUFFERBASE, GS_RINGBUFFERSIZE, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
		if( g_pGSRingPos != GS_RINGBUFFERBASE ) {
			MessageBox(NULL, "Cannot alloc GS ring buffer\n", "Error", MB_OK);
			exit(0);
		}

		memcpy(g_MTGSMem, PS2MEM_GS, sizeof(g_MTGSMem));

		if( GSsetBaseMem != NULL )
			GSsetBaseMem(g_MTGSMem);

		InterlockedExchangePointer(&g_pGSWritePos, GS_RINGBUFFERBASE);
		g_hVuGSThread = CreateThread(NULL, 0, GSThreadProc, NULL, 0, NULL);
	}
}

void gsWaitGS()
{
	while( g_pGSRingPos != g_pGSWritePos )
		Sleep(1);
}

void gsShutdown()
{
	if( CHECK_MULTIGS ) {

		SetEvent(g_hVuGSExit);
		SysPrintf("Closing gs thread\n");
		WaitForSingleObject(g_hVuGSThread, INFINITE);
		CloseHandle(g_hVuGSThread);
		CloseHandle(g_hGsEvent);
		CloseHandle(g_hVuGSExit);
		CloseHandle(g_hGSOpen);
		CloseHandle(g_hGSDone);

		VirtualFree(GS_RINGBUFFERBASE, GS_RINGBUFFERSIZE, MEM_DECOMMIT|MEM_RELEASE);

#ifdef _DEBUG
		if( g_fMTGSWrite != NULL ) {
			fclose(g_fMTGSWrite);
			g_fMTGSWrite = NULL;
		}
		if( g_fMTGSRead != NULL ) {
			fclose(g_fMTGSRead);
			g_fMTGSRead = NULL;
		}
#endif
	}
	else GSclose();
}

typedef u8* PU8;

u8* GSRingBufCopy(void* mem, u32 size, u32 type)
{
	u8* writepos = g_pGSWritePos;
	u8* tempbuf;
	assert( size < GS_RINGBUFFERSIZE );
	assert( writepos < GS_RINGBUFFEREND );
	assert( ((u32)writepos & 15) == 0 );
	assert( (size&15) == 0);

	size += 16;
	tempbuf = *(volatile PU8*)&g_pGSRingPos;
	if( writepos + size > GS_RINGBUFFEREND ) {
	
		// skip to beginning
		while( writepos < tempbuf || tempbuf == GS_RINGBUFFERBASE ) {
			if( !CHECK_DUALCORE ) {
				SetEvent(g_hGsEvent);
				Sleep(1);
			}
			tempbuf = *(volatile PU8*)&g_pGSRingPos;

			if( tempbuf == *(volatile PU8*)&g_pGSWritePos )
				break;
		}

		// notify GS
		if( writepos != GS_RINGBUFFEREND ) {
			InterlockedExchangePointer(writepos, GS_RINGTYPE_RESTART);
		}

		InterlockedExchangePointer(&g_pGSWritePos, GS_RINGBUFFERBASE);
		writepos = GS_RINGBUFFERBASE;
	}

	while( writepos < tempbuf && (writepos+size >= tempbuf || (writepos+size == GS_RINGBUFFEREND && tempbuf == GS_RINGBUFFERBASE)) ) {
		if( !CHECK_DUALCORE ) {
			SetEvent(g_hGsEvent);
			Sleep(1);
		}
		tempbuf = *(volatile PU8*)&g_pGSRingPos;

		if( tempbuf == *(volatile PU8*)&g_pGSWritePos )
			break;
	}

	// just copy
	*(u32*)writepos = type|(((size-16)>>4)<<16);
	return writepos+16;
}

void GSRingBufSimplePacket(int type, int data0, int data1, int data2)
{
	u8* writepos = g_pGSWritePos;
	u8* tempbuf;

	assert( writepos + 16 <= GS_RINGBUFFEREND );

	tempbuf = *(volatile PU8*)&g_pGSRingPos;
	if( writepos < tempbuf && writepos+16 >= tempbuf ) {
		
		do {
			if( !CHECK_DUALCORE ) {
				SetEvent(g_hGsEvent);
				Sleep(1);
			}
			tempbuf = *(volatile PU8*)&g_pGSRingPos;

			if( tempbuf == *(volatile PU8*)&g_pGSWritePos )
				break;
		} while(writepos < tempbuf && writepos+16 >= tempbuf );
	}

	*(u32*)writepos = type;
	*(int*)(writepos+4) = data0;
	*(int*)(writepos+8) = data1;
	*(int*)(writepos+12) = data2;

	writepos += 16;
	if( writepos == GS_RINGBUFFEREND ) writepos = GS_RINGBUFFERBASE;
	InterlockedExchangePointer(&g_pGSWritePos, writepos);

	if( !CHECK_DUALCORE )
		SetEvent(g_hGsEvent);
}

void gsReset()
{
	SysPrintf("GIF reset\n");

	// GSDX crashes
	//if( GSreset ) GSreset();

	if( CHECK_MULTIGS ) {
		ResetEvent(g_hGsEvent);
		ResetEvent(g_hVuGSExit);

		g_pGSRingPos = g_pGSWritePos;
	}

	memset(g_path, 0, sizeof(g_path));
	memset(s_byRegs, 0, sizeof(s_byRegs));

#ifndef WIN32_VIRTUAL_MEM
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

#ifndef WIN32_VIRTUAL_MEM
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
		//GSCSRr = 0x400E; // The host FIFO neeeds to be empty too or GSsync will fail (saqib)
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

#ifdef GIF_LOG
	GIF_LOG("GS write 8 at %8.8lx with data %8.8lx\n", mem, value);
#endif
}

void gsConstWrite8(u32 mem, int mmreg)
{
	switch (mem&~3) {
		case 0x12001000: // GS_CSR
			_eeMoveMMREGtoR(EAX, mmreg);
			iFlushCall(0);
			MOV32MtoR(ECX, (u32)&CSRw);
			AND32ItoR(EAX, 0xff<<(mem&3)*8);
			AND32ItoR(ECX, ~(0xff<<(mem&3)*8));
			OR32ItoR(EAX, ECX);
			PUSH32R(EAX);
			CALLFunc((u32)CSRwrite);
			ADD32ItoR(ESP, 4);
			break;
		default:
			_eeWriteConstMem8( (u32)PS2GS_BASE(mem), mmreg );

			if( CHECK_MULTIGS ) {
				_recPushReg(mmreg);

				iFlushCall(0);

				PUSH32I(mem&0x13ff);
				PUSH32I(GS_RINGTYPE_MEMWRITE8);
				CALLFunc((u32)GSRingBufSimplePacket);
				ADD32ItoR(ESP, 12);
			}
			break;
	}
}

extern void UpdateVSyncRate();

void gsWrite16(u32 mem, u16 value) {
	
	switch (mem) {
		case 0x12000010: // GS_SMODE1
			if((value & 0x6000) == 0x6000) Config.PsxType |= 1; // PAL
			else Config.PsxType &= ~1;	// NTSC
			*(u16*)PS2GS_BASE(mem) = value;

			if( CHECK_MULTIGS ) {
				GSRingBufSimplePacket(GS_RINGTYPE_MEMWRITE16, mem&0x13ff, value, 0);
			}

			UpdateVSyncRate();
			break;
			
		case 0x12000020: // GS_SMODE2
			if(value & 0x1) Config.PsxType |= 2; // Interlaced
			else Config.PsxType &= ~2;	// Non-Interlaced
			*(u16*)PS2GS_BASE(mem) = value;

			if( CHECK_MULTIGS ) {
				GSRingBufSimplePacket(GS_RINGTYPE_MEMWRITE16, mem&0x13ff, value, 0);
			}

			break;
			
		case 0x12001000: // GS_CSR
			CSRwrite( (CSRw&0xffff0000) | value);
			break;
		case 0x12001002: // GS_CSR
			CSRwrite( (CSRw&0xffff) | ((u32)value<<16));
			break;
		case 0x12001010: // GS_IMR
			SysPrintf("writing to IMR 16\n");
			IMRwrite(value);
			break;

		default:
			*(u16*)PS2GS_BASE(mem) = value;

			if( CHECK_MULTIGS ) {
				GSRingBufSimplePacket(GS_RINGTYPE_MEMWRITE16, mem&0x13ff, value, 0);
			}
	}

#ifdef GIF_LOG
	GIF_LOG("GS write 16 at %8.8lx with data %8.8lx\n", mem, value);
#endif
}

void recSetSMODE1()
{
	iFlushCall(0);
	AND32ItoR(EAX, 0x6000);
	CMP32ItoR(EAX, 0x6000);
	j8Ptr[5] = JNE8(0);

	// PAL
	OR32ItoM( (u32)&Config.PsxType, 1);
	j8Ptr[6] = JMP8(0);

	x86SetJ8( j8Ptr[5] );

	// NTSC
	AND32ItoM( (u32)&Config.PsxType, ~1 );

	x86SetJ8( j8Ptr[6] );
	CALLFunc((u32)UpdateVSyncRate);
}

void recSetSMODE2()
{
	TEST32ItoR(EAX, 1);
	j8Ptr[5] = JZ8(0);

	// Interlaced
	OR32ItoM( (u32)&Config.PsxType, 2);
	j8Ptr[6] = JMP8(0);

	x86SetJ8( j8Ptr[5] );

	// Non-Interlaced
	AND32ItoM( (u32)&Config.PsxType, ~2 );

	x86SetJ8( j8Ptr[6] );
}

void gsConstWrite16(u32 mem, int mmreg)
{	
	switch (mem&~3) {
		case 0x12000010: // GS_SMODE1
			assert( !(mem&3));
			_eeMoveMMREGtoR(EAX, mmreg);
			_eeWriteConstMem16( (u32)PS2GS_BASE(mem), mmreg );

			if( CHECK_MULTIGS ) PUSH32R(EAX);

			recSetSMODE1();

			if( CHECK_MULTIGS ) {
				iFlushCall(0);

				PUSH32I(mem&0x13ff);
				PUSH32I(GS_RINGTYPE_MEMWRITE16);
				CALLFunc((u32)GSRingBufSimplePacket);
				ADD32ItoR(ESP, 12);
			}

			break;
			
		case 0x12000020: // GS_SMODE2
			assert( !(mem&3));
			_eeMoveMMREGtoR(EAX, mmreg);
			_eeWriteConstMem16( (u32)PS2GS_BASE(mem), mmreg );

			if( CHECK_MULTIGS ) PUSH32R(EAX);

			recSetSMODE2();

			if( CHECK_MULTIGS ) {
				iFlushCall(0);

				PUSH32I(mem&0x13ff);
				PUSH32I(GS_RINGTYPE_MEMWRITE16);
				CALLFunc((u32)GSRingBufSimplePacket);
				ADD32ItoR(ESP, 12);
			}

			break;
			
		case 0x12001000: // GS_CSR

			assert( !(mem&2) );
			_eeMoveMMREGtoR(EAX, mmreg);
			iFlushCall(0);

			MOV32MtoR(ECX, (u32)&CSRw);
			AND32ItoR(EAX, 0xffff<<(mem&2)*8);
			AND32ItoR(ECX, ~(0xffff<<(mem&2)*8));
			OR32ItoR(EAX, ECX);
			PUSH32R(EAX);
			CALLFunc((u32)CSRwrite);
			ADD32ItoR(ESP, 4);
			break;

		default:
			_eeWriteConstMem16( (u32)PS2GS_BASE(mem), mmreg );

			if( CHECK_MULTIGS ) {
				_recPushReg(mmreg);

				iFlushCall(0);

				PUSH32I(mem&0x13ff);
				PUSH32I(GS_RINGTYPE_MEMWRITE16);
				CALLFunc((u32)GSRingBufSimplePacket);
				ADD32ItoR(ESP, 12);
			}

			break;
	}
}

void gsWrite32(u32 mem, u32 value)
{
	assert( !(mem&3));
	switch (mem) {
		case 0x12000010: // GS_SMODE1
			if((value & 0x6000) == 0x6000) Config.PsxType |= 1; // PAL
			else Config.PsxType &= ~1;	// NTSC
			*(u32*)PS2GS_BASE(mem) = value;
			
			UpdateVSyncRate();

			if( CHECK_MULTIGS ) {
				GSRingBufSimplePacket(GS_RINGTYPE_MEMWRITE32, mem&0x13ff, value, 0);
			}

			break;
		case 0x12000020: // GS_SMODE2
			if(value & 0x1) Config.PsxType |= 2; // Interlaced
			else Config.PsxType &= ~2;	// Non-Interlaced
			*(u32*)PS2GS_BASE(mem) = value;

			if( CHECK_MULTIGS ) {
				GSRingBufSimplePacket(GS_RINGTYPE_MEMWRITE32, mem&0x13ff, value, 0);
			}
			break;
			
		case 0x12001000: // GS_CSR
			CSRwrite(value);
			break;

		case 0x12001010: // GS_IMR
			IMRwrite(value);
			break;
		default:
			*(u32*)PS2GS_BASE(mem) = value;

			if( CHECK_MULTIGS ) {
				GSRingBufSimplePacket(GS_RINGTYPE_MEMWRITE32, mem&0x13ff, value, 0);
			}
	}

#ifdef GIF_LOG
	GIF_LOG("GS write 32 at %8.8lx with data %8.8lx\n", mem, value);
#endif
}

// (value&0x1f00)|0x6000
void gsConstWriteIMR(int mmreg)
{
	const u32 mem = 0x12001010;
	if( mmreg & MEM_XMMTAG ) {
		SSE2_MOVD_XMM_to_M32((u32)PS2GS_BASE(mem), mmreg&0xf);
		AND32ItoM((u32)PS2GS_BASE(mem), 0x1f00);
		OR32ItoM((u32)PS2GS_BASE(mem), 0x6000);
	}
	else if( mmreg & MEM_MMXTAG ) {
		SetMMXstate();
		MOVDMMXtoM((u32)PS2GS_BASE(mem), mmreg&0xf);
		AND32ItoM((u32)PS2GS_BASE(mem), 0x1f00);
		OR32ItoM((u32)PS2GS_BASE(mem), 0x6000);
	}
	else if( mmreg & MEM_EECONSTTAG ) {
		MOV32ItoM( (u32)PS2GS_BASE(mem), (g_cpuConstRegs[(mmreg>>16)&0x1f].UL[0]&0x1f00)|0x6000);
	}
	else {
		AND32ItoR(mmreg, 0x1f00);
		OR32ItoR(mmreg, 0x6000);
		MOV32RtoM( (u32)PS2GS_BASE(mem), mmreg );
	}

	// IMR doesn't need to be updated in MTGS mode
}

void gsConstWrite32(u32 mem, int mmreg) {

	switch (mem) {

		case 0x12000010: // GS_SMODE1
			_eeMoveMMREGtoR(EAX, mmreg);
			_eeWriteConstMem32( (u32)PS2GS_BASE(mem), mmreg );

			if( CHECK_MULTIGS ) PUSH32R(EAX);

			recSetSMODE1();

			if( CHECK_MULTIGS ) {
				iFlushCall(0);

				PUSH32I(mem&0x13ff);
				PUSH32I(GS_RINGTYPE_MEMWRITE32);
				CALLFunc((u32)GSRingBufSimplePacket);
				ADD32ItoR(ESP, 12);
			}

			break;

		case 0x12000020: // GS_SMODE2
			_eeMoveMMREGtoR(EAX, mmreg);
			_eeWriteConstMem32( (u32)PS2GS_BASE(mem), mmreg );

			if( CHECK_MULTIGS ) PUSH32R(EAX);

			recSetSMODE2();

			if( CHECK_MULTIGS ) {
				iFlushCall(0);

				PUSH32I(mem&0x13ff);
				PUSH32I(GS_RINGTYPE_MEMWRITE32);
				CALLFunc((u32)GSRingBufSimplePacket);
				ADD32ItoR(ESP, 12);
			}

			break;
			
		case 0x12001000: // GS_CSR
			_recPushReg(mmreg);
			iFlushCall(0);
			CALLFunc((u32)CSRwrite);
			ADD32ItoR(ESP, 4);
			break;

		case 0x12001010: // GS_IMR
			gsConstWriteIMR(mmreg);
			break;
		default:
			_eeWriteConstMem32( (u32)PS2GS_BASE(mem), mmreg );

			if( CHECK_MULTIGS ) {
				_recPushReg(mmreg);

				iFlushCall(0);

				PUSH32I(mem&0x13ff);
				PUSH32I(GS_RINGTYPE_MEMWRITE32);
				CALLFunc((u32)GSRingBufSimplePacket);
				ADD32ItoR(ESP, 12);
			}

			break;
	}
}

void gsWrite64(u32 mem, u64 value) {

	switch (mem) {
		case 0x12000010: // GS_SMODE1
			if((value & 0x6000) == 0x6000) Config.PsxType |= 1; // PAL
			else Config.PsxType &= ~1;	// NTSC
			UpdateVSyncRate();
			*(u64*)PS2GS_BASE(mem) = value;

			if( CHECK_MULTIGS ) {
				GSRingBufSimplePacket(GS_RINGTYPE_MEMWRITE64, mem&0x13ff, (u32)value, (u32)(value>>32));
			}

			break;

		case 0x12000020: // GS_SMODE2
			if(value & 0x1) Config.PsxType |= 2; // Interlaced
			else Config.PsxType &= ~2;	// Non-Interlaced
			*(u64*)PS2GS_BASE(mem) = value;

			if( CHECK_MULTIGS ) {
				GSRingBufSimplePacket(GS_RINGTYPE_MEMWRITE64, mem&0x13ff, (u32)value, 0);
			}

			break;
		case 0x12001000: // GS_CSR
			CSRwrite((u32)value);
			break;

		case 0x12001010: // GS_IMR
			IMRwrite((u32)value);
			break;

		default:
			*(u64*)PS2GS_BASE(mem) = value;

			if( CHECK_MULTIGS ) {
				GSRingBufSimplePacket(GS_RINGTYPE_MEMWRITE64, mem&0x13ff, (u32)value, (u32)(value>>32));
			}
	}

#ifdef GIF_LOG
	GIF_LOG("GS write 64 at %8.8lx with data %8.8lx_%8.8lx\n", mem, ((u32*)&value)[1], (u32)value);
#endif
}

void gsConstWrite64(u32 mem, int mmreg)
{
	switch (mem) {
		case 0x12000010: // GS_SMODE1
			_eeMoveMMREGtoR(EAX, mmreg);
			_eeWriteConstMem64((u32)PS2GS_BASE(mem), mmreg);

			if( CHECK_MULTIGS ) PUSH32R(EAX);

			recSetSMODE1();

			if( CHECK_MULTIGS ) {
				iFlushCall(0);

				PUSH32I(mem&0x13ff);
				PUSH32I(GS_RINGTYPE_MEMWRITE32);
				CALLFunc((u32)GSRingBufSimplePacket);
				ADD32ItoR(ESP, 12);
			}

			break;

		case 0x12000020: // GS_SMODE2
			_eeMoveMMREGtoR(EAX, mmreg);
			_eeWriteConstMem64((u32)PS2GS_BASE(mem), mmreg);

			if( CHECK_MULTIGS ) PUSH32R(EAX);

			recSetSMODE2();

			if( CHECK_MULTIGS ) {
				iFlushCall(0);

				PUSH32I(mem&0x13ff);
				PUSH32I(GS_RINGTYPE_MEMWRITE32);
				CALLFunc((u32)GSRingBufSimplePacket);
				ADD32ItoR(ESP, 12);
			}

			break;

		case 0x12001000: // GS_CSR
			_recPushReg(mmreg);
			iFlushCall(0);
			CALLFunc((u32)CSRwrite);
			ADD32ItoR(ESP, 4);
			break;

		case 0x12001010: // GS_IMR
			gsConstWriteIMR(mmreg);
			break;

		default:
			_eeWriteConstMem64((u32)PS2GS_BASE(mem), mmreg);

			if( CHECK_MULTIGS ) {
				iFlushCall(0);

				PUSH32M((u32)PS2GS_BASE(mem)+4);
				PUSH32M((u32)PS2GS_BASE(mem));
				PUSH32I(mem&0x13ff);
				PUSH32I(GS_RINGTYPE_MEMWRITE64);
				CALLFunc((u32)GSRingBufSimplePacket);
				ADD32ItoR(ESP, 16);
			}

			break;
	}
}

void gsConstWrite128(u32 mem, int mmreg)
{
	switch (mem) {
		case 0x12000010: // GS_SMODE1
			_eeMoveMMREGtoR(EAX, mmreg);
			_eeWriteConstMem128( (u32)PS2GS_BASE(mem), mmreg);

			if( CHECK_MULTIGS ) PUSH32R(EAX);

			recSetSMODE1();

			if( CHECK_MULTIGS ) {
				iFlushCall(0);

				PUSH32I(mem&0x13ff);
				PUSH32I(GS_RINGTYPE_MEMWRITE32);
				CALLFunc((u32)GSRingBufSimplePacket);
				ADD32ItoR(ESP, 12);
			}

			break;

		case 0x12000020: // GS_SMODE2
			_eeMoveMMREGtoR(EAX, mmreg);
			_eeWriteConstMem128( (u32)PS2GS_BASE(mem), mmreg);

			if( CHECK_MULTIGS ) PUSH32R(EAX);

			recSetSMODE2();

			if( CHECK_MULTIGS ) {
				iFlushCall(0);

				PUSH32I(mem&0x13ff);
				PUSH32I(GS_RINGTYPE_MEMWRITE32);
				CALLFunc((u32)GSRingBufSimplePacket);
				ADD32ItoR(ESP, 12);
			}

			break;

		case 0x12001000: // GS_CSR
			_recPushReg(mmreg);
			iFlushCall(0);
			CALLFunc((u32)CSRwrite);
			ADD32ItoR(ESP, 4);
			break;

		case 0x12001010: // GS_IMR
			// (value&0x1f00)|0x6000
			gsConstWriteIMR(mmreg);
			break;

		default:
			_eeWriteConstMem128( (u32)PS2GS_BASE(mem), mmreg);

			if( CHECK_MULTIGS ) {
				iFlushCall(0);

				PUSH32M((u32)PS2GS_BASE(mem)+4);
				PUSH32M((u32)PS2GS_BASE(mem));
				PUSH32I(mem&0x13ff);
				PUSH32I(GS_RINGTYPE_MEMWRITE64);
				CALLFunc((u32)GSRingBufSimplePacket);
				ADD32ItoR(ESP, 16);

				PUSH32M((u32)PS2GS_BASE(mem)+12);
				PUSH32M((u32)PS2GS_BASE(mem)+8);
				PUSH32I(mem&0x13ff);
				PUSH32I(GS_RINGTYPE_MEMWRITE64);
				CALLFunc((u32)GSRingBufSimplePacket);
				ADD32ItoR(ESP, 16);
			}

			break;
	}
}

u8 gsRead8(u32 mem)
{
#ifdef GIF_LOG
	GIF_LOG("GS read 8 %8.8lx, at %8.8lx\n", *(u8*)(PS2MEM_BASE+(mem&~0xc00)), mem);
#endif

	return *(u8*)PS2GS_BASE(mem);
}

int gsConstRead8(u32 x86reg, u32 mem, u32 sign)
{
#ifdef GIF_LOG
	GIF_LOG("GS read 8 %8.8lx (%8.8x), at %8.8lx\n", (u32)PS2GS_BASE(mem), mem);
#endif
	_eeReadConstMem8(x86reg, (u32)PS2GS_BASE(mem), sign);
	return 0;
}

u16 gsRead16(u32 mem)
{
#ifdef GIF_LOG
	GIF_LOG("GS read 16 %8.8lx, at %8.8lx\n", *(u16*)(PS2MEM_BASE+(mem&~0xc00)), mem);
#endif

	return *(u16*)PS2GS_BASE(mem);
}

int gsConstRead16(u32 x86reg, u32 mem, u32 sign)
{
#ifdef GIF_LOG
	GIF_LOG("GS read 16 %8.8lx (%8.8x), at %8.8lx\n", (u32)PS2GS_BASE(mem), mem);
#endif
	_eeReadConstMem16(x86reg, (u32)PS2GS_BASE(mem), sign);
	return 0;
}

u32 gsRead32(u32 mem) {

#ifdef GIF_LOG
	GIF_LOG("GS read 32 %8.8lx, at %8.8lx\n", *(u32*)(PS2MEM_BASE+(mem&~0xc00)), mem);
#endif
	return *(u32*)PS2GS_BASE(mem);
}

int gsConstRead32(u32 x86reg, u32 mem)
{
#ifdef GIF_LOG
	GIF_LOG("GS read 32 %8.8lx (%8.8x), at %8.8lx\n", (u32)PS2GS_BASE(mem), mem);
#endif
	_eeReadConstMem32(x86reg, (u32)PS2GS_BASE(mem));
	return 0;
}

u64 gsRead64(u32 mem)
{
#ifdef GIF_LOG
	GIF_LOG("GS read 64 %8.8lx (%8.8x), at %8.8lx\n", (u32)PS2GS_BASE(mem), mem);
#endif
	return *(u64*)PS2GS_BASE(mem);
}

void gsConstRead64(u32 mem, int mmreg)
{
#ifdef GIF_LOG
	GIF_LOG("GS read 64 %8.8lx (%8.8x), at %8.8lx\n", (u32)PS2GS_BASE(mem), mem);
#endif
	if( IS_XMMREG(mmreg) ) SSE_MOVLPS_M64_to_XMM(mmreg&0xff, (u32)PS2GS_BASE(mem));
	else {
		MOVQMtoR(mmreg, (u32)PS2GS_BASE(mem));
		SetMMXstate();
	}
}

void gsConstRead128(u32 mem, int xmmreg)
{
#ifdef GIF_LOG
	GIF_LOG("GS read 128 %8.8lx (%8.8x), at %8.8lx\n", (u32)PS2GS_BASE(mem), mem);
#endif
	_eeReadConstMem128( xmmreg, (u32)PS2GS_BASE(mem));
}


void gsIrq() {
	hwIntcIrq(0);
}

static void GSRegHandlerSIGNAL(u32* data)
{
	GSSIGLBLID->SIGID = (GSSIGLBLID->SIGID&~data[1])|(data[0]&data[1]);
	
	if (!(GSCSRr & 0x1)) {
		GSCSRr |= 1; // signal
		//CSRw &= ~1;
		
	}
	if (!(GSIMR&0x100) )
		gsIrq();
	
}

static void GSRegHandlerFINISH(u32* data)
{
	
	if (!(GSCSRr & 0x2)) {
		//CSRw &= ~2;
		GSCSRr |= 2; // finish
		
	}
	if (!(GSIMR&0x200) )
		gsIrq();
}

static void GSRegHandlerLABEL(u32* data)
{
	GSSIGLBLID->LBLID = (GSSIGLBLID->LBLID&~data[1])|(data[0]&data[1]);
}

typedef void (*GIFRegHandler)(u32* data);
static GIFRegHandler s_GSHandlers[3] = { GSRegHandlerSIGNAL, GSRegHandlerFINISH, GSRegHandlerLABEL };

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
		if(nloop == 0) {

			ptag = (GIFTAG*)pMem;
			nreg = ptag->nreg == 0 ? 16 : ptag->nreg;

			pMem+= 4;
			size--;

			if(ptag->nloop == 0 ) {
				if( path == 0 ) {

					if( ptag->eop )
						return size;

					// ffx hack
					if( g_FFXHack )
						continue;

					return size;
				}

				g_path[path].nloop = 0;

				// motogp graphics show
				if( !ptag->eop )
					continue;

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
						if( (pMem[2]&0xff) < 0x64 )
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
                    // midnight madness cares because the tag is 5 dwords
                    int* psrc = (int*)ptag;
                    int* pdst = (int*)&g_path[path];
                    pdst[0] = psrc[0]; pdst[1] = psrc[1]; pdst[2] = psrc[2]; pdst[3] = psrc[3];
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

				for(; size > 0; pMem+= 2, size--)
				{
					if( s_byRegs[path][curreg] >= 0x60 )
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
                    // midnight madness cares because the tag is 5 dwords
                    int* psrc = (int*)ptag;
                    int* pdst = (int*)&g_path[path];
                    pdst[0] = psrc[0]; pdst[1] = psrc[1]; pdst[2] = psrc[2]; pdst[3] = psrc[3];
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
                    // midnight madness cares because the tag is 5 dwords
                    int* psrc = (int*)ptag;
                    int* pdst = (int*)&g_path[path];
                    pdst[0] = psrc[0]; pdst[1] = psrc[1]; pdst[2] = psrc[2]; pdst[3] = psrc[3];
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

int gsInterrupt() {
#ifdef GIF_LOG 
	GIF_LOG("gsInterrupt: %8.8x\n", cpuRegs.cycle);
#endif

	if(gif->qwc > 0) {
		if( !(psHu32(DMAC_CTRL) & 0x1) ) {
			SysPrintf("gs dma masked\n");
			return 0;
		}

		dmaGIF();
		return 0;
	}
		
	gif->chcr &= ~0x100;
	GSCSRr &= ~0xC000; //Clear FIFO stuff
	GSCSRr |= 0x4000;  //FIFO empty
	psHu32(GIF_STAT)&= ~0xE00; // OPH=0 | APATH=0
	psHu32(GIF_STAT)&= ~0x1F000000; // QFC=0
	hwDmacIrq(DMAC_GIF);

	return 1;
}

#define WRITERING_DMA(pMem, qwc) { \
	if( CHECK_MULTIGS) { \
		u8* pgsmem = GSRingBufCopy(pMem, (qwc)<<4, GS_RINGTYPE_P3); \
		if( pgsmem != NULL ) { \
			int sizetoread = (qwc)<<4; \
			u32 pendmem = (u32)gif->madr + sizetoread; \
			/* check if page of endmem is valid (dark cloud2) */ \
			if( dmaGetAddr(pendmem-16) == NULL ) { \
				pendmem = ((pendmem-16)&~0xfff)-16; \
				while(dmaGetAddr(pendmem) == NULL) { \
					pendmem = (pendmem&~0xfff)-16; \
				} \
				memcpy_amd(pgsmem, pMem, pendmem-(u32)gif->madr+16); \
			} \
			else memcpy_amd(pgsmem, pMem, sizetoread); \
			\
			GSRINGBUF_DONECOPY(pgsmem, sizetoread); \
			GSgifTransferDummy(2, pMem, qwc); \
		} \
		\
		if( !CHECK_DUALCORE ) SetEvent(g_hGsEvent); \
	} \
	else { \
		FreezeMMXRegs(1); \
		FreezeXMMRegs(1); \
		GSGIFTRANSFER3(pMem, qwc); \
	} \
} \

int  _GIFchain() {
	u32 qwc = gif->qwc;
	u32 *pMem;

	if (qwc == 0) return 0;

	pMem = (u32*)dmaGetAddr(gif->madr);
	if (pMem == NULL) {
		// reset path3, fixes dark cloud 2
		if( GSgifSoftReset != NULL )
			GSgifSoftReset(4);
		if( CHECK_MULTIGS ) {
			memset(&g_path[2], 0, sizeof(g_path[2]));
		}

		SysPrintf("NULL GIFchain\n");
		return -1;
	}

	WRITERING_DMA(pMem, qwc);

	gif->madr+= qwc*16;
	gif->qwc = 0;
	return (qwc)*2;
}

#define GIFchain() \
	if (gif->qwc) { \
		cycles+= _GIFchain(); /* guessing */ \
	}

int gscount = 0;
static int prevcycles = 0;
static u32* prevtag = NULL;

void dmaGIF() {
	u32 *ptag;
	int done=0;
	int cycles=prevcycles;
	u32 id;
	/*if ((psHu32(DMAC_CTRL) & 0xC0)) { 
			SysPrintf("DMA Stall Control %x\n",(psHu32(DMAC_CTRL) & 0xC0));
			}*/

	if ((psHu32(DMAC_CTRL) & 0xC) == 0xC ) { // GIF MFIFO
		gifMFIFOInterrupt();
		return;
	}
	if( (psHu32(GIF_CTRL) & 8) ) {
		// temporarily stop
		SysPrintf("Gif dma temp paused?\n");
		return;
	}

#ifdef GIF_LOG
	GIF_LOG("dmaGIF chcr = %lx, madr = %lx, qwc  = %lx\n"
			"        tadr = %lx, asr0 = %lx, asr1 = %lx\n",
			gif->chcr, gif->madr, gif->qwc,
			gif->tadr, gif->asr0, gif->asr1);
#endif

	if (psHu32(GIF_MODE) & 0x4) {
	} else
	if (vif1Regs->mskpath3 || psHu32(GIF_MODE) & 0x1) {
		gif->chcr &= ~0x100;
		psHu32(GIF_STAT)&= ~0xE00; // OPH=0 | APATH=0
		hwDmacIrq(2);
		return;
	}

	if ((psHu32(DMAC_CTRL) & 0xC0) == 0x80 && prevcycles != 0) { // STD == GIF
		SysPrintf("GS Stall Control Source = %x, Drain = %x\n MADR = %x, STADR = %x", (psHu32(0xe000) >> 4) & 0x3, (psHu32(0xe000) >> 6) & 0x3,gif->madr, psHu32(DMAC_STADR));

		if( gif->madr + (gif->qwc * 16) > psHu32(DMAC_STADR) ) {
			INT(2, cycles);
			return;
		}
		prevcycles = 0;
		gif->qwc = 0;
	}
	
	GSCSRr &= ~0xC000;  //Clear FIFO stuff
	GSCSRr |= 0x8000;   //FIFO full
	psHu32(GIF_STAT)|= 0xE00; // OPH=1 | APATH=3
	psHu32(GIF_STAT)|= 0x10000000; // FQC=31, hack ;)

	/*if( prevcycles != 0 ) {
		assert( prevtag != NULL );

		ptag = prevtag;
		id        = (ptag[0] >> 28) & 0x7;		//ID for DmaChain copied from bit 28 of the tag
		gif->qwc  = (u16)ptag[0];			    //QWC set to lower 16bits of the tag
		gif->madr = ptag[1];				    //MADR = ADDR field
		// transfer interrupted, so continue
		prevcycles = 0;
		GIFchain();

		if (gif->chcr & 0x80 && ptag[0] >> 31) {			 //Check TIE bit of CHCR and IRQ bit of tag
#ifdef GIF_LOG
			GIF_LOG("dmaIrq Set\n");
#endif					
			done = 1;
			
		}
	}*/

	// Transfer Dn_QWC from Dn_MADR to GIF
	if ((gif->chcr & 0xc) == 0 || gif->qwc > 0) { // Normal Mode
		//gscount++;
		if ((psHu32(DMAC_CTRL) & 0xC0) == 0x80 && (gif->chcr & 0xc) == 0) { 
			SysPrintf("DMA Stall Control on GIF normal\n");
		}
		GIFchain();
	}
	else {
		// Chain Mode
		while (done == 0) {						 // Loop while Dn_CHCR.STR is 1
			ptag = (u32*)dmaGetAddr(gif->tadr);  //Set memory pointer to TADR
			if (ptag == NULL) {					 //Is ptag empty?
				psHu32(DMAC_STAT)|= 1<<15;		 //If yes, set BEIS (BUSERR) in DMAC_STAT register
				break;
			}
			cycles+=2; // Add 1 cycles from the QW read for the tag
			// Transfer dma tag if tte is set
			if (gif->chcr & 0x40) {
				//u32 temptag[4] = {0};
#ifdef PCSX2_DEVBUILD
				//SysPrintf("GIF TTE: %x_%x\n", ptag[3], ptag[2]);
#endif

				//temptag[0] = ptag[2];
				//temptag[1] = ptag[3];
				//GSGIFTRANSFER3(ptag, 1); 
			}

			gif->chcr = ( gif->chcr & 0xFFFF ) | ( (*ptag) & 0xFFFF0000 );  //Transfer upper part of tag to CHCR bits 31-15
		
			id        = (ptag[0] >> 28) & 0x7;		//ID for DmaChain copied from bit 28 of the tag
			gif->qwc  = (u16)ptag[0];			    //QWC set to lower 16bits of the tag
			gif->madr = ptag[1];				    //MADR = ADDR field


			
			done = hwDmacSrcChainWithStack(gif, id);
#ifdef GIF_LOG
			GIF_LOG("dmaChain %8.8x_%8.8x size=%d, id=%d, addr=%lx\n",
					ptag[1], ptag[0], gif->qwc, id, gif->madr);
#endif
			if (!done && (psHu32(DMAC_CTRL) & 0xC0) == 0x80) { // STD == GIF
				// there are still bugs, need to also check if gif->madr +16*qwc >= stadr, if not, stall
				if( gif->madr + (gif->qwc * 16) > psHu32(DMAC_STADR) && id == 4) {
					// stalled
					SysPrintf("GS Stall Control Source = %x, Drain = %x\n MADR = %x, STADR = %x", (psHu32(0xe000) >> 4) & 0x3, (psHu32(0xe000) >> 6) & 0x3,gif->madr, psHu32(DMAC_STADR));
					prevcycles = cycles;
					gif->tadr -= 16;
					hwDmacIrq(13);
					INT(2, cycles);
					FreezeMMXRegs(0);
					FreezeXMMRegs(0);
					return;
				}
			}

			GIFchain();											 //Transfers the data set by the switch

			if ((gif->chcr & 0x80) && ptag[0] >> 31) {			 //Check TIE bit of CHCR and IRQ bit of tag
#ifdef GIF_LOG
				GIF_LOG("dmaIrq Set\n");
#endif
				//SysPrintf("GIF TIE\n");

	//			SysPrintf("GSdmaIrq Set\n");
				done = 1;
				//gif->qwc = 0;
				break;
			}
		}
	}

	prevtag = NULL;
	prevcycles = 0;
	INT(2, cycles);

	FreezeMMXRegs(0);
	FreezeXMMRegs(0);
}

static int mfifocycles;

int mfifoGIFrbTransfer() {
	u32 maddr = psHu32(DMAC_RBOR);
	int msize = psHu32(DMAC_RBSR)+16;
	u32 *src;

	/* Check if the transfer should wrap around the ring buffer */
	if ((gif->madr+gif->qwc*16) >= (maddr+msize)) {
		int s1 = (maddr+msize) - gif->madr;
		int s2 = gif->qwc*16 - s1;

		/* it does, so first copy 's1' bytes from 'addr' to 'data' */
		src = (u32*)PSM(gif->madr);
		if (src == NULL) return -1;
		WRITERING_DMA(src, s1&~15);

		/* and second copy 's2' bytes from 'maddr' to '&data[s1]' */
		src = (u32*)PSM(maddr);
		if (src == NULL) return -1;
		WRITERING_DMA(src, s2&~15);
		
	} else {
		/* it doesn't, so just transfer 'qwc*16' words 
		   from 'gif->madr' to GS */
		src = (u32*)PSM(gif->madr);
		if (src == NULL) return -1;
		
		WRITERING_DMA(src, gif->qwc);
	}

	gif->madr+= gif->qwc*16;
	

	return 0;
}

int mfifoGIFchain() {
	u32 maddr = psHu32(DMAC_RBOR);
	int msize = psHu32(DMAC_RBSR)+16;
	u32 *pMem;

	/* Is QWC = 0? if so there is nothing to transfer */
	if (gif->qwc == 0) return 0;

	if (gif->madr >= maddr &&
		gif->madr <= (maddr+msize)) {
		if (mfifoGIFrbTransfer() == -1) return -1;
	} else {
		pMem = (u32*)dmaGetAddr(gif->madr);
		if (pMem == NULL) return -1;

		WRITERING_DMA(pMem, gif->qwc);
		gif->madr+= gif->qwc*16;
	}

	mfifocycles+= (gif->qwc) * 2; /* guessing */
	gif->madr = psHu32(DMAC_RBOR) + (gif->madr & psHu32(DMAC_RBSR));
	gif->qwc = 0;

	return 0;
}

#define spr0 ((DMACh*)&PS2MEM_HW[0xD000])

static int giftempqwc = 0;
static int g_gifCycles = 0;
static int gifqwc = 0;
static int gifdone = 0;

void mfifoGIFtransfer(int qwc) {
	u32 *ptag;
	int id;
	u32 temp = 0;
	mfifocycles = 0;
	gifqwc += qwc;
	g_gifCycles = 0;
	if(gifqwc == 0) {
	//#ifdef PCSX2_DEVBUILD
				/*if( gifqwc > 1 )
					SysPrintf("gif mfifo tadr==madr but qwc = %d\n", gifqwc);*/
	//#endif
				//INT(11,50);
				return;
			}
/*if ((psHu32(DMAC_CTRL) & 0xC0)) { 
			SysPrintf("DMA Stall Control %x\n",(psHu32(DMAC_CTRL) & 0xC0));
			}*/
#ifdef GIF_LOG
	GIF_LOG("mfifoGIFtransfer %x madr %x, tadr %x\n", gif->chcr, gif->madr, gif->tadr);
#endif
	
 if((gif->chcr & 0x100) == 0)SysPrintf("MFIFO GIF not ready!\n");
	//while (qwc > 0 && done == 0) {
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
			
			
			/*if(gif->chcr & 0x40) { //Not used-doesnt work :P
				ret = GIFtransfer(ptag+2, 2, 1);
				assert(ret == 0 ); // gif stall code not implemented
			}*/
			
			gif->chcr = ( gif->chcr & 0xFFFF ) | ( (*ptag) & 0xFFFF0000 );
	 
	#ifdef GIF_LOG
			GIF_LOG("dmaChain %8.8x_%8.8x size=%d, id=%d, madr=%lx, tadr=%lx mfifo qwc = %x spr0 madr = %x\n",
					ptag[1], ptag[0], gif->qwc, id, gif->madr, gif->tadr, gifqwc, spr0->madr);
	#endif
			
			switch (id) {
				case 0: // Refe - Transfer Packet According to ADDR field
					if(gifqwc < gif->qwc && (gif->madr & ~psHu32(DMAC_RBSR)) == psHu32(DMAC_RBOR)) {
						//SysPrintf("Sliced GIF MFIFO Transfer %d\n", id);
						return;
					}
					gif->tadr += 16;
					gifdone = 1;										//End Transfer
					break;

				case 1: // CNT - Transfer QWC following the tag.
					if(gifqwc < gif->qwc && ((gif->tadr + 16) & ~psHu32(DMAC_RBSR)) == psHu32(DMAC_RBOR)) {
						//SysPrintf("Sliced GIF MFIFO Transfer %d\n", id);
						return;
					}
					gif->madr = gif->tadr + 16;						//Set MADR to QW after Tag            
					gif->tadr = gif->madr + (gif->qwc << 4);			//Set TADR to QW following the data
					gifdone = 0;
					break;

				case 2: // Next - Transfer QWC following tag. TADR = ADDR
					if(gifqwc < gif->qwc && ((gif->tadr + 16) & ~psHu32(DMAC_RBSR)) == psHu32(DMAC_RBOR)) {
						//SysPrintf("Sliced GIF MFIFO Transfer %d\n", id);
						return;
					}
					temp = gif->madr;								//Temporarily Store ADDR
					gif->madr = gif->tadr + 16; 					  //Set MADR to QW following the tag
					gif->tadr = temp;								//Copy temporarily stored ADDR to Tag
					gifdone = 0;
					break;

				case 3: // Ref - Transfer QWC from ADDR field
				case 4: // Refs - Transfer QWC from ADDR field (Stall Control) 
					if(gifqwc < gif->qwc && (gif->madr & ~psHu32(DMAC_RBSR)) == psHu32(DMAC_RBOR)) {
						//SysPrintf("Sliced GIF MFIFO Transfer %d\n", id);
						return;
					}
					gif->tadr += 16;									//Set TADR to next tag
					gifdone = 0;
					break;

				case 7: // End - Transfer QWC following the tag
					if(gifqwc < gif->qwc && ((gif->tadr + 16) & ~psHu32(DMAC_RBSR)) == psHu32(DMAC_RBOR)) {
						//SysPrintf("Sliced GIF MFIFO Transfer %d\n", id);
						return;
					}
					gif->madr = gif->tadr + 16;						//Set MADR to data following the tag
					gif->tadr = gif->madr + (gif->qwc << 4);			//Set TADR to QW following the data
					gifdone = 1;										//End Transfer
					break;
			}
			gifqwc--;
			
			//SysPrintf("GIF MFIFO qwc %d gif qwc %d, madr = %x, tadr = %x\n", qwc, gif->qwc, gif->madr, gif->tadr);
			gif->tadr = psHu32(DMAC_RBOR) + (gif->tadr & psHu32(DMAC_RBSR));
			if((gif->madr & ~psHu32(DMAC_RBSR)) == psHu32(DMAC_RBOR)){
				gif->madr = psHu32(DMAC_RBOR) + (gif->madr & psHu32(DMAC_RBSR));
				gifqwc -= gif->qwc;
			}
	 }

		if (mfifoGIFchain() == -1) {
			SysPrintf("dmaChain error %8.8x_%8.8x size=%d, id=%d, madr=%lx, tadr=%lx\n",
					ptag[1], ptag[0], gif->qwc, id, gif->madr, gif->tadr);
			gifdone = 1;
			INT(11,mfifocycles+g_gifCycles);
		}
		
		if ((gif->chcr & 0x80) && (ptag[0] >> 31)) {
#ifdef GIF_LOG
			GIF_LOG("dmaIrq Set\n");
#endif
			//SysPrintf("mfifoGIFtransfer: dmaIrq Set\n");
			//gifqwc = 0;
			gifdone = 1;
		}

//		if( (cpuRegs.interrupt & (1<<1)) && qwc > 0) {
//			SysPrintf("gif mfifo interrupt %d\n", qwc);
//		}
	//}

	/*if(gifdone == 1) {
		gifqwc = 0;
	}*/
	INT(11,mfifocycles+g_gifCycles);
	if(gifqwc == 0 && giftempqwc > 0) hwDmacIrq(14);
	
	//hwDmacIrq(1);
#ifdef SPR_LOG
	SPR_LOG("mfifoGIFtransfer end %x madr %x, tadr %x\n", gif->chcr, gif->madr, gif->tadr);
#endif
}

int gifMFIFOInterrupt()
{
	mfifocycles = 0;

	if(!(gif->chcr & 0x100)) return 1;
	else if(gifqwc == 0 && gifdone == 0) return 1;

	if(gifdone == 0 && gifqwc != 0) {
		mfifoGIFtransfer(0);
		if(gif->qwc > 0) return 1;
		else return 0;
	}
	if(gifdone == 0 || gif->qwc > 0) {
		SysPrintf("Shouldnt go here\n");
		return 1;
	}
	gifdone = 0;
	gif->chcr &= ~0x100;
	hwDmacIrq(DMAC_GIF);

	return 1;
}

DWORD WINAPI GSThreadProc(LPVOID lpParam)
{
	HANDLE handles[2] = { g_hGsEvent, g_hVuGSExit };
	//SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
	u8* writepos;
	u32 tag;
	u32 counter = 0;

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

	SysPrintf("Starting GS thread\n");

	while(1) {

		if( !CHECK_DUALCORE ) {
			if( WaitForMultipleObjects(2, handles, FALSE, INFINITE) == WAIT_OBJECT_0+1 ) {
				GSclose();
				return 0;
			}
		}
		else if( !(counter++ & 0xffff) ) {
			if( WaitForSingleObject(g_hVuGSExit, 0) == WAIT_OBJECT_0 ) {
				GSclose();
				return 0;
			}
		}

		if( g_pGSRingPos != g_pGSWritePos ) {

			do {
				writepos = *(volatile PU8*)&g_pGSWritePos;

				while( g_pGSRingPos != writepos ) {
					
					assert( g_pGSRingPos < GS_RINGBUFFEREND );
					// process until writepos
					tag = *(u32*)g_pGSRingPos;
					
					switch( tag&0xffff ) {
						case GS_RINGTYPE_RESTART:
							InterlockedExchangePointer(&g_pGSRingPos, GS_RINGBUFFERBASE);

							if( GS_RINGBUFFERBASE == writepos )
								goto ExitGS;

							continue;

						case GS_RINGTYPE_P1:
							MTGS_RECREAD(g_pGSRingPos+16, ((tag>>16)<<4));
							GSgifTransfer1((u32*)(g_pGSRingPos+16), 0);
							InterlockedExchangeAdd((PLONG)&g_pGSRingPos, 16 + ((tag>>16)<<4));
							break;

						case GS_RINGTYPE_P2:
							MTGS_RECREAD(g_pGSRingPos+16, ((tag>>16)<<4));
							GSgifTransfer2((u32*)(g_pGSRingPos+16), tag>>16);
							InterlockedExchangeAdd((PLONG)&g_pGSRingPos, 16 + ((tag>>16)<<4));
							break;
						case GS_RINGTYPE_P3:
							MTGS_RECREAD(g_pGSRingPos+16, ((tag>>16)<<4));
							GSgifTransfer3((u32*)(g_pGSRingPos+16), tag>>16);
							InterlockedExchangeAdd((PLONG)&g_pGSRingPos, 16 + ((tag>>16)<<4));
							break;
						case GS_RINGTYPE_VSYNC:
							GSvsync(*(int*)(g_pGSRingPos+4));
							InterlockedExchangeAdd((PLONG)&g_pGSRingPos, 16);
							break;

						case GS_RINGTYPE_FRAMESKIP:
							GSsetFrameSkip(*(int*)(g_pGSRingPos+4));
							InterlockedExchangeAdd((PLONG)&g_pGSRingPos, 16);
							break;
						case GS_RINGTYPE_MEMWRITE8:
							g_MTGSMem[*(int*)(g_pGSRingPos+4)] = *(u8*)(g_pGSRingPos+8);
							InterlockedExchangeAdd((PLONG)&g_pGSRingPos, 16);
							break;
						case GS_RINGTYPE_MEMWRITE16:
							*(u16*)(g_MTGSMem+*(int*)(g_pGSRingPos+4)) = *(u16*)(g_pGSRingPos+8);
							InterlockedExchangeAdd((PLONG)&g_pGSRingPos, 16);
							break;
						case GS_RINGTYPE_MEMWRITE32:
							*(u32*)(g_MTGSMem+*(int*)(g_pGSRingPos+4)) = *(u32*)(g_pGSRingPos+8);
							InterlockedExchangeAdd((PLONG)&g_pGSRingPos, 16);
							break;
						case GS_RINGTYPE_MEMWRITE64:
							*(u64*)(g_MTGSMem+*(int*)(g_pGSRingPos+4)) = *(u64*)(g_pGSRingPos+8);
							InterlockedExchangeAdd((PLONG)&g_pGSRingPos, 16);
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
								break;
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

							tag = (tag>>16) - (cpuRegs.cycle- *(u32*)(g_pGSRingPos+8));
							if( tag & 0x80000000 ) tag = 0;

							vif1Regs->stat&= ~0x1f000000;
							vif1ch->qwc = 0;

							INT(1, tag);
							InterlockedExchangeAdd((PLONG)&g_pGSRingPos, 16);
							break;
						}

						default:

							SysPrintf("GSThreadProc, bad packet writepos: %x g_pGSRingPos: %x, g_pGSWritePos: %x\n", writepos, g_pGSRingPos, g_pGSWritePos);
							assert(0);
							g_pGSRingPos = g_pGSWritePos;
							//flushall();
					}

					assert( g_pGSRingPos <= GS_RINGBUFFEREND );
					if( g_pGSRingPos == GS_RINGBUFFEREND )
						InterlockedExchangePointer(&g_pGSRingPos, GS_RINGBUFFERBASE);

					if( g_pGSRingPos == g_pGSWritePos ) {
						break;
					}
				}
ExitGS:
				;
			} while(g_pGSRingPos != *(volatile PU8*)&g_pGSWritePos);
		}

		// process vu1
	}

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

	int skipfirst = 0;

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

//				if( g_SaveGSStream != 3 )
//					return;
//
//				if( skipfirst ) {
//					++it;
//					it = packets.erase(packets.begin(), it);
//					skipfirst = 0;
//				}
//				
				it = packets.begin();
				continue;
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
