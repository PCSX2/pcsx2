#ifndef __METAL_VERSION__
	// GLSL is strict about casting integers to bools and all selector macros are integers,
	// so use != FALSE with FALSE = 0 to get around this.
	#define FALSE 0

	// Constants for non-Metal backends (Metal defines them separately).
	#define FMT_32 0
	#define FMT_24 1
	#define FMT_16 2

	#define SHUFFLE_READ  1
	#define SHUFFLE_WRITE 2
	#define SHUFFLE_READWRITE 3

	#ifndef VS_EXPAND_NONE
	#define VS_EXPAND_NONE 0
	#define VS_EXPAND_POINT 1
	#define VS_EXPAND_LINE 2
	#define VS_EXPAND_SPRITE 3
	#define VS_EXPAND_LINE_AA1 4
	#define VS_EXPAND_TRIANGLE_AA1 5
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

	#ifndef PS_ATST_NONE
	#define PS_ATST_NONE 0
	#define PS_ATST_LEQUAL 1
	#define PS_ATST_GEQUAL 2
	#define PS_ATST_EQUAL 3
	#define PS_ATST_NOTEQUAL 4
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
#endif // __METAL_VERSION__

// Raw vertex layout for vertex expand shaders.
struct VSRawVertex
{
	float2 ST;
	uint RGBA;
	float Q;
	uint XY;
	uint Z;
	uint UV;
	uint FOG;
};

// Vertex shader constant buffer fields. Shared by all shaders except Metal.
#define VS_UNIFORMS(X) \
	X(float2, vertex_scale) \
	X(float2, vertex_offset) \
	X(float2, texture_scale) \
	X(float2, texture_offset) \
	X(float2, point_size) \
	X(uint, max_depth_vs) \
	X(float, line_aa1_width)

// Generic VS constant buffer for passing to shared shader code.
struct VSUniformsGeneric
{
	#define X(TYPE, NAME) TYPE NAME;
		VS_UNIFORMS(X)
	#undef X
};

// Vertex shader push constant fields for non-Metal backends.
#define VS_PUSH_CONSTANTS(X) \
	X(uint, BaseVertex) \
	X(uint, BaseIndex) \
	X(uint, pad_cb2_0) \
	X(uint, pad_cb2_1)

// Generic VS inputs for passing to shared shader code.
struct VSInputGeneric
{
	float2 st;
	float4 c;
	float  q;
	uint2  p;
	uint   z;
	uint2  uv;
	float4 f;
};

// Generic VS outputs for shared shader code to pass values.
struct VSOutputGeneric
{
	float4 p;
	float4 t;
	float4 ti;
	float4 c;
	float inv_cov;
	uint interior;
	float point_size;
};

// Pixel shader constant buffer fields.
// Shared by all shaders except Metal, which defines its own version.
#define PS_UNIFORMS(X) \
	X(float3, fog_color) \
	X(float, aref) \
	X(float4, wh)  \
	X(float2, ta) \
	X(float, max_depth_ps) \
	X(float, alpha_fix) \
	X(uint4, fbmask) \
	X(float4, half_texel) \
	X(float4, uv_min_max) \
	X(float4, lod_params) \
	X(float4, st_range) \
	X(uint, channel_shuffle_blue_mask) \
	X(uint, channel_shuffle_blue_shift) \
	X(uint, channel_shuffle_green_mask) \
	X(uint, channel_shuffle_green_shift) \
	X(float2, channel_shuffle_offset) \
	X(float2, tc_offset) \
	X(float2, st_scale) \
	X(float4x4, dither_matrix) \
	X(float4, scale_factor) \
	X(float, line_cov_scale) \
	X(float, _pad0) \
	X(float, _pad1) \
	X(float, _pad2)

// Generic PS constant buffer for passing to shared shader code.
struct PSUniformsGeneric
{
	#define X(TYPE, NAME) TYPE NAME;
		PS_UNIFORMS(X)
	#undef X
};

// Generic PS inputs for passing to shared shader code.
struct PSInputGeneric
{
	float4 p;
	float4 t;
	float4 ti;
	float4 c;
	float4 fc;
	float inv_cov;
	uint interior;
};

// Generic PS outputs for shared shader code to pass values.
struct PSOutputGeneric
{
	float4 c0;
	float4 c1;
	float depth;
};

// Metal defines it's own version of these conversion functions.
#ifndef __METAL_VERSION__
static float4 convert_depth32_rgba8(float value)
{
	uint val = uint(value * EXP2_POS_32);
	return float4(UINT_BITCAST_UCHAR4(val));
}

static float4 convert_depth16_rgba8(float value)
{
	uint val = uint(value * EXP2_POS_32);
	return float4(uint4(val << 3, val >> 2, val >> 7, val >> 8) & uint4(0xf8, 0xf8, 0xf8, 0x80));
}
#endif

static float2 float2_bcast(float val)
{
	return float2(val, val);
}

static float3 float3_bcast(float val)
{
	return float3(val, val, val);
}

static float4 float4_bcast(float val)
{
	return float4(val, val, val, val);
}

static uint4 uint4_bcast(uint val)
{
	return uint4(val, val, val, val);
}

static uint2 uint2_bcast(uint val)
{
	return uint2(val, val);
}

static int3 int3_bcast(int val)
{
	return int3(val, val, val);
}

static bool any_nonzero(float3 val)
{
	return any(notEqual(val, float3_bcast(0.0f)));
}

static bool any_nonzero(int3 val)
{
	return any(notEqual(val, int3_bcast(0)));
}

#ifndef __METAL_VERSION__
	// This is to make sure the selector macros are all defined if we're compiling offline for testing.
	#ifndef VS_TME
		#define VS_TME 0
		#define VS_FST 0
		#define VS_IIP 0
		#define VS_POINT_SIZE 0
		#define VS_EXPAND_TYPE 0
	#endif
	#ifndef PS_FST
		#define PS_FST 0
		#define PS_WMS 0
		#define PS_WMT 0
		#define PS_ADJS 0
		#define PS_ADJT 0
		#define PS_FMT FMT_32
		#define PS_AEM 0
		#define PS_TFX 0
		#define PS_TCC 1
		#define PS_ATST 1
		#define PS_AFAIL 0
		#define PS_FOG 0
		#define PS_BLEND_HW 0
		#define PS_A_MASKED 0
		#define PS_FBA 0
		#define PS_FBMASK 0
		#define PS_LTF 1
		#define PS_TCOFFSETHACK 0
		#define PS_SHUFFLE 0
		#define PS_SHUFFLE_SAME 0
		#define PS_PROCESS_BA 0
		#define PS_PROCESS_RG 0
		#define PS_SHUFFLE_ACROSS 0
		#define PS_WRITE_RG 0
		#define PS_READ16_SRC 0
		#define PS_DST_FMT 0
		#define PS_DEPTH_FMT 0
		#define PS_PAL_FMT 0
		#define PS_CHANNEL 0
		#define PS_TALES_OF_ABYSS_HLE 0
		#define PS_URBAN_CHAOS_HLE 0
		#define PS_COLCLIP_HW 0
		#define PS_COLCLIP 0
		#define PS_BLEND_A 0
		#define PS_BLEND_B 0
		#define PS_BLEND_C 0
		#define PS_BLEND_D 0
		#define PS_FIXED_ONE_A 0
		#define PS_PABE 0
		#define PS_DITHER 0
		#define PS_DITHER_ADJUST 0
		#define PS_ZCLAMP 0
		#define PS_ZFLOOR 0
		#define PS_SCANMSK 0
		#define PS_AUTOMATIC_LOD 0
		#define PS_MANUAL_LOD 0
		#define PS_TEX_IS_FB 0
		#define PS_NO_COLOR 0
		#define PS_NO_COLOR1 0
		#define PS_DATE 0
		#define PS_ROV_COLOR 0
		#define PS_ROV_DEPTH 0
		#define PS_SW_ANISO 0
		#define PS_REGION_RECT 0
		#define PS_RTA_SRC_CORRECTION 0
		#define PS_AEM_FMT 0
		#define PS_IIP 0
		#define PS_ROUND_INV 0
		#define PS_BLEND_MIX 0
		#define PS_RTA_CORRECTION 0
		#define PS_ZTST 0
		#define PS_AA1 0
		#define PS_ABE 0
	#endif

	// Pixel shader definitions derived from the selectors macros.
	// Metal defines it's own version of these.

	#define PS_PRIM_CHECKING_INIT (PS_DATE == 1 || PS_DATE == 2)
	#define PS_TEX_IS_DEPTH (PS_DEPTH_FMT == 1 || PS_DEPTH_FMT == 2)

	#define SW_BLEND (PS_BLEND_A != 0 || PS_BLEND_B != 0 || PS_BLEND_D != 0)
	#define SW_BLEND_NEEDS_RT (SW_BLEND && (PS_BLEND_A == 1 || PS_BLEND_B == 1 || PS_BLEND_C == 1 || PS_BLEND_D == 1))
	#define SW_AD_TO_HW (PS_BLEND_C == 1 && PS_A_MASKED != FALSE)
	#define NEEDS_RT_FOR_BLEND (((PS_BLEND_A != PS_BLEND_B) && (PS_BLEND_A == 1 || PS_BLEND_B == 1 || PS_BLEND_C == 1)) || PS_BLEND_D == 1 || SW_AD_TO_HW)
	#define NEEDS_RT_EARLY (PS_TEX_IS_FB != FALSE || PS_DATE >= 5)
	#define NEEDS_RT_FOR_AFAIL (PS_AFAIL == AFAIL_ZB_ONLY || PS_AFAIL == AFAIL_RGB_ONLY || PS_AFAIL == AFAIL_RGB_ONLY_SW_Z)
	#define NEEDS_RT (NEEDS_RT_FOR_AFAIL || NEEDS_RT_EARLY || (!PS_PRIM_CHECKING_INIT && (PS_FBMASK != 0 || NEEDS_RT_FOR_BLEND)))
	
	#define NEEDS_DEPTH_FOR_AFAIL (PS_AFAIL == AFAIL_FB_ONLY || PS_AFAIL == AFAIL_RGB_ONLY_SW_Z)
	#define NEEDS_DEPTH_FOR_ZTST (PS_ZTST == ZTST_GEQUAL || PS_ZTST == ZTST_GREATER)
	#define NEEDS_DEPTH_FOR_AA1 (PS_AA1 == PS_AA1_TRIANGLE_SW_Z)
	#define SW_DEPTH (NEEDS_DEPTH_FOR_AFAIL || NEEDS_DEPTH_FOR_ZTST || NEEDS_DEPTH_FOR_AA1)
	#define ZWRITE (PS_ZFLOOR != FALSE || PS_ZCLAMP != FALSE || SW_DEPTH)
	#define NEED_PRIMID (PS_DATE >= 1 && PS_DATE <= 3)

	#define PS_RETURN_COLOR_ROV (!PS_NO_COLOR && PS_ROV_COLOR)
	#define PS_RETURN_COLOR (!PS_NO_COLOR && !PS_ROV_COLOR)
	#define PS_RETURN_DEPTH_ROV (PS_ROV_DEPTH == PS_ROV_DEPTH_READ_WRITE)
	#define PS_RETURN_DEPTH (ZWRITE && !PS_ROV_DEPTH)
	#define PS_ROV_EARLYDEPTHSTENCIL (PS_ROV_COLOR && !PS_ROV_DEPTH && !ZWRITE)
#endif
