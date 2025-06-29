// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

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

	const GSDrawingContext* context = m_state->m_context;

	m_primclass = primclass;

	// Setup selector parameters for FindMinMax()
	const u32 iip = m_state->PRIM->IIP;
	const u32 tme = m_state->PRIM->TME;
	const u32 fst = m_state->PRIM->FST;
	const u32 color = !(m_state->PRIM->TME && context->TEX0.TFX == TFX_DECAL && context->TEX0.TCC);

	// Select correct FindMinMax() function to calculaute raw min/max values
	GSVector4 tmin, tmax;
	GSVector4i cmin, cmax, pmin, pmax;
	m_fmm[color][fst][tme][iip][primclass](vertex, index, i_count, &tmin, &tmax, &cmin, &cmax, &pmin, &pmax);

	// Set m_min and m_max values based on the raw min/max values
	const GSVector4 offset(m_state->m_context->XYOFFSET);
	const GSVector4 pscale(1.0f / 16, 1.0f / 16, 0.0f, 1.0f);

	m_min.p = (GSVector4(pmin) - offset) * pscale;
	m_max.p = (GSVector4(pmax) - offset) * pscale;

	// Do Z separately, requires unsigned int conversion
	m_min.p.z = (float)pmin.U32[2];
	m_max.p.z = (float)pmax.U32[2];

	m_min.t = GSVector4::zero();
	m_max.t = GSVector4::zero();
	m_min.c = GSVector4i::zero();
	m_max.c = GSVector4i::zero();

	if (tme)
	{
		const GSVector4 tscale = fst ? GSVector4(1.0f / 16, 1.0f / 16, 1.0f, 1.0f) :
																	 GSVector4((float)(1 << context->TEX0.TW), (float)(1 << context->TEX0.TH), 1.0f, 1.0f);
		m_min.t = tmin * tscale;
		m_max.t = tmax * tscale;
	}

	if (color)
	{
		m_min.c = cmin.u8to32();
		m_max.c = cmax.u8to32();
	}

	// AA1: Set alpha min max to coverage 128 when there is no alpha blending.
	if (!m_state->PRIM->ABE && m_state->PRIM->AA1 && (primclass == GS_LINE_CLASS || primclass == GS_TRIANGLE_CLASS))
	{
		m_min.c.a = 128;
		m_max.c.a = 128;
	}

	m_alpha.valid = false;

	// Set m_eq flags for when vertex values are constant
	const u32 t_eq = (m_min.t == m_max.t).mask();          // GSVector4, 4 bit mask
	const u32 p_eq = GSVector4::cast(pmin == pmax).mask(); // Cast to GSVector4() for 4 bit mask
	const u32 c_eq = (m_min.c == m_max.c).mask();          // GSVector4i, 16 bit mask
	m_eq.value = c_eq | (p_eq << 16) | (t_eq << 20);

	// Potential float overflow detected. Better uses the slower division instead
	// Note: If Q is too big, 1/Q will end up as 0. 1e30 is a random number
	// that feel big enough.
	if (!fst && !m_accurate_stq && m_min.t.z > 1e30)
	{
		Console.Warning("Vertex Trace: float overflow detected ! min %e max %e", m_min.t.z, m_max.t.z);
		m_accurate_stq = true;
	}

	// Determine mipmapping LOD and whether linear filter is used
	if (tme)
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
			const float K = static_cast<float>(TEX1.K) / 16;

			if (TEX1.LCM == 0 && m_state->PRIM->FST == 0) // FST == 1 => Q is not interpolated
			{
				// LOD = log2(1/|Q|) * (1 << L) + K

				GSVector4::storel(&m_lod, m_max.t.uph(m_min.t).log2(3).neg() * static_cast<float>(1 << TEX1.L) + K);

				if (m_lod.x > m_lod.y)
				{
					const float tmp = m_lod.x;
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
