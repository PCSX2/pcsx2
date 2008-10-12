//GiGaHeRz's SPU2 Driver
//Copyright (c) 2003-2008, David Quintana <gigaherz@gmail.com>
//
//This library is free software; you can redistribute it and/or
//modify it under the terms of the GNU Lesser General Public
//License as published by the Free Software Foundation; either
//version 2.1 of the License, or (at your option) any later version.
//
//This library is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//Lesser General Public License for more details.
//
//You should have received a copy of the GNU Lesser General Public
//License along with this library; if not, write to the Free Software
//Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
#ifndef SPU2_H_INCLUDED
#define SPU2_H_INCLUDED
//system defines
#ifdef __LINUX__
	#include <gtk/gtk.h>
#else
#	define WINVER 0x0501
#	define _WIN32_WINNT 0x0501
#	include <windows.h>
#	include <mmsystem.h>
#endif
#include "stdlib.h"
#include "stdio.h"
#include "stdarg.h"
#include "math.h"
#include "time.h"

//SPU2 plugin defines
//#define __MSCW32__
#define SPU2defs
#include "PS2Edefs.h"

//#define EFFECTS_DUMP

//Plugin parts
#include "config.h"
#include "defs.h"
#include "regs.h"
#include "dma.h"
#include "mixer.h"
#include "sndout.h"

#include "spu2replay.h"

#define SPU2_LOG

#include "debug.h"

extern void spdif_set51(u32 is_5_1_out);
extern u32  spdif_init();
extern void spdif_shutdown();
extern void spdif_get_samples(s32 *samples); // fills the buffer with [l,r,c,lfe,sl,sr] if using 5.1 output, or [l,r] if using stereo


extern short *spu2regs;
extern short *_spu2mem;

extern s16 __forceinline *GetMemPtr(u32 addr);

#define spu2Rs16(mmem)	(*(s16 *)((s8 *)spu2regs + ((mmem) & 0x1fff)))
#define spu2Ru16(mmem)	(*(u16 *)((s8 *)spu2regs + ((mmem) & 0x1fff)))

#define spu2Ms16(mmem)	(*GetMemPtr((mmem) & 0xfffff))
#define spu2Mu16(mmem)	(*(u16*)GetMemPtr((mmem) & 0xfffff))

void SysMessage(char *fmt, ...);

extern void VoiceStart(int core,int vc);
extern void VoiceStop(int core,int vc);

extern s32 uTicks;

extern void (* _irqcallback)();
extern void (* dma4callback)();
extern void (* dma7callback)();

extern void SetIrqCall();

extern double srate_pv;

extern s16 *input_data;
extern u32 input_data_ptr;

extern HINSTANCE hInstance;

extern int PlayMode;

extern int recording;
void RecordStart();
void RecordStop();
void RecordWrite(s16 left, s16 right);

extern CRITICAL_SECTION threadSync;

extern u32 lClocks;

extern bool EnableThread;

#define ENTER_CS(cs) do { if(EnableThread) EnterCriticalSection(cs); } while(0)
#define LEAVE_CS(cs) do { if(EnableThread) LeaveCriticalSection(cs); } while(0)

extern u32* cPtr;
extern bool hasPtr;
void CALLBACK TimeUpdate(u32 cClocks, u32 syncType);

void TimestretchUpdate(int bufferusage,int buffersize);

s32  DspLoadLibrary(char *fileName, int modNum);
void DspCloseLibrary();
int  DspProcess(s16 *buffer, int samples);
void DspUpdate(); // to let the Dsp process window messages

void SndUpdateLimitMode();
//#define PCM24_S1_INTERLEAVE

#endif // SPU2_H_INCLUDED //