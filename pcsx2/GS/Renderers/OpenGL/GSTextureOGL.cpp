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
#include <limits.h>
#include "GS/Renderers/OpenGL/GSDeviceOGL.h"
#include "GS/Renderers/OpenGL/GSTextureOGL.h"
#include "GS/Renderers/OpenGL/GLState.h"
#include "GS/GSPerfMon.h"
#include "GS/GSPng.h"
#include "GS/GSGL.h"
#include "common/StringUtil.h"

static constexpr u32 TEXTURE_UPLOAD_ALIGNMENT = 256;

GSTextureOGL::GSTextureOGL(Type type, int width, int height, int levels, Format format, GLuint fbo_read)
{
	// OpenGL didn't like dimensions of size 0
	m_size.x = std::max(1, width);
	m_size.y = std::max(1, height);
	m_format = format;
	m_type = type;
	m_fbo_read = fbo_read;
	m_texture_id = 0;
	m_mipmap_levels = 1;
	int gl_fmt = 0;

	// Bunch of constant parameter
	switch (m_format)
	{
		// 1 Channel integer
		case Format::PrimID:
			gl_fmt = GL_R32F;
			m_int_format = GL_RED;
			m_int_type = GL_INT;
			m_int_shift = 2;
			break;
		case Format::UInt32:
			gl_fmt = GL_R32UI;
			m_int_format = GL_RED_INTEGER;
			m_int_type = GL_UNSIGNED_INT;
			m_int_shift = 2;
			break;
		case Format::UInt16:
			gl_fmt = GL_R16UI;
			m_int_format = GL_RED_INTEGER;
			m_int_type = GL_UNSIGNED_SHORT;
			m_int_shift = 1;
			break;

		// 1 Channel normalized
		case Format::UNorm8:
			gl_fmt = GL_R8;
			m_int_format = GL_RED;
			m_int_type = GL_UNSIGNED_BYTE;
			m_int_shift = 0;
			break;

		// 4 channel normalized
		case Format::Color:
			gl_fmt = GL_RGBA8;
			m_int_format = GL_RGBA;
			m_int_type = GL_UNSIGNED_BYTE;
			m_int_shift = 2;
			break;

		// 4 channel float
		case Format::HDRColor:
			gl_fmt = GL_RGBA16;
			m_int_format = GL_RGBA;
			m_int_type = GL_UNSIGNED_SHORT;
			m_int_shift = 3;
			break;

		// Depth buffer
		case Format::DepthStencil:
		{
			if (!GLLoader::found_framebuffer_fetch)
			{
				gl_fmt = GL_DEPTH32F_STENCIL8;
				m_int_format = GL_DEPTH_STENCIL;
				m_int_type = GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
				m_int_shift = 3; // 4 bytes for depth + 4 bytes for stencil by texels
			}
			else
			{
				gl_fmt = GL_DEPTH_COMPONENT32F;
				m_int_format = GL_DEPTH_COMPONENT;
				m_int_type = GL_FLOAT;
				m_int_shift = 2;
			}
		}
		break;

		case Format::BC1:
			gl_fmt = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
			m_int_format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
			m_int_type = GL_UNSIGNED_BYTE;
			m_int_shift = 1;
			break;

		case Format::BC2:
			gl_fmt = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
			m_int_format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
			m_int_type = GL_UNSIGNED_BYTE;
			m_int_shift = 1;
			break;

		case Format::BC3:
			gl_fmt = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			m_int_format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			m_int_type = GL_UNSIGNED_BYTE;
			m_int_shift = 1;
			break;

		case Format::BC7:
			gl_fmt = GL_COMPRESSED_RGBA_BPTC_UNORM_ARB;
			m_int_format = GL_COMPRESSED_RGBA_BPTC_UNORM_ARB;
			m_int_type = GL_UNSIGNED_BYTE;
			m_int_shift = 1;
			break;

		case Format::Invalid:
			m_int_format = 0;
			m_int_type = 0;
			m_int_shift = 0;
			ASSERT(0);
	}

	// Only 32 bits input texture will be supported for mipmap
	if (m_type == Type::Texture)
		m_mipmap_levels = levels;

	// Create a gl object (texture isn't allocated here)
	glCreateTextures(GL_TEXTURE_2D, 1, &m_texture_id);
	if (m_format == Format::UNorm8)
	{
		// Emulate DX behavior, beside it avoid special code in shader to differentiate
		// palette texture from a GL_RGBA target or a GL_R texture.
		glTextureParameteri(m_texture_id, GL_TEXTURE_SWIZZLE_A, GL_RED);
	}

	static int every_512 = 0;
	GLState::available_vram -= GetMemUsage();
	if ((GLState::available_vram < 0) && (every_512 % 512 == 0))
	{
		fprintf(stderr, "Available VRAM is very low (%lld), a crash is expected! Enable conservative buffer allocation or reduce upscaling!\n", GLState::available_vram);
		every_512++;
		// Pull emergency break
		throw std::bad_alloc();
	}

	glTextureStorage2D(m_texture_id, m_mipmap_levels, gl_fmt, m_size.x, m_size.y);
}

GSTextureOGL::~GSTextureOGL()
{
	/* Unbind the texture from our local state */

	if (m_texture_id == GLState::rt)
		GLState::rt = 0;
	if (m_texture_id == GLState::ds)
		GLState::ds = 0;
	for (GLuint& tex : GLState::tex_unit)
	{
		if (m_texture_id == tex)
			tex = 0;
	}

	glDeleteTextures(1, &m_texture_id);

	GLState::available_vram += GetMemUsage();
}

void* GSTextureOGL::GetNativeHandle() const
{
	return reinterpret_cast<void*>(static_cast<uintptr_t>(m_texture_id));
}

void GSTextureOGL::Clear(const void* data)
{
	glClearTexImage(m_texture_id, GL_TEX_LEVEL_0, m_int_format, m_int_type, data);
}

void GSTextureOGL::Clear(const void* data, const GSVector4i& area)
{
	glClearTexSubImage(m_texture_id, GL_TEX_LEVEL_0, area.x, area.y, 0, area.width(), area.height(), 1, m_int_format, m_int_type, data);
}

bool GSTextureOGL::Update(const GSVector4i& r, const void* data, int pitch, int layer)
{
	ASSERT(m_type != Type::DepthStencil && m_type != Type::Offscreen);

	if (layer >= m_mipmap_levels)
		return true;

	// Default upload path for the texture is the Map/Unmap
	// This path is mostly used for palette. But also for texture that could
	// overflow the pbo buffer
	// Data upload is rather small typically 64B or 1024B. So don't bother with PBO
	// and directly send the data to the GL synchronously

	m_clean = false;

	u32 row_byte = r.width() << m_int_shift;
	u32 map_size = r.height() * row_byte;

#if 0
	if (r.height() == 1) {
		// Palette data. Transfer is small either 64B or 1024B.
		// Sometimes it is faster, sometimes slower.
		glTextureSubImage2D(m_texture_id, GL_TEX_LEVEL_0, r.x, r.y, r.width(), r.height(), m_int_format, m_int_type, data);
		return true;
	}
#endif

	GL_PUSH("Upload Texture %d", m_texture_id);
	g_perfmon.Put(GSPerfMon::TextureUploads, 1);

	// Don't use PBOs for huge texture uploads, let the driver sort it out.
	// Otherwise we'll just be syncing, or worse, crashing because the PBO routine above isn't great.
	if (IsCompressedFormat())
	{
		const u32 row_length = CalcUploadRowLengthFromPitch(pitch);
		const u32 upload_size = CalcUploadSize(r.height(), pitch);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length);
		glCompressedTextureSubImage2D(m_texture_id, layer, r.x, r.y, r.width(), r.height(), m_int_format, upload_size, data);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	}
	else if (GLLoader::buggy_pbo || map_size > GSDeviceOGL::GetTextureUploadBuffer()->GetChunkSize())
	{
		glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch >> m_int_shift);
		glTextureSubImage2D(m_texture_id, layer, r.x, r.y, r.width(), r.height(), m_int_format, m_int_type, data);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0); // Restore default behavior
	}
	else
	{
		GL::StreamBuffer* const sb = GSDeviceOGL::GetTextureUploadBuffer();

		const auto map = sb->Map(TEXTURE_UPLOAD_ALIGNMENT, map_size);
		StringUtil::StrideMemCpy(map.pointer, row_byte, data, pitch, row_byte, r.height());
		sb->Unmap(map_size);
		sb->Bind();

		glTextureSubImage2D(m_texture_id, layer, r.x, r.y, r.width(), r.height(), m_int_format, m_int_type,
			reinterpret_cast<void*>(static_cast<uintptr_t>(map.buffer_offset)));

		sb->Unbind();
	}

	m_needs_mipmaps_generated = true;

	return true;
}

bool GSTextureOGL::Map(GSMap& m, const GSVector4i* _r, int layer)
{
	if (layer >= m_mipmap_levels || IsCompressedFormat())
		return false;

	GSVector4i r = _r ? *_r : GSVector4i(0, 0, m_size.x, m_size.y);
	// Will need some investigation
	ASSERT(r.width() != 0);
	ASSERT(r.height() != 0);

	u32 row_byte = r.width() << m_int_shift;
	m.pitch = row_byte;

	if (m_type == Type::Texture || m_type == Type::RenderTarget)
	{
		const u32 map_size = r.height() * row_byte;
		if (GLLoader::buggy_pbo || map_size > GSDeviceOGL::GetTextureUploadBuffer()->GetChunkSize())
			return false;

		GL_PUSH_("Upload Texture %d", m_texture_id); // POP is in Unmap
		g_perfmon.Put(GSPerfMon::TextureUploads, 1);

		m_clean = false;

		const auto map = GSDeviceOGL::GetTextureUploadBuffer()->Map(TEXTURE_UPLOAD_ALIGNMENT, map_size);
		m.bits = static_cast<u8*>(map.pointer);

		// Save the area for the unmap
		m_r_x = r.x;
		m_r_y = r.y;
		m_r_w = r.width();
		m_r_h = r.height();
		m_layer = layer;
		m_map_offset = map.buffer_offset;

		return true;
	}

	return false;
}

void GSTextureOGL::Unmap()
{
	if (m_type == Type::Texture || m_type == Type::RenderTarget)
	{
		const u32 map_size = (m_r_w << m_int_shift) * m_r_h;
		GL::StreamBuffer* sb = GSDeviceOGL::GetTextureUploadBuffer();
		sb->Unmap(map_size);
		sb->Bind();

		glTextureSubImage2D(m_texture_id, m_layer, m_r_x, m_r_y, m_r_w, m_r_h, m_int_format, m_int_type,
			reinterpret_cast<void*>(static_cast<uintptr_t>(m_map_offset)));

		sb->Unbind();

		m_needs_mipmaps_generated = true;

		GL_POP(); // PUSH is in Map
	}
}

void GSTextureOGL::GenerateMipmap()
{
	ASSERT(m_mipmap_levels > 1);
	glGenerateTextureMipmap(m_texture_id);
}

GSTexture::GSMap GSTextureOGL::Read(const GSVector4i& r, AlignedBuffer<u8, 32>& buffer)
{
	GSMap m;
	m.pitch = r.width() << m_int_shift;
	buffer.MakeRoomFor(m.pitch * r.height());
	m.bits = buffer.GetPtr();

	// The fastest way will be to use a PBO to read the data asynchronously. Unfortunately GS
	// architecture is waiting the data right now.

	// Bind the texture to the read framebuffer to avoid any disturbance
	glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo_read);
	glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture_id, 0);

	// In case a target is 16 bits (GT4)
	glPixelStorei(GL_PACK_ALIGNMENT, 1u << m_int_shift);

	glReadPixels(r.x, r.y, r.width(), r.height(), m_int_format, m_int_type, m.bits);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	return m;
}

bool GSTextureOGL::Save(const std::string& fn)
{
	// Collect the texture data
	u32 pitch = 4 * m_size.x;
	u32 buf_size = pitch * m_size.y * 2; // Note *2 for security (depth/stencil)
	std::unique_ptr<u8[]> image(new u8[buf_size]);
#ifdef PCSX2_DEVBUILD
	GSPng::Format fmt = GSPng::RGB_A_PNG;
#else
	GSPng::Format fmt = GSPng::RGB_PNG;
#endif

	if (IsDepthStencil())
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo_read);

		glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_texture_id, 0);
		glReadPixels(0, 0, m_size.x, m_size.y, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, image.get());

		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

		fmt = GSPng::RGB_A_PNG;
	}
	else if (m_format == Format::PrimID)
	{
		// Note: 4.5 function used for accurate DATE
		glGetTextureImage(m_texture_id, 0, GL_RED_INTEGER, GL_INT, buf_size, image.get());

		fmt = GSPng::R32I_PNG;
	}
	else
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo_read);

		glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture_id, 0);

		if (m_format == Format::Color)
		{
			glReadPixels(0, 0, m_size.x, m_size.y, GL_RGBA, GL_UNSIGNED_BYTE, image.get());
		}
		else if (m_format == Format::UInt16)
		{
			glReadPixels(0, 0, m_size.x, m_size.y, GL_RED_INTEGER, GL_UNSIGNED_SHORT, image.get());
			fmt = GSPng::R16I_PNG;
		}
		else if (m_format == Format::UNorm8)
		{
			fmt = GSPng::R8I_PNG;
			glReadPixels(0, 0, m_size.x, m_size.y, GL_RED, GL_UNSIGNED_BYTE, image.get());
		}

		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	}

	return GSPng::Save(fmt, fn, image.get(), m_size.x, m_size.y, pitch, GSConfig.PNGCompressionLevel);
}

void GSTextureOGL::Swap(GSTexture* tex)
{
	GSTexture::Swap(tex);

	std::swap(m_texture_id, static_cast<GSTextureOGL*>(tex)->m_texture_id);
	std::swap(m_fbo_read, static_cast<GSTextureOGL*>(tex)->m_fbo_read);
	std::swap(m_clean, static_cast<GSTextureOGL*>(tex)->m_clean);
	std::swap(m_r_x, static_cast<GSTextureOGL*>(tex)->m_r_x);
	std::swap(m_r_x, static_cast<GSTextureOGL*>(tex)->m_r_y);
	std::swap(m_r_w, static_cast<GSTextureOGL*>(tex)->m_r_w);
	std::swap(m_r_h, static_cast<GSTextureOGL*>(tex)->m_r_h);
	std::swap(m_layer, static_cast<GSTextureOGL*>(tex)->m_layer);
	std::swap(m_int_format, static_cast<GSTextureOGL*>(tex)->m_int_format);
	std::swap(m_int_type, static_cast<GSTextureOGL*>(tex)->m_int_type);
	std::swap(m_int_shift, static_cast<GSTextureOGL*>(tex)->m_int_shift);
}
