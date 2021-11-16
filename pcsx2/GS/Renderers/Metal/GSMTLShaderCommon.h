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

#pragma once
#include <metal_stdlib>
#include "GSMTLSharedHeader.h"

using namespace metal;

constant uchar2 SCALING_FACTOR [[function_constant(GSMTLConstantIndex_SCALING_FACTOR)]];

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

static inline float4 convert_depth32_rgba8(float value)
{
	uint val = uint(value * 0x1p32);
	return float4(as_type<uchar4>(val));
}

static inline float4 convert_depth16_rgba8(float value)
{
	uint val = uint(value * 0x1p32);
	return float4(uint4(val << 3, val >> 2, val >> 7, val >> 8) & uint4(0xf8, 0xf8, 0xf8, 0x80));
}
