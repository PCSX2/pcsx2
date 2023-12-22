// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GSVertexTrace.h"
#include "GS/GSUtil.h"
#include "GS/GSState.h"

#include "common/Console.h"

GSVertexTrace::GSVertexTrace(const GSState* state, bool provoking_vertex_first)
	: m_state(state)
{
	MULTI_ISA_SELECT(GSVertexTracePopulateFunctions)(*this, provoking_vertex_first);
}

void GSVertexTrace::Update(const void* vertex, const u16* index, int v_count, int i_count, GS_PRIM_CLASS primclass)
{
	if (i_count == 0)
		return;

	m_primclass = primclass;

	u32 iip = m_state->PRIM->IIP;
	u32 tme = m_state->PRIM->TME;
	u32 fst = m_state->PRIM->FST;
	u32 color = !(m_state->PRIM->TME && m_state->m_context->TEX0.TFX == TFX_DECAL && m_state->m_context->TEX0.TCC);

	m_fmm[color][fst][tme][iip][primclass](*this, vertex, index, i_count);

	// Potential float overflow detected. Better uses the slower division instead
	// Note: If Q is too big, 1/Q will end up as 0. 1e30 is a random number
	// that feel big enough.
	if (!fst && !m_accurate_stq && m_min.t.z > 1e30)
	{
		Console.Warning("Vertex Trace: float overflow detected ! min %e max %e", m_min.t.z, m_max.t.z);
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
