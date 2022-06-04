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

// use vs_convert from convert.metal

static float4 ps_crt(float4 color, int i)
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

static float4 ps_scanlines(float4 color, int i)
{
	constexpr float4 mask[2] =
	{
		float4(1, 1, 1, 0),
		float4(0, 0, 0, 0)
	};

	return color * saturate(mask[i] + 0.5f);
}

// use ps_copy from convert.metal

fragment float4 ps_filter_scanlines(ConvertShaderData data [[stage_in]], ConvertPSRes res)
{
	return ps_scanlines(res.sample(data.t), uint(data.p.y) % 2);
}

fragment float4 ps_filter_diagonal(ConvertShaderData data [[stage_in]], ConvertPSRes res)
{
	uint4 p = uint4(data.p);
	return ps_crt(res.sample(data.t), (p.x + (p.y % 3)) % 3);
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
