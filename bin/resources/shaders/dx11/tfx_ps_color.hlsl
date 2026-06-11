// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "tfx_defines.hlsl"

#ifndef PS_FST
#define PS_FST 0
#define PS_AEM 0
#define PS_SHUFFLE 0
#define PS_SHUFFLE_SAME 0
#define PS_PROCESS_BA 0
#define PS_PROCESS_RG 0
#define PS_READ16_SRC 0
#define PS_DEPTH_FMT 0
#define PS_CHANNEL_FETCH 0
#endif

#include "tfx_ps_resources.hlsl"

float4 fetch_red(int2 xy);
float4 fetch_green(int2 xy);
float4 fetch_blue(int2 xy);
float4 fetch_alpha(int2 xy);
float4 fetch_rgb(int2 xy);
float4 fetch_gXbY(int2 xy);

float4 sample_color(float2 st, float uv_w, int2 xy);
float4 sample_depth(float2 st, float2 pos);

float4 tfx(float4 T, float4 C);

float4 fog(float4 c, float f);

// Uses PS_INPUT
export float4 ps_color(float4 p, float4 t, float4 ti, float4 c)
{
#if PS_FST == 0
	float2 st = t.xy / t.w;
	float2 st_int = ti.zw / t.w;
#else
	float2 st = ti.xy;
	float2 st_int = ti.zw;
#endif

#if PS_CHANNEL_FETCH == 1
	float4 T = fetch_red(int2(p.xy + ChannelShuffleOffset));
#elif PS_CHANNEL_FETCH == 2
	float4 T = fetch_green(int2(p.xy + ChannelShuffleOffset));
#elif PS_CHANNEL_FETCH == 3
	float4 T = fetch_blue(int2(p.xy + ChannelShuffleOffset));
#elif PS_CHANNEL_FETCH == 4
	float4 T = fetch_alpha(int2(p.xy + ChannelShuffleOffset));
#elif PS_CHANNEL_FETCH == 5
	float4 T = fetch_rgb(int2(p.xy + ChannelShuffleOffset));
#elif PS_CHANNEL_FETCH == 6
	float4 T = fetch_gXbY(int2(p.xy + ChannelShuffleOffset));
#elif PS_DEPTH_FMT > 0
	float4 T = sample_depth(st_int, p.xy);
#else
	float4 T = sample_color(st, t.w, int2(p.xy));
#endif

	if (PS_SHUFFLE && !PS_SHUFFLE_SAME && !PS_READ16_SRC && !(PS_PROCESS_BA == SHUFFLE_READWRITE && PS_PROCESS_RG == SHUFFLE_READWRITE))
	{
		uint4 denorm_c_before = uint4(T);
		if (PS_PROCESS_BA & SHUFFLE_READ)
		{
			T.r = float((denorm_c_before.b << 3) & 0xF8u);
			T.g = float(((denorm_c_before.b >> 2) & 0x38u) | ((denorm_c_before.a << 6) & 0xC0u));
			T.b = float((denorm_c_before.a << 1) & 0xF8u);
			T.a = float(denorm_c_before.a & 0x80u);
		}
		else
		{
			T.r = float((denorm_c_before.r << 3) & 0xF8u);
			T.g = float(((denorm_c_before.r >> 2) & 0x38u) | ((denorm_c_before.g << 6) & 0xC0u));
			T.b = float((denorm_c_before.g << 1) & 0xF8u);
			T.a = float(denorm_c_before.g & 0x80u);
		}
		
		T.a = (T.a >= 127.5f ? TA.y : !PS_AEM || any(int3(T.rgb) & 0xF8) ? TA.x : 0) * 255.0f;
	}

	float4 C = tfx(T, c);

	C = fog(C, t.z);

	return C;
}
