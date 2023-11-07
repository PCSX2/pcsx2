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
#include "GS/GSExtra.h"
#include "GS/GSPerfMon.h"
#include "GS/GSGL.h"
#include "common/BitUtils.h"
#include "common/AlignedMalloc.h"
#include "common/StringUtil.h"

// Looking across a range of GPUs, the optimal copy alignment for Vulkan drivers seems
// to be between 1 (AMD/NV) and 64 (Intel). So, we'll go with 64 here.
static constexpr u32 TEXTURE_UPLOAD_ALIGNMENT = 64;

// The pitch alignment must be less or equal to the upload alignment.
// We need 32 here for AVX2, so 64 is also fine.
static constexpr u32 TEXTURE_UPLOAD_PITCH_ALIGNMENT = 64;

GSTextureOGL::GSTextureOGL(Type type, int width, int height, int levels, Format format)
{
	// OpenGL didn't like dimensions of size 0
	m_size.x = std::max(1, width);
	m_size.y = std::max(1, height);
	m_format = format;
	m_type = type;
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
			if (!g_gs_device->Features().framebuffer_fetch)
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

	glTextureStorage2D(m_texture_id, m_mipmap_levels, gl_fmt, m_size.x, m_size.y);
}

GSTextureOGL::~GSTextureOGL()
{
	// Textures aren't cleared from attachments on deletion.
	GSDeviceOGL::GetInstance()->OMUnbindTexture(this);

	// But they are unbound.
	for (GLuint& tex : GLState::tex_unit)
	{
		if (m_texture_id == tex)
			tex = 0;
	}

	glDeleteTextures(1, &m_texture_id);
}

void* GSTextureOGL::GetNativeHandle() const
{
	return reinterpret_cast<void*>(static_cast<uintptr_t>(m_texture_id));
}

bool GSTextureOGL::Update(const GSVector4i& r, const void* data, int pitch, int layer)
{
	ASSERT(m_type != Type::DepthStencil);

	if (layer >= m_mipmap_levels)
		return true;

	// Default upload path for the texture is the Map/Unmap
	// This path is mostly used for palette. But also for texture that could
	// overflow the pbo buffer
	// Data upload is rather small typically 64B or 1024B. So don't bother with PBO
	// and directly send the data to the GL synchronously
	GSDeviceOGL::GetInstance()->CommitClear(this, true);

	const u32 preferred_pitch = Common::AlignUpPow2(r.width() << m_int_shift, TEXTURE_UPLOAD_PITCH_ALIGNMENT);
	const u32 map_size = r.height() * preferred_pitch;

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
	GLStreamBuffer* const sb = GSDeviceOGL::GetInstance()->GetTextureUploadBuffer();
	if (IsCompressedFormat())
	{
		const u32 row_length = CalcUploadRowLengthFromPitch(pitch);
		const u32 upload_size = CalcUploadSize(r.height(), pitch);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length);
		glCompressedTextureSubImage2D(m_texture_id, layer, r.x, r.y, r.width(), r.height(), m_int_format, upload_size, data);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	}
	else if (!sb || map_size > sb->GetChunkSize())
	{
		glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch >> m_int_shift);
		glTextureSubImage2D(m_texture_id, layer, r.x, r.y, r.width(), r.height(), m_int_format, m_int_type, data);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0); // Restore default behavior
	}
	else
	{
		const auto map = sb->Map(TEXTURE_UPLOAD_ALIGNMENT, map_size);
		StringUtil::StrideMemCpy(map.pointer, preferred_pitch, data, pitch, r.width() << m_int_shift, r.height());
		sb->Unmap(map_size);
		sb->Bind();

		const u32 row_length = CalcUploadRowLengthFromPitch(preferred_pitch);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length);

		glTextureSubImage2D(m_texture_id, layer, r.x, r.y, r.width(), r.height(), m_int_format, m_int_type,
			reinterpret_cast<void*>(static_cast<uintptr_t>(map.buffer_offset)));

		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

		sb->Unbind();
	}

	m_needs_mipmaps_generated = true;

	return true;
}

bool GSTextureOGL::Map(GSMap& m, const GSVector4i* _r, int layer)
{
	if (layer >= m_mipmap_levels || IsCompressedFormat())
		return false;

	GSDeviceOGL::GetInstance()->CommitClear(this, true);

	GSVector4i r = _r ? *_r : GSVector4i(0, 0, m_size.x, m_size.y);
	// Will need some investigation
	ASSERT(r.width() != 0);
	ASSERT(r.height() != 0);

	const u32 pitch = Common::AlignUpPow2(r.width() << m_int_shift, TEXTURE_UPLOAD_PITCH_ALIGNMENT);
	m.pitch = pitch;

	if (m_type == Type::Texture || m_type == Type::RenderTarget)
	{
		const u32 upload_size = CalcUploadSize(r.height(), pitch);
		GLStreamBuffer* sb = GSDeviceOGL::GetInstance()->GetTextureUploadBuffer();
		if (!sb || upload_size > sb->GetChunkSize())
			return false;

		GL_PUSH_("Upload Texture %d", m_texture_id); // POP is in Unmap
		g_perfmon.Put(GSPerfMon::TextureUploads, 1);

		const auto map = sb->Map(TEXTURE_UPLOAD_ALIGNMENT, upload_size);
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
		GSDeviceOGL::GetInstance()->CommitClear(this, true);

		const u32 pitch = Common::AlignUpPow2(m_r_w << m_int_shift, TEXTURE_UPLOAD_PITCH_ALIGNMENT);
		const u32 upload_size = pitch * m_r_h;
		GLStreamBuffer* sb = GSDeviceOGL::GetInstance()->GetTextureUploadBuffer();
		sb->Unmap(upload_size);
		sb->Bind();

		const u32 row_length = CalcUploadRowLengthFromPitch(pitch);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length);

		glTextureSubImage2D(m_texture_id, m_layer, m_r_x, m_r_y, m_r_w, m_r_h, m_int_format, m_int_type,
			reinterpret_cast<void*>(static_cast<uintptr_t>(m_map_offset)));

		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

		sb->Unbind();

		m_needs_mipmaps_generated = true;

		GL_POP(); // PUSH is in Map
	}
}

void GSTextureOGL::GenerateMipmap()
{
	ASSERT(m_mipmap_levels > 1);
	GSDeviceOGL::GetInstance()->CommitClear(this, true);
	glGenerateTextureMipmap(m_texture_id);
}

void GSTextureOGL::Swap(GSTexture* tex)
{
	GSTexture::Swap(tex);

	std::swap(m_texture_id, static_cast<GSTextureOGL*>(tex)->m_texture_id);
	std::swap(m_r_x, static_cast<GSTextureOGL*>(tex)->m_r_x);
	std::swap(m_r_x, static_cast<GSTextureOGL*>(tex)->m_r_y);
	std::swap(m_r_w, static_cast<GSTextureOGL*>(tex)->m_r_w);
	std::swap(m_r_h, static_cast<GSTextureOGL*>(tex)->m_r_h);
	std::swap(m_layer, static_cast<GSTextureOGL*>(tex)->m_layer);
	std::swap(m_int_format, static_cast<GSTextureOGL*>(tex)->m_int_format);
	std::swap(m_int_type, static_cast<GSTextureOGL*>(tex)->m_int_type);
	std::swap(m_int_shift, static_cast<GSTextureOGL*>(tex)->m_int_shift);
}

GSDownloadTextureOGL::GSDownloadTextureOGL(u32 width, u32 height, GSTexture::Format format)
	: GSDownloadTexture(width, height, format)
{
}

GSDownloadTextureOGL::~GSDownloadTextureOGL()
{
	if (m_buffer_id != 0)
	{
		if (m_sync)
			glDeleteSync(m_sync);

		if (m_map_pointer)
		{
			glBindBuffer(GL_PIXEL_PACK_BUFFER, m_buffer_id);
			glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
			glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		}

		glDeleteBuffers(1, &m_buffer_id);
	}
	else if (m_cpu_buffer)
	{
		_aligned_free(m_cpu_buffer);
	}
}

std::unique_ptr<GSDownloadTextureOGL> GSDownloadTextureOGL::Create(u32 width, u32 height, GSTexture::Format format)
{
	const u32 buffer_size = GetBufferSize(width, height, format, TEXTURE_UPLOAD_PITCH_ALIGNMENT);

	const bool use_buffer_storage = (GLAD_GL_VERSION_4_4 || GLAD_GL_ARB_buffer_storage || GLAD_GL_EXT_buffer_storage) &&
									!GSDeviceOGL::GetInstance()->IsDownloadPBODisabled();
	if (use_buffer_storage)
	{
		GLuint buffer_id;
		glGenBuffers(1, &buffer_id);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer_id);

		const u32 flags = GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
		const u32 map_flags = GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT;

		if (GLAD_GL_VERSION_4_4 || GLAD_GL_ARB_buffer_storage)
			glBufferStorage(GL_PIXEL_PACK_BUFFER, buffer_size, nullptr, flags);
		else if (GLAD_GL_EXT_buffer_storage)
			glBufferStorageEXT(GL_PIXEL_PACK_BUFFER, buffer_size, nullptr, flags);

		u8* buffer_map = static_cast<u8*>(glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, buffer_size, map_flags));

		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

		if (!buffer_map)
		{
			Console.Error("Failed to map persistent download buffer");
			glDeleteBuffers(1, &buffer_id);
			return {};
		}

		std::unique_ptr<GSDownloadTextureOGL> ret(new GSDownloadTextureOGL(width, height, format));
		ret->m_buffer_id = buffer_id;
		ret->m_buffer_size = buffer_size;
		ret->m_map_pointer = buffer_map;
		return ret;
	}

	// Fallback to glReadPixels() + CPU buffer.
	u8* cpu_buffer = static_cast<u8*>(_aligned_malloc(buffer_size, VECTOR_ALIGNMENT));
	if (!cpu_buffer)
		return {};

	std::unique_ptr<GSDownloadTextureOGL> ret(new GSDownloadTextureOGL(width, height, format));
	ret->m_cpu_buffer = cpu_buffer;
	ret->m_map_pointer = cpu_buffer;
	return ret;
}

void GSDownloadTextureOGL::CopyFromTexture(
	const GSVector4i& drc, GSTexture* stex, const GSVector4i& src, u32 src_level, bool use_transfer_pitch)
{
	GSTextureOGL* const glTex = static_cast<GSTextureOGL*>(stex);
	GSDeviceOGL::GetInstance()->CommitClear(glTex, true);

	pxAssert(glTex->GetFormat() == m_format);
	pxAssert(drc.width() == src.width() && drc.height() == src.height());
	pxAssert(src.z <= glTex->GetWidth() && src.w <= glTex->GetHeight());
	pxAssert(static_cast<u32>(drc.z) <= m_width && static_cast<u32>(drc.w) <= m_height);
	pxAssert(src_level < static_cast<u32>(glTex->GetMipmapLevels()));
	pxAssert((drc.left == 0 && drc.top == 0) || !use_transfer_pitch);

	u32 copy_offset, copy_size, copy_rows;
	m_current_pitch = GetTransferPitch(use_transfer_pitch ? static_cast<u32>(drc.width()) : m_width, TEXTURE_UPLOAD_PITCH_ALIGNMENT);
	GetTransferSize(drc, &copy_offset, &copy_size, &copy_rows);
	g_perfmon.Put(GSPerfMon::Readbacks, 1);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, GSDeviceOGL::GetInstance()->GetFBORead());
	glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glTex->GetID(), 0);

	glPixelStorei(GL_PACK_ALIGNMENT, 1u << glTex->GetIntShift());
	glPixelStorei(GL_PACK_ROW_LENGTH, GSTexture::CalcUploadRowLengthFromPitch(m_format, m_current_pitch));

	if (!m_cpu_buffer)
	{
		// Read to PBO.
		glBindBuffer(GL_PIXEL_PACK_BUFFER, m_buffer_id);
	}

	glReadPixels(src.left, src.top, src.width(), src.height(), glTex->GetIntFormat(), glTex->GetIntType(), m_cpu_buffer + copy_offset);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	if (m_cpu_buffer)
	{
		// If using CPU buffers, we never need to flush.
		m_needs_flush = false;
	}
	else
	{
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

		// Create a sync object so we know when the GPU is done copying.
		if (m_sync)
			glDeleteSync(m_sync);

		m_sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		m_needs_flush = true;
	}

	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
}

bool GSDownloadTextureOGL::Map(const GSVector4i& read_rc)
{
	// Either always mapped, or CPU buffer.
	return true;
}

void GSDownloadTextureOGL::Unmap()
{
	// Either always mapped, or CPU buffer.
}

void GSDownloadTextureOGL::Flush()
{
	// If we're using CPU buffers, we did the readback synchronously...
	if (!m_needs_flush || !m_sync)
		return;

	m_needs_flush = false;

	glClientWaitSync(m_sync, GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
	glDeleteSync(m_sync);
	m_sync = {};
}
