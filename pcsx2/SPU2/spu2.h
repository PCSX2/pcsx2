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

#include "Pcsx2Defs.h"
#include "Utilities/Threading.h"
#include "SaveState.h"

extern Threading::MutexRecursive mtx_SPU2Status;

s32 SPU2init();
s32 SPU2reset();
s32 SPU2ps1reset();
s32 SPU2open(void* pDsp);
void SPU2close();
void SPU2shutdown();
void SPU2write(u32 mem, u16 value);
u16 SPU2read(u32 mem);

// extended funcs
// returns true if successful
bool SPU2setupRecording(const std::string* filename);
void SPU2endRecording();

void SPU2setClockPtr(u32* ptr);

void SPU2async(u32 cycles);
s32 SPU2freeze(int mode, freezeData* data);
void SPU2DoFreezeIn(pxInputStream& infp);
void SPU2DoFreezeOut(void* dest);
void SPU2configure();


u32 SPU2ReadMemAddr(int core);
void SPU2WriteMemAddr(int core, u32 value);
void SPU2setDMABaseAddr(uptr baseaddr);
void SPU2setSettingsDir(const char* dir);
void SPU2setLogDir(const char* dir);
void SPU2readDMA4Mem(u16* pMem, u32 size);
void SPU2writeDMA4Mem(u16* pMem, u32 size);
void SPU2interruptDMA4();
void SPU2interruptDMA7();
void SPU2readDMA7Mem(u16* pMem, u32 size);
void SPU2writeDMA7Mem(u16* pMem, u32 size);

extern u8 callirq;

extern u32 lClocks;
extern u32* cyclePtr;

extern void SPU2writeLog(const char* action, u32 rmem, u16 value);
extern void TimeUpdate(u32 cClocks);
extern void SPU2_FastWrite(u32 rmem, u16 value);

extern void LowPassFilterInit();

//#define PCM24_S1_INTERLEAVE
