/*  USBlinuz
 *  Copyright (C) 2002-2004  USBlinuz Team
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

#pragma once

#include <cstdio>
#include <cstring>
#include <string>
#include <limits.h>

#include "SaveState.h"
#include "osdebugout.h"

// ---------------------------------------------------------------------
#define USBdefs

extern u8 *ram;

// ---------------------------------------------------------------------

void USBconfigure();

void DestroyDevices();
void CreateDevices();

s32 USBinit();
void USBasync(u32 cycles);
void USBshutdown();
void USBclose();
s32 USBopen(void *pDsp);
s32 USBfreeze(int mode, freezeData *data);

u8 USBread8(u32 addr);
u16 USBread16(u32 addr);
u32 USBread32(u32 addr);
void USBwrite8(u32 addr,  u8 value);
void USBwrite16(u32 addr, u16 value);
void USBwrite32(u32 addr, u32 value);


void USBsetRAM(void *mem);

extern FILE *usbLog;
s64 get_clock();

/* usb-pad-raw.cpp */
#if _WIN32
extern HWND gsWnd;
# if defined(BUILD_RAW)
extern HWND msgWindow;
int InitWindow(HWND);
void UninitWindow();
# endif
#endif

