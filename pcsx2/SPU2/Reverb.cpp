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

#include "PrecompiledHeader.h"
#include "Global.h"
#include <array>

__forceinline s32 V_Core::RevbGetIndexer(s32 offset)
{
	u32 pos = ReverbX + offset;

	// Fast and simple single step wrapping, made possible by the preparation of the
	// effects buffer addresses.

	if (pos > EffectsEndA)
	{
		pos -= EffectsEndA + 1;
		pos += EffectsStartA;
	}

	assert(pos >= EffectsStartA && pos <= EffectsEndA);
	return pos;
}

void V_Core::Reverb_AdvanceBuffer()
{
	if (RevBuffers.NeedsUpdated)
		UpdateEffectsBufferSize();

	if ((Cycles & 1) && (EffectsBufferSize > 0))
	{
		ReverbX += 1;
		if (ReverbX >= (u32)EffectsBufferSize)
			ReverbX = 0;
	}
}



static constexpr u32 NUM_TAPS = 39;
// 39 tap filter, the 0's could be optimized out
static constexpr std::array<s32, NUM_TAPS> filter_coefs = {
	-1,
	0,
	2,
	0,
	-10,
	0,
	35,
	0,
	-103,
	0,
	266,
	0,
	-616,
	0,
	1332,
	0,
	-2960,
	0,
	10246,
	16384,
	10246,
	0,
	-2960,
	0,
	1332,
	0,
	-616,
	0,
	266,
	0,
	-103,
	0,
	35,
	0,
	-10,
	0,
	2,
	0,
	-1,
};

s32 __forceinline V_Core::ReverbDownsample(bool right)
{
	s32 out = 0;

	// Skipping the 0 coefs.
	for (u32 i = 0; i < NUM_TAPS; i += 2)
	{
		out += RevbDownBuf[right][((RevbSampleBufPos - NUM_TAPS) + i) & 63] * filter_coefs[i];
	}

	// We also skipped the middle so add that in.
	out += RevbDownBuf[right][((RevbSampleBufPos - NUM_TAPS) + 19) & 63] * filter_coefs[19];

	out >>= 15;
	Clampify(out, (s32)INT16_MIN, (s32)INT16_MAX);

	return out;
}


StereoOut32 __forceinline V_Core::ReverbUpsample(bool phase)
{
	s32 ls = 0, rs = 0;

	if (phase)
	{
		ls += RevbUpBuf[0][(((RevbSampleBufPos - NUM_TAPS) >> 1) + 9) & 63] * filter_coefs[19];
		rs += RevbUpBuf[1][(((RevbSampleBufPos - NUM_TAPS) >> 1) + 9) & 63] * filter_coefs[19];
	}
	else
	{

		for (u32 i = 0; i < (NUM_TAPS >> 1) + 1; i++)
		{
			ls += RevbUpBuf[0][(((RevbSampleBufPos - NUM_TAPS) >> 1) + i) & 63] * filter_coefs[i * 2];
		}
		for (u32 i = 0; i < (NUM_TAPS >> 1) + 1; i++)
		{
			rs += RevbUpBuf[1][(((RevbSampleBufPos - NUM_TAPS) >> 1) + i) & 63] * filter_coefs[i * 2];
		}
	}

	ls >>= 14;
	Clampify(ls, (s32)INT16_MIN, (s32)INT16_MAX);
	rs >>= 14;
	Clampify(rs, (s32)INT16_MIN, (s32)INT16_MAX);

	return StereoOut32(ls, rs);
}

/////////////////////////////////////////////////////////////////////////////////////////

StereoOut32 V_Core::DoReverb(const StereoOut32& Input)
{
	if (EffectsBufferSize <= 0)
	{
		return StereoOut32::Empty;
	}

	RevbDownBuf[0][RevbSampleBufPos & 63] = Input.Left;
	RevbDownBuf[1][RevbSampleBufPos & 63] = Input.Right;

	bool R = Cycles & 1;

	// Calculate the read/write addresses we'll be needing for this session of reverb.

	const u32 same_src = RevbGetIndexer(R ? RevBuffers.SAME_R_SRC : RevBuffers.SAME_L_SRC);
	const u32 same_dst = RevbGetIndexer(R ? RevBuffers.SAME_R_DST : RevBuffers.SAME_L_DST);
	const u32 same_prv = RevbGetIndexer(R ? RevBuffers.SAME_R_PRV : RevBuffers.SAME_L_PRV);

	const u32 diff_src = RevbGetIndexer(R ? RevBuffers.DIFF_L_SRC : RevBuffers.DIFF_R_SRC);
	const u32 diff_dst = RevbGetIndexer(R ? RevBuffers.DIFF_R_DST : RevBuffers.DIFF_L_DST);
	const u32 diff_prv = RevbGetIndexer(R ? RevBuffers.DIFF_R_PRV : RevBuffers.DIFF_L_PRV);

	const u32 comb1_src = RevbGetIndexer(R ? RevBuffers.COMB1_R_SRC : RevBuffers.COMB1_L_SRC);
	const u32 comb2_src = RevbGetIndexer(R ? RevBuffers.COMB2_R_SRC : RevBuffers.COMB2_L_SRC);
	const u32 comb3_src = RevbGetIndexer(R ? RevBuffers.COMB3_R_SRC : RevBuffers.COMB3_L_SRC);
	const u32 comb4_src = RevbGetIndexer(R ? RevBuffers.COMB4_R_SRC : RevBuffers.COMB4_L_SRC);

	const u32 apf1_src = RevbGetIndexer(R ? RevBuffers.APF1_R_SRC : RevBuffers.APF1_L_SRC);
	const u32 apf1_dst = RevbGetIndexer(R ? RevBuffers.APF1_R_DST : RevBuffers.APF1_L_DST);
	const u32 apf2_src = RevbGetIndexer(R ? RevBuffers.APF2_R_SRC : RevBuffers.APF2_L_SRC);
	const u32 apf2_dst = RevbGetIndexer(R ? RevBuffers.APF2_R_DST : RevBuffers.APF2_L_DST);

	// -----------------------------------------
	//          Optimized IRQ Testing !
	// -----------------------------------------

	// This test is enhanced by using the reverb effects area begin/end test as a
	// shortcut, since all buffer addresses are within that area.  If the IRQA isn't
	// within that zone then the "bulk" of the test is skipped, so this should only
	// be a slowdown on a few evil games.

	for (int i = 0; i < 2; i++)
	{
		if (Cores[i].IRQEnable && ((Cores[i].IRQA >= EffectsStartA) && (Cores[i].IRQA <= EffectsEndA)))
		{
			if ((Cores[i].IRQA == same_src) || (Cores[i].IRQA == diff_src) ||
				(Cores[i].IRQA == same_dst) || (Cores[i].IRQA == diff_dst) ||
				(Cores[i].IRQA == same_prv) || (Cores[i].IRQA == diff_prv) ||

				(Cores[i].IRQA == comb1_src) || (Cores[i].IRQA == comb2_src) ||
				(Cores[i].IRQA == comb3_src) || (Cores[i].IRQA == comb4_src) ||

				(Cores[i].IRQA == apf1_dst) || (Cores[i].IRQA == apf1_src) ||
				(Cores[i].IRQA == apf2_dst) || (Cores[i].IRQA == apf2_src))
			{
				//printf("Core %d IRQ Called (Reverb). IRQA = %x\n",i,addr);
				SetIrqCall(i);
			}
		}
	}

	// Reverb algorithm pretty much directly ripped from http://drhell.web.fc2.com/ps1/
	// minus the 35 step FIR which just seems to break things.

	s32 in, same, diff, apf1, apf2, out;

#define MUL(x, y) ((x) * (y) >> 15)
	in = MUL(R ? Revb.IN_COEF_R : Revb.IN_COEF_L, ReverbDownsample(R));

	same = MUL(Revb.IIR_VOL, in + MUL(Revb.WALL_VOL, _spu2mem[same_src]) - _spu2mem[same_prv]) + _spu2mem[same_prv];
	diff = MUL(Revb.IIR_VOL, in + MUL(Revb.WALL_VOL, _spu2mem[diff_src]) - _spu2mem[diff_prv]) + _spu2mem[diff_prv];

	out = MUL(Revb.COMB1_VOL, _spu2mem[comb1_src]) + MUL(Revb.COMB2_VOL, _spu2mem[comb2_src]) + MUL(Revb.COMB3_VOL, _spu2mem[comb3_src]) + MUL(Revb.COMB4_VOL, _spu2mem[comb4_src]);

	apf1 = out - MUL(Revb.APF1_VOL, _spu2mem[apf1_src]);
	out = _spu2mem[apf1_src] + MUL(Revb.APF1_VOL, apf1);
	apf2 = out - MUL(Revb.APF2_VOL, _spu2mem[apf2_src]);
	out = _spu2mem[apf2_src] + MUL(Revb.APF2_VOL, apf2);

	// According to no$psx the effects always run but don't always write back, see check in V_Core::Mix
	if (FxEnable)
	{
		_spu2mem[same_dst] = clamp_mix(same);
		_spu2mem[diff_dst] = clamp_mix(diff);
		_spu2mem[apf1_dst] = clamp_mix(apf1);
		_spu2mem[apf2_dst] = clamp_mix(apf2);
	}

	RevbUpBuf[R][(RevbSampleBufPos >> 1) & 63] = clamp_mix(out);

	RevbSampleBufPos++;

	return ReverbUpsample(RevbSampleBufPos & 1);
}
