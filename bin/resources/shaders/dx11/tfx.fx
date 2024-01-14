// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#define FMT_32 0
#define FMT_24 1
#define FMT_16 2

#ifndef VS_TME
#define VS_IIP 0
#define VS_TME 1
#define VS_FST 1
#endif

#ifndef GS_IIP
#define GS_IIP 0
#define GS_PRIM 3
#define GS_FORWARD_PRIMID 0
#endif

#ifndef PS_FST
#define PS_IIP 0
#define PS_FST 0
#define PS_WMS 0
#define PS_WMT 0
#define PS_ADJS 0
#define PS_ADJT 0
#define PS_AEM_FMT FMT_32
#define PS_AEM 0
#define PS_TFX 0
#define PS_TCC 1
#define PS_ATST 1
#define PS_FOG 0
#define PS_IIP 0
#define PS_BLEND_HW 0
#define PS_A_MASKED 0
#define PS_FBA 0
#define PS_FBMASK 0
#define PS_LTF 1
#define PS_TCOFFSETHACK 0
#define PS_POINT_SAMPLER 0
#define PS_REGION_RECT 0
#define PS_SHUFFLE 0
#define PS_SHUFFLE_SAME 0
#define PS_READ_BA 0
#define PS_READ16_SRC 0
#define PS_DST_FMT 0
#define PS_DEPTH_FMT 0
#define PS_PAL_FMT 0
#define PS_CHANNEL_FETCH 0
#define PS_TALES_OF_ABYSS_HLE 0
#define PS_URBAN_CHAOS_HLE 0
#define PS_HDR 0
#define PS_COLCLIP 0
#define PS_BLEND_A 0
#define PS_BLEND_B 0
#define PS_BLEND_C 0
#define PS_BLEND_D 0
#define PS_BLEND_MIX 0
#define PS_ROUND_INV 0
#define PS_FIXED_ONE_A 0
#define PS_PABE 0
#define PS_DITHER 0
#define PS_ZCLAMP 0
#define PS_SCANMSK 0
#define PS_AUTOMATIC_LOD 0
#define PS_MANUAL_LOD 0
#define PS_TEX_IS_FB 0
#define PS_NO_COLOR 0
#define PS_NO_COLOR1 0
#define PS_NO_ABLEND 0
#define PS_ONLY_ALPHA 0
#define PS_DATE 0
#endif

#define SW_BLEND (PS_BLEND_A || PS_BLEND_B || PS_BLEND_D)
#define SW_BLEND_NEEDS_RT (SW_BLEND && (PS_BLEND_A == 1 || PS_BLEND_B == 1 || PS_BLEND_C == 1 || PS_BLEND_D == 1))
#define SW_AD_TO_HW (PS_BLEND_C == 1 && PS_A_MASKED)

struct VS_INPUT
{
	float2 st : TEXCOORD0;
	uint4 c : COLOR0;
	float q : TEXCOORD1;
	uint2 p : POSITION0;
	uint z : POSITION1;
	uint2 uv : TEXCOORD2;
	float4 f : COLOR1;
};

struct VS_OUTPUT
{
	float4 p : SV_Position;
	float4 t : TEXCOORD0;
	float4 ti : TEXCOORD2;

#if VS_IIP != 0 || GS_IIP != 0 || PS_IIP != 0
	float4 c : COLOR0;
#else
	nointerpolation float4 c : COLOR0;
#endif
};

struct PS_INPUT
{
	float4 p : SV_Position;
	float4 t : TEXCOORD0;
	float4 ti : TEXCOORD2;
#if VS_IIP != 0 || GS_IIP != 0 || PS_IIP != 0
	float4 c : COLOR0;
#else
	nointerpolation float4 c : COLOR0;
#endif
#if (PS_DATE >= 1 && PS_DATE <= 3) || GS_FORWARD_PRIMID
	uint primid : SV_PrimitiveID;
#endif
};

#ifdef PIXEL_SHADER

struct PS_OUTPUT
{
#if !PS_NO_COLOR
#if PS_DATE == 1 || PS_DATE == 2
	float c : SV_Target;
#else
	float4 c0 : SV_Target0;
#if !PS_NO_COLOR1
	float4 c1 : SV_Target1;
#endif
#endif
#endif
#if PS_ZCLAMP
	float depth : SV_Depth;
#endif
};

Texture2D<float4> Texture : register(t0);
Texture2D<float4> Palette : register(t1);
Texture2D<float4> RtTexture : register(t2);
Texture2D<float> PrimMinTexture : register(t3);
SamplerState TextureSampler : register(s0);

#ifdef DX12
cbuffer cb1 : register(b1)
#else
cbuffer cb1
#endif
{
	float3 FogColor;
	float AREF;
	float4 WH;
	float2 TA;
	float MaxDepthPS;
	float Af;
	uint4 FbMask;
	float4 HalfTexel;
	float4 MinMax;
	float4 STRange;
	int4 ChannelShuffle;
	float2 TC_OffsetHack;
	float2 STScale;
	float4x4 DitherMatrix;
	float ScaledScaleFactor;
	float RcpScaleFactor;
};

float4 sample_c(float2 uv, float uv_w)
{
#if PS_TEX_IS_FB == 1
	return RtTexture.Load(int3(int2(uv * WH.zw), 0));
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

#if PS_AUTOMATIC_LOD == 1
	return Texture.Sample(TextureSampler, uv);
#elif PS_MANUAL_LOD == 1
	// FIXME add LOD: K - ( LOG2(Q) * (1 << L))
	float K = MinMax.x;
	float L = MinMax.y;
	float bias = MinMax.z;
	float max_lod = MinMax.w;

	float gs_lod = K - log2(abs(uv_w)) * L;
	// FIXME max useful ?
	//float lod = max(min(gs_lod, max_lod) - bias, 0.0f);
	float lod = min(gs_lod, max_lod) - bias;

	return Texture.SampleLevel(TextureSampler, uv, lod);
#else
	return Texture.SampleLevel(TextureSampler, uv, 0); // No lod
#endif
#endif
}

float4 sample_p(uint u)
{
	return Palette.Load(int3(int(u), 0, 0));
}

float4 sample_p_norm(float u)
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

float4x4 sample_4c(float4 uv, float uv_w)
{
	float4x4 c;

	c[0] = sample_c(uv.xy, uv_w);
	c[1] = sample_c(uv.zy, uv_w);
	c[2] = sample_c(uv.xw, uv_w);
	c[3] = sample_c(uv.zw, uv_w);

	return c;
}

uint4 sample_4_index(float4 uv, float uv_w)
{
	float4 c;

	c.x = sample_c(uv.xy, uv_w).a;
	c.y = sample_c(uv.zy, uv_w).a;
	c.z = sample_c(uv.xw, uv_w).a;
	c.w = sample_c(uv.zw, uv_w).a;

	// Denormalize value
	uint4 i = uint4(c * 255.5f);

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

int fetch_raw_depth(int2 xy)
{
#if PS_TEX_IS_FB == 1
	float4 col = RtTexture.Load(int3(xy, 0));
#else
	float4 col = Texture.Load(int3(xy, 0));
#endif
	return (int)(col.r * exp2(32.0f));
}

float4 fetch_raw_color(int2 xy)
{
#if PS_TEX_IS_FB == 1
	return RtTexture.Load(int3(xy, 0));
#else
	return Texture.Load(int3(xy, 0));
#endif
}

float4 fetch_c(int2 uv)
{
	return Texture.Load(int3(uv, 0));
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

float4 sample_depth(float2 st, float2 pos)
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
		int depth = fetch_raw_depth(pos);

		// Convert msb based on the palette
		t = Palette.Load(int3((depth >> 8) & 0xFF, 0, 0)) * 255.0f;
	}
	else if (PS_URBAN_CHAOS_HLE == 1)
	{
		// Depth buffer is read as a RGB5A1 texture. The game try to extract the green channel.
		// So it will do a first channel trick to extract lsb, value is right-shifted.
		// Then a new channel trick to extract msb which will shifted to the left.
		// OpenGL uses a FLOAT32 format for the depth so it requires a couple of conversion.
		// To be faster both steps (msb&lsb) are done in a single pass.

		// Warning: UV can't be used in channel effect
		int depth = fetch_raw_depth(pos);

		// Convert lsb based on the palette
		t = Palette.Load(int3(depth & 0xFF, 0, 0)) * 255.0f;

		// Msb is easier
		float green = (float)((depth >> 8) & 0xFF) * 36.0f;
		green = min(green, 255.0f);
		t.g += green;
	}
	else if (PS_DEPTH_FMT == 1)
	{
		// Based on ps_convert_float32_rgba8 of convert

		// Convert a FLOAT32 depth texture into a RGBA color texture
		uint d = uint(fetch_c(uv).r * exp2(32.0f));
		t = float4(uint4((d & 0xFFu), ((d >> 8) & 0xFFu), ((d >> 16) & 0xFFu), (d >> 24)));
	}
	else if (PS_DEPTH_FMT == 2)
	{
		// Based on ps_convert_float16_rgb5a1 of convert

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

	return t;
}

//////////////////////////////////////////////////////////////////////
// Fetch a Single Channel
//////////////////////////////////////////////////////////////////////

float4 fetch_red(int2 xy)
{
	float4 rt;

	if ((PS_DEPTH_FMT == 1) || (PS_DEPTH_FMT == 2))
	{
		int depth = (fetch_raw_depth(xy)) & 0xFF;
		rt = (float4)(depth) / 255.0f;
	}
	else
	{
		rt = fetch_raw_color(xy);
	}

	return sample_p_norm(rt.r) * 255.0f;
}

float4 fetch_green(int2 xy)
{
	float4 rt;

	if ((PS_DEPTH_FMT == 1) || (PS_DEPTH_FMT == 2))
	{
		int depth = (fetch_raw_depth(xy) >> 8) & 0xFF;
		rt = (float4)(depth) / 255.0f;
	}
	else
	{
		rt = fetch_raw_color(xy);
	}

	return sample_p_norm(rt.g) * 255.0f;
}

float4 fetch_blue(int2 xy)
{
	float4 rt;

	if ((PS_DEPTH_FMT == 1) || (PS_DEPTH_FMT == 2))
	{
		int depth = (fetch_raw_depth(xy) >> 16) & 0xFF;
		rt = (float4)(depth) / 255.0f;
	}
	else
	{
		rt = fetch_raw_color(xy);
	}

	return sample_p_norm(rt.b) * 255.0f;
}

float4 fetch_alpha(int2 xy)
{
	float4 rt = fetch_raw_color(xy);
	return sample_p_norm(rt.a) * 255.0f;
}

float4 fetch_rgb(int2 xy)
{
	float4 rt = fetch_raw_color(xy);
	float4 c = float4(sample_p_norm(rt.r).r, sample_p_norm(rt.g).g, sample_p_norm(rt.b).b, 1.0);
	return c * 255.0f;
}

float4 fetch_gXbY(int2 xy)
{
	if ((PS_DEPTH_FMT == 1) || (PS_DEPTH_FMT == 2))
	{
		int depth = fetch_raw_depth(xy);
		int bg = (depth >> (8 + ChannelShuffle.w)) & 0xFF;
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

float4 sample_color(float2 st, float uv_w)
{
	#if PS_TCOFFSETHACK
	st += TC_OffsetHack.xy;
	#endif

	float4 t;
	float4x4 c;
	float2 dd;

	if (PS_LTF == 0 && PS_AEM_FMT == FMT_32 && PS_PAL_FMT == 0 && PS_REGION_RECT == 0 && PS_WMS < 2 && PS_WMT < 2)
	{
		c[0] = sample_c(st, uv_w);
	}
	else
	{
		float4 uv;

		if(PS_LTF)
		{
			uv = st.xyxy + HalfTexel;
			dd = frac(uv.xy * WH.zw);

			if(PS_FST == 0)
			{
				dd = clamp(dd, (float2)0.0f, (float2)0.9999999f);
			}
		}
		else
		{
			uv = st.xyxy;
		}

		uv = clamp_wrap_uv(uv);

#if PS_PAL_FMT != 0
			c = sample_4p(sample_4_index(uv, uv_w));
#else
			c = sample_4c(uv, uv_w);
#endif
	}

	[unroll]
	for (uint i = 0; i < 4; i++)
	{
		if(PS_AEM_FMT == FMT_24)
		{
			c[i].a = !PS_AEM || any(c[i].rgb) ? TA.x : 0;
		}
		else if(PS_AEM_FMT == FMT_16)
		{
			c[i].a = c[i].a >= 0.5 ? TA.y : !PS_AEM || any(int3(c[i].rgb * 255.0f) & 0xF8) ? TA.x : 0;
		}
	}

	if(PS_LTF)
	{
		t = lerp(lerp(c[0], c[1], dd.x), lerp(c[2], c[3], dd.x), dd.y);
	}
	else
	{
		t = c[0];
	}

	return trunc(t * 255.0f + 0.05f);
}

float4 tfx(float4 T, float4 C)
{
	float4 C_out;
	float4 FxT = trunc((C * T) / 128.0f);

#if (PS_TFX == 0)
	C_out = FxT;
#elif (PS_TFX == 1)
	C_out = T;
#elif (PS_TFX == 2)
	C_out.rgb = FxT.rgb + C.a;
	C_out.a = T.a + C.a;
#elif (PS_TFX == 3)
	C_out.rgb = FxT.rgb + C.a;
	C_out.a = T.a;
#else
	C_out = C;
#endif

#if (PS_TCC == 0)
	C_out.a = C.a;
#endif

#if (PS_TFX == 0) || (PS_TFX == 2) || (PS_TFX == 3)
	// Clamp only when it is useful
	C_out = min(C_out, 255.0f);
#endif

	return C_out;
}

void atst(float4 C)
{
	float a = C.a;

	if(PS_ATST == 0)
	{
		// nothing to do
	}
	else if(PS_ATST == 1)
	{
		if (a > AREF) discard;
	}
	else if(PS_ATST == 2)
	{
		if (a < AREF) discard;
	}
	else if(PS_ATST == 3)
	{
		 if (abs(a - AREF) > 0.5f) discard;
	}
	else if(PS_ATST == 4)
	{
		if (abs(a - AREF) < 0.5f) discard;
	}
}

float4 fog(float4 c, float f)
{
	if(PS_FOG)
	{
		c.rgb = trunc(lerp(FogColor, c.rgb, f));
	}

	return c;
}

float4 ps_color(PS_INPUT input)
{
#if PS_FST == 0
	float2 st = input.t.xy / input.t.w;
	float2 st_int = input.ti.zw / input.t.w;
#else
	float2 st = input.ti.xy;
	float2 st_int = input.ti.zw;
#endif

#if PS_CHANNEL_FETCH == 1
	float4 T = fetch_red(int2(input.p.xy));
#elif PS_CHANNEL_FETCH == 2
	float4 T = fetch_green(int2(input.p.xy));
#elif PS_CHANNEL_FETCH == 3
	float4 T = fetch_blue(int2(input.p.xy));
#elif PS_CHANNEL_FETCH == 4
	float4 T = fetch_alpha(int2(input.p.xy));
#elif PS_CHANNEL_FETCH == 5
	float4 T = fetch_rgb(int2(input.p.xy));
#elif PS_CHANNEL_FETCH == 6
	float4 T = fetch_gXbY(int2(input.p.xy));
#elif PS_DEPTH_FMT > 0
	float4 T = sample_depth(st_int, input.p.xy);
#else
	float4 T = sample_color(st, input.t.w);
#endif

	if (PS_SHUFFLE && !PS_SHUFFLE_SAME && !PS_READ16_SRC)
	{
		uint4 denorm_c_before = uint4(T);
		if (PS_READ_BA)
		{
			T.r = float((denorm_c_before.b << 3) & 0xF8);
			T.g = float(((denorm_c_before.b >> 2) & 0x38) | ((denorm_c_before.a << 6) & 0xC0));
			T.b = float((denorm_c_before.a << 1) & 0xF8);
			T.a = float(denorm_c_before.a & 0x80);
		}
		else
		{
			T.r = float((denorm_c_before.r << 3) & 0xF8);
			T.g = float(((denorm_c_before.r >> 2) & 0x38) | ((denorm_c_before.g << 6) & 0xC0));
			T.b = float((denorm_c_before.g << 1) & 0xF8);
			T.a = float(denorm_c_before.g & 0x80);
		}
	}

	float4 C = tfx(T, input.c);

	atst(C);

	C = fog(C, input.t.z);

	return C;
}

void ps_fbmask(inout float4 C, float2 pos_xy)
{
	if (PS_FBMASK)
	{
		float4 RT = trunc(RtTexture.Load(int3(pos_xy, 0)) * 255.0f + 0.1f);
		C = (float4)(((uint4)C & ~FbMask) | ((uint4)RT & FbMask));
	}
}

void ps_dither(inout float3 C, float2 pos_xy)
{
	if (PS_DITHER)
	{
		int2 fpos;

		if (PS_DITHER == 2)
			fpos = int2(pos_xy);
		else
			fpos = int2(pos_xy * RcpScaleFactor);

		float value = DitherMatrix[fpos.x & 3][fpos.y & 3];
		if (PS_ROUND_INV)
			C -= value;
		else
			C += value;
	}
}

void ps_color_clamp_wrap(inout float3 C)
{
	// When dithering the bottom 3 bits become meaningless and cause lines in the picture
	// so we need to limit the color depth on dithered items
	if (SW_BLEND || PS_DITHER || PS_FBMASK)
	{
		if (PS_DST_FMT == FMT_16 && PS_BLEND_MIX == 0 && PS_ROUND_INV)
			C += 7.0f; // Need to round up, not down since the shader will invert

		// Standard Clamp
		if (PS_COLCLIP == 0 && PS_HDR == 0)
			C = clamp(C, (float3)0.0f, (float3)255.0f);

		// In 16 bits format, only 5 bits of color are used. It impacts shadows computation of Castlevania
		if (PS_DST_FMT == FMT_16 && PS_BLEND_MIX == 0)
			C = (float3)((int3)C & (int3)0xF8);
		else if (PS_COLCLIP == 1 || PS_HDR == 1)
			C = (float3)((int3)C & (int3)0xFF);
	}
}

void ps_blend(inout float4 Color, inout float4 As_rgba, float2 pos_xy)
{
	float As = As_rgba.a;

	if (SW_BLEND)
	{
		// PABE
		if (PS_PABE)
		{
			// No blending so early exit
			if (As < 1.0f)
				return;
		}

		float4 RT = SW_BLEND_NEEDS_RT ? trunc(RtTexture.Load(int3(pos_xy, 0)) * 255.0f + 0.1f) : (float4)0.0f;

		float Ad = RT.a / 128.0f;

		float3 Cd = RT.rgb;
		float3 Cs = Color.rgb;

		float3 A = (PS_BLEND_A == 0) ? Cs : ((PS_BLEND_A == 1) ? Cd : (float3)0.0f);
		float3 B = (PS_BLEND_B == 0) ? Cs : ((PS_BLEND_B == 1) ? Cd : (float3)0.0f);
		float  C = (PS_BLEND_C == 0) ? As : ((PS_BLEND_C == 1) ? Ad : Af);
		float3 D = (PS_BLEND_D == 0) ? Cs : ((PS_BLEND_D == 1) ? Cd : (float3)0.0f);

		// As/Af clamp alpha for Blend mix
		// We shouldn't clamp blend mix with blend hw 1 as we want alpha higher
		float C_clamped = C;
		if (PS_BLEND_MIX > 0 && PS_BLEND_HW != 1)
			C_clamped = min(C_clamped, 1.0f);

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
			// Compensate slightly for Cd*(As + 1) - Cs*As.
			// The initial factor we chose is 1 (0.00392)
			// as that is the minimum color Cd can be,
			// then we multiply by alpha to get the minimum
			// blended value it can be.
			float color_compensate = 1.0f * (C + 1.0f);
			Color.rgb -= (float3)color_compensate;
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
		if (PS_BLEND_HW == 1)
		{
			// Needed for Cd * (As/Ad/F + 1) blending modes

			Color.rgb = (float3)255.0f;
		}
		else if (PS_BLEND_HW == 2)
		{
			// Cd*As,Cd*Ad or Cd*F

			float Alpha = PS_BLEND_C == 2 ? Af : As;

			Color.rgb = max((float3)0.0f, (Alpha - (float3)1.0f));
			Color.rgb *= (float3)255.0f;
		}
		else if (PS_BLEND_HW == 3)
		{
			// Needed for Cs*Ad, Cs*Ad + Cd, Cd - Cs*Ad
			// Multiply Color.rgb by (255/128) to compensate for wrong Ad/255 value when rgb are below 128.
			// When any color channel is higher than 128 then adjust the compensation automatically
			// to give us more accurate colors, otherwise they will be wrong.
			// The higher the value (>128) the lower the compensation will be.
			float max_color = max(max(Color.r, Color.g), Color.b);
			float color_compensate = 255.0f / max(128.0f, max_color);
			Color.rgb *= (float3)color_compensate;
		}
	}
}

PS_OUTPUT ps_main(PS_INPUT input)
{
	float4 C = ps_color(input);

	PS_OUTPUT output;

	if (PS_SCANMSK & 2)
	{
		// fail depth test on prohibited lines
		if ((int(input.p.y) & 1) == (PS_SCANMSK & 1))
			discard;
	}

	// Must be done before alpha correction

	// AA (Fixed one) will output a coverage of 1.0 as alpha
	if (PS_FIXED_ONE_A)
	{
		C.a = 128.0f;
	}

	float4 alpha_blend;
	if (SW_AD_TO_HW)
	{
		float4 RT = trunc(RtTexture.Load(int3(input.p.xy, 0)) * 255.0f + 0.1f);
		alpha_blend = (float4)(RT.a / 128.0f);
	}
	else
	{
		alpha_blend = (float4)(C.a / 128.0f);
	}

	// Alpha correction
	if (PS_DST_FMT == FMT_16)
	{
		float A_one = 128.0f; // alpha output will be 0x80
		C.a = PS_FBA ? A_one : step(A_one, C.a) * A_one;
	}
	else if ((PS_DST_FMT == FMT_32) && PS_FBA)
	{
		float A_one = 128.0f;
		if (C.a < A_one) C.a += A_one;
	}

#if PS_DATE == 3
	// Note gl_PrimitiveID == stencil_ceil will be the primitive that will update
	// the bad alpha value so we must keep it.
	int stencil_ceil = int(PrimMinTexture.Load(int3(input.p.xy, 0)));
	if (int(input.primid) > stencil_ceil)
		discard;
#endif

	// Get first primitive that will write a failling alpha value
#if PS_DATE == 1
	// DATM == 0
	// Pixel with alpha equal to 1 will failed (128-255)
	output.c = (C.a > 127.5f) ? float(input.primid) : float(0x7FFFFFFF);

#elif PS_DATE == 2

	// DATM == 1
	// Pixel with alpha equal to 0 will failed (0-127)
	output.c = (C.a < 127.5f) ? float(input.primid) : float(0x7FFFFFFF);

#else
	// Not primid DATE setup

	ps_blend(C, alpha_blend, input.p.xy);

	if (PS_SHUFFLE)
	{
		if (!PS_SHUFFLE_SAME && !PS_READ16_SRC)
		{
			uint4 denorm_c_after = uint4(C);
			if (PS_READ_BA)
			{
				C.b = float(((denorm_c_after.r >> 3) & 0x1F) | ((denorm_c_after.g << 2) & 0xE0));
				C.a = float(((denorm_c_after.g >> 6) & 0x3) | ((denorm_c_after.b >> 1) & 0x7C) | (denorm_c_after.a & 0x80));
			}
			else
			{
				C.r = float(((denorm_c_after.r >> 3) & 0x1F) | ((denorm_c_after.g << 2) & 0xE0));
				C.g = float(((denorm_c_after.g >> 6) & 0x3) | ((denorm_c_after.b >> 1) & 0x7C) | (denorm_c_after.a & 0x80));
			}
		}

		uint4 denorm_c = uint4(C);
		uint2 denorm_TA = uint2(float2(TA.xy) * 255.0f + 0.5f);

		// Special case for 32bit input and 16bit output, shuffle used by The Godfather
		if (PS_SHUFFLE_SAME)
		{
			if (PS_READ_BA)
				C = (float4)(float((denorm_c.b & 0x7Fu) | (denorm_c.a & 0x80u)));
			else
				C.ga = C.rg;
		}
		// Copy of a 16bit source in to this target
		else if (PS_READ16_SRC)
		{
			C.rb = (float2)float((denorm_c.r >> 3) | (((denorm_c.g >> 3) & 0x7u) << 5));
			if (denorm_c.a & 0x80u)
				C.ga = (float2)float((denorm_c.g >> 6) | ((denorm_c.b >> 3) << 2) | (denorm_TA.y & 0x80u));
			else
				C.ga = (float2)float((denorm_c.g >> 6) | ((denorm_c.b >> 3) << 2) | (denorm_TA.x & 0x80u));
		}
		// Write RB part. Mask will take care of the correct destination
		else if (PS_READ_BA)
		{
			C.rb = C.bb;
			if (denorm_c.a & 0x80u)
				C.ga = (float2)(float((denorm_c.a & 0x7Fu) | (denorm_TA.y & 0x80u)));
			else
				C.ga = (float2)(float((denorm_c.a & 0x7Fu) | (denorm_TA.x & 0x80u)));
		}
		else
		{
			C.rb = C.rr;
			if (denorm_c.g & 0x80u)
				C.ga = (float2)(float((denorm_c.g & 0x7Fu) | (denorm_TA.y & 0x80u)));

			else
				C.ga = (float2)(float((denorm_c.g & 0x7Fu) | (denorm_TA.x & 0x80u)));
		}
	}

	ps_dither(C.rgb, input.p.xy);

	// Color clamp/wrap needs to be done after sw blending and dithering
	ps_color_clamp_wrap(C.rgb);

	ps_fbmask(C, input.p.xy);

#if !PS_NO_COLOR
	output.c0 = PS_HDR ? float4(C.rgb / 65535.0f, C.a / 255.0f) : C / 255.0f;
#if !PS_NO_COLOR1
	output.c1 = alpha_blend;
#endif

#if PS_NO_ABLEND
	// write alpha blend factor into col0
	output.c0.a = alpha_blend.a;
#endif
#if PS_ONLY_ALPHA
	// rgb isn't used
	output.c0.rgb = float3(0.0f, 0.0f, 0.0f);
#endif
#endif

#endif

#if PS_ZCLAMP
	output.depth = min(input.p.z, MaxDepthPS);
#endif

	return output;
}

#endif // PIXEL_SHADER

//////////////////////////////////////////////////////////////////////
// Vertex Shader
//////////////////////////////////////////////////////////////////////

#ifdef VERTEX_SHADER

#ifdef DX12
cbuffer cb0 : register(b0)
#else
cbuffer cb0
#endif
{
	float2 VertexScale;
	float2 VertexOffset;
	float2 TextureScale;
	float2 TextureOffset;
	float2 PointSize;
	uint MaxDepth;
	uint BaseVertex; // Only used in DX11.
};

VS_OUTPUT vs_main(VS_INPUT input)
{
	// Clamp to max depth, gs doesn't wrap
	input.z = min(input.z, MaxDepth);

	VS_OUTPUT output;

	// pos -= 0.05 (1/320 pixel) helps avoiding rounding problems (integral part of pos is usually 5 digits, 0.05 is about as low as we can go)
	// example: ceil(afterseveralvertextransformations(y = 133)) => 134 => line 133 stays empty
	// input granularity is 1/16 pixel, anything smaller than that won't step drawing up/left by one pixel
	// example: 133.0625 (133 + 1/16) should start from line 134, ceil(133.0625 - 0.05) still above 133

	output.p = float4(input.p, input.z, 1.0f) - float4(0.05f, 0.05f, 0, 0);

	output.p.xy = output.p.xy * float2(VertexScale.x, -VertexScale.y) - float2(VertexOffset.x, -VertexOffset.y);
	output.p.z *= exp2(-32.0f);		// integer->float depth

	if(VS_TME)
	{
		float2 uv = input.uv - TextureOffset;
		float2 st = input.st - TextureOffset;

		// Integer nomalized
		output.ti.xy = uv * TextureScale;

		if (VS_FST)
		{
			// Integer integral
			output.ti.zw = uv;
		}
		else
		{
			// float for post-processing in some games
			output.ti.zw = st / TextureScale;
		}
		// Float coords
		output.t.xy = st;
		output.t.w = input.q;
	}
	else
	{
		output.t.xy = 0;
		output.t.w = 1.0f;
		output.ti = 0;
	}

	output.c = input.c;
	output.t.z = input.f.r;

	return output;
}

#if VS_EXPAND != 0

struct VS_RAW_INPUT
{
	float2 ST;
	uint RGBA;
	float Q;
	uint XY;
	uint Z;
	uint UV;
	uint FOG;
};

StructuredBuffer<VS_RAW_INPUT> vertices : register(t0);

VS_INPUT load_vertex(uint index)
{
#ifdef DX12
	VS_RAW_INPUT raw = vertices.Load(index);
#else
	VS_RAW_INPUT raw = vertices.Load(BaseVertex + index);
#endif

	VS_INPUT vert;
	vert.st = raw.ST;
	vert.c = uint4(raw.RGBA & 0xFFu, (raw.RGBA >> 8) & 0xFFu, (raw.RGBA >> 16) & 0xFFu, raw.RGBA >> 24);
	vert.q = raw.Q;
	vert.p = uint2(raw.XY & 0xFFFFu, raw.XY >> 16);
	vert.z = raw.Z;
	vert.uv = uint2(raw.UV & 0xFFFFu, raw.UV >> 16);
	vert.f = float4(float(raw.FOG & 0xFFu), float((raw.FOG >> 8) & 0xFFu), float((raw.FOG >> 16) & 0xFFu), float(raw.FOG >> 24)) / 255.0f;
	return vert;
}

VS_OUTPUT vs_main_expand(uint vid : SV_VertexID)
{
#if VS_EXPAND == 1 // Point

	VS_OUTPUT vtx = vs_main(load_vertex(vid >> 2));

	vtx.p.x += ((vid & 1u) != 0u) ? PointSize.x : 0.0f;
	vtx.p.y += ((vid & 2u) != 0u) ? PointSize.y : 0.0f;

	return vtx;

#elif VS_EXPAND == 2 // Line

	uint vid_base = vid >> 2;
	bool is_bottom = vid & 2;
	bool is_right = vid & 1;
	// All lines will be a pair of vertices next to each other
	// Since DirectX uses provoking vertex first, the bottom point will be the lower of the two
	uint vid_other = is_bottom ? vid_base + 1 : vid_base - 1;
	VS_OUTPUT vtx = vs_main(load_vertex(vid_base));
	VS_OUTPUT other = vs_main(load_vertex(vid_other));

	float2 line_vector = normalize(vtx.p.xy - other.p.xy);
	float2 line_normal = float2(line_vector.y, -line_vector.x);
	float2 line_width = (line_normal * PointSize) / 2;
	// line_normal is inverted for bottom point
	float2 offset = (is_bottom ^ is_right) ? line_width : -line_width;
	vtx.p.xy += offset;

	// Lines will be run as (0 1 2) (1 2 3)
	// This means that both triangles will have a point based off the top line point as their first point
	// So we don't have to do anything for !IIP

	return vtx;

#elif VS_EXPAND == 3 // Sprite

	// Sprite points are always in pairs
	uint vid_base = vid >> 1;
	uint vid_lt = vid_base & ~1u;
	uint vid_rb = vid_base | 1u;

	VS_OUTPUT lt = vs_main(load_vertex(vid_lt));
	VS_OUTPUT rb = vs_main(load_vertex(vid_rb));
	VS_OUTPUT vtx = rb;

	bool is_right = ((vid & 1u) != 0u);
	vtx.p.x = is_right ? lt.p.x : vtx.p.x;
	vtx.t.x = is_right ? lt.t.x : vtx.t.x;
	vtx.ti.xz = is_right ? lt.ti.xz : vtx.ti.xz;

	bool is_bottom = ((vid & 2u) != 0u);
	vtx.p.y = is_bottom ? lt.p.y : vtx.p.y;
	vtx.t.y = is_bottom ? lt.t.y : vtx.t.y;
	vtx.ti.yw = is_bottom ? lt.ti.yw : vtx.ti.yw;

	return vtx;

#endif
}

#endif // VS_EXPAND

#endif // VERTEX_SHADER
