/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2020  PCSX2 Dev Team
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

#include "PrecompiledHeader.h"

#include "USB.h"

u8* ram = nullptr;

void USBconfigure() {}

void DestroyDevices() {}
void CreateDevices() {}

s32 USBinit() { return 0; }
void USBasync(u32 cycles) {}
void USBshutdown() {}
void USBclose() {}
s32 USBopen(const WindowInfo& wi) { return 0; }
s32 USBfreeze(FreezeAction mode, freezeData* data) { return 0; }

u8 USBread8(u32 addr) { return 0; }
u16 USBread16(u32 addr) { return 0; }
u32 USBread32(u32 addr) { return 0; }
void USBwrite8(u32 addr, u8 value) {}
void USBwrite16(u32 addr, u16 value) {}
void USBwrite32(u32 addr, u32 value) {}

void USBsetRAM(void* mem) { ram = static_cast<u8*>(mem); }

FILE* usbLog = nullptr;
s64 get_clock() { return 0; };
