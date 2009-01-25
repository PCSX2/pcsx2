/*  ZeroSPU2
 *  Copyright (C) 2006-2007 zerofrog
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

#ifndef __SPU2_H__
#define __SPU2_H__

#define _CRT_SECURE_NO_DEPRECATE

#include <stdio.h>
#include <string.h>
#include <malloc.h>

extern "C" {
#define SPU2defs
#include "PS2Edefs.h"
}

#ifdef __LINUX__
#include <unistd.h>
#include <gtk/gtk.h>
#include <sys/timeb.h>	// ftime(), struct timeb

#define Sleep(ms) usleep(1000*ms)

inline unsigned long timeGetTime()
{
#ifdef _WIN32
	_timeb t;
	_ftime(&t);
#else
	timeb t;
	ftime(&t);
#endif

	return (unsigned long)(t.time*1000+t.millitm);
}

#include <sys/time.h>

#else
#include <windows.h>
#include <windowsx.h>

#include <sys/timeb.h>	// ftime(), struct timeb
#endif


inline u64 GetMicroTime()
{
#ifdef _WIN32
	extern LARGE_INTEGER g_counterfreq;
	LARGE_INTEGER count;
	QueryPerformanceCounter(&count);
	return count.QuadPart * 1000000 / g_counterfreq.QuadPart;
#else
	timeval t;
	gettimeofday(&t, NULL);
	return t.tv_sec*1000000+t.tv_usec;
#endif
}

#include <string>
#include <vector>
using namespace std;

extern FILE *spu2Log;
#ifdef _DEBUG
#define SPU2_LOG __Log  //debug mode
#else
#define SPU2_LOG 0&&
#endif

#define  SPU2_VERSION  PS2E_SPU2_VERSION
#define SPU2_REVISION 0
#define  SPU2_BUILD	4	// increase that with each version
#define SPU2_MINOR 6

#define OPTION_TIMESTRETCH 1 // stretches samples without changing pitch to reduce cracking
#define OPTION_REALTIME 2 // sync to real time instead of ps2 time
#define OPTION_MUTE 4   // don't output anything
#define OPTION_RECORDING 8

// ADSR constants
#define ATTACK_MS	  494L
#define DECAYHALF_MS   286L
#define DECAY_MS	   572L
#define SUSTAIN_MS	 441L
#define RELEASE_MS	 437L

#define AUDIO_BUFFER 2048

#define NSSIZE	  48	  // ~ 1 ms of data
#define NSFRAMES	16	  // gather at least NSFRAMES of NSSIZE before submitting
#define NSPACKETS 24

#define SAMPLE_RATE 48000L
extern s8 *spu2regs;
extern u16* spu2mem;
extern int iFMod[NSSIZE];
extern u32 MemAddr[2];
extern unsigned long   dwNoiseVal;						  // global noise generator

// functions of main emu, called on spu irq
extern void (*irqCallbackSPU2)();
extern void (*irqCallbackDMA4)();
extern void (*irqCallbackDMA7)();

extern int SPUCycles, SPUWorkerCycles;
extern int SPUStartCycle[2];
extern int SPUTargetCycle[2];

extern u16 interrupt;

typedef struct {
	int Log;
	int options;
} Config;

extern Config conf;

void __Log(char *fmt, ...);
void SaveConfig();
void LoadConfig();
void SysMessage(char *fmt, ...);

void LogRawSound(void* pleft, int leftstride, void* pright, int rightstride, int numsamples);
void LogPacketSound(void* packet, int memsize);

// hardware sound functions
int SetupSound(); // if successful, returns 0
void RemoveSound();
int SoundGetBytesBuffered();
// returns 0 is successful, else nonzero
void SoundFeedVoiceData(unsigned char* pSound,long lBytes);

#if !defined(_MSC_VER) && !defined(HAVE_ALIGNED_MALLOC)

#include <assert.h>

// declare linux equivalents
static  __forceinline void* pcsx2_aligned_malloc(size_t size, size_t align)
{
	assert( align < 0x10000 );
	char* p = (char*)malloc(size+align);
	int off = 2+align - ((int)(uptr)(p+2) % align);

	p += off;
	*(u16*)(p-2) = off;

	return p;
}

static __forceinline void pcsx2_aligned_free(void* pmem)
{
	if( pmem != NULL ) {
		char* p = (char*)pmem;
		free(p - (int)*(u16*)(p-2));
	}
}

#define _aligned_malloc pcsx2_aligned_malloc
#define _aligned_free pcsx2_aligned_free
#endif

// Atomic Operations
#if defined (_WIN32)

#ifndef __x86_64__
extern "C" LONG  __cdecl _InterlockedExchangeAdd(LPLONG volatile Addend, LONG Value);
#endif

#pragma intrinsic (_InterlockedExchangeAdd)
#define InterlockedExchangeAdd _InterlockedExchangeAdd

#else

typedef void* PVOID;

static __forceinline long InterlockedExchangeAdd(long volatile* Addend, long Value)
{
	__asm__ __volatile__(".intel_syntax\n"
						 "lock xadd [%0], %%eax\n"
						 ".att_syntax\n" : : "r"(Addend), "a"(Value) : "memory" );
}

#endif

////////////////////
// SPU2 Registers //
////////////////////
enum
{
// Volume Registers - currently not implemented in ZeroSPU2, like most volume registers.
 REG_VP_VOLL     			 = 0x0000, // Voice Volume Left
 REG_VP_VOLR    			 = 0x0002, // Voice Volume Right
 REG_VP_PITCH    			 = 0x0004, // Pitch
 REG_VP_ADSR1  			 = 0x0006, // Envelope 1 (Attack-Decay-Sustain-Release)
 REG_VP_ADSR2   			 = 0x0008, // Envelope 2 (Attack-Decay-Sustain-Release)
 REG_VP_ENVX     			 = 0x000A, // Current Envelope
 REG_VP_VOLXL    			 = 0x000C, // Current Voice Volume Left
 REG_VP_VOLXR    			 = 0x000E, // Current Voice Volume Right
// end unimplemented section
	
 REG_C0_FMOD1    			 = 0x0180, // Pitch Modulation Spec.
 REG_C0_FMOD2    			 = 0x0182,
 REG_S_NON        			 = 0x0184, // Alloc Noise Generator - unimplemented
 REG_C0_VMIXL1   			 = 0x0188, // Voice Output Mix Left  (Dry)
 REG_C0_VMIXL2    			 = 0x018A,
 REG_S_VMIXEL   			 = 0x018C, // Voice Output Mix Left  (Wet) - unimplemented
 REG_C0_VMIXR1   			 = 0x0190, // Voice Output Mix Right (Dry)
 REG_C0_VMIXR2  			 = 0x0192,
 REG_S_VMIXER   			 = 0x0194, // Voice Output Mix Right (Wet) - unimplemented

 REG_C0_MMIX     			 = 0x0198, // Output Spec. After Voice Mix
 REG_C0_CTRL      			 = 0x019A, // Core X Attrib
 REG_C0_IRQA_HI  			 = 0x019C, // Interrupt Address Spec. - Hi
 REG_C0_IRQA_LO 			 = 0x019E, // Interrupt Address Spec. - Lo

 REG_C0_SPUON1 			 = 0x01A0, // Key On 0/1
 REG_C0_SPUON2  			 = 0x01A2,
 REG_C0_SPUOFF1  			 = 0x01A4, // Key Off 0/1
 REG_C0_SPUOFF2  			 = 0x01A6,

 REG_C0_SPUADDR_HI 		 = 0x01A8, // Transfer starting address - hi
 REG_C0_SPUADDR_LO 		 = 0x01AA, // Transfer starting address - lo
 REG_C0_SPUDATA   		 = 0x01AC, // Transfer data
 REG_C0_DMACTRL  			 = 0x01AE, // unimplemented
 REG_C0_ADMAS     			 = 0x01B0, // AutoDMA Status

 // Section Unimplemented
 REG_VA_SSA      			 = 0x01C0, // Waveform data starting address
 REG_VA_LSAX    			 = 0x01C4, // Loop point address
 REG_VA_NAX      			 = 0x01C8, // Waveform data that should be read next
 REG_A_ESA         			 = 0x02E0, //Address: Top address of working area for effects processing
 R_FB_SRC_A       		  	 = 0x02E4, // Feedback Source A
 R_FB_SRC_B       			 = 0x02E8, // Feedback Source B
R_IIR_DEST_A0       			 = 0x02EC,
R_IIR_DEST_A1       			 = 0x02F0,
R_ACC_SRC_A0       			 = 0x02F4,
R_ACC_SRC_A1       			 = 0x02F8,
R_ACC_SRC_B0       			 = 0x02FC,
R_ACC_SRC_B1       			 = 0x0300,
R_IIR_SRC_A0       			 = 0x0304,
R_IIR_SRC_A1       			 = 0x0308,
R_IIR_DEST_B0       			 = 0x030C,
R_IIR_DEST_B1       			 = 0x0310,
R_ACC_SRC_C0       			 = 0x0314,
R_ACC_SRC_C1       			 = 0x0318,
R_ACC_SRC_D0       			 = 0x031C,
R_ACC_SRC_D1       			 = 0x0320,
R_IIR_SRC_B1       			 = 0x0324,
R_IIR_SRC_B0       			 = 0x0328,
R_MIX_DEST_A0       			 = 0x032C,
R_MIX_DEST_A1       			 = 0x0330,
R_MIX_DEST_B0       			 = 0x0334,
R_MIX_DEST_B1       			 = 0x0338,
 REG_A_EEA         			 = 0x033C, // Address: End address of working area for effects processing (upper part of address only!)
 // end unimplemented section
 
 REG_C0_END1     			 = 0x0340, // End Point passed flag
 REG_C0_END2      			 = 0x0342,
 REG_C0_SPUSTAT 			 = 0x0344, // Status register?
 
 // core 1 has the same registers with 0x400 added, and ends at 0x746.
 REG_C1_FMOD1   			 = 0x0580,
 REG_C1_FMOD2   			 = 0x0582,
 REG_C1_VMIXL1  			 = 0x0588,
 REG_C1_VMIXL2  			 = 0x058A,
 REG_C1_VMIXR1   			 = 0x0590,
 REG_C1_VMIXR2 			 = 0x0592,
 REG_C1_MMIX    			 = 0x0598,
 REG_C1_CTRL      			 = 0x059A,
 REG_C1_IRQA_HI  			 = 0x059C,
 REG_C1_IRQA_LO 			 = 0x059E,
 REG_C1_SPUON1  			 = 0x05A0,
 REG_C1_SPUON2  			 = 0x05A2,
 REG_C1_SPUOFF1 			 = 0x05A4,
 REG_C1_SPUOFF2 			 = 0x05A6,
 REG_C1_SPUADDR_HI 		 = 0x05A8,
 REG_C1_SPUADDR_LO		 = 0x05AA,
 REG_C1_SPUDATA  			 = 0x05AC,
 REG_C1_DMACTRL  			 = 0x05AE, // unimplemented
 REG_C1_ADMAS    			 = 0x05B0,
 REG_C1_END1      			 = 0x0740,
 REG_C1_END2     			 = 0x0742,
 REG_C1_SPUSTAT  			 = 0x0744,
 
 // Interesting to note that *most* of the volume controls aren't implemented in Zerospu2.
 REG_P_MVOLL     			 = 0x0760, // Master Volume Left - unimplemented
 REG_P_MVOLR    			 = 0x0762, // Master Volume Right - unimplemented
 REG_P_EVOLL     			 = 0x0764, // Effect Volume Left - unimplemented
 REG_P_EVOLR     			 = 0x0766, // Effect Volume Right - unimplemented
 REG_P_AVOLL      			 = 0x0768, // Core External Input Volume Left  (Only Core 1) - unimplemented
 REG_P_AVOLR     			 = 0x076A, // Core External Input Volume Right (Only Core 1) - unimplemented
 REG_C0_BVOLL     			 = 0x076C, // Sound Data Volume Left
 REG_C0_BVOLR     			 = 0x076E, // Sound Data Volume Right
 REG_P_MVOLXL   			 = 0x0770, // Current Master Volume Left - unimplemented
 REG_P_MVOLXR   			 = 0x0772, // Current Master Volume Right - unimplemented
 
 // Another unimplemented section
 R_IIR_ALPHA   				 = 0x0774, // IIR alpha (% used)
 R_ACC_COEF_A   			 = 0x0776,
 R_ACC_COEF_B   			 = 0x0778,
 R_ACC_COEF_C   			 = 0x077A,
 R_ACC_COEF_D   			 = 0x077C,
 R_IIR_COEF   				 = 0x077E,
 R_FB_ALPHA   				 = 0x0780, // feedback alpha (% used)
 R_FB_X   					 = 0x0782, // feedback 
 R_IN_COEF_L   				 = 0x0784,
 R_IN_COEF_R   			 = 0x0786,
  // end unimplemented section
  
 REG_C1_BVOLL    			 = 0x0794,
 REG_C1_BVOLR   			 = 0x0796,
 
 SPDIF_OUT          			 = 0x07C0, // SPDIF Out: OFF/'PCM'/Bitstream/Bypass - unimplemented
 REG_IRQINFO     			 = 0x07C2, 
 SPDIF_MODE      			 = 0x07C6, // unimplemented
 SPDIF_MEDIA     			 = 0x07C8, // SPDIF Media: 'CD'/DVD - unimplemented
 SPDIF_COPY_PROT     		 = 0x07CC  // SPDIF Copy Protection - unimplemented 
 // NOTE: SPDIF_COPY is defined in Linux kernel headers as 0x0004.
};			

// These SPDIF defines aren't used yet - swiped from spu2ghz, like a number of the registers I added in.
// -- arcum42
#define SPDIF_OUT_OFF        0x0000		//no spdif output
#define SPDIF_OUT_PCM        0x0020		//encode spdif from spu2 pcm output
#define SPDIF_OUT_BYPASS     0x0100		//bypass spu2 processing

#define SPDIF_MODE_BYPASS_BITSTREAM 0x0002	//bypass mode for digital bitstream data
#define SPDIF_MODE_BYPASS_PCM       0x0000	//bypass mode for pcm data (using analog output)

#define SPDIF_MODE_MEDIA_CD  0x0800		//source media is a CD
#define SPDIF_MODE_MEDIA_DVD 0x0000		//source media is a DVD

#define SPDIF_MEDIA_CDVD     0x0200
#define SPDIF_MEDIA_400      0x0000

#define SPDIF_COPY_NORMAL      0x0000	// spdif stream is not protected
#define SPDIF_COPY_PROHIBIT    0x8000	// spdif stream can't be copied

#define SPU_AUTODMA_ONESHOT 0  //spu2
#define SPU_AUTODMA_LOOP 1   //spu2
#define SPU_AUTODMA_START_ADDR (1 << 1)   //spu2

#define spu2Rs16(mem)	(*(s16*)&spu2regs[(mem) & 0xffff])
#define spu2Ru16(mem)	(*(u16*)&spu2regs[(mem) & 0xffff])

#define IRQINFO spu2Ru16(REG_IRQINFO)

static __forceinline u32 SPU2_GET32BIT(u32 lo, u32 hi)
{
	return (((u32)(spu2Ru16(hi) & 0x3f) << 16) | (u32)spu2Ru16(lo));
}

static __forceinline void SPU2_SET32BIT(u32 value, u32 lo, u32 hi)
{
	spu2Ru16(hi) = ((value) >> 16) & 0x3f;
	spu2Ru16(lo) = (value) & 0xffff;
}

static __forceinline u32 C0_IRQA()
{
	return SPU2_GET32BIT(REG_C0_IRQA_LO, REG_C0_IRQA_HI);
}

static __forceinline u32 C1_IRQA()
{
	return SPU2_GET32BIT(REG_C1_IRQA_LO, REG_C1_IRQA_HI);
}

static __forceinline u32 C0_SPUADDR()
{
	return SPU2_GET32BIT(REG_C0_SPUADDR_LO, REG_C0_SPUADDR_HI);
}

static __forceinline u32 C1_SPUADDR()
{
	return SPU2_GET32BIT(REG_C1_SPUADDR_LO, REG_C1_SPUADDR_HI);
}

static __forceinline void C0_SPUADDR_SET(u32 value)
{
	SPU2_SET32BIT(value, REG_C0_SPUADDR_LO, REG_C0_SPUADDR_HI);
}

static __forceinline void C1_SPUADDR_SET(u32 value)
{
	SPU2_SET32BIT(value, REG_C1_SPUADDR_LO, REG_C1_SPUADDR_HI);
}

#define SPU_NUMBER_VOICES	   48

struct SPU_CONTROL_
{
	u16 extCd : 1;
	u16 extAudio : 1;
	u16 cdreverb : 1; 
	u16 extr : 1;	  // external reverb
	u16 dma : 2;	   // 1 - no dma, 2 - write, 3 - read
	u16 irq : 1;
	u16 reverb : 1;
	u16 noiseFreq : 6;
	u16 spuUnmute : 1;
	u16 spuon : 1;
};

#if defined(_MSC_VER)
#pragma pack(1)
#endif
// the layout of each voice in wSpuRegs
struct _SPU_VOICE
{
	union
	{
		struct {
			u16 Vol : 14;
			u16 Inverted : 1;
			u16 Sweep0 : 1;
		} vol;
		struct {
			u16 Vol : 7;
			u16 res1 : 5;
			u16 Inverted : 1;
			u16 Decrease : 1;  // if 0, increase
			u16 ExpSlope : 1;  // if 0, linear slope
			u16 Sweep1 : 1;	// always one
		} sweep;
		u16 word;
	} left, right;

	u16 pitch : 14;		// 1000 - no pitch, 2000 - pitch + 1, etc
	u16 res0 : 2;

	u16 SustainLvl : 4;
	u16 DecayRate : 4;
	u16 AttackRate : 7; 
	u16 AttackExp : 1;	 // if 0, linear
	
	u16 ReleaseRate : 5;
	u16 ReleaseExp : 1;	// if 0, linear
	u16 SustainRate : 7;
	u16 res1 : 1;
	u16 SustainDec : 1;	// if 0, inc
	u16 SustainExp : 1;	// if 0, linear
	
	u16 AdsrVol;
	u16 Address;		   // add / 8
	u16 RepeatAddr;		// gets reset when sample starts
#if defined(_MSC_VER)
};						//+22
#else
} __attribute__((packed));
#endif

// ADSR INFOS PER   CHANNEL
struct ADSRInfoEx
{
	int			State;
	int			AttackModeExp;
	int			AttackRate;
	int			DecayRate;
	int			SustainLevel;
	int			SustainModeExp;
	int			SustainIncrease;
	int			SustainRate;
	int			ReleaseModeExp;
	int			ReleaseRate;
	int			EnvelopeVol;
	long		   lVolume;
};

#define SPU_VOICE_STATE_SIZE (sizeof(VOICE_PROCESSED)-4*sizeof(void*))

struct VOICE_PROCESSED
{
	VOICE_PROCESSED()
	{
		memset(this, 0, sizeof(VOICE_PROCESSED));
	}

	void SetVolume(int right);
	void StartSound();
	void VoiceChangeFrequency();
	void InterpolateUp();
	void InterpolateDown();
	void FModChangeFrequency(int ns);
	int iGetNoiseVal();
	void StoreInterpolationVal(int fa);
	int iGetInterpolationVal();
	void Stop();

	SPU_CONTROL_* GetCtrl();

	// start save state
	int leftvol, rightvol;	 // left right volumes

	int iSBPos;							 // mixing stuff
	int SB[32+32];
	int spos;
	int sinc;

	int iIrqDone;						   // debug irq done flag
	int s_1;								// last decoding infos
	int s_2;
	int iOldNoise;						  // old noise val for this channel   
	int			   iActFreq;						   // current psx pitch
	int			   iUsedFreq;						  // current pc pitch

	int iStartAddr, iLoopAddr, iNextAddr;
	int bFMod;

	ADSRInfoEx ADSRX;							  // next ADSR settings (will be moved to active on sample start)
	int memoffset;				  // if first core, 0, if second, 0x400
	int chanid; // channel id

	bool bIgnoreLoop, bNew, bNoise, bReverb, bOn, bStop, bVolChanged;
	bool bVolumeR, bVolumeL;
	
	// end save state

	///////////////////
	// Sound Buffers //
	///////////////////
	u8* pStart;		   // start and end addresses
	u8* pLoop, *pCurr;

	_SPU_VOICE* pvoice;
};

struct ADMA
{
	unsigned short * MemAddr;
	int			  Index;
	int			  AmountLeft;
	int			  Enabled; // used to make sure that ADMA doesn't get interrupted with a writeDMA call
};


extern ADMA Adma4;
extern ADMA Adma7;

#endif /* __SPU2_H__ */
