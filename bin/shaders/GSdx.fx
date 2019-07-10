/*===============================================================================*\
|########################      [GSdx FX Suite v2.40]      ########################|
|##########################        By Asmodean          ##########################|
||                                                                               ||
||          This program is free software; you can redistribute it and/or        ||
||          modify it under the terms of the GNU General Public License          ||
||          as published by the Free Software Foundation; either version 2       ||
||          of the License, or (at your option) any later version.               ||
||                                                                               ||
||          This program is distributed in the hope that it will be useful,      ||
||          but WITHOUT ANY WARRANTY; without even the implied warranty of       ||
||          MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        ||
||          GNU General Public License for more details. (c)2016                 ||
||                                                                               ||
|#################################################################################|
\*===============================================================================*/

#ifndef SHADER_MODEL
#define GLSL 1
#else
#define GLSL 0
#endif

/*------------------------------------------------------------------------------
                             [GLOBALS|FUNCTIONS]
------------------------------------------------------------------------------*/
#if GLSL == 1
#define int2 ivec2
#define float2 vec2
#define float3 vec3
#define float4 vec4
#define float4x4 mat4
#define float3x3 mat3
#define float4x3 mat4x3
#define static
#define fmod mod
#define frac fract
#define mul(x, y) (y * x)
#define lerp(x,y,s) mix(x,y,s)
#define saturate(x) clamp(x, 0.0, 1.0)
#define SamplerState sampler2D

in SHADER
{
    vec4 p;
    vec2 t;
    vec4 c;
} PSin;

layout(location = 0) out vec4 SV_Target0;

layout(std140, binding = 14) uniform cb14
{
    vec2 _xyFrame;
    vec4 _rcpFrame;
};

#else

Texture2D Texture : register(t0);
SamplerState TextureSampler : register(s0);

cbuffer cb0
{
    float2 _xyFrame;
    float4 _rcpFrame;
};

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
#endif

static float Epsilon = 1e-10;
static float2 pixelSize = _rcpFrame.xy;
static float2 screenSize = _xyFrame;
static const float3 lumCoeff = float3(0.2126729, 0.7151522, 0.0721750);

//Conversion matrices
float3 RGBtoXYZ(float3 rgb)
{
    const float3x3 m = float3x3(
    0.4124564, 0.3575761, 0.1804375,
    0.2126729, 0.7151522, 0.0721750,
    0.0193339, 0.1191920, 0.9503041);

    return mul(m, rgb);
}

float3 XYZtoRGB(float3 xyz)
{
    const float3x3 m = float3x3(
    3.2404542,-1.5371385,-0.4985314,
   -0.9692660, 1.8760108, 0.0415560,
    0.0556434,-0.2040259, 1.0572252);

    return mul(m, xyz);
}

float3 RGBtoYUV(float3 RGB)
{
    const float3x3 m = float3x3(
    0.2126, 0.7152, 0.0722,
   -0.09991,-0.33609, 0.436,
    0.615, -0.55861, -0.05639);

    return mul(m, RGB);
}

float3 YUVtoRGB(float3 YUV)
{
    const float3x3 m = float3x3(
    1.000, 0.000, 1.28033,
    1.000,-0.21482,-0.38059,
    1.000, 2.12798, 0.000);

    return mul(m, YUV);
}

//Converting XYZ to Yxy
float3 XYZtoYxy(float3 xyz)
{
    float w = (xyz.r + xyz.g + xyz.b);
    float3 Yxy;

    Yxy.r = xyz.g;
    Yxy.g = xyz.r / w;
    Yxy.b = xyz.g / w;

    return Yxy;
}

//Converting Yxy to XYZ
float3 YxytoXYZ(float3 Yxy)
{
    float3 xyz;
    xyz.g = Yxy.r;
    xyz.r = Yxy.r * Yxy.g / Yxy.b;
    xyz.b = Yxy.r * (1.0 - Yxy.g - Yxy.b) / Yxy.b;

    return xyz;
}

//Average relative luminance
float AvgLuminance(float3 color)
{
    return sqrt(dot(color * color, lumCoeff));
}

float smootherstep(float a, float b, float x)
{
    x = saturate((x - a) / (b - a));
    return x*x*x*(x*(x * 6 - 15) + 10);
}

/*
float4 DebugClipping(float4 color)
{
    if (color.x >= 0.99999 && color.y >= 0.99999 &&
    color.z >= 0.99999) color.xyz = float3(1.0f, 0.0f, 0.0f);

    if (color.x <= 0.00001 && color.y <= 0.00001 &&
    color.z <= 0.00001) color.xyz = float3(0.0f, 0.0f, 1.0f);

    return color;
}
*/

float4 sample_tex(SamplerState texSample, float2 texcoord)
{
#if GLSL == 1
    return texture(texSample, texcoord);
#else
    return Texture.Sample(texSample, texcoord);
#endif
}

float4 sample_texLod(SamplerState texSample, float2 texcoord, float lod)
{
#if GLSL == 1
    return textureLod(texSample, texcoord, lod);
#else
    return Texture.SampleLevel(texSample, texcoord, lod);
#endif
}

/*------------------------------------------------------------------------------
                            [FXAA CODE SECTION]
------------------------------------------------------------------------------*/

#if UHQ_FXAA == 1
#if SHADER_MODEL >= 0x500
#define FXAA_HLSL_5 1
#define FXAA_GATHER4_ALPHA 1
#elif GLSL == 1
#define FXAA_GATHER4_ALPHA 1
#else
#define FXAA_HLSL_4 1
#define FXAA_GATHER4_ALPHA 0
#endif

#if FxaaQuality == 4
#define FxaaEdgeThreshold 0.063
#define FxaaEdgeThresholdMin 0.000
#elif FxaaQuality == 3
#define FxaaEdgeThreshold 0.125
#define FxaaEdgeThresholdMin 0.0312
#elif FxaaQuality == 2
#define FxaaEdgeThreshold 0.166
#define FxaaEdgeThresholdMin 0.0625
#elif FxaaQuality == 1
#define FxaaEdgeThreshold 0.250
#define FxaaEdgeThresholdMin 0.0833
#endif

#if FXAA_HLSL_5 == 1
struct FxaaTex { SamplerState smpl; Texture2D tex; };
#define FxaaTexTop(t, p) t.tex.SampleLevel(t.smpl, p, 0.0)
#define FxaaTexOff(t, p, o, r) t.tex.SampleLevel(t.smpl, p, 0.0, o)
#define FxaaTexAlpha4(t, p) t.tex.GatherAlpha(t.smpl, p)
#define FxaaTexOffAlpha4(t, p, o) t.tex.GatherAlpha(t.smpl, p, o)
#define FxaaDiscard clip(-1)
#define FxaaSat(x) saturate(x)

#elif FXAA_HLSL_4 == 1
struct FxaaTex { SamplerState smpl; Texture2D tex; };
#define FxaaTexTop(t, p) t.tex.SampleLevel(t.smpl, p, 0.0)
#define FxaaTexOff(t, p, o, r) t.tex.SampleLevel(t.smpl, p, 0.0, o)
#define FxaaDiscard clip(-1)
#define FxaaSat(x) saturate(x)
#endif

#if GLSL == 1
#define FxaaBool bool
#define FxaaDiscard discard
#define FxaaSat(x) clamp(x, 0.0, 1.0)
#define FxaaTex sampler2D
#define FxaaTexTop(t, p) textureLod(t, p, 0.0)
#define FxaaTexOff(t, p, o, r) textureLodOffset(t, p, 0.0, o)
#if FXAA_GATHER4_ALPHA == 1
#define FxaaTexAlpha4(t, p) textureGather(t, p, 3)
#define FxaaTexOffAlpha4(t, p, o) textureGatherOffset(t, p, o, 3)
#define FxaaTexGreen4(t, p) textureGather(t, p, 1)
#define FxaaTexOffGreen4(t, p, o) textureGatherOffset(t, p, o, 1)
#endif
#endif

#define FXAA_QUALITY__P0 1.0
#define FXAA_QUALITY__P1 1.0
#define FXAA_QUALITY__P2 1.0
#define FXAA_QUALITY__P3 1.0
#define FXAA_QUALITY__P4 1.0
#define FXAA_QUALITY__P5 1.5
#define FXAA_QUALITY__P6 2.0
#define FXAA_QUALITY__P7 2.0
#define FXAA_QUALITY__P8 2.0
#define FXAA_QUALITY__P9 2.0
#define FXAA_QUALITY__P10 4.0
#define FXAA_QUALITY__P11 8.0
#define FXAA_QUALITY__P12 8.0

float FxaaLuma(float4 rgba)
{
    rgba.w = AvgLuminance(rgba.xyz);
    return rgba.w;
}

float4 FxaaPixelShader(float2 pos, FxaaTex tex, float2 fxaaRcpFrame, float fxaaSubpix, float fxaaEdgeThreshold, float fxaaEdgeThresholdMin)
{
    float2 posM;
    posM.x = pos.x;
    posM.y = pos.y;

    #if FXAA_GATHER4_ALPHA == 1
    float4 rgbyM = FxaaTexTop(tex, posM);
    float4 luma4A = FxaaTexAlpha4(tex, posM);
    float4 luma4B = FxaaTexOffAlpha4(tex, posM, int2(-1, -1));
    rgbyM.w = AvgLuminance(rgbyM.xyz);

    #define lumaM rgbyM.w
    #define lumaE luma4A.z
    #define lumaS luma4A.x
    #define lumaSE luma4A.y
    #define lumaNW luma4B.w
    #define lumaN luma4B.z
    #define lumaW luma4B.x
    
    #else
    float4 rgbyM = FxaaTexTop(tex, posM);
    rgbyM.w = AvgLuminance(rgbyM.xyz);
    #define lumaM rgbyM.w

    float lumaS = FxaaLuma(FxaaTexOff(tex, posM, int2( 0, 1), fxaaRcpFrame.xy));
    float lumaE = FxaaLuma(FxaaTexOff(tex, posM, int2( 1, 0), fxaaRcpFrame.xy));
    float lumaN = FxaaLuma(FxaaTexOff(tex, posM, int2( 0,-1), fxaaRcpFrame.xy));
    float lumaW = FxaaLuma(FxaaTexOff(tex, posM, int2(-1, 0), fxaaRcpFrame.xy));
    #endif

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
    #if FxaaEarlyExit == 1
    if(earlyExit) { return rgbyM; }
    #endif

    #if FXAA_GATHER4_ALPHA == 0
    float lumaNW = FxaaLuma(FxaaTexOff(tex, posM, int2(-1,-1), fxaaRcpFrame.xy));
    float lumaSE = FxaaLuma(FxaaTexOff(tex, posM, int2( 1, 1), fxaaRcpFrame.xy));
    float lumaNE = FxaaLuma(FxaaTexOff(tex, posM, int2( 1,-1), fxaaRcpFrame.xy));
    float lumaSW = FxaaLuma(FxaaTexOff(tex, posM, int2(-1, 1), fxaaRcpFrame.xy));
    #else
    float lumaNE = FxaaLuma(FxaaTexOff(tex, posM, int2( 1,-1), fxaaRcpFrame.xy));
    float lumaSW = FxaaLuma(FxaaTexOff(tex, posM, int2(-1, 1), fxaaRcpFrame.xy));
    #endif

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
    posN.x = posB.x - offNP.x * FXAA_QUALITY__P0;
    posN.y = posB.y - offNP.y * FXAA_QUALITY__P0;
    float2 posP;
    posP.x = posB.x + offNP.x * FXAA_QUALITY__P0;
    posP.y = posB.y + offNP.y * FXAA_QUALITY__P0;
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
    if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P1;
    if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P1;
    bool doneNP = (!doneN) || (!doneP);
    if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P1;
    if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P1;

    if(doneNP) {
    if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
    if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
    if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
    if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
    doneN = abs(lumaEndN) >= gradientScaled;
    doneP = abs(lumaEndP) >= gradientScaled;
    if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P2;
    if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P2;
    doneNP = (!doneN) || (!doneP);
    if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P2;
    if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P2;

    if(doneNP) {
    if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
    if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
    if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
    if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
    doneN = abs(lumaEndN) >= gradientScaled;
    doneP = abs(lumaEndP) >= gradientScaled;
    if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P3;
    if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P3;
    doneNP = (!doneN) || (!doneP);
    if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P3;
    if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P3;

    if(doneNP) {
    if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
    if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
    if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
    if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
    doneN = abs(lumaEndN) >= gradientScaled;
    doneP = abs(lumaEndP) >= gradientScaled;
    if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P4;
    if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P4;
    doneNP = (!doneN) || (!doneP);
    if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P4;
    if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P4;

    if(doneNP) {
    if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
    if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
    if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
    if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
    doneN = abs(lumaEndN) >= gradientScaled;
    doneP = abs(lumaEndP) >= gradientScaled;
    if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P5;
    if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P5;
    doneNP = (!doneN) || (!doneP);
    if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P5;
    if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P5;

    if(doneNP) {
    if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
    if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
    if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
    if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
    doneN = abs(lumaEndN) >= gradientScaled;
    doneP = abs(lumaEndP) >= gradientScaled;
    if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P6;
    if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P6;
    doneNP = (!doneN) || (!doneP);
    if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P6;
    if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P6;

    if(doneNP) {
    if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
    if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
    if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
    if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
    doneN = abs(lumaEndN) >= gradientScaled;
    doneP = abs(lumaEndP) >= gradientScaled;
    if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P7;
    if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P7;
    doneNP = (!doneN) || (!doneP);
    if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P7;
    if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P7;

    if(doneNP) {
    if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
    if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
    if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
    if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
    doneN = abs(lumaEndN) >= gradientScaled;
    doneP = abs(lumaEndP) >= gradientScaled;
    if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P8;
    if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P8;
    doneNP = (!doneN) || (!doneP);
    if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P8;
    if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P8;

    if(doneNP) {
    if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
    if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
    if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
    if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
    doneN = abs(lumaEndN) >= gradientScaled;
    doneP = abs(lumaEndP) >= gradientScaled;
    if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P9;
    if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P9;
    doneNP = (!doneN) || (!doneP);
    if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P9;
    if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P9;

    if(doneNP) {
    if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
    if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
    if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
    if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
    doneN = abs(lumaEndN) >= gradientScaled;
    doneP = abs(lumaEndP) >= gradientScaled;
    if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P10;
    if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P10;
    doneNP = (!doneN) || (!doneP);
    if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P10;
    if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P10;

    if(doneNP) {
    if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
    if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
    if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
    if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
    doneN = abs(lumaEndN) >= gradientScaled;
    doneP = abs(lumaEndP) >= gradientScaled;
    if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P11;
    if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P11;
    doneNP = (!doneN) || (!doneP);
    if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P11;
    if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P11;

    if(doneNP) {
    if(!doneN) lumaEndN = FxaaLuma(FxaaTexTop(tex, posN.xy));
    if(!doneP) lumaEndP = FxaaLuma(FxaaTexTop(tex, posP.xy));
    if(!doneN) lumaEndN = lumaEndN - lumaNN * 0.5;
    if(!doneP) lumaEndP = lumaEndP - lumaNN * 0.5;
    doneN = abs(lumaEndN) >= gradientScaled;
    doneP = abs(lumaEndP) >= gradientScaled;
    if(!doneN) posN.x -= offNP.x * FXAA_QUALITY__P12;
    if(!doneN) posN.y -= offNP.y * FXAA_QUALITY__P12;
    doneNP = (!doneN) || (!doneP);
    if(!doneP) posP.x += offNP.x * FXAA_QUALITY__P12;
    if(!doneP) posP.y += offNP.y * FXAA_QUALITY__P12;
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

float4 FxaaPass(float4 FxaaColor, float2 texcoord)
{
    #if GLSL == 1
    FxaaColor = FxaaPixelShader(texcoord, TextureSampler, pixelSize.xy, FxaaSubpixMax, FxaaEdgeThreshold, FxaaEdgeThresholdMin);
    #else
    FxaaTex tex;

    tex.tex = Texture;
    tex.smpl = TextureSampler;
    FxaaColor = FxaaPixelShader(texcoord, tex, pixelSize.xy, FxaaSubpixMax, FxaaEdgeThreshold, FxaaEdgeThresholdMin);
    #endif

    return FxaaColor;
}
#endif

/*------------------------------------------------------------------------------
                        [TEXTURE FILTERING FUNCTIONS]
------------------------------------------------------------------------------*/

float BSpline(float x)
{
    float f = x;

    if (f < 0.0)
    {
        f = -f;
    }
    if (f >= 0.0 && f <= 1.0)
    {
        return (2.0 / 3.0) + (0.5) * (f* f * f) - (f*f);
    }
    else if (f > 1.0 && f <= 2.0)
    {
        return 1.0 / 6.0 * pow((2.0 - f), 3.0);
    }
    return 1.0;
}

float CatMullRom(float x)
{
    float b = 0.0;
    float c = 0.5;
    float f = x;

    if (f < 0.0)
    {
        f = -f;
    }
    if (f < 1.0)
    {
        return ((12.0 - 9.0 * b - 6.0 * c) *
                (f * f * f) + (-18.0 + 12.0 * b + 6.0 * c) *
                (f * f) + (6.0 - 2.0 * b)) / 6.0;
    }
    else if (f >= 1.0 && f < 2.0)
    {
        return ((-b - 6.0 * c) * (f * f * f) +
                (6.0 * b + 30.0 * c) *(f *f) +
                (-(12.0 * b) - 48.0 * c) * f +
                8.0 * b + 24.0 * c) / 6.0;
    }
    else
    {
        return 0.0;
    }
}

float Bell(float x)
{
    float f = (x / 2.0) * 1.5;

    if (f > -1.5 && f < -0.5)
    {
        return(0.5 * pow(f + 1.5, 2.0));
    }
    else if (f > -0.5 && f < 0.5)
    {
        return 3.0 / 4.0 - (f * f);
    }
    else if ((f > 0.5 && f < 1.5))
    {
        return(0.5 * pow(f - 1.5, 2.0));
    }
    return 0.0;
}

float Triangular(float x)
{
    x = x / 2.0;

    if (x < 0.0)
    {
        return (x + 1.0);
    }
    else
    {
        return (1.0 - x);
    }
    return 0.0;
}

float Cubic(float coeff)
{
    float4 n = float4(1.0, 2.0, 3.0, 4.0) - coeff;
    float4 s = n * n * n;

    float x = s.x;
    float y = s.y - 4.0 * s.x;
    float z = s.z - 4.0 * s.y + 6.0 * s.x;
    float w = 6.0 - x - y - z;

    return (x + y + z + w) / 4.0;
}

/*------------------------------------------------------------------------------
                       [BILINEAR FILTERING CODE SECTION]
------------------------------------------------------------------------------*/

#if BILINEAR_FILTERING == 1
float4 SampleBiLinear(SamplerState texSample, float2 texcoord)
{
    float texelSizeX = pixelSize.x;
    float texelSizeY = pixelSize.y;

    int nX = int(texcoord.x * screenSize.x);
    int nY = int(texcoord.y * screenSize.y);

    float2 uvCoord = float2((float(nX) + OffsetAmount) / screenSize.x, (float(nY) + OffsetAmount) / screenSize.y);

    // Take nearest two data in current row.
    float4 SampleA = sample_tex(texSample, uvCoord);
    float4 SampleB = sample_tex(texSample, uvCoord + float2(texelSizeX, 0.0));

    // Take nearest two data in bottom row.
    float4 SampleC = sample_tex(texSample, uvCoord + float2(0.0, texelSizeY));
    float4 SampleD = sample_tex(texSample, uvCoord + float2(texelSizeX, texelSizeY));

    float LX = frac(texcoord.x * screenSize.x); //Get Interpolation factor for X direction.

    // Interpolate in X direction.
    float4 InterpolateA = lerp(SampleA, SampleB, LX); //Top row in X direction.
    float4 InterpolateB = lerp(SampleC, SampleD, LX); //Bottom row in X direction.

    float LY = frac(texcoord.y * screenSize.y); //Get Interpolation factor for Y direction.

    return lerp(InterpolateA, InterpolateB, LY); //Interpolate in Y direction.
}

float4 BiLinearPass(float4 color, float2 texcoord)
{
    float4 bilinear = SampleBiLinear(TextureSampler, texcoord);
    color = lerp(color, bilinear, FilterStrength);

    return color;
}
#endif

/*------------------------------------------------------------------------------
                      [BICUBIC FILTERING CODE SECTION]
------------------------------------------------------------------------------*/

#if BICUBIC_FILTERING == 1
float4 BicubicFilter(SamplerState texSample, float2 texcoord)
{  
    float texelSizeX = pixelSize.x;
    float texelSizeY = pixelSize.y;

    float4 nSum = float4(0.0, 0.0, 0.0, 0.0);
    float4 nDenom = float4(0.0, 0.0, 0.0, 0.0);

    float a = frac(texcoord.x * screenSize.x);
    float b = frac(texcoord.y * screenSize.y);

    int nX = int(texcoord.x * screenSize.x);
    int nY = int(texcoord.y * screenSize.y);

    float2 uvCoord = float2(float(nX) / screenSize.x + PixelOffset / screenSize.x,
    float(nY) / screenSize.y + PixelOffset / screenSize.y);

    for (int m = -1; m <= 2; m++) {
    for (int n = -1; n <= 2; n++) {

    float4 Samples = sample_tex(texSample, uvCoord +
    float2(texelSizeX * float(m), texelSizeY * float(n)));

    float vc1 = Interpolation(float(m) - a);
    float4 vecCoeff1 = float4(vc1, vc1, vc1, vc1);

    float vc2 = Interpolation(-(float(n) - b));
    float4 vecCoeff2 = float4(vc2, vc2, vc2, vc2);

    nSum = nSum + (Samples * vecCoeff2 * vecCoeff1);
    nDenom = nDenom + (vecCoeff2 * vecCoeff1); }}
    return nSum / nDenom;
}

float4 BiCubicPass(float4 color, float2 texcoord)
{
    float4 bicubic = BicubicFilter(TextureSampler, texcoord);
    color = lerp(color, bicubic, BicubicStrength);
    return color;
}
#endif

/*------------------------------------------------------------------------------
                      [GAUSSIAN FILTERING CODE SECTION]
------------------------------------------------------------------------------*/

#if GAUSSIAN_FILTERING == 1
float4 GaussianPass(float4 color, float2 texcoord)
{
    if (screenSize.x < 1024 || screenSize.y < 1024)
    {
        pixelSize.x /= 2.0;
        pixelSize.y /= 2.0;
    }
    
    float2 dx = float2(pixelSize.x * GaussianSpread, 0.0);
    float2 dy = float2(0.0, pixelSize.y * GaussianSpread);

    float2 dx2 = 2.0 * dx;
    float2 dy2 = 2.0 * dy;

    float4 gaussian = sample_tex(TextureSampler, texcoord);

    gaussian += sample_tex(TextureSampler, texcoord - dx2 + dy2);
    gaussian += sample_tex(TextureSampler, texcoord - dx + dy2);
    gaussian += sample_tex(TextureSampler, texcoord + dy2);
    gaussian += sample_tex(TextureSampler, texcoord + dx + dy2);
    gaussian += sample_tex(TextureSampler, texcoord + dx2 + dy2);

    gaussian += sample_tex(TextureSampler, texcoord - dx2 + dy);
    gaussian += sample_tex(TextureSampler, texcoord - dx + dy);
    gaussian += sample_tex(TextureSampler, texcoord + dy);
    gaussian += sample_tex(TextureSampler, texcoord + dx + dy);
    gaussian += sample_tex(TextureSampler, texcoord + dx2 + dy);

    gaussian += sample_tex(TextureSampler, texcoord - dx2);
    gaussian += sample_tex(TextureSampler, texcoord - dx);
    gaussian += sample_tex(TextureSampler, texcoord + dx);
    gaussian += sample_tex(TextureSampler, texcoord + dx2);

    gaussian += sample_tex(TextureSampler, texcoord - dx2 - dy);
    gaussian += sample_tex(TextureSampler, texcoord - dx - dy);
    gaussian += sample_tex(TextureSampler, texcoord - dy);
    gaussian += sample_tex(TextureSampler, texcoord + dx - dy);
    gaussian += sample_tex(TextureSampler, texcoord + dx2 - dy);

    gaussian += sample_tex(TextureSampler, texcoord - dx2 - dy2);
    gaussian += sample_tex(TextureSampler, texcoord - dx - dy2);
    gaussian += sample_tex(TextureSampler, texcoord - dy2);
    gaussian += sample_tex(TextureSampler, texcoord + dx - dy2);
    gaussian += sample_tex(TextureSampler, texcoord + dx2 - dy2);

    gaussian /= 25.0;

    color = lerp(color, gaussian, FilterAmount);

    return color;
}
#endif

/*------------------------------------------------------------------------------
                         [BICUBIC SCALER CODE SECTION]
------------------------------------------------------------------------------*/

#if BICUBLIC_SCALER == 1
float4 BicubicScaler(SamplerState tex, float2 uv, float2 texSize)
{
    float2 inputSize = float2(1.0/texSize.x, 1.0/texSize.y);

    float2 coord_hg = uv * texSize - 0.5;
    float2 index = floor(coord_hg);
    float2 f = coord_hg - index;

    #if GLSL == 1
    mat4 M = mat4( -1.0, 3.0,-3.0, 1.0, 3.0,-6.0, 3.0, 0.0,
                   -3.0, 0.0, 3.0, 0.0, 1.0, 4.0, 1.0, 0.0 );
    #else
    float4x4 M = { -1.0, 3.0,-3.0, 1.0, 3.0,-6.0, 3.0, 0.0,
                   -3.0, 0.0, 3.0, 0.0, 1.0, 4.0, 1.0, 0.0 };
    #endif
    M /= 6.0;

    float4 wx = mul(float4(f.x*f.x*f.x, f.x*f.x, f.x, 1.0), M);
    float4 wy = mul(float4(f.y*f.y*f.y, f.y*f.y, f.y, 1.0), M);
    float2 w0 = float2(wx.x, wy.x);
    float2 w1 = float2(wx.y, wy.y);
    float2 w2 = float2(wx.z, wy.z);
    float2 w3 = float2(wx.w, wy.w);

    float2 g0 = w0 + w1;
    float2 g1 = w2 + w3;
    float2 h0 = w1 / g0 - 1.0;
    float2 h1 = w3 / g1 + 1.0;

    float2 coord00 = index + h0;
    float2 coord10 = index + float2(h1.x, h0.y);
    float2 coord01 = index + float2(h0.x, h1.y);
    float2 coord11 = index + h1;

    coord00 = (coord00 + 0.5) * inputSize;
    coord10 = (coord10 + 0.5) * inputSize;
    coord01 = (coord01 + 0.5) * inputSize;
    coord11 = (coord11 + 0.5) * inputSize;

    float4 tex00 = sample_texLod(tex, coord00, 0);
    float4 tex10 = sample_texLod(tex, coord10, 0);
    float4 tex01 = sample_texLod(tex, coord01, 0);
    float4 tex11 = sample_texLod(tex, coord11, 0);

    tex00 = lerp(tex01, tex00, float4(g0.y, g0.y, g0.y, g0.y));
    tex10 = lerp(tex11, tex10, float4(g0.y, g0.y, g0.y, g0.y));

    float4 res = lerp(tex10, tex00, float4(g0.x, g0.x, g0.x, g0.x));

    return res;
}

float4 BiCubicScalerPass(float4 color, float2 texcoord)
{
    color = BicubicScaler(TextureSampler, texcoord, screenSize);
    return color;
}
#endif

/*------------------------------------------------------------------------------
                         [LANCZOS SCALER CODE SECTION]
------------------------------------------------------------------------------*/

#if LANCZOS_SCALER == 1
float3 PixelPos(float xpos, float ypos)
{
    return sample_tex(TextureSampler, float2(xpos, ypos)).rgb;
}

float4 WeightQuad(float x)
{
    #define FIX(c) max(abs(c), 1e-5);
    const float PI = 3.1415926535897932384626433832795;

    float4 weight = FIX(PI * float4(1.0 + x, x, 1.0 - x, 2.0 - x));
    float4 ret = sin(weight) * sin(weight / 2.0) / (weight * weight);

    return ret / dot(ret, float4(1.0, 1.0, 1.0, 1.0));
}

float3 LineRun(float ypos, float4 xpos, float4 linetaps)
{
    return mul(linetaps, float4x3(
    PixelPos(xpos.x, ypos),
    PixelPos(xpos.y, ypos),
    PixelPos(xpos.z, ypos),
    PixelPos(xpos.w, ypos)));
}

float4 LanczosScaler(float2 texcoord, float2 inputSize)
{
    float2 stepxy = float2(1.0/inputSize.x, 1.0/inputSize.y);
    float2 pos = texcoord + stepxy;
    float2 f = frac(pos / stepxy);

    float2 xystart = (-2.0 - f) * stepxy + pos;
    float4 xpos = float4(xystart.x,
    xystart.x + stepxy.x,
    xystart.x + stepxy.x * 2.0,
    xystart.x + stepxy.x * 3.0);

    float4 linetaps = WeightQuad(f.x);
    float4 columntaps = WeightQuad(f.y);

    // final sum and weight normalization
    return float4(mul(columntaps, float4x3(
    LineRun(xystart.y, xpos, linetaps),
    LineRun(xystart.y + stepxy.y, xpos, linetaps),
    LineRun(xystart.y + stepxy.y * 2.0, xpos, linetaps),
    LineRun(xystart.y + stepxy.y * 3.0, xpos, linetaps))), 1.0);
}

float4 LanczosScalerPass(float4 color, float2 texcoord)
{
    color = LanczosScaler(texcoord, screenSize);
    return color;
}
#endif

/*------------------------------------------------------------------------------
                       [GAMMA CORRECTION CODE SECTION]
------------------------------------------------------------------------------*/

float3 EncodeGamma(float3 color, float gamma)
{
    color = saturate(color);
    color.r = (color.r <= 0.0404482362771082) ?
    color.r / 12.92 : pow((color.r + 0.055) / 1.055, gamma);
    color.g = (color.g <= 0.0404482362771082) ?
    color.g / 12.92 : pow((color.g + 0.055) / 1.055, gamma);
    color.b = (color.b <= 0.0404482362771082) ?
    color.b / 12.92 : pow((color.b + 0.055) / 1.055, gamma);

    return color;
}

float3 DecodeGamma(float3 color, float gamma)
{
    color = saturate(color);
    color.r = (color.r <= 0.00313066844250063) ?
    color.r * 12.92 : 1.055 * pow(color.r, 1.0 / gamma) - 0.055;
    color.g = (color.g <= 0.00313066844250063) ?
    color.g * 12.92 : 1.055 * pow(color.g, 1.0 / gamma) - 0.055;
    color.b = (color.b <= 0.00313066844250063) ?
    color.b * 12.92 : 1.055 * pow(color.b, 1.0 / gamma) - 0.055;

    return color;
}

#if GAMMA_CORRECTION == 1
float4 GammaPass(float4 color, float2 texcoord)
{
    static const float GammaConst = 2.233333;
    color.rgb = EncodeGamma(color.rgb, GammaConst);
    color.rgb = DecodeGamma(color.rgb, float(Gamma));
    color.a = AvgLuminance(color.rgb);

    return color;
}
#endif
/*------------------------------------------------------------------------------
                       [TEXTURE SHARPEN CODE SECTION]
------------------------------------------------------------------------------*/

#if TEXTURE_SHARPEN == 1
float4 SampleBicubic(SamplerState texSample, float2 texcoord)
{
    float texelSizeX = pixelSize.x * float(SharpenBias);
    float texelSizeY = pixelSize.y * float(SharpenBias);

    float4 nSum = float4(0.0, 0.0, 0.0, 0.0);
    float4 nDenom = float4(0.0, 0.0, 0.0, 0.0);

    float a = frac(texcoord.x * screenSize.x);
    float b = frac(texcoord.y * screenSize.y);

    int nX = int(texcoord.x * screenSize.x);
    int nY = int(texcoord.y * screenSize.y);

    float2 uvCoord = float2(float(nX) / screenSize.x, float(nY) / screenSize.y);

    for (int m = -1; m <= 2; m++) {
    for (int n = -1; n <= 2; n++) {

    float4 Samples = sample_tex(texSample, uvCoord +
    float2(texelSizeX * float(m), texelSizeY * float(n)));

    float vc1 = Cubic(float(m) - a);
    float4 vecCoeff1 = float4(vc1, vc1, vc1, vc1);

    float vc2 = Cubic(-(float(n) - b));
    float4 vecCoeff2 = float4(vc2, vc2, vc2, vc2);

    nSum = nSum + (Samples * vecCoeff2 * vecCoeff1);
    nDenom = nDenom + (vecCoeff2 * vecCoeff1); }}
    return nSum / nDenom;
}

float4 TexSharpenPass(float4 color, float2 texcoord)
{
    float3 calcSharpen = lumCoeff * float(SharpenStrength);

    float4 blurredColor = SampleBicubic(TextureSampler, texcoord);
    float3 sharpenedColor = (color.rgb - blurredColor.rgb);

    float sharpenLuma = dot(sharpenedColor, calcSharpen);
    sharpenLuma = clamp(sharpenLuma, -float(SharpenClamp), float(SharpenClamp));

    color.rgb = color.rgb + sharpenLuma;
    color.a = AvgLuminance(color.rgb);

    #if DebugSharpen == 1
        color = saturate(0.5f + (sharpenLuma * 4)).xxxx;
    #endif

    return saturate(color);
}
#endif

/*------------------------------------------------------------------------------
                       [PIXEL VIBRANCE CODE SECTION]
------------------------------------------------------------------------------*/

#if PIXEL_VIBRANCE == 1
float4 VibrancePass(float4 color, float2 texcoord)
{
    float vib = Vibrance;
    
    #if GLSL == 1
    float3 luma = float3(AvgLuminance(color.rgb));
    #else
    float luma = AvgLuminance(color.rgb);
    #endif

    float colorMax = max(color.r, max(color.g, color.b));
    float colorMin = min(color.r, min(color.g, color.b));

    float colorSaturation = colorMax - colorMin;
    float3 colorCoeff = float3(RedVibrance * vib, GreenVibrance * vib, BlueVibrance * vib);

    color.rgb = lerp(luma, color.rgb, (1.0 + (colorCoeff * (1.0 - (sign(colorCoeff) * colorSaturation)))));
    color.a = AvgLuminance(color.rgb);

    return saturate(color); //Debug: return colorSaturation.xxxx;
}
#endif

/*------------------------------------------------------------------------------
                        [BLENDED BLOOM CODE SECTION]
------------------------------------------------------------------------------*/

#if BLENDED_BLOOM == 1
float3 BlendAddLight(float3 bloom, float3 blend)
{
    return saturate(bloom + blend);
}

float3 BlendScreen(float3 bloom, float3 blend)
{
    return (bloom + blend) - (bloom * blend);
}

float3 BlendGlow(float3 bloom, float3 blend)
{
    float glow = AvgLuminance(bloom);
    return lerp((bloom + blend) - (bloom * blend),
    (blend + blend) - (blend * blend), glow);
}

float3 BlendAddGlow(float3 bloom, float3 blend)
{
    float addglow = smootherstep(0.0, 1.0, AvgLuminance(bloom));
    return lerp(saturate(bloom + blend),
    (blend + blend) - (blend * blend), addglow);
}

float3 BlendLuma(float3 bloom, float3 blend)
{
    float lumavg = smootherstep(0.0, 1.0, AvgLuminance(bloom + blend));
    return lerp((bloom * blend), (1.0 -
    ((1.0 - bloom) * (1.0 - blend))), lumavg);
}

float3 BlendOverlay(float3 bloom, float3 blend)
{
    float3 overlay = step(0.5, bloom);
    return lerp((bloom * blend * 2.0), (1.0 - (2.0 *
    (1.0 - bloom) * (1.0 - blend))), overlay);
}

float3 BloomCorrection(float3 color)
{
    float3 bloom = color;

    bloom.r = 2.0 / 3.0 * (1.0 - (bloom.r * bloom.r));
    bloom.g = 2.0 / 3.0 * (1.0 - (bloom.g * bloom.g));
    bloom.b = 2.0 / 3.0 * (1.0 - (bloom.b * bloom.b));

    bloom.r = saturate(color.r + float(BloomReds) * bloom.r);
    bloom.g = saturate(color.g + float(BloomGreens) * bloom.g);
    bloom.b = saturate(color.b + float(BloomBlues) * bloom.b);

    color = bloom;

    return color;
}

float4 DefocusFilter(SamplerState tex, float2 texcoord, float2 defocus)
{
    float2 texel = pixelSize * defocus;

    float4 sampleA = sample_tex(tex, texcoord + float2(0.5, 0.5) * texel);
    float4 sampleB = sample_tex(tex, texcoord + float2(-0.5, 0.5) * texel);
    float4 sampleC = sample_tex(tex, texcoord + float2(0.5, -0.5) * texel);
    float4 sampleD = sample_tex(tex, texcoord + float2(-0.5, -0.5) * texel);

    float fx = frac(texcoord.x * screenSize.x);
    float fy = frac(texcoord.y * screenSize.y);

    float4 interpolateA = lerp(sampleA, sampleB, fx);
    float4 interpolateB = lerp(sampleC, sampleD, fx);

    return lerp(interpolateA, interpolateB, fy);
}

float4 BloomPass(float4 color, float2 texcoord)
{
    float anflare = 4.0;

    float2 defocus = float2(BloomDefocus, BloomDefocus);
    float4 bloom = DefocusFilter(TextureSampler, texcoord, defocus);

    float2 dx = float2(pixelSize.x * float(BloomWidth), 0.0);
    float2 dy = float2(0.0, pixelSize.y * float(BloomWidth));

    float2 mdx = float2(dx.x * defocus.x, 0.0);
    float2 mdy = float2(0.0, dy.y * defocus.y);

    float4 blend = bloom * 0.22520613262190495;

    blend += 0.002589001911021066 * sample_tex(TextureSampler, texcoord - mdx + mdy);
    blend += 0.010778807494659370 * sample_tex(TextureSampler, texcoord - dx + mdy);
    blend += 0.024146616900339800 * sample_tex(TextureSampler, texcoord + mdy);
    blend += 0.010778807494659370 * sample_tex(TextureSampler, texcoord + dx + mdy);
    blend += 0.002589001911021066 * sample_tex(TextureSampler, texcoord + mdx + mdy);

    blend += 0.010778807494659370 * sample_tex(TextureSampler, texcoord - mdx + dy);
    blend += 0.044875475183061630 * sample_tex(TextureSampler, texcoord - dx + dy);
    blend += 0.100529757860782610 * sample_tex(TextureSampler, texcoord + dy);
    blend += 0.044875475183061630 * sample_tex(TextureSampler, texcoord + dx + dy);
    blend += 0.010778807494659370 * sample_tex(TextureSampler, texcoord + mdx + dy);

    blend += 0.024146616900339800 * sample_tex(TextureSampler, texcoord - mdx);
    blend += 0.100529757860782610 * sample_tex(TextureSampler, texcoord - dx);
    blend += 0.100529757860782610 * sample_tex(TextureSampler, texcoord + dx);
    blend += 0.024146616900339800 * sample_tex(TextureSampler, texcoord + mdx);

    blend += 0.010778807494659370 * sample_tex(TextureSampler, texcoord - mdx - dy);
    blend += 0.044875475183061630 * sample_tex(TextureSampler, texcoord - dx - dy);
    blend += 0.100529757860782610 * sample_tex(TextureSampler, texcoord - dy);
    blend += 0.044875475183061630 * sample_tex(TextureSampler, texcoord + dx - dy);
    blend += 0.010778807494659370 * sample_tex(TextureSampler, texcoord + mdx - dy);

    blend += 0.002589001911021066 * sample_tex(TextureSampler, texcoord - mdx - mdy);
    blend += 0.010778807494659370 * sample_tex(TextureSampler, texcoord - dx - mdy);
    blend += 0.024146616900339800 * sample_tex(TextureSampler, texcoord - mdy);
    blend += 0.010778807494659370 * sample_tex(TextureSampler, texcoord + dx - mdy);
    blend += 0.002589001911021066 * sample_tex(TextureSampler, texcoord + mdx - mdy);
    blend = lerp(color, blend, float(BlendStrength));

    bloom.xyz = BloomType(bloom.xyz, blend.xyz);
    bloom.xyz = BloomCorrection(bloom.xyz);

    color.w = AvgLuminance(color.xyz);
    bloom.w = AvgLuminance(bloom.xyz);
    bloom.w *= anflare;

    color = lerp(color, bloom, float(BloomStrength));

    return color;
}
#endif

/*------------------------------------------------------------------------------
                      [SCENE TONE MAPPING CODE SECTION]
------------------------------------------------------------------------------*/

#if SCENE_TONEMAPPING == 1
float3 ScaleLuminance(float3 x)
{
    float W = 1.02;
    float L = 0.06;
    float C = 1.02;

    float N = clamp(0.76 + ToneAmount, 1.0, 2.0);
    float K = (N - L * C) / C;

    float3 tone = L * C + (1.0 - L * C) * (1.0 + K * (x - L) /
    ((W - L) * (W - L))) * (x - L) / (x - L + K);

    float3 color;
    color.r = (x.r > L) ? tone.r : C * x.r;
    color.g = (x.g > L) ? tone.g : C * x.g;
    color.b = (x.b > L) ? tone.b : C * x.b;

    return color;
}

float3 TmMask(float3 color)
{
    float3 tone = color;

    float highTone = 6.2; float greyTone = 0.4;
    float midTone = 1.620; float lowTone = 0.06;

    tone.r = (tone.r * (highTone * tone.r + greyTone))/(
    tone.r * (highTone * tone.r + midTone) + lowTone);
    tone.g = (tone.g * (highTone * tone.g + greyTone))/(
    tone.g * (highTone * tone.g + midTone) + lowTone);
    tone.b = (tone.b * (highTone * tone.b + greyTone))/(
    tone.b * (highTone * tone.b + midTone) + lowTone);

    static const float gamma = 2.42;
    tone = EncodeGamma(tone, gamma);

    color = lerp(color, tone, float(MaskStrength));

    return color;
}

float3 TmCurve(float3 color)
{
    float3 T = color;

    float tnamn = ToneAmount;
    float blevel = length(T);
    float bmask = pow(blevel, 0.02);

    float A = 0.100; float B = 0.300;
    float C = 0.100; float D = tnamn;
    float E = 0.020; float F = 0.300;

    float W = 1.000;

    T.r = ((T.r*(A*T.r + C*B) + D*E) / (T.r*(A*T.r + B) + D*F)) - E / F;
    T.g = ((T.g*(A*T.g + C*B) + D*E) / (T.g*(A*T.g + B) + D*F)) - E / F;
    T.b = ((T.b*(A*T.b + C*B) + D*E) / (T.b*(A*T.b + B) + D*F)) - E / F;

    float denom = ((W*(A*W + C*B) + D*E) / (W*(A*W + B) + D*F)) - E / F;

    float3 black = float3(bmask, bmask, bmask);
    float3 white = float3(denom, denom, denom);

    T = T / white;
    T = T * black;

    color = saturate(T);

    return color;
}

float4 TonemapPass(float4 color, float2 texcoord)
{
    float3 tonemap = color.rgb;
    
    float blackLevel = length(tonemap);
    tonemap = ScaleLuminance(tonemap);

    #if GLSL == 1
    float luminanceAverage = AvgLuminance(float3(Luminance));
    #else
    float luminanceAverage = AvgLuminance(Luminance);
    #endif

    if (TonemapMask == 1) { tonemap = TmMask(tonemap); }
    if (TonemapType == 1) { tonemap = TmCurve(tonemap); }

    // RGB -> XYZ conversion
    const float3x3 RGB2XYZ = float3x3(0.4124564, 0.3575761, 0.1804375,
                                      0.2126729, 0.7151522, 0.0721750,
                                      0.0193339, 0.1191920, 0.9503041);

    float3 XYZ = mul(RGB2XYZ, tonemap);

    // XYZ -> Yxy conversion
    float3 Yxy;

    Yxy.r = XYZ.g;                                  // copy luminance Y
    Yxy.g = XYZ.r / (XYZ.r + XYZ.g + XYZ.b);        // x = X / (X + Y + Z)
    Yxy.b = XYZ.g / (XYZ.r + XYZ.g + XYZ.b);        // y = Y / (X + Y + Z)

    // (Wt) Tone mapped scaling of the initial wp before input modifiers
    float Wt = saturate(Yxy.r / AvgLuminance(XYZ));

    if (TonemapType == 2) { Yxy.r = TmCurve(Yxy).r; }

    // (Lp) Map average luminance to the middlegrey zone by scaling pixel luminance
    float Lp = Yxy.r * float(Exposure) / (luminanceAverage + Epsilon);

    // (Wp) White point calculated, based on the toned white, and input modifier
    float Wp = dot(abs(Wt), float(WhitePoint));

    // (Ld) Scale all luminance within a displayable range of 0 to 1
    Yxy.r = (Lp * (1.0 + Lp / (Wp * Wp))) / (1.0 + Lp);

    // Yxy -> XYZ conversion
    XYZ.r = Yxy.r * Yxy.g / Yxy.b;                  // X = Y * x / y
    XYZ.g = Yxy.r;                                  // copy luminance Y
    XYZ.b = Yxy.r * (1.0 - Yxy.g - Yxy.b) / Yxy.b;  // Z = Y * (1-x-y) / y

    if (TonemapType == 3) { XYZ = TmCurve(XYZ); }

    // XYZ -> RGB conversion
    const float3x3 XYZ2RGB = float3x3(3.2404542,-1.5371385,-0.4985314,
                                     -0.9692660, 1.8760108, 0.0415560,
                                      0.0556434,-0.2040259, 1.0572252);

    tonemap = mul(XYZ2RGB, XYZ);

    float shadowmask = pow(saturate(blackLevel), float(BlackLevels));
    tonemap = tonemap * float3(shadowmask, shadowmask, shadowmask);

    color.rgb = tonemap;
    color.a = AvgLuminance(color.rgb);

    return color;
}
#endif

/*------------------------------------------------------------------------------
                      [CROSS PROCESSING CODE SECTION]
------------------------------------------------------------------------------*/

#if CROSS_PROCESSING == 1
float3 CrossShift(float3 color)
{
    float3 cross;

    float2 CrossMatrix[3] = {
    float2 (0.960, 0.040 * color.x),
    float2 (0.980, 0.020 * color.y),
    float2 (0.970, 0.030 * color.z), };

    cross.x = float(RedShift) * CrossMatrix[0].x + CrossMatrix[0].y;
    cross.y = float(GreenShift) * CrossMatrix[1].x + CrossMatrix[1].y;
    cross.z = float(BlueShift) * CrossMatrix[2].x + CrossMatrix[2].y;

    float lum = AvgLuminance(color);
    float3 black = float3(0.0, 0.0, 0.0);
    float3 white = float3(1.0, 1.0, 1.0);

    cross = lerp(black, cross, saturate(lum * 2.0));
    cross = lerp(cross, white, saturate(lum - 0.5) * 2.0);
    color = lerp(color, cross, saturate(lum * float(ShiftRatio)));

    return color;
}

float4 CrossPass(float4 color, float2 texcoord)
{
    #if FilmicProcess == 1
    color.rgb = CrossShift(color.rgb);

    #elif FilmicProcess == 2
    float3 XYZ = RGBtoXYZ(color.rgb);
    float3 Yxy = XYZtoYxy(XYZ);

    Yxy = CrossShift(Yxy);
    XYZ = YxytoXYZ(Yxy);

    color.rgb = XYZtoRGB(XYZ);

    #elif FilmicProcess == 3
    float3 XYZ = RGBtoXYZ(color.rgb);
    float3 Yxy = XYZtoYxy(XYZ);

    XYZ = YxytoXYZ(Yxy);
    XYZ = CrossShift(XYZ);

    color.rgb = XYZtoRGB(XYZ);
    #endif

    color.a = AvgLuminance(color.rgb);

    return saturate(color);
}
#endif

/*------------------------------------------------------------------------------
                      [COLOR CORRECTION CODE SECTION]
------------------------------------------------------------------------------*/

// Converting pure hue to RGB
float3 HUEtoRGB(float H)
{
    float R = abs(H * 6.0 - 3.0) - 1.0;
    float G = 2.0 - abs(H * 6.0 - 2.0);
    float B = 2.0 - abs(H * 6.0 - 4.0);

    return saturate(float3(R, G, B));
}

// Converting RGB to hue/chroma/value
float3 RGBtoHCV(float3 RGB)
{
    float4 BG = float4(RGB.bg,-1.0, 2.0 / 3.0);
    float4 GB = float4(RGB.gb, 0.0,-1.0 / 3.0);

    float4 P = (RGB.g < RGB.b) ? BG : GB;

    float4 XY = float4(P.xyw, RGB.r);
    float4 YZ = float4(RGB.r, P.yzx);

    float4 Q = (RGB.r < P.x) ? XY : YZ;

    float C = Q.x - min(Q.w, Q.y);
    float H = abs((Q.w - Q.y) / (6.0 * C + Epsilon) + Q.z);

    return float3(H, C, Q.x);
}

// Converting RGB to HSV
float3 RGBtoHSV(float3 RGB)
{
    float3 HCV = RGBtoHCV(RGB);
    float S = HCV.y / (HCV.z + Epsilon);

    return float3(HCV.x, S, HCV.z);
}

// Converting HSV to RGB
float3 HSVtoRGB(float3 HSV)
{
    float3 RGB = HUEtoRGB(HSV.x);
    return ((RGB - 1.0) * HSV.y + 1.0) * HSV.z;
}

#if COLOR_CORRECTION == 1
// Pre correction color mask
float3 PreCorrection(float3 color)
{
    float3 RGB = color;

    RGB.r = 2.0 / 3.0 * (1.0 - (RGB.r * RGB.r));
    RGB.g = 2.0 / 3.0 * (1.0 - (RGB.g * RGB.g));
    RGB.b = 2.0 / 3.0 * (1.0 - (RGB.b * RGB.b));

    RGB.r = saturate(color.r + (float(ChannelR) / 200.0) * RGB.r);
    RGB.g = saturate(color.g + (float(ChannelG) / 200.0) * RGB.g);
    RGB.b = saturate(color.b + (float(ChannelB) / 200.0) * RGB.b);

    color = saturate(RGB);

    return color;
}

float3 ColorCorrection(float3 color)
{
    float X = 1.0 / (1.0 + exp(float(ChannelR) / 2.0));
    float Y = 1.0 / (1.0 + exp(float(ChannelG) / 2.0));
    float Z = 1.0 / (1.0 + exp(float(ChannelB) / 2.0));

    color.r = (1.0 / (1.0 + exp(float(-ChannelR) * (color.r - 0.5))) - X) / (1.0 - 2.0 * X);
    color.g = (1.0 / (1.0 + exp(float(-ChannelG) * (color.g - 0.5))) - Y) / (1.0 - 2.0 * Y);
    color.b = (1.0 / (1.0 + exp(float(-ChannelB) * (color.b - 0.5))) - Z) / (1.0 - 2.0 * Z);

    return saturate(color);
}

float4 CorrectionPass(float4 color, float2 texcoord)
{
    float3 colorspace = PreCorrection(color.rgb);

    #if CorrectionPalette == 1
    colorspace = ColorCorrection(colorspace);

    #elif CorrectionPalette == 2
    float3 XYZ = RGBtoXYZ(colorspace);
    float3 Yxy = XYZtoYxy(XYZ);

    Yxy = ColorCorrection(Yxy);
    XYZ = YxytoXYZ(Yxy);
    colorspace = XYZtoRGB(XYZ);

    #elif CorrectionPalette == 3
    float3 XYZ = RGBtoXYZ(colorspace);
    float3 Yxy = XYZtoYxy(XYZ);

    XYZ = YxytoXYZ(Yxy);
    XYZ = ColorCorrection(XYZ);
    colorspace = XYZtoRGB(XYZ);

    #elif CorrectionPalette == 4
    float3 hsv = RGBtoHSV(colorspace);
    hsv = ColorCorrection(hsv);
    colorspace = HSVtoRGB(hsv);

    #elif CorrectionPalette == 5
    float3 yuv = RGBtoYUV(colorspace);
    yuv = ColorCorrection(yuv);
    colorspace = YUVtoRGB(yuv);
    #endif

    color.rgb = lerp(color.rgb, colorspace, float(PaletteStrength));
    color.a = AvgLuminance(color.rgb);

    return color;
}
#endif


/*------------------------------------------------------------------------------
                       [S-CURVE CONTRAST CODE SECTION]
------------------------------------------------------------------------------*/

#if CURVE_CONTRAST == 1
float4 ContrastPass(float4 color, float2 texcoord)
{
    float CurveBlend = CurvesContrast;

    #if CurveType != 2
    #if GLSL == 1
    float3 luma = float3(AvgLuminance(color.rgb));
    #else
    float3 luma = (float3)AvgLuminance(color.rgb);
    #endif
    float3 chroma = color.rgb - luma;
    #endif

    #if CurveType == 2
    float3 x = color.rgb;
    #elif (CurveType == 1)
    float3 x = chroma;
    x = x * 0.5 + 0.5;
    #else
    float3 x = luma;
    #endif

    //S-Curve - Cubic Bezier spline
    float3 a = float3(0.00, 0.00, 0.00);    //start point
    float3 b = float3(0.25, 0.25, 0.25);    //control point 1
    float3 c = float3(0.85, 0.85, 0.85);    //control point 2
    float3 d = float3(1.00, 1.00, 1.00);    //endpoint

    float3 ab = lerp(a, b, x);              //point between a and b (green)
    float3 bc = lerp(b, c, x);              //point between b and c (green)
    float3 cd = lerp(c, d, x);              //point between c and d (green)
    float3 abbc = lerp(ab, bc, x);          //point between ab and bc (blue)
    float3 bccd = lerp(bc, cd, x);          //point between bc and cd (blue)
    float3 dest = lerp(abbc, bccd, x);      //point on the bezier-curve (black)

    x = dest;

    #if CurveType == 0 //Only Luma
    x = lerp(luma, x, CurveBlend);
    color.rgb = x + chroma;
    #elif (CurveType == 1) //Only Chroma
    x = x * 2 - 1;
    float3 LColor = luma + x;
    color.rgb = lerp(color.rgb, LColor, CurveBlend);
    #elif (CurveType == 2) //Both Luma and Chroma
    float3 LColor = x;
    color.rgb = lerp(color.rgb, LColor, CurveBlend);
    #endif

    color.a = AvgLuminance(color.rgb);

    return saturate(color);
}
#endif

/*------------------------------------------------------------------------------
                         [CEL SHADING CODE SECTION]
------------------------------------------------------------------------------*/

#if CEL_SHADING == 1
float3 CelColor(in float3 RGB)
{
    float3 HSV = RGBtoHSV(RGB);

    float AW = 1.0;
    float AB = 0.0;
    float SL = 7.0;
    float CR = 1.0 / SL;
    float MS = 1.2;
    float ML = fmod(HSV[2], CR);

    if (HSV[2] > AW)
    {
        HSV[1] = 1.0; HSV[2] = 1.0;
    }
    else if (HSV[2] > AB)
    {
        HSV[1] *= MS;  HSV[2] += ((CR * (HSV[2] + 0.6)) - ML);
    }
    else
    {
        HSV[1] = 0.0; HSV[2] = 0.0;
    }

    HSV[2] = clamp(HSV[2], float(int(HSV[2] / CR) - 1) * CR, float(int(HSV[2] / CR) + 1) * CR);
    RGB = HSVtoRGB(HSV);

    return RGB;
}

float4 CelPass(float4 color, float2 uv0)
{   
    float3 yuv;
    float3 sum = color.rgb;

    const int NUM = 9;
    const float2 RoundingOffset = float2(0.15, 0.2);
    const float3 thresholds = float3(9.0, 9.0, 9.0);

    float lum[NUM];
    float3 col[NUM];
    float2 set[NUM] = {
    float2(-0.0078125, -0.0078125),
    float2(0.00, -0.0078125),
    float2(0.0078125, -0.0078125),
    float2(-0.0078125, 0.00),
    float2(0.00, 0.00),
    float2(0.0078125, 0.00),
    float2(-0.0078125, 0.0078125),
    float2(0.00, 0.0078125),
    float2(0.0078125, 0.0078125) };

    for (int i = 0; i < NUM; i++)
    {
        col[i] = sample_tex(TextureSampler, uv0 + set[i] * RoundingOffset).rgb;
        col[i] = CelColor(col[i]);

        #if ColorRounding == 1
        col[i].r = round(col[i].r * thresholds.r) / thresholds.r;
        col[i].g = round(col[i].g * thresholds.g) / thresholds.g;
        col[i].b = round(col[i].b * thresholds.b) / thresholds.b;
        #endif

        lum[i] = AvgLuminance(col[i].xyz);
        yuv = RGBtoYUV(col[i]);
        
        #if UseYuvLuma == 1
        yuv.r = round(yuv.r * thresholds.r) / thresholds.r;
        #endif
        
        yuv = YUVtoRGB(yuv);
        sum += yuv;
    }

    float3 shaded = sum / NUM;
    float3 shadedColor = lerp(color.rgb, shaded, 0.6);

    float cs; float4 offset;
    float4 pos0 = uv0.xyxy;

    offset.xy = -(offset.zw = float2(pixelSize.x * float(EdgeThickness), 0.0));
    float4 pos1 = pos0 + offset;

    offset.xy = -(offset.zw = float2(0.0, pixelSize.y * float(EdgeThickness)));
    float4 pos2 = pos0 + offset;

    float4 pos3 = pos1 + 2.0 * offset;

    float3 c0 = sample_tex(TextureSampler, pos3.xy).rgb;
    float3 c1 = sample_tex(TextureSampler, pos2.xy).rgb;
    float3 c2 = sample_tex(TextureSampler, pos3.zy).rgb;
    float3 c3 = sample_tex(TextureSampler, pos1.xy).rgb;
    float3 c5 = sample_tex(TextureSampler, pos1.zw).rgb;
    float3 c6 = sample_tex(TextureSampler, pos3.xw).rgb;
    float3 c7 = sample_tex(TextureSampler, pos2.zw).rgb;
    float3 c8 = sample_tex(TextureSampler, pos3.zw).rgb;

    float3 o = float3(1.0, 1.0, 1.0);
    float3 h = float3(0.02, 0.02, 0.02);

    float3 hz = h; float k = 0.02; float kz = 0.0035;
    float3 cz = (color.rgb + h) / (dot(o, color.rgb) + k);

    hz = (cz - ((c0 + h) / (dot(o, c0) + k))); cs = kz / (dot(hz, hz) + kz);
    hz = (cz - ((c1 + h) / (dot(o, c1) + k))); cs += kz / (dot(hz, hz) + kz);
    hz = (cz - ((c2 + h) / (dot(o, c2) + k))); cs += kz / (dot(hz, hz) + kz);
    hz = (cz - ((c3 + h) / (dot(o, c3) + k))); cs += kz / (dot(hz, hz) + kz);
    hz = (cz - ((c5 + h) / (dot(o, c5) + k))); cs += kz / (dot(hz, hz) + kz);
    hz = (cz - ((c6 + h) / (dot(o, c6) + k))); cs += kz / (dot(hz, hz) + kz);
    hz = (cz - ((c7 + h) / (dot(o, c7) + k))); cs += kz / (dot(hz, hz) + kz);
    hz = (cz - ((c8 + h) / (dot(o, c8) + k))); cs += kz / (dot(hz, hz) + kz);

    cs /= 8.0;

    color.rgb = lerp(color.rgb, shadedColor, AvgLuminance(color.rgb)) * pow(cs, EdgeStrength);
    color.a = AvgLuminance(color.rgb);

    return saturate(color);
}
#endif

/*------------------------------------------------------------------------------
                        [PAINT SHADING CODE SECTION]
------------------------------------------------------------------------------*/

#if PAINT_SHADING == 1
float3 PaintShading(float3 color, float2 texcoord)
{
    #if PaintMethod == 1
    float2	tex;
    int	k, j, lum, cmax = 0;

    float	C0, C1, C2, C3, C4, C5, C6, C7, C8, C9;
    float3	A, B, C, D, E, F, G, H, I, J, shade;

    for (k = int(-PaintRadius); k < (int(PaintRadius) + 1); k++){
    for (j = int(-PaintRadius); j < (int(PaintRadius) + 1); j++){

    tex.x = texcoord.x + pixelSize.x * k;
    tex.y = texcoord.y + pixelSize.y * j;

    shade = sample_tex(TextureSampler, tex).xyz;

    lum = AvgLuminance(shade) * 9.0;

    C0 = (lum == 0) ? C0 + 1 : C0;
    C1 = (lum == 1) ? C1 + 1 : C1;
    C2 = (lum == 2) ? C2 + 1 : C2;
    C3 = (lum == 3) ? C3 + 1 : C3;
    C4 = (lum == 4) ? C4 + 1 : C4;
    C5 = (lum == 5) ? C5 + 1 : C5;
    C6 = (lum == 6) ? C6 + 1 : C6;
    C7 = (lum == 7) ? C7 + 1 : C7;
    C8 = (lum == 8) ? C8 + 1 : C8;
    C9 = (lum == 9) ? C9 + 1 : C9;

    A = (lum == 0) ? A + shade : A;
    B = (lum == 1) ? B + shade : B;
    C = (lum == 2) ? C + shade : C;
    D = (lum == 3) ? D + shade : D;
    E = (lum == 4) ? E + shade : E;
    F = (lum == 5) ? F + shade : F;
    G = (lum == 6) ? G + shade : G;
    H = (lum == 7) ? H + shade : H;
    I = (lum == 8) ? I + shade : I;
    J = (lum == 9) ? J + shade : J;
    }}

    if (C0 > cmax){ cmax = C0; color.xyz = A / cmax; }
    if (C1 > cmax){ cmax = C1; color.xyz = B / cmax; }
    if (C2 > cmax){ cmax = C2; color.xyz = C / cmax; }
    if (C3 > cmax){ cmax = C3; color.xyz = D / cmax; }
    if (C4 > cmax){ cmax = C4; color.xyz = E / cmax; }
    if (C5 > cmax){ cmax = C5; color.xyz = F / cmax; }
    if (C6 > cmax){ cmax = C6; color.xyz = G / cmax; }
    if (C7 > cmax){ cmax = C7; color.xyz = H / cmax; }
    if (C8 > cmax){ cmax = C8; color.xyz = I / cmax; }
    if (C9 > cmax){ cmax = C9; color.xyz = J / cmax; }

    #else
    int j, i;

    float3 m0, m1, m2, m3, k0, k1, k2, k3, shade;
    float n = float((PaintRadius + 1.0) * (PaintRadius + 1.0));

    for (j = int(-PaintRadius); j <= 0; ++j)  {
    for (i = int(-PaintRadius); i <= 0; ++i)  {

    shade = sample_tex(TextureSampler, texcoord + float2(i, j) / screenSize).rgb;
    m0 += shade; k0 += shade * shade; }}

    for (j = int(-PaintRadius); j <= 0; ++j) {
    for (i = 0; i <= int(PaintRadius); ++i)  {
    shade = sample_tex(TextureSampler, texcoord + float2(i, j) / screenSize).rgb;
    m1 += shade; k1 += shade * shade; }}

    for (j = 0; j <= int(PaintRadius); ++j)  {
    for (i = 0; i <= int(PaintRadius); ++i)  {
    shade = sample_tex(TextureSampler, texcoord + float2(i, j) / screenSize).rgb;
    m2 += shade; k2 += shade * shade; }}

    float min_sigma2 = 1e+2;
    m0 /= n; k0 = abs(k0 / n - m0 * m0);

    float sigma2 = k0.r + k0.g + k0.b;
    if (sigma2 < min_sigma2) {
    min_sigma2 = sigma2; color = m0; }

    m1 /= n; k1 = abs(k1 / n - m1 * m1);
    sigma2 = k1.r + k1.g + k1.b;

    if (sigma2 < min_sigma2) {
    min_sigma2 = sigma2;
    color = m1; }

    m2 /= n; k2 = abs(k2 / n - m2 * m2);
    sigma2 = k2.r + k2.g + k2.b;

    if (sigma2 < min_sigma2) {
    min_sigma2 = sigma2;
    color = m2; }
    #endif

    return color;
}

float4 PaintPass(float4 color, float2 texcoord)
{
    float3 paint = PaintShading(color.rgb, texcoord);
    color.rgb = lerp(color.rgb, paint, float(PaintStrength));
    color.a = AvgLuminance(color.rgb);

    return color;
}
#endif


/*------------------------------------------------------------------------------
                      [COLOR GRADING CODE SECTION]
------------------------------------------------------------------------------*/

#if COLOR_GRADING == 1
float RGBCVtoHUE(float3 RGB, float C, float V)
{
    float3 Delta = (V - RGB) / C;

    Delta.rgb -= Delta.brg;
    Delta.rgb += float3(2.0, 4.0, 6.0);
    Delta.brg = step(V, RGB) * Delta.brg;

    float H;
    H = max(Delta.r, max(Delta.g, Delta.b));
    return frac(H / 6);
}

float3 HSVComplement(float3 HSV)
{
    float3 complement = HSV;
    complement.x -= 0.5;

    if (complement.x < 0.0) { complement.x += 1.0; }
    return(complement);
}

float HueLerp(float h1, float h2, float v)
{
    float d = abs(h1 - h2);

    if (d <= 0.5)
    { return lerp(h1, h2, v); }
    else if (h1 < h2)
    { return frac(lerp((h1 + 1.0), h2, v)); }
    else
    { return frac(lerp(h1, (h2 + 1.0), v)); }
}

float4 ColorGrading(float4 color, float2 texcoord)
{
    float3 guide = float3(RedGrading, GreenGrading, BlueGrading);
    float amount = GradingStrength;
    float correlation = Correlation;
    float concentration = 2.00;

    float3 colorHSV = RGBtoHSV(color.rgb);
    float3 huePoleA = RGBtoHSV(guide);
    float3 huePoleB = HSVComplement(huePoleA);

    float dist1 = abs(colorHSV.x - huePoleA.x); if (dist1 > 0.5) dist1 = 1.0 - dist1;
    float dist2 = abs(colorHSV.x - huePoleB.x); if (dist2 > 0.5) dist2 = 1.0 - dist2;

    float descent = smoothstep(0.0, correlation, colorHSV.y);

    float3 HSVColor = colorHSV;

    if (dist1 < dist2)
    {
        float c = descent * amount * (1.0 - pow((dist1 * 2.0), 1.0 / concentration));
        HSVColor.x = HueLerp(colorHSV.x, huePoleA.x, c);
        HSVColor.y = lerp(colorHSV.y, huePoleA.y, c);
    }
    else
    {
        float c = descent * amount * (1.0 - pow((dist2 * 2.0), 1.0 / concentration));
        HSVColor.x = HueLerp(colorHSV.x, huePoleB.x, c);
        HSVColor.y = lerp(colorHSV.y, huePoleB.y, c);
    }

    color.rgb = HSVtoRGB(HSVColor);
    color.a = AvgLuminance(color.rgb);

    return saturate(color);
}
#endif


/*------------------------------------------------------------------------------
                           [COLOR_TEMPERATURE]
------------------------------------------------------------------------------*/

#if COLOR_TEMPERATURE == 1
float4 TemperaturePass(float4 color, float2 texcoord)
{
   float temp = clamp(White_Point, 2000.0, 12000.0) / 100.0f;

   // all calculations assume a scale of 255. We'll normalize this at the end
   float3 wp = float3(255.0,255.0,255.0);

   if (White_Point <= 6600.0) {
       wp.r = 255.0;
       wp.g = - 155.25485562709179 - 0.44596950469579133 * (temp - 2.0)  + 104.49216199393888 * log(temp - 2.0);
       wp.b = White_Point <= 1900 ? 0.0 : - 254.76935184120902 + 0.8274096064007395 * (temp - 10.0) + 115.67994401066147 * log(temp - 10.0) ;
   } else {
       wp.r = 351.97690566805693 + 0.114206453784165 * (temp - 55.0) - 40.25366309332127 * log(temp - 55.0);
       wp.g = 325.4494125711974  + 0.07943456536662342 * (temp - 50.0) - 28.0852963507957   * log(temp - 50.0);
       wp.b = 255.0;
   }

   // clamp and normalize
   wp.rgb = clamp(wp.rgb, 0.0, 255.0) / 255.0;

   float3 adjusted = color.rgb * wp;
   float3 base_luma = XYZtoYxy(RGBtoXYZ(color.rgb));
   float3 adjusted_luma = XYZtoYxy(RGBtoXYZ(adjusted));
   adjusted = adjusted_luma + (float3(base_luma.r,0.0,0.0) - float3(adjusted_luma.r,0.0,0.0));
   color = float4(XYZtoRGB(YxytoXYZ(adjusted)), 1.0);

   return color;
}
#endif

/*------------------------------------------------------------------------------
                           [SCANLINES CODE SECTION]
------------------------------------------------------------------------------*/

#if SCANLINES == 1
float4 ScanlinesPass(float4 color, float2 texcoord, float4 fragcoord)
{
    float4 intensity;
    
    #if GLSL == 1
    fragcoord = gl_FragCoord;
    #endif

    #if ScanlineType == 0
    if (frac(fragcoord.y * 0.25) > ScanlineScale)
    #elif (ScanlineType == 1)
    if (frac(fragcoord.x * 0.25) > ScanlineScale)
    #elif (ScanlineType == 2)
    if (frac(fragcoord.x * 0.25) > ScanlineScale && frac(fragcoord.y * 0.5) > ScanlineScale)
    #endif
    {
        intensity = float4(0.0, 0.0, 0.0, 0.0);
    }
    else
    {
        intensity = smoothstep(0.2, ScanlineBrightness, color) + normalize(float4(color.xyz, AvgLuminance(color.xyz)));
    }

    float level = (4.0 - texcoord.x) * ScanlineIntensity;

    color = intensity * (0.5 - level) + color * 1.1;

    return color;
}
#endif

/*------------------------------------------------------------------------------
                          [VIGNETTE CODE SECTION]
------------------------------------------------------------------------------*/

#if VIGNETTE == 1
float4 VignettePass(float4 color, float2 texcoord)
{
    const float2 VignetteCenter = float2(0.500, 0.500);
    float2 tc = texcoord - VignetteCenter;

    tc *= float2((2560.0 / 1440.0), VignetteRatio);
    tc /= VignetteRadius;

    float v = dot(tc, tc);

    color.rgb *= (1.0 + pow(v, VignetteSlope * 0.25) * -VignetteAmount);

    return color;
}
#endif

/*------------------------------------------------------------------------------
                      [SUBPIXEL DITHERING CODE SECTION]
------------------------------------------------------------------------------*/

#if SP_DITHERING == 1
float Randomize(float2 texcoord)
{
    float seed = dot(texcoord, float2(12.9898, 78.233));
    float sine = sin(seed);
    float noise = frac(sine * 43758.5453);

    return noise;
}

float4 DitherPass(float4 color, float2 texcoord)
{
    float ditherBits = 8.0;

    #if DitherMethod == 2       //random dithering

    float noise = Randomize(texcoord);
    float ditherShift = (1.0 / (pow(2.0, ditherBits) - 1.0));
    float ditherHalfShift = (ditherShift * 0.5);
    ditherShift = ditherShift * noise - ditherHalfShift;

    color.rgb += float3(-ditherShift, ditherShift, -ditherShift);

    #if ShowMethod == 1
        color.rgb = noise;
    #endif

    #elif DitherMethod == 1     //ordered dithering

    float2 ditherSize = float2(1.0 / 16.0, 10.0 / 36.0);
    float gridPosition = frac(dot(texcoord, (screenSize * ditherSize)) + 0.25);
    float ditherShift = (0.25) * (1.0 / (pow(2.0, ditherBits) - 1.0));

    float3 RGBShift = float3(ditherShift, -ditherShift, ditherShift);
    RGBShift = lerp(2.0 * RGBShift, -2.0 * RGBShift, gridPosition);

    color.rgb += RGBShift;

    #if ShowMethod == 1
        color.rgb = gridPosition;
    #endif

    #endif

    color.a = AvgLuminance(color.rgb);

    return color;
}
#endif

/*------------------------------------------------------------------------------
                           [PX BORDER CODE SECTION]
------------------------------------------------------------------------------*/

float4 BorderPass(float4 colorInput, float2 tex)
{
    float3 border_color_float = BorderColor / 255.0;

    float2 border = (_rcpFrame.xy * BorderWidth);
    float2 within_border = saturate((-tex * tex + tex) - (-border * border + border));

    #if GLSL == 1
    // FIXME GLSL any only support bvec so try to mix it with notEqual
    bvec2 cond = notEqual( within_border, vec2(0.0f) );
    colorInput.rgb = all(cond) ? colorInput.rgb : border_color_float;
    #else
    colorInput.rgb = all(within_border) ? colorInput.rgb : border_color_float;
    #endif

    return colorInput;

}

/*------------------------------------------------------------------------------
                           [LOTTES CRT CODE SECTION]
------------------------------------------------------------------------------*/

#if LOTTES_CRT == 1
float ToLinear1(float c)
{
    c = saturate(c);
    return(c <= 0.04045) ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

float3 ToLinear(float3 c)
{
    return float3(ToLinear1(c.r), ToLinear1(c.g), ToLinear1(c.b));
}

float ToSrgb1(float c)
{
    c = saturate(c);
    return(c < 0.0031308 ? c * 12.92 : 1.055 * pow(c, 0.41666) -0.055);
}

float3 ToSrgb(float3 c)
{
    return float3(ToSrgb1(c.r), ToSrgb1(c.g), ToSrgb1(c.b));
}

float3 Fetch(float2 pos, float2 off)
{
    float2 res = (screenSize * ResolutionScale);
    pos = round(pos * res + off) / res;
    if(max(abs(pos.x - 0.5), abs(pos.y - 0.5)) > 0.5) { return float3(0.0, 0.0, 0.0); }

    return ToLinear(sample_tex(TextureSampler, pos.xy).rgb);
}

float2 Dist(float2 pos)
{
    float2 crtRes = float2(CRTSizeX, CRTSizeY);
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
    float3 b = Fetch(pos,float2(-1.0, off));
    float3 c = Fetch(pos,float2( 0.0, off));
    float3 d = Fetch(pos,float2( 1.0, off));
    float dst = Dist(pos).x;

    // Convert distance to weight.
    float scale = FilterCRTAmount;
    float wb = Gaus(dst-1.0, scale);
    float wc = Gaus(dst+0.0, scale);
    float wd = Gaus(dst+1.0, scale);

    return (b*wb+c*wc+d*wd)/(wb+wc+wd);
}

float3 Horz5(float2 pos, float off)
{
    float3 a = Fetch(pos, float2(-2.0, off));
    float3 b = Fetch(pos, float2(-1.0, off));
    float3 c = Fetch(pos, float2( 0.0, off));
    float3 d = Fetch(pos, float2( 1.0, off));
    float3 e = Fetch(pos, float2( 2.0, off));
    float dst = Dist(pos).x;

    // Convert distance to weight.
    float scale = FilterCRTAmount;

    float wa = Gaus(dst-2.0, scale);
    float wb = Gaus(dst-1.0, scale);
    float wc = Gaus(dst+0.0, scale);
    float wd = Gaus(dst+1.0, scale);
    float we = Gaus(dst+2.0, scale);

    return (a*wa+b*wb+c*wc+d*wd+e*we)/(wa+wb+wc+wd+we);
}

// Return scanline weight.
float Scan(float2 pos, float off)
{
    float dst = Dist(pos).y;
    return Gaus(dst+off, ScanBrightness);
}

float3 Tri(float2 pos)
{
    float3 a = Horz3(pos,-1.0);
    float3 b = Horz5(pos, 0.0);
    float3 c = Horz3(pos, 1.0);

    float wa = Scan(pos,-1.0);
    float wb = Scan(pos, 0.0);
    float wc = Scan(pos, 1.0);

    return a*wa+b*wb+c*wc;
}

float2 Warp(float2 pos)
{
    pos = pos * 2.0-1.0;    
    pos *= float2(1.0 + (pos.y*pos.y) * (HorizontalWarp), 1.0 + (pos.x*pos.x) * (VerticalWarp));
    return pos * 0.5 + 0.5;
}

float3 Mask(float2 pos)
{
    #if MaskingType == 1
    // Very compressed TV style shadow mask.
    float lines = MaskAmountLight;
    float odd = 0.0;

    if(frac(pos.x/6.0) < 0.5) odd = 1.0;
    if (frac((pos.y + odd) / 2.0) < 0.5) lines = MaskAmountDark;
    pos.x = frac(pos.x/3.0);
    float3 mask = float3(MaskAmountDark, MaskAmountDark, MaskAmountDark);

    if(pos.x < 0.333) mask.r = MaskAmountLight;
    else if(pos.x < 0.666)mask.g = MaskAmountLight;
    else mask.b = MaskAmountLight;
    mask *= lines;

    return mask;
    
    #elif MaskingType == 2
    // Aperture-grille.
    pos.x = frac(pos.x/3.0);
    float3 mask = float3(MaskAmountDark, MaskAmountDark, MaskAmountDark);

    if(pos.x < 0.333)mask.r = MaskAmountLight;
    else if(pos.x < 0.666)mask.g = MaskAmountLight;
    else mask.b = MaskAmountLight;

    return mask;
    
    #elif MaskingType == 3
    // Stretched VGA style shadow mask (same as prior shaders).
    pos.x += pos.y*3.0;
    float3 mask = float3(MaskAmountDark, MaskAmountDark, MaskAmountDark);
    pos.x = frac(pos.x/6.0);

    if(pos.x < 0.333)mask.r = MaskAmountLight;
    else if(pos.x < 0.666)mask.g = MaskAmountLight;
    else mask.b = MaskAmountLight;

    return mask;
    
    #else
    // VGA style shadow mask.
    pos.xy = floor(pos.xy*float2(1.0, 0.5));
    pos.x += pos.y*3.0;

    float3 mask = float3(MaskAmountDark, MaskAmountDark, MaskAmountDark);
    pos.x = frac(pos.x/6.0);

    if(pos.x < 0.333)mask.r = MaskAmountLight;
    else if(pos.x < 0.666)mask.g = MaskAmountLight;
    else mask.b= MaskAmountLight;
    return mask;
    #endif
}

float4 LottesCRTPass(float4 color, float2 texcoord, float4 fragcoord)
{
    #if GLSL == 1
    fragcoord = gl_FragCoord;
    float2 inSize = textureSize(TextureSampler, 0);
    #else
    float2 inSize;
    Texture.GetDimensions(inSize.x, inSize.y);
    #endif

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
#endif

/*------------------------------------------------------------------------------
                           [DEBAND CODE SECTION]
------------------------------------------------------------------------------*/

#if DEBANDING == 1
//Deband debug settings
#define DEBAND_SKIP_THRESHOLD_TEST 0   //[0:1] 1 = Skip threshold to see the unfiltered sampling pattern
#define DEBAND_OUTPUT_BOOST 1.0        //[-2.0:2.0] Default = 1.0. Any value other than the default activates debug mode.
#define DEBAND_OUTPUT_OFFSET 0.0       //[-1.0:3.0] Default = 0.0. Any value other than the default activates debug mode.

float rand(float2 pos)
{
	return frac(sin(dot(pos, float2(12.9898, 78.233))) * 43758.5453);
}

bool is_within_threshold(float3 original, float3 other)
{
    #if GLSL == 1
    bvec3 cond = notEqual(max(abs(original - other) - DebandThreshold, float3(0.0, 0.0, 0.0)), float3(0.0, 0.0, 0.0));
    return !any(cond).x;
    #else
    return !any(max(abs(original - other) - DebandThreshold, float3(0.0, 0.0, 0.0))).x;
    #endif
}

float4 DebandPass(float4 color, float2 texcoord)
{
    float2 step = pixelSize * DebandRadius;
    float2 halfstep = step * 0.5;

    // Compute additional sample positions
    float2 seed = texcoord;
    #if  (DebandOffsetMode == 1)
    float2 offset = float2(rand(seed), 0.0);
    #elif(DebandOffsetMode == 2)
    float2 offset = float2(rand(seed).xx);
    #elif(DebandOffsetMode == 3)
    float2 offset = float2(rand(seed), rand(seed + float2(0.1, 0.2)));
    #endif

    float2 on[8] = {
    float2( offset.x,  offset.y) * step,
    float2( offset.y, -offset.x) * step,
    float2(-offset.x, -offset.y) * step,
    float2(-offset.y,  offset.x) * step,
    float2( offset.x,  offset.y) * halfstep,
    float2( offset.y, -offset.x) * halfstep,
    float2(-offset.x, -offset.y) * halfstep,
    float2(-offset.y,  offset.x) * halfstep };

    float3 col0 = color.rgb;
    float4 accu = float4(col0, 1.0);

    for(int i=0; i < int(DebandSampleCount); i++)
    {
        float4 cn = float4(sample_tex(TextureSampler, texcoord + on[i]).rgb, 1.0);
        
        #if (DEBAND_SKIP_THRESHOLD_TEST == 0)
        if(is_within_threshold(col0, cn.rgb))
        #endif
        
        accu += cn;
    }

    accu.rgb /= accu.a;

    // Boost to make it easier to inspect the effect's output
    if(DEBAND_OUTPUT_OFFSET != 0.0 || DEBAND_OUTPUT_BOOST != 1.0)
    {
        accu.rgb -= DEBAND_OUTPUT_OFFSET;
        accu.rgb *= DEBAND_OUTPUT_BOOST;
    }

    // Additional dithering
    #if   (DebandDithering == 1)
    //Ordered dithering
    float dither_bit  = 8.0;
    float grid_position = frac( dot(texcoord,(screenSize * float2(1.0/16.0,10.0/36.0))) + 0.25 );
    float dither_shift = (0.25) * (1.0 / (pow(2,dither_bit) - 1.0));
    float3 dither_shift_RGB = float3(dither_shift, -dither_shift, dither_shift);
    dither_shift_RGB = lerp(2.0 * dither_shift_RGB, -2.0 * dither_shift_RGB, grid_position);
    accu.rgb += dither_shift_RGB;
    #elif (DebandDithering == 2)
    //Random dithering
    float dither_bit  = 8.0;
    float sine = sin(dot(texcoord, float2(12.9898,78.233)));
    float noise = frac(sine * 43758.5453 + texcoord.x);
    float dither_shift = (1.0 / (pow(2,dither_bit) - 1.0));
    float dither_shift_half = (dither_shift * 0.5);
    dither_shift = dither_shift * noise - dither_shift_half;
    accu.rgb += float3(-dither_shift, dither_shift, -dither_shift);
    #elif (DebandDithering == 3)
    float3 vDither = dot(float2(171.0, 231.0), texcoord * screenSize).xxx;
    vDither.rgb = frac( vDither.rgb / float3( 103.0, 71.0, 97.0 ) ) - float3(0.5, 0.5, 0.5);
    accu.rgb += (vDither.rgb / 255.0);
    #endif

    return accu;
}
#endif

/*------------------------------------------------------------------------------
                     [MAIN() & COMBINE PASS CODE SECTION]
------------------------------------------------------------------------------*/

#if GLSL == 1
void ps_main()
#else
PS_OUTPUT ps_main(VS_OUTPUT input)
#endif
{
    #if GLSL == 1
    float2 texcoord = PSin.t;
    float4 position = PSin.p;
    float4 color = texture(TextureSampler, texcoord);
    #else
    PS_OUTPUT output;

    float2 texcoord = input.t;
    float4 position = input.p;
    float4 color = sample_tex(TextureSampler, texcoord);
    #endif

    #if BILINEAR_FILTERING == 1
    color = BiLinearPass(color, texcoord);
    #endif

    #if GAUSSIAN_FILTERING == 1
    color = GaussianPass(color, texcoord);
    #endif

    #if BICUBIC_FILTERING == 1
    color = BiCubicPass(color, texcoord);
    #endif

    #if BICUBLIC_SCALER == 1
    color = BiCubicScalerPass(color, texcoord);
    #endif

    #if LANCZOS_SCALER == 1
    color = LanczosScalerPass(color, texcoord);
    #endif

    #if UHQ_FXAA == 1
    color = FxaaPass(color, texcoord);
    #endif

    #if TEXTURE_SHARPEN == 1
    color = TexSharpenPass(color, texcoord);
    #endif

    #if PAINT_SHADING == 1
    color = PaintPass(color, texcoord);
    #endif

    #if CEL_SHADING == 1
    color = CelPass(color, texcoord);
    #endif

    #if GAMMA_CORRECTION == 1
    color = GammaPass(color, texcoord);
    #endif

    #if PIXEL_VIBRANCE == 1
    color = VibrancePass(color, texcoord);
    #endif

    #if COLOR_GRADING == 1
    color = ColorGrading(color, texcoord);
    #endif

    #if COLOR_CORRECTION == 1
    color = CorrectionPass(color, texcoord);
    #endif

    #if CROSS_PROCESSING == 1
    color = CrossPass(color, texcoord);
    #endif

    #if SCENE_TONEMAPPING == 1
    color = TonemapPass(color, texcoord);
    #endif

    #if BLENDED_BLOOM == 1
    color = BloomPass(color, texcoord);
    #endif

    #if CURVE_CONTRAST == 1
    color = ContrastPass(color, texcoord);
    #endif

    #if COLOR_TEMPERATURE == 1
    color = TemperaturePass(color, texcoord);
    #endif

    #if VIGNETTE == 1
    color = VignettePass(color, texcoord);
    #endif

    #if LOTTES_CRT == 1
    color = LottesCRTPass(color, texcoord, position);
    #endif

    #if SCANLINES == 1
    color = ScanlinesPass(color, texcoord, position);
    #endif
    
    #if SP_DITHERING == 1
    color = DitherPass(color, texcoord);
    #endif

    #if DEBANDING == 1
    color = DebandPass(color, texcoord);
    #endif

    #if PX_BORDER == 1
    color = BorderPass(color, texcoord);
    #endif

    #if GLSL == 1
    SV_Target0 = color;
    #else
    output.c = color;

    return output;
    #endif
}
