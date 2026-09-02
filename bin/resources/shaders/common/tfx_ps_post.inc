PS_STATIC void ps_fbmask(IN_OUT_PARAM(float4, C))
{
	if (PS_FBMASK != FALSE)
	{
		float multi = (PS_COLCLIP_HW != FALSE) ? 65535.f : 255.f;
		float4 RT = trunc(ps_current_color * float4(multi, multi, multi, 255.f) + float4_bcast(0.1f));
		C = float4((uint4(int4(C)) & ~ps_cb.fbmask) | (uint4(RT) & ps_cb.fbmask));
	}
}

PS_STATIC void ps_dither(IN_OUT_PARAM(float4, C), float As)
{
	if (PS_DITHER == 0 || PS_DITHER == 3)
		return;
	ushort2 fpos;
	if (PS_DITHER == 2)
		fpos = ushort2(ps_in.p.xy);
	else
		fpos = ushort2(ps_in.p.xy * float2_bcast(ps_cb.scale_factor.y));
	float value = MAT_GET(ps_cb.dither_matrix, fpos.x & 3u, fpos.y & 3u);

	// The idea here is we add on the dither amount adjusted by the alpha before it goes to the hw blend
	// so after the alpha blend the resulting value should be the same as (Cs - Cd) * As + Cd + Dither.
	if (PS_DITHER_ADJUST != FALSE)
	{
		float Alpha = PS_BLEND_C == 2 ? ps_cb.alpha_fix : As;
		value *= Alpha > 0.f ? min(1.f / Alpha, 1.f) : 1.f;
	}

	if (PS_ROUND_INV != FALSE)
		C.rgb -= value;
	else
		C.rgb += value;
}

PS_STATIC void ps_color_clamp_wrap(IN_OUT_PARAM(float4, C))
{
	// When dithering the bottom 3 bits become meaningless and cause lines in the picture
	// so we need to limit the color depth on dithered items
	if (SW_BLEND || (PS_DITHER > 0 && PS_DITHER < 3) || PS_FBMASK != FALSE)
	{
		if (PS_DST_FMT == FMT_16 && PS_BLEND_MIX == 0 && PS_ROUND_INV != FALSE)
			C.rgb += 7.f; // Need to round up, not down since the shader will invert

		// Correct the Color value based on the output format
		if (PS_COLCLIP == 0 && PS_COLCLIP_HW == 0)
			C.rgb = clamp(C.rgb, 0.f, 255.f); // Standard Clamp

		// FIXME rouding of negative float?
		// compiler uses trunc but it might need floor

		// Warning: normally blending equation is mult(A, B) = A * B >> 7. GPU have the full accuracy
		// GS: Color = 1, Alpha = 255 => output 1
		// GPU: Color = 1/255, Alpha = 255/255 * 255/128 => output 1.9921875
		// In 16 bits format, only 5 bits of colors are used. It impacts shadows computation of Castlevania
		if (PS_DST_FMT == FMT_16 && PS_DITHER != 3 && (PS_BLEND_MIX == 0 || PS_DITHER != FALSE))
			C.rgb = float3(short3(C.rgb) & short(0xF8));
		else if (PS_COLCLIP == 1 || PS_COLCLIP_HW == 1)
			C.rgb = float3(short3(C.rgb) & short(0xFF));
	}
	else if (PS_DST_FMT == FMT_16 && PS_DITHER != 3 && PS_BLEND_MIX == 0 && PS_BLEND_HW == 0)
		C.rgb = float3(short3(C.rgb) & short(0xF8));
}

PS_STATIC void ps_alpha_correct(IN_OUT_PARAM(float4, C))
{
  if (PS_DST_FMT == FMT_16)
	{
		float A_one = 128.f;
		C.a = (PS_FBA != FALSE) ? A_one : step(128.f, C.a) * A_one;
	}
	else if (PS_DST_FMT == FMT_32 && PS_FBA != FALSE)
	{
		if (C.a < 128.f)
			C.a += 128.f;
	}
}
