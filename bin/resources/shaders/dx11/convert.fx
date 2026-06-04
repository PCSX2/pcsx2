// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#if defined(VERTEX_SHADER)

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

VS_OUTPUT vs_main(VS_INPUT input)
{
	VS_OUTPUT output;

	output.p = input.p;
	output.t = input.t;
	output.c = input.c;

	return output;
}

#endif // VERTEX_SHADER

#if defined(PIXEL_SHADER)

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

SamplerState TextureSampler;

#if HAS_FLOAT32_INPUT
Texture2D Texture;
float sample_c(float2 uv)
{
	return Texture.Sample(TextureSampler, uv).r;
}
#elif HAS_INTEGER_INPUT
Texture2D<uint> Texture;
uint sample_c(float2 uv)
{
	uint w, h;
	Texture.GetDimensions(w, h);
	return Texture.Load(int3(w * uv.x, h * uv.y, 0));
}
#else
Texture2D Texture;
float4 sample_c(float2 uv)
{
	return Texture.Sample(TextureSampler, uv);
}
#endif

struct PS_INPUT
{
	float4 p : SV_Position;
	float2 t : TEXCOORD0;
	float4 c : COLOR;
};

#if HAS_INTEGER_INPUT
	#define DEPTH_INPUT_TYPE uint
	#define DEPTH_INPUT_SCALE 1
#else
	#define DEPTH_INPUT_TYPE float
	#define DEPTH_INPUT_SCALE exp2(32.0f)
#endif

#if HAS_INTEGER_OUTPUT
	#define DEPTH_OUTPUT_TYPE uint
	#define DEPTH_OUTPUT_SCALE 1
#else
	#define DEPTH_OUTPUT_TYPE float
	#define DEPTH_OUTPUT_SCALE exp2(-32.0f)
#endif

#if HAS_INTEGER_OUTPUT
	#define OUTPUT_TYPE uint
	#define OUTPUT_SV SV_Target
#elif HAS_DEPTH_OUTPUT
	#define OUTPUT_TYPE float
	#define OUTPUT_SV SV_Depth
#elif HAS_FLOAT32_OUTPUT
	#define OUTPUT_TYPE float
	#define OUTPUT_SV SV_Target
#else
	#define OUTPUT_TYPE float4
	#define OUTPUT_SV SV_Target
#endif

struct PS_OUTPUT
{
	OUTPUT_TYPE o : OUTPUT_SV;
};

uint rgba8_to_uint(float4 c)
{
	uint4 i = uint4(c * 255.5f) & 0xFFu;
	return i.r | (i.g << 8) | (i.b << 16) | (i.a << 24);
}

uint rgb5a1_to_uint(float4 c)
{
	uint4 i = uint4(c * 255.5f) & uint4(0xF8u, 0xF8u, 0xF8u, 0x80u);
	return (i.r >> 3) | (i.g << 2) | (i.b << 7) | (i.a << 8);
}

uint depth_to_uint(DEPTH_INPUT_TYPE d)
{
	return uint(d * DEPTH_INPUT_SCALE);
}

float4 uint_to_rgba8(uint i)
{
	return float4((i & 0xFFu), ((i >> 8) & 0xFFu), ((i >> 16) & 0xFFu), ((i >> 24) & 0xFFu)) / 255.0f;
}

float4 uint_to_rgb5a1(uint i)
{
	return float4(uint4(i << 3, i >> 2, i >> 7, i >> 8) & uint4(0xF8u, 0xF8u, 0xF8u, 0x80u)) / 255.0f;
}

DEPTH_OUTPUT_TYPE uint_to_depth32(uint i)
{
	return DEPTH_OUTPUT_TYPE(i) * DEPTH_OUTPUT_SCALE;
}

DEPTH_OUTPUT_TYPE uint_to_depth24(uint i)
{
	return DEPTH_OUTPUT_TYPE(i & 0xFFFFFFu) * DEPTH_OUTPUT_SCALE;
}

DEPTH_OUTPUT_TYPE uint_to_depth16(uint i)
{
	return DEPTH_OUTPUT_TYPE(i & 0xFFFFu) * DEPTH_OUTPUT_SCALE;
}

DEPTH_OUTPUT_TYPE rgba8_to_depth32(float4 val)
{
	return uint_to_depth32(rgba8_to_uint(val));
}

DEPTH_OUTPUT_TYPE rgba8_to_depth24(float4 val)
{
	return uint_to_depth24(rgba8_to_uint(val));
}

DEPTH_OUTPUT_TYPE rgba8_to_depth16(float4 val)
{
	return uint_to_depth16(rgba8_to_uint(val));
}

DEPTH_OUTPUT_TYPE rgb5a1_to_depth16(float4 val)
{
	return uint_to_depth16(rgb5a1_to_uint(val));
}

float4 depth32_to_rgba8(DEPTH_INPUT_TYPE d)
{
	return uint_to_rgba8(depth_to_uint(d));
}

float4 depth16_to_rgb5a1(DEPTH_INPUT_TYPE d)
{
	return uint_to_rgb5a1(depth_to_uint(d));
}

DEPTH_OUTPUT_TYPE depth32_to_depth24(DEPTH_INPUT_TYPE d)
{
	return uint_to_depth24(depth_to_uint(d));
}

#if defined(__ps_copy__)
OUTPUT_TYPE ps_copy(PS_INPUT input) : OUTPUT_SV
{
	return sample_c(input.t);
}
#endif

#if defined(__ps_depth_copy__)
OUTPUT_TYPE ps_depth_copy(PS_INPUT input) : OUTPUT_SV
{
	return uint_to_depth32(depth_to_uint(sample_c(input.t)));
}
#endif

#if defined(__ps_downsample_copy__)
PS_OUTPUT ps_downsample_copy(PS_INPUT input)
{
	int DownsampleFactor = DOFFSET;
	int2 ClampMin = int2(EMODA, EMODC);
	float Weight = BGColor.x;
	float step_multiplier = BGColor.y;

	int2 coord = max(int2(input.p.xy) * DownsampleFactor, ClampMin);

	PS_OUTPUT output;
	output.o = (float4)0;
	for (int yoff = 0; yoff < DownsampleFactor; yoff++)
	{
		for (int xoff = 0; xoff < DownsampleFactor; xoff++)
			output.o += Texture.Load(int3(coord + int2(xoff * step_multiplier, yoff * step_multiplier), 0));
	}
	output.o /= Weight;
	return output;
}
#endif

#if defined(__ps_filter_transparency__)
PS_OUTPUT ps_filter_transparency(PS_INPUT input)
{
	PS_OUTPUT output;
	float4 c = sample_c(input.t);
	output.o = float4(c.rgb, 1.0);
	return output;
}
#endif

#if defined(__ps_convert_rgb5a1_16bits__)
OUTPUT_TYPE ps_convert_rgb5a1_16bits(PS_INPUT input) : OUTPUT_SV
{
	// Need to be careful with precision here, it can break games like Spider-Man 3 and Dogs Life
	return rgb5a1_to_uint(sample_c(input.t));
}
#endif

#if defined(__ps_datm1__)
void ps_datm1(PS_INPUT input)
{
	clip(sample_c(input.t).a - 127.5f / 255); // >= 0x80 pass
}
#endif

#if defined(__ps_datm0__)
void ps_datm0(PS_INPUT input)
{
	clip(127.5f / 255 - sample_c(input.t).a); // < 0x80 pass (== 0x80 should not pass)
}
#endif

#if defined(__ps_datm1_rta_correction__)
void ps_datm1_rta_correction(PS_INPUT input)
{
	clip(sample_c(input.t).a - 254.5f / 255); // >= 0x80 pass
}
#endif

#if defined(__ps_datm0_rta_correction__)
void ps_datm0_rta_correction(PS_INPUT input)
{
	clip(254.5f / 255 - sample_c(input.t).a); // < 0x80 pass (== 0x80 should not pass)
}
#endif

#if defined(__ps_rta_correction__)
PS_OUTPUT ps_rta_correction(PS_INPUT input)
{
	PS_OUTPUT output;
	float4 value = sample_c(input.t);
	output.o = float4(value.rgb, value.a / (128.25f / 255.0f));
	return output;
}
#endif

#if defined(__ps_rta_decorrection__)
PS_OUTPUT ps_rta_decorrection(PS_INPUT input)
{
	PS_OUTPUT output;
	float4 value = sample_c(input.t);
	output.o = float4(value.rgb, value.a * (128.25f / 255.0f));
	return output;
}
#endif

#if defined(__ps_colclip_init__)
PS_OUTPUT ps_colclip_init(PS_INPUT input)
{
	PS_OUTPUT output;
	float4 value = sample_c(input.t);
	output.o = float4(round(value.rgb * 255) / 65535, value.a);
	return output;
}
#endif

#if defined(__ps_colclip_resolve__)
PS_OUTPUT ps_colclip_resolve(PS_INPUT input)
{
	PS_OUTPUT output;
	float4 value = sample_c(input.t);
	output.o = float4(float3(uint3(value.rgb * 65535.5) & 255) / 255, value.a);
	return output;
}
#endif

#if defined(__ps_convert_depth32_32bits__)
OUTPUT_TYPE ps_convert_depth32_32bits(PS_INPUT input) : OUTPUT_SV
{
	// Convert a depth texture into a 32 bits UINT texture
	return depth_to_uint(sample_c(input.t));
}
#endif

#if defined(__ps_convert_depth32_rgba8__)
OUTPUT_TYPE ps_convert_depth32_rgba8(PS_INPUT input) : OUTPUT_SV
{
	// Convert a depth texture into a RGBA color texture
	return depth32_to_rgba8(sample_c(input.t));
}
#endif

#if defined(__ps_convert_depth16_rgb5a1__)
OUTPUT_TYPE ps_convert_depth16_rgb5a1(PS_INPUT input) : OUTPUT_SV
{
	// Convert depth (only 16 lsb) into a RGB5A1 color texture
	return depth16_to_rgb5a1(sample_c(input.t));
}
#endif

#if defined(__ps_convert_depth32_depth24__)
OUTPUT_TYPE ps_convert_depth32_depth24(PS_INPUT input) : OUTPUT_SV
{
	// Truncates depth value to 24bits
	return depth32_to_depth24(sample_c(input.t));
}
#endif

#if HAS_INTEGER_OUTPUT
uint lerp_depth(uint a, uint b, float c)
{
  uint absdiff = a > b ? a - b : b - a;
  uint adjust = min(uint(round(float(absdiff) * c)), absdiff);
  return a > b ? a - adjust : a + adjust;
}
#else
float lerp_depth(float a, float b, float c)
{
  return lerp(a, b, c);
}
#endif

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
	return lerp_depth(lerp_depth(depthTL, depthTR, mix_vals.x), lerp_depth(depthBL, depthBR, mix_vals.x), mix_vals.y);

#if defined(__ps_convert_rgba8_depth32__)
OUTPUT_TYPE ps_convert_rgba8_depth32(PS_INPUT input) : OUTPUT_SV
{
	// Convert an RGBA texture into a float depth texture
#if HAS_BILN
	SAMPLE_RGBA_DEPTH_BILN(rgba8_to_depth32);
#else
	return rgba8_to_depth32(sample_c(input.t));
#endif
}
#endif

#if defined(__ps_convert_rgba8_depth24__)
OUTPUT_TYPE ps_convert_rgba8_depth24(PS_INPUT input) : OUTPUT_SV
{
	// Same as above but without the alpha channel (24 bits Z)
	// Convert an RGBA texture into a float depth texture
#if HAS_BILN
	SAMPLE_RGBA_DEPTH_BILN(rgba8_to_depth24);
#else
	return rgba8_to_depth24(sample_c(input.t));
#endif
}
#endif

#if defined(__ps_convert_rgba8_depth16__)
OUTPUT_TYPE ps_convert_rgba8_depth16(PS_INPUT input) : OUTPUT_SV
{
	// Same as above but without the A/B channels (16 bits Z)
	// Convert an RGBA texture into a float depth texture
#if HAS_BILN
	SAMPLE_RGBA_DEPTH_BILN(rgba8_to_depth16);
#else
	return rgba8_to_depth16(sample_c(input.t));
#endif
}
#endif

#if defined(__ps_convert_rgb5a1_depth16__)
OUTPUT_TYPE ps_convert_rgb5a1_depth16(PS_INPUT input) : OUTPUT_SV
{
	// Convert an RGB5A1 (saved as RGBA8) color to a 16 bit Z
#if HAS_BILN
	SAMPLE_RGBA_DEPTH_BILN(rgb5a1_to_depth16);
#else
	return rgb5a1_to_depth16(sample_c(input.t));
#endif
}
#endif

#if defined(__ps_convert_rgb5a1_8i__)
PS_OUTPUT ps_convert_rgb5a1_8i(PS_INPUT input)
{
	PS_OUTPUT output;

	// Convert a RGB5A1 texture into a 8 bits packed texture
	// Input column: 16x2 RGB5A1 pixels
	// 0: 16 RGBA
	// 1: 16 RGBA
	// Output column: 16x4 Index pixels
	// 0: 16 R5G2
	// 1: 16 R5G2
	// 2: 16 G2B5A1
	// 3: 16 G2B5A1
	uint2 pos = uint2(input.p.xy);

	// Collapse separate R G B A areas into their base pixel
	uint2 column = (pos & ~uint2(0u, 3u)) / uint2(1u, 2u);
	uint2 subcolumn = (pos & uint2(0u, 1u));
	column.x -= (column.x / 128u) * 64u;
	column.y += (column.y / 32u) * 32u;
	
	uint PSM = uint(DOFFSET);
	
	// Deal with swizzling differences
	if ((PSM & 0x8u) != 0u) // PSMCT16S
	{
		if ((pos.x & 32u) != 0u)
		{
			column.y += 32u; // 4 columns high times 4 to get bottom 4 blocks
			column.x &= ~32u;
		}
		
		if ((pos.x & 64u) != 0u)
		{
			column.x -= 32u;
		}
		
		if (((pos.x & 16u) != 0u) != ((pos.y & 16u) != 0u))
		{
			column.x ^= 16u; 
			column.y ^= 8u;
		}
		
		if ((PSM & 0x30u) != 0u) // PSMZ16S - Untested but hopefully ok if anything uses it.
		{
			column.x ^= 32u;
			column.y ^= 16u;
		}
	}
	else // PSMCT16
	{
		if ((pos.y & 32u) != 0u)
		{
			column.y -= 16u;
			column.x += 32u;
		}
		
		if ((pos.x & 96u) != 0u)
		{
			uint multi = (pos.x & 96u) / 32u;
			column.y += 16u * multi; // 4 columns high times 4 to get bottom 4 blocks
			column.x -= (pos.x & 96u);
		}
		
		if (((pos.x & 16u) != 0u) != ((pos.y & 16) != 0))
		{
			column.x ^= 16u; 
			column.y ^= 8u;
		}
		
		if ((PSM & 0x30u) != 0u) // PSMZ16 - Untested but hopefully ok if anything uses it.
		{
			column.x ^= 32u;
			column.y ^= 32u;
		}
	}
	
	uint2 coord = column | subcolumn;

	// Compensate for potentially differing page pitch.
	uint SBW = uint(EMODA);
	uint DBW = uint(EMODC);
	uint2 block_xy = coord / uint2(64u, 64u);
	uint block_num = (block_xy.y * (DBW / 128u)) + block_xy.x;
	uint2 block_offset = uint2((block_num % (SBW / 64u)) * 64u, (block_num / (SBW / 64u)) * 64u);
	coord = (coord % uint2(64u, 64u)) + block_offset;

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
	uint4 denorm_c = (uint4)(pixel * 255.5f);
	if ((pos.y & 2u) == 0u)
	{
		uint red = (denorm_c.r >> 3) & 0x1Fu;
		uint green = (denorm_c.g >> 3) & 0x1Fu;
		
		output.o = (float4)(((float)(((green << 5) | red) & 0xFFu)) / 255.0f);
	}
	else
	{
		uint green = (denorm_c.g >> 3) & 0x1Fu;
		uint blue = (denorm_c.b >> 3) & 0x1Fu;
		uint alpha = denorm_c.a & 0x80u;

		output.o = (float4)(((float)((alpha | (blue << 2) | (green >> 3)) & 0xFFu)) / 255.0f);
	}
	return output;
}
#endif

#if defined(__ps_convert_rgba_8i__)
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
	output.o = (float4)(sel1); // Divide by something here?
	return output;
}
#endif

#if defined(__ps_convert_clut_4__)
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
	output.o = Texture.Load(int3(final, 0), 0);
	return output;
}
#endif

#if defined(__ps_convert_clut_8__)
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
	output.o = Texture.Load(int3(final, 0), 0);
	return output;
}
#endif

#if defined(__ps_yuv__)
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
			output.o.a = i.a;
			break;
		case 1:
			output.o.a = Y;
			break;
		case 2:
			output.o.a = Y / 2.0f;
			break;
		case 3:
		default:
			output.o.a = 0.0f;
			break;
	}

	switch (EMODC)
	{
		case 0:
			output.o.rgb = i.rgb;
			break;
		case 1:
			output.o.rgb = float3(Y, Y, Y);
			break;
		case 2:
			output.o.rgb = float3(Y, Cb, Cr);
			break;
		case 3:
		default:
			output.o.rgb = float3(i.a, i.a, i.a);
			break;
	}

	return output;
}
#endif

#if defined(__ps_primid_image_init_0__)
float ps_primid_image_init_0(PS_INPUT input) : SV_Target
{
	float c;
	if ((127.5f / 255.0f) < sample_c(input.t).a) // < 0x80 pass (== 0x80 should not pass)
		c = float(-1);
	else
		c = float(0x7FFFFFFF);
	return c;
}
#endif

#if defined(__ps_primid_image_init_1__)
float ps_primid_image_init_1(PS_INPUT input) : SV_Target
{
	float c;
	if (sample_c(input.t).a < (127.5f / 255.0f)) // >= 0x80 pass
		c = float(-1);
	else
		c = float(0x7FFFFFFF);
	return c;
}
#endif

#if defined(__ps_primid_image_init_2__)
float ps_primid_image_init_2(PS_INPUT input) : SV_Target
{
	float c;
	if ((254.5f / 255.0f) < sample_c(input.t).a) // < 0x80 pass (== 0x80 should not pass)
		c = float(-1);
	else
		c = float(0x7FFFFFFF);
	return c;
}
#endif

#if defined(__ps_primid_image_init_3__)
float ps_primid_image_init_3(PS_INPUT input) : SV_Target
{
	float c;
	if (sample_c(input.t).a < (254.5f / 255.0f)) // >= 0x80 pass
		c = float(-1);
	else
		c = float(0x7FFFFFFF);
	return c;
}
#endif

#endif // PIXEL_SHADER
