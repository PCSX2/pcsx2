// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#ifndef PS_HDR
#define PS_HDR 0
#endif

//TODO: delete
#if PS_HDR && 0
#undef PS_HDR
#define PS_HDR 1
#endif

#define FLT_MAX	asfloat(0x7F7FFFFF)  //3.402823466e+38f
#define INT_MAX	0x7FFFFFFF

#define NEUTRAL_ALPHA 128.0f

#if PS_HDR
#define OUTPUT_MAX FLT_MAX
// We could possibly go even lower but it shouldn't really matter
#define HDR_FLT_THRESHOLD 0.0001f
#else
#define OUTPUT_MAX INT_MAX
#endif

struct VS_INPUT
{
	float4 p : POSITION;
	float2 t : TEXCOORD0;
	float4 c : COLOR;
};

struct VS_OUTPUT
{
	float4 p : SV_Position;
	float2 t : TEXCOORD0;
	float4 c : COLOR;
};

cbuffer cb0 : register(b0)
{
	float4 BGColor;
	int EMODA;
	int EMODC;
	int DOFFSET;
};

static const float3x3 rgb2yuv =
{
	{0.587, 0.114, 0.299},
	{-0.311, 0.500, -0.169},
	{-0.419, -0.081, 0.500}
};

Texture2D Texture;
SamplerState TextureSampler;

float4 sample_c(float2 uv)
{
	return Texture.Sample(TextureSampler, uv);
}

struct PS_INPUT
{
	float4 p : SV_Position;
	float2 t : TEXCOORD0;
	float4 c : COLOR;
};

struct PS_OUTPUT
{
	float4 c : SV_Target0;
};

VS_OUTPUT vs_main(VS_INPUT input)
{
	VS_OUTPUT output;

	output.p = input.p;
	output.t = input.t;
	output.c = input.c;

	return output;
}

PS_OUTPUT ps_copy(PS_INPUT input)
{
	PS_OUTPUT output;
	
	output.c = sample_c(input.t);

	return output;
}

float ps_depth_copy(PS_INPUT input) : SV_Depth
{
	return sample_c(input.t).r;
}

PS_OUTPUT ps_downsample_copy(PS_INPUT input)
{
	int DownsampleFactor = DOFFSET;
	int2 ClampMin = int2(EMODA, EMODC);
	float Weight = BGColor.x;

	int2 coord = max(int2(input.p.xy) * DownsampleFactor, ClampMin);
	
	const float gamma = 2.35f; // CRT average

	PS_OUTPUT output;
	output.c = (float4)0;
	for (int yoff = 0; yoff < DownsampleFactor; yoff++)
	{
		for (int xoff = 0; xoff < DownsampleFactor; xoff++)
		{
			float4 c = Texture.Load(int3(coord + int2(xoff, yoff), 0));
#if PS_HDR >= 3 // In HDR, downsample in linear space (assuming we are downscaling rgb colors, which is very likely the case (we do alpha as well, because it's more likely to make sense than not)) (this is an optional feature!)
			c.rgba = pow(c.rgba, gamma); //TODO: alpha too? Add to VK
#endif
			output.c += c;
		}
	}
	output.c /= Weight;
#if PS_HDR >= 3
	output.c.rgba = pow(output.c.rgba, 1.0f / gamma);
#endif
	return output;
}

PS_OUTPUT ps_filter_transparency(PS_INPUT input)
{
	PS_OUTPUT output;
	float4 c = sample_c(input.t);
	output.c = float4(c.rgb, 1.0);
	return output;
}

// Need to be careful with precision here, it can break games like Spider-Man 3 and Dogs Life
uint ps_convert_rgba8_16bits(PS_INPUT input) : SV_Target0
{
	uint4 i = saturate(sample_c(input.t)) * 255.0f + 0.5f;

	return ((i.x & 0x00F8u) >> 3) | ((i.y & 0x00F8u) << 2) | ((i.z & 0x00f8u) << 7) | ((i.w & 0x80u) << 8);
}

PS_OUTPUT ps_datm1(PS_INPUT input)
{
	PS_OUTPUT output;
	
#if PS_HDR > 2
	// In "full" HDR we reduce the tolerance threshold compared to the default branch below. These are >= tests, so if in HDR we have more granularity over the values,
	// we don't want to give it any further tolerance, or we'd risk things like shadow getting larger, due to their alpha values being more nuanced and the test accepting a larger area of a shadow stencil gradient.
	clip(sample_c(input.t).a - (NEUTRAL_ALPHA / 255.0f - HDR_FLT_THRESHOLD));
#else
	// For the "PS_HDR <= 2" cases, there's no need to round the sampled alpha (to the closest value on a scale of 0-255)
	// because it would have been quantized on write, and the HDR formats have enough precisions to preserve it
	clip(sample_c(input.t).a - (NEUTRAL_ALPHA - 0.5f) / 255.0f); // >= 0x80 pass
#endif

	output.c = 0;

	return output;
}

PS_OUTPUT ps_datm0(PS_INPUT input)
{
	PS_OUTPUT output;
	
#if PS_HDR > 2
	clip((NEUTRAL_ALPHA / 255.0f - HDR_FLT_THRESHOLD) - sample_c(input.t).a);
#else
	clip((NEUTRAL_ALPHA - 0.5f) / 255.0f - sample_c(input.t).a); // < 0x80 pass (== 0x80 should not pass)
#endif
	
	output.c = 0;

	return output;
}

PS_OUTPUT ps_datm1_rta_correction(PS_INPUT input)
{
	PS_OUTPUT output;

#if PS_HDR > 2
	clip(sample_c(input.t).a - (1.f - HDR_FLT_THRESHOLD));
#else
	clip(sample_c(input.t).a - 254.5f / 255.0f); // >= 0x80 pass
#endif

	output.c = 0;

	return output;
}

PS_OUTPUT ps_datm0_rta_correction(PS_INPUT input)
{
	PS_OUTPUT output;

#if PS_HDR > 2
	clip((1.f - HDR_FLT_THRESHOLD) - sample_c(input.t).a);
#else
	clip(254.5f / 255.0f - sample_c(input.t).a); // < 0x80 pass (== 0x80 should not pass)
#endif

	output.c = 0;

	return output;
}

// Maps alpha ~0.5 (the original form, given we store in UNORM8, where 128 is ~0.5) to ~1 (and 1 to ~2 wherever possible)
PS_OUTPUT ps_rta_correction(PS_INPUT input)
{
	PS_OUTPUT output;
	// We can be guaranteed that alpha isn't beyond 0-2 even in HDR, as we often pre-clamp it for safety,
	// but if not, alpha will be clamped to 0-2 on blends, so we don't have to worry about clamping it here.
	float4 value = sample_c(input.t);
#if PS_HDR
	output.c = float4(value.rgb, value.a * (255.0f / NEUTRAL_ALPHA));
#else
	output.c = float4(value.rgb, value.a * (255.0f / (NEUTRAL_ALPHA + 0.25f))); // Add 0.25 as a rounding "hack" (it's not entirely clear why)
#endif
	return output;
}

// Maps alpha ~1 to ~0.5 (and ~2 to 1 wherever possible)
PS_OUTPUT ps_rta_decorrection(PS_INPUT input)
{
	PS_OUTPUT output;
	float4 value = sample_c(input.t);
#if PS_HDR
	output.c = float4(value.rgb, value.a * (NEUTRAL_ALPHA / 255.0f));
#else
	output.c = float4(value.rgb, value.a * ((NEUTRAL_ALPHA + 0.25f) / 255.0f));
#endif
	return output;
}

PS_OUTPUT ps_colclip_init(PS_INPUT input)
{
	PS_OUTPUT output;
	float4 value = sample_c(input.t);
	value.rgb = saturate(value.rgb); // Clamp to [0,1] range given we might have upgraded the "Color" texture to float/HDR, to avoid an initial overflow which could't have happened in uint/SDR
	output.c = float4(int3((value.rgb * 255.0) + 0.5) / 65535.0, value.a); // We quantize the source to 8bit even if it was HDR, any finer detail isn't relevant as this is about wrapping
	return output;
}

PS_OUTPUT ps_colclip_resolve(PS_INPUT input)
{
	PS_OUTPUT output;
	float4 value = sample_c(input.t);
	output.c = float4(float3(uint3((value.rgb * 65535.0) + 0.5) & 255) / 255.0, value.a);
	return output;
}

uint ps_convert_float32_32bits(PS_INPUT input) : SV_Target0
{
	// Convert a FLOAT32 depth texture into a 32 bits UINT texture
	return uint(exp2(32.0f) * sample_c(input.t).r);
}

PS_OUTPUT ps_convert_float32_rgba8(PS_INPUT input)
{
	PS_OUTPUT output;

	// Convert a FLOAT32 depth texture into a RGBA color texture
	uint d = uint(sample_c(input.t).r * exp2(32.0f));
	output.c = float4(uint4((d & 0xFFu), ((d >> 8) & 0xFFu), ((d >> 16) & 0xFFu), (d >> 24))) / 255.0f;

	return output;
}

PS_OUTPUT ps_convert_float16_rgb5a1(PS_INPUT input)
{
	PS_OUTPUT output;

	// Convert a FLOAT32 (only 16 lsb) depth into a RGB5A1 color texture
	uint d = uint(sample_c(input.t).r * exp2(32.0f));
	output.c = float4(uint4(d << 3, d >> 2, d >> 7, d >> 8) & uint4(0xf8, 0xf8, 0xf8, 0x80)) / 255.0f;
	return output;
}

float rgba8_to_depth32(float4 val)
{
	uint4 c = uint4(saturate(val) * 255.0f + 0.5f);
	return float(c.r | (c.g << 8) | (c.b << 16) | (c.a << 24)) * exp2(-32.0f);
}

float rgba8_to_depth24(float4 val)
{
	uint3 c = uint3(saturate(val.rgb) * 255.0f + 0.5f);
	return float(c.r | (c.g << 8) | (c.b << 16)) * exp2(-32.0f);
}

float rgba8_to_depth16(float4 val)
{
	uint2 c = uint2(saturate(val.rg) * 255.0f + 0.5f);
	return float(c.r | (c.g << 8)) * exp2(-32.0f);
}

float rgb5a1_to_depth16(float4 val)
{
	uint4 c = uint4(saturate(val) * 255.0f + 0.5f);
	return float(((c.r & 0xF8u) >> 3) | ((c.g & 0xF8u) << 2) | ((c.b & 0xF8u) << 7) | ((c.a & 0x80u) << 8)) * exp2(-32.0f);
}

float ps_convert_float32_float24(PS_INPUT input) : SV_Depth
{
	// Truncates depth value to 24bits
	uint d = uint(sample_c(input.t).r * exp2(32.0f)) & 0xFFFFFFu;
	return float(d) * exp2(-32.0f);
}

float ps_convert_rgba8_float32(PS_INPUT input) : SV_Depth
{
	// Convert an RGBA texture into a float depth texture
	return rgba8_to_depth32(sample_c(input.t));
}

float ps_convert_rgba8_float24(PS_INPUT input) : SV_Depth
{
	// Same as above but without the alpha channel (24 bits Z)

	// Convert an RGBA texture into a float depth texture
	return rgba8_to_depth24(sample_c(input.t));
}

float ps_convert_rgba8_float16(PS_INPUT input) : SV_Depth
{
	// Same as above but without the A/B channels (16 bits Z)

	// Convert an RGBA texture into a float depth texture
	return rgba8_to_depth16(sample_c(input.t));
}

float ps_convert_rgb5a1_float16(PS_INPUT input) : SV_Depth
{
	// Convert an RGB5A1 (saved as RGBA8) color to a 16 bit Z
	return rgb5a1_to_depth16(sample_c(input.t));
}

#define SAMPLE_RGBA_DEPTH_BILN(CONVERT_FN) \
	uint width, height; \
	Texture.GetDimensions(width, height); \
	float2 top_left_f = input.t * float2(width, height) - 0.5f; \
	int2 top_left = int2(floor(top_left_f)); \
	int4 coords = clamp(int4(top_left, top_left + 1), int4(0, 0, 0, 0), int2(width - 1, height - 1).xyxy); \
	float2 mix_vals = frac(top_left_f); \
	float depthTL = CONVERT_FN(Texture.Load(int3(coords.xy, 0))); \
	float depthTR = CONVERT_FN(Texture.Load(int3(coords.zy, 0))); \
	float depthBL = CONVERT_FN(Texture.Load(int3(coords.xw, 0))); \
	float depthBR = CONVERT_FN(Texture.Load(int3(coords.zw, 0))); \
	return lerp(lerp(depthTL, depthTR, mix_vals.x), lerp(depthBL, depthBR, mix_vals.x), mix_vals.y);

float ps_convert_rgba8_float32_biln(PS_INPUT input) : SV_Depth
{
	// Convert an RGBA texture into a float depth texture
	SAMPLE_RGBA_DEPTH_BILN(rgba8_to_depth32);
}

float ps_convert_rgba8_float24_biln(PS_INPUT input) : SV_Depth
{
	// Same as above but without the alpha channel (24 bits Z)

	// Convert an RGBA texture into a float depth texture
	SAMPLE_RGBA_DEPTH_BILN(rgba8_to_depth24);
}

float ps_convert_rgba8_float16_biln(PS_INPUT input) : SV_Depth
{
	// Same as above but without the A/B channels (16 bits Z)

	// Convert an RGBA texture into a float depth texture
	SAMPLE_RGBA_DEPTH_BILN(rgba8_to_depth16);
}

float ps_convert_rgb5a1_float16_biln(PS_INPUT input) : SV_Depth
{
	// Convert an RGB5A1 (saved as RGBA8) color to a 16 bit Z
	SAMPLE_RGBA_DEPTH_BILN(rgb5a1_to_depth16);
}

PS_OUTPUT ps_convert_rgba_8i(PS_INPUT input)
{
	PS_OUTPUT output;

	// Convert a RGBA texture into a 8 bits packed texture
	// Input column: 8x2 RGBA pixels
	// 0: 8 RGBA
	// 1: 8 RGBA
	// Output column: 16x4 Index pixels
	// 0: 8 R | 8 B
	// 1: 8 R | 8 B
	// 2: 8 G | 8 A
	// 3: 8 G | 8 A
	uint2 pos = uint2(input.p.xy);

	// Collapse separate R G B A areas into their base pixel
	uint2 block = (pos & ~uint2(15u, 3u)) >> 1;
	uint2 subblock = pos & uint2(7u, 1u);
	uint2 coord = block | subblock;

	// Compensate for potentially differing page pitch.
	uint SBW = uint(EMODA);
	uint DBW = uint(EMODC);
	uint2 block_xy = coord / uint2(64, 32);
	uint block_num = (block_xy.y * (DBW / 128)) + block_xy.x;
	uint2 block_offset = uint2((block_num % (SBW / 64)) * 64, (block_num / (SBW / 64)) * 32);
	coord = (coord % uint2(64, 32)) + block_offset;

	// Apply offset to cols 1 and 2
	uint is_col23 = pos.y & 4u;
	uint is_col13 = pos.y & 2u;
	uint is_col12 = is_col23 ^ (is_col13 << 1);
	coord.x ^= is_col12; // If cols 1 or 2, flip bit 3 of x

	float ScaleFactor = BGColor.x;
	if (floor(ScaleFactor) != ScaleFactor)
		coord = uint2(float2(coord) * ScaleFactor);
	else
		coord *= uint(ScaleFactor);

	float4 pixel = Texture.Load(int3(int2(coord), 0));
	float2 sel0 = (pos.y & 2u) == 0u ? pixel.rb : pixel.ga;
	float  sel1 = (pos.x & 8u) == 0u ? sel0.x : sel0.y;
	output.c = (float4)(sel1); // Divide by something here?
	return output;
}

PS_OUTPUT ps_convert_clut_4(PS_INPUT input)
{
	// Borrowing the YUV constant buffer.
	float scale = BGColor.x;
	uint2 offset = uint2(uint(EMODA), uint(EMODC)) + uint(DOFFSET);

	// CLUT4 is easy, just two rows of 8x8.
	uint index = uint(input.p.x);
	uint2 pos = uint2(index % 8u, index / 8u);

	int2 final = int2(floor(float2(offset + pos) * scale));
	PS_OUTPUT output;
	output.c = Texture.Load(int3(final, 0), 0);
	return output;
}

PS_OUTPUT ps_convert_clut_8(PS_INPUT input)
{
	float scale = BGColor.x;
	uint2 offset = uint2(uint(EMODA), uint(EMODC));
	uint index = min(uint(input.p.x) + uint(DOFFSET), 255u);

	// CLUT is arranged into 8 groups of 16x2, with the top-right and bottom-left quadrants swapped.
	// This can probably be done better..
	uint subgroup = (index / 8u) % 4u;
	uint2 pos;
	pos.x = (index % 8u) + ((subgroup >= 2u) ? 8u : 0u);
	pos.y = ((index / 32u) * 2u) + (subgroup % 2u);

	int2 final = int2(floor(float2(offset + pos) * scale));
	PS_OUTPUT output;
	output.c = Texture.Load(int3(final, 0), 0);
	return output;
}

PS_OUTPUT ps_yuv(PS_INPUT input)
{
	PS_OUTPUT output;

	float4 i = sample_c(input.t);
	float3 yuv = mul(rgb2yuv, i.gbr);

	float Y = float(0xDB) / 255.0f * yuv.x + float(0x10) / 255.0f;
	float Cr = float(0xE0) / 255.0f * yuv.y + float(0x80) / 255.0f;
	float Cb = float(0xE0) / 255.0f * yuv.z + float(0x80) / 255.0f;

	switch (EMODA)
	{
		case 0:
			output.c.a = i.a;
			break;
		case 1:
			output.c.a = Y;
			break;
		case 2:
			output.c.a = Y / 2.0f;
			break;
		case 3:
		default:
			output.c.a = 0.0f;
			break;
	}

	switch (EMODC)
	{
		case 0:
			output.c.rgb = i.rgb;
			break;
		case 1:
			output.c.rgb = float3(Y, Y, Y);
			break;
		case 2:
			output.c.rgb = float3(Y, Cb, Cr);
			break;
		case 3:
		default:
			output.c.rgb = float3(i.a, i.a, i.a);
			break;
	}

	return output;
}

float ps_stencil_image_init_0(PS_INPUT input) : SV_Target
{
	float c;
#if PS_HDR > 2
	// In "full" HDR we reduce the tolerance threshold compared to the default branch below. These are >= tests, so if in HDR we have more granularity over the values,
	// we don't want to give it any further tolerance, or we'd risk things like shadow getting larger, due to their alpha values being more nuanced and the test accepting a larger area of a shadow stencil gradient.
	if ((NEUTRAL_ALPHA / 255.0f - HDR_FLT_THRESHOLD) < sample_c(input.t).a)
#else
	// For the "PS_HDR <= 2" cases, there's no need to round the sampled alpha (to the closest value on a scale of 0-255)
	// because it would have been quantized on write, and the HDR formats have enough precisions to preserve it
	if (((NEUTRAL_ALPHA - 0.5f) / 255.0f) < sample_c(input.t).a) // < 0x80 pass (== 0x80 should not pass)
#endif
		c = float(-1);
	else
		c = float(OUTPUT_MAX);
	return c;
}

float ps_stencil_image_init_1(PS_INPUT input) : SV_Target
{
	float c;
#if PS_HDR > 2
	if (sample_c(input.t).a < (NEUTRAL_ALPHA / 255.0f - HDR_FLT_THRESHOLD))
#else
	if (sample_c(input.t).a < ((NEUTRAL_ALPHA - 0.5f) / 255.0f)) // >= 0x80 pass
#endif
		c = float(-1);
	else
		c = float(OUTPUT_MAX);
	return c;
}

// RTA corrected
float ps_stencil_image_init_2(PS_INPUT input)
	: SV_Target
{
	float c;
#if PS_HDR > 2
	if ((1.0f - HDR_FLT_THRESHOLD) < sample_c(input.t).a)
#else
	if ((254.5f / 255.0f) < sample_c(input.t).a) // < 0x80 pass (== 0x80 should not pass)
#endif
		c = float(-1);
	else
		c = float(OUTPUT_MAX);
	return c;
}

// RTA corrected
float ps_stencil_image_init_3(PS_INPUT input)
	: SV_Target
{
	float c;
#if PS_HDR > 2
	if (sample_c(input.t).a < (1.0f - HDR_FLT_THRESHOLD))
#else
	if (sample_c(input.t).a < (254.5f / 255.0f)) // >= 0x80 pass
#endif
		c = float(-1);
	else
		c = float(OUTPUT_MAX);
	return c;
}
