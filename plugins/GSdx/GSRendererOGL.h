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
	private:
		GSVector2 m_pixelcenter;
		int  m_accurate_blend;
		bool m_accurate_date;
		bool m_accurate_colclip;
		bool m_accurate_fbmask;

		bool UserHacks_AlphaHack;
		bool UserHacks_AlphaStencil;
		unsigned int UserHacks_TCOffset;
		float UserHacks_TCO_x, UserHacks_TCO_y;

	protected:
		void EmulateGS();
		void SetupIA();

	public:
		GSRendererOGL();
		virtual ~GSRendererOGL() {};

		bool CreateDevice(GSDevice* dev);

		void DrawPrims(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* tex);

		bool PrimitiveOverlap();

		void SendDraw(bool require_barrier);
};
