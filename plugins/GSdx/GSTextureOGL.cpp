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

#include "stdafx.h"
#include <limits.h>
#include "GSTextureOGL.h"
#include "GLState.h"
#include "GSPng.h"

#ifdef ENABLE_OGL_DEBUG_MEM_BW
extern uint64 g_real_texture_upload_byte;
#endif

// FIXME find the optimal number of PBO
#define PBO_POOL_SIZE 8

// FIXME OGL4: investigate, only 1 unpack buffer always bound
namespace PboPool {

	GLuint m_pool[PBO_POOL_SIZE];
	uptr m_offset[PBO_POOL_SIZE];
	char*  m_map[PBO_POOL_SIZE];
	uint32 m_current_pbo = 0;
	uint32 m_size;
	GLsync m_fence[PBO_POOL_SIZE];
	const uint32 m_pbo_size = 8*1024*1024;

	// Option for buffer storage
	// XXX: actually does I really need coherent and barrier???
	// As far as I understand glTexSubImage2D is a client-server transfer so no need to make
	// the value visible to the server
	const GLbitfield common_flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;
	const GLbitfield map_flags = common_flags | GL_MAP_FLUSH_EXPLICIT_BIT;
	const GLbitfield create_flags = common_flags | GL_CLIENT_STORAGE_BIT;

	// Perf impact (test was only done on a gs dump):
	// Normal (fast): Message:Buffer detailed info: Buffer object 9 (bound to
	//	GL_PIXEL_UNPACK_BUFFER_ARB, usage hint is GL_STREAM_COPY) will use VIDEO
	//	memory as the source for buffer object operations.
	//
	// Persistent (slower): Message:Buffer detailed info: Buffer object 8
	//	(bound to GL_PIXEL_UNPACK_BUFFER_ARB, usage hint is GL_DYNAMIC_DRAW)
	//	will use DMA CACHED memory as the source for buffer object operations
	void Init() {
		glGenBuffers(countof(m_pool), m_pool);

		for (size_t i = 0; i < countof(m_pool); i++) {
			BindPbo();

			glBufferStorage(GL_PIXEL_UNPACK_BUFFER, m_pbo_size, NULL, create_flags);
			m_map[m_current_pbo] = (char*)glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, m_pbo_size, map_flags);
			m_fence[m_current_pbo] = 0;

			NextPbo();
		}
		UnbindPbo();
	}

	char* Map(uint32 size) {
		char* map;
		m_size = size;

		if (m_size > m_pbo_size) {
			fprintf(stderr, "BUG: PBO too small %d but need %d\n", m_pbo_size, m_size);
		}

		if (m_offset[m_current_pbo] + m_size >= m_pbo_size) {
			//NextPbo(); // For test purpose
			NextPboWithSync();
		}

		// Note: texsubimage will access currently bound buffer
		// Pbo ready let's get a pointer
		BindPbo();

		map = m_map[m_current_pbo] + m_offset[m_current_pbo];

		return map;
	}

	void Unmap() {
		glFlushMappedBufferRange(GL_PIXEL_UNPACK_BUFFER, m_offset[m_current_pbo], m_size);
	}

	uptr Offset() {
		return m_offset[m_current_pbo];
	}

	void Destroy() {
		for (size_t i = 0; i < countof(m_pool); i++) {
			m_map[i] = NULL;
			m_offset[i] = 0;
			glDeleteSync(m_fence[i]);

			// Don't know if we must do it
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pool[i]);
			glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
		}
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		glDeleteBuffers(countof(m_pool), m_pool);
	}

	void BindPbo() {
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pool[m_current_pbo]);
	}

	void NextPbo() {
		m_current_pbo = (m_current_pbo + 1) & (countof(m_pool)-1);
		// Mark new PBO as free
		m_offset[m_current_pbo] = 0;
	}

	void NextPboWithSync() {
		m_fence[m_current_pbo] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		NextPbo();
		if (m_fence[m_current_pbo]) {
#ifdef ENABLE_OGL_DEBUG_FENCE
			GLenum status = glClientWaitSync(m_fence[m_current_pbo], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
#else
			glClientWaitSync(m_fence[m_current_pbo], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
#endif
			glDeleteSync(m_fence[m_current_pbo]);
			m_fence[m_current_pbo] = 0;

#ifdef ENABLE_OGL_DEBUG_FENCE
			if (status != GL_ALREADY_SIGNALED) {
				fprintf(stderr, "GL_PIXEL_UNPACK_BUFFER: Sync Sync! Buffer too small\n");
			}
#endif
		}
	}

	void UnbindPbo() {
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	}

	void EndTransfer() {
		// Note: keep offset aligned for SSE/AVX
		m_offset[m_current_pbo] = (m_offset[m_current_pbo] + m_size + 63) & ~0x3F;
	}
}

GSTextureOGL::GSTextureOGL(int type, int w, int h, int format, GLuint fbo_read)
	: m_pbo_size(0), m_dirty(false), m_clean(false), m_local_buffer(NULL), m_r_x(0), m_r_y(0), m_r_w(0), m_r_h(0)
{
	// OpenGL didn't like dimensions of size 0
	m_size.x = max(1,w);
	m_size.y = max(1,h);
	m_format = format;
	m_type   = type;
	m_fbo_read = fbo_read;
	m_texture_id = 0;

	// Bunch of constant parameter
	switch (m_format) {
			// 1 Channel integer
		case GL_R32UI:
		case GL_R32I:
			m_int_format    = GL_RED_INTEGER;
			m_int_type      = (m_format == GL_R32UI) ? GL_UNSIGNED_INT : GL_INT;
			m_int_shift     = 2;
			break;
		case GL_R16UI:
			m_int_format    = GL_RED_INTEGER;
			m_int_type      = GL_UNSIGNED_SHORT;
			m_int_shift     = 1;
			break;

			// 1 Channel normalized
		case GL_R8:
			m_int_format    = GL_RED;
			m_int_type      = GL_UNSIGNED_BYTE;
			m_int_shift     = 0;
			break;

			// 4 channel normalized
		case GL_RGBA16:
			m_int_format    = GL_RGBA;
			m_int_type      = GL_UNSIGNED_SHORT;
			m_int_shift     = 3;
			break;
		case GL_RGBA8:
			m_int_format    = GL_RGBA;
			m_int_type      = GL_UNSIGNED_BYTE;
			m_int_shift     = 2;
			break;

			// 4 channel integer
		case GL_RGBA16I:
		case GL_RGBA16UI:
			m_int_format    = GL_RGBA_INTEGER;
			m_int_type      = (m_format == GL_R16UI) ? GL_UNSIGNED_SHORT : GL_SHORT;
			m_int_shift     = 3;
			break;

			// 4 channel float
		case GL_RGBA32F:
			m_int_format    = GL_RGBA;
			m_int_type      = GL_FLOAT;
			m_int_shift     = 4;
			break;
		case GL_RGBA16F:
			m_int_format    = GL_RGBA;
			m_int_type      = GL_HALF_FLOAT;
			m_int_shift     = 3;
			break;

			// Special
		case 0:
		case GL_DEPTH32F_STENCIL8:
			// Backbuffer & dss aren't important
			m_int_format    = 0;
			m_int_type      = 0;
			m_int_shift     = 0;
			break;

		default:
			m_int_format    = 0;
			m_int_type      = 0;
			m_int_shift     = 0;
			ASSERT(0);
	}

	// Generate & Allocate the buffer
	switch (m_type) {
		case GSTexture::Offscreen:
			// 8B is the worst case for depth/stencil
			// FIXME I think it is only used for color. So you can save half of the size
			m_local_buffer = (uint8*)_aligned_malloc(m_size.x * m_size.y * 4, 32);
		case GSTexture::Texture:
		case GSTexture::RenderTarget:
		case GSTexture::DepthStencil:
			glCreateTextures(GL_TEXTURE_2D, 1, &m_texture_id);
			glTextureStorage2D(m_texture_id, 1+GL_TEX_LEVEL_0, m_format, m_size.x, m_size.y);
			if (m_format == GL_R8) {
				// Emulate DX behavior, beside it avoid special code in shader to differentiate
				// palette texture from a GL_RGBA target or a GL_R texture.
				glTextureParameteri(m_texture_id, GL_TEXTURE_SWIZZLE_A, GL_RED);
			}
			break;
		case GSTexture::Backbuffer:
		default:
			break;
	}
}

GSTextureOGL::~GSTextureOGL()
{
	/* Unbind the texture from our local state */

	if (m_texture_id == GLState::rt)
		GLState::rt = 0;
	if (m_texture_id == GLState::ds)
		GLState::ds = 0;
	for (size_t i = 0; i < countof(GLState::tex_unit); i++) {
		if (m_texture_id == GLState::tex_unit[i])
			GLState::tex_unit[i] = 0;
	}

	glDeleteTextures(1, &m_texture_id);

	if (m_local_buffer)
		_aligned_free(m_local_buffer);
}

void GSTextureOGL::Invalidate()
{
	if (m_dirty && glInvalidateTexImage) {
		glInvalidateTexImage(m_texture_id, GL_TEX_LEVEL_0);
		m_dirty = false;
	}
}

bool GSTextureOGL::Update(const GSVector4i& r, const void* data, int pitch)
{
	ASSERT(m_type != GSTexture::DepthStencil && m_type != GSTexture::Offscreen);

	// Default upload path for the texture is the Map/Unmap
	// This path is mostly used for palette. But also for texture that could
	// overflow the pbo buffer
	// Data upload is rather small typically 64B or 1024B. So don't bother with PBO
	// and directly send the data to the GL synchronously

	m_dirty = true;
	m_clean = false;

	uint32 row_byte = r.width() << m_int_shift;
	uint32 map_size = r.height() * row_byte;
#ifdef ENABLE_OGL_DEBUG_MEM_BW
	g_real_texture_upload_byte += map_size;
#endif

#if 0
	if (r.height() == 1) {
		// Palette data. Transfer is small either 64B or 1024B.
		// Sometimes it is faster, sometimes slower.
		glTextureSubImage2D(m_texture_id, GL_TEX_LEVEL_0, r.x, r.y, r.width(), r.height(), m_int_format, m_int_type, data);
		return true;
	}
#endif

	GL_PUSH("Upload Texture %d", m_texture_id);

	// The easy solution without PBO
#if 0
	// Likely a bad texture
	glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch >> m_int_shift);

	glTextureSubImage2D(m_texture_id, GL_TEX_LEVEL_0, r.x, r.y, r.width(), r.height(), m_int_format, m_int_type, data);

	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0); // Restore default behavior
#endif

	// The complex solution with PBO
#if 1
	char* src = (char*)data;
	char* map = PboPool::Map(map_size);

	// PERF: slow path of the texture upload. Dunno if we could do better maybe check if TC can keep row_byte == pitch
	// Note: row_byte != pitch
	for (int h = 0; h < r.height(); h++) {
		memcpy(map, src, row_byte);
		map += row_byte;
		src += pitch;
	}

	PboPool::Unmap();

	glTextureSubImage2D(m_texture_id, GL_TEX_LEVEL_0, r.x, r.y, r.width(), r.height(), m_int_format, m_int_type, (const void*)PboPool::Offset());

	// FIXME OGL4: investigate, only 1 unpack buffer always bound
	PboPool::UnbindPbo();

	PboPool::EndTransfer();
#endif

	GL_POP();

	return true;
}

bool GSTextureOGL::Map(GSMap& m, const GSVector4i* _r)
{
	GSVector4i r = _r ? *_r : GSVector4i(0, 0, m_size.x, m_size.y);

	// LOTS OF CRAP CODE!!!! PLEASE FIX ME !!!
	if (m_type == GSTexture::Offscreen) {
		// The fastest way will be to use a PBO to read the data asynchronously. Unfortunately GSdx
		// architecture is waiting the data right now.

#if 0
		// Maybe it is as good as the code below. I don't know
		// With openGL 4.5 you can use glGetTextureSubImage

		glGetTextureSubImage(m_texture_id, GL_TEX_LEVEL_0, r.x, r.y, 0, r.width(), r.height(), 0, m_int_format, m_int_type, m_size.x * m_size.y * 4, m_local_buffer);
#else

		// Bind the texture to the read framebuffer to avoid any disturbance
		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo_read);
		glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture_id, 0);

		glReadPixels(r.x, r.y, r.width(), r.height(), m_int_format, m_int_type, m_local_buffer);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

#endif

		m.bits = m_local_buffer;
		m.pitch = m_size.x << m_int_shift;

		return true;
	} else if (m_type == GSTexture::Texture || m_type == GSTexture::RenderTarget) {
		GL_PUSH("Upload Texture %d", m_texture_id); // POP is in Unmap

		m_dirty = true;
		m_clean = false;

		uint32 row_byte = r.width() << m_int_shift;
		uint32 map_size = r.height() * row_byte;

		m.bits = (uint8*)PboPool::Map(map_size);
		m.pitch = row_byte;

#ifdef ENABLE_OGL_DEBUG_MEM_BW
	g_real_texture_upload_byte += map_size;
#endif

		// Save the area for the unmap
		m_r_x = r.x;
		m_r_y = r.y;
		m_r_w = r.width();
		m_r_h = r.height();

		return true;
	}

	return false;
}

void GSTextureOGL::Unmap()
{
	if (m_type == GSTexture::Texture || m_type == GSTexture::RenderTarget) {

		PboPool::Unmap();

		glTextureSubImage2D(m_texture_id, GL_TEX_LEVEL_0, m_r_x, m_r_y, m_r_w, m_r_h, m_int_format, m_int_type, (const void*)PboPool::Offset());

		// FIXME OGL4: investigate, only 1 unpack buffer always bound
		PboPool::UnbindPbo();

		PboPool::EndTransfer();

		GL_POP(); // PUSH is in Map
	}
}

bool GSTextureOGL::Save(const string& fn, bool user_image, bool dds)
{
	// Collect the texture data
	uint32 pitch = 4 * m_size.x;
	uint32 buf_size = pitch * m_size.y * 2;// Note *2 for security (depth/stencil)
	std::unique_ptr<uint8[]> image(new uint8[buf_size]);
#ifdef ENABLE_OGL_DEBUG
	GSPng::Format fmt = GSPng::RGB_A_PNG;
#else
	GSPng::Format fmt = GSPng::RGB_PNG;
#endif

	if (IsBackbuffer()) {
		glReadPixels(0, 0, m_size.x, m_size.y, GL_RGBA, GL_UNSIGNED_BYTE, image.get());
	} else if(IsDss()) {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo_read);

		glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_texture_id, 0);
		glReadPixels(0, 0, m_size.x, m_size.y, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, image.get());

		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

		fmt = GSPng::RGB_A_PNG;
	} else if(m_format == GL_R32I) {
		glGetTextureImage(m_texture_id, 0, GL_RED_INTEGER, GL_INT, buf_size, image.get());

		fmt = GSPng::R32I_PNG;
	} else {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo_read);

		glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture_id, 0);

		if (m_format == GL_RGBA8) {
			glReadPixels(0, 0, m_size.x, m_size.y, GL_RGBA, GL_UNSIGNED_BYTE, image.get());
		}
		else if (m_format == GL_R16UI)
		{
			glReadPixels(0, 0, m_size.x, m_size.y, GL_RED_INTEGER, GL_UNSIGNED_SHORT, image.get());
			fmt = GSPng::R16I_PNG;
		}
		else if (m_format == GL_R8)
		{
			fmt = GSPng::R8I_PNG;
			glReadPixels(0, 0, m_size.x, m_size.y, GL_RED, GL_UNSIGNED_BYTE, image.get());
		}

		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	}

	int compression = user_image ? Z_BEST_COMPRESSION : theApp.GetConfig("png_compression_level", Z_BEST_SPEED);
	return GSPng::Save(fmt, fn, image.get(), m_size.x, m_size.y, pitch, compression);
}

uint32 GSTextureOGL::GetMemUsage()
{
	switch (m_type) {
		case GSTexture::Offscreen:
			return m_size.x * m_size.y * (4 + 4); // Texture + buffer
		case GSTexture::Texture:
		case GSTexture::RenderTarget:
			return m_size.x * m_size.y * 4;
		case GSTexture::DepthStencil:
			return m_size.x * m_size.y * 8;
		case GSTexture::Backbuffer:
		default:
			return 0;
	}
}
