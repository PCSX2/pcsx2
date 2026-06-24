// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "tfx_ps_resources.hlsl"

#ifndef PS_ANISOTROPIC_FILTERING
#define PS_AUTOMATIC_LOD 0
#define PS_MANUAL_LOD 0
#define PS_ANISOTROPIC_FILTERING 2
#endif

#if (PS_AUTOMATIC_LOD != 1) && (PS_MANUAL_LOD == 1)
float manual_lod(float uv_w);
#endif

#if PS_ANISOTROPIC_FILTERING > 1
bool2 nan_or_inf(float2 xy)
{
	// FXC (<=SM5.1) may optimise away isnan and isinf.
	// DXC (>=SM6.0) will preserve them.
#ifdef __hlsl_dx_compiler
	return isinf(xy) | isnan(xy);
#else
    return (asuint(xy) & 0x7f800000) == 0x7f800000;
#endif
}

export float4 sample_c_af(float2 uv, float uv_w)
{
	// HW sampler will reject bad UVs, match that here.
    uv = any(nan_or_inf(uv)) ? float2(0, 0) : uv;

	// Large floating point values risk NaN/Inf values.
	// Above this value floats lose decimal precision, so seems a resonable limit for UVs.
	uv = clamp(uv, -8388608.0f, 8388608.0f);

	// Below taken from https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#7.18.11%20LOD%20Calculations
	// With guidance from https://pema.dev/2025/05/09/mipmaps-too-much-detail/ 
	float2 sz;
	Texture.GetDimensions(sz.x, sz.y);
	float2 dX = ddx(uv) * sz;
	float2 dY = ddy(uv) * sz;

	// Calculate Ellipse Transform
	bool d_zero = length(dX) == 0 || length(dY) == 0;
	bool d_par = (dX.x * dY.y - dY.x * dX.y) == 0;
	bool d_per = dot(dX, dY) == 0;
    bool d_inf_nan = any(nan_or_inf(dX) | nan_or_inf(dY));

	if (!(d_zero || d_par || d_per || d_inf_nan))
	{
		float A = dX.y * dX.y + dY.y * dY.y;
		float B = -2 * (dX.x * dX.y + dY.x * dY.y);
		float C = dX.x * dX.x + dY.x * dY.x;
		float f = (dX.x * dY.y - dY.x * dX.y);
		float F = f * f;

		float p = A - C;
		float q = A + C;
		float t = sqrt(p * p + B * B);

		float2 new_dX = float2(
			sqrt(F * (t + p) / (t * (q + t))),
			sqrt(F * (t - p) / (t * (q + t))) * sign(B)
		);
		
		float2 new_dY = float2(
			sqrt(F * (t - p) / (t * (q - t))) * -sign(B),
			sqrt(F * (t + p) / (t * (q - t)))
		);
		
        d_inf_nan = any(nan_or_inf(new_dX) | nan_or_inf(new_dY));
		if (!d_inf_nan)
		{
			dX = new_dX;
			dY = new_dY;
		}
	}

	// Compute AF values
	float squared_length_x = dX.x * dX.x + dX.y * dX.y;
	float squared_length_y = dY.x * dY.x + dY.y * dY.y;
	float determinant = abs(dX.x * dY.y - dX.y * dY.x);
	bool is_major_x = squared_length_x > squared_length_y;
	float squared_length_major = is_major_x ? squared_length_x : squared_length_y;
	float length_major = sqrt(squared_length_major);

	float aniso_ratio;
	float length_lod;
	float2 aniso_line;
	if (length_major <= 1.0f)
	{
		// A zero length_major would result in NaN Lod and break sampling.
		// A small length_major would result in aniso_ratio getting clamped to 1.
		// Perform isotropic filtering instead.
		aniso_ratio = 1.0f;
		length_lod = length_major;
		aniso_line = float2(0, 0);
	}
	else
	{
		float norm_major = 1.0f / length_major;
	
		float2 aniso_line_dir = float2(
			(is_major_x ? dX.x : dY.x) * norm_major,
			(is_major_x ? dX.y : dY.y) * norm_major
		);
	
		aniso_ratio = squared_length_major / determinant;

		// Calculate the minor length of the ellipse for Lod, while also clamping the ratio of anisotropy.
		if (aniso_ratio > PS_ANISOTROPIC_FILTERING)
		{
			// ratio is clamped - Lod is based on ratio (preserves area)
			aniso_ratio = PS_ANISOTROPIC_FILTERING;
			length_lod = length_major / PS_ANISOTROPIC_FILTERING;
		}
		else
		{
			// ratio not clamped - Lod is based on area
			length_lod = determinant / length_major;
		}

		// clamp to top Lod
		if (length_lod < 1.0f)
			aniso_ratio = max(1.0f, aniso_ratio * length_lod);

		aniso_ratio = round(aniso_ratio);
		aniso_line = aniso_line_dir * 0.5f * length_major * (1.0f / sz);
	}

#if PS_AUTOMATIC_LOD == 1
	float lod = log2(length_lod);
#elif PS_MANUAL_LOD == 1
	float lod = manual_lod(uv_w);
#else
	float lod = 0; // No Lod
#endif

	float4 colour;
	if (aniso_ratio == 1.0f)
		colour = Texture.SampleLevel(TextureSampler, uv, lod);
	else
	{
		float4 num = float4(0, 0, 0, 0);
		for (int i = 0; i < aniso_ratio; i++)
		{
			float2 d = -aniso_line + (0.5f + i) * (2.0f * aniso_line) / aniso_ratio;	
			float2 uv_sample = uv + d;
			float4 sample_colour = Texture.SampleLevel(TextureSampler, uv_sample, lod);
			num += sample_colour;
		}

		colour = num / aniso_ratio;
	}
	return colour;
}
#endif
