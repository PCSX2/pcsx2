STATIC void ps_shuffle(IN_OUT_PARAM(FLOAT4, C))
{
	if (PS_SHUFFLE != FALSE)
	{
		if ((PS_SHUFFLE_SAME == FALSE) && (PS_READ16_SRC == FALSE) &&
			!(PS_PROCESS_BA == SHUFFLE_READWRITE && PS_PROCESS_RG == SHUFFLE_READWRITE))
		{
			UINT4 denorm_c_after = UINT4(C);
			if ((PS_PROCESS_BA & SHUFFLE_READ) != 0)
			{
				C.b = float(((denorm_c_after.r >> 3) & 0x1Fu) | ((denorm_c_after.g << 2) & 0xE0u));
				C.a = float(((denorm_c_after.g >> 6) & 0x3u) | ((denorm_c_after.b >> 1) & 0x7Cu) | (denorm_c_after.a & 0x80u));
			}
			else
			{
				C.r = float(((denorm_c_after.r >> 3) & 0x1Fu) | ((denorm_c_after.g << 2) & 0xE0u));
				C.g = float(((denorm_c_after.g >> 6) & 0x3u) | ((denorm_c_after.b >> 1) & 0x7Cu) | (denorm_c_after.a & 0x80u));
			}
		}

		// Special case for 32bit input and 16bit output, shuffle used by The Godfather
		if (PS_SHUFFLE_SAME != FALSE)
		{
			UINT4 denorm_c = UINT4(C);
			
			if ((PS_PROCESS_BA & SHUFFLE_READ) != 0u)
				C = float4_bcast(float((denorm_c.b & 0x7Fu) | (denorm_c.a & 0x80u)));
			else
				C.ga = C.rg;
		}
		// Copy of a 16bit source in to this target
		else if (PS_READ16_SRC != FALSE)
		{
			UINT4 denorm_c = UINT4(C);
			UINT2 denorm_TA = UINT2(ps_cb.ta * 255.5f);
			
			C.rb = float2_bcast(float((denorm_c.r >> 3) | (((denorm_c.g >> 3) & 0x7u) << 5)));
			C.ga = float2_bcast(float((denorm_c.g >> 6) | ((denorm_c.b >> 3) << 2) | (denorm_TA.x & 0x80u)));
		}
		else if (PS_SHUFFLE_ACROSS != FALSE)
		{
			if (PS_PROCESS_BA == SHUFFLE_READWRITE && PS_PROCESS_RG == SHUFFLE_READWRITE)
			{
				C.br = C.rb;
				C.ag = C.ga;
			}
			else if ((PS_PROCESS_BA & SHUFFLE_READ) != 0u)
			{
				C.rb = C.bb;
				C.ga = C.aa;
			}
			else
			{
				C.rb = C.rr;
				C.ga = C.gg;
			}
		}
	}
}

STATIC PSOutputGeneric ps_main_impl()
{
	PSOutputGeneric psout;
	psout.c0 = float4_bcast(0.0f);
	psout.c1 = float4_bcast(0.0f);
	psout.depth = 0.0f;

	float input_z = ps_in.p.z;

	if (BROKEN_SHADER_DEPTH != FALSE)
		input_z -= EXP2_NEG_32;

	if (PS_ZFLOOR != FALSE)
		input_z = floor(input_z * EXP2_POS_32) * EXP2_NEG_32;

	if (PS_ZTST == ZTST_GEQUAL || PS_ZTST == ZTST_GREATER)
	{
		if (PS_ZTST == ZTST_GEQUAL && input_z < ps_current_depth)
			discard_all();
		if (PS_ZTST == ZTST_GREATER && input_z <= ps_current_depth)
			discard_all();
	}

	if ((uint(PS_SCANMSK) & 2u) != 0u)
	{
		if ((uint(ps_in.p.y) & 1u) == (uint(PS_SCANMSK) & 1u))
			discard_all();
	}

	if (PS_DATE >= 5)
	{
		// 1 => DATM == 0, 2 => DATM == 1
		float rt_a = PS_WRITE_RG != FALSE ? ps_current_color.g : ps_current_color.a;
		bool bad = PS_RTA_CORRECTION != FALSE ?
			((uint(PS_DATE) & 3u) == 1u ? (rt_a > (254.5f / 255.f)) : (rt_a < (254.5f / 255.f))) :
			((uint(PS_DATE) & 3u) == 1u ? (rt_a > 0.5) : (rt_a < 0.5));

		if (bad)
			discard_all();
	}

	if (PS_DATE == 3)
	{
		float stencil_ceil = PS_READ_PRIMID(UINT2(ps_in.p.xy));
		// Note prim_id == stencil_ceil will be the primitive that will update
		// the bad alpha value so we must keep it.
		if (float(ps_prim_id) > stencil_ceil)
			discard_all();
	}

	FLOAT4 C = ps_color();

	// Must be done before alpha correction

	if (PS_AA1 != PS_AA1_NONE)
	{
		float cov = PS_AA1 == PS_AA1_LINE
			? SATURATE(ps_cb.line_cov_scale * (1.f - abs(ps_in.inv_cov))) // Blur only outer part of the line by scaling coverage.
			: SATURATE(1.f - abs(ps_in.inv_cov));
		if (PS_ABE == FALSE || floor(C.a) == 128.f) // The coverage is only used if the fragment alpha is 128.
			C.a = 128.f * cov;
	}
	else if (PS_FIXED_ONE_A != FALSE)
	{
		// AA (Fixed one) will output a coverage of 1.0 as alpha
		C.a = 128.0f;
	}

	bool atst_pass = atst(C);
	if (PS_AFAIL == AFAIL_KEEP && !atst_pass)
		discard_all();

	FLOAT4 alpha_blend = float4_bcast(0.f);
	if (SW_AD_TO_HW)
	{
		float RT_a = trunc(ps_current_color.a * (PS_RTA_CORRECTION != FALSE ? 128.f : 255.f) + 0.1f);
		alpha_blend = float4_bcast(RT_a / 128.f);
	}
	else
	{
		alpha_blend = float4_bcast(C.a / 128.f);
	}

	ps_alpha_correct(C);

	// Get first primitive that will write a failing alpha value
	if (PS_DATE == 1)
	{
		// DATM == 0, Pixel with alpha equal to 1 will failed (128-255)
		psout.c0 = float4_bcast(C.a > 127.5f ? float(ps_prim_id) : float(PRIMID_MAX));
		return psout;
	}
	else if (PS_DATE == 2)
	{
		// DATM == 1, Pixel with alpha equal to 0 will failed (0-127)
		psout.c0 = float4_bcast(C.a < 127.5f ? float(ps_prim_id) : PRIMID_MAX);
		return psout;
	}

	ps_blend(C, alpha_blend);

	ps_shuffle(C);

	ps_dither(C, alpha_blend.a);

	// Color clamp/wrap needs to be done after sw blending and dithering
	ps_color_clamp_wrap(C);

	ps_fbmask(C);

	// Use alpha blend factor to determine whether to update A.
	if (PS_AFAIL == AFAIL_RGB_ONLY_DSB)
		alpha_blend.a = float(atst_pass);

	if (PS_NO_COLOR == FALSE)
	{
		psout.c0.a = PS_RTA_CORRECTION != FALSE ? C.a / 128.f : C.a / 255.f;
		psout.c0.rgb = PS_COLCLIP_HW != FALSE ? FLOAT3(C.rgb / 65535.f) : C.rgb / 255.f;
	}
	if (PS_NO_COLOR1 == FALSE)
		psout.c1 = alpha_blend;

	if (PS_ZCLAMP != FALSE)
		input_z = min(input_z, ps_cb.max_depth_ps);

	if (PS_AA1 == PS_AA1_TRIANGLE_SW_Z && ps_in.interior == 0)
		discard_depth(input_z); // No depth update for triangle edges.

	if (!atst_pass)
	{
		if (PS_AFAIL == AFAIL_RGB_ONLY_SW_Z || PS_AFAIL == AFAIL_RGB_ONLY)
			psout.c0.a = ps_current_color.a; // discard alpha
		else if (PS_AFAIL == AFAIL_ZB_ONLY)
			discard_color(psout.c0);

		if (PS_AFAIL == AFAIL_RGB_ONLY_SW_Z || PS_AFAIL == AFAIL_FB_ONLY)
			discard_depth(input_z);
	}

	psout.depth = input_z;

	return psout;
}