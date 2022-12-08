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

class GSRendererHW : public GSRenderer
{
	MULTI_ISA_FRIEND(GSRendererHWFunctions);
public:
	static constexpr int MAX_FRAMEBUFFER_HEIGHT = 1280;

private:
	static constexpr float SSR_UV_TOLERANCE = 1.0f;

#pragma region hacks

	typedef bool (GSRendererHW::*OI_Ptr)(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	typedef void (GSRendererHW::*OO_Ptr)();
	typedef bool (GSRendererHW::*CU_Ptr)();

	// Require special argument
	bool OI_BlitFMV(GSTextureCache::Target* _rt, GSTextureCache::Source* t, const GSVector4i& r_draw);
	bool OI_GsMemClear(); // always on
	void OI_DoubleHalfClear(GSTextureCache::Target*& rt, GSTextureCache::Target*& ds); // always on

	bool OI_BigMuthaTruckers(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_DBZBTGames(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_FFXII(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_FFX(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_MetalSlug6(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_RozenMaidenGebetGarden(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_SonicUnleashed(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_PointListPalette(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_ArTonelico2(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_JakGames(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_BurnoutGames(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);

	void OO_BurnoutGames();

	class Hacks
	{
		template <class T>
		class HackEntry
		{
		public:
			CRC::Title title;
			CRC::Region region;
			T func;

			HackEntry(CRC::Title t, CRC::Region r, T f)
			{
				title = t;
				region = r;
				func = f;
			}
		};

		template <class T>
		class FunctionMap : public GSFunctionMap<u32, T>
		{
			std::list<HackEntry<T>>& m_tbl;

			T GetDefaultFunction(u32 key)
			{
				CRC::Title title = (CRC::Title)(key & 0xffffff);
				CRC::Region region = (CRC::Region)(key >> 24);

				for (const auto& entry : m_tbl)
				{
					if (entry.title == title && (entry.region == CRC::RegionCount || entry.region == region))
					{
						return entry.func;
					}
				}

				return NULL;
			}

		public:
			FunctionMap(std::list<HackEntry<T>>& tbl)
				: m_tbl(tbl)
			{
			}
		};

		std::list<HackEntry<OI_Ptr>> m_oi_list;
		std::list<HackEntry<OO_Ptr>> m_oo_list;

		FunctionMap<OI_Ptr> m_oi_map;
		FunctionMap<OO_Ptr> m_oo_map;

	public:
		OI_Ptr m_oi;
		OO_Ptr m_oo;

		Hacks();

		void SetGameCRC(const CRC::Game& game);

	} m_hacks;

#pragma endregion

	u16 Interpolate_UV(float alpha, int t0, int t1);
	float alpha0(int L, int X0, int X1);
	float alpha1(int L, int X0, int X1);
	void SwSpriteRender();
	bool CanUseSwSpriteRender();

	bool PossibleCLUTDraw();
	bool PossibleCLUTDrawAggressive();
	bool CanUseSwPrimRender(bool no_rt, bool no_ds, bool draw_sprite_tex);
	bool (*SwPrimRender)(GSRendererHW&);

	template <bool linear>
	void RoundSpriteOffset();

	void DrawPrims(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* tex);

	void ResetStates();
	void SetupIA(const float& sx, const float& sy);
	void EmulateTextureShuffleAndFbmask();
	void EmulateChannelShuffle(const GSTextureCache::Source* tex);
	void EmulateBlending(bool& DATE_PRIMID, bool& DATE_BARRIER, bool& blending_alpha_pass);
	void EmulateTextureSampler(const GSTextureCache::Source* tex);
	void EmulateZbuffer();
	void EmulateATST(float& AREF, GSHWDrawConfig::PSSelector& ps, bool pass_2);

	void SetTCOffset();

	GSTextureCache* m_tc;
	GSVector4i m_r;
	GSTextureCache::Source* m_src;

	bool m_reset;
	bool m_tex_is_fb;
	bool m_channel_shuffle;
	bool m_userhacks_tcoffset;
	float m_userhacks_tcoffset_x;
	float m_userhacks_tcoffset_y;

	GSVector2i m_lod; // Min & Max level of detail

	GSHWDrawConfig m_conf;

	// software sprite renderer state
	std::vector<GSVertexSW> m_sw_vertex_buffer;
	std::unique_ptr<GSTextureCacheSW::Texture> m_sw_texture;
	std::unique_ptr<GSVirtualAlignedClass<32>> m_sw_rasterizer;

public:
	GSRendererHW();
	virtual ~GSRendererHW() override;

	__fi static GSRendererHW* GetInstance() { return static_cast<GSRendererHW*>(g_gs_renderer.get()); }
	__fi GSTextureCache* GetTextureCache() const { return m_tc; }

	void Destroy() override;

	void SetGameCRC(u32 crc, int options) override;
	bool CanUpscale() override;
	float GetUpscaleMultiplier() override;
	void Lines2Sprites();
	bool VerifyIndices();
	template <GSHWDrawConfig::VSExpand Expand> void ExpandIndices();
	void EmulateAtst(GSVector4& FogColor_AREF, u8& atst, const bool pass_2);
	void ConvertSpriteTextureShuffle(bool& write_ba, bool& read_ba);
	GSVector4 RealignTargetTextureCoordinate(const GSTextureCache::Source* tex);
	GSVector4i ComputeBoundingBox(const GSVector2& rtscale, const GSVector2i& rtsize);
	void MergeSprite(GSTextureCache::Source* tex);
	GSVector2 GetTextureScaleFactor() override;
	GSVector2i GetOutputSize(int real_h);
	GSVector2i GetTargetSize(GSVector2i* unscaled_size = nullptr);

	void Reset(bool hardware_reset) override;
	void UpdateSettings(const Pcsx2Config::GSOptions& old_config) override;
	void VSync(u32 field, bool registers_written) override;

	GSTexture* GetOutput(int i, int& y_offset) override;
	GSTexture* GetFeedbackOutput() override;
	void ExpandTarget(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r) override;
	void InvalidateVideoMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r, bool eewrite = false) override;
	void InvalidateLocalMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r, bool clut = false) override;
	void Move() override;
	void Draw() override;

	void PurgeTextureCache() override;

	// Called by the texture cache to know if current texture is useful
	bool UpdateTexIsFB(GSTextureCache::Target* src, const GIFRegTEX0& TEX0);

	// Called by the texture cache when optimizing the copy range for sources
	bool IsPossibleTextureShuffle(GSTextureCache::Target* dst, const GIFRegTEX0& TEX0) const;
};
