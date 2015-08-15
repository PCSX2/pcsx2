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

#pragma once

#include "GLState.h"

#ifdef ENABLE_OGL_DEBUG_MEM_BW
extern uint64 g_uniform_upload_byte;
#endif


class GSUniformBufferOGL {
	GLuint buffer;		// data object
	GLuint index;		// GLSL slot
	uint32 size;	    // size of the data

public:
	GSUniformBufferOGL(GLuint index, uint32 size) : index(index)
												  , size(size)
	{
		gl_GenBuffers(1, &buffer);
		bind();
		allocate();
		attach();
	}

	void bind()
	{
		if (GLState::ubo != buffer) {
			GLState::ubo = buffer;
			gl_BindBuffer(GL_UNIFORM_BUFFER, buffer);
		}
	}

	void allocate()
	{
		gl_BufferData(GL_UNIFORM_BUFFER, size, NULL, GL_DYNAMIC_DRAW);
	}

	void attach()
	{
		// From the opengl manpage:
		// glBindBufferBase also binds buffer to the generic buffer binding point specified by target
		GLState::ubo = buffer;
		gl_BindBufferBase(GL_UNIFORM_BUFFER, index, buffer);
	}

	void upload(const void* src)
	{
		bind();
		// glMapBufferRange allow to set various parameter but the call is
		// synchronous whereas glBufferSubData could be asynchronous.
		// TODO: investigate the extension ARB_invalidate_subdata
		gl_BufferSubData(GL_UNIFORM_BUFFER, 0, size, src);
#ifdef ENABLE_OGL_DEBUG_MEM_BW
		g_uniform_upload_byte += size;
#endif
	}

	~GSUniformBufferOGL() {
		gl_DeleteBuffers(1, &buffer);
	}
};

#define UBO_BUFFER_SIZE (4*1024*1024)

class GSUniformBufferStorageOGL {
	GLuint buffer;		// data object
	GLuint index;		// GLSL slot
	uint32 size;	    // size of the data
	uint8* m_buffer_ptr;
	uint32 m_offset;

public:
	GSUniformBufferStorageOGL(GLuint index, uint32 size) : index(index)
												  , size(size), m_offset(0)
	{
		gl_GenBuffers(1, &buffer);
		bind();
		allocate();
		attach();
	}

	void bind()
	{
		if (GLState::ubo != buffer) {
			GLState::ubo = buffer;
			gl_BindBuffer(GL_UNIFORM_BUFFER, buffer);
		}
	}

	void allocate()
	{
		const GLbitfield common_flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT /*| GL_MAP_COHERENT_BIT */;
		const GLbitfield map_flags = common_flags | GL_MAP_FLUSH_EXPLICIT_BIT;
		const GLbitfield create_flags = common_flags /*| GL_CLIENT_STORAGE_BIT */;

		GLsizei buffer_size = UBO_BUFFER_SIZE;
		gl_BufferStorage(GL_UNIFORM_BUFFER, buffer_size, NULL, create_flags);
		m_buffer_ptr = (uint8*) gl_MapBufferRange(GL_UNIFORM_BUFFER, 0, buffer_size, map_flags);
		ASSERT(m_buffer_ptr);
	}

	void attach()
	{
		// From the opengl manpage:
		// glBindBufferBase also binds buffer to the generic buffer binding point specified by target
		GLState::ubo = buffer;
		//gl_BindBufferBase(GL_UNIFORM_BUFFER, index, buffer);
		gl_BindBufferRange(GL_UNIFORM_BUFFER, index, buffer, m_offset, size);
	}

	void upload(const void* src)
	{
#ifdef ENABLE_OGL_DEBUG_MEM_BW
		g_uniform_upload_byte += size;
#endif

		memcpy(m_buffer_ptr + m_offset, src, size);

		attach();
		gl_FlushMappedBufferRange(GL_UNIFORM_BUFFER, m_offset, size);

		m_offset = (m_offset + size + 255u) & ~0xFF;
		if (m_offset >= UBO_BUFFER_SIZE)
			m_offset = 0;
	}

	~GSUniformBufferStorageOGL() {
		bind();
		gl_UnmapBuffer(GL_UNIFORM_BUFFER);
		gl_DeleteBuffers(1, &buffer);
	}
};

#undef UBO_BUFFER_SIZE
