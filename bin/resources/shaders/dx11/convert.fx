// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
	uint4 i = sample_c(input.t) * float4(255.5f, 255.5f, 255.5f, 255.5f);

	return ((i.x & 0x00F8u) >> 3) | ((i.y & 0x00F8u) << 2) | ((i.z & 0x00f8u) << 7) | ((i.w & 0x80u) << 8);
}

PS_OUTPUT ps_datm1(PS_INPUT input)
{
	PS_OUTPUT output;
	
	clip(sample_c(input.t).a - 127.5f / 255); // >= 0x80 pass
	
	output.c = 0;

	return output;
}

PS_OUTPUT ps_datm0(PS_INPUT input)
{
	PS_OUTPUT output;
	
	clip(127.5f / 255 - sample_c(input.t).a); // < 0x80 pass (== 0x80 should not pass)
	
	output.c = 0;

	return output;
}

PS_OUTPUT ps_hdr_init(PS_INPUT input)
{
	PS_OUTPUT output;
	float4 value = sample_c(input.t);
	output.c = float4(round(value.rgb * 255) / 65535, value.a);
	return output;
}

PS_OUTPUT ps_hdr_resolve(PS_INPUT input)
{
	PS_OUTPUT output;
	float4 value = sample_c(input.t);
	output.c = float4(float3(uint3(value.rgb * 65535.5) & 255) / 255, value.a);
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
	uint4 c = uint4(val * 255.5f);
	return float(c.r | (c.g << 8) | (c.b << 16) | (c.a << 24)) * exp2(-32.0f);
}

float rgba8_to_depth24(float4 val)
{
	uint3 c = uint3(val.rgb * 255.5f);
	return float(c.r | (c.g << 8) | (c.b << 16)) * exp2(-32.0f);
}

float rgba8_to_depth16(float4 val)
{
	uint2 c = uint2(val.rg * 255.5f);
	return float(c.r | (c.g << 8)) * exp2(-32.0f);
}

float rgb5a1_to_depth16(float4 val)
{
	uint4 c = uint4(val * 255.5f);
	return float(((c.r & 0xF8u) >> 3) | ((c.g & 0xF8u) << 2) | ((c.b & 0xF8u) << 7) | ((c.a & 0x80u) << 8)) * exp2(-32.0f);
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
	if ((127.5f / 255.0f) < sample_c(input.t).a) // < 0x80 pass (== 0x80 should not pass)
		c = float(-1);
	else
		c = float(0x7FFFFFFF);
	return c;
}

float ps_stencil_image_init_1(PS_INPUT input) : SV_Target
{
	float c;
	if (sample_c(input.t).a < (127.5f / 255.0f)) // >= 0x80 pass
		c = float(-1);
	else
		c = float(0x7FFFFFFF);
	return c;
}
