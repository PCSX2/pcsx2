// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#ifndef PCSX2_VULKAN
	#define PCSX2_VULKAN 0
#endif

#ifndef PCSX2_OPENGL
	#define PCSX2_OPENGL 0
#endif

#if PCSX2_VULKAN == PCSX2_OPENGL
	ERROR: Exactly one of PCSX2_VULKAN or PCSX2_OPENGL should be true.
#endif

/// Start helper macros for shared shader code

// Types
#define FLOAT2 vec2
#define FLOAT3 vec3
#define FLOAT4 vec4
#define FLOAT2x2 mat2x2
#define FLOAT2x4 mat2x4
#define FLOAT4x4 mat4x4
#define UINT2 uvec2
#define UINT3 uvec3
#define UINT4 uvec4
#define INT2 ivec2
#define INT3 ivec3
#define INT4 ivec4
#define USHORT uint
#define USHORT2 uvec2
#define USHORT3 uvec3
#define USHORT4 uvec4
#define SHORT int
#define SHORT2 ivec2
#define SHORT3 ivec3
#define SHORT4 ivec4
#define BOOL2 bvec2
#define BOOL3 bvec3
#define BOOL4 bvec4

// Builtin keywords/functions
#define STATIC
#define DFDX dFdx
#define DFDY dFdy
#define SELECT(COND, TRUE_VAL, FALSE_VAL) mix((COND), (FALSE_VAL), (TRUE_VAL))
#define VEQUAL(X, Y) equal((X), (Y))
#define VGEQUAL(X, Y) greaterThanEqual((X), (Y))
#define VLEQUAL(X, Y) lessThanEqual((X), (Y))
#define VGREATER(X, Y) greaterThan((X), (Y))
#define VLESS(X, Y) lessThan((X), (Y))
#define VNOTEQUAL(X, Y) notEqual((X), (Y))
#define RSQRT(X) inversesqrt(X)
#define GPU_DISCARD discard
#define SATURATE(X) clamp((X), 0.0f, 1.0f)
#define FLOAT_BITCAST_UINT(X) floatBitsToUint(X)
#define FLOAT4_BITCAST_UINT4(X) floatBitsToUint(X)
#define UINT_BITCAST_UCHAR4(X) UINT4((X) & 0xFFu, ((X) >> 8) & 0xFFu, ((X) >> 16) & 0xFFu, ((X) >> 24) & 0xFFu)
#define MAT_MUL(X, Y) ((X) * (Y))
#define MAT_GET(MAT, X, Y) MAT[Y][X]
#define FRACT(X) fract(X)
#define MIX mix
#define IN_PARAM(TYPE, NAME) TYPE NAME
#define IN_OUT_PARAM(TYPE, NAME) inout TYPE NAME
#define IS_NAN_OR_INF_4(X) BOOL4(INT4(isinf(X)) | INT4(isnan(X)))
#define UNROLL

// Constants
#define PRIMID_MAX 0x7FFFFFFF
#define VS_Y_FLIP 1.0f
#define EXP2_NEG_32 exp2(-32.0f)
#define EXP2_POS_32 exp2(32.0f)

// Vertex shader helpers
#if PCSX2_VULKAN
	#define VS_SCALE_RAW_Z(Z) (float(Z) * EXP2_NEG_32)
#elif PCSX2_OPENGL
	#define VS_SCALE_RAW_Z(Z) ((HAS_CLIP_CONTROL != FALSE) ? (float(Z) * EXP2_NEG_32) : ((float(Z) * EXP2_NEG_32) * 2.0f - 1.0f))
#endif
#define VS_VERTICES_PARAM(NAME) uint NAME
#define VS_INDICES_PARAM(NAME) uint NAME
#define VS_BASE_VERTEX BaseVertex
#define VS_BASE_INDEX BaseIndex
#define VS_LOAD_VERTEX(VERTICES, IDX) vertex_buffer[IDX]
#if HAS_INDEX_BUFFER
	#define VS_LOAD_INDEX(INDICES, IDX) index_buffer[IDX]
#else
	#define VS_LOAD_INDEX(INDICES, IDX) 0
#endif
#define VS_NEEDS_EXPAND (VS_EXPAND_TYPE != VS_EXPAND_NONE)

// Pixel shader helpers
#define PS_SAMPLE_TEX(POS) (texture(Texture, FLOAT2(POS)))
#define PS_SAMPLE_TEX_LOD(POS, LOD) (textureLod(Texture, FLOAT2(POS), float(LOD)))
#define PS_SAMPLE_TEX_DEPTH(POS) (PS_SAMPLE_TEX((POS)).r)
#define PS_SAMPLE_TEX_DEPTH_LOD(POS, LOD) (PS_SAMPLE_TEX_LOD((POS), (LOD)).r)
#define PS_READ_TEX(POS, LOD) (texelFetch(Texture, INT2(POS), int(LOD)))
#define PS_READ_TEX_DEPTH(POS, LOD) (PS_READ_TEX((POS), (LOD)).r)
#define PS_READ_PALETTE(POS) (texelFetch(Palette, INT2(POS), 0))
#define PS_READ_PRIMID(POS) (texelFetch(PrimMinTexture, INT2(POS), 0).r)
#define PS_GET_TEX_DIMS(OUT_VAR) (OUT_VAR = UINT2(textureSize(Texture, 0)))
#define PS_GET_TEX_DEPTH_DIMS(OUT_VAR) (PS_GET_TEX_DIMS(OUT_VAR))
// Unused in VK/GL
#define PS_POINT_SAMPLER 0
#define BROKEN_SHADER_DEPTH 0

/// End helper macros for shared shader code

#include "tfx_defs.inc"

#ifdef VERTEX_SHADER

#if VS_EXPAND_TYPE != VS_EXPAND_NONE

// VS constants for determining base vertex/index in expand shader.
#if PCSX2_VULKAN
layout(push_constant) uniform cb2
#elif PCSX2_OPENGL
layout(std140, binding = 4) uniform cb22
#endif
{
	#define X(TYPE, NAME) TYPE NAME;
		VS_PUSH_CONSTANTS(X)
	#undef X
};

// Vertex buffer for expand shaders (sprites, upscaled lines, AA1 edges, etc.)
#if PCSX2_VULKAN
layout(std140, set = 0, binding = 2)
#elif PCSX2_OPENGL
layout(std140, binding = 2)
#endif
readonly buffer VertexBuffer
{
	VSRawVertex vertex_buffer[];
};

#if HAS_INDEX_BUFFER
	// Index buffer for rearranging vertices in AA1 expand shader.
	// Warning: use std430 instead of std140 so that the ints are tightly packed.
	#if PCSX2_VULKAN
	layout(std430, set = 0, binding = 3) 
	#elif PCSX2_OPENGL
	layout(std430, binding = 3)
	#endif
	readonly buffer IndexBuffer
	{
		uint index_buffer[];
	};
#endif

#endif // VS_EXPAND_TYPE

// Note: vertex/index buffers must be defined before common code is included.
#include "tfx_vs.inc"

// Vertex shader constant buffer.
#if PCSX2_VULKAN
layout(std140, set = 0, binding = 0) uniform cb0
#elif PCSX2_OPENGL
layout(std140, binding = 1) uniform cb20
#endif
{
	#define X(TYPE, NAME) TYPE NAME;
		VS_UNIFORMS(X)
	#undef X
};

// Get VS constants for shared code.
VSUniformsGeneric GetVSUniforms()
{
	VSUniformsGeneric cb;
	#define X(TYPE, NAME) cb.NAME = NAME;
		VS_UNIFORMS(X)
	#undef X
	return cb;
}

// Vertex shader outputs
#if PCSX2_VULKAN
layout(location = 0) out VSOutput
#elif PCSX2_OPENGL
out SHADER
#endif
{
	vec4 t;
	vec4 ti;
	#if VS_IIP != 0
		vec4 c;
	#else
		flat vec4 c;
	#endif
	float inv_cov; // We use the inverse to make it simpler to interpolate.
	flat uint interior; // 1 for triangle interior; 0 for edge;
} vsOut;

// Write real VS outputs from shared code ouput.
void WriteVSOutput(VSOutputGeneric v)
{
	gl_Position = v.p;
	vsOut.t = v.t;
	vsOut.ti = v.ti;
	vsOut.c = v.c;
	vsOut.inv_cov = v.inv_cov;
	vsOut.interior = v.interior;
	#if VS_POINT_SIZE
		gl_PointSize = v.point_size;
	#endif
}

#if VS_EXPAND_TYPE == VS_EXPAND_NONE

// Note: VK and GL have different layouts as GL uses the same layout
// for convert shaders and TFX shaders, and thus has more attributes.
#if PCSX2_VULKAN
	layout(location = 0) in vec2  a_st;
	layout(location = 1) in uvec4 a_c;
	layout(location = 2) in float a_q;
	layout(location = 3) in uvec2 a_p;
	layout(location = 4) in uint  a_z;
	layout(location = 5) in uvec2 a_uv;
	layout(location = 6) in vec4  a_f;
#elif PCSX2_OPENGL
	layout(location = 0) in vec2  a_st;
	layout(location = 2) in vec4  a_c;
	layout(location = 3) in float a_q;
	layout(location = 4) in uvec2 a_p;
	layout(location = 5) in uint  a_z;
	layout(location = 6) in uvec2 a_uv;
	layout(location = 7) in vec4  a_f;
#endif

// Get VS inputs for shared code.
VSInputGeneric GetVSInput()
{
	VSInputGeneric vin;
	vin.st = a_st;
	vin.c = FLOAT4(a_c);
	vin.q = a_q;
	vin.p = a_p;
	vin.z = a_z;
	vin.uv = a_uv;
	vin.f = a_f;
	return vin;
}

void main()
{
	VSInputGeneric vin = GetVSInput();
	VSUniformsGeneric cb = GetVSUniforms();
	VSOutputGeneric vout = vs_main_impl(vin, cb);
	WriteVSOutput(vout);
}

#else // VS_EXPAND_TYPE

void main()
{
	#if PCSX2_VULKAN
		uint vid = uint(gl_VertexIndex);
	#elif PCSX2_OPENGL
		uint vid = uint(gl_VertexID);
	#endif
	VSUniformsGeneric cb = GetVSUniforms();
	VSOutputGeneric vout = vs_expand_impl(vid, 0, cb, 0);
	WriteVSOutput(vout);
}

#endif // VS_EXPAND_TYPE

#endif // VERTEX_SHADER

#ifdef FRAGMENT_SHADER

#define USE_FEEDBACK_SAMPLER (DISABLE_TEXTURE_BARRIER || HAS_FEEDBACK_LOOP_LAYOUT)

// Pixel shader constant buffer.
#if PCSX2_VULKAN
layout(std140, set = 0, binding = 1) uniform cb1
#elif PCSX2_OPENGL
layout(std140, binding = 0) uniform cb21
#endif
{
	#define X(TYPE, NAME) TYPE NAME;
		PS_UNIFORMS(X)
	#undef X
};

// Pixel shader inputs
#if PCSX2_VULKAN
layout(location = 0) in VSOutput
#elif PCSX2_OPENGL
in SHADER
#endif
{
	vec4 t;
	vec4 ti;
	#if PS_IIP != 0
		vec4 c;
	#else
		flat vec4 c;
	#endif
	float inv_cov; // We use the inverse to make it simpler to interpolate.
	flat uint interior; // 1 for triangle interior; 0 for edge;
} vsIn;

#define TARGET_0_QUALIFIER out

// OpenGL: Framebuffer fetch macros
#if PCSX2_OPENGL && HAS_FRAMEBUFFER_FETCH && NEEDS_RT
	// We need to force the colour to be defined here, to read from it.
	// Basically the only scenario where this'll happen is RGBA masked and DATE is active.
	#undef PS_NO_COLOR
	#define PS_NO_COLOR 0
	#if defined(GL_EXT_shader_framebuffer_fetch)
		#undef TARGET_0_QUALIFIER
		#define TARGET_0_QUALIFIER inout
		#define LAST_FRAG_COLOR o_col0
	#elif defined(GL_ARM_shader_framebuffer_fetch)
		#define LAST_FRAG_COLOR gl_LastFragColorARM
	#endif
#endif

// Pixel shader outputs
#if PS_RETURN_COLOR
	#if !PS_NO_COLOR1
		layout(location = 0, index = 0) TARGET_0_QUALIFIER vec4 o_col0;
		layout(location = 0, index = 1) out vec4 o_col1;
	#elif !PS_NO_COLOR
		layout(location = 0) out vec4 o_col0;
	#endif
#endif

// OpengL: Depth feedback mode 2 is for depth as color.
// Use FB fetch for the feedback if it's available.
#if PCSX2_OPENGL && SW_DEPTH && PS_NO_COLOR1 && (DEPTH_FEEDBACK_SUPPORT == 2)
	#if HAS_FRAMEBUFFER_FETCH
		layout(location = 1) inout float o_col1;
	#else
		layout(location = 1) out float o_col1;
	#endif
#endif

// Pixels shader resource declarations.
#if PCSX2_VULKAN
	layout(set = 1, binding = 0) uniform sampler2D Texture;
	layout(set = 1, binding = 1) uniform texture2D Palette;
	layout(set = 1, binding = 3) uniform texture2D PrimMinTexture;
	layout(set = 1, binding = 5, rgba8) uniform restrict coherent image2D RtImageRov;
	layout(set = 1, binding = 6, r32f) uniform restrict coherent image2D DepthImageRov;

	#if USE_FEEDBACK_SAMPLER
		layout(set = 1, binding = 2) uniform texture2D RtSampler;
		layout(set = 1, binding = 4) uniform texture2D DepthSampler;
	#else
		// Must consider each case separately since the input attachment indices must be consecutive.
		#if (NEEDS_RT && !PS_ROV_COLOR) && (SW_DEPTH && !PS_ROV_DEPTH)
			layout(input_attachment_index = 0, set = 1, binding = 2) uniform subpassInput RtSampler;
			layout(input_attachment_index = 1, set = 1, binding = 4) uniform subpassInput DepthSampler;
		#elif (NEEDS_RT && !PS_ROV_COLOR)
			layout(input_attachment_index = 0, set = 1, binding = 2) uniform subpassInput RtSampler;
		#elif (SW_DEPTH && !PS_ROV_DEPTH)
			layout(input_attachment_index = 0, set = 1, binding = 4) uniform subpassInput DepthSampler;
		#endif
	#endif
#elif PCSX2_OPENGL
	layout(binding = 0) uniform sampler2D Texture;
	layout(binding = 1) uniform sampler2D Palette;
	layout(binding = 2) uniform sampler2D RtSampler;
	layout(binding = 3) uniform sampler2D PrimMinTexture;
	layout(binding = 4) uniform sampler2D DepthSampler;
#endif

#if ZWRITE && PS_HAS_CONSERVATIVE_DEPTH && !SW_DEPTH
	layout(depth_less) out float gl_FragDepth;
#endif

#if PS_ROV_COLOR || PS_ROV_DEPTH
	layout(pixel_interlock_ordered) in;
#endif

#if PS_ROV_EARLYDEPTHSTENCIL
	layout(early_fragment_tests) in;
#endif

vec4 RtLoad(ivec2 xy)
{
#if PCSX2_VULKAN
	#if PS_ROV_COLOR
		return imageLoad(RtImageRov, xy);
	#elif NEEDS_RT && USE_FEEDBACK_SAMPLER
		return texelFetch(RtSampler, xy, 0);
	#elif NEEDS_RT
		return subpassLoad(RtSampler);
	#else
		return vec4(0.0f);
	#endif
#elif PCSX2_OPENGL
	#if !NEEDS_RT
		return vec4(0.0f);
	#elif HAS_FRAMEBUFFER_FETCH
		return LAST_FRAG_COLOR;
	#else
		return texelFetch(RtSampler, xy, 0);
	#endif
#endif
}

float DepthLoad(ivec2 xy)
{
#if PCSX2_VULKAN
	#if PS_ROV_COLOR
		return imageLoad(DepthImageRov, xy).r;
	#elif SW_DEPTH && USE_FEEDBACK_SAMPLER
		return texelFetch(DepthSampler, xy, 0).r;
	#elif SW_DEPTH
		return subpassLoad(DepthSampler).r;
	#else
		return 0.0f;
	#endif
#elif PCSX2_OPENGL
	#if !SW_DEPTH
		return 0.0f;
	#elif HAS_FRAMEBUFFER_FETCH && (DEPTH_FEEDBACK_SUPPORT == 2)
		return o_col1;
	#else
		return texelFetch(DepthSampler, xy, 0).r;
	#endif
#endif
}

// Get pixel shader constants for shared code.
PSUniformsGeneric GetPSUniforms()
{
	PSUniformsGeneric cb;
	#define X(TYPE, NAME) cb.NAME = NAME;
		PS_UNIFORMS(X)
	#undef X
	return cb;
}

// Get pixel shader inputs for shared code.
PSInputGeneric GetPSInput()
{
	PSInputGeneric ps_in;
	ps_in.p = gl_FragCoord;
	ps_in.t = vsIn.t;
	ps_in.ti = vsIn.ti;
	ps_in.c = vsIn.c;
	ps_in.fc = vsIn.c;
	ps_in.inv_cov = vsIn.inv_cov;
	ps_in.interior = vsIn.interior;
	return ps_in;
}

// Pixel shader global state
PSInputGeneric ps_in;
PSUniformsGeneric ps_cb;
vec4 ps_current_color;
float ps_current_depth;
uint ps_prim_id;
bool ps_color_discarded;
bool ps_depth_discarded;

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

void main()
{
	ps_in = GetPSInput();
	ps_cb = GetPSUniforms();
	ps_prim_id = gl_PrimitiveID;
	ps_color_discarded = false;
	ps_depth_discarded = false;

	ivec2 coord = ivec2(ps_in.p.xy);

	#if PS_ROV_COLOR || PS_ROV_DEPTH
		beginInvocationInterlockARB();
	#endif

	ps_current_depth = DepthLoad(coord);

	ps_current_color = RtLoad(coord);

	PSOutputGeneric psout = ps_main_impl();

	#if PS_RETURN_COLOR
		o_col0 = psout.c0;
		#if !PS_NO_COLOR1
			o_col1 = psout.c1;
		#endif
	#elif PS_RETURN_COLOR_ROV
		psout.c0 = mix(psout.c0, ps_current_color, equal(fbmask, uvec4(0xFFu))); // channel masking
		if (!ps_color_discarded)
			imageStore(RtImageRov, coord, psout.c0);
	#endif
	
	// Writing back depth
	#if PS_RETURN_DEPTH
		gl_FragDepth = psout.depth;
		#if PCSX2_OPENGL && SW_DEPTH && PS_NO_COLOR1 && (DEPTH_FEEDBACK_SUPPORT == 2)
			// Depth as color write. For depth as color feedback we write to both
			// color copy and real depth to avoid having to copy back to real depth.
			// Warning: do not write o_col1 until the end since the value might
			// be needed for FB fetch in sample_from_depth().
			o_col1 = psout.depth;
		#endif
	#elif PS_RETURN_DEPTH_ROV
		if (!ps_depth_discarded)
			imageStore(DepthImageRov, coord, vec4(psout.depth, 0, 0, 1.0f));
	#endif

	#if PS_ROV_COLOR || PS_ROV_DEPTH
		endInvocationInterlockARB();
	#endif
}

#endif
