// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GSVertexTrace.h"
#include "GS/GSState.h"
#include "GS/GSUtil.h"
#include <cfloat>

class CURRENT_ISA::GSVertexTraceFMM
{
	template <GS_PRIM_CLASS primclass, u32 iip, u32 tme, u32 fst, u32 color>
	static void FindMinMax(const void* vertex, const u16* index, int count,
		GSVector4& tmin_out, GSVector4& tmax_out, GSVector4i& cmin_out, GSVector4i& cmax_out,
		GSVector4i& pmin_out, GSVector4i& pmax_out);

	template <GS_PRIM_CLASS primclass, u32 iip, u32 tme, u32 fst, u32 color>
	static constexpr GSVertexTrace::FindMinMaxPtr GetFMM();

public:
	static void Populate(GSVertexTrace& vt);
};

MULTI_ISA_UNSHARED_IMPL;

void CURRENT_ISA::GSVertexTracePopulateFunctions(GSVertexTrace& vt)
{
	GSVertexTraceFMM::Populate(vt);
}

template <GS_PRIM_CLASS primclass, u32 iip, u32 tme, u32 fst, u32 color>
constexpr GSVertexTrace::FindMinMaxPtr GSVertexTraceFMM::GetFMM()
{
	constexpr bool real_iip = (primclass != GS_POINT_CLASS) && (primclass != GS_SPRITE_CLASS) && iip;
	constexpr bool real_fst = tme && fst;

	return FindMinMax<primclass, real_iip, tme, real_fst, color>;
}

void GSVertexTraceFMM::Populate(GSVertexTrace& vt)
{
	#define InitUpdate3(P, IIP, TME, FST, COLOR) \
		vt.m_fmm[COLOR][FST][TME][IIP][P] = GetFMM<P, IIP, TME, FST, COLOR>();

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

	#undef InitUpdate3
	#undef InitUpdate2
	#undef InitUpdate
}

template <GS_PRIM_CLASS primclass, u32 iip, u32 tme, u32 fst, u32 color>
void GSVertexTraceFMM::FindMinMax(const void* vertex, const u16* index, int count,
	GSVector4& tmin_out, GSVector4& tmax_out, GSVector4i& cmin_out, GSVector4i& cmax_out,
	GSVector4i& pmin_out, GSVector4i& pmax_out)
{
	constexpr int n = GSUtil::GetClassVertexCount(primclass);

	pxAssert(count % n == 0);

	GSVector4 tmin = GSVector4::cxpr(FLT_MAX);
	GSVector4 tmax = GSVector4::cxpr(-FLT_MAX);

	GSVector4i cmin = GSVector4i::xffffffff();
	GSVector4i cmax = GSVector4i::zero();

	GSVector4i pmin = GSVector4i::xffffffff();
	GSVector4i pmax = GSVector4i::zero();

	const GSVertex* RESTRICT v = (GSVertex*)vertex;

	// Process 2 vertices at a time for increased efficiency.
	const auto ProcessVertices = [&tmin, &tmax, &cmin, &cmax, &pmin, &pmax]<bool use_color>(
		const GSVertex& v0, const GSVertex& v1)
	{
		// Process color.
		if constexpr (color && use_color)
		{
			const GSVector4i c0 = GSVector4i::load(v0.RGBAQ.U32[0]);
			const GSVector4i c1 = GSVector4i::load(v1.RGBAQ.U32[0]);

			if constexpr (primclass == GS_SPRITE_CLASS || (primclass == GS_LINE_CLASS && !iip))
			{
				// Flat shaded sprites or lines.
				// Last color is provoking in flat-shaded primitives.
				cmin = cmin.min_u8(c1);
				cmax = cmax.max_u8(c1);
			}
			else
			{
				// Otherwise use color from both vertices.
				cmin = cmin.min_u8(c0.min_u8(c1));
				cmax = cmax.max_u8(c0.max_u8(c1));
			}
		}

		// Process UV or ST.
		if constexpr (tme)
		{
			if constexpr (!fst)
			{
				// Process ST.
				GSVector4 stq0 = GSVector4::cast(GSVector4i(v0.m[0]));
				GSVector4 stq1 = GSVector4::cast(GSVector4i(v1.m[0]));

				if (primclass == GS_SPRITE_CLASS)
					pxAssert(stq0.w == stq1.w); // Q should be fixed in vertex queue to be the same for both vertices.

				const GSVector4 q = stq0.wwww(stq1);

				const GSVector4 st = stq0.xyxy(stq1) / q;

				stq0 = st.xyyy(q);
				stq1 = st.zwww(q);

				tmin = tmin.min(stq0.min(stq1));
				tmax = tmax.max(stq0.max(stq1));
			}
			else
			{
				// Process UV.
				const GSVector4i uv0(v0.m[1]);
				const GSVector4i uv1(v1.m[1]);

				const GSVector4 st0 = GSVector4(uv0.uph16()).xyxy();
				const GSVector4 st1 = GSVector4(uv1.uph16()).xyxy();

				tmin = tmin.min(st0.min(st1));
				tmax = tmax.max(st0.max(st1));
			}
		}

		// Process XYZ
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

	if constexpr (iip || primclass == GS_POINT_CLASS)
	{
		// Points or Gouraud shading; treat all vertices the same.
		for (int i = 0; i < count - 1; i += 2) // 2x loop unroll
			ProcessVertices.template operator()<true>(v[index[i]], v[index[i + 1]]);

		// Process last vertex if we had an odd number of vertices.
		if (count & 1)
			ProcessVertices.template operator()<true>(v[index[count - 1]], v[index[count - 1]]);
	}
	else if constexpr (primclass == GS_TRIANGLE_CLASS)
	{
		// Flat shaded triangles.
		// Only case where we process two primitives in parallel and control color with the use_color flag.
		for (int i = 0; i < count - 3; i += 6)
		{
			ProcessVertices.template operator()<false>(v[index[i + 0]], v[index[i + 3]]);
			ProcessVertices.template operator()<false>(v[index[i + 1]], v[index[i + 4]]);
			ProcessVertices.template operator()<true>(v[index[i + 2]], v[index[i + 5]]);
		}
		// Process last triangle if we had an odd number of triangles.
		if (count & 1)
		{
			ProcessVertices.template operator()<false>(v[index[count - 3]], v[index[count - 2]]);
			ProcessVertices.template operator()<true>(v[index[count - 1]], v[index[count - 1]]);
		}
	}
	else
	{
		// Flat shaded lines or sprites.
		pxAssert(primclass == GS_LINE_CLASS || primclass == GS_SPRITE_CLASS);
		for (int i = 0; i < count; i += 2)
			ProcessVertices.template operator()<true>(v[index[i + 0]], v[index[i + 1]]);
	}

	// Write out the results.
	tmin_out = tmin;
	tmax_out = tmax;
	cmin_out = cmin;
	cmax_out = cmax;
	pmin_out = pmin;
	pmax_out = pmax;
}
