/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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

//#version 420 // Keep it for editor detection


#ifdef VERTEX_SHADER

layout(location = 0) in vec2 POSITION;
layout(location = 1) in vec2 TEXCOORD0;
layout(location = 7) in vec4 COLOR;

// FIXME set the interpolation (don't know what dx do)
// flat means that there is no interpolation. The value given to the fragment shader is based on the provoking vertex conventions.
//
// noperspective means that there will be linear interpolation in window-space. This is usually not what you want, but it can have its uses.
//
// smooth, the default, means to do perspective-correct interpolation.
//
// The centroid qualifier only matters when multisampling. If this qualifier is not present, then the value is interpolated to the pixel's center, anywhere in the pixel, or to one of the pixel's samples. This sample may lie outside of the actual primitive being rendered, since a primitive can cover only part of a pixel's area. The centroid qualifier is used to prevent this; the interpolation point must fall within both the pixel's area and the primitive's area.
out vec4 PSin_p;
out vec2 PSin_t;
out vec4 PSin_c;

void vs_main()
{
	PSin_p = vec4(POSITION, 0.5f, 1.0f);
	PSin_t = TEXCOORD0;
	PSin_c = COLOR;
	gl_Position = vec4(POSITION, 0.5f, 1.0f); // NOTE I don't know if it is possible to merge POSITION_OUT and gl_Position
}

#endif

#ifdef FRAGMENT_SHADER

uniform vec4 u_source_rect;
uniform vec4 u_target_rect;
uniform vec2 u_source_size;
uniform vec2 u_target_size;
uniform vec2 u_target_resolution;
uniform vec2 u_rcp_target_resolution; // 1 / u_target_resolution
uniform vec2 u_source_resolution;
uniform vec2 u_rcp_source_resolution; // 1 / u_source_resolution
uniform float u_time;

in vec4 PSin_p;
in vec2 PSin_t;
in vec4 PSin_c;

layout(binding = 0) uniform sampler2D TextureSampler;

layout(location = 0) out vec4 SV_Target0;

vec4 sample_c()
{
	return texture(TextureSampler, PSin_t);
}

vec4 sample_c(vec2 uv)
{
	return texture(TextureSampler, uv);
}

vec4 ps_crt(uint i)
{
	vec4 mask[4] = vec4[4](
		vec4(1, 0, 0, 0),
		vec4(0, 1, 0, 0),
		vec4(0, 0, 1, 0),
		vec4(1, 1, 1, 0));
	return sample_c() * clamp((mask[i] + 0.5f), 0.0f, 1.0f);
}

#ifdef ps_copy
void ps_copy()
{
	SV_Target0 = sample_c();
}
#endif

#ifdef ps_filter_scanlines
vec4 ps_scanlines(uint i)
{
	vec4 mask[2] = vec4[2]
	(
		vec4(1, 1, 1, 0),
		vec4(0, 0, 0, 0)
	);

	return sample_c() * clamp((mask[i] + 0.5f), 0.0f, 1.0f);
}

void ps_filter_scanlines() // scanlines
{
	highp uvec4 p = uvec4(gl_FragCoord);

	vec4 c = ps_scanlines(p.y % 2u);

	SV_Target0 = c;
}
#endif

#ifdef ps_filter_diagonal
void ps_filter_diagonal() // diagonal
{
	highp uvec4 p = uvec4(gl_FragCoord);

	vec4 c = ps_crt((p.x + (p.y % 3u)) % 3u);

	SV_Target0 = c;
}
#endif

#ifdef ps_filter_triangular
void ps_filter_triangular() // triangular
{
	highp uvec4 p = uvec4(gl_FragCoord);

	vec4 c = ps_crt(((p.x + ((p.y >> 1u) & 1u) * 3u) >> 1u) % 3u);

	SV_Target0 = c;
}
#endif

#ifdef ps_filter_complex
void ps_filter_complex()
{
	const float PI = 3.14159265359f;
	vec2 texdim = vec2(textureSize(TextureSampler, 0));
	float factor = (0.9f - 0.4f * cos(2.0f * PI * PSin_t.y * texdim.y));
	vec4 c = factor * texture(TextureSampler, vec2(PSin_t.x, (floor(PSin_t.y * texdim.y) + 0.5f) / texdim.y));

	SV_Target0 = c;
}
#endif

#ifdef ps_filter_lottes

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

#define saturate(x) clamp(x, 0.0, 1.0)

float ToLinear1(float c)
{
	c = saturate(c);
	return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

vec3 ToLinear(vec3 c)
{
	return vec3(ToLinear1(c.r), ToLinear1(c.g), ToLinear1(c.b));
}

float ToSrgb1(float c)
{
	c = saturate(c);
	return c < 0.0031308 ? c * 12.92 : 1.055 * pow(c, 0.41666) - 0.055;
}

vec3 ToSrgb(vec3 c)
{
	return vec3(ToSrgb1(c.r), ToSrgb1(c.g), ToSrgb1(c.b));
}

vec3 Fetch(vec2 pos, vec2 off)
{
	vec2 screenSize = u_source_resolution;
	vec2 res = (screenSize * ResolutionScale);
	pos = round(pos * res + off) / res;
	if (max(abs(pos.x - 0.5), abs(pos.y - 0.5)) > 0.5)
	{
		return vec3(0.0, 0.0, 0.0);
	}
	else
	{
		return ToLinear(texture(TextureSampler, pos.xy).rgb);
	}
}

vec2 Dist(vec2 pos)
{
	vec2 crtRes = u_rcp_target_resolution;
	vec2 res = (crtRes * MaskResolutionScale);
	pos = (pos * res);

	return -((pos - floor(pos)) - vec2(0.5, 0.5));
}

float Gaus(float pos, float scale)
{
	return exp2(scale * pos * pos);
}

vec3 Horz3(vec2 pos, float off)
{
	vec3 b = Fetch(pos, vec2(-1.0, off));
	vec3 c = Fetch(pos, vec2(0.0, off));
	vec3 d = Fetch(pos, vec2(1.0, off));
	float dst = Dist(pos).x;

	// Convert distance to weight.
	float scale = FilterCRTAmount;
	float wb = Gaus(dst - 1.0, scale);
	float wc = Gaus(dst + 0.0, scale);
	float wd = Gaus(dst + 1.0, scale);

	return (b * wb) + (c * wc) + (d * wd) / (wb + wc + wd);
}

vec3 Horz5(vec2 pos, float off)
{
	vec3 a = Fetch(pos, vec2(-2.0, off));
	vec3 b = Fetch(pos, vec2(-1.0, off));
	vec3 c = Fetch(pos, vec2(0.0, off));
	vec3 d = Fetch(pos, vec2(1.0, off));
	vec3 e = Fetch(pos, vec2(2.0, off));
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
float Scan(vec2 pos, float off)
{
	float dst = Dist(pos).y;
	return Gaus(dst + off, ScanBrightness);
}

vec3 Tri(vec2 pos)
{
	vec3 a = Horz3(pos, -1.0);
	vec3 b = Horz5(pos, 0.0);
	vec3 c = Horz3(pos, 1.0);

	float wa = Scan(pos, -1.0);
	float wb = Scan(pos, 0.0);
	float wc = Scan(pos, 1.0);

	return (a * wa) + (b * wb) + (c * wc);
}

vec2 Warp(vec2 pos)
{
	pos = pos * 2.0 - 1.0;
	pos *= vec2(1.0 + (pos.y * pos.y) * HorizontalWarp, 1.0 + (pos.x * pos.x) * VerticalWarp);
	return pos * 0.5 + 0.5;
}

vec3 Mask(vec2 pos)
{
#if MaskingType == 1
	// Very compressed TV style shadow mask.
	float lines = MaskAmountLight;
	float odd = 0.0;

	if (fract(pos.x / 6.0) < 0.5)
	{
		odd = 1.0;
	}
	if (fract((pos.y + odd) / 2.0) < 0.5)
	{
		lines = MaskAmountDark;
	}
	pos.x = fract(pos.x / 3.0);
	vec3 mask = vec3(MaskAmountDark, MaskAmountDark, MaskAmountDark);

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
	pos.x = fract(pos.x / 3.0);
	vec3 mask = vec3(MaskAmountDark, MaskAmountDark, MaskAmountDark);

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
	vec3 mask = vec3(MaskAmountDark, MaskAmountDark, MaskAmountDark);
	pos.x = fract(pos.x / 6.0);

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
	pos.xy = floor(pos.xy * vec2(1.0, 0.5));
	pos.x += pos.y * 3.0;

	vec3 mask = vec3(MaskAmountDark, MaskAmountDark, MaskAmountDark);
	pos.x = fract(pos.x / 6.0);

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

vec4 LottesCRTPass()
{
	//flipped y axis in opengl
	vec2 fragcoord = vec2(gl_FragCoord.x, u_target_resolution.y - gl_FragCoord.y) - u_target_rect.xy;
	vec4 color;
	vec2 inSize = u_target_resolution - (2.0 * u_target_rect.xy);

	vec2 pos = Warp(fragcoord.xy / inSize);

#if UseShadowMask == 0
	color.rgb = Tri(pos);
#else
	color.rgb = Tri(pos) * Mask(fragcoord.xy);
#endif
	color.rgb = ToSrgb(color.rgb);
	color.a = 1.0;

	return color;
}

void ps_filter_lottes()
{
	SV_Target0 = LottesCRTPass();
}

#endif

#ifdef ps_4x_rgss
void ps_4x_rgss()
{
	vec2 dxy = vec2(dFdx(PSin_t.x), dFdy(PSin_t.y));
	vec3 color = vec3(0);

	float s = 1.0/8.0;
	float l = 3.0/8.0;

	color += sample_c(PSin_t + vec2( s, l) * dxy).rgb;
	color += sample_c(PSin_t + vec2( l,-s) * dxy).rgb;
	color += sample_c(PSin_t + vec2(-s,-l) * dxy).rgb;
	color += sample_c(PSin_t + vec2(-l, s) * dxy).rgb;

	SV_Target0 = vec4(color * 0.25,1);
}
#endif

#ifdef ps_automagical_supersampling
void ps_automagical_supersampling()
{
	vec2 ratio = (u_source_size / u_target_size) * 0.5;
	vec2 steps = floor(ratio);
	vec3 col = sample_c(PSin_t).rgb;
	float div = 1;

	for (float y = 0; y < steps.y; y++)
	{
		for (float x = 0; x < steps.x; x++)
		{
			vec2 offset = vec2(x,y) - ratio * 0.5;
			col += sample_c(PSin_t + offset * u_rcp_source_resolution * 2.0).rgb;
			div++;
		}
	}

	SV_Target0 = vec4(col / div, 1);
}
#endif

#endif
