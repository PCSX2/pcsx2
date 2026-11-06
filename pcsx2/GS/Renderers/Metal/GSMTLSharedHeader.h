// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include <simd/simd.h>
#include "../Common/GSShaderEnums.h"

enum GSMTLBufferIndices
{
	GSMTLBufferIndexVertices,
	GSMTLBufferIndexUniforms,
	GSMTLBufferIndexHWVertices,
	GSMTLBufferIndexHWUniforms,
	GSMTLBufferIndexHWIndices,
};

enum GSMTLTextureIndex
{
	GSMTLTextureIndexNonHW,
	GSMTLTextureIndexTex,
	GSMTLTextureIndexPalette,
	GSMTLTextureIndexRenderTarget,
	GSMTLTextureIndexPrimIDs,
	GSMTLTextureIndexDepthTarget,
	GSMTLTextureIndexCount,
};

struct GSMTLConvertPSUniform
{
	int emoda;
	int emodc;
};

struct GSMTLPresentPSUniform
{
	vector_float4 source_rect;
	vector_float4 target_rect;
	vector_float2 source_size;
	vector_float2 target_size;
	vector_float2 target_resolution;
	vector_float2 rcp_target_resolution; ///< 1 / target_resolution
	vector_float2 source_resolution;
	vector_float2 rcp_source_resolution; ///< 1 / source_resolution
	float time;
};

struct GSMTLInterlacePSUniform
{
	vector_float4 ZrH;
};

struct GSMTLCASPSUniform
{
	vector_uint4 const0;
	vector_uint4 const1;
	vector_int2 srcOffset;
};

struct GSMTLCLUTConvertPSUniform
{
	float scale;
	vector_uint2 offset;
	uint doffset;
};

struct GSMTLIndexedConvertPSUniform
{
	float scale;
	uint sbw;
	uint dbw;
	uint psm;
};

struct GSMTLDownsamplePSUniform
{
	vector_uint2 clamp_min;
	uint downsample_factor;
	float weight;
	float step_multiplier;
};

struct GSMTLMainVertex
{
	vector_float2 st;
	vector_uchar4 rgba;
	float q;
	vector_ushort2 xy;
	uint z;
	vector_ushort2 uv;
	unsigned char fog;
};

#ifdef __METAL_VERSION__
	#define PCSX2_MSL
#else
	#define PCSX2_CPP
#endif

namespace GSUniforms
{
#define VERTEX_SHADER
#define PIXEL_SHADER
	typedef vector_uint2 uint2;
	typedef vector_uint4 uint4;
	typedef vector_float2 float2;
	typedef vector_float4 float4;
	typedef matrix_float4x4 float4x4;
	// To better match glsl/hlsl, use packed types for xxx3
#ifdef PCSX2_MSL
	typedef packed_uint3 uint3;
	typedef packed_float3 float3;
#else
	typedef uint uint3[3];
	typedef float float3[3];
#endif
	static_assert(sizeof(uint3) == 12, "Size Check");
	static_assert(sizeof(float3) == 12, "Size Check");
	#include "../../../../bin/resources/shaders/common/tfx_uniforms.inc"
#undef VERTEX_SHADER
#undef PIXEL_SHADER
} // namespace GSUniforms

typedef GSUniforms::VSUniform GSMTLMainVSUniform;
typedef GSUniforms::PSUniform GSMTLMainPSUniform;

enum GSMTLAttributes
{
	GSMTLAttributeIndexST,
	GSMTLAttributeIndexC,
	GSMTLAttributeIndexQ,
	GSMTLAttributeIndexXY,
	GSMTLAttributeIndexZ,
	GSMTLAttributeIndexUV,
	GSMTLAttributeIndexF,
};

enum GSMTLFnConstants
{
	GSMTLConstantIndex_BILN,
	GSMTLConstantIndex_DEPTH_OUT,
	GSMTLConstantIndex_CAS_SHARPEN_ONLY,
	GSMTLConstantIndex_FRAMEBUFFER_FETCH,
	GSMTLConstantIndex_DEPTH_FEEDBACK,
	GSMTLConstantIndex_ROV_NEEDS_R32,
	GSMTLConstantIndex_BROKEN_SHADER_DEPTH,
	GSMTLConstantIndex_FST,
	GSMTLConstantIndex_IIP,
	GSMTLConstantIndex_VS_POINT_SIZE,
	GSMTLConstantIndex_VS_EXPAND_TYPE,
	GSMTLConstantIndex_PS_AEM_FMT,
	GSMTLConstantIndex_PS_PAL_FMT,
	GSMTLConstantIndex_PS_DST_FMT,
	GSMTLConstantIndex_PS_DEPTH_FMT,
	GSMTLConstantIndex_PS_AEM,
	GSMTLConstantIndex_PS_FBA,
	GSMTLConstantIndex_PS_FOG,
	GSMTLConstantIndex_PS_DATE,
	GSMTLConstantIndex_PS_ATST,
	GSMTLConstantIndex_PS_AFAIL,
	GSMTLConstantIndex_PS_ZTST,
	GSMTLConstantIndex_PS_TFX,
	GSMTLConstantIndex_PS_TCC,
	GSMTLConstantIndex_PS_WMS,
	GSMTLConstantIndex_PS_WMT,
	GSMTLConstantIndex_PS_ADJS,
	GSMTLConstantIndex_PS_ADJT,
	GSMTLConstantIndex_PS_LTF,
	GSMTLConstantIndex_PS_SHUFFLE,
	GSMTLConstantIndex_PS_SHUFFLE_SAME,
	GSMTLConstantIndex_PS_PROCESS_BA,
	GSMTLConstantIndex_PS_PROCESS_RG,
	GSMTLConstantIndex_PS_SHUFFLE_ACROSS,
	GSMTLConstantIndex_PS_READ16_SRC,
	GSMTLConstantIndex_PS_WRITE_RG,
	GSMTLConstantIndex_PS_FBMASK,
	GSMTLConstantIndex_PS_BLEND_A,
	GSMTLConstantIndex_PS_BLEND_B,
	GSMTLConstantIndex_PS_BLEND_C,
	GSMTLConstantIndex_PS_BLEND_D,
	GSMTLConstantIndex_PS_BLEND_HW,
	GSMTLConstantIndex_PS_A_MASKED,
	GSMTLConstantIndex_PS_COLCLIP_HW,
	GSMTLConstantIndex_PS_RTA_CORRECTION,
	GSMTLConstantIndex_PS_RTA_SRC_CORRECTION,
	GSMTLConstantIndex_PS_COLCLIP,
	GSMTLConstantIndex_PS_BLEND_MIX,
	GSMTLConstantIndex_PS_ROUND_INV,
	GSMTLConstantIndex_PS_FIXED_ONE_A,
	GSMTLConstantIndex_PS_PABE,
	GSMTLConstantIndex_PS_NO_COLOR,
	GSMTLConstantIndex_PS_NO_COLOR1,
	GSMTLConstantIndex_PS_CHANNEL,
	GSMTLConstantIndex_PS_DITHER,
	GSMTLConstantIndex_PS_DITHER_ADJUST,
	GSMTLConstantIndex_PS_ZCLAMP,
	GSMTLConstantIndex_PS_ZFLOOR,
	GSMTLConstantIndex_PS_TCOFFSETHACK,
	GSMTLConstantIndex_PS_URBAN_CHAOS_HLE,
	GSMTLConstantIndex_PS_TALES_OF_ABYSS_HLE,
	GSMTLConstantIndex_PS_TEX_IS_FB,
	GSMTLConstantIndex_PS_AUTOMATIC_LOD,
	GSMTLConstantIndex_PS_MANUAL_LOD,
	GSMTLConstantIndex_PS_REGION_RECT,
	GSMTLConstantIndex_PS_SCANMSK,
	GSMTLConstantIndex_PS_AA1,
	GSMTLConstantIndex_PS_ABE,
	GSMTLConstantIndex_PS_SW_ANISO,
	GSMTLConstantIndex_PS_ROV_COLOR,
	GSMTLConstantIndex_PS_ROV_DEPTH,
};
