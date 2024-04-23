// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Host/AudioStream.h"
#include "SPU2/Debug.h"
#include "SPU2/defs.h"
#include "SPU2/spu2.h"
#include "SPU2/interpolate_table.h"

#include "common/Assertions.h"

static const s32 tbl_XA_Factor[16][2] =
	{
		{0, 0},
		{60, 0},
		{115, -52},
		{98, -55},
		{122, -60}};

static void __forceinline XA_decode_block(s16* buffer, const s16* block, s32& prev1, s32& prev2)
{
	const s32 header = *block;
	const s32 shift = (header & 0xF) + 16;
	const int id = header >> 4 & 0xF;
	if (id > 4 && SPU2::MsgToConsole())
		SPU2::ConLog("* SPU2: Unknown ADPCM coefficients table id %d\n", id);
	const s32 pred1 = tbl_XA_Factor[id][0];
	const s32 pred2 = tbl_XA_Factor[id][1];

	const s8* blockbytes = (s8*)&block[1];
	const s8* blockend = &blockbytes[13];

	for (; blockbytes <= blockend; ++blockbytes)
	{
		s32 data = ((*blockbytes) << 28) & 0xF0000000;
		s32 pcm = (data >> shift) + (((pred1 * prev1) + (pred2 * prev2) + 32) >> 6);

		pcm = std::clamp<s32>(pcm, -0x8000, 0x7fff);
		*(buffer++) = pcm;

		data = ((*blockbytes) << 24) & 0xF0000000;
		s32 pcm2 = (data >> shift) + (((pred1 * pcm) + (pred2 * prev1) + 32) >> 6);

		pcm2 = std::clamp<s32>(pcm2, -0x8000, 0x7fff);
		*(buffer++) = pcm2;

		prev2 = pcm;
		prev1 = pcm2;
	}
}

static void __forceinline IncrementNextA(V_Core& thiscore, uint voiceidx)
{
	V_Voice& vc(thiscore.Voices[voiceidx]);

	// Important!  Both cores signal IRQ when an address is read, regardless of
	// which core actually reads the address.

	for (int i = 0; i < 2; i++)
	{
		if (Cores[i].IRQEnable && (vc.NextA == Cores[i].IRQA))
		{
			//if( IsDevBuild )
			//	ConLog(" * SPU2 Core %d: IRQ Requested (IRQA (%05X) passed; voice %d).\n", i, Cores[i].IRQA, thiscore.Index * 24 + voiceidx);

			SetIrqCall(i);
		}
	}

	vc.NextA++;
	vc.NextA &= 0xFFFFF;
}

// decoded pcm data, used to cache the decoded data so that it needn't be decoded
// multiple times.  Cache chunks are decoded when the mixer requests the blocks, and
// invalided when DMA transfers and memory writes are performed.
PcmCacheEntry pcm_cache_data[pcm_BlockCount];

int g_counter_cache_hits = 0;
int g_counter_cache_misses = 0;
int g_counter_cache_ignores = 0;

// LOOP/END sets the ENDX bit and sets NAX to LSA, and the voice is muted if LOOP is not set
// LOOP seems to only have any effect on the block with LOOP/END set, where it prevents muting the voice
// (the documented requirement that every block in a loop has the LOOP bit set is nonsense according to tests)
// LOOP/START sets LSA to NAX unless LSA was written manually since sound generation started
// (see LoopMode, the method by which this is achieved on the real SPU2 is unknown)
#define XAFLAG_LOOP_END (1ul << 0)
#define XAFLAG_LOOP (1ul << 1)
#define XAFLAG_LOOP_START (1ul << 2)

static __forceinline s32 GetNextDataBuffered(V_Core& thiscore, uint voiceidx)
{
	V_Voice& vc(thiscore.Voices[voiceidx]);

	if ((vc.SCurrent & 3) == 0)
	{
		if (vc.PendingLoopStart)
		{
			if ((Cycles - vc.PlayCycle) >= 4)
			{
				if (vc.LoopCycle < vc.PlayCycle)
				{
					vc.LoopStartA = vc.PendingLoopStartA;
					if (SPU2::MsgToConsole())
						SPU2::ConLog("Core %d Voice %d Loop Written by HW within 4T of Key On, Now Applying\n", thiscore.Index, voiceidx);
					vc.LoopMode = 1;
				}
				else
				{
					if (SPU2::MsgToConsole())
						SPU2::ConLog("Loop point from waveform set within 4T's, ignoring HW write\n");
				}

				vc.PendingLoopStart = false;
			}
		}
		IncrementNextA(thiscore, voiceidx);

		if ((vc.NextA & 7) == 0) // vc.SCurrent == 24 equivalent
		{
			if (vc.LoopFlags & XAFLAG_LOOP_END)
			{
				thiscore.Regs.ENDX |= (1 << voiceidx);
				vc.NextA = vc.LoopStartA | 1;
				if (!(vc.LoopFlags & XAFLAG_LOOP))
				{
					vc.Stop();

					if (IsDevBuild)
					{
						if (SPU2::MsgVoiceOff())
							SPU2::ConLog("* SPU2: Voice Off by EndPoint: %d \n", voiceidx);
					}
				}
			}
			else
				vc.NextA++; // no, don't IncrementNextA here.  We haven't read the header yet.
		}
	}

	if (vc.SCurrent == 28)
	{
		vc.SCurrent = 0;

		// We'll need the loop flags and buffer pointers regardless of cache status:

		for (int i = 0; i < 2; i++)
			if (Cores[i].IRQEnable && Cores[i].IRQA == (vc.NextA & 0xFFFF8))
				SetIrqCall(i);

		s16* memptr = GetMemPtr(vc.NextA & 0xFFFF8);
		vc.LoopFlags = *memptr >> 8; // grab loop flags from the upper byte.

		if ((vc.LoopFlags & XAFLAG_LOOP_START) && !vc.LoopMode)
		{
			vc.LoopStartA = vc.NextA & 0xFFFF8;
			vc.LoopCycle = Cycles;
		}

		const int cacheIdx = vc.NextA / pcm_WordsPerBlock;
		PcmCacheEntry& cacheLine = pcm_cache_data[cacheIdx];
		vc.SBuffer = cacheLine.Sampledata;

		if (cacheLine.Validated && vc.Prev1 == cacheLine.Prev1 && vc.Prev2 == cacheLine.Prev2)
		{
			// Cached block!  Read from the cache directly.
			// Make sure to propagate the prev1/prev2 ADPCM:

			vc.Prev1 = vc.SBuffer[27];
			vc.Prev2 = vc.SBuffer[26];

			//ConLog( "* SPU2: Cache Hit! NextA=0x%x, cacheIdx=0x%x\n", vc.NextA, cacheIdx );

			if (IsDevBuild)
				g_counter_cache_hits++;
		}
		else
		{
			// Only flag the cache if it's a non-dynamic memory range.
			if (vc.NextA >= SPU2_DYN_MEMLINE)
			{
				cacheLine.Validated = true;
				cacheLine.Prev1 = vc.Prev1;
				cacheLine.Prev2 = vc.Prev2;
			}

			if (IsDevBuild)
			{
				if (vc.NextA < SPU2_DYN_MEMLINE)
					g_counter_cache_ignores++;
				else
					g_counter_cache_misses++;
			}

			XA_decode_block(vc.SBuffer, memptr, vc.Prev1, vc.Prev2);
		}
	}

	return vc.SBuffer[vc.SCurrent++];
}

static __forceinline void GetNextDataDummy(V_Core& thiscore, uint voiceidx)
{
	V_Voice& vc(thiscore.Voices[voiceidx]);

	IncrementNextA(thiscore, voiceidx);

	if ((vc.NextA & 7) == 0) // vc.SCurrent == 24 equivalent
	{
		if (vc.LoopFlags & XAFLAG_LOOP_END)
		{
			thiscore.Regs.ENDX |= (1 << voiceidx);
			vc.NextA = vc.LoopStartA | 1;
		}
		else
			vc.NextA++; // no, don't IncrementNextA here.  We haven't read the header yet.
	}

	if (vc.SCurrent == 28)
	{
		for (int i = 0; i < 2; i++)
			if (Cores[i].IRQEnable && Cores[i].IRQA == (vc.NextA & 0xFFFF8))
				SetIrqCall(i);

		vc.LoopFlags = *GetMemPtr(vc.NextA & 0xFFFF8) >> 8; // grab loop flags from the upper byte.

		if ((vc.LoopFlags & XAFLAG_LOOP_START) && !vc.LoopMode)
			vc.LoopStartA = vc.NextA & 0xFFFF8;

		vc.SCurrent = 0;
	}

	vc.SP -= 0x1000 * (4 - (vc.SCurrent & 3));
	vc.SCurrent += 4 - (vc.SCurrent & 3);
}

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
//                                                                                     //

static __forceinline s32 ApplyVolume(s32 data, s32 volume)
{
	return (volume * data) >> 15;
}

static __forceinline StereoOut32 ApplyVolume(const StereoOut32& data, const V_VolumeLR& volume)
{
	return StereoOut32(
		ApplyVolume(data.Left, volume.Left),
		ApplyVolume(data.Right, volume.Right));
}

static __forceinline StereoOut32 ApplyVolume(const StereoOut32& data, const V_VolumeSlideLR& volume)
{
	return StereoOut32(
		ApplyVolume(data.Left, volume.Left.Value),
		ApplyVolume(data.Right, volume.Right.Value));
}

static void __forceinline UpdatePitch(uint coreidx, uint voiceidx)
{
	V_Voice& vc(Cores[coreidx].Voices[voiceidx]);
	s32 pitch;

	// [Air] : re-ordered comparisons: Modulated is much more likely to be zero than voice,
	//   and so the way it was before it's have to check both voice and modulated values
	//   most of the time.  Now it'll just check Modulated and short-circuit past the voice
	//   check (not that it amounts to much, but eh every little bit helps).
	if ((vc.Modulated == 0) || (voiceidx == 0))
		pitch = vc.Pitch;
	else
		pitch = std::clamp((vc.Pitch * (32768 + Cores[coreidx].Voices[voiceidx - 1].OutX)) >> 15, 0, 0x3fff);

	pitch = std::min(pitch, 0x3FFF);
	vc.SP += pitch;
}

static __forceinline void CalculateADSR(V_Core& thiscore, uint voiceidx)
{
	V_Voice& vc(thiscore.Voices[voiceidx]);

	if (vc.ADSR.Phase == V_ADSR::PHASE_STOPPED)
	{
		vc.ADSR.Value = 0;
		return;
	}

	if (!vc.ADSR.Calculate(thiscore.Index | (voiceidx << 1)))
	{
		if (IsDevBuild)
		{
			if (SPU2::MsgVoiceOff())
				SPU2::ConLog("* SPU2: Voice Off by ADSR: %d \n", voiceidx);
		}
		vc.Stop();
	}

	pxAssume(vc.ADSR.Value >= 0); // ADSR should never be negative...
}

__forceinline static s32 GaussianInterpolate(s32 pv4, s32 pv3, s32 pv2, s32 pv1, s32 i)
{
	s32 out = 0;
	out =  (interpTable[i][0] * pv4) >> 15;
	out += (interpTable[i][1] * pv3) >> 15;
	out += (interpTable[i][2] * pv2) >> 15;
	out += (interpTable[i][3] * pv1) >> 15;

	return out;
}

static __forceinline s32 GetVoiceValues(V_Core& thiscore, uint voiceidx)
{
	V_Voice& vc(thiscore.Voices[voiceidx]);

	while (vc.SP >= 0)
	{
		vc.PV4 = vc.PV3;
		vc.PV3 = vc.PV2;
		vc.PV2 = vc.PV1;
		vc.PV1 = GetNextDataBuffered(thiscore, voiceidx);
		vc.SP -= 0x1000;
	}

	const s32 mu = vc.SP + 0x1000;

	return GaussianInterpolate(vc.PV4, vc.PV3, vc.PV2, vc.PV1, (mu & 0x0ff0) >> 4);
}

// This is Dr. Hell's noise algorithm as implemented in pcsxr
// Supposedly this is 100% accurate
static __forceinline void UpdateNoise(V_Core& thiscore)
{
	static const uint8_t noise_add[64] = {
		1, 0, 0, 1, 0, 1, 1, 0,
		1, 0, 0, 1, 0, 1, 1, 0,
		1, 0, 0, 1, 0, 1, 1, 0,
		1, 0, 0, 1, 0, 1, 1, 0,
		0, 1, 1, 0, 1, 0, 0, 1,
		0, 1, 1, 0, 1, 0, 0, 1,
		0, 1, 1, 0, 1, 0, 0, 1,
		0, 1, 1, 0, 1, 0, 0, 1};

	static const uint16_t noise_freq_add[5] = {
		0, 84, 140, 180, 210};


	u32 level = 0x8000 >> (thiscore.NoiseClk >> 2);
	level <<= 16;

	thiscore.NoiseCnt += 0x10000;

	thiscore.NoiseCnt += noise_freq_add[thiscore.NoiseClk & 3];
	if ((thiscore.NoiseCnt & 0xffff) >= noise_freq_add[4])
	{
		thiscore.NoiseCnt += 0x10000;
		thiscore.NoiseCnt -= noise_freq_add[thiscore.NoiseClk & 3];
	}

	if (thiscore.NoiseCnt >= level)
	{
		while (thiscore.NoiseCnt >= level)
			thiscore.NoiseCnt -= level;

		thiscore.NoiseOut = (thiscore.NoiseOut << 1) | noise_add[(thiscore.NoiseOut >> 10) & 63];
	}
}

static __forceinline s32 GetNoiseValues(V_Core& thiscore)
{
	return (s16)thiscore.NoiseOut;
}

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
//                                                                                     //

// writes a signed value to the SPU2 ram
// Performs no cache invalidation -- use only for dynamic memory ranges
// of the SPU2 (between 0x0000 and SPU2_DYN_MEMLINE)
static __forceinline void spu2M_WriteFast(u32 addr, s16 value)
{
	// Fixes some of the oldest hangs in pcsx2's history! :p
	for (int i = 0; i < 2; i++)
	{
		if (Cores[i].IRQEnable && Cores[i].IRQA == addr)
		{
			//printf("Core %d special write IRQ Called (IRQ passed). IRQA = %x\n",i,addr);
			SetIrqCall(i);
		}
	}
// throw an assertion if the memory range is invalid:
#ifndef DEBUG_FAST
	pxAssume(addr < SPU2_DYN_MEMLINE);
#endif
	*GetMemPtr(addr) = value;
}


static __forceinline StereoOut32 MixVoice(uint coreidx, uint voiceidx)
{
	V_Core& thiscore(Cores[coreidx]);
	V_Voice& vc(thiscore.Voices[voiceidx]);

	// If this assertion fails, it mans SCurrent is being corrupted somewhere, or is not initialized
	// properly.  Invalid values in SCurrent will cause errant IRQs and corrupted audio.
	pxAssertMsg((vc.SCurrent <= 28) && (vc.SCurrent != 0), "Current sample should always range from 1->28");

	// Most games don't use much volume slide effects.  So only call the UpdateVolume
	// methods when needed by checking the flag outside the method here...
	// (Note: Ys 6 : Ark of Nephistm uses these effects)

	vc.Volume.Update();

	// SPU2 Note: The spu2 continues to process voices for eternity, always, so we
	// have to run through all the motions of updating the voice regardless of it's
	// audible status.  Otherwise IRQs might not trigger and emulation might fail.

	UpdatePitch(coreidx, voiceidx);

	StereoOut32 voiceOut(0, 0);
	s32 Value = 0;

	if (vc.ADSR.Phase > V_ADSR::PHASE_STOPPED)
	{
		if (vc.Noise)
			Value = GetNoiseValues(thiscore);
		else
			Value = GetVoiceValues(thiscore, voiceidx);

		// Update and Apply ADSR  (applies to normal and noise sources)

		CalculateADSR(thiscore, voiceidx);
		Value = ApplyVolume(Value, vc.ADSR.Value);
		vc.OutX = Value;

		if (IsDevBuild)
			DebugCores[coreidx].Voices[voiceidx].displayPeak = std::max(DebugCores[coreidx].Voices[voiceidx].displayPeak, (s32)vc.OutX);

		voiceOut = ApplyVolume(StereoOut32(Value, Value), vc.Volume);
	}
	else
	{
		while (vc.SP >= 0)
			GetNextDataDummy(thiscore, voiceidx); // Dummy is enough
	}

	// Write-back of raw voice data (post ADSR applied)
	if (voiceidx == 1)
		spu2M_WriteFast(((0 == coreidx) ? 0x400 : 0xc00) + OutPos, Value);
	else if (voiceidx == 3)
		spu2M_WriteFast(((0 == coreidx) ? 0x600 : 0xe00) + OutPos, Value);

	return voiceOut;
}

const VoiceMixSet VoiceMixSet::Empty((StereoOut32()), (StereoOut32())); // Don't use SteroOut32::Empty because C++ doesn't make any dep/order checks on global initializers.

static __forceinline void MixCoreVoices(VoiceMixSet& dest, const uint coreidx)
{
	V_Core& thiscore(Cores[coreidx]);

	for (uint voiceidx = 0; voiceidx < V_Core::NumVoices; ++voiceidx)
	{
		StereoOut32 VVal(MixVoice(coreidx, voiceidx));

		// Note: Results from MixVoice are ranged at 16 bits.

		dest.Dry.Left += VVal.Left & thiscore.VoiceGates[voiceidx].DryL;
		dest.Dry.Right += VVal.Right & thiscore.VoiceGates[voiceidx].DryR;
		dest.Wet.Left += VVal.Left & thiscore.VoiceGates[voiceidx].WetL;
		dest.Wet.Right += VVal.Right & thiscore.VoiceGates[voiceidx].WetR;
	}
}

StereoOut32 V_Core::Mix(const VoiceMixSet& inVoices, const StereoOut32& Input, const StereoOut32& Ext)
{
	MasterVol.Update();
	UpdateNoise(*this);

	// Saturate final result to standard 16 bit range.
	const VoiceMixSet Voices(clamp_mix(inVoices.Dry), clamp_mix(inVoices.Wet));

	// Write Mixed results To Output Area
	spu2M_WriteFast(((0 == Index) ? 0x1000 : 0x1800) + OutPos, Voices.Dry.Left);
	spu2M_WriteFast(((0 == Index) ? 0x1200 : 0x1A00) + OutPos, Voices.Dry.Right);
	spu2M_WriteFast(((0 == Index) ? 0x1400 : 0x1C00) + OutPos, Voices.Wet.Left);
	spu2M_WriteFast(((0 == Index) ? 0x1600 : 0x1E00) + OutPos, Voices.Wet.Right);

	// Write mixed results to logfile (if enabled)

#ifdef PCSX2_DEVBUILD
	WaveDump::WriteCore(Index, CoreSrc_DryVoiceMix, Voices.Dry);
	WaveDump::WriteCore(Index, CoreSrc_WetVoiceMix, Voices.Wet);
#endif

	// Mix in the Input data

	StereoOut32 TD(
		Input.Left & DryGate.InpL,
		Input.Right & DryGate.InpR);

	// Mix in the Voice data
	TD.Left += Voices.Dry.Left & DryGate.SndL;
	TD.Right += Voices.Dry.Right & DryGate.SndR;

	// Mix in the External (nothing/core0) data
	TD.Left += Ext.Left & DryGate.ExtL;
	TD.Right += Ext.Right & DryGate.ExtR;

	// ----------------------------------------------------------------------------
	//    Reverberation Effects Processing
	// ----------------------------------------------------------------------------
	// SPU2 has an FxEnable bit which seems to disable all reverb processing *and*
	// output, but does *not* disable the advancing buffers.  IRQs are not triggered
	// and reverb is rendered silent.
	//
	// Technically we should advance the buffers even when fx are disabled.  However
	// there are two things that make this very unlikely to matter:
	//
	//  1. Any SPU2 app wanting to avoid noise or pops needs to clear the reverb buffers
	//     when adjusting settings anyway; so the read/write positions in the reverb
	//     buffer after FxEnabled is set back to 1 doesn't really matter.
	//
	//  2. Writes to ESA (and possibly EEA) reset the buffer pointers to 0.
	//
	// On the other hand, updating the buffer is cheap and easy, so might as well. ;)

	StereoOut32 TW;

	// Mix Input, Voice, and External data:

	TW.Left = Input.Left & WetGate.InpL;
	TW.Right = Input.Right & WetGate.InpR;

	TW.Left += Voices.Wet.Left & WetGate.SndL;
	TW.Right += Voices.Wet.Right & WetGate.SndR;
	TW.Left += Ext.Left & WetGate.ExtL;
	TW.Right += Ext.Right & WetGate.ExtR;

#ifdef PCSX2_DEVBUILD
	WaveDump::WriteCore(Index, CoreSrc_PreReverb, TW);
#endif

	StereoOut32 RV = DoReverb(TW);

#ifdef PCSX2_DEVBUILD
	WaveDump::WriteCore(Index, CoreSrc_PostReverb, RV);
#endif

	// Mix Dry + Wet
	// (master volume is applied later to the result of both outputs added together).
	return TD + ApplyVolume(RV, FxVol);
}

static StereoOut32 DCFilter(StereoOut32 input) {
	// A simple DC blocking high-pass filter
	// Implementation from http://peabody.sapp.org/class/dmp2/lab/dcblock/
	// The magic number 0x7f5c is ceil(INT16_MAX * 0.995)
	StereoOut32 output;
	output.Left = (input.Left - DCFilterIn.Left + clamp_mix((0x7f5c * DCFilterOut.Left) >> 15));
	output.Right = (input.Right - DCFilterIn.Right + clamp_mix((0x7f5c * DCFilterOut.Right) >> 15));

	DCFilterIn = input;
	DCFilterOut = output;
	return output;
}

__forceinline void spu2Mix()
{
	// Note: Playmode 4 is SPDIF, which overrides other inputs.
	StereoOut32 InputData[2] =
		{
			// SPDIF is on Core 0:
			// Fixme:
			// 1. We do not have an AC3 decoder for the bitstream.
			// 2. Games usually provide a normal ADMA stream as well and want to see it getting read!
			/*(PlayMode&4) ? StereoOut32::Empty : */ ApplyVolume(Cores[0].ReadInput(), Cores[0].InpVol),

			// CDDA is on Core 1:
			(PlayMode & 8) ? StereoOut32::Empty : ApplyVolume(Cores[1].ReadInput(), Cores[1].InpVol)};

#ifdef PCSX2_DEVBUILD
	WaveDump::WriteCore(0, CoreSrc_Input, InputData[0]);
	WaveDump::WriteCore(1, CoreSrc_Input, InputData[1]);
#endif

	// Todo: Replace me with memzero initializer!
	VoiceMixSet VoiceData[2] = {VoiceMixSet::Empty, VoiceMixSet::Empty}; // mixed voice data for each core.
	MixCoreVoices(VoiceData[0], 0);
	MixCoreVoices(VoiceData[1], 1);

	StereoOut32 Ext(Cores[0].Mix(VoiceData[0], InputData[0], StereoOut32::Empty));

	if ((PlayMode & 4) || (Cores[0].Mute != 0))
		Ext = StereoOut32::Empty;
	else
	{
		Ext = ApplyVolume(clamp_mix(Ext), Cores[0].MasterVol);
	}

	// Commit Core 0 output to ram before mixing Core 1:
	spu2M_WriteFast(0x800 + OutPos, Ext.Left);
	spu2M_WriteFast(0xA00 + OutPos, Ext.Right);

#ifdef PCSX2_DEVBUILD
	WaveDump::WriteCore(0, CoreSrc_External, Ext);
#endif

	Ext = ApplyVolume(Ext, Cores[1].ExtVol);
	StereoOut32 Out(Cores[1].Mix(VoiceData[1], InputData[1], Ext));

	if (PlayMode & 8)
	{
		// Experimental CDDA support
		// The CDDA overrides all other mixer output.  It's a direct feed!

		Out = Cores[1].ReadInput_HiFi();
		//WaveLog::WriteCore( 1, "CDDA-32", OutL, OutR );
	}
	else
	{
		Out = ApplyVolume(clamp_mix(Out), Cores[1].MasterVol);
	}

	// For a long time PCSX2 has had its output volume halved by
	// an incorrect function for applying the master volume above.
	//
	// Adjust volume here so it matches what people have come to expect.
	Out = ApplyVolume(Out, {0x4fff, 0x4fff});
	Out = DCFilter(Out);

#ifdef PCSX2_DEVBUILD
	// Log final output to wavefile.
	WaveDump::WriteCore(1, CoreSrc_External, Out);
#endif

	spu2Output(Out);

	// Update AutoDMA output positioning
	OutPos++;
	if (OutPos >= 0x200)
		OutPos = 0;

	if constexpr (IsDevBuild)
	{
		// used to throttle the output rate of cache stat reports
		static int p_cachestat_counter = 0;

		p_cachestat_counter++;
		if (p_cachestat_counter > (48000 * 10))
		{
			p_cachestat_counter = 0;
			if (SPU2::MsgCache())
			{
				SPU2::ConLog(" * SPU2 > CacheStats > Hits: %d  Misses: %d  Ignores: %d\n",
					   g_counter_cache_hits,
					   g_counter_cache_misses,
					   g_counter_cache_ignores);
			}

			g_counter_cache_hits =
				g_counter_cache_misses =
					g_counter_cache_ignores = 0;
		}
	}
}
