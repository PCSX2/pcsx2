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
#include "GSTextureCache.h"

bool s_IS_OPENGL = false;

GSTextureCache::GSTextureCache(GSRenderer* r)
	: m_renderer(r)
{
	bool userhacks = !!theApp.GetConfig("UserHacks", 0);
	s_IS_OPENGL = (static_cast<GSRendererType>(theApp.GetConfig("Renderer", static_cast<int>(GSRendererType::Default))) == GSRendererType::OGL_HW);

	m_spritehack = userhacks ? theApp.GetConfig("UserHacks_SpriteHack", 0) : 0;
	UserHacks_HalfPixelOffset = userhacks && theApp.GetConfig("UserHacks_HalfPixelOffset", 0);

	m_paltex = !!theApp.GetConfig("paltex", 0);
	m_preload_frame = userhacks && theApp.GetConfig("preload_frame_with_gs_data", 0);
	m_can_convert_depth = s_IS_OPENGL && theApp.GetConfig("texture_cache_depth", 1);
	m_crc_hack_level = theApp.GetConfig("crc_hack_level", 3);
	
	m_temp = (uint8*)_aligned_malloc(1024 * 1024 * sizeof(uint32), 32);
}

GSTextureCache::~GSTextureCache()
{
	RemoveAll();

	_aligned_free(m_temp);
}

void GSTextureCache::RemovePartial()
{
	//m_src.RemoveAll();

	for (int type = 0; type < 2; type++)
	{
		for_each(m_dst[type].begin(), m_dst[type].end(), delete_object());

		m_dst[type].clear();
	}
}

void GSTextureCache::RemoveAll()
{
	m_src.RemoveAll();

	for(int type = 0; type < 2; type++)
	{
		for_each(m_dst[type].begin(), m_dst[type].end(), delete_object());

		m_dst[type].clear();
	}
}

GSTextureCache::Source* GSTextureCache::LookupSource(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, const GSVector4i& r)
{
	const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[TEX0.PSM];
	//const GSLocalMemory::psm_t& cpsm = psm.pal > 0 ? GSLocalMemory::m_psm[TEX0.CPSM] : psm;

	// Until DX is fixed
	if (s_IS_OPENGL) {
		if(psm.pal > 0)
			m_renderer->m_mem.m_clut.Read32(TEX0, TEXA);
	} else {
		GIFRegTEXA plainTEXA;

		plainTEXA.AEM = 1;
		plainTEXA.TA0 = 0;
		plainTEXA.TA1 = 0x80;
		m_renderer->m_mem.m_clut.Read32(TEX0, plainTEXA);
	}

	const uint32* clut = m_renderer->m_mem.m_clut;

	Source* src = NULL;

	list<Source*>& m = m_src.m_map[TEX0.TBP0 >> 5];


	for(list<Source*>::iterator i = m.begin(); i != m.end(); i++)
	{
		Source* s = *i;

		if (((TEX0.u32[0] ^ s->m_TEX0.u32[0]) | ((TEX0.u32[1] ^ s->m_TEX0.u32[1]) & 3)) != 0) // TBP0 TBW PSM TW TH
			continue;

		// Target are converted (AEM & palette) on the fly by the GPU. They don't need extra check
		if (!s->m_target) {
			// We request a palette texture (psm.pal). If the texture was
			// converted by the CPU (s->m_palette == NULL), we need to ensure
			// palette content is the same.
			// Note: content of the palette will be uploaded at the end of the function
			if (psm.pal > 0 && s->m_palette == NULL && !GSVector4i::compare64(clut, s->m_clut, psm.pal * sizeof(clut[0])))
				continue;

			// We request a 24/16 bit RGBA texture. Alpha expansion was done by
			// the CPU.  We need to check that TEXA is identical
			if (psm.pal == 0 && psm.fmt > 0 && s->m_TEXA.u64 != TEXA.u64)
				continue;
		}

		m.splice(m.begin(), m, i);

		src = s;

		break;
	}

	Target* dst = NULL;
	bool half_right = false;

#ifdef DISABLE_HW_TEXTURE_CACHE
	if( 0 )
#else
	if(src == NULL)
#endif
	{
		uint32 bp = TEX0.TBP0;
		uint32 psm = TEX0.PSM;

		// Arc the Lad finds the wrong surface here when looking for a depth stencil.
		// Since we're currently not caching depth stencils (check ToDo in CreateSource) we should not look for it here.

		// (Simply not doing this code at all makes a lot of previsouly missing stuff show (but breaks pretty much everything
		// else.)

		for(list<Target*>::iterator i = m_dst[RenderTarget].begin(); i != m_dst[RenderTarget].end(); i++)
		{
			Target* t = *i;

			if(t->m_used && t->m_dirty.empty()) {
				// Typical bug (MGS3 blue cloud):
				// 1/ RT used as 32 bits => alpha channel written
				// 2/ RT used as 24 bits => no update of alpha channel
				// 3/ Lookup of texture that used alpha channel as index, HasSharedBits will return false
				//    because of the previous draw call format
				//
				// Solution: consider the RT as 32 bits if the alpha was used in the past
				uint32 t_psm = (t->m_dirty_alpha) ? t->m_TEX0.PSM & ~0x1 : t->m_TEX0.PSM;

				if (GSUtil::HasSharedBits(bp, psm, t->m_TEX0.TBP0, t_psm)) {
					if (!s_IS_OPENGL && (psm == PSM_PSMT8)) {
						// OpenGL can convert the texture directly in the GPU. Not sure we want to keep this
						// code for DX. It fixes effect but it is slow (MGS3)

						// It is a complex to convert the code in shader. As a reference, let's do it on the CPU, it will
						// be slow but
						// 1/ it just works :)
						// 2/ even with upscaling
						// 3/ for both DX and OpenGL

						// Gregory: to avoid a massive slow down for nothing, let's only enable
						// this code when CRC is below the FULL level
						if (m_crc_hack_level < 3)
							Read(t, t->m_valid);
						else
							dst = t;
					} else {
						dst = t;
					}

					break;

				} else if ((t->m_TEX0.TBW >= 16) && GSUtil::HasSharedBits(bp, psm, t->m_TEX0.TBP0 + t->m_TEX0.TBW * 0x10, t->m_TEX0.PSM)) {
					// Detect half of the render target (fix snow engine game)
					// Target Page (8KB) have always a width of 64 pixels
					// Half of the Target is TBW/2 pages * 8KB / (1 block * 256B) = 0x10
					half_right = true;
					dst = t;

					break;
				}

			}
		}

		if (dst == NULL && CanConvertDepth()) {
			// Let's try a trick to avoid to use wrongly a depth buffer
			// Unfortunately, I don't have any Arc the Lad testcase
			//
			// 1/ Check only current frame, I guess it is only used as a postprocessing effect
			for(list<Target*>::iterator i = m_dst[DepthStencil].begin(); i != m_dst[DepthStencil].end(); i++) {
				Target* t = *i;

				if(!t->m_age && t->m_used && t->m_dirty.empty() && GSUtil::HasSharedBits(bp, psm, t->m_TEX0.TBP0, t->m_TEX0.PSM))
				{
					dst = t;
					break;
				}
			}
		}
	}

	if(src == NULL)
	{
#ifdef ENABLE_OGL_DEBUG
		if (dst) {
			GL_CACHE("TC: dst %s hit (%s): %d (0x%x, F:0x%x)", to_string(dst->m_type), half_right ? "half" : "full",
						dst->m_texture ? dst->m_texture->GetID() : 0,
						TEX0.TBP0, TEX0.PSM);
		} else {
			GL_CACHE("TC: src miss (0x%x, F:0x%x)", TEX0.TBP0, TEX0.PSM);
		}
#endif
		src = CreateSource(TEX0, TEXA, dst, half_right);

		if(src == NULL)
		{
			return NULL;
		}

	} else {
		GL_CACHE("TC: src hit: %d (0x%x, F:0x%x)",
					src->m_texture ? src->m_texture->GetID() : 0,
					TEX0.TBP0, TEX0.PSM);
	}

	if (src->m_palette)
	{
		int size = psm.pal * sizeof(clut[0]);

		if(src->m_initpalette || !GSVector4i::update(src->m_clut, clut, size))
		{
			src->m_palette->Update(GSVector4i(0, 0, psm.pal, 1), src->m_clut, size);
			src->m_initpalette = false;
		}
	}

	src->Update(r);

	m_src.m_used = true;

	return src;
}

GSTextureCache::Target* GSTextureCache::LookupTarget(const GIFRegTEX0& TEX0, int w, int h, int type, bool used)
{
	uint32 bp = TEX0.TBP0;

	Target* dst = NULL;

	for(list<Target*>::iterator i = m_dst[type].begin(); i != m_dst[type].end(); i++)
	{
		Target* t = *i;

		if(bp == t->m_TEX0.TBP0)
		{
			m_dst[type].splice(m_dst[type].begin(), m_dst[type], i);

			dst = t;

			dst->m_32_bits_fmt |= !(TEX0.PSM & 2);
			dst->m_TEX0 = TEX0;

			break;
		}
	}

	if (dst) {
		GL_CACHE("TC: Lookup Target(%s) %dx%d, hit: %d (0x%x, F:0x%x)", to_string(type), w, h, dst->m_texture->GetID(), bp, TEX0.PSM);

		dst->Update();

		dst->m_dirty_alpha |= (TEX0.PSM != PSM_PSMCT24) && (TEX0.PSM != PSM_PSMZ24);

	} else if (CanConvertDepth()) {

		int rev_type = (type == DepthStencil) ? RenderTarget : DepthStencil;
		GSVector4 sRect(0, 0, 1.0, 1.0);
		GSVector4 dRect(0, 0, w, h);

		// Depth stencil/RT can be an older RT/DS but only check recent RT/DS to avoid to pick
		// some bad data.

		for(list<Target*>::iterator i = m_dst[rev_type].begin(); i != m_dst[rev_type].end(); i++)
		{
			Target* t = *i;

			if(!t->m_age && bp == t->m_TEX0.TBP0)
			{
				dst = CreateTarget(TEX0, w, h, type);
				dst->m_32_bits_fmt = t->m_32_bits_fmt;

				if (type == DepthStencil) {
					GL_CACHE("TC: Lookup Target(Depth) %dx%d, hit Color (0x%x, F:0x%x)", w, h, bp, TEX0.PSM);
					int shader = ShaderConvert_RGBA8_TO_FLOAT32 + GSLocalMemory::m_psm[TEX0.PSM].fmt;
					m_renderer->m_dev->StretchRect(t->m_texture, sRect, dst->m_texture, dRect, shader, false);
				} else {
					GL_CACHE("TC: Lookup Target(Color) %dx%d, hit Depth (0x%x, F:0x%x)", w, h, bp, TEX0.PSM);
					m_renderer->m_dev->StretchRect(t->m_texture, sRect, dst->m_texture, dRect, ShaderConvert_FLOAT32_TO_RGBA8, false);
				}

				break;
			}
		}
	}

	if(dst == NULL)
	{
		GL_CACHE("TC: Lookup Target(%s) %dx%d, miss (0x%x, F:0x%x)", to_string(type), w, h, bp, TEX0.PSM);

		dst = CreateTarget(TEX0, w, h, type);

		if(dst == NULL)
			return NULL;

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
		if (s_IS_OPENGL) {
			if (m_preload_frame) {
				GL_INS("Preloading the RT DATA");
				dst->m_dirty.push_back(GSDirtyRect(GSVector4i(0, 0, TEX0.TBW * 64, h), TEX0.PSM));
				dst->Update();
			} else {
#ifdef ENABLE_OGL_DEBUG
				switch (type) {
					case RenderTarget: m_renderer->m_dev->ClearRenderTarget(dst->m_texture, 0); break;
					case DepthStencil: m_renderer->m_dev->ClearDepth(dst->m_texture, 0); break;
					default:break;
				}
#endif
			}
		}
	}

	if(m_renderer->CanUpscale())
	{
		int multiplier = m_renderer->GetUpscaleMultiplier();

		if(multiplier > 1) // it's limited to a maximum of 4 on reading the config
		{
			dst->m_texture->SetScale(GSVector2((float)multiplier, (float)multiplier));
		}
		else
		{
			GSVector4i fr = m_renderer->GetFrameRect();

			int ww = (int)(fr.left + m_renderer->GetDisplayRect().width());
			int hh = (int)(fr.top + m_renderer->GetDisplayRect().height());

			if(hh <= m_renderer->GetDeviceSize().y / 2)
			{
				hh *= 2;
			}

			// Gregory: I'm sure this sillyness is related to the usage of a 32bits
			// buffer as a 16 bits format. In this case the height of the buffer is
			// multiplyed by 2 (Hence a scissor bigger than the RT)

			// This vp2 fix doesn't work most of the time

			if(hh < 512 && m_renderer->m_context->SCISSOR.SCAY1 == 511) // vp2
			{
				hh = 512;
			}

			if(ww > 0 && hh > 0)
			{
				dst->m_texture->SetScale(GSVector2((float)w / ww, (float)h / hh));
			}
		}
	}

	if(used)
	{
		dst->m_used = true;
	}

	return dst;
}

GSTextureCache::Target* GSTextureCache::LookupTarget(const GIFRegTEX0& TEX0, int w, int h, int real_h)
{
	uint32 bp = TEX0.TBP0;

	Target* dst = NULL;

	for(list<Target*>::iterator i = m_dst[RenderTarget].begin(); i != m_dst[RenderTarget].end(); i++)
	{
		Target* t = *i;

		if(bp == t->m_TEX0.TBP0)
		{
			dst = t;

			GL_CACHE("TC: Lookup Frame %dx%d, perfect hit: %d (0x%x)", w, h, dst->m_texture->GetID(), bp);

			break;
		}
		else
		{
			// HACK: try to find something close to the base pointer

			if(t->m_TEX0.TBP0 <= bp && bp < t->m_TEX0.TBP0 + 0xe00UL && (!dst || t->m_TEX0.TBP0 >= dst->m_TEX0.TBP0))
			{
				GL_CACHE("TC: Lookup Frame %dx%d, close hit: %d (0x%x, took 0x%x)", w, h, t->m_texture->GetID(), bp, t->m_TEX0.TBP0);
				dst = t;
			}
		}
	}

	if(dst == NULL)
	{
		GL_CACHE("TC: Lookup Frame %dx%d, miss (0x%x)", w, h, bp);

		dst = CreateTarget(TEX0, w, h, RenderTarget);

		if(dst == NULL)
		{
			return NULL;
		}

		m_renderer->m_dev->ClearRenderTarget(dst->m_texture, 0); // new frame buffers after reset should be cleared, don't display memory garbage

		if (m_preload_frame) {
			// Load GS data into frame. Game can directly uploads a background or the full image in
			// "CTRC" buffer. It will also avoid various black screen issue in gs dump.
			//
			// Code is more or less an equivalent of the SW renderer
			//
			// Option is hidden and not enabled by default to avoid any regression
			dst->m_dirty.push_back(GSDirtyRect(GSVector4i(0, 0, TEX0.TBW * 64, real_h), TEX0.PSM));
			dst->Update();
		}
	}
	else
	{
		dst->Update();
	}

	dst->m_used = true;

	return dst;
}

// Goal: Depth And Target at the same address is not possible. On GS it is
// the same memory but not on the Dx/GL. Therefore a write to the Depth/Target
// must invalidate the Target/Depth respectively
void GSTextureCache::InvalidateVideoMemType(int type, uint32 bp)
{
	if (!CanConvertDepth())
		return;

	for(list<Target*>::iterator i = m_dst[type].begin(); i != m_dst[type].end(); i++)
	{
		Target* t = *i;

		if(bp == t->m_TEX0.TBP0)
		{
			GL_CACHE("TC: InvalidateVideoMemType: Remove Target(%s) %d (0x%x)", to_string(type),
					t->m_texture ? t->m_texture->GetID() : 0,
					t->m_TEX0.TBP0);

			m_dst[type].erase(i);
			delete t;

			break;
		}
	}

}

// Goal: invalidate data sent to the GPU when the source (GS memory) is modified
// Called each time you want to write to the GS memory
void GSTextureCache::InvalidateVideoMem(GSOffset* off, const GSVector4i& rect, bool target)
{
	if(!off) return; // Fixme. Crashes Dual Hearts, maybe others as well. Was fine before r1549.

	uint32 bp = off->bp;
	uint32 bw = off->bw;
	uint32 psm = off->psm;

	if(!target)
	{
		// Remove Source that have same BP as the render target (color&dss)
		// rendering will dirty the copy
		const list<Source*>& m = m_src.m_map[bp >> 5];

		for(list<Source*>::const_iterator i = m.begin(); i != m.end(); )
		{
			list<Source*>::const_iterator j = i++;

			Source* s = *j;

			if(GSUtil::HasSharedBits(bp, psm, s->m_TEX0.TBP0, s->m_TEX0.PSM))
			{
				m_src.RemoveAt(s);
			}
		}

		uint32 bbp = bp + bw * 0x10;
		if (bw >= 16 && bbp < 16384) {
			// Detect half of the render target (fix snow engine game)
			// Target Page (8KB) have always a width of 64 pixels
			// Half of the Target is TBW/2 pages * 8KB / (1 block * 256B) = 0x10

			const list<Source*>& m = m_src.m_map[bbp >> 5];

			for(list<Source*>::const_iterator i = m.begin(); i != m.end(); )
			{
				list<Source*>::const_iterator j = i++;

				Source* s = *j;

				if(GSUtil::HasSharedBits(bbp, psm, s->m_TEX0.TBP0, s->m_TEX0.PSM))
				{
					m_src.RemoveAt(s);
				}
			}
		}
	}

	GSVector4i r;

	uint32* pages = (uint32*)m_temp;

	off->GetPages(rect, pages, &r);

	bool found = false;

	for(const uint32* p = pages; *p != GSOffset::EOP; p++)
	{
		uint32 page = *p;

		const list<Source*>& m = m_src.m_map[page];

		for(list<Source*>::const_iterator i = m.begin(); i != m.end(); )
		{
			list<Source*>::const_iterator j = i++;

			Source* s = *j;

			if(GSUtil::HasSharedBits(psm, s->m_TEX0.PSM))
			{
				uint32* RESTRICT valid = s->m_valid;

				bool b = bp == s->m_TEX0.TBP0;

				if(!s->m_target)
				{
					// Invalidate data of input texture
					if(s->m_repeating)
					{
						vector<GSVector2i>& l = s->m_p2t[page];

						for(vector<GSVector2i>::iterator k = l.begin(); k != l.end(); k++)
						{
							valid[k->x] &= k->y;
						}
					}
					else
					{
						valid[page] = 0;
					}

					s->m_complete = false;

					found |= b;
				}
				else
				{
					// render target used as input texture
					// TODO

					if(b)
					{
						m_src.RemoveAt(s);
					}
				}
			}
		}
	}

	if(!target) return;

	for(int type = 0; type < 2; type++)
	{
		for(list<Target*>::iterator i = m_dst[type].begin(); i != m_dst[type].end(); )
		{
			list<Target*>::iterator j = i++;

			Target* t = *j;

			// GH: (I think) this code is completely broken. Typical issue:
			// EE write an alpha channel into 32 bits texture
			// Results: the target is deleted (because HasCompatibleBits is false)
			//
			// Major issues are expected if the game try to reuse the target
			// If we dirty the RT, it will likely upload partially invalid data.
			// (The color on the previous example)
			if(GSUtil::HasSharedBits(bp, psm, t->m_TEX0.TBP0, t->m_TEX0.PSM))
			{
				if(!found && GSUtil::HasCompatibleBits(psm, t->m_TEX0.PSM))
				{
					GL_CACHE("TC: Dirty Target(%s) %d (0x%x)", to_string(type),
								t->m_texture ? t->m_texture->GetID() : 0,
								t->m_TEX0.TBP0);
					t->m_dirty.push_back(GSDirtyRect(r, psm));
					t->m_TEX0.TBW = bw;
				}
				else
				{
					m_dst[type].erase(j);
					GL_CACHE("TC: Remove Target(%s) %d (0x%x)", to_string(type),
								t->m_texture ? t->m_texture->GetID() : 0,
								t->m_TEX0.TBP0);
					delete t;
					continue;
				}
			} else if (bp == t->m_TEX0.TBP0) {
				// EE writes the ALPHA channel. Mark it as invalid for
				// the texture cache. Otherwise it will generate a wrong
				// hit on the texture cache.
				// Game: Conflict - Desert Storm (flickering)
				t->m_dirty_alpha = false;
			}

			// GH: Try to detect texture write that will overlap with a target buffer
			if(GSUtil::HasSharedBits(psm, t->m_TEX0.PSM) && bp < t->m_TEX0.TBP0)
			{
				uint32 rowsize = bw * 8192;
				uint32 offset = (uint32)((t->m_TEX0.TBP0 - bp) * 256);

				if(rowsize > 0 && offset % rowsize == 0)
				{
					int y = GSLocalMemory::m_psm[psm].pgs.y * offset / rowsize;

					if(r.bottom > y)
					{
						GL_CACHE("TC: Dirty After Target(%s) %d (0x%x)", to_string(type),
								t->m_texture ? t->m_texture->GetID() : 0,
								t->m_TEX0.TBP0);
						// TODO: do not add this rect above too
						t->m_dirty.push_back(GSDirtyRect(GSVector4i(r.left, r.top - y, r.right, r.bottom - y), psm));
						t->m_TEX0.TBW = bw;
						continue;
					}
				}
			}

			// FIXME: this code "fixes" black FMV issue with rule of rose.
			// Code is completely hardcoded so maybe not the best solution. Besides I don't
			// know the full impact of it.
			// Let's keep this code for the future
#if 0
			if(GSUtil::HasSharedBits(psm, t->m_TEX0.PSM) && (t->m_TEX0.TBP0 + 0x200 == bp))
			{
				GL_CACHE("TC: Dirty in the middle of Target(%s) %d (0x%x)", to_string(type),
						t->m_texture ? t->m_texture->GetID() : 0,
						t->m_TEX0.TBP0);

				uint32 rowsize = bw * 8192u;
				uint32 offset = 0x200 * 256u;
				int y = GSLocalMemory::m_psm[psm].pgs.y * offset / rowsize;

				t->m_dirty.push_back(GSDirtyRect(GSVector4i(r.left, r.top + y, r.right, r.bottom + y), psm));
				t->m_TEX0.TBW = bw;
				continue;
			}
#endif
		}
	}
}

// Goal: retrive the data from the GPU to the GS memory.
// Called each time you want to read from the GS memory
void GSTextureCache::InvalidateLocalMem(GSOffset* off, const GSVector4i& r)
{
	uint32 bp = off->bp;
	uint32 psm = off->psm;
	//uint32 bw = off->bw;

	// No depth handling please.
	if (psm == PSM_PSMZ32 || psm == PSM_PSMZ24 || psm == PSM_PSMZ16 || psm == PSM_PSMZ16S)
		return;

	// This is a shorter but potentially slower version of the below, commented out code.
	// It works for all the games mentioned below and fixes a couple of other ones as well
	// (Busen0: Wizardry and Chaos Legion).
	// Also in a few games the below code ran the Grandia3 case when it shouldn't :p
	for(list<Target*>::iterator i = m_dst[RenderTarget].begin(); i != m_dst[RenderTarget].end(); )
	{
		list<Target*>::iterator j = i++;

		Target* t = *j;

		if (t->m_TEX0.PSM != PSM_PSMZ32 && t->m_TEX0.PSM != PSM_PSMZ24 && t->m_TEX0.PSM != PSM_PSMZ16 && t->m_TEX0.PSM != PSM_PSMZ16S)
		{
			if(GSUtil::HasSharedBits(bp, psm, t->m_TEX0.TBP0, t->m_TEX0.PSM))
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
				if (r.x == 0 && r.y == 0) // Full screen read?
					Read(t, t->m_valid);
				else // Block level read?
					Read(t, r.rintersect(t->m_valid));
			}
		}
	}

	//GSTextureCache::Target* rt2 = NULL;
	//int ymin = INT_MAX;
	//for(list<Target*>::iterator i = m_dst[RenderTarget].begin(); i != m_dst[RenderTarget].end(); )
	//{
	//	list<Target*>::iterator j = i++;

	//	Target* t = *j;

	//	if (t->m_TEX0.PSM != PSM_PSMZ32 && t->m_TEX0.PSM != PSM_PSMZ24 && t->m_TEX0.PSM != PSM_PSMZ16 && t->m_TEX0.PSM != PSM_PSMZ16S)
	//	{
	//		if(GSUtil::HasSharedBits(bp, psm, t->m_TEX0.TBP0, t->m_TEX0.PSM))
	//		{
	//			if(GSUtil::HasCompatibleBits(psm, t->m_TEX0.PSM))
	//			{
	//				Read(t, r.rintersect(t->m_valid));
	//				return;
	//			}
	//			else if(psm == PSM_PSMCT32 && (t->m_TEX0.PSM == PSM_PSMCT16 || t->m_TEX0.PSM == PSM_PSMCT16S))
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
	//		if( (GSUtil::HasSharedBits(psm, t->m_TEX0.PSM) && (bp > t->m_TEX0.TBP0) )
	//			&& ((t->m_TEX0.TBP0 == 0) || (t->m_TEX0.TBP0==3328) || (t->m_TEX0.TBP0==3584) ))
	//		{
	//			//printf("first : %d-%d child : %d-%d\n", psm, bp, t->m_TEX0.PSM, t->m_TEX0.TBP0);
	//			uint32 rowsize = bw * 8192;
	//			uint32 offset = (uint32)((bp - t->m_TEX0.TBP0) * 256);

	//			if(rowsize > 0 && offset % rowsize == 0)
	//			{
	//				int y = GSLocalMemory::m_psm[psm].pgs.y * offset / rowsize;

	//				if(y < ymin && y < 512)
	//				{
	//					rt2 = t;
	//					ymin = y;
	//				}
	//			}
	//		}
	//	}
	//}
	//if(rt2)
	//{
	//	Read(rt2, GSVector4i(r.left, r.top + ymin, r.right, r.bottom + ymin));
	//}


	// TODO: ds
}

void GSTextureCache::IncAge()
{
	int maxage = m_src.m_used ? 3 : 30;

	// You can't use m_map[page] because Source* are duplicated on several pages.
	for(hash_set<Source*>::iterator i = m_src.m_surfaces.begin(); i != m_src.m_surfaces.end(); )
	{
		hash_set<Source*>::iterator j = i++;

		Source* s = *j;

		if(++s->m_age > maxage)
		{
			m_src.RemoveAt(s);
		}
	}

	m_src.m_used = false;

	// Clearing of Rendertargets causes flickering in many scene transitions.
	// Sigh, this seems to be used to invalidate surfaces. So set a huge maxage to avoid flicker,
	// but still invalidate surfaces. (Disgaea 2 fmv when booting the game through the BIOS)
	// Original maxage was 4 here, Xenosaga 2 needs at least 240, else it flickers on scene transitions.
	maxage = 400; // ffx intro scene changes leave the old image untouched for a couple of frames and only then start using it

	for(int type = 0; type < 2; type++)
	{
		for(list<Target*>::iterator i = m_dst[type].begin(); i != m_dst[type].end(); )
		{
			list<Target*>::iterator j = i++;

			Target* t = *j;

			// This variable is used to detect the texture shuffle effect. There is a high
			// probability that game will do it on the current RT.
			// Variable is cleared here to avoid issue with game that uses a 16 bits
			// render target
			if (t->m_age > 0) {
				// GoW2 uses the effect at the start of the frame
				t->m_32_bits_fmt = false;
			}

			if(++t->m_age > maxage)
			{
				m_dst[type].erase(j);
				GL_CACHE("TC: Remove Target(%s): %d (0x%x) due to age", to_string(type),
							t->m_texture ? t->m_texture->GetID() : 0,
							t->m_TEX0.TBP0);

				delete t;
			}
		}
	}
}

//Fixme: Several issues in here. Not handling depth stencil, pitch conversion doesnt work.
GSTextureCache::Source* GSTextureCache::CreateSource(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, Target* dst, bool half_right)
{
	const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[TEX0.PSM];
	Source* src = new Source(m_renderer, TEX0, TEXA, m_temp);

	int tw = 1 << TEX0.TW;
	int th = 1 << TEX0.TH;
	//int tp = TEX0.TBW << 6;

	bool hack = false;

	if(m_spritehack && (TEX0.PSM == PSM_PSMT8 || TEX0.PSM == PSM_PSMT8H))
	{
		src->m_spritehack_t = true;

		if(m_spritehack == 2 && TEX0.CPSM != PSM_PSMCT16)
			src->m_spritehack_t = false;
	}
	else
		src->m_spritehack_t = false;

	if (dst)
	{
		// TODO: clean up this mess

		int shader = dst->m_type != RenderTarget ? ShaderConvert_FLOAT32_TO_RGBA8 : ShaderConvert_COPY;
		bool is_8bits = TEX0.PSM == PSM_PSMT8 && s_IS_OPENGL;

		if (is_8bits) {
			GL_INS("Reading RT as a packed-indexed 8 bits format");
			shader = ShaderConvert_RGBA_TO_8I;
		}

#ifdef ENABLE_OGL_DEBUG
		if (TEX0.PSM == PSM_PSMT4) {
			GL_INS("ERROR: Reading RT as a packed-indexed 4 bits format is not supported");
		}
#endif

		if (TEX0.PSM < PSM_PSMT8 || TEX0.PSM > PSM_PSMT4HH) {
			src->m_32_bits_fmt = dst->m_32_bits_fmt;
		}
		src->m_target = true;

		dst->Update();

		GSTexture* tmp = NULL;

		if (dst->m_texture->IsMSAA())
		{
			tmp = dst->m_texture;

			dst->m_texture = m_renderer->m_dev->Resolve(dst->m_texture);
		}


		// do not round here!!! if edge becomes a black pixel and addressing mode is clamp => everything outside the clamped area turns into black (kh2 shadows)

		int w = (int)(dst->m_texture->GetScale().x * tw);
		int h = (int)(dst->m_texture->GetScale().y * th);
		if (is_8bits) {
			// Unscale 8 bits textures, quality won't be nice but format is really awful
			w = tw;
			h = th;
		}

		GSVector2i dstsize = dst->m_texture->GetSize();

		// pitch conversion

		if(dst->m_TEX0.TBW != TEX0.TBW) // && dst->m_TEX0.PSM == TEX0.PSM
		{
			// This is so broken :p
			////Better not do the code below, "fixes" like every game that ever gets here..
			////Edit: Ratchet and Clank needs this to show most of it's graphics at all.
			////Someone else fix this please, I can't :p
			////delete src; return NULL;

			//// sfex3 uses this trick (bw: 10 -> 5, wraps the right side below the left)

			//ASSERT(dst->m_TEX0.TBW > TEX0.TBW); // otherwise scale.x need to be reduced to make the larger texture fit (TODO)

			//src->m_texture = m_renderer->m_dev->CreateRenderTarget(dstsize.x, dstsize.y, false);

			//GSVector4 size = GSVector4(dstsize).xyxy();
			//GSVector4 scale = GSVector4(dst->m_texture->GetScale()).xyxy();

			//int blockWidth  = 64;
			//int blockHeight = TEX0.PSM == PSM_PSMCT32 || TEX0.PSM == PSM_PSMCT24 ? 32 : 64;

			//GSVector4i br(0, 0, blockWidth, blockHeight);

			//int sw = (int)dst->m_TEX0.TBW << 6;

			//int dw = (int)TEX0.TBW << 6;
			//int dh = 1 << TEX0.TH;

			//if(sw != 0)
			//for(int dy = 0; dy < dh; dy += blockHeight)
			//{
			//	for(int dx = 0; dx < dw; dx += blockWidth)
			//	{
			//		int off = dy * dw / blockHeight + dx;

			//		int sx = off % sw;
			//		int sy = off / sw;

			//		GSVector4 sRect = GSVector4(GSVector4i(sx, sy).xyxy() + br) * scale / size;
			//		GSVector4 dRect = GSVector4(GSVector4i(dx, dy).xyxy() + br) * scale;

			//		m_renderer->m_dev->StretchRect(dst->m_texture, sRect, src->m_texture, dRect);

			//		// TODO: this is quite a lot of StretchRect, do it with one Draw
			//	}
			//}
		}
		else if(tw < 1024)
		{
			// FIXME: timesplitters blurs the render target by blending itself over a couple of times
			hack = true;
			//if(tw == 256 && th == 128 && (TEX0.TBP0 == 0 || TEX0.TBP0 == 0x00e00))
			//{
			//	delete src;
			//	return NULL;
			//}
		}
		// width/height conversion

		GSVector2 scale = dst->m_texture->GetScale();

		GSVector4 dRect(0, 0, w, h);

		// Lengthy explanation of the rescaling code.
		// Here an example in 2x:
		// RT is 1280x1024 but only contains 512x448 valid data (so 256x224 pixels without upscaling)
		//
		// PS2 want to read it back as a 1024x1024 pixels (they don't care about the extra pixels)
		// So in theory we need to shrink a 2048x2048 RT into a 1024x1024 texture. Obviously the RT is
		// too small.
		//
		// So we will only limit the resize to the available data in RT.
		// Therefore we will resize the RT from 1280x1024 to 1280x1024/2048x2048 % of the new texture
		// size (which is 1280x1024) (i.e. 800x512)
		// From the rendering point of view. UV coordinate will be normalized on the real GS texture size
		// This way it can be used on an upscaled texture without extra scaling factor (only requirement is
		// to have same proportion)
		//
		// FIXME: The scaling will create a bad offset. For example if texture coordinate start at 0.5 (pixel 0)
		// At 2x it will become 0.5/128 * 256 = 1 (pixel 1)
		// I think it is the purpose of the UserHacks_HalfPixelOffset below. However implementation is less
		// than ideal.
		// 1/ It suppose games have an half pixel offset on texture coordinate which could be wrong
		// 2/ It doesn't support rescaling of the RT (tw = 1024)
		// Maybe it will be more easy to just round the UV value in the Vertex Shader

		if (!is_8bits) {
			// 8 bits handling is special due to unscaling. It is better to not execute this code
			if (w > dstsize.x)
			{
				scale.x = (float)dstsize.x / tw;
				dRect.z = (float)dstsize.x * scale.x / dst->m_texture->GetScale().x;
				w = dstsize.x;
			}

			if (h > dstsize.y)
			{
				scale.y = (float)dstsize.y / th;
				dRect.w = (float)dstsize.y * scale.y / dst->m_texture->GetScale().y;
				h = dstsize.y;
			}
		}

		GSVector4 sRect(0, 0, w, h);

		GSTexture* sTex = src->m_texture ? src->m_texture : dst->m_texture;
		GSTexture* dTex = m_renderer->m_dev->CreateRenderTarget(w, h, false);

		// GH: by default (m_paltex == 0) GSdx converts texture to the 32 bit format
		// However it is different here. We want to reuse a Render Target as a texture.
		// Because the texture is already on the GPU, CPU can't convert it.
		if (psm.pal > 0) {
			src->m_palette = m_renderer->m_dev->CreateTexture(256, 1);
		}
		// Disable linear filtering for various GS post-processing effect
		// 1/ Palette is used to interpret the alpha channel of the RT as an index.
		// Star Ocean 3 uses it to emulate a stencil buffer.
		// 2/ Z formats are a bad idea to interpolate (discontinuties).
		// 3/ 16 bits buffer is used to move data from a channel to another.
		//
		// I keep linear filtering for standard color even if I'm not sure that it is
		// working correctly.
		// Indeed, texture is reduced so you need to read all covered pixels (9 in 3x)
		// to correctly interpolate the value. Linear interpolation is likely acceptable
		// only in 2x scaling
		//
		// Src texture will still be bilinear interpolated so I'm really not sure
		// that we need to do it here too.
		//
		// Future note: instead to do
		// RT 2048x2048 -> T 1024x1024 -> RT 2048x2048
		// We can maybe sample directly a bigger texture
		// RT 2048x2048 -> T 2048x2048 -> RT 2048x2048
		// Pro: better quality. Copy instead of StretchRect (must be faster)
		// Cons: consume more memory
		//
		// In distant future: investigate to reuse the RT directly without any
		// copy. Likely a speed boost and memory usage reduction.
		bool linear = (TEX0.PSM == PSM_PSMCT32 || TEX0.PSM == PSM_PSMCT24);

		if(!src->m_texture)
		{
			src->m_texture = dTex;
		}

		if ((sRect == dRect).alltrue() && !shader)
		{
			if (half_right) {
				// You typically hit this code in snow engine game. Dstsize is the size of of Dx/GL RT
				// which is arbitrary set to 1280 (biggest RT used by GS). h/w are based on the input texture
				// so the only reliable way to find the real size of the target is to use the TBW value.
				float real_width = dst->m_TEX0.TBW * 64u * dst->m_texture->GetScale().x;
				m_renderer->m_dev->CopyRect(sTex, dTex, GSVector4i(real_width/2.0f, 0, real_width, h));
			} else {
				m_renderer->m_dev->CopyRect(sTex, dTex, GSVector4i(0, 0, w, h)); // <= likely wrong dstsize.x could be bigger than w
			}
		}
		else
		{
			// Different size or not the same format
			sRect.z /= sTex->GetWidth();
			sRect.w /= sTex->GetHeight();

			if (half_right) {
				sRect.x = sRect.z/2.0f;
			}

			m_renderer->m_dev->StretchRect(sTex, sRect, dTex, dRect, shader, linear);
		}

		if(dTex != src->m_texture)
		{
			m_renderer->m_dev->Recycle(src->m_texture);

			src->m_texture = dTex;
		}

		if( src->m_texture )
			src->m_texture->SetScale(scale);
		else
			ASSERT(0);

		if(tmp != NULL)
		{
			// tmp is the texture before a MultiSample resolve
			m_renderer->m_dev->Recycle(dst->m_texture);

			dst->m_texture = tmp;
		}

		// Offset hack. Can be enabled via GSdx options.
		// The offset will be used in Draw().

		float modx = 0.0f;
		float mody = 0.0f;

		if(UserHacks_HalfPixelOffset && hack)
		{
			switch(m_renderer->GetUpscaleMultiplier())
			{
			case 2:  modx = 2.2f; mody = 2.2f; dst->m_texture->LikelyOffset = true;  break;
			case 3:  modx = 3.1f; mody = 3.1f; dst->m_texture->LikelyOffset = true;  break;
			case 4:  modx = 4.2f; mody = 4.2f; dst->m_texture->LikelyOffset = true;  break;
			case 5:  modx = 5.3f; mody = 5.3f; dst->m_texture->LikelyOffset = true;  break;
			case 6:  modx = 6.2f; mody = 6.2f; dst->m_texture->LikelyOffset = true;  break;
			case 8:  modx = 8.2f; mody = 8.2f; dst->m_texture->LikelyOffset = true;  break;
			default: modx = 0.0f; mody = 0.0f; dst->m_texture->LikelyOffset = false; break;
			}
		}

		dst->m_texture->OffsetHack_modx = modx;
		dst->m_texture->OffsetHack_mody = mody;
	}
	else
	{
		if (m_paltex && psm.pal > 0)
		{
			src->m_texture = m_renderer->m_dev->CreateTexture(tw, th, Get8bitFormat());
			src->m_palette = m_renderer->m_dev->CreateTexture(256, 1);
		}
		else
			src->m_texture = m_renderer->m_dev->CreateTexture(tw, th);
	}

	if(src->m_texture == NULL)
	{
		ASSERT(0);
		delete src;
		return NULL;
	}

	if(psm.pal > 0)
	{
		memcpy(src->m_clut, (const uint32*)m_renderer->m_mem.m_clut, psm.pal * sizeof(uint32));
	}

	m_src.Add(src, TEX0, m_renderer->m_context->offset.tex);

	return src;
}

GSTextureCache::Target* GSTextureCache::CreateTarget(const GIFRegTEX0& TEX0, int w, int h, int type)
{
	Target* t = new Target(m_renderer, TEX0, m_temp, CanConvertDepth());

	// FIXME: initial data should be unswizzled from local mem in Update() if dirty

	t->m_type = type;

	if(type == RenderTarget)
	{
		t->m_texture = m_renderer->m_dev->CreateRenderTarget(w, h, true);

		t->m_used = true; // FIXME
	}
	else if(type == DepthStencil)
	{
		t->m_texture = m_renderer->m_dev->CreateDepthStencil(w, h, true);
	}

	if(t->m_texture == NULL)
	{
		ASSERT(0);
		delete t;
		return NULL;
	}

	m_dst[type].push_front(t);

	return t;
}

void GSTextureCache::PrintMemoryUsage()
{
#ifdef ENABLE_OGL_DEBUG
	uint32 tex    = 0;
	uint32 tex_rt = 0;
	uint32 rt     = 0;
	uint32 dss    = 0;
	for(hash_set<Source*>::iterator i = m_src.m_surfaces.begin(); i != m_src.m_surfaces.end(); i++) {
		Source* s = *i;
		if (s) {
			if (s->m_target)
				tex_rt += s->m_texture->GetMemUsage();
			else
				tex    += s->m_texture->GetMemUsage();

		}
	}
	for(list<Target*>::iterator i = m_dst[RenderTarget].begin(); i != m_dst[RenderTarget].end(); i++) {
		Target* t = *i;
		if (t)
			rt += t->m_texture->GetMemUsage();
	}
	for(list<Target*>::iterator i = m_dst[DepthStencil].begin(); i != m_dst[DepthStencil].end(); i++) {
		Target* t = *i;
		if (t)
			dss += t->m_texture->GetMemUsage();
	}

	GL_PERF("MEM: RO Tex %dMB. RW Tex %dMB. Target %dMB. Depth %dMB", tex >> 20u, tex_rt >> 20u, rt >> 20u, dss >> 20u);
#endif
}

// GSTextureCache::Surface

GSTextureCache::Surface::Surface(GSRenderer* r, uint8* temp)
	: m_renderer(r)
	, m_texture(NULL)
	, m_age(0)
	, m_temp(temp)
	, m_32_bits_fmt(false)
{
	m_TEX0.TBP0 = 0x3fff;
}

GSTextureCache::Surface::~Surface()
{
	m_renderer->m_dev->Recycle(m_texture);
}

void GSTextureCache::Surface::Update()
{
	m_age = 0;
}

// GSTextureCache::Source

GSTextureCache::Source::Source(GSRenderer* r, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, uint8* temp)
	: Surface(r, temp)
	, m_palette(NULL)
	, m_initpalette(true)
	, m_target(false)
	, m_complete(false)
	, m_spritehack_t(false)
	, m_p2t(NULL)
{
	m_TEX0 = TEX0;
	m_TEXA = TEXA;

	memset(m_valid, 0, sizeof(m_valid));

	m_clut = (uint32*)_aligned_malloc(256 * sizeof(uint32), 32);

	memset(m_clut, 0, 256*sizeof(uint32));

	m_write.rect = (GSVector4i*)_aligned_malloc(3 * sizeof(GSVector4i), 32);
	m_write.count = 0;

	m_repeating = m_TEX0.IsRepeating();

	if(m_repeating)
	{
		m_p2t = r->m_mem.GetPage2TileMap(m_TEX0);
	}
}

GSTextureCache::Source::~Source()
{
	m_renderer->m_dev->Recycle(m_palette);

	_aligned_free(m_clut);

	_aligned_free(m_write.rect);
}

void GSTextureCache::Source::Update(const GSVector4i& rect)
{
	Surface::Update();

	if(m_complete || m_target)
	{
		return;
	}

	GSVector2i bs = GSLocalMemory::m_psm[m_TEX0.PSM].bs;

	int tw = std::max<int>(1 << m_TEX0.TW, bs.x);
	int th = std::max<int>(1 << m_TEX0.TH, bs.y);

	GSVector4i r = rect.ralign<Align_Outside>(bs);

	if(r.eq(GSVector4i(0, 0, tw, th)))
	{
		m_complete = true; // lame, but better than nothing
	}

	const GSOffset* off = m_renderer->m_context->offset.tex;

	uint32 blocks = 0;

	if(m_repeating)
	{
		for(int y = r.top; y < r.bottom; y += bs.y)
		{
			uint32 base = off->block.row[y >> 3];

			for(int x = r.left, i = (y << 7) + x; x < r.right; x += bs.x, i += bs.x)
			{
				uint32 block = base + off->block.col[x >> 3];

				if(block < MAX_BLOCKS)
				{
					uint32 addr = i >> 3;

					uint32 row = addr >> 5;
					uint32 col = 1 << (addr & 31);

					if((m_valid[row] & col) == 0)
					{
						m_valid[row] |= col;

						Write(GSVector4i(x, y, x + bs.x, y + bs.y));

						blocks++;
					}
				}
			}
		}
	}
	else
	{
		for(int y = r.top; y < r.bottom; y += bs.y)
		{
			uint32 base = off->block.row[y >> 3];

			for(int x = r.left; x < r.right; x += bs.x)
			{
				uint32 block = base + off->block.col[x >> 3];

				if(block < MAX_BLOCKS)
				{
					uint32 row = block >> 5;
					uint32 col = 1 << (block & 31);

					if((m_valid[row] & col) == 0)
					{
						m_valid[row] |= col;

						Write(GSVector4i(x, y, x + bs.x, y + bs.y));

						blocks++;
					}
				}
			}
		}
	}

	if(blocks > 0)
	{
		m_renderer->m_perfmon.Put(GSPerfMon::Unswizzle, bs.x * bs.y * blocks << (m_palette ? 2 : 0));

		Flush(m_write.count);
	}
}

void GSTextureCache::Source::Write(const GSVector4i& r)
{
	m_write.rect[m_write.count++] = r;

	while(m_write.count >= 2)
	{
		GSVector4i& a = m_write.rect[m_write.count - 2];
		GSVector4i& b = m_write.rect[m_write.count - 1];

		if((a == b.zyxw()).mask() == 0xfff0)
		{
			a.right = b.right; // extend right

			m_write.count--;
		}
		else if((a == b.xwzy()).mask() == 0xff0f)
		{
			a.bottom = b.bottom; // extend down

			m_write.count--;
		}
		else
		{
			break;
		}
	}

	if(m_write.count > 2)
	{
		Flush(1);
	}
}

void GSTextureCache::Source::Flush(uint32 count)
{
	// This function as written will not work for paletted formats copied from framebuffers
	// because they are 8 or 4 bit formats on the GS and the GS local memory module reads
	// these into an 8 bit format while the D3D surfaces are 32 bit.
	// However the function is never called for these cases.  This is just for information
	// should someone wish to use this function for these cases later.
	const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[m_TEX0.PSM];

	int tw = 1 << m_TEX0.TW;
	int th = 1 << m_TEX0.TH;

	GSVector4i tr(0, 0, tw, th);

	int pitch = max(tw, psm.bs.x) * sizeof(uint32);

	GSLocalMemory& mem = m_renderer->m_mem;

	const GSOffset* off = m_renderer->m_context->offset.tex;

	GSLocalMemory::readTexture rtx = psm.rtx;

	GIFRegTEXA plainTEXA;

	// Until DX is fixed
	if (s_IS_OPENGL) {
		plainTEXA = m_TEXA;
	} else {
		plainTEXA.AEM = 1;
		plainTEXA.TA0 = 0;
		plainTEXA.TA1 = 0x80;
	}

	if(m_palette)
	{
		pitch >>= 2;
		rtx = psm.rtxP;
	}

	uint8* buff = m_temp;

	for(uint32 i = 0; i < count; i++)
	{
		GSVector4i r = m_write.rect[i];

		if((r > tr).mask() & 0xff00)
		{
			(mem.*rtx)(off, r, buff, pitch, m_TEXA);

			m_texture->Update(r.rintersect(tr), buff, pitch);
		}
		else
		{
			GSTexture::GSMap m;

			if(m_texture->Map(m, &r))
			{
				(mem.*rtx)(off, r, m.bits, m.pitch, plainTEXA);

				m_texture->Unmap();
			}
			else
			{
				(mem.*rtx)(off, r, buff, pitch, plainTEXA);

				m_texture->Update(r, buff, pitch);
			}
		}
	}

	if(count < m_write.count)
	{
		// Warning src and destination overlap. Memmove must be used instead of memcpy
		memmove(&m_write.rect[0], &m_write.rect[count], (m_write.count - count) * sizeof(m_write.rect[0]));
	}

	m_write.count -= count;
}

// GSTextureCache::Target

GSTextureCache::Target::Target(GSRenderer* r, const GIFRegTEX0& TEX0, uint8* temp, bool depth_supported)
	: Surface(r, temp)
	, m_type(-1)
	, m_used(false)
	, m_depth_supported(depth_supported)
{
	m_TEX0 = TEX0;
	m_32_bits_fmt |= !(TEX0.PSM & 2);
	m_dirty_alpha = (TEX0.PSM != PSM_PSMCT24) && (TEX0.PSM != PSM_PSMZ24);

	m_valid = GSVector4i::zero();
}

void GSTextureCache::Target::Update()
{
	Surface::Update();

	// FIXME: the union of the rects may also update wrong parts of the render target (but a lot faster :)
	// GH: it must be doable
	// 1/ rescale the new t to the good size
	// 2/ copy each rectangle (rescale the rectangle) (use CopyRect or multiple vertex)
	// Alternate
	// 1/ uses multiple vertex rectangle

	GSVector4i r = m_dirty.GetDirtyRectAndClear(m_TEX0, m_texture->GetSize());

	if (r.rempty()) return;

	// No handling please
	if ((m_type == DepthStencil) && !m_depth_supported) {
		// do the most likely thing a direct write would do, clear it
		GL_INS("ERROR: Update DepthStencil dummy");

		if((m_renderer->m_game.flags & CRC::ZWriteMustNotClear) == 0)
			m_renderer->m_dev->ClearDepth(m_texture, 0);

		return;
	}

	int w = r.width();
	int h = r.height();

	GIFRegTEXA TEXA;

	TEXA.AEM = 1;
	TEXA.TA0 = 0;
	TEXA.TA1 = 0x80;

	GSTexture* t = m_renderer->m_dev->CreateTexture(w, h);
	if (t == NULL) return;

	const GSOffset* off = m_renderer->m_mem.GetOffset(m_TEX0.TBP0, m_TEX0.TBW, m_TEX0.PSM);

	GSTexture::GSMap m;

	if(t->Map(m))
	{
		m_renderer->m_mem.ReadTexture(off, r, m.bits,  m.pitch, TEXA);

		t->Unmap();
	}
	else
	{
		int pitch = ((w + 3) & ~3) * 4;

		m_renderer->m_mem.ReadTexture(off, r, m_temp, pitch, TEXA);

		t->Update(r.rsize(), m_temp, pitch);
	}

	// m_renderer->m_perfmon.Put(GSPerfMon::Unswizzle, w * h * 4);

	// Copy the new GS memory content into the destination texture.
	if(m_type == RenderTarget)
	{
		GL_INS("ERROR: Update RenderTarget");

		m_renderer->m_dev->StretchRect(t, m_texture, GSVector4(r) * GSVector4(m_texture->GetScale()).xyxy());
	}
	else if(m_type == DepthStencil)
	{
		GL_INS("ERROR: Update DepthStencil");

		// FIXME linear or not?
		m_renderer->m_dev->StretchRect(t, m_texture, GSVector4(r) * GSVector4(m_texture->GetScale()).xyxy(), ShaderConvert_RGBA8_TO_FLOAT32);
	}

	m_renderer->m_dev->Recycle(t);
}

// GSTextureCache::SourceMap

void GSTextureCache::SourceMap::Add(Source* s, const GIFRegTEX0& TEX0, const GSOffset* off)
{
	m_surfaces.insert(s);

	if(s->m_target)
	{
		// TODO

		// GH: I don't know why but it seems we only consider the first page for a render target

		m_map[TEX0.TBP0 >> 5].push_front(s);

		return;
	}

	// Remaining code will compute a list of pages that are dirty (in a similar fashion as GSOffset::GetPages)
	// (Maybe GetPages could be used instead, perf opt?)
	// The source pointer will be stored/duplicated in all m_map[array of pages]
	const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[TEX0.PSM];

	GSVector2i bs = (TEX0.TBP0 & 31) == 0 ? psm.pgs : psm.bs;

	int tw = 1 << TEX0.TW;
	int th = 1 << TEX0.TH;

	for(int y = 0; y < th; y += bs.y)
	{
		uint32 base = off->block.row[y >> 3];

		for(int x = 0; x < tw; x += bs.x)
		{
			uint32 page = (base + off->block.col[x >> 3]) >> 5;

			if(page < MAX_PAGES)
			{
				m_pages[page >> 5] |= 1 << (page & 31);
			}
		}
	}

	for(size_t i = 0; i < countof(m_pages); i++)
	{
		if(uint32 p = m_pages[i])
		{
			m_pages[i] = 0;

			list<Source*>* m = &m_map[i << 5];

			unsigned long j;

			while(_BitScanForward(&j, p))
			{
				p ^= 1 << j;

				m[j].push_front(s);
			}
		}
	}
}

void GSTextureCache::SourceMap::RemoveAll()
{
	for_each(m_surfaces.begin(), m_surfaces.end(), delete_object());

	m_surfaces.clear();

	for(size_t i = 0; i < countof(m_map); i++)
	{
		m_map[i].clear();
	}
}

void GSTextureCache::SourceMap::RemoveAt(Source* s)
{
	m_surfaces.erase(s);

	GL_CACHE("TC: Remove Src Texture: %d (0x%x)",
				s->m_texture ? s->m_texture->GetID() : 0,
				s->m_TEX0.TBP0);

	// Source (except render target) is duplicated for each page they use.
	for(size_t start = s->m_TEX0.TBP0 >> 5, end = s->m_target ? start : countof(m_map) - 1; start <= end; start++)
	{
		list<Source*>& m = m_map[start];

		for(list<Source*>::iterator i = m.begin(); i != m.end(); )
		{
			list<Source*>::iterator j = i++;

			if(*j == s) {m.erase(j); break;}
		}
	}

	delete s;
}
