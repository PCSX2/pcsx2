// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "tfx_ps_resources.hlsl"
#include "tfx_defines.hlsl"

#ifndef PS_FST
// clamp_wrap_uv, sample_color
#define PS_FST 0
// clamp_wrap_uv, sample_color, clamp_wrap_uv_depth
#define PS_WMS 0
// clamp_wrap_uv, sample_color, clamp_wrap_uv_depth
#define PS_WMT 0
// sample_c
#define PS_ADJS 0
// sample_c
#define PS_ADJT 0
// sample_color, sample_depth
#define PS_AEM_FMT FMT_32
// sample_color, sample_depth
#define PS_AEM 0
// sample_color
#define PS_LTF 1
// sample_color
#define PS_TCOFFSETHACK 0
// sample_depth
#define PS_DEPTH_FMT 0
// sample_4_index, sample_color, sample_depth
#define PS_PAL_FMT 0
// sample_depth
#define PS_TALES_OF_ABYSS_HLE 0
// sample_depth
#define PS_URBAN_CHAOS_HLE 0
// sample_4_index, sample_color
#define PS_RTA_SRC_CORRECTION 0
// sample_c
#define PS_POINT_SAMPLER 0
// sample_c, clamp_wrap_uv, sample_color, sample_depth
#define PS_REGION_RECT 0
// manual_lod, sample_c
#define PS_AUTOMATIC_LOD 0
// manual_lod, sample_c
#define PS_MANUAL_LOD 0
// sample_c
#define PS_TEX_IS_FB 0
// sample_c_af, sample_c
#define PS_ANISOTROPIC_FILTERING 0
#endif

#if PS_ANISOTROPIC_FILTERING > 1
float4 sample_c_af(float2 uv, float uv_w);
#endif

float4 RtLoad(int2 xy);

float4 fetch_c(int2 uv);
uint fetch_raw_depth(int2 xy);

#if (PS_AUTOMATIC_LOD != 1) && (PS_MANUAL_LOD == 1)
float manual_lod(float uv_w)
{
	// FIXME add LOD: K - ( LOG2(Q) * (1 << L))
	float K = LODParams.x;
	float L = LODParams.y;
	float bias = LODParams.z;
	float max_lod = LODParams.w;

	float gs_lod = K - log2(abs(uv_w)) * L;
	// FIXME max useful ?
	//return max(min(gs_lod, max_lod) - bias, 0.0f);
	return min(gs_lod, max_lod) - bias;
}
#endif

float4 sample_c(float2 uv, float uv_w, int2 xy)
{
#if PS_TEX_IS_FB == 1
	return RtLoad(xy);
#elif PS_REGION_RECT == 1
	return Texture.Load(int3(int2(uv), 0));
#else
	if (PS_POINT_SAMPLER)
	{
		// Weird issue with ATI/AMD cards,
		// it looks like they add 127/128 of a texel to sampling coordinates
		// occasionally causing point sampling to erroneously round up.
		// I'm manually adjusting coordinates to the centre of texels here,
		// though the centre is just paranoia, the top left corner works fine.
		// As of 2018 this issue is still present.
		uv = (trunc(uv * WH.zw) + float2(0.5, 0.5)) / WH.zw;
	}
#if !PS_ADJS && !PS_ADJT
	uv *= STScale;
#else
	#if PS_ADJS
		uv.x = (uv.x - STRange.x) * STRange.z;
	#else
		uv.x = uv.x * STScale.x;
	#endif
	#if PS_ADJT
		uv.y = (uv.y - STRange.y) * STRange.w;
	#else
		uv.y = uv.y * STScale.y;
	#endif
#endif

#if PS_ANISOTROPIC_FILTERING > 1
	return sample_c_af(uv, uv_w);
#elif PS_AUTOMATIC_LOD == 1
	return Texture.Sample(TextureSampler, uv);
#elif PS_MANUAL_LOD == 1
	return Texture.SampleLevel(TextureSampler, uv, manual_lod(uv_w));
#else
	return Texture.SampleLevel(TextureSampler, uv, 0); // No lod
#endif
#endif
}

float4 sample_p(uint u)
{
	return Palette.Load(int3(int(u), 0, 0));
}

export float4 sample_p_norm(float u)
{
	return sample_p(uint(u * 255.5f));
}

float4 clamp_wrap_uv(float4 uv)
{
	float4 tex_size = WH.xyxy;

	if(PS_WMS == PS_WMT)
	{
		if(PS_REGION_RECT != 0 && PS_WMS == 0)
		{
			uv = frac(uv);
		}
		else if(PS_REGION_RECT != 0 && PS_WMS == 1)
		{
			uv = saturate(uv);
		}
		else if(PS_WMS == 2)
		{
			uv = clamp(uv, MinMax.xyxy, MinMax.zwzw);
		}
		else if(PS_WMS == 3)
		{
			#if PS_FST == 0
			// wrap negative uv coords to avoid an off by one error that shifted
			// textures. Fixes Xenosaga's hair issue.
			uv = frac(uv);
			#endif
			uv = (float4)(((uint4)(uv * tex_size) & asuint(MinMax.xyxy)) | asuint(MinMax.zwzw)) / tex_size;
		}
	}
	else
	{
		if(PS_REGION_RECT != 0 && PS_WMS == 0)
		{
			uv.xz = frac(uv.xz);
		}
		else if(PS_REGION_RECT != 0 && PS_WMS == 1)
		{
			uv.xz = saturate(uv.xz);
		}
		else if(PS_WMS == 2)
		{
			uv.xz = clamp(uv.xz, MinMax.xx, MinMax.zz);
		}
		else if(PS_WMS == 3)
		{
			#if PS_FST == 0
			uv.xz = frac(uv.xz);
			#endif
			uv.xz = (float2)(((uint2)(uv.xz * tex_size.xx) & asuint(MinMax.xx)) | asuint(MinMax.zz)) / tex_size.xx;
		}
		if(PS_REGION_RECT != 0 && PS_WMT == 0)
		{
			uv.yw = frac(uv.yw);
		}
		else if(PS_REGION_RECT != 0 && PS_WMT == 1)
		{
			uv.yw = saturate(uv.yw);
		}
		else if(PS_WMT == 2)
		{
			uv.yw = clamp(uv.yw, MinMax.yy, MinMax.ww);
		}
		else if(PS_WMT == 3)
		{
			#if PS_FST == 0
			uv.yw = frac(uv.yw);
			#endif
			uv.yw = (float2)(((uint2)(uv.yw * tex_size.yy) & asuint(MinMax.yy)) | asuint(MinMax.ww)) / tex_size.yy;
		}
	}

	if(PS_REGION_RECT != 0)
	{
		// Normalized -> Integer Coordinates.
		uv = clamp(uv * WH.zwzw + STRange.xyxy, STRange.xyxy, STRange.zwzw);
	}

	return uv;
}

float4x4 sample_4c(float4 uv, float uv_w, int2 xy)
{
	float4x4 c;

	c[0] = sample_c(uv.xy, uv_w, xy);
	c[1] = sample_c(uv.zy, uv_w, xy);
	c[2] = sample_c(uv.xw, uv_w, xy);
	c[3] = sample_c(uv.zw, uv_w, xy);

	return c;
}

uint4 sample_4_index(float4 uv, float uv_w, int2 xy)
{
	float4 c;

	c.x = sample_c(uv.xy, uv_w, xy).a;
	c.y = sample_c(uv.zy, uv_w, xy).a;
	c.z = sample_c(uv.xw, uv_w, xy).a;
	c.w = sample_c(uv.zw, uv_w, xy).a;

	// Denormalize value
	uint4 i;

	if (PS_RTA_SRC_CORRECTION)
	{
		i = uint4(round(c * 128.25f)); // Denormalize value
	}
	else
	{
		i = uint4(c * 255.5f); // Denormalize value
	}

	if (PS_PAL_FMT == 1)
	{
		// 4HL
		return i & 0xFu;
	}
	else if (PS_PAL_FMT == 2)
	{
		// 4HH
		return i >> 4u;
	}
	else
	{
		// 8
		return i;
	}
}

float4x4 sample_4p(uint4 u)
{
	float4x4 c;

	c[0] = sample_p(u.x);
	c[1] = sample_p(u.y);
	c[2] = sample_p(u.z);
	c[3] = sample_p(u.w);

	return c;
}

export float4 sample_color(float2 st, float uv_w, int2 xy)
{
#if PS_TCOFFSETHACK
	st += TC_OffsetHack.xy;
#endif

	float4 t;
	float4x4 c;
	float2 dd;

	if (PS_LTF == 0 && PS_AEM_FMT == FMT_32 && PS_PAL_FMT == 0 && PS_REGION_RECT == 0 && PS_WMS < 2 && PS_WMT < 2)
	{
		c[0] = sample_c(st, uv_w, xy);
	}
	else
	{
		float4 uv;

		if (PS_LTF)
		{
			uv = st.xyxy + HalfTexel;
			dd = frac(uv.xy * WH.zw);

			if (PS_FST == 0)
			{
				dd = clamp(dd, (float2) 0.0f, (float2) 0.9999999f);
			}
		}
		else
		{
			uv = st.xyxy;
		}

		uv = clamp_wrap_uv(uv);

#if PS_PAL_FMT != 0
		c = sample_4p(sample_4_index(uv, uv_w, xy));
#else
			c = sample_4c(uv, uv_w, xy);
#endif
	}

	[unroll]
	for (uint i = 0; i < 4; i++)
	{
		if (PS_AEM_FMT == FMT_24)
		{
			c[i].a = !PS_AEM || any(c[i].rgb) ? TA.x : 0;
		}
		else if (PS_AEM_FMT == FMT_16)
		{
			c[i].a = c[i].a >= 0.5 ? TA.y : !PS_AEM || any(int3(c[i].rgb * 255.0f) & 0xF8) ? TA.x : 0;
		}
	}

	if (PS_LTF)
	{
		t = lerp(lerp(c[0], c[1], dd.x), lerp(c[2], c[3], dd.x), dd.y);
	}
	else
	{
		t = c[0];
	}

	if (PS_AEM_FMT == FMT_32 && PS_PAL_FMT == 0 && PS_RTA_SRC_CORRECTION)
		t.a = t.a * (128.5f / 255.0f);
			
	return trunc(t * 255.0f + 0.05f);
}

//////////////////////////////////////////////////////////////////////
// Depth sampling
//////////////////////////////////////////////////////////////////////

int2 clamp_wrap_uv_depth(int2 uv)
{
	int4 mask = asint(MinMax) << 4;
	if (PS_WMS == PS_WMT)
	{
		if (PS_WMS == 2)
		{
			uv = clamp(uv, mask.xy, mask.zw);
		}
		else if (PS_WMS == 3)
		{
			uv = (uv & mask.xy) | mask.zw;
		}
	}
	else
	{
		if (PS_WMS == 2)
		{
			uv.x = clamp(uv.x, mask.x, mask.z);
		}
		else if (PS_WMS == 3)
		{
			uv.x = (uv.x & mask.x) | mask.z;
		}
		if (PS_WMT == 2)
		{
			uv.y = clamp(uv.y, mask.y, mask.w);
		}
		else if (PS_WMT == 3)
		{
			uv.y = (uv.y & mask.y) | mask.w;
		}
	}
	return uv;
}

export float4 sample_depth(float2 st, float2 pos)
{
	float2 uv_f = (float2)clamp_wrap_uv_depth(int2(st)) * (float2)ScaledScaleFactor;

#if PS_REGION_RECT == 1
	uv_f = clamp(uv_f + STRange.xy, STRange.xy, STRange.zw);
#endif

	int2 uv = (int2)uv_f;
	float4 t = (float4)(0.0f);

	if (PS_TALES_OF_ABYSS_HLE == 1)
	{
		// Warning: UV can't be used in channel effect
		uint depth = fetch_raw_depth(pos);

		// Convert msb based on the palette
		t = Palette.Load(int3((depth >> 8u) & 0xFFu, 0, 0)) * 255.0f;
	}
	else if (PS_URBAN_CHAOS_HLE == 1)
	{
		// Depth buffer is read as a RGB5A1 texture. The game try to extract the green channel.
		// So it will do a first channel trick to extract lsb, value is right-shifted.
		// Then a new channel trick to extract msb which will shifted to the left.
		// OpenGL uses a FLOAT32 format for the depth so it requires a couple of conversion.
		// To be faster both steps (msb&lsb) are done in a single pass.

		// Warning: UV can't be used in channel effect
		uint depth = fetch_raw_depth(pos);

		// Convert lsb based on the palette
		t = Palette.Load(int3(depth & 0xFFu, 0, 0)) * 255.0f;

		// Msb is easier
		float green = (float)((depth >> 8u) & 0xFFu) * 36.0f;
		green = min(green, 255.0f);
		t.g += green;
	}
	else if (PS_DEPTH_FMT == 1)
	{
		// Based on ps_convert_depth32_rgba8 of convert

		// Convert a FLOAT32 depth texture into a RGBA color texture
		uint d = uint(fetch_c(uv).r * exp2(32.0f));
		t = float4(uint4((d & 0xFFu), ((d >> 8) & 0xFFu), ((d >> 16) & 0xFFu), (d >> 24)));
	}
	else if (PS_DEPTH_FMT == 2)
	{
		// Based on ps_convert_depth16_rgb5a1 of convert

		// Convert a FLOAT32 (only 16 lsb) depth into a RGB5A1 color texture
		uint d = uint(fetch_c(uv).r * exp2(32.0f));
		t = float4(uint4((d & 0x1Fu), ((d >> 5) & 0x1Fu), ((d >> 10) & 0x1Fu), (d >> 15) & 0x01u)) * float4(8.0f, 8.0f, 8.0f, 128.0f);
	}
	else if (PS_DEPTH_FMT == 3)
	{
		// Convert a RGBA/RGB5A1 color texture into a RGBA/RGB5A1 color texture
		t = fetch_c(uv) * 255.0f;
	}

	if (PS_AEM_FMT == FMT_24)
	{
		t.a = ((PS_AEM == 0) || any(bool3(t.rgb))) ? 255.0f * TA.x : 0.0f;
	}
	else if (PS_AEM_FMT == FMT_16)
	{
		t.a = t.a >= 128.0f ? 255.0f * TA.y : ((PS_AEM == 0) || any(bool3(t.rgb))) ? 255.0f * TA.x : 0.0f;
	}
	else if (PS_PAL_FMT != 0 && !PS_TALES_OF_ABYSS_HLE && !PS_URBAN_CHAOS_HLE)
	{
		t = trunc(sample_4p(uint4(t.aaaa))[0] * 255.0f + 0.05f);
	}

	return t;
}
