// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

//////////////////////////////////////////////////////////////////////
// Vertex Shader
//////////////////////////////////////////////////////////////////////

#if defined(VERTEX_SHADER)

layout(std140, set = 0, binding = 0) uniform cb0
{
	vec2 VertexScale;
	vec2 VertexOffset;
	vec2 TextureScale;
	vec2 TextureOffset;
	vec2 PointSize;
	uint MaxDepth;
	uint pad_cb0;
};

layout(location = 0) out VSOutput
{
	vec4 t;
	vec4 ti;

	#if VS_IIP != 0
		vec4 c;
	#else
		flat vec4 c;
	#endif
} vsOut;

#if VS_EXPAND == 0

layout(location = 0) in vec2 a_st;
layout(location = 1) in uvec4 a_c;
layout(location = 2) in float a_q;
layout(location = 3) in uvec2 a_p;
layout(location = 4) in uint a_z;
layout(location = 5) in uvec2 a_uv;
layout(location = 6) in vec4 a_f;

void main()
{
	// Clamp to max depth, gs doesn't wrap
	uint z = min(a_z, MaxDepth);

	// pos -= 0.05 (1/320 pixel) helps avoiding rounding problems (integral part of pos is usually 5 digits, 0.05 is about as low as we can go)
	// example: ceil(afterseveralvertextransformations(y = 133)) => 134 => line 133 stays empty
	// input granularity is 1/16 pixel, anything smaller than that won't step drawing up/left by one pixel
	// example: 133.0625 (133 + 1/16) should start from line 134, ceil(133.0625 - 0.05) still above 133

	gl_Position = vec4(a_p, float(z), 1.0f) - vec4(0.05f, 0.05f, 0, 0);
	gl_Position.xy = gl_Position.xy * vec2(VertexScale.x, -VertexScale.y) - vec2(VertexOffset.x, -VertexOffset.y);
	gl_Position.z *= exp2(-32.0f);		// integer->float depth
	gl_Position.y = -gl_Position.y;

	#if VS_TME
		vec2 uv = a_uv - TextureOffset;
		vec2 st = a_st - TextureOffset;

		// Integer nomalized
		vsOut.ti.xy = uv * TextureScale;

		#if VS_FST
			// Integer integral
			vsOut.ti.zw = uv;
		#else
			// float for post-processing in some games
			vsOut.ti.zw = st / TextureScale;
		#endif

		// Float coords
		vsOut.t.xy = st;
		vsOut.t.w = a_q;
	#else
		vsOut.t = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		vsOut.ti = vec4(0.0f);
	#endif

	#if VS_POINT_SIZE
		gl_PointSize = PointSize.x;
	#endif

	vsOut.c = vec4(a_c);
	vsOut.t.z = a_f.r;
}

#else // VS_EXPAND

struct RawVertex
{
	vec2 ST;
	uint RGBA;
	float Q;
	uint XY;
	uint Z;
	uint UV;
	uint FOG;
};

layout(std140, set = 0, binding = 2) readonly buffer VertexBuffer {
	RawVertex vertex_buffer[];
};

struct ProcessedVertex
{
	vec4 p;
	vec4 t;
	vec4 ti;
	vec4 c;
};

ProcessedVertex load_vertex(uint index)
{
	RawVertex rvtx = vertex_buffer[gl_BaseVertexARB + index];

	vec2 a_st = rvtx.ST;
	uvec4 a_c = uvec4(bitfieldExtract(rvtx.RGBA, 0, 8), bitfieldExtract(rvtx.RGBA, 8, 8),
	                  bitfieldExtract(rvtx.RGBA, 16, 8), bitfieldExtract(rvtx.RGBA, 24, 8));
	float a_q = rvtx.Q;
	uvec2 a_p = uvec2(bitfieldExtract(rvtx.XY, 0, 16), bitfieldExtract(rvtx.XY, 16, 16));
	uint a_z = rvtx.Z;
	uvec2 a_uv = uvec2(bitfieldExtract(rvtx.UV, 0, 16), bitfieldExtract(rvtx.UV, 16, 16));
	vec4 a_f = unpackUnorm4x8(rvtx.FOG);

	ProcessedVertex vtx;

	uint z = min(a_z, MaxDepth);
	vtx.p = vec4(a_p, float(z), 1.0f) - vec4(0.05f, 0.05f, 0, 0);
	vtx.p.xy = vtx.p.xy * vec2(VertexScale.x, -VertexScale.y) - vec2(VertexOffset.x, -VertexOffset.y);
	vtx.p.z *= exp2(-32.0f);		// integer->float depth
	vtx.p.y = -vtx.p.y;

	#if VS_TME
		vec2 uv = a_uv - TextureOffset;
		vec2 st = a_st - TextureOffset;
		vtx.ti.xy = uv * TextureScale;

		#if VS_FST
			vtx.ti.zw = uv;
		#else
			vtx.ti.zw = st / TextureScale;
		#endif

		vtx.t.xy = st;
		vtx.t.w = a_q;
	#else
		vtx.t = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		vtx.ti = vec4(0.0f);
	#endif

	vtx.c = a_c;
	vtx.t.z = a_f.r;

	return vtx;
}

void main()
{
	ProcessedVertex vtx;
	uint vid = uint(gl_VertexIndex - gl_BaseVertexARB);

#if VS_EXPAND == 1 // Point

	vtx = load_vertex(vid >> 2);

	vtx.p.x += ((vid & 1u) != 0u) ? PointSize.x : 0.0f; 
	vtx.p.y += ((vid & 2u) != 0u) ? PointSize.y : 0.0f;

#elif VS_EXPAND == 2 // Line

	uint vid_base = vid >> 2;

	bool is_bottom = (vid & 2u) != 0u;
	bool is_right = (vid & 1u) != 0u;
#ifdef VS_PROVOKING_VERTEX_LAST
	uint vid_other = is_bottom ? vid_base - 1 : vid_base + 1;
#else
	uint vid_other = is_bottom ? vid_base + 1 : vid_base - 1;
#endif
	
	vtx = load_vertex(vid_base);
	ProcessedVertex other = load_vertex(vid_other);

	vec2 line_vector = normalize(vtx.p.xy - other.p.xy);
	vec2 line_normal = vec2(line_vector.y, -line_vector.x);
	vec2 line_width = (line_normal * PointSize) / 2;
	// line_normal is inverted for bottom point
	vec2 offset = ((uint(is_bottom) ^ uint(is_right)) != 0u) ? line_width : -line_width;
	vtx.p.xy += offset;

	// Lines will be run as (0 1 2) (1 2 3)
	// This means that both triangles will have a point based off the top line point as their first point
	// So we don't have to do anything for !IIP

#elif VS_EXPAND == 3 // Sprite

	// Sprite points are always in pairs
	uint vid_base = vid >> 1;
	uint vid_lt = vid_base & ~1u;
	uint vid_rb = vid_base | 1u;

	ProcessedVertex lt = load_vertex(vid_lt);
	ProcessedVertex rb = load_vertex(vid_rb);
	vtx = rb;

	bool is_right = ((vid & 1u) != 0u);
	vtx.p.x = is_right ? lt.p.x : vtx.p.x;
	vtx.t.x = is_right ? lt.t.x : vtx.t.x;
	vtx.ti.xz = is_right ? lt.ti.xz : vtx.ti.xz;

	bool is_bottom = ((vid & 2u) != 0u);
	vtx.p.y = is_bottom ? lt.p.y : vtx.p.y;
	vtx.t.y = is_bottom ? lt.t.y : vtx.t.y;
	vtx.ti.yw = is_bottom ? lt.ti.yw : vtx.ti.yw;

#endif

	gl_Position = vtx.p;
	vsOut.t = vtx.t;
	vsOut.ti = vtx.ti;
	vsOut.c = vtx.c;
}

#endif // VS_EXPAND

#endif // VERTEX_SHADER

#ifdef FRAGMENT_SHADER

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
#define PS_ADJS 0
#define PS_ADJT 0
#define PS_FMT FMT_32
#define PS_AEM 0
#define PS_TFX 0
#define PS_TCC 1
#define PS_ATST 1
#define PS_FOG 0
#define PS_BLEND_HW 0
#define PS_A_MASKED 0
#define PS_FBA 0
#define PS_FBMASK 0
#define PS_LTF 1
#define PS_TCOFFSETHACK 0
#define PS_POINT_SAMPLER 0
#define PS_SHUFFLE 0
#define PS_SHUFFLE_SAME 0
#define PS_READ_BA 0
#define PS_WRITE_RG 0
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
#define PS_FIXED_ONE_A 0
#define PS_PABE 0
#define PS_DITHER 0
#define PS_ZCLAMP 0
#define PS_FEEDBACK_LOOP 0
#define PS_TEX_IS_FB 0
#endif

#define SW_BLEND (PS_BLEND_A || PS_BLEND_B || PS_BLEND_D)
#define SW_BLEND_NEEDS_RT (SW_BLEND && (PS_BLEND_A == 1 || PS_BLEND_B == 1 || PS_BLEND_C == 1 || PS_BLEND_D == 1))
#define SW_AD_TO_HW (PS_BLEND_C == 1 && PS_A_MASKED)

#define PS_FEEDBACK_LOOP_IS_NEEDED (PS_TEX_IS_FB == 1 || PS_FBMASK || SW_BLEND_NEEDS_RT || SW_AD_TO_HW || (PS_DATE >= 5))

#define NEEDS_TEX (PS_TFX != 4)

layout(std140, set = 0, binding = 1) uniform cb1
{
	vec3 FogColor;
	float AREF;
	vec4 WH;
	vec2 TA;
	float MaxDepthPS;
	float Af;
	uvec4 FbMask;
	vec4 HalfTexel;
	vec4 MinMax;
	vec4 STRange;
	ivec4 ChannelShuffle;
	vec2 TC_OffsetHack;
	vec2 STScale;
	mat4 DitherMatrix;
	float ScaledScaleFactor;
	float RcpScaleFactor;
};

layout(location = 0) in VSOutput
{
	vec4 t;
	vec4 ti;
	#if PS_IIP != 0
		vec4 c;
	#else
		flat vec4 c;
	#endif
} vsIn;

#if !defined(DISABLE_DUAL_SOURCE) && !PS_NO_COLOR1
layout(location = 0, index = 0) out vec4 o_col0;
layout(location = 0, index = 1) out vec4 o_col1;
#elif !PS_NO_COLOR
layout(location = 0) out vec4 o_col0;
#endif

#if NEEDS_TEX
layout(set = 1, binding = 0) uniform sampler2D Texture;
layout(set = 1, binding = 1) uniform texture2D Palette;
#endif

#if PS_FEEDBACK_LOOP_IS_NEEDED
	#if defined(DISABLE_TEXTURE_BARRIER) || defined(HAS_FEEDBACK_LOOP_LAYOUT)
		layout(set = 1, binding = 2) uniform texture2D RtSampler;
		vec4 sample_from_rt() { return texelFetch(RtSampler, ivec2(gl_FragCoord.xy), 0); }
	#else
		layout(input_attachment_index = 0, set = 1, binding = 2) uniform subpassInput RtSampler;
		vec4 sample_from_rt() { return subpassLoad(RtSampler); }
	#endif
#endif

#if PS_DATE > 0
layout(set = 1, binding = 3) uniform texture2D PrimMinTexture;
#endif

#if NEEDS_TEX

vec4 sample_c(vec2 uv)
{
#if PS_TEX_IS_FB
	return sample_from_rt();
#elif PS_REGION_RECT
	return texelFetch(Texture, ivec2(uv), 0);
#else
#if PS_POINT_SAMPLER
		// Weird issue with ATI/AMD cards,
		// it looks like they add 127/128 of a texel to sampling coordinates
		// occasionally causing point sampling to erroneously round up.
		// I'm manually adjusting coordinates to the centre of texels here,
		// though the centre is just paranoia, the top left corner works fine.
		// As of 2018 this issue is still present.
		uv = (trunc(uv * WH.zw) + vec2(0.5, 0.5)) / WH.zw;
#endif
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
	return texture(Texture, uv);
#elif PS_MANUAL_LOD == 1
	// FIXME add LOD: K - ( LOG2(Q) * (1 << L))
	float K = MinMax.x;
	float L = MinMax.y;
	float bias = MinMax.z;
	float max_lod = MinMax.w;

	float gs_lod = K - log2(abs(vsIn.t.w)) * L;
	// FIXME max useful ?
	//float lod = max(min(gs_lod, max_lod) - bias, 0.0f);
	float lod = min(gs_lod, max_lod) - bias;

	return textureLod(Texture, uv, lod);
#else
	return textureLod(Texture, uv, 0); // No lod
#endif
#endif
}

vec4 sample_p(uint idx)
{
	return texelFetch(Palette, ivec2(int(idx), 0), 0);
}

vec4 sample_p_norm(float u)
{
	return sample_p(uint(u * 255.5f));
}

vec4 clamp_wrap_uv(vec4 uv)
{
	vec4 tex_size = WH.xyxy;

	#if PS_WMS == PS_WMT
	{
		#if PS_REGION_RECT == 1 && PS_WMS == 0
		{
			uv = fract(uv);
		}
		#elif PS_REGION_RECT == 1 && PS_WMS == 1
		{
			uv = clamp(uv, vec4(0.0f), vec4(1.0f));
		}
		#elif PS_WMS == 2
		{
			uv = clamp(uv, MinMax.xyxy, MinMax.zwzw);
		}
		#elif PS_WMS == 3
		{
			#if PS_FST == 0
			// wrap negative uv coords to avoid an off by one error that shifted
			// textures. Fixes Xenosaga's hair issue.
			uv = fract(uv);
			#endif
			uv = vec4((uvec4(uv * tex_size) & floatBitsToUint(MinMax.xyxy)) | floatBitsToUint(MinMax.zwzw)) / tex_size;
		}
		#endif
	}
	#else
	{
		#if PS_REGION_RECT == 1 && PS_WMS == 0
		{
			uv.xz = fract(uv.xz);
		}
		#elif PS_REGION_RECT == 1 && PS_WMS == 1
		{
			uv.xz = clamp(uv.xz, vec2(0.0f), vec2(1.0f));
		}
		#elif PS_WMS == 2
		{
			uv.xz = clamp(uv.xz, MinMax.xx, MinMax.zz);
		}
		#elif PS_WMS == 3
		{
			#if PS_FST == 0
			uv.xz = fract(uv.xz);
			#endif
			uv.xz = vec2((uvec2(uv.xz * tex_size.xx) & floatBitsToUint(MinMax.xx)) | floatBitsToUint(MinMax.zz)) / tex_size.xx;
		}
		#endif
		#if PS_REGION_RECT == 1 && PS_WMT == 0
		{
			uv.yw = fract(uv.yw);
		}
		#elif PS_REGION_RECT == 1 && PS_WMT == 1
		{
			uv.yw = clamp(uv.yw, vec2(0.0f), vec2(1.0f));
		}
		#elif PS_WMT == 2
		{
			uv.yw = clamp(uv.yw, MinMax.yy, MinMax.ww);
		}
		#elif PS_WMT == 3
		{
			#if PS_FST == 0
			uv.yw = fract(uv.yw);
			#endif
			uv.yw = vec2((uvec2(uv.yw * tex_size.yy) & floatBitsToUint(MinMax.yy)) | floatBitsToUint(MinMax.ww)) / tex_size.yy;
		}
		#endif
	}
	#endif

	#if PS_REGION_RECT == 1
		// Normalized -> Integer Coordinates.
		uv = clamp(uv * WH.zwzw + STRange.xyxy, STRange.xyxy, STRange.zwzw);
	#endif

	return uv;
}

mat4 sample_4c(vec4 uv)
{
	mat4 c;

	c[0] = sample_c(uv.xy);
	c[1] = sample_c(uv.zy);
	c[2] = sample_c(uv.xw);
	c[3] = sample_c(uv.zw);

	return c;
}

uvec4 sample_4_index(vec4 uv)
{
	vec4 c;

	c.x = sample_c(uv.xy).a;
	c.y = sample_c(uv.zy).a;
	c.z = sample_c(uv.xw).a;
	c.w = sample_c(uv.zw).a;

	// Denormalize value
	uvec4 i = uvec4(c * 255.5f);

	#if PS_PAL_FMT == 1
		// 4HL
		return i & 0xFu;
	#elif PS_PAL_FMT == 2
		// 4HH
		return i >> 4u;
	#else
		// 8
		return i;
	#endif
}

mat4 sample_4p(uvec4 u)
{
	mat4 c;

	c[0] = sample_p(u.x);
	c[1] = sample_p(u.y);
	c[2] = sample_p(u.z);
	c[3] = sample_p(u.w);

	return c;
}

int fetch_raw_depth(ivec2 xy)
{
#if PS_TEX_IS_FB
	vec4 col = sample_from_rt();
#else
	vec4 col = texelFetch(Texture, xy, 0);
#endif
	return int(col.r * exp2(32.0f));
}

vec4 fetch_raw_color(ivec2 xy)
{
#if PS_TEX_IS_FB
	return sample_from_rt();
#else
	return texelFetch(Texture, xy, 0);
#endif
}

vec4 fetch_c(ivec2 uv)
{
#if PS_TEX_IS_FB
	return sample_from_rt();
#else
	return texelFetch(Texture, uv, 0);
#endif
}

//////////////////////////////////////////////////////////////////////
// Depth sampling
//////////////////////////////////////////////////////////////////////

ivec2 clamp_wrap_uv_depth(ivec2 uv)
{
	ivec4 mask = floatBitsToInt(MinMax) << 4;
	#if (PS_WMS == PS_WMT)
	{
		#if (PS_WMS == 2)
		{
			uv = clamp(uv, mask.xy, mask.zw);
		}
		#elif (PS_WMS == 3)
		{
			uv = (uv & mask.xy) | mask.zw;
		}
		#endif
	}
	#else
	{
		#if (PS_WMS == 2)
		{
			uv.x = clamp(uv.x, mask.x, mask.z);
		}
		#elif (PS_WMS == 3)
		{
			uv.x = (uv.x & mask.x) | mask.z;
		}
		#endif
		#if (PS_WMT == 2)
		{
			uv.y = clamp(uv.y, mask.y, mask.w);
		}
		#elif (PS_WMT == 3)
		{
			uv.y = (uv.y & mask.y) | mask.w;
		}
		#endif
	}
	#endif
	return uv;
}

vec4 sample_depth(vec2 st, ivec2 pos)
{
	vec2 uv_f = vec2(clamp_wrap_uv_depth(ivec2(st))) * vec2(ScaledScaleFactor);

	#if PS_REGION_RECT == 1
		uv_f = clamp(uv_f + STRange.xy, STRange.xy, STRange.zw);
	#endif

	ivec2 uv = ivec2(uv_f);
	vec4 t = vec4(0.0f);

	#if (PS_TALES_OF_ABYSS_HLE == 1)
	{
		// Warning: UV can't be used in channel effect
		int depth = fetch_raw_depth(pos);

		// Convert msb based on the palette
		t = texelFetch(Palette, ivec2((depth >> 8) & 0xFF, 0), 0) * 255.0f;
	}
	#elif (PS_URBAN_CHAOS_HLE == 1)
	{
		// Depth buffer is read as a RGB5A1 texture. The game try to extract the green channel.
		// So it will do a first channel trick to extract lsb, value is right-shifted.
		// Then a new channel trick to extract msb which will shifted to the left.
		// OpenGL uses a vec32 format for the depth so it requires a couple of conversion.
		// To be faster both steps (msb&lsb) are done in a single pass.

		// Warning: UV can't be used in channel effect
		int depth = fetch_raw_depth(pos);

		// Convert lsb based on the palette
		t = texelFetch(Palette, ivec2(depth & 0xFF, 0), 0) * 255.0f;

		// Msb is easier
		float green = float(((depth >> 8) & 0xFF) * 36.0f);
		green = min(green, 255.0f);
		t.g += green;
	}
	#elif (PS_DEPTH_FMT == 1)
	{
		// Based on ps_convert_float32_rgba8 of convert

		// Convert a vec32 depth texture into a RGBA color texture
		uint d = uint(fetch_c(uv).r * exp2(32.0f));
		t = vec4(uvec4((d & 0xFFu), ((d >> 8) & 0xFFu), ((d >> 16) & 0xFFu), (d >> 24)));
	}
	#elif (PS_DEPTH_FMT == 2)
	{
		// Based on ps_convert_float16_rgb5a1 of convert

		// Convert a vec32 (only 16 lsb) depth into a RGB5A1 color texture
		uint d = uint(fetch_c(uv).r * exp2(32.0f));
		t = vec4(uvec4((d & 0x1Fu), ((d >> 5) & 0x1Fu), ((d >> 10) & 0x1Fu), (d >> 15) & 0x01u)) * vec4(8.0f, 8.0f, 8.0f, 128.0f);
	}
	#elif (PS_DEPTH_FMT == 3)
	{
		// Convert a RGBA/RGB5A1 color texture into a RGBA/RGB5A1 color texture
		t = fetch_c(uv) * 255.0f;
	}
	#endif

	#if (PS_AEM_FMT == FMT_24)
	{
		t.a = ((PS_AEM == 0) || any(bvec3(t.rgb))) ? 255.0f * TA.x : 0.0f;
	}
	#elif (PS_AEM_FMT == FMT_16)
	{
		t.a = t.a >= 128.0f ? 255.0f * TA.y : ((PS_AEM == 0) || any(bvec3(t.rgb))) ? 255.0f * TA.x : 0.0f;
	}
	#endif

	return t;
}

//////////////////////////////////////////////////////////////////////
// Fetch a Single Channel
//////////////////////////////////////////////////////////////////////

vec4 fetch_red(ivec2 xy)
{
	vec4 rt;

	#if (PS_DEPTH_FMT == 1) || (PS_DEPTH_FMT == 2)
		int depth = (fetch_raw_depth(xy)) & 0xFF;
		rt = vec4(float(depth) / 255.0f);
	#else
		rt = fetch_raw_color(xy);
	#endif

	return sample_p_norm(rt.r) * 255.0f;
}

vec4 fetch_green(ivec2 xy)
{
	vec4 rt;

	#if (PS_DEPTH_FMT == 1) || (PS_DEPTH_FMT == 2)
		int depth = (fetch_raw_depth(xy) >> 8) & 0xFF;
		rt = vec4(float(depth) / 255.0f);
	#else
		rt = fetch_raw_color(xy);
	#endif

	return sample_p_norm(rt.g) * 255.0f;
}

vec4 fetch_blue(ivec2 xy)
{
	vec4 rt;

	#if (PS_DEPTH_FMT == 1) || (PS_DEPTH_FMT == 2)
		int depth = (fetch_raw_depth(xy) >> 16) & 0xFF;
		rt = vec4(float(depth) / 255.0f);
	#else
		rt = fetch_raw_color(xy);
	#endif

	return sample_p_norm(rt.b) * 255.0f;
}

vec4 fetch_alpha(ivec2 xy)
{
	vec4 rt = fetch_raw_color(xy);
	return sample_p_norm(rt.a) * 255.0f;
}

vec4 fetch_rgb(ivec2 xy)
{
	vec4 rt = fetch_raw_color(xy);
	vec4 c = vec4(sample_p_norm(rt.r).r, sample_p_norm(rt.g).g, sample_p_norm(rt.b).b, 1.0);
	return c * 255.0f;
}

vec4 fetch_gXbY(ivec2 xy)
{
	#if (PS_DEPTH_FMT == 1) || (PS_DEPTH_FMT == 2)
		int depth = fetch_raw_depth(xy);
		int bg = (depth >> (8 + ChannelShuffle.w)) & 0xFF;
		return vec4(bg);
	#else
		ivec4 rt = ivec4(fetch_raw_color(xy) * 255.0);
		int green = (rt.g >> ChannelShuffle.w) & ChannelShuffle.z;
		int blue = (rt.b << ChannelShuffle.y) & ChannelShuffle.x;
		return vec4(float(green | blue));
	#endif
}

vec4 sample_color(vec2 st)
{
	#if PS_TCOFFSETHACK
	st += TC_OffsetHack.xy;
	#endif

	vec4 t;
	mat4 c;
	vec2 dd;

	#if PS_LTF == 0 && PS_AEM_FMT == FMT_32 && PS_PAL_FMT == 0 && PS_REGION_RECT == 0 && PS_WMS < 2 && PS_WMT < 2
	{
		c[0] = sample_c(st);
	}
	#else
	{
		vec4 uv;

		#if PS_LTF
		{
			uv = st.xyxy + HalfTexel;
			dd = fract(uv.xy * WH.zw);

			#if PS_FST == 0
			{
				dd = clamp(dd, vec2(0.0f), vec2(0.9999999f));
			}
			#endif
		}
		#else
		{
			uv = st.xyxy;
		}
		#endif

		uv = clamp_wrap_uv(uv);

#if PS_PAL_FMT != 0
			c = sample_4p(sample_4_index(uv));
#else
			c = sample_4c(uv);
#endif
	}
	#endif

	for (uint i = 0; i < 4; i++)
	{
		#if (PS_AEM_FMT == FMT_24)
			c[i].a = (PS_AEM == 0 || any(bvec3(c[i].rgb))) ? TA.x : 0.0f;
		#elif (PS_AEM_FMT == FMT_16)
			c[i].a = (c[i].a >= 0.5) ? TA.y : ((PS_AEM == 0 || any(bvec3(ivec3(c[i].rgb * 255.0f) & ivec3(0xF8)))) ? TA.x : 0.0f);
		#endif
	}

	#if PS_LTF
	{
		t = mix(mix(c[0], c[1], dd.x), mix(c[2], c[3], dd.x), dd.y);
	}
	#else
	{
		t = c[0];
	}
	#endif

	return trunc(t * 255.0f + 0.05f);
}

#endif // NEEDS_TEX

vec4 tfx(vec4 T, vec4 C)
{
	vec4 C_out;
	vec4 FxT = trunc((C * T) / 128.0f);

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

void atst(vec4 C)
{
	float a = C.a;

	#if (PS_ATST == 0)
	{
		// nothing to do
	}
	#elif (PS_ATST == 1)
	{
		if (a > AREF) discard;
	}
	#elif (PS_ATST == 2)
	{
		if (a < AREF) discard;
	}
	#elif (PS_ATST == 3)
	{
		if (abs(a - AREF) > 0.5f) discard;
	}
	#elif (PS_ATST == 4)
	{
		if (abs(a - AREF) < 0.5f) discard;
	}
	#endif
}

vec4 fog(vec4 c, float f)
{
	#if PS_FOG
		c.rgb = trunc(mix(FogColor, c.rgb, f));
	#endif

	return c;
}

vec4 ps_color()
{
#if PS_FST == 0
	vec2 st = vsIn.t.xy / vsIn.t.w;
	vec2 st_int = vsIn.ti.zw / vsIn.t.w;
#else
	vec2 st = vsIn.ti.xy;
	vec2 st_int = vsIn.ti.zw;
#endif

#if !NEEDS_TEX
	vec4 T = vec4(0.0f);
#elif PS_CHANNEL_FETCH == 1
	vec4 T = fetch_red(ivec2(gl_FragCoord.xy));
#elif PS_CHANNEL_FETCH == 2
	vec4 T = fetch_green(ivec2(gl_FragCoord.xy));
#elif PS_CHANNEL_FETCH == 3
	vec4 T = fetch_blue(ivec2(gl_FragCoord.xy));
#elif PS_CHANNEL_FETCH == 4
	vec4 T = fetch_alpha(ivec2(gl_FragCoord.xy));
#elif PS_CHANNEL_FETCH == 5
	vec4 T = fetch_rgb(ivec2(gl_FragCoord.xy));
#elif PS_CHANNEL_FETCH == 6
	vec4 T = fetch_gXbY(ivec2(gl_FragCoord.xy));
#elif PS_DEPTH_FMT > 0
	vec4 T = sample_depth(st_int, ivec2(gl_FragCoord.xy));
#else
	vec4 T = sample_color(st);
#endif

	#if PS_SHUFFLE && !PS_SHUFFLE_SAME && !PS_READ16_SRC
		uvec4 denorm_c_before = uvec4(T);
		#if PS_READ_BA
			T.r = float((denorm_c_before.b << 3) & 0xF8);
			T.g = float(((denorm_c_before.b >> 2) & 0x38) | ((denorm_c_before.a << 6) & 0xC0));
			T.b = float((denorm_c_before.a << 1) & 0xF8);
			T.a = float(denorm_c_before.a & 0x80);
		#else
			T.r = float((denorm_c_before.r << 3) & 0xF8);
			T.g = float(((denorm_c_before.r >> 2) & 0x38) | ((denorm_c_before.g << 6) & 0xC0));
			T.b = float((denorm_c_before.g << 1) & 0xF8);
			T.a = float(denorm_c_before.g & 0x80);
		#endif
	#endif
	
	vec4 C = tfx(T, vsIn.c);

	atst(C);

	C = fog(C, vsIn.t.z);

	return C;
}

void ps_fbmask(inout vec4 C)
{
	#if PS_FBMASK
		vec4 RT = trunc(sample_from_rt() * 255.0f + 0.1f);
		C = vec4((uvec4(C) & ~FbMask) | (uvec4(RT) & FbMask));
	#endif
}

void ps_dither(inout vec3 C)
{
	#if PS_DITHER
		ivec2 fpos;

		#if PS_DITHER == 2
			fpos = ivec2(gl_FragCoord.xy);
		#else
			fpos = ivec2(gl_FragCoord.xy * RcpScaleFactor);
		#endif

		float value = DitherMatrix[fpos.y & 3][fpos.x & 3];
		#if PS_ROUND_INV
			C -= value;
		#else
			C += value;
		#endif
	#endif
}

void ps_color_clamp_wrap(inout vec3 C)
{
	// When dithering the bottom 3 bits become meaningless and cause lines in the picture
	// so we need to limit the color depth on dithered items
#if SW_BLEND || PS_DITHER || PS_FBMASK

#if PS_DST_FMT == FMT_16 && PS_BLEND_MIX == 0 && PS_ROUND_INV
	C += 7.0f; // Need to round up, not down since the shader will invert
#endif

	// Correct the Color value based on the output format
#if PS_COLCLIP == 0 && PS_HDR == 0
	// Standard Clamp
	C = clamp(C, vec3(0.0f), vec3(255.0f));
#endif

	// FIXME rouding of negative float?
	// compiler uses trunc but it might need floor

	// Warning: normally blending equation is mult(A, B) = A * B >> 7. GPU have the full accuracy
	// GS: Color = 1, Alpha = 255 => output 1
	// GPU: Color = 1/255, Alpha = 255/255 * 255/128 => output 1.9921875
#if PS_DST_FMT == FMT_16 && PS_BLEND_MIX == 0
	// In 16 bits format, only 5 bits of colors are used. It impacts shadows computation of Castlevania
	C = vec3(ivec3(C) & ivec3(0xF8));
#elif PS_COLCLIP == 1 || PS_HDR == 1
	C = vec3(ivec3(C) & ivec3(0xFF));
#endif

#endif
}

void ps_blend(inout vec4 Color, inout vec4 As_rgba)
{
	float As = As_rgba.a;

	#if SW_BLEND

		// PABE
		#if PS_PABE
			// No blending so early exit
			if (As < 1.0f)
				return;
		#endif

		#if PS_FEEDBACK_LOOP_IS_NEEDED
			vec4 RT = trunc(sample_from_rt() * 255.0f + 0.1f);
		#else
			// Not used, but we define it to make the selection below simpler.
			vec4 RT = vec4(0.0f);
		#endif

			// FIXME FMT_16 case
			// FIXME Ad or Ad * 2?
			float Ad = RT.a / 128.0f;

			// Let the compiler do its jobs !
			vec3 Cd = RT.rgb;
			vec3 Cs = Color.rgb;

		#if PS_BLEND_A == 0
			vec3 A = Cs;
		#elif PS_BLEND_A == 1
			vec3 A = Cd;
		#else
			vec3 A = vec3(0.0f);
		#endif

		#if PS_BLEND_B == 0
			vec3 B = Cs;
		#elif PS_BLEND_B == 1
			vec3 B = Cd;
		#else
			vec3 B = vec3(0.0f);
		#endif

		#if PS_BLEND_C == 0
			float C = As;
		#elif PS_BLEND_C == 1
			float C = Ad;
		#else
			float C = Af;
		#endif

		#if PS_BLEND_D == 0
			vec3 D = Cs;
		#elif PS_BLEND_D == 1
			vec3 D = Cd;
		#else
			vec3 D = vec3(0.0f);
		#endif

		// As/Af clamp alpha for Blend mix
		// We shouldn't clamp blend mix with blend hw 1 as we want alpha higher
		float C_clamped = C;
		#if PS_BLEND_MIX > 0 && PS_BLEND_HW != 1
			C_clamped = min(C_clamped, 1.0f);
		#endif

		#if PS_BLEND_A == PS_BLEND_B
			Color.rgb = D;
		// In blend_mix, HW adds on some alpha factor * dst.
		// Truncating here wouldn't quite get the right result because it prevents the <1 bit here from combining with a <1 bit in dst to form a ≥1 amount that pushes over the truncation.
		// Instead, apply an offset to convert HW's round to a floor.
		// Since alpha is in 1/128 increments, subtracting (0.5 - 0.5/128 == 127/256) would get us what we want if GPUs blended in full precision.
		// But they don't.  Details here: https://github.com/PCSX2/pcsx2/pull/6809#issuecomment-1211473399
		// Based on the scripts at the above link, the ideal choice for Intel GPUs is 126/256, AMD 120/256.  Nvidia is a lost cause.
		// 124/256 seems like a reasonable compromise, providing the correct answer 99.3% of the time on Intel (vs 99.6% for 126/256), and 97% of the time on AMD (vs 97.4% for 120/256).
		#elif PS_BLEND_MIX == 2
			Color.rgb = ((A - B) * C_clamped + D) + (124.0f/256.0f);
		#elif PS_BLEND_MIX == 1
			Color.rgb = ((A - B) * C_clamped + D) - (124.0f/256.0f);
		#else
				Color.rgb = trunc((A - B) * C + D);
		#endif

		#if PS_BLEND_HW == 1
			// As or Af
			As_rgba.rgb = vec3(C);
			// Subtract 1 for alpha to compensate for the changed equation,
			// if c.rgb > 255.0f then we further need to adjust alpha accordingly,
			// we pick the lowest overflow from all colors because it's the safest,
			// we divide by 255 the color because we don't know Cd value,
			// changed alpha should only be done for hw blend.
			vec3 alpha_compensate = max(vec3(1.0f), Color.rgb / vec3(255.0f));
			As_rgba.rgb -= alpha_compensate;
		#elif PS_BLEND_HW == 2
			// Compensate slightly for Cd*(As + 1) - Cs*As.
			// The initial factor we chose is 1 (0.00392)
			// as that is the minimum color Cd can be,
			// then we multiply by alpha to get the minimum
			// blended value it can be.
			float color_compensate = 1.0f * (C + 1.0f);
			Color.rgb -= vec3(color_compensate);
		#elif PS_BLEND_HW == 3
			// As, Ad or Af clamped.
			As_rgba.rgb = vec3(C_clamped);
			// Cs*(Alpha + 1) might overflow, if it does then adjust alpha value
			// that is sent on second output to compensate.
			vec3 overflow_check = (Color.rgb - vec3(255.0f)) / 255.0f;
			vec3 alpha_compensate = max(vec3(0.0f), overflow_check);
			As_rgba.rgb -= alpha_compensate;
		#endif

	#else
		#if PS_BLEND_HW == 1
			// Needed for Cd * (As/Ad/F + 1) blending modes
			Color.rgb = vec3(255.0f);
		#elif PS_BLEND_HW == 2
			// Cd*As,Cd*Ad or Cd*F

			#if PS_BLEND_C == 2
				float Alpha = Af;
			#else
				float Alpha = As;
			#endif

			Color.rgb = max(vec3(0.0f), (Alpha - vec3(1.0f)));
			Color.rgb *= vec3(255.0f);
		#elif PS_BLEND_HW == 3
			// Needed for Cs*Ad, Cs*Ad + Cd, Cd - Cs*Ad
			// Multiply Color.rgb by (255/128) to compensate for wrong Ad/255 value when rgb are below 128.
			// When any color channel is higher than 128 then adjust the compensation automatically
			// to give us more accurate colors, otherwise they will be wrong.
			// The higher the value (>128) the lower the compensation will be.
			float max_color = max(max(Color.r, Color.g), Color.b);
			float color_compensate = 255.0f / max(128.0f, max_color);
			Color.rgb *= vec3(color_compensate);
		#endif
	#endif
}

void main()
{
#if PS_SCANMSK & 2
	// fail depth test on prohibited lines
	if ((int(gl_FragCoord.y) & 1) == (PS_SCANMSK & 1))
		discard;
#endif
#if PS_DATE >= 5

#if PS_WRITE_RG == 1
	// Pseudo 16 bits access.
	float rt_a = sample_from_rt().g;
#else
	float rt_a = sample_from_rt().a;
#endif

#if (PS_DATE & 3) == 1
	// DATM == 0: Pixel with alpha equal to 1 will failed
	bool bad = (127.5f / 255.0f) < rt_a;
#elif (PS_DATE & 3) == 2
	// DATM == 1: Pixel with alpha equal to 0 will failed
	bool bad = rt_a < (127.5f / 255.0f);
#endif

	if (bad) {
		discard;
	}

#endif		// PS_DATE >= 5

#if PS_DATE == 3
	int stencil_ceil = int(texelFetch(PrimMinTexture, ivec2(gl_FragCoord.xy), 0).r);
	// Note gl_PrimitiveID == stencil_ceil will be the primitive that will update
	// the bad alpha value so we must keep it.

	if (gl_PrimitiveID > stencil_ceil) {
		discard;
	}
#endif

	vec4 C = ps_color();

	// Must be done before alpha correction

	// AA (Fixed one) will output a coverage of 1.0 as alpha
#if PS_FIXED_ONE_A
	C.a = 128.0f;
#endif

#if (SW_AD_TO_HW)
	vec4 RT = trunc(sample_from_rt() * 255.0f + 0.1f);
	vec4 alpha_blend = vec4(RT.a / 128.0f);
#else
	vec4 alpha_blend = vec4(C.a / 128.0f);
#endif

  // Correct the ALPHA value based on the output format
#if (PS_DST_FMT == FMT_16)
	float A_one = 128.0f; // alpha output will be 0x80
	C.a = (PS_FBA != 0) ? A_one : step(128.0f, C.a) * A_one;
#elif (PS_DST_FMT == FMT_32) && (PS_FBA != 0)
	if(C.a < 128.0f) C.a += 128.0f;
#endif

	// Get first primitive that will write a failling alpha value
#if PS_DATE == 1

	// DATM == 0
	// Pixel with alpha equal to 1 will failed (128-255)
	o_col0 = (C.a > 127.5f) ? vec4(gl_PrimitiveID) : vec4(0x7FFFFFFF);

#elif PS_DATE == 2

	// DATM == 1
	// Pixel with alpha equal to 0 will failed (0-127)
	o_col0 = (C.a < 127.5f) ? vec4(gl_PrimitiveID) : vec4(0x7FFFFFFF);

#else
	ps_blend(C, alpha_blend);

#if PS_SHUFFLE
		#if !PS_SHUFFLE_SAME && !PS_READ16_SRC
			uvec4 denorm_c_after = uvec4(C);
			#if PS_READ_BA
				C.b = float(((denorm_c_after.r >> 3) & 0x1F) | ((denorm_c_after.g << 2) & 0xE0));
				C.a = float(((denorm_c_after.g >> 6) & 0x3) | ((denorm_c_after.b >> 1) & 0x7C) | (denorm_c_after.a & 0x80));
			#else
				C.r = float(((denorm_c_after.r >> 3) & 0x1F) | ((denorm_c_after.g << 2) & 0xE0));
				C.g = float(((denorm_c_after.g >> 6) & 0x3) | ((denorm_c_after.b >> 1) & 0x7C) | (denorm_c_after.a & 0x80));
			#endif
		#endif

		uvec4 denorm_c = uvec4(C);
		uvec2 denorm_TA = uvec2(vec2(TA.xy) * 255.0f + 0.5f);
		
		// Special case for 32bit input and 16bit output, shuffle used by The Godfather
		#if PS_SHUFFLE_SAME
			#if (PS_READ_BA)
				C = vec4(float((denorm_c.b & 0x7Fu) | (denorm_c.a & 0x80u)));
			#else
				C.ga = C.rg;
			#endif
		// Copy of a 16bit source in to this target
		#elif PS_READ16_SRC
			C.rb = vec2(float((denorm_c.r >> 3) | (((denorm_c.g >> 3) & 0x7u) << 5)));
			if ((denorm_c.a & 0x80u) != 0u)
				C.ga = vec2(float((denorm_c.g >> 6) | ((denorm_c.b >> 3) << 2) | (denorm_TA.y & 0x80u)));
			else
				C.ga = vec2(float((denorm_c.g >> 6) | ((denorm_c.b >> 3) << 2) | (denorm_TA.x & 0x80u)));
		// Write RB part. Mask will take care of the correct destination
		#elif PS_READ_BA
			C.rb = C.bb;
			if ((denorm_c.a & 0x80u) != 0u)
				C.ga = vec2(float((denorm_c.a & 0x7Fu) | (denorm_TA.y & 0x80u)));
			else
				C.ga = vec2(float((denorm_c.a & 0x7Fu) | (denorm_TA.x & 0x80u)));
		#else
			C.rb = C.rr;
			if ((denorm_c.g & 0x80u) != 0u)
				C.ga = vec2(float((denorm_c.g & 0x7Fu) | (denorm_TA.y & 0x80u)));
			else
				C.ga = vec2(float((denorm_c.g & 0x7Fu) | (denorm_TA.x & 0x80u)));
		#endif // PS_SHUFFLE_SAME
	#endif // PS_SHUFFLE

	ps_dither(C.rgb);

	// Color clamp/wrap needs to be done after sw blending and dithering
	ps_color_clamp_wrap(C.rgb);

	ps_fbmask(C);

	#if !PS_NO_COLOR
		#if PS_HDR == 1
			o_col0 = vec4(C.rgb / 65535.0f, C.a / 255.0f);
		#else
			o_col0 = C / 255.0f;
		#endif
		#if !defined(DISABLE_DUAL_SOURCE) && !PS_NO_COLOR1
			o_col1 = alpha_blend;
		#endif

		#if PS_NO_ABLEND
			// write alpha blend factor into col0
			o_col0.a = alpha_blend.a;
		#endif
		#if PS_ONLY_ALPHA
			// rgb isn't used
			o_col0.rgb = vec3(0.0f);
		#endif
	#endif

	#if PS_ZCLAMP
		gl_FragDepth = min(gl_FragCoord.z, MaxDepthPS);
	#endif

#endif // PS_DATE
}

#endif
