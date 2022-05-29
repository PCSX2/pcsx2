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

float4 ps_crt(float4 color, int i)
{
	constexpr float4 mask[4] =
	{
		float4(1, 0, 0, 0),
		float4(0, 1, 0, 0),
		float4(0, 0, 1, 0),
		float4(1, 1, 1, 0),
	};

	return color * saturate(mask[i] + 0.5f);
}

float4 ps_scanlines(float4 color, int i)
{
	constexpr float4 mask[2] =
	{
		float4(1, 1, 1, 0),
		float4(0, 0, 0, 0)
	};

	return color * saturate(mask[i] + 0.5f);
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

fragment void ps_datm1(float4 p [[position]], DirectReadTextureIn<float> tex)
{
	if (tex.read(p).a < (127.5f / 255.f))
		discard_fragment();
}

fragment void ps_datm0(float4 p [[position]], DirectReadTextureIn<float> tex)
{
	if (tex.read(p).a > (127.5f / 255.f))
		discard_fragment();
}

fragment float4 ps_primid_init_datm0(float4 p [[position]], DirectReadTextureIn<float> tex)
{
	return tex.read(p).a > (127.5f / 255.f) ? -1 : FLT_MAX;
}

fragment float4 ps_primid_init_datm1(float4 p [[position]], DirectReadTextureIn<float> tex)
{
	return tex.read(p).a < (127.5f / 255.f) ? -1 : FLT_MAX;
}

fragment float4 ps_mod256(float4 p [[position]], DirectReadTextureIn<float> tex)
{
	float4 c = round(tex.read(p) * 255.f);
	return (c - 256.f * floor(c / 256.f)) / 255.f;
}

fragment float4 ps_filter_scanlines(ConvertShaderData data [[stage_in]], ConvertPSRes res)
{
	return ps_scanlines(res.sample(data.t), uint(data.p.y) % 2);
}

fragment float4 ps_filter_diagonal(ConvertShaderData data [[stage_in]], ConvertPSRes res)
{
	uint4 p = uint4(data.p);
	return ps_crt(res.sample(data.t), (p.x + (p.y % 3)) % 3);
}

fragment float4 ps_filter_transparency(ConvertShaderData data [[stage_in]], ConvertPSRes res)
{
	float4 c = res.sample(data.t);
	c.a = dot(c.rgb, float3(0.299f, 0.587f, 0.114f));
	return c;
}

fragment float4 ps_filter_triangular(ConvertShaderData data [[stage_in]], ConvertPSRes res)
{
	uint4 p = uint4(data.p);
	uint val = ((p.x + ((p.y >> 1) & 1) * 3) >> 1) % 3;
	return ps_crt(res.sample(data.t), val);
}

fragment float4 ps_filter_complex(ConvertShaderData data [[stage_in]], ConvertPSRes res)
{
	float2 texdim = float2(res.texture.get_width(), res.texture.get_height());

	if (dfdy(data.t.y) * texdim.y > 0.5)
	{
		return res.sample(data.t);
	}
	else
	{
		float factor = (0.9f - 0.4f * cos(2.f * M_PI_F * data.t.y * texdim.y));
		float ycoord = (floor(data.t.y * texdim.y) + 0.5f) / texdim.y;
		return factor * res.sample(float2(data.t.x, ycoord));
	}
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

static float pack_rgba8_depth(float4 unorm)
{
	return float(as_type<uint>(uchar4(unorm * 255.f + 0.5f))) * 0x1p-32f;
}

fragment DepthOut ps_convert_rgba8_float32(ConvertShaderData data [[stage_in]], ConvertPSRes res)
{
	return pack_rgba8_depth(res.sample(data.t));
}

fragment DepthOut ps_convert_rgba8_float24(ConvertShaderData data [[stage_in]], ConvertPSRes res)
{
	// Same as above but without the alpha channel (24 bits Z)
	return pack_rgba8_depth(float4(res.sample(data.t).rgb, 0));
}

fragment DepthOut ps_convert_rgba8_float16(ConvertShaderData data [[stage_in]], ConvertPSRes res)
{
	return float(as_type<ushort>(uchar2(res.sample(data.t).rg * 255.f + 0.5f))) * 0x1p-32;
}

fragment DepthOut ps_convert_rgb5a1_float16(ConvertShaderData data [[stage_in]], ConvertPSRes res)
{
	uint4 cu = uint4(res.sample(data.t) * 255.f + 0.5f);
	uint out = (cu.x >> 3) | ((cu.y << 2) & 0x03e0) | ((cu.z << 7) & 0x7c00) | ((cu.w << 8) & 0x8000);
	return float(out) * 0x1p-32;
}

fragment float4 ps_convert_rgba_8i(ConvertShaderData data [[stage_in]], ConvertPSRes res,
	constant GSMTLConvertPSUniform& uniform [[buffer(GSMTLBufferIndexUniforms)]])
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
	float c;

	uint2 sel = uint2(data.p.xy) % uint2(16, 16);
	uint2 tb  = (uint2(data.p.xy) & ~uint2(15, 3)) >> 1;

	uint ty  = tb.y | (uint(data.p.y) & 1);
	uint txN = tb.x | (uint(data.p.x) & 7);
	uint txH = tb.x | ((uint(data.p.x) + 4) & 7);

	txN *= SCALING_FACTOR.x;
	txH *= SCALING_FACTOR.x;
	ty  *= SCALING_FACTOR.y;

	// TODO investigate texture gather
	float4 cN = res.texture.read(uint2(txN, ty));
	float4 cH = res.texture.read(uint2(txH, ty));

	if ((sel.y & 4) == 0)
	{
		// Column 0 and 2
		if ((sel.y & 2) == 0)
		{
			if ((sel.x & 8) == 0)
				c = cN.r;
			else
				c = cN.b;
		}
		else
		{
			if ((sel.x & 8) == 0)
				c = cH.g;
			else
				c = cH.a;
		}
	}
	else
	{
		// Column 1 and 3
		if ((sel.y & 2) == 0)
		{
			if ((sel.x & 8) == 0)
				c = cH.r;
			else
				c = cH.b;
		}
		else
		{
			if ((sel.x & 8) == 0)
				c = cN.g;
			else
				c = cN.a;
		}
	}
	return float4(c);
}

fragment float4 ps_yuv(ConvertShaderData data [[stage_in]], ConvertPSRes res,
	constant GSMTLConvertPSUniform& uniform [[buffer(GSMTLBufferIndexUniforms)]])
{
	float4 i = res.sample(data.t);
	float4 o;

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

fragment half4 ps_imgui_a8(ImGuiShaderData data [[stage_in]], texture2d<half> texture [[texture(GSMTLTextureIndexNonHW)]])
{
	constexpr sampler s(coord::normalized, filter::linear, address::clamp_to_edge);
	return data.c * half4(1, 1, 1, texture.sample(s, data.t).a);
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
