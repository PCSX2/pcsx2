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

#include "GS/Renderers/SW/GSTextureCacheSW.h"
#include "GS/Renderers/SW/GSRasterizer.h"
#include "GS/GSRingHeap.h"
#include "GS/MultiISA.h"

MULTI_ISA_UNSHARED_START

class GSRendererSW final : public GSRenderer
{
public:
	class SharedData : public GSRasterizerData
	{
		struct alignas(16) TextureLevel
		{
			GSVector4i r;
			GSTextureCacheSW::Texture* t;
		};

	public:
		GSOffset::PageLooper m_fb_pages;
		GSOffset::PageLooper m_zb_pages;
		int m_fpsm;
		int m_zpsm;
		bool m_using_pages;
		TextureLevel m_tex[7 + 1]; // NULL terminated
		enum
		{
			SyncNone,
			SyncSource,
			SyncTarget
		} m_syncpoint;

	public:
		SharedData();
		virtual ~SharedData();

		void UsePages(const GSOffset::PageLooper* fb_pages, int fpsm, const GSOffset::PageLooper* zb_pages, int zpsm);
		void ReleasePages();

		void SetSource(GSTextureCacheSW::Texture* t, const GSVector4i& r, int level);
		void UpdateSource();
	};

protected:
	std::unique_ptr<IRasterizer> m_rl;
	std::unique_ptr<GSTextureCacheSW> m_tc;
	GSRingHeap m_vertex_heap;
	std::array<GSTexture*, 3> m_texture = {};
	u8* m_output;
	GSPixelOffset4* m_fzb;
	GSVector4i m_fzb_bbox;
	u32 m_fzb_cur_pages[16];
	std::atomic<u32> m_fzb_pages[512]; // u16 frame/zbuf pages interleaved
	std::atomic<u16> m_tex_pages[512];
	GIFRegDIMX m_last_dimx = {};
	GSVector4i m_dimx[8] = {};

	void Reset(bool hardware_reset) override;
	void VSync(u32 field, bool registers_written, bool idle_frame) override;
	GSTexture* GetOutput(int i, float& scale, int& y_offset) override;
	GSTexture* GetFeedbackOutput(float& scale) override;

	void Draw() override;
	void Queue(GSRingHeap::SharedPtr<GSRasterizerData>& item);
	void Sync(int reason);
	void InvalidateVideoMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r) override;
	void InvalidateLocalMem(const GIFRegBITBLTBUF& BITBLTBUF, const GSVector4i& r, bool clut = false) override;

	void UsePages(const GSOffset::PageLooper& pages, const int type);
	void ReleasePages(const GSOffset::PageLooper& pages, const int type);

	bool CheckTargetPages(const GSOffset::PageLooper* fb_pages, const GSOffset::PageLooper* zb_pages, const GSVector4i& r);
	bool CheckSourcePages(SharedData* sd);

	bool GetScanlineGlobalData(SharedData* data);

public:
	GSRendererSW(int threads);
	~GSRendererSW() override;

	__fi static GSRendererSW* GetInstance() { return static_cast<GSRendererSW*>(g_gs_renderer.get()); }

	void Destroy() override;
};

MULTI_ISA_UNSHARED_END
