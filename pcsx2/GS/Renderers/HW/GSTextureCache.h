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

#include <limits>

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

	constexpr static u32 MAX_BP = 0x3fff;

	constexpr static bool CheckOverlap(const u32 a_bp, const u32 a_bp_end, const u32 b_bp, const u32 b_bp_end) noexcept
	{
		const bool valid = a_bp <= a_bp_end && b_bp <= b_bp_end;
		const bool overlap = a_bp <= b_bp_end && a_bp_end >= b_bp;
		return valid && overlap;
	}

	using HashType = u64;

	struct HashCacheKey
	{
		HashType TEX0Hash, CLUTHash;
		GIFRegTEX0 TEX0;
		GIFRegTEXA TEXA;

		HashCacheKey();

		static HashCacheKey Create(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, const u32* clut, const GSVector2i* lod);

		HashCacheKey WithRemovedCLUTHash() const;
		void RemoveCLUTHash();

		__fi bool operator==(const HashCacheKey& e) const { return std::memcmp(this, &e, sizeof(*this)) == 0; }
		__fi bool operator!=(const HashCacheKey& e) const { return std::memcmp(this, &e, sizeof(*this)) != 0; }
		__fi bool operator<(const HashCacheKey& e) const { return std::memcmp(this, &e, sizeof(*this)) < 0; }
	};

	struct HashCacheKeyHash
	{
		u64 operator()(const HashCacheKey& key) const;
	};

	struct HashCacheEntry
	{
		GSTexture* texture;
		u32 refcount;
		u16 age;
		bool is_replacement;
	};

	class Surface : public GSAlignedClass<32>
	{
	public:
		GSTexture* m_texture;
		HashCacheEntry* m_from_hash_cache;
		GIFRegTEX0 m_TEX0;
		GIFRegTEXA m_TEXA;
		int m_age;
		bool m_32_bits_fmt; // Allow to detect the casting of 32 bits as 16 bits texture
		bool m_shared_texture;
		u32 m_end_block; // Hint of the surface area.

	public:
		Surface();
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

	public:
		Palette(u16 pal, bool need_gs_texture);
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
		u64 operator()(const PaletteKey& key) const;
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

		void PreloadLevel(int level);

		void Write(const GSVector4i& r, int layer);
		void Flush(u32 count, int layer);

	public:
		std::shared_ptr<Palette> m_palette_obj;
		std::unique_ptr<u32[]> m_valid;// each u32 bits map to the 32 blocks of that page
		GSTexture* m_palette;
		GSVector4i m_valid_rect;
		u8 m_valid_hashes = 0;
		u8 m_complete_layers = 0;
		bool m_target;
		bool m_repeating;
		std::vector<GSVector2i>* m_p2t;
		// Keep a trace of the target origin. There is no guarantee that pointer will
		// still be valid on future. However it ought to be good when the source is created
		// so it can be used to access un-converted data for the current draw call.
		GSTexture** m_from_target;
		GIFRegTEX0 m_from_target_TEX0; // TEX0 of the target texture, if any, else equal to texture TEX0
		GIFRegTEX0 m_layer_TEX0[7]; // Detect already loaded value
		HashType m_layer_hash[7];
		// Keep a GSTextureCache::SourceMap::m_map iterator to allow fast erase
		std::array<u16, MAX_PAGES> m_erase_it;
		GSOffset::PageLooper m_pages;

	public:
		Source(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, bool dummy_container = false);
		virtual ~Source();

		__fi bool CanPreload() const { return CanPreloadTextureSize(m_TEX0.TW, m_TEX0.TH); }

		void Update(const GSVector4i& rect, int layer = 0);
		void UpdateLayer(const GIFRegTEX0& TEX0, const GSVector4i& rect, int layer = 0);

		bool ClutMatch(const PaletteKey& palette_key);
	};

	class Target : public Surface
	{
	public:
		const int m_type;
		bool m_used;
		GSDirtyRectList m_dirty;
		GSVector4i m_valid;
		const bool m_depth_supported;
		bool m_dirty_alpha;

	public:
		Target(const GIFRegTEX0& TEX0, const bool depth_supported, const int type);

		void UpdateValidity(const GSVector4i& rect);

		void Update();

		/// Updates the target, if the dirty area intersects with the specified rectangle.
		void UpdateIfDirtyIntersects(const GSVector4i& rc);
	};

	class PaletteMap
	{
	private:
		static const u16 MAX_SIZE = 65535; // Max size of each map.

		// Array of 2 maps, the first for 64B palettes and the second for 1024B palettes.
		// Each map stores the key PaletteKey (clut copy, pal value) pointing to the relevant shared pointer to Palette object.
		// There is one PaletteKey per Palette, and the hashing and comparison of PaletteKey is done with custom operators PaletteKeyHash and PaletteKeyEqual.
		std::array<std::unordered_map<PaletteKey, std::shared_ptr<Palette>, PaletteKeyHash, PaletteKeyEqual>, 2> m_maps;

	public:
		PaletteMap();

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
		void RemoveAt(Source* s);
	};

	struct SurfaceOffsetKeyElem
	{
		u32 psm;
		u32 bp;
		u32 bw;
		GSVector4i rect;
	};

	struct SurfaceOffsetKey
	{
		std::array<SurfaceOffsetKeyElem, 2> elems;  // A and B elems.
	};

	struct SurfaceOffset
	{
		bool is_valid;
		GSVector4i b2a_offset;  // B to A offset in B coords.
	};

	struct SurfaceOffsetKeyHash
	{
		std::size_t operator()(const SurfaceOffsetKey& key) const;
	};

	struct SurfaceOffsetKeyEqual
	{
		bool operator()(const SurfaceOffsetKey& lhs, const SurfaceOffsetKey& rhs) const;
	};

protected:
	PaletteMap m_palette_map;
	SourceMap m_src;
	std::unordered_map<HashCacheKey, HashCacheEntry, HashCacheKeyHash> m_hash_cache;
	u64 m_hash_cache_memory_usage = 0;
	FastList<Target*> m_dst[2];
	static u8* m_temp;
	constexpr static size_t S_SURFACE_OFFSET_CACHE_MAX_SIZE = std::numeric_limits<u16>::max();
	std::unordered_map<SurfaceOffsetKey, SurfaceOffset, SurfaceOffsetKeyHash, SurfaceOffsetKeyEqual> m_surface_offset_cache;
	Source* m_temporary_source = nullptr; // invalidated after the draw

	Source* CreateSource(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, Target* t = NULL, bool half_right = false, int x_offset = 0, int y_offset = 0, const GSVector2i* lod = nullptr, const GSVector4i* src_range = nullptr);
	Target* CreateTarget(const GIFRegTEX0& TEX0, int w, int h, int type, const bool clear);

	/// Looks up a target in the cache, and only returns it if the BP/BW/PSM match exactly.
	Target* GetExactTarget(u32 BP, u32 BW, u32 PSM) const;

	HashCacheEntry* LookupHashCache(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, bool& paltex, const u32* clut, const GSVector2i* lod);

	static void PreloadTexture(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, GSLocalMemory& mem, bool paltex, GSTexture* tex, u32 level);
	static HashType HashTexture(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA);

	// TODO: virtual void Write(Source* s, const GSVector4i& r) = 0;
	// TODO: virtual void Write(Target* t, const GSVector4i& r) = 0;

public:
	GSTextureCache();
	~GSTextureCache();

	__fi u64 GetHashCacheMemoryUsage() const { return m_hash_cache_memory_usage; }

	void Read(Target* t, const GSVector4i& r);
	void Read(Source* t, const GSVector4i& r);
	void RemoveAll();
	void RemovePartial();

	Source* LookupSource(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, const GSVector4i& r, const GSVector2i* lod);
	Source* LookupDepthSource(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, const GSVector4i& r, bool palette = false);

	Target* LookupTarget(const GIFRegTEX0& TEX0, const GSVector2i& size, int type, bool used, u32 fbmask = 0, const bool is_frame = false, const int real_h = 0);
	Target* LookupTarget(const GIFRegTEX0& TEX0, const GSVector2i& size, const int real_h);

	void InvalidateVideoMemType(int type, u32 bp);
	void InvalidateVideoMemSubTarget(GSTextureCache::Target* rt);
	void InvalidateVideoMem(const GSOffset& off, const GSVector4i& r, bool target = true);
	void InvalidateLocalMem(const GSOffset& off, const GSVector4i& r);
	bool Move(u32 SBP, u32 SBW, u32 SPSM, int sx, int sy, u32 DBP, u32 DBW, u32 DPSM, int dx, int dy, int w, int h);

	void IncAge();

	const char* to_string(int type)
	{
		return (type == DepthStencil) ? "Depth" : "Color";
	}

	void PrintMemoryUsage();

	void AttachPaletteToSource(Source* s, u16 pal, bool need_gs_texture);
	SurfaceOffset ComputeSurfaceOffset(const GSOffset& off, const GSVector4i& r, const Target* t);
	SurfaceOffset ComputeSurfaceOffset(const uint32_t bp, const uint32_t bw, const uint32_t psm, const GSVector4i& r, const Target* t);
	SurfaceOffset ComputeSurfaceOffset(const SurfaceOffsetKey& sok);

	/// Expands a target when the block pointer for a display framebuffer is within another target, but the read offset
	/// plus the height is larger than the current size of the target.
	static void ScaleTargetForDisplay(Target* t, const GIFRegTEX0& dispfb, int real_h);

	/// Invalidates a temporary source, a partial copy only created from the current RT/DS for the current draw.
	void InvalidateTemporarySource();

	/// Injects a texture into the hash cache, by using GSTexture::Swap(), transitively applying to all sources. Ownership of tex is transferred.
	void InjectHashCacheTexture(const HashCacheKey& key, GSTexture* tex);
};
