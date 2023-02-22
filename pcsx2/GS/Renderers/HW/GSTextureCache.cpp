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

void GSTextureCache::AddDirtyRectTarget(Target* target, GSVector4i rect, u32 psm, u32 bw)
{
	bool skipdirty = false;
	bool canskip = true;
	std::vector<GSDirtyRect>::iterator it = target->m_dirty.end();
	while (it != target->m_dirty.begin())
	{
		--it;
		if (it[0].bw == bw && it[0].psm == psm)
		{
			if (it[0].r.rintersect(rect).eq(rect) && canskip)
			{
				skipdirty = true;
				break;
			}

			// Edges lined up so just expand the dirty rect
			if (it[0].r.xzxz().eq(rect.xzxz()) ||
				it[0].r.ywyw().eq(rect.ywyw()))
			{
				rect = rect.runion(it[0].r);
				it = target->m_dirty.erase(it);
				canskip = false;
				continue;
			}
		}
	}

	if (!skipdirty)
		target->m_dirty.push_back(GSDirtyRect(rect, psm, bw));
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
	ASSERT(src->m_texture->GetScale() == (dst ? dst->m_texture->GetScale() : GSVector2(1, 1)));

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
				if (!s->m_palette && !s->ClutMatch({clut, psm_s.pal}))
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

				const bool t_clean = t->m_dirty.empty();
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
					x_offset = 0;
					y_offset = 0;
					break;
				}
				// Make sure the texture actually is INSIDE the RT, it's possibly not valid if it isn't.
				// Also check BP >= TBP, create source isn't equpped to expand it backwards and all data comes from the target. (GH3)
				else if (GSConfig.UserHacks_TextureInsideRt && psm >= PSM_PSMCT32 && psm <= PSM_PSMCT16S && t->m_TEX0.PSM == psm &&
						(t->Overlaps(bp, bw, psm, r) || t_wraps) && t->m_age <= 1 && !found_t && bp >= t->m_TEX0.TBP0)
				{
					// Only PSMCT32 to limit false hits.
					// PSM equality needed because CreateSource does not handle PSM conversion.
					// Only inclusive hit to limit false hits.

					// Check if it is possible to hit with valid <x,y> offset on the given Target.
					// Fixes Jak eyes rendering.
					// Fixes Xenosaga 3 last dungeon graphic bug.
					// Fixes Pause menu in The Getaway.

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
						found_t = true;
						// Keep looking, just in case there is an exact match (Situation: Target frame drawn inside target frame, current makes a separate texture)
						continue;
					}
				}
			}
		}
		
		if (tex_in_rt)
		{
			GSVector2i size_delta = { ((x_offset + (1 << TEX0.TW)) - dst->m_valid.z), ((y_offset + (1 << TEX0.TH)) - dst->m_valid.w) };

			if (size_delta.x > 0)
			{
				// Expand the target if it's only partially inside it.
				const GSVector4i dirty_rect = { dst->m_valid.z, 0, x_offset + (1 << TEX0.TW), dst->m_valid.w };

				if (dirty_rect.z > dst->m_valid.z)
				{
					dst->UpdateValidity(dirty_rect);

					AddDirtyRectTarget(dst, dirty_rect, dst->m_TEX0.PSM, dst->m_TEX0.TBW);
				}
			}

			if (size_delta.y > 0)
			{
				// Expand the target if it's only partially inside it.
				const GSVector4i dirty_rect = { 0, dst->m_valid.w, dst->m_valid.z, y_offset + (1 << TEX0.TH) };

				if (dirty_rect.w > dst->m_valid.w)
				{
					dst->UpdateValidity(dirty_rect);

					AddDirtyRectTarget(dst, dirty_rect, dst->m_TEX0.PSM, dst->m_TEX0.TBW);
				}
			}
			const GSVector2 up_s(dst->m_texture->GetScale());
			const int new_w = std::max(static_cast<int>(dst->m_valid.z * up_s.x), dst->m_texture->GetWidth());
			const int new_h = std::max(static_cast<int>(dst->m_valid.w * up_s.y), dst->m_texture->GetHeight());

			if (new_w > dst->m_texture->GetWidth() || new_h > dst->m_texture->GetHeight())
			{
				dst->ResizeTexture(new_w, new_h, up_s);
			}
		}
		// Pure depth texture format will be fetched by LookupDepthSource.
		// However guess what, some games (GoW) read the depth as a standard
		// color format (instead of a depth format). All pixels are scrambled
		// (because color and depth don't have same location). They don't care
		// pixel will be several draw calls later.
		//
		// Sigh... They don't help us.

		if (!found_t && !GSConfig.UserHacks_DisableDepthSupport)
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
		else if (src->m_palette && (!src->m_palette_obj || !src->ClutMatch({clut, psm_s.pal})))
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

GSTextureCache::Target* GSTextureCache::LookupTarget(const GIFRegTEX0& TEX0, const GSVector2i& size, int type, bool used, u32 fbmask, const bool is_frame, const int real_w, const int real_h, bool preload, bool is_clear)
{
	const GSLocalMemory::psm_t& psm_s = GSLocalMemory::m_psm[TEX0.PSM];
	const GSVector2& new_s = static_cast<GSRendererHW*>(g_gs_renderer.get())->GetTextureScaleFactor();
	const u32 bp = TEX0.TBP0;
	GSVector2 res_size{0, 0};
	GSVector2i new_size{0, 0};
	const GSVector4 sRect(0, 0, 1, 1);
	GSVector4 dRect{};
	bool clear = true;
	const auto& calcRescale = [size, new_s, &res_size, &new_size, &clear, &dRect](const GSTexture* tex)
	{
		// TODO Possible optimization: rescale only the validity rectangle of the old target texture into the new one.
		const GSVector2& old_s = tex->GetScale();
		const GSVector2 ratio = new_s / old_s;
		const int old_w = tex->GetWidth();
		const int old_h = tex->GetHeight();
		res_size = GSVector2(old_w, old_h) * ratio;
		new_size.x = std::max(static_cast<int>(std::ceil(res_size.x)), size.x);
		new_size.y = std::max(static_cast<int>(std::ceil(res_size.y)), size.y);
		clear = new_size.x > res_size.x || new_size.y > res_size.y;
		dRect = GSVector4(0.0f, 0.0f, res_size.x, res_size.y);
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
				// Nicktoons Unite tries to change the width from 10 to 8 and breaks FMVs.
				// Haunting ground has some messed textures if you don't modify the rest.
				if (!dst->m_is_frame)
				{
					dst->m_TEX0 = TEX0;
				}
				else
				{
					u32 width = dst->m_TEX0.TBW;
					dst->m_TEX0 = TEX0;
					dst->m_TEX0.TBW = width;
				}

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
				if (real_h > 0 || real_w > 0)
					ScaleTargetForDisplay(dst, TEX0, real_w, real_h);

				break;
			}
		}

		// 2nd try ! Try to find a frame at the requested bp -> bp + size is inside of (or equal to)
		if (!dst)
		{
			const u32 needed_end = GSLocalMemory::m_psm[TEX0.PSM].info.bn(real_w - 1, real_h - 1, bp, TEX0.TBW);
			for (auto t : list)
			{
				// Make sure the target is inside the texture
				if (t->m_TEX0.TBP0 <= bp && bp <= t->m_end_block && t->Inside(bp, TEX0.TBW, TEX0.PSM, GSVector4i(0, 0, real_w, real_h)))
				{
					// If we already have an old one, make sure the "new" one matches at least on one end (double buffer?).
					if (old_found && (t->m_age > 4 || (t->m_TEX0.TBP0 != bp && needed_end != t->m_end_block)))
						continue;

					dst = t;
					GL_CACHE("TC: Lookup Frame %dx%d, inclusive hit: %d (0x%x, took 0x%x -> 0x%x %s)", size.x, size.y, t->m_texture->GetID(), bp, t->m_TEX0.TBP0, t->m_end_block, psm_str(TEX0.PSM));

					if (real_h > 0 || real_w > 0)
						ScaleTargetForDisplay(dst, TEX0, real_w, real_h);

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
		GL_CACHE("TC: Lookup %s(%s) %dx%d, hit: %d (0x%x, %s)", is_frame ? "Frame" : "Target", to_string(type), size.x, size.y, dst->m_texture->GetID(), bp, psm_str(TEX0.PSM));

		dst->Update(!is_frame || old_found == dst);

		const GSVector2& old_s = dst->m_texture->GetScale();
		if (new_s != old_s)
		{
			calcRescale(dst->m_texture);
			GSTexture* tex = type == RenderTarget ? g_gs_device->CreateRenderTarget(new_size.x, new_size.y, GSTexture::Format::Color, clear) :
													g_gs_device->CreateDepthStencil(new_size.x, new_size.y, GSTexture::Format::DepthStencil, clear);
			g_gs_device->StretchRect(dst->m_texture, sRect, tex, dRect, (type == RenderTarget) ? ShaderConvert::COPY : ShaderConvert::DEPTH_COPY, false);
			m_target_memory_usage = (m_target_memory_usage - dst->m_texture->GetMemUsage()) + tex->GetMemUsage();
			g_gs_device->Recycle(dst->m_texture);
			tex->SetScale(new_s);
			dst->m_texture = tex;
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
			calcRescale(dst_match->m_texture);
			dst = CreateTarget(TEX0, new_size.x, new_size.y, type, clear);
			dst->m_32_bits_fmt = dst_match->m_32_bits_fmt;
			ShaderConvert shader;
			// m_32_bits_fmt gets set on a shuffle or if the format isn't 16bit.
			// In this case it needs to make sure it isn't part of a shuffle, where it needs to be interpreted as 32bits.
			const bool fmt_16_bits = (psm_s.bpp == 16 && GSLocalMemory::m_psm[dst_match->m_TEX0.PSM].bpp == 16 && !dst->m_32_bits_fmt);
			if (type == DepthStencil)
			{
				GL_CACHE("TC: Lookup Target(Depth) %dx%d, hit Color (0x%x, %s was %s)", new_size.x, new_size.y, bp, psm_str(TEX0.PSM), psm_str(dst_match->m_TEX0.PSM));
				shader = (fmt_16_bits) ? ShaderConvert::RGB5A1_TO_FLOAT16 : (ShaderConvert)(static_cast<int>(ShaderConvert::RGBA8_TO_FLOAT32) + psm_s.fmt);
			}
			else
			{
				GL_CACHE("TC: Lookup Target(Color) %dx%d, hit Depth (0x%x, %s was %s)", new_size.x, new_size.y, bp, psm_str(TEX0.PSM), psm_str(dst_match->m_TEX0.PSM));
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

		GL_CACHE("TC: Lookup %s(%s) %dx%d, miss (0x%x, %s)", is_frame ? "Frame" : "Target", to_string(type), size.x, size.y, bp, psm_str(TEX0.PSM));

		dst = CreateTarget(TEX0, size.x, size.y, type, true);
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
			const GSVector4i newrect = GSVector4i(0, 0, real_w, real_h);

			if (!is_frame && !forced_preload && !preload)
			{
				std::vector<GSState::GSUploadQueue>::iterator iter;
				for (iter = GSRendererHW::GetInstance()->m_draw_transfers.begin(); iter != GSRendererHW::GetInstance()->m_draw_transfers.end(); )
				{
					// If the format, and location doesn't match, but also the upload is at least the size of the target, don't preload.
					if (iter->blit.DBP == TEX0.TBP0 && GSUtil::HasCompatibleBits(iter->blit.DPSM, TEX0.PSM) && iter->rect.rintersect(newrect).eq(newrect))
					{
						GSRendererHW::GetInstance()->m_draw_transfers.erase(iter);
						GL_INS("Preloading the RT DATA");
						AddDirtyRectTarget(dst, newrect, TEX0.PSM, TEX0.TBW);
						dst->Update(true);
						break;
					}
					iter++;
				}
			}
			else
			{
				GL_INS("Preloading the RT DATA");
				AddDirtyRectTarget(dst, newrect, TEX0.PSM, TEX0.TBW);
				dst->Update(true);
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

	assert(dst && dst->m_texture && dst->m_texture->GetScale() == new_s);
	assert(dst && dst->m_dirty.empty());
	return dst;
}

GSTextureCache::Target* GSTextureCache::LookupDisplayTarget(const GIFRegTEX0& TEX0, const GSVector2i& size, const int real_w, const int real_h)
{
	return LookupTarget(TEX0, size, RenderTarget, true, 0, true, real_w, real_h);
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
	const int old_width = static_cast<int>(static_cast<float>(old_texture->GetWidth()) / old_texture->GetScale().x);
	const int old_height = static_cast<int>(static_cast<float>(old_texture->GetHeight()) / old_texture->GetScale().y);
	const int needed_height = std::min(real_h + y_offset, GSRendererHW::MAX_FRAMEBUFFER_HEIGHT);
	const int scaled_needed_height = std::max(static_cast<int>(static_cast<float>(needed_height) * old_texture->GetScale().y), old_texture->GetHeight());
	const int needed_width = std::min(real_w, static_cast<int>(dispfb.TBW * 64));
	const int scaled_needed_width = std::max(static_cast<int>(static_cast<float>(needed_width) * old_texture->GetScale().x), old_texture->GetWidth());
	if (scaled_needed_height <= old_texture->GetHeight() && scaled_needed_width <= old_texture->GetWidth())
		return;
	// We're expanding, so create a new texture.
	GSTexture* new_texture = g_gs_device->CreateRenderTarget(scaled_needed_width, scaled_needed_height, GSTexture::Format::Color, false);
	if (!new_texture)
	{
		// Memory allocation failure, do our best to hobble along.
		return;
	}
	// Keep the scale of the original texture.
	new_texture->SetScale(old_texture->GetScale());

	GL_CACHE("Expanding target for display output, target height %d @ 0x%X, display %d @ 0x%X offset %d needed %d",
		t->m_texture->GetHeight(), t->m_TEX0.TBP0, real_h, dispfb.TBP0, y_offset, needed_height);

	// Fill the new texture with the old data, and discard the old texture.
	g_gs_device->StretchRect(old_texture, new_texture, GSVector4(old_texture->GetSize()).zwxy(), ShaderConvert::COPY, false);
	m_target_memory_usage = (m_target_memory_usage - old_texture->GetMemUsage()) + new_texture->GetMemUsage();
	g_gs_device->Recycle(old_texture);
	t->m_texture = new_texture;

	// We unconditionally preload the frame here, because otherwise we'll end up with blackness for one frame (when the expand happens).
	const int preload_width = t->m_TEX0.TBW * 64;
	if (old_width < preload_width && old_height < needed_height)
	{
		const GSVector4i right(old_width, 0, preload_width, needed_height);
		const GSVector4i bottom(0, old_height, old_width, needed_height);
		AddDirtyRectTarget(t, right, t->m_TEX0.PSM, t->m_TEX0.TBW);
		AddDirtyRectTarget(t, bottom, t->m_TEX0.PSM, t->m_TEX0.TBW);
	}
	else
	{
		const GSVector4i newrect = GSVector4i((old_height < scaled_needed_height) ? 0 : old_width,
			(old_width < preload_width) ? 0 : old_height,
			preload_width, needed_height);
		AddDirtyRectTarget(t, newrect, t->m_TEX0.PSM, t->m_TEX0.TBW);
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

	for (auto i = list.begin(); i != list.end(); ++i)
	{
		Target* t = *i;

		if (TEX0.TBP0 == t->m_TEX0.TBP0 && TEX0.PSM == t->m_TEX0.PSM && t->Overlaps(TEX0.TBP0, TEX0.TBW, TEX0.PSM, r))
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
		const GSVector2i rect_scaled = GSVector2i(r.z * g_gs_renderer->GetUpscaleMultiplier(), aligned_height * g_gs_renderer->GetUpscaleMultiplier());
		const int upsc_width = std::max(rect_scaled.x, dst->m_texture->GetWidth());
		const int upsc_height = std::max(rect_scaled.y, dst->m_texture->GetHeight());
		if (dst->m_texture->GetWidth() < upsc_width || dst->m_texture->GetHeight() < upsc_height)
		{
			// We don't recycle here, because most of the time when this happens it's strange-sized textures
			// which are being expanded one-line-at-a-time.
			if (dst->ResizeTexture(upsc_width, upsc_height, false))
			{
				AddDirtyRectTarget(dst, r, TEX0.PSM, TEX0.TBW);
				GetTargetHeight(TEX0.TBP0, TEX0.TBW, TEX0.PSM, aligned_height);
				dst->UpdateValidity(r);
				dst->UpdateValidBits(GSLocalMemory::m_psm[TEX0.PSM].fmsk);
			}
		}
	}
	else
	{
		const GSVector4i clamped_r(
			r.rintersect(GSVector4i(0, 0, static_cast<int>(dst->m_texture->GetWidth() / dst->m_texture->GetScale().x),
				static_cast<int>(dst->m_texture->GetHeight() / dst->m_texture->GetScale().y))));
		AddDirtyRectTarget(dst, clamped_r, TEX0.PSM, TEX0.TBW);
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

	GSVector4i r = rect.ralign<Align_Outside>((bp & 31) == 0 ? GSLocalMemory::m_psm[psm].pgs : GSLocalMemory::m_psm[psm].bs);

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
	const u32 end_bp = off.bn(rect.z - 1, rect.w - 1);
	const u32 unwrapped_end_bp = end_bp + ((end_bp < bp) ? MAX_BLOCKS : 0);

	for (int type = 0; type < 2; type++)
	{
		auto& list = m_dst[type];
		for (auto i = list.begin(); i != list.end();)
		{
			auto j = i;
			Target* t = *j;

			// Don't bother checking any further if the target doesn't overlap with the write/invalidation.
			if ((bp < t->m_TEX0.TBP0 && unwrapped_end_bp < t->m_TEX0.TBP0) || bp > t->UnwrappedEndBlock())
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
				if (!found && GSUtil::HasCompatibleBits(psm, t->m_TEX0.PSM) && bw == t->m_TEX0.TBW)
				{
					GL_CACHE("TC: Dirty Target(%s) %d (0x%x) r(%d,%d,%d,%d)", to_string(type),
						t->m_texture ? t->m_texture->GetID() : 0,
						t->m_TEX0.TBP0, r.x, r.y, r.z, r.w);

					if (eewrite)
						t->m_age = 0;

					AddDirtyRectTarget(t, r, psm, bw);
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
						if (bw == t->m_TEX0.TBW && GSLocalMemory::m_psm[psm].bpp == GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp)
						{
							AddDirtyRectTarget(t, rect, psm, bw);
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
							const GSLocalMemory::psm_t& t_psm_s = GSLocalMemory::m_psm[psm];
							const u32 bp_end = t_psm_s.info.bn(r.z - 1, r.w - 1, bp, bw);
							if (GSLocalMemory::m_psm[psm].bpp == GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp ||
								((100.0f / static_cast<float>(t->m_end_block - t->m_TEX0.TBP0)) * static_cast<float>(bp_end - bp)) < 20.0f)
							{
								SurfaceOffset so = ComputeSurfaceOffset(off, r, t);
								if (so.is_valid)
								{
									AddDirtyRectTarget(t, so.b2a_offset, psm, bw);
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

					if (can_erase)
					{
						i = list.erase(j);
						GL_CACHE("TC: Remove Target(%s) %d (0x%x)", to_string(type),
							t->m_texture ? t->m_texture->GetID() : 0,
							t->m_TEX0.TBP0);
						delete t;
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

					// This grossness is needed to fix incorrect invalidations in True Crime: New York City.
					// Because it's writing tiny texture blocks (which are later decompressed) over previous targets,
					// we need to be ensure said targets are invalidated, otherwise the SW prim render path won't be
					// triggered. This whole thing needs rewriting anyway, because it can't handle non-page-aligned
					// writes, but for now we'll just use the unsafer logic when the TC hack is enabled.
					const bool start_of_page = rowsize > 0 && (offset % rowsize == 0);
					if (start_of_page || (rowsize > 0 && GSConfig.UserHacks_CPUSpriteRenderBW != 0))
					{
						int y = GSLocalMemory::m_psm[psm].pgs.y * offset / rowsize;

						if (r.bottom > y && (start_of_page || r.top >= y))
						{
							GL_CACHE("TC: Dirty After Target(%s) %d (0x%x)", to_string(type),
								t->m_texture ? t->m_texture->GetID() : 0,
								t->m_TEX0.TBP0);

							if (eewrite)
								t->m_age = 0;

							const GSVector4i dirty_r = GSVector4i(r.left, r.top - y, r.right, r.bottom - y);
							AddDirtyRectTarget(t, dirty_r, psm, bw);
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
						AddDirtyRectTarget(t, dirty_r, psm, bw);
						continue;
					}
				}
				else if (GSConfig.UserHacks_TextureInsideRt && t->Overlaps(bp, bw, psm, rect) && GSUtil::HasCompatibleBits(psm, t->m_TEX0.PSM))
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

						AddDirtyRectTarget(t, so.b2a_offset, t->m_TEX0.PSM, t->m_TEX0.TBW);
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

	GL_CACHE("TC: InvalidateLocalMem off(0x%x, %u, %s) r(%d, %d => %d, %d)",
		bp,
		bw,
		psm_str(psm),
		r.x,
		r.y,
		r.z,
		r.w);

	// No depth handling please.
	if (psm == PSM_PSMZ32 || psm == PSM_PSMZ24 || psm == PSM_PSMZ16 || psm == PSM_PSMZ16S)
	{
		GL_INS("ERROR: InvalidateLocalMem depth format isn't supported (%d,%d to %d,%d)", r.x, r.y, r.z, r.w);
		if (GSConfig.HWDownloadMode != GSHardwareDownloadMode::Enabled)
		{
			DevCon.Error("Skipping depth readback of %ux%u @ %u,%u", r.width(), r.height(), r.left, r.top);
			return;
		}

		if (!GSConfig.UserHacks_DisableDepthSupport)
		{
			auto& dss = m_dst[DepthStencil];
			for (auto it = dss.rbegin(); it != dss.rend(); ++it) // Iterate targets from LRU to MRU.
			{
				Target* t = *it;

				if (!t->Overlaps(bp, bw, psm, r) || !GSUtil::HasSharedBits(psm, t->m_TEX0.PSM) || t->m_age >= 30)
					continue;

				const bool bpp_match = GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp == GSLocalMemory::m_psm[psm].bpp;
				const bool format_match = (bp == t->m_TEX0.TBP0 && bw == t->m_TEX0.TBW && bpp_match);
				// Calculate the rect offset if the BP doesn't match.
				const GSVector4i targetr = (format_match) ? r.rintersect(t->m_valid) : ComputeSurfaceOffset(bp, bw, psm, r, t).b2a_offset;
				const GSVector4i draw_rect = (t->readbacks_since_draw > 0) ? t->m_drawn_since_read : targetr.rintersect(t->m_drawn_since_read);
				Read(t, draw_rect);

				t->readbacks_since_draw++;
				if(draw_rect.rintersect(t->m_drawn_since_read).eq(t->m_drawn_since_read))
					t->m_drawn_since_read = GSVector4i::zero();
			}
		}
		return;
	}

	// This is a shorter but potentially slower version of the below, commented out code.
	// It works for all the games mentioned below and fixes a couple of other ones as well
	// (Busen0: Wizardry and Chaos Legion).
	// Also in a few games the below code ran the Grandia3 case when it shouldn't :p
	auto& rts = m_dst[RenderTarget];
	for (auto it = rts.rbegin(); it != rts.rend(); ++it) // Iterate targets from LRU to MRU.
	{
		Target* t = *it;
		if (t->m_TEX0.PSM != PSM_PSMZ32 && t->m_TEX0.PSM != PSM_PSMZ24 && t->m_TEX0.PSM != PSM_PSMZ16 && t->m_TEX0.PSM != PSM_PSMZ16S)
		{
			if (!t->Overlaps(bp, bw, psm, r) || !GSUtil::HasSharedBits(psm, t->m_TEX0.PSM) || t->m_age >= 30)
				continue;

			const bool bpp_match = GSLocalMemory::m_psm[t->m_TEX0.PSM].bpp == GSLocalMemory::m_psm[psm].bpp;
			const bool format_match = (bp == t->m_TEX0.TBP0 && bw == t->m_TEX0.TBW && bpp_match);
			// Calculate the rect offset if the BP doesn't match.
			const GSVector4i targetr = (format_match) ? r.rintersect(t->m_valid) : ComputeSurfaceOffset(bp, bw, psm, r, t).b2a_offset;

			// Some games like to offset their GS download memory addresses by
			// using overly big source Y position values.
			// Checking for targets that overlap with the requested memory region
			// instead of just comparing TBPs should fix that.
			// For example, this fixes Judgement ring rendering in Shadow Hearts 2.
			// Be wary of old targets being misdetected, set a sensible range of 30 frames (like Display source lookups).
			if (!targetr.rempty())
			{
				// GH Note: Read will do a StretchRect and then will sizzle data to the GS memory
				// t->m_valid will do the full target texture whereas r.intersect(t->m_valid) will be limited
				// to the useful part for the transfer.
				// 1/ Logically intersect must be enough, except if we miss some call to InvalidateLocalMem
				// or it need the depth part too
				// 2/ Read function is slow but I suspect the swizzle part to be costly. Maybe a compute shader
				// that do the swizzle at the same time of the Stretching could save CPU computation.

				// note: r.rintersect breaks Wizardry and Chaos Legion
				// Read(t, t->m_valid) works in all tested games but is very slow in GUST titles ><
				// Update: 18/02/2023: Chaos legion breaks because it reads the width at half of the real width.
				// Surface offset deals with this.

				// If the game has been spamming downloads, we've already read the whole texture back at this point.
				if (t->m_drawn_since_read.rempty())
					continue;

				// propagate the format from the result of a channel effect
				// texture is 16/8 bit but the real data is 32
				// common use for shuffling is moving data into the alpha channel
				// the game can then draw using 8H format
				// in the case of silent hill blit 8H -> 8P
				// this will matter later when the data ends up in GS memory in the wrong format
				// Be careful to avoid 24 bit textures which are technically 32bit, as you could lose alpha (8H) data.
				if (t->m_32_bits_fmt && t->m_TEX0.PSM > PSM_PSMCT24)
					t->m_TEX0.PSM = PSM_PSMCT32;

				if (GSConfig.HWDownloadMode != GSHardwareDownloadMode::Enabled)
				{
					const GSVector4i rb_rc((!GSConfig.UserHacks_DisablePartialInvalidation && targetr.x == 0 && targetr.y == 0) ? t->m_valid : targetr.rintersect(t->m_valid));
					DevCon.Error("Skipping depth readback of %ux%u @ %u,%u", rb_rc.width(), rb_rc.height(), rb_rc.left, rb_rc.top);
				}
				else
				{
					// If it's a download to the EE, or we've done multiple reads of the same texture between draws, just get it all.
					if (!GSConfig.UserHacks_DisablePartialInvalidation && t->readbacks_since_draw > 0)
					{
						Read(t, t->m_drawn_since_read);
						t->m_drawn_since_read = GSVector4i::zero();
						t->readbacks_since_draw++;
					}
					else if(!targetr.rintersect(t->m_drawn_since_read).rempty()) // Block level read?
					{
						// Read the width of the draw, reading too much could wipe out dirty memory.
						GSVector4i full_lines = GSVector4i(0, targetr.y, t->m_drawn_since_read.z, targetr.w);
						full_lines = targetr.rintersect(t->m_drawn_since_read);
						
						Read(t, full_lines);

						// After reading, try to cut down our "dirty" rect.
						if (full_lines.rintersect(t->m_drawn_since_read).eq(t->m_drawn_since_read))
							t->m_drawn_since_read = GSVector4i::zero();
						else
						{
							// Try to cut down how much we read next, if we can.
							// Fatal Frame reads in vertical strips, SOCOM 2 does horizontal, so we can handle that below.
							if (full_lines.width() == t->m_drawn_since_read.width()
								&& full_lines.w >= t->m_drawn_since_read.y)
							{
								if (full_lines.y <= t->m_drawn_since_read.y)
									t->m_drawn_since_read.y = full_lines.w;
								else if (full_lines.w >= t->m_drawn_since_read.w)
									t->m_drawn_since_read.w = full_lines.y;
							}
							else if (full_lines.height() == t->m_drawn_since_read.height()
									&& full_lines.z >= t->m_drawn_since_read.x)
							{
								if (full_lines.x <= t->m_drawn_since_read.x)
									t->m_drawn_since_read.x = full_lines.z;
								else if (full_lines.z >= t->m_drawn_since_read.z)
									t->m_drawn_since_read.z = full_lines.x;
							}
						}
						t->readbacks_since_draw++;
					}
				}
			}
		}
		else
		{
			GL_INS("ERROR: InvalidateLocalMem target is a depth format");
		}
	}

	//GSTextureCache::Target* rt2 = NULL;
	//int ymin = INT_MAX;
	//for(auto i = m_dst[RenderTarget].begin(); i != m_dst[RenderTarget].end(); )
	//{
	//	auto j = i++;

	//	Target* t = *j;

	//	if (t->m_TEX0.PSM != PSM_PSMZ32 && t->m_TEX0.PSM != PSM_PSMZ24 && t->m_TEX0.PSM != PSM_PSMZ16 && t->m_TEX0.PSM != PSM_PSMZ16S)
	//	{
	//		if (GSUtil::HasSharedBits(bp, psm, t->m_TEX0.TBP0, t->m_TEX0.PSM))
	//		{
	//			if (GSUtil::HasCompatibleBits(psm, t->m_TEX0.PSM))
	//			{
	//				Read(t, r.rintersect(t->m_valid));
	//				return;
	//			}
	//			else if (psm == PSM_PSMCT32 && (t->m_TEX0.PSM == PSM_PSMCT16 || t->m_TEX0.PSM == PSM_PSMCT16S))
	//			{
	//				// ffx-2 riku changing to her default (shoots some reflecting glass at the end), 16-bit rt read as 32-bit
	//				Read(t, GSVector4i(r.left, r.top, r.right, r.top + (r.bottom - r.top) * 2).rintersect(t->m_valid));
	//				return;
	//			}
	//			else
	//			{
	//				if (psm == PSM_PSMT4HH && t->m_TEX0.PSM == PSM_PSMCT32)
	//				{
	//					// Silent Hill Origins shadows: Read 8 bit using only the HIGH bits (4 bit) texture as 32 bit.
	//					Read(t, r.rintersect(t->m_valid));
	//					return;
	//				}
	//				else
	//				{
	//					//printf("Trashing render target. We have a %d type texture and we are trying to write into a %d type texture\n", t->m_TEX0.PSM, psm);
	//					m_dst[RenderTarget].erase(j);
	//					delete t;
	//				}
	//			}
	//		}

	//		// Grandia3, FFX, FFX-2 pause menus. t->m_TEX0.TBP0 magic number checks because otherwise kills xs2 videos
	//		if ((GSUtil::HasSharedBits(psm, t->m_TEX0.PSM) && (bp > t->m_TEX0.TBP0))
	//			&& ((t->m_TEX0.TBP0 == 0) || (t->m_TEX0.TBP0==3328) || (t->m_TEX0.TBP0==3584)))
	//		{
	//			//printf("first : %d-%d child : %d-%d\n", psm, bp, t->m_TEX0.PSM, t->m_TEX0.TBP0);
	//			u32 rowsize = bw * 8192;
	//			u32 offset = (u32)((bp - t->m_TEX0.TBP0) * 256);

	//			if (rowsize > 0 && offset % rowsize == 0)
	//			{
	//				int y = GSLocalMemory::m_psm[psm].pgs.y * offset / rowsize;

	//				if (y < ymin && y < 512)
	//				{
	//					rt2 = t;
	//					ymin = y;
	//				}
	//			}
	//		}
	//	}
	//}
	//if (rt2)
	//{
	//	Read(rt2, GSVector4i(r.left, r.top + ymin, r.right, r.bottom + ymin));
	//}


	// TODO: ds
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
	if (src && !dst && dx == 0 && dy == 0 && ((static_cast<u32>(w) + 63) / 64) == DBW)
	{
		GIFRegTEX0 new_TEX0 = {};
		new_TEX0.TBP0 = DBP;
		new_TEX0.TBW = DBW;
		new_TEX0.PSM = DPSM;

		const int real_height = GetTargetHeight(DBP, DBW, DPSM, h);
		const GSVector2 scale(src->m_texture->GetScale());
		dst = LookupTarget(new_TEX0, GSVector2i(static_cast<int>(w * scale.x), static_cast<int>(real_height * scale.y)), src->m_type, true);
		if (dst)
		{
			dst->m_texture->SetScale(scale);
			dst->UpdateValidity(GSVector4i(dx, dy, dx + w, dy + h));
			dst->m_valid_bits = src->m_valid_bits;
		}
	}

	if (!src || !dst || src->m_texture->GetScale() != dst->m_texture->GetScale())
		return false;

	// Scale coordinates.
	const GSVector2 scale(src->m_texture->GetScale());
	const int scaled_sx = static_cast<int>(sx * scale.x);
	const int scaled_sy = static_cast<int>(sy * scale.y);
	const int scaled_dx = static_cast<int>(dx * scale.x);
	const int scaled_dy = static_cast<int>(dy * scale.y);
	const int scaled_w = static_cast<int>(w * scale.x);
	const int scaled_h = static_cast<int>(h * scale.y);

	// The source isn't in our texture, otherwise it could falsely expand the texture causing a misdetection later, which then renders black.
	if ((scaled_sx + scaled_w) > src->m_texture->GetWidth() || (scaled_sy + scaled_h) > src->m_texture->GetHeight())
		return false;

	// We don't want to copy "old" data that the game has overwritten with writes,
	// so flush any overlapping dirty area.
	src->UpdateIfDirtyIntersects(GSVector4i(sx, sy, sx + w, sy + h));
	dst->UpdateIfDirtyIntersects(GSVector4i(dx, dy, dx + w, dy + h));

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
		const int scaled_new_height = static_cast<int>(static_cast<float>(new_height) * scale.y);
		GL_INS("Resize %dx%d target to %dx%d for move", dst->m_texture->GetWidth(), dst->m_texture->GetHeight(), dst->m_texture->GetHeight(), scaled_new_height);
		GetTargetHeight(DBP, DBW, DPSM, new_height);

		if (!dst->ResizeTexture(dst->m_texture->GetWidth(), scaled_new_height, false))
		{
			// Resize failed, probably ran out of VRAM, better luck next time. Fall back to CPU.
			// We injected the new height into the cache, so hopefully won't happen again.
			return false;
		}
	}

	// Make sure the copy doesn't go out of bounds (it shouldn't).
	if ((scaled_dx + scaled_w) > dst->m_texture->GetWidth() || (scaled_dy + scaled_h) > dst->m_texture->GetHeight())
		return false;

	g_gs_device->CopyRect(src->m_texture, dst->m_texture,
		GSVector4i(scaled_sx, scaled_sy, scaled_sx + scaled_w, scaled_sy + scaled_h),
		scaled_dx, scaled_dy);

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

	static constexpr u32 indices[6] = {0, 1, 2, 2, 1, 3};

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
	config.drawarea = GSVector4i(GSVector4(bbox) * GSVector4(tgt->m_texture->GetScale()).xxyy());
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
	const GSVector2 rtscale(tgt->m_texture->GetScale());
	config.cb_ps.WH = GSVector4(static_cast<float>(rtsize.x) / rtscale.x, static_cast<float>(rtsize.y) / rtscale.y,
		static_cast<float>(rtsize.x), static_cast<float>(rtsize.y));
	config.cb_ps.STScale = rtscale;

	config.cb_vs.vertex_scale = GSVector2(2.0f * rtscale.x / (rtsize.x << 4), 2.0f * rtscale.y / (rtsize.y << 4));
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
	int tw = region.IsFixedTEX0W(1 << TEX0.TW) ? region.GetWidth() : (1 << TEX0.TW);
	int th = region.IsFixedTEX0H(1 << TEX0.TH) ? region.GetHeight() : (1 << TEX0.TH);

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
		const GSVector2 scale(dst->m_texture->GetScale());
		const int x = static_cast<int>(scale.x * x_offset);
		const int y = static_cast<int>(scale.y * y_offset);
		const int w = static_cast<int>(scale.x * tw);
		const int h = static_cast<int>(scale.y * th);

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
		src->m_target = true;
		src->m_from_target = &dst->m_texture;
		src->m_from_target_TEX0 = dst->m_TEX0;
		src->m_texture->SetScale(scale);
		src->m_end_block = dst->m_end_block;

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
		src->m_target = true;
		src->m_shared_texture = true;
		src->m_from_target = &dst->m_texture;
		src->m_from_target_TEX0 = dst->m_TEX0;
		src->m_end_block = dst->m_end_block;
		src->m_texture->SetScale(dst->m_texture->GetScale());
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
		src->m_from_target = &dst->m_texture;
		src->m_from_target_TEX0 = dst->m_TEX0;
		src->m_valid_rect = dst->m_valid;
		src->m_end_block = dst->m_end_block;

		dst->Update(true);

		// do not round here!!! if edge becomes a black pixel and addressing mode is clamp => everything outside the clamped area turns into black (kh2 shadows)

		const GSVector2i dstsize = dst->m_texture->GetSize();

		int w = std::min(dstsize.x, static_cast<int>(dst->m_texture->GetScale().x * tw));
		int h = std::min(dstsize.y, static_cast<int>(dst->m_texture->GetScale().y * th));
		if (is_8bits)
		{
			// Unscale 8 bits textures, quality won't be nice but format is really awful
			w = tw;
			h = th;
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

		const GSVector2 scale = is_8bits ? GSVector2(1, 1) : dst->m_texture->GetScale();

		const bool use_texture = shader == ShaderConvert::COPY;
		GSVector4i sRect(0, 0, w, h);
		int destX = 0;
		int destY = 0;

		if (half_right)
		{
			// You typically hit this code in snow engine game. Dstsize is the size of of Dx/GL RT
			// which is set to some arbitrary number. h/w are based on the input texture
			// so the only reliable way to find the real size of the target is to use the TBW value.
			int half_width = static_cast<int>(dst->m_TEX0.TBW * (64 / 2) * dst->m_texture->GetScale().x);
			if (half_width < dstsize.x)
			{
				int copy_width = std::min(half_width, dstsize.x - half_width);
				sRect.x = half_width;
				sRect.z = half_width + copy_width;
				w = copy_width;
			}
			else
			{
				DevCon.Error("Invalid half-right copy with width %d from %dx%d texture", half_width * 2, w, h);
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

		// Assuming everything matches up, instead of copying the target, we can just sample it directly.
		// It's the same as doing the copy first, except we save GPU time.
		if (!half_right && // not the size change from above
			use_texture && // not reinterpreting the RT
			w == dst->m_texture->GetWidth() && h == dst->m_texture->GetHeight() && // same dimensions
			!m_temporary_source // not the shuffle case above
			)
		{
			// sample the target directly
			src->m_texture = dst->m_texture;
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
								  g_gs_device->CreateTexture(w, h, 1, GSTexture::Format::Color, true) :
								  g_gs_device->CreateRenderTarget(w, h, GSTexture::Format::Color, false);
			dTex->SetScale(scale);
			m_source_memory_usage += dTex->GetMemUsage();

			if (use_texture)
			{
				g_gs_device->CopyRect(sTex, dTex, sRect, destX, destY);
			}
			else
			{
				GSVector4 sRectF(sRect);
				sRectF.z /= sTex->GetWidth();
				sRectF.w /= sTex->GetHeight();

				g_gs_device->StretchRect(sTex, sRectF, dTex, GSVector4(destX, destY, w, h), shader, false);
			}

			src->m_texture = dTex;
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

		dst->m_texture->OffsetHack_modxy = modxy;
	}
	else
	{
		// maintain the clut even when paltex is on for the dump/replacement texture lookup
		bool paltex = (GSConfig.GPUPaletteConversion && psm.pal > 0) || gpu_clut;
		const u32* clut = (psm.pal > 0) ? static_cast<const u32*>(g_gs_renderer->m_mem.m_clut) : nullptr;

		// adjust texture size to fit
		src->m_region = region;
		tw = region.HasX() ? region.GetWidth() : tw;
		th = region.HasY() ? region.GetHeight() : th;

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
	ASSERT(src->m_texture->GetScale() == ((!dst || TEX0.PSM == PSM_PSMT8) ? GSVector2(1, 1) : dst->m_texture->GetScale()));

	src->SetPages();

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
	HashCacheKey key{HashCacheKey::Create(TEX0, TEXA, (dump || replace || !paltex) ? clut : nullptr, lod, region)};

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
				const GIFRegTEX0 MIP_TEX0{g_gs_renderer->GetTex0Layer(basemip + mip)};
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
			const HashCacheEntry entry{replacement_tex, 1u, 0u, true};
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
			const GIFRegTEX0 MIP_TEX0{g_gs_renderer->GetTex0Layer(basemip + mip)};
			PreloadTexture(MIP_TEX0, TEXA, region.AdjustForMipmap(mip), g_gs_renderer->m_mem, paltex, tex, mip);
		}
	}

	// remove the palette hash when using paltex/indexed
	if (paltex)
		key.RemoveCLUTHash();

	// insert into the cache cache, and we're done
	const HashCacheEntry entry{tex, 1u, 0u, false};
	m_hash_cache_memory_usage += tex->GetMemUsage();
	return &m_hash_cache.emplace(key, entry).first->second;
}

GSTextureCache::Target* GSTextureCache::CreateTarget(const GIFRegTEX0& TEX0, int w, int h, int type, const bool clear)
{
	ASSERT(type == RenderTarget || type == DepthStencil);

	Target* t = new Target(TEX0, !GSConfig.UserHacks_DisableDepthSupport, type);

	// FIXME: initial data should be unswizzled from local mem in Update() if dirty

	if (type == RenderTarget)
	{
		t->m_texture = g_gs_device->CreateRenderTarget(w, h, GSTexture::Format::Color, clear);

		t->m_used = true; // FIXME
	}
	else if (type == DepthStencil)
	{
		t->m_texture = g_gs_device->CreateDepthStencil(w, h, GSTexture::Format::DepthStencil, clear);
	}

	t->m_texture->SetScale(static_cast<GSRendererHW*>(g_gs_renderer.get())->GetTextureScaleFactor());
	m_target_memory_usage += t->m_texture->GetMemUsage();

	m_dst[type].push_front(t);

	return t;
}

GSTexture* GSTextureCache::LookupPaletteSource(u32 CBP, u32 CPSM, u32 CBW, GSVector2i& offset, const GSVector2i& size)
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
		Console.Warning("Not reading back target %x PSM %s due to no write mask", TEX0.TBP0, psm_str(TEX0.PSM));
		return;
	}

	// Yes lots of logging, but I'm not confident with this code
	GL_PUSH("Texture Cache Read. Format(0x%x)", TEX0.PSM);

	GL_PERF("TC: Read Back Target: %d (0x%x)[fmt: 0x%x]. Size %dx%d", t->m_texture->GetID(), TEX0.TBP0, TEX0.PSM, r.width(), r.height());

	const GSVector4 src(GSVector4(r) * GSVector4(t->m_texture->GetScale()).xyxy() / GSVector4(t->m_texture->GetSize()).xyxy());
	const GSVector4i drc(0, 0, r.width(), r.height());
	const bool direct_read = (t->m_texture->GetScale() == GSVector2(1, 1) && ps_shader == ShaderConvert::COPY);

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

GSTextureCache::Surface::Surface()
	: m_texture(NULL)
	, m_from_hash_cache(NULL)
	, m_age(0)
	, m_32_bits_fmt(false)
	, m_shared_texture(false)
	, m_end_block(0)
{
	m_TEX0.TBP0 = GSTextureCache::MAX_BP;
}

GSTextureCache::Surface::~Surface() = default;

void GSTextureCache::Surface::UpdateAge()
{
	m_age = 0;
}

bool GSTextureCache::Surface::Inside(u32 bp, u32 bw, u32 psm, const GSVector4i& rect)
{
	// Valid only for color formats.
	const u32 end_block = GSLocalMemory::m_psm[psm].info.bn(rect.z - 1, rect.w - 1, bp, bw);
	return bp >= m_TEX0.TBP0 && end_block <= m_end_block;
}

bool GSTextureCache::Surface::Overlaps(u32 bp, u32 bw, u32 psm, const GSVector4i& rect)
{
	// Valid only for color formats.
	const u32 end_block = GSLocalMemory::m_psm[psm].info.bn(rect.z - 1, rect.w - 1, bp, bw);
	const u32 start_block = GSLocalMemory::m_psm[psm].info.bn(rect.x, rect.y, bp, bw);
	const bool overlap = GSTextureCache::CheckOverlap(m_TEX0.TBP0, m_end_block, start_block, end_block);
	return overlap;
}

// GSTextureCache::Source

GSTextureCache::Source::Source(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA)
	: m_palette_obj(nullptr)
	, m_palette(nullptr)
	, m_valid_rect(0, 0)
	, m_lod(0, 0)
	, m_target(false)
	, m_repeating(false)
	, m_p2t(NULL)
	, m_from_target(NULL)
	, m_from_target_TEX0(TEX0)
{
	m_TEX0 = TEX0;
	m_TEXA = TEXA;

	memset(m_layer_TEX0, 0, sizeof(m_layer_TEX0));
	memset(m_layer_hash, 0, sizeof(m_layer_hash));
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

	if (m_repeating && !CanPreload())
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
				const u32 block = bn.valueNoWrap();

				if (block < MAX_BLOCKS || GSConfig.WrapGSMem)
				{
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
	}
	else
	{
		for (int y = r.top; y < r.bottom; y += bs.y, bn.nextBlockY())
		{
			for (int x = r.left; x < r.right; x += bs.x, bn.nextBlockX())
			{
				u32 block = bn.valueNoWrap();

				if (block < MAX_BLOCKS || GSConfig.WrapGSMem)
				{
					block %= MAX_BLOCKS;

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
	, m_used(false)
	, m_valid(GSVector4i::zero())
	, m_depth_supported(depth_supported)
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

	if (m_dirty.size() <= 0)
		return;

	// No handling please
	if ((m_type == DepthStencil) && !m_depth_supported)
	{
		// do the most likely thing a direct write would do, clear it
		GL_INS("ERROR: Update DepthStencil dummy");
		m_dirty.clear();
		return;
	}

	const GSVector2i unscaled_size(static_cast<int>(m_texture->GetWidth() / m_texture->GetScale().x),
		static_cast<int>(m_texture->GetHeight() / m_texture->GetScale().y));
	const GSVector4i total_rect(m_dirty.GetTotalRect(m_TEX0, unscaled_size));
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
	}

	if (mapped)
		t->Unmap();

	for (size_t i = 0; i < m_dirty.size(); i++)
	{
		const GSVector4i r(m_dirty.GetDirtyRect(i, m_TEX0, total_rect));
		if (r.rempty())
			continue;

		const GSVector4 sRect(GSVector4(r - t_offset) / t_sizef);
		const GSVector4 dRect(GSVector4(r) * GSVector4(m_texture->GetScale()).xyxy());

		// Copy the new GS memory content into the destination texture.
		if (m_type == RenderTarget)
		{
			GL_INS("ERROR: Update RenderTarget 0x%x bw:%d (%d,%d => %d,%d)", m_TEX0.TBP0, m_TEX0.TBW, r.x, r.y, r.z, r.w);

			// Bilinear filtering this is probably not a good thing, at least in native, but upscaling Nearest can be gross and messy.
			g_gs_device->StretchRect(t, sRect, m_texture, dRect, ShaderConvert::COPY, g_gs_renderer->CanUpscale());
		}
		else if (m_type == DepthStencil)
		{
			GL_INS("ERROR: Update DepthStencil 0x%x", m_TEX0.TBP0);

			// FIXME linear or not?
			g_gs_device->StretchRect(t, sRect, m_texture, dRect, ShaderConvert::RGBA8_TO_FLOAT32_BILN);
		}
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

void GSTextureCache::Target::ResizeValidity(const GSVector4i& rect)
{
	if (!m_valid.eq(GSVector4i::zero()))
	{
		m_valid = m_valid.rintersect(rect);
		m_drawn_since_read = m_drawn_since_read.rintersect(rect);
	}
	else
	{
		// No valid size, so need to resize down.
		return;
	}
	// Block of the bottom right texel of the validity rectangle, last valid block of the texture
	// TODO: This is not correct when the PSM changes. e.g. a 512x448 target being shuffled will become 512x896 temporarily, and
	// at the moment, we blow the valid rect out to twice the size. The only thing stopping everything breaking is the fact
	// that we clamp the draw rect to the target size in GSRendererHW::Draw().
	m_end_block = GSLocalMemory::m_psm[m_TEX0.PSM].info.bn(m_valid.z - 1, m_valid.w - 1, m_TEX0.TBP0, m_TEX0.TBW); // Valid only for color formats

	// GL_CACHE("UpdateValidity (0x%x->0x%x) from R:%d,%d Valid: %d,%d", m_TEX0.TBP0, m_end_block, rect.z, rect.w, m_valid.z, m_valid.w);
}

void GSTextureCache::Target::UpdateValidity(const GSVector4i& rect, bool can_resize)
{
	if (m_valid.runion(rect).eq(m_valid))
	{
		m_drawn_since_read = m_drawn_since_read.runion(rect);
		return;
	}

	if (can_resize)
	{
		if (m_valid.eq(GSVector4i::zero()))
			m_valid = rect;
		else
			m_valid = m_valid.runion(rect);
	}

	if (m_drawn_since_read.eq(GSVector4i::zero()) || !can_resize)
	{
		m_drawn_since_read = rect.rintersect(m_valid);
	}
	else
		m_drawn_since_read = m_drawn_since_read.runion(rect);
	// Block of the bottom right texel of the validity rectangle, last valid block of the texture
	// TODO: This is not correct when the PSM changes. e.g. a 512x448 target being shuffled will become 512x896 temporarily, and
	// at the moment, we blow the valid rect out to twice the size. The only thing stopping everything breaking is the fact
	// that we clamp the draw rect to the target size in GSRendererHW::Draw().
	m_end_block = GSLocalMemory::m_psm[m_TEX0.PSM].info.bn(m_valid.z - 1, m_valid.w - 1, m_TEX0.TBP0, m_TEX0.TBW); // Valid only for color formats
	// GL_CACHE("UpdateValidity (0x%x->0x%x) from R:%d,%d Valid: %d,%d", m_TEX0.TBP0, m_end_block, rect.z, rect.w, m_valid.z, m_valid.w);
}

void GSTextureCache::Target::UpdateValidBits(u32 bits_written)
{
	m_valid_bits |= bits_written;
}

bool GSTextureCache::Target::ResizeTexture(int new_width, int new_height, bool recycle_old)
{
	return ResizeTexture(new_width, new_height, m_texture->GetScale(), recycle_old);
}

bool GSTextureCache::Target::ResizeTexture(int new_width, int new_height, GSVector2 new_scale, bool recycle_old)
{
	const int width = m_texture->GetWidth();
	const int height = m_texture->GetHeight();
	if (width == new_width && height == new_height)
		return true;

	// These exceptions *really* need to get lost. This gets called outside of draws, which just crashes
	// when it tries to propogate the exception back.
	const bool clear = (new_width > width || new_height > height);

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

	tex->SetScale(new_scale);

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

	return true;
}

// GSTextureCache::SourceMap

void GSTextureCache::SourceMap::Add(Source* s, const GIFRegTEX0& TEX0, const GSOffset& off)
{
	m_surfaces.insert(s);

	if (s->m_target)
	{
		// TODO

		// GH: I don't know why but it seems we only consider the first page for a render target
		const size_t page = TEX0.TBP0 >> 5;

		s->m_erase_it[page] = m_map[page].InsertFront(s);

		return;
	}

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

	if (s->m_target)
	{
		const size_t page = s->m_TEX0.TBP0 >> 5;
		m_map[page].EraseIndex(s->m_erase_it[page]);
	}
	else
	{
		s->m_pages.loopPages([this, s](u32 page)
		{
			m_map[page].EraseIndex(s->m_erase_it[page]);
		});
	}

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
	SourceRegion ret = {};
	if (HasX())
	{
		const u32 new_minx = GetMinX() >> level;
		const u32 new_maxx = ((GetMaxX() - 1) >> level) + 1;
		ret.SetX(new_minx, new_maxx);
	}
	if (HasY())
	{
		const u32 new_miny = GetMinY() >> level;
		const u32 new_maxy = ((GetMaxY() - 1) >> level) + 1;
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
