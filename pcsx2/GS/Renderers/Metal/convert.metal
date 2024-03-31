// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GSMTLShaderCommon.h"

using namespace metal;

struct ConvertVSIn
{
	vector_float2 position  [[attribute(0)]];
	vector_float2 texcoord0 [[attribute(1)]];
};

struct ImGuiVSIn
{
	vector_float2 position  [[attribute(0)]];
	vector_float2 texcoord0 [[attribute(1)]];
	vector_half4  color     [[attribute(2)]];
};

struct ImGuiShaderData
{
	float4 p [[position]];
	float2 t;
	half4  c;
};

template <typename Format>
struct DirectReadTextureIn
{
	texture2d<Format> tex [[texture(GSMTLTextureIndexNonHW)]];
	vec<Format, 4> read(float4 pos)
	{
		return tex.read(uint2(pos.xy));
	}
};

vertex ConvertShaderData fs_triangle(uint vid [[vertex_id]])
{
	ConvertShaderData out;
	out.p = float4(vid & 1 ? 3 : -1, vid & 2 ? 3 : -1, 0, 1);
	out.t = float2(vid & 1 ? 2 : 0, vid & 2 ? -1 : 1);
	return out;
}

vertex ConvertShaderData vs_convert(ConvertVSIn in [[stage_in]])
{
	ConvertShaderData out;
	out.p = float4(in.position, 0, 1);
	out.t = in.texcoord0;
	return out;
}

vertex ImGuiShaderData vs_imgui(ImGuiVSIn in [[stage_in]], constant float4& cb [[buffer(GSMTLBufferIndexUniforms)]])
{
	ImGuiShaderData out;
	out.p = float4(in.position * cb.xy + cb.zw, 0, 1);
	out.t = in.texcoord0;
	out.c = in.color;
	return out;
}

fragment float4 ps_copy(ConvertShaderData data [[stage_in]], ConvertPSRes res)
{
	return res.sample(data.t);
}

fragment ushort ps_convert_rgba8_16bits(ConvertShaderData data [[stage_in]], ConvertPSRes res)
{
	float4 c = res.sample(data.t);
	uint4 cu = uint4(c * 255.f + 0.5f);
	return (cu.x >> 3) | ((cu.y << 2) & 0x03e0) | ((cu.z << 7) & 0x7c00) | ((cu.w << 8) & 0x8000);
}

fragment float4 ps_copy_fs(float4 p [[position]], DirectReadTextureIn<float> tex)
{
	return tex.read(p);
}

fragment float4 ps_clear(float4 p [[position]], constant float4& color [[buffer(GSMTLBufferIndexUniforms)]])
{
	return color;
}

fragment void ps_datm1(float4 p [[position]], DirectReadTextureIn<float> tex)
{
	if (tex.read(p).a < (127.5f / 255.f))
		discard_fragment();
}

fragment void ps_datm0_rta_correction(float4 p [[position]], DirectReadTextureIn<float> tex)
{
	if (tex.read(p).a > (254.5f / 255.f))
		discard_fragment();
}

fragment void ps_datm1_rta_correction(float4 p [[position]], DirectReadTextureIn<float> tex)
{
	if (tex.read(p).a < (254.5f / 255.f))
		discard_fragment();
}

fragment void ps_datm0(float4 p [[position]], DirectReadTextureIn<float> tex)
{
	if (tex.read(p).a > (127.5f / 255.f))
		discard_fragment();
}

fragment float4 ps_primid_init_datm1(float4 p [[position]], DirectReadTextureIn<float> tex)
{
	return tex.read(p).a < (127.5f / 255.f) ? -1 : FLT_MAX;
}

fragment float4 ps_primid_init_datm0(float4 p [[position]], DirectReadTextureIn<float> tex)
{
	return tex.read(p).a > (127.5f / 255.f) ? -1 : FLT_MAX;
}

fragment float4 ps_primid_rta_init_datm1(float4 p [[position]], DirectReadTextureIn<float> tex)
{
	return tex.read(p).a < (254.5f / 255.f) ? -1 : FLT_MAX;
}

fragment float4 ps_primid_rta_init_datm0(float4 p [[position]], DirectReadTextureIn<float> tex)
{
	return tex.read(p).a > (254.5f / 255.f) ? -1 : FLT_MAX;
}

fragment float4 ps_rta_correction(ConvertShaderData data [[stage_in]], ConvertPSRes res)
{
	float4 in = res.sample(data.t);
	return float4(in.rgb, in.a / (128.25f / 255.0f));
}

fragment float4 ps_rta_decorrection(ConvertShaderData data [[stage_in]], ConvertPSRes res)
{
	float4 in = res.sample(data.t);
	return float4(in.rgb, in.a * (128.25f / 255.0f));
}

fragment float4 ps_hdr_init(float4 p [[position]], DirectReadTextureIn<float> tex)
{
	float4 in = tex.read(p);
	return float4(round(in.rgb * 255.f) / 65535.f, in.a);
}

fragment float4 ps_hdr_resolve(float4 p [[position]], DirectReadTextureIn<float> tex)
{
	float4 in = tex.read(p);
	return float4(float3(uint3(in.rgb * 65535.5f) & 255) / 255.f, in.a);
}

fragment float4 ps_filter_transparency(ConvertShaderData data [[stage_in]], ConvertPSRes res)
{
	float4 c = res.sample(data.t);
	return float4(c.rgb, 1.0);
}

fragment uint ps_convert_float32_32bits(ConvertShaderData data [[stage_in]], ConvertPSDepthRes res)
{
	return uint(0x1p32 * res.sample(data.t));
}

fragment float4 ps_convert_float32_rgba8(ConvertShaderData data [[stage_in]], ConvertPSDepthRes res)
{
	return convert_depth32_rgba8(res.sample(data.t)) / 255.f;
}

fragment float4 ps_convert_float16_rgb5a1(ConvertShaderData data [[stage_in]], ConvertPSDepthRes res)
{
	return convert_depth16_rgba8(res.sample(data.t)) / 255.f;
}

struct DepthOut
{
	float depth [[depth(any)]];
	DepthOut(float depth): depth(depth) {}
};

fragment DepthOut ps_depth_copy(ConvertShaderData data [[stage_in]], ConvertPSDepthRes res)
{
	return res.sample(data.t);
}

static float rgba8_to_depth32(half4 unorm)
{
	return float(as_type<uint>(uchar4(unorm * 255.5h))) * 0x1p-32f;
}

static float rgba8_to_depth24(half4 unorm)
{
	return rgba8_to_depth32(half4(unorm.rgb, 0));
}

static float rgba8_to_depth16(half4 unorm)
{
	return float(as_type<ushort>(uchar2(unorm.rg * 255.5h))) * 0x1p-32f;
}

static float rgb5a1_to_depth16(half4 unorm)
{
	uint4 cu = uint4(unorm * 255.5h);
	uint out = (cu.x >> 3) | ((cu.y << 2) & 0x03e0) | ((cu.z << 7) & 0x7c00) | ((cu.w << 8) & 0x8000);
	return float(out) * 0x1p-32f;
}

struct ConvertToDepthRes
{
	texture2d<half> texture [[texture(GSMTLTextureIndexNonHW)]];
	half4 sample(float2 coord)
	{
		// RGBA bilinear on a depth texture is a bad idea, and should never be used
		// Might as well let the compiler optimize a bit by telling it exactly what sampler we'll be using here
		constexpr sampler s(coord::normalized, filter::nearest, address::clamp_to_edge);
		return texture.sample(s, coord);
	}

	/// Manual bilinear sampling where we do the bilinear *after* rgba → depth conversion
	template <float (&convert)(half4)>
	float sample_biln(float2 coord)
	{
		uint2 dimensions = uint2(texture.get_width(), texture.get_height());
		float2 top_left_f = coord * float2(dimensions) - 0.5f;
		int2 top_left = int2(floor(top_left_f));
		uint4 coords = uint4(clamp(int4(top_left, top_left + 1), 0, int2(dimensions - 1).xyxy));
		float2 mix_vals = fract(top_left_f);

		float depthTL = convert(texture.read(coords.xy));
		float depthTR = convert(texture.read(coords.zy));
		float depthBL = convert(texture.read(coords.xw));
		float depthBR = convert(texture.read(coords.zw));
		return mix(mix(depthTL, depthTR, mix_vals.x), mix(depthBL, depthBR, mix_vals.x), mix_vals.y);
	}
};

fragment DepthOut ps_convert_rgba8_float32(ConvertShaderData data [[stage_in]], ConvertToDepthRes res)
{
	return rgba8_to_depth32(res.sample(data.t));
}

fragment DepthOut ps_convert_rgba8_float24(ConvertShaderData data [[stage_in]], ConvertToDepthRes res)
{
	return rgba8_to_depth24(res.sample(data.t));
}

fragment DepthOut ps_convert_rgba8_float16(ConvertShaderData data [[stage_in]], ConvertToDepthRes res)
{
	return rgba8_to_depth16(res.sample(data.t));
}

fragment DepthOut ps_convert_rgb5a1_float16(ConvertShaderData data [[stage_in]], ConvertToDepthRes res)
{
	return rgb5a1_to_depth16(res.sample(data.t));
}

fragment DepthOut ps_convert_rgba8_float32_biln(ConvertShaderData data [[stage_in]], ConvertToDepthRes res)
{
	return res.sample_biln<rgba8_to_depth32>(data.t);
}

fragment DepthOut ps_convert_rgba8_float24_biln(ConvertShaderData data [[stage_in]], ConvertToDepthRes res)
{
	return res.sample_biln<rgba8_to_depth24>(data.t);
}

fragment DepthOut ps_convert_rgba8_float16_biln(ConvertShaderData data [[stage_in]], ConvertToDepthRes res)
{
	return res.sample_biln<rgba8_to_depth16>(data.t);
}

fragment DepthOut ps_convert_rgb5a1_float16_biln(ConvertShaderData data [[stage_in]], ConvertToDepthRes res)
{
	return res.sample_biln<rgb5a1_to_depth16>(data.t);
}

fragment float4 ps_convert_rgba_8i(ConvertShaderData data [[stage_in]], DirectReadTextureIn<float> res,
	constant GSMTLIndexedConvertPSUniform& uniform [[buffer(GSMTLBufferIndexUniforms)]])
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
	uint2 pos = uint2(data.p.xy);

	// Collapse separate R G B A areas into their base pixel
	uint2 block = (pos & ~uint2(15, 3)) >> 1;
	uint2 subblock = pos & uint2(7, 1);
	uint2 coord = block | subblock;

	// Compensate for potentially differing page pitch.
	uint2 block_xy = coord / uint2(64, 32);
	uint block_num = (block_xy.y * (uniform.dbw / 128)) + block_xy.x;
	uint2 block_offset = uint2((block_num % (uniform.sbw / 64)) * 64, (block_num / (uniform.sbw / 64)) * 32);
	coord = (coord % uint2(64, 32)) + block_offset;

	// Apply offset to cols 1 and 2
	uint is_col23 = pos.y & 4;
	uint is_col13 = pos.y & 2;
	uint is_col12 = is_col23 ^ (is_col13 << 1);
	coord.x ^= is_col12; // If cols 1 or 2, flip bit 3 of x

	if (any(floor(uniform.scale) != uniform.scale))
		coord = uint2(float2(coord) * uniform.scale);
	else
		coord = mul24(coord, uint2(uniform.scale));

	float4 pixel = res.tex.read(coord);
	float2 sel0 = (pos.y & 2) == 0 ? pixel.rb : pixel.ga;
	float  sel1 = (pos.x & 8) == 0 ? sel0.x : sel0.y;
	return float4(sel1);
}

fragment float4 ps_convert_clut_4(ConvertShaderData data [[stage_in]],
	texture2d<float> texture [[texture(GSMTLTextureIndexNonHW)]],
	constant GSMTLCLUTConvertPSUniform& uniform [[buffer(GSMTLBufferIndexUniforms)]])
{
	// CLUT4 is easy, just two rows of 8x8.
	uint index = uint(data.p.x) + uniform.doffset;
	uint2 pos = uint2(index % 8, index / 8);

	uint2 final = uint2(float2(uniform.offset + pos) * uniform.scale);
	return texture.read(final);
}

fragment float4 ps_convert_clut_8(ConvertShaderData data [[stage_in]],
	texture2d<float> texture [[texture(GSMTLTextureIndexNonHW)]],
	constant GSMTLCLUTConvertPSUniform& uniform [[buffer(GSMTLBufferIndexUniforms)]])
{
	uint index = min(uint(data.p.x) + uniform.doffset, 255u);

	// CLUT is arranged into 8 groups of 16x2, with the top-right and bottom-left quadrants swapped.
	// This can probably be done better..
	uint subgroup = (index / 8) % 4;
	uint2 pos;
	pos.x = (index % 8) + ((subgroup >= 2) ? 8 :0u);
	pos.y = ((index / 32u) * 2u) + (subgroup % 2u);

	uint2 final = uint2(float2(uniform.offset + pos) * uniform.scale);
	return texture.read(final);
}

fragment float4 ps_yuv(ConvertShaderData data [[stage_in]], ConvertPSRes res,
	constant GSMTLConvertPSUniform& uniform [[buffer(GSMTLBufferIndexUniforms)]])
{
	float4 i = res.sample(data.t);
	float4 o = float4(0);

	// Value from GS manual
	const float3x3 rgb2yuv =
	{
		{0.587, -0.311, -0.419},
		{0.114,  0.500, -0.081},
		{0.299, -0.169,  0.500}
	};

	float3 yuv = rgb2yuv * i.gbr;

	float Y  = 0xDB / 255.f * yuv.x + 0x10 / 255.f;
	float Cr = 0xE0 / 255.f * yuv.y + 0x80 / 255.f;
	float Cb = 0xE0 / 255.f * yuv.z + 0x80 / 255.f;

	switch (uniform.emoda)
	{
		case 0: o.a = i.a; break;
		case 1: o.a = Y;   break;
		case 2: o.a = Y/2; break;
		case 3: o.a = 0;   break;
	}

	switch (uniform.emodc)
	{
		case 0: o.rgb = i.rgb;             break;
		case 1: o.rgb = float3(Y);         break;
		case 2: o.rgb = float3(Y, Cb, Cr); break;
		case 3: o.rgb = float3(i.a);       break;
	}

	return o;
}

fragment half4 ps_imgui(ImGuiShaderData data [[stage_in]], texture2d<half> texture [[texture(GSMTLTextureIndexNonHW)]])
{
	constexpr sampler s(coord::normalized, filter::linear, address::clamp_to_edge);
	return data.c * texture.sample(s, data.t);
}

fragment float4 ps_shadeboost(float4 p [[position]], DirectReadTextureIn<float> tex, constant float3& cb [[buffer(GSMTLBufferIndexUniforms)]])
{
	const float brt = cb.x;
	const float con = cb.y;
	const float sat = cb.z;
	// Increase or decrease these values to adjust r, g and b color channels separately
	const float AvgLumR = 0.5;
	const float AvgLumG = 0.5;
	const float AvgLumB = 0.5;

	const float3 LumCoeff = float3(0.2125, 0.7154, 0.0721);

	float3 AvgLumin = float3(AvgLumR, AvgLumG, AvgLumB);
	float3 brtColor = tex.read(p).rgb * brt;
	float dot_intensity = dot(brtColor, LumCoeff);
	float3 intensity = float3(dot_intensity, dot_intensity, dot_intensity);
	float3 satColor = mix(intensity, brtColor, sat);
	float3 conColor = mix(AvgLumin, satColor, con);

	return float4(conColor, 1);
}
