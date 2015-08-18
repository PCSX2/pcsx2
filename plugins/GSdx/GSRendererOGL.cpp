/*
 *	Copyright (C) 2011-2011 Gregory hainaut
 *	Copyright (C) 2007-2009 Gabest
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"
#include "GSRendererOGL.h"
#include "GSRenderer.h"


GSRendererOGL::GSRendererOGL()
	: GSRendererHW(new GSTextureCacheOGL(this))
{
	m_accurate_date   = theApp.GetConfig("accurate_date", 0);

	m_sw_blending = theApp.GetConfig("accurate_blending_unit", 1);

	UserHacks_TCOffset       = theApp.GetConfig("UserHacks_TCOffset", 0);
	UserHacks_TCO_x          = (UserHacks_TCOffset & 0xFFFF) / -1000.0f;
	UserHacks_TCO_y          = ((UserHacks_TCOffset >> 16) & 0xFFFF) / -1000.0f;

	if (!theApp.GetConfig("UserHacks", 0)) {
		UserHacks_TCOffset       = 0;
		UserHacks_TCO_x          = 0;
		UserHacks_TCO_y          = 0;
	}
}

bool GSRendererOGL::CreateDevice(GSDevice* dev)
{
	if (!GSRenderer::CreateDevice(dev))
		return false;

	return true;
}

void GSRendererOGL::EmulateGS()
{
	if (m_vt.m_primclass != GS_SPRITE_CLASS) return;

	// each sprite converted to quad needs twice the space

	while(m_vertex.tail * 2 > m_vertex.maxcount)
	{
		GrowVertexBuffer();
	}

	// assume vertices are tightly packed and sequentially indexed (it should be the case)

	if (m_vertex.next >= 2)
	{
		size_t count = m_vertex.next;

		int i = (int)count * 2 - 4;
		GSVertex* s = &m_vertex.buff[count - 2];
		GSVertex* q = &m_vertex.buff[count * 2 - 4];
		uint32* RESTRICT index = &m_index.buff[count * 3 - 6];

		for(; i >= 0; i -= 4, s -= 2, q -= 4, index -= 6)
		{
			GSVertex v0 = s[0];
			GSVertex v1 = s[1];

			v0.RGBAQ = v1.RGBAQ;
			v0.XYZ.Z = v1.XYZ.Z;
			v0.FOG = v1.FOG;

			q[0] = v0;
			q[3] = v1;

			// swap x, s, u

			uint16 x = v0.XYZ.X;
			v0.XYZ.X = v1.XYZ.X;
			v1.XYZ.X = x;

			float s = v0.ST.S;
			v0.ST.S = v1.ST.S;
			v1.ST.S = s;

			uint16 u = v0.U;
			v0.U = v1.U;
			v1.U = u;

			q[1] = v0;
			q[2] = v1;

			index[0] = i + 0;
			index[1] = i + 1;
			index[2] = i + 2;
			index[3] = i + 1;
			index[4] = i + 2;
			index[5] = i + 3;
		}

		m_vertex.head = m_vertex.tail = m_vertex.next = count * 2;
		m_index.tail = count * 3;
	}
}

void GSRendererOGL::SetupIA()
{
	GSDeviceOGL* dev = (GSDeviceOGL*)m_dev;

	if (!GLLoader::found_geometry_shader)
		EmulateGS();

	dev->IASetVertexBuffer(m_vertex.buff, m_vertex.next);
	dev->IASetIndexBuffer(m_index.buff, m_index.tail);

	GLenum t = 0;

	switch(m_vt.m_primclass)
	{
	case GS_POINT_CLASS:
		t = GL_POINTS;
		break;
	case GS_LINE_CLASS:
		t = GL_LINES;
		break;
	case GS_SPRITE_CLASS:
		if (GLLoader::found_geometry_shader)
			t = GL_LINES;
		else
			t = GL_TRIANGLES;
		break;
	case GS_TRIANGLE_CLASS:
		t = GL_TRIANGLES;
		break;
	default:
		__assume(0);
	}

	dev->IASetPrimitiveTopology(t);
}

bool GSRendererOGL::EmulateTextureShuffleAndFbmask(GSDeviceOGL::PSSelector& ps_sel, GSDeviceOGL::OMColorMaskSelector& om_csel)
{
	bool require_barrier = false;

	if (m_texture_shuffle) {
		ps_sel.shuffle = 1;
		ps_sel.dfmt = 0;

		const GIFRegXYOFFSET& o = m_context->XYOFFSET;
		GSVertex* v = &m_vertex.buff[0];
		size_t count = m_vertex.next;

		// vertex position is 8 to 16 pixels, therefore it is the 16-31 bits of the colors
		int  pos = (v[0].XYZ.X - o.OFX) & 0xFF;
		bool write_ba = (pos > 112 && pos < 136);
		// Read texture is 8 to 16 pixels (same as above)
		float tw = (float)(1u << m_context->TEX0.TW);
		int tex_pos = (PRIM->FST) ? v[0].U : tw * v[0].ST.S;
		tex_pos &= 0xFF;
		ps_sel.read_ba = (tex_pos > 112 && tex_pos < 144);

		// Convert the vertex info to a 32 bits color format equivalent
		if (PRIM->FST) {
			GL_INS("First vertex is  P: %d => %d    T: %d => %d", v[0].XYZ.X, v[1].XYZ.X, v[0].U, v[1].U);

			for(size_t i = 0; i < count; i += 2) {
				if (write_ba)
					v[i].XYZ.X   -= 128u;
				else
					v[i+1].XYZ.X += 128u;

				if (ps_sel.read_ba)
					v[i].U       -= 128u;
				else
					v[i+1].U     += 128u;

				// Height is too big (2x).
				int tex_offset = v[i].V & 0xF;
				GSVector4i offset(o.OFY, tex_offset, o.OFY, tex_offset);

				GSVector4i tmp(v[i].XYZ.Y, v[i].V, v[i+1].XYZ.Y, v[i+1].V);
				tmp = GSVector4i(tmp - offset).srl32(1) + offset;

				v[i].XYZ.Y   = tmp.x;
				v[i].V       = tmp.y;
				v[i+1].XYZ.Y = tmp.z;
				v[i+1].V     = tmp.w;
			}
		} else {
			const float offset_8pix = 8.0f / tw;
			GL_INS("First vertex is  P: %d => %d    T: %f => %f (offset %f)", v[0].XYZ.X, v[1].XYZ.X, v[0].ST.S, v[1].ST.S, offset_8pix);

			for(size_t i = 0; i < count; i += 2) {
				if (write_ba)
					v[i].XYZ.X   -= 128u;
				else
					v[i+1].XYZ.X += 128u;

				if (ps_sel.read_ba)
					v[i].ST.S    -= offset_8pix;
				else
					v[i+1].ST.S  += offset_8pix;

				// Height is too big (2x).
				GSVector4i offset(o.OFY, o.OFY);

				GSVector4i tmp(v[i].XYZ.Y, v[i+1].XYZ.Y);
				tmp = GSVector4i(tmp - offset).srl32(1) + offset;

				//fprintf(stderr, "Before %d, After %d\n", v[i+1].XYZ.Y, tmp.y);
				v[i].XYZ.Y   = tmp.x;
				v[i].ST.T   /= 2.0f;
				v[i+1].XYZ.Y = tmp.y;
				v[i+1].ST.T /= 2.0f;
			}
		}

		// If date is enabled you need to test the green channel instead of the
		// alpha channel. Only enable this code in DATE mode to reduce the number
		// of shader.
		ps_sel.write_rg = !write_ba && m_context->TEST.DATE;

		// Please bang my head against the wall!
		// 1/ Reduce the frame mask to a 16 bit format
		const uint32& m = m_context->FRAME.FBMSK;
		uint32 fbmask = ((m >> 3) & 0x1F) | ((m >> 6) & 0x3E0) | ((m >> 9) & 0x7C00) | ((m >> 16) & 0x8000);
		// FIXME GSVector will be nice here
		uint8 rg_mask = fbmask & 0xFF;
		uint8 ba_mask = (fbmask >> 8) & 0xFF;
		om_csel.wrgba = 0;

		// 2 Select the new mask (Please someone put SSE here)
		if (rg_mask != 0xFF) {
			if (write_ba) {
				GL_INS("Color shuffle %s => B", ps_sel.read_ba ? "B" : "R");
				om_csel.wb = 1;
			} else {
				GL_INS("Color shuffle %s => R", ps_sel.read_ba ? "B" : "R");
				om_csel.wr = 1;
			}
			if (rg_mask)
				ps_sel.fbmask = 1;
		}

		if (ba_mask != 0xFF) {
			if (write_ba) {
				GL_INS("Color shuffle %s => A", ps_sel.read_ba ? "A" : "G");
				om_csel.wa = 1;
			} else {
				GL_INS("Color shuffle %s => G", ps_sel.read_ba ? "A" : "G");
				om_csel.wg = 1;
			}
			if (ba_mask)
				ps_sel.fbmask = 1;
		}

		if (ps_sel.fbmask && m_sw_blending) {
			GL_INS("FBMASK SW emulated fb_mask:%x on tex shuffle", fbmask);
			ps_cb.FbMask.r = rg_mask;
			ps_cb.FbMask.g = rg_mask;
			ps_cb.FbMask.b = ba_mask;
			ps_cb.FbMask.a = ba_mask;
			require_barrier = true;
		} else {
			ps_sel.fbmask = 0;
		}

	} else {
		ps_sel.dfmt = GSLocalMemory::m_psm[m_context->FRAME.PSM].fmt;

		GSVector4i fbmask_v = GSVector4i::load((int)m_context->FRAME.FBMSK);
		int ff_fbmask = fbmask_v.eq8(GSVector4i::xffffffff()).mask();
		int zero_fbmask = fbmask_v.eq8(GSVector4i::zero()).mask();

		om_csel.wrgba = ~ff_fbmask; // Enable channel if at least 1 bit is 0

		ps_sel.fbmask = m_sw_blending && (~ff_fbmask & ~zero_fbmask & 0xF);

		if (ps_sel.fbmask) {
			GL_INS("FBMASK SW emulated fb_mask:%x on %d bits format", m_context->FRAME.FBMSK,
					(GSLocalMemory::m_psm[m_context->FRAME.PSM].fmt == 2) ? 16 : 32);
			ps_cb.FbMask = fbmask_v.u8to32();
			require_barrier = true;
		}
	}

	return require_barrier;
}

bool GSRendererOGL::EmulateBlending(GSDeviceOGL::PSSelector& ps_sel, bool DATE_GL42)
{
	GSDeviceOGL* dev         = (GSDeviceOGL*)m_dev;
	const GIFRegALPHA& ALPHA = m_context->ALPHA;
	bool require_barrier     = false;
	bool sw_blending         = false;

	// No blending so early exit
	if (!(PRIM->ABE || PRIM->AA1 && m_vt.m_primclass == GS_LINE_CLASS)) {
#ifdef ENABLE_OGL_DEBUG
		if (m_env.PABE.PABE) {
			GL_INS("!!! ENV PABE  without ABE !!!");
		}
#endif
		dev->OMSetBlendState();
		return false;
	}

	if (m_env.PABE.PABE)
	{
		GL_INS("!!! ENV PABE  not supported !!!");
		if (m_sw_blending >= ACC_BLEND_CCLIP_DALPHA) {
			ps_sel.pabe = 1;
			require_barrier |= (ALPHA.C == 1);
			sw_blending = true;
		}
		//Breath of Fire Dragon Quarter triggers this in battles. Graphics are fine though.
		//ASSERT(0);
	}

	// Compute the blending equation to detect special case
	uint8 blend_index  = ((ALPHA.A * 3 + ALPHA.B) * 3 + ALPHA.C) * 3 + ALPHA.D;
	int blend_flag = GSDeviceOGL::m_blendMapOGL[blend_index].bogus;

	// SW Blend is (nearly) free. Let's use it.
	bool impossible_or_free_blend = (blend_flag & (BLEND_NO_BAR|BLEND_A_MAX|BLEND_ACCU))
			|| (m_prim_overlap == PRIM_OVERLAP_NO);

	// Do the multiplication in shader for blending accumulation: Cs*As + Cd or Cs*Af + Cd
	bool accumulation_blend = (blend_flag & BLEND_ACCU);

	// Warning no break on purpose
	switch (m_sw_blending) {
		case ACC_BLEND_ULTRA:           sw_blending |= true;
		case ACC_BLEND_FULL:            if (!m_vt.m_alpha.valid && (ALPHA.C == 0)) GetAlphaMinMax();
										sw_blending |= (ALPHA.A != ALPHA.B) &&
												((ALPHA.C == 0 && m_vt.m_alpha.max > 128u) || (ALPHA.C == 2 && ALPHA.FIX > 128u));
		case ACC_BLEND_CCLIP_DALPHA:    sw_blending |= (ALPHA.C == 1) || (m_env.COLCLAMP.CLAMP == 0);
		case ACC_BLEND_SPRITE:          sw_blending |= m_vt.m_primclass == GS_SPRITE_CLASS;
		case ACC_BLEND_FREE:            sw_blending |= ps_sel.fbmask || impossible_or_free_blend;
		default:                        sw_blending |= accumulation_blend;
	}
	// SW Blending
	// GL42 interact very badly with sw blending. GL42 uses the primitiveID to find the primitive
	// that write the bad alpha value. Sw blending will force the draw to run primitive by primitive
	// (therefore primitiveID will be constant to 1)
	sw_blending &= !DATE_GL42;

	// Color clip
	if (m_env.COLCLAMP.CLAMP == 0) {
		if (m_prim_overlap == PRIM_OVERLAP_NO) {
			// The fastest algo that requires a single pass
			GL_INS("COLCLIP Free mode ENABLED");
			ps_sel.colclip = 1;
		} else if (accumulation_blend) {
			// A fast algo that requires 2 passes
			GL_INS("COLCLIP Fast HDR mode ENABLED");
			ps_sel.hdr = 1;
		} else if (sw_blending) {
			// A slow algo that could requires several passes (barely used)
			GL_INS("COLCLIP SW ENABLED (blending is %d/%d/%d/%d)", ALPHA.A, ALPHA.B, ALPHA.C, ALPHA.D);
			ps_sel.colclip = 1;
		} else {
			// Speed hack skip previous slow algo
			GL_INS("Sorry colclip isn't supported");
		}
	}

	// Seriously don't expect me to support this kind of crazyness.
	// No mix of COLCLIP + accumulation_blend + DATE GL42
	// Neither fbmask and GL42
	ASSERT(!(ps_sel.hdr && DATE_GL42));
	ASSERT(!(ps_sel.fbmask && DATE_GL42));

	// For stat to optimize accurate option
#if 0
	GL_INS("BLEND_INFO: %d/%d/%d/%d. Clamp:%d. Prim:%d number %d (sw %d)",
			ALPHA.A, ALPHA.B,  ALPHA.C, ALPHA.D, m_env.COLCLAMP.CLAMP, m_vt.m_primclass, m_vertex.next, sw_blending);
#endif
	if (sw_blending) {
		ps_sel.blend_a = ALPHA.A;
		ps_sel.blend_b = ALPHA.B;
		ps_sel.blend_c = ALPHA.C;
		ps_sel.blend_d = ALPHA.D;

		if (accumulation_blend) {
			// Keep HW blending to do the addition/subtraction
			dev->OMSetBlendState(blend_index);
			if (ALPHA.A == 2) {
				// The blend unit does a reverse subtraction so it means
				// the shader must output a positive value.
				// Replace 0 - Cs by Cs - 0
				ps_sel.blend_a = ALPHA.B;
				ps_sel.blend_b = 2;
			}
			// Remove the addition/substraction from the SW blending
			ps_sel.blend_d = 2;
		} else {
			// Disable HW blending
			dev->OMSetBlendState();
		}

		// Require the fix alpha vlaue
		if (ALPHA.C == 2) {
			ps_cb.AlphaCoeff.a = (float)ALPHA.FIX / 128.0f;
		}

		// No need to flush for every primitive
		require_barrier |= !(blend_flag & BLEND_NO_BAR) && !accumulation_blend;
	} else {
		ps_sel.clr1 = !!(blend_flag & BLEND_C_CLR);
		if (ps_sel.dfmt == 1 && ALPHA.C == 1) {
			// 24 bits doesn't have an alpha channel so use 1.0f fix factor as equivalent
			int hacked_blend_index  = blend_index + 3; // +3 <=> +1 on C
			dev->OMSetBlendState(hacked_blend_index, 128, true);
		} else {
			dev->OMSetBlendState(blend_index, ALPHA.FIX, (ALPHA.C == 2));
		}
	}

	return require_barrier;
}

GSRendererOGL::PRIM_OVERLAP GSRendererOGL::PrimitiveOverlap()
{
	// Either 1 triangle or 1 line or 3 POINTs
	// It is bad for the POINTs but low probability that they overlap
	if (m_vertex.next < 4)
		return PRIM_OVERLAP_NO;

	if (m_vt.m_primclass != GS_SPRITE_CLASS)
		return PRIM_OVERLAP_UNKNOW; // maybe, maybe not

	// Check intersection of sprite primitive only
	size_t count = m_vertex.next;
	GSVertex* v = &m_vertex.buff[0];

	// In order to speed up comparaison a boundind-box is accumulated. It removes a
	// loop so code is much faster (check game virtua fighter). Besides it allow to check
	// properly the Y order.
	GSVector4i all(0);
	for(size_t i = 0; i < count; i += 2) {
		GSVector4i sprite;
		if (v[i+1].XYZ.Y < v[i].XYZ.Y) {
			sprite = GSVector4i(v[i].XYZ.X, v[i+1].XYZ.Y, v[i+1].XYZ.X, v[i].XYZ.Y);
		} else {
			sprite = GSVector4i(v[i].XYZ.X, v[i].XYZ.Y, v[i+1].XYZ.X, v[i+1].XYZ.Y);
		}
		if (all.rintersect(sprite).rempty()) {
			all = all.runion(sprite);
		} else {
			return PRIM_OVERLAP_YES;
		}
	}
#if 0
	// Old algo: less constraint but O(n^2) instead of O(n) as above

	// You have no guarantee on the sprite order, first vertex can be either top-left or bottom-left
	// There is a high probability that the draw call will uses same ordering for all vertices.
	// In order to keep a small performance impact only the first sprite will be checked
	//
	// Some safe-guard will be added in the outer-loop to avoid corruption with a limited perf impact
	if (v[1].XYZ.Y < v[0].XYZ.Y) {
		// First vertex is Top-Left
		for(size_t i = 0; i < count; i += 2) {
			if (v[i+1].XYZ.Y > v[i].XYZ.Y) {
				return PRIM_OVERLAP_UNKNOW;
			}
			GSVector4i vi(v[i].XYZ.X, v[i+1].XYZ.Y, v[i+1].XYZ.X, v[i].XYZ.Y);
			for (size_t j = i+2; j < count; j += 2) {
				GSVector4i vj(v[j].XYZ.X, v[j+1].XYZ.Y, v[j+1].XYZ.X, v[j].XYZ.Y);
				GSVector4i inter = vi.rintersect(vj);
				if (!inter.rempty()) {
					return PRIM_OVERLAP_YES;
				}
			}
		}
	} else {
		// First vertex is Bottom-Left
		for(size_t i = 0; i < count; i += 2) {
			if (v[i+1].XYZ.Y < v[i].XYZ.Y) {
				return PRIM_OVERLAP_UNKNOW;
			}
			GSVector4i vi(v[i].XYZ.X, v[i].XYZ.Y, v[i+1].XYZ.X, v[i+1].XYZ.Y);
			for (size_t j = i+2; j < count; j += 2) {
				GSVector4i vj(v[j].XYZ.X, v[j].XYZ.Y, v[j+1].XYZ.X, v[j+1].XYZ.Y);
				GSVector4i inter = vi.rintersect(vj);
				if (!inter.rempty()) {
					return PRIM_OVERLAP_YES;
				}
			}
		}
	}
#endif

	//fprintf(stderr, "%d: Yes, code can be optimized (draw of %d vertices)\n", s_n, count);
	return PRIM_OVERLAP_NO;
}

GSVector4i GSRendererOGL::ComputeBoundingBox(const GSVector2& rtscale, const GSVector2i& rtsize)
{
	GSVector4 scale = GSVector4(rtscale.x, rtscale.y);
	GSVector4 offset = GSVector4(-1.0f, 1.0f); // Round value
	GSVector4 box = m_vt.m_min.p.xyxy(m_vt.m_max.p) + offset.xxyy();
	return GSVector4i(box * scale.xyxy()).rintersect(GSVector4i(0, 0, rtsize.x, rtsize.y));
}

void GSRendererOGL::SendDraw(bool require_barrier)
{
	GSDeviceOGL* dev = (GSDeviceOGL*)m_dev;

	if (!require_barrier) {
		dev->DrawIndexedPrimitive();
	} else if (m_prim_overlap == PRIM_OVERLAP_NO) {
		ASSERT(GLLoader::found_GL_ARB_texture_barrier);
		gl_TextureBarrier();
		dev->DrawIndexedPrimitive();
	} else {
		// FIXME: Investigate: a dynamic check to pack as many primitives as possibles
		// I'm nearly sure GSdx already have this kind of code (maybe we can adapt GSDirtyRect)
		size_t nb_vertex;
		switch (m_vt.m_primclass) {
			case GS_TRIANGLE_CLASS: nb_vertex = 3; break;
			case GS_POINT_CLASS:	nb_vertex = 1; break;
			case GS_SPRITE_CLASS:	nb_vertex = (GLLoader::found_geometry_shader) ? 2 : 6; break;
			default: nb_vertex = 2; break;
		}

		GL_PUSH("Split the draw");

		GL_PERF("Split single draw in %d draw", m_index.tail/nb_vertex);

		for (size_t p = 0; p < m_index.tail; p += nb_vertex) {
			gl_TextureBarrier();
			dev->DrawIndexedPrimitive(p, nb_vertex);
		}

		GL_POP();
	}
}

void GSRendererOGL::DrawPrims(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* tex)
{
	GL_PUSH("GL Draw from %d in %d (Depth %d)",
				tex && tex->m_texture ? tex->m_texture->GetID() : 0,
				rt ? rt->GetID() : -1, ds ? ds->GetID() : -1);

	GSTexture* hdr_rt = NULL;

	const GSVector2i& rtsize = ds ? ds->GetSize()  : rt->GetSize();
	const GSVector2& rtscale = ds ? ds->GetScale() : rt->GetScale();

	bool DATE = m_context->TEST.DATE && m_context->FRAME.PSM != PSM_PSMCT24;
	bool DATE_GL42 = false;
	bool DATE_GL45 = false;

	bool require_barrier = false; // For accurate option

	ASSERT(m_dev != NULL);

	GSDeviceOGL* dev = (GSDeviceOGL*)m_dev;
	dev->s_n = s_n;

	GSDeviceOGL::VSSelector vs_sel;
	GSDeviceOGL::GSSelector gs_sel;

	GSDeviceOGL::PSSelector ps_sel;
	GSDeviceOGL::PSSamplerSelector ps_ssel;

	GSDeviceOGL::OMColorMaskSelector om_csel;
	GSDeviceOGL::OMDepthStencilSelector om_dssel;

	if ((DATE || m_sw_blending) && GLLoader::found_GL_ARB_texture_barrier && (m_vt.m_primclass == GS_SPRITE_CLASS)) {
		// Except 2D games, sprites are often use for special post-processing effect
		m_prim_overlap = PrimitiveOverlap();
	} else {
		m_prim_overlap = PRIM_OVERLAP_UNKNOW;
	}
#ifdef ENABLE_OGL_DEBUG
	if (m_sw_blending && (m_prim_overlap != PRIM_OVERLAP_NO) && (m_context->FRAME.Block() == m_context->TEX0.TBP0) && (m_vertex.next > 2)) {
		GL_INS("ERROR: Source and Target are the same!");
	}
#endif

	require_barrier |= EmulateTextureShuffleAndFbmask(ps_sel, om_csel);

	// DATE: selection of the algorithm. Must be done before blending because GL42 is not compatible with blending

	if (DATE && GLLoader::found_GL_ARB_texture_barrier) {
		if (m_prim_overlap == PRIM_OVERLAP_NO || m_texture_shuffle) {
			// It is way too complex to emulate texture shuffle with DATE. So just use
			// the slow but accurate algo
			require_barrier = true;
			DATE_GL45 = true;
			DATE = false;
		} else if (m_accurate_date && om_csel.wa
				&& (!m_context->TEST.ATE || m_context->TEST.ATST == ATST_ALWAYS)) {
			// texture barrier will split the draw call into n draw call. It is very efficient for
			// few primitive draws. Otherwise it sucks.
			if (m_index.tail < 100) {
				require_barrier = true;
				DATE_GL45 = true;
				DATE = false;
			} else {
				DATE_GL42 = GLLoader::found_GL_ARB_shader_image_load_store;
			}
		}
	}

	// Blend

	if (!IsOpaque() && rt) {
		require_barrier |= EmulateBlending(ps_sel, DATE_GL42);
	} else {
		dev->OMSetBlendState(); // No blending please
	}

	if (ps_sel.dfmt == 1) {
		// Disable writing of the alpha channel
		om_csel.wa = 0;
	}

	// DATE (setup part)

	if (DATE) {
		GSVector4i dRect = ComputeBoundingBox(rtscale, rtsize);

		// Reduce the quantity of clean function
		glScissor( dRect.x, dRect.y, dRect.width(), dRect.height() );
		GLState::scissor = dRect;

		// Must be done here to avoid any GL state pertubation (clear function...)
		// Create an r32ui image that will containt primitive ID
		if (DATE_GL42) {
			dev->InitPrimDateTexture(rt);
		} else {
			GSVector4 src = GSVector4(dRect) / GSVector4(rtsize.x, rtsize.y).xyxy();
			GSVector4 dst = src * 2.0f - 1.0f;

			GSVertexPT1 vertices[] =
			{
				{GSVector4(dst.x, dst.y, 0.0f, 0.0f), GSVector2(src.x, src.y)},
				{GSVector4(dst.z, dst.y, 0.0f, 0.0f), GSVector2(src.z, src.y)},
				{GSVector4(dst.x, dst.w, 0.0f, 0.0f), GSVector2(src.x, src.w)},
				{GSVector4(dst.z, dst.w, 0.0f, 0.0f), GSVector2(src.z, src.w)},
			};

			dev->SetupDATE(rt, ds, vertices, m_context->TEST.DATM);
		}
	}

	//

	dev->BeginScene();

	// om

	if (m_context->TEST.ZTE)
	{
		om_dssel.ztst = m_context->TEST.ZTST;
		om_dssel.zwe = !m_context->ZBUF.ZMSK;
	}
	else
	{
		om_dssel.ztst = ZTST_ALWAYS;
	}

	// vs

	vs_sel.tme = PRIM->TME;
	vs_sel.fst = PRIM->FST;
	vs_sel.wildhack = (UserHacks_WildHack && !isPackedUV_HackFlag) ? 1 : 0;

	// The real GS appears to do no masking based on the Z buffer format and writing larger Z values
	// than the buffer supports seems to be an error condition on the real GS, causing it to crash.
	// We are probably receiving bad coordinates from VU1 in these cases.

	if (om_dssel.ztst >= ZTST_ALWAYS && om_dssel.zwe)
	{
		if (m_context->ZBUF.PSM == PSM_PSMZ24)
		{
			if (m_vt.m_max.p.z > 0xffffff)
			{
				ASSERT(m_vt.m_min.p.z > 0xffffff);
				// Fixme :Following conditional fixes some dialog frame in Wild Arms 3, but may not be what was intended.
				if (m_vt.m_min.p.z > 0xffffff)
				{
					GL_INS("Bad Z size on 24 bits buffers")
					vs_sel.bppz = 1;
					om_dssel.ztst = ZTST_ALWAYS;
				}
			}
		}
		else if (m_context->ZBUF.PSM == PSM_PSMZ16 || m_context->ZBUF.PSM == PSM_PSMZ16S)
		{
			if (m_vt.m_max.p.z > 0xffff)
			{
				ASSERT(m_vt.m_min.p.z > 0xffff); // sfex capcom logo
				// Fixme : Same as above, I guess.
				if (m_vt.m_min.p.z > 0xffff)
				{
					GL_INS("Bad Z size on 16 bits buffers")
					vs_sel.bppz = 2;
					om_dssel.ztst = ZTST_ALWAYS;
				}
			}
		}
	}

	// FIXME Opengl support half pixel center (as dx10). Code could be easier!!!
	float sx = 2.0f * rtscale.x / (rtsize.x << 4);
	float sy = 2.0f * rtscale.y / (rtsize.y << 4);
	float ox = (float)(int)m_context->XYOFFSET.OFX;
	float oy = (float)(int)m_context->XYOFFSET.OFY;
	float ox2 = -1.0f / rtsize.x;
	float oy2 = -1.0f / rtsize.y;

	//This hack subtracts around half a pixel from OFX and OFY. (Cannot do this directly,
	//because DX10 and DX9 have a different pixel center.)
	//
	//The resulting shifted output aligns better with common blending / corona / blurring effects,
	//but introduces a few bad pixels on the edges.

	if (rt && rt->LikelyOffset)
	{
		ox2 *= rt->OffsetHack_modx;
		oy2 *= rt->OffsetHack_mody;
	}

	// Note: DX does y *= -1.0
	vs_cb.Vertex_Scale_Offset = GSVector4(sx, sy, ox * sx + ox2 + 1, oy * sy + oy2 + 1);
	// END of FIXME

	// GS_SPRITE_CLASS are already flat (either by CPU or the GS)
	ps_sel.iip = (m_vt.m_primclass == GS_SPRITE_CLASS) ? 1 : PRIM->IIP;

	if (DATE_GL45) {
		ps_sel.date = 5 + m_context->TEST.DATM;
	} else if (DATE) {
		if (DATE_GL42)
			ps_sel.date = 1 + m_context->TEST.DATM;
		else
			om_dssel.date = 1;
	}

	ps_sel.fba = m_context->FBA.FBA;

	if (PRIM->FGE)
	{
		ps_sel.fog = 1;

		ps_cb.FogColor_AREF = GSVector4::rgba32(m_env.FOGCOL.u32[0]);
	}

	if (m_context->TEST.ATE)
		ps_sel.atst = m_context->TEST.ATST;
	else
		ps_sel.atst = ATST_ALWAYS;

	if (m_context->TEST.ATE && m_context->TEST.ATST > 1)
		ps_cb.FogColor_AREF.a = (float)m_context->TEST.AREF;

	// By default don't use texture
	ps_sel.tfx = 4;
	bool spritehack = false;
	int  atst = ps_sel.atst;

	if (tex)
	{
		const GSLocalMemory::psm_t &psm = GSLocalMemory::m_psm[m_context->TEX0.PSM];
		const GSLocalMemory::psm_t &cpsm = psm.pal > 0 ? GSLocalMemory::m_psm[m_context->TEX0.CPSM] : psm;
		bool bilinear = m_filter == 2 ? m_vt.IsLinear() : m_filter != 0;
		bool simple_sample = !tex->m_palette && cpsm.fmt == 0 && m_context->CLAMP.WMS < 3 && m_context->CLAMP.WMT < 3;
		// Don't force extra filtering on sprite (it creates various upscaling issue)
		bilinear &= !((m_vt.m_primclass == GS_SPRITE_CLASS) && m_userhacks_round_sprite_offset && !m_vt.IsLinear());

		ps_sel.wms = m_context->CLAMP.WMS;
		ps_sel.wmt = m_context->CLAMP.WMT;

		if (ps_sel.shuffle) {
			ps_sel.fmt = 0;
		} else if (tex->m_palette) {
			ps_sel.fmt = cpsm.fmt | 4;
			ps_sel.ifmt = !tex->m_target ? 0
				: (m_context->TEX0.PSM == PSM_PSMT4HL) ? 2
				: (m_context->TEX0.PSM == PSM_PSMT4HH) ? 1
				: 0;

			// In standard mode palette is only used when alpha channel of the RT is
			// reinterpreted as an index. Star Ocean 3 uses it to emulate a stencil buffer.
			// It is a very bad idea to force bilinear filtering on it.
			if (tex->m_target)
				bilinear &= m_vt.IsLinear();

			//GL_INS("Use palette with format %d and index format %d", ps_sel.fmt, ps_sel.ifmt);
		} else {
			ps_sel.fmt = cpsm.fmt;
		}
		ps_sel.aem = m_env.TEXA.AEM;

		if (m_context->TEX0.TFX == TFX_MODULATE && m_vt.m_eq.rgba == 0xFFFF && m_vt.m_min.c.eq(GSVector4i(128))) {
			// Micro optimization that reduces GPU load (removes 5 instructions on the FS program)
			ps_sel.tfx = TFX_DECAL;
		} else {
			ps_sel.tfx = m_context->TEX0.TFX;
		}

		ps_sel.tcc = m_context->TEX0.TCC;

		ps_sel.ltf = bilinear && !simple_sample;
		spritehack = tex->m_spritehack_t;
		// FIXME the ati is currently disabled on the shader. I need to find a .gs to test that we got same
		// bug on opengl
		//ps_sel.point_sampler = 0; // !(bilinear && simple_sample);

		int w = tex->m_texture->GetWidth();
		int h = tex->m_texture->GetHeight();

		int tw = (int)(1 << m_context->TEX0.TW);
		int th = (int)(1 << m_context->TEX0.TH);

		GSVector4 WH(tw, th, w, h);

		if (PRIM->FST)
		{
			vs_cb.TextureScale = GSVector4(1.0f / 16) / WH.xyxy();
			ps_sel.fst = 1;
		}

		ps_cb.WH = WH;
		ps_cb.HalfTexel = GSVector4(-0.5f, 0.5f).xxyy() / WH.zwzw();
		ps_cb.MskFix = GSVector4i(m_context->CLAMP.MINU, m_context->CLAMP.MINV, m_context->CLAMP.MAXU, m_context->CLAMP.MAXV);

		// TC Offset Hack
		ps_sel.tcoffsethack = !!UserHacks_TCOffset;
		ps_cb.TC_OffsetHack = GSVector4(UserHacks_TCO_x, UserHacks_TCO_y).xyxy() / WH.xyxy();

		GSVector4 clamp(ps_cb.MskFix);
		GSVector4 ta(m_env.TEXA & GSVector4i::x000000ff());

		ps_cb.MinMax = clamp / WH.xyxy();
		ps_cb.MinF_TA = (clamp + 0.5f).xyxy(ta) / WH.xyxy(GSVector4(255, 255));

		ps_ssel.tau = (m_context->CLAMP.WMS + 3) >> 1;
		ps_ssel.tav = (m_context->CLAMP.WMT + 3) >> 1;
		ps_ssel.ltf = bilinear && simple_sample;

		// Setup Texture ressources
		dev->SetupSampler(ps_ssel);
		dev->PSSetShaderResources(tex->m_texture, tex->m_palette);

		if (spritehack && (ps_sel.atst == 2)) {
			ps_sel.atst = 1;
		}
	} else {
#ifdef ENABLE_OGL_DEBUG
		// Unattach texture to avoid noise in debugger
		dev->PSSetShaderResources(NULL, NULL);
#endif
	}
	// Always bind the RT. This way special effect can use it.
	dev->PSSetShaderResource(3, rt);


	// GS

#if 0
	if (m_vt.m_primclass == GS_POINT_CLASS) {
		// Upscaling point will create aliasing because point has a size of 0 pixels.
		// This code tries to replace point with sprite. So a point in 4x will be replaced by
		// a 4x4 sprite.
		gs_sel.point = 1;
		// FIXME this formula is potentially wrong
		GSVector4 point_size = GSVector4(rtscale.x / rtsize.x, rtscale.y / rtsize.y) * 2.0f;
		vs_cb.TextureScale = vs_cb.TextureScale.xyxy(point_size);
	}
#endif
	gs_sel.sprite = m_vt.m_primclass == GS_SPRITE_CLASS;

	dev->SetupVS(vs_sel);
	dev->SetupGS(gs_sel);
	dev->SetupPS(ps_sel);

	// rs

	GSVector4i scissor = GSVector4i(GSVector4(rtscale).xyxy() * m_context->scissor.in).rintersect(GSVector4i(rtsize).zwxy());

	GL_PUSH("IA");
	SetupIA();
	GL_POP();

	dev->OMSetColorMaskState(om_csel);
	dev->SetupOM(om_dssel);

	dev->SetupCB(&vs_cb, &ps_cb);

	if (DATE_GL42) {
		GL_PUSH("Date GL42");
		// It could be good idea to use stencil in the same time.
		// Early stencil test will reduce the number of atomic-load operation

		// Create an r32i image that will contain primitive ID
		// Note: do it at the beginning because the clean will dirty the FBO state
		//dev->InitPrimDateTexture(rtsize.x, rtsize.y);

		// I don't know how much is it legal to mount rt as Texture/RT. No write is done.
		// In doubt let's detach RT.
		dev->OMSetRenderTargets(NULL, ds, &scissor);

		// Don't write anything on the color buffer
		// Neither in the depth buffer
		glDepthMask(false);
		// Compute primitiveID max that pass the date test
		SendDraw(false);

		// Ask PS to discard shader above the primitiveID max
		glDepthMask(GLState::depth_mask);

		ps_sel.date = 3;
		dev->SetupPS(ps_sel);

		// Be sure that first pass is finished !
		dev->Barrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		GL_POP();
	}

	if (ps_sel.hdr) {
		hdr_rt = dev->CreateTexture(rtsize.x, rtsize.y, GL_RGBA32F);

		dev->CopyRectConv(rt, hdr_rt, ComputeBoundingBox(rtscale, rtsize), false);

		dev->OMSetRenderTargets(hdr_rt, ds, &scissor);
	} else {
		dev->OMSetRenderTargets(rt, ds, &scissor);
	}

	if (m_context->TEST.DoFirstPass())
	{
		SendDraw(require_barrier);
	}

	if (m_context->TEST.DoSecondPass())
	{
		ASSERT(!m_env.PABE.PABE);

		static const uint32 iatst[] = {1, 0, 5, 6, 7, 2, 3, 4};

		ps_sel.atst = iatst[atst];
		if (spritehack && (ps_sel.atst == 2)) {
			ps_sel.atst = 1;
		}

		dev->SetupPS(ps_sel);

		bool z = om_dssel.zwe;
		bool r = om_csel.wr;
		bool g = om_csel.wg;
		bool b = om_csel.wb;
		bool a = om_csel.wa;

		switch(m_context->TEST.AFAIL)
		{
			case AFAIL_KEEP: z = r = g = b = a = false; break; // none
			case AFAIL_FB_ONLY: z = false; break; // rgba
			case AFAIL_ZB_ONLY: r = g = b = a = false; break; // z
			case AFAIL_RGB_ONLY: z = a = false; break; // rgb
			default: __assume(0);
		}

		if (z || r || g || b || a)
		{
			om_dssel.zwe = z;
			om_csel.wr = r;
			om_csel.wg = g;
			om_csel.wb = b;
			om_csel.wa = a;

			dev->OMSetColorMaskState(om_csel);
			dev->SetupOM(om_dssel);

			SendDraw(require_barrier);
		}
	}

	if (DATE_GL42) {
		dev->RecycleDateTexture();
	}

	dev->EndScene();

	// Warning: EndScene must be called before StretchRect otherwise
	// vertices will be overwritten. Trust me you don't want to do that.
	if (hdr_rt) {
		GSVector4 dRect(ComputeBoundingBox(rtscale, rtsize));
		GSVector4 sRect = dRect / GSVector4(rtsize.x, rtsize.y).xyxy();
		dev->StretchRect(hdr_rt, sRect, rt, dRect, 4, false);

		dev->Recycle(hdr_rt);
	}

	GL_POP();
}
