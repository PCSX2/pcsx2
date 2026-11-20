// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

PS_STATIC void ps_shuffle(IN_OUT_PARAM(float4, C))
{
	__IF__ (PS_SHUFFLE != 0)
	{
		__IF__ ((PS_SHUFFLE_SAME == 0) && (PS_READ16_SRC == 0) && !(PS_PROCESS_BA == SHUFFLE_READWRITE && PS_PROCESS_RG == SHUFFLE_READWRITE))
		{
			uint4 denorm_c_after = uint4(C);
			__IF__ ((PS_PROCESS_BA & SHUFFLE_READ) != 0)
			{
				C.b = float(((denorm_c_after.r >> 3) & 0x1Fu) | ((denorm_c_after.g << 2) & 0xE0u));
				C.a = float(((denorm_c_after.g >> 6) & 0x3u) | ((denorm_c_after.b >> 1) & 0x7Cu) | (denorm_c_after.a & 0x80u));
			}
			__ELSE__
			{
				C.r = float(((denorm_c_after.r >> 3) & 0x1Fu) | ((denorm_c_after.g << 2) & 0xE0u));
				C.g = float(((denorm_c_after.g >> 6) & 0x3u) | ((denorm_c_after.b >> 1) & 0x7Cu) | (denorm_c_after.a & 0x80u));
			}
			__ENDIF__
		}
		__ENDIF__

		// Special case for 32bit input and 16bit output, shuffle used by The Godfather
		__IF__ (PS_SHUFFLE_SAME != 0)
		{
			uint4 denorm_c = uint4(C);
			
			__IF__ ((PS_PROCESS_BA & SHUFFLE_READ) != 0)
				C = splat4(float((denorm_c.b & 0x7Fu) | (denorm_c.a & 0x80u)));
			__ELSE__
				C.ga = C.rg;
			__ENDIF__
		}
		// Copy of a 16bit source in to this target
		__ELIF__ (PS_READ16_SRC != 0)
		{
			uint4 denorm_c = uint4(C);
			uint2 denorm_TA = uint2(cb.ta * 255.5f);
			
			C.rb = splat2(float((denorm_c.r >> 3) | (((denorm_c.g >> 3) & 0x7u) << 5)));
			C.ga = splat2(float((denorm_c.g >> 6) | ((denorm_c.b >> 3) << 2) | (denorm_TA.x & 0x80u)));
		}
		__ELIF__ (PS_SHUFFLE_ACROSS != 0)
		{
			__IF__ (PS_PROCESS_BA == SHUFFLE_READWRITE && PS_PROCESS_RG == SHUFFLE_READWRITE)
			{
				C.br = C.rb;
				C.ag = C.ga;
			}
			__ELIF__ ((PS_PROCESS_BA & SHUFFLE_READ) != 0)
			{
				C.rb = C.bb;
				C.ga = C.aa;
			}
			__ELSE__
			{
				C.rb = C.rr;
				C.ga = C.gg;
			}
			__ENDIF__
		}
		__ENDIF__
	}
	__ENDIF__
}

PS_STATIC PSOutputGeneric ps_main_impl()
{
	PSOutputGeneric psout;
	psout.c0 = splat4(0.0f);
	psout.c1 = splat4(0.0f);
	psout.depth = 0.0f;

	float input_z = ps_in.p.z;

	__IF__ (BROKEN_SHADER_DEPTH != 0)
		input_z -= EXP2_NEG_32;
	__ENDIF__

	__IF__ (PS_ZFLOOR != 0)
		input_z = floor(input_z * EXP2_POS_32) * EXP2_NEG_32;
	__ENDIF__

	__IF__ (PS_ZTST == ZTST_GEQUAL || PS_ZTST == ZTST_GREATER)
	{
		__IF__ (PS_ZTST == ZTST_GEQUAL)
			if (input_z < current_depth)
				discard_all();
		__ENDIF__
		__IF__ (PS_ZTST == ZTST_GREATER)
			if (input_z <= current_depth)
				discard_all();
		__ENDIF__
	}
	__ENDIF__

	__IF__ ((PS_SCANMSK & 2) != 0)
	{
		if ((uint(ps_in.p.y) & 1u) == (uint(PS_SCANMSK) & 1u))
			discard_all();
	}
	__ENDIF__

	__IF__ (PS_DATE >= 5)
	{
		// 1 => DATM == 0, 2 => DATM == 1
		float rt_a = PS_WRITE_RG != 0 ? current_color.g : current_color.a;
		bool bad = PS_RTA_CORRECTION != 0 ?
			((uint(PS_DATE) & 3u) == 1u ? (rt_a > (254.5f / 255.f)) : (rt_a < (254.5f / 255.f))) :
			((uint(PS_DATE) & 3u) == 1u ? (rt_a > 0.5) : (rt_a < 0.5));

		if (bad)
			discard_all();
	}
	__ENDIF__

	__IF__ (PS_DATE == 3)
	{
		float stencil_ceil = read_primid(uint2(ps_in.p.xy));
		// Note prim_id == stencil_ceil will be the primitive that will update
		// the bad alpha value so we must keep it.
		if (float(prim_id) > stencil_ceil)
			discard_all();
	}
	__ENDIF__

	float4 C = ps_color();

	// Must be done before alpha correction

	__IF__ (PS_AA1 != PS_AA1_NONE)
	{
		float cov = PS_AA1 == PS_AA1_LINE
			? saturate(cb.line_cov_scale * (1.f - abs(ps_in.inv_cov))) // Blur only outer part of the line by scaling coverage.
			: saturate(1.f - abs(ps_in.inv_cov));
		if (PS_ABE == 0 || floor(C.a) == 128.f) // The coverage is only used if the fragment alpha is 128.
			C.a = 128.f * cov;
	}
	__ELIF__ (PS_FIXED_ONE_A != 0)
	{
		// AA (Fixed one) will output a coverage of 1.0 as alpha
		C.a = 128.0f;
	}
	__ENDIF__

	bool atst_pass = atst(C);
	__IF__ (PS_AFAIL == AFAIL_KEEP)
	{
		if (!atst_pass)
			discard_all();
	}
	__ENDIF__

	float4 alpha_blend = splat4(0.f);
	__IF__ (SW_AD_TO_HW)
	{
		float RT_a = trunc(current_color.a * (PS_RTA_CORRECTION != 0 ? 128.f : 255.f) + 0.1f);
		alpha_blend = splat4(RT_a / 128.f);
	}
	__ELSE__
	{
		alpha_blend = splat4(C.a / 128.f);
	}
	__ENDIF__

	ps_alpha_correct(C);

	// Get first primitive that will write a failing alpha value
	__IF__ (PS_DATE == 1)
	{
		// DATM == 0, Pixel with alpha equal to 1 will failed (128-255)
		psout.c0 = splat4(C.a > 127.5f ? float(prim_id) : float(PRIMID_MAX));
		return psout;
	}
	__ELIF__ (PS_DATE == 2)
	{
		// DATM == 1, Pixel with alpha equal to 0 will failed (0-127)
		psout.c0 = splat4(C.a < 127.5f ? float(prim_id) : float(PRIMID_MAX));
		return psout;
	}
	__ENDIF__

	ps_blend(C, alpha_blend);

	ps_shuffle(C);

	ps_dither(C, alpha_blend.a);

	// Color clamp/wrap needs to be done after sw blending and dithering
	ps_color_clamp_wrap(C);

	ps_fbmask(C);

	// Use alpha blend factor to determine whether to update A.
	__IF__ (PS_AFAIL == AFAIL_RGB_ONLY_DSB)
		alpha_blend.a = float(atst_pass);
	__ENDIF__

	__IF__ (PS_NO_COLOR == 0)
	{
		psout.c0.a = PS_RTA_CORRECTION != 0 ? C.a / 128.f : C.a / 255.f;
		psout.c0.rgb = PS_COLCLIP_HW != 0 ? float3(C.rgb / 65535.f) : C.rgb / 255.f;
	}
	__ENDIF__

	__IF__ (PS_NO_COLOR1 == 0)
		psout.c1 = alpha_blend;
	__ENDIF__

	__IF__ (PS_ZCLAMP != 0)
		input_z = min(input_z, cb.max_depth_ps);
	__ENDIF__

	__IF__ (PS_AA1 == PS_AA1_TRIANGLE_SW_Z)
	{
		if (ps_in.interior == 0u)
			discard_depth(input_z); // No depth update for triangle edges.
	}
	__ENDIF__

	if (!atst_pass)
	{
		__IF__ (PS_AFAIL == AFAIL_RGB_ONLY_SW_Z || PS_AFAIL == AFAIL_RGB_ONLY)
			psout.c0.a = current_color.a; // discard alpha
		__ELIF__ (PS_AFAIL == AFAIL_ZB_ONLY)
			discard_color(psout.c0);
		__ENDIF__

		__IF__ (PS_AFAIL == AFAIL_RGB_ONLY_SW_Z || PS_AFAIL == AFAIL_FB_ONLY)
			discard_depth(input_z);
		__ENDIF__
	}

	psout.depth = input_z;

	return psout;
}
