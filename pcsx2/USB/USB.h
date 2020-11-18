/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdio>
#include <cstring>
#include <string>
#include <limits.h>

#include "SaveState.h"

// ---------------------------------------------------------------------
#define USBdefs

extern u8* ram;

// ---------------------------------------------------------------------

void USBconfigure();

void DestroyDevices();
void CreateDevices();

s32 USBinit();
void USBasync(u32 cycles);
void USBshutdown();
void USBclose();
s32 USBopen(void* pDsp);
s32 USBfreeze(int mode, freezeData* data);

u8 USBread8(u32 addr);
u16 USBread16(u32 addr);
u32 USBread32(u32 addr);
void USBwrite8(u32 addr, u8 value);
void USBwrite16(u32 addr, u16 value);
void USBwrite32(u32 addr, u32 value);

void USBDoFreezeOut(void* dest);
void USBDoFreezeIn(pxInputStream& infp);


void USBsetRAM(void* mem);

extern FILE* usbLog;
s64 get_clock();

/* usb-pad-raw.cpp */
#if _WIN32
extern HWND gsWnd;
#endif
