// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GSVertexTrace.h"
#include "GS/GSUtil.h"
#include "GS/GSState.h"

#include "common/Console.h"

GSVertexTrace::GSVertexTrace(const GSState* state)
	: m_state(state)
{
	MULTI_ISA_SELECT(GSVertexTracePopulateFunctions)(*this);
}

void GSVertexTrace::Update(const void* vertex, const u16* index, int v_count, int i_count, GS_PRIM_CLASS primclass)
{
	if (i_count == 0)
		return;

	m_primclass = primclass;

	const u32 iip = m_state->PRIM->IIP;
	const u32 tme = m_state->PRIM->TME;
	const u32 fst = m_state->PRIM->FST;
	const u32 color = !(m_state->PRIM->TME && m_state->m_context->TEX0.TFX == TFX_DECAL && m_state->m_context->TEX0.TCC);

	bool fused = false;
#ifdef ARCH_ARM64
	// Fused vertex-trace bounds (see GSVertexKick.h): triangle-class draws
	// accumulate their min/max at index-emission time; consume the accumulator
	// instead of re-walking the index list when the finish step can reproduce
	// the legacy tail bit-exactly (it declines on STQ hazards).
	if (primclass == GS_TRIANGLE_CLASS && m_state->m_vertex->fmm_valid)
	{
		GSVertexKernels::FmmResult r;
		if (GSVertexKernels::FmmFinish(m_state->m_vertex->fmm_acc, tme != 0, fst != 0, color != 0,
				m_state->m_context->XYOFFSET, m_state->m_context->TEX0.TW, m_state->m_context->TEX0.TH, r))
		{
			m_min.p = r.min_p;
			m_max.p = r.max_p;
			m_min.t = r.min_t;
			m_max.t = r.max_t;
			m_min.c = r.min_c;
			m_max.c = r.max_c;
			if (r.write_nan)
				nan.value = r.nan_value;
			fused = true;
		}
	}
#endif

	if (!fused)
	{
		m_fmm[color][fst][tme][iip][primclass](*this, vertex, index, i_count);
	}
#if defined(ARCH_ARM64) && defined(GS_VERTEX_CROSSCHECK)
	else
	{
		const GSVector4 xp_min = m_min.p, xp_max = m_max.p, xt_min = m_min.t, xt_max = m_max.t;
		const GSVector4i xc_min = m_min.c, xc_max = m_max.c;
		const u32 x_nan = nan.value;

		m_fmm[color][fst][tme][iip][primclass](*this, vertex, index, i_count);

		pxAssertRel(GSVector4i::cast(xp_min).eq(GSVector4i::cast(m_min.p)) &&
						GSVector4i::cast(xp_max).eq(GSVector4i::cast(m_max.p)) &&
						GSVector4i::cast(xt_min).eq(GSVector4i::cast(m_min.t)) &&
						GSVector4i::cast(xt_max).eq(GSVector4i::cast(m_max.t)) &&
						xc_min.eq(m_min.c) && xc_max.eq(m_max.c) && x_nan == nan.value,
			"GS_VERTEX_CROSSCHECK: fused FindMinMax divergence");
	}
#endif

	// Potential float overflow detected. Better uses the slower division instead
	// Note: If Q is too big, 1/Q will end up as 0. 1e30 is a random number
	// that feel big enough.
	if (!fst && !m_accurate_stq && m_min.t.z > 1e30)
	{
		Console.Warning("Vertex Trace: float overflow detected ! min %e max %e", m_min.t.z, m_max.t.z);
		m_accurate_stq = true;
	}

	// AA1: Set alpha min max to coverage 128 when there is no alpha blending.
	if (!m_state->PRIM->ABE && m_state->PRIM->AA1 && (m_primclass == GS_LINE_CLASS || m_primclass == GS_TRIANGLE_CLASS))
	{
		m_min.c.a = 128;
		m_max.c.a = 128;
	}

	m_eq.value = (m_min.c == m_max.c).mask() | ((m_min.p == m_max.p).mask() << 16) | ((m_min.t == m_max.t).mask() << 20);

	m_alpha.valid = false;

	// I'm not sure of the cost. In doubt let's do it only when depth is enabled
	if (m_state->m_context->TEST.ZTE == 1 && m_state->m_context->TEST.ZTST > ZTST_ALWAYS)
	{
		CorrectDepthTrace(vertex, v_count);
	}

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

	const int sprite_step = (m_primclass == GS_SPRITE_CLASS) ? 1 : 0;

	u32 z = v[sprite_step].XYZ.Z;

	if (z & 1)
	{
		// Check that first bit is always 1
		for (int i = sprite_step; i < count; i += (sprite_step + 1))
		{
			z &= v[i].XYZ.Z;
		}
	}
	else
	{
		// Check that first bit is always 0
		for (int i = sprite_step; i < count; i += (sprite_step + 1))
		{
			z |= v[i].XYZ.Z;
		}
	}

	if (z == v[sprite_step].XYZ.Z)
	{
		m_eq.z = 1;
	}
	else
	{
		m_eq.z = 0;
	}
}
