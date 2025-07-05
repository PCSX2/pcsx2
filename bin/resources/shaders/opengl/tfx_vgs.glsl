// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

//#version 420 // Keep it for text editor detection

layout(std140, binding = 1) uniform cb20
{
	vec2  VertexScale;
	vec2  VertexOffset;

	vec2  TextureScale;
	vec2  TextureOffset;

	vec2  PointSize;
	uint  MaxDepth;
	uint  pad_cb20;
};

#ifdef VERTEX_SHADER

out SHADER
{
	vec4 t_float;
	vec4 t_int;
	#if VS_IIP != 0
		vec4 c;
	#else
		flat vec4 c;
	#endif
} VSout;

const float exp_min32 = exp2(-32.0f);

#if VS_EXPAND == 0

layout(location = 0) in vec2  i_st;
layout(location = 2) in vec4  i_c;
layout(location = 3) in float i_q;
layout(location = 4) in uvec2 i_p;
layout(location = 5) in uint  i_z;
layout(location = 6) in uvec2 i_uv;
layout(location = 7) in vec4  i_f;

void texture_coord()
{
	vec2 uv = vec2(i_uv) - TextureOffset;
	vec2 st = i_st - TextureOffset;

	// Float coordinate
	VSout.t_float.xy = st;
	VSout.t_float.w  = i_q;

	// Integer coordinate => normalized
	VSout.t_int.xy = uv * TextureScale;
#if VS_FST
	// Integer coordinate => integral
	VSout.t_int.zw = uv;
#else
	// Some games uses float coordinate for post-processing effect
	VSout.t_int.zw = st / TextureScale;
#endif
}

void vs_main()
{
	// Clamp to max depth, gs doesn't wrap
	highp uint z = min(i_z, MaxDepth);

	// pos -= 0.05 (1/320 pixel) helps avoiding rounding problems (integral part of pos is usually 5 digits, 0.05 is about as low as we can go)
	// example: ceil(afterseveralvertextransformations(y = 133)) => 134 => line 133 stays empty
	// input granularity is 1/16 pixel, anything smaller than that won't step drawing up/left by one pixel
	// example: 133.0625 (133 + 1/16) should start from line 134, ceil(133.0625 - 0.05) still above 133
	gl_Position.xy = vec2(i_p) - vec2(0.05f, 0.05f);
	gl_Position.xy = gl_Position.xy * VertexScale - VertexOffset;
	gl_Position.z = float(z) * exp_min32;
	gl_Position.w = 1.0f;

	texture_coord();

	VSout.c = i_c;
	VSout.t_float.z = i_f.x; // pack for with texture

	#if VS_POINT_SIZE
		gl_PointSize = PointSize.x;
	#endif
}

#else // VS_EXPAND

struct RawVertex
{
	vec2 ST;
	uint RGBA;
	float Q;
	uint XY;
	uint Z;
	uint UV;
	uint FOG;
};

layout(std140, binding = 2) readonly buffer VertexBuffer {
	RawVertex vertex_buffer[];
};

struct ProcessedVertex
{
	vec4 p;
	vec4 t_float;
	vec4 t_int;
	vec4 c;
};

ProcessedVertex load_vertex(uint index)
{
	RawVertex rvtx = vertex_buffer[index];

	vec2 i_st = rvtx.ST;
	vec4 i_c = vec4(uvec4(bitfieldExtract(rvtx.RGBA, 0, 8), bitfieldExtract(rvtx.RGBA, 8, 8),
	                      bitfieldExtract(rvtx.RGBA, 16, 8), bitfieldExtract(rvtx.RGBA, 24, 8)));
	float i_q = rvtx.Q;
	uvec2 i_p = uvec2(bitfieldExtract(rvtx.XY, 0, 16), bitfieldExtract(rvtx.XY, 16, 16));
	uint i_z = rvtx.Z;
	uvec2 i_uv = uvec2(bitfieldExtract(rvtx.UV, 0, 16), bitfieldExtract(rvtx.UV, 16, 16));
	vec4 i_f = unpackUnorm4x8(rvtx.FOG);

	ProcessedVertex vtx;

	uint z = min(i_z, MaxDepth);
	vtx.p.xy = vec2(i_p) - vec2(0.05f, 0.05f);
	vtx.p.xy = vtx.p.xy * VertexScale - VertexOffset;
	vtx.p.z = float(z) * exp_min32;
	vtx.p.w = 1.0f;

	vec2 uv = vec2(i_uv) - TextureOffset;
	vec2 st = i_st - TextureOffset;

	vtx.t_float.xy = st;
	vtx.t_float.w  = i_q;

	vtx.t_int.xy = uv * TextureScale;
#if VS_FST
	vtx.t_int.zw = uv;
#else
	vtx.t_int.zw = st / TextureScale;
#endif

	vtx.c = i_c;
	vtx.t_float.z = i_f.x;

	return vtx;
}

void main()
{
	ProcessedVertex vtx;

	uint vid = uint(gl_VertexID);

#if VS_EXPAND == 1 // Point

	vtx = load_vertex(vid >> 2);

	vtx.p.x += ((vid & 1u) != 0u) ? PointSize.x : 0.0f;
	vtx.p.y += ((vid & 2u) != 0u) ? PointSize.y : 0.0f;

#elif VS_EXPAND == 2 // Line

	uint vid_base = vid >> 2;
	bool is_bottom = (vid & 2u) != 0u;
	bool is_right = (vid & 1u) != 0u;
	uint vid_other = is_bottom ? vid_base - 1 : vid_base + 1;
	vtx = load_vertex(vid_base);
	ProcessedVertex other = load_vertex(vid_other);

	vec2 line_vector = normalize(vtx.p.xy - other.p.xy);
	vec2 line_normal = vec2(line_vector.y, -line_vector.x);
	vec2 line_width = (line_normal * PointSize) / 2;
	// line_normal is inverted for bottom point
	vec2 offset = ((uint(is_bottom) ^ uint(is_right)) != 0u) ? line_width : -line_width;
	vtx.p.xy += offset;

	// Lines will be run as (0 1 2) (1 2 3)
	// This means that both triangles will have a point based off the top line point as their first point
	// So we don't have to do anything for !IIP

#elif VS_EXPAND == 3 // Sprite

	// Sprite points are always in pairs
	uint vid_base = vid >> 1;
	uint vid_lt = vid_base & ~1u;
	uint vid_rb = vid_base | 1u;

	ProcessedVertex lt = load_vertex(vid_lt);
	ProcessedVertex rb = load_vertex(vid_rb);
	vtx = rb;

	bool is_right = ((vid & 1u) != 0u);
	vtx.p.x = is_right ? lt.p.x : vtx.p.x;
	vtx.t_float.x = is_right ? lt.t_float.x : vtx.t_float.x;
	vtx.t_int.xz = is_right ? lt.t_int.xz : vtx.t_int.xz;

	bool is_bottom = ((vid & 2u) != 0u);
	vtx.p.y = is_bottom ? lt.p.y : vtx.p.y;
	vtx.t_float.y = is_bottom ? lt.t_float.y : vtx.t_float.y;
	vtx.t_int.yw = is_bottom ? lt.t_int.yw : vtx.t_int.yw;

#endif

	gl_Position = vtx.p;
	VSout.t_float = vtx.t_float;
	VSout.t_int = vtx.t_int;
	VSout.c = vtx.c;
}

#endif // VS_EXPAND

#endif // VERTEX_SHADER
