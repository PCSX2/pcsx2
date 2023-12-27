// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include <simd/simd.h>

enum GSMTLBufferIndices
{
	GSMTLBufferIndexVertices,
	GSMTLBufferIndexUniforms,
	GSMTLBufferIndexHWVertices,
	GSMTLBufferIndexHWUniforms,
};

enum GSMTLTextureIndex
{
	GSMTLTextureIndexNonHW,
	GSMTLTextureIndexTex,
	GSMTLTextureIndexPalette,
	GSMTLTextureIndexRenderTarget,
	GSMTLTextureIndexPrimIDs,
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

struct GSMTLMainVSUniform
{
	vector_float2 vertex_scale;
	vector_float2 vertex_offset;
	vector_float2 texture_scale;
	vector_float2 texture_offset;
	vector_float2 point_size;
	uint max_depth;
};

struct GSMTLMainPSUniform
{
	union
	{
		vector_float4 fog_color_aref;
		vector_float3 fog_color;
		struct
		{
			float pad0[3];
			float aref;
		};
	};
	vector_float4 wh; ///< xy => PS2, zw => actual (upscaled)
	vector_float2 ta;
	float max_depth;
	float alpha_fix;
	vector_uint4 fbmask;

	vector_float4 half_texel;
	union
	{
		vector_float4 uv_min_max;
		vector_uint4 uv_msk_fix;
	};
	vector_float4 st_range;
	struct
	{
		unsigned int blue_mask;
		unsigned int blue_shift;
		unsigned int green_mask;
		unsigned int green_shift;
	} channel_shuffle;
	vector_float2 tc_offset;
	vector_float2 st_scale;
	matrix_float4x4 dither_matrix;

	vector_float4 scale_factor;
};

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

enum class GSMTLExpandType : unsigned char
{
	None = 0,
	Point = 1,
	Line = 2,
	Sprite = 3,
};

enum GSMTLFnConstants
{
	GSMTLConstantIndex_CAS_SHARPEN_ONLY,
	GSMTLConstantIndex_FRAMEBUFFER_FETCH,
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
	GSMTLConstantIndex_PS_TFX,
	GSMTLConstantIndex_PS_TCC,
	GSMTLConstantIndex_PS_WMS,
	GSMTLConstantIndex_PS_WMT,
	GSMTLConstantIndex_PS_ADJS,
	GSMTLConstantIndex_PS_ADJT,
	GSMTLConstantIndex_PS_LTF,
	GSMTLConstantIndex_PS_SHUFFLE,
	GSMTLConstantIndex_PS_SHUFFLE_SAME,
	GSMTLConstantIndex_PS_READ_BA,
	GSMTLConstantIndex_PS_READ16_SRC,
	GSMTLConstantIndex_PS_WRITE_RG,
	GSMTLConstantIndex_PS_FBMASK,
	GSMTLConstantIndex_PS_BLEND_A,
	GSMTLConstantIndex_PS_BLEND_B,
	GSMTLConstantIndex_PS_BLEND_C,
	GSMTLConstantIndex_PS_BLEND_D,
	GSMTLConstantIndex_PS_BLEND_HW,
	GSMTLConstantIndex_PS_A_MASKED,
	GSMTLConstantIndex_PS_HDR,
	GSMTLConstantIndex_PS_COLCLIP,
	GSMTLConstantIndex_PS_BLEND_MIX,
	GSMTLConstantIndex_PS_ROUND_INV,
	GSMTLConstantIndex_PS_FIXED_ONE_A,
	GSMTLConstantIndex_PS_PABE,
	GSMTLConstantIndex_PS_NO_COLOR,
	GSMTLConstantIndex_PS_NO_COLOR1,
	GSMTLConstantIndex_PS_ONLY_ALPHA,
	GSMTLConstantIndex_PS_CHANNEL,
	GSMTLConstantIndex_PS_DITHER,
	GSMTLConstantIndex_PS_ZCLAMP,
	GSMTLConstantIndex_PS_TCOFFSETHACK,
	GSMTLConstantIndex_PS_URBAN_CHAOS_HLE,
	GSMTLConstantIndex_PS_TALES_OF_ABYSS_HLE,
	GSMTLConstantIndex_PS_TEX_IS_FB,
	GSMTLConstantIndex_PS_AUTOMATIC_LOD,
	GSMTLConstantIndex_PS_MANUAL_LOD,
	GSMTLConstantIndex_PS_POINT_SAMPLER,
	GSMTLConstantIndex_PS_REGION_RECT,
	GSMTLConstantIndex_PS_SCANMSK,
};
