// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#define FMT_32 0
#define FMT_24 1
#define FMT_16 2

#define SHUFFLE_READ  1
#define SHUFFLE_WRITE 2
#define SHUFFLE_READWRITE (SHUFFLE_READ | SHUFFLE_WRITE)

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
#define PS_PROCESS_BA 0
#define PS_PROCESS_RG 0
#define PS_SHUFFLE_ACROSS 0
#define PS_READ16_SRC 0
#define PS_DST_FMT 0
#define PS_DEPTH_FMT 0
#define PS_PAL_FMT 0
#define PS_CHANNEL_FETCH 0
#define PS_TALES_OF_ABYSS_HLE 0
#define PS_URBAN_CHAOS_HLE 0
#define PS_COLCLIP_HW 0
#define PS_RTA_CORRECTION 0
#define PS_RTA_SRC_CORRECTION 0
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
#define PS_DITHER_ADJUST 0
#define PS_ZCLAMP 0
#define PS_SCANMSK 0
#define PS_AUTOMATIC_LOD 0
#define PS_MANUAL_LOD 0
#define PS_TEX_IS_FB 0
#define PS_NO_COLOR 0
#define PS_NO_COLOR1 0
#define PS_DATE 0
// 0 Off
// 1 Unclamp ceiling only (allow rgb values beyond 255, but quantize to 8bit/int or 5bit values anyway, and dither anyway)
// 2 Unclamp ceiling (allow values beyond 255) and quantize alpha only
// 3 Unclamp ceiling (allow values beyond 255) and do not quantize neither rgb nor alpha
#define PS_HDR 0
#endif

//TODO: clear (add a new "simple" HDR branch that just unlocks some brightness beyond 255?)
#if 0
#undef PS_HDR
#define PS_HDR 0
#endif
#if PS_HDR && 0
#undef PS_HDR
#define PS_HDR 1
#endif

//TODO: clean and decide
// Force quantization to 8bit if colclip is enabled, to make sure it all works as it did on real HW, otherwise we could risk running into >255 wraps more easily, due to the quantization/truncation branches we skip in HDR
#if PS_HDR && PS_COLCLIP_HW && 1
#undef PS_HDR
#define PS_HDR 0
#endif

// Useful to allow "PS_HDR" to be >= 1, to properly handle float textures in and out, but prevent any (or anyway most) HDR colors from being generated
#define PS_HDR_FORCE_OFF 0
//TODO: finish
// Given that HDR doesn't truncate values, slightly decreate them instead to keep the average brightness roughly the same, while retaining a higher range and avoiding quantization
#define PS_BALANCE_HDR 0

#define SW_BLEND (PS_BLEND_A || PS_BLEND_B || PS_BLEND_D)
#define SW_BLEND_NEEDS_RT (SW_BLEND && (PS_BLEND_A == 1 || PS_BLEND_B == 1 || PS_BLEND_C == 1 || PS_BLEND_D == 1))
#define SW_AD_TO_HW (PS_BLEND_C == 1 && PS_A_MASKED)

// If this is true, shuffle is simply swapping g/a or r/b within a single pass, by doing two 16<->32 bit reinterprets on read and write.
// Note that we might also want to accept "PS_TFX == 0" if the whole pass vertex color is 128, but that's not necessary until proven otherwise. Fog might also be active but with intensity 0.
// Other types of blends might also be active as long as they didn't influence the input (e.g. different blend flags, or fixed alpha at 128 (neutral)), we can't can't easily account for them all.
#define SHUFFLE_TEX_PASSTHROUGH (PS_FOG == 0 && PS_TFX == 1 && PS_TCC == 0 && PS_FIXED_ONE_A == 0 && SW_BLEND ? (PS_BLEND_A == PS_BLEND_B && PS_BLEND_D == 0) : (PS_BLEND_MIX == 0 && PS_BLEND_HW == 0))
#define SHUFFLE_RT_PASSTHROUGH (SW_BLEND && PS_BLEND_A == PS_BLEND_B && PS_BLEND_D == 1)

#define FLT_MIN	asfloat(0x00800000)  //1.175494351e-38f
#define FLT_MAX	asfloat(0x7F7FFFFF)  //3.402823466e+38f

// Neutral alpha (1) on PS2 has a value of 128 out of 255, not 127.5 as one might thing. This means the maximum maps to 1.9921875, not 2. Though internally it always works with integers.
// For HDR, we still retain this "asymmetrical" feature. If we wanted to remap neutral alpha to be 127.5 in our render targets (which we can, because in float textures that value is possible),
// we'd need to branch on whether we are reading a texture loaded from the game disc, or one that we previously wrote through through these shaders, and adjust it accordingly.
// Overall, that's too complicated and not worth the extra code, especially because there's palettes/LUTs in the middle, which would be hard to define a behaviour for.
#define NEUTRAL_ALPHA 128.0f

#if PS_HDR > 1
#define ctype float
#define ctype3 float3
#define ctype4 float4
#define HDR_FLT_THRESHOLD 0.0001f
#define OUTPUT_MAX FLT_MAX
#else
#define ctype uint
#define ctype3 uint3
#define ctype4 uint4
#define OUTPUT_MAX 0x7FFFFFFF
#endif

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

// The source color
Texture2D<float4> Texture : register(t0);
// LUT
Texture2D<float4> Palette : register(t1);
// The same texture we are writing to
Texture2D<float4> RtTexture : register(t2);
Texture2D<float> PrimMinTexture : register(t3);
// The current color source sampler
SamplerState TextureSampler : register(s0);
// A forced linear sampler for any usage
SamplerState TextureLinearSampler : register(s1);

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
	float4 LODParams;
	float4 STRange;
	int4 ChannelShuffle;
	float2 TC_OffsetHack;
	float2 STScale;
	float4x4 DitherMatrix;
	float ScaledScaleFactor;
	float RcpScaleFactor;
};

float fmod_positive(float a, float b)
{
	return fmod(fmod(a, b) + b, b);
}
float3 fmod_positive(float3 a, float b)
{
	return float3(fmod_positive(a.x, b), fmod_positive(a.y, b), fmod_positive(a.z, b));
}

// BT.601 (PS2's most likely color space)
float luminance(float3 c)
{
	return dot(c, float3(0.299, 0.587, 0.144));
}

// Code from Filippo Tarpini, from the "Luma Framework" (https://github.com/Filoppi/Luma-Framework/)
// 
// LUT sample that allows to go beyond the 0-1 coordinates range through extrapolation.
// It finds the rate of change (acceleration) of the LUT color around the requested clamped coordinates, and guesses what color the sampling would have with the out of range coordinates.
// Extrapolating LUT by re-apply the rate of change has the benefit of consistency. If the LUT has the same color at (e.g.) uv 0.9 0.9 and 1.0 1.0, thus clipping to white or black, the extrapolation will also stay clipped.
// Additionally, if the LUT had inverted colors or highly fluctuating colors, extrapolation would work a lot better than a raw LUT out of range extraction with a luminance multiplier.
//
// Note that this function might return "invalid colors", they could have negative values etc etc, so make sure to clamp them after if you need to.
float4 sampleLUTWithExtrapolation(Texture2D<float4> lut, float unclampedPixelU)
{
  // LUT size in texels
  float lutWidth; // 256.0;
  float lutHeight; // 1.0;
  lut.GetDimensions(lutWidth, lutHeight);
  const float2 lutSize = float2(lutWidth, lutHeight);
  const float2 lutMax = lutSize - 1.0;
  const float2 uvScale = lutMax / lutSize;        // Also "1-(1/lutSize)"
  const float2 uvOffset = 1.0 / (2.0 * lutSize);  // Also "(1/lutSize)/2"

  float2 unclampedUV = float2(unclampedPixelU / lutMax.x, 0.5);
  const float2 clampedUV = saturate(unclampedUV);
  const float distanceFromUnclampedToClamped = abs(unclampedUV.x - clampedUV.x);
  const bool uvOutOfRange = distanceFromUnclampedToClamped > FLT_MIN; // Some threshold is needed to avoid divisions by tiny numbers

  const float4 clampedSample = lut.Sample(TextureLinearSampler, (clampedUV * uvScale) + uvOffset).xyzw;

  if (uvOutOfRange)
  {
    // Travel backwards down the LUT by 25% (anything from 1 texel backwards to 33% should be good)
    float backwardsOffset = 0.25;
    float2 centeredUV = float2(clampedUV.x >= 0.5 ? max(clampedUV.x - backwardsOffset, 0.5) : min(clampedUV.x + backwardsOffset, 0.5), unclampedUV.y);
    const float4 centeredSample = lut.Sample(TextureLinearSampler, (centeredUV * uvScale) + uvOffset).xyzw;
    const float distanceFromClampedToCentered = abs(clampedUV.x - centeredUV.x);
    const float extrapolationRatio = distanceFromClampedToCentered == 0.0 ? 0.0 : (distanceFromUnclampedToClamped / distanceFromClampedToCentered);
    const float4 extrapolatedSample = lerp(centeredSample, clampedSample, 1.0 + extrapolationRatio);
    return extrapolatedSample;
  }
  return clampedSample;
}

// Takes normalized input
float4 DecodeTex(float4 C, float quantization_rgb = 255.0f, float quantization_a = 255.0f)
{
#if PS_HDR //TODO1: do this for SDR as well in case we allowed HDR textures? They might actually have negative values...
	// Theoretically we force subtractive blends that could cause negative values to run in SW (and thus get clamped), but that might not always happen, so we clamp here for safety
	C = max(C, 0.f);
#endif

	//TODO: a lot of the code in this file quantizes ("trunc(x+0.5)") again after decoding the texture, we could probably make this function to do it for all needed cases, in SDR and HDR
#if PS_HDR == 1
	// Re-quantize all source colors to 8bit
	if (quantization_rgb != 0.0f)
		C.rgb = round(C.rgb * quantization_rgb) / quantization_rgb;
#endif
#if PS_HDR >= 1 && PS_HDR <= 2
	if (quantization_a != 0.0f)
		C.a = round(C.a * quantization_a) / quantization_a;
#endif

	return C;
}

float4 DecodeCTex(float4 C)
{
	return DecodeTex(C, 255.0f, PS_RTA_SRC_CORRECTION ? NEUTRAL_ALPHA : 255.0f);
}
float4 DecodeRTTex(float4 C)
{
	return DecodeTex(C, PS_COLCLIP_HW ? 65535.0f : 255.0f, PS_RTA_CORRECTION ? NEUTRAL_ALPHA : 255.0f);
}

// Takes 0-255 input
float4 EncodeTex(float4 C)
{
#if PS_HDR == 1
	C.rgb = round(C.rgb);
#endif
#if PS_HDR >= 1 && PS_HDR <= 2
	C.a = round(C.a);
#endif

#if 0 // There should be no negative values at this point
	C = max(C, 0.f);
#endif

	return C;
}

float4 sample_c(float2 uv, float uv_w)
{
#if PS_TEX_IS_FB == 1
	return DecodeCTex(RtTexture.Load(int3(int2(uv * WH.zw), 0)));
#elif PS_REGION_RECT == 1
	return DecodeCTex(Texture.Load(int3(int2(uv), 0)));
#else
	if (PS_POINT_SAMPLER)
	{
		// Weird issue with ATI/AMD cards,
		// it looks like they add 127/128 of a texel to sampling coordinates
		// occasionally causing point sampling to erroneously round up.
		// I'm manually adjusting coordinates to the centre of texels here,
		// though the centre is just paranoia, the top left corner works fine.
		// As of 2018 this issue is still present.
#if 0 // Shouldn't be done on NV? //TODO
		uv = (trunc(uv * WH.zw) + float2(0.5, 0.5)) / WH.zw;
#endif
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
	// This wil automatically pick the appropriate mip if the sampler is linear
	return DecodeCTex(Texture.Sample(TextureSampler, uv));
#elif PS_MANUAL_LOD == 1
	// PS2 mip formula
	// FIXME add LOD: K - ( LOG2(Q) * (1 << L))
	float K = LODParams.x;
	float L = LODParams.y;
	float bias = LODParams.z;
	float max_lod = LODParams.w;

	float gs_lod = K - log2(abs(uv_w)) * L;
	// FIXME max useful ?
	//float lod = max(min(gs_lod, max_lod) - bias, 0.0f);
	float lod = min(gs_lod, max_lod) - bias;

	return DecodeCTex(Texture.SampleLevel(TextureSampler, uv, lod));
#else
	return DecodeCTex(Texture.SampleLevel(TextureSampler, uv, 0)); // No lod
#endif
#endif
}

// Samples a 255x1 palette that outputs 4 channel from 1
float4 sample_p(ctype u)
{
	if (PS_HDR)
	{
#if PS_HDR > 1
		float4 p = sampleLUTWithExtrapolation(Palette, u);
		p = max(p, 0.f); // Any negative value should probably be clamped as it'd be accidental and have negative consequences
#if PS_HDR <= 2
		// Always make sure alpha is quantized
		p.a = round(p.a * 255.0f) / 255.0f;
#endif
#else // Old basic extrapolation code (it doesn't work well with wierd LUTs)
		float2 size;
		Palette.GetDimensions(size.x, size.y);
		// Y is always 1 texel large
		float excess = max(u - (size.x - 1.f), 0.f) / (size.x - 1.f);
		float4 p = Palette.Load(int3(int(min(round(u), size.x - 1.f)), 0, 0));
		p.rgb *= excess + 1.f; // Don't extrapolate alpha
#endif
		return p;
	}
	return Palette.Load(int3(min(int(u), 255), 0, 0));
}

// Samples a palette based on any rgba color channel
float4 sample_p_norm(float u)
{
#if PS_HDR
	return sample_p(u * 255.0f);
#else
	return sample_p(u * 255.0f + 0.5f); // Denormalize value (and round so that 254.5 to 255 map to the last texel)
#endif
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

// Returns palette input coordinates sampled from color alphas
ctype4 sample_4_index(float4 uv, float uv_w)
{
	float4 c;

	c.x = sample_c(uv.xy, uv_w).a;
	c.y = sample_c(uv.zy, uv_w).a;
	c.z = sample_c(uv.xw, uv_w).a;
	c.w = sample_c(uv.zw, uv_w).a;

	ctype4 i;
	
#if PS_HDR <= 1
	if (PS_RTA_SRC_CORRECTION)
	{
		i = ctype4(round(c * (NEUTRAL_ALPHA + 0.25f))); // Denormalize values (with a slight empirically found offset to better round it) (they are all alphas)
	}
	else
	{
		i = ctype4(c * 255.f + 0.5f); // Denormalize value (so that 254.5 to 255 map to the last texel)
	}
#else
	i = c * (PS_RTA_SRC_CORRECTION ? NEUTRAL_ALPHA : 255.0f);
#endif

	// Remap coordinates for the current palette size (range)
	if (PS_PAL_FMT == 1)
	{
		// 4HL
#if PS_HDR <= 1
		return i & 0xFu;
#else
		// Note: negative handling is a bit random here but it should be fine
		return fmod(i, 16.0f);
#endif
	}
	else if (PS_PAL_FMT == 2)
	{
		// 4HH
#if PS_HDR <= 1
		return i >> 4u; // i / 16 with truncation
#else
		return i / 16.0f;
#endif
	}
	else
	{
		// 8
		return i;
	}
}

// Samples palette 4 times based on 4 coordinates
float4x4 sample_4p(ctype4 u)
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
	return DecodeCTex(RtTexture.Load(int3(xy, 0)));
#else
	return DecodeCTex(Texture.Load(int3(xy, 0)));
#endif
}

// For depth
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
		t = float4(uint4((d & 0x1Fu), ((d >> 5) & 0x1Fu), ((d >> 10) & 0x1Fu), (d >> 15) & 0x01u)) * float4(8.0f, 8.0f, 8.0f, NEUTRAL_ALPHA);
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
		t.a = t.a >= NEUTRAL_ALPHA ? 255.0f * TA.y : ((PS_AEM == 0) || any(bool3(t.rgb))) ? 255.0f * TA.x : 0.0f;
	}
	else if (PS_PAL_FMT != 0 && !PS_TALES_OF_ABYSS_HLE && !PS_URBAN_CHAOS_HLE)
	{
		t = sample_4p(ctype4(t.aaaa))[0] * 255.0f;
		t = trunc(t + 0.5f);
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

// See PS_SHUFFLE for more
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
		float4 rt_float = fetch_raw_color(xy) * 255.0f;
#if PS_HDR > 1 // The other HDR cases should already be clamped and quantized
		rt_float = min(rt_float, 255.0f); // We simply clamp HDR values here, it's not common enough to bother supporting a float/HDR bit mask (see fbmask in case, it does that)
#endif
		int4 rt = (int4)(rt_float + 0.5f);
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
#if PS_HDR > 2
			// Any 16bit texture would have clipped values below 7 (the first 5bit rgb sample maps to 0, the second one to 8, on a 0-255 scale),
			// given that in HDR these have increased accuracy, we still need to give it some threshold to assume everything is black,
			// to emulate the original test result. We pick 4 instead of ~7.5 as that's the mid point.
			// We don't take any tolerance in HDR, alpha 127.5 to ~127.999 needs to get rejected, or we'd risk passing more values that would have gotten truncated and rejected in SDR.
			c[i].a = c[i].a >= ((NEUTRAL_ALPHA / 255.0f) - HDR_FLT_THRESHOLD) ? TA.y : !PS_AEM || any((c[i].rgb * 255.0f) > 4.f) ? TA.x : 0;
#else
			// Check if the color is black (we discared the last 7 values, as these bits don't exist on real HW)
			// 0.5 is already between 127 and 128 (the test should originally pass if >= 128).
			c[i].a = c[i].a >= 0.5 ? TA.y : !PS_AEM || any(int3(c[i].rgb * 255.0f) & 0xF8) ? TA.x : 0;
#endif
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

	if (PS_AEM_FMT == FMT_32 && PS_PAL_FMT == 0 && PS_RTA_SRC_CORRECTION)
	{
		t.a *= NEUTRAL_ALPHA / 255.0f;
	}

	t *= 255.0f;
#if PS_HDR <= 1
	t = trunc(t + 0.5f);
#endif
	return t;
}

// Texture function
// T: Texture color (0-255)
// C: Vertex color (0-255)
float4 tfx(const float4 T, const float4 C)
{
	float4 C_out;
	// On console this clamps to 255 (it's divided by 128 (in integer, so with truncation) as it does >>7 on console, making 128 the neutral vertex color),
	// but the formula can actually generate HDR brightnesses (beyond 255) if we skip clamping (avoiding truncation can lead to higher average brightnesses as well, especially on recursive effects like bloom)
	float4 FxT = (C * T) / 128.0f;
	if (PS_HDR <= 1)
		FxT = trunc(FxT);
	else if (PS_HDR <= 2)
		FxT.a = trunc(FxT.a);
	if (PS_HDR >= 2 && PS_BALANCE_HDR)
		FxT.rgb *= 1.0f - (0.5f / 128.0f);

	// Allow retaining any alpha beyond 255 if it was already in the source color (but if this function increased it, we avoid it growing further beyond 255).
	// This is useful to retain HDR values in green-alpha shuffles.
	float alpha_max = T.a;

// Modulate
// Multiplies texture by vertex (can generate HDR brightnesses)
#if (PS_TFX == 0)
	C_out = FxT;
// Decal
// Ignore vertex color
#elif (PS_TFX == 1)
	C_out = T;
// Highlight
// Additive vertex color (can generate HDR brightnesses)
#elif (PS_TFX == 2)
	C_out.rgb = FxT.rgb + C.a;
	C_out.a = T.a + C.a;
// Highlight 2
#elif (PS_TFX == 3)
	C_out.rgb = FxT.rgb + C.a;
	C_out.a = T.a;
#else
	C_out = C;
	alpha_max = 255.0f;
#endif

#if (PS_TCC == 0)
	C_out.a = C.a;
	alpha_max = 255.0f;
#endif

#if (PS_TFX == 0) || (PS_TFX == 2) || (PS_TFX == 3)
	// Clamp only when it is useful
	if (PS_HDR == 0)
		C_out = min(C_out, 255.0f);
	else
		C_out.a = min(C_out.a, max(alpha_max, 255.0f));
#endif

	return C_out;
}

// Alpha test (in 0-255 range)
bool atst(float4 C)
{
	float a = C.a;

	float a_test = AREF;

	// Less
	if(PS_ATST == 1)
	{
		return (a <= a_test);
	}
	// Greater
	else if(PS_ATST == 2)
	{
		return (a >= a_test);
	}
	// Equal
	else if(PS_ATST == 3)
	{
		 return (abs(a - a_test) <= 0.5f);
	}
	// Not equal
	else if(PS_ATST == 4)
	{
		return (abs(a - a_test) >= 0.5f);
	}
	else
	{
		// nothing to do
		return true;
	}
}

// See PS_SHUFFLE for more
float4 shuffle(float4 C, bool true_read_false_write)
{
#if PS_HDR > 1 // The other HDR cases should already be clamped and quantized
	C = min(C, 255.0f);
#endif
	uint4 denorm_c_before = uint4(C + 0.5f); 
	if (PS_PROCESS_BA & (true_read_false_write ? SHUFFLE_READ : SHUFFLE_WRITE))
	{
		// rgba from b-ba-a-a
		C.r = float((denorm_c_before.b << 3) & 0xF8u);
		C.g = float(((denorm_c_before.b >> 2) & 0x38u) | ((denorm_c_before.a << 6) & 0xC0u));
		C.b = float((denorm_c_before.a << 1) & 0xF8u);
		C.a = float(denorm_c_before.a & 0x80u);
	}
	else // PS_PROCESS_RG & SHUFFLE_READ/SHUFFLE_WRITE
	{
		// rgba from r-rg-g-g
		C.r = float((denorm_c_before.r << 3) & 0xF8u);
		C.g = float(((denorm_c_before.r >> 2) & 0x38u) | ((denorm_c_before.g << 6) & 0xC0u));
		C.b = float((denorm_c_before.g << 1) & 0xF8u);
		C.a = float(denorm_c_before.g & 0x80u);
	}
	return C;
}

// Depth based fog
float4 fog(float4 c, float f)
{
	if(PS_FOG)
	{
		// PS2 formula is "((f * c) + ((256 - f) * fc)) >> 8" in unsigned integer, in a 0-255 range (f also from 0 to 255).
		// This would always clip the maximum value to 254.
		if (PS_HDR <= 1)
		{
			f = trunc(f * 255.0f + 0.5f);
			c.rgb = trunc(((f * c.rgb) + ((256.0f - f) * FogColor)) / 256.0f);
		}
		// Anything that goes through fog is unlikely to expect specific rgb quantized values (e.g. stencil tests) in later passes,
		// by the nature of its unreliability, so it's ok to do a lerp instead
		else
		{
			c.rgb = lerp(FogColor, c.rgb, f);
		}
	}

	return c;
}

float4 ps_color(PS_INPUT input, out float4 pre_shuffle_C)
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

	pre_shuffle_C = T;

	if (PS_SHUFFLE && !PS_SHUFFLE_SAME && !PS_READ16_SRC && !(PS_PROCESS_BA == SHUFFLE_READWRITE && PS_PROCESS_RG == SHUFFLE_READWRITE))
	{
		T = shuffle(T, true);
		
#if PS_HDR > 2
		// Any 16bit texture would have clipped values below 7 (the first 5bit rgb sample maps to 0, the second one to 8, on a 0-255 scale),
		// given that in HDR these have increased accuracy, we still need to give it some threshold to assume everything is black,
		// to emulate the original test result. We pick 4 instead of ~7.5 as that's the mid point.
		T.a = (T.a >= (NEUTRAL_ALPHA - HDR_FLT_THRESHOLD * NEUTRAL_ALPHA) ? TA.y : !PS_AEM || any(T.rgb > 4.f) ? TA.x : 0) * 255.0f;
#else
		// Check if the color is black (we discared the last 7 values, as these bits don't exist on real HW)
		T.a = (T.a >= (NEUTRAL_ALPHA - 0.5f) ? TA.y : !PS_AEM || any(int3(T.rgb) & 0xF8) ? TA.x : 0) * 255.0f;
#endif
	}

	float4 C = tfx(T, input.c);

	C = fog(C, input.t.z);

	return C;
}

// Frame buffer mask
// This will read the current frame buffer (render target) and mask our color with it
void ps_fbmask(inout float4 C, float2 pos_xy)
{
	if (PS_FBMASK)
	{
		if (PS_HDR > 1 && !PS_COLCLIP_HW) // Don't do this in colclip hw mode as it can't output "HDR" (theoretically "PS_HDR" would already be disabled for that case)
		{
			float4 RT = DecodeRTTex(RtTexture.Load(int3(pos_xy, 0))) * 255.0f;
			bool4 lo_bit = (FbMask & 0x01) != 0;
			bool4 hi_bit = (FbMask & 0x80) != 0;
			// Only clip when necessary (if "hi_bit" is false, the value is < 128)
			RT = hi_bit ? RT : min(RT, 255.0f);
			C  = hi_bit ? min(C, 255.0f) : C;
			uint4 RTi = (uint4)(RT + 0.5f);
			uint4 Ci  = (uint4)(C  + 0.5f);
			// Sign extend mask.
			// The native mask in 8bit, by shifting it left and then right again, we make the topmost bit (highest value) trail on all bits to the left of it,
			// which will allow us to take all the values beyond 255 from that color.
			uint4 mask = ((int4)FbMask << 24) >> 24;
			C = (float4)((Ci & ~mask) | (RTi & mask));
			// Retain any fractional value from the color with the lowest bit mask, to avoid quantizing to 8 bit.
			// This hopefully never hurts stencil tests that depend on this mask.
			if (PS_HDR >= 3) //TODO1: evaluate if this is even needed, this stuff is used for stencils etc. This makes Sly and Spyro (A New Beginning) shadow darker!
				C += lo_bit ? (RT - (float4)RTi) : (C - (float4)Ci);
		}
		else
		{
			float multi = PS_COLCLIP_HW ? 65535.0f : 255.0f;
			float4 RT = DecodeRTTex(RtTexture.Load(int3(pos_xy, 0))) * multi;
			C = (float4)(((uint4)(C + 0.5f) & ~FbMask) | ((uint4)(RT + 0.5f) & FbMask));
		}
	}
}

void ps_dither(inout float3 C, float As, float2 pos_xy)
{
	// On PS2 dithering only happened when writing to RGB5A1 textures.
	// Dithering shouldn't be particularly needed in HDR given it upgrades all textures and doesn't quantize to 5 or 8 bit color during calculations,
	// it might still look good in some cases though, for example the source color texture was 5bpc, but that's not a case that has ever been accounted for.
	// Theoretically games could have also used dithering to darker or brighten up textures (by setting the dither matrix to all positive or all negative values),
	// but in practice that never seems to have happened, so we don't need to emulate that behaviour if we skip dithering.
	if (PS_DITHER > 0 && PS_DITHER < 3 && PS_HDR <= 1)
	{
		int2 fpos;

		if (PS_DITHER == 2)
			fpos = int2(pos_xy);
		else
			fpos = int2(pos_xy * RcpScaleFactor);

		float value = DitherMatrix[fpos.x & 3][fpos.y & 3];
		
		// The idea here is we add on the dither amount adjusted by the alpha before it goes to the hw blend
		// so after the alpha blend the resulting value should be the same as (Cs - Cd) * As + Cd + Dither.
		if (PS_DITHER_ADJUST)
		{
			float Alpha = PS_BLEND_C == 2 ? Af : As;
			value *= Alpha > 0.0f ? min(1.0f / Alpha, 1.0f) : 1.0f;
		}

		if (PS_ROUND_INV)
			C -= value;
		else
			C += value;
	}
}

void ps_color_clamp_wrap(inout float3 C)
{
	uint mask = 0;

	// When dithering the bottom 3 bits become meaningless and cause lines in the picture
	// so we need to limit the color depth on dithered items
	// 0xF8 is 248, which is "11111000" (lower 3 bits are masked off, as in, the lower values)
	if (SW_BLEND || (PS_DITHER > 0 && PS_DITHER < 3) || PS_FBMASK)
	{
		// If the blend equation subtracts src color, the ps2 rounds down after negating, but the shader is running pre-negate, so we need to round up instead, so add an offset to turn the round down into a round up.
		// It should be when working with floats with no decimal, think (x + 7) & ~7 in integer, but with floats instead.
		// "PS_ROUND_INV" implies hardware blends are active.
		if (PS_DST_FMT == FMT_16 && PS_BLEND_MIX == 0 && PS_ROUND_INV && PS_HDR <= 1)
			C += 7.0f; // 0xFF - 0xF8

		// Standard Clamp (alpha is never directly edited by blends (at worse it's shuffled), so we don't need to clamp it)
		if (PS_COLCLIP == 0 && PS_COLCLIP_HW == 0)
		{
			if (PS_HDR == 0 || PS_HDR_FORCE_OFF)
				C = clamp(C, 0.0f, 255.0f);
			else // Games use subtractive blends (-1) to clear render targets to black, so we need to clip them to 0 (anyway without this, bloom can often can go negative and make the scene darker, and colclip hw wouldn't wrap correctly on initialization)
				C = clamp(C, 0.0f, 255.0f * pow(10000.f / 203.f, 1.f / 2.35f)); //TODO: limit this anyway in HDR for safety? Maybe to 4 times? Or 2 times?
		}

		// In 16 bits format, only 5 bits of color are used. It impacts shadows computation of Castlevania
		// With "PS_DITHER" set to 3, we force all RTs to be treated as 16bpc, and avoid clamping them
		// to 248 as they used to be (this can make the final image a bit brighter, but fixes banding).
		// colclip hw could still be enabled in this case.
		if (PS_DST_FMT == FMT_16 && PS_DITHER != 3 && (PS_BLEND_MIX == 0 || PS_DITHER) && PS_HDR <= 1)
			mask = 0xF8;
		else if (PS_COLCLIP == 1 || PS_COLCLIP_HW == 1)
			mask = 0xFF;
	}
	else if (PS_DST_FMT == FMT_16 && PS_DITHER != 3 && PS_BLEND_MIX == 0 && PS_BLEND_HW == 0)
		mask = 0xF8;

	if (mask != 0)
	{
#if PS_HDR >= 2 // Avoid quantization to 8bit in HDR (note that colclip hw will still round to the closest 0-255 value on write, which could lead to extra wraps)
#if 0 // Ignore mapping to 0-248 for 5bpc textures, we don't want quantization in HDR (also theoretically we should apply both masks/fmods if colclip hw is enabled!)
		if (mask == 0xF8)
			C = C - fmod_positive(C, 8.0f); // Round down to the nearest multiple of 8
#endif
		if (mask == 0xFF)
			C = fmod_positive(C, 256.0f); // Wrapping needs to be mirrored for negative values to best emulate original HW
#else
		C = (float3)((int3)C & (int3)mask); //TODO1: breaks in HDR on strong lights if dithering is off...
#endif
	}
	if (PS_HDR >= 2 && PS_BALANCE_HDR && mask != 0xFF)
		C *= 1.0f - (4.0f / 256.0f);
}

void ps_blend(inout float4 Color, inout float4 As_rgba, float2 pos_xy, out float4 pre_shuffle_RT)
{
	float As = As_rgba.a;

	if (SW_BLEND)
	{
		// PABE
		if (PS_PABE)
		{
			// As_rgba needed for accumulation blend to manipulate Cd.
			// No blending so early exit
			if (As < 1.0f)
			{
				As_rgba.rgb = (float3)0.0f;
				return;
			}

			As_rgba.rgb = (float3)1.0f;
		}

		float color_multi = PS_COLCLIP_HW ? 65535.0f : 255.0f;
		float4 RT = SW_BLEND_NEEDS_RT ? DecodeRTTex(RtTexture.Load(int3(pos_xy, 0))) : (float4)0.0f;
		pre_shuffle_RT = RT;

		if (PS_SHUFFLE && SW_BLEND_NEEDS_RT)
		{
			RT = shuffle(RT, false);
		}

		float Ad = (RT.a * (PS_RTA_CORRECTION ? NEUTRAL_ALPHA : 255.0f)) / NEUTRAL_ALPHA;
		if (PS_HDR <= 2) // RT Alpha rounding is probably already done while decoding the texture in these HDR modes, but let's do it again for extra safety
		{
			Ad = trunc(Ad + 0.5f / NEUTRAL_ALPHA);
		}
		float3 Cd = RT.rgb * color_multi;
		if (PS_HDR <= 1)
		{
			Cd = trunc(Cd + 0.5f);
		}
		float3 Cs = Color.rgb;

		// The proper formula on PS2 is "(((A - B) * C) >> 7) + D" (>>7 is equivalent to dividing by 128 with truncation), with it working in signed integer,
		// and the blend output being constrained to a 9bit register during clamp/wrap (it can go negative, up to -512, or beyond 255, up to 511, with -1 and 511 being identical in bits)
		float3 A = (PS_BLEND_A == 0) ? Cs : ((PS_BLEND_A == 1) ? Cd : (float3)0.0f);
		float3 B = (PS_BLEND_B == 0) ? Cs : ((PS_BLEND_B == 1) ? Cd : (float3)0.0f);
		float  C = (PS_BLEND_C == 0) ? As : ((PS_BLEND_C == 1) ? Ad : Af); // Source color alpha or Target color alpha or Fixed alpha (from CPU)
		float3 D = (PS_BLEND_D == 0) ? Cs : ((PS_BLEND_D == 1) ? Cd : (float3)0.0f);

		// As/Af clamp alpha for Blend mix
		// We shouldn't clamp blend mix with blend hw 1 as we want alpha higher
		float C_clamped_hw = C;
		float C_clamped_sw = C;
		if (PS_BLEND_MIX > 0 && PS_BLEND_HW != 1 && PS_BLEND_HW != 2)
			C_clamped_hw = saturate(C_clamped_hw);
		if (PS_HDR)
		{
			C_clamped_sw = clamp(0.f, C_clamped_sw, 255.0f / NEUTRAL_ALPHA); // Clamp to 1.9921875, the max PS2 alpha, it could have gone beyond in HDR
			if (PS_HDR >= 2 && PS_BALANCE_HDR)
				C_clamped_sw *= 1.0f - (0.5f / 128.0f);
		}

		// A and B nullify each other, we can skip the whole thing
		if (PS_BLEND_A == PS_BLEND_B)
			Color.rgb = D;
		else if (PS_BLEND_MIX == 2)
			Color.rgb = (A - B) * C_clamped_hw + D;
		else if (PS_BLEND_MIX == 1)
			Color.rgb = (A - B) * C_clamped_hw + D;
		else
			Color.rgb = (A - B) * C_clamped_sw + D;

		if (PS_BLEND_A != PS_BLEND_B)
		{
			// In blend_mix, HW adds on some alpha factor * dst.
			// Truncating here wouldn't quite get the right result because it prevents the <1 bit here from combining with a <1 bit in dst to form a ≥1 amount that pushes over the truncation.
			// Instead, apply an offset to convert HW's round to a floor.
			// Since alpha is in 1/128 increments (with 128 being neutral alpha (1)), subtracting (0.5 - 0.5/128 == 127/256) would get us what we want if GPUs blended in full precision.
			// But they don't.  Details here: https://github.com/PCSX2/pcsx2/pull/6809#issuecomment-1211473399
			// Based on the scripts at the above link, the ideal choice for Intel GPUs is 126/256, AMD 120/256.  Nvidia is a lost cause.
			// 124/256 seems like a reasonable compromise, providing the correct answer 99.3% of the time on Intel (vs 99.6% for 126/256), and 97% of the time on AMD (vs 97.4% for 120/256).
			if (PS_BLEND_MIX == 2 && !PS_HDR)
				Color.rgb += 124.0f / 256.0f;
			else if (PS_BLEND_MIX == 1 && !PS_HDR)
				Color.rgb -= 124.0f / 256.0f;
			// Flooring the color here to properly replicate the original formula. This will also quantize the color to 8bpc.
			// Skipping this will often make post process effects like bloom brighter, as every pass not being truncated results in higher values (which might then influence further calculations).
			else if (PS_BLEND_MIX == 0 && PS_HDR <= 1)
				Color.rgb = floor(Color.rgb);
		}

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
			// Since we can't do Cd*(Alpha + 1) - Cs*Alpha in hw blend
			// what we can do is adjust the Cs value that will be
			// subtracted, this way we can get a better result in hw blend.
			// Result is still wrong but less wrong than before.
			float division_alpha = 1.0f + C;
			Color.rgb /= (float3)division_alpha;
		}
		else if (PS_BLEND_HW == 3)
		{
			// As, Ad or Af clamped.
			As_rgba.rgb = (float3)C_clamped_hw;
			// Cs*(Alpha + 1) might overflow, if it does then adjust alpha value
			// that is sent on second output to compensate.
			float3 overflow_check = (Color.rgb - (float3)255.0f) / 255.0f;
			float3 alpha_compensate = max((float3)0.0f, overflow_check);
			As_rgba.rgb -= alpha_compensate;
		}
	}
	else
	{
		pre_shuffle_RT = 0;

		float3 Alpha = PS_BLEND_C == 2 ? (float3)Af : (float3)As;

		if (PS_BLEND_HW == 1)
		{
			// Needed for Cd * (As/Ad/F + 1) blending modes
			Color.rgb = (float3)255.0f;
		}
		else if (PS_BLEND_HW == 2)
		{
			// Cd*As,Cd*Ad or Cd*F
			Color.rgb = saturate(Alpha - (float3)1.0f) * (float3)255.0f;
		}
		else if (PS_BLEND_HW == 3 && PS_RTA_CORRECTION == 0)
		{
			// Needed for Cs*Ad, Cs*Ad + Cd, Cd - Cs*Ad
			// Multiply Color.rgb by (255/128) to compensate for wrong Ad/255 value when rgb are below 128.
			// When any color channel is higher than 128 then adjust the compensation automatically
			// to give us more accurate colors, otherwise they will be wrong.
			// The higher the value (>128) the lower the compensation will be.
			float max_color = max(max(Color.r, Color.g), Color.b);
			float color_compensate = 255.0f / max(NEUTRAL_ALPHA, max_color);
			Color.rgb *= (float3)color_compensate;
		}
		else if (PS_BLEND_HW == 4)
		{
			// Needed for Cd * (1 - Ad) and Cd*(1 + Alpha).
			As_rgba.rgb = Alpha * (float3)(NEUTRAL_ALPHA / 255.0f);
			Color.rgb = (float3)127.5f;
		}
		else if (PS_BLEND_HW == 5)
		{
			// Needed for Cs*Alpha + Cd*(1 - Alpha).
			Alpha *= (float3)(NEUTRAL_ALPHA / 255.0f);
			As_rgba.rgb = (Alpha - (float3)0.5f);
			Color.rgb = (Color.rgb * Alpha);
		}
		else if (PS_BLEND_HW == 6)
		{
			// Needed for Cd*Alpha + Cs*(1 - Alpha).
			Alpha *= (float3)(NEUTRAL_ALPHA / 255.0f);
			As_rgba.rgb = Alpha;
			Color.rgb *= (Alpha - (float3)0.5f);
		}
	}
}

PS_OUTPUT ps_main(PS_INPUT input)
{
	// In 0-255 range
	float4 pre_shuffle_C;

	float4 C = ps_color(input, pre_shuffle_C);

	bool atst_pass = atst(C);

#if PS_AFAIL == 0 // KEEP or ATST off
	if (!atst_pass)
		discard;
#endif

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
		C.a = NEUTRAL_ALPHA;
	}

	float4 alpha_blend = (float4)0.0f;
	if (SW_AD_TO_HW)
	{
		float RTa = DecodeRTTex(RtTexture.Load(int3(input.p.xy, 0))).a * (PS_RTA_CORRECTION ? NEUTRAL_ALPHA : 255.0f);
		if (PS_HDR <= 2) // RT Alpha rounding is probably already done while decoding the texture in these HDR modes, but let's do it again for extra safety
		{
			RTa = trunc(RTa + 0.5f);
		}
		alpha_blend = (float4)(RTa / NEUTRAL_ALPHA);
	}
	else
	{
		alpha_blend = (float4)(C.a / NEUTRAL_ALPHA);
	}

	// Alpha correction
	if (PS_DST_FMT == FMT_16)
	{
		float A_one = NEUTRAL_ALPHA; // alpha output will be 0x80
		C.a = PS_FBA ? A_one : step(A_one, C.a) * A_one;
	}
	else if ((PS_DST_FMT == FMT_32) && PS_FBA)
	{
		float A_one = NEUTRAL_ALPHA;
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
#if PS_HDR > 2
	output.c = (C.a >= (NEUTRAL_ALPHA - HDR_FLT_THRESHOLD * NEUTRAL_ALPHA)) ? float(input.primid) : float(OUTPUT_MAX);
#else
	output.c = (C.a > (NEUTRAL_ALPHA - 0.5f)) ? float(input.primid) : float(OUTPUT_MAX);
#endif

#elif PS_DATE == 2

	// DATM == 1
	// Pixel with alpha equal to 0 will failed (0-127)
#if PS_HDR > 2
	output.c = (C.a <= (NEUTRAL_ALPHA - HDR_FLT_THRESHOLD * NEUTRAL_ALPHA)) ? float(input.primid) : float(OUTPUT_MAX);
#else
	output.c = (C.a < (NEUTRAL_ALPHA - 0.5f)) ? float(input.primid) : float(OUTPUT_MAX);
#endif

#else // Not primid DATE setup

	float4 pre_shuffle_RT;
	ps_blend(C, alpha_blend, input.p.xy, pre_shuffle_RT);

	if (PS_SHUFFLE)
	{
		// Same conditions for the source color "shuffle" (generally "PS_SHUFFLE_ACROSS" will be true as well).
		// BA and RG shuffles can either both be read and write, or one will be read and one will be write.
		if (!PS_SHUFFLE_SAME && !PS_READ16_SRC && !(PS_PROCESS_BA == SHUFFLE_READWRITE && PS_PROCESS_RG == SHUFFLE_READWRITE))
		{
#if PS_HDR > 1 // The other HDR cases should already be clamped and quantized
			C = min(C, 255.0f);
#endif
			uint4 denorm_c_after = uint4(C + 0.5f);
			if (PS_PROCESS_BA & SHUFFLE_READ)
			{
				// b-a from rgba
				C.b = float(((denorm_c_after.r >> 3) & 0x1Fu) | ((denorm_c_after.g << 2) & 0xE0u));
				C.a = float(((denorm_c_after.g >> 6) & 0x3u) | ((denorm_c_after.b >> 1) & 0x7Cu) | (denorm_c_after.a & 0x80u));
			}
			else // PS_PROCESS_RG & SHUFFLE_READ
			{
				// r-g from rgba
				C.r = float(((denorm_c_after.r >> 3) & 0x1Fu) | ((denorm_c_after.g << 2) & 0xE0u));
				C.g = float(((denorm_c_after.g >> 6) & 0x3u) | ((denorm_c_after.b >> 1) & 0x7Cu) | (denorm_c_after.a & 0x80u));
			}
		}

		// Special case for 32bit input and 16bit output, shuffle used by The Godfather
		if (PS_SHUFFLE_SAME)
		{
#if PS_HDR > 1 // We simply clamp HDR values here, it's not common enough to bother supporting a float mask
			C = min(C, 255.0f);
#endif
			uint4 denorm_c = uint4(C + 0.5f);
			
			if (PS_PROCESS_BA & SHUFFLE_READ)
				C = (float4)(float((denorm_c.b & 0x7Fu) | (denorm_c.a & 0x80u)));
			else // PS_PROCESS_RG & SHUFFLE_READ
				C.ga = C.rg;
		}
		// Copy of a 16bit source in to this target
		else if (PS_READ16_SRC)
		{
#if PS_HDR > 1 // We simply clamp HDR values here, it's not common enough to bother supporting a float mask
			C = min(C, 255.0f);
#endif
			uint4 denorm_c = uint4(C + 0.5f);
			uint2 denorm_TA = uint2(TA.xy * 255.0f + 0.5f);
			C.rb = (float2)float((denorm_c.r >> 3) | (((denorm_c.g >> 3) & 0x7u) << 5));
			if (denorm_c.a & 0x80u)
				C.ga = (float2)float((denorm_c.g >> 6) | ((denorm_c.b >> 3) << 2) | (denorm_TA.y & 0x80u));
			else
				C.ga = (float2)float((denorm_c.g >> 6) | ((denorm_c.b >> 3) << 2) | (denorm_TA.x & 0x80u));
		}
		else if (PS_SHUFFLE_ACROSS)
		{
			// We wanted to shuffle both the source (read) and the target (write),
			// so instead of doing bit shifts, simply swap everything here
			if (PS_PROCESS_BA == SHUFFLE_READWRITE && PS_PROCESS_RG == SHUFFLE_READWRITE)
			{
				C.br = C.rb;
				C.ag = C.ga;
			}
			else if(PS_PROCESS_BA & SHUFFLE_READ)
			{
				C.rb = C.bb;
				C.ga = C.aa;
			}
			else // PS_PROCESS_RG & SHUFFLE_READ
			{
				C.rb = C.rr;
				C.ga = C.gg;
			}

			//TODO: this might not cover 100% of the cases
			// If we have shuffles in read and write, we are essentially doing two 16<->32 bit reinterprets and swapping channels.
			// In the SDR code, to emulate real HW, this clipped all values beyond 255. For HDR, we want to preserve the original value without clipping it
			// (e.g. GoW/Sly store the scene green color in the alpha channel during shadow calculations, to later restore it)
			if (PS_HDR > 1 && ((SHUFFLE_TEX_PASSTHROUGH && !(PS_PROCESS_BA == SHUFFLE_READWRITE && PS_PROCESS_RG == SHUFFLE_READWRITE)) || SHUFFLE_RT_PASSTHROUGH))
			{
				float4 unclamped_C = SHUFFLE_TEX_PASSTHROUGH ? pre_shuffle_C : pre_shuffle_RT;
				if (PS_PROCESS_BA & (SHUFFLE_TEX_PASSTHROUGH ? SHUFFLE_READ : SHUFFLE_WRITE))
				{
					C.a = unclamped_C.g;
					C.g = unclamped_C.a;
				}
				else // PS_PROCESS_RG & SHUFFLE_READ/SHUFFLE_WRITE
				{
					C.r = unclamped_C.b;
					C.b = unclamped_C.r;
				}
			}
			else if (PS_HDR > 1) // Avoid alpha ever going beyond two in HDR (if its value came from another HDR channel)
				C.a = min(C.a, 255.0f); // Theoretically we should quantize alpha again too here, but we are doing it just below, before the final output
		}
	}

	ps_dither(C.rgb, alpha_blend.a, input.p.xy);

	// Color clamp/wrap needs to be done after sw blending and dithering
	ps_color_clamp_wrap(C.rgb);

	ps_fbmask(C, input.p.xy);

#if PS_AFAIL == 3 // RGB_ONLY
	// Use alpha blend factor to determine whether to update A.
	alpha_blend.a = float(atst_pass);
#endif

#if !PS_NO_COLOR
	C = EncodeTex(C);
	//if (any(isnan(C.rgba))) C = 0; //TODO
	output.c0.a = PS_RTA_CORRECTION ? (C.a / NEUTRAL_ALPHA) : (C.a / 255.0f);
	output.c0.rgb = PS_COLCLIP_HW ? (C.rgb / 65535.0f) : (C.rgb / 255.0f);
#if !PS_NO_COLOR1 // Only used for HW blending
	output.c1 = alpha_blend;
#if PS_HDR >= 2
	output.c1.a = clamp(output.c1.a, 0.f, 255.0f / NEUTRAL_ALPHA);
#endif
#endif
#endif // !PS_NO_COLOR

#endif // PS_DATE != 1/2

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
