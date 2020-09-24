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

s32 SPU2init();
s32 SPU2reset();
s32 SPU2ps1reset();
s32 SPU2open(void *pDsp);
void SPU2close();
void SPU2shutdown();
void SPU2write(u32 mem, u16 value);
u16 SPU2read(u32 mem);

// extended funcs
// if start is 1, starts recording spu2 data, else stops
// returns a non zero value if successful
// for now, pData is not used
int SPU2setupRecording(int start, std::wstring* filename);

void SPU2setClockPtr(u32 *ptr);

void SPU2async(u32 cycles);
s32 SPU2freeze(int mode, freezeData *data);
void SPU2configure();
void SPU2about();
s32 SPU2test();

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
