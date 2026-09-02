static float4 sample_c_af(float2 uv, float uv_w)
{
	// HW sampler will reject bad UVs, match that here.
	uv = any(IS_NAN_OR_INF_4(float4(uv, 0, 0))) ? float2_bcast(0.0f) : uv;

	// Large floating point values risk NaN/Inf values.
	// Above this value floats lose decimal precision, so seems a resonable limit for UVs.
	uv = clamp(uv, -8388608.0f, 8388608.0f);

	// Below taken from https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#7.18.11%20LOD%20Calculations
	// And https://registry.khronos.org/OpenGL/extensions/EXT/EXT_texture_filter_anisotropic.txt
	// With guidance from https://pema.dev/2025/05/09/mipmaps-too-much-detail/
	float2 sz = float2(get_tex_dims());
	float2 dX = ddx(uv) * sz;
	float2 dY = ddy(uv) * sz;

	float length_x = length(dX);
	float length_y = length(dY);

	// Calculate Ellipse Transform
	bool d_zero = length_x < 0.001f || length_y < 0.001f;
	float f = (dX.x * dY.y - dX.y * dY.x);
	bool d_par = f < 0.001f;
	bool d_per = dot(dX, dY) < 0.001f;
	bool d_inf_nan = any(IS_NAN_OR_INF_4(float4(dX, dY)));

	if (!(d_zero || d_par || d_per || d_inf_nan))
	{
		float A = dX.y * dX.y + dY.y * dY.y;
		float B = -2 * (dX.x * dX.y + dY.x * dY.y);
		float C = dX.x * dX.x + dY.x * dY.x;
		float F = f * f;

		float p = A - C;
		float q = A + C;
		float t = sqrt(p * p + B * B);

		float denom_plus  = t * (q + t);
		float denom_minus = t * (q - t);

		float sqrt_num_plus = sqrt(F * (t + p));
		float sqrt_num_minus = sqrt(F * (t - p));

		float inv_sqrt_denom_plus  = rsqrt(denom_plus);
		float inv_sqrt_denom_minus = rsqrt(denom_minus);

		float signB = sign(B);

		float2 new_dX = float2(
			sqrt_num_plus * inv_sqrt_denom_plus,
			sqrt_num_minus * inv_sqrt_denom_plus * signB
		);

		float2 new_dY = float2(
			sqrt_num_minus * inv_sqrt_denom_minus * -signB,
			sqrt_num_plus * inv_sqrt_denom_minus
		);

		d_inf_nan = any(IS_NAN_OR_INF_4(float4(new_dX, new_dY)));
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
		aniso_line = float2_bcast(0.0f);
	}
	else
	{
		float2 aniso_line_dir = is_major_x ? dX : dY;

		aniso_ratio = min(length_major / length_minor, float(PS_SW_ANISO));
		length_lod = length_major / aniso_ratio;

		// clamp to top Lod
		if (length_lod < 1.0f)
			aniso_ratio = max(1.0f, aniso_ratio * length_lod);

		aniso_ratio = round(aniso_ratio);

		aniso_line = aniso_line_dir * 0.5f * (1.0f / sz);
	}

	float lod = PS_AUTOMATIC_LOD != FALSE ? log2(length_lod) : PS_MANUAL_LOD != FALSE ? manual_lod(uv_w) : 0.0f;

	float4 colour;
	if (aniso_ratio == 1.0f)
	{
		colour = sample_tex_lod(uv, lod);
	}
	else
	{
		float4 num = float4_bcast(0.0f);
		float2 segment = (2.0f * aniso_line) / aniso_ratio;
		
		int aniso_ratio_i = int(aniso_ratio);
		for (int i = 0; i < aniso_ratio_i; i++)
		{
			float2 d = -aniso_line + (0.5f + i) * segment;
			float2 uv_sample = uv + d;
			float4 sample_colour = sample_tex_lod(uv_sample, lod);
			num += sample_colour;
		}

		colour = num / aniso_ratio;
	}
	return colour;
}
