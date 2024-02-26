// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GSTextureCache.h"
#include "GSTextureReplacements.h"
#include "GSRendererHW.h"
#include "GS/GSState.h"
#include "GS/GSGL.h"
#include "GS/GSPerfMon.h"
#include "GS/GSUtil.h"
#include "GS/GSXXH.h"

#include "common/Console.h"
#include "common/BitUtils.h"
#include "common/HashCombine.h"
#include "common/SmallString.h"

#include "fmt/format.h"

#include <cinttypes>

#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif

std::unique_ptr<GSTextureCache> g_texture_cache;

static u8* s_unswizzle_buffer;

/// List of candidates for purging when the hash cache gets too large.
static std::vector<std::pair<GSTextureCache::HashCacheMap::iterator, s32>> s_hash_cache_purge_list;

#ifdef PCSX2_DEVBUILD
// We can only set one texture name per command buffer, which would break our fancy texture cache RT/DS/texture naming.
// So, when debug device is enabled, don't reuse any textures that are drawable.
__fi static bool PreferReusedLabelledTexture()
{
	return !GSConfig.UseDebugDevice;
}
#else
__fi static constexpr bool PreferReusedLabelledTexture()
{
	return true;
}
#endif

GSTextureCache::GSTextureCache()
{
	// In theory 4MB is enough but 9MB is safer for overflow (8MB
	// isn't enough in custom resolution)
	// Test: onimusha 3 PAL 60Hz
	s_unswizzle_buffer = (u8*)_aligned_malloc(9 * 1024 * 1024, VECTOR_ALIGNMENT);
	pxAssertRel(s_unswizzle_buffer, "Failed to allocate unswizzle buffer");

	m_surface_offset_cache.reserve(S_SURFACE_OFFSET_CACHE_MAX_SIZE);
}

GSTextureCache::~GSTextureCache()
{
	RemoveAll(true, true, true);

	s_hash_cache_purge_list = {};
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

void GSTextureCache::RemoveAll(bool sources, bool targets, bool hash_cache)
{
	if (sources || targets)
	{
		m_src.RemoveAll();
		m_palette_map.Clear();
		m_source_memory_usage = 0;
	}

	if (targets)
	{
		for (int type = 0; type < 2; type++)
		{
			for (auto t : m_dst[type])
				delete t;

			m_dst[type].clear();
		}

		m_target_heights.clear();
		m_surface_offset_cache.clear();
		m_target_memory_usage = 0;
	}

	if (hash_cache)
	{
		for (auto it : m_hash_cache)
			g_gs_device->Recycle(it.second.texture);

		m_hash_cache.clear();
		m_hash_cache_memory_usage = 0;
		m_hash_cache_replacement_memory_usage = 0;
	}
}

bool GSTextureCache::FullRectDirty(Target* target, u32 rgba_mask)
{
	// One complete dirty rect, not pieces (Add dirty rect function should be able to join these all together).
	if (target->m_dirty.size() == 1 && (rgba_mask & target->m_dirty[0].rgba._u32) == rgba_mask && target->m_valid.rintersect(target->m_dirty[0].r).eq(target->m_valid))
	{
		if (rgba_mask == 0x8 && target->m_TEX0.PSM == PSMCT32)
		{
			target->m_valid_alpha_high = false;
			target->m_valid_alpha_low = false;
			return false;
		}
		return true;
	}

	// Check drawn areas on dst matches.
	if (target->m_was_dst_matched && target->m_dirty.size() == 1 && target->m_drawn_since_read.rintersect(target->m_dirty[0].r).eq(target->m_drawn_since_read))
	{
		return true;
	}

	return false;
}

bool GSTextureCache::FullRectDirty(Target* target)
{
	// Why?
	if (target->m_age == 0)
		return false;

	return FullRectDirty(target, GSUtil::GetChannelMask(target->m_TEX0.PSM));
}

void GSTextureCache::AddDirtyRectTarget(Target* target, GSVector4i rect, u32 psm, u32 bw, RGBAMask rgba, bool req_linear)
{
	bool skipdirty = false;
	bool canskip = true;

	if (rect.rempty())
		return;

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
				(it[0].r.ywyw().eq(rect.ywyw()) && (it[0].r.zzzz().eq(rect.xxxx()) || it[0].r.xxxx().eq(rect.zzzz()))) ||
				it[0].r.rintersect(rect).eq(it[0].r)) // If the new rect completely envelops the old one.
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
		GL_INS("Dirty rect added for BP %x rect %d,%d->%d->%d", target->m_TEX0.TBP0, rect.x, rect.y, rect.z, rect.w);
		target->m_dirty.push_back(GSDirtyRect(rect, psm, bw, rgba, req_linear));

		if (!target->m_drawn_since_read.rempty())
		{
			// If we're covering the drawn area, clear it, in case of readback.
			if (target->m_dirty[0].rgba._u32 == GSUtil::GetChannelMask(target->m_TEX0.PSM) && target->m_drawn_since_read.rintersect(target->m_dirty.GetTotalRect(target->m_TEX0, target->m_unscaled_size)).eq(target->m_drawn_since_read))
			{
				// Probably not completely covered and is still relevant, so don't wipe it.
				if (target->m_dirty.size() > 1 && target->m_age == 0)
					return;

				target->m_drawn_since_read = GSVector4i::zero();
			}
		}
	}
}

void  GSTextureCache::ResizeTarget(Target* t, GSVector4i rect, u32 tbp, u32 psm, u32 tbw)
{
	// Valid area isn't the whole texture anyway, no point in expanding.
	if (t->m_valid.z < t->m_unscaled_size.x || t->m_valid.w < t->m_unscaled_size.y)
		return;

	const GSVector2i size_delta = { std::max(0, (rect.z - t->m_valid.z)), std::max(0, (rect.w - t->m_valid.w)) };
	// If it's 1 row, it's probably the texture bounds accounting for bilinear, ignore it.
	if (size_delta.x > 1 || size_delta.y > 1)
	{
		RGBAMask rgba;
		rgba._u32 = GSUtil::GetChannelMask(t->m_TEX0.PSM);
		// Dirty the expanded areas.
		AddDirtyRectTarget(t, GSVector4i(t->m_valid.x, t->m_valid.w, t->m_valid.z + std::max(0, size_delta.x), t->m_valid.w + std::max(0, size_delta.y)), t->m_TEX0.PSM, t->m_TEX0.TBW, rgba);
		AddDirtyRectTarget(t, GSVector4i(t->m_valid.z, t->m_valid.y, t->m_valid.z + std::max(0, size_delta.x), t->m_valid.w), t->m_TEX0.PSM, t->m_TEX0.TBW, rgba);
		const GSVector4i valid_rect = { t->m_valid.x, t->m_valid.y, t->m_valid.z + std::max(0, size_delta.x), t->m_valid.w + std::max(0, size_delta.y) };
		// Resizes of edges due to bilinear filtering and tex is rt could cause bad valid rects.
		t->UpdateValidity(valid_rect, size_delta.x > 2 || size_delta.y > 2);
		GetTargetSize(tbp, tbw, psm, valid_rect.z, valid_rect.w);
		const int new_w = std::max(t->m_unscaled_size.x, valid_rect.z);
		const int new_h = std::max(t->m_unscaled_size.y, valid_rect.w);
		t->ResizeTexture(new_w, new_h);
	}
}


bool GSTextureCache::CanTranslate(u32 bp, u32 bw, u32 spsm, GSVector4i r, u32 dbp, u32 dpsm, u32 dbw)
{
	const GSVector2i src_page_size = GSLocalMemory::m_psm[spsm].pgs;
	const GSVector2i dst_page_size = GSLocalMemory::m_psm[dpsm].pgs;
	const bool bp_page_aligned_bp = ((bp & ~((1 << 5) - 1)) == bp) || bp == dbp;
	const bool block_layout_match = GSLocalMemory::m_psm[spsm].bpp == GSLocalMemory::m_psm[dpsm].bpp;
	const GSVector4i page_mask(GSVector4i((src_page_size.x - 1), (src_page_size.y - 1)).xyxy());
	const GSVector4i masked_rect(r & ~page_mask);
	const int src_pixel_width = static_cast<int>(bw * 64);
	// We can do this if:
	// The page width matches.
	// The rect width is less than the width of the destination texture and the height is less than or equal to 1 page high.
	// The rect width and height is equal to the page size and it covers the width of the incoming bw, so lines are sequential.
	const bool page_aligned_rect = masked_rect.eq(r);
	const bool width_match = ((bw * 64) / src_page_size.x) == ((dbw * 64) / dst_page_size.x);
	const bool sequential_pages = page_aligned_rect && r.x == 0 && r.z == src_pixel_width;
	const bool single_row = (((bw * 64) / src_page_size.x) <= ((dbw * 64) / dst_page_size.x)) && r.z <= src_pixel_width && r.w <= src_page_size.y;

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


GSVector4i GSTextureCache::TranslateAlignedRectByPage(u32 tbp, u32 tebp, u32 tbw, u32 tpsm, u32 sbp, u32 spsm, u32 sbw, GSVector4i src_r, bool is_invalidation)
{
	const GSVector2i src_page_size = GSLocalMemory::m_psm[spsm].pgs;
	const GSVector2i dst_page_size = GSLocalMemory::m_psm[tpsm].pgs;
	const int src_bw = static_cast<int>(std::max(1U, sbw) * 64);
	const int dst_bw = static_cast<int>(std::max(1U, tbw) * 64);
	const int src_pgw = std::max(1, src_bw / src_page_size.x);
	const int dst_pgw = std::max(1, dst_bw / dst_page_size.x);
	GSVector4i in_rect = src_r;

	if (sbp < tebp && tebp < tbp)
		sbp += 0x4000;

	int page_offset = (static_cast<int>(sbp) - static_cast<int>(tbp)) >> 5;
	bool single_page = (in_rect.width() / src_page_size.x) <= 1 && (in_rect.height() / src_page_size.y) <= 1;
	if (!single_page)
	{
		const int inc_vertical_offset = (page_offset / src_pgw) * src_page_size.y;
		const int inc_horizontal_offset = (page_offset % src_pgw) * src_page_size.x;
		in_rect = (in_rect + GSVector4i(0, inc_vertical_offset).xyxy()).max_i32(GSVector4i(0));
		in_rect = (in_rect + GSVector4i(inc_horizontal_offset, 0).xyxy()).max_i32(GSVector4i(0));
		page_offset = 0;
		single_page = (in_rect.width() / src_page_size.x) <= 1 && (in_rect.height() / src_page_size.y) <= 1;
	}
	const int vertical_offset = (page_offset / dst_pgw) * dst_page_size.y;
	int horizontal_offset = (page_offset % dst_pgw) * dst_page_size.x;
	const GSVector4i rect_pages = GSVector4i(in_rect.x / src_page_size.x, in_rect.y / src_page_size.y, (in_rect.z + src_page_size.x - 1) / src_page_size.x, (in_rect.w + (src_page_size.y - 1)) / src_page_size.y);
	const bool block_layout_match = GSLocalMemory::m_psm[spsm].bpp == GSLocalMemory::m_psm[tpsm].bpp;
	GSVector4i new_rect = {};

	if (src_pgw != dst_pgw || sbw == 0)
	{
		if (sbw == 0) // Intentionally check this separately
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
		else if (static_cast<int>(src_pgw) == rect_pages.width())
		{

			const int totalpages = rect_pages.width() * rect_pages.height();
			const bool full_rows = in_rect.width() == (src_pgw * src_page_size.x);
			const bool single_row = in_rect.x == 0 && in_rect.y == 0 && totalpages <= dst_pgw;
			const bool uneven_pages = (horizontal_offset || (totalpages % dst_pgw) != 0) && !single_row;

			// Less than or equal a page and the block layout matches, just copy the rect
			if (block_layout_match && single_page)
			{
				new_rect = in_rect;
			}
			else if (uneven_pages)
			{
				// Results won't be square, if it's not invalidation, it's a texture, which is problematic to translate, so let's not (FIFA 2005).
				if (!is_invalidation)
				{
					DevCon.Warning("Uneven pages mess up sbp %x dbp %x spgw %d dpgw %d", sbp, tbp, src_pgw, dst_pgw);
					return GSVector4i::zero();
				}

				//TODO: Maybe control dirty blocks directly and add them page at a time for better granularity.
				const u32 start_y_page = (rect_pages.y * src_pgw) / dst_pgw;
				const u32 end_y_page = ((rect_pages.w * src_pgw) + (dst_pgw - 1)) / dst_pgw;

				// Not easily translatable full pages and make sure the height is rounded upto encompass the half row.
				horizontal_offset = 0;
				new_rect.x = 0;
				new_rect.z = (dst_pgw * dst_page_size.x);
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
					new_rect.z = std::min(totalpages * dst_page_size.x, dst_pgw * dst_page_size.x);
					new_rect.y = start_y_page * dst_page_size.y;
					new_rect.w = new_rect.y + (((totalpages + (dst_pgw - 1)) / dst_pgw) * dst_page_size.y);
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
			// The width is mismatched to the page.
			// Kinda scary but covering the whole row and the next one should be okay? :/ (Check with MVP 07, sbp == 0x39a0)
			if (rect_pages.z > dst_pgw)
			{
				if (!is_invalidation)
					return GSVector4i::zero();

				// const u32 offset = rect_pages.z - (dst_pgw);
				new_rect.x = 0;
				new_rect.z = dst_pgw * dst_page_size.x;
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

	if (new_rect.z > (static_cast<int>(tbw) * dst_page_size.x))
	{
		new_rect.z = (dst_pgw * dst_page_size.x);
		new_rect.w += dst_page_size.y;
	}

	return new_rect;
}

GSVector4i GSTextureCache::TranslateAlignedRectByPage(Target* t, u32 sbp, u32 spsm, u32 sbw, GSVector4i src_r, bool is_invalidation)
{
	return TranslateAlignedRectByPage(t->m_TEX0.TBP0, t->m_end_block, t->m_TEX0.TBW, t->m_TEX0.PSM, sbp, spsm, sbw, src_r, is_invalidation);
}

void GSTextureCache::DirtyRectByPage(u32 sbp, u32 spsm, u32 sbw, Target* t, GSVector4i src_r)
{
	if (src_r.rempty())
		return;

	const u32 start_bp = GSLocalMemory::GetStartBlockAddress(sbp, sbw, spsm, src_r);
	const u32 end_bp = GSLocalMemory::GetEndBlockAddress(sbp, sbw, spsm, src_r);
	GL_INS("Invalidating BP: 0x%x (%x -> %x) BW: %d PSM %s Target BP: 0x%x BW %x PSM %s", sbp, start_bp, end_bp, sbw, psm_str(spsm), t->m_TEX0.TBP0, t->m_TEX0.TBW, psm_str(t->m_TEX0.PSM));

	// If the whole thing is covered, just invalidate the whole rect.
	if (start_bp <= t->m_TEX0.TBP0 && end_bp >= t->UnwrappedEndBlock())
	{
		RGBAMask rgba;
		rgba._u32 = GSUtil::GetChannelMask(spsm);
		AddDirtyRectTarget(t, t->m_valid, t->m_TEX0.PSM, t->m_TEX0.TBW, rgba);
		return;
	}

	GSLocalMemory::psm_t* src_info = &GSLocalMemory::m_psm[spsm];
	const GSLocalMemory::psm_t* dst_info = &GSLocalMemory::m_psm[t->m_TEX0.PSM];
	const int dst_width = std::max(static_cast<int>(t->m_TEX0.TBW * 64), 64);

	int src_width = std::max(static_cast<int>(sbw * 64), 64);
	int src_psm = spsm;

	GSVector4i in_rect = src_r;
	u32 target_bp = t->m_TEX0.TBP0;
	int block_offset = static_cast<int>(sbp) - static_cast<int>(target_bp);
	// Different format needs to be page aligned, unless the block layout matches, then we can block align
	// Might be able to translate the original rect.
	if (!(src_info->bpp == dst_info->bpp))
	{
		const int src_bpp = src_info->bpp;

		if(block_offset)
			in_rect = in_rect.ralign<Align_Outside>(src_info->bs);
		else
			in_rect = in_rect.ralign<Align_Outside>(src_info->pgs);

		// Convert rect down in to pages and blocks.
		const GSVector4i in_pages = GSVector4i(in_rect.x / src_info->pgs.x, in_rect.y / src_info->pgs.y, in_rect.z / src_info->pgs.x, in_rect.w / src_info->pgs.y);
		in_rect -= GSVector4i(in_pages.x * src_info->pgs.x, in_pages.y * src_info->pgs.y, in_pages.z * src_info->pgs.x, in_pages.w * src_info->pgs.y);
		// Handle a minimum of 1 block, they are a different shape between 16 and 32bit. 8x8 vs 16x8.
		// FIXME: Block layouts are different between 32bit/8bit and other formats (8x4 instead of 4x8), so this could be a problem if the game invalidates too much.
		const GSVector4i in_blocks = GSVector4i(in_rect.x / src_info->bs.x, in_rect.y / src_info->bs.y, (in_rect.z + (src_info->bs.x - 1)) / src_info->bs.x, (in_rect.w + (src_info->bs.y - 1)) / src_info->bs.y);

		src_psm = t->m_TEX0.PSM;
		src_info = &GSLocalMemory::m_psm[src_psm];

		// Translate back to the new(dst) format.
		in_rect = GSVector4i(in_pages.x * src_info->pgs.x, in_pages.y * src_info->pgs.y, in_pages.z * src_info->pgs.x, in_pages.w * src_info->pgs.y);
		in_rect += GSVector4i(in_blocks.x * src_info->bs.x, in_blocks.y * src_info->bs.y, in_blocks.z * src_info->bs.x, in_blocks.w * src_info->bs.y);

		if (in_rect.rempty())
			return;

		if (src_bpp < 16 || dst_info->bpp < 16)
		{
			// FIXME: This could break down if the width is 1 (64 pixels) as this is half a page in memory, but I don't have a good solution right now.
			if (dst_info->bpp >= 16 && !(src_width & 127))
				src_width = std::max(src_width / 2, 64);
			else if (dst_info->bpp < 16)
				src_width *= 2;
		}
	}

	// This will round up pages for smaller formats such as PSMT8 and PSMT4.
	// FIXME: Is this a problem? Does having buffer widths less than pages in size throw a real spanner in the works?
	const int src_pg_width = std::max((src_width + (src_info->pgs.x - 1)) / src_info->pgs.x, 1);
	const int dst_pg_width = std::max((dst_width + (dst_info->pgs.x - 1)) / dst_info->pgs.x, 1);

	int page_offset = (block_offset) >> 5;
	// remove any hoizontal offset, this is added back on later.
	int start_page = page_offset + (in_rect.x / src_info->pgs.x) + ((in_rect.y / src_info->pgs.y) * std::max(static_cast<int>(src_width / 64), 1));
	const int horizontal_pages = (start_page % src_pg_width);
	start_page -= horizontal_pages;

	// Pages aligned.
	const GSVector4i page_mask(GSVector4i((src_info->pgs.x - 1), (src_info->pgs.y - 1)).xyxy());
	const GSVector4i page_masked_rect(in_rect & ~page_mask);

	const bool page_aligned_rect = page_masked_rect.eq(in_rect);

	// Blocks aligned.
	const GSVector4i block_mask(GSVector4i((src_info->bs.x - 1), (src_info->bs.y - 1)).xyxy());
	const GSVector4i block_masked_rect(in_rect & ~block_mask);

	const bool block_aligned_rect = block_masked_rect.eq(in_rect);

	int x_offset = 0;
	int y_offset = 0;

	// Deal with the page offset first, this will have the largest stride.
	if (page_offset)
	{
		const int inc_vertical_offset = (page_offset / src_pg_width) * src_info->pgs.y;
		const int inc_horizontal_offset = (page_offset % src_pg_width) * src_info->pgs.x;
		in_rect = (in_rect + GSVector4i(0, inc_vertical_offset).xyxy()).max_i32(GSVector4i(0));
		if (inc_horizontal_offset)
		{
			if (inc_horizontal_offset > 0)
			{
				const int max_horizontal_adjust = std::min(src_width - in_rect.z, inc_horizontal_offset);
				in_rect = (in_rect + GSVector4i(max_horizontal_adjust, 0).xyxy()).max_i32(GSVector4i(0));
				const int h_page_offset = (inc_horizontal_offset / src_info->pgs.x);
				page_offset -= h_page_offset;
				sbp -= h_page_offset << 5;

				if (max_horizontal_adjust == 0)
					x_offset = inc_horizontal_offset;
			}
			else if (inc_horizontal_offset < 0)
			{
				const int max_horizontal_adjust = std::min(in_rect.x, std::abs(inc_horizontal_offset));
				in_rect = (in_rect - GSVector4i(max_horizontal_adjust, 0).xyxy()).max_i32(GSVector4i(0));
				const int h_page_offset = (max_horizontal_adjust / src_info->pgs.x);
				page_offset += h_page_offset;
				sbp += h_page_offset << 5;
			}
		}
		const int vertical_offset = (inc_vertical_offset / src_info->pgs.y) * src_pg_width;
		page_offset -= vertical_offset;

		if (start_page < 0 && in_rect.x == 0 && in_rect.y == 0)
			start_page -= vertical_offset;

		sbp -= vertical_offset << 5;
		// Update the block offset.
		block_offset = static_cast<int>(sbp) - static_cast<int>(target_bp);
	}
	
	if (!x_offset)
		start_page += horizontal_pages;

	const bool matched_format = (src_info->bpp == dst_info->bpp);
	const bool block_matched_format = matched_format && block_aligned_rect;
	const bool req_depth_offset = (src_info->depth > 0 && !page_aligned_rect);

	// If there is block offset left over, try to adjust to that.
	if (block_matched_format && (block_offset || req_depth_offset))
	{
		if (block_offset > 0 || req_depth_offset)
		{
			const int xblocks = in_rect.width() / src_info->bs.x;
			const int yblocks = in_rect.height() / src_info->bs.y;
			// if !(block_offset & 0x7) is false, this is technically incorrect, but FFX hates it and starts making a mess, so it's better this way without adding complexity.
			// TODO maybe: Add per block invalidation? ugh, would have to keep that to small blocks. 2 blocks in the case of FFX.
			if ((xblocks <= 4 && yblocks <= 2) || req_depth_offset)
			{
				GSVector4i b2a_offset = GSVector4i::zero();
				const GSVector4i target_rect = GSVector4i(0, 0, src_width, 2048);
				bool offset_found = false;

				for (b2a_offset.x = target_rect.x; b2a_offset.x < target_rect.z; b2a_offset.x += src_info->bs.x)
				{
					for (b2a_offset.y = target_rect.y; b2a_offset.y < target_rect.w; b2a_offset.y += src_info->bs.y)
					{
						const u32 a_candidate_bp = src_info->info.bn(b2a_offset.x, b2a_offset.y, target_bp, src_pg_width);
						if (sbp == a_candidate_bp)
						{
							offset_found = true;
							break;
						}
					}
					if (offset_found)
						break;
				}

				if (offset_found)
				{
					if (b2a_offset.x && (b2a_offset.x + in_rect.z) > src_width)
					{
						if ((b2a_offset.x + in_rect.z) <= dst_width)
						{
							x_offset += b2a_offset.x;
						}

						b2a_offset.x = 0;
					}
					in_rect = (in_rect + b2a_offset.xyxy()).max_i32(GSVector4i(0));

					if (block_offset > 0 && !req_depth_offset)
					{
						sbp = dst_info->info.bn(b2a_offset.x + x_offset, b2a_offset.y, target_bp, t->m_TEX0.TBW);
						// Adjust the target BP to be pointing to the position without the offset.
						target_bp = dst_info->info.bn(b2a_offset.x, b2a_offset.y, target_bp, t->m_TEX0.TBW);
					}
				}
			}
			else
			{
				in_rect.x = in_rect.x & ~(src_info->pgs.x - 1);
				in_rect.z = (in_rect.z + (src_info->pgs.x - 1)) & ~(src_info->pgs.x - 1);
				in_rect.y = in_rect.y & ~(src_info->pgs.y - 1);
				in_rect.w = (in_rect.w + (src_info->pgs.y - 1)) & ~(src_info->pgs.y - 1);
			}

		}
		else
		{
			in_rect.x = in_rect.x & ~(src_info->pgs.x - 1);
			in_rect.z = (in_rect.z + (src_info->pgs.x - 1)) & ~(src_info->pgs.x - 1);
			in_rect.y = in_rect.y & ~(src_info->pgs.y - 1);
			in_rect.w = (in_rect.w + (src_info->pgs.y - 1)) & ~(src_info->pgs.y - 1);
		}
	}

	// Match the space for the width of the buffer, based on 64 pixels (1 page) for all formats, except PSMT8/4 which is half.
	// So multiply the others when comparing, so we don't lose "off by 1 width" comparison.
	const int adjusted_src_width = (src_info->bpp >= 16) ? src_width * 2 : src_width;
	const int adjusted_dst_width = (dst_info->bpp >= 16) ? dst_width * 2 : dst_width;

	const bool width_okay = (adjusted_src_width == adjusted_dst_width) || (in_rect.z <= dst_width && in_rect.w <= src_info->pgs.y);

	RGBAMask rgba;
	rgba._u32 = GSUtil::GetChannelMask(spsm);

	// Pick if we can do the fast or slow way.
	if ((matched_format || page_aligned_rect || block_matched_format) && width_okay && sbp == target_bp)
	{
		in_rect = in_rect.rintersect(t->m_valid);

		if (!in_rect.rempty())
			AddDirtyRectTarget(t, in_rect, t->m_TEX0.PSM, t->m_TEX0.TBW, rgba);
	}
	else // Slow way.
	{
		const int page_draw = (std::max(src_info->pgs.x, in_rect.width()) + (src_info->pgs.x - 1)) / src_info->pgs.x;
		const int page_skip = src_pg_width - page_draw;
		const int vertical_pages = (std::max(src_info->pgs.y, in_rect.height()) / src_info->pgs.y);
		const int horisontal_pages = (std::max(src_info->pgs.x, in_rect.width()) / src_info->pgs.x);
		const int totalpages = vertical_pages * horisontal_pages + (page_skip * (vertical_pages-1));
		const bool single_width = page_draw == 1;

		// We can use the offset here for the X to pick the right function.
		if (block_offset || req_depth_offset)
		{
			const int block_x = in_rect.x & (src_info->pgs.x - 1);
			const int block_y = in_rect.y & (src_info->pgs.y - 1);
			x_offset += block_x;
			y_offset += block_y;
			if(block_x)
				in_rect = GSVector4i(in_rect.x - block_x, in_rect.y, in_rect.z - block_x, in_rect.w);
			if (block_y)
				in_rect = GSVector4i(in_rect.x, in_rect.y - block_y, in_rect.z, in_rect.w - block_y);
		}
		// Managed to page align everything, easier to deal with.
		// If sbp is offset, make sure the target_bp is too, that means we've already dealt with the offset.
		if ((!(sbp & 31) || sbp == target_bp) && !x_offset && (in_rect.z <= src_width || (in_rect.z % src_width) == 0) &&
			(!(src_width & (src_info->pgs.x - 1)) || totalpages == 1) && !(in_rect.x & (src_info->pgs.x - 1)))
		{
			//const int start_page = (in_rect.x / src_info->pgs.x) + ((in_rect.y / src_info->pgs.y) * src_pg_width);
			const int end_page = start_page + totalpages;
			const int width = ((totalpages == 1 || single_width) && matched_format) ? in_rect.width() : dst_info->pgs.x;
			const int height = (totalpages == 1 && matched_format) ? in_rect.height() : dst_info->pgs.y;
			GSVector4i new_rect = GSVector4i::zero();

			int drawn = 0;
			for (int page = start_page; page < end_page; page++)
			{
				new_rect.x = x_offset + (page % dst_pg_width) * dst_info->pgs.x;
				new_rect.z = new_rect.x + width;
				new_rect.y = y_offset + (page / dst_pg_width) * dst_info->pgs.y;
				new_rect.w = new_rect.y + height;
				new_rect = new_rect.rintersect(t->m_valid);

				if (!new_rect.rempty())
					AddDirtyRectTarget(t, new_rect, t->m_TEX0.PSM, t->m_TEX0.TBW, rgba);

				drawn++;
				if (drawn == page_draw)
				{
					drawn = 0;
					page += page_skip;
				}
			}
		}
		else
		{
			if (in_rect.x & (dst_info->pgs.x - 1))
			{
				const u32 rect_off = (in_rect.x & (dst_info->pgs.x-1));
				in_rect.x -= rect_off;
				in_rect.z -= rect_off;
				x_offset += rect_off;
			}
			if (in_rect.y & (dst_info->pgs.y - 1))
			{
				const u32 rect_off = (in_rect.y & (dst_info->pgs.y - 1));
				in_rect.y -= rect_off;
				in_rect.w -= rect_off;
				y_offset += rect_off;
			}

			const int offset_pages = (x_offset / dst_info->pgs.x);
			const int new_start_page = offset_pages + start_page;
			const int end_page = new_start_page + totalpages;
			const int width = ((totalpages == 1 || single_width) && matched_format) ? in_rect.width() : dst_info->pgs.x;
			const int height = (totalpages == 1 && matched_format) ? in_rect.height() : dst_info->pgs.y;

			GSVector4i new_rect = GSVector4i::zero();
			x_offset -= offset_pages * dst_info->pgs.x;

			int drawn = 0;
			for (int page = new_start_page; page < end_page; page++)
			{
				int overflow = 0;
				new_rect.x = x_offset + (page % dst_pg_width) * dst_info->pgs.x;

				int x_end_pos = new_rect.x + width;
				if (x_end_pos > dst_width)
				{
					overflow = x_end_pos - dst_width;
					x_end_pos = dst_width;
				}
				new_rect.z = std::min(x_end_pos, dst_width);
				new_rect.y = y_offset + (page / dst_pg_width) * dst_info->pgs.y;
				new_rect.w = new_rect.y + height;
				new_rect = new_rect.rintersect(t->m_valid);

				if (!new_rect.rempty())
					AddDirtyRectTarget(t, new_rect, t->m_TEX0.PSM, t->m_TEX0.TBW, rgba);

				if (overflow)
				{
					new_rect.x = 0;
					new_rect.z = overflow;
					new_rect.y = ((page + 1) / dst_pg_width) * dst_info->pgs.y;
					new_rect.w = new_rect.y + height;
					new_rect = new_rect.rintersect(t->m_valid);

					if (!new_rect.rempty())
						AddDirtyRectTarget(t, new_rect, t->m_TEX0.PSM, t->m_TEX0.TBW, rgba);
				}

				drawn++;
				if (drawn == page_draw)
				{
					drawn = 0;
					page += page_skip;
				}
			}
		}
	}
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

GSTextureCache::Source* GSTextureCache::LookupDepthSource(const bool is_depth, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, const GIFRegCLAMP& CLAMP, const GSVector4i& r, const bool possible_shuffle, const bool linear, const u32 frame_fbp, bool req_color, bool req_alpha, bool palette)
{
	if (GSConfig.UserHacks_DisableDepthSupport)
	{
		GL_CACHE("LookupDepthSource not supported (0x%x, F:0x%x)", TEX0.TBP0, TEX0.PSM);
		return nullptr;
	}

	GL_CACHE("TC: Lookup Depth Source <%d,%d => %d,%d> (0x%x, %s, BW: %u, CBP: 0x%x, TW: %d, TH: %d)", r.x, r.y, r.z,
		r.w, TEX0.TBP0, psm_str(TEX0.PSM), TEX0.TBW, TEX0.CBP, 1 << TEX0.TW, 1 << TEX0.TH);

	const SourceRegion region = SourceRegion::Create(TEX0, CLAMP);
	const GSLocalMemory::psm_t& psm_s = GSLocalMemory::m_psm[TEX0.PSM];
	// Yes, this can get called with color PSMs that have palettes
	const u32* const clut = g_gs_renderer->m_mem.m_clut;
	GSTexture* const gpu_clut = (psm_s.pal > 0) ? g_gs_renderer->m_mem.m_clut.GetGPUTexture() : nullptr;
	Source* src = FindSourceInMap(TEX0, TEXA, psm_s, clut, gpu_clut, GSVector2i(0, 0), region,
		region.IsFixedTEX0(TEX0), m_src.m_map[TEX0.TBP0 >> 5]);
	if (src)
	{
		GL_CACHE("TC: src hit: (0x%x, %s)", TEX0.TBP0, psm_str(TEX0.PSM));
		src->Update(r);
		return src;
	}

	Target* dst = nullptr;

	// Check only current frame, I guess it is only used as a postprocessing effect
	const u32 bp = TEX0.TBP0;
	const u32 psm = TEX0.PSM;
	bool inside_target = false;
	GSVector4i target_rc;

	for (auto t : m_dst[DepthStencil])
	{
		if (!t->m_used || (!t->m_dirty.empty() && !is_depth))
			continue;
		
		if (GSUtil::HasSharedBits(bp, psm, t->m_TEX0.TBP0, t->m_TEX0.PSM))
		{
			pxAssert(GSLocalMemory::m_psm[t->m_TEX0.PSM].depth);
			if (t->m_age == 0)
			{
				// Perfect Match
				dst = t;
				inside_target = false;
				break;
			}
			else if (t->m_age == 1)
			{
				// Better than nothing (Full Spectrum Warrior)
				dst = t;
				inside_target = false;
			}
		}
		else if (!dst && bp >= t->m_TEX0.TBP0 && bp < t->m_end_block)
		{
			const GSVector2i page_size = GSLocalMemory::m_psm[t->m_TEX0.PSM].pgs;
			const bool can_translate = CanTranslate(bp, TEX0.TBW, psm, r, t->m_TEX0.TBP0, t->m_TEX0.PSM, t->m_TEX0.TBW);
			const bool swizzle_match = psm_s.depth == GSLocalMemory::m_psm[t->m_TEX0.PSM].depth;
			GSVector4i new_rect = r;

			if (linear)
			{
				new_rect.z -= 1;
				new_rect.w -= 1;
			}

			if (can_translate)
			{
				if (swizzle_match)
				{
					target_rc = TranslateAlignedRectByPage(t, bp, psm, TEX0.TBW, new_rect);
				}
				else
				{
					// If it's not page aligned, grab the whole pages it covers, to be safe.
					if (psm_s.bpp != GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp)
					{
						const GSVector2i dst_page_size = psm_s.pgs;
						target_rc = GSVector4i(target_rc.x / page_size.x, target_rc.y / page_size.y,
							(target_rc.z + (page_size.x - 1)) / page_size.x,
							(target_rc.w + (page_size.y - 1)) / page_size.y);
						target_rc = GSVector4i(target_rc.x * dst_page_size.x, target_rc.y * dst_page_size.y,
							target_rc.z * dst_page_size.x, target_rc.w * dst_page_size.y);
					}
					else
					{
						target_rc.x &= ~(page_size.x - 1);
						target_rc.y &= ~(page_size.y - 1);
						target_rc.z = (new_rect.z + (page_size.x - 1)) & ~(page_size.x - 1);
						target_rc.w = (new_rect.w + (page_size.y - 1)) & ~(page_size.y - 1);
					}
					target_rc = TranslateAlignedRectByPage(t, bp & ~((1 << 5) - 1), psm, TEX0.TBW, target_rc);
				}

				if (!target_rc.rempty())
				{
					dst = t;
					inside_target = true;
				}
			}
			if (linear)
			{
				new_rect.z += 1;
				new_rect.w += 1;
			}
		}
	}

	if (dst && psm_s.trbpp != 24 && !dst->HasValidAlpha())
	{
		for (Target* t : m_dst[RenderTarget])
		{
			if (t->m_age <= 1 && t->m_TEX0.TBP0 == bp && t->m_TEX0.TBW == TEX0.TBW && t->HasValidAlpha())
			{
				GL_CACHE("TC depth: Using RT %x instead of depth because of missing alpha", t->m_TEX0.TBP0);

				// Have to update here, because this is a source, it won't Update().
				if (FullRectDirty(t, 0x7))
					t->Update();
				else if (!t->m_valid_rgb)
					CopyRGBFromDepthToColor(t, dst);

				dst = t;
				inside_target = false;
				break;
			}
		}
	}

	if (!dst && is_depth)
	{
		// Retry on the render target (Silent Hill 4)
		for (auto t : m_dst[RenderTarget])
		{
			// FIXME: do I need to allow m_age == 1 as a potential match (as DepthStencil) ???
			if (t->m_age <= 1 && t->m_used && t->m_dirty.empty() && GSUtil::HasSharedBits(bp, psm, t->m_TEX0.TBP0, t->m_TEX0.PSM))
			{
				pxAssert(GSLocalMemory::m_psm[t->m_TEX0.PSM].depth);
				dst = t;
				inside_target = false;
				break;
			}
		}
	}

	if (dst)
	{
		GL_CACHE("TC depth: dst %s hit: (0x%x, %s)", to_string(dst->m_type),
			TEX0.TBP0, psm_str(psm));

		// Create a shared texture source
		src = new Source(TEX0, TEXA);
		src->m_texture = dst->m_texture;
		src->m_scale = dst->m_scale;
		src->m_unscaled_size = dst->m_unscaled_size;
		src->m_shared_texture = true;
		src->m_target = true; // So renderer can check if a conversion is required
		src->m_from_target = dst; // avoid complex condition on the renderer
		src->m_from_target_TEX0 = dst->m_TEX0;
		src->m_32_bits_fmt = dst->m_32_bits_fmt;
		src->m_valid_rect = dst->m_valid;
		src->m_end_block = dst->m_end_block;

		if (inside_target)
		{
			// Need to set it up as a region target.
			src->m_region.SetX(target_rc.x, target_rc.z);
			src->m_region.SetY(target_rc.y, target_rc.w);
		}

		if (GSRendererHW::GetInstance()->IsTBPFrameOrZ(dst->m_TEX0.TBP0))
		{
			m_temporary_source = src;
		}
		else
		{
			src->SetPages();
			m_src.Add(src, TEX0);
		}

		if (palette)
		{
			AttachPaletteToSource(src, psm_s.pal, true);
		}
	}
	else
	{
		// This is a bit of a worry, since it could load junk from local memory... but it's better than skipping the draw.
		return is_depth ? LookupSource(false, TEX0, TEXA, CLAMP, r, nullptr, possible_shuffle, linear, frame_fbp, req_color, req_alpha) : nullptr;
	}

	pxAssert(src->m_texture);
	pxAssert(src->m_scale == (dst ? dst->m_scale : 1.0f));

	return src;
}

GSTextureCache::Source* GSTextureCache::LookupSource(const bool is_color, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, const GIFRegCLAMP& CLAMP, const GSVector4i& r, const GSVector2i* lod, const bool possible_shuffle, const bool linear, const u32 frame_fbp, bool req_color, bool req_alpha)
{
	GL_CACHE("TC: Lookup Source <%d,%d => %d,%d> (0x%x, %s, BW: %u, CBP: 0x%x, TW: %d, TH: %d)", r.x, r.y, r.z, r.w, TEX0.TBP0, psm_str(TEX0.PSM), TEX0.TBW, TEX0.CBP, 1 << TEX0.TW, 1 << TEX0.TH);

	const GSLocalMemory::psm_t& psm_s = GSLocalMemory::m_psm[TEX0.PSM];
	//const GSLocalMemory::psm_t& cpsm = psm.pal > 0 ? GSLocalMemory::m_psm[TEX0.CPSM] : psm;

	const u32* const clut = g_gs_renderer->m_mem.m_clut;
	GSTexture* const gpu_clut = (psm_s.pal > 0) ? g_gs_renderer->m_mem.m_clut.GetGPUTexture() : nullptr;

	const SourceRegion region = SourceRegion::Create(TEX0, CLAMP);

	// Prevent everything going to rubbish if a game somehow sends a TW/TH above 10, and region isn't being used.
	if ((TEX0.TW > 10 && !region.HasX()) || (TEX0.TH > 10 && !region.HasY()))
	{
		GL_CACHE("Invalid TEX0 size %ux%u without region, aborting draw.", TEX0.TW, TEX0.TH);
		return nullptr;
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

	if (src && src->m_from_target && GSConfig.UserHacks_TextureInsideRt >= GSTextureInRtMode::MergeTargets && GSLocalMemory::GetUnwrappedEndBlockAddress(TEX0.TBP0, TEX0.TBW, TEX0.PSM, r) > src->m_from_target->m_end_block)
	{
		m_src.RemoveAt(src);
		src = nullptr;
	}

	Target* dst = nullptr;
	bool half_right = false;
	int x_offset = 0;
	int y_offset = 0;

#ifdef DISABLE_HW_TEXTURE_CACHE
	if (0)
#else
	if (!src && (GSConfig.UserHacks_CPUFBConversion || TEX0.PSM != PSMT4))
#endif
	{
		const u32 bp = TEX0.TBP0;
		const u32 psm = TEX0.PSM;
		const u32 bw = TEX0.TBW;

		GSVector4i req_rect = r;

		// The read area might be offset but the start of the texture is at the beginning of the space.
		req_rect.x = region.HasX() ? region.GetMinX() : 0;
		req_rect.y = region.HasY() ? region.GetMinY() : 0;

		// Arc the Lad finds the wrong surface here when looking for a depth stencil.
		// Since we're currently not caching depth stencils (check ToDo in CreateSource) we should not look for it here.

		// (Simply not doing this code at all makes a lot of previsouly missing stuff show (but breaks pretty much everything
		// else.)

		bool found_t = false;
		bool tex_merge_rt = false;
		auto& list = m_dst[RenderTarget];
		for (auto i = list.begin(); i != list.end(); ++i)
		{
			Target* t = *i;
			// Make sure it is page aligned, otherwise things get messy with the pixel order (Tomb Raider Legend).
			if (t->m_used)
			{
				// If the BP is block offset it's unlikely to be a target, but a target can be made by a HW move, so we need to check for a match.
				// Good for Baldurs Gate Dark Alliance (HW Move), bad for Tomb Raider Legends (just offset).
				if (((bp & (BLOCKS_PER_PAGE - 1)) != (t->m_TEX0.TBP0 & (BLOCKS_PER_PAGE - 1))) && (bp & (BLOCKS_PER_PAGE - 1)))
					continue;

				const bool overlaps = t->Overlaps(bp, bw, psm, req_rect);

				// Try to make sure the target has available what we need, be careful of self referencing frames with font in the alpha.
				if (!overlaps)
					continue;

				const bool width_match = (std::max(64U, bw * 64U) >> GSLocalMemory::m_psm[psm].info.pageShiftX()) ==
					(std::max(64U, t->m_TEX0.TBW * 64U) >> GSLocalMemory::m_psm[t->m_TEX0.PSM].info.pageShiftX());

				if (bp == t->m_TEX0.TBP0 && !t->m_dirty.empty() && overlaps && GSUtil::GetChannelMask(psm) == GSUtil::GetChannelMask(t->m_TEX0.PSM) && GSRendererHW::GetInstance()->m_draw_transfers.size() > 0)
				{
					bool can_use = true;

					if (!possible_shuffle && !(GSLocalMemory::m_psm[TEX0.PSM].bpp == 16 && GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp == 32) && !width_match && t->m_dirty.size() >= 1 && !t->m_dirty.GetTotalRect(t->m_TEX0, t->m_unscaled_size).eq(t->m_valid))
					{
						std::vector<GSState::GSUploadQueue>::reverse_iterator iter;

						const int start_draw = GSRendererHW::GetInstance()->m_draw_transfers.back().draw;

						for (iter = GSRendererHW::GetInstance()->m_draw_transfers.rbegin(); iter != GSRendererHW::GetInstance()->m_draw_transfers.rend(); )
						{
							if (TEX0.TBP0 == iter->blit.DBP && GSUtil::HasCompatibleBits(iter->blit.DPSM, TEX0.PSM) && req_rect.rintersect(iter->rect).eq(req_rect))
							{
								can_use = false;
								break;
							}

							// Give up after checking recent draws
							if (start_draw - iter->draw > 0)
								break;

							++iter;
						}
					}

					if (!can_use)
					{
						InvalidateSourcesFromTarget(t);
						i = list.erase(i);
						delete t;
						continue;
					}
				}

				// Typical bug (MGS3 blue cloud):
				// 1/ RT used as 32 bits => alpha channel written
				// 2/ RT used as 24 bits => no update of alpha channel
				// 3/ Lookup of texture that used alpha channel as index, HasSharedBits will return false
				//    because of the previous draw call format
				//
				// Solution: consider the RT as 32 bits if the alpha was used in the past
				// We can render to the target as C32, but mask alpha, in which case, pretend like it doesn't have any.
				const u32 t_psm = t->HasValidAlpha() ? t->m_TEX0.PSM & ~0x1 : ((t->m_TEX0.PSM == PSMCT32) ? PSMCT24 : t->m_TEX0.PSM);
				bool rect_clean = GSUtil::HasSameSwizzleBits(psm, t_psm) || (t_psm == PSMCT32 && GSLocalMemory::m_psm[psm].bpp == 16 && possible_shuffle);
				const bool tex_overlaps = bp >= t->m_TEX0.TBP0 && bp < t->UnwrappedEndBlock();
				const bool real_fmt_match = (GSLocalMemory::m_psm[psm].trbpp == 16) == (t->m_32_bits_fmt == false);
				if (rect_clean && tex_overlaps && !t->m_dirty.empty() && width_match)
				{
					GSVector4i new_rect = req_rect;

					if (linear)
					{
						new_rect.z -= 1;
						new_rect.w -= 1;
					}

					bool partial = false;
					// If it's compatible and page aligned, then handle it this way.
					// It's quicker, and Surface Offsets can get it wrong.
					// Example doing PSMT8H to C32, BP 0x1c80, TBP 0x1d80, incoming rect 0,128 -> 128,256
					// Surface offsets translates it to 0, 128 -> 128, 128, not 0, 0 -> 128, 128.
					if (bp > t->m_TEX0.TBP0)
					{
						const GSVector2i page_size = GSLocalMemory::m_psm[t->m_TEX0.PSM].pgs;
						const bool can_translate = CanTranslate(bp, bw, psm, new_rect, t->m_TEX0.TBP0, t->m_TEX0.PSM, t->m_TEX0.TBW);
						const bool swizzle_match = GSLocalMemory::m_psm[psm].depth == GSLocalMemory::m_psm[t->m_TEX0.PSM].depth;

						if (can_translate)
						{
							if (swizzle_match)
							{
								new_rect = TranslateAlignedRectByPage(t, bp, psm, bw, new_rect);
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
									new_rect.z = (new_rect.z + (page_size.x - 1)) & ~(page_size.x - 1);
									new_rect.w = (new_rect.w + (page_size.y - 1)) & ~(page_size.y - 1);
								}
								new_rect = TranslateAlignedRectByPage(t, bp & ~((1 << 5) - 1), psm, bw, new_rect);
							}

							rect_clean = !new_rect.eq(GSVector4i::zero());
						}
						else
						{
							SurfaceOffsetKey sok;
							sok.elems[0].bp = bp;
							sok.elems[0].bw = bw;
							sok.elems[0].psm = psm;
							sok.elems[0].rect = new_rect;
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

					if (rect_clean)
					{
						for (auto& dirty : t->m_dirty)
						{
							GSVector4i dirty_rect = dirty.GetDirtyRect(t->m_TEX0, true);
							if (!dirty_rect.rintersect(new_rect).rempty())
							{
								rect_clean = false;
								partial |= !new_rect.rintersect(dirty_rect).eq(new_rect);
								break;
							}
						}
					}

					const u32 channel_mask = GSUtil::GetChannelMask(psm);
					const u32 channels = t->m_dirty.GetDirtyChannels() & channel_mask;

					// If the source is reading the rt, make sure it's big enough.
					if (!possible_shuffle && t && GSUtil::HasCompatibleBits(psm, t->m_TEX0.PSM) && real_fmt_match)
					{
						if (t->Overlaps(bp, bw, psm, new_rect))
							ResizeTarget(t, new_rect, bp, psm, bw);
					}
					// If not all channels are clean/dirty or only part of the rect is dirty, we need to update the target.
					if (((channels & channel_mask) != channel_mask || partial))
						t->Update();

					if (linear)
					{
						new_rect.z += 1;
						new_rect.w += 1;
					}
				}
				else
				{
					rect_clean = t->m_dirty.empty();
					if (!possible_shuffle && frame_fbp != t->m_TEX0.TBP0 && rect_clean && bp == t->m_TEX0.TBP0 && t && GSUtil::HasCompatibleBits(psm, t->m_TEX0.PSM) && width_match && real_fmt_match)
					{
						if (!tex_merge_rt && t->Overlaps(bp, bw, psm, req_rect))
							ResizeTarget(t, req_rect, bp, psm, bw);
					}
				}

				if (t->m_TEX0.TBP0 != frame_fbp && !possible_shuffle && bp > t->m_TEX0.TBP0 && t->Overlaps(bp, bw, psm, req_rect) && GSUtil::GetChannelMask(psm) == GSUtil::GetChannelMask(t->m_TEX0.PSM) && !width_match)
				{
					GSVector4i new_rect = req_rect;

					if (linear)
					{
						new_rect.z -= 1;
						new_rect.w -= 1;
					}

					const GSLocalMemory::psm_t* src_info = &GSLocalMemory::m_psm[psm];
					const int  block_offset = static_cast<int>(bp) - static_cast<int>(t->m_TEX0.TBP0);
					const int page_offset = (block_offset) >> 5;
					const int start_page = page_offset + (new_rect.x / src_info->pgs.x) + ((new_rect.y / src_info->pgs.y) * std::max(static_cast<int>(bw), 1));
					const int src_page_width = std::max(static_cast<int>((bw * 64) / src_info->pgs.x), 1);
					const int dst_page_width = std::max(static_cast<int>((t->m_TEX0.TBW * 64) / GSLocalMemory::m_psm[t->m_TEX0.PSM].pgs.x), 1);
					if (((start_page % dst_page_width) + src_page_width) > dst_page_width)
					{
						const u32 read_start = GSLocalMemory::GetStartBlockAddress(bp, bw, psm, new_rect);
						const u32 read_end = GSLocalMemory::GetEndBlockAddress(bp, bw, psm, new_rect);
					
						if (read_start > t->m_TEX0.TBP0 && read_end < t->m_end_block)
						{
							// Probably a bad overlapping target.
							InvalidateSourcesFromTarget(t);
							i = list.erase(i);
							delete t;
							continue;
						}
					}
				}

				bool overlapping_dirty = true;

				if (!rect_clean)
				{
					const u32 read_start = GSLocalMemory::GetStartBlockAddress(bp, bw, psm, req_rect);
					const u32 read_end = GSLocalMemory::GetUnwrappedEndBlockAddress(bp, bw, psm, req_rect);
					const GSVector4i dirty_rect = t->m_dirty.GetTotalRect(t->m_TEX0, t->m_unscaled_size);
					const u32 dirty_start = GSLocalMemory::GetStartBlockAddress(t->m_TEX0.TBP0, t->m_TEX0.TBW, t->m_TEX0.PSM, dirty_rect);
					const u32 dirty_end = GSLocalMemory::GetUnwrappedEndBlockAddress(t->m_TEX0.TBP0, t->m_TEX0.TBW, t->m_TEX0.PSM, dirty_rect);

					overlapping_dirty = read_start <= dirty_end && read_end >= dirty_start;
				}

				const bool t_clean = ((t->m_dirty.GetDirtyChannels() & GSUtil::GetChannelMask(psm)) == 0) || rect_clean;
				const u32 color_psm = ((psm & 0x30) == 0x30) ? (psm & ~0x30) : psm;
				const u32 tex_color_psm = ((t->m_TEX0.PSM & 0x30) == 0x30) ? (t->m_TEX0.PSM & ~0x30) : t->m_TEX0.PSM;
				const bool can_convert = (GSUtil::HasCompatibleBits(psm, t_psm) && ((bw == t->m_TEX0.TBW) || (bw <= 1 && req_rect.w < GSLocalMemory::m_psm[psm].pgs.y))) ||
										(possible_shuffle && ((bw == t->m_TEX0.TBW) || (bw == (t->m_TEX0.TBW * 2) || bw <= 2)) && GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp == 32);

				// Match if we haven't already got a tex in rt
				if (((!t_clean && can_convert) || t_clean || !overlapping_dirty) && GSUtil::HasSharedBits(bp, psm, t->m_TEX0.TBP0, t_psm))
				{
					bool match = true;
					if (found_t && (bw != t->m_TEX0.TBW || t->m_TEX0.PSM != psm))
						match = false;

					//if (!t_clean && can_convert)
					//	DevCon.Warning("Expected %x Got %x shuffle %d draw %d", psm, t_psm, possible_shuffle, GSState::s_n);
					if (match)
					{
						// It is a complex to convert the code in shader. As a reference, let's do it on the CPU, it will be slow but
						// 1/ it just works :)
						// 2/ even with upscaling
						// 3/ for both Direct3D and OpenGL
						if (psm == PSMT4 || (GSConfig.UserHacks_CPUFBConversion && psm == PSMT8))
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
						{
							// We don't have a shader for this.
							if (!possible_shuffle && TEX0.PSM == PSMT8 && GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp != 32)
							{
								continue;
							}
							else
							{
								if ((psm == PSMT4 || psm == PSMT8) && t->m_was_dst_matched && !t->m_valid_rgb)
								{

									GL_CACHE("TC: Attempt to repopulate RGB for target [%x] on source lookup", t->m_TEX0.TBP0);
									for (Target* dst_match : m_dst[DepthStencil])
									{
										if (dst_match->m_TEX0.TBP0 != t->m_TEX0.TBP0 || !dst_match->m_valid_rgb)
											continue;
										
										if (!CopyRGBFromDepthToColor(t, dst_match))
										{
											// If we can't update it, then just read back the valid data.
											DevCon.Warning("Failed to update dst matched texture");
										}
										t->m_valid_rgb = true;
										break;
									}

								}

								if (overlaps && !t->HasValidBitsForFormat(psm, req_color, req_alpha) && !(possible_shuffle && GSLocalMemory::m_psm[psm].bpp == 16 && GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp == 32))
									continue;

								dst = t;

								found_t = true;
								x_offset = 0;
								y_offset = 0;

								if (GSConfig.UserHacks_TextureInsideRt >= GSTextureInRtMode::MergeTargets && GSLocalMemory::GetUnwrappedEndBlockAddress(bp, bw, psm, req_rect) > dst->m_end_block)
									continue;
								else
								{
									tex_merge_rt = false;
									break;
								}
							}
						}
					}
				}
				else if ((t->m_TEX0.TBW >= 16) && GSUtil::HasSharedBits(bp, psm, t->m_TEX0.TBP0 + t->m_TEX0.TBW * 0x10, t->m_TEX0.PSM))
				{
					// Detect half of the render target (fix snow engine game)
					// Target Page (8KB) have always a width of 64 pixels
					// Half of the Target is TBW/2 pages * 8KB / (1 block * 256B) = 0x10
					if (overlaps && !t->HasValidBitsForFormat(psm, req_color, req_alpha) && !(possible_shuffle && GSLocalMemory::m_psm[psm].bpp == 16 && GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp == 32))
						continue;

					half_right = true;
					dst = t;
					found_t = true;
					tex_merge_rt = false;
					x_offset = 0;
					y_offset = 0;
					break;
				}
				// Make sure the texture actually is INSIDE the RT, it's possibly not valid if it isn't.
				// Also check BP >= TBP, create source isn't equpped to expand it backwards and all data comes from the target. (GH3)
				else if (GSConfig.UserHacks_TextureInsideRt >= GSTextureInRtMode::InsideTargets && color_psm >= PSMCT32 &&
					color_psm <= PSMCT16S && (GSUtil::HasCompatibleBits(tex_color_psm, color_psm) ||
					(possible_shuffle && tex_color_psm <= PSMCT24 && ((((t->UnwrappedEndBlock() + 1) - t->m_TEX0.TBP0) >> 1) + t->m_TEX0.TBP0) == bp)) &&
					(t->Overlaps(bp, bw, psm, req_rect) || t->Wraps()) &&
					t->m_age <= 1 && (!found_t || dst->m_TEX0.TBW < bw))
				{
					if (overlaps && !t->HasValidBitsForFormat(psm, req_color, req_alpha) && !(possible_shuffle && GSLocalMemory::m_psm[psm].bpp == 16 && GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp == 32))
						continue;

					// PSM equality needed because CreateSource does not handle PSM conversion.
					// Only inclusive hit to limit false hits.
					GSVector4i rect = req_rect;
					int src_bw = bw;
					int src_psm = psm;

					// If the input is C16 and it's actually a shuffle of 32bits we need to correct the size.
					if ((tex_color_psm & 0xF) <= PSMCT24 && (psm & 0x7) == PSMCT16 && possible_shuffle)
					{
						src_psm = t->m_TEX0.PSM;
						// If it's taking double width for the shuffle, half that.
						if (src_bw == (t->m_TEX0.TBW * 2))
						{
							src_bw = t->m_TEX0.TBW;

							rect.x /= 2;
							rect.z /= 2;
						}
						else
						{
							rect.y /= 2;
							rect.w /= 2;
						}
					}
					if (bp > t->m_TEX0.TBP0)
					{
						GSVector4i new_rect = rect;
						if (linear)
						{
							new_rect.z -= 1;
							new_rect.w -= 1;
						}
						// Check if it is possible to hit with valid <x,y> offset on the given Target.
						// Fixes Jak eyes rendering.
						// Fixes Xenosaga 3 last dungeon graphic bug.
						// Fixes Pause menu in The Getaway.
						const bool can_translate = CanTranslate(bp, bw, src_psm, new_rect, t->m_TEX0.TBP0, t->m_TEX0.PSM, t->m_TEX0.TBW);
						if (can_translate)
						{
							const bool swizzle_match = GSLocalMemory::m_psm[src_psm].depth == GSLocalMemory::m_psm[t->m_TEX0.PSM].depth;
							const GSVector2i& page_size = GSLocalMemory::m_psm[t->m_TEX0.PSM].pgs;
							const GSVector4i page_mask(GSVector4i((page_size.x - 1), (page_size.y - 1)).xyxy());
							rect = new_rect & ~page_mask;

							if (swizzle_match)
							{
								rect = TranslateAlignedRectByPage(t, bp, src_psm, bw, new_rect);
								rect.x -= new_rect.x;
								rect.y -= new_rect.y;
							}
							else
							{
								// If it's not page aligned, grab the whole pages it covers, to be safe.
								if (GSLocalMemory::m_psm[psm].bpp != GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp)
								{
									const GSVector2i& dst_page_size = GSLocalMemory::m_psm[t->m_TEX0.PSM].pgs;
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
								rect = TranslateAlignedRectByPage(t, bp & ~((1 << 5) - 1), src_psm, bw, new_rect);
								rect.x -= new_rect.x & ~(page_size.x - 1);
								rect.y -= new_rect.y & ~(page_size.y - 1);
							}

							rect = rect.rintersect(t->m_valid);

							if (rect.rempty())
								continue;

							if (!t->m_dirty.empty())
							{
								const GSVector4i dirty_rect = t->m_dirty.GetTotalRect(t->m_TEX0, GSVector2i(rect.z, rect.w)).rintersect(rect);
								if (dirty_rect.eq(rect))
									continue;
							}

							x_offset = rect.x;
							y_offset = rect.y;
							dst = t;
							tex_merge_rt = false;
							found_t = true;
							if (dst->m_TEX0.TBP0 == frame_fbp && possible_shuffle)
								break;
							else
								continue;
						}
						else
						{
							SurfaceOffset so = ComputeSurfaceOffset(bp, bw, src_psm, new_rect, t);
							if (!so.is_valid && t->Wraps())
							{
								// Improves Beyond Good & Evil shadow.
								const u32 bp_unwrap = bp + GSTextureCache::MAX_BP + 0x1;
								so = ComputeSurfaceOffset(bp_unwrap, bw, src_psm, new_rect, t);
							}
							if (so.is_valid)
							{
								dst = t;
								// Offset from Target to Source in Target coords.
								x_offset = so.b2a_offset.x;
								y_offset = so.b2a_offset.y;
								tex_merge_rt = false;
								found_t = true;
								// Keep looking, just in case there is an exact match (Situation: Target frame drawn inside target frame, current makes a separate texture)
								if (dst->m_TEX0.TBP0 == frame_fbp && possible_shuffle)
									break;
								else
									continue;
							}
						}
					}
					else
					{
						// Some games, such as Tomb Raider: Underworld, and Destroy All Humans shift the texture pointer
						// back behind the framebuffer, but then offset their texture coordinates to compensate. Why they
						// do this, I have no idea... but it's usually only a page wide/high of an offset. Thankfully,
						// they also seem to set region clamp with the offset as well, so we can use that to find the target
						// that they're expecting to read from. Example from Tomb Raider: TBP0 0xee0 with a TBW of 8, MINU/V
						// of 64,32, which means 9 pages down, or 0x1000, which lines up with the main framebuffer. Originally
						// I had a check on the end block too, but since the game's a bit rude, it channel shuffles into new
						// targets that it never regularly draws to, which means the end block for them won't be correct.
						// Omitting that check here seemed less risky than blowing CS targets out...
						const GSVector2i& page_size = GSLocalMemory::m_psm[src_psm].pgs;
						const GSOffset offset(GSLocalMemory::m_psm[src_psm].info, bp, bw, psm);
						if (bp < t->m_TEX0.TBP0 && region.HasX() && region.HasY() &&
							(region.GetMinX() & (page_size.x - 1)) == 0 && (region.GetMinY() & (page_size.y - 1)) == 0 &&
							offset.bn(region.GetMinX(), region.GetMinY()) == t->m_TEX0.TBP0)
						{
							GL_CACHE("TC: Target 0x%x detected in front of TBP 0x%x with %d,%d offset (%d pages)",
								t->m_TEX0.TBP0, TEX0.TBP0, region.GetMinX(), region.GetMinY(),
								(region.GetMinY() / page_size.y) * TEX0.TBW + (region.GetMinX() / page_size.x));
							x_offset = -region.GetMinX();
							y_offset = -region.GetMinY();
							dst = t;
							tex_merge_rt = false;
							found_t = true;

							// Catwoman offsets the RT as well as the texture.
							if (GSRendererHW::GetInstance()->GetCachedCtx()->FRAME.Block() == TEX0.TBP0)
							{
								// Should be page aligned.
								pxAssert(((t->m_TEX0.TBP0 - TEX0.TBP0) % 32) == 0);
								const s32 page_offset = (t->m_TEX0.TBP0 - TEX0.TBP0) >> 5;
								GL_CACHE("TC: RT also in front of TBP, offsetting draw by %d pages and [%d,%d].", page_offset, x_offset, y_offset);
								GSRendererHW::GetInstance()->OffsetDraw(page_offset, page_offset, x_offset, y_offset);
								break;
							}

							if (dst->m_TEX0.TBP0 == frame_fbp && possible_shuffle)
								break;
							else
								continue;
						}

						// Strictly speaking this path is no longer needed, but I'm leaving it here for now because Guitar
						// Hero III needs it to merge crowd textures.
						else if (GSConfig.UserHacks_TextureInsideRt >= GSTextureInRtMode::MergeTargets && !tex_merge_rt)
						{
							dst = t;
							x_offset = 0;
							y_offset = 0;
							tex_merge_rt = true;

							// Prefer a target inside over a target outside.
							found_t = false;
							if (dst->m_TEX0.TBP0 == frame_fbp && possible_shuffle)
								break;
							else
								continue;
						}
					}
				}
			}
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
			GSVector4i new_rect = req_rect;

			// Just in case the TextureMinMax trolls us as it does, when checking if inside the target.
			new_rect.z -= 2;
			new_rect.w -= 2;

			// Let's try a trick to avoid to use wrongly a depth buffer
			// Unfortunately, I don't have any Arc the Lad testcase
			//
			// 1/ Check only current frame, I guess it is only used as a postprocessing effect
			if (is_color)
			{
				for (auto t : m_dst[DepthStencil])
				{
					if (t->m_age <= 1 && t->m_used && t->m_dirty.empty() && GSUtil::HasSharedBits(bp, psm, t->m_TEX0.TBP0, t->m_TEX0.PSM) && t->Inside(bp, bw, psm, new_rect))
					{
						GL_INS("TC: Warning depth format read as color format. Pixels will be scrambled");
						// Let's fetch a depth format texture. Rational, it will avoid the texture allocation and the
						// rescaling of the current function.
						if (psm_s.bpp > 8)
						{
							GIFRegTEX0 depth_TEX0;
							depth_TEX0.U32[0] = TEX0.U32[0] | (0x30u << 20u);
							depth_TEX0.U32[1] = TEX0.U32[1];
							src = LookupDepthSource(false, depth_TEX0, TEXA, CLAMP, req_rect, possible_shuffle, linear, frame_fbp, req_color, req_alpha);

							if (src != nullptr)
							{

								if (TEX0.PSM == PSMT8H)
								{
									// Attach palette for GPU texture conversion
									AttachPaletteToSource(src, psm_s.pal, true);
								}

								return src;
							}
						}
						else
						{
							if (!possible_shuffle && TEX0.PSM == PSMT8 && GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp != 32)
							{
								continue;
							}
							else
							{
								src = LookupDepthSource(false, TEX0, TEXA, CLAMP, req_rect, possible_shuffle, linear, frame_fbp, req_color, req_alpha, true);

								if (src != nullptr)
								{
									if (TEX0.PSM == PSMT8H)
									{
										// Attach palette for GPU texture conversion
										AttachPaletteToSource(src, psm_s.pal, true);
									}

									return src;
								}
							}
						}
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
			GL_CACHE("TC: dst %s hit (%s, OFF <%d,%d>): (0x%x, %s)",
				to_string(dst->m_type),
				half_right ? "half" : "full",
				x_offset,
				y_offset,
				TEX0.TBP0,
				psm_str(TEX0.PSM));
		}
		else
		{
			GL_CACHE("TC: src miss (0x%x, 0x%x, %s)", TEX0.TBP0, psm_s.pal > 0 ? TEX0.CBP : 0, psm_str(TEX0.PSM));
		}
#endif
		src = CreateSource(TEX0, TEXA, dst, half_right, x_offset, y_offset, lod, &r, gpu_clut, region);
		if (!src) [[unlikely]]
			return nullptr;
	}
	else
	{
		GL_CACHE("TC: src hit: (0x%x, 0x%x, %s)",
			TEX0.TBP0, psm_s.pal > 0 ? TEX0.CBP : 0,
			psm_str(TEX0.PSM));

		// If it's from a target, we need to make sure the alpha information is up to date, especially in 16/24 bit formats where it can change draw to draw.
		// Guard against merged targets which don't actually link.
		if (src->m_target && src->m_from_target)
		{
			if ((src->m_TEX0.PSM & 0xf) == PSMCT24)
			{
				src->m_alpha_minmax.first = TEXA.AEM ? 0 : TEXA.TA0;
				src->m_alpha_minmax.second = TEXA.TA0;
			}
			else
			{
				src->m_alpha_minmax.first = src->m_from_target->m_alpha_min;
				src->m_alpha_minmax.second = src->m_from_target->m_alpha_max;

				if (!src->m_32_bits_fmt)
				{
					const bool using_both = (src->m_alpha_minmax.first ^ src->m_alpha_minmax.second) & 128;
					const bool using_ta1 = (src->m_alpha_minmax.second & 128);

					src->m_alpha_minmax.first = TEXA.AEM ? 0 : (using_both ? std::min(TEXA.TA1, TEXA.TA0) : (using_ta1 ? TEXA.TA1 : TEXA.TA0));
					src->m_alpha_minmax.second = (using_both ? std::max(TEXA.TA1, TEXA.TA0) : (using_ta1 ? TEXA.TA1 : TEXA.TA0));
				}
			}
		}

		if (gpu_clut)
			AttachPaletteToSource(src, gpu_clut);
		else if (src->m_palette && (!src->m_palette_obj || !src->ClutMatch({ clut, psm_s.pal })))
			AttachPaletteToSource(src, psm_s.pal, true);
	}

	src->Update(r);
	return src;
}

GSTextureCache::Target* GSTextureCache::FindTargetOverlap(Target* target, int type, int psm)
{
	for (auto t : m_dst[type])
	{
		// Only checks that the texure starts at the requested bp, which shares data. Size isn't considered.
		if (t != target && t->m_TEX0.TBW == target->m_TEX0.TBW && t->m_TEX0.TBP0 >= target->m_TEX0.TBP0 &&
			t->UnwrappedEndBlock() <= target->UnwrappedEndBlock() && GSUtil::HasCompatibleBits(t->m_TEX0.PSM, psm))
			return t;
	}
	return nullptr;
}

GSVector2i GSTextureCache::ScaleRenderTargetSize(const GSVector2i& sz, float scale)
{
	return GSVector2i(static_cast<int>(std::ceil(static_cast<float>(sz.x) * scale)),
		static_cast<int>(std::ceil(static_cast<float>(sz.y) * scale)));
}

GSTextureCache::Target* GSTextureCache::LookupTarget(GIFRegTEX0 TEX0, const GSVector2i& size, float scale, int type,
	bool used, u32 fbmask, bool is_frame, bool preload, bool preserve_rgb, bool preserve_alpha, const GSVector4i draw_rect, bool is_shuffle, bool possible_clear)
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
		new_size = size.max(tgt->m_unscaled_size);
		new_scaled_size = ScaleRenderTargetSize(new_size, scale);
		dRect = (GSVector4(GSVector4i::loadh(tgt->m_unscaled_size)) * GSVector4(scale)).ceil();
		GL_INS("TC Rescale: %dx%d: %dx%d @ %f -> %dx%d @ %f", tgt->m_unscaled_size.x, tgt->m_unscaled_size.y,
			tgt->m_texture->GetWidth(), tgt->m_texture->GetHeight(), tgt->m_scale, new_scaled_size.x, new_scaled_size.y,
			scale);
	};

	Target* dst = nullptr;
	auto& list = m_dst[type];

	// TODO: Move all frame stuff to its own routine too.
	if (!is_frame)
	{
		for (auto i = list.begin(); i != list.end(); ++i)
		{
			Target* t = *i;

			if (bp == t->m_TEX0.TBP0)
			{
				bool can_use = true;
				// if It's an old target and it's being completely overwritten, kill it.
				// Dragon Quest 8 reuses a render-target sized buffer as a single-page buffer, without clearing it. But,
				// it does dirty it by writing over the 64x64 region. So while we can't use this heuristic for tossing
				// targets at BW=1 because it breaks other games, we can when the *new* buffer area is completely dirty.
				if (!preserve_rgb && !preserve_alpha && TEX0.TBW != t->m_TEX0.TBW)
				{
					if (TEX0.TBW > 1 && t->m_age >= 1)
					{
						can_use = false;
					}
					else if (!t->m_dirty.empty())
					{
						const GSVector4i size_rect = GSVector4i::loadh(size);
						can_use = !t->m_dirty.GetTotalRect(TEX0, size).rintersect(size_rect).eq(size_rect);
					}
				}

				// TODO: What might be a nicer solution than this, is to rearrange the targets to match the new layout, however this comes with some caviets:
				// 1. They can draw wider than the FBW
				// 2. The dirty+valid rects will need to also be rearranged
				// 3. This could mean larger targets hanging around more
				// 4. Sources which reference a target may become invalid and will need to be removed
				// 5. Potential performance implications from additional render passes/copying
				//
				// But the bonuses are:
				// 1. Rearranging the page layout will fix quite a few games which do this
				// 2. Preserved data will be in the correct place (in most cases)
				// 3. Less deleting sources/targets
				// 4. We can basically do clears in hardware, if they aren't insane ones
				if (can_use && !is_shuffle && ((preserve_alpha && preserve_rgb) || (size.y > GSLocalMemory::m_psm[t->m_TEX0.PSM].pgs.y && !possible_clear)) && TEX0.TBW != t->m_TEX0.TBW && t->m_dirty.size() >= 1)
				{
					can_use = false;
				}

				if (can_use)
				{
					list.MoveFront(i.Index());

					dst = t;

					dst->m_32_bits_fmt |= (psm_s.bpp != 16);
					break;
				}
				else
				{
					GL_INS("TC: Deleting RT BP 0x%x BW %d PSM %s due to change in target", t->m_TEX0.TBP0, t->m_TEX0.TBW, psm_str(t->m_TEX0.PSM));
					InvalidateSourcesFromTarget(t);
					i = list.erase(i);
					delete t;
				}
			}
		}
	}
	else
	{
		pxAssert(type == RenderTarget);
		// Let's try to find a perfect frame that contains valid data
		for (auto i = list.begin(); i != list.end(); ++i)
		{
			Target* t = *i;

			// Only checks that the texure starts at the requested bp, size isn't considered.
			if (bp == t->m_TEX0.TBP0 && t->m_end_block >= bp)
			{
				if (TEX0.TBW != t->m_TEX0.TBW && t->m_TEX0.TBW > 1 && t->m_age > 0)
				{
					// If frame is old and dirty, probably modified by the EE, so kill the wrong dimension version.
					if (!t->m_dirty.empty())
					{
						DevCon.Warning("Wanted %x psm %x bw %x, got %x psm %x bw %x, deleting", TEX0.TBP0, TEX0.PSM, TEX0.TBW, t->m_TEX0.TBP0, t->m_TEX0.PSM, t->m_TEX0.TBW);
						InvalidateSourcesFromTarget(t);
						i = list.erase(i);
						delete t;
						continue;
					}
				}
				dst = t;
				GL_CACHE("TC: Lookup Frame %dx%d, perfect hit: (0x%x -> 0x%x %s)", size.x, size.y, bp, t->m_end_block, psm_str(TEX0.PSM));
				if (size.x > 0 || size.y > 0)
					ScaleTargetForDisplay(dst, TEX0, size.x, size.y);

				break;
			}
		}

		// 2nd try ! Try to find a frame at the requested bp -> bp + size is inside of (or equal to)
		if (!dst)
		{
			for (auto i = list.begin(); i != list.end(); ++i)
			{
				Target* t = *i;
				// Make sure the target is inside the texture
				if (t->m_TEX0.TBP0 <= bp && bp <= t->m_end_block && t->Inside(bp, TEX0.TBW, TEX0.PSM, GSVector4i::loadh(size)))
				{
					if (TEX0.TBW != t->m_TEX0.TBW && t->m_TEX0.TBW > 1 && t->m_age > 0)
					{
						// If frame is old and dirty, probably modified by the EE, so kill the wrong dimension version.
						if (!t->m_dirty.empty())
						{
							DevCon.Warning("2 Wanted %x psm %x bw %x, got %x psm %x bw %x, deleting", TEX0.TBP0, TEX0.PSM, TEX0.TBW, t->m_TEX0.TBP0, t->m_TEX0.PSM, t->m_TEX0.TBW);
							InvalidateSourcesFromTarget(t);
							i = list.erase(i);
							delete t;
							continue;
						}
					}

					dst = t;
					GL_CACHE("TC: Lookup Frame %dx%d, inclusive hit: (0x%x, took 0x%x -> 0x%x %s)", size.x, size.y, bp, t->m_TEX0.TBP0, t->m_end_block, psm_str(TEX0.PSM));

					if (size.x > 0 || size.y > 0)
						ScaleTargetForDisplay(dst, TEX0, size.x, size.y);

					break;
				}
			}
		}

		// 3rd try ! Try to find a frame that doesn't contain valid data (honestly I'm not sure we need to do it)
		if (!dst)
		{
			for (auto i = list.begin(); i != list.end(); ++i)
			{
				Target* t = *i;
				if (bp == t->m_TEX0.TBP0 && TEX0.TBW == t->m_TEX0.TBW)
				{
					if (TEX0.TBW != t->m_TEX0.TBW && t->m_TEX0.TBW > 1 && t->m_age > 0)
					{
						// If frame is old and dirty, probably modified by the EE, so kill the wrong dimension version.
						if (!t->m_dirty.empty())
						{
							DevCon.Warning("3 Wanted %x psm %x bw %x, got %x psm %x bw %x, deleting", TEX0.TBP0, TEX0.PSM, TEX0.TBW, t->m_TEX0.TBP0, t->m_TEX0.PSM, t->m_TEX0.TBW);
							InvalidateSourcesFromTarget(t);
							i = list.erase(i);
							delete t;
							continue;
						}
					}

					dst = t;
					GL_CACHE("TC: Lookup Frame %dx%d, empty hit: (0x%x -> 0x%x %s)", size.x, size.y, bp, t->m_end_block, psm_str(TEX0.PSM));
					break;
				}
			}
		}
	}

	if (dst)
	{
		if (type == DepthStencil)
		{
			GL_CACHE("TC: Lookup Target(Depth) %dx%d (0x%x, BW:%u, %s) hit (0x%x, BW:%d, %s)", size.x, size.y, bp,
				TEX0.TBW, psm_str(TEX0.PSM), dst->m_TEX0.TBP0, dst->m_TEX0.TBW, psm_str(dst->m_TEX0.PSM));
		}
		else
		{
			GL_CACHE("TC: Lookup %s(Color) %dx%d (0x%x, BW:%u, FBMSK %08x, %s) hit (0x%x, BW:%d, %s)",
				is_frame ? "Frame" : "Target", size.x, size.y, bp, TEX0.TBW, fbmask, psm_str(TEX0.PSM),
				dst->m_TEX0.TBP0, dst->m_TEX0.TBW, psm_str(dst->m_TEX0.PSM));
		}

		if (dst->m_scale != scale)
		{
			calcRescale(dst);
			GSTexture* tex = type == RenderTarget ? g_gs_device->CreateRenderTarget(new_scaled_size.x, new_scaled_size.y, GSTexture::Format::Color, clear) :
													g_gs_device->CreateDepthStencil(new_scaled_size.x, new_scaled_size.y, GSTexture::Format::DepthStencil, clear);
			if (!tex)
				return nullptr;

			g_gs_device->StretchRect(dst->m_texture, sRect, tex, dRect, (type == RenderTarget) ? ShaderConvert::COPY : ShaderConvert::DEPTH_COPY, false);
			g_perfmon.Put(GSPerfMon::TextureCopies, 1);
			m_target_memory_usage = (m_target_memory_usage - dst->m_texture->GetMemUsage()) + tex->GetMemUsage();
			
			// If we're changing resolution scale, just toss the texture, it's not going to get reused.
			if (!GSConfig.UserHacks_NativePaletteDraw || (dst->m_scale != 1.0f && scale != 1.0f))
				delete dst->m_texture;
			else
				g_gs_device->Recycle(dst->m_texture);

			dst->m_texture = tex;
			dst->m_scale = scale;
			dst->m_unscaled_size = new_size;
		}

		// If our RGB was invalidated, we need to pull it from depth.
		// Terminator 3 will reuse our dst_matched target with the RGB masked, then later use the full ARGB area, so we need to update the depth.
		const bool preserve_target = preserve_rgb || preserve_alpha;
		if (type == RenderTarget && (preserve_target || !dst->m_valid.rintersect(draw_rect).eq(dst->m_valid)) &&
			!dst->m_valid_rgb && !FullRectDirty(dst, 0x7) &&
			(GSLocalMemory::m_psm[TEX0.PSM].trbpp < 24 || fbmask != 0x00FFFFFFu))
		{
			// Neo Contra clears 0x1400 with Z16S, then uses that address to upload C32 frames, this gets confused and makes a mess of it.
			// TODO: Look in to making sure bad format conversions don't happen.
			if (!is_frame)
			{
				GL_CACHE("TC: Attempt to repopulate RGB for %s[%x]", to_string(type), dst->m_TEX0.TBP0);
				for (Target* dst_match : m_dst[DepthStencil])
				{
					if (dst_match->m_TEX0.TBP0 != TEX0.TBP0 || !dst_match->m_valid_rgb)
						continue;

					dst->m_was_dst_matched = true;
					dst->m_TEX0.TBW = dst_match->m_TEX0.TBW;
					dst->UpdateValidity(dst->m_valid);
					if (!CopyRGBFromDepthToColor(dst, dst_match))
					{
						// Needed new texture and memory allocation failed.
						return nullptr;
					}

					break;
				}
			}

			if (!dst->m_valid_rgb)
			{
				GL_CACHE("TC: Cannot find RGB target for %s[%x], clearing.", to_string(type), dst->m_TEX0.TBP0);

				// We couldn't get RGB from any depth targets. So clear and preload.
				// Unfortunately, we still have an alpha channel to preserve, and we can't clear RGB...
				// So, create a new target, clear/preload it, and copy RGB in.
				GSTexture* tex = (type == RenderTarget) ?
									 g_gs_device->CreateRenderTarget(dst->m_texture->GetWidth(),
										 dst->m_texture->GetHeight(), GSTexture::Format::Color, true) :
									 g_gs_device->CreateDepthStencil(dst->m_texture->GetWidth(),
										 dst->m_texture->GetHeight(), GSTexture::Format::DepthStencil, true);
				if (!tex)
					return nullptr;

				std::swap(dst->m_texture, tex);
				PreloadTarget(TEX0, size, GSVector2i(dst->m_valid.z, dst->m_valid.w), is_frame, preload,
					preserve_target, draw_rect, dst);
				g_gs_device->StretchRect(tex, GSVector4::cxpr(0.0f, 0.0f, 1.0f, 1.0f), dst->m_texture,
					GSVector4(dst->m_texture->GetRect()), false, false, false, true);
				g_gs_device->Recycle(tex);
				dst->m_valid_rgb = true;
			}
		}

		// Drop dirty rect if we're overwriting the whole target.
		if (!preserve_target && draw_rect.rintersect(dst->m_valid).eq(dst->m_valid))
		{
			// Preserve alpha if this is a 32-bit target being used as 24-bit.
			const bool dont_invalidate_alpha = (dst->HasValidAlpha() && (psm_s.fmt == GSLocalMemory::PSM_FMT_24 || (fbmask & 0xFF000000u) != 0));
			if (dont_invalidate_alpha)
			{
				GL_INS("TC: Preserving alpha on 24-bit/masked %s[%x] because it was previously valid.", to_string(type), dst->m_TEX0.TBP0);

				// We can still toss all dirty RGB writes though. Gotta save those uploads.
				if (!dst->m_dirty.empty())
				{
					GL_INS("TC: Clearing RGB dirty list for %s[%x] because we're overwriting the whole target.", to_string(type), dst->m_TEX0.TBP0);
					for (s32 i = static_cast<s32>(dst->m_dirty.size()) - 1; i >= 0; i--)
					{
						if (!dst->m_dirty[i].rgba.c.a)
							dst->m_dirty.erase(dst->m_dirty.begin() + static_cast<size_t>(i));
					}
				}
			}
			else
			{
				if (!dst->m_dirty.empty())
				{
					GL_INS("TC: Clearing dirty list for %s[%x] because we're overwriting the whole target.", to_string(type), dst->m_TEX0.TBP0);
					dst->m_dirty.clear();
				}

				// And invalidate the target, we're drawing over it so we don't care what's there.
				GL_INS("TC: Invalidating target %s[%x] because it's completely overwritten.", to_string(type), dst->m_TEX0.TBP0);
				g_gs_device->InvalidateRenderTarget(dst->m_texture);
			}
		}
	}
	else if (!is_frame && !GSConfig.UserHacks_DisableDepthSupport)
	{
		const int rev_type = (type == DepthStencil) ? RenderTarget : DepthStencil;

		// Depth stencil/RT can be an older RT/DS but only check recent RT/DS to avoid to pick
		// some bad data.
		auto& rev_list = m_dst[rev_type];
		Target* dst_match = nullptr;
		for (auto i = rev_list.begin(); i != rev_list.end(); ++i)
		{
			Target* t = *i;
			// Don't pull in targets without valid lower 24 bits, it makes no sense to convert them.
			// FIXME: Technically the difference in size is fine, but if the target gets reinterpreted, the hw renderer doesn't rearrange the target.
			// This does cause some extra uploads in some games (like Burnout), but without this, bad data gets displayed in games like Transformers.
			if (bp != t->m_TEX0.TBP0 || !t->m_valid_rgb || (!is_shuffle && t->m_TEX0.TBW < TEX0.TBW && possible_clear))
			{
				continue;
			}

			const GSLocalMemory::psm_t& t_psm_s = GSLocalMemory::m_psm[t->m_TEX0.PSM];
			if (t_psm_s.bpp != psm_s.bpp)
			{
				bool remove_target = possible_clear;

				// If we have a BW change, and it's not a multiple of 2 (for a shuffle), the game's going to get a jigsaw
				// puzzle of pages and can't be expecting to have legitimate data. Tokimeki Memorial 3 reuses a BW 17
				// buffer as BW 10, and if we don't discard the BW 17 buffer, the BW 10 buffer ends up twice the size.
				const u32 shuffle_bw = (psm_s.bpp > t_psm_s.bpp) ? (TEX0.TBW / 2u) : (TEX0.TBW * 2u);
				if (t->m_TEX0.TBW != TEX0.TBW && t->m_TEX0.TBW != shuffle_bw)
				{
					// But we'll make sure the whole existing target's actually being drawn over to be safe.
					const u32 end_block = GSLocalMemory::GetUnwrappedEndBlockAddress(TEX0.TBP0, TEX0.TBW, TEX0.PSM, draw_rect);
					if (end_block >= t->UnwrappedEndBlock())
					{
						GL_CACHE("Not converting %s at %x TBW %u with end block of %x when we're drawing through %x",
							to_string(rev_type), t->m_TEX0.TBP0, t->m_TEX0.TBW, t->UnwrappedEndBlock(), end_block);
						remove_target = true;
					}
				}

				// Probably an old target, get rid of it.
				if (remove_target)
				{
					InvalidateSourcesFromTarget(t);
					i = rev_list.erase(i);
					delete t;
					continue;
				}
			}

			if (t->m_age == 0)
			{
				dst_match = t;
				break;
			}
			else if (t->m_age == 1 && (preserve_rgb || (preserve_alpha && (t->m_valid_alpha_low || t->m_valid_alpha_high))))
			{
				dst_match = t;
			}
		}
		// We only want to use a matched target if it's actually being used.
		if (dst_match)
		{
			calcRescale(dst_match);

			// If we don't need A, and the existing target doesn't have valid alpha, don't bother converting it.
			const bool has_alpha = dst_match->HasValidAlpha();
			const bool preserve_target = (preserve_rgb || (preserve_alpha && has_alpha)) ||
										 !draw_rect.rintersect(dst_match->m_valid).eq(dst_match->m_valid);

			bool half_width = false;

			if (GSLocalMemory::m_psm[TEX0.PSM].bpp == 32 && GSLocalMemory::m_psm[dst_match->m_TEX0.PSM].bpp == 16)
				if (dst_match->m_valid.z == ((TEX0.TBW * 64) * 2))
					half_width = true;

			// Clear instead of invalidating if there is anything which isn't touched.
			clear |= (!preserve_target && fbmask != 0);
			GIFRegTEX0 new_TEX0;
			new_TEX0.TBP0 = TEX0.TBP0;
			new_TEX0.TBW = (!half_width) ? dst_match->m_TEX0.TBW : TEX0.TBW;
			new_TEX0.PSM = is_shuffle ? dst_match->m_TEX0.PSM :TEX0.PSM;

			dst = Target::Create(new_TEX0, new_size.x, new_size.y, scale, type, clear);
			if (!dst)
				return nullptr;

			dst->m_32_bits_fmt = dst_match->m_32_bits_fmt;
			dst->OffsetHack_modxy = dst_match->OffsetHack_modxy;
			dst->m_end_block = dst_match->m_end_block; // If we're copying the size, we need to keep the end block.
			dst->m_valid = dst_match->m_valid;
			dst->m_valid_alpha_low = dst_match->m_valid_alpha_low;//&& psm_s.trbpp != 24;
			dst->m_valid_alpha_high = dst_match->m_valid_alpha_high;//&& psm_s.trbpp != 24;
			dst->m_valid_rgb = dst_match->m_valid_rgb;
			dst->m_was_dst_matched = true;

			if(GSLocalMemory::m_psm[dst->m_TEX0.PSM].bpp == 16 && GSLocalMemory::m_psm[dst_match->m_TEX0.PSM].bpp > 16)
				dst->m_TEX0.TBW = dst_match->m_TEX0.TBW; // Be careful of shuffles of the depth as C16, but using a buffer width of 16 (Mercenaries).
			else if (GSLocalMemory::m_psm[dst->m_TEX0.PSM].bpp == 32 && GSLocalMemory::m_psm[dst_match->m_TEX0.PSM].bpp == 16)
			{
				// If we're coming from a 16bit target to 32bit, one of the dimensions is smaller.
				if (half_width)
					dst->m_valid.z /= 2;
				else
					dst->m_valid.w /= 2;

				dst->UpdateValidity(dst->m_valid);
			}

			ShaderConvert shader;
			// m_32_bits_fmt gets set on a shuffle or if the format isn't 16bit.
			// In this case it needs to make sure it isn't part of a shuffle, where it needs to be interpreted as 32bits.
			const bool fmt_16_bits = (psm_s.bpp == 16 && GSLocalMemory::m_psm[dst_match->m_TEX0.PSM].bpp == 16 && !dst->m_32_bits_fmt);
			if (type == DepthStencil)
			{
				GL_CACHE("TC: Lookup Target(Depth) %dx%d, hit Color (0x%x, TBW %d, %s was %s)", new_size.x, new_size.y,
					bp, TEX0.TBW, psm_str(TEX0.PSM), psm_str(dst_match->m_TEX0.PSM));
				shader = (fmt_16_bits) ? ShaderConvert::RGB5A1_TO_FLOAT16 :
										 (ShaderConvert)(static_cast<int>(ShaderConvert::RGBA8_TO_FLOAT32) + psm_s.fmt);
			}
			else
			{
				GL_CACHE("TC: Lookup Target(Color) %dx%d, hit Depth (0x%x, TBW %d, FBMSK %0x, %s was %s)", new_size.x,
					new_size.y, bp, TEX0.TBW, fbmask, psm_str(TEX0.PSM), psm_str(dst_match->m_TEX0.PSM));
				shader = (fmt_16_bits) ? ShaderConvert::FLOAT16_TO_RGB5A1 : ShaderConvert::FLOAT32_TO_RGBA8;
			}


			if (!preserve_target)
			{
				GL_INS("TC: Not converting existing %s[%x] because it's fully overwritten.", to_string(!type), dst->m_TEX0.TBP0);
			}
			else
			{
				// The old target's going to get invalidated (at least until we handle concurrent frame+depth at the same BP),
				// so just move the dirty rects across, unless the format is diffent, in which case we need to update it.
				if (dst->m_TEX0.PSM != dst_match->m_TEX0.PSM)
					dst_match->Update();

				dst->m_dirty = std::move(dst_match->m_dirty);
				dst_match->m_dirty = {};
				dst->m_alpha_max = dst_match->m_alpha_max;
				dst->m_alpha_min = dst_match->m_alpha_min;

				// Don't bother copying the old target in if the whole thing is dirty.
				if (dst->m_dirty.empty() || (~dst->m_dirty.GetDirtyChannels() & GSUtil::GetChannelMask(TEX0.PSM)) != 0 ||
					!dst->m_dirty.GetDirtyRect(0, TEX0, dst->GetUnscaledRect(), false).eq(dst->GetUnscaledRect()))
				{
					// If the old target was cleared, simply propagate that through.
					if (dst_match->m_texture->GetState() == GSTexture::State::Cleared)
					{
						if (type == DepthStencil)
						{
							const u32 cc = dst_match->m_texture->GetClearColor();
							const float cd = ConvertColorToDepth(cc, shader);
							GL_INS("TC: Convert clear color[%08X] to depth[%f]", cc, cd);
							g_gs_device->ClearDepth(dst->m_texture, cd);
						}
						else
						{
							const float cd = dst_match->m_texture->GetClearDepth();
							const u32 cc = ConvertDepthToColor(cd, shader);
							GL_INS("TC: Convert clear depth[%f] to color[%08X]", cd, cc);
							g_gs_device->ClearRenderTarget(dst->m_texture, cc);
						}
					}
					else if (dst_match->m_texture->GetState() == GSTexture::State::Dirty)
					{
						g_gs_device->StretchRect(dst_match->m_texture, sRect, dst->m_texture, dRect, shader, false);
						g_perfmon.Put(GSPerfMon::TextureCopies, 1);
					}
				}

				// Now pull in any dirty areas in the new format.
				dst->Update();
			}
		}
	}

	if (dst)
	{
		dst->m_used |= used;
		dst->readbacks_since_draw = 0;

		pxAssert(dst && dst->m_texture && dst->m_scale == scale);
	}

	return dst;
}

GSTextureCache::Target* GSTextureCache::CreateTarget(GIFRegTEX0 TEX0, const GSVector2i& size, const GSVector2i& valid_size, float scale, int type,
	bool used, u32 fbmask, bool is_frame, bool preload, bool preserve_target, const GSVector4i draw_rect, GSTextureCache::Source* src)
{
	if (type == DepthStencil)
	{
		GL_CACHE("TC: Lookup Target(Depth) %dx%d, miss (0x%x, TBW %d, %s) draw %d", size.x, size.y, TEX0.TBP0,
			TEX0.TBW, psm_str(TEX0.PSM), g_gs_renderer->s_n);
	}
	else
	{
		GL_CACHE("TC: Lookup %s(Color) %dx%d FBMSK %08x, miss (0x%x, TBW %d, %s) draw %d", is_frame ? "Frame" : "Target",
			size.x, size.y, fbmask, TEX0.TBP0, TEX0.TBW, psm_str(TEX0.PSM), g_gs_renderer->s_n);
	}
	// Avoid making garbage targets (usually PCRTC).
	if (GSVector4i::loadh(size).rempty())
		return nullptr;

	Target* dst = Target::Create(TEX0, size.x, size.y, scale, type, true);
	if (!dst) [[unlikely]]
		return nullptr;

	const bool was_clear = PreloadTarget(TEX0, size, valid_size, is_frame, preload, preserve_target, draw_rect, dst, src);

	dst->m_is_frame = is_frame;

	dst->m_used |= used;

	if (!is_frame)
	{
		// Not *strictly* correct if RGB is masked, but we won't use it as a texture if not..
		dst->m_valid_rgb = true;

		// If there is an opposite target without valid RGB, we need to match them up
		auto& rev_list = m_dst[1 - type];
		for (auto j = rev_list.begin(); j != rev_list.end();)
		{
			Target* const rev_t = *j;
			if (rev_t->m_TEX0.TBP0 == dst->m_TEX0.TBP0 && GSLocalMemory::m_psm[rev_t->m_TEX0.PSM].bpp == GSLocalMemory::m_psm[dst->m_TEX0.PSM].bpp)
			{
				if(!rev_t->m_valid_rgb)
					rev_t->m_was_dst_matched = true;
				break;
			}
			++j;
		}

		const int bpp = GSLocalMemory::m_psm[TEX0.PSM].trbpp;
		const u32 mask = GSLocalMemory::m_psm[TEX0.PSM].fmsk;
		// If the alpha is masked and preloaded, we need to say it's valid else textures might fail to use the whole texture if RGB is valid.
		if (((fbmask & 0xFF000000) & mask) != (mask & 0xFF000000) && bpp != 24)
		{
			dst->m_valid_alpha_high = (~(fbmask & mask) & 0xf0000000) & mask;
			dst->m_valid_alpha_low = (~(fbmask & mask) & 0x0f000000) & mask;

			if (bpp == 16)
				dst->m_valid_alpha_low = dst->m_valid_alpha_high;
		}
		else if ((dst->m_dirty.GetDirtyChannels() & 0x8) && bpp != 24)
		{
			if (!preload || was_clear)
			{
				dst->m_valid_alpha_high = true;
				dst->m_valid_alpha_low = true;
			}
			else
			{
				std::vector<GSState::GSUploadQueue>::reverse_iterator iter;
				const int start_transfer = g_gs_renderer->s_transfer_n;
				const u32 tex_end = GSLocalMemory::GetUnwrappedEndBlockAddress(TEX0.TBP0, TEX0.TBW, TEX0.PSM, GSVector4i::loadh(size));
				for (iter = GSRendererHW::GetInstance()->m_draw_transfers.rbegin(); iter != GSRendererHW::GetInstance()->m_draw_transfers.rend(); )
				{
					const u32 transfer_end = GSLocalMemory::GetUnwrappedEndBlockAddress(iter->blit.DBP, iter->blit.DBW, iter->blit.DPSM, iter->rect);
					// If the format, and location doesn't overlap
					if (TEX0.TBP0 == iter->blit.DBP && GSUtil::HasCompatibleBits(iter->blit.DPSM, TEX0.PSM) && (iter->blit.DBW == dst->m_TEX0.TBW || (transfer_end >= tex_end && (iter->blit.DBW * 64) == iter->rect.z)))
					{
						dst->m_valid_alpha_high = iter->blit.DPSM != PSMT4HL;
						dst->m_valid_alpha_low = iter->blit.DPSM != PSMT4HH;
						break;
					}

					if ((start_transfer - iter->draw) > 100)
						break;

					++iter;
				}
			}
		}
		if (was_clear)
		{
			GL_INS("TC: Clear dirty list on new target %x, because it was a zero clear", TEX0.TBP0);
			dst->m_dirty.clear();
		}
	}

	dst->readbacks_since_draw = 0;

	pxAssert(dst && dst->m_texture && dst->m_scale == scale);
	return dst;
}

bool GSTextureCache::PreloadTarget(GIFRegTEX0 TEX0, const GSVector2i& size, const GSVector2i& valid_size, bool is_frame,
	bool preload, bool preserve_target, const GSVector4i draw_rect, Target* dst, GSTextureCache::Source* src)
{
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
	const GSLocalMemory::psm_t& psm_s = GSLocalMemory::m_psm[TEX0.PSM];
	const bool supported_fmt = !GSConfig.UserHacks_DisableDepthSupport || psm_s.depth == 0;
	std::optional<bool> hw_clear;

	if (TEX0.TBW > 0 && supported_fmt)
	{
		const GSVector4i newrect = GSVector4i::loadh(size);
		const u32 rect_end = GSLocalMemory::GetUnwrappedEndBlockAddress(TEX0.TBP0, TEX0.TBW, TEX0.PSM, newrect);

		RGBAMask rgba;
		rgba._u32 = GSUtil::GetChannelMask(TEX0.PSM);
		dst->UpdateValidity(GSVector4i::loadh(valid_size));

		if (!is_frame && !preload && !(src && src->m_TEX0.TBP0 == dst->m_TEX0.TBP0))
		{
			if ((preserve_target || !draw_rect.eq(dst->m_valid)) && GSRendererHW::GetInstance()->m_draw_transfers.size() > 0)
			{
				auto& transfers = GSRendererHW::GetInstance()->m_draw_transfers;
				const int last_draw = transfers.back().draw;
				GSVector4i eerect = GSVector4i::zero();

				for (auto iter = transfers.rbegin(); iter != transfers.rend(); ++iter)
				{
					// Would be nice to make this 100, but B-Boy seems to rely on data uploaded ~200 draws ago. Making it bigger for now to be safe.
					if (last_draw - iter->draw > 500)
						break;

					const u32 transfer_end = GSLocalMemory::GetUnwrappedEndBlockAddress(iter->blit.DBP, iter->blit.DBW, iter->blit.DPSM, iter->rect);

					// If the format, and location doesn't overlap
					if (transfer_end >= TEX0.TBP0 && iter->blit.DBP <= rect_end && GSUtil::HasCompatibleBits(iter->blit.DPSM, TEX0.PSM))
					{
						GSVector4i targetr = {};
						const bool can_translate = CanTranslate(iter->blit.DBP, iter->blit.DBW, iter->blit.DPSM, iter->rect, TEX0.TBP0, TEX0.PSM, TEX0.TBW);
						const bool swizzle_match = GSLocalMemory::m_psm[iter->blit.DPSM].depth == GSLocalMemory::m_psm[TEX0.PSM].depth;
						if (can_translate)
						{
							if (swizzle_match)
							{
								targetr = TranslateAlignedRectByPage(dst, iter->blit.DBP, iter->blit.DPSM, iter->blit.DBW, iter->rect, true);
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
								targetr = TranslateAlignedRectByPage(dst, iter->blit.DBP & ~((1 << 5) - 1), iter->blit.DPSM, iter->blit.DBW, new_rect, true);
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

						// Later writes might be partial over a previously cleared area. We want to upload in these cases.
						hw_clear = hw_clear.has_value() ? (hw_clear.value() && iter->zero_clear) : iter->zero_clear;

						// When the write covers the entire target, don't bother checking any earlier writes.
						if (iter->blit.DBP <= TEX0.TBP0 && transfer_end >= rect_end)
						{
							// Some games clear RT and Z at the same time, only erase if it's specifically this target.
							if (iter->blit.DBP == TEX0.TBP0 && transfer_end == rect_end)
								transfers.erase(iter.base() - 1);

							break;
						}

						if (eerect.rintersect(newrect).eq(newrect))
							break;
					}
				}

				if (!eerect.rempty())
				{
					GL_INS("Preloading the RT DATA from updated GS Memory");
					AddDirtyRectTarget(dst, newrect, TEX0.PSM, TEX0.TBW, rgba, GSLocalMemory::m_psm[TEX0.PSM].trbpp >= 16);
				}
			}
		}
		else
		{
			if (GSRendererHW::GetInstance()->m_draw_transfers.size() > 0)
			{
				GSState::GSUploadQueue last_draw = GSRendererHW::GetInstance()->m_draw_transfers.back();
				if (last_draw.zero_clear && last_draw.blit.DBP == TEX0.TBP0 && last_draw.blit.DBW == TEX0.TBW)
				{
					hw_clear = true;
					GSRendererHW::GetInstance()->m_draw_transfers.pop_back();
				}
			}
			GL_INS("Preloading the RT DATA");
			AddDirtyRectTarget(dst, newrect, TEX0.PSM, TEX0.TBW, rgba, GSLocalMemory::m_psm[TEX0.PSM].trbpp >= 16);
		}
	}

	dst->UpdateValidity(GSVector4i::loadh(valid_size));

	// Can't do channel writes to depth targets, and DirectX can't partial copy depth targets.
	if (psm_s.depth == 0)
	{
		for (int type = 0; type < 2; type++)
		{
			auto& list = m_dst[type];
			for (auto i = list.begin(); i != list.end();)
			{
				auto j = i;
				Target* t = *j;

				if (dst != t && t->m_TEX0.TBW == dst->m_TEX0.TBW && t->m_TEX0.PSM == dst->m_TEX0.PSM && t->m_TEX0.TBW > 4)
					if (t->Overlaps(dst->m_TEX0.TBP0, dst->m_TEX0.TBW, dst->m_TEX0.PSM, dst->m_valid))
					{
						// could be overwriting a double buffer, so if it's the second half of it, just reduce the size down to half.
						if (((((t->UnwrappedEndBlock() + 1) - t->m_TEX0.TBP0) >> 1) + t->m_TEX0.TBP0) == dst->m_TEX0.TBP0)
						{
							GSVector4i new_valid = t->m_valid;
							new_valid.w /= 2;
							GL_INS("RT resize buffer for FBP 0x%x, %dx%d => %d,%d", t->m_TEX0.TBP0, t->m_valid.width(), t->m_valid.height(), new_valid.width(), new_valid.height());
							t->ResizeValidity(new_valid);
							return hw_clear.value_or(false);
						}
						// The new texture is behind it but engulfs the whole thing, shrink the new target so it grows in the HW Draw resize.
						else if (((((dst->UnwrappedEndBlock() + 1) - dst->m_TEX0.TBP0) >> 1) + dst->m_TEX0.TBP0) == t->m_TEX0.TBP0)
						{
							const int rt_pages = ((t->UnwrappedEndBlock() + 1) - t->m_TEX0.TBP0) >> 5;
							const int overlapping_pages = std::min(rt_pages, static_cast<int>((dst->UnwrappedEndBlock() + 1) - t->m_TEX0.TBP0) >> 5);
							const int overlapping_pages_height = (overlapping_pages / dst->m_TEX0.TBW) * GSLocalMemory::m_psm[t->m_TEX0.PSM].pgs.y;

							if (overlapping_pages_height == 0 || (overlapping_pages % dst->m_TEX0.TBW))
							{
								// No overlap top copy or the widths don't match.
								i++;
								continue;
							}

							if (!hw_clear && (preserve_target || preload))
							{
								const int copy_width = (t->m_texture->GetWidth()) > (dst->m_texture->GetWidth()) ? (dst->m_texture->GetWidth()) : t->m_texture->GetWidth();
								const int copy_height = overlapping_pages_height * t->m_scale;
								const int dst_offset = (dst->m_valid.w - overlapping_pages_height) * dst->m_scale;
								GL_INS("RT double buffer copy from FBP 0x%x, %dx%d => %d,%d", t->m_TEX0.TBP0, copy_width, copy_height, 0, dst_offset);

								pxAssert(copy_width <= dst->GetTexture()->GetWidth() && copy_height <= dst->GetTexture()->GetHeight() &&
										copy_width <= t->GetTexture()->GetWidth() && copy_height <= t->GetTexture()->GetHeight());

								pxAssert(dst_offset > 0);

								// Clear the dirty first
								dst->Update();
								// Invalidate has been moved to after DrawPrims(), because we might kill the current sources' backing.
								if (!t->m_valid_rgb || !(t->m_valid_alpha_high || t->m_valid_alpha_low))
								{
									const GSVector4 src_rect = GSVector4(0, 0, copy_width, copy_height) / (GSVector4(t->m_texture->GetSize()).xyxy());
									const GSVector4 dst_rect = GSVector4(0, dst_offset, copy_width, copy_height);
									g_gs_device->StretchRect(t->m_texture, src_rect, dst->m_texture, dst_rect, t->m_valid_rgb, t->m_valid_rgb, t->m_valid_rgb, t->m_valid_alpha_high || t->m_valid_alpha_low);
								}
								else
								{
									// Invalidate has been moved to after DrawPrims(), because we might kill the current sources' backing.
									g_gs_device->CopyRect(t->m_texture, dst->m_texture, GSVector4i(0, 0, copy_width, copy_height), 0, dst_offset);
								}
							}

							if ((overlapping_pages < rt_pages) || (src && src->m_target && src->m_from_target == t))
							{
								// This should never happen as we're making a new target so the src should never be something it overlaps, but just incase..
								GSVector4i new_valid = t->m_valid;
								new_valid.y = std::max(new_valid.y - overlapping_pages_height, 0);
								new_valid.w = std::max(new_valid.w - overlapping_pages_height, 0);
								t->m_TEX0.TBP0 += (overlapping_pages_height / GSLocalMemory::m_psm[t->m_TEX0.PSM].pgs.y) << 5;
								t->ResizeValidity(new_valid);
							}
							else
							{
								InvalidateSourcesFromTarget(t);
								i = list.erase(j);
								delete t;
							}
							return hw_clear.value_or(false);
						}
					}
				i++;
			}
		}
	}

	return hw_clear.value_or(false);
}

GSTextureCache::Target* GSTextureCache::LookupDisplayTarget(GIFRegTEX0 TEX0, const GSVector2i& size, float scale, bool is_feedback)
{
	Target* dst = LookupTarget(TEX0, size, scale, RenderTarget, true, 0, true);
	if (dst)
		return dst;

	// Didn't find a target, check if the frame was uploaded.

	bool can_create = is_feedback;

	if (!is_feedback && GSRendererHW::GetInstance()->m_draw_transfers.size() > 0)
	{
		const GSVector4i newrect = GSVector4i::loadh(size);
		const u32 rect_end = GSLocalMemory::GetUnwrappedEndBlockAddress(TEX0.TBP0, TEX0.TBW, TEX0.PSM, newrect);

		std::vector<GSState::GSUploadQueue>::reverse_iterator iter;
		GSVector4i eerect = GSVector4i::zero();
		const int last_draw = GSRendererHW::GetInstance()->m_draw_transfers.back().draw;

		for (iter = GSRendererHW::GetInstance()->m_draw_transfers.rbegin(); iter != GSRendererHW::GetInstance()->m_draw_transfers.rend(); )
		{
			// Would be nice to make this 100, but B-Boy seems to rely on data uploaded ~200 draws ago. Making it bigger for now to be safe.
			if (last_draw - iter->draw > 500)
				break;

			const u32 transfer_end = GSLocalMemory::GetUnwrappedEndBlockAddress(iter->blit.DBP, iter->blit.DBW, iter->blit.DPSM, iter->rect);

			// If the format, and location doesn't overlap
			if (transfer_end >= TEX0.TBP0 && iter->blit.DBP <= rect_end && GSUtil::HasCompatibleBits(iter->blit.DPSM, TEX0.PSM))
			{
				GSVector4i targetr = iter->rect;
				
				if (eerect.rempty())
					eerect = targetr;
				else
					eerect = eerect.runion(targetr);

				if (iter->zero_clear && iter->draw == last_draw)
				{
					can_create = false;
					break;
				}

				if (iter->blit.DBP == TEX0.TBP0 && transfer_end == rect_end)
				{
					iter = std::vector<GSState::GSUploadQueue>::reverse_iterator(GSRendererHW::GetInstance()->m_draw_transfers.erase(iter.base() - 1));
				}
				else
					++iter;

				// In theory it might not be a full rect, but it should be enough to display *something*.
				// It's also possible we haven't saved enough of the transfers to fill the rect if the game draws the picture in lots of small transfers.
				can_create = true;
				break;
			}
			else
				++iter;
		}
	}

	return can_create ? CreateTarget(TEX0, size, size, scale, RenderTarget, true, 0, true) : nullptr;
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
	g_perfmon.Put(GSPerfMon::TextureCopies, 1);
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

	// Inject the new size back into the cache.
	GetTargetSize(t->m_TEX0.TBP0, t->m_TEX0.TBW, t->m_TEX0.PSM, 0, static_cast<u32>(needed_height));
}

float GSTextureCache::ConvertColorToDepth(u32 c, ShaderConvert convert)
{
	const float mult = std::exp2(g_gs_device->Features().clip_control ? -32.0f : -24.0f);
	switch (convert)
	{
		case ShaderConvert::RGB5A1_TO_FLOAT16:
			return static_cast<float>(((c & 0xF8u) >> 3) | (((c >> 8) & 0xF8u) << 2) | (((c >> 16) & 0xF8u) << 7) |
									  (((c >> 24) & 0x80u) << 8)) *
				   mult;

		case ShaderConvert::RGBA8_TO_FLOAT16:
			return static_cast<float>(c & 0x0000FFFF) * mult;

		case ShaderConvert::RGBA8_TO_FLOAT24:
			return static_cast<float>(c & 0x00FFFFFF) * mult;

		case ShaderConvert::RGBA8_TO_FLOAT32:
		default:
			return static_cast<float>(c) * mult;
	}
}

u32 GSTextureCache::ConvertDepthToColor(float d, ShaderConvert convert)
{
	const float mult = std::exp2(g_gs_device->Features().clip_control ? 32.0f : 24.0f);
	switch (convert)
	{
		case ShaderConvert::FLOAT16_TO_RGB5A1:
		{
			const u32 cc = static_cast<u32>(d * mult);

			// Truely awful.
			const GSVector4i vcc = GSVector4i(
				GSVector4(GSVector4i(cc & 0x1Fu, (cc >> 5) & 0x1Fu, (cc >> 10) & 0x1Fu, (cc >> 15) & 0x01u)) *
				GSVector4::cxpr(255.0f / 31.0f));
			return (vcc.r | (vcc.g << 8) | (vcc.b << 16) | (vcc.a << 24));
		}

		case ShaderConvert::FLOAT32_TO_RGBA8:
		default:
			return static_cast<u32>(d * mult);
	}
}

bool GSTextureCache::CopyRGBFromDepthToColor(Target* dst, Target* depth_src)
{
	GL_CACHE("TC: Copy RGB from %dx%d %s[%x, %s] to %dx%d %s[%x, %s]", depth_src->GetUnscaledWidth(),
		depth_src->GetUnscaledHeight(), to_string(depth_src->m_type), depth_src->m_TEX0.TBP0,
		psm_str(depth_src->m_TEX0.PSM), dst->GetUnscaledWidth(), dst->GetUnscaledHeight(), to_string(dst->m_type),
		dst->m_TEX0.TBP0, psm_str(dst->m_TEX0.PSM));

	// The depth target might be larger (Driv3r).
	const GSVector2i new_size = depth_src->GetUnscaledSize().max(dst->GetUnscaledSize());
	const GSVector2i new_scaled_size = ScaleRenderTargetSize(new_size, dst->GetScale());
	const bool needs_new_tex = (new_size != dst->m_unscaled_size);
	GSTexture* tex = dst->m_texture;
	if (needs_new_tex)
	{
		tex = g_gs_device->CreateRenderTarget(new_scaled_size.x, new_scaled_size.y, GSTexture::Format::Color,
			new_size != dst->m_unscaled_size || new_size != depth_src->m_unscaled_size);
		if (!tex)
			return false;

		m_target_memory_usage = (m_target_memory_usage - dst->m_texture->GetMemUsage()) + tex->GetMemUsage();
	}

	// Remove any dirty rectangles contained by this update, we don't want to pull from local memory.
	const GSVector4i clear_dirty_rc = GSVector4i::loadh(depth_src->GetUnscaledSize());
	for (u32 i = 0; i < dst->m_dirty.size(); i++)
	{
		GSDirtyRect& dr = dst->m_dirty[i];
		const GSVector4i drc = dr.GetDirtyRect(dst->m_TEX0, false);
		if (!drc.rintersect(clear_dirty_rc).rempty())
		{
			if ((dr.rgba._u32 &= ~0x7) == 0)
			{
				GL_CACHE("TC: Remove dirty rect (%d,%d=>%d,%d) from %s[%x, %s] due to incoming depth.", drc.left,
					drc.top, drc.right, drc.bottom, to_string(dst->m_type), dst->m_TEX0.TBP0, psm_str(dst->m_TEX0.PSM));
				dst->m_dirty.erase(dst->m_dirty.begin() + i);
			}
		}
	}

	// Depth source should be up to date.
	depth_src->Update();

	constexpr ShaderConvert shader = ShaderConvert::FLOAT32_TO_RGB8;
	if (depth_src->m_texture->GetState() == GSTexture::State::Cleared)
	{
		g_gs_device->ClearRenderTarget(tex, ConvertDepthToColor(depth_src->m_texture->GetClearDepth(), shader));
	}
	else if (depth_src->m_texture->GetState() != GSTexture::State::Invalidated)
	{
		const GSVector4 convert_rect = GSVector4(depth_src->GetUnscaledRect().rintersect(GSVector4i::loadh(new_size)));
		g_gs_device->StretchRect(depth_src->m_texture, convert_rect / GSVector4(depth_src->GetUnscaledSize()).xyxy(),
			tex, convert_rect * GSVector4(dst->GetScale()), shader, false);
		g_perfmon.Put(GSPerfMon::TextureCopies, 1);
	}

	// Copy in alpha if we're a new texture.
	if (needs_new_tex)
	{
		if (dst->m_valid_alpha_low || dst->m_valid_alpha_high)
		{
			g_gs_device->StretchRect(dst->m_texture, GSVector4::cxpr(0.0f, 0.0f, 1.0f, 1.0f), tex,
				GSVector4(GSVector4i::loadh(dst->m_unscaled_size)), false, false, false, true);
			g_perfmon.Put(GSPerfMon::TextureCopies, 1);
		}

		g_gs_device->Recycle(dst->m_texture);
		dst->m_texture = tex;
	}

	dst->m_unscaled_size = new_size;
	dst->m_valid_rgb = true;
	dst->m_32_bits_fmt = true;
	return true;
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

#ifdef PCSX2_DEVBUILD
	(*tex)->SetDebugName(TinyString::from_fmt("Texture Cache {}x{} {} Readback",
		new_width, new_height, GSTexture::GetFormatName(format)));
#endif

	return true;
}

void GSTextureCache::InvalidateContainedTargets(u32 start_bp, u32 end_bp, u32 write_psm)
{
	const bool preserve_alpha = (GSLocalMemory::m_psm[write_psm].trbpp == 24);
	for (int type = 0; type < 2; type++)
	{
		auto& list = m_dst[type];
		for (auto i = list.begin(); i != list.end();)
		{
			Target* const t = *i;
			if (start_bp != t->m_TEX0.TBP0 && (t->m_TEX0.TBP0 < start_bp || t->UnwrappedEndBlock() > end_bp))
			{
				++i;
				continue;
			}

			InvalidateSourcesFromTarget(t);

			t->m_valid_alpha_low &= preserve_alpha;
			t->m_valid_alpha_high &= preserve_alpha;
			t->m_valid_rgb = false;

			// Don't keep partial depth buffers around.
			if ((!t->m_valid_alpha_low && !t->m_valid_alpha_high && !t->m_valid_rgb) || type == DepthStencil)
			{
				auto& rev_list = m_dst[1 - type];
				for (auto j = rev_list.begin(); j != rev_list.end();)
				{
					Target* const rev_t = *j;
					if (rev_t->m_TEX0.TBP0 == t->m_TEX0.TBP0 && GSLocalMemory::m_psm[rev_t->m_TEX0.PSM].bpp == GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp)
					{
						rev_t->m_was_dst_matched = false;
						break;
					}
					++j;
				}

				GL_CACHE("TC: InvalidateContainedTargets: Remove Target %s[%x, %s]", to_string(type), t->m_TEX0.TBP0, psm_str(t->m_TEX0.PSM));
				i = list.erase(i);
				delete t;
				continue;
			}

			GL_CACHE("TC: InvalidateContainedTargets: Clear RGB valid on %s[%x, %s]", to_string(type), t->m_TEX0.TBP0, psm_str(t->m_TEX0.PSM));
			++i;
		}
	}
}

// Goal: Depth And Target at the same address is not possible. On GS it is
// the same memory but not on the Dx/GL. Therefore a write to the Depth/Target
// must invalidate the Target/Depth respectively
void GSTextureCache::InvalidateVideoMemType(int type, u32 bp, u32 write_psm, u32 write_fbmsk, bool dirty_only)
{
	auto& list = m_dst[type];
	for (auto i = list.begin(); i != list.end(); ++i)
	{
		Target* const t = *i;
		if (bp != t->m_TEX0.TBP0 || (dirty_only && t->m_dirty.empty()))
			continue;

		const GSLocalMemory::psm_t& psm_s = GSLocalMemory::m_psm[write_psm];
		const bool new_valid_alpha_low = t->m_valid_alpha_low && (psm_s.trbpp == 24 || (psm_s.trbpp == 32 && (write_fbmsk & 0x0F000000) == 0x0F000000));
		const bool new_valid_alpha_high = t->m_valid_alpha_high && (psm_s.trbpp == 24 || (psm_s.trbpp == 32 && (write_fbmsk & 0xF0000000) == 0xF0000000));
		const bool new_valid_rgb = t->m_valid_rgb && (psm_s.trbpp >= 24 && (write_fbmsk & 0x00FFFFFF) == 0x00FFFFFF);

		// Don't bother trying to keep partial depth buffers around.
		if ((new_valid_alpha_low || new_valid_alpha_high || new_valid_rgb) &&
			(type != DepthStencil || (GSLocalMemory::m_psm[t->m_TEX0.PSM].trbpp == 24 && new_valid_rgb)))
		{
			if (t->m_valid_alpha_low != new_valid_alpha_low || t->m_valid_alpha_high != new_valid_alpha_high || t->m_valid_rgb != new_valid_rgb)
			{
				GL_CACHE("TC: InvalidateVideoMemType: Partial Remove Target(%s) (0x%x) RGB: %s->%s, Alow: %s->%s Ahigh: %s->%s",
					to_string(type), t->m_TEX0.TBP0, t->m_valid_rgb ? "valid" : "invalid", new_valid_rgb ? "valid" : "invalid",
					t->m_valid_alpha_low ? "valid" : "invalid", new_valid_alpha_low ? "valid" : "invalid",
					t->m_valid_alpha_high ? "valid" : "invalid", new_valid_alpha_high ? "valid" : "invalid");
			}

			t->m_valid_alpha_low = new_valid_alpha_low;
			t->m_valid_alpha_high = new_valid_alpha_high;
			t->m_valid_rgb = new_valid_rgb;
			break;
		}


		GL_CACHE("TC: InvalidateVideoMemType: Remove Target(%s) (0x%x)", to_string(type),
			t->m_TEX0.TBP0);

		// Need to also remove any sources which reference this target.
		InvalidateSourcesFromTarget(t);

		// If we dst_matched and copied, no need to keep it marked as a copy if the original no longer exists.
		auto& rev_list = m_dst[1 - type];
		for (auto j = rev_list.begin(); j != rev_list.end();)
		{
			Target* const rev_t = *j;
			if (rev_t->m_TEX0.TBP0 == t->m_TEX0.TBP0 && GSLocalMemory::m_psm[rev_t->m_TEX0.PSM].bpp == GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp)
			{
				rev_t->m_was_dst_matched = false;
				break;
			}
			++j;
		}

		list.erase(i);
		delete t;
		break;
	}
}

// Goal: invalidate data sent to the GPU when the source (GS memory) is modified
// Called each time you want to write to the GS memory
void GSTextureCache::InvalidateVideoMem(const GSOffset& off, const GSVector4i& rect, bool target)
{
	const u32 bp = off.bp();
	const u32 bw = off.bw();
	const u32 psm = off.psm();

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
				(GSUtil::HasSharedBits(bp, psm, s->m_from_target_TEX0.TBP0, s->m_TEX0.PSM) && s->m_target))
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
	}

	bool found = false;
	// Previously: rect.ralign<Align_Outside>((bp & 31) == 0 ? GSLocalMemory::m_psm[psm].pgs : GSLocalMemory::m_psm[psm].bs)
	// But this causes rects to be too big, especially in WRC games, I don't think there's any need to align them here.
	GSVector4i r = rect;

	off.loopPages(rect, [this, &rect, bp, bw, psm, &found](u32 page)
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

	// Get the bounds that we're invalidating in blocks, so we can remove any targets which are completely contained.
	// Unfortunately sometimes the draw rect is incorrect, and since the end block gets the rect -1, it'll underflow,
	// so we need to prevent that from happening. Just make it a single block in that case, and hope for the best.
	const u32 start_bp = GSLocalMemory::GetStartBlockAddress(off.bp(), off.bw(), off.psm(), rect);
	const u32 end_bp = rect.rempty() ? start_bp : GSLocalMemory::GetUnwrappedEndBlockAddress(off.bp(), off.bw(), off.psm(), rect);

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

			++i;

			if (GSUtil::HasSharedBits(psm, t->m_TEX0.PSM))
			{
				if (bp == t->m_TEX0.TBP0)
				{
					if (GSUtil::HasCompatibleBits(psm, t->m_TEX0.PSM) && bw == std::max(t->m_TEX0.TBW, 1U))
					{
						GL_CACHE("TC: Dirty Target(%s) (0x%x) r(%d,%d,%d,%d)", to_string(type),
							t->m_TEX0.TBP0, r.x, r.y, r.z, r.w);

						if (GSLocalMemory::m_psm[psm].depth)
							DirtyRectByPage(bp, psm, bw, t, r);
						else
							AddDirtyRectTarget(t, r, psm, bw, rgba);

						if (FullRectDirty(t))
						{
							InvalidateSourcesFromTarget(t);
							i = list.erase(j);
							GL_CACHE("TC: Remove Target(%s) (0x%x)", to_string(type),
								t->m_TEX0.TBP0);
							delete t;
						}

						continue;
					}
				}

				if (t->Overlaps(bp, bw, psm, r))
				{
					// Try the hard way for partial invalidation.
					DirtyRectByPage(bp, psm, bw, t, r);

					if (FullRectDirty(t, rgba._u32))
					{
						InvalidateSourcesFromTarget(t);
						i = list.erase(j);
						GL_CACHE("TC: Remove Target(%s) (0x%x)", to_string(type),
							t->m_TEX0.TBP0);
						delete t;
					}
				}
			}
			// This is a situation where it is uploading in to the alpha channel but that is not part of the mask for the target format.
			// So we need to make sure the alpha is not marked as valid. (Juiced does a shuffle on the Z24 depth, making the alpha valid data).
			else if (GSUtil::GetChannelMask(psm) == 0x8 && GSUtil::GetChannelMask(t->m_TEX0.PSM) == 0x7 && t->Overlaps(bp, bw, psm, r))
			{
				t->m_valid_alpha_high &= !(psm == PSMT8H || psm == PSMT4HH);
				t->m_valid_alpha_low &= !(psm == PSMT8H || psm == PSMT4HL);
			}
		}
	}
}

// Goal: retrive the data from the GPU to the GS memory.
// Called each time you want to read from the GS memory.
// full_flush is set when it's a Local->Local stransfer and both src and destination are the same.
void GSTextureCache::InvalidateLocalMem(const GSOffset& off, const GSVector4i& r, bool full_flush)
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

				if (t->m_32_bits_fmt && t->m_TEX0.PSM > PSMZ24)
					t->m_TEX0.PSM = PSMZ32;

				// Check the offset of the read, if they're not pointing at or inside this texture, it's probably not what we want.
				//const bool expecting_this_tex = ((bp <= t->m_TEX0.TBP0 && read_start >= t->m_TEX0.TBP0) || bp >= t->m_TEX0.TBP0) && read_end <= t->m_end_block;
				const bool bpp_match = GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp == GSLocalMemory::m_psm[psm].bpp;
				const u32 page_mask = ((1 << 5) - 1);
				const bool expecting_this_tex = bpp_match && (((read_start & ~page_mask) == t->m_TEX0.TBP0) || (bp >= t->m_TEX0.TBP0 && ((read_end + page_mask) & ~page_mask) <= ((t->m_end_block + page_mask) & ~page_mask)));
				if (!expecting_this_tex)
					continue;

				t->readbacks_since_draw++;

				GSVector2i page_size = GSLocalMemory::m_psm[t->m_TEX0.PSM].pgs;
				const bool can_translate = CanTranslate(bp, bw, psm, r, t->m_TEX0.TBP0, t->m_TEX0.PSM, t->m_TEX0.TBW);
				const bool swizzle_match = GSLocalMemory::m_psm[psm].depth == GSLocalMemory::m_psm[t->m_TEX0.PSM].depth;
				// Calculate the rect offset if the BP doesn't match.
				GSVector4i targetr = {};
				if (full_flush || t->readbacks_since_draw > 1)
				{
					targetr = t->m_drawn_since_read;
				}
				else if (can_translate)
				{
					if (swizzle_match)
					{
						targetr = TranslateAlignedRectByPage(t, bp, psm, bw, r, true);
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
						targetr = TranslateAlignedRectByPage(t, bp & ~((1 << 5) - 1), psm, bw, new_rect, true);
					}
				}
				else
				{
					targetr = t->m_drawn_since_read;
				}

				const GSVector4i draw_rect = (t->readbacks_since_draw > 1) ? t->m_drawn_since_read : targetr.rintersect(t->m_drawn_since_read);
				const GSVector4i dirty_rect = t->m_dirty.GetTotalRect(t->m_TEX0, t->m_unscaled_size);
				// Recently made this section dirty, no need to read it.
				if (draw_rect.rintersect(dirty_rect).eq(draw_rect))
					return;

				if (t->m_drawn_since_read.eq(GSVector4i::zero()))
				{
					if (draw_rect.rintersect(t->m_valid).eq(draw_rect))
						return;
					else
						continue;
				}

				if (!draw_rect.rempty())
				{
					// The draw rect and read rect overlap somewhat, we should update the target before downloading it.
					if (t->m_TEX0.TBP0 == bp && !dirty_rect.rintersect(targetr).rempty())
						t->Update();

					Read(t, draw_rect);

					// Getaway (J) stores a Z texture at 0x2800 which it uses and the next frame it stores the reflection map in
					// 0x2800, so this will misdetect. So if it's not expecting a Z, check for RT's too.
					z_found = read_start >= t->m_TEX0.TBP0 && read_end <= t->m_end_block && GSLocalMemory::m_psm[psm].depth == GSLocalMemory::m_psm[t->m_TEX0.PSM].depth;

					if (draw_rect.rintersect(t->m_drawn_since_read).eq(t->m_drawn_since_read))
						t->m_drawn_since_read = GSVector4i::zero();
				}
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

			if (t->m_32_bits_fmt && t->m_TEX0.PSM > PSMCT24)
				t->m_TEX0.PSM = PSMCT32;

			const bool exact_bp = t->m_TEX0.TBP0 == bp;
			// pass 0 == Exact match, pass 1 == partial match
			if (pass == 0)
			{
				// Check exact match first
				const bool bpp_match = GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp == GSLocalMemory::m_psm[psm].bpp;
				const u32 page_mask = ((1 << 5) - 1);
				const bool exact_mem_match = (read_start & ~page_mask) == (t->m_TEX0.TBP0 & ~page_mask) && ((read_end + page_mask) & ~page_mask) == ((t->m_end_block + page_mask) & ~page_mask);
				const bool expecting_this_tex = exact_mem_match || (bpp_match && bw == t->m_TEX0.TBW && (((read_start & ~page_mask) == t->m_TEX0.TBP0) || (bp >= t->m_TEX0.TBP0 && ((read_end + page_mask) & ~page_mask) <= ((t->m_end_block + page_mask) & ~page_mask))));

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
			if (full_flush || t->readbacks_since_draw > 1)
			{
				targetr = t->m_drawn_since_read;
			}
			else if (can_translate)
			{
				if (swizzle_match)
				{
					targetr = TranslateAlignedRectByPage(t, bp, psm, bw, r, true);
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
					targetr = TranslateAlignedRectByPage(t, bp & ~((1 << 5) - 1), psm, bw, new_rect, true);
				}
			}
			else
			{
				targetr = t->m_drawn_since_read;
			}

			if (t->m_drawn_since_read.eq(GSVector4i::zero()))
			{
				if (targetr.rintersect(t->m_valid).eq(targetr) && exact_bp)
					return;
				else
					continue;
			}

			const GSVector4i dirty_rect = t->m_dirty.GetTotalRect(t->m_TEX0, t->m_unscaled_size);
			// Recently made this section dirty, no need to read it.
			if (targetr.rintersect(dirty_rect).eq(targetr))
			{
				if (exact_bp)
					return;
				else
					continue;
			}

			// Need to check the drawn area first.
			// Shadow Hearts From the New World tries to double buffer, then because we don't support rendering inside an RT, it makes a new one.
			// This makes sure any misdetection doesn't accidentally break the drawn area or skip download completely.
			targetr = targetr.rintersect(t->m_drawn_since_read);

			if (!targetr.rempty())
			{
				// We can skip the download if all pages that are read from are not dirty.
				if (!t->m_dirty.empty())
				{
					bool only_in_dirty_area = true;
					off.pageLooperForRect(r).loopPagesWithBreak([t, &dirty_rect, &only_in_dirty_area](u32 page) {
						const GSVector4i page_rect = GSLocalMemory::GetRectForPageOffset(t->m_TEX0.TBP0,
							page * BLOCKS_PER_PAGE, t->m_TEX0.TBW, t->m_TEX0.PSM);
						if (!dirty_rect.rintersect(page_rect).eq(page_rect))
						{
							only_in_dirty_area = false;
							return false;
						}
						return true;
					});

					if (only_in_dirty_area)
					{
						if (exact_bp)
							return;
						else
							continue;
					}
				}

				if (GSConfig.HWDownloadMode != GSHardwareDownloadMode::Enabled)
				{
					DevCon.Error("Skipping depth readback of %ux%u @ %u,%u", targetr.width(), targetr.height(), targetr.left, targetr.top);
					continue;
				}

				// The draw rect and read rect overlap somewhat, we should update the target before downloading it.
				if (exact_bp && !dirty_rect.rintersect(targetr).rempty())
					t->Update();

				Read(t, targetr);

				// Try to cut down how much we read next, if we can.
				// Fatal Frame reads in vertical strips, SOCOM 2 does horizontal, so we can handle that below.
				if (t->m_drawn_since_read.rintersect(targetr).eq(t->m_drawn_since_read))
				{
					t->m_drawn_since_read = GSVector4i::zero();
				}
				else if (targetr.xzxz().eq(t->m_drawn_since_read.xzxz())
					&& targetr.w >= t->m_drawn_since_read.y)
				{
					if (targetr.y <= t->m_drawn_since_read.y)
						t->m_drawn_since_read.y = targetr.w;
					else if (targetr.w >= t->m_drawn_since_read.w)
						t->m_drawn_since_read.w = targetr.y;
				}
				else if (targetr.ywyw().eq(t->m_drawn_since_read.ywyw())
					&& targetr.z >= t->m_drawn_since_read.x)
				{
					if (targetr.x <= t->m_drawn_since_read.x)
						t->m_drawn_since_read.x = targetr.z;
					else if (targetr.z >= t->m_drawn_since_read.z)
						t->m_drawn_since_read.z = targetr.x;
				}

				if (exact_bp && read_end <= t->m_end_block)
					return;
			}
		}
	}
}

bool GSTextureCache::Move(u32 SBP, u32 SBW, u32 SPSM, int sx, int sy, u32 DBP, u32 DBW, u32 DPSM, int dx, int dy, int w, int h)
{
	if (SBP == DBP && SPSM == DPSM && !GSLocalMemory::m_psm[SPSM].depth && ShuffleMove(SBP, SBW, SPSM, sx, sy, dx, dy, w, h))
		return true;

	if (SPSM == DPSM && SBW == 1 && SBW == DBW && PageMove(SBP, DBP, SBW, SPSM, sx, sy, dx, dy, w, h))
		return true;

	bool alpha_only = false;

	if (SPSM == PSMT8H && SPSM == DPSM)
		alpha_only = true;

	// TODO: In theory we could do channel swapping on the GPU, but we haven't found anything which needs it so far.
	// Not even going to go down the rabbit hole of palette formats on the GPU.. We shouldn't have any targets with P4/P8 anyway.
	const GSLocalMemory::psm_t& spsm_s = GSLocalMemory::m_psm[SPSM];
	const GSLocalMemory::psm_t& dpsm_s = GSLocalMemory::m_psm[DPSM];
	if (SPSM != DPSM || ((spsm_s.pal + dpsm_s.pal) != 0 && !alpha_only))
	{
		GL_CACHE("Skipping HW move from 0x%X to 0x%X with SPSM=%s DPSM=%s", SBP, DBP, psm_str(SPSM), psm_str(DPSM));
		return false;
	}

	// DX11/12 is a bit lame and can't partial copy depth targets. We could do this with a blit instead,
	// but so far haven't seen anything which needs it.
	const GSRendererType renderer = GSGetCurrentRenderer();
	if (renderer == GSRendererType::DX11 || renderer == GSRendererType::DX12)
	{
		if (spsm_s.depth || dpsm_s.depth)
			return false;
	}

	if (m_expected_src_bp == static_cast<int>(SBP) && m_expected_dst_bp == static_cast<int>(DBP))
	{
		// Get the new position so we can work out the offset.
		GSVector4i rect_offset = TranslateAlignedRectByPage(m_remembered_src_bp, m_remembered_src_bp + 1, SBW, SPSM, SBP, SPSM, SBW, GSVector4i(sx, sy, sx + w, sy + h), false);
		rect_offset.x = rect_offset.x - sx;
		rect_offset.y = rect_offset.y - sy;
		sx += rect_offset.x;
		sy += rect_offset.y;
		dx += rect_offset.x;
		dy += rect_offset.y;

		GL_INS("Detected striped move, realigning from SBP %x->%x DBP %x->%x", SBP, m_remembered_src_bp, DBP, m_remembered_dst_bp);

		SBP = m_remembered_src_bp;
		DBP = m_remembered_dst_bp;

		m_expected_src_bp = -1;
		m_remembered_src_bp = -1;
		m_expected_dst_bp = -1;
		m_remembered_dst_bp = -1;
	}
	// Look for an exact match on the targets.
	GSTextureCache::Target* src = GetExactTarget(SBP, SBW, spsm_s.depth ? DepthStencil : RenderTarget, SBP);
	GSTextureCache::Target* dst = GetExactTarget(DBP, DBW, dpsm_s.depth ? DepthStencil : RenderTarget, DBP);

	if (alpha_only && (!dst || GSLocalMemory::m_psm[dst->m_TEX0.PSM].bpp != 32))
		return false;

	// Beware of the case where a game might create a larger texture by moving a bunch of chunks around.
	// We use dx/dy == 0 and the TBW check as a safeguard to make sure these go through to local memory.
	// Good test case for this is the Xenosaga I cutscene transitions, or Gradius V.
	if (src && !dst && dx == 0 && dy == 0 && ((static_cast<u32>(w) + 63) / 64) <= DBW)
	{
		GIFRegTEX0 new_TEX0 = {};
		new_TEX0.TBP0 = DBP;
		new_TEX0.TBW = DBW;
		new_TEX0.PSM = DPSM;

		const GSVector2i target_size = GetTargetSize(DBP, DBW, DPSM, Common::AlignUpPow2(w, 64), h);
		dst = LookupTarget(new_TEX0, target_size, src->m_scale, src->m_type);
		if (!dst)
			dst = CreateTarget(new_TEX0, target_size, target_size, src->m_scale, src->m_type);
		if (!dst)
			return false;

		dst->UpdateValidity(GSVector4i(dx, dy, dx + w, dy + h));
		dst->OffsetHack_modxy = src->OffsetHack_modxy;
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
	dst->Update();

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
		GetTargetSize(DBP, DBW, DPSM, 0, new_height);

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
	GL_CACHE("HW Move 0x%x[BW:%u PSM:%s] to 0x%x[BW:%u PSM:%s] <%d,%d->%d,%d> -> <%d,%d->%d,%d>", SBP, SBW,
		psm_str(SPSM), DBP, DBW, psm_str(DPSM), sx, sy, sx + w, sy + h, dx, dy, dx + w, dy + h);

	// If the copies overlap, this is a validation error, so we need to copy to a temporary texture first.
	if ((SBP == DBP) && !(GSVector4i(sx, sy, sx + w, sy + h).rintersect(GSVector4i(dx, dy, dx + w, dy + h))).rempty())
	{
		GSTexture* tmp_texture = src->m_texture->IsDepthStencil() ?
									 g_gs_device->CreateDepthStencil(src->m_texture->GetWidth(), src->m_texture->GetHeight(), src->m_texture->GetFormat(), false) :
									 g_gs_device->CreateRenderTarget(src->m_texture->GetWidth(), src->m_texture->GetHeight(), src->m_texture->GetFormat(), false);
		if (!tmp_texture)
		{
			Console.Error("(HW Move) Failed to allocate temporary %dx%d texture on HW move", w, h);
			return false;
		}

		if(tmp_texture->IsDepthStencil())
		{
			const GSVector4 src_rect = GSVector4(scaled_sx, scaled_sy, scaled_sx + scaled_w, scaled_sy + scaled_h);
			const GSVector4 tmp_rect = src_rect / (GSVector4(tmp_texture->GetSize()).xyxy());
			const GSVector4 dst_rect = GSVector4(scaled_dx, scaled_dy, (scaled_dx + scaled_w), (scaled_dy + scaled_h));
			g_gs_device->StretchRect(src->m_texture, tmp_rect, tmp_texture, src_rect, ShaderConvert::DEPTH_COPY, false);
			g_gs_device->StretchRect(tmp_texture, tmp_rect, dst->m_texture, dst_rect, ShaderConvert::DEPTH_COPY, false);
		}
		else
		{
			if (SPSM == PSMT8H && SPSM == DPSM)
			{
				const GSVector4 src_rect = GSVector4(scaled_sx, scaled_sy, scaled_sx + scaled_w, scaled_sy + scaled_h);
				const GSVector4 tmp_rect = src_rect / (GSVector4(tmp_texture->GetSize()).xyxy());
				const GSVector4 dst_rect = GSVector4(scaled_dx, scaled_dy, (scaled_dx + scaled_w), (scaled_dy + scaled_h));
				g_gs_device->StretchRect(src->m_texture, tmp_rect, tmp_texture, src_rect, false, false, false, true);
				g_gs_device->StretchRect(tmp_texture, tmp_rect, dst->m_texture, dst_rect, false, false, false, true);
			}
			else
			{
				const GSVector4i src_rect = GSVector4i(scaled_sx, scaled_sy, scaled_sx + scaled_w, scaled_sy + scaled_h);
				// Fast memcpy()-like path for color targets.
				g_gs_device->CopyRect(src->m_texture, tmp_texture,
					src_rect,
					scaled_sx, scaled_sy);

				g_gs_device->CopyRect(tmp_texture, dst->m_texture,
					src_rect,
					scaled_dx, scaled_dy);
			}
		}

		g_gs_device->Recycle(tmp_texture);
	}
	else
	{
		if (SPSM == PSMT8H && SPSM == DPSM)
		{
			const GSVector4 src_rect = GSVector4(scaled_sx, scaled_sy, scaled_sx + scaled_w, scaled_sy + scaled_h) / (GSVector4(src->m_texture->GetSize()).xyxy());
			const GSVector4 dst_rect = GSVector4(scaled_dx, scaled_dy, (scaled_dx + scaled_w), (scaled_dy + scaled_h));
			g_gs_device->StretchRect(src->m_texture, src_rect, dst->m_texture, dst_rect, false, false, false, true);
		}
		else
		{
			g_gs_device->CopyRect(src->m_texture, dst->m_texture,
				GSVector4i(scaled_sx, scaled_sy, scaled_sx + scaled_w, scaled_sy + scaled_h),
				scaled_dx, scaled_dy);
		}
	}

	// You'd think we'd update to use the source valid bits, but it's not, because it might be copying some data which was uploaded and dirtied the target.
	// An example of this is Cross Channel - To All People where it renders a picture with 0x7f000000 FBMSK at 0x1180, which was all cleared to black on boot,
	// Then it moves it to 0x2e80, where some garbage has been loaded underneath, so we can't assume that's the only valid data.
	// We need to be cautious of the validity of the channels vs the format it's using, you don't want to set the alpha to true if it's only copying RGB.
	if (GSUtil::GetChannelMask(DPSM) & 0x7)
		dst->m_valid_rgb |= src->m_valid_rgb;

	if (GSUtil::GetChannelMask(DPSM) & 0x8)
	{
		if(DPSM != PSMT4HH)
			dst->m_valid_alpha_low |= src->m_valid_alpha_low;
		if (DPSM != PSMT4HL)
			dst->m_valid_alpha_high |= src->m_valid_alpha_high;
		dst->m_alpha_max = src->m_alpha_max;
		dst->m_alpha_min = src->m_alpha_min;
	}

	if (GSLocalMemory::IsPageAligned(src->m_TEX0.PSM, GSVector4i(sx, sy, sx + w, sy + h)) && (w == GSLocalMemory::m_psm[src->m_TEX0.PSM].pgs.x || h == GSLocalMemory::m_psm[src->m_TEX0.PSM].pgs.y))
	{
		// Vertical Strips
		if (w == GSLocalMemory::m_psm[src->m_TEX0.PSM].pgs.x)
		{
			m_expected_src_bp = GSLocalMemory::GetStartBlockAddress(src->m_TEX0.TBP0, src->m_TEX0.TBW, src->m_TEX0.PSM, GSVector4i(sx + w, 0, sx + w + w, h));
			m_expected_dst_bp = GSLocalMemory::GetStartBlockAddress(dst->m_TEX0.TBP0, dst->m_TEX0.TBW, dst->m_TEX0.PSM, GSVector4i(dx + w, 0, dx + w + w, h));
		}
		else
		{
			m_expected_src_bp = GSLocalMemory::GetStartBlockAddress(src->m_TEX0.TBP0, src->m_TEX0.TBW, src->m_TEX0.PSM, GSVector4i(0, sy + h, w, sy + h + h));
			m_expected_dst_bp = GSLocalMemory::GetStartBlockAddress(dst->m_TEX0.TBP0, dst->m_TEX0.TBW, dst->m_TEX0.PSM, GSVector4i(0, dy + h, w, dy + h + h));
		}
		
		if (static_cast<u32>(m_expected_src_bp) < src->UnwrappedEndBlock() && static_cast<u32>(m_expected_src_bp) >= src->m_TEX0.TBP0)
		{
			m_remembered_src_bp = src->m_TEX0.TBP0;
			m_remembered_dst_bp = dst->m_TEX0.TBP0;
		}
		else // If the expected BP is not inside the source, don't bother
		{
			m_expected_src_bp = -1;
			m_remembered_src_bp = -1;
			m_expected_dst_bp = -1;
			m_remembered_dst_bp = -1;
		}
	}
	dst->UpdateValidity(GSVector4i(dx, dy, dx + w, dy + h));
	dst->UpdateDrawn(GSVector4i(dx, dy, dx + w, dy + h));
	// Invalidate any sources that overlap with the target (since they're now stale).
	InvalidateVideoMem(g_gs_renderer->m_mem.GetOffset(DBP, DBW, DPSM), GSVector4i(dx, dy, dx + w, dy + h), false);
	return true;
}

bool GSTextureCache::ShuffleMove(u32 BP, u32 BW, u32 PSM, int sx, int sy, int dx, int dy, int w, int h)
{
	// What are we doing here? Final Fantasy XII uses moves to copy the contents of the RG channels to the BA channels,
	// by rendering in PSMCT32, and doing a PSMCT16 move with an 8x0 offset on the destination. This effectively reads
	// from the original red/green channels, and writes to the blue/alpha channels. Who knows why they did it this way,
	// when they could've used sprites, but it means that they had to offset the block pointer for each move. So, we
	// need to use tex-in-rt here to figure out what the offset into the original PSMCT32 texture was, and HLE the move.
	if (PSM != PSMCT16)
		return false;

	GL_CACHE("Trying ShuffleMove: BP=%04X BW=%u PSM=%u SX=%d SY=%d DX=%d DY=%d W=%d H=%d", BP, BW, PSM, sx, sy, dx, dy, w, h);

	GSTextureCache::Target* tgt = nullptr;
	for (auto t : m_dst[RenderTarget])
	{
		if (t->m_TEX0.PSM == PSMCT32 && BP >= t->m_TEX0.TBP0 && BP <= t->m_end_block)
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

	const GSVector4i bbox = write_rg ? GSVector4i(dx, dy, dx + w, dy + h) : GSVector4i(sx, sy, sx + w, sy + h);

	GSHWDrawConfig& config = GSRendererHW::GetInstance()->BeginHLEHardwareDraw(tgt->m_texture, nullptr, tgt->m_scale, tgt->m_texture, tgt->m_scale, bbox);
	config.colormask.wrgba = (write_rg ? (1 | 2) : (4 | 8));
	config.ps.read_ba = read_ba;
	config.ps.write_rg = write_rg;
	config.ps.shuffle = true;
	GSRendererHW::GetInstance()->EndHLEHardwareDraw(false);
	return true;
}

bool GSTextureCache::PageMove(u32 SBP, u32 DBP, u32 BW, u32 PSM, int sx, int sy, int dx, int dy, int w, int h)
{
	// Only supports 1-wide at the moment.
	pxAssert(BW == 1);

	const GSVector4i src = GSVector4i(sx, sy, sx + w, sy + h);
	const GSVector4i drc = GSVector4i(dx, dy, dx + w, dy + h);
	if (!GSLocalMemory::IsPageAligned(PSM, src) || !GSLocalMemory::IsPageAligned(PSM, drc))
		return false;

	// How many pages are we dealing with?
	const GSVector2i& pgs = GSLocalMemory::m_psm[PSM].pgs;
	const u32 num_pages = (h / pgs.y) * BW;
	const u32 src_page_offset = ((sy / pgs.y) * BW) + (sx / pgs.x);
	const u32 src_block_end = SBP + (((src_page_offset + num_pages) * BLOCKS_PER_PAGE) - 1);
	const u32 dst_page_offset = ((dy / pgs.y) * BW) + (dx / pgs.x);
	const u32 dst_block_end = DBP + (((dst_page_offset + num_pages) * BLOCKS_PER_PAGE) - 1);
	pxAssert(num_pages > 0);
	GL_PUSH("GSTextureCache::PageMove(): %u pages, with offset of %u src %u dst", num_pages, src_page_offset,
		dst_page_offset);

	// Find our targets.
	Target* stgt = nullptr;
	Target* dtgt = nullptr;
	for (int type = 0; type < 2; type++)
	{
		for (Target* tgt : m_dst[type])
		{
			// We _could_ do compatible bits here maybe?
			if (tgt->m_TEX0.PSM != PSM)
				continue;

			// Check that the end block is in range. If it's not, we can't do this, and have to fall back to local memory.
			const u32 tgt_end = tgt->UnwrappedEndBlock();
			if (tgt->m_TEX0.TBP0 <= SBP && src_block_end <= tgt_end)
				stgt = tgt;
			if (tgt->m_TEX0.TBP0 <= DBP && dst_block_end <= tgt_end)
				dtgt = tgt;

			if (stgt && dtgt)
				break;
		}

		if (stgt && dtgt)
			break;
	}
	if (!stgt || !dtgt)
	{
		GL_INS("Targets not found.");
		return false;
	}

	// Double-check that we're not copying to a non-page-aligned target.
	if (((SBP - stgt->m_TEX0.TBP0) % BLOCKS_PER_PAGE) != 0 || ((DBP - dtgt->m_TEX0.TBP0) % BLOCKS_PER_PAGE) != 0)
	{
		GL_INS("Effective SBP of %x or DBP of %x is not page aligned.", SBP - stgt->m_TEX0.TBP0, DBP - dtgt->m_TEX0.TBP0);
		return false;
	}

	// Need to offset based on the target's actual BP.
	const u32 real_src_offset = ((SBP - stgt->m_TEX0.TBP0) / BLOCKS_PER_PAGE) + src_page_offset;
	const u32 real_dst_offset = ((DBP - dtgt->m_TEX0.TBP0) / BLOCKS_PER_PAGE) + dst_page_offset;
	CopyPages(stgt, stgt->m_TEX0.TBW, real_src_offset, dtgt, dtgt->m_TEX0.TBW, real_dst_offset, num_pages);
	return true;
}

void GSTextureCache::CopyPages(Target* src, u32 sbw, u32 src_offset, Target* dst, u32 dbw, u32 dst_offset, u32 num_pages, ShaderConvert shader)
{
	GL_PUSH("GSTextureCache::CopyPages(): %u pages at %x[eff %x] BW %u to %x[eff %x] BW %u", num_pages,
		src->m_TEX0.TBP0, src->m_TEX0.TBP0 + src_offset, sbw, dst->m_TEX0.TBP0, dst->m_TEX0.TBP0 + dst_offset, dbw);

	// Create rectangles for the pages.
	const GSVector2i& pgs = GSLocalMemory::m_psm[dst->m_TEX0.PSM].pgs;
	const GSVector4i page_rc = GSVector4i::loadh(pgs);
	const GSVector4 src_size = GSVector4(src->GetUnscaledSize()).xyxy();
	const GSVector4 dst_scale = GSVector4(dst->GetScale());
	GSDevice::MultiStretchRect* rects = static_cast<GSDevice::MultiStretchRect*>(alloca(sizeof(GSDevice::MultiStretchRect) * num_pages));
	for (u32 i = 0; i < num_pages; i++)
	{
		const u32 src_page_num = src_offset + i;
		const GSVector2i src_offset = GSVector2i((src_page_num % sbw) * pgs.x, (src_page_num / sbw) * pgs.y);
		const u32 dst_page_num = dst_offset + i;
		const GSVector2i dst_offset = GSVector2i((dst_page_num % dbw) * pgs.x, (dst_page_num / dbw) * pgs.y);

		const GSVector4i src_rect = page_rc + GSVector4i(src_offset).xyxy();
		const GSVector4i dst_rect = page_rc + GSVector4i(dst_offset).xyxy();

		GL_INS("Copy page %u @ <%d,%d=>%d,%d> to %u @ <%d,%d=>%d,%d>", src_page_num, src_rect.x, src_rect.y, src_rect.z,
			src_rect.w, dst_page_num, dst_rect.x, dst_rect.y, dst_rect.z, dst_rect.w);

		GSDevice::MultiStretchRect& rc = rects[i];
		rc.src = src->m_texture;
		rc.src_rect = GSVector4(src_rect) / src_size;
		rc.dst_rect = GSVector4(dst_rect) * dst_scale;
		rc.linear = false;
		rc.wmask.wrgba = 0xf;
	}

	// No need to sort here, it's all from the same texture.
	g_gs_device->DrawMultiStretchRects(rects, num_pages, dst->m_texture, shader);
}

GSTextureCache::Target* GSTextureCache::GetExactTarget(u32 BP, u32 BW, int type, u32 end_bp)
{
	auto& rts = m_dst[type];
	for (auto it = rts.begin(); it != rts.end(); ++it) // Iterate targets from MRU to LRU.
	{
		Target* t = *it;

		if (t->m_TEX0.TBP0 == BP && t->m_TEX0.TBW == BW && t->UnwrappedEndBlock() >= end_bp)
		{
			rts.MoveFront(it.Index());
			return t;
		}
	}

	return nullptr;
}

GSTextureCache::Target* GSTextureCache::GetTargetWithSharedBits(u32 BP, u32 PSM) const
{
	auto& rts = m_dst[GSLocalMemory::m_psm[PSM].depth ? DepthStencil : RenderTarget];
	for (auto it = rts.begin(); it != rts.end(); ++it) // Iterate targets from MRU to LRU.
	{
		Target* t = *it;
		const u32 t_psm = (t->HasValidAlpha()) ? t->m_TEX0.PSM & ~0x1 : t->m_TEX0.PSM;
		if (GSUtil::HasSharedBits(BP, PSM, t->m_TEX0.TBP0, t_psm))
			return t;
	}

	return nullptr;
}

GSTextureCache::Target* GSTextureCache::FindOverlappingTarget(u32 BP, u32 end_bp) const
{
	for (int i = 0; i < 2; i++)
	{
		for (Target* tgt : m_dst[i])
		{
			if (CheckOverlap(tgt->m_TEX0.TBP0, tgt->m_end_block, BP, end_bp))
				return tgt;
		}
	}

	return nullptr;
}

GSTextureCache::Target* GSTextureCache::FindOverlappingTarget(u32 BP, u32 BW, u32 PSM, GSVector4i rc) const
{
	const u32 end_bp = GSLocalMemory::GetUnwrappedEndBlockAddress(BP, BW, PSM, rc);
	return FindOverlappingTarget(BP, end_bp);
}

GSVector2i GSTextureCache::GetTargetSize(u32 bp, u32 fbw, u32 psm, s32 min_width, s32 min_height)
{
	TargetHeightElem search = {};
	search.bp = bp;
	search.fbw = fbw;
	search.psm = psm;
	search.width = min_width;
	search.height = min_height;

	for (auto it = m_target_heights.begin(); it != m_target_heights.end(); ++it)
	{
		TargetHeightElem& elem = const_cast<TargetHeightElem&>(*it);
		if (elem.bits == search.bits)
		{
			if (elem.width < min_width || elem.height < min_height)
			{
				DbgCon.WriteLn("Expand size at %x %u %u from %ux%u to %ux%u", bp, fbw, psm, elem.width, elem.height,
					min_width, min_height);
			}

			elem.width = std::max(elem.width, min_width);
			elem.height = std::max(elem.height, min_height);

			m_target_heights.MoveFront(it.Index());
			elem.age = 0;
			return GSVector2i(elem.width, elem.height);
		}
	}

	DbgCon.WriteLn("New size at %x %u %u: %ux%u", bp, fbw, psm, min_width, min_height);
	m_target_heights.push_front(search);
	return GSVector2i(min_width, min_height);
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

			// Need to also remove any sources which reference this target.
			InvalidateSourcesFromTarget(t);

			i = list.erase(i);
			delete t;
		}
		else
		{
			++i;
		}
	}
}

void GSTextureCache::InvalidateSourcesFromTarget(const Target* t)
{
	for (auto it = m_src.m_surfaces.begin(); it != m_src.m_surfaces.end();)
	{
		Source* src = *it++;
		if (src->m_from_target == t)
		{
			GL_CACHE("TC: Removing source at %x referencing target", src->m_TEX0.TBP0);
			m_src.RemoveAt(src);
		}
	}
}

void GSTextureCache::ReplaceSourceTexture(Source* s, GSTexture* new_texture, float new_scale,
	const GSVector2i& new_unscaled_size, HashCacheEntry* hc_entry, bool new_texture_is_shared)
{
	pxAssert(!hc_entry || !new_texture_is_shared);

	if (s->m_from_hash_cache)
	{
		pxAssert(s->m_from_hash_cache->refcount > 0);
		if ((--s->m_from_hash_cache->refcount) == 0)
			s->m_from_hash_cache->age = 0;
	}
	else if (!s->m_shared_texture)
	{
		m_source_memory_usage -= s->m_texture->GetMemUsage();
		g_gs_device->Recycle(s->m_texture);
	}

	s->m_texture = new_texture;
	s->m_shared_texture = new_texture_is_shared;
	s->m_from_hash_cache = hc_entry;
	s->m_unscaled_size = new_unscaled_size;
	s->m_scale = new_scale;

	if (s->m_from_hash_cache)
		s->m_from_hash_cache->refcount++;
	else if (!s->m_shared_texture)
		m_source_memory_usage += s->m_texture->GetMemUsage();
}

void GSTextureCache::IncAge()
{
	static constexpr int max_age = 3;
	static constexpr int max_preload_age = 30;

	// You can't use m_map[page] because Source* are duplicated on several pages.
	for (auto i = m_src.m_surfaces.begin(); i != m_src.m_surfaces.end();)
	{
		Source* s = *i;

		++i;
		if (++s->m_age > ((!s->m_from_hash_cache && s->CanPreload()) ? max_preload_age : max_age))
			m_src.RemoveAt(s);
	}

	AgeHashCache();

	// Clearing of Rendertargets causes flickering in many scene transitions.
	// Sigh, this seems to be used to invalidate surfaces. So set a huge maxage to avoid flicker,
	// but still invalidate surfaces. (Disgaea 2 fmv when booting the game through the BIOS)
	// Original maxage was 4 here, Xenosaga 2 needs at least 240, else it flickers on scene transitions.
	static constexpr int max_rt_age = 400; // ffx intro scene changes leave the old image untouched for a couple of frames and only then start using it

	// Toss and recompute sizes after 2 seconds of not being used. Should be sufficient for most loading screens.
	static constexpr int max_size_age = 120;

	for (int type = 0; type < 2; type++)
	{
		auto& list = m_dst[type];
		for (auto i = list.begin(); i != list.end();)
		{
			Target* t = *i;

			if (++t->m_age > max_rt_age)
			{
				i = list.erase(i);
				GL_CACHE("TC: Remove Target(%s): (0x%x) due to age", to_string(type),
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
		if (elem.age >= max_size_age)
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

	// For debugging, we have an option to force copies instead of sampling the target directly.
	static constexpr bool force_target_copy = false;

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
		tlevels = (GSConfig.HWMipmap != HWMipmapLevel::Full) ? -1 : std::min(lod->y - lod->x + 1, GSDevice::GetMipmapLevelsForSize(tw, th));
		src->m_lod = *lod;
	}

	bool hack = false;
	bool channel_shuffle = false;

	if (dst && (x_offset != 0 || y_offset != 0))
	{
		const float scale = dst->m_scale;
		const int x = static_cast<int>(scale * x_offset);
		const int y = static_cast<int>(scale * y_offset);
		const int w = static_cast<int>(std::ceil(scale * tw));
		const int h = static_cast<int>(std::ceil(scale * th));

		dst->Update();

		// If we have a source larger than the target (from tex-in-rt), texelFetch() for target region will return black.
		if constexpr (force_target_copy)
		{
			// If we have a source larger than the target, we need to clear it, otherwise we'll read junk
			const bool outside_target = ((x + w) > dst->m_texture->GetWidth() || (y + h) > dst->m_texture->GetHeight());
			GSTexture* sTex = dst->m_texture;
			GSTexture* dTex = outside_target ?
								  g_gs_device->CreateRenderTarget(w, h, GSTexture::Format::Color, true, PreferReusedLabelledTexture()) :
								  g_gs_device->CreateTexture(w, h, tlevels, GSTexture::Format::Color, PreferReusedLabelledTexture());
			if (!dTex) [[unlikely]]
			{
				Console.Error("Failed to allocate %dx%d texture for offset source", w, h);
				delete src;
				return nullptr;
			}

			m_source_memory_usage += dTex->GetMemUsage();

			// copy the rt in
			const GSVector4i area(GSVector4i(x, y, x + w, y + h).rintersect(GSVector4i(sTex->GetSize()).zwxy()));
			if (!area.rempty())
				g_gs_device->CopyRect(sTex, dTex, area, 0, 0);

			src->m_texture = dTex;
			src->m_unscaled_size = GSVector2i(tw, th);
		}
		else
		{
			GL_CACHE("TC: Sample offset (%d,%d) reduced region directly from target: %dx%d -> %dx%d @ %d,%d",
				dst->m_texture->GetWidth(), x_offset, y_offset, dst->m_texture->GetHeight(), w, h, x_offset, y_offset);

			if (x_offset < 0)
				src->m_region.SetX(x_offset, region.GetMaxX() + x_offset);
			else
				src->m_region.SetX(x_offset, x_offset + tw);
			if (y_offset < 0)
				src->m_region.SetY(y_offset, region.GetMaxY() + y_offset);
			else
				src->m_region.SetY(y_offset, y_offset + th);
			src->m_texture = dst->m_texture;
			src->m_unscaled_size = dst->m_unscaled_size;
			src->m_shared_texture = true;
		}

		// Invalidate immediately on recursive draws, because if we don't here, InvalidateVideoMem() will.
		if (GSRendererHW::GetInstance()->IsTBPFrameOrZ(dst->m_TEX0.TBP0))
			m_temporary_source = src;

		// Keep a trace of origin of the texture
		src->m_scale = scale;
		src->m_end_block = dst->m_end_block;
		src->m_target = true;
		src->m_from_target = dst;
		src->m_from_target_TEX0 = dst->m_TEX0;

		if ((src->m_TEX0.PSM & 0xf) == PSMCT24)
		{
			src->m_alpha_minmax.first = TEXA.AEM ? 0 : TEXA.TA0;
			src->m_alpha_minmax.second = TEXA.TA0;
		}
		else
		{
			src->m_alpha_minmax.first = dst->m_alpha_min;
			src->m_alpha_minmax.second = dst->m_alpha_max;

			if (!dst->m_32_bits_fmt)
			{
				const bool using_both = (src->m_alpha_minmax.first ^ src->m_alpha_minmax.second) & 128;
				const bool using_ta1 = (src->m_alpha_minmax.second & 128);

				src->m_alpha_minmax.first = TEXA.AEM ? 0 : (using_both ? std::min(TEXA.TA1, TEXA.TA0) : (using_ta1 ? TEXA.TA1 : TEXA.TA0));
				src->m_alpha_minmax.second = (using_both ? std::max(TEXA.TA1, TEXA.TA0) : (using_ta1 ? TEXA.TA1 : TEXA.TA0));
			}
		}
		src->m_32_bits_fmt = dst->m_32_bits_fmt;

		if (psm.pal > 0)
		{
			// Attach palette for GPU texture conversion
			AttachPaletteToSource(src, psm.pal, true);
		}

#ifdef PCSX2_DEVBUILD
		if (GSConfig.UseDebugDevice)
		{
			if (psm.pal > 0)
			{
				src->m_texture->SetDebugName(TinyString::from_fmt("Offset {},{} from 0x{:X} {} CBP 0x{:X}", x_offset, y_offset,
					static_cast<u32>(TEX0.TBP0), psm_str(TEX0.PSM), static_cast<u32>(TEX0.CBP)));
			}
			else
			{
				src->m_texture->SetDebugName(TinyString::from_fmt("Offset {},{} from 0x{:X} {} ", x_offset, y_offset,
					static_cast<u32>(TEX0.TBP0), psm_str(TEX0.PSM), static_cast<u32>(TEX0.CBP)));
			}
		}
#endif
	}
	else if (dst)
	{
		// TODO: clean up this mess

		ShaderConvert shader = dst->m_type != RenderTarget ? ShaderConvert::FLOAT32_TO_RGBA8 : ShaderConvert::COPY;
		channel_shuffle = GSRendererHW::GetInstance()->TestChannelShuffle(dst);

		const bool is_8bits = TEX0.PSM == PSMT8 && !channel_shuffle;
		if (is_8bits)
		{
			GL_INS("Reading RT as a packed-indexed 8 bits format");
			shader = ShaderConvert::RGBA_TO_8I;
		}

#ifdef ENABLE_OGL_DEBUG
		if (TEX0.PSM == PSMT4)
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
		src->m_from_target = dst;
		src->m_from_target_TEX0 = dst->m_TEX0;
		src->m_valid_rect = dst->m_valid;
		src->m_end_block = dst->m_end_block;

		// Do this first as we could be adding in alpha from an upgraded 24bit target.
		dst->Update();

		if ((src->m_TEX0.PSM & 0xf) == PSMCT24)
		{
			src->m_alpha_minmax.first = TEXA.AEM ? 0 : TEXA.TA0;
			src->m_alpha_minmax.second = TEXA.TA0;
		}
		else
		{
			src->m_alpha_minmax.first = dst->m_alpha_min;
			src->m_alpha_minmax.second = dst->m_alpha_max;

			if (!dst->m_32_bits_fmt)
			{
				const bool using_both = (src->m_alpha_minmax.first ^ src->m_alpha_minmax.second) & 128;
				const bool using_ta1 = (src->m_alpha_minmax.second & 128);

				src->m_alpha_minmax.first = TEXA.AEM ? 0 : (using_both ? std::min(TEXA.TA1, TEXA.TA0) : (using_ta1 ? TEXA.TA1 : TEXA.TA0));
				src->m_alpha_minmax.second = (using_both ? std::max(TEXA.TA1, TEXA.TA0) : (using_ta1 ? TEXA.TA1 : TEXA.TA0));
			}
		}
		src->m_32_bits_fmt = dst->m_32_bits_fmt;

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
			//int blockHeight = TEX0.PSM == PSMCT32 || TEX0.PSM == PSMCT24 ? 32 : 64;

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

		// Create a cleared RT if we somehow end up with an empty source rect (because the RT isn't large enough).
		const bool source_rect_empty = sRect.rempty();
		const bool use_texture = (shader == ShaderConvert::COPY && !source_rect_empty);
		GSVector4i region_rect = GSVector4i(0, 0, tw, th);

		if (half_right)
		{
			// You typically hit this code in snow engine game. Dstsize is the size of of Dx/GL RT
			// which is set to some arbitrary number. h/w are based on the input texture
			// so the only reliable way to find the real size of the target is to use the TBW value.
			const int half_width = static_cast<int>(dst->m_TEX0.TBW * (64 / 2));
			if (half_width < dst->m_unscaled_size.x)
			{
				const int copy_width = std::min(half_width, dst->m_unscaled_size.x - half_width);
				region_rect = GSVector4i(half_width, 0, half_width + copy_width, th);
				GL_CACHE("TC: Half right fix: %d,%d => %d,%d", region_rect.x, region_rect.y, region_rect.z, region_rect.w);

				sRect = sRect.blend32<5>(GSVector4i(GSVector4(region_rect.rintersect(dst->GetUnscaledRect())) * GSVector4(dst->m_scale)));
				new_size.x = sRect.width();
				src->m_unscaled_size.x = copy_width;
			}
			else
			{
				DevCon.Error("Invalid half-right copy with width %d from %dx%d texture", half_width * 2, dst->m_unscaled_size.x, dst->m_unscaled_size.y);
			}
		}

		// Assuming everything matches up, instead of copying the target, we can just sample it directly.
		// It's the same as doing the copy first, except we save GPU time.
		// TODO: We still need to copy if the TBW is mismatched. Except when TBW <= 1 (Jak 2).
		const GSVector2i dst_texture_size = dst->m_texture->GetSize();
		if ((!half_right || region_rect.z >= dst->m_unscaled_size.x) && // not a smaller subsample
			use_texture && // not reinterpreting the RT
			!force_target_copy)
		{
			// sample the target directly
			src->m_texture = dst->m_texture;
			src->m_scale = dst->m_scale;
			src->m_unscaled_size = dst->m_unscaled_size;
			src->m_shared_texture = true;
			src->m_32_bits_fmt = dst->m_32_bits_fmt;

			// if the size doesn't match, we need to engage shader sampling.
			if (new_size != dst_texture_size)
			{
				GL_CACHE("TC: Sample reduced region directly from target: %dx%d -> %dx%d", dst_texture_size.x,
					dst_texture_size.y, new_size.x, new_size.y);

				if (new_size.x != dst_texture_size.x)
					src->m_region.SetX(region_rect.x, region_rect.z);
				if (new_size.y != dst_texture_size.y)
					src->m_region.SetY(region_rect.y, region_rect.w);
			}

			// kill source immediately if it's the RT/DS, because that'll get invalidated immediately
			if (GSRendererHW::GetInstance()->IsTBPFrameOrZ(dst->m_TEX0.TBP0))
			{
				GL_CACHE("TC: Source is RT or ZBUF, invalidating after draw.");
				m_temporary_source = src;
			}
		}
		else
		{
			// Don't be fooled by the name. 'dst' is the old target (hence the input)
			// 'src' is the new texture cache entry (hence the output)
			GSTexture* sTex = dst->m_texture;
			GSTexture* dTex = use_texture ?
								  g_gs_device->CreateTexture(new_size.x, new_size.y, 1, GSTexture::Format::Color,
									  PreferReusedLabelledTexture()) :
								  g_gs_device->CreateRenderTarget(new_size.x, new_size.y, GSTexture::Format::Color,
									  source_rect_empty || destX != 0 || destY != 0, PreferReusedLabelledTexture());
			if (!dTex) [[unlikely]]
			{
				Console.Error("Failed to allocate %dx%d texture for target copy to source", new_size.x, new_size.y);
				delete src;
				return nullptr;
			}

			m_source_memory_usage += dTex->GetMemUsage();
			src->m_texture = dTex;

			if (use_texture)
			{
				g_gs_device->CopyRect(sTex, dTex, sRect, destX, destY);
				g_perfmon.Put(GSPerfMon::TextureCopies, 1);

#ifdef PCSX2_DEVBUILD
				if (GSConfig.UseDebugDevice)
				{
					src->m_texture->SetDebugName(TinyString::from_fmt("{}x{} copy of 0x{:X} {}", new_size.x, new_size.y,
						static_cast<u32>(TEX0.TBP0), psm_str(TEX0.PSM)));
				}
#endif
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

				g_perfmon.Put(GSPerfMon::TextureCopies, 1);

#ifdef PCSX2_DEVBUILD
				if (GSConfig.UseDebugDevice)
				{
					if (psm.pal > 0)
					{
						src->m_texture->SetDebugName(TinyString::from_fmt("Reinterpret 0x{:X} from {} to {} CBP 0x{:X}",
							static_cast<u32>(TEX0.TBP0), psm_str(dst->m_TEX0.PSM), psm_str(TEX0.PSM), static_cast<u32>(TEX0.CBP)));
					}
					else
					{
						src->m_texture->SetDebugName(TinyString::from_fmt("Reinterpret 0x{:X} from {} to {}",
							static_cast<u32>(TEX0.TBP0), psm_str(dst->m_TEX0.PSM), psm_str(TEX0.PSM)));
					}
				}
#endif
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
		dst->OffsetHack_modxy = hack ? g_gs_renderer->GetModXYOffset() : 0.0f;
	}
	else
	{
		if (GSUtil::GetChannelMask(TEX0.PSM) == 0xf && TEX0.TBP0 != GSRendererHW::GetInstance()->GetCachedCtx()->FRAME.Block() && TEX0.TBP0 != GSRendererHW::GetInstance()->GetCachedCtx()->ZBUF.Block())
		{
			// Kill any possible targets we missed, they might be wrong now.
			g_texture_cache->InvalidateVideoMemType(GSTextureCache::RenderTarget, TEX0.TBP0, TEX0.PSM, GSRendererHW::GetInstance()->GetCachedCtx()->FRAME.FBMSK, true);
			g_texture_cache->InvalidateVideoMemType(GSTextureCache::DepthStencil, TEX0.TBP0, TEX0.PSM, GSRendererHW::GetInstance()->GetCachedCtx()->FRAME.FBMSK, true);
		}

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
			src->m_alpha_minmax = src->m_from_hash_cache->alpha_minmax;

			if (gpu_clut)
				AttachPaletteToSource(src, gpu_clut);
			else if (psm.pal > 0)
				AttachPaletteToSource(src, psm.pal, paltex);
		}
		else if (paltex)
		{
			src->m_texture = g_gs_device->CreateTexture(tw, th, tlevels, GSTexture::Format::UNorm8);
			if (!src->m_texture) [[unlikely]]
			{
				Console.Error("Failed to allocate %dx%d paltex texture", tw, th);
				delete src;
				return nullptr;
			}

			m_source_memory_usage += src->m_texture->GetMemUsage();
			if (gpu_clut)
				AttachPaletteToSource(src, gpu_clut);
			else
				AttachPaletteToSource(src, psm.pal, true);
		}
		else
		{
			src->m_texture = g_gs_device->CreateTexture(tw, th, tlevels, GSTexture::Format::Color);
			if (!src->m_texture) [[unlikely]]
			{
				Console.Error("Failed to allocate %dx%d source texture", tw, th);
				delete src;
				return nullptr;
			}

			m_source_memory_usage += src->m_texture->GetMemUsage();
			if (gpu_clut)
				AttachPaletteToSource(src, gpu_clut);
			else if (psm.pal > 0)
				AttachPaletteToSource(src, psm.pal, false);
		}

#ifdef PCSX2_DEVBUILD
		if (GSConfig.UseDebugDevice)
		{
			if (psm.pal > 0)
			{
				src->m_texture->SetDebugName(TinyString::from_fmt("{}x{} {} @ 0x{:X} TBW={} CBP=0x{:X}",
					tw, th, psm_str(TEX0.PSM), static_cast<u32>(TEX0.TBP0), static_cast<u32>(TEX0.TBW),
					static_cast<u32>(TEX0.CBP)));
			}
			else
			{
				src->m_texture->SetDebugName(TinyString::from_fmt("{}x{} {} @ 0x{:X} TBW={}",
					tw, th, psm_str(TEX0.PSM), static_cast<u32>(TEX0.TBP0), static_cast<u32>(TEX0.TBW)));
			}
		}
#endif
	}

	pxAssert(src->m_texture);
	pxAssert(src->m_target == (dst != nullptr));
	pxAssert(src->m_from_target == dst);
	pxAssert(src->m_scale == ((!dst || (TEX0.PSM == PSMT8 && !channel_shuffle)) ? 1.0f : dst->m_scale));

	if (src != m_temporary_source)
	{
		src->SetPages();
		m_src.Add(src, TEX0);
	}

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
	auto preload_page = [&TEXA, scale, &psm, &lm_off, &lmtex, &lmtex_map, &lmtex_mapped,
		page_width, page_height, tex_width, tex_height, copy_queue, &copy_count](int dst_x, int dst_y) {
		if (!lmtex)
		{
			lmtex = g_gs_device->CreateTexture(tex_width, tex_height, 1, GSTexture::Format::Color, false);
			if (!lmtex) [[unlikely]]
			{
				Console.Error("Failed to allocate %dx%d texture for page preloading", tex_width, tex_height);
				return;
			}

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
			if (this_start_block >= t->m_TEX0.TBP0 && this_end_block <= t->m_end_block && GSUtil::HasCompatibleBits(t->m_TEX0.PSM, TEX0.PSM))
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
	if (!dtex) [[unlikely]]
	{
		Console.Error("Failed to allocate %dx%d merged dest texture", scaled_width, scaled_height);
		return nullptr;
	}

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
	const GSOffset offset = g_gs_renderer->m_mem.GetOffset(TEX0.TBP0, TEX0.TBW, TEX0.PSM);
	src->m_pages = offset.pageLooperForRect(GSVector4i(0, 0, tex_width, tex_height));
	m_src.Add(src, TEX0);

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
	bool can_cache = (TEX0.PSM >= PSMT8H && TEX0.PSM <= PSMT4HH) ? CanPreloadTextureSize(TEX0.TW, TEX0.TH) : CanCacheTextureSize(TEX0.TW, TEX0.TH);
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
		GL_CACHE("HC Hit: %" PRIx64 " %" PRIx64 " R-%" PRIx64, key.TEX0Hash, key.CLUTHash, key.region.bits);
		HashCacheEntry* entry = &it->second;
		paltex &= (entry->texture->GetFormat() == GSTexture::Format::UNorm8);
		entry->refcount++;
		return entry;
	}

	// cache miss.
	GL_CACHE("HC Miss: %" PRIx64 " %" PRIx64 " R-%" PRIx64, key.TEX0Hash, key.CLUTHash, key.region.bits);

	// check for a replacement texture with the full clut key
	if (replace)
	{
		bool replacement_texture_pending = false;
		std::pair<u8, u8> alpha_minmax;
		GSTexture* replacement_tex = GSTextureReplacements::LookupReplacementTexture(key, lod != nullptr,
			&replacement_texture_pending, &alpha_minmax);
		if (replacement_tex)
		{
			// found a replacement texture! insert it into the hash cache, and clear paltex (since it's not indexed)
			paltex = false;
			const HashCacheEntry entry{ replacement_tex, 1u, 0u, alpha_minmax, true };
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
	const int tlevels = lod ? ((GSConfig.HWMipmap != HWMipmapLevel::Full) ? -1 : std::min(lod->y - lod->x + 1, GSDevice::GetMipmapLevelsForSize(tw, th))) : 1;
	GSTexture* tex = g_gs_device->CreateTexture(tw, th, tlevels, paltex ? GSTexture::Format::UNorm8 : GSTexture::Format::Color);
	if (!tex)
	{
		// out of video memory if we hit here
		return nullptr;
	}

	// upload base level
	const bool is_direct = (GSLocalMemory::m_psm[TEX0.PSM].pal == 0);
	std::pair<u8, u8> alpha_minmax = {0u, 255u};
	PreloadTexture(TEX0, TEXA, region, g_gs_renderer->m_mem, paltex, tex, 0, is_direct ? &alpha_minmax : nullptr);

	// upload mips if present
	if (lod)
	{
		const int basemip = lod->x;
		const int nmips = lod->y - lod->x + 1;
		for (int mip = 1; mip < nmips; mip++)
		{
			const GIFRegTEX0 MIP_TEX0{g_gs_renderer->GetTex0Layer(basemip + mip)};
			std::pair<u8, u8> mip_alpha_minmax;
			PreloadTexture(MIP_TEX0, TEXA, region.AdjustForMipmap(mip), g_gs_renderer->m_mem, paltex, tex, mip,
				is_direct ? &mip_alpha_minmax : nullptr);
			if (!is_direct)
			{
				alpha_minmax.first = std::min(alpha_minmax.first, mip_alpha_minmax.first);
				alpha_minmax.second = std::max(alpha_minmax.second, mip_alpha_minmax.second);
			}
		}
	}

	// remove the palette hash when using paltex/indexed
	if (paltex)
		key.RemoveCLUTHash();

	// insert into the cache cache, and we're done
	const HashCacheEntry entry{tex, 1u, 0u, alpha_minmax, false};
	m_hash_cache_memory_usage += tex->GetMemUsage();
	return &m_hash_cache.emplace(key, entry).first->second;
}

void GSTextureCache::RemoveFromHashCache(HashCacheMap::iterator it)
{
	HashCacheEntry& e = it->second;
	const u32 mem_usage = e.texture->GetMemUsage();
	if (e.is_replacement)
		m_hash_cache_replacement_memory_usage -= mem_usage;
	else
		m_hash_cache_memory_usage -= mem_usage;
	g_gs_device->Recycle(e.texture);
	m_hash_cache.erase(it);
}

void GSTextureCache::AgeHashCache()
{
	// Where did this number come from?
	// A game called Corvette draws its background FMVs with a ton of 17x17 tiles, which ends up
	// being about 600 texture uploads per frame. We'll use 800 as an upper bound for a bit of
	// a buffer, hopefully nothing's going to end up with more textures than that.
	constexpr u32 MAX_HASH_CACHE_SIZE = 800;
	constexpr u32 MAX_HASH_CACHE_AGE = 30;

	bool might_need_cache_purge = (m_hash_cache.size() > MAX_HASH_CACHE_SIZE);
	if (might_need_cache_purge)
		s_hash_cache_purge_list.clear();

	for (auto it = m_hash_cache.begin(); it != m_hash_cache.end();)
	{
		HashCacheEntry& e = it->second;
		if (e.refcount > 0)
		{
			++it;
			continue;
		}

		if (++e.age > MAX_HASH_CACHE_AGE)
		{
			RemoveFromHashCache(it++);
			continue;
		}

		// We might free up enough just with "normal" removals above.
		if (might_need_cache_purge)
		{
			might_need_cache_purge = (m_hash_cache.size() > MAX_HASH_CACHE_SIZE);
			if (might_need_cache_purge)
				s_hash_cache_purge_list.emplace_back(it, static_cast<s32>(e.age));
		}

		++it;
	}

	// Pushing to a list, sorting, and removing ends up faster than re-iterating the map.
	if (might_need_cache_purge)
	{
		std::sort(s_hash_cache_purge_list.begin(), s_hash_cache_purge_list.end(),
			[](const auto& lhs, const auto& rhs) { return lhs.second > rhs.second; });

		const u32 entries_to_purge = std::min(static_cast<u32>(m_hash_cache.size() - MAX_HASH_CACHE_SIZE),
			static_cast<u32>(s_hash_cache_purge_list.size()));
		for (u32 i = 0; i < entries_to_purge; i++)
			RemoveFromHashCache(s_hash_cache_purge_list[i].first);
	}
}

GSTextureCache::Target* GSTextureCache::Target::Create(GIFRegTEX0 TEX0, int w, int h, float scale, int type, bool clear)
{
	pxAssert(type == RenderTarget || type == DepthStencil);

	const int scaled_w = static_cast<int>(std::ceil(static_cast<float>(w) * scale));
	const int scaled_h = static_cast<int>(std::ceil(static_cast<float>(h) * scale));
	GSTexture* texture = (type == RenderTarget) ?
		g_gs_device->CreateRenderTarget(scaled_w, scaled_h, GSTexture::Format::Color, clear, PreferReusedLabelledTexture()) :
		g_gs_device->CreateDepthStencil(scaled_w, scaled_h, GSTexture::Format::DepthStencil, clear, PreferReusedLabelledTexture());
	if (!texture)
		return nullptr;

	Target* t = new Target(TEX0, type, GSVector2i(w, h), scale, texture);

	g_texture_cache->m_target_memory_usage += t->m_texture->GetMemUsage();

	g_texture_cache->m_dst[type].push_front(t);

	t->UpdateTextureDebugName();

	return t;
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
				if (!dirty.GetDirtyRect(t->m_TEX0, false).rintersect(clut_rc).rempty())
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

std::shared_ptr<GSTextureCache::Palette> GSTextureCache::LookupPaletteObject(const u32* clut, u16 pal, bool need_gs_texture)
{
	return m_palette_map.LookupPalette(clut, pal, need_gs_texture);
}

void GSTextureCache::Read(Target* t, const GSVector4i& r)
{
	if ((!t->m_dirty.empty() && !t->m_dirty.GetTotalRect(t->m_TEX0, t->m_unscaled_size).rintersect(r).rempty())
		|| r.width() == 0 || r.height() == 0)
		return;

	const GIFRegTEX0& TEX0 = t->m_TEX0;
	const bool is_depth = (t->m_type == DepthStencil);

	GSTexture::Format fmt;
	ShaderConvert ps_shader;
	std::unique_ptr<GSDownloadTexture>* dltex;
	switch (TEX0.PSM)
	{
		case PSMCT32:
		case PSMCT24:
		{
			// If we're downloading a depth buffer that's been reinterpreted as a color
			// format, convert it to integer. The format/swizzle is likely wrong, but it's
			// better than writing back FP values to local memory.
			if (is_depth)
			{
				fmt = GSTexture::Format::UInt32;
				ps_shader = ShaderConvert::FLOAT32_TO_32_BITS;
				dltex = &m_uint32_download_texture;
			}
			else
			{
				fmt = GSTexture::Format::Color;
				ps_shader = ShaderConvert::COPY;
				dltex = &m_color_download_texture;
			}
		}
		break;

		case PSMCT16:
		case PSMCT16S:
		{
			fmt = GSTexture::Format::UInt16;
			ps_shader = is_depth ? ShaderConvert::FLOAT32_TO_16_BITS : ShaderConvert::RGBA8_TO_16_BITS;
			dltex = &m_uint16_download_texture;
		}
		break;

		case PSMZ32:
		case PSMZ24:
		{
			fmt = GSTexture::Format::UInt32;
			ps_shader = ShaderConvert::FLOAT32_TO_32_BITS;
			dltex = &m_uint32_download_texture;
		}
		break;

		case PSMZ16:
		case PSMZ16S:
		{
			fmt = GSTexture::Format::UInt16;
			ps_shader = ShaderConvert::FLOAT32_TO_16_BITS;
			dltex = &m_uint16_download_texture;
		}
		break;

		default:
			return;
	}

	// Don't overwrite bits which aren't used in the target's format.
	// Stops Burnout 3's sky from breaking when flushing targets to local memory.
	const u32 write_mask = (t->m_valid_rgb ? 0x00FFFFFFu : 0) | (t->m_valid_alpha_low ? 0x0F000000u : 0) | (t->m_valid_alpha_high ? 0xF0000000u : 0);
	if (write_mask == 0)
	{
		DbgCon.Warning("Not reading back target %x PSM %s due to no write mask", TEX0.TBP0, psm_str(TEX0.PSM));
		return;
	}

	GL_PERF("TC: Read Back Target: (0x%x)[fmt: 0x%x]. Size %dx%d", TEX0.TBP0, TEX0.PSM, r.width(), r.height());

	const GSVector4 src(GSVector4(r) * GSVector4(t->m_scale) / GSVector4(t->m_texture->GetSize()).xyxy());
	const GSVector4i drc(0, 0, r.width(), r.height());
	const bool direct_read = (t->m_type == RenderTarget && t->m_scale == 1.0f && ps_shader == ShaderConvert::COPY);

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
			g_perfmon.Put(GSPerfMon::TextureCopies, 1);
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
		case PSMCT32:
		case PSMZ32:
		case PSMCT24:
		case PSMZ24:
			g_gs_renderer->m_mem.WritePixel32(bits, pitch, off, r, write_mask);
			break;
		case PSMCT16:
		case PSMCT16S:
		case PSMZ16:
		case PSMZ16S:
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

bool GSTextureCache::Surface::Inside(u32 bp, u32 bw, u32 psm, const GSVector4i& rect)
{
	// Valid only for color formats.
	const GSOffset off(GSLocalMemory::m_psm[psm].info, bp, bw, psm);
	const u32 end_block = off.bnNoWrap(rect.z - 1, rect.w - 1);
	return bp >= m_TEX0.TBP0 && end_block <= UnwrappedEndBlock();
}

bool GSTextureCache::Surface::Overlaps(u32 bp, u32 bw, u32 psm, const GSVector4i& rect)
{
	const GSOffset off(GSLocalMemory::m_psm[psm].info, bp, bw, psm);

	// Computing the end block from the bottom-right pixel will not be correct for Z formats,
	// as the block swizzle is not sequential.
	u32 end_block = off.bnNoWrap(rect.z - 1, rect.w - 1);
	u32 start_block = off.bnNoWrap(rect.x, rect.y);

	// So, if the rectangle we're checking is page-aligned, round the block number up to the end of the page.
	const GSVector2i page_size = GSLocalMemory::m_psm[psm].pgs;
	if ((rect.z & (page_size.x - 1)) == 0 && (rect.w & (page_size.y - 1)) == 0)
	{
		constexpr u32 page_mask = (1 << 5) - 1;
		end_block = (((end_block + page_mask) & ~page_mask)) - 1;
	}
	// Due to block ordering, end can be below start in a page, so if it's within a page, swap them.
	else if (end_block < start_block && ((start_block - end_block) < (1 << 5)))
	{
		std::swap(start_block, end_block);
	}

	// Wrapping around to the beginning of memory.
	if (end_block > MAX_BLOCKS && bp < m_end_block && m_end_block < m_TEX0.TBP0)
		bp += MAX_BLOCKS;

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
		g_texture_cache->m_source_memory_usage -= m_texture->GetMemUsage();
		g_gs_device->Recycle(m_texture);
	}
}

bool GSTextureCache::Source::IsPaletteFormat() const
{
	return (GSLocalMemory::m_psm[m_TEX0.PSM].pal > 0);
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

	// Target regions are inverted - we're pointing to an area from the target BP, which might be offset,
	// not an area from the source BP which is definitely offset. So use the target's BP instead, otherwise
	// if the target gets invalidated, and we're offset, and the area doesn't cover us, we won't get removed.
	const GSOffset offset =
		(m_target && m_region.HasEither()) ?
			g_gs_renderer->m_mem.GetOffset(m_from_target_TEX0.TBP0, m_from_target_TEX0.TBW, m_from_target_TEX0.PSM) :
			g_gs_renderer->m_mem.GetOffset(m_TEX0.TBP0, m_TEX0.TBW, m_TEX0.PSM);
	const GSVector4i rect = m_region.GetRect(tw, th);
	m_pages = offset.pageLooperForRect(rect);
}

void GSTextureCache::Source::Update(const GSVector4i& rect, int level)
{
	m_age = 0;
	if (m_from_target)
		m_from_target->m_age = 0;

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

	// Clamp to region, the input rect should already be moved to it.
	if (m_region.HasEither())
		r = r.rintersect(region_rect);

	r = r.ralign<Align_Outside>(bs);

	if (region_rect.eq(r.rintersect(region_rect)))
		m_complete_layers |= (1u << level);

	const GSOffset off = g_gs_renderer->m_mem.GetOffset(m_TEX0.TBP0, m_TEX0.TBW, m_TEX0.PSM);
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

					Write(GSVector4i(x, y, x + bs.x, y + bs.y), level, off);

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

					Write(GSVector4i(x, y, x + bs.x, y + bs.y), level, off);

					blocks++;
				}
			}
		}
	}

	if (blocks > 0)
	{
		g_perfmon.Put(GSPerfMon::Unswizzle, bs.x * bs.y * blocks << (m_palette ? 2 : 0));
		Flush(m_write.count, level, off);
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

void GSTextureCache::Source::Write(const GSVector4i& r, int layer, const GSOffset& off)
{
	if (!m_write.rect)
		m_write.rect = static_cast<GSVector4i*>(_aligned_malloc(3 * sizeof(GSVector4i), 16));

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
		Flush(1, layer, off);
	}
}

void GSTextureCache::Source::Flush(u32 count, int layer, const GSOffset& off)
{
	// This function as written will not work for paletted formats copied from framebuffers
	// because they are 8 or 4 bit formats on the GS and the GS local memory module reads
	// these into an 8 bit format while the D3D surfaces are 32 bit.
	// However the function is never called for these cases.  This is just for information
	// should someone wish to use this function for these cases later.
	const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[m_TEX0.PSM];
	const SourceRegion region((layer == 0) ? m_region : m_region.AdjustForMipmap(layer));

	// For the invalid tex0 case, the region might be larger than TEX0.TW/TH.
	const int tw = std::max(region.GetWidth(), 1 << m_TEX0.TW);
	const int th = std::max(region.GetHeight(), 1 << m_TEX0.TH);
	const GSVector4i tex_r(region.GetRect(tw, th));

	int pitch = std::max(tw, psm.bs.x) * sizeof(u32);

	GSLocalMemory& mem = g_gs_renderer->m_mem;

	GSLocalMemory::readTexture rtx = psm.rtx;

	if (m_palette)
	{
		pitch >>= 2;
		rtx = psm.rtxP;
	}

	pitch = VectorAlign(pitch);

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
	if (IsPaletteFormat())
	{
		PreloadTexture(m_TEX0, m_TEXA, m_region.AdjustForMipmap(level), g_gs_renderer->m_mem, m_palette != nullptr,
			m_texture, level, nullptr);
	}
	else
	{
		std::pair<u8, u8> mip_alpha_minmax;
		PreloadTexture(m_TEX0, m_TEXA, m_region.AdjustForMipmap(level), g_gs_renderer->m_mem, m_palette != nullptr,
			m_texture, level, &mip_alpha_minmax);
		// The entire alpha is always recalculated
		m_alpha_minmax.first = std::min(m_alpha_minmax.first, mip_alpha_minmax.first);
		m_alpha_minmax.second = std::max(m_alpha_minmax.second, mip_alpha_minmax.second);
	}
}

bool GSTextureCache::Source::ClutMatch(const PaletteKey& palette_key)
{
	return PaletteKeyEqual()(palette_key, m_palette_obj->GetPaletteKey());
}

// GSTextureCache::Target

GSTextureCache::Target::Target(GIFRegTEX0 TEX0, int type, const GSVector2i& unscaled_size, float scale, GSTexture* texture)
	: m_type(type)
	, m_used(type == RenderTarget) // FIXME
	, m_valid(GSVector4i::zero())
{
	m_TEX0 = TEX0;
	m_end_block = m_TEX0.TBP0;
	m_unscaled_size = unscaled_size;
	m_scale = scale;
	m_texture = texture;
	m_alpha_min = 0;
	m_alpha_max = 0;
	m_32_bits_fmt |= (GSLocalMemory::m_psm[TEX0.PSM].trbpp != 16);
}

GSTextureCache::Target::~Target()
{
	// Targets should never be shared.
	pxAssert(!m_shared_texture);
	if (m_texture)
	{
		g_texture_cache->m_target_memory_usage -= m_texture->GetMemUsage();
		g_gs_device->Recycle(m_texture);
	}

#ifdef PCSX2_DEVBUILD
	// Make sure all sources referencing this target have been removed.
	for (GSTextureCache::Source* src : g_texture_cache->m_src.m_surfaces)
	{
		if (src->m_from_target == this)
		{
			pxFail(fmt::format("Source at TBP {:x} for target at TBP {:x} on target invalidation",
				static_cast<u32>(src->m_TEX0.TBP0), static_cast<u32>(m_TEX0.TBP0))
					   .c_str());
			break;
		}
	}
#endif
}

void GSTextureCache::Target::Update()
{
	m_age = 0;

	// FIXME: the union of the rects may also update wrong parts of the render target (but a lot faster :)
	// GH: it must be doable
	// 1/ rescale the new t to the good size
	// 2/ copy each rectangle (rescale the rectangle) (use CopyRect or multiple vertex)
	// Alternate
	// 1/ uses multiple vertex rectangle

	if (m_dirty.empty())
		return;

	// No handling please
	if (m_type == DepthStencil && GSConfig.UserHacks_DisableDepthSupport)
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
	if (!t) [[unlikely]]
	{
		Console.Error("Failed to allocate %dx%d for update source", t_size.z, t_size.w);
		return;
	}

	GSTexture::GSMap m;
	const bool mapped = t->Map(m);

	GIFRegTEXA TEXA = {};
	TEXA.AEM = 1;
	TEXA.TA0 = 0;
	TEXA.TA1 = 0x80;

	// Bilinear filtering this is probably not a good thing, at least in native, but upscaling Nearest can be gross and messy.
	// It's needed for depth, though.. filtering depth doesn't make much sense, but SMT3 needs it..
	const bool upscaled = (m_scale != 1.0f);
	const bool override_linear = (upscaled && GSConfig.UserHacks_BilinearHack == GSBilinearDirtyMode::ForceBilinear);
	const bool linear = (m_type == RenderTarget && upscaled && GSConfig.UserHacks_BilinearHack != GSBilinearDirtyMode::ForceNearest);

	GSDevice::MultiStretchRect* drects = static_cast<GSDevice::MultiStretchRect*>(
		alloca(sizeof(GSDevice::MultiStretchRect) * static_cast<u32>(m_dirty.size())));
	u32 ndrects = 0;

	const GSOffset off(g_gs_renderer->m_mem.GetOffset(m_TEX0.TBP0, m_TEX0.TBW, m_TEX0.PSM));
	std::pair<u8, u8> alpha_minmax = {255, 0};

	for (size_t i = 0; i < m_dirty.size(); i++)
	{
		// Don't align the area we write to the target to the block size. If the format matches, the writes don't need
		// to be block aligned. We still read the whole thing in, because that's the granularity that ReadTexture
		// operates at, but discard those pixels when updating the framebuffer. Onimusha 2 does this dance where it
		// uploads the left 4 pixels to the middle of the image, then moves it to the left, and because we process
		// the move in hardware, local memory never gets updated, and thus is stale.
		const GSVector4i update_r = m_dirty.GetDirtyRect(i, m_TEX0, total_rect, false);
		if (update_r.rempty())
			continue;

		const GSVector4i read_r = m_dirty.GetDirtyRect(i, m_TEX0, total_rect, true);
		const GSVector4i t_r(read_r - t_offset);
		if (mapped)
		{
			if (m_32_bits_fmt && (m_TEX0.PSM & 0xf) != PSMCT24)
			{
				// TODO: Only read once in 32bit and copy to the mapped texture. Bit out of scope of this PR and not a huge impact.
				const int pitch = VectorAlign(read_r.width() * sizeof(u32));
				g_gs_renderer->m_mem.ReadTexture(off, read_r, s_unswizzle_buffer, pitch, TEXA);

				std::pair<u8, u8> new_alpha_minmax = GSGetRGBA8AlphaMinMax(s_unswizzle_buffer, read_r.width(), read_r.height(), pitch);
				alpha_minmax.first = std::min(alpha_minmax.first, new_alpha_minmax.first);
				alpha_minmax.second = std::max(alpha_minmax.second, new_alpha_minmax.second);
			}

			g_gs_renderer->m_mem.ReadTexture(
				off, read_r, m.bits + t_r.y * static_cast<u32>(m.pitch) + (t_r.x * sizeof(u32)), m.pitch, TEXA);
		}
		else
		{
			const int pitch = VectorAlign(read_r.width() * sizeof(u32));
			g_gs_renderer->m_mem.ReadTexture(off, read_r, s_unswizzle_buffer, pitch, TEXA);

			if (m_32_bits_fmt && (m_TEX0.PSM & 0xf) != PSMCT24)
			{
				std::pair<u8, u8> new_alpha_minmax = GSGetRGBA8AlphaMinMax(s_unswizzle_buffer, read_r.width(), read_r.height(), pitch);
				alpha_minmax.first = std::min(alpha_minmax.first, new_alpha_minmax.first);
				alpha_minmax.second = std::max(alpha_minmax.second, new_alpha_minmax.second);
			}

			t->Update(t_r, s_unswizzle_buffer, pitch);
		}

		GSDevice::MultiStretchRect& drect = drects[ndrects++];
		drect.src = t;
		drect.src_rect = GSVector4(update_r - t_offset) / t_sizef;
		drect.dst_rect = GSVector4(update_r) * GSVector4(m_scale);
		drect.linear = linear && (m_dirty[i].req_linear || override_linear);

		// Copy the new GS memory content into the destination texture.
		if (m_type == RenderTarget)
		{
			GL_INS("ERROR: Update RenderTarget 0x%x bw:%d (%d,%d => %d,%d)", m_TEX0.TBP0, m_TEX0.TBW,
				update_r.x, update_r.y, update_r.z, update_r.w);
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
		ShaderConvert depth_shader = upscaled ? ShaderConvert::RGBA8_TO_FLOAT32_BILN : ShaderConvert::RGBA8_TO_FLOAT32;
		if (m_type == DepthStencil && GSLocalMemory::m_psm[m_TEX0.PSM].trbpp != 32)
		{
			switch (GSLocalMemory::m_psm[m_TEX0.PSM].trbpp)
			{
				case 24:
					depth_shader = upscaled ? ShaderConvert::RGBA8_TO_FLOAT24_BILN : ShaderConvert::RGBA8_TO_FLOAT24;
					break;
				case 16:
					depth_shader = upscaled ? ShaderConvert::RGB5A1_TO_FLOAT16_BILN : ShaderConvert::RGB5A1_TO_FLOAT16;
					break;
				default:
					break;
			}
		}

		// No need to sort here, it's all the one texture.
		const ShaderConvert shader = (m_type == RenderTarget) ? ShaderConvert::COPY : depth_shader;

		g_gs_device->DrawMultiStretchRects(drects, ndrects, m_texture, shader);
	}

	if ((m_TEX0.PSM & 0xf) == PSMCT24)
	{
		m_alpha_min = 128;
		m_alpha_max = 128;
	}
	else
	{
		if (m_32_bits_fmt)
		{
			if (!total_rect.eq(m_valid))
			{
				m_alpha_min = std::min(static_cast<int>(alpha_minmax.first), m_alpha_min);
				m_alpha_max = std::max(static_cast<int>(alpha_minmax.second), m_alpha_max);
			}
			else
			{
				m_alpha_min = alpha_minmax.first;
				m_alpha_max = alpha_minmax.second;
			}
		}
		else
		{
			m_alpha_min = 0;
			m_alpha_max = 128;
		}
	}
	g_gs_device->Recycle(t);
	m_dirty.clear();
}

void GSTextureCache::Target::UpdateIfDirtyIntersects(const GSVector4i& rc)
{
	m_age = 0;

	for (auto& dirty : m_dirty)
	{
		const GSVector4i dirty_rc(dirty.GetDirtyRect(m_TEX0, false));
		if (dirty_rc.rintersect(rc).rempty())
			continue;

		// strictly speaking, we only need to update the area outside of the move.
		// but, to keep things simple, just update the whole thing
		GL_CACHE("TC: Update dirty rectangle [%d,%d,%d,%d] due to intersection with [%d,%d,%d,%d]",
			dirty_rc.x, dirty_rc.y, dirty_rc.z, dirty_rc.w, rc.x, rc.y, rc.z, rc.w);
		Update();
		break;
	}
}

void GSTextureCache::Target::UpdateValidChannels(u32 psm, u32 fbmsk)
{
	const GSLocalMemory::psm_t& psm_s = GSLocalMemory::m_psm[psm];
	m_valid_alpha_low |= (psm_s.trbpp == 32 && (fbmsk & 0x0F000000) != 0x0F000000) || (psm_s.trbpp == 16);
	m_valid_alpha_high |= (psm_s.trbpp == 32 && (fbmsk & 0xF0000000) != 0xF0000000) || (psm_s.trbpp == 16);
	m_valid_rgb |= (psm_s.trbpp >= 24 && (fbmsk & 0x00FFFFFF) != 0x00FFFFFF) || (psm_s.trbpp == 16);
}

bool GSTextureCache::Target::HasValidBitsForFormat(u32 psm, bool req_color, bool req_alpha)
{
	// Grab validities..
	bool alpha_valid = false;
	bool color_valid = false;

	switch (psm)
	{
		case PSMT4:
			return (m_valid_rgb && m_valid_alpha_low && m_valid_alpha_high);
		case PSMT8H:
			return m_valid_alpha_low || m_valid_alpha_high;
		case PSMT4HL:
			return m_valid_alpha_low;
		case PSMT4HH:
			return m_valid_alpha_high;
		case PSMT8: // Down here because of channel shuffles.
		default:
			alpha_valid = m_valid_alpha_low || m_valid_alpha_high;
			color_valid = m_valid_rgb;

			if (req_alpha && !alpha_valid && color_valid && (m_TEX0.PSM & 0xF) <= PSMCT24 && (psm & 0xF) == PSMCT32)
			{
			
				RGBAMask mask;
				mask._u32 = 0x8;
				m_TEX0.PSM &= ~PSMCT24;

				if (!(m_dirty.GetDirtyChannels() & 0x8))
					AddDirtyRectTarget(this, m_valid, m_TEX0.PSM, m_TEX0.TBW, mask, false);

				alpha_valid = true; // This is going to get resolved going forward.
			}
			break;
	}

	return (!req_color || color_valid) && (!req_alpha || alpha_valid);
}

void GSTextureCache::Target::ResizeDrawn(const GSVector4i& rect)
{
	m_drawn_since_read = m_drawn_since_read.rintersect(rect);
}


void GSTextureCache::Target::UpdateDrawn(const GSVector4i& rect, bool can_update_size)
{
	if (m_drawn_since_read.rempty())
	{
		m_drawn_since_read = rect.rintersect(m_valid);
	}
	else if (can_update_size)
	{
		m_drawn_since_read = m_drawn_since_read.runion(rect);
	}
}

void GSTextureCache::Target::ResizeValidity(const GSVector4i& rect)
{
	if (!m_valid.eq(GSVector4i::zero()))
	{
		m_valid = m_valid.rintersect(rect);
		m_drawn_since_read = m_drawn_since_read.rintersect(rect);
		m_end_block = GSLocalMemory::GetEndBlockAddress(m_TEX0.TBP0, m_TEX0.TBW, m_TEX0.PSM, m_valid);
	}
	// Else No valid size, so need to resize down.

	// GL_CACHE("ResizeValidity (0x%x->0x%x) from R:%d,%d Valid: %d,%d", m_TEX0.TBP0, m_end_block, rect.z, rect.w, m_valid.z, m_valid.w);
}

void GSTextureCache::Target::UpdateValidity(const GSVector4i& rect, bool can_resize)
{
	if (m_valid.eq(GSVector4i::zero()))
	{
		m_valid = rect;

		m_end_block = GSLocalMemory::GetEndBlockAddress(m_TEX0.TBP0, m_TEX0.TBW, m_TEX0.PSM, m_valid);
	}
	else if (can_resize)
	{
		m_valid = m_valid.runion(rect);

		m_end_block = GSLocalMemory::GetEndBlockAddress(m_TEX0.TBP0, m_TEX0.TBW, m_TEX0.PSM, m_valid);
	}
	// GL_CACHE("UpdateValidity (0x%x->0x%x) from R:%d,%d Valid: %d,%d", m_TEX0.TBP0, m_end_block, rect.z, rect.w, m_valid.z, m_valid.w);
}

bool GSTextureCache::Target::ResizeTexture(int new_unscaled_width, int new_unscaled_height, bool recycle_old)
{
	if (m_unscaled_size.x == new_unscaled_width && m_unscaled_size.y == new_unscaled_height)
		return true;

	const GSVector2i size = m_texture->GetSize();
	const GSVector2i new_unscaled_size = GSVector2i(new_unscaled_width, new_unscaled_height);
	const GSVector2i new_size = ScaleRenderTargetSize(new_unscaled_size, m_scale);
	const bool clear = (new_size.x > size.x || new_size.y > size.y);

	GSTexture* tex = m_texture->IsDepthStencil() ?
						 g_gs_device->CreateDepthStencil(new_size.x, new_size.y, m_texture->GetFormat(), clear,
							 PreferReusedLabelledTexture()) :
						 g_gs_device->CreateRenderTarget(new_size.x, new_size.y, m_texture->GetFormat(), clear,
							 PreferReusedLabelledTexture());
	if (!tex)
	{
		Console.Error("(ResizeTexture) Failed to allocate %dx%d texture from %dx%d texture", size.x, size.y, new_size.x, new_size.y);
		return false;
	}

	// Only need to copy if it's been written to.
	if (m_texture->GetState() == GSTexture::State::Dirty)
	{
		const GSVector4i rc = GSVector4i::loadh(size.min(new_size));
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

		g_perfmon.Put(GSPerfMon::TextureCopies, 1);
	}
	else if (m_texture->GetState() == GSTexture::State::Cleared)
	{
		// Otherwise just pass the clear through.
		if (tex->GetType() != GSTexture::Type::DepthStencil)
			g_gs_device->ClearRenderTarget(tex, m_texture->GetClearColor());
		else
			g_gs_device->ClearDepth(tex, m_texture->GetClearDepth());
	}
	else
	{
		g_gs_device->InvalidateRenderTarget(tex);
	}

	g_texture_cache->m_target_memory_usage = (g_texture_cache->m_target_memory_usage - m_texture->GetMemUsage()) + tex->GetMemUsage();

	if (recycle_old)
		g_gs_device->Recycle(m_texture);
	else
		delete m_texture;

	m_texture = tex;
	m_unscaled_size = new_unscaled_size;

	UpdateTextureDebugName();

	return true;
}

void GSTextureCache::Target::UpdateTextureDebugName()
{
#ifdef PCSX2_DEVBUILD
	if (GSConfig.UseDebugDevice)
	{
		m_texture->SetDebugName(SmallString::from_fmt("{} 0x{:X} {} BW={} {}x{}",
			m_type ? "DS" : "RT", static_cast<u32>(m_TEX0.TBP0), psm_str(m_TEX0.PSM), static_cast<u32>(m_TEX0.TBW),
			m_unscaled_size.x, m_unscaled_size.y));
	}
#endif
}

// GSTextureCache::SourceMap

void GSTextureCache::SourceMap::Add(Source* s, const GIFRegTEX0& TEX0)
{
	m_surfaces.insert(s);

	// The source pointer will be stored/duplicated in all m_map[array of pages]
	s->m_pages.loopPages([this, s](u32 page)
	{
		s->m_erase_it[page] = m_map[page].InsertFront(s);
	});
}

void GSTextureCache::SourceMap::SwapTexture(GSTexture* old_tex, GSTexture* new_tex)
{
	for (auto s : m_surfaces)
	{
		if (s->m_texture == old_tex)
			s->m_texture = new_tex;
	}
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

	GL_CACHE("TC: Remove Src Texture: 0x%x TBW %u PSM %s",
		s->m_TEX0.TBP0, s->m_TEX0.TBW, psm_str(s->m_TEX0.PSM));

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
	s->m_alpha_minmax = s->m_palette_obj->GetAlphaMinMax();
}

void GSTextureCache::AttachPaletteToSource(Source* s, GSTexture* gpu_clut)
{
	s->m_palette_obj = nullptr;
	s->m_palette = gpu_clut;

	// Unknown.
	s->m_alpha_minmax.first = 0;
	s->m_alpha_minmax.second = 255;
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

	pxAssert(!so.is_valid || b2a_offset.x >= b_rect.x);
	pxAssert(!so.is_valid || b2a_offset.x < b_rect.z);
	pxAssert(!so.is_valid || b2a_offset.y >= b_rect.y);
	pxAssert(!so.is_valid || b2a_offset.y < b_rect.w);

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

	pxAssert(!so.is_valid || b2a_offset.z > b2a_offset.x);
	pxAssert(!so.is_valid || b2a_offset.z <= b_rect.z);
	pxAssert(!so.is_valid || b2a_offset.w > b_rect.y);
	pxAssert(!so.is_valid || b2a_offset.w <= b_rect.w);

	so.b2a_offset = b2a_offset;

	const GSVector4i& r1 = so.b2a_offset;
	const GSVector4i& r2 = b_rect;
	[[maybe_unused]] const GSVector4i ri = r1.rintersect(r2);
	pxAssert(!so.is_valid || (r1.eq(ri) && r1.x >= 0 && r1.y >= 0 && r1.z > 0 && r1.w > 0));

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

	delete m_temporary_source;
	m_temporary_source = nullptr;
}

void GSTextureCache::InjectHashCacheTexture(const HashCacheKey& key, GSTexture* tex, const std::pair<u8, u8>& alpha_minmax)
{
	// When we insert we update memory usage. Old texture gets removed below.
	m_hash_cache_replacement_memory_usage += tex->GetMemUsage();

	auto it = m_hash_cache.find(key);
	if (it == m_hash_cache.end())
	{
		// We must've got evicted before we finished loading. No matter, add it in there anyway;
		// if it's not used again, it'll get tossed out later.
		const HashCacheEntry entry{tex, 1u, 0u, alpha_minmax, true};
		m_hash_cache.emplace(key, entry);
		return;
	}

	// Reset age so we don't get thrown out too early.
	it->second.age = 0;
	it->second.alpha_minmax = alpha_minmax;

	// Update memory usage, swap the textures, and recycle the old one for reuse.
	if (!it->second.is_replacement)
		m_hash_cache_memory_usage -= it->second.texture->GetMemUsage();
	else
		m_hash_cache_replacement_memory_usage -= it->second.texture->GetMemUsage();

	it->second.is_replacement = true;
	m_src.SwapTexture(it->second.texture, tex);
	g_gs_device->Recycle(it->second.texture);
	it->second.texture = tex;	
}

// GSTextureCache::Palette

GSTextureCache::Palette::Palette(const u32* clut, u16 pal, bool need_gs_texture)
	: m_tex_palette(nullptr)
	, m_pal(pal)
{
	const u16 palette_size = pal * sizeof(u32);
	m_clut = (u32*)_aligned_malloc(palette_size, 64);
	memcpy(m_clut, clut, palette_size);
	if (need_gs_texture)
	{
		InitializeTexture();
	}

	m_alpha_minmax = GSGetRGBA8AlphaMinMax(m_clut, pal, 1, 0);
}

GSTextureCache::Palette::~Palette()
{
	if (m_tex_palette)
	{
		g_texture_cache->m_source_memory_usage -= m_tex_palette->GetMemUsage();
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
		if (!m_tex_palette) [[unlikely]]
		{
			Console.Error("Failed to allocate %ux1 texture for palette", m_pal);
			return;
		}

		m_tex_palette->Update(GSVector4i(0, 0, m_pal, 1), m_clut, m_pal * sizeof(m_clut[0]));
		g_texture_cache->m_source_memory_usage += m_tex_palette->GetMemUsage();
	}
}

// GSTextureCache::PaletteKeyHash

u64 GSTextureCache::PaletteKeyHash::operator()(const PaletteKey& key) const
{
	pxAssert(key.pal == 16 || key.pal == 256);
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
	return LookupPalette(g_gs_renderer->m_mem.m_clut, pal, need_gs_texture);
}

std::shared_ptr<GSTextureCache::Palette> GSTextureCache::PaletteMap::LookupPalette(const u32* clut, u16 pal, bool need_gs_texture)
{
	pxAssert(pal == 16 || pal == 256);

	// Choose which hash map search into:
	//    pal == 16  : index 0
	//    pal == 256 : index 1
	auto& map = m_maps[pal == 16 ? 0 : 1];

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

	std::shared_ptr<Palette> palette = std::make_shared<Palette>(clut, pal, need_gs_texture);

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
		const SurfaceOffsetKeyElem& lhs_elem = lhs.elems[i];
		const SurfaceOffsetKeyElem& rhs_elem = rhs.elems[i];
		if (lhs_elem.bp != rhs_elem.bp
			|| lhs_elem.bw != rhs_elem.bw
			|| lhs_elem.psm != rhs_elem.psm
			|| !lhs_elem.rect.eq(rhs_elem.rect))
			return false;
	}
	return true;
}

bool GSTextureCache::SourceRegion::IsFixedTEX0(GIFRegTEX0 TEX0) const
{
	return IsFixedTEX0(1 << TEX0.TW, 1 << TEX0.TH);
}

bool GSTextureCache::SourceRegion::IsFixedTEX0(int tw, int th) const
{
	return IsFixedTEX0W(tw) || IsFixedTEX0H(th);
}

bool GSTextureCache::SourceRegion::IsFixedTEX0W(int tw) const
{
	return (GetMaxX() > tw);
}

bool GSTextureCache::SourceRegion::IsFixedTEX0H(int th) const
{
	return (GetMaxY() > th);
}

GSVector2i GSTextureCache::SourceRegion::GetSize(int tw, int th) const
{
	return GSVector2i(HasX() ? GetWidth() : tw, HasY() ? GetHeight() : th);
}

GSVector4i GSTextureCache::SourceRegion::GetRect(int tw, int th) const
{
	return GSVector4i(HasX() ? GetMinX() : 0, HasY() ? GetMinY() : 0, HasX() ? GetMaxX() : tw, HasY() ? GetMaxY() : th);
}

GSVector4i GSTextureCache::SourceRegion::GetOffset(int tw, int th) const
{
	const int xoffs = (GetMaxX() > tw) ? GetMinX() : 0;
	const int yoffs = (GetMaxY() > th) ? GetMinY() : 0;
	return GSVector4i(xoffs, yoffs, xoffs, yoffs);
}

GSTextureCache::SourceRegion GSTextureCache::SourceRegion::AdjustForMipmap(u32 level) const
{
	// Texture levels must be at least one pixel wide/high.
	SourceRegion ret = {};
	if (HasX())
	{
		const s32 new_minx = GetMinX() >> level;
		const s32 new_maxx = new_minx + std::max(GetWidth() >> level, 1);
		ret.SetX(new_minx, new_maxx);
	}
	if (HasY())
	{
		const s32 new_miny = GetMinY() >> level;
		const s32 new_maxy = new_miny + std::max(GetHeight() >> level, 1);
		ret.SetY(new_miny, new_maxy);
	}
	return ret;
}

void GSTextureCache::SourceRegion::AdjustTEX0(GIFRegTEX0* TEX0) const
{
	const GSOffset offset(GSLocalMemory::m_psm[TEX0->PSM].info, TEX0->TBP0, TEX0->TBW, TEX0->PSM);
	TEX0->TBP0 += offset.bn(GetMinX(), GetMinY());
}

GSTextureCache::SourceRegion GSTextureCache::SourceRegion::Create(GIFRegTEX0 TEX0, GIFRegCLAMP CLAMP)
{
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

	return region;
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
		const bool palette = (psm.pal > 0);
		const u32 pitch = VectorAlign(static_cast<u32>(block_rect.z) << (palette ? 0 : 2));
		const u32 row_size = static_cast<u32>(tw) << (palette ? 0 : 2);
		const GSLocalMemory::readTexture rtx = palette ? psm.rtxP : psm.rtx;

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

void GSTextureCache::PreloadTexture(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, SourceRegion region, GSLocalMemory& mem,
	bool paltex, GSTexture* tex, u32 level, std::pair<u8, u8>* alpha_minmax)
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
	if (rect.eq(block_rect) && !alpha_minmax && tex->Map(map, &unoffset_rect, level))
	{
		rtx(mem, off, block_rect, map.bits, map.pitch, TEXA);
		tex->Unmap();
		// Temporary, can't read the texture here so we need to come up with a smarter solution, but this will get around it being broken.
		if (alpha_minmax)
		{
			*alpha_minmax = std::make_pair<u8, u8>(0, 255);
		}
	}
	else
	{
		pitch = VectorAlign(pitch);

		u8* buff = s_unswizzle_buffer;
		rtx(mem, off, block_rect, buff, pitch, TEXA);

		const u8* ptr = buff + (pitch * static_cast<u32>(rect.top - block_rect.top)) +
						(static_cast<u32>(rect.left - block_rect.left) << (paltex ? 0 : 2));
		if (alpha_minmax)
		{
			pxAssert(GSLocalMemory::m_psm[TEX0.PSM].pal == 0);
			*alpha_minmax = GSGetRGBA8AlphaMinMax(buff, unoffset_rect.width(), unoffset_rect.height(), pitch);
		}
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
