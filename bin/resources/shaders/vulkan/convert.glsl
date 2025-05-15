// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
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

#ifndef PS_HDR
#define PS_HDR 0
#endif

#define FLT_MAX	3.402823466e+38f
#define INT_MAX	0x7FFFFFFF

#define NEUTRAL_ALPHA 128.0f

#if PS_HDR
#define OUTPUT_MAX FLT_MAX
// We could possibly go even lower but it shouldn't really matter
#define HDR_FLT_THRESHOLD 0.0001f
#else
#define OUTPUT_MAX INT_MAX
#endif

layout(location = 0) in vec2 v_tex;

#if defined(ps_convert_rgba8_16bits) || defined(ps_convert_float32_32bits)
layout(location = 0) out uint o_col0;
#elif !defined(ps_datm1) && \
	!defined(ps_datm0) && \
	!defined(ps_datm1_rta_correction) && \
	!defined(ps_datm0_rta_correction) && \
	!defined(ps_convert_rgba8_float32) && \
	!defined(ps_convert_rgba8_float24) && \
	!defined(ps_convert_rgba8_float16) && \
	!defined(ps_convert_rgb5a1_float16) && \
	!defined(ps_convert_rgba8_float32_biln) && \
	!defined(ps_convert_rgba8_float24_biln) && \
	!defined(ps_convert_rgba8_float16_biln) && \
	!defined(ps_convert_rgb5a1_float16_biln) && \
	!defined(ps_depth_copy)
layout(location = 0) out vec4 o_col0;
#endif

layout(set = 0, binding = 0) uniform sampler2D samp0;

vec4 sample_c(vec2 uv)
{
	return texture(samp0, uv);
}

float saturate(float c)
{
	return clamp(c, 0.0, 1.0);
}
vec2 saturate(vec2 c)
{
	return clamp(c, 0.0, 1.0);
}
vec3 saturate(vec3 c)
{
	return clamp(c, 0.0, 1.0);
}
vec4 saturate(vec4 c)
{
	return clamp(c, 0.0, 1.0);
}

#ifdef ps_copy
void ps_copy()
{
	o_col0 = sample_c(v_tex);
}
#endif

#ifdef ps_depth_copy
void ps_depth_copy()
{
  gl_FragDepth = sample_c(v_tex).r;
}
#endif

#ifdef ps_downsample_copy
layout(push_constant) uniform cb10
{
	ivec2 ClampMin;
	int DownsampleFactor;
	int pad0;
	float Weight;
	vec3 pad1;
};
void ps_downsample_copy()
{
	ivec2 coord = max(ivec2(gl_FragCoord.xy) * DownsampleFactor, ClampMin);
	vec4 result = vec4(0);
	for (int yoff = 0; yoff < DownsampleFactor; yoff++)
	{
		for (int xoff = 0; xoff < DownsampleFactor; xoff++)
			result += texelFetch(samp0, coord + ivec2(xoff, yoff), 0);
	}
	o_col0 = result / Weight;
}
#endif

#ifdef ps_filter_transparency
void ps_filter_transparency()
{
	vec4 c = sample_c(v_tex);
	o_col0 = vec4(c.rgb, 1.0);
}
#endif

#ifdef ps_convert_rgba8_16bits
// Need to be careful with precision here, it can break games like Spider-Man 3 and Dogs Life
void ps_convert_rgba8_16bits()
{
	uvec4 i = uvec4(saturate(sample_c(v_tex)) * vec4(255.5f, 255.5f, 255.5f, 255.5f));

	o_col0 = ((i.x & 0x00F8u) >> 3) | ((i.y & 0x00F8u) << 2) | ((i.z & 0x00f8u) << 7) | ((i.w & 0x80u) << 8);
}
#endif

#ifdef ps_datm1
void ps_datm1()
{
#if PS_HDR > 2
	// In "full" HDR we reduce the tolerance threshold compared to the default branch below. These are >= tests, so if in HDR we have more granularity over the values,
	// we don't want to give it any further tolerance, or we'd risk things like shadow getting larger, due to their alpha values being more nuanced and the test accepting a larger area of a shadow stencil gradient.
	if(sample_c(v_tex).a < (NEUTRAL_ALPHA / 255.0f - HDR_FLT_THRESHOLD))
#else
	// For the "PS_HDR <= 2" cases, there's no need to round the sampled alpha (to the closest value on a scale of 0-255)
	// because it would have been quantized on write, and the HDR formats have enough precisions to preserve it
	if(sample_c(v_tex).a < (NEUTRAL_ALPHA - 0.5f) / 255.0f) // >= 0x80 pass
#endif
		discard;

}
#endif

#ifdef ps_datm0
void ps_datm0()
{
#if PS_HDR > 2
	if((NEUTRAL_ALPHA / 255.0f - HDR_FLT_THRESHOLD) < sample_c(v_tex).a)
#else
	if((NEUTRAL_ALPHA - 0.5f) / 255.0f < sample_c(v_tex).a) // < 0x80 pass (== 0x80 should not pass)
#endif
		discard;
}
#endif

#ifdef ps_datm1_rta_correction
void ps_datm1_rta_correction()
{
#if PS_HDR > 2
	if(sample_c(v_tex).a < (1.f - HDR_FLT_THRESHOLD))
#else
	if(sample_c(v_tex).a < (254.5f / 255.0f)) // >= 0x80 pass
#endif
		discard;
}
#endif

#ifdef ps_datm0_rta_correction
void ps_datm0_rta_correction()
{
#if PS_HDR > 2
	if((1.f - HDR_FLT_THRESHOLD) < sample_c(v_tex).a)
#else
	if((254.5f / 255.0f) < sample_c(v_tex).a) // < 0x80 pass (== 0x80 should not pass)
#endif
		discard;
}
#endif

// Maps alpha ~0.5 (the original form, given we store in UNORM8, where 128 is ~0.5) to ~1 (and 1 to ~2 wherever possible)
#ifdef ps_rta_correction
void ps_rta_correction()
{
	// We can be guaranteed that alpha isn't beyond 0-2 even in HDR, as we often pre-clamp it for safety,
	// but if not, alpha will be clamped to 0-2 on blends, so we don't have to worry about clamping it here.
	vec4 value = sample_c(v_tex);
#if PS_HDR
	o_col0 = vec4(value.rgb, value.a * (255.0f / NEUTRAL_ALPHA));
#else
	o_col0 = vec4(value.rgb, value.a * (255.0f / (NEUTRAL_ALPHA + 0.25f))); // Add 0.25 as a rounding "hack" (it's not entirely clear why)
#endif
}
#endif

// Maps alpha ~1 to ~0.5 (and ~2 to 1 wherever possible)
#ifdef ps_rta_decorrection
void ps_rta_decorrection()
{
	vec4 value = sample_c(v_tex);
#if PS_HDR
	o_col0 = vec4(value.rgb, value.a * (NEUTRAL_ALPHA / 255.0f));
#else
	o_col0 = vec4(value.rgb, value.a * ((NEUTRAL_ALPHA + 0.25f) / 255.0f));
#endif
}
#endif

#ifdef ps_colclip_init
void ps_colclip_init()
{
	vec4 value = sample_c(v_tex);
	value.rgb = saturate(value.rgb); // Clamp to [0,1] range given we might have upgraded the "Color" texture to float/HDR, to avoid an initial overflow which could't have happened in uint/SDR
	o_col0 = vec4(ivec3((value.rgb * 255.0) + 0.5) / 65535.0, value.a); // We quantize the source to 8bit even if it was HDR, any finer detail isn't relevant as this is about wrapping
}
#endif

#ifdef ps_colclip_resolve
void ps_colclip_resolve()
{
	vec4 value = sample_c(v_tex);
	o_col0 = vec4(vec3(uvec3((value.rgb * 65535.0) + 0.5) & 255) / 255.0, value.a);
}
#endif

#ifdef ps_convert_float32_32bits
void ps_convert_float32_32bits()
{
	// Convert a vec32 depth texture into a 32 bits UINT texture
	o_col0 = uint(exp2(32.0f) * sample_c(v_tex).r);
}
#endif

#ifdef ps_convert_float32_rgba8
void ps_convert_float32_rgba8()
{
	// Convert a vec32 depth texture into a RGBA color texture
	uint d = uint(sample_c(v_tex).r * exp2(32.0f));
	o_col0 = vec4(uvec4((d & 0xFFu), ((d >> 8) & 0xFFu), ((d >> 16) & 0xFFu), (d >> 24))) / vec4(255.0);
}
#endif

#ifdef ps_convert_float16_rgb5a1
void ps_convert_float16_rgb5a1()
{
	// Convert a vec32 (only 16 lsb) depth into a RGB5A1 color texture
	uint d = uint(sample_c(v_tex).r * exp2(32.0f));
	o_col0 = vec4(uvec4(d << 3, d >> 2, d >> 7, d >> 8) & uvec4(0xf8, 0xf8, 0xf8, 0x80)) / 255.0f;
}
#endif

float rgba8_to_depth32(vec4 unorm)
{
	uvec4 c = uvec4(saturate(unorm) * vec4(255.5f));
	return float(c.r | (c.g << 8) | (c.b << 16) | (c.a << 24)) * exp2(-32.0f);
}

float rgba8_to_depth24(vec4 unorm)
{
	uvec3 c = uvec3(saturate(unorm.rgb) * vec3(255.5f));
	return float(c.r | (c.g << 8) | (c.b << 16)) * exp2(-32.0f);
}

float rgba8_to_depth16(vec4 unorm)
{
	uvec2 c = uvec2(saturate(unorm.rg) * vec2(255.5f));
	return float(c.r | (c.g << 8)) * exp2(-32.0f);
}

float rgb5a1_to_depth16(vec4 unorm)
{
	uvec4 c = uvec4(saturate(unorm) * vec4(255.5f));
	return float(((c.r & 0xF8u) >> 3) | ((c.g & 0xF8u) << 2) | ((c.b & 0xF8u) << 7) | ((c.a & 0x80u) << 8)) * exp2(-32.0f);
}

#ifdef ps_convert_float32_float24
void ps_convert_float32_float24()
{
	// Truncates depth value to 24bits
	uint d = uint(sample_c(v_tex).r * exp2(32.0f)) & 0xFFFFFFu;
	gl_FragDepth = float(d) * exp2(-32.0f);
}
#endif

#ifdef ps_convert_rgba8_float32
void ps_convert_rgba8_float32()
{
	// Convert an RGBA texture into a float depth texture
	gl_FragDepth = rgba8_to_depth32(sample_c(v_tex));
}
#endif

#ifdef ps_convert_rgba8_float24
void ps_convert_rgba8_float24()
{
	// Same as above but without the alpha channel (24 bits Z)

	// Convert an RGBA texture into a float depth texture
	gl_FragDepth = rgba8_to_depth24(sample_c(v_tex));
}
#endif

#ifdef ps_convert_rgba8_float16
void ps_convert_rgba8_float16()
{
	// Same as above but without the A/B channels (16 bits Z)

	// Convert an RGBA texture into a float depth texture
	gl_FragDepth = rgba8_to_depth16(sample_c(v_tex));
}
#endif

#ifdef ps_convert_rgb5a1_float16
void ps_convert_rgb5a1_float16()
{
	// Convert an RGB5A1 (saved as RGBA8) color to a 16 bit Z
	gl_FragDepth = rgb5a1_to_depth16(sample_c(v_tex));
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
	gl_FragDepth = mix(mix(depthTL, depthTR, mix_vals.x), mix(depthBL, depthBR, mix_vals.x), mix_vals.y);

#ifdef ps_convert_rgba8_float32_biln
void ps_convert_rgba8_float32_biln()
{
	// Convert an RGBA texture into a float depth texture
	SAMPLE_RGBA_DEPTH_BILN(rgba8_to_depth32);
}
#endif

#ifdef ps_convert_rgba8_float24_biln
void ps_convert_rgba8_float24_biln()
{
	// Same as above but without the alpha channel (24 bits Z)

	// Convert an RGBA texture into a float depth texture
	SAMPLE_RGBA_DEPTH_BILN(rgba8_to_depth24);
}
#endif

#ifdef ps_convert_rgba8_float16_biln
void ps_convert_rgba8_float16_biln()
{
	// Same as above but without the A/B channels (16 bits Z)

	// Convert an RGBA texture into a float depth texture
	SAMPLE_RGBA_DEPTH_BILN(rgba8_to_depth16);
}
#endif

#ifdef ps_convert_rgb5a1_float16_biln
void ps_convert_rgb5a1_float16_biln()
{
	// Convert an RGB5A1 (saved as RGBA8) color to a 16 bit Z
	SAMPLE_RGBA_DEPTH_BILN(rgb5a1_to_depth16);
}
#endif

#ifdef ps_convert_rgba_8i
layout(push_constant) uniform cb10
{
	uint SBW;
	uint DBW;
	uvec2 cb_pad1;
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

#if defined(ps_stencil_image_init_0) || defined(ps_stencil_image_init_1) || defined(ps_stencil_image_init_2) || defined(ps_stencil_image_init_3)

void main()
{
	o_col0 = vec4(0x7FFFFFFF);

	#ifdef ps_stencil_image_init_0
		#if PS_HDR > 2
			// In "full" HDR we reduce the tolerance threshold compared to the default branch below. These are >= tests, so if in HDR we have more granularity over the values,
			// we don't want to give it any further tolerance, or we'd risk things like shadow getting larger, due to their alpha values being more nuanced and the test accepting a larger area of a shadow stencil gradient.
			if((NEUTRAL_ALPHA / 255.0f - HDR_FLT_THRESHOLD) < sample_c(v_tex).a)
		#else
			// For the "PS_HDR <= 2" cases, there's no need to round the sampled alpha (to the closest value on a scale of 0-255)
			// because it would have been quantized on write, and the HDR formats have enough precisions to preserve it
			if(((NEUTRAL_ALPHA - 0.5f) / 255.0f) < sample_c(v_tex).a) // < 0x80 pass (== 0x80 should not pass)
		#endif
				o_col0 = vec4(-1);
	#endif
	#ifdef ps_stencil_image_init_1
		#if PS_HDR > 2
			if(sample_c(v_tex).a < (NEUTRAL_ALPHA / 255.0f - HDR_FLT_THRESHOLD))
		#else
			if(sample_c(v_tex).a < ((NEUTRAL_ALPHA - 0.5f) / 255.0f)) // >= 0x80 pass
		#endif
				o_col0 = vec4(-1);
	#endif
	// RTA corrected
	#ifdef ps_stencil_image_init_2
		#if PS_HDR > 2
			if((1.0f - HDR_FLT_THRESHOLD) < sample_c(v_tex).a)
		#else
			if((254.5f / 255.0f) < sample_c(v_tex).a) // < 0x80 pass (== 0x80 should not pass)
		#endif
				o_col0 = vec4(-1);
	#endif
	// RTA corrected
	#ifdef ps_stencil_image_init_3
		#if PS_HDR > 2
			if(sample_c(v_tex).a < (1.0f - HDR_FLT_THRESHOLD))
		#else
			if(sample_c(v_tex).a < (254.5f / 255.0f)) // >= 0x80 pass
		#endif
				o_col0 = vec4(-1);
	#endif
}
#endif

#endif
