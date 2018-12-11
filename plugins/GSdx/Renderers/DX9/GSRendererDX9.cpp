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
#include "GSRendererDX9.h"
#include "GSCrc.h"
#include "resource.h"

GSRendererDX9::GSRendererDX9()
	: GSRendererDX(new GSTextureCache9(this))
{
}

bool GSRendererDX9::CreateDevice(GSDevice* dev)
{
	if(!__super::CreateDevice(dev))
		return false;

	//

	memset(&m_fba.dss, 0, sizeof(m_fba.dss));

	m_fba.dss.StencilEnable = true;
	m_fba.dss.StencilReadMask = 2;
	m_fba.dss.StencilWriteMask = 2;
	m_fba.dss.StencilFunc = D3DCMP_EQUAL;
	m_fba.dss.StencilPassOp = D3DSTENCILOP_ZERO;
	m_fba.dss.StencilFailOp = D3DSTENCILOP_ZERO;
	m_fba.dss.StencilDepthFailOp = D3DSTENCILOP_ZERO;
	m_fba.dss.StencilRef = 2;

	memset(&m_fba.bs, 0, sizeof(m_fba.bs));

	m_fba.bs.RenderTargetWriteMask = D3DCOLORWRITEENABLE_ALPHA;

	//

	return true;
}

void GSRendererDX9::EmulateChannelShuffle(GSTexture** rt, const GSTextureCache::Source* tex)
{
	// Channel shuffle will never be supported on Direct3D9 through shaders so just
	// use code that skips the bad draw calls.
	if (m_channel_shuffle)
	{
		if (m_game.title == CRC::Tekken5)
		{
			if (m_context->FRAME.FBW == 1)
			{
				// Used in stages: Secret Garden, Acid Rain, Moonlit Wilderness
				// 12 pages: 2 calls by channel, 3 channels, 1 blit
				// Minus current draw call
				m_skip = 12 * (3 + 3 + 1) - 1;
			}
			else
			{
				// Could skip model drawing if wrongly detected
				m_channel_shuffle = false;
			}
		}
		else if ((tex->m_texture->GetType() == GSTexture::DepthStencil) && !(tex->m_32_bits_fmt))
		{
			// So far 2 games hit this code path. Urban Chaos and Tales of Abyss.
			throw GSDXRecoverableError();
		}
		else if (m_index.tail <= 64 && m_context->CLAMP.WMT == 3)
		{
			// Blood will tell. I think it is channel effect too but again
			// implemented in a different way. I don't want to add more CRC stuff. So
			// let's disable channel when the signature is different.
			//
			// Note: Tales Of Abyss and Tekken5 could hit this path too. Those games are
			// handled above.
			m_channel_shuffle = false;
		}
		else if (m_context->CLAMP.WMS == 3 && ((m_context->CLAMP.MAXU & 0x8) == 8))
		{
			// Read either blue or Alpha.
			// MGS3/Kill Zone
			throw GSDXRecoverableError();
		}
		else if (m_context->CLAMP.WMS == 3 && ((m_context->CLAMP.MINU & 0x8) == 0))
		{
			// Read either Red or Green.
			// Terminator 3
			throw GSDXRecoverableError();
		}
		else
		{
			m_channel_shuffle = false;
		}
	}
}

void GSRendererDX9::EmulateTextureShuffleAndFbmask()
{
	if (m_texture_shuffle)
	{
		// Texture shuffle is not supported so make sure nothing is written on all channels.
		m_om_bsel.wrgba = 0;
	}
	else
	{
		m_ps_sel.dfmt = GSLocalMemory::m_psm[m_context->FRAME.PSM].fmt;

		m_om_bsel.wrgba = ~GSVector4i::load((int)m_context->FRAME.FBMSK).eq8(GSVector4i::xffffffff()).mask();
	}
}

void GSRendererDX9::SetupIA(const float& sx, const float& sy)
{
	D3DPRIMITIVETYPE topology;

	switch(m_vt.m_primclass)
	{
	case GS_POINT_CLASS:

		topology = D3DPT_POINTLIST;

		break;

	case GS_LINE_CLASS:

		topology = D3DPT_LINELIST;

		if(PRIM->IIP == 0)
		{
			for(size_t i = 0, j = m_index.tail; i < j; i += 2) 
			{
				uint32 tmp = m_index.buff[i + 0]; 
				m_index.buff[i + 0] = m_index.buff[i + 1];
				m_index.buff[i + 1] = tmp;
			}
		}

		break;

	case GS_TRIANGLE_CLASS:

		topology = D3DPT_TRIANGLELIST;

		if(PRIM->IIP == 0)
		{
			for(size_t i = 0, j = m_index.tail; i < j; i += 3) 
			{
				uint32 tmp = m_index.buff[i + 0]; 
				m_index.buff[i + 0] = m_index.buff[i + 2];
				m_index.buff[i + 2] = tmp;
			}
		}

		break;

	case GS_SPRITE_CLASS:

		topology = D3DPT_TRIANGLELIST;

		// each sprite converted to quad needs twice the space

		Lines2Sprites();

		break;

	default:
		__assume(0);
	}

	GSDevice9* dev = (GSDevice9*)m_dev;

	(*dev)->SetRenderState(D3DRS_SHADEMODE, PRIM->IIP ? D3DSHADE_GOURAUD : D3DSHADE_FLAT); // TODO

	void* ptr = NULL;

	if(dev->IAMapVertexBuffer(&ptr, sizeof(GSVertexHW9), m_vertex.next))
	{
		GSVertex* RESTRICT s = (GSVertex*)m_vertex.buff;
		GSVertexHW9* RESTRICT d = (GSVertexHW9*)ptr;

		for(uint32 i = 0; i < m_vertex.next; i++, s++, d++)
		{
			GSVector4 p = GSVector4(GSVector4i::load(s->XYZ.u32[0]).upl16());

			if(PRIM->TME && !PRIM->FST)
			{
				p = p.xyxy(GSVector4((float)s->XYZ.Z, s->RGBAQ.Q));
			}
			else
			{
				p = p.xyxy(GSVector4::load((float)s->XYZ.Z));
			}

			GSVector4 t = GSVector4::zero();

			if(PRIM->TME)
			{
				if(PRIM->FST)
				{
					if(UserHacks_WildHack && !isPackedUV_HackFlag)
					{
						t = GSVector4(GSVector4i::load(s->UV & 0x3FEF3FEF).upl16());
						//printf("GSDX: %08X | D3D9(%d) %s\n", s->UV & 0x3FEF3FEF, m_vertex.next, i == 0 ? "*" : "");
					}
					else
					{
						t = GSVector4(GSVector4i::load(s->UV).upl16());
					}
				}
				else
				{
					t = GSVector4::loadl(&s->ST);
				}
			}

			t = t.xyxy(GSVector4::cast(GSVector4i(s->RGBAQ.u32[0], s->FOG)));

			d->p = p;
			d->t = t;
		}

		dev->IAUnmapVertexBuffer();
	}

	dev->IASetIndexBuffer(m_index.buff, m_index.tail);

	dev->IASetPrimitiveTopology(topology);
}

void GSRendererDX9::UpdateFBA(GSTexture* rt)
{
	if (!rt)
		return;

	GSDevice9* dev = (GSDevice9*)m_dev;

	dev->BeginScene();

	// om

	dev->OMSetDepthStencilState(&m_fba.dss);
	dev->OMSetBlendState(&m_fba.bs, 0);

	// ia

	GSVector4 s = GSVector4(rt->GetScale().x / rt->GetWidth(), rt->GetScale().y / rt->GetHeight());
	GSVector4 off = GSVector4(-1.0f, 1.0f);

	GSVector4 src = ((m_vt.m_min.p.xyxy(m_vt.m_max.p) + off.xxyy()) * s.xyxy()).sat(off.zzyy());
	GSVector4 dst = src * 2.0f + off.xxxx();

	GSVertexPT1 vertices[] =
	{
		{GSVector4(dst.x, -dst.y, 0.5f, 1.0f), GSVector2(0)},
		{GSVector4(dst.z, -dst.y, 0.5f, 1.0f), GSVector2(0)},
		{GSVector4(dst.x, -dst.w, 0.5f, 1.0f), GSVector2(0)},
		{GSVector4(dst.z, -dst.w, 0.5f, 1.0f), GSVector2(0)},
	};

	dev->IASetVertexBuffer(vertices, sizeof(vertices[0]), countof(vertices));
	dev->IASetInputLayout(dev->m_convert.il);
	dev->IASetPrimitiveTopology(D3DPT_TRIANGLESTRIP);

	// vs

	dev->VSSetShader(dev->m_convert.vs, NULL, 0);

	// ps

	dev->PSSetShader(dev->m_convert.ps[4], NULL, 0);

	//

	dev->DrawPrimitive();

	//

	dev->EndScene();
}
