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
	m_pixelcenter = GSVector2(-0.5f, -0.5f);

	m_accurate_blend  = theApp.GetConfig("accurate_blend", 1);
	m_accurate_date   = theApp.GetConfig("accurate_date", 0);
	m_accurate_colclip = theApp.GetConfig("accurate_colclip", 0);

	UserHacks_AlphaHack      = theApp.GetConfig("UserHacks_AlphaHack", 0);
	UserHacks_AlphaStencil   = theApp.GetConfig("UserHacks_AlphaStencil", 0);
	UserHacks_TCOffset       = theApp.GetConfig("UserHacks_TCOffset", 0);
	UserHacks_TCO_x          = (UserHacks_TCOffset & 0xFFFF) / -1000.0f;
	UserHacks_TCO_y          = ((UserHacks_TCOffset >> 16) & 0xFFFF) / -1000.0f;

	if (!theApp.GetConfig("UserHacks", 0)) {
		UserHacks_AlphaHack      = false;
		UserHacks_AlphaStencil   = false;
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

bool GSRendererOGL::PrimitiveOverlap()
{
	if (m_vertex.next < 4)
		return false;

	if (m_vt.m_primclass != GS_SPRITE_CLASS)
		return true;

	// Check intersection of sprite primitive only
	size_t count = m_vertex.next;
	GSVertex* v = &m_vertex.buff[0];

	for(size_t i = 0; i < count; i += 2) {
		// Very bad code
		GSVector4i vi(v[i].XYZ.X, v[i].XYZ.Y, v[i+1].XYZ.X, v[i+1].XYZ.Y);
		for (size_t j = i+2; j < count; j += 2) {
			GSVector4i vj(v[j].XYZ.X, v[j].XYZ.Y, v[j+1].XYZ.X, v[j+1].XYZ.Y);
			GSVector4i inter = vi.rintersect(vj);
			if (!inter.rempty()) {
				//fprintf(stderr, "Overlap found between %d and %d (draw of %d vertices)\n", i, j, count);
				return true;
			}
		}
	}

	//fprintf(stderr, "Yes, code can be optimized (draw of %d vertices)\n", count);
	return false;
}

void GSRendererOGL::SendDraw(bool require_barrier)
{
	GSDeviceOGL* dev = (GSDeviceOGL*)m_dev;

	if (!require_barrier || !PrimitiveOverlap()) {
		dev->DrawIndexedPrimitive();
	} else {
		ASSERT(m_vt.m_primclass != GS_POINT_CLASS);
		ASSERT(m_vt.m_primclass != GS_LINE_CLASS);
		ASSERT(GLLoader::found_geometry_shader);

		// FIXME: Investigate: a dynamic check to pack as many primitives as possibles
		// I'm nearly sure GSdx already have this kind of code (maybe we can adapt GSDirtyRect)
		size_t nb_vertex = (m_vt.m_primclass == GS_TRIANGLE_CLASS) ? 3 : 2;

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
				rt ? rt->GetID() : -1, ds->GetID());

	GSDrawingEnvironment& env = m_env;
	GSDrawingContext* context = m_context;

	const GSVector2i& rtsize = ds->GetSize();
	const GSVector2& rtscale = ds->GetScale();

	bool DATE = m_context->TEST.DATE && context->FRAME.PSM != PSM_PSMCT24;
	bool DATE_GL42 = false;
	bool DATE_GL45 = false;

	bool require_barrier = false; // For blend (and maybe in date in the future)

	ASSERT(m_dev != NULL);

	GSDeviceOGL* dev = (GSDeviceOGL*)m_dev;
	dev->s_n = s_n;

	GSDeviceOGL::VSSelector vs_sel;
	GSDeviceOGL::VSConstantBuffer vs_cb;

	GSDeviceOGL::PSSelector ps_sel;
	GSDeviceOGL::PSConstantBuffer ps_cb;
	GSDeviceOGL::PSSamplerSelector ps_ssel;

	GSDeviceOGL::OMBlendSelector om_bsel;
	GSDeviceOGL::OMColorMaskSelector om_csel;
	GSDeviceOGL::OMDepthStencilSelector om_dssel;

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
		int tex_pos = v[0].U & 0xFF;
		ps_sel.read_ba = (tex_pos > 112 && tex_pos < 144);

		//GL_INS("First vertex is  P: %d => %d    T: %d => %d", v[0].XYZ.X, v[1].XYZ.X, v[0].U, v[1].U);

		// Convert the vertex info to a 32 bits color format equivalent
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

		// Please bang my head against the wall!
		// 1/ Reduce the frame mask to a 16 bit format
		const uint32& m = context->FRAME.FBMSK;
		uint32 fbmask = ((m >> 3) & 0x1F) | ((m >> 6) & 0x3E0) | ((m >> 9) & 0x7C00) | ((m >> 31) & 0x8000);
		om_csel.wrgba = 0;

		// 2 Select the new mask (Please someone put SSE here)
		if ((fbmask & 0xFF) == 0) {
			if (write_ba) {
				GL_INS("Color shuffle %s => B", ps_sel.read_ba ? "B" : "R");
				om_csel.wb = 1;
			} else {
				GL_INS("Color shuffle %s => R", ps_sel.read_ba ? "B" : "R");
				om_csel.wr = 1;
			}
		} else if ((fbmask & 0xFF) != 0xFF) {
			GL_INS("ERROR: not supported RG mask:%x", fbmask & 0xFF);
			ASSERT(0);
		}

		fbmask >>= 8;
		if ((fbmask & 0xFF) == 0) {
			if (write_ba) {
				GL_INS("Color shuffle %s => A", ps_sel.read_ba ? "A" : "G");
				om_csel.wa = 1;
			} else {
				GL_INS("Color shuffle %s => G", ps_sel.read_ba ? "A" : "G");
				om_csel.wg = 1;
			}
		} else if ((fbmask & 0xFF) != 0xFF) {
			GL_INS("ERROR: not supported BA mask:%x", fbmask & 0xFF);
			ASSERT(0);
		}

	} else {
		ps_sel.dfmt = GSLocalMemory::m_psm[context->FRAME.PSM].fmt;

		om_csel.wrgba = ~GSVector4i::load((int)context->FRAME.FBMSK).eq8(GSVector4i::xffffffff()).mask();

		{
#ifdef ENABLE_OGL_DEBUG
			uint8 r_mask = (context->FRAME.FBMSK >> 0)  & 0xFF;
			uint8 g_mask = (context->FRAME.FBMSK >> 8)  & 0xFF;
			uint8 b_mask = (context->FRAME.FBMSK >> 16) & 0xFF;
			uint8 a_mask = (context->FRAME.FBMSK >> 24) & 0xFF;
			uint8 bits = (GSLocalMemory::m_psm[context->FRAME.PSM].fmt == 2) ? 16 : 32;
			if (r_mask != 0 && r_mask != 0xFF) {
				GL_INS("ERROR: not supported r_mask:%x on %d bits format", r_mask, bits);
				ASSERT(0);
			}
			if (g_mask != 0 && g_mask != 0xFF) {
				GL_INS("ERROR: not supported g_mask:%x on %d bits format", g_mask, bits);
				ASSERT(0);
			}
			if (b_mask != 0 && b_mask != 0xFF) {
				GL_INS("ERROR: not supported b_mask:%x on %d bits format", b_mask, bits);
				ASSERT(0);
			}
			if (a_mask != 0 && a_mask != 0xFF) {
				GL_INS("ERROR: not supported a_mask:%x on %d bits format", a_mask, bits);
				ASSERT(0);
			}
#endif
		}
	}

	// Format of the output

	GIFRegALPHA ALPHA = context->ALPHA;
	float afix = (float)context->ALPHA.FIX / 0x80;

	// Blend

	if (!IsOpaque())
	{
		om_bsel.abe = PRIM->ABE || PRIM->AA1 && m_vt.m_primclass == GS_LINE_CLASS;

		om_bsel.a = ALPHA.A;
		om_bsel.b = ALPHA.B;
		om_bsel.c = ALPHA.C;
		om_bsel.d = ALPHA.D;

		if (env.PABE.PABE)
		{
			if (om_bsel.a == 0 && om_bsel.b == 1 && om_bsel.c == 0 && om_bsel.d == 1)
			{
				// this works because with PABE alpha blending is on when alpha >= 0x80, but since the pixel shader
				// cannot output anything over 0x80 (== 1.0) blending with 0x80 or turning it off gives the same result

				om_bsel.abe = 0;
			}
			else
			{
				//Breath of Fire Dragon Quarter triggers this in battles. Graphics are fine though.
				//ASSERT(0);
#ifdef ENABLE_OGL_DEBUG
				fprintf(stderr, "env PABE  not supported\n");
				GL_INS("!!! ENV PABE  not supported !!!");
#endif
			}
		}
	}

	if (ps_sel.dfmt == 1) {
		if (ALPHA.C == 1) {
			// 24 bits no alpha channel so use 1.0f fix factor as equivalent
			ALPHA.C = 2;
			afix = 1.0f;
		}
		// Disable writing of the alpha channel
		om_csel.wa = 0;
	}

	if (DATE) {
		if (GLLoader::found_GL_ARB_texture_barrier && !PrimitiveOverlap()) {
			DATE_GL45 = true;
			DATE = false;
		} else if (m_accurate_date && !UserHacks_AlphaStencil &&
				om_csel.wa && (!context->TEST.ATE || context->TEST.ATST == ATST_ALWAYS)) {
			// texture barrier will split the draw call into n draw call. It is very efficient for
			// few primitive draws. Otherwise it sucks.
			if (GLLoader::found_GL_ARB_texture_barrier && (m_index.tail < 100)) {
				require_barrier = true;
				DATE_GL45 = true;
				DATE = false;
			} else {
				DATE_GL42 = GLLoader::found_GL_ARB_shader_image_load_store;
			}
		}
	}

	// DATE

	if (DATE_GL45) {
		gl_TextureBarrier();
		dev->PSSetShaderResource(3, rt);
	} else if (DATE) {
		// TODO: do I need to clamp the value (if yes how? rintersect with rt?)
		GSVector4 si = GSVector4(rtscale.x, rtscale.y);
		GSVector4 off = GSVector4(-1.0f, 1.0f); // Round value
		GSVector4 b = m_vt.m_min.p.xyxy(m_vt.m_max.p) + off.xxyy();
		GSVector4i ri = GSVector4i(b * si.xyxy());

		// Reduce the quantity of clean function
		glScissor( ri.x, ri.y, ri.width(), ri.height() );
		GLState::scissor = ri;

		// Must be done here to avoid any GL state pertubation (clear function...)
		// Create an r32ui image that will containt primitive ID
		if (DATE_GL42) {
			dev->InitPrimDateTexture(rt);
			dev->PSSetShaderResource(3, rt);
		} else {
			GSVector4 s = GSVector4(rtscale.x / rtsize.x, rtscale.y / rtsize.y);

			GSVector4 src = (b * s.xyxy()).sat(off.zzyy());
			GSVector4 dst = src * 2.0f + off.xxxx();

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

	if (context->TEST.ZTE)
	{
		om_dssel.ztst = context->TEST.ZTST;
		om_dssel.zwe = !context->ZBUF.ZMSK;
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
		if (context->ZBUF.PSM == PSM_PSMZ24)
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
		else if (context->ZBUF.PSM == PSM_PSMZ16 || context->ZBUF.PSM == PSM_PSMZ16S)
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
	float ox = (float)(int)context->XYOFFSET.OFX;
	float oy = (float)(int)context->XYOFFSET.OFY;
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
		ps_sel.date = 5 + context->TEST.DATM;
	} else if (DATE) {
		if (DATE_GL42)
			ps_sel.date = 1 + context->TEST.DATM;
		else
			om_dssel.date = 1;
	}

	bool colclip_wrap = env.COLCLAMP.CLAMP == 0 && !tex && PRIM->PRIM != GS_POINTLIST && !m_accurate_colclip;
	bool acc_colclip_wrap = env.COLCLAMP.CLAMP == 0 && m_accurate_colclip;
	if (ALPHA.A == ALPHA.B) { // Optimize-away colclip
		// No addition neither substraction so no risk of overflow the [0:255] range.
		colclip_wrap = false;
		acc_colclip_wrap = false;
#ifdef ENABLE_OGL_DEBUG
		if (colclip_wrap || acc_colclip_wrap) {
			const char *col[3] = {"Cs", "Cd", "0"};
			GL_INS("COLCLIP: DISABLED: blending is a plain copy of %s", col[ALPHA.D]);
		}
#endif
	}
	if (colclip_wrap) {
		ps_sel.colclip = 1;
		GL_INS("COLCLIP ENABLED (blending is %d/%d/%d/%d)", ALPHA.A, ALPHA.B, ALPHA.C, ALPHA.D);
	} else if (acc_colclip_wrap) {
		ps_sel.colclip = 3;
		GL_INS("COLCLIP SW ENABLED (blending is %d/%d/%d/%d)", ALPHA.A, ALPHA.B, ALPHA.C, ALPHA.D);
	} else if (env.COLCLAMP.CLAMP == 0 && (ALPHA.A != ALPHA.B)) {
		GL_INS("COLCLIP NOT SUPPORTED (blending is %d/%d/%d/%d)", ALPHA.A, ALPHA.B, ALPHA.C, ALPHA.D);
	}

	ps_sel.fba = context->FBA.FBA;
	// TODO deprecat this stuff
	ps_sel.aout = context->FRAME.PSM == PSM_PSMCT16 || context->FRAME.PSM == PSM_PSMCT16S || (context->FRAME.FBMSK & 0xff000000) == 0x7f000000 ? 1 : 0;
	ps_sel.aout &= !ps_sel.shuffle;
		
	if (UserHacks_AlphaHack) ps_sel.aout = 1;

	if (PRIM->FGE)
	{
		ps_sel.fog = 1;

		ps_cb.FogColor_AREF = GSVector4::rgba32(env.FOGCOL.u32[0]) / 255;
	}

	if (context->TEST.ATE)
		ps_sel.atst = context->TEST.ATST;
	else
		ps_sel.atst = ATST_ALWAYS;

	if (context->TEST.ATE && context->TEST.ATST > 1)
		ps_cb.FogColor_AREF.a = (float)context->TEST.AREF;

	// Destination alpha pseudo stencil hack: use a stencil operation combined with an alpha test
	// to only draw pixels which would cause the destination alpha test to fail in the future once.
	// Unfortunately this also means only drawing those pixels at all, which is why this is a hack.
	// The interaction with FBA in D3D9 is probably less than ideal.
	if (UserHacks_AlphaStencil && DATE && dev->HasStencil() && om_csel.wa && (!context->TEST.ATE || context->TEST.ATST == ATST_ALWAYS))
	{
		if (!context->FBA.FBA)
		{
			if (context->TEST.DATM == 0)
				ps_sel.atst = ATST_GEQUAL; // >=
			else
				ps_sel.atst = ATST_LESS; // <
			ps_cb.FogColor_AREF.a = (float)0x80;
		}
		if (!(context->FBA.FBA && context->TEST.DATM == 1))
			om_dssel.alpha_stencil = 1;
	}

	// By default don't use texture
	ps_sel.tfx = 4;
	bool spritehack = false;
	int  atst = ps_sel.atst;

	if (tex)
	{
		const GSLocalMemory::psm_t &psm = GSLocalMemory::m_psm[context->TEX0.PSM];
		const GSLocalMemory::psm_t &cpsm = psm.pal > 0 ? GSLocalMemory::m_psm[context->TEX0.CPSM] : psm;
		bool bilinear = m_filter == 2 ? m_vt.IsLinear() : m_filter != 0;
		bool simple_sample = !tex->m_palette && cpsm.fmt == 0 && context->CLAMP.WMS < 3 && context->CLAMP.WMT < 3;
		// Don't force extra filtering on sprite (it creates various upscaling issue)
		bilinear &= !((m_vt.m_primclass == GS_SPRITE_CLASS) && m_userhacks_round_sprite_offset && !m_vt.IsLinear());

		ps_sel.wms = context->CLAMP.WMS;
		ps_sel.wmt = context->CLAMP.WMT;

		if (ps_sel.shuffle) {
			ps_sel.fmt = 0;
		} else if (tex->m_palette) {
			ps_sel.fmt = cpsm.fmt | 4;
			ps_sel.ifmt = !tex->m_target ? 0
				: (context->TEX0.PSM == PSM_PSMT4HL) ? 2
				: (context->TEX0.PSM == PSM_PSMT4HH) ? 1
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
		ps_sel.aem = env.TEXA.AEM;
		ps_sel.tfx = context->TEX0.TFX;
		ps_sel.tcc = context->TEX0.TCC;
		ps_sel.ltf = bilinear && !simple_sample;
		spritehack = tex->m_spritehack_t;
		// FIXME the ati is currently disabled on the shader. I need to find a .gs to test that we got same
		// bug on opengl
		// FIXME for the moment disable it on subroutine (it will kill my perf for nothings)
		//ps_sel.point_sampler = 0; // !(bilinear && simple_sample) && !GLLoader::found_GL_ARB_shader_subroutine;

		int w = tex->m_texture->GetWidth();
		int h = tex->m_texture->GetHeight();

		int tw = (int)(1 << context->TEX0.TW);
		int th = (int)(1 << context->TEX0.TH);

		GSVector4 WH(tw, th, w, h);

		if (PRIM->FST)
		{
			vs_cb.TextureScale = GSVector4(1.0f / 16) / WH.xyxy();
			ps_sel.fst = 1;
		}

		ps_cb.WH = WH;
		ps_cb.HalfTexel = GSVector4(-0.5f, 0.5f).xxyy() / WH.zwzw();
		ps_cb.MskFix = GSVector4i(context->CLAMP.MINU, context->CLAMP.MINV, context->CLAMP.MAXU, context->CLAMP.MAXV);

		// TC Offset Hack
		ps_sel.tcoffsethack = !!UserHacks_TCOffset;
		ps_cb.TC_OffsetHack = GSVector4(UserHacks_TCO_x, UserHacks_TCO_y).xyxy() / WH.xyxy();

		GSVector4 clamp(ps_cb.MskFix);
		GSVector4 ta(env.TEXA & GSVector4i::x000000ff());

		ps_cb.MinMax = clamp / WH.xyxy();
		ps_cb.MinF_TA = (clamp + 0.5f).xyxy(ta) / WH.xyxy(GSVector4(255, 255));

		ps_ssel.tau = (context->CLAMP.WMS + 3) >> 1;
		ps_ssel.tav = (context->CLAMP.WMT + 3) >> 1;
		ps_ssel.ltf = bilinear && simple_sample;

		// Setup Texture ressources
		if (GLLoader::found_GL_ARB_bindless_texture) {
			GLuint64 handle[2];
			handle[0] = tex->m_texture ? static_cast<GSTextureOGL*>(tex->m_texture)->GetHandle(dev->GetSamplerID(ps_ssel)): 0;
			handle[1] = tex->m_palette ? static_cast<GSTextureOGL*>(tex->m_palette)->GetHandle(dev->GetPaletteSamplerID()): 0;

			dev->m_shader->PS_ressources(handle);
		} else {
			dev->SetupSampler(ps_ssel);

			if (tex->m_palette) {
				dev->PSSetShaderResources(tex->m_texture, tex->m_palette);
			} else if (tex->m_texture) {
				dev->PSSetShaderResource(0, tex->m_texture);
#ifdef ENABLE_OGL_DEBUG
				// Unattach texture to avoid noise in debugger
				dev->PSSetShaderResource(1, NULL);
#endif
			}
		}

		if (spritehack && (ps_sel.atst == 2)) {
			ps_sel.atst = 1;
		}
	} else {
#ifdef ENABLE_OGL_DEBUG
		// Unattach texture to avoid noise in debugger
		dev->PSSetShaderResource(0, NULL);
		dev->PSSetShaderResource(1, NULL);
#endif
	}

	// Compute the blending equation to detect special case
	int blend_sel    = ((om_bsel.a * 3 + om_bsel.b) * 3 + om_bsel.c) * 3 + om_bsel.d;
	int bogus_blend  = GSDeviceOGL::m_blendMapD3D9[blend_sel].bogus;
	bool all_sw = !( (ALPHA.A == ALPHA.B) || (ALPHA.C == 2 && afix <= 1.002f) ) && (m_accurate_blend > 1);
	bool sw_blending = (m_accurate_blend && (bogus_blend & A_MAX)) || acc_colclip_wrap || all_sw;

	if (sw_blending && om_bsel.abe && rt) {
		GL_INS("!!! SW blending effect used (0x%x from sel %d) !!!", bogus_blend, blend_sel);

		// select a shader that support blending
		ps_sel.blend = bogus_blend & 0xFF;

		dev->PSSetShaderResource(3, rt);

		// Require the fix alpha vlaue
		if (ALPHA.C == 2) {
			ps_cb.AlphaCoeff.a = afix;
		}

		// No need to flush for every primitive
		require_barrier = !(bogus_blend & NO_BAR);
	} else {
		ps_sel.clr1 = om_bsel.IsCLR1();
	}

	// WARNING: setup of the program must be done first. So you can setup
	// 1/ subroutine uniform
	// 2/ bindless texture uniform
	// 3/ others uniform?
	dev->SetupVS(vs_sel);
	dev->SetupGS(m_vt.m_primclass == GS_SPRITE_CLASS);
	dev->SetupPS(ps_sel);

	// rs

	GSVector4i scissor = GSVector4i(GSVector4(rtscale).xyxy() * context->scissor.in).rintersect(GSVector4i(rtsize).zwxy());

	GL_PUSH("IA");
	SetupIA();
	GL_POP();

	dev->OMSetColorMaskState(om_csel);
	dev->SetupOM(om_dssel, om_bsel, afix, sw_blending);

	dev->SetupCB(&vs_cb, &ps_cb);

	if (DATE_GL42) {
		GL_PUSH("Date GL42");
		ASSERT((bogus_blend & A_MAX) == 0);
		// It could be good idea to use stencil in the same time.
		// Early stencil test will reduce the number of atomic-load operation

		// Create an r32i image that will contain primitive ID
		// Note: do it at the beginning because the clean will dirty the FBO state
		//dev->InitPrimDateTexture(rtsize.x, rtsize.y);

		// I don't know how much is it legal to mount rt as Texture/RT. No write is done.
		// In doubt let's detach RT.
		dev->OMSetRenderTargets(NULL, ds, &scissor);

		// Don't write anything on the color buffer
		dev->OMSetWriteBuffer(GL_NONE);
		// Compute primitiveID max that pass the date test
		SendDraw(false);

		// Ask PS to discard shader above the primitiveID max
		dev->OMSetWriteBuffer();

		ps_sel.date = 3;
		dev->SetupPS(ps_sel);

		// Be sure that first pass is finished !
		dev->Barrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

		GL_POP();
	}

	dev->OMSetRenderTargets(rt, ds, &scissor);

	if (context->TEST.DoFirstPass())
	{
		SendDraw(require_barrier);

		if (colclip_wrap)
		{
			ASSERT((bogus_blend & A_MAX) == 0);
			GL_PUSH("COLCLIP");
			GSDeviceOGL::OMBlendSelector om_bselneg(om_bsel);
			GSDeviceOGL::PSSelector ps_selneg(ps_sel);

			om_bselneg.negative = 1;
			ps_selneg.colclip = 2;

			dev->SetupOM(om_dssel, om_bselneg, afix);
			dev->SetupPS(ps_selneg);

			SendDraw(false);
			dev->SetupOM(om_dssel, om_bsel, afix);
			GL_POP();
		}
	}

	if (context->TEST.DoSecondPass())
	{
		ASSERT(!env.PABE.PABE);

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

		switch(context->TEST.AFAIL)
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
			dev->SetupOM(om_dssel, om_bsel, afix);

			SendDraw(require_barrier);

			if (colclip_wrap)
			{
				ASSERT((bogus_blend & A_MAX) == 0);
				GL_PUSH("COLCLIP");
				GSDeviceOGL::OMBlendSelector om_bselneg(om_bsel);
				GSDeviceOGL::PSSelector ps_selneg(ps_sel);

				om_bselneg.negative = 1;
				ps_selneg.colclip = 2;

				dev->SetupOM(om_dssel, om_bselneg, afix);
				dev->SetupPS(ps_selneg);

				SendDraw(false);
				GL_POP();
			}
		}
	}
	if (DATE_GL42)
		dev->RecycleDateTexture();

	dev->EndScene();

	GL_POP();
}
