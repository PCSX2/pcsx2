// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "SPU2/defs.h"
#include "GS/GSVector.h"

#include "common/Console.h"

#include <array>

void V_Core::AnalyzeReverbPreset()
{
	Console.WriteLn("Reverb Parameter Update for Core %d:", Index);
	Console.WriteLn("----------------------------------------------------------");

	Console.WriteLn("    IN_COEF_L, IN_COEF_R       0x%08x, 0x%08x", Revb.IN_COEF_L, Revb.IN_COEF_R);
	Console.WriteLn("    APF1_SIZE, APF2_SIZE       0x%08x, 0x%08x", Revb.APF1_SIZE, Revb.APF2_SIZE);
	Console.WriteLn("    APF1_VOL, APF2_VOL         0x%08x, 0x%08x", Revb.APF1_VOL, Revb.APF2_VOL);

	Console.WriteLn("    COMB1_VOL                  0x%08x", Revb.COMB1_VOL);
	Console.WriteLn("    COMB2_VOL                  0x%08x", Revb.COMB2_VOL);
	Console.WriteLn("    COMB3_VOL                  0x%08x", Revb.COMB3_VOL);
	Console.WriteLn("    COMB4_VOL                  0x%08x", Revb.COMB4_VOL);

	Console.WriteLn("    COMB1_L_SRC, COMB1_R_SRC   0x%08x, 0x%08x", Revb.COMB1_L_SRC, Revb.COMB1_R_SRC);
	Console.WriteLn("    COMB2_L_SRC, COMB2_R_SRC   0x%08x, 0x%08x", Revb.COMB2_L_SRC, Revb.COMB2_R_SRC);
	Console.WriteLn("    COMB3_L_SRC, COMB3_R_SRC   0x%08x, 0x%08x", Revb.COMB3_L_SRC, Revb.COMB3_R_SRC);
	Console.WriteLn("    COMB4_L_SRC, COMB4_R_SRC   0x%08x, 0x%08x", Revb.COMB4_L_SRC, Revb.COMB4_R_SRC);

	Console.WriteLn("    SAME_L_SRC, SAME_R_SRC     0x%08x, 0x%08x", Revb.SAME_L_SRC, Revb.SAME_R_SRC);
	Console.WriteLn("    DIFF_L_SRC, DIFF_R_SRC     0x%08x, 0x%08x", Revb.DIFF_L_SRC, Revb.DIFF_R_SRC);
	Console.WriteLn("    SAME_L_DST, SAME_R_DST     0x%08x, 0x%08x", Revb.SAME_L_DST, Revb.SAME_R_DST);
	Console.WriteLn("    DIFF_L_DST, DIFF_R_DST     0x%08x, 0x%08x", Revb.DIFF_L_DST, Revb.DIFF_R_DST);
	Console.WriteLn("    IIR_VOL, WALL_VOL          0x%08x, 0x%08x", Revb.IIR_VOL, Revb.WALL_VOL);

	Console.WriteLn("    APF1_L_DST                 0x%08x", Revb.APF1_L_DST);
	Console.WriteLn("    APF1_R_DST                 0x%08x", Revb.APF1_R_DST);
	Console.WriteLn("    APF2_L_DST                 0x%08x", Revb.APF2_L_DST);
	Console.WriteLn("    APF2_R_DST                 0x%08x", Revb.APF2_R_DST);

	Console.WriteLn("    EffectStartA               0x%x", EffectsStartA & 0x3f'ffff);
	Console.WriteLn("    EffectsEndA                0x%x", EffectsEndA & 0x3f'ffff);
	Console.WriteLn("----------------------------------------------------------");
}

__forceinline s32 V_Core::RevbGetIndexer(s32 offset)
{
	u32 start = EffectsStartA & 0x3f'ffff;
	u32 end = (EffectsEndA & 0x3f'ffff) | 0xffff;

	u32 x = ((Cycles >> 1) + offset) % ((end - start) + 1);

	x += start;

	return x & 0xf'ffff;
}

StereoOut32 V_Core::DoReverb(StereoOut32 Input)
{
	if (EffectsStartA >= EffectsEndA)
	{
		return StereoOut32::Empty;
	}

	Input = clamp_mix(Input);

	RevbDownBuf[0][RevbSampleBufPos] = Input.Left;
	RevbDownBuf[1][RevbSampleBufPos] = Input.Right;
	RevbDownBuf[0][RevbSampleBufPos | 64] = Input.Left;
	RevbDownBuf[1][RevbSampleBufPos | 64] = Input.Right;

	bool R = Cycles & 1;

	// Calculate the read/write addresses we'll be needing for this session of reverb.

	const u32 same_src = RevbGetIndexer(R ? Revb.SAME_R_SRC : Revb.SAME_L_SRC);
	const u32 same_dst = RevbGetIndexer(R ? Revb.SAME_R_DST : Revb.SAME_L_DST);
	const u32 same_prv = RevbGetIndexer(R ? Revb.SAME_R_DST - 1 : Revb.SAME_L_DST - 1);

	const u32 diff_src = RevbGetIndexer(R ? Revb.DIFF_L_SRC : Revb.DIFF_R_SRC);
	const u32 diff_dst = RevbGetIndexer(R ? Revb.DIFF_R_DST : Revb.DIFF_L_DST);
	const u32 diff_prv = RevbGetIndexer(R ? Revb.DIFF_R_DST - 1 : Revb.DIFF_L_DST - 1);

	const u32 comb1_src = RevbGetIndexer(R ? Revb.COMB1_R_SRC : Revb.COMB1_L_SRC);
	const u32 comb2_src = RevbGetIndexer(R ? Revb.COMB2_R_SRC : Revb.COMB2_L_SRC);
	const u32 comb3_src = RevbGetIndexer(R ? Revb.COMB3_R_SRC : Revb.COMB3_L_SRC);
	const u32 comb4_src = RevbGetIndexer(R ? Revb.COMB4_R_SRC : Revb.COMB4_L_SRC);

	const u32 apf1_src = RevbGetIndexer(R ? (Revb.APF1_R_DST - Revb.APF1_SIZE) : (Revb.APF1_L_DST - Revb.APF1_SIZE));
	const u32 apf1_dst = RevbGetIndexer(R ? Revb.APF1_R_DST : Revb.APF1_L_DST);
	const u32 apf2_src = RevbGetIndexer(R ? (Revb.APF2_R_DST - Revb.APF2_SIZE) : (Revb.APF2_L_DST - Revb.APF2_SIZE));
	const u32 apf2_dst = RevbGetIndexer(R ? Revb.APF2_R_DST : Revb.APF2_L_DST);

	// -----------------------------------------
	//          Optimized IRQ Testing !
	// -----------------------------------------

	// This test is enhanced by using the reverb effects area begin/end test as a
	// shortcut, since all buffer addresses are within that area.  If the IRQA isn't
	// within that zone then the "bulk" of the test is skipped, so this should only
	// be a slowdown on a few evil games.

	for (int i = 0; i < 2; i++)
	{
		if (FxEnable && Cores[i].IRQEnable && ((Cores[i].IRQA >= EffectsStartA) && (Cores[i].IRQA <= EffectsEndA)))
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
	in = MUL(R ? Revb.IN_COEF_R : Revb.IN_COEF_L, ReverbDownsample(*this, R));

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

	out = clamp_mix(out);

	RevbUpBuf[R][RevbSampleBufPos] = out;
	RevbUpBuf[!R][RevbSampleBufPos] = 0;

	RevbUpBuf[R][RevbSampleBufPos | 64] = out;
	RevbUpBuf[!R][RevbSampleBufPos | 64] = 0;

	RevbSampleBufPos = (RevbSampleBufPos + 1) & 63;

	return ReverbUpsample(*this);
}
