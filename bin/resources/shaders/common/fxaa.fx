// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#ifndef FXAA_HLSL_5
    #define FXAA_HLSL_5 0
#endif
#ifndef FXAA_GLSL_130
    #define FXAA_GLSL_130 0
#endif
#ifndef FXAA_GLSL_VK
    #define FXAA_GLSL_VK 0
#endif

#define UHQ_FXAA 1          //High Quality Fast Approximate Anti Aliasing. Adapted for GS from Timothy Lottes FXAA 3.11.
#define FxaaSubpixMax 0.0   //[0.00 to 1.00] Amount of subpixel aliasing removal. 0.00: Edge only antialiasing (no blurring)
#define FxaaEarlyExit 1     //[0 or 1] Use Fxaa early exit pathing. When disabled, the entire scene is antialiased(FSAA). 0 is off, 1 is on.

/*------------------------------------------------------------------------------
							 [GLOBALS|FUNCTIONS]
------------------------------------------------------------------------------*/
#if (FXAA_GLSL_130 == 1)

in vec2 PSin_t;

layout(location = 0) out vec4 SV_Target0;
layout(binding = 0) uniform sampler2D TextureSampler;

#elif (FXAA_GLSL_VK == 1)

layout(location = 0) in vec2 PSin_t;
layout(location = 0) out vec4 SV_Target0;
layout(set = 0, binding = 0) uniform sampler2D TextureSampler;

#elif (FXAA_HLSL_5 == 1)
Texture2D Texture : register(t0);
SamplerState TextureSampler : register(s0);

struct VS_INPUT
{
	float4 p : POSITION;
	float2 t : TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 p : SV_Position;
	float2 t : TEXCOORD0;
};

struct PS_OUTPUT
{
	float4 c : SV_Target0;
};

#elif defined(__METAL_VERSION__)
static constexpr sampler MAIN_SAMPLER(coord::normalized, address::clamp_to_edge, filter::linear);
#endif

/*------------------------------------------------------------------------------
                             [FXAA CODE SECTION]
------------------------------------------------------------------------------*/

#if (FXAA_HLSL_5 == 1)
struct FxaaTex { SamplerState smpl; Texture2D tex; };
#define FxaaTexTop(t, p) t.tex.SampleLevel(t.smpl, p, 0.0)
#define FxaaTexOff(t, p, o, r) t.tex.SampleLevel(t.smpl, p, 0.0, o)
#define FxaaTexAlpha4(t, p) t.tex.GatherAlpha(t.smpl, p)
#define FxaaTexOffAlpha4(t, p, o) t.tex.GatherAlpha(t.smpl, p, o)
#define FxaaDiscard clip(-1)
#define FxaaSat(x) saturate(x)

#elif (FXAA_GLSL_130 == 1 || FXAA_GLSL_VK == 1)
#define int2 ivec2
#define float2 vec2
#define float3 vec3
#define float4 vec4
#define FxaaDiscard discard
#define FxaaSat(x) clamp(x, 0.0, 1.0)
#define FxaaTex sampler2D
#define FxaaTexTop(t, p) textureLod(t, p, 0.0)
#define FxaaTexOff(t, p, o, r) textureLodOffset(t, p, 0.0, o)

#elif defined(__METAL_VERSION__)
#define FxaaTex texture2d<float>
#define FxaaTexTop(t, p) t.sample(MAIN_SAMPLER, p)
#define FxaaTexOff(t, p, o, r) t.sample(MAIN_SAMPLER, p, o)
#define FxaaTexAlpha4(t, p) t.gather(MAIN_SAMPLER, p, 0, component::w)
#define FxaaTexOffAlpha4(t, p, o) t.gather(MAIN_SAMPLER, p, o, component::w)
#define FxaaDiscard discard_fragment()
#define FxaaSat(x) saturate(x)
#endif

#define FxaaEdgeThreshold 0.063
#define FxaaEdgeThresholdMin 0.00
#define FXAA_QUALITY_P0 1.0
#define FXAA_QUALITY_P1 1.5
#define FXAA_QUALITY_P2 2.0
#define FXAA_QUALITY_P3 2.0
#define FXAA_QUALITY_P4 2.0
#define FXAA_QUALITY_P5 2.0
#define FXAA_QUALITY_P6 2.0
#define FXAA_QUALITY_P7 2.0
#define FXAA_QUALITY_P8 2.0
#define FXAA_QUALITY_P9 2.0
#define FXAA_QUALITY_P10 4.0
#define FXAA_QUALITY_P11 8.0
#define FXAA_QUALITY_P12 8.0

/*------------------------------------------------------------------------------
                        [GAMMA PREPASS CODE SECTION]
------------------------------------------------------------------------------*/
float RGBLuminance(float3 color)
{
	const float3 lumCoeff = float3(0.2126729, 0.7151522, 0.0721750);
	return dot(color.rgb, lumCoeff);
}

float3 RGBGammaToLinear(float3 color, float gamma)
{
	color = FxaaSat(color);
	color.r = (color.r <= 0.0404482362771082) ?
	color.r / 12.92 : pow((color.r + 0.055) / 1.055, gamma);
	color.g = (color.g <= 0.0404482362771082) ?
	color.g / 12.92 : pow((color.g + 0.055) / 1.055, gamma);
	color.b = (color.b <= 0.0404482362771082) ?
	color.b / 12.92 : pow((color.b + 0.055) / 1.055, gamma);

	return color;
}

float3 LinearToRGBGamma(float3 color, float gamma)
{
	color = FxaaSat(color);
	color.r = (color.r <= 0.00313066844250063) ?
	color.r * 12.92 : 1.055 * pow(color.r, 1.0 / gamma) - 0.055;
	color.g = (color.g <= 0.00313066844250063) ?
	color.g * 12.92 : 1.055 * pow(color.g, 1.0 / gamma) - 0.055;
	color.b = (color.b <= 0.00313066844250063) ?
	color.b * 12.92 : 1.055 * pow(color.b, 1.0 / gamma) - 0.055;

	return color;
}

float4 PreGammaPass(float4 color)
{
	const float GammaConst = 2.233;
	color.rgb = RGBGammaToLinear(color.rgb, GammaConst);
	color.rgb = LinearToRGBGamma(color.rgb, GammaConst);
	color.a = RGBLuminance(color.rgb);

	return color;
}


/*------------------------------------------------------------------------------
                        [FXAA CODE SECTION]
------------------------------------------------------------------------------*/

float FxaaLuma(float4 rgba)
{ 
	rgba.w = RGBLuminance(rgba.xyz);
	return rgba.w; 
}

float4 FxaaPixelShader(float2 pos, FxaaTex tex, float2 fxaaRcpFrame, float fxaaSubpix, float fxaaEdgeThreshold, float fxaaEdgeThresholdMin)
{
	float2 posM;
	posM.x = pos.x;
	posM.y = pos.y;

	float4 rgbyM = FxaaTexTop(tex, posM);
	rgbyM.w = RGBLuminance(rgbyM.xyz);
	#define lumaM rgbyM.w

	float lumaS = FxaaLuma(FxaaTexOff(tex, posM, int2( 0, 1), fxaaRcpFrame.xy));
	float lumaE = FxaaLuma(FxaaTexOff(tex, posM, int2( 1, 0), fxaaRcpFrame.xy));
	float lumaN = FxaaLuma(FxaaTexOff(tex, posM, int2( 0,-1), fxaaRcpFrame.xy));
	float lumaW = FxaaLuma(FxaaTexOff(tex, posM, int2(-1, 0), fxaaRcpFrame.xy));

	float maxSM = max(lumaS, lumaM);
	float minSM = min(lumaS, lumaM);
	float maxESM = max(lumaE, maxSM);
	float minESM = min(lumaE, minSM);
	float maxWN = max(lumaN, lumaW);
	float minWN = min(lumaN, lumaW);

	float rangeMax = max(maxWN, maxESM);
	float rangeMin = min(minWN, minESM);
	float range = rangeMax - rangeMin;
	float rangeMaxScaled = rangeMax * fxaaEdgeThreshold;
	float rangeMaxClamped = max(fxaaEdgeThresholdMin, rangeMaxScaled);

	bool earlyExit = range < rangeMaxClamped;
	#if (FxaaEarlyExit == 1)
	if(earlyExit) { return rgbyM; }
	#endif

	float lumaNW = FxaaLuma(FxaaTexOff(tex, posM, int2(-1,-1), fxaaRcpFrame.xy));
	float lumaSE = FxaaLuma(FxaaTexOff(tex, posM, int2( 1, 1), fxaaRcpFrame.xy));
	float lumaNE = FxaaLuma(FxaaTexOff(tex, posM, int2( 1,-1), fxaaRcpFrame.xy));
	float lumaSW = FxaaLuma(FxaaTexOff(tex, posM, int2(-1, 1), fxaaRcpFrame.xy));

	float lumaNS = lumaN + lumaS;
	float lumaWE = lumaW + lumaE;
	float subpixRcpRange = 1.0/range;
	float subpixNSWE = lumaNS + lumaWE;
	float edgeHorz1 = (-2.0 * lumaM) + lumaNS;
	float edgeVert1 = (-2.0 * lumaM) + lumaWE;
	float lumaNESE = lumaNE + lumaSE;
	float lumaNWNE = lumaNW + lumaNE;
	float edgeHorz2 = (-2.0 * lumaE) + lumaNESE;
	float edgeVert2 = (-2.0 * lumaN) + lumaNWNE;

	float lumaNWSW = lumaNW + lumaSW;
	float lumaSWSE = lumaSW + lumaSE;
	float edgeHorz4 = (abs(edgeHorz1) * 2.0) + abs(edgeHorz2);
	float edgeVert4 = (abs(edgeVert1) * 2.0) + abs(edgeVert2);
	float edgeHorz3 = (-2.0 * lumaW) + lumaNWSW;
	float edgeVert3 = (-2.0 * lumaS) + lumaSWSE;
	float edgeHorz = abs(edgeHorz3) + edgeHorz4;
	float edgeVert = abs(edgeVert3) + edgeVert4;

	float subpixNWSWNESE = lumaNWSW + lumaNESE;
	float lengthSign = fxaaRcpFrame.x;
	bool horzSpan = edgeHorz >= edgeVert;
	float subpixA = subpixNSWE * 2.0 + subpixNWSWNESE;
	if(!horzSpan) lumaN = lumaW;
	if(!horzSpan) lumaS = lumaE;
	if(horzSpan) lengthSign = fxaaRcpFrame.y;
	float subpixB = (subpixA * (1.0/12.0)) - lumaM;

	float gradientN = lumaN - lumaM;
	float gradientS = lumaS - lumaM;
	float lumaNN = lumaN + lumaM;
	float lumaSS = lumaS + lumaM;
	bool pairN = abs(gradientN) >= abs(gradientS);
	float gradient = max(abs(gradientN), abs(gradientS));
	if(pairN) lengthSign = -lengthSign;
	float subpixC = FxaaSat(abs(subpixB) * subpixRcpRange);

	float2 posB;
	posB.x = posM.x;
	posB.y = posM.y;
	float2 offNP;
	offNP.x = (!horzSpan) ? 0.0 : fxaaRcpFrame.x;
	offNP.y = ( horzSpan) ? 0.0 : fxaaRcpFrame.y;
	if(!horzSpan) posB.x += lengthSign * 0.5;
	if( horzSpan) posB.y += lengthSign * 0.5;

	float2 posN;
	posN.x = posB.x - offNP.x * FXAA_QUALITY_P0;
	posN.y = posB.y - offNP.y * FXAA_QUALITY_P0;
	float2 posP;
	posP.x = posB.x + offNP.x * FXAA_QUALITY_P0;
	posP.y = posB.y + offNP.y * FXAA_QUALITY_P0;
	float subpixD = ((-2.0)*subpixC) + 3.0;
	float lumaEndN = FxaaLuma(FxaaTexTop(tex, posN));
	float subpixE = subpixC * subpixC;
	float lumaEndP = FxaaLuma(FxaaTexTop(tex, posP));

	if(!pairN) lumaNN = lumaSS;
	float gradientScaled = gradient * 1.0/4.0;
	float lumaMM = lumaM - lumaNN * 0.5;
	float subpixF = subpixD * subpixE;
	bool lumaMLTZero = lumaMM < 0.0;
	lumaEndN -= lumaNN * 0.5;
	lumaEndP -= lumaNN * 0.5;
	bool doneN = abs(lumaEndN) >= gradientScaled;
	bool doneP = abs(lumaEndP) >= gradientScaled;
	if(!doneN) posN.x -= offNP.x * FXAA_QUALITY_P1;
	if(!doneN) posN.y -= offNP.y * FXAA_QUALITY_P1;
	bool doneNP = (!doneN) || (!doneP);
	if(!doneP) posP.x += offNP.x * FXAA_QUALITY_P1;
	if(!doneP) posP.y += offNP.y * FXAA_QUALITY_P1;

	if(doneNP) {
	if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
	if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
	if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
	if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
	doneN = abs(lumaEndN) >= gradientScaled;
	doneP = abs(lumaEndP) >= gradientScaled;
	if(!doneN) posN.x -= offNP.x * FXAA_QUALITY_P2;
	if(!doneN) posN.y -= offNP.y * FXAA_QUALITY_P2;
	doneNP = (!doneN) || (!doneP);
	if(!doneP) posP.x += offNP.x * FXAA_QUALITY_P2;
	if(!doneP) posP.y += offNP.y * FXAA_QUALITY_P2;

	if(doneNP) {
	if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
	if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
	if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
	if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
	doneN = abs(lumaEndN) >= gradientScaled;
	doneP = abs(lumaEndP) >= gradientScaled;
	if(!doneN) posN.x -= offNP.x * FXAA_QUALITY_P3;
	if(!doneN) posN.y -= offNP.y * FXAA_QUALITY_P3;
	doneNP = (!doneN) || (!doneP);
	if(!doneP) posP.x += offNP.x * FXAA_QUALITY_P3;
	if(!doneP) posP.y += offNP.y * FXAA_QUALITY_P3;

	if(doneNP) {
	if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
	if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
	if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
	if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
	doneN = abs(lumaEndN) >= gradientScaled;
	doneP = abs(lumaEndP) >= gradientScaled;
	if(!doneN) posN.x -= offNP.x * FXAA_QUALITY_P4;
	if(!doneN) posN.y -= offNP.y * FXAA_QUALITY_P4;
	doneNP = (!doneN) || (!doneP);
	if(!doneP) posP.x += offNP.x * FXAA_QUALITY_P4;
	if(!doneP) posP.y += offNP.y * FXAA_QUALITY_P4;

	if(doneNP) {
	if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
	if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
	if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
	if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
	doneN = abs(lumaEndN) >= gradientScaled;
	doneP = abs(lumaEndP) >= gradientScaled;
	if(!doneN) posN.x -= offNP.x * FXAA_QUALITY_P5;
	if(!doneN) posN.y -= offNP.y * FXAA_QUALITY_P5;
	doneNP = (!doneN) || (!doneP);
	if(!doneP) posP.x += offNP.x * FXAA_QUALITY_P5;
	if(!doneP) posP.y += offNP.y * FXAA_QUALITY_P5;

	if(doneNP) {
	if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
	if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
	if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
	if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
	doneN = abs(lumaEndN) >= gradientScaled;
	doneP = abs(lumaEndP) >= gradientScaled;
	if(!doneN) posN.x -= offNP.x * FXAA_QUALITY_P6;
	if(!doneN) posN.y -= offNP.y * FXAA_QUALITY_P6;
	doneNP = (!doneN) || (!doneP);
	if(!doneP) posP.x += offNP.x * FXAA_QUALITY_P6;
	if(!doneP) posP.y += offNP.y * FXAA_QUALITY_P6;

	if(doneNP) {
	if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
	if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
	if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
	if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
	doneN = abs(lumaEndN) >= gradientScaled;
	doneP = abs(lumaEndP) >= gradientScaled;
	if(!doneN) posN.x -= offNP.x * FXAA_QUALITY_P7;
	if(!doneN) posN.y -= offNP.y * FXAA_QUALITY_P7;
	doneNP = (!doneN) || (!doneP);
	if(!doneP) posP.x += offNP.x * FXAA_QUALITY_P7;
	if(!doneP) posP.y += offNP.y * FXAA_QUALITY_P7;

	if(doneNP) {
	if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
	if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
	if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
	if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
	doneN = abs(lumaEndN) >= gradientScaled;
	doneP = abs(lumaEndP) >= gradientScaled;
	if(!doneN) posN.x -= offNP.x * FXAA_QUALITY_P8;
	if(!doneN) posN.y -= offNP.y * FXAA_QUALITY_P8;
	doneNP = (!doneN) || (!doneP);
	if(!doneP) posP.x += offNP.x * FXAA_QUALITY_P8;
	if(!doneP) posP.y += offNP.y * FXAA_QUALITY_P8;

	if(doneNP) {
	if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
	if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
	if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
	if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
	doneN = abs(lumaEndN) >= gradientScaled;
	doneP = abs(lumaEndP) >= gradientScaled;
	if(!doneN) posN.x -= offNP.x * FXAA_QUALITY_P9;
	if(!doneN) posN.y -= offNP.y * FXAA_QUALITY_P9;
	doneNP = (!doneN) || (!doneP);
	if(!doneP) posP.x += offNP.x * FXAA_QUALITY_P9;
	if(!doneP) posP.y += offNP.y * FXAA_QUALITY_P9;

	if(doneNP) {
	if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
	if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
	if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
	if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
	doneN = abs(lumaEndN) >= gradientScaled;
	doneP = abs(lumaEndP) >= gradientScaled;
	if(!doneN) posN.x -= offNP.x * FXAA_QUALITY_P10;
	if(!doneN) posN.y -= offNP.y * FXAA_QUALITY_P10;
	doneNP = (!doneN) || (!doneP);
	if(!doneP) posP.x += offNP.x * FXAA_QUALITY_P10;
	if(!doneP) posP.y += offNP.y * FXAA_QUALITY_P10;

	if(doneNP) {
	if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
	if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
	if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
	if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
	doneN = abs(lumaEndN) >= gradientScaled;
	doneP = abs(lumaEndP) >= gradientScaled;
	if(!doneN) posN.x -= offNP.x * FXAA_QUALITY_P11;
	if(!doneN) posN.y -= offNP.y * FXAA_QUALITY_P11;
	doneNP = (!doneN) || (!doneP);
	if(!doneP) posP.x += offNP.x * FXAA_QUALITY_P11;
	if(!doneP) posP.y += offNP.y * FXAA_QUALITY_P11;

	if(doneNP) {
	if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
	if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
	if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
	if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
	doneN = abs(lumaEndN) >= gradientScaled;
	doneP = abs(lumaEndP) >= gradientScaled;
	if(!doneN) posN.x -= offNP.x * FXAA_QUALITY_P12;
	if(!doneN) posN.y -= offNP.y * FXAA_QUALITY_P12;
	doneNP = (!doneN) || (!doneP);
	if(!doneP) posP.x += offNP.x * FXAA_QUALITY_P12;
	if(!doneP) posP.y += offNP.y * FXAA_QUALITY_P12;
	}}}}}}}}}}}

	float dstN = posM.x - posN.x;
	float dstP = posP.x - posM.x;
	if(!horzSpan) dstN = posM.y - posN.y;
	if(!horzSpan) dstP = posP.y - posM.y;

	bool goodSpanN = (lumaEndN < 0.0) != lumaMLTZero;
	float spanLength = (dstP + dstN);
	bool goodSpanP = (lumaEndP < 0.0) != lumaMLTZero;
	float spanLengthRcp = 1.0/spanLength;

	bool directionN = dstN < dstP;
	float dst = min(dstN, dstP);
	bool goodSpan = directionN ? goodSpanN : goodSpanP;
	float subpixG = subpixF * subpixF;
	float pixelOffset = (dst * (-spanLengthRcp)) + 0.5;
	float subpixH = subpixG * fxaaSubpix;

	float pixelOffsetGood = goodSpan ? pixelOffset : 0.0;
	float pixelOffsetSubpix = max(pixelOffsetGood, subpixH);
	if(!horzSpan) posM.x += pixelOffsetSubpix * lengthSign;
	if( horzSpan) posM.y += pixelOffsetSubpix * lengthSign;

	return float4(FxaaTexTop(tex, posM).xyz, lumaM);
}

#if (FXAA_GLSL_130 == 1 || FXAA_GLSL_VK == 1)
float4 FxaaPass(float4 FxaaColor, float2 uv0)
#elif (FXAA_HLSL_5 == 1)
float4 FxaaPass(float4 FxaaColor : COLOR0, float2 uv0 : TEXCOORD0)
#elif defined(__METAL_VERSION__)
float4 FxaaPass(float4 FxaaColor, float2 uv0, texture2d<float> tex)
#endif
{

	#if (FXAA_HLSL_5 == 1)
	FxaaTex tex;
	tex.tex = Texture;
	tex.smpl = TextureSampler;

	float2 PixelSize;
	Texture.GetDimensions(PixelSize.x, PixelSize.y);
	FxaaColor = FxaaPixelShader(uv0, tex, 1.0/PixelSize.xy, FxaaSubpixMax, FxaaEdgeThreshold, FxaaEdgeThresholdMin);

	#elif (FXAA_GLSL_130 == 1 || FXAA_GLSL_VK == 1)
	vec2 PixelSize = vec2(textureSize(TextureSampler, 0));
	FxaaColor = FxaaPixelShader(uv0, TextureSampler, 1.0/PixelSize.xy, FxaaSubpixMax, FxaaEdgeThreshold, FxaaEdgeThresholdMin);
	#elif defined(__METAL_VERSION__)
	float2 PixelSize = float2(tex.get_width(), tex.get_height());
	FxaaColor = FxaaPixelShader(uv0, tex, 1.f/PixelSize, FxaaSubpixMax, FxaaEdgeThreshold, FxaaEdgeThresholdMin);
	#endif

	return FxaaColor;
}

/*------------------------------------------------------------------------------
                      [MAIN() & COMBINE PASS CODE SECTION]
------------------------------------------------------------------------------*/
#if (FXAA_GLSL_130 == 1 || FXAA_GLSL_VK == 1)

void main()
{
	vec4 color = texture(TextureSampler, PSin_t);
	color      = PreGammaPass(color);
	color      = FxaaPass(color, PSin_t);

	SV_Target0 = float4(color.rgb, 1.0);
}

#elif (FXAA_HLSL_5 == 1)
PS_OUTPUT main(VS_OUTPUT input)
{
	PS_OUTPUT output;

	float4 color = Texture.Sample(TextureSampler, input.t);

	color = PreGammaPass(color);
	color = FxaaPass(color, input.t);

	output.c = float4(color.rgb, 1.0);
	
	return output;
}

// Metal main function in in fxaa.metal
#endif
