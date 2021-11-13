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

#include "GSDeviceOGL.h"
#include "GS/Renderers/HW/GSRendererHW.h"
#include "GS/Renderers/HW/GSVertexHW.h"

class GSRendererOGL final : public GSRendererHW
{
	enum PRIM_OVERLAP
	{
		PRIM_OVERLAP_UNKNOW,
		PRIM_OVERLAP_YES,
		PRIM_OVERLAP_NO
	};

	enum ACC_BLEND
	{
		ACC_BLEND_NONE   = 0,
		ACC_BLEND_BASIC  = 1,
		ACC_BLEND_MEDIUM = 2,
		ACC_BLEND_HIGH   = 3,
		ACC_BLEND_FULL   = 4,
		ACC_BLEND_ULTRA  = 5
	};

private:
	PRIM_OVERLAP m_prim_overlap;
	std::vector<size_t> m_drawlist;

	TriFiltering UserHacks_tri_filter;

	GSDeviceOGL::VSConstantBuffer vs_cb;
	GSDeviceOGL::PSConstantBuffer ps_cb;

	bool m_require_one_barrier;
	bool m_require_full_barrier;

	GSDeviceOGL::VSSelector m_vs_sel;
	GSDeviceOGL::GSSelector m_gs_sel;
	GSDeviceOGL::PSSelector m_ps_sel;

	GSDeviceOGL::PSSamplerSelector      m_ps_ssel;
	GSDeviceOGL::OMColorMaskSelector    m_om_csel;
	GSDeviceOGL::OMDepthStencilSelector m_om_dssel;

private:
	inline void ResetStates();
	inline void SetupIA(const float& sx, const float& sy);
	inline void EmulateTextureShuffleAndFbmask();
	inline void EmulateChannelShuffle(GSTexture** rt, const GSTextureCache::Source* tex);
	inline void EmulateBlending(bool& DATE_GL42, bool& DATE_GL45);
	inline void EmulateTextureSampler(const GSTextureCache::Source* tex);
	inline void EmulateZbuffer();

public:
	GSRendererOGL();
	virtual ~GSRendererOGL() {}

	void DrawPrims(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* tex) final;

	PRIM_OVERLAP PrimitiveOverlap();

	void SendDraw();

	bool IsDummyTexture() const final;
};
