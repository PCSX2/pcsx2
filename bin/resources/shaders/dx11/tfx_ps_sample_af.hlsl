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
	uv = any(nan_or_inf(uv)) ? float2(0.0f, 0.0f) : uv;

	// Large floating point values risk NaN/Inf values.
	// Above this value floats lose decimal precision, so seems a resonable limit for UVs.
	uv = clamp(uv, -8388608.0f, 8388608.0f);

	// Below taken from https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#7.18.11%20LOD%20Calculations
	// And https://registry.khronos.org/OpenGL/extensions/EXT/EXT_texture_filter_anisotropic.txt
	// With guidance from https://pema.dev/2025/05/09/mipmaps-too-much-detail/ 
	float2 sz;
	Texture.GetDimensions(sz.x, sz.y);
	float2 dX = ddx(uv) * sz;
	float2 dY = ddy(uv) * sz;

	float length_x = length(dX);
	float length_y = length(dY);

	// Calculate Ellipse Transform
	bool d_zero = length_x < 0.001f || length_y < 0.001f;
	float f = (dX.x * dY.y - dX.y * dY.x);
	bool d_par = f < 0.001f;
	bool d_per = dot(dX, dY) < 0.001f;
	bool d_inf_nan = any(nan_or_inf(dX) | nan_or_inf(dY));

	if (!(d_zero || d_par || d_per || d_inf_nan))
	{
		float A = dX.y * dX.y + dY.y * dY.y;
		float B = -2 * (dX.x * dX.y + dY.x * dY.y);
		float C = dX.x * dX.x + dY.x * dY.x;
		float F = f * f;

		float p = A - C;
		float q = A + C;
		float t = sqrt(p * p + B * B);

		float sqrt_num_plus  = sqrt(F * (t + p));
		float sqrt_num_minus = sqrt(F * (t - p));

		float inv_sqrt_denom_plus  = rsqrt(t * (q + t));
		float inv_sqrt_denom_minus = rsqrt(t * (q - t));

		float signB = sign(B);

		float2 new_dX = float2(
			sqrt_num_plus  * inv_sqrt_denom_plus,
			sqrt_num_minus * inv_sqrt_denom_plus * signB
		);

		float2 new_dY = float2(
			sqrt_num_minus * inv_sqrt_denom_minus * -signB,
			sqrt_num_plus  * inv_sqrt_denom_minus
		);

		d_inf_nan = any(nan_or_inf(new_dX) | nan_or_inf(new_dY));
		if (!d_inf_nan)
		{
			dX = new_dX;
			dY = new_dY;
			length_x = length(dX);
			length_y = length(dY);
		}
	}

	// Compute AF values
	bool is_major_x = length_x > length_y;
	float length_major = is_major_x ? length_x : length_y;
	float length_minor = is_major_x ? length_y : length_x;

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
		aniso_line = float2(0.0f, 0.0f);
	}
	else
	{
		float2 aniso_line_dir = is_major_x ? dX : dY;

		aniso_ratio = min(length_major / length_minor, PS_ANISOTROPIC_FILTERING);
		length_lod = length_major / aniso_ratio;

		// clamp to top Lod
		if (length_lod < 1.0f)
			aniso_ratio = max(1.0f, aniso_ratio * length_lod);

		aniso_ratio = round(aniso_ratio);

		aniso_line = aniso_line_dir * 0.5f * (1.0f / sz);
	}

#if PS_AUTOMATIC_LOD == 1
	float lod = log2(length_lod);
#elif PS_MANUAL_LOD == 1
	float lod = manual_lod(uv_w);
#else
	float lod = 0.0f; // No Lod
#endif

	float4 colour;
	if (aniso_ratio == 1.0f)
		colour = Texture.SampleLevel(TextureSampler, uv, lod);
	else
	{
		float4 num = float4(0.0f, 0.0f, 0.0f, 0.0f);
		float2 segment = (2.0f * aniso_line) / aniso_ratio;

		int aniso_ratio_i = (int)aniso_ratio;
		for (int i = 0; i < aniso_ratio_i; i++)
		{
			float2 d = -aniso_line + (0.5f + i) * segment;
			float2 uv_sample = uv + d;
			float4 sample_colour = Texture.SampleLevel(TextureSampler, uv_sample, lod);
			num += sample_colour;
		}

		colour = num / aniso_ratio;
	}
	return colour;
}
#endif
