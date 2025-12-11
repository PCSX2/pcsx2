// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/Renderers/HW/GSRendererHW.h"
#include "GS/Renderers/HW/GSTextureReplacements.h"
#include "GS/GSGL.h"
#include "GS/GSPerfMon.h"
#include "GS/GSUtil.h"
#include "Host.h"
#include "common/Console.h"
#include "common/BitUtils.h"
#include "common/StringUtil.h"
#include <bit>

GSRendererHW::GSRendererHW()
	: GSRenderer()
{
	MULTI_ISA_SELECT(GSRendererHWPopulateFunctions)(*this);
	m_mipmap = GSConfig.HWMipmap;
	SetTCOffset();

	pxAssert(!g_texture_cache);
	g_texture_cache = std::make_unique<GSTextureCache>();
	GSTextureReplacements::Initialize();

	// Hope nothing requires too many draw calls.
	m_drawlist.reserve(2048);

	memset(static_cast<void*>(&m_conf), 0, sizeof(m_conf));

	ResetStates();
}

void GSRendererHW::SetTCOffset()
{
	m_userhacks_tcoffset_x = std::max<s32>(GSConfig.UserHacks_TCOffsetX, 0) / -1000.0f;
	m_userhacks_tcoffset_y = std::max<s32>(GSConfig.UserHacks_TCOffsetY, 0) / -1000.0f;
	m_userhacks_tcoffset = m_userhacks_tcoffset_x < 0.0f || m_userhacks_tcoffset_y < 0.0f;
}

GSRendererHW::~GSRendererHW()
{
	g_texture_cache.reset();
}

void GSRendererHW::Destroy()
{
	g_texture_cache->RemoveAll(true, true, true);
	GSRenderer::Destroy();
}

void GSRendererHW::PurgeTextureCache(bool sources, bool targets, bool hash_cache)
{
	g_texture_cache->RemoveAll(sources, targets, hash_cache);
}

void GSRendererHW::ReadbackTextureCache()
{
	g_texture_cache->ReadbackAll();
}

GSTexture* GSRendererHW::LookupPaletteSource(u32 CBP, u32 CPSM, u32 CBW, GSVector2i& offset, float* scale, const GSVector2i& size)
{
	return g_texture_cache->LookupPaletteSource(CBP, CPSM, CBW, offset, scale, size);
}

bool GSRendererHW::CanUpscale()
{
	return GSConfig.UpscaleMultiplier != 1.0f;
}

float GSRendererHW::GetUpscaleMultiplier()
{
	return GSConfig.UpscaleMultiplier;
}

void GSRendererHW::Reset(bool hardware_reset)
{
	// Read back on CSR Reset, conditional downloading on render swap etc handled elsewhere.
	if (!hardware_reset)
		g_texture_cache->ReadbackAll();

	g_texture_cache->RemoveAll(true, true, true);

	GSRenderer::Reset(hardware_reset);
}

void GSRendererHW::UpdateSettings(const Pcsx2Config::GSOptions& old_config)
{
	GSRenderer::UpdateSettings(old_config);
	m_mipmap = GSConfig.HWMipmap;
	SetTCOffset();
}

void GSRendererHW::VSync(u32 field, bool registers_written, bool idle_frame)
{
	if (GSConfig.LoadTextureReplacements)
		GSTextureReplacements::ProcessAsyncLoadedTextures();

	if (!idle_frame)
	{
		// If it did draws very recently, we should keep the recent stuff in case it hasn't been preloaded/used yet.
		// Rocky Legend does this with the main menu FMV's.
		if (s_last_transfer_draw_n > (s_n - 5))
		{
			for (auto iter = m_draw_transfers.rbegin(); iter != m_draw_transfers.rend(); iter++)
			{
				if ((s_n - iter->draw) > 50)
				{
					m_draw_transfers.erase(m_draw_transfers.begin(), std::next(iter).base());
					break;
				}
			}
		}
		else
		{
			m_draw_transfers.clear();
		}

		g_texture_cache->IncAge();
	}
	else
	{
		// Don't age the texture cache when no draws or EE writes have occurred.
		// Xenosaga needs its targets kept around while it's loading, because it uses them for a fade transition.
		GL_INS("HW: No draws or transfers, not aging TC");
	}

	if (g_texture_cache->GetHashCacheMemoryUsage() > 1024 * 1024 * 1024)
	{
		Host::AddKeyedOSDMessage("HashCacheOverflow",
			fmt::format(TRANSLATE_FS("GS", "Hash cache has used {:.2f} MB of VRAM, disabling."),
				static_cast<float>(g_texture_cache->GetHashCacheMemoryUsage()) / 1048576.0f),
			Host::OSD_ERROR_DURATION);
		g_texture_cache->RemoveAll(true, false, true);
		g_gs_device->PurgePool();
		GSConfig.TexturePreloading = TexturePreloadingLevel::Partial;
	}

	m_skip = 0;
	m_skip_offset = 0;

	GSRenderer::VSync(field, registers_written, idle_frame);
}

GSTexture* GSRendererHW::GetOutput(int i, float& scale, int& y_offset)
{
	int index = i >= 0 ? i : 1;

	GSPCRTCRegs::PCRTCDisplay& curFramebuffer = PCRTCDisplays.PCRTCDisplays[index];
	const GSVector2i framebufferSize(PCRTCDisplays.GetFramebufferSize(i));

	if (curFramebuffer.framebufferRect.rempty() || curFramebuffer.FBW == 0)
		return nullptr;

	PCRTCDisplays.RemoveFramebufferOffset(i);
	// TRACE(_T("[%d] GetOutput %d %05x (%d)\n"), (int)m_perfmon.GetFrame(), i, (int)TEX0.TBP0, (int)TEX0.PSM);

	GSTexture* t = nullptr;

	GIFRegTEX0 TEX0 = {};
	TEX0.TBP0 = curFramebuffer.Block();
	TEX0.TBW = curFramebuffer.FBW;
	TEX0.PSM = curFramebuffer.PSM;

	if (GSTextureCache::Target* rt = g_texture_cache->LookupDisplayTarget(TEX0, framebufferSize, GetTextureScaleFactor(), false))
	{
		rt->Update();
		t = rt->m_texture;
		scale = rt->m_scale;

		const int delta = TEX0.TBP0 - rt->m_TEX0.TBP0;
		if (delta > 0 && curFramebuffer.FBW != 0)
		{
			const int pages = delta >> 5u;
			int y_pages = pages / curFramebuffer.FBW;
			y_offset = y_pages * GSLocalMemory::m_psm[curFramebuffer.PSM].pgs.y;
			GL_CACHE("HW: Frame y offset %d pixels, unit %d", y_offset, i);
		}

		if (GSConfig.SaveFrame && GSConfig.ShouldDump(s_n, g_perfmon.GetFrame()))
		{
			t->Save(GetDrawDumpPath("%05d_f%05lld_fr%d_%05x_%s.bmp", s_n, g_perfmon.GetFrame(), i, static_cast<int>(TEX0.TBP0), GSUtil::GetPSMName(TEX0.PSM)));
		}
	}

	return t;
}

GSTexture* GSRendererHW::GetFeedbackOutput(float& scale)
{
	const int index = m_regs->EXTBUF.FBIN & 1;
	const GSVector2i fb_size(PCRTCDisplays.GetFramebufferSize(index));

	GIFRegTEX0 TEX0 = {};
	TEX0.TBP0 = m_regs->EXTBUF.EXBP;
	TEX0.TBW = m_regs->EXTBUF.EXBW;
	TEX0.PSM = PCRTCDisplays.PCRTCDisplays[index].PSM;

	GSTextureCache::Target* rt = g_texture_cache->LookupDisplayTarget(TEX0, fb_size, GetTextureScaleFactor(), true);
	if (!rt)
		return nullptr;

	rt->Update();
	GSTexture* t = rt->m_texture;
	scale = rt->m_scale;

	if (GSConfig.SaveFrame && GSConfig.ShouldDump(s_n, g_perfmon.GetFrame()))
		t->Save(GetDrawDumpPath("%05d_f%05lld_fr%d_%05x_%s.bmp", s_n, g_perfmon.GetFrame(), 3, static_cast<int>(TEX0.TBP0), GSUtil::GetPSMName(TEX0.PSM)));

	return t;
}

void GSRendererHW::Lines2Sprites()
{
	pxAssert(m_vt.m_primclass == GS_SPRITE_CLASS);

	// each sprite converted to quad needs twice the space

	while (m_vertex.tail * 2 > m_vertex.maxcount)
	{
		GrowVertexBuffer();
	}

	// assume vertices are tightly packed and sequentially indexed (it should be the case)
	const bool predivide_q = PRIM->TME && !PRIM->FST && m_vt.m_accurate_stq;

	if (m_vertex.next >= 2)
	{
		const u32 count = m_vertex.next;

		int i = static_cast<int>(count) * 2 - 4;
		GSVertex* s = &m_vertex.buff[count - 2];
		GSVertex* q = &m_vertex.buff[count * 2 - 4];
		u16* RESTRICT index = &m_index.buff[count * 3 - 6];

		// Sprites are flat shaded, so the provoking vertex doesn't matter here.
		constexpr GSVector4i indices = GSVector4i::cxpr16(0, 1, 2, 1, 2, 3, 0, 0);

		for (; i >= 0; i -= 4, s -= 2, q -= 4, index -= 6)
		{
			GSVertex v0 = s[0];
			GSVertex v1 = s[1];

			v0.RGBAQ = v1.RGBAQ;
			v0.XYZ.Z = v1.XYZ.Z;
			v0.FOG = v1.FOG;

			if (predivide_q)
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

			const float v0_st_s = v0.ST.S;
			v0.ST.S = v1.ST.S;
			v1.ST.S = v0_st_s;

			const u16 u = v0.U;
			v0.U = v1.U;
			v1.U = u;

			q[1] = v0;
			q[2] = v1;

			const GSVector4i this_indices = GSVector4i::broadcast16(i).add16(indices);
			const int high = this_indices.extract32<2>();
			GSVector4i::storel(index, this_indices);
			std::memcpy(&index[4], &high, sizeof(high));
		}

		m_vertex.head = m_vertex.tail = m_vertex.next = count * 2;
		m_index.tail = count * 3;
	}
}

void GSRendererHW::ExpandLineIndices()
{
	const u32 process_count = (m_index.tail + 7) / 8 * 8;
	constexpr u32 expansion_factor = 3;
	m_index.tail *= expansion_factor;
	GSVector4i* end = reinterpret_cast<GSVector4i*>(m_index.buff);
	GSVector4i* read = reinterpret_cast<GSVector4i*>(m_index.buff + process_count);
	GSVector4i* write = reinterpret_cast<GSVector4i*>(m_index.buff + process_count * expansion_factor);

	constexpr GSVector4i mask0 = GSVector4i::cxpr8(0, 1, 0, 1, 2, 3, 0, 1, 2, 3, 2, 3, 4, 5, 4, 5);
	constexpr GSVector4i mask1 = GSVector4i::cxpr8(6, 7, 4, 5, 6, 7, 6, 7, 8, 9, 8, 9, 10, 11, 8, 9);
	constexpr GSVector4i mask2 = GSVector4i::cxpr8(10, 11, 10, 11, 12, 13, 12, 13, 14, 15, 12, 13, 14, 15, 14, 15);

	constexpr GSVector4i low0 = GSVector4i::cxpr16(0, 1, 2, 1, 2, 3, 0, 1);
	constexpr GSVector4i low1 = GSVector4i::cxpr16(2, 1, 2, 3, 0, 1, 2, 1);
	constexpr GSVector4i low2 = GSVector4i::cxpr16(2, 3, 0, 1, 2, 1, 2, 3);

	while (read > end)
	{
		read -= 1;
		write -= expansion_factor;

		const GSVector4i in = read->sll16<2>();
		write[0] = in.shuffle8(mask0) | low0;
		write[1] = in.shuffle8(mask1) | low1;
		write[2] = in.shuffle8(mask2) | low2;
	}
}

// Return true if the sprite reverses the same 8 pixels between the src and dst.
// Used by games to corrected reversed pixels in a texture shuffle.
__fi bool GSRendererHW::Is8PixelReverseSprite(const GSVertex& v0, const GSVertex& v1)
{
	pxAssert(m_vt.m_primclass == GS_SPRITE_CLASS);

	const GIFRegXYOFFSET& o = m_context->XYOFFSET;
	const float tw = static_cast<float>(1u << m_cached_ctx.TEX0.TW);

	int pos0 = std::max(static_cast<int>(v0.XYZ.X) - static_cast<int>(o.OFX), 0);
	int pos1 = std::max(static_cast<int>(v1.XYZ.X) - static_cast<int>(o.OFX), 0);

	const bool rev_pos = pos0 > pos1;
	if (rev_pos)
		std::swap(pos0, pos1);

	int tex0 = (PRIM->FST) ? v0.U : static_cast<int>(tw * v0.ST.S * 16.0f);
	int tex1 = (PRIM->FST) ? v1.U : static_cast<int>(tw * v1.ST.S * 16.0f);

	const bool rev_tex = tex0 > tex1;
	if (rev_tex)
		std::swap(tex0, tex1);

	// Sprite flips a single column and does nothing else.
	return std::abs(pos1 - pos0) < 136 &&
	       std::abs(pos0 - tex0) <= 8 && std::abs(pos1 - tex1) <= 8 &&
	       rev_pos != rev_tex;
}

// Fix the vertex position/tex_coordinate from 16 bits color to 32 bits color
void GSRendererHW::ConvertSpriteTextureShuffle(u32& process_rg, u32& process_ba, bool& shuffle_across, GSTextureCache::Target* rt, GSTextureCache::Source* tex)
{
	pxAssert(m_vertex.next % 2 == 0); // Either sprites or an even number of triangles.

	const bool recursive_draw = m_cached_ctx.FRAME.Block() == m_cached_ctx.TEX0.TBP0;
	const bool sprites = m_vt.m_primclass == GS_SPRITE_CLASS;

	u32 count = m_vertex.next;
	GSVertex* v = &m_vertex.buff[0];
	const GIFRegXYOFFSET& o = m_context->XYOFFSET;
	// Could be drawing upside down or just back to front on the actual verts.
	// Iterate through the sprites in order and find one to infer which channels are being shuffled.
	const int prim_order = v[0].XYZ.X <= v[count - 2].XYZ.X ? 1 : -1;
	const int prim_start = prim_order > 0 ? 0 : count - 2;
	const int prim_end = prim_order > 0 ? count - 2 : 0;
	int tries = 0;
	int prim = prim_start;
	for (prim = prim_start;; prim += 2 * prim_order, tries++)
	{
		if (!(recursive_draw && sprites && Is8PixelReverseSprite(v[prim], v[prim + 1])))
			break; // Found the right prim.

		// Two tries at most, by the second prim we should be able to infer the channels.
		if (prim == prim_end || tries >= 2) 
		{
			prim = prim_start; // Use the first prim in order by default.
			GL_INS("Warning: ConvertSpriteTextureShuffle: Could not find correct prim for shuffle.");
			break;
		}
	}

	// Get first and second vertex for the prim we will use to infer shuffled channels.
	const bool rev_pos = v[prim].XYZ.X > v[prim + 1].XYZ.X;
	const GSVertex& first_vert = rev_pos ? v[prim + 1] : v[prim];
	const GSVertex& second_vert = rev_pos ? v[prim] : v[prim + 1];
	const int pos = std::max(static_cast<int>(first_vert.XYZ.X) - static_cast<int>(o.OFX), 0) & 0xFF;

	// Read texture is 8 to 16 pixels (same as above)
	const float tw = static_cast<float>(1u << m_cached_ctx.TEX0.TW);
	const int u0 = PRIM->FST ? v[prim].U : static_cast<int>(tw * v[prim].ST.S * 16.0f);
	const int u1 = PRIM->FST ? v[prim + 1].U : static_cast<int>(tw * v[prim + 1].ST.S * 16.0f);
	const bool rev_tex = u0 > u1;
	int tex_pos = rev_tex ? u1 : u0;
	tex_pos &= 0xFF;
	shuffle_across = (((tex_pos + 8) >> 4) ^ ((pos + 8) >> 4)) & 0x8;

	const bool full_width = ((second_vert.XYZ.X - first_vert.XYZ.X) >> 4) >= 16 && m_r.width() > 8 && tex && tex->m_from_target && rt == tex->m_from_target;
	const bool width_multiple_16 = (((second_vert.XYZ.X - first_vert.XYZ.X) >> 7) & 1) == 0;
	const bool rev_pixels = rev_pos != rev_tex; // Whether pixels are reversed between src and dst.
	shuffle_across |= full_width && rev_pixels;
	process_ba = ((pos > 112 && pos < 136) || full_width) ? SHUFFLE_WRITE : 0;
	process_rg = (!process_ba || full_width) ? SHUFFLE_WRITE : 0;
	// "same group" means it can read blue and write alpha using C32 tricks
	process_ba |= ((tex_pos > 112 && tex_pos < 144) || (m_same_group_texture_shuffle && (m_cached_ctx.FRAME.FBMSK & 0xFFFF0000) != 0xFFFF0000) || full_width) ? SHUFFLE_READ : 0;
	process_rg |= (!(process_ba & SHUFFLE_READ) || full_width) ? SHUFFLE_READ : 0;

	// Another way of selecting whether to read RG/BA is to use region repeat.
	// Ace Combat 04 reads RG, writes to RGBA by setting a MINU of 1015.
	if (m_cached_ctx.CLAMP.WMS == CLAMP_REGION_REPEAT)
	{
		GL_INS("HW: REGION_REPEAT clamp with texture shuffle, FBMSK=%08x, MINU=%u, MINV=%u, MAXU=%u, MAXV=%u",
			m_cached_ctx.FRAME.FBMSK, m_cached_ctx.CLAMP.MINU, m_cached_ctx.CLAMP.MINV, m_cached_ctx.CLAMP.MAXU,
			m_cached_ctx.CLAMP.MAXV);

		// offset coordinates swap around RG/BA.
		const u32 maxu = (m_cached_ctx.CLAMP.MAXU & 8);
		const u32 minu = (m_cached_ctx.CLAMP.MINU & 8);
		if (maxu)
		{
			process_ba |=  SHUFFLE_READ;
			process_rg &= ~SHUFFLE_READ;
			if (!PRIM->ABE && (process_rg & SHUFFLE_WRITE))
			{
				process_ba &= ~SHUFFLE_WRITE;
				shuffle_across = true;
			}
		}
		else if (minu == 0)
		{
			process_rg |=  SHUFFLE_READ;
			process_ba &= ~SHUFFLE_READ;

			if (!PRIM->ABE && (process_ba & SHUFFLE_WRITE))
			{
				process_rg &= ~SHUFFLE_WRITE;
				shuffle_across = true;
			}
		}
	}
	bool half_right_vert = true;
	bool half_bottom_vert = true;

	if (m_split_texture_shuffle_pages > 0 || m_same_group_texture_shuffle)
	{
		if (m_same_group_texture_shuffle)
		{
			if (m_r.x & 8)
			{
				m_r.x &= ~8;
				m_vt.m_min.p.x = m_r.x;
			}
			else if ((m_r.z + 1) & 8)
			{
				m_r.z += 8;
				m_vt.m_max.p.z = m_r.z;
			}

			if (m_cached_ctx.FRAME.FBW != rt->m_TEX0.TBW && m_cached_ctx.FRAME.FBW == rt->m_TEX0.TBW * 2)
			{
				half_bottom_vert = false;
				m_vt.m_min.p.x /= 2.0f;
				m_vt.m_max.p.x = floor(m_vt.m_max.p.y + 1.9f) / 2.0f;
			}
			else
			{
				half_right_vert = false;
				m_vt.m_min.p.y /= 2.0f;
				m_vt.m_max.p.y = floor(m_vt.m_max.p.y + 1.9f) / 2.0f;
			}

			m_context->scissor.in.x = m_vt.m_min.p.x;
			m_context->scissor.in.z = m_vt.m_max.p.x + 0.9f;
			m_context->scissor.in.y = m_vt.m_min.p.y;
			m_context->scissor.in.w = m_vt.m_max.p.y + 0.9f;
		}

		// Input vertices might be bad, so rewrite them.
		// We can't use the draw rect exactly here, because if the target was actually larger
		// for some reason... unhandled clears, maybe, it won't have been halved correctly.
		// So, halve it ourselves.

		const GSVector4i dr = m_r;
		const GSVector4i r = half_bottom_vert ? dr.blend32<0xA>(dr.sra32<1>()) : dr.blend32<5>(dr.sra32<1>()); // Half Y : Half X
		GL_CACHE("HW: ConvertSpriteTextureShuffle: Rewrite from %d,%d => %d,%d to %d,%d => %d,%d",
			static_cast<int>(m_vt.m_min.p.x), static_cast<int>(m_vt.m_min.p.y), static_cast<int>(m_vt.m_min.p.z),
			static_cast<int>(m_vt.m_min.p.w), r.x, r.y, r.z, r.w);

		const GSVector4i fpr = r.sll32<4>();
		v[0].XYZ.X = static_cast<u16>(m_context->XYOFFSET.OFX + fpr.x);
		v[0].XYZ.Y = static_cast<u16>(m_context->XYOFFSET.OFY + fpr.y);

		v[1].XYZ.X = static_cast<u16>(m_context->XYOFFSET.OFX + fpr.z);
		v[1].XYZ.Y = static_cast<u16>(m_context->XYOFFSET.OFY + fpr.w);

		if (m_same_group_texture_shuffle)
		{
			// no need to adjust v[0] because it should already be correct.
			if (PRIM->FST)
			{
				v[1].U = v[m_index.buff[m_index.tail - 1]].U;
				v[1].V = v[m_index.buff[m_index.tail - 1]].V;
			}
			else
			{
				v[1].ST = v[m_index.buff[m_index.tail - 1]].ST;
			}
		}
		else
		{
			if (PRIM->FST)
			{
				v[0].U = fpr.x;
				v[0].V = fpr.y;
				v[1].U = fpr.z;
				v[1].V = fpr.w;
			}
			else
			{
				const float th = static_cast<float>(1 << m_cached_ctx.TEX0.TH);
				const GSVector4 st = GSVector4(r) / GSVector4(GSVector2(tw, th)).xyxy();
				GSVector4::storel(&v[0].ST.S, st);
				GSVector4::storeh(&v[1].ST.S, st);
			}
		}
		m_r = r;
		m_vertex.head = m_vertex.tail = m_vertex.next = 2;
		m_index.tail = 2;
		return;
	}

	bool half_right_uv = !m_copy_16bit_to_target_shuffle && !m_same_group_texture_shuffle;
	bool half_bottom_uv = !m_copy_16bit_to_target_shuffle && !m_same_group_texture_shuffle;

	{
		// Different source (maybe?)
		// If a game does the texture and frame doubling differently, they can burn in hell.
		if (!m_copy_16bit_to_target_shuffle && m_cached_ctx.TEX0.TBP0 != m_cached_ctx.FRAME.Block())
		{
			// No super source of truth here, since the width can get batted around, the valid is probably our best bet.
			// Dogs will reuse the Z in a different size format for a completely unrelated draw with an FBW of 2, then go back to using it in full width
			const bool size_is_wrong = tex->m_target ? (static_cast<int>(tex->m_from_target_TEX0.TBW * 64) < tex->m_from_target->m_valid.z / 2) : false;
			const u32 draw_page_width = std::max(static_cast<int>(m_vt.m_max.p.x + (!(process_ba & SHUFFLE_WRITE) ? 8.9f : 0.9f)) / 64, 1);
			const bool single_direction_doubled = (m_vt.m_max.p.y > rt->m_valid.w) != (m_vt.m_max.p.x > rt->m_valid.z) || (IsSinglePageDraw() && m_r.height() > 32);

			if (size_is_wrong || (rt && ((rt->m_TEX0.TBW % draw_page_width) == 0 || single_direction_doubled)))
			{
				unsigned int max_tex_draw_width = std::min(static_cast<int>(floor(m_vt.m_max.t.x + (!(process_ba & SHUFFLE_READ) ? 8.9f : 0.9f))), 1 << m_cached_ctx.TEX0.TW);
				const unsigned int clamp_minu = m_context->CLAMP.MINU;
				const unsigned int clamp_maxu = m_context->CLAMP.MAXU;

				switch (m_context->CLAMP.WMS)
				{
					case CLAMP_REGION_CLAMP:
						max_tex_draw_width = std::min(max_tex_draw_width, clamp_maxu);
						break;
					case CLAMP_REGION_REPEAT:
						max_tex_draw_width = std::min(max_tex_draw_width, (clamp_maxu | clamp_minu));
						break;
					default:
						break;
				}

				const int width_diff = static_cast<int>(m_env.CTXT[m_env.PRIM.CTXT].TEX0.TBW) - static_cast<int>((m_cached_ctx.FRAME.FBW + 1) >> 1);
				// We can check the future for a clue as this can be more accurate, be careful of different draws like channel shuffles or single page draws.
				if (m_env.PRIM.TME && m_env.CTXT[m_env.PRIM.CTXT].TEX0.TBP0 == m_cached_ctx.FRAME.Block() && GSLocalMemory::m_psm[m_env.CTXT[m_env.PRIM.CTXT].TEX0.PSM].bpp == 32 && width_diff >= 0)
				{
					// width_diff will be zero is both are BW == 1, so be careful of that.
					const bool same_width = width_diff > 0 || (m_cached_ctx.FRAME.FBW == 1 && width_diff == 0);
					// Draw is double width and the draw is twice the width of the next draws texture.
					if ((!same_width && max_tex_draw_width >= (m_cached_ctx.FRAME.FBW * 64)) || (single_direction_doubled && (m_vt.m_max.p.x >= (rt->m_valid.z * 2))))
					{
						half_bottom_uv = false;
						half_bottom_vert = false;
					}
					else
					{
						half_right_uv = false;
						half_right_vert = false;
					}
				}
				else
				{
					const int tex_width = tex->m_target ? std::min(tex->m_from_target->m_valid.z, size_is_wrong ? tex->m_from_target->m_valid.z : static_cast<int>(tex->m_from_target_TEX0.TBW * 64)) : max_tex_draw_width;
					const int tex_tbw = tex->m_target ? tex->m_from_target_TEX0.TBW : tex->m_TEX0.TBW;

					if (static_cast<int>(m_cached_ctx.TEX0.TBW * 64) >= (tex_width * 2) && tex_tbw != m_cached_ctx.TEX0.TBW)
					{
						half_bottom_uv = false;
						half_bottom_vert = false;
					}
					else
					{
						half_right_uv = false;
						half_right_vert = false;
					}
				}
			}
			else
			{
				half_bottom_uv = false;
				half_bottom_vert = false;
				half_right_uv = false;
				half_right_vert = false;
			}
		}
		else
		{
			if (((m_r.width() + 8) & ~(GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].pgs.x - 1)) != GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].pgs.x && (floor(m_vt.m_max.p.y) <= rt->m_valid.w) && ((floor(m_vt.m_max.p.x) > (m_cached_ctx.FRAME.FBW * 64)) || (rt->m_TEX0.TBW < m_cached_ctx.FRAME.FBW)))
			{
				half_bottom_vert = false;
				half_bottom_uv = false;
			}
			else
			{
				half_right_vert = false;
				half_right_uv = false;
			}
		}
	}

	// If necessary, cull sprites that only do single column pixels reversal.
	// Only known game that requires this path is Colin McRae Rally 2005.
	if (recursive_draw && sprites && rev_pixels)
	{
		u32 ri, wi; // read/write index.
		for (wi = ri = 0; ri < count; ri += 2)
		{
			if (!Is8PixelReverseSprite(v[ri], v[ri + 1]))
			{
				if (wi != ri)
				{
					v[wi] = v[ri];
					v[wi + 1] = v[ri + 1];
				}
				wi += 2;
			}
		}
		if (wi != count)
		{
			count = m_vertex.head = m_vertex.tail = m_vertex.next = wi;
			m_index.tail = wi;
		}
	}

	if (PRIM->FST)
	{
		GL_INS("HW: First vertex is  P: %d => %d    T: %d => %d", v[0].XYZ.X, v[1].XYZ.X, v[0].U, v[1].U);
		const int reversed_pos = (v[0].XYZ.X > v[1].XYZ.X) ? 1 : 0;
		const int reversed_U = (v[0].U > v[1].U) ? 1 : 0;
		for (u32 i = 0; i < count; i += 2)
		{
			if (rev_pixels)
				std::swap(v[i].U, v[i + 1].U);
			if (!full_width)
			{
				if (process_ba & SHUFFLE_WRITE)
					v[i + reversed_pos].XYZ.X -= 128u;
				else
					v[i + 1 - reversed_pos].XYZ.X += 128u;

				if (process_ba & SHUFFLE_READ)
					v[i + reversed_U].U -= 128u;
				else
					v[i + 1 - reversed_U].U += 128u;
			}
			else if (!width_multiple_16)
			{
				// In this case the sprite does not span an exact columns boundary
				// probably because some of the copied channels are not being used/discarded.
				// Just align the range to the nearest column boundary and copy all
				// channels regardless.
				if ((((v[i + reversed_pos].XYZ.X - o.OFX) + 8) >> 4) & 0x8)
					v[i + reversed_pos].XYZ.X -= 128u;
				if ((((v[i + reversed_pos].XYZ.X - o.OFX) + 8) >> 4) & 0x8)
					v[i + 1 - reversed_pos].XYZ.X += 128u;
				if (v[i + reversed_U].U & 128)
					v[i + reversed_U].U -= 128u;
				if (v[i + 1 - reversed_U].U & 128)
					v[i + 1 - reversed_U].U += 128u;
			}
			else
			{
				if (((pos + 8) >> 4) & 0x8)
				{
					v[i + reversed_pos].XYZ.X -= 128u;
					v[i + 1 - reversed_pos].XYZ.X -= 128u;
				}
				// Needed for when there's no barriers.
				if (v[i + reversed_U].U & 128)
				{
					v[i + reversed_U].U -= 128u;
					v[i + 1 - reversed_U].U -= 128u;
				}
			}

			if (half_bottom_vert)
			{
				// Height is too big (2x).
				const int tex_offset = v[i].V & 0xF;
				const GSVector4i offset(o.OFY, tex_offset, o.OFY, tex_offset);

				GSVector4i tmp(v[i].XYZ.Y, v[i].V, v[i + 1].XYZ.Y, v[i + 1].V);
				tmp = GSVector4i(tmp - offset).srl32<1>() + offset;

				v[i].XYZ.Y = static_cast<u16>(tmp.x);
				v[i + 1].XYZ.Y = static_cast<u16>(tmp.z);

				if (half_bottom_uv)
				{
					v[i].V = static_cast<u16>(tmp.y);
					v[i + 1].V = static_cast<u16>(tmp.w);
				}
			}
			else if (half_right_vert)
			{
				// Width is too big (2x).
				const int tex_offset = v[i].U & 0xF;
				const GSVector4i offset(o.OFX, tex_offset, o.OFX, tex_offset);

				GSVector4i tmp(v[i].XYZ.X, v[i].U, v[i + 1].XYZ.X, v[i + 1].U);
				tmp = GSVector4i(tmp - offset).srl32<1>() + offset;

				v[i].XYZ.X = static_cast<u16>(tmp.x);
				v[i + 1].XYZ.X = static_cast<u16>(tmp.z);

				if (half_right_uv)
				{
					v[i].U = static_cast<u16>(tmp.y);
					v[i + 1].U = static_cast<u16>(tmp.w);
				}
			}
		}
	}
	else
	{
		const float offset_8pix = 8.0f / tw;
		GL_INS("HW: First vertex is  P: %d => %d    T: %f => %f (offset %f)", v[0].XYZ.X, v[1].XYZ.X, v[0].ST.S, v[1].ST.S, offset_8pix);
		const int reversed_pos = (v[0].XYZ.X > v[1].XYZ.X) ? 1 : 0;
		const int reversed_S = (v[0].ST.S > v[1].ST.S) ? 1 : 0;

		for (u32 i = 0; i < count; i += 2)
		{
			if (rev_pixels)
				std::swap(v[i].ST.S, v[i + 1].ST.S);
			if (!full_width)
			{
				if (process_ba & SHUFFLE_WRITE)
					v[i + reversed_pos].XYZ.X -= 128u;
				else
					v[i + 1 - reversed_pos].XYZ.X += 128u;

				if (process_ba & SHUFFLE_READ)
					v[i + reversed_S].ST.S -= offset_8pix;
				else
					v[i + 1 - reversed_S].ST.S += offset_8pix;
			}
			else
			{
				if (static_cast<int>(v[i + reversed_S].ST.S * tw) & 8)
				{
					v[i + reversed_S].ST.S -= offset_8pix;
					v[i + 1 - reversed_S].ST.S -= offset_8pix;
				}
			}

			if (half_bottom_vert)
			{
				// Height is too big (2x).
				const GSVector4i offset(o.OFY, o.OFY);

				GSVector4i tmp(v[i].XYZ.Y, v[i + 1].XYZ.Y);
				tmp = GSVector4i(tmp - offset).srl32<1>() + offset;

				//fprintf(stderr, "HW: Before %d, After %d\n", v[i + 1].XYZ.Y, tmp.y);
				v[i].XYZ.Y = static_cast<u16>(tmp.x);
				v[i + 1].XYZ.Y = static_cast<u16>(tmp.y);

				if (half_bottom_uv)
				{
					v[i].ST.T /= 2.0f;
					v[i + 1].ST.T /= 2.0f;
				}
			}
			else if (half_right_vert)
			{
				// Width is too big (2x).
				const GSVector4i offset(o.OFX, o.OFX);

				GSVector4i tmp(v[i].XYZ.X, v[i + 1].XYZ.X);
				tmp = GSVector4i(tmp - offset).srl32<1>() + offset;

				//fprintf(stderr, "HW: Before %d, After %d\n", v[i + 1].XYZ.Y, tmp.y);
				v[i].XYZ.X = static_cast<u16>(tmp.x);
				v[i + 1].XYZ.X = static_cast<u16>(tmp.y);

				if (half_right_uv)
				{
					v[i].ST.S /= 2.0f;
					v[i + 1].ST.S /= 2.0f;
				}
			}
		}
	}

	if (m_index.tail == 0)
	{
		GL_INS("HW: ConvertSpriteTextureShuffle: Culled all vertices; exiting.");
		return;
	}

	if (!full_width)
	{
		// Update vertex trace too. Avoid issue to compute bounding box
		if (process_ba & SHUFFLE_WRITE)
			m_vt.m_min.p.x -= 8.0f;
		else
			m_vt.m_max.p.x += 8.0f;

		if (!m_same_group_texture_shuffle)
		{
			if (process_ba & SHUFFLE_READ)
				m_vt.m_min.t.x -= 8.0f;
			else
				m_vt.m_max.t.x += 8.0f;
		}
	}
	else
	{
		if (fmod(std::floor(m_vt.m_min.p.x), 64.0f) == 8.0f)
		{
			m_vt.m_min.p.x -= 8.0f;
			m_vt.m_max.p.x -= 8.0f;
		}
	}

	if (half_right_vert)
	{
		m_vt.m_min.p.x /= 2.0f;
		m_vt.m_max.p.x = floor(m_vt.m_max.p.x + 1.9f) / 2.0f;
	}

	if (half_bottom_vert)
	{
		m_vt.m_min.p.y /= 2.0f;
		m_vt.m_max.p.y = floor(m_vt.m_max.p.y + 1.9f) / 2.0f;
	}

	if (m_context->scissor.in.x & 8)
	{
		m_context->scissor.in.x &= ~0xf; //m_vt.m_min.p.x;

		if (half_right_vert)
			m_context->scissor.in.x /= 2;
	}
	if (m_context->scissor.in.z & 8)
	{
		m_context->scissor.in.z += 8; //m_vt.m_min.p.x;

		if (half_right_vert)
			m_context->scissor.in.z /= 2;
	}
	if (half_bottom_vert)
	{
		m_context->scissor.in.y /= 2;
		m_context->scissor.in.w /= 2;
	}

	// Only do this is the source is being interpreted as 16bit
	if (half_bottom_uv)
	{
		m_vt.m_min.t.y /= 2.0f;
		m_vt.m_max.t.y = (m_vt.m_max.t.y + 1.9f) / 2.0f;
	}

	if (half_right_uv)
	{
		m_vt.m_min.t.x /= 2.0f;
		m_vt.m_max.t.x = (m_vt.m_max.t.x + 1.9f) / 2.0f;
	}

	// Special case used in Call of Duty - World at War where it doubles the height and halves the width, but the height is double doubled.
	// Check the height of the original texture, if it's half of the draw height, then make it wide instead.
	if (half_bottom_uv && tex->m_from_target && m_cached_ctx.TEX0.TBW == m_cached_ctx.FRAME.FBW &&
		tex->m_from_target->m_TEX0.TBW == (m_cached_ctx.TEX0.TBW * 2) && (m_cached_ctx.TEX0.TBW * 64) == floor(m_vt.m_max.t.x) && m_vt.m_max.t.y > tex->m_from_target->m_valid.w)
	{
		m_r.z *= 2;
		m_r.w /= 2;

		m_vt.m_max.t.y /= 2;
		m_vt.m_max.t.x *= 2;
		m_vt.m_max.p.y /= 2;
		m_vt.m_max.p.x *= 2;
		m_context->scissor.in.w /= 2;
		m_context->scissor.in.z *= 2;

		v[1].XYZ.X = ((v[m_index.buff[m_index.tail - 1]].XYZ.X - m_context->XYOFFSET.OFX) * 2) + m_context->XYOFFSET.OFX;
		v[1].XYZ.Y = ((v[m_index.buff[m_index.tail - 1]].XYZ.Y - m_context->XYOFFSET.OFY) / 2) + m_context->XYOFFSET.OFY;

		v[1].U = v[m_index.buff[m_index.tail - 1]].U * 2;
		v[1].V = v[m_index.buff[m_index.tail - 1]].V / 2;

		v[1].ST.S = v[m_index.buff[m_index.tail - 1]].ST.S * 2;
		v[1].ST.T = v[m_index.buff[m_index.tail - 1]].ST.T / 2;

		m_vertex.head = m_vertex.tail = m_vertex.next = 2;
		m_index.tail = 2;

		m_cached_ctx.TEX0.TBW *= 2;
		m_cached_ctx.FRAME.FBW *= 2;
		GL_CACHE("HW: Half width/double height shuffle detected, width changed to %d", m_cached_ctx.FRAME.FBW);
	}
}

GSVector4 GSRendererHW::RealignTargetTextureCoordinate(const GSTextureCache::Source* tex)
{
	if (GSConfig.UserHacks_HalfPixelOffset <= GSHalfPixelOffset::Normal ||
		GSConfig.UserHacks_HalfPixelOffset >= GSHalfPixelOffset::Native ||
		GetUpscaleMultiplier() == 1.0f || m_downscale_source || tex->GetScale() == 1.0f)
	{
		return GSVector4(0.0f);
	}

	const GSVertex* v = &m_vertex.buff[0];
	const float scale = tex->GetScale();
	const bool linear = m_vt.IsRealLinear();
	const int t_position = v[0].U;
	GSVector4 half_offset(0.0f);

	// FIXME Let's start with something wrong same mess on X and Y
	// FIXME Maybe it will be enough to check linear

	if (PRIM->FST)
	{
		if (GSConfig.UserHacks_HalfPixelOffset == GSHalfPixelOffset::SpecialAggressive)
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
				half_offset.x = 8 - 8 / scale;
				half_offset.y = 8 - 8 / scale;
			}
			else if (linear && t_position == 16)
			{
				half_offset.x = 16 - 16 / scale;
				half_offset.y = 16 - 16 / scale;
			}
			else if (m_vt.m_min.p.x == -0.5f)
			{
				half_offset.x = 8;
				half_offset.y = 8;
			}
		}

		GL_INS("HW: offset detected %f,%f t_pos %d (linear %d, scale %f)",
			half_offset.x, half_offset.y, t_position, linear, scale);
	}
	else if (m_vt.m_eq.q)
	{
		const float tw = static_cast<float>(1 << m_cached_ctx.TEX0.TW);
		const float th = static_cast<float>(1 << m_cached_ctx.TEX0.TH);
		const float q = v[0].RGBAQ.Q;

		// Tales of Abyss
		half_offset.x = 0.5f * q / tw;
		half_offset.y = 0.5f * q / th;

		GL_INS("HW: ST offset detected %f,%f (linear %d, scale %f)",
			half_offset.x, half_offset.y, linear, scale);
	}

	return half_offset;
}

GSVector4i GSRendererHW::ComputeBoundingBox(const GSVector2i& rtsize, float rtscale)
{
	const GSVector4 offset = GSVector4(-1.0f, 1.0f); // Round value
	const GSVector4 box = m_vt.m_min.p.upld(m_vt.m_max.p) + offset.xxyy();
	return GSVector4i(box * GSVector4(rtscale)).rintersect(GSVector4i(0, 0, rtsize.x, rtsize.y));
}

void GSRendererHW::MergeSprite(GSTextureCache::Source* tex)
{
	// Upscaling hack to avoid various line/grid issues
	if (GSConfig.UserHacks_MergePPSprite && CanUpscale() && tex && tex->m_target && (m_vt.m_primclass == GS_SPRITE_CLASS))
	{
		if (PRIM->FST && GSLocalMemory::m_psm[tex->m_TEX0.PSM].fmt < 2 && ((m_vt.m_eq.value & 0xCFFFF) == 0xCFFFF))
		{
			// Ideally the hack ought to be enabled in a true paving mode only. I don't know how to do it accurately
			// neither in a fast way. So instead let's just take the hypothesis that all sprites must have the same
			// size.
			// Tested on Tekken 5.
			const GSVertex* v = &m_vertex.buff[0];
			bool is_paving = true;
			bool is_paving_h = true;
			bool is_paving_v = true;
			// SSE optimization: shuffle m[1] to have (4*32 bits) X, Y, U, V
			const int first_dpX = v[1].XYZ.X - v[0].XYZ.X;
			const int first_dpU = v[1].U - v[0].U;
			const int first_dpY = v[1].XYZ.Y - v[0].XYZ.Y;
			const int first_dpV = v[1].V - v[0].V;
			for (u32 i = 0; i < m_vertex.next; i += 2)
			{
				const int dpX = v[i + 1].XYZ.X - v[i].XYZ.X;
				const int dpU = v[i + 1].U - v[i].U;

				const int dpY = v[i + 1].XYZ.Y - v[i].XYZ.Y;
				const int dpV = v[i + 1].V - v[i].V;
				if (dpX != first_dpX || dpU != first_dpU)
				{
					is_paving_h = false;
				}

				if (dpY != first_dpY || dpV != first_dpV)
				{
					is_paving_v = false;
				}

				if (!is_paving_h && !is_paving_v)
					break;
			}
			is_paving = is_paving_h || is_paving_v;
#if 0
			const GSVector4 delta_p = m_vt.m_max.p - m_vt.m_min.p;
			const GSVector4 delta_t = m_vt.m_max.t - m_vt.m_min.t;
			const bool is_blit = PrimitiveOverlap() == PRIM_OVERLAP_NO;
			GL_INS("HW: PP SAMPLER: Dp %f %f Dt %f %f. Is blit %d, is paving %d, count %d", delta_p.x, delta_p.y, delta_t.x, delta_t.y, is_blit, is_paving, m_vertex.tail);
#endif

			if (is_paving)
			{
				// Replace all sprite with a single fullscreen sprite.
				u32 unique_verts = 2;
				GSVertex* s = &m_vertex.buff[0];
				if (is_paving_h)
				{
					s[0].XYZ.X = static_cast<u16>((16.0f * m_vt.m_min.p.x) + m_context->XYOFFSET.OFX);
					s[1].XYZ.X = static_cast<u16>((16.0f * m_vt.m_max.p.x) + m_context->XYOFFSET.OFX);

					s[0].U = static_cast<u16>(16.0f * m_vt.m_min.t.x);
					s[1].U = static_cast<u16>(16.0f * m_vt.m_max.t.x);
				}
				else
				{
					for (u32 i = 2; i < (m_vertex.tail & ~1); i++)
					{
						bool unique_found = false;

						for (u32 j = i & 1; j < unique_verts; i += 2)
						{
							if (s[i].XYZ.X != s[j].XYZ.X)
							{
								unique_found = true;
								break;
							}
						}
						if (unique_found)
						{
							unique_verts += 2;
							s[unique_verts - 2].XYZ.X = s[i & ~1].XYZ.X;
							s[unique_verts - 1].XYZ.X = s[i | 1].XYZ.X;
							s[unique_verts - 2].U = s[i & ~1].U;
							s[unique_verts - 1].U = s[i | 1].U;

							s[unique_verts - 2].XYZ.Y = static_cast<u16>((16.0f * m_vt.m_min.p.y) + m_context->XYOFFSET.OFY);
							s[unique_verts - 1].XYZ.Y = static_cast<u16>((16.0f * m_vt.m_max.p.y) + m_context->XYOFFSET.OFY);

							s[unique_verts - 2].V = static_cast<u16>(16.0f * m_vt.m_min.t.y);
							s[unique_verts - 1].V = static_cast<u16>(16.0f * m_vt.m_max.t.y);

							i |= 1;
						}
					}
				}

				if (is_paving_v)
				{
					s[0].XYZ.Y = static_cast<u16>((16.0f * m_vt.m_min.p.y) + m_context->XYOFFSET.OFY);
					s[1].XYZ.Y = static_cast<u16>((16.0f * m_vt.m_max.p.y) + m_context->XYOFFSET.OFY);

					s[0].V = static_cast<u16>(16.0f * m_vt.m_min.t.y);
					s[1].V = static_cast<u16>(16.0f * m_vt.m_max.t.y);
				}
				else
				{
					for (u32 i = 2; i < (m_vertex.tail & ~1); i++)
					{
						bool unique_found = false;

						for (u32 j = i & 1; j < unique_verts; i+=2)
						{
							if (s[i].XYZ.Y != s[j].XYZ.Y)
							{
								unique_found = true;
								break;
							}
						}
						if (unique_found)
						{
							unique_verts += 2;
							s[unique_verts - 2].XYZ.Y = s[i & ~1].XYZ.Y;
							s[unique_verts - 1].XYZ.Y = s[i | 1].XYZ.Y;
							s[unique_verts - 2].V = s[i & ~1].V;
							s[unique_verts - 1].V = s[i | 1].V;

							s[unique_verts - 2].XYZ.X = static_cast<u16>((16.0f * m_vt.m_min.p.x) + m_context->XYOFFSET.OFX);
							s[unique_verts - 1].XYZ.X = static_cast<u16>((16.0f * m_vt.m_max.p.x) + m_context->XYOFFSET.OFX);

							s[unique_verts - 2].U = static_cast<u16>(16.0f * m_vt.m_min.t.x);
							s[unique_verts - 1].U = static_cast<u16>(16.0f * m_vt.m_max.t.x);

							i |= 1;
						}
					}
				}

				m_vertex.head = m_vertex.tail = m_vertex.next = unique_verts;
				m_index.tail = unique_verts;
			}
		}
	}
}

float GSRendererHW::GetTextureScaleFactor()
{
	return GetUpscaleMultiplier();
}

GSVector2i GSRendererHW::GetValidSize(const GSTextureCache::Source* tex, const bool is_shuffle)
{
	// Don't blindly expand out to the scissor size if we're not drawing to it.
	// e.g. Burnout 3, God of War II, etc.
	int height = std::min<int>(m_context->scissor.in.w, m_r.w);

	const GSLocalMemory::psm_t& frame_psm = GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM];
	// We can check if the next draw is doing the same from the next page, and assume it's a per line clear.
	// Battlefield 2 does this.
	const int pages = ((GSLocalMemory::GetEndBlockAddress(m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBW, m_cached_ctx.FRAME.PSM, m_r) + 1) - m_cached_ctx.FRAME.Block()) >> 5;
	if (m_cached_ctx.FRAME.FBW > 1 && m_r.height() == frame_psm.pgs.y && (pages % m_cached_ctx.FRAME.FBW) == 0 && m_env.CTXT[m_backed_up_ctx].FRAME.FBP == (m_cached_ctx.FRAME.FBP + pages) &&
		!IsPossibleChannelShuffle() && NextDrawMatchesShuffle())
		height = std::max<int>(m_context->scissor.in.w, height);

	// If the draw is less than a page high, FBW=0 is the same as FBW=1.
	int width = std::min(std::max<int>(m_cached_ctx.FRAME.FBW, 1) * 64, m_context->scissor.in.z);
	if (m_cached_ctx.FRAME.FBW == 0 && m_r.w > frame_psm.pgs.y)
	{
		GL_INS("HW: FBW=0 when drawing more than 1 page in height (PSM %s, PGS %dx%d).", GSUtil::GetPSMName(m_cached_ctx.FRAME.PSM),
			frame_psm.pgs.x, frame_psm.pgs.y);
	}

	// If it's a channel shuffle, it'll likely be just a single page, so assume full screen.
	if (m_channel_shuffle || (tex && IsPageCopy()))
	{
		const int page_x = frame_psm.pgs.x - 1;
		const int page_y = frame_psm.pgs.y - 1;
		pxAssert(tex);

		// Round up the page as channel shuffles are generally done in pages at a time
		// Keep in mind the source might be an 8bit texture
		int src_width = tex->m_from_target ? tex->m_from_target->m_valid.width() : tex->GetUnscaledWidth();
		int src_height = tex->m_from_target ? tex->m_from_target->m_valid.height() : tex->GetUnscaledHeight();

		if (!tex->m_from_target && GSLocalMemory::m_psm[tex->m_TEX0.PSM].bpp == 8)
		{
			src_width >>= 1;
			src_height >>= 1;
		}

		width = (std::max(src_width, width) + page_x) & ~page_x;
		height = (std::max(src_height, height) + page_y) & ~page_y;
	}

	// Align to page size. Since FRAME/Z has to always start on a page boundary, in theory no two should overlap.
	width = Common::AlignUpPow2(width, frame_psm.pgs.x);
	height = Common::AlignUpPow2(height, frame_psm.pgs.y);

	// Early detection of texture shuffles. These double the input height because they're interpreting 64x32 C32 pages as 64x64 C16.
	// Why? Well, we don't want to be doubling the heights of targets, but also we don't want to align C32 targets to 64 instead of 32.
	// Yumeria's text breaks, and GOW goes to 512x448 instead of 512x416 if we don't.
	const bool possible_texture_shuffle = tex && m_vt.m_primclass == GS_SPRITE_CLASS && frame_psm.bpp == 16 &&
			GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].bpp == 16 &&
			(is_shuffle || (tex->m_32_bits_fmt ||
				(m_cached_ctx.TEX0.TBP0 != m_cached_ctx.FRAME.Block() && IsOpaque() && !(m_context->TEX1.MMIN & 1) &&
					m_cached_ctx.FRAME.FBMSK && g_texture_cache->Has32BitTarget(m_cached_ctx.FRAME.Block()))));
	if (possible_texture_shuffle)
	{
		const u32 tex_width_pgs = (tex->m_target ? tex->m_from_target_TEX0.TBW : tex->m_TEX0.TBW);
		const u32 half_draw_width_pgs = ((width + (frame_psm.pgs.x - 1)) / frame_psm.pgs.x) >> 1;

		// Games such as Midnight Club 3 draw headlights with a texture shuffle, but instead of doubling the height, they doubled the width.
		if (tex_width_pgs == half_draw_width_pgs)
		{
			GL_CACHE("HW: Halving width due to texture shuffle with double width, %dx%d -> %dx%d", width, height, width / 2, height);
			const int src_width = tex ? (tex->m_from_target ? tex->m_from_target->m_valid.width() : tex->GetUnscaledWidth()) : (width / 2);
			width = std::min(width / 2, src_width);
		}
		else
		{
			GL_CACHE("HW: Halving height due to texture shuffle, %dx%d -> %dx%d", width, height, width, height / 2);
			const int src_height = tex ? (tex->m_from_target ? tex->m_from_target->m_valid.height() : tex->GetUnscaledHeight()) : (height / 2);
			height = std::min(height / 2, src_height);
		}
	}

	// Make sure sizes are within max limit of 2048,
	// this shouldn't happen but if it does it needs to be addressed,
	// clamp the size so at least it doesn't cause a crash.
	constexpr int valid_max_size = 2047;
	if ((width > valid_max_size) || (height > valid_max_size))
	{
		DevCon.Warning("Warning: GetValidSize out of bounds, X:%d Y:%d", width, height);
		width = std::min(width, valid_max_size);
		height = std::min(height, valid_max_size);
	}

	return GSVector2i(width, height);
}

GSVector2i GSRendererHW::GetTargetSize(const GSTextureCache::Source* tex, const bool can_expand, const bool is_shuffle)
{
	const GSVector2i valid_size = GetValidSize(tex, is_shuffle);

	return g_texture_cache->GetTargetSize(m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBW, m_cached_ctx.FRAME.PSM, valid_size.x, valid_size.y, can_expand);
}

bool GSRendererHW::NextDrawColClip() const
{
	const int get_next_ctx = (m_state_flush_reason == CONTEXTCHANGE) ? m_env.PRIM.CTXT : m_backed_up_ctx;
	const GSDrawingContext& next_ctx = m_env.CTXT[get_next_ctx];

	// If it wasn't a context change we can't guarantee the next draw is going to be set up
	if (m_state_flush_reason != GSFlushReason::CONTEXTCHANGE || m_env.COLCLAMP.CLAMP != 0 || m_env.PRIM.ABE == 0 ||
		(m_context->FRAME.U64 ^ next_ctx.FRAME.U64) != 0 || (m_env.PRIM.TME && next_ctx.TEX0.TBP0 == m_context->FRAME.Block()))
	{
		return false;
	}

	return true;
}

bool GSRendererHW::IsPossibleChannelShuffle() const
{
	if (!PRIM->TME || m_cached_ctx.TEX0.PSM != PSMT8 || // 8-bit texture draw
		m_vt.m_primclass != GS_SPRITE_CLASS || // draw_sprite_tex
		(m_vertex.tail <= 2 && (((m_vt.m_max.p - m_vt.m_min.p) <= GSVector4(8.0f)).mask() & 0x3) == 0x3)) // Powerdrome does a tiny shuffle on a couple of pixels, can't reliably translate this.
	{
		return false;
	}

	const int mask = (((m_vt.m_max.p - m_vt.m_min.p) <= GSVector4(64.0f)).mask() & 0x3);
	if (mask == 0x3) // single_page
	{
		const GSVertex* v = &m_vertex.buff[0];

		const int draw_width = std::abs(v[1].XYZ.X - v[0].XYZ.X) >> 4;
		const int draw_height = std::abs(v[1].XYZ.Y - v[0].XYZ.Y) >> 4;

		const bool mask_clamp = (m_cached_ctx.CLAMP.WMS | m_cached_ctx.CLAMP.WMT) & 0x2;

		const bool draw_match = (draw_height == 2) || (draw_width == 8);

		if (draw_match || mask_clamp)
			return true;
		else
			return false;
	}
	else if (mask != 0x1) // Not a single page in width.
		return false;

	// WRC 4 does channel shuffles in vertical strips. So check for page alignment.
	// Texture TBW should also be twice the framebuffer FBW, because the page is twice as wide.
	if (m_cached_ctx.TEX0.TBW == (m_cached_ctx.FRAME.FBW * 2) &&
		GSLocalMemory::IsPageAligned(m_cached_ctx.FRAME.PSM, GSVector4i(m_vt.m_min.p.upld(m_vt.m_max.p))))
	{
		const GSVertex* v = &m_vertex.buff[0];

		const int draw_width = std::abs(v[1].XYZ.X - v[0].XYZ.X) >> 4;
		const int draw_height = std::abs(v[1].XYZ.Y - v[0].XYZ.Y) >> 4;

		const bool mask_clamp = (m_cached_ctx.CLAMP.WMS | m_cached_ctx.CLAMP.WMT) & 0x2;
		const bool draw_match = (draw_height == 2) || (draw_width == 8);

		if (draw_match || mask_clamp)
			return true;
		else
			return false;
	}

	return false;
}

bool GSRendererHW::IsPageCopy() const
{
	if (!PRIM->TME)
		return false;

	const int get_next_ctx = (m_state_flush_reason == CONTEXTCHANGE) ? m_env.PRIM.CTXT : m_backed_up_ctx;
	const GSDrawingContext& next_ctx = m_env.CTXT[get_next_ctx];

	if (next_ctx.TEX0.TBP0 != (m_cached_ctx.TEX0.TBP0 + 0x20))
		return false;

	if (next_ctx.FRAME.FBP != (m_cached_ctx.FRAME.FBP + 0x1))
		return false;

	if (!NextDrawMatchesShuffle())
		return false;

	return true;
}

bool GSRendererHW::NextDrawMatchesShuffle() const
{
	// Make sure nothing unexpected has changed.
	// Twinsanity seems to screw with ZBUF here despite it being irrelevant.
	const int get_next_ctx = (m_state_flush_reason == CONTEXTCHANGE) ? m_env.PRIM.CTXT : m_backed_up_ctx;
	const GSDrawingContext& next_ctx = m_env.CTXT[get_next_ctx];
	if (((m_context->TEX0.U64 ^ next_ctx.TEX0.U64) & (~0x3FFF)) != 0 ||
		m_context->TEX1.U64 != next_ctx.TEX1.U64 ||
		m_context->CLAMP.U64 != next_ctx.CLAMP.U64 ||
		m_context->TEST.U64 != next_ctx.TEST.U64 ||
		((m_context->FRAME.U64 ^ next_ctx.FRAME.U64) & (~0x1FF)) != 0 ||
		m_context->ZBUF.ZMSK != next_ctx.ZBUF.ZMSK)
	{
		return false;
	}

	return true;
}

bool GSRendererHW::IsSplitTextureShuffle(GIFRegTEX0& rt_TEX0, GSVector4i& valid_area)
{
	// For this to work, we're peeking into the next draw, therefore we need dirty registers.
	if (m_dirty_gs_regs == 0)
		return false;

	if (!NextDrawMatchesShuffle())
		return false;

	// Different channel being shuffled, so needs to be handled separately (misdetection in 50 Cent)
	if (m_vertex.buff[m_index.buff[0]].U != m_v.U)
		return false;

	// Check that both the position and texture coordinates are page aligned, so we can work in pages instead of coordinates.
	// For texture shuffles, the U will be offset by 8.

	const GSVector4i pos_rc = GSVector4i(m_vt.m_min.p.upld(m_vt.m_max.p + GSVector4::cxpr(0.5f)));
	const GSVector4i tex_rc = GSVector4i(m_vt.m_min.t.upld(m_vt.m_max.t));

	// Width/height should match.
	if (std::abs(pos_rc.width() - tex_rc.width()) > 8 || pos_rc.height() != tex_rc.height())
		return false;

	// X might be offset by up to -8/+8, but either the position or UV should be aligned.
	GSVector4i aligned_rc = pos_rc.min_i32(tex_rc).blend32<12>(pos_rc.max_i32(tex_rc));

	const GSLocalMemory::psm_t& frame_psm = GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM];
	const GSDrawingContext& next_ctx = m_env.CTXT[m_backed_up_ctx];

	// Also don't allow pag sized shuffles when we have RT inside RT, we handle this manually. See Peter Jackson's - King Kong.
	const bool in_rt_per_page = GSConfig.UserHacks_TextureInsideRt >= GSTextureInRtMode::InsideTargets && (pos_rc.width() <= frame_psm.pgs.x && pos_rc.height() <= frame_psm.pgs.y);

	// Y should be page aligned. X should be too, but if it's doing a copy with a shuffle (which is kinda silly), both the
	// position and coordinates may be offset by +8. See Psi-Ops - The Mindgate Conspiracy.
	if ((aligned_rc.x & 7) != 0 || aligned_rc.x > 8 || (aligned_rc.z & 7) != 0 ||
		aligned_rc.y != 0 || (aligned_rc.w & (frame_psm.pgs.y - 1)) != 0 || in_rt_per_page)
	{
		return false;
	}

	// Matrix Path of Neo draws 512x512 instead of 512x448, then scissors to 512x448.
	aligned_rc = aligned_rc.rintersect(m_context->scissor.in);

	// We should have the same number of pages in both the position and UV.
	const u32 pages_high = static_cast<u32>(aligned_rc.height()) / frame_psm.pgs.y;
	const u32 num_pages = m_context->FRAME.FBW * pages_high;
	// Jurassic - The Hunted will do a split shuffle with a height of 512 (256) when it's supposed to be 448, so it redoes one row of the shuffle.
	const u32 rt_half = (((valid_area.height() / GSLocalMemory::m_psm[rt_TEX0.PSM].pgs.y) / 2) * rt_TEX0.TBW) + (rt_TEX0.TBP0 >> 5);
	// If this is a split texture shuffle, the next draw's FRAME/TEX0 should line up.
	// Re-add the offset we subtracted in Draw() to get the original FBP/TBP0.. this won't handle wrapping. Oh well.
	// "Potential" ones are for Jak3 which does a split shuffle on a 128x128 texture with a width of 256, writing to the lower half then offsetting 2 pages.
	const u32 expected_next_FBP = (m_cached_ctx.FRAME.FBP + m_split_texture_shuffle_pages) + num_pages;
	const u32 potential_expected_next_FBP = m_cached_ctx.FRAME.FBP + ((m_context->FRAME.FBW * 64) / aligned_rc.width());
	const u32 expected_next_TBP0 = (m_cached_ctx.TEX0.TBP0 + (m_split_texture_shuffle_pages + num_pages) * GS_BLOCKS_PER_PAGE);
	const u32 potential_expected_next_TBP0 = m_cached_ctx.TEX0.TBP0 + (GS_BLOCKS_PER_PAGE * ((m_context->TEX0.TBW * 64) / aligned_rc.width()));
	GL_CACHE("HW: IsSplitTextureShuffle: Draw covers %ux%u pages, next FRAME %x TEX %x",
		static_cast<u32>(aligned_rc.width()) / frame_psm.pgs.x, pages_high, expected_next_FBP * GS_BLOCKS_PER_PAGE,
		expected_next_TBP0);

	if (next_ctx.TEX0.TBP0 != expected_next_TBP0 && next_ctx.TEX0.TBP0 != potential_expected_next_TBP0 && next_ctx.TEX0.TBP0 != (rt_half << 5))
	{
		GL_CACHE("HW: IsSplitTextureShuffle: Mismatch on TBP0, expecting %x, got %x", expected_next_TBP0, next_ctx.TEX0.TBP0);
		return false;
	}

	// Some games don't offset the FBP.
	if (next_ctx.FRAME.FBP != expected_next_FBP && next_ctx.FRAME.FBP != m_cached_ctx.FRAME.FBP && next_ctx.FRAME.FBP != potential_expected_next_FBP && next_ctx.FRAME.FBP != rt_half)
	{
		GL_CACHE("HW: IsSplitTextureShuffle: Mismatch on FBP, expecting %x, got %x", expected_next_FBP * GS_BLOCKS_PER_PAGE,
			next_ctx.FRAME.FBP * GS_BLOCKS_PER_PAGE);
		return false;
	}

	// Great, everything lines up, so skip 'em.
	GL_CACHE("HW: IsSplitTextureShuffle: Match, buffering and skipping draw.");

	if (m_split_texture_shuffle_pages == 0)
	{
		m_split_texture_shuffle_start_FBP = m_cached_ctx.FRAME.FBP;
		m_split_texture_shuffle_start_TBP = m_cached_ctx.TEX0.TBP0;

		// If the game has changed the texture width to 1 we need to retanslate it to whatever the rt has so the final rect is correct.
		if (m_cached_ctx.FRAME.FBW == 1)
			m_split_texture_shuffle_fbw = rt_TEX0.TBW;
		else
			m_split_texture_shuffle_fbw = m_cached_ctx.FRAME.FBW;
	}

	u32 vertical_pages = pages_high;
	u32 total_pages = num_pages;

	// If the current draw is further than the half way point and the next draw is the half way point, then we can assume it's just overdrawing.
	if (next_ctx.FRAME.FBP == rt_half && num_pages > (rt_half - (rt_TEX0.TBP0 >> 5)))
	{
		vertical_pages = (valid_area.height() / GSLocalMemory::m_psm[rt_TEX0.PSM].pgs.y) / 2;
		total_pages = vertical_pages * rt_TEX0.TBW;
	}

	if ((m_split_texture_shuffle_pages % m_split_texture_shuffle_fbw) == 0)
		m_split_texture_shuffle_pages_high += vertical_pages;

	m_split_texture_shuffle_pages += total_pages;
	return true;
}

GSVector4i GSRendererHW::GetSplitTextureShuffleDrawRect() const
{
	const GSLocalMemory::psm_t& frame_psm = GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM];
	GSVector4i r = GSVector4i(m_vt.m_min.p.upld(m_vt.m_max.p + GSVector4::cxpr(0.5f))).rintersect(m_context->scissor.in);

	// Some games (e.g. Crash Twinsanity) adjust both FBP and TBP0, so the rectangle will be half the size
	// of the actual shuffle. Others leave the FBP alone, but only adjust TBP0, and offset the draw rectangle
	// to the second half of the fb. In which case, the rectangle bounds will be correct.
	if (m_context->FRAME.FBP != m_split_texture_shuffle_start_FBP)
	{
		const int pages_high = (r.height() + frame_psm.pgs.y - 1) / frame_psm.pgs.y;
		r.w = (m_split_texture_shuffle_pages_high + pages_high) * frame_psm.pgs.y;
	}

	// But we still need to page align, because of the +/- 8 offset.
	return r.insert64<0>(0).ralign<Align_Outside>(frame_psm.pgs);
}

u32 GSRendererHW::GetEffectiveTextureShuffleFbmsk() const
{
	pxAssert(m_texture_shuffle);
	const u32 m = m_cached_ctx.FRAME.FBMSK & GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].fmsk;
	const u32 fbmask = ((m >> 3) & 0x1F) | ((m >> 6) & 0x3E0) | ((m >> 9) & 0x7C00) | ((m >> 16) & 0x8000);
	const u32 rb_mask = fbmask & 0xFF;
	const u32 ga_mask = (fbmask >> 8) & 0xFF;
	const u32 eff_mask =
		((rb_mask == 0xFF && ga_mask == 0xFF) ? 0x00FFFFFFu : 0) | ((ga_mask == 0xFF) ? 0xFF000000u : 0);
	return eff_mask;
}

GSVector4i GSRendererHW::GetDrawRectForPages(u32 bw, u32 psm, u32 num_pages)
{
	const GSVector2i& pgs = GSLocalMemory::m_psm[psm].pgs;
	const GSVector2i size = GSVector2i(static_cast<int>(bw) * pgs.x, static_cast<int>(num_pages / std::max(1U, bw)) * pgs.y);
	return GSVector4i::loadh(size);
}

bool GSRendererHW::IsSinglePageDraw() const
{
	const GSVector2i& frame_pgs = GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].pgs;

	if (m_r.width() <= frame_pgs.x && m_r.height() <= frame_pgs.y)
		return true;

	return false;
}

bool GSRendererHW::TryToResolveSinglePageFramebuffer(GIFRegFRAME& FRAME, bool only_next_draw)
{
	const u32 start_bp = FRAME.Block();
	u32 new_bw = FRAME.FBW;
	u32 new_psm = FRAME.PSM;
	pxAssert(new_bw <= 1);

	if (m_backed_up_ctx >= 0)
	{
		const GSDrawingContext& next_ctx = m_env.CTXT[m_backed_up_ctx];
		if (next_ctx.FRAME.FBW != new_bw)
		{
			// Using it as a target/Z next (Superman Returns).
			if (start_bp == next_ctx.FRAME.Block())
			{
				GL_INS("HW: TryToResolveSinglePageWidth(): Next FBP is split clear, using FBW of %u", next_ctx.FRAME.FBW);
				new_bw = next_ctx.FRAME.FBW;
				new_psm = next_ctx.FRAME.PSM;
			}
			else if (start_bp == next_ctx.ZBUF.Block())
			{
				GL_INS("HW: TryToResolveSinglePageWidth(): Next ZBP is split clear, using FBW of %u", next_ctx.FRAME.FBW);
				new_bw = next_ctx.FRAME.FBW;
			}
		}

		// Might be using it as a texture next (NARC).
		if (new_bw <= 1 && next_ctx.TEX0.TBP0 == start_bp && new_bw != next_ctx.TEX0.TBW)
		{
			GL_INS("HW: TryToResolveSinglePageWidth(): Next texture is using split clear, using FBW of %u", next_ctx.TEX0.TBW);
			new_bw = next_ctx.TEX0.TBW;
			new_psm = next_ctx.TEX0.PSM;
		}
	}

	if (!only_next_draw)
	{
		// Try for an exiting target at the start BP. (Tom & Jerry)
		if (new_bw <= 1)
		{
			GSTextureCache::Target* tgt = g_texture_cache->GetTargetWithSharedBits(start_bp, new_psm);
			if (!tgt)
			{
				// Try with Z or FRAME (whichever we're not using).
				tgt = g_texture_cache->GetTargetWithSharedBits(start_bp, new_psm ^ 0x30);
			}
			if (tgt && ((start_bp + (m_split_clear_pages * GS_BLOCKS_PER_PAGE)) - 1) <= tgt->m_end_block)
			{
				GL_INS("HW: TryToResolveSinglePageWidth(): Using FBW of %u and PSM %s from existing target",
					tgt->m_TEX0.PSM, GSUtil::GetPSMName(tgt->m_TEX0.PSM));
				new_bw = tgt->m_TEX0.TBW;
				new_psm = tgt->m_TEX0.PSM;
			}
		}

		// Still bad FBW? Fall back to the resolution hack (Brave).
		if (new_bw <= 1)
		{
			// Framebuffer is likely to be read as 16bit later, so we will need to double the width if the write is 32bit.
			const bool double_width =
				GSLocalMemory::m_psm[new_psm].bpp == 32 && PCRTCDisplays.GetFramebufferBitDepth() == 16;
			const GSVector2i fb_size = PCRTCDisplays.GetFramebufferSize(-1);
			u32 width =
				std::ceil(static_cast<float>(m_split_clear_pages * GSLocalMemory::m_psm[new_psm].pgs.y) / fb_size.y) *
				64;
			width = std::max((width * (double_width ? 2 : 1)), static_cast<u32>(fb_size.x));
			new_bw = (width + 63) / 64;
			GL_INS("HW: TryToResolveSinglePageWidth(): Fallback guess target FBW of %u", new_bw);
		}
	}

	if (new_bw <= 1)
		return false;

	FRAME.FBW = new_bw;
	FRAME.PSM = new_psm;
	return true;
}

bool GSRendererHW::IsSplitClearActive() const
{
	return (m_split_clear_pages != 0);
}

bool GSRendererHW::IsStartingSplitClear()
{
	// Shouldn't have gaps.
	if (m_vt.m_eq.rgba != 0xFFFF || (!m_cached_ctx.ZBUF.ZMSK && !m_vt.m_eq.z) || m_primitive_covers_without_gaps != NoGapsType::FullCover)
		return false;

	// Limit to only single page wide tall draws for now. Too many false positives otherwise (e.g. NFSU).
	if (m_context->FRAME.FBW > 1 || m_r.height() < 1024)
		return false;

	u32 pages_covered;
	if (!CheckNextDrawForSplitClear(m_r, &pages_covered))
		return false;

	m_split_clear_start = m_cached_ctx.FRAME;
	m_split_clear_start_Z = m_cached_ctx.ZBUF;
	m_split_clear_pages = pages_covered;
	m_split_clear_color = GetConstantDirectWriteMemClearColor();

	GL_INS("HW: Starting split clear at FBP %x FBW %u PSM %s with %dx%d rect covering %u pages",
		m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBW, GSUtil::GetPSMName(m_cached_ctx.FRAME.PSM),
		m_r.width(), m_r.height(), pages_covered);

	// Remove any targets which are directly at the start.
	if (IsDiscardingDstColor())
	{
		const u32 bp = m_cached_ctx.FRAME.Block();
		g_texture_cache->InvalidateVideoMemType(GSTextureCache::RenderTarget, bp, m_cached_ctx.FRAME.PSM);
		g_texture_cache->InvalidateVideoMemType(GSTextureCache::DepthStencil, bp, m_cached_ctx.FRAME.PSM);
	}

	return true;
}

bool GSRendererHW::ContinueSplitClear()
{
	// Should be a mem clear type draw.
	if (!IsConstantDirectWriteMemClear())
		return false;

	// Shouldn't have gaps.
	if (m_vt.m_eq.rgba != 0xFFFF || (!m_cached_ctx.ZBUF.ZMSK && !m_vt.m_eq.z) || m_primitive_covers_without_gaps != NoGapsType::FullCover)
		return false;

	// Remove any targets which are directly at the start, since we checked this draw in the last.
	if (IsDiscardingDstColor())
	{
		const u32 bp = m_cached_ctx.FRAME.Block();
		g_texture_cache->InvalidateVideoMemType(GSTextureCache::RenderTarget, bp, m_cached_ctx.FRAME.PSM);
		g_texture_cache->InvalidateVideoMemType(GSTextureCache::DepthStencil, bp, m_cached_ctx.FRAME.PSM);
	}

	// Check next draw.
	u32 pages_covered;
	const bool skip = CheckNextDrawForSplitClear(m_r, &pages_covered);

	// We might've found the end, but this draw still counts.
	m_split_clear_pages += pages_covered;
	return skip;
}

bool GSRendererHW::CheckNextDrawForSplitClear(const GSVector4i& r, u32* pages_covered_by_this_draw) const
{
	const u32 end_block = GSLocalMemory::GetEndBlockAddress(m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBW, m_cached_ctx.FRAME.PSM, r);
	if (pages_covered_by_this_draw)
	{
		if (end_block < m_cached_ctx.FRAME.Block())
			*pages_covered_by_this_draw = (((GS_MAX_BLOCKS - end_block) + m_cached_ctx.FRAME.Block()) + (GS_BLOCKS_PER_PAGE)) / GS_BLOCKS_PER_PAGE;
		else
			*pages_covered_by_this_draw = ((end_block - m_cached_ctx.FRAME.Block()) + (GS_BLOCKS_PER_PAGE)) / GS_BLOCKS_PER_PAGE;
	}

	// must be changing FRAME
	if (m_backed_up_ctx < 0 || (m_dirty_gs_regs & (1u << DIRTY_REG_FRAME)) == 0)
		return false;

	// rect width should match the FBW (page aligned)
	if (r.width() != m_cached_ctx.FRAME.FBW * 64)
		return false;

	// next FBP should point to the end of the rect
	const GSDrawingContext& next_ctx = m_env.CTXT[m_backed_up_ctx];
	if (next_ctx.FRAME.Block() != ((end_block + 1) % GS_MAX_BLOCKS) ||
		m_context->TEX0.U64 != next_ctx.TEX0.U64 ||
		m_context->TEX1.U64 != next_ctx.TEX1.U64 || m_context->CLAMP.U64 != next_ctx.CLAMP.U64 ||
		m_context->TEST.U64 != next_ctx.TEST.U64 || ((m_context->FRAME.U64 ^ next_ctx.FRAME.U64) & (~0x1FF)) != 0 ||
		((m_context->ZBUF.U64 ^ next_ctx.ZBUF.U64) & (~0x1FF)) != 0)
	{
		return false;
	}

	// check ZBP if we're doing Z too
	if (!m_cached_ctx.ZBUF.ZMSK && m_cached_ctx.FRAME.FBP != m_cached_ctx.ZBUF.ZBP)
	{
		const u32 end_z_block = GSLocalMemory::GetEndBlockAddress(
			m_cached_ctx.ZBUF.Block(), m_cached_ctx.FRAME.FBW, m_cached_ctx.ZBUF.PSM, r);
		if (next_ctx.ZBUF.Block() != ((end_z_block + 1) % GS_MAX_BLOCKS))
			return false;
	}

	return true;
}

void GSRendererHW::FinishSplitClear()
{
	GL_INS("HW: FinishSplitClear(): Start %x FBW %u PSM %s, %u pages, %08X color", m_split_clear_start.Block(),
		m_split_clear_start.FBW, GSUtil::GetPSMName(m_split_clear_start.PSM), m_split_clear_pages, m_split_clear_color);

	// If this was a tall single-page draw, try to get a better BW from somewhere.
	if (m_split_clear_start.FBW <= 1 && m_split_clear_pages >= 16) // 1024 high
		TryToResolveSinglePageFramebuffer(m_split_clear_start, false);

	SetNewFRAME(m_split_clear_start.Block(), m_split_clear_start.FBW, m_split_clear_start.PSM);
	SetNewZBUF(m_split_clear_start_Z.Block(), m_split_clear_start_Z.PSM);
	ReplaceVerticesWithSprite(
		GetDrawRectForPages(m_split_clear_start.FBW, m_split_clear_start.PSM, m_split_clear_pages), GSVector2i(1, 1));
	GL_INS("HW: FinishSplitClear(): New draw rect is (%d,%d=>%d,%d) with FBW %u and PSM %s", m_r.x, m_r.y, m_r.z, m_r.w,
		m_split_clear_start.FBW, GSUtil::GetPSMName(m_split_clear_start.PSM));
	m_split_clear_start.U64 = 0;
	m_split_clear_start_Z.U64 = 0;
	m_split_clear_pages = 0;
	m_split_clear_color = 0;
}

bool GSRendererHW::NeedsBlending()
{
	const u32 temp_fbmask = (GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].bpp == 16) ? 0x00F8F8F8 : 0x00FFFFFF;
	const bool FBMASK_skip = (m_cached_ctx.FRAME.FBMSK & temp_fbmask) == temp_fbmask;
	return PRIM->ABE && !FBMASK_skip;
}

bool GSRendererHW::IsRTWritten()
{
	const GIFRegTEST TEST = m_cached_ctx.TEST;
	const bool only_z_written = (TEST.ATE && TEST.ATST == ATST_NEVER && TEST.AFAIL == AFAIL_ZB_ONLY);
	if (only_z_written)
		return false;

	const u32 written_bits = (~m_cached_ctx.FRAME.FBMSK & GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].fmsk);
	const GIFRegALPHA ALPHA = m_context->ALPHA;
	return (
	        // A not masked
	        (written_bits & 0xFF000000u) != 0) ||
	       (
	        // RGB not entirely masked
	        ((written_bits & 0x00FFFFFFu) != 0) &&
	        // RGB written through no-blending, or blend result being non-zero
	        (!PRIM->ABE || // not blending
	         ALPHA.D != 1 || // additive to Cs
	         (ALPHA.A != ALPHA.B && // left side is not zero
	          (ALPHA.C == 1 || // multiply by Ad
	           (ALPHA.C == 2 && ALPHA.FIX != 0) || // multiply by 0
	           (ALPHA.C == 0 && GetAlphaMinMax().max != 0)))));
}

bool GSRendererHW::IsDepthAlwaysPassing()
{
	const u32 max_z = (0xFFFFFFFF >> (GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].fmt * 8));
	const int check_index = m_vt.m_primclass == GS_SPRITE_CLASS ? 1 : 0;
	// Depth is always pass/fail (no read) and write are discarded.
	return (!m_cached_ctx.TEST.ZTE || m_cached_ctx.TEST.ZTST <= ZTST_ALWAYS) ||
	       // Depth test will always pass
	       (m_cached_ctx.TEST.ZTST == ZTST_GEQUAL && m_vt.m_eq.z && std::min(m_vertex.buff[check_index].XYZ.Z, max_z) == max_z);
}

bool GSRendererHW::IsUsingCsInBlend()
{
	const GIFRegALPHA ALPHA = m_context->ALPHA;
	const bool blend_zero = (ALPHA.A == ALPHA.B || (ALPHA.C == 2 && ALPHA.FIX == 0) || (ALPHA.C == 0 && GetAlphaMinMax().max == 0));
	return (NeedsBlending() && ((ALPHA.IsUsingCs() && !blend_zero) || m_context->ALPHA.D == 0));
}

bool GSRendererHW::IsUsingAsInBlend()
{
	return (NeedsBlending() && m_context->ALPHA.IsUsingAs() && GetAlphaMinMax().max != 0);
}
bool GSRendererHW::ChannelsSharedTEX0FRAME()
{
	if (!m_cached_ctx.TEST.DATE && !IsRTWritten())
		return false;

	return GSUtil::GetChannelMask(m_cached_ctx.FRAME.PSM, m_cached_ctx.FRAME.FBMSK) & GSUtil::GetChannelMask(m_cached_ctx.TEX0.PSM);
}

bool GSRendererHW::IsTBPFrameOrZ(u32 tbp, bool frame_only)
{
	const bool is_frame = (m_cached_ctx.FRAME.Block() == tbp) && (GSUtil::GetChannelMask(m_cached_ctx.FRAME.PSM) & GSUtil::GetChannelMask(m_cached_ctx.TEX0.PSM));
	const bool is_z = (m_cached_ctx.ZBUF.Block() == tbp) && (GSUtil::GetChannelMask(m_cached_ctx.ZBUF.PSM) & GSUtil::GetChannelMask(m_cached_ctx.TEX0.PSM));
	if (!is_frame && !is_z)
		return false;

	const u32 fm = m_cached_ctx.FRAME.FBMSK;
	const u32 zm = m_cached_ctx.ZBUF.ZMSK || m_cached_ctx.TEST.ZTE == 0 ? 0xffffffff : 0;
	const u32 fm_mask = GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].fmsk;

	const u32 max_z = (0xFFFFFFFF >> (GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].fmt * 8));
	const bool no_rt = (!m_cached_ctx.TEST.DATE && !IsRTWritten());
	const bool no_ds = (
	                       // Depth is always pass/fail (no read) and write are discarded.
	                       (zm != 0 && m_cached_ctx.TEST.ZTST <= ZTST_ALWAYS) ||
	                       // Depth test will always pass
	                       (zm != 0 && m_cached_ctx.TEST.ZTST == ZTST_GEQUAL && m_vt.m_eq.z && std::min(m_vertex.buff[0].XYZ.Z, max_z) == max_z) ||
	                       // Depth will be written through the RT
	                       (!no_rt && m_cached_ctx.FRAME.FBP == m_cached_ctx.ZBUF.ZBP && !PRIM->TME && zm == 0 && (fm & fm_mask) == 0 && m_cached_ctx.TEST.ZTE)) ||
	                   // No color or Z being written.
	                   (no_rt && zm != 0);

	// Relying a lot on the optimizer here... I don't like it.
	return (is_frame && !no_rt) || (is_z && !no_ds && !frame_only);
}

void GSRendererHW::HandleManualDeswizzle()
{
	if (!m_vt.m_eq.z)
		return;

	// Check if it's doing manual deswizzling first (draws are 32x16), if they are, check if the Z is flat, if not,
	// we're gonna have to get creative and swap around the quandrants, but that's a TODO.
	GSVertex* v = &m_vertex.buff[0];

	// Check for page quadrant and compare it to the quadrant from the verts, if it does match then we need to do correction.
	const GSVector2i page_quadrant = GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].pgs / 2;

	if (PRIM->FST)
	{
		for (u32 i = 0; i < m_index.tail; i += 2)
		{
			const u32 index_first = m_index.buff[i];
			const u32 index_last = m_index.buff[i + 1];

			if ((abs((v[index_last].U) - (v[index_first].U)) >> 4) != page_quadrant.x || (abs((v[index_last].V) - (v[index_first].V)) >> 4) != page_quadrant.y)
				return;
		}
	}
	else
	{
		for (u32 i = 0; i < m_index.tail; i += 2)
		{
			const u32 index_first = m_index.buff[i];
			const u32 index_last = m_index.buff[i + 1];
			const u32 x = abs(((v[index_last].ST.S / v[index_last].RGBAQ.Q) * (1 << m_context->TEX0.TW)) - ((v[index_first].ST.S / v[index_first].RGBAQ.Q) * (1 << m_context->TEX0.TW)));
			const u32 y = abs(((v[index_last].ST.T / v[index_last].RGBAQ.Q) * (1 << m_context->TEX0.TH)) - ((v[index_first].ST.T / v[index_first].RGBAQ.Q) * (1 << m_context->TEX0.TH)));

			if (x != static_cast<u32>(page_quadrant.x) || y != static_cast<u32>(page_quadrant.y))
				return;
		}
	}

	GSVector4i tex_rect = GSVector4i(m_vt.m_min.t.x, m_vt.m_min.t.y, m_vt.m_max.t.x, m_vt.m_max.t.y);
	ReplaceVerticesWithSprite(m_r, tex_rect, GSVector2i(1 << m_cached_ctx.TEX0.TW, 1 << m_cached_ctx.TEX0.TH), m_context->scissor.in);
}

void GSRendererHW::InvalidateVideoMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r)
{
	// printf("HW: [%d] InvalidateVideoMem %d,%d - %d,%d %05x (%d)\n", static_cast<int>(g_perfmon.GetFrame()), r.left, r.top, r.right, r.bottom, static_cast<int>(BITBLTBUF.DBP), static_cast<int>(BITBLTBUF.DPSM));

	// This is gross, but if the EE write loops, we need to split it on the 2048 border.
	GSVector4i rect = r;
	bool loop_h = false;
	bool loop_w = false;
	if (r.w > 2048)
	{
		rect.w = 2048;
		loop_h = true;
	}
	if (r.z > 2048)
	{
		rect.z = 2048;
		loop_w = true;
	}
	if (loop_h || loop_w)
	{
		g_texture_cache->InvalidateVideoMem(m_mem.GetOffset(BITBLTBUF.DBP, BITBLTBUF.DBW, BITBLTBUF.DPSM), rect);
		if (loop_h)
		{
			rect.y = 0;
			rect.w = r.w - 2048;
		}
		if (loop_w)
		{
			rect.x = 0;
			rect.z = r.z - 2048;
		}
		g_texture_cache->InvalidateVideoMem(m_mem.GetOffset(BITBLTBUF.DBP, BITBLTBUF.DBW, BITBLTBUF.DPSM), rect);
	}
	else
		g_texture_cache->InvalidateVideoMem(m_mem.GetOffset(BITBLTBUF.DBP, BITBLTBUF.DBW, BITBLTBUF.DPSM), r);
}

void GSRendererHW::InvalidateLocalMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r, bool clut)
{
	// printf("HW: [%d] InvalidateLocalMem %d,%d - %d,%d %05x (%d)\n", static_cast<int>(g_perfmon.GetFrame()), r.left, r.top, r.right, r.bottom, static_cast<int>(BITBLTBUF.SBP), static_cast<int>(BITBLTBUF.SPSM));

	if (clut)
		return; // FIXME

	auto iter = m_draw_transfers.end();
	bool skip = false;
	// If the EE write overlaps the readback and was done since the last draw, there's no need to read it back.
	// Dog's life does this.
	while (iter != m_draw_transfers.begin())
	{
		--iter;

		if (!(iter->draw == s_n && BITBLTBUF.SBP == iter->blit.DBP && iter->blit.DPSM == BITBLTBUF.SPSM && r.eq(iter->rect)))
			continue;

		g_texture_cache->InvalidateVideoMem(m_mem.GetOffset(BITBLTBUF.SBP, BITBLTBUF.SBW, BITBLTBUF.SPSM), r);
		skip = true;
		break;
	}

	if (!skip)
	{
		const bool recursive_copy = (BITBLTBUF.SBP == BITBLTBUF.DBP) && (m_env.TRXDIR.XDIR == 2);
		g_texture_cache->InvalidateLocalMem(m_mem.GetOffset(BITBLTBUF.SBP, BITBLTBUF.SBW, BITBLTBUF.SPSM), r, recursive_copy);
	}
}

void GSRendererHW::Move()
{
	if (m_mv && m_mv(*this))
	{
		// Handled by HW hack.
		return;
	}

	if (m_env.TRXDIR.XDIR == 3)
		return;

	const int sx = m_env.TRXPOS.SSAX;
	const int sy = m_env.TRXPOS.SSAY;
	const int dx = m_env.TRXPOS.DSAX;
	const int dy = m_env.TRXPOS.DSAY;

	const int w = m_env.TRXREG.RRW;
	const int h = m_env.TRXREG.RRH;
	GL_CACHE("HW: Starting Move! 0x%x W:%d F:%s => 0x%x W:%d F:%s (DIR %d%d), sPos(%d %d) dPos(%d %d) size(%d %d) draw %d",
		m_env.BITBLTBUF.SBP, m_env.BITBLTBUF.SBW, GSUtil::GetPSMName(m_env.BITBLTBUF.SPSM),
		m_env.BITBLTBUF.DBP, m_env.BITBLTBUF.DBW, GSUtil::GetPSMName(m_env.BITBLTBUF.DPSM),
		m_env.TRXPOS.DIRX, m_env.TRXPOS.DIRY,
		sx, sy, dx, dy, w, h, s_n);
	if (g_texture_cache->Move(m_env.BITBLTBUF.SBP, m_env.BITBLTBUF.SBW, m_env.BITBLTBUF.SPSM, sx, sy,
			m_env.BITBLTBUF.DBP, m_env.BITBLTBUF.DBW, m_env.BITBLTBUF.DPSM, dx, dy, w, h))
	{
		m_env.TRXDIR.XDIR = 3;
		// Handled entirely in TC, no need to update local memory.
		return;
	}

	GSRenderer::Move();
}

u16 GSRendererHW::Interpolate_UV(float alpha, int t0, int t1)
{
	const float t = (1.0f - alpha) * t0 + alpha * t1;
	return static_cast<u16>(t) & ~0xF; // cheap rounding
}

float GSRendererHW::alpha0(int L, int X0, int X1)
{
	const int x = (X0 + 15) & ~0xF; // Round up
	return static_cast<float>(x - X0) / static_cast<float>(L);
}

float GSRendererHW::alpha1(int L, int X0, int X1)
{
	const int x = (X1 - 1) & ~0xF; // Round down. Note -1 because right pixel isn't included in primitive so 0x100 must return 0.
	return static_cast<float>(x - X0) / static_cast<float>(L);
}

void GSRendererHW::SwSpriteRender()
{
	// Supported drawing attributes
	pxAssert(PRIM->PRIM == GS_TRIANGLESTRIP || PRIM->PRIM == GS_SPRITE);
	pxAssert(!PRIM->FGE); // No FOG
	pxAssert(!PRIM->AA1); // No antialiasing
	pxAssert(!PRIM->FIX); // Normal fragment value control

	pxAssert(!m_draw_env->DTHE.DTHE); // No dithering

	pxAssert(!m_cached_ctx.TEST.ATE); // No alpha test
	pxAssert(!m_cached_ctx.TEST.DATE); // No destination alpha test
	pxAssert(!m_cached_ctx.DepthRead() && !m_cached_ctx.DepthWrite()); // No depth handling

	pxAssert(!m_cached_ctx.TEX0.CSM); // No CLUT usage

	pxAssert(!m_draw_env->PABE.PABE); // No PABE

	// PSMCT32 pixel format
	pxAssert(!PRIM->TME || m_cached_ctx.TEX0.PSM == PSMCT32);
	pxAssert(m_cached_ctx.FRAME.PSM == PSMCT32);

	// No rasterization required
	pxAssert(PRIM->PRIM == GS_SPRITE
		|| ((PRIM->IIP || m_vt.m_eq.rgba == 0xffff)
			&& m_vt.m_eq.z == 0x1
			&& (!PRIM->TME || PRIM->FST || m_vt.m_eq.q == 0x1)));  // Check Q equality only if texturing enabled and STQ coords used

	const bool texture_mapping_enabled = PRIM->TME;

	const GSVector4i r = m_r;

#ifndef NDEBUG
	const int tw = 1 << m_cached_ctx.TEX0.TW;
	const int th = 1 << m_cached_ctx.TEX0.TH;
	const float meas_tw = m_vt.m_max.t.x - m_vt.m_min.t.x;
	const float meas_th = m_vt.m_max.t.y - m_vt.m_min.t.y;
	pxAssert(!PRIM->TME || (abs(meas_tw - r.width()) <= SSR_UV_TOLERANCE && abs(meas_th - r.height()) <= SSR_UV_TOLERANCE)); // No input texture min/mag, if any.
	pxAssert(!PRIM->TME || (abs(m_vt.m_min.t.x) <= SSR_UV_TOLERANCE && abs(m_vt.m_min.t.y) <= SSR_UV_TOLERANCE && abs(meas_tw - tw) <= SSR_UV_TOLERANCE && abs(meas_th - th) <= SSR_UV_TOLERANCE)); // No texture UV wrap, if any.
#endif

	GIFRegTRXPOS trxpos = {};

	trxpos.DSAX = r.x;
	trxpos.DSAY = r.y;
	trxpos.SSAX = static_cast<int>(m_vt.m_min.t.x / 2) * 2; // Rounded down to closest even integer.
	trxpos.SSAY = static_cast<int>(m_vt.m_min.t.y / 2) * 2;

	pxAssert(r.x % 2 == 0 && r.y % 2 == 0);

	GIFRegTRXREG trxreg = {};

	trxreg.RRW = r.width();
	trxreg.RRH = r.height();

	pxAssert(r.width() % 2 == 0 && r.height() % 2 == 0);

	// SW rendering code, mainly taken from GSState::Move(), TRXPOS.DIR{X,Y} management excluded

	const int sx = trxpos.SSAX;
	int sy = trxpos.SSAY;
	const int dx = trxpos.DSAX;
	int dy = trxpos.DSAY;
	const int w = trxreg.RRW;
	const int h = trxreg.RRH;

	GL_INS("HW: SwSpriteRender: Dest 0x%x W:%d F:%s, size(%d %d)", m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBW, GSUtil::GetPSMName(m_cached_ctx.FRAME.PSM), w, h);

	const GSOffset spo = m_mem.GetOffset(m_context->TEX0.TBP0, m_context->TEX0.TBW, m_context->TEX0.PSM);
	const GSOffset& dpo = m_context->offset.fb;

	const bool alpha_blending_enabled = NeedsBlending();

	const GSVertex& v = m_index.tail > 0 ? m_vertex.buff[m_index.buff[m_index.tail - 1]] : GSVertex(); // Last vertex if any.
	const GSVector4i vc = GSVector4i(v.RGBAQ.R, v.RGBAQ.G, v.RGBAQ.B, v.RGBAQ.A) // 0x000000AA000000BB000000GG000000RR
	                          .ps32();                                           // 0x00AA00BB00GG00RR00AA00BB00GG00RR

	const GSVector4i a_mask = GSVector4i::xff000000().u8to16();                  // 0x00FF00000000000000FF000000000000

	const bool fb_mask_enabled = m_cached_ctx.FRAME.FBMSK != 0x0;
	const GSVector4i fb_mask = GSVector4i(m_cached_ctx.FRAME.FBMSK).u8to16();    // 0x00AA00BB00GG00RR00AA00BB00GG00RR

	const u8 tex0_tfx = m_cached_ctx.TEX0.TFX;
	const u8 tex0_tcc = m_cached_ctx.TEX0.TCC;
	const u8 alpha_a = m_context->ALPHA.A;
	const u8 alpha_b = m_context->ALPHA.B;
	const u8 alpha_c = m_context->ALPHA.C;
	const u8 alpha_d = m_context->ALPHA.D;
	const u8 alpha_fix = m_context->ALPHA.FIX;

	if (texture_mapping_enabled)
		g_texture_cache->InvalidateLocalMem(spo, GSVector4i(sx, sy, sx + w, sy + h));
	constexpr bool invalidate_local_mem_before_fb_read = false;
	if (invalidate_local_mem_before_fb_read && (alpha_blending_enabled || fb_mask_enabled))
		g_texture_cache->InvalidateLocalMem(dpo, m_r);

	for (int y = 0; y < h; y++, ++sy, ++dy)
	{
		u32* vm = m_mem.vm32();
		const GSOffset::PAHelper spa = spo.paMulti(sx, sy);
		const GSOffset::PAHelper dpa = dpo.paMulti(dx, dy);

		pxAssert(w % 2 == 0);

		for (int x = 0; x < w; x += 2)
		{
			u32* di = &vm[dpa.value(x)];
			pxAssert(di + 1 == &vm[dpa.value(x + 1)]); // Destination pixel pair is adjacent in memory

			GSVector4i sc = {};
			if (texture_mapping_enabled)
			{
				const u32* si = &vm[spa.value(x)];
				// Read 2 source pixel colors
				pxAssert(si + 1 == &vm[spa.value(x + 1)]); // Source pixel pair is adjacent in memory
				sc = GSVector4i::loadl(si).u8to16(); // 0x00AA00BB00GG00RR00aa00bb00gg00rr

				// Apply TFX
				pxAssert(tex0_tfx == 0 || tex0_tfx == 1);
				if (tex0_tfx == 0)
					sc = sc.mul16l(vc).srl16<7>().clamp8(); // clamp((sc * vc) >> 7, 0, 255), srl16 is ok because 16 bit values are unsigned

				if (tex0_tcc == 0)
					sc = sc.blend(vc, a_mask);
			}
			else
				sc = vc;

			// No FOG

			GSVector4i dc0 = {};
			GSVector4i dc = {};

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
				const GSVector4i C = alpha_c == 2 ? GSVector4i(alpha_fix).xxxx().ps32()
				                                  : (alpha_c == 0 ? sc : dc0).yyww()      // 0x00AA00BB00AA00BB00aa00bb00aa00bb
				                                                             .srl32<16>() // 0x000000AA000000AA000000aa000000aa
				                                                             .ps32()      // 0x00AA00AA00aa00aa00AA00AA00aa00aa
				                                                             .xxyy();     // 0x00AA00AA00AA00AA00aa00aa00aa00aa
				const GSVector4i D = alpha_d == 0 ? sc : alpha_d == 1 ? dc0 : GSVector4i::zero();
				dc = A.sub16(B).mul16l(C).sra16<7>().add16(D); // (((A - B) * C) >> 7) + D, must use sra16 due to signed 16 bit values.
				// dc alpha channels (dc.u16[3], dc.u16[7]) dirty
			}
			else
				dc = sc;

			// No dithering

			// Clamping
			if (m_draw_env->COLCLAMP.CLAMP)
				dc = dc.clamp8(); // clamp(dc, 0, 255)
			else
				dc = dc.sll16<8>().srl16<8>(); // Mask, lower 8 bits enabled per channel

			// No Alpha Correction
			pxAssert(m_context->FBA.FBA == 0);
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

	g_texture_cache->InvalidateVideoMem(dpo, m_r);
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
	if (m_cached_ctx.DepthRead() || m_cached_ctx.DepthWrite()) // No depth handling
		return false;
	if (m_cached_ctx.FRAME.PSM != PSMCT32) // Frame buffer format is 32 bit color
		return false;
	if (PRIM->TME)
	{
		// Texture mapping enabled

		if (m_cached_ctx.TEX0.PSM != PSMCT32) // Input texture format is 32 bit color
			return false;
		if (IsMipMapDraw()) // No mipmapping.
			return false;
		const int tw = 1 << m_cached_ctx.TEX0.TW;
		const int th = 1 << m_cached_ctx.TEX0.TH;
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
	const u32 count = m_vertex.next;
	GSVertex* v = &m_vertex.buff[0];

	for (u32 i = 0; i < count; i += 2)
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
			fprintf(stderr, "HW: u0:%d and u1:%d\n", v[i].U, v[i + 1].U);
			fprintf(stderr, "HW: a0:%f and a1:%f\n", ax0, ax1);
			fprintf(stderr, "HW: :%d and t1:%d\n", tx0, tx1);
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
			fprintf(stderr, "HW: v0:%d and v1:%d\n", v[i].V, v[i + 1].V);
			fprintf(stderr, "HW: a0:%f and a1:%f\n", ay0, ay1);
			fprintf(stderr, "HW: t0:%d and t1:%d\n", ty0, ty1);
		}
#endif

#ifdef DEBUG_U
		if (debug)
			fprintf(stderr, "HW: GREP_BEFORE %d => %d\n", v[i].U, v[i + 1].U);
#endif
#ifdef DEBUG_V
		if (debug)
			fprintf(stderr, "HW: GREP_BEFORE %d => %d\n", v[i].V, v[i + 1].V);
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
			fprintf(stderr, "HW: GREP_AFTER %d => %d\n\n", v[i].U, v[i + 1].U);
#endif
#ifdef DEBUG_V
		if (debug)
			fprintf(stderr, "HW: GREP_AFTER %d => %d\n\n", v[i].V, v[i + 1].V);
#endif
	}
}

void GSRendererHW::Draw()
{
	static u32 num_skipped_channel_shuffle_draws = 0;

	// We mess with this state as an optimization, so take a copy and use that instead.
	const GSDrawingContext* context = m_context;
	m_cached_ctx.TEX0 = context->TEX0;
	m_cached_ctx.TEXA = m_draw_env->TEXA;
	m_cached_ctx.CLAMP = context->CLAMP;
	m_cached_ctx.TEST = context->TEST;
	m_cached_ctx.FRAME = context->FRAME;
	m_cached_ctx.ZBUF = context->ZBUF;

	if (IsBadFrame())
	{
		GL_INS("HW: Warning skipping a draw call (%d)", s_n);
		return;
	}

	// Channel shuffles repeat lots of draws. Get out early if we can.
	if (m_channel_shuffle)
	{
		// NFSU2 does consecutive channel shuffles with blending, reducing the alpha channel over time.
		// Fortunately, it seems to change the FBMSK along the way, so this check alone is sufficient.
		// Tomb Raider: Underworld does similar, except with R, G, B in separate palettes, therefore
		// we need to split on those too.
		m_channel_shuffle = !m_channel_shuffle_abort && IsPossibleChannelShuffle() && m_last_channel_shuffle_fbmsk == m_context->FRAME.FBMSK &&
		                    m_last_channel_shuffle_fbp <= m_context->FRAME.Block() && m_last_channel_shuffle_end_block > m_context->FRAME.Block() &&
		                    m_last_channel_shuffle_tbp <= m_context->TEX0.TBP0;

		if (m_channel_shuffle)
		{
			// Tombraider does vertical strips 2 pages at a time, then puts them horizontally, it's a mess, so let it do the full screen shuffle.
			m_full_screen_shuffle |= !IsPageCopy() && NextDrawMatchesShuffle();
			// These HLE's skip several channel shuffles in a row which change blends etc. Let's not break the flow, it gets upset.
			if (!m_conf.ps.urban_chaos_hle && !m_conf.ps.tales_of_abyss_hle)
			{
				m_last_channel_shuffle_fbp = m_context->FRAME.Block();
				m_last_channel_shuffle_tbp = m_context->TEX0.TBP0;
			}

			num_skipped_channel_shuffle_draws++;
			return;
		}

		if (m_channel_shuffle_width)
		{
			if (m_last_rt)
			{
				//DevCon.Warning("Skipped %d draw %d was abort %d", num_skipped_channel_shuffle_draws, s_n, (int)m_channel_shuffle_abort);
				// Some games like Tomb raider abort early, we're never going to know the real height, and the system doesn't work right for partials.
				// But it's good enough for games like Hitman Blood Money which only shuffle part of the screen
				const int width = std::max(static_cast<int>(m_last_rt->m_TEX0.TBW) * 64, 64);
				const int shuffle_height = (((num_skipped_channel_shuffle_draws + 1 + (std::max(1, (width / 64) - 1))) * 64) / width) * 32;
				const int shuffle_width = std::min((num_skipped_channel_shuffle_draws + 1) * 64, static_cast<u32>(width));
				GSVector4i valid_area = GSVector4i::loadh(GSVector2i(shuffle_width, shuffle_height));
				const int offset = (((m_last_channel_shuffle_fbp + 0x20) - m_last_rt->m_TEX0.TBP0) >> 5) - (num_skipped_channel_shuffle_draws + 1);

				if (offset)
				{
					int vertical_offset = (offset / std::max(1U, m_channel_shuffle_width)) * 32;
					valid_area.y += vertical_offset;
					valid_area.w += vertical_offset;
				}

				if (!m_full_screen_shuffle)
				{
					m_conf.scissor.w = m_conf.scissor.y + shuffle_height * m_conf.cb_ps.ScaleFactor.z;
					if (shuffle_width)
						m_conf.scissor.z = m_conf.scissor.x + (shuffle_width * m_conf.cb_ps.ScaleFactor.z);
					else
						m_conf.scissor.z = std::min(m_conf.scissor.z, static_cast<int>((m_channel_shuffle_width * 64) * m_conf.cb_ps.ScaleFactor.z));
				}

				m_last_rt->UpdateValidity(valid_area);

				g_gs_device->RenderHW(m_conf);

				if (GSConfig.DumpGSData)
				{
					if (GSConfig.ShouldDump(s_n - 1, g_perfmon.GetFrame()))
					{
						if (m_last_rt && GSConfig.SaveRT)
						{
							const u64 frame = g_perfmon.GetFrame();

							std::string s = GetDrawDumpPath("%05d_f%05lld_rt1_%05x_(%05x)_%s.bmp", s_n - 1, frame, m_last_channel_shuffle_fbp, m_last_rt->m_TEX0.TBP0, GSUtil::GetPSMName(m_cached_ctx.FRAME.PSM));

							m_last_rt->m_texture->Save(s);
						}
					}
				}
				g_texture_cache->InvalidateTemporarySource();
				CleanupDraw(false);
			}
		}
#ifdef ENABLE_OGL_DEBUG
		if (num_skipped_channel_shuffle_draws > 0)
			GL_CACHE("HW: Skipped %d channel shuffle draws ending at %d", num_skipped_channel_shuffle_draws, s_n);
#endif
		num_skipped_channel_shuffle_draws = 0;

		m_last_channel_shuffle_fbp = 0xffff;
		m_last_channel_shuffle_tbp = 0xffff;
		m_last_channel_shuffle_end_block = 0xffff;
	}

	m_last_rt = nullptr;
	m_channel_shuffle_width = 0;
	m_full_screen_shuffle = false;
	m_channel_shuffle_abort = false;
	m_channel_shuffle_src_valid = GSVector4i::zero();

	GL_PUSH("HW: Draw %d (Context %u)", s_n, PRIM->CTXT);
	GL_INS("HW: FLUSH REASON: %s%s", GetFlushReasonString(m_state_flush_reason),
		(m_state_flush_reason != GSFlushReason::CONTEXTCHANGE && m_dirty_gs_regs) ? " AND POSSIBLE CONTEXT CHANGE" :
																					"");

	// When the format is 24bit (Z or C), DATE ceases to function.
	// It was believed that in 24bit mode all pixels pass because alpha doesn't exist
	// however after testing this on a PS2 it turns out nothing passes, it ignores the draw.
	if ((m_cached_ctx.FRAME.PSM & 0xF) == PSMCT24 && m_context->TEST.DATE)
	{
		GL_CACHE("HW: DATE on a 24bit format, Frame PSM %x", m_context->FRAME.PSM);
		return;
	}

	// skip alpha test if possible
	// Note: do it first so we know if frame/depth writes are masked
	u32 fm = m_cached_ctx.FRAME.FBMSK;
	u32 zm = (m_cached_ctx.ZBUF.ZMSK || m_cached_ctx.TEST.ZTE == 0 ||
	             (m_cached_ctx.TEST.ATE && m_cached_ctx.TEST.ATST == ZTST_NEVER && m_cached_ctx.TEST.AFAIL != AFAIL_ZB_ONLY)) ?
	             0xffffffffu :
	             0;
	const u32 fm_mask = GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].fmsk;

	// Note required to compute TryAlphaTest below. So do it now.
	const GSLocalMemory::psm_t& tex_psm = GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM];
	if (PRIM->TME && tex_psm.pal > 0)
	{
		m_mem.m_clut.Read32(m_cached_ctx.TEX0, m_cached_ctx.TEXA);
		if (m_mem.m_clut.GetGPUTexture())
		{
			CalcAlphaMinMax(0, 255);
		}
	}

	//  Test if we can optimize Alpha Test as a NOP
	m_cached_ctx.TEST.ATE = m_cached_ctx.TEST.ATE && !GSRenderer::TryAlphaTest(fm, zm);

	// Need to fix the alpha test, since the alpha will be fixed to 1.0 if ABE is disabled and AA1 is enabled
	// So if it doesn't meet the condition, always fail, if it does, always pass (turn off the test).
	if (IsCoverageAlpha() && m_cached_ctx.TEST.ATE && m_cached_ctx.TEST.ATST > 1)
	{
		const float aref = static_cast<float>(m_cached_ctx.TEST.AREF);
		const int old_ATST = m_cached_ctx.TEST.ATST;
		m_cached_ctx.TEST.ATST = 0;

		switch (old_ATST)
		{
			case ATST_LESS:
				if (128.0f < aref)
					m_cached_ctx.TEST.ATE = false;
				break;
			case ATST_LEQUAL:
				if (128.0f <= aref)
					m_cached_ctx.TEST.ATE = false;
				break;
			case ATST_EQUAL:
				if (128.0f == aref)
					m_cached_ctx.TEST.ATE = false;
				break;
			case ATST_GEQUAL:
				if (128.0f >= aref)
					m_cached_ctx.TEST.ATE = false;
				break;
			case ATST_GREATER:
				if (128.0f > aref)
					m_cached_ctx.TEST.ATE = false;
				break;
			case ATST_NOTEQUAL:
				if (128.0f != aref)
					m_cached_ctx.TEST.ATE = false;
				break;
			default:
				break;
		}
	}

	m_cached_ctx.FRAME.FBMSK = fm;
	m_cached_ctx.ZBUF.ZMSK = zm != 0;

	// It is allowed to use the depth and rt at the same location. However at least 1 must
	// be disabled. Or the written value must be the same on both channels.
	// 1/ GoW uses a Cd blending on a 24 bits buffer (no alpha)
	// 2/ SuperMan really draws (0,0,0,0) color and a (0) 32-bits depth
	// 3/ 50cents really draws (0,0,0,128) color and a (0) 24 bits depth
	// Note: FF DoC has both buffer at same location but disable the depth test (write?) with ZTE = 0
	bool no_rt = (!m_cached_ctx.TEST.DATE && !IsRTWritten());
	const bool all_depth_tests_pass = IsDepthAlwaysPassing();
	bool no_ds = (zm != 0 && all_depth_tests_pass) ||
	             // No color or Z being written.
	             (no_rt && zm != 0);

	// No Z test if no z buffer.
	if (no_ds || all_depth_tests_pass)
	{
		if (m_cached_ctx.TEST.ZTST != ZTST_ALWAYS)
			GL_CACHE("HW: Disabling Z tests because all tests will pass.");

		m_cached_ctx.TEST.ZTST = ZTST_ALWAYS;
	}

	if (no_rt && no_ds)
	{
		GL_CACHE("HW: Skipping draw with no color nor depth output.");
		return;
	}

	// I hate that I have to do this, but some games (like Pac-Man World Rally) troll us by causing a flush with degenerate triangles, so we don't have all available information about the next draw.
	// So we have to check when the next draw happens if our frame has changed or if it's become recursive.
	const bool has_colclip_texture = g_gs_device->GetColorClipTexture() != nullptr;
	if (!no_rt && has_colclip_texture && (m_conf.colclip_frame.FBP != m_cached_ctx.FRAME.FBP || m_conf.colclip_frame.Block() == m_cached_ctx.TEX0.TBP0))
	{
		GIFRegTEX0 FRAME;
		FRAME.TBP0 = m_conf.colclip_frame.Block();
		FRAME.TBW = m_conf.colclip_frame.FBW;
		FRAME.PSM = m_conf.colclip_frame.PSM;

		GSTextureCache::Target* old_rt = g_texture_cache->LookupTarget(FRAME, GSVector2i(1, 1), GetTextureScaleFactor(), GSTextureCache::RenderTarget, true,
			fm, false, false, true, true, GSVector4i(0, 0, 1, 1), true, false, false);

		if (old_rt)
		{
			GL_CACHE("HW: Pre-draw resolve of colclip! Address: %x", FRAME.TBP0);
			GSTexture* colclip_texture = g_gs_device->GetColorClipTexture();
			g_gs_device->StretchRect(colclip_texture, GSVector4(m_conf.colclip_update_area) / GSVector4(GSVector4i(colclip_texture->GetSize()).xyxy()), old_rt->m_texture, GSVector4(m_conf.colclip_update_area),
				ShaderConvert::COLCLIP_RESOLVE, false);

			g_gs_device->Recycle(colclip_texture);

			g_gs_device->SetColorClipTexture(nullptr);
		}
		else
			DevCon.Warning("HW: Error resolving colclip texture for pre-draw resolve");
	}

	const bool draw_sprite_tex = PRIM->TME && (m_vt.m_primclass == GS_SPRITE_CLASS);

	// GS doesn't fill the right or bottom edges of sprites/triangles, and for a pixel to be shaded, the vertex
	// must cross the center. In other words, the range is equal to the floor of coordinates +0.5. Except for
	// the case where the minimum equals the maximum, because at least one pixel is filled per line.
	// Test cases for the math:
	//                                --------------------------------------
	//                                | Position range | Draw Range | Size |
	//                                |       -0.5,0.0 |        0-0 |    1 |
	//                                |       -0.5,0.5 |        0-0 |    1 |
	//                                |            0,1 |        0-0 |    1 |
	//                                |          0,1.5 |        0-1 |    2 |
	//                                |        0.5,1.5 |        1-1 |    1 |
	//                                |       0.5,1.75 |        1-1 |    1 |
	//                                |       0.5,2.25 |        1-1 |    1 |
	//                                |        0.5,2.5 |        1-2 |    2 |
	//                                --------------------------------------
	m_r = GSVector4i((m_vt.m_min.p.upld(m_vt.m_max.p) + GSVector4::cxpr(0.4f)).round<Round_NearestInt>());
	m_r = m_r.blend8(m_r + GSVector4i::cxpr(0, 0, 1, 1), (m_r.xyxy() == m_r.zwzw()));
	m_r_no_scissor = m_r;
	m_r = m_r.rintersect(context->scissor.in);

	// Draw is too small, just skip it.
	if (m_r.rempty())
	{
		GL_INS("HW: Draw %d skipped due to having an empty rect");
		return;
	}

	m_process_texture = PRIM->TME && !(NeedsBlending() && m_context->ALPHA.IsBlack() && !m_cached_ctx.TEX0.TCC) && !(no_rt && (!m_cached_ctx.TEST.ATE || m_cached_ctx.TEST.ATST <= ATST_ALWAYS));

	// We trigger the sw prim render here super early, to avoid creating superfluous render targets.
	if (CanUseSwPrimRender(no_rt, no_ds, draw_sprite_tex && m_process_texture) && SwPrimRender(*this, true, true))
	{
		GL_CACHE("HW: Possible texture decompression, drawn with SwPrimRender() (BP %x BW %u TBP0 %x TBW %u)",
			m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBMSK, m_cached_ctx.TEX0.TBP0, m_cached_ctx.TEX0.TBW);
		return;
	}

	// We want to fix up the context if we're doing a double half clear, regardless of whether we do the CPU fill.
	const ClearType is_possible_mem_clear = IsConstantDirectWriteMemClear();
	if (!GSConfig.UserHacks_DisableSafeFeatures && is_possible_mem_clear)
	{
		if (!DetectStripedDoubleClear(no_rt, no_ds))
			if (!DetectDoubleHalfClear(no_rt, no_ds))
				DetectRedundantBufferClear(no_rt, no_ds, fm_mask);
	}

	CalculatePrimitiveCoversWithoutGaps();

	const bool not_writing_to_all = (m_primitive_covers_without_gaps != NoGapsType::FullCover || AreAnyPixelsDiscarded() || !all_depth_tests_pass);
	bool preserve_depth =
		not_writing_to_all || (!no_ds && (!all_depth_tests_pass || !m_cached_ctx.DepthWrite() || m_cached_ctx.TEST.ATE));

	const u32 frame_end_bp = GSLocalMemory::GetUnwrappedEndBlockAddress(m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBW, m_cached_ctx.FRAME.PSM, m_r);

	// This is a first pass, but it could be disabled further down.
	bool tex_is_rt = (m_process_texture && m_cached_ctx.TEX0.TBP0 >= m_cached_ctx.FRAME.Block() &&
		m_cached_ctx.TEX0.TBP0 < frame_end_bp);
	bool preserve_rt_rgb = (!no_rt && (!IsDiscardingDstRGB() || not_writing_to_all || tex_is_rt));
	bool preserve_rt_alpha =
		(!no_rt && (!IsDiscardingDstAlpha() || not_writing_to_all ||
					   (tex_is_rt && GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].trbpp != 24)));
	bool preserve_rt_color = preserve_rt_rgb || preserve_rt_alpha;


	// SW CLUT Render enable.
	bool force_preload = GSConfig.PreloadFrameWithGSData;
	if (GSConfig.UserHacks_CPUCLUTRender > 0 || GSConfig.UserHacks_GPUTargetCLUTMode != GSGPUTargetCLUTMode::Disabled)
	{
		const CLUTDrawTestResult result = (GSConfig.UserHacks_CPUCLUTRender == 2) ? PossibleCLUTDrawAggressive() : PossibleCLUTDraw();
		m_mem.m_clut.ClearDrawInvalidity();
		if (result == CLUTDrawTestResult::CLUTDrawOnCPU && GSConfig.UserHacks_CPUCLUTRender > 0)
		{
			if (SwPrimRender(*this, true, true))
			{
				GL_CACHE("HW: Possible clut draw, drawn with SwPrimRender()");
				return;
			}
		}
		else if (result != CLUTDrawTestResult::NotCLUTDraw)
		{
			// Force enable preloading if any of the existing data is needed.
			// e.g. NFSMW only writes the alpha channel, and needs the RGB preloaded.
			force_preload |= preserve_rt_color;
			if (preserve_rt_color)
				GL_INS("HW: Forcing preload due to partial/blended CLUT draw");
		}
	}

	if (!m_channel_shuffle && m_cached_ctx.FRAME.Block() == m_cached_ctx.TEX0.TBP0 &&
		IsPossibleChannelShuffle())
	{
		// Special post-processing effect
		GL_INS("HW: Possible channel shuffle effect detected");
		m_channel_shuffle = true;
		m_last_channel_shuffle_fbmsk = m_context->FRAME.FBMSK;
	}
	else if (IsSplitClearActive())
	{
		if (ContinueSplitClear())
		{
			GL_INS("HW: Skipping due to continued split clear, FBP %x FBW %u", m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBW);
			return;
		}
		else
		{
			FinishSplitClear();
		}
	}

	m_texture_shuffle = false;
	m_copy_16bit_to_target_shuffle = false;
	m_same_group_texture_shuffle = false;
	m_using_temp_z = false;

	const bool is_split_texture_shuffle = (m_split_texture_shuffle_pages > 0);
	if (is_split_texture_shuffle)
	{
		// Adjust the draw rectangle to the new page range, so we get the correct fb height.
		const GSVector4i new_r = GetSplitTextureShuffleDrawRect();
		GL_CACHE(
			"Split texture shuffle: FBP %x -> %x, TBP0 %x -> %x, draw %d,%d => %d,%d -> %d,%d => %d,%d",
			m_cached_ctx.FRAME.Block(), m_split_texture_shuffle_start_FBP * GS_BLOCKS_PER_PAGE,
			m_cached_ctx.TEX0.TBP0, m_split_texture_shuffle_start_TBP,
			m_r.x, m_r.y, m_r.z, m_r.w,
			new_r.x, new_r.y, new_r.z, new_r.w);
		m_r = new_r;

		// Adjust the scissor too, if it's in two parts, this will be wrong.
		m_context->scissor.in = new_r;

		// Fudge FRAME and TEX0 to point to the start of the shuffle.
		m_cached_ctx.TEX0.TBP0 = m_split_texture_shuffle_start_TBP;

		// We correct this again at the end of the split
		SetNewFRAME(m_split_texture_shuffle_start_FBP << 5, m_context->FRAME.FBW, m_cached_ctx.FRAME.PSM);

		// TEX0 may also be just using single width with offsets also, so let's deal with that.
		if (m_split_texture_shuffle_pages > 1 && !NextDrawMatchesShuffle())
		{
			if (m_context->FRAME.FBW != m_split_texture_shuffle_fbw && m_cached_ctx.TEX0.TBW == 1)
			{
				const GSLocalMemory::psm_t& frame_psm = GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM];
				// This is the final draw of the shuffle, so let's fudge the numbers
				// Need to update the final rect as it could be wrong.
				if (m_context->FRAME.FBW == 1 && m_split_texture_shuffle_fbw != m_context->FRAME.FBW)
				{
					m_r.x = 0; // Need to keep the X offset to calculate the shuffle.
					m_r.z = m_split_texture_shuffle_fbw * frame_psm.pgs.x;
					m_r.y = 0;
					m_r.w = std::min(1024U, m_split_texture_shuffle_pages_high * frame_psm.pgs.y); // Max we can shuffle is 1024 (512)

					//Fudge the scissor and frame
					m_context->scissor.in = m_r;

					SetNewFRAME(m_split_texture_shuffle_start_FBP << 5, m_split_texture_shuffle_fbw, m_cached_ctx.FRAME.PSM);
				}

				const int pages = m_split_texture_shuffle_pages + 1;
				const int width = m_split_texture_shuffle_fbw;
				const int height = (pages >= width) ? (pages / width) : 1;
				// We must update the texture size! It will likely be 64x64, which is no good, so let's fudge that.
				m_cached_ctx.TEX0.TW = std::ceil(std::log2(std::min(1024, width * tex_psm.pgs.x)));
				m_cached_ctx.TEX0.TH = std::ceil(std::log2(std::min(1024, height * tex_psm.pgs.y)));
				m_cached_ctx.TEX0.TBW = m_split_texture_shuffle_fbw;
			}

			m_vt.m_min.p.x = m_r.x;
			m_vt.m_min.p.y = m_r.y;
			m_vt.m_min.t.x = m_r.x;
			m_vt.m_min.t.y = m_r.y;
			m_vt.m_max.p.x = m_r.z;
			m_vt.m_max.p.y = m_r.w;
			m_vt.m_max.t.x = m_r.z;
			m_vt.m_max.t.y = m_r.w;
		}
	}

	if (!GSConfig.UserHacks_DisableSafeFeatures && is_possible_mem_clear)
	{
		GL_INS("HW: WARNING: Possible mem clear.");

		// We'll finish things off later.
		if (IsStartingSplitClear())
		{
			CleanupDraw(false);
			return;
		}

		const int get_next_ctx = m_env.PRIM.CTXT;
		const GSDrawingContext& next_ctx = m_env.CTXT[get_next_ctx];

		// Try to fix large single-page-wide draws.
		bool height_invalid = m_r.w >= 1024;
		const GSVector2i& pgs = GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].pgs;
		const bool width_change = next_ctx.FRAME.FBW > m_cached_ctx.FRAME.FBW && next_ctx.FRAME.FBP == m_cached_ctx.FRAME.FBP && next_ctx.FRAME.PSM == m_cached_ctx.FRAME.PSM;
		if (height_invalid && m_cached_ctx.FRAME.FBW <= 1 &&
			TryToResolveSinglePageFramebuffer(m_cached_ctx.FRAME, true))
		{
			ReplaceVerticesWithSprite(
				GetDrawRectForPages(m_cached_ctx.FRAME.FBW, m_cached_ctx.FRAME.PSM, (m_r.w + (pgs.y - 1)) / pgs.y),
				GSVector2i(1, 1));
			height_invalid = false;
		}
		else if (width_change)
		{
			const int num_pages = m_cached_ctx.FRAME.FBW * ((m_r.w + (pgs.y - 1)) / pgs.y);
			m_cached_ctx.FRAME.FBW = next_ctx.FRAME.FBW;

			ReplaceVerticesWithSprite(
				GetDrawRectForPages(m_cached_ctx.FRAME.FBW, m_cached_ctx.FRAME.PSM, num_pages),
				GSVector2i(1, 1));
		}

		const u32 vert_index = (m_vt.m_primclass == GS_TRIANGLE_CLASS) ? 2 : 1;
		u32 const_color = m_vertex.buff[m_index.buff[vert_index]].RGBAQ.U32[0];
		u32 fb_mask = m_cached_ctx.FRAME.FBMSK;

		// If we could just check the colour, it would be great, but Echo Night decided it's going to set the alpha and green to 128, for some reason, and actually be 32bit, so it ruined my day.
		GSTextureCache::Target* rt_tgt = g_texture_cache->GetExactTarget(m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBW, GSTextureCache::RenderTarget, m_cached_ctx.FRAME.Block() + 1);
		const bool clear_16bit_likely = !(context->FRAME.PSM & 0x2) && ((rt_tgt && (rt_tgt->m_TEX0.PSM & 2)) || (!rt_tgt && ((static_cast<int>(context->FRAME.FBW) * 64) <= (PCRTCDisplays.GetResolution().x >> 1) || m_r.height() <= (PCRTCDisplays.GetResolution().y >> 1))));

		rt_tgt = nullptr;

		if (clear_16bit_likely && ((const_color != 0 && (const_color >> 16) == (const_color & 0xFFFF) && ((const_color >> 8) & 0xFF) != (const_color & 0xFF)) ||
												(fb_mask != 0 && (fb_mask >> 16) == (fb_mask & 0xFFFF) && ((fb_mask >> 8) & 0xFF) != (fb_mask & 0xFF))))
		{

			GL_CACHE("Clear 16bit with 32bit %d", s_n);

			// May have already been resized through the split draw checks.
			if (!(m_cached_ctx.FRAME.PSM & 2))
			{
				if (next_ctx.FRAME.FBW == (m_cached_ctx.FRAME.FBW * 2))
				{
					m_cached_ctx.FRAME.FBW *= 2;
					m_r.z *= 2;
				}
				else
				{
					m_r.w *= 2;
				}
			}

			// Convert colour and masks to 16bit and set a custom TEXA for this draw.
			const_color = ((const_color & 0x1F) << 3) | ((const_color & 0x3E0) << 6) | ((const_color & 0x7C00) << 9) | ((const_color & 0x8000) << 16);
			m_cached_ctx.FRAME.FBMSK = ((fb_mask & 0x1F) << 3) | ((fb_mask & 0x3E0) << 6) | ((fb_mask & 0x7C00) << 9) | ((fb_mask & 0x8000) << 16);
			m_cached_ctx.TEXA.AEM = 0;
			m_cached_ctx.TEXA.TA0 = 0;
			m_cached_ctx.TEXA.TA1 = 128;
			m_cached_ctx.FRAME.PSM = (m_cached_ctx.FRAME.PSM & 2) ? m_cached_ctx.FRAME.PSM : PSMCT16;
			m_vertex.buff[m_index.buff[1]].RGBAQ.U32[0] = const_color;
			ReplaceVerticesWithSprite(m_r, GSVector2i(m_r.width(), m_r.height()));
		}

		// Be careful of being 1 pixel from filled.
		const bool page_aligned = (m_r.w % pgs.y) == (pgs.y - 1) || (m_r.w % pgs.y) == 0;
		const bool is_zero_color_clear = (GetConstantDirectWriteMemClearColor() == 0 && !preserve_rt_color && page_aligned);
		const bool is_zero_depth_clear = (GetConstantDirectWriteMemClearDepth() == 0 && !preserve_depth && page_aligned);

		// If it's an invalid-sized draw, do the mem clear on the CPU, we don't want to create huge targets.
		// If clearing to zero, don't bother creating the target. Games tend to clear more than they use, wasting VRAM/bandwidth.
		if (is_zero_color_clear || is_zero_depth_clear || height_invalid)
		{
			u32 rt_end_bp = GSLocalMemory::GetUnwrappedEndBlockAddress(
				m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBW, m_cached_ctx.FRAME.PSM, m_r);
			const u32 ds_end_bp = GSLocalMemory::GetUnwrappedEndBlockAddress(
				m_cached_ctx.ZBUF.Block(), m_cached_ctx.FRAME.FBW, m_cached_ctx.ZBUF.PSM, m_r);

			// This can get missed by the double half clear, but we can make sure we nuke everything inside if the Z is butted up against the FRAME.
			if (!no_ds && (rt_end_bp + 1) == m_cached_ctx.ZBUF.Block() && GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].trbpp == GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].trbpp)
				rt_end_bp = ds_end_bp;

			// If this is a partial clear of a larger buffer, we can't invalidate the target, since we'll be losing data
			// which only existed on the GPU. Assume a BW change is a new target, though. Test case: Persona 3 shadows.
			GSTextureCache::Target* tgt;
			const bool overwriting_whole_rt =
				(no_rt || height_invalid ||
					(tgt = g_texture_cache->GetExactTarget(m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBW,
						 GSTextureCache::RenderTarget, rt_end_bp)) == nullptr ||
					m_r.rintersect(tgt->m_valid).eq(tgt->m_valid));
			const bool overwriting_whole_ds =
				(no_ds || height_invalid ||
					(tgt = g_texture_cache->GetExactTarget(m_cached_ctx.ZBUF.Block(), m_cached_ctx.FRAME.FBW,
						 GSTextureCache::DepthStencil, ds_end_bp)) == nullptr ||
					m_r.rintersect(tgt->m_valid).eq(tgt->m_valid));

			if (g_texture_cache->GetTemporaryZ() != nullptr && ((m_cached_ctx.FRAME.FBMSK != 0xFFFFFFFF && m_cached_ctx.FRAME.Block() == g_texture_cache->GetTemporaryZInfo().ZBP) || (!m_cached_ctx.ZBUF.ZMSK && m_cached_ctx.ZBUF.Block() == g_texture_cache->GetTemporaryZInfo().ZBP)))
			{
				g_texture_cache->InvalidateTemporaryZ();
			}

			if (overwriting_whole_rt && overwriting_whole_ds &&
				TryGSMemClear(no_rt, preserve_rt_color, is_zero_color_clear, rt_end_bp,
					no_ds, preserve_depth, is_zero_depth_clear, ds_end_bp))
			{
				GL_INS("HW: Skipping (%d,%d=>%d,%d) draw at FBP %x/ZBP %x due to invalid height or zero clear.", m_r.x, m_r.y,
					m_r.z, m_r.w, m_cached_ctx.FRAME.Block(), m_cached_ctx.ZBUF.Block());

				// Since we're not creating a target here, if this is the first draw to the target, it's not going
				// to be in the height cache, and we might create a smaller size target. We also need to record
				// it for HW moves (e.g. Devil May Cry subtitles).
				if (!height_invalid)
				{
					const GSVector2i target_size = GetValidSize(nullptr);
					if (!no_rt && is_zero_color_clear)
					{
						g_texture_cache->GetTargetSize(m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBW, m_cached_ctx.FRAME.PSM,
							target_size.x, target_size.y);
					}
					if (!no_ds && is_zero_depth_clear)
					{
						g_texture_cache->GetTargetSize(m_cached_ctx.ZBUF.Block(), m_cached_ctx.FRAME.FBW, m_cached_ctx.ZBUF.PSM,
							target_size.x, target_size.y);
					}
				}

				CleanupDraw(false);
				return;
			}
		}
	}

	GIFRegTEX0 TEX0 = {};
	GSTextureCache::Source* src = nullptr;
	TextureMinMaxResult tmm;
	bool possible_shuffle = false;
	bool draw_uses_target = false;
	// Disable texture mapping if the blend is black and using alpha from vertex.
	if (m_process_texture)
	{
		GIFRegCLAMP MIP_CLAMP = m_cached_ctx.CLAMP;
		GSVector2i hash_lod_range(0, 0);
		m_lod = GSVector2i(0, 0);

		// Code from the SW renderer
		if (IsMipMapActive())
		{
			const int interpolation = (context->TEX1.MMIN & 1) + 1; // 1: round, 2: tri

			int k = (m_context->TEX1.K + 8) >> 4;
			int lcm = m_context->TEX1.LCM;
			const int mxl = std::min<int>(static_cast<int>(m_context->TEX1.MXL), 6);

			if (static_cast<int>(m_vt.m_lod.x) >= mxl)
			{
				k = mxl; // set lod to max level
				lcm = 1; // constant lod
			}

			if (PRIM->FST)
			{
				pxAssert(lcm == 1);
				//pxAssert(((m_vt.m_min.t.uph(m_vt.m_max.t) == GSVector4::zero()).mask() & 3) == 3); // ratchet and clank (menu)

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
					m_lod.x = std::max<int>(static_cast<int>(floor(m_vt.m_lod.x)), 0);
				}
				else
				{
					// On GS lod is a fixed float number 7:4 (4 bit for the frac part)
#if 0
					m_lod.x = std::max<int>(static_cast<int>(round(m_vt.m_lod.x + 0.0625)), 0);
#else
					// Same as above with a bigger margin on rounding
					// The goal is to avoid 1 undrawn pixels around the edge which trigger the load of the big
					// layer.
					if (ceil(m_vt.m_lod.x) < m_vt.m_lod.y)
						m_lod.x = std::max<int>(static_cast<int>(round(m_vt.m_lod.x + 0.0625 + 0.01)), 0);
					else
						m_lod.x = std::max<int>(static_cast<int>(round(m_vt.m_lod.x + 0.0625)), 0);
#endif
				}

				m_lod.y = std::max<int>(static_cast<int>(ceil(m_vt.m_lod.y)), 0);
			}

			m_lod.x = std::min<int>(m_lod.x, mxl);
			m_lod.y = std::min<int>(m_lod.y, mxl);

			TEX0 = (m_lod.x == 0) ? m_cached_ctx.TEX0 : GetTex0Layer(m_lod.x);

			// upload the full chain (with offset) for the hash cache, in case some other texture uses more levels
			// for basic mipmapping, we can get away with just doing the base image, since all the mips get generated anyway.
			hash_lod_range = GSVector2i(m_lod.x, GSConfig.HWMipmap ? mxl : m_lod.x);

			MIP_CLAMP.MINU >>= m_lod.x;
			MIP_CLAMP.MINV >>= m_lod.x;
			MIP_CLAMP.MAXU >>= m_lod.x;
			MIP_CLAMP.MAXV >>= m_lod.x;

			for (int i = 0; i < m_lod.x; i++)
			{
				m_vt.m_min.t *= 0.5f;
				m_vt.m_max.t *= 0.5f;
			}

			GL_CACHE("HW: Mipmap LOD %d %d (%f %f) new size %dx%d (K %d L %u)", m_lod.x, m_lod.y, m_vt.m_lod.x, m_vt.m_lod.y, 1 << TEX0.TW, 1 << TEX0.TH, m_context->TEX1.K, m_context->TEX1.L);
		}
		else
		{
			TEX0 = m_cached_ctx.TEX0;
		}

		tmm = GetTextureMinMax(TEX0, MIP_CLAMP, m_vt.IsLinear(), false);

		// Snowblind games set TW/TH to 1024, and use UVs for smaller textures inside that.
		// Such textures usually contain junk in local memory, so try to make them smaller based on UVs.
		// We can only do this for UVs, because ST repeat won't be correct.

		if (GSConfig.UserHacks_EstimateTextureRegion && // enabled
			(PRIM->FST || (MIP_CLAMP.WMS == CLAMP_CLAMP && MIP_CLAMP.WMT == CLAMP_CLAMP)) && // UV or ST with clamp
			TEX0.TW >= 9 && TEX0.TH >= 9 && // 512x512
			MIP_CLAMP.WMS < CLAMP_REGION_CLAMP && MIP_CLAMP.WMT < CLAMP_REGION_CLAMP && // not using custom region
			((m_vt.m_max.t >= GSVector4(512.0f)).mask() & 0x3) == 0) // If the UVs actually are large, don't optimize.
		{
			// Clamp to the UVs of the texture. We could align this to something, but it ends up working better to just duplicate
			// for different sizes in the hash cache, rather than hashing more and duplicating based on local memory.
			const GSVector4i maxt(m_vt.m_max.t + GSVector4(m_vt.IsLinear() ? 0.5f : 0.0f));
			MIP_CLAMP.WMS = CLAMP_REGION_CLAMP;
			MIP_CLAMP.WMT = CLAMP_REGION_CLAMP;
			MIP_CLAMP.MINU = 0;
			MIP_CLAMP.MAXU = maxt.x >> m_lod.x;
			MIP_CLAMP.MINV = 0;
			MIP_CLAMP.MAXV = maxt.y >> m_lod.x;
			GL_CACHE("HW: Estimated texture region: %u,%u -> %u,%u", MIP_CLAMP.MINU, MIP_CLAMP.MINV, MIP_CLAMP.MAXU + 1,
				MIP_CLAMP.MAXV + 1);
		}

		GIFRegTEX0 FRAME_TEX0;
		bool shuffle_target = false;
		const u32 page_alignment = GSLocalMemory::IsPageAlignedMasked(m_cached_ctx.TEX0.PSM, m_r);
		const bool page_aligned = (page_alignment & 0xF0F0) != 0; // Make sure Y is page aligned.
		if (!no_rt && page_aligned && m_cached_ctx.ZBUF.ZMSK && GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].bpp == 16 && GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].bpp >= 16 &&
			(m_vt.m_primclass == GS_SPRITE_CLASS || (m_vt.m_primclass == GS_TRIANGLE_CLASS && (m_index.tail % 6) == 0 && TrianglesAreQuads(true) && m_index.tail > 6)))
		{
			// Tail check is to make sure we have enough strips to go all the way across the page, or if it's using a region clamp could be used to draw strips.
			if (GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].bpp == 16 &&
				(m_index.tail >= (m_cached_ctx.TEX0.TBW * 2) || m_cached_ctx.TEX0.TBP0 == m_cached_ctx.FRAME.Block() || m_cached_ctx.CLAMP.WMS > CLAMP_CLAMP || m_cached_ctx.CLAMP.WMT > CLAMP_CLAMP))
			{
				const GSVertex* v = &m_vertex.buff[0];

				const int first_x = std::clamp((static_cast<int>(((v[0].XYZ.X - m_context->XYOFFSET.OFX) + 8))) >> 4, 0, 2048);
				const bool offset_last = PRIM->FST ? (v[1].U > v[0].U) : ((v[1].ST.S / v[1].RGBAQ.Q) > (v[0].ST.S / v[1].RGBAQ.Q));
				const int first_u = PRIM->FST ? ((v[0].U + (offset_last ? 0 : 9)) >> 4) : std::clamp(static_cast<int>(((1 << m_cached_ctx.TEX0.TW) * (v[0].ST.S / v[1].RGBAQ.Q)) + (offset_last ? 0.0f : 0.6f)), 0, 2048);
				const int second_u = PRIM->FST ? ((v[1].U + (offset_last ? 9 : 0)) >> 4) : std::clamp(static_cast<int>(((1 << m_cached_ctx.TEX0.TW) * (v[1].ST.S / v[1].RGBAQ.Q)) + (offset_last ? 0.6f : 0.0f)), 0, 2048);
				// offset coordinates swap around RG/BA. (Ace Combat)
				const u32 minv = m_cached_ctx.CLAMP.MINV;
				const u32 minu = m_cached_ctx.CLAMP.MINU;
				// Make sure minu or minv are actually a mask on some bits, false positives of games setting 512 (0x1ff) are not masks used for shuffles.
				const bool rgba_shuffle = ((m_cached_ctx.CLAMP.WMS == m_cached_ctx.CLAMP.WMT && m_cached_ctx.CLAMP.WMS == CLAMP_REGION_REPEAT) && (minu && minv && ((minu + 1 & minu) || (minv + 1 & minv))));
				const bool shuffle_coords = ((first_x ^ first_u) & 0xF) == 8 || rgba_shuffle;

				// Round up half of second coord, it can sometimes be slightly under.
				const int draw_width = std::abs(v[1].XYZ.X + 9 - v[0].XYZ.X) >> 4;
				const int read_width = std::abs(second_u - first_u);

				// m_skip check is just mainly for NFS Undercover, but should hopefully pick up any other games which rewrite shuffles.
				shuffle_target = shuffle_coords && (((draw_width & 7) == 0 && std::abs(draw_width - read_width) <= 1) || m_skip > 50);
			}

			// It's possible it's writing to an old 32bit target, but is actually just a 16bit copy, so let's make sure it's actually using a mask.
			if (!shuffle_target)
			{
				bool shuffle_channel_reads = !m_cached_ctx.FRAME.FBMSK;
				const u32 increment = (m_vt.m_primclass == GS_TRIANGLE_CLASS) ? 3 : 2;
				const GSVertex* v = &m_vertex.buff[0];

				if (shuffle_channel_reads)
				{
					for (u32 i = 0; i < m_index.tail; i += increment)
					{
						const int first_u = (PRIM->FST ? v[i].U : static_cast<int>(v[i].ST.S / v[(increment == 2) ? i + 1 : i].RGBAQ.Q)) >> 4;
						const int second_u = (PRIM->FST ? v[i + 1].U : static_cast<int>(v[i + 1].ST.S / v[i + 1].RGBAQ.Q)) >> 4;
						const int vector_width = std::abs(v[i + 1].XYZ.X - v[i].XYZ.X) / 16;
						const int tex_width = std::abs(second_u - first_u);
						// & 7 just a quicker way of doing % 8
						if ((vector_width & 7) != 0 || (tex_width & 7) != 0 || tex_width != vector_width)
						{
							shuffle_channel_reads = false;
							break;
						}
					}
				}
				if (m_cached_ctx.FRAME.FBMSK || shuffle_channel_reads)
				{
					// FBW is going to be wrong for channel shuffling into a new target, so take it from the source.
					FRAME_TEX0.U64 = 0;
					FRAME_TEX0.TBP0 = m_cached_ctx.FRAME.Block();
					FRAME_TEX0.TBW = m_cached_ctx.FRAME.FBW;
					FRAME_TEX0.PSM = m_cached_ctx.FRAME.PSM;

					GSTextureCache::Target* tgt = g_texture_cache->FindOverlappingTarget(FRAME_TEX0.TBP0, GSLocalMemory::GetEndBlockAddress(FRAME_TEX0.TBP0, FRAME_TEX0.TBW, FRAME_TEX0.PSM, m_r));

					if (tgt)
						shuffle_target = tgt->m_32_bits_fmt;
					else
						shuffle_target = shuffle_channel_reads;

					tgt = nullptr;
				}
			}
		}

		possible_shuffle = !no_rt && (((shuffle_target /*&& GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].bpp == 16*/) /*|| (m_cached_ctx.FRAME.Block() == m_cached_ctx.TEX0.TBP0 && ((m_cached_ctx.TEX0.PSM & 0x6) || m_cached_ctx.FRAME.PSM != m_cached_ctx.TEX0.PSM))*/) || IsPossibleChannelShuffle());
		const bool need_aem_color = GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].trbpp <= 24 && GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].pal == 0 && ((NeedsBlending() && m_context->ALPHA.C == 0) || IsDiscardingDstAlpha()) && m_cached_ctx.TEXA.AEM;
		const u32 color_mask = (m_vt.m_max.c > GSVector4i::zero()).mask();
		const bool texture_function_color = m_cached_ctx.TEX0.TFX == TFX_DECAL || (color_mask & 0xFFF) || (m_cached_ctx.TEX0.TFX > TFX_DECAL && (color_mask & 0xF000));
		const bool texture_function_alpha = m_cached_ctx.TEX0.TFX != TFX_MODULATE || (color_mask & 0xF000);
		const bool req_color = (texture_function_color && (!PRIM->ABE || GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].bpp < 16 || (NeedsBlending() && IsUsingCsInBlend())) && (possible_shuffle || (m_cached_ctx.FRAME.FBMSK & (fm_mask & 0x00FFFFFF)) != (fm_mask & 0x00FFFFFF))) || need_aem_color;
		const bool alpha_used = (GSUtil::GetChannelMask(m_context->TEX0.PSM) == 0x8 || (m_context->TEX0.TCC && texture_function_alpha)) && ((NeedsBlending() && IsUsingAsInBlend()) || (m_cached_ctx.TEST.ATE && m_cached_ctx.TEST.ATST > ATST_ALWAYS) || (possible_shuffle || (m_cached_ctx.FRAME.FBMSK & (fm_mask & 0xFF000000)) != (fm_mask & 0xFF000000)));
		const bool req_alpha = (GSUtil::GetChannelMask(m_context->TEX0.PSM) & 0x8) && alpha_used;

		// TODO: Be able to send an alpha of 1.0 (blended with vertex alpha maybe?) so we can avoid sending the texture, since we don't always need it.
		// Example games: Evolution Snowboarding, Final Fantasy Dirge of Cerberus, Red Dead Revolver, Stuntman, Tony Hawk's Underground 2, Ultimate Spider-Man.
		if (!req_color && !alpha_used)
		{
			m_process_texture = false;
			possible_shuffle = false;
		}
		else
		{
			src = tex_psm.depth ? g_texture_cache->LookupDepthSource(true, TEX0, m_cached_ctx.TEXA, MIP_CLAMP, tmm.coverage, possible_shuffle, m_vt.IsLinear(), m_cached_ctx.FRAME, req_color, req_alpha)
			                    : g_texture_cache->LookupSource(true, TEX0, m_cached_ctx.TEXA, MIP_CLAMP, tmm.coverage, (GSConfig.HWMipmap || GSConfig.TriFilter == TriFiltering::Forced) ? &hash_lod_range : nullptr,
			                         possible_shuffle, m_vt.IsLinear(), m_cached_ctx.FRAME, req_color, req_alpha);

			if (!src) [[unlikely]]
			{
				GL_INS("HW: ERROR: Source lookup failed, skipping.");
				CleanupDraw(true);
				return;
			}



			const u32 draw_end = GSLocalMemory::GetEndBlockAddress(m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBW, m_cached_ctx.FRAME.PSM, m_r) + 1;
			const u32 draw_start = GSLocalMemory::GetStartBlockAddress(m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBW, m_cached_ctx.FRAME.PSM, m_r);
			draw_uses_target = src->m_from_target && ((src->m_from_target_TEX0.TBP0 <= draw_start && src->m_from_target->UnwrappedEndBlock() > m_cached_ctx.FRAME.Block()) ||
			                                          (m_cached_ctx.FRAME.Block() < src->m_from_target_TEX0.TBP0 && draw_end > src->m_from_target_TEX0.TBP0));

			if (possible_shuffle && GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].bpp != 16)
				possible_shuffle &= draw_uses_target;

			const bool shuffle_source = possible_shuffle && src && ((src->m_from_target != nullptr && GSLocalMemory::m_psm[src->m_from_target->m_TEX0.PSM].bpp != 16) || m_skip);

			if (!shuffle_source && possible_shuffle)
			{
				const bool is_16bit_copy = m_cached_ctx.TEX0.TBP0 != m_cached_ctx.FRAME.Block() && shuffle_target && IsOpaque() && !(context->TEX1.MMIN & 1) && !src->m_32_bits_fmt && m_cached_ctx.FRAME.FBMSK;
				possible_shuffle &= is_16bit_copy || (m_cached_ctx.TEX0.TBP0 == m_cached_ctx.FRAME.Block() && shuffle_target);
			}
			// We don't know the alpha range of direct sources when we first tried to optimize the alpha test.
			// Moving the texture lookup before the ATST optimization complicates things a lot, so instead,
			// recompute it, and everything derived from it again if it changes.
			// No channel shuffle as the alpha of a target used a source is meaningless to us,
			// since it's not really an indexed texture.
			if (!IsPossibleChannelShuffle() && src->m_valid_alpha_minmax)
			{
				CalcAlphaMinMax(src->m_alpha_minmax.first, src->m_alpha_minmax.second);

				u32 new_fm = m_context->FRAME.FBMSK;
				u32 new_zm = (m_cached_ctx.ZBUF.ZMSK || m_cached_ctx.TEST.ZTE == 0 ||
				             (m_cached_ctx.TEST.ATE && m_cached_ctx.TEST.ATST == ZTST_NEVER && m_cached_ctx.TEST.AFAIL != AFAIL_ZB_ONLY)) ?
				             0xffffffffu :
				             0;
				if (m_cached_ctx.TEST.ATE && GSRenderer::TryAlphaTest(new_fm, new_zm))
				{
					m_cached_ctx.TEST.ATE = false;
					m_cached_ctx.FRAME.FBMSK = new_fm;
					m_cached_ctx.ZBUF.ZMSK = (new_zm != 0);
					fm = new_fm;
					zm = new_zm;
					no_rt = no_rt || (!m_cached_ctx.TEST.DATE && !IsRTWritten());
					no_ds = no_ds || (zm != 0 && all_depth_tests_pass) ||
					        // Depth will be written through the RT
					        (!no_rt && m_cached_ctx.FRAME.FBP == m_cached_ctx.ZBUF.ZBP && !PRIM->TME && zm == 0 && (fm & fm_mask) == 0 && m_cached_ctx.TEST.ZTE) ||
					        // No color or Z being written.
					        (no_rt && zm != 0);
				}
				else
				{
					no_rt = no_rt || (!m_cached_ctx.TEST.DATE && !IsRTWritten());
					no_ds = no_ds ||
					        // Depth will be written through the RT
					        (!no_rt && m_cached_ctx.FRAME.FBP == m_cached_ctx.ZBUF.ZBP && !PRIM->TME && zm == 0 && (fm & fm_mask) == 0 && m_cached_ctx.TEST.ZTE) ||
					        // No color or Z being written.
					        (no_rt && zm != 0);
				}

				if (no_rt && no_ds)
				{
					GL_INS("HW: Late draw cancel.");
					CleanupDraw(true);
					return;
				}
			}
		}
	}

	// Urban Reign trolls by scissoring a draw to a target at 0x0-0x117F to 378x449 which ends up the size being rounded up to 640x480
	// causing the buffer to expand to around 0x1400, which makes a later framebuffer at 0x1180 to fail to be created correctly.
	// We can cheese this by checking if the Z is masked and the resultant colour is going to be black anyway.
	const bool output_black = NeedsBlending() && ((m_context->ALPHA.A == 1 && m_context->ALPHA.D > 1) || (m_context->ALPHA.IsBlack() && m_context->ALPHA.D != 1)) && m_draw_env->COLCLAMP.CLAMP == 1;
	const bool can_expand = !(m_cached_ctx.ZBUF.ZMSK && output_black);

	// Estimate size based on the scissor rectangle and height cache.
	GSVector2i t_size = GetTargetSize(src, can_expand, possible_shuffle);
	const GSVector4i t_size_rect = GSVector4i::loadh(t_size);

	// Ensure draw rect is clamped to framebuffer size. Necessary for updating valid area.
	const GSVector4i unclamped_draw_rect = m_r;

	float target_scale = GetTextureScaleFactor();
	bool scaled_copy = false;
	int scale_draw = IsScalingDraw(src, m_primitive_covers_without_gaps != NoGapsType::GapsFound);
	if (GSConfig.UserHacks_NativeScaling != GSNativeScaling::Off)
	{
		if (target_scale > 1.0f && scale_draw > 0)
		{
			// 1 == Downscale, so we need to reduce the size of the target also.
			// 2 == Upscale, so likely putting it over the top of the render target.
			if (scale_draw == 1)
			{
				if (!PRIM->ABE || GSConfig.UserHacks_NativeScaling < GSNativeScaling::NormalUpscaled)
					target_scale = 1.0f;
				m_downscale_source = src->m_from_target ? src->m_from_target->GetScale() > 1.0f : false;
			}
			else
				m_downscale_source = ((GSConfig.UserHacks_NativeScaling != GSNativeScaling::Aggressive && GSConfig.UserHacks_NativeScaling != GSNativeScaling::AggressiveUpscaled) || !src->m_from_target) ? false : src->m_from_target->GetScale() > 1.0f; // Bad for GTA + Full Spectrum Warrior, good for Sacred Blaze + Parappa.
		}
		else
		{
			// if it's directly copying keep the scale - Ratchet and clank hits this, stops edge garbage happening.
			// Keep it to small targets of 256 or lower.
			if (scale_draw == -1 && src && (!src->m_from_target || (src->m_from_target && src->m_from_target->m_downscaled)) && ((static_cast<int>(m_cached_ctx.FRAME.FBW * 64) <= (PCRTCDisplays.GetResolution().x >> 1) &&
				(GSVector4i(m_vt.m_min.p).xyxy() == GSVector4i(m_vt.m_min.t).xyxy()).alltrue() && (GSVector4i(m_vt.m_max.p).xyxy() == GSVector4i(m_vt.m_max.t).xyxy()).alltrue()) || possible_shuffle))
			{
				target_scale = src->m_from_target ? src->m_from_target->GetScale() : 1.0f;
				scale_draw = 1;
				scaled_copy = true;
			}

			m_downscale_source = false;
		}
	}

	if (IsPossibleChannelShuffle() && src && src->m_from_target && src->m_from_target->GetScale() != target_scale)
	{
		target_scale = src->m_from_target->GetScale();
	}
	// This upscaling hack is for games which construct P8 textures by drawing a bunch of small sprites in C32,
	// then reinterpreting it as P8. We need to keep the off-screen intermediate textures at native resolution,
	// but not propagate that through to the normal render targets. Test Case: Crash Wrath of Cortex.
	if (no_ds && src && !m_channel_shuffle && src->m_from_target && (GSConfig.UserHacks_NativePaletteDraw || (src->m_target_direct  && src->m_from_target->m_downscaled && scale_draw <= 1)) &&
		src->m_scale == 1.0f && (src->m_TEX0.PSM == PSMT8 || src->m_TEX0.TBP0 == m_cached_ctx.FRAME.Block()))
	{
		GL_CACHE("HW: Using native resolution for target based on texture source");
		target_scale = 1.0f;
	}

	if (!m_process_texture && tex_is_rt)
	{
		tex_is_rt = (m_process_texture && m_cached_ctx.TEX0.TBP0 >= m_cached_ctx.FRAME.Block() &&
			m_cached_ctx.TEX0.TBP0 < frame_end_bp);
		preserve_rt_rgb = (!no_rt && (!IsDiscardingDstRGB() || not_writing_to_all || tex_is_rt));
		preserve_rt_alpha =
			(!no_rt && (!IsDiscardingDstAlpha() || not_writing_to_all ||
				(tex_is_rt && GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].trbpp != 24)));
		preserve_rt_color = preserve_rt_rgb || preserve_rt_alpha;
	}

	GSTextureCache::Target* rt = nullptr;
	GIFRegTEX0 FRAME_TEX0;
	const GSLocalMemory::psm_t& frame_psm = GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM];

	m_in_target_draw = false;
	m_target_offset = 0;

	GSTextureCache::Target* ds = nullptr;
	GIFRegTEX0 ZBUF_TEX0;
	ZBUF_TEX0.U64 = 0;

	if (!no_ds)
	{
		ZBUF_TEX0.TBP0 = m_cached_ctx.ZBUF.Block();
		ZBUF_TEX0.TBW = m_cached_ctx.FRAME.FBW;
		ZBUF_TEX0.PSM = m_cached_ctx.ZBUF.PSM;

		ds = g_texture_cache->LookupTarget(ZBUF_TEX0, t_size, target_scale, GSTextureCache::DepthStencil,
			m_cached_ctx.DepthWrite(), 0, false, force_preload, preserve_depth, preserve_depth, unclamped_draw_rect, IsPossibleChannelShuffle(), is_possible_mem_clear && ZBUF_TEX0.TBP0 != m_cached_ctx.FRAME.Block(), false,
			src, nullptr, -1);

		ZBUF_TEX0.TBW = m_channel_shuffle ? src->m_from_target_TEX0.TBW : m_cached_ctx.FRAME.FBW;

		if (!ds && m_cached_ctx.FRAME.FBP != m_cached_ctx.ZBUF.ZBP)
		{
			ds = g_texture_cache->CreateTarget(ZBUF_TEX0, t_size, GetValidSize(src, possible_shuffle), target_scale, GSTextureCache::DepthStencil,
				true, 0, false, force_preload, preserve_depth, m_r, src);
			if (!ds) [[unlikely]]
			{
				GL_INS("HW: ERROR: Failed to create ZBUF target, skipping.");
				CleanupDraw(true);
				return;
			}
		}
		else
		{
			// If it failed to check depth test earlier, we can now check the top bits from the alpha to get a bit more accurate picture.
			if (((zm && m_cached_ctx.TEST.ZTST > ZTST_ALWAYS) || (m_vt.m_eq.z && m_cached_ctx.TEST.ZTST == ZTST_GEQUAL)) && GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].trbpp == 32)
			{
				if (ds->m_alpha_max != 0)
				{
					const u32 max_z = (static_cast<u64>(ds->m_alpha_max + 1) << 24) - 1;

					switch (m_cached_ctx.TEST.ZTST)
					{
						case ZTST_GEQUAL:
							// Every Z value will pass
							if (max_z <= m_vt.m_min.p.z)
							{
								m_cached_ctx.TEST.ZTST = ZTST_ALWAYS;
								if (zm)
								{
									ds = nullptr;
									no_ds = true;
								}
							}
							break;
						case ZTST_GREATER:
							// Every Z value will pass
							if (max_z < m_vt.m_min.p.z)
							{
								m_cached_ctx.TEST.ZTST = ZTST_ALWAYS;
								if (zm)
								{
									ds = nullptr;
									no_ds = true;
								}
							}
							break;
						default:
							break;
					}
				}
			}
		}

		if (no_rt && ds && ds->m_TEX0.TBP0 != m_cached_ctx.ZBUF.Block())
		{
			const GSLocalMemory::psm_t& zbuf_psm = GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM];
			int vertical_offset = ((static_cast<int>(m_cached_ctx.ZBUF.Block() - ds->m_TEX0.TBP0) / 32) / std::max(static_cast<int>(ds->m_TEX0.TBW), 1)) * zbuf_psm.pgs.y; // I know I could just not shift it..
			int texture_offset = 0;
			int horizontal_offset = ((static_cast<int>((m_cached_ctx.ZBUF.Block() - ds->m_TEX0.TBP0)) / 32) % static_cast<int>(std::max(ds->m_TEX0.TBW, 1U))) * zbuf_psm.pgs.x;
			// Used to reduce the offset made later in channel shuffles
			m_target_offset = std::abs(static_cast<int>((m_cached_ctx.ZBUF.Block() - ds->m_TEX0.TBP0)) >> 5);

			if (vertical_offset < 0)
			{
				ds->m_TEX0.TBP0 = m_cached_ctx.ZBUF.Block();
				GSVector2i new_size = ds->m_unscaled_size;
				// Make sure to use the original format for the offset.
				const int new_offset = std::abs((vertical_offset / zbuf_psm.pgs.y) * GSLocalMemory::m_psm[ds->m_TEX0.PSM].pgs.y);
				texture_offset = new_offset;

				new_size.y += new_offset;

				const GSVector4i new_drect = GSVector4i(0, new_offset * ds->m_scale, new_size.x * ds->m_scale, new_size.y * ds->m_scale);
				ds->ResizeTexture(new_size.x, new_size.y, true, true, new_drect);

				if (src && src->m_from_target && src->m_from_target == ds && src->m_target_direct)
				{
					src->m_texture = ds->m_texture;

					// If we've moved it and the source is expecting to be inside this target, we need to update the region to point to it.
					int max_region_y = src->m_region.GetMaxY() + new_offset;
					if (max_region_y == new_offset)
						max_region_y = new_size.y;

					src->m_region.SetY(src->m_region.GetMinY() + new_offset, max_region_y);
				}

				ds->m_valid.y += new_offset;
				ds->m_valid.w += new_offset;
				ds->m_drawn_since_read.y += new_offset;
				ds->m_drawn_since_read.w += new_offset;

				g_texture_cache->CombineAlignedInsideTargets(ds, src);

				if (ds->m_dirty.size())
				{
					for (int i = 0; i < static_cast<int>(ds->m_dirty.size()); i++)
					{
						ds->m_dirty[i].r.y += new_offset;
						ds->m_dirty[i].r.w += new_offset;
					}
				}

				t_size.y += std::abs(vertical_offset);
				vertical_offset = 0;
			}

			if (horizontal_offset < 0)
			{
				// Thankfully this doesn't really happen, but catwoman moves the framebuffer backwards 1 page with a channel shuffle, which is really messy and not easy to deal with.
				// Hopefully the quick channel shuffle will just guess this and run with it.
				ds->m_TEX0.TBP0 += horizontal_offset;
				horizontal_offset = 0;
			}

			if (vertical_offset || horizontal_offset)
			{
				GSVertex* v = &m_vertex.buff[0];

				for (u32 i = 0; i < m_vertex.tail; i++)
				{
					v[i].XYZ.X += horizontal_offset << 4;
					v[i].XYZ.Y += vertical_offset << 4;
				}

				if (texture_offset && src && src->m_from_target && src->m_target_direct && src->m_from_target == ds)
				{
					GSVector4i src_region = src->GetRegionRect();

					if (src_region.rempty())
					{
						src_region = GSVector4i::loadh(ds->m_unscaled_size);
						src_region.y += texture_offset;
					}
					else
					{
						src_region.y += texture_offset;
						src_region.w += texture_offset;
					}
					src->m_region.SetX(src_region.x, src_region.z);
					src->m_region.SetY(src_region.y, src_region.w);
				}

				m_context->scissor.in.x += horizontal_offset;
				m_context->scissor.in.z += horizontal_offset;
				m_context->scissor.in.y += vertical_offset;
				m_context->scissor.in.w += vertical_offset;
				m_r.y += vertical_offset;
				m_r.w += vertical_offset;
				m_r.x += horizontal_offset;
				m_r.z += horizontal_offset;
				m_in_target_draw = ds->m_TEX0.TBP0 != m_cached_ctx.ZBUF.Block();
				m_vt.m_min.p.x += horizontal_offset;
				m_vt.m_max.p.x += horizontal_offset;
				m_vt.m_min.p.y += vertical_offset;
				m_vt.m_max.p.y += vertical_offset;

				t_size.y = ds->m_unscaled_size.y - vertical_offset;
				t_size.x = ds->m_unscaled_size.x - horizontal_offset;
			}

			// Don't resize if the BPP don't match.
			GSVector2i new_size = GetValidSize(src, possible_shuffle);
			if (new_size.x > ds->m_unscaled_size.x || new_size.y > ds->m_unscaled_size.y)
			{
				const u32 new_width = std::max(new_size.x, ds->m_unscaled_size.x);
				const u32 new_height = std::max(new_size.y, ds->m_unscaled_size.y);

				//DevCon.Warning("HW: Resizing texture %d x %d draw %d", ds->m_unscaled_size.x, new_height, s_n);
				ds->ResizeTexture(new_width, new_height);
			}
			else if ((IsPageCopy() || is_possible_mem_clear) && m_r.width() <= zbuf_psm.pgs.x && m_r.height() <= zbuf_psm.pgs.y)
			{
				const int get_next_ctx = m_env.PRIM.CTXT;
				const GSDrawingContext& next_ctx = m_env.CTXT[get_next_ctx];
				GSVector4i update_valid = GSVector4i::loadh(GSVector2i(horizontal_offset + GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].pgs.x, GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].pgs.y + vertical_offset));
				ds->UpdateValidity(update_valid, true);
				if (is_possible_mem_clear)
				{
					if ((horizontal_offset + GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].pgs.x) >= static_cast<int>(ds->m_TEX0.TBW * 64) && next_ctx.ZBUF.Block() == (m_cached_ctx.ZBUF.Block() + 0x20))
					{
						update_valid.x = 0;
						update_valid.z = GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].pgs.x;
						update_valid.y += GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].pgs.y;
						update_valid.w += GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].pgs.y;
						ds->UpdateValidity(update_valid, true);
					}
				}
			}
		}
	}

	if (!no_rt)
	{
		possible_shuffle |= draw_sprite_tex && m_process_texture && m_primitive_covers_without_gaps != NoGapsType::FullCover &&
		                    (((src && src->m_target && src->m_from_target && src->m_from_target->m_32_bits_fmt) &&
		                      (GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].bpp == 16 || draw_uses_target) &&
		                      GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].bpp == 16) ||
		                     IsPossibleChannelShuffle());

		const bool possible_horizontal_texture_shuffle = possible_shuffle && src && src->m_from_target && m_r.w <= src->m_from_target->m_valid.w && m_r.z > src->m_from_target->m_valid.z && m_cached_ctx.FRAME.FBW > src->m_from_target_TEX0.TBW;

		// FBW is going to be wrong for channel shuffling into a new target, so take it from the source.
		FRAME_TEX0.U64 = 0;
		FRAME_TEX0.TBP0 = ((m_last_channel_shuffle_end_block + 1) == m_cached_ctx.FRAME.Block() && possible_shuffle) ? m_last_channel_shuffle_fbp : m_cached_ctx.FRAME.Block();
		FRAME_TEX0.TBW = (possible_horizontal_texture_shuffle || (possible_shuffle && src && src->m_from_target && IsPossibleChannelShuffle() && m_cached_ctx.FRAME.FBW <= 2)) ? src->m_from_target_TEX0.TBW : m_cached_ctx.FRAME.FBW;
		FRAME_TEX0.PSM = m_cached_ctx.FRAME.PSM;

		// Don't clamp on shuffle, the height cache may troll us with the REAL height.
		if (!possible_shuffle && m_split_texture_shuffle_pages == 0)
			m_r = m_r.rintersect(t_size_rect);

		GSVector4i lookup_rect = unclamped_draw_rect;
		// Do the lookup with the real format on a shuffle, if possible.
		if (possible_shuffle && GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].bpp == 16 && GSLocalMemory ::m_psm[m_cached_ctx.FRAME.PSM].bpp == 16)
		{
			// Creating a new target on a shuffle, possible temp buffer, but let's try to get the real format.
			const int get_next_ctx = (m_state_flush_reason == CONTEXTCHANGE) ? m_env.PRIM.CTXT : m_backed_up_ctx;
			const GSDrawingContext& next_ctx = m_env.CTXT[get_next_ctx];

			if (next_ctx.FRAME.Block() == FRAME_TEX0.TBP0 && next_ctx.FRAME.PSM != FRAME_TEX0.PSM)
				FRAME_TEX0.PSM = next_ctx.FRAME.PSM;
			else if (next_ctx.TEX0.TBP0 == FRAME_TEX0.TBP0 && next_ctx.TEX0.PSM != FRAME_TEX0.PSM)
				FRAME_TEX0.PSM = next_ctx.TEX0.PSM;
			else
				FRAME_TEX0.PSM = PSMCT32; // Guess full color if no upcoming hint, it'll fix itself later.

			// This is just for overlap detection, it doesn't matter which direction we do this in
			if (GSLocalMemory::m_psm[FRAME_TEX0.PSM].bpp == 32 && src && src->m_from_target)
			{
				// Shuffling with a double width (Sonic Unleashed for example which does a wierd shuffle/not shuffle green backup/restore).
				if (std::abs((lookup_rect.width() / 2) - src->m_from_target->m_unscaled_size.x) <= 8)
				{
					lookup_rect.x /= 2;
					lookup_rect.z /= 2;
				}
				else
				{
					lookup_rect.y /= 2;
					lookup_rect.w /= 2;
				}
			}
		}

		// Normally we would use 1024 here to match the clear above, but The Godfather does a 1023x1023 draw instead
		// (very close to 1024x1024, but apparently the GS rounds down..). So, catch that here, we don't want to
		// create that target, because the clear isn't black, it'll hang around and never get invalidated.
		const bool is_large_rect = (t_size.y >= t_size.x) && m_r.w >= 1023 && m_primitive_covers_without_gaps == NoGapsType::FullCover;
		const bool is_clear = is_possible_mem_clear && is_large_rect;

		// Preserve downscaled target when copying directly from a downscaled target, or it's a normal draw using a downscaled target. Clears that are drawing to the target can also preserve size.
		// Of course if this size is different (in width) or this is a shuffle happening, this will be bypassed.
		const bool preserve_downscale_draw = (GSConfig.UserHacks_NativeScaling != GSNativeScaling::Off && (std::abs(scale_draw) == 1 || (scale_draw == 0 && src && src->m_from_target && src->m_from_target->m_downscaled))) || is_possible_mem_clear == ClearType::ClearWithDraw;

		rt = g_texture_cache->LookupTarget(FRAME_TEX0, t_size, ((src && src->m_scale != 1) && (GSConfig.UserHacks_NativeScaling == GSNativeScaling::Normal || GSConfig.UserHacks_NativeScaling == GSNativeScaling::NormalUpscaled) && !possible_shuffle) ? GetTextureScaleFactor() : target_scale, GSTextureCache::RenderTarget, true,
			fm, false, force_preload, preserve_rt_rgb, preserve_rt_alpha, lookup_rect, possible_shuffle, is_possible_mem_clear && FRAME_TEX0.TBP0 != m_cached_ctx.ZBUF.Block(),
			GSConfig.UserHacks_NativeScaling != GSNativeScaling::Off && preserve_downscale_draw && is_possible_mem_clear != ClearType::NormalClear, src, ds, (no_ds || !ds) ? -1 : (m_cached_ctx.ZBUF.Block() - ds->m_TEX0.TBP0));

		// Draw skipped because it was a clear and there was no target.
		if (!rt)
		{
			if (is_clear)
			{
				GL_INS("HW: Clear draw with no target, skipping.");

				const bool is_zero_color_clear = (GetConstantDirectWriteMemClearColor() == 0 && !preserve_rt_color);
				const bool is_zero_depth_clear = (GetConstantDirectWriteMemClearDepth() == 0 && !preserve_depth);
				const u32 rt_end_bp = GSLocalMemory::GetUnwrappedEndBlockAddress(
					m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBW, m_cached_ctx.FRAME.PSM, m_r);
				const u32 ds_end_bp = GSLocalMemory::GetUnwrappedEndBlockAddress(
					m_cached_ctx.ZBUF.Block(), m_cached_ctx.FRAME.FBW, m_cached_ctx.ZBUF.PSM, m_r);
				TryGSMemClear(no_rt, preserve_rt_color, is_zero_color_clear, rt_end_bp,
					no_ds, preserve_depth, is_zero_depth_clear, ds_end_bp);

				CleanupDraw(true);
				return;
			}
			else if (IsPageCopy() && src->m_from_target && m_cached_ctx.TEX0.TBP0 >= src->m_from_target->m_TEX0.TBP0 && m_cached_ctx.FRAME.FBW < ((src->m_from_target->m_TEX0.TBW + 1) >> 1))
			{
				FRAME_TEX0.TBW = src->m_from_target->m_TEX0.TBW;
			}

			if (possible_shuffle && IsSplitTextureShuffle(FRAME_TEX0, lookup_rect))
			{
				// If TEX0 == FBP, we're going to have a source left in the TC.
				// That source will get used in the actual draw unsafely, so kick it out.
				if (m_cached_ctx.FRAME.Block() == m_cached_ctx.TEX0.TBP0)
					g_texture_cache->InvalidateVideoMem(context->offset.fb, m_r, false);

				CleanupDraw(true);
				return;
			}

			rt = g_texture_cache->CreateTarget(FRAME_TEX0, t_size, GetValidSize(src, possible_shuffle), (GSConfig.UserHacks_NativeScaling != GSNativeScaling::Off && scale_draw < 0 && is_possible_mem_clear != ClearType::NormalClear) ? ((src && src->m_from_target) ? src->m_from_target->GetScale() : (ds ? ds->m_scale : 1.0f)) : target_scale,
			                                   GSTextureCache::RenderTarget, true, fm, false, force_preload, preserve_rt_color || possible_shuffle, lookup_rect, src);

			if (!rt) [[unlikely]]
			{
				GL_INS("HW: ERROR: Failed to create FRAME target, skipping.");
				CleanupDraw(true);
				return;
			}

			if (IsPageCopy() && m_cached_ctx.FRAME.FBW == 1)
			{
				rt->UpdateValidity(GSVector4i::loadh(GSVector2i(GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].pgs.x, GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].pgs.y)), true);
			}

			if (src && !src->m_from_target && GSLocalMemory::m_psm[src->m_TEX0.PSM].bpp == GSLocalMemory::m_psm[m_context->FRAME.PSM].bpp &&
				(GSUtil::GetChannelMask(src->m_TEX0.PSM) & GSUtil::GetChannelMask(m_context->FRAME.PSM)) != 0)
			{
				const u32 draw_end = GSLocalMemory::GetEndBlockAddress(m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBW, m_cached_ctx.FRAME.PSM, m_r) + 1;
				const u32 draw_start = GSLocalMemory::GetStartBlockAddress(m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBW, m_cached_ctx.FRAME.PSM, m_r);

				if (draw_start <= src->m_TEX0.TBP0 && draw_end > src->m_TEX0.TBP0)
				{
					g_texture_cache->ReplaceSourceTexture(src, rt->GetTexture(), rt->GetScale(), rt->GetUnscaledSize(), nullptr, true);

					src->m_from_target = rt;
					src->m_from_target_TEX0 = rt->m_TEX0;
					src->m_target_direct = true;
					src->m_shared_texture = true;
					src->m_target = true;
					src->m_texture = rt->m_texture;
					src->m_32_bits_fmt = rt->m_32_bits_fmt;
					src->m_valid_rect = rt->m_valid;
					src->m_alpha_minmax.first = rt->m_alpha_min;
					src->m_alpha_minmax.second = rt->m_alpha_max;

					const int target_width = std::max(FRAME_TEX0.TBW, 1U);
					const int page_offset = (src->m_TEX0.TBP0 - rt->m_TEX0.TBP0) >> 5;
					const int vertical_page_offset = page_offset / target_width;
					const int horizontal_page_offset = page_offset - (vertical_page_offset * target_width);

					if (vertical_page_offset)
					{
						const int height = std::max(rt->m_valid.w, possible_shuffle ? (m_r.w / 2) : m_r.w);
						src->m_region.SetY(vertical_page_offset * GSLocalMemory::m_psm[rt->m_TEX0.PSM].pgs.y, height);
					}
					if (horizontal_page_offset)
						src->m_region.SetX(horizontal_page_offset * GSLocalMemory::m_psm[rt->m_TEX0.PSM].pgs.x, target_width * GSLocalMemory::m_psm[rt->m_TEX0.PSM].pgs.x);

					if (rt->m_dirty.empty())
					{
						RGBAMask rgba_mask;
						rgba_mask._u32 = GSUtil::GetChannelMask(rt->m_TEX0.PSM);
						g_texture_cache->AddDirtyRectTarget(rt, m_r, FRAME_TEX0.PSM, FRAME_TEX0.TBW, rgba_mask, GSLocalMemory::m_psm[FRAME_TEX0.PSM].trbpp >= 16);
					}
				}
			}
		}
		else if (rt->m_TEX0.TBP0 != m_cached_ctx.FRAME.Block())
		{
			int vertical_offset = ((static_cast<int>(m_cached_ctx.FRAME.Block() - rt->m_TEX0.TBP0) / 32) / std::max(static_cast<int>(rt->m_TEX0.TBW), 1)) * frame_psm.pgs.y; // I know I could just not shift it..
			int texture_offset = 0;
			int horizontal_offset = ((static_cast<int>((m_cached_ctx.FRAME.Block() - rt->m_TEX0.TBP0)) / 32) % static_cast<int>(std::max(rt->m_TEX0.TBW, 1U))) * frame_psm.pgs.x;
			// Used to reduce the offset made later in channel shuffles
			m_target_offset = std::abs(static_cast<int>((m_cached_ctx.FRAME.Block() - rt->m_TEX0.TBP0)) >> 5);

			if (vertical_offset < 0)
			{
				rt->m_TEX0.TBP0 = m_cached_ctx.FRAME.Block();
				GSVector2i new_size = rt->m_unscaled_size;
				// Make sure to use the original format for the offset.
				const int new_offset = std::abs((vertical_offset / frame_psm.pgs.y) * GSLocalMemory::m_psm[rt->m_TEX0.PSM].pgs.y);
				texture_offset = new_offset;

				new_size.y += new_offset;

				const GSVector4i new_drect = GSVector4i(0, new_offset * rt->m_scale, new_size.x * rt->m_scale, new_size.y * rt->m_scale);
				rt->ResizeTexture(new_size.x, new_size.y, true, true, new_drect);

				if (src && src->m_from_target && src->m_from_target == rt && src->m_target_direct)
				{
					src->m_texture = rt->m_texture;

					// If we've moved it and the source is expecting to be inside this target, we need to update the region to point to it.
					int max_region_y = src->m_region.GetMaxY() + new_offset;
					if (max_region_y == new_offset)
						max_region_y = new_size.y;

					src->m_region.SetY(src->m_region.GetMinY() + new_offset, max_region_y);
				}

				rt->m_valid.y += new_offset;
				rt->m_valid.w += new_offset;
				rt->m_drawn_since_read.y += new_offset;
				rt->m_drawn_since_read.w += new_offset;

				g_texture_cache->CombineAlignedInsideTargets(rt, src);

				if (rt->m_dirty.size())
				{
					for (int i = 0; i < static_cast<int>(rt->m_dirty.size()); i++)
					{
						rt->m_dirty[i].r.y += new_offset;
						rt->m_dirty[i].r.w += new_offset;
					}
				}

				t_size.y += std::abs(vertical_offset);
				vertical_offset = 0;
			}

			if (horizontal_offset < 0)
			{
				// Thankfully this doesn't really happen, but catwoman moves the framebuffer backwards 1 page with a channel shuffle, which is really messy and not easy to deal with.
				// Hopefully the quick channel shuffle will just guess this and run with it.
				rt->m_TEX0.TBP0 += horizontal_offset;
				horizontal_offset = 0;
			}

			if (vertical_offset || horizontal_offset)
			{
				GSVertex* v = &m_vertex.buff[0];

				for (u32 i = 0; i < m_vertex.tail; i++)
				{
					v[i].XYZ.X += horizontal_offset << 4;
					v[i].XYZ.Y += vertical_offset << 4;
				}

				if (texture_offset && src && src->m_from_target && src->m_target_direct && src->m_from_target == rt)
				{
					GSVector4i src_region = src->GetRegionRect();

					if (src_region.rempty())
					{
						src_region = GSVector4i::loadh(rt->m_unscaled_size);
						src_region.y += texture_offset;
					}
					else
					{
						src_region.y += texture_offset;
						src_region.w += texture_offset;
					}
					src->m_region.SetX(src_region.x, src_region.z);
					src->m_region.SetY(src_region.y, src_region.w);
				}

				m_context->scissor.in.x += horizontal_offset;
				m_context->scissor.in.z += horizontal_offset;
				m_context->scissor.in.y += vertical_offset;
				m_context->scissor.in.w += vertical_offset;
				m_r.y += vertical_offset;
				m_r.w += vertical_offset;
				m_r.x += horizontal_offset;
				m_r.z += horizontal_offset;
				m_in_target_draw = rt->m_TEX0.TBP0 != m_cached_ctx.FRAME.Block();
				m_vt.m_min.p.x += horizontal_offset;
				m_vt.m_max.p.x += horizontal_offset;
				m_vt.m_min.p.y += vertical_offset;
				m_vt.m_max.p.y += vertical_offset;

				t_size.x = rt->m_unscaled_size.x - horizontal_offset;
				t_size.y = rt->m_unscaled_size.y - vertical_offset;
			}

			// Don't resize if the BPP don't match.
			GSVector2i new_size = GetValidSize(src, possible_shuffle);
			if (new_size.x > rt->m_unscaled_size.x || new_size.y > rt->m_unscaled_size.y)
			{
				const u32 new_width = std::max(new_size.x, rt->m_unscaled_size.x);
				const u32 new_height = std::max(new_size.y, rt->m_unscaled_size.y);

				//DevCon.Warning("HW: Resizing texture %d x %d draw %d", rt->m_unscaled_size.x, new_height, s_n);
				rt->ResizeTexture(new_width, new_height);
			}
			else if ((IsPageCopy() || is_possible_mem_clear) && m_r.width() <= frame_psm.pgs.x && m_r.height() <= frame_psm.pgs.y)
			{
				const int get_next_ctx = m_env.PRIM.CTXT;
				const GSDrawingContext& next_ctx = m_env.CTXT[get_next_ctx];
				GSVector4i update_valid = GSVector4i::loadh(GSVector2i(horizontal_offset + GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].pgs.x, GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].pgs.y + vertical_offset));
				rt->UpdateValidity(update_valid, true);
				if (is_possible_mem_clear)
				{
					if ((horizontal_offset + GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].pgs.x) >= static_cast<int>(rt->m_TEX0.TBW * 64) && next_ctx.FRAME.Block() == (m_cached_ctx.FRAME.Block() + 0x20))
					{
						update_valid.x = 0;
						update_valid.z = GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].pgs.x;
						update_valid.y += GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].pgs.y;
						update_valid.w += GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].pgs.y;
						rt->UpdateValidity(update_valid, true);
					}
				}
			}
		}
		// Z or RT are offset from each other, so we need a temp Z to align it
		if (ds && rt && ((m_cached_ctx.ZBUF.Block() - ds->m_TEX0.TBP0) != (m_cached_ctx.FRAME.Block() - rt->m_TEX0.TBP0) || (g_texture_cache->GetTemporaryZ() != nullptr && g_texture_cache->GetTemporaryZInfo().ZBP == ds->m_TEX0.TBP0)))
		{
			m_using_temp_z = true;
			const int page_offset = static_cast<int>(m_cached_ctx.ZBUF.Block() - ds->m_TEX0.TBP0) / 32;
			const int rt_page_offset = static_cast<int>(m_cached_ctx.FRAME.Block() - rt->m_TEX0.TBP0) / 32;
			const int z_vertical_offset = (page_offset / std::max(static_cast<int>(ds->m_TEX0.TBW), 1)) * GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].pgs.y;
			const int z_horizontal_offset = (page_offset % std::max(static_cast<int>(ds->m_TEX0.TBW), 1)) * GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].pgs.x;

			if (g_texture_cache->GetTemporaryZ() != nullptr)
			{
				GSTextureCache::TempZAddress z_address_info = g_texture_cache->GetTemporaryZInfo();

				const int old_z_vertical_offset = (z_address_info.offset / std::max(static_cast<int>(ds->m_TEX0.TBW), 1)) * GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].pgs.y;
				const int old_z_horizontal_offset = (z_address_info.offset % std::max(static_cast<int>(ds->m_TEX0.TBW), 1)) * GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].pgs.x;

				if (ds->m_TEX0.TBP0 != z_address_info.ZBP || z_address_info.offset != page_offset || z_address_info.rt_offset != rt_page_offset)
				{
					if (m_temp_z_full_copy && z_address_info.ZBP == ds->m_TEX0.TBP0)
					{
						const int vertical_offset = (z_address_info.rt_offset / std::max(static_cast<int>(rt->m_TEX0.TBW), 1)) * frame_psm.pgs.y;
						const int horizontal_offset = (z_address_info.rt_offset % std::max(static_cast<int>(rt->m_TEX0.TBW), 1)) * frame_psm.pgs.x;
						const int old_z_vertical_offset = (z_address_info.offset / std::max(static_cast<int>(ds->m_TEX0.TBW), 1)) * GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].pgs.y;
						const int old_z_horizontal_offset = (z_address_info.offset % std::max(static_cast<int>(ds->m_TEX0.TBW), 1)) * GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].pgs.x;

						const GSVector4i dRect = GSVector4i((ds->m_valid.x + old_z_vertical_offset) * ds->m_scale, (ds->m_valid.y + old_z_horizontal_offset) * ds->m_scale, (ds->m_valid.z + old_z_vertical_offset + (1.0f / ds->m_scale)) * ds->m_scale, (ds->m_valid.w + old_z_horizontal_offset + (1.0f / ds->m_scale)) * ds->m_scale);
						const GSVector4 sRect = GSVector4(((ds->m_valid.x + horizontal_offset) * ds->m_scale) / static_cast<float>(g_texture_cache->GetTemporaryZ()->GetWidth()), static_cast<float>((ds->m_valid.y + vertical_offset) * ds->m_scale) / static_cast<float>(g_texture_cache->GetTemporaryZ()->GetHeight()), (((ds->m_valid.z + horizontal_offset) + (1.0f / ds->m_scale)) * ds->m_scale) / static_cast<float>(g_texture_cache->GetTemporaryZ()->GetWidth()),
							static_cast<float>((ds->m_valid.w + vertical_offset + (1.0f / ds->m_scale)) * ds->m_scale) / static_cast<float>(g_texture_cache->GetTemporaryZ()->GetHeight()));

						GL_CACHE("HW: RT in RT Z copy back draw %d z_vert_offset %d z_offset %d", s_n, z_vertical_offset, vertical_offset);
						g_gs_device->StretchRect(g_texture_cache->GetTemporaryZ(), sRect, ds->m_texture, GSVector4(dRect), ShaderConvert::DEPTH_COPY, false);
						g_perfmon.Put(GSPerfMon::TextureCopies, 1);
					}

					g_texture_cache->InvalidateTemporaryZ();
					m_temp_z_full_copy = false;
					m_using_temp_z = false;
				}
				else if (!m_r.rintersect(z_address_info.rect_since).rempty() && m_cached_ctx.TEST.ZTST > ZTST_ALWAYS)
				{
					GL_CACHE("HW: RT in RT Updating Z copy on draw %d z_offset %d", s_n, z_address_info.offset);
					GSVector4 sRect = GSVector4(z_address_info.rect_since.x / static_cast<float>(ds->m_unscaled_size.x), z_address_info.rect_since.y / static_cast<float>(ds->m_unscaled_size.y), (z_address_info.rect_since.z + (1.0f / ds->m_scale)) / static_cast<float>(ds->m_unscaled_size.x), (z_address_info.rect_since.w + (1.0f / ds->m_scale)) / static_cast<float>(ds->m_unscaled_size.y));
					GSVector4i dRect = GSVector4i((old_z_horizontal_offset + z_address_info.rect_since.x) * ds->m_scale, (old_z_vertical_offset + z_address_info.rect_since.y) * ds->m_scale, (old_z_horizontal_offset + z_address_info.rect_since.z + (1.0f / ds->m_scale)) * ds->m_scale, (old_z_vertical_offset + z_address_info.rect_since.w + (1.0f / ds->m_scale)) * ds->m_scale);

					sRect = sRect.min(GSVector4(1.0f));
					dRect = dRect.min_u32(GSVector4i(ds->m_unscaled_size.x * ds->m_scale, ds->m_unscaled_size.y * ds->m_scale).xyxy());

					g_gs_device->StretchRect(ds->m_texture, sRect, g_texture_cache->GetTemporaryZ(), GSVector4(dRect), ShaderConvert::DEPTH_COPY, false);
					g_perfmon.Put(GSPerfMon::TextureCopies, 1);
					z_address_info.rect_since = GSVector4i::zero();
					g_texture_cache->SetTemporaryZInfo(z_address_info);
				}
			}

			if (g_texture_cache->GetTemporaryZ() == nullptr && (m_cached_ctx.ZBUF.Block() - ds->m_TEX0.TBP0) != (m_cached_ctx.FRAME.Block() - rt->m_TEX0.TBP0))
			{
				ds->Update(); // We need to update any dirty bits of Z before the copy
				
				m_using_temp_z = true;
				const int get_next_ctx = m_env.PRIM.CTXT;
				const GSDrawingContext& next_ctx = m_env.CTXT[get_next_ctx];
				const int vertical_page_offset = (rt_page_offset / std::max(static_cast<int>(rt->m_TEX0.TBW), 1));
				const int vertical_offset = vertical_page_offset * frame_psm.pgs.y;
				const int horizontal_offset = (rt_page_offset - (vertical_page_offset * std::max(static_cast<int>(rt->m_TEX0.TBW), 1))) * frame_psm.pgs.x;

				const u32 horizontal_size = std::max(rt->m_unscaled_size.x, ds->m_unscaled_size.x);
				const u32 vertical_size = std::max(rt->m_unscaled_size.y, ds->m_unscaled_size.y);

				GSVector4i dRect = GSVector4i(horizontal_offset * ds->m_scale, vertical_offset * ds->m_scale, (horizontal_offset + (ds->m_unscaled_size.x - z_horizontal_offset)) * ds->m_scale, (vertical_offset + (ds->m_unscaled_size.y - z_vertical_offset)) * ds->m_scale);

				// Size here should match whichever is biggest, since that's probably what's going to happen with it further down.
				const int new_height = std::min(2048, std::max(t_size.y, static_cast<int>(vertical_size))) * ds->m_scale;
				const int new_width = std::min(2048, std::max(t_size.x, static_cast<int>(horizontal_size))) * ds->m_scale;

				if (GSTexture* tex = g_gs_device->CreateDepthStencil(new_width, new_height, GSTexture::Format::DepthStencil, true))
				{
					GSVector4 sRect = GSVector4(static_cast<float>(z_horizontal_offset) / static_cast<float>(ds->m_unscaled_size.x), static_cast<float>(z_vertical_offset) / static_cast<float>(ds->m_unscaled_size.y), 1.0f , 1.0f);

					const bool restricted_copy = !(((next_ctx.ZBUF.ZBP == m_context->ZBUF.ZBP && next_ctx.FRAME.FBP == m_context->FRAME.FBP)) && !(IsPossibleChannelShuffle() && src && (!src->m_from_target || EmulateChannelShuffle(src->m_from_target, true)) && !IsPageCopy()));

					if (restricted_copy)
					{
						// m_r already has horizontal_offset (rt offset) applied)
						dRect = GSVector4i(m_r.x * ds->m_scale, m_r.y * ds->m_scale, ((1 + m_r.z) * ds->m_scale), ((1 + m_r.w) * ds->m_scale));
						sRect = GSVector4(static_cast<float>((m_r.x - horizontal_offset) + z_horizontal_offset) / static_cast<float>(ds->m_unscaled_size.x), static_cast<float>((m_r.y - vertical_offset) + z_vertical_offset) / static_cast<float>(ds->m_unscaled_size.y), (static_cast<float>((m_r.z - horizontal_offset) + z_horizontal_offset) + 1.0f) / static_cast<float>(ds->m_unscaled_size.x), (static_cast<float>((m_r.w - vertical_offset) + z_vertical_offset) + 1.0f) / static_cast<float>(ds->m_unscaled_size.y));
					}

					// No point in copying more width than the width of the draw, it's going to be wasted (could still be tall, though).
					sRect.z = std::min(sRect.z, sRect.x + ((1.0f * ds->m_scale) + (static_cast<float>(m_cached_ctx.FRAME.FBW * 64)) / static_cast<float>(ds->m_unscaled_size.x)));
					dRect.z = std::min(dRect.z, dRect.x + static_cast<int>(1 * ds->m_scale) + static_cast<int>(static_cast<float>(m_cached_ctx.FRAME.FBW * 64) * ds->m_scale));

					GL_CACHE("HW: RT in RT Z copy on draw %d z_vert_offset %d", s_n, page_offset);

					if (m_cached_ctx.TEST.ZTST > ZTST_ALWAYS || !dRect.rintersect(GSVector4i(GSVector4(m_r) * ds->m_scale)).eq(dRect))
					{
						g_gs_device->StretchRect(ds->m_texture, sRect, tex, GSVector4(dRect), ShaderConvert::DEPTH_COPY, false);
						g_perfmon.Put(GSPerfMon::TextureCopies, 1);
					}
					g_texture_cache->SetTemporaryZ(tex);
					g_texture_cache->SetTemporaryZInfo(ds->m_TEX0.TBP0, page_offset, rt_page_offset);
					t_size.y = std::max(static_cast<int>(new_height / ds->m_scale), t_size.y);
				}
				else
				{
					DevCon.Warning("HW: Temporary depth buffer creation failed.");
					m_using_temp_z = false;
				}
			}
		}

		if (src && src->m_from_target && src->m_target_direct && src->m_from_target == rt)
		{
			src->m_texture = rt->m_texture;
			src->m_scale = rt->GetScale();
			src->m_unscaled_size = rt->m_unscaled_size;
		}

		target_scale = rt->GetScale();

		if (ds && ds->m_scale != target_scale)
		{
			const GSVector2i unscaled_size(ds->m_unscaled_size.x, ds->m_unscaled_size.y);
			ds->m_scale = 1;
			ds->ResizeTexture(ds->m_unscaled_size.x * target_scale, ds->m_unscaled_size.y * target_scale, true);
			// Slightly abusing the texture resize.
			ds->m_scale = target_scale;
			ds->m_unscaled_size = unscaled_size;
			ds->m_downscaled = rt->m_downscaled;
		}
		// The target might have previously been a C32 format with valid alpha. If we're switching to C24, we need to preserve it.
		preserve_rt_alpha |= (GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].trbpp == 24 && rt->HasValidAlpha());
		preserve_rt_color = preserve_rt_rgb || preserve_rt_alpha;

		if (m_channel_shuffle)
		{
			m_last_channel_shuffle_fbp = rt->m_TEX0.TBP0;
			m_last_channel_shuffle_tbp = src->m_TEX0.TBP0;

			// If it's a new target, we don't know where the end is as it's starting on a shuffle, so just do every shuffle following.
			m_last_channel_shuffle_end_block = (rt->m_last_draw >= s_n) ? (GS_MAX_BLOCKS - 1) : (rt->m_end_block < rt->m_TEX0.TBP0 ? (rt->m_end_block + GS_MAX_BLOCKS) : rt->m_end_block);
		}
		else
			m_last_channel_shuffle_end_block = 0xFFFF;
	}

	// Only run if DS was new and matched the framebuffer.
	if (!no_ds && !ds)
	{
		ZBUF_TEX0.U64 = 0;
		ZBUF_TEX0.TBP0 = m_cached_ctx.ZBUF.Block();
		ZBUF_TEX0.TBW = m_cached_ctx.FRAME.FBW;
		ZBUF_TEX0.PSM = m_cached_ctx.ZBUF.PSM;

		ds = g_texture_cache->LookupTarget(ZBUF_TEX0, t_size, target_scale, GSTextureCache::DepthStencil,
			m_cached_ctx.DepthWrite(), 0, false, force_preload, preserve_depth, preserve_depth, unclamped_draw_rect, IsPossibleChannelShuffle(), is_possible_mem_clear && ZBUF_TEX0.TBP0 != m_cached_ctx.FRAME.Block(), false,
			src, nullptr, -1);

		ZBUF_TEX0.TBW = m_channel_shuffle ? src->m_from_target_TEX0.TBW : m_cached_ctx.FRAME.FBW;

		// This should never happen, but just to be safe..
		if (!ds)
		{
			ds = g_texture_cache->CreateTarget(ZBUF_TEX0, t_size, GetValidSize(src, possible_shuffle), target_scale, GSTextureCache::DepthStencil,
				true, 0, false, force_preload, preserve_depth, m_r, src);
			if (!ds) [[unlikely]]
			{
				GL_INS("HW: ERROR: Failed to create ZBUF target, skipping.");
				CleanupDraw(true);
				return;
			}
		}
		else
		{
			// If it failed to check depth test earlier, we can now check the top bits from the alpha to get a bit more accurate picture.
			if (((zm && m_cached_ctx.TEST.ZTST > ZTST_ALWAYS) || (m_vt.m_eq.z && m_cached_ctx.TEST.ZTST == ZTST_GEQUAL)) && GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].trbpp == 32)
			{
				if (ds->m_alpha_max != 0)
				{
					const u32 max_z = (static_cast<u64>(ds->m_alpha_max + 1) << 24) - 1;

					switch (m_cached_ctx.TEST.ZTST)
					{
						case ZTST_GEQUAL:
							// Every Z value will pass
							if (max_z <= m_vt.m_min.p.z)
							{
								m_cached_ctx.TEST.ZTST = ZTST_ALWAYS;
								if (zm)
								{
									ds = nullptr;
									no_ds = true;
								}
							}
							break;
						case ZTST_GREATER:
							// Every Z value will pass
							if (max_z < m_vt.m_min.p.z)
							{
								m_cached_ctx.TEST.ZTST = ZTST_ALWAYS;
								if (zm)
								{
									ds = nullptr;
									no_ds = true;
								}
							}
							break;
						default:
							break;
					}
				}
			}
		}
	}

	if (m_process_texture)
	{
		GIFRegCLAMP MIP_CLAMP = m_cached_ctx.CLAMP;
		const GSVertex* v = &m_vertex.buff[0];

		if (rt)
		{
			// Hypothesis: texture shuffle is used as a postprocessing effect so texture will be an old target.
			// Initially code also tested the RT but it gives too much false-positive
			const int horizontal_offset = ((static_cast<int>((m_cached_ctx.FRAME.Block() - rt->m_TEX0.TBP0)) / 32) % static_cast<int>(std::max(rt->m_TEX0.TBW, 1U))) * frame_psm.pgs.x;
			const int first_x = (((v[0].XYZ.X - m_context->XYOFFSET.OFX) + 8) >> 4) - horizontal_offset;
			const int first_u = PRIM->FST ? ((v[0].U + 8) >> 4) : static_cast<int>(((1 << m_cached_ctx.TEX0.TW) * (v[0].ST.S / v[1].RGBAQ.Q)) + 0.5f);
			const bool shuffle_coords = (first_x ^ first_u) & 8;

			// copy of a 16bit source in to this target, make sure it's opaque and not bilinear to reduce false positives.
			m_copy_16bit_to_target_shuffle = m_cached_ctx.TEX0.TBP0 != m_cached_ctx.FRAME.Block() && rt->m_32_bits_fmt == true && IsOpaque()
			                              && !(context->TEX1.MMIN & 1) && !src->m_32_bits_fmt && m_cached_ctx.FRAME.FBMSK;

			// It's not actually possible to do a C16->C16 texture shuffle of B to A as they are the same group
			// However you can do it by using C32 and offsetting the target verticies to point to B A, then mask as appropriate.
			m_same_group_texture_shuffle = draw_uses_target && (m_cached_ctx.TEX0.PSM & 0xE) == PSMCT32 && (m_cached_ctx.FRAME.PSM & 0x7) == PSMCT16 && (m_vt.m_min.p.x == 8.0f);

			// Both input and output are 16 bits and texture was initially 32 bits! Same for the target, Sonic Unleash makes a new target which really is 16bit.
			m_texture_shuffle = ((m_same_group_texture_shuffle || (tex_psm.bpp == 16)) && (GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].bpp == 16) && (shuffle_coords || rt->m_32_bits_fmt)) &&
			                    (src->m_32_bits_fmt || m_copy_16bit_to_target_shuffle) &&
			                    (draw_sprite_tex || (m_vt.m_primclass == GS_TRIANGLE_CLASS && (m_index.tail % 6) == 0 && TrianglesAreQuads(true)));

			if (m_texture_shuffle && IsSplitTextureShuffle(rt->m_TEX0, rt->m_valid))
			{
				// If TEX0 == FBP, we're going to have a source left in the TC.
				// That source will get used in the actual draw unsafely, so kick it out.
				if (m_cached_ctx.FRAME.Block() == m_cached_ctx.TEX0.TBP0)
					g_texture_cache->InvalidateVideoMem(context->offset.fb, m_r, false);

				CleanupDraw(true);
				return;
			}
		}

		if ((src->m_target || (m_cached_ctx.FRAME.Block() == m_cached_ctx.TEX0.TBP0)) && IsPossibleChannelShuffle())
		{
			if (!src->m_target)
			{
				g_texture_cache->ReplaceSourceTexture(src, rt->GetTexture(), rt->GetScale(), rt->GetUnscaledSize(), nullptr, true);
				src->m_from_target = rt;
				src->m_from_target_TEX0 = rt->m_TEX0;
				src->m_target = true;
				src->m_target_direct = true;
				src->m_valid_rect = rt->m_valid;
				src->m_alpha_minmax.first = rt->m_alpha_min;
				src->m_alpha_minmax.second = rt->m_alpha_max;
			}

			GL_INS("HW: Channel shuffle effect detected (2nd shot)");
			m_channel_shuffle = true;
			m_last_channel_shuffle_fbmsk = m_context->FRAME.FBMSK;
			if (rt)
			{
				m_last_channel_shuffle_fbp = rt->m_TEX0.TBP0;
				m_last_channel_shuffle_tbp = src->m_TEX0.TBP0;
				// Urban Chaos goes from Z16 to C32, so let's just use the rt's original end block.
				if (!src->m_from_target || GSLocalMemory::m_psm[src->m_from_target_TEX0.PSM].bpp != GSLocalMemory::m_psm[rt->m_TEX0.PSM].bpp)
					m_last_channel_shuffle_end_block = rt->m_end_block;
				else
					m_last_channel_shuffle_end_block = (rt->m_TEX0.TBP0 + (src->m_from_target->m_end_block - src->m_from_target_TEX0.TBP0));

				if (m_last_channel_shuffle_end_block < rt->m_TEX0.TBP0)
					m_last_channel_shuffle_end_block += GS_MAX_BLOCKS;

				// if the RT is bigger, then use that instead.
				if (m_last_channel_shuffle_end_block < rt->m_end_block)
					m_last_channel_shuffle_end_block = rt->m_end_block;
			}
		}
		else
		{
			m_channel_shuffle = false;
		}
#if 0
		// FIXME: We currently crop off the rightmost and bottommost pixel when upscaling clamps,
		// until the issue is properly solved we should keep this disabled as it breaks many games when upscaling.
		// See #5387, #5853, #5851 on GH for more details.
		// 
		// Texture clamp optimizations (try to move everything to sampler hardware)
		if (m_cached_ctx.CLAMP.WMS == CLAMP_REGION_CLAMP && MIP_CLAMP.MINU == 0 && MIP_CLAMP.MAXU == tw - 1)
			m_cached_ctx.CLAMP.WMS = CLAMP_CLAMP;
		else if (m_cached_ctx.CLAMP.WMS == CLAMP_REGION_REPEAT && MIP_CLAMP.MINU == tw - 1 && MIP_CLAMP.MAXU == 0)
			m_cached_ctx.CLAMP.WMS = CLAMP_REPEAT;
		else if ((m_cached_ctx.CLAMP.WMS & 2) && !(tmm.uses_boundary & TextureMinMaxResult::USES_BOUNDARY_U))
			m_cached_ctx.CLAMP.WMS = CLAMP_CLAMP;
		if (m_cached_ctx.CLAMP.WMT == CLAMP_REGION_CLAMP && MIP_CLAMP.MINV == 0 && MIP_CLAMP.MAXV == th - 1)
			m_cached_ctx.CLAMP.WMT = CLAMP_CLAMP;
		else if (m_cached_ctx.CLAMP.WMT == CLAMP_REGION_REPEAT && MIP_CLAMP.MINV == th - 1 && MIP_CLAMP.MAXV == 0)
			m_cached_ctx.CLAMP.WMT = CLAMP_REPEAT;
		else if ((m_cached_ctx.CLAMP.WMT & 2) && !(tmm.uses_boundary & TextureMinMaxResult::USES_BOUNDARY_V))
			m_cached_ctx.CLAMP.WMT = CLAMP_CLAMP;
#endif
		const int tw = 1 << TEX0.TW;
		const int th = 1 << TEX0.TH;
		const bool is_shuffle = m_channel_shuffle || m_texture_shuffle;

		// If m_src is from a target that isn't the same size as the texture, texture sample edge modes won't work quite the same way
		// If the game actually tries to access stuff outside of the rendered target, it was going to get garbage anyways so whatever
		// But the game could issue reads that wrap to valid areas, so move wrapping to the shader if wrapping is used
		const GSVector2i unscaled_size = src->m_target ? src->GetRegionSize() : src->GetUnscaledSize();

		if (!is_shuffle && m_cached_ctx.CLAMP.WMS == CLAMP_REPEAT && (tmm.uses_boundary & TextureMinMaxResult::USES_BOUNDARY_U) && unscaled_size.x != tw)
		{
			// Our shader-emulated region repeat doesn't upscale :(
			// Try to avoid it if possible
			// TODO: Upscale-supporting shader-emulated region repeat
			if (unscaled_size.x < tw && m_vt.m_min.t.x > -(tw - unscaled_size.x) && m_vt.m_max.t.x < tw)
			{
				// Game only extends into data we don't have (but doesn't wrap around back onto good data), clamp seems like the most reasonable solution
				m_cached_ctx.CLAMP.WMS = CLAMP_CLAMP;
			}
			else
			{
				m_cached_ctx.CLAMP.WMS = CLAMP_REGION_REPEAT;
				m_cached_ctx.CLAMP.MINU = (1 << m_cached_ctx.TEX0.TW) - 1;
				m_cached_ctx.CLAMP.MAXU = 0;
			}
		}
		if (!is_shuffle && m_cached_ctx.CLAMP.WMT == CLAMP_REPEAT && (tmm.uses_boundary & TextureMinMaxResult::USES_BOUNDARY_V) && unscaled_size.y != th)
		{
			if (unscaled_size.y < th && m_vt.m_min.t.y > -(th - unscaled_size.y) && m_vt.m_max.t.y < th)
			{
				m_cached_ctx.CLAMP.WMT = CLAMP_CLAMP;
			}
			else
			{
				m_cached_ctx.CLAMP.WMT = CLAMP_REGION_REPEAT;
				m_cached_ctx.CLAMP.MINV = (1 << m_cached_ctx.TEX0.TH) - 1;
				m_cached_ctx.CLAMP.MAXV = 0;
			}
		}

		// Round 2
		if (IsMipMapActive() && GSConfig.HWMipmap && !tex_psm.depth && !src->m_from_hash_cache)
		{
			// Upload remaining texture layers
			const GSVector4 tmin = m_vt.m_min.t;
			const GSVector4 tmax = m_vt.m_max.t;

			// Backup original coverage.
			const GSVector4i coverage = tmm.coverage;

			for (int layer = m_lod.x + 1; layer <= m_lod.y; layer++)
			{
				const GIFRegTEX0 MIP_TEX0(GetTex0Layer(layer));

				MIP_CLAMP.MINU >>= 1;
				MIP_CLAMP.MINV >>= 1;
				MIP_CLAMP.MAXU >>= 1;
				MIP_CLAMP.MAXV >>= 1;

				m_vt.m_min.t *= 0.5f;
				m_vt.m_max.t *= 0.5f;

				tmm = GetTextureMinMax(MIP_TEX0, MIP_CLAMP, m_vt.IsLinear(), true);

				src->UpdateLayer(MIP_TEX0, tmm.coverage, layer - m_lod.x);
			}

			// we don't need to generate mipmaps since they were provided
			src->m_texture->ClearMipmapGenerationFlag();
			m_vt.m_min.t = tmin;
			m_vt.m_max.t = tmax;

			// Restore original coverage.
			tmm.coverage = coverage;
		}
	}

	if (rt)
	{
		// Be sure texture shuffle detection is properly propagated
		// Otherwise set or clear the flag (Code in texture cache only set the flag)
		// Note: it is important to clear the flag when RT is used as a real 16 bits target.
		rt->m_32_bits_fmt = m_texture_shuffle || (GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].bpp != 16);
	}

	// Do the same for the depth target. Jackie Chan Adventures swaps from C32 to Z16 after a clear.
	if (ds)
		ds->m_32_bits_fmt = (GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].bpp != 16);

	// Deferred update of TEX0. We don't want to change it when we're doing a shuffle/clear, because it
	// may increase the buffer width, or change PSM, which breaks P8 conversion amongst other things.
	// Some texture shuffles can be to new targets (or reused ones) so they may need their valid rects adjusting.
	const bool can_update_size = !is_possible_mem_clear && !m_texture_shuffle && !m_channel_shuffle;

	if (!m_texture_shuffle && !m_channel_shuffle)
	{
		// Try to turn blits in to single sprites, saves upscaling problems when striped clears/blits.
		if (m_vt.m_primclass == GS_SPRITE_CLASS && m_primitive_covers_without_gaps == NoGapsType::FullCover && m_index.tail > 2 && (!PRIM->TME || TextureCoversWithoutGapsNotEqual()) && m_vt.m_eq.rgba == 0xFFFF)
		{
			// Full final framebuffer only.
			const GSVector2i fb_size = PCRTCDisplays.GetFramebufferSize(-1);
			if (std::abs(fb_size.x - m_r.width()) <= 1 && std::abs(fb_size.y - m_r.height()) <= 1)
			{
				GSVertex* v = m_vertex.buff;

				v[0].XYZ.Z = v[1].XYZ.Z;
				v[0].RGBAQ = v[1].RGBAQ;
				v[0].FOG = v[1].FOG;
				m_vt.m_eq.rgba = 0xFFFF;
				m_vt.m_eq.z = true;
				m_vt.m_eq.f = true;

				v[1].XYZ.X = v[m_index.tail - 1].XYZ.X;
				v[1].XYZ.Y = v[m_index.tail - 1].XYZ.Y;

				if (PRIM->FST)
				{
					v[1].U = v[m_index.tail - 1].U;
					v[1].V = v[m_index.tail - 1].V;
				}
				else
				{
					v[1].ST.S = v[m_index.tail - 1].ST.S;
					v[1].ST.T = v[m_index.tail - 1].ST.T;
					v[1].RGBAQ.Q = v[m_index.tail - 1].RGBAQ.Q;
				}

				m_vertex.head = m_vertex.tail = m_vertex.next = 2;
				m_index.tail = 2;
			}
		}

		const bool blending_cd = NeedsBlending() && !m_context->ALPHA.IsOpaque();
		bool valid_width_change = false;
		if (rt && ((!is_possible_mem_clear || blending_cd) || rt->m_TEX0.PSM != FRAME_TEX0.PSM) && !m_in_target_draw)
		{
			const u32 frame_mask = (m_cached_ctx.FRAME.FBMSK & frame_psm.fmsk);
			valid_width_change = rt->m_TEX0.TBW != FRAME_TEX0.TBW && (frame_mask != (frame_psm.fmsk & 0x00FFFFFF) || rt->m_valid_rgb == false);
			if (valid_width_change && !m_cached_ctx.ZBUF.ZMSK && (m_cached_ctx.FRAME.FBMSK & 0xFF000000))
			{
				// Alpha could be a font, and since the width is changing it's no longer valid.
				// Be careful of downsize copies or other effects, checking Z MSK should hopefully be enough.. (Okami).
				if (m_cached_ctx.FRAME.FBMSK & 0x0F000000)
					rt->m_valid_alpha_low = false;
				if (m_cached_ctx.FRAME.FBMSK & 0xF0000000)
					rt->m_valid_alpha_high = false;
			}
			if (FRAME_TEX0.TBW != 1 || (m_r.width() > frame_psm.pgs.x || m_r.height() > frame_psm.pgs.y) || (scale_draw == 1 && !scaled_copy))
			{
				FRAME_TEX0.TBP0 = rt->m_TEX0.TBP0;
				rt->m_TEX0 = FRAME_TEX0;
			}

			if (valid_width_change)
			{
				GSVector4i new_valid_width = rt->m_valid;
				new_valid_width.z = std::min(new_valid_width.z, static_cast<int>(rt->m_TEX0.TBW) * 64);
				rt->ResizeValidity(new_valid_width);
			}
		}

		if (ds && (!is_possible_mem_clear || ds->m_TEX0.PSM != ZBUF_TEX0.PSM || (rt && ds->m_TEX0.TBW != rt->m_TEX0.TBW)) && !m_in_target_draw)
		{
			if (ZBUF_TEX0.TBW != 1 || (m_r.width() > frame_psm.pgs.x || m_r.height() > frame_psm.pgs.y) || (scale_draw == 1 && !scaled_copy))
			{
				ZBUF_TEX0.TBP0 = ds->m_TEX0.TBP0;
				ds->m_TEX0 = ZBUF_TEX0;
			}
			if (valid_width_change)
			{
				GSVector4i new_valid_width = ds->m_valid;
				new_valid_width.z = std::min(new_valid_width.z, static_cast<int>(ds->m_TEX0.TBW) * 64);
				ds->ResizeValidity(new_valid_width);
			}
		}

		if (rt)
			g_texture_cache->CombineAlignedInsideTargets(rt, src);
		if (ds)
			g_texture_cache->CombineAlignedInsideTargets(ds, src);
	}
	else if (!m_texture_shuffle)
	{
		// Allow FB PSM to update on channel shuffle, it should be correct, unlike texture shuffle.
		// The FBW should also be okay, since it's coming from the source.
		if (rt)
		{
			const bool update_fbw = (FRAME_TEX0.TBW != rt->m_TEX0.TBW || rt->m_TEX0.TBW == 1) && !m_in_target_draw && (m_channel_shuffle && src->m_target) && (!NeedsBlending() || IsOpaque() || m_context->ALPHA.IsBlack());
			rt->m_TEX0.TBW = update_fbw ? ((src && src->m_from_target && src->m_32_bits_fmt) ? src->m_from_target->m_TEX0.TBW : FRAME_TEX0.TBW) : std::max(rt->m_TEX0.TBW, FRAME_TEX0.TBW);
			rt->m_TEX0.PSM = FRAME_TEX0.PSM;
		}
		if (ds)
		{
			ds->m_TEX0.TBW = std::max(ds->m_TEX0.TBW, ZBUF_TEX0.TBW);
			ds->m_TEX0.PSM = ZBUF_TEX0.PSM;
		}
	}
	// Probably grabbed an old 16bit target (Band Hero)
	/*else if (m_texture_shuffle && GSLocalMemory::m_psm[rt->m_TEX0.PSM].bpp == 16)
	{
		rt->m_TEX0.PSM = PSMCT32;
	}*/

	// Figure out which channels we're writing.
	if (rt)
		rt->UpdateValidChannels(rt->m_TEX0.PSM, m_texture_shuffle ? GetEffectiveTextureShuffleFbmsk() : fm);
	if (ds)
		ds->UpdateValidChannels(ZBUF_TEX0.PSM, zm);

	const GSVector2i resolution = PCRTCDisplays.GetResolution();
	GSTextureCache::Target* old_rt = nullptr;
	GSTextureCache::Target* old_ds = nullptr;

	// If the draw is dated, we're going to expand in to black, so it's just a pointless rescale which will mess up our valid rects and end blocks.
	if (!(m_cached_ctx.TEST.DATE && m_cached_ctx.TEST.DATM))
	{
		GSVector2i new_size = t_size;
		GSVector4i update_rect = m_r;
		const GIFRegTEX0& draw_TEX0 = rt ? rt->m_TEX0 : ds->m_TEX0;
		const int buffer_width = std::max(draw_TEX0.TBW, 1U) * 64;
		// We need to adjust the size if it's a texture shuffle as we could end up making the RT twice the size.
		if (src && m_texture_shuffle && !m_copy_16bit_to_target_shuffle)
		{
			if ((new_size.x > src->m_valid_rect.z && m_vt.m_max.p.x == new_size.x) || (new_size.y > src->m_valid_rect.w && m_vt.m_max.p.y == new_size.y))
			{
				if (new_size.y <= src->m_valid_rect.w && (rt->m_TEX0.TBW != m_cached_ctx.FRAME.FBW))
				{
					new_size.x /= 2;
				}
				else
				{
					new_size.y /= 2;
				}
			}

			if (update_rect.z > src->m_valid_rect.z && (rt->m_TEX0.TBW != m_cached_ctx.FRAME.FBW))
			{
				// This is a case for Superman Shadow of Apokalypse where it is *nearly* double height and slightly wider, but the page count adds up.
				if (update_rect.w > src->m_valid_rect.w)
				{
					update_rect = src->m_valid_rect;
				}
				else
				{
					update_rect.x /= 2;
					update_rect.z /= 2;
				}
			}
			else
			{
				update_rect.y /= 2;
				update_rect.w /= 2;
			}
		}
		// NFS Undercover does a draw with double width of the actual width 1280x240, which functions the same as doubling the height.
		// Ignore single page/0 page stuff, that's just gonna get silly
		else if (m_texture_shuffle && buffer_width > 64 && update_rect.z > buffer_width)
		{
			update_rect.w *= static_cast<float>(update_rect.z) / static_cast<float>(buffer_width);
			update_rect.z = buffer_width;
		}

		/*if (m_in_target_draw && src && m_channel_shuffle && src->m_from_target && src->m_from_target == rt && m_cached_ctx.TEX0.TBP0 == src->m_from_target->m_TEX0.TBP0)
		{
			new_size.y = std::max(new_size.y, static_cast<int>((((m_cached_ctx.FRAME.Block() - rt->m_TEX0.TBP0) >> 5) / rt->m_TEX0.TBW) * frame_psm.pgs.y));
			GSVector4i new_valid = rt->m_valid;
			new_valid.w = std::max(new_valid.w, static_cast<int>((((m_cached_ctx.FRAME.Block() - rt->m_TEX0.TBP0) >> 5) / rt->m_TEX0.TBW) * frame_psm.pgs.y) + frame_psm.pgs.y);
			rt->UpdateValidity(new_valid, true);
		}*/

		// We still need to make sure the dimensions of the targets match.
		// Limit new size to 2048, the GS can't address more than this so may avoid some bugs/crashes.
		GSVector2i ds_size = m_using_temp_z ? GSVector2i(g_texture_cache->GetTemporaryZ()->GetSize() / ds->m_scale) : (ds ? ds->m_unscaled_size : GSVector2i(0,0));

		const int new_w = std::min(2048, std::max(new_size.x, std::max(rt ? rt->m_unscaled_size.x : 0, ds ? ds_size.x : 0)));
		const int new_h = std::min(2048, std::max(new_size.y, std::max(rt ? rt->m_unscaled_size.y : 0, ds ? ds_size.y : 0)));

		const bool full_cover_clear = is_possible_mem_clear && GSLocalMemory::IsPageAligned(m_cached_ctx.FRAME.PSM, m_r) && m_r.x == 0 && m_r.y == 0 && !preserve_rt_rgb &&
									  !IsPageCopy() && m_r.width() == (m_cached_ctx.FRAME.FBW * 64);

		if (rt)
		{
			const u32 old_end_block = rt->m_end_block;
			const bool new_rect = rt->m_valid.rempty();
			const bool new_height = new_h > rt->GetUnscaledHeight();
			const int old_height = rt->m_texture->GetHeight();
			bool merge_targets = false;
			pxAssert(rt->GetScale() == target_scale);
			if (rt->GetUnscaledWidth() != new_w || rt->GetUnscaledHeight() != new_h)
				GL_INS("HW: Resize RT from %dx%d to %dx%d", rt->GetUnscaledWidth(), rt->GetUnscaledHeight(), new_w, new_h);

			// May not be needed/could cause problems with garbage loaded from GS memory
			/*if (preserve_rt_color)
			{
				RGBAMask mask;
				mask._u32 = 0xF;

				if (new_w > rt->m_unscaled_size.x)
				{
					GSVector4i width_dirty_rect = GSVector4i(rt->m_unscaled_size.x, 0, new_w, new_h);
					g_texture_cache->AddDirtyRectTarget(rt, width_dirty_rect, rt->m_TEX0.PSM, rt->m_TEX0.TBW, mask);
				}

				if (new_h > rt->m_unscaled_size.y)
				{
					GSVector4i height_dirty_rect = GSVector4i(0, rt->m_unscaled_size.y, new_w, new_h);
					g_texture_cache->AddDirtyRectTarget(rt, height_dirty_rect, rt->m_TEX0.PSM, rt->m_TEX0.TBW, mask);
				}
			}*/

			if ((new_w > rt->m_unscaled_size.x || new_h > rt->m_unscaled_size.y) && GSConfig.UserHacks_TextureInsideRt >= GSTextureInRtMode::InsideTargets)
				merge_targets = true;

			rt->ResizeTexture(new_w, new_h);

			if (!m_texture_shuffle && !m_channel_shuffle)
			{
				// if the height cache gave a different size to our final size, we need to check if it needs preloading.
				// Pirates - Legend of the Black Kat starts a draw of 416, but Z is 448 and it preloads the background.
				if (rt->m_drawn_since_read.rempty() && rt->m_dirty.size() > 0 && new_height && (preserve_rt_color || preserve_rt_alpha))
				{
					RGBAMask mask;
					mask._u32 = preserve_rt_color ? 0x7 : 0;
					mask.c.a |= preserve_rt_alpha;
					g_texture_cache->AddDirtyRectTarget(rt, GSVector4i(rt->m_valid.x, rt->m_valid.w, rt->m_valid.z, new_h), rt->m_TEX0.PSM, rt->m_TEX0.TBW, mask, false);
					g_texture_cache->GetTargetSize(rt->m_TEX0.TBP0, rt->m_TEX0.TBW, rt->m_TEX0.PSM, 0, new_h);
				}
				const bool rt_cover = full_cover_clear && (m_r.height() + frame_psm.pgs.y) >= rt->m_valid.height();
				rt->ResizeValidity(rt_cover ? update_rect : rt->m_valid.rintersect(rt->GetUnscaledRect()));
				rt->ResizeDrawn(rt_cover ? update_rect : rt->m_drawn_since_read.rintersect(rt->GetUnscaledRect()));
			}

			const bool rt_update = can_update_size || (is_possible_mem_clear && m_vt.m_min.c.a > 0) || (m_texture_shuffle && (src && src->m_from_target != rt));

			// If it's updating from a texture shuffle, limit the size to the source size.
			if (rt_update && !can_update_size)
			{
				if (src && src->m_from_target)
					update_rect = update_rect.rintersect(src->m_from_target->m_valid);

				update_rect = update_rect.rintersect(GSVector4i::loadh(GSVector2i(new_w, new_h)));
			}

			// if frame is masked or afailing always to never write frame, wanna make sure we don't touch it. This might happen if DATE or Alpha Test is being used to write to Z.
			const bool frame_masked = ((m_cached_ctx.FRAME.FBMSK & frame_psm.fmsk) == frame_psm.fmsk) || (m_cached_ctx.TEST.ATE && m_cached_ctx.TEST.ATST == ATST_NEVER && !(m_cached_ctx.TEST.AFAIL & AFAIL_FB_ONLY));
			// Limit to 2x the vertical height of the resolution (for double buffering)
			rt->UpdateValidity(update_rect, !frame_masked && (rt_update || (m_r.w <= (resolution.y * 2) && !m_texture_shuffle)));
			rt->UpdateDrawn(update_rect, !frame_masked && (rt_update || (m_r.w <= (resolution.y * 2) && !m_texture_shuffle)));

			if (merge_targets)
				g_texture_cache->CombineAlignedInsideTargets(rt, src);
			// Probably changing to double buffering, so invalidate any old target that was next to it.
			// This resolves an issue where the PCRTC will find the old target in FMV's causing flashing.
			// Grandia Xtreme, Onimusha Warlord.
			if (!new_rect && new_height && old_end_block != rt->m_end_block)
			{
				old_rt = g_texture_cache->FindTargetOverlap(rt, GSTextureCache::RenderTarget, m_cached_ctx.FRAME.PSM);

				if (old_rt && old_rt != rt && GSUtil::HasSharedBits(old_rt->m_TEX0.PSM, rt->m_TEX0.PSM))
				{
					const int copy_width = (old_rt->m_texture->GetWidth()) > (rt->m_texture->GetWidth()) ? (rt->m_texture->GetWidth()) : old_rt->m_texture->GetWidth();
					const int copy_height = (old_rt->m_texture->GetHeight()) > (rt->m_texture->GetHeight() - old_height) ? (rt->m_texture->GetHeight() - old_height) : old_rt->m_texture->GetHeight();
					GL_INS("HW: RT double buffer copy from FBP 0x%x, %dx%d => %d,%d", old_rt->m_TEX0.TBP0, copy_width, copy_height, 0, old_height);

					// Invalidate has been moved to after DrawPrims(), because we might kill the current sources' backing.
					g_gs_device->CopyRect(old_rt->m_texture, rt->m_texture, GSVector4i(0, 0, copy_width, copy_height), 0, old_height);
					preserve_rt_color = true;
				}
				else
				{
					old_rt = nullptr;
				}
			}
		}
		if (ds)
		{
			const u32 old_end_block = ds->m_end_block;
			const bool new_rect = ds->m_valid.rempty();
			const bool new_height = new_h > ds->GetUnscaledHeight();
			const int old_height = ds->m_texture->GetHeight();

			pxAssert(ds->GetScale() == target_scale);
			if (ds->GetUnscaledWidth() != new_w || ds->GetUnscaledHeight() != new_h)
				GL_INS("HW: Resize DS from %dx%d to %dx%d", ds->GetUnscaledWidth(), ds->GetUnscaledHeight(), new_w, new_h);

			ds->ResizeTexture(new_w, new_h);


			if (m_using_temp_z)
			{
				const int z_width = g_texture_cache->GetTemporaryZ()->GetWidth() / ds->m_scale;
				const int z_height = g_texture_cache->GetTemporaryZ()->GetHeight() / ds->m_scale;

				if (z_width != new_w || z_height != new_h)
				{
					if (GSTexture* tex = g_gs_device->CreateDepthStencil(new_w * ds->m_scale, new_h * ds->m_scale, GSTexture::Format::DepthStencil, true))
					{
						const GSVector4i dRect = GSVector4i(0, 0, g_texture_cache->GetTemporaryZ()->GetWidth(), g_texture_cache->GetTemporaryZ()->GetHeight());
						g_gs_device->StretchRect(g_texture_cache->GetTemporaryZ(), GSVector4(0.0f, 0.0f, 1.0f, 1.0f), tex, GSVector4(dRect), ShaderConvert::DEPTH_COPY, false);
						g_perfmon.Put(GSPerfMon::TextureCopies, 1);
						g_texture_cache->InvalidateTemporaryZ();
						g_texture_cache->SetTemporaryZ(tex);
					}
					else
						DevCon.Warning("HW: Temporary depth buffer creation failed.");
				}
			}
			const bool z_masked = m_cached_ctx.ZBUF.ZMSK;

			if (!m_texture_shuffle && !m_channel_shuffle)
			{
				const bool z_cover = full_cover_clear && (m_r.height() + GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].pgs.y) >= ds->m_valid.height();
				ds->ResizeValidity(z_cover ? m_r : ds->GetUnscaledRect());
				ds->ResizeDrawn(z_cover ? m_r : ds->GetUnscaledRect());
			}

			// Limit to 2x the vertical height of the resolution (for double buffering)
			// Dark cloud writes to 424 when the buffer is only 416 high, but masks the Z.
			// Updating the valid causes the Z to overlap the framebuffer, which is obviously incorrect.
			const bool z_update = (can_update_size || (is_possible_mem_clear && m_vt.m_min.p.z > 0)) && !z_masked;

			if (rt && m_using_temp_z)
			{
				const GSLocalMemory::psm_t& z_psm = GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM];
				const int vertical_offset = ((static_cast<int>(m_cached_ctx.FRAME.Block() - rt->m_TEX0.TBP0) / 32) / std::max(static_cast<int>(rt->m_TEX0.TBW), 1)) * frame_psm.pgs.y;
				const int z_vertical_offset = ((static_cast<int>(m_cached_ctx.ZBUF.Block() - ds->m_TEX0.TBP0) / 32) / std::max(static_cast<int>(ds->m_TEX0.TBW), 1)) * z_psm.pgs.y;
				const int z_horizontal_offset = ((static_cast<int>(m_cached_ctx.ZBUF.Block() - ds->m_TEX0.TBP0) / 32) % std::max(rt->m_TEX0.TBW, 1U)) * z_psm.pgs.x;
				const int horizontal_offset = ((static_cast<int>(m_cached_ctx.FRAME.Block() - rt->m_TEX0.TBP0) / 32) % std::max(static_cast<int>(rt->m_TEX0.TBW), 1)) * frame_psm.pgs.x;
				
				const GSVector4i ds_rect = m_r - GSVector4i(horizontal_offset - z_horizontal_offset, vertical_offset - z_vertical_offset).xyxy();
				ds->UpdateValidity(ds_rect, z_update && (can_update_size || (ds_rect.w <= (resolution.y * 2) && !m_texture_shuffle)));
				ds->UpdateDrawn(ds_rect, z_update && (can_update_size || (ds_rect.w <= (resolution.y * 2) && !m_texture_shuffle)));
			}
			else
			{
				ds->UpdateValidity(m_r, z_update && (can_update_size || m_r.w <= (resolution.y * 2)));
				ds->UpdateDrawn(m_r, z_update && (can_update_size || m_r.w <= (resolution.y * 2)));
			}

			if (!new_rect && new_height && old_end_block != ds->m_end_block)
			{
				old_ds = g_texture_cache->FindTargetOverlap(ds, GSTextureCache::DepthStencil, m_cached_ctx.ZBUF.PSM);

				if (old_ds && old_ds != ds && GSUtil::HasSharedBits(old_ds->m_TEX0.PSM, ds->m_TEX0.PSM))
				{
					const int copy_width = (old_ds->m_texture->GetWidth()) > (ds->m_texture->GetWidth()) ? (ds->m_texture->GetWidth()) : old_ds->m_texture->GetWidth();
					const int copy_height = (old_ds->m_texture->GetHeight()) > (ds->m_texture->GetHeight() - old_height) ? (ds->m_texture->GetHeight() - old_height) : old_ds->m_texture->GetHeight();
					GL_INS("HW: DS double buffer copy from FBP 0x%x, %dx%d => %d,%d", old_ds->m_TEX0.TBP0, copy_width, copy_height, 0, old_height);

					g_gs_device->CopyRect(old_ds->m_texture, ds->m_texture, GSVector4i(0, 0, copy_width, copy_height), 0, old_height);
					preserve_depth = true;
				}
				else
				{
					old_ds = nullptr;
				}
			}
		}
	}
	else
	{
		// RT and DS sizes need to match, even if we're not doing any resizing.
		const int new_w = std::max(rt ? rt->m_unscaled_size.x : 0, ds ? ds->m_unscaled_size.x : 0);
		const int new_h = std::max(rt ? rt->m_unscaled_size.y : 0, ds ? ds->m_unscaled_size.y : 0);
		if (rt)
			rt->ResizeTexture(new_w, new_h);
		if (ds)
			ds->ResizeTexture(new_w, new_h);
	}

	// Hitman Contracts double duties the framebuffer it's messing with and swaps between 16bit and 32bit data, so if it's grabbed the 32bit target, we need to resize it.
	if (!m_texture_shuffle && !m_channel_shuffle && rt && src && src->m_from_target == rt && src->m_target_direct && rt->m_texture == src->m_texture)
	{
		if (GSLocalMemory::m_psm[src->m_from_target_TEX0.PSM].bpp != (GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].bpp))
		{
			GSVector2i new_size = src->m_from_target->m_unscaled_size;

			if (GSLocalMemory::m_psm[src->m_from_target->m_TEX0.PSM].bpp == 32)
				new_size.y *= 2;
			else
				new_size.y /= 2;

			const GSVector4i dRect = GSVector4i(GSVector4(GSVector4i(0, 0, new_size.x, new_size.y)) * rt->m_scale);
			const GSVector2i old_unscaled = rt->m_unscaled_size;
			rt->ResizeTexture(new_size.x, new_size.y, false, true, dRect, true);

			if (GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].bpp >= 16)
			{
				GSTexture* new_tex = rt->m_texture;
				rt->m_texture = src->m_texture;
				rt->m_unscaled_size = old_unscaled;
				src->m_target_direct = false;
				src->m_shared_texture = false;
				src->m_texture = new_tex;
				src->m_unscaled_size = new_size;
				src->m_TEX0.PSM = m_cached_ctx.TEX0.PSM;
			}
		}
	}
	bool skip_draw = false;
	if (!GSConfig.UserHacks_DisableSafeFeatures && is_possible_mem_clear)
		skip_draw = TryTargetClear(rt, ds, preserve_rt_color, preserve_depth);

	if (rt)
	{
		// Always update the preloaded data (marks s_n to last draw or newer)
		if (rt->m_last_draw >= s_n || m_texture_shuffle || m_channel_shuffle || (!rt->m_dirty.empty() && !rt->m_dirty.GetTotalRect(rt->m_TEX0, rt->m_unscaled_size).rintersect(m_r).rempty()))
		{
			const u32 alpha = m_cached_ctx.FRAME.FBMSK >> 24;
			const u32 alpha_mask = GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].fmsk >> 24;
			rt->Update(m_texture_shuffle || (alpha != 0 && (alpha & alpha_mask) != alpha_mask) || (!alpha && (GetAlphaMinMax().max | (m_context->FBA.FBA << 7)) > 128));
		}
		else
			rt->m_age = 0;
	}
	if (ds)
	{
		if (ds->m_last_draw >= s_n || m_texture_shuffle || m_channel_shuffle || (!ds->m_dirty.empty() && !ds->m_dirty.GetTotalRect(ds->m_TEX0, ds->m_unscaled_size).rintersect(m_r).rempty()))
			ds->Update();
		else
			ds->m_age = 0;
	}

	if (src && src->m_shared_texture && src->m_texture != src->m_from_target->m_texture)
	{
		// Target texture changed, update reference.
		src->m_texture = src->m_from_target->m_texture;
	}

	if (GSConfig.ShouldDump(s_n, g_perfmon.GetFrame()))
	{
		const u64 frame = g_perfmon.GetFrame();

		std::string s;

		if (GSConfig.SaveTexture && src)
		{
			s = GetDrawDumpPath("%05d_f%05lld_itex_%s_%05x(%05x)_%s_%d%d_%02x_%02x_%02x_%02x.dds",
				s_n, frame, (src->m_from_target ? "tgt" : "gs"), static_cast<int>(m_cached_ctx.TEX0.TBP0), (src->m_from_target ? src->m_from_target->m_TEX0.TBP0 : src->m_TEX0.TBP0), GSUtil::GetPSMName(m_cached_ctx.TEX0.PSM),
				static_cast<int>(m_cached_ctx.CLAMP.WMS), static_cast<int>(m_cached_ctx.CLAMP.WMT),
				static_cast<int>(m_cached_ctx.CLAMP.MINU), static_cast<int>(m_cached_ctx.CLAMP.MAXU),
				static_cast<int>(m_cached_ctx.CLAMP.MINV), static_cast<int>(m_cached_ctx.CLAMP.MAXV));

			src->m_texture->Save(s);

			if (src->m_palette)
			{
				s = GetDrawDumpPath("%05d_f%05lld_itpx_%05x_%s.dds", s_n, frame, m_cached_ctx.TEX0.CBP, GSUtil::GetPSMName(m_cached_ctx.TEX0.CPSM));

				src->m_palette->Save(s);
			}
		}

		if (rt && GSConfig.SaveRT)
		{
			s = GetDrawDumpPath("%05d_f%05lld_rt0_%05x_(%05x)_%s.bmp", s_n, frame, m_cached_ctx.FRAME.Block(), rt->m_TEX0.TBP0, GSUtil::GetPSMName(m_cached_ctx.FRAME.PSM));

			if (rt->m_texture)
				rt->m_texture->Save(s);
		}

		if (ds && GSConfig.SaveDepth)
		{
			s = GetDrawDumpPath("%05d_f%05lld_rz0_%05x_(%05x)_%s.bmp", s_n, frame, m_cached_ctx.ZBUF.Block(), ds->m_TEX0.TBP0, GSUtil::GetPSMName(m_cached_ctx.ZBUF.PSM));

			if (m_using_temp_z)
				g_texture_cache->GetTemporaryZ()->Save(s);
			else if (ds->m_texture)
				ds->m_texture->Save(s);
		}
	}

	if (m_oi && !m_oi(*this, rt ? rt->m_texture : nullptr, ds ? ds->m_texture : nullptr, src))
	{
		GL_INS("HW: Warning skipping a draw call (%d)", s_n);
		CleanupDraw(true);
		return;
	}

	if (!OI_BlitFMV(rt, src, m_r))
	{
		GL_INS("HW: Warning skipping a draw call (%d)", s_n);
		CleanupDraw(true);
		return;
	}

	// A couple of hack to avoid upscaling issue. So far it seems to impacts mostly sprite
	// Note: first hack corrects both position and texture coordinate
	// Note: second hack corrects only the texture coordinate
	// Be careful to not correct downscaled targets, this can get messy and break post processing
	// but it still needs to adjust native stuff from memory as it's not been compensated for upscaling (Dragon Quest 8 font for example).
	if (CanUpscale() && (m_vt.m_primclass == GS_SPRITE_CLASS) && rt && rt->GetScale() > 1.0f)
	{
		const u32 count = m_vertex.next;
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
				for (u32 i = 0; i < count; i += 2)
				{
					v[i + 1].XYZ.X += 8;
					// I really don't know if it is a good idea. Neither what to do for !PRIM->FST
					if (unaligned_texture)
						v[i + 1].U += 8;
				}
			}
		}

		// Noting to do if no texture is sampled
		if (PRIM->FST && draw_sprite_tex && m_process_texture)
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
	const GSVector4i real_rect = m_r;

	if (!skip_draw)
		DrawPrims(rt, ds, src, tmm);


	// Temporary source *must* be invalidated before normal, because otherwise it'll be double freed.
	g_texture_cache->InvalidateTemporarySource();

	// Invalidation of old targets when changing to double-buffering.
	if (old_rt)
		g_texture_cache->InvalidateVideoMemType(GSTextureCache::RenderTarget, old_rt->m_TEX0.TBP0);
	if (old_ds)
		g_texture_cache->InvalidateVideoMemType(GSTextureCache::DepthStencil, old_ds->m_TEX0.TBP0);

	if ((fm & fm_mask) != fm_mask && rt)
	{
		const bool frame_masked = ((m_cached_ctx.FRAME.FBMSK & frame_psm.fmsk) == frame_psm.fmsk) || (m_cached_ctx.TEST.ATE && m_cached_ctx.TEST.ATST == ATST_NEVER && !(m_cached_ctx.TEST.AFAIL & AFAIL_FB_ONLY));
		//rt->m_valid = rt->m_valid.runion(r);
		// Limit to 2x the vertical height of the resolution (for double buffering)
		rt->UpdateValidity(real_rect, !frame_masked && (can_update_size || (real_rect.w <= (resolution.y * 2) && !m_texture_shuffle)));

	}

	if (ds)
	{
		const bool z_masked = m_cached_ctx.ZBUF.ZMSK;
		const bool was_written = zm != 0xffffffff && m_cached_ctx.DepthWrite();

		//ds->m_valid = ds->m_valid.runion(r);
		// Limit to 2x the vertical height of the resolution (for double buffering)

		if (m_using_temp_z)
		{
			const int get_next_ctx = m_env.PRIM.CTXT;
			const GSDrawingContext& next_ctx = m_env.CTXT[get_next_ctx];
			const int z_vertical_offset = ((static_cast<int>(m_cached_ctx.ZBUF.Block() - ds->m_TEX0.TBP0) / 32) / std::max(static_cast<int>(rt->m_TEX0.TBW), 1)) * GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].pgs.y;
			const int z_horizontal_offset = ((static_cast<int>(m_cached_ctx.ZBUF.Block() - ds->m_TEX0.TBP0) / 32) % std::max(static_cast<int>(rt->m_TEX0.TBW), 1)) * GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].pgs.x;
			const int vertical_offset = ((static_cast<int>(m_cached_ctx.FRAME.Block() - rt->m_TEX0.TBP0) / 32) / std::max(static_cast<int>(rt->m_TEX0.TBW), 1)) * frame_psm.pgs.y;
			const int horizontal_offset = ((static_cast<int>(m_cached_ctx.FRAME.Block() - rt->m_TEX0.TBP0) / 32) % std::max(static_cast<int>(rt->m_TEX0.TBW), 1)) * frame_psm.pgs.x;

			if (was_written)
			{
				const GSVector4i ds_real_rect = real_rect - GSVector4i(horizontal_offset - z_horizontal_offset, vertical_offset - z_vertical_offset).xyxy();
				ds->UpdateValidity(ds_real_rect, !z_masked && (can_update_size || (ds_real_rect.w <= (resolution.y * 2) && !m_texture_shuffle)));
			}

			if (((m_state_flush_reason != CONTEXTCHANGE) || (next_ctx.ZBUF.ZBP == m_context->ZBUF.ZBP && next_ctx.FRAME.FBP == m_context->FRAME.FBP)) && !(m_channel_shuffle && !IsPageCopy()))
			{
				m_temp_z_full_copy |= was_written;
			}
			else
			{
				if (!m_temp_z_full_copy && was_written)
				{
					GSVector4i dRect = GSVector4i((z_horizontal_offset + (real_rect.x - horizontal_offset)) * ds->m_scale, (z_vertical_offset + (real_rect.y - vertical_offset)) * ds->m_scale, ((z_horizontal_offset + real_rect.z + (1.0f / ds->m_scale)) - horizontal_offset) * ds->m_scale, (z_vertical_offset + (real_rect.w + (1.0f / ds->m_scale) - vertical_offset)) * ds->m_scale);
					GSVector4 sRect = GSVector4(
						(real_rect.x * ds->m_scale) / static_cast<float>(g_texture_cache->GetTemporaryZ()->GetWidth()),
						static_cast<float>(real_rect.y * ds->m_scale) / static_cast<float>(g_texture_cache->GetTemporaryZ()->GetHeight()),
						((real_rect.z + (1.0f / ds->m_scale)) * ds->m_scale) / static_cast<float>(g_texture_cache->GetTemporaryZ()->GetWidth()),
						static_cast<float>((real_rect.w + (1.0f / ds->m_scale)) * ds->m_scale) / static_cast<float>(g_texture_cache->GetTemporaryZ()->GetHeight()));

					GL_CACHE("HW: RT in RT Z copy back draw %d z_vert_offset %d rt_vert_offset %d z_horz_offset %d rt_horz_offset %d", s_n, z_vertical_offset, vertical_offset, z_horizontal_offset, horizontal_offset);
					g_gs_device->StretchRect(g_texture_cache->GetTemporaryZ(), sRect, ds->m_texture, GSVector4(dRect), ShaderConvert::DEPTH_COPY, false);
					g_perfmon.Put(GSPerfMon::TextureCopies, 1);
				}
				else if (m_temp_z_full_copy)
				{
					GSVector4i dRect = GSVector4i((ds->m_valid.x + z_horizontal_offset) * ds->m_scale, (ds->m_valid.y + z_vertical_offset) * ds->m_scale, (ds->m_valid.z + z_horizontal_offset + (1.0f / ds->m_scale)) * ds->m_scale, (ds->m_valid.w + z_vertical_offset + (1.0f / ds->m_scale)) * ds->m_scale);
					GSVector4 sRect = GSVector4(
						((ds->m_valid.x + horizontal_offset) * ds->m_scale) / static_cast<float>(g_texture_cache->GetTemporaryZ()->GetWidth()),
						static_cast<float>((ds->m_valid.y + vertical_offset) * ds->m_scale) / static_cast<float>(g_texture_cache->GetTemporaryZ()->GetHeight()),
						(((ds->m_valid.z + horizontal_offset) + (1.0f / ds->m_scale)) * ds->m_scale) / static_cast<float>(g_texture_cache->GetTemporaryZ()->GetWidth()),
						static_cast<float>((ds->m_valid.w + vertical_offset + (1.0f / ds->m_scale)) * ds->m_scale) / static_cast<float>(g_texture_cache->GetTemporaryZ()->GetHeight()));

					GL_CACHE("HW: RT in RT Z copy back draw %d z_vert_offset %d z_offset %d", s_n, z_vertical_offset, vertical_offset);
					g_gs_device->StretchRect(g_texture_cache->GetTemporaryZ(), sRect, ds->m_texture, GSVector4(dRect), ShaderConvert::DEPTH_COPY, false);
					g_perfmon.Put(GSPerfMon::TextureCopies, 1);
				}

				m_temp_z_full_copy = false;
			}
		}
		else if (was_written && g_texture_cache->GetTemporaryZ() != nullptr)
		{
			ds->UpdateValidity(real_rect, !z_masked && (can_update_size || (real_rect.w <= (resolution.y * 2) && !m_texture_shuffle)));
			ds->UpdateDrawn(real_rect, !z_masked && (can_update_size || (real_rect.w <= (resolution.y * 2) && !m_texture_shuffle)));

			GSTextureCache::TempZAddress z_address_info = g_texture_cache->GetTemporaryZInfo();
			if (ds->m_TEX0.TBP0 == z_address_info.ZBP)
			{
				if (z_address_info.rect_since.rempty())
					z_address_info.rect_since = real_rect;
				else
					z_address_info.rect_since = z_address_info.rect_since.runion(real_rect);
				g_texture_cache->SetTemporaryZInfo(z_address_info);
			}
		}
	}

	if (GSConfig.ShouldDump(s_n, g_perfmon.GetFrame()))
	{
		const bool writeback_colclip_texture = g_gs_device->GetColorClipTexture() != nullptr;
		if (writeback_colclip_texture)
		{
			GSTexture* colclip_texture = g_gs_device->GetColorClipTexture();
			g_gs_device->StretchRect(colclip_texture, GSVector4(m_conf.colclip_update_area) / GSVector4(GSVector4i(colclip_texture->GetSize()).xyxy()), rt->m_texture, GSVector4(m_conf.colclip_update_area),
				ShaderConvert::COLCLIP_RESOLVE, false);
		}

		const u64 frame = g_perfmon.GetFrame();

		std::string s;

		if (rt && GSConfig.SaveRT && !m_last_rt)
		{
			s = GetDrawDumpPath("%05d_f%05lld_rt1_%05x_(%05x)_%s.bmp", s_n, frame, m_cached_ctx.FRAME.Block(), rt->m_TEX0.TBP0, GSUtil::GetPSMName(m_cached_ctx.FRAME.PSM));

			rt->m_texture->Save(s);
		}

		if (ds && GSConfig.SaveDepth)
		{
			s = GetDrawDumpPath("%05d_f%05lld_rz1_%05x_%s.bmp", s_n, frame, m_cached_ctx.ZBUF.Block(), GSUtil::GetPSMName(m_cached_ctx.ZBUF.PSM));

			if (m_using_temp_z)
				g_texture_cache->GetTemporaryZ()->Save(s);
			else
				ds->m_texture->Save(s);
		}
	}

	if (rt)
		rt->m_last_draw = s_n;

	if (ds)
		ds->m_last_draw = s_n;

	if ((fm & fm_mask) != fm_mask && !no_rt)
	{
		g_texture_cache->InvalidateVideoMem(context->offset.fb, real_rect, false);

		// Remove overwritten Zs at the FBP.
		g_texture_cache->InvalidateVideoMemType(GSTextureCache::DepthStencil, m_cached_ctx.FRAME.Block(),
			m_cached_ctx.FRAME.PSM, m_texture_shuffle ? GetEffectiveTextureShuffleFbmsk() : fm);

		if (rt && !m_using_temp_z && g_texture_cache->GetTemporaryZ() != nullptr)
		{
			GSTextureCache::TempZAddress temp_z_info = g_texture_cache->GetTemporaryZInfo();
			if (GSLocalMemory::GetStartBlockAddress(rt->m_TEX0.TBP0, rt->m_TEX0.TBW, rt->m_TEX0.PSM, real_rect) <= temp_z_info.ZBP && GSLocalMemory::GetEndBlockAddress(rt->m_TEX0.TBP0, rt->m_TEX0.TBW, rt->m_TEX0.PSM, real_rect) > temp_z_info.ZBP)
				g_texture_cache->InvalidateTemporaryZ();
		}
	}

	if (zm != 0xffffffff && !no_ds)
	{
		g_texture_cache->InvalidateVideoMem(context->offset.zb, real_rect, false);

		// Remove overwritten RTs at the ZBP.
		g_texture_cache->InvalidateVideoMemType(
			GSTextureCache::RenderTarget, m_cached_ctx.ZBUF.Block(), m_cached_ctx.ZBUF.PSM, zm);
	}
#ifdef DISABLE_HW_TEXTURE_CACHE
	if (rt)
		g_texture_cache->Read(rt, real_rect);
#endif

	//

	CleanupDraw(false);
}

/// Verifies assumptions we expect to hold about indices
bool GSRendererHW::VerifyIndices()
{
	switch (m_vt.m_primclass)
	{
		case GS_SPRITE_CLASS:
			if (m_index.tail % 2 != 0)
				return false;
			[[fallthrough]];
		case GS_POINT_CLASS:
			// Expect indices to be flat increasing
			for (u32 i = 0; i < m_index.tail; i++)
			{
				if (m_index.buff[i] != i)
					return false;
			}
			break;
		case GS_LINE_CLASS:
			if (m_index.tail % 2 != 0)
				return false;
			// Expect each line to be a pair next to each other
			// VS expand relies on this!
			for (u32 i = 0; i < m_index.tail; i += 2)
			{
				if (m_index.buff[i] + 1 != m_index.buff[i + 1])
					return false;
			}
			break;
		case GS_TRIANGLE_CLASS:
			if (m_index.tail % 3 != 0)
				return false;
			break;
		case GS_INVALID_CLASS:
			break;
	}
	return true;
}

// Fix the colors in vertices in case the API only supports "provoking first vertex"
// (i.e., when using flat shading the color comes from the first vertex, unlike PS2
// which is "provoking last vertex").
void GSRendererHW::HandleProvokingVertexFirst()
{
	                                                     // Early exit conditions:
	if (g_gs_device->Features().provoking_vertex_last || // device supports provoking last vertex
	    m_conf.vs.iip ||                                 // we are doing Gouraud shading
	    m_vt.m_primclass == GS_POINT_CLASS ||            // drawing points (one vertex per primitive; color is unambiguous)
	    m_vt.m_primclass == GS_SPRITE_CLASS)             // drawing sprites (handled by the sprites -> triangles expand shader)
		return;

	const int n = GSUtil::GetClassVertexCount(m_vt.m_primclass);

	// If all first/last vertices have the same color there is nothing to do.
	bool first_eq_last = true;
	for (u32 i = 0; i < m_index.tail; i += n)
	{
		if (m_vertex.buff[m_index.buff[i]].RGBAQ.U32[0] != m_vertex.buff[m_index.buff[i + n - 1]].RGBAQ.U32[0])
		{
			first_eq_last = false;
			break;
		}
	}
	if (first_eq_last)
		return;

	// De-index the vertices using the copy buffer
	while (m_vertex.maxcount < m_index.tail)
		GrowVertexBuffer();
	for (int i = static_cast<int>(m_index.tail) - 1; i >= 0; i--)
	{
		m_vertex.buff_copy[i] = m_vertex.buff[m_index.buff[i]];
		m_index.buff[i] = static_cast<u16>(i);
	}
	std::swap(m_vertex.buff, m_vertex.buff_copy);
	m_vertex.head = m_vertex.next = m_vertex.tail = m_index.tail;

	// Put correct color in the first vertex
	for (u32 i = 0; i < m_index.tail; i += n)
	{
		m_vertex.buff[i].RGBAQ.U32[0] = m_vertex.buff[i + n - 1].RGBAQ.U32[0];
		m_vertex.buff[i + n - 1].RGBAQ.U32[0] = 0xff; // Make last vertex red for debugging if used improperly
	}
}

void GSRendererHW::SetupIA(float target_scale, float sx, float sy, bool req_vert_backup)
{
	GL_PUSH("HW: IA");

	if (GSConfig.UserHacks_ForceEvenSpritePosition && !m_isPackedUV_HackFlag && m_process_texture && PRIM->FST)
	{
		for (u32 i = 0; i < m_vertex.next; i++)
			m_vertex.buff[i].UV &= 0x3FEF3FEF;
	}

	const bool unscale_pt_ln = !GSConfig.UserHacks_DisableSafeFeatures && (target_scale != 1.0f);
	const GSDevice::FeatureSupport features = g_gs_device->Features();

	pxAssert(VerifyIndices());

	switch (m_vt.m_primclass)
	{
		case GS_POINT_CLASS:
			{
				m_conf.topology = GSHWDrawConfig::Topology::Point;
				m_conf.indices_per_prim = 1;
				if (unscale_pt_ln)
				{
					if (features.point_expand)
					{
						m_conf.vs.point_size = true;
						m_conf.cb_vs.point_size = GSVector2(target_scale);
					}
					else if (features.vs_expand)
					{
						m_conf.vs.expand = GSHWDrawConfig::VSExpand::Point;
						m_conf.cb_vs.point_size = GSVector2(16.0f * sx, 16.0f * sy);
						m_conf.topology = GSHWDrawConfig::Topology::Triangle;
						m_conf.verts = m_vertex.buff;
						m_conf.nverts = m_vertex.next;
						m_conf.nindices = m_index.tail * 6;
						m_conf.indices_per_prim = 6;
						return;
					}
				}
				else
				{
					// Vulkan/GL still need to set point size.
					m_conf.cb_vs.point_size = target_scale;

					// M1 requires point size output on *all* points.
					m_conf.vs.point_size = true;
				}
			}
			break;

		case GS_LINE_CLASS:
			{
				m_conf.topology = GSHWDrawConfig::Topology::Line;
				m_conf.indices_per_prim = 2;
				if (unscale_pt_ln)
				{
					if (features.line_expand)
					{
						m_conf.line_expand = true;
					}
					else if (features.vs_expand)
					{
						m_conf.vs.expand = GSHWDrawConfig::VSExpand::Line;
						m_conf.cb_vs.point_size = GSVector2(16.0f * sx, 16.0f * sy);
						m_conf.topology = GSHWDrawConfig::Topology::Triangle;
						m_conf.indices_per_prim = 6;
						ExpandLineIndices();
					}
				}
			}
			break;

		case GS_SPRITE_CLASS:
			{
				// Need to pre-divide ST by Q if Q is very large, to avoid precision issues on some GPUs.
				// May as well just expand the whole thing out with the CPU path in such a case.
				if (features.vs_expand && !m_vt.m_accurate_stq)
				{
					m_conf.topology = GSHWDrawConfig::Topology::Triangle;
					m_conf.vs.expand = GSHWDrawConfig::VSExpand::Sprite;

					if (req_vert_backup)
					{
						memcpy(m_draw_vertex.buff, m_vertex.buff, sizeof(GSVertex) * m_vertex.next);
						memcpy(m_draw_index.buff, m_index.buff, sizeof(u16) * m_index.tail);

						m_conf.verts = m_draw_vertex.buff;
						m_conf.indices = m_draw_index.buff;
					}
					else
					{
						m_conf.verts = m_vertex.buff;
						m_conf.indices = m_index.buff;
					}
					m_conf.nverts = m_vertex.next;
					m_conf.nindices = m_index.tail * 3;
					m_conf.indices_per_prim = 6;
					return;
				}
				else
				{
					Lines2Sprites();

					m_conf.topology = GSHWDrawConfig::Topology::Triangle;
					m_conf.indices_per_prim = 6;
				}
			}
			break;

		case GS_TRIANGLE_CLASS:
			{
				m_conf.topology = GSHWDrawConfig::Topology::Triangle;
				m_conf.indices_per_prim = 3;

				// See note above in GS_SPRITE_CLASS.
				if (m_vt.m_accurate_stq && m_vt.m_eq.stq) [[unlikely]]
				{
					GSVertex* const v = m_vertex.buff;
					const GSVector4 v_q = GSVector4(v[0].RGBAQ.Q);
					for (u32 i = 0; i < m_vertex.next; i++)
					{
						// v[i].ST.ST /= v[i].RGBAQ.Q; v[i].RGBAQ.Q = 1.0f; (Q / Q = 1)
						GSVector4 v_st = GSVector4::load<true>(&v[i].ST);
						v_st = (v_st / v_q).insert32<2, 2>(v_st);
						GSVector4::store<true>(&v[i].ST, v_st);
					}
				}
			}
			break;

		default:
			ASSUME(0);
	}

	if (req_vert_backup)
	{
		memcpy(m_draw_vertex.buff, m_vertex.buff, sizeof(GSVertex) * m_vertex.next);
		memcpy(m_draw_index.buff, m_index.buff, sizeof(u16) * m_index.tail);

		m_conf.verts = m_draw_vertex.buff;
		m_conf.indices = m_draw_index.buff;
	}
	else
	{
		m_conf.verts = m_vertex.buff;
		m_conf.indices = m_index.buff;
	}
	m_conf.nverts = m_vertex.next;
	m_conf.nindices = m_index.tail;
}

void GSRendererHW::EmulateZbuffer(const GSTextureCache::Target* ds)
{
	if (ds && m_cached_ctx.TEST.ZTE)
	{
		m_conf.depth.ztst = m_cached_ctx.TEST.ZTST;
		// AA1: Z is not written on lines since coverage is always less than 0x80.
		m_conf.depth.zwe = (m_cached_ctx.ZBUF.ZMSK || (PRIM->AA1 && m_vt.m_primclass == GS_LINE_CLASS)) ? 0 : 1;
	}
	else
	{
		m_conf.depth.ztst = ZTST_ALWAYS;
	}

	// On the real GS we appear to do clamping on the max z value the format allows.
	// Clamping is done after rasterization.
	const u32 max_z = 0xFFFFFFFF >> (GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].fmt * 8);
	const bool clamp_z = static_cast<u32>(GSVector4i(m_vt.m_max.p).z) > max_z;

	m_conf.cb_vs.max_depth = GSVector2i(0xFFFFFFFF);
	//ps_cb.MaxDepth = GSVector4(0.0f, 0.0f, 0.0f, 1.0f);
	m_conf.ps.zclamp = 0;

	if (clamp_z)
	{
		if (m_vt.m_primclass == GS_SPRITE_CLASS || m_vt.m_primclass == GS_POINT_CLASS)
		{
			m_conf.cb_vs.max_depth = GSVector2i(max_z);
		}
		else if (!m_cached_ctx.ZBUF.ZMSK)
		{
			m_conf.cb_ps.TA_MaxDepth_Af.z = static_cast<float>(max_z) * 0x1p-32f;
			m_conf.ps.zclamp = 1;
		}
	}
}

void GSRendererHW::EmulateTextureShuffleAndFbmask(GSTextureCache::Target* rt, GSTextureCache::Source* tex)
{
	// Uncomment to disable texture shuffle emulation.
	// m_texture_shuffle = false;

	const bool enable_fbmask_emulation = GSConfig.AccurateBlendingUnit != AccBlendLevel::Minimum;
	const GSDevice::FeatureSupport features = g_gs_device->Features();

	if (m_texture_shuffle)
	{
		m_conf.ps.shuffle = 1;
		m_conf.ps.dst_fmt = GSLocalMemory::PSM_FMT_32;

		u32 process_rg = 0;
		u32 process_ba = 0;
		bool shuffle_across = true;

		ConvertSpriteTextureShuffle(process_rg, process_ba, shuffle_across, rt, tex);

		if (m_index.tail == 0)
			return; // Rewriting sprites can result in an empty draw.

		// If date is enabled you need to test the green channel instead of the alpha channel.
		// Only enable this code in DATE mode to reduce the number of shaders.
		m_conf.ps.write_rg = (process_rg & SHUFFLE_WRITE) && (features.texture_barrier || features.multidraw_fb_copy) && m_cached_ctx.TEST.DATE;
		m_conf.ps.real16src = m_copy_16bit_to_target_shuffle;
		m_conf.ps.shuffle_same = m_same_group_texture_shuffle;
		// Please bang my head against the wall!
		// 1/ Reduce the frame mask to a 16 bit format
		const u32 m = m_cached_ctx.FRAME.FBMSK & GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].fmsk;

		// fbmask is converted to a 16bit version to represent the 2 32bit channels it's writing to.
		// The lower 8 bits represents the Red/Blue channels, the top 8 bits is Green/Alpha, depending on write_ba.
		const u32 fbmask = ((m >> 3) & 0x1F) | ((m >> 6) & 0x3E0) | ((m >> 9) & 0x7C00) | ((m >> 16) & 0x8000);
		// r = rb mask, g = ga mask
		const GSVector2i rb_ga_mask = GSVector2i(fbmask & 0xFF, (fbmask >> 8) & 0xFF);

		m_conf.ps.process_rg = process_rg;
		m_conf.ps.process_ba = process_ba;
		m_conf.ps.shuffle_across = shuffle_across;
		// Ace Combat 04 sets FBMSK to 0 for the shuffle, duplicating RG across RGBA.
		// Given how touchy texture shuffles are, I'm not ready to make it 100% dependent on the real FBMSK yet.
		// TODO: Remove this if, and see what breaks.
		m_conf.colormask.wrgba = 0;

		// 2 Select the new mask
		if (rb_ga_mask.r != 0xFF)
		{
			if (process_ba & SHUFFLE_WRITE)
			{
				GL_INS("HW: Color shuffle %s => B", ((process_rg & SHUFFLE_READ) && shuffle_across) ? "R" : "B");
				m_conf.colormask.wb = 1;
			}

			if (process_rg & SHUFFLE_WRITE)
			{
				GL_INS("HW: Color shuffle %s => R", ((process_ba & SHUFFLE_READ) && shuffle_across) ? "B" : "R");
				m_conf.colormask.wr = 1;
			}
			if (rb_ga_mask.r)
				m_conf.ps.fbmask = 1;
		}

		if (rb_ga_mask.g != 0xFF)
		{
			if (process_ba & SHUFFLE_WRITE)
			{
				GL_INS("HW: Color shuffle %s => A", ((process_rg & SHUFFLE_READ) && shuffle_across) ? "G" : "A");
				m_conf.colormask.wa = 1;
			}

			if (process_rg & SHUFFLE_WRITE)
			{
				GL_INS("HW: Color shuffle %s => G", ((process_ba & SHUFFLE_READ) && shuffle_across) ? "A" : "G");
				m_conf.colormask.wg = 1;
			}
			if (rb_ga_mask.g)
				m_conf.ps.fbmask = 1;
		}

		if (m_conf.ps.fbmask && enable_fbmask_emulation)
		{
			m_conf.cb_ps.FbMask.r = rb_ga_mask.r;
			m_conf.cb_ps.FbMask.g = rb_ga_mask.g;
			m_conf.cb_ps.FbMask.b = rb_ga_mask.r;
			m_conf.cb_ps.FbMask.a = rb_ga_mask.g;

			// No need for full barrier on fbmask with shuffle.
			GL_INS("HW: FBMASK SW emulated fb_mask:%x on tex shuffle", fbmask);
			m_conf.require_one_barrier = true;
		}
		else
		{
			m_conf.ps.fbmask = 0;
		}

		// Set dirty alpha on target, but only if we're actually writing to it.
		rt->m_valid_alpha_low |= m_conf.colormask.wa;
		rt->m_valid_alpha_high |= m_conf.colormask.wa;

		// Once we draw the shuffle, no more buffering.
		m_split_texture_shuffle_pages = 0;
		m_split_texture_shuffle_pages_high = 0;
		m_split_texture_shuffle_start_FBP = 0;
		m_split_texture_shuffle_start_TBP = 0;

		// Get rid of any clamps, we're basically overriding this (more of an issue for D3D).
		if (m_cached_ctx.CLAMP.WMS > CLAMP_CLAMP)
			m_cached_ctx.CLAMP.WMS = m_cached_ctx.CLAMP.WMS == CLAMP_REGION_CLAMP ? CLAMP_CLAMP : CLAMP_REPEAT;
		if (m_cached_ctx.CLAMP.WMT > CLAMP_CLAMP)
			m_cached_ctx.CLAMP.WMT = m_cached_ctx.CLAMP.WMT == CLAMP_REGION_CLAMP ? CLAMP_CLAMP : CLAMP_REPEAT;

		m_primitive_covers_without_gaps = rt->m_valid.rintersect(m_r).eq(rt->m_valid) ? NoGapsType::FullCover : NoGapsType::GapsFound;
	}
	else
	{
		m_conf.ps.dst_fmt = GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].fmt;

		// Don't allow only unused bits on 16bit format to enable fbmask,
		// let's set the mask to 0 in such cases.
		int fbmask = static_cast<int>(m_cached_ctx.FRAME.FBMSK);
		const int fbmask_r = GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].fmsk;
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
			if (!PRIM->ABE || !(~ff_fbmask & ~zero_fbmask & 0x7) || !(features.texture_barrier || features.multidraw_fb_copy))
			{
				GL_INS("HW: FBMASK Unsafe SW emulated fb_mask:%x on %d bits format", m_cached_ctx.FRAME.FBMSK,
					(m_conf.ps.dst_fmt == GSLocalMemory::PSM_FMT_16) ? 16 : 32);
				m_conf.require_one_barrier = true;
			}
			else
			{
				// The safe and accurate path (but slow)
				GL_INS("HW: FBMASK SW emulated fb_mask:%x on %d bits format", m_cached_ctx.FRAME.FBMSK,
					(m_conf.ps.dst_fmt == GSLocalMemory::PSM_FMT_16) ? 16 : 32);
				m_conf.require_full_barrier = true;
			}
		}
	}
}

bool GSRendererHW::TestChannelShuffle(GSTextureCache::Target* src)
{
	// We have to do the second test early here, because it might be a different source.
	const bool shuffle = m_channel_shuffle || IsPossibleChannelShuffle();

	// This is a little redundant since it'll get called twice, but the only way to stop us wasting time on copies.
	m_channel_shuffle = (shuffle && EmulateChannelShuffle(src, true));
	return m_channel_shuffle;
}

__ri bool GSRendererHW::EmulateChannelShuffle(GSTextureCache::Target* src, bool test_only, GSTextureCache::Target* rt)
{
	if ((src->m_texture->GetType() == GSTexture::Type::DepthStencil) && !src->m_32_bits_fmt)
	{
		// So far 2 games hit this code path. Urban Chaos and Tales of Abyss
		// UC: will copy depth to green channel
		// ToA: will copy depth to alpha channel
		if ((m_cached_ctx.FRAME.FBMSK & 0x00FF0000) == 0x00FF0000)
		{
			// Green channel is masked
			GL_INS("HW: HLE Shuffle Tales Of Abyss");
			if (test_only)
				return true;

			m_conf.ps.tales_of_abyss_hle = 1;
		}
		else
		{
			GL_INS("HW: HLE Shuffle Urban Chaos");
			if (test_only)
				return true;

			m_conf.ps.urban_chaos_hle = 1;
		}
	}
	else if (m_index.tail <= 64 && !IsPageCopy() && m_cached_ctx.CLAMP.WMT == 3)
	{
		// Blood will tell. I think it is channel effect too but again
		// implemented in a different way. I don't want to add more CRC stuff. So
		// let's disable channel when the signature is different
		//
		// Note: Tales Of Abyss and Tekken5 could hit this path too. Those games are
		// handled above.
		GL_INS("HW: Might not be channel shuffle");
		if (test_only)
			return false;

		m_channel_shuffle = false;
		return false;
	}
	else if (m_cached_ctx.CLAMP.WMS == 3 && ((m_cached_ctx.CLAMP.MAXU & 0x8) == 8))
	{
		// MGS3/Kill Zone
		if (test_only)
			return true;

		const ChannelFetch channel_select = ((m_cached_ctx.CLAMP.WMT != 3 && (m_vertex.buff[m_index.buff[0]].V & 0x20) == 0) || (m_cached_ctx.CLAMP.WMT == 3 && ((m_cached_ctx.CLAMP.MAXV & 0x2) == 0))) ? ChannelFetch_BLUE : ChannelFetch_ALPHA;

		GL_INS("HW: %s channel", (channel_select == ChannelFetch_BLUE) ? "blue" : "alpha");

		m_conf.ps.channel = channel_select;
	}
	else if (m_cached_ctx.CLAMP.WMS == 3 && ((m_cached_ctx.CLAMP.MINU & 0x8) == 0))
	{
		// Read either Red or Green. Let's check the V coordinate. 0-1 is likely top so
		// red. 2-3 is likely bottom so green (actually depends on texture base pointer offset)
		const bool green = PRIM->FST && (m_vertex.buff[0].V & 32);
		if (green && (m_cached_ctx.FRAME.FBMSK & 0x00FFFFFF) == 0x00FFFFFF)
		{
			// Typically used in Terminator 3
			const int blue_mask = m_cached_ctx.FRAME.FBMSK >> 24;
			int blue_shift = -1;

			// Note: potentially we could also check the value of the clut
			switch (blue_mask)
			{
				case 0xFF: pxAssert(0);      break;
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

				GL_INS("HW: Green/Blue channel (%d, %d)", blue_shift, green_shift);
				if (test_only)
					return true;

				m_conf.cb_ps.ChannelShuffle = GSVector4i(blue_mask, blue_shift, green_mask, green_shift);
				m_conf.ps.channel = ChannelFetch_GXBY;
				m_cached_ctx.FRAME.FBMSK = 0x00FFFFFF;
			}
			else
			{
				GL_INS("HW: Green channel (wrong mask) (fbmask %x)", blue_mask);
				if (test_only)
					return true;

				m_conf.ps.channel = ChannelFetch_GREEN;
			}
		}
		else if (green)
		{
			GL_INS("HW: Green channel");
			if (test_only)
				return true;

			m_conf.ps.channel = ChannelFetch_GREEN;
		}
		else
		{
			// Pop
			GL_INS("HW: Red channel");
			if (test_only)
				return true;

			m_conf.ps.channel = ChannelFetch_RED;
		}
	}
	else
	{
		// We can use the minimum UV to work out which channel it's grabbing.
		// Used by Ape Escape 2, Everybody's Tennis/Golf, Okage, and Valkyrie Profile 2.
		// Page align test to limit false detections (there is a few).
		GSVector4i min_uv = GSVector4i(m_vt.m_min.t.upld(GSVector4::zero()));
		ChannelFetch channel = ChannelFetch_NONE;
		const GSLocalMemory::psm_t& t_psm = GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM];
		const GSLocalMemory::psm_t& f_psm = GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM];
		GSVector4i block_offset = GSVector4i(min_uv.x / t_psm.bs.x, min_uv.y / t_psm.bs.y).xyxy();
		GSVector4i m_r_block_offset = GSVector4i((m_r.x & (f_psm.pgs.x - 1)) / f_psm.bs.x, (m_r.y & (f_psm.pgs.y - 1)) / f_psm.bs.y);

		// Adjust it back to the page boundary
		min_uv.x -= block_offset.x * t_psm.bs.x;
		min_uv.y -= block_offset.y * t_psm.bs.y;
		// Mask the channel.
		min_uv.y &= 2;
		min_uv.x &= 8;
		//if (/*GSLocalMemory::IsPageAligned(src->m_TEX0.PSM, m_r) &&*/
		//	block_offset.eq(m_r_block_offset))
		{
			if (min_uv.eq(GSVector4i::cxpr(0, 0, 0, 0)))
				channel = ChannelFetch_RED;
			else if (min_uv.eq(GSVector4i::cxpr(0, 2, 0, 0)))
				channel = ChannelFetch_GREEN;
			else if (min_uv.eq(GSVector4i::cxpr(8, 0, 0, 0)))
				channel = ChannelFetch_BLUE;
			else if (min_uv.eq(GSVector4i::cxpr(8, 2, 0, 0)))
				channel = ChannelFetch_ALPHA;
		}

		if (channel != ChannelFetch_NONE)
		{
#ifdef ENABLE_OGL_DEBUG
			static constexpr const char* channel_names[] = { "Red", "Green", "Blue", "Alpha" };
			GL_INS("HW: %s channel from min UV: r={%d,%d=>%d,%d} min uv = %d,%d", channel_names[static_cast<u32>(channel - 1)],
				m_r.x, m_r.y, m_r.z, m_r.w, min_uv.x, min_uv.y);
#endif

			if (test_only)
				return true;

			m_conf.ps.channel = channel;
		}
		else
		{
			GL_INS("HW: Channel not supported r={%d,%d=>%d,%d} min uv = %d,%d",
				m_r.x, m_r.y, m_r.z, m_r.w, min_uv.x, min_uv.y);

			if (test_only)
				return false;

			m_channel_shuffle = false;
			return false;
		}
	}

	pxAssert(m_channel_shuffle);

	// Effect is really a channel shuffle effect so let's cheat a little
	m_conf.tex = src->m_texture;

	// Replace current draw with a fullscreen sprite
	//
	// Performance GPU note: it could be wise to reduce the size to
	// the rendered size of the framebuffer

	const GSLocalMemory::psm_t frame_psm = GSLocalMemory::m_psm[m_context->FRAME.PSM];
	m_full_screen_shuffle = (m_r.height() > frame_psm.pgs.y) || (m_r.width() > frame_psm.pgs.x) || GSConfig.UserHacks_TextureInsideRt == GSTextureInRtMode::Disabled;
	m_channel_shuffle_src_valid = src->m_valid;
	if (GSConfig.UserHacks_TextureInsideRt == GSTextureInRtMode::Disabled || ((src->m_TEX0.TBW == rt->m_TEX0.TBW) && (!m_in_target_draw && IsPageCopy())) || m_conf.ps.urban_chaos_hle || m_conf.ps.tales_of_abyss_hle)
	{
		GSVertex* s = &m_vertex.buff[0];
		s[0].XYZ.X = static_cast<u16>(m_context->XYOFFSET.OFX + 0);
		s[1].XYZ.X = static_cast<u16>(m_context->XYOFFSET.OFX + 16384);
		s[0].XYZ.Y = static_cast<u16>(m_context->XYOFFSET.OFY + 0);
		s[1].XYZ.Y = static_cast<u16>(m_context->XYOFFSET.OFY + 16384);

		s[0].U = 0;
		s[1].U = 16384;
		s[0].V = 0;
		s[1].V = 16384;

		m_r = GSVector4i(0, 0, 1024, 1024);

		// We need to count the pages that get shuffled to, some games (like Hitman Blood Money dialogue blur effects) only do half the screen.
		if (!m_full_screen_shuffle && !m_conf.ps.urban_chaos_hle && !m_conf.ps.tales_of_abyss_hle && src)
		{
			// We've probably gotten a fake number, so just reset it, it'll be updated again later.
			if (rt->m_last_draw >= s_n)
				rt->ResizeValidity(GSVector4i::zero());

			m_channel_shuffle_width = src->m_TEX0.TBW;
		}
	}
	else
	{
		const u32 frame_page_offset = std::max(static_cast<int>(((m_r.x / frame_psm.pgs.x) + (m_r.y / frame_psm.pgs.y) * rt->m_TEX0.TBW)), 0);
		m_r = GSVector4i(m_r.x & ~(frame_psm.pgs.x - 1), m_r.y & ~(frame_psm.pgs.y - 1), (m_r.z + (frame_psm.pgs.x - 1)) & ~(frame_psm.pgs.x - 1), (m_r.w + (frame_psm.pgs.y - 1)) & ~(frame_psm.pgs.y - 1));

		// Hitman suffers from this, not sure on the exact scenario at the moment, but we need the barrier.
		if (NeedsBlending() && m_context->ALPHA.IsCdInBlend())
		{
			// Needed to enable IsFeedbackLoop.
			m_conf.ps.channel_fb = 1;
			// Assume no overlap when it's a channel shuffle, no need for full barriers.
			m_conf.require_one_barrier = true;
		}

		// This is for offsetting the texture, however if the texture has a region clamp, we don't want to move it.
		// A good two test games for this is Ghost in the Shell (no region clamp) and Tekken 5 (offset clamp on shadows)
		if (rt && rt->m_TEX0.TBP0 == m_cached_ctx.FRAME.Block())
		{
			const bool req_offset = (m_cached_ctx.CLAMP.WMS != 3 || (m_cached_ctx.CLAMP.MAXU & ~0xF) == 0) &&
			                        (m_cached_ctx.CLAMP.WMT != 3 || (m_cached_ctx.CLAMP.MAXV & ~0x3) == 0);
			//DevCon.Warning("HW: Draw %d offset %d", s_n, frame_page_offset);
			// Offset the frame but clear the draw offset
			if (req_offset)
				m_cached_ctx.FRAME.FBP += frame_page_offset;
		}

		m_in_target_draw |= frame_page_offset > 0;
		GSVertex* s = &m_vertex.buff[0];
		s[0].XYZ.X = static_cast<u16>(m_context->XYOFFSET.OFX + (m_r.x << 4));
		s[1].XYZ.X = static_cast<u16>(m_context->XYOFFSET.OFX + (m_r.z << 4));
		s[0].XYZ.Y = static_cast<u16>(m_context->XYOFFSET.OFY + (m_r.y << 4));
		s[1].XYZ.Y = static_cast<u16>(m_context->XYOFFSET.OFY + (m_r.w << 4));

		s[0].U = m_r.x << 4;
		s[1].U = m_r.z << 4;
		s[0].V = m_r.y << 4;
		s[1].V = m_r.w << 4;
		m_last_channel_shuffle_fbmsk = 0xFFFFFFFF;

		// If we're doing per page copying, then set the valid 1 frame ahead if we're continuing, as this will save the target lookup making a new target for the new row.
		const u32 frame_offset = m_cached_ctx.FRAME.Block() + (IsPageCopy() ? 0x20 : 0);
		GSVector4i new_valid = rt->m_valid;
		int offset_height = static_cast<int>((((frame_offset - rt->m_TEX0.TBP0) >> 5) / rt->m_TEX0.TBW) * frame_psm.pgs.y) + frame_psm.pgs.y;

		const int get_next_ctx = (m_state_flush_reason == CONTEXTCHANGE) ? m_env.PRIM.CTXT : m_backed_up_ctx;
		const GSDrawingContext& next_ctx = m_env.CTXT[get_next_ctx];
		const u32 safe_TBW = std::max(rt->m_TEX0.TBW, 1U);
		// This is an annoying case where the draw is offset to draw on the right hand side of a texture (Hitman Blood Money pause screen).
		if (m_state_flush_reason == GSFlushReason::CONTEXTCHANGE && !IsPageCopy() && NextDrawMatchesShuffle() && next_ctx.FRAME.FBP > m_cached_ctx.FRAME.FBP && (next_ctx.FRAME.FBP < (m_cached_ctx.FRAME.FBP + safe_TBW)) &&
			(next_ctx.FRAME.FBP - m_cached_ctx.FRAME.FBP) < safe_TBW && (next_ctx.FRAME.FBP % safe_TBW) != ((m_cached_ctx.FRAME.FBP % safe_TBW) + 1))
		{
			offset_height += frame_psm.pgs.y;
		}

		new_valid.w = std::max(new_valid.w, offset_height);
		rt->UpdateValidity(new_valid, true);
	}

	m_vertex.head = m_vertex.tail = m_vertex.next = 2;
	m_index.tail = 2;

	m_primitive_covers_without_gaps = NoGapsType::FullCover;
	m_channel_shuffle_abort = false;

	return true;
}

void GSRendererHW::EmulateBlending(int rt_alpha_min, int rt_alpha_max, const bool DATE, bool& DATE_PRIMID, bool& DATE_BARRIER,
	GSTextureCache::Target* rt, bool can_scale_rt_alpha, bool& new_rt_alpha_scale)
{
	const GIFRegALPHA& ALPHA = m_context->ALPHA;
	{
		// AA1: Blending needs to be enabled on draw.
		const bool AA1 = PRIM->AA1 && (m_vt.m_primclass == GS_LINE_CLASS || m_vt.m_primclass == GS_TRIANGLE_CLASS);
		// PABE: Check condition early as an optimization, no blending when As < 128.
		// For Cs*As + Cd*(1 - As) if As is 128 then blending can be disabled as well.
		const bool PABE_skip = m_draw_env->PABE.PABE &&
			((GetAlphaMinMax().max < 128) || (GetAlphaMinMax().max == 128 && ALPHA.A == 0 && ALPHA.B == 1 && ALPHA.C == 0 && ALPHA.D == 1));

		// No blending or coverage anti-aliasing so early exit
		if (PABE_skip || !(NeedsBlending() || AA1))
		{
			m_conf.blend = {};
			m_conf.ps.no_color1 = true;

			// TODO: Find games that may benefit from adding full coverage on RTA Scale when we're overwriting the whole target.

			return;
		}
	}

	// Compute the blending equation to detect special case
	const GSDevice::FeatureSupport features(g_gs_device->Features());
	const GIFRegCOLCLAMP& COLCLAMP = m_draw_env->COLCLAMP;
	// AFIX: Afix factor.
	u8 AFIX = ALPHA.FIX;

	// Set blending to shader bits
	m_conf.ps.blend_a = ALPHA.A;
	m_conf.ps.blend_b = ALPHA.B;
	m_conf.ps.blend_c = ALPHA.C;
	m_conf.ps.blend_d = ALPHA.D;

#ifdef ENABLE_OGL_DEBUG
	static constexpr const char* col[3] = {"Cs", "Cd", "0"};
	static constexpr const char* alpha[3] = {"As", "Ad", "Af"};
	GL_INS("HW: EmulateBlending(): (%s - %s) * %s + %s", col[ALPHA.A], col[ALPHA.B], alpha[ALPHA.C], col[ALPHA.D]);
	GL_INS("HW: Draw AlphaMinMax: %d-%d, RT AlphaMinMax: %d-%d, AFIX: %u", GetAlphaMinMax().min, GetAlphaMinMax().max, rt_alpha_min, rt_alpha_max, AFIX);
#endif

	// If the colour is modulated to zero or we're not using a texture and the color is zero, we can replace any Cs with 0
	if ((!PRIM->TME || m_cached_ctx.TEX0.TFX != TFX_DECAL) && (!PRIM->FGE || m_draw_env->FOGCOL.U32[0] == 0) &&
		((m_vt.m_max.c == GSVector4i::zero()).mask() & 0xfff) == 0xfff)
	{
		// If using modulate or is HIGHLIGHT by the vertex alpha is zero, we should be safe to kill it.
		if (!PRIM->TME || m_cached_ctx.TEX0.TFX == TFX_MODULATE || m_vt.m_max.c.a == 0)
		{
			if (m_conf.ps.blend_a == 0)
				m_conf.ps.blend_a = 2;

			if (m_conf.ps.blend_b == 0)
				m_conf.ps.blend_b = 2;

			if (m_conf.ps.blend_d == 0)
				m_conf.ps.blend_d = 2;
		}
	}
	if (m_conf.ps.blend_c == 1)
	{
		// When both rt alpha min and max are equal replace Ad with Af, easier to manage.
		if (rt_alpha_min == rt_alpha_max)
		{
			AFIX = rt_alpha_min;
			m_conf.ps.blend_c = 2;
		}
		// 24 bits doesn't have an alpha channel so use 128 (1.0f) fix factor as equivalent.
		else if (m_conf.ps.dst_fmt == GSLocalMemory::PSM_FMT_24)
		{
			AFIX = 128;
			m_conf.ps.blend_c = 2;
		}
	}
	else if (m_conf.ps.blend_c == 0 && GetAlphaMinMax().min == GetAlphaMinMax().max)
	{
		AFIX = GetAlphaMinMax().max;
		m_conf.ps.blend_c = 2;
	}

	// Get alpha value
	const bool alpha_c0_eq_zero = (m_conf.ps.blend_c == 0 && GetAlphaMinMax().max == 0);
	const bool alpha_c0_eq_one = (m_conf.ps.blend_c == 0 && (GetAlphaMinMax().min == 128) && (GetAlphaMinMax().max == 128));
	const bool alpha_c0_high_min_one = (m_conf.ps.blend_c == 0 && GetAlphaMinMax().min > 128);
	const bool alpha_c0_high_max_one = (m_conf.ps.blend_c == 0 && GetAlphaMinMax().max > 128);
	const bool alpha_c0_eq_less_max_one = (m_conf.ps.blend_c == 0 && GetAlphaMinMax().max <= 128);
	const bool alpha_c1_high_min_one = (m_conf.ps.blend_c == 1 && rt_alpha_min > 128);
	const bool alpha_c1_high_max_one = (m_conf.ps.blend_c == 1 && rt_alpha_max > 128);
	const bool alpha_c1_eq_less_max_one = (m_conf.ps.blend_c == 1 && rt_alpha_max <= 128);
	bool alpha_c1_high_no_rta_correct = m_conf.ps.blend_c == 1 && !(new_rt_alpha_scale || can_scale_rt_alpha);
	const bool alpha_c2_eq_zero = (m_conf.ps.blend_c == 2 && AFIX == 0u);
	const bool alpha_c2_eq_one = (m_conf.ps.blend_c == 2 && AFIX == 128u);
	const bool alpha_c2_eq_less_one = (m_conf.ps.blend_c == 2 && AFIX <= 128u);
	const bool alpha_c2_high_one = (m_conf.ps.blend_c == 2 && AFIX > 128u);
	const bool alpha_eq_one = alpha_c0_eq_one || alpha_c2_eq_one;
	const bool alpha_high_one = alpha_c0_high_min_one || alpha_c2_high_one;
	const bool alpha_eq_less_one = alpha_c0_eq_less_max_one || alpha_c2_eq_less_one;

	// Optimize blending equations, must be done before index calculation
	if ((m_conf.ps.blend_a == m_conf.ps.blend_b) || ((m_conf.ps.blend_b == m_conf.ps.blend_d) && alpha_eq_one))
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
	else if (alpha_c0_eq_zero || alpha_c2_eq_zero)
	{
		// C == 0.0f
		// (A - B) * C, result will be 0.0f so set A B to Cs
		m_conf.ps.blend_a = 0;
		m_conf.ps.blend_b = 0;
	}
	else if (COLCLAMP.CLAMP && m_conf.ps.blend_a == 2
		&& (m_conf.ps.blend_d == 2 || (m_conf.ps.blend_b == m_conf.ps.blend_d && (alpha_high_one || alpha_c1_high_min_one))))
	{
		// CLAMP 1, negative result will be clamped to 0.
		// Condition 1:
		// (0  - Cs)*Alpha +  0, (0  - Cd)*Alpha +  0
		// Condition 2:
		// Alpha is either As or F higher than 1.0f
		// (0  - Cd)*Alpha  + Cd, (0  - Cs)*F  + Cs
		// Results will be 0.0f, make sure D is set to 2.
		m_conf.ps.blend_a = 0;
		m_conf.ps.blend_b = 0;
		m_conf.ps.blend_c = 0;
		m_conf.ps.blend_d = 2;
	}

	// TODO: blend_ad_alpha_masked, as well as other blend cases can be optimized on dx11/dx12/gl to use
	// blend multipass more which might be faster, vk likely won't benefit as barriers are already fast.

	// Ad cases, alpha write is masked, one barrier is enough, for d3d11 read the fb
	// Replace Ad with As, blend flags will be used from As since we are chaging the blend_index value.
	// Must be done before index calculation, after blending equation optimizations
	const bool blend_ad = m_conf.ps.blend_c == 1;
	bool blend_ad_alpha_masked = blend_ad && !m_conf.colormask.wa;
	const bool is_basic_blend = GSConfig.AccurateBlendingUnit != AccBlendLevel::Minimum;
	if (blend_ad_alpha_masked && ((is_basic_blend || (COLCLAMP.CLAMP == 0) || m_conf.require_one_barrier)))
	{
		// Swap Ad with As for hw blend.
		m_conf.ps.a_masked = 1;
		m_conf.ps.blend_c = 0;
		m_conf.require_one_barrier |= true;
	}
	else
		blend_ad_alpha_masked = false;

	const u8 blend_index = static_cast<u8>(((m_conf.ps.blend_a * 3 + m_conf.ps.blend_b) * 3 + m_conf.ps.blend_c) * 3 + m_conf.ps.blend_d);
	HWBlend blend = GSDevice::GetBlend(blend_index);
	const int blend_flag = blend.flags;

	// Re set alpha, it was modified, must be done after index calculation
	if (blend_ad_alpha_masked)
		m_conf.ps.blend_c = ALPHA.C;

	// HW blend can handle Cd output.
	bool color_dest_blend = !!(blend_flag & BLEND_CD);

	// Per pixel alpha blending.
	const bool PABE = m_draw_env->PABE.PABE && GetAlphaMinMax().min < 128;

	// HW blend can handle it, no need for sw or hw colclip, Cd*Alpha or Cd*(1 - Alpha) where Alpha <= 128.
	bool color_dest_blend2 = !PABE && ((m_conf.ps.blend_a == 1 && m_conf.ps.blend_b == 2 && m_conf.ps.blend_d == 2) || (m_conf.ps.blend_a == 2 && m_conf.ps.blend_b == 1 && m_conf.ps.blend_d == 1)) &&
		(alpha_eq_less_one || (alpha_c1_eq_less_max_one && new_rt_alpha_scale));
	// HW blend can handle it, no need for sw or hw colclip, Cs*Alpha + Cd*(1 - Alpha) or Cd*Alpha + Cs*(1 - Alpha) where Alpha <= 128.
	bool blend_zero_to_one_range = !PABE && ((m_conf.ps.blend_a == 0 && m_conf.ps.blend_b == 1 && m_conf.ps.blend_d == 1) || (blend_flag & BLEND_MIX3)) &&
		(alpha_eq_less_one || (alpha_c1_eq_less_max_one && new_rt_alpha_scale));

	// Do the multiplication in shader for blending accumulation: Cs*As + Cd or Cs*Af + Cd
	bool accumulation_blend = !!(blend_flag & BLEND_ACCU);
	// If alpha == 1.0, almost everything is an accumulation blend!
	// Ones that use (1 + Alpha) can't guarante the mixed sw+hw blending this enables will give an identical result to sw due to clamping
	// But enable for everything else that involves dst color
	if (alpha_eq_one && (m_conf.ps.blend_a != m_conf.ps.blend_d) && blend.dst != GSDevice::CONST_ZERO)
		accumulation_blend = true;

	// Blending doesn't require barrier, or sampling of the rt
	const bool blend_non_recursive = !!(blend_flag & BLEND_NO_REC);

	// BLEND MIX selection, use a mix of hw/sw blending
	const bool blend_mix1 = !!(blend_flag & BLEND_MIX1) && !(m_conf.ps.blend_b == m_conf.ps.blend_d && alpha_high_one);
	const bool blend_mix2 = !!(blend_flag & BLEND_MIX2);
	const bool blend_mix3 = !!(blend_flag & BLEND_MIX3);
	bool blend_mix = (blend_mix1 || blend_mix2 || blend_mix3) && COLCLAMP.CLAMP;

	// Primitives don't overlap.
	const bool no_prim_overlap = (m_prim_overlap == PRIM_OVERLAP_NO);

	// HW blend can be done in multiple passes when there's no overlap.
	// Blend multi pass is only useful when texture barriers aren't supported.
	// Speed wise Texture barriers > blend multi pass > texture copies.
	const bool blend_multi_pass_support = !features.texture_barrier && no_prim_overlap && is_basic_blend && COLCLAMP.CLAMP;
	const bool bmix1_multi_pass1 = blend_multi_pass_support && blend_mix1 && (alpha_c0_high_max_one || alpha_c2_high_one) && m_conf.ps.blend_d == 2;
	const bool bmix1_multi_pass2 = blend_multi_pass_support && (blend_flag & BLEND_MIX1) && m_conf.ps.blend_b == m_conf.ps.blend_d && !m_conf.ps.dither && alpha_high_one;
	const bool bmix3_multi_pass = blend_multi_pass_support && blend_mix3 && !m_conf.ps.dither && alpha_high_one;
	// We don't want to enable blend mix if we are doing a multi pass, it's useless.
	blend_mix &= !(bmix1_multi_pass1 || bmix1_multi_pass2 || bmix3_multi_pass);

	const bool one_barrier = m_conf.require_one_barrier || blend_ad_alpha_masked;
	// Condition 1: Require full sw blend for full barrier.
	// Condition 2: One barrier is already enabled, prims don't overlap or is a channel shuffle so let's use sw blend instead.
	// Condition 3: A texture shuffle is unlikely to overlap, so we can prefer full sw blend.
	// Condition 4: If it's tex in fb draw and there's no overlap prefer sw blend, fb is already being read.
	const bool prefer_sw_blend = ((features.texture_barrier || features.multidraw_fb_copy) && m_conf.require_full_barrier) || (m_conf.require_one_barrier && (no_prim_overlap || m_channel_shuffle)) || m_conf.ps.shuffle || (no_prim_overlap && (m_conf.tex == m_conf.rt));
	const bool free_blend = blend_non_recursive // Free sw blending, doesn't require barriers or reading fb
	                        || accumulation_blend; // Mix of hw/sw blending

	// Warning no break on purpose
	// Note: the [[fallthrough]] attribute tell compilers not to complain about not having breaks.
	bool sw_blending = false;
	// Try to lower sw blend on dx11, try to use blend multipass if possible on basic blend.
	const bool blend_multipass_group = blend_multi_pass_support && !features.texture_barrier &&
		(bmix1_multi_pass1 || bmix1_multi_pass2 || bmix3_multi_pass || (blend_flag & (BLEND_HW3 | BLEND_HW4 | BLEND_HW5 | BLEND_HW6 | BLEND_HW7 | BLEND_HW8 | BLEND_HW9)));

	const bool barriers_supported = features.texture_barrier || features.multidraw_fb_copy;
	const bool blend_requires_barrier =
		// We don't want the cases to be enabled if barriers aren't supported so limit it to no overlap.
		(no_prim_overlap || barriers_supported)
		// Impossible blending.
		&& ((blend_flag & BLEND_A_MAX)
		// Blend can be done in a single draw, and we already need a barrier.
		// On fbfetch, one barrier is like full barrier.
		|| (one_barrier && (no_prim_overlap || features.framebuffer_fetch))
		// Blending with alpha > 1 will be wrong, except BLEND_HW2.
		|| (!(blend_flag & BLEND_HW2) && !blend_multipass_group && (alpha_c2_high_one || alpha_c0_high_max_one) && no_prim_overlap)
		// Ad blends are completely wrong without sw blend (Ad is 0.5 not 1 for 128). We can spare a barrier for it.
		|| (blend_ad && !blend_multipass_group && no_prim_overlap && !new_rt_alpha_scale));

	switch (GSConfig.AccurateBlendingUnit)
	{
		case AccBlendLevel::Maximum:
			sw_blending |= true;
			[[fallthrough]];
		case AccBlendLevel::Full:
			sw_blending |= m_conf.ps.blend_a != m_conf.ps.blend_b && alpha_c0_high_max_one;
			[[fallthrough]];
		case AccBlendLevel::High:
			sw_blending |= (alpha_c1_high_max_one || alpha_c1_high_no_rta_correct) || (m_conf.ps.blend_a != m_conf.ps.blend_b && alpha_c2_high_one);
			[[fallthrough]];
		case AccBlendLevel::Medium:
			// Initial idea was to enable accurate blending for sprite rendering to handle
			// correctly post-processing effect. Some games (ZoE) use tons of sprites as particles.
			// In order to keep it fast, let's limit it to smaller draw call.
			sw_blending |= barriers_supported && m_vt.m_primclass == GS_SPRITE_CLASS && ComputeDrawlistGetSize(rt->m_scale) < 100;
			// We don't want the cases to be enabled if barriers aren't supported so limit it to no overlap.
			sw_blending &= (no_prim_overlap || barriers_supported);
			[[fallthrough]];
		case AccBlendLevel::Basic:
		default:
			// Prefer sw blend if possible.
			color_dest_blend &= !(m_channel_shuffle || m_conf.ps.dither);
			color_dest_blend2 &= !(prefer_sw_blend || m_conf.ps.dither);
			blend_zero_to_one_range &= !(prefer_sw_blend || m_conf.ps.dither);
			accumulation_blend &= !prefer_sw_blend;
			// Enable sw blending for barriers.
			sw_blending |= blend_requires_barrier || prefer_sw_blend;
			// Enable sw blending for free blending.
			sw_blending |= free_blend;
			// Do not run BLEND MIX if sw blending is already present, it's less accurate.
			blend_mix &= !sw_blending;
			sw_blending |= blend_mix;
			[[fallthrough]];
		case AccBlendLevel::Minimum:
			break;
	}

	if (features.framebuffer_fetch)
	{
		// If we have fbfetch, use software blending when we need the fb value for anything else.
		// This saves outputting the second color when it's not needed.
		if (one_barrier || m_conf.require_full_barrier)
		{
			sw_blending = true;
			color_dest_blend = false;
			accumulation_blend = false;
			blend_mix = false;
		}
	}

	// Color clip
	if (COLCLAMP.CLAMP == 0)
	{
		bool has_colclip_texture = g_gs_device->GetColorClipTexture() != nullptr;

		// Don't know any game that resizes the RT mid colclip, but gotta be careful.
		if (has_colclip_texture)
		{
			GSTexture* colclip_texture = g_gs_device->GetColorClipTexture();

			if (colclip_texture->GetSize() != rt->m_texture->GetSize())
			{
				GL_CACHE("HW: Pre-Blend resolve of colclip due to size change! Address: %x", rt->m_TEX0.TBP0);
				g_gs_device->StretchRect(colclip_texture, GSVector4(m_conf.colclip_update_area) / GSVector4(GSVector4i(colclip_texture->GetSize()).xyxy()), rt->m_texture, GSVector4(m_conf.colclip_update_area),
					ShaderConvert::COLCLIP_RESOLVE, false);

				g_gs_device->Recycle(colclip_texture);

				g_gs_device->SetColorClipTexture(nullptr);

				has_colclip_texture = false;
			}
		}

		const bool free_colclip = !has_colclip_texture && (features.framebuffer_fetch || no_prim_overlap || blend_non_recursive);
		if (color_dest_blend || color_dest_blend2 || blend_zero_to_one_range)
		{
			// No overflow, disable colclip.
			GL_INS("HW: COLCLIP mode DISABLED");
			sw_blending = false;
			m_conf.colclip_mode = (has_colclip_texture && !NextDrawColClip()) ? GSHWDrawConfig::ColClipMode::ResolveOnly : GSHWDrawConfig::ColClipMode::NoModify;
		}
		else if (free_colclip)
		{
			// The fastest algo that requires a single pass
			GL_INS("HW: COLCLIP Free mode ENABLED");
			m_conf.ps.colclip  = 1;
			sw_blending        = true;
			// Disable the colclip hw algo
			accumulation_blend = false;
			blend_mix          = false;
			m_conf.colclip_mode = (has_colclip_texture && !NextDrawColClip()) ? GSHWDrawConfig::ColClipMode::ResolveOnly : GSHWDrawConfig::ColClipMode::NoModify;
		}
		else if (accumulation_blend)
		{
			// A fast algo that requires 2 passes
			GL_INS("HW: COLCLIP ACCU HW mode ENABLED");
			m_conf.ps.colclip_hw = 1;
			sw_blending = true; // Enable sw blending for the colclip algo

			m_conf.colclip_mode = has_colclip_texture ? (NextDrawColClip() ? GSHWDrawConfig::ColClipMode::NoModify : GSHWDrawConfig::ColClipMode::ResolveOnly) : (NextDrawColClip() ? GSHWDrawConfig::ColClipMode::ConvertOnly : GSHWDrawConfig::ColClipMode::ConvertAndResolve);
		}
		else if (sw_blending)
		{
			// A slow algo that could requires several passes (barely used)
			GL_INS("HW: COLCLIP SW mode ENABLED");
			m_conf.ps.colclip = 1;
			m_conf.colclip_mode = (has_colclip_texture && !NextDrawColClip()) ? GSHWDrawConfig::ColClipMode::ResolveOnly : GSHWDrawConfig::ColClipMode::NoModify;
		}
		else
		{
			GL_INS("HW: COLCLIP HW mode ENABLED");
			m_conf.ps.colclip_hw = 1;
			m_conf.colclip_mode = has_colclip_texture ? (NextDrawColClip() ? GSHWDrawConfig::ColClipMode::NoModify : GSHWDrawConfig::ColClipMode::ResolveOnly) : (NextDrawColClip() ? GSHWDrawConfig::ColClipMode::ConvertOnly : GSHWDrawConfig::ColClipMode::ConvertAndResolve);
		}

		m_conf.colclip_frame = m_cached_ctx.FRAME;
	}

	// Per pixel alpha blending
	if (PABE)
	{
		// Breath of Fire Dragon Quarter, Strawberry Shortcake, Super Robot Wars, Cartoon Network Racing, Simple 2000 Series Vol.81, SOTC.

		// TODO: We can expand pabe hw to Cd*(1 - Alpha) where alpha is As or Af and replace the formula with Cs + 0 when As < 128,
		// but need to find test cases where it makes a difference,
		// C 12 Final Resistance triggers it but there's no difference and it's a psx game.
		if (sw_blending)
		{
			if (accumulation_blend && (blend.op != GSDevice::OP_REV_SUBTRACT))
			{
				// PABE accumulation blend:
				// Idea is to achieve final output Cs when As < 1, we do this with manipulating Cd using the src1 output.
				// This can't be done with reverse subtraction as we want Cd to be 0 when As < 1.
				// TODO: Blend mix is excluded as no games were found, otherwise it can be added.

				m_conf.ps.pabe = 1;
			}
			else if (features.texture_barrier || features.multidraw_fb_copy)
			{
				// PABE sw blend:
				// Disable hw/sw mix and do pure sw blend with reading the framebuffer.
				color_dest_blend   = false;
				accumulation_blend = false;
				blend_mix          = false;
				m_conf.ps.pabe     = 1;

				// hw colclip mode should be disabled when doing sw blend, swap with sw colclip.
				if (m_conf.ps.colclip_hw)
				{
					const bool has_colclip_texture = g_gs_device->GetColorClipTexture() != nullptr;
					m_conf.ps.colclip_hw = 0;
					m_conf.ps.colclip = 1;
					m_conf.colclip_mode = has_colclip_texture ? GSHWDrawConfig::ColClipMode::EarlyResolve : GSHWDrawConfig::ColClipMode::NoModify;
				}
			}
			else
			{
				// PABE sw blend:
				m_conf.ps.pabe = !(accumulation_blend || blend_mix);
			}

			GL_INS("HW: PABE mode %s", m_conf.ps.pabe ? "ENABLED" : "DISABLED");
		}
	}

	if (color_dest_blend)
	{
		// Blend output will be Cd, disable hw/sw blending.
		m_conf.blend = {};
		m_conf.ps.no_color1 = true;
		m_conf.ps.blend_a = m_conf.ps.blend_b = m_conf.ps.blend_c = m_conf.ps.blend_d = 0;
		sw_blending = false; // DATE_PRIMID

		// Output is Cd, set rgb write to 0.
		m_conf.colormask.wrgba &= 0x8;

		// TODO: Find games that may benefit from adding full coverage on RTA Scale when we're overwriting the whole target,
		// then the rest of then conditions can be added.
		if (can_scale_rt_alpha && !new_rt_alpha_scale && m_conf.colormask.wa)
		{
			const bool afail_fb_only = m_cached_ctx.TEST.AFAIL == AFAIL_FB_ONLY;
			const bool full_cover = rt->m_valid.rintersect(m_r).eq(rt->m_valid) && m_primitive_covers_without_gaps == NoGapsType::FullCover && !(DATE || !afail_fb_only || !IsDepthAlwaysPassing());

			// Restrict this to only when we're overwriting the whole target.
			new_rt_alpha_scale = full_cover;
		}

		return;
	}
	else if (sw_blending)
	{
		// Require the fix alpha vlaue
		if (m_conf.ps.blend_c == 2)
			m_conf.cb_ps.TA_MaxDepth_Af.a = static_cast<float>(AFIX) / 128.0f;

		if (accumulation_blend)
		{
			// Keep HW blending to do the addition/subtraction
			m_conf.blend = {true, GSDevice::CONST_ONE, GSDevice::CONST_ONE, blend.op, GSDevice::CONST_ONE, GSDevice::CONST_ZERO, false, 0};

			// Remove Cd from sw blend, it's handled in hw
			if (m_conf.ps.blend_a == 1)
				m_conf.ps.blend_a = 2;
			if (m_conf.ps.blend_b == 1)
				m_conf.ps.blend_b = 2;
			if (m_conf.ps.blend_d == 1)
				m_conf.ps.blend_d = 2;

			if (m_conf.ps.blend_a == 2)
			{
				// Accumulation blend is only available in (Cs - 0)*Something + Cd, or with alpha == 1
				pxAssert(m_conf.ps.blend_d == 2 || alpha_eq_one);
				// A bit of normalization
				m_conf.ps.blend_a = m_conf.ps.blend_d;
				m_conf.ps.blend_d = 2;
			}

			if (blend.op == GSDevice::OP_REV_SUBTRACT)
			{
				pxAssert(m_conf.ps.blend_a == 2);
				if (m_conf.ps.colclip_hw)
				{
					// HW colclip uses unorm, which is always positive
					// Have the shader do the inversion, then clip to remove the negative
					m_conf.blend.op = GSDevice::OP_ADD;
				}
				else
				{
					// The blend unit does a reverse subtraction so it means
					// the shader must output a positive value.
					// Replace 0 - Cs by Cs - 0
					m_conf.ps.blend_a = m_conf.ps.blend_b;
					m_conf.ps.blend_b = 2;
				}
			}
			else if (m_conf.ps.pabe)
			{
				m_conf.blend.dst_factor = GSDevice::SRC1_COLOR;
			}

			// Dual source output not needed (accumulation blend replaces it with ONE).
			m_conf.ps.no_color1 = (m_conf.ps.pabe == 0);
		}
		else if (blend_mix)
		{
			// Disable dithering on blend mix if needed.
			if (m_conf.ps.dither)
			{
				// TODO: Either exclude BMIX1_ALPHA_HIGH_ONE case or allow alpha > 1.0 on dither adjust, case is currently disabled.
				const bool can_dither = (m_conf.ps.blend_a == 0 && m_conf.ps.blend_b == 1) || (m_conf.ps.blend_a == 1 && m_conf.ps.blend_b == 0);
				m_conf.ps.dither = can_dither;
				m_conf.ps.dither_adjust = can_dither;
			}

			if (blend_mix1)
			{
				if (m_conf.ps.blend_b == m_conf.ps.blend_d && (alpha_c0_high_min_one || alpha_c1_high_min_one || alpha_c2_high_one))
				{
					// Alpha is guaranteed to be > 128.
					// Replace Cs*Alpha + Cd*(1 - Alpha) with Cs*Alpha - Cd*(Alpha - 1).
					blend.dst = GSDevice::SRC1_COLOR;
					blend.op = GSDevice::OP_SUBTRACT;
					m_conf.ps.blend_hw = static_cast<u8>(HWBlendType::BMIX1_ALPHA_HIGH_ONE);
				}
				else if (m_conf.ps.blend_a == m_conf.ps.blend_d)
				{
					// Cd*(Alpha + 1) - Cs*Alpha will always be wrong.
					// Let's cheat a little and divide blended Cs by Alpha.
					// Result will still be wrong but closer to what we want.
					m_conf.ps.blend_hw = static_cast<u8>(HWBlendType::BMIX1_SRC_HALF);
				}

				m_conf.ps.blend_a = 0;
				m_conf.ps.blend_b = 2;
				m_conf.ps.blend_d = 2;
			}
			else if (blend_mix2)
			{
				// Allow to compensate when Cs*(Alpha + 1) overflows,
				// to compensate we change the alpha output value for Cd*Alpha.
				blend.dst = GSDevice::SRC1_COLOR;
				m_conf.ps.blend_hw = static_cast<u8>(HWBlendType::BMIX2_OVERFLOW);

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

			// Elide DSB colour output if not used by dest.
			m_conf.ps.no_color1 = !GSDevice::IsDualSourceBlendFactor(blend.dst);

			// For mixed blend, the source blend is done in the shader (so we use CONST_ONE as a factor).
			m_conf.blend = {true, GSDevice::CONST_ONE, blend.dst, blend.op, GSDevice::CONST_ONE, GSDevice::CONST_ZERO, m_conf.ps.blend_c == 2, AFIX};
			m_conf.ps.blend_mix = (blend.op == GSDevice::OP_REV_SUBTRACT) ? 2 : 1;
		}
		else
		{
			// Disable HW blending
			m_conf.blend = {};
			m_conf.ps.no_color1 = true;

			// No need to set a_masked bit for blend_ad_alpha_masked case
			const bool blend_non_recursive_one_barrier = blend_non_recursive && blend_ad_alpha_masked;
			if (blend_non_recursive_one_barrier)
				m_conf.require_one_barrier |= true;
			else if (features.texture_barrier || features.multidraw_fb_copy)
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

		const bool rta_correction = can_scale_rt_alpha && !blend_ad_alpha_masked && m_conf.ps.blend_c == 1 && !(blend_flag & BLEND_A_MAX);
		if (rta_correction)
		{
			const bool afail_always_fb_alpha = m_cached_ctx.TEST.AFAIL == AFAIL_FB_ONLY || (m_cached_ctx.TEST.AFAIL == AFAIL_RGB_ONLY && GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].trbpp != 32);
			const bool always_passing_alpha = !m_cached_ctx.TEST.ATE || afail_always_fb_alpha || (m_cached_ctx.TEST.ATE && m_cached_ctx.TEST.ATST == ATST_ALWAYS);
			const bool full_cover = rt->m_valid.rintersect(m_r).eq(rt->m_valid) && m_primitive_covers_without_gaps == NoGapsType::FullCover && !(DATE_PRIMID || DATE_BARRIER || !always_passing_alpha || !IsDepthAlwaysPassing());

			if (!full_cover)
			{
				rt->ScaleRTAlpha();
				m_conf.rt = rt->m_texture;
			}

			new_rt_alpha_scale = true;
			alpha_c1_high_no_rta_correct = false;

			m_conf.ps.rta_correction = rt->m_rt_alpha_scale;
		}

		if (blend_multi_pass_support)
		{
			const HWBlend blend_multi_pass = GSDevice::GetBlend(blend_index);
			if (bmix1_multi_pass1)
			{
				// Alpha = As or Af.
				// Cs*Alpha - Cd*Alpha, Cd*Alpha - Cs*Alpha.
				// Render pass 1: Do (Cd - Cs) or (Cs - Cd) on first pass.
				blend.src = GSDevice::CONST_ONE;
				blend.dst = GSDevice::CONST_ONE;
				// Render pass 2: Blend the result (Cd) from render pass 1 with alpha range of 0-2.
				m_conf.blend_multi_pass.enable = true;
				m_conf.blend_multi_pass.blend_hw = static_cast<u8>(HWBlendType::SRC_ALPHA_DST_FACTOR);
				m_conf.blend_multi_pass.blend = {true, GSDevice::DST_COLOR, (m_conf.ps.blend_c == 2) ? GSDevice::CONST_COLOR : GSDevice::SRC1_COLOR, GSDevice::OP_ADD, GSDevice::CONST_ONE, GSDevice::CONST_ZERO, m_conf.ps.blend_c == 2, AFIX};
			}
			else if (bmix1_multi_pass2)
			{
				// Alpha = As or Af.
				// Cs*Alpha + Cd*(1 - Alpha).
				// Render pass 1: Do the blend but halve the alpha, subtract instead of add since alpha is higher than 1.
				m_conf.ps.blend_hw = static_cast<u8>(HWBlendType::SRC_INV_DST_BLEND_HALF);
				blend.src = GSDevice::CONST_ONE;
				blend.dst = GSDevice::SRC1_COLOR;
				blend.op = GSDevice::OP_SUBTRACT;
				// Render pass 2: Take result (Cd) from render pass 1 and double it.
				m_conf.blend_multi_pass.enable = true;
				m_conf.blend_multi_pass.blend_hw = static_cast<u8>(HWBlendType::SRC_ONE_DST_FACTOR);
				m_conf.blend_multi_pass.blend = {true, GSDevice::DST_COLOR, GSDevice::CONST_ONE, blend_multi_pass.op, GSDevice::CONST_ONE, GSDevice::CONST_ZERO, false, 0};
			}
			else if (bmix3_multi_pass)
			{
				// Alpha = As or Af.
				// Cd*Alpha + Cs*(1 - Alpha).
				// Render pass 1: Do the blend but halve the alpha, subtract instead of add since alpha is higher than 1.
				m_conf.ps.blend_hw = static_cast<u8>(HWBlendType::INV_SRC_DST_BLEND_HALF);
				blend.src = GSDevice::CONST_ONE;
				blend.dst = GSDevice::SRC1_COLOR;
				blend.op = GSDevice::OP_REV_SUBTRACT;
				// Render pass 2: Take result (Cd) from render pass 1 and double it.
				m_conf.blend_multi_pass.enable = true;
				m_conf.blend_multi_pass.blend_hw = static_cast<u8>(HWBlendType::SRC_ONE_DST_FACTOR);
				m_conf.blend_multi_pass.blend = {true, GSDevice::DST_COLOR, GSDevice::CONST_ONE, blend_multi_pass.op, GSDevice::CONST_ONE, GSDevice::CONST_ZERO, false, 0};
			}
			else if ((alpha_c0_high_max_one || alpha_c1_high_no_rta_correct || alpha_c2_high_one) && (blend_flag & BLEND_HW1))
			{
				// Alpha = As, Ad or Af.
				// Cd*(1 + Alpha).
				// Render pass 1: Do Cd*(1 + Alpha) with a half result in the end.
				m_conf.ps.blend_hw = static_cast<u8>(HWBlendType::SRC_HALF_ONE_DST_FACTOR);
				blend.dst = (m_conf.ps.blend_c == 1) ? GSDevice::DST_ALPHA : GSDevice::SRC1_COLOR;
				// Render pass 2: Take result (Cd) from render pass 1 and double it.
				m_conf.blend_multi_pass.enable = true;
				m_conf.blend_multi_pass.blend_hw = static_cast<u8>(HWBlendType::SRC_ONE_DST_FACTOR);
				m_conf.blend_multi_pass.blend = {true, blend_multi_pass.src, GSDevice::CONST_ONE, blend_multi_pass.op, GSDevice::CONST_ONE, GSDevice::CONST_ZERO, false, 0};
			}
			else if (alpha_c1_high_no_rta_correct && (blend_flag & BLEND_HW3))
			{
				// Alpha = Ad.
				// Cs*Alpha, Cs*Alpha + Cd, Cd - Cs*Alpha.
				// Render pass 1: Do Cs*Alpha, Cs*Alpha + Cd or Cd - Cs*Alpha on first pass.
				// Render pass 2: Take result (Cd) from render pass 1 and either add or rev subtract Cs*Alpha based on the blend operation.
				m_conf.blend_multi_pass.enable = true;
				m_conf.blend_multi_pass.blend = {true, blend_multi_pass.src, GSDevice::CONST_ONE, blend_multi_pass.op, GSDevice::CONST_ONE, GSDevice::CONST_ZERO, false, 0};
			}
			else if ((alpha_c0_high_max_one || alpha_c2_high_one) && (blend_flag & BLEND_HW4))
			{
				// Alpha = As or Af.
				// Cs + Cd*Alpha, Cs - Cd*Alpha.
				const u8 dither = m_conf.ps.dither;
				// Render pass 1: Calculate Cd*Alpha with an alpha range of 0-2.
				m_conf.ps.blend_hw = static_cast<u8>(HWBlendType::SRC_ALPHA_DST_FACTOR);
				m_conf.ps.dither = 0;
				blend.src = GSDevice::DST_COLOR;
				blend.dst = (m_conf.ps.blend_c == 2) ? GSDevice::CONST_COLOR : GSDevice::SRC1_COLOR;
				blend.op = GSDevice::OP_ADD;
				// Render pass 2: Add or subtract result of render pass 1(Cd) from Cs.
				m_conf.blend_multi_pass.enable = true;
				m_conf.blend_multi_pass.dither = dither * GSConfig.Dithering;
				m_conf.blend_multi_pass.blend = {true, blend_multi_pass.src, GSDevice::CONST_ONE, blend_multi_pass.op, GSDevice::CONST_ONE, GSDevice::CONST_ZERO, false, 0};
			}
			else if (alpha_c1_high_no_rta_correct && (blend_flag & BLEND_HW5))
			{
				// Alpha = Ad.
				// Cd*Alpha - Cs*Alpha, Cs*Alpha - Cd*Alpha.
				// Render pass 1: Do (Cd - Cs)*Alpha, (Cs - Cd)*Alpha or Cd*Alpha on first pass.
				// Render pass 2: Take result (Cd) from render pass 1 and double it.
				m_conf.blend_multi_pass.enable = true;
				m_conf.blend_multi_pass.blend_hw = static_cast<u8>(HWBlendType::SRC_ONE_DST_FACTOR);
				m_conf.blend_multi_pass.blend = {true, GSDevice::DST_COLOR, GSDevice::CONST_ONE, GSDevice::OP_ADD, GSDevice::CONST_ONE, GSDevice::CONST_ZERO, false, 0};
			}
			else if (alpha_c1_high_no_rta_correct && (blend_flag & BLEND_HW6))
			{
				// Alpha = Ad.
				// Cs + Cd*Alpha, Cs - Cd*Alpha.
				// Render pass 1: Multiply Cs by 0.5, then do Cs + Cd*Alpha or Cs - Cd*Alpha.
				m_conf.ps.blend_c = 2;
				AFIX = 64;
				blend.src = GSDevice::CONST_COLOR;
				// Render pass 2: Take result (Cd) from render pass 1 and double it.
				m_conf.blend_multi_pass.enable = true;
				m_conf.blend_multi_pass.blend_hw = static_cast<u8>(HWBlendType::SRC_ONE_DST_FACTOR);
				m_conf.blend_multi_pass.blend = {true, GSDevice::DST_COLOR, GSDevice::CONST_ONE, GSDevice::OP_ADD, GSDevice::CONST_ONE, GSDevice::CONST_ZERO, false, 0};
			}
			else if (alpha_c1_high_no_rta_correct && (blend_flag & BLEND_HW7))
			{
				// Alpha = Ad.
				// Cd*(1 - Alpha).
				// Render pass 1: Multiply Cd by 0.5, then do Cd - Cd*Alpha.
				m_conf.ps.blend_hw = static_cast<u8>(HWBlendType::SRC_HALF_ONE_DST_FACTOR);
				blend.src = GSDevice::DST_COLOR;
				blend.dst = GSDevice::DST_ALPHA;
				blend.op = GSDevice::OP_SUBTRACT;
				// Render pass 2: Take result (Cd) from render pass 1 and double it.
				m_conf.blend_multi_pass.enable = true;
				m_conf.blend_multi_pass.blend_hw = static_cast<u8>(HWBlendType::SRC_ONE_DST_FACTOR);
				m_conf.blend_multi_pass.blend = {true, GSDevice::DST_COLOR, GSDevice::CONST_ONE, GSDevice::OP_ADD, GSDevice::CONST_ONE, GSDevice::CONST_ZERO, false, 0};
			}
			else if (blend_flag & BLEND_HW8)
			{
				// Alpha = Ad.
				// Cs*(1 + Alpha).
				// Render pass 1: Do Cs.
				// Render pass 2: Try to double Cs, then take result (Cd) from render pass 1 and add Cs*Alpha to it.
				m_conf.blend_multi_pass.enable = true;
				m_conf.blend_multi_pass.blend_hw = static_cast<u8>(HWBlendType::SRC_DOUBLE);
				m_conf.blend_multi_pass.blend = {true, GSDevice::DST_ALPHA, GSDevice::CONST_ONE, blend_multi_pass.op, GSDevice::CONST_ONE, GSDevice::CONST_ZERO, false, 0};
			}
			else if (alpha_c1_high_no_rta_correct && (blend_flag & BLEND_HW9))
			{
				// Alpha = Ad.
				// Cs*(1 - Alpha).
				// Render pass 1: Do Cs*(1 - Alpha).
				// Render pass 2: Take result (Cd) from render pass 1 and subtract Cs*Alpha from it.
				m_conf.blend_multi_pass.enable = true;
				m_conf.blend_multi_pass.blend = {true, GSDevice::DST_ALPHA, GSDevice::CONST_ONE, GSDevice::OP_REV_SUBTRACT, GSDevice::CONST_ONE, GSDevice::CONST_ZERO, false, 0};
			}

			// Remove second color output when unused. Works around bugs in some drivers (e.g. Intel).
			m_conf.blend_multi_pass.no_color1 = !m_conf.blend_multi_pass.enable ||
			                                    (!GSDevice::IsDualSourceBlendFactor(m_conf.blend_multi_pass.blend.src_factor) &&
			                                     !GSDevice::IsDualSourceBlendFactor(m_conf.blend_multi_pass.blend.dst_factor));
		}

		if (!m_conf.blend_multi_pass.enable && blend_flag & BLEND_HW1)
		{
			m_conf.ps.blend_hw = static_cast<u8>(HWBlendType::SRC_ONE_DST_FACTOR);
		}
		else if (blend_flag & BLEND_HW2)
		{
			m_conf.ps.blend_hw = static_cast<u8>(HWBlendType::SRC_ALPHA_DST_FACTOR);
		}
		else if (!m_conf.blend_multi_pass.enable && alpha_c1_high_no_rta_correct && (blend_flag & BLEND_HW3))
		{
			m_conf.ps.blend_hw = static_cast<u8>(HWBlendType::SRC_DOUBLE);
		}

		if (m_conf.ps.blend_c == 2 && (m_conf.ps.blend_hw == static_cast<u8>(HWBlendType::SRC_ALPHA_DST_FACTOR)
			|| m_conf.ps.blend_hw == static_cast<u8>(HWBlendType::SRC_HALF_ONE_DST_FACTOR)
			|| m_conf.ps.blend_hw == static_cast<u8>(HWBlendType::SRC_INV_DST_BLEND_HALF)
			|| m_conf.ps.blend_hw == static_cast<u8>(HWBlendType::INV_SRC_DST_BLEND_HALF)
			|| m_conf.blend_multi_pass.blend_hw == static_cast<u8>(HWBlendType::SRC_ALPHA_DST_FACTOR)))
		{
			m_conf.cb_ps.TA_MaxDepth_Af.a = static_cast<float>(AFIX) / 128.0f;
		}

		const GSDevice::BlendFactor src_factor_alpha = m_conf.blend_multi_pass.enable ? GSDevice::CONST_ZERO : GSDevice::CONST_ONE;
		const GSDevice::BlendFactor dst_factor_alpha = m_conf.blend_multi_pass.enable ? GSDevice::CONST_ONE : GSDevice::CONST_ZERO;
		m_conf.blend = {true, blend.src, blend.dst, blend.op, src_factor_alpha, dst_factor_alpha, m_conf.ps.blend_c == 2, AFIX};

		// Remove second color output when unused. Works around bugs in some drivers (e.g. Intel).
		m_conf.ps.no_color1 = !GSDevice::IsDualSourceBlendFactor(m_conf.blend.src_factor) &&
		                      !GSDevice::IsDualSourceBlendFactor(m_conf.blend.dst_factor);
	}

	// Notify the shader that it needs to invert rounding
	if (m_conf.blend.op == GSDevice::OP_REV_SUBTRACT)
		m_conf.ps.round_inv = 1;

	// DATE_PRIMID interact very badly with sw blending. DATE_PRIMID uses the primitiveID to find the primitive
	// that write the bad alpha value. Sw blending will force the draw to run primitive by primitive
	// (therefore primitiveID will be constant to 1).
	// Switch DATE_PRIMID with DATE_BARRIER in such cases to ensure accuracy.
	// No mix of COLCLIP + sw blend + DATE_PRIMID, neither sw fbmask + DATE_PRIMID.
	// Note: Do the swap in the end, saves the expensive draw splitting/barriers when mixed software blending is used.
	if (sw_blending && DATE_PRIMID && m_conf.require_full_barrier &&
		(features.texture_barrier || (features.multidraw_fb_copy && !no_prim_overlap)))
	{
		GL_PERF("DATE: Swap DATE_PRIMID with DATE_BARRIER");
		DATE_PRIMID = false;
		DATE_BARRIER = true;
	}
}

__ri static constexpr bool IsRedundantClamp(u8 clamp, u32 clamp_min, u32 clamp_max, u32 tsize)
{
	// Don't shader sample when the clamp/repeat is configured to the texture size.
	const u32 textent = (1u << tsize) - 1u;
	if (clamp == CLAMP_REGION_CLAMP)
		return (clamp_min == 0 && clamp_max >= textent);
	else if (clamp == CLAMP_REGION_REPEAT)
		return (clamp_max == 0 && clamp_min == textent);
	else
		return false;
}

__ri static constexpr u8 EffectiveClamp(u8 clamp, bool has_region)
{
	// When we have extracted the region in the texture, we can use the hardware sampler for repeat/clamp.
	// (weird flip here because clamp/repeat is inverted for region vs non-region).
	return (clamp >= CLAMP_REGION_CLAMP && has_region) ? (clamp ^ 3) : clamp;
}

__ri void GSRendererHW::EmulateTextureSampler(const GSTextureCache::Target* rt, const GSTextureCache::Target* ds, GSTextureCache::Source* tex,
	const TextureMinMaxResult& tmm, GSDevice::RecycledTexture& src_copy)
{
	// don't overwrite the texture when using channel shuffle, but keep the palette
	if (!m_channel_shuffle)
		m_conf.tex = tex->m_texture;
	m_conf.pal = tex->m_palette;

	// Hazard handling (i.e. reading from the current RT/DS).
	GSTextureCache::SourceRegion source_region = tex->GetRegion();
	bool target_region = tex->IsFromTarget() && source_region.HasEither();
	GSVector2i unscaled_size = target_region ? tex->GetRegionSize() : tex->GetUnscaledSize();
	float scale = tex->GetScale();
	HandleTextureHazards(rt, ds, tex, tmm, source_region, target_region, unscaled_size, scale, src_copy);

	// This is used for reading depth sources, so we should go off the source scale.
	// the Z vector contains line width which will be based on the target draw, where XY are used for source reading.
	const float scale_factor = scale;
	const float scale_rt = rt ? rt->GetScale() : ds->GetScale();

	m_conf.cb_ps.ScaleFactor = GSVector4(scale_factor * (1.0f / 16.0f), 1.0f / scale_factor, scale_rt, 0.0f);

	// Warning fetch the texture PSM format rather than the context format. The latter could have been corrected in the texture cache for depth.
	//const GSLocalMemory::psm_t &psm = GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM];
	const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[tex->m_TEX0.PSM];
	const GSLocalMemory::psm_t& cpsm = psm.pal > 0 ? GSLocalMemory::m_psm[m_cached_ctx.TEX0.CPSM] : psm;

	// Redundant clamp tests are restricted to local memory/1x sources only, if we're from a target,
	// we keep the shader clamp. See #5851 on github, and the note in Draw().
	[[maybe_unused]] static constexpr const char* clamp_modes[] = {"REPEAT", "CLAMP", "REGION_CLAMP", "REGION_REPEAT"};
	const bool redundant_wms = IsRedundantClamp(m_cached_ctx.CLAMP.WMS, m_cached_ctx.CLAMP.MINU,
	                                            m_cached_ctx.CLAMP.MAXU, m_cached_ctx.TEX0.TW);
	const bool redundant_wmt = IsRedundantClamp(m_cached_ctx.CLAMP.WMT, m_cached_ctx.CLAMP.MINV,
	                                            m_cached_ctx.CLAMP.MAXV, m_cached_ctx.TEX0.TH);
	const u8 wms = EffectiveClamp(m_cached_ctx.CLAMP.WMS, !tex->m_target && (source_region.HasX() || redundant_wms));
	const u8 wmt = EffectiveClamp(m_cached_ctx.CLAMP.WMT, !tex->m_target && (source_region.HasY() || redundant_wmt));
	const bool complex_wms_wmt = !!((wms | wmt) & 2) || target_region;
	GL_CACHE("HW: FST: %s WMS: %s [%s%s] WMT: %s [%s%s] Complex: %d TargetRegion: %d MINU: %d MAXU: %d MINV: %d MAXV: %d",
		PRIM->FST ? "UV" : "STQ", clamp_modes[m_cached_ctx.CLAMP.WMS], redundant_wms ? "redundant," : "",
		clamp_modes[wms], clamp_modes[m_cached_ctx.CLAMP.WMT], redundant_wmt ? "redundant," : "", clamp_modes[wmt],
		complex_wms_wmt, target_region, m_cached_ctx.CLAMP.MINU, m_cached_ctx.CLAMP.MAXU, m_cached_ctx.CLAMP.MINV,
		m_cached_ctx.CLAMP.MAXV);

	const bool need_mipmap = IsMipMapDraw();
	const bool shader_emulated_sampler = tex->m_palette || (tex->m_target && !m_conf.ps.shuffle && cpsm.fmt != 0) ||
	                                     complex_wms_wmt || psm.depth || target_region;
	const bool can_trilinear = !tex->m_palette && !tex->m_target && !m_conf.ps.shuffle;
	const bool trilinear_manual = need_mipmap && GSConfig.HWMipmap;

	bool bilinear = m_vt.IsLinear();
	int trilinear = 0;
	bool trilinear_auto = false; // Generate mipmaps if needed (basic).
	switch (GSConfig.TriFilter)
	{
		case TriFiltering::Forced:
		{
			// Force bilinear otherwise we can end up with min/mag nearest and mip linear.
			// We don't need to check for HWMipmapLevel::Off here, because forced trilinear implies forced mipmaps.
			bilinear = true;
			if (can_trilinear)
			{
				trilinear = static_cast<u8>(GS_MIN_FILTER::Linear_Mipmap_Linear);
				trilinear_auto = !tex->m_target && (!need_mipmap || !GSConfig.HWMipmap);
			}
		}
		break;

		case TriFiltering::PS2:
		case TriFiltering::Automatic:
		{
			// Can only use PS2 trilinear when mipmapping is enabled.
			if (need_mipmap && GSConfig.HWMipmap && can_trilinear)
			{
				trilinear = m_context->TEX1.MMIN;
				trilinear_auto = !tex->m_target && !GSConfig.HWMipmap;
			}
		}
		break;

		case TriFiltering::Off:
		default:
			break;
	}

	// 1 and 0 are equivalent
	m_conf.ps.wms = (wms & 2 || target_region) ? wms : 0;
	m_conf.ps.wmt = (wmt & 2 || target_region) ? wmt : 0;

	// Depth + bilinear filtering isn't done yet. But if the game has just set a Z24 swizzle on a colour texture, we can
	// just pretend it's not a depth format, since in the texture cache, it's not.
	// Other games worth testing: Area 51, Burnout
	if (psm.depth && m_vt.IsLinear() && tex->GetTexture()->IsDepthStencil())
		GL_INS("HW: WARNING: Depth + bilinear filtering not supported");

	// Performance note:
	// 1/ Don't set 0 as it is the default value
	// 2/ Only keep aem when it is useful (avoid useless shader permutation)
	if (m_conf.ps.shuffle)
	{
		const GIFRegTEXA& TEXA = m_cached_ctx.TEXA;

		// Force a 32 bits access (normally shuffle is done on 16 bits)
		// m_ps_sel.tex_fmt = 0; // removed as an optimization

		//ASSERT(tex->m_target);
		m_conf.ps.aem = TEXA.AEM;

		// Require a float conversion if the texure is a depth otherwise uses Integral scaling
		if (psm.depth)
		{
			m_conf.ps.depth_fmt = (tex->m_texture->GetType() != GSTexture::Type::DepthStencil) ? 3 : tex->m_32_bits_fmt ? 1 : 2;
		}

		// Shuffle is a 16 bits format, so aem is always required
		if (m_cached_ctx.TEX0.TCC)
		{
			GSVector4 ta(TEXA & GSVector4i::x000000ff());
			ta /= 255.0f;
			m_conf.cb_ps.TA_MaxDepth_Af.x = ta.x;
			m_conf.cb_ps.TA_MaxDepth_Af.y = ta.y;
		}

		// The purpose of texture shuffle is to move color channel. Extra interpolation is likely a bad idea.
		bilinear &= m_vt.IsLinear();

		const GSVector4 half_pixel = RealignTargetTextureCoordinate(tex);
		m_conf.cb_vs.texture_offset = GSVector2(half_pixel.x, half_pixel.y);

		// Can be seen with the cabin part of the ship in God of War, offsets are required when using FST.
		// ST uses a normalized position so doesn't need an offset here, will break Bionicle Heroes.
		if (GSConfig.UserHacks_HalfPixelOffset == GSHalfPixelOffset::NativeWTexOffset)
		{
			const u32 psm = rt ? rt->m_TEX0.PSM : ds->m_TEX0.PSM;
			const bool can_offset = m_r.width() > GSLocalMemory::m_psm[psm].pgs.x || m_r.height() > GSLocalMemory::m_psm[psm].pgs.y;

			if (can_offset && tex->m_scale > 1.0f)
			{
				const GSVertex* v = &m_vertex.buff[0];
				if (PRIM->FST)
				{
					const int x1_frac = ((v[1].XYZ.X - m_context->XYOFFSET.OFX) & 0xf);
					const int y1_frac = ((v[1].XYZ.Y - m_context->XYOFFSET.OFY) & 0xf);

					if (!(x1_frac & 8))
						m_conf.cb_vs.texture_offset.x = (1.0f - ((0.5f / (tex->m_unscaled_size.x * tex->m_scale)) * tex->m_unscaled_size.x)) * 8.0f;
					if (!(y1_frac & 8))
						m_conf.cb_vs.texture_offset.y = (1.0f - ((0.5f / (tex->m_unscaled_size.y * tex->m_scale)) * tex->m_unscaled_size.y)) * 8.0f;
				}
			}
		}
	}
	else if (tex->m_target)
	{
		const GIFRegTEXA& TEXA = m_cached_ctx.TEXA;

		// Use an old target. AEM and index aren't resolved it must be done
		// on the GPU

		// Select the 32/24/16 bits color (AEM)
		m_conf.ps.aem_fmt = cpsm.fmt;
		m_conf.ps.aem = TEXA.AEM;

		// Don't upload AEM if format is 32 bits
		if (cpsm.fmt)
		{
			GSVector4 ta(TEXA & GSVector4i::x000000ff());
			ta /= 255.0f;
			m_conf.cb_ps.TA_MaxDepth_Af.x = ta.x;
			m_conf.cb_ps.TA_MaxDepth_Af.y = ta.y;
		}

		// Select the index format
		if (tex->m_palette)
		{
			// FIXME Potentially improve fmt field in GSLocalMemory
			if (m_cached_ctx.TEX0.PSM == PSMT4HL)
				m_conf.ps.pal_fmt = 1;
			else if (m_cached_ctx.TEX0.PSM == PSMT4HH)
				m_conf.ps.pal_fmt = 2;
			else
				m_conf.ps.pal_fmt = 3;

			// Alpha channel of the RT is reinterpreted as an index. Star
			// Ocean 3 uses it to emulate a stencil buffer.  It is a very
			// bad idea to force bilinear filtering on it.
			bilinear &= m_vt.IsLinear();
		}

		// Depth format
		if (tex->m_texture->IsDepthStencil())
		{
			// Require a float conversion if the texure is a depth format
			m_conf.ps.depth_fmt = (psm.bpp == 16) ? 2 : 1;

			// Don't force interpolation on depth format
			bilinear &= m_vt.IsLinear();
		}

		const GSVector4 half_pixel = RealignTargetTextureCoordinate(tex);
		m_conf.cb_vs.texture_offset = GSVector2(half_pixel.x, half_pixel.y);

		if (GSConfig.UserHacks_HalfPixelOffset == GSHalfPixelOffset::NativeWTexOffset)
		{
			const u32 psm = rt ? rt->m_TEX0.PSM : ds->m_TEX0.PSM;
			const bool can_offset = m_r.width() > GSLocalMemory::m_psm[psm].pgs.x || m_r.height() > GSLocalMemory::m_psm[psm].pgs.y;

			if (can_offset && tex->m_scale > 1.0f)
			{
				const GSVertex* v = &m_vertex.buff[0];
				if (PRIM->FST)
				{
					const int x1_frac = ((v[1].XYZ.X - m_context->XYOFFSET.OFX) & 0xf);
					const int y1_frac = ((v[1].XYZ.Y - m_context->XYOFFSET.OFY) & 0xf);

					if (!(x1_frac & 8))
						m_conf.cb_vs.texture_offset.x = (1.0f - ((0.5f / (tex->m_unscaled_size.x * tex->m_scale)) * tex->m_unscaled_size.x)) * 8.0f;
					if (!(y1_frac & 8))
						m_conf.cb_vs.texture_offset.y = (1.0f - ((0.5f / (tex->m_unscaled_size.y * tex->m_scale)) * tex->m_unscaled_size.y)) * 8.0f;
				}
				else if (m_vt.m_eq.q)
				{
					const float tw = static_cast<float>(1 << m_cached_ctx.TEX0.TW);
					const float th = static_cast<float>(1 << m_cached_ctx.TEX0.TH);
					const float q = v[0].RGBAQ.Q;

					m_conf.cb_vs.texture_offset.x = 0.5f * q / tw;
					m_conf.cb_vs.texture_offset.y = 0.5f * q / th;
				}
			}
		}

		if (m_vt.m_primclass == GS_SPRITE_CLASS && m_index.tail >= 4 && GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].bpp >= 16 &&
			((tex->m_from_target_TEX0.PSM & 0x30) == 0x30 || GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].pal > 0))
		{
			HandleManualDeswizzle();
		}
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

	if (m_cached_ctx.TEX0.TFX == TFX_MODULATE && m_vt.m_eq.rgba == 0xFFFF && m_vt.m_min.c.eq(GSVector4i(128)))
	{
		// Micro optimization that reduces GPU load (removes 5 instructions on the FS program)
		m_conf.ps.tfx = TFX_DECAL;
	}
	else
	{
		m_conf.ps.tfx = m_cached_ctx.TEX0.TFX;
	}

	m_conf.ps.tcc = m_cached_ctx.TEX0.TCC;

	m_conf.ps.ltf = bilinear && shader_emulated_sampler;
	m_conf.ps.point_sampler = g_gs_device->Features().broken_point_sampler && GSConfig.GPUPaletteConversion && !target_region && (!bilinear || shader_emulated_sampler);

	const int tw = static_cast<int>(1 << m_cached_ctx.TEX0.TW);
	const int th = static_cast<int>(1 << m_cached_ctx.TEX0.TH);
	const int miptw = 1 << tex->m_TEX0.TW;
	const int mipth = 1 << tex->m_TEX0.TH;

	const GSVector4 WH(static_cast<float>(tw), static_cast<float>(th), miptw * scale, mipth * scale);

	// Reduction factor when source is a target and smaller/larger than TW/TH.
	m_conf.cb_ps.STScale = GSVector2(static_cast<float>(miptw) / static_cast<float>(unscaled_size.x),
		static_cast<float>(mipth) / static_cast<float>(unscaled_size.y));

	if (target_region)
	{
		// Use texelFetch() and clamp. Subtract one because the upper bound is exclusive.
		m_conf.cb_ps.STRange = GSVector4(tex->GetRegionRect() - GSVector4i::cxpr(0, 0, 1, 1)) * GSVector4(scale);
		m_conf.ps.region_rect = true;
	}
	else if (!tex->m_target)
	{
		// Targets aren't currently offset, so STScale takes care of it.
		if (source_region.HasX())
		{
			m_conf.cb_ps.STRange.x = static_cast<float>(source_region.GetMinX()) / static_cast<float>(miptw);
			m_conf.cb_ps.STRange.z = static_cast<float>(miptw) / static_cast<float>(source_region.GetWidth());
			m_conf.ps.adjs = 1;
		}
		if (source_region.HasY())
		{
			m_conf.cb_ps.STRange.y = static_cast<float>(source_region.GetMinY()) / static_cast<float>(mipth);
			m_conf.cb_ps.STRange.w = static_cast<float>(mipth) / static_cast<float>(source_region.GetHeight());
			m_conf.ps.adjt = 1;
		}
	}

	m_conf.ps.fst = !!PRIM->FST;

	m_conf.cb_ps.WH = WH;
	m_conf.cb_ps.HalfTexel = GSVector4(-0.5f, 0.5f).xxyy() / WH.zwzw();
	if (complex_wms_wmt)
	{
		// Add 0.5 to the coordinates because the region clamp is inclusive, size is exclusive. We use 0.5 because we want to clamp
		// to the last texel in the image, not halfway between it and wrapping around. We *should* be doing this when upscaling,
		// but having it off-by-one masks some draw issues in VP2 and Xenosaga. TODO: Fix the underlying draw issues.
		const GSVector4i clamp(m_cached_ctx.CLAMP.MINU, m_cached_ctx.CLAMP.MINV, m_cached_ctx.CLAMP.MAXU, m_cached_ctx.CLAMP.MAXV);
		const GSVector4 region_repeat = GSVector4::cast(clamp);

		// Apply a small offset (based on upscale amount) for edges of textures to avoid reading garbage during a clamp+stscale down
		// Bigger problem when WH is 1024x1024 and the target is only small.
		// This "fixes" a lot of the rainbow garbage in games when upscaling (and xenosaga shadows + VP2 forest seem quite happy).
		// Note that this is done on the original texture scale, during upscales it can mess up otherwise.
		const GSVector4 region_clamp_offset = ((GSConfig.UserHacks_HalfPixelOffset == GSHalfPixelOffset::Native && tex->GetScale() > 1.0f) && !m_channel_shuffle) ? 
												(GSVector4::cxpr(1.0f, 1.0f, 0.1f, 0.1f) + (GSVector4::cxpr(0.1f, 0.1f, 0.0f, 0.0f) * tex->GetScale())) :
		                                         GSVector4::cxpr(0.5f, 0.5f, 0.1f, 0.1f);

		const GSVector4 region_clamp = (GSVector4(clamp) + region_clamp_offset) / WH.xyxy();
		if (wms >= CLAMP_REGION_CLAMP)
		{
			m_conf.cb_ps.MinMax.x = (wms == CLAMP_REGION_CLAMP && !m_conf.ps.depth_fmt) ? region_clamp.x : region_repeat.x;
			m_conf.cb_ps.MinMax.z = (wms == CLAMP_REGION_CLAMP && !m_conf.ps.depth_fmt) ? region_clamp.z : region_repeat.z;
		}
		if (wmt >= CLAMP_REGION_CLAMP)
		{
			m_conf.cb_ps.MinMax.y = (wmt == CLAMP_REGION_CLAMP && !m_conf.ps.depth_fmt) ? region_clamp.y : region_repeat.y;
			m_conf.cb_ps.MinMax.w = (wmt == CLAMP_REGION_CLAMP && !m_conf.ps.depth_fmt) ? region_clamp.w : region_repeat.w;
		}
	}

	if (trilinear_manual)
	{
		m_conf.cb_ps.LODParams.x = static_cast<float>(m_context->TEX1.K) / 16.0f;
		m_conf.cb_ps.LODParams.y = static_cast<float>(1 << m_context->TEX1.L);
		m_conf.cb_ps.LODParams.z = static_cast<float>(m_lod.x); // Offset because first layer is m_lod, dunno if we can do better
		m_conf.cb_ps.LODParams.w = static_cast<float>(m_lod.y);
		m_conf.ps.manual_lod = 1;
	}
	else if (trilinear_auto)
	{
		tex->m_texture->GenerateMipmapsIfNeeded();
		m_conf.ps.automatic_lod = 1;
	}

	// TC Offset Hack
	m_conf.ps.tcoffsethack = m_userhacks_tcoffset;
	const GSVector4 tc_oh_ts = GSVector4(1 / 16.0f, 1 / 16.0f, m_userhacks_tcoffset_x, m_userhacks_tcoffset_y) / WH.xyxy();
	m_conf.cb_ps.TCOffsetHack = GSVector2(tc_oh_ts.z, tc_oh_ts.w);
	m_conf.cb_vs.texture_scale = GSVector2(tc_oh_ts.x, tc_oh_ts.y);

	// Only enable clamping in CLAMP mode. REGION_CLAMP will be done manually in the shader
	m_conf.sampler.tau = (wms == CLAMP_REPEAT && !target_region);
	m_conf.sampler.tav = (wmt == CLAMP_REPEAT && !target_region);
	if (shader_emulated_sampler)
	{
		m_conf.sampler.biln = 0;
		m_conf.sampler.aniso = 0;

		// Remove linear from trilinear, since we're doing the bilinear in the shader, and we only want this for mip selection.
		m_conf.sampler.triln = (trilinear >= static_cast<u8>(GS_MIN_FILTER::Linear_Mipmap_Nearest)) ?
		                           (trilinear - static_cast<u8>(GS_MIN_FILTER::Nearest_Mipmap_Nearest)) :
		                           0;
	}
	else
	{
		m_conf.sampler.biln = bilinear;
		// Aniso filtering doesn't work with textureLod so use texture (automatic_lod) instead.
		// Enable aniso only for triangles. Sprites are flat so aniso is likely useless (it would save perf for others primitives).
		const bool anisotropic = m_vt.m_primclass == GS_TRIANGLE_CLASS && !trilinear_manual;
		m_conf.sampler.aniso = anisotropic;
		m_conf.sampler.triln = trilinear;
		if (anisotropic && !trilinear_manual)
			m_conf.ps.automatic_lod = 1;
	}

	// clamp to base level if we're not providing or generating mipmaps
	// manual trilinear causes the chain to be uploaded, auto causes it to be generated
	m_conf.sampler.lodclamp = !(trilinear_manual || trilinear_auto);
}

__ri void GSRendererHW::HandleTextureHazards(const GSTextureCache::Target* rt, const GSTextureCache::Target* ds,
	const GSTextureCache::Source* tex, const TextureMinMaxResult& tmm, GSTextureCache::SourceRegion& source_region,
	bool& target_region, GSVector2i& unscaled_size, float& scale, GSDevice::RecycledTexture& src_copy)
{

	const int tex_diff = tex->m_from_target ? static_cast<int>(m_cached_ctx.TEX0.TBP0 - tex->m_from_target->m_TEX0.TBP0) : static_cast<int>(m_cached_ctx.TEX0.TBP0 - tex->m_TEX0.TBP0);
	const int frame_diff = rt ? static_cast<int>(m_cached_ctx.FRAME.Block() - rt->m_TEX0.TBP0) : 0;

	// Detect framebuffer read that will need special handling
	const GSTextureCache::Target* src_target = nullptr;
	if (!m_downscale_source || !tex->m_from_target)
	{
		if (rt && m_conf.tex == m_conf.rt && !(m_channel_shuffle && tex && (tex_diff != frame_diff || target_region)))
		{
			// Can we read the framebuffer directly? (i.e. sample location matches up).
			if (CanUseTexIsFB(rt, tex, tmm))
			{
				m_conf.tex = nullptr;
				m_conf.ps.tex_is_fb = true;
				if (m_prim_overlap == PRIM_OVERLAP_NO || !(g_gs_device->Features().texture_barrier || g_gs_device->Features().multidraw_fb_copy))
					m_conf.require_one_barrier = true;
				else
					m_conf.require_full_barrier = true;

				unscaled_size = rt->GetUnscaledSize();
				scale = rt->GetScale();
				return;
			}

			GL_CACHE("HW: Source is render target, taking copy.");
			src_target = rt;
		}
		// Be careful of single page channel shuffles where depth is the source but it's not going to the same place, we can't read this directly.
		else if (ds && m_conf.tex == m_conf.ds && (!m_channel_shuffle || (rt && static_cast<int>(m_cached_ctx.FRAME.Block() - rt->m_TEX0.TBP0) == static_cast<int>(m_cached_ctx.ZBUF.Block() - ds->m_TEX0.TBP0))))
		{
			// GL, Vulkan (in General layout), DirectX11 (binding dsv as read only) no support for DirectX12 yet!
			const bool can_read_current_depth_buffer = g_gs_device->Features().test_and_sample_depth;

			// If this is our current Z buffer, we might not be able to read it directly if it's being written to.
			// Rather than leaving the backend to do it, we'll check it here.
			if (can_read_current_depth_buffer && (m_cached_ctx.ZBUF.ZMSK || m_cached_ctx.TEST.ZTST == ZTST_NEVER))
			{
				// Safe to read!
				GL_CACHE("HW: Source is depth buffer, not writing, safe to read.");
				unscaled_size = ds->GetUnscaledSize();
				scale = ds->GetScale();
				return;
			}

			// Can't safely read the depth buffer, so we need to take a copy of it.
			GL_CACHE("HW: Source is depth buffer, unsafe to read, taking copy.");
			src_target = ds;
		}
		else if (m_channel_shuffle && tex->m_from_target && tex_diff != frame_diff)
		{
			src_target = tex->m_from_target;
		}
		else
		{
			// No match.
			return;
		}
	}
	else
		src_target = tex->m_from_target;

	// We need to copy. Try to cut down the source range as much as possible so we don't copy texels we're not reading.
	const GSVector2i& src_unscaled_size = src_target->GetUnscaledSize();
	const GSVector4i src_bounds = src_target->GetUnscaledRect();
	GSVector4i copy_range = GSVector4i::zero();
	GSVector2i copy_size = GSVector2i(0);
	GSVector2i copy_dst_offset = GSVector2i(0);
	// Shuffles take the whole target. This should've already been halved.
	// We can't partially copy depth targets in DirectX, and GL/Vulkan should use the direct read above.
	// Restricting it also breaks Tom and Jerry...
	if (m_downscale_source || m_channel_shuffle || tex->m_texture->GetType() == GSTexture::Type::DepthStencil)
	{
		if (m_channel_shuffle)
		{
			// Just make it the size of the RT, since it will be making a new target every draw (most likely) it saves making 130 new targets and drown
			copy_size.x = rt->m_unscaled_size.x;
			copy_size.y = rt->m_unscaled_size.y;
			copy_range.x = copy_range.y = 0;
			copy_range.z = std::min(m_r.width() + 1, copy_size.x);
			copy_range.w = std::min(m_r.height() + 1, copy_size.y);
		}
		else
		{
			copy_range = src_bounds;
			copy_size = src_unscaled_size;
		}

		GSVector4i::storel(&copy_dst_offset, copy_range);
		if (m_channel_shuffle && (tex_diff || frame_diff))
		{

			const u32 page_offset = (m_cached_ctx.TEX0.TBP0 - src_target->m_TEX0.TBP0) >> 5;
			const u32 horizontal_offset = (page_offset % src_target->m_TEX0.TBW) * GSLocalMemory::m_psm[src_target->m_TEX0.PSM].pgs.x;
			const u32 vertical_offset = (page_offset / src_target->m_TEX0.TBW) * GSLocalMemory::m_psm[src_target->m_TEX0.PSM].pgs.y;

			copy_range.x += horizontal_offset;
			copy_range.y += vertical_offset;
			copy_range.z += horizontal_offset;
			copy_range.w += vertical_offset;

			if (!m_channel_shuffle)
			{
				copy_size.y -= vertical_offset;
				copy_size.x -= horizontal_offset;
			}
			target_region = false;
			source_region.bits = 0;
			//copied_rt = tex->m_from_target != nullptr;
			if (m_in_target_draw && (page_offset || frame_diff))
			{
				copy_range.z = copy_range.x + m_r.width();
				copy_range.w = copy_range.y + m_r.height();

				if (tex_diff != frame_diff)
				{
					GSVector4i::storel(&copy_dst_offset, m_r);
				}
			}

			copy_range.z = std::min(copy_range.z, src_target->m_unscaled_size.x);
			copy_range.w = std::min(copy_range.w, src_target->m_unscaled_size.y);
		}
	}
	else
	{
		// If we're using TW/TH-based sizing, take the size from TEX0, not the target.
		const GSVector2i tex_size = GSVector2i(1 << m_cached_ctx.TEX0.TW, 1 << m_cached_ctx.TEX0.TH);
		copy_size.x = std::min(tex_size.x, src_unscaled_size.x);
		copy_size.y = std::min(tex_size.y, src_unscaled_size.y);

		// Use the texture min/max to get the copy range if not reinterpreted.
		if (m_texture_shuffle || m_channel_shuffle)
			copy_range = GSVector4i::loadh(copy_size);
		else
			copy_range = tmm.coverage;

		// Texture size above might be invalid (Timesplitters 2), extend if needed.
		if (m_cached_ctx.CLAMP.WMS >= CLAMP_REGION_CLAMP && copy_range.z > copy_size.x)
			copy_size.x = src_unscaled_size.x;
		if (m_cached_ctx.CLAMP.WMT >= CLAMP_REGION_CLAMP && copy_range.w > copy_size.y)
			copy_size.y = src_unscaled_size.y;

		// Apply target region offset.
		// TODO: Shrink the output texture to only the copy size.
		// Currently there's precision issues when using point sampling with normalized coordinates.
		// Once we move those over to texelFetch(), we should be able to shrink the size of the copy textures.
		if (target_region)
		{
			// Create a new texture using only the carved out region. Might save a bit of GPU time if we're lucky.
			const GSVector4i src_offset = GSVector4i(source_region.GetMinX(), source_region.GetMinY()).xyxy();
			copy_range += src_offset;
			copy_range = copy_range.rintersect(source_region.GetRect(src_unscaled_size.x, src_unscaled_size.y));
			GL_CACHE("HW: Applying target region at copy: %dx%d @ %d,%d => %d,%d", copy_range.width(), copy_range.height(),
				tmm.coverage.x, tmm.coverage.y, copy_range.x, copy_range.y);

			// Remove target region flag, we don't need to offset the coordinates anymore.
			source_region = {};
			target_region = false;

			// Make sure it's not out of the source's bounds.
			copy_range = copy_range.rintersect(src_bounds);

			// Unapply the region offset for the destination coordinates.
			const GSVector4i dst_range = copy_range - src_offset;
			GSVector4i::storel(&copy_dst_offset, dst_range);

			// We shouldn't need a larger texture because of the TS2 check above, but just in case.
			GSVector4i::storel(&copy_size, GSVector4i(copy_size).max_i32(dst_range.zwzw()));
		}
		else
		{
			// TODO: We also could use source region here to offset the coordinates.
			copy_range = copy_range.rintersect(src_bounds);
			GSVector4i::storel(&copy_dst_offset, copy_range);
		}
	}

	if (copy_range.rempty())
	{
		// Reading outside of the RT range.
		GL_CACHE("HW: ERROR: Reading outside of the RT range, using null texture.");
		unscaled_size = GSVector2i(1, 1);
		scale = 1.0f;
		m_conf.tex = nullptr;
		m_conf.ps.tfx = 4;
		return;
	}

	unscaled_size = copy_size;
	scale = m_downscale_source ? 1.0f : src_target->GetScale();
	GL_CACHE("HW: Copy size: %dx%d, range: %d,%d -> %d,%d (%dx%d) @ %.1f", copy_size.x, copy_size.y, copy_range.x,
		copy_range.y, copy_range.z, copy_range.w, copy_range.width(), copy_range.height(), scale);

	const GSVector2i scaled_copy_size = GSVector2i(static_cast<int>(std::ceil(static_cast<float>(copy_size.x) * scale)),
		static_cast<int>(std::ceil(static_cast<float>(copy_size.y) * scale)));

	src_copy.reset(src_target->m_texture->IsDepthStencil() ?
	                   g_gs_device->CreateDepthStencil(scaled_copy_size.x, scaled_copy_size.y, src_target->m_texture->GetFormat(), false) :
	                   g_gs_device->CreateRenderTarget(scaled_copy_size.x, scaled_copy_size.y, src_target->m_texture->GetFormat(), true, true));
	if (!src_copy) [[unlikely]]
	{
		Console.Error("HW: Failed to allocate %dx%d texture for hazard copy", scaled_copy_size.x, scaled_copy_size.y);
		m_conf.tex = nullptr;
		m_conf.ps.tfx = 4;
		return;
	}

	if (m_downscale_source)
	{
		g_perfmon.Put(GSPerfMon::TextureCopies, 1);

		// Can't use box filtering on depth (yet), or fractional scales.
		if (src_target->m_texture->IsDepthStencil() || std::floor(src_target->GetScale()) != src_target->GetScale())
		{
			GSVector4 src_rect = GSVector4(tmm.coverage) / GSVector4(GSVector4i::loadh(src_unscaled_size).zwzw());
			const GSVector4 dst_rect = GSVector4(tmm.coverage);
			g_gs_device->StretchRect(src_target->m_texture, src_rect, src_copy.get(), dst_rect,
				src_target->m_texture->IsDepthStencil() ? ShaderConvert::DEPTH_COPY : ShaderConvert::COPY, false);
		}
		else
		{
			// When using native HPO, the top-left column/row of pixels are often not drawn. Clamp these away to avoid sampling black,
			// causing bleeding into the edges of the downsampled texture.
			const u32 downsample_factor = static_cast<u32>(src_target->GetScale());
			const GSVector2i clamp_min = (GSConfig.UserHacks_HalfPixelOffset < GSHalfPixelOffset::Native) ?
			                                 GSVector2i(0, 0) :
			                                 GSVector2i(downsample_factor, downsample_factor);
			GSVector4i copy_rect = tmm.coverage;
			if (target_region)
			{
				copy_rect += GSVector4i(source_region.GetMinX(), source_region.GetMinY()).xyxy();
			}
			const GSVector4 dRect = GSVector4((copy_rect + GSVector4i(-1, 1).xxyy()).rintersect(src_target->GetUnscaledRect()));
			g_gs_device->FilteredDownsampleTexture(src_target->m_texture, src_copy.get(), downsample_factor, clamp_min, dRect);
		}
	}
	else
	{
		g_perfmon.Put(GSPerfMon::TextureCopies, 1);
		const GSVector4i offset = copy_range - GSVector4i(copy_dst_offset).xyxy();

		// Adjust for bilinear, must be done after calculating offset.
		copy_range.x -= 1;
		copy_range.y -= 1;
		copy_range.z += 1;
		copy_range.w += 1;
		copy_range = copy_range.rintersect(src_bounds);

		const GSVector4 src_rect = GSVector4(copy_range) / GSVector4(src_unscaled_size).xyxy();
		const GSVector4 dst_rect = (GSVector4(copy_range) - GSVector4(offset).xyxy()) * scale;

		g_gs_device->StretchRect(src_target->m_texture, src_rect, src_copy.get(), dst_rect,
			src_target->m_texture->IsDepthStencil() ? ShaderConvert::DEPTH_COPY : ShaderConvert::COPY, false);
	}
	m_conf.tex = src_copy.get();
}

bool GSRendererHW::CanUseTexIsFB(const GSTextureCache::Target* rt, const GSTextureCache::Source* tex,
	const TextureMinMaxResult& tmm)
{
	// Minimum blending -> we can't use tex-is-fb.
	if (GSConfig.AccurateBlendingUnit == AccBlendLevel::Minimum)
	{
		GL_CACHE("HW: Disabling tex-is-fb due to minimum blending.");
		return false;
	}

	// the texture is offset, and the frame isn't also offset, we can't do this.
	if (tex->GetRegion().HasX() || tex->GetRegion().HasY())
	{
		if (m_cached_ctx.FRAME.Block() != m_cached_ctx.TEX0.TBP0)
			return false;
	}

	// If it's a channel shuffle, tex-is-fb should be fine.
	if (m_channel_shuffle)
	{
		GL_CACHE("HW: Enabling tex-is-fb for channel shuffle.");
		return true;
	}

	// If it's a channel shuffle, tex-is-fb is always fine.
	if (m_texture_shuffle)
	{
		// We can't do tex is FB if the source and destination aren't pointing to the same bit of texture.
		if (floor(abs(m_vt.m_min.t.y) + tex->m_region.GetMinY()) != floor(abs(m_vt.m_min.p.y)))
			return false;

		if (abs(floor(abs(m_vt.m_min.t.x) + tex->m_region.GetMinX()) - floor(abs(m_vt.m_min.p.x))) > 16)
			return false;

		GL_CACHE("HW: Enabling tex-is-fb for texture shuffle.");
		return true;
	}

	// No barriers -> we can't use tex-is-fb when there's overlap.
	if (!(g_gs_device->Features().texture_barrier || g_gs_device->Features().multidraw_fb_copy) && m_prim_overlap != PRIM_OVERLAP_NO)
	{
		GL_CACHE("HW: Disabling tex-is-fb due to no barriers.");
		return false;
	}

	static constexpr auto check_clamp = [](u32 clamp, u32 min, u32 max, s32 tmin, s32 tmax) {
		if (clamp == CLAMP_REGION_CLAMP)
		{
			if (tmin < static_cast<s32>(min) || tmax > static_cast<s32>(max + 1))
			{
				GL_CACHE("HW: Disabling tex-is-fb due to REGION_CLAMP [%d, %d] with TMM of [%d, %d]", min, max, tmin, tmax);
				return false;
			}
		}
		else if (clamp == CLAMP_REGION_REPEAT)
		{
			const u32 req_tbits = (tmax > 1) ? (std::bit_ceil(static_cast<u32>(tmax - 1)) - 1) : 0x1;
			if ((min & req_tbits) != req_tbits)
			{
				GL_CACHE("HW: Disabling tex-is-fb due to REGION_REPEAT [%d, %d] with TMM of [%d, %d] and tbits of %d",
					min, max, tmin, tmax, req_tbits);
				return false;
			}
		}

		return true;
	};
	if (!check_clamp(
			m_cached_ctx.CLAMP.WMS, m_cached_ctx.CLAMP.MINU, m_cached_ctx.CLAMP.MAXU, tmm.coverage.x, tmm.coverage.z) ||
		!check_clamp(
			m_cached_ctx.CLAMP.WMT, m_cached_ctx.CLAMP.MINV, m_cached_ctx.CLAMP.MAXV, tmm.coverage.y, tmm.coverage.w))
	{
		return false;
	}

	// Texture is actually the frame buffer. Stencil emulation to compute shadow (Jak series/tri-ace game)
	// Will hit the "m_ps_sel.tex_is_fb = 1" path in the draw
	const bool is_quads = (m_vt.m_primclass == GS_SPRITE_CLASS || m_prim_overlap == PRIM_OVERLAP_NO);
	if (is_quads)
	{
		// No bilinear for tex-is-fb.
		if (m_vt.IsLinear())
		{
			GL_CACHE("HW: Disabling tex-is-fb due to bilinear sampling.");
			return false;
		}

		// Can't do tex-is-fb if paletted and we're not a shuffle (C32 -> P8).
		// This one shouldn't happen anymore, because all conversion should be done already.
		const GSLocalMemory::psm_t& tex_psm = GSLocalMemory::m_psm[tex->m_TEX0.PSM];
		const GSLocalMemory::psm_t& rt_psm = GSLocalMemory::m_psm[rt->m_TEX0.PSM];
		if (tex_psm.pal > 0 && tex_psm.bpp < rt_psm.bpp)
		{
			GL_CACHE("HW: Enabling tex-is-fb for palette conversion.");
			return true;
		}

		// Make sure that we're not sampling away from the area we're rendering.
		// We need to take the absolute here, because Beyond Good and Evil undithers itself using a -1,-1 offset.
		const GSVector4 diff(m_vt.m_min.p.upld(m_vt.m_max.p) - m_vt.m_min.t.upld(m_vt.m_max.t));
		GL_CACHE("HW: Coord diff: %f,%f", diff.x, diff.y);
		if ((diff.abs() < GSVector4(1.0f)).alltrue())
		{
			GL_CACHE("HW: Enabling tex-is-fb for sampling from rendered texel.");
			return true;
		}

		GL_CACHE("HW: Disabling tex-is-fb due to coord diff too large.");
		return false;
	}

	if (m_vt.m_primclass == GS_TRIANGLE_CLASS)
	{
		// This pattern is used by several games to emulate a stencil (shadow)
		// Ratchet & Clank, Jak do alpha integer multiplication (tfx) which is mostly equivalent to +1/-1
		// Tri-Ace (Star Ocean 3/RadiataStories/VP2) uses a palette to handle the +1/-1
		// Update: This isn't really needed anymore, if you use autoflush, however there's a reasonable speed impact (about 30%) in not using this.
		// Added the diff check to make sure it's reading the same data seems to be fine for Jak, VP2, Star Ocean, without breaking X2: Wolverine, which broke without that check.
		const GSVector4 diff(m_vt.m_min.p.upld(m_vt.m_max.p) - m_vt.m_min.t.upld(m_vt.m_max.t));
		if (m_cached_ctx.FRAME.FBMSK == 0x00FFFFFF && (diff.abs() < GSVector4(1.0f)).alltrue())
		{
			GL_CACHE("HW: Elabling tex-is-fb hack for Jak.");
			return true;
		}

		GL_CACHE("HW: Disabling tex-is-fb tue to triangle draw.");
		return false;
	}

	return false;
}

void GSRendererHW::EmulateATST(float& AREF, GSHWDrawConfig::PSSelector& ps, bool pass_2)
{
	static const u32 inverted_atst[] = {ATST_ALWAYS, ATST_NEVER, ATST_GEQUAL, ATST_GREATER, ATST_NOTEQUAL, ATST_LESS, ATST_LEQUAL, ATST_EQUAL};

	if (!m_cached_ctx.TEST.ATE)
		return;

	// Check for pass 2, otherwise do pass 1.
	const int atst = pass_2 ? inverted_atst[m_cached_ctx.TEST.ATST] : m_cached_ctx.TEST.ATST;
	const float aref = static_cast<float>(m_cached_ctx.TEST.AREF);

	switch (atst)
	{
		case ATST_LESS:
			AREF = aref - 0.1f;
			ps.atst = 1;
			break;
		case ATST_LEQUAL:
			AREF = aref - 0.1f + 1.0f;
			ps.atst = 1;
			break;
		case ATST_GEQUAL:
			AREF = aref - 0.1f;
			ps.atst = 2;
			break;
		case ATST_GREATER:
			AREF = aref - 0.1f + 1.0f;
			ps.atst = 2;
			break;
		case ATST_EQUAL:
			AREF = aref;
			ps.atst = 3;
			break;
		case ATST_NOTEQUAL:
			AREF = aref;
			ps.atst = 4;
			break;
		case ATST_NEVER: // Draw won't be done so no need to implement it in shader
		case ATST_ALWAYS:
		default:
			ps.atst = 0;
			break;
	}
}

void GSRendererHW::CleanupDraw(bool invalidate_temp_src)
{
	// Remove any RT source.
	if (invalidate_temp_src)
		g_texture_cache->InvalidateTemporarySource();
	// Restore Scissor.
	m_context->UpdateScissor();

	// Restore offsets.
	if ((m_context->FRAME.U32[0] ^ m_cached_ctx.FRAME.U32[0]) & 0x3f3f01ff)
		m_context->offset.fb = m_mem.GetOffset(m_context->FRAME.Block(), m_context->FRAME.FBW, m_context->FRAME.PSM);
	if ((m_context->ZBUF.U32[0] ^ m_cached_ctx.ZBUF.U32[0]) & 0x3f0001ff)
		m_context->offset.zb = m_mem.GetOffset(m_context->ZBUF.Block(), m_context->FRAME.FBW, m_context->ZBUF.PSM);
}

void GSRendererHW::ResetStates()
{
	// We don't want to zero out the constant buffers, since fields used by the current draw could result in redundant uploads.
	// This memset should be pretty efficient - the struct is 16 byte aligned, as is the cb_vs offset.
	memset(static_cast<void*>(&m_conf), 0, reinterpret_cast<const char*>(&m_conf.cb_vs) - reinterpret_cast<const char*>(&m_conf));
}

__ri void GSRendererHW::DrawPrims(GSTextureCache::Target* rt, GSTextureCache::Target* ds, GSTextureCache::Source* tex, const TextureMinMaxResult& tmm)
{
#ifdef ENABLE_OGL_DEBUG
	const GSVector4i area_out = GSVector4i(m_vt.m_min.p.upld(m_vt.m_max.p)).rintersect(m_context->scissor.in);
	const GSVector4i area_in = GSVector4i(m_vt.m_min.t.upld(m_vt.m_max.t));

	GL_PUSH("HW: GL Draw from (area %d,%d => %d,%d) in (area %d,%d => %d,%d)",
		area_in.x, area_in.y, area_in.z, area_in.w,
		area_out.x, area_out.y, area_out.z, area_out.w);
#endif

	const GSDrawingEnvironment& env = *m_draw_env;
	bool DATE = rt && m_cached_ctx.TEST.DATE && m_cached_ctx.FRAME.PSM != PSMCT24;
	bool DATE_PRIMID = false;
	bool DATE_BARRIER = false;
	bool DATE_one = false;

	ResetStates();

	m_conf.cb_vs.texture_offset = {};
	m_conf.ps.scanmsk = env.SCANMSK.MSK;
	m_conf.rt = rt ? rt->m_texture : nullptr;
	m_conf.ds = ds ? (m_using_temp_z ? g_texture_cache->GetTemporaryZ() : ds->m_texture) : nullptr;

	pxAssert(!ds || !rt || (m_conf.ds->GetSize().x == m_conf.rt->GetSize().x && m_conf.ds->GetSize().y == m_conf.rt->GetSize().y));

	// Z setup has to come before channel shuffle
	EmulateZbuffer(ds);

	// HLE implementation of the channel selection effect
	//
	// Warning it must be done at the begining because it will change the
	// vertex list (it will interact with PrimitiveOverlap and accurate
	// blending)
	if (m_channel_shuffle && tex && tex->m_from_target)
		EmulateChannelShuffle(tex->m_from_target, false, rt);

	// Upscaling hack to avoid various line/grid issues
	MergeSprite(tex);

	m_prim_overlap = PrimitiveOverlap(false);

	if (rt)
	{
		EmulateTextureShuffleAndFbmask(rt, tex);
		if (m_index.tail == 0)
		{
			GL_INS("HW: DrawPrims: Texture shuffle emulation culled all vertices; exiting.");
			return;
		}
	}

	const GSDevice::FeatureSupport features = g_gs_device->Features();

	if (DATE)
	{
		const bool is_overlap_alpha = m_prim_overlap != PRIM_OVERLAP_NO && !(m_cached_ctx.FRAME.FBMSK & 0x80000000);
		if (m_cached_ctx.TEST.DATM == 0)
		{
			// Some pixles are >= 1 so some fail, or some pixels get written but the written alpha matches or exceeds 1 (so overlap doesn't always pass).
			DATE = rt->m_alpha_max >= 128 || (is_overlap_alpha && rt->m_alpha_min < 128 && (GetAlphaMinMax().max >= 128 || (m_context->FBA.FBA || IsCoverageAlpha())));

			// All pixels fail.
			if (DATE && rt->m_alpha_min >= 128)
				return;
		}
		else
		{
			// Some pixles are < 1 so some fail, or some pixels get written but the written alpha goes below 1 (so overlap doesn't always pass).
			DATE = rt->m_alpha_min < 128 || (is_overlap_alpha && rt->m_alpha_max >= 128 && (GetAlphaMinMax().min < 128 && !(m_context->FBA.FBA || IsCoverageAlpha())));

			// All pixels fail.
			if (DATE && rt->m_alpha_max < 128)
				return;
		}
	}

	const int afail_type = m_cached_ctx.TEST.GetAFAIL(m_cached_ctx.FRAME.PSM);
	if (m_cached_ctx.TEST.ATE && ((afail_type != AFAIL_FB_ONLY && afail_type != AFAIL_RGB_ONLY) || !NeedsBlending() || !IsUsingAsInBlend()))
	{
		const int aref = static_cast<int>(m_cached_ctx.TEST.AREF);
		CorrectATEAlphaMinMax(m_cached_ctx.TEST.ATST, aref);
	}

	const bool needs_ad = rt && m_context->ALPHA.C == 1 && rt->m_alpha_min != rt->m_alpha_max && rt->m_alpha_max > 128;

	// Blend
	int blend_alpha_min = 0, blend_alpha_max = 255;
	int rt_new_alpha_min = 0, rt_new_alpha_max = 255;
	if (rt)
	{
		GL_INS("HW: RT alpha was %s before draw", rt->m_rt_alpha_scale ? "scaled" : "NOT scaled");

		blend_alpha_min = rt_new_alpha_min = rt->m_alpha_min;
		blend_alpha_max = rt_new_alpha_max = rt->m_alpha_max;

		const int fba_value = m_draw_env->CTXT[m_draw_env->PRIM.CTXT].FBA.FBA * 128;
		const bool is_24_bit = (GSLocalMemory::m_psm[rt->m_TEX0.PSM].trbpp == 24);
		if (is_24_bit)
		{
			// C24/Z24 - alpha is 1.
			blend_alpha_min = 128;
			blend_alpha_max = 128;
		}

		if (GSUtil::GetChannelMask(m_cached_ctx.FRAME.PSM) & 0x8 && !m_texture_shuffle)
		{
			const int s_alpha_max = GetAlphaMinMax().max | fba_value;
			const int s_alpha_min = GetAlphaMinMax().min | fba_value;

			const bool afail_always_fb_alpha = m_cached_ctx.TEST.AFAIL == AFAIL_FB_ONLY || (m_cached_ctx.TEST.AFAIL == AFAIL_RGB_ONLY && GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].trbpp != 32);
			const bool always_passing_alpha = !m_cached_ctx.TEST.ATE || afail_always_fb_alpha || (m_cached_ctx.TEST.ATE && m_cached_ctx.TEST.ATST == ATST_ALWAYS);
			const bool full_cover = rt->m_valid.rintersect(m_r).eq(rt->m_valid) && m_primitive_covers_without_gaps == NoGapsType::FullCover && !(DATE || !always_passing_alpha || !IsDepthAlwaysPassing());

			// On DX FBMask emulation can be missing on lower blend levels, so we'll do whatever the API does.
			const u32 fb_mask = m_conf.colormask.wa ? (m_conf.ps.fbmask ? m_conf.cb_ps.FbMask.a : 0) : 0xFF;
			const u32 alpha_mask = (GSLocalMemory::m_psm[rt->m_TEX0.PSM].fmsk & 0xFF000000) >> 24;
			if ((fb_mask & alpha_mask) == 0)
			{
				if (full_cover)
				{
					rt_new_alpha_max = s_alpha_max;
					rt_new_alpha_min = s_alpha_min;
				}
				else
				{
					rt_new_alpha_max = std::max(s_alpha_max, rt_new_alpha_max);
					rt_new_alpha_min = std::min(s_alpha_min, rt_new_alpha_min);
				}
			}
			else if ((fb_mask & alpha_mask) != alpha_mask) // We can't be sure of the alpha if it's partially masked.
			{
				// Any number of bits could be set, so let's be paranoid about it
				const u32 new_max_alpha = (s_alpha_max != s_alpha_min) ? (std::min(s_alpha_max, ((1 << (32 - std::countl_zero(static_cast<u32>(s_alpha_max)))) - 1)) & ~fb_mask) : (s_alpha_max & ~fb_mask);
				const u32 curr_max = (rt_new_alpha_max != rt_new_alpha_min && rt->m_alpha_range) ? (((1 << (32 - std::countl_zero(static_cast<u32>(rt_new_alpha_max)))) - 1) & fb_mask) : ((rt_new_alpha_max | rt_new_alpha_min) & fb_mask);
				if (full_cover)
					rt_new_alpha_max = new_max_alpha | curr_max;
				else
					rt_new_alpha_max = std::max(static_cast<int>(new_max_alpha | curr_max), rt_new_alpha_max);

				rt_new_alpha_min = std::min(s_alpha_min, rt_new_alpha_min);
			}

			if ((fb_mask & alpha_mask) != alpha_mask)
			{
				if (full_cover && (fb_mask & alpha_mask) == 0)
					rt->m_alpha_range = s_alpha_max != s_alpha_min;
				else
					rt->m_alpha_range |= (s_alpha_max & ~fb_mask) != (s_alpha_min & ~fb_mask);
			}
		}
		else if ((m_texture_shuffle && m_conf.colormask.wa))
		{
			// in shuffles, the alpha top bit values are set according to TEXA
			const GSVector4i shuffle_rect = GSVector4i(m_vt.m_min.p.x, m_vt.m_min.p.y, m_vt.m_max.p.x, m_vt.m_max.p.y);
			if (!rt->m_valid.rintersect(shuffle_rect).eq(rt->m_valid) || (m_cached_ctx.FRAME.FBMSK & 0xFFFC0000))
			{
				rt_new_alpha_max = std::max(static_cast<int>((std::max(m_cached_ctx.TEXA.TA1, m_cached_ctx.TEXA.TA0) & 0x80) + 127), rt_new_alpha_max) | fba_value;
				rt_new_alpha_min = std::min(static_cast<int>(std::min(m_cached_ctx.TEXA.TA1, m_cached_ctx.TEXA.TA0) & 0x80), rt_new_alpha_min);
			}
			else
			{
				rt_new_alpha_max = (std::max(m_cached_ctx.TEXA.TA1, m_cached_ctx.TEXA.TA0) & 0x80) + 127 | fba_value;
				rt_new_alpha_min = (std::min(m_cached_ctx.TEXA.TA1, m_cached_ctx.TEXA.TA0) & 0x80) | fba_value;
			}
			rt->m_alpha_range = true;
		}

		GL_INS("HW: RT Alpha Range: %d-%d => %d-%d", blend_alpha_min, blend_alpha_max, rt_new_alpha_min, rt_new_alpha_max);

		// If there's no overlap, the values in the RT before FB write will be the old values.
		if (m_prim_overlap != PRIM_OVERLAP_NO)
		{
			// Otherwise, it may be a mix of the old/new values.
			blend_alpha_min = std::min(blend_alpha_min, rt_new_alpha_min);
			blend_alpha_max = std::max(blend_alpha_max, rt_new_alpha_max);
		}

		if (!rt->m_32_bits_fmt)
		{
			rt_new_alpha_max &= 128;
			rt_new_alpha_min &= 128;

			if (rt_new_alpha_max == rt_new_alpha_min)
				rt->m_alpha_range = false;
		}
	}

	// DATE: selection of the algorithm. Must be done before blending because GL42 is not compatible with blending
	if (DATE)
	{
		if (m_cached_ctx.TEST.DATM)
		{
			blend_alpha_min = std::max(blend_alpha_min, 128);
			blend_alpha_max = std::max(blend_alpha_max, 128);
		}
		else
		{
			blend_alpha_min = std::min(blend_alpha_min, 127);
			blend_alpha_max = std::min(blend_alpha_max, 127);
		}

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
		// When Blending is disabled and Edge Anti Aliasing is enabled,
		// the output alpha is Coverage (which we force to 128) so DATE will fail/pass guaranteed on second pass.
		else if (m_conf.colormask.wa && (m_context->FBA.FBA || IsCoverageAlpha()) && features.stencil_buffer)
		{
			GL_PERF("DATE: Fast with FBA, all pixels will be >= 128");
			DATE_one = !m_cached_ctx.TEST.DATM;
		}
		else if (m_conf.colormask.wa && !m_cached_ctx.TEST.ATE && !(m_cached_ctx.FRAME.FBMSK & 0x80000000))
		{
			// Performance note: check alpha range with GetAlphaMinMax()
			// Note: all my dump are already above 120fps, but it seems to reduce GPU load
			// with big upscaling
			if (m_cached_ctx.TEST.DATM && GetAlphaMinMax().max < 128 && features.stencil_buffer)
			{
				// Only first pixel (write 0) will pass (alpha is 1)
				GL_PERF("DATE: Fast with alpha %d-%d", GetAlphaMinMax().min, GetAlphaMinMax().max);
				DATE_one = true;
			}
			else if (!m_cached_ctx.TEST.DATM && GetAlphaMinMax().min >= 128 && features.stencil_buffer)
			{
				// Only first pixel (write 1) will pass (alpha is 0)
				GL_PERF("DATE: Fast with alpha %d-%d", GetAlphaMinMax().min, GetAlphaMinMax().max);
				DATE_one = true;
			}
			else if (features.texture_barrier && ((m_vt.m_primclass == GS_SPRITE_CLASS && ComputeDrawlistGetSize(rt->m_scale) < 10) || (m_index.tail < 30)))
			{
				// texture barrier will split the draw call into n draw call. It is very efficient for
				// few primitive draws. Otherwise it sucks.
				GL_PERF("DATE: Accurate with alpha %d-%d", GetAlphaMinMax().min, GetAlphaMinMax().max);
				m_conf.require_full_barrier = true;
				DATE_BARRIER = true;
			}
			else if ((features.texture_barrier || features.multidraw_fb_copy) && m_conf.require_full_barrier)
			{
				// Full barrier is enabled (likely sw fbmask), we need to use date barrier.
				GL_PERF("DATE: Accurate with alpha %d-%d", GetAlphaMinMax().min, GetAlphaMinMax().max);
				m_conf.require_full_barrier = true;
				DATE_BARRIER = true;
			}
			else if (features.primitive_id)
			{
				GL_PERF("DATE: Accurate with alpha %d-%d", GetAlphaMinMax().min, GetAlphaMinMax().max);
				DATE_PRIMID = true;
			}
			else if (features.texture_barrier)
			{
				GL_PERF("DATE: Accurate with alpha %d-%d", GetAlphaMinMax().min, GetAlphaMinMax().max);
				m_conf.require_full_barrier = true;
				DATE_BARRIER = true;
			}
			else if (features.stencil_buffer)
			{
				// Might be inaccurate in some cases but we shouldn't hit this path.
				GL_PERF("DATE: Fast with alpha %d-%d", GetAlphaMinMax().min, GetAlphaMinMax().max);
				DATE_one = true;
			}
		}
		else if (!m_conf.colormask.wa && !m_cached_ctx.TEST.ATE)
		{
			GL_PERF("DATE: Accurate with no alpha write");
			if (g_gs_device->Features().texture_barrier)
			{
				m_conf.require_one_barrier = true;
				DATE_BARRIER = true;
			}
		}

		// Will save my life !
		pxAssert(!(DATE_BARRIER && DATE_one));
		pxAssert(!(DATE_PRIMID && DATE_one));
		pxAssert(!(DATE_PRIMID && DATE_BARRIER));
	}

	// Before emulateblending, dither will be used
	m_conf.ps.dither = GSConfig.Dithering > 0 && m_conf.ps.dst_fmt == GSLocalMemory::PSM_FMT_16 && env.DTHE.DTHE;

	if (m_conf.ps.dst_fmt == GSLocalMemory::PSM_FMT_24)
	{
		// Disable writing of the alpha channel
		m_conf.colormask.wa = 0;
	}


	// Not gonna spend too much time with this, it's not likely to be used much, can't be less accurate than it was.
	if (ds)
	{
		ds->m_alpha_max = std::max(static_cast<u32>(ds->m_alpha_max), static_cast<u32>(m_vt.m_max.p.z) >> 24);
		ds->m_alpha_min = std::min(static_cast<u32>(ds->m_alpha_min), static_cast<u32>(m_vt.m_min.p.z) >> 24);
		GL_INS("HW: New DS Alpha Range: %d-%d", ds->m_alpha_min, ds->m_alpha_max);

		if (GSLocalMemory::m_psm[ds->m_TEX0.PSM].bpp == 16)
		{
			ds->m_alpha_max &= 128;
			ds->m_alpha_min &= 128;
		}
	}

	// If we Correct/Decorrect and tex is rt, we will need to update the texture reference
	const bool req_src_update = tex && rt && tex->m_target && tex->m_target_direct && tex->m_texture == rt->m_texture;

	// We defer updating the alpha scaled flag until after the texture setup if we're scaling as part of the draw,
	// because otherwise we'll treat tex-is-fb as a scaled source, when it's not yet.
	bool can_scale_rt_alpha = false;
	bool new_scale_rt_alpha = false;
	if (rt)
	{
		can_scale_rt_alpha = !needs_ad && (GSUtil::GetChannelMask(m_cached_ctx.FRAME.PSM) & 0x8) && rt_new_alpha_max <= 128;

		const bool partial_fbmask = (m_conf.ps.fbmask && m_conf.cb_ps.FbMask.a != 0xFF && m_conf.cb_ps.FbMask.a != 0);
		const bool rta_decorrection = m_channel_shuffle || m_texture_shuffle || (m_conf.colormask.wa && (rt_new_alpha_max > 128 || partial_fbmask));

		if (rta_decorrection)
		{
			if (m_texture_shuffle)
			{
				if (m_conf.ps.process_ba & SHUFFLE_READ)
				{
					can_scale_rt_alpha = false;

					rt->UnscaleRTAlpha();
					m_conf.rt = rt->m_texture;

					if (req_src_update)
						tex->m_texture = rt->m_texture;
				}
				else if (m_conf.colormask.wa)
				{
					if (!(m_cached_ctx.FRAME.FBMSK & 0xFFFC0000))
					{
						can_scale_rt_alpha = false;
						rt->m_rt_alpha_scale = false;
					}
					else if (m_cached_ctx.FRAME.FBMSK & 0xFFFC0000)
					{
						can_scale_rt_alpha = false;
						rt->UnscaleRTAlpha();
						m_conf.rt = rt->m_texture;

						if (req_src_update)
							tex->m_texture = rt->m_texture;
					}
				}
			}
			else if (m_channel_shuffle)
			{
				if (m_conf.ps.tales_of_abyss_hle || (tex && tex->m_from_target && tex->m_from_target == rt && m_conf.ps.channel == ChannelFetch_ALPHA) || partial_fbmask || rt_new_alpha_max > 128)
				{
					can_scale_rt_alpha = false;
					rt->UnscaleRTAlpha();
					m_conf.rt = rt->m_texture;

					if (req_src_update)
						tex->m_texture = rt->m_texture;
				}
			}
			else if (rt->m_last_draw == s_n)
			{
				can_scale_rt_alpha = false;
				rt->m_rt_alpha_scale = false;
			}
			else
			{
				can_scale_rt_alpha = false;
				rt->UnscaleRTAlpha();
				m_conf.rt = rt->m_texture;

				if (req_src_update)
					tex->m_texture = rt->m_texture;
			}
		}

		new_scale_rt_alpha = rt->m_rt_alpha_scale;
	}

	GSDevice::RecycledTexture tex_copy;
	if (tex)
	{
		EmulateTextureSampler(rt, ds, tex, tmm, tex_copy);
	}
	else
	{
		const float scale_factor = rt ? rt->GetScale() : ds->GetScale();
		m_conf.cb_ps.ScaleFactor = GSVector4(scale_factor * (1.0f / 16.0f), 1.0f / scale_factor, scale_factor, 0.0f);

		m_conf.ps.tfx = 4;
	}

	// AA1: Set alpha source to coverage 128 when there is no alpha blending.
	m_conf.ps.fixed_one_a = IsCoverageAlpha();

	if ((!IsOpaque() || m_context->ALPHA.IsBlack()) && rt && ((m_conf.colormask.wrgba & 0x7) || (m_texture_shuffle && !m_copy_16bit_to_target_shuffle && !m_same_group_texture_shuffle)))
	{
		EmulateBlending(blend_alpha_min, blend_alpha_max, DATE, DATE_PRIMID, DATE_BARRIER, rt, can_scale_rt_alpha, new_scale_rt_alpha);
	}
	else
	{
		m_conf.blend = {}; // No blending please
		m_conf.ps.no_color1 = true;

		if (can_scale_rt_alpha && !new_scale_rt_alpha && m_conf.colormask.wa)
		{
			const bool afail_always_fb_alpha = m_cached_ctx.TEST.AFAIL == AFAIL_FB_ONLY || (m_cached_ctx.TEST.AFAIL == AFAIL_RGB_ONLY && GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].trbpp != 32);
			const bool always_passing_alpha = !m_cached_ctx.TEST.ATE || afail_always_fb_alpha || (m_cached_ctx.TEST.ATE && m_cached_ctx.TEST.ATST == ATST_ALWAYS);
			const bool full_cover = rt->m_valid.rintersect(m_r).eq(rt->m_valid) && m_primitive_covers_without_gaps == NoGapsType::FullCover && !(DATE || !always_passing_alpha || !IsDepthAlwaysPassing());

			// Restrict this to only when we're overwriting the whole target.
			new_scale_rt_alpha = full_cover || rt->m_last_draw >= s_n;
		}
	}

	// Similar to IsRTWritten(), check if the rt will change.
	const bool no_rt = !rt || !(m_conf.colormask.wrgba || m_channel_shuffle);
	const bool no_ds = !ds ||
		// Depth will be written through the RT.
		(!no_rt && m_cached_ctx.FRAME.FBP == m_cached_ctx.ZBUF.ZBP && !PRIM->TME && m_cached_ctx.ZBUF.ZMSK == 0 &&
			(m_cached_ctx.FRAME.FBMSK & GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].fmsk) == 0 && m_cached_ctx.TEST.ZTE) ||
		// No color or Z being written.
		(no_rt && m_cached_ctx.ZBUF.ZMSK != 0);

	if (no_rt && no_ds)
	{
		GL_INS("HW: Late draw cancel DrawPrims().");
		return;
	}

	// Always swap DATE with DATE_BARRIER if we have barriers on when alpha write is masked.
	// This is always enabled on vk/gl but not on dx11/12 as copies are slow so we can selectively enable it like now.
	if (DATE && !m_conf.colormask.wa && (m_conf.require_one_barrier || m_conf.require_full_barrier))
		DATE_BARRIER = true;

	if ((m_conf.ps.tex_is_fb && rt && rt->m_rt_alpha_scale) || (tex && tex->m_from_target && tex->m_target_direct && tex->m_from_target->m_rt_alpha_scale))
		m_conf.ps.rta_source_correction = 1;

	if (req_src_update && tex->m_texture != rt->m_texture)
		tex->m_texture = rt->m_texture;

	if (rt)
	{
		rt->m_alpha_max = rt_new_alpha_max;
		rt->m_alpha_min = rt_new_alpha_min;
	}
	// Warning must be done after EmulateZbuffer
	// Depth test is always true so it can be executed in 2 passes (no order required) unlike color.
	// The idea is to compute first the color which is independent of the alpha test. And then do a 2nd
	// pass to handle the depth based on the alpha test.
	const bool ate_first_pass = m_cached_ctx.TEST.DoFirstPass();
	bool ate_second_pass = m_cached_ctx.TEST.DoSecondPass();
	bool ate_RGBA_then_Z = false;
	bool ate_RGB_then_Z = false;
	GL_INS("HW: %sAlpha Test, ATST=%s, AFAIL=%s", (ate_first_pass && ate_second_pass) ? "Complex" : "",
		GSUtil::GetATSTName(m_cached_ctx.TEST.ATST), GSUtil::GetAFAILName(m_cached_ctx.TEST.AFAIL));
	if (ate_first_pass && ate_second_pass)
	{
		const bool commutative_depth = (m_conf.depth.ztst == ZTST_GEQUAL && m_vt.m_eq.z) || (m_conf.depth.ztst == ZTST_ALWAYS) || !m_conf.depth.zwe;
		const bool commutative_alpha = (m_context->ALPHA.C != 1) || !m_conf.colormask.wa; // when either Alpha Src or a constant, or not updating A

		ate_RGBA_then_Z = (afail_type == AFAIL_FB_ONLY) && commutative_depth;
		ate_RGB_then_Z = (afail_type == AFAIL_RGB_ONLY) && commutative_depth && commutative_alpha;
	}

	if (ate_RGBA_then_Z)
	{
		GL_INS("HW: Alternate ATE handling: ate_RGBA_then_Z");
		// Render all color but don't update depth
		// ATE is disabled here
		m_conf.depth.zwe = false;
	}
	else
	{
		float aref = m_conf.cb_ps.FogColor_AREF.a;
		EmulateATST(aref, m_conf.ps, false);

		// avoid redundant cbuffer updates
		m_conf.cb_ps.FogColor_AREF.a = aref;
		m_conf.alpha_second_pass.ps_aref = aref;

		if (ate_RGB_then_Z)
		{
			GL_INS("HW: Alternate ATE handling: ate_RGB_then_Z");

			// Blending might be off, ensure it's enabled.
			// We write the alpha pass/fail to SRC1_ALPHA, which is used to update A.
			m_conf.ps.afail = AFAIL_RGB_ONLY;
			if ((features.framebuffer_fetch && m_conf.require_one_barrier) || m_conf.require_full_barrier)
			{
				// We're reading the rt anyways, use it for AFAIL
				// This ensures we don't attempt to use fbfetch + blend, which breaks Intel GPUs on Metal
				// Setting afail to RGB_ONLY without enabling color1 will enable this mode in the shader, so nothing more to do here.
			}
			else
			{
				m_conf.ps.no_color1 = false;
				if (!m_conf.blend.enable)
				{
					m_conf.blend = GSHWDrawConfig::BlendState(true, GSDevice::CONST_ONE, GSDevice::CONST_ZERO,
						GSDevice::OP_ADD, GSDevice::SRC1_ALPHA, GSDevice::INV_SRC1_ALPHA, false, 0);
				}
				else
				{
					if (m_conf.blend_multi_pass.enable)
					{
						m_conf.blend_multi_pass.no_color1 = false;
						m_conf.blend_multi_pass.blend.src_factor_alpha = GSDevice::SRC1_ALPHA;
						m_conf.blend_multi_pass.blend.dst_factor_alpha = GSDevice::INV_SRC1_ALPHA;
					}
					else
					{
						m_conf.blend.src_factor_alpha = GSDevice::SRC1_ALPHA;
						m_conf.blend.dst_factor_alpha = GSDevice::INV_SRC1_ALPHA;
					}
				}
			}

			// If Z writes are on, unfortunately we can't single pass it.
			// But we can write Z in the second pass instead.
			ate_RGBA_then_Z = m_conf.depth.zwe;
			ate_second_pass &= ate_RGBA_then_Z;
			m_conf.depth.zwe = false;

			// Swap stencil DATE for PrimID DATE, for both Z on and off cases.
			// Because we're making some pixels pass, but not update A, the stencil won't be synced.
			if (DATE && !DATE_BARRIER && features.primitive_id)
			{
				if (!DATE_PRIMID)
					GL_INS("HW: Swap stencil DATE for PrimID, due to AFAIL");

				DATE_one = false;
				DATE_PRIMID = true;
			}
		}
	}

	// No point outputting colours if we're just writing depth.
	// We might still need the framebuffer for DATE, though.
	if (!rt || m_conf.colormask.wrgba == 0)
	{
		m_conf.ps.DisableColorOutput();
		m_conf.colormask.wrgba = 0;
	}

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

	if (new_scale_rt_alpha)
		m_conf.datm = static_cast<SetDATM>(m_cached_ctx.TEST.DATM + 2);
	else
		m_conf.datm = static_cast<SetDATM>(m_cached_ctx.TEST.DATM);

	// If we're doing stencil DATE and we don't have a depth buffer, we need to allocate a temporary one.
	GSDevice::RecycledTexture temp_ds;
	if (m_conf.destination_alpha >= GSHWDrawConfig::DestinationAlphaMode::Stencil &&
		m_conf.destination_alpha <= GSHWDrawConfig::DestinationAlphaMode::StencilOne && !m_conf.ds)
	{
		const bool is_one_barrier = (features.texture_barrier && m_conf.require_full_barrier && (m_prim_overlap == PRIM_OVERLAP_NO || m_conf.ps.shuffle || m_channel_shuffle));
		if ((temp_ds.reset(g_gs_device->CreateDepthStencil(m_conf.rt->GetWidth(), m_conf.rt->GetHeight(), GSTexture::Format::DepthStencil, false)), temp_ds))
		{
			m_conf.ds = temp_ds.get();
		}
		else if (features.primitive_id && !(m_conf.ps.scanmsk & 2) && (!m_conf.require_full_barrier || is_one_barrier))
		{
			DATE_one = false;
			DATE_PRIMID = true;
			m_conf.destination_alpha = GSHWDrawConfig::DestinationAlphaMode::PrimIDTracking;
			DevCon.Warning("HW: Depth buffer creation failed for Stencil Date. Fallback to PrimIDTracking.");
		}
		else
		{
			DATE = false;
			DATE_one = false;
			m_conf.destination_alpha = GSHWDrawConfig::DestinationAlphaMode::Off;
			DevCon.Warning("HW: Depth buffer creation failed for Stencil Date.");
		}
	}

	// vs

	m_conf.vs.tme = m_process_texture;
	m_conf.vs.fst = PRIM->FST;

	// FIXME D3D11 and GL support half pixel center. Code could be easier!!!
	const GSTextureCache::Target* rt_or_ds = rt ? rt : ds;
	const float rtscale = rt_or_ds->GetScale();
	const GSVector2i rtsize = rt_or_ds->GetTexture()->GetSize();
	float sx, sy, ox2, oy2;
	const float ox = static_cast<float>(static_cast<int>(m_context->XYOFFSET.OFX));
	const float oy = static_cast<float>(static_cast<int>(m_context->XYOFFSET.OFY));

	if ((GSConfig.UserHacks_HalfPixelOffset < GSHalfPixelOffset::Native) && rtscale > 1.0f)
	{
		sx = 2.0f * rtscale / (rtsize.x << 4);
		sy = 2.0f * rtscale / (rtsize.y << 4);
		ox2 = -1.0f / rtsize.x;
		oy2 = -1.0f / rtsize.y;
		float mod_xy = 0.0f;
		//This hack subtracts around half a pixel from OFX and OFY.
		//
		//The resulting shifted output aligns better with common blending / corona / blurring effects,
		//but introduces a few bad pixels on the edges.
		if (!rt)
			mod_xy = GetModXYOffset();
		else
			mod_xy = rt->OffsetHack_modxy;

		if (mod_xy > 1.0f)
		{
			ox2 *= mod_xy;
			oy2 *= mod_xy;
		}
	}
	else
	{
		// Align coordinates to native resolution framebuffer, hope for the best.
		const int unscaled_x = rt_or_ds ? rt_or_ds->GetUnscaledWidth() : 0;
		const int unscaled_y = rt_or_ds ? rt_or_ds->GetUnscaledHeight() : 0;
		sx = 2.0f / (unscaled_x << 4);
		sy = 2.0f / (unscaled_y << 4);

		if (GSConfig.UserHacks_HalfPixelOffset == GSHalfPixelOffset::NativeWTexOffset)
		{
			ox2 = (-1.0f / (unscaled_x * rtscale));
			oy2 = (-1.0f / (unscaled_y * rtscale));

			// Having the vertex negatively offset is a common thing for copying sprites but this causes problems when upscaling, so we need to further adjust the offset.
			// This kinda screws things up when using ST, so let's not.
			if (m_vt.m_primclass == GS_SPRITE_CLASS && rtscale > 1.0f && (tex && PRIM->FST))
			{
				const GSVertex* v = &m_vertex.buff[0];
				const int x1_frac = ((v[1].XYZ.X - m_context->XYOFFSET.OFX) & 0xf);
				const int y1_frac = ((v[1].XYZ.Y - m_context->XYOFFSET.OFY) & 0xf);
				if (x1_frac & 8)
					ox2 *= 1.0f + ((static_cast<float>(16 - x1_frac) / 8.0f) * rtscale);

				if (y1_frac & 8)
					oy2 *= 1.0f + ((static_cast<float>(16 - y1_frac) / 8.0f) * rtscale);
			}
		}
		else
		{
			ox2 = -1.0f / unscaled_x;
			oy2 = -1.0f / unscaled_y;
		}
	}

	m_conf.cb_vs.vertex_scale = GSVector2(sx, sy);
	m_conf.cb_vs.vertex_offset = GSVector2(ox * sx + ox2 + 1, oy * sy + oy2 + 1);
	// END of FIXME

	// GS_SPRITE_CLASS are already flat (either by CPU or the GS)
	m_conf.ps.iip = (m_vt.m_primclass == GS_SPRITE_CLASS) ? 0 : PRIM->IIP;
	m_conf.vs.iip = m_conf.ps.iip;

	if (DATE_BARRIER)
	{
		m_conf.ps.date = 5 + m_cached_ctx.TEST.DATM;
	}
	else if (DATE_one)
	{
		const bool multidraw_fb_copy = features.multidraw_fb_copy && (m_conf.require_one_barrier || m_conf.require_full_barrier);
		if (features.texture_barrier || multidraw_fb_copy)
		{
			m_conf.require_one_barrier = true;
			m_conf.ps.date = 5 + m_cached_ctx.TEST.DATM;
		}
		m_conf.depth.date = 1;
		m_conf.depth.date_one = 1;
	}
	else if (DATE_PRIMID)
	{
		m_conf.ps.date = 1 + m_cached_ctx.TEST.DATM;
	}
	else if (DATE)
	{
		m_conf.depth.date = 1;
	}

	m_conf.ps.fba = m_context->FBA.FBA;

	if (m_conf.ps.dither || m_conf.blend_multi_pass.dither)
	{
		const GIFRegDIMX& DIMX = m_draw_env->DIMX;
		GL_DBG("DITHERING mode %d (%d)", (GSConfig.Dithering == 3) ? "Force 32bit" : ((GSConfig.Dithering == 0) ? "Disabled" : "Enabled"), GSConfig.Dithering);

		if (m_conf.ps.dither || GSConfig.Dithering == 3)
			m_conf.ps.dither = GSConfig.Dithering;

		m_conf.cb_ps.DitherMatrix[0] = GSVector4(DIMX.DM00, DIMX.DM01, DIMX.DM02, DIMX.DM03);
		m_conf.cb_ps.DitherMatrix[1] = GSVector4(DIMX.DM10, DIMX.DM11, DIMX.DM12, DIMX.DM13);
		m_conf.cb_ps.DitherMatrix[2] = GSVector4(DIMX.DM20, DIMX.DM21, DIMX.DM22, DIMX.DM23);
		m_conf.cb_ps.DitherMatrix[3] = GSVector4(DIMX.DM30, DIMX.DM31, DIMX.DM32, DIMX.DM33);
	}
	else if (GSConfig.Dithering > 2)
	{
		m_conf.ps.dither = GSConfig.Dithering;
		m_conf.blend_multi_pass.dither = GSConfig.Dithering;
	}

	if (PRIM->FGE)
	{
		m_conf.ps.fog = 1;

		const GSVector4 fc = GSVector4::rgba32(m_draw_env->FOGCOL.U32[0]);
		// Blend AREF to avoid to load a random value for alpha (dirty cache)
		m_conf.cb_ps.FogColor_AREF = fc.blend32<8>(m_conf.cb_ps.FogColor_AREF);
	}



	// Update RT scaled alpha flag, nothing's going to read it anymore.
	if (rt)
	{
		GL_INS("HW: RT alpha is now %s", rt->m_rt_alpha_scale ? "scaled" : "NOT scaled");
		rt->m_rt_alpha_scale = new_scale_rt_alpha;
		m_conf.ps.rta_correction = rt->m_rt_alpha_scale;
	}

	if (features.framebuffer_fetch)
	{
		// Intel GPUs on Metal lock up if you try to use DSB and framebuffer fetch at once
		// We should never need to do that (since using framebuffer fetch means you should be able to do all blending in shader), but sometimes it slips through
		if (m_conf.require_one_barrier || m_conf.require_full_barrier)
			pxAssert(!m_conf.blend.enable);

		// Barriers aren't needed with fbfetch.
		m_conf.require_one_barrier = false;
		m_conf.require_full_barrier = false;
	}
	// Multi-pass algorithms shouldn't be needed with full barrier and backends may not handle this correctly
	pxAssert(!m_conf.require_full_barrier || !m_conf.ps.colclip_hw);

	// Swap full barrier for one barrier when there's no overlap, or a shuffle.
	if ((features.texture_barrier || features.multidraw_fb_copy) && m_conf.require_full_barrier && (m_prim_overlap == PRIM_OVERLAP_NO || m_conf.ps.shuffle || m_channel_shuffle))
	{
		m_conf.require_full_barrier = false;
		m_conf.require_one_barrier = true;
	}
	else if (!(features.texture_barrier || features.multidraw_fb_copy))
	{
		// These shouldn't be enabled if texture barriers aren't supported, make sure they are off.
		m_conf.ps.write_rg = 0;
		m_conf.require_full_barrier = false;
	}

	// rs
	const GSVector4i hacked_scissor = m_channel_shuffle ? GSVector4i::cxpr(0, 0, 1024, 1024) : m_context->scissor.in;
	const GSVector4i scissor(GSVector4i(GSVector4(rtscale) * GSVector4(hacked_scissor)).rintersect(GSVector4i::loadh(rtsize)));

	m_conf.drawarea = m_channel_shuffle ? scissor : scissor.rintersect(ComputeBoundingBox(rtsize, rtscale));
	m_conf.scissor = (DATE && !DATE_BARRIER) ? m_conf.drawarea : scissor;

	HandleProvokingVertexFirst();

	SetupIA(rtscale, sx, sy, m_channel_shuffle_width != 0);

	if (ate_second_pass)
	{
		pxAssert(!m_conf.ps.pabe);

		std::memcpy(&m_conf.alpha_second_pass.ps, &m_conf.ps, sizeof(m_conf.ps));
		std::memcpy(&m_conf.alpha_second_pass.colormask, &m_conf.colormask, sizeof(m_conf.colormask));
		std::memcpy(&m_conf.alpha_second_pass.depth, &m_conf.depth, sizeof(m_conf.depth));

		// Not doing single pass AFAIL.
		m_conf.alpha_second_pass.ps.afail = AFAIL_KEEP;

		if (ate_RGBA_then_Z)
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
		switch (afail_type)
		{
			case AFAIL_KEEP: z = r = g = b = a = false; break; // none
			case AFAIL_FB_ONLY: z = false; break; // rgba
			case AFAIL_ZB_ONLY: r = g = b = a = false; break; // z
			case AFAIL_RGB_ONLY: z = a = false; break; // rgb
			default: ASSUME(0);
		}

		// Depth test should be disabled when depth writes are masked and similarly, Alpha test must be disabled
		// when writes to all of the alpha bits in the Framebuffer are masked.
		if (ate_RGBA_then_Z)
		{
			z = !m_cached_ctx.ZBUF.ZMSK;
			r = g = b = a = false;
		}

		m_conf.alpha_second_pass.enable = true;

		if (z || r || g || b || a)
		{
			m_conf.alpha_second_pass.depth.zwe = z;
			m_conf.alpha_second_pass.colormask.wr = r;
			m_conf.alpha_second_pass.colormask.wg = g;
			m_conf.alpha_second_pass.colormask.wb = b;
			m_conf.alpha_second_pass.colormask.wa = a;
			if (m_conf.alpha_second_pass.colormask.wrgba == 0)
			{
				m_conf.alpha_second_pass.ps.DisableColorOutput();
			}
			if (m_conf.alpha_second_pass.ps.IsFeedbackLoop())
			{
				m_conf.alpha_second_pass.require_one_barrier = m_conf.require_one_barrier;
				m_conf.alpha_second_pass.require_full_barrier = m_conf.require_full_barrier;
			}
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
		std::memcpy(&m_conf.ps, &m_conf.alpha_second_pass.ps, sizeof(m_conf.ps));
		std::memcpy(&m_conf.colormask, &m_conf.alpha_second_pass.colormask, sizeof(m_conf.colormask));
		std::memcpy(&m_conf.depth, &m_conf.alpha_second_pass.depth, sizeof(m_conf.depth));
		m_conf.cb_ps.FogColor_AREF.a = m_conf.alpha_second_pass.ps_aref;
		m_conf.alpha_second_pass.enable = false;
	}

	if (m_conf.require_full_barrier && (g_gs_device->Features().texture_barrier || g_gs_device->Features().multidraw_fb_copy))
	{
		ComputeDrawlistGetSize(rt->m_scale);
		m_conf.drawlist = &m_drawlist;
		m_conf.drawlist_bbox = &m_drawlist_bbox;
	}

	if (!m_channel_shuffle_width)
		g_gs_device->RenderHW(m_conf);
	else
		m_last_rt = rt;
}

// If the EE uploaded a new CLUT since the last draw, use that.
bool GSRendererHW::HasEEUpload(GSVector4i r)
{
	for (auto iter = m_draw_transfers.begin(); iter != m_draw_transfers.end(); ++iter)
	{
		if (iter->draw == (s_n - 1) && iter->blit.DBP == m_cached_ctx.TEX0.TBP0 && GSUtil::HasSharedBits(iter->blit.DPSM, m_cached_ctx.TEX0.PSM))
		{
			GSVector4i rect = r;

			if (!GSUtil::HasCompatibleBits(iter->blit.DPSM, m_cached_ctx.TEX0.PSM))
			{
				GSTextureCache::SurfaceOffsetKey sok;
				sok.elems[0].bp = iter->blit.DBP;
				sok.elems[0].bw = iter->blit.DBW;
				sok.elems[0].psm = iter->blit.DPSM;
				sok.elems[0].rect = iter->rect;
				sok.elems[1].bp = m_cached_ctx.TEX0.TBP0;
				sok.elems[1].bw = m_cached_ctx.TEX0.TBW;
				sok.elems[1].psm = m_cached_ctx.TEX0.PSM;
				sok.elems[1].rect = r;

				rect = g_texture_cache->ComputeSurfaceOffset(sok).b2a_offset;
			}
			if (rect.rintersect(r).eq(r))
				return true;
		}
	}
	return false;
}

GSRendererHW::CLUTDrawTestResult GSRendererHW::PossibleCLUTDraw()
{
	// No shuffles.
	if (m_channel_shuffle || m_texture_shuffle)
		return CLUTDrawTestResult::NotCLUTDraw;

	// Keep the draws simple, no alpha testing, blending, mipmapping, Z writes, and make sure it's flat.
	const bool fb_only = m_cached_ctx.TEST.ATE && m_cached_ctx.TEST.GetAFAIL(m_cached_ctx.FRAME.PSM) == AFAIL_FB_ONLY && m_cached_ctx.TEST.ATST == ATST_NEVER;

	// No Z writes, unless it's points, then it's quite likely to be a palette and they left it on.
	if (!m_cached_ctx.ZBUF.ZMSK && !fb_only && !(m_vt.m_primclass == GS_POINT_CLASS))
		return CLUTDrawTestResult::NotCLUTDraw;

	// Make sure it's flat.
	if (m_vt.m_eq.z != 0x1)
		return CLUTDrawTestResult::NotCLUTDraw;

	// No mipmapping, please never be any mipmapping...
	if (m_context->TEX1.MXL)
		return CLUTDrawTestResult::NotCLUTDraw;

	// Writing to the framebuffer for output. We're not interested. - Note: This stops NFS HP2 Busted screens working, but they're glitchy anyway
	// what NFS HP2 really needs is a kind of shuffle with mask, 32bit target is interpreted as 16bit and masked.
	if ((m_regs->DISP[0].DISPFB.Block() == m_cached_ctx.FRAME.Block()) || (m_regs->DISP[1].DISPFB.Block() == m_cached_ctx.FRAME.Block()) ||
		(m_process_texture && ((m_regs->DISP[0].DISPFB.Block() == m_cached_ctx.TEX0.TBP0) || (m_regs->DISP[1].DISPFB.Block() == m_cached_ctx.TEX0.TBP0)) && !(m_mem.m_clut.IsInvalid() & 2)))
		return CLUTDrawTestResult::NotCLUTDraw;

	// Ignore large render targets, make sure it's staying in page width.
	if (m_process_texture && (m_cached_ctx.FRAME.FBW != 1 && m_cached_ctx.TEX0.TBW == m_cached_ctx.FRAME.FBW))
		return CLUTDrawTestResult::NotCLUTDraw;

	// Hopefully no games draw a CLUT with a CLUT, that would be evil, most likely a channel shuffle.
	if (m_process_texture && GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].pal > 0)
		return CLUTDrawTestResult::NotCLUTDraw;

	const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM];

	// Make sure the CLUT formats are matching.
	if (GSLocalMemory::m_psm[m_mem.m_clut.GetCLUTCPSM()].bpp != psm.bpp)
		return CLUTDrawTestResult::NotCLUTDraw;

	// Max size for a CLUT/Current page size.
	constexpr float min_clut_width = 7.0f;
	constexpr float min_clut_height = 1.0f;
	const float page_width = static_cast<float>(psm.pgs.x);
	const float page_height = static_cast<float>(psm.pgs.y);

	// If the coordinates aren't starting within the page, it's likely not a CLUT draw.
	if (floor(m_vt.m_min.p.x) < 0 || floor(m_vt.m_min.p.y) < 0 || floor(m_vt.m_min.p.x) > page_width || floor(m_vt.m_min.p.y) > page_height)
		return CLUTDrawTestResult::NotCLUTDraw;

	// Make sure it's a division of 8 in width to avoid bad draws. Points will go from 0-7 inclusive, but sprites etc will do 0-16 exclusive.
	int draw_divder_match = false;
	const int valid_sizes[] = {8, 16, 32, 64};

	for (int i = 0; i < 4; i++)
	{
		draw_divder_match = ((m_vt.m_primclass == GS_POINT_CLASS) ? ((static_cast<int>(m_vt.m_max.p.x + 1) & ~1) == valid_sizes[i]) : (static_cast<int>(m_vt.m_max.p.x) == valid_sizes[i]));

		if (draw_divder_match)
			break;
	}
	// Make sure it's kinda CLUT sized, at least. Be wary, it can draw a line at a time (Guitar Hero - Metallica)
	// Driver Parallel Lines draws a bunch of CLUT's at once, ending up as a 64x256 draw, very annoying.
	const float draw_width = (m_vt.m_max.p.x - m_vt.m_min.p.x);
	const float draw_height = (m_vt.m_max.p.y - m_vt.m_min.p.y);
	const bool valid_size = ((draw_width >= min_clut_width || draw_height >= min_clut_height))
		&& (((draw_width < page_width && draw_height <= page_height) || (draw_width == page_width)) && draw_divder_match); // Make sure draw is multiples of 8 wide (AC5 midetection).
	
	// Make sure the draw hits the next CLUT and it's marked as invalid (kind of a sanity check).
	// We can also allow draws which are of a sensible size within the page, as they could also be CLUT draws (or gradients for the CLUT).
	if (!valid_size)
		return CLUTDrawTestResult::NotCLUTDraw;

	if (m_process_texture)
	{
		// If we're using a texture to draw our CLUT/whatever, we need the GPU to write back dirty data we need.
		const GSVector4i r = GetTextureMinMax(m_cached_ctx.TEX0, m_cached_ctx.CLAMP, m_vt.IsLinear(), false).coverage;

		// If we have GPU CLUT enabled, don't do a CPU draw when it would result in a download.
		if (GSConfig.UserHacks_GPUTargetCLUTMode != GSGPUTargetCLUTMode::Disabled)
		{
			if (HasEEUpload(r))
				return CLUTDrawTestResult::CLUTDrawOnCPU;

			GSTextureCache::Target* tgt = g_texture_cache->FindOverlappingTarget(
				m_cached_ctx.TEX0.TBP0, m_cached_ctx.TEX0.TBW, m_cached_ctx.TEX0.PSM, r);
			if (tgt)
			{
				tgt->UnscaleRTAlpha();
				bool is_dirty = false;
				for (const GSDirtyRect& rc : tgt->m_dirty)
				{
					if (!rc.GetDirtyRect(m_cached_ctx.TEX0, false).rintersect(r).rempty())
					{
						is_dirty = true;
						break;
					}
				}
				if (!is_dirty)
				{
					GL_INS("HW: GPU clut is enabled and this draw would readback, leaving on GPU");
					return CLUTDrawTestResult::CLUTDrawOnGPU;
				}
			}
		}
		else
		{
			if (HasEEUpload(r))
				return CLUTDrawTestResult::CLUTDrawOnCPU;
		}

		GIFRegBITBLTBUF BITBLTBUF = {};
		BITBLTBUF.SBP = m_cached_ctx.TEX0.TBP0;
		BITBLTBUF.SBW = m_cached_ctx.TEX0.TBW;
		BITBLTBUF.SPSM = m_cached_ctx.TEX0.PSM;

		InvalidateLocalMem(BITBLTBUF, r);
	}
	// Debugging stuff..
	//const u32 startbp = psm.info.bn(m_vt.m_min.p.x, m_vt.m_min.p.y, m_FRAME.Block(), m_FRAME.FBW);
	//const u32 endbp = psm.info.bn(m_vt.m_max.p.x, m_vt.m_max.p.y, m_FRAME.Block(), m_FRAME.FBW);
	//DevCon.Warning("HW: Draw width %f height %f page width %f height %f TPSM %x TBP0 %x FPSM %x FBP %x CBP %x valid size %d Invalid %d DISPFB0 %x DISPFB1 %x start %x end %x draw %d", draw_width, draw_height, page_width, page_height, m_cached_ctx.TEX0.PSM, m_cached_ctx.TEX0.TBP0, m_FRAME.PSM, m_FRAME.Block(), m_mem.m_clut.GetCLUTCBP(), valid_size, m_mem.m_clut.IsInvalid(), m_regs->DISP[0].DISPFB.Block(), m_regs->DISP[1].DISPFB.Block(), startbp, endbp, s_n);

	return CLUTDrawTestResult::CLUTDrawOnCPU;
}

// Slight more aggressive version that kinda YOLO's it if the draw is anywhere near the CLUT or is point/line (providing it's not too wide of a draw and a few other parameters.
// This is pretty much tuned for the Sega Model 2 games, which draw a huge gradient, then pick lines out of it to make up CLUT's for about 4000 draws...
GSRendererHW::CLUTDrawTestResult GSRendererHW::PossibleCLUTDrawAggressive()
{
	// Avoid any shuffles.
	if (m_channel_shuffle || m_texture_shuffle)
		return CLUTDrawTestResult::NotCLUTDraw;

	// Keep the draws simple, no alpha testing, blending, mipmapping, Z writes, and make sure it's flat.
	if (m_cached_ctx.TEST.ATE)
		return CLUTDrawTestResult::NotCLUTDraw;

	if (NeedsBlending())
		return CLUTDrawTestResult::NotCLUTDraw;

	if (m_context->TEX1.MXL)
		return CLUTDrawTestResult::NotCLUTDraw;

	if (m_cached_ctx.FRAME.FBW != 1)
		return CLUTDrawTestResult::NotCLUTDraw;

	if (!m_cached_ctx.ZBUF.ZMSK)
		return CLUTDrawTestResult::NotCLUTDraw;

	if (m_vt.m_eq.z != 0x1)
		return CLUTDrawTestResult::NotCLUTDraw;

	if (!((m_vt.m_primclass == GS_POINT_CLASS || m_vt.m_primclass == GS_LINE_CLASS) || ((m_mem.m_clut.GetCLUTCBP() >> 5) >= m_cached_ctx.FRAME.FBP && (m_cached_ctx.FRAME.FBP + 1U) >= (m_mem.m_clut.GetCLUTCBP() >> 5) && m_vt.m_primclass == GS_SPRITE_CLASS)))
		return CLUTDrawTestResult::NotCLUTDraw;

	// Avoid invalidating anything here, we just want to avoid the thing being drawn on the GPU.
	return CLUTDrawTestResult::CLUTDrawOnCPU;
}

bool GSRendererHW::CanUseSwPrimRender(bool no_rt, bool no_ds, bool draw_sprite_tex)
{
	// Master enable.
	const int bw = GSConfig.UserHacks_CPUSpriteRenderBW;
	const int level = GSConfig.UserHacks_CPUSpriteRenderLevel;

	if (bw == 0)
		return false;

	// We don't ever want to do this when we have a depth buffer, and only for textured sprites.
	if (no_rt || !no_ds || (level == 0 && !draw_sprite_tex))
		return false;

	// Check the size threshold. Spider-man 2 uses a FBW of 32 for some silly reason...
	if (m_cached_ctx.FRAME.FBW > static_cast<u32>(bw) && m_cached_ctx.FRAME.FBW != 32)
		return false;

	// We shouldn't be using mipmapping, and this shouldn't be a blended draw.
	if (level < 2 && (IsMipMapActive() || !IsOpaque()))
		return false;

	// Middle of a shuffle, don't SW it.
	if (m_split_texture_shuffle_pages)
		return false;

	// Make sure this isn't something we've actually rendered to (e.g. a texture shuffle).
	if (PRIM->TME)
	{
		GSTextureCache::Target* src_target = g_texture_cache->GetTargetWithSharedBits(m_cached_ctx.TEX0.TBP0, m_cached_ctx.TEX0.PSM);
		if (src_target)
		{
			// If we also have the destination target and it's all valid data and it needs blending, then we can't SW render it as we will nuke valid information.
			if (!IsOpaque())
			{
				GSTextureCache::Target* dst_target = g_texture_cache->GetTargetWithSharedBits(m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.PSM);

				if (dst_target && dst_target->m_dirty.empty() && ((!(GSUtil::GetChannelMask(m_cached_ctx.FRAME.PSM) & 0x7)) || dst_target->m_valid_rgb) &&
					((!(GSUtil::GetChannelMask(m_cached_ctx.FRAME.PSM) & 0x8)) || (dst_target->m_valid_alpha_low && dst_target->m_valid_alpha_high)))
					return false;
			}

			const bool need_aem_color = GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].trbpp <= 24 && GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].pal == 0 && ((NeedsBlending() && m_context->ALPHA.C == 0) || IsDiscardingDstAlpha()) && m_cached_ctx.TEXA.AEM;
			const u32 color_mask = (m_vt.m_max.c > GSVector4i::zero()).mask();
			const bool texture_function_color = m_cached_ctx.TEX0.TFX == TFX_DECAL || (color_mask & 0xFFF) || (m_cached_ctx.TEX0.TFX > TFX_DECAL && (color_mask & 0xF000));
			const bool texture_function_alpha = m_cached_ctx.TEX0.TFX != TFX_MODULATE || (color_mask & 0xF000);
			const u32 fm_mask = GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].fmsk;
			const bool req_color = (texture_function_color && (!PRIM->ABE || GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].bpp < 16 || (NeedsBlending() && IsUsingCsInBlend())) && (m_cached_ctx.FRAME.FBMSK & (fm_mask & 0x00FFFFFF)) != (fm_mask & 0x00FFFFFF)) || need_aem_color;
			const bool alpha_used = (GSUtil::GetChannelMask(m_context->TEX0.PSM) == 0x8 || (m_context->TEX0.TCC && texture_function_alpha)) && ((NeedsBlending() && IsUsingAsInBlend()) || (m_cached_ctx.TEST.ATE && m_cached_ctx.TEST.ATST > ATST_ALWAYS) || (m_cached_ctx.FRAME.FBMSK & (fm_mask & 0xFF000000)) != (fm_mask & 0xFF000000));
			const bool req_alpha = (GSUtil::GetChannelMask(m_context->TEX0.PSM) & 0x8) && alpha_used;

			if ((req_color && !src_target->m_valid_rgb) || (req_alpha && (!src_target->m_valid_alpha_low || !src_target->m_valid_alpha_high)))
				return true;

			bool req_readback = false;
			// If the EE has written over our sample area, we're fine to do this on the CPU, despite the target.
			if (!src_target->m_dirty.empty())
			{
				const GSVector4i tr(GetTextureMinMax(m_cached_ctx.TEX0, m_cached_ctx.CLAMP, m_vt.IsLinear(), false).coverage);

				const u32 start_bp = GSLocalMemory::GetStartBlockAddress(m_cached_ctx.TEX0.TBP0, m_cached_ctx.TEX0.TBW, m_cached_ctx.TEX0.PSM, tr);
				const u32 end_bp = GSLocalMemory::GetEndBlockAddress(m_cached_ctx.TEX0.TBP0, m_cached_ctx.TEX0.TBW, m_cached_ctx.TEX0.PSM, tr);

				for (GSDirtyRect& rc : src_target->m_dirty)
				{
					const GSVector4i dirty_rect = rc.GetDirtyRect(src_target->m_TEX0, false);
					const u32 dirty_start_bp = GSLocalMemory::GetStartBlockAddress(src_target->m_TEX0.TBP0, src_target->m_TEX0.TBW, src_target->m_TEX0.PSM, dirty_rect);
					const u32 dirty_end_bp = GSLocalMemory::GetEndBlockAddress(src_target->m_TEX0.TBP0, src_target->m_TEX0.TBW, src_target->m_TEX0.PSM, dirty_rect);

					if (start_bp < dirty_end_bp && end_bp > dirty_start_bp)
					{
						if (dirty_start_bp <= start_bp && dirty_end_bp >= end_bp)
						{
							return true;
						}
						else if (GSUtil::HasSameSwizzleBits(m_cached_ctx.TEX0.PSM, src_target->m_TEX0.PSM) || m_primitive_covers_without_gaps == NoGapsType::FullCover)
							return false;
					}
				}
			}
			else
			{
				// If the target isn't dirty we might have valid data, so let's check their areas overlap, if so we need to read it back for SW.
				GSVector4i src_rect = GSVector4i(m_vt.m_min.t.x, m_vt.m_min.t.y, m_vt.m_max.t.x, m_vt.m_max.t.x);
				GSVector4i area = g_texture_cache->TranslateAlignedRectByPage(src_target, m_cached_ctx.TEX0.TBP0, m_cached_ctx.TEX0.PSM, m_cached_ctx.TEX0.TBW, src_rect, false);
				req_readback = !area.rintersect(src_target->m_drawn_since_read).eq(GSVector4i::zero());
			}
			// Make sure it actually makes sense to use this target as a source, given the formats, and it wouldn't just sample as garbage.
			// We can't rely exclusively on the dirty rect check above, because sometimes the targets are from older frames and too large.
			if (!GSUtil::HasSameSwizzleBits(m_cached_ctx.TEX0.PSM, src_target->m_TEX0.PSM) &&
				(!src_target->m_32_bits_fmt || GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].bpp != 16))
			{
				if (req_readback)
					g_texture_cache->Read(src_target, src_target->m_drawn_since_read);
				return true;
			}

			return false;
		}
	}

	if (PRIM->ABE && m_vt.m_eq.rgba == 0xffff && !m_context->ALPHA.IsOpaque(GetAlphaMinMax().min, GetAlphaMinMax().max))
	{
		GSTextureCache::Target* rt = g_texture_cache->GetTargetWithSharedBits(m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.PSM);

		if (!rt || (!rt->m_dirty.empty() && rt->m_dirty.GetTotalRect(rt->m_TEX0, rt->m_unscaled_size).rintersect(m_r).eq(m_r)))
			return true;

		rt = nullptr;
		return false;
	}

	// We can use the sw prim render path!
	return true;
}

void GSRendererHW::SetNewFRAME(u32 bp, u32 bw, u32 psm)
{
	m_cached_ctx.FRAME.FBP = bp >> 5;
	m_cached_ctx.FRAME.FBW = bw;
	m_cached_ctx.FRAME.PSM = psm;
	m_context->offset.fb = m_mem.GetOffset(bp, bw, psm);
}

void GSRendererHW::SetNewZBUF(u32 bp, u32 psm)
{
	m_cached_ctx.ZBUF.ZBP = bp >> 5;
	m_cached_ctx.ZBUF.PSM = psm;
	m_context->offset.zb = m_mem.GetOffset(bp, m_cached_ctx.FRAME.FBW, psm);
}

bool GSRendererHW::DetectStripedDoubleClear(bool& no_rt, bool& no_ds)
{
	const bool single_page_offset =
		std::abs(static_cast<int>(m_cached_ctx.FRAME.FBP) - static_cast<int>(m_cached_ctx.ZBUF.ZBP)) == 1;
	const bool z_is_frame = (m_cached_ctx.FRAME.FBP == m_cached_ctx.ZBUF.ZBP || (m_cached_ctx.FRAME.FBW > 1 && single_page_offset)) && // GT4O Public Beta
	                        !m_cached_ctx.ZBUF.ZMSK &&
	                        (m_cached_ctx.FRAME.PSM & 0x30) != (m_cached_ctx.ZBUF.PSM & 0x30) &&
	                        (m_cached_ctx.FRAME.PSM & 0xF) == (m_cached_ctx.ZBUF.PSM & 0xF) && m_vt.m_eq.z == 1 &&
	                        m_vertex.buff[1].XYZ.Z == m_vertex.buff[1].RGBAQ.U32[0];

	// Z and color must be constant and the same and must be drawing strips.
	if (!z_is_frame || m_vt.m_eq.rgba != 0xFFFF)
		return false;

	const GSVector2i page_size = GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].pgs;
	const int strip_size = ((single_page_offset) ? page_size.x : (page_size.x / 2));

	// Find the biggest gap out of all the verts, most of the time games are nice and do strips,
	// however Lord of the Rings - The Third Age draws the strips 8x8 per sprite, until it makes up 32x8, then does the next 32x8 below.
	// I know, unneccesary, but that's what they did. But this loop should calculate the largest gap, then we can confirm it.
	// LOTR has 4096 verts, so this isn't going to be super fast on that game, most games will be just 16 verts so they should be ok,
	// and I could cheat and stop when we get a size that matches, but that might be a lucky misdetection, I don't wanna risk it.
	int vertex_offset = 0;
	int last_vertex = m_vertex.buff[0].XYZ.X;

	for (u32 i = 1; i < m_vertex.tail; i++)
	{
		vertex_offset = std::max(static_cast<int>((m_vertex.buff[i].XYZ.X - last_vertex) >> 4), vertex_offset);
		last_vertex = m_vertex.buff[i].XYZ.X;

		// Found a gap which is much bigger, no point continuing to scan.
		if (vertex_offset > strip_size)
			break;
	}

	const bool is_strips = vertex_offset == strip_size;

	if (!is_strips)
		return false;

	// Half a page extra width is written through Z.
	// When the FRAME is lower or the same and including offset matches the frame width, it will be set back 64/32 pixels.
	// When the FRAME is higher, that means ZBUF is ahead behind 1 page, so the beginning will be 1 page in
	if (m_cached_ctx.FRAME.FBP < m_cached_ctx.ZBUF.ZBP || m_r.x == 0)
		m_r.z += vertex_offset;
	else
		m_r.x -= vertex_offset;

	GL_INS("HW: DetectStripedDoubleClear(): %d,%d => %d,%d @ FBP %x FBW %u ZBP %x", m_r.x, m_r.y, m_r.z, m_r.w,
		m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBW, m_cached_ctx.ZBUF.Block());

	// And replace the vertex with a fullscreen quad.
	ReplaceVerticesWithSprite(m_r, GSVector2i(1, 1));

	// Remove Z, we'll write it through colour.
	m_cached_ctx.ZBUF.ZMSK = true;
	no_rt = false;
	no_ds = true;
	return true;
}

bool GSRendererHW::DetectDoubleHalfClear(bool& no_rt, bool& no_ds)
{
	if (m_cached_ctx.TEST.ZTST != ZTST_ALWAYS || m_cached_ctx.ZBUF.ZMSK)
		return false;

	// Block when any bits are masked. Too many false positives if we don't.
	// Siren does a C32+Z24 clear with A masked, GTA:LCS does C32+Z24 but doesn't set FBMSK, leaving half
	// of the alpha channel untouched (no effect because it uses Z24 elsewhere).
	const GSLocalMemory::psm_t& frame_psm = GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM];
	const GSLocalMemory::psm_t& zbuf_psm = GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM];
	if (((m_cached_ctx.FRAME.FBMSK & frame_psm.fmsk) != 0 && (m_cached_ctx.FRAME.FBMSK & zbuf_psm.fmsk) != 0))
	{
		if ((m_cached_ctx.FRAME.FBMSK & frame_psm.fmsk) == (0xFF000000 & frame_psm.fmsk))
		{
			// Alpha is masked, if the alpha is black anyways, and Z is writing to alpha, then just allow it. Tony Hawk Pro Skater 4 doesn't use the alpha channel but does this messy double clear.
			if (frame_psm.trbpp == 32 && zbuf_psm.trbpp == 32 && m_vt.m_max.c.a == 0 && m_vt.m_max.p.z < 0x1000000)
			{
				GSTextureCache::Target* frame = g_texture_cache->GetExactTarget(m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBW, GSTextureCache::RenderTarget, m_cached_ctx.FRAME.Block());

				if (frame && frame->m_alpha_max > 0)
					return false;
			}
			else
				return false;
		}
		else
			return false;
	}

	// Z and color must be constant and the same and both are enabled.
	if (m_vt.m_eq.rgba != 0xFFFF || !m_vt.m_eq.z || (no_ds != no_rt))
		return false;

	const u32 write_color = GetConstantDirectWriteMemClearColor();
	const u32 write_depth = GetConstantDirectWriteMemClearDepth();
	if (write_color != write_depth)
		return false;

	// Frame and depth pointer can be inverted
	const bool clear_depth = (m_cached_ctx.FRAME.FBP > m_cached_ctx.ZBUF.ZBP);
	const u32 base = clear_depth ? m_cached_ctx.ZBUF.ZBP : m_cached_ctx.FRAME.FBP;
	const u32 half = clear_depth ? m_cached_ctx.FRAME.FBP : m_cached_ctx.ZBUF.ZBP;
	const bool enough_bits = (frame_psm.trbpp == zbuf_psm.trbpp);

	// Size of the current draw
	const u32 w_pages = (m_r.z + (frame_psm.pgs.x - 1)) / frame_psm.pgs.x;
	const u32 h_pages = (m_r.w + (frame_psm.pgs.y - 1)) / frame_psm.pgs.y;
	const u32 written_pages = w_pages * h_pages;

	// If both buffers are side by side we can expect a fast clear in on-going
	if (half > (base + written_pages) || half <= base)
		return false;

	// CoD: World at War draws a shadow map with RGB masked at the same page as the depth buffer, which is
	// double-half cleared. For testing, ignore any targets that don't have the bits we're drawing to.
	const bool req_valid_alpha = ((frame_psm.fmsk & zbuf_psm.fmsk) & 0xFF000000u) != 0;
	GSTextureCache::Target* half_point = g_texture_cache->GetExactTarget(half << 5, m_cached_ctx.FRAME.FBW, clear_depth ? GSTextureCache::RenderTarget : GSTextureCache::DepthStencil, half << 5);
	half_point = (half_point && half_point->m_valid_rgb && half_point->HasValidAlpha() == req_valid_alpha) ? half_point : nullptr;
	if (half_point && half_point->m_age <= 1)
		return false;

	// Don't allow double half clear to go through when the number of bits written through FRAME and Z are different.
	// GTA: LCS does this setup, along with a few other games. Thankfully if it's a zero clear, we'll clear both
	// separately, and the end result is the same because it gets invalidated. That's better than falsely detecting
	// double half clears, and ending up with 1024 high render targets which really shouldn't be.
	if ((!enough_bits && frame_psm.fmt != zbuf_psm.fmt && m_cached_ctx.FRAME.FBMSK != ((zbuf_psm.fmt == 1) ? 0xFF000000u : 0)) ||
		!GSUtil::HasCompatibleBits(m_cached_ctx.FRAME.PSM & ~0x30, m_cached_ctx.ZBUF.PSM & ~0x30)) // Bit depth is not the same (i.e. 32bit + 16bit).
	{
		GL_INS("HW: DetectDoubleHalfClear(): Inconsistent FRAME [%s, %08x] and ZBUF [%s] formats, not using double-half clear.",
			GSUtil::GetPSMName(m_cached_ctx.FRAME.PSM), m_cached_ctx.FRAME.FBMSK, GSUtil::GetPSMName(m_cached_ctx.ZBUF.PSM));

		// Spiderman: Web of Shadows clears its depth buffer with 32-bit FRAME and 24-bit Z. So the upper 8 bits of half
		// the depth buffer are not cleared, yay. We can't turn this into a 32-bit clear, because then it'll zero out
		// those bits, which other games need (e.g. Jak 2). We can't do a 24-bit clear, because something might rely
		// on half those bits actually getting zeroed. So, instead, we toss the depth buffer, and let the mem clear
		// path write out FRAME and Z separately, with their associated masks. Limit it to black to avoid false positives.
		if (write_color == 0)
		{
			const GSTextureCache::Target* base_tgt = g_texture_cache->GetExactTarget(base * GS_BLOCKS_PER_PAGE,
				m_cached_ctx.FRAME.FBW, clear_depth ? GSTextureCache::DepthStencil : GSTextureCache::RenderTarget,
				GSLocalMemory::GetEndBlockAddress(half * GS_BLOCKS_PER_PAGE, m_cached_ctx.FRAME.FBW, m_cached_ctx.FRAME.PSM, m_r));
			if (base_tgt)
			{
				GL_INS("HW: DetectDoubleHalfClear(): Invalidating targets at 0x%x/0x%x due to different formats, and clear to black.",
					base * GS_BLOCKS_PER_PAGE, half * GS_BLOCKS_PER_PAGE);
				g_texture_cache->InvalidateVideoMemType(GSTextureCache::RenderTarget, base * GS_BLOCKS_PER_PAGE);
				g_texture_cache->InvalidateVideoMemType(GSTextureCache::RenderTarget, half * GS_BLOCKS_PER_PAGE);
				g_texture_cache->InvalidateVideoMemType(GSTextureCache::DepthStencil, base * GS_BLOCKS_PER_PAGE);
				g_texture_cache->InvalidateVideoMemType(GSTextureCache::DepthStencil, half * GS_BLOCKS_PER_PAGE);
			}
		}

		return false;
	}

	// Shortcut, if it's clearing Z then the clut overlap is not reading Z.
	if (m_state_flush_reason == CLUTCHANGE && clear_depth)
		return false;

	const int next_ctx = (m_state_flush_reason == CONTEXTCHANGE) ? m_env.PRIM.CTXT : (1 - m_env.PRIM.CTXT);

	// This is likely a full screen, can only really tell if this frame is used in the next draw, and we need to check if the height fills the next scissor.
	if (m_env.CTXT[next_ctx].FRAME.FBP == m_cached_ctx.FRAME.FBP && m_env.CTXT[next_ctx].FRAME.FBW == m_cached_ctx.FRAME.FBW && m_r.width() == static_cast<int>(m_cached_ctx.FRAME.FBW * 64) && m_r.height() >= static_cast<int>(m_env.CTXT[next_ctx].SCISSOR.SCAY1 + 1))
		return false;

	// Try peeking ahead to confirm whether this is a "normal" clear, where the two buffers just happen to be
	// bang up next to each other, or a double half clear. The two are really difficult to differentiate.
	// Have to check both contexts, because God of War 2 likes to do this in-between setting TRXDIR, which
	// causes a flush, and we don't have the next context backed up index set.
	bool horizontal = std::abs(static_cast<int>(m_cached_ctx.FRAME.FBP) - static_cast<int>(m_cached_ctx.ZBUF.ZBP)) == (m_cached_ctx.FRAME.FBW >> 1);
	const bool possible_next_clear = !m_env.PRIM.TME && !(m_env.SCANMSK.MSK & 2) && !m_env.CTXT[next_ctx].TEST.ATE && !m_env.CTXT[next_ctx].TEST.DATE &&
		(!m_env.CTXT[next_ctx].TEST.ZTE || m_env.CTXT[next_ctx].TEST.ZTST == ZTST_ALWAYS);

	const bool next_draw_match = m_env.CTXT[next_ctx].FRAME.FBP == m_cached_ctx.FRAME.FBP && m_env.CTXT[next_ctx].ZBUF.ZBP == m_cached_ctx.ZBUF.ZBP;

	// Match either because we got here early or the information is the same on the next draw and the next draw is not a clear.
	// Likely a misdetection.
	if (next_draw_match && !possible_next_clear)
	{
		return false;
	}
	else
	{
		// Check for a target matching the starting point. It might be in Z or FRAME...
		GSTextureCache::Target* tgt = g_texture_cache->GetTargetWithSharedBits(
			base * GS_BLOCKS_PER_PAGE, clear_depth ? m_cached_ctx.ZBUF.PSM : m_cached_ctx.FRAME.PSM);
		tgt = (tgt && tgt->m_valid_rgb && tgt->HasValidAlpha() == req_valid_alpha) ? tgt : nullptr;
		if (!tgt)
		{
			tgt = g_texture_cache->GetTargetWithSharedBits(
				base * GS_BLOCKS_PER_PAGE, clear_depth ? m_cached_ctx.FRAME.PSM : m_cached_ctx.ZBUF.PSM);
			tgt = (tgt && tgt->m_valid_rgb && tgt->HasValidAlpha() == req_valid_alpha) ? tgt : nullptr;
		}

		u32 end_block = ((half + written_pages) * GS_BLOCKS_PER_PAGE) - 1;

		if (tgt && tgt->m_age <= 1)
		{
			// Games generally write full pages when doing half clears, so if the half of the buffer doesn't match, we need to round it up to the page edge.
			// Dropship does this with half buffers of 128 high (32 * 4) when the final buffer is only actually 224 high (112 is half, centre of a page).
			GSVector4i target_rect = tgt->GetUnscaledRect();
			if ((target_rect.w / 2) & (frame_psm.pgs.y - 1))
			{
				target_rect.w = ((target_rect.w / 2) + (frame_psm.pgs.y - 1)) & ~(frame_psm.pgs.y - 1);
				target_rect.w *= 2;
			}
			// If the full size is an odd width and it's trying to do half (in the case of FF7 DoC it goes from 7 to 4), we need to recalculate our end check.
			if ((m_cached_ctx.FRAME.FBW * 2) == (tgt->m_TEX0.TBW + 1))
				end_block = GSLocalMemory::GetUnwrappedEndBlockAddress(tgt->m_TEX0.TBP0, tgt->m_TEX0.TBW + 1, tgt->m_TEX0.PSM, target_rect);
			else
				end_block = GSLocalMemory::GetUnwrappedEndBlockAddress(tgt->m_TEX0.TBP0, (m_cached_ctx.FRAME.FBW == (tgt->m_TEX0.TBW / 2)) ? tgt->m_TEX0.TBW : m_cached_ctx.FRAME.FBW, tgt->m_TEX0.PSM, target_rect);

			// Siren double half clears horizontally with half FBW instead of vertically.
			// We could use the FBW here, but using the rectangle seems a bit safer, because changing FBW
			// from one RT to another isn't uncommon.
			const GSVector4 vr = GSVector4(m_r.rintersect(tgt->m_valid)) / GSVector4(tgt->m_valid);
			horizontal = (vr.z < vr.w);
		}
		else if (((m_env.CTXT[next_ctx].FRAME.FBW + 1) & ~1) == m_cached_ctx.FRAME.FBW * 2)
		{
			horizontal = true;
		}

		// Are we clearing over the middle of this target?
		if ((((half + written_pages) * GS_BLOCKS_PER_PAGE) - 1) > end_block)
		{
			return false;
		}
	}

	GL_INS("HW: DetectDoubleHalfClear(): Clearing %s %s, fbp=%x, zbp=%x, pages=%u, base=%x, half=%x, rect=(%d,%d=>%d,%d)",
		clear_depth ? "depth" : "color", horizontal ? "horizontally" : "vertically", m_cached_ctx.FRAME.Block(),
		m_cached_ctx.ZBUF.Block(), written_pages, base * GS_BLOCKS_PER_PAGE, half * GS_BLOCKS_PER_PAGE, m_r.x, m_r.y, m_r.z,
		m_r.w);

	// Double the clear rect.
	if (horizontal)
	{
		const int width = m_r.width();
		m_r.z = (w_pages * frame_psm.pgs.x);
		m_r.z += m_r.x + width;

		const u32 new_w_pages = (m_r.z + 63) / 64;
		if (new_w_pages > m_cached_ctx.FRAME.FBW)
		{
			GL_INS("HW: DetectDoubleHalfClear(): Doubling FBW because %u pages wide is less than FBW %u", new_w_pages, m_cached_ctx.FRAME.FBW);
			m_cached_ctx.FRAME.FBW *= 2;
		}
	}
	else
	{
		const int height = m_r.height();

		// We don't want to double half clear already full sized targets, making them double the size, this could be very bad.
		// This gets triggered by Monster Lab which clears the Z and FRAME in one go, butted up against each other.
		// It's highly unlikely that it will actually require a > 600 high framebuffer, but check with the display height first.
		const int display_height = PCRTCDisplays.GetResolution().y;
		if ((display_height != 0 && height >= (display_height - 1)) || height > 300)
			return false;

		m_r.w = ((half - base) / m_cached_ctx.FRAME.FBW) * frame_psm.pgs.y;
		m_r.w += m_r.y + height;
	}
	ReplaceVerticesWithSprite(m_r, GSVector2i(1, 1));

	// Prevent wasting time looking up and creating the target which is getting blown away.
	if (frame_psm.trbpp >= zbuf_psm.trbpp)
	{
		SetNewFRAME(base * GS_BLOCKS_PER_PAGE, m_cached_ctx.FRAME.FBW, m_cached_ctx.FRAME.PSM);
		m_cached_ctx.ZBUF.ZMSK = true;
		no_rt = false;
		no_ds = true;
	}
	else
	{
		SetNewZBUF(base * GS_BLOCKS_PER_PAGE, m_cached_ctx.ZBUF.PSM);
		m_cached_ctx.FRAME.FBMSK = 0xFFFFFFFF;
		no_rt = true;
		no_ds = false;
	}

	// Remove any targets at the half-buffer point, they're getting overwritten.
	g_texture_cache->InvalidateVideoMemType(GSTextureCache::RenderTarget, half * GS_BLOCKS_PER_PAGE);
	g_texture_cache->InvalidateVideoMemType(GSTextureCache::DepthStencil, half * GS_BLOCKS_PER_PAGE);
	return true;
}

bool GSRendererHW::DetectRedundantBufferClear(bool& no_rt, bool& no_ds, u32 fm_mask)
{
	// This function handles the case where the game points FRAME and ZBP at the same page, and both FRAME and Z
	// write the same bits. A few games do this, including Flatout 2, DMC3, Ratchet & Clank, Gundam, and Superman.
	if (m_cached_ctx.FRAME.FBP != m_cached_ctx.ZBUF.ZBP || m_cached_ctx.ZBUF.ZMSK)
		return false;

	// Frame and Z aren't writing any overlapping bits.
	// We can't check for exactly the same bitmask, because some games do C32 FRAME with Z24, and no FBMSK.
	if (((~m_cached_ctx.FRAME.FBMSK & fm_mask) & GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].fmsk) == 0)
		return false;

	// Make sure the width is page aligned, so we don't break powerdrome-style clears where Z writes the right side of the page.
	// We can't check page alignment on the size entirely, because Ratchet does 256x127 clears...
	// Test cases: Devil May Cry 3, Tom & Jerry.
	if ((m_r.x & 63) != 0 || (m_r.z & 63) != 0)
		return false;

	// Compute how many bits are actually written through FRAME. Normally we'd use popcnt, but we still have to
	// support SSE4.1. If we somehow don't have a contiguous FBMSK, we're in trouble anyway...
	const u32 frame_bits_written = 32 - std::countl_zero(~m_cached_ctx.FRAME.FBMSK & fm_mask);

	// Keep Z if we have a target at this location already, or if Z is writing more bits than FRAME.
	const u32 z_bits_written = GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].trbpp;
	const GSTextureCache::Target* ztgt = g_texture_cache->GetTargetWithSharedBits(m_cached_ctx.ZBUF.Block(), m_cached_ctx.ZBUF.PSM);
	const bool keep_z = (ztgt && ztgt->m_valid_rgb && z_bits_written >= frame_bits_written) || (z_bits_written > frame_bits_written);
	GL_INS("HW: FRAME and ZBUF writing page-aligned same data, discarding %s", keep_z ? "FRAME" : "ZBUF");
	if (keep_z)
	{
		m_cached_ctx.FRAME.FBMSK = 0xFFFFFFFFu;
		no_rt = true;
		no_ds = false;
	}
	else
	{
		m_cached_ctx.ZBUF.ZMSK = true;
		no_ds = true;
		no_rt = false;
	}

	return true;
}

bool GSRendererHW::TryTargetClear(GSTextureCache::Target* rt, GSTextureCache::Target* ds, bool preserve_rt_color, bool preserve_depth)
{
	if (m_vt.m_eq.rgba != 0xFFFF || !m_vt.m_eq.z)
		return false;

	bool skip = true;
	if (rt)
	{
		if (!preserve_rt_color && !IsReallyDithered() && m_r.rintersect(rt->m_valid).eq(rt->m_valid))
		{
			const u32 c = GetConstantDirectWriteMemClearColor();
			u32 clear_c = c;
			const bool has_alpha = GSLocalMemory::m_psm[rt->m_TEX0.PSM].trbpp != 24;
			const bool alpha_one_or_less = has_alpha && (c >> 24) <= 0x80;
			GL_INS("HW: TryTargetClear(): RT at %x <= %08X", rt->m_TEX0.TBP0, c);

			if (rt->m_rt_alpha_scale || alpha_one_or_less)
			{
				if (alpha_one_or_less)
				{
					const u32 new_alpha = std::min((c >> 24) * 2U, 255U);
					clear_c = (clear_c & 0xFFFFFF) | (new_alpha << 24);
					rt->m_rt_alpha_scale = true;
				}
				else
				{
					rt->m_rt_alpha_scale = false;
				}
			}

			g_gs_device->ClearRenderTarget(rt->m_texture, clear_c);
			rt->m_dirty.clear();

			if (has_alpha)
			{
				rt->m_alpha_max = c >> 24;
				rt->m_alpha_min = c >> 24;
			}

			if (!rt->m_32_bits_fmt)
			{
				rt->m_alpha_max &= 128;
				rt->m_alpha_min &= 128;
			}
			rt->m_alpha_range = false;
		}
		else
		{
			skip = false;
		}
	}

	if (ds)
	{
		if (ds && !preserve_depth && m_r.rintersect(ds->m_valid).eq(ds->m_valid))
		{
			const u32 max_z = 0xFFFFFFFF >> (GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].fmt * 8);
			const u32 z = std::min(max_z, m_vertex.buff[1].XYZ.Z);
			const float d = static_cast<float>(z) * 0x1p-32f;
			GL_INS("HW: TryTargetClear(): DS at %x <= %f", ds->m_TEX0.TBP0, d);
			g_gs_device->ClearDepth(ds->m_texture, d);
			ds->m_dirty.clear();
			ds->m_alpha_max = z >> 24;
			ds->m_alpha_min = z >> 24;

			if (GSLocalMemory::m_psm[ds->m_TEX0.PSM].bpp == 16)
			{
				ds->m_alpha_max &= 128;
				ds->m_alpha_min &= 128;
			}
		}
		else
		{
			skip = false;
		}
	}

	return skip;
}

bool GSRendererHW::TryGSMemClear(bool no_rt, bool preserve_rt, bool invalidate_rt, u32 rt_end_bp,
	bool no_ds, bool preserve_z, bool invalidate_z, u32 ds_end_bp)
{
	if (m_primitive_covers_without_gaps == NoGapsType::GapsFound)
		return false;

	// Limit the hack to a single full buffer clear. Some games might use severals column to clear a screen
	// but hopefully it will be enough.
	if (m_r.width() < ((static_cast<int>(m_cached_ctx.FRAME.FBW) - 1) * 64))
		return false;

	if (!no_rt && !preserve_rt)
	{
		ClearGSLocalMemory(m_context->offset.fb, m_r, GetConstantDirectWriteMemClearColor());

		if (invalidate_rt)
		{
			g_texture_cache->InvalidateVideoMem(m_context->offset.fb, m_r, false);
			g_texture_cache->InvalidateContainedTargets(
				GSLocalMemory::GetStartBlockAddress(
					m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBW, m_cached_ctx.FRAME.PSM, m_r),
				rt_end_bp, m_cached_ctx.FRAME.PSM, m_cached_ctx.FRAME.FBW);

			GSUploadQueue clear_queue;
			clear_queue.draw = s_n;
			clear_queue.rect = m_r;
			clear_queue.blit.DBP = m_cached_ctx.FRAME.Block();
			clear_queue.blit.DBW = m_cached_ctx.FRAME.FBW;
			clear_queue.blit.DPSM = m_cached_ctx.FRAME.PSM;
			clear_queue.zero_clear = true;
			m_draw_transfers.push_back(clear_queue);
		}
	}

	if (!no_ds && !preserve_z)
	{
		ClearGSLocalMemory(m_context->offset.zb, m_r, m_vertex.buff[1].XYZ.Z);

		if (invalidate_z)
		{
			g_texture_cache->InvalidateVideoMem(m_context->offset.zb, m_r, false);
			g_texture_cache->InvalidateContainedTargets(
				GSLocalMemory::GetStartBlockAddress(
					m_cached_ctx.ZBUF.Block(), m_cached_ctx.FRAME.FBW, m_cached_ctx.ZBUF.PSM, m_r),
				ds_end_bp, m_cached_ctx.ZBUF.PSM, m_cached_ctx.FRAME.FBW);
		}
	}

	return ((invalidate_rt || no_rt) && (invalidate_z || no_ds));
}

void GSRendererHW::ClearGSLocalMemory(const GSOffset& off, const GSVector4i& r, u32 vert_color)
{
	GL_INS("HW: ClearGSLocalMemory(): %08X %d,%d => %d,%d @ BP %x BW %u %s", vert_color, r.x, r.y, r.z, r.w, off.bp(),
		off.bw(), GSUtil::GetPSMName(off.psm()));

	const u32 psm = (off.psm() == PSMCT32 && m_cached_ctx.FRAME.FBMSK == 0xFF000000u) ? PSMCT24 : off.psm();
	const int format = GSLocalMemory::m_psm[psm].fmt;

	const int left = r.left;
	const int right = r.right;
	const int bottom = r.bottom;
	int top = r.top;

	// Process the page aligned region first, then fall back to anything which is not.
	// Since pages are linear in memory, we can do it basically with a vector memset.
	// If the draw area is greater than the FBW.. I don't want to deal with that here..

	const u32 fbw = m_cached_ctx.FRAME.FBW;
	const u32 pages_wide = r.z / 64u;
	const GSVector2i& pgs = GSLocalMemory::m_psm[psm].pgs;
	if (left == 0 && top == 0 && (right & (pgs.x - 1)) == 0 && pages_wide <= fbw)
	{
		const u32 pixels_per_page = pgs.x * pgs.y;
		const int page_aligned_bottom = (bottom & ~(pgs.y - 1));

		if (format == GSLocalMemory::PSM_FMT_32)
		{
			const GSVector4i vcolor = GSVector4i(vert_color);
			const u32 iterations_per_page = (pages_wide * pixels_per_page) / 4;
			pxAssert((off.bp() & (GS_BLOCKS_PER_PAGE - 1)) == 0);
			for (u32 current_page = off.bp() >> 5; top < page_aligned_bottom; top += pgs.y, current_page += fbw)
			{
				current_page &= (GS_MAX_PAGES - 1);
				GSVector4i* ptr = reinterpret_cast<GSVector4i*>(m_mem.vm8() + current_page * GS_PAGE_SIZE);
				GSVector4i* const ptr_end = ptr + iterations_per_page;
				while (ptr != ptr_end)
					*(ptr++) = vcolor;
			}
		}
		else if (format == GSLocalMemory::PSM_FMT_24)
		{
			const GSVector4i mask = GSVector4i::xff000000();
			const GSVector4i vcolor = GSVector4i(vert_color & 0x00ffffffu);
			const u32 iterations_per_page = (pages_wide * pixels_per_page) / 4;
			pxAssert((off.bp() & (GS_BLOCKS_PER_PAGE - 1)) == 0);
			for (u32 current_page = off.bp() >> 5; top < page_aligned_bottom; top += pgs.y, current_page += fbw)
			{
				current_page &= (GS_MAX_PAGES - 1);
				GSVector4i* ptr = reinterpret_cast<GSVector4i*>(m_mem.vm8() + current_page * GS_PAGE_SIZE);
				GSVector4i* const ptr_end = ptr + iterations_per_page;
				while (ptr != ptr_end)
				{
					*ptr = (*ptr & mask) | vcolor;
					ptr++;
				}
			}
		}
		else if (format == GSLocalMemory::PSM_FMT_16)
		{
			const u16 converted_color = ((vert_color >> 16) & 0x8000) | ((vert_color >> 9) & 0x7C00) |
			                            ((vert_color >> 6) & 0x7E0) | ((vert_color >> 3) & 0x1F);
			const GSVector4i vcolor = GSVector4i::broadcast16(converted_color);
			const u32 iterations_per_page = (pages_wide * pixels_per_page) / 8;
			pxAssert((off.bp() & (GS_BLOCKS_PER_PAGE - 1)) == 0);
			for (u32 current_page = off.bp() >> 5; top < page_aligned_bottom; top += pgs.y, current_page += fbw)
			{
				current_page &= (GS_MAX_PAGES - 1);
				GSVector4i* ptr = reinterpret_cast<GSVector4i*>(m_mem.vm8() + current_page * GS_PAGE_SIZE);
				GSVector4i* const ptr_end = ptr + iterations_per_page;
				while (ptr != ptr_end)
					*(ptr++) = vcolor;
			}
		}
	}

	if (format == GSLocalMemory::PSM_FMT_32)
	{
		// Based on WritePixel32
		u32* vm = m_mem.vm32();
		for (int y = top; y < bottom; y++)
		{
			GSOffset::PAHelper pa = off.assertSizesMatch(GSLocalMemory::swizzle32).paMulti(0, y);

			for (int x = left; x < right; x++)
				vm[pa.value(x)] = vert_color;
		}
	}
	else if (format == GSLocalMemory::PSM_FMT_24)
	{
		// Based on WritePixel24
		u32* vm = m_mem.vm32();
		const u32 write_color = vert_color & 0xffffffu;
		for (int y = top; y < bottom; y++)
		{
			GSOffset::PAHelper pa = off.assertSizesMatch(GSLocalMemory::swizzle32).paMulti(0, y);

			for (int x = left; x < right; x++)
				vm[pa.value(x)] = (vm[pa.value(x)] & 0xff000000u) | write_color;
		}
	}
	else if (format == GSLocalMemory::PSM_FMT_16)
	{
		const u16 converted_color = ((vert_color >> 16) & 0x8000) | ((vert_color >> 9) & 0x7C00) | ((vert_color >> 6) & 0x7E0) | ((vert_color >> 3) & 0x1F);

		// Based on WritePixel16
		u16* vm = m_mem.vm16();
		for (int y = top; y < bottom; y++)
		{
			GSOffset::PAHelper pa = off.assertSizesMatch(GSLocalMemory::swizzle16).paMulti(0, y);

			for (int x = left; x < right; x++)
				vm[pa.value(x)] = converted_color;
		}
	}
}

bool GSRendererHW::OI_BlitFMV(GSTextureCache::Target* _rt, GSTextureCache::Source* tex, const GSVector4i& r_draw)
{
	// Not required when using Tex in RT
	if (r_draw.w > 1024 && (m_vt.m_primclass == GS_SPRITE_CLASS) && (m_vertex.next == 2) && m_process_texture && !PRIM->ABE &&
		tex && !tex->m_target && m_cached_ctx.TEX0.TBW > 0 && GSConfig.UserHacks_TextureInsideRt == GSTextureInRtMode::Disabled)
	{
		GL_PUSH("HW: OI_BlitFMV");

		GL_INS("HW: OI_BlitFMV");

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

		const int tw = static_cast<int>(1 << m_cached_ctx.TEX0.TW);
		const int th = static_cast<int>(1 << m_cached_ctx.TEX0.TH);

		// Compute the Bottom of texture rectangle
		pxAssert(m_cached_ctx.TEX0.TBP0 > m_cached_ctx.FRAME.Block());
		const int offset = (m_cached_ctx.TEX0.TBP0 - m_cached_ctx.FRAME.Block()) / m_cached_ctx.TEX0.TBW;
		GSVector4i r_texture(r_draw);
		r_texture.y -= offset;
		r_texture.w -= offset;

		if (GSTexture* rt = g_gs_device->CreateRenderTarget(tw, th, GSTexture::Format::Color))
		{
			// sRect is the top of texture
			// Need to half pixel offset the dest tex coordinates as draw pixels are top left instead of centre for texel reads.
			const GSVector4 sRect(m_vt.m_min.t.x / tw, m_vt.m_min.t.y / th, m_vt.m_max.t.x / tw, m_vt.m_max.t.y / th);
			const GSVector4 dRect = GSVector4(r_texture) + GSVector4(0.5f);
			const GSVector4i r_full(0, 0, tw, th);

			g_gs_device->CopyRect(tex->m_texture, rt, r_full, 0, 0);

			g_gs_device->StretchRect(tex->m_texture, sRect, rt, dRect, ShaderConvert::COPY, m_vt.IsRealLinear());
			g_perfmon.Put(GSPerfMon::TextureCopies, 1);

			g_gs_device->CopyRect(rt, tex->m_texture, r_full, 0, 0);
			g_perfmon.Put(GSPerfMon::TextureCopies, 1);

			g_gs_device->Recycle(rt);
		}

		// Copy back the texture into the GS mem. I don't know why but it will be
		// reuploaded again later
		g_texture_cache->Read(tex, r_texture.rintersect(tex->m_texture->GetRect()));

		g_texture_cache->InvalidateVideoMemSubTarget(_rt);

		return false; // skip current draw
	}

	// Nothing to see keep going
	return true;
}

bool GSRendererHW::AreAnyPixelsDiscarded() const
{
	return ((m_draw_env->SCANMSK.MSK & 2) || // skipping rows
	        (m_cached_ctx.TEST.ATE && m_cached_ctx.TEST.AFAIL != AFAIL_FB_ONLY) || // testing alpha (might discard some pixels)
	        m_cached_ctx.TEST.DATE); // reading alpha
}

bool GSRendererHW::IsDiscardingDstColor()
{
	return ((!PRIM->ABE || IsOpaque() || m_context->ALPHA.IsBlack()) && // no blending or writing black
	        !AreAnyPixelsDiscarded() && (m_cached_ctx.FRAME.FBMSK & GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].fmsk) == 0); // no channels masked
}

bool GSRendererHW::IsDiscardingDstRGB()
{
	return ((!PRIM->ABE || IsOpaque() || m_context->ALPHA.IsBlack() || !m_context->ALPHA.IsCdInBlend()) && // no blending or writing black
	        ((m_cached_ctx.FRAME.FBMSK & GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].fmsk) & 0xFFFFFFu) == 0); // RGB isn't masked
}

bool GSRendererHW::IsDiscardingDstAlpha() const
{
	return ((!PRIM->ABE || m_context->ALPHA.C != 1) && // not using Ad
	        ((m_cached_ctx.FRAME.FBMSK & GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].fmsk) & 0xFF000000u) == 0) && // alpha isn't masked
	       (!m_cached_ctx.TEST.ATE || !(m_cached_ctx.TEST.ATST == ATST_NEVER && m_cached_ctx.TEST.AFAIL == AFAIL_RGB_ONLY && m_cached_ctx.FRAME.PSM == PSMCT32)); // No alpha test or no rbg only
}

// Like PrimitiveCoversWithoutGaps but with texture coordinates.
bool GSRendererHW::TextureCoversWithoutGapsNotEqual()
{
	if (m_vt.m_primclass != GS_SPRITE_CLASS)
	{
		return false;
	}

	// Simple case: one sprite.
	if (m_index.tail == 2)
	{
		return true;
	}

	const GSVertex* v = &m_vertex.buff[0];
	const int first_dpY = v[1].XYZ.Y - v[0].XYZ.Y;
	const int first_dpX = v[1].XYZ.X - v[0].XYZ.X;
	const int first_dtV = v[1].V - v[0].V;
	const int first_dtU = v[1].U - v[0].U;

	// Horizontal Match.
	if ((first_dpX >> 4) == m_r.z)
	{
		// Borrowed from MergeSprite() modified to calculate heights.
		for (u32 i = 2; i < m_vertex.next; i += 2)
		{
			const int last_tV = v[i - 1].V;
			const int dtV = v[i + 1].V - v[i].V;
			const u32 last_tV_diff = std::abs(static_cast<int>(v[i].XYZ.Y) - last_tV);
			if (std::abs(dtV - first_dtV) >= 16 || last_tV_diff >= 16 || last_tV_diff == 0)
			{
				return false;
			}
		}

		return true;
	}

	// Vertical Match.
	if ((first_dpY >> 4) == m_r.w)
	{
		// Borrowed from MergeSprite().
		for (u32 i = 2; i < m_vertex.next; i += 2)
		{
			const int last_tU = v[i - 1].U;
			const int this_start_U = v[i].U;
			const int last_start_U = v[i - 2].U;

			const int dtU = v[i + 1].U - v[i].U;

			if (this_start_U < last_start_U)
			{
				if (std::abs(dtU - last_start_U) >= 16 || std::abs(this_start_U) >= 16)
				{
					return false;
				}
			}
			else
			{
				const u32 last_tU_diff = std::abs(this_start_U - last_tU);
				if (std::abs(dtU - first_dtU) >= 16 || last_tU_diff >= 16 || last_tU_diff == 0)
				{
					return false;
				}
			}
		}

		return true;
	}

	return false;
}

int GSRendererHW::IsScalingDraw(GSTextureCache::Source* src, bool no_gaps)
{
	if (GSConfig.UserHacks_NativeScaling == GSNativeScaling::Off)
		return 0;

	const GSVector2i draw_size = GSVector2i(m_vt.m_max.p.x - m_vt.m_min.p.x, m_vt.m_max.p.y - m_vt.m_min.p.y);
	GSVector2i tex_size = GSVector2i(m_vt.m_max.t.x - m_vt.m_min.t.x, m_vt.m_max.t.y - m_vt.m_min.t.y);

	tex_size.x = std::min(tex_size.x, 1 << m_cached_ctx.TEX0.TW);
	tex_size.y = std::min(tex_size.y, 1 << m_cached_ctx.TEX0.TH);

	const bool is_target_src = src && src->m_from_target;

	// Try to catch cases of stupid draws like Manhunt and Syphon Filter where they sample a single pixel.
	// Also make sure it's grabbing most of the texture.
	if (tex_size.x == 0 || tex_size.y == 0 || draw_size.x == 0 || draw_size.y == 0)
		return 0;

	const bool no_resize = (std::abs(draw_size.x - tex_size.x) <= 1 && std::abs(draw_size.y - tex_size.y) <= 1);
	const bool can_maintain = no_resize || (!is_target_src && m_index.tail == 2);

	if (!src || ((!is_target_src || src->m_from_target->m_downscaled) && can_maintain))
		return -1;

	const GSDrawingContext& next_ctx = m_env.CTXT[m_env.PRIM.CTXT];
	const bool next_tex0_is_draw = m_env.PRIM.TME && next_ctx.TEX0.TBP0 == m_cached_ctx.FRAME.Block() && next_ctx.TEX1.MMAG == 1;
	if (!PRIM->TME || (m_context->TEX1.MMAG != 1 && !next_tex0_is_draw) || m_vt.m_primclass < GS_TRIANGLE_CLASS || m_cached_ctx.FRAME.Block() == m_cached_ctx.TEX0.TBP0 ||
		IsMipMapDraw() || GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].trbpp <= 8)
		return 0;

	// Should usually be 2x but some games like Monster House goes from 512x448 -> 128x128
	const bool is_downscale = m_cached_ctx.TEX0.TBW >= m_cached_ctx.FRAME.FBW && draw_size.x <= (tex_size.x * 0.75f) && draw_size.y <= (tex_size.y * 0.75f);
	// Check we're getting most of the texture and not just stenciling a part of it.
	// Only allow non-bilineared downscales if it's most of the target (misdetections of shadows in Naruto, Transformers etc), otherwise it's fine.
	const GSVector4i src_valid = src->m_from_target ? src->m_from_target->m_valid : src->m_valid_rect;
	const GSVector2i tex_size_half = GSVector2i((src->GetRegion().HasX() ? src->GetRegionSize().x : src_valid.width()) / 2, (src->GetRegion().HasY() ? src->GetRegionSize().y : src_valid.height()) / 2);
	const bool possible_downscale = m_context->TEX1.MMIN == 1 || !src->m_from_target || src->m_from_target->m_downscaled || tex_size.x >= tex_size_half.x || tex_size.y >= tex_size_half.y;

	if (is_downscale && (draw_size.x >= PCRTCDisplays.GetResolution().x || !possible_downscale))
		return 0;

	const bool is_upscale = m_cached_ctx.TEX0.TBW <= m_cached_ctx.FRAME.FBW && ((draw_size.x / tex_size.x) >= 4 || (draw_size.y / tex_size.y) >= 4);
	// DMC does a blit in strips with the scissor to keep it inside page boundaries, so that's not technically full coverage
	// but good enough for what we want.
	const bool no_gaps_or_single_sprite = (is_downscale || is_upscale) && (no_gaps || (m_vt.m_primclass == GS_SPRITE_CLASS && SpriteDrawWithoutGaps()));

	const bool dst_discarded = IsDiscardingDstRGB() || IsDiscardingDstAlpha();
	if (no_gaps_or_single_sprite && ((is_upscale && !m_context->ALPHA.IsOpaque()) ||
		(is_downscale && (dst_discarded || (PRIM->ABE && m_context->ALPHA.C == 2 && m_context->ALPHA.FIX == 255)))))
	{
		GL_INS("HW: %s draw detected - from %dx%d to %dx%d draw %d", is_downscale ? "Downscale" : "Upscale", tex_size.x, tex_size.y, draw_size.x, draw_size.y, s_n);
		return is_upscale ? 2 : 1;
	}

	// Last ditched check if it's doing a lot of small draws exactly the same which could be recursive lighting bloom.
	if (m_vt.m_primclass == GS_SPRITE_CLASS && m_index.tail > 2 && !no_gaps_or_single_sprite && m_context->TEX1.MMAG == 1 && !m_context->ALPHA.IsOpaque())
	{
		GSVertex* v = &m_vertex.buff[0];
		float tw = 1 << src->m_TEX0.TW;
		float th = 1 << src->m_TEX0.TH;

		const int first_u = (PRIM->FST) ? (v[1].U - v[0].U) >> 4 : std::floor(static_cast<int>(tw * v[1].ST.S) - static_cast<int>(tw * v[0].ST.S));
		const int first_v = (PRIM->FST) ? (v[1].V - v[0].V) >> 4 : std::floor(static_cast<int>(th * v[1].ST.T) - static_cast<int>(th * v[0].ST.T));
		const int first_x = (v[1].XYZ.X - v[0].XYZ.X) >> 4;
		const int first_y = (v[1].XYZ.Y - v[0].XYZ.Y) >> 4;

		if (first_x > first_u && first_y > first_v && !no_resize && std::abs(draw_size.x - first_x) <= 4 && std::abs(draw_size.y - first_y) <= 4)
		{
			for (u32 i = 2; i < m_index.tail; i += 2)
			{
				const int next_u = (PRIM->FST) ? (v[i + 1].U - v[i].U) >> 4 : std::floor(static_cast<int>(tw * v[i + 1].ST.S) - static_cast<int>(tw * v[i].ST.S));
				const int next_v = (PRIM->FST) ? (v[i + 1].V - v[i].V) >> 4 : std::floor(static_cast<int>(th * v[i + 1].ST.T) - static_cast<int>(th * v[i].ST.T));
				const int next_x = (v[i + 1].XYZ.X - v[i].XYZ.X) >> 4;
				const int next_y = (v[i + 1].XYZ.Y - v[i].XYZ.Y) >> 4;

				if (std::abs(draw_size.x - next_x) > 4 || std::abs(draw_size.y - next_y) > 4)
					break;

				if (next_u != first_u || next_v != first_v || next_x != first_x || next_y != first_y)
					break;

				if (i + 2 >= m_index.tail)
					return 2;
			}
		}
	}

	return 0;
}

ClearType GSRendererHW::IsConstantDirectWriteMemClear()
{
	const bool direct_draw = (m_vt.m_primclass == GS_SPRITE_CLASS) || (m_vt.m_primclass == GS_TRIANGLE_CLASS && (m_index.tail % 6) == 0 && TrianglesAreQuads());
	// Constant Direct Write without texture/test/blending (aka a GS mem clear)
	if (direct_draw && !PRIM->TME // Direct write
		&& !(m_draw_env->SCANMSK.MSK & 2) && !m_cached_ctx.TEST.ATE // no alpha test
		&& !m_cached_ctx.TEST.DATE // no destination alpha test
		&& (!m_cached_ctx.TEST.ZTE || m_cached_ctx.TEST.ZTST == ZTST_ALWAYS) // no depth test
		&& (m_vt.m_eq.rgba == 0xFFFF || m_vertex.next == 2) // constant color write
		&& (!PRIM->FGE || m_vt.m_min.p.w == 255.0f)) // No fog effect
	{
		if ((PRIM->ABE && !m_context->ALPHA.IsOpaque()) || (m_cached_ctx.FRAME.FBMSK & GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].fmsk))
			return ClearWithDraw;

		return NormalClear;
	}
	return NotClear;
}

u32 GSRendererHW::GetConstantDirectWriteMemClearColor() const
{
	// Take the vertex colour, but check if the blending would make it black.
	const u32 vert_index = (m_vt.m_primclass == GS_TRIANGLE_CLASS) ? 2 : 1;
	u32 vert_color = m_vertex.buff[m_index.buff[vert_index]].RGBAQ.U32[0];
	if (PRIM->ABE && m_context->ALPHA.IsBlack())
		vert_color &= 0xFF000000u;

	// 24-bit format? Otherwise, FBA sets the high bit in alpha.
	const u32 cfmt = GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].fmt;
	if (cfmt == 1)
		vert_color &= 0xFFFFFFu;
	else
		vert_color |= m_context->FBA.FBA << 31;

	// Apply mask for 16-bit formats.
	if (cfmt == 2)
		vert_color &= 0x80F8F8F8u;

	return vert_color;
}

u32 GSRendererHW::GetConstantDirectWriteMemClearDepth() const
{
	const u32 max_z = (0xFFFFFFFF >> (GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].fmt * 8));
	return std::min(m_vertex.buff[1].XYZ.Z, max_z);
}

bool GSRendererHW::IsReallyDithered() const
{
	// Must have dither on, not disabled in config, and using 16-bit.
	const GSDrawingEnvironment* env = m_draw_env;
	if (!env->DTHE.DTHE || GSConfig.Dithering == 0 || GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].fmt != 2)
		return false;

	// Dithering is still on, but if the matrix is all-zero, it has no effect.
	if ((env->DIMX.U64 & UINT64_C(0x7777777777777777)) == 0)
		return false;

	return true;
}

void GSRendererHW::ReplaceVerticesWithSprite(const GSVector4i& unscaled_rect, const GSVector4i& unscaled_uv_rect,
	const GSVector2i& unscaled_size, const GSVector4i& scissor)
{
	const GSVector4i fpr = unscaled_rect.sll32<4>();
	const GSVector4i fpuv = unscaled_uv_rect.sll32<4>();
	GSVertex* v = m_vertex.buff;

	v[0].XYZ.X = static_cast<u16>(m_context->XYOFFSET.OFX + fpr.x);
	v[0].XYZ.Y = static_cast<u16>(m_context->XYOFFSET.OFY + fpr.y);
	v[0].XYZ.Z = v[1].XYZ.Z;
	v[0].RGBAQ = v[1].RGBAQ;
	v[0].FOG = v[1].FOG;

	v[1].XYZ.X = static_cast<u16>(m_context->XYOFFSET.OFX + fpr.z);
	v[1].XYZ.Y = static_cast<u16>(m_context->XYOFFSET.OFY + fpr.w);

	if (PRIM->FST)
	{
		v[0].U = fpuv.x;
		v[0].V = fpuv.y;
		v[1].U = fpuv.z;
		v[1].V = fpuv.w;
	}
	else
	{
		const GSVector4 st = GSVector4(unscaled_uv_rect) / GSVector4(GSVector4i(unscaled_size).xyxy());
		GSVector4::storel(&v[0].ST.S, st);
		GSVector4::storeh(&v[1].ST.S, st);
	}

	// Fix up vertex trace.
	m_vt.m_min.p.x = unscaled_rect.x;
	m_vt.m_min.p.y = unscaled_rect.y;
	m_vt.m_min.p.z = v[0].XYZ.Z;
	m_vt.m_max.p.x = unscaled_rect.z;
	m_vt.m_max.p.y = unscaled_rect.w;
	m_vt.m_max.p.z = v[0].XYZ.Z;
	m_vt.m_min.t.x = unscaled_uv_rect.x;
	m_vt.m_min.t.y = unscaled_uv_rect.y;
	m_vt.m_max.t.x = unscaled_uv_rect.z;
	m_vt.m_max.t.y = unscaled_uv_rect.w;
	m_vt.m_min.c = GSVector4i(v[0].RGBAQ.U32[0]).u8to32();
	m_vt.m_max.c = m_vt.m_min.c;
	m_vt.m_eq.rgba = 0xFFFF;
	m_vt.m_eq.z = true;
	m_vt.m_eq.f = true;

	m_vertex.head = m_vertex.tail = m_vertex.next = 2;
	m_index.tail = 2;

	m_r = unscaled_rect;
	m_context->scissor.in = scissor;
	m_vt.m_primclass = GS_SPRITE_CLASS;

	m_drawlist.clear();
	m_prim_overlap = PRIM_OVERLAP_NO;
}

void GSRendererHW::ReplaceVerticesWithSprite(const GSVector4i& unscaled_rect, const GSVector2i& unscaled_size)
{
	ReplaceVerticesWithSprite(unscaled_rect, unscaled_rect, unscaled_size, unscaled_rect);
}

void GSRendererHW::OffsetDraw(s32 fbp_offset, s32 zbp_offset, s32 xoffset, s32 yoffset)
{
	GL_INS("HW: Offseting render target by %d pages [%x -> %x], Z by %d pages [%x -> %x]",
		fbp_offset, m_cached_ctx.FRAME.FBP << 5, zbp_offset, (m_cached_ctx.FRAME.FBP + fbp_offset) << 5);
	GL_INS("HW: Offseting vertices by [%d, %d]", xoffset, yoffset);

	m_cached_ctx.FRAME.FBP += fbp_offset;
	m_cached_ctx.ZBUF.ZBP += zbp_offset;

	const s32 fp_xoffset = xoffset << 4;
	const s32 fp_yoffset = yoffset << 4;
	for (u32 i = 0; i < m_vertex.next; i++)
	{
		m_vertex.buff[i].XYZ.X += fp_xoffset;
		m_vertex.buff[i].XYZ.Y += fp_yoffset;
	}
}

GSHWDrawConfig& GSRendererHW::BeginHLEHardwareDraw(
	GSTexture* rt, GSTexture* ds, float rt_scale, GSTexture* tex, float tex_scale, const GSVector4i& unscaled_rect)
{
	ResetStates();

	// Bit gross, but really no other way to ensure there's nothing of the last draw left over.
	GSHWDrawConfig& config = m_conf;
	std::memset(static_cast<void*>(&config.cb_vs), 0, sizeof(config.cb_vs));
	std::memset(static_cast<void*>(&config.cb_ps), 0, sizeof(config.cb_ps));

	// Reused between draws, since the draw config is shared, you can't have multiple draws in flight anyway.
	static GSVertex vertices[4];
	static constexpr u16 indices[6] = {0, 1, 2, 2, 1, 3};

#define V(i, x, y, u, v) \
	do \
	{ \
		vertices[i].XYZ.X = x; \
		vertices[i].XYZ.Y = y; \
		vertices[i].U = u; \
		vertices[i].V = v; \
	} while (0)

	const GSVector4i fp_rect = unscaled_rect.sll32<4>();
	V(0, fp_rect.x, fp_rect.y, fp_rect.x, fp_rect.y); // top-left
	V(1, fp_rect.z, fp_rect.y, fp_rect.z, fp_rect.y); // top-right
	V(2, fp_rect.x, fp_rect.w, fp_rect.x, fp_rect.w); // bottom-left
	V(3, fp_rect.z, fp_rect.w, fp_rect.z, fp_rect.w); // bottom-right

#undef V

	GSTexture* rt_or_ds = rt ? rt : ds;
	config.rt = rt;
	config.ds = ds;
	config.tex = tex;
	config.pal = nullptr;
	config.indices = indices;
	config.verts = vertices;
	config.nverts = static_cast<u32>(std::size(vertices));
	config.nindices = static_cast<u32>(std::size(indices));
	config.indices_per_prim = 3;
	config.drawlist = nullptr;
	config.scissor = rt_or_ds->GetRect();
	config.drawarea = config.scissor;
	config.topology = GSHWDrawConfig::Topology::Triangle;
	config.blend = GSHWDrawConfig::BlendState();
	config.depth = GSHWDrawConfig::DepthStencilSelector::NoDepth();
	config.colormask = GSHWDrawConfig::ColorMaskSelector();
	config.colormask.wrgba = 0xf;
	config.require_one_barrier = false;
	config.require_full_barrier = false;
	config.destination_alpha = GSHWDrawConfig::DestinationAlphaMode::Off;
	config.datm = SetDATM::DATM0;
	config.line_expand = false;
	config.alpha_second_pass.enable = false;
	config.vs.key = 0;
	config.vs.tme = tex != nullptr;
	config.vs.iip = true;
	config.vs.fst = true;
	config.ps.key_lo = 0;
	config.ps.key_hi = 0;
	config.ps.tfx = tex ? TFX_DECAL : TFX_NONE;
	config.ps.iip = true;
	config.ps.fst = true;

	if (tex)
	{
		const GSVector2i texsize = tex->GetSize();
		config.cb_ps.WH = GSVector4(static_cast<float>(texsize.x) / tex_scale,
			static_cast<float>(texsize.y) / tex_scale, static_cast<float>(texsize.x), static_cast<float>(texsize.y));
		config.cb_ps.STScale = GSVector2(1.0f);
		config.cb_vs.texture_scale = GSVector2((1.0f / 16.0f) / config.cb_ps.WH.x, (1.0f / 16.0f) / config.cb_ps.WH.y);
	}

	const GSVector2i rtsize = rt_or_ds->GetSize();
	config.cb_vs.vertex_scale = GSVector2(2.0f * rt_scale / (rtsize.x << 4), 2.0f * rt_scale / (rtsize.y << 4));
	config.cb_vs.vertex_offset = GSVector2(-1.0f / rtsize.x + 1.0f, -1.0f / rtsize.y + 1.0f);

	return config;
}

void GSRendererHW::EndHLEHardwareDraw(bool force_copy_on_hazard /* = false */)
{
	GSHWDrawConfig& config = m_conf;

	GL_PUSH("HW: HLE hardware draw in %d,%d => %d,%d", config.drawarea.left, config.drawarea.top, config.drawarea.right,
		config.drawarea.bottom);

	GSTexture* copy = nullptr;
	if (config.tex && (config.tex == config.rt || config.tex == config.ds))
	{
		const GSDevice::FeatureSupport features = g_gs_device->Features();

		if (!force_copy_on_hazard && config.tex == config.rt)
		{
			// Sample RT 1:1.
			config.require_one_barrier = !features.framebuffer_fetch;
			config.ps.tex_is_fb = true;
		}
		else if (!force_copy_on_hazard && config.tex == config.ds && !config.depth.zwe &&
				 features.test_and_sample_depth)
		{
			// Safe to read depth buffer.
		}
		else
		{
			// Have to copy texture. Assume the whole thing is read, in all the cases this is used, it is.
			GSTexture* src = (config.tex == config.rt) ? config.rt : config.ds;
			copy = g_gs_device->CreateTexture(src->GetWidth(), src->GetHeight(), 1, src->GetFormat(), true);
			if (!copy)
			{
				Console.Error("HW: Texture allocation failure in EndHLEHardwareDraw()");
				return;
			}

			const GSVector4i copy_rect = config.drawarea.rintersect(src->GetRect());
			g_gs_device->CopyRect(src, copy, copy_rect - copy_rect.xyxy(), copy_rect.x, copy_rect.y);
			config.tex = copy;
		}
	}

	// Drop color1 if dual-source is not being used.
	config.ps.no_color = !config.rt;
	config.ps.no_color1 = !config.rt || !config.blend.enable ||
	                      (!GSDevice::IsDualSourceBlendFactor(config.blend.src_factor) &&
	                       !GSDevice::IsDualSourceBlendFactor(config.blend.dst_factor));

	g_gs_device->RenderHW(m_conf);

	if (copy)
		g_gs_device->Recycle(copy);
}

std::size_t GSRendererHW::ComputeDrawlistGetSize(float scale)
{
	if (m_drawlist.empty())
	{
		const bool save_bbox = !g_gs_device->Features().texture_barrier && g_gs_device->Features().multidraw_fb_copy;
		GetPrimitiveOverlapDrawlist(true, save_bbox, scale);
	}
	return m_drawlist.size();
}
