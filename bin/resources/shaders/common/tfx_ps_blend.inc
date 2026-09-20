#define PICK3(SELECTOR, ZERO, ONE, TWO) ((SELECTOR) == 0 ? (ZERO) : (SELECTOR) == 1 ? (ONE) : (TWO))

PS_STATIC void ps_blend(IN_OUT_PARAM(float4, Color), IN_OUT_PARAM(float4, As_rgba))
{
	float As = As_rgba.a;
	
	if (SW_BLEND)
	{
		// PABE
		if (PS_PABE != FALSE)
		{
			// As_rgba needed for accumulation blend to manipulate Cd.
			// No blending so early exit
			if (As < 1.f)
			{
				As_rgba.rgb = splat3(0.f);
				return;
			}

			As_rgba.rgb = splat3(1.f);
		}

		float4 RT = splat4(0.f);
		if (NEEDS_RT_FOR_BLEND)
		{
			RT = ps_current_color;
			float color_multi = PS_COLCLIP_HW != FALSE ? 65535.0f : 255.0f;
			float alpha_multi = PS_RTA_CORRECTION != FALSE ? 128.0f : 255.0f;
			RT.rgb = trunc(RT.rgb * color_multi + 0.1f);
			RT.a = trunc(RT.a * alpha_multi + 0.1f);
		}
		
		if (PS_SHUFFLE != FALSE && NEEDS_RT_FOR_BLEND)
		{
			uint4 denorm_rt = uint4(RT);
			if ((PS_PROCESS_BA & SHUFFLE_WRITE) != 0)
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

		float Ad = RT.a / 128.0f;
		float3 Cd = RT.rgb;
		float3 Cs = Color.rgb;

		float3 A = PICK3(PS_BLEND_A, Cs, Cd, splat3(0.0f));
		float3 B = PICK3(PS_BLEND_B, Cs, Cd, splat3(0.0f));
		float  C = PICK3(PS_BLEND_C, As, Ad, ps_cb.alpha_fix);
		float3 D = PICK3(PS_BLEND_D, Cs, Cd, splat3(0.0f));

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
			Color.rgb = ((A - B) * C_clamped + D) + (124.f / 256.f);
		else if (PS_BLEND_MIX == 1)
			Color.rgb = ((A - B) * C_clamped + D) - (124.f / 256.f);
		else
			Color.rgb = trunc((A - B) * C + D);

		if (PS_BLEND_HW == 1)
		{
			// As or Af
			As_rgba.rgb = splat3(C);
			// Subtract 1 for alpha to compensate for the changed equation,
			// if c.rgb > 255.0f then we further need to adjust alpha accordingly,
			// we pick the lowest overflow from all colors because it's the safest,
			// we divide by 255 the color because we don't know Cd value,
			// changed alpha should only be done for hw blend.
			float3 alpha_compensate = max(splat3(1.f), Color.rgb / splat3(255.f));
			As_rgba.rgb -= alpha_compensate;
		}
		else if (PS_BLEND_HW == 2)
		{
			// Since we can't do Cd*(Alpha + 1) - Cs*Alpha in hw blend
			// what we can do is adjust the Cs value that will be
			// subtracted, this way we can get a better result in hw blend.
			// Result is still wrong but less wrong than before.
			float division_alpha = 1.f + C;
			Color.rgb /= splat3(division_alpha);
		}
		else if (PS_BLEND_HW == 3)
		{
			// As, Ad or Af clamped.
			As_rgba.rgb = splat3(C_clamped);
			// Cs*(Alpha + 1) might overflow, if it does then adjust alpha value
			// that is sent on second output to compensate.
			float3 overflow_check = (Color.rgb - splat3(255.f)) / 255.f;
			float3 alpha_compensate = max(splat3(0.f), overflow_check);
			As_rgba.rgb -= alpha_compensate;
		}
	}
	else
	{
		float Alpha = PS_BLEND_C == 2 ? ps_cb.alpha_fix : As;

		if (PS_BLEND_HW == 1)
		{
			// Needed for Cd * (As/Ad/F + 1) blending modes
			Color.rgb = splat3(255.f);
		}
		else if (PS_BLEND_HW == 2)
		{
			// Cd*As,Cd*Ad or Cd*F
			Color.rgb = splat3(saturate(Alpha - 1.f) * 255.f);
		}
		else if (PS_BLEND_HW == 3 && PS_RTA_CORRECTION == 0)
		{
			// Needed for Cs*Ad, Cs*Ad + Cd, Cd - Cs*Ad
			// Multiply Color.rgb by (255/128) to compensate for wrong Ad/255 value when rgb are below 128.
			// When any color channel is higher than 128 then adjust the compensation automatically
			// to give us more accurate colors, otherwise they will be wrong.
			// The higher the value (>128) the lower the compensation will be.
			float max_color = max(max(Color.r, Color.g), Color.b);
			float color_compensate = 255.f / max(128.f, max_color);
			Color.rgb *= splat3(color_compensate);
		}
		else if (PS_BLEND_HW == 4)
		{
			// Needed for Cd * (1 - Ad) and Cd*(1 + Alpha).
			As_rgba.rgb = splat3(Alpha * (128.0f / 255.0f));
			Color.rgb = splat3(127.5f);
		}
		else if (PS_BLEND_HW == 5)
		{
			// Needed for Cs*Alpha + Cd*(1 - Alpha).
			Alpha *= 128.0f / 255.0f;
			As_rgba.rgb = splat3(Alpha - 0.5f);
			Color.rgb = (Color.rgb * Alpha);
		}
		else if (PS_BLEND_HW == 6)
		{
			// Needed for Cd*Alpha + Cs*(1 - Alpha).
			Alpha *= 128.0f / 255.0f;
			As_rgba.rgb = splat3(Alpha);
			Color.rgb *= (Alpha - 0.5f);
		}
	}
}

#undef PICK3
