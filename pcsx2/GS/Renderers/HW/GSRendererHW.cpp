// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
	m_mipmap = (GSConfig.HWMipmap >= HWMipmapLevel::Basic);
	SetTCOffset();

	pxAssert(!g_texture_cache);
	g_texture_cache = std::make_unique<GSTextureCache>();
	GSTextureReplacements::Initialize();

	// Hope nothing requires too many draw calls.
	m_drawlist.reserve(2048);

	memset(&m_conf, 0, sizeof(m_conf));

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
	m_mipmap = (GSConfig.HWMipmap >= HWMipmapLevel::Basic);
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
		GL_INS("No draws or transfers, not aging TC");
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
			GL_CACHE("Frame y offset %d pixels, unit %d", y_offset, i);
		}

#ifdef ENABLE_OGL_DEBUG
		if (GSConfig.DumpGSData)
		{
			if (GSConfig.SaveFrame && s_n >= GSConfig.SaveN)
			{
				t->Save(GetDrawDumpPath("%05d_f%lld_fr%d_%05x_%s.bmp", s_n, g_perfmon.GetFrame(), i, static_cast<int>(TEX0.TBP0), psm_str(TEX0.PSM)));
			}
		}
#endif
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

#ifdef ENABLE_OGL_DEBUG
	if (GSConfig.DumpGSData && GSConfig.SaveFrame && s_n >= GSConfig.SaveN)
		t->Save(GetDrawDumpPath("%05d_f%lld_fr%d_%05x_%s.bmp", s_n, g_perfmon.GetFrame(), 3, static_cast<int>(TEX0.TBP0), psm_str(TEX0.PSM)));
#endif

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

			const float s = v0.ST.S;
			v0.ST.S = v1.ST.S;
			v1.ST.S = s;

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
	const u32 expansion_factor = 3;
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

// Fix the vertex position/tex_coordinate from 16 bits color to 32 bits color
void GSRendererHW::ConvertSpriteTextureShuffle(bool& write_ba, bool& read_ba, GSTextureCache::Target* rt, GSTextureCache::Source* tex)
{
	const u32 count = m_vertex.next;
	GSVertex* v = &m_vertex.buff[0];
	const GIFRegXYOFFSET& o = m_context->XYOFFSET;
	// Could be drawing upside down or just back to front on the actual verts.
	const GSVertex* start_verts = (v[0].XYZ.X <= v[m_vertex.tail - 2].XYZ.X) ? &v[0] : &v[m_vertex.tail - 2];
	const GSVertex first_vert = (start_verts[0].XYZ.X <= start_verts[1].XYZ.X) ? start_verts[0] : start_verts[1];
	// vertex position is 8 to 16 pixels, therefore it is the 16-31 bits of the colors
	const int pos = (first_vert.XYZ.X - o.OFX) & 0xFF;
	write_ba = (pos > 112 && pos < 136);

	// Read texture is 8 to 16 pixels (same as above)
	const float tw = static_cast<float>(1u << m_cached_ctx.TEX0.TW);
	int tex_pos = (PRIM->FST) ? first_vert.U : static_cast<int>(tw * first_vert.ST.S);
	tex_pos &= 0xFF;
	// "same group" means it can read blue and write alpha using C32 tricks
	read_ba = (tex_pos > 112 && tex_pos < 144) || (m_same_group_texture_shuffle && (m_cached_ctx.FRAME.FBMSK & 0xFFFF0000) != 0xFFFF0000);

	// Another way of selecting whether to read RG/BA is to use region repeat.
	// Ace Combat 04 reads RG, writes to RGBA by setting a MINU of 1015.
	if (m_cached_ctx.CLAMP.WMS == CLAMP_REGION_REPEAT)
	{
		GL_INS("REGION_REPEAT clamp with texture shuffle, FBMSK=%08x, MINU=%u, MINV=%u, MAXU=%u, MAXV=%u",
			m_cached_ctx.FRAME.FBMSK, m_cached_ctx.CLAMP.MINU, m_cached_ctx.CLAMP.MINV, m_cached_ctx.CLAMP.MAXU,
			m_cached_ctx.CLAMP.MAXV);

		// offset coordinates swap around RG/BA.
		const bool invert = read_ba; // (tex_pos > 112 && tex_pos < 144), i.e. 8 fixed point
		const u32 minu = (m_cached_ctx.CLAMP.MINU & 8) ^ (invert ? 8 : 0);
		read_ba = ((minu & 8) != 0);
	}

	if (m_split_texture_shuffle_pages > 0)
	{
		// Input vertices might be bad, so rewrite them.
		// We can't use the draw rect exactly here, because if the target was actually larger
		// for some reason... unhandled clears, maybe, it won't have been halved correctly.
		// So, halve it ourselves.
		const GSVector4i dr = m_r;
		const GSVector4i r = dr.blend32<9>(dr.sra32(1));
		GL_CACHE("ConvertSpriteTextureShuffle: Rewrite from %d,%d => %d,%d to %d,%d => %d,%d",
			static_cast<int>(m_vt.m_min.p.x), static_cast<int>(m_vt.m_min.p.y), static_cast<int>(m_vt.m_min.p.z),
			static_cast<int>(m_vt.m_min.p.w), r.x, r.y, r.z, r.w);

		const GSVector4i fpr = r.sll32<4>();
		v[0].XYZ.X = static_cast<u16>(m_context->XYOFFSET.OFX + fpr.x);
		v[0].XYZ.Y = static_cast<u16>(m_context->XYOFFSET.OFY + fpr.y);

		v[1].XYZ.X = static_cast<u16>(m_context->XYOFFSET.OFX + fpr.z);
		v[1].XYZ.Y = static_cast<u16>(m_context->XYOFFSET.OFY + fpr.w);

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

		m_vertex.head = m_vertex.tail = m_vertex.next = 2;
		m_index.tail = 2;
		return;
	}

	bool half_bottom_vert = true;
	bool half_right_vert = true;
	bool half_bottom_uv = true;
	bool half_right_uv = true;

	if (m_same_group_texture_shuffle)
	{
		if (m_cached_ctx.FRAME.FBW != rt->m_TEX0.TBW && m_cached_ctx.FRAME.FBW == rt->m_TEX0.TBW * 2)
			half_right_vert = false;
		else
			half_bottom_vert = false;
	}
	else
	{
		// Different source (maybe?)
		// If a game does the texture and frame doubling differently, they can burn in hell.
		if (m_cached_ctx.TEX0.TBP0 != m_cached_ctx.FRAME.Block())
		{
			// No super source of truth here, since the width can get batted around, the valid is probably our best bet.
			const int tex_width = tex->m_target ? tex->m_from_target->m_valid.z : (tex->m_TEX0.TBW * 64);
			const int tex_tbw = tex->m_target ? tex->m_from_target_TEX0.TBW : tex->m_TEX0.TBW;
			const int clamp_minu = m_context->CLAMP.MINU;
			const int clamp_maxu = m_context->CLAMP.MAXU;
			int max_tex_draw_width = std::min(static_cast<int>(m_vt.m_max.t.x), 1 << m_cached_ctx.TEX0.TW);

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

			if ((static_cast<int>(m_cached_ctx.TEX0.TBW * 64) >= std::min(tex_width * 2, 1024) && tex_tbw != m_cached_ctx.TEX0.TBW) || (m_cached_ctx.TEX0.TBW * 64) < floor(max_tex_draw_width))
			{
				half_right_uv = false;
				half_right_vert = false;
			}
			else
			{
				half_bottom_uv = false;
				half_bottom_vert = false;
			}
		}
		else
		{
			if ((floor(m_vt.m_max.p.y) <= rt->m_valid.w) && ((floor(m_vt.m_max.p.x) > (m_cached_ctx.FRAME.FBW * 64)) || (rt->m_TEX0.TBW != m_cached_ctx.FRAME.FBW)))
			{
				half_right_vert = false;
				half_right_uv = false;
			}
			else
			{
				half_bottom_vert = false;
				half_bottom_uv = false;
			}
		}
	}

	if (PRIM->FST)
	{
		GL_INS("First vertex is  P: %d => %d    T: %d => %d", v[0].XYZ.X, v[1].XYZ.X, v[0].U, v[1].U);
		const int reversed_pos = (v[0].XYZ.X > v[1].XYZ.X) ? 1 : 0;
		const int reversed_U = (v[0].U > v[1].U) ? 1 : 0;
		for (u32 i = 0; i < count; i += 2)
		{
			if (write_ba)
				v[i + reversed_pos].XYZ.X -= 128u;
			else
				v[i + 1 - reversed_pos].XYZ.X += 128u;

			if (read_ba)
				v[i + reversed_U].U -= 128u;
			else
				v[i + 1 - reversed_U].U += 128u;

			if (!half_bottom_vert)
			{
				// Height is too big (2x).
				const int tex_offset = v[i].V & 0xF;
				const GSVector4i offset(o.OFY, tex_offset, o.OFY, tex_offset);

				GSVector4i tmp(v[i].XYZ.Y, v[i].V, v[i + 1].XYZ.Y, v[i + 1].V);
				tmp = GSVector4i(tmp - offset).srl32<1>() + offset;

				v[i].XYZ.Y = static_cast<u16>(tmp.x);
				v[i + 1].XYZ.Y = static_cast<u16>(tmp.z);

				if (!half_bottom_uv)
				{
					v[i].V = static_cast<u16>(tmp.y);
					v[i + 1].V = static_cast<u16>(tmp.w);
				}
			}
		}
	}
	else
	{
		const float offset_8pix = 8.0f / tw;
		GL_INS("First vertex is  P: %d => %d    T: %f => %f (offset %f)", v[0].XYZ.X, v[1].XYZ.X, v[0].ST.S, v[1].ST.S, offset_8pix);
		const int reversed_pos = (v[0].XYZ.X > v[1].XYZ.X) ? 1 : 0;
		const int reversed_S = (v[0].ST.S > v[1].ST.S) ? 1 : 0;

		for (u32 i = 0; i < count; i += 2)
		{
			if (write_ba)
				v[i + reversed_pos].XYZ.X -= 128u;
			else
				v[i + 1 - reversed_pos].XYZ.X += 128u;

			if (read_ba)
				v[i + reversed_S].ST.S -= offset_8pix;
			else
				v[i + 1 - reversed_S].ST.S += offset_8pix;

			if (!half_bottom_vert)
			{
				// Height is too big (2x).
				const GSVector4i offset(o.OFY, o.OFY);

				GSVector4i tmp(v[i].XYZ.Y, v[i + 1].XYZ.Y);
				tmp = GSVector4i(tmp - offset).srl32<1>() + offset;

				//fprintf(stderr, "Before %d, After %d\n", v[i + 1].XYZ.Y, tmp.y);
				v[i].XYZ.Y = static_cast<u16>(tmp.x);
				v[i + 1].XYZ.Y = static_cast<u16>(tmp.y);

				if (!half_bottom_uv)
				{
					v[i].ST.T /= 2.0f;
					v[i + 1].ST.T /= 2.0f;
				}
			}
		}
	}

	// Update vertex trace too. Avoid issue to compute bounding box
	if (write_ba)
		m_vt.m_min.p.x -= 8.0f;
	else
		m_vt.m_max.p.x += 8.0f;

	if (!m_same_group_texture_shuffle)
	{
		if (read_ba)
			m_vt.m_min.t.x -= 8.0f;
		else
			m_vt.m_max.t.x += 8.0f;
	}

	if (!half_right_vert)
	{
		m_vt.m_min.p.x /= 2.0f;
		m_vt.m_max.p.x /= 2.0f;
		m_context->scissor.in.x = m_vt.m_min.p.x;
		m_context->scissor.in.z = m_vt.m_max.p.x + 8.0f;
	}

	if (!half_bottom_vert)
	{
		m_vt.m_min.p.y /= 2.0f;
		m_vt.m_max.p.y /= 2.0f;
		m_context->scissor.in.y = m_vt.m_min.p.y;
		m_context->scissor.in.w = m_vt.m_max.p.y + 8.0f;
	}

	// Only do this is the source is being interpreted as 16bit
	if (!half_bottom_uv)
	{
		m_vt.m_min.t.y /= 2.0f;
		m_vt.m_max.t.y /= 2.0f;
	}

	if (!half_right_uv)
	{
		m_vt.m_min.t.y /= 2.0f;
		m_vt.m_max.t.y /= 2.0f;
	}
}

GSVector4 GSRendererHW::RealignTargetTextureCoordinate(const GSTextureCache::Source* tex)
{
	if (GSConfig.UserHacks_HalfPixelOffset <= GSHalfPixelOffset::Normal ||
		GSConfig.UserHacks_HalfPixelOffset == GSHalfPixelOffset::Native ||
		GetUpscaleMultiplier() == 1.0f)
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

		GL_INS("offset detected %f,%f t_pos %d (linear %d, scale %f)",
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

		GL_INS("ST offset detected %f,%f (linear %d, scale %f)",
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
			// SSE optimization: shuffle m[1] to have (4*32 bits) X, Y, U, V
			const int first_dpX = v[1].XYZ.X - v[0].XYZ.X;
			const int first_dpU = v[1].U - v[0].U;
			for (u32 i = 0; i < m_vertex.next; i += 2)
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
			const GSVector4 delta_p = m_vt.m_max.p - m_vt.m_min.p;
			const GSVector4 delta_t = m_vt.m_max.t - m_vt.m_min.t;
			const bool is_blit = PrimitiveOverlap() == PRIM_OVERLAP_NO;
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

float GSRendererHW::GetTextureScaleFactor()
{
	return GetUpscaleMultiplier();
}

GSVector2i GSRendererHW::GetValidSize(const GSTextureCache::Source* tex)
{
	// Don't blindly expand out to the scissor size if we're not drawing to it.
	// e.g. Burnout 3, God of War II, etc.
	int height = std::min<int>(m_context->scissor.in.w, m_r.w);

	// If the draw is less than a page high, FBW=0 is the same as FBW=1.
	const GSLocalMemory::psm_t& frame_psm = GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM];
	int width = std::min(std::max<int>(m_cached_ctx.FRAME.FBW, 1) * 64, m_context->scissor.in.z);
	if (m_cached_ctx.FRAME.FBW == 0 && m_r.w > frame_psm.pgs.y)
	{
		GL_INS("FBW=0 when drawing more than 1 page in height (PSM %s, PGS %dx%d).", psm_str(m_cached_ctx.FRAME.PSM),
			frame_psm.pgs.x, frame_psm.pgs.y);
	}

	// If it's a channel shuffle, it'll likely be just a single page, so assume full screen.
	if (m_channel_shuffle)
	{
		const int page_x = frame_psm.pgs.x - 1;
		const int page_y = frame_psm.pgs.y - 1;
		pxAssert(tex);

		// Round up the page as channel shuffles are generally done in pages at a time
		width = (std::max(tex->GetUnscaledWidth(), width) + page_x) & ~page_x;
		height = (std::max(tex->GetUnscaledHeight(), height) + page_y) & ~page_y;
	}

	// Align to page size. Since FRAME/Z has to always start on a page boundary, in theory no two should overlap.
	width = Common::AlignUpPow2(width, frame_psm.pgs.x);
	height = Common::AlignUpPow2(height, frame_psm.pgs.y);

	// Early detection of texture shuffles. These double the input height because they're interpreting 64x32 C32 pages as 64x64 C16.
	// Why? Well, we don't want to be doubling the heights of targets, but also we don't want to align C32 targets to 64 instead of 32.
	// Yumeria's text breaks, and GOW goes to 512x448 instead of 512x416 if we don't.
	const bool possible_texture_shuffle =
		(tex && m_vt.m_primclass == GS_SPRITE_CLASS && frame_psm.bpp == 16 &&
			GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].bpp == 16 &&
			(tex->m_32_bits_fmt ||
				(m_cached_ctx.TEX0.TBP0 != m_cached_ctx.FRAME.Block() && IsOpaque() && !(m_context->TEX1.MMIN & 1) &&
					m_cached_ctx.FRAME.FBMSK && g_texture_cache->Has32BitTarget(m_cached_ctx.FRAME.Block()))));
	if (possible_texture_shuffle)
	{
		const u32 tex_width_pgs = (tex->m_target ? tex->m_from_target_TEX0.TBW : tex->m_TEX0.TBW);
		const u32 half_draw_width_pgs = ((width + (frame_psm.pgs.x - 1)) / frame_psm.pgs.x) >> 1;

		// Games such as Midnight Club 3 draw headlights with a texture shuffle, but instead of doubling the height, they doubled the width.
		if (tex_width_pgs == half_draw_width_pgs)
		{
			GL_CACHE("Halving width due to texture shuffle with double width, %dx%d -> %dx%d", width, height, width / 2, height);
			width /= 2;
		}
		else
		{
			GL_CACHE("Halving height due to texture shuffle, %dx%d -> %dx%d", width, height, width, height / 2);
			height /= 2;
		}
	}

	return  GSVector2i(width, height);
}

GSVector2i GSRendererHW::GetTargetSize(const GSTextureCache::Source* tex)
{
	const GSVector2i valid_size = GetValidSize(tex);

	return g_texture_cache->GetTargetSize(m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBW, m_cached_ctx.FRAME.PSM, valid_size.x, valid_size.y);
}

bool GSRendererHW::IsPossibleChannelShuffle() const
{
	if (!PRIM->TME || m_cached_ctx.TEX0.PSM != PSMT8 || // 8-bit texture draw
		m_vt.m_primclass != GS_SPRITE_CLASS) // draw_sprite_tex
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

bool GSRendererHW::NextDrawMatchesShuffle() const
{
	// Make sure nothing unexpected has changed.
	// Twinsanity seems to screw with ZBUF here despite it being irrelevant.
	const GSDrawingContext& next_ctx = m_env.CTXT[m_backed_up_ctx];
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

bool GSRendererHW::IsSplitTextureShuffle(GSTextureCache::Target* rt)
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
	// Y should be page aligned. X should be too, but if it's doing a copy with a shuffle (which is kinda silly), both the
	// position and coordinates may be offset by +8. See Psi-Ops - The Mindgate Conspiracy.
	if ((aligned_rc.x & 7) != 0 || aligned_rc.x > 8 || (aligned_rc.z & 7) != 0 ||
		aligned_rc.y != 0 || (aligned_rc.w & (frame_psm.pgs.y - 1)) != 0)
	{
		return false;
	}

	// Matrix Path of Neo draws 512x512 instead of 512x448, then scissors to 512x448.
	aligned_rc = aligned_rc.rintersect(m_context->scissor.in);

	// We should have the same number of pages in both the position and UV.
	const u32 pages_high = static_cast<u32>(aligned_rc.height()) / frame_psm.pgs.y;
	const u32 num_pages = m_context->FRAME.FBW * pages_high;
	// Jurassic - The Hunted will do a split shuffle with a height of 512 (256) when it's supposed to be 448, so it redoes one row of the shuffle.
	const u32 rt_half = (((rt->m_valid.height() / GSLocalMemory::m_psm[rt->m_TEX0.PSM].pgs.y) / 2) * rt->m_TEX0.TBW) + (rt->m_TEX0.TBP0 >> 5);
	// If this is a split texture shuffle, the next draw's FRAME/TEX0 should line up.
	// Re-add the offset we subtracted in Draw() to get the original FBP/TBP0.. this won't handle wrapping. Oh well.
	// "Potential" ones are for Jak3 which does a split shuffle on a 128x128 texture with a width of 256, writing to the lower half then offsetting 2 pages.
	const u32 expected_next_FBP = (m_cached_ctx.FRAME.FBP + m_split_texture_shuffle_pages) + num_pages;
	const u32 potential_expected_next_FBP = m_cached_ctx.FRAME.FBP + ((m_context->FRAME.FBW * 64) / aligned_rc.width());
	const u32 expected_next_TBP0 = (m_cached_ctx.TEX0.TBP0 + (m_split_texture_shuffle_pages + num_pages) * BLOCKS_PER_PAGE);
	const u32 potential_expected_next_TBP0 = m_cached_ctx.TEX0.TBP0 + (BLOCKS_PER_PAGE * ((m_context->TEX0.TBW * 64) / aligned_rc.width()));
	GL_CACHE("IsSplitTextureShuffle: Draw covers %ux%u pages, next FRAME %x TEX %x",
		static_cast<u32>(aligned_rc.width()) / frame_psm.pgs.x, pages_high, expected_next_FBP * BLOCKS_PER_PAGE,
		expected_next_TBP0);

	if (next_ctx.TEX0.TBP0 != expected_next_TBP0 && next_ctx.TEX0.TBP0 != potential_expected_next_TBP0 && next_ctx.TEX0.TBP0 != (rt_half << 5))
	{
		GL_CACHE("IsSplitTextureShuffle: Mismatch on TBP0, expecting %x, got %x", expected_next_TBP0, next_ctx.TEX0.TBP0);
		return false;
	}

	// Some games don't offset the FBP.
	if (next_ctx.FRAME.FBP != expected_next_FBP && next_ctx.FRAME.FBP != m_cached_ctx.FRAME.FBP && next_ctx.FRAME.FBP != potential_expected_next_FBP && next_ctx.FRAME.FBP != rt_half)
	{
		GL_CACHE("IsSplitTextureShuffle: Mismatch on FBP, expecting %x, got %x", expected_next_FBP * BLOCKS_PER_PAGE,
			next_ctx.FRAME.FBP * BLOCKS_PER_PAGE);
		return false;
	}

	// Great, everything lines up, so skip 'em.
	GL_CACHE("IsSplitTextureShuffle: Match, buffering and skipping draw.");

	if (m_split_texture_shuffle_pages == 0)
	{
		m_split_texture_shuffle_start_FBP = m_cached_ctx.FRAME.FBP;
		m_split_texture_shuffle_start_TBP = m_cached_ctx.TEX0.TBP0;

		// If the game has changed the texture width to 1 we need to retanslate it to whatever the rt has so the final rect is correct.
		if (m_cached_ctx.FRAME.FBW == 1)
			m_split_texture_shuffle_fbw = rt->m_TEX0.TBW;
		else
			m_split_texture_shuffle_fbw = m_cached_ctx.FRAME.FBW;
	}

	u32 vertical_pages = pages_high;
	u32 total_pages = num_pages;

	// If the current draw is further than the half way point and the next draw is the half way point, then we can assume it's just overdrawing.
	if (next_ctx.FRAME.FBP == rt_half && num_pages > (rt_half - (rt->m_TEX0.TBP0 >> 5)))
	{
		vertical_pages = (rt->m_valid.height() / GSLocalMemory::m_psm[rt->m_TEX0.PSM].pgs.y) / 2;
		total_pages = vertical_pages * rt->m_TEX0.TBW;
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
				GL_INS("TryToResolveSinglePageWidth(): Next FBP is split clear, using FBW of %u", next_ctx.FRAME.FBW);
				new_bw = next_ctx.FRAME.FBW;
				new_psm = next_ctx.FRAME.PSM;
			}
			else if (start_bp == next_ctx.ZBUF.Block())
			{
				GL_INS("TryToResolveSinglePageWidth(): Next ZBP is split clear, using FBW of %u", next_ctx.FRAME.FBW);
				new_bw = next_ctx.FRAME.FBW;
			}
		}

		// Might be using it as a texture next (NARC).
		if (new_bw <= 1 && next_ctx.TEX0.TBP0 == start_bp && new_bw != next_ctx.TEX0.TBW)
		{
			GL_INS("TryToResolveSinglePageWidth(): Next texture is using split clear, using FBW of %u", next_ctx.TEX0.TBW);
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
			if (tgt && ((start_bp + (m_split_clear_pages * BLOCKS_PER_PAGE)) - 1) <= tgt->m_end_block)
			{
				GL_INS("TryToResolveSinglePageWidth(): Using FBW of %u and PSM %s from existing target",
					tgt->m_TEX0.PSM, psm_str(tgt->m_TEX0.PSM));
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
			GL_INS("TryToResolveSinglePageWidth(): Fallback guess target FBW of %u", new_bw);
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
	if (m_vt.m_eq.rgba != 0xFFFF || (!m_cached_ctx.ZBUF.ZMSK && !m_vt.m_eq.z) || !PrimitiveCoversWithoutGaps())
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

	GL_INS("Starting split clear at FBP %x FBW %u PSM %s with %dx%d rect covering %u pages",
		m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBW, psm_str(m_cached_ctx.FRAME.PSM),
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
	if (m_vt.m_eq.rgba != 0xFFFF || (!m_cached_ctx.ZBUF.ZMSK && !m_vt.m_eq.z) || !PrimitiveCoversWithoutGaps())
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
			*pages_covered_by_this_draw = (((MAX_BLOCKS - end_block) + m_cached_ctx.FRAME.Block()) + (BLOCKS_PER_PAGE)) / BLOCKS_PER_PAGE;
		else
			*pages_covered_by_this_draw = ((end_block - m_cached_ctx.FRAME.Block()) + (BLOCKS_PER_PAGE)) / BLOCKS_PER_PAGE;
	}

	// must be changing FRAME
	if (m_backed_up_ctx < 0 || (m_dirty_gs_regs & (1u << DIRTY_REG_FRAME)) == 0)
		return false;

	// rect width should match the FBW (page aligned)
	if (r.width() != m_cached_ctx.FRAME.FBW * 64)
		return false;

	// next FBP should point to the end of the rect
	const GSDrawingContext& next_ctx = m_env.CTXT[m_backed_up_ctx];
	if (next_ctx.FRAME.Block() != ((end_block + 1) % MAX_BLOCKS) ||
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
		if (next_ctx.ZBUF.Block() != ((end_z_block + 1) % MAX_BLOCKS))
			return false;
	}

	return true;
}

void GSRendererHW::FinishSplitClear()
{
	GL_INS("FinishSplitClear(): Start %x FBW %u PSM %s, %u pages, %08X color", m_split_clear_start.Block(),
		m_split_clear_start.FBW, psm_str(m_split_clear_start.PSM), m_split_clear_pages, m_split_clear_color);

	// If this was a tall single-page draw, try to get a better BW from somewhere.
	if (m_split_clear_start.FBW <= 1 && m_split_clear_pages >= 16) // 1024 high
		TryToResolveSinglePageFramebuffer(m_split_clear_start, false);

	SetNewFRAME(m_split_clear_start.Block(), m_split_clear_start.FBW, m_split_clear_start.PSM);
	SetNewZBUF(m_split_clear_start_Z.Block(), m_split_clear_start_Z.PSM);
	ReplaceVerticesWithSprite(
		GetDrawRectForPages(m_split_clear_start.FBW, m_split_clear_start.PSM, m_split_clear_pages), GSVector2i(1, 1));
	GL_INS("FinishSplitClear(): New draw rect is (%d,%d=>%d,%d) with FBW %u and PSM %s", m_r.x, m_r.y, m_r.z, m_r.w,
		m_split_clear_start.FBW, psm_str(m_split_clear_start.PSM));
	m_split_clear_start.U64 = 0;
	m_split_clear_start_Z.U64 = 0;
	m_split_clear_pages = 0;
	m_split_clear_color = 0;
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

bool GSRendererHW::IsUsingCsInBlend()
{
	const GIFRegALPHA ALPHA = m_context->ALPHA;
	const bool blend_zero = (ALPHA.A == ALPHA.B || (ALPHA.C == 2 && ALPHA.FIX == 0) || (ALPHA.C == 0 && GetAlphaMinMax().max == 0));
	return (PRIM->ABE && ((ALPHA.IsUsingCs() && !blend_zero) || m_context->ALPHA.D == 0));
}

bool GSRendererHW::IsUsingAsInBlend()
{
	return (PRIM->ABE && m_context->ALPHA.IsUsingAs() && GetAlphaMinMax().max != 0);
}

bool GSRendererHW::IsTBPFrameOrZ(u32 tbp)
{
	const bool is_frame = (m_cached_ctx.FRAME.Block() == tbp);
	const bool is_z = (m_cached_ctx.ZBUF.Block() == tbp);
	if (!is_frame && !is_z)
		return false;

	const u32 fm = m_cached_ctx.FRAME.FBMSK;
	const u32 zm = m_cached_ctx.ZBUF.ZMSK || m_cached_ctx.TEST.ZTE == 0 ? 0xffffffff : 0;
	const u32 fm_mask = GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].fmsk;

	const u32 max_z = (0xFFFFFFFF >> (GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].fmt * 8));
	const bool no_rt = (!IsRTWritten() && !m_cached_ctx.TEST.DATE);
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
	return (is_frame && !no_rt) || (is_z && !no_ds);
}


void GSRendererHW::InvalidateVideoMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r)
{
	// printf("[%d] InvalidateVideoMem %d,%d - %d,%d %05x (%d)\n", static_cast<int>(g_perfmon.GetFrame()), r.left, r.top, r.right, r.bottom, static_cast<int>(BITBLTBUF.DBP), static_cast<int>(BITBLTBUF.DPSM));

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
	// printf("[%d] InvalidateLocalMem %d,%d - %d,%d %05x (%d)\n", static_cast<int>(g_perfmon.GetFrame()), r.left, r.top, r.right, r.bottom, static_cast<int>(BITBLTBUF.SBP), static_cast<int>(BITBLTBUF.SPSM));

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

	GL_INS("SwSpriteRender: Dest 0x%x W:%d F:%s, size(%d %d)", m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBW, psm_str(m_cached_ctx.FRAME.PSM), w, h);

	const GSOffset spo = m_mem.GetOffset(m_context->TEX0.TBP0, m_context->TEX0.TBW, m_context->TEX0.PSM);
	const GSOffset& dpo = m_context->offset.fb;

	const bool alpha_blending_enabled = PRIM->ABE;

	const GSVertex& v = m_index.tail > 0 ? m_vertex.buff[m_index.buff[m_index.tail - 1]] : GSVertex(); // Last vertex if any.
	const GSVector4i vc = GSVector4i(v.RGBAQ.R, v.RGBAQ.G, v.RGBAQ.B, v.RGBAQ.A) // 0x000000AA000000BB000000GG000000RR
							  .ps32(); // 0x00AA00BB00GG00RR00AA00BB00GG00RR

	const GSVector4i a_mask = GSVector4i::xff000000().u8to16(); // 0x00FF00000000000000FF000000000000

	const bool fb_mask_enabled = m_cached_ctx.FRAME.FBMSK != 0x0;
	const GSVector4i fb_mask = GSVector4i(m_cached_ctx.FRAME.FBMSK).u8to16(); // 0x00AA00BB00GG00RR00AA00BB00GG00RR

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
				                                  : (alpha_c == 0 ? sc : dc0).yyww()    // 0x00AA00BB00AA00BB00aa00bb00aa00bb
				                                                             .srl32(16) // 0x000000AA000000AA000000aa000000aa
				                                                             .ps32()    // 0x00AA00AA00aa00aa00AA00AA00aa00aa
				                                                             .xxyy();   // 0x00AA00AA00AA00AA00aa00aa00aa00aa
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
	if (GSConfig.DumpGSData && (s_n >= GSConfig.SaveN))
	{
		std::string s;

		// Dump Register state
		s = GetDrawDumpPath("%05d_context.txt", s_n);

		m_draw_env->Dump(s);
		m_context->Dump(s);

		// Dump vertices
		s = GetDrawDumpPath("%05d_vertex.txt", s_n);
		DumpVertices(s);
	}

#ifdef ENABLE_OGL_DEBUG
	static u32 num_skipped_channel_shuffle_draws = 0;
#endif

	// We mess with this state as an optimization, so take a copy and use that instead.
	const GSDrawingContext* context = m_context;
	m_cached_ctx.TEX0 = context->TEX0;
	m_cached_ctx.CLAMP = context->CLAMP;
	m_cached_ctx.TEST = context->TEST;
	m_cached_ctx.FRAME = context->FRAME;
	m_cached_ctx.ZBUF = context->ZBUF;
	m_primitive_covers_without_gaps.reset();

	if (IsBadFrame())
	{
		GL_INS("Warning skipping a draw call (%d)", s_n);
		return;
	}

	// Channel shuffles repeat lots of draws. Get out early if we can.
	if (m_channel_shuffle)
	{
		// NFSU2 does consecutive channel shuffles with blending, reducing the alpha channel over time.
		// Fortunately, it seems to change the FBMSK along the way, so this check alone is sufficient.
		// Tomb Raider: Underworld does similar, except with R, G, B in separate palettes, therefore
		// we need to split on those too.
		m_channel_shuffle = IsPossibleChannelShuffle() && m_last_channel_shuffle_fbmsk == m_context->FRAME.FBMSK;

#ifdef ENABLE_OGL_DEBUG
		if (m_channel_shuffle)
		{
			num_skipped_channel_shuffle_draws++;
			return;
		}

		if (num_skipped_channel_shuffle_draws > 0)
			GL_INS("Skipped %u channel shuffle draws", num_skipped_channel_shuffle_draws);
		num_skipped_channel_shuffle_draws = 0;
#else
		if (m_channel_shuffle)
			return;
#endif
	}

	GL_PUSH("HW Draw %d (Context %u)", s_n, PRIM->CTXT);
	GL_INS("FLUSH REASON: %s%s", GetFlushReasonString(m_state_flush_reason),
		(m_state_flush_reason != GSFlushReason::CONTEXTCHANGE && m_dirty_gs_regs) ? " AND POSSIBLE CONTEXT CHANGE" :
																					"");

	// When the format is 24bit (Z or C), DATE ceases to function.
	// It was believed that in 24bit mode all pixels pass because alpha doesn't exist
	// however after testing this on a PS2 it turns out nothing passes, it ignores the draw.
	if ((m_cached_ctx.FRAME.PSM & 0xF) == PSMCT24 && m_context->TEST.DATE)
	{
		GL_CACHE("DATE on a 24bit format, Frame PSM %x", m_context->FRAME.PSM);
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
	const GSDrawingEnvironment& env = *m_draw_env;
	const GSLocalMemory::psm_t& tex_psm = GSLocalMemory::m_psm[context->TEX0.PSM];
	if (PRIM->TME && tex_psm.pal > 0)
		m_mem.m_clut.Read32(m_cached_ctx.TEX0, env.TEXA);

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
	const u32 max_z = (0xFFFFFFFF >> (GSLocalMemory::m_psm[m_cached_ctx.ZBUF.PSM].fmt * 8));
	bool no_rt = (!IsRTWritten() && !m_cached_ctx.TEST.DATE);
	const bool all_depth_tests_pass =
		// Depth is always pass/fail (no read) and write are discarded.
		(!m_cached_ctx.TEST.ZTE || m_cached_ctx.TEST.ZTST <= ZTST_ALWAYS) ||
		// Depth test will always pass
		(m_cached_ctx.TEST.ZTST == ZTST_GEQUAL && m_vt.m_eq.z && std::min(m_vertex.buff[0].XYZ.Z, max_z) == max_z);
	bool no_ds = (zm != 0 && all_depth_tests_pass) ||
				 // Depth will be written through the RT
				 (!no_rt && m_cached_ctx.FRAME.FBP == m_cached_ctx.ZBUF.ZBP && !PRIM->TME && zm == 0 && (fm & fm_mask) == 0 && m_cached_ctx.TEST.ZTE) ||
				 // No color or Z being written.
				 (no_rt && zm != 0);

	// No Z test if no z buffer.
	if (no_ds || all_depth_tests_pass)
	{
		if (m_cached_ctx.TEST.ZTST != ZTST_ALWAYS)
			GL_CACHE("Disabling Z tests because all tests will pass.");

		m_cached_ctx.TEST.ZTST = ZTST_ALWAYS;
	}

	if (no_rt && no_ds)
	{
		GL_CACHE("Skipping draw with no color nor depth output.");
		return;
	}

	const bool draw_sprite_tex = PRIM->TME && (m_vt.m_primclass == GS_SPRITE_CLASS);

	// We trigger the sw prim render here super early, to avoid creating superfluous render targets.
	if (CanUseSwPrimRender(no_rt, no_ds, draw_sprite_tex) && SwPrimRender(*this, true, true))
	{
		GL_CACHE("Possible texture decompression, drawn with SwPrimRender() (BP %x BW %u TBP0 %x TBW %u)",
			m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBMSK, m_cached_ctx.TEX0.TBP0, m_cached_ctx.TEX0.TBW);
		return;
	}

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
	m_r = GSVector4i(m_vt.m_min.p.upld(m_vt.m_max.p) + GSVector4::cxpr(0.5f));
	m_r = m_r.blend8(m_r + GSVector4i::cxpr(0, 0, 1, 1), (m_r.xyxy() == m_r.zwzw()));
	m_r = m_r.rintersect(context->scissor.in);

	// Draw is too small, just skip it.
	if (m_r.rempty())
	{
		GL_INS("Draw %d skipped due to having an empty rect");
		return;
	}

	// We want to fix up the context if we're doing a double half clear, regardless of whether we do the CPU fill.
	const bool is_possible_mem_clear = IsConstantDirectWriteMemClear();
	if (!GSConfig.UserHacks_DisableSafeFeatures && is_possible_mem_clear)
	{
		if (!DetectStripedDoubleClear(no_rt, no_ds))
			DetectDoubleHalfClear(no_rt, no_ds);
	}

	m_process_texture = PRIM->TME && !(PRIM->ABE && m_context->ALPHA.IsBlack() && !m_cached_ctx.TEX0.TCC);
	const bool no_gaps = PrimitiveCoversWithoutGaps();
	const bool not_writing_to_all = (!no_gaps || AreAnyPixelsDiscarded() || !all_depth_tests_pass);
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
				GL_CACHE("Possible clut draw, drawn with SwPrimRender()");
				return;
			}
		}
		else if (result != CLUTDrawTestResult::NotCLUTDraw)
		{
			// Force enable preloading if any of the existing data is needed.
			// e.g. NFSMW only writes the alpha channel, and needs the RGB preloaded.
			force_preload |= preserve_rt_color;
			if (preserve_rt_color)
				GL_INS("Forcing preload due to partial/blended CLUT draw");
		}
	}

	if (!m_channel_shuffle && m_cached_ctx.FRAME.Block() == m_cached_ctx.TEX0.TBP0 &&
		IsPossibleChannelShuffle())
	{
		// Special post-processing effect
		GL_INS("Possible channel shuffle effect detected");
		m_channel_shuffle = true;
		m_last_channel_shuffle_fbmsk = m_context->FRAME.FBMSK;
	}
	else if (IsSplitClearActive())
	{
		if (ContinueSplitClear())
		{
			GL_INS("Skipping due to continued split clear, FBP %x FBW %u", m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBW);
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

	const bool is_split_texture_shuffle = (m_split_texture_shuffle_pages > 0);
	if (is_split_texture_shuffle)
	{
		// Adjust the draw rectangle to the new page range, so we get the correct fb height.
		const GSVector4i new_r = GetSplitTextureShuffleDrawRect();
		GL_CACHE(
			"Split texture shuffle: FBP %x -> %x, TBP0 %x -> %x, draw %d,%d => %d,%d -> %d,%d => %d,%d",
			m_cached_ctx.FRAME.Block(), m_split_texture_shuffle_start_FBP * BLOCKS_PER_PAGE,
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
				const GSLocalMemory::psm_t& tex_psm = GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM];
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
		}
	}

	if (!GSConfig.UserHacks_DisableSafeFeatures && is_possible_mem_clear)
	{
		GL_INS("WARNING: Possible mem clear.");

		// We'll finish things off later.
		if (IsStartingSplitClear())
		{
			CleanupDraw(false);
			return;
		}

		// Try to fix large single-page-wide draws.
		bool height_invalid = m_r.w >= 1024;
		if (height_invalid && m_cached_ctx.FRAME.FBW <= 1 &&
			TryToResolveSinglePageFramebuffer(m_cached_ctx.FRAME, true))
		{
			const GSVector2i& pgs = GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].pgs;
			ReplaceVerticesWithSprite(
				GetDrawRectForPages(m_cached_ctx.FRAME.FBW, m_cached_ctx.FRAME.PSM, (m_r.w + (pgs.y - 1)) / pgs.y),
				GSVector2i(1, 1));
			height_invalid = false;
		}

		const bool is_zero_color_clear = (GetConstantDirectWriteMemClearColor() == 0 && !preserve_rt_color);
		const bool is_zero_depth_clear = (GetConstantDirectWriteMemClearDepth() == 0 && !preserve_depth);

		// If it's an invalid-sized draw, do the mem clear on the CPU, we don't want to create huge targets.
		// If clearing to zero, don't bother creating the target. Games tend to clear more than they use, wasting VRAM/bandwidth.
		if (is_zero_color_clear || is_zero_depth_clear || height_invalid)
		{
			const u32 rt_end_bp = GSLocalMemory::GetUnwrappedEndBlockAddress(
				m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBW, m_cached_ctx.FRAME.PSM, m_r);
			const u32 ds_end_bp = GSLocalMemory::GetUnwrappedEndBlockAddress(
				m_cached_ctx.ZBUF.Block(), m_cached_ctx.FRAME.FBW, m_cached_ctx.ZBUF.PSM, m_r);
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

			if (overwriting_whole_rt && overwriting_whole_ds &&
				TryGSMemClear(no_rt, preserve_rt_color, is_zero_color_clear, rt_end_bp,
					no_ds, preserve_depth, is_zero_depth_clear, ds_end_bp))
			{
				GL_INS("Skipping (%d,%d=>%d,%d) draw at FBP %x/ZBP %x due to invalid height or zero clear.", m_r.x, m_r.y,
					m_r.z, m_r.w, m_cached_ctx.FRAME.Block(), m_cached_ctx.ZBUF.Block());

				CleanupDraw(false);
				return;
			}
		}
	}

	GIFRegTEX0 TEX0 = {};
	GSTextureCache::Source* src = nullptr;
	TextureMinMaxResult tmm;

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
				pxAssert(((m_vt.m_min.t.uph(m_vt.m_max.t) == GSVector4::zero()).mask() & 3) == 3); // ratchet and clank (menu)

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
			GL_CACHE("Estimated texture region: %u,%u -> %u,%u", MIP_CLAMP.MINU, MIP_CLAMP.MINV, MIP_CLAMP.MAXU + 1,
				MIP_CLAMP.MAXV + 1);
		}

		GIFRegTEX0 FRAME_TEX0;
		bool shuffle_target = false;
		if (!no_rt && m_cached_ctx.FRAME.Block() != m_cached_ctx.TEX0.TBP0 && GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].bpp == 16)
		{
			// FBW is going to be wrong for channel shuffling into a new target, so take it from the source.
			FRAME_TEX0.U64 = 0;
			FRAME_TEX0.TBP0 = m_cached_ctx.FRAME.Block();
			FRAME_TEX0.TBW = m_cached_ctx.FRAME.FBW;
			FRAME_TEX0.PSM = m_cached_ctx.FRAME.PSM;

			GSTextureCache::Target* tgt = g_texture_cache->LookupTarget(FRAME_TEX0, GSVector2i(m_vt.m_max.p.x, m_vt.m_max.p.y), GetTextureScaleFactor(), GSTextureCache::RenderTarget, true,
				fm);

			if (tgt)
				shuffle_target = tgt->m_32_bits_fmt;
			else
			{
				const GSVertex* v = &m_vertex.buff[0];

				const int first_x = ((v[0].XYZ.X - m_context->XYOFFSET.OFX) + 8) >> 4;
				const int first_u = PRIM->FST ? ((v[0].U + 8) >> 4) : static_cast<int>(((1 << m_cached_ctx.TEX0.TW) * (v[0].ST.S / v[1].RGBAQ.Q)) + 0.5f);
				const int second_u = PRIM->FST ? ((v[1].U + 8) >> 4) : static_cast<int>(((1 << m_cached_ctx.TEX0.TW) * (v[1].ST.S / v[1].RGBAQ.Q)) + 0.5f);
				const bool shuffle_coords = (first_x ^ first_u) & 8;
				const int draw_width = std::abs(v[1].XYZ.X - v[0].XYZ.X) >> 4;
				const int read_width = std::abs(second_u - first_u);

				shuffle_target = shuffle_coords && draw_width == 8 && draw_width == read_width;
			}

			tgt = nullptr;
		}
		const bool possible_shuffle = !no_rt && (((shuffle_target && GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].bpp == 16) || (m_cached_ctx.FRAME.Block() == m_cached_ctx.TEX0.TBP0 && ((m_cached_ctx.TEX0.PSM & 0x6) || m_cached_ctx.FRAME.PSM != m_cached_ctx.TEX0.PSM))) || IsPossibleChannelShuffle());
		const bool need_aem_color = GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].trbpp <= 24 && GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].pal == 0 && m_context->ALPHA.C == 0 && m_env.TEXA.AEM;
		const bool req_color = (!PRIM->ABE || (PRIM->ABE && (IsUsingCsInBlend() || need_aem_color))) && (possible_shuffle || (m_cached_ctx.FRAME.FBMSK & (fm_mask & 0x00FFFFFF)) != (fm_mask & 0x00FFFFFF));
		const bool alpha_used = m_context->TEX0.TCC && ((PRIM->ABE && IsUsingAsInBlend()) || (m_cached_ctx.TEST.ATE && m_cached_ctx.TEST.ATST > ATST_ALWAYS) || (possible_shuffle || (m_cached_ctx.FRAME.FBMSK & (fm_mask & 0xFF000000)) != (fm_mask & 0xFF000000)));
		const bool req_alpha = (GSUtil::GetChannelMask(m_context->TEX0.PSM) & 0x8) && alpha_used;

		// TODO: Be able to send an alpha of 1.0 (blended with vertex alpha maybe?) so we can avoid sending the texture, since we don't always need it.
		// Example games: Evolution Snowboarding, Final Fantasy Dirge of Cerberus, Red Dead Revolver, Stuntman, Tony Hawk's Underground 2, Ultimate Spider-Man.
		if (!req_color && !alpha_used)
			m_process_texture = false;
		else
		{
			src = tex_psm.depth ? g_texture_cache->LookupDepthSource(true, TEX0, env.TEXA, MIP_CLAMP, tmm.coverage, possible_shuffle, m_vt.IsLinear(), m_cached_ctx.FRAME.Block(), req_color, req_alpha) :
				g_texture_cache->LookupSource(true, TEX0, env.TEXA, MIP_CLAMP, tmm.coverage, (GSConfig.HWMipmap >= HWMipmapLevel::Basic || GSConfig.TriFilter == TriFiltering::Forced) ? &hash_lod_range : nullptr,
					possible_shuffle, m_vt.IsLinear(), m_cached_ctx.FRAME.Block(), req_color, req_alpha);

			if (!src) [[unlikely]]
			{
				GL_INS("ERROR: Source lookup failed, skipping.");
				CleanupDraw(true);
				return;
			}

			// We don't know the alpha range of direct sources when we first tried to optimize the alpha test.
			// Moving the texture lookup before the ATST optimization complicates things a lot, so instead,
			// recompute it, and everything derived from it again if it changes.
			if (GSLocalMemory::m_psm[src->m_TEX0.PSM].pal == 0)
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
					no_rt = no_rt || (!IsRTWritten() && !m_cached_ctx.TEST.DATE);
					no_ds = no_ds || (zm != 0 && all_depth_tests_pass) ||
									// Depth will be written through the RT
									(!no_rt && m_cached_ctx.FRAME.FBP == m_cached_ctx.ZBUF.ZBP && !PRIM->TME && zm == 0 && (fm & fm_mask) == 0 && m_cached_ctx.TEST.ZTE) ||
									// No color or Z being written.
									(no_rt && zm != 0);
					if (no_rt && no_ds)
					{
						GL_INS("Late draw cancel because no pixels pass alpha test.");
						CleanupDraw(true);
						return;
					}
				}
			}
		}
	}

	// Estimate size based on the scissor rectangle and height cache.
	const GSVector2i t_size = GetTargetSize(src);
	const GSVector4i t_size_rect = GSVector4i::loadh(t_size);

	// Ensure draw rect is clamped to framebuffer size. Necessary for updating valid area.
	const GSVector4i unclamped_draw_rect = m_r;
	// Don't clamp on shuffle, the height cache may troll us with the REAL height.
	if (!m_texture_shuffle && m_split_texture_shuffle_pages == 0)
		m_r = m_r.rintersect(t_size_rect);

	float target_scale = GetTextureScaleFactor();

	// This upscaling hack is for games which construct P8 textures by drawing a bunch of small sprites in C32,
	// then reinterpreting it as P8. We need to keep the off-screen intermediate textures at native resolution,
	// but not propagate that through to the normal render targets. Test Case: Crash Wrath of Cortex.
	if (no_ds && src && !m_channel_shuffle && GSConfig.UserHacks_NativePaletteDraw && src->m_from_target &&
		src->m_scale == 1.0f && (src->m_TEX0.PSM == PSMT8 || src->m_TEX0.TBP0 == m_cached_ctx.FRAME.Block()))
	{
		GL_CACHE("Using native resolution for target based on texture source");
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
	if (!no_rt)
	{
		// FBW is going to be wrong for channel shuffling into a new target, so take it from the source.
		FRAME_TEX0.U64 = 0;
		FRAME_TEX0.TBP0 = m_cached_ctx.FRAME.Block();
		FRAME_TEX0.TBW = m_channel_shuffle ? src->m_from_target_TEX0.TBW : m_cached_ctx.FRAME.FBW;
		FRAME_TEX0.PSM = m_cached_ctx.FRAME.PSM;

		// Normally we would use 1024 here to match the clear above, but The Godfather does a 1023x1023 draw instead
		// (very close to 1024x1024, but apparently the GS rounds down..). So, catch that here, we don't want to
		// create that target, because the clear isn't black, it'll hang around and never get invalidated.
		const bool is_square = (t_size.y == t_size.x) && m_r.w >= 1023 && PrimitiveCoversWithoutGaps();
		const bool is_clear = is_possible_mem_clear && is_square;
		const bool possible_shuffle = draw_sprite_tex && (((src && src->m_target && src->m_from_target && src->m_from_target->m_32_bits_fmt && GSLocalMemory::m_psm[src->m_from_target_TEX0.PSM].depth == GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].depth) &&
															  GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM].bpp == 16 && GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].bpp == 16) ||
															 IsPossibleChannelShuffle());
		rt = g_texture_cache->LookupTarget(FRAME_TEX0, t_size, target_scale, GSTextureCache::RenderTarget, true,
			fm, false, force_preload, preserve_rt_rgb, preserve_rt_alpha, unclamped_draw_rect, possible_shuffle, is_possible_mem_clear && FRAME_TEX0.TBP0 != m_cached_ctx.ZBUF.Block());

		// Draw skipped because it was a clear and there was no target.
		if (!rt)
		{
			if (is_clear)
			{
				GL_INS("Clear draw with no target, skipping.");

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

			rt = g_texture_cache->CreateTarget(FRAME_TEX0, t_size, GetValidSize(src), target_scale, GSTextureCache::RenderTarget, true,
				fm, false, force_preload, preserve_rt_color, m_r, src);
			if (!rt) [[unlikely]]
			{
				GL_INS("ERROR: Failed to create FRAME target, skipping.");
				CleanupDraw(true);
				return;
			}
		}
	}

	GSTextureCache::Target* ds = nullptr;
	GIFRegTEX0 ZBUF_TEX0;
	if (!no_ds)
	{
		ZBUF_TEX0.U64 = 0;
		ZBUF_TEX0.TBP0 = m_cached_ctx.ZBUF.Block();
		ZBUF_TEX0.TBW = m_channel_shuffle ? src->m_from_target_TEX0.TBW : m_cached_ctx.FRAME.FBW;
		ZBUF_TEX0.PSM = m_cached_ctx.ZBUF.PSM;

		ds = g_texture_cache->LookupTarget(ZBUF_TEX0, t_size, target_scale, GSTextureCache::DepthStencil,
			m_cached_ctx.DepthWrite(), 0, false, force_preload, preserve_depth, preserve_depth, unclamped_draw_rect, IsPossibleChannelShuffle(), is_possible_mem_clear && ZBUF_TEX0.TBP0 != m_cached_ctx.FRAME.Block());
		if (!ds)
		{
			ds = g_texture_cache->CreateTarget(ZBUF_TEX0, t_size, GetValidSize(src), target_scale, GSTextureCache::DepthStencil,
				true, 0, false, force_preload, preserve_depth, m_r, src);
			if (!ds) [[unlikely]]
			{
				GL_INS("ERROR: Failed to create ZBUF target, skipping.");
				CleanupDraw(true);
				return;
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
			const int first_x = ((v[0].XYZ.X - m_context->XYOFFSET.OFX) + 8) >> 4;
			const int first_u = PRIM->FST ? ((v[0].U + 8) >> 4) : static_cast<int>(((1 << m_cached_ctx.TEX0.TW) * (v[0].ST.S / v[1].RGBAQ.Q)) + 0.5f);
			const bool shuffle_coords = (first_x ^ first_u) & 8;
			const u32 draw_end = GSLocalMemory::GetEndBlockAddress(m_cached_ctx.FRAME.Block(), m_cached_ctx.FRAME.FBW, m_cached_ctx.FRAME.PSM, m_r) + 1;
			const bool draw_uses_target = src->m_from_target && ((src->m_from_target_TEX0.TBP0 <= m_cached_ctx.FRAME.Block() &&
				src->m_from_target->UnwrappedEndBlock() > m_cached_ctx.FRAME.Block()) ||
				(m_cached_ctx.FRAME.Block() < src->m_from_target_TEX0.TBP0 && draw_end > src->m_from_target_TEX0.TBP0));

			// copy of a 16bit source in to this target, make sure it's opaque and not bilinear to reduce false positives.
			m_copy_16bit_to_target_shuffle = m_cached_ctx.TEX0.TBP0 != m_cached_ctx.FRAME.Block() && rt->m_32_bits_fmt == true && IsOpaque()
											&& !(context->TEX1.MMIN & 1) && !src->m_32_bits_fmt && m_cached_ctx.FRAME.FBMSK;

			// It's not actually possible to do a C16->C16 texture shuffle of B to A as they are the same group
			// However you can do it by using C32 and offsetting the target verticies to point to B A, then mask as appropriate.
			m_same_group_texture_shuffle = draw_uses_target && (m_cached_ctx.TEX0.PSM & 0xE) == PSMCT32 && (m_cached_ctx.FRAME.PSM & 0x7) == PSMCT16 && (m_vt.m_min.p.x == 8.0f);

			// Both input and output are 16 bits and texture was initially 32 bits! Same for the target, Sonic Unleash makes a new target which really is 16bit.
			m_texture_shuffle = ((m_same_group_texture_shuffle || (tex_psm.bpp == 16)) && (GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].bpp == 16) &&
				(shuffle_coords || rt->m_32_bits_fmt))
				&& draw_sprite_tex && (src->m_32_bits_fmt || m_copy_16bit_to_target_shuffle);
		};

		// Okami mustn't call this code
		if (m_texture_shuffle && m_vertex.next < 3 && PRIM->FST && ((m_cached_ctx.FRAME.FBMSK & fm_mask) == 0))
		{
			// Avious dubious call to m_texture_shuffle on 16 bits games
			// The pattern is severals column of 8 pixels. A single sprite
			// smell fishy but a big sprite is wrong.

			// Shadow of Memories/Destiny shouldn't call this code.
			// Causes shadow flickering.
			m_texture_shuffle = ((v[1].U - v[0].U) < 256) ||
				// Tomb Raider Angel of Darkness relies on this behavior to produce a fog effect.
				// In this case, the address of the framebuffer and texture are the same.
				// The game will take RG => BA and then the BA => RG of next pixels.
				// However, only RG => BA needs to be emulated because RG isn't used.
				m_cached_ctx.FRAME.Block() == m_cached_ctx.TEX0.TBP0 ||
				// DMC3, Onimusha 3 rely on this behavior.
				// They do fullscreen rectangle with scissor, then shift by 8 pixels, not done with recursion.
				// So we check if it's a TS effect by checking the scissor.
				((m_context->SCISSOR.SCAX1 - m_context->SCISSOR.SCAX0) < 32);

			GL_INS("WARNING: Possible misdetection of effect, texture shuffle is %s", m_texture_shuffle ? "Enabled" : "Disabled");
		}

		if (m_texture_shuffle && IsSplitTextureShuffle(rt))
		{
			// If TEX0 == FBP, we're going to have a source left in the TC.
			// That source will get used in the actual draw unsafely, so kick it out.
			if (m_cached_ctx.FRAME.Block() == m_cached_ctx.TEX0.TBP0)
				g_texture_cache->InvalidateVideoMem(context->offset.fb, m_r, false);

			CleanupDraw(true);
			return;
		}

		if (src->m_target && IsPossibleChannelShuffle())
		{
			GL_INS("Channel shuffle effect detected (2nd shot)");
			m_channel_shuffle = true;
			m_last_channel_shuffle_fbmsk = m_context->FRAME.FBMSK;
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
		if (IsMipMapActive() && GSConfig.HWMipmap == HWMipmapLevel::Full && !tex_psm.depth && !src->m_from_hash_cache)
		{
			// Upload remaining texture layers
			const GSVector4 tmin = m_vt.m_min.t;
			const GSVector4 tmax = m_vt.m_max.t;

			for (int layer = m_lod.x + 1; layer <= m_lod.y; layer++)
			{
				const GIFRegTEX0 MIP_TEX0(GetTex0Layer(layer));

				MIP_CLAMP.MINU >>= 1;
				MIP_CLAMP.MINV >>= 1;
				MIP_CLAMP.MAXU >>= 1;
				MIP_CLAMP.MAXV >>= 1;

				m_vt.m_min.t *= 0.5f;
				m_vt.m_max.t *= 0.5f;

				tmm = GetTextureMinMax(MIP_TEX0, MIP_CLAMP, m_vt.IsLinear(), false);

				src->UpdateLayer(MIP_TEX0, tmm.coverage, layer - m_lod.x);
			}

			// we don't need to generate mipmaps since they were provided
			src->m_texture->ClearMipmapGenerationFlag();
			m_vt.m_min.t = tmin;
			m_vt.m_max.t = tmax;
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
	const bool can_update_size = !is_possible_mem_clear && !m_texture_shuffle && !m_channel_shuffle;
	if (!m_texture_shuffle && !m_channel_shuffle)
	{
		// Try to turn blits in to single sprites, saves upscaling problems when striped clears/blits.
		if (m_vt.m_primclass == GS_SPRITE_CLASS && no_gaps && m_index.tail > 2 && (!PRIM->TME || TextureCoversWithoutGapsNotEqual()) && m_vt.m_eq.rgba == 0xFFFF)
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
		const bool blending_cd = PRIM->ABE && !m_context->ALPHA.IsOpaque();
		if (rt && ((!is_possible_mem_clear || blending_cd) || rt->m_TEX0.PSM != FRAME_TEX0.PSM))
		{
			if (rt->m_TEX0.TBW != FRAME_TEX0.TBW && !m_cached_ctx.ZBUF.ZMSK && (m_cached_ctx.FRAME.FBMSK & 0xFF000000))
			{
				// Alpha could be a font, and since the width is changing it's no longer valid.
				// Be careful of downsize copies or other effects, checking Z MSK should hopefully be enough.. (Okami).
				if (m_cached_ctx.FRAME.FBMSK & 0x0F000000)
					rt->m_valid_alpha_low = false;
				if (m_cached_ctx.FRAME.FBMSK & 0xF0000000)
					rt->m_valid_alpha_high = false;
			}
			rt->m_TEX0 = FRAME_TEX0;
		}

		if (ds && (!is_possible_mem_clear || ds->m_TEX0.PSM != ZBUF_TEX0.PSM || (rt && ds->m_TEX0.TBW != rt->m_TEX0.TBW)))
			ds->m_TEX0 = ZBUF_TEX0;
	}
	else if (!m_texture_shuffle)
	{
		// Allow FB PSM to update on channel shuffle, it should be correct, unlike texture shuffle.
		// The FBW should also be okay, since it's coming from the source.
		if (rt)
		{
			rt->m_TEX0.TBW = std::max(rt->m_TEX0.TBW, FRAME_TEX0.TBW);
			rt->m_TEX0.PSM = FRAME_TEX0.PSM;
		}
		if (ds)
		{
			ds->m_TEX0.TBW = std::max(ds->m_TEX0.TBW, ZBUF_TEX0.TBW);
			ds->m_TEX0.PSM = ZBUF_TEX0.PSM;
		}
	}

	// Figure out which channels we're writing.
	if (rt)
		rt->UpdateValidChannels(rt->m_TEX0.PSM, m_texture_shuffle ? GetEffectiveTextureShuffleFbmsk() : fm);
	if (ds)
		ds->UpdateValidChannels(ZBUF_TEX0.PSM, zm);

	const GSVector2i resolution = PCRTCDisplays.GetResolution();
	GSTextureCache::Target* old_rt = nullptr;
	GSTextureCache::Target* old_ds = nullptr;

	// If the draw is dated, we're going to expand in to black, so it's just a pointless rescale which will mess up our valid rects and end blocks.
	if(!(m_cached_ctx.TEST.DATE && m_cached_ctx.TEST.DATM))
	{
		GSVector2i new_size = t_size;

		// We need to adjust the size if it's a texture shuffle as we could end up making the RT twice the size.
		if (src && m_texture_shuffle && m_split_texture_shuffle_pages == 0)
		{
			if ((new_size.x > src->m_valid_rect.z && m_vt.m_max.p.x == new_size.x) || (new_size.y > src->m_valid_rect.w && m_vt.m_max.p.y == new_size.y))
			{
				if (new_size.y <= src->m_valid_rect.w && (rt->m_TEX0.TBW != m_cached_ctx.FRAME.FBW))
					new_size.x /= 2;
				else
					new_size.y /= 2;
			}
		}

		// We still need to make sure the dimensions of the targets match.
		const int new_w = std::max(new_size.x, std::max(rt ? rt->m_unscaled_size.x : 0, ds ? ds->m_unscaled_size.x : 0));
		const int new_h = std::max(new_size.y, std::max(rt ? rt->m_unscaled_size.y : 0, ds ? ds->m_unscaled_size.y : 0));
		if (rt)
		{
			const u32 old_end_block = rt->m_end_block;
			const bool new_rect = rt->m_valid.rempty();
			const bool new_height = new_h > rt->GetUnscaledHeight();
			const int old_height = rt->m_texture->GetHeight();

			pxAssert(rt->GetScale() == target_scale);
			if (rt->GetUnscaledWidth() != new_w || rt->GetUnscaledHeight() != new_h)
				GL_INS("Resize RT from %dx%d to %dx%d", rt->GetUnscaledWidth(), rt->GetUnscaledHeight(), new_w, new_h);

			rt->ResizeTexture(new_w, new_h);

			if (!m_texture_shuffle && !m_channel_shuffle)
			{
				// if the height cache gave a different size to our final size, we need to check if it needs preloading.
				// Pirates - Legend of the Black Kat starts a draw of 416, but Z is 448 and it preloads the background.
				if (rt->m_drawn_since_read.rempty() && rt->m_dirty.size() > 0 && new_height && (preserve_rt_color || preserve_rt_alpha)) {
					RGBAMask mask;
					mask._u32 = preserve_rt_color ? 0x7 : 0;
					mask.c.a |= preserve_rt_alpha;
					g_texture_cache->AddDirtyRectTarget(rt, GSVector4i(rt->m_valid.x, rt->m_valid.w, rt->m_valid.z, new_h), rt->m_TEX0.PSM, rt->m_TEX0.TBW, mask, false);
					g_texture_cache->GetTargetSize(rt->m_TEX0.TBP0, rt->m_TEX0.TBW, rt->m_TEX0.PSM, 0, new_h);
				}

				rt->ResizeValidity(rt->GetUnscaledRect());
				rt->ResizeDrawn(rt->GetUnscaledRect());
			}

			const GSVector4i update_rect = m_r.rintersect(GSVector4i::loadh(new_size));
			// Limit to 2x the vertical height of the resolution (for double buffering)
			rt->UpdateValidity(update_rect, can_update_size || (m_r.w <= (resolution.y * 2) && !m_texture_shuffle));
			rt->UpdateDrawn(update_rect, can_update_size || (m_r.w <= (resolution.y * 2) && !m_texture_shuffle));
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
					GL_INS("RT double buffer copy from FBP 0x%x, %dx%d => %d,%d", old_rt->m_TEX0.TBP0, copy_width, copy_height, 0, old_height);

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
				GL_INS("Resize DS from %dx%d to %dx%d", ds->GetUnscaledWidth(), ds->GetUnscaledHeight(), new_w, new_h);
			ds->ResizeTexture(new_w, new_h);

			if (!m_texture_shuffle && !m_channel_shuffle)
			{
				ds->ResizeValidity(ds->GetUnscaledRect());
				ds->ResizeDrawn(ds->GetUnscaledRect());
			}

			// Limit to 2x the vertical height of the resolution (for double buffering)
			ds->UpdateValidity(m_r, can_update_size || m_r.w <= (resolution.y * 2));
			ds->UpdateDrawn(m_r, can_update_size || m_r.w <= (resolution.y * 2));

			if (!new_rect && new_height && old_end_block != ds->m_end_block)
			{
				old_ds = g_texture_cache->FindTargetOverlap(ds, GSTextureCache::DepthStencil, m_cached_ctx.ZBUF.PSM);

				if (old_ds && old_ds != ds && GSUtil::HasSharedBits(old_ds->m_TEX0.PSM, ds->m_TEX0.PSM))
				{
					const int copy_width = (old_ds->m_texture->GetWidth()) > (ds->m_texture->GetWidth()) ? (ds->m_texture->GetWidth()) : old_ds->m_texture->GetWidth();
					const int copy_height = (old_ds->m_texture->GetHeight()) > (ds->m_texture->GetHeight() - old_height) ? (ds->m_texture->GetHeight() - old_height) : old_ds->m_texture->GetHeight();
					GL_INS("DS double buffer copy from FBP 0x%x, %dx%d => %d,%d", old_ds->m_TEX0.TBP0, copy_width, copy_height, 0, old_height);

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

	if (rt)
	{
		if (m_texture_shuffle || m_channel_shuffle || (!rt->m_dirty.empty() && !rt->m_dirty.GetTotalRect(rt->m_TEX0, rt->m_unscaled_size).rintersect(m_r).rempty()))
			rt->Update();
		else
			rt->m_age = 0;
	}
	if (ds)
	{
		if (m_texture_shuffle || m_channel_shuffle || (!ds->m_dirty.empty() && !ds->m_dirty.GetTotalRect(ds->m_TEX0, ds->m_unscaled_size).rintersect(m_r).rempty()))
			ds->Update();
		else
			ds->m_age = 0;
	}

	if (src && src->m_shared_texture && src->m_texture != src->m_from_target->m_texture)
	{
		// Target texture changed, update reference.
		src->m_texture = src->m_from_target->m_texture;
	}

	if (GSConfig.DumpGSData)
	{
		const u64 frame = g_perfmon.GetFrame();

		std::string s;

		if (GSConfig.SaveTexture && s_n >= GSConfig.SaveN && src)
		{
			s = GetDrawDumpPath("%05d_f%lld_itex_%05x_%s_%d%d_%02x_%02x_%02x_%02x.dds",
				s_n, frame, static_cast<int>(m_cached_ctx.TEX0.TBP0), psm_str(m_cached_ctx.TEX0.PSM),
				static_cast<int>(m_cached_ctx.CLAMP.WMS), static_cast<int>(m_cached_ctx.CLAMP.WMT),
				static_cast<int>(m_cached_ctx.CLAMP.MINU), static_cast<int>(m_cached_ctx.CLAMP.MAXU),
				static_cast<int>(m_cached_ctx.CLAMP.MINV), static_cast<int>(m_cached_ctx.CLAMP.MAXV));

			src->m_texture->Save(s);

			if (src->m_palette)
			{
				s = GetDrawDumpPath("%05d_f%lld_itpx_%05x_%s.dds", s_n, frame, m_cached_ctx.TEX0.CBP, psm_str(m_cached_ctx.TEX0.CPSM));

				src->m_palette->Save(s);
			}
		}

		if (rt && GSConfig.SaveRT && s_n >= GSConfig.SaveN)
		{
			s = GetDrawDumpPath("%05d_f%lld_rt0_%05x_%s.bmp", s_n, frame, m_cached_ctx.FRAME.Block(), psm_str(m_cached_ctx.FRAME.PSM));

			if (rt->m_texture)
				rt->m_texture->Save(s);
		}

		if (ds && GSConfig.SaveDepth && s_n >= GSConfig.SaveN)
		{
			s = GetDrawDumpPath("%05d_f%lld_rz0_%05x_%s.bmp", s_n, frame, m_cached_ctx.ZBUF.Block(), psm_str(m_cached_ctx.ZBUF.PSM));

			if (ds->m_texture)
				ds->m_texture->Save(s);
		}
	}

	if (m_oi && !m_oi(*this, rt ? rt->m_texture : nullptr, ds ? ds->m_texture : nullptr, src))
	{
		GL_INS("Warning skipping a draw call (%d)", s_n);
		CleanupDraw(true);
		return;
	}

	if (!OI_BlitFMV(rt, src, m_r))
	{
		GL_INS("Warning skipping a draw call (%d)", s_n);
		CleanupDraw(true);
		return;
	}

	bool skip_draw = false;
	if (!GSConfig.UserHacks_DisableSafeFeatures && is_possible_mem_clear)
		skip_draw = TryTargetClear(rt, ds, preserve_rt_color, preserve_depth);

	// A couple of hack to avoid upscaling issue. So far it seems to impacts mostly sprite
	// Note: first hack corrects both position and texture coordinate
	// Note: second hack corrects only the texture coordinate
	if (CanUpscale() && (m_vt.m_primclass == GS_SPRITE_CLASS))
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

	if (!skip_draw)
		DrawPrims(rt, ds, src, tmm);

	//

	// Temporary source *must* be invalidated before normal, because otherwise it'll be double freed.
	g_texture_cache->InvalidateTemporarySource();

	// Invalidation of old targets when changing to double-buffering.
	if (old_rt)
		g_texture_cache->InvalidateVideoMemType(GSTextureCache::RenderTarget, old_rt->m_TEX0.TBP0);
	if (old_ds)
		g_texture_cache->InvalidateVideoMemType(GSTextureCache::DepthStencil, old_ds->m_TEX0.TBP0);

	if ((fm & fm_mask) != fm_mask && rt)
	{
		//rt->m_valid = rt->m_valid.runion(r);
		// Limit to 2x the vertical height of the resolution (for double buffering)
		rt->UpdateValidity(m_r, can_update_size || (m_r.w <= (resolution.y * 2) && !m_texture_shuffle));

		g_texture_cache->InvalidateVideoMem(context->offset.fb, m_r, false);

		// Remove overwritten Zs at the FBP.
		g_texture_cache->InvalidateVideoMemType(GSTextureCache::DepthStencil, m_cached_ctx.FRAME.Block(),
			m_cached_ctx.FRAME.PSM, m_texture_shuffle ? GetEffectiveTextureShuffleFbmsk() : fm);
	}

	if (zm != 0xffffffff && ds)
	{
		//ds->m_valid = ds->m_valid.runion(r);
		// Limit to 2x the vertical height of the resolution (for double buffering)
		ds->UpdateValidity(m_r, can_update_size || (m_r.w <= (resolution.y * 2) && !m_texture_shuffle));

		g_texture_cache->InvalidateVideoMem(context->offset.zb, m_r, false);

		// Remove overwritten RTs at the ZBP.
		g_texture_cache->InvalidateVideoMemType(
			GSTextureCache::RenderTarget, m_cached_ctx.ZBUF.Block(), m_cached_ctx.ZBUF.PSM, zm);
	}

	//

	if (GSConfig.DumpGSData)
	{
		const u64 frame = g_perfmon.GetFrame();

		std::string s;

		if (GSConfig.SaveRT && s_n >= GSConfig.SaveN)
		{
			s = GetDrawDumpPath("%05d_f%lld_rt1_%05x_%s.bmp", s_n, frame, m_cached_ctx.FRAME.Block(), psm_str(m_cached_ctx.FRAME.PSM));

			if (rt)
				rt->m_texture->Save(s);
		}

		if (GSConfig.SaveDepth && s_n >= GSConfig.SaveN)
		{
			s = GetDrawDumpPath("%05d_f%lld_rz1_%05x_%s.bmp", s_n, frame, m_cached_ctx.ZBUF.Block(), psm_str(m_cached_ctx.ZBUF.PSM));

			if (ds)
				ds->m_texture->Save(s);
		}

		if (GSConfig.SaveL > 0 && (s_n - GSConfig.SaveN) > GSConfig.SaveL)
		{
			GSConfig.DumpGSData = 0;
		}
	}

#ifdef DISABLE_HW_TEXTURE_CACHE
	if (rt)
		g_texture_cache->Read(rt, m_r);
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
			if (g_gs_device->Features().provoking_vertex_last)
			{
				for (u32 i = 0; i < m_index.tail; i += 2)
				{
					if (m_index.buff[i] + 1 != m_index.buff[i + 1])
						return false;
				}
			}
			else
			{
				for (u32 i = 0; i < m_index.tail; i += 2)
				{
					if (m_index.buff[i] != m_index.buff[i + 1] + 1)
						return false;
				}
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

void GSRendererHW::SetupIA(float target_scale, float sx, float sy)
{
	GL_PUSH("IA");

	if (GSConfig.UserHacks_WildHack && !m_isPackedUV_HackFlag && m_process_texture && PRIM->FST)
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
					m_conf.verts = m_vertex.buff;
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
			}
			break;

		default:
			ASSUME(0);
	}

	m_conf.verts = m_vertex.buff;
	m_conf.nverts = m_vertex.next;
	m_conf.indices = m_index.buff;
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
			m_conf.cb_ps.TA_MaxDepth_Af.z = static_cast<float>(max_z) * (g_gs_device->Features().clip_control ? 0x1p-32f : 0x1p-24f);
			m_conf.ps.zclamp = 1;
		}
	}
}

void GSRendererHW::EmulateTextureShuffleAndFbmask(GSTextureCache::Target* rt, GSTextureCache::Source* tex)
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
		m_conf.ps.dst_fmt = GSLocalMemory::PSM_FMT_32;

		bool write_ba;
		bool read_ba;

		ConvertSpriteTextureShuffle(write_ba, read_ba, rt, tex);

		// If date is enabled you need to test the green channel instead of the
		// alpha channel. Only enable this code in DATE mode to reduce the number
		// of shader.
		m_conf.ps.write_rg = !write_ba && features.texture_barrier && m_cached_ctx.TEST.DATE;

		m_conf.ps.read_ba = read_ba;
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

		// Ace Combat 04 sets FBMSK to 0 for the shuffle, duplicating RG across RGBA.
		// Given how touchy texture shuffles are, I'm not ready to make it 100% dependent on the real FBMSK yet.
		// TODO: Remove this if, and see what breaks.
		if (fbmask != 0)
		{
			m_conf.colormask.wrgba = 0;
		}
		else
		{
			m_conf.colormask.wr = m_conf.colormask.wg = (rb_ga_mask.r != 0xFF);
			m_conf.colormask.wb = m_conf.colormask.wa = (rb_ga_mask.g != 0xFF);
		}

		// 2 Select the new mask
		if (rb_ga_mask.r != 0xFF)
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
			if (rb_ga_mask.r)
				m_conf.ps.fbmask = 1;
		}

		if (rb_ga_mask.g != 0xFF)
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
			if (rb_ga_mask.g)
				m_conf.ps.fbmask = 1;
		}

		if (m_conf.ps.fbmask && enable_fbmask_emulation)
		{
			m_conf.cb_ps.FbMask.r = rb_ga_mask.r;
			m_conf.cb_ps.FbMask.g = rb_ga_mask.g;
			m_conf.cb_ps.FbMask.b = rb_ga_mask.r;
			m_conf.cb_ps.FbMask.a = rb_ga_mask.g;

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

		// Set dirty alpha on target, but only if we're actually writing to it.
		if (rt)
		{
			rt->m_valid_alpha_low |= m_conf.colormask.wa;
			rt->m_valid_alpha_high |= m_conf.colormask.wa;
		}

		// Once we draw the shuffle, no more buffering.
		m_split_texture_shuffle_pages = 0;
		m_split_texture_shuffle_pages_high = 0;
		m_split_texture_shuffle_start_FBP = 0;
		m_split_texture_shuffle_start_TBP = 0;
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
			if (!PRIM->ABE || !(~ff_fbmask & ~zero_fbmask & 0x7) || !g_gs_device->Features().texture_barrier)
			{
				GL_INS("FBMASK Unsafe SW emulated fb_mask:%x on %d bits format", m_cached_ctx.FRAME.FBMSK,
					(m_conf.ps.dst_fmt == GSLocalMemory::PSM_FMT_16) ? 16 : 32);
				m_conf.require_one_barrier = true;
			}
			else
			{
				// The safe and accurate path (but slow)
				GL_INS("FBMASK SW emulated fb_mask:%x on %d bits format", m_cached_ctx.FRAME.FBMSK,
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

__ri bool GSRendererHW::EmulateChannelShuffle(GSTextureCache::Target* src, bool test_only)
{
	if ((src->m_texture->GetType() == GSTexture::Type::DepthStencil) && !src->m_32_bits_fmt)
	{
		// So far 2 games hit this code path. Urban Chaos and Tales of Abyss
		// UC: will copy depth to green channel
		// ToA: will copy depth to alpha channel
		if ((m_cached_ctx.FRAME.FBMSK & 0x00FF0000) == 0x00FF0000)
		{
			// Green channel is masked
			GL_INS("Tales Of Abyss Crazyness (MSB 16b depth to Alpha)");
			if (test_only)
				return true;

			m_conf.ps.tales_of_abyss_hle = 1;
		}
		else
		{
			GL_INS("Urban Chaos Crazyness (Green extraction)");
			if (test_only)
				return true;

			m_conf.ps.urban_chaos_hle = 1;
		}
	}
	else if (m_index.tail <= 64 && m_cached_ctx.CLAMP.WMT == 3)
	{
		// Blood will tell. I think it is channel effect too but again
		// implemented in a different way. I don't want to add more CRC stuff. So
		// let's disable channel when the signature is different
		//
		// Note: Tales Of Abyss and Tekken5 could hit this path too. Those games are
		// handled above.
		GL_INS("Maybe not a channel!");
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

		ChannelFetch channel_select = (m_cached_ctx.CLAMP.WMT != 3 || (m_cached_ctx.CLAMP.WMT == 3 && ((m_cached_ctx.CLAMP.MAXV & 0x2) == 0))) ? ChannelFetch_BLUE : ChannelFetch_ALPHA;

		GL_INS("%s channel", (channel_select == ChannelFetch_BLUE) ? "blue" : "alpha");

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

				GL_INS("Green/Blue channel (%d, %d)", blue_shift, green_shift);
				if (test_only)
					return true;

				m_conf.cb_ps.ChannelShuffle = GSVector4i(blue_mask, blue_shift, green_mask, green_shift);
				m_conf.ps.channel = ChannelFetch_GXBY;
				m_cached_ctx.FRAME.FBMSK = 0x00FFFFFF;
			}
			else
			{
				GL_INS("Green channel (wrong mask) (fbmask %x)", blue_mask);
				if (test_only)
					return true;

				m_conf.ps.channel = ChannelFetch_GREEN;
			}
		}
		else if (green)
		{
			GL_INS("Green channel");
			if (test_only)
				return true;

			m_conf.ps.channel = ChannelFetch_GREEN;
		}
		else
		{
			// Pop
			GL_INS("Red channel");
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
		const GSVector4i min_uv = GSVector4i(m_vt.m_min.t.upld(GSVector4::zero()));
		ChannelFetch channel = ChannelFetch_NONE;
		if (GSLocalMemory::IsPageAligned(src->m_TEX0.PSM, m_r) &&
			m_r.upl64(GSVector4i::zero()).eq(GSVector4i::zero()))
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
			GL_INS("%s channel from min UV: r={%d,%d=>%d,%d} min uv = %d,%d", channel_names[static_cast<u32>(channel - 1)],
				m_r.x, m_r.y, m_r.z, m_r.w, min_uv.x, min_uv.y);
#endif

			if (test_only)
				return true;

			m_conf.ps.channel = channel;
		}
		else
		{
			GL_INS("Channel not supported r={%d,%d=>%d,%d} min uv = %d,%d",
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

	GSVertex* s = &m_vertex.buff[0];
	s[0].XYZ.X = static_cast<u16>(m_context->XYOFFSET.OFX + 0);
	s[1].XYZ.X = static_cast<u16>(m_context->XYOFFSET.OFX + 16384);
	s[0].XYZ.Y = static_cast<u16>(m_context->XYOFFSET.OFY + 0);
	s[1].XYZ.Y = static_cast<u16>(m_context->XYOFFSET.OFY + 16384);

	m_vertex.head = m_vertex.tail = m_vertex.next = 2;
	m_index.tail = 2;
	return true;
}

void GSRendererHW::EmulateBlending(int rt_alpha_min, int rt_alpha_max, bool& DATE_PRIMID, bool& DATE_BARRIER, bool& blending_alpha_pass)
{
	{
		// AA1: Blending needs to be enabled on draw.
		const bool AA1 = PRIM->AA1 && (m_vt.m_primclass == GS_LINE_CLASS || m_vt.m_primclass == GS_TRIANGLE_CLASS);
		// PABE: Check condition early as an optimization.
		const bool PABE = PRIM->ABE && m_draw_env->PABE.PABE && (GetAlphaMinMax().max < 128);
		// FBMASK: Color is not written, no need to do blending.
		const u32 temp_fbmask = m_conf.ps.dst_fmt == GSLocalMemory::PSM_FMT_16 ? 0x00F8F8F8 : 0x00FFFFFF;
		const bool FBMASK = (m_cached_ctx.FRAME.FBMSK & temp_fbmask) == temp_fbmask;

		// No blending or coverage anti-aliasing so early exit
		if (FBMASK || PABE || !(PRIM->ABE || AA1))
		{
			m_conf.blend = {};
			m_conf.ps.no_color1 = true;

			return;
		}
	}

	// Compute the blending equation to detect special case
	const GSDevice::FeatureSupport features(g_gs_device->Features());
	const GIFRegALPHA& ALPHA = m_context->ALPHA;
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
	GL_INS("EmulateBlending(): (%s - %s) * %s + %s", col[ALPHA.A], col[ALPHA.B], alpha[ALPHA.C], col[ALPHA.D]);
	GL_INS("Draw AlphaMinMax: %d-%d, RT AlphaMinMax: %d-%d", GetAlphaMinMax().min, GetAlphaMinMax().max, rt_alpha_min, rt_alpha_max);
#endif

	bool blend_ad_improved = false;
	const bool alpha_mask = (m_cached_ctx.FRAME.FBMSK & 0xFF000000) == 0xFF000000;

	// When AA1 is enabled and Alpha Blending is disabled, alpha blending done with coverage instead of alpha.
	// We use a COV value of 128 (full coverage) in triangles (except the edge geometry, which we can't do easily).
	if (IsCoverageAlpha())
	{
		m_conf.ps.fixed_one_a = 1;
		m_conf.ps.blend_c = 0;
	}
	else if (m_conf.ps.blend_c == 1)
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
		// Check whenever we can use rt alpha min as the new alpha value, will be more accurate.
		else if (!alpha_mask && (rt_alpha_min >= (rt_alpha_max / 2)))
		{
			AFIX = rt_alpha_min;
			m_conf.ps.blend_c = 2;
			blend_ad_improved = true;
		}
	}

	// Get alpha value
	const bool alpha_c0_zero = (m_conf.ps.blend_c == 0 && GetAlphaMinMax().max == 0);
	const bool alpha_c0_one = (m_conf.ps.blend_c == 0 && (GetAlphaMinMax().min == 128) && (GetAlphaMinMax().max == 128));
	const bool alpha_c0_high_min_one = (m_conf.ps.blend_c == 0 && GetAlphaMinMax().min > 128);
	const bool alpha_c0_high_max_one = (m_conf.ps.blend_c == 0 && GetAlphaMinMax().max > 128);
	const bool alpha_c2_zero = (m_conf.ps.blend_c == 2 && AFIX == 0u);
	const bool alpha_c2_one = (m_conf.ps.blend_c == 2 && AFIX == 128u);
	const bool alpha_c2_high_one = (m_conf.ps.blend_c == 2 && AFIX > 128u);
	const bool alpha_one = alpha_c0_one || alpha_c2_one;

	// Optimize blending equations, must be done before index calculation
	if ((m_conf.ps.blend_a == m_conf.ps.blend_b) || ((m_conf.ps.blend_b == m_conf.ps.blend_d) && alpha_one))
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
	else if (COLCLAMP.CLAMP && m_conf.ps.blend_a == 2
		&& (m_conf.ps.blend_d == 2 || (m_conf.ps.blend_b == m_conf.ps.blend_d && (alpha_c0_high_min_one || alpha_c2_high_one))))
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

	// Ad cases, alpha write is masked, one barrier is enough, for d3d11 read the fb
	// Replace Ad with As, blend flags will be used from As since we are chaging the blend_index value.
	// Must be done before index calculation, after blending equation optimizations
	const bool blend_ad = m_conf.ps.blend_c == 1;
	bool blend_ad_alpha_masked = blend_ad && alpha_mask;
	if (((GSConfig.AccurateBlendingUnit >= AccBlendLevel::Basic) || (COLCLAMP.CLAMP == 0))
		&& g_gs_device->Features().texture_barrier && blend_ad_alpha_masked)
		m_conf.ps.blend_c = 0;
	else if (((GSConfig.AccurateBlendingUnit >= AccBlendLevel::Medium)
		// Detect barrier aka fbmask on d3d11.
		|| m_conf.require_one_barrier)
		&& blend_ad_alpha_masked)
		m_conf.ps.blend_c = 0;
	else
		blend_ad_alpha_masked = false;

	u8 blend_index = static_cast<u8>(((m_conf.ps.blend_a * 3 + m_conf.ps.blend_b) * 3 + m_conf.ps.blend_c) * 3 + m_conf.ps.blend_d);
	const HWBlend blend_preliminary = GSDevice::GetBlend(blend_index, false);
	const int blend_flag = blend_preliminary.flags;

	// Re set alpha, it was modified, must be done after index calculation
	if (blend_ad_alpha_masked)
		m_conf.ps.blend_c = ALPHA.C;

	// HW blend can handle Cd output.
	bool color_dest_blend = !!(blend_flag & BLEND_CD);

	// Do the multiplication in shader for blending accumulation: Cs*As + Cd or Cs*Af + Cd
	bool accumulation_blend = !!(blend_flag & BLEND_ACCU);
	// If alpha == 1.0, almost everything is an accumulation blend!
	// Ones that use (1 + Alpha) can't guarante the mixed sw+hw blending this enables will give an identical result to sw due to clamping
	// But enable for everything else that involves dst color
	if (alpha_one && (m_conf.ps.blend_a != m_conf.ps.blend_d) && blend_preliminary.dst != GSDevice::CONST_ZERO)
		accumulation_blend = true;

	// Blending doesn't require barrier, or sampling of the rt
	const bool blend_non_recursive = !!(blend_flag & BLEND_NO_REC);

	// BLEND MIX selection, use a mix of hw/sw blending
	const bool blend_mix1 = !!(blend_flag & BLEND_MIX1) &&
							(features.dual_source_blend || !(m_conf.ps.blend_b == m_conf.ps.blend_d && (alpha_c0_high_min_one || alpha_c2_high_one)));
	const bool blend_mix2 = !!(blend_flag & BLEND_MIX2);
	const bool blend_mix3 = !!(blend_flag & BLEND_MIX3);
	bool blend_mix = (blend_mix1 || blend_mix2 || blend_mix3) && COLCLAMP.CLAMP;

	const bool one_barrier = m_conf.require_one_barrier || blend_ad_alpha_masked;

	// Blend can be done on hw. As and F cases should be accurate.
	// BLEND_HW_CLR1 with Ad, BLEND_HW_CLR3  Cs > 0.5f will require sw blend.
	// BLEND_HW_CLR1 with As/F and BLEND_HW_CLR2 can be done in hw.
	const bool clr_blend = !!(blend_flag & (BLEND_HW_CLR1 | BLEND_HW_CLR2 | BLEND_HW_CLR3));
	bool clr_blend1_2 = (blend_flag & (BLEND_HW_CLR1 | BLEND_HW_CLR2)) && (m_conf.ps.blend_c != 1) && !blend_ad_improved // Make sure it isn't an Ad case
						&& !(m_draw_env->PABE.PABE && GetAlphaMinMax().min < 128) // No PABE as it will require sw blending.
						&& (COLCLAMP.CLAMP) // Let's add a colclamp check too, hw blend will clamp to 0-1.
						&& !(one_barrier || m_conf.require_full_barrier); // Also don't run if there are barriers present.

	// Warning no break on purpose
	// Note: the [[fallthrough]] attribute tell compilers not to complain about not having breaks.
	bool sw_blending = false;
	if (features.texture_barrier)
	{
		// Condition 1: Require full sw blend for full barrier.
		// Condition 2: One barrier is already enabled, prims don't overlap so let's use sw blend instead.
		const bool prefer_sw_blend = m_conf.require_full_barrier || (one_barrier && m_prim_overlap == PRIM_OVERLAP_NO);
		const bool no_prim_overlap = (m_prim_overlap == PRIM_OVERLAP_NO);
		const bool free_blend = blend_non_recursive // Free sw blending, doesn't require barriers or reading fb
			|| accumulation_blend; // Mix of hw/sw blending
		const bool blend_requires_barrier = (blend_flag & BLEND_A_MAX) // Impossible blending
			|| (m_conf.require_full_barrier) // Another effect (for example fbmask) already requires a full barrier
			// Blend can be done in a single draw, and we already need a barrier
			// On fbfetch, one barrier is like full barrier
			|| (one_barrier && (no_prim_overlap || features.framebuffer_fetch))
			|| ((alpha_c2_high_one || alpha_c0_high_max_one) && no_prim_overlap)
			// Ad blends are completely wrong without sw blend (Ad is 0.5 not 1 for 128). We can spare a barrier for it.
			|| ((blend_ad || blend_ad_improved) && no_prim_overlap);

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
				// Enable sw blending for barriers.
				sw_blending |= blend_requires_barrier;
				// Try to do hw blend for clr2 case.
				sw_blending &= !clr_blend1_2;
				// blend_ad_improved should only run if no other barrier blend is enabled, otherwise restore bit values.
				if (blend_ad_improved && (sw_blending || prefer_sw_blend))
				{
					AFIX = 0;
					m_conf.ps.blend_c = 1;
				}
				// Enable sw blending for free blending, should be done after blend_ad_improved check.
				sw_blending |= free_blend;
				// Do not run BLEND MIX if sw blending is already present, it's less accurate.
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
				// Blending requires reading the framebuffer when there's no overlap.
				sw_blending |= fbmask_no_overlap;
				// Try to do hw blend for clr2 case.
				sw_blending &= !clr_blend1_2;
				// blend_ad_improved should only run if no other barrier blend is enabled, otherwise restore bit values.
				if (blend_ad_improved && (sw_blending || fbmask_no_overlap))
				{
					AFIX = 0;
					m_conf.ps.blend_c = 1;
				}
				// Enable sw blending for free blending, should be done after blend_ad_improved check.
				sw_blending |= accumulation_blend || blend_non_recursive;
				// Do not run BLEND MIX if sw blending is already present, it's less accurate.
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
		else if (accumulation_blend)
		{
			// A fast algo that requires 2 passes
			GL_INS("COLCLIP Fast HDR mode ENABLED");
			m_conf.ps.hdr = 1;
			sw_blending = true; // Enable sw blending for the HDR algo
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
	if (m_draw_env->PABE.PABE && GetAlphaMinMax().min < 128)
	{
		// Breath of Fire Dragon Quarter, Strawberry Shortcake, Super Robot Wars, Cartoon Network Racing, Simple 2000 Series Vol.81, SOTC, Super Robot Wars.

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
	GL_INS("BLEND_INFO: %u/%u/%u/%u. Clamp:%u. Prim:%d number %u (drawlist %zu) (sw %d)",
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

		return;
	}
	else if (sw_blending)
	{
		// Require the fix alpha vlaue
		if (m_conf.ps.blend_c == 2)
			m_conf.cb_ps.TA_MaxDepth_Af.a = static_cast<float>(AFIX) / 128.0f;

		const HWBlend blend = GSDevice::GetBlend(blend_index, replace_dual_src);
		if (accumulation_blend)
		{
			// Keep HW blending to do the addition/subtraction
			m_conf.blend = {true, GSDevice::CONST_ONE, GSDevice::CONST_ONE, blend.op, false, 0};
			blending_alpha_pass = false;

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
				pxAssert(m_conf.ps.blend_d == 2 || alpha_one);
				// A bit of normalization
				m_conf.ps.blend_a = m_conf.ps.blend_d;
				m_conf.ps.blend_d = 2;
			}

			if (blend.op == GSDevice::OP_REV_SUBTRACT)
			{
				pxAssert(m_conf.ps.blend_a == 2);
				if (m_conf.ps.hdr)
				{
					// HDR uses unorm, which is always positive
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

			// Dual source output not needed (accumulation blend replaces it with ONE).
			m_conf.ps.no_color1 = true;

			// Only Ad case will require one barrier
			// No need to set a_masked bit for blend_ad_alpha_masked case
			m_conf.require_one_barrier |= blend_ad_alpha_masked;
		}
		else if (blend_mix)
		{
			// For mixed blend, the source blend is done in the shader (so we use CONST_ONE as a factor).
			m_conf.blend = {true, GSDevice::CONST_ONE, blend.dst, blend.op, m_conf.ps.blend_c == 2, AFIX};
			m_conf.ps.blend_mix = (blend.op == GSDevice::OP_REV_SUBTRACT) ? 2 : 1;

			// Elide DSB colour output if not used by dest.
			m_conf.ps.no_color1 |= !GSDevice::IsDualSourceBlendFactor(blend.dst);

			if (blend_mix1)
			{
				if (m_conf.ps.blend_b == m_conf.ps.blend_d && (alpha_c0_high_min_one || alpha_c2_high_one))
				{
					// Replace Cs*As + Cd*(1 - As) with Cs*As - Cd*(As - 1).
					// Replace Cs*F + Cd*(1 - F) with Cs*F - Cd*(F - 1).
					// As - 1 or F - 1 subtraction is only done for the dual source output (hw blending part) since we are changing the equation.
					// Af will be replaced with As in shader and send it to dual source output.
					m_conf.blend = {true, GSDevice::CONST_ONE, GSDevice::SRC1_COLOR, GSDevice::OP_SUBTRACT, false, 0};
					// blend hw 1 will disable alpha clamp, we can reuse the old bits.
					m_conf.ps.blend_hw = 1;
					// DSB output will always be used.
					m_conf.ps.no_color1 = false;
				}
				else if (m_conf.ps.blend_a == m_conf.ps.blend_d)
				{
					// Compensate slightly for Cd*(As + 1) - Cs*As.
					// Try to compensate a bit with subtracting 1 (0.00392) * (Alpha + 1) from Cs.
					m_conf.ps.blend_hw = 2;
				}

				m_conf.ps.blend_a = 0;
				m_conf.ps.blend_b = 2;
				m_conf.ps.blend_d = 2;
			}
			else if (blend_mix2)
			{
				// Allow to compensate when Cs*(Alpha + 1) overflows, to compensate we change
				// the alpha output value for Cd*Alpha.
				m_conf.blend = {true, GSDevice::CONST_ONE, GSDevice::SRC1_COLOR, blend.op, false, 0};
				m_conf.ps.blend_hw = 3;
				m_conf.ps.no_color1 = false;

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
				// Swap Ad with As for hw blend
				m_conf.ps.a_masked = 1;
				m_conf.require_one_barrier |= true;
			}
		}
		else
		{
			// Disable HW blending
			m_conf.blend = {};
			m_conf.ps.no_color1 = true;
			replace_dual_src = false;
			blending_alpha_pass = false;

			// No need to set a_masked bit for blend_ad_alpha_masked case
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

		// Care for hw blend value, 6 is for hw/sw, sw blending used.
		if (blend_flag & BLEND_HW_CLR1)
		{
			m_conf.ps.blend_hw = 1;
		}
		else if (blend_flag & BLEND_HW_CLR2)
		{
			if (m_conf.ps.blend_c == 2)
				m_conf.cb_ps.TA_MaxDepth_Af.a = static_cast<float>(AFIX) / 128.0f;

			m_conf.ps.blend_hw = 2;
		}
		else if (blend_flag & BLEND_HW_CLR3)
		{
			m_conf.ps.blend_hw = 3;
		}

		if (blend_ad_alpha_masked)
		{
			m_conf.ps.a_masked = 1;
			m_conf.require_one_barrier |= true;
		}

		const HWBlend blend(GSDevice::GetBlend(blend_index, replace_dual_src));
		m_conf.blend = {true, blend.src, blend.dst, blend.op, m_conf.ps.blend_c == 2, AFIX};

		// Remove second color output when unused. Works around bugs in some drivers (e.g. Intel).
		m_conf.ps.no_color1 |= !GSDevice::IsDualSourceBlendFactor(m_conf.blend.src_factor) &&
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
	if (sw_blending && DATE_PRIMID && m_conf.require_full_barrier)
	{
		GL_PERF("DATE: Swap DATE_PRIMID with DATE_BARRIER");
		m_conf.require_full_barrier = true;
		DATE_PRIMID = false;
		DATE_BARRIER = true;
	}
}

__ri static constexpr bool IsRedundantClamp(u8 clamp, u32 clamp_min, u32 clamp_max, u32 tsize)
{
	// Don't shader sample when the clamp/repeat is configured to the texture size.
	// That way trilinear etc still works.
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

__ri void GSRendererHW::EmulateTextureSampler(const GSTextureCache::Target* rt, const GSTextureCache::Target* ds, GSTextureCache::Source* tex, const TextureMinMaxResult& tmm, GSTexture*& src_copy)
{
	// don't overwrite the texture when using channel shuffle, but keep the palette
	if (!m_channel_shuffle)
		m_conf.tex = tex->m_texture;
	m_conf.pal = tex->m_palette;

	// Hazard handling (i.e. reading from the current RT/DS).
	GSTextureCache::SourceRegion source_region = tex->GetRegion();
	bool target_region = (tex->IsFromTarget() && source_region.HasEither());
	GSVector2i unscaled_size = target_region ? tex->GetRegionSize() : tex->GetUnscaledSize();
	float scale = tex->GetScale();
	HandleTextureHazards(rt, ds, tex, tmm, source_region, target_region, unscaled_size, scale, src_copy);

	// Warning fetch the texture PSM format rather than the context format. The latter could have been corrected in the texture cache for depth.
	//const GSLocalMemory::psm_t &psm = GSLocalMemory::m_psm[m_cached_ctx.TEX0.PSM];
	const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[tex->m_TEX0.PSM];
	const GSLocalMemory::psm_t& cpsm = psm.pal > 0 ? GSLocalMemory::m_psm[m_cached_ctx.TEX0.CPSM] : psm;

	// Redundant clamp tests are restricted to local memory/1x sources only, if we're from a target,
	// we keep the shader clamp. See #5851 on github, and the note in Draw().
	[[maybe_unused]] static constexpr const char* clamp_modes[] = {"REPEAT", "CLAMP", "REGION_CLAMP", "REGION_REPEAT"};
	const bool redundant_wms = IsRedundantClamp(m_cached_ctx.CLAMP.WMS, m_cached_ctx.CLAMP.MINU,
													 m_cached_ctx.CLAMP.MAXU, tex->m_TEX0.TW);
	const bool redundant_wmt = IsRedundantClamp(m_cached_ctx.CLAMP.WMT, m_cached_ctx.CLAMP.MINV,
													 m_cached_ctx.CLAMP.MAXV, tex->m_TEX0.TH);
	const u8 wms = EffectiveClamp(m_cached_ctx.CLAMP.WMS, !tex->m_target && (source_region.HasX() || redundant_wms));
	const u8 wmt = EffectiveClamp(m_cached_ctx.CLAMP.WMT, !tex->m_target && (source_region.HasY() || redundant_wmt));
	const bool complex_wms_wmt = !!((wms | wmt) & 2) || target_region;
	GL_CACHE("FST: %s WMS: %s [%s%s] WMT: %s [%s%s] Complex: %d TargetRegion: %d MINU: %d MAXU: %d MINV: %d MAXV: %d",
		PRIM->FST ? "UV" : "STQ", clamp_modes[m_cached_ctx.CLAMP.WMS], redundant_wms ? "redundant," : "",
		clamp_modes[wms], clamp_modes[m_cached_ctx.CLAMP.WMT], redundant_wmt ? "redundant," : "", clamp_modes[wmt],
		complex_wms_wmt, target_region, m_cached_ctx.CLAMP.MINU, m_cached_ctx.CLAMP.MAXU, m_cached_ctx.CLAMP.MINV,
		m_cached_ctx.CLAMP.MAXV);

	const bool need_mipmap = IsMipMapDraw();
	const bool shader_emulated_sampler = tex->m_palette || (tex->m_target && !m_conf.ps.shuffle && cpsm.fmt != 0) ||
										 complex_wms_wmt || psm.depth || target_region;
	const bool trilinear_manual = need_mipmap && GSConfig.HWMipmap == HWMipmapLevel::Full;

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
			trilinear = static_cast<u8>(GS_MIN_FILTER::Linear_Mipmap_Linear);
			trilinear_auto = !tex->m_target && (!need_mipmap || GSConfig.HWMipmap != HWMipmapLevel::Full);
		}
		break;

		case TriFiltering::PS2:
		{
			// Can only use PS2 trilinear when mipmapping is enabled.
			if (need_mipmap && GSConfig.HWMipmap != HWMipmapLevel::Off)
			{
				trilinear = m_context->TEX1.MMIN;
				trilinear_auto = !tex->m_target && GSConfig.HWMipmap != HWMipmapLevel::Full;
			}
		}
		break;

		case TriFiltering::Automatic:
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
		GL_INS("WARNING: Depth + bilinear filtering not supported");

	// Performance note:
	// 1/ Don't set 0 as it is the default value
	// 2/ Only keep aem when it is useful (avoid useless shader permutation)
	if (m_conf.ps.shuffle)
	{
		const GIFRegTEXA& TEXA = m_draw_env->TEXA;

		// Force a 32 bits access (normally shuffle is done on 16 bits)
		// m_ps_sel.tex_fmt = 0; // removed as an optimization

		//ASSERT(tex->m_target);

		// Require a float conversion if the texure is a depth otherwise uses Integral scaling
		if (psm.depth)
		{
			m_conf.ps.depth_fmt = (tex->m_texture->GetType() != GSTexture::Type::DepthStencil) ? 3 : tex->m_32_bits_fmt ? 1 : 2;
		}

		// Shuffle is a 16 bits format, so aem is always required
		if (m_cached_ctx.TEX0.TCC)
		{
			m_conf.ps.aem = TEXA.AEM;
			GSVector4 ta(TEXA & GSVector4i::x000000ff());
			ta /= 255.0f;
			m_conf.cb_ps.TA_MaxDepth_Af.x = ta.x;
			m_conf.cb_ps.TA_MaxDepth_Af.y = ta.y;
		}
		else
		{
			m_conf.cb_ps.TA_MaxDepth_Af.x = 0;
			m_conf.cb_ps.TA_MaxDepth_Af.y = 1.0f;
		}

		// The purpose of texture shuffle is to move color channel. Extra interpolation is likely a bad idea.
		bilinear &= m_vt.IsLinear();

		const GSVector4 half_pixel = RealignTargetTextureCoordinate(tex);
		m_conf.cb_vs.texture_offset = GSVector2(half_pixel.x, half_pixel.y);
	}
	else if (tex->m_target)
	{
		const GIFRegTEXA& TEXA = m_draw_env->TEXA;

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
	m_conf.ps.point_sampler = g_gs_device->Features().broken_point_sampler && !target_region && (!bilinear || shader_emulated_sampler);

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
		const GSVector4 region_clamp_offset =
			(tex->GetScale() != 1.0f) ? GSVector4::cxpr(0.0f, 0.0f, 0.0f, 0.0f) : GSVector4::cxpr(0.0f, 0.0f, 0.5f, 0.5f);
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
	else if (trilinear_manual)
	{
		// Reuse uv_min_max for mipmap parameter to avoid an extension of the UBO
		m_conf.cb_ps.MinMax.x = static_cast<float>(m_context->TEX1.K) / 16.0f;
		m_conf.cb_ps.MinMax.y = static_cast<float>(1 << m_context->TEX1.L);
		m_conf.cb_ps.MinMax.z = static_cast<float>(m_lod.x); // Offset because first layer is m_lod, dunno if we can do better
		m_conf.cb_ps.MinMax.w = static_cast<float>(m_lod.y);
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

	// Only enable clamping in CLAMP mode. REGION_CLAMP will be done manually in the shader
	m_conf.sampler.tau = (wms == CLAMP_REPEAT && !target_region);
	m_conf.sampler.tav = (wmt == CLAMP_REPEAT && !target_region);
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
}

__ri void GSRendererHW::HandleTextureHazards(const GSTextureCache::Target* rt, const GSTextureCache::Target* ds,
	const GSTextureCache::Source* tex, const TextureMinMaxResult& tmm, GSTextureCache::SourceRegion& source_region,
	bool& target_region, GSVector2i& unscaled_size, float& scale, GSTexture*& src_copy)
{
	// Detect framebuffer read that will need special handling
	const GSTextureCache::Target* src_target = nullptr;
	if (m_conf.tex == m_conf.rt)
	{
		// Can we read the framebuffer directly? (i.e. sample location matches up).
		if (CanUseTexIsFB(rt, tex, tmm))
		{
			m_conf.tex = nullptr;
			m_conf.ps.tex_is_fb = true;
			if (m_prim_overlap == PRIM_OVERLAP_NO || !g_gs_device->Features().texture_barrier)
				m_conf.require_one_barrier = true;
			else
				m_conf.require_full_barrier = true;

			unscaled_size = rt->GetUnscaledSize();
			scale = rt->GetScale();
			return;
		}

		GL_CACHE("Source is render target, taking copy.");
		src_target = rt;
	}
	else if (m_conf.tex == m_conf.ds)
	{
		// GL, Vulkan (in General layout), not DirectX!
		const bool can_read_current_depth_buffer = g_gs_device->Features().test_and_sample_depth;

		// If this is our current Z buffer, we might not be able to read it directly if it's being written to.
		// Rather than leaving the backend to do it, we'll check it here.
		if (can_read_current_depth_buffer && (m_cached_ctx.ZBUF.ZMSK || m_cached_ctx.TEST.ZTST == ZTST_NEVER))
		{
			// Safe to read!
			GL_CACHE("Source is depth buffer, not writing, safe to read.");
			unscaled_size = ds->GetUnscaledSize();
			scale = ds->GetScale();
			return;
		}

		// Can't safely read the depth buffer, so we need to take a copy of it.
		GL_CACHE("Source is depth buffer, unsafe to read, taking copy.");
		src_target = ds;
	}
	else
	{
		// No match.
		return;
	}

	// We need to copy. Try to cut down the source range as much as possible so we don't copy texels we're not reading.
	const GSVector2i& src_unscaled_size = src_target->GetUnscaledSize();
	const GSVector4i src_bounds = src_target->GetUnscaledRect();
	GSVector4i copy_range;
	GSVector2i copy_size;
	GSVector2i copy_dst_offset;

	// Shuffles take the whole target. This should've already been halved.
	// We can't partially copy depth targets in DirectX, and GL/Vulkan should use the direct read above.
	// Restricting it also breaks Tom and Jerry...
	if (m_channel_shuffle || tex->m_texture->GetType() == GSTexture::Type::DepthStencil)
	{
		copy_range = src_bounds;
		copy_size = src_unscaled_size;
		GSVector4i::storel(&copy_dst_offset, copy_range);
	}
	else
	{
		// If we're using TW/TH-based sizing, take the size from TEX0, not the target.
		const GSVector2i tex_size = GSVector2i(1 << m_cached_ctx.TEX0.TW, 1 << m_cached_ctx.TEX0.TH);
		copy_size.x = std::min(tex_size.x, src_unscaled_size.x);
		copy_size.y = std::min(tex_size.y, src_unscaled_size.y);

		// Use the texture min/max to get the copy range.
		copy_range = tmm.coverage;

		// Texture size above might be invalid (Timesplitters 2), extend if needed.
		if (m_cached_ctx.CLAMP.WMS >= CLAMP_REGION_CLAMP && copy_range.z > copy_size.x)
			copy_size.x = src_unscaled_size.x;
		if (m_cached_ctx.CLAMP.WMT >= CLAMP_REGION_CLAMP && copy_range.w > copy_size.y)
			copy_size.y = src_unscaled_size.y;

		// Texture shuffles might read up to +/- 8 pixels on either side.
		if (m_texture_shuffle)
			copy_range = (copy_range + GSVector4i::cxpr(-8, 0, 8, 0)).max_i32(GSVector4i::zero());

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
			GL_CACHE("Applying target region at copy: %dx%d @ %d,%d => %d,%d", copy_range.width(), copy_range.height(),
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
		GL_CACHE("ERROR: Reading outside of the RT range, using null texture.");
		unscaled_size = GSVector2i(1, 1);
		scale = 1.0f;
		m_conf.tex = nullptr;
		m_conf.ps.tfx = 4;
		return;
	}

	unscaled_size = copy_size;
	scale = src_target->GetScale();
	GL_CACHE("Copy size: %dx%d, range: %d,%d -> %d,%d (%dx%d) @ %.1f", copy_size.x, copy_size.y, copy_range.x,
		copy_range.y, copy_range.z, copy_range.w, copy_range.width(), copy_range.height(), scale);

	const GSVector2i scaled_copy_size = GSVector2i(static_cast<int>(std::ceil(static_cast<float>(copy_size.x) * scale)),
		static_cast<int>(std::ceil(static_cast<float>(copy_size.y) * scale)));
	const GSVector4i scaled_copy_range = GSVector4i((GSVector4(copy_range) * GSVector4(scale)).ceil());
	const GSVector2i scaled_copy_dst_offset =
		GSVector2i(static_cast<int>(std::ceil(static_cast<float>(copy_dst_offset.x) * scale)),
			static_cast<int>(std::ceil(static_cast<float>(copy_dst_offset.y) * scale)));

	src_copy = src_target->m_texture->IsDepthStencil() ?
				   g_gs_device->CreateDepthStencil(
					   scaled_copy_size.x, scaled_copy_size.y, src_target->m_texture->GetFormat(), false) :
				   g_gs_device->CreateTexture(
					   scaled_copy_size.x, scaled_copy_size.y, 1, src_target->m_texture->GetFormat(), true);
	if (!src_copy) [[unlikely]]
	{
		Console.Error("Failed to allocate %dx%d texture for hazard copy", scaled_copy_size.x, scaled_copy_size.y);
		m_conf.tex = nullptr;
		m_conf.ps.tfx = 4;
		return;
	}
	g_gs_device->CopyRect(
		src_target->m_texture, src_copy, scaled_copy_range, scaled_copy_dst_offset.x, scaled_copy_dst_offset.y);
	m_conf.tex = src_copy;
}

bool GSRendererHW::CanUseTexIsFB(const GSTextureCache::Target* rt, const GSTextureCache::Source* tex,
	const TextureMinMaxResult& tmm)
{
	// Minimum blending or no barriers -> we can't use tex-is-fb.
	if (GSConfig.AccurateBlendingUnit == AccBlendLevel::Minimum || !g_gs_device->Features().texture_barrier)
	{
		GL_CACHE("Can't use tex-is-fb due to no barriers.");
		return false;
	}

	// If we're a shuffle, tex-is-fb is always fine.
	if (m_texture_shuffle || m_channel_shuffle)
	{
		GL_CACHE("Activating tex-is-fb for %s shuffle.", m_texture_shuffle ? "texture" : "channel");
		return true;
	}

	static constexpr auto check_clamp = [](u32 clamp, u32 min, u32 max, s32 tmin, s32 tmax) {
		if (clamp == CLAMP_REGION_CLAMP)
		{
			if (tmin < static_cast<s32>(min) || tmax > static_cast<s32>(max + 1))
			{
				GL_CACHE("Can't use tex-is-fb because of REGION_CLAMP [%d, %d] with TMM of [%d, %d]", min, max, tmin, tmax);
				return false;
			}
		}
		else if (clamp == CLAMP_REGION_REPEAT)
		{
			const u32 req_tbits = (tmax > 1) ? (std::bit_ceil(static_cast<u32>(tmax - 1)) - 1) : 0x1;
			if ((min & req_tbits) != req_tbits)
			{
				GL_CACHE("Can't use tex-is-fb because of REGION_REPEAT [%d, %d] with TMM of [%d, %d] and tbits of %d",
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
			GL_CACHE("Can't use tex-is-fb due to bilinear sampling.");
			return false;
		}

		// Can't do tex-is-fb if paletted and we're not a shuffle (C32 -> P8).
		// This one shouldn't happen anymore, because all conversion should be done already.
		const GSLocalMemory::psm_t& tex_psm = GSLocalMemory::m_psm[tex->m_TEX0.PSM];
		const GSLocalMemory::psm_t& rt_psm = GSLocalMemory::m_psm[rt->m_TEX0.PSM];
		if (tex_psm.pal > 0 && tex_psm.bpp < rt_psm.bpp)
		{
			Console.Error("Draw %d: Can't use tex-is-fb due to palette conversion", s_n);
			return true;
		}

		// Make sure that we're not sampling away from the area we're rendering.
		// We need to take the absolute here, because Beyond Good and Evil undithers itself using a -1,-1 offset.
		const GSVector4 diff(m_vt.m_min.p.upld(m_vt.m_max.p) - m_vt.m_min.t.upld(m_vt.m_max.t));
		GL_CACHE("Coord diff: %f,%f", diff.x, diff.y);
		if ((diff.abs() < GSVector4(1.0f)).alltrue())
		{
			GL_CACHE("Sampling from rendered texel, using tex-is-fb.");
			return true;
		}

		GL_CACHE("Coord diff too large, not using tex-is-fb.");
		return false;
	}

	if (m_vt.m_primclass == GS_TRIANGLE_CLASS)
	{
		// This pattern is used by several games to emulate a stencil (shadow)
		// Ratchet & Clank, Jak do alpha integer multiplication (tfx) which is mostly equivalent to +1/-1
		// Tri-Ace (Star Ocean 3/RadiataStories/VP2) uses a palette to handle the +1/-1
		if (m_cached_ctx.FRAME.FBMSK == 0x00FFFFFF)
		{
			GL_CACHE("Tex-is-fb hack for Jak");
			return true;
		}

		GL_CACHE("Triangle draw, not using tex-is-fb");
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
	memset(&m_conf, 0, reinterpret_cast<const char*>(&m_conf.cb_vs) - reinterpret_cast<const char*>(&m_conf));
}

__ri void GSRendererHW::DrawPrims(GSTextureCache::Target* rt, GSTextureCache::Target* ds, GSTextureCache::Source* tex, const TextureMinMaxResult& tmm)
{
#ifdef ENABLE_OGL_DEBUG
	const GSVector4i area_out = GSVector4i(m_vt.m_min.p.upld(m_vt.m_max.p)).rintersect(m_context->scissor.in);
	const GSVector4i area_in = GSVector4i(m_vt.m_min.t.upld(m_vt.m_max.t));

	GL_PUSH("GL Draw from (area %d,%d => %d,%d) in (area %d,%d => %d,%d)",
		area_in.x, area_in.y, area_in.z, area_in.w,
		area_out.x, area_out.y, area_out.z, area_out.w);
#endif

	const GSDrawingEnvironment& env = *m_draw_env;
	bool DATE = m_cached_ctx.TEST.DATE && m_cached_ctx.FRAME.PSM != PSMCT24;
	bool DATE_PRIMID = false;
	bool DATE_BARRIER = false;
	bool DATE_one = false;

	const bool ate_first_pass = m_cached_ctx.TEST.DoFirstPass();
	const bool ate_second_pass = m_cached_ctx.TEST.DoSecondPass();

	ResetStates();

	const float scale_factor = rt ? rt->GetScale() : ds->GetScale();
	m_conf.cb_vs.texture_offset = {};
	m_conf.cb_ps.ScaleFactor = GSVector4(scale_factor * (1.0f / 16.0f), 1.0f / scale_factor, scale_factor, 0.0f);
	m_conf.ps.scanmsk = env.SCANMSK.MSK;
	m_conf.rt = rt ? rt->m_texture : nullptr;
	m_conf.ds = ds ? ds->m_texture : nullptr;

	// Z setup has to come before channel shuffle
	EmulateZbuffer(ds);

	// HLE implementation of the channel selection effect
	//
	// Warning it must be done at the begining because it will change the
	// vertex list (it will interact with PrimitiveOverlap and accurate
	// blending)
	if (m_channel_shuffle && tex && tex->m_from_target)
		EmulateChannelShuffle(tex->m_from_target, false);

	// Upscaling hack to avoid various line/grid issues
	MergeSprite(tex);

	m_prim_overlap = PrimitiveOverlap();

	EmulateTextureShuffleAndFbmask(rt, tex);

	const GSDevice::FeatureSupport features = g_gs_device->Features();

	// Blend
	int blend_alpha_min = 0, blend_alpha_max = 255;
	if (rt)
	{
		blend_alpha_min = rt->m_alpha_min;
		blend_alpha_max = rt->m_alpha_max;

		const bool is_24_bit = (GSLocalMemory::m_psm[rt->m_TEX0.PSM].trbpp == 24);
		if (is_24_bit)
		{
			// C24/Z24 - alpha is 1.
			blend_alpha_min = 128;
			blend_alpha_max = 128;
		}

		if (!m_channel_shuffle && !m_texture_shuffle)
		{
			const int fba_value = m_prev_env.CTXT[m_prev_env.PRIM.CTXT].FBA.FBA * 128;
			if ((m_cached_ctx.FRAME.FBMSK & 0xff000000) == 0)
			{
				if (rt->m_valid.rintersect(m_r).eq(rt->m_valid) && PrimitiveCoversWithoutGaps() && !(m_cached_ctx.TEST.DATE || m_cached_ctx.TEST.ATE || m_cached_ctx.TEST.ZTST != ZTST_ALWAYS))
				{
					rt->m_alpha_max = GetAlphaMinMax().max | fba_value;
					rt->m_alpha_min = GetAlphaMinMax().min | fba_value;
				}
				else
				{
					rt->m_alpha_max = std::max(GetAlphaMinMax().max | fba_value, rt->m_alpha_max);
					rt->m_alpha_min = std::min(GetAlphaMinMax().min | fba_value, rt->m_alpha_min);
				}
			}
			else if ((m_cached_ctx.FRAME.FBMSK & 0xff000000) != 0xff000000) // We can't be sure of the alpha if it's partially masked.
			{
				rt->m_alpha_max |= std::max(GetAlphaMinMax().max | fba_value, rt->m_alpha_max);
				rt->m_alpha_min = std::min(GetAlphaMinMax().min | fba_value, rt->m_alpha_min);
			}
			else if (!is_24_bit)
			{
				// If both are zero then we probably don't know what the alpha is.
				if (rt->m_alpha_max == 0 && rt->m_alpha_min == 0)
				{
					rt->m_alpha_max = 255;
					rt->m_alpha_min = 0;
				}
			}
		}
		else if ((m_texture_shuffle && m_conf.ps.write_rg == false) || m_channel_shuffle)
		{
			rt->m_alpha_max = 255;
			rt->m_alpha_min = 0;
		}

		GL_INS("RT Alpha Range: %d-%d => %d-%d", blend_alpha_min, blend_alpha_max, rt->m_alpha_min, rt->m_alpha_max);

		// If there's no overlap, the values in the RT before FB write will be the old values.
		if (m_prim_overlap != PRIM_OVERLAP_NO)
		{
			// Otherwise, it may be a mix of the old/new values.
			blend_alpha_min = std::min(blend_alpha_min, rt->m_alpha_min);
			blend_alpha_max = std::max(blend_alpha_max, rt->m_alpha_max);
		}

		if (!rt->m_32_bits_fmt)
		{
			rt->m_alpha_max &= 128;
			rt->m_alpha_min &= 128;
		}
	}

	// DATE: selection of the algorithm. Must be done before blending because GL42 is not compatible with blending
	if (DATE)
	{
		if (m_cached_ctx.TEST.DATM)
		{
			if (rt)
			{
				// Destination and incoming pixels are all 1 or higher, no need for DATE.
				if ((rt->m_alpha_min >= 128 || (m_cached_ctx.FRAME.FBMSK & 0x80000000)) && blend_alpha_min >= 128)
				{
					DATE = false;
					m_cached_ctx.TEST.DATE = false;
				}
				else if (blend_alpha_max < 128) // All dest pixels are less than 1, everything fails.
				{
					rt->m_alpha_max = blend_alpha_max;
					rt->m_alpha_min = blend_alpha_min;
					return;
				}
			}
		}
		else
		{
			if (rt)
			{
				// Destination and incoming pixels are all less than 1, no need for DATE.
				if ((rt->m_alpha_max < 128 || (m_cached_ctx.FRAME.FBMSK & 0x80000000)) && blend_alpha_max < 128)
				{
					DATE = false;
					m_cached_ctx.TEST.DATE = false;
				}
				else if (blend_alpha_min >= 128) // All dest pixels are 1 or higher, everything fails.
				{
					rt->m_alpha_max = blend_alpha_max;
					rt->m_alpha_min = blend_alpha_min;
					return;
				}
			}
		}

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
				else if (features.texture_barrier && ((m_vt.m_primclass == GS_SPRITE_CLASS && m_drawlist.size() < 10) || (m_index.tail < 30)))
				{
					// texture barrier will split the draw call into n draw call. It is very efficient for
					// few primitive draws. Otherwise it sucks.
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
		ds->m_alpha_max = std::max(ds->m_alpha_max, static_cast<int>(m_vt.m_max.p.z) >> 24);
		ds->m_alpha_min = std::min(ds->m_alpha_min, static_cast<int>(m_vt.m_min.p.z) >> 24);
		GL_INS("New DS Alpha Range: %d-%d", ds->m_alpha_min, ds->m_alpha_max);

		if (GSLocalMemory::m_psm[ds->m_TEX0.PSM].bpp == 16)
		{
			ds->m_alpha_max &= 128;
			ds->m_alpha_min &= 128;
		}
	}

	bool blending_alpha_pass = false;
	if ((!IsOpaque() || m_context->ALPHA.IsBlack()) && rt && ((m_conf.colormask.wrgba & 0x7) || (m_texture_shuffle && !m_copy_16bit_to_target_shuffle && !m_same_group_texture_shuffle)))
	{
		EmulateBlending(blend_alpha_min, blend_alpha_max, DATE_PRIMID, DATE_BARRIER, blending_alpha_pass);
	}
	else
	{
		m_conf.blend = {}; // No blending please
		m_conf.ps.no_color1 = true;
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

	m_conf.datm = m_cached_ctx.TEST.DATM;

	// If we're doing stencil DATE and we don't have a depth buffer, we need to allocate a temporary one.
	GSTexture* temp_ds = nullptr;
	if (m_conf.destination_alpha >= GSHWDrawConfig::DestinationAlphaMode::Stencil &&
		m_conf.destination_alpha <= GSHWDrawConfig::DestinationAlphaMode::StencilOne && !m_conf.ds)
	{
		temp_ds = g_gs_device->CreateDepthStencil(m_conf.rt->GetWidth(), m_conf.rt->GetHeight(), GSTexture::Format::DepthStencil, false);
		m_conf.ds = temp_ds;
	}

	// vs

	m_conf.vs.tme = m_process_texture;
	m_conf.vs.fst = PRIM->FST;

	// FIXME D3D11 and GL support half pixel center. Code could be easier!!!
	const GSTextureCache::Target* rt_or_ds = rt ? rt : ds;
	const GSVector2i rtsize = rt_or_ds->GetTexture()->GetSize();
	const float rtscale = rt_or_ds->GetScale();
	float sx, sy, ox, oy, ox2, oy2;
	if (GSConfig.UserHacks_HalfPixelOffset != GSHalfPixelOffset::Native)
	{
		sx = 2.0f * rtscale / (rtsize.x << 4);
		sy = 2.0f * rtscale / (rtsize.y << 4);
		ox = static_cast<float>(static_cast<int>(m_context->XYOFFSET.OFX));
		oy = static_cast<float>(static_cast<int>(m_context->XYOFFSET.OFY));
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
		sx = 2.0f / (rt_or_ds->GetUnscaledWidth() << 4);
		sy = 2.0f / (rt_or_ds->GetUnscaledHeight() << 4);
		ox = static_cast<float>(static_cast<int>(m_context->XYOFFSET.OFX));
		oy = static_cast<float>(static_cast<int>(m_context->XYOFFSET.OFY));
		ox2 = -1.0f / rt_or_ds->GetUnscaledWidth();
		oy2 = -1.0f / rt_or_ds->GetUnscaledHeight();
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
		if (features.texture_barrier)
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

	if (m_conf.ps.dither)
	{
		const GIFRegDIMX& DIMX = m_draw_env->DIMX;
		GL_DBG("DITHERING mode ENABLED (%d)", GSConfig.Dithering);

		m_conf.ps.dither = GSConfig.Dithering;
		m_conf.cb_ps.DitherMatrix[0] = GSVector4(DIMX.DM00, DIMX.DM01, DIMX.DM02, DIMX.DM03);
		m_conf.cb_ps.DitherMatrix[1] = GSVector4(DIMX.DM10, DIMX.DM11, DIMX.DM12, DIMX.DM13);
		m_conf.cb_ps.DitherMatrix[2] = GSVector4(DIMX.DM20, DIMX.DM21, DIMX.DM22, DIMX.DM23);
		m_conf.cb_ps.DitherMatrix[3] = GSVector4(DIMX.DM30, DIMX.DM31, DIMX.DM32, DIMX.DM33);
	}

	if (PRIM->FGE)
	{
		m_conf.ps.fog = 1;

		const GSVector4 fc = GSVector4::rgba32(m_draw_env->FOGCOL.U32[0]);
		// Blend AREF to avoid to load a random value for alpha (dirty cache)
		m_conf.cb_ps.FogColor_AREF = fc.blend32<8>(m_conf.cb_ps.FogColor_AREF);
	}

	// Warning must be done after EmulateZbuffer
	// Depth test is always true so it can be executed in 2 passes (no order required) unlike color.
	// The idea is to compute first the color which is independent of the alpha test. And then do a 2nd
	// pass to handle the depth based on the alpha test.
	bool ate_RGBA_then_Z = false;
	bool ate_RGB_then_ZA = false;
	if (ate_first_pass && ate_second_pass)
	{
		GL_DBG("Complex Alpha Test");
		const bool commutative_depth = (m_conf.depth.ztst == ZTST_GEQUAL && m_vt.m_eq.z) || (m_conf.depth.ztst == ZTST_ALWAYS);
		const bool commutative_alpha = (m_context->ALPHA.C != 1); // when either Alpha Src or a constant

		ate_RGBA_then_Z = m_cached_ctx.TEST.GetAFAIL(m_cached_ctx.FRAME.PSM) == AFAIL_FB_ONLY && commutative_depth;
		ate_RGB_then_ZA = m_cached_ctx.TEST.GetAFAIL(m_cached_ctx.FRAME.PSM) == AFAIL_RGB_ONLY && commutative_depth && commutative_alpha;
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

	GSTexture* tex_copy = nullptr;
	if (tex)
	{
		EmulateTextureSampler(rt, ds, tex, tmm, tex_copy);
	}
	else
	{
		m_conf.ps.tfx = 4;
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
	pxAssert(!m_conf.require_full_barrier || !m_conf.ps.hdr);

	// Swap full barrier for one barrier when there's no overlap.
	if (m_conf.require_full_barrier && m_prim_overlap == PRIM_OVERLAP_NO)
	{
		m_conf.require_full_barrier = false;
		m_conf.require_one_barrier = true;
	}

	// rs
	const GSVector4i hacked_scissor = m_channel_shuffle ? GSVector4i::cxpr(0, 0, 1024, 1024) : m_context->scissor.in;
	const GSVector4i scissor(GSVector4i(GSVector4(rtscale) * GSVector4(hacked_scissor)).rintersect(GSVector4i::loadh(rtsize)));

	m_conf.drawarea = m_channel_shuffle ? scissor : scissor.rintersect(ComputeBoundingBox(rtsize, rtscale));
	m_conf.scissor = (DATE && !DATE_BARRIER) ? m_conf.drawarea : scissor;

	SetupIA(rtscale, sx, sy);

	m_conf.alpha_second_pass.enable = ate_second_pass;

	if (ate_second_pass)
	{
		pxAssert(!env.PABE.PABE);
		memcpy(&m_conf.alpha_second_pass.ps,        &m_conf.ps,        sizeof(m_conf.ps));
		memcpy(&m_conf.alpha_second_pass.colormask, &m_conf.colormask, sizeof(m_conf.colormask));
		memcpy(&m_conf.alpha_second_pass.depth,     &m_conf.depth,     sizeof(m_conf.depth));

		if (ate_RGBA_then_Z || ate_RGB_then_ZA)
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
		const int fail_type = m_cached_ctx.TEST.GetAFAIL(m_cached_ctx.FRAME.PSM);
		switch (fail_type)
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
		else if (ate_RGB_then_ZA)
		{
			z = !m_cached_ctx.ZBUF.ZMSK;
			a = (m_cached_ctx.FRAME.FBMSK & 0xFF000000) != 0xFF000000;
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
		{
			CleanupDraw(true);
			return;
		}

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

	if (tex_copy)
		g_gs_device->Recycle(tex_copy);
	if (temp_ds)
		g_gs_device->Recycle(temp_ds);
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

			const GSTextureCache::Target* tgt = g_texture_cache->FindOverlappingTarget(
				m_cached_ctx.TEX0.TBP0, m_cached_ctx.TEX0.TBW, m_cached_ctx.TEX0.PSM, r);
			if (tgt)
			{
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
					GL_INS("GPU clut is enabled and this draw would readback, leaving on GPU");
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
	//DevCon.Warning("Draw width %f height %f page width %f height %f TPSM %x TBP0 %x FPSM %x FBP %x CBP %x valid size %d Invalid %d DISPFB0 %x DISPFB1 %x start %x end %x draw %d", draw_width, draw_height, page_width, page_height, m_cached_ctx.TEX0.PSM, m_cached_ctx.TEX0.TBP0, m_FRAME.PSM, m_FRAME.Block(), m_mem.m_clut.GetCLUTCBP(), valid_size, m_mem.m_clut.IsInvalid(), m_regs->DISP[0].DISPFB.Block(), m_regs->DISP[1].DISPFB.Block(), startbp, endbp, s_n);

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

	if (PRIM->ABE)
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

	// Make sure this isn't something we've actually rendered to (e.g. a texture shuffle).
	if (PRIM->TME)
	{
		GSTextureCache::Target* src_target = g_texture_cache->GetTargetWithSharedBits(m_cached_ctx.TEX0.TBP0, m_cached_ctx.TEX0.PSM);
		if (src_target)
		{
			// If the EE has written over our sample area, we're fine to do this on the CPU, despite the target.
			if (!src_target->m_dirty.empty())
			{
				const GSVector4i tr(GetTextureMinMax(m_cached_ctx.TEX0, m_cached_ctx.CLAMP, m_vt.IsLinear(), false).coverage);
				for (GSDirtyRect& rc : src_target->m_dirty)
				{
					if (!rc.GetDirtyRect(m_cached_ctx.TEX0, false).rintersect(tr).rempty())
						return true;
				}
			}

			return false;
		}
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
	const bool z_is_frame = (m_cached_ctx.FRAME.FBP == m_cached_ctx.ZBUF.ZBP ||
								(m_cached_ctx.FRAME.FBW > 1 && single_page_offset)) && // GT4O Public Beta
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

	GL_INS("DetectStripedDoubleClear(): %d,%d => %d,%d @ FBP %x FBW %u ZBP %x", m_r.x, m_r.y, m_r.z, m_r.w,
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
		return false;

	// Z and color must be constant and the same
	GSVertex* v = &m_vertex.buff[0];
	if (m_vt.m_eq.rgba != 0xFFFF || !m_vt.m_eq.z || v[1].XYZ.Z != v[1].RGBAQ.U32[0])
		return false;

	// Frame and depth pointer can be inverted
	const bool clear_depth = (m_cached_ctx.FRAME.FBP > m_cached_ctx.ZBUF.ZBP);
	const u32 base = clear_depth ? m_cached_ctx.ZBUF.ZBP : m_cached_ctx.FRAME.FBP;
	const u32 half = clear_depth ? m_cached_ctx.FRAME.FBP : m_cached_ctx.ZBUF.ZBP;
	const bool enough_bits = clear_depth ? (frame_psm.trbpp >= zbuf_psm.trbpp) : (zbuf_psm.trbpp >= frame_psm.trbpp);

	// Size of the current draw
	const u32 w_pages = (m_r.z + (frame_psm.pgs.x - 1)) / frame_psm.pgs.x;
	const u32 h_pages = (m_r.w + (frame_psm.pgs.y - 1)) / frame_psm.pgs.y;
	const u32 written_pages = w_pages * h_pages;

	// If both buffers are side by side we can expect a fast clear in on-going
	if (half > (base + written_pages) || half <= base)
		return false;

	GSTextureCache::Target* half_point = g_texture_cache->GetExactTarget(half << 5, m_cached_ctx.FRAME.FBW, clear_depth ? GSTextureCache::RenderTarget : GSTextureCache::DepthStencil, half << 5);
	if (half_point && half_point->m_age <= 1)
		return false;

	// Don't allow double half clear to go through when the number of bits written through FRAME and Z are different.
	// GTA: LCS does this setup, along with a few other games. Thankfully if it's a zero clear, we'll clear both
	// separately, and the end result is the same because it gets invalidated. That's better than falsely detecting
	// double half clears, and ending up with 1024 high render targets which really shouldn't be.
	if ((!enough_bits && frame_psm.fmt != zbuf_psm.fmt && m_cached_ctx.FRAME.FBMSK != ((zbuf_psm.fmt == 1) ? 0xFF000000u : 0)) ||
		!GSUtil::HasCompatibleBits(m_cached_ctx.FRAME.PSM & ~0x30, m_cached_ctx.ZBUF.PSM & ~0x30)) // Bit depth is not the same (i.e. 32bit + 16bit).
	{
		GL_INS("Inconsistent FRAME [%s, %08x] and ZBUF [%s] formats, not using double-half clear.",
			psm_str(m_cached_ctx.FRAME.PSM), m_cached_ctx.FRAME.FBMSK, psm_str(m_cached_ctx.ZBUF.PSM));
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
	bool horizontal = false;
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
			base * BLOCKS_PER_PAGE, clear_depth ? m_cached_ctx.ZBUF.PSM : m_cached_ctx.FRAME.PSM);
		if (!tgt)
		{
			tgt = g_texture_cache->GetTargetWithSharedBits(
				base * BLOCKS_PER_PAGE, clear_depth ? m_cached_ctx.FRAME.PSM : m_cached_ctx.ZBUF.PSM);
		}

		u32 end_block = ((half + written_pages) * BLOCKS_PER_PAGE) - 1;

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
		if ((((half + written_pages) * BLOCKS_PER_PAGE) - 1) > end_block)
		{
			return false;
		}
	}

	GL_INS("DetectDoubleHalfClear(): Clearing %s %s, fbp=%x, zbp=%x, pages=%u, base=%x, half=%x, rect=(%d,%d=>%d,%d)",
		clear_depth ? "depth" : "color", horizontal ? "horizontally" : "vertically", m_cached_ctx.FRAME.Block(),
		m_cached_ctx.ZBUF.Block(), written_pages, base * BLOCKS_PER_PAGE, half * BLOCKS_PER_PAGE, m_r.x, m_r.y, m_r.z,
		m_r.w);

	// Double the clear rect.
	if (horizontal)
	{
		const int width = m_r.width();
		m_cached_ctx.FRAME.FBW *= 2;
		m_r.z = (w_pages * frame_psm.pgs.x);
		m_r.z += m_r.x + width;
	}
	else
	{
		const int height = m_r.height();
		m_r.w = ((half - base) / m_cached_ctx.FRAME.FBW) * frame_psm.pgs.y;
		m_r.w += m_r.y + height;
	}
	ReplaceVerticesWithSprite(m_r, GSVector2i(1, 1));

	// Prevent wasting time looking up and creating the target which is getting blown away.
	if (frame_psm.trbpp >= zbuf_psm.trbpp)
	{
		SetNewFRAME(base * BLOCKS_PER_PAGE, m_cached_ctx.FRAME.FBW, m_cached_ctx.FRAME.PSM);
		m_cached_ctx.ZBUF.ZMSK = true;
		no_rt = false;
		no_ds = true;
	}
	else
	{
		SetNewZBUF(base * BLOCKS_PER_PAGE, m_cached_ctx.ZBUF.PSM);
		m_cached_ctx.FRAME.FBMSK = 0xFFFFFFFF;
		no_rt = true;
		no_ds = false;
	}

	// Remove any targets at the half-buffer point, they're getting overwritten.
	g_texture_cache->InvalidateVideoMemType(GSTextureCache::RenderTarget, half * BLOCKS_PER_PAGE);
	g_texture_cache->InvalidateVideoMemType(GSTextureCache::DepthStencil, half * BLOCKS_PER_PAGE);
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
			GL_INS("TryTargetClear(): RT at %x <= %08X", rt->m_TEX0.TBP0, c);
			g_gs_device->ClearRenderTarget(rt->m_texture, c);
			rt->m_alpha_max = c >> 24;
			rt->m_alpha_min = c >> 24;

			if (!rt->m_32_bits_fmt)
			{
				rt->m_alpha_max &= 128;
				rt->m_alpha_min &= 128;
			}
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
			const float d = static_cast<float>(z) * (g_gs_device->Features().clip_control ? 0x1p-32f : 0x1p-24f);
			GL_INS("TryTargetClear(): DS at %x <= %f", ds->m_TEX0.TBP0, d);
			g_gs_device->ClearDepth(ds->m_texture, d);
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
	if (!PrimitiveCoversWithoutGaps())
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
				rt_end_bp, m_cached_ctx.FRAME.PSM);

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
				ds_end_bp, m_cached_ctx.ZBUF.PSM);
		}
	}

	return ((invalidate_rt || no_rt) && (invalidate_z || no_ds));
}

void GSRendererHW::ClearGSLocalMemory(const GSOffset& off, const GSVector4i& r, u32 vert_color)
{
	GL_INS("ClearGSLocalMemory(): %08X %d,%d => %d,%d @ BP %x BW %u %s", vert_color, r.x, r.y, r.z, r.w, off.bp(),
		off.bw(), psm_str(off.psm()));

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
			pxAssert((off.bp() & (BLOCKS_PER_PAGE - 1)) == 0);
			for (u32 current_page = off.bp() >> 5; top < page_aligned_bottom; top += pgs.y, current_page += fbw)
			{
				current_page &= 0x7ff;
				GSVector4i* ptr = reinterpret_cast<GSVector4i*>(m_mem.vm8() + current_page * PAGE_SIZE);
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
			pxAssert((off.bp() & (BLOCKS_PER_PAGE - 1)) == 0);
			for (u32 current_page = off.bp() >> 5; top < page_aligned_bottom; top += pgs.y, current_page += fbw)
			{
				current_page &= 0x7ff;
				GSVector4i* ptr = reinterpret_cast<GSVector4i*>(m_mem.vm8() + current_page * PAGE_SIZE);
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
			pxAssert((off.bp() & (BLOCKS_PER_PAGE - 1)) == 0);
			for (u32 current_page = off.bp() >> 5; top < page_aligned_bottom; top += pgs.y, current_page += fbw)
			{
				current_page &= 0x7ff;
				GSVector4i* ptr = reinterpret_cast<GSVector4i*>(m_mem.vm8() + current_page * PAGE_SIZE);
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
	if (r_draw.w > 1024 && (m_vt.m_primclass == GS_SPRITE_CLASS) && (m_vertex.next == 2) && m_process_texture && !PRIM->ABE && tex && !tex->m_target && m_cached_ctx.TEX0.TBW > 0)
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
			g_perfmon.Put(GSPerfMon::TextureCopies, 1);

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
			m_cached_ctx.TEST.ATE || // testing alpha (might discard some pixels)
			m_cached_ctx.TEST.DATE); // reading alpha
}

bool GSRendererHW::IsDiscardingDstColor()
{
	return ((!PRIM->ABE || IsOpaque() || m_context->ALPHA.IsBlack()) && // no blending or writing black
			!AreAnyPixelsDiscarded() && (m_cached_ctx.FRAME.FBMSK & GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].fmsk) == 0); // no channels masked
}

bool GSRendererHW::IsDiscardingDstRGB()
{
	return ((!PRIM->ABE || IsOpaque() || m_context->ALPHA.IsBlack()) && // no blending or writing black
			((m_cached_ctx.FRAME.FBMSK & GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].fmsk) & 0xFFFFFFu) == 0); // RGB isn't masked
}

bool GSRendererHW::IsDiscardingDstAlpha() const
{
	return ((!PRIM->ABE || m_context->ALPHA.C != 1) && // not using Ad
			((m_cached_ctx.FRAME.FBMSK & GSLocalMemory::m_psm[m_cached_ctx.FRAME.PSM].fmsk) & 0xFF000000u) == 0); // alpha isn't masked
}

bool GSRendererHW::PrimitiveCoversWithoutGaps()
{
	if (m_primitive_covers_without_gaps.has_value())
		return m_primitive_covers_without_gaps.value();

	// Draw shouldn't be offset.
	if (((m_r.eq32(GSVector4i::zero())).mask() & 0xff) != 0xff)
	{
		m_primitive_covers_without_gaps = false;
		return false;
	}

	if (m_vt.m_primclass == GS_POINT_CLASS)
	{
		m_primitive_covers_without_gaps = (m_vertex.next < 2);
		return m_primitive_covers_without_gaps.value();
	}
	else if (m_vt.m_primclass == GS_TRIANGLE_CLASS)
	{
		m_primitive_covers_without_gaps = (m_index.tail == 6 && TrianglesAreQuads());
		return m_primitive_covers_without_gaps.value();
	}
	else if (m_vt.m_primclass != GS_SPRITE_CLASS)
	{
		m_primitive_covers_without_gaps = false;
		return false;
	}

	// Simple case: one sprite.
	if (m_index.tail == 2)
	{
		m_primitive_covers_without_gaps = true;
		return true;
	}

	// Check that the height matches. Xenosaga 3 draws a letterbox around
	// the FMV with a sprite at the top and bottom of the framebuffer.
	const GSVertex* v = &m_vertex.buff[0];
	const int first_dpY = v[1].XYZ.Y - v[0].XYZ.Y;
	const int first_dpX = v[1].XYZ.X - v[0].XYZ.X;

	// Horizontal Match.
	if ((first_dpX >> 4) == m_r.z)
	{
		// Borrowed from MergeSprite() modified to calculate heights.
		for (u32 i = 2; i < m_vertex.next; i += 2)
		{
			const int last_pY = v[i - 1].XYZ.Y;
			const int dpY = v[i + 1].XYZ.Y - v[i].XYZ.Y;
			if (std::abs(dpY - first_dpY) >= 16 || std::abs(static_cast<int>(v[i].XYZ.Y) - last_pY) >= 16)
			{
				m_primitive_covers_without_gaps = false;
				return false;
			}
		}

		m_primitive_covers_without_gaps = true;
		return true;
	}

	// Vertical Match.
	if ((first_dpY >> 4) == m_r.w)
	{
		// Borrowed from MergeSprite().
		const int offset_X = m_context->XYOFFSET.OFX;
		for (u32 i = 2; i < m_vertex.next; i += 2)
		{
			const int last_pX = v[i - 1].XYZ.X;
			const int this_start_X = v[i].XYZ.X;
			const int last_start_X = v[i - 2].XYZ.X;

			const int  dpX = v[i + 1].XYZ.X - v[i].XYZ.X;

			if (this_start_X < last_start_X)
			{
				const int prev_X = last_start_X - offset_X;
				if (std::abs(dpX - prev_X) >= 16 || std::abs(this_start_X - offset_X) >= 16)
				{
					m_primitive_covers_without_gaps = false;
					return false;
				}
			}
			else
			{
				if (std::abs(dpX - first_dpX) >= 16 || std::abs(this_start_X - last_pX) >= 16)
				{
					m_primitive_covers_without_gaps = false;
					return false;
				}
			}
		}

		m_primitive_covers_without_gaps = true;
		return true;
	}

	m_primitive_covers_without_gaps = false;
	return false;
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

			const int  dtU = v[i + 1].U - v[i].U;

			if (this_start_U < last_start_U)
			{
				if (std::abs(dtU - last_start_U) >= 16 || std::abs(this_start_U) >= 16)
				{;
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

bool GSRendererHW::IsConstantDirectWriteMemClear()
{
	const bool direct_draw = (m_vt.m_primclass == GS_SPRITE_CLASS) || (((m_index.tail % 6) == 0 && TrianglesAreQuads()) && m_vt.m_primclass == GS_TRIANGLE_CLASS);
	// Constant Direct Write without texture/test/blending (aka a GS mem clear)
	if (direct_draw && !PRIM->TME // Direct write
		&& !(m_draw_env->SCANMSK.MSK & 2)
		&& !m_cached_ctx.TEST.ATE // no alpha test
		&& !m_cached_ctx.TEST.DATE // no destination alpha test
		&& (!m_cached_ctx.TEST.ZTE || m_cached_ctx.TEST.ZTST == ZTST_ALWAYS) // no depth test
		&& (m_vt.m_eq.rgba == 0xFFFF || m_vertex.next == 2)) // constant color write
		return true;

	return false;
}

u32 GSRendererHW::GetConstantDirectWriteMemClearColor() const
{
	// Take the vertex colour, but check if the blending would make it black.
	u32 vert_color = m_vertex.buff[1].RGBAQ.U32[0];
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
}

void GSRendererHW::ReplaceVerticesWithSprite(const GSVector4i& unscaled_rect, const GSVector2i& unscaled_size)
{
	ReplaceVerticesWithSprite(unscaled_rect, unscaled_rect, unscaled_size, unscaled_rect);
}

void GSRendererHW::OffsetDraw(s32 fbp_offset, s32 zbp_offset, s32 xoffset, s32 yoffset)
{
	GL_INS("Offseting render target by %d pages [%x -> %x], Z by %d pages [%x -> %x]",
		fbp_offset, m_cached_ctx.FRAME.FBP << 5, zbp_offset, (m_cached_ctx.FRAME.FBP + fbp_offset) << 5);
	GL_INS("Offseting vertices by [%d, %d]", xoffset, yoffset);

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
	std::memset(&config.cb_vs, 0, sizeof(config.cb_vs));
	std::memset(&config.cb_ps, 0, sizeof(config.cb_ps));

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
	config.datm = false;
	config.line_expand = false;
	config.separate_alpha_pass = false;
	config.second_separate_alpha_pass = false;
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

	GL_PUSH("HLE hardware draw in %d,%d => %d,%d", config.drawarea.left, config.drawarea.top, config.drawarea.right,
		config.drawarea.bottom);

	GSTexture* copy = nullptr;
	if (config.tex && (config.tex == config.rt || config.tex == config.ds))
	{
		const GSDevice::FeatureSupport features = g_gs_device->Features();

		if (!force_copy_on_hazard && config.tex == config.rt && features.texture_barrier)
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
				Console.Error("Texture allocation failure in EndHLEHardwareDraw()");
				return;
			}

			// DX11 can't partial copy depth textures.
			const GSVector4i copy_rect = (src->IsDepthStencil() && !features.test_and_sample_depth) ?
											 src->GetRect() :
											 config.drawarea.rintersect(src->GetRect());
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
