#if defined(SHADER_MODEL) || defined(FXAA_GLSL_130)

#ifndef FXAA_GLSL_130
    #define FXAA_GLSL_130 0
#endif

#define UHQ_FXAA 1          //High Quality Fast Approximate Anti Aliasing. Adapted for GSdx from Timothy Lottes FXAA 3.11.
#define FxaaSubpixMax 0.0   //[0.00 to 1.00] Amount of subpixel aliasing removal. 0.00: Edge only antialiasing (no blurring)
#define FxaaEarlyExit 1     //[0 or 1] Use Fxaa early exit pathing. When disabled, the entire scene is antialiased(FSAA). 0 is off, 1 is on.

/*------------------------------------------------------------------------------
							 [GLOBALS|FUNCTIONS]
------------------------------------------------------------------------------*/
#if (FXAA_GLSL_130 == 1)

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

#if (SHADER_MODEL >= 0x400)
Texture2D Texture : register(t0);
SamplerState TextureSampler : register(s0);
#else
texture2D Texture : register(t0);
sampler2D TextureSampler : register(s0);
#define SamplerState sampler2D
#endif

cbuffer cb0
{
	float4 _rcpFrame : register(c0);
};

struct VS_INPUT
{
	float4 p : POSITION;
	float2 t : TEXCOORD0;
};

struct VS_OUTPUT
{
	#if (SHADER_MODEL >= 0x400)
	float4 p : SV_Position;
	#else
	float4 p : TEXCOORD1;
	#endif
	float2 t : TEXCOORD0;
};

struct PS_OUTPUT
{
	#if (SHADER_MODEL >= 0x400)
	float4 c : SV_Target0;
	#else
	float4 c : COLOR0;
	#endif
};

#endif

/*------------------------------------------------------------------------------
                             [FXAA CODE SECTION]
------------------------------------------------------------------------------*/

#if (SHADER_MODEL >= 0x500)
#define FXAA_HLSL_5 1
#define FXAA_GATHER4_ALPHA 1
#elif (SHADER_MODEL >= 0x400)
#define FXAA_HLSL_4 1
#define FXAA_GATHER4_ALPHA 0
#elif (FXAA_GLSL_130 == 1)
#define FXAA_GATHER4_ALPHA 1
#else
#define FXAA_HLSL_3 1
#define FXAA_GATHER4_ALPHA 0
#endif

#if (FXAA_HLSL_5 == 1)
struct FxaaTex { SamplerState smpl; Texture2D tex; };
#define FxaaTexTop(t, p) t.tex.SampleLevel(t.smpl, p, 0.0)
#define FxaaTexOff(t, p, o, r) t.tex.SampleLevel(t.smpl, p, 0.0, o)
#define FxaaTexAlpha4(t, p) t.tex.GatherAlpha(t.smpl, p)
#define FxaaTexOffAlpha4(t, p, o) t.tex.GatherAlpha(t.smpl, p, o)
#define FxaaDiscard clip(-1)
#define FxaaSat(x) saturate(x)

#elif (FXAA_HLSL_4 == 1)
struct FxaaTex { SamplerState smpl; Texture2D tex; };
#define FxaaTexTop(t, p) t.tex.SampleLevel(t.smpl, p, 0.0)
#define FxaaTexOff(t, p, o, r) t.tex.SampleLevel(t.smpl, p, 0.0, o)
#define FxaaDiscard clip(-1)
#define FxaaSat(x) saturate(x)

#elif (FXAA_HLSL_3 == 1)
#define FxaaTex sampler2D
#define int2 float2
#define FxaaSat(x) saturate(x)
#define FxaaTexTop(t, p) tex2Dlod(t, float4(p, 0.0, 0.0))
#define FxaaTexOff(t, p, o, r) tex2Dlod(t, float4(p + (o * r), 0, 0))

#elif (FXAA_GLSL_130 == 1)

#define int2 ivec2
#define float2 vec2
#define float3 vec3
#define float4 vec4
#define FxaaDiscard discard
#define FxaaSat(x) clamp(x, 0.0, 1.0)
#define FxaaTex sampler2D
#define FxaaTexTop(t, p) textureLod(t, p, 0.0)
#define FxaaTexOff(t, p, o, r) textureLodOffset(t, p, 0.0, o)
#if (FXAA_GATHER4_ALPHA == 1)
// use #extension GL_ARB_gpu_shader5 : enable
#define FxaaTexAlpha4(t, p) textureGather(t, p, 3)
#define FxaaTexOffAlpha4(t, p, o) textureGatherOffset(t, p, o, 3)
#endif

#endif

#define FxaaEdgeThreshold 0.063
#define FxaaEdgeThresholdMin 0.00
#define FXAA_QUALITY__P0 1.0
#define FXAA_QUALITY__P1 1.5
#define FXAA_QUALITY__P2 2.0
#define FXAA_QUALITY__P3 2.0
#define FXAA_QUALITY__P4 2.0
#define FXAA_QUALITY__P5 2.0
#define FXAA_QUALITY__P6 2.0
#define FXAA_QUALITY__P7 2.0
#define FXAA_QUALITY__P8 2.0
#define FXAA_QUALITY__P9 2.0
#define FXAA_QUALITY__P10 4.0
#define FXAA_QUALITY__P11 8.0
#define FXAA_QUALITY__P12 8.0

/*------------------------------------------------------------------------------
                        [GAMMA PREPASS CODE SECTION]
------------------------------------------------------------------------------*/
float RGBLuminance(float3 color)
{
	const float3 lumCoeff = float3(0.2126729, 0.7151522, 0.0721750);
	return dot(color.rgb, lumCoeff);
}

#if (FXAA_GLSL_130 == 0)
#define PixelSize float2(_rcpFrame.x, _rcpFrame.y)
#endif


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

float4 PreGammaPass(float4 color, float2 uv0)
{
	#if (SHADER_MODEL >= 0x400)
		color = Texture.Sample(TextureSampler, uv0);
    #elif (FXAA_GLSL_130 == 1)
		color = texture(TextureSampler, uv0);
	#else
		color = tex2D(TextureSampler, uv0);
	#endif

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

	#if (FXAA_GATHER4_ALPHA == 1)
	float4 rgbyM = FxaaTexTop(tex, posM);
	float4 luma4A = FxaaTexAlpha4(tex, posM);
	float4 luma4B = FxaaTexOffAlpha4(tex, posM, int2(-1, -1));
	rgbyM.w = RGBLuminance(rgbyM.xyz);

	#define lumaM rgbyM.w
	#define lumaE luma4A.z
	#define lumaS luma4A.x
	#define lumaSE luma4A.y
	#define lumaNW luma4B.w
	#define lumaN luma4B.z
	#define lumaW luma4B.x
    
	#else
	float4 rgbyM = FxaaTexTop(tex, posM);
	rgbyM.w = RGBLuminance(rgbyM.xyz);
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
	#if (FxaaEarlyExit == 1)
	if(earlyExit) { return rgbyM; }
	#endif

	#if (FXAA_GATHER4_ALPHA == 0)
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

#if (FXAA_GLSL_130 == 1)
float4 FxaaPass(float4 FxaaColor, float2 uv0)
#else
float4 FxaaPass(float4 FxaaColor : COLOR0, float2 uv0 : TEXCOORD0)
#endif
{

	#if (SHADER_MODEL >= 0x400)
	FxaaTex tex;
	tex.tex = Texture;
	tex.smpl = TextureSampler;

	Texture.GetDimensions(PixelSize.x, PixelSize.y);
	FxaaColor = FxaaPixelShader(uv0, tex, 1.0/PixelSize.xy, FxaaSubpixMax, FxaaEdgeThreshold, FxaaEdgeThresholdMin);

    #elif (FXAA_GLSL_130 == 1)

	vec2 PixelSize = textureSize(TextureSampler, 0);
	FxaaColor = FxaaPixelShader(uv0, TextureSampler, 1.0/PixelSize.xy, FxaaSubpixMax, FxaaEdgeThreshold, FxaaEdgeThresholdMin);

	#else
	FxaaTex tex;
	tex = TextureSampler;
	FxaaColor = FxaaPixelShader(uv0, tex, PixelSize.xy, FxaaSubpixMax, FxaaEdgeThreshold, FxaaEdgeThresholdMin);
	#endif

	return FxaaColor;
}

/*------------------------------------------------------------------------------
                      [MAIN() & COMBINE PASS CODE SECTION]
------------------------------------------------------------------------------*/
#if (FXAA_GLSL_130 == 1)

void ps_main()
{
    vec4 color = texture(TextureSampler, PSin.t);
    color      = PreGammaPass(color, PSin.t);
    color      = FxaaPass(color, PSin.t);

    SV_Target0 = color;
}

#else

PS_OUTPUT ps_main(VS_OUTPUT input)
{
	PS_OUTPUT output;

	#if (SHADER_MODEL >= 0x400)
		float4 color = Texture.Sample(TextureSampler, input.t);

		color = PreGammaPass(color, input.t);
		color = FxaaPass(color, input.t);
	#else
		float4 color = tex2D(TextureSampler, input.t);

		color = PreGammaPass(color, input.t);
		color = FxaaPass(color, input.t);
	#endif

	output.c = color;
	
	return output;
}

#endif

#endif
