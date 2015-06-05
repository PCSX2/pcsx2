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
#include "GSRendererHW.h"

GSRendererHW::GSRendererHW(GSTextureCache* tc)
	: m_width(1280)
	, m_height(1024)
	, m_skip(0)
	, m_reset(false)
	, m_upscale_multiplier(1)
	, m_tc(tc)
{
	m_upscale_multiplier = theApp.GetConfig("upscale_multiplier", 1);
	m_userhacks_skipdraw = !!theApp.GetConfig("UserHacks", 0) ? theApp.GetConfig("UserHacks_SkipDraw", 0) : 0;
	m_userhacks_align_sprite_X = !!theApp.GetConfig("UserHacks_align_sprite_X", 0) && !!theApp.GetConfig("UserHacks", 0);
	m_userhacks_round_sprite_offset = !!theApp.GetConfig("UserHacks", 0) ? theApp.GetConfig("UserHacks_round_sprite_offset", 0) : 0;

	if (m_upscale_multiplier == 1 && !m_nativeres) { //Custom
		m_width = theApp.GetConfig("resx", m_width);
		m_height = theApp.GetConfig("resy", m_height);
	}

	if (m_upscale_multiplier == 1) {
		m_userhacks_round_sprite_offset = 0;
		m_userhacks_align_sprite_X = 0;
	}

}

void GSRendererHW::SetScaling() {

	m_buffer_size = max(m_context->FRAME.FBW * 64, m_regs->DISP[m_regs->PMODE.EN1 == 1 ? 0 : 1].DISPFB.FBW * 64);
	
	//Only increase the buffer size, don't make it smaller, it breaks games (GH3)

	if (!m_nativeres && m_width < (m_buffer_size * m_upscale_multiplier)){
		m_tc->RemovePartial();
	}
	else {
		return;
	}

	m_height = m_buffer_size < 1024 ? 512 : 1024;
	
	m_upscale_multiplier = theApp.GetConfig("upscale_multiplier", m_upscale_multiplier);

	if (m_upscale_multiplier == 1 && !m_nativeres) { //Custom
		m_width = theApp.GetConfig("resx", m_width);
		m_height = theApp.GetConfig("resy", m_height);
	}
		
	if (m_upscale_multiplier > 1)
	{
		if (m_upscale_multiplier > 6)
		{
			m_upscale_multiplier = 1; // use the normal upscale math
		}

		m_width = m_buffer_size * m_upscale_multiplier;
		m_height *= m_upscale_multiplier;
	}
	
	
	printf("Frame buffer size set to  %dx%d (%dx%d)\n", (m_width / m_upscale_multiplier), (m_height / m_upscale_multiplier), m_width, m_height);
}

GSRendererHW::~GSRendererHW()
{
	delete m_tc;
}

void GSRendererHW::SetGameCRC(uint32 crc, int options)
{
	GSRenderer::SetGameCRC(crc, options);

	m_hacks.SetGameCRC(m_game);
}

bool GSRendererHW::CanUpscale()
{
	if(m_hacks.m_cu && !(this->*m_hacks.m_cu)())
	{
		return false;
	}

	return !m_nativeres && m_regs->PMODE.EN != 0; // upscale ratio depends on the display size, with no output it may not be set correctly (ps2 logo to game transition)
}

int GSRendererHW::GetUpscaleMultiplier()
{
	return m_upscale_multiplier;
}

void GSRendererHW::Reset()
{
	// TODO: GSreset can come from the main thread too => crash
	// m_tc->RemoveAll();

	m_reset = true;

	GSRenderer::Reset();
}

void GSRendererHW::VSync(int field)
{
	//Check if the frame buffer width or display width has changed
	SetScaling();

	if(m_reset)
	{
		m_tc->RemoveAll();

		m_reset = false;
	}

	GSRenderer::VSync(field);

	m_tc->IncAge();

	m_skip = 0;
}

void GSRendererHW::ResetDevice()
{
	m_tc->RemoveAll();

	GSRenderer::ResetDevice();
}

GSTexture* GSRendererHW::GetOutput(int i)
{
	const GSRegDISPFB& DISPFB = m_regs->DISP[i].DISPFB;

	GIFRegTEX0 TEX0;

	TEX0.TBP0 = DISPFB.Block();
	TEX0.TBW = DISPFB.FBW;
	TEX0.PSM = DISPFB.PSM;

	// TRACE(_T("[%d] GetOutput %d %05x (%d)\n"), (int)m_perfmon.GetFrame(), i, (int)TEX0.TBP0, (int)TEX0.PSM);

	GSTexture* t = NULL;

	if(GSTextureCache::Target* rt = m_tc->LookupTarget(TEX0, m_width, m_height))
	{
		t = rt->m_texture;

		if(s_dump)
		{
			if(s_savef && s_n >= s_saven)
			{
				t->Save(root_hw + format("%05d_f%lld_fr%d_%05x_%d.bmp", s_n, m_perfmon.GetFrame(), i, (int)TEX0.TBP0, (int)TEX0.PSM));
			}
		}

		s_n++; // Alaways increment it
	}

	return t;
}

void GSRendererHW::InvalidateVideoMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r)
{
	// printf("[%d] InvalidateVideoMem %d,%d - %d,%d %05x (%d)\n", (int)m_perfmon.GetFrame(), r.left, r.top, r.right, r.bottom, (int)BITBLTBUF.DBP, (int)BITBLTBUF.DPSM);

	m_tc->InvalidateVideoMem(m_mem.GetOffset(BITBLTBUF.DBP, BITBLTBUF.DBW, BITBLTBUF.DPSM), r);
}

void GSRendererHW::InvalidateLocalMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r, bool clut)
{
	// printf("[%d] InvalidateLocalMem %d,%d - %d,%d %05x (%d)\n", (int)m_perfmon.GetFrame(), r.left, r.top, r.right, r.bottom, (int)BITBLTBUF.SBP, (int)BITBLTBUF.SPSM);

	if(clut) return; // FIXME
		
	m_tc->InvalidateLocalMem(m_mem.GetOffset(BITBLTBUF.SBP, BITBLTBUF.SBW, BITBLTBUF.SPSM), r);
}

int GSRendererHW::Interpolate_UV(float alpha, int t0, int t1)
{
	float t = (1.0f - alpha) * t0 + alpha * t1;
	return (int)t & ~0xF; // cheap rounding
}

float GSRendererHW::alpha0(int L, int X0, int X1)
{
	float x = (X0 + 15) & ~0xF; // Round up
	return (x - X0) / (float)L;
}

float GSRendererHW::alpha1(int L, int X0, int X1)
{
	float x = (X1 - 1) & ~0xF; // Round down. Note -1 because right pixel isn't included in primitive so 0x100 must return 0.
	return (x - X0) / (float)L;
}

template <bool linear>
void GSRendererHW::RoundSpriteOffset()
{
//#define DEBUG_U
//#define DEBUG_V
#if defined(DEBUG_V) || defined(DEBUG_U)
	bool debug = linear;
#endif
	size_t count = m_vertex.next;
	GSVertex* v = &m_vertex.buff[0];

	for(size_t i = 0; i < count; i += 2) {
		// Performance note: if it had any impact on perf, someone would port it to SSE (AKA GSVector)

		// Compute the coordinate of first and last texels (in native with a linear filtering)
		int   ox  = m_context->XYOFFSET.OFX;
		int   X0  = v[i].XYZ.X   - ox;
		int   X1  = v[i+1].XYZ.X - ox;
		int   Lx  = (v[i+1].XYZ.X - v[i].XYZ.X);
		float ax0 = alpha0(Lx, X0, X1);
		float ax1 = alpha1(Lx, X0, X1);
		int   tx0 = Interpolate_UV(ax0, v[i].U, v[i+1].U);
		int   tx1 = Interpolate_UV(ax1, v[i].U, v[i+1].U);
#ifdef DEBUG_U
		if (debug) {
			fprintf(stderr, "u0:%d and u1:%d\n", v[i].U, v[i+1].U);
			fprintf(stderr, "a0:%f and a1:%f\n", ax0, ax1);
			fprintf(stderr, "t0:%d and t1:%d\n", tx0, tx1);
		}
#endif

		int   oy  = m_context->XYOFFSET.OFY;
		int   Y0  = v[i].XYZ.Y   - oy;
		int   Y1  = v[i+1].XYZ.Y - oy;
		int   Ly  = (v[i+1].XYZ.Y - v[i].XYZ.Y);
		float ay0 = alpha0(Ly, Y0, Y1);
		float ay1 = alpha1(Ly, Y0, Y1);
		int   ty0 = Interpolate_UV(ay0, v[i].V, v[i+1].V);
		int   ty1 = Interpolate_UV(ay1, v[i].V, v[i+1].V);
#ifdef DEBUG_V
		if (debug) {
			fprintf(stderr, "v0:%d and v1:%d\n", v[i].V, v[i+1].V);
			fprintf(stderr, "a0:%f and a1:%f\n", ay0, ay1);
			fprintf(stderr, "t0:%d and t1:%d\n", ty0, ty1);
		}
#endif

#ifdef DEBUG_U
		if (debug)
			fprintf(stderr, "GREP_BEFORE %d => %d\n", v[i].U, v[i+1].U);
#endif
#ifdef DEBUG_V
		if (debug)
			fprintf(stderr, "GREP_BEFORE %d => %d\n", v[i].V, v[i+1].V);
#endif

#if 1
		// Use rounded value of the newly computed texture coordinate. It ensures
		// that sampling will remains inside texture boundary
		//
		// Note for bilinear: by definition it will never work correctly! A sligh modification
		// of interpolation migth trigger a discard (with alpha testing)
		// Let's use something simple that correct really bad case (for a couple of 2D games).
		// I hope it won't create too much glitches.
		if (linear) {
			int Lu = v[i+1].U - v[i].U;
			// Note 32 is based on taisho-mononoke
			if ((Lu > 0) && (Lu <= (Lx+32))) {
				v[i+1].U -= 8;
			}
		} else {
			if (tx0 <= tx1) {
				v[i].U   = tx0;
				v[i+1].U = tx1 + 16;
			} else {
				v[i].U   = tx0 + 15;
				v[i+1].U = tx1;
			}
		}
#endif
#if 1
		if (linear) {
			int Lv = v[i+1].V - v[i].V;
			if ((Lv > 0) && (Lv <= (Ly+32))) {
				v[i+1].V -= 8;
			}
		} else {
			if (ty0 <= ty1) {
				v[i].V   = ty0;
				v[i+1].V = ty1 + 16;
			} else {
				v[i].V   = ty0 + 15;
				v[i+1].V = ty1;
			}
		}
#endif

#ifdef DEBUG_U
		if (debug)
			fprintf(stderr, "GREP_AFTER %d => %d\n\n", v[i].U, v[i+1].U);
#endif
#ifdef DEBUG_V
		if (debug)
			fprintf(stderr, "GREP_AFTER %d => %d\n\n", v[i].V, v[i+1].V);
#endif

	}
}

void GSRendererHW::Draw()
{
	if(m_dev->IsLost() || GSRenderer::IsBadFrame(m_skip, m_userhacks_skipdraw)) {
		GL_INS("Warning skipping a draw call (%d)", s_n);
		s_n += 3; // Keep it sync with SW renderer
		return;
	}
	GL_PUSH("HW Draw %d", s_n);

	GSDrawingEnvironment& env = m_env;
	GSDrawingContext* context = m_context;

	// It is allowed to use the depth and rt at the same location. However at least 1 must
	// be disabled. GoW uses a Cd blending on a 24 bits buffer (no alpha)
	const bool no_rt = context->ALPHA.IsCd() && PRIM->ABE && (context->FRAME.PSM == 1);

	GIFRegTEX0 TEX0;

	TEX0.TBP0 = context->FRAME.Block();
	TEX0.TBW = context->FRAME.FBW;
	TEX0.PSM = context->FRAME.PSM;

	GSTextureCache::Target* rt = no_rt ? NULL : m_tc->LookupTarget(TEX0, m_width, m_height, GSTextureCache::RenderTarget, true);

	TEX0.TBP0 = context->ZBUF.Block();
	TEX0.TBW = context->FRAME.FBW;
	TEX0.PSM = context->ZBUF.PSM;

	GSTextureCache::Target* ds = m_tc->LookupTarget(TEX0, m_width, m_height, GSTextureCache::DepthStencil, context->DepthWrite());

	if((!rt && !no_rt) || !ds)
	{
		GL_POP();
		ASSERT(0);
		return;
	}

	GSTextureCache::Source* tex = NULL;

	if(PRIM->TME)
	{
#ifdef ENABLE_OGL_DEBUG
		if ((context->FRAME.Block() == context->TEX0.TBP0) && (m_vertex.next > 2)) {
			GL_INS("ERROR: Source and Target are the same!");
		}
#endif
		/*
		
		// m_tc->LookupSource will mess with the palette, should not, but we do this after, until it is sorted out

		if(GSLocalMemory::m_psm[context->TEX0.PSM].pal > 0)
		{
			m_mem.m_clut.Read32(context->TEX0, env.TEXA);
		}

		*/

		GSVector4i r;

		GetTextureMinMax(r, context->TEX0, context->CLAMP, m_vt.IsLinear());

		tex = m_tc->LookupSource(context->TEX0, env.TEXA, r);

		if(!tex) {
			GL_POP();
			return;
		}

		if(GSLocalMemory::m_psm[context->TEX0.PSM].pal > 0)
		{
			m_mem.m_clut.Read32(context->TEX0, env.TEXA);
		}
	}

	if(s_dump)
	{
		uint64 frame = m_perfmon.GetFrame();

		string s;

		if (s_n >= s_saven) {
			// Dump Register state
			s = format("%05d_context.txt", s_n);

			m_env.Dump(root_hw+s);
			m_context->Dump(root_hw+s);
		}

		if(s_savet && s_n >= s_saven && tex)
		{
			s = format("%05d_f%lld_tex_%05x_%d_%d%d_%02x_%02x_%02x_%02x.dds",
				s_n, frame, (int)context->TEX0.TBP0, (int)context->TEX0.PSM,
				(int)context->CLAMP.WMS, (int)context->CLAMP.WMT,
				(int)context->CLAMP.MINU, (int)context->CLAMP.MAXU,
				(int)context->CLAMP.MINV, (int)context->CLAMP.MAXV);

			tex->m_texture->Save(root_hw+s, true);

			if(tex->m_palette)
			{
				s = format("%05d_f%lld_tpx_%05x_%d.dds", s_n, frame, context->TEX0.CBP, context->TEX0.CPSM);

				tex->m_palette->Save(root_hw+s, true);
			}
		}

		s_n++;

		if(s_save && s_n >= s_saven)
		{
			s = format("%05d_f%lld_rt0_%05x_%d.bmp", s_n, frame, context->FRAME.Block(), context->FRAME.PSM);

			if (rt)
				rt->m_texture->Save(root_hw+s);
		}

		if(s_savez && s_n >= s_saven)
		{
			s = format("%05d_f%lld_rz0_%05x_%d.bmp", s_n, frame, context->ZBUF.Block(), context->ZBUF.PSM);

			ds->m_texture->Save(root_hw+s);
		}

		s_n++;

#ifdef ENABLE_OGL_DEBUG
	} else {
		s_n += 2;
#endif
	}

	if(m_hacks.m_oi && !(this->*m_hacks.m_oi)(NULL, ds->m_texture, tex))
	{
		s_n += 1; // keep counter sync
		GL_POP();
		return;
	}

	// skip alpha test if possible

	GIFRegTEST TEST = context->TEST;
	GIFRegFRAME FRAME = context->FRAME;
	GIFRegZBUF ZBUF = context->ZBUF;

	uint32 fm = context->FRAME.FBMSK;
	uint32 zm = context->ZBUF.ZMSK || context->TEST.ZTE == 0 ? 0xffffffff : 0;

	if(context->TEST.ATE && context->TEST.ATST != ATST_ALWAYS)
	{
		if(GSRenderer::TryAlphaTest(fm, zm))
		{
			context->TEST.ATST = ATST_ALWAYS;
		}
	}

	context->FRAME.FBMSK = fm;
	context->ZBUF.ZMSK = zm != 0;

	// A couple of hack to avoid upscaling issue. So far it seems to impacts mostly sprite
	if ((m_upscale_multiplier > 1) && (m_vt.m_primclass == GS_SPRITE_CLASS)) {
		size_t count = m_vertex.next;
		GSVertex* v = &m_vertex.buff[0];

		// Hack to avoid vertical black line in various games (ace combat/tekken)
		if (m_userhacks_align_sprite_X) {
			// Note for performance reason I do the check only once on the first
			// primitive
			int win_position = v[1].XYZ.X - context->XYOFFSET.OFX;
			const bool unaligned_position = ((win_position & 0xF) == 8);
			const bool unaligned_texture  = ((v[1].U & 0xF) == 0) && PRIM->FST; // I'm not sure this check is useful
			const bool hole_in_vertex = (count < 4) || (v[1].XYZ.X != v[2].XYZ.X);
			if (hole_in_vertex && unaligned_position && (unaligned_texture || !PRIM->FST)) {
				// Normaly vertex are aligned on full pixels and texture in half
				// pixels. Let's extend the coverage of an half-pixel to avoid
				// hole after upscaling
				for(size_t i = 0; i < count; i += 2) {
					v[i+1].XYZ.X += 8;
					// I really don't know if it is a good idea. Neither what to do for !PRIM->FST
					if (unaligned_texture)
						v[i+1].U += 8;
				}
			}
		}

		if (PRIM->FST) {
			if ((m_userhacks_round_sprite_offset > 1) || (m_userhacks_round_sprite_offset == 1 && !m_vt.IsLinear())) {
				if (m_vt.IsLinear())
					RoundSpriteOffset<true>();
				else
					RoundSpriteOffset<false>();
			}
		} else {
			; // vertical line in Yakuza (note check m_userhacks_align_sprite_X behavior)
		}
	}

	//

	DrawPrims(rt ? rt->m_texture : NULL, ds->m_texture, tex);

	//

	context->TEST = TEST;
	context->FRAME = FRAME;
	context->ZBUF = ZBUF;

	//

	GSVector4i r = GSVector4i(m_vt.m_min.p.xyxy(m_vt.m_max.p)).rintersect(GSVector4i(context->scissor.in));

	if(fm != 0xffffffff && rt)
	{
		rt->m_valid = rt->m_valid.runion(r);

		m_tc->InvalidateVideoMem(context->offset.fb, r, false);

		m_tc->InvalidateVideoMemType(GSTextureCache::DepthStencil, context->FRAME.Block());
	}

	if(zm != 0xffffffff)
	{
		ds->m_valid = ds->m_valid.runion(r);

		m_tc->InvalidateVideoMem(context->offset.zb, r, false);

		m_tc->InvalidateVideoMemType(GSTextureCache::RenderTarget, context->ZBUF.Block());
	}

	//

	if(m_hacks.m_oo)
	{
		(this->*m_hacks.m_oo)();
	}

	if(s_dump)
	{
		uint64 frame = m_perfmon.GetFrame();

		string s;

		if(s_save && s_n >= s_saven)
		{
			s = format("%05d_f%lld_rt1_%05x_%d.bmp", s_n, frame, context->FRAME.Block(), context->FRAME.PSM);

			if (rt)
				rt->m_texture->Save(root_hw+s);
		}

		if(s_savez && s_n >= s_saven)
		{
			s = format("%05d_f%lld_rz1_%05x_%d.bmp", s_n, frame, context->ZBUF.Block(), context->ZBUF.PSM);

			ds->m_texture->Save(root_hw+s);
		}

		s_n++;

		if ((s_n - s_saven) > s_savel) {
			s_dump = 0;
		}
#ifdef ENABLE_OGL_DEBUG
	} else {
		s_n += 1;
#endif
	}

	#ifdef DISABLE_HW_TEXTURE_CACHE
	
	if (rt)
		m_tc->Read(rt, r);

	#endif

	GL_POP();
}

// hacks

GSRendererHW::Hacks::Hacks()
	: m_oi_map(m_oi_list)
	, m_oo_map(m_oo_list)
	, m_cu_map(m_cu_list)
	, m_oi(NULL)
	, m_oo(NULL)
	, m_cu(NULL)
{
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::FFXII, CRC::EU, &GSRendererHW::OI_FFXII));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::FFX, CRC::RegionCount, &GSRendererHW::OI_FFX));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::MetalSlug6, CRC::RegionCount, &GSRendererHW::OI_MetalSlug6));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::GodOfWar2, CRC::RegionCount, &GSRendererHW::OI_GodOfWar2));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::SimpsonsGame, CRC::RegionCount, &GSRendererHW::OI_SimpsonsGame));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::RozenMaidenGebetGarden, CRC::RegionCount, &GSRendererHW::OI_RozenMaidenGebetGarden));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::SpidermanWoS, CRC::RegionCount, &GSRendererHW::OI_SpidermanWoS));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::TyTasmanianTiger, CRC::RegionCount, &GSRendererHW::OI_TyTasmanianTiger));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::TyTasmanianTiger2, CRC::RegionCount, &GSRendererHW::OI_TyTasmanianTiger));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::DigimonRumbleArena2, CRC::RegionCount, &GSRendererHW::OI_DigimonRumbleArena2));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::StarWarsForceUnleashed, CRC::RegionCount, &GSRendererHW::OI_StarWarsForceUnleashed));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::BlackHawkDown, CRC::RegionCount, &GSRendererHW::OI_BlackHawkDown));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::XmenOriginsWolverine, CRC::RegionCount, &GSRendererHW::OI_XmenOriginsWolverine));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::CallofDutyFinalFronts, CRC::RegionCount, &GSRendererHW::OI_CallofDutyFinalFronts));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::SpyroNewBeginning, CRC::RegionCount, &GSRendererHW::OI_SpyroNewBeginning));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::SpyroEternalNight, CRC::RegionCount, &GSRendererHW::OI_SpyroEternalNight));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::TalesOfLegendia, CRC::RegionCount, &GSRendererHW::OI_TalesOfLegendia));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::SMTNocturne, CRC::RegionCount, &GSRendererHW::OI_SMTNocturne));

	m_oo_list.push_back(HackEntry<OO_Ptr>(CRC::DBZBT2, CRC::RegionCount, &GSRendererHW::OO_DBZBT2));
	m_oo_list.push_back(HackEntry<OO_Ptr>(CRC::MajokkoALaMode2, CRC::RegionCount, &GSRendererHW::OO_MajokkoALaMode2));

	m_cu_list.push_back(HackEntry<CU_Ptr>(CRC::DBZBT2, CRC::RegionCount, &GSRendererHW::CU_DBZBT2));
	m_cu_list.push_back(HackEntry<CU_Ptr>(CRC::MajokkoALaMode2, CRC::RegionCount, &GSRendererHW::CU_MajokkoALaMode2));
	m_cu_list.push_back(HackEntry<CU_Ptr>(CRC::TalesOfAbyss, CRC::RegionCount, &GSRendererHW::CU_TalesOfAbyss));
}

void GSRendererHW::Hacks::SetGameCRC(const CRC::Game& game)
{
	uint32 hash = (uint32)((game.region << 24) | game.title);

	m_oi = m_oi_map[hash];
	m_oo = m_oo_map[hash];
	m_cu = m_cu_map[hash];

	if(game.flags & CRC::PointListPalette)
	{
		ASSERT(m_oi == NULL);

		m_oi = &GSRendererHW::OI_PointListPalette;
	}
}

// OI (others input?/implementation?) hacks replace current draw call

bool GSRendererHW::OI_FFXII(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	static uint32* video = NULL;
	static size_t lines = 0;

	if(lines == 0)
	{
		if(m_vt.m_primclass == GS_LINE_CLASS && (m_vertex.next == 448 * 2 || m_vertex.next == 512 * 2))
		{
			lines = m_vertex.next / 2;
		}
	}
	else
	{
		if(m_vt.m_primclass == GS_POINT_CLASS)
		{
			if(m_vertex.next >= 16 * 512)
			{
				// incoming pixels are stored in columns, one column is 16x512, total res 448x512 or 448x454

				if(!video) video = new uint32[512 * 512];

				int ox = m_context->XYOFFSET.OFX - 8;
				int oy = m_context->XYOFFSET.OFY - 8;

				const GSVertex* RESTRICT v = m_vertex.buff;

				for(int i = (int)m_vertex.next; i > 0; i--, v++)
				{
					int x = (v->XYZ.X - ox) >> 4;
					int y = (v->XYZ.Y - oy) >> 4;
					
					if (x < 0 || x >= 448 || y < 0 || y >= (int)lines) return false; // le sigh
					
					video[(y << 8) + (y << 7) + (y << 6) + x] = v->RGBAQ.u32[0];
				}

				return false;
			}
			else
			{
				lines = 0;
			}
		}
		else if(m_vt.m_primclass == GS_LINE_CLASS)
		{
			if(m_vertex.next == lines * 2)
			{
				// normally, this step would copy the video onto screen with 512 texture mapped horizontal lines,
				// but we use the stored video data to create a new texture, and replace the lines with two triangles

				m_dev->Recycle(t->m_texture);

				t->m_texture = m_dev->CreateTexture(512, 512);

				t->m_texture->Update(GSVector4i(0, 0, 448, lines), video, 448 * 4);

				m_vertex.buff[2] = m_vertex.buff[m_vertex.next - 2];
				m_vertex.buff[3] = m_vertex.buff[m_vertex.next - 1];

				m_index.buff[0] = 0;
				m_index.buff[1] = 1;
				m_index.buff[2] = 2;
				m_index.buff[3] = 1;
				m_index.buff[4] = 2;
				m_index.buff[5] = 3;

				m_vertex.head = m_vertex.tail = m_vertex.next = 4;
				m_index.tail = 6;

				m_vt.Update(m_vertex.buff, m_index.buff, m_index.tail, GS_TRIANGLE_CLASS);
			}
			else
			{
				lines = 0;
			}
		}
	}

	return true;
}

bool GSRendererHW::OI_FFX(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	uint32 FBP = m_context->FRAME.Block();
	uint32 ZBP = m_context->ZBUF.Block();
	uint32 TBP = m_context->TEX0.TBP0;

	if((FBP == 0x00d00 || FBP == 0x00000) && ZBP == 0x02100 && PRIM->TME && TBP == 0x01a00 && m_context->TEX0.PSM == PSM_PSMCT16S)
	{
		// random battle transition (z buffer written directly, clear it now)

		m_dev->ClearDepth(ds, 0);
	}

	return true;
}

bool GSRendererHW::OI_MetalSlug6(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	// missing red channel fix (looks alright in pcsx2 r5000+)

	GSVertex* RESTRICT v = m_vertex.buff;

	for(int i = (int)m_vertex.next; i > 0; i--, v++)
	{
		uint32 c = v->RGBAQ.u32[0];

		uint32 r = (c >> 0) & 0xff;
		uint32 g = (c >> 8) & 0xff;
		uint32 b = (c >> 16) & 0xff;

		if(r == 0 && g != 0 && b != 0)
		{
			v->RGBAQ.u32[0] = (c & 0xffffff00) | ((g + b + 1) >> 1);
		}
	}

	m_vt.Update(m_vertex.buff, m_index.buff, m_index.tail, m_vt.m_primclass);
	
	return true;
}

bool GSRendererHW::OI_GodOfWar2(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	uint32 FBP = m_context->FRAME.Block();
	uint32 FBW = m_context->FRAME.FBW;
	uint32 FPSM = m_context->FRAME.PSM;

	if((FBP == 0x00f00 || FBP == 0x00100 || FBP == 0x01280) && FPSM == PSM_PSMZ24) // ntsc 0xf00, pal 0x100, ntsc "HD" 0x1280
	{
		// z buffer clear

		GIFRegTEX0 TEX0;

		TEX0.TBP0 = FBP;
		TEX0.TBW = FBW;
		TEX0.PSM = FPSM;

		if(GSTextureCache::Target* ds = m_tc->LookupTarget(TEX0, m_width, m_height, GSTextureCache::DepthStencil, true))
		{
			m_dev->ClearDepth(ds->m_texture, 0);
		}

		return false;
	}

	return true;
}

bool GSRendererHW::OI_SimpsonsGame(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	uint32 FBP = m_context->FRAME.Block();
	uint32 FPSM = m_context->FRAME.PSM;

	if((FBP == 0x01500 || FBP == 0x01800) && FPSM == PSM_PSMZ24)	//0x1800 pal, 0x1500 ntsc
	{
		// instead of just simply drawing a full height 512x512 sprite to clear the z buffer,
		// it uses a 512x256 sprite only, yet it is still able to fill the whole surface with zeros,
		// how? by using a render target that overlaps with the lower half of the z buffer...
		
		// TODO: tony hawk pro skater 4 same problem, the empty half is not visible though, painted over fully

		m_dev->ClearDepth(ds, 0);

		return false;
	}

	return true;
}

bool GSRendererHW::OI_RozenMaidenGebetGarden(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	if(!PRIM->TME)
	{
		uint32 FBP = m_context->FRAME.Block();
		uint32 ZBP = m_context->ZBUF.Block();

		if(FBP == 0x008c0 && ZBP == 0x01a40)
		{
			//  frame buffer clear, atst = fail, afail = write z only, z buffer points to frame buffer

			GIFRegTEX0 TEX0;

			TEX0.TBP0 = ZBP;
			TEX0.TBW = m_context->FRAME.FBW;
			TEX0.PSM = m_context->FRAME.PSM;

			if(GSTextureCache::Target* rt = m_tc->LookupTarget(TEX0, m_width, m_height, GSTextureCache::RenderTarget, true))
			{
				m_dev->ClearRenderTarget(rt->m_texture, 0);
			}

			return false;
		}
		else if(FBP == 0x00000 && m_context->ZBUF.Block() == 0x01180)
		{
			// z buffer clear, frame buffer now points to the z buffer (how can they be so clever?)

			GIFRegTEX0 TEX0;

			TEX0.TBP0 = FBP;
			TEX0.TBW = m_context->FRAME.FBW;
			TEX0.PSM = m_context->ZBUF.PSM;

			if(GSTextureCache::Target* ds = m_tc->LookupTarget(TEX0, m_width, m_height, GSTextureCache::DepthStencil, true))
			{
				m_dev->ClearDepth(ds->m_texture, 0);
			}

			return false;
		}
	}

	return true;
}

bool GSRendererHW::OI_SpidermanWoS(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	uint32 FBP = m_context->FRAME.Block();
	uint32 FPSM = m_context->FRAME.PSM;

	if((FBP == 0x025a0 || FBP == 0x02800) && FPSM == PSM_PSMCT32)	//0x2800 pal, 0x25a0 ntsc
	{
		//only top half of the screen clears
		m_dev->ClearDepth(ds, 0);
	}

	return true;
}

bool GSRendererHW::OI_TyTasmanianTiger(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	uint32 FBP = m_context->FRAME.Block();
	uint32 FPSM = m_context->FRAME.PSM;

	if((FBP == 0x02800 || FBP == 0x02BC0) && FPSM == PSM_PSMCT24)	//0x2800 pal, 0x2bc0 ntsc
	{
		//half height buffer clear
		m_dev->ClearDepth(ds, 0);

		return false;
	}

	return true;
}

bool GSRendererHW::OI_DigimonRumbleArena2(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	uint32 FBP = m_context->FRAME.Block();
	uint32 FPSM = m_context->FRAME.PSM;

	if(!PRIM->TME)
	{
		if((FBP == 0x02300 || FBP == 0x03fc0) && FPSM == PSM_PSMCT32)
		{
			//half height buffer clear
			m_dev->ClearDepth(ds, 0);
		}
	}

	return true;
}

bool GSRendererHW::OI_BlackHawkDown(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	uint32 FBP = m_context->FRAME.Block();
	uint32 FPSM = m_context->FRAME.PSM;

	if(FBP == 0x02000 && FPSM == PSM_PSMZ24)
	{
		//half height buffer clear
		m_dev->ClearDepth(ds, 0);

		return false;
	}

	return true;
}

bool GSRendererHW::OI_StarWarsForceUnleashed(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	uint32 FBP = m_context->FRAME.Block();
	uint32 FPSM = m_context->FRAME.PSM;

	if(!PRIM->TME)
	{
		if(FPSM == PSM_PSMCT24 && FBP == 0x2bc0)
		{
			m_dev->ClearDepth(ds, 0);

			return false;
		}
	}
	else if(PRIM->TME)
	{
		if((FBP == 0x0 || FBP == 0x01180) && FPSM == PSM_PSMCT32 && (m_vt.m_eq.z && m_vt.m_max.p.z == 0))
		{
			m_dev->ClearDepth(ds, 0);
		}
	}

	return true;
}

bool GSRendererHW::OI_XmenOriginsWolverine(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	uint32 FBP = m_context->FRAME.Block();
	uint32 FPSM = m_context->FRAME.PSM;

	if(FBP == 0x0 && FPSM == PSM_PSMCT16)
	{
		//half height buffer clear
		m_dev->ClearDepth(ds, 0);
	}

	return true;
}

bool GSRendererHW::OI_CallofDutyFinalFronts(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	uint32 FBP = m_context->FRAME.Block();
	uint32 FPSM = m_context->FRAME.PSM;

	if(FBP == 0x02300 && FPSM == PSM_PSMZ24)
	{
		//half height buffer clear
		m_dev->ClearDepth(ds, 0);

		return false;
	}

	return true;
}

bool GSRendererHW::OI_SpyroNewBeginning(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	uint32 FBP = m_context->FRAME.Block();
	uint32 FPSM = m_context->FRAME.PSM;

	if(!PRIM->TME)
	{
		if(FPSM == PSM_PSMCT24 && (FBP == 0x02800 || FBP == 0x02bc0))	//0x2800 pal, 0x2bc0 ntsc
		{
			//half height buffer clear
			m_dev->ClearDepth(ds, 0);

			return false;
		}
	}
	else if(PRIM->TME)
	{
		if((FBP == 0x0 || FBP == 0x01180) && FPSM == PSM_PSMCT32 && (m_vt.m_eq.z && m_vt.m_min.p.z == 0))
		{
			m_dev->ClearDepth(ds, 0);
		}
	}

	return true;
}

bool GSRendererHW::OI_SpyroEternalNight(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	uint32 FBP = m_context->FRAME.Block();
	uint32 FPSM = m_context->FRAME.PSM;

	if(!PRIM->TME)
	{
		if(FPSM == PSM_PSMCT24 && FBP == 0x2bc0)
		{
			//half height buffer clear
			m_dev->ClearDepth(ds, 0);

			return false;
		}
	}
	else if(PRIM->TME)
	{
		if((FBP == 0x0 || FBP == 0x01180) && FPSM == PSM_PSMCT32 && (m_vt.m_eq.z && m_vt.m_min.p.z == 0))
		{
			m_dev->ClearDepth(ds, 0);
		}
	}

	return true;
}

bool GSRendererHW::OI_TalesOfLegendia(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	uint32 FBP = m_context->FRAME.Block();
	uint32 FPSM = m_context->FRAME.PSM;

	if (FPSM == PSM_PSMCT32 && FBP == 0x01c00 && !m_context->TEST.ATE && m_vt.m_eq.z)
	{
		m_context->TEST.ZTST = ZTST_ALWAYS;
		//m_dev->ClearDepth(ds, 0);
	}

	return true;
}

bool GSRendererHW::OI_SMTNocturne(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	uint32 FBMSK = m_context->FRAME.FBMSK;
	uint32 FBP = m_context->FRAME.Block();
	uint32 FBW = m_context->FRAME.FBW;
	uint32 FPSM = m_context->FRAME.PSM;

	if(FBMSK == 16777215 && m_vertex.head != 2 && m_vertex.tail != 4 && m_vertex.next != 4)
	{

		GIFRegTEX0 TEX0;

		TEX0.TBP0 = FBP;
		TEX0.TBW = FBW;
		TEX0.PSM = FPSM;
		if (GSTextureCache::Target* ds = m_tc->LookupTarget(TEX0, m_width, m_height, GSTextureCache::DepthStencil, true))
		{
			m_dev->ClearDepth(ds->m_texture, 0);
		}
		return false;
	}

	return true;
}

bool GSRendererHW::OI_PointListPalette(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	if(m_vt.m_primclass == GS_POINT_CLASS && !PRIM->TME)
	{
		uint32 FBP = m_context->FRAME.Block();
		uint32 FBW = m_context->FRAME.FBW;

		if(FBP >= 0x03f40 && (FBP & 0x1f) == 0)
		{
			if(m_vertex.next == 16)
			{
				GSVertex* RESTRICT v = m_vertex.buff;

				for(int i = 0; i < 16; i++, v++)
				{
					uint32 c = v->RGBAQ.u32[0];
					uint32 a = c >> 24;

					c = (a >= 0x80 ? 0xff000000 : (a << 25)) | (c & 0x00ffffff);

					v->RGBAQ.u32[0] = c;

					m_mem.WritePixel32(i & 7, i >> 3, c, FBP, FBW);
				}

				m_mem.m_clut.Invalidate();

				return false;
			}
			else if(m_vertex.next == 256)
			{
				GSVertex* RESTRICT v = m_vertex.buff;

				for(int i = 0; i < 256; i++, v++)
				{
					uint32 c = v->RGBAQ.u32[0];
					uint32 a = c >> 24;

					c = (a >= 0x80 ? 0xff000000 : (a << 25)) | (c & 0x00ffffff);

					v->RGBAQ.u32[0] = c;

					m_mem.WritePixel32(i & 15, i >> 4, c, FBP, FBW);
				}

				m_mem.m_clut.Invalidate();

				return false;
			}
			else
			{
				ASSERT(0);
			}
		}
	}

	return true;
}

// OO (others output?) hacks: invalidate extra local memory after the draw call

void GSRendererHW::OO_DBZBT2()
{
	// palette readback (cannot detect yet, when fetching the texture later)

	uint32 FBP = m_context->FRAME.Block();
	uint32 TBP0 = m_context->TEX0.TBP0;

	if(PRIM->TME && (FBP == 0x03c00 && TBP0 == 0x03c80 || FBP == 0x03ac0 && TBP0 == 0x03b40))
	{
		GIFRegBITBLTBUF BITBLTBUF;

		BITBLTBUF.SBP = FBP;
		BITBLTBUF.SBW = 1;
		BITBLTBUF.SPSM = PSM_PSMCT32;

		InvalidateLocalMem(BITBLTBUF, GSVector4i(0, 0, 64, 64));
	}
}

void GSRendererHW::OO_MajokkoALaMode2()
{
	// palette readback

	uint32 FBP = m_context->FRAME.Block();

	if(!PRIM->TME && FBP == 0x03f40)
	{
		GIFRegBITBLTBUF BITBLTBUF;

		BITBLTBUF.SBP = FBP;
		BITBLTBUF.SBW = 1;
		BITBLTBUF.SPSM = PSM_PSMCT32;

		InvalidateLocalMem(BITBLTBUF, GSVector4i(0, 0, 16, 16));
	}
}

// Can Upscale hacks: disable upscaling for some draw calls

bool GSRendererHW::CU_DBZBT2()
{
	// palette should stay 64 x 64

	uint32 FBP = m_context->FRAME.Block();

	return FBP != 0x03c00 && FBP != 0x03ac0;
}

bool GSRendererHW::CU_MajokkoALaMode2()
{
	// palette should stay 16 x 16

	uint32 FBP = m_context->FRAME.Block();

	return FBP != 0x03f40;
}

bool GSRendererHW::CU_TalesOfAbyss()
{
	// full image blur and brightening

	uint32 FBP = m_context->FRAME.Block();

	return FBP != 0x036e0 && FBP != 0x03560 && FBP != 0x038e0;
}
