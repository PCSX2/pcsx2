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
	uint32 m_offset[PBO_POOL_SIZE];
	char*  m_map[PBO_POOL_SIZE];
	uint32 m_current_pbo = 0;
	uint32 m_size;
	bool   m_texture_storage;
	GLsync m_fence[PBO_POOL_SIZE];
	const uint32 m_pbo_size = 4*1024*1024;

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
		gl_GenBuffers(countof(m_pool), m_pool);
		m_texture_storage  = GLLoader::found_GL_ARB_buffer_storage;
		// Code is really faster on MT driver. So far only nvidia support it
		if (!(GLLoader::nvidia_buggy_driver && theApp.GetConfig("enable_nvidia_multi_thread", 1)))
			m_texture_storage  &= (theApp.GetConfig("ogl_texture_storage", 0) == 1);

		for (size_t i = 0; i < countof(m_pool); i++) {
			BindPbo();

			if (m_texture_storage) {
				gl_BufferStorage(GL_PIXEL_UNPACK_BUFFER, m_pbo_size, NULL, create_flags);
				m_map[m_current_pbo] = (char*)gl_MapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, m_pbo_size, map_flags);
				m_fence[m_current_pbo] = 0;
			} else {
				gl_BufferData(GL_PIXEL_UNPACK_BUFFER, m_pbo_size, NULL, GL_STREAM_COPY);
				m_map[m_current_pbo] = NULL;
			}

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

		if (m_texture_storage) {
			if (m_offset[m_current_pbo] + m_size >= m_pbo_size) {
				//NextPbo(); // For test purpose
				NextPboWithSync();
			}

			// Note: texsubimage will access currently bound buffer
			// Pbo ready let's get a pointer
			BindPbo();

			map = m_map[m_current_pbo] + m_offset[m_current_pbo];

		} else {
			GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_INVALIDATE_RANGE_BIT;

			if (m_offset[m_current_pbo] + m_size >= m_pbo_size) {
				NextPbo();

				flags &= ~GL_MAP_INVALIDATE_RANGE_BIT;
				flags |= GL_MAP_INVALIDATE_BUFFER_BIT;
			}

			// Pbo ready let's get a pointer
			BindPbo();

			// Be sure the map is aligned
			map = (char*)gl_MapBufferRange(GL_PIXEL_UNPACK_BUFFER, m_offset[m_current_pbo], m_size, flags);
		}

		return map;
	}

	void Unmap() {
		if (m_texture_storage) {
			gl_FlushMappedBufferRange(GL_PIXEL_UNPACK_BUFFER, m_offset[m_current_pbo], m_size);
		} else {
			gl_UnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
		}
	}

	uint32 Offset() {
		return m_offset[m_current_pbo];
	}

	void Destroy() {
		if (m_texture_storage) {
			for (size_t i = 0; i < countof(m_pool); i++) {
				m_map[i] = NULL;
				m_offset[i] = 0;
				gl_DeleteSync(m_fence[i]);

				// Don't know if we must do it
				gl_BindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pool[i]);
				gl_UnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
			}
			gl_BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		}
		gl_DeleteBuffers(countof(m_pool), m_pool);
	}

	void BindPbo() {
		gl_BindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pool[m_current_pbo]);
	}

	void NextPbo() {
		m_current_pbo = (m_current_pbo + 1) & (countof(m_pool)-1);
		// Mark new PBO as free
		m_offset[m_current_pbo] = 0;
	}

	void NextPboWithSync() {
		m_fence[m_current_pbo] = gl_FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		NextPbo();
		if (m_fence[m_current_pbo]) {
#ifdef ENABLE_OGL_DEBUG_FENCE
			GLenum status = gl_ClientWaitSync(m_fence[m_current_pbo], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
#else
			gl_ClientWaitSync(m_fence[m_current_pbo], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
#endif
			gl_DeleteSync(m_fence[m_current_pbo]);
			m_fence[m_current_pbo] = 0;

#ifdef ENABLE_OGL_DEBUG_FENCE
			if (status != GL_ALREADY_SIGNALED) {
				fprintf(stderr, "GL_PIXEL_UNPACK_BUFFER: Sync Sync! Buffer too small\n");
			}
#endif
		}
	}

	void UnbindPbo() {
		gl_BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	}

	void EndTransfer() {
		// Note: keep offset aligned for SSE/AVX
		m_offset[m_current_pbo] = (m_offset[m_current_pbo] + m_size + 63) & ~0x3F;
	}
}

// FIXME: check if it possible to always use those setup by default
// glPixelStorei(GL_PACK_ALIGNMENT, 1);
// glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

GSTextureOGL::GSTextureOGL(int type, int w, int h, int format, GLuint fbo_read)
	: m_pbo_size(0), m_dirty(false), m_clean(false), m_local_buffer(NULL)
{
	// OpenGL didn't like dimensions of size 0
	m_size.x = max(1,w);
	m_size.y = max(1,h);
	m_format = format;
	m_type   = type;
	m_fbo_read = fbo_read;
	m_texture_id = 0;
	memset(&m_handles, 0, countof(m_handles) * sizeof(m_handles[0]) );

	// Bunch of constant parameter
	switch (m_format) {
		case GL_R32UI:
		case GL_R32I:
			m_int_format    = GL_RED_INTEGER;
			m_int_type      = (m_format == GL_R32UI) ? GL_UNSIGNED_INT : GL_INT;
			m_int_alignment = 4;
			m_int_shift     = 2;
			break;
		case GL_R16UI:
			m_int_format    = GL_RED_INTEGER;
			m_int_type      = GL_UNSIGNED_SHORT;
			m_int_alignment = 2;
			m_int_shift     = 1;
			break;
		case GL_RGBA8:
			m_int_format    = GL_RGBA;
			m_int_type      = GL_UNSIGNED_BYTE;
			m_int_alignment = 4;
			m_int_shift     = 2;
			break;
		case GL_R8:
			m_int_format    = GL_RED;
			m_int_type      = GL_UNSIGNED_BYTE;
			m_int_alignment = 1;
			m_int_shift     = 0;
			break;
		case 0:
		case GL_DEPTH32F_STENCIL8:
			// Backbuffer & dss aren't important
			m_int_format    = 0;
			m_int_type      = 0;
			m_int_alignment = 0;
			m_int_shift     = 0;
			break;
		default:
			ASSERT(0);
	}

	// Generate & Allocate the buffer
	switch (m_type) {
		case GSTexture::Offscreen:
			// 8B is the worst case for depth/stencil
			m_local_buffer = (uint8*)_aligned_malloc(m_size.x * m_size.y * 8, 32);
		case GSTexture::Texture:
		case GSTexture::RenderTarget:
		case GSTexture::DepthStencil:
			gl_CreateTextures(GL_TEXTURE_2D, 1, &m_texture_id);
			gl_TextureStorage2D(m_texture_id, 1+GL_TEX_LEVEL_0, m_format, m_size.x, m_size.y);
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
	if (m_dirty && gl_InvalidateTexImage) {
		gl_InvalidateTexImage(m_texture_id, GL_TEX_LEVEL_0);
		m_dirty = false;
	}
}

bool GSTextureOGL::Update(const GSVector4i& r, const void* data, int pitch)
{
	ASSERT(m_type != GSTexture::DepthStencil && m_type != GSTexture::Offscreen);
	GL_PUSH("Upload Texture %d", m_texture_id);

	m_dirty = true;
	m_clean = false;

	glPixelStorei(GL_UNPACK_ALIGNMENT, m_int_alignment);

	char* src = (char*)data;
	uint32 row_byte = r.width() << m_int_shift;
	uint32 map_size = r.height() * row_byte;
	char* map = PboPool::Map(map_size);

#ifdef ENABLE_OGL_DEBUG_MEM_BW
	g_real_texture_upload_byte += map_size;
#endif

	// Note: row_byte != pitch
	for (int h = 0; h < r.height(); h++) {
		memcpy(map, src, row_byte);
		map += row_byte;
		src += pitch;
	}

	PboPool::Unmap();

	gl_TextureSubImage2D(m_texture_id, GL_TEX_LEVEL_0, r.x, r.y, r.width(), r.height(), m_int_format, m_int_type, (const void*)PboPool::Offset());

	// FIXME OGL4: investigate, only 1 unpack buffer always bound
	PboPool::UnbindPbo();

	PboPool::EndTransfer();

	GL_POP();
	return true;

	// For reference, standard upload without pbo (Used to crash on FGLRX)
#if 0
	// pitch is in byte wherease GL_UNPACK_ROW_LENGTH is in pixel
	glPixelStorei(GL_UNPACK_ALIGNMENT, m_int_alignment);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch >> m_int_shift);

	gl_TextureSubImage2D(m_texture_id, GL_TEX_LEVEL_0, r.x, r.y, r.width(), r.height(), m_int_format, m_int_type, data);

	// FIXME useful?
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0); // Restore default behavior

	return true;
#endif
}

GLuint64 GSTextureOGL::GetHandle(GLuint sampler_id)
{
	ASSERT(sampler_id < 12);
	if (!m_handles[sampler_id]) {
		m_handles[sampler_id] = gl_GetTextureSamplerHandleARB(m_texture_id, sampler_id);
		gl_MakeTextureHandleResidentARB(m_handles[sampler_id]);
	}

	return m_handles[sampler_id];
}

bool GSTextureOGL::Map(GSMap& m, const GSVector4i* r)
{
	// LOTS OF CRAP CODE!!!! PLEASE FIX ME !!!
	if (m_type != GSTexture::Offscreen) return false;

	// The fastest way will be to use a PBO to read the data asynchronously. Unfortunately GSdx
	// architecture is waiting the data right now.

#if 0
	// Maybe it is as good as the code below. I don't know

	gl_GetTextureImage(m_texture_id, GL_TEX_LEVEL_0, m_int_format, m_int_type, 1024*1024*16, m_local_buffer);

#else

	// Bind the texture to the read framebuffer to avoid any disturbance
	gl_BindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo_read);
	gl_FramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture_id, 0);

	glPixelStorei(GL_PACK_ALIGNMENT, m_int_alignment);
	glReadPixels(0, 0, m_size.x, m_size.y, m_int_format, m_int_type, m_local_buffer);
	gl_BindFramebuffer(GL_READ_FRAMEBUFFER, 0);

#endif

	m.bits = m_local_buffer;
	m.pitch = m_size.x << m_int_shift;

	return true;
}

void GSTextureOGL::Unmap()
{
}

#ifndef _WINDOWS

#pragma pack(push, 1)

struct BITMAPFILEHEADER
{
	uint16 bfType;
	uint32 bfSize;
	uint16 bfReserved1;
	uint16 bfReserved2;
	uint32 bfOffBits;
};

struct BITMAPINFOHEADER
{
	uint32 biSize;
	int32 biWidth;
	int32 biHeight;
	uint16 biPlanes;
	uint16 biBitCount;
	uint32 biCompression;
	uint32 biSizeImage;
	int32 biXPelsPerMeter;
	int32 biYPelsPerMeter;
	uint32 biClrUsed;
	uint32 biClrImportant;
};

#define BI_RGB 0

#pragma pack(pop)

#endif
void GSTextureOGL::Save(const string& fn, const void* image, uint32 pitch)
{
	// Build a BMP file
	FILE* fp = fopen(fn.c_str(), "wb");
	if (fp == NULL)
		return;

	BITMAPINFOHEADER bih;

	memset(&bih, 0, sizeof(bih));

	bih.biSize = sizeof(bih);
	bih.biWidth = m_size.x;
	bih.biHeight = m_size.y;
	bih.biPlanes = 1;
	bih.biBitCount = 32;
	bih.biCompression = BI_RGB;
	bih.biSizeImage = m_size.x * m_size.y << 2;

	BITMAPFILEHEADER bfh;

	memset(&bfh, 0, sizeof(bfh));

	uint8* bfType = (uint8*)&bfh.bfType;

	// bfh.bfType = 'MB';
	bfType[0] = 0x42;
	bfType[1] = 0x4d;
	bfh.bfOffBits = sizeof(bfh) + sizeof(bih);
	bfh.bfSize = bfh.bfOffBits + bih.biSizeImage;
	bfh.bfReserved1 = bfh.bfReserved2 = 0;

	fwrite(&bfh, 1, sizeof(bfh), fp);
	fwrite(&bih, 1, sizeof(bih), fp);

	uint8* data = (uint8*)image + (m_size.y - 1) * pitch;

	for(int h = m_size.y; h > 0; h--, data -= pitch)
	{
		if (false && IsDss()) {
			// Only get the depth and convert it to an integer
			uint8* better_data = data;
			for (int w = m_size.x; w > 0; w--, better_data += 8) {
				float* input = (float*)better_data;
				// FIXME how to dump 32 bits value into 8bits component color
				GLuint depth_integer = (GLuint)(*input * (float)UINT_MAX);
				uint8 r = (depth_integer >>  0) & 0xFF;
				uint8 g = (depth_integer >>  8) & 0xFF;
				uint8 b = (depth_integer >> 16) & 0xFF;
				uint8 a = (depth_integer >> 24) & 0xFF;

				fwrite(&r, 1, 1, fp);
				fwrite(&g, 1, 1, fp);
				fwrite(&b, 1, 1, fp);
				fwrite(&a, 1, 1, fp);
			}
		} else {
			// swap red and blue
			uint8* better_data = data;
			for (int w = m_size.x; w > 0; w--, better_data += 4) {
				uint8 red = better_data[2];
				better_data[2] = better_data[0];
				better_data[0] = red;
				fwrite(better_data, 1, 4, fp);
			}
		}
	}

	fclose(fp);
}

bool GSTextureOGL::Save(const string& fn, bool dds)
{
	// Collect the texture data
	uint32 pitch = 4 * m_size.x;
	uint32 buf_size = pitch * m_size.y * 2;// Note *2 for security (depth/stencil)
	char* image = (char*)malloc(buf_size);
	bool status = true;
#ifdef ENABLE_OGL_DEBUG
	GSPng::Format fmt = GSPng::RGB_A_PNG;
#else
	GSPng::Format fmt = GSPng::RGB_PNG;
#endif

	if (IsBackbuffer()) {
		glReadPixels(0, 0, m_size.x, m_size.y, GL_RGBA, GL_UNSIGNED_BYTE, image);
	} else if(IsDss()) {
		gl_BindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo_read);

		gl_FramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_texture_id, 0);
		glReadPixels(0, 0, m_size.x, m_size.y, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, image);

		gl_BindFramebuffer(GL_READ_FRAMEBUFFER, 0);

		fmt = GSPng::DEPTH_PNG;
	} else if(m_format == GL_R32I) {
		gl_GetTextureImage(m_texture_id, 0, GL_RED_INTEGER, GL_INT, buf_size, image);

		fmt = GSPng::R32I_PNG;

		// Not supported in Save function
		status = false;

	} else {
		gl_BindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo_read);

		gl_FramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture_id, 0);

		if (m_format == GL_RGBA8) {
			glReadPixels(0, 0, m_size.x, m_size.y, GL_RGBA, GL_UNSIGNED_BYTE, image);
		}
		else if (m_format == GL_R16UI)
		{
			glReadPixels(0, 0, m_size.x, m_size.y, GL_RED_INTEGER, GL_UNSIGNED_SHORT, image);
			fmt = GSPng::R16I_PNG;
			// Not supported in Save function
			status = false;
		}
		else if (m_format == GL_R8)
		{
			fmt = GSPng::R8I_PNG;
			glReadPixels(0, 0, m_size.x, m_size.y, GL_RED, GL_UNSIGNED_BYTE, image);
			// Not supported in Save function
			status = false;
		}

		gl_BindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	}

#ifdef ENABLE_OGL_PNG
	GSPng::Save(fmt, fn, image, m_size.x, m_size.y, pitch);
#else
	if (status) Save(fn, image, pitch);
#endif
	free(image);

	return status;
}

