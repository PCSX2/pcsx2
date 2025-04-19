// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

//#version 420 // Keep it for editor detection

#ifndef PS_HDR_INPUT
#define PS_HDR_INPUT 0
#endif
#ifndef PS_HDR_OUTPUT
#define PS_HDR_OUTPUT 0
#endif

// SMPTE 170M - BT.601 (NTSC-M) -> BT.709
mat3 from_NTSCM = transpose(mat3(
	0.939497225737661, 0.0502268452914346, 0.0102759289709032,
	0.0177558637510127, 0.965824605885027, 0.0164195303639603,
	-0.00162163209967010, -0.00437400622653655, 1.00599563832621));

// ARIB TR-B9 (9300K+27MPCD with chromatic adaptation) (NTSC-J) -> BT.709
mat3 from_NTSCJ = transpose(mat3(
	0.823613036967492, -0.0943227111084757, 0.00799341532931119,
	0.0289258355537324, 1.02310733489462, 0.00243547111576797,
	-0.00569501554980891, 0.0161828357559315, 1.22328453915712));

// EBU - BT.470BG/BT.601 (PAL) -> BT.709
mat3 from_PAL = transpose(mat3(
	1.04408168421813, -0.0440816842181253, 0.000000000000000,
	0.000000000000000, 1.00000000000000, 0.000000000000000,
	0.000000000000000, 0.0118044782106489, 0.988195521789351));
   
// Applies exponential ("Photographic") luminance/luma compression.
float RangeCompress(float X)
{
	// Branches are for static parameters optimizations
	// This does e^X. We expect X to be between 0 and 1.
	return 1.f - exp(-X);
}

// Refurbished DICE HDR tonemapper (per channel or luminance).
float LuminanceCompress(
  float InValue,
  float OutMaxValue,
  float ShoulderStart /*= 0.f*/)
{
	const float compressableValue = InValue - ShoulderStart;
	const float compressedRange = OutMaxValue - ShoulderStart;
	const float possibleOutValue = ShoulderStart + compressedRange * RangeCompress(compressableValue / compressedRange);
	return (InValue <= ShoulderStart) ? InValue : possibleOutValue;
}

#ifdef VERTEX_SHADER

layout(location = 0) in vec4 a_pos;
layout(location = 1) in vec2 a_tex;

layout(location = 0) out vec2 v_tex;

void main()
{
	gl_Position = vec4(a_pos.x, -a_pos.y, a_pos.z, a_pos.w);
	v_tex = a_tex;
}

#endif

/*
** Contrast, saturation, brightness
** Code of this function is from TGM's shader pack
** http://irrlicht.sourceforge.net/phpBB2/viewtopic.php?t=21057
** TGM's author comment about the license (included in the previous link)
** "do with it, what you want! its total free!
** (but would be nice, if you say that you used my shaders  :wink: ) but not necessary"
*/

#ifdef FRAGMENT_SHADER

layout(push_constant) uniform cb0
{
   vec4 correction;
	vec4 adjustment;
};

layout(set = 0, binding = 0) uniform sampler2D samp0;
layout(location = 0) in vec2 v_tex;
layout(location = 0) out vec4 o_col0;

// For all settings: 1.0 = 100% 0.5=50% 1.5 = 150%
vec4 ContrastSaturationBrightness(vec4 color)
{
    float brt = adjustment.x;
    float con = adjustment.y;
    float sat = adjustment.z;
    
#if 1 // For linear space in/out
    vec3 AvgLumin = vec3(0.18); // Mid gray
#else
    // Increase or decrease these values to adjust r, g and b color channels separately
    const float AvgLumR = 0.5;
    const float AvgLumG = 0.5;
    const float AvgLumB = 0.5;
    vec3 AvgLumin = vec3(AvgLumR, AvgLumG, AvgLumB);
#endif

    const vec3 LumCoeff = vec3(0.2125, 0.7154, 0.0721); // Rec.709

    vec3 brtColor = color.rgb * brt;
    float dot_intensity = dot(brtColor, LumCoeff);
    vec3 intensity = vec3(dot_intensity, dot_intensity, dot_intensity);
    vec3 satColor = mix(intensity, brtColor, sat);
    vec3 conColor = mix(AvgLumin, satColor, con);

    color.rgb = conColor;
    return color;
}

void main()
{
    vec4 c = texture(samp0, v_tex);
    
#if PS_HDR_INPUT
	// Tonemap in gamma space (this specific formula looks better with it) and by channel, to best retain the original color hues.
	// Theoretically tonemapping should be done in the color space of the output display (e.g. BT.2020 in HDR and BT.709 in SDR),
	// because displays usually clip individual rgb values to the peak brightness value of HDR,
	// but for simplicity, we do it in the raw game color space.
	
	// In HDR, we only compress the range above SDR (1), in SDR, we compress the top 20% range, to avoid clipping and retain HDR detail.
	float shoulderStart = 1.f;
#if !PS_HDR_OUTPUT
	shoulderStart = 0.8f;
#endif
	
	float peakWhite = correction.w;
	
	c.r = LuminanceCompress(c.r, peakWhite, shoulderStart);
	c.g = LuminanceCompress(c.g, peakWhite, shoulderStart);
	c.b = LuminanceCompress(c.b, peakWhite, shoulderStart);
#endif

    // Linearize
    c.rgb = pow(abs(c.rgb), vec3(correction.x)) * sign(c.rgb);

#if PS_HDR_OUTPUT && 0 // Print HDR colors
    if (any(c.rgb > 1.0))
    {
        c.rgb = vec3(1, 0, 1);
    }
#endif

    // Convert to BT.709 from the user specified game color space
    if (correction.y == 1.f)
    {
        c.rgb = c.rgb  * from_NTSCM;
    }
    else if (correction.y == 2.f)
    {
        c.rgb = c.rgb * from_NTSCJ;
    }
    else if (correction.y == 3.f)
    {
        c.rgb = c.rgb * from_PAL;
    }

    float HDRPaperWhite = correction.z;

#if PS_HDR_OUTPUT && 0 // AutoHDR
    c.rgb = PumboAutoHDR(c.rgb, 750.0, HDRPaperWhite * 80.0);
#endif

    c = ContrastSaturationBrightness(c);

#if 0 // Moved to presentation
    c.rgb *= HDRPaperWhite;
#endif

#if PS_HDR_OUTPUT
    // Leave as linear, for scRGB HDR
#else
    // Convert to Gamma 2.2 (not sRGB)
    c.rgb = pow(max(c.rgb, vec3(0.0)), vec3(1.0 / 2.2));
#endif

    o_col0 = c;
}


#endif
