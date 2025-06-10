// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GSVertexTrace.h"
#include "GS/GSState.h"
#include "GS/GSUtil.h"
#include <cfloat>

class CURRENT_ISA::GSVertexTraceFMM
{
	template <GS_PRIM_CLASS primclass, u32 iip, u32 tme, u32 fst, u32 color, bool flat_swapped>
	static void FindMinMax(const void* vertex, const u16* index, int count,
		GSVector4* tmin_out, GSVector4* tmax_out, GSVector4i* cmin_out, GSVector4i* cmax_out, GSVector4i* pmin_out, GSVector4i* pmax_out);

	template <GS_PRIM_CLASS primclass, u32 iip, u32 tme, u32 fst, u32 color>
	static constexpr GSVertexTrace::FindMinMaxPtr GetFMM(bool provoking_vertex_first);

public:
	static void Populate(GSVertexTrace& vt, bool provoking_vertex_first);
};

MULTI_ISA_UNSHARED_IMPL;

void CURRENT_ISA::GSVertexTracePopulateFunctions(GSVertexTrace& vt, bool provoking_vertex_first)
{
	GSVertexTraceFMM::Populate(vt, provoking_vertex_first);
}

template <GS_PRIM_CLASS primclass, u32 iip, u32 tme, u32 fst, u32 color>
constexpr GSVertexTrace::FindMinMaxPtr GSVertexTraceFMM::GetFMM(bool provoking_vertex_first)
{
	constexpr bool real_iip = (primclass != GS_POINT_CLASS) && (primclass != GS_SPRITE_CLASS) && iip;
	constexpr bool real_fst = tme && fst;
	constexpr bool provoking_vertex_first_class = primclass == GS_LINE_CLASS || primclass == GS_TRIANGLE_CLASS;
	const bool swap = provoking_vertex_first_class && !iip && provoking_vertex_first;

	if (swap)
		return FindMinMax<primclass, real_iip, tme, real_fst, color, true>;
	else
		return FindMinMax<primclass, real_iip, tme, real_fst, color, false>;
}

void GSVertexTraceFMM::Populate(GSVertexTrace& vt, bool provoking_vertex_first)
{
	#define InitUpdate3(P, IIP, TME, FST, COLOR) \
		vt.m_fmm[COLOR][FST][TME][IIP][P] = GetFMM<P, IIP, TME, FST, COLOR>(provoking_vertex_first);

	#define InitUpdate2(P, IIP, TME) \
		InitUpdate3(P, IIP, TME, 0, 0) \
		InitUpdate3(P, IIP, TME, 0, 1) \
		InitUpdate3(P, IIP, TME, 1, 0) \
		InitUpdate3(P, IIP, TME, 1, 1) \

	#define InitUpdate(P) \
		InitUpdate2(P, 0, 0) \
		InitUpdate2(P, 0, 1) \
		InitUpdate2(P, 1, 0) \
		InitUpdate2(P, 1, 1) \

	InitUpdate(GS_POINT_CLASS);
	InitUpdate(GS_LINE_CLASS);
	InitUpdate(GS_TRIANGLE_CLASS);
	InitUpdate(GS_SPRITE_CLASS);
}

template <GS_PRIM_CLASS primclass, u32 iip, u32 tme, u32 fst, u32 color, bool flat_swapped>
void GSVertexTraceFMM::FindMinMax(const void* vertex, const u16* index, int count,
	GSVector4* tmin_out, GSVector4* tmax_out, GSVector4i* cmin_out, GSVector4i* cmax_out, GSVector4i* pmin_out, GSVector4i* pmax_out)
{
	const int n = GSUtil::GetClassVertexCount(primclass);

	GSVector4 tmin = GSVector4::cxpr(FLT_MAX);
	GSVector4 tmax = GSVector4::cxpr(-FLT_MAX);

	GSVector4i cmin = GSVector4i::xffffffff();
	GSVector4i cmax = GSVector4i::zero();

	GSVector4i pmin = GSVector4i::xffffffff();
	GSVector4i pmax = GSVector4i::zero();

	const GSVertex* RESTRICT v = (GSVertex*)vertex;

	// Process 2 vertices at a time for increased efficiency
	auto ProcessVertices = [&tmin, &tmax, &cmin, &cmax, &pmin, &pmax, n](const GSVertex& v0, const GSVertex& v1, bool ignore_color = false)
	{
		if (color && !ignore_color)
		{
			GSVector4i c0 = GSVector4i::load(v0.RGBAQ.U32[0]);
			GSVector4i c1 = GSVector4i::load(v1.RGBAQ.U32[0]);

			if (primclass == GS_SPRITE_CLASS || (primclass == GS_LINE_CLASS && !iip))
			{
				// Flat shaded sprites or lines.
				// Means that c0, c1 belong to same primitive.
				const GSVector4i c = flat_swapped ? c0 : c1;
				cmin = cmin.min_u8(c);
				cmax = cmax.max_u8(c);
			}
			else
			{
				// In these cases we use color from both vertices
				cmin = cmin.min_u8(c0.min_u8(c1));
				cmax = cmax.max_u8(c0.max_u8(c1));
			}
		}

		if (tme)
		{
			if (!fst)
			{
				GSVector4 stq0 = GSVector4::cast(GSVector4i(v0.m[0]));
				GSVector4 stq1 = GSVector4::cast(GSVector4i(v1.m[0]));

				pxAssert(primclass != GS_SPRITE_CLASS || stq0.w == stq1.w);

				GSVector4 q = stq0.wwww(stq1);

				GSVector4 st = stq0.xyxy(stq1) / q;

				stq0 = st.xyyy(q);
				stq1 = st.zwww(q);

				tmin = tmin.min(stq0.min(stq1));
				tmax = tmax.max(stq0.max(stq1));
			}
			else
			{
				GSVector4i uv0(v0.m[1]);
				GSVector4i uv1(v1.m[1]);

				GSVector4 st0 = GSVector4(uv0.uph16()).xyxy();
				GSVector4 st1 = GSVector4(uv1.uph16()).xyxy();

				tmin = tmin.min(st0.min(st1));
				tmax = tmax.max(st0.max(st1));

			}
		}

		GSVector4i xyzf0(v0.m[1]);
		GSVector4i xyzf1(v1.m[1]);

		GSVector4i xy0 = xyzf0.upl16();
		GSVector4i zf0 = xyzf0.ywyw();
		GSVector4i xy1 = xyzf1.upl16();
		GSVector4i zf1 = xyzf1.ywyw();

		GSVector4i p0 = xy0.upl64(primclass == GS_SPRITE_CLASS ? zf1 : zf0);
		GSVector4i p1 = xy1.upl64(zf1);
	
		pmin = pmin.min_u32(p0.min_u32(p1));
		pmax = pmax.max_u32(p0.max_u32(p1));
	};

	if (iip) // Gourad shading; treat all vertices the same.
	{
		for (int i = 0; i < count - 1; i += 2) // 2x loop unroll
		{
			ProcessVertices(v[index[i]], v[index[i + 1]]);
		}
		if (count & 1)
		{
			ProcessVertices(v[index[count - 1]], v[index[count - 1]]);
		}
	}
	else if (primclass == GS_TRIANGLE_CLASS)
	{
		// Flat shaded triangles.
		// Only case where we process two primitives in parallel and control color with the ignore flag.
		// Process two triangles in parallel
		for (int i = 0; i < count - 3; i += 6)
		{
			ProcessVertices(v[index[i + 0]], v[index[i + 3]], !flat_swapped);
			ProcessVertices(v[index[i + 1]], v[index[i + 4]], true);
			ProcessVertices(v[index[i + 2]], v[index[i + 5]], flat_swapped);
		}
		// Process last triangle if we had an odd number of triangles
		if (count & 1)
		{
			ProcessVertices(v[index[count - 3]], v[index[count - 2]], !flat_swapped);
			ProcessVertices(v[index[count - 1]], v[index[count - 2]], flat_swapped);
		}
	}
	else
	{
		// Flat shaded everything else. Process vertices sequentially two at a time.
		// Color control for lines/sprites is handled in ProcessVertices.
		for (int i = 0; i < count - 1; i += 2) // 2x loop unroll
		{
			ProcessVertices(v[index[i + 0]], v[index[i + 1]]);
		}
		if (count & 1)
		{
			ProcessVertices(v[index[count - 1]], v[index[count - 1]]);
		}
	}

	// Write out the results
	*tmin_out = tmin;
	*tmax_out = tmax;
	*cmin_out = cmin;
	*cmax_out = cmax;
	*pmin_out = pmin;
	*pmax_out = pmax;
}
