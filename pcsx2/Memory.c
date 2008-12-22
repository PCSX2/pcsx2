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

/*

RAM
---
0x00100000-0x01ffffff this is the physical address for the ram.its cached there
0x20100000-0x21ffffff uncached 
0x30100000-0x31ffffff uncached & acceleretade 
0xa0000000-0xa1ffffff MIRROR might...???
0x80000000-0x81ffffff MIRROR might... ????  

scratch pad
----------
0x70000000-0x70003fff scratch pad

BIOS
----
0x1FC00000 - 0x1FFFFFFF un-cached
0x9FC00000 - 0x9FFFFFFF cached
0xBFC00000 - 0xBFFFFFFF un-cached
*/

//////////
// Rewritten by zerofrog(@gmail.com) to add os virtual memory
//////////


#if _WIN32_WINNT < 0x0500
#define _WIN32_WINNT 0x0500
#endif

#pragma warning(disable:4799) // No EMMS at end of function

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <sys/stat.h>

#ifndef PCSX2_VIRTUAL_MEM
#include <vector>
#endif
#include "Common.h"

#ifdef PCSX2_NORECBUILD
#define REC_CLEARM(mem)
#else
#include "iR5900.h"
#endif

#include "PsxMem.h"
#include "R3000A.h"
#include "PsxHw.h"
#include "VUmicro.h"
#include "GS.h"
#include "vtlb.h"

#ifdef ENABLECACHE
#include "Cache.h"
#endif

#include <assert.h>

#ifdef __LINUX__
#include <sys/mman.h>
#endif
extern u32 maxrecmem;
extern int rdram_devices, rdram_sdevid;

#ifndef __x86_64__
extern void * memcpy_fast(void *dest, const void *src, size_t n);
#endif

//#define FULLTLB
int MemMode = 0;		// 0 is Kernel Mode, 1 is Supervisor Mode, 2 is User Mode

u16 ba0R16(u32 mem) {
	//MEM_LOG("ba00000 Memory read16 address %x\n", mem);

	
	if (mem == 0x1a000006) {
		static int ba6;
		ba6++;
		if (ba6 == 3) ba6 = 0;
		return ba6;
	}
	return 0;
}

/////////////////////////////
// VIRTUAL MEM START 
/////////////////////////////
#ifdef PCSX2_VIRTUAL_MEM

PSMEMORYBLOCK s_psM = {0}, s_psHw = {0}, s_psS = {0}, s_psxM = {0}, s_psVuMem = {0};

#define PHYSICAL_ALLOC(ptr, size, block) { \
	if(SysPhysicalAlloc(size, &block) == -1 ) \
		goto eCleanupAndExit; \
	if(SysVirtualPhyAlloc((void*)ptr, size, &block) == -1) \
		goto eCleanupAndExit; \
} \

#define PHYSICAL_FREE(ptr, size, block) { \
	SysVirtualFree(ptr, size); \
	SysPhysicalFree(&block); \
} \

#ifdef _WIN32 // windows implementation of vm

static PSMEMORYMAP initMemoryMap(uptr* aPFNs, uptr* aVFNs)
{
	PSMEMORYMAP m;
	m.aPFNs = aPFNs;
	m.aVFNs = aVFNs;
	return m;
}

// only do vm hack for release
#ifndef PCSX2_DEVBUILD
#define VM_HACK
#endif

// virtual memory blocks
PSMEMORYMAP *memLUT = NULL;

#define VIRTUAL_ALLOC(base, size, Protection) { \
	LPVOID lpMemReserved = VirtualAlloc( base, size, MEM_RESERVE|MEM_COMMIT, Protection ); \
	if( lpMemReserved == NULL || base != lpMemReserved ) \
	{ \
		SysPrintf("Cannot reserve memory at 0x%8.8x(%x), error: %d.\n", base, lpMemReserved, GetLastError()); \
		goto eCleanupAndExit; \
	} \
} \

#define VIRTUAL_FREE(ptr, size) { \
	VirtualFree(ptr, size, MEM_DECOMMIT); \
	VirtualFree(ptr, 0, MEM_RELEASE); \
} \

int memInit() {

	int i;
	LPVOID pExtraMem = NULL;	

	// release the previous reserved mem
	VirtualFree(PS2MEM_BASE, 0, MEM_RELEASE);

	// allocate all virtual memory
	PHYSICAL_ALLOC(PS2MEM_BASE, 0x02000000, s_psM);
	VIRTUAL_ALLOC(PS2MEM_ROM, 0x00400000, PAGE_READONLY);
	VIRTUAL_ALLOC(PS2MEM_ROM1, 0x00040000, PAGE_READONLY);
	VIRTUAL_ALLOC(PS2MEM_ROM2, 0x00080000, PAGE_READONLY);
	VIRTUAL_ALLOC(PS2MEM_EROM, 0x001C0000, PAGE_READONLY);
	PHYSICAL_ALLOC(PS2MEM_SCRATCH, 0x00010000, s_psS);
	PHYSICAL_ALLOC(PS2MEM_HW, 0x00010000, s_psHw);
	PHYSICAL_ALLOC(PS2MEM_PSX, 0x00200000, s_psxM);
	PHYSICAL_ALLOC(PS2MEM_VU0MICRO, 0x00010000, s_psVuMem);

	VIRTUAL_ALLOC(PS2MEM_PSXHW, 0x00010000, PAGE_READWRITE);
	//VIRTUAL_ALLOC(PS2MEM_PSXHW2, 0x00010000, PAGE_READWRITE);
	VIRTUAL_ALLOC(PS2MEM_PSXHW4, 0x00010000, PAGE_NOACCESS);
	VIRTUAL_ALLOC(PS2MEM_GS, 0x00002000, PAGE_READWRITE);
	VIRTUAL_ALLOC(PS2MEM_DEV9, 0x00010000, PAGE_NOACCESS);
	VIRTUAL_ALLOC(PS2MEM_SPU2, 0x00010000, PAGE_NOACCESS);
	VIRTUAL_ALLOC(PS2MEM_SPU2_, 0x00010000, PAGE_NOACCESS);

	VIRTUAL_ALLOC(PS2MEM_B80, 0x00010000, PAGE_READWRITE);
	VIRTUAL_ALLOC(PS2MEM_BA0, 0x00010000, PAGE_READWRITE);

	// reserve the left over 224Mb, don't map
	pExtraMem = VirtualAlloc(PS2MEM_BASE+0x02000000, 0x0e000000, MEM_RESERVE|MEM_PHYSICAL, PAGE_READWRITE);
	if( pExtraMem != PS2MEM_BASE+0x02000000 )
		goto eCleanupAndExit;

	// reserve left over psx mem
	pExtraMem = VirtualAlloc(PS2MEM_PSX+0x00200000, 0x00600000, MEM_RESERVE|MEM_PHYSICAL, PAGE_READWRITE);
	if( pExtraMem != PS2MEM_PSX+0x00200000 )
		goto eCleanupAndExit;

	// reserve gs mem
	pExtraMem = VirtualAlloc(PS2MEM_BASE+0x20000000, 0x10000000, MEM_RESERVE|MEM_PHYSICAL, PAGE_READWRITE);
	if( pExtraMem != PS2MEM_BASE+0x20000000 )
		goto eCleanupAndExit;
	
	// special addrs mmap
	VIRTUAL_ALLOC(PS2MEM_BASE+0x5fff0000, 0x10000, PAGE_READWRITE);

	// alloc virtual mappings
	memLUT = (PSMEMORYMAP*)_aligned_malloc(0x100000 * sizeof(PSMEMORYMAP), 16);
	memset(memLUT, 0, sizeof(PSMEMORYMAP)*0x100000);
	for (i=0; i<0x02000; i++) memLUT[i + 0x00000] = initMemoryMap(&s_psM.aPFNs[i], &s_psM.aVFNs[i]);
	for (i=2; i<0x00010; i++) memLUT[i + 0x10000] = initMemoryMap(&s_psHw.aPFNs[i], &s_psHw.aVFNs[i]);
	for (i=0; i<0x00800; i++) memLUT[i + 0x1c000] = initMemoryMap(&s_psxM.aPFNs[(i & 0x1ff)], &s_psxM.aVFNs[(i & 0x1ff)]);
	for (i=0; i<0x00004; i++) memLUT[i + 0x11000] = initMemoryMap(&s_psVuMem.aPFNs[0], &s_psVuMem.aVFNs[0]);
	for (i=0; i<0x00004; i++) memLUT[i + 0x11004] = initMemoryMap(&s_psVuMem.aPFNs[1], &s_psVuMem.aVFNs[1]);
	for (i=0; i<0x00004; i++) memLUT[i + 0x11008] = initMemoryMap(&s_psVuMem.aPFNs[4+i], &s_psVuMem.aVFNs[4+i]);
	for (i=0; i<0x00004; i++) memLUT[i + 0x1100c] = initMemoryMap(&s_psVuMem.aPFNs[8+i], &s_psVuMem.aVFNs[8+i]);

	for (i=0; i<0x00004; i++) memLUT[i + 0x50000] = initMemoryMap(&s_psS.aPFNs[i], &s_psS.aVFNs[i]);

	// map to other modes
	memcpy(memLUT+0x80000, memLUT, 0x20000*sizeof(PSMEMORYMAP));
	memcpy(memLUT+0xa0000, memLUT, 0x20000*sizeof(PSMEMORYMAP));

	if (psxInit() == -1)
		goto eCleanupAndExit;

	return 0;

eCleanupAndExit:
	if( pExtraMem != NULL )
		VirtualFree(pExtraMem, 0x0e000000, MEM_RELEASE);
	memShutdown();
	return -1;
}

void memShutdown()
{
	VirtualFree(PS2MEM_BASE+0x02000000, 0, MEM_RELEASE);
	VirtualFree(PS2MEM_PSX+0x00200000, 0, MEM_RELEASE);
	VirtualFree(PS2MEM_BASE+0x20000000, 0, MEM_RELEASE);

	PHYSICAL_FREE(PS2MEM_BASE, 0x02000000, s_psM);
	VIRTUAL_FREE(PS2MEM_ROM, 0x00400000);
	VIRTUAL_FREE(PS2MEM_ROM1, 0x00080000);
	VIRTUAL_FREE(PS2MEM_ROM2, 0x00080000);
	VIRTUAL_FREE(PS2MEM_EROM, 0x001C0000);
	PHYSICAL_FREE(PS2MEM_SCRATCH, 0x00010000, s_psS);
	PHYSICAL_FREE(PS2MEM_HW, 0x00010000, s_psHw);
	PHYSICAL_FREE(PS2MEM_PSX, 0x00200000, s_psxM);
	PHYSICAL_FREE(PS2MEM_VU0MICRO, 0x00010000, s_psVuMem);

	VIRTUAL_FREE(PS2MEM_VU0MICRO, 0x00010000); // allocate for all VUs

	VIRTUAL_FREE(PS2MEM_PSXHW, 0x00010000);
	//VIRTUAL_FREE(PS2MEM_PSXHW2, 0x00010000);
	VIRTUAL_FREE(PS2MEM_PSXHW4, 0x00010000);
	VIRTUAL_FREE(PS2MEM_GS, 0x00010000);
	VIRTUAL_FREE(PS2MEM_DEV9, 0x00010000);
	VIRTUAL_FREE(PS2MEM_SPU2, 0x00010000);
	VIRTUAL_FREE(PS2MEM_SPU2_, 0x00010000);

	VIRTUAL_FREE(PS2MEM_B80, 0x00010000);
	VIRTUAL_FREE(PS2MEM_BA0, 0x00010000);

	VirtualFree(PS2MEM_VU0MICRO, 0, MEM_RELEASE);

	_aligned_free(memLUT); memLUT = NULL;

	// reserve mem
	VirtualAlloc(PS2MEM_BASE, 0x40000000, MEM_RESERVE, PAGE_NOACCESS);
}

//NOTE: A lot of the code reading depends on the registers being less than 8
// MOV8 88/8A
// MOV16 6689
// MOV32 89/8B
// SSEMtoR64 120f
// SSERtoM64 130f
// SSEMtoR128 280f
// SSERtoM128 290f

#define SKIP_WRITE() { \
	switch(code&0xff) { \
		case 0x88: \
			if( !(code&0x8000) ) goto DefaultHandler; \
			ContextRecord->Eip += 6; \
			break; \
		case 0x66: \
			assert( code&0x800000 ); \
			assert( (code&0xffff) == 0x8966 ); \
			ContextRecord->Eip += 7; \
			break; \
		case 0x89: \
			assert( code&0x8000 ); \
			ContextRecord->Eip += 6; \
			break; \
		case 0x0f: /* 130f, 230f*/ \
			assert( (code&0xffff) == 0x290f || (code&0xffff) == 0x130f ); \
			assert( code&0x800000 ); \
			ContextRecord->Eip += 7; \
			break; \
		default: \
			goto DefaultHandler; \
	} \
} \

#define SKIP_READ() { \
	switch(code&0xff) { \
		case 0x8A: \
			if( !(code&0x8000) ) goto DefaultHandler; \
			ContextRecord->Eip += 6; \
			rd = (code>>(8+3))&7; \
			break; \
		case 0x66: \
			if( (code&0x07000000) == 0x05000000 ) ContextRecord->Eip += 8; /* 8 for mem reads*/ \
			else ContextRecord->Eip += 4 + ((code&0x1f000000) == 0x0c000000) + !!(code&0x40000000); \
			rd = (code>>(24+3))&7; \
			break; \
		case 0x8B: \
			if( !(code&0x8000) ) goto DefaultHandler; \
			ContextRecord->Eip += 6; \
			rd = (code>>(8+3))&7; \
			break; \
		case 0x0f: { \
			assert( (code&0xffff)==0x120f || (code&0xffff)==0x280f || (code&0xffff) == 0xb60f || (code&0xffff) == 0xb70f ); \
			if( !(code&0x800000) ) goto DefaultHandler; \
			ContextRecord->Eip += 7; \
			rd = (code>>(16+3))&7; \
			break; } \
		default: \
			goto DefaultHandler; \
	} \
} \

int SysPageFaultExceptionFilter(EXCEPTION_POINTERS* eps)
{
	struct _EXCEPTION_RECORD* ExceptionRecord = eps->ExceptionRecord;
	struct _CONTEXT* ContextRecord = eps->ContextRecord;

	u32 addr;

	C_ASSERT(sizeof(ContextRecord->Eax) == 4);

	// If the exception is not a page fault, exit.
	if (ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}

	// get bad virtual address
	addr = (u32)ExceptionRecord->ExceptionInformation[1];

	if( (unsigned)(addr-(u32)PS2MEM_BASE) < 0x60000000) {
		PSMEMORYMAP* pmap;
		
		pmap = &memLUT[(addr-(u32)PS2MEM_BASE)>>12];
		
		if( !pmap->aPFNs ) {
			// NOTE: this is a hack because the address is truncated and there's no way
			// to tell what it's upper bits are (due to OS limitations).
			pmap += 0x80000;
			if( !pmap->aPFNs ) {
				pmap += 0x20000;
				if( !pmap->aPFNs ) goto OtherException;
			}
			//else addr += 0x20000000;
		}

		{
			//LPVOID pnewaddr; not used
			uptr curvaddr = pmap->aVFNs[0];

			if( curvaddr ) {
				// delete the current mapping
				SysMapUserPhysicalPages((void*)curvaddr, 1, NULL, 0);
			}

			assert( pmap->aPFNs[0] != 0 );

			pmap->aVFNs[0] = curvaddr = addr&~0xfff;
			if( SysMapUserPhysicalPages((void*)curvaddr, 1, pmap->aPFNs, 0) )
				return EXCEPTION_CONTINUE_EXECUTION;

			// try allocing the virtual mem
			//pnewaddr = <- not used
			/* use here the size of allocation granularity and force rounding down,
			   because in reserve mode the address is rounded up/down to the nearest
			   multiple of this granularity; if you did it not this way, in some cases
			   the same address would be used twice, so the api fails */
			VirtualAlloc((void*)(curvaddr&~0xffff), 0x10000, MEM_RESERVE|MEM_PHYSICAL, PAGE_READWRITE);

			if( SysMapUserPhysicalPages((void*)curvaddr, 1, pmap->aPFNs, 0) )
				return EXCEPTION_CONTINUE_EXECUTION;

			SysPrintf("Fatal error, virtual page 0x%x cannot be found %d (p:%x,v:%x)\n",
				addr-(u32)PS2MEM_BASE, GetLastError(), pmap->aPFNs[0], curvaddr);
		}
	}
	// check if vumem
	else if( (addr&0xffff4000) == 0x11000000 ) {
		// vu0mem
		SysMapUserPhysicalPages((void*)s_psVuMem.aVFNs[1], 1, NULL, 0);
		
		s_psVuMem.aVFNs[1] = addr&~0xfff;
		SysMapUserPhysicalPages((void*)addr, 1, s_psVuMem.aPFNs, 1);

		//SysPrintf("Exception: vumem\n");
		return EXCEPTION_CONTINUE_EXECUTION;
	}
OtherException:

#ifdef VM_HACK
	{
		u32 code = *(u32*)ExceptionRecord->ExceptionAddress;
		u32 rd = 0;

		if( ExceptionRecord->ExceptionInformation[0] ) {
			//SKIP_WRITE();
			// shouldn't be writing
			SysPrintf("Exception: Write\n"); // Naruto Ultimate Ninja 3 crashes right after a write!
		}
		else {
			SysPrintf("vmhack ");
			SKIP_READ();
			//((u32*)&ContextRecord->Eax)[rd] = 0;
			return EXCEPTION_CONTINUE_EXECUTION; // TODO: verify this!!!
		}
	}
DefaultHandler:
#endif

	return EXCEPTION_CONTINUE_SEARCH;
}

#else // linux implementation

#define VIRTUAL_ALLOC(base, size, Protection) { \
    void* lpMemReserved = mmap( base, size, Protection, MAP_PRIVATE|MAP_ANONYMOUS ); \
	if( lpMemReserved == NULL || base != lpMemReserved ) \
	{ \
		SysPrintf("Cannot reserve memory at 0x%8.8x(%x).\n", base, lpMemReserved); \
        perror("err"); \
		goto eCleanupAndExit; \
	} \
} \

#define VIRTUAL_FREE(ptr, size) munmap(ptr, size)

uptr *memLUT = NULL;

int memInit()
{
int i;
	LPVOID pExtraMem = NULL;	

	// release the previous reserved mem
    munmap(PS2MEM_BASE, 0x40000000);

	// allocate all virtual memory
	PHYSICAL_ALLOC(PS2MEM_BASE, 0x02000000, s_psM);
	VIRTUAL_ALLOC(PS2MEM_ROM, 0x00400000, PROT_READ);
	VIRTUAL_ALLOC(PS2MEM_ROM1, 0x00040000, PROT_READ);
	VIRTUAL_ALLOC(PS2MEM_ROM2, 0x00080000, PROT_READ);
	VIRTUAL_ALLOC(PS2MEM_EROM, 0x001C0000, PROT_READ);
	PHYSICAL_ALLOC(PS2MEM_SCRATCH, 0x00010000, s_psS);
	PHYSICAL_ALLOC(PS2MEM_HW, 0x00010000, s_psHw);
	PHYSICAL_ALLOC(PS2MEM_PSX, 0x00200000, s_psxM);
	PHYSICAL_ALLOC(PS2MEM_VU0MICRO, 0x00010000, s_psVuMem);

	VIRTUAL_ALLOC(PS2MEM_PSXHW, 0x00010000, PROT_READ|PROT_WRITE);
	VIRTUAL_ALLOC(PS2MEM_PSXHW4, 0x00010000, PROT_NONE);
	VIRTUAL_ALLOC(PS2MEM_GS, 0x00002000, PROT_READ|PROT_WRITE);
	VIRTUAL_ALLOC(PS2MEM_DEV9, 0x00010000, PROT_NONE);
	VIRTUAL_ALLOC(PS2MEM_SPU2, 0x00010000, PROT_NONE);
	VIRTUAL_ALLOC(PS2MEM_SPU2_, 0x00010000, PROT_NONE);

	VIRTUAL_ALLOC(PS2MEM_B80, 0x00010000, PROT_READ|PROT_WRITE);
	VIRTUAL_ALLOC(PS2MEM_BA0, 0x00010000, PROT_READ|PROT_WRITE);

	// special addrs mmap
	VIRTUAL_ALLOC(PS2MEM_BASE+0x5fff0000, 0x10000, PROT_READ|PROT_WRITE);

	// alloc virtual mappings
	memLUT = (PSMEMORYMAP*)_aligned_malloc(0x100000 * sizeof(uptr), 16);
	memset(memLUT, 0, sizeof(uptr)*0x100000);
	for (i=0; i<0x02000; i++) memLUT[i + 0x00000] = PS2MEM_BASE+(i<<12);
	for (i=2; i<0x00010; i++) memLUT[i + 0x10000] = PS2MEM_BASE+0x10000000+(i<<12);
	for (i=0; i<0x00800; i++) memLUT[i + 0x1c000] = PS2MEM_BASE+0x1c000000+(i<<12);
	for (i=0; i<0x00004; i++) memLUT[i + 0x11000] = PS2MEM_VU0MICRO;
	for (i=0; i<0x00004; i++) memLUT[i + 0x11004] = PS2MEM_VU0MEM;
	for (i=0; i<0x00004; i++) memLUT[i + 0x11008] = PS2MEM_VU1MICRO+(i<<12);
	for (i=0; i<0x00004; i++) memLUT[i + 0x1100c] = PS2MEM_VU1MEM+(i<<12);

	for (i=0; i<0x00004; i++) memLUT[i + 0x50000] = PS2MEM_SCRATCH+(i<<12);

	// map to other modes
	memcpy(memLUT+0x80000, memLUT, 0x20000*sizeof(uptr));
	memcpy(memLUT+0xa0000, memLUT, 0x20000*sizeof(uptr));

	if (psxInit() == -1)
		goto eCleanupAndExit;

eCleanupAndExit:
	memShutdown();
	return -1;
}

void memShutdown()
{
	VIRTUAL_FREE(PS2MEM_BASE, 0x40000000);
	VIRTUAL_FREE(PS2MEM_PSX, 0x00800000);

	PHYSICAL_FREE(PS2MEM_BASE, 0x02000000, s_psM);
	VIRTUAL_FREE(PS2MEM_ROM, 0x00400000);
	VIRTUAL_FREE(PS2MEM_ROM1, 0x00080000);
	VIRTUAL_FREE(PS2MEM_ROM2, 0x00080000);
	VIRTUAL_FREE(PS2MEM_EROM, 0x001C0000);
	PHYSICAL_FREE(PS2MEM_SCRATCH, 0x00010000, s_psS);
	PHYSICAL_FREE(PS2MEM_HW, 0x00010000, s_psHw);
	PHYSICAL_FREE(PS2MEM_PSX, 0x00800000, s_psxM);
	PHYSICAL_FREE(PS2MEM_VU0MICRO, 0x00010000, s_psVuMem);

	VIRTUAL_FREE(PS2MEM_VU0MICRO, 0x00010000); // allocate for all VUs

	VIRTUAL_FREE(PS2MEM_PSXHW, 0x00010000);
	VIRTUAL_FREE(PS2MEM_PSXHW4, 0x00010000);
	VIRTUAL_FREE(PS2MEM_GS, 0x00010000);
	VIRTUAL_FREE(PS2MEM_DEV9, 0x00010000);
	VIRTUAL_FREE(PS2MEM_SPU2, 0x00010000);
	VIRTUAL_FREE(PS2MEM_SPU2_, 0x00010000);

	VIRTUAL_FREE(PS2MEM_B80, 0x00010000);
	VIRTUAL_FREE(PS2MEM_BA0, 0x00010000);

	VirtualFree(PS2MEM_VU0MICRO, 0, MEM_RELEASE);

	_aligned_free(memLUT); memLUT = NULL;

	// reserve mem
    if( mmap(PS2MEM_BASE, 0x40000000, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, 0, 0) != PS2MEM_BASE ) {
        SysPrintf("failed to reserve mem\n");
    }
}

#endif // _WIN32

// Some games read/write between different addrs but same physical memory
// this causes major slowdowns because it goes into the exception handler, so use this (zerofrog)
u32 VM_RETRANSLATE(u32 mem)
{
	u8* p, *pbase;
	if( (mem&0xffff0000) == 0x50000000 ) // reserved scratch pad mem
		return PS2MEM_BASE_+mem;

	p = (u8*)dmaGetAddrBase(mem);

#ifdef _WIN32	
	// do manual LUT since IPU/SPR seems to use addrs 0x3000xxxx quite often
	if( memLUT[ (p-PS2MEM_BASE)>>12 ].aPFNs == NULL ) {
		return PS2MEM_BASE_+mem;
	}

	pbase = (u8*)memLUT[ (p-PS2MEM_BASE)>>12 ].aVFNs[0];
	if( pbase != NULL )
		p = pbase + ((u32)p&0xfff);
#endif

	return (u32)p;
}

void memSetPageAddr(u32 vaddr, u32 paddr) {

	PSMEMORYMAP* pmap;

	if( vaddr == paddr )
		return;

	if( (vaddr>>28) != 1 && (vaddr>>28) != 9 && (vaddr>>28) != 11 ) {
#ifdef _WIN32
		pmap = &memLUT[vaddr >> 12];
		
		if( pmap->aPFNs != NULL && (pmap->aPFNs != memLUT[paddr>>12].aPFNs ||
			pmap->aVFNs[0] != TRANSFORM_ADDR(vaddr)+(u32)PS2MEM_BASE) ) {

			SysMapUserPhysicalPages((void*)pmap->aVFNs[0], 1, NULL, 0);
			pmap->aVFNs[0] = 0;
		}

		*pmap = memLUT[paddr >> 12];
#else
        memLUT[vaddr>>12] = memLUT[paddr>>12];
#endif
	}
}

void memClearPageAddr(u32 vaddr) {
//	SysPrintf("memClearPageAddr: %8.8x\n", vaddr);

	if ((vaddr & 0xffffc000) == 0x70000000) return;

#ifdef _WIN32
//	if( vaddr >= 0x20000000 && vaddr < 0x80000000 ) {
//		Cpu->Clear(vaddr&~0xfff, 0x1000/4);
//		if( memLUT[vaddr>>12].aVFNs != NULL ) {
//			SysMapUserPhysicalPages((void*)memLUT[vaddr>>12].aVFNs[0], 1, NULL, 0 );
//			memLUT[vaddr>>12].aVFNs = NULL;
//			memLUT[vaddr>>12].aPFNs = NULL;
//		}
//	}
#else
    if( memLUT[vaddr>>12] != NULL ) {
        SysVirtualFree(memLUT[vaddr>>12], 0x1000);
        memLUT[vaddr>>12] = 0;
    }
#endif
}

u8 recMemRead8()
{
    register u32 mem;
	__asm mov mem, ecx // already anded with ~0xa0000000

	switch( (mem&~0xffff) ) {
		case 0x1f400000: return psxHw4Read8(mem);
		case 0x10000000: return hwRead8(mem);
		case 0x1f800000: return psxHwRead8(mem);
		case 0x12000000: return *(PS2MEM_BASE+(mem&~0xc00));
		case 0x14000000:
		{
			u32 ret = DEV9read8(mem & ~0x04000000);
			SysPrintf("DEV9 read8 %8.8lx: %2.2lx\n", mem & ~0x04000000, ret);
			return ret;
		}
			
		default:
			return *(u8*)(PS2MEM_BASE+mem);
	}
	MEM_LOG("Unknown Memory read32  from address %8.8x\n", mem);
	
	cpuTlbMissR(mem, cpuRegs.branch);

	return 0;
}

void _eeReadConstMem8(int mmreg, u32 mem, int sign)
{
	assert( !IS_XMMREG(mmreg));

	if( IS_MMXREG(mmreg) ) {
		SetMMXstate();
		MOVDMtoMMX(mmreg&0xf, mem-3);
		assert(0);
	}
	else {
		if( sign ) MOVSX32M8toR(mmreg, mem);
		else MOVZX32M8toR(mmreg, mem);
	}
}

void _eeReadConstMem16(int mmreg, u32 mem, int sign)
{
	assert( !IS_XMMREG(mmreg));

	if( IS_MMXREG(mmreg) ) {
		SetMMXstate();
		MOVDMtoMMX(mmreg&0xf, mem-2);
		assert(0);
	}
	else {
		if( sign ) MOVSX32M16toR(mmreg, mem);
		else MOVZX32M16toR(mmreg, mem);
	}
}

void _eeReadConstMem32(int mmreg, u32 mem)
{
	if( IS_XMMREG(mmreg) ) SSEX_MOVD_M32_to_XMM(mmreg&0xf, mem);
	else if( IS_MMXREG(mmreg) ) {
		SetMMXstate();
		MOVDMtoMMX(mmreg&0xf, mem);
	}
	else MOV32MtoR(mmreg, mem);
}

void _eeReadConstMem128(int mmreg, u32 mem)
{
	if( IS_MMXREG(mmreg) ) {
		SetMMXstate();
		MOVQMtoR((mmreg>>4)&0xf, mem+8);
		MOVQMtoR(mmreg&0xf, mem);
	}
	else SSEX_MOVDQA_M128_to_XMM( mmreg&0xf, mem);
}

void _eeWriteConstMem8(u32 mem, int mmreg)
{
	assert( !IS_XMMREG(mmreg) && !IS_MMXREG(mmreg) );
	if( IS_EECONSTREG(mmreg) ) MOV8ItoM(mem, g_cpuConstRegs[((mmreg>>16)&0x1f)].UL[0]);
	else if( IS_PSXCONSTREG(mmreg) ) MOV8ItoM(mem, g_psxConstRegs[((mmreg>>16)&0x1f)]);
	else MOV8RtoM(mem, mmreg);
}

void _eeWriteConstMem16(u32 mem, int mmreg)
{
	assert( !IS_XMMREG(mmreg) && !IS_MMXREG(mmreg) );
	if( IS_EECONSTREG(mmreg) ) MOV16ItoM(mem, g_cpuConstRegs[((mmreg>>16)&0x1f)].UL[0]);
	else if( IS_PSXCONSTREG(mmreg) ) MOV16ItoM(mem, g_psxConstRegs[((mmreg>>16)&0x1f)]);
	else MOV16RtoM(mem, mmreg);
}

void _eeWriteConstMem16OP(u32 mem, int mmreg, int op)
{
	assert( !IS_XMMREG(mmreg) && !IS_MMXREG(mmreg) );
	switch(op) {
		case 0: // and
			if( IS_EECONSTREG(mmreg) ) AND16ItoM(mem, g_cpuConstRegs[((mmreg>>16)&0x1f)].UL[0]);
			else if( IS_PSXCONSTREG(mmreg) ) AND16ItoM(mem, g_psxConstRegs[((mmreg>>16)&0x1f)]);
			else AND16RtoM(mem, mmreg);
			break;
		case 1: // and
			if( IS_EECONSTREG(mmreg) ) OR16ItoM(mem, g_cpuConstRegs[((mmreg>>16)&0x1f)].UL[0]);
			else if( IS_PSXCONSTREG(mmreg) ) OR16ItoM(mem, g_psxConstRegs[((mmreg>>16)&0x1f)]);
			else OR16RtoM(mem, mmreg);
			break;
		default: assert(0);
	}
}

void _eeWriteConstMem32(u32 mem, int mmreg)
{
	if( IS_XMMREG(mmreg) ) SSE2_MOVD_XMM_to_M32(mem, mmreg&0xf);
	else if( IS_MMXREG(mmreg) ) {
		SetMMXstate();
		MOVDMMXtoM(mem, mmreg&0xf);
	}
	else if( IS_EECONSTREG(mmreg) ) MOV32ItoM(mem, g_cpuConstRegs[((mmreg>>16)&0x1f)].UL[0]);
	else if( IS_PSXCONSTREG(mmreg) ) MOV32ItoM(mem, g_psxConstRegs[((mmreg>>16)&0x1f)]);
	else MOV32RtoM(mem, mmreg);
}

void _eeWriteConstMem32OP(u32 mem, int mmreg, int op)
{
	switch(op) {
		case 0: // and
			if( IS_XMMREG(mmreg) ) {
				_deleteEEreg((mmreg>>16)&0x1f, 1);
				SSE2_PAND_M128_to_XMM(mmreg&0xf, mem);
				SSE2_MOVD_XMM_to_M32(mem, mmreg&0xf);
			}
			else if( IS_MMXREG(mmreg) ) {
				_deleteEEreg((mmreg>>16)&0x1f, 1);
				SetMMXstate();
				PANDMtoR(mmreg&0xf, mem);
				MOVDMMXtoM(mem, mmreg&0xf);
			}
			else if( IS_EECONSTREG(mmreg) ) {
				AND32ItoM(mem, g_cpuConstRegs[((mmreg>>16)&0x1f)].UL[0]);
			}
			else if( IS_PSXCONSTREG(mmreg) ) {
				AND32ItoM(mem, g_psxConstRegs[((mmreg>>16)&0x1f)]);
			}
			else {
				AND32RtoM(mem, mmreg&0xf);
			}
			break;

		case 1: // or
			if( IS_XMMREG(mmreg) ) {
				_deleteEEreg((mmreg>>16)&0x1f, 1);
				SSE2_POR_M128_to_XMM(mmreg&0xf, mem);
				SSE2_MOVD_XMM_to_M32(mem, mmreg&0xf);
			}
			else if( IS_MMXREG(mmreg) ) {
				_deleteEEreg((mmreg>>16)&0x1f, 1);
				SetMMXstate();
				PORMtoR(mmreg&0xf, mem);
				MOVDMMXtoM(mem, mmreg&0xf);
			}
			else if( IS_EECONSTREG(mmreg) ) {
				OR32ItoM(mem, g_cpuConstRegs[((mmreg>>16)&0x1f)].UL[0]);
			}
			else if( IS_PSXCONSTREG(mmreg) ) {
				OR32ItoM(mem, g_psxConstRegs[((mmreg>>16)&0x1f)]);
			}
			else {
				OR32RtoM(mem, mmreg&0xf);
			}
			break;

		case 2: // not and
			if( mmreg & MEM_XMMTAG ) {
				_deleteEEreg(mmreg>>16, 1);
				SSEX_PANDN_M128_to_XMM(mmreg&0xf, mem);
				SSEX_MOVD_XMM_to_M32(mem, mmreg&0xf);
			}
			else if( mmreg & MEM_MMXTAG ) {
				_deleteEEreg(mmreg>>16, 1);
				PANDNMtoR(mmreg&0xf, mem);
				MOVDMMXtoM(mem, mmreg&0xf);
			}
			else if( IS_EECONSTREG(mmreg) ) {
				AND32ItoM(mem, ~g_cpuConstRegs[((mmreg>>16)&0x1f)].UL[0]);
			}
			else if( IS_PSXCONSTREG(mmreg) ) {
				AND32ItoM(mem, ~g_psxConstRegs[((mmreg>>16)&0x1f)]);
			}
			else {
				NOT32R(mmreg&0xf);
				AND32RtoM(mem, mmreg&0xf);
			}
			break;

		default: assert(0);
	}
}

void _eeWriteConstMem64(u32 mem, int mmreg)
{
	if( IS_XMMREG(mmreg) ) SSE_MOVLPS_XMM_to_M64(mem, mmreg&0xf);
	else if( IS_MMXREG(mmreg) ) {
		SetMMXstate();
		MOVQRtoM(mem, mmreg&0xf);
	}
	else if( IS_EECONSTREG(mmreg) ) {
		MOV32ItoM(mem, g_cpuConstRegs[((mmreg>>16)&0x1f)].UL[0]);
		MOV32ItoM(mem+4, g_cpuConstRegs[((mmreg>>16)&0x1f)].UL[1]);
	}
	else assert(0);
}

void _eeWriteConstMem128(u32 mem, int mmreg)
{
	if( IS_MMXREG(mmreg) ) {
		SetMMXstate();
		MOVQRtoM(mem, mmreg&0xf);
		MOVQRtoM(mem+8, (mmreg>>4)&0xf);
	}
	else if( IS_EECONSTREG(mmreg) ) {
		SetMMXstate();
		MOV32ItoM(mem, g_cpuConstRegs[((mmreg>>16)&0x1f)].UL[0]);
		MOV32ItoM(mem+4, g_cpuConstRegs[((mmreg>>16)&0x1f)].UL[1]);
		MOVQRtoM(mem+8, mmreg&0xf);
	}
	else SSEX_MOVDQA_XMM_to_M128(mem, mmreg&0xf);
}

void _eeMoveMMREGtoR(x86IntRegType to, int mmreg)
{
	if( IS_XMMREG(mmreg) ) SSE2_MOVD_XMM_to_R(to, mmreg&0xf);
	else if( IS_MMXREG(mmreg) ) {
		SetMMXstate();
		MOVD32MMXtoR(to, mmreg&0xf);
	}
	else if( IS_EECONSTREG(mmreg) ) MOV32ItoR(to, g_cpuConstRegs[((mmreg>>16)&0x1f)].UL[0]);
	else if( IS_PSXCONSTREG(mmreg) ) MOV32ItoR(to, g_psxConstRegs[((mmreg>>16)&0x1f)]);
	else if( mmreg != to ) MOV32RtoR(to, mmreg);
}

int recMemConstRead8(u32 x86reg, u32 mem, u32 sign)
{
	mem = TRANSFORM_ADDR(mem);

	switch( mem>>16 ) {
		case 0x1f40: return psxHw4ConstRead8(x86reg, mem, sign);
		case 0x1000: return hwConstRead8(x86reg, mem, sign);
		case 0x1f80: return psxHwConstRead8(x86reg, mem, sign);
		case 0x1200: return gsConstRead8(x86reg, mem, sign);
		case 0x1400:
		{
			iFlushCall(0);
			PUSH32I(mem & ~0x04000000);
			CALLFunc((u32)DEV9read8);
			if( sign ) MOVSX32R8toR(EAX, EAX);
			else MOVZX32R8toR(EAX, EAX);
			return 1;
		}
			
		default:
			_eeReadConstMem8(x86reg, VM_RETRANSLATE(mem), sign);
			return 0;
	}
}

u16 recMemRead16()  {

	register u32 mem;
	__asm mov mem, ecx // already anded with ~0xa0000000

	switch( mem>>16 ) {
		case 0x1000: return hwRead16(mem);
		case 0x1f80: return psxHwRead16(mem);
		case 0x1200: return *(u16*)(PS2MEM_BASE+(mem&~0xc00));
		case 0x1800: return 0;
		case 0x1a00: return ba0R16(mem);
		case 0x1f90:
		case 0x1f00:
			return SPU2read(mem);
		case 0x1400:
		{
			u32 ret = DEV9read16(mem & ~0x04000000);
			SysPrintf("DEV9 read8 %8.8lx: %2.2lx\n", mem & ~0x04000000, ret);
			return ret;
		}

		default:
			return *(u16*)(PS2MEM_BASE+mem);
	}
	MEM_LOG("Unknown Memory read16  from address %8.8x\n", mem);
	cpuTlbMissR(mem, cpuRegs.branch);
	return 0;
}

int recMemConstRead16(u32 x86reg, u32 mem, u32 sign)
{
	mem = TRANSFORM_ADDR(mem);

	switch( mem>>16 ) {
		case 0x1000: return hwConstRead16(x86reg, mem, sign);
		case 0x1f80: return psxHwConstRead16(x86reg, mem, sign);
		case 0x1200: return gsConstRead16(x86reg, mem, sign);
		case 0x1800: 
			if( IS_MMXREG(x86reg) ) PXORRtoR(x86reg&0xf, x86reg&0xf);
			else XOR32RtoR(x86reg, x86reg);
			return 0;
		case 0x1a00:
			iFlushCall(0);
			PUSH32I(mem);
			CALLFunc((u32)ba0R16);
			ADD32ItoR(ESP, 4);
			if( sign ) MOVSX32R16toR(EAX, EAX);
			else MOVZX32R16toR(EAX, EAX);
			return 1;

		case 0x1f90:
		case 0x1f00:
			iFlushCall(0);
			PUSH32I(mem);
			CALLFunc((u32)SPU2read);
			if( sign ) MOVSX32R16toR(EAX, EAX);
			else MOVZX32R16toR(EAX, EAX);
			return 1;

		case 0x1400:
			iFlushCall(0);
			PUSH32I(mem & ~0x04000000);
			CALLFunc((u32)DEV9read16);
			if( sign ) MOVSX32R16toR(EAX, EAX);
			else MOVZX32R16toR(EAX, EAX);
			return 1;

		default:
			_eeReadConstMem16(x86reg, VM_RETRANSLATE(mem), sign);
			return 0;
	}
}

__declspec(naked)
u32 recMemRead32()  {
	// ecx is address - already anded with ~0xa0000000
	__asm {

		mov edx, ecx
		shr edx, 16
		cmp dx, 0x1000
		je hwread
		cmp dx, 0x1f80
		je psxhwread
		cmp dx, 0x1200
		je gsread
		cmp dx, 0x1400
		je devread

		// default read
		mov eax, dword ptr [ecx+PS2MEM_BASE_]
		ret
	}

hwread:
	{
		__asm {
			cmp ecx, 0x10002000
			jb counterread

			cmp ecx, 0x1000f260
			je hwsifpresetread
			cmp ecx, 0x1000f240
			je hwsifsyncread
            cmp ecx, 0x1000f440
			je hwmch_drd
            cmp ecx, 0x1000f430
			je hwmch_ricm
			
			cmp ecx, 0x10003000
			jb hwdefread2
			mov eax, dword ptr [ecx+PS2MEM_BASE_]
			ret

			// ipu
hwdefread2:
			push ecx
			call ipuRead32
			add esp, 4	
			ret
			
			// sif
hwsifpresetread:
			xor eax, eax
			ret
hwsifsyncread:
            mov eax, 0x1000F240
			mov eax, dword ptr [eax+PS2MEM_BASE_]
			or eax, 0xF0000102
			ret
		}
		
counterread:
        {
			static u32 mem, index;
		
			// counters
			__asm mov mem, ecx
			index = (mem>>11)&3;

			if( (mem&0x7ff) == 0 ) {
				__asm {
					push index
					call rcntRcount
					add esp, 4
					and eax, 0xffff
					ret
				}
			}

			index = (u32)&counters[index] + ((mem>>2)&0xc);

			__asm {
				mov eax, index
				mov eax, dword ptr [eax]
				movzx eax, ax
				ret
			}
		}

hwmch_drd: // MCH_DRD
        __asm {
            mov eax, dword ptr [ecx+PS2MEM_BASE_-0x10]
            shr eax, 6
            test eax, 0xf
            jz mch_drd_2
hwmch_ricm:
            xor eax, eax
            ret

mch_drd_2:
            shr eax, 10
            and eax, 0xfff
            cmp eax, 0x21 // INIT
            je mch_drd_init
            cmp eax, 0x23 // CNFGA
            je mch_drd_cnfga
            cmp eax, 0x24 // CNFGB
            je mch_drd_cnfgb
            cmp eax, 0x40 // DEVID
            je mch_drd_devid
            xor eax, eax 
            ret

mch_drd_init:
            mov edx, rdram_devices
            xor eax, eax
            cmp edx, rdram_sdevid
            setg al
            add rdram_sdevid, eax
            imul eax, 0x1f
            ret

mch_drd_cnfga:
            mov eax, 0x0D0D
            ret

mch_drd_cnfgb:
            mov eax, 0x0090
            ret

mch_drd_devid:
            mov eax, dword ptr [ecx+PS2MEM_BASE_-0x10]
            and eax, 0x1f
            ret
        }
	}

psxhwread:
	__asm {
		push ecx
		call psxHwRead32		
		add esp, 4
		ret
	}

gsread:
	__asm {
		and ecx, 0xfffff3ff
		mov eax, dword ptr [ecx+PS2MEM_BASE_]
		ret
	}
	
devread:
	__asm {
		and ecx, 0xfbffffff
		push ecx
		call DEV9read32
		add esp, 4
		ret
	}
}

int recMemConstRead32(u32 x86reg, u32 mem)
{
	mem = TRANSFORM_ADDR(mem);

	switch( (mem&~0xffff) ) {
		case 0x10000000: return hwConstRead32(x86reg, mem);
		case 0x1f800000: return psxHwConstRead32(x86reg, mem);
		case 0x12000000: return gsConstRead32(x86reg, mem);
		case 0x14000000:
			iFlushCall(0);
			PUSH32I(mem & ~0x04000000);
			CALLFunc((u32)DEV9read32);
			return 1;

		default:
			_eeReadConstMem32(x86reg, VM_RETRANSLATE(mem));
			return 0;
	}
}

void recMemRead64(u64 *out)
{	
	register u32 mem;
	__asm mov mem, ecx // already anded with ~0xa0000000

	switch( (mem&0xffff0000) ) {
		case 0x10000000: *out = hwRead64(mem); return;
		case 0x11000000: *out = *(u64*)(PS2MEM_BASE+mem); return;
		case 0x12000000: *out = *(u64*)(PS2MEM_BASE+(mem&~0xc00)); return;
		
		default:
			//assert(0);
			*out = *(u64*)(PS2MEM_BASE+mem);
			return;
	}

#ifdef PCSX2_DEVBUILD
	MEM_LOG("Unknown Memory read32  from address %8.8x (Status=%8.8x)\n", mem, cpuRegs.CP0.n.Status);
#endif
	cpuTlbMissR(mem, cpuRegs.branch);
}

void recMemConstRead64(u32 mem, int mmreg)
{
	mem = TRANSFORM_ADDR(mem);

	switch( (mem&0xffff0000) ) {
		case 0x10000000: hwConstRead64(mem, mmreg); return;
		case 0x12000000: gsConstRead64(mem, mmreg); return;
		default:
			if( IS_XMMREG(mmreg) ) SSE_MOVLPS_M64_to_XMM(mmreg&0xff, VM_RETRANSLATE(mem));
			else {
				MOVQMtoR(mmreg, VM_RETRANSLATE(mem));
				SetMMXstate();
			}
			return;
	}
}

void recMemRead128(u64 *out)  {

	register u32 mem;
	__asm mov mem, ecx // already anded with ~0xa0000000

	switch( (mem&0xffff0000) ) {
		case 0x10000000: 
			hwRead128(mem, out);
			return;
		case 0x12000000:
			out[0] = *(u64*)(PS2MEM_BASE+(mem&~0xc00));
			out[1] = *(u64*)(PS2MEM_BASE+(mem&~0xc00)+8);
			return;
		case 0x11000000:
			out[0] = *(u64*)(PS2MEM_BASE+mem);
			out[1] = *(u64*)(PS2MEM_BASE+mem+8);
			return;
		default:
			//assert(0);
			out[0] = *(u64*)(PS2MEM_BASE+mem);
			out[1] = *(u64*)(PS2MEM_BASE+mem+8);
			return;
	}

#ifdef PCSX2_DEVBUILD
	MEM_LOG("Unknown Memory read32  from address %8.8x (Status=%8.8x)\n", mem, cpuRegs.CP0.n.Status);
#endif
	cpuTlbMissR(mem, cpuRegs.branch);
}

void recMemConstRead128(u32 mem, int xmmreg)
{
	mem = TRANSFORM_ADDR(mem);

	switch( (mem&0xffff0000) ) {
		case 0x10000000: hwConstRead128(mem, xmmreg); return;
		case 0x12000000: gsConstRead128(mem, xmmreg); return;
		default:
			_eeReadConstMem128(xmmreg, VM_RETRANSLATE(mem));
			return;
	}
}

void errwrite()
{
	int i, bit, tempeax;
	__asm mov i, ecx
	__asm mov tempeax, eax
	__asm mov bit, edx
	SysPrintf("Error write%d at %x\n", bit, i);
	assert(0);
	__asm mov eax, tempeax
	__asm mov ecx, i
}

void recMemWrite8()
{
	register u32 mem;
	register u8 value;
	__asm mov mem, ecx // already anded with ~0xa0000000
	__asm mov value, al

	switch( mem>>16 ) {
		case 0x1f40: psxHw4Write8(mem, value); return;
		case 0x1000: hwWrite8(mem, value); return;
		case 0x1f80: psxHwWrite8(mem, value); return;
		case 0x1200: gsWrite8(mem, value); return;
		case 0x1400:
			DEV9write8(mem & ~0x04000000, value);
			SysPrintf("DEV9 write8 %8.8lx: %2.2lx\n", mem & ~0x04000000, value);
			return;

#ifdef _DEBUG
		case 0x1100: assert(0);
#endif
		default:
			// vus, bad addrs, etc
			*(u8*)(PS2MEM_BASE+mem) = value;
			return;
	}
	MEM_LOG("Unknown Memory write8   to  address %x with data %2.2x\n", mem, value);
	cpuTlbMissW(mem, cpuRegs.branch);
}

int recMemConstWrite8(u32 mem, int mmreg)
{
	mem = TRANSFORM_ADDR(mem);

	switch( mem>>16 ) {
		case 0x1f40: psxHw4ConstWrite8(mem, mmreg); return 0;
		case 0x1000: hwConstWrite8(mem, mmreg); return 0;
		case 0x1f80: psxHwConstWrite8(mem, mmreg); return 0;
		case 0x1200: gsConstWrite8(mem, mmreg); return 0;
		case 0x1400:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(mem & ~0x04000000);
			CALLFunc((u32)DEV9write8);
			return 0;

		case 0x1100:
			_eeWriteConstMem8(PS2MEM_BASE_+mem, mmreg);

			if( mem < 0x11004000 ) {
				PUSH32I(1);
				PUSH32I(mem&0x3ff8);
				CALLFunc((u32)Cpu->ClearVU0);
				ADD32ItoR(ESP, 8);
			}
			else if( mem >= 0x11008000 && mem < 0x1100c000 ) {
				PUSH32I(1);
				PUSH32I(mem&0x3ff8);
				CALLFunc((u32)Cpu->ClearVU1);
				ADD32ItoR(ESP, 8);
			}
			return 0;

		default:
			_eeWriteConstMem8(PS2MEM_BASE_+mem, mmreg);
			return 1;
	}
}

void recMemWrite16()   {

	register u32 mem;
	register u16 value;
	__asm mov mem, ecx // already anded with ~0xa0000000
	__asm mov value, ax

	switch( mem>>16 ) {
		case 0x1000: hwWrite16(mem, value); return;
		case 0x1600:
			//HACK: DEV9 VM crash fix
			return;
		case 0x1f80: psxHwWrite16(mem, value); return;
		case 0x1200: gsWrite16(mem, value); return;
		case 0x1f90:
		case 0x1f00: SPU2write(mem, value); return;
		case 0x1400:
			DEV9write16(mem & ~0x04000000, value);
			SysPrintf("DEV9 write16 %8.8lx: %4.4lx\n", mem & ~0x04000000, value);
			return;

#ifdef _DEBUG
		case 0x1100: assert(0);
#endif
		default:
			// vus, bad addrs, etc
			*(u16*)(PS2MEM_BASE+mem) = value;
			return;
	}
	MEM_LOG("Unknown Memory write16  to  address %x with data %4.4x\n", mem, value);
	cpuTlbMissW(mem, cpuRegs.branch);
}

int recMemConstWrite16(u32 mem, int mmreg)
{
	mem = TRANSFORM_ADDR(mem);

	switch( mem>>16 ) {
		case 0x1000: hwConstWrite16(mem, mmreg); return 0;
        case 0x1600:
			//HACK: DEV9 VM crash fix
			return 0;
		case 0x1f80: psxHwConstWrite16(mem, mmreg); return 0;
		case 0x1200: gsConstWrite16(mem, mmreg); return 0;
		case 0x1f90:
		case 0x1f00:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(mem);
			CALLFunc((u32)SPU2write);
			return 0;
		case 0x1400:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(mem & ~0x04000000);
			CALLFunc((u32)DEV9write16);
			return 0;

		case 0x1100:
			_eeWriteConstMem16(PS2MEM_BASE_+mem, mmreg);

			if( mem < 0x11004000 ) {
				PUSH32I(1);
				PUSH32I(mem&0x3ff8);
				CALLFunc((u32)Cpu->ClearVU0);
				ADD32ItoR(ESP, 8);
			}
			else if( mem >= 0x11008000 && mem < 0x1100c000 ) {
				PUSH32I(1);
				PUSH32I(mem&0x3ff8);
				CALLFunc((u32)Cpu->ClearVU1);
				ADD32ItoR(ESP, 8);
			}
			return 0;

		default:
			_eeWriteConstMem16(PS2MEM_BASE_+mem, mmreg);
			return 1;
	}
}

C_ASSERT( sizeof(BASEBLOCK) == 8 );

__declspec(naked)
void recMemWrite32()
{
	// ecx is address - already anded with ~0xa0000000
	__asm {

		mov edx, ecx
		shr edx, 16
		cmp dx, 0x1000
		je hwwrite
		cmp dx, 0x1f80
		je psxwrite
		cmp dx, 0x1200
		je gswrite
		cmp dx, 0x1400
		je devwrite
		cmp dx, 0x1100
		je vuwrite
	}

	__asm {
		// default write
		mov dword ptr [ecx+PS2MEM_BASE_], eax
		ret

hwwrite:
		push eax
		push ecx
		call hwWrite32
		add esp, 8
		ret
psxwrite:
		push eax
		push ecx
		call psxHwWrite32
		add esp, 8
		ret
gswrite:
		push eax
		push ecx
		call gsWrite32
		add esp, 8
		ret
devwrite:
		and ecx, 0xfbffffff
		push eax
		push ecx
		call DEV9write32
		add esp, 8
		ret
vuwrite:
		// default write
		mov dword ptr [ecx+PS2MEM_BASE_], eax
		
		cmp ecx, 0x11004000
		jge vu1write
		and ecx, 0x3ff8
		// clear vu0mem
		mov eax, Cpu
		push 1
		push ecx
		call [eax]Cpu.ClearVU0
		add esp, 8
		ret

vu1write:
		cmp ecx, 0x11008000
		jl vuend
		cmp ecx, 0x1100c000
		jge vuend
		// clear vu1mem
		and ecx, 0x3ff8
		mov eax, Cpu
		push 1
		push ecx
		call [eax]Cpu.ClearVU1
		add esp, 8
vuend:
		ret
	}
}

int recMemConstWrite32(u32 mem, int mmreg)
{
	mem = TRANSFORM_ADDR(mem);

	switch( mem&0xffff0000 ) {
		case 0x10000000: hwConstWrite32(mem, mmreg); return 0;
		case 0x1f800000: psxHwConstWrite32(mem, mmreg); return 0;
		case 0x12000000: gsConstWrite32(mem, mmreg); return 0;
		case 0x1f900000:
		case 0x1f000000:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(mem);
			CALLFunc((u32)SPU2write);
			return 0;
		case 0x14000000:
			_recPushReg(mmreg);
			iFlushCall(0);
			PUSH32I(mem & ~0x04000000);
			CALLFunc((u32)DEV9write32);
			return 0;

		case 0x11000000:
			_eeWriteConstMem32(PS2MEM_BASE_+mem, mmreg);

			if( mem < 0x11004000 ) {
				PUSH32I(1);
				PUSH32I(mem&0x3ff8);
				CALLFunc((u32)Cpu->ClearVU0);
				ADD32ItoR(ESP, 8);
			}
			else if( mem >= 0x11008000 && mem < 0x1100c000 ) {
				PUSH32I(1);
				PUSH32I(mem&0x3ff8);
				CALLFunc((u32)Cpu->ClearVU1);
				ADD32ItoR(ESP, 8);
			}
			return 0;

		default:
			_eeWriteConstMem32(PS2MEM_BASE_+mem, mmreg);
			return 1;
	}
}

__declspec(naked) void recMemWrite64()
{
	__asm {
		mov edx, ecx
		shr edx, 16
		cmp dx, 0x1000
		je hwwrite
		cmp dx, 0x1200
		je gswrite
		cmp dx, 0x1100
		je vuwrite
	}

	__asm {
		// default write
		mov edx, 64
		call errwrite

hwwrite:
		push dword ptr [eax+4]
		push dword ptr [eax]
		push ecx
		call hwWrite64
		add esp, 12
		ret

gswrite:
		push dword ptr [eax+4]
		push dword ptr [eax]
		push ecx
		call gsWrite64
		add esp, 12
		ret

vuwrite:
		mov ebx, dword ptr [eax]
		mov edx, dword ptr [eax+4]
		mov dword ptr [ecx+PS2MEM_BASE_], ebx
		mov dword ptr [ecx+PS2MEM_BASE_+4], edx
		
		cmp ecx, 0x11004000
		jge vu1write
		and ecx, 0x3ff8
		// clear vu0mem
		mov eax, Cpu
		push 2
		push ecx
		call [eax]Cpu.ClearVU0
		add esp, 8
		ret

vu1write:
		cmp ecx, 0x11008000
		jl vuend
		cmp ecx, 0x1100c000
		jge vuend
		// clear vu1mem
		and ecx, 0x3ff8
		mov eax, Cpu
		push 2
		push ecx
		call [eax]Cpu.ClearVU1
		add esp, 8
vuend:
		ret
	}
}

int recMemConstWrite64(u32 mem, int mmreg)
{
	mem = TRANSFORM_ADDR(mem);

	switch( (mem>>16) ) {
		case 0x1000: hwConstWrite64(mem, mmreg); return 0;
		case 0x1200: gsConstWrite64(mem, mmreg); return 0;

		case 0x1100:
			_eeWriteConstMem64(PS2MEM_BASE_+mem, mmreg);

			if( mem < 0x11004000 ) {
				PUSH32I(2);
				PUSH32I(mem&0x3ff8);
				CALLFunc((u32)Cpu->ClearVU0);
				ADD32ItoR(ESP, 8);
			}
			else if( mem >= 0x11008000 && mem < 0x1100c000 ) {
				PUSH32I(2);
				PUSH32I(mem&0x3ff8);
				CALLFunc((u32)Cpu->ClearVU1);
				ADD32ItoR(ESP, 8);
			}
			return 0;

		default:
			_eeWriteConstMem64(PS2MEM_BASE_+mem, mmreg);
			return 1;
	}
}

__declspec(naked)
void recMemWrite128()
{
	__asm {

		mov edx, ecx
		shr edx, 16
		cmp dx, 0x1000
		je hwwrite
		cmp dx, 0x1200
		je gswrite
		cmp dx, 0x1100
		je vuwrite
	}

	__asm {
		mov edx, 128
		call errwrite

hwwrite:

		push eax
		push ecx
		call hwWrite128
		add esp, 8
		ret

vuwrite:
		mov ebx, dword ptr [eax]
		mov edx, dword ptr [eax+4]
		mov edi, dword ptr [eax+8]
		mov eax, dword ptr [eax+12]
		mov dword ptr [ecx+PS2MEM_BASE_], ebx
		mov dword ptr [ecx+PS2MEM_BASE_+4], edx
		mov dword ptr [ecx+PS2MEM_BASE_+8], edi
		mov dword ptr [ecx+PS2MEM_BASE_+12], eax

		cmp ecx, 0x11004000
		jge vu1write
		and ecx, 0x3ff8
		// clear vu0mem
		mov eax, Cpu
		push 4
		push ecx
		call [eax]Cpu.ClearVU0
		add esp, 8
		ret

vu1write:
		cmp ecx, 0x11008000
		jl vuend
		cmp ecx, 0x1100c000
		jge vuend
		// clear vu1mem
		and ecx, 0x3ff8
		mov eax, Cpu
		push 4
		push ecx
		call [eax]Cpu.ClearVU1
		add esp, 8
vuend:

		// default write
		//movaps xmm7, qword ptr [eax]

		// removes possible exceptions and saves on remapping memory
		// *might* be faster for certain games, no way to tell
//		cmp ecx, 0x20000000
//		jb Write128
//
//		// look for better mapping
//		mov edx, ecx
//		shr edx, 12
//		shl edx, 3
//		add edx, memLUT
//		mov edx, dword ptr [edx + 4]
//		cmp edx, 0
//		je Write128
//		mov edx, dword ptr [edx]
//		cmp edx, 0
//		je Write128
//		and ecx, 0xfff
//		movaps qword ptr [ecx+edx], xmm7
//		jmp CheckOverwrite
//Write128:
		//movaps qword ptr [ecx+PS2MEM_BASE_], xmm7
		ret

gswrite:
		sub esp, 8
		movlps xmm7, qword ptr [eax]
		movlps qword ptr [esp], xmm7
		push ecx
		call gsWrite64

		// call again for upper 8 bytes
		movlps xmm7, qword ptr [eax+8]
		movlps qword ptr [esp+4], xmm7
		add [esp], 8
		call gsWrite64
		add esp, 12
		ret
	}
}

int recMemConstWrite128(u32 mem, int mmreg)
{
	mem = TRANSFORM_ADDR(mem);

	switch( (mem&0xffff0000) ) {
		case 0x10000000: hwConstWrite128(mem, mmreg); return 0;
		case 0x12000000: gsConstWrite128(mem, mmreg); return 0;

		case 0x11000000:
			_eeWriteConstMem128(PS2MEM_BASE_+mem, mmreg);

			if( mem < 0x11004000 ) {
				PUSH32I(4);
				PUSH32I(mem&0x3ff8);
				CALLFunc((u32)Cpu->ClearVU0);
				ADD32ItoR(ESP, 8);
			}
			else if( mem >= 0x11008000 && mem < 0x1100c000 ) {
				PUSH32I(4);
				PUSH32I(mem&0x3ff8);
				CALLFunc((u32)Cpu->ClearVU1);
				ADD32ItoR(ESP, 8);
			}
			return 0;

		default:
			_eeWriteConstMem128(PS2MEM_BASE_+mem, mmreg);
			return 1;
	}
}

int  __fastcall memRead8 (u32 mem, u8  *out)  {

	mem = TRANSFORM_ADDR(mem);
	switch( (mem&~0xffff) ) {
		case 0x1f400000: *out = psxHw4Read8(mem); return 0;
		case 0x10000000: *out = hwRead8(mem); return 0;
		case 0x1f800000: *out = psxHwRead8(mem); return 0;
		case 0x12000000: *out = gsRead8(mem); return 0;
		case 0x14000000:
			*out = DEV9read8(mem & ~0x04000000);
			SysPrintf("DEV9 read8 %8.8lx: %2.2lx\n", mem & ~0x04000000, *out);
			return 0;

		default:
			*out = *(u8*)(PS2MEM_BASE+mem);
			return 0;
	}

#ifdef MEM_LOG
	MEM_LOG("Unknown Memory read32  from address %8.8x\n", mem);
#endif
	cpuTlbMissR(mem, cpuRegs.branch);

	return -1;
}

int  __fastcall memRead8RS (u32 mem, u64 *out)
{	
	mem = TRANSFORM_ADDR(mem);
	switch( (mem&~0xffff) ) {
		case 0x1f400000: *out = (s8)psxHw4Read8(mem); return 0;
		case 0x10000000: *out = (s8)hwRead8(mem); return 0;
		case 0x1f800000: *out = (s8)psxHwRead8(mem); return 0;
		case 0x12000000: *out = (s8)gsRead8(mem); return 0;
		case 0x14000000:
			*out = (s8)DEV9read8(mem & ~0x04000000);
			SysPrintf("DEV9 read8 %8.8lx: %2.2lx\n", mem & ~0x04000000, *out);
			return 0;

		default:
			*out = *(s8*)(PS2MEM_BASE+mem);
			return 0;
	}
	MEM_LOG("Unknown Memory read32  from address %8.8x\n", mem);
	cpuTlbMissR(mem, cpuRegs.branch);

	return -1;
}

int __fastcall memRead8RU (u32 mem, u64 *out)
{
	mem = TRANSFORM_ADDR(mem);
	switch( (mem&~0xffff) ) {
		case 0x1f400000: *out = (u8)psxHw4Read8(mem); return 0;
		case 0x10000000: *out = (u8)hwRead8(mem); return 0;
		case 0x1f800000: *out = (u8)psxHwRead8(mem); return 0;
		case 0x12000000: *out = (u8)gsRead8(mem); return 0;
		case 0x14000000:
			*out = (u8)DEV9read8(mem & ~0x04000000);
			SysPrintf("DEV9 read8 %8.8lx: %2.2lx\n", mem & ~0x04000000, *out);
			return 0;

		default:
			*out = *(u8*)(PS2MEM_BASE+mem);
			return 0;
	}
	MEM_LOG("Unknown Memory read32  from address %8.8x\n", mem);
	cpuTlbMissR(mem, cpuRegs.branch);

	return -1;
}

int __fastcall memRead16(u32 mem, u16 *out)  {
	
	mem = TRANSFORM_ADDR(mem);
	switch( (mem&~0xffff) ) {
		case 0x10000000: *out = hwRead16(mem); return 0;
		case 0x1f800000: *out = psxHwRead16(mem); return 0;
		case 0x12000000: *out = gsRead16(mem); return 0;
		case 0x18000000: *out = 0; return 0;
		case 0x1a000000: *out = ba0R16(mem); return 0;
		case 0x1f900000:
		case 0x1f000000:
			*out = SPU2read(mem); return 0;
			break;
		case 0x14000000:
			*out = DEV9read16(mem & ~0x04000000);
			SysPrintf("DEV9 read8 %8.8lx: %2.2lx\n", mem & ~0x04000000, *out);
			return 0;

		default:
			*out = *(u16*)(PS2MEM_BASE+mem);
			return 0;
	}
	MEM_LOG("Unknown Memory read16  from address %8.8x\n", mem);
	cpuTlbMissR(mem, cpuRegs.branch);
	return -1;
}

int __fastcall memRead16RS(u32 mem, u64 *out)  {

	mem = TRANSFORM_ADDR(mem);
	switch( (mem&~0xffff) ) {
		case 0x10000000: *out = (s16)hwRead16(mem); return 0;
		case 0x1f800000: *out = (s16)psxHwRead16(mem); return 0;
		case 0x12000000: *out = (s16)gsRead16(mem); return 0;
		case 0x18000000: *out = 0; return 0;
		case 0x1a000000: *out = (s16)ba0R16(mem); return 0;
		case 0x1f900000:
		case 0x1f000000:
			*out = (s16)SPU2read(mem); return 0;
			break;
		case 0x14000000:
			*out = (s16)DEV9read16(mem & ~0x04000000);
			SysPrintf("DEV9 read8 %8.8lx: %2.2lx\n", mem & ~0x04000000, *out);
			return 0;

		default:
			*out = *(s16*)(PS2MEM_BASE+mem);
			return 0;
	}
	MEM_LOG("Unknown Memory read16  from address %8.8x\n", mem);
	cpuTlbMissR(mem, cpuRegs.branch);
	return -1;
}

int __fastcall memRead16RU(u32 mem, u64 *out)  {
	
	mem = TRANSFORM_ADDR(mem);
	switch( (mem&~0xffff) ) {
		case 0x10000000: *out = (u16)hwRead16(mem ); return 0;
		case 0x1f800000: *out = (u16)psxHwRead16(mem ); return 0;
		case 0x12000000: *out = (u16)gsRead16(mem); return 0;
		case 0x18000000: *out = 0; return 0;
		case 0x1a000000: *out = (u16)ba0R16(mem); return 0;
		case 0x1f900000:
		case 0x1f000000:
			*out = (u16)SPU2read(mem ); return 0;
			break;
		case 0x14000000:
			*out = (u16)DEV9read16(mem & ~0x04000000);
			SysPrintf("DEV9 read8 %8.8lx: %2.2lx\n", mem & ~0x04000000, *out);
			return 0;

		default:
			*out = *(u16*)(PS2MEM_BASE+mem);
			return 0;
	}
	MEM_LOG("Unknown Memory read16  from address %8.8x\n", mem);
	cpuTlbMissR(mem, cpuRegs.branch);
	return -1;
}

int __fastcall memRead32(u32 mem, u32 *out)  {

	mem = TRANSFORM_ADDR(mem);
	switch( (mem&~0xffff) ) {
		case 0x10000000: *out = hwRead32(mem); return 0;
		case 0x1f800000: *out = psxHwRead32(mem); return 0;
		case 0x12000000: *out = gsRead32(mem); return 0;
		case 0x14000000:
			*out = (u32)DEV9read32(mem & ~0x04000000);
			SysPrintf("DEV9 read8 %8.8lx: %2.2lx\n", mem & ~0x04000000, *out);
			return 0;

		default:
			*out = *(u32*)(PS2MEM_BASE+mem);
			return 0;
	}

	MEM_LOG("Unknown Memory read32  from address %8.8x (Status=%8.8x)\n", mem, cpuRegs.CP0.n.Status.val);
	cpuTlbMissR(mem, cpuRegs.branch);

	return -1;
}

int __fastcall memRead32RS(u32 mem, u64 *out)
{
	mem = TRANSFORM_ADDR(mem);
	switch( (mem&~0xffff) ) {
		case 0x10000000: *out = (s32)hwRead32(mem); return 0;
		case 0x1f800000: *out = (s32)psxHwRead32(mem); return 0;
		case 0x12000000: *out = (s32)gsRead32(mem); return 0;
		case 0x14000000:
			*out = (s32)DEV9read32(mem & ~0x04000000);
			SysPrintf("DEV9 read8 %8.8lx: %2.2lx\n", mem & ~0x04000000, *out);
			return 0;

		default:
			*out = *(s32*)(PS2MEM_BASE+mem);
			return 0;
	}

	MEM_LOG("Unknown Memory read32  from address %8.8x (Status=%8.8x)\n", mem, cpuRegs.CP0.n.Status.val);
	cpuTlbMissR(mem, cpuRegs.branch);

	return -1;
}

int __fastcall memRead32RU(u32 mem, u64 *out)
{
	mem = TRANSFORM_ADDR(mem);
	switch( (mem&~0xffff) ) {
		case 0x10000000: *out = (u32)hwRead32(mem); return 0;
		case 0x1f800000: *out = (u32)psxHwRead32(mem); return 0;
		case 0x12000000: *out = (u32)gsRead32(mem); return 0;
		case 0x14000000:
			*out = (u32)DEV9read32(mem & ~0x04000000);
			SysPrintf("DEV9 read8 %8.8lx: %2.2lx\n", mem & ~0x04000000, *out);
			return 0;

		default:
			*out = *(u32*)(PS2MEM_BASE+mem);
			return 0;
	}

	MEM_LOG("Unknown Memory read32  from address %8.8x (Status=%8.8x)\n", mem, cpuRegs.CP0.n.Status.val);
	cpuTlbMissR(mem, cpuRegs.branch);

	return -1;
}

int __fastcall memRead64(u32 mem, u64 *out)  {
	mem = TRANSFORM_ADDR(mem);
	switch( (mem&~0xffff) ) {
		case 0x10000000: *out = hwRead64(mem); return 0;
		case 0x12000000: *out = gsRead64(mem); return 0;

		default:
			*out = *(u64*)(PS2MEM_BASE+mem);
			return 0;
	}

	MEM_LOG("Unknown Memory read32  from address %8.8x (Status=%8.8x)\n", mem, cpuRegs.CP0.n.Status.val);
	cpuTlbMissR(mem, cpuRegs.branch);

	return -1;
}

int __fastcall memRead128(u32 mem, u64 *out)  {

	mem = TRANSFORM_ADDR(mem);
	switch( (mem&~0xffff) ) {
		case 0x10000000: 
			hwRead128(mem, out);
			return 0;
		case 0x12000000:
			out[0] = gsRead64(mem);
			out[1] = gsRead64(mem + 8);
			return 0;

		default:
			out[0] = *(u64*)(PS2MEM_BASE+mem);
			out[1] = *(u64*)(PS2MEM_BASE+mem+8);
			return 0;
	}

	MEM_LOG("Unknown Memory read32  from address %8.8x (Status=%8.8x)\n", mem, cpuRegs.CP0.n.Status.val);
	cpuTlbMissR(mem, cpuRegs.branch);

	return -1;
}

void __fastcall memWrite8 (u32 mem, u8  value)   {
	
	mem = TRANSFORM_ADDR(mem);
	switch( (mem&~0xffff) ) {
		case 0x1f400000: psxHw4Write8(mem, value); return;
		case 0x10000000: hwWrite8(mem, value); return;
		case 0x1f800000: psxHwWrite8(mem, value); return;
		case 0x12000000: gsWrite8(mem, value); return;
		case 0x14000000:
			DEV9write8(mem & ~0x04000000, value);
			SysPrintf("DEV9 write8 %8.8lx: %2.2lx\n", mem & ~0x04000000, value);
			return;

		default:
			*(u8*)(PS2MEM_BASE+mem) = value;
			
			if (CHECK_EEREC) {
				REC_CLEARM(mem&~3);
			}
			return;
	}
	MEM_LOG("Unknown Memory write8   to  address %x with data %2.2x\n", mem, value);
	cpuTlbMissW(mem, cpuRegs.branch);
}

void __fastcall memWrite16(u32 mem, u16 value)   {

	mem = TRANSFORM_ADDR(mem);
	switch( (mem&~0xffff) ) {
		case 0x10000000: hwWrite16(mem, value); return;
		case 0x1f800000: psxHwWrite16(mem, value); return;
		case 0x12000000: gsWrite16(mem, value); return;
		case 0x1f900000:
		case 0x1f000000: SPU2write(mem, value); return;
		case 0x14000000:
			DEV9write16(mem & ~0x04000000, value);
			SysPrintf("DEV9 write16 %8.8lx: %4.4lx\n", mem & ~0x04000000, value);
			return;

		default:
			*(u16*)(PS2MEM_BASE+mem) = value;
			if (CHECK_EEREC) {
				REC_CLEARM(mem&~3);
			}
			return;
	}
	MEM_LOG("Unknown Memory write16  to  address %x with data %4.4x\n", mem, value);
	cpuTlbMissW(mem, cpuRegs.branch);
}

void __fastcall memWrite32(u32 mem, u32 value)
{
	mem = TRANSFORM_ADDR(mem);
	switch( (mem&~0xffff) ) {
		case 0x10000000: hwWrite32(mem, value); return;
		case 0x1f800000: psxHwWrite32(mem, value); return;
		case 0x12000000: gsWrite32(mem, value); return;
		case 0x1f900000:
		case 0x1f000000: SPU2write(mem, value); return;
		case 0x14000000:
			DEV9write32(mem & ~0x4000000, value);
			SysPrintf("DEV9 write32 %8.8lx: %8.8lx\n", mem & ~0x4000000, value);
			return;

		default:
			*(u32*)(PS2MEM_BASE+mem) = value;
	
			if (CHECK_EEREC) {
				REC_CLEARM(mem);
			}
			return;
	}
	MEM_LOG("Unknown Memory write32  to  address %x with data %8.8x\n", mem, value);
	cpuTlbMissW(mem, cpuRegs.branch);
}

void __fastcall memWrite64(u32 mem, const u64* value)   {

	mem = TRANSFORM_ADDR(mem);
	switch( (mem&~0xffff) ) {
		case 0x10000000: hwWrite64(mem, *value); return;
		case 0x12000000: gsWrite64(mem, *value); return;

		default:
			*(u64*)(PS2MEM_BASE+mem) = *value;

			if (CHECK_EEREC) {
				REC_CLEARM(mem);
				REC_CLEARM(mem+4);
			}
			return;
	}
	MEM_LOG("Unknown Memory write64  to  address %x with data %8.8x_%8.8x\n", mem, (u32)((*value)>>32), (u32)value);
	cpuTlbMissW(mem, cpuRegs.branch);
}

void __fastcall memWrite128(u32 mem, const u64 *value) {

	mem = TRANSFORM_ADDR(mem);
	switch( (mem&~0xffff) ) {
		case 0x10000000: hwWrite128(mem, value); return;
		case 0x12000000:
			gsWrite64(mem, value[0]);
			gsWrite64(mem + 8, value[1]);
			return;

		default:
			*(u64*)(PS2MEM_BASE+mem) = value[0];
			*(u64*)(PS2MEM_BASE+mem+8) = value[1];

			if (CHECK_EEREC) {
				REC_CLEARM(mem);
				REC_CLEARM(mem+4);
				REC_CLEARM(mem+8);
				REC_CLEARM(mem+12);
			}
			return;
	}
	MEM_LOG("Unknown Memory write128 to  address %x with data %8.8x_%8.8x_%8.8x_%8.8x\n", mem, ((u32*)value)[3], ((u32*)value)[2], ((u32*)value)[1], ((u32*)value)[0]);
	cpuTlbMissW(mem, cpuRegs.branch);
}

#else //VIRTUAL_MEM
u32 psMPWC[(0x02000000/32)>>12];
std::vector<u32> psMPWVA[0x02000000>>12];

u8  *psM; //32mb Main Ram
u8  *psR; //4mb rom area
u8  *psR1; //256kb rom1 area (actually 196kb, but can't mask this)
u8  *psR2; // 0x00080000
u8  *psER; // 0x001C0000
u8  *psS; //0.015 mb, scratch pad

#define CHECK_MEM(mem) //MyMemCheck(mem)

void MyMemCheck(u32 mem)
{
    if( mem == 0x1c02f2a0 )
        SysPrintf("yo\n");
}

/////////////////////////////
// REGULAR MEM START 
/////////////////////////////
vtlbHandler tlb_fallback_0;
vtlbHandler tlb_fallback_1;
vtlbHandler tlb_fallback_2;
vtlbHandler tlb_fallback_3;
vtlbHandler tlb_fallback_4;
vtlbHandler tlb_fallback_5;
vtlbHandler tlb_fallback_6;
vtlbHandler tlb_fallback_7;
vtlbHandler tlb_fallback_8;

vtlbHandler vu0_micro_mem;
vtlbHandler vu1_micro_mem;

void memMapPhy()
{
	//Main mem
	vtlb_MapBlock(psM,0x00000000,0x02000000);//mirrored on first 256 mb ?
	
	//Rom
	vtlb_MapBlock(psR,0x1fc00000,0x00400000);//Writable ?
	//Rom 1
	vtlb_MapBlock(psR1,0x1e000000,0x00040000);//Writable ?
	//Rom 2 ?
	vtlb_MapBlock(psR2,0x1e400000,0x00080000);//Writable ?
	//EEProm ?
	vtlb_MapBlock(psER,0x1e040000,0x001C0000);//Writable ?

	//IOP mem
	vtlb_MapBlock(psxM,0x1c000000,0x00800000);

	//VU0:Micro
	//vtlb_MapBlock(VU0.Micro,0x11000000,0x00004000,0x1000);
	vtlb_MapHandler(vu0_micro_mem,0x11000000,0x00004000);
	//VU0:Mem
	vtlb_MapBlock(VU0.Mem,0x11004000,0x00004000,0x1000);
	//VU1:Micro
	//vtlb_MapBlock(VU1.Micro,0x11008000,0x00004000);
	vtlb_MapHandler(vu1_micro_mem,0x11008000,0x00004000);
	//VU1:Mem
	vtlb_MapBlock(VU1.Mem,0x1100c000,0x00004000);

	//These fallback to mem* stuff ...
	vtlb_MapHandler(tlb_fallback_1,0x10000000,0x10000);
	vtlb_MapHandler(tlb_fallback_6,0x12000000,0x10000);
	vtlb_MapHandler(tlb_fallback_7,0x14000000,0x10000);
	vtlb_MapHandler(tlb_fallback_4,0x18000000,0x10000);
	vtlb_MapHandler(tlb_fallback_5,0x1a000000,0x10000);
	vtlb_MapHandler(tlb_fallback_8,0x1f000000,0x10000);
	vtlb_MapHandler(tlb_fallback_3,0x1f400000,0x10000);
	vtlb_MapHandler(tlb_fallback_2,0x1f800000,0x10000);
	vtlb_MapHandler(tlb_fallback_8,0x1f900000,0x10000);
}

//Why is this required ?
void memMapKernelMem()
{
	//lower 512 mb: direct map
	//vtlb_VMap(0x00000000,0x00000000,0x20000000);
	//0x8* mirror
	vtlb_VMap(0x80000000,0x00000000,0x20000000);
	//0xa* mirror
	vtlb_VMap(0xA0000000,0x00000000,0x20000000);
}

//what do do with these ?
void memMapSupervisorMem() 
{
}

void memMapUserMem() 
{
}

template<int p>
int __fastcall _ext_memRead8 (u32 mem, u8  *out)  {

	switch (p) 
	{
		case 1: // hwm
			*out = hwRead8(mem); return 0;
		case 2: // psh
			*out = psxHwRead8(mem); return 0;
		case 3: // psh4
			*out = psxHw4Read8(mem); return 0;
		case 6: // gsm
			*out = gsRead8(mem); return 0;
		case 7: // dev9
			*out = DEV9read8(mem & ~0xa4000000);
			SysPrintf("DEV9 read8 %8.8lx: %2.2lx\n", mem & ~0xa4000000, *out);
			return 0;
	}

	MEM_LOG("Unknown Memory read32  from address %8.8x\n", mem);
	cpuTlbMissR(mem, cpuRegs.branch);

	return -1;
}
template<int p>
int __fastcall _ext_memRead16(u32 mem, u16 *out)  {
	switch (p) {
		case 1: // hwm
			*out = hwRead16(mem); return 0;
		case 2: // psh
			*out = psxHwRead16(mem); return 0;
		case 4: // b80
			MEM_LOG("b800000 Memory read16 address %x\n", mem);
			*out = 0; return 0;
		case 5: // ba0
			*out = ba0R16(mem); return 0;
		case 6: // gsm
			*out = gsRead16(mem); return 0;
		case 7: // dev9
			*out = DEV9read16(mem & ~0xa4000000);
			SysPrintf("DEV9 read16 %8.8lx: %4.4lx\n", mem & ~0xa4000000, *out);
			return 0;
		case 8: // spu2
			*out = SPU2read(mem); return 0;
	}
	MEM_LOG("Unknown Memory read16  from address %8.8x\n", mem);
	cpuTlbMissR(mem, cpuRegs.branch);

	return -1;
}
template<int p>
int __fastcall _ext_memRead32(u32 mem, u32 *out) 
{
	switch ((int)(uptr)p) {
		case 1: // hwm
			*out = hwRead32(mem); return 0;
		case 2: // psh
			*out = psxHwRead32(mem); return 0;
		case 6: // gsm
			*out = gsRead32(mem); return 0;
		case 7: // dev9
			*out = DEV9read32(mem & ~0xa4000000);
			SysPrintf("DEV9 read32 %8.8lx: %8.8lx\n", mem & ~0xa4000000, *out);
			return 0;
	}

#ifdef PCSX2_DEVBUILD
	MEM_LOG("Unknown Memory read32  from address %8.8x (Status=%8.8x)\n", mem, cpuRegs.CP0.n.Status);
#endif
	cpuTlbMissR(mem, cpuRegs.branch);

	return -1;
}
template<int p>
int __fastcall _ext_memRead64(u32 mem, u64 *out)
{
	switch ((int)(uptr)p) {
		case 1: // hwm
			*out = hwRead64(mem); return 0;
		case 6: // gsm
			*out = gsRead64(mem); return 0;
	}

#ifdef MEM_LOG
	MEM_LOG("Unknown Memory read64  from address %8.8x\n", mem);
#endif
	cpuTlbMissR(mem, cpuRegs.branch);

	return -1;
}

template<int p>
int __fastcall _ext_memRead128(u32 mem, u64 *out)
{

	switch ((int)(uptr)p) {
		case 1: // hwm
			hwRead128(mem & ~0xa0000000, out); return 0;
		case 6: // gsm
			out[0] = gsRead64((mem  ));
			out[1] = gsRead64((mem+8)); return 0;
	}

#ifdef MEM_LOG
	MEM_LOG("Unknown Memory read128 from address %8.8x\n", mem);
#endif
	cpuTlbMissR(mem, cpuRegs.branch);

	return -1;
}

template<int p>
void __fastcall _ext_memWrite8 (u32 mem, u8  value)
{

	switch ((int)(uptr)p) {
		case 1: // hwm
			hwWrite8(mem, value);
			return;
		case 2: // psh
			psxHwWrite8(mem, value); return;
		case 3: // psh4
			psxHw4Write8(mem, value); return;
		case 6: // gsm
			gsWrite8(mem, value); return;
		case 7: // dev9
			DEV9write8(mem & ~0xa4000000, value);
			SysPrintf("DEV9 write8 %8.8lx: %2.2lx\n", mem & ~0xa4000000, value);
			return;
	}

#ifdef MEM_LOG
	MEM_LOG("Unknown Memory write8   to  address %x with data %2.2x\n", mem, value);
#endif
	cpuTlbMissW(mem, cpuRegs.branch);
}
template<int p>
void __fastcall _ext_memWrite16(u32 mem, u16 value)
{
	switch ((int)(uptr)p) {
		case 1: // hwm
			hwWrite16(mem, value);
			return;
		case 2: // psh
			psxHwWrite16(mem, value); return;
		case 5: // ba0
			MEM_LOG("ba00000 Memory write16 to  address %x with data %x\n", mem, value);
			return;
		case 6: // gsm
			gsWrite16(mem, value); return;
		case 7: // dev9
			DEV9write16(mem & ~0xa4000000, value);
			SysPrintf("DEV9 write16 %8.8lx: %4.4lx\n", mem & ~0xa4000000, value);
			return;
		case 8: // spu2
			SPU2write(mem, value); return;
	}
	MEM_LOG("Unknown Memory write16  to  address %x with data %4.4x\n", mem, value);
	cpuTlbMissW(mem, cpuRegs.branch);
}
template<int p>
void __fastcall _ext_memWrite32(u32 mem, u32 value)
{
	switch ((int)(uptr)p) {
		case 1: // hwm
			hwWrite32(mem, value);
			return;
		case 2: // psh
			psxHwWrite32(mem, value); return;
		case 6: // gsm
			gsWrite32(mem, value); return;
		case 7: // dev9
			DEV9write32(mem & ~0xa4000000, value);
			SysPrintf("DEV9 write32 %8.8lx: %8.8lx\n", mem & ~0xa4000000, value);
			return;
	}
	MEM_LOG("Unknown Memory write32  to  address %x with data %8.8x\n", mem, value);
	cpuTlbMissW(mem, cpuRegs.branch);
}
template<int p>
void __fastcall _ext_memWrite64(u32 mem, const u64* value)
{

	switch (p) {
		case 1: // hwm
			hwWrite64(mem & ~0xa0000000, *value);
			return;
		case 6: // gsm
			gsWrite64(mem & ~0xa0000000, *value); return;
	}

	MEM_LOG("Unknown Memory write64  to  address %x with data %8.8x_%8.8x\n", mem, (u32)(*value>>32), (u32)*value);

	cpuTlbMissW(mem, cpuRegs.branch);
}
template<int p>
void __fastcall _ext_memWrite128(u32 mem, const u64 *value)
{
	switch (p) {
		case 1: // hwm
			hwWrite128(mem & ~0xa0000000, value);
			return;
		case 6: // gsm
			mem &= ~0xa0000000;
			gsWrite64(mem,   value[0]);
			gsWrite64(mem+8, value[1]); return;
	}

	MEM_LOG("Unknown Memory write128 to  address %x with data %8.8x_%8.8x_%8.8x_%8.8x\n", mem, ((u32*)value)[3], ((u32*)value)[2], ((u32*)value)[1], ((u32*)value)[0]);
	cpuTlbMissW(mem, cpuRegs.branch);
}

#define vtlb_RegisterHandlerTempl1(nam,t) vtlb_RegisterHandler(nam##Read8<t>,nam##Read16<t>,nam##Read32<t>,nam##Read64<t>,nam##Read128<t>, \
															   nam##Write8<t>,nam##Write16<t>,nam##Write32<t>,nam##Write64<t>,nam##Write128<t>);
template<int vunum>
int __fastcall vuMicroRead8(u32 addr,mem8_t* data)
{
	addr&=(vunum==0)?0xfff:0x3fff;
	VURegs* vu=(vunum==0)?&VU0:&VU1;

	*data=vu->Micro[addr];
	return 0;
}
template<int vunum>
int __fastcall vuMicroRead16(u32 addr,mem16_t* data)
{
	addr&=(vunum==0)?0xfff:0x3fff;
	VURegs* vu=(vunum==0)?&VU0:&VU1;

	*data=*(u16*)&vu->Micro[addr];
	return 0;
}
template<int vunum>
int __fastcall vuMicroRead32(u32 addr,mem32_t* data)
{
	addr&=(vunum==0)?0xfff:0x3fff;
	VURegs* vu=(vunum==0)?&VU0:&VU1;

	*data=*(u32*)&vu->Micro[addr];
	return 0;
}
template<int vunum>
int __fastcall vuMicroRead64(u32 addr,mem64_t* data)
{
	addr&=(vunum==0)?0xfff:0x3fff;
	VURegs* vu=(vunum==0)?&VU0:&VU1;

	*data=*(u64*)&vu->Micro[addr];
	return 0;
}
template<int vunum>
int __fastcall vuMicroRead128(u32 addr,mem128_t* data)
{
	addr&=(vunum==0)?0xfff:0x3fff;
	VURegs* vu=(vunum==0)?&VU0:&VU1;

	data[0]=*(u64*)&vu->Micro[addr];
	data[1]=*(u64*)&vu->Micro[addr+8];
	return 0;
}
template<int vunum>
void __fastcall vuMicroWrite8(u32 addr,mem8_t data)
{
	addr&=(vunum==0)?0xfff:0x3fff;
	VURegs* vu=(vunum==0)?&VU0:&VU1;

	if (vu->Micro[addr]!=data)
	{
		vu->Micro[addr]=data;

		if (vunum==0)
			Cpu->ClearVU0(addr&(~7),1);
		else
			Cpu->ClearVU1(addr&(~7),1);
	}
}
template<int vunum>
void __fastcall vuMicroWrite16(u32 addr,mem16_t data)
{
	addr&=(vunum==0)?0xfff:0x3fff;
	VURegs* vu=(vunum==0)?&VU0:&VU1;

	if (*(u16*)&vu->Micro[addr]!=data)
	{
		*(u16*)&vu->Micro[addr]=data;

		if (vunum==0)
			Cpu->ClearVU0(addr&(~7),1);
		else
			Cpu->ClearVU1(addr&(~7),1);
	}
}
template<int vunum>
void __fastcall vuMicroWrite32(u32 addr,mem32_t data)
{
	addr&=(vunum==0)?0xfff:0x3fff;
	VURegs* vu=(vunum==0)?&VU0:&VU1;

	if (*(u32*)&vu->Micro[addr]!=data)
	{
		*(u32*)&vu->Micro[addr]=data;

		if (vunum==0)
			Cpu->ClearVU0(addr&(~7),1);
		else
			Cpu->ClearVU1(addr&(~7),1);
	}
}
template<int vunum>
void __fastcall vuMicroWrite64(u32 addr,const mem64_t* data)
{
	addr&=(vunum==0)?0xfff:0x3fff;
	VURegs* vu=(vunum==0)?&VU0:&VU1;;

	if (*(u64*)&vu->Micro[addr]!=data[0])
	{
		*(u64*)&vu->Micro[addr]=data[0];

		if (vunum==0)
			Cpu->ClearVU0(addr,1);
		else
			Cpu->ClearVU1(addr,1);
	}
}
template<int vunum>
void __fastcall vuMicroWrite128(u32 addr,const mem128_t* data)
{
	addr&=(vunum==0)?0xfff:0x3fff;
	VURegs* vu=(vunum==0)?&VU0:&VU1;

	if (*(u64*)&vu->Micro[addr]!=data[0] || *(u64*)&vu->Micro[addr+8]!=data[1])
	{
		*(u64*)&vu->Micro[addr]=data[0];
		*(u64*)&vu->Micro[addr+8]=data[1];

		if (vunum==0)
			Cpu->ClearVU0(addr,2);
		else
			Cpu->ClearVU1(addr,2);
	}
}
int memInit() 
{
	if (!vtlb_Init())
		return -1;

	tlb_fallback_0=vtlb_RegisterHandlerTempl1(_ext_mem,0);
	tlb_fallback_1=vtlb_RegisterHandlerTempl1(_ext_mem,1);
	tlb_fallback_2=vtlb_RegisterHandlerTempl1(_ext_mem,2);
	tlb_fallback_3=vtlb_RegisterHandlerTempl1(_ext_mem,3);
	tlb_fallback_4=vtlb_RegisterHandlerTempl1(_ext_mem,4);
	tlb_fallback_5=vtlb_RegisterHandlerTempl1(_ext_mem,5);
	tlb_fallback_6=vtlb_RegisterHandlerTempl1(_ext_mem,6);
	tlb_fallback_7=vtlb_RegisterHandlerTempl1(_ext_mem,7);
	tlb_fallback_8=vtlb_RegisterHandlerTempl1(_ext_mem,8);

	vu0_micro_mem=vtlb_RegisterHandlerTempl1(vuMicro,0);
	vu1_micro_mem=vtlb_RegisterHandlerTempl1(vuMicro,1);

	psR  = (u8*)_aligned_malloc(0x00400010, 16);
	psR1 = (u8*)_aligned_malloc(0x00080010, 16);
	psR2 = (u8*)_aligned_malloc(0x00080010, 16);
	
	if (psxInit() == -1)
		return -1;
	
	psM  = (u8*)_aligned_malloc(0x02001000, 4096);
	psER = (u8*)_aligned_malloc(0x001C0010, 16);
	psS  = (u8*)_aligned_malloc(0x00004010, 16);
	if (psM  == NULL || psR  == NULL || psR1 == NULL || 
		psR2 == NULL || psER == NULL || psS == NULL) {
		SysMessage(_("Error allocating memory")); return -1;
	}

	memset(psM,  0, 0x02000000);
	memset(psR,  0, 0x00400010);
	memset(psR1, 0, 0x00080010);
	memset(psR2, 0, 0x00080010);
	memset(psER, 0, 0x001C0010);
	memset(psS,  0, 0x00004010);

	memMapPhy();

	memMapKernelMem();
	memMapSupervisorMem();
	memMapUserMem();

	memSetKernelMode();


#ifdef ENABLECACHE
	memset(pCache,0,sizeof(_cacheS)*64);
#endif

	return 0;
}

void memShutdown() 
{
	FREE(psM);
	FREE(psR);
	FREE(psR1);
	FREE(psR2);
	FREE(psER);
	FREE(psS);

	vtlb_Term();
}

void memSetPageAddr(u32 vaddr, u32 paddr) 
{
	//SysPrintf("memSetPageAddr: %8.8x -> %8.8x\n", vaddr, paddr);

	vtlb_VMap(vaddr,paddr,0x1000);

}

void memClearPageAddr(u32 vaddr) 
{
	//SysPrintf("memClearPageAddr: %8.8x\n", vaddr);

	vtlb_VMapUnmap(vaddr,0x1000); // -> whut ?

#ifdef FULLTLB
	memLUTRK[vaddr >> 12] = 0;
	memLUTWK[vaddr >> 12] = 0;
#endif
}
#endif // PCSX2_VIRTUAL_MEM

void loadBiosRom(const char *ext, u8 *dest) {
	struct stat buf;
	char Bios1[g_MaxPath];
	char Bios[g_MaxPath];
	FILE *fp;
	char *ptr;
	int i;

	CombinePaths( Bios, Config.BiosDir, Config.Bios );

	sprintf(Bios1, "%s.%s", Bios, ext);
	if (stat(Bios1, &buf) != -1) {	
		fp = fopen(Bios1, "rb");
		fread(dest, 1, buf.st_size, fp);
		fclose(fp);
		return;
	}

	sprintf(Bios1, "%s", Bios);
	ptr = Bios1; i = strlen(Bios1);
	while (i > 0) { if (ptr[i] == '.') break; i--; }
	ptr[i+1] = 0;
	strcat(Bios1, ext);
	if (stat(Bios1, &buf) != -1) {	
		fp = fopen(Bios1, "rb");
		fread(dest, 1, buf.st_size, fp);
		fclose(fp);
		return;
	}

	sprintf(Bios1, "%s%s.bin", Config.BiosDir, ext);
	if (stat(Bios1, &buf) != -1) {	
		fp = fopen(Bios1, "rb");
		fread(dest, 1, buf.st_size, fp);
		fclose(fp);
		return;
	}
	SysPrintf("\n\n\n");
	SysPrintf("**************\n");
	SysPrintf("%s NOT FOUND\n", ext);
	SysPrintf("**************\n\n\n");
}

int memReset() {
	struct stat buf;
	char Bios[g_MaxPath];
	FILE *fp;

#ifdef PCSX2_VIRTUAL_MEM
	DWORD OldProtect;
	memset(PS2MEM_BASE, 0, 0x02000000);
	memset(PS2MEM_SCRATCH, 0, 0x00004000);
#else
	memset(psM, 0, 0x02000000);
	memset(psS, 0, 0x00004000);
#endif

	CombinePaths( Bios, Config.BiosDir, Config.Bios );

	if (stat(Bios, &buf) == -1) {	
		SysMessage(_("Unable to load bios: '%s', PCSX2 can't run without that"), Bios);
		return 0;
	}

#ifdef PCSX2_VIRTUAL_MEM

#ifdef _WIN32
	// make sure can write
	VirtualProtect(PS2MEM_ROM, 0x00400000, PAGE_READWRITE, &OldProtect);
	VirtualProtect(PS2MEM_ROM1, 0x00040000, PAGE_READWRITE, &OldProtect);
	VirtualProtect(PS2MEM_ROM2, 0x00080000, PAGE_READWRITE, &OldProtect);
	VirtualProtect(PS2MEM_EROM, 0x001C0000, PAGE_READWRITE, &OldProtect);
#else
	mprotect(PS2EMEM_ROM, 0x00400000, PROT_READ|PROT_WRITE);
	mprotect(PS2EMEM_ROM1, 0x00400000, PROT_READ|PROT_WRITE);
	mprotect(PS2EMEM_ROM2, 0x00800000, PROT_READ|PROT_WRITE);
	mprotect(PS2EMEM_EROM, 0x001C0000, PROT_READ|PROT_WRITE);
#endif

#endif

	fp = fopen(Bios, "rb");
	fread(PS2MEM_ROM, 1, buf.st_size, fp);
	fclose(fp);

	BiosVersion = GetBiosVersion();
	SysPrintf("Bios Version %d.%d\n", BiosVersion >> 8, BiosVersion & 0xff);

	//injectIRX("host.irx");	//not fully tested; still buggy

#ifndef PCSX2_VIRTUAL_MEM
	// reset memLUT (?)
	vtlb_VMap(0x00000000,0x00000000,0x20000000);
	vtlb_VMapUnmap(0x20000000,0x60000000);
#endif

	loadBiosRom("rom1", PS2MEM_ROM1);
	loadBiosRom("rom2", PS2MEM_ROM2);
	loadBiosRom("erom", PS2MEM_EROM);

#ifdef PCSX2_VIRTUAL_MEM

#ifdef _WIN32
	VirtualProtect(PS2MEM_ROM, 0x00400000, PAGE_READONLY, &OldProtect);
	VirtualProtect(PS2MEM_ROM1, 0x00040000, PAGE_READONLY, &OldProtect);
	VirtualProtect(PS2MEM_ROM2, 0x00080000, PAGE_READONLY, &OldProtect);
	VirtualProtect(PS2MEM_EROM, 0x001C0000, PAGE_READONLY, &OldProtect);
#else
	mprotect(PS2EMEM_ROM, 0x00400000, PROT_READ);
	mprotect(PS2EMEM_ROM1, 0x00400000, PROT_READ);
	mprotect(PS2EMEM_ROM2, 0x00800000, PROT_READ);
	mprotect(PS2EMEM_EROM, 0x001C0000, PROT_READ);
#endif

#endif
	return 1;
}

void memSetKernelMode() {
	//Do something here
	MemMode = 0;
}

void memSetSupervisorMode() {
}

void memSetUserMode() {

}

#ifndef PCSX2_VIRTUAL_MEM
int mmap_GetRamPageInfo(void* ptr)
{
	u32 offset=((u8*)ptr-psM);
	if (offset>=0x02000000)
		return -1; //not in ram, no tracking done ...
	offset>>=12;
	return (psMPWC[(offset/32)]&(1<<(offset&31)))?1:0;
}
void mmap_MarkCountedRamPage(void* ptr,u32 vaddr)
{
#ifdef _WIN32
	DWORD old;
	VirtualProtect(ptr,1,PAGE_READONLY,&old);
#else
	mprotect(ptr, 1, PROT_READ);
#endif
	
	u32 offset=((u8*)ptr-psM);
	offset>>=12;

	for (u32 i=0;i<psMPWVA[offset].size();i++)
	{
		if (psMPWVA[offset][i]==vaddr)
			return;
	}
	psMPWVA[offset].push_back(vaddr);
}
void mmap_ResetBlockTracking()
{
	SysPrintf("vtlb/mmap: Block Tracking reseted ..\n");
	memset(psMPWC,0,sizeof(psMPWC));
	for(u32 i=0;i<(0x02000000>>12);i++)
	{
		psMPWVA[i].clear();
	}
#ifdef _WIN32
	DWORD old;
	VirtualProtect(psM,0x02000000,PAGE_READWRITE,&old);
#else
	mprotect(psM,0x02000000, PROT_READ|PROT_WRITE);
#endif
}

#ifdef _WIN32
int SysPageFaultExceptionFilter(EXCEPTION_POINTERS* eps)
{
	struct _EXCEPTION_RECORD* ExceptionRecord = eps->ExceptionRecord;
	struct _CONTEXT* ContextRecord = eps->ContextRecord;
	
	if (eps->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
	{
		return EXCEPTION_CONTINUE_SEARCH;
	}

	// get bad virtual address
	u32 offset = (u8*)eps->ExceptionRecord->ExceptionInformation[1]-psM;

	if (offset>=0x02000000)
		return EXCEPTION_CONTINUE_SEARCH;

	DWORD old;
	VirtualProtect(&psM[offset],1,PAGE_READWRITE,&old);

	offset>>=12;
	psMPWC[(offset/32)]|=(1<<(offset&31));

	for (u32 i=0;i<psMPWVA[offset].size();i++)
	{
		Cpu->Clear(psMPWVA[offset][i],0x1000);
	}
	psMPWVA[offset].clear();

	return EXCEPTION_CONTINUE_EXECUTION;
}
#endif
#endif