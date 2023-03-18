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
#include "GSTextureCache.h"
#include "GSTextureReplacements.h"
#include "GSRendererHW.h"
#include "GS/GSState.h"
#include "GS/GSGL.h"
#include "GS/GSIntrin.h"
#include "GS/GSUtil.h"
#include "GS/GSXXH.h"
#include "common/Align.h"
#include "common/HashCombine.h"

#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif

static u8* s_unswizzle_buffer;

GSTextureCache::GSTextureCache()
{
	// In theory 4MB is enough but 9MB is safer for overflow (8MB
	// isn't enough in custom resolution)
	// Test: onimusha 3 PAL 60Hz
	s_unswizzle_buffer = (u8*)_aligned_malloc(9 * 1024 * 1024, 32);

	m_surface_offset_cache.reserve(S_SURFACE_OFFSET_CACHE_MAX_SIZE);
}

GSTextureCache::~GSTextureCache()
{
	GSTextureReplacements::Shutdown();

	RemoveAll();

	_aligned_free(s_unswizzle_buffer);
}

void GSTextureCache::ReadbackAll()
{
	for (int type = 0; type < 2; type++)
	{
		for (auto t : m_dst[type])
			Read(t, t->m_drawn_since_read);
	}
}

void GSTextureCache::RemoveAll()
{
	m_src.RemoveAll();

	for (int type = 0; type < 2; type++)
	{
		for (auto t : m_dst[type])
			delete t;

		m_dst[type].clear();
	}

	for (auto it : m_hash_cache)
		g_gs_device->Recycle(it.second.texture);

	m_hash_cache.clear();
	m_hash_cache_memory_usage = 0;
	m_hash_cache_replacement_memory_usage = 0;

	m_palette_map.Clear();
	m_target_heights.clear();

	m_source_memory_usage = 0;
	m_target_memory_usage = 0;

	m_surface_offset_cache.clear();
}

void GSTextureCache::AddDirtyRectTarget(Target* target, GSVector4i rect, u32 psm, u32 bw, RGBAMask rgba, bool req_linear)
{
	bool skipdirty = false;
	bool canskip = true;

	std::vector<GSDirtyRect>::iterator it = target->m_dirty.end();
	while (it != target->m_dirty.begin())
	{
		--it;
		if (it[0].bw == bw && it[0].psm == psm && it[0].rgba._u32 == rgba._u32)
		{
			if (it[0].r.rintersect(rect).eq(rect) && canskip)
			{
				skipdirty = true;
				break;
			}

			// Edges lined up so just expand the dirty rect
			if ((it[0].r.xzxz().eq(rect.xzxz()) && (it[0].r.wwww().eq(rect.yyyy()) || it[0].r.yyyy().eq(rect.wwww()))) ||
				(it[0].r.ywyw().eq(rect.ywyw()) && (it[0].r.zzzz().eq(rect.xxxx()) || it[0].r.xxxx().eq(rect.zzzz()))))
			{
				rect = rect.runion(it[0].r);
				it = target->m_dirty.erase(it);
				canskip = false;
				continue;
			}
		}
	}

	if (!skipdirty)
	{
		target->m_dirty.push_back(GSDirtyRect(rect, psm, bw, rgba, req_linear));

		if (!target->m_drawn_since_read.rempty())
		{
			// If we're covering the drawn area, clear it, in case of readback.
			if (target->m_drawn_since_read.rintersect(target->m_dirty.GetTotalRect(target->m_TEX0, target->m_unscaled_size)).eq(target->m_drawn_since_read))
				target->m_drawn_since_read = GSVector4i::zero();
		}
	}
}



bool GSTextureCache::CanTranslate(u32 bp, u32 bw, u32 spsm, GSVector4i r, u32 dbp, u32 dpsm, u32 dbw)
{
	const GSVector2i page_size = GSLocalMemory::m_psm[spsm].pgs;
	const bool bp_page_aligned_bp = ((bp & ~((1 << 5) - 1)) == bp) || bp == dbp;
	const bool block_layout_match = GSLocalMemory::m_psm[spsm].bpp == GSLocalMemory::m_psm[dpsm].bpp;
	const GSVector4i page_mask(GSVector4i((page_size.x - 1), (page_size.y - 1)).xyxy());
	const GSVector4i masked_rect(r & ~page_mask);
	const int src_pixel_width = page_size.x * static_cast<int>(bw);
	// We can do this if:
	// The page width matches.
	// The rect width is less than the width of the destination texture and the height is less than or equal to 1 page high.
	// The rect width and height is equal to the page size and it covers the width of the incoming bw, so lines are sequential.
	const bool page_aligned_rect = masked_rect.eq(r);
	const bool width_match = bw == dbw;
	const bool sequential_pages = page_aligned_rect && r.x == 0 && r.z == src_pixel_width;
	const bool single_row = bw < dbw && r.z <= src_pixel_width && r.w <= page_size.y;

	if (block_layout_match)
	{
		// Same swizzle, so as long as the block is aligned and it's not a crazy size, we can translate it.
		return bp_page_aligned_bp && (width_match || single_row || sequential_pages);
	}
	else
	{
		// If the format is different, the rect needs to additionally aligned to the pages.
		return bp_page_aligned_bp && page_aligned_rect && (single_row || width_match || sequential_pages);
	}
}


GSVector4i GSTextureCache::TranslateAlignedRectByPage(u32 sbp, u32 spsm, u32 sbw, GSVector4i src_r, u32 dbp, u32 dpsm, u32 bw, bool is_invalidation)
{
	const GSVector2i src_page_size = GSLocalMemory::m_psm[spsm].pgs;
	const GSVector2i dst_page_size = GSLocalMemory::m_psm[dpsm].pgs;
	const u32 src_bw = std::max(1U, sbw);
	const u32 dst_bw = std::max(1U, bw);
	GSVector4i in_rect = src_r;
	int page_offset = (static_cast<int>(sbp) - static_cast<int>(dbp)) >> 5;
	bool single_page = (in_rect.width() / src_page_size.x) <= 1 && (in_rect.height() / src_page_size.y) <= 1;
	if (!single_page)
	{
		const int inc_vertical_offset = (page_offset / static_cast<int>(src_bw)) * src_page_size.y;
		int inc_horizontal_offset = (page_offset % static_cast<int>(src_bw)) * src_page_size.x;
		in_rect = (in_rect + GSVector4i(0, inc_vertical_offset).xyxy()).max_i32(GSVector4i(0));
		in_rect = (in_rect + GSVector4i(inc_horizontal_offset, 0).xyxy()).max_i32(GSVector4i(0));
		page_offset = 0;
		single_page = (in_rect.width() / src_page_size.x) <= 1 && (in_rect.height() / src_page_size.y) <= 1;
	}
	const int vertical_offset = (page_offset / static_cast<int>(dst_bw)) * dst_page_size.y;
	int horizontal_offset = (page_offset % static_cast<int>(dst_bw)) * dst_page_size.x;
	const GSVector4i rect_pages = GSVector4i(in_rect.x / src_page_size.x, in_rect.y / src_page_size.y, (in_rect.z + src_page_size.x - 1) / src_page_size.x, (in_rect.w + (src_page_size.y - 1)) / src_page_size.y);
	const bool block_layout_match = GSLocalMemory::m_psm[spsm].bpp == GSLocalMemory::m_psm[dpsm].bpp;
	GSVector4i new_rect = {};

	if (sbw != bw)
	{
		if (sbw == 0)
		{
			// BW == 0 loops vertically on the first page. So just copy the whole page vertically.
			if (in_rect.z > dst_page_size.x)
			{
				new_rect.x = 0;
				new_rect.z = (dst_page_size.x);
			}
			else
			{
				new_rect.x = in_rect.x;
				new_rect.z = in_rect.z;
			}
			if (in_rect.w > dst_page_size.y)
			{
				new_rect.y = 0;
				new_rect.w = dst_page_size.y;
			}
			else
			{
				new_rect.y = in_rect.y;
				new_rect.w = in_rect.w;
			}
		}
		else if (src_bw == rect_pages.width())
		{

			const u32 totalpages = rect_pages.width() * rect_pages.height();
			const bool full_rows = in_rect.width() == (src_bw * src_page_size.x);
			const bool single_row = in_rect.x == 0 && in_rect.y == 0 && totalpages <= dst_bw;
			bool uneven_pages = (horizontal_offset || (totalpages % dst_bw) != 0) && !single_row;

			// Less than or equal a page and the block layout matches, just copy the rect
			if (block_layout_match && single_page)
			{
				new_rect = in_rect;
			}
			else if (uneven_pages)
			{
				// Results won't be square, if it's not invalidation, it's a texture, which is problematic to translate, so let's not (FIFA 2005).
				if (!is_invalidation)
					return GSVector4i::zero();

				//TODO: Maybe control dirty blocks directly and add them page at a time for better granularity.
				const u32 start_y_page = (rect_pages.y * src_bw) / dst_bw;
				const u32 end_y_page = ((rect_pages.w * src_bw) + (dst_bw - 1)) / dst_bw;

				// Not easily translatable full pages and make sure the height is rounded upto encompass the half row.
				horizontal_offset = 0;
				new_rect.x = 0;
				new_rect.z = (dst_bw * dst_page_size.x);
				new_rect.y = (start_y_page * dst_page_size.y);
				new_rect.w = (end_y_page * dst_page_size.y);
			}
			else
			{
				const u32 start_y_page = (rect_pages.y * src_bw) / dst_bw;
				// Full rows in original rect, so pages are sequential
				if (single_row || full_rows)
				{
					new_rect.x = 0;
					new_rect.z = std::min(totalpages * dst_page_size.x, dst_bw * dst_page_size.x);
					new_rect.y = start_y_page * dst_page_size.y;
					new_rect.w = new_rect.y + (((totalpages + (dst_bw - 1)) / dst_bw) * dst_page_size.y);
				}
				else
				{
					DevCon.Warning("Panic! How did we get here?");
				}
			}
		}
		else if (single_page)
		{
			//The offsets will move this to the right place
			new_rect = GSVector4i(rect_pages.x * dst_page_size.x, rect_pages.y * dst_page_size.y, rect_pages.z * dst_page_size.x, rect_pages.w * dst_page_size.y);
		}
		else
			return GSVector4i::zero();
	}
	else
	{
		if (block_layout_match)
		{
			new_rect = in_rect;
			// The width is mismatched to the bw.
			// Kinda scary but covering the whole row and the next one should be okay? :/ (Check with MVP 07, sbp == 0x39a0)
			if (rect_pages.z > static_cast<int>(bw))
			{
				if (!is_invalidation)
					return GSVector4i::zero();

				u32 offset = rect_pages.z - (bw * dst_page_size.x);
				new_rect.x = offset;
				new_rect.z -= offset;
				new_rect.w += dst_page_size.y;
			}
		}
		else
		{
			new_rect = GSVector4i(rect_pages.x * dst_page_size.x, rect_pages.y * dst_page_size.y, rect_pages.z * dst_page_size.x, rect_pages.w * dst_page_size.y);
		}
	}

	new_rect = (new_rect + GSVector4i(0, vertical_offset).xyxy()).max_i32(GSVector4i(0));
	new_rect = (new_rect + GSVector4i(horizontal_offset, 0).xyxy()).max_i32(GSVector4i(0));

	if (new_rect.z > (static_cast<int>(bw) * dst_page_size.x))
	{
		new_rect.z = (bw * dst_page_size.x);
		new_rect.w += dst_page_size.y;
	}

	return new_rect;
}

void GSTextureCache::DirtyRectByPage(u32 sbp, u32 spsm, u32 sbw, Target* t, GSVector4i src_r, u32 dbp, u32 dpsm, u32 bw)
{
	const GSVector2i src_page_size = GSLocalMemory::m_psm[spsm].pgs;
	const GSVector2i dst_page_size = GSLocalMemory::m_psm[dpsm].pgs;
	const u32 src_bw = std::max(1U, sbw);
	const u32 dst_bw = std::max(1U, bw);
	GSVector4i in_rect = src_r;
	int page_offset = (static_cast<int>(sbp) - static_cast<int>(dbp)) >> 5;
	bool single_page = (in_rect.width() / src_page_size.x) <= 1 && (in_rect.height() / src_page_size.y) <= 1;
	if (!single_page)
	{
		const int inc_vertical_offset = (page_offset / static_cast<int>(src_bw)) * src_page_size.y;
		int inc_horizontal_offset = (page_offset % static_cast<int>(src_bw)) * src_page_size.x;
		in_rect = (in_rect + GSVector4i(0, inc_vertical_offset).xyxy()).max_i32(GSVector4i(0));
		in_rect = (in_rect + GSVector4i(inc_horizontal_offset, 0).xyxy()).max_i32(GSVector4i(0));
		page_offset = 0;
		single_page = (in_rect.width() / src_page_size.x) <= 1 && (in_rect.height() / src_page_size.y) <= 1;
	}
	const int vertical_offset = (page_offset / static_cast<int>(dst_bw)) * dst_page_size.y;
	int horizontal_offset = (page_offset % static_cast<int>(dst_bw)) * dst_page_size.x;
	const GSVector4i rect_pages = GSVector4i(in_rect.x / src_page_size.x, in_rect.y / src_page_size.y, (in_rect.z + src_page_size.x - 1) / src_page_size.x, (in_rect.w + (src_page_size.y - 1)) / src_page_size.y);
	const bool block_layout_match = GSLocalMemory::m_psm[spsm].bpp == GSLocalMemory::m_psm[dpsm].bpp;
	GSVector4i new_rect = {};
	RGBAMask rgba;
	rgba._u32 = GSUtil::GetChannelMask(spsm);

	if (sbw != bw)
	{
		if (sbw == 0)
		{
			// BW == 0 loops vertically on the first page. So just copy the whole page vertically.
			if (in_rect.z > dst_page_size.x)
			{
				new_rect.x = 0;
				new_rect.z = (dst_page_size.x);
			}
			else
			{
				new_rect.x = in_rect.x;
				new_rect.z = in_rect.z;
			}
			if (in_rect.w > dst_page_size.y)
			{
				new_rect.y = 0;
				new_rect.w = dst_page_size.y;
			}
			else
			{
				new_rect.y = in_rect.y;
				new_rect.w = in_rect.w;
			}
		}
		else if (src_bw == rect_pages.width())
		{

			const u32 totalpages = rect_pages.width() * rect_pages.height();
			const bool full_rows = in_rect.width() == (src_bw * src_page_size.x);
			const bool single_row = in_rect.x == 0 && in_rect.y == 0 && totalpages <= dst_bw;
			bool uneven_pages = (horizontal_offset || (totalpages % dst_bw) != 0) && !single_row;

			// Less than or equal a page and the block layout matches, just copy the rect
			if (block_layout_match && single_page)
			{
				new_rect = in_rect;
			}
			else if (uneven_pages)
			{
				//TODO: Maybe control dirty blocks directly and add them page at a time for better granularity.
				const u32 start_page = (rect_pages.y * src_bw) + rect_pages.x;
				const u32 end_page = start_page + totalpages;

				for (u32 i = start_page; i < end_page; i++)
				{
					new_rect.x = (i % dst_bw) * dst_page_size.x;
					new_rect.z = new_rect.x + dst_page_size.x;
					new_rect.y = (i / dst_bw) * dst_page_size.y;
					new_rect.w = new_rect.y + dst_page_size.y;
					AddDirtyRectTarget(t, new_rect, t->m_TEX0.PSM, t->m_TEX0.TBW, rgba);
					
				}
				return;
				// Not easily translatable full pages and make sure the height is rounded upto encompass the half row.
				horizontal_offset = 0;
				
			}
			else
			{
				const u32 start_y_page = (rect_pages.y * src_bw) / dst_bw;
				// Full rows in original rect, so pages are sequential
				if (single_row || full_rows)
				{
					new_rect.x = 0;
					new_rect.z = std::min(totalpages * dst_page_size.x, dst_bw * dst_page_size.x);
					new_rect.y = start_y_page * dst_page_size.y;
					new_rect.w = new_rect.y + (((totalpages + (dst_bw - 1)) / dst_bw) * dst_page_size.y);
				}
				else
				{
					DevCon.Warning("Panic! How did we get here?");
				}
			}
		}
		else if (single_page)
		{
			//The offsets will move this to the right place
			new_rect = GSVector4i(rect_pages.x * dst_page_size.x, rect_pages.y * dst_page_size.y, rect_pages.z * dst_page_size.x, rect_pages.w * dst_page_size.y);
		}
		else // Last resort, hopefully never hit this, but it's better than nothing.
		{
			SurfaceOffsetKey sok;
			sok.elems[0].bp = sbp;
			sok.elems[0].bw = sbw;
			sok.elems[0].psm = spsm;
			sok.elems[0].rect = src_r;
			sok.elems[1].bp = t->m_TEX0.TBP0;
			sok.elems[1].bw = t->m_TEX0.TBW;
			sok.elems[1].psm = t->m_TEX0.PSM;
			sok.elems[1].rect = t->m_valid;

			const SurfaceOffset so = ComputeSurfaceOffset(sok);
			if (so.is_valid)
				AddDirtyRectTarget(t, so.b2a_offset, t->m_TEX0.PSM, t->m_TEX0.TBW, rgba);
			return;
		}
	}
	else
	{
		if (block_layout_match)
		{
			new_rect = in_rect;
			// The width is mismatched to the bw.
			// Kinda scary but covering the whole row and the next one should be okay? :/ (Check with MVP 07, sbp == 0x39a0)
			if (rect_pages.z > static_cast<int>(bw))
			{
				u32 offset = rect_pages.z - (bw * dst_page_size.x);
				new_rect.x = offset;
				new_rect.z -= offset;
				new_rect.w += dst_page_size.y;
			}
		}
		else
		{
			new_rect = GSVector4i(rect_pages.x * dst_page_size.x, rect_pages.y * dst_page_size.y, rect_pages.z * dst_page_size.x, rect_pages.w * dst_page_size.y);
		}
	}

	new_rect = (new_rect + GSVector4i(0, vertical_offset).xyxy()).max_i32(GSVector4i(0));
	new_rect = (new_rect + GSVector4i(horizontal_offset, 0).xyxy()).max_i32(GSVector4i(0));

	if (new_rect.z > (static_cast<int>(bw) * dst_page_size.x))
	{
		new_rect.z = (bw * dst_page_size.x);
		new_rect.w += dst_page_size.y;
	}

	AddDirtyRectTarget(t, new_rect, t->m_TEX0.PSM, t->m_TEX0.TBW, rgba);
}

GSTextureCache::Source* GSTextureCache::LookupDepthSource(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, const GIFRegCLAMP& CLAMP, const GSVector4i& r, bool palette)
{
	if (GSConfig.UserHacks_DisableDepthSupport)
	{
		GL_CACHE("LookupDepthSource not supported (0x%x, F:0x%x)", TEX0.TBP0, TEX0.PSM);
		throw GSRecoverableError();
	}

	const GSLocalMemory::psm_t& psm_s = GSLocalMemory::m_psm[TEX0.PSM];

	Source* src = NULL;
	Target* dst = NULL;

	// Check only current frame, I guess it is only used as a postprocessing effect
	const u32 bp = TEX0.TBP0;
	const u32 psm = TEX0.PSM;

	for (auto t : m_dst[DepthStencil])
	{
		if (t->m_used && t->m_dirty.empty() && GSUtil::HasSharedBits(bp, psm, t->m_TEX0.TBP0, t->m_TEX0.PSM))
		{
			ASSERT(GSLocalMemory::m_psm[t->m_TEX0.PSM].depth);
			if (t->m_age == 0)
			{
				// Perfect Match
				dst = t;
				break;
			}
			else if (t->m_age == 1)
			{
				// Better than nothing (Full Spectrum Warrior)
				dst = t;
			}
		}
	}

	if (!dst)
	{
		// Retry on the render target (Silent Hill 4)
		for (auto t : m_dst[RenderTarget])
		{
			// FIXME: do I need to allow m_age == 1 as a potential match (as DepthStencil) ???
			if (t->m_age <= 1 && t->m_used && t->m_dirty.empty() && GSUtil::HasSharedBits(bp, psm, t->m_TEX0.TBP0, t->m_TEX0.PSM))
			{
				ASSERT(GSLocalMemory::m_psm[t->m_TEX0.PSM].depth);
				dst = t;
				break;
			}
		}
	}

	if (dst)
	{
		GL_CACHE("TC depth: dst %s hit: %d (0x%x, %s)", to_string(dst->m_type),
			dst->m_texture ? dst->m_texture->GetID() : 0,
			TEX0.TBP0, psm_str(psm));

		// Create a shared texture source
		src = new Source(TEX0, TEXA);
		src->m_texture = dst->m_texture;
		src->m_scale = dst->m_scale;
		src->m_unscaled_size = dst->m_unscaled_size;
		src->m_shared_texture = true;
		src->m_target = true; // So renderer can check if a conversion is required
		src->m_from_target = &dst->m_texture; // avoid complex condition on the renderer
		src->m_from_target_TEX0 = dst->m_TEX0;
		src->m_32_bits_fmt = dst->m_32_bits_fmt;
		src->m_valid_rect = dst->m_valid;
		src->m_end_block = dst->m_end_block;

		// Insert the texture in the hash set to keep track of it. But don't bother with
		// texture cache list. It means that a new Source is created everytime we need it.
		// If it is too expensive, one could cut memory allocation in Source constructor for this
		// use case.
		if (palette)
		{
			AttachPaletteToSource(src, psm_s.pal, true);
		}

		m_src.m_surfaces.insert(src);
	}
	else if (g_gs_renderer->m_game.title == CRC::SVCChaos || g_gs_renderer->m_game.title == CRC::KOF2002)
	{
		// SVCChaos black screen & KOF2002 blue screen on main menu, regardless of depth enabled or disabled.
		return LookupSource(TEX0, TEXA, CLAMP, r, nullptr);
	}
	else
	{
		GL_CACHE("TC depth: ERROR miss (0x%x, %s)", TEX0.TBP0, psm_str(psm));
		// Possible ? In this case we could call LookupSource
		// Or just put a basic texture
		// src->m_texture = g_gs_device->CreateTexture(tw, th);
		// In all cases rendering will be broken
		//
		// Note: might worth to check previous frame
		// Note: otherwise return NULL and skip the draw

		// Full Spectrum Warrior: first draw call of cut-scene rendering
		// The game tries to emulate a texture shuffle with an old depth buffer
		// (don't exists yet for us due to the cache)
		// Rendering is nicer (less garbage) if we skip the draw call.
		throw GSRecoverableError();
	}

	ASSERT(src->m_texture);
	ASSERT(src->m_scale == (dst ? dst->m_scale : 1.0f));

	return src;
}

__ri static GSTextureCache::Source* FindSourceInMap(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA,
	const GSLocalMemory::psm_t& psm_s, const u32* clut, const GSTexture* gpu_clut, const GSVector2i& compare_lod,
	const GSTextureCache::SourceRegion& region, u32 fixed_tex0, FastList<GSTextureCache::Source*>& map)
{
	for (auto i = map.begin(); i != map.end(); ++i)
	{
		GSTextureCache::Source* s = *i;

		if (((TEX0.U32[0] ^ s->m_TEX0.U32[0]) | ((TEX0.U32[1] ^ s->m_TEX0.U32[1]) & 3)) != 0) // TBP0 TBW PSM TW TH
			continue;

		// Target are converted (AEM & palette) on the fly by the GPU. They don't need extra check
		if (!s->m_target)
		{
			if (psm_s.pal > 0)
			{
				// If we're doing GPU CLUT, we don't want to use the CPU-converted version.
				if (gpu_clut && !s->m_palette)
					continue;

				// We request a palette texture (psm_s.pal). If the texture was
				// converted by the CPU (!s->m_palette), we need to ensure
				// palette content is the same.
				if (!s->m_palette && !s->ClutMatch({ clut, psm_s.pal }))
					continue;
			}
			else
			{
				// We request a 24/16 bit RGBA texture. Alpha expansion was done by
				// the CPU.  We need to check that TEXA is identical
				if (psm_s.fmt > 0 && s->m_TEXA.U64 != TEXA.U64)
					continue;
			}

			// When fixed tex0 is used, we must find a matching region texture. The base likely
			// doesn't contain to the correct region. Bit cheeky here, avoid a logical or by
			// adding the invalid tex0 bit in.
			if (((s->m_region.bits | fixed_tex0) != 0) && s->m_region.bits != region.bits)
				continue;

			// Same base mip texture, but we need to check that MXL was the same as well.
			// When mipmapping is off, this will be 0,0 vs 0,0.
			if (s->m_lod != compare_lod)
				continue;
		}

		map.MoveFront(i.Index());
		return s;
	}

	return nullptr;
}

GSTextureCache::Source* GSTextureCache::LookupSource(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, const GIFRegCLAMP& CLAMP, const GSVector4i& r, const GSVector2i* lod)
{
	GL_CACHE("TC: Lookup Source <%d,%d => %d,%d> (0x%x, %s, BW: %u, CBP: 0x%x, TW: %d, TH: %d)", r.x, r.y, r.z, r.w, TEX0.TBP0, psm_str(TEX0.PSM), TEX0.TBW, TEX0.CBP, 1 << TEX0.TW, 1 << TEX0.TH);

	const GSLocalMemory::psm_t& psm_s = GSLocalMemory::m_psm[TEX0.PSM];
	//const GSLocalMemory::psm_t& cpsm = psm.pal > 0 ? GSLocalMemory::m_psm[TEX0.CPSM] : psm;

	const u32* const clut = g_gs_renderer->m_mem.m_clut;
	GSTexture* const gpu_clut = (psm_s.pal > 0) ? g_gs_renderer->m_mem.m_clut.GetGPUTexture() : nullptr;

	SourceRegion region = {};
	if (CLAMP.WMS == CLAMP_REGION_CLAMP && CLAMP.MAXU >= CLAMP.MINU)
	{
		// Another Lupin case here, it uses region clamp with UV (not ST), puts a clamp region further
		// into the texture, but a smaller TW/TH. Catch this by looking for a clamp range above TW.
		const u32 rw = CLAMP.MAXU - CLAMP.MINU + 1;
		if (rw < (1u << TEX0.TW) || CLAMP.MAXU >= (1u << TEX0.TW))
		{
			region.SetX(CLAMP.MINU, CLAMP.MAXU + 1);
			GL_CACHE("TC: Region clamp optimization: %d width -> %d", 1 << TEX0.TW, region.GetWidth());
		}
	}
	else if (CLAMP.WMS == CLAMP_REGION_REPEAT && CLAMP.MINU != 0)
	{
		// Lupin the 3rd is really evil, it sets TW/TH to the texture size, but then uses region repeat
		// to offset the actual texture data to elsewhere. So, we'll just force any cases like this down
		// the region texture path.
		const u32 rw = ((CLAMP.MINU | CLAMP.MAXU) - CLAMP.MAXU) + 1;
		if (rw < (1u << TEX0.TW) || (CLAMP.MAXU != 0 && (rw <= (1u << TEX0.TW))))
		{
			region.SetX(CLAMP.MAXU, (CLAMP.MINU | CLAMP.MAXU) + 1);
			GL_CACHE("TC: Region repeat optimization: %d width -> %d", 1 << TEX0.TW, region.GetWidth());
		}
	}
	if (CLAMP.WMT == CLAMP_REGION_CLAMP && CLAMP.MAXV >= CLAMP.MINV)
	{
		const u32 rh = CLAMP.MAXV - CLAMP.MINV + 1;
		if (rh < (1u << TEX0.TH) || CLAMP.MAXV >= (1u << TEX0.TH))
		{
			region.SetY(CLAMP.MINV, CLAMP.MAXV + 1);
			GL_CACHE("TC: Region clamp optimization: %d height -> %d", 1 << TEX0.TW, region.GetHeight());
		}
	}
	else if (CLAMP.WMT == CLAMP_REGION_REPEAT && CLAMP.MINV != 0)
	{
		const u32 rh = ((CLAMP.MINV | CLAMP.MAXV) - CLAMP.MAXV) + 1;
		if (rh < (1u << TEX0.TH) || (CLAMP.MAXV != 0 && (rh <= (1u << TEX0.TH))))
		{
			region.SetY(CLAMP.MAXV, (CLAMP.MINV | CLAMP.MAXV) + 1);
			GL_CACHE("TC: Region repeat optimization: %d height -> %d", 1 << TEX0.TW, region.GetHeight());
		}
	}

	// Prevent everything going to rubbish if a game somehow sends a TW/TH above 10, and region isn't being used.
	if ((TEX0.TW > 10 && !region.HasX()) || (TEX0.TH > 10 && !region.HasY()))
	{
		GL_CACHE("Invalid TEX0 size %ux%u without region, aborting draw.", TEX0.TW, TEX0.TH);
		throw GSRecoverableError();
	}

	const GSVector2i compare_lod(lod ? *lod : GSVector2i(0, 0));
	Source* src = nullptr;

	// Region textures might be placed in a different page, so check that first.
	const u32 lookup_page = TEX0.TBP0 >> 5;
	const bool is_fixed_tex0 = region.IsFixedTEX0(1 << TEX0.TW, 1 << TEX0.TH);
	if (region.GetMinX() != 0 || region.GetMinY() != 0)
	{
		const GSOffset offset(psm_s.info, TEX0.TBP0, TEX0.TBW, TEX0.PSM);
		const u32 region_page = offset.bn(region.GetMinX(), region.GetMinY()) >> 5;
		if (lookup_page != region_page)
			src = FindSourceInMap(TEX0, TEXA, psm_s, clut, gpu_clut, compare_lod, region, is_fixed_tex0, m_src.m_map[region_page]);
	}
	if (!src)
		src = FindSourceInMap(TEX0, TEXA, psm_s, clut, gpu_clut, compare_lod, region, is_fixed_tex0, m_src.m_map[lookup_page]);


	Target* dst = nullptr;
	bool half_right = false;
	int x_offset = 0;
	int y_offset = 0;

#ifdef DISABLE_HW_TEXTURE_CACHE
	if (0)
#else
	if (!src)
#endif
	{
		const u32 bp = TEX0.TBP0;
		const u32 psm = TEX0.PSM;
		const u32 bw = TEX0.TBW;

		// Arc the Lad finds the wrong surface here when looking for a depth stencil.
		// Since we're currently not caching depth stencils (check ToDo in CreateSource) we should not look for it here.

		// (Simply not doing this code at all makes a lot of previsouly missing stuff show (but breaks pretty much everything
		// else.)

		bool found_t = false;
		bool tex_in_rt = false;
		bool tex_merge_rt = false;
		for (auto t : m_dst[RenderTarget])
		{
			if (t->m_used)
			{
				// Typical bug (MGS3 blue cloud):
				// 1/ RT used as 32 bits => alpha channel written
				// 2/ RT used as 24 bits => no update of alpha channel
				// 3/ Lookup of texture that used alpha channel as index, HasSharedBits will return false
				//    because of the previous draw call format
				//
				// Solution: consider the RT as 32 bits if the alpha was used in the past
				const u32 t_psm = (t->m_dirty_alpha) ? t->m_TEX0.PSM & ~0x1 : t->m_TEX0.PSM;
				bool rect_clean = GSUtil::HasSameSwizzleBits(psm, t_psm);
				if (rect_clean && bp >= t->m_TEX0.TBP0 && bp < t->m_end_block && bw == t->m_TEX0.TBW && bp <= t->m_end_block && !t->m_dirty.empty())
				{
					GSVector4i new_rect = r;
					bool partial = false;
					// If it's compatible and page aligned, then handle it this way.
					// It's quicker, and Surface Offsets can get it wrong.
					// Example doing PSMT8H to C32, BP 0x1c80, TBP 0x1d80, incoming rect 0,128 -> 128,256
					// Surface offsets translates it to 0, 128 -> 128, 128, not 0, 0 -> 128, 128.
					if (bp > t->m_TEX0.TBP0)
					{
						const GSVector2i page_size = GSLocalMemory::m_psm[t->m_TEX0.PSM].pgs;
						const bool can_translate = CanTranslate(bp, bw, psm, r, t->m_TEX0.TBP0, t->m_TEX0.PSM, t->m_TEX0.TBW);
						const bool swizzle_match = GSLocalMemory::m_psm[psm].depth == GSLocalMemory::m_psm[t->m_TEX0.PSM].depth;

						if (can_translate)
						{
							if (swizzle_match)
							{
								new_rect = TranslateAlignedRectByPage(bp, psm, bw, r, t->m_TEX0.TBP0, t->m_TEX0.PSM, t->m_TEX0.TBW);

								if (new_rect.eq(GSVector4i::zero()))
								{
									rect_clean = false;
									break;
								}
							}
							else
							{
								// If it's not page aligned, grab the whole pages it covers, to be safe.
								if (GSLocalMemory::m_psm[psm].bpp != GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp)
								{
									const GSVector2i dst_page_size = GSLocalMemory::m_psm[psm].pgs;
									new_rect = GSVector4i(new_rect.x / page_size.x, new_rect.y / page_size.y, (new_rect.z + (page_size.x - 1)) / page_size.x, (new_rect.w + (page_size.y - 1)) / page_size.y);
									new_rect = GSVector4i(new_rect.x * dst_page_size.x, new_rect.y * dst_page_size.y, new_rect.z * dst_page_size.x, new_rect.w * dst_page_size.y);
								}
								else
								{
									new_rect.x &= ~(page_size.x - 1);
									new_rect.y &= ~(page_size.y - 1);
									new_rect.z = (r.z + (page_size.x - 1)) & ~(page_size.x - 1);
									new_rect.w = (r.w + (page_size.y - 1)) & ~(page_size.y - 1);
								}
								new_rect = TranslateAlignedRectByPage(bp & ~((1 << 5) - 1), psm, bw, new_rect, t->m_TEX0.TBP0, t->m_TEX0.PSM, t->m_TEX0.TBW);
							}
						}
						else
						{
							SurfaceOffsetKey sok;
							sok.elems[0].bp = bp;
							sok.elems[0].bw = bw;
							sok.elems[0].psm = psm;
							sok.elems[0].rect = r;
							sok.elems[1].bp = t->m_TEX0.TBP0;
							sok.elems[1].bw = t->m_TEX0.TBW;
							sok.elems[1].psm = t->m_TEX0.PSM;
							sok.elems[1].rect = t->m_valid;

							const SurfaceOffset so = ComputeSurfaceOffset(sok);
							if (so.is_valid)
							{
								new_rect = so.b2a_offset;
							}
						}
					}

					for (auto& dirty : t->m_dirty)
					{
						GSVector4i dirty_rect = dirty.GetDirtyRect(t->m_TEX0);
						if (!dirty_rect.rintersect(new_rect).rempty())
						{
							rect_clean = false;
							partial |= !new_rect.rintersect(dirty_rect).eq(new_rect);
							break;
						}
					}

					const u32 channel_mask = GSUtil::GetChannelMask(psm);
					const u32 channels = t->m_dirty.GetDirtyChannels() & channel_mask;
					// If not all channels are clean/dirty or only part of the rect is dirty, we need to update the target.
					if (((channels & channel_mask) != channel_mask || partial) && !rect_clean)
						t->Update(false);
				}
				else
					rect_clean = t->m_dirty.empty();

				const bool t_clean = ((t->m_dirty.GetDirtyChannels() & GSUtil::GetChannelMask(psm)) == 0) || rect_clean;
				const bool t_wraps = t->m_end_block > GSTextureCache::MAX_BP;
				// Match if we haven't already got a tex in rt
				if (t_clean && GSUtil::HasSharedBits(bp, psm, t->m_TEX0.TBP0, t_psm))
				{
					bool match = true;
					if (found_t && (bw != t->m_TEX0.TBW || t->m_TEX0.PSM != psm))
						match = false;

					if (match)
					{
						// It is a complex to convert the code in shader. As a reference, let's do it on the CPU, it will be slow but
						// 1/ it just works :)
						// 2/ even with upscaling
						// 3/ for both Direct3D and OpenGL
						if (GSConfig.UserHacks_CPUFBConversion && (psm == PSM_PSMT4 || psm == PSM_PSMT8))
						{
							// Forces 4-bit and 8-bit frame buffer conversion to be done on the CPU instead of the GPU, but performance will be slower.
							// There is no dedicated shader to handle 4-bit conversion (Stuntman has been confirmed to use 4-bit).
							// Direct3D10/11 and OpenGL support 8-bit fb conversion but don't render some corner cases properly (Harry Potter games).
							// The hack can fix glitches in some games.
							if (!t->m_drawn_since_read.rempty())
							{
								Read(t, t->m_drawn_since_read);

								t->m_drawn_since_read = GSVector4i::zero();
							}
						}
						else
							dst = t;

						found_t = true;
						tex_in_rt = false;
						tex_merge_rt = false;
						x_offset = 0;
						y_offset = 0;
						break;
					}
				}
				else if (t_clean && (t->m_TEX0.TBW >= 16) && GSUtil::HasSharedBits(bp, psm, t->m_TEX0.TBP0 + t->m_TEX0.TBW * 0x10, t->m_TEX0.PSM))
				{
					// Detect half of the render target (fix snow engine game)
					// Target Page (8KB) have always a width of 64 pixels
					// Half of the Target is TBW/2 pages * 8KB / (1 block * 256B) = 0x10
					half_right = true;
					dst = t;
					found_t = true;
					tex_in_rt = false;
					tex_merge_rt = false;
					x_offset = 0;
					y_offset = 0;
					break;
				}
				// Make sure the texture actually is INSIDE the RT, it's possibly not valid if it isn't.
				// Also check BP >= TBP, create source isn't equpped to expand it backwards and all data comes from the target. (GH3)
				else if (GSConfig.UserHacks_TextureInsideRt >= GSTextureInRtMode::InsideTargets && psm >= PSM_PSMCT32 &&
						 psm <= PSM_PSMCT16S && t->m_TEX0.PSM == psm && (t->Overlaps(bp, bw, psm, r) || t_wraps) &&
						 t->m_age <= 1 && !found_t)
				{
					// PSM equality needed because CreateSource does not handle PSM conversion.
					// Only inclusive hit to limit false hits.

					if (bp > t->m_TEX0.TBP0)
					{
						// Check if it is possible to hit with valid <x,y> offset on the given Target.
						// Fixes Jak eyes rendering.
						// Fixes Xenosaga 3 last dungeon graphic bug.
						// Fixes Pause menu in The Getaway.
						const bool can_translate = CanTranslate(bp, bw, psm, r, t->m_TEX0.TBP0, t->m_TEX0.PSM, t->m_TEX0.TBW);
						if (can_translate)
						{
							const bool swizzle_match = GSLocalMemory::m_psm[psm].depth == GSLocalMemory::m_psm[t->m_TEX0.PSM].depth;
							const GSVector2i page_size = GSLocalMemory::m_psm[t->m_TEX0.PSM].pgs;
							const GSVector4i page_mask(GSVector4i((page_size.x - 1), (page_size.y - 1)).xyxy());
							GSVector4i rect = r & ~page_mask;

							if (swizzle_match)
							{
								rect = TranslateAlignedRectByPage(bp, psm, bw, r, t->m_TEX0.TBP0, t->m_TEX0.PSM, t->m_TEX0.TBW);
								rect.x -= r.x;
								rect.y -= r.y;
							}
							else
							{
								// If it's not page aligned, grab the whole pages it covers, to be safe.
								if (GSLocalMemory::m_psm[psm].bpp != GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp)
								{
									const GSVector2i dst_page_size = GSLocalMemory::m_psm[psm].pgs;
									rect = GSVector4i(rect.x / page_size.x, rect.y / page_size.y, (rect.z + (page_size.x - 1)) / page_size.x, (rect.w + (page_size.y - 1)) / page_size.y);
									rect = GSVector4i(rect.x * dst_page_size.x, rect.y * dst_page_size.y, rect.z * dst_page_size.x, rect.w * dst_page_size.y);
								}
								else
								{
									rect.x &= ~(page_size.x - 1);
									rect.y &= ~(page_size.y - 1);
									rect.z = (r.z + (page_size.x - 1)) & ~(page_size.x - 1);
									rect.w = (r.w + (page_size.y - 1)) & ~(page_size.y - 1);
								}
								rect = TranslateAlignedRectByPage(bp & ~((1 << 5) - 1), psm, bw, rect, t->m_TEX0.TBP0, t->m_TEX0.PSM, t->m_TEX0.TBW);
								rect.x -= r.x & ~(page_size.y - 1);
								rect.y -= r.x & ~(page_size.y - 1);
							}

							rect = rect.rintersect(t->m_valid);

							if (rect.rempty())
								continue;

							if (!t->m_dirty.empty())
							{
								const GSVector4i dirty_rect = t->m_dirty.GetTotalRect(t->m_TEX0, GSVector2i(rect.z, rect.w)).rintersect(rect);
								if (!dirty_rect.eq(rect))
								{
									// Only update if the rect isn't empty
									if (!dirty_rect.rempty())
										t->Update(false);
								}
								else
									continue;
							}

							x_offset = rect.x;
							y_offset = rect.y;
							dst = t;
							tex_in_rt = true;
							tex_merge_rt = false;
							found_t = true;
							continue;
						}
						else
						{
							SurfaceOffset so = ComputeSurfaceOffset(bp, bw, psm, r, t);
							if (!so.is_valid && t_wraps)
							{
								// Improves Beyond Good & Evil shadow.
								const u32 bp_unwrap = bp + GSTextureCache::MAX_BP + 0x1;
								so = ComputeSurfaceOffset(bp_unwrap, bw, psm, r, t);
							}
							if (so.is_valid)
							{
								dst = t;
								// Offset from Target to Source in Target coords.
								x_offset = so.b2a_offset.x;
								y_offset = so.b2a_offset.y;
								tex_in_rt = true;
								tex_merge_rt = false;
								found_t = true;
								// Keep looking, just in case there is an exact match (Situation: Target frame drawn inside target frame, current makes a separate texture)
								continue;
							}
						}
					}
					else if (GSConfig.UserHacks_TextureInsideRt >= GSTextureInRtMode::MergeTargets && !tex_merge_rt)
					{
						dst = t;
						x_offset = 0;
						y_offset = 0;
						tex_in_rt = false;
						tex_merge_rt = true;

						// Prefer a target inside over a target outside.
						found_t = false;
						continue;
					}
				}
			}
		}

		if (tex_in_rt)
		{
			GSVector2i size_delta = { ((x_offset + (1 << TEX0.TW)) - dst->m_valid.z), ((y_offset + (1 << TEX0.TH)) - dst->m_valid.w) };
			RGBAMask rgba;
			rgba._u32 = GSUtil::GetChannelMask(psm);

			if (size_delta.x > 0)
			{
				// Expand the target if it's only partially inside it.
				const GSVector4i dirty_rect = { dst->m_valid.z, 0, x_offset + (1 << TEX0.TW), dst->m_valid.w };

				if (dirty_rect.z > dst->m_valid.z)
				{
					dst->UpdateValidity(dirty_rect);

					AddDirtyRectTarget(dst, dirty_rect, dst->m_TEX0.PSM, dst->m_TEX0.TBW, rgba);
				}
			}

			if (size_delta.y > 0)
			{
				// Expand the target if it's only partially inside it.
				const GSVector4i dirty_rect = { 0, dst->m_valid.w, dst->m_valid.z, y_offset + (1 << TEX0.TH) };

				if (dirty_rect.w > dst->m_valid.w)
				{
					dst->UpdateValidity(dirty_rect);

					AddDirtyRectTarget(dst, dirty_rect, dst->m_TEX0.PSM, dst->m_TEX0.TBW, rgba);
				}
			}

			if (dst->m_valid.z > dst->m_unscaled_size.x || dst->m_valid.w > dst->m_unscaled_size.y)
				dst->ResizeTexture(dst->m_valid.z, dst->m_valid.w);
		}
		// Pure depth texture format will be fetched by LookupDepthSource.
		// However guess what, some games (GoW) read the depth as a standard
		// color format (instead of a depth format). All pixels are scrambled
		// (because color and depth don't have same location). They don't care
		// pixel will be several draw calls later.
		//
		// Sigh... They don't help us.

		if (!found_t && !dst && !GSConfig.UserHacks_DisableDepthSupport)
		{
			// Let's try a trick to avoid to use wrongly a depth buffer
			// Unfortunately, I don't have any Arc the Lad testcase
			//
			// 1/ Check only current frame, I guess it is only used as a postprocessing effect
			for (auto t : m_dst[DepthStencil])
			{
				if (t->m_age <= 1 && t->m_used && t->m_dirty.empty() && GSUtil::HasSharedBits(bp, psm, t->m_TEX0.TBP0, t->m_TEX0.PSM))
				{
					GL_INS("TC: Warning depth format read as color format. Pixels will be scrambled");
					// Let's fetch a depth format texture. Rational, it will avoid the texture allocation and the
					// rescaling of the current function.
					if (psm_s.bpp > 8)
					{
						GIFRegTEX0 depth_TEX0;
						depth_TEX0.U32[0] = TEX0.U32[0] | (0x30u << 20u);
						depth_TEX0.U32[1] = TEX0.U32[1];
						return LookupDepthSource(depth_TEX0, TEXA, CLAMP, r);
					}
					else
					{
						return LookupDepthSource(TEX0, TEXA, CLAMP, r, true);
					}
				}
			}
		}

		if (tex_merge_rt)
			src = CreateMergedSource(TEX0, TEXA, region, dst->m_scale);
	}

	if (!src)
	{
#ifdef ENABLE_OGL_DEBUG
		if (dst)
		{
			GL_CACHE("TC: dst %s hit (%s, OFF <%d,%d>): %d (0x%x, %s)",
				to_string(dst->m_type),
				half_right ? "half" : "full",
				x_offset,
				y_offset,
				dst->m_texture ? dst->m_texture->GetID() : 0,
				TEX0.TBP0,
				psm_str(TEX0.PSM));
		}
		else
		{
			GL_CACHE("TC: src miss (0x%x, 0x%x, %s)", TEX0.TBP0, psm_s.pal > 0 ? TEX0.CBP : 0, psm_str(TEX0.PSM));
		}
#endif
		src = CreateSource(TEX0, TEXA, dst, half_right, x_offset, y_offset, lod, &r, gpu_clut, region);
	}
	else
	{
		GL_CACHE("TC: src hit: %d (0x%x, 0x%x, %s)",
			src->m_texture ? src->m_texture->GetID() : 0,
			TEX0.TBP0, psm_s.pal > 0 ? TEX0.CBP : 0,
			psm_str(TEX0.PSM));

		if (gpu_clut)
			AttachPaletteToSource(src, gpu_clut);
		else if (src->m_palette && (!src->m_palette_obj || !src->ClutMatch({ clut, psm_s.pal })))
			AttachPaletteToSource(src, psm_s.pal, true);
	}

	src->Update(r);

	m_src.m_used = true;

	return src;
}

GSTextureCache::Target* GSTextureCache::FindTargetOverlap(u32 bp, u32 end_block, int type, int psm)
{
	u32 end_block_bp = end_block < bp ? (MAX_BP + 1) : end_block;

	for (auto t : m_dst[type])
	{
		// Only checks that the texure starts at the requested bp, which shares data. Size isn't considered.
		if (t->m_TEX0.TBP0 >= bp && t->m_TEX0.TBP0 < end_block_bp && GSUtil::HasSharedBits(t->m_TEX0.PSM, psm))
			return t;
	}
	return nullptr;
}

GSTextureCache::Target* GSTextureCache::LookupTarget(const GIFRegTEX0& TEX0, const GSVector2i& size, float scale, int type, bool used, u32 fbmask, const bool is_frame, bool preload, bool is_clear)
{
	const GSLocalMemory::psm_t& psm_s = GSLocalMemory::m_psm[TEX0.PSM];
	const u32 bp = TEX0.TBP0;
	GSVector2i new_size{0, 0};
	GSVector2i new_scaled_size{0, 0};
	const GSVector4 sRect(0, 0, 1, 1);
	GSVector4 dRect{};
	bool clear = true;
	const auto& calcRescale = [&size, &scale, &new_size, &new_scaled_size, &clear, &dRect](const Target* tgt)
	{
		// TODO Possible optimization: rescale only the validity rectangle of the old target texture into the new one.
		clear = (size.x > tgt->m_unscaled_size.x || size.y > tgt->m_unscaled_size.y);
		new_size.x = std::max(size.x, tgt->m_unscaled_size.x);
		new_size.y = std::max(size.y, tgt->m_unscaled_size.y);
		new_scaled_size.x = static_cast<int>(std::ceil(static_cast<float>(new_size.x) * scale));
		new_scaled_size.y = static_cast<int>(std::ceil(static_cast<float>(new_size.y) * scale));
		dRect = (GSVector4(GSVector4i::loadh(tgt->m_unscaled_size)) * GSVector4(scale)).ceil();
		GL_INS("TC Rescale: %dx%d: %dx%d @ %f -> %dx%d @ %f", tgt->m_unscaled_size.x, tgt->m_unscaled_size.y,
			tgt->m_texture->GetWidth(), tgt->m_texture->GetHeight(), tgt->m_scale, new_scaled_size.x, new_scaled_size.y,
			scale);
	};

	Target* dst = nullptr;
	auto& list = m_dst[type];
	Target* old_found = nullptr;

	if (!is_frame)
	{
		for (auto i = list.begin(); i != list.end(); ++i)
		{
			Target* t = *i;

			if (bp == t->m_TEX0.TBP0)
			{
				list.MoveFront(i.Index());

				dst = t;

				dst->m_32_bits_fmt |= (psm_s.bpp != 16);
				break;
			}
		}
	}
	else
	{
		assert(type == RenderTarget);
		// Let's try to find a perfect frame that contains valid data
		for (auto t : list)
		{
			// Only checks that the texure starts at the requested bp, size isn't considered.
			if (bp == t->m_TEX0.TBP0 && t->m_end_block >= bp)
			{
				// If the frame is older than 30 frames (0.5 seconds) then it hasn't been updated for ages, so it's probably not a valid output frame.
				// The rest of the checks will get better equality, so suffer less from misdetection.
				// Kind of arbitrary but it's low enough to not break Grandia Xtreme and high enough not to break Mission Impossible Operation Surma.
				if (t->m_age > 30 && !old_found)
				{
					old_found = t;
					continue;
				}

				dst = t;
				GL_CACHE("TC: Lookup Frame %dx%d, perfect hit: %d (0x%x -> 0x%x %s)", size.x, size.y, dst->m_texture->GetID(), bp, t->m_end_block, psm_str(TEX0.PSM));
				if (size.x > 0 || size.y > 0)
					ScaleTargetForDisplay(dst, TEX0, size.x, size.y);

				break;
			}
		}

		// 2nd try ! Try to find a frame at the requested bp -> bp + size is inside of (or equal to)
		if (!dst)
		{
			const u32 needed_end = GSLocalMemory::m_psm[TEX0.PSM].info.bn(size.x - 1, size.y - 1, bp, TEX0.TBW);
			for (auto t : list)
			{
				// Make sure the target is inside the texture
				if (t->m_TEX0.TBP0 <= bp && bp <= t->m_end_block && t->Inside(bp, TEX0.TBW, TEX0.PSM, GSVector4i::loadh(size)))
				{
					// If we already have an old one, make sure the "new" one matches at least on one end (double buffer?).
					if (old_found && (t->m_age > 4 || (t->m_TEX0.TBP0 != bp && needed_end != t->m_end_block)))
						continue;

					dst = t;
					GL_CACHE("TC: Lookup Frame %dx%d, inclusive hit: %d (0x%x, took 0x%x -> 0x%x %s)", size.x, size.y, t->m_texture->GetID(), bp, t->m_TEX0.TBP0, t->m_end_block, psm_str(TEX0.PSM));

					if (size.x > 0 || size.y > 0)
						ScaleTargetForDisplay(dst, TEX0, size.x, size.y);

					break;
				}
			}
		}

		if (!dst && old_found)
		{
			dst = old_found;
		}

		// 3rd try ! Try to find a frame that doesn't contain valid data (honestly I'm not sure we need to do it)
		if (!dst)
		{
			for (auto t : list)
			{
				if (bp == t->m_TEX0.TBP0)
				{
					dst = t;
					GL_CACHE("TC: Lookup Frame %dx%d, empty hit: %d (0x%x -> 0x%x %s)", size.x, size.y, dst->m_texture->GetID(), bp, t->m_end_block, psm_str(TEX0.PSM));
					break;
				}
			}
		}

		if (dst)
		{
			dst->m_TEX0.TBW = TEX0.TBW; // Fix Jurassic Park - Operation Genesis loading disk logo.
			dst->m_is_frame |= is_frame; // Nicktoons Unite tries to change the width from 10 to 8 and breaks FMVs.
		}
	}

	if (dst)
	{
		GL_CACHE("TC: Lookup %s(%s) %dx%d (0x%x, BW:%u, %s) hit (0x%x, BW:%d, %s)", is_frame ? "Frame" : "Target",
			to_string(type), size.x, size.y, bp, TEX0.TBW, psm_str(TEX0.PSM), dst->m_TEX0.TBP0, dst->m_TEX0.TBW, psm_str(dst->m_TEX0.PSM));

		// Update is done by caller after TEX0 update for non-frame.
		if (is_frame)
			dst->Update(old_found == dst);

		if (dst->m_scale != scale)
		{
			calcRescale(dst);
			GSTexture* tex = type == RenderTarget ? g_gs_device->CreateRenderTarget(new_scaled_size.x, new_scaled_size.y, GSTexture::Format::Color, clear) :
													g_gs_device->CreateDepthStencil(new_scaled_size.x, new_scaled_size.y, GSTexture::Format::DepthStencil, clear);
			g_gs_device->StretchRect(dst->m_texture, sRect, tex, dRect, (type == RenderTarget) ? ShaderConvert::COPY : ShaderConvert::DEPTH_COPY, false);
			m_target_memory_usage = (m_target_memory_usage - dst->m_texture->GetMemUsage()) + tex->GetMemUsage();
			g_gs_device->Recycle(dst->m_texture);
			dst->m_texture = tex;
			dst->m_scale = scale;
			dst->m_unscaled_size = new_size;
		}

		if (!is_frame)
			dst->m_dirty_alpha |= (psm_s.trbpp == 32 && (fbmask & 0xFF000000) != 0xFF000000) || (psm_s.trbpp == 16);
	}
	else if (!is_frame && !GSConfig.UserHacks_DisableDepthSupport)
	{

		int rev_type = (type == DepthStencil) ? RenderTarget : DepthStencil;

		// Depth stencil/RT can be an older RT/DS but only check recent RT/DS to avoid to pick
		// some bad data.
		Target* dst_match = nullptr;
		for (auto t : m_dst[rev_type])
		{
			if (bp == t->m_TEX0.TBP0)
			{
				if (t->m_age == 0)
				{
					dst_match = t;
					break;
				}
				else if (t->m_age == 1)
				{
					dst_match = t;
				}
			}
		}

		if (dst_match)
		{
			dst_match->Update(true);
			calcRescale(dst_match);
			dst = CreateTarget(TEX0, new_size.x, new_size.y, scale, type, clear);
			dst->m_32_bits_fmt = dst_match->m_32_bits_fmt;
			dst->OffsetHack_modxy = dst_match->OffsetHack_modxy;
			ShaderConvert shader;
			// m_32_bits_fmt gets set on a shuffle or if the format isn't 16bit.
			// In this case it needs to make sure it isn't part of a shuffle, where it needs to be interpreted as 32bits.
			const bool fmt_16_bits = (psm_s.bpp == 16 && GSLocalMemory::m_psm[dst_match->m_TEX0.PSM].bpp == 16 && !dst->m_32_bits_fmt);
			if (type == DepthStencil)
			{
				GL_CACHE("TC: Lookup Target(Depth) %dx%d, hit Color (0x%x, TBW %d, %s was %s)", new_size.x, new_size.y, bp, TEX0.TBW, psm_str(TEX0.PSM), psm_str(dst_match->m_TEX0.PSM));
				shader = (fmt_16_bits) ? ShaderConvert::RGB5A1_TO_FLOAT16 : (ShaderConvert)(static_cast<int>(ShaderConvert::RGBA8_TO_FLOAT32) + psm_s.fmt);
			}
			else
			{
				GL_CACHE("TC: Lookup Target(Color) %dx%d, hit Depth (0x%x, TBW %d, %s was %s)", new_size.x, new_size.y, bp, TEX0.TBW, psm_str(TEX0.PSM), psm_str(dst_match->m_TEX0.PSM));
				shader = (fmt_16_bits) ? ShaderConvert::FLOAT16_TO_RGB5A1 : ShaderConvert::FLOAT32_TO_RGBA8;
			}
			g_gs_device->StretchRect(dst_match->m_texture, sRect, dst->m_texture, dRect, shader, false);
		}
	}

	if (!dst)
	{
		// Skip full screen clears from making massive targets.
		if (is_clear)
		{
			GL_CACHE("TC: Create RT skipped on clear draw");
			return nullptr;
		}

		GL_CACHE("TC: Lookup %s(%s) %dx%d, miss (0x%x, TBW %d, %s)", is_frame ? "Frame" : "Target", to_string(type), size.x, size.y, bp, TEX0.TBW, psm_str(TEX0.PSM));

		dst = CreateTarget(TEX0, size.x, size.y, scale, type, true);
		// In theory new textures contain invalidated data. Still in theory a new target
		// must contains the content of the GS memory.
		// In practice, TC will wrongly invalidate some RT. For example due to write on the alpha
		// channel but colors is still valid. Unfortunately TC doesn't support the upload of data
		// in target.
		//
		// Cleaning the code here will likely break several games. However it might reduce
		// the noise in draw call debugging. It is the main reason to enable it on debug build.
		//
		// From a performance point of view, it might cost a little on big upscaling
		// but normally few RT are miss so it must remain reasonable.
		const bool supported_fmt = !GSConfig.UserHacks_DisableDepthSupport || psm_s.depth == 0;

		if (TEX0.TBW > 0 && supported_fmt)
		{
			const bool forced_preload = GSRendererHW::GetInstance()->m_force_preload > 0;
			const GSVector4i newrect = GSVector4i::loadh(size);
			const u32 rect_end = GSLocalMemory::m_psm[TEX0.PSM].info.bn(newrect.z - 1, newrect.w - 1, TEX0.TBP0, TEX0.TBW);
			RGBAMask rgba;
			rgba._u32 = GSUtil::GetChannelMask(TEX0.PSM);

			if (!is_frame && !forced_preload && !preload)
			{
				std::vector<GSState::GSUploadQueue>::iterator iter;
				GSVector4i eerect = GSVector4i::zero();

				for (iter = GSRendererHW::GetInstance()->m_draw_transfers.begin(); iter != GSRendererHW::GetInstance()->m_draw_transfers.end(); )
				{
					// If the format, and location doesn't overlap
					if (iter->blit.DBP >= TEX0.TBP0 && iter->blit.DBP <= rect_end && GSUtil::HasCompatibleBits(iter->blit.DPSM, TEX0.PSM))
					{
						GSVector4i targetr = {};
						const bool can_translate = CanTranslate(iter->blit.DBP, iter->blit.DBW, iter->blit.DPSM, iter->rect, TEX0.TBP0, TEX0.PSM, TEX0.TBW);
						const bool swizzle_match = GSLocalMemory::m_psm[iter->blit.DPSM].depth == GSLocalMemory::m_psm[TEX0.PSM].depth;
						if (can_translate)
						{
							if (swizzle_match)
							{
								targetr = TranslateAlignedRectByPage(iter->blit.DBP, iter->blit.DPSM, iter->blit.DBW, iter->rect, TEX0.TBP0, TEX0.PSM, TEX0.TBW, true);
							}
							else
							{
								// If it's not page aligned, grab the whole pages it covers, to be safe.
								GSVector4i new_rect = iter->rect;
								const GSVector2i page_size = GSLocalMemory::m_psm[iter->blit.DPSM].pgs;

								if (GSLocalMemory::m_psm[iter->blit.DPSM].bpp != GSLocalMemory::m_psm[TEX0.PSM].bpp)
								{
									const GSVector2i dst_page_size = GSLocalMemory::m_psm[iter->blit.DPSM].pgs;
									new_rect = GSVector4i(new_rect.x / page_size.x, new_rect.y / page_size.y, (new_rect.z + (page_size.x - 1)) / page_size.x, (new_rect.w + (page_size.y - 1)) / page_size.y);
									new_rect = GSVector4i(new_rect.x * dst_page_size.x, new_rect.y * dst_page_size.y, new_rect.z * dst_page_size.x, new_rect.w * dst_page_size.y);
								}
								else
								{
									new_rect.x &= ~(page_size.x - 1);
									new_rect.y &= ~(page_size.y - 1);
									new_rect.z = (new_rect.z + (page_size.x - 1)) & ~(page_size.x - 1);
									new_rect.w = (new_rect.w + (page_size.y - 1)) & ~(page_size.y - 1);
								}
								targetr = TranslateAlignedRectByPage(iter->blit.DBP & ~((1 << 5) - 1), iter->blit.DPSM, iter->blit.DBW, new_rect, TEX0.TBP0, TEX0.PSM, TEX0.TBW, true);
							}
						}
						else
						{
							GSTextureCache::SurfaceOffsetKey sok;
							sok.elems[0].bp = iter->blit.DBP;
							sok.elems[0].bw = iter->blit.DBW;
							sok.elems[0].psm = iter->blit.DPSM;
							sok.elems[0].rect = iter->rect;
							sok.elems[1].bp = TEX0.TBP0;
							sok.elems[1].bw = TEX0.TBW;
							sok.elems[1].psm = TEX0.PSM;
							sok.elems[1].rect = newrect;

							// Calculate the rect offset if the BP doesn't match.
							targetr = (iter->blit.DBP == TEX0.TBP0) ? iter->rect : ComputeSurfaceOffset(sok).b2a_offset;
						}

						if (eerect.rempty())
							eerect = targetr;
						else
							eerect = eerect.runion(targetr);

						iter = GSRendererHW::GetInstance()->m_draw_transfers.erase(iter);

						if (eerect.rintersect(newrect).eq(newrect))
							break;
						else
							continue;
					}
					iter++;
				}

				if (!eerect.rempty())
				{
					GL_INS("Preloading the RT DATA");
					eerect = eerect.rintersect(newrect);
					dst->UpdateValidity(newrect);
					AddDirtyRectTarget(dst, eerect, TEX0.PSM, TEX0.TBW, rgba, GSLocalMemory::m_psm[TEX0.PSM].trbpp >= 16);
				}
			}
			else
			{
				GL_INS("Preloading the RT DATA");
				dst->UpdateValidity(newrect);
				AddDirtyRectTarget(dst, newrect, TEX0.PSM, TEX0.TBW, rgba, GSLocalMemory::m_psm[TEX0.PSM].trbpp >= 16);
			}
		}
		dst->m_is_frame = is_frame;
	}
	if (used)
	{
		dst->m_used = true;
	}
	if (is_frame)
		dst->m_dirty_alpha = false;

	dst->readbacks_since_draw = 0;

	assert(dst && dst->m_texture && dst->m_scale == scale);
	return dst;
}

GSTextureCache::Target* GSTextureCache::LookupDisplayTarget(const GIFRegTEX0& TEX0, const GSVector2i& size, float scale)
{
	return LookupTarget(TEX0, size, scale, RenderTarget, true, 0, true);
}

void GSTextureCache::ScaleTargetForDisplay(Target* t, const GIFRegTEX0& dispfb, int real_w, int real_h)
{
	// This handles a case where you have two images stacked on top of one another (usually FMVs), and
	// the size of the top framebuffer is larger than the height of the image. Usually happens when
	// conservative FB is on, as off it'll create a 1280 high framebuffer.

	// The game alternates DISPFB between the top image, where the block pointer matches the target,
	// but when it switches to the other buffer, LookupTarget() will score a partial match on the target
	// because e.g. 448 < 512, but the target doesn't actually contain the full image. This usually leads
	// to flickering. Test case: Neo Contra intro FMVs.

	// So, for these cases, we simply expand the target to include both images, based on the read height.
	// It won't affect normal rendering, since that doesn't go through this path.

	// Compute offset into the target that we'll start reading from.
	const int delta = dispfb.TBP0 - t->m_TEX0.TBP0;
	int y_offset = 0;
	if (delta > 0 && t->m_TEX0.TBW != 0)
	{
		const int pages = delta >> 5u;
		const int y_pages = pages / t->m_TEX0.TBW;
		y_offset = y_pages * GSLocalMemory::m_psm[t->m_TEX0.PSM].pgs.y;
	}

	// Take that into consideration to find the extent of the target which will be sampled.
	GSTexture* old_texture = t->m_texture;
	const float scale = t->m_scale;
	const int old_width = t->m_unscaled_size.x;
	const int old_height = t->m_unscaled_size.y;
	const int needed_height = std::min(real_h + y_offset, GSRendererHW::MAX_FRAMEBUFFER_HEIGHT);
	const int needed_width = std::min(real_w, static_cast<int>(dispfb.TBW * 64));
	if (needed_height <= t->m_unscaled_size.y && needed_width <= t->m_unscaled_size.x)
		return;
	// We're expanding, so create a new texture.
	const int new_height = std::max(t->m_unscaled_size.y, needed_height);
	const int new_width = std::max(t->m_unscaled_size.x, needed_width);
	const int scaled_new_height = static_cast<int>(std::ceil(static_cast<float>(new_height) * scale));
	const int scaled_new_width = static_cast<int>(std::ceil(static_cast<float>(new_width) * scale));
	GSTexture* new_texture = g_gs_device->CreateRenderTarget(scaled_new_width, scaled_new_height, GSTexture::Format::Color, false);
	if (!new_texture)
	{
		// Memory allocation failure, do our best to hobble along.
		return;
	}

	GL_CACHE("Expanding target for display output, target height %d @ 0x%X, display %d @ 0x%X offset %d needed %d",
		t->m_unscaled_size.y, t->m_TEX0.TBP0, real_h, dispfb.TBP0, y_offset, needed_height);

	// Fill the new texture with the old data, and discard the old texture.
	g_gs_device->StretchRect(old_texture, new_texture, GSVector4(old_texture->GetSize()).zwxy(), ShaderConvert::COPY, false);
	m_target_memory_usage = (m_target_memory_usage - old_texture->GetMemUsage()) + new_texture->GetMemUsage();
	g_gs_device->Recycle(old_texture);
	t->m_texture = new_texture;
	t->m_unscaled_size = GSVector2i(new_width, new_height);

	RGBAMask rgba;
	rgba._u32 = GSUtil::GetChannelMask(t->m_TEX0.PSM);
	// We unconditionally preload the frame here, because otherwise we'll end up with blackness for one frame (when the expand happens).
	const int preload_width = t->m_TEX0.TBW * 64;
	if (old_width < preload_width && old_height < needed_height)
	{
		const GSVector4i right(old_width, 0, preload_width, needed_height);
		const GSVector4i bottom(0, old_height, old_width, needed_height);
		AddDirtyRectTarget(t, right, t->m_TEX0.PSM, t->m_TEX0.TBW, rgba);
		AddDirtyRectTarget(t, bottom, t->m_TEX0.PSM, t->m_TEX0.TBW, rgba);
	}
	else
	{
		const GSVector4i newrect = GSVector4i((old_height < new_height) ? 0 : old_width,
			(old_width < preload_width) ? 0 : old_height,
			preload_width, needed_height);
		AddDirtyRectTarget(t, newrect, t->m_TEX0.PSM, t->m_TEX0.TBW, rgba);
	}

	// Inject the new height back into the cache.
	GetTargetHeight(t->m_TEX0.TBP0, t->m_TEX0.TBW, t->m_TEX0.PSM, static_cast<u32>(needed_height));
}

bool GSTextureCache::PrepareDownloadTexture(u32 width, u32 height, GSTexture::Format format, std::unique_ptr<GSDownloadTexture>* tex)
{
	GSDownloadTexture* ctex = tex->get();
	if (ctex && ctex->GetWidth() >= width && ctex->GetHeight() >= height)
		return true;

	// In the case of oddly sized texture reads, we'll keep the larger dimension.
	const u32 new_width = ctex ? std::max(ctex->GetWidth(), width) : width;
	const u32 new_height = ctex ? std::max(ctex->GetHeight(), height) : height;
	tex->reset();
	*tex = g_gs_device->CreateDownloadTexture(new_width, new_height, format);
	if (!tex)
	{
		Console.WriteLn("Failed to create %ux%u download texture", new_width, new_height);
		return false;
	}

	return true;
}

// Expands targets where the write from the EE overlaps the edge of a render target and uses the same base pointer.
void GSTextureCache::ExpandTarget(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r)
{
	GIFRegTEX0 TEX0;
	TEX0.TBP0 = BITBLTBUF.DBP;
	TEX0.TBW = BITBLTBUF.DBW;
	TEX0.PSM = BITBLTBUF.DPSM;
	Target* dst = nullptr;
	auto& list = m_dst[RenderTarget];
	RGBAMask rgba;
	rgba._u32 = GSUtil::GetChannelMask(TEX0.PSM);

	for (auto i = list.begin(); i != list.end(); ++i)
	{
		Target* t = *i;

		if (TEX0.TBP0 == t->m_TEX0.TBP0 && GSLocalMemory::m_psm[TEX0.PSM].bpp == GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp && t->Overlaps(TEX0.TBP0, TEX0.TBW, TEX0.PSM, r))
		{
			list.MoveFront(i.Index());

			dst = t;
			break;
		}
	}

	if (!dst)
		return;

	// Only expand the target when the FBW matches. Otherwise, games like GT4 will end up with the main render target
	// being 2000+ due to unrelated EE writes.
	if (TEX0.TBW == dst->m_TEX0.TBW)
	{
		// Round up to the nearest even height, like the draw target allocator.
		const s32 aligned_height = Common::AlignUpPow2(r.w, 2);
		if (r.z > dst->m_unscaled_size.x || aligned_height > dst->m_unscaled_size.y)
		{
			// We don't recycle here, because most of the time when this happens it's strange-sized textures
			// which are being expanded one-line-at-a-time.
			if (dst->ResizeTexture(std::max(r.z, dst->m_unscaled_size.x),
					std::max(aligned_height, dst->m_unscaled_size.y), false))
			{
				AddDirtyRectTarget(dst, r, TEX0.PSM, TEX0.TBW, rgba);
				GetTargetHeight(TEX0.TBP0, TEX0.TBW, TEX0.PSM, aligned_height);
				dst->UpdateValidity(r);
				dst->UpdateValidBits(GSLocalMemory::m_psm[TEX0.PSM].fmsk);
			}
		}
	}
	else
	{
		const GSVector4i clamped_r(r.rintersect(dst->GetUnscaledRect()));
		AddDirtyRectTarget(dst, clamped_r, TEX0.PSM, TEX0.TBW, rgba);
		dst->UpdateValidity(clamped_r);
		dst->UpdateValidBits(GSLocalMemory::m_psm[TEX0.PSM].fmsk);
	}
}
// Goal: Depth And Target at the same address is not possible. On GS it is
// the same memory but not on the Dx/GL. Therefore a write to the Depth/Target
// must invalidate the Target/Depth respectively
void GSTextureCache::InvalidateVideoMemType(int type, u32 bp)
{
	if (GSConfig.UserHacks_DisableDepthSupport)
		return;

	// The Getaway games need this function disabled for player shadows to work correctly.
	if (g_gs_renderer->m_game.title == CRC::GetawayGames)
		return;

	auto& list = m_dst[type];
	for (auto i = list.begin(); i != list.end(); ++i)
	{
		Target* t = *i;

		if (bp == t->m_TEX0.TBP0)
		{
			GL_CACHE("TC: InvalidateVideoMemType: Remove Target(%s) %d (0x%x)", to_string(type),
				t->m_texture ? t->m_texture->GetID() : 0,
				t->m_TEX0.TBP0);

			list.erase(i);
			delete t;

			break;
		}
	}
}

// Goal: invalidate data sent to the GPU when the source (GS memory) is modified
// Called each time you want to write to the GS memory
void GSTextureCache::InvalidateVideoMem(const GSOffset& off, const GSVector4i& rect, bool eewrite, bool target)
{
	u32 bp = off.bp();
	u32 bw = off.bw();
	u32 psm = off.psm();

	if (!target)
	{
		// Remove Source that have same BP as the render target (color&dss)
		// rendering will dirty the copy
		auto& list = m_src.m_map[bp >> 5];
		for (auto i = list.begin(); i != list.end();)
		{
			Source* s = *i;
			++i;

			if (GSUtil::HasSharedBits(bp, psm, s->m_TEX0.TBP0, s->m_TEX0.PSM) ||
				GSUtil::HasSharedBits(bp, psm, s->m_from_target_TEX0.TBP0, s->m_TEX0.PSM))
			{
				m_src.RemoveAt(s);
			}
		}

		u32 bbp = bp + bw * 0x10;
		if (bw >= 16 && bbp < 16384)
		{
			// Detect half of the render target (fix snow engine game)
			// Target Page (8KB) have always a width of 64 pixels
			// Half of the Target is TBW/2 pages * 8KB / (1 block * 256B) = 0x10
			auto& list = m_src.m_map[bbp >> 5];
			for (auto i = list.begin(); i != list.end();)
			{
				Source* s = *i;
				++i;

				if (GSUtil::HasSharedBits(bbp, psm, s->m_TEX0.TBP0, s->m_TEX0.PSM))
				{
					m_src.RemoveAt(s);
				}
			}
		}

		// Haunting ground write frame buffer 0x3000 and expect to write data to 0x3380
		// Note: the game only does a 0 direct write. If some games expect some real data
		// we are screwed.
		if (g_gs_renderer->m_game.title == CRC::HauntingGround)
		{
			u32 end_block = GSLocalMemory::m_psm[psm].info.bn(rect.z - 1, rect.w - 1, bp, bw); // Valid only for color formats
			auto type = RenderTarget;

			for (auto t : m_dst[type])
			{
				if (t->m_TEX0.TBP0 > bp && t->m_end_block <= end_block)
				{
					// Haunting ground expect to clean buffer B with a rendering into buffer A.
					// Situation is quite messy as it would require to extract the data from the buffer A
					// and to move in buffer B.
					//
					// Of course buffers don't share the same line width. You can't delete the buffer as next
					// miss will load invalid data.
					//
					// So just clear the damn buffer and forget about it.
					GL_CACHE("TC: Clear Sub Target(%s) %d (0x%x)", to_string(type),
						t->m_texture ? t->m_texture->GetID() : 0,
						t->m_TEX0.TBP0);
					g_gs_device->ClearRenderTarget(t->m_texture, 0);
					t->m_dirty.clear();
				}
			}
		}
	}

	bool found = false;
	// Previously: rect.ralign<Align_Outside>((bp & 31) == 0 ? GSLocalMemory::m_psm[psm].pgs : GSLocalMemory::m_psm[psm].bs)
	// But this causes rects to be too big, especially in WRC games, I don't think there's any need to align them here.
	GSVector4i r = rect;

	off.loopPages(rect, [&](u32 page)
	{
		auto& list = m_src.m_map[page];
		for (auto i = list.begin(); i != list.end();)
		{
			Source* s = *i;
			++i;

			if (GSUtil::HasSharedBits(psm, s->m_TEX0.PSM))
			{
				bool b = bp == s->m_TEX0.TBP0;

				if (!s->m_target)
				{
					found |= b;

					// No point keeping invalidated sources around when the hash cache is active,
					// we can just re-hash and create a new source from the cached texture.
					if (s->m_from_hash_cache || (GSConfig.UserHacks_DisablePartialInvalidation && s->m_repeating))
					{
						m_src.RemoveAt(s);
					}
					else
					{
						u32* RESTRICT valid = s->m_valid.get();

						if (valid && !s->CanPreload())
						{
							// Invalidate data of input texture
							if (s->m_repeating)
							{
								// Note: very hot path on snowbling engine game
								for (const GSVector2i& k : s->m_p2t[page])
								{
									valid[k.x] &= k.y;
								}
							}
							else
							{
								valid[page] = 0;
							}
						}

						s->m_complete_layers = 0;
					}
				}
				else
				{
					// render target used as input texture
					b |= bp == s->m_from_target_TEX0.TBP0;

					if (!b)
						b = s->Overlaps(bp, bw, psm, rect);

					if (b)
					{
						m_src.RemoveAt(s);
					}
				}
			}
		}
	});

	if (!target)
		return;

	// Handle the case where the transfer wrapped around the end of GS memory.
	const u32 end_bp = off.bnNoWrap(rect.z - 1, rect.w - 1);

	// Ideally in the future we can turn this on unconditionally, but for now it breaks too much.
	const bool check_inside_target = (GSConfig.UserHacks_TargetPartialInvalidation ||
									  GSConfig.UserHacks_TextureInsideRt != GSTextureInRtMode::Disabled);
	RGBAMask rgba;
	rgba._u32 = GSUtil::GetChannelMask(psm);

	for (int type = 0; type < 2; type++)
	{
		auto& list = m_dst[type];
		for (auto i = list.begin(); i != list.end();)
		{
			auto j = i;
			Target* t = *j;

			// Don't bother checking any further if the target doesn't overlap with the write/invalidation.
			if ((bp < t->m_TEX0.TBP0 && end_bp < t->m_TEX0.TBP0) || bp > t->UnwrappedEndBlock())
			{
				++i;
				continue;
			}

			// GH: (I think) this code is completely broken. Typical issue:
			// EE write an alpha channel into 32 bits texture
			// Results: the target is deleted (because HasCompatibleBits is false)
			//
			// Major issues are expected if the game try to reuse the target
			// If we dirty the RT, it will likely upload partially invalid data.
			// (The color on the previous example)
			if (GSUtil::HasSharedBits(bp, psm, t->m_TEX0.TBP0, t->m_TEX0.PSM))
			{
				if (!found && GSUtil::HasCompatibleBits(psm, t->m_TEX0.PSM) && bw == std::max(t->m_TEX0.TBW, 1U))
				{
					GL_CACHE("TC: Dirty Target(%s) %d (0x%x) r(%d,%d,%d,%d)", to_string(type),
						t->m_texture ? t->m_texture->GetID() : 0,
						t->m_TEX0.TBP0, r.x, r.y, r.z, r.w);

					if (eewrite)
						t->m_age = 0;
					
					AddDirtyRectTarget(t, r, psm, bw, rgba);
					
					++i;
					continue;
				}
				else
				{
					// YOLO skipping t->m_TEX0.TBW = bw; It would change the surface offset results...
					// This code exists because Destruction Derby Arenas uploads a 16x16 CLUT to the same BP as the depth buffer and invalidating the depth is bad (because it's not invalid).
					// Possibly because the block layout is opposite for the 32bit colour and depth, it never actually overwrites the depth, so this is kind of a miss detection.
					// The new code rightfully calculates that the depth does not become dirty, but in other cases, like bigger draws of the same format
					// it might become invalid, so we check below and erase as before if so.
					bool can_erase = true;
					if (!found && t->m_age <= 1)
					{
						// Compatible formats and same width, probably updating the same texture, so just mark it as dirty.
						if (bw == std::max(t->m_TEX0.TBW, 1U) && GSLocalMemory::m_psm[psm].bpp == GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp)
						{
							AddDirtyRectTarget(t, rect, psm, bw, rgba);
							GL_CACHE("TC: Direct Dirty in the middle [aggressive] of Target(%s) %d [PSM:%s BP:0x%x->0x%x BW:%u rect(%d,%d=>%d,%d)] write[PSM:%s BP:0x%x BW:%u rect(%d,%d=>%d,%d)]",
								to_string(type),
								t->m_texture ? t->m_texture->GetID() : 0,
								psm_str(t->m_TEX0.PSM),
								t->m_TEX0.TBP0,
								t->m_end_block,
								t->m_TEX0.TBW,
								rect.x,
								rect.y,
								rect.z,
								rect.w,
								psm_str(psm),
								bp,
								bw,
								r.x,
								r.y,
								r.z,
								r.w);

							can_erase = false;
						}
						else
						{
							// Incompatible format write, small writes *should* be okay (Destruction Derby Arenas) and matching bpp should be fine.
							// If it's overwriting a good chunk of the texture, it's more than likely a different texture, so kill it (Dragon Quest 8).
							const GSVector2i page_size = GSLocalMemory::m_psm[psm].pgs;
							const bool can_translate = CanTranslate(bp, bw, psm, r, t->m_TEX0.TBP0, t->m_TEX0.PSM, t->m_TEX0.TBW);
							const bool swizzle_match = GSLocalMemory::m_psm[psm].depth == GSLocalMemory::m_psm[t->m_TEX0.PSM].depth;

							if (can_translate)
							{
								// Alter echo match bit depth problem.
								if (swizzle_match)
								{
									DirtyRectByPage(bp, psm, bw, t, r, t->m_TEX0.TBP0, t->m_TEX0.PSM, t->m_TEX0.TBW);

									if (eewrite)
										t->m_age = 0;

									can_erase = t->m_dirty.GetTotalRect(t->m_TEX0, GSVector2i(t->m_valid.z, t->m_valid.w)).eq(t->m_valid);
								}
								else
								{
									// If it's not page aligned, grab the whole pages it covers, to be safe.
									GSVector4i new_rect = r;
									if (GSLocalMemory::m_psm[psm].bpp != GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp)
									{
										const GSVector2i dst_page_size = GSLocalMemory::m_psm[psm].pgs;
										new_rect = GSVector4i(new_rect.x / page_size.x, new_rect.y / page_size.y, (new_rect.z + (page_size.x - 1)) / page_size.x, (new_rect.w + (page_size.y - 1)) / page_size.y);
										new_rect = GSVector4i(new_rect.x * dst_page_size.x, new_rect.y * dst_page_size.y, new_rect.z * dst_page_size.x, new_rect.w * dst_page_size.y);
									}
									else
									{
										new_rect.x &= ~(page_size.x - 1);
										new_rect.y &= ~(page_size.y - 1);
										new_rect.z = (r.z + (page_size.x - 1)) & ~(page_size.x - 1);
										new_rect.w = (r.w + (page_size.y - 1)) & ~(page_size.y - 1);
									}
									DirtyRectByPage(bp & ~((1 << 5) - 1), psm, bw, t, new_rect, t->m_TEX0.TBP0, t->m_TEX0.PSM, t->m_TEX0.TBW);

									if (eewrite)
										t->m_age = 0;

									can_erase = t->m_dirty.GetTotalRect(t->m_TEX0, GSVector2i(t->m_valid.z, t->m_valid.w)).eq(t->m_valid);
								}
							}
							else
							{
								const GSLocalMemory::psm_t& t_psm_s = GSLocalMemory::m_psm[psm];
								const u32 bp_end = t_psm_s.info.bn(r.z - 1, r.w - 1, bp, bw);
								if (GSLocalMemory::m_psm[psm].bpp == GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp ||
									((100.0f / static_cast<float>(t->m_end_block - t->m_TEX0.TBP0)) * static_cast<float>(bp_end - bp)) < 20.0f)
								{
									SurfaceOffset so = ComputeSurfaceOffset(off, r, t);
									if (so.is_valid)
									{
										AddDirtyRectTarget(t, so.b2a_offset, psm, bw, rgba);
										GL_CACHE("TC: Dirty in the middle [aggressive] of Target(%s) %d [PSM:%s BP:0x%x->0x%x BW:%u rect(%d,%d=>%d,%d)] write[PSM:%s BP:0x%x BW:%u rect(%d,%d=>%d,%d)]",
											to_string(type),
											t->m_texture ? t->m_texture->GetID() : 0,
											psm_str(t->m_TEX0.PSM),
											t->m_TEX0.TBP0,
											t->m_end_block,
											t->m_TEX0.TBW,
											so.b2a_offset.x,
											so.b2a_offset.y,
											so.b2a_offset.z,
											so.b2a_offset.w,
											psm_str(psm),
											bp,
											bw,
											r.x,
											r.y,
											r.z,
											r.w);

										can_erase = false;
									}
								}
							}
						}
					}

					if (can_erase)
					{
						// If it's a 32bit value and only the alpha channel is being killed
						// instead of losing the RGB data, drop it back to 24bit.
						if (rgba._u32 == 0x8 && t->m_TEX0.PSM == PSM_PSMCT32)
						{
							t->m_TEX0.PSM = PSM_PSMCT24;
							t->m_dirty_alpha = false;
							++i;
						}
						else
						{
							i = list.erase(j);
							GL_CACHE("TC: Remove Target(%s) %d (0x%x)", to_string(type),
								t->m_texture ? t->m_texture->GetID() : 0,
								t->m_TEX0.TBP0);
							delete t;
						}
					}
					else
					{
						if (eewrite)
							t->m_age = 0;
						++i;
					}
					continue;
				}
			}
			else if (bp == t->m_TEX0.TBP0)
			{
				// EE writes the ALPHA channel. Mark it as invalid for
				// the texture cache. Otherwise it will generate a wrong
				// hit on the texture cache.
				// Game: Conflict - Desert Storm (flickering)
				t->m_dirty_alpha = false;
			}

			++i;

			// GH: Try to detect texture write that will overlap with a target buffer
			// TODO Use ComputeSurfaceOffset below.
			if (GSUtil::HasSharedBits(psm, t->m_TEX0.PSM))
			{
				if (bp < t->m_TEX0.TBP0)
				{
					const u32 rowsize = bw * 8192;
					const u32 offset = static_cast<u32>((t->m_TEX0.TBP0 - bp) * 256);

					if (rowsize > 0 && offset % rowsize == 0)
					{
						int y = GSLocalMemory::m_psm[psm].pgs.y * offset / rowsize;

						if (r.bottom > y)
						{
							GL_CACHE("TC: Dirty After Target(%s) %d (0x%x)", to_string(type),
								t->m_texture ? t->m_texture->GetID() : 0,
								t->m_TEX0.TBP0);

							if (eewrite)
								t->m_age = 0;

							const GSVector4i dirty_r = GSVector4i(r.left, r.top - y, r.right, r.bottom - y);
							AddDirtyRectTarget(t, dirty_r, psm, bw, rgba);
							continue;
						}
					}
				}

				// FIXME: this code "fixes" black FMV issue with rule of rose.
#if 1
				// Greg: I'm not sure the 'bw' equality is required but it won't hurt too much
				//
				// Ben 10 Alien Force : Vilgax Attacks uses a small temporary target for multiple textures (different bw)
				// It is too complex to handle, and purpose of the code was to handle FMV (large bw). So let's skip small
				// (128 pixels) target
				if (bw > 2 && t->m_TEX0.TBW == bw && t->Inside(bp, bw, psm, rect) && GSUtil::HasCompatibleBits(psm, t->m_TEX0.PSM))
				{
					const u32 rowsize = bw * 8192u;
					const u32 offset = static_cast<u32>((bp - t->m_TEX0.TBP0) * 256);

					if (offset % rowsize == 0)
					{
						const int y = GSLocalMemory::m_psm[psm].pgs.y * offset / rowsize;

						GL_CACHE("TC: Dirty in the middle of Target(%s) %d (0x%x->0x%x) pos(%d,%d => %d,%d) bw:%u", to_string(type),
							t->m_texture ? t->m_texture->GetID() : 0,
							t->m_TEX0.TBP0, t->m_end_block,
							r.left, r.top + y, r.right, r.bottom + y, bw);

						if (eewrite)
							t->m_age = 0;

						const GSVector4i dirty_r = GSVector4i(r.left, r.top + y, r.right, r.bottom + y);
						AddDirtyRectTarget(t, dirty_r, psm, bw, rgba);
						continue;
					}
				}
				else if (check_inside_target && t->Overlaps(bp, bw, psm, rect) && GSUtil::HasSharedBits(psm, t->m_TEX0.PSM))
				{
					// If it's compatible and page aligned, then handle it this way.
					// It's quicker, and Surface Offsets can get it wrong.
					// Example doing PSMT8H to C32, BP 0x1c80, TBP 0x1d80, incoming rect 0,128 -> 128,256
					// Surface offsets translates it to 0, 128 -> 128, 128, not 0, 0 -> 128, 128.

					// If the RT is more than 1 frame old and the new data just doesn't line up properly, it's probably not made for it, kill the old target.
					if (t->m_age > 1 && bw > 1 && bw != t->m_TEX0.TBW)
					{
						i = list.erase(j);
						GL_CACHE("TC: Tex in RT Remove Old Target(%s) %d (0x%x) TPSM %x PSM %x bp 0x%x", to_string(type),
							t->m_texture ? t->m_texture->GetID() : 0,
							t->m_TEX0.TBP0,
							t->m_TEX0.PSM,
							psm,
							bp);
						delete t;
						continue;
					}

					const GSVector2i page_size = GSLocalMemory::m_psm[psm].pgs;
					const bool can_translate = CanTranslate(bp, bw, psm, r, t->m_TEX0.TBP0, t->m_TEX0.PSM, t->m_TEX0.TBW);

					if (can_translate)
					{
						const bool swizzle_match = GSLocalMemory::m_psm[psm].depth == GSLocalMemory::m_psm[t->m_TEX0.PSM].depth;
						if (swizzle_match)
						{
							DirtyRectByPage(bp, psm, bw, t, r, t->m_TEX0.TBP0, t->m_TEX0.PSM, t->m_TEX0.TBW);

							if (eewrite)
								t->m_age = 0;
						}
						else
						{
							// If it's not page aligned, grab the whole pages it covers, to be safe.
							GSVector4i new_rect = r;
							new_rect.x &= ~(page_size.x - 1);
							new_rect.y &= ~(page_size.y - 1);
							new_rect.z = (r.z + (page_size.x - 1)) & ~(page_size.x - 1);
							new_rect.w = (r.w + (page_size.y - 1)) & ~(page_size.y - 1);

							DirtyRectByPage(bp & ~((1 << 5) - 1), psm, bw, t, new_rect, t->m_TEX0.TBP0, t->m_TEX0.PSM, t->m_TEX0.TBW);

							if (eewrite)
								t->m_age = 0;
						}
					}
					else
					{
						if (GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp >= 16 && GSLocalMemory::m_psm[psm].bpp <= 8)
						{
							// could be overwriting a double buffer, so if it's the second half of it, just reduce the size down to half.
							if (((t->m_end_block - t->m_TEX0.TBP0) >> 1) < (bp - t->m_TEX0.TBP0))
							{
								GSVector4i new_valid = t->m_valid;
								new_valid.w = new_valid.w / 2;
								t->ResizeValidity(new_valid);
							}
							else
							{
								i = list.erase(j);
								GL_CACHE("TC: Tex in RT Remove Target(%s) %d (0x%x) TPSM %x PSM %x bp 0x%x", to_string(type),
									t->m_texture ? t->m_texture->GetID() : 0,
									t->m_TEX0.TBP0,
									t->m_TEX0.PSM,
									psm,
									bp);
								delete t;
								continue;
							}
						}
						else
						{
							SurfaceOffsetKey sok;
							sok.elems[0].bp = bp;
							sok.elems[0].bw = bw;
							sok.elems[0].psm = psm;
							sok.elems[0].rect = r;
							sok.elems[1].bp = t->m_TEX0.TBP0;
							sok.elems[1].bw = t->m_TEX0.TBW;
							sok.elems[1].psm = t->m_TEX0.PSM;
							sok.elems[1].rect = t->m_valid;

							const SurfaceOffset so = ComputeSurfaceOffset(sok);
							if (so.is_valid)
							{
								if (eewrite)
									t->m_age = 0;

								AddDirtyRectTarget(t, so.b2a_offset, t->m_TEX0.PSM, t->m_TEX0.TBW, rgba);
							}
						}
					}
				}
#endif
			}
		}
	}
}

// Goal: retrive the data from the GPU to the GS memory.
// Called each time you want to read from the GS memory
void GSTextureCache::InvalidateLocalMem(const GSOffset& off, const GSVector4i& r)
{
	const u32 bp = off.bp();
	const u32 psm = off.psm();
	[[maybe_unused]] const u32 bw = off.bw();
	const u32 read_start = GSLocalMemory::m_psm[psm].info.bn(r.x, r.y, bp, bw);
	const u32 read_end = GSLocalMemory::m_psm[psm].info.bn(r.z - 1, r.w - 1, bp, bw);

	GL_CACHE("TC: InvalidateLocalMem off(0x%x, %u, %s) r(%d, %d => %d, %d)",
		bp,
		bw,
		psm_str(psm),
		r.x,
		r.y,
		r.z,
		r.w);

	const bool read_is_depth = (psm & 0x30) == 0x30;

	// Could be reading Z24/32 back as CT32 (Gundam Battle Assault 3)
	if (GSLocalMemory::m_psm[psm].bpp >= 16)
	{
		if (GSConfig.HWDownloadMode != GSHardwareDownloadMode::Enabled)
		{
			DevCon.Error("Skipping depth readback of %ux%u @ %u,%u", r.width(), r.height(), r.left, r.top);
			return;
		}

		bool z_found = false;

		if (!GSConfig.UserHacks_DisableDepthSupport)
		{
			auto& dss = m_dst[DepthStencil];
			for (auto it = dss.rbegin(); it != dss.rend(); ++it) // Iterate targets from LRU to MRU.
			{
				Target* t = *it;

				if (t->m_32_bits_fmt && t->m_TEX0.PSM > PSM_PSMZ24)
					t->m_TEX0.PSM = PSM_PSMZ32;

				// Check the offset of the read, if they're not pointing at or inside this texture, it's probably not what we want.
				//const bool expecting_this_tex = ((bp <= t->m_TEX0.TBP0 && read_start >= t->m_TEX0.TBP0) || bp >= t->m_TEX0.TBP0) && read_end <= t->m_end_block;
				const bool bpp_match = GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp == GSLocalMemory::m_psm[psm].bpp;
				const u32 page_mask = ((1 << 5) - 1);
				const bool expecting_this_tex = bpp_match && (((read_start & ~page_mask) == t->m_TEX0.TBP0) || (bp >= t->m_TEX0.TBP0 && ((read_end + page_mask) & ~page_mask) <= ((t->m_end_block + page_mask) & ~page_mask)));
				if (!expecting_this_tex)
					continue;

				z_found = true;

				t->readbacks_since_draw++;

				GSVector2i page_size = GSLocalMemory::m_psm[t->m_TEX0.PSM].pgs;
				const bool can_translate = CanTranslate(bp, bw, psm, r, t->m_TEX0.TBP0, t->m_TEX0.PSM, t->m_TEX0.TBW);
				const bool swizzle_match = GSLocalMemory::m_psm[psm].depth == GSLocalMemory::m_psm[t->m_TEX0.PSM].depth;
				// Calculate the rect offset if the BP doesn't match.
				GSVector4i targetr = {};
				if (t->readbacks_since_draw > 1)
				{
					targetr = t->m_drawn_since_read;
				}
				else if (can_translate)
				{
					if (swizzle_match)
					{
						targetr = TranslateAlignedRectByPage(bp, psm, bw, r, t->m_TEX0.TBP0, t->m_TEX0.PSM, t->m_TEX0.TBW, true);
					}
					else
					{
						// If it's not page aligned, grab the whole pages it covers, to be safe.
						GSVector4i new_rect = r;

						if (GSLocalMemory::m_psm[psm].bpp != GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp)
						{
							const GSVector2i dst_page_size = GSLocalMemory::m_psm[psm].pgs;
							new_rect = GSVector4i(new_rect.x / page_size.x, new_rect.y / page_size.y, (new_rect.z + (page_size.x - 1)) / page_size.x, (new_rect.w + (page_size.y - 1)) / page_size.y);
							new_rect = GSVector4i(new_rect.x * dst_page_size.x, new_rect.y * dst_page_size.y, new_rect.z * dst_page_size.x, new_rect.w * dst_page_size.y);
						}
						else
						{
							new_rect.x &= ~(page_size.x - 1);
							new_rect.y &= ~(page_size.y - 1);
							new_rect.z = (r.z + (page_size.x - 1)) & ~(page_size.x - 1);
							new_rect.w = (r.w + (page_size.y - 1)) & ~(page_size.y - 1);
						}
						if (new_rect.eq(GSVector4i::zero()))
						{

							SurfaceOffsetKey sok;
							sok.elems[0].bp = bp;
							sok.elems[0].bw = bw;
							sok.elems[0].psm = psm;
							sok.elems[0].rect = r;
							sok.elems[1].bp = t->m_TEX0.TBP0;
							sok.elems[1].bw = t->m_TEX0.TBW;
							sok.elems[1].psm = t->m_TEX0.PSM;
							sok.elems[1].rect = t->m_valid;

							new_rect = ComputeSurfaceOffset(sok).b2a_offset;
						}
						targetr = TranslateAlignedRectByPage(bp & ~((1 << 5) - 1), psm, bw, new_rect, t->m_TEX0.TBP0, t->m_TEX0.PSM, t->m_TEX0.TBW, true);
					}
				}
				else
				{
					targetr = t->m_drawn_since_read;
				}

				const GSVector4i draw_rect = (t->readbacks_since_draw > 1) ? t->m_drawn_since_read : targetr.rintersect(t->m_drawn_since_read);

				// Recently made this section dirty, no need to read it.
				if (draw_rect.rintersect(t->m_dirty.GetTotalRect(t->m_TEX0, t->m_unscaled_size)).eq(draw_rect))
					return;

				if (t->m_drawn_since_read.eq(GSVector4i::zero()))
				{
					if (draw_rect.rintersect(t->m_valid).eq(draw_rect))
						return;
					else
						continue;
				}

				Read(t, draw_rect);
				if (draw_rect.rintersect(t->m_drawn_since_read).eq(t->m_drawn_since_read))
					t->m_drawn_since_read = GSVector4i::zero();
			}
		}
		if (z_found)
			return;
	}

	// Games of note that use this for various effects/transfers which may cause problems.
	// Silent Hill Shattered Memories
	// Chaos Legion
	// Busin 0: Wizardry Alternative
	// Kingdom Hearts 2
	// Final Fantasy X
	// Dark Cloud 2
	// Dog's Life
	// SOCOM 2
	// Fatal Frame series
	auto& rts = m_dst[RenderTarget];

	for (int pass = 0; pass < 2; pass++)
	{
		for (auto it = rts.rbegin(); it != rts.rend(); it++) // Iterate targets from LRU to MRU.
		{
			Target* t = *it;

			if (t->m_32_bits_fmt && t->m_TEX0.PSM > PSM_PSMCT24)
				t->m_TEX0.PSM = PSM_PSMCT32;

			// pass 0 == Exact match, pass 1 == partial match
			if (pass == 0)
			{
				// Check exact match first
				const bool bpp_match = GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp == GSLocalMemory::m_psm[psm].bpp;
				const u32 page_mask = ((1 << 5) - 1);
				const bool expecting_this_tex = bpp_match && bw == t->m_TEX0.TBW && (((read_start & ~page_mask) == t->m_TEX0.TBP0) || (bp >= t->m_TEX0.TBP0 && ((read_end + page_mask) & ~page_mask) <= ((t->m_end_block + page_mask) & ~page_mask)));

				if (!expecting_this_tex)
					continue;
			}
			else
			{
				// Check loose matches if we still haven't got all the data.
				const bool expecting_this_tex = t->Overlaps(bp, bw, psm, r);

				if (!expecting_this_tex || !GSUtil::HasSharedBits(psm, t->m_TEX0.PSM))
					continue;
			}

			t->readbacks_since_draw++;

			GSVector2i page_size = GSLocalMemory::m_psm[t->m_TEX0.PSM].pgs;
			const bool can_translate = CanTranslate(bp, bw, psm, r, t->m_TEX0.TBP0, t->m_TEX0.PSM, t->m_TEX0.TBW);
			const bool swizzle_match = GSLocalMemory::m_psm[psm].depth == GSLocalMemory::m_psm[t->m_TEX0.PSM].depth;
			// Calculate the rect offset if the BP doesn't match.
			GSVector4i targetr = {};
			if (t->readbacks_since_draw > 1)
			{
				targetr = t->m_drawn_since_read;
			}
			else if (can_translate)
			{
				if (swizzle_match)
				{
					targetr = TranslateAlignedRectByPage(bp, psm, bw, r, t->m_TEX0.TBP0, t->m_TEX0.PSM, t->m_TEX0.TBW, true);
				}
				else
				{
					// If it's not page aligned, grab the whole pages it covers, to be safe.
					GSVector4i new_rect = r;

					if (GSLocalMemory::m_psm[psm].bpp != GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp)
					{
						const GSVector2i dst_page_size = GSLocalMemory::m_psm[psm].pgs;
						new_rect = GSVector4i(new_rect.x / page_size.x, new_rect.y / page_size.y, (new_rect.z + (page_size.x - 1)) / page_size.x, (new_rect.w + (page_size.y - 1)) / page_size.y);
						new_rect = GSVector4i(new_rect.x * dst_page_size.x, new_rect.y * dst_page_size.y, new_rect.z * dst_page_size.x, new_rect.w * dst_page_size.y);
					}
					else
					{
						new_rect.x &= ~(page_size.x - 1);
						new_rect.y &= ~(page_size.y - 1);
						new_rect.z = (r.z + (page_size.x - 1)) & ~(page_size.x - 1);
						new_rect.w = (r.w + (page_size.y - 1)) & ~(page_size.y - 1);
					}
					if (new_rect.eq(GSVector4i::zero()))
					{

						SurfaceOffsetKey sok;
						sok.elems[0].bp = bp;
						sok.elems[0].bw = bw;
						sok.elems[0].psm = psm;
						sok.elems[0].rect = r;
						sok.elems[1].bp = t->m_TEX0.TBP0;
						sok.elems[1].bw = t->m_TEX0.TBW;
						sok.elems[1].psm = t->m_TEX0.PSM;
						sok.elems[1].rect = t->m_valid;

						new_rect = ComputeSurfaceOffset(sok).b2a_offset;
					}
					targetr = TranslateAlignedRectByPage(bp & ~((1 << 5) - 1), psm, bw, new_rect, t->m_TEX0.TBP0, t->m_TEX0.PSM, t->m_TEX0.TBW, true);
				}
			}
			else
			{
				targetr = t->m_drawn_since_read;
			}

			if (t->m_drawn_since_read.eq(GSVector4i::zero()))
			{
				if (targetr.rintersect(t->m_valid).eq(targetr))
					return;
				else
					continue;
			}

			// Recently made this section dirty, no need to read it.
			if (targetr.rintersect(t->m_dirty.GetTotalRect(t->m_TEX0, t->m_unscaled_size)).eq(targetr))
				return;

			if (!targetr.rempty())
			{
				if (GSConfig.HWDownloadMode != GSHardwareDownloadMode::Enabled)
				{
					DevCon.Error("Skipping depth readback of %ux%u @ %u,%u", targetr.width(), targetr.height(), targetr.left, targetr.top);
					continue;
				}

				Read(t, targetr.rintersect(t->m_drawn_since_read));

				// Try to cut down how much we read next, if we can.
				// Fatal Frame reads in vertical strips, SOCOM 2 does horizontal, so we can handle that below.
				if (t->m_drawn_since_read.rintersect(targetr).eq(t->m_drawn_since_read))
				{
					t->m_drawn_since_read = GSVector4i::zero();
				}
				else if (targetr.width() == t->m_drawn_since_read.width()
					&& targetr.w >= t->m_drawn_since_read.y)
				{
					if (targetr.y <= t->m_drawn_since_read.y)
						t->m_drawn_since_read.y = targetr.w;
					else if (targetr.w >= t->m_drawn_since_read.w)
						t->m_drawn_since_read.w = targetr.y;
				}
				else if (targetr.height() == t->m_drawn_since_read.height()
					&& targetr.z >= t->m_drawn_since_read.x)
				{
					if (targetr.x <= t->m_drawn_since_read.x)
						t->m_drawn_since_read.x = targetr.z;
					else if (targetr.z >= t->m_drawn_since_read.z)
						t->m_drawn_since_read.z = targetr.x;
				}

				if (targetr.rintersect(t->m_valid).eq(targetr))
					return;
			}
		}
	}
}

bool GSTextureCache::Move(u32 SBP, u32 SBW, u32 SPSM, int sx, int sy, u32 DBP, u32 DBW, u32 DPSM, int dx, int dy, int w, int h)
{
	if (SBP == DBP && SPSM == DPSM && !GSLocalMemory::m_psm[SPSM].depth && ShuffleMove(SBP, SBW, SPSM, sx, sy, dx, dy, w, h))
	{
		return true;
	}

	// TODO: In theory we could do channel swapping on the GPU, but we haven't found anything which needs it so far.
	// Same with SBP == DBP, but this behavior could change based on direction?
	if (SPSM != DPSM || ((SBP == DBP) && !(GSVector4i(sx, sy, sx + w, sy + h).rintersect(GSVector4i(dx, dy, dx + w, dy + h))).rempty()))
	{
		GL_CACHE("Skipping HW move from 0x%X to 0x%X with SPSM=%u DPSM=%u", SBP, DBP, SPSM, DPSM);
		return false;
	}

	// DX11/12 is a bit lame and can't partial copy depth targets. We could do this with a blit instead,
	// but so far haven't seen anything which needs it.
	if (GSConfig.Renderer == GSRendererType::DX11 || GSConfig.Renderer == GSRendererType::DX12)
	{
		if (GSLocalMemory::m_psm[SPSM].depth || GSLocalMemory::m_psm[DPSM].depth)
			return false;
	}

	// Look for an exact match on the targets.
	GSTextureCache::Target* src = GetExactTarget(SBP, SBW, SPSM);
	GSTextureCache::Target* dst = GetExactTarget(DBP, DBW, DPSM);

	// Beware of the case where a game might create a larger texture by moving a bunch of chunks around.
	// We use dx/dy == 0 and the TBW check as a safeguard to make sure these go through to local memory.
	// Good test case for this is the Xenosaga I cutscene transitions, or Gradius V.
	if (src && !dst && dx == 0 && dy == 0 && ((static_cast<u32>(w) + 63) / 64) <= DBW)
	{
		GIFRegTEX0 new_TEX0 = {};
		new_TEX0.TBP0 = DBP;
		new_TEX0.TBW = DBW;
		new_TEX0.PSM = DPSM;

		const int real_height = GetTargetHeight(DBP, DBW, DPSM, h);
		dst = LookupTarget(new_TEX0, GSVector2i(static_cast<int>(Common::AlignUpPow2(w, 64)),
			static_cast<int>(real_height)), src->m_scale, src->m_type, true);
		if (dst)
		{
			dst->UpdateValidity(GSVector4i(dx, dy, dx + w, dy + h));
			dst->m_valid_bits = src->m_valid_bits;
			dst->OffsetHack_modxy = src->OffsetHack_modxy;
		}
	}

	if (!src || !dst || src->m_scale != dst->m_scale)
		return false;

	// Scale coordinates.
	const float scale = src->m_scale;
	const int scaled_sx = static_cast<int>(sx * scale);
	const int scaled_sy = static_cast<int>(sy * scale);
	const int scaled_dx = static_cast<int>(dx * scale);
	const int scaled_dy = static_cast<int>(dy * scale);
	const int scaled_w = static_cast<int>(w * scale);
	const int scaled_h = static_cast<int>(h * scale);

	// The source isn't in our texture, otherwise it could falsely expand the texture causing a misdetection later, which then renders black.
	if ((scaled_sx + scaled_w) > src->m_texture->GetWidth() || (scaled_sy + scaled_h) > src->m_texture->GetHeight())
		return false;

	// We don't want to copy "old" data that the game has overwritten with writes,
	// so flush any overlapping dirty area.
	src->UpdateIfDirtyIntersects(GSVector4i(sx, sy, sx + w, sy + h));

	// The main point of HW moves is so GPU data can get used as sources. If we don't flush all writes,
	// we're not going to be able to use it as a source.
	dst->Update(true);

	// Expand the target when we used a more conservative size.
	const int required_dh = scaled_dy + scaled_h;
	if ((scaled_dx + scaled_w) <= dst->m_texture->GetWidth() && required_dh > dst->m_texture->GetHeight())
	{
		int new_height = dy + h;
		if (new_height > GSRendererHW::MAX_FRAMEBUFFER_HEIGHT)
			return false;

		// Align height to page size, that way we don't do too many small resizes (Dark Cloud).
		new_height = Common::AlignUpPow2(new_height, static_cast<unsigned>(GSLocalMemory::m_psm[DPSM].bs.y));

		// We don't recycle the old texture here, because the height cache will track the new size,
		// so the old size won't get created again.
		GL_INS("Resize %dx%d target to %dx%d for move", dst->m_unscaled_size.x, dst->m_unscaled_size.y, dst->m_unscaled_size.x, new_height);
		GetTargetHeight(DBP, DBW, DPSM, new_height);

		if (!dst->ResizeTexture(dst->m_unscaled_size.x, new_height, false))
		{
			// Resize failed, probably ran out of VRAM, better luck next time. Fall back to CPU.
			// We injected the new height into the cache, so hopefully won't happen again.
			return false;
		}
	}

	// Make sure the copy doesn't go out of bounds (it shouldn't).
	if ((scaled_dx + scaled_w) > dst->m_texture->GetWidth() || (scaled_dy + scaled_h) > dst->m_texture->GetHeight())
		return false;
	GL_CACHE("HW Move 0x%x to 0x%x <%d,%d->%d,%d> -> <%d,%d->%d,%d>", SBP, DBP, sx, sy, sx + w, sy, h, dx, dy, dx + w, dy + h);
	g_gs_device->CopyRect(src->m_texture, dst->m_texture,
		GSVector4i(scaled_sx, scaled_sy, scaled_sx + scaled_w, scaled_sy + scaled_h),
		scaled_dx, scaled_dy);

	dst->UpdateValidity(GSVector4i(dx, dy, dx + w, dy + h));
	// Invalidate any sources that overlap with the target (since they're now stale).
	InvalidateVideoMem(g_gs_renderer->m_mem.GetOffset(DBP, DBW, DPSM), GSVector4i(dx, dy, dx + w, dy + h), false, false);
	return true;
}

bool GSTextureCache::ShuffleMove(u32 BP, u32 BW, u32 PSM, int sx, int sy, int dx, int dy, int w, int h)
{
	// What are we doing here? Final Fantasy XII uses moves to copy the contents of the RG channels to the BA channels,
	// by rendering in PSMCT32, and doing a PSMCT16 move with an 8x0 offset on the destination. This effectively reads
	// from the original red/green channels, and writes to the blue/alpha channels. Who knows why they did it this way,
	// when they could've used sprites, but it means that they had to offset the block pointer for each move. So, we
	// need to use tex-in-rt here to figure out what the offset into the original PSMCT32 texture was, and HLE the move.
	if (PSM != PSM_PSMCT16)
		return false;

	GL_CACHE("Trying ShuffleMove: BP=%04X BW=%u PSM=%u SX=%d SY=%d DX=%d DY=%d W=%d H=%d", BP, BW, PSM, sx, sy, dx, dy, w, h);

	GSTextureCache::Target* tgt = nullptr;
	for (auto t : m_dst[RenderTarget])
	{
		if (t->m_TEX0.PSM == PSM_PSMCT32 && BP >= t->m_TEX0.TBP0 && BP <= t->m_end_block)
		{
			const SurfaceOffset so(ComputeSurfaceOffset(BP, BW, PSM, GSVector4i(sx, sy, sx + w, sy + h), t));
			if (so.is_valid)
			{
				tgt = t;
				GL_CACHE("ShuffleMove: Surface offset %d,%d from BP %04X - %04X", so.b2a_offset.x, so.b2a_offset.y, BP, t->m_TEX0.TBP0);
				sx += so.b2a_offset.x;
				sy += so.b2a_offset.y;
				dx += so.b2a_offset.x;
				dy += so.b2a_offset.y;
				break;
			}
		}
	}
	if (!tgt)
	{
		GL_CACHE("ShuffleMove: No target found");
		return false;
	}

	// Since we're only concerned with 32->16 shuffles, the difference should be 8x8 for this to work.
	const s32 diff_x = (dx - sx);
	if (std::abs(diff_x) != 8 || sy != dy)
	{
		GL_CACHE("ShuffleMove: Difference is not 8 pixels");
		return false;
	}

	const bool read_ba = (diff_x < 0);
	const bool write_rg = (diff_x < 0);

	const GSVector4i bbox(write_rg ? GSVector4i(dx, dy, dx + w, dy + h) : GSVector4i(sx, sy, sx + w, sy + h));

	GSVertex vertices[4] = {};
#define V(i, x, y, u, v) \
	do \
	{ \
		vertices[i].XYZ.X = x; \
		vertices[i].XYZ.Y = y; \
		vertices[i].U = u; \
		vertices[i].V = v; \
	} while (0)

	const GSVector4i bbox_fp(bbox.sll32(4));
	V(0, bbox_fp.x, bbox_fp.y, bbox_fp.x, bbox_fp.y); // top-left
	V(1, bbox_fp.z, bbox_fp.y, bbox_fp.z, bbox_fp.y); // top-right
	V(2, bbox_fp.x, bbox_fp.w, bbox_fp.x, bbox_fp.w); // bottom-left
	V(3, bbox_fp.z, bbox_fp.w, bbox_fp.z, bbox_fp.w); // bottom-right

#undef V

	static constexpr u32 indices[6] = { 0, 1, 2, 2, 1, 3 };

	// If we ever do this sort of thing somewhere else, extract this to a helper function.
	GSHWDrawConfig config;
	config.rt = tgt->m_texture;
	config.ds = nullptr;
	config.tex = tgt->m_texture;
	config.pal = nullptr;
	config.indices = indices;
	config.verts = vertices;
	config.nverts = static_cast<u32>(std::size(vertices));
	config.nindices = static_cast<u32>(std::size(indices));
	config.indices_per_prim = 3;
	config.drawlist = nullptr;
	config.scissor = GSVector4i(0, 0, tgt->m_texture->GetWidth(), tgt->m_texture->GetHeight());
	config.drawarea = GSVector4i(GSVector4(bbox) * GSVector4(tgt->m_scale));
	config.topology = GSHWDrawConfig::Topology::Triangle;
	config.blend = GSHWDrawConfig::BlendState();
	config.depth = GSHWDrawConfig::DepthStencilSelector::NoDepth();
	config.colormask = GSHWDrawConfig::ColorMaskSelector();
	config.colormask.wrgba = (write_rg ? (1 | 2) : (4 | 8));
	config.require_one_barrier = !g_gs_device->Features().framebuffer_fetch;
	config.require_full_barrier = false;
	config.destination_alpha = GSHWDrawConfig::DestinationAlphaMode::Off;
	config.datm = false;
	config.line_expand = false;
	config.separate_alpha_pass = false;
	config.second_separate_alpha_pass = false;
	config.alpha_second_pass.enable = false;
	config.vs.key = 0;
	config.vs.tme = true;
	config.vs.iip = true;
	config.vs.fst = true;
	config.gs.key = 0;
	config.ps.key_lo = 0;
	config.ps.key_hi = 0;
	config.ps.read_ba = read_ba;
	config.ps.write_rg = write_rg;
	config.ps.shuffle = true;
	config.ps.iip = true;
	config.ps.fst = true;
	config.ps.tex_is_fb = true;
	config.ps.tfx = TFX_DECAL;

	const GSVector2i rtsize(tgt->m_texture->GetSize());
	const float rtscale = tgt->m_scale;
	config.cb_ps.WH = GSVector4(static_cast<float>(rtsize.x) / rtscale, static_cast<float>(rtsize.y) / rtscale,
		static_cast<float>(rtsize.x), static_cast<float>(rtsize.y));
	config.cb_ps.STScale = GSVector2(1.0f);

	config.cb_vs.vertex_scale = GSVector2(2.0f * rtscale / (rtsize.x << 4), 2.0f * rtscale / (rtsize.y << 4));
	config.cb_vs.vertex_offset = GSVector2(-1.0f / rtsize.x + 1.0f, -1.0f / rtsize.y + 1.0f);
	config.cb_vs.texture_scale = GSVector2((1.0f / 16.0f) / config.cb_ps.WH.x, (1.0f / 16.0f) / config.cb_ps.WH.y);

	g_gs_device->RenderHW(config);
	return true;
}

GSTextureCache::Target* GSTextureCache::GetExactTarget(u32 BP, u32 BW, u32 PSM) const
{
	auto& rts = m_dst[GSLocalMemory::m_psm[PSM].depth ? DepthStencil : RenderTarget];
	for (auto it = rts.begin(); it != rts.end(); ++it) // Iterate targets from MRU to LRU.
	{
		Target* t = *it;
		if (t->m_TEX0.TBP0 == BP && t->m_TEX0.TBW == BW && t->m_TEX0.PSM == PSM)
			return t;
	}

	return nullptr;
}

GSTextureCache::Target* GSTextureCache::GetTargetWithSharedBits(u32 BP, u32 PSM) const
{
	auto& rts = m_dst[GSLocalMemory::m_psm[PSM].depth ? DepthStencil : RenderTarget];
	for (auto it = rts.begin(); it != rts.end(); ++it) // Iterate targets from MRU to LRU.
	{
		Target* t = *it;
		const u32 t_psm = (t->m_dirty_alpha) ? t->m_TEX0.PSM & ~0x1 : t->m_TEX0.PSM;
		if (GSUtil::HasSharedBits(BP, PSM, t->m_TEX0.TBP0, t_psm))
			return t;
	}

	return nullptr;
}

u32 GSTextureCache::GetTargetHeight(u32 fbp, u32 fbw, u32 psm, u32 min_height)
{
	TargetHeightElem search = {};
	search.fbp = fbp;
	search.fbw = fbw;
	search.psm = psm;
	search.height = min_height;

	for (auto it = m_target_heights.begin(); it != m_target_heights.end(); ++it)
	{
		TargetHeightElem& elem = const_cast<TargetHeightElem&>(*it);
		if (elem.bits == search.bits)
		{
			if (elem.height < min_height)
			{
				DbgCon.WriteLn("Expand height at %x %u %u from %u to %u", fbp, fbw, psm, elem.height, min_height);
				elem.height = min_height;
			}

			m_target_heights.MoveFront(it.Index());
			elem.age = 0;
			return elem.height;
		}
	}

	DbgCon.WriteLn("New height at %x %u %u: %u", fbp, fbw, psm, min_height);
	m_target_heights.push_front(search);
	return min_height;
}

bool GSTextureCache::Has32BitTarget(u32 bp)
{
	// Look for 32-bit targets at the matching block.
	for (auto i = m_dst[RenderTarget].begin(); i != m_dst[RenderTarget].end(); ++i)
	{
		const Target* const t = *i;
		if (bp == t->m_TEX0.TBP0 && t->m_32_bits_fmt)
		{
			// May as well move it to the front, because we're going to be looking it up again.
			m_dst[RenderTarget].MoveFront(i.Index());
			return true;
		}
	}

	// Try depth.
	for (auto i = m_dst[DepthStencil].begin(); i != m_dst[DepthStencil].end(); ++i)
	{
		const Target* const t = *i;
		if (bp == t->m_TEX0.TBP0 && t->m_32_bits_fmt)
		{
			// May as well move it to the front, because we're going to be looking it up again.
			m_dst[DepthStencil].MoveFront(i.Index());
			return true;
		}
	}

	return false;
}

// Hack: remove Target that are strictly included in current rt. Typically uses for FMV
// For example, game is rendered at 0x800->0x1000, fmv will be uploaded to 0x0->0x2800
// FIXME In theory, we ought to report the data from the sub rt to the main rt. But let's
// postpone it for later.
void GSTextureCache::InvalidateVideoMemSubTarget(GSTextureCache::Target* rt)
{
	if (!rt)
		return;

	auto& list = m_dst[RenderTarget];

	for (auto i = list.begin(); i != list.end();)
	{
		Target* t = *i;

		if ((t->m_TEX0.TBP0 > rt->m_TEX0.TBP0) && (t->m_end_block < rt->m_end_block) && (t->m_TEX0.TBW == rt->m_TEX0.TBW) && (t->m_TEX0.TBP0 < t->m_end_block))
		{
			GL_INS("InvalidateVideoMemSubTarget: rt 0x%x -> 0x%x, sub rt 0x%x -> 0x%x",
				rt->m_TEX0.TBP0, rt->m_end_block, t->m_TEX0.TBP0, t->m_end_block);

			i = list.erase(i);
			delete t;
		}
		else
		{
			++i;
		}
	}
}

void GSTextureCache::IncAge()
{
	const int max_age = m_src.m_used ? 3 : 6;
	const int max_preload_age = m_src.m_used ? 30 : 60;

	// You can't use m_map[page] because Source* are duplicated on several pages.
	for (auto i = m_src.m_surfaces.begin(); i != m_src.m_surfaces.end();)
	{
		Source* s = *i;

		if (s->m_shared_texture)
		{
			// Shared textures are temporary only added in the hash set but not in the texture
			// cache list therefore you can't use RemoveAt
			i = m_src.m_surfaces.erase(i);
			delete s;
		}
		else
		{
			++i;
			if (++s->m_age > (s->CanPreload() ? max_preload_age : max_age))
			{
				m_src.RemoveAt(s);
			}
		}
	}

	const u32 max_hash_cache_age = 30;
	for (auto it = m_hash_cache.begin(); it != m_hash_cache.end();)
	{
		HashCacheEntry& e = it->second;
		if (e.refcount == 0 && ++e.age > max_hash_cache_age)
		{
			const u32 mem_usage = e.texture->GetMemUsage();
			if (e.is_replacement)
				m_hash_cache_replacement_memory_usage -= mem_usage;
			else
				m_hash_cache_memory_usage -= mem_usage;
			g_gs_device->Recycle(e.texture);
			m_hash_cache.erase(it++);
		}
		else
		{
			++it;
		}
	}

	m_src.m_used = false;

	// Clearing of Rendertargets causes flickering in many scene transitions.
	// Sigh, this seems to be used to invalidate surfaces. So set a huge maxage to avoid flicker,
	// but still invalidate surfaces. (Disgaea 2 fmv when booting the game through the BIOS)
	// Original maxage was 4 here, Xenosaga 2 needs at least 240, else it flickers on scene transitions.
	static constexpr int max_rt_age = 400; // ffx intro scene changes leave the old image untouched for a couple of frames and only then start using it

	for (int type = 0; type < 2; type++)
	{
		auto& list = m_dst[type];
		for (auto i = list.begin(); i != list.end();)
		{
			Target* t = *i;

			// This variable is used to detect the texture shuffle effect. There is a high
			// probability that game will do it on the current RT.
			// Variable is cleared here to avoid issue with game that uses a 16 bits
			// render target
			if (t->m_age > 0)
			{
				// GoW2 uses the effect at the start of the frame
				t->m_32_bits_fmt = false;
			}

			if (++t->m_age > max_rt_age)
			{
				i = list.erase(i);
				GL_CACHE("TC: Remove Target(%s): %d (0x%x) due to age", to_string(type),
					t->m_texture ? t->m_texture->GetID() : 0,
					t->m_TEX0.TBP0);

				delete t;
			}
			else
			{
				++i;
			}
		}
	}

	for (auto it = m_target_heights.begin(); it != m_target_heights.end();)
	{
		TargetHeightElem& elem = const_cast<TargetHeightElem&>(*it);
		if (elem.age >= max_rt_age)
		{
			it = m_target_heights.erase(it);
		}
		else
		{
			elem.age++;
			++it;
		}
	}
}

//Fixme: Several issues in here. Not handling depth stencil, pitch conversion doesnt work.
GSTextureCache::Source* GSTextureCache::CreateSource(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, Target* dst, bool half_right, int x_offset, int y_offset, const GSVector2i* lod, const GSVector4i* src_range, GSTexture* gpu_clut, SourceRegion region)
{
	const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[TEX0.PSM];
	Source* src = new Source(TEX0, TEXA);

	// Normally we wouldn't use the region with targets, but for the case where we're drawing UVs and the
	// clamp rectangle exceeds the TW/TH (which is now unused), we do need to use it. Timesplitters 2 does
	// its frame blending effect using a smaller TW/TH, *and* triangles instead of sprites just to be extra
	// annoying.
	// Be careful with offset targets as we can end up sampling the wrong part/not enough, but TW/TH can be nonsense, so we take the biggest one if there is an RT(dst).
	// DBZ BT3 uses a region clamp offset when processing 2 player split screen and they set it 1 pixel too wide, meaning this code gets triggered.
	// TS2 has junk small TW and TH, but the region makes more sense for the draw.
	int tw = std::max(region.IsFixedTEX0W(1 << TEX0.TW) ? static_cast<int>(region.GetWidth()) : (1 << TEX0.TW), dst ? (1 << TEX0.TW) : 0);
	int th = std::max(region.IsFixedTEX0H(1 << TEX0.TH) ? static_cast<int>(region.GetHeight()) : (1 << TEX0.TH), dst ? (1 << TEX0.TH) : 0);

	int tlevels = 1;
	if (lod)
	{
		// lod won't contain the full range when using basic mipmapping, only that
		// which is hashed, so we just allocate the full thing.
		tlevels = (GSConfig.HWMipmap != HWMipmapLevel::Full) ? -1 : (lod->y - lod->x + 1);
		src->m_lod = *lod;
	}

	bool hack = false;

	if (dst && (x_offset != 0 || y_offset != 0))
	{
		const float scale = dst->m_scale;
		const int x = static_cast<int>(scale * x_offset);
		const int y = static_cast<int>(scale * y_offset);
		const int w = static_cast<int>(std::ceil(scale * tw));
		const int h = static_cast<int>(std::ceil(scale * th));

		// if we have a source larger than the target (from tex-in-rt), we need to clear it, otherwise we'll read junk
		const bool outside_target = ((x + w) > dst->m_texture->GetWidth() || (y + h) > dst->m_texture->GetHeight());
		GSTexture* sTex = dst->m_texture;
		GSTexture* dTex = outside_target ?
							  g_gs_device->CreateRenderTarget(w, h, GSTexture::Format::Color, true) :
							  g_gs_device->CreateTexture(w, h, tlevels, GSTexture::Format::Color, true);
		m_source_memory_usage += dTex->GetMemUsage();

		// copy the rt in
		const GSVector4i area(GSVector4i(x, y, x + w, y + h).rintersect(GSVector4i(sTex->GetSize()).zwxy()));
		if (!area.rempty())
			g_gs_device->CopyRect(sTex, dTex, area, 0, 0);

		// Keep a trace of origin of the texture
		src->m_texture = dTex;
		src->m_scale = scale;
		src->m_unscaled_size = GSVector2i(tw, th);
		src->m_end_block = dst->m_end_block;
		src->m_target = true;
		src->m_from_target = &dst->m_texture;
		src->m_from_target_TEX0 = dst->m_TEX0;

		if (psm.pal > 0)
		{
			// Attach palette for GPU texture conversion
			AttachPaletteToSource(src, psm.pal, true);
		}
	}
	else if (dst && GSRendererHW::GetInstance()->UpdateTexIsFB(dst, TEX0))
	{
		// This shortcut is a temporary solution. It isn't a good solution
		// as it won't work with Channel Shuffle/Texture Shuffle pattern
		// (we need texture cache result to detect those effects).
		// Instead a better solution would be to defer the copy/StrechRect later
		// in the rendering.
		// Still this poor solution is enough for a huge speed up in a couple of games
		//
		// Be aware that you can't use StrechRect between BeginScene/EndScene.
		// So it could be tricky to put in the middle of the DrawPrims

		// Keep a trace of origin of the texture
		src->m_texture = dst->m_texture;
		src->m_scale = dst->m_scale;
		src->m_unscaled_size = dst->m_unscaled_size;
		src->m_target = true;
		src->m_shared_texture = true;
		src->m_from_target = &dst->m_texture;
		src->m_from_target_TEX0 = dst->m_TEX0;
		src->m_end_block = dst->m_end_block;
		src->m_32_bits_fmt = dst->m_32_bits_fmt;

		// Even if we sample the framebuffer directly we might need the palette
		// to handle the format conversion on GPU
		if (psm.pal > 0)
			AttachPaletteToSource(src, psm.pal, true);

		// This will get immediately invalidated.
		m_temporary_source = src;
	}
	else if (dst)
	{
		// TODO: clean up this mess

		ShaderConvert shader = dst->m_type != RenderTarget ? ShaderConvert::FLOAT32_TO_RGBA8 : ShaderConvert::COPY;
		const bool is_8bits = TEX0.PSM == PSM_PSMT8;

		if (is_8bits)
		{
			GL_INS("Reading RT as a packed-indexed 8 bits format");
			shader = ShaderConvert::RGBA_TO_8I;
		}

#ifdef ENABLE_OGL_DEBUG
		if (TEX0.PSM == PSM_PSMT4)
		{
			GL_INS("ERROR: Reading RT as a packed-indexed 4 bits format is not supported");
		}
#endif

		if (GSLocalMemory::m_psm[TEX0.PSM].bpp > 8)
		{
			src->m_32_bits_fmt = dst->m_32_bits_fmt;
		}

		// Keep a trace of origin of the texture
		src->m_target = true;
		src->m_unscaled_size = GSVector2i(std::min(dst->m_unscaled_size.x, tw), std::min(dst->m_unscaled_size.y, th));
		src->m_from_target = &dst->m_texture;
		src->m_from_target_TEX0 = dst->m_TEX0;
		src->m_valid_rect = dst->m_valid;
		src->m_end_block = dst->m_end_block;

		dst->Update(true);

		// Rounding up should never exceed the texture size (since it itself should be rounded up), but just in case.
		GSVector2i new_size(
			std::min(static_cast<int>(std::ceil(static_cast<float>(src->m_unscaled_size.x) * dst->m_scale)),
				dst->m_texture->GetWidth()),
			std::min(static_cast<int>(std::ceil(static_cast<float>(src->m_unscaled_size.y) * dst->m_scale)),
				dst->m_texture->GetHeight()));

		if (is_8bits)
		{
			// Unscale 8 bits textures, quality won't be nice but format is really awful
			src->m_unscaled_size.x = tw;
			src->m_unscaled_size.y = th;
			new_size.x = tw;
			new_size.y = th;
		}

		// pitch conversion

		if (dst->m_TEX0.TBW != TEX0.TBW) // && dst->m_TEX0.PSM == TEX0.PSM
		{
			// This is so broken :p
			////Better not do the code below, "fixes" like every game that ever gets here..
			////Edit: Ratchet and Clank needs this to show most of it's graphics at all.
			////Someone else fix this please, I can't :p
			////delete src; return NULL;

			//// sfex3 uses this trick (bw: 10 -> 5, wraps the right side below the left)

			//ASSERT(dst->m_TEX0.TBW > TEX0.TBW); // otherwise scale.x need to be reduced to make the larger texture fit (TODO)

			//src->m_texture = g_gs_device->CreateRenderTarget(dstsize.x, dstsize.y, false);

			//GSVector4 size = GSVector4(dstsize).xyxy();
			//GSVector4 scale = GSVector4(dst->m_texture->GetScale()).xyxy();

			//int blockWidth  = 64;
			//int blockHeight = TEX0.PSM == PSM_PSMCT32 || TEX0.PSM == PSM_PSMCT24 ? 32 : 64;

			//GSVector4i br(0, 0, blockWidth, blockHeight);

			//int sw = (int)dst->m_TEX0.TBW << 6;

			//int dw = (int)TEX0.TBW << 6;
			//int dh = 1 << TEX0.TH;

			//if (sw != 0)
			//for (int dy = 0; dy < dh; dy += blockHeight)
			//{
			//	for (int dx = 0; dx < dw; dx += blockWidth)
			//	{
			//		int off = dy * dw / blockHeight + dx;

			//		int sx = off % sw;
			//		int sy = off / sw;

			//		GSVector4 sRect = GSVector4(GSVector4i(sx, sy).xyxy() + br) * scale / size;
			//		GSVector4 dRect = GSVector4(GSVector4i(dx, dy).xyxy() + br) * scale;

			//		g_gs_device->StretchRect(dst->m_texture, sRect, src->m_texture, dRect);

			//		// TODO: this is quite a lot of StretchRect, do it with one Draw
			//	}
			//}
		}
		else if (tw < 1024)
		{
			// FIXME: timesplitters blurs the render target by blending itself over a couple of times
			hack = true;
			//if (tw == 256 && th == 128 && (TEX0.TBP0 == 0 || TEX0.TBP0 == 0x00e00))
			//{
			//	delete src;
			//	return NULL;
			//}
		}
		// width/height conversion

		const float scale = is_8bits ? 1.0f : dst->m_scale;
		src->m_scale = scale;

		GSVector4i sRect = GSVector4i::loadh(new_size);
		int destX = 0;
		int destY = 0;

		if (half_right)
		{
			// You typically hit this code in snow engine game. Dstsize is the size of of Dx/GL RT
			// which is set to some arbitrary number. h/w are based on the input texture
			// so the only reliable way to find the real size of the target is to use the TBW value.
			const int half_width = static_cast<int>(dst->m_TEX0.TBW * (64 / 2));
			if (half_width < dst->m_unscaled_size.x)
			{
				const int copy_width = std::min(half_width, dst->m_unscaled_size.x - half_width);
				sRect.x = static_cast<int>(static_cast<float>(half_width) * dst->m_scale);
				sRect.z = std::min(static_cast<int>(static_cast<float>(half_width + copy_width) * dst->m_scale), dst->m_texture->GetWidth());
				new_size.x = sRect.width();
				src->m_unscaled_size.x = copy_width;
			}
			else
			{
				DevCon.Error("Invalid half-right copy with width %d from %dx%d texture", half_width * 2, dst->m_unscaled_size.x, dst->m_unscaled_size.y);
			}
		}
		else if (src_range && dst->m_TEX0.TBW == TEX0.TBW && !is_8bits)
		{
			// optimization for TBP == FRAME
			const GSDrawingContext* const context = g_gs_renderer->m_context;
			if (context->FRAME.Block() == TEX0.TBP0 || context->ZBUF.Block() == TEX0.TBP0)
			{
				// For the TS2 case above, src_range is going to be incorrect, since TW/TH are incorrect.
				// We can remove this check once we move it to tex-is-fb instead.
				if (!region.IsFixedTEX0(1 << TEX0.TW, 1 << TEX0.TH))
				{
					// if it looks like a texture shuffle, we might read up to +/- 8 pixels on either side.
					GSVector4 adjusted_src_range(*src_range);
					if (GSRendererHW::GetInstance()->IsPossibleTextureShuffle(dst, TEX0))
						adjusted_src_range += GSVector4(-8.0f, 0.0f, 8.0f, 0.0f);

					// don't forget to scale the copy range
					adjusted_src_range = adjusted_src_range * GSVector4(scale).xyxy();
					sRect = sRect.rintersect(GSVector4i(adjusted_src_range));
					destX = sRect.x;
					destY = sRect.y;
				}

				// clean up immediately afterwards
				m_temporary_source = src;
			}
		}

		// Create a cleared RT if we somehow end up with an empty source rect (because the RT isn't large enough).
		const bool source_rect_empty = sRect.rempty();
		const bool use_texture = (shader == ShaderConvert::COPY && !source_rect_empty);

		// Assuming everything matches up, instead of copying the target, we can just sample it directly.
		// It's the same as doing the copy first, except we save GPU time.
		if (!half_right && // not the size change from above
			use_texture && // not reinterpreting the RT
			new_size == dst->m_texture->GetSize() && // same dimensions
			!m_temporary_source // not the shuffle case above
			)
		{
			// sample the target directly
			src->m_texture = dst->m_texture;
			src->m_scale = dst->m_scale;
			src->m_unscaled_size = dst->m_unscaled_size;
			src->m_shared_texture = true;
			src->m_target = true; // So renderer can check if a conversion is required
			src->m_from_target = &dst->m_texture; // avoid complex condition on the renderer
			src->m_from_target_TEX0 = dst->m_TEX0;
			src->m_32_bits_fmt = dst->m_32_bits_fmt;
			src->m_valid_rect = dst->m_valid;
			src->m_end_block = dst->m_end_block;

			// kill the source afterwards, since we don't want to have to track changes to the target
			m_temporary_source = src;
		}
		else
		{
			// Don't be fooled by the name. 'dst' is the old target (hence the input)
			// 'src' is the new texture cache entry (hence the output)
			GSTexture* sTex = dst->m_texture;
			GSTexture* dTex = use_texture ?
								  g_gs_device->CreateTexture(new_size.x, new_size.y, 1, GSTexture::Format::Color, true) :
								  g_gs_device->CreateRenderTarget(new_size.x, new_size.y, GSTexture::Format::Color, source_rect_empty || destX != 0 || destY != 0);
			m_source_memory_usage += dTex->GetMemUsage();
			src->m_texture = dTex;

			if (use_texture)
			{
				g_gs_device->CopyRect(sTex, dTex, sRect, destX, destY);
			}
			else if (!source_rect_empty)
			{
				if (is_8bits)
				{
					g_gs_device->ConvertToIndexedTexture(sTex, dst->m_scale, x_offset, y_offset,
						std::max<u32>(dst->m_TEX0.TBW, 1u) * 64, dst->m_TEX0.PSM, dTex,
						std::max<u32>(TEX0.TBW, 1u) * 64, TEX0.PSM);
				}
				else
				{
					const GSVector4 sRectF = GSVector4(sRect) / GSVector4(1, 1, sTex->GetWidth(), sTex->GetHeight());
					g_gs_device->StretchRect(
						sTex, sRectF, dTex, GSVector4(destX, destY, new_size.x, new_size.y), shader, false);
				}
			}
		}

		// GH: by default (m_paltex == 0) GS converts texture to the 32 bit format
		// However it is different here. We want to reuse a Render Target as a texture.
		// Because the texture is already on the GPU, CPU can't convert it.
		if (psm.pal > 0)
		{
			AttachPaletteToSource(src, psm.pal, true);
		}

		// Offset hack. Can be enabled via GS options.
		// The offset will be used in Draw().
		float modxy = 0.0f;

		if (GSConfig.UserHacks_HalfPixelOffset == 1 && hack)
		{
			modxy = g_gs_renderer->GetUpscaleMultiplier();
			switch (static_cast<int>(std::round(g_gs_renderer->GetUpscaleMultiplier())))
			{
				case 2: case 4: case 6: case 8: modxy += 0.2f; break;
				case 3: case 7:                 modxy += 0.1f; break;
				case 5:                         modxy += 0.3f; break;
				default:                        modxy  = 0.0f; break;
			}
		}

		dst->OffsetHack_modxy = modxy;
	}
	else
	{
		// maintain the clut even when paltex is on for the dump/replacement texture lookup
		bool paltex = (GSConfig.GPUPaletteConversion && psm.pal > 0) || gpu_clut;
		const u32* clut = (psm.pal > 0) ? static_cast<const u32*>(g_gs_renderer->m_mem.m_clut) : nullptr;

		// adjust texture size to fit
		tw = region.HasX() ? region.GetWidth() : tw;
		th = region.HasY() ? region.GetHeight() : th;
		src->m_region = region;
		src->m_unscaled_size = GSVector2i(tw, th);
		src->m_scale = 1.0f;

		// try the hash cache
		if ((src->m_from_hash_cache = LookupHashCache(TEX0, TEXA, paltex, clut, lod, region)) != nullptr)
		{
			src->m_texture = src->m_from_hash_cache->texture;
			if (gpu_clut)
				AttachPaletteToSource(src, gpu_clut);
			else if (psm.pal > 0)
				AttachPaletteToSource(src, psm.pal, paltex);
		}
		else if (paltex)
		{
			src->m_texture = g_gs_device->CreateTexture(tw, th, tlevels, GSTexture::Format::UNorm8);
			m_source_memory_usage += src->m_texture->GetMemUsage();
			if (gpu_clut)
				AttachPaletteToSource(src, gpu_clut);
			else
				AttachPaletteToSource(src, psm.pal, true);
		}
		else
		{
			src->m_texture = g_gs_device->CreateTexture(tw, th, tlevels, GSTexture::Format::Color);
			m_source_memory_usage += src->m_texture->GetMemUsage();
			if (gpu_clut)
				AttachPaletteToSource(src, gpu_clut);
			else if (psm.pal > 0)
				AttachPaletteToSource(src, psm.pal, false);
		}
	}

	ASSERT(src->m_texture);
	ASSERT(src->m_target == (dst != nullptr));
	ASSERT(src->m_from_target == (dst ? &dst->m_texture : nullptr));
	ASSERT(src->m_scale == ((!dst || TEX0.PSM == PSM_PSMT8) ? 1.0f : dst->m_scale));

	src->SetPages();

	m_src.Add(src, TEX0, g_gs_renderer->m_context->offset.tex);

	return src;
}

GSTextureCache::Source* GSTextureCache::CreateMergedSource(GIFRegTEX0 TEX0, GIFRegTEXA TEXA, SourceRegion region, float scale)
{
	// We *should* be able to use the TBW here as an indicator of size... except Destroy All Humans 2 sets
	// TBW to 10, and samples from 64 through 703... which means it'd be grabbing the next row at the end.
	const int tex_width = std::max<int>(64 * TEX0.TBW, region.GetMaxX());
	const int tex_height = region.HasY() ? region.GetHeight() : (1 << TEX0.TH);
	const int scaled_width = static_cast<int>(static_cast<float>(tex_width) * scale);
	const int scaled_height = static_cast<int>(static_cast<float>(tex_height) * scale);

	// Compute new end block based on size.
	const u32 end_block = GSLocalMemory::m_psm[TEX0.PSM].info.bn(tex_width - 1, tex_height - 1, TEX0.TBP0, TEX0.TBW);
	GL_PUSH("Merging targets from %x through %x", TEX0.TBP0, end_block);

	const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[TEX0.PSM];
	const int page_width = psm.pgs.x;
	const int page_height = psm.pgs.y;
	constexpr int page_blocks = 32;

	// Number of pages to increment by for each row. This may be smaller than tex_width.
	const int row_page_increment = TEX0.TBW;
	const int page_block_increment = row_page_increment * page_blocks;
	const int width_in_pages = (tex_width + (page_width - 1)) / page_width;
	const int height_in_pages = (tex_height + (page_height - 1)) / page_height;
	const int num_pages = width_in_pages * height_in_pages;
	const GSVector4i page_rect(GSVector4i(psm.pgs).zwxy());

	// Temporary texture for preloading local memory.
	const GSOffset lm_off(psm.info, TEX0.TBP0, TEX0.TBW, TEX0.PSM);
	GSTexture* lmtex = nullptr;
	GSTexture::GSMap lmtex_map;
	bool lmtex_mapped = false;

	u8* pages_done = static_cast<u8*>(alloca((num_pages + 7) / 8));
	std::memset(pages_done, 0, (num_pages + 7) / 8);

	// Queue of rectangles to copy, we try to batch as many at once as possible.
	// Multiply by 2 in case we need to preload.
	GSDevice::MultiStretchRect* copy_queue =
		static_cast<GSDevice::MultiStretchRect*>(alloca(sizeof(GSDevice::MultiStretchRect) * num_pages * 2));
	u32 copy_count = 0;

	// Page counters.
	u32 start_TBP0 = TEX0.TBP0;
	u32 current_TBP0 = start_TBP0;
	int page_x = 0;
	int page_y = 0;

	// Helper to preload a page.
	auto preload_page = [&](int dst_x, int dst_y) {
		if (!lmtex)
		{
			lmtex = g_gs_device->CreateTexture(tex_width, tex_height, 1, GSTexture::Format::Color, false);
			lmtex_mapped = lmtex->Map(lmtex_map);
		}

		const GSVector4i rect(
			dst_x, dst_y, std::min(dst_x + page_width, tex_width), std::min(dst_y + page_height, tex_height));

		if (lmtex_mapped)
		{
			psm.rtx(g_gs_renderer->m_mem, lm_off, rect, lmtex_map.bits + dst_y * lmtex_map.pitch + dst_x * sizeof(u32),
				lmtex_map.pitch, TEXA);
		}
		else
		{
			// Slow for DX11... page_width * 4 should still be 32 byte aligned for AVX.
			const int pitch = page_width * sizeof(u32);
			psm.rtx(g_gs_renderer->m_mem, lm_off, rect, s_unswizzle_buffer, pitch, TEXA);
			lmtex->Update(rect, s_unswizzle_buffer, pitch);
		}

		// Upload texture -> render target.
		const bool linear = (scale != 1.0f);
		copy_queue[copy_count++] = {GSVector4(rect) / GSVector4(lmtex->GetSize()).xyxy(),
			GSVector4(rect) * GSVector4(scale).xyxy(), lmtex, linear, 0xf};
	};

	// The idea: loop through pages that this texture covers, find targets which overlap, and copy them in.
	// For pages which don't have any targets covering, if preloading is enabled, load that page from local memory.
	// Try to batch as much as possible, ideally this shouldn't be more than a handful of draw calls.
	for (int page_num = 0; page_num < num_pages; page_num++)
	{
		const u32 this_start_block = current_TBP0;
		const u32 this_end_block = (current_TBP0 + page_blocks) - 1;
		bool found = false;

		if (pages_done[page_num / 8] & (1u << (page_num % 8)))
			goto next_page;

		GL_INS("Searching for block range %x - %x for (%u,%u)", this_start_block, this_end_block, page_x * page_width,
			page_y * page_height);

		for (auto i = m_dst[RenderTarget].begin(); i != m_dst[RenderTarget].end(); ++i)
		{
			Target* const t = *i;
			if (this_start_block >= t->m_TEX0.TBP0 && this_end_block <= t->m_end_block && t->m_TEX0.PSM == TEX0.PSM)
			{
				GL_INS("  Candidate at BP %x BW %d PSM %d", t->m_TEX0.TBP0, t->m_TEX0.TBW, t->m_TEX0.PSM);

				// Can't copy multiple pages when we're past the TBW.. only grab one page at a time then.
				GSVector4i src_rect(page_rect);
				bool copy_multiple_pages = (t->m_TEX0.TBW == TEX0.TBW && page_x < row_page_increment);
				int available_pages_x = 1;
				int available_pages_y = 1;
				if (copy_multiple_pages)
				{
					// TBW matches, we can copy multiple pages.
					available_pages_x = (row_page_increment - page_x);
					available_pages_y = (height_in_pages - page_y);
					src_rect.z = available_pages_x * page_width;
					src_rect.w = available_pages_y * page_height;
				}

				// Why do we do this? Things go really haywire when we try to get surface offsets when
				// the valid rect isn't starting at 0. In most cases, the target has been cleared anyway,
				// so we really should be setting this to 0, not the draw...
				//
				// Guitar Hero needs it, the player meshes don't necessarily touch the corner, and so
				// does Aqua Teen Hunger Force.
				//
				const GSVector4i t_rect(t->m_valid.insert64<0>(0));

				SurfaceOffset so;
				if (this_start_block > t->m_TEX0.TBP0)
				{
					SurfaceOffsetKey sok;
					sok.elems[0].bp = this_start_block;
					sok.elems[0].bw = TEX0.TBW;
					sok.elems[0].psm = TEX0.PSM;
					sok.elems[0].rect = src_rect;
					sok.elems[1].bp = t->m_TEX0.TBP0;
					sok.elems[1].bw = t->m_TEX0.TBW;
					sok.elems[1].psm = t->m_TEX0.PSM;
					sok.elems[1].rect = t_rect;
					so = ComputeSurfaceOffset(sok);
					if (!so.is_valid)
						goto next_page;
				}
				else
				{
					so.is_valid = true;
					so.b2a_offset = src_rect.rintersect(t_rect);
				}

				// Adjust to what the target actually has.
				if (copy_multiple_pages)
				{
					// Min here because we don't want to go off the end of the target.
					available_pages_x = std::min(available_pages_x, (so.b2a_offset.width() + (psm.pgs.x - 1)) / psm.pgs.x);
					available_pages_y = std::min(available_pages_y, (so.b2a_offset.height() + (psm.pgs.y - 1)) / psm.pgs.y);
				}

				// We might not even have a full page valid..
				const bool linear = (scale != t->m_scale);
				const int src_x_end = so.b2a_offset.z;
				const int src_y_end = so.b2a_offset.w;
				int src_y = so.b2a_offset.y;
				int dst_y = page_y * page_height;
				int current_copy_page = page_num;

				for (int copy_page_y = 0; copy_page_y < available_pages_y; copy_page_y++)
				{
					if (src_y >= src_y_end)
						break;

					const int wanted_height = std::min(tex_height - dst_y, page_height);
					const int copy_height = std::min(src_y_end - src_y, wanted_height);
					int src_x = so.b2a_offset.x;
					int dst_x = page_x * page_width;
					int row_page = current_copy_page;
					pxAssert(dst_y < tex_height && copy_height > 0);

					for (int copy_page_x = 0; copy_page_x < available_pages_x; copy_page_x++)
					{
						if (src_x >= src_x_end)
							break;

						if ((pages_done[row_page / 8] & (1u << (row_page % 8))) == 0)
						{
							pages_done[row_page / 8] |= (1u << (row_page % 8));

							// In case a whole page isn't valid.
							const int wanted_width = std::min(tex_width - dst_x, page_width);
							const int copy_width = std::min(src_x_end - src_x, wanted_width);
							pxAssert(dst_x < tex_width && copy_width > 0);

							// Preload any missing parts. This will  happen when the valid rect isn't page aligned.
							if (GSConfig.PreloadFrameWithGSData &&
								(copy_width < wanted_width || copy_height < wanted_height))
							{
								preload_page(dst_x, dst_y);
							}

							GL_INS("  Copy from %d,%d -> %d,%d (%dx%d)", src_x, src_y, dst_x, dst_y, copy_width, copy_height);
							copy_queue[copy_count++] = {
								(GSVector4(src_x, src_y, src_x + copy_width, src_y + copy_height) *
									GSVector4(t->m_scale).xyxy()) /
									GSVector4(t->m_texture->GetSize()).xyxy(),
								GSVector4(dst_x, dst_y, dst_x + copy_width, dst_y + copy_height) *
									GSVector4(scale).xyxy(),
								t->m_texture, linear, 0xf };
						}

						row_page++;
						src_x += page_width;
						dst_x += page_width;
					}

					current_copy_page += width_in_pages;
					src_y += page_height;
					dst_y += page_height;
				}

				found = true;
				break;
			}
		}

		if (!found)
		{
			if (GSConfig.PreloadFrameWithGSData)
			{
				pages_done[page_num / 8] |= (1u << (page_num % 8));

				GL_INS("  *** NOT FOUND, preloading from local memory");
				const int dst_x = page_x * page_width;
				const int dst_y = page_y * page_height;
				preload_page(dst_x, dst_y);
			}
			else
			{
				GL_INS("  *** NOT FOUND");
			}
		}

	next_page:
		current_TBP0 += page_blocks;
		page_x++;
		if (page_x == width_in_pages)
		{
			start_TBP0 += page_block_increment;
			current_TBP0 = start_TBP0;
			page_x = 0;
			page_y++;
		}
	}

	// If we didn't find anything, abort.
	if (copy_count == 0)
	{
		GL_INS("No sources found.");
		return nullptr;
	}

	// Actually do the drawing.
	if (lmtex_mapped)
		lmtex->Unmap();

	// Allocate our render target for drawing everything to.
	GSTexture* dtex = g_gs_device->CreateRenderTarget(scaled_width, scaled_height, GSTexture::Format::Color, true);
	m_source_memory_usage += dtex->GetMemUsage();

	// Sort rect list by the texture, we want to batch as many as possible together.
	g_gs_device->SortMultiStretchRects(copy_queue, copy_count);
	g_gs_device->DrawMultiStretchRects(copy_queue, copy_count, dtex, ShaderConvert::COPY);
	g_perfmon.Put(GSPerfMon::TextureCopies, 1);

	if (lmtex)
		g_gs_device->Recycle(lmtex);

	Source* src = new Source(TEX0, TEXA);
	src->m_texture = dtex;
	src->m_scale = scale;
	src->m_unscaled_size = GSVector2i(tex_width, tex_height);
	src->m_end_block = end_block;
	src->m_target = true;

	// Can't use the normal SetPages() here, it'll try to use TW/TH, which might be bad.
	src->m_pages = g_gs_renderer->m_context->offset.tex.pageLooperForRect(GSVector4i(0, 0, tex_width, tex_height));

	m_src.Add(src, TEX0, g_gs_renderer->m_context->offset.tex);
	return src;
}

// This really needs a better home...
extern bool FMVstarted;

GSTextureCache::HashCacheEntry* GSTextureCache::LookupHashCache(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, bool& paltex, const u32* clut, const GSVector2i* lod, SourceRegion region)
{
	// don't bother hashing if we're not dumping or replacing.
	const bool dump = GSConfig.DumpReplaceableTextures && (!FMVstarted || GSConfig.DumpTexturesWithFMVActive) &&
					  (clut ? GSConfig.DumpPaletteTextures : GSConfig.DumpDirectTextures);
	const bool replace = GSConfig.LoadTextureReplacements && GSTextureReplacements::HasAnyReplacementTextures();
	bool can_cache = CanCacheTextureSize(TEX0.TW, TEX0.TH);
	if (!dump && !replace && !can_cache)
		return nullptr;

	// need the hash either for replacing, dumping or caching.
	// if dumping/replacing is on, we compute the clut hash regardless, since replacements aren't indexed
	HashCacheKey key{ HashCacheKey::Create(TEX0, TEXA, (dump || replace || !paltex) ? clut : nullptr, lod, region) };

	// handle dumping first, this is mostly isolated.
	if (dump)
	{
		// dump base level
		GSTextureReplacements::DumpTexture(key, TEX0, TEXA, region, g_gs_renderer->m_mem, 0);

		// and the mips
		if (lod && GSConfig.DumpReplaceableMipmaps)
		{
			const int basemip = lod->x;
			const int nmips = lod->y - lod->x + 1;
			for (int mip = 1; mip < nmips; mip++)
			{
				const GIFRegTEX0 MIP_TEX0{ g_gs_renderer->GetTex0Layer(basemip + mip) };
				GSTextureReplacements::DumpTexture(key, MIP_TEX0, TEXA, region, g_gs_renderer->m_mem, mip);
			}
		}
	}

	// check with the full key
	auto it = m_hash_cache.find(key);

	// if this fails, and paltex is on, try indexed texture
	const bool needs_second_lookup = paltex && (dump || replace);
	if (needs_second_lookup && it == m_hash_cache.end())
		it = m_hash_cache.find(key.WithRemovedCLUTHash());

	// did we find either a replacement, cached/indexed texture?
	if (it != m_hash_cache.end())
	{
		// super easy, cache hit. remove paltex if it's a replacement texture.
		HashCacheEntry* entry = &it->second;
		paltex &= (entry->texture->GetFormat() == GSTexture::Format::UNorm8);
		entry->refcount++;
		return entry;
	}

	// cache miss.
	// check for a replacement texture with the full clut key
	if (replace)
	{
		bool replacement_texture_pending = false;
		GSTexture* replacement_tex = GSTextureReplacements::LookupReplacementTexture(key, lod != nullptr, &replacement_texture_pending);
		if (replacement_tex)
		{
			// found a replacement texture! insert it into the hash cache, and clear paltex (since it's not indexed)
			paltex = false;
			const HashCacheEntry entry{ replacement_tex, 1u, 0u, true };
			m_hash_cache_replacement_memory_usage += entry.texture->GetMemUsage();
			return &m_hash_cache.emplace(key, entry).first->second;
		}
		else if (
			replacement_texture_pending ||

			// With preloading + paltex; when there's multiple textures with the same vram data, but different
			// palettes, if we don't replace all of them, the first one to get loaded in will prevent any of the
			// others from getting tested for replacement. So, disable paltex for the textures when any of the
			// palette variants have replacements.
			(paltex && GSTextureReplacements::HasReplacementTextureWithOtherPalette(key)))
		{
			// We didn't have a texture immediately, but there is a replacement available (and being loaded).
			// so clear paltex, since when it gets injected back, it's not going to be indexed.
			paltex = false;

			// If the hash cache is disabled, this will be false, and we need to force it to be cached,
			// so that when the replacement comes back, there's something for it to swap with.
			can_cache = true;
		}
	}

	// Using paltex without full preloading is a disaster case here. basically, unless *all* textures are
	// replaced, any texture can get populated without the hash cache, which means it'll get partial invalidated,
	// and unless it's 100% removed, this partial texture will always take precedence over future hash cache
	// lookups, making replacements impossible.
	if (paltex && !can_cache && TEX0.TW <= MAXIMUM_TEXTURE_HASH_CACHE_SIZE && TEX0.TH <= MAXIMUM_TEXTURE_HASH_CACHE_SIZE)
	{
		// We only need to remove paltex here if we're dumping, because we need all the palette permutations.
		paltex &= !dump;

		// We need to get it into the hash cache for dumping and replacing, because of the issue above.
		can_cache = true;
	}

	// if this texture isn't cacheable, bail out now since we don't want to waste time preloading it
	if (!can_cache)
		return nullptr;

	// expand/upload texture
	const int tw = region.HasX() ? region.GetWidth() : (1 << TEX0.TW);
	const int th = region.HasY() ? region.GetHeight() : (1 << TEX0.TH);
	const int tlevels = lod ? ((GSConfig.HWMipmap != HWMipmapLevel::Full) ? -1 : (lod->y - lod->x + 1)) : 1;
	GSTexture* tex = g_gs_device->CreateTexture(tw, th, tlevels, paltex ? GSTexture::Format::UNorm8 : GSTexture::Format::Color);
	if (!tex)
	{
		// out of video memory if we hit here
		return nullptr;
	}

	// upload base level
	PreloadTexture(TEX0, TEXA, region, g_gs_renderer->m_mem, paltex, tex, 0);

	// upload mips if present
	if (lod)
	{
		const int basemip = lod->x;
		const int nmips = lod->y - lod->x + 1;
		for (int mip = 1; mip < nmips; mip++)
		{
			const GIFRegTEX0 MIP_TEX0{ g_gs_renderer->GetTex0Layer(basemip + mip) };
			PreloadTexture(MIP_TEX0, TEXA, region.AdjustForMipmap(mip), g_gs_renderer->m_mem, paltex, tex, mip);
		}
	}

	// remove the palette hash when using paltex/indexed
	if (paltex)
		key.RemoveCLUTHash();

	// insert into the cache cache, and we're done
	const HashCacheEntry entry{ tex, 1u, 0u, false };
	m_hash_cache_memory_usage += tex->GetMemUsage();
	return &m_hash_cache.emplace(key, entry).first->second;
}

GSTextureCache::Target* GSTextureCache::CreateTarget(const GIFRegTEX0& TEX0, int w, int h, float scale, int type, const bool clear)
{
	ASSERT(type == RenderTarget || type == DepthStencil);

	const int scaled_w = static_cast<int>(std::ceil(static_cast<float>(w) * scale));
	const int scaled_h = static_cast<int>(std::ceil(static_cast<float>(h) * scale));

	// TODO: This leaks if memory allocation fails. Use a unique_ptr so it gets freed, but these
	// exceptions really need to get lost.
	std::unique_ptr<Target> t = std::make_unique<Target>(TEX0, !GSConfig.UserHacks_DisableDepthSupport, type);
	t->m_unscaled_size = GSVector2i(w, h);
	t->m_scale = scale;

	if (type == RenderTarget)
	{
		t->m_texture = g_gs_device->CreateRenderTarget(scaled_w, scaled_h, GSTexture::Format::Color, clear);

		t->m_used = true; // FIXME
	}
	else if (type == DepthStencil)
	{
		t->m_texture = g_gs_device->CreateDepthStencil(scaled_w, scaled_h, GSTexture::Format::DepthStencil, clear);
	}

	m_target_memory_usage += t->m_texture->GetMemUsage();

	m_dst[type].push_front(t.get());

	return t.release();
}

GSTexture* GSTextureCache::LookupPaletteSource(u32 CBP, u32 CPSM, u32 CBW, GSVector2i& offset, float* scale, const GSVector2i& size)
{
	for (auto t : m_dst[RenderTarget])
	{
		if (!t->m_used)
			continue;

		GSVector2i this_offset;
		if (t->m_TEX0.TBP0 == CBP)
		{
			// Exact match, this one's likely fine, unless the format is different.
			if (t->m_TEX0.PSM != CPSM || (CBW != 0 && t->m_TEX0.TBW != CBW))
				continue;

			GL_INS("Exact match on BP 0x%04x BW %u", t->m_TEX0.TBP0, t->m_TEX0.TBW);
			this_offset.x = 0;
			this_offset.y = 0;
		}
		else if (GSConfig.UserHacks_GPUTargetCLUTMode == GSGPUTargetCLUTMode::InsideTarget &&
				 t->m_TEX0.TBP0 < CBP && t->m_end_block >= CBP)
		{
			// Somewhere within this target, can we find it?
			const GSVector4i rc(0, 0, size.x, size.y);
			SurfaceOffset so = ComputeSurfaceOffset(CBP, std::max<u32>(CBW, 0), CPSM, rc, t);
			if (!so.is_valid)
				continue;

			GL_INS("Match inside RT at BP 0x%04X-0x%04X BW %u", t->m_TEX0.TBP0, t->m_end_block, t->m_TEX0.TBW);
			this_offset.x = so.b2a_offset.left;
			this_offset.y = so.b2a_offset.top;
		}
		else
		{
			// Not inside this target, skip.
			continue;
		}

		// Make sure the clut isn't in an area of the target where the EE has overwritten it.
		// Otherwise, we'll be using stale data on the CPU.
		if (!t->m_dirty.empty())
		{
			GL_INS("Candidate is dirty, checking");

			const GSVector4i clut_rc(this_offset.x, this_offset.y, this_offset.x + size.x, this_offset.y + size.y);
			bool is_dirty = false;
			for (GSDirtyRect& dirty : t->m_dirty)
			{
				if (!dirty.GetDirtyRect(t->m_TEX0).rintersect(clut_rc).rempty())
				{
					GL_INS("Dirty rectangle overlaps CLUT rectangle, skipping");
					is_dirty = true;
					break;
				}
			}
			if (is_dirty)
				continue;
		}

		offset = this_offset;
		*scale = t->m_scale;
		return t->m_texture;
	}

	return nullptr;
}

void GSTextureCache::Read(Target* t, const GSVector4i& r)
{
	if (!t->m_dirty.empty() || r.width() == 0 || r.height() == 0)
		return;

	const GIFRegTEX0& TEX0 = t->m_TEX0;

	GSTexture::Format fmt;
	ShaderConvert ps_shader;
	std::unique_ptr<GSDownloadTexture>* dltex;
	switch (TEX0.PSM)
	{
		case PSM_PSMCT32:
		case PSM_PSMCT24:
			fmt = GSTexture::Format::Color;
			ps_shader = ShaderConvert::COPY;
			dltex = &m_color_download_texture;
			break;

		case PSM_PSMCT16:
		case PSM_PSMCT16S:
			fmt = GSTexture::Format::UInt16;
			ps_shader = ShaderConvert::RGBA8_TO_16_BITS;
			dltex = &m_uint16_download_texture;
			break;

		case PSM_PSMZ32:
		case PSM_PSMZ24:
			fmt = GSTexture::Format::UInt32;
			ps_shader = ShaderConvert::FLOAT32_TO_32_BITS;
			dltex = &m_uint32_download_texture;
			break;

		case PSM_PSMZ16:
		case PSM_PSMZ16S:
			fmt = GSTexture::Format::UInt16;
			ps_shader = ShaderConvert::FLOAT32_TO_16_BITS;
			dltex = &m_uint16_download_texture;
			break;

		default:
			return;
	}

	// Don't overwrite bits which aren't used in the target's format.
	// Stops Burnout 3's sky from breaking when flushing targets to local memory.
	const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[TEX0.PSM];
	const u32 write_mask = t->m_valid_bits & psm.fmsk;
	if (psm.bpp > 16 && write_mask == 0)
	{
		DbgCon.Warning("Not reading back target %x PSM %s due to no write mask", TEX0.TBP0, psm_str(TEX0.PSM));
		return;
	}

	// Yes lots of logging, but I'm not confident with this code
	GL_PUSH("Texture Cache Read. Format(0x%x)", TEX0.PSM);

	GL_PERF("TC: Read Back Target: %d (0x%x)[fmt: 0x%x]. Size %dx%d", t->m_texture->GetID(), TEX0.TBP0, TEX0.PSM, r.width(), r.height());

	const GSVector4 src(GSVector4(r) * GSVector4(t->m_scale) / GSVector4(t->m_texture->GetSize()).xyxy());
	const GSVector4i drc(0, 0, r.width(), r.height());
	const bool direct_read = (t->m_scale == 1.0f && ps_shader == ShaderConvert::COPY);

	if (!PrepareDownloadTexture(drc.z, drc.w, fmt, dltex))
		return;

	if (direct_read)
	{
		dltex->get()->CopyFromTexture(drc, t->m_texture, r, 0, true);
	}
	else
	{
		GSTexture* tmp = g_gs_device->CreateRenderTarget(drc.z, drc.w, fmt, false);
		if (tmp)
		{
			g_gs_device->StretchRect(t->m_texture, src, tmp, GSVector4(drc), ps_shader, false);
			dltex->get()->CopyFromTexture(drc, tmp, drc, 0, true);
			g_gs_device->Recycle(tmp);
		}
		else
		{
			Console.Error("Failed to allocate temporary %dx%d target for read.", drc.z, drc.w);
			return;
		}
	}

	dltex->get()->Flush();
	if (!dltex->get()->Map(drc))
		return;

	// Why does WritePixelNN() not take a const pointer?
	const GSOffset off = g_gs_renderer->m_mem.GetOffset(TEX0.TBP0, TEX0.TBW, TEX0.PSM);
	u8* bits = const_cast<u8*>(dltex->get()->GetMapPointer());
	const u32 pitch = dltex->get()->GetMapPitch();

	switch (TEX0.PSM)
	{
		case PSM_PSMCT32:
		case PSM_PSMZ32:
		case PSM_PSMCT24:
		case PSM_PSMZ24:
			g_gs_renderer->m_mem.WritePixel32(bits, pitch, off, r, write_mask);
			break;
		case PSM_PSMCT16:
		case PSM_PSMCT16S:
		case PSM_PSMZ16:
		case PSM_PSMZ16S:
			g_gs_renderer->m_mem.WritePixel16(bits, pitch, off, r);
			break;

		default:
			Console.Error("Unknown PSM %u on Read", TEX0.PSM);
			break;
	}

	dltex->get()->Unmap();
}

void GSTextureCache::Read(Source* t, const GSVector4i& r)
{
	if (r.rempty())
		return;

	const GSVector4i drc(0, 0, r.width(), r.height());

	if (!PrepareDownloadTexture(drc.z, drc.w, GSTexture::Format::Color, &m_color_download_texture))
		return;

	m_color_download_texture->CopyFromTexture(drc, t->m_texture, r, 0, true);
	m_color_download_texture->Flush();

	if (m_color_download_texture->Map(drc))
	{
		const GSOffset off = g_gs_renderer->m_mem.GetOffset(t->m_TEX0.TBP0, t->m_TEX0.TBW, t->m_TEX0.PSM);
		g_gs_renderer->m_mem.WritePixel32(
			const_cast<u8*>(m_color_download_texture->GetMapPointer()), m_color_download_texture->GetMapPitch(), off, r);
		m_color_download_texture->Unmap();
	}
}

// GSTextureCache::Surface

GSTextureCache::Surface::Surface() = default;

GSTextureCache::Surface::~Surface() = default;

void GSTextureCache::Surface::UpdateAge()
{
	m_age = 0;
}

bool GSTextureCache::Surface::Inside(u32 bp, u32 bw, u32 psm, const GSVector4i& rect)
{
	// Valid only for color formats.
	const GSOffset off(GSLocalMemory::m_psm[psm].info, bp, bw, psm);
	const u32 end_block = off.bnNoWrap(rect.z - 1, rect.w - 1);
	return bp >= m_TEX0.TBP0 && end_block <= UnwrappedEndBlock();
}

bool GSTextureCache::Surface::Overlaps(u32 bp, u32 bw, u32 psm, const GSVector4i& rect)
{
	// Valid only for color formats.
	const GSOffset off(GSLocalMemory::m_psm[psm].info, bp, bw, psm);
	u32 end_block = off.bnNoWrap(rect.z - 1, rect.w - 1);
	u32 start_block = off.bnNoWrap(rect.x, rect.y);
	// Due to block ordering, end can be below start in a page, so if it's within a page, swap them.
	if (end_block < start_block && ((start_block - end_block) < (1 << 5)))
	{
		std::swap(start_block, end_block);
	}
	const bool overlap = GSTextureCache::CheckOverlap(m_TEX0.TBP0, UnwrappedEndBlock(), start_block, end_block);
	return overlap;
}

// GSTextureCache::Source

GSTextureCache::Source::Source(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA)
{
	m_TEX0 = TEX0;
	m_TEXA = TEXA;
}

GSTextureCache::Source::~Source()
{
	_aligned_free(m_write.rect);

	// Shared textures are pointers copy. Therefore no allocation
	// to recycle.
	if (!m_shared_texture && !m_from_hash_cache && m_texture)
	{
		GSRendererHW::GetInstance()->GetTextureCache()->m_source_memory_usage -= m_texture->GetMemUsage();
		g_gs_device->Recycle(m_texture);
	}
}

void GSTextureCache::Source::SetPages()
{
	const int tw = 1 << m_TEX0.TW;
	const int th = 1 << m_TEX0.TH;

	m_repeating = !m_from_hash_cache && m_TEX0.IsRepeating() && !m_region.IsFixedTEX0(tw, th);

	if (m_repeating && !m_target && !CanPreload())
	{
		// TODO: wrong for lupin/invalid tex0
		m_p2t = g_gs_renderer->m_mem.GetPage2TileMap(m_TEX0);
	}

	const GSVector4i rect(m_region.GetRect(tw, th));
	m_pages = g_gs_renderer->m_context->offset.tex.pageLooperForRect(rect);
}

void GSTextureCache::Source::Update(const GSVector4i& rect, int level)
{
	Surface::UpdateAge();

	if (m_target || m_from_hash_cache || (m_complete_layers & (1u << level)))
		return;

	if (CanPreload())
	{
		PreloadLevel(level);
		return;
	}

	const GSVector2i& bs = GSLocalMemory::m_psm[m_TEX0.PSM].bs;
	const int tw = 1 << m_TEX0.TW;
	const int th = 1 << m_TEX0.TH;

	GSVector4i r(rect);
	const GSVector4i region_rect(m_region.GetRect(tw, th));

	// Offset the pages we use by the clamp region.
	if (m_region.HasEither())
		r = (r + m_region.GetOffset(tw, th)).rintersect(region_rect);

	r = r.ralign<Align_Outside>(bs);

	if (region_rect.eq(m_region.HasEither() ? r.rintersect(region_rect) : r))
		m_complete_layers |= (1u << level);

	const GSOffset& off = g_gs_renderer->m_context->offset.tex;
	GSOffset::BNHelper bn = off.bnMulti(r.left, r.top);

	u32 blocks = 0;

	if (!m_valid)
		m_valid = std::make_unique<u32[]>(MAX_PAGES);

	if (m_repeating)
	{
		for (int y = r.top; y < r.bottom; y += bs.y, bn.nextBlockY())
		{
			for (int x = r.left; x < r.right; bn.nextBlockX(), x += bs.x)
			{
				const int i = (bn.blkY() << 7) + bn.blkX();
				const u32 addr = i % MAX_BLOCKS;

				const u32 row = addr >> 5u;
				const u32 col = 1 << (addr & 31u);

				if ((m_valid[row] & col) == 0)
				{
					m_valid[row] |= col;

					Write(GSVector4i(x, y, x + bs.x, y + bs.y), level);

					blocks++;
				}
			}
		}
	}
	else
	{
		for (int y = r.top; y < r.bottom; y += bs.y, bn.nextBlockY())
		{
			for (int x = r.left; x < r.right; x += bs.x, bn.nextBlockX())
			{
				const u32 block = bn.value();
				const u32 row = block >> 5u;
				const u32 col = 1 << (block & 31u);

				if ((m_valid[row] & col) == 0)
				{
					m_valid[row] |= col;

					Write(GSVector4i(x, y, x + bs.x, y + bs.y), level);

					blocks++;
				}
			}
		}
	}

	if (blocks > 0)
	{
		g_perfmon.Put(GSPerfMon::Unswizzle, bs.x * bs.y * blocks << (m_palette ? 2 : 0));
		Flush(m_write.count, level);
	}
}

void GSTextureCache::Source::UpdateLayer(const GIFRegTEX0& TEX0, const GSVector4i& rect, int layer)
{
	if (layer > 6)
		return;

	if (m_target) // Yeah keep dreaming
		return;

	if (TEX0 == m_layer_TEX0[layer])
		return;

	const GIFRegTEX0 old_TEX0 = m_TEX0;

	m_layer_TEX0[layer] = TEX0;
	m_TEX0 = TEX0;

	Update(rect, layer);

	m_TEX0 = old_TEX0;
}

void GSTextureCache::Source::Write(const GSVector4i& r, int layer)
{
	if (!m_write.rect)
		m_write.rect = static_cast<GSVector4i*>(_aligned_malloc(3 * sizeof(GSVector4i), 32));

	m_write.rect[m_write.count++] = r;

	while (m_write.count >= 2)
	{
		GSVector4i& a = m_write.rect[m_write.count - 2];
		GSVector4i& b = m_write.rect[m_write.count - 1];

		if ((a == b.zyxw()).mask() == 0xfff0)
		{
			a.right = b.right; // extend right

			m_write.count--;
		}
		else if ((a == b.xwzy()).mask() == 0xff0f)
		{
			a.bottom = b.bottom; // extend down

			m_write.count--;
		}
		else
		{
			break;
		}
	}

	if (m_write.count > 2)
	{
		Flush(1, layer);
	}
}

void GSTextureCache::Source::Flush(u32 count, int layer)
{
	// This function as written will not work for paletted formats copied from framebuffers
	// because they are 8 or 4 bit formats on the GS and the GS local memory module reads
	// these into an 8 bit format while the D3D surfaces are 32 bit.
	// However the function is never called for these cases.  This is just for information
	// should someone wish to use this function for these cases later.
	const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[m_TEX0.PSM];
	const SourceRegion region((layer == 0) ? m_region : m_region.AdjustForMipmap(layer));

	// For the invalid tex0 case, the region might be larger than TEX0.TW/TH.
	const int tw = std::max(region.GetWidth(), 1u << m_TEX0.TW);
	const int th = std::max(region.GetHeight(), 1u << m_TEX0.TH);
	const GSVector4i tex_r(region.GetRect(tw, th));

	int pitch = std::max(tw, psm.bs.x) * sizeof(u32);

	GSLocalMemory& mem = g_gs_renderer->m_mem;

	const GSOffset& off = g_gs_renderer->m_context->offset.tex;

	GSLocalMemory::readTexture rtx = psm.rtx;

	if (m_palette)
	{
		pitch >>= 2;
		rtx = psm.rtxP;
	}

	for (u32 i = 0; i < count; i++)
	{
		const GSVector4i r(m_write.rect[i]);

		// if update rect lies to the left/above of the region rectangle, or extends past the texture bounds, we can't use a direct map
		if (((r > tex_r).mask() & 0xff00) == 0 && ((tex_r > r).mask() & 0x00ff) == 0)
		{
			GSTexture::GSMap m;
			const GSVector4i map_r(r - tex_r.xyxy());
			if (m_texture->Map(m, &map_r, layer))
			{
				rtx(mem, off, r, m.bits, m.pitch, m_TEXA);
				m_texture->Unmap();
				continue;
			}
		}

		const GSVector4i rint(r.rintersect(tex_r));
		if (rint.width() == 0 || rint.height() == 0)
			continue;

		rtx(mem, off, r, s_unswizzle_buffer, pitch, m_TEXA);

		// need to offset if we're a region texture
		const u8* src = s_unswizzle_buffer + (pitch * static_cast<u32>(std::max(tex_r.top - r.top, 0))) +
						(static_cast<u32>(std::max(tex_r.left - r.left, 0)) << (m_palette ? 0 : 2));
		m_texture->Update(rint - tex_r.xyxy(), src, pitch, layer);
	}

	if (count < m_write.count)
	{
		// Warning src and destination overlap. Memmove must be used instead of memcpy
		memmove(&m_write.rect[0], &m_write.rect[count], (m_write.count - count) * sizeof(m_write.rect[0]));
	}

	m_write.count -= count;
}

void GSTextureCache::Source::PreloadLevel(int level)
{
	// m_TEX0 is adjusted for mips (messy, should be changed).
	const HashType hash = HashTexture(m_TEX0, m_TEXA, m_region);

	// Layer is complete again, regardless of whether the hash matches or not (and we reupload).
	const u8 layer_bit = static_cast<u8>(1) << level;
	m_complete_layers |= layer_bit;

	// Check whether the hash matches. Black textures will be 0, so check the valid bit.
	if ((m_valid_hashes & layer_bit) && m_layer_hash[level] == hash)
		return;

	m_valid_hashes |= layer_bit;
	m_layer_hash[level] = hash;

	// And upload the texture.
	PreloadTexture(m_TEX0, m_TEXA, m_region.AdjustForMipmap(level), g_gs_renderer->m_mem, m_palette != nullptr, m_texture, level);
}

bool GSTextureCache::Source::ClutMatch(const PaletteKey& palette_key)
{
	return PaletteKeyEqual()(palette_key, m_palette_obj->GetPaletteKey());
}

// GSTextureCache::Target

GSTextureCache::Target::Target(const GIFRegTEX0& TEX0, const bool depth_supported, const int type)
	: m_type(type)
	, m_depth_supported(depth_supported)
	, m_used(false)
	, m_valid(GSVector4i::zero())
{
	m_TEX0 = TEX0;
	m_32_bits_fmt |= (GSLocalMemory::m_psm[TEX0.PSM].trbpp != 16);
	m_dirty_alpha = GSLocalMemory::m_psm[TEX0.PSM].trbpp != 24;
}

GSTextureCache::Target::~Target()
{
	// Targets should never be shared.
	pxAssert(!m_shared_texture);
	if (m_texture)
	{
		GSRendererHW::GetInstance()->GetTextureCache()->m_target_memory_usage -= m_texture->GetMemUsage();
		g_gs_device->Recycle(m_texture);
	}
}

void GSTextureCache::Target::Update(bool reset_age)
{
	if (reset_age)
		Surface::UpdateAge();

	// FIXME: the union of the rects may also update wrong parts of the render target (but a lot faster :)
	// GH: it must be doable
	// 1/ rescale the new t to the good size
	// 2/ copy each rectangle (rescale the rectangle) (use CopyRect or multiple vertex)
	// Alternate
	// 1/ uses multiple vertex rectangle

	if (m_dirty.empty())
		return;

	// No handling please
	if ((m_type == DepthStencil) && !m_depth_supported)
	{
		// do the most likely thing a direct write would do, clear it
		GL_INS("ERROR: Update DepthStencil dummy");
		m_dirty.clear();
		return;
	}

	const GSVector4i total_rect = m_dirty.GetTotalRect(m_TEX0, m_unscaled_size);
	if (total_rect.rempty())
	{
		GL_INS("ERROR: Nothing to update?");
		m_dirty.clear();
		return;
	}

	const GSVector4i t_offset(total_rect.xyxy());
	const GSVector4i t_size(total_rect - t_offset);
	const GSVector4 t_sizef(t_size.zwzw());

	// This'll leave undefined data in pixels that we're not reading from... shouldn't hurt anything.
	GSTexture* const t = g_gs_device->CreateTexture(t_size.z, t_size.w, 1, GSTexture::Format::Color);
	GSTexture::GSMap m;
	const bool mapped = t->Map(m);

	GIFRegTEXA TEXA = {};
	TEXA.AEM = 1;
	TEXA.TA0 = 0;
	TEXA.TA1 = 0x80;

	// Bilinear filtering this is probably not a good thing, at least in native, but upscaling Nearest can be gross and messy.
	// It's needed for depth, though.. filtering depth doesn't make much sense, but SMT3 needs it..
	const bool upscaled = (m_scale != 1.0f);
	const bool override_linear = upscaled && GSConfig.UserHacks_BilinearHack;
	const bool linear = (m_type == RenderTarget && upscaled);

	GSDevice::MultiStretchRect* drects = static_cast<GSDevice::MultiStretchRect*>(
		alloca(sizeof(GSDevice::MultiStretchRect) * static_cast<u32>(m_dirty.size())));
	u32 ndrects = 0;

	const GSOffset off(g_gs_renderer->m_mem.GetOffset(m_TEX0.TBP0, m_TEX0.TBW, m_TEX0.PSM));
	for (size_t i = 0; i < m_dirty.size(); i++)
	{
		const GSVector4i r(m_dirty.GetDirtyRect(i, m_TEX0, total_rect));
		if (r.rempty())
			continue;

		const GSVector4i t_r(r - t_offset);
		if (mapped)
		{
			g_gs_renderer->m_mem.ReadTexture(
				off, r, m.bits + t_r.y * static_cast<u32>(m.pitch) + (t_r.x * sizeof(u32)), m.pitch, TEXA);
		}
		else
		{
			const int pitch = Common::AlignUpPow2(r.width() * sizeof(u32), 32);
			g_gs_renderer->m_mem.ReadTexture(off, r, s_unswizzle_buffer, pitch, TEXA);

			t->Update(t_r, s_unswizzle_buffer, pitch);
		}

		GSDevice::MultiStretchRect& drect = drects[ndrects++];
		drect.src = t;
		drect.src_rect = GSVector4(r - t_offset) / t_sizef;
		drect.dst_rect = GSVector4(r) * GSVector4(m_scale);
		drect.linear = linear && (m_dirty[i].req_linear || override_linear);

		// Copy the new GS memory content into the destination texture.
		if (m_type == RenderTarget)
		{
			GL_INS("ERROR: Update RenderTarget 0x%x bw:%d (%d,%d => %d,%d)", m_TEX0.TBP0, m_TEX0.TBW, r.x, r.y, r.z, r.w);
			drect.wmask = static_cast<u8>(m_dirty[i].rgba._u32);
		}
		else if (m_type == DepthStencil)
		{
			GL_INS("ERROR: Update DepthStencil 0x%x", m_TEX0.TBP0);
			drect.wmask = 0xF;
		}
	}

	if (mapped)
		t->Unmap();

	if (ndrects > 0)
	{
		// No need to sort here, it's all the one texture.
		const ShaderConvert shader = (m_type == RenderTarget) ? ShaderConvert::COPY :
																(upscaled ? ShaderConvert::RGBA8_TO_FLOAT32_BILN :
																			ShaderConvert::RGBA8_TO_FLOAT32);
		g_gs_device->DrawMultiStretchRects(drects, ndrects, m_texture, shader);
	}

	UpdateValidity(total_rect);

	g_gs_device->Recycle(t);
	m_dirty.clear();
}

void GSTextureCache::Target::UpdateIfDirtyIntersects(const GSVector4i& rc)
{
	for (auto& dirty : m_dirty)
	{
		const GSVector4i dirty_rc(dirty.GetDirtyRect(m_TEX0));
		if (dirty_rc.rintersect(rc).rempty())
			continue;

		// strictly speaking, we only need to update the area outside of the move.
		// but, to keep things simple, just update the whole thing
		GL_CACHE("TC: Update dirty rectangle [%d,%d,%d,%d] due to intersection with [%d,%d,%d,%d]",
			dirty_rc.x, dirty_rc.y, dirty_rc.z, dirty_rc.w, rc.x, rc.y, rc.z, rc.w);
		Update(true);
		break;
	}
}
void GSTextureCache::Target::ResizeDrawn(const GSVector4i& rect)
{
	m_drawn_since_read = m_drawn_since_read.rintersect(rect);
}


void GSTextureCache::Target::UpdateDrawn(const GSVector4i& rect, bool can_resize)
{
	if (can_resize)
	{
		if(m_drawn_since_read.rempty())
			m_drawn_since_read = rect.rintersect(m_valid);
		else
			m_drawn_since_read = m_drawn_since_read.runion(rect);
	}
}

void GSTextureCache::Target::ResizeValidity(const GSVector4i& rect)
{
	if (!m_valid.eq(GSVector4i::zero()))
	{
		m_valid = m_valid.rintersect(rect);
		m_drawn_since_read = m_drawn_since_read.rintersect(rect);
		m_end_block = GSLocalMemory::m_psm[m_TEX0.PSM].info.bn(m_valid.z - 1, m_valid.w - 1, m_TEX0.TBP0, m_TEX0.TBW); // Valid only for color formats
		// Because m_end_block, especially on Z is not remotely linear, the end of the block can be near the beginning,
		// meaning any overlap checks on blocks could fail (FFX with Tex in RT).
		// So if the coordinates page align, round it up to the next page and minus one.
		const GSVector2i page_size = GSLocalMemory::m_psm[m_TEX0.PSM].pgs;
		if ((m_valid.z & (page_size.x - 1)) == 0 && (m_valid.w & (page_size.y - 1)) == 0)
		{
			constexpr u32 page_mask = (1 << 5) - 1;
			m_end_block = (((m_end_block + page_mask) & ~page_mask)) - 1;
		}
	}
	else
	{
		// No valid size, so need to resize down.
		return;
	}

	// GL_CACHE("ResizeValidity (0x%x->0x%x) from R:%d,%d Valid: %d,%d", m_TEX0.TBP0, m_end_block, rect.z, rect.w, m_valid.z, m_valid.w);
}

void GSTextureCache::Target::UpdateValidity(const GSVector4i& rect, bool can_resize)
{
	if (can_resize)
	{
		if (m_valid.eq(GSVector4i::zero()))
			m_valid = rect;
		else
			m_valid = m_valid.runion(rect);

		m_end_block = GSLocalMemory::m_psm[m_TEX0.PSM].info.bn(m_valid.z - 1, m_valid.w - 1, m_TEX0.TBP0, m_TEX0.TBW); // Valid only for color formats
		// Because m_end_block, especially on Z is not remotely linear, the end of the block can be near the beginning,
		// meaning any overlap checks on blocks could fail (FFX with Tex in RT).
		// So if the coordinates page align, round it up to the next page and minus one.
		const GSVector2i page_size = GSLocalMemory::m_psm[m_TEX0.PSM].pgs;
		if ((m_valid.z & (page_size.x - 1)) == 0 && (m_valid.w & (page_size.y - 1)) == 0)
		{
			constexpr u32 page_mask = (1 << 5) - 1;
			m_end_block = (((m_end_block + page_mask) & ~page_mask)) - 1;
		}
	}
	// GL_CACHE("UpdateValidity (0x%x->0x%x) from R:%d,%d Valid: %d,%d", m_TEX0.TBP0, m_end_block, rect.z, rect.w, m_valid.z, m_valid.w);
}

void GSTextureCache::Target::UpdateValidBits(u32 bits_written)
{
	m_valid_bits |= bits_written;
}

bool GSTextureCache::Target::ResizeTexture(int new_unscaled_width, int new_unscaled_height, bool recycle_old)
{
	if (m_unscaled_size.x == new_unscaled_width && m_unscaled_size.y == new_unscaled_height)
		return true;

	const int width = m_texture->GetWidth();
	const int height = m_texture->GetHeight();
	const int new_width = static_cast<int>(std::ceil(new_unscaled_width) * m_scale);
	const int new_height = static_cast<int>(std::ceil(new_unscaled_height) * m_scale);
	const bool clear = (new_width > width || new_height > height);

	// These exceptions *really* need to get lost. This gets called outside of draws, which just crashes
	// when it tries to propagate the exception back.
	GSTexture* tex = nullptr;
	try
	{
		tex = m_texture->IsDepthStencil() ?
				  g_gs_device->CreateDepthStencil(new_width, new_height, m_texture->GetFormat(), clear) :
				  g_gs_device->CreateRenderTarget(new_width, new_height, m_texture->GetFormat(), clear);
	}
	catch (const std::bad_alloc&)
	{
	}

	if (!tex)
	{
		Console.Error("(ResizeTexture) Failed to allocate %dx%d texture from %dx%d texture", new_width, new_height, width, height);
		return false;
	}

	const GSVector4i rc(0, 0, std::min(width, new_width), std::min(height, new_height));
	if (tex->IsDepthStencil())
	{
		// Can't do partial copies in DirectX for depth textures, and it's probably not ideal in other
		// APIs either. So use a fullscreen quad setting depth instead.
		g_gs_device->StretchRect(m_texture, tex, GSVector4(rc), ShaderConvert::DEPTH_COPY, false);
	}
	else
	{
		// Fast memcpy()-like path for color targets.
		g_gs_device->CopyRect(m_texture, tex, rc, 0, 0);
	}

	GSTextureCache* tc = GSRendererHW::GetInstance()->GetTextureCache();
	tc->m_target_memory_usage = (tc->m_target_memory_usage - m_texture->GetMemUsage()) + tex->GetMemUsage();

	if (recycle_old)
		g_gs_device->Recycle(m_texture);
	else
		delete m_texture;

	m_texture = tex;
	m_unscaled_size.x = new_unscaled_width;
	m_unscaled_size.y = new_unscaled_height;

	return true;
}

// GSTextureCache::SourceMap

void GSTextureCache::SourceMap::Add(Source* s, const GIFRegTEX0& TEX0, const GSOffset& off)
{
	m_surfaces.insert(s);

	// The source pointer will be stored/duplicated in all m_map[array of pages]
	s->m_pages.loopPages([this, s](u32 page)
	{
		s->m_erase_it[page] = m_map[page].InsertFront(s);
	});
}

void GSTextureCache::SourceMap::RemoveAll()
{
	for (auto s : m_surfaces)
	{
		if (s->m_from_hash_cache)
		{
			pxAssert(s->m_from_hash_cache->refcount > 0);
			if ((--s->m_from_hash_cache->refcount) == 0)
				s->m_from_hash_cache->age = 0;
		}

		delete s;
	}

	m_surfaces.clear();

	for (FastList<Source*>& item : m_map)
	{
		item.clear();
	}
}

void GSTextureCache::SourceMap::RemoveAt(Source* s)
{
	m_surfaces.erase(s);

	GL_CACHE("TC: Remove Src Texture: %d (0x%x)",
		s->m_texture ? s->m_texture->GetID() : 0,
		s->m_TEX0.TBP0);

	s->m_pages.loopPages([this, s](u32 page)
	{
		m_map[page].EraseIndex(s->m_erase_it[page]);
	});

	if (s->m_from_hash_cache)
	{
		pxAssert(s->m_from_hash_cache->refcount > 0);
		if ((--s->m_from_hash_cache->refcount) == 0)
			s->m_from_hash_cache->age = 0;
	}

	delete s;
}

void GSTextureCache::AttachPaletteToSource(Source* s, u16 pal, bool need_gs_texture)
{
	s->m_palette_obj = m_palette_map.LookupPalette(pal, need_gs_texture);
	s->m_palette = need_gs_texture ? s->m_palette_obj->GetPaletteGSTexture() : nullptr;
}

void GSTextureCache::AttachPaletteToSource(Source* s, GSTexture* gpu_clut)
{
	s->m_palette_obj = nullptr;
	s->m_palette = gpu_clut;
}

GSTextureCache::SurfaceOffset GSTextureCache::ComputeSurfaceOffset(const GSOffset& off, const GSVector4i& r, const Target* t)
{
	// Computes offset from Target to offset+rectangle in Target coords.
	if (!t)
		return {false};
	const SurfaceOffset so = ComputeSurfaceOffset(off.bp(), off.bw(), off.psm(), r, t);
	return so;
}

GSTextureCache::SurfaceOffset GSTextureCache::ComputeSurfaceOffset(const uint32_t bp, const uint32_t bw, const uint32_t psm, const GSVector4i& r, const Target* t)
{
	// Computes offset from Target to bp+bw+psm+r in Target coords.
	if (!t)
		return {false};
	SurfaceOffsetKey sok;
	sok.elems[0].bp = bp;
	sok.elems[0].bw = bw;
	sok.elems[0].psm = psm;
	sok.elems[0].rect = r;
	sok.elems[1].bp = t->m_TEX0.TBP0;
	sok.elems[1].bw = t->m_TEX0.TBW;
	sok.elems[1].psm = t->m_TEX0.PSM;
	sok.elems[1].rect = t->m_valid;
	const SurfaceOffset so = ComputeSurfaceOffset(sok);
	// Check if any dirty rect in the target overlaps with the offset.
	if (so.is_valid && !t->m_dirty.empty())
	{
		const SurfaceOffsetKeyElem& t_sok = sok.elems[1];
		const GSLocalMemory::psm_t& t_psm_s = GSLocalMemory::m_psm[t_sok.psm];
		const u32 so_bp = t_psm_s.info.bn(so.b2a_offset.x, so.b2a_offset.y, t_sok.bp, t_sok.bw);
		const u32 so_bp_end = t_psm_s.info.bn(so.b2a_offset.z - 1, so.b2a_offset.w - 1, t_sok.bp, t_sok.bw);
		for (const auto& dr : t->m_dirty)
		{
			const GSLocalMemory::psm_t& dr_psm_s = GSLocalMemory::m_psm[dr.psm];
			const u32 dr_bp = dr_psm_s.info.bn(dr.r.x, dr.r.y, t_sok.bp, dr.bw);
			const u32 dr_bp_end = dr_psm_s.info.bn(dr.r.z - 1, dr.r.w - 1, t_sok.bp, dr.bw);
			const bool overlap = GSTextureCache::CheckOverlap(dr_bp, dr_bp_end, so_bp, so_bp_end);
			if (overlap)
			{
				// Dirty rectangle in target overlaps with the found offset.
				return {false};
			}
		}
	}
	return so;
}

GSTextureCache::SurfaceOffset GSTextureCache::ComputeSurfaceOffset(const SurfaceOffsetKey& sok)
{
	const SurfaceOffsetKeyElem& a_el = sok.elems[0];
	const SurfaceOffsetKeyElem& b_el = sok.elems[1];
	const GSLocalMemory::psm_t& a_psm_s = GSLocalMemory::m_psm[a_el.psm];
	const GSLocalMemory::psm_t& b_psm_s = GSLocalMemory::m_psm[b_el.psm];
	const GSVector4i a_rect = a_el.rect.ralign<Align_Outside>(a_psm_s.bs);
	const GSVector4i b_rect = b_el.rect.ralign<Align_Outside>(b_psm_s.bs);
	if (a_rect.width() <= 0 || a_rect.height() <= 0 || a_rect.x < 0 || a_rect.y < 0)
		return {false}; // Invalid A rectangle.
	if (b_rect.width() <= 0 || b_rect.height() <= 0 || b_rect.x < 0 || b_rect.y < 0)
		return {false}; // Invalid B rectangle.
	const u32 a_bp_end = a_psm_s.info.bn(a_rect.z - 1, a_rect.w - 1, a_el.bp, a_el.bw);
	const u32 b_bp_end = b_psm_s.info.bn(b_rect.z - 1, b_rect.w - 1, b_el.bp, b_el.bw);
	const bool overlap = GSTextureCache::CheckOverlap(a_el.bp, a_bp_end, b_el.bp, b_bp_end);
	if (!overlap)
		return {false}; // A and B do not overlap.

	// Key parameter is valid.
	const auto it = m_surface_offset_cache.find(sok);
	if (it != m_surface_offset_cache.end())
		return it->second; // Cache HIT.

	// Cache MISS.
	// Search for a valid <x,y> offset from B to A in B coordinates.
	SurfaceOffset so;
	so.is_valid = false;
	const int dx = b_psm_s.bs.x;
	const int dy = b_psm_s.bs.y;
	GSVector4i b2a_offset = GSVector4i::zero();
	if (a_el.bp >= b_el.bp)
	{
		// A starts after B, search <x,y> offset from B to A in B coords.
		for (b2a_offset.x = b_rect.x; b2a_offset.x < b_rect.z; b2a_offset.x += dx)
		{
			for (b2a_offset.y = b_rect.y; b2a_offset.y < b_rect.w; b2a_offset.y += dy)
			{
				const u32 a_candidate_bp = b_psm_s.info.bn(b2a_offset.x, b2a_offset.y, b_el.bp, b_el.bw);
				if (a_el.bp == a_candidate_bp)
				{
					so.is_valid = true; // Sweep search HIT: <x,y> offset found.
					break;
				}
			}
			if (so.is_valid)
				break;
		}
	}
	else
	{
		// B starts after A, suppose <x,y> offset is <x,y> of the B validity rectangle.
		so.is_valid = true;
		b2a_offset.x = b_rect.x;
		b2a_offset.y = b_rect.y;
	}

	assert(!so.is_valid || b2a_offset.x >= b_rect.x);
	assert(!so.is_valid || b2a_offset.x < b_rect.z);
	assert(!so.is_valid || b2a_offset.y >= b_rect.y);
	assert(!so.is_valid || b2a_offset.y < b_rect.w);

	if (so.is_valid)
	{
		// Search for a valid <z,w> offset from B to the end of A in B coordinates.
		if (a_bp_end >= b_bp_end)
		{
			// A ends after B, suppose <z,w> offset is <z,w> of the B validity rectangle.
			b2a_offset.z = b_rect.z;
			b2a_offset.w = b_rect.w;
		}
		else
		{
			// B ends after A, sweep search <z,w> offset in B coordinates.
			so.is_valid = false;
			for (b2a_offset.z = b2a_offset.x; b2a_offset.z <= b_rect.z; b2a_offset.z += dx)
			{
				for (b2a_offset.w = b2a_offset.y; b2a_offset.w <= b_rect.w; b2a_offset.w += dy)
				{
					const u32 a_candidate_bp_end = b_psm_s.info.bn(b2a_offset.z - 1, b2a_offset.w - 1, b_el.bp, b_el.bw);
					if (a_bp_end == a_candidate_bp_end)
					{
						// Align b2a_offset outside.
						if (b2a_offset.z == b2a_offset.x)
							++b2a_offset.z;
						if (b2a_offset.w == b2a_offset.y)
							++b2a_offset.w;
						b2a_offset = b2a_offset.ralign<Align_Outside>(b_psm_s.bs);
						so.is_valid = true; // Sweep search HIT: <z,w> offset found.
						break;
					}
				}
				if (so.is_valid)
					break;
			}
			if (!so.is_valid)
			{
				// Sweep search <z,w> MISS.
				GL_CACHE("TC: ComputeSurfaceOffset - Could not find <z,w> offset.");
				b2a_offset.z = b_rect.z;
				b2a_offset.w = b_rect.w;
			}
		}
	}

	assert(!so.is_valid || b2a_offset.z > b2a_offset.x);
	assert(!so.is_valid || b2a_offset.z <= b_rect.z);
	assert(!so.is_valid || b2a_offset.w > b_rect.y);
	assert(!so.is_valid || b2a_offset.w <= b_rect.w);

	so.b2a_offset = b2a_offset;

	const GSVector4i& r1 = so.b2a_offset;
	const GSVector4i& r2 = b_rect;
	[[maybe_unused]] const GSVector4i ri = r1.rintersect(r2);
	assert(!so.is_valid || (r1.eq(ri) && r1.x >= 0 && r1.y >= 0 && r1.z > 0 && r1.w > 0));

	// Clear cache if size too big.
	if (m_surface_offset_cache.size() + 1 > S_SURFACE_OFFSET_CACHE_MAX_SIZE)
	{
		GL_PERF("TC: ComputeSurfaceOffset - Size of cache %d too big, clearing it.", m_surface_offset_cache.size());
		m_surface_offset_cache.clear();
	}
	m_surface_offset_cache.emplace(std::make_pair(sok, so));
	if (so.is_valid)
	{
		GL_CACHE("TC: ComputeSurfaceOffset - Cached HIT element (size %d), [B] BW %d, PSM %s, BP 0x%x (END 0x%x) + OFF <%d,%d => %d,%d> ---> [A] BP 0x%x (END: 0x%x).",
			m_surface_offset_cache.size(), b_el.bw, psm_str(b_el.psm), b_el.bp, b_bp_end,
			so.b2a_offset.x, so.b2a_offset.y, so.b2a_offset.z, so.b2a_offset.w,
			a_el.bp, a_bp_end);
	}
	else
	{
		GL_CACHE("TC: ComputeSurfaceOffset - Cached MISS element (size %d), [B] BW %d, PSM %s, BP 0x%x (END 0x%x) -/-> [A] BP 0x%x (END: 0x%x).",
			m_surface_offset_cache.size(), b_el.bw, psm_str(b_el.psm), b_el.bp, b_bp_end,
			a_el.bp, a_bp_end);
	}
	return so;
}

void GSTextureCache::InvalidateTemporarySource()
{
	if (!m_temporary_source)
		return;

	m_src.RemoveAt(m_temporary_source);
	m_temporary_source = nullptr;
}

void GSTextureCache::InjectHashCacheTexture(const HashCacheKey& key, GSTexture* tex)
{
	// When we insert we update memory usage. Old texture gets removed below.
	m_hash_cache_replacement_memory_usage += tex->GetMemUsage();

	auto it = m_hash_cache.find(key);
	if (it == m_hash_cache.end())
	{
		// We must've got evicted before we finished loading. No matter, add it in there anyway;
		// if it's not used again, it'll get tossed out later.
		const HashCacheEntry entry{tex, 1u, 0u, true};
		m_hash_cache.emplace(key, entry);
		return;
	}

	// Reset age so we don't get thrown out too early.
	it->second.age = 0;

	// Update memory usage, swap the textures, and recycle the old one for reuse.
	if (!it->second.is_replacement)
		m_hash_cache_memory_usage -= it->second.texture->GetMemUsage();
	else
		m_hash_cache_replacement_memory_usage -= it->second.texture->GetMemUsage();

	it->second.is_replacement = true;
	it->second.texture->Swap(tex);
	g_gs_device->Recycle(tex);
}

// GSTextureCache::Palette

GSTextureCache::Palette::Palette(u16 pal, bool need_gs_texture)
	: m_pal(pal)
	, m_tex_palette(nullptr)
{
	const u16 palette_size = pal * sizeof(u32);
	m_clut = (u32*)_aligned_malloc(palette_size, 64);
	memcpy(m_clut, (const u32*)g_gs_renderer->m_mem.m_clut, palette_size);
	if (need_gs_texture)
	{
		InitializeTexture();
	}
}

GSTextureCache::Palette::~Palette()
{
	if (m_tex_palette)
	{
		GSRendererHW::GetInstance()->GetTextureCache()->m_source_memory_usage -= m_tex_palette->GetMemUsage();
		g_gs_device->Recycle(m_tex_palette);
	}

	_aligned_free(m_clut);
}

GSTexture* GSTextureCache::Palette::GetPaletteGSTexture()
{
	return m_tex_palette;
}

GSTextureCache::PaletteKey GSTextureCache::Palette::GetPaletteKey()
{
	return {m_clut, m_pal};
}

void GSTextureCache::Palette::InitializeTexture()
{
	if (!m_tex_palette)
	{
		// A palette texture is always created with dimensions 256x1 (also in the case that m_pal is 16, thus a 16x1 texture
		// would be enough to store the CLUT data) because the coordinates that the shader uses for
		// sampling such texture are always normalized by 255.
		// This is because indexes are stored as normalized values of an RGBA texture (e.g. index 15 will be read as (15/255),
		// and therefore will read texel 15/255 * texture size).
		m_tex_palette = g_gs_device->CreateTexture(m_pal, 1, 1, GSTexture::Format::Color);
		m_tex_palette->Update(GSVector4i(0, 0, m_pal, 1), m_clut, m_pal * sizeof(m_clut[0]));
		GSRendererHW::GetInstance()->GetTextureCache()->m_source_memory_usage += m_tex_palette->GetMemUsage();
	}
}

// GSTextureCache::PaletteKeyHash

u64 GSTextureCache::PaletteKeyHash::operator()(const PaletteKey& key) const
{
	ASSERT(key.pal == 16 || key.pal == 256);
	return key.pal == 16 ?
			   GSXXH3_64bits(key.clut, sizeof(key.clut[0]) * 16) :
			   GSXXH3_64bits(key.clut, sizeof(key.clut[0]) * 256);
};

// GSTextureCache::PaletteKeyEqual

bool GSTextureCache::PaletteKeyEqual::operator()(const PaletteKey& lhs, const PaletteKey& rhs) const
{
	if (lhs.pal != rhs.pal)
	{
		return false;
	}

	return GSVector4i::compare64(lhs.clut, rhs.clut, lhs.pal * sizeof(lhs.clut[0]));
};

// GSTextureCache::PaletteMap

GSTextureCache::PaletteMap::PaletteMap()
{
	for (auto& map : m_maps)
	{
		map.reserve(MAX_SIZE);
	}
}

std::shared_ptr<GSTextureCache::Palette> GSTextureCache::PaletteMap::LookupPalette(u16 pal, bool need_gs_texture)
{
	ASSERT(pal == 16 || pal == 256);

	// Choose which hash map search into:
	//    pal == 16  : index 0
	//    pal == 256 : index 1
	auto& map = m_maps[pal == 16 ? 0 : 1];

	const u32* clut = (const u32*)g_gs_renderer->m_mem.m_clut;

	// Create PaletteKey for searching into map (clut is actually not copied, so do not store this key into the map)
	const PaletteKey palette_key = {clut, pal};

	const auto& it1 = map.find(palette_key);

	if (it1 != map.end())
	{
		// Clut content match, HIT
		if (need_gs_texture && !it1->second->GetPaletteGSTexture())
		{
			// Generate GSTexture and upload clut content if needed and not done yet
			it1->second->InitializeTexture();
		}
		return it1->second;
	}

	// No palette with matching clut content, MISS

	if (map.size() > MAX_SIZE)
	{
		// If the map is too big, try to clean it by disposing and removing unused palettes, before adding the new one
		GL_INS("WARNING, %u-bit PaletteMap (Size %u): Max size %u exceeded, clearing unused palettes.", pal * sizeof(u32), map.size(), MAX_SIZE);

		const u32 current_size = map.size();

		for (auto it = map.begin(); it != map.end();)
		{
			// If the palette is unused, there is only one shared pointers holding a reference to the unused Palette object,
			// and this shared pointer is the one stored in the map itself
			if (it->second.use_count() <= 1)
			{
				// Palette is unused
				it = map.erase(it); // Erase element from map
									// The palette object should now be gone as the shared pointer to the object in the map is deleted
			}
			else
			{
				++it;
			}
		}

		const u32 cleared_palette_count = current_size - static_cast<u32>(map.size());

		if (cleared_palette_count == 0)
		{
			GL_INS("ERROR, %u-bit PaletteMap (Size %u): Max size %u exceeded, could not clear any palette, negative performance impact.", pal * sizeof(u32), map.size(), MAX_SIZE);
		}
		else
		{
			map.reserve(MAX_SIZE); // Ensure map capacity is not modified by the clearing
			GL_INS("INFO, %u-bit PaletteMap (Size %u): Cleared %u palettes.", pal * sizeof(u32), map.size(), cleared_palette_count);
		}
	}

	std::shared_ptr<Palette> palette = std::make_shared<Palette>(pal, need_gs_texture);

	map.emplace(palette->GetPaletteKey(), palette);

	GL_CACHE("TC, %u-bit PaletteMap (Size %u): Added new palette.", pal * sizeof(u32), map.size());

	return palette;
}

void GSTextureCache::PaletteMap::Clear()
{
	for (auto& map : m_maps)
	{
		map.clear(); // Clear all the nodes of the map, deleting Palette objects managed by shared pointers as they should be unused elsewhere
		map.reserve(MAX_SIZE); // Ensure map capacity is not modified by the clearing
	}
}

std::size_t GSTextureCache::SurfaceOffsetKeyHash::operator()(const GSTextureCache::SurfaceOffsetKey& key) const
{
	std::hash<u32> hash_fn_u32;
	std::hash<int> hash_fn_int;
	std::hash<size_t> hash_fn_szt;
	size_t hash = 0x9e3779b9;
	for (const SurfaceOffsetKeyElem& elem : key.elems)
	{
		hash = hash ^ hash_fn_u32(elem.bp) << 1;
		hash = hash ^ hash_fn_u32(elem.bw) << 1;
		hash = hash ^ hash_fn_u32(elem.psm) << 1;
		hash = hash ^ hash_fn_int(elem.rect.x) << 1;
		hash = hash ^ hash_fn_int(elem.rect.y) << 1;
		hash = hash ^ hash_fn_int(elem.rect.z) << 1;
		hash = hash ^ hash_fn_int(elem.rect.w) << 1;
	}
	return hash_fn_szt(hash);
}

bool GSTextureCache::SurfaceOffsetKeyEqual::operator()(const GSTextureCache::SurfaceOffsetKey& lhs, const GSTextureCache::SurfaceOffsetKey& rhs) const
{
	for (size_t i = 0; i < lhs.elems.size(); ++i)
	{
		const SurfaceOffsetKeyElem& lhs_elem = lhs.elems.at(i);
		const SurfaceOffsetKeyElem& rhs_elem = rhs.elems.at(i);
		if (lhs_elem.bp != rhs_elem.bp
			|| lhs_elem.bw != rhs_elem.bw
			|| lhs_elem.psm != rhs_elem.psm
			|| !lhs_elem.rect.eq(rhs_elem.rect))
			return false;
	}
	return true;
}

bool GSTextureCache::SourceRegion::IsFixedTEX0(int tw, int th) const
{
	return IsFixedTEX0W(tw) || IsFixedTEX0H(th);
}

bool GSTextureCache::SourceRegion::IsFixedTEX0W(int tw) const
{
	return (GetMaxX() > static_cast<u32>(tw));
}

bool GSTextureCache::SourceRegion::IsFixedTEX0H(int th) const
{
	return (GetMaxY() > static_cast<u32>(th));
}

GSVector4i GSTextureCache::SourceRegion::GetRect(int tw, int th) const
{
	return GSVector4i(HasX() ? GetMinX() : 0, HasY() ? GetMinY() : 0, HasX() ? GetMaxX() : tw, HasY() ? GetMaxY() : th);
}

GSVector4i GSTextureCache::SourceRegion::GetOffset(int tw, int th) const
{
	const int xoffs = (GetMaxX() > static_cast<u32>(tw)) ? static_cast<int>(GetMinX()) : 0;
	const int yoffs = (GetMaxY() > static_cast<u32>(th)) ? static_cast<int>(GetMinY()) : 0;
	return GSVector4i(xoffs, yoffs, xoffs, yoffs);
}

GSTextureCache::SourceRegion GSTextureCache::SourceRegion::AdjustForMipmap(u32 level) const
{
	// Texture levels must be at least one pixel wide/high.
	SourceRegion ret = {};
	if (HasX())
	{
		const u32 new_minx = GetMinX() >> level;
		const u32 new_maxx = std::max<u32>(GetMaxX() >> level, new_minx + 1);
		ret.SetX(new_minx, new_maxx);
	}
	if (HasY())
	{
		const u32 new_miny = GetMinY() >> level;
		const u32 new_maxy = std::max<u32>(GetMaxY() >> level, new_miny + 1);
		ret.SetY(new_miny, new_maxy);
	}
	return ret;
}

void GSTextureCache::SourceRegion::AdjustTEX0(GIFRegTEX0* TEX0) const
{
	const GSOffset offset(GSLocalMemory::m_psm[TEX0->PSM].info, TEX0->TBP0, TEX0->TBW, TEX0->PSM);
	TEX0->TBP0 += offset.bn(GetMinX(), GetMinY());
}

using BlockHashState = XXH3_state_t;

__fi static void BlockHashReset(BlockHashState& st)
{
	XXH3_64bits_reset(&st);
}

__fi static void BlockHashAccumulate(BlockHashState& st, const u8* bp)
{
	GSXXH3_64bits_update(&st, bp, BLOCK_SIZE);
}

__fi static void BlockHashAccumulate(BlockHashState& st, const u8* bp, u32 size)
{
	GSXXH3_64bits_update(&st, bp, size);
}

__fi static GSTextureCache::HashType FinishBlockHash(BlockHashState& st)
{
	return GSXXH3_64bits_digest(&st);
}

static void HashTextureLevel(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, GSTextureCache::SourceRegion region, BlockHashState& hash_st, u8* temp)
{
	const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[TEX0.PSM];
	const GSVector2i& bs = psm.bs;
	const int tw = region.HasX() ? region.GetWidth() : (1 << TEX0.TW);
	const int th = region.HasY() ? region.GetHeight() : (1 << TEX0.TH);

	// From GSLocalMemory foreachBlock(), used for reading textures.
	// We want to hash the exact same blocks here.
	const GSVector4i rect(region.GetRect(tw, th));
	const GSVector4i block_rect(rect.ralign<Align_Outside>(bs));
	GSLocalMemory& mem = g_gs_renderer->m_mem;
	const GSOffset off = mem.GetOffset(TEX0.TBP0, TEX0.TBW, TEX0.PSM);

	// For textures which are smaller than the block size, we expand and then hash.
	// This is because otherwise we get the padding bytes, which can be random junk.
	// We also expand formats where the bits contributing to the texture do not cover
	// all the 32 bits, as otherwise we'll get differing hash values if game overlap
	// the texture data with other textures/framebuffers/etc (which is common).
	// Even though you might think this would be slower than just hashing for the hash
	// cache, it actually ends up faster (unswizzling is faster than hashing).
	if (tw < bs.x || th < bs.y || psm.fmsk != 0xFFFFFFFFu || region.GetMaxX() > 0 || region.GetMinY() > 0)
	{
		// Expand texture indices. Align to 32 bytes for AVX2.
		const u32 pitch = Common::AlignUpPow2(static_cast<u32>(block_rect.z), 32);
		const u32 row_size = static_cast<u32>(tw);
		const GSLocalMemory::readTexture rtx = psm.rtxP;

		// Use temp buffer for expanding, since we may not need to update.
		rtx(mem, off, block_rect, temp, pitch, TEXA);

		// Hash the expanded texture.
		u8* ptr = temp + (pitch * static_cast<u32>(rect.top - block_rect.top)) +
				  static_cast<u32>(rect.left - block_rect.left);
		if (pitch == row_size)
		{
			BlockHashAccumulate(hash_st, ptr, pitch * static_cast<u32>(th));
		}
		else
		{
			for (int y = 0; y < th; y++, ptr += pitch)
				BlockHashAccumulate(hash_st, ptr, row_size);
		}
	}
	else
	{
		GSOffset::BNHelper bn = off.bnMulti(block_rect.left, block_rect.top);
		const int right = block_rect.right >> off.blockShiftX();
		const int bottom = block_rect.bottom >> off.blockShiftY();
		const int xAdd = (1 << off.blockShiftX()) * (psm.bpp / 8);

		for (; bn.blkY() < bottom; bn.nextBlockY())
		{
			for (int x = 0; bn.blkX() < right; bn.nextBlockX(), x += xAdd)
			{
				BlockHashAccumulate(hash_st, mem.BlockPtr(bn.value()));
			}
		}
	}
}

GSTextureCache::HashType GSTextureCache::HashTexture(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, SourceRegion region)
{
	BlockHashState hash_st;
	BlockHashReset(hash_st);
	HashTextureLevel(TEX0, TEXA, region, hash_st, s_unswizzle_buffer);
	return FinishBlockHash(hash_st);
}

void GSTextureCache::PreloadTexture(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, SourceRegion region, GSLocalMemory& mem, bool paltex, GSTexture* tex, u32 level)
{
	// m_TEX0 is adjusted for mips (messy, should be changed).
	const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[TEX0.PSM];
	const GSVector2i& bs = psm.bs;
	const int tw = region.HasX() ? region.GetWidth() : (1 << TEX0.TW);
	const int th = region.HasY() ? region.GetHeight() : (1 << TEX0.TH);

	// Expand texture/apply palette.
	const GSVector4i rect(region.GetRect(tw, th));
	const GSVector4i block_rect(rect.ralign<Align_Outside>(bs));
	const GSOffset off(mem.GetOffset(TEX0.TBP0, TEX0.TBW, TEX0.PSM));
	const int read_width = block_rect.width();
	u32 pitch = static_cast<u32>(read_width) * sizeof(u32);
	GSLocalMemory::readTexture rtx = psm.rtx;
	if (paltex)
	{
		pitch >>= 2;
		rtx = psm.rtxP;
	}

	// If we can stream it directly to GPU memory, do so, otherwise go through a temp buffer.
	const GSVector4i unoffset_rect(0, 0, tw, th);
	GSTexture::GSMap map;
	if (rect.eq(block_rect) && tex->Map(map, &unoffset_rect, level))
	{
		rtx(mem, off, block_rect, map.bits, map.pitch, TEXA);
		tex->Unmap();
	}
	else
	{
		// Align pitch to 32 bytes for AVX2 if we're going through the temp buffer path.
		pitch = Common::AlignUpPow2(pitch, 32);

		u8* buff = s_unswizzle_buffer;
		rtx(mem, off, block_rect, buff, pitch, TEXA);

		const u8* ptr = buff + (pitch * static_cast<u32>(rect.top - block_rect.top)) +
						(static_cast<u32>(rect.left - block_rect.left) << (paltex ? 0 : 2));
		tex->Update(unoffset_rect, ptr, pitch, level);
	}
}

GSTextureCache::HashCacheKey::HashCacheKey()
	: TEX0Hash(0)
	, CLUTHash(0)
{
	TEX0.U64 = 0;
	TEXA.U64 = 0;
}

GSTextureCache::HashCacheKey GSTextureCache::HashCacheKey::Create(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, const u32* clut, const GSVector2i* lod, SourceRegion region)
{
	const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[TEX0.PSM];

	HashCacheKey ret;
	ret.TEX0.U64 = TEX0.U64 & 0x00000007FFF00000ULL;
	ret.TEXA.U64 = (psm.pal == 0 && psm.fmt > 0) ? (TEXA.U64 & 0x000000FF000080FFULL) : 0;
	ret.CLUTHash = clut ? GSTextureCache::PaletteKeyHash{}({clut, psm.pal}) : 0;
	ret.region = region;

	BlockHashState hash_st;
	BlockHashReset(hash_st);

	// base level is always hashed
	HashTextureLevel(TEX0, TEXA, region, hash_st, s_unswizzle_buffer);

	if (lod)
	{
		// hash and combine full mipmaps when enabled
		const int basemip = lod->x;
		const int nmips = lod->y - lod->x + 1;
		for (int i = 1; i < nmips; i++)
		{
			const GIFRegTEX0 MIP_TEX0{g_gs_renderer->GetTex0Layer(basemip + i)};
			HashTextureLevel(MIP_TEX0, TEXA, region.AdjustForMipmap(i), hash_st, s_unswizzle_buffer);
		}
	}

	ret.TEX0Hash = FinishBlockHash(hash_st);

	return ret;
}

GSTextureCache::HashCacheKey GSTextureCache::HashCacheKey::WithRemovedCLUTHash() const
{
	HashCacheKey ret{*this};
	ret.CLUTHash = 0;
	return ret;
}

void GSTextureCache::HashCacheKey::RemoveCLUTHash()
{
	CLUTHash = 0;
}

u64 GSTextureCache::HashCacheKeyHash::operator()(const HashCacheKey& key) const
{
	std::size_t h = 0;
	HashCombine(h, key.TEX0Hash, key.CLUTHash, key.TEX0.U64, key.TEXA.U64, key.region.bits);
	return h;
}
