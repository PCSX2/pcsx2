// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "SPU2/Mixer.h"
#include "SPU2/SndOut.h"
#include "SPU2/Global.h"

#include "GS/MultiISA.h"

#include <array>

// --------------------------------------------------------------------------------------
//  SPU2 Register Table LUT
// --------------------------------------------------------------------------------------
extern const std::array<u16*, 0x401> regtable;

// --------------------------------------------------------------------------------------
//  SPU2 Memory Indexers
// --------------------------------------------------------------------------------------

#define spu2Rs16(mmem) (*(s16*)((s8*)spu2regs + ((mmem)&0x1fff)))
#define spu2Ru16(mmem) (*(u16*)((s8*)spu2regs + ((mmem)&0x1fff)))

extern s16* GetMemPtr(u32 addr);
extern s16 spu2M_Read(u32 addr);
extern void spu2M_Write(u32 addr, s16 value);
extern void spu2M_Write(u32 addr, u16 value);

static __forceinline s16 SignExtend16(u16 v)
{
	return (s16)v;
}

static __forceinline s32 clamp_mix(s32 x)
{
	return std::clamp(x, -0x8000, 0x7fff);
}

static __forceinline StereoOut32 clamp_mix(StereoOut32 sample)
{
	return StereoOut32(clamp_mix(sample.Left), clamp_mix(sample.Right));
}

struct V_VolumeLR
{
	static V_VolumeLR Max;

	s32 Left;
	s32 Right;

	V_VolumeLR() = default;
	V_VolumeLR(s32 both)
		: Left(both)
		, Right(both)
	{
	}

	void DebugDump(FILE* dump, const char* title);
};

struct V_VolumeSlide
{
	// Holds the "original" value of the volume for this voice, prior to slides.
	// (ie, the volume as written to the register)

	union
	{
		u16 Reg_VOL;
		struct
		{
			u16 Step : 2;
			u16 Shift : 5;
			u16 : 5;
			u16 Phase : 1;
			u16 Decr : 1;
			u16 Exp : 1;
			u16 Enable : 1;
		};
	};

	u32 Counter;
	s32 Value;

public:
	V_VolumeSlide() = default;
	V_VolumeSlide(s16 regval, s32 fullvol)
		: Reg_VOL(regval)
		, Value(fullvol)
	{
	}

	void Update();
	void RegSet(u16 src); // used to set the volume from a register source

#ifdef PCSX2_DEVBUILD
	void DebugDump(FILE* dump, const char* title, const char* nameLR);
#endif
};

struct V_VolumeSlideLR
{
	static V_VolumeSlideLR Max;

	V_VolumeSlide Left;
	V_VolumeSlide Right;

public:
	V_VolumeSlideLR() = default;
	V_VolumeSlideLR(s16 regval, s32 bothval)
		: Left(regval, bothval)
		, Right(regval, bothval)
	{
	}

	void Update()
	{
		Left.Update();
		Right.Update();
	}

#ifdef PCSX2_DEVBUILD
	void DebugDump(FILE* dump, const char* title);
#endif
};

struct V_ADSR
{
	union
	{
		u32 reg32;

		struct
		{
			u16 regADSR1;
			u16 regADSR2;
		};

		struct
		{
			u32 SustainLevel : 4;
			u32 DecayShift : 4;
			u32 AttackStep : 2;
			u32 AttackShift : 5;
			u32 AttackMode : 1;
			u32 ReleaseShift : 5;
			u32 ReleaseMode : 1;
			u32 SustainStep : 2;
			u32 SustainShift : 5;
			u32 : 1;
			u32 SustainDir : 1;
			u32 SustainMode : 1;
		};
	};

	static constexpr int ADSR_PHASES = 5;

	static constexpr int PHASE_STOPPED = 0;
	static constexpr int PHASE_ATTACK = 1;
	static constexpr int PHASE_DECAY = 2;
	static constexpr int PHASE_SUSTAIN = 3;
	static constexpr int PHASE_RELEASE = 4;

	struct CachedADSR
	{
		bool Decr;
		bool Exp;
		u8 Shift;
		s8 Step;
		s32 Target;
	};

	std::array<CachedADSR, ADSR_PHASES> CachedPhases;

	u32 Counter;
	s32 Value; // Ranges from 0 to 0x7fff (signed values are clamped to 0) [Reg_ENVX]
	u8 Phase; // monitors current phase of ADSR envelope

public:
	void UpdateCache();
	bool Calculate(int voiceidx);
	void Attack();
	void Release();
};


struct V_Voice
{
	u32 PlayCycle; // SPU2 cycle where the Playing started
	u32 LoopCycle; // SPU2 cycle where it last set its own Loop

	u32 PendingLoopStartA;
	bool PendingLoopStart;

	V_VolumeSlideLR Volume;

	// Envelope
	V_ADSR ADSR;
	// Pitch (also Reg_PITCH)
	u16 Pitch;
	// Loop Start address (also Reg_LSAH/L)
	u32 LoopStartA;
	// Sound Start address (also Reg_SSAH/L)
	u32 StartA;
	// Next Read Data address (also Reg_NAXH/L)
	u32 NextA;
	// Voice Decoding State
	s32 Prev1;
	s32 Prev2;

	// Pitch Modulated by previous voice
	bool Modulated;
	// Source (Wave/Noise)
	bool Noise;

	s8 LoopMode;
	s8 LoopFlags;

	// Sample pointer (19:12 bit fixed point)
	s32 SP;

	// Sample pointer for Cubic Interpolation
	// Cubic interpolation mixes a sample behind Linear, so that it
	// can have sample data to either side of the end points from which
	// to extrapolate.  This SP represents that late sample position.
	s32 SPc;

	// Previous sample values - used for interpolation
	// Inverted order of these members to match the access order in the
	//   code (might improve cache hits).
	s32 PV4;
	s32 PV3;
	s32 PV2;
	s32 PV1;

	// Last outputted audio value, used for voice modulation.
	s32 OutX;
	s32 NextCrest; // temp value for Crest calculation

	// SBuffer now points directly to an ADPCM cache entry.
	s16* SBuffer;

	// sample position within the current decoded packet.
	s32 SCurrent;

	// it takes a few ticks for voices to start on the real SPU2?
	void Start();
	void Stop();
};

// ** Begin Debug-only variables section **
// Separated from the V_Voice struct to improve cache performance of
// the Public Release build.
struct V_VoiceDebug
{
	s8 FirstBlock;
	s32 SampleData;
	s32 PeakX;
	s32 displayPeak;
	s32 lastSetStartA;
};

struct V_CoreDebug
{
	V_VoiceDebug Voices[24];
	// Last Transfer Size
	u32 lastsize;

	// draw adma waveform in the visual debugger
	s32 admaWaveformL[0x100];
	s32 admaWaveformR[0x100];

	// Enabled when a dma write starts, disabled when the visual debugger showed it once
	s32 dmaFlag;
};

// Debug tracking information - 24 voices and 2 cores.
extern V_CoreDebug DebugCores[2];

struct V_Reverb
{
	s16 IN_COEF_L;
	s16 IN_COEF_R;

	u32 APF1_SIZE;
	u32 APF2_SIZE;

	s16 APF1_VOL;
	s16 APF2_VOL;

	u32 SAME_L_SRC;
	u32 SAME_R_SRC;
	u32 DIFF_L_SRC;
	u32 DIFF_R_SRC;
	u32 SAME_L_DST;
	u32 SAME_R_DST;
	u32 DIFF_L_DST;
	u32 DIFF_R_DST;

	s16 IIR_VOL;
	s16 WALL_VOL;

	u32 COMB1_L_SRC;
	u32 COMB1_R_SRC;
	u32 COMB2_L_SRC;
	u32 COMB2_R_SRC;
	u32 COMB3_L_SRC;
	u32 COMB3_R_SRC;
	u32 COMB4_L_SRC;
	u32 COMB4_R_SRC;

	s16 COMB1_VOL;
	s16 COMB2_VOL;
	s16 COMB3_VOL;
	s16 COMB4_VOL;

	u32 APF1_L_DST;
	u32 APF1_R_DST;
	u32 APF2_L_DST;
	u32 APF2_R_DST;
};

struct V_SPDIF
{
	u16 Out;
	u16 Info;
	u16 Unknown1;
	u16 Mode;
	u16 Media;
	u16 Unknown2;
	u16 Protection;
};

struct V_CoreRegs
{
	u32 PMON;
	u32 NON;
	u32 VMIXL;
	u32 VMIXR;
	u32 VMIXEL;
	u32 VMIXER;
	u32 ENDX;

	u16 MMIX;
	u16 STATX;
	u16 ATTR;
	u16 _1AC;
};

struct V_VoiceGates
{
	s32 DryL; // 'AND Gate' for Direct Output to Left Channel
	s32 DryR; // 'AND Gate' for Direct Output for Right Channel
	s32 WetL; // 'AND Gate' for Effect Output for Left Channel
	s32 WetR; // 'AND Gate' for Effect Output for Right Channel
};

struct V_CoreGates
{
	s32 InpL; // Sound Data Input to Direct Output (Left)
	s32 InpR; // Sound Data Input to Direct Output (Right)
	s32 SndL; // Voice Data to Direct Output (Left)
	s32 SndR; // Voice Data to Direct Output (Right)
	s32 ExtL; // External Input to Direct Output (Left)
	s32 ExtR; // External Input to Direct Output (Right)
};

struct VoiceMixSet
{
	static const VoiceMixSet Empty;
	StereoOut32 Dry, Wet;

	VoiceMixSet() {}
	VoiceMixSet(const StereoOut32& dry, const StereoOut32& wet)
		: Dry(dry)
		, Wet(wet)
	{
	}
};

struct V_Core
{
	static const uint NumVoices = 24;

	u32 Index; // Core index identifier.

	// Voice Gates -- These are SSE-related values, and must always be
	// first to ensure 16 byte alignment

	V_VoiceGates VoiceGates[NumVoices];
	V_CoreGates DryGate;
	V_CoreGates WetGate;

	V_VolumeSlideLR MasterVol; // Master Volume
	V_VolumeLR ExtVol; // Volume for External Data Input
	V_VolumeLR InpVol; // Volume for Sound Data Input
	V_VolumeLR FxVol; // Volume for Output from Effects

	V_Voice Voices[NumVoices];

	u32 IRQA; // Interrupt Address
	u32 TSA; // DMA Transfer Start Address
	u32 ActiveTSA; // Active DMA TSA - Required for NFL 2k5 which overwrites it mid transfer

	bool IRQEnable; // Interrupt Enable
	bool FxEnable; // Effect Enable
	bool Mute; // Mute
	bool AdmaInProgress;

	s8 DMABits; // DMA related?
	u8 NoiseClk; // Noise Clock
	u32 NoiseCnt; // Noise Counter
	u32 NoiseOut; // Noise Output
	u16 AutoDMACtrl; // AutoDMA Status
	s32 DMAICounter; // DMA Interrupt Counter
	u32 LastClock; // DMA Interrupt Clock Cycle Counter
	u32 InputDataLeft; // Input Buffer
	u32 InputDataTransferred; // Used for simulating MADR increase (GTA VC)
	u32 InputPosWrite;
	u32 InputDataProgress;

	V_Reverb Revb; // Reverb Registers

	s16 RevbDownBuf[2][64 * 2]; // Downsample buffer for reverb, one for each channel
	s16 RevbUpBuf[2][64 * 2]; // Upsample buffer for reverb, one for each channel
	u32 RevbSampleBufPos;
	u32 EffectsStartA;
	u32 EffectsEndA;

	V_CoreRegs Regs; // Registers

	// Preserves the channel processed last cycle
	StereoOut32 LastEffect;

	u8 CoreEnabled;

	u8 AttrBit0;
	u8 DmaMode;

	// new dma only
	bool DmaStarted;
	u32 AutoDmaFree;

	// old dma only
	u16* DMAPtr;
	u16* DMARPtr; // Mem pointer for DMA Reads
	u32 ReadSize;
	bool IsDMARead;

	u32 KeyOn; // not the KON register (though maybe it is)

	// psxmode caches
	u16 psxSoundDataTransferControl;
	u16 psxSPUSTAT;

	// HACK -- This is a temp buffer which is (or isn't?) used to circumvent some memory
	// corruption that originates elsewhere. >_<  The actual ADMA buffer
	// is an area mapped to SPU2 main memory.
	//s16				ADMATempBuffer[0x1000];

	// ----------------------------------------------------------------------------------
	//  V_Core Methods
	// ----------------------------------------------------------------------------------

	// uninitialized constructor
	V_Core()
		: Index(-1)
		, DMAPtr(nullptr)
	{
	}
	V_Core(int idx) : Index(idx) {};

	void Init(int index);
	void UpdateEffectsBufferSize();
	void AnalyzeReverbPreset();

	void WriteRegPS1(u32 mem, u16 value);
	u16 ReadRegPS1(u32 mem);

	// --------------------------------------------------------------------------------------
	//  Mixer Section
	// --------------------------------------------------------------------------------------

	StereoOut32 Mix(const VoiceMixSet& inVoices, const StereoOut32& Input, const StereoOut32& Ext);
	StereoOut32 DoReverb(StereoOut32 Input);
	s32 RevbGetIndexer(s32 offset);

	StereoOut32 ReadInput();
	StereoOut32 ReadInput_HiFi();

	// --------------------------------------------------------------------------
	//  DMA Section
	// --------------------------------------------------------------------------

	// Returns the index of the DMA channel (4 for Core 0, or 7 for Core 1)
	int GetDmaIndex() const
	{
		return (Index == 0) ? 4 : 7;
	}

	// returns either '4' or '7'
	char GetDmaIndexChar() const
	{
		return 0x30 + GetDmaIndex();
	}

	__forceinline u16 DmaRead()
	{
		const u16 ret = static_cast<u16>(spu2M_Read(ActiveTSA));
		++ActiveTSA;
		ActiveTSA &= 0xfffff;
		TSA = ActiveTSA;
		return ret;
	}

	__forceinline void DmaWrite(u16 value)
	{
		spu2M_Write(ActiveTSA, value);
		++ActiveTSA;
		ActiveTSA &= 0xfffff;
		TSA = ActiveTSA;
	}

	void LogAutoDMA(FILE* fp);

	void DoDMAwrite(u16* pMem, u32 size);
	void DoDMAread(u16* pMem, u32 size);
	void FinishDMAread();

	void AutoDMAReadBuffer(int mode);
	void StartADMAWrite(u16* pMem, u32 sz);
	void PlainDMAWrite(u16* pMem, u32 sz);
	void FinishDMAwrite();
};

MULTI_ISA_DEF(
	StereoOut32 ReverbUpsample(V_Core& core);
	s32 ReverbDownsample(V_Core& core, bool right);
)

extern StereoOut32 (*ReverbUpsample)(V_Core& core);
extern s32 (*ReverbDownsample)(V_Core& core, bool right);

extern V_Core Cores[2];
extern V_SPDIF Spdif;

// Output Buffer Writing Position (the same for all data);
extern u16 OutPos;
// Input Buffer Reading Position (the same for all data);
extern u16 InputPos;
// SPU Mixing Cycles ("Ticks mixed" counter)
extern u32 Cycles;
// DC Filter state
extern StereoOut32 DCFilterIn, DCFilterOut;

extern s16 spu2regs[0x010000 / sizeof(s16)];
extern s16 _spu2mem[0x200000 / sizeof(s16)];
extern int PlayMode;

extern void SetIrqCall(int core);
extern void SetIrqCallDMA(int core);
extern void StartVoices(int core, u32 value);
extern void StopVoices(int core, u32 value);
extern void CalculateADSR(V_Voice& vc);
extern void UpdateSpdifMode();

namespace SPU2Savestate
{
	struct DataBlock;

	extern s32 FreezeIt(DataBlock& spud);
	extern s32 ThawIt(DataBlock& spud);
	extern s32 SizeIt();
} // namespace SPU2Savestate

// --------------------------------------------------------------------------------------
//  ADPCM Decoder Cache
// --------------------------------------------------------------------------------------
//  the cache data size is determined by taking the number of adpcm blocks
//  (2MB / 16) and multiplying it by the decoded block size (28 samples).
//  Thus: pcm_cache_data = 7,340,032 bytes (ouch!)
//  Expanded: 16 bytes expands to 56 bytes [3.5:1 ratio]
//    Resulting in 2MB * 3.5.

// The SPU2 has a dynamic memory range which is used for several internal operations, such as
// registers, CORE 1/2 mixing, AutoDMAs, and some other fancy stuff.  We exclude this range
// from the cache here:
static constexpr s32 SPU2_DYN_MEMLINE = 0x2800;

// 8 short words per encoded PCM block. (as stored in SPU2 ram)
static constexpr int pcm_WordsPerBlock = 8;

// number of cachable ADPCM blocks (any blocks above the SPU2_DYN_MEMLINE)
static constexpr int pcm_BlockCount = 0x100000 / pcm_WordsPerBlock;

// 28 samples per decoded PCM block (as stored in our cache)
static constexpr int pcm_DecodedSamplesPerBlock = 28;

struct PcmCacheEntry
{
	bool Validated;
	s16 Sampledata[pcm_DecodedSamplesPerBlock];
	s32 Prev1;
	s32 Prev2;
};

extern PcmCacheEntry pcm_cache_data[pcm_BlockCount];
