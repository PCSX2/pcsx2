// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "tfx_defines.hlsl"

#ifndef VS_TME
#define VS_IIP 0
#define VS_TME 1
#define VS_FST 1
#endif

#ifndef GS_IIP
#define GS_IIP 0
#define GS_PRIM 3
#define GS_FORWARD_PRIMID 0
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

#ifndef PS_FST
#define PS_IIP 0
#define PS_FST 0
#define PS_WMS 0
#define PS_WMT 0
#define PS_ADJS 0
#define PS_ADJT 0
#define PS_AEM_FMT FMT_32
#define PS_AEM 0
#define PS_TFX 0
#define PS_TCC 1
#define PS_ATST 1
#define PS_FOG 0
#define PS_IIP 0
#define PS_BLEND_HW 0
#define PS_A_MASKED 0
#define PS_FBA 0
#define PS_FBMASK 0
#define PS_LTF 1
#define PS_TCOFFSETHACK 0
#define PS_POINT_SAMPLER 0
#define PS_REGION_RECT 0
#define PS_SHUFFLE 0
#define PS_SHUFFLE_SAME 0
#define PS_PROCESS_BA 0
#define PS_PROCESS_RG 0
#define PS_SHUFFLE_ACROSS 0
#define PS_READ16_SRC 0
#define PS_WRITE_RG 0
#define PS_DST_FMT 0
#define PS_DEPTH_FMT 0
#define PS_PAL_FMT 0
#define PS_CHANNEL_FETCH 0
#define PS_TALES_OF_ABYSS_HLE 0
#define PS_URBAN_CHAOS_HLE 0
#define PS_COLCLIP_HW 0
#define PS_RTA_CORRECTION 0
#define PS_RTA_SRC_CORRECTION 0
#define PS_COLCLIP 0
#define PS_BLEND_A 0
#define PS_BLEND_B 0
#define PS_BLEND_C 0
#define PS_BLEND_D 0
#define PS_BLEND_MIX 0
#define PS_ROUND_INV 0
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
#define PS_TEX_IS_FB 0
#define PS_AA1 0
#define PS_ABE 0
#define PS_ROV_COLOR 0
#define PS_ROV_DEPTH 0
#endif

#ifndef VS_EXPAND_NONE
#define VS_EXPAND_NONE 0
#define VS_EXPAND_POINT 1
#define VS_EXPAND_LINE 2
#define VS_EXPAND_SPRITE 3
#define VS_EXPAND_LINE_AA1 4
#define VS_EXPAND_TRIANGLE_AA1 5
#endif

#define SW_BLEND (PS_BLEND_A || PS_BLEND_B || PS_BLEND_D)
#define SW_BLEND_NEEDS_RT (SW_BLEND && (PS_BLEND_A == 1 || PS_BLEND_B == 1 || PS_BLEND_C == 1 || PS_BLEND_D == 1))
#define SW_AD_TO_HW (PS_BLEND_C == 1 && PS_A_MASKED)
#define NEEDS_RT_FOR_AFAIL (PS_AFAIL == AFAIL_ZB_ONLY || PS_AFAIL == AFAIL_RGB_ONLY || PS_AFAIL == AFAIL_RGB_ONLY_SW_Z)
#define NEEDS_DEPTH_FOR_AFAIL (PS_AFAIL == AFAIL_FB_ONLY || PS_AFAIL == AFAIL_RGB_ONLY_SW_Z)
#define NEEDS_DEPTH_FOR_ZTST (PS_ZTST == ZTST_GEQUAL || PS_ZTST == ZTST_GREATER)
#define NEEDS_DEPTH_FOR_AA1 (PS_AA1 == PS_AA1_TRIANGLE_SW_Z)
#define SW_DEPTH (NEEDS_DEPTH_FOR_AFAIL || NEEDS_DEPTH_FOR_ZTST || NEEDS_DEPTH_FOR_AA1)
#define ZWRITE (PS_ZFLOOR || PS_ZCLAMP || SW_DEPTH)

#define PS_RETURN_COLOR_ROV (!PS_NO_COLOR && PS_ROV_COLOR)
#define PS_RETURN_COLOR (!PS_NO_COLOR && !PS_ROV_COLOR)
#define PS_RETURN_DEPTH_ROV (PS_ROV_DEPTH == PS_ROV_DEPTH_READ_WRITE)
#define PS_RETURN_DEPTH (ZWRITE && !PS_ROV_DEPTH)
#define PS_ROV_EARLYDEPTHSTENCIL (PS_ROV_COLOR && !PS_ROV_DEPTH && !ZWRITE)

struct VS_INPUT
{
	float2 st : TEXCOORD0;
	uint4 c : COLOR0;
	float q : TEXCOORD1;
	uint2 p : POSITION0;
	uint z : POSITION1;
	uint2 uv : TEXCOORD2;
	float4 f : COLOR1;
};

struct VS_OUTPUT
{
	float4 p : SV_Position;
	float4 t : TEXCOORD0;
	float4 ti : TEXCOORD2;

#if VS_IIP != 0 || GS_IIP != 0 || PS_IIP != 0
	float4 c : COLOR0;
#else
	nointerpolation float4 c : COLOR0;
#endif

	float inv_cov : COLOR1; // We use the inverse to make it simpler to interpolate.
	nointerpolation uint interior : COLOR2; // 1 for triangle interior; 0 for edge;
};

#ifdef PIXEL_SHADER

#define MONOLITHIC 1

#include "tfx_ps_resources.hlsl"

#include "tfx_ps_rt.hlsl"
#include "tfx_ps_rta_correction.hlsl"
#include "tfx_ps_sample_af.hlsl"
#include "tfx_ps_sample.hlsl"
#include "tfx_ps_fetch.hlsl"
#include "tfx_ps_tfx.hlsl"
#include "tfx_ps_atst.hlsl"
#include "tfx_ps_fog.hlsl"
#include "tfx_ps_color.hlsl"
#include "tfx_ps_misc.hlsl"
#include "tfx_ps_blend.hlsl"
#include "tfx_ps_main.hlsl"
#endif // PIXEL_SHADER

//////////////////////////////////////////////////////////////////////
// Vertex Shader
//////////////////////////////////////////////////////////////////////

#ifdef VERTEX_SHADER

#ifdef DX12
cbuffer cb0 : register(b0)
#else
cbuffer cb0
#endif
{
	float2 VertexScale;
	float2 VertexOffset;
	float2 TextureScale;
	float2 TextureOffset;
	float2 PointSize;
	uint MaxDepth;
	float LineAA1Width;
};

#ifdef DX12
cbuffer cb2 : register(b2)
#else
cbuffer cb2
#endif
{
	uint BaseVertex;
	uint BaseIndex;
	uint _cb2_pad0;
	uint _cb2_pad1;
};

VS_OUTPUT vs_main(VS_INPUT input)
{
	// Clamp to max depth, gs doesn't wrap
	input.z = min(input.z, MaxDepth);

	VS_OUTPUT output;

	// pos -= 0.05 (1/320 pixel) helps avoiding rounding problems (integral part of pos is usually 5 digits, 0.05 is about as low as we can go)
	// example: ceil(afterseveralvertextransformations(y = 133)) => 134 => line 133 stays empty
	// input granularity is 1/16 pixel, anything smaller than that won't step drawing up/left by one pixel
	// example: 133.0625 (133 + 1/16) should start from line 134, ceil(133.0625 - 0.05) still above 133

	output.p = float4(input.p, input.z, 1.0f) - float4(0.05f, 0.05f, 0, 0);

	output.p.xy = output.p.xy * float2(VertexScale.x, -VertexScale.y) - float2(VertexOffset.x, -VertexOffset.y);
	output.p.z *= exp2(-32.0f);		// integer->float depth

	if(VS_TME)
	{
		float2 uv = input.uv - TextureOffset;
		float2 st = input.st - TextureOffset;

		// Integer nomalized
		output.ti.xy = uv * TextureScale;

		if (VS_FST)
		{
			// Integer integral
			output.ti.zw = uv;
		}
		else
		{
			// float for post-processing in some games
			output.ti.zw = st / TextureScale;
		}
		// Float coords
		output.t.xy = st;
		output.t.w = input.q;
	}
	else
	{
		output.t.xy = 0;
		output.t.w = 1.0f;
		output.ti = 0;
	}

	output.c = input.c;
	output.t.z = input.f.r;

	// Silence compiler warnings; should be optimized out when not needed.
	output.inv_cov = 0.0f;
	output.interior = 0;

	return output;
}

#if VS_EXPAND != VS_EXPAND_NONE

struct VS_RAW_INPUT
{
	float2 ST;
	uint RGBA;
	float Q;
	uint XY;
	uint Z;
	uint UV;
	uint FOG;
};

StructuredBuffer<VS_RAW_INPUT> vertices : register(t0);
StructuredBuffer<uint> IndexBuffer : register(t5);

uint load_index(uint _i)
{
	uint i = _i + BaseIndex;
	// i is even => load lower 16 bits; i odd => load upper 16 bits.
	uint shift = (i & 1u) << 4u;
	return (IndexBuffer.Load(i >> 1u) >> shift) & 0xFFFFu;
}

VS_INPUT load_vertex(uint index)
{
	VS_RAW_INPUT raw = vertices.Load(BaseVertex + index);

	VS_INPUT vert;
	vert.st = raw.ST;
	vert.c = uint4(raw.RGBA & 0xFFu, (raw.RGBA >> 8) & 0xFFu, (raw.RGBA >> 16) & 0xFFu, raw.RGBA >> 24);
	vert.q = raw.Q;
	vert.p = uint2(raw.XY & 0xFFFFu, raw.XY >> 16);
	vert.z = raw.Z;
	vert.uv = uint2(raw.UV & 0xFFFFu, raw.UV >> 16);
	vert.f = float4(float(raw.FOG & 0xFFu), float((raw.FOG >> 8) & 0xFFu), float((raw.FOG >> 16) & 0xFFu), float(raw.FOG >> 24)) / 255.0f;
	return vert;
}

// Convert XY from NDC to GS pixel coordinates (i.e. 1.0 = 1 GS pixel).
float2 get_xy_unscaled(float2 xy)
{
	return round(xy / VertexScale) / 16.0f;
}

// Get the XY deltas in GS pixel coordinates, using first vertex as the origin.
float2x2 get_xy_deltas_unscaled(VS_OUTPUT v0, VS_OUTPUT v1, VS_OUTPUT v2)
{
	float2 xy0 = get_xy_unscaled(v0.p.xy);
	float2 xy1 = get_xy_unscaled(v1.p.xy);
	float2 xy2 = get_xy_unscaled(v2.p.xy);
	return float2x2(xy1 - xy0, xy2 - xy0);
}

// Get the AA1 outward expand direction to the edge formed by the first two vertices.
// This is up or down for shallow (X dominant) edges, and right or left for steep (Y dominant) edges.
// Similar expansion to line AA1 except instead of expanding on both sides of the line,
// expand on on the side towards the outside of the triangle.
float2 get_aa1_triangle_expand_dir(VS_OUTPUT v0, VS_OUTPUT v1, VS_OUTPUT v2)
{
	float2x2 xy_deltas = get_xy_deltas_unscaled(v0, v1, v2);
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

float2x2 get_inverse(float2x2 mat, float det)
{
	return float2x2(mat[1][1], -mat[0][1], -mat[1][0], mat[0][0]) * (1 / det);
}

// Extrapolate triangle attributes from the first vertex along the given direction.
// dp_mat is derived from the input vertices, it is passed in to avoid recomputing.
void extrapolate_aa1_triangle_edge(inout VS_OUTPUT v0, VS_OUTPUT v1, VS_OUTPUT v2, float2x2 dp_mat, float2 dp)
{
	// Get texture deltas
	#if VS_TME
		#if VS_FST
			float2x2 dt = float2x2(v1.ti.zw - v0.ti.zw, v2.ti.zw - v0.ti.zw);
		#else
			float2x2 dt = float2x2(v1.t.xy - v0.t.xy, v2.t.xy - v0.t.xy);
		#endif
	#endif

	// Get color delta if interpolating
	#if VS_IIP
		float2x4 dc = float2x4(v1.c - v0.c, v2.c - v0.c);
	#endif

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

	float2 weights = min_perp_length < 2 ? 0 : mul(dp, inv_dp_mat);

	v0.p.xy += dp * PointSize; // Extrapolate position

	// Extrapolate texture coords
	#if VS_TME
		#if VS_FST
			v0.ti.zw += mul(weights, dt);
			v0.ti.xy = v0.ti.zw * TextureScale;
		#else
			v0.t.xy += mul(weights, dt);
			v0.ti.zw = v0.t.xy / TextureScale;
			v0.t.w += dot(weights, dq);
		#endif
	#endif

	// Extrapolate and clamp color
	#if VS_IIP
		v0.c += mul(weights, dc);
		v0.c = clamp(v0.c, 0, 255);
	#endif

	v0.p.z += dot(weights, dz); // Extrapolate depth

	v0.t.z += dot(weights, df); // Extrapolate fog
}

VS_OUTPUT vs_main_expand(uint vid : SV_VertexID)
{
#if VS_EXPAND == VS_EXPAND_POINT

	VS_OUTPUT vtx = vs_main(load_vertex(vid >> 2));

	vtx.p.x += ((vid & 1u) != 0u) ? PointSize.x : 0.0f;
	vtx.p.y += ((vid & 2u) != 0u) ? PointSize.y : 0.0f;

	return vtx;

#elif (VS_EXPAND == VS_EXPAND_LINE) || (VS_EXPAND == VS_EXPAND_LINE_AA1)

	// The difference between EXPAND_LINE and EXPAND_LINE_AA1
	// is that EXPAND_LINE expands in the perpendicular direction while
	// EXPAND_LINE_AA1 expands in the Y direction for shallow lines (X dominant)
	// and the X direction for steep lines (Y dominant).
	// EXPAND_LINE_AA1 also adds coverage to the output.

	uint vid_base = vid >> 2;
	bool is_bottom = vid & 2;
	bool is_right = vid & 1;
	uint vid_other = is_bottom ? vid_base - 1 : vid_base + 1;
	VS_OUTPUT vtx = vs_main(load_vertex(vid_base));
	VS_OUTPUT other = vs_main(load_vertex(vid_other));

	// Use bottom minus top for delta regardless of which vertex we are expanding.
	float2 line_delta = is_bottom ? (vtx.p.xy - other.p.xy) : (other.p.xy - vtx.p.xy);
	float2 line_vector = normalize(line_delta / VertexScale);
	float2 line_expand = float2(line_vector.y, -line_vector.x);
#if VS_EXPAND == VS_EXPAND_LINE_AA1
	line_expand *= 2.0f * LineAA1Width;
#endif
	float2 line_width = (line_expand * PointSize) / 2;
	float2 offset = is_right ? line_width : -line_width;
	vtx.p.xy += offset;

#if VS_EXPAND == VS_EXPAND_LINE_AA1
	vtx.inv_cov = is_right ? 1.0f : -1.0f;
#endif

	// Lines will be run as (0 1 2) (1 2 3)
	// This means that both triangles will have a point based off the top line point as their first point
	// So we don't have to do anything for !IIP

	return vtx;

#elif VS_EXPAND == VS_EXPAND_SPRITE

	// Sprite points are always in pairs
	uint vid_base = vid >> 1;
	uint vid_lt = vid_base & ~1u;
	uint vid_rb = vid_base | 1u;

	VS_OUTPUT lt = vs_main(load_vertex(vid_lt));
	VS_OUTPUT rb = vs_main(load_vertex(vid_rb));
	VS_OUTPUT vtx = rb;

	bool is_right = ((vid & 1u) != 0u);
	vtx.p.x = is_right ? lt.p.x : vtx.p.x;
	vtx.t.x = is_right ? lt.t.x : vtx.t.x;
	vtx.ti.xz = is_right ? lt.ti.xz : vtx.ti.xz;

	bool is_bottom = ((vid & 2u) != 0u);
	vtx.p.y = is_bottom ? lt.p.y : vtx.p.y;
	vtx.t.y = is_bottom ? lt.t.y : vtx.t.y;
	vtx.ti.yw = is_bottom ? lt.ti.yw : vtx.ti.yw;

	return vtx;

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

	VS_OUTPUT vtx;
	if (interior)
	{
		vtx = vs_main(load_vertex(load_index(3 * prim_id + prim_offset)));
		vtx.inv_cov = 0.0f; // Full coverage
		vtx.interior = 1;
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
		bool is_outside = edge_offset & 1;

		vtx = vs_main(load_vertex(load_index(3 * prim_id + (is_bottom ? i1 : i0))));
		VS_OUTPUT other = vs_main(load_vertex(load_index(3 * prim_id + (is_bottom ? i0 : i1))));
		VS_OUTPUT opposite = vs_main(load_vertex(load_index(3 * prim_id + i2)));

		float2x2 pos_deltas = get_xy_deltas_unscaled(vtx, other, opposite);

		float2 expand_dir = is_outside ? get_aa1_triangle_expand_dir(vtx, other, opposite) : 0;

		// Do actual extrapolation, or no-op if expand_dir == 0.
		extrapolate_aa1_triangle_edge(vtx, other, opposite, pos_deltas, expand_dir);

		vtx.inv_cov = is_outside ? 1.0f : 0.0f; // No coverage on outside, otherwise full.

		vtx.interior = 0;
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

		vtx = vs_main(load_vertex(load_index(3 * prim_id + i0)));
		VS_OUTPUT other = vs_main(load_vertex(load_index(3 * prim_id + (is_first_tri ? i1 : i2))));
		VS_OUTPUT opposite = vs_main(load_vertex(load_index(3 * prim_id + (is_first_tri ? i2 : i1))));

		float2x2 pos_deltas = get_xy_deltas_unscaled(vtx, other, opposite);

		// Get the edge expansion directions of both incident edges.
		float2 edge_expand_dir_0 = get_aa1_triangle_expand_dir(vtx, other, opposite);
		float2 edge_expand_dir_1 = get_aa1_triangle_expand_dir(vtx, opposite, other);

		// Check if the corner is already filled by the expanded edges.
		// This happens if the expand directions are the same.
		// If so we output a degenerate triangle at this corner.
		bool corner_filled = all(edge_expand_dir_0 == edge_expand_dir_1);

		// Nothing if corner is filled, otherwise opposite to the bisector of the corner angle.
		float2 far_corner_dir = corner_filled ? 0 : -normalize((pos_deltas[0] + pos_deltas[1]) / 2);

		// Determine the expand direction.
		float2 expand_dir = is_near_corner ? 0 :             // No extrapolation
		                    is_far_corner ? far_corner_dir : // Opposite to the angle bisector of corner
		                    edge_expand_dir_0;               // Standard AA1 edge expansion

		// Do the actual extrapolation (no-op if expand_dir == 0).
		extrapolate_aa1_triangle_edge(vtx, other, opposite, pos_deltas, expand_dir);

		vtx.inv_cov = is_near_corner ? 0.0f : 1.0f; // Full coverage at near corner, otherwise none.
	
		vtx.interior = 0;

		#if !VS_IIP
			// Get the provoking vertex color (first vertex in DX)
			vtx.c = i0 == 0 ? vtx.c : (i1 == 0 ? other.c : opposite.c);
		#endif
	}

	return vtx;

#endif
}

#endif // VS_EXPAND

#endif // VERTEX_SHADER
