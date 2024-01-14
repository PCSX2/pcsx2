// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GSMTLShaderCommon.h"

constant uint FMT_32 = 0;
constant uint FMT_24 = 1;
constant uint FMT_16 = 2;

constant bool HAS_FBFETCH           [[function_constant(GSMTLConstantIndex_FRAMEBUFFER_FETCH)]];
constant bool FST                   [[function_constant(GSMTLConstantIndex_FST)]];
constant bool IIP                   [[function_constant(GSMTLConstantIndex_IIP)]];
constant bool VS_POINT_SIZE         [[function_constant(GSMTLConstantIndex_VS_POINT_SIZE)]];
constant uint VS_EXPAND_TYPE_RAW    [[function_constant(GSMTLConstantIndex_VS_EXPAND_TYPE)]];
constant uint PS_AEM_FMT            [[function_constant(GSMTLConstantIndex_PS_AEM_FMT)]];
constant uint PS_PAL_FMT            [[function_constant(GSMTLConstantIndex_PS_PAL_FMT)]];
constant uint PS_DST_FMT            [[function_constant(GSMTLConstantIndex_PS_DST_FMT)]];
constant uint PS_DEPTH_FMT          [[function_constant(GSMTLConstantIndex_PS_DEPTH_FMT)]];
constant bool PS_AEM                [[function_constant(GSMTLConstantIndex_PS_AEM)]];
constant bool PS_FBA                [[function_constant(GSMTLConstantIndex_PS_FBA)]];
constant bool PS_FOG                [[function_constant(GSMTLConstantIndex_PS_FOG)]];
constant uint PS_DATE               [[function_constant(GSMTLConstantIndex_PS_DATE)]];
constant uint PS_ATST               [[function_constant(GSMTLConstantIndex_PS_ATST)]];
constant uint PS_TFX                [[function_constant(GSMTLConstantIndex_PS_TFX)]];
constant bool PS_TCC                [[function_constant(GSMTLConstantIndex_PS_TCC)]];
constant uint PS_WMS                [[function_constant(GSMTLConstantIndex_PS_WMS)]];
constant uint PS_WMT                [[function_constant(GSMTLConstantIndex_PS_WMT)]];
constant bool PS_ADJS               [[function_constant(GSMTLConstantIndex_PS_ADJS)]];
constant bool PS_ADJT               [[function_constant(GSMTLConstantIndex_PS_ADJT)]];
constant bool PS_LTF                [[function_constant(GSMTLConstantIndex_PS_LTF)]];
constant bool PS_SHUFFLE            [[function_constant(GSMTLConstantIndex_PS_SHUFFLE)]];
constant bool PS_SHUFFLE_SAME       [[function_constant(GSMTLConstantIndex_PS_SHUFFLE_SAME)]];
constant bool PS_READ_BA            [[function_constant(GSMTLConstantIndex_PS_READ_BA)]];
constant bool PS_READ16_SRC         [[function_constant(GSMTLConstantIndex_PS_READ16_SRC)]];
constant bool PS_WRITE_RG           [[function_constant(GSMTLConstantIndex_PS_WRITE_RG)]];
constant bool PS_FBMASK             [[function_constant(GSMTLConstantIndex_PS_FBMASK)]];
constant uint PS_BLEND_A            [[function_constant(GSMTLConstantIndex_PS_BLEND_A)]];
constant uint PS_BLEND_B            [[function_constant(GSMTLConstantIndex_PS_BLEND_B)]];
constant uint PS_BLEND_C            [[function_constant(GSMTLConstantIndex_PS_BLEND_C)]];
constant uint PS_BLEND_D            [[function_constant(GSMTLConstantIndex_PS_BLEND_D)]];
constant uint PS_BLEND_HW           [[function_constant(GSMTLConstantIndex_PS_BLEND_HW)]];
constant bool PS_A_MASKED           [[function_constant(GSMTLConstantIndex_PS_A_MASKED)]];
constant bool PS_HDR                [[function_constant(GSMTLConstantIndex_PS_HDR)]];
constant bool PS_COLCLIP            [[function_constant(GSMTLConstantIndex_PS_COLCLIP)]];
constant uint PS_BLEND_MIX          [[function_constant(GSMTLConstantIndex_PS_BLEND_MIX)]];
constant bool PS_ROUND_INV          [[function_constant(GSMTLConstantIndex_PS_ROUND_INV)]];
constant bool PS_FIXED_ONE_A        [[function_constant(GSMTLConstantIndex_PS_FIXED_ONE_A)]];
constant bool PS_PABE               [[function_constant(GSMTLConstantIndex_PS_PABE)]];
constant bool PS_NO_COLOR           [[function_constant(GSMTLConstantIndex_PS_NO_COLOR)]];
constant bool PS_NO_COLOR1          [[function_constant(GSMTLConstantIndex_PS_NO_COLOR1)]];
constant bool PS_ONLY_ALPHA         [[function_constant(GSMTLConstantIndex_PS_ONLY_ALPHA)]];
constant uint PS_CHANNEL            [[function_constant(GSMTLConstantIndex_PS_CHANNEL)]];
constant uint PS_DITHER             [[function_constant(GSMTLConstantIndex_PS_DITHER)]];
constant bool PS_ZCLAMP             [[function_constant(GSMTLConstantIndex_PS_ZCLAMP)]];
constant bool PS_TCOFFSETHACK       [[function_constant(GSMTLConstantIndex_PS_TCOFFSETHACK)]];
constant bool PS_URBAN_CHAOS_HLE    [[function_constant(GSMTLConstantIndex_PS_URBAN_CHAOS_HLE)]];
constant bool PS_TALES_OF_ABYSS_HLE [[function_constant(GSMTLConstantIndex_PS_TALES_OF_ABYSS_HLE)]];
constant bool PS_TEX_IS_FB          [[function_constant(GSMTLConstantIndex_PS_TEX_IS_FB)]];
constant bool PS_AUTOMATIC_LOD      [[function_constant(GSMTLConstantIndex_PS_AUTOMATIC_LOD)]];
constant bool PS_MANUAL_LOD         [[function_constant(GSMTLConstantIndex_PS_MANUAL_LOD)]];
constant bool PS_POINT_SAMPLER      [[function_constant(GSMTLConstantIndex_PS_POINT_SAMPLER)]];
constant bool PS_REGION_RECT        [[function_constant(GSMTLConstantIndex_PS_REGION_RECT)]];
constant uint PS_SCANMSK            [[function_constant(GSMTLConstantIndex_PS_SCANMSK)]];

constant GSMTLExpandType VS_EXPAND_TYPE = static_cast<GSMTLExpandType>(VS_EXPAND_TYPE_RAW);

#if defined(__METAL_MACOS__) && __METAL_VERSION__ >= 220
	#define PRIMID_SUPPORT 1
#else
	#define PRIMID_SUPPORT 0
#endif

#if defined(__METAL_IOS__) || __METAL_VERSION__ >= 230
	#define FBFETCH_SUPPORT 1
#else
	#define FBFETCH_SUPPORT 0
#endif

constant bool PS_PRIM_CHECKING_INIT = PS_DATE == 1 || PS_DATE == 2;
constant bool PS_PRIM_CHECKING_READ = PS_DATE == 3;
#if PRIMID_SUPPORT
constant bool NEEDS_PRIMID = PS_PRIM_CHECKING_INIT || PS_PRIM_CHECKING_READ;
#endif
constant bool PS_TEX_IS_DEPTH = PS_URBAN_CHAOS_HLE || PS_TALES_OF_ABYSS_HLE || PS_DEPTH_FMT == 1 || PS_DEPTH_FMT == 2;
constant bool PS_TEX_IS_COLOR = !PS_TEX_IS_DEPTH;
constant bool PS_HAS_PALETTE = PS_PAL_FMT != 0 || (PS_CHANNEL >= 1 && PS_CHANNEL <= 5);
constant bool NOT_IIP = !IIP;
constant bool SW_BLEND = (PS_BLEND_A != PS_BLEND_B) || PS_BLEND_D;
constant bool SW_AD_TO_HW = (PS_BLEND_C == 1 && PS_A_MASKED);
constant bool NEEDS_RT_FOR_BLEND = (((PS_BLEND_A != PS_BLEND_B) && (PS_BLEND_A == 1 || PS_BLEND_B == 1 || PS_BLEND_C == 1)) || PS_BLEND_D == 1 || SW_AD_TO_HW);
constant bool NEEDS_RT_EARLY = PS_TEX_IS_FB || PS_DATE >= 5;
constant bool NEEDS_RT = NEEDS_RT_EARLY || (!PS_PRIM_CHECKING_INIT && (PS_FBMASK || NEEDS_RT_FOR_BLEND));

constant bool PS_COLOR0 = !PS_NO_COLOR;
constant bool PS_COLOR1 = !PS_NO_COLOR1;

struct MainVSIn
{
	float2 st [[attribute(GSMTLAttributeIndexST)]];
	float4 c  [[attribute(GSMTLAttributeIndexC)]];
	float  q  [[attribute(GSMTLAttributeIndexQ)]];
	uint2  p  [[attribute(GSMTLAttributeIndexXY)]];
	uint   z  [[attribute(GSMTLAttributeIndexZ)]];
	uint2  uv [[attribute(GSMTLAttributeIndexUV)]];
	float4 f  [[attribute(GSMTLAttributeIndexF)]];
};

struct MainVSOut
{
	float4 p [[position]];
	float4 t;
	float4 ti;
	float4 c [[function_constant(IIP)]];
	float4 fc [[flat, function_constant(NOT_IIP)]];
	float point_size [[point_size, function_constant(VS_POINT_SIZE)]];
};

struct MainPSIn
{
	float4 p [[position]];
	float4 t;
	float4 ti;
	float4 c [[function_constant(IIP)]];
	float4 fc [[flat, function_constant(NOT_IIP)]];
};

struct MainPSOut
{
	float4 c0 [[color(0), index(0), function_constant(PS_COLOR0)]];
	float4 c1 [[color(0), index(1), function_constant(PS_COLOR1)]];
	float depth [[depth(less), function_constant(PS_ZCLAMP)]];
};

// MARK: - Vertex functions

static void texture_coord(thread const MainVSIn& v, thread MainVSOut& out, constant GSMTLMainVSUniform& cb)
{
	float2 uv = float2(v.uv) - cb.texture_offset;
	float2 st = v.st - cb.texture_offset;

	// Float coordinate
	out.t.xy = st;
	out.t.w = v.q;

	// Integer coordinate => normalized
	out.ti.xy = uv * cb.texture_scale;

	if (FST)
	{
		// Integer coordinate => integral
		out.ti.zw = uv;
	}
	else
	{
		// Some games uses float coordinate for post-processing effects
		out.ti.zw = st / cb.texture_scale;
	}
}

static MainVSOut vs_main_run(thread const MainVSIn& v, constant GSMTLMainVSUniform& cb)
{
	constexpr float exp_min32 = 0x1p-32;
	MainVSOut out;
	// Clamp to max depth, gs doesn't wrap
	uint z = min(v.z, cb.max_depth);
	out.p.xy = float2(v.p) - float2(0.05, 0.05);
	out.p.xy = out.p.xy * float2(cb.vertex_scale.x, -cb.vertex_scale.y) - float2(cb.vertex_offset.x, -cb.vertex_offset.y);
	out.p.w = 1;
	out.p.z = float(z) * exp_min32;

	texture_coord(v, out, cb);

	if (IIP)
		out.c = v.c;
	else
		out.fc = v.c;

	out.t.z = v.f.x; // pack fog with texture

	if (VS_POINT_SIZE)
		out.point_size = cb.point_size.x;

	return out;
}

vertex MainVSOut vs_main(MainVSIn v [[stage_in]], constant GSMTLMainVSUniform& cb [[buffer(GSMTLBufferIndexHWUniforms)]])
{
	return vs_main_run(v, cb);
}

static MainVSIn load_vertex(GSMTLMainVertex base)
{
	MainVSIn out;
	out.st = base.st;
	out.c = float4(base.rgba);
	out.q = base.q;
	out.p = uint2(base.xy);
	out.z = base.z;
	out.uv = uint2(base.uv);
	out.f = float4(static_cast<float>(base.fog) / 255.f);
	return out;
}

vertex MainVSOut vs_main_expand(
	uint vid [[vertex_id]],
	device const GSMTLMainVertex* vertices [[buffer(GSMTLBufferIndexHWVertices)]],
	constant GSMTLMainVSUniform& cb [[buffer(GSMTLBufferIndexHWUniforms)]])
{
	switch (VS_EXPAND_TYPE)
	{
		case GSMTLExpandType::None:
			return vs_main_run(load_vertex(vertices[vid]), cb);
		case GSMTLExpandType::Point:
		{
			MainVSOut point = vs_main_run(load_vertex(vertices[vid >> 2]), cb);
			if (vid & 1)
				point.p.x += cb.point_size.x;
			if (vid & 2)
				point.p.y += cb.point_size.y;
			return point;
		}
		case GSMTLExpandType::Line:
		{
			uint vid_base = vid >> 2;
			bool is_bottom = vid & 2;
			bool is_right = vid & 1;
			// All lines will be a pair of vertices next to each other
			// Since Metal uses provoking vertex first, the bottom point will be the lower of the two
			uint vid_other = is_bottom ? vid_base + 1 : vid_base - 1;
			MainVSOut point = vs_main_run(load_vertex(vertices[vid_base]), cb);
			MainVSOut other = vs_main_run(load_vertex(vertices[vid_other]), cb);

			float2 line_vector = normalize(point.p.xy - other.p.xy);
			float2 line_normal = float2(line_vector.y, -line_vector.x);
			float2 line_width = (line_normal * cb.point_size) / 2;
			// line_normal is inverted for bottom point
			float2 offset = (is_bottom ^ is_right) ? line_width : -line_width;
			point.p.xy += offset;

			// Lines will be run as (0 1 2) (1 2 3)
			// This means that both triangles will have a point based off the top line point as their first point
			// So we don't have to do anything for !IIP

			return point;
		}
		case GSMTLExpandType::Sprite:
		{
			uint vid_base = vid >> 1;
			bool is_bottom = vid & 2;
			bool is_right = vid & 1;
			// Sprite points are always in pairs
			uint vid_lt = vid_base & ~1;
			uint vid_rb = vid_base | 1;

			MainVSOut lt = vs_main_run(load_vertex(vertices[vid_lt]), cb);
			MainVSOut rb = vs_main_run(load_vertex(vertices[vid_rb]), cb);
			MainVSOut out = rb;

			if (!is_right)
			{
				out.p.x = lt.p.x;
				out.t.x = lt.t.x;
				out.ti.xz = lt.ti.xz;
			}

			if (!is_bottom)
			{
				out.p.y = lt.p.y;
				out.t.y = lt.t.y;
				out.ti.yw = lt.ti.yw;
			}

			return out;
		}
	}
}

// MARK: - Fragment functions

struct PSMain
{
	texture2d<float> tex;
	depth2d<float> tex_depth;
	texture2d<float> palette;
	texture2d<float> prim_id_tex;
	sampler tex_sampler;
	float4 current_color;
	uint prim_id;
	const thread MainPSIn& in;
	constant GSMTLMainPSUniform& cb;

	PSMain(const thread MainPSIn& in, constant GSMTLMainPSUniform& cb): in(in), cb(cb) {}

	template <typename... Args>
	float4 sample_tex(Args... args)
	{
		if (PS_TEX_IS_DEPTH)
			return float4(tex_depth.sample(args...));
		else
			return tex.sample(args...);
	}

	float4 read_tex(uint2 pos, uint lod = 0)
	{
		if (PS_TEX_IS_DEPTH)
			return float4(tex_depth.read(pos, lod));
		else
			return tex.read(pos, lod);
	}

	float4 sample_c(float2 uv)
	{
		if (PS_TEX_IS_FB)
			return current_color;
		if (PS_REGION_RECT)
			return read_tex(uint2(uv));

		if (PS_POINT_SAMPLER)
		{
			// Weird issue with ATI/AMD cards,
			// it looks like they add 127/128 of a texel to sampling coordinates
			// occasionally causing point sampling to erroneously round up.
			// I'm manually adjusting coordinates to the centre of texels here,
			// though the centre is just paranoia, the top left corner works fine.
			// As of 2018 this issue is still present.
			uv = (trunc(uv * cb.wh.zw) + 0.5) / cb.wh.zw;
		}
		if (!PS_ADJS && !PS_ADJT)
		{
			uv *= cb.st_scale;
		}
		else
		{
			if (PS_ADJS)
				uv.x = (uv.x - cb.st_range.x) * cb.st_range.z;
			else
				uv.x = uv.x * cb.st_scale.x;
			if (PS_ADJT)
				uv.y = (uv.y - cb.st_range.y) * cb.st_range.w;
			else
				uv.y = uv.y * cb.st_scale.y;
		}

		if (PS_AUTOMATIC_LOD)
		{
			return sample_tex(tex_sampler, uv);
		}
		else if (PS_MANUAL_LOD)
		{
			float K = cb.uv_min_max.x;
			float L = cb.uv_min_max.y;
			float bias = cb.uv_min_max.z;
			float max_lod = cb.uv_min_max.w;

			float gs_lod = K - log2(abs(in.t.w)) * L;
			// FIXME max useful ?
			//float lod = max(min(gs_lod, max_lod) - bias, 0.f);
			float lod = min(gs_lod, max_lod) - bias;

			return sample_tex(tex_sampler, uv, level(lod));
		}
		else
		{
			return sample_tex(tex_sampler, uv, level(0));
		}
	}

	float4 sample_p(uint idx)
	{
		return palette.read(uint2(idx, 0));
	}

	float4 sample_p_norm(float u)
	{
		return sample_p(uint(u * 255.5f));
	}

	float4 clamp_wrap_uv(float4 uv)
	{
		float4 tex_size = cb.wh.xyxy;

		if (PS_WMS == PS_WMT)
		{
			if (PS_REGION_RECT && PS_WMS == 0)
			{
				uv = fract(uv);
			}
			else if (PS_REGION_RECT && PS_WMS == 1)
			{
				uv = saturate(uv);
			}
			else if (PS_WMS == 2)
			{
				uv = clamp(uv, cb.uv_min_max.xyxy, cb.uv_min_max.zwzw);
			}
			else if (PS_WMS == 3)
			{
				// wrap negative uv coords to avoid an off by one error that shifted
				// textures. Fixes Xenosaga's hair issue.
				if (!FST)
					uv = fract(uv);

				uv = float4((ushort4(uv * tex_size) & ushort4(cb.uv_msk_fix.xyxy)) | ushort4(cb.uv_msk_fix.zwzw)) / tex_size;
			}
		}
		else
		{
			if (PS_REGION_RECT && PS_WMS == 0)
			{
				uv.xz = fract(uv.xz);
			}
			else if (PS_REGION_RECT && PS_WMS == 1)
			{
				uv.xz = saturate(uv.xz);
			}
			else if (PS_WMS == 2)
			{
				uv.xz = clamp(uv.xz, cb.uv_min_max.xx, cb.uv_min_max.zz);
			}
			else if (PS_WMS == 3)
			{
				if (!FST)
					uv.xz = fract(uv.xz);

				uv.xz = float2((ushort2(uv.xz * tex_size.xx) & ushort2(cb.uv_msk_fix.xx)) | ushort2(cb.uv_msk_fix.zz)) / tex_size.xx;
			}

			if (PS_REGION_RECT && PS_WMT == 0)
			{
				uv.yw = fract(uv.yw);
			}
			else if (PS_REGION_RECT && PS_WMT == 1)
			{
				uv.yw = saturate(uv.yw);
			}
			else if (PS_WMT == 2)
			{
				uv.yw = clamp(uv.yw, cb.uv_min_max.yy, cb.uv_min_max.ww);
			}
			else if (PS_WMT == 3)
			{
				if (!FST)
					uv.yw = fract(uv.yw);

				uv.yw = float2((ushort2(uv.yw * tex_size.yy) & ushort2(cb.uv_msk_fix.yy)) | ushort2(cb.uv_msk_fix.ww)) / tex_size.yy;
			}
		}

		if (PS_REGION_RECT)
		{
			// Normalized -> Integer Coordinates.
			uv = clamp(uv * cb.wh.zwzw + cb.st_range.xyxy, cb.st_range.xyxy, cb.st_range.zwzw);
		}

		return uv;
	}

	float4x4 sample_4c(float4 uv)
	{
		return {
			sample_c(uv.xy),
			sample_c(uv.zy),
			sample_c(uv.xw),
			sample_c(uv.zw),
		};
	}

	uint4 sample_4_index(float4 uv)
	{
		float4 c;

		// Either GS will send a texture that contains a single alpha channel
		// Or we have an old RT (ie RGBA8) that contains index (4/8) in the alpha channel

		// Note: texture gather can't be used because of special clamping/wrapping
		// Also it doesn't support lod
		c.x = sample_c(uv.xy).a;
		c.y = sample_c(uv.zy).a;
		c.z = sample_c(uv.xw).a;
		c.w = sample_c(uv.zw).a;

		uint4 i = uint4(c * 255.5f); // Denormalize value

		if (PS_PAL_FMT == 1)
			return i & 0xF;
		if (PS_PAL_FMT == 2)
			return i >> 4;

		return i;
	}

	float4x4 sample_4p(uint4 u)
	{
		return {
			sample_p(u.x),
			sample_p(u.y),
			sample_p(u.z),
			sample_p(u.w),
		};
	}

	uint fetch_raw_depth()
	{
		return tex_depth.read(ushort2(in.p.xy)) * 0x1p32f;
	}

	float4 fetch_raw_color()
	{
		if (PS_TEX_IS_FB)
			return current_color;
		else
			return tex.read(ushort2(in.p.xy));
	}

	float4 fetch_c(ushort2 uv)
	{
		return PS_TEX_IS_DEPTH ? tex_depth.read(uv) : tex.read(uv);
	}

	// MARK: Depth sampling

	ushort2 clamp_wrap_uv_depth(ushort2 uv)
	{
		ushort2 uv_out = uv;
		// Keep the full precision
		// It allow to multiply the ScalingFactor before the 1/16 coeff
		ushort4 mask = ushort4(cb.uv_msk_fix) << 4;

		if (PS_WMS == PS_WMT)
		{
			if (PS_WMS == 2)
				uv_out = clamp(uv, mask.xy, mask.zw);
			else if (PS_WMS == 3)
				uv_out = (uv & mask.xy) | mask.zw;
		}
		else
		{
			if (PS_WMS == 2)
				uv_out.x = clamp(uv.x, mask.x, mask.z);
			else if (PS_WMS == 3)
				uv_out.x = (uv.x & mask.x) | mask.z;

			if (PS_WMT == 2)
				uv_out.y = clamp(uv.y, mask.y, mask.w);
			else if (PS_WMT == 3)
				uv_out.y = (uv.y & mask.y) | mask.w;
		}

		return uv_out;
	}

	float4 sample_depth(float2 st)
	{
		float2 uv_f = float2(clamp_wrap_uv_depth(ushort2(st))) * float2(cb.scale_factor.x);

		if (PS_REGION_RECT)
			uv_f = clamp(uv_f + cb.st_range.xy, cb.st_range.xy, cb.st_range.zw);

		ushort2 uv = ushort2(uv_f);
		float4 t = float4(0);

		if (PS_TALES_OF_ABYSS_HLE)
		{
			// Warning: UV can't be used in channel effect
			ushort depth = fetch_raw_depth();
			// Convert msb based on the palette
			t = palette.read(ushort2((depth >> 8) & 0xFF, 0)) * 255.f;
		}
		else if (PS_URBAN_CHAOS_HLE)
		{
			// Depth buffer is read as a RGB5A1 texture. The game try to extract the green channel.
			// So it will do a first channel trick to extract lsb, value is right-shifted.
			// Then a new channel trick to extract msb which will shifted to the left.
			// OpenGL uses a FLOAT32 format for the depth so it requires a couple of conversion.
			// To be faster both steps (msb&lsb) are done in a single pass.

			// Warning: UV can't be used in channel effect
			ushort depth = fetch_raw_depth();

			// Convert lsb based on the palette
			t = palette.read(ushort2(depth & 0xFF, 0)) * 255.f;

			// Msb is easier
			float green = float((depth >> 8) & 0xFF) * 36.f;
			green = min(green, 255.0f);

			t.g += green;
		}
		else if (PS_DEPTH_FMT == 1)
		{
			t = convert_depth32_rgba8(fetch_c(uv).r);
		}
		else if (PS_DEPTH_FMT == 2)
		{
			t = convert_depth16_rgba8(fetch_c(uv).r);
		}
		else if (PS_DEPTH_FMT == 3)
		{
			t = fetch_c(uv) * 255.f;
		}

		// macOS 10.15 ICE's on bool3(t.rgb), so use != 0 instead
		if (PS_AEM_FMT == FMT_24)
			t.a = (!PS_AEM || any(t.rgb != 0)) ? 255.f * cb.ta.x : 0.f;
		else if (PS_AEM_FMT == FMT_16)
			t.a = t.a >= 128.f ? 255.f * cb.ta.y : (!PS_AEM || any(t.rgb != 0)) ? 255.f * cb.ta.x : 0.f;

		return t;
	}

	// MARK: Fetch a Single Channel

	float4 fetch_red()
	{
		float rt = PS_TEX_IS_DEPTH ? float(fetch_raw_depth() & 0xFF) / 255.f : fetch_raw_color().r;
		return sample_p_norm(rt) * 255.f;
	}

	float4 fetch_green()
	{
		float rt = PS_TEX_IS_DEPTH ? float((fetch_raw_depth() >> 8) & 0xFF) / 255.f : fetch_raw_color().g;
		return sample_p_norm(rt) * 255.f;
	}

	float4 fetch_blue()
	{
		float rt = PS_TEX_IS_DEPTH ? float((fetch_raw_depth() >> 16) & 0xFF) / 255.f : fetch_raw_color().b;
		return sample_p_norm(rt) * 255.f;
	}

	float4 fetch_alpha()
	{
		return sample_p_norm(fetch_raw_color().a) * 255.f;
	}

	float4 fetch_rgb()
	{
		float4 rt = fetch_raw_color();
		return float4(sample_p_norm(rt.r).r, sample_p_norm(rt.g).g, sample_p_norm(rt.b).b, 1) * 255.f;
	}

	float4 fetch_gXbY()
	{
		if (PS_TEX_IS_DEPTH)
		{
			uint depth = fetch_raw_depth();
			uint bg = (depth >> (8 + cb.channel_shuffle.green_shift)) & 0xFF;
			return float4(bg);
		}
		else
		{
			uint4 rt = uint4(fetch_raw_color() * 255.5f);
			uint green = (rt.g >> cb.channel_shuffle.green_shift) & cb.channel_shuffle.green_mask;
			uint blue  = (rt.b >> cb.channel_shuffle.blue_shift)  & cb.channel_shuffle.blue_mask;
			return float4(green | blue);
		}
	}

	float4 sample_color(float2 st)
	{
		if (PS_TCOFFSETHACK)
			st += cb.tc_offset;

		float4 t;
		float4x4 c;
		float2 dd;

		if (!PS_LTF && PS_AEM_FMT == FMT_32 && PS_PAL_FMT == 0 && !PS_REGION_RECT && PS_WMS < 2 && PS_WMT < 2)
		{
			c[0] = sample_c(st);
		}
		else
		{
			float4 uv;
			if (PS_LTF)
			{
				uv = st.xyxy + cb.half_texel;
				dd = fract(uv.xy * cb.wh.zw);
				if (!FST)
				{
					// Background in Shin Megami Tensei Lucifers
					// I suspect that uv isn't a standard number, so fract is outside of the [0;1] range
					dd = saturate(dd);
				}
			}
			else
			{
				uv = st.xyxy;
			}

			uv = clamp_wrap_uv(uv);

			if (PS_PAL_FMT != 0)
				c = sample_4p(sample_4_index(uv));
			else
				c = sample_4c(uv);
		}

		for (int i = 0; i < 4; i++)
		{
			// macOS 10.15 ICE's on bool3(c[i].rgb), so use != 0 instead
			if (PS_AEM_FMT == FMT_24)
				c[i].a = !PS_AEM || any(c[i].rgb != 0) ? cb.ta.x : 0.f;
			else if (PS_AEM_FMT == FMT_16)
				c[i].a = c[i].a >= 0.5 ? cb.ta.y : !PS_AEM || any((int3(c[i].rgb * 255.0f) & 0xF8) != 0) ? cb.ta.x : 0.f;
		}

		if (PS_LTF)
			t = mix(mix(c[0], c[1], dd.x), mix(c[2], c[3], dd.x), dd.y);
		else
			t = c[0];

		// The 0.05f helps to fix the overbloom of sotc
		// I think the issue is related to the rounding of texture coodinate. The linear (from fixed unit)
		// interpolation could be slightly below the correct one.
		return trunc(t * 255.f + 0.05f);
	}

	float4 tfx(float4 T, float4 C)
	{
		float4 C_out;
		float4 FxT = trunc((C * T) / 128.f);
		if (PS_TFX == 0)
			C_out = FxT;
		else if (PS_TFX == 1)
			C_out = T;
		else if (PS_TFX == 2)
			C_out = float4(FxT.rgb, T.a) + C.a;
		else if (PS_TFX == 3)
			C_out = float4(FxT.rgb + C.a, T.a);
		else
			C_out = C;

		if (!PS_TCC)
			C_out.a = C.a;

		// Clamp only when it is useful
		if (PS_TFX == 0 || PS_TFX == 2 || PS_TFX == 3)
			C_out = min(C_out, 255.f);

		return C_out;
	}

	bool atst(float4 C)
	{
		float a = C.a;
		switch (PS_ATST)
		{
			case 0:
				break; // Nothing to do
			case 1:
				if (a > cb.aref)
					return false;
				break;
			case 2:
				if (a < cb.aref)
					return false;
				break;
			case 3:
				if (abs(a - cb.aref) > 0.5f)
					return false;
				break;
			case 4:
				if (abs(a - cb.aref) < 0.5f)
					return false;
				break;
		}
		return true;
	}

	void fog(thread float4& C, float f)
	{
		if (PS_FOG)
			C.rgb = trunc(mix(cb.fog_color, C.rgb, f));
	}

	float4 ps_color()
	{
		float2 st, st_int;
		if (!FST)
		{
			st = in.t.xy / in.t.w;
			st_int = in.ti.zw / in.t.w;
		}
		else
		{
			// Note: xy are normalized coordinates
			st = in.ti.xy;
			st_int = in.ti.zw;
		}

		float4 T;
		if (PS_CHANNEL == 1)
			T = fetch_red();
		else if (PS_CHANNEL == 2)
			T = fetch_green();
		else if (PS_CHANNEL == 3)
			T = fetch_blue();
		else if (PS_CHANNEL == 4)
			T = fetch_alpha();
		else if (PS_CHANNEL == 5)
			T = fetch_rgb();
		else if (PS_CHANNEL == 6)
			T = fetch_gXbY();
		else if (PS_DEPTH_FMT != 0)
			T = sample_depth(st_int);
		else
			T = sample_color(st);

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
	
		float4 C = tfx(T, IIP ? in.c : in.fc);
		if (!atst(C))
			discard_fragment();
		fog(C, in.t.z);

		return C;
	}

	void ps_fbmask(thread float4& C)
	{
		if (PS_FBMASK)
			C = float4((uint4(int4(C)) & (cb.fbmask ^ 0xff)) | (uint4(current_color * 255.5) & cb.fbmask));
	}

	void ps_dither(thread float4& C)
	{
		if (PS_DITHER == 0)
			return;
		ushort2 fpos;
		if (PS_DITHER == 2)
			fpos = ushort2(in.p.xy);
		else
			fpos = ushort2(in.p.xy * float2(cb.scale_factor.y));
		float value = cb.dither_matrix[fpos.y & 3][fpos.x & 3];;
		if (PS_ROUND_INV)
			C.rgb -= value;
		else
			C.rgb += value;
	}

	void ps_color_clamp_wrap(thread float4& C)
	{
		// When dithering the bottom 3 bits become meaningless and cause lines in the picture so we need to limit the color depth on dithered items
		if (!SW_BLEND && !PS_DITHER && !PS_FBMASK)
			return;

		if (PS_DST_FMT == FMT_16 && PS_BLEND_MIX == 0 && PS_ROUND_INV)
			C.rgb += 7.f; // Need to round up, not down since the shader will invert

		// Correct the Color value based on the output format
		if (!PS_COLCLIP && !PS_HDR)
			C.rgb = clamp(C.rgb, 0.f, 255.f); // Standard Clamp

		// FIXME rouding of negative float?
		// compiler uses trunc but it might need floor

		// Warning: normally blending equation is mult(A, B) = A * B >> 7. GPU have the full accuracy
		// GS: Color = 1, Alpha = 255 => output 1
		// GPU: Color = 1/255, Alpha = 255/255 * 255/128 => output 1.9921875
		if (PS_DST_FMT == FMT_16 && PS_BLEND_MIX == 0)
			// In 16 bits format, only 5 bits of colors are used. It impacts shadows computation of Castlevania
			C.rgb = float3(short3(C.rgb) & 0xF8);
		else if (PS_COLCLIP || PS_HDR)
			C.rgb = float3(short3(C.rgb) & 0xFF);
	}

	template <typename T>
	static T pick(uint selector, T zero, T one, T two)
	{
		return selector == 0 ? zero : selector == 1 ? one : two;
	}

	void ps_blend(thread float4& Color, thread float4& As_rgba)
	{
		float As = As_rgba.a;
		
		if (SW_BLEND)
		{
			// PABE
			if (PS_PABE)
			{
				// No blending so early exit
				if (As < 1.f)
					return;
			}

			float Ad = trunc(current_color.a * 255.5f) / 128.f;

			float3 Cd = trunc(current_color.rgb * 255.5f);
			float3 Cs = Color.rgb;

			float3 A = pick(PS_BLEND_A, Cs, Cd, float3(0.f));
			float3 B = pick(PS_BLEND_B, Cs, Cd, float3(0.f));
			float  C = pick(PS_BLEND_C, As, Ad, cb.alpha_fix);
			float3 D = pick(PS_BLEND_D, Cs, Cd, float3(0.f));

			// As/Af clamp alpha for Blend mix
			// We shouldn't clamp blend mix with blend hw 1 as we want alpha higher
			float C_clamped = C;
			if (PS_BLEND_MIX > 0 && PS_BLEND_HW != 1)
				C_clamped = min(C_clamped, 1.f);

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
				Color.rgb = ((A - B) * C_clamped + D) + (124.f/256.f);
			else if (PS_BLEND_MIX == 1)
				Color.rgb = ((A - B) * C_clamped + D) - (124.f/256.f);
			else
				Color.rgb = trunc((A - B) * C + D);

			if (PS_BLEND_HW == 1)
			{
				// As or Af
				As_rgba.rgb = float3(C);
				// Subtract 1 for alpha to compensate for the changed equation,
				// if c.rgb > 255.0f then we further need to adjust alpha accordingly,
				// we pick the lowest overflow from all colors because it's the safest,
				// we divide by 255 the color because we don't know Cd value,
				// changed alpha should only be done for hw blend.
				float3 alpha_compensate = max(float3(1.f), Color.rgb / float3(255.f));
				As_rgba.rgb -= alpha_compensate;
			}
			else if (PS_BLEND_HW == 2)
			{
				// Compensate slightly for Cd*(As + 1) - Cs*As.
				// The initial factor we chose is 1 (0.00392)
				// as that is the minimum color Cd can be,
				// then we multiply by alpha to get the minimum
				// blended value it can be.
				float color_compensate = 1.f * (C + 1.f);
				Color.rgb -= float3(color_compensate);
			}
			else if (PS_BLEND_HW == 3)
			{
				// As, Ad or Af clamped.
				As_rgba.rgb = float3(C_clamped);
				// Cs*(Alpha + 1) might overflow, if it does then adjust alpha value
				// that is sent on second output to compensate.
				float3 overflow_check = (Color.rgb - float3(255.f)) / 255.f;
				float3 alpha_compensate = max(float3(0.f), overflow_check);
				As_rgba.rgb -= alpha_compensate;
			}
		}
		else
		{
			// Needed for Cd * (As/Ad/F + 1) blending mdoes
			if (PS_BLEND_HW == 1)
			{
				Color.rgb = 255.f;
			}
			else if (PS_BLEND_HW == 2)
			{
				float Alpha = PS_BLEND_C == 2 ? cb.alpha_fix : As;
				Color.rgb = saturate(Alpha - 1.f) * 255.f;
			}
			else if (PS_BLEND_HW == 3)
			{
				// Needed for Cs*Ad, Cs*Ad + Cd, Cd - Cs*Ad
				// Multiply Color.rgb by (255/128) to compensate for wrong Ad/255 value when rgb are below 128.
				// When any color channel is higher than 128 then adjust the compensation automatically
				// to give us more accurate colors, otherwise they will be wrong.
				// The higher the value (>128) the lower the compensation will be.
				float max_color = max(max(Color.r, Color.g), Color.b);
				float color_compensate = 255.f / max(128.f, max_color);
				Color.rgb *= float3(color_compensate);
			}
		}
	}

	MainPSOut ps_main()
	{
		MainPSOut out = {};

		if (PS_SCANMSK & 2)
		{
			if ((uint(in.p.y) & 1) == (PS_SCANMSK & 1))
				discard_fragment();
		}

		if (PS_DATE >= 5)
		{
			// 1 => DATM == 0, 2 => DATM == 1
			float rt_a = PS_WRITE_RG ? current_color.g : current_color.a;
			bool bad = (PS_DATE & 3) == 1 ? (rt_a > 0.5) : (rt_a < 0.5);

			if (bad)
				discard_fragment();
		}

		if (PS_DATE == 3)
		{
			float stencil_ceil = prim_id_tex.read(uint2(in.p.xy)).r;
			// Note prim_id == stencil_ceil will be the primitive that will update
			// the bad alpha value so we must keep it.
			if (float(prim_id) > stencil_ceil)
				discard_fragment();
		}

		float4 C = ps_color();

		// Must be done before alpha correction

		// AA (Fixed one) will output a coverage of 1.0 as alpha
		if (PS_FIXED_ONE_A)
		{
			C.a = 128.0f;
		}

		float4 alpha_blend = SW_AD_TO_HW ? float4(trunc(current_color.a * 255.5f) / 128.f) : float4(C.a / 128.f);

		if (PS_DST_FMT == FMT_16)
		{
			float A_one = 128.f;
			C.a = (PS_FBA) ? A_one : step(128.f, C.a) * A_one;
		}
		else if (PS_DST_FMT == FMT_32 && PS_FBA)
		{
			if (C.a < 128.f)
				C.a += 128.f;
		}

		// Get first primitive that will write a failing alpha value
		if (PS_DATE == 1)
		{
			// DATM == 0, Pixel with alpha equal to 1 will failed (128-255)
			out.c0 = C.a > 127.5f ? float(prim_id) : FLT_MAX;
			return out;
		}
		else if (PS_DATE == 2)
		{
			// DATM == 1, Pixel with alpha equal to 0 will failed (0-127)
			out.c0 = C.a < 127.5f ? float(prim_id) : FLT_MAX;
			return out;
		}

		ps_blend(C, alpha_blend);

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
			uint2 denorm_TA = uint2(cb.ta * 255.5f);

			// Special case for 32bit input and 16bit output, shuffle used by The Godfather
			if (PS_SHUFFLE_SAME)
			{
				if (PS_READ_BA)
					C = (denorm_c.b & 0x7Fu) | (denorm_c.a & 0x80);
				else
					C.ga = C.rg;
			}
			// Copy of a 16bit source in to this target
			else if (PS_READ16_SRC)
			{
				C.rb = (denorm_c.r >> 3) | (((denorm_c.g >> 3) & 0x7u) << 5);
				if (denorm_c.a & 0x80)
					C.ga = (denorm_c.g >> 6) | ((denorm_c.b >> 3) << 2) | (denorm_TA.y & 0x80);
				else
					C.ga = (denorm_c.g >> 6) | ((denorm_c.b >> 3) << 2) | (denorm_TA.x & 0x80);
			}
			// Write RB part. Mask will take care of the correct destination
			else if (PS_READ_BA)
			{
				C.rb = C.bb;	
				C.ga = (denorm_c.a & 0x7F) | (denorm_c.a & 0x80 ? denorm_TA.y & 0x80 : denorm_TA.x & 0x80);
			}
			else
			{
				C.rb = C.rr;
				C.ga = (denorm_c.g & 0x7F) | (denorm_c.g & 0x80 ? denorm_TA.y & 0x80 : denorm_TA.x & 0x80);
			}
		}
		
		ps_dither(C);

		// Color clamp/wrap needs to be done after sw blending and dithering
		ps_color_clamp_wrap(C);

		ps_fbmask(C);

		if (PS_COLOR0)
			out.c0 = PS_HDR ? float4(C.rgb / 65535.f, C.a / 255.f) : C / 255.f;
		if (PS_COLOR0 && PS_ONLY_ALPHA)
			out.c0.rgb = 0;
		if (PS_COLOR1)
			out.c1 = alpha_blend;
		if (PS_ZCLAMP)
			out.depth = min(in.p.z, cb.max_depth);

		return out;
	}
};

#if FBFETCH_SUPPORT
fragment float4 fbfetch_test(float4 in [[color(0), raster_order_group(0)]])
{
	return in * 2;
}

constant bool NEEDS_RT_TEX = NEEDS_RT && !HAS_FBFETCH;
constant bool NEEDS_RT_FBF = NEEDS_RT &&  HAS_FBFETCH;
#else
constant bool NEEDS_RT_TEX = NEEDS_RT;
#endif

fragment MainPSOut ps_main(
	MainPSIn in [[stage_in]],
	constant GSMTLMainPSUniform& cb [[buffer(GSMTLBufferIndexHWUniforms)]],
	sampler s [[sampler(0)]],
#if PRIMID_SUPPORT
	uint primid [[primitive_id, function_constant(NEEDS_PRIMID)]],
#endif
#if FBFETCH_SUPPORT
	float4 rt_fbf [[color(0), raster_order_group(0), function_constant(NEEDS_RT_FBF)]],
#endif
	texture2d<float> tex       [[texture(GSMTLTextureIndexTex),          function_constant(PS_TEX_IS_COLOR)]],
	depth2d<float>   depth     [[texture(GSMTLTextureIndexTex),          function_constant(PS_TEX_IS_DEPTH)]],
	texture2d<float> palette   [[texture(GSMTLTextureIndexPalette),      function_constant(PS_HAS_PALETTE)]],
	texture2d<float> rt        [[texture(GSMTLTextureIndexRenderTarget), function_constant(NEEDS_RT_TEX)]],
	texture2d<float> primidtex [[texture(GSMTLTextureIndexPrimIDs),      function_constant(PS_PRIM_CHECKING_READ)]])
{
	PSMain main(in, cb);
	main.tex_sampler = s;
	if (PS_TEX_IS_COLOR)
		main.tex = tex;
	else
		main.tex_depth = depth;
	if (PS_HAS_PALETTE)
		main.palette = palette;
	if (PS_PRIM_CHECKING_READ)
		main.prim_id_tex = primidtex;
#if PRIMID_SUPPORT
	if (NEEDS_PRIMID)
		main.prim_id = primid;
#endif

	if (NEEDS_RT)
	{
#if FBFETCH_SUPPORT
		main.current_color = HAS_FBFETCH ? rt_fbf : rt.read(uint2(in.p.xy));
#else
		main.current_color = rt.read(uint2(in.p.xy));
#endif
	}
	else
	{
		main.current_color = 0;
	}

	return main.ps_main();
}

#if PRIMID_SUPPORT
fragment uint primid_test(uint id [[primitive_id]])
{
	return id;
}
#endif

// MARK: Markers for detecting the Metal version a metallib was compiled against

#if __METAL_VERSION__ >= 210
kernel void metal_version_21() {}
#endif
#if __METAL_VERSION__ >= 220
kernel void metal_version_22() {}
#endif
#if __METAL_VERSION__ >= 230
kernel void metal_version_23() {}
#endif
