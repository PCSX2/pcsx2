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

	uint BaseVertex;
	uint BaseIndex;
};

#ifdef VERTEX_SHADER

#ifndef VS_EXPAND_NONE
#define VS_EXPAND_NONE 0
#define VS_EXPAND_POINT 1
#define VS_EXPAND_LINE 2
#define VS_EXPAND_SPRITE 3
#define VS_EXPAND_LINE_AA1 4
#define VS_EXPAND_TRIANGLE_AA1 5
#endif

out SHADER
{
	vec4 t_float;
	vec4 t_int;
	#if VS_IIP != 0
		vec4 c;
	#else
		flat vec4 c;
	#endif
	float inv_cov; // We use the inverse to make it simpler to interpolate.
	uint interior; // 1 for triangle interior; 0 for edge;
} VSout;

const float exp_min32 = exp2(-32.0f);

#if VS_EXPAND == VS_EXPAND_NONE

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

// Warning: use std430 instead of std140 so that the ints are tightly packed.
layout(std430, binding = 3) readonly buffer IndexBuffer {
	uint index_buffer[];
};

struct ProcessedVertex
{
	vec4 p;
	vec4 t_float;
	vec4 t_int;
	vec4 c;
};

uint load_index(uint _i)
{
	uint i = _i + BaseIndex;
	// i is even => load lower 16 bits; i odd => load upper 16 bits.
	uint shift = (i & 1) << 4;
	return BaseVertex + ((index_buffer[i >> 1] >> shift) & 0xFFFF);
}

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

#if VS_EXPAND == VS_EXPAND_POINT

	vtx = load_vertex(vid >> 2);

	vtx.p.x += ((vid & 1u) != 0u) ? PointSize.x : 0.0f;
	vtx.p.y += ((vid & 2u) != 0u) ? PointSize.y : 0.0f;

#elif (VS_EXPAND == VS_EXPAND_LINE) || (VS_EXPAND == VS_EXPAND_LINE_AA1)

	uint vid_base = vid >> 2;
	bool is_bottom = (vid & 2u) != 0u;
	bool is_right = (vid & 1u) != 0u;
	uint vid_other = is_bottom ? vid_base - 1 : vid_base + 1;
	vtx = load_vertex(vid_base);
	ProcessedVertex other = load_vertex(vid_other);

	// Use bottom minus top for delta regardless of which vertex we are expanding.
	vec2 line_delta = is_bottom ? (vtx.p.xy - other.p.xy) : (other.p.xy - vtx.p.xy);
	vec2 line_vector = normalize(line_delta);
#if VS_EXPAND == VS_EXPAND_LINE
	vec2 line_expand = vec2(line_vector.y, -line_vector.x);
#elif VS_EXPAND == VS_EXPAND_LINE_AA1
	// Expand in y direction for shallow lines and x direction for steep lines.
	line_delta /= VertexScale;
	vec2 line_expand = abs(line_delta.x) >= abs(line_delta.y) ? vec2(0.0f, 2.0f) : vec2(2.0f, 0.0f);
#endif
	vec2 line_width = (line_expand * PointSize) / 2;
	vec2 offset = is_right ? line_width : -line_width;
	vtx.p.xy += offset;

#if VS_EXPAND == VS_EXPAND_LINE_AA1
	VSout.inv_cov = is_right ? 1.0f : -1.0f;
#endif

	// Lines will be run as (0 1 2) (1 2 3)
	// This means that both triangles will have a point based off the top line point as their first point
	// So we don't have to do anything for !IIP

#elif VS_EXPAND == VS_EXPAND_SPRITE

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

#elif VS_EXPAND == VS_EXPAND_TRIANGLE_AA1

	// Triangles with AA1 are expanded as follows:
	// - Vertices 0-2: Interior of triangle (1 triangle).
	// - Vertices 3-8: First edge expanded (2 triangles).
	// - Vertices 9-14: Second edge expanded (2 triangles).
	// - Vertices 15-20: Third edge expanded (2 triangles).

	uint prim_id = vid / 21;
	uint prim_offset = vid - 21 * prim_id; // range: 0-20
	bool interior = prim_offset < 3;

	if (interior)
	{
		vtx = load_vertex(load_index(3 * prim_id + prim_offset));
		VSout.inv_cov = 0.0f; // Full coverage
		VSout.interior = 1;
	}
	else
	{
		// Vertex indices for this edge. We need all 3 for determining exterior/interior.
		uint prim_offset_edges = prim_offset - 3; // range: 0-17
		uint i0 = prim_offset_edges / 6;
		uint i1 = (i0 >= 2) ? i0 - 2 : i0 + 1;
		uint i2 = (i0 >= 1) ? i0 - 1 : i0 + 2;
		uint edge_offset = prim_offset_edges - 6 * i0; // range: 0-5

		// Note: order of top/bottom, inside/outside order is arbitrary,
		// as long as it assembles into two triangles forming a quad.
		bool is_bottom = (2 <= edge_offset) && (edge_offset <= 4);
		bool is_outside = (edge_offset & 1) != 0;

		vtx = load_vertex(load_index(3 * prim_id + i0));
		ProcessedVertex other = load_vertex(load_index(3 * prim_id + i1));
		ProcessedVertex opposite = load_vertex(load_index(3 * prim_id + i2));

		// Similar expansion to line AA1 except instead of expanding on both sides of
		// the line we expand on on the side towards the outside of the triangle.
		vec2 line_delta = vtx.p.xy - other.p.xy;
		vec2 line_normal = normalize(vec2(line_delta.y, -line_delta.x));
		vec2 line_expand = abs(line_delta.x) >= abs(line_delta.y) ? vec2(0.0f, 2.0f) : vec2(2.0f, 0.0f);
		if ((dot(line_expand, line_normal) >= 0.0f) == (dot(opposite.p.xy - vtx.p.xy, line_normal) >= 0.0f))
		{
			// Expand direction point towards the interior so flip it.
			line_expand = -line_expand;
		}
		vec2 line_width = (line_expand * PointSize) / 2;

		if (is_bottom)
			vtx = other;
		if (is_outside)
		{
			vtx.p.xy += line_width;
			VSout.inv_cov = 1.0f; // No coverage
		}
		else
		{
			VSout.inv_cov = 0.0f; // Full coverage
		}

		VSout.interior = 0;
	}

#endif

	gl_Position = vtx.p;
	VSout.t_float = vtx.t_float;
	VSout.t_int = vtx.t_int;
	VSout.c = vtx.c;
}

#endif // VS_EXPAND

#endif // VERTEX_SHADER
