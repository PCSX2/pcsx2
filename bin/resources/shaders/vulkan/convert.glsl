// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#ifdef VERTEX_SHADER

layout(location = 0) in vec4 a_pos;
layout(location = 1) in vec2 a_tex;

layout(location = 0) out vec2 v_tex;

void main()
{
	gl_Position = vec4(a_pos.x, -a_pos.y, a_pos.z, a_pos.w);
	v_tex = a_tex;
}

#endif

#ifdef FRAGMENT_SHADER

layout(location = 0) in vec2 v_tex;

#if HAS_INTEGER_OUTPUT
	layout(location = 0) out uint o_col0;
	#define OUTPUT o_col0
#elif HAS_DEPTH_OUTPUT
	out float gl_FragDepth;
	#define OUTPUT gl_FragDepth
#elif HAS_FLOAT32_OUTPUT
	layout(location = 0) out float o_col0;
	#define OUTPUT o_col0
#elif HAS_STENCIL_OUTPUT
#else
	layout(location = 0) out vec4 o_col0;
	#define OUTPUT o_col0
#endif

layout(set = 0, binding = 0) uniform sampler2D samp0;

#if HAS_FLOAT32_INPUT

float sample_c(vec2 uv)
{
	return texture(samp0, uv).r;
}

#else

vec4 sample_c(vec2 uv)
{
	return texture(samp0, uv);
}

#endif

uint rgba8_to_uint(vec4 c)
{
	uvec4 i = uvec4(c * 255.5f) & 0xFFu;
	return i.r | (i.g << 8) | (i.b << 16) | (i.a << 24);
}

uint rgb5a1_to_uint(vec4 c)
{
	uvec4 i = uvec4(c * 255.5f) & uvec4(0xF8u, 0xF8u, 0xF8u, 0x80u);
	return (i.r >> 3) | (i.g << 2) | (i.b << 7) | (i.a << 8);
}

uint depth_to_uint(float d)
{
	return uint(d * exp2(32.0f));
}

vec4 uint_to_rgba8(uint i)
{
	return vec4((i & 0xFFu), ((i >> 8) & 0xFFu), ((i >> 16) & 0xFFu), ((i >> 24) & 0xFFu)) / 255.0f;
}

vec4 uint_to_rgb5a1(uint i)
{
	return vec4(uvec4(i << 3, i >> 2, i >> 7, i >> 8) & uvec4(0xF8u, 0xF8u, 0xF8u, 0x80u)) / 255.0f;
}

float uint_to_depth32(uint i)
{
	return float(i) * exp2(-32.0f);
}

float uint_to_depth24(uint i)
{
	return float(i & 0xFFFFFFu) * exp2(-32.0f);
}

float uint_to_depth16(uint i)
{
	return float(i & 0xFFFFu) * exp2(-32.0f);
}

float rgba8_to_depth32(vec4 val)
{
	return uint_to_depth32(rgba8_to_uint(val));
}

float rgba8_to_depth24(vec4 val)
{
	return uint_to_depth24(rgba8_to_uint(val));
}

float rgba8_to_depth16(vec4 val)
{
	return uint_to_depth16(rgba8_to_uint(val));
}

float rgb5a1_to_depth16(vec4 val)
{
	return uint_to_depth16(rgb5a1_to_uint(val));
}

vec4 depth32_to_rgba8(float d)
{
	return uint_to_rgba8(depth_to_uint(d));
}

vec4 depth16_to_rgb5a1(float d)
{
	return uint_to_rgb5a1(depth_to_uint(d));
}

float depth32_to_depth24(float d)
{
	return uint_to_depth24(depth_to_uint(d));
}

#ifdef ps_copy
void ps_copy()
{
	OUTPUT = sample_c(v_tex);
}
#endif

#ifdef ps_depth_copy
void ps_depth_copy()
{
	OUTPUT = sample_c(v_tex);
}
#endif

#ifdef ps_downsample_copy
layout(push_constant) uniform cb10
{
	ivec2 ClampMin;
	int DownsampleFactor;
	int pad0;
	float Weight;
	float step_multiplier;
	vec2 pad1;
};
void ps_downsample_copy()
{
	ivec2 coord = max(ivec2(gl_FragCoord.xy) * DownsampleFactor, ClampMin);
	vec4 result = vec4(0);
	for (int yoff = 0; yoff < DownsampleFactor; yoff++)
	{
		for (int xoff = 0; xoff < DownsampleFactor; xoff++)
		{
			result += texelFetch(samp0, coord + ivec2(xoff * step_multiplier, yoff * step_multiplier), 0);
		}
	}
	OUTPUT = result / Weight;
}
#endif

#ifdef ps_filter_transparency
void ps_filter_transparency()
{
	vec4 c = sample_c(v_tex);
	OUTPUT = vec4(c.rgb, 1.0);
}
#endif

#ifdef ps_convert_rgb5a1_16bits
void ps_convert_rgb5a1_16bits()
{
	// Need to be careful with precision here, it can break games like Spider-Man 3 and Dogs Life
	OUTPUT = rgb5a1_to_uint(sample_c(v_tex));
}
#endif

#ifdef ps_datm1
void ps_datm1()
{
	if(sample_c(v_tex).a < (127.5f / 255.0f)) // >= 0x80 pass
		discard;
}
#endif

#ifdef ps_datm0
void ps_datm0()
{
	if((127.5f / 255.0f) < sample_c(v_tex).a) // < 0x80 pass (== 0x80 should not pass)
		discard;
}
#endif

#ifdef ps_datm1_rta_correction
void ps_datm1_rta_correction()
{
	if(sample_c(v_tex).a < (254.5f / 255.0f)) // >= 0x80 pass
		discard;
}
#endif

#ifdef ps_datm0_rta_correction
void ps_datm0_rta_correction()
{
	if((254.5f / 255.0f) < sample_c(v_tex).a) // < 0x80 pass (== 0x80 should not pass)
		discard;
}
#endif

#ifdef ps_rta_correction
void ps_rta_correction()
{
	vec4 value = sample_c(v_tex);
	OUTPUT = vec4(value.rgb, value.a / (128.25f / 255.0f));
}
#endif

#ifdef ps_rta_decorrection
void ps_rta_decorrection()
{
	vec4 value = sample_c(v_tex);
	OUTPUT = vec4(value.rgb, value.a * (128.25f / 255.0f));
}
#endif

#ifdef ps_colclip_init
void ps_colclip_init()
{
	vec4 value = sample_c(v_tex);
	OUTPUT = vec4(roundEven(value.rgb * 255.0f) / 65535.0f, value.a);
}
#endif

#ifdef ps_colclip_resolve
void ps_colclip_resolve()
{
	vec4 value = sample_c(v_tex);
	OUTPUT = vec4(vec3(uvec3(value.rgb * 65535.5f) & 255u) / 255.0f, value.a);
}
#endif

#ifdef ps_convert_depth32_32bits
void ps_convert_depth32_32bits()
{
	// Convert a vec32 depth texture into a 32 bits UINT texture
	OUTPUT = depth_to_uint(sample_c(v_tex));
}
#endif

#ifdef ps_convert_depth32_rgba8
void ps_convert_depth32_rgba8()
{
	// Convert a vec32 depth texture into a RGBA color texture
	OUTPUT = depth32_to_rgba8(sample_c(v_tex));
}
#endif

#ifdef ps_convert_depth16_rgb5a1
void ps_convert_depth16_rgb5a1()
{
	// Convert a vec32 (only 16 lsb) depth into a RGB5A1 color texture
	OUTPUT = depth16_to_rgb5a1(sample_c(v_tex));
}
#endif

#ifdef ps_convert_depth32_depth24
void ps_convert_depth32_depth24()
{
	// Truncates depth value to 24bits
	OUTPUT = depth32_to_depth24(sample_c(v_tex));
}
#endif

#define SAMPLE_RGBA_DEPTH_BILN(CONVERT_FN) \
	ivec2 dims = textureSize(samp0, 0); \
	vec2 top_left_f = v_tex * vec2(dims) - 0.5f; \
	ivec2 top_left = ivec2(floor(top_left_f)); \
	ivec4 coords = clamp(ivec4(top_left, top_left + 1), ivec4(0), dims.xyxy - 1); \
	vec2 mix_vals = fract(top_left_f); \
	float depthTL = CONVERT_FN(texelFetch(samp0, coords.xy, 0)); \
	float depthTR = CONVERT_FN(texelFetch(samp0, coords.zy, 0)); \
	float depthBL = CONVERT_FN(texelFetch(samp0, coords.xw, 0)); \
	float depthBR = CONVERT_FN(texelFetch(samp0, coords.zw, 0)); \
	OUTPUT = mix(mix(depthTL, depthTR, mix_vals.x), mix(depthBL, depthBR, mix_vals.x), mix_vals.y);

#ifdef ps_convert_rgba8_depth32
void ps_convert_rgba8_depth32()
{
	// Convert an RGBA texture into a float depth texture
#if HAS_BILN
	SAMPLE_RGBA_DEPTH_BILN(rgba8_to_depth32);
#else
	OUTPUT = rgba8_to_depth32(sample_c(v_tex));
#endif
}
#endif

#ifdef ps_convert_rgba8_depth24
void ps_convert_rgba8_depth24()
{
	// Same as above but without the alpha channel (24 bits Z)
#if HAS_BILN
	SAMPLE_RGBA_DEPTH_BILN(rgba8_to_depth24);
#else
	OUTPUT = rgba8_to_depth24(sample_c(v_tex));
#endif
}
#endif

#ifdef ps_convert_rgba8_depth16
void ps_convert_rgba8_depth16()
{
	// Same as above but without the A/B channels (16 bits Z)
#if HAS_BILN
	SAMPLE_RGBA_DEPTH_BILN(rgba8_to_depth16);
#else
	OUTPUT = rgba8_to_depth16(sample_c(v_tex));
#endif
}
#endif

#ifdef ps_convert_rgb5a1_depth16
void ps_convert_rgb5a1_depth16()
{
	// Convert an RGB5A1 (saved as RGBA8) color to a 16 bit Z
#if HAS_BILN
	SAMPLE_RGBA_DEPTH_BILN(rgb5a1_to_depth16);
#else
	OUTPUT = rgb5a1_to_depth16(sample_c(v_tex));
#endif
}
#endif

#ifdef ps_convert_rgb5a1_8i
layout(push_constant) uniform cb10
{
	uint SBW;
	uint DBW;
	uint PSM;
	float cb_pad1;
	float ScaleFactor;
	vec3 cb_pad2;
};

void ps_convert_rgb5a1_8i()
{
	// Convert a RGB5A1 texture into a 8 bits packed texture
	// Input column: 16x2 RGB5A1 pixels
	// 0: 16 RGBA
	// 1: 16 RGBA
	// Output column: 16x4 Index pixels
	// 0: 16 R5G2
	// 1: 16 R5G2
	// 2: 16 G2B5A1
	// 3: 16 G2B5A1

	uvec2 pos = uvec2(gl_FragCoord.xy);

	// Collapse separate R G B A areas into their base pixel
	uvec2 column = (pos & ~uvec2(0u, 3u)) / uvec2(1u, 2u);
	uvec2 subcolumn = (pos & uvec2(0u, 1u));
	column.x -= (column.x / 128u) * 64u;
	column.y += (column.y / 32u) * 32u;

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

		if (((pos.x & 16u) != 0u) != ((pos.y & 16u) != 0u))
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
	uvec2 coord = column | subcolumn;

	// Compensate for potentially differing page pitch.
	uvec2 block_xy = coord / uvec2(64u, 64u);
	uint block_num = (block_xy.y * (DBW / 128u)) + block_xy.x;
	uvec2 block_offset = uvec2((block_num % (SBW / 64u)) * 64u, (block_num / (SBW / 64u)) * 64u);
	coord = (coord % uvec2(64u, 64u)) + block_offset;

	// Apply offset to cols 1 and 2
	uint is_col23 = pos.y & 4u;
	uint is_col13 = pos.y & 2u;
	uint is_col12 = is_col23 ^ (is_col13 << 1);
	coord.x ^= is_col12; // If cols 1 or 2, flip bit 3 of x

	if (floor(ScaleFactor) != ScaleFactor)
		coord = uvec2(vec2(coord) * ScaleFactor);
	else
		coord *= uvec2(ScaleFactor);

	vec4 pixel = texelFetch(samp0, ivec2(coord), 0);

	uvec4 denorm_c = uvec4(pixel * 255.5f);
	if ((pos.y & 2u) == 0u)
	{
		uint red = (denorm_c.r >> 3) & 0x1Fu;
		uint green = (denorm_c.g >> 3) & 0x1Fu;

		o_col0 = vec4(float(((green << 5) | red) & 0xFFu) / 255.0f);
	}
	else
	{
		uint green = (denorm_c.g >> 3) & 0x1Fu;
		uint blue = (denorm_c.b >> 3) & 0x1Fu;
		uint alpha = denorm_c.a & 0x80u;

		o_col0 = vec4(float((alpha | (blue << 2) | (green >> 3)) & 0xFFu) / 255.0f);
	}
}
#endif

#ifdef ps_convert_rgba_8i
layout(push_constant) uniform cb10
{
	uint SBW;
	uint DBW;
	uint PSM;
	float cb_pad1;
	float ScaleFactor;
	vec3 cb_pad2;
};

void ps_convert_rgba_8i()
{
	// Convert a RGBA texture into a 8 bits packed texture
	// Input column: 8x2 RGBA pixels
	// 0: 8 RGBA
	// 1: 8 RGBA
	// Output column: 16x4 Index pixels
	// 0: 8 R | 8 B
	// 1: 8 R | 8 B
	// 2: 8 G | 8 A
	// 3: 8 G | 8 A
	uvec2 pos = uvec2(gl_FragCoord.xy);

	// Collapse separate R G B A areas into their base pixel
	uvec2 block = (pos & ~uvec2(15u, 3u)) >> 1;
	uvec2 subblock = pos & uvec2(7u, 1u);
	uvec2 coord = block | subblock;

	// Compensate for potentially differing page pitch.
	uvec2 block_xy = coord / uvec2(64u, 32u);
	uint block_num = (block_xy.y * (DBW / 128u)) + block_xy.x;
	uvec2 block_offset = uvec2((block_num % (SBW / 64u)) * 64u, (block_num / (SBW / 64u)) * 32u);
	coord = (coord % uvec2(64u, 32u)) + block_offset;

	// Apply offset to cols 1 and 2
	uint is_col23 = pos.y & 4u;
	uint is_col13 = pos.y & 2u;
	uint is_col12 = is_col23 ^ (is_col13 << 1);
	coord.x ^= is_col12; // If cols 1 or 2, flip bit 3 of x

	if (floor(ScaleFactor) != ScaleFactor)
		coord = uvec2(vec2(coord) * ScaleFactor);
	else
		coord *= uvec2(ScaleFactor);

	vec4 pixel = texelFetch(samp0, ivec2(coord), 0);
	vec2  sel0 = (pos.y & 2u) == 0u ? pixel.rb : pixel.ga;
	float sel1 = (pos.x & 8u) == 0u ? sel0.x : sel0.y;
	o_col0 = vec4(sel1); // Divide by something here?
}
#endif

#ifdef ps_convert_clut_4
layout(push_constant) uniform cb10
{
	uvec2 offset;
	uint doffset;
	uint cb_pad1;
	float scale;
	vec3 cb_pad2;
};

void ps_convert_clut_4()
{
	// CLUT4 is easy, just two rows of 8x8.
	uint index = uint(gl_FragCoord.x) + doffset;
	uvec2 pos = uvec2(index % 8u, index / 8u);

	ivec2 final = ivec2(floor(vec2(offset + pos) * vec2(scale)));
	o_col0 = texelFetch(samp0, final, 0);
}
#endif

#ifdef ps_convert_clut_8
layout(push_constant) uniform cb10
{
	uvec2 offset;
	uint doffset;
	uint cb_pad1;
	float scale;
	vec3 cb_pad2;
};

void ps_convert_clut_8()
{
	uint index = min(uint(gl_FragCoord.x) + doffset, 255u);

	// CLUT is arranged into 8 groups of 16x2, with the top-right and bottom-left quadrants swapped.
	// This can probably be done better..
	uint subgroup = (index / 8u) % 4u;
	uvec2 pos;
	pos.x = (index % 8u) + ((subgroup >= 2u) ? 8u : 0u);
	pos.y = ((index / 32u) * 2u) + (subgroup % 2u);

	ivec2 final = ivec2(floor(vec2(offset + pos) * vec2(scale)));
	o_col0 = texelFetch(samp0, final, 0);
}
#endif

#ifdef ps_yuv
layout(push_constant) uniform cb10
{
	int EMODA;
	int EMODC;
};

void ps_yuv()
{
	vec4 i = sample_c(v_tex);
	vec4 o = vec4(0.0f);

	mat3 rgb2yuv;
	rgb2yuv[0] = vec3(0.587, -0.311, -0.419);
	rgb2yuv[1] = vec3(0.114, 0.500, -0.081);
	rgb2yuv[2] = vec3(0.299, -0.169, 0.500);

	vec3 yuv = rgb2yuv * i.gbr;

	float Y = float(0xDB)/255.0f * yuv.x + float(0x10)/255.0f;
	float Cr = float(0xE0)/255.0f * yuv.y + float(0x80)/255.0f;
	float Cb = float(0xE0)/255.0f * yuv.z + float(0x80)/255.0f;

	switch(EMODA)
	{
		case 0:
			o.a = i.a;
			break;
		case 1:
			o.a = Y;
			break;
		case 2:
			o.a = Y/2.0f;
			break;
		case 3:
			o.a = 0.0f;
			break;
	}

	switch(EMODC)
	{
		case 0:
			o.rgb = i.rgb;
			break;
		case 1:
			o.rgb = vec3(Y);
			break;
		case 2:
			o.rgb = vec3(Y, Cb, Cr);
			break;
		case 3:
			o.rgb = vec3(i.a);
			break;
	}

	o_col0 = o;
}
#endif

#if defined(ps_primid_image_init_0) || defined(ps_primid_image_init_1) || defined(ps_primid_image_init_2) || defined(ps_primid_image_init_3)

void main()
{
	o_col0 = vec4(0x7FFFFFFF);

	#ifdef ps_primid_image_init_0
		if((127.5f / 255.0f) < sample_c(v_tex).a) // < 0x80 pass (== 0x80 should not pass)
			o_col0 = vec4(-1);
	#endif
	#ifdef ps_primid_image_init_1
		if(sample_c(v_tex).a < (127.5f / 255.0f)) // >= 0x80 pass
			o_col0 = vec4(-1);
	#endif
	#ifdef ps_primid_image_init_2
		if((254.5f / 255.0f) < sample_c(v_tex).a) // < 0x80 pass (== 0x80 should not pass)
			o_col0 = vec4(-1);
	#endif
	#ifdef ps_primid_image_init_3
		if(sample_c(v_tex).a < (254.5f / 255.0f)) // >= 0x80 pass
			o_col0 = vec4(-1);
	#endif
}
#endif

#if defined(PS_ROV_COPY_COLOR) || defined(PS_ROV_COPY_DEPTH)
	layout(pixel_interlock_ordered) in;
	#if PS_ROV_COPY_COLOR
		layout(set = 1, binding = 2) uniform texture2D RtSampler;
		layout(set = 1, binding = 5, rgba8) uniform restrict coherent image2D RtImageRov;
	#endif
	#if PS_ROV_COPY_DEPTH
		layout(set = 1, binding = 4) uniform texture2D DepthSampler;
		layout(set = 1, binding = 6, r32f) uniform restrict coherent image2D DepthImageRov;
	#endif
	void ps_rov_copy()
	{
		#if PS_ROV_COPY_COLOR
			vec4 c = texelFetch(RtSampler, ivec2(gl_FragCoord.xy), 0);
		#endif
		#if PS_ROV_COPY_DEPTH
			vec4 d = texelFetch(DepthSampler, ivec2(gl_FragCoord.xy), 0);
		#endif
		
		beginInvocationInterlockARB();
		
		#if PS_ROV_COPY_COLOR
			imageStore(RtImageRov, ivec2(gl_FragCoord.xy), c);
		#endif
		#if PS_ROV_COPY_DEPTH
			imageStore(DepthImageRov, ivec2(gl_FragCoord.xy), d);
		#endif

		endInvocationInterlockARB();
	}
#endif

#endif
