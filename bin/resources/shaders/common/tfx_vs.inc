#ifndef __METAL_VERSION__
// Non-Metal backends can only define buffers with u32 elements, but PCSX2 indices are u16.
uint load_index(VS_INDICES_PARAM(ignored), uint _i)
{
	#if VS_NEEDS_EXPAND
		uint i = _i + VS_BASE_INDEX;
		// i is even => load lower 16 bits; i odd => load upper 16 bits.
		uint shift = (i & 1u) << 4u;
		return (VS_LOAD_INDEX(ignored, i >> 1u) >> shift) & 0xFFFFu;
	#else
		// Don't reference the index buffer unless we really need to.
		return 0;
	#endif
}

VSInputGeneric load_vertex(VS_VERTICES_PARAM(vertices), uint index)
{
	VSInputGeneric vert;
	#if VS_NEEDS_EXPAND
		VSRawVertex raw = VS_LOAD_VERTEX(vertices, VS_BASE_VERTEX + index);
		vert.st = raw.ST;
		vert.c = uint4(raw.RGBA & 0xFFu, (raw.RGBA >> 8) & 0xFFu, (raw.RGBA >> 16) & 0xFFu, raw.RGBA >> 24);
		vert.q = raw.Q;
		vert.p = uint2(raw.XY & 0xFFFFu, raw.XY >> 16);
		vert.z = raw.Z;
		vert.uv = uint2(raw.UV & 0xFFFFu, raw.UV >> 16);
		vert.f = float4_bcast(float(raw.FOG) / 255.0f);
	#else
		// Don't reference the vertex buffer unless we really need to.
		vert.st = float2_bcast(0.0f);
		vert.c = uint4_bcast(0);
		vert.q = 0.0f;
		vert.p = uint2_bcast(0);
		vert.z = 0;
		vert.uv = uint2_bcast(0);
		vert.f = float4_bcast(0.0f);
	#endif
	return vert;
}
#endif

static void vs_broken_shader_depth(IN_OUT_PARAM(VSOutputGeneric, vout))
{
	// Apple GPUs use slightly different algorithms to calculate the Z they send to the shader vs the Z they use in hardware.
	// This breaks a lot of things (the most common is conservative depth rejecting pixels that should have depth equal to current depth but now don't).
	// Work around by always routing depth through the shader, and never using hardware depth values.
	// To allow us to continue to use [[depth(less)]] optimizations, add a bit in the VS and subtract it off in the FS,
	// so that "equal" depth doesn't ever fail a hardware depth test.
	vout.p.z += EXP2_NEG_32;
}

static VSOutputGeneric vs_main_impl(IN_PARAM(VSInputGeneric, v), IN_PARAM(VSUniformsGeneric, cb))
{
	VSOutputGeneric vout;
	// Clamp to max depth, GS doesn't wrap
	uint z = min(v.z, cb.max_depth_vs);
	vout.p.xy = float2(v.p) - float2(0.05f, 0.05f);
	vout.p.xy = vout.p.xy * cb.vertex_scale - cb.vertex_offset;
	vout.p.y *= VS_Y_FLIP;
	vout.p.w = 1.0f;
	vout.p.z = VS_SCALE_RAW_Z(z);

	float2 uv = float2(v.uv) - cb.texture_offset;
	float2 st = v.st - cb.texture_offset;

	// Float coordinate
	vout.t.xy = st;
	vout.t.w = v.q;

	// Integer coordinate => normalized
	vout.ti.xy = uv * cb.texture_scale;

	if (VS_FST != FALSE)
	{
		// Integer coordinate => integral
		vout.ti.zw = uv;
	}
	else
	{
		// Some games uses float coordinate for post-processing effects
		vout.ti.zw = st / cb.texture_scale;
	}

	vout.c = v.c;

	vout.t.z = v.f.x; // pack fog with texture

	if (VS_POINT_SIZE != FALSE)
		vout.point_size = cb.point_size.x;
	else
		vout.point_size = 0.0f;

	if (BROKEN_SHADER_DEPTH != FALSE)
		vs_broken_shader_depth(vout);

	vout.inv_cov = 0.0f;
	vout.interior = 0;

	return vout;
}

// Convert XY from NDC to GS pixel coordinates (i.e. 1.0 = 1 GS pixel).
static float2 get_xy_unscaled(float2 xy, IN_PARAM(VSUniformsGeneric, cb))
{
	return round(xy / cb.vertex_scale) / 16.0f;
}

// Get the XY deltas in GS pixel coordinates, using first vertex as the origin.
static float2x2 get_xy_deltas_unscaled(IN_PARAM(VSOutputGeneric, v0), IN_PARAM(VSOutputGeneric, v1), IN_PARAM(VSOutputGeneric, v2), IN_PARAM(VSUniformsGeneric, cb))
{
	float2 xy0 = get_xy_unscaled(v0.p.xy, cb);
	float2 xy1 = get_xy_unscaled(v1.p.xy, cb);
	float2 xy2 = get_xy_unscaled(v2.p.xy, cb);
	return float2x2(xy1 - xy0, xy2 - xy0);
}

// Get the AA1 outward expand direction to the edge formed by the first two vertices.
// This is up or down for shallow (X dominant) edges, and right or left for steep (Y dominant) edges.
// Similar expansion to line AA1 except instead of expanding on both sides of the line,
// expand on on the side towards the outside of the triangle.
static float2 get_aa1_triangle_expand_dir(IN_PARAM(VSOutputGeneric, v0), IN_PARAM(VSOutputGeneric, v1), IN_PARAM(VSOutputGeneric, v2), IN_PARAM(VSUniformsGeneric, cb))
{
	float2x2 xy_deltas = get_xy_deltas_unscaled(v0, v1, v2, cb);
	float2 line_delta = xy_deltas[0];
	float2 line_opposite = xy_deltas[1];

	float2 line_normal = float2(line_delta.y, -line_delta.x);
	float2 line_expand = abs(line_delta.x) >= abs(line_delta.y) ? float2(0.0f, 1.0f) : float2(1.0f, 0.0f);

	if ((dot(line_expand, line_normal) >= 0.0f) == (dot(line_opposite, line_normal) >= 0.0f))
	{
		// Expand direction point towards the interior so flip it.
		line_expand = -line_expand;
	}

	return line_expand;
}

// This works for GLSL, HLSL, MSL in spite of different row/column ordering.
static float2x2 get_inverse(IN_PARAM(float2x2, mat), float det)
{
	return float2x2(mat[1][1], -mat[0][1], -mat[1][0], mat[0][0]) * (1 / det);
}

// Extrapolate triangle attributes from the first vertex along the given direction.
// dp_mat is derived from the input vertices, it is passed in to avoid recomputing.
static void extrapolate_aa1_triangle_edge(IN_OUT_PARAM(VSOutputGeneric, v0), IN_PARAM(VSOutputGeneric, v1), IN_PARAM(VSOutputGeneric, v2),
	IN_PARAM(float2x2, dp_mat), float2 dp, IN_PARAM(VSUniformsGeneric, cb))
{
	// Get texture deltas
	float2x2 dt;
	if (VS_FST != FALSE)
	{
		dt = float2x2(v1.ti.zw - v0.ti.zw, v2.ti.zw - v0.ti.zw);
	}
	else
	{
		dt = float2x2(v1.t.xy - v0.t.xy, v2.t.xy - v0.t.xy);
	}

	// Get color delta if interpolating
	float2x4 dc;
	if (VS_IIP != FALSE)
	{
		dc = float2x4(v1.c - v0.c, v2.c - v0.c);
	}

	float2 dz = float2(v1.p.z - v0.p.z, v2.p.z - v0.p.z); // Z deltas

	float2 df = float2(v1.t.z - v0.t.z, v2.t.z - v0.t.z); // Fog deltas

	float2 dq = float2(v1.t.w - v0.t.w, v2.t.w - v0.t.w); // Q deltas

	// To prevent unstable extrapolation, do not extrapolate if the
	// minimum perpendicular length of the triangle is < 2 pixels.
	float dp_det = determinant(dp_mat); // Twice signed triangle area.
	float len0 = length(dp_mat[0]);
	float len1 = length(dp_mat[1]);
	float len2 = length(dp_mat[1] - dp_mat[0]);
	float min_perp_length = abs(dp_det) / max(max(len0, len1), len2);

	// Get the position -> barycentric weight matrix
	float2x2 inv_dp_mat = get_inverse(dp_mat, dp_det);

	float2 weights = min_perp_length < 2 ? float2_bcast(0.0f) : MAT_MUL(inv_dp_mat, dp);

	v0.p.xy += dp * cb.point_size; // Extrapolate position

	// Extrapolate texture coords
	if (VS_FST != FALSE)
	{
			v0.ti.zw += MAT_MUL(dt, weights);
			v0.ti.xy = v0.ti.zw * cb.texture_scale;
	}
	else
	{
		v0.t.xy += MAT_MUL(dt, weights);
		v0.ti.zw = v0.t.xy / cb.texture_scale;
		v0.t.w += dot(dq, weights);
	}

	// Extrapolate and clamp color
	if (VS_IIP != FALSE)
	{
		v0.c += MAT_MUL(dc, weights);
		v0.c = clamp(v0.c, 0, 255);
	}

	v0.p.z += dot(dz, weights); // Extrapolate depth

	v0.t.z += dot(df, weights); // Extrapolate fog
}

static VSOutputGeneric vs_expand_none_impl(uint vid, VS_VERTICES_PARAM(vertices), IN_PARAM(VSUniformsGeneric, cb))
{
	return vs_main_impl(load_vertex(vertices, vid), cb);
}

static VSOutputGeneric vs_expand_point_impl(uint vid, VS_VERTICES_PARAM(vertices), IN_PARAM(VSUniformsGeneric, cb))
{
	VSOutputGeneric vout = vs_main_impl(load_vertex(vertices, vid), cb);
	if ((vid & 1u) != 0u)
		vout.p.x += cb.point_size.x;
	if ((vid & 2u) != 0u)
		vout.p.y += cb.point_size.y;
	return vout;
}

static VSOutputGeneric vs_expand_line_impl(uint vid, VS_VERTICES_PARAM(vertices), IN_PARAM(VSUniformsGeneric, cb))
{
	uint vid_base = vid >> 2;
	bool is_bottom = (vid & 2u) != 0u;
	bool is_right = (vid & 1u) != 0u;
	uint vid_other = is_bottom ? vid_base - 1 : vid_base + 1;
	VSOutputGeneric vout = vs_main_impl(load_vertex(vertices, vid_base), cb);
	VSOutputGeneric other = vs_main_impl(load_vertex(vertices, vid_other), cb);

	// Use bottom minus top for delta regardless of which vertex we are expanding.
	float2 line_delta = is_bottom ? vout.p.xy - other.p.xy : other.p.xy - vout.p.xy;
	float2 line_vector = normalize(line_delta / cb.vertex_scale);
	float2 line_expand = float2(line_vector.y, -line_vector.x);

	if (VS_EXPAND_TYPE == VS_EXPAND_LINE_AA1)
		line_expand *= 2.f * cb.line_aa1_width;

	float2 line_width = (line_expand * cb.point_size) / 2;
	float2 offset = is_right ? line_width : -line_width;
	vout.p.xy += offset;

	if (VS_EXPAND_TYPE == VS_EXPAND_LINE_AA1)
		vout.inv_cov = is_right ? 1.f : -1.f;

	// Lines will be run as (0 1 2) (1 2 3)
	// This means that both triangles will have a point based off the top line point as their first point
	// So we don't have to do anything for !IIP

	return vout;
}

static VSOutputGeneric vs_expand_sprite_impl(uint vid, VS_VERTICES_PARAM(vertices), IN_PARAM(VSUniformsGeneric, cb))
{
	uint vid_base = vid >> 1;
	bool is_bottom = (vid & 2u) != 0u;
	bool is_right = (vid & 1u) != 0u;
	// Sprite points are always in pairs
	uint vid_lt = vid_base & ~1u;
	uint vid_rb = vid_base | 1u;

	VSOutputGeneric lt = vs_main_impl(load_vertex(vertices, vid_lt), cb);
	VSOutputGeneric rb = vs_main_impl(load_vertex(vertices, vid_rb), cb);
	VSOutputGeneric vout = rb;

	if (!is_right)
	{
		vout.p.x = lt.p.x;
		vout.t.x = lt.t.x;
		vout.ti.xz = lt.ti.xz;
	}

	if (!is_bottom)
	{
		vout.p.y = lt.p.y;
		vout.t.y = lt.t.y;
		vout.ti.yw = lt.ti.yw;
	}

	return vout;
}

static VSOutputGeneric vs_expand_triangle_aa1_impl(uint vid, VS_VERTICES_PARAM(vertices), IN_PARAM(VSUniformsGeneric, cb),
	VS_INDICES_PARAM(indices))
{
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

	VSOutputGeneric vout;
	if (interior)
	{
		vout = vs_main_impl(load_vertex(vertices, load_index(indices, 3 * prim_id + prim_offset)), cb);
		vout.inv_cov = 0.f;
		vout.interior = 1;
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
		bool is_outside = (edge_offset & 1u) != 0u;

		vout                     = vs_main_impl(load_vertex(vertices, load_index(indices, 3 * prim_id + (is_bottom ? i1 : i0))), cb);
		VSOutputGeneric other    = vs_main_impl(load_vertex(vertices, load_index(indices, 3 * prim_id + (is_bottom ? i0 : i1))), cb);
		VSOutputGeneric opposite = vs_main_impl(load_vertex(vertices, load_index(indices, 3 * prim_id + i2)), cb);

		float2x2 pos_deltas = get_xy_deltas_unscaled(vout, other, opposite, cb);

		float2 expand_dir = is_outside ? get_aa1_triangle_expand_dir(vout, other, opposite, cb) : float2_bcast(0.0f);

		// Do actual extrapolation, or no-op if expand_dir == 0.
		extrapolate_aa1_triangle_edge(vout, other, opposite, pos_deltas, expand_dir, cb);

		vout.inv_cov = is_outside ? 1.0f : 0.0f; // No coverage on outside, otherwise full.

		vout.interior = 0;
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

		vout                     = vs_main_impl(load_vertex(vertices, load_index(indices, 3 * prim_id + i0)), cb);
		VSOutputGeneric other    = vs_main_impl(load_vertex(vertices, load_index(indices, 3 * prim_id + (is_first_tri ? i1 : i2))), cb);
		VSOutputGeneric opposite = vs_main_impl(load_vertex(vertices, load_index(indices, 3 * prim_id + (is_first_tri ? i2 : i1))), cb);

		float2x2 pos_deltas = get_xy_deltas_unscaled(vout, other, opposite, cb);

		// Get the edge expansion directions of both incident edges.
		float2 edge_expand_dir_0 = get_aa1_triangle_expand_dir(vout, other, opposite, cb);
		float2 edge_expand_dir_1 = get_aa1_triangle_expand_dir(vout, opposite, other, cb);

		// Check if the corner is already filled by the expanded edges.
		// This happens if the expand directions are the same.
		// If so we output a degenerate triangle at this corner.
		bool corner_filled = all(equal(edge_expand_dir_0, edge_expand_dir_1));

		// Nothing if corner is filled, otherwise opposite to the bisector of the corner angle.
		float2 far_corner_dir = corner_filled ? float2_bcast(0.0f) : -normalize((pos_deltas[0] + pos_deltas[1]) / 2);

		// Determine the expand direction.
		float2 expand_dir = is_near_corner ? float2_bcast(0.0f) : // No extrapolation
												is_far_corner ? far_corner_dir :      // Opposite to the angle bisector of corner
												edge_expand_dir_0;                    // Standard AA1 edge expansion

		// Do the actual extrapolation (no-op if expand_dir == 0).
		extrapolate_aa1_triangle_edge(vout, other, opposite, pos_deltas, expand_dir, cb);

		vout.inv_cov = is_near_corner ? 0.0f : 1.0f; // Full coverage at near corner, otherwise none.
	
		vout.interior = 0;
	}

	return vout;
}

static VSOutputGeneric vs_expand_impl(uint vid, VS_VERTICES_PARAM(vertices), IN_PARAM(VSUniformsGeneric, cb), VS_INDICES_PARAM(indices))
{
	switch (VS_EXPAND_TYPE)
	{
		case VS_EXPAND_NONE:
			return vs_expand_none_impl(vid, vertices, cb);
		case VS_EXPAND_POINT:
			return vs_expand_point_impl(vid, vertices, cb);
		case VS_EXPAND_LINE:
		case VS_EXPAND_LINE_AA1:
			return vs_expand_line_impl(vid, vertices, cb);
		case VS_EXPAND_SPRITE:
			return vs_expand_sprite_impl(vid, vertices, cb);
		case VS_EXPAND_TRIANGLE_AA1:
			return vs_expand_triangle_aa1_impl(vid, vertices, cb, indices);
	}
}
