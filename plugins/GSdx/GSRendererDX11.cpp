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
