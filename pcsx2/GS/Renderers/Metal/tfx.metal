/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "GSMTLShaderCommon.h"

constant uint FMT_32 = 0;
constant uint FMT_24 = 1;
constant uint FMT_16 = 2;

constant bool HAS_FBFETCH           [[function_constant(GSMTLConstantIndex_FRAMEBUFFER_FETCH)]];
constant bool FST                   [[function_constant(GSMTLConstantIndex_FST)]];
constant bool IIP                   [[function_constant(GSMTLConstantIndex_IIP)]];
constant bool VS_POINT_SIZE         [[function_constant(GSMTLConstantIndex_VS_POINT_SIZE)]];
constant uint PS_AEM_FMT            [[function_constant(GSMTLConstantIndex_PS_AEM_FMT)]];
constant uint PS_PAL_FMT            [[function_constant(GSMTLConstantIndex_PS_PAL_FMT)]];
constant uint PS_DFMT               [[function_constant(GSMTLConstantIndex_PS_DFMT)]];
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
constant bool PS_LTF                [[function_constant(GSMTLConstantIndex_PS_LTF)]];
constant bool PS_SHUFFLE            [[function_constant(GSMTLConstantIndex_PS_SHUFFLE)]];
constant bool PS_READ_BA            [[function_constant(GSMTLConstantIndex_PS_READ_BA)]];
constant bool PS_WRITE_RG           [[function_constant(GSMTLConstantIndex_PS_WRITE_RG)]];
constant bool PS_FBMASK             [[function_constant(GSMTLConstantIndex_PS_FBMASK)]];
constant uint PS_BLEND_A            [[function_constant(GSMTLConstantIndex_PS_BLEND_A)]];
constant uint PS_BLEND_B            [[function_constant(GSMTLConstantIndex_PS_BLEND_B)]];
constant uint PS_BLEND_C            [[function_constant(GSMTLConstantIndex_PS_BLEND_C)]];
constant uint PS_BLEND_D            [[function_constant(GSMTLConstantIndex_PS_BLEND_D)]];
constant uint PS_CLR_HW             [[function_constant(GSMTLConstantIndex_PS_CLR_HW)]];
constant bool PS_HDR                [[function_constant(GSMTLConstantIndex_PS_HDR)]];
constant bool PS_COLCLIP            [[function_constant(GSMTLConstantIndex_PS_COLCLIP)]];
constant bool PS_BLEND_MIX          [[function_constant(GSMTLConstantIndex_PS_BLEND_MIX)]];
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
constant bool PS_INVALID_TEX0       [[function_constant(GSMTLConstantIndex_PS_INVALID_TEX0)]];
constant uint PS_SCANMSK            [[function_constant(GSMTLConstantIndex_PS_SCANMSK)]];

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
constant bool SW_AD_TO_HW = PS_BLEND_C == 1 && PS_CLR_HW > 3;
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
		out.point_size = SCALING_FACTOR.x;

	return out;
}

vertex MainVSOut vs_main(MainVSIn v [[stage_in]], constant GSMTLMainVSUniform& cb [[buffer(GSMTLBufferIndexHWUniforms)]])
{
	return vs_main_run(v, cb);
}

// MARK: - Fragment functions

constexpr sampler palette_sampler(filter::nearest, address::clamp_to_edge);

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

	float4 sample_c(float2 uv)
	{
		if (PS_TEX_IS_FB)
			return current_color;

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
		uv *= cb.st_scale;

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

	float4 sample_p(float idx)
	{
		return palette.sample(palette_sampler, float2(idx, 0));
	}

	float4 clamp_wrap_uv(float4 uv)
	{
		float4 uv_out = uv;
		float4 tex_size = PS_INVALID_TEX0 ? cb.wh.zwzw : cb.wh.xyxy;

		if (PS_WMS == PS_WMT)
		{
			if (PS_WMS == 2)
			{
				uv_out = clamp(uv, cb.uv_min_max.xyxy, cb.uv_min_max.zwzw);
			}
			else if (PS_WMS == 3)
			{
				// wrap negative uv coords to avoid an off by one error that shifted
				// textures. Fixes Xenosaga's hair issue.
				if (!FST)
					uv = fract(uv);

				uv_out = float4((ushort4(uv * tex_size) & ushort4(cb.uv_msk_fix.xyxy)) | ushort4(cb.uv_msk_fix.zwzw)) / tex_size;
			}
		}
		else
		{
			if (PS_WMS == 2)
			{
				uv_out.xz = clamp(uv.xz, cb.uv_min_max.xx, cb.uv_min_max.zz);
			}
			else if (PS_WMS == 3)
			{
				if (!FST)
					uv.xz = fract(uv.xz);

				uv_out.xz = float2((ushort2(uv.xz * tex_size.xx) & ushort2(cb.uv_msk_fix.xx)) | ushort2(cb.uv_msk_fix.zz)) / tex_size.xx;
			}

			if (PS_WMT == 2)
			{
				uv_out.yw = clamp(uv.yw, cb.uv_min_max.yy, cb.uv_min_max.ww);
			}
			else if (PS_WMT == 3)
			{
				if (!FST)
					uv.yw = fract(uv.yw);

				uv_out.yw = float2((ushort2(uv.yw * tex_size.yy) & ushort2(cb.uv_msk_fix.yy)) | ushort2(cb.uv_msk_fix.ww)) / tex_size.yy;
			}
		}

		return uv_out;
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

	float4 sample_4_index(float4 uv)
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
			return float4(i & 0xF) / 255.f;
		if (PS_PAL_FMT == 2)
			return float4(i >> 4) / 255.f;

		// Most textures will hit this code so keep normalized float value
		return c;
	}

	float4x4 sample_4p(float4 u)
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
		float2 uv_f = float2(clamp_wrap_uv_depth(ushort2(st))) * (float2(SCALING_FACTOR) * float2(1.f / 16.f));
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

		if (PS_AEM_FMT == FMT_24)
			t.a = (!PS_AEM || any(bool3(t.rgb))) ? 255.f * cb.ta.x : 0.f;
		else if (PS_AEM_FMT == FMT_16)
			t.a = t.a >= 128.f ? 255.f * cb.ta.y : (!PS_AEM || any(bool3(t.rgb))) ? 255.f * cb.ta.x : 0.f;

		return t;
	}

	// MARK: Fetch a Single Channel

	float4 fetch_red()
	{
		float rt = PS_TEX_IS_DEPTH ? float(fetch_raw_depth() & 0xFF) / 255.f : fetch_raw_color().r;
		return sample_p(rt) * 255.f;
	}

	float4 fetch_green()
	{
		float rt = PS_TEX_IS_DEPTH ? float((fetch_raw_depth() >> 8) & 0xFF) / 255.f : fetch_raw_color().g;
		return sample_p(rt) * 255.f;
	}

	float4 fetch_blue()
	{
		float rt = PS_TEX_IS_DEPTH ? float((fetch_raw_depth() >> 16) & 0xFF) / 255.f : fetch_raw_color().b;
		return sample_p(rt) * 255.f;
	}

	float4 fetch_alpha()
	{
		return sample_p(fetch_raw_color().a) * 255.f;
	}

	float4 fetch_rgb()
	{
		float4 rt = fetch_raw_color();
		return float4(sample_p(rt.r).r, sample_p(rt.g).g, sample_p(rt.b).b, 1) * 255.f;
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

		if (!PS_LTF && PS_AEM_FMT == FMT_32 && PS_PAL_FMT == 0 && PS_WMS < 2 && PS_WMT < 2)
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
			if (PS_AEM_FMT == FMT_24)
				c[i].a = !PS_AEM || any(bool3(c[i].rgb)) ? cb.ta.x : 0.f;
			else if (PS_AEM_FMT == FMT_16)
				c[i].a = c[i].a >= 0.5 ? cb.ta.y : !PS_AEM || any(bool3(c[i].rgb)) ? cb.ta.x : 0.f;
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
		float4 FxT = trunc(trunc(C) * T / 128.f);
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
		if (!FST && PS_INVALID_TEX0)
		{
			st = (in.t.xy * cb.wh.xy) / (in.t.w * cb.wh.zw);
		}
		else if (!FST)
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

		float4 C = tfx(T, IIP ? in.c : in.fc);
		if (!atst(C))
			discard_fragment();
		fog(C, in.t.z);

		return C;
	}

	void ps_fbmask(thread float4& C)
	{
		if (PS_FBMASK)
			C = float4((uint4(C) & ~cb.fbmask) | (uint4(current_color * 255.5) & cb.fbmask));
	}

	void ps_dither(thread float4& C)
	{
		if (PS_DITHER == 0)
			return;
		ushort2 fpos;
		if (PS_DITHER == 2)
			fpos = ushort2(in.p.xy);
		else
			fpos = ushort2(in.p.xy / float2(SCALING_FACTOR));
		C.rgb += cb.dither_matrix[fpos.y & 3][fpos.x & 3];
	}

	void ps_color_clamp_wrap(thread float4& C)
	{
		// When dithering the bottom 3 bits become meaningless and cause lines in the picture so we need to limit the color depth on dithered items
		if (!SW_BLEND && !PS_DITHER)
			return;

		// Correct the Color value based on the output format
		if (!PS_COLCLIP && !PS_HDR)
			C.rgb = clamp(C.rgb, 0.f, 255.f); // Standard Clamp

		// FIXME rouding of negative float?
		// compiler uses trunc but it might need floor

		// Warning: normally blending equation is mult(A, B) = A * B >> 7. GPU have the full accuracy
		// GS: Color = 1, Alpha = 255 => output 1
		// GPU: Color = 1/255, Alpha = 255/255 * 255/128 => output 1.9921875
		if (PS_DFMT == FMT_16 && (PS_HDR || !PS_BLEND_MIX))
			// In 16 bits format, only 5 bits of colors are used. It impacts shadows computation of Castlevania
			C.rgb = float3(short3(C.rgb) & 0xF8);
		else if (PS_COLCLIP && !PS_HDR)
			C.rgb = float3(short3(C.rgb) & 0xFF);
	}

	template <typename T>
	static T pick(uint selector, T zero, T one, T two)
	{
		return selector == 0 ? zero : selector == 1 ? one : two;
	}

	void ps_blend(thread float4& Color, float As)
	{
		if (SW_BLEND)
		{

			float Ad = PS_DFMT == FMT_24 ? 1.f : trunc(current_color.a * 255.5f) / 128.f;

			float3 Cd = trunc(current_color.rgb * 255.5f);
			float3 Cs = Color.rgb;

			float3 A = pick(PS_BLEND_A, Cs, Cd, float3(0.f));
			float3 B = pick(PS_BLEND_B, Cs, Cd, float3(0.f));
			float  C = pick(PS_BLEND_C, As, Ad, cb.alpha_fix);
			float3 D = pick(PS_BLEND_D, Cs, Cd, float3(0.f));

			if (PS_BLEND_MIX)
				C = min(C, 1.f);

			if (PS_BLEND_A == PS_BLEND_B)
				Color.rgb = D;
			else
				Color.rgb = trunc((A - B) * C + D);

			if (PS_PABE)
				Color.rgb = (As >= 1.f) ? Color.rgb : Cs;
		}
		else
		{
			// Needed for Cd * (As/Ad/F + 1) blending mdoes
			if (PS_CLR_HW == 1 || PS_CLR_HW == 5)
			{
				Color.rgb = 255.f;
			}
			else if (PS_CLR_HW == 2 || PS_CLR_HW == 4)
			{
				float Alpha = PS_BLEND_C == 2 ? cb.alpha_fix : As;
				Color.rgb = saturate(Alpha - 1.f) * 255.f;
			}
			else if (PS_CLR_HW == 3)
			{
				// Needed for Cs*Ad, Cs*Ad + Cd, Cd - Cs*Ad
				// Multiply Color.rgb by (255/128) to compensate for wrong Ad/255 value
				Color.rgb *= (255.f / 128.f);
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

		if (PS_SHUFFLE)
		{
			uint4 denorm_c = uint4(C);
			uint2 denorm_TA = uint2(cb.ta * 255.5f);

			C.rb = PS_READ_BA ? C.bb : C.rr;
			if (PS_READ_BA)
				C.ga = (denorm_c.a & 0x7F) | (denorm_c.a & 0x80 ? denorm_TA.y & 0x80 : denorm_TA.x & 0x80);
			else
				C.ga = (denorm_c.g & 0x7F) | (denorm_c.g & 0x80 ? denorm_TA.y & 0x80 : denorm_TA.x & 0x80);
		}

		// Must be done before alpha correction
		float alpha_blend = SW_AD_TO_HW ? (PS_DFMT == FMT_24 ? 1.f : trunc(current_color.a * 255.5f) / 128.f) : (C.a / 128.f);

		if (PS_DFMT == FMT_16)
		{
			float A_one = 128.f;
			C.a = (PS_FBA) ? A_one : step(128.f, C.a) * A_one;
		}
		else if (PS_DFMT == FMT_32 && PS_FBA)
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

		ps_dither(C);

		// Color clamp/wrap needs to be done after sw blending and dithering
		ps_color_clamp_wrap(C);

		ps_fbmask(C);

		if (PS_COLOR0)
			out.c0 = C / 255.f;
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
