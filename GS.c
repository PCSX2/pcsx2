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

#include "Common.h"

#include <xmmintrin.h>

#include <assert.h>
#include "ir5900.h"
#include "VUmicro.h"

#ifdef WIN32_VIRTUAL_MEM
#define gif ((DMACh*)&PS2MEM_HW[0xA000])
#else
#define gif ((DMACh*)&psH[0xA000])
#endif

#define ENABLE_GS_CACHING_SIZE 0x4000 // min size
//#define _RINGBUF_DEBUG

#ifdef WIN32_VIRTUAL_MEM
#define PS2GS_BASE(mem) ((PS2MEM_BASE+0x12000000)+(mem&0x13ff))
#else
u8 g_RealGSMem[0x2000];
#define PS2GS_BASE(mem) (g_RealGSMem+(mem&0x13ff))
#endif

#ifdef GSCAPTURE
u32 g_loggs = 0;
u32 g_gstransnum = 0;
u32 g_gslimit = 0;
u32 g_gsfinalnum = 0;
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
__declspec(align(16)) static BYTE s_byRegs[3][16];

// could convert to pthreads easily, just don't have the time	
HANDLE g_hGsEvent = NULL, // set when path3 is ready to be processed
	g_hVuGSExit = NULL;		// set when thread needs to exit
HANDLE g_hGSOpen = NULL, g_hGSDone = NULL;
HANDLE g_hVuGSThread = NULL;
u32 CSRw;

HANDLE g_hAllGsReady[3] = {NULL};

DWORD WINAPI VuGSThreadProc(LPVOID lpParam);

// g_pGSRingPos == g_pGSWritePos => fifo is empty
u8* g_pGSRingPos = NULL, // cur pos ring is at
	*g_pGSWritePos = NULL; // cur pos ee thread is at
u8 *g_pGSTempWritePos = NULL, *g_pGSTempReadPos = NULL; // cur pos ee thread is at

extern int g_nCounters[];

extern void * memcpy_amd(void *dest, const void *src, size_t n);

CRITICAL_SECTION g_PageAddrSection;
u32 *g_pGSCurFreePages = NULL, *g_pGSWriteFreePages = NULL;

void gsInit()
{
	if( VirtualAlloc(GS_PAGEADDRS, GSPAGES_STRIDE*(0x10000000>>GS_SHIFT), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE) != GS_PAGEADDRS ) {
		MessageBox(NULL, "Cannot alloc GS page buffer\n", "Error", MB_OK);
		exit(0);
	}
	memset(GS_PAGEADDRS, 0, GSPAGES_STRIDE*(0x02000000>>GS_SHIFT));

	if( CHECK_MULTIGS ) {
		g_hGsEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

		g_hVuGSExit = CreateEvent(NULL, FALSE, FALSE, NULL);
		g_hGSOpen = CreateEvent(NULL, FALSE, FALSE, NULL);
		g_hGSDone = CreateEvent(NULL, FALSE, FALSE, NULL);

		SysPrintf("gsInit\n");

		g_pGSRingPos = VirtualAlloc(GS_RINGBUFFERBASE, GS_RINGBUFFERSIZE, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
		if( g_pGSRingPos != GS_RINGBUFFERBASE ) {
			MessageBox(NULL, "Cannot alloc GS ring buffer\n", "Error", MB_OK);
			exit(0);
		}

		g_pGSTempReadPos = g_pGSTempWritePos = VirtualAlloc(GS_RINGTEMPBASE, GS_RINGTEMPSIZE, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
		if( g_pGSTempWritePos != GS_RINGTEMPBASE ) {
			MessageBox(NULL, "Cannot alloc GS RingTemp buffer\n", "Error", MB_OK);
			exit(0);
		}

		InitializeCriticalSection(&g_PageAddrSection);

		InterlockedExchangePointer(&g_pGSWritePos, GS_RINGBUFFERBASE);
		g_hVuGSThread = CreateThread(NULL, 0, VuGSThreadProc, NULL, 0, NULL);
	}
}

void gsWaitGS()
{
	while( g_pGSRingPos != g_pGSWritePos )
		Sleep(1);
}

void gsShutdown()
{
	VirtualFree(GS_PAGEADDRS, GSPAGES_STRIDE*(0x02000000>>GS_SHIFT), MEM_DECOMMIT|MEM_RELEASE);

	if( CHECK_MULTIGS ) {

		SetEvent(g_hVuGSExit);
		SysPrintf("Closing gs thread\n");
		WaitForSingleObject(g_hVuGSThread, INFINITE);
		CloseHandle(g_hVuGSThread);
		CloseHandle(g_hGsEvent);
		CloseHandle(g_hVuGSExit);
		CloseHandle(g_hGSOpen);
		CloseHandle(g_hGSDone);

		g_pGSWriteFreePages = g_pGSCurFreePages = NULL;
		g_pGSTempReadPos = NULL;

		DeleteCriticalSection(&g_PageAddrSection);

		VirtualFree(GS_RINGBUFFERBASE, GS_RINGBUFFERSIZE, MEM_DECOMMIT|MEM_RELEASE);
		VirtualFree(GS_RINGTEMPBASE, GS_RINGTEMPSIZE, MEM_DECOMMIT|MEM_RELEASE);
	}
	else GSclose();
}

static void GSRingTempReserve(u32 size)
{
	u8* writepos = g_pGSTempWritePos;
	assert( size <= GS_RINGTEMPSIZE );

	// there always has to be memory
	if( g_pGSTempWritePos + size > GS_RINGTEMPEND ) {
		if( writepos < g_pGSTempReadPos ) {
			do {
				SetEvent(g_hGsEvent);
				Sleep(1);
			} while( writepos < g_pGSTempReadPos );
		}

		// notify GS
		writepos = GS_RINGTEMPBASE;
	}

	if( writepos < g_pGSTempReadPos && writepos+size >= g_pGSTempReadPos ) {
		do {
			SetEvent(g_hGsEvent);
			Sleep(1);
		} while( writepos < g_pGSTempReadPos && writepos+size >= g_pGSTempReadPos );
	}

	g_pGSTempWritePos = writepos;
}

static u8* GSRingTempCopy(u32 size)
{
	u8* writepos;
	assert( size <= GS_RINGTEMPSIZE );
	//GSRingTempReserve(size);

	writepos = g_pGSTempWritePos;
	g_pGSTempWritePos += size;
	assert( g_pGSTempWritePos <= GS_RINGTEMPEND );

	//SysPrintf("writepos copy: %x %d\n", writepos, size);
	return writepos;
}

typedef u8* PU8;

u8* GSRingBufCopy(void* mem, u32 size, u32 type)
{
	u8* writepos = g_pGSWritePos;
	u8* tempbuf;
	assert( size < 0x10000*16 );
	assert( writepos < GS_RINGBUFFEREND );
	assert( ((u32)writepos & 15) == 0 );
	assert( (size&15) == 0);

	size += 16;
	tempbuf = *(volatile PU8*)&g_pGSRingPos;
	if( writepos + size > GS_RINGBUFFEREND ) {
	
		// skip to beginning
		while( writepos < tempbuf || tempbuf == GS_RINGBUFFERBASE ) {
			SetEvent(g_hGsEvent);
			Sleep(1);
			tempbuf = *(volatile PU8*)&g_pGSRingPos;
		}

		// notify GS
		if( writepos != GS_RINGBUFFEREND ) {
			*(u32*)writepos = GS_RINGTYPE_RESTART;
		}

		writepos = GS_RINGBUFFERBASE;
	}

	while( writepos < tempbuf && (writepos+size >= g_pGSRingPos || (writepos+size == GS_RINGBUFFEREND && g_pGSRingPos == GS_RINGBUFFERBASE)) ) {
		SetEvent(g_hGsEvent);
		Sleep(1);
		tempbuf = *(volatile PU8*)&g_pGSRingPos;
	}

#ifdef WIN32_VIRTUAL_MEM
	if( ENABLE_GS_CACHING && size >= ENABLE_GS_CACHING_SIZE && mem != NULL && (u8*)mem < PS2MEM_BASE+0x10000000 ) {
		u32 val;
		u32 i;
		u32* page = GS_PAGEADDRS+(((u32)((u8*)mem-PS2MEM_BASE)&0x01ffffff)>>GS_SHIFT)*(GSPAGES_MEMADDRS+1);
		u32 numpages = (((u32)mem+size-1)>>GS_SHIFT) - ((u32)mem>>GS_SHIFT)+1;

		// inc all pages
		for(i = 0; i < numpages; ++i) {
			if( (u8)page[0] >= GSPAGES_MEMADDRS )
				break;

			val = InterlockedExchangeAdd(page, 0x40000001);

			assert( (u8)val < GSPAGES_MEMADDRS );

			// upper 2 bits are index
			page[(val>>30)+1] = (u32)writepos;
			page += GSPAGES_MEMADDRS+1;
		}

		if( i == numpages ) {
			// succeeded, so fill necessary data
			*(u32*)writepos = GS_RINGTYPE_MEMREF|type|(((size-16)>>4)<<16);
			*(u32*)(writepos+4) = (u32)mem;
			*(u32*)(writepos+8) = (u32)(page-(GSPAGES_MEMADDRS+1)*numpages);
			*(u32*)(writepos+12) = (u32)numpages;

			writepos += 16;

#ifndef _RINGBUF_DEBUG
			if( writepos == GS_RINGBUFFEREND ) writepos = GS_RINGBUFFERBASE;
			InterlockedExchangePointer(&g_pGSWritePos, writepos);

//			SetEvent(g_hGsEvent);
//			while( g_pGSWritePos != g_pGSRingPos ) Sleep(1);

			return NULL;
#else
			return writepos;
#endif
		}
		
		SysPrintf("not enough pages %x, %d\n", mem, size);

		// BUG, the interlocked exchange add should be done in a crit section!
		// dec the current pages
		while(i-- > 0) {
			page -= GSPAGES_MEMADDRS+1;
			InterlockedExchangeAdd(page, -0x40000001);
		} 

		// don't have enough mem, so fall through
	}
#endif

	// just copy
	*(u32*)writepos = type|(((size-16)>>4)<<16);
	return writepos+16;
}

void GSRingBufVSync(int field)
{
	u8* writepos = g_pGSWritePos;
	u8* tempbuf;

	assert( writepos + 16 <= GS_RINGBUFFEREND );

	tempbuf = *(volatile PU8*)&g_pGSRingPos;
	if( writepos < tempbuf && writepos+16 >= tempbuf ) {
		
		do {
			SetEvent(g_hGsEvent);
			Sleep(1);
			tempbuf = *(volatile PU8*)&g_pGSRingPos;
		} while(writepos < tempbuf && writepos+16 >= tempbuf );
	}

	*(u32*)writepos = GS_RINGTYPE_VSYNC;
	*(int*)(writepos+4) = field;

	writepos += 16;
	if( writepos == GS_RINGBUFFEREND ) writepos = GS_RINGBUFFERBASE;
	InterlockedExchangePointer(&g_pGSWritePos, writepos);

	if( !CHECK_DUALCORE )
		SetEvent(g_hGsEvent);
}

// called from gs thread or exception handler
static void GSFreeRingBuf(u8* pringbuf)
{
	u32 mem = *(u32*)(pringbuf+4) & ~0xfff;
	u32* pageaddr = *(u32**)(pringbuf+8);
	u32 pages = *(u32*)(pringbuf+12);
	u32 val, index;

	mem &= ~0xfff;

	while(pages-- > 0 ) {

		val = pageaddr[0];
		assert( (val&0xff) > 0 );

		index = (((val>>30)-(val&7))&3);

		if( pageaddr[ index + 1 ] != (u32)pringbuf ) {
			// deleting before others, so remove it from the entry
			for(val = 1; val < 5; ++val ) {
				if( pageaddr[val] == (u32)pringbuf ) {
					
					val--;

					// shift the rest
					while(index != val) {
						pageaddr[1+(val)%GSPAGES_MEMADDRS] = pageaddr[(val-1)%GSPAGES_MEMADDRS+1];
						val = (val-1)%GSPAGES_MEMADDRS;
					}

					break;
				}
			}
		}

		InterlockedExchangeAdd(pageaddr, -1);
		mem += 0x1000;
		pageaddr += GSPAGES_MEMADDRS+1;
	}
}

void GSFreePage(u32* page)
{
	u32 indices, num;
	u32 gssize;
	u8* gsmem, *ringmem;

	num = *(u32*)page;

	// count approximately the size needed to be allocated
	indices = ((num>>30)-(num&0xff))&3;
	num &= 0xff;
	gssize = 0;
	
	while(num-- > 0) {
		// first copy to a new memory
		ringmem = *(u8**)(page + 1 + indices);
		gssize += (*(u32*)ringmem >> 16) << 4;
		// go to the next index
		indices = (indices+1)%GSPAGES_MEMADDRS;
	}

	GSRingTempReserve(gssize);

	// lock section to start freeing
	EnterCriticalSection(&g_PageAddrSection);

	// check again since could have changed
	num = *(u32*)page;
	if( (u8)num ) {
		// note that values might be different
		indices = ((num>>30)-(num&0xff))&3;
		num &= 0xff;
		
		while(num-- > 0) {
			// first copy to a new memory
			ringmem = *(u8**)(page + 1 + indices);

			assert( !(*(u32*)ringmem & GS_RINGTYPE_MEMNOFREE) );

			gssize = (*(u32*)ringmem >> 16) << 4;
			gsmem = GSRingTempCopy(gssize);
			memcpy(gsmem, *(u8**)(ringmem+4), gssize);

			GSFreeRingBuf(ringmem);

			// indicate that using different mem
			*(u8**)(ringmem+4) = gsmem;
			*(u32*)ringmem |= GS_RINGTYPE_MEMNOFREE;

			// go to the next index
			indices = (indices+1)%GSPAGES_MEMADDRS;
		}
	}

	LeaveCriticalSection(&g_PageAddrSection);
}

void gsReset()
{
	SysPrintf("GIF reset\n");

	// GSDX crashes
	//if( GSreset ) GSreset();
	memset(GS_PAGEADDRS, 0, GSPAGES_STRIDE*(0x02000000>>GS_SHIFT));

	if( CHECK_MULTIGS ) {
		ResetEvent(g_hGsEvent);
		ResetEvent(g_hVuGSExit);

		g_pGSRingPos = g_pGSWritePos;
		g_pGSCurFreePages = g_pGSWriteFreePages;
		g_pGSTempReadPos = g_pGSTempWritePos = GS_RINGTEMPBASE;
	}

	memset(g_path, 0, sizeof(g_path));
	memset(s_byRegs, 0, sizeof(s_byRegs));

#ifndef WIN32_VIRTUAL_MEM
	memset(g_RealGSMem, 0, 0x2000);
#endif

	GSCSRr = 0x551B4000;   // Set the FINISH bit to 1 for now
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
//	else
//		GSreset();

	GSCSRr = 0x551B4000;   // Set the FINISH bit to 1 for now
	GSIMR = 0x7f00;
	psHu32(GIF_STAT) = 0;
	psHu32(GIF_CTRL) = 0;
	psHu32(GIF_MODE) = 0;
}

void CSRwrite(u32 value)
{	
	GSwriteCSR(value);

	GSCSRr = ((GSCSRr&~value)&0x1f)|(GSCSRr&~0x1f);

	if( value & 0x100 ) { // FLUSH
		//SysPrintf("GS_CSR FLUSH GS fifo: %x (CSRr=%x)\n", value, GSCSRr);
	}

	CSRw |= value & ~0x60;
	if (value & 0x200) { // resetGS
		//GSCSRr = 0x400E; // The host FIFO neeeds to be empty too or GSsync will fail (saqib)
		//GSIMR = 0xff00;
		if( GSgifSoftReset != NULL ) 
			GSgifSoftReset(7);
		else GSreset();

		GSCSRr = 0x551B4002;   // Set the FINISH bit to 1 - GS is always at a finish state as we don't have a FIFO(saqib)
	             //Since when!! Refraction, since 4/21/06 (zerofrog)
		GSIMR = 0x7F00; //This is bits 14-8 thats all that should be 1

		// and this too (fixed megaman ac)
		GSwriteCSR(GSCSRr);
	}
}

static void IMRwrite(u32 value) {
	GSIMR = (value & 0x1f00)|0x6000;
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
	}

#ifdef HW_LOG
	HW_LOG("GS write 8 at %8.8lx with data %8.8lx\n", mem, value);
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
			UpdateVSyncRate();
			break;
			
		case 0x12000020: // GS_SMODE2
			if(value & 0x1) Config.PsxType |= 2; // Interlaced
			else Config.PsxType &= ~2;	// Non-Interlaced
			*(u16*)PS2GS_BASE(mem) = value;
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
	}

#ifdef HW_LOG
	HW_LOG("GS write 16 at %8.8lx with data %8.8lx\n", mem, value);
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
			recSetSMODE1();
			break;
			
		case 0x12000020: // GS_SMODE2
			assert( !(mem&3));
			_eeMoveMMREGtoR(EAX, mmreg);
			_eeWriteConstMem16( (u32)PS2GS_BASE(mem), mmreg );
			recSetSMODE2();
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
			break;
		case 0x12000020: // GS_SMODE2
			if(value & 0x1) Config.PsxType |= 2; // Interlaced
			else Config.PsxType &= ~2;	// Non-Interlaced
			*(u32*)PS2GS_BASE(mem) = value;
			break;
			
		case 0x12001000: // GS_CSR
			CSRwrite(value);
			break;

		case 0x12001010: // GS_IMR
			IMRwrite(value);
			break;
		default:
			*(u32*)PS2GS_BASE(mem) = value;
	}

#ifdef HW_LOG
	HW_LOG("GS write 32 at %8.8lx with data %8.8lx\n", mem, value);
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
}
void gsConstWrite32(u32 mem, int mmreg) {

	switch (mem) {

		case 0x12000010: // GS_SMODE1
			_eeMoveMMREGtoR(EAX, mmreg);
			_eeWriteConstMem32( (u32)PS2GS_BASE(mem), mmreg );
			recSetSMODE1();
			break;

		case 0x12000020: // GS_SMODE2
			_eeMoveMMREGtoR(EAX, mmreg);
			_eeWriteConstMem32( (u32)PS2GS_BASE(mem), mmreg );
			recSetSMODE2();
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
			break;

		case 0x12000020: // GS_SMODE2
			if(value & 0x1) Config.PsxType |= 2; // Interlaced
			else Config.PsxType &= ~2;	// Non-Interlaced
			*(u64*)PS2GS_BASE(mem) = value;
			break;
		case 0x12001000: // GS_CSR
			CSRwrite((u32)value);
			break;

		case 0x12001010: // GS_IMR
			IMRwrite((u32)value);
			break;

		default:
			*(u64*)PS2GS_BASE(mem) = value;
	}

#ifdef HW_LOG
	HW_LOG("GS write 64 at %8.8lx with data %8.8lx_%8.8lx\n", mem, ((u32*)&value)[1], (u32)value);
#endif
}

void gsConstWrite64(u32 mem, int mmreg)
{
	switch (mem) {
		case 0x12000010: // GS_SMODE1
			_eeMoveMMREGtoR(EAX, mmreg);
			_eeWriteConstMem64((u32)PS2GS_BASE(mem), mmreg);
			recSetSMODE1();
			break;

		case 0x12000020: // GS_SMODE2
			_eeMoveMMREGtoR(EAX, mmreg);
			_eeWriteConstMem64((u32)PS2GS_BASE(mem), mmreg);
			recSetSMODE2();
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
			break;
	}
}

void gsConstWrite128(u32 mem, int mmreg)
{
	switch (mem) {
		case 0x12000010: // GS_SMODE1
			_eeMoveMMREGtoR(EAX, mmreg);
			_eeWriteConstMem128( (u32)PS2GS_BASE(mem), mmreg);
			recSetSMODE1();
			break;

		case 0x12000020: // GS_SMODE2
			_eeMoveMMREGtoR(EAX, mmreg);
			_eeWriteConstMem128( (u32)PS2GS_BASE(mem), mmreg);
			recSetSMODE2();
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
			break;
	}
}

u8 gsRead8(u32 mem)
{
#ifdef HW_LOG
	//HW_LOG("GS read 8 %8.8lx, at %8.8lx\n", *(u8*)(PS2MEM_BASE+(mem&~0xc00)), mem);
#endif

	return *(u8*)PS2GS_BASE(mem);
}

int gsConstRead8(u32 x86reg, u32 mem, u32 sign)
{
	_eeReadConstMem8(x86reg, (u32)PS2GS_BASE(mem), sign);
	return 0;
}

u16 gsRead16(u32 mem)
{
#ifdef HW_LOG
	//HW_LOG("GS read 16 %8.8lx, at %8.8lx\n", *(u16*)(PS2MEM_BASE+(mem&~0xc00)), mem);
#endif

	return *(u16*)PS2GS_BASE(mem);
}

int gsConstRead16(u32 x86reg, u32 mem, u32 sign)
{
	_eeReadConstMem16(x86reg, (u32)PS2GS_BASE(mem), sign);
	return 0;
}

u32 gsRead32(u32 mem) {

#ifdef HW_LOG
	//HW_LOG("GS read 32 %8.8lx, at %8.8lx\n", *(u32*)(PS2MEM_BASE+(mem&~0xc00)), mem);
#endif
	return *(u32*)PS2GS_BASE(mem);
}

int gsConstRead32(u32 x86reg, u32 mem)
{
	_eeReadConstMem32(x86reg, (u32)PS2GS_BASE(mem));
	return 0;
}

u64 gsRead64(u32 mem)
{
#ifdef HW_LOG
	//HW_LOG("GS read 64 %8.8lx (%8.8x), at %8.8lx\n", *(u64*)(PS2MEM_BASE+(mem&~0xc00)), mem);
#endif

	return *(u64*)PS2GS_BASE(mem);
}

void gsConstRead64(u32 mem, int mmreg)
{
	if( IS_XMMREG(mmreg) ) SSE_MOVLPS_M64_to_XMM(mmreg&0xff, (u32)PS2GS_BASE(mem));
	else {
		MOVQMtoR(mmreg, (u32)PS2GS_BASE(mem));
		SetMMXstate();
	}
}

void gsConstRead128(u32 mem, int xmmreg)
{
	_eeReadConstMem128( xmmreg, (u32)PS2GS_BASE(mem));
}


void gsIrq() {
	hwIntcIrq(0);
}

static void GSRegHandlerSIGNAL(u32* data)
{
	GSSIGLBLID->SIGID = (GSSIGLBLID->SIGID&~data[1])|(data[0]&data[1]);

	if (CSRw & 0x1) {
		GSCSRr |= 1; // signal
		//CSRw &= ~1;
	}
	if (!(GSIMR&0x100) )
		gsIrq();
}

static void GSRegHandlerFINISH(u32* data)
{
	if (CSRw & 0x2) {
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

// ffx pal, ntsc, jap
#define GSHACK_FFX (ElfCRC==0xbb3d833a||ElfCRC==0xa39517ab||ElfCRC==0x6A4EFE60)

// simulates a GIF tag
u32 GSgifTransferDummy(int path, u32 *pMem, u32 size)
{
	int nreg, i, nloop;
	u32 curreg;
	u32 tempreg;
	GIFTAG* ptag = &g_path[path];

#ifdef GSCAPTURE
	u32 tempcount = 0;
#endif

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

#ifdef GSCAPTURE
			if( g_loggs && g_gstransnum == 19 && tempcount++ > g_gslimit ) {
				break;
			}
#endif

			pMem+= 4;
			size--;

			if(ptag->nloop == 0 ) {
				if( path == 0 ) {
					// ffx hack
					if( GSHACK_FFX )
						continue;
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
					g_path[path] = *ptag;
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
					g_path[path] = *ptag;
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
					g_path[path] = *ptag;
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
#ifdef GS_LOG 
	GS_LOG("gsInterrupt: %8.8x\n", cpuRegs.cycle);
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

#ifdef GSCAPTURE
#define WRITINGDMA_TRANSFER(pMem, qwc) \
	extern u32 g_loggs, g_gstransnum, g_gsfinalnum; \
	if( !g_loggs || (g_loggs && g_gstransnum++ < g_gsfinalnum)) { \
		GSgifTransfer3(pMem, qwc-GSgifTransferDummy(2, pMem, qwc)); \
	} \

#else
#define WRITINGDMA_TRANSFER(pMem, qwc) GSgifTransfer3(pMem, qwc);
#endif

#define WRITERING_DMA(pMem, qwc) { \
	if( CHECK_MULTIGS) { \
		u8* pgsmem = GSRingBufCopy(pMem, (qwc)<<4, GS_RINGTYPE_P3); \
		if( pgsmem != NULL ) { \
			memcpy_amd(pgsmem, pMem, (qwc)<<4); \
			\
			pgsmem += (qwc)<<4; \
			assert( pgsmem <= GS_RINGBUFFEREND ); \
			if( pgsmem == GS_RINGBUFFEREND ) pgsmem = GS_RINGBUFFERBASE; \
			InterlockedExchangePointer(&g_pGSWritePos, pgsmem); \
			GSgifTransferDummy(2, pMem, qwc); \
		} \
		\
		if( !CHECK_DUALCORE ) SetEvent(g_hGsEvent); \
	} \
	else { \
		FreezeMMXRegs(1); \
		FreezeXMMRegs(1); \
		WRITINGDMA_TRANSFER(pMem, qwc); \
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

		SysPrintf("NULL GIFchain\n");
		return -1;
	}

	WRITERING_DMA(pMem, qwc);

	gif->madr+= qwc*16;
	gif->qwc = 0;
	return (qwc)*BIAS;
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

	if ((psHu32(DMAC_CTRL) & 0xC) == 0xC/* &&
		(gif->tadr == psHu32(0xe050))*/) { // GIF MFIFO
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

	if ((psHu32(DMAC_CTRL) & 0xC0) == 0x80) { // STD == GIF
		//SysPrintf("GS Stall Control Source = %x, Drain = %x\n", (psHu32(0xe000) >> 4) & 0x3, (psHu32(0xe000) >> 6) & 0x3);

		if( gif->madr >= psHu32(DMAC_STADR) ) {
			return;
		}
	}
	
	GSCSRr &= ~0xC000;  //Clear FIFO stuff
	GSCSRr |= 0x8000;   //FIFO full
	psHu32(GIF_STAT)|= 0xE00; // OPH=1 | APATH=3
	psHu32(GIF_STAT)|= 0x10000000; // FQC=31, hack ;)

	if( prevcycles != 0 ) {
		assert( prevtag != NULL );

		ptag = prevtag;
		// transfer interrupted, so continue
		GIFchain();

		if (gif->chcr & 0x80 && ptag[0] >> 31) {			 //Check TIE bit of CHCR and IRQ bit of tag
#ifdef GIF_LOG
			GIF_LOG("dmaIrq Set\n");
#endif					
			done = 1;
			
		}
	}

	// Transfer Dn_QWC from Dn_MADR to GIF
	if ((gif->chcr & 0xc) == 0 || gif->qwc > 0) { // Normal Mode
		//gscount++;
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
			cycles+=1; // Add 1 cycles from the QW read for the tag
			// Transfer dma tag if tte is set
			if (gif->chcr & 0x40) {
				//u32 temptag[4];
#ifdef GIF_LOG
				//SysPrintf("GIF TTE: %x_%x\n", ptag[3], ptag[2]);
#endif

//				temptag[2] = ptag[2];
//				temptag[3] = ptag[3];
//				GSgifTransfer3(temptag, 1); 
			}

			gif->chcr = ( gif->chcr & 0xFFFF ) | ( (*ptag) & 0xFFFF0000 );  //Transfer upper part of tag to CHCR bits 31-15
		
			id        = (ptag[0] >> 28) & 0x7;		//ID for DmaChain copied from bit 28 of the tag
			gif->qwc  = (u16)ptag[0];			    //QWC set to lower 16bits of the tag
			gif->madr = ptag[1];				    //MADR = ADDR field

#ifdef GIF_LOG
			GIF_LOG("dmaChain %8.8x_%8.8x size=%d, id=%d, addr=%lx\n",
					ptag[1], ptag[0], gif->qwc, id, gif->madr);
#endif
			
			done = hwDmacSrcChainWithStack(gif, id);

			if (!done && (psHu32(DMAC_CTRL) & 0xC0) == 0x80) { // STD == GIF
				// there are still bugs, need to also check if gif->madr +16*qwc >= stadr, if not, stall
				if( gif->madr >= psHu32(DMAC_STADR) ) {
					// stalled
					psHu32(GIF_STAT)&= ~0x1f000E00; // OPH=0 | APATH=0
					GSCSRr &= ~0xC000; //Clear FIFO stuff
					GSCSRr |= 0x4000;  //FIFO empty
					prevcycles = cycles;
					prevtag = ptag;
					hwDmacIrq(13);
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
	if ((gif->madr+gif->qwc*16) > (maddr+msize)) {
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
	//gif->madr = psHu32(DMAC_RBOR) + (gif->madr & psHu32(DMAC_RBSR));

	return 0;
}

int mfifoGIFchain() {
	u32 maddr = psHu32(DMAC_RBOR);
	int msize = psHu32(DMAC_RBSR)+16;
	u32 *pMem;

	/* Is QWC = 0? if so there is nothing to transfer */
	if (gif->qwc == 0) return 0;

	if (gif->madr >= maddr &&
		gif->madr < (maddr+msize)) {
		if (mfifoGIFrbTransfer() == -1) return -1;
	} else {
		pMem = (u32*)dmaGetAddr(gif->madr);
		if (pMem == NULL) return -1;

		WRITERING_DMA(pMem, gif->qwc);
		gif->madr+= gif->qwc*16;
	}

	mfifocycles+= (gif->qwc) * BIAS; /* guessing */
	gif->qwc = 0;

	return 0;
}

void mfifoGIFtransfer(int qwc) {
	u32 *ptag;
	int id;
	int done = 0;
	mfifocycles = 0;
#ifdef GIF_LOG
	GIF_LOG("mfifoGIFtransfer qwc=0x%x\n", qwc);
#endif
	if((gif->chcr & 0x100) == 0)SysPrintf("MFIFO GIF not ready %x\n", gif->chcr);

	while (qwc > 0 && done == 0) {  // Loop while Dn_CHCR.STR is 1
		gif->tadr = psHu32(DMAC_RBOR) + (gif->tadr & psHu32(DMAC_RBSR));
		ptag = (u32*)dmaGetAddr(gif->tadr);
 
		id        = (ptag[0] >> 28) & 0x7;
		gif->qwc  = (u16)ptag[0];
		gif->madr = ptag[1];
		mfifocycles += 2;

#ifdef GIF_LOG
		GIF_LOG("dmaChain %8.8x_%8.8x size=%d, id=%d, addr=%lx\n",
				ptag[1], ptag[0], gif->qwc, id, gif->madr);
#endif

//		if(gif->chcr & 0x40)
//			GSgifTransfer3(ptag, 1);

		switch (id) {
			case 0: // refe
				gif->tadr += 16;
				qwc = 0;
				INT(11,mfifocycles);
				done = 1;
				break;
 
			case 1: // cnt
				gif->madr = gif->tadr + 16;
				qwc-= gif->qwc + 1;
				// Set the taddr to the next tag
				gif->tadr += 16 + (gif->qwc * 16);
				break;
 
			case 3: // ref
			case 4: // refs
				gif->tadr += 16;
				qwc--;
				break;
 
			case 7: // end
				gif->madr = gif->tadr + 16;
				gif->tadr = gif->madr + (gif->qwc * 16);
				qwc = 0;
				INT(11,mfifocycles);
				done = 1;
				break;
		}

		if (mfifoGIFchain() == -1) {
			break;
		}
		
		if ((gif->chcr & 0x80) && (ptag[0] >> 31)) {
#ifdef GIF_LOG
			GIF_LOG("dmaIrq Set\n");
#endif
			INT(11,mfifocycles);
			//SysPrintf("mfifoGIFTransfer: dmaIrq Set\n");
			//qwc = 0;
			
			done = 1;
		}
	}
	if(done == 0 && qwc == 0)hwDmacIrq(14);
	/*if(qwc == 0 && gif->chcr & 0x100) {
		hwDmacIrq(14);
		return;
	}*/
	//INT(11,cycles);
	//hwDmacIrq(2);
	
}

int gifMFIFOInterrupt() {
	mfifocycles = 0;
	gif->chcr &= ~0x100;
	hwDmacIrq(DMAC_GIF);
	
	return 1;
}

extern long pDsp;
DWORD WINAPI VuGSThreadProc(LPVOID lpParam)
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
		GSCSRr = 0x551B4000; // 0x55190000
		SysPrintf("gsOpen done\n");
		if (ret != 0) { SysMessage (_("Error Opening GS Plugin")); return -1; }
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
		else if( (counter++ & 0xffff) == 0 ) {
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
							GSgifTransfer1((u32*)(g_pGSRingPos+16), 0);
							InterlockedExchangeAdd((PLONG)&g_pGSRingPos, 16 + ((tag>>16)<<4));
							break;

						case GS_RINGTYPE_P2:
							GSgifTransfer2((u32*)(g_pGSRingPos+16), tag>>16);
							InterlockedExchangeAdd((PLONG)&g_pGSRingPos, 16 + ((tag>>16)<<4));
							break;
						case GS_RINGTYPE_P3:
							GSgifTransfer3((u32*)(g_pGSRingPos+16), tag>>16);
							InterlockedExchangeAdd((PLONG)&g_pGSRingPos, 16 + ((tag>>16)<<4));
							break;
						case GS_RINGTYPE_VSYNC:
							GSvsync(*(int*)(g_pGSRingPos+4));
							InterlockedExchangeAdd((PLONG)&g_pGSRingPos, 16);
							break;

#ifdef GS_ENABLE_CACHING
						case GS_RINGTYPE_P2|GS_RINGTYPE_MEMREF:
						case GS_RINGTYPE_P2|GS_RINGTYPE_MEMREF|GS_RINGTYPE_MEMNOFREE:
							EnterCriticalSection(&g_PageAddrSection);

							// read again since could have changed
							tag = *(u32*)g_pGSRingPos;

							GSgifTransfer2(*(u32**)(g_pGSRingPos+4), tag>>16);

							if( ENABLE_GS_CACHING ) {
								if( !(tag & GS_RINGTYPE_MEMNOFREE) ) {
									GSFreeRingBuf(g_pGSRingPos);
								}
								else {
									// using temp memory, so release
									g_pGSTempReadPos = *(u8**)(g_pGSRingPos+4) + ((tag>>16)<<4);
								}
							}

							LeaveCriticalSection(&g_PageAddrSection);

#ifdef _RINGBUF_DEBUG
							InterlockedExchangeAdd((PLONG)&g_pGSRingPos, 16 + ((tag>>16)<<4));
#else
							InterlockedExchangeAdd((PLONG)&g_pGSRingPos, 16);
#endif

							break;
						case GS_RINGTYPE_P3|GS_RINGTYPE_MEMREF:
						case GS_RINGTYPE_P3|GS_RINGTYPE_MEMREF|GS_RINGTYPE_MEMNOFREE:

							EnterCriticalSection(&g_PageAddrSection);

							// read again since could have changed
							tag = *(u32*)g_pGSRingPos;

#ifdef _RINGBUF_DEBUG
							assert( memcmp(*(u32**)(g_pGSRingPos+4), g_pGSRingPos+16, (tag>>16)*16) == 0 );
#endif
							GSgifTransfer3(*(u32**)(g_pGSRingPos+4), tag>>16);

							if( ENABLE_GS_CACHING ) {
								if( !(tag & GS_RINGTYPE_MEMNOFREE) ) {
									GSFreeRingBuf(g_pGSRingPos);
								}
								else {
									// using temp memory, so release
									g_pGSTempReadPos = *(u8**)(g_pGSRingPos+4) + ((tag>>16)<<4);
								}
							}
							
							LeaveCriticalSection(&g_PageAddrSection);

#ifdef _RINGBUF_DEBUG
							InterlockedExchangeAdd((PLONG)&g_pGSRingPos, 16 + ((tag>>16)<<4));
#else
							InterlockedExchangeAdd((PLONG)&g_pGSRingPos, 16);
#endif						
							break;

#endif // end GS_ENABLE_CACHING

						case GS_RINGTYPE_VIFFIFO:
						{
							u64* pMem;
							assert( vif1ch->madr == *(u32*)(g_pGSRingPos+4) );
							assert( vif1ch->qwc == (tag>>16) );

							pMem = (u64*)dmaGetAddr(vif1ch->madr);

							if (pMem == NULL) {
								psHu32(DMAC_STAT)|= 1<<15;
								break;
							}
							GSreadFIFO2(pMem, tag>>16);

							// set incase read
							psHu64(0x5000) = pMem[2*(tag>>16)-2];
							psHu64(0x5008) = pMem[2*(tag>>16)-1];

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

							SysPrintf("VsGSThreadProc, bad packet\n");
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

#undef GIFchain