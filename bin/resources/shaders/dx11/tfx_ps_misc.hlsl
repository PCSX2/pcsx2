// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "tfx_defines.hlsl"
#include "tfx_ps_resources.hlsl"

#ifndef PS_BLEND_HW
// ps_color_clamp_wrap
#define PS_BLEND_HW 0
// ps_alpha_correction
#define PS_FBA 0
// ps_fbmask, ps_color_clamp_wrap
#define PS_FBMASK 0
// ps_color_clamp_wrap, ps_alpha_correction
#define PS_DST_FMT 0
// ps_fbmask, ps_color_clamp_wrap
#define PS_COLCLIP_HW 0
// ps_color_clamp_wrap
#define PS_COLCLIP 0
// SW_BLEND
#define PS_BLEND_A 0
// SW_BLEND
#define PS_BLEND_B 0
// ps_dither
#define PS_BLEND_C 0
// SW_BLEND
#define PS_BLEND_D 0
// ps_color_clamp_wrap
#define PS_BLEND_MIX 0
// ps_dither
#define PS_ROUND_INV 0
// ps_dither, ps_color_clamp_wrap
#define PS_DITHER 0
// ps_dither
#define PS_DITHER_ADJUST 0
#endif

// ps_color_clamp_wrap
#define SW_BLEND (PS_BLEND_A || PS_BLEND_B || PS_BLEND_D)

float4 RtLoad(int2 xy);

export void ps_fbmask(inout float4 C, float2 pos_xy)
{
	if (PS_FBMASK)
	{
		float multi = PS_COLCLIP_HW ? 65535.0f : 255.0f;
		float4 RT = trunc(RtLoad(int2(pos_xy)) * multi + 0.1f);
		C = (float4)(((uint4)C & ~FbMask) | ((uint4)RT & FbMask));
	}
}

export void ps_dither(inout float3 C, float As, float2 pos_xy)
{
	if (PS_DITHER > 0 && PS_DITHER < 3)
	{
		int2 fpos;

		if (PS_DITHER == 2)
			fpos = int2(pos_xy);
		else
			fpos = int2(pos_xy * RcpScaleFactor);

		float value = DitherMatrix[fpos.x & 3][fpos.y & 3];
		
		// The idea here is we add on the dither amount adjusted by the alpha before it goes to the hw blend
		// so after the alpha blend the resulting value should be the same as (Cs - Cd) * As + Cd + Dither.
		if (PS_DITHER_ADJUST)
		{
			float Alpha = PS_BLEND_C == 2 ? Af : As;
			value *= Alpha > 0.0f ? min(1.0f / Alpha, 1.0f) : 1.0f;
		}
		
		if (PS_ROUND_INV)
			C -= value;
		else
			C += value;
	}
}

export void ps_color_clamp_wrap(inout float3 C)
{
	// When dithering the bottom 3 bits become meaningless and cause lines in the picture
	// so we need to limit the color depth on dithered items
	if (SW_BLEND || (PS_DITHER > 0 && PS_DITHER < 3) || PS_FBMASK)
	{
		if (PS_DST_FMT == FMT_16 && PS_BLEND_MIX == 0 && PS_ROUND_INV)
			C += 7.0f; // Need to round up, not down since the shader will invert

		// Standard Clamp
		if (PS_COLCLIP == 0 && PS_COLCLIP_HW == 0)
			C = clamp(C, (float3)0.0f, (float3)255.0f);

		// In 16 bits format, only 5 bits of color are used. It impacts shadows computation of Castlevania
		if (PS_DST_FMT == FMT_16 && PS_DITHER != 3 && (PS_BLEND_MIX == 0 || PS_DITHER))
			C = (float3)((int3)C & (int3)0xF8);
		else if (PS_COLCLIP == 1 || PS_COLCLIP_HW == 1)
			C = (float3)((int3)C & (int3)0xFF);
	}
	else if (PS_DST_FMT == FMT_16 && PS_DITHER != 3 && PS_BLEND_MIX == 0 && PS_BLEND_HW == 0)
		C = (float3)((int3)C & (int3)0xF8);
}

export void ps_alpha_correction(inout float Ca)
{
	if (PS_DST_FMT == FMT_16)
	{
		float A_one = 128.0f; // alpha output will be 0x80
		Ca = PS_FBA ? A_one : step(A_one, Ca) * A_one;
	}
	else if ((PS_DST_FMT == FMT_32) && PS_FBA)
	{
		float A_one = 128.0f;
		if (Ca < A_one) Ca += A_one;
	}
}
