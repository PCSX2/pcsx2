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

#include "PrecompiledHeader.h"
#include "GSVertexTrace.h"
#include "GS/GSUtil.h"
#include "GS/GSState.h"
#include <cfloat>

CONSTINIT const GSVector4 GSVertexTrace::s_minmax = GSVector4::cxpr(FLT_MAX, -FLT_MAX, 0.f, 0.f);

GSVertexTrace::GSVertexTrace(const GSState* state, bool provoking_vertex_first)
	: m_accurate_stq(false), m_state(state), m_primclass(GS_INVALID_CLASS)
{
	memset(&m_alpha, 0, sizeof(m_alpha));

	#define InitUpdate3(P, IIP, TME, FST, COLOR) \
	m_fmm[COLOR][FST][TME][IIP][P] = \
		provoking_vertex_first ? &GSVertexTrace::FindMinMax<P, IIP, TME, FST, COLOR, true> : \
                                 &GSVertexTrace::FindMinMax<P, IIP, TME, FST, COLOR, false>;

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

void GSVertexTrace::Update(const void* vertex, const u32* index, int v_count, int i_count, GS_PRIM_CLASS primclass)
{
	if (i_count == 0)
		return;

	m_primclass = primclass;

	u32 iip = m_state->PRIM->IIP;
	u32 tme = m_state->PRIM->TME;
	u32 fst = m_state->PRIM->FST;
	u32 color = !(m_state->PRIM->TME && m_state->m_context->TEX0.TFX == TFX_DECAL && m_state->m_context->TEX0.TCC);

	(this->*m_fmm[color][fst][tme][iip][primclass])(vertex, index, i_count);

	// Potential float overflow detected. Better uses the slower division instead
	// Note: If Q is too big, 1/Q will end up as 0. 1e30 is a random number
	// that feel big enough.
	if (!fst && !m_accurate_stq && m_min.t.z > 1e30)
	{
		fprintf(stderr, "Vertex Trace: float overflow detected ! min %e max %e\n", m_min.t.z, m_max.t.z);
		m_accurate_stq = true;
	}

	m_eq.value = (m_min.c == m_max.c).mask() | ((m_min.p == m_max.p).mask() << 16) | ((m_min.t == m_max.t).mask() << 20);

	m_alpha.valid = false;

	// I'm not sure of the cost. In doubt let's do it only when depth is enabled
	if (m_state->m_context->TEST.ZTE == 1 && m_state->m_context->TEST.ZTST > ZTST_ALWAYS)
	{
		CorrectDepthTrace(vertex, v_count);
	}

	if (m_state->PRIM->TME)
	{
		const GIFRegTEX1& TEX1 = m_state->m_context->TEX1;

		m_filter.mmag = TEX1.IsMagLinear();
		m_filter.mmin = TEX1.IsMinLinear();

		if (TEX1.MXL == 0) // MXL == 0 => MMIN ignored, tested it on ps2
		{
			m_filter.linear = m_filter.mmag;
		}
		else
		{
			float K = (float)TEX1.K / 16;

			if (TEX1.LCM == 0 && m_state->PRIM->FST == 0) // FST == 1 => Q is not interpolated
			{
				// LOD = log2(1/|Q|) * (1 << L) + K

				GSVector4::storel(&m_lod, m_max.t.uph(m_min.t).log2(3).neg() * (float)(1 << TEX1.L) + K);

				if (m_lod.x > m_lod.y)
				{
					float tmp = m_lod.x;
					m_lod.x = m_lod.y;
					m_lod.y = tmp;
				}
			}
			else
			{
				m_lod.x = K;
				m_lod.y = K;
			}

			if (m_lod.y <= 0)
			{
				m_filter.linear = m_filter.mmag;
			}
			else if (m_lod.x > 0)
			{
				m_filter.linear = m_filter.mmin;
			}
			else
			{
				m_filter.linear = m_filter.mmag | m_filter.mmin;
			}
		}

		switch (GSConfig.TextureFiltering)
		{
			case BiFiltering::Nearest:
				m_filter.opt_linear = 0;
				break;

			case BiFiltering::Forced_But_Sprite:
				// Special case to reduce the number of glitch when upscaling is enabled
				m_filter.opt_linear = (m_primclass == GS_SPRITE_CLASS) ? m_filter.linear : 1;
				break;

			case BiFiltering::Forced:
				m_filter.opt_linear = 1;
				break;

			case BiFiltering::PS2:
			default:
				m_filter.opt_linear = m_filter.linear;
				break;
		}
	}
}

template <GS_PRIM_CLASS primclass, u32 iip, u32 tme, u32 fst, u32 color, bool flat_swapped>
void GSVertexTrace::FindMinMax(const void* vertex, const u32* index, int count)
{
	const GSDrawingContext* context = m_state->m_context;

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

	const GSVertex* RESTRICT v = (GSVertex*)vertex;

	// Process 2 vertices at a time for increased efficiency
	auto processVertices = [&](const GSVertex& v0, const GSVertex& v1, bool finalVertex)
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
				cmin = cmin.min_u8(c1);
				cmax = cmax.max_u8(c1);
			}
		}

		if (tme)
		{
			if (!fst)
			{
				GSVector4 stq0 = GSVector4::cast(GSVector4i(v0.m[0]));
				GSVector4 stq1 = GSVector4::cast(GSVector4i(v1.m[0]));

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
				GSVector4 st = stq0.xyxy(stq1) / q;

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
		GSVector4i z0 = xyzf0.yyyy();
		GSVector4i xy1 = xyzf1.upl16();
		GSVector4i z1 = xyzf1.yyyy();

		GSVector4i p0 = xy0.blend16<0xf0>(z0.uph32(primclass == GS_SPRITE_CLASS ? xyzf1 : xyzf0));
		GSVector4i p1 = xy1.blend16<0xf0>(z1.uph32(xyzf1));

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
			processVertices(v[index[i + 0]], v[index[i + 1]], flat_swapped);
			// Compiler optimizations go!
			// (And if they don't, it's only one vertex out of many)
			processVertices(v[index[i + 2]], v[index[i + 2]], !flat_swapped);
		}
	}
	else
	{
		pxAssertRel(0, "Bad n value");
	}

	GSVector4 o(context->XYOFFSET);
	GSVector4 s(1.0f / 16, 1.0f / 16, 2.0f, 1.0f);

	m_min.p = (GSVector4(pmin) - o) * s;
	m_max.p = (GSVector4(pmax) - o) * s;

	// Fix signed int conversion
	m_min.p = m_min.p.insert32<0, 2>(GSVector4::load((float)(u32)pmin.extract32<2>()));
	m_max.p = m_max.p.insert32<0, 2>(GSVector4::load((float)(u32)pmax.extract32<2>()));

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

		m_min.t = tmin * s;
		m_max.t = tmax * s;
	}
	else
	{
		m_min.t = GSVector4::zero();
		m_max.t = GSVector4::zero();
	}

	if (color)
	{
		m_min.c = cmin.u8to32();
		m_max.c = cmax.u8to32();
	}
	else
	{
		m_min.c = GSVector4i::zero();
		m_max.c = GSVector4i::zero();
	}
}

void GSVertexTrace::CorrectDepthTrace(const void* vertex, int count)
{
	if (m_eq.z == 0)
		return;

	// FindMinMax isn't accurate for the depth value. Lsb bit is always 0.
	// The code below will check that depth value is really constant
	// and will update m_min/m_max/m_eq accordingly
	//
	// Really impact Xenosaga3
	//
	// Hopefully function is barely called so AVX/SSE will be useless here


	const GSVertex* RESTRICT v = (GSVertex*)vertex;
	u32 z = v[0].XYZ.Z;

	// ought to check only 1/2 for sprite
	if (z & 1)
	{
		// Check that first bit is always 1
		for (int i = 0; i < count; i++)
		{
			z &= v[i].XYZ.Z;
		}
	}
	else
	{
		// Check that first bit is always 0
		for (int i = 0; i < count; i++)
		{
			z |= v[i].XYZ.Z;
		}
	}

	if (z == v[0].XYZ.Z)
	{
		m_eq.z = 1;
	}
	else
	{
		m_eq.z = 0;
	}
}
