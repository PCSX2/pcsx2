#ifdef SHADER_MODEL // make safe to include in resource file to enforce dependency

#define FMT_32 0
#define FMT_24 1
#define FMT_16 2

#ifndef VS_TME
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
#define PS_FBMASK 0
#define PS_LTF 1
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
#define PS_INVALID_TEX0 0
#define PS_SCALE_FACTOR 1
#define PS_HDR 0
#define PS_COLCLIP 0
#define PS_BLEND_A 0
#define PS_BLEND_B 0
#define PS_BLEND_C 0
#define PS_BLEND_D 0
#define PS_PABE 0
#define PS_DITHER 0
#define PS_ZCLAMP 0
#endif

#define SW_BLEND (PS_BLEND_A || PS_BLEND_B || PS_BLEND_D)
#define PS_AEM_FMT (PS_FMT & 3)

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
	float4 c : COLOR0;
};

struct PS_INPUT
{
	float4 p : SV_Position;
	float4 t : TEXCOORD0;
	float4 ti : TEXCOORD2;
	float4 c : COLOR0;
};

struct PS_OUTPUT
{
	float4 c0 : SV_Target0;
	float4 c1 : SV_Target1;
#if PS_ZCLAMP
	float depth : SV_Depth;
#endif
};

Texture2D<float4> Texture : register(t0);
Texture2D<float4> Palette : register(t1);
Texture2D<float4> RtSampler : register(t3);
Texture2D<float4> RawTexture : register(t4);
SamplerState TextureSampler : register(s0);
SamplerState PaletteSampler : register(s1);

cbuffer cb0
{
	float4 VertexScale;
	float4 VertexOffset;
	float4 Texture_Scale_Offset;
	uint MaxDepth;
	uint3 pad_cb0;
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
	uint4 FbMask;
	float4 TC_OffsetHack;
	float Af;
	float MaxDepthPS;
	float2 pad_cb1;
	float4x4 DitherMatrix;
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

float4 clamp_wrap_uv(float4 uv)
{
	float4 tex_size;

	if (PS_INVALID_TEX0 == 1)
		tex_size = WH.zwzw;
	else
		tex_size = WH.xyxy;

	if(PS_WMS == PS_WMT)
	{
		if(PS_WMS == 2)
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
			uv = (float4)(((uint4)(uv * tex_size) & MskFix.xyxy) | MskFix.zwzw) / tex_size;
		}
	}
	else
	{
		if(PS_WMS == 2)
		{
			uv.xz = clamp(uv.xz, MinMax.xx, MinMax.zz);
		}
		else if(PS_WMS == 3)
		{
			#if PS_FST == 0
			uv.xz = frac(uv.xz);
			#endif
			uv.xz = (float2)(((uint2)(uv.xz * tex_size.xx) & MskFix.xx) | MskFix.zz) / tex_size.xx;
		}
		if(PS_WMT == 2)
		{
			uv.yw = clamp(uv.yw, MinMax.yy, MinMax.ww);
		}
		else if(PS_WMT == 3)
		{
			#if PS_FST == 0
			uv.yw = frac(uv.yw);
			#endif
			uv.yw = (float2)(((uint2)(uv.yw * tex_size.yy) & MskFix.yy) | MskFix.ww) / tex_size.yy;
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

int fetch_raw_depth(int2 xy)
{
	float4 col = RawTexture.Load(int3(xy, 0));
	return (int)(col.r * exp2(32.0f));
}

float4 fetch_raw_color(int2 xy)
{
	return RawTexture.Load(int3(xy, 0));
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
		// Based on ps_main11 of convert

		// Convert a FLOAT32 depth texture into a RGBA color texture
		const float4 bitSh = float4(exp2(24.0f), exp2(16.0f), exp2(8.0f), exp2(0.0f));
		const float4 bitMsk = float4(0.0, 1.0f / 256.0f, 1.0f / 256.0f, 1.0f / 256.0f);

		float4 res = frac((float4)fetch_c(uv).r * bitSh);

		t = (res - res.xxyz * bitMsk) * 256.0f;
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

	return sample_p(rt.r) * 255.0f;
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

	return sample_p(rt.b) * 255.0f;
}

float4 fetch_green(int2 xy)
{
	float4 rt = fetch_raw_color(xy);
	return sample_p(rt.g) * 255.0f;
}

float4 fetch_alpha(int2 xy)
{
	float4 rt = fetch_raw_color(xy);
	return sample_p(rt.a) * 255.0f;
}

float4 fetch_rgb(int2 xy)
{
	float4 rt = fetch_raw_color(xy);
	float4 c = float4(sample_p(rt.r).r, sample_p(rt.g).g, sample_p(rt.b).b, 1.0);
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
			c = sample_4p(sample_4_index(uv));
#else
			c = sample_4c(uv);
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

	return trunc(t * 255.0f + 0.05f);
}

float4 tfx(float4 T, float4 C)
{
	float4 C_out;
	float4 FxT = trunc(trunc(C) * T / 128.0f);

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
#if PS_FST == 0 && PS_INVALID_TEX0 == 1
	// Re-normalize coordinate from invalid GS to corrected texture size
	float2 st = (input.t.xy * WH.xy) / (input.t.w * WH.zw);
	// no st_int yet
#elif PS_FST == 0
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
	float4 T = sample_color(st);
#endif

	float4 C = tfx(T, input.c);

	atst(C);

	C = fog(C, input.t.z);

	if(PS_CLR1) // needed for Cd * (As/Ad/F + 1) blending modes
	{
		C.rgb = (float3)255.0f;
	}

	return C;
}

void ps_fbmask(inout float4 C, float2 pos_xy)
{
	if (PS_FBMASK)
	{
		float4 RT = trunc(RtSampler.Load(int3(pos_xy, 0)) * 255.0f + 0.1f);
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
			fpos = int2(pos_xy / (float)PS_SCALE_FACTOR);

		C += DitherMatrix[fpos.y & 3][fpos.x & 3];
	}
}

void ps_blend(inout float4 Color, float As, float2 pos_xy)
{
	if (SW_BLEND)
	{
		float4 RT = trunc(RtSampler.Load(int3(pos_xy, 0)) * 255.0f + 0.1f);

		float Ad = (PS_DFMT == FMT_24) ? 1.0f : RT.a / 128.0f;

		float3 Cd = RT.rgb;
		float3 Cs = Color.rgb;
		float3 Cv;

		float3 A = (PS_BLEND_A == 0) ? Cs : ((PS_BLEND_A == 1) ? Cd : (float3)0.0f);
		float3 B = (PS_BLEND_B == 0) ? Cs : ((PS_BLEND_B == 1) ? Cd : (float3)0.0f);
		float3 C = (PS_BLEND_C == 0) ? As : ((PS_BLEND_C == 1) ? Ad : Af);
		float3 D = (PS_BLEND_D == 0) ? Cs : ((PS_BLEND_D == 1) ? Cd : (float3)0.0f);

		Cv = (PS_BLEND_A == PS_BLEND_B) ? D : trunc(((A - B) * C) + D);

		// PABE
		if (PS_PABE)
			Cv = (Color.a >= 128.0f) ? Cv : Color.rgb;

		// Dithering
		ps_dither(Cv, pos_xy);

		// Standard Clamp
		if (PS_COLCLIP == 0 && PS_HDR == 0)
			Cv = clamp(Cv, (float3)0.0f, (float3)255.0f);

		// In 16 bits format, only 5 bits of color are used. It impacts shadows computation of Castlevania
		if (PS_DFMT == FMT_16)
			Cv = (float3)((int3)Cv & (int3)0xF8);
		else if (PS_COLCLIP == 1 && PS_HDR == 0)
			Cv = (float3)((int3)Cv & (int3)0xFF);

		Color.rgb = Cv;
	}
}

PS_OUTPUT ps_main(PS_INPUT input)
{
	float4 C = ps_color(input);

	PS_OUTPUT output;

	if (PS_SHUFFLE)
	{
		uint4 denorm_c = uint4(C);
		uint2 denorm_TA = uint2(float2(TA.xy) * 255.0f + 0.5f);

		// Mask will take care of the correct destination
		if (PS_READ_BA)
			C.rb = C.bb;
		else
			C.rb = C.rr;

		if (PS_READ_BA)
		{
			if (denorm_c.a & 0x80u)
				C.ga = (float2)(float((denorm_c.a & 0x7Fu) | (denorm_TA.y & 0x80u)));
			else
				C.ga = (float2)(float((denorm_c.a & 0x7Fu) | (denorm_TA.x & 0x80u)));
		}
		else
		{
			if (denorm_c.g & 0x80u)
				C.ga = (float2)(float((denorm_c.g & 0x7Fu) | (denorm_TA.y & 0x80u)));
			else
				C.ga = (float2)(float((denorm_c.g & 0x7Fu) | (denorm_TA.x & 0x80u)));
		}
	}

	// Must be done before alpha correction
	float alpha_blend = C.a / 128.0f;

	// Alpha correction
	if (PS_DFMT == FMT_16)
	{
		float A_one = 128.0f; // alpha output will be 0x80
		C.a = PS_FBA ? A_one : step(A_one, C.a) * A_one;
	}
	else if ((PS_DFMT == FMT_32) && PS_FBA)
	{
		float A_one = 128.0f;
		if (C.a < A_one) C.a += A_one;
	}

	if (!SW_BLEND)
		ps_dither(C.rgb, input.p.xy);

	ps_blend(C, alpha_blend, input.p.xy);

	ps_fbmask(C, input.p.xy);

	// When dithering the bottom 3 bits become meaningless and cause lines in the picture
	// so we need to limit the color depth on dithered items
	// SW_BLEND already deals with this so no need to do in those cases
	if (!SW_BLEND && PS_DITHER && PS_DFMT == FMT_16 && !PS_COLCLIP)
	{
		C.rgb = clamp(C.rgb, (float3)0.0f, (float3)255.0f);
		C.rgb = (uint3)((uint3)C.rgb & (uint3)0xF8);
	}

	output.c0 = C / 255.0f;
	output.c1 = (float4)(alpha_blend);

#if PS_ZCLAMP
	output.depth = min(input.p.z, MaxDepthPS);
#endif

	return output;
}

//////////////////////////////////////////////////////////////////////
// Vertex Shader
//////////////////////////////////////////////////////////////////////

VS_OUTPUT vs_main(VS_INPUT input)
{
	// Clamp to max depth, gs doesn't wrap
	input.z = min(input.z, MaxDepth);

	VS_OUTPUT output;

	// pos -= 0.05 (1/320 pixel) helps avoiding rounding problems (integral part of pos is usually 5 digits, 0.05 is about as low as we can go)
	// example: ceil(afterseveralvertextransformations(y = 133)) => 134 => line 133 stays empty
	// input granularity is 1/16 pixel, anything smaller than that won't step drawing up/left by one pixel
	// example: 133.0625 (133 + 1/16) should start from line 134, ceil(133.0625 - 0.05) still above 133

	float4 p = float4(input.p, input.z, 0) - float4(0.05f, 0.05f, 0, 0);

	output.p = p * VertexScale - VertexOffset;

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

//////////////////////////////////////////////////////////////////////
// Geometry Shader
//////////////////////////////////////////////////////////////////////

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
#endif
