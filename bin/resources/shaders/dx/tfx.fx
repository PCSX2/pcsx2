// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#ifndef PCSX2_DX12
	#define PCSX2_DX12 0
#endif

#ifndef PCSX2_DX11
	#define PCSX2_DX11 0
#endif

#if PCSX2_DX12 == PCSX2_DX11
	ERROR: Exactly one of PCSX2_DX12 or PCSX2_DX11 should be true.
#endif

/// Start helper macros for shared shader code.

// Types
#define FLOAT2 float2
#define FLOAT3 float3
#define FLOAT4 float4
#define FLOAT2x2 float2x2
#define FLOAT2x4 float2x4
#define FLOAT4x4 float4x4
#define UINT2 uint2
#define UINT3 uint3
#define UINT4 uint4
#define INT2 int2
#define INT3 int3
#define INT4 int4
#define USHORT uint
#define USHORT2 uint2
#define USHORT3 uint3
#define USHORT4 uint4
#define SHORT int
#define SHORT2 int2
#define SHORT3 int3
#define SHORT4 int4
#define BOOL2 bool2
#define BOOL3 bool3
#define BOOL4 bool4

// Builtin keywords/functions
#define STATIC
#define DFDX ddx
#define DFDY ddy
#define SELECT(COND, TRUE_VAL, FALSE_VAL) ((COND) ? (FALSE_VAL) : (TRUE_VAL))
#define VEQUAL(X, Y) ((X) == (Y))
#define VGEQUAL(X, Y) ((X) >= (Y))
#define VLEQUAL(X, Y) ((X) <= (Y))
#define VGREATER(X, Y) ((X) > (Y))
#define VLESS(X, Y) ((X) < (Y))
#define VNOTEQUAL(X, Y) ((X) != (Y))
#define RSQRT(X) rsqrt(X)
#define GPU_DISCARD discard
#define SATURATE(X) saturate(X)
#define FLOAT_BITCAST_UINT(X) asuint(X)
#define FLOAT4_BITCAST_UINT4(X) asuint(X)
#define UINT_BITCAST_UCHAR4(X) UINT4((X) & 0xFFu, ((X) >> 8) & 0xFFu, ((X) >> 16) & 0xFFu, ((X) >> 24) & 0xFFu)
// Warning: X, Y opposite order of GLSL and MSL!
#define MAT_MUL(X, Y) mul((Y), (X))
// Warning: X, Y opposite order of GLSL and MSL!
#define MAT_GET(MAT, X, Y) MAT[X][Y]
#define FRACT(X) frac(X)
#define MIX lerp
#define IN_PARAM(TYPE, NAME) TYPE NAME
#define IN_OUT_PARAM(TYPE, NAME) inout TYPE NAME
// FXC (<=SM5.1) may optimise away isnan and isinf.
// DXC (>=SM6.0) will preserve them.
#ifdef __hlsl_dx_compiler
	#define IS_NAN_OR_INF_4(X) (isinf(X) | isnan(X))
#else
	#define IS_NAN_OR_INF_4(X) ((asuint(X) & 0x7f800000) == 0x7f800000)
#endif
#define UNROLL [unroll]

// Constants
#define PRIMID_MAX 0x7FFFFFFF
#define VS_Y_FLIP -1.0f
#define EXP2_NEG_32 exp2(-32.0f)
#define EXP2_POS_32 exp2(32.0f)

// Vertex shader helpers
#define VS_SCALE_RAW_Z(Z) (float(Z) * EXP2_NEG_32)
#define VS_VERTICES_PARAM(NAME) uint NAME
#define VS_INDICES_PARAM(NAME) uint NAME
#define VS_BASE_VERTEX BaseVertex
#define VS_BASE_INDEX BaseIndex
#define VS_LOAD_VERTEX(VERTICES, IDX) (VertexBuffer.Load(IDX))
#define VS_LOAD_INDEX(INDICES, IDX) (IndexBuffer.Load(IDX))
#define VS_NEEDS_EXPAND (VS_EXPAND_TYPE != VS_EXPAND_NONE)
// Unused in DX
#define VS_POINT_SIZE 0
#define BROKEN_SHADER_DEPTH 0

// Pixel shader helpers
#define PS_SAMPLE_TEX(POS) (Texture.Sample(TextureSampler, FLOAT2(POS)))
#define PS_SAMPLE_TEX_LOD(POS, LOD) (Texture.SampleLevel(TextureSampler, FLOAT2(POS), float(LOD)))
#define PS_SAMPLE_TEX_DEPTH(POS) (PS_SAMPLE_TEX((POS)).r)
#define PS_SAMPLE_TEX_DEPTH_LOD(POS, LOD) (PS_SAMPLE_TEX_LOD((POS), (LOD)).r)
#define PS_READ_TEX(POS, LOD) (Texture.Load(INT3(INT2(POS), int(LOD))))
#define PS_READ_TEX_DEPTH(POS, LOD) (PS_READ_TEX((POS), (LOD)).r)
#define PS_READ_PALETTE(POS) (Palette.Load(INT3(INT2(POS), 0)))
#define PS_READ_PRIMID(POS) (PrimMinTexture.Load(INT3(INT2(POS), 0)).r)
#define PS_GET_TEX_DIMS(OUT_VAR) (Texture.GetDimensions(OUT_VAR.x, OUT_VAR.y))
#define PS_GET_TEX_DEPTH_DIMS(OUT_VAR) (PS_GET_TEX_DIMS(OUT_VAR))

/// End helper macros for shared shader code.

#include "tfx_defs.inc"

#ifdef VERTEX_SHADER

/// VS constants for determining base vertex/index in expand shader.
#if PCSX2_DX12
cbuffer cb2 : register(b2)
#elif PCSX2_DX11
cbuffer cb2 : register(b1)
#endif
{
	#define X(TYPE, NAME) TYPE NAME;
		VS_PUSH_CONSTANTS(X)
	#undef X
};

#if VS_EXPAND_TYPE != VS_EXPAND_NONE
// Vertex buffer for expand shaders (sprites, upscaled lines, AA1 edges, etc.)
StructuredBuffer<VSRawVertex> VertexBuffer : register(t0);

// Index buffer for rearranging vertices in AA1 expand shader
StructuredBuffer<uint> IndexBuffer : register(t5);
#endif // VS_EXPAND_TYPE

// Note: vertex/index buffers must be defined before common code is included.
#include "tfx_vs.inc"

struct VS_INPUT
{
	float2 st : TEXCOORD0;
	uint4 c : COLOR0;
	float q : TEXCOORD1;
	uint2 p : POSITION0;
	uint z : POSITION1;
	uint2 uv : TEXCOORD2;
	float4 f : COLOR1;
};

struct VS_OUTPUT
{
	float4 p : SV_Position;
	float4 t : TEXCOORD0;
	float4 ti : TEXCOORD2;

#if VS_IIP != 0
	float4 c : COLOR0;
#else
	nointerpolation float4 c : COLOR0;
#endif

	float inv_cov : COLOR1; // We use the inverse to make it simpler to interpolate.
	nointerpolation uint interior : COLOR2; // 1 for triangle interior; 0 for edge;
};

// VS Constant Buffer
cbuffer cb0 : register(b0)
{
	#define X(TYPE, NAME) TYPE NAME;
		VS_UNIFORMS(X)
	#undef X
};

// Convert VS constants for shared code.
VSUniformsGeneric GetVSUniforms()
{
	VSUniformsGeneric cb;
	#define X(TYPE, NAME) cb.NAME = NAME;
		VS_UNIFORMS(X)
	#undef X
	return cb;
}

// Convert VS inputs for shared code.
VSInputGeneric GetVSInput(VS_INPUT vin)
{
	VSInputGeneric vin_gen;
	vin_gen.st = vin.st;
	vin_gen.c = FLOAT4(vin.c);
	vin_gen.q = vin.q;
	vin_gen.p = vin.p;
	vin_gen.z = vin.z;
	vin_gen.uv = vin.uv;
	vin_gen.f = vin.f;
	return vin_gen;
}

// Convert VS outputs from generic outputs to real outputs.
VS_OUTPUT GetVSOutput(VSOutputGeneric vout_gen)
{
	VS_OUTPUT vout;
	vout.p = vout_gen.p;
	vout.t = vout_gen.t;
	vout.ti = vout_gen.ti;
	vout.c = vout_gen.c;
	vout.inv_cov = vout_gen.inv_cov;
	vout.interior = vout_gen.interior;
	return vout;
}

#if VS_EXPAND_TYPE == VS_EXPAND_NONE

VS_OUTPUT vs_main(VS_INPUT vin)
{
	VSInputGeneric vin_gen = GetVSInput(vin);
	VSUniformsGeneric cb = GetVSUniforms();
	VSOutputGeneric vout_gen = vs_main_impl(vin_gen, cb);
	return GetVSOutput(vout_gen);
}

#else // VS_EXPAND_TYPE

VS_OUTPUT vs_main_expand(uint vid : SV_VertexID)
{
	VSUniformsGeneric cb = GetVSUniforms();
	VSOutputGeneric vout_gen = vs_expand_impl(vid, 0, cb, 0);
	return GetVSOutput(vout_gen);
}

#endif // VS_EXPAND_TYPE

#endif // VERTEX_SHADER

#ifdef PIXEL_SHADER

// Pixel shader input
struct PS_INPUT
{
	noperspective centroid float4 p : SV_Position;
	float4 t : TEXCOORD0;
	float4 ti : TEXCOORD2;
#if PS_IIP != 0
	float4 c : COLOR0;
#else
	nointerpolation float4 c : COLOR0;
#endif
	float inv_cov : COLOR1; // We use the inverse to make it simpler to interpolate.
	nointerpolation uint interior : COLOR2; // 1 for triangle interior; 0 for edge;
#if NEED_PRIMID
	uint prim_id : SV_PrimitiveID;
#endif
};

// Pixel shader output
struct PS_OUTPUT
{
#if PS_RETURN_COLOR
	float4 c0 : SV_Target0;
	#if !PS_NO_COLOR1
		float4 c1 : SV_Target1;
	#endif
#endif

#if PS_RETURN_DEPTH
	// In DX12 we do depth feedback loops with a color copy.
	#if SW_DEPTH && PS_NO_COLOR1 && PS_DEPTH_FEEDBACK_SUPPORT == 2
		#if PS_RETURN_COLOR
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
};

// Pixel shader resources
Texture2D<float4> Texture : register(t0);
SamplerState TextureSampler : register(s0);
Texture2D<float4> Palette : register(t1);
Texture2D<float> PrimMinTexture : register(t3);
#if PS_ROV_COLOR
	RasterizerOrderedTexture2D<unorm float4> RtTextureRov : register(u0);
#else
	Texture2D<float4> RtTexture : register(t2);
#endif
#if PS_ROV_DEPTH
	RasterizerOrderedTexture2D<float> DepthTextureRov : register(u1);
#else
	Texture2D<float> DepthTexture : register(t4);
#endif

// Pixel shader constant buffer.
#if PCSX2_DX12
cbuffer cb1 : register(b1)
#elif PCSX2_DX11
cbuffer cb1 : register(b0)
#endif
{
	#define X(TYPE, NAME) TYPE NAME;
		PS_UNIFORMS(X)
	#undef X
};

// Get pixel shader input for passing to shared code.
PSInputGeneric GetPSInput(PS_INPUT ps_in)
{
	PSInputGeneric psin_gen;
	psin_gen.p = ps_in.p;
	psin_gen.t = ps_in.t;
	psin_gen.ti = ps_in.ti;
	psin_gen.c = ps_in.c;
	psin_gen.fc = ps_in.c;
	psin_gen.inv_cov = ps_in.inv_cov;
	psin_gen.interior = ps_in.interior;
	return psin_gen;
}

// Get pixel shader constants for passing to shared code.
PSUniformsGeneric GetPSUniforms()
{
	PSUniformsGeneric cb;
	#define X(TYPE, NAME) cb.NAME = NAME;
		PS_UNIFORMS(X)
	#undef X
	return cb;
}

float4 RtLoad(int2 xy)
{
#if PS_ROV_COLOR
	return RtTextureRov[xy];
#else
	return RtTexture.Load(int3(int2(xy), 0));
#endif
}

float DepthLoad(int2 xy)
{
#if PS_ROV_DEPTH
	return DepthTextureRov[xy];
#else
	return DepthTexture.Load(int3(int2(xy), 0));
#endif
}

void RtWrite(int2 xy, float4 c)
{
#if PS_ROV_COLOR
	RtTextureRov[xy] = c;
#endif
}

void DepthWrite(int2 xy, float d)
{
#if PS_ROV_DEPTH
	DepthTextureRov[xy] = d;
#endif
}

// Pixel shader global state
static PSInputGeneric ps_in;
static PSUniformsGeneric ps_cb;
static float4 ps_current_color;
static float ps_current_depth;
static uint ps_prim_id;
static bool ps_color_discarded;
static bool ps_depth_discarded;

#include "tfx_ps_header.inc"
#include "tfx_ps_util.inc"
#include "tfx_ps_sample_af.inc"
#include "tfx_ps_fetch.inc"
#include "tfx_ps_sample.inc"
#include "tfx_ps_tfx.inc"
#include "tfx_ps_atst.inc"
#include "tfx_ps_fog.inc"
#include "tfx_ps_color.inc"
#include "tfx_ps_post.inc"
#include "tfx_ps_blend.inc"
#include "tfx_ps_main.inc"

#if PS_ROV_EARLYDEPTHSTENCIL
[earlydepthstencil]
#endif

#if (PS_RETURN_COLOR || PS_RETURN_DEPTH)
PS_OUTPUT ps_main(PS_INPUT input)
#else
void ps_main(PS_INPUT input)
#endif
{
	ps_in = GetPSInput(input);
	ps_cb = GetPSUniforms();
	#if NEED_PRIMID
		ps_prim_id = input.prim_id;
	#else
		ps_prim_id = 0;
	#endif
	ps_color_discarded = false;
	ps_depth_discarded = false;

	int2 coord = int2(ps_in.p.xy);

	ps_current_depth = DepthLoad(coord);

	ps_current_color = RtLoad(coord);

	#if (PS_RETURN_COLOR || PS_RETURN_DEPTH)
		PS_OUTPUT psout;
	#endif

	PSOutputGeneric psout_gen = ps_main_impl();

	// Color write back
	#if PS_RETURN_COLOR
		psout.c0 = psout_gen.c0;
		#if !PS_NO_COLOR1
			psout.c1 = psout_gen.c1;
		#endif
	#elif PS_RETURN_COLOR_ROV
		psout_gen.c0 = (fbmask == 0xFFu) ? ps_current_color : psout_gen.c0; // channel masking
		if (!ps_color_discarded)
			RtTextureRov[ps_in.p.xy] = psout_gen.c0;
	#endif

	// Depth write back
	#if PS_RETURN_DEPTH
		psout.depth = psout_gen.depth;
		#if SW_DEPTH && PS_NO_COLOR1 && PS_DEPTH_FEEDBACK_SUPPORT == 2
			// Output color clone for feedback.
			psout.depth_color = psout_gen.depth;
		#endif
	#elif PS_RETURN_DEPTH_ROV
		if (!ps_depth_discarded)
			DepthTextureRov[ps_in.p.xy] = psout_gen.depth;
	#endif

	#if (PS_RETURN_COLOR || PS_RETURN_DEPTH)
		return psout;
	#endif
}

#endif // PIXEL_SHADER
