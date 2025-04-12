// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "GS/Renderers/Common/GSRenderer.h"
#include "GS/Renderers/Common/GSFastList.h"
#include "GS/Renderers/Common/GSDirtyRect.h"

#include <unordered_set>
#include <utility>
#include <limits>

class GSHwHack;

// Only for debugging. Reads back every target to local memory after drawing, effectively
// disabling caching between draws.
//#define DISABLE_HW_TEXTURE_CACHE

class GSTextureCache
{
public:
	friend GSHwHack;

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

	struct SourceRegion
	{
		u64 bits;

		bool HasX() const { return static_cast<u32>(bits) != 0; }
		bool HasY() const { return static_cast<u32>(bits >> 32) != 0; }
		bool HasEither() const { return (bits != 0); }

		void SetX(s32 min, s32 max) { bits |= (static_cast<u64>(static_cast<u16>(min)) | (static_cast<u64>(static_cast<u16>(max) << 16))); }
		void SetY(s32 min, s32 max) { bits |= ((static_cast<u64>(static_cast<u16>(min)) << 32) | (static_cast<u64>(static_cast<u16>(max)) << 48)); }

		s32 GetMinX() const { return static_cast<s16>(bits); }
		s32 GetMaxX() const { return static_cast<s16>((bits >> 16) & 0xFFFFu); }
		s32 GetMinY() const { return static_cast<s16>((bits >> 32) & 0xFFFFu); }
		s32 GetMaxY() const { return static_cast<s16>(bits >> 48); }

		s32 GetWidth() const { return (GetMaxX() - GetMinX()); }
		s32 GetHeight() const { return (GetMaxY() - GetMinY()); }

		/// Returns true if the area of the region exceeds the TW/TH size (i.e. "fixed tex0").
		bool IsFixedTEX0(GIFRegTEX0 TEX0) const;
		bool IsFixedTEX0(int tw, int th) const;
		bool IsFixedTEX0W(int tw) const;
		bool IsFixedTEX0H(int th) const;

		/// Returns the size that the region occupies.
		GSVector2i GetSize(int tw, int th) const;

		/// Returns the rectangle relative to the texture base pointer that the region occupies.
		GSVector4i GetRect(int tw, int th) const;

		/// When TW/TH is less than the extents covered by the region ("fixed tex0"), returns the offset
		/// which should be applied to any coordinates to relocate them to the actual region.
		GSVector4i GetOffset(int tw, int th) const;

		/// Reduces the range of texels relative to the specified mipmap level.
		SourceRegion AdjustForMipmap(u32 level) const;

		/// Adjusts the texture base pointer and block width relative to the region.
		void AdjustTEX0(GIFRegTEX0* TEX0) const;

		/// Creates a new source region based on the CLAMP register.
		static SourceRegion Create(GIFRegTEX0 TEX0, GIFRegCLAMP CLAMP);
	};

	using HashType = u64;

	struct HashCacheKey
	{
		HashType TEX0Hash, CLUTHash;
		GIFRegTEX0 TEX0;
		GIFRegTEXA TEXA;
		u32 region_width;
		u32 region_height;

		HashCacheKey();

		static HashCacheKey Create(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, const u32* clut, const GSVector2i* lod, SourceRegion region);

		HashCacheKey WithRemovedCLUTHash() const;
		void RemoveCLUTHash();

		__fi bool operator==(const HashCacheKey& e) const { return std::memcmp(this, &e, sizeof(*this)) == 0; }
		__fi bool operator!=(const HashCacheKey& e) const { return std::memcmp(this, &e, sizeof(*this)) != 0; }
		__fi bool operator<(const HashCacheKey& e) const { return std::memcmp(this, &e, sizeof(*this)) < 0; }
	};
	static_assert(sizeof(HashCacheKey) == 40, "HashCacheKey has no padding");

	struct HashCacheKeyHash
	{
		u64 operator()(const HashCacheKey& key) const;
	};

	struct HashCacheEntry
	{
		GSTexture* texture;
		u32 refcount;
		u16 age;
		std::pair<u8, u8> alpha_minmax;
		bool valid_alpha_minmax;
		bool is_replacement;
	};

	using HashCacheMap = std::unordered_map<HashCacheKey, HashCacheEntry, HashCacheKeyHash>;

	class Surface : public GSAlignedClass<32>
	{
	protected:
		Surface();
		~Surface();

	public:
		GSTexture* m_texture = nullptr;
		GIFRegTEX0 m_TEX0 = {};
		GIFRegTEXA m_TEXA = {};
		GSVector2i m_unscaled_size = {};
		float m_scale = 0.0f;
		int m_age = 0;
		u32 m_end_block = MAX_BP; // Hint of the surface area.
		bool m_32_bits_fmt = false; // Allow to detect the casting of 32 bits as 16 bits texture
		bool m_was_dst_matched = false;
		bool m_shared_texture = false;

		__fi GSTexture* GetTexture() const { return m_texture; }
		__fi int GetUnscaledWidth() const { return m_unscaled_size.x; }
		__fi int GetUnscaledHeight() const { return m_unscaled_size.y; }
		__fi const GSVector2i& GetUnscaledSize() const { return m_unscaled_size; }
		__fi GSVector4i GetUnscaledRect() const { return GSVector4i::loadh(m_unscaled_size); }
		__fi float GetScale() const { return m_scale; }

		/// Returns true if the target wraps around the end of GS memory.
		bool Wraps() const { return (m_end_block < m_TEX0.TBP0); }

		/// Returns the end block for the target, but doesn't wrap at 0x3FFF.
		/// Can be used for overlap tests.
		u32 UnwrappedEndBlock() const { return (m_end_block + (Wraps() ? MAX_BLOCKS : 0)); }

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
		GSTexture* m_tex_palette;
		u16 m_pal;
		std::pair<u8, u8> m_alpha_minmax;

	public:
		Palette(const u32* clut, u16 pal, bool need_gs_texture);
		~Palette();

		__fi std::pair<u8, u8> GetAlphaMinMax() const { return m_alpha_minmax; }
		std::pair<u8, u8> GetAlphaMinMax(u8 min_index, u8 max_index) const;

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

	class Target : public Surface
	{
	public:
		const int m_type = 0;
		int m_alpha_max = 0;
		int m_alpha_min = 0;
		bool m_alpha_range = false;

		// Valid alpha means "we have rendered to the alpha channel of this target".
		// A false value means that the alpha in local memory is still valid/up-to-date.
		bool m_valid_alpha_low = false;
		bool m_valid_alpha_high = false;
		bool m_valid_rgb = false;
		bool m_rt_alpha_scale = false;
		bool m_downscaled = false;
		int m_last_draw = 0;

		bool m_is_frame = false;
		bool m_used = false;
		float OffsetHack_modxy = 0.0f;
		GSDirtyRectList m_dirty;
		GSVector4i m_valid{};
		GSVector4i m_drawn_since_read{};
		int readbacks_since_draw = 0;

	public:
		Target(GIFRegTEX0 TEX0, int type, const GSVector2i& unscaled_size, float scale, GSTexture* texture);
		~Target();

		static Target* Create(GIFRegTEX0 TEX0, int w, int h, float scale, int type, bool clear);

		__fi bool HasValidAlpha() const { return (m_valid_alpha_low | m_valid_alpha_high); }
		bool HasValidBitsForFormat(u32 psm, bool req_color, bool req_alpha);

		void ResizeDrawn(const GSVector4i& rect);
		void UpdateDrawn(const GSVector4i& rect, bool can_resize = true);
		void ResizeValidity(const GSVector4i& rect);
		void UpdateValidity(const GSVector4i& rect, bool can_resize = true);

		void ScaleRTAlpha();
		void UnscaleRTAlpha();

		void Update(bool cannot_scale = false);

		/// Updates the target, if the dirty area intersects with the specified rectangle.
		void UpdateIfDirtyIntersects(const GSVector4i& rc);

		/// Updates the valid alpha flag, based on PSM and fbmsk.
		void UpdateValidChannels(u32 psm, u32 fbmsk);

		/// Resizes target texture, DOES NOT RESCALE.
		bool ResizeTexture(int new_unscaled_width, int new_unscaled_height, bool recycle_old = true, bool require_offset = false, GSVector4i offset = GSVector4i::zero(), bool keep_old = false);

	private:
		void UpdateTextureDebugName();
	};

	class Source : public Surface
	{
		struct
		{
			GSVector4i* rect;
			u32 count;
		} m_write = {};

		void PreloadLevel(int level);

		void Write(const GSVector4i& r, int layer, const GSOffset& off);
		void Flush(u32 count, int layer, const GSOffset& off);

	public:
		HashCacheEntry* m_from_hash_cache = nullptr;
		std::shared_ptr<Palette> m_palette_obj;
		std::unique_ptr<u32[]> m_valid;// each u32 bits map to the 32 blocks of that page
		GSTexture* m_palette = nullptr;
		GSVector4i m_valid_rect = {};
		GSVector2i m_lod = {};
		SourceRegion m_region = {};
		u8 m_valid_hashes = 0;
		u8 m_complete_layers = 0;
		bool m_target = false;
		bool m_target_direct = false;
		bool m_repeating = false;
		bool m_valid_alpha_minmax = false;
		std::pair<u8, u8> m_alpha_minmax = {0u, 255u};
		std::vector<GSVector2i>* m_p2t = nullptr;
		// Keep a trace of the target origin. There is no guarantee that pointer will
		// still be valid on future. However it ought to be good when the source is created
		// so it can be used to access un-converted data for the current draw call.
		Target* m_from_target = nullptr;
		GIFRegTEX0 m_from_target_TEX0 = {}; // TEX0 of the target texture, if any, else equal to texture TEX0
		GIFRegTEX0 m_layer_TEX0[7] = {}; // Detect already loaded value
		HashType m_layer_hash[7] = {};
		// Keep a GSTextureCache::SourceMap::m_map iterator to allow fast erase
		// Deliberately not initialized to save cycles.
		std::array<u16, MAX_PAGES> m_erase_it;
		GSOffset::PageLooper m_pages;

	public:
		Source(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA);
		virtual ~Source();

		__fi bool CanPreload() const { return CanPreloadTextureSize(m_TEX0.TW, m_TEX0.TH); }
		__fi bool IsFromTarget() const { return m_target; }
		bool IsPaletteFormat() const;

		__fi const SourceRegion& GetRegion() const { return m_region; }
		__fi GSVector2i GetRegionSize() const { return m_region.GetSize(m_unscaled_size.x, m_unscaled_size.y); }
		__fi GSVector4i GetRegionRect() const { return m_region.GetRect(m_unscaled_size.x, m_unscaled_size.y); }
		__fi const std::pair<u8, u8> GetAlphaMinMax() const { return m_alpha_minmax; }

		void SetPages();

		void Update(const GSVector4i& rect, int layer = 0);
		void UpdateLayer(const GIFRegTEX0& TEX0, const GSVector4i& rect, int layer = 0);

		bool ClutMatch(const PaletteKey& palette_key);
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
		std::shared_ptr<Palette> LookupPalette(const u32* clut, u16 pal, bool need_gs_texture);

		void Clear(); // Clears m_maps, thus deletes Palette objects
	};

	class SourceMap
	{
	public:
		std::unordered_set<Source*> m_surfaces;
		std::array<FastList<Source*>, MAX_PAGES> m_map;

		void Add(Source* s, const GIFRegTEX0& TEX0);
		void SwapTexture(GSTexture* old_tex, GSTexture* new_tex);
		void RemoveAll();
		void RemoveAt(Source* s);
	};

	struct TargetHeightElem
	{
		union
		{
			u32 bits;

			struct
			{
				u32 bp : 14;
				u32 fbw : 6;
				u32 psm : 6;
				u32 pad : 6;
			};
		};

		s32 width;
		s32 height;
		u32 age;
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
	u64 m_source_memory_usage = 0;
	HashCacheMap m_hash_cache;
	u64 m_hash_cache_memory_usage = 0;
	u64 m_hash_cache_replacement_memory_usage = 0;

	FastList<Target*> m_dst[2];
	FastList<TargetHeightElem> m_target_heights;
	u64 m_target_memory_usage = 0;

	int m_expected_src_bp = -1;
	int m_remembered_src_bp = -1;
	int m_expected_dst_bp = -1;
	int m_remembered_dst_bp = -1;

	constexpr static size_t S_SURFACE_OFFSET_CACHE_MAX_SIZE = std::numeric_limits<u16>::max();
	std::unordered_map<SurfaceOffsetKey, SurfaceOffset, SurfaceOffsetKeyHash, SurfaceOffsetKeyEqual> m_surface_offset_cache;

	Source* m_temporary_source = nullptr; // invalidated after the draw
	GSTexture* m_temporary_z = nullptr; // invalidated after the draw

	std::unique_ptr<GSDownloadTexture> m_color_download_texture;
	std::unique_ptr<GSDownloadTexture> m_uint16_download_texture;
	std::unique_ptr<GSDownloadTexture> m_uint32_download_texture;

	Source* CreateSource(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, Target* t, bool half_right, int x_offset, int y_offset, const GSVector2i* lod, const GSVector4i* src_range, GSTexture* gpu_clut, SourceRegion region);

	bool PreloadTarget(GIFRegTEX0 TEX0, const GSVector2i& size, const GSVector2i& valid_size, bool is_frame,
		bool preload, bool preserve_target, const GSVector4i draw_rect, Target* dst, GSTextureCache::Source* src = nullptr);

	// Returns scaled texture size.
	static GSVector2i ScaleRenderTargetSize(const GSVector2i& sz, float scale);

	/// Expands a target when the block pointer for a display framebuffer is within another target, but the read offset
	/// plus the height is larger than the current size of the target.
	void ScaleTargetForDisplay(Target* t, const GIFRegTEX0& dispfb, int real_w, int real_h);

	/// Resizes the download texture if needed.
	bool PrepareDownloadTexture(u32 width, u32 height, GSTexture::Format format, std::unique_ptr<GSDownloadTexture>* tex);

	HashCacheEntry* LookupHashCache(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, bool& paltex, const u32* clut, const GSVector2i* lod, SourceRegion region);
	void RemoveFromHashCache(HashCacheMap::iterator it);
	void AgeHashCache();

	static void PreloadTexture(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, SourceRegion region, GSLocalMemory& mem, bool paltex, GSTexture* tex, u32 level, std::pair<u8, u8>* alpha_minmax);
	static HashType HashTexture(const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, SourceRegion region);

	// TODO: virtual void Write(Source* s, const GSVector4i& r) = 0;
	// TODO: virtual void Write(Target* t, const GSVector4i& r) = 0;

	Source* CreateMergedSource(GIFRegTEX0 TEX0, GIFRegTEXA TEXA, SourceRegion region, float scale);

public:
	GSTextureCache();
	~GSTextureCache();

	__fi u64 GetHashCacheMemoryUsage() const { return m_hash_cache_memory_usage; }
	__fi u64 GetHashCacheReplacementMemoryUsage() const { return m_hash_cache_replacement_memory_usage; }
	__fi u64 GetTotalHashCacheMemoryUsage() const { return (m_hash_cache_memory_usage + m_hash_cache_replacement_memory_usage); }
	__fi u64 GetSourceMemoryUsage() const { return m_source_memory_usage; }
	__fi u64 GetTargetMemoryUsage() const { return m_target_memory_usage; }

	void Read(Target* t, const GSVector4i& r);
	void Read(Source* t, const GSVector4i& r);
	void RemoveAll(bool sources, bool targets, bool hash_cache);
	void ReadbackAll();
	static void AddDirtyRectTarget(Target* target, GSVector4i rect, u32 psm, u32 bw, RGBAMask rgba, bool req_linear = false);
	void ResizeTarget(Target* t, GSVector4i rect, u32 tbp, u32 psm, u32 tbw);
	static bool FullRectDirty(Target* target, u32 rgba_mask);
	static bool FullRectDirty(Target* target);
	bool CanTranslate(u32 bp, u32 bw, u32 spsm, GSVector4i r, u32 dbp, u32 dpsm, u32 dbw);
	GSVector4i TranslateAlignedRectByPage(u32 tbp, u32 tebp, u32 tbw, u32 tpsm, u32 sbp, u32 spsm, u32 sbw, GSVector4i src_r, bool is_invalidation = false);
	GSVector4i TranslateAlignedRectByPage(Target* t, u32 sbp, u32 spsm, u32 sbw, GSVector4i src_r, bool is_invalidation = false);
	void DirtyRectByPage(u32 sbp, u32 spsm, u32 sbw, Target* t, GSVector4i src_r);
	void DirtyRectByPageOld(u32 sbp, u32 spsm, u32 sbw, Target* t, GSVector4i src_r);
	GSTexture* LookupPaletteSource(u32 CBP, u32 CPSM, u32 CBW, GSVector2i& offset, float* scale, const GSVector2i& size);
	std::shared_ptr<Palette> LookupPaletteObject(const u32* clut, u16 pal, bool need_gs_texture);

	Source* LookupSource(const bool is_color, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, const GIFRegCLAMP& CLAMP, const GSVector4i& r, const GSVector2i* lod, const bool possible_shuffle, const bool linear, const u32 frame_fbp = 0xFFFFFFFF, bool req_color = true, bool req_alpha = true);
	Source* LookupDepthSource(const bool is_depth, const GIFRegTEX0& TEX0, const GIFRegTEXA& TEXA, const GIFRegCLAMP& CLAMP, const GSVector4i& r, const bool possible_shuffle, const bool linear, const u32 frame_fbp = 0xFFFFFFFF, bool req_color = true, bool req_alpha = true, bool palette = false);

	Target* FindTargetOverlap(Target* target, int type, int psm);
	Target* LookupTarget(GIFRegTEX0 TEX0, const GSVector2i& size, float scale, int type, bool used = true, u32 fbmask = 0,
						 bool is_frame = false, bool preload = GSConfig.PreloadFrameWithGSData, bool preserve_rgb = true, bool preserve_alpha = true,
		const GSVector4i draw_rc = GSVector4i::zero(), bool is_shuffle = false, bool possible_clear = false, bool preserve_scale = false, GSTextureCache::Source* src = nullptr, GSTextureCache::Target* ds = nullptr, int offset = -1);
	Target* CreateTarget(GIFRegTEX0 TEX0, const GSVector2i& size, const GSVector2i& valid_size,float scale, int type, bool used = true, u32 fbmask = 0,
		bool is_frame = false, bool preload = GSConfig.PreloadFrameWithGSData, bool preserve_target = true,
		const GSVector4i draw_rc = GSVector4i::zero(), GSTextureCache::Source* src = nullptr);
	Target* LookupDisplayTarget(GIFRegTEX0 TEX0, const GSVector2i& size, float scale, bool is_feedback);

	/// Looks up a target in the cache, and only returns it if the BP/BW match exactly.
	Target* GetExactTarget(u32 BP, u32 BW, int type, u32 end_bp);
	Target* GetTargetWithSharedBits(u32 BP, u32 PSM) const;
	Target* FindOverlappingTarget(GSTextureCache::Target* target) const;
	Target* FindOverlappingTarget(u32 BP, u32 end_bp) const;
	Target* FindOverlappingTarget(u32 BP, u32 BW, u32 PSM, GSVector4i rc) const;

	GSVector2i GetTargetSize(u32 bp, u32 fbw, u32 psm, s32 min_width, s32 min_height, bool can_expand = true);
	bool HasTargetInHeightCache(u32 bp, u32 fbw, u32 psm, u32 max_age = std::numeric_limits<u32>::max(), bool move_front = true);
	bool Has32BitTarget(u32 bp);

	void InvalidateContainedTargets(u32 start_bp, u32 end_bp, u32 write_psm = PSMCT32, u32 write_bw = 1);
	void InvalidateVideoMemType(int type, u32 bp, u32 write_psm = PSMCT32, u32 write_fbmsk = 0, bool dirty_only = false);
	void InvalidateVideoMemSubTarget(GSTextureCache::Target* rt);
	void InvalidateVideoMem(const GSOffset& off, const GSVector4i& r, bool target = true);
	void InvalidateLocalMem(const GSOffset& off, const GSVector4i& r, bool full_flush = false);

	/// Removes any sources which point to the specified target.
	void InvalidateSourcesFromTarget(const Target* t);

	/// Removes any sources which point to the same address as a new target.
	void ReplaceSourceTexture(Source* s, GSTexture* new_texture, float new_scale, const GSVector2i& new_unscaled_size,
		HashCacheEntry* hc_entry, bool new_texture_is_shared);

	/// Converts single color value to depth using the specified shader expression.
	static float ConvertColorToDepth(u32 c, ShaderConvert convert);

	/// Converts single depth value to colour using the specified shader expression.
	static u32 ConvertDepthToColor(float d, ShaderConvert convert);

	/// Copies RGB channels from depth target to a color target.
	bool CopyRGBFromDepthToColor(Target* dst, Target* depth_src);

	bool Move(u32 SBP, u32 SBW, u32 SPSM, int sx, int sy, u32 DBP, u32 DBW, u32 DPSM, int dx, int dy, int w, int h);
	bool ShuffleMove(u32 BP, u32 BW, u32 PSM, int sx, int sy, int dx, int dy, int w, int h);
	bool PageMove(u32 SBP, u32 DBP, u32 BW, u32 PSM, int sx, int sy, int dx, int dy, int w, int h);
	void CopyPages(Target* src, u32 sbw, u32 src_offset, Target* dst, u32 dbw, u32 dst_offset, u32 num_pages,
		ShaderConvert shader = ShaderConvert::COPY);

	void IncAge();

	static const char* to_string(int type)
	{
		return (type == DepthStencil) ? "Depth" : "Color";
	}

	void AttachPaletteToSource(Source* s, u16 pal, bool need_gs_texture, bool update_alpha_minmax);
	void AttachPaletteToSource(Source* s, GSTexture* gpu_clut);
	SurfaceOffset ComputeSurfaceOffset(const GSOffset& off, const GSVector4i& r, const Target* t);
	SurfaceOffset ComputeSurfaceOffset(const uint32_t bp, const uint32_t bw, const uint32_t psm, const GSVector4i& r, const Target* t);
	SurfaceOffset ComputeSurfaceOffset(const SurfaceOffsetKey& sok);

	/// Invalidates a temporary source, a partial copy only created from the current RT/DS for the current draw.
	void InvalidateTemporarySource();
	void SetTemporaryZ(GSTexture* temp_z);
	GSTexture* GetTemporaryZ();

	/// Invalidates a temporary Z, a partial copy only created from the current DS for the current draw when Z is not offset but RT is
	void InvalidateTemporaryZ();

	/// Injects a texture into the hash cache, by using GSTexture::Swap(), transitively applying to all sources. Ownership of tex is transferred.
	void InjectHashCacheTexture(const HashCacheKey& key, GSTexture* tex, const std::pair<u8, u8>& alpha_minmax);
};

extern std::unique_ptr<GSTextureCache> g_texture_cache;
