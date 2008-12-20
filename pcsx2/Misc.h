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
 *	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef __MISC_H__
#define __MISC_H__

#include <stddef.h>
#include <malloc.h>
#include <assert.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#endif

#include "PS2Etypes.h"

// compile-time assert
#ifndef C_ASSERT
#define C_ASSERT(e) typedef char __C_ASSERT__[(e)?1:-1]
#endif

#ifdef __x86_64__
#define X86_32CODE(x)
#else
#define X86_32CODE(x) x
#endif

#ifndef __LINUX__
#define __unused
#endif

// --->> Path Utilities [PathUtil.c]

#define g_MaxPath 255			// 255 is safer with antiquitated Win32 ASCII APIs.
extern int g_Error_PathTooLong;

int isPathRooted( const char* path );
void CombinePaths( char* dest, const char* srcPath, const char* srcFile );

// <<--- END Path Utilities [PathUtil.c]

#define PCSX2_GSMULTITHREAD 1 // uses multithreaded gs
//#define PCSX2_DUALCORE 2 // speed up for dual cores
#define PCSX2_FRAMELIMIT 4 // limits frames to normal speeds
#define PCSX2_EEREC 0x10
#define PCSX2_VU0REC 0x20
#define PCSX2_VU1REC 0x40
#define PCSX2_COP2REC 0x80
#define PCSX2_FORCEABS 0x100
#define PCSX2_FRAMELIMIT_MASK 0xc00
#define PCSX2_FRAMELIMIT_NORMAL 0x000
#define PCSX2_FRAMELIMIT_LIMIT 0x400
#define PCSX2_FRAMELIMIT_SKIP 0x800
#define PCSX2_FRAMELIMIT_VUSKIP 0xc00

#define CHECK_MULTIGS (Config.Options&PCSX2_GSMULTITHREAD)
//#define CHECK_DUALCORE (Config.Options&PCSX2_DUALCORE)
#define CHECK_EEREC (Config.Options&PCSX2_EEREC)
#define CHECK_COP2REC (Config.Options&PCSX2_COP2REC) // goes with ee option
//------------ SPEED/MISC HACKS!!! ---------------
#define CHECK_OVERFLOW (!(Config.Hacks & 0x2))
#define CHECK_EXTRA_OVERFLOW (Config.Hacks & 0x40) // If enabled, Operands are checked for infinities before being used in the VU recs
#define CHECK_FPU_OVERFLOW (!(Config.Hacks & 0x800))
#define CHECK_FPU_EXTRA_OVERFLOW (Config.Hacks & 0x1000) // If enabled, Operands are checked for infinities before being used in the FPU recs
#define CHECK_EESYNC_HACK (Config.Hacks & 0x1)
#define CHECK_IOPSYNC_HACK (Config.Hacks & 0x10)
#define CHECK_EE_IOP_EXTRA (Config.Hacks & 0x20)
#define CHECK_UNDERFLOW (!(Config.Hacks & 0x8))
//#define CHECK_DENORMALS ((Config.Hacks & 0x400) ? 0xffc0 : 0x7f80) //If enabled, Denormals are Zero for the recs and flush to zero is enabled as well
#define CHECK_VU_EXTRA_FLAGS 0 // Always disabled now, doesn't seem to affect games positively. // (!(Config.Hacks & 0x100)) // Sets correct flags in the VU recs
#define CHECK_FPU_EXTRA_FLAGS 0 // Always disabled now, doesn't seem to affect games positively. // (!(Config.Hacks & 0x200)) // Sets correct flags in the FPU recs
#define CHECK_ESCAPE_HACK (Config.Hacks & 0x400)
//------------ SPECIAL GAME FIXES!!! ---------------
#define CHECK_FPUCLAMPHACK (Config.GameFixes & 0x4) // Special Fix for Tekken 5, different clamping for FPU (sets NaN to zero; doesn't clamp infinities)
#define CHECK_VUCLIPHACK (Config.GameFixes & 0x2) // Special Fix for GoW, updates the clipflag differently in recVUMI_CLIP() (note: turning this hack on, breaks Rockstar games)
#define CHECK_VUBRANCHHACK (Config.GameFixes & 0x8) // Special Fix for Magna Carta (note: Breaks Crash Bandicoot)

//------------ DEFAULT sseMXCSR VALUES!!! ---------------
#define DEFAULT_sseMXCSR 0x9fc0 //disable all exception, round to 0, flush to 0
#define DEFAULT_sseVUMXCSR 0x7f80 //disable all exception

#define CHECK_FRAMELIMIT (Config.Options&PCSX2_FRAMELIMIT_MASK)

#define CHECK_VU0REC (Config.Options&PCSX2_VU0REC)
#define CHECK_VU1REC (Config.Options&PCSX2_VU1REC)

struct PcsxConfig {
	char Bios[g_MaxPath];
	char GS[g_MaxPath];
	char PAD1[g_MaxPath];
	char PAD2[g_MaxPath];
	char SPU2[g_MaxPath];
	char CDVD[g_MaxPath];
	char DEV9[g_MaxPath];
	char USB[g_MaxPath];
	char FW[g_MaxPath];
	char Mcd1[g_MaxPath];
	char Mcd2[g_MaxPath];
	char PluginsDir[g_MaxPath];
	char BiosDir[g_MaxPath];
	char Lang[g_MaxPath];
	u32 Options; // PCSX2_X options
	int PsxOut;
	int PsxType;
	int Cdda;
	int Mdec;
	int Patch;
	int ThPriority;
	int CustomFps;
	int Hacks;
	int GameFixes;
	int CustomFrameSkip;
	int CustomConsecutiveFrames;
	int CustomConsecutiveSkip;
	u32 sseMXCSR;
	u32 sseVUMXCSR;
};

extern PcsxConfig Config;
extern u32 BiosVersion;
extern char CdromId[12];

int LoadCdrom();
int CheckCdrom();
int GetPS2ElfName(char*);

extern const char *LabelAuthors;
extern const char *LabelGreets;

// --->> Savestate stuff [PathUtil.c]

// Savestate Versioning!
//  If you make changes to the savestate version, please increment the value below.

#ifdef PCSX2_VIRTUAL_MEM
static const u32 g_SaveVersion = 0x7a300010;
#else
static const u32 g_SaveVersion = 0x8b400000;
#endif

int SaveState(const char *file);
int LoadState(const char *file);
int CheckState(const char *file);

int SaveGSState(const char *file);
int LoadGSState(const char *file);

#define gzfreeze(ptr, size) \
	if (Mode == 1) gzwrite(f, ptr, size); \
	else if (Mode == 0) gzread(f, ptr, size);

#define gzfreezel(ptr) gzfreeze(ptr, sizeof(ptr))

// <<--- End Savestate Stuff

char *ParseLang(char *id);
void ProcessFKeys(int fkey, int shift); // processes fkey related commands value 1-12

#ifdef _WIN32

void ListPatches (HWND hW);
int ReadPatch (HWND hW, char fileName[1024]);
char * lTrim (char *s);
BOOL Save_Patch_Proc( char * filename );

#else

// functions that linux lacks
#define Sleep(seconds) usleep(1000*(seconds))

#include <sys/timeb.h>

static __forceinline u32 timeGetTime()
{
	struct timeb t;
	ftime(&t);
	return (u32)(t.time*1000+t.millitm);
}

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#define BOOL int

#undef TRUE
#define TRUE  1
#undef FALSE
#define FALSE 0

#ifndef strnicmp
#define strnicmp strncasecmp
#endif

#ifndef stricmp
#define stricmp strcasecmp
#endif

#endif

#define DIRENTRY_SIZE 16

#if defined(_MSC_VER)
#pragma pack(1)
#endif

struct romdir{
	char fileName[10];
	u16 extInfoSize;
	u32 fileSize;
#if defined(_MSC_VER)
};						//+22
#else
} __attribute__((packed));
#endif

u32 GetBiosVersion();
int IsBIOS(char *filename, char *description);

// check to see if needs freezing
#ifdef PCSX2_NORECBUILD
#define FreezeMMXRegs(save)
#define FreezeXMMRegs(save)
#else
extern void FreezeXMMRegs_(int save);
extern bool g_EEFreezeRegs;
#define FreezeXMMRegs(save) if( g_EEFreezeRegs ) { FreezeXMMRegs_(save); }

#ifndef __x86_64__
void FreezeMMXRegs_(int save);
#define FreezeMMXRegs(save) if( g_EEFreezeRegs ) { FreezeMMXRegs_(save); }
#else
#define FreezeMMXRegs(save)
#endif

#endif


#ifdef PCSX2_NORECBUILD
#define memcpy_fast memcpy
#else

#if defined(_WIN32) && !defined(__x86_64__)
// faster memcpy
void __fastcall memcpy_raz_u(void *dest, const void *src, size_t bytes);
void __fastcall memcpy_raz_(void *dest, const void *src, size_t qwc);
void * memcpy_amd_(void *dest, const void *src, size_t n);
#define memcpy_fast memcpy_amd_
//#define memcpy_fast memcpy //Dont use normal memcpy, it has sse in 2k5!
#else
// for now disable linux fast memcpy
#define memcpy_fast memcpy
#endif

#endif

u8 memcmp_mmx(const void* src1, const void* src2, int cmpsize);
void memxor_mmx(void* dst, const void* src1, int cmpsize);

#ifdef	_MSC_VER
#pragma pack()
#endif

void __Log(const char *fmt, ...);
void injectIRX(char *filename);

#if !defined(_MSC_VER) && !defined(HAVE_ALIGNED_MALLOC)

// declare linux equivalents
static  __forceinline void* pcsx2_aligned_malloc(size_t size, size_t align)
{
    assert( align < 0x10000 );
	char* p = (char*)malloc(size+align);
	int off = 2+align - ((int)(uptr)(p+2) % align);

	p += off;
	*(u16*)(p-2) = off;

	return p;
}

static __forceinline void pcsx2_aligned_free(void* pmem)
{
    if( pmem != NULL ) {
        char* p = (char*)pmem;
        free(p - (int)*(u16*)(p-2));
    }
}

#define _aligned_malloc pcsx2_aligned_malloc
#define _aligned_free pcsx2_aligned_free

#endif

// cross-platform atomic operations
#ifndef _WIN32
typedef void* PVOID;

/*inline unsigned long _Atomic_swap(unsigned long * __p, unsigned long __q) {
 #       if __mips < 3 || !(defined (_ABIN32) || defined(_ABI64))
             return test_and_set(__p, __q);
 #       else
             return __test_and_set(__p, (unsigned long)__q);
 #       endif
 }*/
static __forceinline void InterlockedExchangePointer(PVOID volatile* Target, void* Value)
{
#ifdef __x86_64__
	__asm__ __volatile__(".intel_syntax\n"
						 "lock xchg [%0], %%rax\n"
						 ".att_syntax\n" : : "r"(Target), "a"(Value) : "memory" );
#else
	__asm__ __volatile__(".intel_syntax\n"
						 "lock xchg [%0], %%eax\n"
						 ".att_syntax\n" : : "r"(Target), "a"(Value) : "memory" );
#endif
}

static __forceinline long InterlockedExchange(long volatile* Target, long Value)
{
	__asm__ __volatile__(".intel_syntax\n"
						 "lock xchg [%0], %%eax\n"
						 ".att_syntax\n" : : "r"(Target), "a"(Value) : "memory" );
	return 0; // The only function that even looks at this is a debugging function
}

static __forceinline long InterlockedExchangeAdd(long volatile* Addend, long Value)
{
	__asm__ __volatile__(".intel_syntax\n"
						 "lock xadd [%0], %%eax\n"
						 ".att_syntax\n" : : "r"(Addend), "a"(Value) : "memory" );
	return 0; // The return value is never looked at.
}

static __forceinline long InterlockedIncrement( volatile long* Addend )
{
	return InterlockedExchangeAdd( Addend, 1 );
}

static __forceinline long InterlockedDecrement( volatile long* Addend )
{
	return InterlockedExchangeAdd( Addend, -1 );
}

static __forceinline long InterlockedCompareExchange(volatile long *dest, long exch, long comp)
{
	long old;

#ifdef __x86_64__
	  __asm__ __volatile__ 
	(
		"lock; cmpxchgq %q2, %1"
		: "=a" (old), "=m" (*dest)
		: "r" (exch), "m" (*dest), "0" (comp)); 
#else
	__asm__ __volatile__
	(
		"lock; cmpxchgl %2, %0"
		: "=m" (*dest), "=a" (old)
		: "r" (exch), "m" (*dest), "a" (comp)
	);
#endif
	
	return(old);
}

static __forceinline long InterlockedCompareExchangePointer(PVOID volatile *dest, PVOID exch, long comp)
{
	long old;

	// Note: This *should* be 32/64 compatibles since the assembler should pick the opcode
	// that matches the size of the pointer type, so no need to ifdef it like the std non-compare
	// exchange.

#ifdef __x86_64__
	__asm__ __volatile__
	( 
		"lock; cmpxchgq %q2, %1"
		: "=a" (old), "=m" (*dest)
		: "r" (exch), "m" (*dest), "0" (comp)
	);
#else
	__asm__ __volatile__
	(
		"lock; cmpxchgl %2, %0"
		: "=m" (*dest), "=a" (old)
		: "r" (exch), "m" (*dest), "a" (comp)
	);
#endif
	return(old);
}
#endif

// define some overloads for InterlockedExchanges, for commonly used types.

// Note: _unused is there simply to get rid of a few Linux compiler warnings while
// debugging, as any compiler warnings in Misc.h get repeated x100 or so.
__unused static void AtomicExchange( u32& Target, u32 value )
{
	InterlockedExchange( (volatile LONG*)&Target, value );
}

__unused static void AtomicIncrement( u32& Target )
{
	InterlockedIncrement( (volatile LONG*)&Target );
}

__unused static void AtomicDecrement( u32& Target )
{
	InterlockedDecrement( (volatile LONG*)&Target );
}

__unused static void AtomicExchange( s32& Target, s32 value )
{
	InterlockedExchange( (volatile LONG*)&Target, value );
}

__unused static void AtomicIncrement( s32& Target )
{
	InterlockedIncrement( (volatile LONG*)&Target );
}

__unused static void AtomicDecrement( s32& Target )
{
	InterlockedDecrement( (volatile LONG*)&Target );
}

// No fancy templating or overloading can save us from having to use C-style dereferences here.
#define AtomicExchangePointer( target, value ) \
	InterlockedExchangePointer( reinterpret_cast<PVOID volatile*>(&target), reinterpret_cast<uptr>(value) )

extern void InitCPUTicks();
extern u64 GetTickFrequency();
extern u64 GetCPUTicks();

// Timeslice releaser for those many idle loop spots through out PCSX2.
extern void _TIMESLICE();

#endif /* __MISC_H__ */

