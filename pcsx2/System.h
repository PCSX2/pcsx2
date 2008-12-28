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

#include "PS2Etypes.h"

#include "Exceptions.h"
#include "Paths.h"

void SysDetect();						// Detects cpu type and fills cpuInfo structs.
bool SysInit();							// Init logfiles, directories, plugins, and other OS-specific junk
void SysReset();						// Resets the various PS2 cpus, sub-systems, and recompilers.
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


static __forceinline void SysMunmap( void* base, u32 size )
{
	SysMunmap( (uptr)base, size );
}

// Console Namespace -- Replacements for SysPrintf.
// SysPrintf is depreciated -- We should phase these in over time.
namespace Console
{
	enum Colors
	{
		Color_Black = 0,
		Color_Red,
		Color_Green,
		Color_Yellow,
		Color_Blue,
		Color_Magenta,
		Color_Cyan,
		Color_White
	};

	extern void Open();
	extern void Close();
	extern void SetTitle( const char* title );

	// Changes the active console color.
	// This color will be unset by calls to colored text methods
	// such as ErrorMsg and Notice.
	extern void __fastcall SetColor( Colors color );

	// Restores the console color to default (usually low-intensity white on Win32)
	extern void ClearColor();

	// The following Write functions return bool so that we can use macros to exclude
	// them from different buildypes.  The return values are always zero.

	// Writes a newline to the console.
	extern bool __fastcall WriteLn();

	// Writes an unformatted string of text to the console (fast!)
	// No newline is appended.
	extern bool __fastcall Write( const char* fmt );

	// Writes an unformatted string of text to the console (fast!)
	// A newline is automatically appended.
	extern bool __fastcall WriteLn( const char* fmt );

	// Writes a line of colored text to the console, with automatic newline appendage.
	// The console color is reset to default when the operation is complete.
	extern bool MsgLn( Colors color, const char* fmt, ... );

	// Writes a line of colored text to the console (no newline).
	// The console color is reset to default when the operation is complete.
	extern bool Msg( Colors color, const char* fmt, ... );

	// Writes a formatted message to the console (no newline)
	extern bool Msg( const char* fmt, ... );

	// Writes a formatted message to the console, with appended newline.
	extern bool MsgLn( const char* fmt, ... );

	// Displays a message in the console with red emphasis.
	// Newline is automatically appended.
	extern bool Error( const char* fmt, ... );

	// Displays a message in the console with yellow emphasis.
	// Newline is automatically appended.
	extern bool Notice( const char* fmt, ... );
}

using Console::Color_Red;
using Console::Color_Green;
using Console::Color_Blue;
using Console::Color_Magenta;
using Console::Color_Cyan;
using Console::Color_Yellow;
using Console::Color_White;

//////////////////////////////////////////////////////////////////
// Class for allocating a resizable memory block.

class MemoryAlloc
{
public:
	static const int DefaultChunkSize = 0x1000;

protected:
	u8* m_ptr;
	int m_alloc;	// size of the allocation of memory

public: 
	int ChunkSize;

public:
	virtual ~MemoryAlloc();
	MemoryAlloc( int initalSize );
	MemoryAlloc();

	int GetSize() const { return m_alloc; }

	// Gets the pointer to the beginning of the memory allocation.
	// Returns null if no memory has been allocated to this handle.
	void *GetPtr() { return m_ptr; }
	const void *GetPtr() const { return m_ptr; }

	void MakeRoomFor( int blockSize );

};

//////////////////////////////////////////////////////////////
// Safe deallocation macros -- always check pointer validity (non-null)
// and set pointer to null on deallocation.

#define safe_delete( ptr ) \
	if( ptr != NULL ) { \
		delete ptr; \
		ptr = NULL; \
	}

#define safe_delete_array( ptr ) \
	if( ptr != NULL ) { \
		delete[] ptr; \
		ptr = NULL; \
	}

#define safe_free( ptr ) \
	if( ptr != NULL ) { \
		free( ptr ); \
		ptr = NULL; \
	}

#define safe_aligned_free( ptr ) \
	if( ptr != NULL ) { \
		_aligned_free( ptr ); \
		ptr = NULL; \
	}

#define SafeSysMunmap( ptr, size ) \
	if( ptr != NULL ) { \
		SysMunmap( (uptr)ptr, size ); \
		ptr = NULL; \
	}


//////////////////////////////////////////////////////////////
// Macros for ifdef'ing out specific lines of code.

#ifdef PCSX2_DEVBUILD

#	define DevCon Console
static const bool IsDevBuild = true;

#else

#	define DevCon 0&&Console
static const bool IsDevBuild = false;

#endif

#ifdef _DEBUG

#	define DbgCon 0&&Console
static const bool IsDebugBuild = true;

#else

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
