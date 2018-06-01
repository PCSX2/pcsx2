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

#include "stdafx.h"
#include "GSRendererDX11.h"
#include "GSCrc.h"
#include "resource.h"

GSRendererDX11::GSRendererDX11()
	: GSRendererDX(new GSTextureCache11(this), GSVector2(-0.5f))
{
	if (theApp.GetConfigB("UserHacks")) {
		UserHacks_unscale_pt_ln = theApp.GetConfigB("UserHacks_unscale_point_line");
	} else {
		UserHacks_unscale_pt_ln = false;
	}
}

bool GSRendererDX11::CreateDevice(GSDevice* dev)
{
	if(!__super::CreateDevice(dev))
		return false;

	return true;
}

void GSRendererDX11::EmulateTextureShuffleAndFbmask()
{
	size_t count = m_vertex.next;
	GSVertex* v = &m_vertex.buff[0];

	// Note: D3D1011 is limited and can't read the current framebuffer so we can't have PS_FBMASK and PS_WRITE_RG shaders ported and working.
	if (m_texture_shuffle) {
		m_ps_sel.shuffle = 1;
		m_ps_sel.dfmt = 0;

		const GIFRegXYOFFSET& o = m_context->XYOFFSET;

		// vertex position is 8 to 16 pixels, therefore it is the 16-31 bits of the colors
		int  pos = (v[0].XYZ.X - o.OFX) & 0xFF;
		bool write_ba = (pos > 112 && pos < 136);
		// Read texture is 8 to 16 pixels (same as above)
		float tw = (float)(1u << m_context->TEX0.TW);
		int tex_pos = (PRIM->FST) ? v[0].U : (int)(tw * v[0].ST.S);
		tex_pos &= 0xFF;
		m_ps_sel.read_ba = (tex_pos > 112 && tex_pos < 144);

		// Convert the vertex info to a 32 bits color format equivalent
		if (PRIM->FST) {

			for(size_t i = 0; i < count; i += 2) {
				if (write_ba)
					v[i].XYZ.X   -= 128u;
				else
					v[i+1].XYZ.X += 128u;

				if (m_ps_sel.read_ba)
					v[i].U       -= 128u;
				else
					v[i+1].U     += 128u;

				// Height is too big (2x).
				int tex_offset = v[i].V & 0xF;
				GSVector4i offset(o.OFY, tex_offset, o.OFY, tex_offset);

				GSVector4i tmp(v[i].XYZ.Y, v[i].V, v[i+1].XYZ.Y, v[i+1].V);
				tmp = GSVector4i(tmp - offset).srl32(1) + offset;

				v[i].XYZ.Y   = (uint16)tmp.x;
				v[i].V       = (uint16)tmp.y;
				v[i+1].XYZ.Y = (uint16)tmp.z;
				v[i+1].V     = (uint16)tmp.w;
			}
		} else {
			const float offset_8pix = 8.0f / tw;

			for(size_t i = 0; i < count; i += 2) {
				if (write_ba)
					v[i].XYZ.X   -= 128u;
				else
					v[i+1].XYZ.X += 128u;

				if (m_ps_sel.read_ba)
					v[i].ST.S    -= offset_8pix;
				else
					v[i+1].ST.S  += offset_8pix;

				// Height is too big (2x).
				GSVector4i offset(o.OFY, o.OFY);

				GSVector4i tmp(v[i].XYZ.Y, v[i+1].XYZ.Y);
				tmp = GSVector4i(tmp - offset).srl32(1) + offset;

				//fprintf(stderr, "Before %d, After %d\n", v[i+1].XYZ.Y, tmp.y);
				v[i].XYZ.Y   = (uint16)tmp.x;
				v[i].ST.T   /= 2.0f;
				v[i+1].XYZ.Y = (uint16)tmp.y;
				v[i+1].ST.T /= 2.0f;
			}
		}

		// Please bang my head against the wall!
		// 1/ Reduce the frame mask to a 16 bit format
		const uint32& m = m_context->FRAME.FBMSK;
		uint32 fbmask = ((m >> 3) & 0x1F) | ((m >> 6) & 0x3E0) | ((m >> 9) & 0x7C00) | ((m >> 16) & 0x8000);
		// FIXME GSVector will be nice here
		uint8 rg_mask = fbmask & 0xFF;
		uint8 ba_mask = (fbmask >> 8) & 0xFF;
		om_bsel.wrgba = 0;

		// 2 Select the new mask (Please someone put SSE here)
		if (rg_mask != 0xFF) {
			if (write_ba) {
				om_bsel.wb = 1;
			} else {
				om_bsel.wr = 1;
			}
		} else if ((fbmask & 0xFF) != 0xFF) {
#ifdef _DEBUG
			fprintf(stderr, "Please fix me! wb %u wr %u\n", om_bsel.wb, om_bsel.wr);
#endif
			//ASSERT(0);
		}

		if (ba_mask != 0xFF) {
			if (write_ba) {
				om_bsel.wa = 1;
			} else {
				om_bsel.wg = 1;
			}
		} else if ((fbmask & 0xFF) != 0xFF) {
#ifdef _DEBUG
			fprintf(stderr, "Please fix me! wa %u wg %u\n", om_bsel.wa, om_bsel.wg);
#endif
			//ASSERT(0);
		}

	} else {
		m_ps_sel.dfmt = GSLocalMemory::m_psm[m_context->FRAME.PSM].fmt;

		om_bsel.wrgba = ~GSVector4i::load((int)m_context->FRAME.FBMSK).eq8(GSVector4i::xffffffff()).mask();
	}
}

void GSRendererDX11::SetupIA(const float& sx, const float& sy)
{
	GSDevice11* dev = (GSDevice11*)m_dev;

	D3D11_PRIMITIVE_TOPOLOGY t;

	bool unscale_hack = UserHacks_unscale_pt_ln && (GetUpscaleMultiplier() != 1);

	switch (m_vt.m_primclass)
	{
	case GS_POINT_CLASS:
		if (unscale_hack) {
			m_gs_sel.point = 1;
			gs_cb.PointSize = GSVector2(16.0f * sx, 16.0f * sy);
		}

		t = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
		break;
	case GS_LINE_CLASS:
		if (unscale_hack) {
			m_gs_sel.line = 1;
			gs_cb.PointSize = GSVector2(16.0f * sx, 16.0f * sy);
		}

		t = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;

		break;
	case GS_SPRITE_CLASS:
		t = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
		break;
	case GS_TRIANGLE_CLASS:

		t = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		break;
	default:
		__assume(0);
	}

	void* ptr = NULL;

	if(dev->IAMapVertexBuffer(&ptr, sizeof(GSVertex), m_vertex.next))
	{
		GSVector4i::storent(ptr, m_vertex.buff, sizeof(GSVertex) * m_vertex.next);
		
		if(UserHacks_WildHack && !isPackedUV_HackFlag)
		{
			GSVertex* RESTRICT d = (GSVertex*)ptr;
		
			for(unsigned int i = 0; i < m_vertex.next; i++)
			{
				if(PRIM->TME && PRIM->FST) d[i].UV &= 0x3FEF3FEF;
			}
		}
		
		dev->IAUnmapVertexBuffer();
	}

	dev->IASetIndexBuffer(m_index.buff, m_index.tail);
	dev->IASetPrimitiveTopology(t);
}
