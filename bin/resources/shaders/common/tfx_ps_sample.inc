STATIC float manual_lod(IN_PARAM(PSMainState, state), float uv_w)
{
	float K = state.cb.lod_params.x;
	float L = state.cb.lod_params.y;
	float bias = state.cb.lod_params.z;
	float max_lod = state.cb.lod_params.w;
	float gs_lod = K - log2(abs(uv_w)) * L;
	return min(gs_lod, max_lod) - bias;
}

STATIC FLOAT4 sample_c(IN_PARAM(PSMainState, state), FLOAT2 uv)
{
	if (PS_TEX_IS_FB != 0)
		return state.current_color;
	if (PS_REGION_RECT != 0)
		return read_tex(state, UINT2(uv));
	if (PS_POINT_SAMPLER != FALSE)
	{
		// Weird issue with ATI/AMD cards on DX,
		// it looks like they add 127/128 of a texel to sampling coordinates
		// occasionally causing point sampling to erroneously round up.
		// I'm manually adjusting coordinates to the centre of texels here,
		// though the centre is just paranoia, the top left corner works fine.
		// As of 2018 this issue is still present.
		uv = (trunc(uv * state.cb.wh.zw) + FLOAT2(0.5, 0.5)) / state.cb.wh.zw;
	}

	if (PS_ADJS == FALSE && PS_ADJT == FALSE)
	{
		uv *= state.cb.st_scale;
	}
	else
	{
		if (PS_ADJS != FALSE)
			uv.x = (uv.x - state.cb.st_range.x) * state.cb.st_range.z;
		else
			uv.x = uv.x * state.cb.st_scale.x;
		if (PS_ADJT != FALSE)
			uv.y = (uv.y - state.cb.st_range.y) * state.cb.st_range.w;
		else
			uv.y = uv.y * state.cb.st_scale.y;
	}

	if (PS_SW_ANISO > 1)
		return sample_c_af(state, uv, state.psin.t.w);
	else if (PS_AUTOMATIC_LOD != FALSE)
		return sample_tex(state, uv);
	else if (PS_MANUAL_LOD != FALSE)
		return sample_tex_lod(state, uv, manual_lod(state, state.psin.t.w));
	else
		return sample_tex_lod(state, uv, 0);
}

STATIC FLOAT4 sample_p(IN_PARAM(PSMainState, state), uint idx)
{
	return PS_READ_PALETTE(state, UINT2(idx, 0));
}

STATIC FLOAT4 sample_p_norm(IN_PARAM(PSMainState, state), float u)
{
	return sample_p(state, uint(u * 255.5f));
}

STATIC FLOAT4 clamp_wrap_uv(IN_PARAM(PSMainState, state), FLOAT4 uv)
{
	FLOAT4 tex_size = state.cb.wh.xyxy;

	if (PS_WMS == PS_WMT)
	{
		if (PS_REGION_RECT != FALSE && PS_WMS == 0)
		{
			uv = FRACT(uv);
		}
		else if (PS_REGION_RECT != FALSE && PS_WMS == 1)
		{
			uv = SATURATE(uv);
		}
		else if (PS_WMS == 2)
		{
			uv = clamp(uv, state.cb.uv_min_max.xyxy, state.cb.uv_min_max.zwzw);
		}
		else if (PS_WMS == 3)
		{
			// wrap negative uv coords to avoid an off by one error that shifted
			// textures. Fixes Xenosaga's hair issue.
			if (PS_FST == FALSE)
				uv = FRACT(uv);

			UINT4 uv_msk_fix = FLOAT4_BITCAST_UINT4(state.cb.uv_min_max);
			uv = FLOAT4((USHORT4(uv * tex_size) & USHORT4(uv_msk_fix.xyxy)) | USHORT4(uv_msk_fix.zwzw)) / tex_size;
		}
	}
	else
	{
		if (PS_REGION_RECT != FALSE && PS_WMS == 0)
		{
			uv.xz = FRACT(uv.xz);
		}
		else if (PS_REGION_RECT != FALSE && PS_WMS == 1)
		{
			uv.xz = SATURATE(uv.xz);
		}
		else if (PS_WMS == 2)
		{
			uv.xz = clamp(uv.xz, state.cb.uv_min_max.xx, state.cb.uv_min_max.zz);
		}
		else if (PS_WMS == 3)
		{
			if (PS_FST == FALSE)
				uv.xz = FRACT(uv.xz);

			UINT4 uv_msk_fix = FLOAT4_BITCAST_UINT4(state.cb.uv_min_max);
			uv.xz = FLOAT2((USHORT2(uv.xz * tex_size.xx) & USHORT2(uv_msk_fix.xx)) | USHORT2(uv_msk_fix.zz)) / tex_size.xx;
		}

		if (PS_REGION_RECT != FALSE && PS_WMT == 0)
		{
			uv.yw = FRACT(uv.yw);
		}
		else if (PS_REGION_RECT != FALSE && PS_WMT == 1)
		{
			uv.yw = SATURATE(uv.yw);
		}
		else if (PS_WMT == 2)
		{
			uv.yw = clamp(uv.yw, state.cb.uv_min_max.yy, state.cb.uv_min_max.ww);
		}
		else if (PS_WMT == 3)
		{
			if (PS_FST == FALSE)
				uv.yw = FRACT(uv.yw);

			UINT4 uv_msk_fix = FLOAT4_BITCAST_UINT4(state.cb.uv_min_max);
			uv.yw = FLOAT2((USHORT2(uv.yw * tex_size.yy) & USHORT2(uv_msk_fix.yy)) | USHORT2(uv_msk_fix.ww)) / tex_size.yy;
		}
	}

	if (PS_REGION_RECT != FALSE)
	{
		// Normalized -> Integer Coordinates.
		uv = clamp(uv * state.cb.wh.zwzw + state.cb.st_range.xyxy, state.cb.st_range.xyxy, state.cb.st_range.zwzw);
	}

	return uv;
}

STATIC FLOAT4x4 sample_4c(IN_PARAM(PSMainState, state), FLOAT4 uv)
{
	return FLOAT4x4(
		sample_c(state, uv.xy),
		sample_c(state, uv.zy),
		sample_c(state, uv.xw),
		sample_c(state, uv.zw)
	);
}

STATIC UINT4 sample_4_index(IN_PARAM(PSMainState, state), FLOAT4 uv)
{
	FLOAT4 c;

	// Either GS will send a texture that contains a single alpha channel
	// Or we have an old RT (ie RGBA8) that contains index (4/8) in the alpha channel

	// Note: texture gather can't be used because of special clamping/wrapping
	// Also it doesn't support lod
	c.x = sample_c(state, uv.xy).a;
	c.y = sample_c(state, uv.zy).a;
	c.z = sample_c(state, uv.xw).a;
	c.w = sample_c(state, uv.zw).a;
	
	UINT4 i;
	
	if (PS_RTA_SRC_CORRECTION != FALSE)
	{
		i = UINT4(round(c * 128.25f)); // Denormalize value
	}
	else
	{
		i = UINT4(c * 255.5f); // Denormalize value
	}
	
	if (PS_PAL_FMT == 1)
		return i & 0xFu;
	if (PS_PAL_FMT == 2)
		return i >> 4;

	return i;
}

STATIC FLOAT4x4 sample_4p(IN_PARAM(PSMainState, state), UINT4 u)
{
	return FLOAT4x4(
		sample_p(state, u.x),
		sample_p(state, u.y),
		sample_p(state, u.z),
		sample_p(state, u.w)
	);
}

STATIC FLOAT4 sample_color(IN_PARAM(PSMainState, state), FLOAT2 st)
{
	if (PS_TCOFFSETHACK != FALSE)
		st += state.cb.tc_offset;

	FLOAT4 t;
	FLOAT4x4 c;
	FLOAT2 dd;

	if (PS_LTF == FALSE && PS_AEM_FMT == FMT_32 && PS_PAL_FMT == 0 && PS_REGION_RECT == FALSE && PS_WMS < 2 && PS_WMT < 2)
	{
		c[0] = sample_c(state, st);
	}
	else
	{
		FLOAT4 uv;
		if (PS_LTF != FALSE)
		{
			uv = st.xyxy + state.cb.half_texel;
			dd = FRACT(uv.xy * state.cb.wh.zw);
			if (PS_FST == FALSE)
			{
				// Background in Shin Megami Tensei Lucifers
				// I suspect that uv isn't a standard number, so fract is outside of the [0;1] range
				dd = clamp(dd, float2_bcast(0.0f), float2_bcast(0.9999999f));
			}
		}
		else
		{
			uv = st.xyxy;
		}

		uv = clamp_wrap_uv(state, uv);

		if (PS_PAL_FMT != 0)
			c = sample_4p(state, sample_4_index(state, uv));
		else
			c = sample_4c(state, uv);
	}

	UNROLL
	for (int i = 0; i < 4; i++)
	{
		if (PS_AEM_FMT == FMT_24)
			c[i].a = (PS_AEM == FALSE || any_nonzero(c[i].rgb)) ? state.cb.ta.x : 0.f;
		else if (PS_AEM_FMT == FMT_16)
			c[i].a = c[i].a >= 0.5 ? state.cb.ta.y : PS_AEM == FALSE || any_nonzero(INT3(c[i].rgb * 255.0f) & int(0xF8)) ? state.cb.ta.x : 0.f;
	}

	if (PS_LTF != FALSE)
		t = MIX(MIX(c[0], c[1], dd.x), MIX(c[2], c[3], dd.x), dd.y);
	else
		t = c[0];

	if (PS_AEM_FMT == FMT_32 && PS_PAL_FMT == 0 && PS_RTA_SRC_CORRECTION != FALSE)
		t.a = t.a * (128.5f / 255.0f);
		
	// The 0.05f helps to fix the overbloom of sotc
	// I think the issue is related to the rounding of texture coodinate. The linear (from fixed unit)
	// interpolation could be slightly below the correct one.
	
	return trunc(t * 255.f + 0.05f);
}

STATIC USHORT2 clamp_wrap_uv_depth(IN_PARAM(PSMainState, state), USHORT2 uv)
{
	USHORT2 uv_out = uv;
	// Keep the full precision
	// It allow to multiply the ScalingFactor before the 1/16 coeff
	UINT4 uv_msk_fix = FLOAT4_BITCAST_UINT4(state.cb.uv_min_max);
	USHORT4 mask = USHORT4(uv_msk_fix << 4);

	if (PS_WMS == PS_WMT)
	{
		if (PS_WMS == 2)
			uv_out = clamp(uv, mask.xy, mask.zw);
		else if (PS_WMS == 3)
			uv_out = (uv & mask.xy) | mask.zw;
	}
	else
	{
		if (PS_WMS == 2)
			uv_out.x = clamp(uv.x, mask.x, mask.z);
		else if (PS_WMS == 3)
			uv_out.x = (uv.x & mask.x) | mask.z;

		if (PS_WMT == 2)
			uv_out.y = clamp(uv.y, mask.y, mask.w);
		else if (PS_WMT == 3)
			uv_out.y = (uv.y & mask.y) | mask.w;
	}

	return uv_out;
}

STATIC FLOAT4 sample_depth(IN_PARAM(PSMainState, state), FLOAT2 st)
{
	FLOAT2 uv_f = FLOAT2(clamp_wrap_uv_depth(state, USHORT2(st))) * float2_bcast(state.cb.scale_factor.x);

	if (PS_REGION_RECT != FALSE)
		uv_f = clamp(uv_f + state.cb.st_range.xy, state.cb.st_range.xy, state.cb.st_range.zw);

	USHORT2 uv = USHORT2(uv_f);
	FLOAT4 t = float4_bcast(0.0f);

	if (PS_TALES_OF_ABYSS_HLE != FALSE)
	{
		// Warning: UV can't be used in channel effect
		USHORT depth = fetch_raw_depth(state);
		// Convert msb based on the palette
		t = PS_READ_PALETTE(state, USHORT2((depth >> 8) & 0xFFu, 0)) * 255.f;
	}
	else if (PS_URBAN_CHAOS_HLE != FALSE)
	{
		// Depth buffer is read as a RGB5A1 texture. The game try to extract the green channel.
		// So it will do a first channel trick to extract lsb, value is right-shifted.
		// Then a new channel trick to extract msb which will shifted to the left.
		// OpenGL uses a FLOAT32 format for the depth so it requires a couple of conversion.
		// To be faster both steps (msb&lsb) are done in a single pass.

		// Warning: UV can't be used in channel effect
		USHORT depth = fetch_raw_depth(state);

		// Convert lsb based on the palette
		t = PS_READ_PALETTE(state, USHORT2(depth & 0xFFu, 0)) * 255.f;

		// Msb is easier
		float green = float((depth >> 8) & 0xFFu) * 36.f;
		green = min(green, 255.0f);

		t.g += green;
	}
	else if (PS_DEPTH_FMT == 1)
	{
		t = convert_depth32_rgba8(fetch_c(state, uv).r);
	}
	else if (PS_DEPTH_FMT == 2)
	{
		t = convert_depth16_rgba8(fetch_c(state, uv).r);
	}
	else if (PS_DEPTH_FMT == 3)
	{
		t = fetch_c(state, uv) * 255.f;
	}

	if (PS_AEM_FMT == FMT_24)
		t.a = (PS_AEM == FALSE || any_nonzero(t.rgb)) ? 255.f * state.cb.ta.x : 0.f;
	else if (PS_AEM_FMT == FMT_16)
		t.a = t.a >= 128.f ? 255.f * state.cb.ta.y : (PS_AEM == FALSE || any_nonzero(t.rgb)) ? 255.f * state.cb.ta.x : 0.f;
	else if (PS_PAL_FMT != 0 && PS_TALES_OF_ABYSS_HLE == FALSE && PS_URBAN_CHAOS_HLE == FALSE)
		t = trunc(sample_4p(state, UINT4(t.aaaa))[0] * 255.0f + 0.05f);
	
	return t;
}
