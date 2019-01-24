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

#pragma once

#include "Renderers/HW/GSRendererHW.h"
#include "GSTextureCache11.h"
#include "Renderers/HW/GSVertexHW.h"

class GSRendererDX11 final : public GSRendererHW
{
private:
	bool UserHacks_AlphaHack;
	bool UserHacks_AlphaStencil;

private:
	inline void ResetStates();
	inline void SetupIA(const float& sx, const float& sy);
	inline void EmulateAtst(const int pass, const GSTextureCache::Source* tex);
	inline void EmulateZbuffer();
	inline void EmulateTextureShuffleAndFbmask();
	inline void EmulateChannelShuffle(GSTexture** rt, const GSTextureCache::Source* tex);
	inline void EmulateTextureSampler(const GSTextureCache::Source* tex);

	GSDeviceDX::VSSelector m_vs_sel;
	GSDeviceDX::GSSelector m_gs_sel;
	GSDeviceDX::PSSelector m_ps_sel;

	GSDeviceDX::PSSamplerSelector      m_ps_ssel;
	GSDeviceDX::OMBlendSelector        m_om_bsel;
	GSDeviceDX::OMDepthStencilSelector m_om_dssel;

	GSDeviceDX::PSConstantBuffer ps_cb;
	GSDeviceDX::VSConstantBuffer vs_cb;
	GSDeviceDX::GSConstantBuffer gs_cb;

public:
	GSRendererDX11();
	virtual ~GSRendererDX11() {}

	void DrawPrims(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* tex) final;

	bool CreateDevice(GSDevice* dev);
};
