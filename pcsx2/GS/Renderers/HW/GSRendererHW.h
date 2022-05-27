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
#include "GS/GSState.h"

class GSRendererHW : public GSRenderer
{
public:
	static constexpr int MAX_FRAMEBUFFER_HEIGHT = 1280;

private:
	int m_width;
	int m_height;

	static constexpr float SSR_UV_TOLERANCE = 1.0f;

#pragma region hacks

	typedef bool (GSRendererHW::*OI_Ptr)(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	typedef void (GSRendererHW::*OO_Ptr)();
	typedef bool (GSRendererHW::*CU_Ptr)();

	// Require special argument
	bool OI_BlitFMV(GSTextureCache::Target* _rt, GSTextureCache::Source* t, const GSVector4i& r_draw);
	void OI_GsMemClear(); // always on
	void OI_DoubleHalfClear(GSTexture* rt, GSTexture* ds); // always on

	bool OI_BigMuthaTruckers(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_DBZBTGames(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_FFXII(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_FFX(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_MetalSlug6(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_RozenMaidenGebetGarden(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_SonicUnleashed(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_PointListPalette(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
	bool OI_SuperManReturns(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* t);
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
		std::list<HackEntry<CU_Ptr>> m_cu_list;

		FunctionMap<OI_Ptr> m_oi_map;
		FunctionMap<OO_Ptr> m_oo_map;
		FunctionMap<CU_Ptr> m_cu_map;

	public:
		OI_Ptr m_oi;
		OO_Ptr m_oo;
		CU_Ptr m_cu;

		Hacks();

		void SetGameCRC(const CRC::Game& game);

	} m_hacks;

#pragma endregion

	u16 Interpolate_UV(float alpha, int t0, int t1);
	float alpha0(int L, int X0, int X1);
	float alpha1(int L, int X0, int X1);
	void SwSpriteRender();
	bool CanUseSwSpriteRender();

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
	bool m_channel_shuffle;
	bool m_userhacks_tcoffset;
	float m_userhacks_tcoffset_x;
	float m_userhacks_tcoffset_y;

	GSVector2i m_lod; // Min & Max level of detail

	GSHWDrawConfig m_conf;

public:
	GSRendererHW();
	virtual ~GSRendererHW() override;

	__fi static GSRendererHW* GetInstance() { return static_cast<GSRendererHW*>(g_gs_renderer.get()); }
	__fi GSTextureCache* GetTextureCache() const { return m_tc; }

	void Destroy() override;

	void SetGameCRC(u32 crc, int options) override;
	bool CanUpscale() override;
	int GetUpscaleMultiplier() override;
	void SetScaling();
	void Lines2Sprites();
	void EmulateAtst(GSVector4& FogColor_AREF, u8& atst, const bool pass_2);
	void ConvertSpriteTextureShuffle(bool& write_ba, bool& read_ba);
	GSVector4 RealignTargetTextureCoordinate(const GSTextureCache::Source* tex);
	GSVector4i ComputeBoundingBox(const GSVector2& rtscale, const GSVector2i& rtsize);
	void MergeSprite(GSTextureCache::Source* tex);
	GSVector2 GetTextureScaleFactor(const bool force_upscaling);
	GSVector2 GetTextureScaleFactor() override;
	GSVector2i GetTargetSize();

	void Reset(bool hardware_reset) override;
	void UpdateSettings(const Pcsx2Config::GSOptions& old_config) override;
	void VSync(u32 field, bool registers_written) override;

	GSTexture* GetOutput(int i, int& y_offset) override;
	GSTexture* GetFeedbackOutput() override;
	void InvalidateVideoMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r) override;
	void InvalidateLocalMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r, bool clut = false) override;
	void Move() override;
	void Draw() override;

	void PurgeTextureCache() override;

	// Called by the texture cache to know if current texture is useful
	bool IsDummyTexture() const;

	// Called by the texture cache when optimizing the copy range for sources
	bool IsPossibleTextureShuffle(GSTextureCache::Source* src) const;
};
