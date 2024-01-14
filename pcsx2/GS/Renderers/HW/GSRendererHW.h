// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GSTextureCache.h"
#include "GS/Renderers/Common/GSFunctionMap.h"
#include "GS/Renderers/Common/GSRenderer.h"
#include "GS/Renderers/SW/GSTextureCacheSW.h"
#include "GS/GSState.h"
#include "GS/MultiISA.h"

class GSRendererHW;
MULTI_ISA_DEF(class GSRendererHWFunctions;)
MULTI_ISA_DEF(void GSRendererHWPopulateFunctions(GSRendererHW& renderer);)

class GSHwHack;

class GSRendererHW : public GSRenderer
{
	MULTI_ISA_FRIEND(GSRendererHWFunctions);
	friend GSHwHack;

public:
	static constexpr int MAX_FRAMEBUFFER_HEIGHT = 1280;

private:
	static constexpr float SSR_UV_TOLERANCE = 1.0f;

	using GSC_Ptr = bool(*)(GSRendererHW& r, int& skip);	// GSC - Get Skip Count
	using OI_Ptr = bool(*)(GSRendererHW& r, GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t); // OI - Before draw
	using MV_Ptr = bool(*)(GSRendererHW& r); // MV - Move

	// Require special argument
	bool OI_BlitFMV(GSTextureCache::Target* _rt, GSTextureCache::Source* t, const GSVector4i& r_draw);
	bool TryGSMemClear(bool no_rt, bool preserve_rt, bool invalidate_rt, u32 rt_end_bp, bool no_ds,
		bool preserve_z, bool invalidate_z, u32 ds_end_bp);
	void ClearGSLocalMemory(const GSOffset& off, const GSVector4i& r, u32 vert_color);
	bool DetectDoubleHalfClear(bool& no_rt, bool& no_ds);
	bool DetectStripedDoubleClear(bool& no_rt, bool& no_ds);
	bool TryTargetClear(GSTextureCache::Target* rt, GSTextureCache::Target* ds, bool preserve_rt_color, bool preserve_depth);
	void SetNewFRAME(u32 bp, u32 bw, u32 psm);
	void SetNewZBUF(u32 bp, u32 psm);

	u16 Interpolate_UV(float alpha, int t0, int t1);
	float alpha0(int L, int X0, int X1);
	float alpha1(int L, int X0, int X1);
	void SwSpriteRender();
	bool CanUseSwSpriteRender();
	bool IsConstantDirectWriteMemClear();
	u32 GetConstantDirectWriteMemClearColor() const;
	u32 GetConstantDirectWriteMemClearDepth() const;
	bool IsReallyDithered() const;
	bool AreAnyPixelsDiscarded() const;
	bool IsDiscardingDstColor();
	bool IsDiscardingDstRGB();
	bool IsDiscardingDstAlpha() const;
	bool PrimitiveCoversWithoutGaps();
	bool TextureCoversWithoutGapsNotEqual();

	enum class CLUTDrawTestResult
	{
		NotCLUTDraw,
		CLUTDrawOnCPU,
		CLUTDrawOnGPU,
	};

	bool HasEEUpload(GSVector4i r);
	CLUTDrawTestResult PossibleCLUTDraw();
	CLUTDrawTestResult PossibleCLUTDrawAggressive();
	bool CanUseSwPrimRender(bool no_rt, bool no_ds, bool draw_sprite_tex);
	bool (*SwPrimRender)(GSRendererHW&, bool invalidate_tc, bool add_ee_transfer);

	template <bool linear>
	void RoundSpriteOffset();

	void DrawPrims(GSTextureCache::Target* rt, GSTextureCache::Target* ds, GSTextureCache::Source* tex, const TextureMinMaxResult& tmm);

	void ResetStates();
	void SetupIA(float target_scale, float sx, float sy);
	void EmulateTextureShuffleAndFbmask(GSTextureCache::Target* rt, GSTextureCache::Source* tex);
	bool EmulateChannelShuffle(GSTextureCache::Target* src, bool test_only);
	void EmulateBlending(int rt_alpha_min, int rt_alpha_max, bool& DATE_PRIMID, bool& DATE_BARRIER, bool& blending_alpha_pass);
	void CleanupDraw(bool invalidate_temp_src);

	void EmulateTextureSampler(const GSTextureCache::Target* rt, const GSTextureCache::Target* ds,
		GSTextureCache::Source* tex, const TextureMinMaxResult& tmm, GSTexture*& src_copy);
	void HandleTextureHazards(const GSTextureCache::Target* rt, const GSTextureCache::Target* ds,
		const GSTextureCache::Source* tex, const TextureMinMaxResult& tmm, GSTextureCache::SourceRegion& source_region,
		bool& target_region, GSVector2i& unscaled_size, float& scale, GSTexture*& src_copy);
	bool CanUseTexIsFB(const GSTextureCache::Target* rt, const GSTextureCache::Source* tex,
		const TextureMinMaxResult& tmm);

	void EmulateZbuffer(const GSTextureCache::Target* ds);
	void EmulateATST(float& AREF, GSHWDrawConfig::PSSelector& ps, bool pass_2);

	void SetTCOffset();

	bool IsPossibleChannelShuffle() const;
	bool NextDrawMatchesShuffle() const;
	bool IsSplitTextureShuffle(GSTextureCache::Target* rt);
	GSVector4i GetSplitTextureShuffleDrawRect() const;
	u32 GetEffectiveTextureShuffleFbmsk() const;

	static GSVector4i GetDrawRectForPages(u32 bw, u32 psm, u32 num_pages);
	bool TryToResolveSinglePageFramebuffer(GIFRegFRAME& FRAME, bool only_next_draw);

	bool IsSplitClearActive() const;
	bool CheckNextDrawForSplitClear(const GSVector4i& r, u32* pages_covered_by_this_draw) const;
	bool IsStartingSplitClear();
	bool ContinueSplitClear();
	void FinishSplitClear();

	bool IsRTWritten();
	bool IsUsingCsInBlend();
	bool IsUsingAsInBlend();

	GSVector4i m_r = {};
	
	// We modify some of the context registers to optimize away unnecessary operations.
	// Instead of messing with the real context, we copy them and use those instead.
	struct HWCachedCtx
	{
		GIFRegTEX0 TEX0;
		GIFRegCLAMP CLAMP;
		GIFRegTEST TEST;
		GIFRegFRAME FRAME;
		GIFRegZBUF ZBUF;

		__ri bool DepthRead() const { return TEST.ZTE && TEST.ZTST >= 2; }

		__ri bool DepthWrite() const
		{
			if (TEST.ATE && TEST.ATST == ATST_NEVER &&
				TEST.AFAIL != AFAIL_ZB_ONLY) // alpha test, all pixels fail, z buffer is not updated
			{
				return false;
			}

			return ZBUF.ZMSK == 0 && TEST.ZTE != 0; // ZTE == 0 is bug on the real hardware, write is blocked then
		}
	};

	// CRC Hacks
	bool IsBadFrame();
	GSC_Ptr m_gsc = nullptr;
	OI_Ptr m_oi = nullptr;
	MV_Ptr m_mv = nullptr;
	int m_skip = 0;
	int m_skip_offset = 0;

	u32 m_split_texture_shuffle_pages = 0;
	u32 m_split_texture_shuffle_pages_high = 0;
	u32 m_split_texture_shuffle_start_FBP = 0;
	u32 m_split_texture_shuffle_start_TBP = 0;
	u32 m_split_texture_shuffle_fbw = 0;

	u32 m_last_channel_shuffle_fbmsk = 0;

	GIFRegFRAME m_split_clear_start = {};
	GIFRegZBUF m_split_clear_start_Z = {};
	u32 m_split_clear_pages = 0; // if zero, inactive
	u32 m_split_clear_color = 0;

	std::optional<bool> m_primitive_covers_without_gaps;
	bool m_userhacks_tcoffset = false;
	float m_userhacks_tcoffset_x = 0.0f;
	float m_userhacks_tcoffset_y = 0.0f;

	GSVector2i m_lod = {}; // Min & Max level of detail

	GSHWDrawConfig m_conf = {};
	HWCachedCtx m_cached_ctx;

	// software sprite renderer state
	std::vector<GSVertexSW> m_sw_vertex_buffer;
	std::unique_ptr<GSTextureCacheSW::Texture> m_sw_texture[7 + 1];
	std::unique_ptr<GSVirtualAlignedClass<32>> m_sw_rasterizer;

public:
	GSRendererHW();
	virtual ~GSRendererHW() override;

	__fi static GSRendererHW* GetInstance() { return static_cast<GSRendererHW*>(g_gs_renderer.get()); }
	__fi HWCachedCtx* GetCachedCtx() { return &m_cached_ctx; }
	void Destroy() override;

	void UpdateRenderFixes() override;

	bool CanUpscale() override;
	float GetUpscaleMultiplier() override;
	void Lines2Sprites();
	bool VerifyIndices();
	void ExpandLineIndices();
	void ConvertSpriteTextureShuffle(bool& write_ba, bool& read_ba, GSTextureCache::Target* rt, GSTextureCache::Source* tex);
	GSVector4 RealignTargetTextureCoordinate(const GSTextureCache::Source* tex);
	GSVector4i ComputeBoundingBox(const GSVector2i& rtsize, float rtscale);
	void MergeSprite(GSTextureCache::Source* tex);
	float GetTextureScaleFactor() override;
	GSVector2i GetValidSize(const GSTextureCache::Source* tex = nullptr);
	GSVector2i GetTargetSize(const GSTextureCache::Source* tex = nullptr);

	void Reset(bool hardware_reset) override;
	void UpdateSettings(const Pcsx2Config::GSOptions& old_config) override;
	void VSync(u32 field, bool registers_written, bool idle_frame) override;

	GSTexture* GetOutput(int i, float& scale, int& y_offset) override;
	GSTexture* GetFeedbackOutput(float& scale) override;
	void InvalidateVideoMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r) override;
	void InvalidateLocalMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r, bool clut = false) override;
	void Move() override;
	void Draw() override;

	void PurgeTextureCache(bool sources, bool targets, bool hash_cache) override;
	void ReadbackTextureCache() override;
	GSTexture* LookupPaletteSource(u32 CBP, u32 CPSM, u32 CBW, GSVector2i& offset, float* scale, const GSVector2i& size) override;

	/// Called by the texture cache to know for certain whether there is a channel shuffle.
	bool TestChannelShuffle(GSTextureCache::Target* src);

	/// Returns true if the specified texture address matches the frame or Z buffer.
	bool IsTBPFrameOrZ(u32 tbp);

	/// Offsets the current draw, used for RT-in-RT. Offsets are relative to the *current* FBP, not the new FBP.
	void OffsetDraw(s32 fbp_offset, s32 zbp_offset, s32 xoffset, s32 yoffset);

	/// Replaces vertices with the specified fullscreen quad.
	void ReplaceVerticesWithSprite(const GSVector4i& unscaled_rect, const GSVector4i& unscaled_uv_rect,
		const GSVector2i& unscaled_size, const GSVector4i& scissor);
	void ReplaceVerticesWithSprite(const GSVector4i& unscaled_rect, const GSVector2i& unscaled_size);

	/// Starts a HLE'ed hardware draw, which can be further customized by the caller.
	GSHWDrawConfig& BeginHLEHardwareDraw(
		GSTexture* rt, GSTexture* ds, float rt_scale, GSTexture* tex, float tex_scale, const GSVector4i& unscaled_rect);

	/// Submits a previously set up HLE hardware draw, copying any textures as needed if there's hazards.
	void EndHLEHardwareDraw(bool force_copy_on_hazard = false);
};
