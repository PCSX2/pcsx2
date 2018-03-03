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
	: m_width(default_rt_size.x)
	, m_height(default_rt_size.y)
	, m_custom_width(1024)
	, m_custom_height(1024)
	, m_reset(false)
	, m_upscale_multiplier(1)
	, m_tc(tc)
	, m_channel_shuffle(false)
	, m_lod(GSVector2i(0,0))
{
	m_mipmap = theApp.GetConfigI("mipmap_hw");
	m_upscale_multiplier = theApp.GetConfigI("upscale_multiplier");
	m_large_framebuffer  = theApp.GetConfigB("large_framebuffer");
	if (theApp.GetConfigB("UserHacks")) {
		m_userhacks_align_sprite_X       = theApp.GetConfigB("UserHacks_align_sprite_X");
		m_userhacks_round_sprite_offset  = theApp.GetConfigI("UserHacks_round_sprite_offset");
		m_userhacks_disable_gs_mem_clear = theApp.GetConfigB("UserHacks_DisableGsMemClear");
		m_userHacks_HPO                  = theApp.GetConfigI("UserHacks_HalfPixelOffset");
		m_userHacks_merge_sprite         = theApp.GetConfigB("UserHacks_merge_pp_sprite");
	} else {
		m_userhacks_align_sprite_X       = false;
		m_userhacks_round_sprite_offset  = 0;
		m_userhacks_disable_gs_mem_clear = false;
		m_userHacks_HPO                  = 0;
		m_userHacks_merge_sprite         = false;
	}

	if (!m_upscale_multiplier) { //Custom Resolution
		m_custom_width = m_width = theApp.GetConfigI("resx");
		m_custom_height = m_height = theApp.GetConfigI("resy");
	}

	if (m_upscale_multiplier == 1) { // hacks are only needed for upscaling issues.
		m_userhacks_round_sprite_offset  = 0;
		m_userhacks_align_sprite_X       = false;
		m_userHacks_merge_sprite         = false;
	}

	m_dump_root = root_hw;
}

void GSRendererHW::SetScaling()
{
	if (!m_upscale_multiplier)
	{
		CustomResolutionScaling();
		return;
	}

	GSVector2i crtc_size(GetDisplayRect().width(), GetDisplayRect().height());

	// Details of (potential) perf impact of a big framebuffer
	// 1/ extra memory
	// 2/ texture cache framebuffer rescaling/copy
	// 3/ upload of framebuffer (preload hack)
	// 4/ framebuffer clear (color/depth/stencil)
	// 5/ read back of the frambuffer
	// 6/ MSAA
	//
	// With the solution
	// 1/ Nothing to do.Except the texture cache bug (channel shuffle effect)
	//    most of the market is 1GB of VRAM (and soon 2GB)
	// 2/ limit rescaling/copy to the valid data of the framebuffer
	// 3/ ??? no solution so far
	// 4a/ stencil can be limited to valid data.
	// 4b/ is it useful to clear color? depth? (in any case, it ought to be few operation)
	// 5/ limit the read to the valid data
	// 6/ not support on openGL

	// Framebuffer width is always a multiple of 64 so at certain cases it can't cover some weird width values.
	// 480P , 576P use width as 720 which is not referencable by FBW * 64. so it produces 704 ( the closest value multiple by 64).
	// In such cases, let's just use the CRTC width.
	int fb_width = std::max({ (int)m_context->FRAME.FBW * 64, crtc_size.x , 512 });
	// GS doesn't have a specific register for the FrameBuffer height. so we get the height
	// from physical units of the display rectangle in case the game uses a heigher value of height.
	//
	// Gregory: the framebuffer must have enough room to draw
	// * at least 2 frames such as FMV (see OI_BlitFMV)
	// * high resolution game such as snowblind engine game
	//
	// Autodetection isn't a good idea because it will create flickering
	// If memory consumption is an issue, there are 2 possibilities
	// * 1/ Avoid to create hundreds of RT
	// * 2/ Use sparse texture (requires recent HW)
	//
	// Avoid to alternate between 640x1280 and 1280x1024 on snow blind engine game
	// int fb_height = (fb_width < 1024) ? 1280 : 1024;
	//
	// Until performance issue is properly fixed, let's keep an option to reduce the framebuffer size.
	int fb_height = m_large_framebuffer ? 1280 :
		(fb_width < 1024) ? std::max(512, crtc_size.y) : 1024;

	int upscaled_fb_w = fb_width * m_upscale_multiplier;
	int upscaled_fb_h = fb_height * m_upscale_multiplier;
	bool good_rt_size = m_width >= upscaled_fb_w && m_height >= upscaled_fb_h;

	// No need to resize for native/custom resolutions as default size will be enough for native and we manually get RT Buffer size for custom.
	// don't resize until the display rectangle and register states are stabilized.
	if ( m_upscale_multiplier <= 1 || good_rt_size)
		return;

	m_tc->RemovePartial();
	m_width = upscaled_fb_w;
	m_height = upscaled_fb_h;
	printf("Frame buffer size set to  %dx%d (%dx%d)\n", fb_width, fb_height , m_width, m_height);
}

void GSRendererHW::CustomResolutionScaling()
{
	const int crtc_width = GetDisplayRect().width();
	const int crtc_height = GetDisplayRect().height();
	const float scaling_ratio = std::ceil(static_cast<float>(m_custom_height) / crtc_height);
	// Avoid using a scissor value which is too high, developers can even leave the scissor to max (2047)
	// at some cases when they don't want to limit the rendering size. Our assumption is that developers
	// set the scissor to the actual data in the buffer. Let's use the scissoring value only at such cases
	const int scissor_height = std::min(640, static_cast<int>(m_context->SCISSOR.SCAY1 - m_context->SCISSOR.SCAY0) + 1);
	const int single_buffer_size = std::max(GetDisplayRect().height(), scissor_height);

	// We have two contexts of framebuffer height -
	// One for lower memory consumption and low accuracy
	// Another one for higher memory consumption at necessary scenarios for higher accuracy
	std::array<int, 2> framebuffer_height;
	// When Large Framebuffer is disabled - Let's only consider the height of the display rectangle
	// as the base (This is wrong implementation when we consider it theoretically as CRTC has no relation
	// to the Framebuffer size)
	framebuffer_height[0] = static_cast<int>(std::round((crtc_height * scaling_ratio)));
	// When Large Framebuffer is enabled - We also consider for potential scissor sizes which are around
	// the size of the actual image data stored. (Helps ICO to properly scale to right size by help of the
	// scissoring values) Display rectangle has a height of 256 but scissor has a height of 512 which seems to
	// be the real buffer size.
	framebuffer_height[1] = static_cast<int>(std::round(single_buffer_size * scaling_ratio));

	if (m_width >= m_custom_width && m_height >= framebuffer_height[m_large_framebuffer])
		return;

	m_tc->RemovePartial();
	m_width = std::max(m_width, default_rt_size.x);
	m_height = std::max(framebuffer_height[m_large_framebuffer], default_rt_size.y);

	std::string overhead = std::to_string(framebuffer_height[1] - framebuffer_height[0]);
	std::string message = "(Custom resolution) Framebuffer size set to " + std::to_string(crtc_width) + "x" + std::to_string(crtc_height);
	message += " (" + std::to_string(m_width) + "x" + std::to_string(m_height) + ")\n";

	if (m_large_framebuffer)
	{
		message += "Additional " + overhead + " pixels overhead by enabling Large Framebuffer\n";
	}
	else
	{
		message += "Saved " + overhead + " pixels overhead by disabling Large Framebuffer\n";
	}
	printf("%s", message.c_str());
}

GSRendererHW::~GSRendererHW()
{
	delete m_tc;
}

void GSRendererHW::SetGameCRC(uint32 crc, int options)
{
	GSRenderer::SetGameCRC(crc, options);

	m_hacks.SetGameCRC(m_game);

	// Code for Automatic Mipmapping. Relies on game CRCs.
	if (theApp.GetConfigT<HWMipmapLevel>("mipmap_hw") == HWMipmapLevel::Automatic) {
		switch (CRC::Lookup(crc).title) {
		case CRC::AceCombatZero:
		case CRC::AceCombat4:
		case CRC::AceCombat5:
		case CRC::BrianLaraInternationalCricket:
		case CRC::DarkCloud:
		case CRC::DestroyAllHumans:
		case CRC::DestroyAllHumans2:
		case CRC::FIFA03:
		case CRC::FIFA04:
		case CRC::FIFA05:
		case CRC::SoulReaver2:
		case CRC::LegacyOfKainDefiance:
		case CRC::RatchetAndClank:
		case CRC::RatchetAndClank2:
		case CRC::RatchetAndClank3:
		case CRC::RatchetAndClank4:
		case CRC::RatchetAndClank5:
		case CRC::RickyPontingInternationalCricket:
		case CRC::TombRaiderAnniversary:
		case CRC::TribesAerialAssault:
		case CRC::Whiplash:
			m_mipmap = static_cast<int>(HWMipmapLevel::Basic);
			break;
		default:
			m_mipmap = static_cast<int>(HWMipmapLevel::Off);
			break;
		}
	}
}

bool GSRendererHW::CanUpscale()
{
	if(m_hacks.m_cu && !(this->*m_hacks.m_cu)())
	{
		return false;
	}

	return m_upscale_multiplier!=1 && m_regs->PMODE.EN != 0; // upscale ratio depends on the display size, with no output it may not be set correctly (ps2 logo to game transition)
}

int GSRendererHW::GetUpscaleMultiplier()
{
	return m_upscale_multiplier;
}

GSVector2i GSRendererHW::GetCustomResolution()
{
	return GSVector2i(m_custom_width, m_custom_height);
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

	m_tc->PrintMemoryUsage();
	m_dev->PrintMemoryUsage();

	m_skip = 0;
}

void GSRendererHW::ResetDevice()
{
	m_tc->RemoveAll();

	GSRenderer::ResetDevice();
}

GSTexture* GSRendererHW::GetOutput(int i, int& y_offset)
{
	const GSRegDISPFB& DISPFB = m_regs->DISP[i].DISPFB;

	GIFRegTEX0 TEX0;

	TEX0.TBP0 = DISPFB.Block();
	TEX0.TBW = DISPFB.FBW;
	TEX0.PSM = DISPFB.PSM;

	// TRACE(_T("[%d] GetOutput %d %05x (%d)\n"), (int)m_perfmon.GetFrame(), i, (int)TEX0.TBP0, (int)TEX0.PSM);

	GSTexture* t = NULL;

	if(GSTextureCache::Target* rt = m_tc->LookupTarget(TEX0, m_width, m_height, GetFramebufferHeight()))
	{
		t = rt->m_texture;

		int delta = TEX0.TBP0 - rt->m_TEX0.TBP0;
		if (delta > 0 && DISPFB.FBW != 0) {
			int pages = delta >> 5u;
			int y_pages = pages / DISPFB.FBW;
			y_offset = y_pages * GSLocalMemory::m_psm[DISPFB.PSM].pgs.y;
			GL_CACHE("Frame y offset %d pixels, unit %d", y_offset, i);
		}

#ifdef ENABLE_OGL_DEBUG
		if(s_dump)
		{
			if(s_savef && s_n >= s_saven)
			{
				t->Save(m_dump_root + format("%05d_f%lld_fr%d_%05x_%s.bmp", s_n, m_perfmon.GetFrame(), i, (int)TEX0.TBP0, psm_str(TEX0.PSM)));
			}
		}
#endif
	}

	return t;
}

GSTexture* GSRendererHW::GetFeedbackOutput()
{
	GIFRegTEX0 TEX0;

	TEX0.TBP0 = m_regs->EXTBUF.EXBP;
	TEX0.TBW = m_regs->EXTBUF.EXBW;
	TEX0.PSM = m_regs->DISP[m_regs->EXTBUF.FBIN & 1].DISPFB.PSM;

	GSTextureCache::Target* rt = m_tc->LookupTarget(TEX0, m_width, m_height, /*GetFrameRect(i).bottom*/0);

	GSTexture* t = rt->m_texture;

#ifdef ENABLE_OGL_DEBUG
	if(s_dump && s_savef && s_n >= s_saven)
		t->Save(m_dump_root + format("%05d_f%lld_fr%d_%05x_%s.bmp", s_n, m_perfmon.GetFrame(), 3, (int)TEX0.TBP0, psm_str(TEX0.PSM)));
#endif

	return t;
}

void GSRendererHW::Lines2Sprites()
{
	ASSERT(m_vt.m_primclass == GS_SPRITE_CLASS);

	// each sprite converted to quad needs twice the space

	while (m_vertex.tail * 2 > m_vertex.maxcount)
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

		for (; i >= 0; i -= 4, s -= 2, q -= 4, index -= 6)
		{
			GSVertex v0 = s[0];
			GSVertex v1 = s[1];

			v0.RGBAQ = v1.RGBAQ;
			v0.XYZ.Z = v1.XYZ.Z;
			v0.FOG = v1.FOG;

			if (PRIM->TME && !PRIM->FST) {
				GSVector4 st0 = GSVector4::loadl(&v0.ST.u64);
				GSVector4 st1 = GSVector4::loadl(&v1.ST.u64);
				GSVector4 Q = GSVector4(v1.RGBAQ.Q, v1.RGBAQ.Q, v1.RGBAQ.Q, v1.RGBAQ.Q);
				GSVector4 st = st0.upld(st1) / Q;

				GSVector4::storel(&v0.ST.u64, st);
				GSVector4::storeh(&v1.ST.u64, st);

				v0.RGBAQ.Q = 1.0f;
				v1.RGBAQ.Q = 1.0f;
			}

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

GSVector4 GSRendererHW::RealignTargetTextureCoordinate(const GSTextureCache::Source* tex)
{
	if (m_userHacks_HPO <= 1 || GetUpscaleMultiplier() == 1) return GSVector4(0.0f);

	GSVertex* v             = &m_vertex.buff[0];
	const GSVector2& scale  = tex->m_texture->GetScale();
	bool  linear            = m_vt.IsRealLinear();
	int t_position          = v[0].U;
	GSVector4 half_offset(0.0f);

	// FIXME Let's start with something wrong same mess on X and Y
	// FIXME Maybe it will be enough to check linear

	if (PRIM->FST) {

		if (m_userHacks_HPO == 3) {
			if (!linear && t_position == 8) {
				half_offset.x = 8;
				half_offset.y = 8;
			} else if (linear && t_position == 16) {
				half_offset.x = 16;
				half_offset.y = 16;
			} else if (m_vt.m_min.p.x == -0.5f) {
				half_offset.x = 8;
				half_offset.y = 8;
			}
		} else {
			if (!linear && t_position == 8) {
				half_offset.x = 8 - 8 / scale.x;
				half_offset.y = 8 - 8 / scale.y;
			} else if (linear && t_position == 16) {
				half_offset.x = 16 - 16 / scale.x;
				half_offset.y = 16 - 16 / scale.y;
			} else if (m_vt.m_min.p.x == -0.5f) {
				half_offset.x = 8;
				half_offset.y = 8;
			}
		}

		GL_INS("offset detected %f,%f t_pos %d (linear %d, scale %f)",
				half_offset.x, half_offset.y, t_position, linear, scale.x);

	} else if (m_vt.m_eq.q) {
		float tw = (float)(1 << m_context->TEX0.TW);
		float th = (float)(1 << m_context->TEX0.TH);
		float q  = v[0].RGBAQ.Q;

		// Tales of Abyss
		half_offset.x = 0.5f * q / tw;
		half_offset.y = 0.5f * q / th;

		GL_INS("ST offset detected %f,%f (linear %d, scale %f)",
				half_offset.x, half_offset.y, linear, scale.x);

	}

	return half_offset;
}

void GSRendererHW::MergeSprite(GSTextureCache::Source* tex)
{
	// Upscaling hack to avoid various line/grid issues
	if (m_userHacks_merge_sprite && tex && tex->m_target && (m_vt.m_primclass == GS_SPRITE_CLASS)) {
		if (PRIM->FST && GSLocalMemory::m_psm[tex->m_TEX0.PSM].fmt < 2 && ((m_vt.m_eq.value & 0xCFFFF) == 0xCFFFF)) {

			// Ideally the hack ought to be enabled in a true paving mode only. I don't know how to do it accurately
			// neither in a fast way. So instead let's just take the hypothesis that all sprites must have the same
			// size.
			// Tested on Tekken 5.
			GSVertex* v = &m_vertex.buff[0];
			bool is_paving = true;
			// SSE optimization: shuffle m[1] to have (4*32 bits) X, Y, U, V
			int first_dpX = v[1].XYZ.X - v[0].XYZ.X;
			int first_dpU = v[1].U - v[0].U;
			for (size_t i = 0; i < m_vertex.next; i += 2) {
				int dpX = v[i + 1].XYZ.X - v[i].XYZ.X;
				int dpU = v[i + 1].U - v[i].U;
				if (dpX != first_dpX || dpU != first_dpU) {
					is_paving = false;
					break;
				}
			}

#if 0
			GSVector4 delta_p = m_vt.m_max.p - m_vt.m_min.p;
			GSVector4 delta_t = m_vt.m_max.t - m_vt.m_min.t;
			bool is_blit = PrimitiveOverlap() == PRIM_OVERLAP_NO;
			GL_INS("PP SAMPLER: Dp %f %f Dt %f %f. Is blit %d, is paving %d, count %d", delta_p.x, delta_p.y, delta_t.x, delta_t.y, is_blit, is_paving, m_vertex.tail);
#endif

			if (is_paving) {
				// Replace all sprite with a single fullscreen sprite.
				GSVertex* s = &m_vertex.buff[0];

				s[0].XYZ.X = static_cast<uint16>((16.0f * m_vt.m_min.p.x) + m_context->XYOFFSET.OFX);
				s[1].XYZ.X = static_cast<uint16>((16.0f * m_vt.m_max.p.x) + m_context->XYOFFSET.OFX);
				s[0].XYZ.Y = static_cast<uint16>((16.0f * m_vt.m_min.p.y) + m_context->XYOFFSET.OFY);
				s[1].XYZ.Y = static_cast<uint16>((16.0f * m_vt.m_max.p.y) + m_context->XYOFFSET.OFY);

				s[0].U = static_cast<uint16>(16.0f * m_vt.m_min.t.x);
				s[0].V = static_cast<uint16>(16.0f * m_vt.m_min.t.y);
				s[1].U = static_cast<uint16>(16.0f * m_vt.m_max.t.x);
				s[1].V = static_cast<uint16>(16.0f * m_vt.m_max.t.y);

				m_vertex.head = m_vertex.tail = m_vertex.next = 2;
				m_index.tail = 2;
			}
		}
	}
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

uint16 GSRendererHW::Interpolate_UV(float alpha, int t0, int t1)
{
	float t = (1.0f - alpha) * t0 + alpha * t1;
	return (uint16)t & ~0xF; // cheap rounding
}

float GSRendererHW::alpha0(int L, int X0, int X1)
{
	int x = (X0 + 15) & ~0xF; // Round up
	return float(x - X0) / (float)L;
}

float GSRendererHW::alpha1(int L, int X0, int X1)
{
	int x = (X1 - 1) & ~0xF; // Round down. Note -1 because right pixel isn't included in primitive so 0x100 must return 0.
	return float(x - X0) / (float)L;
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
		int	   ox  = m_context->XYOFFSET.OFX;
		int    X0  = v[i].XYZ.X   - ox;
		int	   X1  = v[i+1].XYZ.X - ox;
		int	   Lx  = (v[i+1].XYZ.X - v[i].XYZ.X);
		float  ax0 = alpha0(Lx, X0, X1);
		float  ax1 = alpha1(Lx, X0, X1);
		uint16 tx0 = Interpolate_UV(ax0, v[i].U, v[i+1].U);
		uint16 tx1 = Interpolate_UV(ax1, v[i].U, v[i+1].U);
#ifdef DEBUG_U
		if (debug) {
			fprintf(stderr, "u0:%d and u1:%d\n", v[i].U, v[i+1].U);
			fprintf(stderr, "a0:%f and a1:%f\n", ax0, ax1);
			fprintf(stderr, "t0:%d and t1:%d\n", tx0, tx1);
		}
#endif

		int	   oy  = m_context->XYOFFSET.OFY;
		int	   Y0  = v[i].XYZ.Y   - oy;
		int	   Y1  = v[i+1].XYZ.Y - oy;
		int	   Ly  = (v[i+1].XYZ.Y - v[i].XYZ.Y);
		float  ay0 = alpha0(Ly, Y0, Y1);
		float  ay1 = alpha1(Ly, Y0, Y1);
		uint16 ty0 = Interpolate_UV(ay0, v[i].V, v[i+1].V);
		uint16 ty1 = Interpolate_UV(ay1, v[i].V, v[i+1].V);
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
	if(m_dev->IsLost() || IsBadFrame()) {
		GL_INS("Warning skipping a draw call (%d)", s_n);
		return;
	}
	GL_PUSH("HW Draw %d", s_n);

	GSDrawingEnvironment& env = m_env;
	GSDrawingContext* context = m_context;
	const GSLocalMemory::psm_t& tex_psm = GSLocalMemory::m_psm[m_context->TEX0.PSM];

	// skip alpha test if possible
	// Note: do it first so we know if frame/depth writes are masked

	GIFRegTEST TEST = context->TEST;
	GIFRegFRAME FRAME = context->FRAME;
	GIFRegZBUF ZBUF = context->ZBUF;

	uint32 fm = context->FRAME.FBMSK;
	uint32 zm = context->ZBUF.ZMSK || context->TEST.ZTE == 0 ? 0xffffffff : 0;

	// Note required to compute TryAlphaTest below. So do it now.
	if (PRIM->TME && tex_psm.pal > 0)
		m_mem.m_clut.Read32(context->TEX0, env.TEXA);

	//  Test if we can optimize Alpha Test as a NOP
	context->TEST.ATE = context->TEST.ATE && !GSRenderer::TryAlphaTest(fm, zm);

	context->FRAME.FBMSK = fm;
	context->ZBUF.ZMSK = zm != 0;

	// It is allowed to use the depth and rt at the same location. However at least 1 must
	// be disabled. Or the written value must be the same on both channels.
	// 1/ GoW uses a Cd blending on a 24 bits buffer (no alpha)
	// 2/ SuperMan really draws (0,0,0,0) color and a (0) 32-bits depth
	// 3/ 50cents really draws (0,0,0,128) color and a (0) 24 bits depth
	// Note: FF DoC has both buffer at same location but disable the depth test (write?) with ZTE = 0
	const bool no_rt = (context->ALPHA.IsCd() && PRIM->ABE && (context->FRAME.PSM == 1));
	const bool no_ds = !no_rt && (
			// Depth is always pass/fail (no read) and write are discarded (tekken 5).  (Note: DATE is currently implemented with a stencil buffer => a depth/stencil buffer)
			(zm != 0 && m_context->TEST.ZTST <= ZTST_ALWAYS && !m_context->TEST.DATE) ||
			// Depth will be written through the RT
			(context->FRAME.FBP == context->ZBUF.ZBP && !PRIM->TME && zm == 0 && fm == 0 && context->TEST.ZTE)
			);

	const bool draw_sprite_tex = PRIM->TME && (m_vt.m_primclass == GS_SPRITE_CLASS);
	const GSVector4 delta_p = m_vt.m_max.p - m_vt.m_min.p;
	bool single_page = (delta_p.x <= 64.0f) && (delta_p.y <= 64.0f);

	if (m_channel_shuffle) {
		m_channel_shuffle = draw_sprite_tex && (m_context->TEX0.PSM == PSM_PSMT8) && single_page;
		if (m_channel_shuffle) {
			GL_CACHE("Channel shuffle effect detected SKIP");
			return;
		}
	} else if (draw_sprite_tex && m_context->FRAME.Block() == m_context->TEX0.TBP0) {
		// Special post-processing effect
		if ((m_context->TEX0.PSM == PSM_PSMT8) && single_page) {
			GL_INS("Channel shuffle effect detected");
			m_channel_shuffle = true;
		} else {
			GL_DBG("Special post-processing effect not supported");
			m_channel_shuffle = false;
		}
	} else {
		m_channel_shuffle = false;
	}

	GIFRegTEX0 TEX0;

	TEX0.TBP0 = context->FRAME.Block();
	TEX0.TBW = context->FRAME.FBW;
	TEX0.PSM = context->FRAME.PSM;

	GSTextureCache::Target* rt = NULL;
	GSTexture* rt_tex = NULL;
	if (!no_rt) {
		rt = m_tc->LookupTarget(TEX0, m_width, m_height, GSTextureCache::RenderTarget, true, fm);
		rt_tex = rt->m_texture;
	}

	TEX0.TBP0 = context->ZBUF.Block();
	TEX0.TBW = context->FRAME.FBW;
	TEX0.PSM = context->ZBUF.PSM;

	GSTextureCache::Target* ds = NULL;
	GSTexture* ds_tex = NULL;
	if (!no_ds) {
		ds = m_tc->LookupTarget(TEX0, m_width, m_height, GSTextureCache::DepthStencil, context->DepthWrite());
		ds_tex = ds->m_texture;
	}

	GSTextureCache::Source* tex = NULL;
	m_texture_shuffle = false;

	if(PRIM->TME)
	{
		GIFRegCLAMP MIP_CLAMP = context->CLAMP;
		int mxl = std::min<int>((int)m_context->TEX1.MXL, 6);
		m_lod = GSVector2i(0, 0);

		// Code from the SW renderer
		if (IsMipMapActive()) {
			int interpolation = (context->TEX1.MMIN & 1) + 1; // 1: round, 2: tri

			int k = (m_context->TEX1.K + 8) >> 4;
			int lcm = m_context->TEX1.LCM;

			if ((int)m_vt.m_lod.x >= mxl) {
				k = mxl; // set lod to max level
				lcm = 1; // constant lod
			}

			if (PRIM->FST) {
				ASSERT(lcm == 1);
				ASSERT(((m_vt.m_min.t.uph(m_vt.m_max.t) == GSVector4::zero()).mask() & 3) == 3); // ratchet and clank (menu)

				lcm = 1;
			}

			if (lcm == 1) {
				m_lod.x = std::max<int>(k, 0);
				m_lod.y = m_lod.x;
			} else {
				// Not constant but who care !
				if (interpolation == 2) {
					// Mipmap Linear. Both layers are sampled, only take the big one
					m_lod.x = std::max<int>((int)floor(m_vt.m_lod.x), 0);
				} else {
					// On GS lod is a fixed float number 7:4 (4 bit for the frac part)
#if 0
					m_lod.x = std::max<int>((int)round(m_vt.m_lod.x + 0.0625), 0);
#else
					// Same as above with a bigger margin on rounding
					// The goal is to avoid 1 undrawn pixels around the edge which trigger the load of the big
					// layer.
					if (ceil(m_vt.m_lod.x) < m_vt.m_lod.y)
						m_lod.x = std::max<int>((int)round(m_vt.m_lod.x + 0.0625 + 0.01), 0);
					else
						m_lod.x = std::max<int>((int)round(m_vt.m_lod.x + 0.0625), 0);
#endif
				}

				m_lod.y = std::max<int>((int)ceil(m_vt.m_lod.y), 0);
			}

			m_lod.x = std::min<int>(m_lod.x, mxl);
			m_lod.y = std::min<int>(m_lod.y, mxl);

			TEX0 = GetTex0Layer(m_lod.x);

			MIP_CLAMP.MINU >>= m_lod.x;
			MIP_CLAMP.MINV >>= m_lod.x;
			MIP_CLAMP.MAXU >>= m_lod.x;
			MIP_CLAMP.MAXV >>= m_lod.x;

			for (int i = 0; i < m_lod.x; i++) {
				m_vt.m_min.t *= 0.5f;
				m_vt.m_max.t *= 0.5f;
			}

			GL_CACHE("Mipmap LOD %d %d (%f %f) new size %dx%d (K %d L %u)", m_lod.x, m_lod.y, m_vt.m_lod.x, m_vt.m_lod.y, 1 << TEX0.TW, 1 << TEX0.TH, m_context->TEX1.K, m_context->TEX1.L);
		} else {
			TEX0 = GetTex0Layer(0);
		}

		m_context->offset.tex = m_mem.GetOffset(TEX0.TBP0, TEX0.TBW, TEX0.PSM);

		GSVector4i r;

		GetTextureMinMax(r, TEX0, MIP_CLAMP, m_vt.IsLinear());

		tex = tex_psm.depth ? m_tc->LookupDepthSource(TEX0, env.TEXA, r) : m_tc->LookupSource(TEX0, env.TEXA, r);

		// Round 2
		if (IsMipMapActive() && m_mipmap == 2 && !tex_psm.depth) {
			// Upload remaining texture layers
			GSVector4 tmin = m_vt.m_min.t;
			GSVector4 tmax = m_vt.m_max.t;

			for (int layer = m_lod.x + 1; layer <= m_lod.y; layer++) {
				const GIFRegTEX0& MIP_TEX0 = GetTex0Layer(layer);

				m_context->offset.tex = m_mem.GetOffset(MIP_TEX0.TBP0, MIP_TEX0.TBW, MIP_TEX0.PSM);

				MIP_CLAMP.MINU >>= 1;
				MIP_CLAMP.MINV >>= 1;
				MIP_CLAMP.MAXU >>= 1;
				MIP_CLAMP.MAXV >>= 1;

				m_vt.m_min.t *= 0.5f;
				m_vt.m_max.t *= 0.5f;

				GetTextureMinMax(r, MIP_TEX0, MIP_CLAMP, m_vt.IsLinear());

				tex->UpdateLayer(MIP_TEX0, r, layer - m_lod.x);
			}

			m_vt.m_min.t = tmin;
			m_vt.m_max.t = tmax;
		}

		// Hypothesis: texture shuffle is used as a postprocessing effect so texture will be an old target.
		// Initially code also tested the RT but it gives too much false-positive
		//
		// Both input and output are 16 bits and texture was initially 32 bits!
		m_texture_shuffle = (GSLocalMemory::m_psm[context->FRAME.PSM].bpp == 16) && (tex_psm.bpp == 16)
			&& draw_sprite_tex && tex->m_32_bits_fmt;

		// Texture shuffle is not yet supported with strange clamp mode
		ASSERT(!m_texture_shuffle || (context->CLAMP.WMS < 3 && context->CLAMP.WMT < 3));

		if (tex->m_target && m_context->TEX0.PSM == PSM_PSMT8 && single_page && draw_sprite_tex) {
			GL_INS("Channel shuffle effect detected (2nd shot)");
			m_channel_shuffle = true;
		} else {
			m_channel_shuffle = false;
		}
	}
	if (rt) {
		// Be sure texture shuffle detection is properly propagated
		// Otherwise set or clear the flag (Code in texture cache only set the flag)
		// Note: it is important to clear the flag when RT is used as a real 16 bits target.
		rt->m_32_bits_fmt = m_texture_shuffle || (GSLocalMemory::m_psm[context->FRAME.PSM].bpp != 16);
	}

	if(s_dump)
	{
		uint64 frame = m_perfmon.GetFrame();

		std::string s;

		if (s_n >= s_saven) {
			// Dump Register state
			s = format("%05d_context.txt", s_n);

			m_env.Dump(m_dump_root+s);
			m_context->Dump(m_dump_root+s);
		}

		if(s_savet && s_n >= s_saven && tex)
		{
			s = format("%05d_f%lld_itex_%05x_%s_%d%d_%02x_%02x_%02x_%02x.dds",
				s_n, frame, (int)context->TEX0.TBP0, psm_str(context->TEX0.PSM),
				(int)context->CLAMP.WMS, (int)context->CLAMP.WMT,
				(int)context->CLAMP.MINU, (int)context->CLAMP.MAXU,
				(int)context->CLAMP.MINV, (int)context->CLAMP.MAXV);

			tex->m_texture->Save(m_dump_root+s, true);

			if(tex->m_palette)
			{
				s = format("%05d_f%lld_itpx_%05x_%s.dds", s_n, frame, context->TEX0.CBP, psm_str(context->TEX0.CPSM));

				tex->m_palette->Save(m_dump_root+s, true);
			}
		}

		if(s_save && s_n >= s_saven)
		{
			s = format("%05d_f%lld_rt0_%05x_%s.bmp", s_n, frame, context->FRAME.Block(), psm_str(context->FRAME.PSM));

			if (rt)
				rt->m_texture->Save(m_dump_root+s);
		}

		if(s_savez && s_n >= s_saven)
		{
			s = format("%05d_f%lld_rz0_%05x_%s.bmp", s_n, frame, context->ZBUF.Block(), psm_str(context->ZBUF.PSM));

			if (ds_tex)
				ds_tex->Save(m_dump_root+s);
		}

	}

	// The rectangle of the draw
	GSVector4i r = GSVector4i(m_vt.m_min.p.xyxy(m_vt.m_max.p)).rintersect(GSVector4i(context->scissor.in));

	if(m_hacks.m_oi && !(this->*m_hacks.m_oi)(rt_tex, ds_tex, tex))
	{
		GL_INS("Warning skipping a draw call (%d)", s_n);
		return;
	}

	if (!OI_BlitFMV(rt, tex, r)) {
		GL_INS("Warning skipping a draw call (%d)", s_n);
		return;
	}

	if (!m_userhacks_disable_gs_mem_clear) {
		// Constant Direct Write without texture/test/blending (aka a GS mem clear)
		if ((m_vt.m_primclass == GS_SPRITE_CLASS) && !PRIM->TME // Direct write
				&& (!PRIM->ABE || m_context->ALPHA.IsOpaque()) // No transparency
				&& (m_context->FRAME.FBMSK == 0) // no color mask
				&& !m_context->TEST.ATE // no alpha test
				&& (!m_context->TEST.ZTE || m_context->TEST.ZTST == ZTST_ALWAYS) // no depth test
				&& (m_vt.m_eq.rgba == 0xFFFF) // constant color write
				&& r.x == 0 && r.y == 0) { // Likely full buffer write

			OI_GsMemClear();

			OI_DoubleHalfClear(rt_tex, ds_tex);
		}
	}

	// A couple of hack to avoid upscaling issue. So far it seems to impacts mostly sprite
	// Note: first hack corrects both position and texture coordinate
	// Note: second hack corrects only the texture coordinate
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

		// Noting to do if no texture is sampled
		if (PRIM->FST && draw_sprite_tex) {
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

	DrawPrims(rt_tex, ds_tex, tex);

	//

	context->TEST = TEST;
	context->FRAME = FRAME;
	context->ZBUF = ZBUF;

	//

	// Help to detect rendering outside of the framebuffer
#if _DEBUG
	if (m_upscale_multiplier * r.z > m_width) {
		GL_INS("ERROR: RT width is too small only %d but require %d", m_width, m_upscale_multiplier * r.z);
	}
	if (m_upscale_multiplier * r.w > m_height) {
		GL_INS("ERROR: RT height is too small only %d but require %d", m_height, m_upscale_multiplier * r.w);
	}
#endif

	if(fm != 0xffffffff && rt)
	{
		//rt->m_valid = rt->m_valid.runion(r);
		rt->UpdateValidity(r);

		m_tc->InvalidateVideoMem(context->offset.fb, r, false);

		m_tc->InvalidateVideoMemType(GSTextureCache::DepthStencil, context->FRAME.Block());
	}

	if(zm != 0xffffffff && ds)
	{
		//ds->m_valid = ds->m_valid.runion(r);
		ds->UpdateValidity(r);

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

		std::string s;

		if(s_save && s_n >= s_saven)
		{
			s = format("%05d_f%lld_rt1_%05x_%s.bmp", s_n, frame, context->FRAME.Block(), psm_str(context->FRAME.PSM));

			if (rt)
				rt->m_texture->Save(m_dump_root+s);
		}

		if(s_savez && s_n >= s_saven)
		{
			s = format("%05d_f%lld_rz1_%05x_%s.bmp", s_n, frame, context->ZBUF.Block(), psm_str(context->ZBUF.PSM));

			if (ds_tex)
				ds_tex->Save(m_dump_root+s);
		}

		if(s_savel > 0 && (s_n - s_saven) > s_savel)
		{
			s_dump = 0;
		}
	}

	#ifdef DISABLE_HW_TEXTURE_CACHE

	if (rt)
		m_tc->Read(rt, r);

	#endif
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
	bool is_opengl = theApp.GetCurrentRendererType() == GSRendererType::OGL_HW;
	bool can_handle_depth = (!theApp.GetConfigB("UserHacks") || !theApp.GetConfigB("UserHacks_DisableDepthSupport")) && is_opengl;

	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::FFXII, CRC::EU, &GSRendererHW::OI_FFXII));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::FFX, CRC::RegionCount, &GSRendererHW::OI_FFX));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::MetalSlug6, CRC::RegionCount, &GSRendererHW::OI_MetalSlug6));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::RozenMaidenGebetGarden, CRC::RegionCount, &GSRendererHW::OI_RozenMaidenGebetGarden));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::StarWarsForceUnleashed, CRC::RegionCount, &GSRendererHW::OI_StarWarsForceUnleashed));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::SpyroNewBeginning, CRC::RegionCount, &GSRendererHW::OI_SpyroNewBeginning));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::SpyroEternalNight, CRC::RegionCount, &GSRendererHW::OI_SpyroEternalNight));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::SuperManReturns, CRC::RegionCount, &GSRendererHW::OI_SuperManReturns));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::TalesOfLegendia, CRC::RegionCount, &GSRendererHW::OI_TalesOfLegendia));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::ArTonelico2, CRC::RegionCount, &GSRendererHW::OI_ArTonelico2));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::ItadakiStreet, CRC::RegionCount, &GSRendererHW::OI_ItadakiStreet));

	if (!can_handle_depth) {
		m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::SMTNocturne, CRC::RegionCount, &GSRendererHW::OI_SMTNocturne));
		m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::GodOfWar2, CRC::RegionCount, &GSRendererHW::OI_GodOfWar2));
	}

	m_oo_list.push_back(HackEntry<OO_Ptr>(CRC::DBZBT2, CRC::RegionCount, &GSRendererHW::OO_DBZBT2));
	m_oo_list.push_back(HackEntry<OO_Ptr>(CRC::MajokkoALaMode2, CRC::RegionCount, &GSRendererHW::OO_MajokkoALaMode2));
	m_oo_list.push_back(HackEntry<OO_Ptr>(CRC::Jak2, CRC::RegionCount, &GSRendererHW::OO_Jak));
	m_oo_list.push_back(HackEntry<OO_Ptr>(CRC::Jak3, CRC::RegionCount, &GSRendererHW::OO_Jak));
	m_oo_list.push_back(HackEntry<OO_Ptr>(CRC::JakX, CRC::RegionCount, &GSRendererHW::OO_Jak));

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

	if (game.flags & CRC::PointListPalette) {
		ASSERT(m_oi == NULL);

		m_oi = &GSRendererHW::OI_PointListPalette;
	}
}

// Trick to do a fast clear on the GS
// Set frame buffer pointer on the start of the buffer. Set depth buffer pointer on the half buffer
// FB + depth write will fill the full buffer.
void GSRendererHW::OI_DoubleHalfClear(GSTexture* rt, GSTexture* ds)
{
	// Note gs mem clear must be tested before calling this function

	// Limit further to unmask Z write
	if (!m_context->ZBUF.ZMSK && rt && ds) {
		const GSVertex* v = &m_vertex.buff[0];
		const GSLocalMemory::psm_t& frame_psm = GSLocalMemory::m_psm[m_context->FRAME.PSM];
		//const GSLocalMemory::psm_t& depth_psm = GSLocalMemory::m_psm[m_context->ZBUF.PSM];

		// Z and color must be constant and the same
		if (m_vt.m_eq.rgba != 0xFFFF || !m_vt.m_eq.z || v[1].XYZ.Z != v[1].RGBAQ.u32[0])
			return;

		// Format doesn't have the same size. It smells fishy (xmen...)
		//if (frame_psm.trbpp != depth_psm.trbpp)
		//	return;

		// Size of the current draw
		uint32 w_pages = static_cast<uint32>(roundf(m_vt.m_max.p.x / frame_psm.pgs.x));
		uint32 h_pages = static_cast<uint32>(roundf(m_vt.m_max.p.y / frame_psm.pgs.y));
		uint32 written_pages = w_pages * h_pages;

		// Frame and depth pointer can be inverted
		uint32 base;
		uint32 half;
		if (m_context->FRAME.FBP > m_context->ZBUF.ZBP) {
			base = m_context->ZBUF.ZBP;
			half = m_context->FRAME.FBP;
		} else {
			base = m_context->FRAME.FBP;
			half = m_context->ZBUF.ZBP;
		}

		// If both buffers are side by side we can expect a fast clear in on-going
		if (half <= (base + written_pages)) {
			uint32 color = v[1].RGBAQ.u32[0];

			GL_INS("OI_DoubleHalfClear: base %x half %x. w_pages %d h_pages %d fbw %d. Color %x", base << 5, half << 5, w_pages, h_pages, m_context->FRAME.FBW, color);

			if (m_context->FRAME.FBP > m_context->ZBUF.ZBP) {
				// Only pure clear are supported for depth
				ASSERT(color == 0);
				m_dev->ClearDepth(ds);
			} else {
				m_dev->ClearRenderTarget(rt, color);
			}
		}
	}
}

// Note: hack is safe, but it could impact the perf a little (normally games do only a couple of clear by frame)
void GSRendererHW::OI_GsMemClear()
{
	// Note gs mem clear must be tested before calling this function

	// Limit it further to a full screen 0 write
	if ((m_vertex.next == 2) &&  m_vt.m_min.c.eq(GSVector4i(0))) {
		GSOffset* off = m_context->offset.fb;
		GSVector4i r = GSVector4i(m_vt.m_min.p.xyxy(m_vt.m_max.p)).rintersect(GSVector4i(m_context->scissor.in));
		// Limit the hack to a single fullscreen clear. Some games might use severals column to clear a screen
		// but hopefully it will be enough.
		if (r.width() <= 128 || r.height() <= 128)
			return;

		GL_INS("OI_GsMemClear (%d,%d => %d,%d)", r.x, r.y, r.z, r.w);
		int format = GSLocalMemory::m_psm[m_context->FRAME.PSM].fmt;

		// FIXME: loop can likely be optimized with AVX/SSE. Pixels aren't
		// linear but the value will be done for all pixels of a block.
		// FIXME: maybe we could limit the write to the top and bottom row page.
		if (format == 0) {
			// Based on WritePixel32
			for(int y = r.top; y < r.bottom; y++)
			{
				uint32* RESTRICT d = &m_mem.m_vm32[off->pixel.row[y]];
				int* RESTRICT col = off->pixel.col[0];

				for(int x = r.left; x < r.right; x++)
				{
					d[col[x]] = 0; // Here the constant color
				}
			}
		} else if (format == 1) {
			// Based on WritePixel24
			for(int y = r.top; y < r.bottom; y++)
			{
				uint32* RESTRICT d = &m_mem.m_vm32[off->pixel.row[y]];
				int* RESTRICT col = off->pixel.col[0];

				for(int x = r.left; x < r.right; x++)
				{
					d[col[x]] &= 0xff000000; // Clear the color
				}
			}
		} else if (format == 2) {
			; // Hack is used for FMV which are likely 24/32 bits. Let's keep the for reference
#if 0
			// Based on WritePixel16
			for(int y = r.top; y < r.bottom; y++)
			{
				uint32* RESTRICT d = &m_mem.m_vm16[off->pixel.row[y]];
				int* RESTRICT col = off->pixel.col[0];

				for(int x = r.left; x < r.right; x++)
				{
					d[col[x]] = 0; // Here the constant color
				}
			}
#endif
		}
	}
}

bool GSRendererHW::OI_BlitFMV(GSTextureCache::Target* _rt, GSTextureCache::Source* tex, const GSVector4i& r_draw)
{
	if (r_draw.w > 1024 && (m_vt.m_primclass == GS_SPRITE_CLASS) && (m_vertex.next == 2) && PRIM->TME && !PRIM->ABE && tex && !tex->m_target && m_context->TEX0.TBW > 0) {
		GL_PUSH("OI_BlitFMV");

		GL_INS("OI_BlitFMV");

		// The draw is done past the RT at the location of the texture. To avoid various upscaling mess
		// We will blit the data from the top to the bottom of the texture manually.

		// Expected memory representation
		// -----------------------------------------------------------------
		// RT (2 half frame)
		// -----------------------------------------------------------------
		// Top of Texture (full height frame)
		//
		// Bottom of Texture (half height frame, will be the copy of Top texture after the draw)
		// -----------------------------------------------------------------

		// sRect is the top of texture
		int tw = (int)(1 << m_context->TEX0.TW);
		int th = (int)(1 << m_context->TEX0.TH);
		GSVector4 sRect;
		sRect.x = m_vt.m_min.t.x / tw;
		sRect.y = m_vt.m_min.t.y / th;
		sRect.z = m_vt.m_max.t.x / tw;
		sRect.w = m_vt.m_max.t.y / th;

		// Compute the Bottom of texture rectangle
		ASSERT(m_context->TEX0.TBP0 > m_context->FRAME.Block());
		int offset = (m_context->TEX0.TBP0 - m_context->FRAME.Block()) / m_context->TEX0.TBW;
		GSVector4i r_texture(r_draw);
		r_texture.y -= offset;
		r_texture.w -= offset;

		GSVector4 dRect(r_texture);

		// Do the blit. With a Copy mess to avoid issue with limited API (dx)
		// m_dev->StretchRect(tex->m_texture, sRect, tex->m_texture, dRect);
		GSVector4i r_full(0, 0, tw, th);
		if (GSTexture* rt = m_dev->CreateRenderTarget(tw, th, false)) {
			m_dev->CopyRect(tex->m_texture, rt, r_full);

			m_dev->StretchRect(tex->m_texture, sRect, rt, dRect);

			m_dev->CopyRect(rt, tex->m_texture, r_full);

			m_dev->Recycle(rt);
		}

		// Copy back the texture into the GS mem. I don't know why but it will be
		// reuploaded again later
		m_tc->Read(tex, r_texture);

		m_tc->InvalidateVideoMemSubTarget(_rt);

		return false; // skip current draw
	}

	// Nothing to see keep going
	return true;
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

				m_vt.Update(m_vertex.buff, m_index.buff, m_vertex.tail, m_index.tail, GS_TRIANGLE_CLASS);
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

		m_dev->ClearDepth(ds);
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

	m_vt.Update(m_vertex.buff, m_index.buff, m_vertex.tail, m_index.tail, m_vt.m_primclass);

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

		if(GSTextureCache::Target* tmp_ds = m_tc->LookupTarget(TEX0, m_width, m_height, GSTextureCache::DepthStencil, true))
		{
			m_dev->ClearDepth(tmp_ds->m_texture);
		}

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

			if(GSTextureCache::Target* tmp_rt = m_tc->LookupTarget(TEX0, m_width, m_height, GSTextureCache::RenderTarget, true))
			{
				m_dev->ClearRenderTarget(tmp_rt->m_texture, 0);
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

			if(GSTextureCache::Target* tmp_ds = m_tc->LookupTarget(TEX0, m_width, m_height, GSTextureCache::DepthStencil, true))
			{
				m_dev->ClearDepth(tmp_ds->m_texture);
			}

			return false;
		}
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
			m_dev->ClearDepth(ds);

			return false;
		}
	}
	else if(PRIM->TME)
	{
		if((FBP == 0x0 || FBP == 0x01180) && FPSM == PSM_PSMCT32 && (m_vt.m_eq.z && m_vt.m_max.p.z == 0))
		{
			m_dev->ClearDepth(ds);
		}
	}

	return true;
}

bool GSRendererHW::OI_SpyroNewBeginning(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	uint32 FBP = m_context->FRAME.Block();
	uint32 FPSM = m_context->FRAME.PSM;

	if(PRIM->TME)
	{
		if((FBP == 0x0 || FBP == 0x01180) && FPSM == PSM_PSMCT32 && (m_vt.m_eq.z && m_vt.m_min.p.z == 0))
		{
			m_dev->ClearDepth(ds);
		}
	}

	return true;
}

bool GSRendererHW::OI_SpyroEternalNight(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	uint32 FBP = m_context->FRAME.Block();
	uint32 FPSM = m_context->FRAME.PSM;

	if(PRIM->TME)
	{
		if((FBP == 0x0 || FBP == 0x01180) && FPSM == PSM_PSMCT32 && (m_vt.m_eq.z && m_vt.m_min.p.z == 0))
		{
			m_dev->ClearDepth(ds);
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
		//m_dev->ClearDepth(ds);
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
		if (GSTextureCache::Target* tmp_ds = m_tc->LookupTarget(TEX0, m_width, m_height, GSTextureCache::DepthStencil, true))
		{
			m_dev->ClearDepth(tmp_ds->m_texture);
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

bool GSRendererHW::OI_SuperManReturns(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	// Instead to use a fullscreen rectangle they use a 32 pixels, 4096 pixels with a FBW of 1.
	// Technically the FB wrap/overlap on itself...
	GSDrawingContext* ctx = m_context;
#ifndef NDEBUG
	GSVertex* v = &m_vertex.buff[0];
#endif

	if (!(ctx->FRAME.FBP == ctx->ZBUF.ZBP && !PRIM->TME && !ctx->ZBUF.ZMSK && !ctx->FRAME.FBMSK && m_vt.m_eq.rgba == 0xFFFF))
		return true;

	// Please kill those crazy devs!
	ASSERT(m_vertex.next == 2);
	ASSERT(m_vt.m_primclass == GS_SPRITE_CLASS);
	ASSERT((v->RGBAQ.A << 24 | v->RGBAQ.B << 16 | v->RGBAQ.G << 8 | v->RGBAQ.R) == (int)v->XYZ.Z);

	// Do a direct write
	m_dev->ClearRenderTarget(rt, GSVector4(m_vt.m_min.c));

	m_tc->InvalidateVideoMemType(GSTextureCache::DepthStencil, ctx->FRAME.Block());

	return false;
}

bool GSRendererHW::OI_ArTonelico2(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	// world map clipping
	//
	// The bad draw call is a sprite rendering to clear the z buffer

	/*
	   Depth buffer description
	   * width is 10 pages
	   * texture/scissor size is 640x448
	   * depth is 16 bits so it writes 70 (10w * 7h) pages of data.

	   following draw calls will use the buffer as 6 pages width with a scissor
	   test of 384x672. So the above texture can be seen as a

	   * texture width: 6 pages * 64 pixels/page = 384
	   * texture height: 70/6 pages * 64 pixels/page =746

	   So as you can see the GS issue a write of 640x448 but actually it
	   expects to clean a 384x746 area. Ideally the fix will transform the
	   buffer to adapt the page width properly.
	 */

	GSVertex* v = &m_vertex.buff[0];

	if (m_vertex.next == 2 && !PRIM->TME && m_context->FRAME.FBW == 10 && v->XYZ.Z == 0 && m_context->TEST.ZTST == ZTST_ALWAYS) {
		GL_INS("OI_ArTonelico2");
		m_dev->ClearDepth(ds);
	}

	return true;
}

bool GSRendererHW::OI_ItadakiStreet(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	if (m_context->TEST.ATST == ATST_NOTEQUAL && m_context->TEST.AREF == 0) {
		// It is also broken on the SW renderer. Issue appears because fragment alpha is 0
		// I suspect the game expect low value of alpha, and due to bad rounding on the core
		// you have wrongly 0.
		// Otherwise some draws calls are empty (all pixels are discarded).
		// It fixes missing element on the board

		GL_INS("OI_ItadakiStreetSpecial disable alpha test");
		m_context->TEST.ATST = ATST_ALWAYS;

#if 0 // Not enough
		uint32 dummy_fm;
		uint32 dummy_zm;

		if (!TryAlphaTest(dummy_fm, dummy_zm)) {
			GL_INS("OI_ItadakiStreetSpecial disable alpha test");
			m_context->TEST.ATST = ATST_ALWAYS;
		}
#endif
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

void GSRendererHW::OO_Jak()
{
	// FIXME might need a CU_Jak too
	GSVector4i r = GSVector4i(m_vt.m_min.p.xyxy(m_vt.m_max.p)).rintersect(GSVector4i(m_context->scissor.in));
	GSVector4i r_p = GSVector4i(0, 0, 16, 16);

	if(!PRIM->FST && PRIM->TME && (r == r_p).alltrue() && m_context->TEX0.TW == 4 && m_context->TEX0.TH == 4 && m_context->TEX0.PSM == PSM_PSMCT32) {
		// Game will render a texture directly into a palette.
		uint32 FBP = m_context->FRAME.Block();
		GL_INS("OO_Jak read back 0x%x", FBP);

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
