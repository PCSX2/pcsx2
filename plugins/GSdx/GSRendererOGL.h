/*
 *	Copyright (C) 2011-2011 Gregory hainaut
 *	Copyright (C) 2007-2009 Gabest
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

#include "GSRendererHW.h"

#include "GSRenderer.h"
#include "GSTextureCacheOGL.h"
#include "GSVertexHW.h"

class GSRendererOGL : public GSRendererHW
{
	enum PRIM_OVERLAP {
		PRIM_OVERLAP_UNKNOW,
		PRIM_OVERLAP_YES,
		PRIM_OVERLAP_NO
	};

	enum ACC_BLEND {
		ACC_BLEND_NONE = 0,
		ACC_BLEND_FREE = 1,
		ACC_BLEND_SPRITE = 2,
		ACC_BLEND_CCLIP_DALPHA = 3,
		ACC_BLEND_FULL = 4,
		ACC_BLEND_ULTRA = 5
	};

	private:
		bool m_accurate_date;
		int m_sw_blending;
		PRIM_OVERLAP m_prim_overlap;
		bool m_unsafe_fbmask;
		vector<size_t> m_drawlist;

		unsigned int UserHacks_TCOffset;
		float UserHacks_TCO_x, UserHacks_TCO_y;
		bool UserHacks_safe_fbmask;

		GSDeviceOGL::VSConstantBuffer vs_cb;
		GSDeviceOGL::PSConstantBuffer ps_cb;

		GSVector4i ComputeBoundingBox(const GSVector2& rtscale, const GSVector2i& rtsize);

	protected:
		void EmulateGS();
		void SetupIA();
		bool EmulateTextureShuffleAndFbmask(GSDeviceOGL::PSSelector& ps_sel, GSDeviceOGL::OMColorMaskSelector& om_csel);
		bool EmulateBlending(GSDeviceOGL::PSSelector& ps_sel, bool DATE_GL42);

	public:
		GSRendererOGL();
		virtual ~GSRendererOGL() {};

		bool CreateDevice(GSDevice* dev);

		void DrawPrims(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* tex);

		PRIM_OVERLAP PrimitiveOverlap();

		void SendDraw(bool require_barrier);
};
