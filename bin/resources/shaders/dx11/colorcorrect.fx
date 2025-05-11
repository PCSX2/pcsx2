// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#ifndef PS_HDR_INPUT
#define PS_HDR_INPUT 0
#endif
#ifndef PS_HDR_OUTPUT
#define PS_HDR_OUTPUT 0
#endif

//TODO
#define PS_HDR_TEST 0

Texture2D Texture;
SamplerState Sampler;

cbuffer cb0 : register(b0)
{
	float4 correction;
	float4 adjustment;
};

// SMPTE 170M - BT.601 (NTSC-M) -> BT.709
static const float3x3 from_NTSCM = float3x3(
	0.939497225737661, 0.0502268452914346, 0.0102759289709032,
	0.0177558637510127, 0.965824605885027, 0.0164195303639603,
	-0.00162163209967010, -0.00437400622653655, 1.00599563832621);

// ARIB TR-B9 (9300K+27MPCD with chromatic adaptation) (NTSC-J) -> BT.709
static const float3x3 from_NTSCJ = float3x3(
	0.823613036967492, -0.0943227111084757, 0.00799341532931119,
	0.0289258355537324, 1.02310733489462, 0.00243547111576797,
	-0.00569501554980891, 0.0161828357559315, 1.22328453915712);

// EBU - BT.470BG/BT.601 (PAL) -> BT.709
static const float3x3 from_PAL = float3x3(
	1.04408168421813, -0.0440816842181253, 0.000000000000000,
	0.000000000000000, 1.00000000000000, 0.000000000000000,
	0.000000000000000, 0.0118044782106489, 0.988195521789351);

static const float3x3 BT709_2_BT2020 = float3x3(
	0.627403914928436279296875f, 0.3292830288410186767578125f, 0.0433130674064159393310546875f,
	0.069097287952899932861328125f, 0.9195404052734375f, 0.011362315155565738677978515625f,
	0.01639143936336040496826171875f, 0.08801330626010894775390625f, 0.895595252513885498046875f);

static const float3x3 BT2020_2_BT709 = float3x3(
	1.66049098968505859375f, -0.58764111995697021484375f, -0.072849862277507781982421875f,
	-0.12455047667026519775390625f, 1.13289988040924072265625f, -0.0083494223654270172119140625f,
	-0.01815076358616352081298828125f, -0.100578896701335906982421875f, 1.11872971057891845703125f);

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
  float ShoulderStart = 0.f)
{
	const float compressableValue = InValue - ShoulderStart;
	const float compressedRange = OutMaxValue - ShoulderStart;
	const float possibleOutValue = ShoulderStart + compressedRange * RangeCompress(compressableValue / compressedRange);
	return (InValue <= ShoulderStart) ? InValue : possibleOutValue;
}

/*
** Contrast, saturation, brightness
** Code of this function is from TGM's shader pack
** http://irrlicht.sourceforge.net/phpBB2/viewtopic.php?t=21057
*/

// For all settings: 1.0 = 100% 0.5=50% 1.5 = 150% 
float4 ContrastSaturationBrightness(float4 color) // Ported to HLSL
{
	float brt = adjustment.x;
	float con = adjustment.y;
	float sat = adjustment.z;
		
	const float3 LumCoeff = float3(0.2125, 0.7154, 0.0721);
	
#if 1 // For linear space in/out
	float3 AvgLumin = 0.18; // Mid gray
#else
	// Increase or decrease these values to adjust r, g and b color channels separately
	const float AvgLumR = 0.5;
	const float AvgLumG = 0.5;
	const float AvgLumB = 0.5;
	float3 AvgLumin = float3(AvgLumR, AvgLumG, AvgLumB);
#endif
	float3 brtColor = color.rgb * brt;
	float intensity = dot(brtColor, LumCoeff);
	float3 satColor = lerp(intensity, brtColor, sat);
	float3 conColor = lerp(AvgLumin, satColor, con);

	color.rgb = conColor;	
	return color;
}

struct PS_INPUT
{
	float4 p : SV_Position;
	float2 t : TEXCOORD0;
};

// AdvancedAutoHDR pass to generate some HDR brightness out of an SDR signal.
// This is hue conserving and only really affects highlights.
// "SDRColor" is meant to be in "SDR range" (linear), as in, a value of 1 matching SDR white (something between 80, 100, 203, 300 nits, or whatever else)
// From: https://github.com/Filoppi/PumboAutoHDR (MIT)
float3 PumboAutoHDR(float3 SDRColor, float PeakWhiteNits = 400.f, float PaperWhiteNits = 203.f, float ShoulderPow = 3.5f)
{
	const float3 LumCoeff = float3(0.2125, 0.7154, 0.0721);
	const float SDRRatio = dot(SDRColor, LumCoeff);
	// Limit AutoHDR brightness, it won't look good beyond a certain level.
	// The paper white multiplier is applied later so we account for that.
	const float AutoHDRMaxWhite = max(min(PeakWhiteNits, 1000.f) / PaperWhiteNits, 1.f);
	const float AutoHDRShoulderRatio = saturate(SDRRatio);
	const float AutoHDRExtraRatio = pow(max(AutoHDRShoulderRatio, 0.f), ShoulderPow) * (AutoHDRMaxWhite - 1.f);
	const float AutoHDRTotalRatio = SDRRatio + AutoHDRExtraRatio;
	return SDRColor * (SDRRatio > 0.f ? (AutoHDRTotalRatio / SDRRatio) : 1.f);
}

float4 ps_main(PS_INPUT input) : SV_Target0
{
	float4 c = Texture.Sample(Sampler, input.t);
	
#if PS_HDR_INPUT // Clamp negative colors, they weren't meant to be
	c.rgb = max(c.rgb, 0.f);
#endif
	
	// Linearize
	c.rgb = pow(abs(c.rgb), correction.x) * sign(c.rgb);
		
#if PS_HDR_OUTPUT && 0 // Print HDR colors
	if (any(c.rgb > 1.0))
	{
		c.rgb = float3(1, 0, 1);
	}
#endif

	// Convert to BT.709 from the user specified game color space
	if (correction.y == 1.f)
	{
		c.rgb = mul(c.rgb, from_NTSCM);
	}
	else if (correction.y == 2.f)
	{
		c.rgb = mul(c.rgb, from_NTSCJ);
	}
	else if (correction.y == 3.f)
	{
		c.rgb = mul(c.rgb, from_PAL);
	}

//TODO: expose setting? In VK too. Also... clean this all around (move define, remove branches)
#define PS_FAKE_HDR 1
#if PS_HDR_OUTPUT && PS_FAKE_HDR
	// If the game doesn't have many bright highlights, the dynamic range is relatively low, this helps alleviate that
#if PS_HDR_INPUT // "Fake" HDR
	const float normalizationPoint = 0.333; // Found empyrically
	const float fakeHDRIntensity = 0.25; // Found empyrically
	const float3 LumCoeff = float3(0.2125, 0.7154, 0.0721);
	float cLuminanceOriginal = dot(c.rgb, LumCoeff);
	float cLuminance = cLuminanceOriginal / normalizationPoint;
	cLuminance = cLuminance > 1.0 ? pow(cLuminance, 1.0 + fakeHDRIntensity) : cLuminance;
	cLuminance *= normalizationPoint;
	if (input.t.x >= 0.5f || !PS_HDR_TEST)
	c.rgb *= cLuminanceOriginal == 0.f ? 1.f : (cLuminance / cLuminanceOriginal);
#else // AutoHDR
	float HDRPaperWhite = correction.z;
	if (input.t.x >= 0.5f || !PS_HDR_TEST)
	c.rgb = PumboAutoHDR(c.rgb, 750.0, HDRPaperWhite * 80.0);
#endif
	if (input.t.x <= 0.5f && PS_HDR_TEST)
		c.rgb = saturate(c.rgb);
#endif
	
#if PS_HDR_INPUT // Display map
	// In HDR, we only compress the range above SDR (1), in SDR, we compress the top 20% range, to avoid clipping and retain HDR detail.
	float shoulderStart = 1.f;
#if PS_HDR_OUTPUT
	// Tonemapping should be done in the color space of the output display (e.g. BT.2020 in HDR and BT.709 in SDR),
	// because displays usually clip individual rgb values to the peak brightness value of HDR.
	c.rgb = mul(BT709_2_BT2020, c.rgb);
#else
	shoulderStart = 0.8f;
#endif
	
	float peakWhite = correction.w;
	
	// Tonemap in gamma space (this specific formula looks better with it) and by channel, to best retain the original color hues (even if we do it in a different color space).
	const float tonemapGamma = 2.75; // Found empyrically, any value between 1 and 3 is good, with 2-3 being especially good
	c.rgb = pow(abs(c.rgb), 1.0 / tonemapGamma) * sign(c.rgb);
	peakWhite = pow(peakWhite, 1.0 / tonemapGamma);
	
	c.r = LuminanceCompress(c.r, peakWhite, shoulderStart);
	c.g = LuminanceCompress(c.g, peakWhite, shoulderStart);
	c.b = LuminanceCompress(c.b, peakWhite, shoulderStart);
	
	c.rgb = pow(abs(c.rgb), tonemapGamma) * sign(c.rgb);
	
#if PS_HDR_OUTPUT
	c.rgb = mul(BT2020_2_BT709, c.rgb);
#endif
#endif
	
	c = ContrastSaturationBrightness(c);
	
#if PS_HDR_OUTPUT
	// Leave as linear, for scRGB HDR
#else
	// Convert to Gamma 2.2 (not sRGB)
	c.rgb = pow(max(c.rgb, 0.0), 1.0 / 2.2);
#endif
	
	return c;
}
