/*
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
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
#include "GSRendererDX.h"
#include "GSDeviceDX.h"

GSRendererDX::GSRendererDX(GSTextureCache* tc, const GSVector2& pixelcenter)
	: GSRendererHW(tc)
	, m_pixelcenter(pixelcenter)
{
	m_logz = theApp.GetConfigB("logz");
	m_fba = theApp.GetConfigB("fba");

	if (theApp.GetConfigB("UserHacks")) {
		UserHacks_AlphaHack    = theApp.GetConfigB("UserHacks_AlphaHack");
		UserHacks_AlphaStencil = theApp.GetConfigB("UserHacks_AlphaStencil");
		UserHacks_TCOffset     = theApp.GetConfigI("UserHacks_TCOffset");
		UserHacks_HPO          = theApp.GetConfigI("UserHacks_HalfPixelOffset");
	} else {
		UserHacks_AlphaHack    = false;
		UserHacks_AlphaStencil = false;
		UserHacks_TCOffset     = 0;
		UserHacks_HPO          = 0;
	}

	UserHacks_TCO_x = (UserHacks_TCOffset & 0xFFFF) / -1000.0f;
	UserHacks_TCO_y = ((UserHacks_TCOffset >> 16) & 0xFFFF) / -1000.0f;
}

GSRendererDX::~GSRendererDX()
{
}

void GSRendererDX::EmulateAtst(const int pass, const GSTextureCache::Source* tex)
{
	static const uint32 inverted_atst[] = {ATST_ALWAYS, ATST_NEVER, ATST_GEQUAL, ATST_GREATER, ATST_NOTEQUAL, ATST_LESS, ATST_LEQUAL, ATST_EQUAL};
	int atst = (pass == 2) ? inverted_atst[m_context->TEST.ATST] : m_context->TEST.ATST;

	if (!m_context->TEST.ATE) return;

	switch (atst) {
		case ATST_LESS:
			if (tex && tex->m_spritehack_t) {
				m_ps_sel.atst = 0;
			} else {
				ps_cb.FogColor_AREF.a = (float)m_context->TEST.AREF - 0.1f;
				m_ps_sel.atst = 1;
			}
			break;
		case ATST_LEQUAL:
			ps_cb.FogColor_AREF.a = (float)m_context->TEST.AREF - 0.1f + 1.0f;
			m_ps_sel.atst = 1;
			break;
		case ATST_GEQUAL:
			// Maybe a -1 trick multiplication factor could be used to merge with ATST_LEQUAL case
			ps_cb.FogColor_AREF.a = (float)m_context->TEST.AREF - 0.1f;
			m_ps_sel.atst = 2;
			break;
		case ATST_GREATER:
			// Maybe a -1 trick multiplication factor could be used to merge with ATST_LESS case
			ps_cb.FogColor_AREF.a = (float)m_context->TEST.AREF - 0.1f + 1.0f;
			m_ps_sel.atst = 2;
			break;
		case ATST_EQUAL:
			ps_cb.FogColor_AREF.a = (float)m_context->TEST.AREF;
			m_ps_sel.atst = 3;
			break;
		case ATST_NOTEQUAL:
			ps_cb.FogColor_AREF.a = (float)m_context->TEST.AREF;
			m_ps_sel.atst = 4;
			break;

		case ATST_NEVER: // Draw won't be done so no need to implement it in shader
		case ATST_ALWAYS:
		default:
			m_ps_sel.atst = 0;
			break;
	}
}

void GSRendererDX::EmulateZbuffer()
{
	if (m_context->TEST.ZTE)
	{
		om_dssel.ztst = m_context->TEST.ZTST;
		om_dssel.zwe = !m_context->ZBUF.ZMSK;
	}
	else
	{
		om_dssel.ztst = ZTST_ALWAYS;
	}

	uint32 max_z;
	if (m_context->ZBUF.PSM == PSM_PSMZ32) {
		max_z = 0xFFFFFFFF;
	}
	else if (m_context->ZBUF.PSM == PSM_PSMZ24) {
		max_z = 0xFFFFFF;
	}
	else {
		max_z = 0xFFFF;
	}

	// The real GS appears to do no masking based on the Z buffer format and writing larger Z values
	// than the buffer supports seems to be an error condition on the real GS, causing it to crash.
	// We are probably receiving bad coordinates from VU1 in these cases.

	if (om_dssel.ztst >= ZTST_ALWAYS && om_dssel.zwe && (m_context->ZBUF.PSM != PSM_PSMZ32)) {
		if (m_vt.m_max.p.z > max_z) {
			ASSERT(m_vt.m_min.p.z > max_z); // sfex capcom logo
			// Fixme :Following conditional fixes some dialog frame in Wild Arms 3, but may not be what was intended.
			if (m_vt.m_min.p.z > max_z) {
#ifdef _DEBUG
				fprintf(stdout, "Bad Z size on %s buffers\n", psm_str(m_context->ZBUF.PSM));
#endif
				om_dssel.ztst = ZTST_ALWAYS;
			}
		}
	}

	GSVertex* v = &m_vertex.buff[0];
	// Minor optimization of a corner case (it allow to better emulate some alpha test effects)
	if (om_dssel.ztst == ZTST_GEQUAL && m_vt.m_eq.z && v[0].XYZ.Z == max_z) {
#ifdef _DEBUG
		fprintf(stdout, "Optimize Z test GEQUAL to ALWAYS (%s)\n", psm_str(m_context->ZBUF.PSM));
#endif
		om_dssel.ztst = ZTST_ALWAYS;
	}
}

void GSRendererDX::EmulateTextureSampler(const GSTextureCache::Source* tex)
{
	const GSLocalMemory::psm_t &psm = GSLocalMemory::m_psm[m_context->TEX0.PSM];
	const GSLocalMemory::psm_t &cpsm = psm.pal > 0 ? GSLocalMemory::m_psm[m_context->TEX0.CPSM] : psm;

	const uint8 wms = m_context->CLAMP.WMS;
	const uint8 wmt = m_context->CLAMP.WMT;
	bool complex_wms_wmt = !!((wms | wmt) & 2);

	bool bilinear = m_vt.IsLinear();
	bool shader_emulated_sampler = tex->m_palette || cpsm.fmt != 0 || complex_wms_wmt;

	// 1 and 0 are equivalent
	m_ps_sel.wms = (wms & 2) ? wms : 0;
	m_ps_sel.wmt = (wmt & 2) ? wmt : 0;

	int w = tex->m_texture->GetWidth();
	int h = tex->m_texture->GetHeight();

	int tw = (int)(1 << m_context->TEX0.TW);
	int th = (int)(1 << m_context->TEX0.TH);

	GSVector4 WH(tw, th, w, h);

	// Performance note:
	// 1/ Don't set 0 as it is the default value
	// 2/ Only keep aem when it is useful (avoid useless shader permutation)
	if (m_ps_sel.shuffle) {
		// Force a 32 bits access (normally shuffle is done on 16 bits)
		// m_ps_sel.tex_fmt = 0; // removed as an optimization
		m_ps_sel.aem = m_env.TEXA.AEM;
		ASSERT(tex->m_target);

		// Shuffle is a 16 bits format, so aem is always required
		GSVector4 ta(m_env.TEXA & GSVector4i::x000000ff());
		ps_cb.MinF_TA = (GSVector4(ps_cb.MskFix) + 0.5f).xyxy(ta) / WH.xyxy(GSVector4(255, 255));

		bilinear &= m_vt.IsLinear();

		GSVector4 half_offset = RealignTargetTextureCoordinate(tex);
		vs_cb.Texture_Scale_Offset.z = half_offset.x;
		vs_cb.Texture_Scale_Offset.w = half_offset.y;

	} else if (tex->m_target) {
		// Use an old target. AEM and index aren't resolved it must be done
		// on the GPU

		// Select the 32/24/16 bits color (AEM)
		m_ps_sel.fmt = cpsm.fmt;
		m_ps_sel.aem = m_env.TEXA.AEM;

		// Don't upload AEM if format is 32 bits
		if (cpsm.fmt) {
			GSVector4 ta(m_env.TEXA & GSVector4i::x000000ff());
			ps_cb.MinF_TA = (GSVector4(ps_cb.MskFix) + 0.5f).xyxy(ta) / WH.xyxy(GSVector4(255, 255));
		}

		// Select the index format
		if (tex->m_palette) {
			// FIXME Potentially improve fmt field in GSLocalMemory
			if (m_context->TEX0.PSM == PSM_PSMT4HL)
				m_ps_sel.fmt |= 1 << 2;
			else if (m_context->TEX0.PSM == PSM_PSMT4HH)
				m_ps_sel.fmt |= 2 << 2;
			else
				m_ps_sel.fmt |= 3 << 2;

			// Alpha channel of the RT is reinterpreted as an index. Star
			// Ocean 3 uses it to emulate a stencil buffer.  It is a very
			// bad idea to force bilinear filtering on it.
			bilinear &= m_vt.IsLinear();
		}

		GSVector4 half_offset = RealignTargetTextureCoordinate(tex);
		vs_cb.Texture_Scale_Offset.z = half_offset.x;
		vs_cb.Texture_Scale_Offset.w = half_offset.y;
	}
	else if (tex->m_palette) {
		// Use a standard 8 bits texture. AEM is already done on the CLUT
		// Therefore you only need to set the index
		// m_ps_sel.aem     = 0; // removed as an optimization

		// Note 4 bits indexes are converted to 8 bits
		m_ps_sel.fmt = 3 << 2;

	}
	else {
		// Standard texture. Both index and AEM expansion were already done by the CPU.
		// m_ps_sel.tex_fmt = 0; // removed as an optimization
		// m_ps_sel.aem     = 0; // removed as an optimization
	}


	if (m_context->TEX0.TFX == TFX_MODULATE && m_vt.m_eq.rgba == 0xFFFF && m_vt.m_min.c.eq(GSVector4i(128))) {
		// Micro optimization that reduces GPU load (removes 5 instructions on the FS program)
		m_ps_sel.tfx = TFX_DECAL;
	}
	else {
		m_ps_sel.tfx = m_context->TEX0.TFX;
	}

	m_ps_sel.tcc = m_context->TEX0.TCC;

	m_ps_sel.ltf = bilinear && shader_emulated_sampler;

	m_ps_sel.rt = tex->m_target;
	m_ps_sel.spritehack = tex->m_spritehack_t;
	m_ps_sel.point_sampler = !(bilinear && !shader_emulated_sampler);

	if (PRIM->FST)
	{
		GSVector4 TextureScale = GSVector4(0.0625f) / WH.xyxy();
		vs_cb.Texture_Scale_Offset.x = TextureScale.x;
		vs_cb.Texture_Scale_Offset.y = TextureScale.y;
		//Maybe better?
		//vs_cb.TextureScale = GSVector4(1.0f / 16) * GSVector4(tex->m_texture->GetScale()).xyxy() / WH.zwzw();
		m_ps_sel.fst = 1;
	}

	ps_cb.WH = WH;
	ps_cb.HalfTexel = GSVector4(-0.5f, 0.5f).xxyy() / WH.zwzw();
	if (complex_wms_wmt) {
		ps_cb.MskFix = GSVector4i(m_context->CLAMP.MINU, m_context->CLAMP.MINV, m_context->CLAMP.MAXU, m_context->CLAMP.MAXV);
		ps_cb.MinMax = GSVector4(ps_cb.MskFix) / WH.xyxy();
	}

	// TC Offset Hack
	m_ps_sel.tcoffsethack = !!UserHacks_TCOffset;
	ps_cb.TC_OffsetHack = GSVector4(UserHacks_TCO_x, UserHacks_TCO_y).xyxy() / WH.xyxy();

	m_ps_ssel.tau = (m_context->CLAMP.WMS + 3) >> 1;
	m_ps_ssel.tav = (m_context->CLAMP.WMT + 3) >> 1;
	m_ps_ssel.ltf = bilinear && !shader_emulated_sampler;
}

void GSRendererDX::DrawPrims(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* tex)
{
	const GSVector2i& rtsize = ds ? ds->GetSize()  : rt->GetSize();
	const GSVector2& rtscale = ds ? ds->GetScale() : rt->GetScale();

	DATE = m_context->TEST.DATE && m_context->FRAME.PSM != PSM_PSMCT24;

	bool ate_first_pass = m_context->TEST.DoFirstPass();
	bool ate_second_pass = m_context->TEST.DoSecondPass();

	m_gs_sel.key = 0;

	vs_cb.Texture_Scale_Offset = GSVector4(0.0f);

	GSTexture* rtcopy = NULL;

	ASSERT(m_dev != NULL);

	dev = (GSDeviceDX*)m_dev;

	// Channel shuffle effect not supported on DX. Let's keep the logic because it help to
	// reduce memory requirement (and why not a partial port)

	// Uncomment to disable (allow to trace the draw call)
	// m_channel_shuffle = false;

	if (m_channel_shuffle) {
		if (m_game.title == CRC::Tekken5) {
			if (m_context->FRAME.FBW == 1) {
				// Used in stages: Secret Garden, Acid Rain, Moonlit Wilderness
				// Skip channel effect, it misses a shader for proper screen effect but at least the top left screen issue isn't appearing anymore 
				// 12 pages: 2 calls by channel, 3 channels, 1 blit
				// Minus current draw call
				m_skip = 12 * (3 + 3 + 1) - 1;
			} else {
				// Could skip model drawing if wrongly detected
				m_channel_shuffle = false;
			}
		} else if (m_context->CLAMP.WMS == 3 && ((m_context->CLAMP.MAXU & 0x8) == 8)) {
			;
		} else if (m_context->CLAMP.WMS == 3 && ((m_context->CLAMP.MINU & 0x8) == 0)) {
			;
		} else {
			m_channel_shuffle = false;
		}
	}

	// Upscaling hack to avoid various line/grid issues
	MergeSprite(tex);

	if(DATE)
	{
		if(dev->HasStencil())
		{
			GSVector4 s = GSVector4(rtscale.x / rtsize.x, rtscale.y / rtsize.y);
			GSVector4 off = GSVector4(-1.0f, 1.0f);

			GSVector4 src = ((m_vt.m_min.p.xyxy(m_vt.m_max.p) + off.xxyy()) * s.xyxy()).sat(off.zzyy());
			GSVector4 dst = src * 2.0f + off.xxxx();

			GSVertexPT1 vertices[] =
			{
				{GSVector4(dst.x, -dst.y, 0.5f, 1.0f), GSVector2(src.x, src.y)},
				{GSVector4(dst.z, -dst.y, 0.5f, 1.0f), GSVector2(src.z, src.y)},
				{GSVector4(dst.x, -dst.w, 0.5f, 1.0f), GSVector2(src.x, src.w)},
				{GSVector4(dst.z, -dst.w, 0.5f, 1.0f), GSVector2(src.z, src.w)},
			};

			dev->SetupDATE(rt, ds, vertices, m_context->TEST.DATM);
		}
		else
		{
			rtcopy = dev->CreateRenderTarget(rtsize.x, rtsize.y, false, rt->GetFormat());

			// I'll use VertexTrace when I consider it more trustworthy

			dev->CopyRect(rt, rtcopy, GSVector4i(rtsize).zwxy());
		}
	}

	//

	dev->BeginScene();

	// om

	om_dssel.key = 0;

	EmulateZbuffer();

	if (m_fba)
	{
		om_dssel.fba = m_context->FBA.FBA;
	}

	om_bsel.key = 0;

	if (!IsOpaque())
	{
		om_bsel.abe = PRIM->ABE || PRIM->AA1 && m_vt.m_primclass == GS_LINE_CLASS;

		om_bsel.a = m_context->ALPHA.A;
		om_bsel.b = m_context->ALPHA.B;
		om_bsel.c = m_context->ALPHA.C;
		om_bsel.d = m_context->ALPHA.D;

		if (m_env.PABE.PABE)
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
			}
		}
	}

	om_bsel.wrgba = ~GSVector4i::load((int)m_context->FRAME.FBMSK).eq8(GSVector4i::xffffffff()).mask();

	// vs

	GSDeviceDX::VSSelector vs_sel;

	vs_sel.tme = PRIM->TME;
	vs_sel.fst = PRIM->FST;
	vs_sel.logz = dev->HasDepth32() ? 0 : m_logz ? 1 : 0;
	vs_sel.rtcopy = !!rtcopy;

	float sx = 2.0f * rtscale.x / (rtsize.x << 4);
	float sy = 2.0f * rtscale.y / (rtsize.y << 4);
	float ox = (float)(int)m_context->XYOFFSET.OFX;
	float oy = (float)(int)m_context->XYOFFSET.OFY;
	float ox2 = 2.0f * m_pixelcenter.x / rtsize.x;
	float oy2 = 2.0f * m_pixelcenter.y / rtsize.y;

	//This hack subtracts around half a pixel from OFX and OFY. (Cannot do this directly,
	//because DX10 and DX9 have a different pixel center.)
	//
	//The resulting shifted output aligns better with common blending / corona / blurring effects,
	//but introduces a few bad pixels on the edges.

	if(rt && rt->LikelyOffset && UserHacks_HPO == 1)
	{
		// DX9 has pixelcenter set to 0.0, so give it some value here

		if(m_pixelcenter.x == 0 && m_pixelcenter.y == 0) { ox2 = -0.0003f; oy2 = -0.0003f; }
		
		ox2 *= rt->OffsetHack_modx;
		oy2 *= rt->OffsetHack_mody;
	}

	vs_cb.VertexScale  = GSVector4(sx, -sy, ldexpf(1, -32), 0.0f);
	vs_cb.VertexOffset = GSVector4(ox * sx + ox2 + 1, -(oy * sy + oy2 + 1), 0.0f, -1.0f);

	// gs

	m_gs_sel.iip = PRIM->IIP;
	m_gs_sel.prim = m_vt.m_primclass;

	// ps

	m_ps_sel.key = 0;
	m_ps_ssel.key = 0;

	// Gregory: code is not yet ready so let's only enable it when
	// CRC is below the FULL level
	if (m_texture_shuffle && (m_crc_hack_level < CRCHackLevel::Full)) {
		m_ps_sel.shuffle = 1;
		m_ps_sel.fmt = 0;

		const GIFRegXYOFFSET& o = m_context->XYOFFSET;
		GSVertex* v = &m_vertex.buff[0];
		size_t count = m_vertex.next;

		// vertex position is 8 to 16 pixels, therefore it is the 16-31 bits of the colors
		int  pos = (v[0].XYZ.X - o.OFX) & 0xFF;
		bool write_ba = (pos > 112 && pos < 136);
		// Read texture is 8 to 16 pixels (same as above)
		int tex_pos = v[0].U & 0xFF;
		m_ps_sel.read_ba = (tex_pos > 112 && tex_pos < 144);

		GL_INS("Color shuffle %s => %s", m_ps_sel.read_ba ? "BA" : "RG", write_ba ? "BA" : "RG");

		// Convert the vertex info to a 32 bits color format equivalent
		for (size_t i = 0; i < count; i += 2) {
			if (write_ba)
				v[i].XYZ.X -= 128u;
			else
				v[i + 1].XYZ.X += 128u;

			if (m_ps_sel.read_ba)
				v[i].U -= 128u;
			else
				v[i + 1].U += 128u;

			// Height is too big (2x).
			int tex_offset = v[i].V & 0xF;
			GSVector4i offset(o.OFY, tex_offset, o.OFY, tex_offset);

			GSVector4i tmp(v[i].XYZ.Y, v[i].V, v[i + 1].XYZ.Y, v[i + 1].V);
			tmp = GSVector4i(tmp - offset).srl32(1) + offset;

			v[i].XYZ.Y = (uint16)tmp.x;
			v[i].V = (uint16)tmp.y;
			v[i + 1].XYZ.Y = (uint16)tmp.z;
			v[i + 1].V = (uint16)tmp.w;
		}

		// Please bang my head against the wall!
		// 1/ Reduce the frame mask to a 16 bit format
		const uint32& m = m_context->FRAME.FBMSK;
		uint32 fbmask = ((m >> 3) & 0x1F) | ((m >> 6) & 0x3E0) | ((m >> 9) & 0x7C00) | ((m >> 31) & 0x8000);
		om_bsel.wrgba = 0;

		// 2 Select the new mask (Please someone put SSE here)
		if ((fbmask & 0xFF) == 0) {
			if (write_ba)
				om_bsel.wb = 1;
			else
				om_bsel.wr = 1;
		}
		else if ((fbmask & 0xFF) != 0xFF) {
#ifdef _DEBUG
			fprintf(stderr, "Please fix me! wb %u wr %u\n", om_bsel.wb, om_bsel.wr);
#endif
			//ASSERT(0);
		}

		fbmask >>= 8;
		if ((fbmask & 0xFF) == 0) {
			if (write_ba)
				om_bsel.wa = 1;
			else
				om_bsel.wg = 1;
		}
		else if ((fbmask & 0xFF) != 0xFF) {
#ifdef _DEBUG
			fprintf(stderr, "Please fix me! wa %u wg %u\n", om_bsel.wa, om_bsel.wg);
#endif
			//ASSERT(0);
		}

	}
	else {
		//ps_sel.fmt = GSLocalMemory::m_psm[m_context->FRAME.PSM].fmt;

		om_bsel.wrgba = ~GSVector4i::load((int)m_context->FRAME.FBMSK).eq8(GSVector4i::xffffffff()).mask();
	}

	if(DATE)
	{
		if(dev->HasStencil())
		{
			om_dssel.date = 1;
		}
		else
		{
			m_ps_sel.date = 1 + m_context->TEST.DATM;
		}
	}

	if(m_env.COLCLAMP.CLAMP == 0 && /* hack */ !tex && PRIM->PRIM != GS_POINTLIST)
	{
		m_ps_sel.colclip = 1;
	}

	m_ps_sel.clr1 = om_bsel.IsCLR1();
	m_ps_sel.fba = m_context->FBA.FBA;
	m_ps_sel.aout = m_context->FRAME.PSM == PSM_PSMCT16 || m_context->FRAME.PSM == PSM_PSMCT16S || (m_context->FRAME.FBMSK & 0xff000000) == 0x7f000000 ? 1 : 0;
	m_ps_sel.aout &= !m_ps_sel.shuffle;
	if(UserHacks_AlphaHack) m_ps_sel.aout = 1;

	if(PRIM->FGE)
	{
		m_ps_sel.fog = 1;

		ps_cb.FogColor_AREF = GSVector4::rgba32(m_env.FOGCOL.u32[0]) / 255;
	}

	// Warning must be done after EmulateZbuffer
	// Depth test is always true so it can be executed in 2 passes (no order required) unlike color.
	// The idea is to compute first the color which is independent of the alpha test. And then do a 2nd
	// pass to handle the depth based on the alpha test.
	bool ate_RGBA_then_Z = false;
	bool ate_RGB_then_ZA = false;
	if (ate_first_pass & ate_second_pass) {
#ifdef _DEBUG
		fprintf(stdout, "Complex Alpha Test\n");
#endif
		bool commutative_depth = (om_dssel.ztst == ZTST_GEQUAL && m_vt.m_eq.z) || (om_dssel.ztst == ZTST_ALWAYS);
		bool commutative_alpha = (m_context->ALPHA.C != 1); // when either Alpha Src or a constant

		ate_RGBA_then_Z = (m_context->TEST.AFAIL == AFAIL_FB_ONLY) & commutative_depth;
		ate_RGB_then_ZA = (m_context->TEST.AFAIL == AFAIL_RGB_ONLY) & commutative_depth & commutative_alpha;
	}

	if (ate_RGBA_then_Z) {
#ifdef _DEBUG
		fprintf(stdout, "Alternate ATE handling: ate_RGBA_then_Z\n");
#endif
		// Render all color but don't update depth
		// ATE is disabled here
		om_dssel.zwe = false;
	} else if (ate_RGB_then_ZA) {
#ifdef _DEBUG
		fprintf(stdout, "Alternate ATE handling: ate_RGB_then_ZA\n");
#endif
		// Render RGB color but don't update depth/alpha
		// ATE is disabled here
		om_dssel.zwe = false;
		om_bsel.wa = false;
	} else {
		EmulateAtst(1, tex);
	}

	// Destination alpha pseudo stencil hack: use a stencil operation combined with an alpha test
	// to only draw pixels which would cause the destination alpha test to fail in the future once.
	// Unfortunately this also means only drawing those pixels at all, which is why this is a hack.
	// The interaction with FBA in D3D9 is probably less than ideal.
	if (UserHacks_AlphaStencil && DATE && dev->HasStencil() && om_bsel.wa && !m_context->TEST.ATE)
	{
		if (!m_context->FBA.FBA)
		{
			if (m_context->TEST.DATM == 0)
				m_ps_sel.atst = 2; // >=
			else {
				if (tex && tex->m_spritehack_t)
					m_ps_sel.atst = 0; // <
				else
					m_ps_sel.atst = 1; // <
			}
			ps_cb.FogColor_AREF.a = (float)0x80;
		}
		if (!(m_context->FBA.FBA && m_context->TEST.DATM == 1))
			om_dssel.alpha_stencil = 1;
	}

	if(tex)
	{
		EmulateTextureSampler(tex);
	}
	else
	{
		m_ps_sel.tfx = 4;
	}

	// rs

	GSVector4i scissor = GSVector4i(GSVector4(rtscale).xyxy() * m_context->scissor.in).rintersect(GSVector4i(rtsize).zwxy());

	dev->OMSetRenderTargets(rt, ds, &scissor);
	dev->PSSetShaderResource(0, tex ? tex->m_texture : NULL);
	dev->PSSetShaderResource(1, tex ? tex->m_palette : NULL);
	dev->PSSetShaderResource(2, rtcopy);

	uint8 afix = m_context->ALPHA.FIX;

	SetupIA(sx, sy);

	dev->SetupOM(om_dssel, om_bsel, afix);
	dev->SetupVS(vs_sel, &vs_cb);
	dev->SetupGS(m_gs_sel, &gs_cb);
	dev->SetupPS(m_ps_sel, &ps_cb, m_ps_ssel);

	// draw

	if (ate_first_pass)
	{
		dev->DrawIndexedPrimitive();

		if (m_env.COLCLAMP.CLAMP == 0 && /* hack */ !tex && PRIM->PRIM != GS_POINTLIST)
		{
			GSDeviceDX::OMBlendSelector om_bselneg(om_bsel);
			GSDeviceDX::PSSelector ps_selneg(m_ps_sel);

			om_bselneg.negative = 1;
			ps_selneg.colclip = 2;

			dev->SetupOM(om_dssel, om_bselneg, afix);
			dev->SetupPS(ps_selneg, &ps_cb, m_ps_ssel);

			dev->DrawIndexedPrimitive();
			dev->SetupOM(om_dssel, om_bsel, afix);
		}
	}

	if (ate_second_pass)
	{
		ASSERT(!m_env.PABE.PABE);

		if (ate_RGBA_then_Z | ate_RGB_then_ZA) {
			// Enable ATE as first pass to update the depth
			// of pixels that passed the alpha test
			EmulateAtst(1, tex);
		}
		else {
			// second pass will process the pixels that failed
			// the alpha test
			EmulateAtst(2, tex);
		}

		dev->SetupPS(m_ps_sel, &ps_cb, m_ps_ssel);

		bool z = om_dssel.zwe;
		bool r = om_bsel.wr;
		bool g = om_bsel.wg;
		bool b = om_bsel.wb;
		bool a = om_bsel.wa;

		switch(m_context->TEST.AFAIL)
		{
			case 0: z = r = g = b = a = false; break; // none
			case 1: z = false; break; // rgba
			case 2: r = g = b = a = false; break; // z
			case 3: z = a = false; break; // rgb
			default: __assume(0);
		}

		// Depth test should be disabled when depth writes are masked and similarly, Alpha test must be disabled
		// when writes to all of the alpha bits in the Framebuffer are masked.
		if (ate_RGBA_then_Z) {
			z = !m_context->ZBUF.ZMSK;
			r = g = b = a = false;
		} else if (ate_RGB_then_ZA) {
			z = !m_context->ZBUF.ZMSK;
			a = (m_context->FRAME.FBMSK & 0xFF000000) != 0xFF000000;
			r = g = b = false;
		}

		if(z || r || g || b || a)
		{
			om_dssel.zwe = z;
			om_bsel.wr = r;
			om_bsel.wg = g;
			om_bsel.wb = b;
			om_bsel.wa = a;

			dev->SetupOM(om_dssel, om_bsel, afix);

			dev->DrawIndexedPrimitive();

			if (m_env.COLCLAMP.CLAMP == 0 && /* hack */ !tex && PRIM->PRIM != GS_POINTLIST)
			{
				GSDeviceDX::OMBlendSelector om_bselneg(om_bsel);
				GSDeviceDX::PSSelector ps_selneg(m_ps_sel);

				om_bselneg.negative = 1;
				ps_selneg.colclip = 2;

				dev->SetupOM(om_dssel, om_bselneg, afix);
				dev->SetupPS(ps_selneg, &ps_cb, m_ps_ssel);

				dev->DrawIndexedPrimitive();
			}
		}
	}

	dev->EndScene();

	dev->Recycle(rtcopy);

	if(om_dssel.fba) UpdateFBA(rt);
}
