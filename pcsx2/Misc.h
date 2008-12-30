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

#include "PS2Etypes.h"
#include "System.h"
#include "SaveState.h"
#include "assert.h"

// --->> GNU GetText / NLS

#ifdef ENABLE_NLS

#ifdef _WIN32
#include "libintlmsc.h"
#else
#include <locale.h>
#include <libintl.h>
#endif

#undef _
#define _(String) dgettext (PACKAGE, String)
#ifdef gettext_noop
#  define N_(String) gettext_noop (String)
#else
#  define N_(String) (String)
#endif

#else

#define _(msgid) msgid
#define N_(msgid) msgid

#endif		// ENABLE_NLS

// <<--- End GNU GetText / NLS 


// --->> Path Utilities [PathUtil.c]

#define g_MaxPath 255			// 255 is safer with antiquitated Win32 ASCII APIs.
extern int g_Error_PathTooLong;

int isPathRooted( const char* path );
void CombinePaths( char* dest, const char* srcPath, const char* srcFile );

// <<--- END Path Utilities [PathUtil.c]

#define PCSX2_GSMULTITHREAD 1 // uses multithreaded gs
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
#define CHECK_EEREC (Config.Options&PCSX2_EEREC)
#define CHECK_COP2REC (Config.Options&PCSX2_COP2REC) // goes with ee option
//------------ SPEED/MISC HACKS!!! ---------------
#define CHECK_EESYNC_HACK	(Config.Hacks & 0x1)
#define CHECK_IOPSYNC_HACK	(Config.Hacks & 0x10)
#define CHECK_EE_IOP_EXTRA	(Config.Hacks & 0x20)
#define CHECK_ESCAPE_HACK	(Config.Hacks & 0x400)
//------------ SPECIAL GAME FIXES!!! ---------------
#define CHECK_FPUCLAMPHACK	(Config.GameFixes & 0x4) // Special Fix for Tekken 5, different clamping for FPU (sets NaN to zero; doesn't clamp infinities)
#define CHECK_VUCLIPHACK	(Config.GameFixes & 0x2) // Special Fix for GoW, updates the clipflag differently in recVUMI_CLIP() (note: turning this hack on, breaks Rockstar games)
#define CHECK_VUBRANCHHACK	(Config.GameFixes & 0x8) // Special Fix for Magna Carta (note: Breaks Crash Bandicoot)
//------------ Advanced Options!!! ---------------
#define CHECK_VU_OVERFLOW		(Config.vuOptions & 0x1)
#define CHECK_VU_EXTRA_OVERFLOW	(Config.vuOptions & 0x2) // If enabled, Operands are checked for infinities before being used in the VU recs
#define CHECK_VU_SIGN_OVERFLOW	(Config.vuOptions & 0x4)
#define CHECK_VU_UNDERFLOW		(Config.vuOptions & 0x8)
#define CHECK_VU_EXTRA_FLAGS 0	// Always disabled now, doesn't seem to affect games positively. // Sets correct flags in the VU recs
#define CHECK_FPU_OVERFLOW			(Config.eeOptions & 0x1)
#define CHECK_FPU_EXTRA_OVERFLOW	(Config.eeOptions & 0x2) // If enabled, Operands are checked for infinities before being used in the FPU recs
#define CHECK_FPU_EXTRA_FLAGS 0	// Always disabled now, doesn't seem to affect games positively. // Sets correct flags in the FPU recs
#define DEFAULT_eeOptions	0x01
#define DEFAULT_vuOptions	0x01
//------------ DEFAULT sseMXCSR VALUES!!! ---------------
#define DEFAULT_sseMXCSR	0x9fc0 //disable all exception, round to 0, flush to 0
#define DEFAULT_sseVUMXCSR	0x7f80 //disable all exception

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

	bool PsxOut;
	bool Profiler;

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
	u32 eeOptions;
	u32 vuOptions;
};

extern PcsxConfig Config;
extern u32 BiosVersion;
extern char CdromId[12];
extern uptr pDsp;		// what the hell is this unused piece of crap passed to every plugin for? (air)

int LoadCdrom();
int CheckCdrom();
int GetPS2ElfName(char*);

extern const char *LabelAuthors;
extern const char *LabelGreets;

void SaveGSState(const char *file);
void LoadGSState(const char *file);

char *ParseLang(char *id);
void ProcessFKeys(int fkey, int shift); // processes fkey related commands value 1-12

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
extern void FreezeXMMRegs_(int save);
extern bool g_EEFreezeRegs;
#define FreezeXMMRegs(save) if( g_EEFreezeRegs ) { FreezeXMMRegs_(save); }

#ifndef __x86_64__
	void FreezeMMXRegs_(int save);
#define FreezeMMXRegs(save) if( g_EEFreezeRegs ) { FreezeMMXRegs_(save); }
#else
#	define FreezeMMXRegs(save)
#endif

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
	#define memcpy_raz_ memcpy
	#define memcpy_raz_u memcpy
#endif


u8 memcmp_mmx(const void* src1, const void* src2, int cmpsize);
void memxor_mmx(void* dst, const void* src1, int cmpsize);

#ifdef	_MSC_VER
#pragma pack()
#endif

void injectIRX(const char *filename);

// aligned_malloc: Implement/declare linux equivalents here!
#if !defined(_MSC_VER) && !defined(HAVE_ALIGNED_MALLOC)

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

extern void InitCPUTicks();
extern u64 GetTickFrequency();
extern u64 GetCPUTicks();


#endif /* __MISC_H__ */

