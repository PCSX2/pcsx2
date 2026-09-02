// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GSMTLShaderCommon.h"

/// Start helper macros for shared shader code

// Builtin keywords/functions
#define ddx dfdx
#define ddy dfdy
#define SELECT(COND, TRUE_VAL, FALSE_VAL) ((COND) ? (TRUE_VAL) : (FALSE_VAL))
#define equal(X, Y) ((X) == (Y))
#define greaterThanEqual(X, Y) ((X) >= (Y))
#define lessThanEqual(X, Y) ((X) <= (Y))
#define greaterThan(X, Y) ((X) > (Y))
#define lessThan(X, Y) ((X) < (Y))
#define notEqual(X, Y) ((X) != (Y))
#define discard discard_fragment()
#define FLOAT_BITCAST_UINT(X) as_type<uint>(X)
#define FLOAT4_BITCAST_UINT4(X) as_type<uint4>(X)
#define UINT_BITCAST_UCHAR4(X) as_type<uchar4>(X)
#define MAT_MUL(X, Y) ((X) * (Y))
#define MAT_GET(MAT, X, Y) MAT[Y][X]
#define frac(X) fract(X)
#define lerp mix
#define IN_PARAM(TYPE, NAME) thread const TYPE & NAME
#define IN_OUT_PARAM(TYPE, NAME) thread TYPE & NAME
#define IS_NAN_OR_INF_4(X) (isinf(X) | isnan(X))
#define UNROLL

// Constants
#define PRIMID_MAX FLT_MAX
#define VS_Y_FLIP -1.0f
#define EXP2_NEG_32 0x1p-32f
#define EXP2_POS_32 0x1p+32f

// Vertex shader helpers
#define VS_SCALE_RAW_Z(Z) (float(Z) * EXP2_NEG_32)
#define VS_VERTICES_PARAM(NAME) device const GSMTLMainVertex* NAME [[buffer(GSMTLBufferIndexHWVertices)]]
#define VS_INDICES_PARAM(NAME) device const ushort* NAME [[buffer(GSMTLBufferIndexHWIndices), function_constant(VS_NEEDS_INDEX_BUFFER)]]
#define VS_BASE_VERTEX 0
#define VS_BASE_INDEX 0
#define VS_LOAD_VERTEX(VERTICES, VID) VERTICES[VID]
#define VS_LOAD_INDEX(INDICES, VID) INDICES[VID]

// Pixel shader helpers
#define PS_SAMPLE_TEX(POS) (tex.sample(tex_sampler, float2(POS)))
#define PS_SAMPLE_TEX_LOD(POS, LOD) (tex.sample(tex_sampler, float2(POS), level(LOD)))
#define PS_SAMPLE_TEX_DEPTH(POS) (tex_depth.sample(tex_sampler, float2(POS)))
#define PS_SAMPLE_TEX_DEPTH_LOD(POS, LOD) (tex_depth.sample(tex_sampler, float2(POS), level(LOD)))
#define PS_READ_TEX(POS, LOD) (tex.read(uint2(POS), (LOD)))
#define PS_READ_TEX_DEPTH(POS, LOD) (tex_depth.read(uint2(POS), (LOD)))
#define PS_READ_PALETTE(POS) (palette.read(uint2(POS), 0))
#define PS_READ_PRIMID(POS) (prim_id_tex.read(uint2(POS), 0).r)
#define PS_GET_TEX_DIMS(OUT_VAR) (OUT_VAR = uint2(tex.get_width(), tex.get_height()))
#define PS_GET_TEX_DEPTH_DIMS(OUT_VAR) (OUT_VAR = uint2(tex_depth.get_width(), tex_depth.get_height()))
#define PS_STATIC

// Enum constants
#ifndef VS_EXPAND_NONE
#define VS_EXPAND_NONE VSExpand::None
#define VS_EXPAND_POINT VSExpand::Point
#define VS_EXPAND_LINE VSExpand::Line
#define VS_EXPAND_SPRITE VSExpand::Sprite
#define VS_EXPAND_LINE_AA1 VSExpand::LineAA1
#define VS_EXPAND_TRIANGLE_AA1 VSExpand::TriangleAA1
#endif

#ifndef ZTST_GEQUAL
#define ZTST_GEQUAL ZTST::GEQUAL
#define ZTST_GREATER ZTST::GREATER
#endif

#ifndef AFAIL_KEEP
#define AFAIL_KEEP AFAIL::KEEP
#define AFAIL_FB_ONLY AFAIL::FB_ONLY
#define AFAIL_ZB_ONLY AFAIL::ZB_ONLY
#define AFAIL_RGB_ONLY AFAIL::RGB_ONLY
#define AFAIL_RGB_ONLY_DSB AFAIL::RGB_ONLY_DSB
#define AFAIL_RGB_ONLY_SW_Z AFAIL::RGB_ONLY_SW_Z
#endif

#ifndef PS_ATST_NONE
#define PS_ATST_NONE ATST::NONE
#define PS_ATST_LEQUAL ATST::LEQUAL
#define PS_ATST_GEQUAL ATST::GEQUAL
#define PS_ATST_EQUAL ATST::EQUAL
#define PS_ATST_NOTEQUAL ATST::NOTEQUAL
#endif

#ifndef PS_AA1_NONE
#define PS_AA1_NONE AA1::NONE
#define PS_AA1_LINE AA1::LINE
#define PS_AA1_TRIANGLE AA1::TRIANGLE
#define PS_AA1_TRIANGLE_SW_Z AA1::TRIANGLE_SW_Z
#endif

#ifndef PS_ROV_DEPTH_NONE
#define PS_ROV_DEPTH_NONE ROV_DEPTH::NONE
#define PS_ROV_DEPTH_READ_WRITE ROV_DEPTH::READ_WRITE
#define PS_ROV_DEPTH_READ_ONLY ROV_DEPTH::READ_ONLY
#endif

/// End helper macros for shared shader code

constant uint FMT_32 = 0;
constant uint FMT_24 = 1;
constant uint FMT_16 = 2;

constant uint SHUFFLE_READ = 1;
[[maybe_unused]] constant uint SHUFFLE_WRITE = 2;
constant uint SHUFFLE_READWRITE = 3;

constant bool HAS_FBFETCH           [[function_constant(GSMTLConstantIndex_FRAMEBUFFER_FETCH)]];
constant bool DEPTH_FEEDBACK        [[function_constant(GSMTLConstantIndex_DEPTH_FEEDBACK)]];
constant bool ROV_NEEDS_R32         [[function_constant(GSMTLConstantIndex_ROV_NEEDS_R32)]];
constant bool BROKEN_SHADER_DEPTH   [[function_constant(GSMTLConstantIndex_BROKEN_SHADER_DEPTH)]];
constant bool FST                   [[function_constant(GSMTLConstantIndex_FST)]];
constant bool IIP                   [[function_constant(GSMTLConstantIndex_IIP)]];
constant bool VS_POINT_SIZE         [[function_constant(GSMTLConstantIndex_VS_POINT_SIZE)]];
constant uint VS_EXPAND_TYPE_RAW    [[function_constant(GSMTLConstantIndex_VS_EXPAND_TYPE)]];
constant uint PS_AEM_FMT            [[function_constant(GSMTLConstantIndex_PS_AEM_FMT)]];
constant uint PS_PAL_FMT            [[function_constant(GSMTLConstantIndex_PS_PAL_FMT)]];
constant uint PS_DST_FMT            [[function_constant(GSMTLConstantIndex_PS_DST_FMT)]];
constant uint PS_DEPTH_FMT          [[function_constant(GSMTLConstantIndex_PS_DEPTH_FMT)]];
constant bool PS_AEM                [[function_constant(GSMTLConstantIndex_PS_AEM)]];
constant bool PS_FBA                [[function_constant(GSMTLConstantIndex_PS_FBA)]];
constant bool PS_FOG                [[function_constant(GSMTLConstantIndex_PS_FOG)]];
constant uint PS_DATE               [[function_constant(GSMTLConstantIndex_PS_DATE)]];
constant uint PS_ATST_RAW           [[function_constant(GSMTLConstantIndex_PS_ATST)]];
constant uint PS_AFAIL_RAW          [[function_constant(GSMTLConstantIndex_PS_AFAIL)]];
constant uint PS_ZTST_RAW           [[function_constant(GSMTLConstantIndex_PS_ZTST)]];
constant uint PS_TFX                [[function_constant(GSMTLConstantIndex_PS_TFX)]];
constant bool PS_TCC                [[function_constant(GSMTLConstantIndex_PS_TCC)]];
constant uint PS_WMS                [[function_constant(GSMTLConstantIndex_PS_WMS)]];
constant uint PS_WMT                [[function_constant(GSMTLConstantIndex_PS_WMT)]];
constant bool PS_ADJS               [[function_constant(GSMTLConstantIndex_PS_ADJS)]];
constant bool PS_ADJT               [[function_constant(GSMTLConstantIndex_PS_ADJT)]];
constant bool PS_LTF                [[function_constant(GSMTLConstantIndex_PS_LTF)]];
constant bool PS_SHUFFLE            [[function_constant(GSMTLConstantIndex_PS_SHUFFLE)]];
constant bool PS_SHUFFLE_SAME       [[function_constant(GSMTLConstantIndex_PS_SHUFFLE_SAME)]];
constant uint PS_PROCESS_BA         [[function_constant(GSMTLConstantIndex_PS_PROCESS_BA)]];
constant uint PS_PROCESS_RG         [[function_constant(GSMTLConstantIndex_PS_PROCESS_RG)]];
constant bool PS_SHUFFLE_ACROSS     [[function_constant(GSMTLConstantIndex_PS_SHUFFLE_ACROSS)]];
constant bool PS_READ16_SRC         [[function_constant(GSMTLConstantIndex_PS_READ16_SRC)]];
constant bool PS_WRITE_RG           [[function_constant(GSMTLConstantIndex_PS_WRITE_RG)]];
constant bool PS_FBMASK             [[function_constant(GSMTLConstantIndex_PS_FBMASK)]];
constant uint PS_BLEND_A            [[function_constant(GSMTLConstantIndex_PS_BLEND_A)]];
constant uint PS_BLEND_B            [[function_constant(GSMTLConstantIndex_PS_BLEND_B)]];
constant uint PS_BLEND_C            [[function_constant(GSMTLConstantIndex_PS_BLEND_C)]];
constant uint PS_BLEND_D            [[function_constant(GSMTLConstantIndex_PS_BLEND_D)]];
constant uint PS_BLEND_HW           [[function_constant(GSMTLConstantIndex_PS_BLEND_HW)]];
constant bool PS_A_MASKED           [[function_constant(GSMTLConstantIndex_PS_A_MASKED)]];
constant bool PS_COLCLIP_HW         [[function_constant(GSMTLConstantIndex_PS_COLCLIP_HW)]];
constant bool PS_RTA_CORRECTION     [[function_constant(GSMTLConstantIndex_PS_RTA_CORRECTION)]];
constant bool PS_RTA_SRC_CORRECTION [[function_constant(GSMTLConstantIndex_PS_RTA_SRC_CORRECTION)]];
constant bool PS_COLCLIP            [[function_constant(GSMTLConstantIndex_PS_COLCLIP)]];
constant uint PS_BLEND_MIX          [[function_constant(GSMTLConstantIndex_PS_BLEND_MIX)]];
constant bool PS_ROUND_INV          [[function_constant(GSMTLConstantIndex_PS_ROUND_INV)]];
constant bool PS_FIXED_ONE_A        [[function_constant(GSMTLConstantIndex_PS_FIXED_ONE_A)]];
constant bool PS_PABE               [[function_constant(GSMTLConstantIndex_PS_PABE)]];
constant bool PS_NO_COLOR           [[function_constant(GSMTLConstantIndex_PS_NO_COLOR)]];
constant bool PS_NO_COLOR1          [[function_constant(GSMTLConstantIndex_PS_NO_COLOR1)]];
constant uint PS_CHANNEL            [[function_constant(GSMTLConstantIndex_PS_CHANNEL)]];
constant uint PS_DITHER             [[function_constant(GSMTLConstantIndex_PS_DITHER)]];
constant uint PS_DITHER_ADJUST      [[function_constant(GSMTLConstantIndex_PS_DITHER_ADJUST)]];
constant bool PS_ZCLAMP             [[function_constant(GSMTLConstantIndex_PS_ZCLAMP)]];
constant bool PS_ZFLOOR             [[function_constant(GSMTLConstantIndex_PS_ZFLOOR)]];
constant bool PS_TCOFFSETHACK       [[function_constant(GSMTLConstantIndex_PS_TCOFFSETHACK)]];
constant bool PS_URBAN_CHAOS_HLE    [[function_constant(GSMTLConstantIndex_PS_URBAN_CHAOS_HLE)]];
constant bool PS_TALES_OF_ABYSS_HLE [[function_constant(GSMTLConstantIndex_PS_TALES_OF_ABYSS_HLE)]];
constant bool PS_TEX_IS_FB          [[function_constant(GSMTLConstantIndex_PS_TEX_IS_FB)]];
constant bool PS_AUTOMATIC_LOD      [[function_constant(GSMTLConstantIndex_PS_AUTOMATIC_LOD)]];
constant bool PS_MANUAL_LOD         [[function_constant(GSMTLConstantIndex_PS_MANUAL_LOD)]];
constant bool PS_REGION_RECT        [[function_constant(GSMTLConstantIndex_PS_REGION_RECT)]];
constant uint PS_SCANMSK            [[function_constant(GSMTLConstantIndex_PS_SCANMSK)]];
constant uint PS_AA1_RAW            [[function_constant(GSMTLConstantIndex_PS_AA1)]];
constant bool PS_ABE                [[function_constant(GSMTLConstantIndex_PS_ABE)]];
constant uint PS_SW_ANISO           [[function_constant(GSMTLConstantIndex_PS_SW_ANISO)]];
constant bool PS_ROV_COLOR          [[function_constant(GSMTLConstantIndex_PS_ROV_COLOR)]];
constant uint PS_ROV_DEPTH_RAW      [[function_constant(GSMTLConstantIndex_PS_ROV_DEPTH)]];

using GSShader::VSExpand;
using AFAIL = GSShader::PS_AFAIL;
using ATST = GSShader::PS_ATST;
using GSShader::ZTST;
using AA1 = GSShader::PS_AA1;
using ROV_DEPTH = GSShader::PS_ROV_DEPTH;
constant VSExpand VS_EXPAND_TYPE = static_cast<VSExpand>(VS_EXPAND_TYPE_RAW);
constant AFAIL PS_AFAIL = static_cast<AFAIL>(PS_AFAIL_RAW);
constant ATST  PS_ATST  = static_cast<ATST>(PS_ATST_RAW);
constant ZTST  PS_ZTST  = static_cast<ZTST>(PS_ZTST_RAW);
constant AA1   PS_AA1   = static_cast<AA1>(PS_AA1_RAW);
constant ROV_DEPTH PS_ROV_DEPTH = static_cast<ROV_DEPTH>(PS_ROV_DEPTH_RAW);

#if defined(__METAL_MACOS__) && __METAL_VERSION__ >= 220
	#define PRIMID_SUPPORT 1
#else
	#define PRIMID_SUPPORT 0
#endif

#if defined(__METAL_IOS__) || __METAL_VERSION__ >= 230
	#define FBFETCH_SUPPORT 1
#else
	#define FBFETCH_SUPPORT 0
#endif

constant bool PS_PRIM_CHECKING_INIT = PS_DATE == 1 || PS_DATE == 2;
constant bool PS_PRIM_CHECKING_READ = PS_DATE == 3;
#if PRIMID_SUPPORT
constant bool NEEDS_PRIMID = PS_PRIM_CHECKING_INIT || PS_PRIM_CHECKING_READ;
#endif
constant bool PS_TEX_IS_DEPTH = PS_URBAN_CHAOS_HLE || PS_TALES_OF_ABYSS_HLE || PS_DEPTH_FMT == 1 || PS_DEPTH_FMT == 2;
constant bool PS_TEX_IS_COLOR = !PS_TEX_IS_DEPTH;
constant bool PS_HAS_PALETTE = PS_PAL_FMT != 0 || (PS_CHANNEL >= 1 && PS_CHANNEL <= 5);
constant bool NOT_IIP = !IIP;
constant bool SW_BLEND = (PS_BLEND_A != PS_BLEND_B) || PS_BLEND_D;
constant bool SW_AD_TO_HW = (PS_BLEND_C == 1 && PS_A_MASKED);
constant bool NEEDS_RT_FOR_BLEND = (((PS_BLEND_A != PS_BLEND_B) && (PS_BLEND_A == 1 || PS_BLEND_B == 1 || PS_BLEND_C == 1)) || PS_BLEND_D == 1 || SW_AD_TO_HW);
constant bool NEEDS_RT_EARLY = PS_TEX_IS_FB || PS_DATE >= 5;
constant bool NEEDS_RT_FOR_AFAIL = PS_AFAIL == AFAIL::ZB_ONLY || PS_AFAIL == AFAIL::RGB_ONLY || PS_AFAIL == AFAIL::RGB_ONLY_SW_Z;
constant bool NEEDS_RT = NEEDS_RT_FOR_AFAIL || NEEDS_RT_EARLY || (!PS_PRIM_CHECKING_INIT && (PS_FBMASK || NEEDS_RT_FOR_BLEND));
constant bool NEEDS_DEPTH_FOR_AFAIL = PS_AFAIL == AFAIL::FB_ONLY || PS_AFAIL == AFAIL::RGB_ONLY_SW_Z;
constant bool NEEDS_DEPTH_FOR_ZTST  = PS_ZTST == ZTST::GEQUAL || PS_ZTST == ZTST::GREATER;
constant bool NEEDS_DEPTH_FOR_AA1   = PS_AA1 == AA1::TRIANGLE_SW_Z;
constant bool SW_DEPTH = NEEDS_DEPTH_FOR_AFAIL || NEEDS_DEPTH_FOR_ZTST || NEEDS_DEPTH_FOR_AA1;

constant bool PS_OUTPUT_COLOR0 = !PS_NO_COLOR  && !PS_ROV_COLOR;
constant bool PS_OUTPUT_COLOR1 = !PS_NO_COLOR1 && !PS_ROV_COLOR;
constant bool PS_ZOUTPUT = (PS_ZCLAMP || PS_ZFLOOR || SW_DEPTH) && PS_ROV_DEPTH == ROV_DEPTH::NONE;
constant bool PS_ZOUTPUT_LESS = PS_ZOUTPUT && !SW_DEPTH;
constant bool PS_ZOUTPUT_ANY  = PS_ZOUTPUT && SW_DEPTH;
constant bool PS_ZOUTPUT_COLOR = PS_ZOUTPUT_ANY && !DEPTH_FEEDBACK;
constant bool VS_NEEDS_INDEX_BUFFER = VS_EXPAND_TYPE == VSExpand::TriangleAA1;
constant bool VS_COVERAGE = VS_EXPAND_TYPE == VSExpand::LineAA1 || VS_EXPAND_TYPE == VSExpand::TriangleAA1;
constant bool VS_INTERIOR = VS_EXPAND_TYPE == VSExpand::TriangleAA1;
constant bool PS_COVERAGE = PS_AA1 != AA1::NONE;
constant bool PS_INTERIOR = PS_AA1 == AA1::TRIANGLE_SW_Z;

// Need to define these for shared code.
constant bool VS_FST = FST;
constant bool PS_FST = FST;
constant bool VS_IIP = IIP;
constant bool PS_IIP = IIP;
constant bool PS_POINT_SAMPLER = false;
constant bool FALSE = false;

#include "../../../../bin/resources/shaders/common/tfx_defs.inc"

struct MainVSIn
{
	float2 st [[attribute(GSMTLAttributeIndexST)]];
	float4 c  [[attribute(GSMTLAttributeIndexC)]];
	float  q  [[attribute(GSMTLAttributeIndexQ)]];
	uint2  p  [[attribute(GSMTLAttributeIndexXY)]];
	uint   z  [[attribute(GSMTLAttributeIndexZ)]];
	uint2  uv [[attribute(GSMTLAttributeIndexUV)]];
	float4 f  [[attribute(GSMTLAttributeIndexF)]];

	// Convert VS inputs for shared code.
	VSInputGeneric GetGeneric()
	{
		VSInputGeneric vin_gen;
		vin_gen.st = st;
		vin_gen.c = c;
		vin_gen.q = q;
		vin_gen.p = p;
		vin_gen.z = z;
		vin_gen.uv = uv;
		vin_gen.f = f;
		return vin_gen;
	}
};

struct MainVSOut
{
	float4 p [[position]];
	float4 t;
	float4 ti;
	float4 c [[function_constant(IIP)]];
	float4 fc [[flat, function_constant(NOT_IIP)]];
	float inv_cov [[function_constant(VS_COVERAGE)]];
	uint interior [[function_constant(VS_INTERIOR)]];
	float point_size [[point_size, function_constant(VS_POINT_SIZE)]];

	MainVSOut(VSOutputGeneric vout)
	{
		p = vout.p;
		t = vout.t;
		ti = vout.ti;
		if (IIP)
			c = vout.c;
		else
			fc = vout.c;
		if (VS_COVERAGE)
			inv_cov = vout.inv_cov;
		if (VS_INTERIOR)
			interior = vout.interior;
		if (VS_POINT_SIZE)
			point_size = vout.point_size;
	}
};

// MARK: - Vertex functions

// Convert VS constants for shared code.
VSUniformsGeneric GetVSUniforms(constant GSMTLMainVSUniform& cb [[buffer(GSMTLBufferIndexHWUniforms)]])
{
  VSUniformsGeneric cb_gen;
	#define X(TYPE, NAME) cb_gen.NAME = cb.NAME;
		VS_UNIFORMS(X)
	#undef X
  return cb_gen;
}

static VSInputGeneric load_vertex(device const GSMTLMainVertex* vertices, uint idx)
{
	GSMTLMainVertex base = vertices[idx];
	VSInputGeneric out;
	out.st = base.st;
	out.c = float4(base.rgba);
	out.q = base.q;
	out.p = uint2(base.xy);
	out.z = base.z;
	out.uv = uint2(base.uv);
	out.f = float4(static_cast<float>(base.fog) / 255.f);
	return out;
}

static uint load_index(device const ushort* indices [[buffer(GSMTLBufferIndexHWIndices)]], uint idx)
{
	return indices[idx];
}

// Note: load_vertex() and load_index() must be declared before including common code.
#include "../../../../bin/resources/shaders/common/tfx_vs.inc"

vertex MainVSOut vs_main(MainVSIn v [[stage_in]], constant GSMTLMainVSUniform& cb [[buffer(GSMTLBufferIndexHWUniforms)]])
{
	return MainVSOut(vs_main_impl(v.GetGeneric(), GetVSUniforms(cb)));
}

vertex MainVSOut vs_main_expand(
	uint vid [[vertex_id]],
	device const GSMTLMainVertex* vertices [[buffer(GSMTLBufferIndexHWVertices)]],
	constant GSMTLMainVSUniform& cb [[buffer(GSMTLBufferIndexHWUniforms)]],
	device const ushort* indices [[buffer(GSMTLBufferIndexHWIndices), function_constant(VS_NEEDS_INDEX_BUFFER)]])
{
	return MainVSOut(vs_expand_impl(vid, vertices, GetVSUniforms(cb), indices));
}

// MARK: - Fragment functions

#if FBFETCH_SUPPORT
fragment float4 fbfetch_test(float4 in [[color(0), raster_order_group(0)]])
{
	return in * 2;
}

constant bool NEEDS_RT_TEX = NEEDS_RT && !HAS_FBFETCH && !PS_ROV_COLOR;
constant bool NEEDS_RT_FBF = NEEDS_RT &&  HAS_FBFETCH && !PS_ROV_COLOR;
constant bool NEEDS_DS_FBF = SW_DEPTH &&  HAS_FBFETCH && !DEPTH_FEEDBACK && PS_ROV_DEPTH == ROV_DEPTH::NONE;
#else
constant bool NEEDS_RT_TEX = NEEDS_RT && !PS_ROV_COLOR;
constant bool NEEDS_DS_FBF = false;
constant float ds_fbf = 0;
#endif
constant bool NEEDS_DS_TEX   = SW_DEPTH && !DEPTH_FEEDBACK && !NEEDS_DS_FBF && PS_ROV_DEPTH == ROV_DEPTH::NONE;
constant bool NEEDS_DS_DEPTH = (SW_DEPTH && DEPTH_FEEDBACK || NEEDS_DS_FBF) && PS_ROV_DEPTH == ROV_DEPTH::NONE;
constant bool NEEDS_RT_ROV = PS_ROV_COLOR && !ROV_NEEDS_R32;
constant bool NEEDS_RT_U32 = PS_ROV_COLOR &&  ROV_NEEDS_R32;
constant bool NEEDS_DS_ROV = PS_ROV_DEPTH != ROV_DEPTH::NONE;

struct MainPSIn
{
	float4 p [[position]];
	float4 t;
	float4 ti;
	float4 c [[function_constant(IIP)]];
	float4 fc [[flat, function_constant(NOT_IIP)]];
	float inv_cov [[function_constant(PS_COVERAGE)]];
	uint interior [[function_constant(PS_INTERIOR)]];
};

struct MainPSOut
{
	float4 c0 [[color(0), index(0), function_constant(PS_OUTPUT_COLOR0)]];
	float4 c1 [[color(0), index(1), function_constant(PS_OUTPUT_COLOR1)]];
	float depthColor [[color(1), function_constant(PS_ZOUTPUT_COLOR)]];
	float depthLess [[depth(less), function_constant(PS_ZOUTPUT_LESS)]];
	float depthAny  [[depth(any),  function_constant(PS_ZOUTPUT_ANY)]];
	MainPSOut(PSOutputGeneric res)
	{
		if (PS_OUTPUT_COLOR0)
			c0 = res.c0;
		if (PS_OUTPUT_COLOR1)
			c1 = res.c1;
		if (PS_ZOUTPUT_LESS)
			depthLess = res.depth;
		if (PS_ZOUTPUT_ANY)
			depthAny = res.depth;
		if (PS_ZOUTPUT_COLOR)
			depthColor = res.depth;
	}
};

struct PSMainState
{
	texture2d<float> ps_tex;
	depth2d<float> ps_tex_depth;
	texture2d<float> ps_palette;
	texture2d<float> ps_prim_id_tex;
	sampler ps_tex_sampler;
	float4 ps_current_color;
	float ps_current_depth;
	uint ps_prim_id;
	bool ps_color_discarded = false;
	bool ps_depth_discarded = false;
	const thread MainPSIn& ps_in;
	constant GSMTLMainPSUniform& ps_cb;

	PSMainState(const thread MainPSIn& ps_in, constant GSMTLMainPSUniform& ps_cb): ps_in(ps_in), ps_cb(ps_cb) {}

	#include "../../../../bin/resources/shaders/common/tfx_ps_header.inc"
	#include "../../../../bin/resources/shaders/common/tfx_ps_util.inc"
	#include "../../../../bin/resources/shaders/common/tfx_ps_sample_af.inc"
	#include "../../../../bin/resources/shaders/common/tfx_ps_fetch.inc"
	#include "../../../../bin/resources/shaders/common/tfx_ps_sample.inc"
	#include "../../../../bin/resources/shaders/common/tfx_ps_tfx.inc"
	#include "../../../../bin/resources/shaders/common/tfx_ps_atst.inc"
	#include "../../../../bin/resources/shaders/common/tfx_ps_fog.inc"
	#include "../../../../bin/resources/shaders/common/tfx_ps_color.inc"
	#include "../../../../bin/resources/shaders/common/tfx_ps_post.inc"
	#include "../../../../bin/resources/shaders/common/tfx_ps_blend.inc"
	#include "../../../../bin/resources/shaders/common/tfx_ps_main.inc"
};

fragment MainPSOut ps_main(
	MainPSIn in [[stage_in]],
	constant GSMTLMainPSUniform& cb [[buffer(GSMTLBufferIndexHWUniforms)]],
	sampler s [[sampler(0)]],
#if PRIMID_SUPPORT
	uint primid [[primitive_id, function_constant(NEEDS_PRIMID)]],
#endif
#if FBFETCH_SUPPORT
	float4 rt_fbf [[color(0), raster_order_group(0), function_constant(NEEDS_RT_FBF)]],
	float  ds_fbf [[color(1), raster_order_group(1), function_constant(NEEDS_DS_FBF)]],
#endif
	texture2d<float> tex       [[texture(GSMTLTextureIndexTex),          function_constant(PS_TEX_IS_COLOR)]],
	depth2d<float>   depth     [[texture(GSMTLTextureIndexTex),          function_constant(PS_TEX_IS_DEPTH)]],
	texture2d<float> palette   [[texture(GSMTLTextureIndexPalette),      function_constant(PS_HAS_PALETTE)]],
	texture2d<float> rt        [[texture(GSMTLTextureIndexRenderTarget), function_constant(NEEDS_RT_TEX)]],
	texture2d<float> primidtex [[texture(GSMTLTextureIndexPrimIDs),      function_constant(PS_PRIM_CHECKING_READ)]],
	texture2d<float> ds_tex    [[texture(GSMTLTextureIndexDepthTarget),  function_constant(NEEDS_DS_TEX)]],
	depth2d<float>   ds_depth  [[texture(GSMTLTextureIndexDepthTarget),  function_constant(NEEDS_DS_DEPTH)]],
	texture2d<float, access::read_write> rt_rov [[texture(GSMTLTextureIndexRenderTarget), raster_order_group(0), function_constant(NEEDS_RT_ROV)]],
	texture2d<uint,  access::read_write> rt_u32 [[texture(GSMTLTextureIndexRenderTarget), raster_order_group(0), function_constant(NEEDS_RT_U32)]],
	texture2d<float, access::read_write> ds_rov [[texture(GSMTLTextureIndexDepthTarget),  raster_order_group(1), function_constant(NEEDS_DS_ROV)]])
{
	PSMainState state(in, cb);

	state.tex_sampler = s;
	if (PS_TEX_IS_COLOR)
		state.tex = tex;
	else
		state.tex_depth = depth;
	if (PS_HAS_PALETTE)
		state.palette = palette;
	if (PS_PRIM_CHECKING_READ)
		state.prim_id_tex = primidtex;
#if PRIMID_SUPPORT
	if (NEEDS_PRIMID)
		state.prim_id = primid;
#endif

	uint2 coord = uint2(in.p.xy);

	if (SW_DEPTH)
	{
		if (PS_ROV_DEPTH != ROV_DEPTH::NONE)
			state.current_depth = ds_rov.read(coord).x;
		else if (DEPTH_FEEDBACK)
			state.current_depth = ds_depth.read(coord);
		else if (NEEDS_DS_FBF)
			state.current_depth = ds_fbf < 0 ? ds_depth.read(coord) : ds_fbf;
		else
			state.current_depth = ds_tex.read(coord).x;
	}

	if (NEEDS_RT || (PS_ROV_COLOR && any(cb.fbmask == 0xff)))
	{
		if (PS_ROV_COLOR)
		{
			if (ROV_NEEDS_R32)
				state.current_color = unpack_unorm4x8_to_float(rt_u32.read(coord).x);
			else
				state.current_color = rt_rov.read(coord);
		}
		else
		{
#if FBFETCH_SUPPORT
			state.current_color = HAS_FBFETCH ? rt_fbf : rt.read(coord);
#else
			state.current_color = rt.read(coord);
#endif
		}
	}
	else
	{
		state.current_color = 0;
	}

	PSOutputGeneric out = state.ps_main_impl();

	if (PS_ROV_DEPTH == ROV_DEPTH::READ_WRITE && !state.depth_discarded)
		ds_rov.write(out.depth, coord);
	if (PS_ROV_COLOR && !state.color_discarded)
	{
		if (!PS_FBMASK)
			out.c0 = select(out.c0, state.current_color, cb.fbmask == 0xff);
		if (ROV_NEEDS_R32)
			rt_u32.write(pack_float_to_unorm4x8(out.c0), coord);
		else
			rt_rov.write(out.c0, coord);
	}
	return MainPSOut(out);
}

// Metal doesn't let you toggle eft with function constants so we need a separate function for it
[[early_fragment_tests]]
fragment void ps_main_rov_eft(
	MainPSIn in [[stage_in]],
	constant GSMTLMainPSUniform& cb [[buffer(GSMTLBufferIndexHWUniforms)]],
	sampler s [[sampler(0)]],
	texture2d<float> tex     [[texture(GSMTLTextureIndexTex),     function_constant(PS_TEX_IS_COLOR)]],
	depth2d<float>   depth   [[texture(GSMTLTextureIndexTex),     function_constant(PS_TEX_IS_DEPTH)]],
	texture2d<float> palette [[texture(GSMTLTextureIndexPalette), function_constant(PS_HAS_PALETTE)]],
	texture2d<float, access::read_write> rt_rov [[texture(GSMTLTextureIndexRenderTarget), raster_order_group(0), function_constant(NEEDS_RT_ROV)]],
	texture2d<uint,  access::read_write> rt_u32 [[texture(GSMTLTextureIndexRenderTarget), raster_order_group(0), function_constant(NEEDS_RT_U32)]])
{
	PSMainState state(in, cb);
	state.tex_sampler = s;
	if (PS_TEX_IS_COLOR)
		state.tex = tex;
	else
		state.tex_depth = depth;
	if (PS_HAS_PALETTE)
		state.palette = palette;

	uint2 coord = uint2(in.p.xy);
	if (ROV_NEEDS_R32)
		state.current_color = unpack_unorm4x8_to_float(rt_u32.read(coord).x);
	else
		state.current_color = rt_rov.read(coord);
	
	PSOutputGeneric out = state.ps_main_impl();

	if (!state.color_discarded)
	{
		if (!PS_FBMASK)
			out.c0 = select(out.c0, state.current_color, cb.fbmask == 0xff);
		if (ROV_NEEDS_R32)
			rt_u32.write(pack_float_to_unorm4x8(out.c0), coord);
		else
			rt_rov.write(out.c0, coord);
	}
}

#if PRIMID_SUPPORT
fragment uint primid_test(uint id [[primitive_id]])
{
	return id;
}
#endif

// MARK: Markers for detecting the Metal version a metallib was compiled against

#if __METAL_VERSION__ >= 210
kernel void metal_version_21() {}
#endif
#if __METAL_VERSION__ >= 220
kernel void metal_version_22() {}
#endif
#if __METAL_VERSION__ >= 230
kernel void metal_version_23() {}
#endif
