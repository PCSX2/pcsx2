#ifdef SHADER_MODEL // make safe to include in resource file to enforce dependency
#define FMT_32 0
#define FMT_24 1
#define FMT_16 2

#if SHADER_MODEL >= 0x400

#ifndef VS_BPPZ
#define VS_BPPZ 0
#define VS_TME 1
#define VS_FST 1
#endif

#ifndef GS_IIP
#define GS_IIP 0
#define GS_PRIM 3
#define GS_POINT 0
#define GS_LINE 0
#endif

#ifndef PS_FST
#define PS_FST 0
#define PS_WMS 0
#define PS_WMT 0
#define PS_FMT FMT_32
#define PS_AEM 0
#define PS_TFX 0
#define PS_TCC 1
#define PS_ATST 1
#define PS_FOG 0
#define PS_CLR1 0
#define PS_FBA 0
#define PS_AOUT 0
#define PS_LTF 1
#define PS_COLCLIP 0
#define PS_DATE 0
#define PS_SPRITEHACK 0
#define PS_TCOFFSETHACK 0
#define PS_POINT_SAMPLER 0
#define PS_SHUFFLE 0
#define PS_READ_BA 0
#define PS_DFMT 0
#define PS_DEPTH_FMT 0
#define PS_PAL_FMT 0
#define PS_CHANNEL_FETCH 0
#define PS_TALES_OF_ABYSS_HLE 0
#define PS_URBAN_CHAOS_HLE 0
#define PS_SCALE_FACTOR 1
#endif

struct VS_INPUT
{
	float2 st : TEXCOORD0;
	float4 c : COLOR0;
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
#if VS_RTCOPY
	float4 tp : TEXCOORD1;
#endif
	float4 ti : TEXCOORD2;
	float4 c : COLOR0;
};

struct PS_INPUT
{
	float4 p : SV_Position;
	float4 t : TEXCOORD0;
#if PS_DATE > 0
	float4 tp : TEXCOORD1;
#endif
	float4 ti : TEXCOORD2;
	float4 c : COLOR0;
};

struct PS_OUTPUT
{
	float4 c0 : SV_Target0;
	float4 c1 : SV_Target1;
};

Texture2D<float4> Texture : register(t0);
Texture2D<float4> Palette : register(t1);
Texture2D<float4> RTCopy : register(t2);
Texture2D<float4> RawTexture : register(t4);
SamplerState TextureSampler : register(s0);
SamplerState PaletteSampler : register(s1);
SamplerState RTCopySampler : register(s2);

cbuffer cb0
{
	float4 VertexScale;
	float4 VertexOffset;
	float4 Texture_Scale_Offset;
};

cbuffer cb1
{
	float3 FogColor;
	float AREF;
	float4 HalfTexel;
	float4 WH;
	float4 MinMax;
	float2 MinF;
	float2 TA;
	uint4 MskFix;
	int4 ChannelShuffle;
	float4 TC_OffsetHack;
};

cbuffer cb2
{
	float2 PointSize;
};

float4 sample_c(float2 uv)
{
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
	return Texture.Sample(TextureSampler, uv);
}

float4 sample_p(float u)
{
	return Palette.Sample(PaletteSampler, u);
}

float4 sample_rt(float2 uv)
{
	return RTCopy.Sample(RTCopySampler, uv);
}

float4 fetch_raw_color(int2 xy)
{
	return RawTexture.Load(int3(xy, 0));
}

int fetch_raw_depth(int2 xy)
{
	float4 col = RawTexture.Load(int3(xy, 0));
	return (int)(col.r * exp2(32.0f));
}

float4 fetch_c(int2 uv)
{
	return Texture.Load(int3(uv, 0));
}

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

	return sample_p(rt.r);
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

	return sample_p(rt.b);
}

float4 fetch_green(int2 xy)
{
	float4 rt = fetch_raw_color(xy);
	return sample_p(rt.g);
}

float4 fetch_alpha(int2 xy)
{
	float4 rt = fetch_raw_color(xy);
	return sample_p(rt.a);
}

float4 fetch_rgb(int2 xy)
{
	float4 rt = fetch_raw_color(xy);
	float4 c = float4(sample_p(rt.r).r, sample_p(rt.g).g, sample_p(rt.b).b, 1.0);
	return c;
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
		return (float4)(green | blue) / 255.0;
	}
}

#elif SHADER_MODEL <= 0x300

#ifndef VS_BPPZ
#define VS_BPPZ 0
#define VS_TME 1
#define VS_FST 1
#define VS_LOGZ 1
#endif

#ifndef PS_FST
#define PS_FST 0
#define PS_WMS 0
#define PS_WMT 0
#define PS_FMT FMT_32
#define PS_AEM 0
#define PS_TFX 0
#define PS_TCC 0
#define PS_ATST 4
#define PS_FOG 0
#define PS_CLR1 0
#define PS_RT 0
#define PS_LTF 0
#define PS_COLCLIP 0
#define PS_DATE 0
#define PS_PAL_FMT 0
#endif

struct VS_INPUT
{
	float4 p : POSITION0;
	float2 t : TEXCOORD0;
	float4 c : COLOR0;
	float4 f : COLOR1;
};

struct VS_OUTPUT
{
	float4 p : POSITION;
	float4 t : TEXCOORD0;
#if VS_RTCOPY
	float4 tp : TEXCOORD1;
#endif
	float4 c : COLOR0;
};

struct PS_INPUT
{
	float4 t : TEXCOORD0;
#if PS_DATE > 0
	float4 tp : TEXCOORD1;
#endif
	float4 c : COLOR0;
};

sampler Texture : register(s0);
sampler Palette : register(s1);
sampler RTCopy : register(s2);
sampler1D UMSKFIX : register(s3);
sampler1D VMSKFIX : register(s4);

float4 vs_params[3];

#define VertexScale vs_params[0]
#define VertexOffset vs_params[1]
#define Texture_Scale_Offset vs_params[2]

float4 ps_params[7];

#define FogColor	ps_params[0].bgr
#define AREF		ps_params[0].a
#define HalfTexel	ps_params[1]
#define WH			ps_params[2]
#define MinMax		ps_params[3]
#define MinF		ps_params[4].xy
#define TA			ps_params[4].zw

#define TC_OffsetHack ps_params[6]

float4 sample_c(float2 uv)
{
	return tex2D(Texture, uv);
}

float4 sample_p(float u)
{
	return tex2D(Palette, u);
}

float4 sample_rt(float2 uv)
{
	return tex2D(RTCopy, uv);
}

#endif

#define PS_AEM_FMT (PS_FMT & 3)

#if SHADER_MODEL >= 0x400
int2 clamp_wrap_uv_depth(int2 uv)
{
	int4 mask = (int4)MskFix << 4;
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
	float2 uv_f = (float2)clamp_wrap_uv_depth(int2(st)) * (float2)PS_SCALE_FACTOR * (float2)(1.0f / 16.0f);
	int2 uv = (int2)uv_f;

	float4 t = (float4)(0.0f);

	if (PS_TALES_OF_ABYSS_HLE == 1)
	{
		// Warning: UV can't be used in channel effect
		int depth = fetch_raw_depth(pos);

		// Convert msb based on the palette
		t = Palette.Load(int3((depth >> 8) & 0xFF, 0, 0));
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
		t = Palette.Load(int3(depth & 0xFF, 0, 0));

		// Msb is easier
		float green = (float)((depth >> 8) & 0xFF) * 36.0f;
		green = min(green, 255.0f);
		t.g += green / 255.0f;
	}
	else if (PS_DEPTH_FMT == 1)
	{
		// Based on ps_main11 of convert

		// Convert a FLOAT32 depth texture into a RGBA color texture
		const float4 bitSh = float4(exp2(24.0f), exp2(16.0f), exp2(8.0f), exp2(0.0f));
		const float4 bitMsk = float4(0.0, 1.0f / 256.0f, 1.0f / 256.0f, 1.0f / 256.0f);

		float4 res = frac((float4)fetch_c(uv).r * bitSh);

		t = (res - res.xxyz * bitMsk) * 256.0f / 255.0f;
	}
	else if (PS_DEPTH_FMT == 2)
	{
		// Based on ps_main12 of convert

		// Convert a FLOAT32 (only 16 lsb) depth into a RGB5A1 color texture
		const float4 bitSh = float4(exp2(32.0f), exp2(27.0f), exp2(22.0f), exp2(17.0f));
		const uint4 bitMsk = uint4(0x1F, 0x1F, 0x1F, 0x1);
		uint4 color = (uint4)((float4)fetch_c(uv).r * bitSh) & bitMsk;

		t = (float4)color * float4(8.0f, 8.0f, 8.0f, 128.0f);
	}
	else if (PS_DEPTH_FMT == 3)
	{
		// Convert a RGBA/RGB5A1 color texture into a RGBA/RGB5A1 color texture
		t = fetch_c(uv);
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
#endif

float4 clamp_wrap_uv(float4 uv)
{
	if(PS_WMS == PS_WMT)
	{
/*
		if(PS_WMS == 0)
		{
			uv = frac(uv);
		}
		else if(PS_WMS == 1)
		{
			uv = saturate(uv);
		}
		else
*/
		if(PS_WMS == 2)
		{
			uv = clamp(uv, MinMax.xyxy, MinMax.zwzw);
		}
		else if(PS_WMS == 3)
		{
			#if SHADER_MODEL >= 0x400
			#if PS_FST == 0
			// wrap negative uv coords to avoid an off by one error that shifted
			// textures. Fixes Xenosaga's hair issue.
			uv = frac(uv);
			#endif
			uv = (float4)(((uint4)(uv * WH.xyxy) & MskFix.xyxy) | MskFix.zwzw) / WH.xyxy;
			#elif SHADER_MODEL <= 0x300
			uv.x = tex1D(UMSKFIX, uv.x);
			uv.y = tex1D(VMSKFIX, uv.y);
			uv.z = tex1D(UMSKFIX, uv.z);
			uv.w = tex1D(VMSKFIX, uv.w);
			#endif
		}
	}
	else
	{
/*
		if(PS_WMS == 0)
		{
			uv.xz = frac(uv.xz);
		}
		else if(PS_WMS == 1)
		{
			uv.xz = saturate(uv.xz);
		}
		else
*/
		if(PS_WMS == 2)
		{
			uv.xz = clamp(uv.xz, MinMax.xx, MinMax.zz);
		}
		else if(PS_WMS == 3)
		{
			#if SHADER_MODEL >= 0x400
			#if PS_FST == 0
			uv.xz = frac(uv.xz);
			#endif
			uv.xz = (float2)(((uint2)(uv.xz * WH.xx) & MskFix.xx) | MskFix.zz) / WH.xx;
			#elif SHADER_MODEL <= 0x300
			uv.x = tex1D(UMSKFIX, uv.x);
			uv.z = tex1D(UMSKFIX, uv.z);
			#endif
		}
/*
		if(PS_WMT == 0)
		{
			uv.yw = frac(uv.yw);
		}
		else if(PS_WMT == 1)
		{
			uv.yw = saturate(uv.yw);
		}
		else
*/
		if(PS_WMT == 2)
		{
			uv.yw = clamp(uv.yw, MinMax.yy, MinMax.ww);
		}
		else if(PS_WMT == 3)
		{
			#if SHADER_MODEL >= 0x400
			#if PS_FST == 0
			uv.yw = frac(uv.yw);
			#endif
			uv.yw = (float2)(((uint2)(uv.yw * WH.yy) & MskFix.yy) | MskFix.ww) / WH.yy;
			#elif SHADER_MODEL <= 0x300
			uv.y = tex1D(VMSKFIX, uv.y);
			uv.w = tex1D(VMSKFIX, uv.w);
			#endif
		}
	}

	return uv;
}

float4x4 sample_4c(float4 uv)
{
	float4x4 c;

	c[0] = sample_c(uv.xy);
	c[1] = sample_c(uv.zy);
	c[2] = sample_c(uv.xw);
	c[3] = sample_c(uv.zw);

	return c;
}

float4 sample_4_index(float4 uv)
{
	float4 c;

	c.x = sample_c(uv.xy).a;
	c.y = sample_c(uv.zy).a;
	c.z = sample_c(uv.xw).a;
	c.w = sample_c(uv.zw).a;

#if SHADER_MODEL <= 0x300

	if (PS_RT) c *= 128.0f / 255;
	// D3D9 doesn't support integer operations

#else

	// Denormalize value
	uint4 i = uint4(c * 255.0f + 0.5f);

	if (PS_PAL_FMT == 1)
	{
		// 4HL
		c = float4(i & 0xFu) / 255.0f;
	}
	else if (PS_PAL_FMT == 2)
	{
		// 4HH
		c = float4(i >> 4u) / 255.0f;
	}

#endif

	// Most of texture will hit this code so keep normalized float value
	// 8 bits
	return c * 255./256 + 0.5/256;
}

float4x4 sample_4p(float4 u)
{
	float4x4 c;

	c[0] = sample_p(u.x);
	c[1] = sample_p(u.y);
	c[2] = sample_p(u.z);
	c[3] = sample_p(u.w);

	return c;
}

float4 sample_color(float2 st)
{
	#if PS_TCOFFSETHACK
	st += TC_OffsetHack.xy;
	#endif

	float4 t;
	float4x4 c;
	float2 dd;

	if (PS_LTF == 0 && PS_AEM_FMT == FMT_32 && PS_PAL_FMT == 0 && PS_WMS < 2 && PS_WMT < 2)
	{
		c[0] = sample_c(st);
	}
	else
	{
		float4 uv;

		if(PS_LTF)
		{
			uv = st.xyxy + HalfTexel;
			dd = frac(uv.xy * WH.zw);
			#if SHADER_MODEL >= 0x400
			if(PS_FST == 0)
			{
				dd = clamp(dd, (float2)0.0, (float2)0.9999999);
			}
			#endif
		}
		else
		{
			uv = st.xyxy;
		}

		uv = clamp_wrap_uv(uv);

#if PS_PAL_FMT != 0
			c = sample_4p(sample_4_index(uv));
#else
			c = sample_4c(uv);
#endif
	}

	[unroll]
	for (uint i = 0; i < 4; i++)
	{
		if(PS_AEM_FMT == FMT_32)
		{
			#if SHADER_MODEL <= 0x300
			if(PS_RT) c[i].a *= 128.0f / 255;
			#endif
		}
		else if(PS_AEM_FMT == FMT_24)
		{
			c[i].a = !PS_AEM || any(c[i].rgb) ? TA.x : 0;
		}
		else if(PS_AEM_FMT == FMT_16)
		{
			c[i].a = c[i].a >= 0.5 ? TA.y : !PS_AEM || any(c[i].rgb) ? TA.x : 0;
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

	return t;
}

float4 tfx(float4 t, float4 c)
{
	if(PS_TFX == 0)
	{
		if(PS_TCC)
		{
			c = c * t * 255.0f / 128;
		}
		else
		{
			c.rgb = c.rgb * t.rgb * 255.0f / 128;
		}
	}
	else if(PS_TFX == 1)
	{
		if(PS_TCC)
		{
			c = t;
		}
		else
		{
			c.rgb = t.rgb;
		}
	}
	else if(PS_TFX == 2)
	{
		c.rgb = c.rgb * t.rgb * 255.0f / 128 + c.a;

		if(PS_TCC)
		{
			c.a += t.a;
		}
	}
	else if(PS_TFX == 3)
	{
		c.rgb = c.rgb * t.rgb * 255.0f / 128 + c.a;

		if(PS_TCC)
		{
			c.a = t.a;
		}
	}

	return saturate(c);
}

void datst(PS_INPUT input)
{
#if PS_DATE > 0
	float alpha = sample_rt(input.tp.xy).a;
#if SHADER_MODEL >= 0x400
	float alpha0x80 = 128. / 255;
#else
	float alpha0x80 = 1;
#endif

	if (PS_DATE == 1 && alpha >= alpha0x80)
		discard;
	else if (PS_DATE == 2 && alpha < alpha0x80)
		discard;
#endif
}

void atst(float4 c)
{
	float a = trunc(c.a * 255 + 0.01);

#if 0
    switch(Uber_ATST) {
        case 0:
            break;
        case 1:
            if (a > AREF) discard;
            break;
        case 2:
            if (a < AREF) discard;
            break;
        case 3:
            if (abs(a - AREF) > 0.5f) discard;
            break;
        case 4:
            if (abs(a - AREF) < 0.5f) discard;
            break;
    }

#endif

#if 1
	if(PS_ATST == 0)
	{
		// nothing to do
	}
	else if(PS_ATST == 1)
	{
		#if PS_SPRITEHACK == 0
		if (a > AREF) discard;
		#endif
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
#endif
}

float4 fog(float4 c, float f)
{
	if(PS_FOG)
	{
		c.rgb = lerp(FogColor, c.rgb, f);
	}

	return c;
}

float4 ps_color(PS_INPUT input)
{
	datst(input);
	
#if PS_FST == 0
	float2 st = input.t.xy / input.t.w;
	float2 st_int = input.ti.zw / input.t.w;
#else
	float2 st = input.ti.xy;
	float2 st_int = input.ti.zw;
#endif

#if PS_CHANNEL_FETCH == 1
	float4 t = fetch_red(int2(input.p.xy));
#elif PS_CHANNEL_FETCH == 2
	float4 t = fetch_green(int2(input.p.xy));
#elif PS_CHANNEL_FETCH == 3
	float4 t = fetch_blue(int2(input.p.xy));
#elif PS_CHANNEL_FETCH == 4
	float4 t = fetch_alpha(int2(input.p.xy));
#elif PS_CHANNEL_FETCH == 5
	float4 t = fetch_rgb(int2(input.p.xy));
#elif PS_CHANNEL_FETCH == 6
	float4 t = fetch_gXbY(int2(input.p.xy));
#elif PS_DEPTH_FMT > 0
	float4 t = sample_depth(st_int, input.p.xy);
#else
	float4 t = sample_color(st);
#endif

	float4 c = tfx(t, input.c);

	atst(c);

	c = fog(c, input.t.z);

	// FIXME: Colclip and Depth sampling shouldn't run together.
	if (PS_COLCLIP == 2)
	{
		c.rgb = 256./255. - c.rgb;
	}
	if (PS_COLCLIP > 0)
	{
		c.rgb *= c.rgb < 128./255;
	}

	if(PS_CLR1) // needed for Cd * (As/Ad/F + 1) blending modes
	{
		c.rgb = 1;
	}

	return c;
}

#if SHADER_MODEL >= 0x400

VS_OUTPUT vs_main(VS_INPUT input)
{
	if(VS_BPPZ == 1) // 24
	{
		input.z = input.z & 0xffffff;
	}
	else if(VS_BPPZ == 2) // 16
	{
		input.z = input.z & 0xffff;
	}

	VS_OUTPUT output;

	// pos -= 0.05 (1/320 pixel) helps avoiding rounding problems (integral part of pos is usually 5 digits, 0.05 is about as low as we can go)
	// example: ceil(afterseveralvertextransformations(y = 133)) => 134 => line 133 stays empty
	// input granularity is 1/16 pixel, anything smaller than that won't step drawing up/left by one pixel
	// example: 133.0625 (133 + 1/16) should start from line 134, ceil(133.0625 - 0.05) still above 133

	float4 p = float4(input.p, input.z, 0) - float4(0.05f, 0.05f, 0, 0);

	output.p = p * VertexScale - VertexOffset;
#if VS_RTCOPY
	output.tp = (p * VertexScale - VertexOffset) * float4(0.5, -0.5, 0, 0) + 0.5;
#endif

	if(VS_TME)
	{
		float2 uv = input.uv - Texture_Scale_Offset.zw;
		float2 st = input.st - Texture_Scale_Offset.zw;

		// Integer nomalized
		output.ti.xy = uv * Texture_Scale_Offset.xy;

		if (VS_FST)
		{
			// Integer integral
			output.ti.zw = uv;
		}
		else
		{
			// float for post-processing in some games
			output.ti.zw = st / Texture_Scale_Offset.xy;
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

#if GS_PRIM == 0 && GS_POINT == 0

[maxvertexcount(1)]
void gs_main(point VS_OUTPUT input[1], inout PointStream<VS_OUTPUT> stream)
{
	stream.Append(input[0]);
}

#elif GS_PRIM == 0 && GS_POINT == 1

[maxvertexcount(6)]
void gs_main(point VS_OUTPUT input[1], inout TriangleStream<VS_OUTPUT> stream)
{
	// Transform a point to a NxN sprite
	VS_OUTPUT Point = input[0];

	// Get new position
	float4 lt_p = input[0].p;
	float4 rb_p = input[0].p + float4(PointSize.x, PointSize.y, 0.0f, 0.0f);
	float4 lb_p = rb_p;
	float4 rt_p = rb_p;
	lb_p.x = lt_p.x;
	rt_p.y = lt_p.y;

	// Triangle 1
	Point.p = lt_p;
	stream.Append(Point);

	Point.p = lb_p;
	stream.Append(Point);

	Point.p = rt_p;
	stream.Append(Point);

	// Triangle 2
	Point.p = lb_p;
	stream.Append(Point);

	Point.p = rt_p;
	stream.Append(Point);

	Point.p = rb_p;
	stream.Append(Point);
}

#elif GS_PRIM == 1 && GS_LINE == 0

[maxvertexcount(2)]
void gs_main(line VS_OUTPUT input[2], inout LineStream<VS_OUTPUT> stream)
{
#if GS_IIP == 0
	input[0].c = input[1].c;
#endif

	stream.Append(input[0]);
	stream.Append(input[1]);
}

#elif GS_PRIM == 1 && GS_LINE == 1

[maxvertexcount(6)]
void gs_main(line VS_OUTPUT input[2], inout TriangleStream<VS_OUTPUT> stream)
{
	// Transform a line to a thick line-sprite
	VS_OUTPUT left = input[0];
	VS_OUTPUT right = input[1];
	float2 lt_p = input[0].p.xy;
	float2 rt_p = input[1].p.xy;

	// Potentially there is faster math
	float2 line_vector = normalize(rt_p.xy - lt_p.xy);
	float2 line_normal = float2(line_vector.y, -line_vector.x);
	float2 line_width = (line_normal * PointSize) / 2;

	lt_p -= line_width;
	rt_p -= line_width;
	float2 lb_p = input[0].p.xy + line_width;
	float2 rb_p = input[1].p.xy + line_width;

	#if GS_IIP == 0
	left.c = right.c;
	#endif

	// Triangle 1
	left.p.xy = lt_p;
	stream.Append(left);

	left.p.xy = lb_p;
	stream.Append(left);

	right.p.xy = rt_p;
	stream.Append(right);
	stream.RestartStrip();

	// Triangle 2
	left.p.xy = lb_p;
	stream.Append(left);

	right.p.xy = rt_p;
	stream.Append(right);

	right.p.xy = rb_p;
	stream.Append(right);
	stream.RestartStrip();
}

#elif GS_PRIM == 2

[maxvertexcount(3)]
void gs_main(triangle VS_OUTPUT input[3], inout TriangleStream<VS_OUTPUT> stream)
{
	#if GS_IIP == 0
	input[0].c = input[2].c;
	input[1].c = input[2].c;
	#endif

	stream.Append(input[0]);
	stream.Append(input[1]);
	stream.Append(input[2]);
}

#elif GS_PRIM == 3

[maxvertexcount(4)]
void gs_main(line VS_OUTPUT input[2], inout TriangleStream<VS_OUTPUT> stream)
{
	VS_OUTPUT lt = input[0];
	VS_OUTPUT rb = input[1];

	// flat depth
	lt.p.z = rb.p.z;
	// flat fog and texture perspective
	lt.t.zw = rb.t.zw;

	// flat color
	lt.c = rb.c;

	// Swap texture and position coordinate
	VS_OUTPUT lb = rb;
	lb.p.x = lt.p.x;
	lb.t.x = lt.t.x;
	lb.ti.x = lt.ti.x;
	lb.ti.z = lt.ti.z;

	VS_OUTPUT rt = rb;
	rt.p.y = lt.p.y;
	rt.t.y = lt.t.y;
	rt.ti.y = lt.ti.y;
	rt.ti.w = lt.ti.w;

	stream.Append(lt);
	stream.Append(lb);
	stream.Append(rt);
	stream.Append(rb);
}

#endif

PS_OUTPUT ps_main(PS_INPUT input)
{
	float4 c = ps_color(input);

	PS_OUTPUT output;

	if (PS_SHUFFLE)
	{
		uint4 denorm_c = uint4(c * 255.0f + 0.5f);
		uint2 denorm_TA = uint2(float2(TA.xy) * 255.0f + 0.5f);

		// Mask will take care of the correct destination
		if (PS_READ_BA)
			c.rb = c.bb;
		else
			c.rb = c.rr;

		if (PS_READ_BA)
		{
			if (denorm_c.a & 0x80u)
				c.ga = (float2)(float((denorm_c.a & 0x7Fu) | (denorm_TA.y & 0x80u)) / 255.0f);
			else
				c.ga = (float2)(float((denorm_c.a & 0x7Fu) | (denorm_TA.x & 0x80u)) / 255.0f);
		}
		else
		{
			if (denorm_c.g & 0x80u)
				c.ga = (float2)(float((denorm_c.g & 0x7Fu) | (denorm_TA.y & 0x80u)) / 255.0f);
			else
				c.ga = (float2)(float((denorm_c.g & 0x7Fu) | (denorm_TA.x & 0x80u)) / 255.0f);
		}
	}

	output.c1 = c.a * 2; // used for alpha blending

	if((PS_DFMT == FMT_16) || PS_AOUT) // 16 bit output
	{
		float a = 128.0f / 255; // alpha output will be 0x80

		c.a = PS_FBA ? a : step(0.5, c.a) * a;
	}
	else if((PS_DFMT == FMT_32) && PS_FBA)
	{
		if(c.a < 0.5) c.a += 0.5;
	}

	output.c0 = c;

	return output;
}

#elif SHADER_MODEL <= 0x300

VS_OUTPUT vs_main(VS_INPUT input)
{
	if(VS_BPPZ == 1) // 24
	{
		input.p.z = fmod(input.p.z, 0x1000000);
	}
	else if(VS_BPPZ == 2) // 16
	{
		input.p.z = fmod(input.p.z, 0x10000);
	}

	VS_OUTPUT output;

	// pos -= 0.05 (1/320 pixel) helps avoiding rounding problems (integral part of pos is usually 5 digits, 0.05 is about as low as we can go)
	// example: ceil(afterseveralvertextransformations(y = 133)) => 134 => line 133 stays empty
	// input granularity is 1/16 pixel, anything smaller than that won't step drawing up/left by one pixel
	// example: 133.0625 (133 + 1/16) should start from line 134, ceil(133.0625 - 0.05) still above 133

	float4 p = input.p - float4(0.05f, 0.05f, 0, 0);

	output.p = p * VertexScale - VertexOffset;
#if VS_RTCOPY
	output.tp = (p * VertexScale - VertexOffset) * float4(0.5, -0.5, 0, 0) + 0.5;
#endif

	if(VS_LOGZ)
	{
		output.p.z = log2(1.0f + input.p.z) / 32;
	}

	if(VS_TME)
	{
		float2 t = input.t - Texture_Scale_Offset.zw;
		if(VS_FST)
		{

			output.t.xy = t * Texture_Scale_Offset.xy;
			output.t.zw = t;
		}
		else
		{
			output.t.xy = t;
			output.t.w = input.p.w;
		}
	}
	else
	{
		output.t.xy = 0;
		output.t.w = 1.0f;
	}

	output.c = input.c;
	output.t.z = input.f.b;

	return output;
}

float4 ps_main(PS_INPUT input) : COLOR
{
	float4 c = ps_color(input);

	c.a *= 2;

	return c;
}

#endif
#endif
