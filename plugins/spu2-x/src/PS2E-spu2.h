/* SPU2-X, A plugin for Emulating the Sound Processing Unit of the Playstation 2
 * Developed and maintained by the Pcsx2 Development Team.
 *
 * Original portions from SPU2ghz are (c) 2008 by David Quintana [gigaherz]
 *
 * SPU2-X is free software: you can redistribute it and/or modify it under the terms
 * of the GNU Lesser General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * SPU2-X is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with SPU2-X.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "Pcsx2Defs.h"
#include "PS2Edefs.h"

#ifdef __POSIX__
//Until I get around to putting in Linux svn code, this is an unknown svn version.
#define SVN_REV_UNKNOWN
#endif

#ifdef _MSC_VER
#define EXPORT_C_(type) extern "C" type CALLBACK
#else
#define EXPORT_C_(type) extern "C" __attribute__((stdcall, externally_visible, visibility("default"))) type
#endif

// We have our own versions that have the DLLExport attribute configured:

EXPORT_C_(s32)
SPU2init();
EXPORT_C_(s32)
SPU2reset();
EXPORT_C_(s32)
SPU2ps1reset();
EXPORT_C_(s32)
SPU2open(void *pDsp);
EXPORT_C_(void)
SPU2close();
EXPORT_C_(void)
SPU2shutdown();
EXPORT_C_(void)
SPU2write(u32 mem, u16 value);
EXPORT_C_(u16)
SPU2read(u32 mem);

// These defines are useless and gcc-4.6 complain about redefinition
// so we remove them on linux
#ifndef __POSIX__
EXPORT_C_(void)
SPU2readDMA4Mem(u16 *pMem, u32 size);
EXPORT_C_(void)
SPU2writeDMA4Mem(u16 *pMem, u32 size);
EXPORT_C_(void)
SPU2interruptDMA4();
EXPORT_C_(void)
SPU2readDMA7Mem(u16 *pMem, u32 size);
EXPORT_C_(void)
SPU2writeDMA7Mem(u16 *pMem, u32 size);
EXPORT_C_(void)
SPU2interruptDMA7();

// all addresses passed by dma will be pointers to the array starting at baseaddr
// This function is necessary to successfully save and reload the spu2 state
EXPORT_C_(u32)
SPU2ReadMemAddr(int core);
EXPORT_C_(void)
SPU2WriteMemAddr(int core, u32 value);
EXPORT_C_(void)
SPU2irqCallback(void (*SPU2callback)(), void (*DMA4callback)(), void (*DMA7callback)());
#endif

// extended funcs
// if start is 1, starts recording spu2 data, else stops
// returns a non zero value if successful
// for now, pData is not used
EXPORT_C_(int)
SPU2setupRecording(int start, std::wstring* filename);

EXPORT_C_(void)
SPU2setClockPtr(u32 *ptr);

EXPORT_C_(void)
SPU2async(u32 cycles);
EXPORT_C_(s32)
SPU2freeze(int mode, freezeData *data);
EXPORT_C_(void)
SPU2configure();
EXPORT_C_(void)
SPU2about();
EXPORT_C_(s32)
SPU2test();

#include "Spu2replay.h"

extern u8 callirq;

extern void (*_irqcallback)();

extern void (*dma4callback)();
extern void (*dma7callback)();

extern s16 *input_data;
extern u32 input_data_ptr;

extern double srate_pv;

extern int recording;
extern u32 lClocks;
extern u32 *cyclePtr;

extern void SPU2writeLog(const char *action, u32 rmem, u16 value);
extern void TimeUpdate(u32 cClocks);
extern void SPU2_FastWrite(u32 rmem, u16 value);

extern void LowPassFilterInit();

//#define PCM24_S1_INTERLEAVE
