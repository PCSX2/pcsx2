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

#pragma once

#include "GS/Renderers/Common/GSRenderer.h"
#include "GS/Renderers/Common/GSFastList.h"
#include "GS/Renderers/Common/GSDirtyRect.h"
#include <unordered_set>

class GSTextureCache
{
public:
	enum
	{
		RenderTarget,
		DepthStencil
	};

	class Surface : public GSAlignedClass<32>
	{
	protected:
		GSRenderer* m_renderer;

	public:
		GSTexture* m_texture;
		GIFRegTEX0 m_TEX0;
		GIFRegTEXA m_TEXA;
		int m_age;
		u8* m_temp;
		bool m_32_bits_fmt; // Allow to detect the casting of 32 bits as 16 bits texture
		bool m_shared_texture;
		u32 m_end_block; // Hint of the surface area.

	public:
		Surface(GSRenderer* r, u8* temp);
		virtual ~Surface();

		void UpdateAge();
		bool Inside(u32 bp, u32 bw, u32 psm, const GSVector4i& rect);
		bool Overlaps(u32 bp, u32 bw, u32 psm, const GSVector4i& rect);
	};

	struct PaletteKey
	{
		const u32* clut;
		u16 pal;
	};

	class Palette
	{
	private:
		u32* m_clut;
		u16 m_pal;
		GSTexture* m_tex_palette;
		const GSRenderer* m_renderer;

	public:
		Palette(const GSRenderer* renderer, u16 pal, bool need_gs_texture);
		~Palette();

		// Disable copy constructor and copy operator
		Palette(const Palette&) = delete;
		Palette& operator=(const Palette&) = delete;

		// Disable move constructor and move operator
		Palette(const Palette&&) = delete;
		Palette& operator=(const Palette&&) = delete;

		GSTexture* GetPaletteGSTexture();

		PaletteKey GetPaletteKey();

		void InitializeTexture();
	};

	struct PaletteKeyHash
	{
		// Calculate hash
		std::size_t operator()(const PaletteKey& key) const;
	};

	struct PaletteKeyEqual
	{
		// Compare pal value and clut contents
		bool operator()(const PaletteKey& lhs, const PaletteKey& rhs) const;
	};

	class Source : public Surface
	{
		struct
		{
			GSVector4i* rect;
			u32 count;
		} m_write;

		void Write(const GSVector4i& r, int layer);
		void Flush(u32 count, int layer);

	public:
		std::shared_ptr<Palette> m_palette_obj;
		GSTexture* m_palette;
		u32 m_valid[MAX_PAGES]; // each u32 bits map to the 32 blocks of that page
		GSVector4i m_valid_rect;
		bool m_target;
		bool m_complete;
		bool m_repeating;
		std::vector<GSVector2i>* m_p2t;
		// Keep a trace of the target origin. There is no guarantee that pointer will
		// still be valid on future. However it ought to be good when the source is created
		// so it can be used to access un-converted data for the current draw call.
		GSTexture* m_from_target;
		GIFRegTEX0 m_from_target_TEX0; // TEX0 of the target texture, if any, else equal to texture TEX0
		GIFRegTEX0 m_layer_TEX0[7]; // Detect already loaded value
		// Keep a GSTextureCache::SourceMap::m_map iterator to allow fast erase
		std::array<u16, MAX_PAGES> m_erase_it;
		GSOffset::PageLooper m_pages;

	public:
		Source(GSRenderer* r, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, u8* temp, bool dummy_container = false);
		virtual ~Source();

		void Update(const GSVector4i& rect, int layer = 0);
		void UpdateLayer(const GIFRegTEX0& TEX0, const GSVector4i& rect, int layer = 0);

		bool ClutMatch(PaletteKey palette_key);
	};

	class Target : public Surface
	{
	public:
		int m_type;
		bool m_used;
		GSDirtyRectList m_dirty;
		GSVector4i m_valid;
		bool m_depth_supported;
		bool m_dirty_alpha;

	public:
		Target(GSRenderer* r, const GIFRegTEX0& TEX0, u8* temp, bool depth_supported);

		void UpdateValidity(const GSVector4i& rect);

		void Update();
	};

	class PaletteMap
	{
	private:
		static const u16 MAX_SIZE = 65535; // Max size of each map.
		const GSRenderer* m_renderer;

		// Array of 2 maps, the first for 64B palettes and the second for 1024B palettes.
		// Each map stores the key PaletteKey (clut copy, pal value) pointing to the relevant shared pointer to Palette object.
		// There is one PaletteKey per Palette, and the hashing and comparison of PaletteKey is done with custom operators PaletteKeyHash and PaletteKeyEqual.
		std::array<std::unordered_map<PaletteKey, std::shared_ptr<Palette>, PaletteKeyHash, PaletteKeyEqual>, 2> m_maps;

	public:
		PaletteMap(const GSRenderer* renderer);

		// Retrieves a shared pointer to a valid Palette from m_maps or creates a new one adding it to the data structure
		std::shared_ptr<Palette> LookupPalette(u16 pal, bool need_gs_texture);

		void Clear(); // Clears m_maps, thus deletes Palette objects
	};

	class SourceMap
	{
	public:
		std::unordered_set<Source*> m_surfaces;
		std::array<FastList<Source*>, MAX_PAGES> m_map;
		u32 m_pages[16]; // bitmap of all pages
		bool m_used;

		SourceMap()
			: m_used(false)
		{
			memset(m_pages, 0, sizeof(m_pages));
		}

		void Add(Source* s, const GIFRegTEX0& TEX0, const GSOffset& off);
		void RemoveAll();
		void RemovePartial();
		void RemoveAt(Source* s);
	};

	struct TexInsideRtCacheEntry
	{
		u32 psm;
		u32 bp;
		u32 bp_end;
		u32 bw;
		u32 t_tex0_tbp0;
		u32 m_end_block;
		bool has_valid_offset;
		int x_offset;
		int y_offset;
	};

protected:
	GSRenderer* m_renderer;
	PaletteMap m_palette_map;
	SourceMap m_src;
	FastList<Target*> m_dst[2];
	bool m_paltex;
	bool m_preload_frame;
	u8* m_temp;
	bool m_can_convert_depth;
	bool m_cpu_fb_conversion;
	CRCHackLevel m_crc_hack_level;
	static bool m_disable_partial_invalidation;
	bool m_texture_inside_rt;
	static bool m_wrap_gs_mem;
	u8 m_texture_inside_rt_cache_size = 255;
	std::vector<TexInsideRtCacheEntry> m_texture_inside_rt_cache;

	Source* CreateSource(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, Target* t = NULL, bool half_right = false, int x_offset = 0, int y_offset = 0);
	Target* CreateTarget(const GIFRegTEX0& TEX0, int w, int h, int type);

	// TODO: virtual void Write(Source* s, const GSVector4i& r) = 0;
	// TODO: virtual void Write(Target* t, const GSVector4i& r) = 0;

public:
	GSTextureCache(GSRenderer* r);
	~GSTextureCache();
	void Read(Target* t, const GSVector4i& r);
	void Read(Source* t, const GSVector4i& r);
	void RemoveAll();
	void RemovePartial();

	Source* LookupSource(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, const GSVector4i& r);
	Source* LookupDepthSource(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, const GSVector4i& r, bool palette = false);

	Target* LookupTarget(const GIFRegTEX0& TEX0, int w, int h, int type, bool used, u32 fbmask = 0);
	Target* LookupTarget(const GIFRegTEX0& TEX0, int w, int h, int real_h);

	void InvalidateVideoMemType(int type, u32 bp);
	void InvalidateVideoMemSubTarget(GSTextureCache::Target* rt);
	void InvalidateVideoMem(const GSOffset& off, const GSVector4i& r, bool target = true);
	void InvalidateLocalMem(const GSOffset& off, const GSVector4i& r);

	void IncAge();
	bool UserHacks_HalfPixelOffset;
	void ScaleTexture(GSTexture* texture);

	bool ShallSearchTextureInsideRt();

	const char* to_string(int type)
	{
		return (type == DepthStencil) ? "Depth" : "Color";
	}

	void PrintMemoryUsage();

	void AttachPaletteToSource(Source* s, u16 pal, bool need_gs_texture);
};
