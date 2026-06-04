// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include <metal_stdlib>
#include "GSMTLSharedHeader.h"

using namespace metal;

struct ConvertShaderData
{
	float4 p [[position]];
	float2 t;
};

struct ConvertPSRes
{
	texture2d<float> texture [[texture(GSMTLTextureIndexNonHW)]];
	sampler s [[sampler(0)]];
	float4 sample(float2 coord)
	{
		return texture.sample(s, coord);
	}
	float4 sample_level(float2 coord, float lod)
	{
		return texture.sample(s, coord, level(lod));
	}
};

struct ConvertPSDepthRes
{
	depth2d<float> texture [[texture(GSMTLTextureIndexNonHW)]];
	sampler s [[sampler(0)]];
	float sample(float2 coord)
	{
		return texture.sample(s, coord);
	}
};

static inline float4 convert_depth32_rgba8(uint value)
{
	return float4(as_type<uchar4>(value));
}

static inline float4 convert_depth32_rgba8(float value)
{
	return convert_depth32_rgba8(uint(value * 0x1p32));
}

static inline float4 convert_depth16_rgba8(uint value)
{
	return float4(uint4(value << 3, value >> 2, value >> 7, value >> 8) & uint4(0xf8, 0xf8, 0xf8, 0x80));
}

static inline float4 convert_depth16_rgba8(float value)
{
	return convert_depth16_rgba8(uint(value * 0x1p32));
}

#ifndef __HAVE_MUL24__
template <typename T> T mul24(T a, T b) { return a * b; }
#endif
