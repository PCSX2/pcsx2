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

// ======================================================================================
//  spu2sys.cpp -- Emulation module for the SPU2 'virtual machine'
// ======================================================================================
// This module contains (most!) stuff which is directly related to SPU2 emulation.
// Contents should be cross-platform compatible whenever possible.


#include "PrecompiledHeader.h"
#include "Global.h"
#include "Dma.h"
#include "IopDma.h"
#include "IopCounters.h"
#include "R3000A.h"
#include "IopHw.h"

#include "spu2.h" // needed until I figure out a nice solution for irqcallback dependencies.

s16* spu2regs = nullptr;
s16* _spu2mem = nullptr;

V_CoreDebug DebugCores[2];
V_Core Cores[2];
V_SPDIF Spdif;

u16 OutPos;
u16 InputPos;
u32 Cycles;

int PlayMode;

bool has_to_call_irq[2] = { false, false };
bool has_to_call_irq_dma[2] = { false, false };

bool psxmode = false;

void SetIrqCall(int core)
{
	// reset by an irq disable/enable cycle, behaviour found by
	// test programs that bizarrely only fired one interrupt
	has_to_call_irq[core] = true;
}

void SetIrqCallDMA(int core)
{
	// reset by an irq disable/enable cycle, behaviour found by
	// test programs that bizarrely only fired one interrupt
	has_to_call_irq_dma[core] = true;
}

__forceinline s16* GetMemPtr(u32 addr)
{
#ifndef DEBUG_FAST
	// In case you're wondering, this assert is the reason SPU2
	// runs so incrediously slow in Debug mode. :P
	pxAssume(addr < 0x100000);
#endif
	return (_spu2mem + addr);
}

__forceinline s16 spu2M_Read(u32 addr)
{
	return *GetMemPtr(addr & 0xfffff);
}

// writes a signed value to the SPU2 ram
// Invalidates the ADPCM cache in the process.
__forceinline void spu2M_Write(u32 addr, s16 value)
{
	// Make sure the cache is invalidated:
	// (note to self : addr address WORDs, not bytes)

	addr &= 0xfffff;
	if (addr >= SPU2_DYN_MEMLINE)
	{
		const int cacheIdx = addr / pcm_WordsPerBlock;
		pcm_cache_data[cacheIdx].Validated = false;

		if (MsgToConsole() && MsgCache())
			ConLog("* SPU2: PcmCache Block Clear at 0x%x (cacheIdx=0x%x)\n", addr, cacheIdx);
	}
	*GetMemPtr(addr) = value;
}

// writes an unsigned value to the SPU2 ram
__forceinline void spu2M_Write(u32 addr, u16 value)
{
	spu2M_Write(addr, (s16)value);
}

V_VolumeLR V_VolumeLR::Max(0x7FFFFFFF);
V_VolumeSlideLR V_VolumeSlideLR::Max(0x3FFF, 0x7FFFFFFF);

V_Core::V_Core(int coreidx)
	: Index(coreidx)
//LogFile_AutoDMA( nullptr )
{
	/*char fname[128];
	sprintf( fname, "logs/adma%d.raw", GetDmaIndex() );
	LogFile_AutoDMA = fopen( fname, "wb" );*/
}

V_Core::~V_Core() throw()
{
	// Can't use this yet because we dumb V_Core into savestates >_<
	/*if( LogFile_AutoDMA != nullptr )
	{
		fclose( LogFile_AutoDMA );
		LogFile_AutoDMA = nullptr;
	}*/
}

void V_Core::Init(int index)
{
	ConLog("* SPU2: Init SPU2 core %d \n", index);
	//memset(this, 0, sizeof(V_Core));
	// Explicitly initializing variables instead.
	Mute = false;
	DMABits = 0;
	NoiseClk = 0;
	NoiseCnt = 0;
	NoiseOut = 0;
	AutoDMACtrl = 0;
	InputDataLeft = 0;
	InputPosWrite = 0x100;
	InputDataProgress = 0;
	InputDataTransferred = 0;
	ReverbX = 0;
	LastEffect.Left = 0;
	LastEffect.Right = 0;
	CoreEnabled = 0;
	AttrBit0 = 0;
	DmaMode = 0;
	DMAPtr = nullptr;
	KeyOn = 0;
	OutPos = 0;

	psxmode = false;
	psxSoundDataTransferControl = 0;
	psxSPUSTAT = 0;

	const int c = Index = index;

	Regs.STATX = 0;
	Regs.ATTR = 0;
	ExtVol = V_VolumeLR::Max;
	InpVol = V_VolumeLR::Max;
	FxVol = V_VolumeLR(0);

	MasterVol = V_VolumeSlideLR(0, 0);

	memset(&DryGate, -1, sizeof(DryGate));
	memset(&WetGate, -1, sizeof(WetGate));
	DryGate.ExtL = 0;
	DryGate.ExtR = 0;
	if (!c)
	{
		WetGate.ExtL = 0;
		WetGate.ExtR = 0;
	}

	Regs.MMIX = c ? 0xFFC : 0xFF0; // PS2 confirmed (f3c and f30 after BIOS ran, ffc and ff0 after sdinit)
	Regs.VMIXL = 0xFFFFFF;
	Regs.VMIXR = 0xFFFFFF;
	Regs.VMIXEL = 0xFFFFFF;
	Regs.VMIXER = 0xFFFFFF;
	EffectsStartA = c ? 0xFFFF8 : 0xEFFF8;
	EffectsEndA = c ? 0xFFFFF : 0xEFFFF;
	ExtEffectsStartA = EffectsStartA;
	ExtEffectsEndA = EffectsEndA;

	FxEnable = false; // Uninitialized it's 0 for both cores. Resetting libs however may set this to 0 or 1.
	// These are real PS2 values, mainly constant apart from a few bits: 0x3220EAA4, 0x40505E9C.
	// These values mean nothing.  They do not reflect the actual address the SPU2 is testing,
	// it would seem that reading the IRQA register returns the last written value, not the
	// value of the internal register.  Rewriting the registers with their current values changes
	// whether interrupts fire (they do while uninitialised, but do not when rewritten).
	// The exact boot value is unknown and probably unknowable, but it seems to be somewhere
	// in the input or output areas, so we're using 0x800.
	// F1 2005 is known to rely on an uninitialised IRQA being an address which will be hit.
	IRQA = 0x800;
	IRQEnable = false; // PS2 confirmed

	for (uint v = 0; v < NumVoices; ++v)
	{
		VoiceGates[v].DryL = -1;
		VoiceGates[v].DryR = -1;
		VoiceGates[v].WetL = -1;
		VoiceGates[v].WetR = -1;

		Voices[v].Volume = V_VolumeSlideLR(0, 0); // V_VolumeSlideLR::Max;
		Voices[v].SCurrent = 28;

		Voices[v].ADSR.Value = 0;
		Voices[v].ADSR.Phase = 0;
		Voices[v].Pitch = 0x3FFF;
		Voices[v].NextA = 0x2801;
		Voices[v].StartA = 0x2800;
		Voices[v].LoopStartA = 0x2800;
	}

	DMAICounter = 0;
	AdmaInProgress = false;

	Regs.STATX = 0x80;
	Regs.ENDX = 0xffffff; // PS2 confirmed

	RevBuffers.NeedsUpdated = true;
	RevbSampleBufPos = 0;
	memset(RevbDownBuf, 0, sizeof(RevbDownBuf));
	memset(RevbUpBuf, 0, sizeof(RevbUpBuf));

	UpdateEffectsBufferSize();
}

void V_Core::AnalyzeReverbPreset()
{
	ConLog("Reverb Parameter Update for Core %d:\n", Index);
	ConLog("----------------------------------------------------------\n");

	ConLog("    IN_COEF_L, IN_COEF_R        0x%08x, 0x%08x\n", Revb.IN_COEF_L, Revb.IN_COEF_R);
	ConLog("    APF1_SIZE, APF2_SIZE          0x%08x, 0x%08x\n", Revb.APF1_SIZE, Revb.APF2_SIZE);
	ConLog("    APF1_VOL, APF2_VOL              0x%08x, 0x%08x\n", Revb.APF1_VOL, Revb.APF2_VOL);

	ConLog("    COMB1_VOL                  0x%08x\n", Revb.COMB1_VOL);
	ConLog("    COMB2_VOL                  0x%08x\n", Revb.COMB2_VOL);
	ConLog("    COMB3_VOL                  0x%08x\n", Revb.COMB3_VOL);
	ConLog("    COMB4_VOL                  0x%08x\n", Revb.COMB4_VOL);

	ConLog("    COMB1_L_SRC, COMB1_R_SRC      0x%08x, 0x%08x\n", Revb.COMB1_L_SRC, Revb.COMB1_R_SRC);
	ConLog("    COMB2_L_SRC, COMB2_R_SRC      0x%08x, 0x%08x\n", Revb.COMB2_L_SRC, Revb.COMB2_R_SRC);
	ConLog("    COMB3_L_SRC, COMB3_R_SRC      0x%08x, 0x%08x\n", Revb.COMB3_L_SRC, Revb.COMB3_R_SRC);
	ConLog("    COMB4_L_SRC, COMB4_R_SRC      0x%08x, 0x%08x\n", Revb.COMB4_L_SRC, Revb.COMB4_R_SRC);

	ConLog("    SAME_L_SRC, SAME_R_SRC      0x%08x, 0x%08x\n", Revb.SAME_L_SRC, Revb.SAME_R_SRC);
	ConLog("    DIFF_L_SRC, DIFF_R_SRC      0x%08x, 0x%08x\n", Revb.DIFF_L_SRC, Revb.DIFF_R_SRC);
	ConLog("    SAME_L_DST, SAME_R_DST    0x%08x, 0x%08x\n", Revb.SAME_L_DST, Revb.SAME_R_DST);
	ConLog("    DIFF_L_DST, DIFF_R_DST    0x%08x, 0x%08x\n", Revb.DIFF_L_DST, Revb.DIFF_R_DST);
	ConLog("    IIR_VOL, WALL_VOL         0x%08x, 0x%08x\n", Revb.IIR_VOL, Revb.WALL_VOL);

	ConLog("    APF1_L_DST                 0x%08x\n", Revb.APF1_L_DST);
	ConLog("    APF1_R_DST                 0x%08x\n", Revb.APF1_R_DST);
	ConLog("    APF2_L_DST                 0x%08x\n", Revb.APF2_L_DST);
	ConLog("    APF2_R_DST                 0x%08x\n", Revb.APF2_R_DST);

	ConLog("    EffectsBufferSize           0x%x\n", EffectsBufferSize);
	ConLog("----------------------------------------------------------\n");
}
s32 V_Core::EffectsBufferIndexer(s32 offset) const
{
	// Should offsets be multipled by 4 or not?  Reverse-engineering of IOP code reveals
	// that it *4's all addresses before upping them to the SPU2 -- so our buffers are
	// already x4'd.  It doesn't really make sense that we should x4 them again, and this
	// seems to work. (feedback-free in bios and DDS)  --air

	// Need to use modulus here, because games can and will drop the buffer size
	// without notice, and it leads to offsets several times past the end of the buffer.

	if ((u32)offset >= (u32)EffectsBufferSize)
		return EffectsStartA + (offset % EffectsBufferSize) + (offset < 0 ? EffectsBufferSize : 0);
	else
		return EffectsStartA + offset;
}

void V_Core::UpdateEffectsBufferSize()
{
	const s32 newbufsize = EffectsEndA - EffectsStartA + 1;

	RevBuffers.NeedsUpdated = false;
	EffectsBufferSize = newbufsize;
	EffectsBufferStart = EffectsStartA;

	if (EffectsBufferSize <= 0)
		return;

	// debug: shows reverb parameters in console
	if (MsgToConsole())
		AnalyzeReverbPreset();

	// Rebuild buffer indexers.
	RevBuffers.COMB1_L_SRC = EffectsBufferIndexer(Revb.COMB1_L_SRC);
	RevBuffers.COMB1_R_SRC = EffectsBufferIndexer(Revb.COMB1_R_SRC);
	RevBuffers.COMB2_L_SRC = EffectsBufferIndexer(Revb.COMB2_L_SRC);
	RevBuffers.COMB2_R_SRC = EffectsBufferIndexer(Revb.COMB2_R_SRC);
	RevBuffers.COMB3_L_SRC = EffectsBufferIndexer(Revb.COMB3_L_SRC);
	RevBuffers.COMB3_R_SRC = EffectsBufferIndexer(Revb.COMB3_R_SRC);
	RevBuffers.COMB4_L_SRC = EffectsBufferIndexer(Revb.COMB4_L_SRC);
	RevBuffers.COMB4_R_SRC = EffectsBufferIndexer(Revb.COMB4_R_SRC);

	RevBuffers.SAME_L_DST = EffectsBufferIndexer(Revb.SAME_L_DST);
	RevBuffers.SAME_R_DST = EffectsBufferIndexer(Revb.SAME_R_DST);
	RevBuffers.DIFF_L_DST = EffectsBufferIndexer(Revb.DIFF_L_DST);
	RevBuffers.DIFF_R_DST = EffectsBufferIndexer(Revb.DIFF_R_DST);

	RevBuffers.SAME_L_SRC = EffectsBufferIndexer(Revb.SAME_L_SRC);
	RevBuffers.SAME_R_SRC = EffectsBufferIndexer(Revb.SAME_R_SRC);
	RevBuffers.DIFF_L_SRC = EffectsBufferIndexer(Revb.DIFF_L_SRC);
	RevBuffers.DIFF_R_SRC = EffectsBufferIndexer(Revb.DIFF_R_SRC);

	RevBuffers.APF1_L_DST = EffectsBufferIndexer(Revb.APF1_L_DST);
	RevBuffers.APF1_R_DST = EffectsBufferIndexer(Revb.APF1_R_DST);
	RevBuffers.APF2_L_DST = EffectsBufferIndexer(Revb.APF2_L_DST);
	RevBuffers.APF2_R_DST = EffectsBufferIndexer(Revb.APF2_R_DST);

	RevBuffers.SAME_L_PRV = EffectsBufferIndexer(Revb.SAME_L_DST - 1);
	RevBuffers.SAME_R_PRV = EffectsBufferIndexer(Revb.SAME_R_DST - 1);
	RevBuffers.DIFF_L_PRV = EffectsBufferIndexer(Revb.DIFF_L_DST - 1);
	RevBuffers.DIFF_R_PRV = EffectsBufferIndexer(Revb.DIFF_R_DST - 1);

	RevBuffers.APF1_L_SRC = EffectsBufferIndexer(Revb.APF1_L_DST - Revb.APF1_SIZE);
	RevBuffers.APF1_R_SRC = EffectsBufferIndexer(Revb.APF1_R_DST - Revb.APF1_SIZE);
	RevBuffers.APF2_L_SRC = EffectsBufferIndexer(Revb.APF2_L_DST - Revb.APF2_SIZE);
	RevBuffers.APF2_R_SRC = EffectsBufferIndexer(Revb.APF2_R_DST - Revb.APF2_SIZE);
}

void V_Voice::Start()
{
	PlayCycle = Cycles;
	LoopCycle = Cycles - 1; // Get it out of the start range as to not confuse it
	PendingLoopStart = false;
}

void V_Voice::Stop()
{
	ADSR.Value = 0;
	ADSR.Phase = 0;
}

uint TickInterval = 768;
static const int SanityInterval = 4800;
extern void UpdateDebugDialog();

__forceinline bool StartQueuedVoice(uint coreidx, uint voiceidx)
{
	V_Voice& vc(Cores[coreidx].Voices[voiceidx]);

	if ((Cycles - vc.PlayCycle) < 2)
		return false;

	if (vc.StartA & 7)
	{
		fprintf(stderr, " *** Misaligned StartA %05x!\n", vc.StartA);
		vc.StartA = (vc.StartA + 0xFFFF8) + 0x8;
	}

	vc.ADSR.Releasing = false;
	vc.ADSR.Value = 1;
	vc.ADSR.Phase = 1;
	vc.SCurrent = 28;
	vc.LoopMode = 0;

	// When SP >= 0 the next sample will be grabbed, we don't want this to happen
	// instantly because in the case of pitch being 0 we want to delay getting
	// the next block header.
	vc.SP = -1;

	vc.LoopFlags = 0;
	vc.NextA = vc.StartA | 1;
	vc.Prev1 = 0;
	vc.Prev2 = 0;

	vc.PV1 = vc.PV2 = 0;
	vc.PV3 = vc.PV4 = 0;
	vc.NextCrest = -0x8000;

	return true;
}

__forceinline void TimeUpdate(u32 cClocks)
{
	u32 dClocks = cClocks - lClocks;

	// Sanity Checks:
	//  It's not totally uncommon for the IOP's clock to jump backwards a cycle or two, and in
	//  such cases we just want to ignore the TimeUpdate call.

	if (dClocks > (u32)-15)
		return;

	//  But if for some reason our clock value seems way off base (typically due to bad dma
	//  timings from PCSX2), just mix out a little bit, skip the rest, and hope the ship
	//  "rights" itself later on.

	if (dClocks > (u32)(TickInterval * SanityInterval))
	{
		if (MsgToConsole())
			ConLog(" * SPU2 > TimeUpdate Sanity Check (Tick Delta: %d) (PS2 Ticks: %d)\n", dClocks / TickInterval, cClocks / TickInterval);
		dClocks = TickInterval * SanityInterval;
		lClocks = cClocks - dClocks;
	}

// Visual debug display showing all core's activity! Disabled via #define on release builds.
#if defined(_WIN32) && !defined(PCSX2_CORE)
	UpdateDebugDialog();
#endif

	if (SynchMode == 1) // AsyncMix on
		SndBuffer::UpdateTempoChangeAsyncMixing();
	else
		TickInterval = 768; // Reset to default, in case the user hotswitched from async to something else.

	//Update Mixing Progress
	while (dClocks >= TickInterval)
	{
		for (int i = 0; i < 2; i++)
		{
			if (has_to_call_irq[i])
			{
				//ConLog("* SPU2: Irq Called (%04x) at cycle %d.\n", Spdif.Info, Cycles);
				has_to_call_irq[i] = false;
				if (!(Spdif.Info & (4 << i)) && Cores[i].IRQEnable)
				{
					Spdif.Info |= (4 << i);
					spu2Irq();
				}
			}
		}

		dClocks -= TickInterval;
		lClocks += TickInterval;
		Cycles++;

		// Start Queued Voices, they start after 2T (Tested on real HW)
		for(int c = 0; c < 2; c++)
			for (int v = 0; v < 24; v++)
				if(Cores[c].KeyOn & (1 << v))
					if(StartQueuedVoice(c, v))
						Cores[c].KeyOn &= ~(1 << v);
		// Note: IOP does not use MMX regs, so no need to save them.
		//SaveMMXRegs();
		Mix();
		//RestoreMMXRegs();
	}

	//Update DMA4 interrupt delay counter
	if (Cores[0].DMAICounter > 0 && (psxRegs.cycle - Cores[0].LastClock) > 0)
	{
		const u32 amt = std::min(psxRegs.cycle - Cores[0].LastClock, (u32)Cores[0].DMAICounter);
		Cores[0].DMAICounter -= amt;
		Cores[0].LastClock = psxRegs.cycle;
		if(!Cores[0].AdmaInProgress)
			HW_DMA4_MADR += amt / 2;

		if (Cores[0].DMAICounter <= 0)
		{
			for (int i = 0; i < 2; i++)
			{
				if (has_to_call_irq_dma[i])
				{
					//ConLog("* SPU2: Irq Called (%04x) at cycle %d.\n", Spdif.Info, Cycles);
					has_to_call_irq_dma[i] = false;
					if (!(Spdif.Info & (4 << i)) && Cores[i].IRQEnable)
					{
						Spdif.Info |= (4 << i);
						spu2Irq();
					}
				}
			}

			if (((Cores[0].AutoDMACtrl & 1) != 1) && Cores[0].ReadSize)
			{
				if (Cores[0].IsDMARead)
					Cores[0].FinishDMAread();
				else
					Cores[0].FinishDMAwrite();
			}

			if (Cores[0].DMAICounter <= 0)
			{
				HW_DMA4_MADR = HW_DMA4_TADR;
				spu2DMA4Irq();
			}
		}
		else
		{
			if (((psxCounters[6].sCycleT + psxCounters[6].CycleT) - psxRegs.cycle) > (u32)Cores[0].DMAICounter)
			{
				psxCounters[6].sCycleT = psxRegs.cycle;
				psxCounters[6].CycleT = Cores[0].DMAICounter;

				psxNextCounter -= (psxRegs.cycle - psxNextsCounter);
				psxNextsCounter = psxRegs.cycle;
				if (psxCounters[6].CycleT < psxNextCounter)
					psxNextCounter = psxCounters[6].CycleT;
			}
		}
	}

	//Update DMA7 interrupt delay counter
	if (Cores[1].DMAICounter > 0 && (psxRegs.cycle - Cores[1].LastClock) > 0)
	{
		const u32 amt = std::min(psxRegs.cycle - Cores[1].LastClock, (u32)Cores[1].DMAICounter);
		Cores[1].DMAICounter -= amt;
		Cores[1].LastClock = psxRegs.cycle;
		if (!Cores[1].AdmaInProgress)
			HW_DMA7_MADR += amt / 2;
		if (Cores[1].DMAICounter <= 0)
		{
			for (int i = 0; i < 2; i++)
			{
				if (has_to_call_irq_dma[i])
				{
					//ConLog("* SPU2: Irq Called (%04x) at cycle %d.\n", Spdif.Info, Cycles);
					has_to_call_irq_dma[i] = false;
					if (!(Spdif.Info & (4 << i)) && Cores[i].IRQEnable)
					{
						Spdif.Info |= (4 << i);
						spu2Irq();
					}
				}
			}

			if (((Cores[1].AutoDMACtrl & 2) != 2) && Cores[1].ReadSize)
			{
				if (Cores[1].IsDMARead)
					Cores[1].FinishDMAread();
				else
					Cores[1].FinishDMAwrite();
			}

			if (Cores[1].DMAICounter <= 0)
			{
				HW_DMA7_MADR = HW_DMA7_TADR;
				spu2DMA7Irq();
			}
		}
		else
		{
			if (((psxCounters[6].sCycleT + psxCounters[6].CycleT) - psxRegs.cycle) > (u32)Cores[1].DMAICounter)
			{
				psxCounters[6].sCycleT = psxRegs.cycle;
				psxCounters[6].CycleT = Cores[1].DMAICounter;

				psxNextCounter -= (psxRegs.cycle - psxNextsCounter);
				psxNextsCounter = psxRegs.cycle;
				if (psxCounters[6].CycleT < psxNextCounter)
					psxNextCounter = psxCounters[6].CycleT;
			}
		}
	}
}

__forceinline void UpdateSpdifMode()
{
	int OPM = PlayMode;

	if (Spdif.Out & 0x4) // use 24/32bit PCM data streaming
	{
		PlayMode = 8;
		ConLog("* SPU2: WARNING: Possibly CDDA mode set!\n");
		return;
	}

	if (Spdif.Out & SPDIF_OUT_BYPASS)
	{
		PlayMode = 2;
		if (!(Spdif.Mode & SPDIF_MODE_BYPASS_BITSTREAM))
			PlayMode = 4; //bitstream bypass
	}
	else
	{
		PlayMode = 0; //normal processing
		if (Spdif.Out & SPDIF_OUT_PCM)
		{
			PlayMode = 1;
		}
	}
	if (OPM != PlayMode)
	{
		ConLog("* SPU2: Play Mode Set to %s (%d).\n",
			   (PlayMode == 0) ? "Normal" : ((PlayMode == 1) ? "PCM Clone" : ((PlayMode == 2) ? "PCM Bypass" : "BitStream Bypass")), PlayMode);
	}
}

// Converts an SPU2 register volume write into a 32 bit SPU2 volume.  The value is extended
// properly into the lower 16 bits of the value to provide a full spectrum of volumes.
static s32 GetVol32(u16 src)
{
	return (((s32)src) << 16) | ((src << 1) & 0xffff);
}

void V_VolumeSlide::RegSet(u16 src)
{
	Value = GetVol32(src);
}

static u32 map_spu1to2(u32 addr)
{
	return addr * 4 + (addr >= 0x200 ? 0xc0000 : 0);
}

static u32 map_spu2to1(u32 addr)
{
	// if (addr >= 0x800 && addr < 0xc0000) oh dear
	return (addr - (addr >= 0xc0000 ? 0xc0000 : 0)) / 4;
}

void V_Core::WriteRegPS1(u32 mem, u16 value)
{
	pxAssume(Index == 0); // Valid on Core 0 only!

	bool show = true;
	u32 reg = mem & 0xffff;

	if ((reg >= 0x1c00) && (reg < 0x1d80))
	{
		//voice values
		u8 voice = ((reg - 0x1c00) >> 4);
		u8 vval = reg & 0xf;
		switch (vval)
		{
			case 0x0: //VOLL (Volume L)
			case 0x2: //VOLR (Volume R)
			{
				V_VolumeSlide& thisvol = vval == 0 ? Voices[voice].Volume.Left : Voices[voice].Volume.Right;
				thisvol.Reg_VOL = value;

				if (value & 0x8000) // +Lin/-Lin/+Exp/-Exp
				{
					thisvol.Mode = (value & 0xF000) >> 12;
					thisvol.Increment = (value & 0x7F);
					// We're not sure slides work 100%
					if (IsDevBuild)
						ConLog("* SPU2: Voice uses Slides in Mode = %x, Increment = %x\n", thisvol.Mode, thisvol.Increment);
				}
				else
				{
					// Constant Volume mode (no slides or envelopes)
					// Volumes range from 0x3fff to 0x7fff, with 0x4000 serving as
					// the "sign" bit, so a simple bitwise extension will do the trick:

					thisvol.RegSet(value << 1);
					thisvol.Mode = 0;
					thisvol.Increment = 0;
				}
				//ConLog("voice %x VOL%c write: %x\n", voice, vval == 0 ? 'L' : 'R', value);
				break;
			}
			case 0x4:
				Voices[voice].Pitch = value;
				//ConLog("voice %x Pitch write: %x\n", voice, Voices[voice].Pitch);
				break;
			case 0x6:
				Voices[voice].StartA = map_spu1to2(value);
				//ConLog("voice %x StartA write: %x\n", voice, Voices[voice].StartA);
				break;

			case 0x8: // ADSR1 (Envelope)
				Voices[voice].ADSR.regADSR1 = value;
				//ConLog("voice %x regADSR1 write: %x\n", voice, Voices[voice].ADSR.regADSR1);
				break;

			case 0xa: // ADSR2 (Envelope)
				Voices[voice].ADSR.regADSR2 = value;
				//ConLog("voice %x regADSR2 write: %x\n", voice, Voices[voice].ADSR.regADSR2);
				break;
			case 0xc: // Voice 0..23 ADSR Current Volume
				// not commonly set by games
				Voices[voice].ADSR.Value = value * 0x10001U;
				ConLog("voice %x ADSR.Value write: %x\n", voice, Voices[voice].ADSR.Value);
				break;
			case 0xe:
				Voices[voice].LoopStartA = map_spu1to2(value);
				//ConLog("voice %x LoopStartA write: %x\n", voice, Voices[voice].LoopStartA);
				break;

				jNO_DEFAULT;
		}
	}

	else
		switch (reg)
		{
			case 0x1d80: //         Mainvolume left
				MasterVol.Left.Mode = 0;
				MasterVol.Left.RegSet(value);
				break;

			case 0x1d82: //         Mainvolume right
				MasterVol.Right.Mode = 0;
				MasterVol.Right.RegSet(value);
				break;

			case 0x1d84: //         Reverberation depth left
				FxVol.Left = GetVol32(value);
				break;

			case 0x1d86: //         Reverberation depth right
				FxVol.Right = GetVol32(value);
				break;

			case 0x1d88: //         Voice ON  (0-15)
				SPU2_FastWrite(REG_S_KON, value);
				break;
			case 0x1d8a: //         Voice ON  (16-23)
				SPU2_FastWrite(REG_S_KON + 2, value);
				break;

			case 0x1d8c: //         Voice OFF (0-15)
				SPU2_FastWrite(REG_S_KOFF, value);
				break;
			case 0x1d8e: //         Voice OFF (16-23)
				SPU2_FastWrite(REG_S_KOFF + 2, value);
				break;

			case 0x1d90: //         Channel FM (pitch lfo) mode (0-15)
				SPU2_FastWrite(REG_S_PMON, value);
				if (value != 0)
					ConLog("SPU2 warning: wants to set Pitch Modulation reg1 to %x \n", value);
				break;

			case 0x1d92: //         Channel FM (pitch lfo) mode (16-23)
				SPU2_FastWrite(REG_S_PMON + 2, value);
				if (value != 0)
					ConLog("SPU2 warning: wants to set Pitch Modulation reg2 to %x \n", value);
				break;


			case 0x1d94: //         Channel Noise mode (0-15)
				SPU2_FastWrite(REG_S_NON, value);
				if (value != 0)
					ConLog("SPU2 warning: wants to set Channel Noise mode reg1 to %x\n", value);
				break;

			case 0x1d96: //         Channel Noise mode (16-23)
				SPU2_FastWrite(REG_S_NON + 2, value);
				if (value != 0)
					ConLog("SPU2 warning: wants to set Channel Noise mode reg2 to %x\n", value);
				break;

			case 0x1d98: //         1F801D98h - Voice 0..23 Reverb mode aka Echo On (EON) (R/W)
				//Regs.VMIXEL = value & 0xFFFF;
				SPU2_FastWrite(REG_S_VMIXEL, value);
				SPU2_FastWrite(REG_S_VMIXER, value);
				//ConLog("SPU2 warning: setting reverb mode reg1 to %x \n", Regs.VMIXEL);
				break;

			case 0x1d9a: //         1F801D98h + 2 - Voice 0..23 Reverb mode aka Echo On (EON) (R/W)
				//Regs.VMIXEL = value << 16;
				SPU2_FastWrite(REG_S_VMIXEL + 2, value);
				SPU2_FastWrite(REG_S_VMIXER + 2, value);
				//ConLog("SPU2 warning: setting reverb mode reg2 to %x \n", Regs.VMIXEL);
				break;

			// this was wrong? // edit: appears so!
			//case 0x1d9c://         Channel Reverb mode (0-15)
			//	SPU2_FastWrite(REG_S_VMIXL,value);
			//	SPU2_FastWrite(REG_S_VMIXR,value);
			//break;

			//case 0x1d9e://         Channel Reverb mode (16-23)
			//	SPU2_FastWrite(REG_S_VMIXL+2,value);
			//	SPU2_FastWrite(REG_S_VMIXR+2,value);
			//break;
			case 0x1d9c: // Voice 0..15 ON/OFF (status) (ENDX) (R) // writeable but hw overrides it shortly after
				//Regs.ENDX &= 0xff0000;
				ConLog("SPU2 warning: wants to set ENDX reg1 to %x \n", value);
				break;

			case 0x1d9e: //         // Voice 15..23 ON/OFF (status) (ENDX) (R) // writeable but hw overrides it shortly after
				//Regs.ENDX &= 0xffff;
				ConLog("SPU2 warning: wants to set ENDX reg2 to %x \n", value);
				break;

			case 0x1da2: //         Reverb work area start
			{
				EffectsStartA = map_spu1to2(value);
				//EffectsEndA = 0xFFFFF; // fixed EndA in psx mode
				Cores[0].RevBuffers.NeedsUpdated = true;
				ReverbX = 0;
			}
			break;

			case 0x1da4:
				IRQA = map_spu1to2(value);
				//ConLog("SPU2 Setting IRQA to %x \n", IRQA);
				break;

			case 0x1da6:
				TSA = map_spu1to2(value);
				ConLog("SPU2 Setting TSA to %x \n", TSA);
				break;

			case 0x1da8: // Spu Write to Memory
				//ConLog("SPU direct DMA Write. Current TSA = %x\n", TSA);
				Cores[0].ActiveTSA = Cores[0].TSA;
				if (Cores[0].IRQEnable && (Cores[0].IRQA <= Cores[0].ActiveTSA))
				{
					SetIrqCall(0);
					spu2Irq();
				}
				DmaWrite(value);
				show = false;
				break;

			case 0x1daa:
				SPU2_FastWrite(REG_C_ATTR, value);
				break;

			case 0x1dac: // 1F801DACh - Sound RAM Data Transfer Control (should be 0004h)
				ConLog("SPU Sound RAM Data Transfer Control (should be 4) : value = %x \n", value);
				psxSoundDataTransferControl = value;
				break;

			case 0x1dae: // 1F801DAEh - SPU Status Register (SPUSTAT) (R)
						 // The SPUSTAT register should be treated read-only (writing is possible in so far that the written
						 // value can be read-back for a short moment, however, thereafter the hardware is overwriting that value).
						 //Regs.STATX = value;
				break;

			case 0x1DB0: // 1F801DB0h 4  CD Volume Left/Right
				break;   // cd left?
			case 0x1DB2:
				break;   // cd right?
			case 0x1DB4: // 1F801DB4h 4  Extern Volume Left / Right
				break;   // Extern left?
			case 0x1DB6:
				break;   // Extern right?
			case 0x1DB8: // 1F801DB8h 4  Current Main Volume Left/Right
				break;   // Current left?
			case 0x1DBA:
				break;   // Current right?
			case 0x1DBC: // 1F801DBCh 4  Unknown? (R/W)
				break;
			case 0x1DBE:
				break;

			case 0x1DC0:
				Revb.APF1_SIZE = value * 4;
				break;
			case 0x1DC2:
				Revb.APF2_SIZE = value * 4;
				break;
			case 0x1DC4:
				Revb.IIR_VOL = value;
				break;
			case 0x1DC6:
				Revb.COMB1_VOL = value;
				break;
			case 0x1DC8:
				Revb.COMB2_VOL = value;
				break;
			case 0x1DCA:
				Revb.COMB3_VOL = value;
				break;
			case 0x1DCC:
				Revb.COMB4_VOL = value;
				break;
			case 0x1DCE:
				Revb.WALL_VOL = value;
				break;
			case 0x1DD0:
				Revb.APF1_VOL = value;
				break;
			case 0x1DD2:
				Revb.APF2_VOL = value;
				break;
			case 0x1DD4:
				Revb.SAME_L_DST = value * 4;
				break;
			case 0x1DD6:
				Revb.SAME_R_DST = value * 4;
				break;
			case 0x1DD8:
				Revb.COMB1_L_SRC = value * 4;
				break;
			case 0x1DDA:
				Revb.COMB1_R_SRC = value * 4;
				break;
			case 0x1DDC:
				Revb.COMB2_L_SRC = value * 4;
				break;
			case 0x1DDE:
				Revb.COMB2_R_SRC = value * 4;
				break;
			case 0x1DE0:
				Revb.SAME_L_SRC = value * 4;
				break;
			case 0x1DE2:
				Revb.SAME_R_SRC = value * 4;
				break;
			case 0x1DE4:
				Revb.DIFF_L_DST = value * 4;
				break;
			case 0x1DE6:
				Revb.DIFF_R_DST = value * 4;
				break;
			case 0x1DE8:
				Revb.COMB3_L_SRC = value * 4;
				break;
			case 0x1DEA:
				Revb.COMB3_R_SRC = value * 4;
				break;
			case 0x1DEC:
				Revb.COMB4_L_SRC = value * 4;
				break;
			case 0x1DEE:
				Revb.COMB4_R_SRC = value * 4;
				break;
			case 0x1DF0:
				Revb.DIFF_L_SRC = value * 4;
				break; // DIFF_R_SRC and DIFF_L_SRC supposedly swapped on SPU2
			case 0x1DF2:
				Revb.DIFF_R_SRC = value * 4;
				break; // but I don't believe it! (games in psxmode sound better unswapped)
			case 0x1DF4:
				Revb.APF1_L_DST = value * 4;
				break;
			case 0x1DF6:
				Revb.APF1_R_DST = value * 4;
				break;
			case 0x1DF8:
				Revb.APF2_L_DST = value * 4;
				break;
			case 0x1DFA:
				Revb.APF2_R_DST = value * 4;
				break;
			case 0x1DFC:
				Revb.IN_COEF_L = value;
				break;
			case 0x1DFE:
				Revb.IN_COEF_R = value;
				break;
		}

	if (show)
		FileLog("[%10d] (!) SPU write mem %08x value %04x\n", Cycles, mem, value);

	spu2Ru16(mem) = value;
}

u16 V_Core::ReadRegPS1(u32 mem)
{
	pxAssume(Index == 0); // Valid on Core 0 only!

	bool show = true;
	u16 value = spu2Ru16(mem);

	u32 reg = mem & 0xffff;

	if ((reg >= 0x1c00) && (reg < 0x1d80))
	{
		//voice values
		u8 voice = ((reg - 0x1c00) >> 4);
		u8 vval = reg & 0xf;
		switch (vval)
		{
			case 0x0: //VOLL (Volume L)
				//value=Voices[voice].VolumeL.Mode;
				//value=Voices[voice].VolumeL.Value;
				value = Voices[voice].Volume.Left.Reg_VOL;
				break;

			case 0x2: //VOLR (Volume R)
				//value=Voices[voice].VolumeR.Mode;
				//value=Voices[voice].VolumeR.Value;
				value = Voices[voice].Volume.Right.Reg_VOL;
				break;

			case 0x4:
				value = Voices[voice].Pitch;
				//ConLog("voice %d read pitch result = %x\n", voice, value);
				break;
			case 0x6:
				value = map_spu2to1(Voices[voice].StartA);
				//ConLog("voice %d read StartA result = %x\n", voice, value);
				break;
			case 0x8:
				value = Voices[voice].ADSR.regADSR1;
				break;
			case 0xa:
				value = Voices[voice].ADSR.regADSR2;
				break;
			case 0xc:                                   // Voice 0..23 ADSR Current Volume
				value = Voices[voice].ADSR.Value >> 16; // no clue
				//if (value != 0) ConLog("voice %d read ADSR.Value result = %x\n", voice, value);
				break;
			case 0xe:
				value = map_spu2to1(Voices[voice].LoopStartA);
				//ConLog("voice %d read LoopStartA result = %x\n", voice, value);
				break;

				jNO_DEFAULT;
		}
	}
	else
		switch (reg)
		{
			case 0x1d80:
				value = MasterVol.Left.Value >> 16;
				break;
			case 0x1d82:
				value = MasterVol.Right.Value >> 16;
				break;
			case 0x1d84:
				value = FxVol.Left >> 16;
				break;
			case 0x1d86:
				value = FxVol.Right >> 16;
				break;

			case 0x1d88:
				value = 0;
				break; // Voice 0..23 Key ON(Start Attack / Decay / Sustain) (W)
			case 0x1d8a:
				value = 0;
				break;
			case 0x1d8c:
				value = 0;
				break; // Voice 0..23 Key OFF (Start Release) (W)
			case 0x1d8e:
				value = 0;
				break;

			case 0x1d90:
				value = Regs.PMON & 0xFFFF;
				break; // Voice 0..23 Channel FM(pitch lfo) mode(R / W)
			case 0x1d92:
				value = Regs.PMON >> 16;
				break;

			case 0x1d94:
				value = Regs.NON & 0xFFFF;
				break; // Voice 0..23 Channel Noise mode (R/W)
			case 0x1d96:
				value = Regs.NON >> 16;
				break;

			case 0x1d98:
				value = Regs.VMIXEL & 0xFFFF;
				break; // Voice 0..23 Channel Reverb mode (R/W)
			case 0x1d9a:
				value = Regs.VMIXEL >> 16;
				break;
				/*case 0x1d9c: value = Regs.VMIXL&0xFFFF;  break;*/ // this is wrong?
			/*case 0x1d9e: value = Regs.VMIXL >> 16;   break;*/
			case 0x1d9c:
				value = Regs.ENDX & 0xFFFF;
				break; // Voice 0..23 Channel ON / OFF(status) (R) (ENDX)
			case 0x1d9e:
				value = Regs.ENDX >> 16;
				break;
			case 0x1da2:
				value = map_spu2to1(EffectsStartA);
				break;
			case 0x1da4:
				value = map_spu2to1(IRQA);
				//ConLog("SPU2 IRQA read: 0x1da4 = %x , (IRQA = %x)\n", value, IRQA);
				break;
			case 0x1da6:
				value = map_spu2to1(TSA);
				//ConLog("SPU2 TSA read: 0x1da6 = %x , (TSA = %x)\n", value, TSA);
				break;
			case 0x1da8:
				ActiveTSA = TSA;
				value = DmaRead();
				show = false;
				break;
			case 0x1daa:
				value = Cores[0].Regs.ATTR;
				//ConLog("SPU2 ps1 reg psxSPUCNT read return value: %x\n", value);
				break;
			case 0x1dac: // 1F801DACh - Sound RAM Data Transfer Control (should be 0004h)
				value = psxSoundDataTransferControl;
				break;
			case 0x1dae:
				value = Cores[0].Regs.STATX;
				//ConLog("SPU2 ps1 reg REG_P_STATX read return value: %x\n", value);
				break;
		}

	if (show)
		FileLog("[%10d] (!) SPU read mem %08x value %04x\n", Cycles, mem, value);
	return value;
}

// Ah the joys of endian-specific code! :D
static __forceinline void SetHiWord(u32& src, u16 value)
{
	((u16*)&src)[1] = value;
}

static __forceinline void SetLoWord(u32& src, u16 value)
{
	((u16*)&src)[0] = value;
}

static __forceinline u16 GetHiWord(u32& src)
{
	return ((u16*)&src)[1];
}

static __forceinline u16 GetLoWord(u32& src)
{
	return ((u16*)&src)[0];
}

template <int CoreIdx, int VoiceIdx, int param>
static void RegWrite_VoiceParams(u16 value)
{
	const int core = CoreIdx;
	const int voice = VoiceIdx;

	V_Voice& thisvoice = Cores[core].Voices[voice];

	switch (param)
	{
		case 0: //VOLL (Volume L)
		case 1: //VOLR (Volume R)
		{
			V_VolumeSlide& thisvol = (param == 0) ? thisvoice.Volume.Left : thisvoice.Volume.Right;
			thisvol.Reg_VOL = value;

			if (value & 0x8000) // +Lin/-Lin/+Exp/-Exp
			{
				thisvol.Mode = (value & 0xF000) >> 12;
				thisvol.Increment = (value & 0x7F);
				// We're not sure slides work 100%
				if (IsDevBuild)
					ConLog("* SPU2: Voice uses Slides in Mode = %x, Increment = %x\n", thisvol.Mode, thisvol.Increment);
			}
			else
			{
				// Constant Volume mode (no slides or envelopes)
				// Volumes range from 0x3fff to 0x7fff, with 0x4000 serving as
				// the "sign" bit, so a simple bitwise extension will do the trick:

				thisvol.RegSet(value << 1);
				thisvol.Mode = 0;
				thisvol.Increment = 0;
			}
		}
		break;

		case 2:
			thisvoice.Pitch = value;
			break;

		case 3: // ADSR1 (Envelope)
			thisvoice.ADSR.regADSR1 = value;
			break;

		case 4: // ADSR2 (Envelope)
			thisvoice.ADSR.regADSR2 = value;
			break;

			// REG_VP_ENVX, REG_VP_VOLXL and REG_VP_VOLXR have been confirmed to not be allowed to be written to, so code has been commented out.
			// Colin McRae Rally 2005 triggers case 5 (ADSR), but it doesn't produce issues enabled or disabled.

		case 5:
			// [Air] : Mysterious ADSR set code.  Too bad none of my games ever use it.
			//      (as usual... )
			//thisvoice.ADSR.Value = (value << 16) | value;
			//ConLog("* SPU2: Mysterious ADSR Volume Set to 0x%x\n", value);
			break;

		case 6:
			//thisvoice.Volume.Left.RegSet(value);
			break;
		case 7:
			//thisvoice.Volume.Right.RegSet(value);
			break;

			jNO_DEFAULT;
	}
}

template <int CoreIdx, int VoiceIdx, int address>
static void RegWrite_VoiceAddr(u16 value)
{
	const int core = CoreIdx;
	const int voice = VoiceIdx;

	V_Voice& thisvoice = Cores[core].Voices[voice];

	switch (address)
	{
		case 0: // SSA (Waveform Start Addr) (hiword, 4 bits only)
			thisvoice.StartA = ((u32)(value & 0x0F) << 16) | (thisvoice.StartA & 0xFFF8);
			if (IsDevBuild)
				DebugCores[core].Voices[voice].lastSetStartA = thisvoice.StartA;
			break;

		case 1: // SSA (loword)
			thisvoice.StartA = (thisvoice.StartA & 0x0F0000) | (value & 0xFFF8);
			if (IsDevBuild)
				DebugCores[core].Voices[voice].lastSetStartA = thisvoice.StartA;
			break;

		case 2:
			{
				u32* LoopReg;
				if ((Cycles - thisvoice.PlayCycle) < 4)
				{
					LoopReg = &thisvoice.PendingLoopStartA;
					thisvoice.PendingLoopStart = true;
				}
				else
				{
					LoopReg = &thisvoice.LoopStartA;
					thisvoice.LoopMode = 1;
				}

				*LoopReg = ((u32)(value & 0x0F) << 16) | (*LoopReg & 0xFFF8);
			}
			break;

		case 3:
			{
				u32* LoopReg;
				if ((Cycles - thisvoice.PlayCycle) < 4)
				{
					LoopReg = &thisvoice.PendingLoopStartA;
					thisvoice.PendingLoopStart = true;
				}
				else
				{
					LoopReg = &thisvoice.LoopStartA;
					thisvoice.LoopMode = 1;
				}
				
				*LoopReg = (*LoopReg & 0x0F0000) | (value & 0xFFF8);
			}
			break;

			// Note that there's no proof that I know of that writing to NextA is
			// even allowed or handled by the SPU2 (it might be disabled or ignored,
			// for example).  Tests should be done to find games that write to this
			// reg, and see if they're buggy or not. --air

			// FlatOut & Soul Reaver 2 trigger these cases, but don't produce issues enabled or disabled.
			// Wallace And Gromit: Curse Of The Were-Rabbit triggers case 4 and 5 to produce proper sound,
			// without it some sound effects get cut off so we need the two NextA cases enabled.

		case 4:
			thisvoice.NextA = ((u32)(value & 0x0F) << 16) | (thisvoice.NextA & 0xFFF8) | 1;
			thisvoice.SCurrent = 28;
			break;

		case 5:
			thisvoice.NextA = (thisvoice.NextA & 0x0F0000) | (value & 0xFFF8) | 1;
			thisvoice.SCurrent = 28;
			break;
	}
}

template <int CoreIdx, int cAddr>
static void RegWrite_Core(u16 value)
{
	const int omem = cAddr;
	const int core = CoreIdx;
	V_Core& thiscore = Cores[core];

	switch (omem)
	{
		case REG__1AC:
			// ----------------------------------------------------------------------------
			// 0x1ac / 0x5ac : direct-write to DMA address : special register (undocumented)
			// ----------------------------------------------------------------------------
			// On the GS, DMAs are actually pushed through a hardware register.  Chances are the
			// SPU works the same way, and "technically" *all* DMA data actually passes through
			// the HW registers at 0x1ac (core0) and 0x5ac (core1).  We handle normal DMAs in
			// optimized block copy fashion elsewhere, but some games will write this register
			// directly, so handle those here:

			// Performance Note: The PS2 Bios uses this extensively right before booting games,
			// causing massive slowdown if we don't shortcut it here.
			thiscore.ActiveTSA = thiscore.TSA;
			for (int i = 0; i < 2; i++)
			{
				if (Cores[i].IRQEnable && (Cores[i].IRQA == thiscore.ActiveTSA))
				{
					SetIrqCall(i);
				}
			}
			thiscore.DmaWrite(value);
			break;

		case REG_C_ATTR:
		{
			bool irqe = thiscore.IRQEnable;
			int bit0 = thiscore.AttrBit0;
			bool fxenable = thiscore.FxEnable;
			u8 oldDmaMode = thiscore.DmaMode;

			thiscore.AttrBit0 = (value >> 0) & 0x01;  //1 bit
			thiscore.DMABits = (value >> 1) & 0x07;   //3 bits
			thiscore.DmaMode = (value >> 4) & 0x03;   //2 bit (not necessary, we get the direction from the iop)
			thiscore.IRQEnable = (value >> 6) & 0x01; //1 bit
			thiscore.FxEnable = (value >> 7) & 0x01;  //1 bit
			thiscore.NoiseClk = (value >> 8) & 0x3f;  //6 bits
			//thiscore.Mute		=(value>>14) & 0x01; //1 bit
			thiscore.Mute = 0;
			//thiscore.CoreEnabled=(value>>15) & 0x01; //1 bit
			// no clue
			thiscore.Regs.ATTR = value & 0xffff;

			if (fxenable && !thiscore.FxEnable && (thiscore.EffectsStartA != thiscore.ExtEffectsStartA || thiscore.EffectsEndA != thiscore.ExtEffectsEndA))
			{
				thiscore.EffectsStartA = thiscore.ExtEffectsStartA;
				thiscore.EffectsEndA = thiscore.ExtEffectsEndA;
				thiscore.ReverbX = 0;
				thiscore.RevBuffers.NeedsUpdated = true;
			}

			if (!thiscore.DmaMode && !(thiscore.Regs.STATX & 0x400))
				thiscore.Regs.STATX &= ~0x80;
			else if(!oldDmaMode && thiscore.DmaMode)
				thiscore.Regs.STATX |= 0x80;

			thiscore.ActiveTSA = thiscore.TSA;

			if (value & 0x000E)
			{
				if (MsgToConsole())
					ConLog("* SPU2: Core %d ATTR unknown bits SET! value=%04x\n", core, value);
			}

			if (thiscore.AttrBit0 != bit0)
			{
				if (MsgToConsole())
					ConLog("* SPU2: ATTR bit 0 set to %d\n", thiscore.AttrBit0);
			}
			if (thiscore.IRQEnable != irqe)
			{
				//ConLog("* SPU2: Core%d IRQ %s at cycle %d. Current IRQA = %x Current EffectA = %x\n",
				//	core, ((thiscore.IRQEnable==0)?"disabled":"enabled"), Cycles, thiscore.IRQA, thiscore.EffectsStartA);

				if (!thiscore.IRQEnable)
					Spdif.Info &= ~(4 << thiscore.Index);
				else
					if ((thiscore.IRQA & 0xFFF00000) != 0)
						DevCon.Warning("SPU2: Core %d IRQA Outside of SPU2 memory, Addr %x", thiscore.Index, thiscore.IRQA);
			}
		}
		break;

		case REG_S_PMON:
			for (int vc = 1; vc < 16; ++vc)
				thiscore.Voices[vc].Modulated = (value >> vc) & 1;
			SetLoWord(thiscore.Regs.PMON, value);
			break;

		case (REG_S_PMON + 2):
			for (int vc = 0; vc < 8; ++vc)
				thiscore.Voices[vc + 16].Modulated = (value >> vc) & 1;
			SetHiWord(thiscore.Regs.PMON, value);
			break;

		case REG_S_NON:
			for (int vc = 0; vc < 16; ++vc)
				thiscore.Voices[vc].Noise = (value >> vc) & 1;
			SetLoWord(thiscore.Regs.NON, value);
			break;

		case (REG_S_NON + 2):
			for (int vc = 0; vc < 8; ++vc)
				thiscore.Voices[vc + 16].Noise = (value >> vc) & 1;
			SetHiWord(thiscore.Regs.NON, value);
			break;

// Games like to repeatedly write these regs over and over with the same value, hence
// the shortcut that skips the bitloop if the values are equal.
#define vx_SetSomeBits(reg_out, mask_out, hiword)                       \
	{                                                                   \
		const u32 result = thiscore.Regs.reg_out;                       \
		if (hiword)                                                     \
			SetHiWord(thiscore.Regs.reg_out, value);                    \
		else                                                            \
			SetLoWord(thiscore.Regs.reg_out, value);                    \
		if (result == thiscore.Regs.reg_out)                            \
			break;                                                      \
                                                                        \
		const uint start_bit = (hiword) ? 16 : 0;                       \
		const uint end_bit = (hiword) ? 24 : 16;                        \
		for (uint vc = start_bit, vx = 1; vc < end_bit; ++vc, vx <<= 1) \
			thiscore.VoiceGates[vc].mask_out = (value & vx) ? -1 : 0;   \
	}

		case REG_S_VMIXL:
			vx_SetSomeBits(VMIXL, DryL, false);
			break;

		case (REG_S_VMIXL + 2):
			vx_SetSomeBits(VMIXL, DryL, true);
			break;

		case REG_S_VMIXEL:
			vx_SetSomeBits(VMIXEL, WetL, false);
			break;

		case (REG_S_VMIXEL + 2):
			vx_SetSomeBits(VMIXEL, WetL, true);
			break;

		case REG_S_VMIXR:
			vx_SetSomeBits(VMIXR, DryR, false);
			break;

		case (REG_S_VMIXR + 2):
			vx_SetSomeBits(VMIXR, DryR, true);
			break;

		case REG_S_VMIXER:
			vx_SetSomeBits(VMIXER, WetR, false);
			break;

		case (REG_S_VMIXER + 2):
			vx_SetSomeBits(VMIXER, WetR, true);
			break;

		case REG_P_MMIX:
		{
			// Each MMIX gate is assigned either 0 or 0xffffffff depending on the status
			// of the MMIX bits.  I use -1 below as a shorthand for 0xffffffff. :)

			const int vx = value & ((core == 0) ? 0xFF0 : 0xFFF);
			thiscore.WetGate.ExtR = (vx & 0x001) ? -1 : 0;
			thiscore.WetGate.ExtL = (vx & 0x002) ? -1 : 0;
			thiscore.DryGate.ExtR = (vx & 0x004) ? -1 : 0;
			thiscore.DryGate.ExtL = (vx & 0x008) ? -1 : 0;
			thiscore.WetGate.InpR = (vx & 0x010) ? -1 : 0;
			thiscore.WetGate.InpL = (vx & 0x020) ? -1 : 0;
			thiscore.DryGate.InpR = (vx & 0x040) ? -1 : 0;
			thiscore.DryGate.InpL = (vx & 0x080) ? -1 : 0;
			thiscore.WetGate.SndR = (vx & 0x100) ? -1 : 0;
			thiscore.WetGate.SndL = (vx & 0x200) ? -1 : 0;
			thiscore.DryGate.SndR = (vx & 0x400) ? -1 : 0;
			thiscore.DryGate.SndL = (vx & 0x800) ? -1 : 0;
			thiscore.Regs.MMIX = value;
		}
		break;

		case (REG_S_KON + 2):
			StartVoices(core, ((u32)value) << 16);
			spu2regs[omem >> 1 | core * 0x200] = value;
			break;

		case REG_S_KON:
			StartVoices(core, ((u32)value));
			spu2regs[omem >> 1 | core * 0x200] = value;
			break;

		case (REG_S_KOFF + 2):
			StopVoices(core, ((u32)value) << 16);
			spu2regs[omem >> 1 | core * 0x200] = value;
			break;

		case REG_S_KOFF:
			StopVoices(core, ((u32)value));
			spu2regs[omem >> 1 | core * 0x200] = value;
			break;

		case REG_S_ENDX:
			thiscore.Regs.ENDX &= 0xff0000;
			break;

		case (REG_S_ENDX + 2):
			thiscore.Regs.ENDX &= 0xffff;
			break;

		// Reverb Start and End Address Writes!
		//  * These regs are only writable when Effects are *DISABLED* (FxEnable is false).
		//    Writes while enabled should be ignored.
		//    NOTE: Above is false by testing but there are references saying this, so for
		//    now we think that writing is allowed but the internal register doesn't reflect
		//    the value until effects area writing is disabled.
		//  * Yes, these are backwards from all the volumes -- the hiword comes FIRST (wtf!)
		//  * End position is a hiword only!  Loword is always ffff.
		//  * The Reverb buffer position resets on writes to StartA.  It probably resets
		//    on writes to End too.  Docs don't say, but they're for PSX, which couldn't
		//    change the end address anyway.
		//
		case REG_A_ESA:
			SetHiWord(thiscore.ExtEffectsStartA, value & 0xF);
			if (!thiscore.FxEnable)
			{
				thiscore.EffectsStartA = thiscore.ExtEffectsStartA;
				thiscore.ReverbX = 0;
				thiscore.RevBuffers.NeedsUpdated = true;
			}
			break;

		case (REG_A_ESA + 2):
			SetLoWord(thiscore.ExtEffectsStartA, value);
			if (!thiscore.FxEnable)
			{
				thiscore.EffectsStartA = thiscore.ExtEffectsStartA;
				thiscore.ReverbX = 0;
				thiscore.RevBuffers.NeedsUpdated = true;
			}
			break;

		case REG_A_EEA:
			thiscore.ExtEffectsEndA = ((u32)(value & 0xF) << 16) | 0xFFFF;
			if (!thiscore.FxEnable)
			{
				thiscore.EffectsEndA = thiscore.ExtEffectsEndA;
				thiscore.ReverbX = 0;
				thiscore.RevBuffers.NeedsUpdated = true;
			}
			break;

		case REG_S_ADMAS:
			if (MsgToConsole())
				ConLog("* SPU2: Core %d AutoDMAControl set to %d (at cycle %d)\n", core, value, Cycles);

			if (psxmode)
				ConLog("* SPU2: Writing to REG_S_ADMAS while in PSX mode! value: %x", value);
			// hack for ps1driver which writes -1 (and never turns the adma off after psxlogo).
			// adma isn't available in psx mode either
			if (value == 32767)
			{
				psxmode = true;
				//memset(_spu2mem, 0, 0x200000);
				Cores[1].FxEnable = 0;
				Cores[1].EffectsStartA = 0x7FFF8; // park core1 effect area in inaccessible mem
				Cores[1].EffectsEndA = 0x7FFFF;
				Cores[1].ExtEffectsStartA = 0x7FFF8;
				Cores[1].ExtEffectsEndA = 0x7FFFF;
				Cores[1].ReverbX = 0;
				Cores[1].RevBuffers.NeedsUpdated = true;
				Cores[0].ReverbX = 0;
				Cores[0].RevBuffers.NeedsUpdated = true;
				for (uint v = 0; v < 24; ++v)
				{
					Cores[1].Voices[v].Volume = V_VolumeSlideLR(0, 0); // V_VolumeSlideLR::Max;
					Cores[1].Voices[v].SCurrent = 28;

					Cores[1].Voices[v].ADSR.Value = 0;
					Cores[1].Voices[v].ADSR.Phase = 0;
					Cores[1].Voices[v].Pitch = 0x0;
					Cores[1].Voices[v].NextA = 0x6FFFF;
					Cores[1].Voices[v].StartA = 0x6FFFF;
					Cores[1].Voices[v].LoopStartA = 0x6FFFF;
					Cores[1].Voices[v].Modulated = 0;
				}
				return;
			}
			thiscore.AutoDMACtrl = value;
			if (!(value & 0x3) && thiscore.AdmaInProgress)
			{
				// Kill the current transfer so it doesn't continue
				thiscore.AdmaInProgress = 0;
				thiscore.InputDataLeft = 0;
				thiscore.DMAICounter = 0;
				thiscore.InputDataTransferred = 0;
			}
			break;

		default:
		{
			const int addr = omem | ((core == 1) ? 0x400 : 0);
			*(regtable[addr >> 1]) = value;
		}
		break;
	}
}

template <int CoreIdx, int addr>
static void RegWrite_CoreExt(u16 value)
{
	V_Core& thiscore = Cores[CoreIdx];
	const int core = CoreIdx;

	switch (addr)
	{
			// Master Volume Address Write!

		case REG_P_MVOLL:
		case REG_P_MVOLR:
		{
			V_VolumeSlide& thisvol = (addr == REG_P_MVOLL) ? thiscore.MasterVol.Left : thiscore.MasterVol.Right;

			if (value & 0x8000) // +Lin/-Lin/+Exp/-Exp
			{
				thisvol.Mode = (value & 0xF000) >> 12;
				thisvol.Increment = (value & 0x7F);
				//printf("slides Mode = %x, Increment = %x\n",thisvol.Mode,thisvol.Increment);
			}
			else
			{
				// Constant Volume mode (no slides or envelopes)
				// Volumes range from 0x3fff to 0x7fff, with 0x4000 serving as
				// the "sign" bit, so a simple bitwise extension will do the trick:

				thisvol.Value = GetVol32(value << 1);
				thisvol.Mode = 0;
				thisvol.Increment = 0;
			}
			thisvol.Reg_VOL = value;
		}
		break;

		case REG_P_EVOLL:
			thiscore.FxVol.Left = GetVol32(value);
			break;

		case REG_P_EVOLR:
			thiscore.FxVol.Right = GetVol32(value);
			break;

		case REG_P_AVOLL:
			thiscore.ExtVol.Left = GetVol32(value);
			break;

		case REG_P_AVOLR:
			thiscore.ExtVol.Right = GetVol32(value);
			break;

		case REG_P_BVOLL:
			thiscore.InpVol.Left = GetVol32(value);
			break;

		case REG_P_BVOLR:
			thiscore.InpVol.Right = GetVol32(value);
			break;

			// MVOLX has been confirmed to not be allowed to be written to, so cases have been added as a no-op.
			// Tokyo Xtreme Racer Zero triggers this code, caused left side volume to be reduced.

		case REG_P_MVOLXL:
		case REG_P_MVOLXR:
			break;

		default:
		{
			const int raddr = addr + ((core == 1) ? 0x28 : 0);
			*(regtable[raddr >> 1]) = value;
		}
		break;
	}
}


template <int core, int addr>
static void RegWrite_Reverb(u16 value)
{
	// Signal to the Reverb code that the effects buffers need to be re-aligned.
	// This is both simple, efficient, and safe, since we only want to re-align
	// buffers after both hi and lo words have been written.

	// Update: This may have been written when it wasn't yet known that games
	// have to disable the Reverb Engine to change settings.
	// As such we only need to update buffers and parameters when we see
	// the FxEnable bit go down, then high again. (rama)
	*(regtable[addr >> 1]) = value;
	//Cores[core].RevBuffers.NeedsUpdated = true; // See update above
}

template <int addr>
static void RegWrite_SPDIF(u16 value)
{
	*(regtable[addr >> 1]) = value;
	UpdateSpdifMode();
}

template <int addr>
static void RegWrite_Raw(u16 value)
{
	*(regtable[addr >> 1]) = value;
}

static void RegWrite_Null(u16 value)
{
}

// --------------------------------------------------------------------------------------
//  Macros for tbl_reg_writes
// --------------------------------------------------------------------------------------
#define VoiceParamsSet(core, voice)                                                 \
	RegWrite_VoiceParams<core, voice, 0>, RegWrite_VoiceParams<core, voice, 1>,     \
		RegWrite_VoiceParams<core, voice, 2>, RegWrite_VoiceParams<core, voice, 3>, \
		RegWrite_VoiceParams<core, voice, 4>, RegWrite_VoiceParams<core, voice, 5>, \
		RegWrite_VoiceParams<core, voice, 6>, RegWrite_VoiceParams<core, voice, 7>

#define VoiceParamsCore(core)                                                                                   \
	VoiceParamsSet(core, 0), VoiceParamsSet(core, 1), VoiceParamsSet(core, 2), VoiceParamsSet(core, 3),         \
		VoiceParamsSet(core, 4), VoiceParamsSet(core, 5), VoiceParamsSet(core, 6), VoiceParamsSet(core, 7),     \
		VoiceParamsSet(core, 8), VoiceParamsSet(core, 9), VoiceParamsSet(core, 10), VoiceParamsSet(core, 11),   \
		VoiceParamsSet(core, 12), VoiceParamsSet(core, 13), VoiceParamsSet(core, 14), VoiceParamsSet(core, 15), \
		VoiceParamsSet(core, 16), VoiceParamsSet(core, 17), VoiceParamsSet(core, 18), VoiceParamsSet(core, 19), \
		VoiceParamsSet(core, 20), VoiceParamsSet(core, 21), VoiceParamsSet(core, 22), VoiceParamsSet(core, 23)

#define VoiceAddrSet(core, voice)                                               \
	RegWrite_VoiceAddr<core, voice, 0>, RegWrite_VoiceAddr<core, voice, 1>,     \
		RegWrite_VoiceAddr<core, voice, 2>, RegWrite_VoiceAddr<core, voice, 3>, \
		RegWrite_VoiceAddr<core, voice, 4>, RegWrite_VoiceAddr<core, voice, 5>


#define CoreParamsPair(core, omem) \
	RegWrite_Core<core, omem>, RegWrite_Core<core, ((omem) + 2)>

#define ReverbPair(core, mem) \
	RegWrite_Reverb<core, mem>, RegWrite_Core<core, ((mem) + 2)>

#define REGRAW(addr) RegWrite_Raw<addr>

// --------------------------------------------------------------------------------------
//  tbl_reg_writes  - Register Write Function Invocation LUT
// --------------------------------------------------------------------------------------

typedef void RegWriteHandler(u16 value);
static RegWriteHandler* const tbl_reg_writes[0x401] =
	{
		VoiceParamsCore(0), // 0x000 -> 0x180
		CoreParamsPair(0, REG_S_PMON),
		CoreParamsPair(0, REG_S_NON),
		CoreParamsPair(0, REG_S_VMIXL),
		CoreParamsPair(0, REG_S_VMIXEL),
		CoreParamsPair(0, REG_S_VMIXR),
		CoreParamsPair(0, REG_S_VMIXER),

		RegWrite_Core<0, REG_P_MMIX>,
		RegWrite_Core<0, REG_C_ATTR>,

		CoreParamsPair(0, REG_A_IRQA),
		CoreParamsPair(0, REG_S_KON),
		CoreParamsPair(0, REG_S_KOFF),
		CoreParamsPair(0, REG_A_TSA),
		CoreParamsPair(0, REG__1AC),

		RegWrite_Core<0, REG_S_ADMAS>,
		REGRAW(0x1b2),

		REGRAW(0x1b4), REGRAW(0x1b6),
		REGRAW(0x1b8), REGRAW(0x1ba),
		REGRAW(0x1bc), REGRAW(0x1be),

		// 0x1c0!

		VoiceAddrSet(0, 0), VoiceAddrSet(0, 1), VoiceAddrSet(0, 2), VoiceAddrSet(0, 3), VoiceAddrSet(0, 4), VoiceAddrSet(0, 5),
		VoiceAddrSet(0, 6), VoiceAddrSet(0, 7), VoiceAddrSet(0, 8), VoiceAddrSet(0, 9), VoiceAddrSet(0, 10), VoiceAddrSet(0, 11),
		VoiceAddrSet(0, 12), VoiceAddrSet(0, 13), VoiceAddrSet(0, 14), VoiceAddrSet(0, 15), VoiceAddrSet(0, 16), VoiceAddrSet(0, 17),
		VoiceAddrSet(0, 18), VoiceAddrSet(0, 19), VoiceAddrSet(0, 20), VoiceAddrSet(0, 21), VoiceAddrSet(0, 22), VoiceAddrSet(0, 23),

		CoreParamsPair(0, REG_A_ESA),

		ReverbPair(0, R_APF1_SIZE),   //       0x02E4		// Feedback Source A
		ReverbPair(0, R_APF2_SIZE),   //       0x02E8		// Feedback Source B
		ReverbPair(0, R_SAME_L_DST),  //    0x02EC
		ReverbPair(0, R_SAME_R_DST),  //    0x02F0
		ReverbPair(0, R_COMB1_L_SRC), //     0x02F4
		ReverbPair(0, R_COMB1_R_SRC), //     0x02F8
		ReverbPair(0, R_COMB2_L_SRC), //     0x02FC
		ReverbPair(0, R_COMB2_R_SRC), //     0x0300
		ReverbPair(0, R_SAME_L_SRC),  //     0x0304
		ReverbPair(0, R_SAME_R_SRC),  //     0x0308
		ReverbPair(0, R_DIFF_L_DST),  //    0x030C
		ReverbPair(0, R_DIFF_R_DST),  //    0x0310
		ReverbPair(0, R_COMB3_L_SRC), //     0x0314
		ReverbPair(0, R_COMB3_R_SRC), //     0x0318
		ReverbPair(0, R_COMB4_L_SRC), //     0x031C
		ReverbPair(0, R_COMB4_R_SRC), //     0x0320
		ReverbPair(0, R_DIFF_L_SRC),  //     0x0324
		ReverbPair(0, R_DIFF_R_SRC),  //     0x0328
		ReverbPair(0, R_APF1_L_DST),  //    0x032C
		ReverbPair(0, R_APF1_R_DST),  //    0x0330
		ReverbPair(0, R_APF2_L_DST),  //    0x0334
		ReverbPair(0, R_APF2_R_DST),  //    0x0338

		RegWrite_Core<0, REG_A_EEA>, RegWrite_Null,

		CoreParamsPair(0, REG_S_ENDX), //       0x0340	// End Point passed flag
		RegWrite_Core<0, REG_P_STATX>, //      0x0344 	// Status register?

		//0x346 here
		REGRAW(0x346),
		REGRAW(0x348), REGRAW(0x34A), REGRAW(0x34C), REGRAW(0x34E),
		REGRAW(0x350), REGRAW(0x352), REGRAW(0x354), REGRAW(0x356),
		REGRAW(0x358), REGRAW(0x35A), REGRAW(0x35C), REGRAW(0x35E),
		REGRAW(0x360), REGRAW(0x362), REGRAW(0x364), REGRAW(0x366),
		REGRAW(0x368), REGRAW(0x36A), REGRAW(0x36C), REGRAW(0x36E),
		REGRAW(0x370), REGRAW(0x372), REGRAW(0x374), REGRAW(0x376),
		REGRAW(0x378), REGRAW(0x37A), REGRAW(0x37C), REGRAW(0x37E),
		REGRAW(0x380), REGRAW(0x382), REGRAW(0x384), REGRAW(0x386),
		REGRAW(0x388), REGRAW(0x38A), REGRAW(0x38C), REGRAW(0x38E),
		REGRAW(0x390), REGRAW(0x392), REGRAW(0x394), REGRAW(0x396),
		REGRAW(0x398), REGRAW(0x39A), REGRAW(0x39C), REGRAW(0x39E),
		REGRAW(0x3A0), REGRAW(0x3A2), REGRAW(0x3A4), REGRAW(0x3A6),
		REGRAW(0x3A8), REGRAW(0x3AA), REGRAW(0x3AC), REGRAW(0x3AE),
		REGRAW(0x3B0), REGRAW(0x3B2), REGRAW(0x3B4), REGRAW(0x3B6),
		REGRAW(0x3B8), REGRAW(0x3BA), REGRAW(0x3BC), REGRAW(0x3BE),
		REGRAW(0x3C0), REGRAW(0x3C2), REGRAW(0x3C4), REGRAW(0x3C6),
		REGRAW(0x3C8), REGRAW(0x3CA), REGRAW(0x3CC), REGRAW(0x3CE),
		REGRAW(0x3D0), REGRAW(0x3D2), REGRAW(0x3D4), REGRAW(0x3D6),
		REGRAW(0x3D8), REGRAW(0x3DA), REGRAW(0x3DC), REGRAW(0x3DE),
		REGRAW(0x3E0), REGRAW(0x3E2), REGRAW(0x3E4), REGRAW(0x3E6),
		REGRAW(0x3E8), REGRAW(0x3EA), REGRAW(0x3EC), REGRAW(0x3EE),
		REGRAW(0x3F0), REGRAW(0x3F2), REGRAW(0x3F4), REGRAW(0x3F6),
		REGRAW(0x3F8), REGRAW(0x3FA), REGRAW(0x3FC), REGRAW(0x3FE),

		// AND... we reached 0x400!
		// Last verse, same as the first:

		VoiceParamsCore(1), // 0x000 -> 0x180
		CoreParamsPair(1, REG_S_PMON),
		CoreParamsPair(1, REG_S_NON),
		CoreParamsPair(1, REG_S_VMIXL),
		CoreParamsPair(1, REG_S_VMIXEL),
		CoreParamsPair(1, REG_S_VMIXR),
		CoreParamsPair(1, REG_S_VMIXER),

		RegWrite_Core<1, REG_P_MMIX>,
		RegWrite_Core<1, REG_C_ATTR>,

		CoreParamsPair(1, REG_A_IRQA),
		CoreParamsPair(1, REG_S_KON),
		CoreParamsPair(1, REG_S_KOFF),
		CoreParamsPair(1, REG_A_TSA),
		CoreParamsPair(1, REG__1AC),

		RegWrite_Core<1, REG_S_ADMAS>,
		REGRAW(0x5b2),

		REGRAW(0x5b4), REGRAW(0x5b6),
		REGRAW(0x5b8), REGRAW(0x5ba),
		REGRAW(0x5bc), REGRAW(0x5be),

		// 0x1c0!

		VoiceAddrSet(1, 0), VoiceAddrSet(1, 1), VoiceAddrSet(1, 2), VoiceAddrSet(1, 3), VoiceAddrSet(1, 4), VoiceAddrSet(1, 5),
		VoiceAddrSet(1, 6), VoiceAddrSet(1, 7), VoiceAddrSet(1, 8), VoiceAddrSet(1, 9), VoiceAddrSet(1, 10), VoiceAddrSet(1, 11),
		VoiceAddrSet(1, 12), VoiceAddrSet(1, 13), VoiceAddrSet(1, 14), VoiceAddrSet(1, 15), VoiceAddrSet(1, 16), VoiceAddrSet(1, 17),
		VoiceAddrSet(1, 18), VoiceAddrSet(1, 19), VoiceAddrSet(1, 20), VoiceAddrSet(1, 21), VoiceAddrSet(1, 22), VoiceAddrSet(1, 23),

		CoreParamsPair(1, REG_A_ESA),

		ReverbPair(1, R_APF1_SIZE),   //       0x02E4		// Feedback Source A
		ReverbPair(1, R_APF2_SIZE),   //       0x02E8		// Feedback Source B
		ReverbPair(1, R_SAME_L_DST),  //    0x02EC
		ReverbPair(1, R_SAME_R_DST),  //    0x02F0
		ReverbPair(1, R_COMB1_L_SRC), //     0x02F4
		ReverbPair(1, R_COMB1_R_SRC), //     0x02F8
		ReverbPair(1, R_COMB2_L_SRC), //     0x02FC
		ReverbPair(1, R_COMB2_R_SRC), //     0x0300
		ReverbPair(1, R_SAME_L_SRC),  //     0x0304
		ReverbPair(1, R_SAME_R_SRC),  //     0x0308
		ReverbPair(1, R_DIFF_L_DST),  //    0x030C
		ReverbPair(1, R_DIFF_R_DST),  //    0x0310
		ReverbPair(1, R_COMB3_L_SRC), //     0x0314
		ReverbPair(1, R_COMB3_R_SRC), //     0x0318
		ReverbPair(1, R_COMB4_L_SRC), //     0x031C
		ReverbPair(1, R_COMB4_R_SRC), //     0x0320
		ReverbPair(1, R_DIFF_R_SRC),  //     0x0324
		ReverbPair(1, R_DIFF_L_SRC),  //     0x0328
		ReverbPair(1, R_APF1_L_DST),  //    0x032C
		ReverbPair(1, R_APF1_R_DST),  //    0x0330
		ReverbPair(1, R_APF2_L_DST),  //    0x0334
		ReverbPair(1, R_APF2_R_DST),  //    0x0338

		RegWrite_Core<1, REG_A_EEA>, RegWrite_Null,

		CoreParamsPair(1, REG_S_ENDX), //       0x0340	// End Point passed flag
		RegWrite_Core<1, REG_P_STATX>, //      0x0344 	// Status register?

		REGRAW(0x746),
		REGRAW(0x748), REGRAW(0x74A), REGRAW(0x74C), REGRAW(0x74E),
		REGRAW(0x750), REGRAW(0x752), REGRAW(0x754), REGRAW(0x756),
		REGRAW(0x758), REGRAW(0x75A), REGRAW(0x75C), REGRAW(0x75E),

		// ------ -------

		RegWrite_CoreExt<0, REG_P_MVOLL>,  //     0x0760		// Master Volume Left
		RegWrite_CoreExt<0, REG_P_MVOLR>,  //     0x0762		// Master Volume Right
		RegWrite_CoreExt<0, REG_P_EVOLL>,  //     0x0764		// Effect Volume Left
		RegWrite_CoreExt<0, REG_P_EVOLR>,  //     0x0766		// Effect Volume Right
		RegWrite_CoreExt<0, REG_P_AVOLL>,  //     0x0768		// Core External Input Volume Left  (Only Core 1)
		RegWrite_CoreExt<0, REG_P_AVOLR>,  //     0x076A		// Core External Input Volume Right (Only Core 1)
		RegWrite_CoreExt<0, REG_P_BVOLL>,  //     0x076C 		// Sound Data Volume Left
		RegWrite_CoreExt<0, REG_P_BVOLR>,  //     0x076E		// Sound Data Volume Right
		RegWrite_CoreExt<0, REG_P_MVOLXL>, //     0x0770		// Current Master Volume Left
		RegWrite_CoreExt<0, REG_P_MVOLXR>, //     0x0772		// Current Master Volume Right

		RegWrite_CoreExt<0, R_IIR_VOL>,   //     0x0774		//IIR alpha (% used)
		RegWrite_CoreExt<0, R_COMB1_VOL>, //     0x0776
		RegWrite_CoreExt<0, R_COMB2_VOL>, //     0x0778
		RegWrite_CoreExt<0, R_COMB3_VOL>, //     0x077A
		RegWrite_CoreExt<0, R_COMB4_VOL>, //     0x077C
		RegWrite_CoreExt<0, R_WALL_VOL>,  //     0x077E
		RegWrite_CoreExt<0, R_APF1_VOL>,  //     0x0780		//feedback alpha (% used)
		RegWrite_CoreExt<0, R_APF2_VOL>,  //     0x0782		//feedback
		RegWrite_CoreExt<0, R_IN_COEF_L>, //     0x0784
		RegWrite_CoreExt<0, R_IN_COEF_R>, //     0x0786

		// ------ -------

		RegWrite_CoreExt<1, REG_P_MVOLL>,  //     0x0788		// Master Volume Left
		RegWrite_CoreExt<1, REG_P_MVOLR>,  //     0x078A		// Master Volume Right
		RegWrite_CoreExt<1, REG_P_EVOLL>,  //     0x0764		// Effect Volume Left
		RegWrite_CoreExt<1, REG_P_EVOLR>,  //     0x0766		// Effect Volume Right
		RegWrite_CoreExt<1, REG_P_AVOLL>,  //     0x0768		// Core External Input Volume Left  (Only Core 1)
		RegWrite_CoreExt<1, REG_P_AVOLR>,  //     0x076A		// Core External Input Volume Right (Only Core 1)
		RegWrite_CoreExt<1, REG_P_BVOLL>,  //     0x076C		// Sound Data Volume Left
		RegWrite_CoreExt<1, REG_P_BVOLR>,  //     0x076E		// Sound Data Volume Right
		RegWrite_CoreExt<1, REG_P_MVOLXL>, //     0x0770		// Current Master Volume Left
		RegWrite_CoreExt<1, REG_P_MVOLXR>, //     0x0772		// Current Master Volume Right

		RegWrite_CoreExt<1, R_IIR_VOL>,   //     0x0774		//IIR alpha (% used)
		RegWrite_CoreExt<1, R_COMB1_VOL>, //     0x0776
		RegWrite_CoreExt<1, R_COMB2_VOL>, //     0x0778
		RegWrite_CoreExt<1, R_COMB3_VOL>, //     0x077A
		RegWrite_CoreExt<1, R_COMB4_VOL>, //     0x077C
		RegWrite_CoreExt<1, R_WALL_VOL>,  //     0x077E
		RegWrite_CoreExt<1, R_APF1_VOL>,  //     0x0780		//feedback alpha (% used)
		RegWrite_CoreExt<1, R_APF2_VOL>,  //     0x0782		//feedback
		RegWrite_CoreExt<1, R_IN_COEF_L>, //     0x0784
		RegWrite_CoreExt<1, R_IN_COEF_R>, //     0x0786

		REGRAW(0x7B0), REGRAW(0x7B2), REGRAW(0x7B4), REGRAW(0x7B6),
		REGRAW(0x7B8), REGRAW(0x7BA), REGRAW(0x7BC), REGRAW(0x7BE),

		//  SPDIF interface

		RegWrite_SPDIF<SPDIF_OUT>,     //    0x07C0		// SPDIF Out: OFF/'PCM'/Bitstream/Bypass
		RegWrite_SPDIF<SPDIF_IRQINFO>, //    0x07C2
		REGRAW(0x7C4),
		RegWrite_SPDIF<SPDIF_MODE>,  //    0x07C6
		RegWrite_SPDIF<SPDIF_MEDIA>, //    0x07C8		// SPDIF Media: 'CD'/DVD
		REGRAW(0x7CA),
		RegWrite_SPDIF<SPDIF_PROTECT>, //	 0x07CC		// SPDIF Copy Protection

		REGRAW(0x7CE),
		REGRAW(0x7D0), REGRAW(0x7D2), REGRAW(0x7D4), REGRAW(0x7D6),
		REGRAW(0x7D8), REGRAW(0x7DA), REGRAW(0x7DC), REGRAW(0x7DE),
		REGRAW(0x7E0), REGRAW(0x7E2), REGRAW(0x7E4), REGRAW(0x7E6),
		REGRAW(0x7E8), REGRAW(0x7EA), REGRAW(0x7EC), REGRAW(0x7EE),
		REGRAW(0x7F0), REGRAW(0x7F2), REGRAW(0x7F4), REGRAW(0x7F6),
		REGRAW(0x7F8), REGRAW(0x7FA), REGRAW(0x7FC), REGRAW(0x7FE),

		nullptr // should be at 0x400!  (we assert check it on startup)
};


void SPU2_FastWrite(u32 rmem, u16 value)
{
	tbl_reg_writes[(rmem & 0x7ff) / 2](value);
}


void StartVoices(int core, u32 value)
{
	// Optimization: Games like to write zero to the KeyOn reg a lot, so shortcut
	// this loop if value is zero.
	if (value == 0)
		return;

	ConLog("KeyOn Write %x\n", value);

	Cores[core].KeyOn |= value;
	Cores[core].Regs.ENDX &= ~value;

	for (u8 vc = 0; vc < V_Core::NumVoices; vc++)
	{
		if (!((value >> vc) & 1))
			continue;

		if ((Cycles - Cores[core].Voices[vc].PlayCycle) < 2)
		{
			ConLog("Attempt to start voice %d on core %d in less than 2T since last KeyOn\n", vc, core);
			continue;
		}

		Cores[core].Voices[vc].Start();

		if (IsDevBuild)
		{
			V_Voice& thisvc(Cores[core].Voices[vc]);

			if (MsgKeyOnOff())
				ConLog("* SPU2: KeyOn: C%dV%02d: SSA: %8x; M: %s%s%s%s; H: %04x; P: %04x V: %04x/%04x; ADSR: %04x%04x\n",
					   core, vc, thisvc.StartA,
					   (Cores[core].VoiceGates[vc].DryL) ? "+" : "-", (Cores[core].VoiceGates[vc].DryR) ? "+" : "-",
					   (Cores[core].VoiceGates[vc].WetL) ? "+" : "-", (Cores[core].VoiceGates[vc].WetR) ? "+" : "-",
					   *(u16*)GetMemPtr(thisvc.StartA),
					   thisvc.Pitch,
					   thisvc.Volume.Left.Value >> 16, thisvc.Volume.Right.Value >> 16,
					   thisvc.ADSR.regADSR1, thisvc.ADSR.regADSR2);
		}
	}
}

void StopVoices(int core, u32 value)
{
	if (value == 0)
		return;

	ConLog("KeyOff Write %x\n", value);

	for (u8 vc = 0; vc < V_Core::NumVoices; vc++)
	{
		if (!((value >> vc) & 1))
			continue;

		if (Cycles - Cores[core].Voices[vc].PlayCycle < 2)
		{
			ConLog("Attempt to stop voice %d on core %d in less than 2T since KeyOn\n", vc, core);
			continue;
		}

		Cores[core].Voices[vc].ADSR.Releasing = true;
		if (MsgKeyOnOff())
			ConLog("* SPU2: KeyOff: Core %d; Voice %d.\n", core, vc);
	}
}
