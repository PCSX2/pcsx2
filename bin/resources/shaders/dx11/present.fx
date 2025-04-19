// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#ifndef PS_HDR
#define PS_HDR 0
#endif

struct VS_INPUT
{
	float4 p : POSITION;
	float2 t : TEXCOORD0;
	float4 c : COLOR;
};

struct VS_OUTPUT
{
	float4 p : SV_Position;
	float2 t : TEXCOORD0;
	float4 c : COLOR;
};

cbuffer cb0 : register(b0)
{
	float4 u_source_rect;
	float4 u_target_rect;
	float2 u_source_size;
	float2 u_target_size;
	float2 u_target_resolution;
	float2 u_rcp_target_resolution; // 1 / u_target_resolution
	float2 u_source_resolution;
	float2 u_rcp_source_resolution; // 1 / u_source_resolution
	float2 u_time_and_brightness; // time, user brightness scale (HDR)
};

Texture2D Texture;
SamplerState TextureSampler;

float4 sample_c(float2 uv)
{
	return Texture.Sample(TextureSampler, uv);
}

struct PS_INPUT
{
	float4 p : SV_Position;
	float2 t : TEXCOORD0;
	float4 c : COLOR;
};

struct PS_OUTPUT
{
	float4 c : SV_Target0;
};

VS_OUTPUT vs_main(VS_INPUT input)
{
	VS_OUTPUT output;

	output.p = input.p;
	output.t = input.t;
	output.c = input.c;

	return output;
}

PS_OUTPUT EncodeOutput(PS_OUTPUT output)
{
	// If necessary we could convert to any color space here,
	// assuming we are starting Rec.709 with gamma 2.2.
#if !PS_HDR && 1 //TODO: Test only!
	// Convert to sRGB encoding (useful to test SDR in HDR as Windows interprets SDR content as sRGB)
	float3 color_in_excess = output.c.rgb - saturate(output.c.rgb);
	output.c.rgb = saturate(output.c.rgb);
	output.c.rgb = pow(output.c.rgb, 2.2);
	output.c.rgb = (output.c.rgb < 0.0031308) ? (output.c.rgb * 12.92) : (1.055 * pow(output.c.rgb, 0.41666) - 0.055);
	output.c.rgb += color_in_excess;
#endif
	
	// Apply the user brightness level
	output.c.rgb *= u_time_and_brightness.y;
	
	return output;
}

PS_OUTPUT ps_copy(PS_INPUT input)
{
	PS_OUTPUT output;

	output.c = sample_c(input.t);

	return EncodeOutput(output);
}

float4 ps_crt(PS_INPUT input, int i)
{
	float4 mask[4] =
		{
			float4(1, 0, 0, 0),
			float4(0, 1, 0, 0),
			float4(0, 0, 1, 0),
			float4(1, 1, 1, 0)
		};

	return sample_c(input.t) * saturate(mask[i] + 0.5f);
}

float4 ps_scanlines(PS_INPUT input, int i)
{
	float4 mask[2] =
		{
			float4(1, 1, 1, 0),
			float4(0, 0, 0, 0)
		};

	return sample_c(input.t) * saturate(mask[i] + 0.5f);
}

PS_OUTPUT ps_filter_scanlines(PS_INPUT input)
{
	PS_OUTPUT output;

	uint4 p = (uint4)input.p;

	output.c = ps_scanlines(input, p.y % 2);

	return EncodeOutput(output);
}

PS_OUTPUT ps_filter_diagonal(PS_INPUT input)
{
	PS_OUTPUT output;

	uint4 p = (uint4)input.p;

	output.c = ps_crt(input, (p.x + (p.y % 3)) % 3);

	return EncodeOutput(output);
}

PS_OUTPUT ps_filter_triangular(PS_INPUT input)
{
	PS_OUTPUT output;

	uint4 p = (uint4)input.p;

	// output.c = ps_crt(input, ((p.x + (p.y & 1) * 3) >> 1) % 3);
	output.c = ps_crt(input, ((p.x + ((p.y >> 1) & 1) * 3) >> 1) % 3);

	return EncodeOutput(output);
}

static const float PI = 3.14159265359f;
PS_OUTPUT ps_filter_complex(PS_INPUT input) // triangular
{
	PS_OUTPUT output;

	float2 texdim; 
	Texture.GetDimensions(texdim.x, texdim.y);

	output.c = (0.9 - 0.4 * cos(2 * PI * input.t.y * texdim.y)) * sample_c(float2(input.t.x, (floor(input.t.y * texdim.y) + 0.5) / texdim.y));

	return EncodeOutput(output);
}

//Lottes CRT
#define MaskingType 4                      //[1|2|3|4] The type of CRT shadow masking used. 1: compressed TV style, 2: Aperture-grille, 3: Stretched VGA style, 4: VGA style.
#define ScanBrightness -8.00               //[-16.0 to 1.0] The overall brightness of the scanline effect. Lower for darker, higher for brighter.
#define FilterCRTAmount -3.00              //[-4.0 to 1.0] The amount of filtering used, to replicate the TV CRT look. Lower for less, higher for more.
#define HorizontalWarp 0.00                //[0.0 to 0.1] The distortion warping effect for the horizontal (x) axis of the screen. Use small increments.
#define VerticalWarp 0.00                  //[0.0 to 0.1] The distortion warping effect for the verticle (y) axis of the screen. Use small increments.
#define MaskAmountDark 0.50                //[0.0 to 1.0] The value of the dark masking line effect used. Lower for darker lower end masking, higher for brighter.
#define MaskAmountLight 1.50               //[0.0 to 2.0] The value of the light masking line effect used. Lower for darker higher end masking, higher for brighter.
#define BloomPixel -1.50                   //[-2.0 -0.5] Pixel bloom radius. Higher for increased softness of bloom.
#define BloomScanLine -2.0                 //[-4.0 -1.0] Scanline bloom radius. Higher for increased softness of bloom.
#define BloomAmount 0.15                   //[0.0 1.0] Bloom intensity. Higher for brighter.
#define Shape 2.0                          //[0.0 10.0] Kernal filter shape. Lower values will darken image and introduce moire patterns if used with curvature.
#define UseShadowMask 1                    //[0 or 1] Enables, or disables the use of the CRT shadow mask. 0 is disabled, 1 is enabled.

float ToLinear1(float c)
{
#if PS_HDR // Already linear
	return c;
#endif
	return pow(abs(c), 2.2) * sign(c);
}

float3 ToLinear(float3 c)
{
	return float3(ToLinear1(c.r), ToLinear1(c.g), ToLinear1(c.b));
}

float ToGamma1(float c)
{
#if PS_HDR // Already linear
	return c;
#endif
	return pow(abs(c), 1.0 / 2.2) * sign(c);
}

float3 ToGamma(float3 c)
{
	return float3(ToGamma1(c.r), ToGamma1(c.g), ToGamma1(c.b));
}

float3 Fetch(float2 pos, float2 off)
{
	pos = (floor(pos * u_target_size + off) + float2(0.5, 0.5)) / u_target_size;
	if (max(abs(pos.x - 0.5), abs(pos.y - 0.5)) > 0.5)
	{
		return float3(0.0, 0.0, 0.0);
	}
	else
	{
		return ToLinear(Texture.Sample(TextureSampler, pos.xy).rgb);
	}
}

float2 Dist(float2 pos)
{
	pos = pos * float2(640, 480);

	return -((pos - floor(pos)) - float2(0.5, 0.5));
}

float Gaus(float pos, float scale)
{
	return exp2(scale * pos * pos);
}

float3 Horz3(float2 pos, float off)
{
	float3 b = Fetch(pos, float2(-1.0, off));
	float3 c = Fetch(pos, float2(0.0, off));
	float3 d = Fetch(pos, float2(1.0, off));
	float dst = Dist(pos).x;

	// Convert distance to weight.
	float scale = FilterCRTAmount;
	float wb = Gaus(dst - 1.0, scale);
	float wc = Gaus(dst + 0.0, scale);
	float wd = Gaus(dst + 1.0, scale);

	return (b * wb + c * wc + d * wd) / (wb + wc + wd);
}

float3 Horz5(float2 pos, float off)
{
	float3 a = Fetch(pos, float2(-2.0, off));
	float3 b = Fetch(pos, float2(-1.0, off));
	float3 c = Fetch(pos, float2(0.0, off));
	float3 d = Fetch(pos, float2(1.0, off));
	float3 e = Fetch(pos, float2(2.0, off));
	float dst = Dist(pos).x;

	// Convert distance to weight.
	float scale = FilterCRTAmount;

	float wa = Gaus(dst - 2.0, scale);
	float wb = Gaus(dst - 1.0, scale);
	float wc = Gaus(dst + 0.0, scale);
	float wd = Gaus(dst + 1.0, scale);
	float we = Gaus(dst + 2.0, scale);

	return (a * wa + b * wb + c * wc + d * wd + e * we) / (wa + wb + wc + wd + we);
}

float3 Horz7(float2 pos, float off)
{
	float3 a = Fetch(pos, float2(-3.0, off));
	float3 b = Fetch(pos, float2(-2.0, off));
	float3 c = Fetch(pos, float2(-1.0, off));
	float3 d = Fetch(pos, float2( 0.0, off));
	float3 e = Fetch(pos, float2( 1.0, off));
	float3 f = Fetch(pos, float2( 2.0, off));
	float3 g = Fetch(pos, float2( 3.0, off));

	float dst = Dist(pos).x;
	// Convert distance to weight.
	float scale = BloomPixel;
	float wa = Gaus(dst - 3.0, scale);
	float wb = Gaus(dst - 2.0, scale);
	float wc = Gaus(dst - 1.0, scale);
	float wd = Gaus(dst + 0.0, scale);
	float we = Gaus(dst + 1.0, scale);
	float wf = Gaus(dst + 2.0, scale);
	float wg = Gaus(dst + 3.0, scale);

	// Return filtered sample.
	return (a * wa + b * wb + c * wc + d * wd + e * we + f * wf + g * wg) / (wa + wb + wc + wd + we + wf + wg);
}

// Return scanline weight.
float Scan(float2 pos, float off)
{
	float dst = Dist(pos).y;
	return Gaus(dst + off, ScanBrightness);
}

float BloomScan(float2 pos, float off)
{
	float dst = Dist(pos).y;

	return Gaus(dst + off, BloomScanLine);
}

float3 Tri(float2 pos)
{
	float3 a = Horz3(pos, -1.0);
	float3 b = Horz5(pos, 0.0);
	float3 c = Horz3(pos, 1.0);

	float wa = Scan(pos, -1.0);
	float wb = Scan(pos, 0.0);
	float wc = Scan(pos, 1.0);

	return (a * wa) + (b * wb) + (c * wc);
}

float3 Bloom(float2 pos)
{
	float3 a = Horz5(pos,-2.0);
	float3 b = Horz7(pos,-1.0);
	float3 c = Horz7(pos, 0.0);
	float3 d = Horz7(pos, 1.0);
	float3 e = Horz5(pos, 2.0);

	float wa = BloomScan(pos,-2.0);
	float wb = BloomScan(pos,-1.0); 
	float wc = BloomScan(pos, 0.0);
	float wd = BloomScan(pos, 1.0);
	float we = BloomScan(pos, 2.0);

	return a * wa + b * wb + c * wc + d * wd + e * we;
}

float2 Warp(float2 pos)
{
	pos = pos * 2.0 - 1.0;
	pos *= float2(1.0 + (pos.y * pos.y) * HorizontalWarp, 1.0 + (pos.x * pos.x) * VerticalWarp);
	return pos * 0.5 + 0.5;
}

float3 Mask(float2 pos)
{
#if MaskingType == 1
	// Very compressed TV style shadow mask.
	float lines = MaskAmountLight;
	float odd = 0.0;

	if (frac(pos.x / 6.0) < 0.5)
	{
		odd = 1.0;
	}
	if (frac((pos.y + odd) / 2.0) < 0.5)
	{
		lines = MaskAmountDark;
	}
	pos.x = frac(pos.x / 3.0);
	float3 mask = float3(MaskAmountDark, MaskAmountDark, MaskAmountDark);

	if (pos.x < 0.333)
	{
		mask.r = MaskAmountLight;
	}
	else if (pos.x < 0.666)
	{
		mask.g = MaskAmountLight;
	}
	else
	{
		mask.b = MaskAmountLight;
	}

	mask *= lines;

	return mask;

#elif MaskingType == 2
	// Aperture-grille.
	pos.x = frac(pos.x / 3.0);
	float3 mask = float3(MaskAmountDark, MaskAmountDark, MaskAmountDark);

	if (pos.x < 0.333)
	{
		mask.r = MaskAmountLight;
	}
	else if (pos.x < 0.666)
	{
		mask.g = MaskAmountLight;
	}
	else
	{
		mask.b = MaskAmountLight;
	}

	return mask;

#elif MaskingType == 3
	// Stretched VGA style shadow mask (same as prior shaders).
	pos.x += pos.y * 3.0;
	float3 mask = float3(MaskAmountDark, MaskAmountDark, MaskAmountDark);
	pos.x = frac(pos.x / 6.0);

	if (pos.x < 0.333)
	{
		mask.r = MaskAmountLight;
	}
	else if (pos.x < 0.666)
	{
		mask.g = MaskAmountLight;
	}
	else
	{
		mask.b = MaskAmountLight;
	}

	return mask;

#else
	// VGA style shadow mask.
	pos.xy = floor(pos.xy * float2(1.0, 0.5));
	pos.x += pos.y * 3.0;

	float3 mask = float3(MaskAmountDark, MaskAmountDark, MaskAmountDark);
	pos.x = frac(pos.x / 6.0);

	if (pos.x < 0.333)
	{
		mask.r = MaskAmountLight;
	}
	else if (pos.x < 0.666)
	{
		mask.g = MaskAmountLight;
	}
	else
	{
		mask.b = MaskAmountLight;
	}
	return mask;
#endif
}

float4 LottesCRTPass(float4 fragcoord)
{
	float4 color;
	fragcoord -= u_target_rect;
	float2 inSize = u_target_resolution - (2 * u_target_rect.xy);

	float2 pos = Warp(fragcoord.xy / inSize);
	color.rgb = Tri(pos);
	color.rgb += Bloom(pos) * BloomAmount;
#if UseShadowMask
	color.rgb *= Mask(fragcoord.xy);
#endif
	color.rgb = ToGamma(color.rgb);
	color.a = 1.0;

	return color;
}

PS_OUTPUT ps_filter_lottes(PS_INPUT input)
{
	PS_OUTPUT output;
	output.c = LottesCRTPass(input.p);

	return EncodeOutput(output);
}

PS_OUTPUT ps_4x_rgss(PS_INPUT input)
{
	PS_OUTPUT output;

	float2 dxy = float2(ddx(input.t.x), ddy(input.t.y));
	float3 color = 0;

	float s = 1.0/8.0;
	float l = 3.0/8.0;

	color += sample_c(input.t + float2( s, l) * dxy).rgb;
	color += sample_c(input.t + float2( l,-s) * dxy).rgb;
	color += sample_c(input.t + float2(-s,-l) * dxy).rgb;
	color += sample_c(input.t + float2(-l, s) * dxy).rgb;

	output.c = float4(color * 0.25,1);
	return EncodeOutput(output);
}

float Luminance(float3 color)
{
	float3 Rec709_Luminance = float3(0.2126, 0.7152, 0.0722);
	return dot(color, Rec709_Luminance);
}

// Non filtered gamma corrected sample (nearest neighbor)
float4 QuickSample(float2 uv, float gamma)
{
	float4 color = Texture.Sample(TextureSampler, uv); //TODO: bilinear or nearest???
#if !PS_HDR // HDR is already linear
	color.rgb = pow(color.rgb, gamma);
#endif
	return color;
}
float4 QuickSampleByPixel(float2 xy, float gamma)
{
	return QuickSample(xy * u_rcp_source_resolution, gamma);
}

// By Sam Belliveau and Filippo Tarpini. Public Domain license.
// Effectively a more accurate sharp bilinear filter when upscaling,
// that also works as a mathematically perfect downscale filter.
// https://entropymine.com/imageworsener/pixelmixing/
// https://github.com/obsproject/obs-studio/pull/1715
// https://legacy.imagemagick.org/Usage/filter/
float4 AreaSampling(float2 uv, float gamma)
{
	// Determine the sizes of the source and target images.
	float2 source_size = u_source_resolution; //TODO: "size" for these?
	float2 target_size = u_target_resolution;
	float2 inverted_target_size = u_rcp_target_resolution;

	// Compute the top-left and bottom-right corners of the target pixel box.
	float2 t_beg = floor(uv * target_size);
	float2 t_end = t_beg + float2(1.0, 1.0);

	// Convert the target pixel box to source pixel box.
	float2 beg = t_beg * inverted_target_size * source_size;
	float2 end = t_end * inverted_target_size * source_size;

	// Compute the top-left and bottom-right corners of the pixel box.
	float2 f_beg = floor(beg);
	float2 f_end = floor(end);

	// Compute how much of the start and end pixels are covered horizontally & vertically.
	float area_w = 1.0 - frac(beg.x);
	float area_n = 1.0 - frac(beg.y);
	float area_e = frac(end.x);
	float area_s = frac(end.y);

	// Compute the areas of the corner pixels in the pixel box.
	float area_nw = area_n * area_w;
	float area_ne = area_n * area_e;
	float area_sw = area_s * area_w;
	float area_se = area_s * area_e;

	// Initialize the color accumulator.
	float4 avg_color = float4(0.0, 0.0, 0.0, 0.0);
	float avg_luminance = 0.0;
	float4 temp_color;

	float luminance_gamma = 2.2;
	float luminance_inv_gamma = 1.0 / luminance_gamma;

	// Prevents rounding errors due to the coordinates flooring above
	const float2 offset = float2(0.5, 0.5);

	// Accumulate corner pixels.
	temp_color = QuickSampleByPixel(float2(f_beg.x, f_beg.y) + offset, gamma);
	avg_color += area_nw * temp_color;
	avg_luminance += area_nw * pow(Luminance(temp_color.rgb), luminance_inv_gamma);
	temp_color = QuickSampleByPixel(float2(f_end.x, f_beg.y) + offset, gamma);
	avg_color += area_ne * temp_color;
	avg_luminance += area_ne * pow(Luminance(temp_color.rgb), luminance_inv_gamma);
	temp_color = QuickSampleByPixel(float2(f_beg.x, f_end.y) + offset, gamma);
	avg_color += area_sw * temp_color;
	avg_luminance += area_sw * pow(Luminance(temp_color.rgb), luminance_inv_gamma);
	temp_color = QuickSampleByPixel(float2(f_end.x, f_end.y) + offset, gamma);
	avg_color += area_se * temp_color;
	avg_luminance += area_se * pow(Luminance(temp_color.rgb), luminance_inv_gamma);

	// Determine the size of the pixel box.
	int x_range = int(f_end.x - f_beg.x - 0.5);
	int y_range = int(f_end.y - f_beg.y - 0.5);

	// Workaround to compile the shader with DX11/12.
	// If this isn't done, it will complain that the loop could have too many iterations.
	// This number should be enough to guarantee downscaling from very high to very small resolutions.
	// Note that this number might be referenced in the UI.
	const int max_iterations = 16;

	// Fix up the average calculations in case we reached the upper limit
	x_range = min(x_range, max_iterations);
	y_range = min(y_range, max_iterations);

	// Accumulate top and bottom edge pixels.
	for (int ix = 0; ix < max_iterations; ++ix)
	{
		if (ix < x_range)
		{
			float x = f_beg.x + 1.0 + float(ix);
			temp_color = QuickSampleByPixel(float2(x, f_beg.y) + offset, gamma);
			avg_color += area_n * temp_color;
			avg_luminance += area_n * pow(Luminance(temp_color.rgb), luminance_inv_gamma);
			temp_color = QuickSampleByPixel(float2(x, f_end.y) + offset, gamma);
			avg_color += area_s * temp_color;
			avg_luminance += area_s * pow(Luminance(temp_color.rgb), luminance_inv_gamma);
		}
	}

	// Accumulate left and right edge pixels and all the pixels in between.
	for (int iy = 0; iy < max_iterations; ++iy)
	{
		if (iy < y_range)
		{
			float y = f_beg.y + 1.0 + float(iy);
			
			temp_color = QuickSampleByPixel(float2(f_beg.x, y) + offset, gamma);
			avg_color += area_w * temp_color;
			avg_luminance += area_w * pow(Luminance(temp_color.rgb), luminance_inv_gamma);
			temp_color = QuickSampleByPixel(float2(f_end.x, y) + offset, gamma);
			avg_color += area_e * temp_color;
			avg_luminance += area_e * pow(Luminance(temp_color.rgb), luminance_inv_gamma);

			for (int ix = 0; ix < max_iterations; ++ix)
			{
				if (ix < x_range)
				{
					float x = f_beg.x + 1.0 + float(ix);
					temp_color = QuickSampleByPixel(float2(x, y) + offset, gamma);
					avg_color += temp_color;
					avg_luminance += pow(Luminance(temp_color.rgb), luminance_inv_gamma);
				}
			}
		}
	}

	// Compute the area of the pixel box that was sampled.
	float area_corners = area_nw + area_ne + area_sw + area_se;
	float area_edges = float(x_range) * (area_n + area_s) + float(y_range) * (area_w + area_e);
	float area_center = float(x_range) * float(y_range);
	
	float4 nrm_color = avg_color / (area_corners + area_edges + area_center);
	float target_nrm_color_luminance = avg_luminance / (area_corners + area_edges + area_center);

#if PS_HDR
	// Restore the averaged "gamma" space luminance, for better gamma correction.
	// This retains the best feature of gamma correct sampling (no hue shifts),
	// while also maintaining the perceptual "brightness" level of blending two colors with an alpha
	// (in linear space a 0.5 alpha won't produce a color that has a perceptual brightness in the middle point of the two source colors).
	float nrm_color_luminance = Luminance(nrm_color.rgb);
	if (nrm_color_luminance != 0.0)
	{
		nrm_color.rgb *= pow(target_nrm_color_luminance, luminance_gamma) / nrm_color_luminance;
	}
#endif
	
	// Return the normalized average color.
	return nrm_color;
}

PS_OUTPUT ps_automagical_supersampling(PS_INPUT input)
{
	PS_OUTPUT output;

#if 1 //TODO: ...
	float source_gamma = 2.2f;
#if PS_HDR
	source_gamma = 1.f;
#endif
	output.c = AreaSampling(input.t, source_gamma);
#else
	float2 ratio = (u_source_size / u_target_size) * 0.5;
	float2 steps = floor(ratio);
	float3 col = sample_c(input.t).rgb;
	float div = 1;

	for (float y = 0; y < steps.y; y++)
	{
		for (float x = 0; x < steps.x; x++)
		{
			float2 offset = float2(x,y) - ratio * 0.5;
			col += sample_c(input.t + offset * u_rcp_source_resolution * 2.0).rgb;
			div++;
		}
	}
	
	output.c = float4(col / div, 1);
#endif

	return EncodeOutput(output);
}
