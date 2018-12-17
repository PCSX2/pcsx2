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
}

bool GSRendererDX11::CreateDevice(GSDevice* dev)
{
	if (!__super::CreateDevice(dev))
		return false;

	return true;
}

void GSRendererDX11::EmulateTextureShuffleAndFbmask()
{
	size_t count = m_vertex.next;
	GSVertex* v = &m_vertex.buff[0];

	// Note: D3D1011 is limited and can't read the current framebuffer so we can't have PS_FBMASK and PS_WRITE_RG shaders ported and working.
	if (m_texture_shuffle)
	{
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
		if (PRIM->FST)
		{

			for(size_t i = 0; i < count; i += 2)
			{
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
		}
		else
		{
			const float offset_8pix = 8.0f / tw;

			for(size_t i = 0; i < count; i += 2)
			{
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
		m_om_bsel.wrgba = 0;

		// 2 Select the new mask (Please someone put SSE here)
		if (rg_mask != 0xFF)
		{
			if (write_ba)
			{
				m_om_bsel.wb = 1;
			}
			else
			{
				m_om_bsel.wr = 1;
			}
		}
		else if ((fbmask & 0xFF) != 0xFF)
		{
#ifdef _DEBUG
			fprintf(stderr, "Please fix me! wb %u wr %u\n", m_om_bsel.wb, m_om_bsel.wr);
#endif
			//ASSERT(0);
		}

		if (ba_mask != 0xFF)
		{
			if (write_ba)
			{
				m_om_bsel.wa = 1;
			}
			else
			{
				m_om_bsel.wg = 1;
			}
		}
		else if ((fbmask & 0xFF) != 0xFF)
		{
#ifdef _DEBUG
			fprintf(stderr, "Please fix me! wa %u wg %u\n", m_om_bsel.wa, m_om_bsel.wg);
#endif
			//ASSERT(0);
		}
	}
	else
	{
		m_ps_sel.dfmt = GSLocalMemory::m_psm[m_context->FRAME.PSM].fmt;

		m_om_bsel.wrgba = ~GSVector4i::load((int)m_context->FRAME.FBMSK).eq8(GSVector4i::xffffffff()).mask();
	}
}

void GSRendererDX11::EmulateChannelShuffle(GSTexture** rt, const GSTextureCache::Source* tex)
{
	// Uncomment to disable HLE emulation (allow to trace the draw call)
	// m_channel_shuffle = false;

	// First let's check we really have a channel shuffle effect
	if (m_channel_shuffle)
	{
		if (m_game.title == CRC::GT4 || m_game.title == CRC::GT3 || m_game.title == CRC::GTConcept || m_game.title == CRC::TouristTrophy)
		{
			// fprintf(stderr, "Gran Turismo RGB Channel\n");
			m_ps_sel.channel = ChannelFetch_RGB;
			m_context->TEX0.TFX = TFX_DECAL;
			*rt = tex->m_from_target;
		}
		else if (m_game.title == CRC::Tekken5)
		{
			if (m_context->FRAME.FBW == 1)
			{
				// Used in stages: Secret Garden, Acid Rain, Moonlit Wilderness
				// fprintf(stderr, "Tekken5 RGB Channel\n");
				m_ps_sel.channel = ChannelFetch_RGB;
				m_context->FRAME.FBMSK = 0xFF000000;
				// 12 pages: 2 calls by channel, 3 channels, 1 blit
				// Minus current draw call
				m_skip = 12 * (3 + 3 + 1) - 1;
				*rt = tex->m_from_target;
			}
			else
			{
				// Could skip model drawing if wrongly detected
				m_channel_shuffle = false;
			}
		}
		else if ((tex->m_texture->GetType() == GSTexture::DepthStencil) && !(tex->m_32_bits_fmt))
		{
			// So far 2 games hit this code path. Urban Chaos and Tales of Abyss
			// UC: will copy depth to green channel
			// ToA: will copy depth to alpha channel
			if ((m_context->FRAME.FBMSK & 0xFF0000) == 0xFF0000)
			{
				// Green channel is masked
				// fprintf(stderr, "Tales Of Abyss Crazyness (MSB 16b depth to Alpha)\n");
				m_ps_sel.tales_of_abyss_hle = 1;
			}
			else
			{
				// fprintf(stderr, "Urban Chaos Crazyness (Green extraction)\n");
				m_ps_sel.urban_chaos_hle = 1;
			}
		}
		else if (m_index.tail <= 64 && m_context->CLAMP.WMT == 3)
		{
			// Blood will tell. I think it is channel effect too but again
			// implemented in a different way. I don't want to add more CRC stuff. So
			// let's disable channel when the signature is different.
			//
			// Note: Tales Of Abyss and Tekken5 could hit this path too. Those games are
			// handled above.
			// fprintf(stderr, "Maybe not a channel!\n");
			m_channel_shuffle = false;
		}
		else if (m_context->CLAMP.WMS == 3 && ((m_context->CLAMP.MAXU & 0x8) == 8))
		{
			// Read either blue or Alpha. Let's go for Blue ;)
			// MGS3/Kill Zone
			// fprintf(stderr, "Blue channel\n");
			m_ps_sel.channel = ChannelFetch_BLUE;
		}
		else if (m_context->CLAMP.WMS == 3 && ((m_context->CLAMP.MINU & 0x8) == 0))
		{
			// Read either Red or Green. Let's check the V coordinate. 0-1 is likely top so
			// red. 2-3 is likely bottom so green (actually depends on texture base pointer offset)
			bool green = PRIM->FST && (m_vertex.buff[0].V & 32);
			if (green && (m_context->FRAME.FBMSK & 0x00FFFFFF) == 0x00FFFFFF)
			{
				// Typically used in Terminator 3
				int blue_mask = m_context->FRAME.FBMSK >> 24;
				int green_mask = ~blue_mask & 0xFF;
				int blue_shift = -1;

				// Note: potentially we could also check the value of the clut
				switch (m_context->FRAME.FBMSK >> 24)
				{
					case 0xFF: ASSERT(0);      break;
					case 0xFE: blue_shift = 1; break;
					case 0xFC: blue_shift = 2; break;
					case 0xF8: blue_shift = 3; break;
					case 0xF0: blue_shift = 4; break;
					case 0xE0: blue_shift = 5; break;
					case 0xC0: blue_shift = 6; break;
					case 0x80: blue_shift = 7; break;
					default:   ASSERT(0);      break;
				}

				int green_shift = 8 - blue_shift;
				ps_cb.ChannelShuffle = GSVector4i(blue_mask, blue_shift, green_mask, green_shift);

				if (blue_shift >= 0)
				{
					// fprintf(stderr, "Green/Blue channel (%d, %d)\n", blue_shift, green_shift);
					m_ps_sel.channel = ChannelFetch_GXBY;
					m_context->FRAME.FBMSK = 0x00FFFFFF;
				}
				else
				{
					// fprintf(stderr, "Green channel (wrong mask) (fbmask %x)\n", m_context->FRAME.FBMSK >> 24);
					m_ps_sel.channel = ChannelFetch_GREEN;
				}

			}
			else if (green)
			{
				// fprintf(stderr, "Green channel\n");
				m_ps_sel.channel = ChannelFetch_GREEN;
			}
			else
			{
				// Pop
				// fprintf(stderr, "Red channel\n");
				m_ps_sel.channel = ChannelFetch_RED;
			}
		}
		else
		{
			// fprintf(stderr, "Channel not supported\n");
			m_channel_shuffle = false;
		}
	}

	// Effect is really a channel shuffle effect so let's cheat a little
	if (m_channel_shuffle)
	{
		// FIXME: Slot 4 - unbind texture when it isn't used.
		dev->PSSetShaderResource(4, tex->m_from_target);
		// Replace current draw with a fullscreen sprite
		//
		// Performance GPU note: it could be wise to reduce the size to
		// the rendered size of the framebuffer

		GSVertex* s = &m_vertex.buff[0];
		s[0].XYZ.X = (uint16)(m_context->XYOFFSET.OFX + 0);
		s[1].XYZ.X = (uint16)(m_context->XYOFFSET.OFX + 16384);
		s[0].XYZ.Y = (uint16)(m_context->XYOFFSET.OFY + 0);
		s[1].XYZ.Y = (uint16)(m_context->XYOFFSET.OFY + 16384);

		m_vertex.head = m_vertex.tail = m_vertex.next = 2;
		m_index.tail = 2;
	}
	else
	{
#ifdef _DEBUG
		dev->PSSetShaderResource(4, NULL);
#endif
	}
}

void GSRendererDX11::SetupIA(const float& sx, const float& sy)
{
	GSDevice11* dev = (GSDevice11*)m_dev;

	D3D11_PRIMITIVE_TOPOLOGY t;

	bool unscale_pt_ln = (GetUpscaleMultiplier() != 1);

	switch (m_vt.m_primclass)
	{
	case GS_POINT_CLASS:
		if (unscale_pt_ln)
		{
			m_gs_sel.point = 1;
			gs_cb.PointSize = GSVector2(16.0f * sx, 16.0f * sy);
		}

		t = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
		break;
	case GS_LINE_CLASS:
		if (unscale_pt_ln)
		{
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

	if (dev->IAMapVertexBuffer(&ptr, sizeof(GSVertex), m_vertex.next))
	{
		GSVector4i::storent(ptr, m_vertex.buff, sizeof(GSVertex) * m_vertex.next);
		
		if (UserHacks_WildHack && !isPackedUV_HackFlag)
		{
			GSVertex* RESTRICT d = (GSVertex*)ptr;
		
			for(unsigned int i = 0; i < m_vertex.next; i++)
			{
				if (PRIM->TME && PRIM->FST) d[i].UV &= 0x3FEF3FEF;
			}
		}
		
		dev->IAUnmapVertexBuffer();
	}

	dev->IASetIndexBuffer(m_index.buff, m_index.tail);
	dev->IASetPrimitiveTopology(t);
}
