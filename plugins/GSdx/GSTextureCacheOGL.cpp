/*
 *	Copyright (C) 2011-2011 Gregory hainaut
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
#include "GSTextureCacheOGL.h"

GSTextureCacheOGL::GSTextureCacheOGL(GSRenderer* r)
	: GSTextureCache(r)
{
}

void GSTextureCacheOGL::Read(Target* t, const GSVector4i& r)
{
	if(!t->m_dirty.empty())
	{
		return;
	}

	const GIFRegTEX0& TEX0 = t->m_TEX0;

	GLuint fmt;
	int ps_shader;
	switch (TEX0.PSM)
	{
		case PSM_PSMCT32:
		case PSM_PSMCT24:
			fmt = GL_RGBA8;
			ps_shader = 0;
			break;

		case PSM_PSMCT16:
		case PSM_PSMCT16S:
			fmt = GL_R16UI;
			ps_shader = 1;
			break;

		case PSM_PSMZ32:
			fmt = GL_R32UI;
			ps_shader = 10;
			break;

		case PSM_PSMZ24:
			fmt = GL_R32UI;
			ps_shader = 10;
			break;

		case PSM_PSMZ16:
		case PSM_PSMZ16S:
			fmt = GL_R16UI;
			ps_shader = 10;
			break;

		default:
			return;
	}


	// Yes lots of logging, but I'm not confident with this code
	GL_PUSH("Texture Cache Read. Format(0x%x)", TEX0.PSM);

	GL_CACHE("TC: Read Back Target: %d (0x%x)[fmt: 0x%x]",
			t->m_texture->GetID(), TEX0.TBP0, TEX0.PSM);

	GL_PERF("Read texture from GPU. Format(0x%x)", TEX0.PSM);

	GSVector4 src = GSVector4(r) * GSVector4(t->m_texture->GetScale()).xyxy() / GSVector4(t->m_texture->GetSize()).xyxy();

	if(GSTexture* offscreen = m_renderer->m_dev->CopyOffscreen(t->m_texture, src, r.width(), r.height(), fmt, ps_shader))
	{
		GSTexture::GSMap m;

		if(offscreen->Map(m))
		{
			// TODO: block level write

			GSOffset* off = m_renderer->m_mem.GetOffset(TEX0.TBP0, TEX0.TBW, TEX0.PSM);

			switch(TEX0.PSM)
			{
				case PSM_PSMCT32:
					m_renderer->m_mem.WritePixel32(m.bits, m.pitch, off, r);
					break;
				case PSM_PSMCT24:
					m_renderer->m_mem.WritePixel24(m.bits, m.pitch, off, r);
					break;
				case PSM_PSMCT16:
				case PSM_PSMCT16S:
					m_renderer->m_mem.WritePixel16(m.bits, m.pitch, off, r);
					break;

				case PSM_PSMZ32:
					m_renderer->m_mem.WritePixel32(m.bits, m.pitch, off, r);
					break;
				case PSM_PSMZ24:
					m_renderer->m_mem.WritePixel24(m.bits, m.pitch, off, r);
					break;
				case PSM_PSMZ16:
				case PSM_PSMZ16S:
					m_renderer->m_mem.WritePixel16(m.bits, m.pitch, off, r);

				default:
					ASSERT(0);
			}

			offscreen->Unmap();
		}

		// FIXME invalidate data
		m_renderer->m_dev->Recycle(offscreen);
	}

	GL_POP();
}

