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

#ifndef __MISC_H__
#define __MISC_H__

#include <stddef.h>
#include <malloc.h>

#undef s_addr

// compile-time assert
#ifndef C_ASSERT
#define C_ASSERT(e) typedef char __C_ASSERT__[(e)?1:-1]
#endif

#define PCSX2_GSMULTITHREAD 1 // uses multithreaded gs
#define PCSX2_DUALCORE 2 // speed up for dual cores
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
#define CHECK_DUALCORE (Config.Options&PCSX2_DUALCORE)
#define CHECK_EEREC (Config.Options&PCSX2_EEREC)
#define CHECK_COP2REC (Config.Options&PCSX2_COP2REC) // goes with ee option
#define CHECK_FORCEABS 1// always on, (Config.Options&PCSX2_FORCEABS)

#define CHECK_FRAMELIMIT (Config.Options&PCSX2_FRAMELIMIT_MASK)

//#ifdef PCSX2_DEVBUILD
#define CHECK_VU0REC (Config.Options&PCSX2_VU0REC)
#define CHECK_VU1REC (Config.Options&PCSX2_VU1REC)
//#else
//// force to VU recs all the time
//#define CHECK_VU0REC 1
//#define CHECK_VU1REC 1
//
//#endif

typedef struct {
	char Bios[256];
	char GS[256];
	char PAD1[256];
	char PAD2[256];
	char SPU2[256];
	char CDVD[256];
	char DEV9[256];
	char USB[256];
	char FW[256];
	char Mcd1[256];
	char Mcd2[256];
	char PluginsDir[256];
	char BiosDir[256];
	char Lang[256];
	u32 Options; // PCSX2_X options
	int PsxOut;
	int PsxType;
	int Cdda;
	int Mdec;
	int Patch;
	int ThPriority;
	int SafeCnts;
} PcsxConfig;

extern PcsxConfig Config;
extern u32 BiosVersion;
extern char CdromId[12];

#define gzfreeze(ptr, size) \
	if (Mode == 1) gzwrite(f, ptr, size); \
	else if (Mode == 0) gzread(f, ptr, size);

#define gzfreezel(ptr) gzfreeze(ptr, sizeof(ptr))

int LoadCdrom();
int CheckCdrom();
int GetPS2ElfName(char*);

extern char *LabelAuthors;
extern char *LabelGreets;
int SaveState(char *file);
int LoadState(char *file);
int CheckState(char *file);

int SaveGSState(char *file);
int LoadGSState(char *file);

char *ParseLang(char *id);

#ifdef __WIN32__
void ListPatches (HWND hW);
int ReadPatch (HWND hW, char fileName[1024]);
char * lTrim (char *s);
BOOL Save_Patch_Proc( char * filename );
#endif

#define DIRENTRY_SIZE 16

#if defined(__WIN32__)
#pragma pack(1)
#endif

struct romdir{
	char fileName[10];
	u16 extInfoSize;
	u32 fileSize;
#if defined(__WIN32__)
};						//+22
#else
} __attribute__((packed));
#endif

u32 GetBiosVersion();
int IsBIOS(char *filename, char *description);

void * memcpy_amd(void *dest, const void *src, size_t n);
u8 memcmp_mmx(const void* src1, const void* src2, int cmpsize);
void memxor_mmx(void* dst, const void* src1, int cmpsize);

#ifdef	__WIN32__
#pragma pack()
#endif

void __Log(char *fmt, ...);
void injectIRX(char *filename);

#if !defined(_MSC_VER) && !defined(HAVE_ALIGNED_MALLOC)

// declare linux equivalents
inline void* pcsx2_aligned_malloc(size_t size, size_t align)
{
	char* p = (char*)malloc(size+align+1);
	int off = 1+align - ((int)(p+1) % align);

	p += off;
	p[-1] = off;

	return p;
}

inline void pcsx2_aligned_free(void* pmem)
{
	char* p = (char*)pmem;
	free(p - (int)p[-1]);
}

#define _aligned_malloc pcsx2_aligned_malloc
#define _aligned_free pcsx2_aligned_free

#endif

// cross-platform atomic operations
#if defined (__MSCW32__)

LONG  __cdecl _InterlockedIncrement(LONG volatile *Addend);
LONG  __cdecl _InterlockedDecrement(LONG volatile *Addend);
LONG  __cdecl _InterlockedCompareExchange(LPLONG volatile Dest, LONG Exchange, LONG Comp);
LONG  __cdecl _InterlockedExchange(LPLONG volatile Target, LONG Value);
PVOID __cdecl _InterlockedExchangePointer(PVOID volatile* Target, PVOID Value);

LONG  __cdecl _InterlockedExchangeAdd(LPLONG volatile Addend, LONG Value);
LONG  __cdecl _InterlockedAnd(LPLONG volatile Addend, LONG Value);

#pragma intrinsic (_InterlockedCompareExchange)
#define InterlockedCompareExchange _InterlockedCompareExchange

#pragma intrinsic (_InterlockedExchange)
#define InterlockedExchange _InterlockedExchange 

#pragma intrinsic (_InterlockedExchangeAdd)
#define InterlockedExchangeAdd _InterlockedExchangeAdd

#pragma intrinsic (_InterlockedIncrement)
#define InterlockedIncrement _InterlockedIncrement

#pragma intrinsic (_InterlockedDecrement)
#define InterlockedDecrement _InterlockedDecrement

#pragma intrinsic (_InterlockedAnd)
#define InterlockedAnd _InterlockedAnd

#elif defined(__LINUX__)

#endif

#endif /* __MISC_H__ */
