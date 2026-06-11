// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "tfx_defines.hlsl"
#include "tfx_ps_resources.hlsl"

#ifndef PS_BLEND_HW
#define PS_BLEND_HW 0
#define PS_A_MASKED 0
#define PS_SHUFFLE 0
#define PS_PROCESS_BA 0
#define PS_COLCLIP_HW 0
#define PS_BLEND_A 0
#define PS_BLEND_B 0
#define PS_BLEND_C 0
#define PS_BLEND_D 0
#define PS_BLEND_MIX 0
#define PS_PABE 0
#endif

#define SW_BLEND (PS_BLEND_A || PS_BLEND_B || PS_BLEND_D)
#define SW_BLEND_NEEDS_RT (SW_BLEND && (PS_BLEND_A == 1 || PS_BLEND_B == 1 || PS_BLEND_C == 1 || PS_BLEND_D == 1))
#define SW_AD_TO_HW (PS_BLEND_C == 1 && PS_A_MASKED)

float4 RtLoad(int2 xy);

float rta_correction_factor();
void rta_correction_blend_hw_3(inout float4 Color);

export void ps_blend(inout float4 Color, inout float4 As_rgba, float2 pos_xy)
{
	float As = As_rgba.a;

	if (SW_BLEND)
	{
		// PABE
		if (PS_PABE)
		{
			// As_rgba needed for accumulation blend to manipulate Cd.
			// No blending so early exit
			if (As < 1.0f)
			{
				As_rgba.rgb = (float3)0.0f;
				return;
			}

			As_rgba.rgb = (float3)1.0f;
		}

		float4 RT = SW_BLEND_NEEDS_RT ? RtLoad(int2(pos_xy)) : (float4) 0.0f;

		if (PS_SHUFFLE && SW_BLEND_NEEDS_RT)
		{
			uint4 denorm_rt = uint4(RT);
			if (PS_PROCESS_BA & SHUFFLE_WRITE)
			{
				RT.r = float((denorm_rt.b << 3) & 0xF8u);
				RT.g = float(((denorm_rt.b >> 2) & 0x38u) | ((denorm_rt.a << 6) & 0xC0u));
				RT.b = float((denorm_rt.a << 1) & 0xF8u);
				RT.a = float(denorm_rt.a & 0x80u);
			}
			else
			{
				RT.r = float((denorm_rt.r << 3) & 0xF8u);
				RT.g = float(((denorm_rt.r >> 2) & 0x38u) | ((denorm_rt.g << 6) & 0xC0u));
				RT.b = float((denorm_rt.g << 1) & 0xF8u);
				RT.a = float(denorm_rt.g & 0x80u);
			}
		}

        float Ad = trunc(RT.a * rta_correction_factor() + 0.1f) / 128.0f;
		float color_multi = PS_COLCLIP_HW ? 65535.0f : 255.0f;
		float3 Cd = trunc(RT.rgb * color_multi + 0.1f);
		float3 Cs = Color.rgb;

		float3 A = (PS_BLEND_A == 0) ? Cs : ((PS_BLEND_A == 1) ? Cd : (float3)0.0f);
		float3 B = (PS_BLEND_B == 0) ? Cs : ((PS_BLEND_B == 1) ? Cd : (float3)0.0f);
		float  C = (PS_BLEND_C == 0) ? As : ((PS_BLEND_C == 1) ? Ad : Af);
		float3 D = (PS_BLEND_D == 0) ? Cs : ((PS_BLEND_D == 1) ? Cd : (float3)0.0f);

		// As/Af clamp alpha for Blend mix
		// We shouldn't clamp blend mix with blend hw 1 as we want alpha higher
		float C_clamped = C;
		if (PS_BLEND_MIX > 0 && PS_BLEND_HW != 1 && PS_BLEND_HW != 2)
			C_clamped = saturate(C_clamped);

		if (PS_BLEND_A == PS_BLEND_B)
			Color.rgb = D;
		// In blend_mix, HW adds on some alpha factor * dst.
		// Truncating here wouldn't quite get the right result because it prevents the <1 bit here from combining with a <1 bit in dst to form a ≥1 amount that pushes over the truncation.
		// Instead, apply an offset to convert HW's round to a floor.
		// Since alpha is in 1/128 increments, subtracting (0.5 - 0.5/128 == 127/256) would get us what we want if GPUs blended in full precision.
		// But they don't.  Details here: https://github.com/PCSX2/pcsx2/pull/6809#issuecomment-1211473399
		// Based on the scripts at the above link, the ideal choice for Intel GPUs is 126/256, AMD 120/256.  Nvidia is a lost cause.
		// 124/256 seems like a reasonable compromise, providing the correct answer 99.3% of the time on Intel (vs 99.6% for 126/256), and 97% of the time on AMD (vs 97.4% for 120/256).
		else if (PS_BLEND_MIX == 2)
			Color.rgb = ((A - B) * C_clamped + D) + (124.0f / 256.0f);
		else if (PS_BLEND_MIX == 1)
			Color.rgb = ((A - B) * C_clamped + D) - (124.0f / 256.0f);
		else
			Color.rgb = trunc(((A - B) * C) + D);

		if (PS_BLEND_HW == 1)
		{
			// As or Af
			As_rgba.rgb = (float3)C;
			// Subtract 1 for alpha to compensate for the changed equation,
			// if c.rgb > 255.0f then we further need to adjust alpha accordingly,
			// we pick the lowest overflow from all colors because it's the safest,
			// we divide by 255 the color because we don't know Cd value,
			// changed alpha should only be done for hw blend.
			float3 alpha_compensate = max((float3)1.0f, Color.rgb / (float3)255.0f);
			As_rgba.rgb -= alpha_compensate;
		}
		else if (PS_BLEND_HW == 2)
		{
			// Since we can't do Cd*(Alpha + 1) - Cs*Alpha in hw blend
			// what we can do is adjust the Cs value that will be
			// subtracted, this way we can get a better result in hw blend.
			// Result is still wrong but less wrong than before.
			float division_alpha = 1.0f + C;
			Color.rgb /= (float3)division_alpha;
		}
		else if (PS_BLEND_HW == 3)
		{
			// As, Ad or Af clamped.
			As_rgba.rgb = (float3)C_clamped;
			// Cs*(Alpha + 1) might overflow, if it does then adjust alpha value
			// that is sent on second output to compensate.
			float3 overflow_check = (Color.rgb - (float3)255.0f) / 255.0f;
			float3 alpha_compensate = max((float3)0.0f, overflow_check);
			As_rgba.rgb -= alpha_compensate;
		}
	}
	else
	{
		float3 Alpha = PS_BLEND_C == 2 ? (float3)Af : (float3)As;

		if (PS_BLEND_HW == 1)
		{
			// Needed for Cd * (As/Ad/F + 1) blending modes
			Color.rgb = (float3)255.0f;
		}
		else if (PS_BLEND_HW == 2)
		{
			// Cd*As,Cd*Ad or Cd*F
			Color.rgb = saturate(Alpha - (float3)1.0f) * (float3)255.0f;
		}
		else if (PS_BLEND_HW == 3)
		{
			// Needed for Cs*Ad, Cs*Ad + Cd, Cd - Cs*Ad
			// Multiply Color.rgb by (255/128) to compensate for wrong Ad/255 value when rgb are below 128.
			// When any color channel is higher than 128 then adjust the compensation automatically
			// to give us more accurate colors, otherwise they will be wrong.
			// The higher the value (>128) the lower the compensation will be.
            rta_correction_blend_hw_3(Color);
        }
		else if (PS_BLEND_HW == 4)
		{
			// Needed for Cd * (1 - Ad) and Cd*(1 + Alpha).
			As_rgba.rgb = Alpha * (float3)(128.0f / 255.0f);
			Color.rgb = (float3)127.5f;
		}
		else if (PS_BLEND_HW == 5)
		{
			// Needed for Cs*Alpha + Cd*(1 - Alpha).
			Alpha *= (float3)(128.0f / 255.0f);
			As_rgba.rgb = (Alpha - (float3)0.5f);
			Color.rgb = (Color.rgb * Alpha);
		}
		else if (PS_BLEND_HW == 6)
		{
			// Needed for Cd*Alpha + Cs*(1 - Alpha).
			Alpha *= (float3)(128.0f / 255.0f);
			As_rgba.rgb = Alpha;
			Color.rgb *= (Alpha - (float3)0.5f);
		}
	}
}

export float4 ps_alpha_blend(float2 xy, float Ca)
{
	if (SW_AD_TO_HW)
	{
        float4 RT = trunc(RtLoad(xy) * rta_correction_factor() + 0.1f);
		return (float4)(RT.a / 128.0f);
	}
	else
	{
		return (float4)(Ca / 128.0f);
	}
}
