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

struct GSFrameInfo
{
	u32 FBP;
	u32 FPSM;
	u32 FBMSK;
	u32 ZBP;
	u32 ZMSK;
	u32 ZTST;
	u32 TME;
	u32 TBP0;
	u32 TPSM;
};

class GSRendererHW : public GSRenderer
{
	MULTI_ISA_FRIEND(GSRendererHWFunctions);
	friend GSHwHack;

public:
	static constexpr int MAX_FRAMEBUFFER_HEIGHT = 1280;

private:
	static constexpr float SSR_UV_TOLERANCE = 1.0f;

	using GSC_Ptr = bool(*)(GSRendererHW& r, const GSFrameInfo& fi, int& skip);	// GSC - Get Skip Count
	using OI_Ptr = bool(*)(GSRendererHW& r, GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t); // OI - Before draw

	// Require special argument
	bool OI_BlitFMV(GSTextureCache::Target* _rt, GSTextureCache::Source* t, const GSVector4i& r_draw);
	bool OI_GsMemClear(); // always on
	void OI_DoubleHalfClear(GSTextureCache::Target*& rt, GSTextureCache::Target*& ds); // always on

	u16 Interpolate_UV(float alpha, int t0, int t1);
	float alpha0(int L, int X0, int X1);
	float alpha1(int L, int X0, int X1);
	void SwSpriteRender();
	bool CanUseSwSpriteRender();
	bool IsConstantDirectWriteMemClear(bool include_zero);
	bool IsBlendedOrOpaque();

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
	bool (*SwPrimRender)(GSRendererHW&, bool invalidate_tc);

	template <bool linear>
	void RoundSpriteOffset();

	void DrawPrims(GSTextureCache::Target* rt, GSTextureCache::Target* ds, GSTextureCache::Source* tex);

	void ResetStates();
	void SetupIA(float target_scale, float sx, float sy);
	void EmulateTextureShuffleAndFbmask();
	void EmulateChannelShuffle(const GSTextureCache::Source* tex);
	void EmulateBlending(bool& DATE_PRIMID, bool& DATE_BARRIER, bool& blending_alpha_pass);
	void EmulateTextureSampler(const GSTextureCache::Source* tex);
	void EmulateZbuffer();
	void EmulateATST(float& AREF, GSHWDrawConfig::PSSelector& ps, bool pass_2);

	void SetTCOffset();

	bool IsSplitTextureShuffle();
	GSVector4i GetSplitTextureShuffleDrawRect() const;

	GSTextureCache* m_tc;
	GSVector4i m_r = {};
	GSTextureCache::Source* m_src = nullptr;

	// CRC Hacks
	bool IsBadFrame();
	GSC_Ptr m_gsc = nullptr;
	OI_Ptr m_oi = nullptr;
	int m_skip = 0;
	int m_skip_offset = 0;

	u32 m_split_texture_shuffle_pages = 0;
	u32 m_split_texture_shuffle_pages_high = 0;
	u32 m_split_texture_shuffle_start_FBP = 0;
	u32 m_split_texture_shuffle_start_TBP = 0;

	u32 m_last_channel_shuffle_fbmsk = 0;
	bool m_channel_shuffle = false;

	bool m_tex_is_fb = false;
	bool m_userhacks_tcoffset = false;
	float m_userhacks_tcoffset_x = 0.0f;
	float m_userhacks_tcoffset_y = 0.0f;

	GSVector2i m_lod = {}; // Min & Max level of detail

	GSHWDrawConfig m_conf = {};

	// software sprite renderer state
	std::vector<GSVertexSW> m_sw_vertex_buffer;
	std::unique_ptr<GSTextureCacheSW::Texture> m_sw_texture[7 + 1];
	std::unique_ptr<GSVirtualAlignedClass<32>> m_sw_rasterizer;

	// Tracking draw counters for idle frame detection.
	int m_last_draw_n = 0;
	int m_last_transfer_n = 0;

public:
	GSRendererHW();
	virtual ~GSRendererHW() override;

	__fi static GSRendererHW* GetInstance() { return static_cast<GSRendererHW*>(g_gs_renderer.get()); }
	__fi GSTextureCache* GetTextureCache() const { return m_tc; }

	void Destroy() override;

	void SetGameCRC(u32 crc) override;
	void UpdateCRCHacks() override;

	bool CanUpscale() override;
	float GetUpscaleMultiplier() override;
	void Lines2Sprites();
	bool VerifyIndices();
	template <GSHWDrawConfig::VSExpand Expand> void ExpandIndices();
	void ConvertSpriteTextureShuffle(bool& write_ba, bool& read_ba);
	GSVector4 RealignTargetTextureCoordinate(const GSTextureCache::Source* tex);
	GSVector4i ComputeBoundingBox(const GSVector2i& rtsize, float rtscale);
	void MergeSprite(GSTextureCache::Source* tex);
	float GetTextureScaleFactor() override;
	GSVector2i GetTargetSize(const GSTextureCache::Source* tex = nullptr);

	void Reset(bool hardware_reset) override;
	void UpdateSettings(const Pcsx2Config::GSOptions& old_config) override;
	void VSync(u32 field, bool registers_written) override;

	GSTexture* GetOutput(int i, float& scale, int& y_offset) override;
	GSTexture* GetFeedbackOutput(float& scale) override;
	void ExpandTarget(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r) override;
	void InvalidateVideoMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r, bool eewrite = false) override;
	void InvalidateLocalMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r, bool clut = false) override;
	void Move() override;
	void Draw() override;

	void PurgeTextureCache() override;
	void ReadbackTextureCache() override;
	GSTexture* LookupPaletteSource(u32 CBP, u32 CPSM, u32 CBW, GSVector2i& offset, float* scale, const GSVector2i& size) override;

	// Called by the texture cache to know if current texture is useful
	bool UpdateTexIsFB(GSTextureCache::Target* src, const GIFRegTEX0& TEX0);

	// Called by the texture cache when optimizing the copy range for sources
	bool IsPossibleTextureShuffle(GSTextureCache::Target* dst, const GIFRegTEX0& TEX0) const;
};
