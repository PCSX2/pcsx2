// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#ifndef PS_DEPTH_FMT
// fetch_red, fetch_green, fetch_blue, fetch_gXbY
#define PS_DEPTH_FMT 0
// fetch_raw_depth, fetch_raw_color, fetch_c
#define PS_TEX_IS_FB 0
#endif

#include "tfx_ps_resources.hlsl"

float4 RtLoad(int2 xy);

float4 sample_p_norm(float u);

uint fetch_raw_depth(int2 xy)
{
#if PS_TEX_IS_FB == 1
	float4 col = RtLoad(xy);
#else
	float4 col = Texture.Load(int3(xy, 0));
#endif
	return (uint)(col.r * exp2(32.0f));
}

float4 fetch_raw_color(int2 xy)
{
#if PS_TEX_IS_FB == 1
	return RtLoad(xy);
#else
	return Texture.Load(int3(xy, 0));
#endif
}

export float4 fetch_c(int2 uv)
{
#if PS_TEX_IS_FB == 1
	return RtLoad(uv);
#else
	return Texture.Load(int3(uv, 0));
#endif
}

export float4 fetch_red(int2 xy)
{
	float4 rt;

	if ((PS_DEPTH_FMT == 1) || (PS_DEPTH_FMT == 2))
	{
		uint depth = (fetch_raw_depth(xy)) & 0xFFu;
		rt = (float4)(depth) / 255.0f;
	}
	else
	{
		rt = fetch_raw_color(xy);
	}

	return sample_p_norm(rt.r) * 255.0f;
}

export float4 fetch_green(int2 xy)
{
	float4 rt;

	if ((PS_DEPTH_FMT == 1) || (PS_DEPTH_FMT == 2))
	{
		uint depth = (fetch_raw_depth(xy) >> 8u) & 0xFFu;
		rt = (float4)(depth) / 255.0f;
	}
	else
	{
		rt = fetch_raw_color(xy);
	}

	return sample_p_norm(rt.g) * 255.0f;
}

export float4 fetch_blue(int2 xy)
{
	float4 rt;

	if ((PS_DEPTH_FMT == 1) || (PS_DEPTH_FMT == 2))
	{
		uint depth = (fetch_raw_depth(xy) >> 16u) & 0xFFu;
		rt = (float4)(depth) / 255.0f;
	}
	else
	{
		rt = fetch_raw_color(xy);
	}

	return sample_p_norm(rt.b) * 255.0f;
}

export float4 fetch_alpha(int2 xy)
{
	float4 rt = fetch_raw_color(xy);
	return sample_p_norm(rt.a) * 255.0f;
}

export float4 fetch_rgb(int2 xy)
{
	float4 rt = fetch_raw_color(xy);
	float4 c = float4(sample_p_norm(rt.r).r, sample_p_norm(rt.g).g, sample_p_norm(rt.b).b, 1.0);
	return c * 255.0f;
}

export float4 fetch_gXbY(int2 xy)
{
	if ((PS_DEPTH_FMT == 1) || (PS_DEPTH_FMT == 2))
	{
		uint depth = fetch_raw_depth(xy);
		uint bg = (depth >> (8u + uint(ChannelShuffle.w))) & 0xFFu;
		return (float4)(bg);
	}
	else
	{
		int4 rt = (int4)(fetch_raw_color(xy) * 255.0);
		int green = (rt.g >> ChannelShuffle.w) & ChannelShuffle.z;
		int blue = (rt.b << ChannelShuffle.y) & ChannelShuffle.x;
		return (float4)(green | blue);
	}
}
