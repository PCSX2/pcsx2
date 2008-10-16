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

#ifndef DEFS_H_INCLUDED
#define DEFS_H_INCLUDED

typedef enum {SPU2_VOL_MODE_CONST,SPU2_VOL_MODE_PLIN,SPU2_VOL_MODE_NLIN,SPU2_VOL_MODE_PLOG,SPU2_VOL_MODE_NLOG} V_VolMode;

typedef struct {
	u16 Reg_VOL;
	s16 Value; //also Reg_VOLX
	s8 Increment;
	s8 Mode;
} V_Volume;

typedef struct {
	u16 Reg_ADSR1;
	u16 Reg_ADSR2;
//also Reg_ENVX
	u32 Value; 
// Phase
	u8 Phase;
//Attack Rate
	u8 Ar; 
//Attack Mode
	u8 Am; 
//Decay Rate
	u8 Dr; 
//Sustain Level
	u8 Sl; 
//Sustain Rate
	u8 Sr; 
//Sustain Mode
	u8 Sm; 
//Release Rate
	u8 Rr; 
//Release Mode
	u8 Rm;
//Ready To Release
	u8 Releasing;
} V_ADSR;


typedef struct {
// SPU2 cycle where the Playing started
	u32 PlayCycle;
// Left Volume
	V_Volume VolumeL;
// Right Volume
	V_Volume VolumeR;
// Envelope
	V_ADSR ADSR;
// Pitch (also Reg_PITCH)
	s16 Pitch; 
// Pitch Modulated by previous voice
	s8 Modulated;
// Source (Wave/Noise)
	s8 Noise;
// Direct Output for Left Channel
	s8 DryL;
// Direct Output for Right Channel
	s8 DryR;
// Effect Output for Left Channel
	s8 WetL;
// Effect Output for Right Channel
	s8 WetR;
// Loop Start Adress (also Reg_LSAH/L)
	u32 LoopStartA; 
// Sound Start Adress (also Reg_SSAH/L)
	u32 StartA;
// Next Read Data Adress (also Reg_NAXH/L)
	u32 NextA;
// Voice Decoding State
	s32 Prev1;
	s32 Prev2;

	s8 LoopMode;
	s8 LoopStart;
	s8 Loop;
	s8 LoopEnd;

	s32 SP;

	s32 PV1;
	s32 PV2;
	s32 PV3;
	s32 PV4;

	s32 OutX;

	s8 FirstBlock;

	s32 PeakX;
	s32 SampleData;

	// [Air]: Changed SBuffer from 32-bit to 16-bit. (this breaks old savestates)
	//   Everything stored in SBuffer is 16-bit values, and on modern CPUs the benefit
	//   of reduced data cache clutter out-weighs the benefit of using 'cpu native' 32-bit
	//   values. (doesn't apply to SIMD of course, but no SIMD here anyway)
	//   Because this breaks savestates it might not be worth the bother though.
	s16 SBuffer[32];
	s32 SCurrent;

	s32 displayPeak;

	s32 lastSetStartA;

	s32 lastStopReason;
} V_Voice;

typedef struct {
	u16 IN_COEF_L;
	u16 IN_COEF_R;
	u32 FB_SRC_A;
	u32 FB_SRC_B;
	u16 FB_ALPHA;
	u16 FB_X;
	u32 IIR_SRC_A0;
	u32 IIR_SRC_A1;
	u32 IIR_SRC_B1;
	u32 IIR_SRC_B0;
	u32 IIR_DEST_A0;
	u32 IIR_DEST_A1;
	u32 IIR_DEST_B0;
	u32 IIR_DEST_B1;
	u16 IIR_ALPHA;
	u16 IIR_COEF;
	u32 ACC_SRC_A0;
	u32 ACC_SRC_A1;
	u32 ACC_SRC_B0;
	u32 ACC_SRC_B1;
	u32 ACC_SRC_C0;
	u32 ACC_SRC_C1;
	u32 ACC_SRC_D0;
	u32 ACC_SRC_D1;
	u16 ACC_COEF_A;
	u16 ACC_COEF_B;
	u16 ACC_COEF_C;
	u16 ACC_COEF_D;
	u32 MIX_DEST_A0;
	u32 MIX_DEST_A1;
	u32 MIX_DEST_B0;
	u32 MIX_DEST_B1;
} V_Reverb;

typedef struct {
	u16 Out;
	u16 Info;
	u16 Unknown1;
	u16 Mode;
	u16 Media;
	u16 Unknown2;
	u16 Protection;
} V_SPDIF;

typedef struct {
	u32 PMON;
	u32 NON;
	u32 VMIXL;
	u32 VMIXR;
	u32 VMIXEL;
	u32 VMIXER;
	u16 MMIX;
	u32 ENDX;
	u16 STATX;
	u16 ATTR;
	u16 _1AC;
} V_CoreRegs;

typedef struct {
// Core Voices
	V_Voice Voices[24];
// Master Volume for Left Channel
	V_Volume MasterL;
// Master Volume for Right Channel
	V_Volume MasterR;
// Volume for External Data Input (Left Channel)
	u16 ExtL;
// Volume for External Data Input (Right Channel)
	u16 ExtR;
// Volume for Sound Data Input (Left Channel)
	u16 InpL;
// Volume for Sound Data Input (Right Channel)
	u16 InpR;
// Volume for Output from Effects (Left Channel)
	u16 FxL;
// Volume for Output from Effects (Right Channel)
	u16 FxR;
// Interrupt Address
	u32 IRQA;
// DMA Transfer Start Address
	u32 TSA;  
// DMA Transfer Data Address (Internal...)
	u32 TDA;  
// External Input to Direct Output (Left)
	s8 ExtDryL;
// External Input to Direct Output (Right)
	s8 ExtDryR;
// External Input to Effects (Left)
	s8 ExtWetL;
// External Input to Effects (Right)
	s8 ExtWetR;
// Sound Data Input to Direct Output (Left)
	s8 InpDryL;
// Sound Data Input to Direct Output (Right)
	s8 InpDryR;
// Sound Data Input to Effects (Left)
	s8 InpWetL;
// Sound Data Input to Effects (Right)
	s8 InpWetR;
// Voice Data to Direct Output (Left)
	s8 SndDryL;
// Voice Data to Direct Output (Right)
	s8 SndDryR;
// Voice Data to Effects (Left)
	s8 SndWetL;
// Voice Data to Effects (Right)
	s8 SndWetR;
// Interrupt Enable
	s8 IRQEnable;
// DMA related?
	s8 DMABits;
// Effect Enable
	s8 FxEnable;
// Noise Clock
	s8 NoiseClk;
// AutoDMA Status
	u16 AutoDMACtrl;
// DMA Interrupt Counter
	s32 DMAICounter;
// Mute
	s8 Mute;
// Input Buffer
	u32 InputDataLeft;
	u32 InputPos;
	u32 InputDataProgress;
	u8 AdmaInProgress;

// Reverb
	V_Reverb Revb;
	u32 EffectsStartA;
	u32 EffectsEndA;
	u32 ReverbX;
// Last Transfer Size
	u32 lastsize;
// Registers
	V_CoreRegs Regs;

	u8 InitDelay;

	u8 CoreEnabled;

	u8 AttrBit0;
	u8 AttrBit4;
	u8 AttrBit5;

	u16*DMAPtr;
	u32 MADR;
	u32 TADR;

	s16 ADMATempBuffer[0x1000];

	u32 ADMAPV;
	u32 ADMAPL;
	u32 ADMAPR;

	s32 AutoDMAPeak;
} V_Core;

extern V_Core Cores[2];
extern V_SPDIF Spdif;

// Output Buffer Writing Position (the same for all data);
extern s16 OutPos;
// Input Buffer Reading Position (the same for all data);
extern s16 InputPos;
// SPU Mixing Cycles ("Ticks mixed" counter)
extern u32 Cycles;
extern u8 InpBuff;
// 1b0 "hack"
extern u32 Num;


#endif // DEFS_H_INCLUDED //
