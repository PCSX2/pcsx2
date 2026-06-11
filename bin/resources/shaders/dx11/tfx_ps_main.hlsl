// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "tfx_defines.hlsl"
#include "tfx_ps_resources.hlsl"

#ifndef VS_IIP
#define VS_IIP 0
#endif

#ifndef GS_IIP
#define GS_IIP 0
#define GS_FORWARD_PRIMID 0
#endif

#ifndef ZTST_GEQUAL
#define ZTST_GEQUAL 2
#define ZTST_GREATER 3
#endif

#ifndef AFAIL_KEEP
#define AFAIL_KEEP 0
#define AFAIL_FB_ONLY 1
#define AFAIL_ZB_ONLY 2
#define AFAIL_RGB_ONLY 3
#define AFAIL_RGB_ONLY_DSB 4
#define AFAIL_RGB_ONLY_SW_Z 5
#endif

#ifndef PS_AA1_NONE
#define PS_AA1_NONE 0
#define PS_AA1_LINE 1
#define PS_AA1_TRIANGLE 2
#define PS_AA1_TRIANGLE_SW_Z 3
#endif

#ifndef PS_ROV_DEPTH_NONE
#define PS_ROV_DEPTH_NONE 0
#define PS_ROV_DEPTH_READ_WRITE 1
#define PS_ROV_DEPTH_READ_ONLY 2
#endif

#ifndef PS_IIP
#define PS_IIP 0
#define PS_SHUFFLE 0
#define PS_SHUFFLE_SAME 0
#define PS_PROCESS_BA 0
#define PS_PROCESS_RG 0
#define PS_SHUFFLE_ACROSS 0
#define PS_READ16_SRC 0
#define PS_WRITE_RG 0
#define PS_COLCLIP_HW 0
#define PS_FIXED_ONE_A 0
#define PS_ZCLAMP 0
#define PS_ZFLOOR 0
#define PS_SCANMSK 0
#define PS_NO_COLOR 0
#define PS_NO_COLOR1 0
#define PS_DATE 0
#define PS_AA1 0
#define PS_ABE 0
#define PS_AFAIL 0
#define PS_ZTST 0

#define PS_ROV_COLOR 0
#define PS_ROV_DEPTH 0
#endif

#define NEEDS_RT_FOR_AFAIL (PS_AFAIL == AFAIL_ZB_ONLY || PS_AFAIL == AFAIL_RGB_ONLY || PS_AFAIL == AFAIL_RGB_ONLY_SW_Z)
#define NEEDS_DEPTH_FOR_AFAIL (PS_AFAIL == AFAIL_FB_ONLY || PS_AFAIL == AFAIL_RGB_ONLY_SW_Z)
#define NEEDS_DEPTH_FOR_ZTST (PS_ZTST == ZTST_GEQUAL || PS_ZTST == ZTST_GREATER)
#define NEEDS_DEPTH_FOR_AA1 (PS_AA1 == PS_AA1_TRIANGLE_SW_Z)
#define SW_DEPTH (NEEDS_DEPTH_FOR_AFAIL || NEEDS_DEPTH_FOR_ZTST || NEEDS_DEPTH_FOR_AA1)
#define ZWRITE (PS_ZFLOOR || PS_ZCLAMP || SW_DEPTH)

#define PS_RETURN_COLOR_ROV (!PS_NO_COLOR && PS_ROV_COLOR)
#define PS_RETURN_COLOR (!PS_NO_COLOR && !PS_ROV_COLOR)
#define PS_RETURN_DEPTH_ROV (PS_ROV_DEPTH == PS_ROV_DEPTH_READ_WRITE)
#define PS_RETURN_DEPTH (ZWRITE && !PS_ROV_DEPTH)
#define PS_ROV_EARLYDEPTHSTENCIL (PS_ROV_COLOR && !PS_ROV_DEPTH && !ZWRITE)

#if !PS_ROV_DEPTH
Texture2D<float> DepthTexture : register(t4);
#else
RasterizerOrderedTexture2D<float> DepthTextureRov : register(u1);
static float rov_depth_value;
#endif

struct PS_INPUT
{
	noperspective centroid float4 p : SV_Position;
	float4 t : TEXCOORD0;
	float4 ti : TEXCOORD2;
#if VS_IIP != 0 || GS_IIP != 0 || PS_IIP != 0
	float4 c : COLOR0;
#else
	nointerpolation float4 c : COLOR0;
#endif
	float inv_cov : COLOR1; // We use the inverse to make it simpler to interpolate.
	nointerpolation uint interior : COLOR2; // 1 for triangle interior; 0 for edge;
#if (PS_DATE >= 1 && PS_DATE <= 3) || GS_FORWARD_PRIMID
	uint primid : SV_PrimitiveID;
#endif
};

struct PS_OUTPUT
{
#define NUM_RTS 0

#if PS_RETURN_COLOR
	#if PS_DATE == 1 || PS_DATE == 2
		float c : SV_Target;
	#else
		
		float4 c0 : SV_Target0;

		#undef NUM_RTS
		#define NUM_RTS 1
		
		#if !PS_NO_COLOR1
			float4 c1 : SV_Target1;
		#endif
	#endif
#endif

#if PS_RETURN_DEPTH
	// In DX12 we do depth feedback loops with a color copy.
	#if SW_DEPTH && PS_NO_COLOR1 && PS_DEPTH_FEEDBACK_SUPPORT == 2
		#if NUM_RTS > 0
			float depth_color : SV_Target1;
		#else
			float depth_color : SV_Target0;
		#endif
	#endif
	#if PS_HAS_CONSERVATIVE_DEPTH && !SW_DEPTH
		float depth : SV_DepthLessEqual;
	#else
		float depth : SV_Depth;
	#endif
#endif

#undef NUM_RTS
};

void RtInit(int2 xy);
float4 RtLoad(int2 xy);
void RtWrite(int2 xy, float4 c);

bool atst(float4 C);

float4 ps_color(float4 p, float4 t, float4 ti, float4 c);
void ps_fbmask(inout float4 C, float2 pos_xy);
void ps_dither(inout float3 C, float As, float2 pos_xy);
void ps_color_clamp_wrap(inout float3 C);
void ps_blend(inout float4 Color, inout float4 As_rgba, float2 pos_xy);
float4 ps_alpha_blend(float2 p, float Ca);
void ps_alpha_correction(inout float Ca);

float rta_correction_factor();
float rta_correction_lim();

float DepthLoad(int2 xy)
{
#if PS_ROV_DEPTH
	return rov_depth_value;
#else
	return DepthTexture.Load(int3(int2(xy), 0));
#endif
}

void DepthWrite(int2 xy, float d)
{
#if PS_ROV_DEPTH
	DepthTextureRov[xy] = d;
#endif
}

#if PS_ROV_EARLYDEPTHSTENCIL
[earlydepthstencil]
#endif

#if PS_ROV_COLOR || PS_ROV_DEPTH
	#define DISCARD { rov_discard_color = true; rov_discard_depth = true; }
	#define DISCARD_COLOR rov_discard_color = true
	#define DISCARD_DEPTH rov_discard_depth = true
#else
	#define DISCARD discard
	#define DISCARD_COLOR o_col0 = RtLoad(input.p.xy)
	#define DISCARD_DEPTH input.p.z = DepthLoad(input.p.xy)
#endif

#ifndef MONOLITHIC
[shader("pixel")]
#endif
#if (PS_RETURN_COLOR || PS_RETURN_DEPTH)
PS_OUTPUT ps_main(PS_INPUT input)
#else
void ps_main(PS_INPUT input)
#endif
{
	// Must floor before depth testing.
#if PS_ZFLOOR
	input.p.z = floor(input.p.z * exp2(32.0f)) * exp2(-32.0f);
#endif

#if PS_ROV_COLOR
	RtInit(input.p.xy);
#endif

#if PS_ROV_DEPTH
	rov_depth_value = DepthTextureRov[input.p.xy];
#endif

#if PS_ROV_COLOR || PS_ROV_DEPTH
	bool rov_discard_color = false;
	bool rov_discard_depth = false;
#endif

	// Use ROV discard macro for since we cannot do
	// conditional discard based on value read from ROV.
#if PS_ZTST == ZTST_GEQUAL
	if (input.p.z < DepthLoad(input.p.xy))
		DISCARD;
#elif PS_ZTST == ZTST_GREATER
	if (input.p.z <= DepthLoad(input.p.xy))
		DISCARD;
#endif

	float4 C = ps_color(input.p, input.t, input.ti, input.c);

#if PS_AA1
	#if PS_AA1 == PS_AA1_LINE
		// Blur only outer part of the line by scaling coverage.
		float cov = clamp(LineCovScale * (1.0f - abs(input.inv_cov)), 0.0f, 1.0f);
	#else
		float cov = clamp(1.0f - abs(input.inv_cov), 0.0f, 1.0f);
	#endif
	#if PS_ABE
		if (floor(C.a) == 128.0f) // The coverage is only used if the fragment alpha is 128.
			C.a = 128.0f * cov;
	#else
		C.a = 128.0f * cov;
	#endif
#elif PS_FIXED_ONE_A
	// AA (Fixed one) will output a coverage of 1.0 as alpha
	C.a = 128.0f;
#endif

	bool atst_pass = atst(C);

#if PS_AFAIL == AFAIL_KEEP
	// atst_pass always true with PS_ATST_NONE
    if (!atst_pass)
        discard;
#endif

	if (PS_SCANMSK & 2)
	{
		// fail depth test on prohibited lines
		if ((int(input.p.y) & 1) == (PS_SCANMSK & 1))
			discard;
	}

	float4 alpha_blend = ps_alpha_blend(input.p.xy, C.a);

	// Alpha correction
	ps_alpha_correction(C.a);

#if PS_DATE >= 5

#if PS_WRITE_RG == 1
	// Pseudo 16 bits access.
	float rt_a = RtLoad(input.p.xy).g;
#else
	float rt_a = RtLoad(input.p.xy).a;
#endif

#if (PS_DATE & 3) == 1
	// DATM == 0: Pixel with alpha equal to 1 will failed
	bool bad = rta_correction_lim() < rt_a;
#elif (PS_DATE & 3) == 2
	// DATM == 1: Pixel with alpha equal to 0 will failed
	bool bad = rt_a < rta_correction_lim();
#endif

	if (bad)
		discard;

#endif

#if PS_DATE == 3
	// Note gl_PrimitiveID == stencil_ceil will be the primitive that will update
	// the bad alpha value so we must keep it.
	int stencil_ceil = int(PrimMinTexture.Load(int3(input.p.xy, 0)));
	if (int(input.primid) > stencil_ceil)
		discard;
#endif

	// Output values
#if !PS_NO_COLOR
	#if PS_DATE == 1 || PS_DATE == 2
		float o_col0;
	#else
		float4 o_col0;
		#if !PS_NO_COLOR1
			float4 o_col1;
		#endif
	#endif
#endif

	// Get first primitive that will write a failling alpha value
#if PS_DATE == 1
	// DATM == 0
	// Pixel with alpha equal to 1 will failed (128-255)
	o_col0 = (C.a > 127.5f) ? float(input.primid) : float(0x7FFFFFFF);

#elif PS_DATE == 2

	// DATM == 1
	// Pixel with alpha equal to 0 will failed (0-127)
	o_col0 = (C.a < 127.5f) ? float(input.primid) : float(0x7FFFFFFF);

#else
	// Not primid DATE setup

	ps_blend(C, alpha_blend, input.p.xy);

	if (PS_SHUFFLE)
	{
		if (!PS_SHUFFLE_SAME && !PS_READ16_SRC && !(PS_PROCESS_BA == SHUFFLE_READWRITE && PS_PROCESS_RG == SHUFFLE_READWRITE))
		{
			uint4 denorm_c_after = uint4(C);
			if (PS_PROCESS_BA & SHUFFLE_READ)
			{
				C.b = float(((denorm_c_after.r >> 3) & 0x1Fu) | ((denorm_c_after.g << 2) & 0xE0u));
				C.a = float(((denorm_c_after.g >> 6) & 0x3u) | ((denorm_c_after.b >> 1) & 0x7Cu) | (denorm_c_after.a & 0x80u));
			}
			else
			{
				C.r = float(((denorm_c_after.r >> 3) & 0x1Fu) | ((denorm_c_after.g << 2) & 0xE0u));
				C.g = float(((denorm_c_after.g >> 6) & 0x3u) | ((denorm_c_after.b >> 1) & 0x7Cu) | (denorm_c_after.a & 0x80u));
			}
		}


		// Special case for 32bit input and 16bit output, shuffle used by The Godfather
		if (PS_SHUFFLE_SAME)
		{
			uint4 denorm_c = uint4(C);
			
			if (PS_PROCESS_BA & SHUFFLE_READ)
				C = (float4)(float((denorm_c.b & 0x7Fu) | (denorm_c.a & 0x80u)));
			else
				C.ga = C.rg;
		}
		// Copy of a 16bit source in to this target
		else if (PS_READ16_SRC)
		{
			uint4 denorm_c = uint4(C);
			uint2 denorm_TA = uint2(float2(TA.xy) * 255.0f + 0.5f);
			C.rb = (float2)float((denorm_c.r >> 3) | (((denorm_c.g >> 3) & 0x7u) << 5));
			C.ga = (float2)float((denorm_c.g >> 6) | ((denorm_c.b >> 3) << 2) | (denorm_TA.x & 0x80u));
		}
		else if (PS_SHUFFLE_ACROSS)
		{
			if (PS_PROCESS_BA == SHUFFLE_READWRITE && PS_PROCESS_RG == SHUFFLE_READWRITE)
			{
				C.br = C.rb;
				C.ag = C.ga;
			}
			else if(PS_PROCESS_BA & SHUFFLE_READ)
			{
				C.rb = C.bb;
				C.ga = C.aa;
			}
			else
			{
				C.rb = C.rr;
				C.ga = C.gg;
			}
		}
	}

	ps_dither(C.rgb, alpha_blend.a, input.p.xy);

	// Color clamp/wrap needs to be done after sw blending and dithering
	ps_color_clamp_wrap(C.rgb);

	ps_fbmask(C, input.p.xy);

#if (PS_AFAIL == AFAIL_RGB_ONLY_DSB) && !PS_NO_COLOR1
	// Use alpha blend factor to determine whether to update A.
	alpha_blend.a = float(atst_pass);
#endif

	// Output color scaling
#if !PS_NO_COLOR
	o_col0.a = C.a / rta_correction_factor();
	o_col0.rgb = PS_COLCLIP_HW ? float3(C.rgb / 65535.0f) : C.rgb / 255.0f;
#if !PS_NO_COLOR1
	o_col1 = alpha_blend;
#endif
#endif // !PS_NO_COLOR

	// Alpha test with feedback
#if PS_AFAIL == AFAIL_FB_ONLY
	if (!atst_pass)
		DISCARD_DEPTH;
#elif PS_AFAIL == AFAIL_ZB_ONLY
	if (!atst_pass)
		DISCARD_COLOR;
#elif PS_AFAIL == AFAIL_RGB_ONLY || PS_AFAIL == AFAIL_RGB_ONLY_SW_Z
	if (!atst_pass)
	{
		o_col0.a = RtLoad(input.p.xy).a; // discard alpha
	#if PS_AFAIL == AFAIL_RGB_ONLY_SW_Z
		DISCARD_DEPTH;
	#endif
	}
#endif

#endif // PS_DATE != 1/2

#if PS_ZCLAMP
	input.p.z = min(input.p.z, MaxDepthPS);
#endif

#if PS_AA1 == PS_AA1_TRIANGLE_SW_Z
	if (!bool(input.interior))
		DISCARD_DEPTH; // No depth update for triangle edges.
#endif

#if (PS_RETURN_COLOR || PS_RETURN_DEPTH)
	PS_OUTPUT output;
#endif

	// Color write back
#if PS_RETURN_COLOR
	output.c0 = o_col0;
	#if !PS_NO_COLOR1
		output.c1 = o_col1;
	#endif
#elif PS_RETURN_COLOR_ROV
	#ifdef __hlsl_dx_compiler
		#define SELECT(cond, true_val, false_val) select(cond, true_val, false_val)
	#else
		#define SELECT(cond, true_val, false_val) (cond) ? true_val : false_val
	#endif
	o_col0 = SELECT(FbMask == 0xFFu, RtLoad(input.p.xy), o_col0);
	if (!rov_discard_color)
		RtWrite(input.p.xy, o_col0);
#endif

	// Depth write back
#if PS_RETURN_DEPTH
	output.depth = input.p.z;
#if SW_DEPTH && PS_NO_COLOR1 && PS_DEPTH_FEEDBACK_SUPPORT == 2
		// Output color clone for feedback.
		output.depth_color = input.p.z;
#endif
#elif PS_RETURN_DEPTH_ROV
	if (!rov_discard_depth)
		DepthWrite(input.p.xy, input.p.z);
#endif

#if (PS_RETURN_COLOR || PS_RETURN_DEPTH)
	return output;
#endif
}
