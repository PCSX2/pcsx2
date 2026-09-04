PS_STATIC float manual_lod(float uv_w)
{
	float K = ps_cb.lod_params.x;
	float L = ps_cb.lod_params.y;
	float bias = ps_cb.lod_params.z;
	float max_lod = ps_cb.lod_params.w;
	float gs_lod = K - log2(abs(uv_w)) * L;
	return min(gs_lod, max_lod) - bias;
}

PS_STATIC float4 sample_c(float2 uv)
{
	if (PS_TEX_IS_FB != 0)
		return ps_current_color;
	if (PS_REGION_RECT != 0)
		return read_tex(uint2(uv));
	if (PS_POINT_SAMPLER != FALSE)
	{
		// Weird issue with ATI/AMD cards on DX,
		// it looks like they add 127/128 of a texel to sampling coordinates
		// occasionally causing point sampling to erroneously round up.
		// I'm manually adjusting coordinates to the centre of texels here,
		// though the centre is just paranoia, the top left corner works fine.
		// As of 2018 this issue is still present.
		uv = (trunc(uv * ps_cb.wh.zw) + float2(0.5, 0.5)) / ps_cb.wh.zw;
	}

	if (PS_ADJS == FALSE && PS_ADJT == FALSE)
	{
		uv *= ps_cb.st_scale;
	}
	else
	{
		if (PS_ADJS != FALSE)
			uv.x = (uv.x - ps_cb.st_range.x) * ps_cb.st_range.z;
		else
			uv.x = uv.x * ps_cb.st_scale.x;
		if (PS_ADJT != FALSE)
			uv.y = (uv.y - ps_cb.st_range.y) * ps_cb.st_range.w;
		else
			uv.y = uv.y * ps_cb.st_scale.y;
	}

	if (PS_SW_ANISO > 1)
		return sample_c_af(uv, ps_in.t.w);
	else if (PS_AUTOMATIC_LOD != FALSE)
		return sample_tex(uv);
	else if (PS_MANUAL_LOD != FALSE)
		return sample_tex_lod(uv, manual_lod(ps_in.t.w));
	else
		return sample_tex_lod(uv, 0);
}

PS_STATIC float4 sample_p(uint idx)
{
	return PS_READ_PALETTE(uint2(idx, 0));
}

PS_STATIC float4 sample_p_norm(float u)
{
	return sample_p(uint(u * 255.5f));
}

PS_STATIC float4 clamp_wrap_uv(float4 uv)
{
	float4 tex_size = ps_cb.wh.xyxy;

	if (PS_WMS == PS_WMT)
	{
		if (PS_REGION_RECT != FALSE && PS_WMS == 0)
		{
			uv = frac(uv);
		}
		else if (PS_REGION_RECT != FALSE && PS_WMS == 1)
		{
			uv = saturate(uv);
		}
		else if (PS_WMS == 2)
		{
			uv = clamp(uv, ps_cb.uv_min_max.xyxy, ps_cb.uv_min_max.zwzw);
		}
		else if (PS_WMS == 3)
		{
			// wrap negative uv coords to avoid an off by one error that shifted
			// textures. Fixes Xenosaga's hair issue.
			if (PS_FST == FALSE)
				uv = frac(uv);

			uint4 uv_msk_fix = FLOAT4_BITCAST_UINT4(ps_cb.uv_min_max);
			uv = float4((ushort4(uv * tex_size) & ushort4(uv_msk_fix.xyxy)) | ushort4(uv_msk_fix.zwzw)) / tex_size;
		}
	}
	else
	{
		if (PS_REGION_RECT != FALSE && PS_WMS == 0)
		{
			uv.xz = frac(uv.xz);
		}
		else if (PS_REGION_RECT != FALSE && PS_WMS == 1)
		{
			uv.xz = saturate(uv.xz);
		}
		else if (PS_WMS == 2)
		{
			uv.xz = clamp(uv.xz, ps_cb.uv_min_max.xx, ps_cb.uv_min_max.zz);
		}
		else if (PS_WMS == 3)
		{
			if (PS_FST == FALSE)
				uv.xz = frac(uv.xz);

			uint4 uv_msk_fix = FLOAT4_BITCAST_UINT4(ps_cb.uv_min_max);
			uv.xz = float2((ushort2(uv.xz * tex_size.xx) & ushort2(uv_msk_fix.xx)) | ushort2(uv_msk_fix.zz)) / tex_size.xx;
		}

		if (PS_REGION_RECT != FALSE && PS_WMT == 0)
		{
			uv.yw = frac(uv.yw);
		}
		else if (PS_REGION_RECT != FALSE && PS_WMT == 1)
		{
			uv.yw = saturate(uv.yw);
		}
		else if (PS_WMT == 2)
		{
			uv.yw = clamp(uv.yw, ps_cb.uv_min_max.yy, ps_cb.uv_min_max.ww);
		}
		else if (PS_WMT == 3)
		{
			if (PS_FST == FALSE)
				uv.yw = frac(uv.yw);

			uint4 uv_msk_fix = FLOAT4_BITCAST_UINT4(ps_cb.uv_min_max);
			uv.yw = float2((ushort2(uv.yw * tex_size.yy) & ushort2(uv_msk_fix.yy)) | ushort2(uv_msk_fix.ww)) / tex_size.yy;
		}
	}

	if (PS_REGION_RECT != FALSE)
	{
		// Normalized -> Integer Coordinates.
		uv = clamp(uv * ps_cb.wh.zwzw + ps_cb.st_range.xyxy, ps_cb.st_range.xyxy, ps_cb.st_range.zwzw);
	}

	return uv;
}

PS_STATIC float4x4 sample_4c(float4 uv)
{
	return float4x4(
		sample_c(uv.xy),
		sample_c(uv.zy),
		sample_c(uv.xw),
		sample_c(uv.zw)
	);
}

PS_STATIC uint4 sample_4_index(float4 uv)
{
	float4 c;

	// Either GS will send a texture that contains a single alpha channel
	// Or we have an old RT (ie RGBA8) that contains index (4/8) in the alpha channel

	// Note: texture gather can't be used because of special clamping/wrapping
	// Also it doesn't support lod
	c.x = sample_c(uv.xy).a;
	c.y = sample_c(uv.zy).a;
	c.z = sample_c(uv.xw).a;
	c.w = sample_c(uv.zw).a;
	
	uint4 i;
	
	if (PS_RTA_SRC_CORRECTION != FALSE)
	{
		i = uint4(round(c * 128.25f)); // Denormalize value
	}
	else
	{
		i = uint4(c * 255.5f); // Denormalize value
	}
	
	if (PS_PAL_FMT == 1)
		return i & 0xFu;
	if (PS_PAL_FMT == 2)
		return i >> 4;

	return i;
}

PS_STATIC float4x4 sample_4p(uint4 u)
{
	return float4x4(
		sample_p(u.x),
		sample_p(u.y),
		sample_p(u.z),
		sample_p(u.w)
	);
}

PS_STATIC float4 sample_color(float2 st)
{
	if (PS_TCOFFSETHACK != FALSE)
		st += ps_cb.tc_offset;

	float4 t;
	float4x4 c;
	float2 dd;

	if (PS_LTF == FALSE && PS_AEM_FMT == FMT_32 && PS_PAL_FMT == 0 && PS_REGION_RECT == FALSE && PS_WMS < 2 && PS_WMT < 2)
	{
		c[0] = sample_c(st);
	}
	else
	{
		float4 uv;
		if (PS_LTF != FALSE)
		{
			uv = st.xyxy + ps_cb.half_texel;
			dd = frac(uv.xy * ps_cb.wh.zw);
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

		uv = clamp_wrap_uv(uv);

		if (PS_PAL_FMT != 0)
			c = sample_4p(sample_4_index(uv));
		else
			c = sample_4c(uv);
	}

	UNROLL
	for (int i = 0; i < 4; i++)
	{
		if (PS_AEM_FMT == FMT_24)
			c[i].a = (PS_AEM == FALSE || any_nonzero(c[i].rgb)) ? ps_cb.ta.x : 0.f;
		else if (PS_AEM_FMT == FMT_16)
			c[i].a = c[i].a >= 0.5 ? ps_cb.ta.y : PS_AEM == FALSE || any_nonzero(int3(c[i].rgb * 255.0f) & int(0xF8)) ? ps_cb.ta.x : 0.f;
	}

	if (PS_LTF != FALSE)
		t = lerp(lerp(c[0], c[1], dd.x), lerp(c[2], c[3], dd.x), dd.y);
	else
		t = c[0];

	if (PS_AEM_FMT == FMT_32 && PS_PAL_FMT == 0 && PS_RTA_SRC_CORRECTION != FALSE)
		t.a = t.a * (128.5f / 255.0f);
		
	// The 0.05f helps to fix the overbloom of sotc
	// I think the issue is related to the rounding of texture coodinate. The linear (from fixed unit)
	// interpolation could be slightly below the correct one.
	
	return trunc(t * 255.f + 0.05f);
}

PS_STATIC ushort2 clamp_wrap_uv_depth(ushort2 uv)
{
	ushort2 uv_out = uv;
	// Keep the full precision
	// It allow to multiply the ScalingFactor before the 1/16 coeff
	uint4 uv_msk_fix = FLOAT4_BITCAST_UINT4(ps_cb.uv_min_max);
	ushort4 mask = ushort4(uv_msk_fix << 4);

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

PS_STATIC float4 sample_depth(float2 st)
{
	float2 uv_f = float2(clamp_wrap_uv_depth(ushort2(st))) * float2_bcast(ps_cb.scale_factor.x);

	if (PS_REGION_RECT != FALSE)
		uv_f = clamp(uv_f + ps_cb.st_range.xy, ps_cb.st_range.xy, ps_cb.st_range.zw);

	ushort2 uv = ushort2(uv_f);
	float4 t = float4_bcast(0.0f);

	if (PS_TALES_OF_ABYSS_HLE != FALSE)
	{
		// Warning: UV can't be used in channel effect
		ushort depth = fetch_raw_depth();
		// Convert msb based on the palette
		t = PS_READ_PALETTE(ushort2((depth >> 8) & 0xFFu, 0)) * 255.f;
	}
	else if (PS_URBAN_CHAOS_HLE != FALSE)
	{
		// Depth buffer is read as a RGB5A1 texture. The game try to extract the green channel.
		// So it will do a first channel trick to extract lsb, value is right-shifted.
		// Then a new channel trick to extract msb which will shifted to the left.
		// OpenGL uses a FLOAT32 format for the depth so it requires a couple of conversion.
		// To be faster both steps (msb&lsb) are done in a single pass.

		// Warning: UV can't be used in channel effect
		ushort depth = fetch_raw_depth();

		// Convert lsb based on the palette
		t = PS_READ_PALETTE(ushort2(depth & 0xFFu, 0)) * 255.f;

		// Msb is easier
		float green = float((depth >> 8) & 0xFFu) * 36.f;
		green = min(green, 255.0f);

		t.g += green;
	}
	else if (PS_DEPTH_FMT == 1)
	{
		t = convert_depth32_rgba8(fetch_c(uv).r);
	}
	else if (PS_DEPTH_FMT == 2)
	{
		t = convert_depth16_rgba8(fetch_c(uv).r);
	}
	else if (PS_DEPTH_FMT == 3)
	{
		t = fetch_c(uv) * 255.f;
	}

	if (PS_AEM_FMT == FMT_24)
		t.a = (PS_AEM == FALSE || any_nonzero(t.rgb)) ? 255.f * ps_cb.ta.x : 0.f;
	else if (PS_AEM_FMT == FMT_16)
		t.a = t.a >= 128.f ? 255.f * ps_cb.ta.y : (PS_AEM == FALSE || any_nonzero(t.rgb)) ? 255.f * ps_cb.ta.x : 0.f;
	else if (PS_PAL_FMT != 0 && PS_TALES_OF_ABYSS_HLE == FALSE && PS_URBAN_CHAOS_HLE == FALSE)
		t = trunc(sample_4p(uint4(t.aaaa))[0] * 255.0f + 0.05f);
	
	return t;
}
