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

#include "PrecompiledHeader.h"
#include "GSRendererDX11.h"

GSRendererDX11::GSRendererDX11()
{
	m_sw_blending = theApp.GetConfigI("accurate_blending_unit_d3d11");

	ResetStates();
}

void GSRendererDX11::SetupIA(const float& sx, const float& sy)
{
	GSDevice11* dev = (GSDevice11*)m_dev;

	D3D11_PRIMITIVE_TOPOLOGY t{};

	const bool unscale_pt_ln = m_userHacks_enabled_unscale_ptln && (GetUpscaleMultiplier() != 1);

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
			// Lines: GPU conversion.
			// Triangles: CPU conversion.
			if (!m_vt.m_accurate_stq && m_vertex.next > 32) // <=> 16 sprites (based on Shadow Hearts)
			{
				t = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
			}
			else
			{
				m_gs_sel.cpu_sprite = 1;
				Lines2Sprites();

				t = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			}

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

		if (m_userhacks_wildhack && !m_isPackedUV_HackFlag)
		{
			GSVertex* RESTRICT d = (GSVertex*)ptr;

			for (unsigned int i = 0; i < m_vertex.next; i++)
			{
				if (PRIM->TME && PRIM->FST)
					d[i].UV &= 0x3FEF3FEF;
			}
		}

		dev->IAUnmapVertexBuffer();
	}

	dev->IASetIndexBuffer(m_index.buff, m_index.tail);
	dev->IASetPrimitiveTopology(t);
}

void GSRendererDX11::EmulateZbuffer()
{
	if (m_context->TEST.ZTE)
	{
		m_om_dssel.ztst = m_context->TEST.ZTST;
		// AA1: Z is not written on lines since coverage is always less than 0x80.
		m_om_dssel.zwe = (m_context->ZBUF.ZMSK || (PRIM->AA1 && m_vt.m_primclass == GS_LINE_CLASS)) ? 0 : 1;
	}
	else
	{
		m_om_dssel.ztst = ZTST_ALWAYS;
	}

	// On the real GS we appear to do clamping on the max z value the format allows.
	// Clamping is done after rasterization.
	const u32 max_z = 0xFFFFFFFF >> (GSLocalMemory::m_psm[m_context->ZBUF.PSM].fmt * 8);
	const bool clamp_z = (u32)(GSVector4i(m_vt.m_max.p).z) > max_z;

	vs_cb.MaxDepth = GSVector2i(0xFFFFFFFF);
	//ps_cb.Af_MaxDepth.y = 1.0f;
	m_ps_sel.zclamp = 0;

	if (clamp_z)
	{
		if (m_vt.m_primclass == GS_SPRITE_CLASS || m_vt.m_primclass == GS_POINT_CLASS)
		{
			vs_cb.MaxDepth = GSVector2i(max_z);
		}
		else if (!m_context->ZBUF.ZMSK)
		{
			ps_cb.Af_MaxDepth.y = max_z * ldexpf(1, -32);
			m_ps_sel.zclamp = 1;
		}
	}

	const GSVertex* v = &m_vertex.buff[0];
	// Minor optimization of a corner case (it allow to better emulate some alpha test effects)
	if (m_om_dssel.ztst == ZTST_GEQUAL && m_vt.m_eq.z && v[0].XYZ.Z == max_z)
	{
#ifdef _DEBUG
		fprintf(stdout, "%d: Optimize Z test GEQUAL to ALWAYS (%s)\n", s_n, psm_str(m_context->ZBUF.PSM));
#endif
		m_om_dssel.ztst = ZTST_ALWAYS;
	}
}

void GSRendererDX11::EmulateTextureShuffleAndFbmask()
{
	// FBmask blend level selection.
	// We do this becaue:
	// 1. D3D sucks.
	// 2. FB copy is slow, especially on triangle primitives which is unplayable with some games.
	// 3. SW blending isn't implemented yet.
	bool enable_fbmask_emulation = false;
	switch (m_sw_blending)
	{
		case ACC_BLEND_HIGH_D3D11:
			// Fully enable Fbmask emulation like on opengl, note misses sw blending to work as opengl on some games (Genji).
			// Debug
			enable_fbmask_emulation = true;
			break;
		case ACC_BLEND_MEDIUM_D3D11:
			// Enable Fbmask emulation excluding triangle class because it is quite slow.
			// Exclude 0x80000000 because Genji needs sw blending, otherwise it breaks some effects.
			enable_fbmask_emulation = ((m_vt.m_primclass != GS_TRIANGLE_CLASS) && (m_context->FRAME.FBMSK != 0x80000000));
			break;
		case ACC_BLEND_BASIC_D3D11:
			// Enable Fbmask emulation excluding triangle class because it is quite slow.
			// Exclude 0x80000000 because Genji needs sw blending, otherwise it breaks some effects.
			// Also exclude fbmask emulation on texture shuffle just in case, it is probably safe tho.
			enable_fbmask_emulation = (!m_texture_shuffle && (m_vt.m_primclass != GS_TRIANGLE_CLASS) && (m_context->FRAME.FBMSK != 0x80000000));
			break;
		case ACC_BLEND_NONE_D3D11:
		default:
			break;
	}


	// Uncomment to disable texture shuffle emulation.
	// m_texture_shuffle = false;

	if (m_texture_shuffle)
	{
		m_ps_sel.shuffle = 1;
		m_ps_sel.dfmt = 0;

		bool write_ba;
		bool read_ba;

		ConvertSpriteTextureShuffle(write_ba, read_ba);

		m_ps_sel.read_ba = read_ba;

		// Please bang my head against the wall!
		// 1/ Reduce the frame mask to a 16 bit format
		const u32& m = m_context->FRAME.FBMSK;
		const u32 fbmask = ((m >> 3) & 0x1F) | ((m >> 6) & 0x3E0) | ((m >> 9) & 0x7C00) | ((m >> 16) & 0x8000);
		// FIXME GSVector will be nice here
		const u8 rg_mask = fbmask & 0xFF;
		const u8 ba_mask = (fbmask >> 8) & 0xFF;
		m_om_bsel.wrgba = 0;

		// 2 Select the new mask (Please someone put SSE here)
		if (rg_mask != 0xFF)
		{
			if (write_ba)
			{
				// fprintf(stderr, "%d: Color shuffle %s => B\n", s_n, read_ba ? "B" : "R");
				m_om_bsel.wb = 1;
			}
			else
			{
				// fprintf(stderr, "%d: Color shuffle %s => R\n", s_n, read_ba ? "B" : "R");
				m_om_bsel.wr = 1;
			}
			if (rg_mask)
				m_ps_sel.fbmask = 1;
		}

		if (ba_mask != 0xFF)
		{
			if (write_ba)
			{
				// fprintf(stderr, "%d: Color shuffle %s => A\n", s_n, read_ba ? "A" : "G");
				m_om_bsel.wa = 1;
			}
			else
			{
				// fprintf(stderr, "%d: Color shuffle %s => G\n", s_n, read_ba ? "A" : "G");
				m_om_bsel.wg = 1;
			}
			if (ba_mask)
				m_ps_sel.fbmask = 1;
		}

		if (m_ps_sel.fbmask && enable_fbmask_emulation)
		{
			// fprintf(stderr, "%d: FBMASK Unsafe SW emulated fb_mask:%x on tex shuffle\n", s_n, fbmask);
			ps_cb.FbMask.r = rg_mask;
			ps_cb.FbMask.g = rg_mask;
			ps_cb.FbMask.b = ba_mask;
			ps_cb.FbMask.a = ba_mask;
			m_bind_rtsample = true;
		}
		else
		{
			m_ps_sel.fbmask = 0;
		}
	}
	else
	{
		m_ps_sel.dfmt = GSLocalMemory::m_psm[m_context->FRAME.PSM].fmt;

		const GSVector4i fbmask_v = GSVector4i::load((int)m_context->FRAME.FBMSK);
		const int ff_fbmask = fbmask_v.eq8(GSVector4i::xffffffff()).mask();
		const int zero_fbmask = fbmask_v.eq8(GSVector4i::zero()).mask();

		m_om_bsel.wrgba = ~ff_fbmask; // Enable channel if at least 1 bit is 0

		m_ps_sel.fbmask = enable_fbmask_emulation && (~ff_fbmask & ~zero_fbmask & 0xF);

		if (m_ps_sel.fbmask)
		{
			ps_cb.FbMask = fbmask_v.u8to32();
			// Only alpha is special here, I think we can take a very unsafe shortcut
			// Alpha isn't blended on the GS but directly copyied into the RT.
			//
			// Behavior is clearly undefined however there is a high probability that
			// it will work. Masked bit will be constant and normally the same everywhere
			// RT/FS output/Cached value.

			/*fprintf(stderr, "%d: FBMASK Unsafe SW emulated fb_mask:%x on %d bits format\n", s_n, m_context->FRAME.FBMSK,
				(GSLocalMemory::m_psm[m_context->FRAME.PSM].fmt == 2) ? 16 : 32);*/
			m_bind_rtsample = true;
		}
	}
}

void GSRendererDX11::EmulateChannelShuffle(GSTexture** rt, const GSTextureCache::Source* tex)
{
	GSDevice11* dev = (GSDevice11*)m_dev;

	// Uncomment to disable HLE emulation (allow to trace the draw call)
	// m_channel_shuffle = false;

	// First let's check we really have a channel shuffle effect
	if (m_channel_shuffle)
	{
		if (m_game.title == CRC::GT4 || m_game.title == CRC::GT3 || m_game.title == CRC::GTConcept || m_game.title == CRC::TouristTrophy)
		{
			// fprintf(stderr, "%d: Gran Turismo RGB Channel\n", s_n);
			m_ps_sel.channel = ChannelFetch_RGB;
			m_context->TEX0.TFX = TFX_DECAL;
			*rt = tex->m_from_target;
		}
		else if (m_game.title == CRC::Tekken5)
		{
			if (m_context->FRAME.FBW == 1)
			{
				// Used in stages: Secret Garden, Acid Rain, Moonlit Wilderness
				// fprintf(stderr, "%d: Tekken5 RGB Channel\n", s_n);
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
		else if ((tex->m_texture->GetType() == GSTexture::Type::DepthStencil) && !(tex->m_32_bits_fmt))
		{
			// So far 2 games hit this code path. Urban Chaos and Tales of Abyss
			// UC: will copy depth to green channel
			// ToA: will copy depth to alpha channel
			if ((m_context->FRAME.FBMSK & 0xFF0000) == 0xFF0000)
			{
				// Green channel is masked
				// fprintf(stderr, "%d: Tales Of Abyss Crazyness (MSB 16b depth to Alpha)\n", s_n);
				m_ps_sel.tales_of_abyss_hle = 1;
			}
			else
			{
				// fprintf(stderr, "%d: Urban Chaos Crazyness (Green extraction)\n", s_n);
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
			// fprintf(stderr, "%d: Maybe not a channel!\n", s_n);
			m_channel_shuffle = false;
		}
		else if (m_context->CLAMP.WMS == 3 && ((m_context->CLAMP.MAXU & 0x8) == 8))
		{
			// Read either blue or Alpha. Let's go for Blue ;)
			// MGS3/Kill Zone
			// fprintf(stderr, "%d: Blue channel\n", s_n);
			m_ps_sel.channel = ChannelFetch_BLUE;
		}
		else if (m_context->CLAMP.WMS == 3 && ((m_context->CLAMP.MINU & 0x8) == 0))
		{
			// Read either Red or Green. Let's check the V coordinate. 0-1 is likely top so
			// red. 2-3 is likely bottom so green (actually depends on texture base pointer offset)
			const bool green = PRIM->FST && (m_vertex.buff[0].V & 32);
			if (green && (m_context->FRAME.FBMSK & 0x00FFFFFF) == 0x00FFFFFF)
			{
				// Typically used in Terminator 3
				const int blue_mask = m_context->FRAME.FBMSK >> 24;
				int blue_shift = -1;

				// Note: potentially we could also check the value of the clut
				switch (blue_mask)
				{
					case 0xFF: ASSERT(0);      break;
					case 0xFE: blue_shift = 1; break;
					case 0xFC: blue_shift = 2; break;
					case 0xF8: blue_shift = 3; break;
					case 0xF0: blue_shift = 4; break;
					case 0xE0: blue_shift = 5; break;
					case 0xC0: blue_shift = 6; break;
					case 0x80: blue_shift = 7; break;
					default:                   break;
				}

				if (blue_shift >= 0)
				{
					const int green_mask = ~blue_mask & 0xFF;
					const int green_shift = 8 - blue_shift;

					// fprintf(stderr, "%d: Green/Blue channel (%d, %d)\n", s_n, blue_shift, green_shift);
					ps_cb.ChannelShuffle = GSVector4i(blue_mask, blue_shift, green_mask, green_shift);
					m_ps_sel.channel = ChannelFetch_GXBY;
					m_context->FRAME.FBMSK = 0x00FFFFFF;
				}
				else
				{
					// fprintf(stderr, "%d: Green channel (wrong mask) (fbmask %x)\n", s_n, blue_mask);
					m_ps_sel.channel = ChannelFetch_GREEN;
				}
			}
			else if (green)
			{
				// fprintf(stderr, "%d: Green channel\n", s_n);
				m_ps_sel.channel = ChannelFetch_GREEN;
			}
			else
			{
				// Pop
				// fprintf(stderr, "%d: Red channel\n", s_n);
				m_ps_sel.channel = ChannelFetch_RED;
			}
		}
		else
		{
			// fprintf(stderr, "%d: Channel not supported\n", s_n);
			m_channel_shuffle = false;
		}
	}

	// Effect is really a channel shuffle effect so let's cheat a little
	if (m_channel_shuffle)
	{
		dev->PSSetShaderResource(4, tex->m_from_target);
		// Replace current draw with a fullscreen sprite
		//
		// Performance GPU note: it could be wise to reduce the size to
		// the rendered size of the framebuffer

		GSVertex* s = &m_vertex.buff[0];
		s[0].XYZ.X = (u16)(m_context->XYOFFSET.OFX + 0);
		s[1].XYZ.X = (u16)(m_context->XYOFFSET.OFX + 16384);
		s[0].XYZ.Y = (u16)(m_context->XYOFFSET.OFY + 0);
		s[1].XYZ.Y = (u16)(m_context->XYOFFSET.OFY + 16384);

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

void GSRendererDX11::EmulateBlending(u8& afix)
{
	// Partial port of OGL SW blending. Currently only works for accumulation and non recursive blend.

	// AA1: Don't enable blending on AA1, not yet implemented on hardware mode,
	// it requires coverage sample so it's safer to turn it off instead.
	const bool aa1 = PRIM->AA1 && (m_vt.m_primclass == GS_LINE_CLASS);

	// No blending or coverage anti-aliasing so early exit
	if (!(PRIM->ABE || m_env.PABE.PABE || aa1))
		return;

	m_om_bsel.abe = 1;
	const GIFRegALPHA& ALPHA = m_context->ALPHA;
	m_om_bsel.blend_index = u8(((ALPHA.A * 3 + ALPHA.B) * 3 + ALPHA.C) * 3 + ALPHA.D);
	const int blend_flag = m_dev->GetBlendFlags(m_om_bsel.blend_index);

	// Do the multiplication in shader for blending accumulation: Cs*As + Cd or Cs*Af + Cd
	const bool accumulation_blend = !!(blend_flag & BLEND_ACCU);

	// Blending doesn't require sampling of the rt
	const bool blend_non_recursive = !!(blend_flag & BLEND_NO_REC);

	// BLEND MIX selection, use a mix of hw/sw blending
	if (!m_vt.m_alpha.valid && (ALPHA.C == 0))
		GetAlphaMinMax();
	const bool blend_mix1 = !!(blend_flag & BLEND_MIX1);
	const bool blend_mix2 = !!(blend_flag & BLEND_MIX2);
	const bool blend_mix3 = !!(blend_flag & BLEND_MIX3);
	bool blend_mix = (blend_mix1 || blend_mix2 || blend_mix3)
		// Do not enable if As > 128 or F > 128, hw blend clamps to 1
		&& !((ALPHA.C == 0 && m_vt.m_alpha.max > 128) || (ALPHA.C == 2 && ALPHA.FIX > 128u));

	bool sw_blending = false;
	switch (m_sw_blending)
	{
		case ACC_BLEND_HIGH_D3D11:
		case ACC_BLEND_MEDIUM_D3D11:
		case ACC_BLEND_BASIC_D3D11:
			sw_blending |= accumulation_blend || blend_non_recursive;
			[[fallthrough]];
		default:
			break;
	}

	// Do not run BLEND MIX if sw blending is already present, it's less accurate
	if (m_sw_blending)
	{
		blend_mix &= !sw_blending;
		sw_blending |= blend_mix;
	}

	// Color clip
	if (m_env.COLCLAMP.CLAMP == 0)
	{
		// fprintf(stderr, "%d: COLCLIP Info (Blending: %d/%d/%d/%d)\n", s_n, ALPHA.A, ALPHA.B, ALPHA.C, ALPHA.D);
		if (blend_non_recursive)
		{
			// The fastest algo that requires a single pass
			// fprintf(stderr, "%d: COLCLIP Free mode ENABLED\n", s_n);
			m_ps_sel.colclip = 1;
			sw_blending = true;
		}
		else if (accumulation_blend || blend_mix)
		{
			// fprintf(stderr, "%d: COLCLIP Fast HDR mode ENABLED\n", s_n);
			sw_blending = true;
			m_ps_sel.hdr = 1;
		}
		else
		{
			// fprintf(stderr, "%d: COLCLIP HDR mode ENABLED\n", s_n);
			m_ps_sel.hdr = 1;
		}
	}

	// Per pixel alpha blending
	if (m_env.PABE.PABE)
	{
		// Breath of Fire Dragon Quarter, Strawberry Shortcake, Super Robot Wars, Cartoon Network Racing.

		if (ALPHA.A == 0 && ALPHA.B == 1 && ALPHA.C == 0 && ALPHA.D == 1)
		{
			// this works because with PABE alpha blending is on when alpha >= 0x80, but since the pixel shader
			// cannot output anything over 0x80 (== 1.0) blending with 0x80 or turning it off gives the same result

			m_om_bsel.abe = 0;
			m_om_bsel.blend_index = 0;
		}
		if (sw_blending)
		{
			// fprintf(stderr, "%d: PABE mode ENABLED\n", s_n);
			m_ps_sel.pabe = 1;
			sw_blending = blend_non_recursive;
		}
	}

	/*fprintf(stderr, "%d: BLEND_INFO: %d/%d/%d/%d. Clamp:%d. Prim:%d number %d (sw %d)\n",
		s_n, ALPHA.A, ALPHA.B, ALPHA.C, ALPHA.D, m_env.COLCLAMP.CLAMP, m_vt.m_primclass, m_vertex.next, sw_blending);*/

	if (sw_blending)
	{
		m_ps_sel.blend_a = ALPHA.A;
		m_ps_sel.blend_b = ALPHA.B;
		m_ps_sel.blend_c = ALPHA.C;
		m_ps_sel.blend_d = ALPHA.D;

		if (accumulation_blend)
		{
			// Keep HW blending to do the addition/subtraction
			m_om_bsel.accu_blend = 1;
			afix = 0;
			if (ALPHA.A == 2)
			{
				// The blend unit does a reverse subtraction so it means
				// the shader must output a positive value.
				// Replace 0 - Cs by Cs - 0
				m_ps_sel.blend_a = ALPHA.B;
				m_ps_sel.blend_b = 2;
			}
			// Remove the addition/substraction from the SW blending
			m_ps_sel.blend_d = 2;
		}
		else if (blend_mix)
		{
			afix = (ALPHA.C == 2) ? ALPHA.FIX : 0;
			m_om_bsel.blend_mix = 1;

			if (blend_mix1)
			{
				m_ps_sel.blend_a = 0;
				m_ps_sel.blend_b = 2;
				m_ps_sel.blend_d = 2;
			}
			else if (blend_mix2)
			{
				m_ps_sel.blend_a = 0;
				m_ps_sel.blend_b = 2;
				m_ps_sel.blend_d = 0;
			}
			else if (blend_mix3)
			{
				m_ps_sel.blend_a = 2;
				m_ps_sel.blend_b = 0;
				m_ps_sel.blend_d = 0;
			}
		}
		else
		{
			// Disable HW blending
			m_om_bsel.abe = 0;
			m_om_bsel.blend_index = 0;
			afix = 0;

			// Only BLEND_NO_REC should hit this code path for now
			ASSERT(blend_non_recursive);
		}

		// Require the fix alpha vlaue
		if (ALPHA.C == 2)
			ps_cb.Af_MaxDepth.x = (float)ALPHA.FIX / 128.0f;
	}
	else
	{
		m_ps_sel.clr1 = !!(blend_flag & BLEND_C_CLR);
		afix = (ALPHA.C == 2) ? ALPHA.FIX : 0;
		// FIXME: When doing HW blending with a 24 bit frambuffer and ALPHA.C == 1 (Ad) it should be handled
		// as if Ad = 1.0f. As with OGL side it is probably best to set m_om_bsel.c = 1 (Af) and use
		// AFIX = 0x80 (Af = 1.0f).
	}
}

void GSRendererDX11::EmulateTextureSampler(const GSTextureCache::Source* tex)
{
	// Warning fetch the texture PSM format rather than the context format. The latter could have been corrected in the texture cache for depth.
	//const GSLocalMemory::psm_t &psm = GSLocalMemory::m_psm[m_context->TEX0.PSM];
	const GSLocalMemory::psm_t& psm = GSLocalMemory::m_psm[tex->m_TEX0.PSM];
	const GSLocalMemory::psm_t& cpsm = psm.pal > 0 ? GSLocalMemory::m_psm[m_context->TEX0.CPSM] : psm;

	const u8 wms = m_context->CLAMP.WMS;
	const u8 wmt = m_context->CLAMP.WMT;
	const bool complex_wms_wmt = !!((wms | wmt) & 2);

	bool bilinear = m_vt.IsLinear();
	const bool shader_emulated_sampler = tex->m_palette || cpsm.fmt != 0 || complex_wms_wmt || psm.depth;

	// 1 and 0 are equivalent
	m_ps_sel.wms = (wms & 2) ? wms : 0;
	m_ps_sel.wmt = (wmt & 2) ? wmt : 0;

	const int w = tex->m_texture->GetWidth();
	const int h = tex->m_texture->GetHeight();

	const int tw = (int)(1 << m_context->TEX0.TW);
	const int th = (int)(1 << m_context->TEX0.TH);

	const GSVector4 WH(tw, th, w, h);

	// Depth + bilinear filtering isn't done yet (And I'm not sure we need it anyway but a game will prove me wrong)
	// So of course, GTA set the linear mode, but sampling is done at texel center so it is equivalent to nearest sampling
	ASSERT(!(psm.depth && m_vt.IsLinear()));

	// Performance note:
	// 1/ Don't set 0 as it is the default value
	// 2/ Only keep aem when it is useful (avoid useless shader permutation)
	if (m_ps_sel.shuffle)
	{
		// Force a 32 bits access (normally shuffle is done on 16 bits)
		// m_ps_sel.fmt = 0; // removed as an optimization
		m_ps_sel.aem = m_env.TEXA.AEM;
		ASSERT(tex->m_target);

		// Require a float conversion if the texure is a depth otherwise uses Integral scaling
		if (psm.depth)
		{
			m_ps_sel.depth_fmt = (tex->m_texture->GetType() != GSTexture::Type::DepthStencil) ? 3 : 1;
		}

		// Shuffle is a 16 bits format, so aem is always required
		const GSVector4 ta(m_env.TEXA & GSVector4i::x000000ff());
		ps_cb.MinF_TA = (GSVector4(ps_cb.MskFix) + 0.5f).xyxy(ta) / WH.xyxy(GSVector4(255, 255));

		bilinear &= m_vt.IsLinear();

		const GSVector4 half_offset = RealignTargetTextureCoordinate(tex);
		vs_cb.Texture_Scale_Offset.z = half_offset.x;
		vs_cb.Texture_Scale_Offset.w = half_offset.y;
	}
	else if (tex->m_target)
	{
		// Use an old target. AEM and index aren't resolved it must be done
		// on the GPU

		// Select the 32/24/16 bits color (AEM)
		m_ps_sel.fmt = cpsm.fmt;
		m_ps_sel.aem = m_env.TEXA.AEM;

		// Don't upload AEM if format is 32 bits
		if (cpsm.fmt)
		{
			const GSVector4 ta(m_env.TEXA & GSVector4i::x000000ff());
			ps_cb.MinF_TA = (GSVector4(ps_cb.MskFix) + 0.5f).xyxy(ta) / WH.xyxy(GSVector4(255, 255));
		}

		// Select the index format
		if (tex->m_palette)
		{
			// FIXME Potentially improve fmt field in GSLocalMemory
			if (m_context->TEX0.PSM == PSM_PSMT4HL)
				m_ps_sel.fmt |= 1 << 2;
			else if (m_context->TEX0.PSM == PSM_PSMT4HH)
				m_ps_sel.fmt |= 2 << 2;
			else
				m_ps_sel.fmt |= 3 << 2;

			// Alpha channel of the RT is reinterpreted as an index. Star
			// Ocean 3 uses it to emulate a stencil buffer.  It is a very
			// bad idea to force bilinear filtering on it.
			bilinear &= m_vt.IsLinear();
		}

		// Depth format
		if (tex->m_texture->GetType() == GSTexture::Type::DepthStencil)
		{
			// Require a float conversion if the texure is a depth format
			m_ps_sel.depth_fmt = (psm.bpp == 16) ? 2 : 1;

			// Don't force interpolation on depth format
			bilinear &= m_vt.IsLinear();
		}
		else if (psm.depth)
		{
			// Use Integral scaling
			m_ps_sel.depth_fmt = 3;

			// Don't force interpolation on depth format
			bilinear &= m_vt.IsLinear();
		}

		const GSVector4 half_offset = RealignTargetTextureCoordinate(tex);
		vs_cb.Texture_Scale_Offset.z = half_offset.x;
		vs_cb.Texture_Scale_Offset.w = half_offset.y;
	}
	else if (tex->m_palette)
	{
		// Use a standard 8 bits texture. AEM is already done on the CLUT
		// Therefore you only need to set the index
		// m_ps_sel.aem     = 0; // removed as an optimization

		// Note 4 bits indexes are converted to 8 bits
		m_ps_sel.fmt = 3 << 2;
	}
	else
	{
		// Standard texture. Both index and AEM expansion were already done by the CPU.
		// m_ps_sel.fmt = 0; // removed as an optimization
		// m_ps_sel.aem = 0; // removed as an optimization
	}

	if (m_context->TEX0.TFX == TFX_MODULATE && m_vt.m_eq.rgba == 0xFFFF && m_vt.m_min.c.eq(GSVector4i(128)))
	{
		// Micro optimization that reduces GPU load (removes 5 instructions on the FS program)
		m_ps_sel.tfx = TFX_DECAL;
	}
	else
	{
		m_ps_sel.tfx = m_context->TEX0.TFX;
	}

	m_ps_sel.tcc = m_context->TEX0.TCC;

	m_ps_sel.ltf = bilinear && shader_emulated_sampler;

	m_ps_sel.point_sampler = !bilinear || shader_emulated_sampler;

	const GSVector4 TextureScale = GSVector4(0.0625f) / WH.xyxy();
	vs_cb.Texture_Scale_Offset.x = TextureScale.x;
	vs_cb.Texture_Scale_Offset.y = TextureScale.y;

	if (PRIM->FST)
	{
		//Maybe better?
		//vs_cb.TextureScale = GSVector4(1.0f / 16) * GSVector4(tex->m_texture->GetScale()).xyxy() / WH.zwzw();
		m_ps_sel.fst = 1;
	}

	ps_cb.WH = WH;
	ps_cb.HalfTexel = GSVector4(-0.5f, 0.5f).xxyy() / WH.zwzw();
	if (complex_wms_wmt)
	{
		ps_cb.MskFix = GSVector4i(m_context->CLAMP.MINU, m_context->CLAMP.MINV, m_context->CLAMP.MAXU, m_context->CLAMP.MAXV);
		ps_cb.MinMax = GSVector4(ps_cb.MskFix) / WH.xyxy();
	}

	// TC Offset Hack
	m_ps_sel.tcoffsethack = m_userhacks_tcoffset;
	ps_cb.TC_OffsetHack = GSVector4(m_userhacks_tcoffset_x, m_userhacks_tcoffset_y).xyxy() / WH.xyxy();

	// Must be done after all coordinates math
	if (m_context->HasFixedTEX0() && !PRIM->FST)
	{
		m_ps_sel.invalid_tex0 = 1;
		// Use invalid size to denormalize ST coordinate
		ps_cb.WH.x = (float)(1 << m_context->stack.TEX0.TW);
		ps_cb.WH.y = (float)(1 << m_context->stack.TEX0.TH);

		// We can't handle m_target with invalid_tex0 atm due to upscaling
		ASSERT(!tex->m_target);
	}

	// Only enable clamping in CLAMP mode. REGION_CLAMP will be done manually in the shader
	m_ps_ssel.tau = (wms != CLAMP_CLAMP);
	m_ps_ssel.tav = (wmt != CLAMP_CLAMP);
	m_ps_ssel.ltf = bilinear && !shader_emulated_sampler;
}

void GSRendererDX11::ResetStates()
{
	m_bind_rtsample = false;

	m_vs_sel.key = 0;
	m_gs_sel.key = 0;
	m_ps_sel.key = 0;

	m_ps_ssel.key  = 0;
	m_om_bsel.key  = 0;
	m_om_dssel.key = 0;
}

void GSRendererDX11::DrawPrims(GSTexture* rt, GSTexture* ds, GSTextureCache::Source* tex)
{
	GSTexture* hdr_rt = NULL;

	const GSVector2i& rtsize = ds ? ds->GetSize()  : rt->GetSize();
	const GSVector2& rtscale = ds ? ds->GetScale() : rt->GetScale();

	const bool DATE = m_context->TEST.DATE && m_context->FRAME.PSM != PSM_PSMCT24;
	bool DATE_one = false;

	const bool ate_first_pass = m_context->TEST.DoFirstPass();
	const bool ate_second_pass = m_context->TEST.DoSecondPass();

	ResetStates();
	vs_cb.Texture_Scale_Offset = GSVector4(0.0f);

	ASSERT(m_dev != NULL);
	GSDevice11* dev = (GSDevice11*)m_dev;

	// HLE implementation of the channel selection effect
	//
	// Warning it must be done at the begining because it will change the vertex list
	EmulateChannelShuffle(&rt, tex);

	// Upscaling hack to avoid various line/grid issues
	MergeSprite(tex);

	EmulateTextureShuffleAndFbmask();

	// DATE: selection of the algorithm.
	if (DATE)
	{
		if (m_texture_shuffle)
		{
			// DATE case not supported yet so keep using the old method.
			// Leave the check in to make sure other DATE cases are triggered correctly.
			// fprintf(stderr, "%d: DATE: With texture shuffle\n", s_n);
		}
		else if (m_om_bsel.wa && !m_context->TEST.ATE)
		{
			// Performance note: check alpha range with GetAlphaMinMax()
			GetAlphaMinMax();
			if (m_context->TEST.DATM && m_vt.m_alpha.max < 128)
			{
				// Only first pixel (write 0) will pass (alpha is 1)
				// fprintf(stderr, "%d: DATE: Fast with alpha %d-%d\n", s_n, m_vt.m_alpha.min, m_vt.m_alpha.max);
				DATE_one = true;
			}
			else if (!m_context->TEST.DATM && m_vt.m_alpha.min >= 128)
			{
				// Only first pixel (write 1) will pass (alpha is 0)
				// fprintf(stderr, "%d: DATE: Fast with alpha %d-%d\n", s_n, m_vt.m_alpha.min, m_vt.m_alpha.max);
				DATE_one = true;
			}
			else if ((m_vt.m_primclass == GS_SPRITE_CLASS /*&& m_drawlist.size() < 50*/) || (m_index.tail < 100))
			{
				// DATE case not supported yet so keep using the old method.
				// Leave the check in to make sure other DATE cases are triggered correctly.
				// fprintf(stderr, "%d: DATE: Slow with alpha %d-%d not supported\n", s_n, m_vt.m_alpha.min, m_vt.m_alpha.max);
			}
			else if (m_accurate_date)
			{
				// fprintf(stderr, "%d: DATE: Fast AD with alpha %d-%d\n", s_n, m_vt.m_alpha.min, m_vt.m_alpha.max);
				DATE_one = true;
			}
		}
		else if (!m_om_bsel.wa && !m_context->TEST.ATE)
		{
			// TODO: is it legal ? Likely but it need to be tested carefully.
		}
	}

	// Blend
	u8 afix = 0;
	if (!IsOpaque() && rt)
	{
		EmulateBlending(afix);
	}

	if (m_ps_sel.hdr)
	{
		const GSVector4 dRect(ComputeBoundingBox(rtscale, rtsize));
		const GSVector4 sRect = dRect / GSVector4(rtsize.x, rtsize.y).xyxy();
		hdr_rt = dev->CreateRenderTarget(rtsize.x, rtsize.y, GSTexture::Format::FloatColor);
		// Warning: StretchRect must be called before BeginScene otherwise
		// vertices will be overwritten. Trust me you don't want to do that.
		dev->StretchRect(rt, sRect, hdr_rt, dRect, ShaderConvert::COPY, false);
	}

	if (m_ps_sel.dfmt == 1)
	{
		// Disable writing of the alpha channel
		m_om_bsel.wa = 0;
	}

	if (DATE)
	{
		const GSVector4i dRect = ComputeBoundingBox(rtscale, rtsize);

		const GSVector4 src = GSVector4(dRect) / GSVector4(rtsize.x, rtsize.y).xyxy();
		const GSVector4 dst = src * 2.0f - 1.0f;

		GSVertexPT1 vertices[] =
		{
			{GSVector4(dst.x, -dst.y, 0.5f, 1.0f), GSVector2(src.x, src.y)},
			{GSVector4(dst.z, -dst.y, 0.5f, 1.0f), GSVector2(src.z, src.y)},
			{GSVector4(dst.x, -dst.w, 0.5f, 1.0f), GSVector2(src.x, src.w)},
			{GSVector4(dst.z, -dst.w, 0.5f, 1.0f), GSVector2(src.z, src.w)},
		};

		dev->SetupDATE(rt, ds, vertices, m_context->TEST.DATM);
	}

	//

	dev->BeginScene();

	// om

	EmulateZbuffer();

	// vs

	m_vs_sel.tme = PRIM->TME;
	m_vs_sel.fst = PRIM->FST;

	// FIXME D3D11 and GL support half pixel center. Code could be easier!!!
	const float sx = 2.0f * rtscale.x / (rtsize.x << 4);
	const float sy = 2.0f * rtscale.y / (rtsize.y << 4);
	const float ox = (float)(int)m_context->XYOFFSET.OFX;
	const float oy = (float)(int)m_context->XYOFFSET.OFY;
	float ox2 = -1.0f / rtsize.x;
	float oy2 = -1.0f / rtsize.y;

	//This hack subtracts around half a pixel from OFX and OFY.
	//
	//The resulting shifted output aligns better with common blending / corona / blurring effects,
	//but introduces a few bad pixels on the edges.

	if (rt && rt->LikelyOffset && m_userHacks_HPO == 1)
	{
		ox2 *= rt->OffsetHack_modx;
		oy2 *= rt->OffsetHack_mody;
	}

	vs_cb.VertexScale  = GSVector4(sx, -sy, ldexpf(1, -32), 0.0f);
	vs_cb.VertexOffset = GSVector4(ox * sx + ox2 + 1, -(oy * sy + oy2 + 1), 0.0f, -1.0f);
	// END of FIXME

	// gs

	m_gs_sel.iip = PRIM->IIP;
	m_gs_sel.prim = m_vt.m_primclass;

	// ps

	if (DATE)
	{
		m_om_dssel.date = 1;
		if (DATE_one)
		{
			m_om_dssel.date_one = 1;
		}
	}

	m_ps_sel.fba = m_context->FBA.FBA;
	m_ps_sel.dither = m_dithering > 0 && m_ps_sel.dfmt == 2 && m_env.DTHE.DTHE;

	if (m_ps_sel.dither)
	{
		m_ps_sel.dither = m_dithering;
		ps_cb.DitherMatrix[0] = GSVector4(m_env.DIMX.DM00, m_env.DIMX.DM10, m_env.DIMX.DM20, m_env.DIMX.DM30);
		ps_cb.DitherMatrix[1] = GSVector4(m_env.DIMX.DM01, m_env.DIMX.DM11, m_env.DIMX.DM21, m_env.DIMX.DM31);
		ps_cb.DitherMatrix[2] = GSVector4(m_env.DIMX.DM02, m_env.DIMX.DM12, m_env.DIMX.DM22, m_env.DIMX.DM32);
		ps_cb.DitherMatrix[3] = GSVector4(m_env.DIMX.DM03, m_env.DIMX.DM13, m_env.DIMX.DM23, m_env.DIMX.DM33);
	}

	if (PRIM->FGE)
	{
		m_ps_sel.fog = 1;

		const GSVector4 fc = GSVector4::rgba32(m_env.FOGCOL.U32[0]);
		// Blend AREF to avoid to load a random value for alpha (dirty cache)
		ps_cb.FogColor_AREF = fc.blend32<8>(ps_cb.FogColor_AREF);
	}

	// Warning must be done after EmulateZbuffer
	// Depth test is always true so it can be executed in 2 passes (no order required) unlike color.
	// The idea is to compute first the color which is independent of the alpha test. And then do a 2nd
	// pass to handle the depth based on the alpha test.
	bool ate_RGBA_then_Z = false;
	bool ate_RGB_then_ZA = false;
	u8 ps_atst = 0;
	if (ate_first_pass & ate_second_pass)
	{
		// fprintf(stdout, "%d: Complex Alpha Test\n", s_n);
		const bool commutative_depth = (m_om_dssel.ztst == ZTST_GEQUAL && m_vt.m_eq.z) || (m_om_dssel.ztst == ZTST_ALWAYS);
		const bool commutative_alpha = (m_context->ALPHA.C != 1); // when either Alpha Src or a constant

		ate_RGBA_then_Z = (m_context->TEST.AFAIL == AFAIL_FB_ONLY) & commutative_depth;
		ate_RGB_then_ZA = (m_context->TEST.AFAIL == AFAIL_RGB_ONLY) & commutative_depth & commutative_alpha;
	}

	if (ate_RGBA_then_Z)
	{
		// fprintf(stdout, "%d: Alternate ATE handling: ate_RGBA_then_Z\n", s_n);
		// Render all color but don't update depth
		// ATE is disabled here
		m_om_dssel.zwe = false;
	}
	else if (ate_RGB_then_ZA)
	{
		// fprintf(stdout, "%d: Alternate ATE handling: ate_RGB_then_ZA\n", s_n);
		// Render RGB color but don't update depth/alpha
		// ATE is disabled here
		m_om_dssel.zwe = false;
		m_om_bsel.wa = false;
	}
	else
	{
		EmulateAtst(ps_cb.FogColor_AREF, ps_atst, false);
		m_ps_sel.atst = ps_atst;
	}

	if (tex)
	{
		EmulateTextureSampler(tex);
	}
	else
	{
		m_ps_sel.tfx = 4;
	}

	if (m_bind_rtsample)
	{
		// Bind the RT.This way special effect can use it.
		// Do not always bind the rt when it's not needed,
		// only bind it when effects use it such as fbmask emulation currently
		// because we copy the frame buffer and it is quite slow.
		dev->PSSetShaderResource(3, rt);
	}

	if (m_game.title == CRC::ICO)
	{
		const GSVertex* v = &m_vertex.buff[0];
		const GSVideoMode mode = GetVideoMode();
		if (tex && m_vt.m_primclass == GS_SPRITE_CLASS && m_vertex.next == 2 && PRIM->ABE && // Blend texture
				((v[1].U == 8200 && v[1].V == 7176 && mode == GSVideoMode::NTSC) || // at display resolution 512x448
				(v[1].U == 8200 && v[1].V == 8200 && mode == GSVideoMode::PAL)) && // at display resolution 512x512
				tex->m_TEX0.PSM == PSM_PSMT8H) // i.e. read the alpha channel of a 32 bits texture
		{
			// Note potentially we can limit to TBP0:0x2800

			// Depth buffer was moved so GS will invalide it which means a
			// downscale. ICO uses the MSB depth bits as the texture alpha
			// channel.  However this depth of field effect requires
			// texel:pixel mapping accuracy.
			//
			// Use an HLE shader to sample depth directly as the alpha channel

			// OutputDebugString("ICO HLE");

			m_ps_sel.depth_fmt = 1;
			m_ps_sel.channel = ChannelFetch_BLUE;

			dev->PSSetShaderResource(4, ds);

			if (!tex->m_palette)
			{
				const u16 pal = GSLocalMemory::m_psm[tex->m_TEX0.PSM].pal;
				m_tc->AttachPaletteToSource(tex, pal, true);
			}
		}
	}

	// rs
	const GSVector4& hacked_scissor = m_channel_shuffle ? GSVector4(0, 0, 1024, 1024) : m_context->scissor.in;
	const GSVector4i scissor = GSVector4i(GSVector4(rtscale).xyxy() * hacked_scissor).rintersect(GSVector4i(rtsize).zwxy());

	if (hdr_rt)
		dev->OMSetRenderTargets(hdr_rt, ds, &scissor);
	else
		dev->OMSetRenderTargets(rt, ds, &scissor);

	dev->PSSetShaderResource(0, tex ? tex->m_texture : NULL);
	dev->PSSetShaderResource(1, tex ? tex->m_palette : NULL);

	SetupIA(sx, sy);

	dev->SetupOM(m_om_dssel, m_om_bsel, afix);
	dev->SetupVS(m_vs_sel, &vs_cb);
	dev->SetupGS(m_gs_sel, &gs_cb);
	dev->SetupPS(m_ps_sel, &ps_cb, m_ps_ssel);

	// draw

	if (ate_first_pass)
	{
		dev->DrawIndexedPrimitive();
	}

	if (ate_second_pass)
	{
		ASSERT(!m_env.PABE.PABE);

		if (ate_RGBA_then_Z | ate_RGB_then_ZA)
		{
			// Enable ATE as first pass to update the depth
			// of pixels that passed the alpha test
			EmulateAtst(ps_cb.FogColor_AREF, ps_atst, false);
		}
		else
		{
			// second pass will process the pixels that failed
			// the alpha test
			EmulateAtst(ps_cb.FogColor_AREF, ps_atst, true);
		}

		m_ps_sel.atst = ps_atst;

		dev->SetupPS(m_ps_sel, &ps_cb, m_ps_ssel);

		bool z = m_om_dssel.zwe;
		bool r = m_om_bsel.wr;
		bool g = m_om_bsel.wg;
		bool b = m_om_bsel.wb;
		bool a = m_om_bsel.wa;

		switch (m_context->TEST.AFAIL)
		{
			case AFAIL_KEEP: z = r = g = b = a = false; break; // none
			case AFAIL_FB_ONLY: z = false; break; // rgba
			case AFAIL_ZB_ONLY: r = g = b = a = false; break; // z
			case AFAIL_RGB_ONLY: z = a = false; break; // rgb
			default: __assume(0);
		}

		// Depth test should be disabled when depth writes are masked and similarly, Alpha test must be disabled
		// when writes to all of the alpha bits in the Framebuffer are masked.
		if (ate_RGBA_then_Z)
		{
			z = !m_context->ZBUF.ZMSK;
			r = g = b = a = false;
		}
		else if (ate_RGB_then_ZA)
		{
			z = !m_context->ZBUF.ZMSK;
			a = (m_context->FRAME.FBMSK & 0xFF000000) != 0xFF000000;
			r = g = b = false;
		}

		if (z || r || g || b || a)
		{
			m_om_dssel.zwe = z;
			m_om_bsel.wr = r;
			m_om_bsel.wg = g;
			m_om_bsel.wb = b;
			m_om_bsel.wa = a;

			dev->SetupOM(m_om_dssel, m_om_bsel, afix);

			dev->DrawIndexedPrimitive();
		}
	}

	dev->EndScene();

	// Warning: EndScene must be called before StretchRect otherwise
	// vertices will be overwritten. Trust me you don't want to do that.
	if (hdr_rt)
	{
		const GSVector4 dRect(ComputeBoundingBox(rtscale, rtsize));
		const GSVector4 sRect = dRect / GSVector4(rtsize.x, rtsize.y).xyxy();
		dev->StretchRect(hdr_rt, sRect, rt, dRect, ShaderConvert::MOD_256, false);

		dev->Recycle(hdr_rt);
	}
}
