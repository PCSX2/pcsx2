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

#ifndef __SYSTEM_H__
#define __SYSTEM_H__

int  SysInit();							// Init mem and plugins
void SysReset();						// Resets mem
void SysPrintf(char *fmt, ...);			// Printf used by bios syscalls
void SysMessage(char *fmt, ...);		// Message used to print msg to users
void SysUpdate();						// Called on VBlank (to update i.e. pads)
void SysRunGui();						// Returns to the Gui
void SysClose();						// Close mem and plugins
void *SysLoadLibrary(char *lib);		// Loads Library
void *SysLoadSym(void *lib, char *sym);	// Loads Symbol from Library
char *SysLibError();					// Gets previous error loading sysbols
void SysCloseLibrary(void *lib);		// Closes Library
void *SysMmap(uptr base, u32 size);
void SysMunmap(uptr base, u32 size);

#ifdef WIN32_VIRTUAL_MEM
typedef struct _PSMEMORYBLOCK
{
	ULONG_PTR NumberPages;
	ULONG_PTR* aPFNs;
	ULONG_PTR* aVFNs; // virtual pages that own the physical pages
} PSMEMORYBLOCK;

int SysPhysicalAlloc(u32 size, PSMEMORYBLOCK* pblock);
void SysPhysicalFree(PSMEMORYBLOCK* pblock);
int SysVirtualPhyAlloc(void* base, u32 size, PSMEMORYBLOCK* pblock);
void SysVirtualFree(void* lpMemReserved, u32 size);

void SysVirtualProtectAlloc(void* base, u32 size, PSMEMORYBLOCK* pblock);
void SysVirtualProtectFree(void* lpMemReserved, u32 size);

BOOL SysMapUserPhysicalPages(PVOID Addr, ULONG_PTR NumPages, PULONG_PTR PageArray);

// call to enable physical page allocation
BOOL SysLoggedSetLockPagesPrivilege ( HANDLE hProcess, BOOL bEnable);

#endif

#endif /* __SYSTEM_H__ */
