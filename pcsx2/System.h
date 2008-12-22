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

#ifndef __SYSTEM_H__
#define __SYSTEM_H__

int  SysInit();							// Init mem and plugins
int  SysReset();						// Resets mem
void SysUpdate();						// Called on VBlank (to update i.e. pads)
void SysRunGui();						// Returns to the Gui
void SysClose();						// Close mem and plugins
void *SysLoadLibrary(const char *lib);	// Loads Library
void *SysLoadSym(void *lib, const char *sym);	// Loads Symbol from Library
const char *SysLibError();				// Gets previous error loading sysbols
void SysCloseLibrary(void *lib);		// Closes Library

// Causes a pop-up to appear with the specified message.  Use this to issue
// critical or fatal messages to the user.
void SysMessage(const char *fmt, ...);

// Maps a block of memory for use as a recompiled code buffer.
// The allocated block has code execution privliges.
// Returns NULL on allocation failure.
void *SysMmap(uptr base, u32 size);

// Unamps a block allocated by SysMmap
void SysMunmap(uptr base, u32 size);

// Writes text to the console.
// *DEPRECIATED* Use Console namespace methods instead.
void SysPrintf(const char *fmt, ...);	// *DEPRECIATED* 


// Console Namespace -- Replacements for SysPrintf.
// SysPrintf is depreciated -- We should phase these in over time.
namespace Console
{
	extern void Open();
	extern void Close();
	extern void SetTitle( const char* title );

	// The following Write functions return bool so that we can use macros to exclude
	// them from different buildypes.  The return values are always zero.

	extern bool __fastcall WriteLn();
	extern bool __fastcall Write( const char* fmt );
	extern bool __fastcall WriteLn( const char* fmt );
	extern bool Format( const char* fmt, ... );
	extern bool FormatLn( const char* fmt, ... );
}

//////////////////////////////////////////////////////////////
// Macros for ifdef'ing out specific lines of code.

#ifdef PCSX2_DEVBUILD

#	define DEVCODE(x) (x)
#	define DevCon Console
#	define PUBCODE 0&&

static const bool IsDevBuild = true;

#else

#	define DEVCODE(x) 0&&(x)
#	define DevCon 0&&Console
#	define PUBCODE(x) (x)

static const bool IsDevBuild = false;

#endif

#ifdef _DEBUG

static const bool IsDebugBuild = true;

#	define DBGCODE(x) (x)
#	define DbgCon 0&&Console

#else

#	define DBGCODE(x) 0&&
#	define DbgCon 0&&Console

static const bool IsDebugBuild = false;
#endif

#ifdef PCSX2_VIRTUAL_MEM

struct PSMEMORYBLOCK
{
#ifdef _WIN32
    int NumberPages;
	uptr* aPFNs;
	uptr* aVFNs; // virtual pages that own the physical pages
#else
    int fd; // file descriptor
    char* pname; // given name
    int size; // size of allocated region
#endif
};

int SysPhysicalAlloc(u32 size, PSMEMORYBLOCK* pblock);
void SysPhysicalFree(PSMEMORYBLOCK* pblock);
int SysVirtualPhyAlloc(void* base, u32 size, PSMEMORYBLOCK* pblock);
void SysVirtualFree(void* lpMemReserved, u32 size);

// returns 1 if successful, 0 otherwise
int SysMapUserPhysicalPages(void* Addr, uptr NumPages, uptr* pblock, int pageoffset);

// call to enable physical page allocation
//BOOL SysLoggedSetLockPagesPrivilege ( HANDLE hProcess, BOOL bEnable);

#endif

#endif /* __SYSTEM_H__ */
