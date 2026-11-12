#ifdef VERTEX_SHADER

#ifndef PCSX2_GLSL
struct VSUniform
#elif PCSX2_VULKAN
layout(std140, set = 0, binding = 0) uniform cb0
#elif PCSX2_OPENGL
layout(std140, binding = 1) uniform cb20
#endif
{
	float2 vertex_scale;
	float2 vertex_offset;
	float2 texture_scale;
	float2 texture_offset;
	float2 point_size;
	uint max_depth_vs;
	float line_aa1_width;
}
#ifdef PCSX2_GLSL
cb
#endif
;

#endif // VERTEX_SHADER

#if defined(FRAGMENT_SHADER) || defined(PIXEL_SHADER)

#ifndef PCSX2_GLSL
struct PSUniform
#elif PCSX2_VULKAN
layout(std140, set = 0, binding = 1) uniform cb1
#elif PCSX2_OPENGL
layout(std140, binding = 0) uniform cb21
#endif
{
	float3 fog_color;
	float aref;
	float4 wh; ///< xy => PS2, zw => actual (upscaled)
	float2 ta;
	float max_depth_ps;
	float alpha_fix;
	uint4 fbmask;
	float4 half_texel;
#if defined(PCSX2_MSL) || defined(PCSX2_CPP)
	union
	{
		float4 uv_min_max;
		uint4 uv_msk_fix;
	};
#else
	float4 uv_min_max;
#endif
	float4 lod_params;
	float4 st_range;
	uint channel_shuffle_blue_mask;
	uint channel_shuffle_blue_shift;
	uint channel_shuffle_green_mask;
	uint channel_shuffle_green_shift;
	float2 channel_shuffle_offset;
	float2 tc_offset;
	float2 st_scale;
	float4x4 dither_matrix;
	float4 scale_factor;
	float line_cov_scale;
	float _pad0;
	float _pad1;
	float _pad2;
}
#ifdef PCSX2_GLSL
cb
#endif
;
#endif // defined(FRAGMENT_SHADER) || defined(PIXEL_SHADER)
