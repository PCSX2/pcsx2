// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
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
	float LineAA1Width;

	uvec2 XYOffset;
	float ScaleRT;
	float ScaleTex;
};

#ifdef VERTEX_SHADER

#ifndef VS_EXPAND_NONE
#define VS_EXPAND_NONE 0
#define VS_EXPAND_POINT 1
#define VS_EXPAND_LINE 2
#define VS_EXPAND_SPRITE 3
#define VS_EXPAND_LINE_AA1 4
#define VS_EXPAND_TRIANGLE_AA1 5
#define VS_EXPAND_TRIANGLE 6
#endif

#ifndef VS_CLAMP_UV_NONE
#define VS_CLAMP_UV_NONE 0
#define VS_CLAMP_UV_NEAREST 1
#define VS_CLAMP_UV_LINEAR 2
#endif

#ifndef VS_ALIGN_UV_NONE
#define VS_ALIGN_UV_NONE 0
#define VS_ALIGN_UV_ALIGN 1
#define VS_ALIGN_UV_PASSTHROUGH 2
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
	flat uint interior; // 1 for triangle interior; 0 for edge.

	#if VS_ROUND_UV || VS_CLAMP_UV || VS_ALIGN_UV
		flat uvec4 rounduv;
	#endif

	#if VS_ROUND_UV
		flat vec4 scaleuv;
	#endif

	#if VS_CLAMP_UV
		flat vec4 clampuv;
	#endif
} VSout;

const float exp_min32 = exp2(-32.0f);

uvec4 extract_round_uv_bits(float q)
{
	uint qi = floatBitsToUint(q);
	return uvec4(
		(qi >> 0) & 0xFFF,  // Prim left
		(qi >> 12) & 0xFFF, // Prim top
		(qi >> 24) & 0xF,   // Round U flags
		(qi >> 28) & 0xF    // Round V flags
	);
}

vec2 raw_coords_to_ndc(uvec2 raw_pos)
{
	// pos -= 0.05 (1/320 pixel) helps avoiding rounding problems (integral part of pos is usually 5 digits, 0.05 is about as low as we can go)
	// example: ceil(afterseveralvertextransformations(y = 133)) => 134 => line 133 stays empty
	// input granularity is 1/16 pixel, anything smaller than that won't step drawing up/left by one pixel
	// example: 133.0625 (133 + 1/16) should start from line 134, ceil(133.0625 - 0.05) still above 133
	return (vec2(raw_pos) - vec2(0.05f)) * VertexScale - VertexOffset;
}

vec2 window_coords_to_ndc(vec2 window_pos)
{
	// This function is only used by shader sprite align and always uses a native HPO.
	return (window_pos + vec2(8.0f - 0.05f)) * vec2(VertexScale.xy) - vec2(1.0f);
}

float raw_z_to_ndc(uint z)
{
	return float(z) * exp2(-32.0f); // integer->float depth
}

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
#if !(VS_ROUND_UV || VS_CLAMP_UV || VS_ALIGN_UV)
	vec2 uv = vec2(i_uv) - TextureOffset;
#else
	vec2 uv = i_st - TextureOffset;
#endif
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

// Get UV rounding info saved in Q.
#if VS_ROUND_UV || VS_CLAMP_UV || VS_ALIGN_UV
	VSout.rounduv = extract_round_uv_bits(i_q);
	VSout.t_float.w = 1.0f;
#endif
}

void vs_main()
{
	// Clamp to max depth, gs doesn't wrap
	highp uint z = min(i_z, MaxDepth);

	gl_Position = vec4(raw_coords_to_ndc(i_p), raw_z_to_ndc(z), 1.0f);

	#if HAS_CLIP_CONTROL
		gl_Position.z = float(z) * exp_min32;
	#else
		gl_Position.z = (float(z) * exp_min32) * 2.0f - 1.0f;
	#endif

	gl_Position.w = 1.0f;

	texture_coord();

	VSout.c = i_c;
	VSout.t_float.z = i_f.x; // pack fog with texture

	#if VS_POINT_SIZE
		gl_PointSize = PointSize.x;
	#endif

	#if VS_CLAMP_UV
		VSout.clampuv = vec4(0.0f);
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

layout(std140, binding = 4) uniform cb22
{
	uint BaseVertex;
	uint BaseIndex;
	uint pad_cb22_0;
	uint pad_cb22_1;
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
	#if VS_ROUND_UV || VS_CLAMP_UV || VS_ALIGN_UV
		vec2 window_pos;
		uvec4 rounduv;
	#endif
};

vec4 sprite_clamp_uv_range(vec4 pos, vec4 tex, uvec4 round_info)
{
	bool rev_x = pos.x > pos.z;
	bool rev_y = pos.y > pos.w;

	if (rev_x)
	{
		pos.xz = pos.zx;
		tex.xz = tex.zx;
	}

	if (rev_y)
	{
		pos.yw = pos.wy;
		tex.yw = tex.wy;
	}

	vec4 pos_round = vec4(ceil(pos.xy / 16.0f) * 16.0f, floor((pos.zw - vec2(1.0f)) / 16.0f) * 16.0f);

	vec4 d_tex = tex.zwzw - tex.xyxy;
	vec4 d_pos = pos.zwzw - pos.xyxy;

	vec4 grad = d_tex / d_pos;

	tex += grad * (pos_round - pos);

	// Do rounding of the endpoints.
	uvec4 topleft = uvec4(equal(pos / 16.0f, vec4(round_info.xyxy)));
	uvec4 round_flags = round_info.zwzw & uvec4(ROUND_UV_DOWN | ROUND_UV_UP);
	uvec4 round_down = uvec4(equal(round_flags, uvec4(ROUND_UV_DOWN))) &  ~topleft;
	uvec4 round_up = uvec4(equal(round_flags, uvec4(ROUND_UV_UP))) |
	                 (uvec4(equal(round_flags, uvec4(ROUND_UV_DOWN))) & topleft);
	tex = mix(tex, tex - 1 / 32.0f, bvec4(round_down));
	tex = mix(tex, tex + 1 / 32.0f, bvec4(round_up));

	tex = vec4(min(tex.xy, tex.zw), max(tex.xy, tex.zw));

	#if VS_CLAMP_UV == VS_CLAMP_UV_LINEAR
		// Bilinear: truncate to 1/16 texel;
		tex = floor(tex) + vec4(ROUND_UV_THRESHOLD);
	#elif VS_CLAMP_UV == VS_CLAMP_UV_NEAREST
		// Nearest: place in texel center, accounting for upscaling.
		tex = vec4(floor(tex / 16.0f) * 16.0f) + vec2(8.0f / ScaleTex, 16.0f - 8.0f / ScaleTex).xxyy;
	#endif

	return tex;
}

void sprite_align_and_round(inout vec4 pos, inout vec4 tex)
{
	bool rev_x = pos.x > pos.z;
	bool rev_y = pos.y > pos.w;

	if (rev_x)
	{
		pos.xz = pos.zx;
		tex.xz = tex.zx;
	}

	if (rev_y)
	{
		pos.yw = pos.wy;
		tex.yw = tex.wy;
	}

	vec4 d_tex = tex.zwzw - tex.xyxy;
	vec4 d_pos = pos.zwzw - pos.xyxy;

	vec4 grad = d_tex / d_pos;

	vec4 pos_round = vec4(ceil(pos.xy / 16.0f) * 16.0f, floor((pos.zw - vec2(1.0f)) / 16.0f) * 16.0f);

	pos_round.xy += -8.0f;
	pos_round.zw += 8.0f;

	tex += grad * (pos_round - pos);

	pos = pos_round;

	if (rev_x)
	{
		pos.xz = pos.zx;
		tex.xz = tex.zx;
	}

	if (rev_y)
	{
		pos.yw = pos.wy;
		tex.yw = tex.wy;
	}
}

uint load_index(uint _i)
{
	uint i = _i + BaseIndex;
	// i is even => load lower 16 bits; i odd => load upper 16 bits.
	uint shift = (i & 1u) << 4u;
	return (index_buffer[i >> 1u] >> shift) & 0xFFFFu;
}

ProcessedVertex load_vertex(uint index)
{
	RawVertex rvtx = vertex_buffer[BaseVertex + index];

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

	vtx.p = vec4(raw_coords_to_ndc(i_p), raw_z_to_ndc(z), 1.0f);

	#if VS_ROUND_UV || VS_CLAMP_UV || VS_ALIGN_UV
		vtx.window_pos = vec2(i_p) - vec2(XYOffset);
	#endif

	#if HAS_CLIP_CONTROL
		vtx.p.z = float(z) * exp_min32;
	#else
		vtx.p.z = (float(z) * exp_min32) * 2.0f - 1.0f;
	#endif

	vtx.p.w = 1.0f;

	#if !(VS_ROUND_UV || VS_CLAMP_UV || VS_ALIGN_UV)
		vec2 uv = vec2(i_uv) - TextureOffset;
	#else
		vec2 uv = i_st - TextureOffset;
	#endif
	vec2 st = i_st - TextureOffset;

	vtx.t_float.xy = st;
	vtx.t_float.w  = i_q;

	vtx.t_int.xy = uv * TextureScale;
#if VS_FST
	vtx.t_int.zw = uv;
#else
	vtx.t_int.zw = st / TextureScale;
#endif

// Get UV rounding info saved in Q.
#if VS_ROUND_UV || VS_CLAMP_UV || VS_ALIGN_UV
	vtx.rounduv = extract_round_uv_bits(i_q);
	vtx.t_float.w = 1.0f;
#endif

	vtx.c = i_c;
	vtx.t_float.z = i_f.x;

	return vtx;
}

// Convert XY from NDC to GS pixel coordinates (i.e. 1.0 = 1 GS pixel).
vec2 get_xy_unscaled(vec2 xy)
{
	return round(xy / VertexScale) / 16.0f;
}

// Get the XY deltas in GS pixel coordinates, using first vertex as the origin.
mat2 get_xy_deltas_unscaled(ProcessedVertex v0, ProcessedVertex v1, ProcessedVertex v2)
{
	vec2 xy0 = get_xy_unscaled(v0.p.xy);
	vec2 xy1 = get_xy_unscaled(v1.p.xy);
	vec2 xy2 = get_xy_unscaled(v2.p.xy);
	return mat2(xy1 - xy0, xy2 - xy0);
}

// Get the AA1 outward expand direction to the edge formed by the first two vertices.
// This is up or down for shallow (X dominant) edges, and right or left for steep (Y dominant) edges.
// Similar expansion to line AA1 except instead of expanding on both sides of the line,
// expand on on the side towards the outside of the triangle.
vec2 get_aa1_triangle_expand_dir(ProcessedVertex v0, ProcessedVertex v1, ProcessedVertex v2)
{
	mat2 xy_deltas = get_xy_deltas_unscaled(v0, v1, v2);
	vec2 line_delta = xy_deltas[0];
	vec2 line_opposite = xy_deltas[1];

	vec2 line_normal = vec2(line_delta.y, -line_delta.x);
	vec2 line_expand = abs(line_delta.x) >= abs(line_delta.y) ? vec2(0.0f, 1.0f) : vec2(1.0f, 0.0f);

	if ((dot(line_expand, line_normal) >= 0.0f) == (dot(line_opposite, line_normal) >= 0.0f))
	{
		// Expand direction point towards the interior so flip it.
		line_expand = -line_expand;
	}

	return line_expand;
}

mat2 get_inverse(mat2 mat, float det)
{
	return mat2(mat[1][1], -mat[0][1], -mat[1][0], mat[0][0]) * (1 / det);
}

// Extrapolate triangle attributes from the first vertex along the given direction.
// dp_mat is derived from the input vertices, it is passed in to avoid recomputing.
void extrapolate_aa1_triangle_edge(inout ProcessedVertex v0, ProcessedVertex v1, ProcessedVertex v2, mat2 dp_mat, vec2 dp)
{
	// Get texture deltas
	#if VS_TME
		#if VS_FST
			mat2 dt = mat2(v1.t_int.zw - v0.t_int.zw, v2.t_int.zw - v0.t_int.zw);
		#else
			mat2 dt = mat2(v1.t_float.xy - v0.t_float.xy, v2.t_float.xy - v0.t_float.xy);
		#endif
	#endif

	// Get color delta if interpolating
	#if VS_IIP
		mat2x4 dc = mat2x4(v1.c - v0.c, v2.c - v0.c);
	#endif

	vec2 dz = vec2(v1.p.z - v0.p.z, v2.p.z - v0.p.z); // Z deltas

	vec2 df = vec2(v1.t_float.z - v0.t_float.z, v2.t_float.z - v0.t_float.z); // Fog deltas

	vec2 dq = vec2(v1.t_float.w - v0.t_float.w, v2.t_float.w - v0.t_float.w); // Q deltas

	// To prevent unstable extrapolation, do not extrapolate if the
	// minimum perpendicular length of the triangle is < 2 pixels.
	float dp_det = determinant(dp_mat); // Twice signed triangle area.
	float len0 = length(dp_mat[0]);
	float len1 = length(dp_mat[1]);
	float len2 = length(dp_mat[1] - dp_mat[0]);
	float min_perp_length = abs(dp_det) / max(max(len0, len1), len2);

	// Get the position -> barycentric weight matrix
	mat2 inv_dp_mat = get_inverse(dp_mat, dp_det);

	vec2 weights = min_perp_length < 2 ? vec2(0) : inv_dp_mat * dp;

	v0.p.xy += dp * PointSize; // Extrapolate position

	// Extrapolate texture coords
	#if VS_TME
		#if VS_FST
			v0.t_int.zw += dt * weights;
			v0.t_int.xy = v0.t_int.zw * TextureScale;
		#else
			v0.t_float.xy += dt * weights;
			v0.t_int.zw = v0.t_float.xy / TextureScale;
			v0.t_float.w += dot(dq, weights);
		#endif
	#endif

	// Extrapolate and clamp color
	#if VS_IIP
		v0.c += dc * weights;
		v0.c = clamp(v0.c, vec4(0), vec4(255));
	#endif

	v0.p.z += dot(dz, weights); // Extrapolate depth

	v0.t_float.z += dot(df, weights); // Extrapolate fog
}

void main()
{
	ProcessedVertex vtx;

	uint vid = uint(gl_VertexID);

	vec4 clampuv = vec4(0.0f);
	vec4 scaleuv = vec4(0.0f);

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
	vec2 line_vector = normalize(line_delta / VertexScale);
	vec2 line_expand = vec2(line_vector.y, -line_vector.x);
#if VS_EXPAND == VS_EXPAND_LINE_AA1
	line_expand *= 2.0f * LineAA1Width;
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

	#if VS_CLAMP_UV || VS_ALIGN_UV || VS_ROUND_UV
		vec4 pos = vec4(lt.window_pos, rb.window_pos);
		vec4 tex = vec4(lt.t_int.zw, rb.t_int.zw);

		#if VS_ROUND_UV
			vec4 d_tex = tex.zwzw - tex.xyxy;
			vec4 d_pos = pos.zwzw - pos.xyxy;
			scaleuv = d_tex / d_pos;
		#endif
	
		#if VS_CLAMP_UV
			clampuv = sprite_clamp_uv_range(pos, tex, lt.rounduv);
		#endif

		#if VS_ALIGN_UV == VS_ALIGN_UV_ALIGN
			sprite_align_and_round(pos, tex);
			lt.p.xy = window_coords_to_ndc(pos.xy);
			rb.p.xy = window_coords_to_ndc(pos.zw);

			lt.t_int.zw = tex.xy;
			lt.t_int.xy = lt.t_int.zw * TextureScale;
			rb.t_int.zw = tex.zw;
			rb.t_int.xy = rb.t_int.zw * TextureScale;
		#endif
	#endif

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
	// - Vertices 21-26: First corner cap (2 triangles).
	// - Vertices 27-32: Second corner cap (2 triangles).
	// - Vertices 33-38: Third corner cap (2 triangles).

	uint prim_id = vid / 39;
	uint prim_offset = vid - 39 * prim_id; // range: 0-38
	bool interior = prim_offset < 3;
	bool edge = 3 <= prim_offset && prim_offset < 21;

	if (interior)
	{
		vtx = load_vertex(load_index(3 * prim_id + prim_offset));
		VSout.inv_cov = 0.0f; // Full coverage
		VSout.interior = 1;
	}
	else if (edge)
	{
		// Vertex indices for this edge. We need all 3 for determining exterior/interior.
		uint prim_offset_edges = prim_offset - 3; // range: 0-17
		uint i0 = prim_offset_edges / 6;
		uint i1 = (i0 >= 2) ? i0 - 2 : i0 + 1;
		uint i2 = (i0 >= 1) ? i0 - 1 : i0 + 2;
		uint edge_offset = prim_offset_edges - 6 * i0; // range: 0-5

		// Note: order of top/bottom, inside/outside is arbitrary,
		// as long as it assembles into two triangles forming a quad.
		bool is_bottom = (2 <= edge_offset) && (edge_offset <= 4);
		bool is_outside = (edge_offset & 1u) != 0;

		vtx = load_vertex(load_index(3 * prim_id + (is_bottom ? i1 : i0)));
		ProcessedVertex other = load_vertex(load_index(3 * prim_id + (is_bottom ? i0 : i1)));
		ProcessedVertex opposite = load_vertex(load_index(3 * prim_id + i2));

		mat2 pos_deltas = get_xy_deltas_unscaled(vtx, other, opposite);

		vec2 expand_dir = is_outside ? get_aa1_triangle_expand_dir(vtx, other, opposite) : vec2(0);

		// Do actual extrapolation, or no-op if expand_dir == 0.
		extrapolate_aa1_triangle_edge(vtx, other, opposite, pos_deltas, expand_dir);

		VSout.inv_cov = is_outside ? 1.0f : 0.0f; // No coverage on outside, otherwise full.

		VSout.interior = 0;
	}
	else // Corner cap
	{
		// Vertex indices for this cap. We need all 3 for determining exterior/interior.
		uint prim_offset_cap = prim_offset - 21; // range: 0-8
		uint i0 = prim_offset_cap / 6;
		uint i1 = (i0 >= 2) ? i0 - 2 : i0 + 1;
		uint i2 = (i0 >= 1) ? i0 - 1 : i0 + 2;
		uint cap_offset = prim_offset_cap - 6 * i0; // range: 0-5

		bool is_near_corner = cap_offset == 0 || cap_offset == 3;
		bool is_far_corner = cap_offset == 2 || cap_offset == 5;
		bool is_first_tri = cap_offset < 3;

		// First triangle is on the side of vertex i1 and second is on the side of vertex i2.
		vtx = load_vertex(load_index(3 * prim_id + i0));
		ProcessedVertex other = load_vertex(load_index(3 * prim_id + (is_first_tri ? i1 : i2)));
		ProcessedVertex opposite = load_vertex(load_index(3 * prim_id + (is_first_tri ? i2 : i1)));

		mat2 pos_deltas = get_xy_deltas_unscaled(vtx, other, opposite);

		// Get the edge expansion directions of both incident edges.
		vec2 edge_expand_dir_0 = get_aa1_triangle_expand_dir(vtx, other, opposite);
		vec2 edge_expand_dir_1 = get_aa1_triangle_expand_dir(vtx, opposite, other);

		// Check if the corner is already filled by the expanded edges.
		// This happens if the expand directions are the same.
		// If so we output a degenerate triangle at this corner.
		bool corner_filled = all(equal(edge_expand_dir_0, edge_expand_dir_1));

		// Nothing if corner is filled, otherwise opposite to the bisector of the corner angle.
		vec2 far_corner_dir = corner_filled ? vec2(0) : -normalize((pos_deltas[0] + pos_deltas[1]) / 2);

		// Determine the expand direction.
		vec2 expand_dir = is_near_corner ? vec2(0) :       // No extrapolation
		                  is_far_corner ? far_corner_dir : // Opposite to the angle bisector of corner
		                  edge_expand_dir_0;               // Standard AA1 edge expansion

		// Do the actual extrapolation (no-op if expand_dir == 0).
		extrapolate_aa1_triangle_edge(vtx, other, opposite, pos_deltas, expand_dir);

		VSout.inv_cov = is_near_corner ? 0.0f : 1.0f; // Full coverage at near corner, otherwise none.
	
		VSout.interior = 0;
	}

#elif VS_EXPAND == VS_EXPAND_TRIANGLE // Triangle
	
	uint vid_0 = 3 * (vid / 3);
	uint vid_1 = vid_0 + 1;
	uint vid_2 = vid_0 + 2;

	ProcessedVertex v0 = load_vertex(vid_0);
	ProcessedVertex v1 = load_vertex(vid_1);
	ProcessedVertex v2 = load_vertex(vid_2);

	#if VS_CLAMP_UV || VS_ALIGN_UV || VS_ROUND_UV
		vec4 pos = vec4(v0.window_pos, v1.window_pos.x, v2.window_pos.y);
		vec4 tex = vec4(v0.t_int.zw, v1.t_int.z, v2.t_int.w);

		#if VS_ROUND_UV
			vec4 d_tex = tex.zwzw - tex.xyxy;
			vec4 d_pos = pos.zwzw - pos.xyxy;
			scaleuv = d_tex / d_pos;
		#endif

		#if VS_CLAMP_UV
			clampuv = sprite_clamp_uv_range(pos, tex, v0.rounduv);
		#endif

		#if VS_ALIGN_UV == VS_ALIGN_UV_ALIGN
			sprite_align_and_round(pos, tex);
			v0.p.xy = window_coords_to_ndc(pos.xy);
			v1.p.xy = window_coords_to_ndc(pos.zy);
			v2.p.xy = window_coords_to_ndc(pos.xw);

			v0.t_int.zw = tex.xy;
			v1.t_int.zw = tex.zy;
			v2.t_int.zw = tex.xw;

			// Swapping for rotated textures.
			vec2 texscale = mix(TextureScale, TextureScale.yx, bvec2(v0.rounduv.zw & ROUND_UV_SWAP));
			v0.t_int.xy = v0.t_int.zw * texscale;
			v1.t_int.xy = v1.t_int.zw * texscale;
			v2.t_int.xy = v2.t_int.zw * texscale;
		#endif
	#endif

	uint vid_mod = vid - vid_0;

	if (vid_mod == 0)
	{
		vtx = v0;
	}
	else if (vid_mod == 1)
	{
		vtx = v1;
	}
	else
	{
		vtx = v2;
	}

#endif

	gl_Position = vtx.p;
	VSout.t_float = vtx.t_float;
	VSout.t_int = vtx.t_int;
	VSout.c = vtx.c;
#if VS_ROUND_UV || VS_CLAMP_UV || VS_ALIGN_UV
	VSout.rounduv = vtx.rounduv;
#endif
#if VS_ROUND_UV
	VSout.scaleuv = scaleuv;
#endif
#if VS_CLAMP_UV
	VSout.clampuv = clampuv;
#endif
}

#endif // VS_EXPAND

#endif // VERTEX_SHADER
