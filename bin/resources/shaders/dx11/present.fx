// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
	float u_time;
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

PS_OUTPUT ps_copy(PS_INPUT input)
{
	PS_OUTPUT output;

	output.c = sample_c(input.t);

	return output;
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

	return output;
}

PS_OUTPUT ps_filter_diagonal(PS_INPUT input)
{
	PS_OUTPUT output;

	uint4 p = (uint4)input.p;

	output.c = ps_crt(input, (p.x + (p.y % 3)) % 3);

	return output;
}

PS_OUTPUT ps_filter_triangular(PS_INPUT input)
{
	PS_OUTPUT output;

	uint4 p = (uint4)input.p;

	// output.c = ps_crt(input, ((p.x + (p.y & 1) * 3) >> 1) % 3);
	output.c = ps_crt(input, ((p.x + ((p.y >> 1) & 1) * 3) >> 1) % 3);

	return output;
}

static const float PI = 3.14159265359f;
PS_OUTPUT ps_filter_complex(PS_INPUT input) // triangular
{
	PS_OUTPUT output;

	float2 texdim; 
	Texture.GetDimensions(texdim.x, texdim.y);

	output.c = (0.9 - 0.4 * cos(2 * PI * input.t.y * texdim.y)) * sample_c(float2(input.t.x, (floor(input.t.y * texdim.y) + 0.5) / texdim.y));

	return output;
}

//Lottes CRT
#define MaskingType 4                      //[1|2|3|4] The type of CRT shadow masking used. 1: compressed TV style, 2: Aperture-grille, 3: Stretched VGA style, 4: VGA style.
#define ScanBrightness -8.00               //[-16.0 to 1.0] The overall brightness of the scanline effect. Lower for darker, higher for brighter.
#define FilterCRTAmount -1.00              //[-4.0 to 1.0] The amount of filtering used, to replicate the TV CRT look. Lower for less, higher for more.
#define HorizontalWarp 0.00                //[0.0 to 0.1] The distortion warping effect for the horizontal (x) axis of the screen. Use small increments.
#define VerticalWarp 0.00                  //[0.0 to 0.1] The distortion warping effect for the verticle (y) axis of the screen. Use small increments.
#define MaskAmountDark 0.80                //[0.0 to 1.0] The value of the dark masking line effect used. Lower for darker lower end masking, higher for brighter.
#define MaskAmountLight 1.50               //[0.0 to 2.0] The value of the light masking line effect used. Lower for darker higher end masking, higher for brighter.
#define ResolutionScale 2.00               //[0.1 to 4.0] The scale of the image resolution. Lowering this can give off a nice retro TV look. Raising it can clear up the image.
#define MaskResolutionScale 0.80           //[0.1 to 2.0] The scale of the CRT mask resolution. Use this for balancing the scanline mask scale for difference resolution scaling.
#define UseShadowMask 1                    //[0 or 1] Enables, or disables the use of the CRT shadow mask. 0 is disabled, 1 is enabled.

float ToLinear1(float c)
{
	c = saturate(c);
	return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

float3 ToLinear(float3 c)
{
	return float3(ToLinear1(c.r), ToLinear1(c.g), ToLinear1(c.b));
}

float ToSrgb1(float c)
{
	c = saturate(c);
	return c < 0.0031308 ? c * 12.92 : 1.055 * pow(c, 0.41666) - 0.055;
}

float3 ToSrgb(float3 c)
{
	return float3(ToSrgb1(c.r), ToSrgb1(c.g), ToSrgb1(c.b));
}

float3 Fetch(float2 pos, float2 off)
{
	float2 screenSize = u_source_resolution;
	float2 res = (screenSize * ResolutionScale);
	pos = round(pos * res + off) / res;
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
	float2 crtRes = u_rcp_target_resolution;
	float2 res = (crtRes * MaskResolutionScale);
	pos = (pos * res);

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

	return (b * wb) + (c * wc) + (d * wd) / (wb + wc + wd);
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

	return (a * wa) + (b * wb) + (c * wc) + (d * wd) + (e * we) / (wa + wb + wc + wd + we);
}

// Return scanline weight.
float Scan(float2 pos, float off)
{
	float dst = Dist(pos).y;
	return Gaus(dst + off, ScanBrightness);
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
	fragcoord -= u_target_rect;
	float2 inSize = u_target_resolution - (2 * u_target_rect.xy);
	float4 color;
	float2 pos = Warp(fragcoord.xy / inSize);

#if UseShadowMask == 0
	color.rgb = Tri(pos);
#else
	color.rgb = Tri(pos) * Mask(fragcoord.xy);
#endif
	color.rgb = ToSrgb(color.rgb);
	color.a = 1.0;

	return color;
}

PS_OUTPUT ps_filter_lottes(PS_INPUT input)
{
	PS_OUTPUT output;
	output.c = LottesCRTPass(input.p);

	return output;
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
	return output;
}

PS_OUTPUT ps_automagical_supersampling(PS_INPUT input)
{
	PS_OUTPUT output;

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
	return output;
}
