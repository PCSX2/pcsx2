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
#include "GSTextureCacheOGL.h"

GSTextureCacheOGL::GSTextureCacheOGL(GSRenderer* r)
	: GSTextureCache(r)
{
}

void GSTextureCacheOGL::Read(Target* t, const GSVector4i& r)
{
	if (!t->m_dirty.empty() || r.width() == 0 || r.height() == 0)
		return;

	const GIFRegTEX0& TEX0 = t->m_TEX0;

	GLuint fmt;
	int ps_shader;
	switch (TEX0.PSM)
	{
		case PSM_PSMCT32:
		case PSM_PSMCT24:
			fmt = GL_RGBA8;
			ps_shader = ShaderConvert_COPY;
			break;

		case PSM_PSMCT16:
		case PSM_PSMCT16S:
			fmt = GL_R16UI;
			ps_shader = ShaderConvert_RGBA8_TO_16_BITS;
			break;

		case PSM_PSMZ32:
		case PSM_PSMZ24:
			fmt = GL_R32UI;
			ps_shader = ShaderConvert_FLOAT32_TO_32_BITS;
			break;

		case PSM_PSMZ16:
		case PSM_PSMZ16S:
			fmt = GL_R16UI;
			ps_shader = ShaderConvert_FLOAT32_TO_32_BITS;
			break;

		default:
			return;
	}


	// Yes lots of logging, but I'm not confident with this code
	GL_PUSH("Texture Cache Read. Format(0x%x)", TEX0.PSM);

	GL_PERF("TC: Read Back Target: %d (0x%x)[fmt: 0x%x]. Size %dx%d",
			t->m_texture->GetID(), TEX0.TBP0, TEX0.PSM, r.width(), r.height());

	GSVector4 src = GSVector4(r) * GSVector4(t->m_texture->GetScale()).xyxy() / GSVector4(t->m_texture->GetSize()).xyxy();

	if (GSTexture* offscreen = m_renderer->m_dev->CopyOffscreen(t->m_texture, src, r.width(), r.height(), fmt, ps_shader))
	{
		GSTexture::GSMap m;
		GSVector4i r_offscreen(0, 0, r.width(), r.height());

		if (offscreen->Map(m, &r_offscreen))
		{
			// TODO: block level write

			GSOffset* off = m_renderer->m_mem.GetOffset(TEX0.TBP0, TEX0.TBW, TEX0.PSM);

			switch (TEX0.PSM)
			{
				case PSM_PSMCT32:
				case PSM_PSMZ32:
					m_renderer->m_mem.WritePixel32(m.bits, m.pitch, off, r);
					break;
				case PSM_PSMCT24:
				case PSM_PSMZ24:
					m_renderer->m_mem.WritePixel24(m.bits, m.pitch, off, r);
					break;
				case PSM_PSMCT16:
				case PSM_PSMCT16S:
				case PSM_PSMZ16:
				case PSM_PSMZ16S:
					m_renderer->m_mem.WritePixel16(m.bits, m.pitch, off, r);
					break;

				default:
					ASSERT(0);
			}

			offscreen->Unmap();
		}

		// FIXME invalidate data
		m_renderer->m_dev->Recycle(offscreen);
	}
}

void GSTextureCacheOGL::Read(Source* t, const GSVector4i& r)
{
	const GIFRegTEX0& TEX0 = t->m_TEX0;

	// FIXME Create a get function to avoid the useless copy
	// Note: With openGL 4.5 you can use glGetTextureSubImage

	if (GSTexture* offscreen = m_renderer->m_dev->CreateOffscreen(r.width(), r.height()))
	{
		m_renderer->m_dev->CopyRect(t->m_texture, offscreen, r);

		GSTexture::GSMap m;
		GSVector4i r_offscreen(0, 0, r.width(), r.height());

		if (offscreen->Map(m, &r_offscreen))
		{
			GSOffset* off = m_renderer->m_mem.GetOffset(TEX0.TBP0, TEX0.TBW, TEX0.PSM);

			m_renderer->m_mem.WritePixel32(m.bits, m.pitch, off, r);

			offscreen->Unmap();
		}

		// FIXME invalidate data
		m_renderer->m_dev->Recycle(offscreen);
	}
}
