/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "GSVertexTrace.h"
#include "GS/GSState.h"
#include <cfloat>

class CURRENT_ISA::GSVertexTraceFMM
{
	static constexpr GSVector4 s_minmax = GSVector4::cxpr(FLT_MAX, -FLT_MAX, 0.f, 0.f);

	template <GS_PRIM_CLASS primclass, u32 iip, u32 tme, u32 fst, u32 color, bool flat_swapped>
	static void FindMinMax(GSVertexTrace& vt, void* vertex, const u16* index, int count);

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
	constexpr bool real_iip = primclass == GS_SPRITE_CLASS ? false : iip;
	constexpr bool real_fst = tme ? fst : false;
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
void GSVertexTraceFMM::FindMinMax(GSVertexTrace& vt, void* vertex, const u16* index, int count)
{
	const GSDrawingContext* context = vt.m_state->m_context;

	int n = 1;

	switch (primclass)
	{
		case GS_POINT_CLASS:
			n = 1;
			break;
		case GS_LINE_CLASS:
		case GS_SPRITE_CLASS:
			n = 2;
			break;
		case GS_TRIANGLE_CLASS:
			n = 3;
			break;
	}

	GSVector4 tmin = s_minmax.xxxx();
	GSVector4 tmax = s_minmax.yyyy();
	GSVector4i cmin = GSVector4i::xffffffff();
	GSVector4i cmax = GSVector4i::zero();

	GSVector4i pmin = GSVector4i::xffffffff();
	GSVector4i pmax = GSVector4i::zero();

	GSVertex* RESTRICT v = static_cast<GSVertex*>(vertex);

	// Process 2 vertices at a time for increased efficiency
	auto processVertices = [&tmin, &tmax, &cmin, &cmax, &pmin, &pmax, n](GSVertex& v0, GSVertex& v1, bool finalVertex)
	{
		if (color)
		{
			GSVector4i c0 = GSVector4i::load(v0.RGBAQ.U32[0]);
			GSVector4i c1 = GSVector4i::load(v1.RGBAQ.U32[0]);
			if (iip || finalVertex)
			{
				cmin = cmin.min_u8(c0.min_u8(c1));
				cmax = cmax.max_u8(c0.max_u8(c1));
			}
			else if (n == 2)
			{
				// For even n, we process v1 and v2 of the same prim
				// (For odd n, we process one vertex from each of two prims)
				GSVector4i c = flat_swapped ? c0 : c1;
				cmin = cmin.min_u8(c);
				cmax = cmax.max_u8(c);
			}
		}

		if (tme)
		{
			if (!fst)
			{
				GSVector4 stq0 = GSVector4::load<true>(&v0.m[0]);
				GSVector4 stq1 = GSVector4::load<true>(&v1.m[0]);

				GSVector4 q;
				// Sprites always have indices == vertices, so we don't have to look at the index table here
				if (primclass == GS_SPRITE_CLASS)
					q = stq1.wwww();
				else
					q = stq0.wwww(stq1);

				// Note: If in the future this is changed in a way that causes parts of calculations to go unused,
				//       make sure to remove the z (rgba) field as it's often denormal.
				//       Then, use GSVector4::noopt() to prevent clang from optimizing out your "useless" shuffle
				//       e.g. stq = (stq.xyww() / stq.wwww()).noopt().xyww(stq);
				GSVector4 st = stq0.xyxy(stq1);

				// Texel coordinate rounding
				// Helps Manhunt (lights shining through objects).
				// Can help with some alignment issues when upscaling too, and is for both Software and Hardware renderers.
				// Sometimes hardware doesn't get affected, likely due to the difference in how GPU's handle textures (Persona minimap).
				//
				// Why are we doing it here? Because it's the only time we loop over all the vertices, and the altered S/T change
				// the min/max in the vertex trace. Better than doing a separate loop.
				// TODO: Is it worth using AVX2 here for twice the throughput?
				const GSVector4i hexff = GSVector4i::cxpr(0xff);

				// expST = (st >> 23) & 0xff
				const GSVector4i expST = GSVector4i::cast(st).srl32(23) & hexff;

				// expQ = (q >> 23) & 0xff
				const GSVector4i expQ = GSVector4i::cast(q).srl32(23) & hexff;

				// maxExp = max(expST, expQ)
				const GSVector4i maxExp = expST.max_u32(expQ);

				// amount = min(9 + (maxExp - expST), 23)
				const GSVector4i amount = maxExp.sub32(expST).add32(GSVector4i::cxpr(9)).min_u32(GSVector4i::cxpr(23));

				// mask = (1 << amount) - 1
				const GSVector4i mask = GSVector4i::cxpr(1).sllv32(amount).sub32(GSVector4i::cxpr(1));

				// st = st & ~mask
				st = st.andnot(GSVector4::cast(mask));

				// q = q & ~0xff
				q = GSVector4::cast(GSVector4i::cast(q) & ~hexff);

				// Since we're going through the vertices anyway, may as well write it back, rather than doing it on the GPU.
				// These shuffles preserve the original RGBA value, only changing ST and Q. We could do it as two stores per,
				// vertex, but a single store will be better for store->load forwarding.

				// v0.m[0] = (st.x, st.y, v0.m[0].z, q0)
				GSVector4::store<true>(&v0.m[0], st.xyzw(stq0).insert32<0, 3>(q));

				// v1.m[0] = (st.z, st.w, v1.m[0].z, q1)
				GSVector4::store<true>(&v1.m[0], st.zwzw(stq1).blend32<8>(q));

				st /= q;

				stq0 = st.xyww(primclass == GS_SPRITE_CLASS ? stq1 : stq0);
				stq1 = st.zwww(stq1);

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

		GSVector4i p0 = xy0.blend32<0xc>(primclass == GS_SPRITE_CLASS ? zf1 : zf0);
		GSVector4i p1 = xy1.blend32<0xc>(zf1);

		pmin = pmin.min_u32(p0.min_u32(p1));
		pmax = pmax.max_u32(p0.max_u32(p1));
	};

	if (n == 2)
	{
		for (int i = 0; i < count; i += 2)
		{
			processVertices(v[index[i + 0]], v[index[i + 1]], false);
		}
	}
	else if (iip || n == 1) // iip means final and non-final vertexes are treated the same
	{
		int i = 0;
		for (; i < (count - 1); i += 2) // 2x loop unroll
		{
			processVertices(v[index[i + 0]], v[index[i + 1]], true);
		}
		if (count & 1)
		{
			// Compiler optimizations go!
			// (And if they don't, it's only one vertex out of many)
			processVertices(v[index[i]], v[index[i]], true);
		}
	}
	else if (n == 3)
	{
		int i = 0;
		for (; i < (count - 3); i += 6)
		{
			processVertices(v[index[i + 0]], v[index[i + 3]], flat_swapped);
			processVertices(v[index[i + 1]], v[index[i + 4]], false);
			processVertices(v[index[i + 2]], v[index[i + 5]], !flat_swapped);
		}
		if (count & 1)
		{
			if (flat_swapped)
			{
				processVertices(v[index[i + 1]], v[index[i + 2]], false);
				// Compiler optimizations go!
				// (And if they don't, it's only one vertex out of many)
				processVertices(v[index[i + 0]], v[index[i + 0]], true);
			}
			else
			{
				processVertices(v[index[i + 0]], v[index[i + 1]], false);
				// Compiler optimizations go!
				// (And if they don't, it's only one vertex out of many)
				processVertices(v[index[i + 2]], v[index[i + 2]], true);
			}
		}
	}
	else
	{
		pxAssertRel(0, "Bad n value");
	}

	GSVector4 o(context->XYOFFSET);
	GSVector4 s(1.0f / 16, 1.0f / 16, 2.0f, 1.0f);

	vt.m_min.p = (GSVector4(pmin) - o) * s;
	vt.m_max.p = (GSVector4(pmax) - o) * s;

	// Fix signed int conversion
	vt.m_min.p = vt.m_min.p.insert32<0, 2>(GSVector4::load((float)(u32)pmin.extract32<2>()));
	vt.m_max.p = vt.m_max.p.insert32<0, 2>(GSVector4::load((float)(u32)pmax.extract32<2>()));

	if (tme)
	{
		if (fst)
		{
			s = GSVector4(1.0f / 16, 1.0f).xxyy();
		}
		else
		{
			s = GSVector4(1 << context->TEX0.TW, 1 << context->TEX0.TH, 1, 1);
		}

		vt.m_min.t = tmin * s;
		vt.m_max.t = tmax * s;
	}
	else
	{
		vt.m_min.t = GSVector4::zero();
		vt.m_max.t = GSVector4::zero();
	}

	if (color)
	{
		vt.m_min.c = cmin.u8to32();
		vt.m_max.c = cmax.u8to32();
	}
	else
	{
		vt.m_min.c = GSVector4i::zero();
		vt.m_max.c = GSVector4i::zero();
	}
}
