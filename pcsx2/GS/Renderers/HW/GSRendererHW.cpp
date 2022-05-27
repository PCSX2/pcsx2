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
#include "GSRendererHW.h"
#include "GSTextureReplacements.h"
#include "GS/GSGL.h"
#include "Host.h"
#include "common/StringUtil.h"

GSRendererHW::GSRendererHW()
	: GSRenderer()
	, m_width(default_rt_size.x)
	, m_height(default_rt_size.y)
	, m_tc(new GSTextureCache())
	, m_src(nullptr)
	, m_reset(false)
	, m_channel_shuffle(false)
	, m_userhacks_tcoffset(false)
	, m_userhacks_tcoffset_x(0)
	, m_userhacks_tcoffset_y(0)
	, m_lod(GSVector2i(0, 0))
{
	m_mipmap = (GSConfig.HWMipmap >= HWMipmapLevel::Basic);
	SetTCOffset();

	m_dump_root = root_hw;
	GSTextureReplacements::Initialize(m_tc);

	// Hope nothing requires too many draw calls.
	m_drawlist.reserve(2048);

	memset(&m_conf, 0, sizeof(m_conf));

	m_prim_overlap = PRIM_OVERLAP_UNKNOW;
	ResetStates();
}

void GSRendererHW::SetScaling()
{
	GSVector2i crtc_size(GetResolution());

	// Correct framebuffer size to get output size when offsets not considered (uses framebuffer height)
	if (!GSConfig.PCRTCOffsets)
	{
		const int videomode = static_cast<int>(GetVideoMode()) - 1;
		const int display_width = (VideoModeDividers[videomode].z + 1) / GetDisplayHMagnification();
		int display_height = VideoModeOffsets[videomode].y;

		if (isinterlaced() && !m_regs->SMODE2.FFMD)
			display_height *= 2;

		if (crtc_size.x < display_width || crtc_size.y < display_height)
		{
			GSVector2i display_baseline = { 4096, 4096 };

			for (int i = 0; i < 2; i++)
			{
				if (IsEnabled(i))
				{
					GSVector4i dr = GetDisplayRect(i);

					GSVector2i display_diff(dr.left - display_baseline.x, dr.top - display_baseline.y);

					if (display_diff.x != 0 && abs(display_diff.x) < 4 && crtc_size.x < display_width)
						crtc_size.x -= display_diff.x;

					if (display_diff.y != 0 && abs(display_diff.y) < 4 && crtc_size.y < display_height)
						crtc_size.y -= display_diff.y;

					display_baseline.x = std::min(dr.left, display_baseline.x);
					display_baseline.y = std::min(dr.top, display_baseline.y);
				}
			}
		}
	}

	// Details of (potential) perf impact of a big framebuffer
	// 1/ extra memory
	// 2/ texture cache framebuffer rescaling/copy
	// 3/ upload of framebuffer (preload hack)
	// 4/ framebuffer clear (color/depth/stencil)
	// 5/ read back of the frambuffer
	//
	// With the solution
	// 1/ Nothing to do.Except the texture cache bug (channel shuffle effect)
	//    most of the market is 1GB of VRAM (and soon 2GB)
	// 2/ limit rescaling/copy to the valid data of the framebuffer
	// 3/ ??? no solution so far
	// 4a/ stencil can be limited to valid data.
	// 4b/ is it useful to clear color? depth? (in any case, it ought to be few operation)
	// 5/ limit the read to the valid data

	// Framebuffer width is always a multiple of 64 so at certain cases it can't cover some weird width values.
	// 480P , 576P use width as 720 which is not referencable by FBW * 64. so it produces 704 ( the closest value multiple by 64).
	// In such cases, let's just use the CRTC width.
	const int fb_width = std::max({(int)m_context->FRAME.FBW * 64, crtc_size.x, 512});
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
	//
	// m_large_framebuffer has been inverted to m_conservative_framebuffer, it isn't an option that benefits being enabled all the time for everyone.
	int fb_height = MAX_FRAMEBUFFER_HEIGHT;
	if (GSConfig.ConservativeFramebuffer)
	{
		fb_height = fb_width < 1024 ? std::max(512, crtc_size.y) : 1024;
	}

	const int upscaled_fb_w = fb_width * GSConfig.UpscaleMultiplier;
	const int upscaled_fb_h = fb_height * GSConfig.UpscaleMultiplier;
	const bool good_rt_size = m_width >= upscaled_fb_w && m_height >= upscaled_fb_h;

	// No need to resize for native/custom resolutions as default size will be enough for native and we manually get RT Buffer size for custom.
	// don't resize until the display rectangle and register states are stabilized.
	if (good_rt_size)
		return;

	m_tc->RemovePartial();
	m_width = upscaled_fb_w;
	m_height = upscaled_fb_h;
	printf("Frame buffer size set to  %dx%d (%dx%d)\n", fb_width, fb_height, m_width, m_height);
}

void GSRendererHW::SetTCOffset()
{
	m_userhacks_tcoffset_x = std::max<s32>(GSConfig.UserHacks_TCOffsetX, 0) / -1000.0f;
	m_userhacks_tcoffset_y = std::max<s32>(GSConfig.UserHacks_TCOffsetY, 0) / -1000.0f;
	m_userhacks_tcoffset = m_userhacks_tcoffset_x < 0.0f || m_userhacks_tcoffset_y < 0.0f;
}

GSRendererHW::~GSRendererHW()
{
	delete m_tc;
}

void GSRendererHW::Destroy()
{
	m_tc->RemoveAll();
	GSTextureReplacements::Shutdown();
	GSRenderer::Destroy();
}

void GSRendererHW::PurgeTextureCache()
{
	GSRenderer::PurgeTextureCache();
	m_tc->RemoveAll();
}

bool GSRendererHW::IsPossibleTextureShuffle(GSTextureCache::Source* src) const
{
	return (PRIM->TME && m_vt.m_primclass == GS_SPRITE_CLASS &&
		src->m_32_bits_fmt && GSLocalMemory::m_psm[src->m_TEX0.PSM].bpp == 16 &&
		GSLocalMemory::m_psm[m_context->FRAME.PSM].bpp == 16);
}

void GSRendererHW::SetGameCRC(u32 crc, int options)
{
	GSRenderer::SetGameCRC(crc, options);

	m_hacks.SetGameCRC(m_game);

	GSTextureReplacements::GameChanged();
}

bool GSRendererHW::CanUpscale()
{
	if (m_hacks.m_cu && !(this->*m_hacks.m_cu)())
	{
		return false;
	}

	return GSConfig.UpscaleMultiplier != 1;
}

int GSRendererHW::GetUpscaleMultiplier()
{
	return GSConfig.UpscaleMultiplier;
}

void GSRendererHW::Reset(bool hardware_reset)
{
	// TODO: GSreset can come from the main thread too => crash
	// m_tc->RemoveAll();

	m_reset = true;

	GSRenderer::Reset(hardware_reset);
}

void GSRendererHW::UpdateSettings(const Pcsx2Config::GSOptions& old_config)
{
	GSRenderer::UpdateSettings(old_config);
	m_mipmap = (GSConfig.HWMipmap >= HWMipmapLevel::Basic);
	SetTCOffset();
}

void GSRendererHW::VSync(u32 field, bool registers_written)
{
	if (m_reset)
	{
		m_tc->RemoveAll();

		// Reset RT size.
		m_width = default_rt_size.x;
		m_height =  default_rt_size.y;

		m_reset = false;
	}

	if (GSConfig.LoadTextureReplacements)
		GSTextureReplacements::ProcessAsyncLoadedTextures();

	//Check if the frame buffer width or display width has changed
	SetScaling();

	GSRenderer::VSync(field, registers_written);

	m_tc->IncAge();

	if (m_tc->GetHashCacheMemoryUsage() > 1024 * 1024 * 1024)
	{
		Host::AddKeyedFormattedOSDMessage("HashCacheOverflow", 15.0f, "Hash cache has used %.2f MB of VRAM, disabling.",
			static_cast<float>(m_tc->GetHashCacheMemoryUsage()) / 1048576.0f);
		m_tc->RemoveAll();
		g_gs_device->PurgePool();
		GSConfig.TexturePreloading = TexturePreloadingLevel::Partial;
	}

	m_tc->PrintMemoryUsage();
	g_gs_device->PrintMemoryUsage();

	m_skip = 0;
	m_skip_offset = 0;
}

GSTexture* GSRendererHW::GetOutput(int i, int& y_offset)
{
	const GSRegDISPFB& DISPFB = m_regs->DISP[i].DISPFB;

	GIFRegTEX0 TEX0;

	TEX0.TBP0 = DISPFB.Block();
	TEX0.TBW = DISPFB.FBW;
	TEX0.PSM = DISPFB.PSM;

	const int videomode = static_cast<int>(GetVideoMode()) - 1;
	const int display_height = VideoModeOffsets[videomode].y * ((isinterlaced() && !m_regs->SMODE2.FFMD) ? 2 : 1);
	const int display_offset = GetResolutionOffset(i).y;
	int fb_height = std::min(GetFramebufferHeight(), display_height) + DISPFB.DBY;
	
	// If there is a negative vertical offset on the picture, we need to read more.
	if (display_offset < 0)
	{
		fb_height += -display_offset;
	}
	// TRACE(_T("[%d] GetOutput %d %05x (%d)\n"), (int)m_perfmon.GetFrame(), i, (int)TEX0.TBP0, (int)TEX0.PSM);

	GSTexture* t = NULL;

	if (GSTextureCache::Target* rt = m_tc->LookupTarget(TEX0, GetTargetSize(), fb_height))
	{
		t = rt->m_texture;

		const int delta = TEX0.TBP0 - rt->m_TEX0.TBP0;
		if (delta > 0 && DISPFB.FBW != 0)
		{
			const int pages = delta >> 5u;
			int y_pages = pages / DISPFB.FBW;
			y_offset = y_pages * GSLocalMemory::m_psm[DISPFB.PSM].pgs.y;
			GL_CACHE("Frame y offset %d pixels, unit %d", y_offset, i);
		}

#ifdef ENABLE_OGL_DEBUG
		if (s_dump)
		{
			if (s_savef && s_n >= s_saven)
			{
				t->Save(m_dump_root + StringUtil::StdStringFromFormat("%05d_f%lld_fr%d_%05x_%s.bmp", s_n, g_perfmon.GetFrame(), i, (int)TEX0.TBP0, psm_str(TEX0.PSM)));
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

	GSTextureCache::Target* rt = m_tc->LookupTarget(TEX0, GetTargetSize(), /*GetFrameRect(i).bottom*/ m_regs->DISP[m_regs->EXTBUF.FBIN & 1].DISPLAY.DH);

	GSTexture* t = rt->m_texture;

#ifdef ENABLE_OGL_DEBUG
	if (s_dump && s_savef && s_n >= s_saven)
		t->Save(m_dump_root + StringUtil::StdStringFromFormat("%05d_f%lld_fr%d_%05x_%s.bmp", s_n, g_perfmon.GetFrame(), 3, (int)TEX0.TBP0, psm_str(TEX0.PSM)));
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
		const size_t count = m_vertex.next;

		int i = (int)count * 2 - 4;
		GSVertex* s = &m_vertex.buff[count - 2];
		GSVertex* q = &m_vertex.buff[count * 2 - 4];
		u32* RESTRICT index = &m_index.buff[count * 3 - 6];

		alignas(16) static constexpr std::array<int, 8> tri_normal_indices = {{0, 1, 2, 1, 2, 3}};
		alignas(16) static constexpr std::array<int, 8> tri_swapped_indices = {{0, 1, 2, 1, 2, 3}};
		const bool index_swap = !g_gs_device->Features().provoking_vertex_last;
		const int* tri_indices = index_swap ? tri_swapped_indices.data() : tri_normal_indices.data();
		const GSVector4i indices_low(GSVector4i::load<true>(tri_indices));
		const GSVector4i indices_high(GSVector4i::loadl(tri_indices + 4));

		for (; i >= 0; i -= 4, s -= 2, q -= 4, index -= 6)
		{
			GSVertex v0 = s[0];
			GSVertex v1 = s[1];

			v0.RGBAQ = v1.RGBAQ;
			v0.XYZ.Z = v1.XYZ.Z;
			v0.FOG = v1.FOG;

			if (PRIM->TME && !PRIM->FST)
			{
				const GSVector4 st0 = GSVector4::loadl(&v0.ST.U64);
				const GSVector4 st1 = GSVector4::loadl(&v1.ST.U64);
				const GSVector4 Q = GSVector4(v1.RGBAQ.Q, v1.RGBAQ.Q, v1.RGBAQ.Q, v1.RGBAQ.Q);
				const GSVector4 st = st0.upld(st1) / Q;

				GSVector4::storel(&v0.ST.U64, st);
				GSVector4::storeh(&v1.ST.U64, st);

				v0.RGBAQ.Q = 1.0f;
				v1.RGBAQ.Q = 1.0f;
			}

			q[0] = v0;
			q[3] = v1;

			// swap x, s, u

			const u16 x = v0.XYZ.X;
			v0.XYZ.X = v1.XYZ.X;
			v1.XYZ.X = x;

			const float s = v0.ST.S;
			v0.ST.S = v1.ST.S;
			v1.ST.S = s;

			const u16 u = v0.U;
			v0.U = v1.U;
			v1.U = u;

			q[1] = v0;
			q[2] = v1;

			const GSVector4i i_splat(i);
			GSVector4i::store<false>(index, i_splat + indices_low);
			GSVector4i::storel(index + 4, i_splat + indices_high);
		}

		m_vertex.head = m_vertex.tail = m_vertex.next = count * 2;
		m_index.tail = count * 3;
	}
}

void GSRendererHW::EmulateAtst(GSVector4& FogColor_AREF, u8& ps_atst, const bool pass_2)
{
	static const u32 inverted_atst[] = {ATST_ALWAYS, ATST_NEVER, ATST_GEQUAL, ATST_GREATER, ATST_NOTEQUAL, ATST_LESS, ATST_LEQUAL, ATST_EQUAL};

	if (!m_context->TEST.ATE)
		return;

	// Check for pass 2, otherwise do pass 1.
	const int atst = pass_2 ? inverted_atst[m_context->TEST.ATST] : m_context->TEST.ATST;

	switch (atst)
	{
		case ATST_LESS:
			FogColor_AREF.a = (float)m_context->TEST.AREF - 0.1f;
			ps_atst = 1;
			break;
		case ATST_LEQUAL:
			FogColor_AREF.a = (float)m_context->TEST.AREF - 0.1f + 1.0f;
			ps_atst = 1;
			break;
		case ATST_GEQUAL:
			// Maybe a -1 trick multiplication factor could be used to merge with ATST_LEQUAL case
			FogColor_AREF.a = (float)m_context->TEST.AREF - 0.1f;
			ps_atst = 2;
			break;
		case ATST_GREATER:
			// Maybe a -1 trick multiplication factor could be used to merge with ATST_LESS case
			FogColor_AREF.a = (float)m_context->TEST.AREF - 0.1f + 1.0f;
			ps_atst = 2;
			break;
		case ATST_EQUAL:
			FogColor_AREF.a = (float)m_context->TEST.AREF;
			ps_atst = 3;
			break;
		case ATST_NOTEQUAL:
			FogColor_AREF.a = (float)m_context->TEST.AREF;
			ps_atst = 4;
			break;
		case ATST_NEVER: // Draw won't be done so no need to implement it in shader
		case ATST_ALWAYS:
		default:
			ps_atst = 0;
			break;
	}
}

// Fix the vertex position/tex_coordinate from 16 bits color to 32 bits color
void GSRendererHW::ConvertSpriteTextureShuffle(bool& write_ba, bool& read_ba)
{
	const size_t count = m_vertex.next;
	GSVertex* v = &m_vertex.buff[0];
	const GIFRegXYOFFSET& o = m_context->XYOFFSET;

	// vertex position is 8 to 16 pixels, therefore it is the 16-31 bits of the colors
	const int pos = (v[0].XYZ.X - o.OFX) & 0xFF;
	write_ba = (pos > 112 && pos < 136);

	// Read texture is 8 to 16 pixels (same as above)
	const float tw = (float)(1u << m_context->TEX0.TW);
	int tex_pos = (PRIM->FST) ? v[0].U : (int)(tw * v[0].ST.S);
	tex_pos &= 0xFF;
	read_ba = (tex_pos > 112 && tex_pos < 144);

	bool half_bottom = false;
	switch (GSConfig.UserHacks_HalfBottomOverride)
	{
		case 0:
			// Force Disabled.
			// Force Disabled will help games such as Xenosaga.
			// Xenosaga handles the half bottom as an vertex offset instead of a buffer offset which does the effect twice.
			// Half bottom won't trigger a cache miss that skip the draw because it is still the normal buffer but with a vertices offset.
			half_bottom = false;
			break;
		case 1:
			// Force Enabled.
			// Force Enabled will help games such as Superman Shadows of Apokolips, The Lord of the Rings: The Two Towers,
			// Demon Stone, Midnight Club 3.
			half_bottom = true;
			break;
		case -1:
		default:
			// Default, Automatic.
			// Here's the idea
			// TS effect is 16 bits but we emulate it on a 32 bits format
			// Normally this means we need to divide size by 2.
			//
			// Some games do two TS effects on each half of the buffer.
			// This makes a mess for us in the TC because we end up with two targets
			// when we only want one, thus half screen bug.
			//
			// 32bits emulation means we can do the effect once but double the size.
			// Test cases: Crash Twinsantiy and DBZ BT3
			// Test Case: NFS: HP2 splits the effect h:256 and h:192 so 64
			// Other games: Midnight Club 3 headlights, black bar in Xenosaga 3 dialogue,
			// Firefighter FD18 fire occlusion, PSI Ops half screen green overlay, Lord of the Rings - Two Towers,
			// Demon Stone , Sonic Unleashed, Lord of the Rings Two Towers,
			// Superman Shadow of Apokolips, Matrix Path of Neo, Big Mutha Truckers

			int maxvert = 0;
			int minvert = 4096;
			for (size_t i = 0; i < count; i ++)
			{
				int YCord = 0;

				if (!PRIM->FST)
					YCord = (int)((1 << m_context->TEX0.TH) * (v[i].ST.T / v[i].RGBAQ.Q));
				else
					YCord = (v[i].V >> 4);

				if (maxvert < YCord)
					maxvert = YCord;
				if (minvert > YCord)
					minvert = YCord;
			}

			half_bottom = minvert == 0 && m_r.height() <= maxvert;
			break;
	}

	if (PRIM->FST)
	{
		GL_INS("First vertex is  P: %d => %d    T: %d => %d", v[0].XYZ.X, v[1].XYZ.X, v[0].U, v[1].U);

		for (size_t i = 0; i < count; i += 2)
		{
			if (write_ba)
				v[i].XYZ.X   -= 128u;
			else
				v[i+1].XYZ.X += 128u;

			if (read_ba)
				v[i].U       -= 128u;
			else
				v[i+1].U     += 128u;

			if (!half_bottom)
			{
				// Height is too big (2x).
				const int tex_offset = v[i].V & 0xF;
				const GSVector4i offset(o.OFY, tex_offset, o.OFY, tex_offset);

				GSVector4i tmp(v[i].XYZ.Y, v[i].V, v[i + 1].XYZ.Y, v[i + 1].V);
				tmp = GSVector4i(tmp - offset).srl32(1) + offset;

				v[i].XYZ.Y = (u16)tmp.x;
				v[i].V = (u16)tmp.y;
				v[i + 1].XYZ.Y = (u16)tmp.z;
				v[i + 1].V = (u16)tmp.w;
			}
		}
	}
	else
	{
		const float offset_8pix = 8.0f / tw;
		GL_INS("First vertex is  P: %d => %d    T: %f => %f (offset %f)", v[0].XYZ.X, v[1].XYZ.X, v[0].ST.S, v[1].ST.S, offset_8pix);

		for (size_t i = 0; i < count; i += 2)
		{
			if (write_ba)
				v[i].XYZ.X   -= 128u;
			else
				v[i+1].XYZ.X += 128u;

			if (read_ba)
				v[i].ST.S    -= offset_8pix;
			else
				v[i+1].ST.S  += offset_8pix;

			if (!half_bottom)
			{
				// Height is too big (2x).
				const GSVector4i offset(o.OFY, o.OFY);

				GSVector4i tmp(v[i].XYZ.Y, v[i + 1].XYZ.Y);
				tmp = GSVector4i(tmp - offset).srl32(1) + offset;

				//fprintf(stderr, "Before %d, After %d\n", v[i + 1].XYZ.Y, tmp.y);
				v[i].XYZ.Y = (u16)tmp.x;
				v[i].ST.T /= 2.0f;
				v[i + 1].XYZ.Y = (u16)tmp.y;
				v[i + 1].ST.T /= 2.0f;
			}
		}
	}

	// Update vertex trace too. Avoid issue to compute bounding box
	if (write_ba)
		m_vt.m_min.p.x -= 8.0f;
	else
		m_vt.m_max.p.x += 8.0f;

	if (!half_bottom)
	{
		const float delta_Y = m_vt.m_max.p.y - m_vt.m_min.p.y;
		m_vt.m_max.p.y -= delta_Y / 2.0f;
	}

	if (read_ba)
		m_vt.m_min.t.x -= 8.0f;
	else
		m_vt.m_max.t.x += 8.0f;

	if (!half_bottom)
	{
		const float delta_T = m_vt.m_max.t.y - m_vt.m_min.t.y;
		m_vt.m_max.t.y -= delta_T / 2.0f;
	}
}

GSVector4 GSRendererHW::RealignTargetTextureCoordinate(const GSTextureCache::Source* tex)
{
	if (GSConfig.UserHacks_HalfPixelOffset <= 1 || GetUpscaleMultiplier() == 1)
		return GSVector4(0.0f);

	const GSVertex* v = &m_vertex.buff[0];
	const GSVector2& scale = tex->m_texture->GetScale();
	const bool linear = m_vt.IsRealLinear();
	const int t_position = v[0].U;
	GSVector4 half_offset(0.0f);

	// FIXME Let's start with something wrong same mess on X and Y
	// FIXME Maybe it will be enough to check linear

	if (PRIM->FST)
	{

		if (GSConfig.UserHacks_HalfPixelOffset == 3)
		{
			if (!linear && t_position == 8)
			{
				half_offset.x = 8;
				half_offset.y = 8;
			}
			else if (linear && t_position == 16)
			{
				half_offset.x = 16;
				half_offset.y = 16;
			}
			else if (m_vt.m_min.p.x == -0.5f)
			{
				half_offset.x = 8;
				half_offset.y = 8;
			}
		}
		else
		{
			if (!linear && t_position == 8)
			{
				half_offset.x = 8 - 8 / scale.x;
				half_offset.y = 8 - 8 / scale.y;
			}
			else if (linear && t_position == 16)
			{
				half_offset.x = 16 - 16 / scale.x;
				half_offset.y = 16 - 16 / scale.y;
			}
			else if (m_vt.m_min.p.x == -0.5f)
			{
				half_offset.x = 8;
				half_offset.y = 8;
			}
		}

		GL_INS("offset detected %f,%f t_pos %d (linear %d, scale %f)",
			half_offset.x, half_offset.y, t_position, linear, scale.x);
	}
	else if (m_vt.m_eq.q)
	{
		const float tw = (float)(1 << m_context->TEX0.TW);
		const float th = (float)(1 << m_context->TEX0.TH);
		const float q = v[0].RGBAQ.Q;

		// Tales of Abyss
		half_offset.x = 0.5f * q / tw;
		half_offset.y = 0.5f * q / th;

		GL_INS("ST offset detected %f,%f (linear %d, scale %f)",
			half_offset.x, half_offset.y, linear, scale.x);
	}

	return half_offset;
}

GSVector4i GSRendererHW::ComputeBoundingBox(const GSVector2& rtscale, const GSVector2i& rtsize)
{
	const GSVector4 scale = GSVector4(rtscale.x, rtscale.y);
	const GSVector4 offset = GSVector4(-1.0f, 1.0f); // Round value
	const GSVector4 box = m_vt.m_min.p.xyxy(m_vt.m_max.p) + offset.xxyy();
	return GSVector4i(box * scale.xyxy()).rintersect(GSVector4i(0, 0, rtsize.x, rtsize.y));
}

void GSRendererHW::MergeSprite(GSTextureCache::Source* tex)
{
	// Upscaling hack to avoid various line/grid issues
	if (GSConfig.UserHacks_MergePPSprite && tex && tex->m_target && (m_vt.m_primclass == GS_SPRITE_CLASS))
	{
		if (PRIM->FST && GSLocalMemory::m_psm[tex->m_TEX0.PSM].fmt < 2 && ((m_vt.m_eq.value & 0xCFFFF) == 0xCFFFF))
		{

			// Ideally the hack ought to be enabled in a true paving mode only. I don't know how to do it accurately
			// neither in a fast way. So instead let's just take the hypothesis that all sprites must have the same
			// size.
			// Tested on Tekken 5.
			const GSVertex* v = &m_vertex.buff[0];
			bool is_paving = true;
			// SSE optimization: shuffle m[1] to have (4*32 bits) X, Y, U, V
			const int first_dpX = v[1].XYZ.X - v[0].XYZ.X;
			const int first_dpU = v[1].U - v[0].U;
			for (size_t i = 0; i < m_vertex.next; i += 2)
			{
				const int dpX = v[i + 1].XYZ.X - v[i].XYZ.X;
				const int dpU = v[i + 1].U - v[i].U;
				if (dpX != first_dpX || dpU != first_dpU)
				{
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

			if (is_paving)
			{
				// Replace all sprite with a single fullscreen sprite.
				GSVertex* s = &m_vertex.buff[0];

				s[0].XYZ.X = static_cast<u16>((16.0f * m_vt.m_min.p.x) + m_context->XYOFFSET.OFX);
				s[1].XYZ.X = static_cast<u16>((16.0f * m_vt.m_max.p.x) + m_context->XYOFFSET.OFX);
				s[0].XYZ.Y = static_cast<u16>((16.0f * m_vt.m_min.p.y) + m_context->XYOFFSET.OFY);
				s[1].XYZ.Y = static_cast<u16>((16.0f * m_vt.m_max.p.y) + m_context->XYOFFSET.OFY);

				s[0].U = static_cast<u16>(16.0f * m_vt.m_min.t.x);
				s[0].V = static_cast<u16>(16.0f * m_vt.m_min.t.y);
				s[1].U = static_cast<u16>(16.0f * m_vt.m_max.t.x);
				s[1].V = static_cast<u16>(16.0f * m_vt.m_max.t.y);

				m_vertex.head = m_vertex.tail = m_vertex.next = 2;
				m_index.tail = 2;
			}
		}
	}
}

GSVector2 GSRendererHW::GetTextureScaleFactor(const bool force_upscaling)
{
	GSVector2 scale_factor{ 1.0f, 1.0f };
	if (force_upscaling || CanUpscale())
	{
		const int multiplier = GetUpscaleMultiplier();
		scale_factor.x = multiplier;
		scale_factor.y = multiplier;
	}

	return scale_factor;
}

GSVector2 GSRendererHW::GetTextureScaleFactor()
{
	return GetTextureScaleFactor(false);
}

GSVector2i GSRendererHW::GetTargetSize()
{
	const GSVector2i t_size = { m_width, m_height };
	if (GetUpscaleMultiplier() == 1 || CanUpscale())
		return t_size;
	// Undo the upscaling for native resolution draws.
	const GSVector2 up_s = GetTextureScaleFactor(true);
	return {
		static_cast<int>(std::ceil(static_cast<float>(t_size.x) / up_s.x)),
		static_cast<int>(std::ceil(static_cast<float>(t_size.y) / up_s.y)),
	};
}

void GSRendererHW::InvalidateVideoMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r)
{
	// printf("[%d] InvalidateVideoMem %d,%d - %d,%d %05x (%d)\n", (int)m_perfmon.GetFrame(), r.left, r.top, r.right, r.bottom, (int)BITBLTBUF.DBP, (int)BITBLTBUF.DPSM);

	m_tc->InvalidateVideoMem(m_mem.GetOffset(BITBLTBUF.DBP, BITBLTBUF.DBW, BITBLTBUF.DPSM), r);
}

void GSRendererHW::InvalidateLocalMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r, bool clut)
{
	// printf("[%d] InvalidateLocalMem %d,%d - %d,%d %05x (%d)\n", (int)m_perfmon.GetFrame(), r.left, r.top, r.right, r.bottom, (int)BITBLTBUF.SBP, (int)BITBLTBUF.SPSM);

	if (clut)
		return; // FIXME

	m_tc->InvalidateLocalMem(m_mem.GetOffset(BITBLTBUF.SBP, BITBLTBUF.SBW, BITBLTBUF.SPSM), r);
}

void GSRendererHW::Move()
{
	const int sx = m_env.TRXPOS.SSAX;
	const int sy = m_env.TRXPOS.SSAY;
	const int dx = m_env.TRXPOS.DSAX;
	const int dy = m_env.TRXPOS.DSAY;

	const int w = m_env.TRXREG.RRW;
	const int h = m_env.TRXREG.RRH;

	if (m_tc->Move(m_env.BITBLTBUF.SBP, m_env.BITBLTBUF.SBW, m_env.BITBLTBUF.SPSM, sx, sy,
			m_env.BITBLTBUF.DBP, m_env.BITBLTBUF.DBW, m_env.BITBLTBUF.DPSM, dx, dy, w, h))
	{
		// Handled entirely in TC, no need to update local memory.
		return;
	}

	GSRenderer::Move();
}

u16 GSRendererHW::Interpolate_UV(float alpha, int t0, int t1)
{
	const float t = (1.0f - alpha) * t0 + alpha * t1;
	return (u16)t & ~0xF; // cheap rounding
}

float GSRendererHW::alpha0(int L, int X0, int X1)
{
	const int x = (X0 + 15) & ~0xF; // Round up
	return float(x - X0) / (float)L;
}

float GSRendererHW::alpha1(int L, int X0, int X1)
{
	const int x = (X1 - 1) & ~0xF; // Round down. Note -1 because right pixel isn't included in primitive so 0x100 must return 0.
	return float(x - X0) / (float)L;
}

void GSRendererHW::SwSpriteRender()
{
	// Supported drawing attributes
	ASSERT(PRIM->PRIM == GS_TRIANGLESTRIP || PRIM->PRIM == GS_SPRITE);
	ASSERT(!PRIM->FGE); // No FOG
	ASSERT(!PRIM->AA1); // No antialiasing
	ASSERT(!PRIM->FIX); // Normal fragment value control

	ASSERT(!m_env.DTHE.DTHE); // No dithering

	ASSERT(!m_context->TEST.ATE); // No alpha test
	ASSERT(!m_context->TEST.DATE); // No destination alpha test
	ASSERT(!m_context->DepthRead() && !m_context->DepthWrite()); // No depth handling

	ASSERT(!m_context->TEX0.CSM); // No CLUT usage

	ASSERT(!m_env.PABE.PABE); // No PABE

	// PSMCT32 pixel format
	ASSERT(!PRIM->TME || (PRIM->TME && m_context->TEX0.PSM == PSM_PSMCT32));
	ASSERT(m_context->FRAME.PSM == PSM_PSMCT32);

	// No rasterization required
	ASSERT(PRIM->PRIM == GS_SPRITE
		|| ((PRIM->IIP || m_vt.m_eq.rgba == 0xffff)
			&& m_vt.m_eq.z == 0x1
			&& (!PRIM->TME || PRIM->FST || m_vt.m_eq.q == 0x1)));  // Check Q equality only if texturing enabled and STQ coords used

	const bool texture_mapping_enabled = PRIM->TME;

	const GSVector4i r = m_r;

#ifndef NDEBUG
	const int tw = 1 << m_context->TEX0.TW;
	const int th = 1 << m_context->TEX0.TH;
	const float meas_tw = m_vt.m_max.t.x - m_vt.m_min.t.x;
	const float meas_th = m_vt.m_max.t.y - m_vt.m_min.t.y;
	ASSERT(!PRIM->TME || (abs(meas_tw - r.width()) <= SSR_UV_TOLERANCE && abs(meas_th - r.height()) <= SSR_UV_TOLERANCE)); // No input texture min/mag, if any.
	ASSERT(!PRIM->TME || (abs(m_vt.m_min.t.x) <= SSR_UV_TOLERANCE && abs(m_vt.m_min.t.y) <= SSR_UV_TOLERANCE && abs(meas_tw - tw) <= SSR_UV_TOLERANCE && abs(meas_th - th) <= SSR_UV_TOLERANCE)); // No texture UV wrap, if any.
#endif

	GIFRegTRXPOS trxpos = {};

	trxpos.DSAX = r.x;
	trxpos.DSAY = r.y;
	trxpos.SSAX = static_cast<int>(m_vt.m_min.t.x / 2) * 2; // Rounded down to closest even integer.
	trxpos.SSAY = static_cast<int>(m_vt.m_min.t.y / 2) * 2;

	ASSERT(r.x % 2 == 0 && r.y % 2 == 0);

	GIFRegTRXREG trxreg = {};

	trxreg.RRW = r.width();
	trxreg.RRH = r.height();

	ASSERT(r.width() % 2 == 0 && r.height() % 2 == 0);

	// SW rendering code, mainly taken from GSState::Move(), TRXPOS.DIR{X,Y} management excluded

	const int sx = trxpos.SSAX;
	int sy = trxpos.SSAY;
	const int dx = trxpos.DSAX;
	int dy = trxpos.DSAY;
	const int w = trxreg.RRW;
	const int h = trxreg.RRH;

	GL_INS("SwSpriteRender: Dest 0x%x W:%d F:%s, size(%d %d)", m_context->FRAME.Block(), m_context->FRAME.FBW, psm_str(m_context->FRAME.PSM), w, h);

	const GSOffset& spo = m_context->offset.tex;
	const GSOffset& dpo = m_context->offset.fb;

	const bool alpha_blending_enabled = PRIM->ABE;

	const GSVertex& v = m_vertex.buff[m_index.buff[m_index.tail - 1]]; // Last vertex.
	const GSVector4i vc = GSVector4i(v.RGBAQ.R, v.RGBAQ.G, v.RGBAQ.B, v.RGBAQ.A) // 0x000000AA000000BB000000GG000000RR
							  .ps32(); // 0x00AA00BB00GG00RR00AA00BB00GG00RR

	const GSVector4i a_mask = GSVector4i::xff000000().u8to16(); // 0x00FF00000000000000FF000000000000

	const bool fb_mask_enabled = m_context->FRAME.FBMSK != 0x0;
	const GSVector4i fb_mask = GSVector4i(m_context->FRAME.FBMSK).u8to16(); // 0x00AA00BB00GG00RR00AA00BB00GG00RR

	const u8 tex0_tfx = m_context->TEX0.TFX;
	const u8 tex0_tcc = m_context->TEX0.TCC;
	const u8 alpha_a = m_context->ALPHA.A;
	const u8 alpha_b = m_context->ALPHA.B;
	const u8 alpha_c = m_context->ALPHA.C;
	const u8 alpha_d = m_context->ALPHA.D;
	const u8 alpha_fix = m_context->ALPHA.FIX;

	if (texture_mapping_enabled)
		m_tc->InvalidateLocalMem(spo, GSVector4i(sx, sy, sx + w, sy + h));
	constexpr bool invalidate_local_mem_before_fb_read = false;
	if (invalidate_local_mem_before_fb_read && (alpha_blending_enabled || fb_mask_enabled))
		m_tc->InvalidateLocalMem(dpo, m_r);

	for (int y = 0; y < h; y++, ++sy, ++dy)
	{
		const auto& spa = spo.paMulti(m_mem.vm32(), sx, sy);
		const auto& dpa = dpo.paMulti(m_mem.vm32(), dx, dy);

		ASSERT(w % 2 == 0);

		for (int x = 0; x < w; x += 2)
		{
			u32* di = dpa.value(x);
			ASSERT(di + 1 == dpa.value(x + 1)); // Destination pixel pair is adjacent in memory

			GSVector4i sc;
			if (texture_mapping_enabled)
			{
				const u32* si = spa.value(x);
				// Read 2 source pixel colors
				ASSERT(si + 1 == spa.value(x + 1)); // Source pixel pair is adjacent in memory
				sc = GSVector4i::loadl(si).u8to16(); // 0x00AA00BB00GG00RR00aa00bb00gg00rr

				// Apply TFX
				ASSERT(tex0_tfx == 0 || tex0_tfx == 1);
				if (tex0_tfx == 0)
					sc = sc.mul16l(vc).srl16(7).clamp8(); // clamp((sc * vc) >> 7, 0, 255), srl16 is ok because 16 bit values are unsigned

				if (tex0_tcc == 0)
					sc = sc.blend(vc, a_mask);
			}
			else
				sc = vc;

			// No FOG

			GSVector4i dc0;
			GSVector4i dc;

			if (alpha_blending_enabled || fb_mask_enabled)
			{
				// Read 2 destination pixel colors
				dc0 = GSVector4i::loadl(di).u8to16(); // 0x00AA00BB00GG00RR00aa00bb00gg00rr
			}

			if (alpha_blending_enabled)
			{
				// Blending
				const GSVector4i A = alpha_a == 0 ? sc : alpha_a == 1 ? dc0 : GSVector4i::zero();
				const GSVector4i B = alpha_b == 0 ? sc : alpha_b == 1 ? dc0 : GSVector4i::zero();
				const GSVector4i C = alpha_c == 2 ? GSVector4i(alpha_fix).xxxx().ps32() : (alpha_c == 0 ? sc : dc0).yyww() // 0x00AA00BB00AA00BB00aa00bb00aa00bb
																							  .srl32(16) // 0x000000AA000000AA000000aa000000aa
																							  .ps32() // 0x00AA00AA00aa00aa00AA00AA00aa00aa
																							  .xxyy(); // 0x00AA00AA00AA00AA00aa00aa00aa00aa
				const GSVector4i D = alpha_d == 0 ? sc : alpha_d == 1 ? dc0 : GSVector4i::zero();
				dc = A.sub16(B).mul16l(C).sra16(7).add16(D); // (((A - B) * C) >> 7) + D, must use sra16 due to signed 16 bit values.
				// dc alpha channels (dc.u16[3], dc.u16[7]) dirty
			}
			else
				dc = sc;

			// No dithering

			// Clamping
			if (m_env.COLCLAMP.CLAMP)
				dc = dc.clamp8(); // clamp(dc, 0, 255)
			else
				dc = dc.sll16(8).srl16(8); // Mask, lower 8 bits enabled per channel

			// No Alpha Correction
			ASSERT(m_context->FBA.FBA == 0);
			dc = dc.blend(sc, a_mask);
			// dc alpha channels valid

			// Frame buffer mask
			if (fb_mask_enabled)
				dc = dc.blend(dc0, fb_mask);

			// Store 2 pixel colors
			dc = dc.pu16(GSVector4i::zero()); // 0x0000000000000000AABBGGRRaabbggrr
			GSVector4i::storel(di, dc);
		}
	}

	m_tc->InvalidateVideoMem(dpo, m_r);
}

bool GSRendererHW::CanUseSwSpriteRender()
{
	const GSVector4i r = m_r;
	if (r.x % 2 != 0 || r.y % 2 != 0)
		return false; // Even offset.
	const int w = r.width();
	const int h = r.height();
	if (w % 2 != 0 || h % 2 != 0)
		return false; // Even size.
	if (w > 64 || h > 64)
		return false; // Small draw.
	if (PRIM->PRIM != GS_SPRITE
		&& ((PRIM->IIP && m_vt.m_eq.rgba != 0xffff)
			|| (PRIM->TME && !PRIM->FST && m_vt.m_eq.q != 0x1)
			|| m_vt.m_eq.z != 0x1)) // No rasterization
		return false;
	if (m_vt.m_primclass != GS_TRIANGLE_CLASS && m_vt.m_primclass != GS_SPRITE_CLASS) // Triangle or sprite class prims
		return false;
	if (PRIM->PRIM != GS_TRIANGLESTRIP && PRIM->PRIM != GS_SPRITE) // Triangle strip or sprite draw
		return false;
	if (m_vt.m_primclass == GS_TRIANGLE_CLASS && (PRIM->PRIM != GS_TRIANGLESTRIP || m_vertex.tail != 4)) // If triangle class, strip draw with 4 vertices (two prims, emulating single sprite prim)
		return false;
	// TODO If GS_TRIANGLESTRIP draw, check that the draw is axis aligned
	if (m_vt.m_primclass == GS_SPRITE_CLASS && (PRIM->PRIM != GS_SPRITE || m_vertex.tail != 2)) // If sprite class, sprite draw with 2 vertices (one prim)
		return false;
	if (m_context->DepthRead() || m_context->DepthWrite()) // No depth handling
		return false;
	if (m_context->FRAME.PSM != PSM_PSMCT32) // Frame buffer format is 32 bit color
		return false;
	if (PRIM->TME)
	{
		// Texture mapping enabled

		if (m_context->TEX0.PSM != PSM_PSMCT32) // Input texture format is 32 bit color
			return false;
		if (IsMipMapDraw()) // No mipmapping.
			return false;
		const int tw = 1 << m_context->TEX0.TW;
		const int th = 1 << m_context->TEX0.TH;
		const float meas_tw = m_vt.m_max.t.x - m_vt.m_min.t.x;
		const float meas_th = m_vt.m_max.t.y - m_vt.m_min.t.y;
		if (abs(m_vt.m_min.t.x) > SSR_UV_TOLERANCE ||
			abs(m_vt.m_min.t.y) > SSR_UV_TOLERANCE ||
			abs(meas_tw - tw) > SSR_UV_TOLERANCE ||
			abs(meas_th - th) > SSR_UV_TOLERANCE) // No UV wrapping.
			return false;
		if (abs(meas_tw - w) > SSR_UV_TOLERANCE || abs(meas_th - h) > SSR_UV_TOLERANCE) // No texture width or height mag/min.
			return false;
	}

	// The draw call is a good candidate for using the SwSpriteRender to replace the GPU draw
	// However, some draw attributes might not be supported yet by the SwSpriteRender,
	// so if any bug occurs in using it, enabling debug build would probably
	// make failing some of the assertions used in the SwSpriteRender to highlight its limitations.
	// In that case, either the condition can be added here to discard the draw, or the
	// SwSpriteRender can be improved by adding the missing features.
	return true;
}

template <bool linear>
void GSRendererHW::RoundSpriteOffset()
{
//#define DEBUG_U
//#define DEBUG_V
#if defined(DEBUG_V) || defined(DEBUG_U)
	bool debug = linear;
#endif
	const size_t count = m_vertex.next;
	GSVertex* v = &m_vertex.buff[0];

	for (size_t i = 0; i < count; i += 2)
	{
		// Performance note: if it had any impact on perf, someone would port it to SSE (AKA GSVector)

		// Compute the coordinate of first and last texels (in native with a linear filtering)
		const int ox = m_context->XYOFFSET.OFX;
		const int X0 = v[i].XYZ.X - ox;
		const int X1 = v[i + 1].XYZ.X - ox;
		const int Lx = (v[i + 1].XYZ.X - v[i].XYZ.X);
		const float ax0 = alpha0(Lx, X0, X1);
		const float ax1 = alpha1(Lx, X0, X1);
		const u16 tx0 = Interpolate_UV(ax0, v[i].U, v[i + 1].U);
		const u16 tx1 = Interpolate_UV(ax1, v[i].U, v[i + 1].U);
#ifdef DEBUG_U
		if (debug)
		{
			fprintf(stderr, "u0:%d and u1:%d\n", v[i].U, v[i + 1].U);
			fprintf(stderr, "a0:%f and a1:%f\n", ax0, ax1);
			fprintf(stderr, "t0:%d and t1:%d\n", tx0, tx1);
		}
#endif

		const int oy = m_context->XYOFFSET.OFY;
		const int Y0 = v[i].XYZ.Y - oy;
		const int Y1 = v[i + 1].XYZ.Y - oy;
		const int Ly = (v[i + 1].XYZ.Y - v[i].XYZ.Y);
		const float ay0 = alpha0(Ly, Y0, Y1);
		const float ay1 = alpha1(Ly, Y0, Y1);
		const u16 ty0 = Interpolate_UV(ay0, v[i].V, v[i + 1].V);
		const u16 ty1 = Interpolate_UV(ay1, v[i].V, v[i + 1].V);
#ifdef DEBUG_V
		if (debug)
		{
			fprintf(stderr, "v0:%d and v1:%d\n", v[i].V, v[i + 1].V);
			fprintf(stderr, "a0:%f and a1:%f\n", ay0, ay1);
			fprintf(stderr, "t0:%d and t1:%d\n", ty0, ty1);
		}
#endif

#ifdef DEBUG_U
		if (debug)
			fprintf(stderr, "GREP_BEFORE %d => %d\n", v[i].U, v[i + 1].U);
#endif
#ifdef DEBUG_V
		if (debug)
			fprintf(stderr, "GREP_BEFORE %d => %d\n", v[i].V, v[i + 1].V);
#endif

#if 1
		// Use rounded value of the newly computed texture coordinate. It ensures
		// that sampling will remains inside texture boundary
		//
		// Note for bilinear: by definition it will never work correctly! A sligh modification
		// of interpolation migth trigger a discard (with alpha testing)
		// Let's use something simple that correct really bad case (for a couple of 2D games).
		// I hope it won't create too much glitches.
		if (linear)
		{
			const int Lu = v[i + 1].U - v[i].U;
			// Note 32 is based on taisho-mononoke
			if ((Lu > 0) && (Lu <= (Lx + 32)))
			{
				v[i + 1].U -= 8;
			}
		}
		else
		{
			if (tx0 <= tx1)
			{
				v[i].U = tx0;
				v[i + 1].U = tx1 + 16;
			}
			else
			{
				v[i].U = tx0 + 15;
				v[i + 1].U = tx1;
			}
		}
#endif
#if 1
		if (linear)
		{
			const int Lv = v[i + 1].V - v[i].V;
			if ((Lv > 0) && (Lv <= (Ly + 32)))
			{
				v[i + 1].V -= 8;
			}
		}
		else
		{
			if (ty0 <= ty1)
			{
				v[i].V = ty0;
				v[i + 1].V = ty1 + 16;
			}
			else
			{
				v[i].V = ty0 + 15;
				v[i + 1].V = ty1;
			}
		}
#endif

#ifdef DEBUG_U
		if (debug)
			fprintf(stderr, "GREP_AFTER %d => %d\n\n", v[i].U, v[i + 1].U);
#endif
#ifdef DEBUG_V
		if (debug)
			fprintf(stderr, "GREP_AFTER %d => %d\n\n", v[i].V, v[i + 1].V);
#endif
	}
}

void GSRendererHW::Draw()
{
	if (s_dump && (s_n >= s_saven))
	{
		std::string s;

		// Dump Register state
		s = StringUtil::StdStringFromFormat("%05d_context.txt", s_n);

		m_env.Dump(m_dump_root + s);
		m_context->Dump(m_dump_root + s);

		// Dump vertices
		s = StringUtil::StdStringFromFormat("%05d_vertex.txt", s_n);
		DumpVertices(m_dump_root + s);
	}
	if (IsBadFrame())
	{
		GL_INS("Warning skipping a draw call (%d)", s_n);
		return;
	}
	GL_PUSH("HW Draw %d", s_n);

	const GSDrawingEnvironment& env = m_env;
	GSDrawingContext* context = m_context;
	const GSLocalMemory::psm_t& tex_psm = GSLocalMemory::m_psm[m_context->TEX0.PSM];

	if (!context->FRAME.FBW)
	{
		GL_CACHE("Skipping draw with FRAME.FBW = 0.");
		return;
	}

	// When the format is 24bit (Z or C), DATE ceases to function.
	// It was believed that in 24bit mode all pixels pass because alpha doesn't exist
	// however after testing this on a PS2 it turns out nothing passes, it ignores the draw.
	if ((m_context->FRAME.PSM & 0xF) == PSM_PSMCT24 && m_context->TEST.DATE)
	{
		GL_CACHE("DATE on a 24bit format, Frame PSM %x", m_context->FRAME.PSM);
		return;
	}

	// Fix TEX0 size
	if (PRIM->TME && !IsMipMapActive())
		m_context->ComputeFixedTEX0(m_vt.m_min.t.xyxy(m_vt.m_max.t));

	// skip alpha test if possible
	// Note: do it first so we know if frame/depth writes are masked

	const GIFRegTEST TEST = context->TEST;
	const GIFRegFRAME FRAME = context->FRAME;
	const GIFRegZBUF ZBUF = context->ZBUF;

	u32 fm = context->FRAME.FBMSK;
	u32 zm = context->ZBUF.ZMSK || context->TEST.ZTE == 0 ? 0xffffffff : 0;
	const u32 fm_mask = GSLocalMemory::m_psm[m_context->FRAME.PSM].fmsk;

	// Note required to compute TryAlphaTest below. So do it now.
	if (PRIM->TME && tex_psm.pal > 0)
		m_mem.m_clut.Read32(context->TEX0, env.TEXA);

	//  Test if we can optimize Alpha Test as a NOP
	context->TEST.ATE = context->TEST.ATE && !GSRenderer::TryAlphaTest(fm, fm_mask, zm);

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
			(context->FRAME.FBP == context->ZBUF.ZBP && !PRIM->TME && zm == 0 && (fm & fm_mask) == 0 && context->TEST.ZTE)
			);

	if (no_rt && no_ds)
	{
		GL_CACHE("Skipping draw with no color nor depth output.");
		return;
	}

	const bool draw_sprite_tex = PRIM->TME && (m_vt.m_primclass == GS_SPRITE_CLASS);
	const GSVector4 delta_p = m_vt.m_max.p - m_vt.m_min.p;
	const bool single_page = (delta_p.x <= 64.0f) && (delta_p.y <= 64.0f);

	if (m_channel_shuffle)
	{
		m_channel_shuffle = draw_sprite_tex && (m_context->TEX0.PSM == PSM_PSMT8) && single_page;
		if (m_channel_shuffle)
		{
			GL_CACHE("Channel shuffle effect detected SKIP");
			return;
		}
	}
	else if (draw_sprite_tex && m_context->FRAME.Block() == m_context->TEX0.TBP0)
	{
		// Special post-processing effect
		if ((m_context->TEX0.PSM == PSM_PSMT8) && single_page)
		{
			GL_INS("Channel shuffle effect detected");
			m_channel_shuffle = true;
		}
		else
		{
			GL_DBG("Special post-processing effect not supported");
			m_channel_shuffle = false;
		}
	}
	else
	{
		m_channel_shuffle = false;
	}

	GIFRegTEX0 TEX0;

	m_src = nullptr;
	m_texture_shuffle = false;

	if (PRIM->TME)
	{
		GIFRegCLAMP MIP_CLAMP = context->CLAMP;
		GSVector2i hash_lod_range(0, 0);
		m_lod = GSVector2i(0, 0);

		// Code from the SW renderer
		if (IsMipMapActive())
		{
			const int interpolation = (context->TEX1.MMIN & 1) + 1; // 1: round, 2: tri

			int k = (m_context->TEX1.K + 8) >> 4;
			int lcm = m_context->TEX1.LCM;
			const int mxl = std::min<int>((int)m_context->TEX1.MXL, 6);

			if ((int)m_vt.m_lod.x >= mxl)
			{
				k = mxl; // set lod to max level
				lcm = 1; // constant lod
			}

			if (PRIM->FST)
			{
				ASSERT(lcm == 1);
				ASSERT(((m_vt.m_min.t.uph(m_vt.m_max.t) == GSVector4::zero()).mask() & 3) == 3); // ratchet and clank (menu)

				lcm = 1;
			}

			if (lcm == 1)
			{
				m_lod.x = std::max<int>(k, 0);
				m_lod.y = m_lod.x;
			}
			else
			{
				// Not constant but who care !
				if (interpolation == 2)
				{
					// Mipmap Linear. Both layers are sampled, only take the big one
					m_lod.x = std::max<int>((int)floor(m_vt.m_lod.x), 0);
				}
				else
				{
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

			// upload the full chain (with offset) for the hash cache, in case some other texture uses more levels
			// for basic mipmapping, we can get away with just doing the base image, since all the mips get generated anyway.
			hash_lod_range = GSVector2i(m_lod.x, (GSConfig.HWMipmap == HWMipmapLevel::Full) ? mxl : m_lod.x);

			MIP_CLAMP.MINU >>= m_lod.x;
			MIP_CLAMP.MINV >>= m_lod.x;
			MIP_CLAMP.MAXU >>= m_lod.x;
			MIP_CLAMP.MAXV >>= m_lod.x;

			for (int i = 0; i < m_lod.x; i++)
			{
				m_vt.m_min.t *= 0.5f;
				m_vt.m_max.t *= 0.5f;
			}

			GL_CACHE("Mipmap LOD %d %d (%f %f) new size %dx%d (K %d L %u)", m_lod.x, m_lod.y, m_vt.m_lod.x, m_vt.m_lod.y, 1 << TEX0.TW, 1 << TEX0.TH, m_context->TEX1.K, m_context->TEX1.L);
		}
		else
		{
			TEX0 = GetTex0Layer(0);
		}

		m_context->offset.tex = m_mem.GetOffset(TEX0.TBP0, TEX0.TBW, TEX0.PSM);

		TextureMinMaxResult tmm = GetTextureMinMax(TEX0, MIP_CLAMP, m_vt.IsLinear());

		m_src = tex_psm.depth ? m_tc->LookupDepthSource(TEX0, env.TEXA, tmm.coverage) :
			m_tc->LookupSource(TEX0, env.TEXA, tmm.coverage, (GSConfig.HWMipmap >= HWMipmapLevel::Basic ||
				GSConfig.UserHacks_TriFilter == TriFiltering::Forced) ? &hash_lod_range : nullptr);

		const int tw = 1 << TEX0.TW;
		const int th = 1 << TEX0.TH;
		// Texture clamp optimizations (try to move everything to sampler hardware)
		if (m_context->CLAMP.WMS == CLAMP_REGION_CLAMP && MIP_CLAMP.MINU == 0 && MIP_CLAMP.MAXU == tw - 1)
			m_context->CLAMP.WMS = CLAMP_CLAMP;
		else if (m_context->CLAMP.WMS == CLAMP_REGION_REPEAT && MIP_CLAMP.MINU == tw - 1 && MIP_CLAMP.MAXU == 0)
			m_context->CLAMP.WMS = CLAMP_REPEAT;
		else if ((m_context->CLAMP.WMS & 2) && !(tmm.uses_boundary & TextureMinMaxResult::USES_BOUNDARY_U))
			m_context->CLAMP.WMS = CLAMP_CLAMP;
		if (m_context->CLAMP.WMT == CLAMP_REGION_CLAMP && MIP_CLAMP.MINV == 0 && MIP_CLAMP.MAXV == th - 1)
			m_context->CLAMP.WMT = CLAMP_CLAMP;
		else if (m_context->CLAMP.WMT == CLAMP_REGION_REPEAT && MIP_CLAMP.MINV == th - 1 && MIP_CLAMP.MAXV == 0)
			m_context->CLAMP.WMT = CLAMP_REPEAT;
		else if ((m_context->CLAMP.WMT & 2) && !(tmm.uses_boundary & TextureMinMaxResult::USES_BOUNDARY_V))
			m_context->CLAMP.WMT = CLAMP_CLAMP;

		// If m_src is from a target that isn't the same size as the texture, texture sample edge modes won't work quite the same way
		// If the game actually tries to access stuff outside of the rendered target, it was going to get garbage anyways so whatever
		// But the game could issue reads that wrap to valid areas, so move wrapping to the shader if wrapping is used
		GSVector4i unscaled_size = GSVector4i(GSVector4(m_src->m_texture->GetSize()) / GSVector4(m_src->m_texture->GetScale()));
		if (m_context->CLAMP.WMS == CLAMP_REPEAT && (tmm.uses_boundary & TextureMinMaxResult::USES_BOUNDARY_U) && unscaled_size.x != tw)
		{
			// Our shader-emulated region repeat doesn't upscale :(
			// Try to avoid it if possible
			// TODO: Upscale-supporting shader-emulated region repeat
			if (unscaled_size.x < tw && m_vt.m_min.t.x > -(tw - unscaled_size.x) && m_vt.m_max.t.x < tw)
			{
				// Game only extends into data we don't have (but doesn't wrap around back onto good data), clamp seems like the most reasonable solution
				m_context->CLAMP.WMS = CLAMP_CLAMP;
			}
			else
			{
				m_context->CLAMP.WMS = CLAMP_REGION_REPEAT;
				m_context->CLAMP.MINU = (1 << m_context->TEX0.TW) - 1;
				m_context->CLAMP.MAXU = 0;
			}
		}
		if (m_context->CLAMP.WMT == CLAMP_REPEAT && (tmm.uses_boundary & TextureMinMaxResult::USES_BOUNDARY_V) && unscaled_size.y != th)
		{
			if (unscaled_size.y < th && m_vt.m_min.t.y > -(th - unscaled_size.y) && m_vt.m_max.t.y < th)
			{
				m_context->CLAMP.WMT = CLAMP_CLAMP;
			}
			else
			{
				m_context->CLAMP.WMT = CLAMP_REGION_REPEAT;
				m_context->CLAMP.MINV = (1 << m_context->TEX0.TH) - 1;
				m_context->CLAMP.MAXV = 0;
			}
		}

		// Round 2
		if (IsMipMapActive() && GSConfig.HWMipmap == HWMipmapLevel::Full && !tex_psm.depth && !m_src->m_from_hash_cache)
		{
			// Upload remaining texture layers
			const GSVector4 tmin = m_vt.m_min.t;
			const GSVector4 tmax = m_vt.m_max.t;

			for (int layer = m_lod.x + 1; layer <= m_lod.y; layer++)
			{
				const GIFRegTEX0& MIP_TEX0 = GetTex0Layer(layer);

				m_context->offset.tex = m_mem.GetOffset(MIP_TEX0.TBP0, MIP_TEX0.TBW, MIP_TEX0.PSM);

				MIP_CLAMP.MINU >>= 1;
				MIP_CLAMP.MINV >>= 1;
				MIP_CLAMP.MAXU >>= 1;
				MIP_CLAMP.MAXV >>= 1;

				m_vt.m_min.t *= 0.5f;
				m_vt.m_max.t *= 0.5f;

				tmm = GetTextureMinMax(MIP_TEX0, MIP_CLAMP, m_vt.IsLinear());

				m_src->UpdateLayer(MIP_TEX0, tmm.coverage, layer - m_lod.x);
			}

			// we don't need to generate mipmaps since they were provided
			m_src->m_texture->ClearMipmapGenerationFlag();
			m_vt.m_min.t = tmin;
			m_vt.m_max.t = tmax;
		}

		// Hypothesis: texture shuffle is used as a postprocessing effect so texture will be an old target.
		// Initially code also tested the RT but it gives too much false-positive
		//
		// Both input and output are 16 bits and texture was initially 32 bits!
		m_texture_shuffle = (GSLocalMemory::m_psm[context->FRAME.PSM].bpp == 16) && (tex_psm.bpp == 16)
			&& draw_sprite_tex && m_src->m_32_bits_fmt;

		// Okami mustn't call this code
		if (m_texture_shuffle && m_vertex.next < 3 && PRIM->FST && ((m_context->FRAME.FBMSK & fm_mask) == 0))
		{
			// Avious dubious call to m_texture_shuffle on 16 bits games
			// The pattern is severals column of 8 pixels. A single sprite
			// smell fishy but a big sprite is wrong.

			// Shadow of Memories/Destiny shouldn't call this code.
			// Causes shadow flickering.
			const GSVertex* v = &m_vertex.buff[0];
			m_texture_shuffle = ((v[1].U - v[0].U) < 256) ||
				// Tomb Raider Angel of Darkness relies on this behavior to produce a fog effect.
				// In this case, the address of the framebuffer and texture are the same.
				// The game will take RG => BA and then the BA => RG of next pixels.
				// However, only RG => BA needs to be emulated because RG isn't used.
				m_context->FRAME.Block() == m_context->TEX0.TBP0 ||
				// DMC3, Onimusha 3 rely on this behavior.
				// They do fullscreen rectangle with scissor, then shift by 8 pixels, not done with recursion.
				// So we check if it's a TS effect by checking the scissor.
				((m_context->SCISSOR.SCAX1 - m_context->SCISSOR.SCAX0) < 32);

			GL_INS("WARNING: Possible misdetection of effect, texture shuffle is %s", m_texture_shuffle ? "Enabled" : "Disabled");
		}

		// Texture shuffle is not yet supported with strange clamp mode
		ASSERT(!m_texture_shuffle || (context->CLAMP.WMS < 3 && context->CLAMP.WMT < 3));

		if (m_src->m_target && m_context->TEX0.PSM == PSM_PSMT8 && single_page && draw_sprite_tex)
		{
			GL_INS("Channel shuffle effect detected (2nd shot)");
			m_channel_shuffle = true;
		}
		else
		{
			m_channel_shuffle = false;
		}
	}

	const GSVector2i t_size = GetTargetSize();

	TEX0.TBP0 = context->FRAME.Block();
	TEX0.TBW = context->FRAME.FBW;
	TEX0.PSM = context->FRAME.PSM;

	GSTextureCache::Target* rt = nullptr;
	GSTexture* rt_tex = nullptr;
	if (!no_rt)
	{
		rt = m_tc->LookupTarget(TEX0, t_size, GSTextureCache::RenderTarget, true, fm);
		rt_tex = rt->m_texture;
	}

	TEX0.TBP0 = context->ZBUF.Block();
	TEX0.TBW = context->FRAME.FBW;
	TEX0.PSM = context->ZBUF.PSM;

	GSTextureCache::Target* ds = nullptr;
	GSTexture* ds_tex = nullptr;
	if (!no_ds)
	{
		ds = m_tc->LookupTarget(TEX0, t_size, GSTextureCache::DepthStencil, context->DepthWrite());
		ds_tex = ds->m_texture;
	}

	if (rt)
	{
		// Be sure texture shuffle detection is properly propagated
		// Otherwise set or clear the flag (Code in texture cache only set the flag)
		// Note: it is important to clear the flag when RT is used as a real 16 bits target.
		rt->m_32_bits_fmt = m_texture_shuffle || (GSLocalMemory::m_psm[context->FRAME.PSM].bpp != 16);
	}

	// The rectangle of the draw
	m_r = GSVector4i(m_vt.m_min.p.xyxy(m_vt.m_max.p)).rintersect(GSVector4i(context->scissor.in));

	{
		const GSVector2 up_s = GetTextureScaleFactor();
		const int up_w = static_cast<int>(std::ceil(static_cast<float>(m_r.z) * up_s.x));
		const int up_h = static_cast<int>(std::ceil(static_cast<float>(m_r.w) * up_s.y));
		const int new_w = std::max(up_w, std::max(rt_tex ? rt_tex->GetWidth() : 0, ds_tex ? ds_tex->GetWidth() : 0));
		const int new_h = std::max(up_h, std::max(rt_tex ? rt_tex->GetHeight() : 0, ds_tex ? ds_tex->GetHeight() : 0));
		std::array<GSTextureCache::Target*, 2> ts{ rt, ds };
		for (GSTextureCache::Target* t : ts)
		{
			if (t)
			{
				// Adjust texture size to fit current draw if necessary.
				GSTexture* tex = t->m_texture;
				assert(up_s == tex->GetScale());
				const int w = tex->GetWidth();
				const int h = tex->GetHeight();
				if (w != new_w || h != new_h)
				{
					const bool is_rt = t == rt;
					t->m_texture = is_rt ?
						g_gs_device->CreateSparseRenderTarget(new_w, new_h, tex->GetFormat()) :
						g_gs_device->CreateSparseDepthStencil(new_w, new_h, tex->GetFormat());
					const GSVector4i r{ 0, 0, w, h };
					g_gs_device->CopyRect(tex, t->m_texture, r, 0, 0);
					g_gs_device->Recycle(tex);
					t->m_texture->SetScale(up_s);
					(is_rt ? rt_tex : ds_tex) = t->m_texture;
				}
			}
		}
	}

	if (m_src && m_src->m_shared_texture && m_src->m_texture != *m_src->m_from_target)
	{
		// Target texture changed, update reference.
		m_src->m_texture = *m_src->m_from_target;
	}

	if (s_dump)
	{
		const u64 frame = g_perfmon.GetFrame();

		std::string s;

		if (s_savet && s_n >= s_saven && m_src)
		{
			s = StringUtil::StdStringFromFormat("%05d_f%lld_itex_%05x_%s_%d%d_%02x_%02x_%02x_%02x.dds",
				s_n, frame, (int)context->TEX0.TBP0, psm_str(context->TEX0.PSM),
				(int)context->CLAMP.WMS, (int)context->CLAMP.WMT,
				(int)context->CLAMP.MINU, (int)context->CLAMP.MAXU,
				(int)context->CLAMP.MINV, (int)context->CLAMP.MAXV);

			m_src->m_texture->Save(m_dump_root + s);

			if (m_src->m_palette)
			{
				s = StringUtil::StdStringFromFormat("%05d_f%lld_itpx_%05x_%s.dds", s_n, frame, context->TEX0.CBP, psm_str(context->TEX0.CPSM));

				m_src->m_palette->Save(m_dump_root + s);
			}
		}

		if (s_save && s_n >= s_saven)
		{
			s = StringUtil::StdStringFromFormat("%05d_f%lld_rt0_%05x_%s.bmp", s_n, frame, context->FRAME.Block(), psm_str(context->FRAME.PSM));

			if (rt_tex)
				rt_tex->Save(m_dump_root + s);
		}

		if (s_savez && s_n >= s_saven)
		{
			s = StringUtil::StdStringFromFormat("%05d_f%lld_rz0_%05x_%s.bmp", s_n, frame, context->ZBUF.Block(), psm_str(context->ZBUF.PSM));

			if (ds_tex)
				ds_tex->Save(m_dump_root + s);
		}
	}

	if (m_hacks.m_oi && !(this->*m_hacks.m_oi)(rt_tex, ds_tex, m_src))
	{
		GL_INS("Warning skipping a draw call (%d)", s_n);
		return;
	}

	if (!OI_BlitFMV(rt, m_src, m_r))
	{
		GL_INS("Warning skipping a draw call (%d)", s_n);
		return;
	}

	if (!GSConfig.UserHacks_DisableSafeFeatures)
	{
		// Constant Direct Write without texture/test/blending (aka a GS mem clear)
		if ((m_vt.m_primclass == GS_SPRITE_CLASS) && !PRIM->TME // Direct write
				&& (!PRIM->ABE || m_context->ALPHA.IsOpaque()) // No transparency
				&& (m_context->FRAME.FBMSK == 0) // no color mask
				&& !m_context->TEST.ATE // no alpha test
				&& (!m_context->TEST.ZTE || m_context->TEST.ZTST == ZTST_ALWAYS) // no depth test
				&& (m_vt.m_eq.rgba == 0xFFFF) // constant color write
				&& m_r.x == 0 && m_r.y == 0) { // Likely full buffer write

			OI_GsMemClear();

			OI_DoubleHalfClear(rt_tex, ds_tex);
		}
	}

	// A couple of hack to avoid upscaling issue. So far it seems to impacts mostly sprite
	// Note: first hack corrects both position and texture coordinate
	// Note: second hack corrects only the texture coordinate
	if (CanUpscale() && (m_vt.m_primclass == GS_SPRITE_CLASS))
	{
		const size_t count = m_vertex.next;
		GSVertex* v = &m_vertex.buff[0];

		// Hack to avoid vertical black line in various games (ace combat/tekken)
		if (GSConfig.UserHacks_AlignSpriteX)
		{
			// Note for performance reason I do the check only once on the first
			// primitive
			const int win_position = v[1].XYZ.X - context->XYOFFSET.OFX;
			const bool unaligned_position = ((win_position & 0xF) == 8);
			const bool unaligned_texture = ((v[1].U & 0xF) == 0) && PRIM->FST; // I'm not sure this check is useful
			const bool hole_in_vertex = (count < 4) || (v[1].XYZ.X != v[2].XYZ.X);
			if (hole_in_vertex && unaligned_position && (unaligned_texture || !PRIM->FST))
			{
				// Normaly vertex are aligned on full pixels and texture in half
				// pixels. Let's extend the coverage of an half-pixel to avoid
				// hole after upscaling
				for (size_t i = 0; i < count; i += 2)
				{
					v[i + 1].XYZ.X += 8;
					// I really don't know if it is a good idea. Neither what to do for !PRIM->FST
					if (unaligned_texture)
						v[i + 1].U += 8;
				}
			}
		}

		// Noting to do if no texture is sampled
		if (PRIM->FST && draw_sprite_tex)
		{
			if ((GSConfig.UserHacks_RoundSprite > 1) || (GSConfig.UserHacks_RoundSprite == 1 && !m_vt.IsLinear()))
			{
				if (m_vt.IsLinear())
					RoundSpriteOffset<true>();
				else
					RoundSpriteOffset<false>();
			}
		}
		else
		{
			; // vertical line in Yakuza (note check m_userhacks_align_sprite_X behavior)
		}
	}

	//

	DrawPrims(rt_tex, ds_tex, m_src);

	//

	context->TEST = TEST;
	context->FRAME = FRAME;
	context->ZBUF = ZBUF;

	//

	// Temporary source *must* be invalidated before normal, because otherwise it'll be double freed.
	m_tc->InvalidateTemporarySource();

	//

	if ((fm & fm_mask) != fm_mask && rt)
	{
		//rt->m_valid = rt->m_valid.runion(r);
		rt->UpdateValidity(m_r);

		m_tc->InvalidateVideoMem(context->offset.fb, m_r, false);

		m_tc->InvalidateVideoMemType(GSTextureCache::DepthStencil, context->FRAME.Block());
	}

	if (zm != 0xffffffff && ds)
	{
		//ds->m_valid = ds->m_valid.runion(r);
		ds->UpdateValidity(m_r);

		m_tc->InvalidateVideoMem(context->offset.zb, m_r, false);

		m_tc->InvalidateVideoMemType(GSTextureCache::RenderTarget, context->ZBUF.Block());
	}

	//

	if (m_hacks.m_oo)
	{
		(this->*m_hacks.m_oo)();
	}

	if (s_dump)
	{
		const u64 frame = g_perfmon.GetFrame();

		std::string s;

		if (s_save && s_n >= s_saven)
		{
			s = StringUtil::StdStringFromFormat("%05d_f%lld_rt1_%05x_%s.bmp", s_n, frame, context->FRAME.Block(), psm_str(context->FRAME.PSM));

			if (rt_tex)
				rt_tex->Save(m_dump_root + s);
		}

		if (s_savez && s_n >= s_saven)
		{
			s = StringUtil::StdStringFromFormat("%05d_f%lld_rz1_%05x_%s.bmp", s_n, frame, context->ZBUF.Block(), psm_str(context->ZBUF.PSM));

			if (ds_tex)
				ds_tex->Save(m_dump_root + s);
		}

		if (s_savel > 0 && (s_n - s_saven) > s_savel)
		{
			s_dump = 0;
		}
	}

#ifdef DISABLE_HW_TEXTURE_CACHE
	if (rt)
		m_tc->Read(rt, m_r);
#endif
}

void GSRendererHW::SetupIA(const float& sx, const float& sy)
{
	GL_PUSH("IA");

	if (GSConfig.UserHacks_WildHack && !m_isPackedUV_HackFlag && PRIM->TME && PRIM->FST)
	{
		for (unsigned int i = 0; i < m_vertex.next; i++)
			m_vertex.buff[i].UV &= 0x3FEF3FEF;
	}
	const bool unscale_pt_ln = !GSConfig.UserHacks_DisableSafeFeatures && (GetUpscaleMultiplier() != 1);
	const GSDevice::FeatureSupport features = g_gs_device->Features();

	switch (m_vt.m_primclass)
	{
		case GS_POINT_CLASS:
			if (unscale_pt_ln)
			{
				if (features.point_expand)
				{
					m_conf.vs.point_size = true;
				}
				else if (features.geometry_shader)
				{
					m_conf.gs.expand = true;
					m_conf.cb_vs.point_size = GSVector2(16.0f * sx, 16.0f * sy);
				}
			}

			m_conf.gs.topology = GSHWDrawConfig::GSTopology::Point;
			m_conf.topology = GSHWDrawConfig::Topology::Point;
			m_conf.indices_per_prim = 1;
			break;

		case GS_LINE_CLASS:
			if (unscale_pt_ln)
			{
				if (features.line_expand)
				{
					m_conf.line_expand = true;
				}
				else if (features.geometry_shader)
				{
					m_conf.gs.expand = true;
					m_conf.cb_vs.point_size = GSVector2(16.0f * sx, 16.0f * sy);
				}
			}

			m_conf.gs.topology = GSHWDrawConfig::GSTopology::Line;
			m_conf.topology = GSHWDrawConfig::Topology::Line;
			m_conf.indices_per_prim = 2;
			break;

		case GS_SPRITE_CLASS:
			// Heuristics: trade-off
			// Lines: GPU conversion => ofc, more GPU. And also more CPU due to extra shader validation stage.
			// Triangles: CPU conversion => ofc, more CPU ;) more bandwidth (72 bytes / sprite)
			//
			// Note: severals openGL operation does draw call under the wood like texture upload. So even if
			// you do 10 consecutive draw with the geometry shader, you will still pay extra validation if new
			// texture are uploaded. (game Shadow Hearts)
			//
			// Note2: Due to MultiThreaded driver, Nvidia suffers less of the previous issue. Still it isn't free
			// Shadow Heart is 90 fps (gs) vs 113 fps (no gs)
			//
			// Note3: Some GPUs (Happens on GT 750m, not on Intel 5200) don't properly divide by large floats (e.g. FLT_MAX/FLT_MAX == 0)
			// Lines2Sprites predivides by Q, avoiding this issue, so always use it if m_vt.m_accurate_stq

			// If the draw calls contains few primitives. Geometry Shader gain with be rather small versus
			// the extra validation cost of the extra stage.
			//
			// Note: keep Geometry Shader in the replayer to ease debug.
			if (g_gs_device->Features().geometry_shader && !m_vt.m_accurate_stq && m_vertex.next > 32) // <=> 16 sprites (based on Shadow Hearts)
			{
				m_conf.gs.expand = true;

				m_conf.topology = GSHWDrawConfig::Topology::Line;
				m_conf.indices_per_prim = 2;
			}
			else
			{
				Lines2Sprites();

				m_conf.topology = GSHWDrawConfig::Topology::Triangle;
				m_conf.indices_per_prim = 6;
			}
			m_conf.gs.topology = GSHWDrawConfig::GSTopology::Sprite;
			break;

		case GS_TRIANGLE_CLASS:
			m_conf.gs.topology = GSHWDrawConfig::GSTopology::Triangle;
			m_conf.topology = GSHWDrawConfig::Topology::Triangle;
			m_conf.indices_per_prim = 3;
			break;

		default:
			__assume(0);
	}

	m_conf.verts = m_vertex.buff;
	m_conf.nverts = m_vertex.next;
	m_conf.indices = m_index.buff;
	m_conf.nindices = m_index.tail;
}

void GSRendererHW::EmulateZbuffer()
{
	if (m_context->TEST.ZTE)
	{
		m_conf.depth.ztst = m_context->TEST.ZTST;
		// AA1: Z is not written on lines since coverage is always less than 0x80.
		m_conf.depth.zwe = (m_context->ZBUF.ZMSK || (PRIM->AA1 && m_vt.m_primclass == GS_LINE_CLASS)) ? 0 : 1;
	}
	else
	{
		m_conf.depth.ztst = ZTST_ALWAYS;
	}

	// On the real GS we appear to do clamping on the max z value the format allows.
	// Clamping is done after rasterization.
	const u32 max_z = 0xFFFFFFFF >> (GSLocalMemory::m_psm[m_context->ZBUF.PSM].fmt * 8);
	const bool clamp_z = (u32)(GSVector4i(m_vt.m_max.p).z) > max_z;

	m_conf.cb_vs.max_depth = GSVector2i(0xFFFFFFFF);
	//ps_cb.MaxDepth = GSVector4(0.0f, 0.0f, 0.0f, 1.0f);
	m_conf.ps.zclamp = 0;

	if (clamp_z)
	{
		if (m_vt.m_primclass == GS_SPRITE_CLASS || m_vt.m_primclass == GS_POINT_CLASS)
		{
			m_conf.cb_vs.max_depth = GSVector2i(max_z);
		}
		else if (!m_context->ZBUF.ZMSK)
		{
			m_conf.cb_ps.TA_MaxDepth_Af.z = static_cast<float>(max_z) * 0x1p-32f;
			m_conf.ps.zclamp = 1;
		}
	}

	const GSVertex* v = &m_vertex.buff[0];
	// Minor optimization of a corner case (it allow to better emulate some alpha test effects)
	if (m_conf.depth.ztst == ZTST_GEQUAL && m_vt.m_eq.z && v[0].XYZ.Z == max_z)
	{
		GL_DBG("Optimize Z test GEQUAL to ALWAYS (%s)", psm_str(m_context->ZBUF.PSM));
		m_conf.depth.ztst = ZTST_ALWAYS;
	}
}

void GSRendererHW::EmulateTextureShuffleAndFbmask()
{
	// Uncomment to disable texture shuffle emulation.
	// m_texture_shuffle = false;

	bool enable_fbmask_emulation = false;
	const GSDevice::FeatureSupport features = g_gs_device->Features();
	if (features.texture_barrier)
	{
		enable_fbmask_emulation = GSConfig.AccurateBlendingUnit != AccBlendLevel::Minimum;
	}
	else
	{
		// FBmask blend level selection.
		// We do this becaue:
		// 1. D3D sucks.
		// 2. FB copy is slow, especially on triangle primitives which is unplayable with some games.
		// 3. SW blending isn't implemented yet.
		switch (GSConfig.AccurateBlendingUnit)
		{
			case AccBlendLevel::Maximum:
			case AccBlendLevel::Full:
			case AccBlendLevel::High:
			case AccBlendLevel::Medium:
				enable_fbmask_emulation = true;
				break;
			case AccBlendLevel::Basic:
				// Enable Fbmask emulation excluding triangle class because it is quite slow.
				enable_fbmask_emulation = (m_vt.m_primclass != GS_TRIANGLE_CLASS);
				break;
			case AccBlendLevel::Minimum:
				break;
		}
	}

	if (m_texture_shuffle)
	{
		m_conf.ps.shuffle = 1;
		m_conf.ps.dfmt = 0;

		bool write_ba;
		bool read_ba;

		ConvertSpriteTextureShuffle(write_ba, read_ba);

		// If date is enabled you need to test the green channel instead of the
		// alpha channel. Only enable this code in DATE mode to reduce the number
		// of shader.
		m_conf.ps.write_rg = !write_ba && features.texture_barrier && m_context->TEST.DATE;

		m_conf.ps.read_ba = read_ba;

		// Please bang my head against the wall!
		// 1/ Reduce the frame mask to a 16 bit format
		const u32 m = m_context->FRAME.FBMSK & GSLocalMemory::m_psm[m_context->FRAME.PSM].fmsk;
		const u32 fbmask = ((m >> 3) & 0x1F) | ((m >> 6) & 0x3E0) | ((m >> 9) & 0x7C00) | ((m >> 16) & 0x8000);
		// FIXME GSVector will be nice here
		const u8 rg_mask = fbmask & 0xFF;
		const u8 ba_mask = (fbmask >> 8) & 0xFF;
		m_conf.colormask.wrgba = 0;

		// 2 Select the new mask (Please someone put SSE here)
		if (rg_mask != 0xFF)
		{
			if (write_ba)
			{
				GL_INS("Color shuffle %s => B", read_ba ? "B" : "R");
				m_conf.colormask.wb = 1;
			}
			else
			{
				GL_INS("Color shuffle %s => R", read_ba ? "B" : "R");
				m_conf.colormask.wr = 1;
			}
			if (rg_mask)
				m_conf.ps.fbmask = 1;
		}

		if (ba_mask != 0xFF)
		{
			if (write_ba)
			{
				GL_INS("Color shuffle %s => A", read_ba ? "A" : "G");
				m_conf.colormask.wa = 1;
			}
			else
			{
				GL_INS("Color shuffle %s => G", read_ba ? "A" : "G");
				m_conf.colormask.wg = 1;
			}
			if (ba_mask)
				m_conf.ps.fbmask = 1;
		}

		if (m_conf.ps.fbmask && enable_fbmask_emulation)
		{
			m_conf.cb_ps.FbMask.r = rg_mask;
			m_conf.cb_ps.FbMask.g = rg_mask;
			m_conf.cb_ps.FbMask.b = ba_mask;
			m_conf.cb_ps.FbMask.a = ba_mask;

			// No blending so hit unsafe path.
			if (!PRIM->ABE || !features.texture_barrier)
			{
				GL_INS("FBMASK Unsafe SW emulated fb_mask:%x on tex shuffle", fbmask);
				m_conf.require_one_barrier = true;
			}
			else
			{
				GL_INS("FBMASK SW emulated fb_mask:%x on tex shuffle", fbmask);
				m_conf.require_full_barrier = true;
			}
		}
		else
		{
			m_conf.ps.fbmask = 0;
		}
	}
	else
	{
		m_conf.ps.dfmt = GSLocalMemory::m_psm[m_context->FRAME.PSM].fmt;

		// Don't allow only unused bits on 16bit format to enable fbmask,
		// let's set the mask to 0 in such cases.
		int fbmask = static_cast<int>(m_context->FRAME.FBMSK);
		const int fbmask_r = GSLocalMemory::m_psm[m_context->FRAME.PSM].fmsk;
		fbmask &= fbmask_r;
		const GSVector4i fbmask_v = GSVector4i::load(fbmask);
		const GSVector4i fbmask_vr = GSVector4i::load(fbmask_r);
		const int ff_fbmask = fbmask_v.eq8(fbmask_vr).mask();
		const int zero_fbmask = fbmask_v.eq8(GSVector4i::zero()).mask();

		m_conf.colormask.wrgba = ~ff_fbmask; // Enable channel if at least 1 bit is 0

		m_conf.ps.fbmask = enable_fbmask_emulation && (~ff_fbmask & ~zero_fbmask & 0xF);

		if (m_conf.ps.fbmask)
		{
			m_conf.cb_ps.FbMask = fbmask_v.u8to32();
			// Only alpha is special here, I think we can take a very unsafe shortcut
			// Alpha isn't blended on the GS but directly copyied into the RT.
			//
			// Behavior is clearly undefined however there is a high probability that
			// it will work. Masked bit will be constant and normally the same everywhere
			// RT/FS output/Cached value.
			//
			// Just to be sure let's add a new safe hack for unsafe access :)
			//
			// Here the GL spec quote to emphasize the unexpected behavior.
			/*
			   - If a texel has been written, then in order to safely read the result
			   a texel fetch must be in a subsequent Draw separated by the command

			   void TextureBarrier(void);

			   TextureBarrier() will guarantee that writes have completed and caches
			   have been invalidated before subsequent Draws are executed.
			 */
			// No blending so hit unsafe path.
			if (!PRIM->ABE || !(~ff_fbmask & ~zero_fbmask & 0x7) || !g_gs_device->Features().texture_barrier)
			{
				GL_INS("FBMASK Unsafe SW emulated fb_mask:%x on %d bits format", m_context->FRAME.FBMSK,
					(m_conf.ps.dfmt == 2) ? 16 : 32);
				m_conf.require_one_barrier = true;
			}
			else
			{
				// The safe and accurate path (but slow)
				GL_INS("FBMASK SW emulated fb_mask:%x on %d bits format", m_context->FRAME.FBMSK,
					(m_conf.ps.dfmt == 2) ? 16 : 32);
				m_conf.require_full_barrier = true;
			}
		}
	}
}

void GSRendererHW::EmulateChannelShuffle(const GSTextureCache::Source* tex)
{
	// Uncomment to disable HLE emulation (allow to trace the draw call)
	// m_channel_shuffle = false;

	// First let's check we really have a channel shuffle effect
	if (m_channel_shuffle)
	{
		if (m_game.title == CRC::PolyphonyDigitalGames)
		{
			GL_INS("Gran Turismo RGB Channel");
			m_conf.ps.channel = ChannelFetch_RGB;
			m_context->TEX0.TFX = TFX_DECAL;
			m_conf.rt = *tex->m_from_target;
		}
		else if (m_game.title == CRC::Tekken5)
		{
			if (m_context->FRAME.FBW == 1)
			{
				// Used in stages: Secret Garden, Acid Rain, Moonlit Wilderness
				GL_INS("Tekken5 RGB Channel");
				m_conf.ps.channel = ChannelFetch_RGB;
				m_context->FRAME.FBMSK = 0xFF000000;
				// 12 pages: 2 calls by channel, 3 channels, 1 blit
				// Minus current draw call
				m_skip = 12 * (3 + 3 + 1) - 1;
				m_conf.rt = *tex->m_from_target;
			}
			else
			{
				// Could skip model drawing if wrongly detected
				m_channel_shuffle = false;
			}
		}
		else if ((tex->m_texture->GetType() == GSTexture::Type::DepthStencil) && !(tex->m_32_bits_fmt))
		{
			// So far 2 games hit this code path. Urban Chaos and Tales of Abyss
			// UC: will copy depth to green channel
			// ToA: will copy depth to alpha channel
			if ((m_context->FRAME.FBMSK & 0xFF0000) == 0xFF0000)
			{
				// Green channel is masked
				GL_INS("Tales Of Abyss Crazyness (MSB 16b depth to Alpha)");
				m_conf.ps.tales_of_abyss_hle = 1;
			}
			else
			{
				GL_INS("Urban Chaos Crazyness (Green extraction)");
				m_conf.ps.urban_chaos_hle = 1;
			}
		}
		else if (m_index.tail <= 64 && m_context->CLAMP.WMT == 3)
		{
			// Blood will tell. I think it is channel effect too but again
			// implemented in a different way. I don't want to add more CRC stuff. So
			// let's disable channel when the signature is different
			//
			// Note: Tales Of Abyss and Tekken5 could hit this path too. Those games are
			// handled above.
			GL_INS("Maybe not a channel!");
			m_channel_shuffle = false;
		}
		else if (m_context->CLAMP.WMS == 3 && ((m_context->CLAMP.MAXU & 0x8) == 8))
		{
			// Read either blue or Alpha. Let's go for Blue ;)
			// MGS3/Kill Zone
			GL_INS("Blue channel");
			m_conf.ps.channel = ChannelFetch_BLUE;
		}
		else if (m_context->CLAMP.WMS == 3 && ((m_context->CLAMP.MINU & 0x8) == 0))
		{
			// Read either Red or Green. Let's check the V coordinate. 0-1 is likely top so
			// red. 2-3 is likely bottom so green (actually depends on texture base pointer offset)
			const bool green = PRIM->FST && (m_vertex.buff[0].V & 32);
			if (green && (m_context->FRAME.FBMSK & 0x00FFFFFF) == 0x00FFFFFF)
			{
				// Typically used in Terminator 3
				const int blue_mask = m_context->FRAME.FBMSK >> 24;
				int blue_shift = -1;

				// Note: potentially we could also check the value of the clut
				switch (blue_mask)
				{
					case 0xFF: ASSERT(0);      break;
					case 0xFE: blue_shift = 1; break;
					case 0xFC: blue_shift = 2; break;
					case 0xF8: blue_shift = 3; break;
					case 0xF0: blue_shift = 4; break;
					case 0xE0: blue_shift = 5; break;
					case 0xC0: blue_shift = 6; break;
					case 0x80: blue_shift = 7; break;
					default:                   break;
				}

				if (blue_shift >= 0)
				{
					const int green_mask = ~blue_mask & 0xFF;
					const int green_shift = 8 - blue_shift;

					GL_INS("Green/Blue channel (%d, %d)", blue_shift, green_shift);
					m_conf.cb_ps.ChannelShuffle = GSVector4i(blue_mask, blue_shift, green_mask, green_shift);
					m_conf.ps.channel = ChannelFetch_GXBY;
					m_context->FRAME.FBMSK = 0x00FFFFFF;
				}
				else
				{
					GL_INS("Green channel (wrong mask) (fbmask %x)", blue_mask);
					m_conf.ps.channel = ChannelFetch_GREEN;
				}
			}
			else if (green)
			{
				GL_INS("Green channel");
				m_conf.ps.channel = ChannelFetch_GREEN;
			}
			else
			{
				// Pop
				GL_INS("Red channel");
				m_conf.ps.channel = ChannelFetch_RED;
			}
		}
		else
		{
			GL_INS("Channel not supported");
			m_channel_shuffle = false;
		}
	}

	// Effect is really a channel shuffle effect so let's cheat a little
	if (m_channel_shuffle)
	{
		m_conf.tex = *tex->m_from_target;
		if (m_conf.tex)
		{
			if (m_conf.tex == m_conf.rt)
			{
				// sample from fb instead
				m_conf.tex = nullptr;
				m_conf.ps.tex_is_fb = true;
				m_conf.require_one_barrier = true;
			}
			else if (m_conf.tex == m_conf.ds)
			{
				// if depth testing is disabled, we don't need to copy, and can just unbind the depth buffer
				// no need for a barrier for GL either, since it's not bound to depth and texture concurrently
				// otherwise, the backend should recognise the hazard, and copy the buffer (D3D/Vulkan).
				if (m_conf.depth.ztst == ZTST_ALWAYS)
					m_conf.ds = nullptr;
			}
		}

		// Replace current draw with a fullscreen sprite
		//
		// Performance GPU note: it could be wise to reduce the size to
		// the rendered size of the framebuffer

		GSVertex* s = &m_vertex.buff[0];
		s[0].XYZ.X = (u16)(m_context->XYOFFSET.OFX + 0);
		s[1].XYZ.X = (u16)(m_context->XYOFFSET.OFX + 16384);
		s[0].XYZ.Y = (u16)(m_context->XYOFFSET.OFY + 0);
		s[1].XYZ.Y = (u16)(m_context->XYOFFSET.OFY + 16384);

		m_vertex.head = m_vertex.tail = m_vertex.next = 2;
		m_index.tail = 2;
	}
}

void GSRendererHW::EmulateBlending(bool& DATE_PRIMID, bool& DATE_BARRIER, bool& blending_alpha_pass)
{
	// AA1: Don't enable blending on AA1, not yet implemented on hardware mode,
	// it requires coverage sample so it's safer to turn it off instead.
	const bool AA1 = PRIM->AA1 && (m_vt.m_primclass == GS_LINE_CLASS);
	// PABE: Check condition early as an optimization.
	const bool PABE = PRIM->ABE && m_env.PABE.PABE && (GetAlphaMinMax().max < 128);
	// FBMASK: Color is not written, no need to do blending.
	const u32 temp_fbmask = m_conf.ps.dfmt == 2 ? 0x00F8F8F8 : 0x00FFFFFF;
	const bool FBMASK = (m_context->FRAME.FBMSK & temp_fbmask) == temp_fbmask;

	// No blending or coverage anti-aliasing so early exit
	if (FBMASK || PABE || !(PRIM->ABE || AA1))
	{
		m_conf.blend = {};
		m_conf.ps.no_color1 = true;
		return;
	}

	// Compute the blending equation to detect special case
	const GSDevice::FeatureSupport features(g_gs_device->Features());
	const GIFRegALPHA& ALPHA = m_context->ALPHA;

	// Set blending to shader bits
	m_conf.ps.blend_a = ALPHA.A;
	m_conf.ps.blend_b = ALPHA.B;
	m_conf.ps.blend_c = ALPHA.C;
	m_conf.ps.blend_d = ALPHA.D;

	// Get alpha value
	const bool alpha_c0_zero = (m_conf.ps.blend_c == 0 && GetAlphaMinMax().max == 0);
	const bool alpha_c0_one = (m_conf.ps.blend_c == 0 && (GetAlphaMinMax().min == 128) && (GetAlphaMinMax().max == 128));
	const bool alpha_c0_high_max_one = (m_conf.ps.blend_c == 0 && GetAlphaMinMax().max > 128);
	const bool alpha_c2_zero = (m_conf.ps.blend_c == 2 && ALPHA.FIX == 0u);
	const bool alpha_c2_one = (m_conf.ps.blend_c == 2 && ALPHA.FIX == 128u);
	const bool alpha_c2_high_one = (m_conf.ps.blend_c == 2 && ALPHA.FIX > 128u);

	// Optimize blending equations, must be done before index calculation
	if ((m_conf.ps.blend_a == m_conf.ps.blend_b) || ((m_conf.ps.blend_b == m_conf.ps.blend_d) && (alpha_c0_one || alpha_c2_one)))
	{
		// Condition 1:
		// A == B
		// (A - B) * C, result will be 0.0f so set A B to Cs, C to As
		// Condition 2:
		// B == D
		// Swap D with A
		// A == B
		// (A - B) * C, result will be 0.0f so set A B to Cs, C to As
		if (m_conf.ps.blend_a != m_conf.ps.blend_b)
			m_conf.ps.blend_d = m_conf.ps.blend_a;
		m_conf.ps.blend_a = 0;
		m_conf.ps.blend_b = 0;
		m_conf.ps.blend_c = 0;
	}
	else if (alpha_c0_zero || alpha_c2_zero)
	{
		// C == 0.0f
		// (A - B) * C, result will be 0.0f so set A B to Cs
		m_conf.ps.blend_a = 0;
		m_conf.ps.blend_b = 0;
	}

	// Ad cases, alpha write is masked, one barrier is enough, for d3d11 read the fb
	// Replace Ad with As, blend flags will be used from As since we are chaging the blend_index value.
	// Must be done before index calculation, after blending equation optimizations
	bool blend_ad_alpha_masked = (m_conf.ps.blend_c == 1) && (m_context->FRAME.FBMSK & 0xFF000000) == 0xFF000000;
	if (((GSConfig.AccurateBlendingUnit >= AccBlendLevel::Basic) || (m_env.COLCLAMP.CLAMP == 0))
		&& g_gs_device->Features().texture_barrier && blend_ad_alpha_masked)
		m_conf.ps.blend_c = 0;
	else if (((GSConfig.AccurateBlendingUnit >= AccBlendLevel::Medium)
		// Detect barrier aka fbmask on d3d11.
		|| m_conf.require_one_barrier)
		&& blend_ad_alpha_masked)
		m_conf.ps.blend_c = 0;
	else
		blend_ad_alpha_masked = false;

	u8 blend_index = u8(((m_conf.ps.blend_a * 3 + m_conf.ps.blend_b) * 3 + m_conf.ps.blend_c) * 3 + m_conf.ps.blend_d);
	const int blend_flag = GSDevice::GetBlendFlags(blend_index);

	// Re set alpha, it was modified, must be done after index calculation
	if (blend_ad_alpha_masked)
		m_conf.ps.blend_c = ALPHA.C;

	// HW blend can handle Cd output.
	bool color_dest_blend = !!(blend_flag & BLEND_CD);

	// Do the multiplication in shader for blending accumulation: Cs*As + Cd or Cs*Af + Cd
	bool accumulation_blend = !!(blend_flag & BLEND_ACCU);

	// Blending doesn't require barrier, or sampling of the rt
	const bool blend_non_recursive = !!(blend_flag & BLEND_NO_REC);

	// BLEND MIX selection, use a mix of hw/sw blending
	const bool blend_mix1 = !!(blend_flag & BLEND_MIX1);
	const bool blend_mix2 = !!(blend_flag & BLEND_MIX2);
	const bool blend_mix3 = !!(blend_flag & BLEND_MIX3);
	bool blend_mix = (blend_mix1 || blend_mix2 || blend_mix3);

	// Blend can be done on hw. As and F cases should be accurate.
	// BLEND_C_CLR1 with Ad, BLEND_C_CLR3  Cs > 0.5f will require sw blend.
	// BLEND_C_CLR1 with As/F, BLEND_C_CLR2_AF, BLEND_C_CLR2_AS can be done in hw.
	const bool clr_blend = !!(blend_flag & (BLEND_C_CLR1 | BLEND_C_CLR2_AF | BLEND_C_CLR2_AS | BLEND_C_CLR3));
	bool clr_blend1_2 = (blend_flag & (BLEND_C_CLR1 | BLEND_C_CLR2_AF | BLEND_C_CLR2_AS))
		&& (m_conf.ps.blend_c != 1)                                      // Make sure it isn't an Ad case
		&& !m_env.PABE.PABE                                              // No PABE as it will require sw blending.
		&& (m_env.COLCLAMP.CLAMP)                                        // Let's add a colclamp check too, hw blend will clamp to 0-1.
		&& !(m_conf.require_one_barrier || m_conf.require_full_barrier); // Also don't run if there are barriers present.

	// Warning no break on purpose
	// Note: the [[fallthrough]] attribute tell compilers not to complain about not having breaks.
	bool sw_blending = false;
	if (features.texture_barrier)
	{
		// Condition 1: Require full sw blend for full barrier.
		// Condition 2: One barrier is already enabled, prims don't overlap so let's use sw blend instead.
		const bool prefer_sw_blend = m_conf.require_full_barrier || (m_conf.require_one_barrier && m_prim_overlap == PRIM_OVERLAP_NO);

		// SW Blend is (nearly) free. Let's use it.
		const bool one_barrier = m_conf.require_one_barrier || blend_ad_alpha_masked;
		const bool no_prim_overlap = features.framebuffer_fetch ? (m_vt.m_primclass == GS_SPRITE_CLASS) : (m_prim_overlap == PRIM_OVERLAP_NO);
		const bool impossible_or_free_blend = (blend_flag & BLEND_A_MAX) // Impossible blending
			|| blend_non_recursive                 // Free sw blending, doesn't require barriers or reading fb
			|| accumulation_blend                  // Mix of hw/sw blending
			|| no_prim_overlap                     // Blend can be done in a single draw
			|| (m_conf.require_full_barrier)       // Another effect (for example fbmask) already requires a full barrier
			|| (one_barrier && features.framebuffer_fetch); // On fbfetch, one barrier is like full barrier

		switch (GSConfig.AccurateBlendingUnit)
		{
			case AccBlendLevel::Maximum:
				clr_blend1_2 = false;
				sw_blending |= true;
				[[fallthrough]];
			case AccBlendLevel::Full:
				sw_blending |= m_conf.ps.blend_a != m_conf.ps.blend_b && alpha_c0_high_max_one;
				[[fallthrough]];
			case AccBlendLevel::High:
				sw_blending |= m_conf.ps.blend_c == 1 || (m_conf.ps.blend_a != m_conf.ps.blend_b && alpha_c2_high_one);
				[[fallthrough]];
			case AccBlendLevel::Medium:
				// Initial idea was to enable accurate blending for sprite rendering to handle
				// correctly post-processing effect. Some games (ZoE) use tons of sprites as particles.
				// In order to keep it fast, let's limit it to smaller draw call.
				sw_blending |= m_vt.m_primclass == GS_SPRITE_CLASS && m_drawlist.size() < 100;
				[[fallthrough]];
			case AccBlendLevel::Basic:
				// SW FBMASK, needs sw blend, avoid hitting any hw blend pre enabled (accumulation, blend mix, blend cd),
				// fixes shadows in Superman shadows of Apokolips.
				// DATE_BARRIER already does full barrier so also makes more sense to do full sw blend.
				color_dest_blend &= !prefer_sw_blend;
				// If prims don't overlap prefer full sw blend on blend_ad_alpha_masked cases.
				accumulation_blend &= !(prefer_sw_blend || (blend_ad_alpha_masked && m_prim_overlap == PRIM_OVERLAP_NO));
				sw_blending |= impossible_or_free_blend;
				// Try to do hw blend for clr2 case.
				sw_blending &= !clr_blend1_2;
				// Do not run BLEND MIX if sw blending is already present, it's less accurate
				blend_mix &= !sw_blending;
				sw_blending |= blend_mix;
				// Disable dithering on blend mix.
				m_conf.ps.dither &= !blend_mix;
				[[fallthrough]];
			case AccBlendLevel::Minimum:
				break;
		}
	}
	else
	{
		// FBMASK or channel shuffle already reads the fb so it is safe to enable sw blend when there is no overlap.
		const bool fbmask_no_overlap = m_conf.require_one_barrier && (m_prim_overlap == PRIM_OVERLAP_NO);

		switch (GSConfig.AccurateBlendingUnit)
		{
			case AccBlendLevel::Maximum:
				if (m_prim_overlap == PRIM_OVERLAP_NO)
				{
					clr_blend1_2 = false;
					sw_blending |= true;
				}
				[[fallthrough]];
			case AccBlendLevel::Full:
				sw_blending |= ((m_conf.ps.blend_c == 1 || (blend_mix && (alpha_c2_high_one || alpha_c0_high_max_one))) && (m_prim_overlap == PRIM_OVERLAP_NO));
				[[fallthrough]];
			case AccBlendLevel::High:
				sw_blending |= (!(clr_blend || blend_mix) && (m_prim_overlap == PRIM_OVERLAP_NO));
				[[fallthrough]];
			case AccBlendLevel::Medium:
				// If prims don't overlap prefer full sw blend on blend_ad_alpha_masked cases.
				if (blend_ad_alpha_masked && m_prim_overlap == PRIM_OVERLAP_NO)
				{
					accumulation_blend = false;
					sw_blending |= true;
				}
				[[fallthrough]];
			case AccBlendLevel::Basic:
				// Disable accumulation blend when there is fbmask with no overlap, will be faster.
				color_dest_blend   &= !fbmask_no_overlap;
				accumulation_blend &= !fbmask_no_overlap;
				sw_blending |= accumulation_blend || blend_non_recursive || fbmask_no_overlap;
				// Try to do hw blend for clr2 case.
				sw_blending &= !clr_blend1_2;
				// Do not run BLEND MIX if sw blending is already present, it's less accurate
				blend_mix &= !sw_blending;
				sw_blending |= blend_mix;
				// Disable dithering on blend mix.
				m_conf.ps.dither &= !blend_mix;
				[[fallthrough]];
			case AccBlendLevel::Minimum:
				break;
		}
	}

	bool replace_dual_src = false;
	if (!features.dual_source_blend && GSDevice::IsDualSourceBlend(blend_index))
	{
		// if we don't have an alpha channel, we don't need a second pass, just output the alpha blend
		// in the single colour's alpha chnanel, and blend with it
		if (!m_conf.colormask.wa)
		{
			GL_INS("Outputting alpha blend in col0 because of no alpha write");
			m_conf.ps.no_ablend = true;
			replace_dual_src = true;
		}
		else if (features.framebuffer_fetch || m_conf.require_one_barrier || m_conf.require_full_barrier)
		{
			// prefer single pass sw blend (if barrier) or framebuffer fetch over dual pass alpha when supported
			sw_blending = true;
			color_dest_blend = false;
			accumulation_blend &= !features.framebuffer_fetch;
			blend_mix = false;
		}
		else
		{
			// split the draw into two
			blending_alpha_pass = true;
			replace_dual_src = true;
		}
	}
	else if (features.framebuffer_fetch)
	{
		// If we have fbfetch, use software blending when we need the fb value for anything else.
		// This saves outputting the second color when it's not needed.
		if (m_conf.require_one_barrier || m_conf.require_full_barrier)
		{
			sw_blending = true;
			color_dest_blend = false;
			accumulation_blend = false;
			blend_mix = false;
		}
	}

	// Color clip
	if (m_env.COLCLAMP.CLAMP == 0)
	{
		bool free_colclip = false;
		if (features.framebuffer_fetch)
			free_colclip = true;
		else if (features.texture_barrier)
			free_colclip = m_prim_overlap == PRIM_OVERLAP_NO || blend_non_recursive;
		else
			free_colclip = blend_non_recursive;

		GL_DBG("COLCLIP Info (Blending: %u/%u/%u/%u, OVERLAP: %d)", m_conf.ps.blend_a, m_conf.ps.blend_b, m_conf.ps.blend_c, m_conf.ps.blend_d, m_prim_overlap);
		if (color_dest_blend)
		{
			// No overflow, disable colclip.
			GL_INS("COLCLIP mode DISABLED");
		}
		else if (free_colclip)
		{
			// The fastest algo that requires a single pass
			GL_INS("COLCLIP Free mode ENABLED");
			m_conf.ps.colclip  = 1;
			sw_blending        = true;
			// Disable the HDR algo
			accumulation_blend = false;
			blend_mix          = false;
		}
		else if (accumulation_blend || blend_mix)
		{
			// A fast algo that requires 2 passes
			GL_INS("COLCLIP Fast HDR mode ENABLED");
			m_conf.ps.hdr = 1;
			sw_blending   = true; // Enable sw blending for the HDR algo
		}
		else if (sw_blending)
		{
			// A slow algo that could requires several passes (barely used)
			GL_INS("COLCLIP SW mode ENABLED");
			m_conf.ps.colclip = 1;
		}
		else
		{
			GL_INS("COLCLIP HDR mode ENABLED");
			m_conf.ps.hdr = 1;
		}
	}

	// Per pixel alpha blending
	if (m_env.PABE.PABE)
	{
		// Breath of Fire Dragon Quarter, Strawberry Shortcake, Super Robot Wars, Cartoon Network Racing.

		if (sw_blending)
		{
			GL_INS("PABE mode ENABLED");
			if (features.texture_barrier)
			{
				// Disable hw/sw blend and do pure sw blend with reading the framebuffer.
				color_dest_blend   = false;
				accumulation_blend = false;
				blend_mix          = false;
				m_conf.ps.pabe     = 1;

				// HDR mode should be disabled when doing sw blend, swap with sw colclip.
				if (m_conf.ps.hdr)
				{
					m_conf.ps.hdr     = 0;
					m_conf.ps.colclip = 1;
				}
			}
			else
			{
				m_conf.ps.pabe = !(accumulation_blend || blend_mix);
			}
		}
		else if (m_conf.ps.blend_a == 0 && m_conf.ps.blend_b == 1 && m_conf.ps.blend_c == 0 && m_conf.ps.blend_d == 1)
		{
			// this works because with PABE alpha blending is on when alpha >= 0x80, but since the pixel shader
			// cannot output anything over 0x80 (== 1.0) blending with 0x80 or turning it off gives the same result
			blend_index = 0;
		}
	}

	// For stat to optimize accurate option
#if 0
	GL_INS("BLEND_INFO: %u/%u/%u/%u. Clamp:%u. Prim:%d number %u (drawlist %u) (sw %d)",
		m_conf.ps.blend_a, m_conf.ps.blend_b, m_conf.ps.blend_c, m_conf.ps.blend_d,
		m_env.COLCLAMP.CLAMP, m_vt.m_primclass, m_vertex.next, m_drawlist.size(), sw_blending);
#endif
	if (color_dest_blend)
	{
		// Blend output will be Cd, disable hw/sw blending.
		m_conf.blend = {};
		m_conf.ps.no_color1 = true;
		m_conf.ps.blend_a = m_conf.ps.blend_b = m_conf.ps.blend_c = m_conf.ps.blend_d = 0;
		sw_blending = false; // DATE_PRIMID

		// Output is Cd, set rgb write to 0.
		m_conf.colormask.wrgba &= 0x8;
	}
	else if (sw_blending)
	{
		// Require the fix alpha vlaue
		if (m_conf.ps.blend_c == 2)
			m_conf.cb_ps.TA_MaxDepth_Af.a = static_cast<float>(ALPHA.FIX) / 128.0f;

		const HWBlend blend = GSDevice::GetBlend(blend_index, replace_dual_src);
		if (accumulation_blend)
		{
			// Keep HW blending to do the addition/subtraction
			m_conf.blend = {true, GSDevice::CONST_ONE, GSDevice::CONST_ONE, blend.op, false, 0};
			if (m_conf.ps.blend_a == 2)
			{
				// The blend unit does a reverse subtraction so it means
				// the shader must output a positive value.
				// Replace 0 - Cs by Cs - 0
				m_conf.ps.blend_a = m_conf.ps.blend_b;
				m_conf.ps.blend_b = 2;
			}
			// Remove the addition/substraction from the SW blending
			m_conf.ps.blend_d = 2;

			// Dual source output not needed (accumulation blend replaces it with ONE).
			m_conf.ps.no_color1 = true;

			// Only Ad case will require one barrier
			m_conf.require_one_barrier |= blend_ad_alpha_masked;
		}
		else if (blend_mix)
		{
			// For mixed blend, the source blend is done in the shader (so we use CONST_ONE as a factor).
			m_conf.blend = {true, GSDevice::CONST_ONE, blend.dst, blend.op, m_conf.ps.blend_c == 2, ALPHA.FIX};
			m_conf.ps.blend_mix = 1;

			// Elide DSB colour output if not used by dest.
			m_conf.ps.no_color1 |= !GSDevice::IsDualSourceBlendFactor(blend.dst);

			if (blend_mix1)
			{
				m_conf.ps.blend_a = 0;
				m_conf.ps.blend_b = 2;
				m_conf.ps.blend_d = 2;
			}
			else if (blend_mix2)
			{
				m_conf.ps.blend_a = 0;
				m_conf.ps.blend_b = 2;
				m_conf.ps.blend_d = 0;
			}
			else if (blend_mix3)
			{
				m_conf.ps.blend_a = 2;
				m_conf.ps.blend_b = 0;
				m_conf.ps.blend_d = 0;
			}

			// Only Ad case will require one barrier
			if (blend_ad_alpha_masked)
			{
				m_conf.require_one_barrier |= true;
				// Swap Ad with As for hw blend
				m_conf.ps.clr_hw = 6;
			}
		}
		else
		{
			// Disable HW blending
			m_conf.blend = {};
			m_conf.ps.no_color1 = true;
			replace_dual_src = false;
			blending_alpha_pass = false;

			const bool blend_non_recursive_one_barrier = blend_non_recursive && blend_ad_alpha_masked;
			if (blend_non_recursive_one_barrier)
				m_conf.require_one_barrier |= true;
			else if (features.texture_barrier)
				m_conf.require_full_barrier |= !blend_non_recursive;
			else
				m_conf.require_one_barrier |= !blend_non_recursive;
		}
	}
	else
	{
		// No sw blending
		m_conf.ps.blend_a = 0;
		m_conf.ps.blend_b = 0;
		m_conf.ps.blend_d = 0;

		// Care for clr_hw value, 6 is for hw/sw, sw blending used.
		if (blend_flag & BLEND_C_CLR1)
		{
			if (blend_ad_alpha_masked)
			{
				m_conf.ps.blend_c = 1;
				m_conf.ps.clr_hw = 5;
				m_conf.require_one_barrier |= true;
			}
			else
			{
				m_conf.ps.clr_hw = 1;
			}
		}
		else if (blend_flag & (BLEND_C_CLR2_AF | BLEND_C_CLR2_AS))
		{
			if (blend_ad_alpha_masked)
			{
				m_conf.ps.blend_c = 1;
				m_conf.ps.clr_hw = 4;
				m_conf.require_one_barrier |= true;
			}
			else if (m_conf.ps.blend_c == 2)
			{
				m_conf.ps.blend_c = 2;
				m_conf.cb_ps.TA_MaxDepth_Af.a = static_cast<float>(ALPHA.FIX) / 128.0f;
				m_conf.ps.clr_hw = 2;
			}
			else // m_conf.ps.blend_c == 0
			{
				m_conf.ps.blend_c = 0;
				m_conf.ps.clr_hw = 2;
			}
		}
		else if (blend_flag & BLEND_C_CLR3)
		{
			m_conf.ps.clr_hw = 3;
		}
		else if (blend_ad_alpha_masked)
		{
			m_conf.ps.blend_c = 1;
			m_conf.ps.clr_hw = 6;
			m_conf.require_one_barrier |= true;
		}

		if (m_conf.ps.dfmt == 1 && m_conf.ps.blend_c == 1)
		{
			// 24 bits doesn't have an alpha channel so use 1.0f fix factor as equivalent
			const HWBlend blend(GSDevice::GetBlend(blend_index + 3, replace_dual_src)); // +3 <=> +1 on C
			m_conf.blend = {true, blend.src, blend.dst, blend.op, true, 128};
		}
		else
		{
			const HWBlend blend(GSDevice::GetBlend(blend_index, replace_dual_src));
			m_conf.blend = {true, blend.src, blend.dst, blend.op, m_conf.ps.blend_c == 2, ALPHA.FIX};
		}

		// Remove second color output when unused. Works around bugs in some drivers (e.g. Intel).
		m_conf.ps.no_color1 |= !GSDevice::IsDualSourceBlendFactor(m_conf.blend.src_factor) &&
		                       !GSDevice::IsDualSourceBlendFactor(m_conf.blend.dst_factor);
	}

	// DATE_PRIMID interact very badly with sw blending. DATE_PRIMID uses the primitiveID to find the primitive
	// that write the bad alpha value. Sw blending will force the draw to run primitive by primitive
	// (therefore primitiveID will be constant to 1).
	// Switch DATE_PRIMID with DATE_BARRIER in such cases to ensure accuracy.
	// No mix of COLCLIP + sw blend + DATE_PRIMID, neither sw fbmask + DATE_PRIMID.
	// Note: Do the swap in the end, saves the expensive draw splitting/barriers when mixed software blending is used.
	if (sw_blending && DATE_PRIMID && m_conf.require_full_barrier)
	{
		GL_PERF("DATE: Swap DATE_PRIMID with DATE_BARRIER");
		m_conf.require_full_barrier = true;
		DATE_PRIMID = false;
		DATE_BARRIER = true;
	}
}

void GSRendererHW::EmulateTextureSampler(const GSTextureCache::Source* tex)
{
	// Warning fetch the texture PSM format rather than the context format. The latter could have been corrected in the texture cache for depth.
	//const GSLocalMemory::psm_t &psm = GSLocalMemory::m_psm[m_context->TEX0.PSM];
	const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[tex->m_TEX0.PSM];
	const GSLocalMemory::psm_t& cpsm = psm.pal > 0 ? GSLocalMemory::m_psm[m_context->TEX0.CPSM] : psm;

	const u8 wms = m_context->CLAMP.WMS;
	const u8 wmt = m_context->CLAMP.WMT;
	const bool complex_wms_wmt = !!((wms | wmt) & 2);

	const bool need_mipmap = IsMipMapDraw();
	const bool shader_emulated_sampler = tex->m_palette || cpsm.fmt != 0 || complex_wms_wmt || psm.depth;
	const bool trilinear_manual = need_mipmap && GSConfig.HWMipmap == HWMipmapLevel::Full;

	bool bilinear = m_vt.IsLinear();
	int trilinear = 0;
	bool trilinear_auto = false;
	switch (GSConfig.UserHacks_TriFilter)
	{
		case TriFiltering::Forced:
			{
				// force bilinear otherwise we can end up with min/mag nearest and mip linear.
				bilinear = true;
				trilinear = static_cast<u8>(GS_MIN_FILTER::Linear_Mipmap_Linear);
				trilinear_auto = !need_mipmap || GSConfig.HWMipmap != HWMipmapLevel::Full;
			}
			break;

		case TriFiltering::PS2:
			if (need_mipmap && GSConfig.HWMipmap != HWMipmapLevel::Full)
			{
				trilinear = m_context->TEX1.MMIN;
				trilinear_auto = true;
			}
			break;

		case TriFiltering::Automatic:
		case TriFiltering::Off:
		default:
			break;
	}

	// 1 and 0 are equivalent
	m_conf.ps.wms = (wms & 2) ? wms : 0;
	m_conf.ps.wmt = (wmt & 2) ? wmt : 0;

	// Depth + bilinear filtering isn't done yet (And I'm not sure we need it anyway but a game will prove me wrong)
	// So of course, GTA set the linear mode, but sampling is done at texel center so it is equivalent to nearest sampling
	// Other games worth testing: Area 51, Burnout
	if (psm.depth && m_vt.IsLinear())
		GL_INS("WARNING: Depth + bilinear filtering not supported");

	// Performance note:
	// 1/ Don't set 0 as it is the default value
	// 2/ Only keep aem when it is useful (avoid useless shader permutation)
	if (m_conf.ps.shuffle)
	{
		// Force a 32 bits access (normally shuffle is done on 16 bits)
		// m_ps_sel.tex_fmt = 0; // removed as an optimization
		m_conf.ps.aem = m_env.TEXA.AEM;
		ASSERT(tex->m_target);

		// Require a float conversion if the texure is a depth otherwise uses Integral scaling
		if (psm.depth)
		{
			m_conf.ps.depth_fmt = (tex->m_texture->GetType() != GSTexture::Type::DepthStencil) ? 3 : 1;
		}

		// Shuffle is a 16 bits format, so aem is always required
		GSVector4 ta(m_env.TEXA & GSVector4i::x000000ff());
		ta /= 255.0f;
		m_conf.cb_ps.TA_MaxDepth_Af.x = ta.x;
		m_conf.cb_ps.TA_MaxDepth_Af.y = ta.y;

		// The purpose of texture shuffle is to move color channel. Extra interpolation is likely a bad idea.
		bilinear &= m_vt.IsLinear();

		const GSVector4 half_pixel = RealignTargetTextureCoordinate(tex);
		m_conf.cb_vs.texture_offset = GSVector2(half_pixel.x, half_pixel.y);
	}
	else if (tex->m_target)
	{
		// Use an old target. AEM and index aren't resolved it must be done
		// on the GPU

		// Select the 32/24/16 bits color (AEM)
		m_conf.ps.aem_fmt = cpsm.fmt;
		m_conf.ps.aem = m_env.TEXA.AEM;

		// Don't upload AEM if format is 32 bits
		if (cpsm.fmt)
		{
			GSVector4 ta(m_env.TEXA & GSVector4i::x000000ff());
			ta /= 255.0f;
			m_conf.cb_ps.TA_MaxDepth_Af.x = ta.x;
			m_conf.cb_ps.TA_MaxDepth_Af.y = ta.y;
		}

		// Select the index format
		if (tex->m_palette)
		{
			// FIXME Potentially improve fmt field in GSLocalMemory
			if (m_context->TEX0.PSM == PSM_PSMT4HL)
				m_conf.ps.pal_fmt = 1;
			else if (m_context->TEX0.PSM == PSM_PSMT4HH)
				m_conf.ps.pal_fmt = 2;
			else
				m_conf.ps.pal_fmt = 3;

			// Alpha channel of the RT is reinterpreted as an index. Star
			// Ocean 3 uses it to emulate a stencil buffer.  It is a very
			// bad idea to force bilinear filtering on it.
			bilinear &= m_vt.IsLinear();
		}

		// Depth format
		if (tex->m_texture->GetType() == GSTexture::Type::DepthStencil)
		{
			// Require a float conversion if the texure is a depth format
			m_conf.ps.depth_fmt = (psm.bpp == 16) ? 2 : 1;

			// Don't force interpolation on depth format
			bilinear &= m_vt.IsLinear();
		}
		else if (psm.depth)
		{
			// Use Integral scaling
			m_conf.ps.depth_fmt = 3;

			// Don't force interpolation on depth format
			bilinear &= m_vt.IsLinear();
		}

		const GSVector4 half_pixel = RealignTargetTextureCoordinate(tex);
		m_conf.cb_vs.texture_offset = GSVector2(half_pixel.x, half_pixel.y);
	}
	else if (tex->m_palette)
	{
		// Use a standard 8 bits texture. AEM is already done on the CLUT
		// Therefore you only need to set the index
		// m_conf.ps.aem     = 0; // removed as an optimization

		// Note 4 bits indexes are converted to 8 bits
		m_conf.ps.pal_fmt = 3;
	}
	else
	{
		// Standard texture. Both index and AEM expansion were already done by the CPU.
		// m_conf.ps.tex_fmt = 0; // removed as an optimization
		// m_conf.ps.aem     = 0; // removed as an optimization
	}

	if (m_context->TEX0.TFX == TFX_MODULATE && m_vt.m_eq.rgba == 0xFFFF && m_vt.m_min.c.eq(GSVector4i(128)))
	{
		// Micro optimization that reduces GPU load (removes 5 instructions on the FS program)
		m_conf.ps.tfx = TFX_DECAL;
	}
	else
	{
		m_conf.ps.tfx = m_context->TEX0.TFX;
	}

	m_conf.ps.tcc = m_context->TEX0.TCC;

	m_conf.ps.ltf = bilinear && shader_emulated_sampler;
	m_conf.ps.point_sampler = g_gs_device->Features().broken_point_sampler && (!bilinear || shader_emulated_sampler);

	const GSVector2 scale = tex->m_texture->GetScale();
	const int w = tex->m_texture->GetWidth();
	const int h = tex->m_texture->GetHeight();

	const int tw = (int)(1 << m_context->TEX0.TW);
	const int th = (int)(1 << m_context->TEX0.TH);
	const int miptw = 1 << tex->m_TEX0.TW;
	const int mipth = 1 << tex->m_TEX0.TH;

	const GSVector4 WH(static_cast<float>(tw), static_cast<float>(th), miptw * scale.x, mipth * scale.y);
	const GSVector4 st_scale = WH.zwzw() / GSVector4(w, h).xyxy();
	m_conf.cb_ps.STScale = GSVector2(st_scale.x, st_scale.y);

	m_conf.ps.fst = !!PRIM->FST;

	m_conf.cb_ps.WH = WH;
	m_conf.cb_ps.HalfTexel = GSVector4(-0.5f, 0.5f).xxyy() / WH.zwzw();
	if (complex_wms_wmt)
	{
		m_conf.cb_ps.MskFix = GSVector4i(m_context->CLAMP.MINU, m_context->CLAMP.MINV, m_context->CLAMP.MAXU, m_context->CLAMP.MAXV);;
		m_conf.cb_ps.MinMax = GSVector4(m_conf.cb_ps.MskFix) / WH.xyxy();
	}
	else if (trilinear_manual)
	{
		// Reuse uv_min_max for mipmap parameter to avoid an extension of the UBO
		m_conf.cb_ps.MinMax.x = (float)m_context->TEX1.K / 16.0f;
		m_conf.cb_ps.MinMax.y = float(1 << m_context->TEX1.L);
		m_conf.cb_ps.MinMax.z = float(m_lod.x); // Offset because first layer is m_lod, dunno if we can do better
		m_conf.cb_ps.MinMax.w = float(m_lod.y);
	}
	else if (trilinear_auto)
	{
		tex->m_texture->GenerateMipmapsIfNeeded();
	}

	// TC Offset Hack
	m_conf.ps.tcoffsethack = m_userhacks_tcoffset;
	const GSVector4 tc_oh_ts = GSVector4(1 / 16.0f, 1 / 16.0f, m_userhacks_tcoffset_x, m_userhacks_tcoffset_y) / WH.xyxy();
	m_conf.cb_ps.TCOffsetHack = GSVector2(tc_oh_ts.z, tc_oh_ts.w);
	m_conf.cb_vs.texture_scale = GSVector2(tc_oh_ts.x, tc_oh_ts.y);

	// Must be done after all coordinates math
	if (m_context->HasFixedTEX0() && !PRIM->FST)
	{
		m_conf.ps.invalid_tex0 = 1;
		// Use invalid size to denormalize ST coordinate
		m_conf.cb_ps.WH.x = (float)(1 << m_context->stack.TEX0.TW);
		m_conf.cb_ps.WH.y = (float)(1 << m_context->stack.TEX0.TH);

		// We can't handle m_target with invalid_tex0 atm due to upscaling
		ASSERT(!tex->m_target);
	}

	// Only enable clamping in CLAMP mode. REGION_CLAMP will be done manually in the shader
	m_conf.sampler.tau = (wms != CLAMP_CLAMP);
	m_conf.sampler.tav = (wmt != CLAMP_CLAMP);
	if (shader_emulated_sampler)
	{
		m_conf.sampler.biln = 0;
		m_conf.sampler.aniso = 0;
		m_conf.sampler.triln = 0;
	}
	else
	{
		m_conf.sampler.biln = bilinear;
		// Aniso filtering doesn't work with textureLod so use texture (automatic_lod) instead.
		// Enable aniso only for triangles. Sprites are flat so aniso is likely useless (it would save perf for others primitives).
		const bool anisotropic = m_vt.m_primclass == GS_TRIANGLE_CLASS && !trilinear_manual;
		m_conf.sampler.aniso = anisotropic;
		m_conf.sampler.triln = trilinear;
		if (trilinear_manual)
		{
			m_conf.ps.manual_lod = 1;
		}
		else if (trilinear_auto || anisotropic)
		{
			m_conf.ps.automatic_lod = 1;
		}
	}

	// clamp to base level if we're not providing or generating mipmaps
	// manual trilinear causes the chain to be uploaded, auto causes it to be generated
	m_conf.sampler.lodclamp = !(trilinear_manual || trilinear_auto);

	// don't overwrite the texture when using channel shuffle, but keep the palette
	if (!m_channel_shuffle)
		m_conf.tex = tex->m_texture;
	m_conf.pal = tex->m_palette;
}

void GSRendererHW::EmulateATST(float& AREF, GSHWDrawConfig::PSSelector& ps, bool pass_2)
{
	static const u32 inverted_atst[] = {ATST_ALWAYS, ATST_NEVER, ATST_GEQUAL, ATST_GREATER, ATST_NOTEQUAL, ATST_LESS, ATST_LEQUAL, ATST_EQUAL};

	if (!m_context->TEST.ATE)
		return;

	// Check for pass 2, otherwise do pass 1.
	const int atst = pass_2 ? inverted_atst[m_context->TEST.ATST] : m_context->TEST.ATST;


	switch (atst)
	{
		case ATST_LESS:
			AREF = static_cast<float>(m_context->TEST.AREF) - 0.1f;
			ps.atst = 1;
			break;
		case ATST_LEQUAL:
			AREF = static_cast<float>(m_context->TEST.AREF) - 0.1f + 1.0f;
			ps.atst = 1;
			break;
		case ATST_GEQUAL:
			AREF = static_cast<float>(m_context->TEST.AREF) - 0.1f;
			ps.atst = 2;
			break;
		case ATST_GREATER:
			AREF = static_cast<float>(m_context->TEST.AREF) - 0.1f + 1.0f;
			ps.atst = 2;
			break;
		case ATST_EQUAL:
			AREF = static_cast<float>(m_context->TEST.AREF);
			ps.atst = 3;
			break;
		case ATST_NOTEQUAL:
			AREF = static_cast<float>(m_context->TEST.AREF);
			ps.atst = 4;
			break;
		case ATST_NEVER: // Draw won't be done so no need to implement it in shader
		case ATST_ALWAYS:
		default:
			ps.atst = 0;
			break;
	}
}

void GSRendererHW::ResetStates()
{
	// We don't want to zero out the constant buffers, since fields used by the current draw could result in redundant uploads.
	// This memset should be pretty efficient - the struct is 16 byte aligned, as is the cb_vs offset.
	memset(&m_conf, 0, reinterpret_cast<const char*>(&m_conf.cb_vs) - reinterpret_cast<const char*>(&m_conf));
}

void GSRendererHW::DrawPrims(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* tex)
{
#ifdef ENABLE_OGL_DEBUG
	const GSVector4i area_out = GSVector4i(m_vt.m_min.p.xyxy(m_vt.m_max.p)).rintersect(GSVector4i(m_context->scissor.in));
	const GSVector4i area_in = GSVector4i(m_vt.m_min.t.xyxy(m_vt.m_max.t));

	GL_PUSH("GL Draw from %d (area %d,%d => %d,%d) in %d (Depth %d) (area %d,%d => %d,%d)",
		tex && tex->m_texture ? tex->m_texture->GetID() : -1,
		area_in.x, area_in.y, area_in.z, area_in.w,
		rt ? rt->GetID() : -1, ds ? ds->GetID() : -1,
		area_out.x, area_out.y, area_out.z, area_out.w);
#endif

	const GSVector2i& rtsize = ds ? ds->GetSize()  : rt->GetSize();
	const GSVector2& rtscale = ds ? ds->GetScale() : rt->GetScale();
	const GSDevice::FeatureSupport features(g_gs_device->Features());

	const bool DATE = m_context->TEST.DATE && m_context->FRAME.PSM != PSM_PSMCT24;
	bool DATE_PRIMID = false;
	bool DATE_BARRIER = false;
	bool DATE_one  = false;

	const bool ate_first_pass = m_context->TEST.DoFirstPass();
	const bool ate_second_pass = m_context->TEST.DoSecondPass();

	ResetStates();
	m_conf.cb_vs.texture_offset = GSVector2(0, 0);
	m_conf.ps.scanmsk = m_env.SCANMSK.MSK;
	m_conf.rt = rt;
	m_conf.ds = ds;

	ASSERT(g_gs_device != NULL);

	// Z setup has to come before channel shuffle
	EmulateZbuffer();

	// HLE implementation of the channel selection effect
	//
	// Warning it must be done at the begining because it will change the
	// vertex list (it will interact with PrimitiveOverlap and accurate
	// blending)
	EmulateChannelShuffle(tex);

	// Upscaling hack to avoid various line/grid issues
	MergeSprite(tex);

	if (!features.framebuffer_fetch)
		m_prim_overlap = PrimitiveOverlap();
	else
		m_prim_overlap = PRIM_OVERLAP_UNKNOW;

	// Detect framebuffer read that will need special handling
	if (features.texture_barrier && (m_context->FRAME.Block() == m_context->TEX0.TBP0) && PRIM->TME && GSConfig.AccurateBlendingUnit != AccBlendLevel::Minimum)
	{
		const u32 fb_mask = GSLocalMemory::m_psm[m_context->FRAME.PSM].fmsk;
		if (((m_context->FRAME.FBMSK & fb_mask) == (fb_mask & 0x00FFFFFF)) && (m_vt.m_primclass == GS_TRIANGLE_CLASS))
		{
			// This pattern is used by several games to emulate a stencil (shadow)
			// Ratchet & Clank, Jak do alpha integer multiplication (tfx) which is mostly equivalent to +1/-1
			// Tri-Ace (Star Ocean 3/RadiataStories/VP2) uses a palette to handle the +1/-1
			GL_DBG("Source and Target are the same! Let's sample the framebuffer");
			m_conf.ps.tex_is_fb = 1;
			m_conf.require_full_barrier = !features.framebuffer_fetch;
		}
		else if (m_prim_overlap != PRIM_OVERLAP_NO)
		{
			// Note: It is fine if the texture fits in a single GS page. First access will cache
			// the page in the GS texture buffer.
			GL_INS("ERROR: Source and Target are the same!");
		}
	}

	EmulateTextureShuffleAndFbmask();

	// DATE: selection of the algorithm. Must be done before blending because GL42 is not compatible with blending
	if (DATE)
	{
		// It is way too complex to emulate texture shuffle with DATE, so use accurate path.
		// No overlap should be triggered on gl/vk only as they support DATE_BARRIER.
		if (features.framebuffer_fetch)
		{
			// Full DATE is "free" with framebuffer fetch. The barrier gets cleared below.
			DATE_BARRIER = true;
			m_conf.require_full_barrier = true;
		}
		else if ((features.texture_barrier && m_prim_overlap == PRIM_OVERLAP_NO) || m_texture_shuffle)
		{
			GL_PERF("DATE: Accurate with %s", (features.texture_barrier && m_prim_overlap == PRIM_OVERLAP_NO) ? "no overlap" : "texture shuffle");
			if (features.texture_barrier)
			{
				m_conf.require_full_barrier = true;
				DATE_BARRIER = true;
			}
		}
		else if (m_context->FBA.FBA)
		{
			GL_PERF("DATE: Fast with FBA, all pixels will be >= 128");
			DATE_one = !m_context->TEST.DATM;
		}
		else if (m_conf.colormask.wa && !m_context->TEST.ATE)
		{
			// Performance note: check alpha range with GetAlphaMinMax()
			// Note: all my dump are already above 120fps, but it seems to reduce GPU load
			// with big upscaling
			if (m_context->TEST.DATM && GetAlphaMinMax().max < 128 && features.stencil_buffer)
			{
				// Only first pixel (write 0) will pass (alpha is 1)
				GL_PERF("DATE: Fast with alpha %d-%d", GetAlphaMinMax().min, GetAlphaMinMax().max);
				DATE_one = true;
			}
			else if (!m_context->TEST.DATM && GetAlphaMinMax().min >= 128 && features.stencil_buffer)
			{
				// Only first pixel (write 1) will pass (alpha is 0)
				GL_PERF("DATE: Fast with alpha %d-%d", GetAlphaMinMax().min, GetAlphaMinMax().max);
				DATE_one = true;
			}
			else if ((m_vt.m_primclass == GS_SPRITE_CLASS && m_drawlist.size() < 50) || (m_index.tail < 100))
			{
				// texture barrier will split the draw call into n draw call. It is very efficient for
				// few primitive draws. Otherwise it sucks.
				GL_PERF("DATE: Accurate with alpha %d-%d", GetAlphaMinMax().min, GetAlphaMinMax().max);
				if (features.texture_barrier)
				{
					m_conf.require_full_barrier = true;
					DATE_BARRIER = true;
				}
			}
			else if (GSConfig.AccurateDATE)
			{
				// Note: Fast level (DATE_one) was removed as it's less accurate.
				GL_PERF("DATE: Accurate with alpha %d-%d", GetAlphaMinMax().min, GetAlphaMinMax().max);
				if (features.image_load_store)
				{
					DATE_PRIMID = true;
				}
				else if (features.texture_barrier)
				{
					m_conf.require_full_barrier = true;
					DATE_BARRIER = true;
				}
				else if (features.stencil_buffer)
				{
					DATE_one = true;
				}
			}
		}
		else if (!m_conf.colormask.wa && !m_context->TEST.ATE)
		{
			GL_PERF("DATE: Accurate with no alpha write");
			if (g_gs_device->Features().texture_barrier)
			{
				m_conf.require_one_barrier = true;
				DATE_BARRIER = true;
			}
		}

		// Will save my life !
		ASSERT(!(DATE_BARRIER && DATE_one));
		ASSERT(!(DATE_PRIMID && DATE_one));
		ASSERT(!(DATE_PRIMID && DATE_BARRIER));
	}

	// Before emulateblending, dither will be used
	m_conf.ps.dither = GSConfig.Dithering > 0 && m_conf.ps.dfmt == 2 && m_env.DTHE.DTHE;

	if (m_conf.ps.dfmt == 1)
	{
		// Disable writing of the alpha channel
		m_conf.colormask.wa = 0;
	}

	// Blend

	bool blending_alpha_pass = false;
	if (!IsOpaque() && rt && m_conf.colormask.wrgba != 0)
	{
		EmulateBlending(DATE_PRIMID, DATE_BARRIER, blending_alpha_pass);
	}
	else
	{
		m_conf.blend = {}; // No blending please
		m_conf.ps.no_color1 = true;
	}

	// No point outputting colours if we're just writing depth.
	// We might still need the framebuffer for DATE, though.
	if (!rt || m_conf.colormask.wrgba == 0)
		m_conf.ps.DisableColorOutput();

	if (m_conf.ps.scanmsk & 2)
		DATE_PRIMID = false; // to have discard in the shader work correctly

	// DATE setup, no DATE_BARRIER please

	if (!DATE)
		m_conf.destination_alpha = GSHWDrawConfig::DestinationAlphaMode::Off;
	else if (DATE_one)
		m_conf.destination_alpha = GSHWDrawConfig::DestinationAlphaMode::StencilOne;
	else if (DATE_PRIMID)
		m_conf.destination_alpha = GSHWDrawConfig::DestinationAlphaMode::PrimIDTracking;
	else if (DATE_BARRIER)
		m_conf.destination_alpha = GSHWDrawConfig::DestinationAlphaMode::Full;
	else if (features.stencil_buffer)
		m_conf.destination_alpha = GSHWDrawConfig::DestinationAlphaMode::Stencil;

	m_conf.datm = m_context->TEST.DATM;

	// vs

	m_conf.vs.tme = PRIM->TME;
	m_conf.vs.fst = PRIM->FST;

	// FIXME D3D11 and GL support half pixel center. Code could be easier!!!
	const float sx = 2.0f * rtscale.x / (rtsize.x << 4);
	const float sy = 2.0f * rtscale.y / (rtsize.y << 4);
	const float ox = (float)(int)m_context->XYOFFSET.OFX;
	const float oy = (float)(int)m_context->XYOFFSET.OFY;
	float ox2 = -1.0f / rtsize.x;
	float oy2 = -1.0f / rtsize.y;

	//This hack subtracts around half a pixel from OFX and OFY.
	//
	//The resulting shifted output aligns better with common blending / corona / blurring effects,
	//but introduces a few bad pixels on the edges.

	if (rt && rt->OffsetHack_modxy > 1.0f)
	{
		ox2 *= rt->OffsetHack_modxy;
		oy2 *= rt->OffsetHack_modxy;
	}

	m_conf.cb_vs.vertex_scale = GSVector2(sx, sy);
	m_conf.cb_vs.vertex_offset = GSVector2(ox * sx + ox2 + 1, oy * sy + oy2 + 1);
	// END of FIXME

	// GS_SPRITE_CLASS are already flat (either by CPU or the GS)
	m_conf.ps.iip = (m_vt.m_primclass == GS_SPRITE_CLASS) ? 0 : PRIM->IIP;
	m_conf.gs.iip = m_conf.ps.iip;
	m_conf.vs.iip = m_conf.ps.iip;

	if (DATE_BARRIER)
	{
		m_conf.ps.date = 5 + m_context->TEST.DATM;
	}
	else if (DATE_one)
	{
		if (features.texture_barrier)
		{
			m_conf.require_one_barrier = true;
			m_conf.ps.date = 5 + m_context->TEST.DATM;
		}
		m_conf.depth.date = 1;
		m_conf.depth.date_one = 1;
	}
	else if (DATE)
	{
		if (DATE_PRIMID)
			m_conf.ps.date = 1 + m_context->TEST.DATM;
		else
			m_conf.depth.date = 1;
	}

	m_conf.ps.fba = m_context->FBA.FBA;

	if (m_conf.ps.dither)
	{
		GL_DBG("DITHERING mode ENABLED (%d)", m_dithering);

		m_conf.ps.dither = GSConfig.Dithering;
		m_conf.cb_ps.DitherMatrix[0] = GSVector4(m_env.DIMX.DM00, m_env.DIMX.DM01, m_env.DIMX.DM02, m_env.DIMX.DM03);
		m_conf.cb_ps.DitherMatrix[1] = GSVector4(m_env.DIMX.DM10, m_env.DIMX.DM11, m_env.DIMX.DM12, m_env.DIMX.DM13);
		m_conf.cb_ps.DitherMatrix[2] = GSVector4(m_env.DIMX.DM20, m_env.DIMX.DM21, m_env.DIMX.DM22, m_env.DIMX.DM23);
		m_conf.cb_ps.DitherMatrix[3] = GSVector4(m_env.DIMX.DM30, m_env.DIMX.DM31, m_env.DIMX.DM32, m_env.DIMX.DM33);
	}

	if (PRIM->FGE)
	{
		m_conf.ps.fog = 1;

		const GSVector4 fc = GSVector4::rgba32(m_env.FOGCOL.U32[0]);
		// Blend AREF to avoid to load a random value for alpha (dirty cache)
		m_conf.cb_ps.FogColor_AREF = fc.blend32<8>(m_conf.cb_ps.FogColor_AREF);
	}

	// Warning must be done after EmulateZbuffer
	// Depth test is always true so it can be executed in 2 passes (no order required) unlike color.
	// The idea is to compute first the color which is independent of the alpha test. And then do a 2nd
	// pass to handle the depth based on the alpha test.
	bool ate_RGBA_then_Z = false;
	bool ate_RGB_then_ZA = false;
	if (ate_first_pass & ate_second_pass)
	{
		GL_DBG("Complex Alpha Test");
		const bool commutative_depth = (m_conf.depth.ztst == ZTST_GEQUAL && m_vt.m_eq.z) || (m_conf.depth.ztst == ZTST_ALWAYS);
		const bool commutative_alpha = (m_context->ALPHA.C != 1); // when either Alpha Src or a constant

		ate_RGBA_then_Z = (m_context->TEST.AFAIL == AFAIL_FB_ONLY) & commutative_depth;
		ate_RGB_then_ZA = (m_context->TEST.AFAIL == AFAIL_RGB_ONLY) & commutative_depth & commutative_alpha;
	}

	if (ate_RGBA_then_Z)
	{
		GL_DBG("Alternate ATE handling: ate_RGBA_then_Z");
		// Render all color but don't update depth
		// ATE is disabled here
		m_conf.depth.zwe = false;
	}
	else if (ate_RGB_then_ZA)
	{
		GL_DBG("Alternate ATE handling: ate_RGB_then_ZA");
		// Render RGB color but don't update depth/alpha
		// ATE is disabled here
		m_conf.depth.zwe = false;
		m_conf.colormask.wa = false;
	}
	else
	{
		float aref = m_conf.cb_ps.FogColor_AREF.a;
		EmulateATST(aref, m_conf.ps, false);

		// avoid redundant cbuffer updates
		m_conf.cb_ps.FogColor_AREF.a = aref;
		m_conf.alpha_second_pass.ps_aref = aref;
	}

	if (tex)
	{
		EmulateTextureSampler(tex);
	}
	else
	{
		m_conf.ps.tfx = 4;
	}

	if (m_game.title == CRC::ICO)
	{
		const GSVertex* v = &m_vertex.buff[0];
		const GSVideoMode mode = GetVideoMode();
		if (tex && m_vt.m_primclass == GS_SPRITE_CLASS && m_vertex.next == 2 && PRIM->ABE && // Blend texture
			((v[1].U == 8200 && v[1].V == 7176 && mode == GSVideoMode::NTSC) || // at display resolution 512x448
			(v[1].U == 8200 && v[1].V == 8200 && mode == GSVideoMode::PAL)) && // at display resolution 512x512
			tex->m_TEX0.PSM == PSM_PSMT8H) // i.e. read the alpha channel of a 32 bits texture
		{
			// Note potentially we can limit to TBP0:0x2800

			// Depth buffer was moved so GS will invalide it which means a
			// downscale. ICO uses the MSB depth bits as the texture alpha
			// channel.  However this depth of field effect requires
			// texel:pixel mapping accuracy.
			//
			// Use an HLE shader to sample depth directly as the alpha channel
			GL_INS("ICO sample depth as alpha");
			m_conf.require_full_barrier = true;
			// Extract the depth as palette index
			m_conf.ps.depth_fmt = 1;
			m_conf.ps.channel = ChannelFetch_BLUE;
			m_conf.tex = ds;

			// We need the palette to convert the depth to the correct alpha value.
			if (!tex->m_palette)
			{
				const u16 pal = GSLocalMemory::m_psm[tex->m_TEX0.PSM].pal;
				m_tc->AttachPaletteToSource(tex, pal, true);
				m_conf.pal = tex->m_palette;
			}
		}
	}

	if (features.framebuffer_fetch)
	{
		// Intel GPUs on Metal lock up if you try to use DSB and framebuffer fetch at once
		// We should never need to do that (since using framebuffer fetch means you should be able to do all blending in shader), but sometimes it slips through
		if (m_conf.require_one_barrier || m_conf.require_full_barrier)
			ASSERT(!m_conf.blend.enable);

		// Barriers aren't needed with fbfetch.
		m_conf.require_one_barrier = false;
		m_conf.require_full_barrier = false;
	}

	// Swap full barrier for one barrier when there's no overlap.
	if (m_conf.require_full_barrier && m_prim_overlap == PRIM_OVERLAP_NO)
	{
		m_conf.require_full_barrier = false;
		m_conf.require_one_barrier = true;
	}

	// rs
	const GSVector4 hacked_scissor(m_channel_shuffle ? GSVector4(0, 0, 1024, 1024) : m_context->scissor.in);
	const GSVector4i scissor(GSVector4i(GSVector4(rtscale).xyxy() * hacked_scissor).rintersect(GSVector4i(rtsize).zwxy()));

	m_conf.drawarea = m_channel_shuffle ? scissor : scissor.rintersect(ComputeBoundingBox(rtscale, rtsize));
	m_conf.scissor = (DATE && !DATE_BARRIER) ? m_conf.drawarea : scissor;

	SetupIA(sx, sy);

	if (rt)
		rt->CommitRegion(GSVector2i(m_conf.drawarea.z, m_conf.drawarea.w));

	if (ds)
		ds->CommitRegion(GSVector2i(m_conf.drawarea.z, m_conf.drawarea.w));

	m_conf.alpha_second_pass.enable = ate_second_pass;

	if (ate_second_pass)
	{
		ASSERT(!m_env.PABE.PABE);
		memcpy(&m_conf.alpha_second_pass.ps,        &m_conf.ps,        sizeof(m_conf.ps));
		memcpy(&m_conf.alpha_second_pass.colormask, &m_conf.colormask, sizeof(m_conf.colormask));
		memcpy(&m_conf.alpha_second_pass.depth,     &m_conf.depth,     sizeof(m_conf.depth));

		if (ate_RGBA_then_Z | ate_RGB_then_ZA)
		{
			// Enable ATE as first pass to update the depth
			// of pixels that passed the alpha test
			EmulateATST(m_conf.alpha_second_pass.ps_aref, m_conf.alpha_second_pass.ps, false);
		}
		else
		{
			// second pass will process the pixels that failed
			// the alpha test
			EmulateATST(m_conf.alpha_second_pass.ps_aref, m_conf.alpha_second_pass.ps, true);
		}


		bool z = m_conf.depth.zwe;
		bool r = m_conf.colormask.wr;
		bool g = m_conf.colormask.wg;
		bool b = m_conf.colormask.wb;
		bool a = m_conf.colormask.wa;

		switch (m_context->TEST.AFAIL)
		{
			case AFAIL_KEEP: z = r = g = b = a = false; break; // none
			case AFAIL_FB_ONLY: z = false; break; // rgba
			case AFAIL_ZB_ONLY: r = g = b = a = false; break; // z
			case AFAIL_RGB_ONLY: z = a = false; break; // rgb
			default: __assume(0);
		}

		// Depth test should be disabled when depth writes are masked and similarly, Alpha test must be disabled
		// when writes to all of the alpha bits in the Framebuffer are masked.
		if (ate_RGBA_then_Z)
		{
			z = !m_context->ZBUF.ZMSK;
			r = g = b = a = false;
		}
		else if (ate_RGB_then_ZA)
		{
			z = !m_context->ZBUF.ZMSK;
			a = (m_context->FRAME.FBMSK & 0xFF000000) != 0xFF000000;
			r = g = b = false;
		}

		if (z || r || g || b || a)
		{
			m_conf.alpha_second_pass.depth.zwe = z;
			m_conf.alpha_second_pass.colormask.wr = r;
			m_conf.alpha_second_pass.colormask.wg = g;
			m_conf.alpha_second_pass.colormask.wb = b;
			m_conf.alpha_second_pass.colormask.wa = a;
			if (m_conf.alpha_second_pass.colormask.wrgba == 0)
				m_conf.alpha_second_pass.ps.DisableColorOutput();
		}
		else
		{
			m_conf.alpha_second_pass.enable = false;
		}
	}

	if (!ate_first_pass)
	{
		if (!m_conf.alpha_second_pass.enable)
			return;

		// RenderHW always renders first pass, replace first pass with second
		memcpy(&m_conf.ps,        &m_conf.alpha_second_pass.ps,        sizeof(m_conf.ps));
		memcpy(&m_conf.colormask, &m_conf.alpha_second_pass.colormask, sizeof(m_conf.colormask));
		memcpy(&m_conf.depth,     &m_conf.alpha_second_pass.depth,     sizeof(m_conf.depth));
		m_conf.cb_ps.FogColor_AREF.a = m_conf.alpha_second_pass.ps_aref;
		m_conf.alpha_second_pass.enable = false;
	}

	if (blending_alpha_pass)
	{
		// write alpha blend as the single alpha output
		m_conf.ps.no_ablend = true;

		// there's a case we can skip this: RGB_then_ZA alternate handling.
		// but otherwise, we need to write alpha separately.
		if (m_conf.colormask.wa)
		{
			m_conf.colormask.wa = false;
			m_conf.separate_alpha_pass = true;
		}

		// do we need to do this for the failed alpha fragments?
		if (m_conf.alpha_second_pass.enable)
		{
			// there's also a case we can skip here: when we're not writing RGB, there's
			// no blending, so we can just write the normal alpha!
			const u8 second_pass_wrgba = m_conf.alpha_second_pass.colormask.wrgba;
			if ((second_pass_wrgba & (1 << 3)) != 0 && second_pass_wrgba != (1 << 3))
			{
				// this sucks. potentially up to 4 passes. but no way around it when we don't have dual-source blend.
				m_conf.alpha_second_pass.ps.no_ablend = true;
				m_conf.alpha_second_pass.colormask.wa = false;
				m_conf.second_separate_alpha_pass = true;
			}
		}
	}

	m_conf.drawlist = (m_conf.require_full_barrier && m_vt.m_primclass == GS_SPRITE_CLASS) ? &m_drawlist : nullptr;

	g_gs_device->RenderHW(m_conf);
}

bool GSRendererHW::IsDummyTexture() const
{
	// Texture is actually the frame buffer. Stencil emulation to compute shadow (Jak series/tri-ace game)
	// Will hit the "m_ps_sel.tex_is_fb = 1" path in the draw
	return g_gs_device->Features().texture_barrier && (m_context->FRAME.Block() == m_context->TEX0.TBP0) && PRIM->TME && GSConfig.AccurateBlendingUnit != AccBlendLevel::Minimum && m_vt.m_primclass == GS_TRIANGLE_CLASS && (m_context->FRAME.FBMSK == 0x00FFFFFF);
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
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::BigMuthaTruckers, CRC::RegionCount, &GSRendererHW::OI_BigMuthaTruckers));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::DBZBT2, CRC::RegionCount, &GSRendererHW::OI_DBZBTGames));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::DBZBT3, CRC::RegionCount, &GSRendererHW::OI_DBZBTGames));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::FFXII, CRC::EU, &GSRendererHW::OI_FFXII));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::FFX, CRC::RegionCount, &GSRendererHW::OI_FFX));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::MetalSlug6, CRC::RegionCount, &GSRendererHW::OI_MetalSlug6));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::RozenMaidenGebetGarden, CRC::RegionCount, &GSRendererHW::OI_RozenMaidenGebetGarden));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::SonicUnleashed, CRC::RegionCount, &GSRendererHW::OI_SonicUnleashed));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::SuperManReturns, CRC::RegionCount, &GSRendererHW::OI_SuperManReturns));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::ArTonelico2, CRC::RegionCount, &GSRendererHW::OI_ArTonelico2));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::Jak2, CRC::RegionCount, &GSRendererHW::OI_JakGames));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::Jak3, CRC::RegionCount, &GSRendererHW::OI_JakGames));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::JakX, CRC::RegionCount, &GSRendererHW::OI_JakGames));
	m_oi_list.push_back(HackEntry<OI_Ptr>(CRC::BurnoutGames, CRC::RegionCount, &GSRendererHW::OI_BurnoutGames));

	m_oo_list.push_back(HackEntry<OO_Ptr>(CRC::BurnoutGames, CRC::RegionCount, &GSRendererHW::OO_BurnoutGames));
}

void GSRendererHW::Hacks::SetGameCRC(const CRC::Game& game)
{
	const u32 hash = (u32)((game.region << 24) | game.title);

	m_oi = m_oi_map[hash];
	m_oo = m_oo_map[hash];
	m_cu = m_cu_map[hash];

	if (GSConfig.PointListPalette)
	{
		if (m_oi)
			Console.Warning("Overriding m_oi with PointListPalette");

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
	if (!m_context->ZBUF.ZMSK && rt && ds)
	{
		const GSVertex* v = &m_vertex.buff[0];
		const GSLocalMemory::psm_t& frame_psm = GSLocalMemory::m_psm[m_context->FRAME.PSM];
		//const GSLocalMemory::psm_t& depth_psm = GSLocalMemory::m_psm[m_context->ZBUF.PSM];

		// Z and color must be constant and the same
		if (m_vt.m_eq.rgba != 0xFFFF || !m_vt.m_eq.z || v[1].XYZ.Z != v[1].RGBAQ.U32[0])
			return;

		// Format doesn't have the same size. It smells fishy (xmen...)
		//if (frame_psm.trbpp != depth_psm.trbpp)
		//	return;

		// Size of the current draw
		const u32 w_pages = static_cast<u32>(roundf(m_vt.m_max.p.x / frame_psm.pgs.x));
		const u32 h_pages = static_cast<u32>(roundf(m_vt.m_max.p.y / frame_psm.pgs.y));
		const u32 written_pages = w_pages * h_pages;

		// Frame and depth pointer can be inverted
		u32 base = 0, half = 0;
		if (m_context->FRAME.FBP > m_context->ZBUF.ZBP)
		{
			base = m_context->ZBUF.ZBP;
			half = m_context->FRAME.FBP;
		}
		else
		{
			base = m_context->FRAME.FBP;
			half = m_context->ZBUF.ZBP;
		}

		// If both buffers are side by side we can expect a fast clear in on-going
		if (half <= (base + written_pages))
		{
			const u32 color = v[1].RGBAQ.U32[0];
			const bool clear_depth = (m_context->FRAME.FBP > m_context->ZBUF.ZBP);

			GL_INS("OI_DoubleHalfClear:%s: base %x half %x. w_pages %d h_pages %d fbw %d. Color %x",
				clear_depth ? "depth" : "target", base << 5, half << 5, w_pages, h_pages, m_context->FRAME.FBW, color);

			// Commit texture with a factor 2 on the height
			GSTexture* t = clear_depth ? ds : rt;
			const GSVector4i commitRect = ComputeBoundingBox(t->GetScale(), t->GetSize());
			t->CommitRegion(GSVector2i(commitRect.z, 2 * commitRect.w));

			if (clear_depth)
			{
				// Only pure clear are supported for depth
				ASSERT(color == 0);
				g_gs_device->ClearDepth(t);
			}
			else
			{
				g_gs_device->ClearRenderTarget(t, color);
			}
		}
	}
	// Striped double clear done by Powerdrome and Snoopy Vs Red Baron, it will clear in 32 pixel stripes half done by the Z and half done by the FRAME
	else if (rt && !ds && m_context->FRAME.FBP == m_context->ZBUF.ZBP && (m_context->FRAME.PSM & 0x30) != (m_context->ZBUF.PSM & 0x30)
			&& (m_context->FRAME.PSM & 0xF) == (m_context->ZBUF.PSM & 0xF) && (u32)(GSVector4i(m_vt.m_max.p).z) == 0)
	{
		const GSVertex* v = &m_vertex.buff[0];
		
		// Z and color must be constant and the same
		if (m_vt.m_eq.rgba != 0xFFFF || !m_vt.m_eq.z || v[1].XYZ.Z != v[1].RGBAQ.U32[0])
			return;

		// If both buffers are side by side we can expect a fast clear in on-going
		const u32 color = v[1].RGBAQ.U32[0];
		const GSVector4i commitRect = ComputeBoundingBox(rt->GetScale(), rt->GetSize());
		rt->CommitRegion(GSVector2i(commitRect.z, commitRect.w));

		g_gs_device->ClearRenderTarget(rt, color);
	}
}

// Note: hack is safe, but it could impact the perf a little (normally games do only a couple of clear by frame)
void GSRendererHW::OI_GsMemClear()
{
	// Note gs mem clear must be tested before calling this function

	// Striped double clear done by Powerdrome and Snoopy Vs Red Baron, it will clear in 32 pixel stripes half done by the Z and half done by the FRAME
	const bool ZisFrame = m_context->FRAME.FBP == m_context->ZBUF.ZBP && (m_context->FRAME.PSM & 0x30) != (m_context->ZBUF.PSM & 0x30)
							&& (m_context->FRAME.PSM & 0xF) == (m_context->ZBUF.PSM & 0xF) && (u32)(GSVector4i(m_vt.m_max.p).z) == 0;

	// Limit it further to a full screen 0 write
	if (((m_vertex.next == 2) || ZisFrame) && m_vt.m_min.c.eq(GSVector4i(0)))
	{
		const GSOffset& off = m_context->offset.fb;
		const GSVector4i r = GSVector4i(m_vt.m_min.p.xyxy(m_vt.m_max.p)).rintersect(GSVector4i(m_context->scissor.in));
		// Limit the hack to a single fullscreen clear. Some games might use severals column to clear a screen
		// but hopefully it will be enough.
		if (r.width() <= 128 || r.height() <= 128)
			return;

		GL_INS("OI_GsMemClear (%d,%d => %d,%d)", r.x, r.y, r.z, r.w);
		const int format = GSLocalMemory::m_psm[m_context->FRAME.PSM].fmt;

		// FIXME: loop can likely be optimized with AVX/SSE. Pixels aren't
		// linear but the value will be done for all pixels of a block.
		// FIXME: maybe we could limit the write to the top and bottom row page.
		if (format == 0)
		{
			// Based on WritePixel32
			for (int y = r.top; y < r.bottom; y++)
			{
				auto pa = off.assertSizesMatch(GSLocalMemory::swizzle32).paMulti(m_mem.vm32(), 0, y);

				for (int x = r.left; x < r.right; x++)
				{
					*pa.value(x) = 0; // Here the constant color
				}
			}
		}
		else if (format == 1)
		{
			// Based on WritePixel24
			for (int y = r.top; y < r.bottom; y++)
			{
				auto pa = off.assertSizesMatch(GSLocalMemory::swizzle32).paMulti(m_mem.vm32(), 0, y);

				for (int x = r.left; x < r.right; x++)
				{
					*pa.value(x) &= 0xff000000; // Clear the color
				}
			}
		}
		else if (format == 2)
		{
			; // Hack is used for FMV which are likely 24/32 bits. Let's keep the for reference
#if 0
			// Based on WritePixel16
			for (int y = r.top; y < r.bottom; y++)
			{
				auto pa = off.assertSizesMatch(GSLocalMemory::swizzle16).paMulti(m_mem.m_vm16, 0, y);

				for (int x = r.left; x < r.right; x++)
				{
					*pa.value(x) = 0; // Here the constant color
				}
			}
#endif
		}
	}
}

bool GSRendererHW::OI_BlitFMV(GSTextureCache::Target* _rt, GSTextureCache::Source* tex, const GSVector4i& r_draw)
{
	if (r_draw.w > 1024 && (m_vt.m_primclass == GS_SPRITE_CLASS) && (m_vertex.next == 2) && PRIM->TME && !PRIM->ABE && tex && !tex->m_target && m_context->TEX0.TBW > 0)
	{
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
		const int tw = (int)(1 << m_context->TEX0.TW);
		const int th = (int)(1 << m_context->TEX0.TH);
		GSVector4 sRect;
		sRect.x = m_vt.m_min.t.x / tw;
		sRect.y = m_vt.m_min.t.y / th;
		sRect.z = m_vt.m_max.t.x / tw;
		sRect.w = m_vt.m_max.t.y / th;

		// Compute the Bottom of texture rectangle
		ASSERT(m_context->TEX0.TBP0 > m_context->FRAME.Block());
		const int offset = (m_context->TEX0.TBP0 - m_context->FRAME.Block()) / m_context->TEX0.TBW;
		GSVector4i r_texture(r_draw);
		r_texture.y -= offset;
		r_texture.w -= offset;

		const GSVector4 dRect(r_texture);

		// Do the blit. With a Copy mess to avoid issue with limited API (dx)
		// m_dev->StretchRect(tex->m_texture, sRect, tex->m_texture, dRect);
		const GSVector4i r_full(0, 0, tw, th);
		if (GSTexture* rt = g_gs_device->CreateRenderTarget(tw, th, GSTexture::Format::Color))
		{
			g_gs_device->CopyRect(tex->m_texture, rt, r_full, 0, 0);

			g_gs_device->StretchRect(tex->m_texture, sRect, rt, dRect);

			g_gs_device->CopyRect(rt, tex->m_texture, r_full, 0, 0);

			g_gs_device->Recycle(rt);
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

bool GSRendererHW::OI_BigMuthaTruckers(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	// Rendering pattern:
	// CRTC frontbuffer at 0x0 is interlaced (half vertical resolution),
	// game needs to do a depth effect (so green channel to alpha),
	// but there is a vram limitation so green is pushed into the alpha channel of the CRCT buffer,
	// vertical resolution is half so only half is processed at once
	// We, however, don't have this limitation so we'll replace the draw with a full-screen TS.

	const GIFRegTEX0 Texture = m_context->TEX0;

	GIFRegTEX0 Frame;
	Frame.TBW = m_context->FRAME.FBW;
	Frame.TBP0 = m_context->FRAME.FBP;
	Frame.TBP0 = m_context->FRAME.Block();

	if (PRIM->TME && Frame.TBW == 10 && Texture.TBW == 10 && Frame.TBP0 == 0x00a00 && Texture.PSM == PSM_PSMT8H && (m_r.y == 256 || m_r.y == 224))
	{
		// 224 ntsc, 256 pal.
		GL_INS("OI_BigMuthaTruckers half bottom offset");

		const size_t count = m_vertex.next;
		GSVertex* v = &m_vertex.buff[0];
		const u16 offset = (u16)m_r.y * 16;

		for (size_t i = 0; i < count; i++)
			v[i].V += offset;
	}

	return true;
}

bool GSRendererHW::OI_DBZBTGames(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	if (t && t->m_from_target) // Avoid slow framebuffer readback
		return true;

	if (!((m_r == GSVector4i(0, 0, 16, 16)).alltrue() || (m_r == GSVector4i(0, 0, 64, 64)).alltrue()))
		return true; // Only 16x16 or 64x64 draws.

	// Sprite rendering
	if (!CanUseSwSpriteRender())
		return true;

	SwSpriteRender();

	return false; // Skip current draw
}

bool GSRendererHW::OI_FFXII(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	static u32* video = NULL;
	static size_t lines = 0;

	if (lines == 0)
	{
		if (m_vt.m_primclass == GS_LINE_CLASS && (m_vertex.next == 448 * 2 || m_vertex.next == 512 * 2))
		{
			lines = m_vertex.next / 2;
		}
	}
	else
	{
		if (m_vt.m_primclass == GS_POINT_CLASS)
		{
			if (m_vertex.next >= 16 * 512)
			{
				// incoming pixels are stored in columns, one column is 16x512, total res 448x512 or 448x454

				if (!video)
					video = new u32[512 * 512];

				const int ox = m_context->XYOFFSET.OFX - 8;
				const int oy = m_context->XYOFFSET.OFY - 8;

				const GSVertex* RESTRICT v = m_vertex.buff;

				for (int i = (int)m_vertex.next; i > 0; i--, v++)
				{
					int x = (v->XYZ.X - ox) >> 4;
					int y = (v->XYZ.Y - oy) >> 4;

					if (x < 0 || x >= 448 || y < 0 || y >= (int)lines)
						return false; // le sigh

					video[(y << 8) + (y << 7) + (y << 6) + x] = v->RGBAQ.U32[0];
				}

				return false;
			}
			else
			{
				lines = 0;
			}
		}
		else if (m_vt.m_primclass == GS_LINE_CLASS)
		{
			if (m_vertex.next == lines * 2)
			{
				// normally, this step would copy the video onto screen with 512 texture mapped horizontal lines,
				// but we use the stored video data to create a new texture, and replace the lines with two triangles

				g_gs_device->Recycle(t->m_texture);

				t->m_texture = g_gs_device->CreateTexture(512, 512, false, GSTexture::Format::Color);

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
	const u32 FBP = m_context->FRAME.Block();
	const u32 ZBP = m_context->ZBUF.Block();
	const u32 TBP = m_context->TEX0.TBP0;

	if ((FBP == 0x00d00 || FBP == 0x00000) && ZBP == 0x02100 && PRIM->TME && TBP == 0x01a00 && m_context->TEX0.PSM == PSM_PSMCT16S)
	{
		// random battle transition (z buffer written directly, clear it now)
		GL_INS("OI_FFX ZB clear");
		if (ds)
			ds->Commit(); // Don't bother to save few MB for a single game
		g_gs_device->ClearDepth(ds);
	}

	return true;
}

bool GSRendererHW::OI_MetalSlug6(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	// missing red channel fix (looks alright in pcsx2 r5000+)

	GSVertex* RESTRICT v = m_vertex.buff;

	for (int i = (int)m_vertex.next; i > 0; i--, v++)
	{
		const u32 c = v->RGBAQ.U32[0];

		const u32 r = (c >> 0) & 0xff;
		const u32 g = (c >> 8) & 0xff;
		const u32 b = (c >> 16) & 0xff;

		if (r == 0 && g != 0 && b != 0)
		{
			v->RGBAQ.U32[0] = (c & 0xffffff00) | ((g + b + 1) >> 1);
		}
	}

	m_vt.Update(m_vertex.buff, m_index.buff, m_vertex.tail, m_index.tail, m_vt.m_primclass);

	return true;
}

bool GSRendererHW::OI_RozenMaidenGebetGarden(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	if (!PRIM->TME)
	{
		const u32 FBP = m_context->FRAME.Block();
		const u32 ZBP = m_context->ZBUF.Block();

		if (FBP == 0x008c0 && ZBP == 0x01a40)
		{
			//  frame buffer clear, atst = fail, afail = write z only, z buffer points to frame buffer

			GIFRegTEX0 TEX0;

			TEX0.TBP0 = ZBP;
			TEX0.TBW = m_context->FRAME.FBW;
			TEX0.PSM = m_context->FRAME.PSM;

			if (GSTextureCache::Target* tmp_rt = m_tc->LookupTarget(TEX0, GetTargetSize(), GSTextureCache::RenderTarget, true))
			{
				GL_INS("OI_RozenMaidenGebetGarden FB clear");
				tmp_rt->m_texture->Commit(); // Don't bother to save few MB for a single game
				g_gs_device->ClearRenderTarget(tmp_rt->m_texture, 0);
			}

			return false;
		}
		else if (FBP == 0x00000 && m_context->ZBUF.Block() == 0x01180)
		{
			// z buffer clear, frame buffer now points to the z buffer (how can they be so clever?)

			GIFRegTEX0 TEX0;

			TEX0.TBP0 = FBP;
			TEX0.TBW = m_context->FRAME.FBW;
			TEX0.PSM = m_context->ZBUF.PSM;

			if (GSTextureCache::Target* tmp_ds = m_tc->LookupTarget(TEX0, GetTargetSize(), GSTextureCache::DepthStencil, true))
			{
				GL_INS("OI_RozenMaidenGebetGarden ZB clear");
				tmp_ds->m_texture->Commit(); // Don't bother to save few MB for a single game
				g_gs_device->ClearDepth(tmp_ds->m_texture);
			}

			return false;
		}
	}

	return true;
}

bool GSRendererHW::OI_SonicUnleashed(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	// Rendering pattern is:
	// Save RG channel with a kind of a TS (replaced by a copy in this hack),
	// compute shadow in RG,
	// save result in alpha with a TS,
	// Restore RG channel that we previously copied to render shadows.

	const GIFRegTEX0 Texture = m_context->TEX0;

	GIFRegTEX0 Frame;
	Frame.TBW = m_context->FRAME.FBW;
	Frame.TBP0 = m_context->FRAME.FBP;
	Frame.TBP0 = m_context->FRAME.Block();
	Frame.PSM = m_context->FRAME.PSM;

	if ((!PRIM->TME) || (GSLocalMemory::m_psm[Texture.PSM].bpp != 16) || (GSLocalMemory::m_psm[Frame.PSM].bpp != 16))
		return true;

	if ((Texture.TBP0 == Frame.TBP0) || (Frame.TBW != 16 && Texture.TBW != 16))
		return true;

	GL_INS("OI_SonicUnleashed replace draw by a copy");

	GSTextureCache::Target* src = m_tc->LookupTarget(Texture, GetTargetSize(), GSTextureCache::RenderTarget, true);

	const GSVector2i size = rt->GetSize();

	const GSVector4 sRect(0, 0, 1, 1);
	const GSVector4 dRect(0, 0, size.x, size.y);

	g_gs_device->StretchRect(src->m_texture, sRect, rt, dRect, true, true, true, false);

	return false;
}

bool GSRendererHW::OI_PointListPalette(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	const size_t n_vertices = m_vertex.next;
	const int w = m_r.width();
	const int h = m_r.height();
	const bool is_copy = !PRIM->ABE || (
		m_context->ALPHA.A == m_context->ALPHA.B // (A - B) == 0 in blending equation, makes C value irrelevant.
		&& m_context->ALPHA.D == 0 // Copy source RGB(A) color into frame buffer.
	);
	if (m_vt.m_primclass == GS_POINT_CLASS && w <= 64 // Small draws.
		&& h <= 64 // Small draws.
		&& n_vertices <= 256 // Small draws.
		&& is_copy // Copy (no blending).
		&& !PRIM->TME // No texturing please.
		&& m_context->FRAME.PSM == PSM_PSMCT32 // Only 32-bit pixel format (CLUT format).
		&& !PRIM->FGE // No FOG.
		&& !PRIM->AA1 // No antialiasing.
		&& !PRIM->FIX // Normal fragment value control.
		&& !m_env.DTHE.DTHE // No dithering.
		&& !m_context->TEST.ATE // No alpha test.
		&& !m_context->TEST.DATE // No destination alpha test.
		&& (!m_context->DepthRead() && !m_context->DepthWrite()) // No depth handling.
		&& !m_context->TEX0.CSM // No CLUT usage.
		&& !m_env.PABE.PABE // No PABE.
		&& m_context->FBA.FBA == 0 // No Alpha Correction.
		&& m_context->FRAME.FBMSK == 0 // No frame buffer masking.
	)
	{
		const u32 FBP = m_context->FRAME.Block();
		const u32 FBW = m_context->FRAME.FBW;
		GL_INS("PointListPalette - m_r = <%d, %d => %d, %d>, n_vertices = %zu, FBP = 0x%x, FBW = %u", m_r.x, m_r.y, m_r.z, m_r.w, n_vertices, FBP, FBW);
		const GSVertex* RESTRICT v = m_vertex.buff;
		const int ox(m_context->XYOFFSET.OFX);
		const int oy(m_context->XYOFFSET.OFY);
		for (size_t i = 0; i < n_vertices; ++i)
		{
			const GSVertex& vi = v[i];
			const GIFRegXYZ& xyz = vi.XYZ;
			const int x = (int(xyz.X) - ox) / 16;
			const int y = (int(xyz.Y) - oy) / 16;
			if (x < m_r.x || x > m_r.z)
				continue;
			if (y < m_r.y || y > m_r.w)
				continue;
			const u32 c = vi.RGBAQ.U32[0];
			m_mem.WritePixel32(x, y, c, FBP, FBW);
		}
		m_tc->InvalidateVideoMem(m_context->offset.fb, m_r);
		return false;
	}
	return true;
}

bool GSRendererHW::OI_SuperManReturns(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	// Instead to use a fullscreen rectangle they use a 32 pixels, 4096 pixels with a FBW of 1.
	// Technically the FB wrap/overlap on itself...
	const GSDrawingContext* ctx = m_context;
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
	if (rt)
		rt->Commit(); // Don't bother to save few MB for a single game
	g_gs_device->ClearRenderTarget(rt, GSVector4(m_vt.m_min.c));

	m_tc->InvalidateVideoMemType(GSTextureCache::DepthStencil, ctx->FRAME.Block());
	GL_INS("OI_SuperManReturns");

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

	const GSVertex* v = &m_vertex.buff[0];

	if (m_vertex.next == 2 && !PRIM->TME && m_context->FRAME.FBW == 10 && v->XYZ.Z == 0 && m_context->TEST.ZTST == ZTST_ALWAYS)
	{
		GL_INS("OI_ArTonelico2");
		if (ds)
			ds->Commit(); // Don't bother to save few MB for a single game
		g_gs_device->ClearDepth(ds);
	}

	return true;
}

bool GSRendererHW::OI_JakGames(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	if (!(m_r == GSVector4i(0, 0, 16, 16)).alltrue())
		return true; // Only 16x16 draws.

	if (!CanUseSwSpriteRender())
		return true;

	// Render 16x16 palette via CPU.
	SwSpriteRender();

	return false; // Skip current draw.
}

bool GSRendererHW::OI_BurnoutGames(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t)
{
	if (!OI_PointListPalette(rt, ds, t))
		return false; // Render point list palette.

	if (t && t->m_from_target) // Avoid slow framebuffer readback
		return true;

	if (!CanUseSwSpriteRender())
		return true;

	// Render palette via CPU.
	SwSpriteRender();

	return false;
}

// OO (others output?) hacks: invalidate extra local memory after the draw call

void GSRendererHW::OO_BurnoutGames()
{
	const GIFRegTEX0& TEX0 = m_context->TEX0;
	const GIFRegALPHA& ALPHA = m_context->ALPHA;
	const GIFRegFRAME& FRAME = m_context->FRAME;
	if (PRIM->PRIM == GS_SPRITE
		&& !PRIM->IIP
		&& PRIM->TME
		&& !PRIM->FGE
		&& PRIM->ABE
		&& !PRIM->AA1
		&& !PRIM->FST
		&& !PRIM->FIX
		&& TEX0.TBW == 16
		&& TEX0.TW == 10
		&& TEX0.TCC
		&& !TEX0.TFX
		&& TEX0.PSM == PSM_PSMT8
		&& TEX0.CPSM == PSM_PSMCT32
		&& !TEX0.CSM
		&& TEX0.TH == 8
		&& ALPHA.A == ALPHA.B
		&& ALPHA.D == 0
		&& FRAME.FBW == 16
		&& FRAME.PSM == PSM_PSMCT32)
	{
		// Readback clouds being rendered during level loading.
		// Later the alpha channel from the 32 bit frame buffer is used as an 8 bit indexed texture to draw
		// the clouds on top of the sky at each frame.
		// Burnout 3 PAL 50Hz: 0x3ba0 => 0x1e80.
		GL_INS("OO_BurnoutGames - Readback clouds renderered from TEX0.TBP0 = 0x%04x (TEX0.CBP = 0x%04x) to FBP = 0x%04x", TEX0.TBP0, TEX0.CBP, FRAME.Block());
		m_tc->InvalidateLocalMem(m_context->offset.fb, m_r);
	}
}

// Can Upscale hacks: disable upscaling for some draw calls

// None required.
