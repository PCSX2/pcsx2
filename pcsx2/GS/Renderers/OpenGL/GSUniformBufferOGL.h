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


class GSUniformBufferOGL
{
	GLuint m_buffer; // data object
	GLuint m_index;  // GLSL slot
	uint32 m_size;   // size of the data
	uint8* m_cache;  // content of the previous upload

public:
	GSUniformBufferOGL(const std::string& pretty_name, GLuint index, uint32 size)
		: m_index(index), m_size(size)
	{
		glGenBuffers(1, &m_buffer);
		bind();
		glObjectLabel(GL_BUFFER, m_buffer, pretty_name.size(), pretty_name.c_str());
		allocate();
		attach();
		m_cache = (uint8*)_aligned_malloc(m_size, 32);
		memset(m_cache, 0, m_size);
	}

	void bind()
	{
		if (GLState::ubo != m_buffer)
		{
			GLState::ubo = m_buffer;
			glBindBuffer(GL_UNIFORM_BUFFER, m_buffer);
		}
	}

	void allocate()
	{
		glBufferData(GL_UNIFORM_BUFFER, m_size, NULL, GL_DYNAMIC_DRAW);
	}

	void attach()
	{
		// From the opengl manpage:
		// glBindBufferBase also binds buffer to the generic buffer binding point specified by target
		GLState::ubo = m_buffer;
		glBindBufferBase(GL_UNIFORM_BUFFER, m_index, m_buffer);
	}

	void upload(const void* src)
	{
		bind();
		// glMapBufferRange allow to set various parameter but the call is
		// synchronous whereas glBufferSubData could be asynchronous.
		// TODO: investigate the extension ARB_invalidate_subdata
		glBufferSubData(GL_UNIFORM_BUFFER, 0, m_size, src);
#ifdef ENABLE_OGL_DEBUG_MEM_BW
		g_uniform_upload_byte += m_size;
#endif
	}

	void cache_upload(const void* src)
	{
		if (memcmp(m_cache, src, m_size) != 0)
		{
			memcpy(m_cache, src, m_size);
			upload(src);
		}
	}

	~GSUniformBufferOGL()
	{
		glDeleteBuffers(1, &m_buffer);
		_aligned_free(m_cache);
	}
};

#define UBO_BUFFER_SIZE (4 * 1024 * 1024)

class GSUniformBufferStorageOGL
{
	GLuint m_buffer; // data object
	GLuint m_index;  // GLSL slot
	uint32 m_size;   // size of the data
	uint8* m_buffer_ptr;
	uint32 m_offset;

public:
	GSUniformBufferStorageOGL(GLuint index, uint32 size)
		: m_index(index) , m_size(size) , m_offset(0)
	{
		glGenBuffers(1, &m_buffer);
		bind();
		allocate();
		attach();
	}

	void bind()
	{
		if (GLState::ubo != m_buffer)
		{
			GLState::ubo = m_buffer;
			glBindBuffer(GL_UNIFORM_BUFFER, m_buffer);
		}
	}

	void allocate()
	{
		const GLbitfield common_flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT /*| GL_MAP_COHERENT_BIT */;
		const GLbitfield map_flags = common_flags | GL_MAP_FLUSH_EXPLICIT_BIT;
		const GLbitfield create_flags = common_flags /*| GL_CLIENT_STORAGE_BIT */;

		GLsizei buffer_size = UBO_BUFFER_SIZE;
		glBufferStorage(GL_UNIFORM_BUFFER, buffer_size, NULL, create_flags);
		m_buffer_ptr = (uint8*)glMapBufferRange(GL_UNIFORM_BUFFER, 0, buffer_size, map_flags);
		ASSERT(m_buffer_ptr);
	}

	void attach()
	{
		// From the opengl manpage:
		// glBindBufferBase also binds buffer to the generic buffer binding point specified by target
		GLState::ubo = m_buffer;
		//glBindBufferBase(GL_UNIFORM_BUFFER, m_index, m_buffer);
		glBindBufferRange(GL_UNIFORM_BUFFER, m_index, m_buffer, m_offset, m_size);
	}

	void upload(const void* src)
	{
#ifdef ENABLE_OGL_DEBUG_MEM_BW
		g_uniform_upload_byte += m_size;
#endif

		memcpy(m_buffer_ptr + m_offset, src, m_size);

		attach();
		glFlushMappedBufferRange(GL_UNIFORM_BUFFER, m_offset, m_size);

		m_offset = (m_offset + m_size + 255u) & ~0xFF;
		if (m_offset >= UBO_BUFFER_SIZE)
			m_offset = 0;
	}

	~GSUniformBufferStorageOGL()
	{
		glDeleteBuffers(1, &m_buffer);
	}
};

#undef UBO_BUFFER_SIZE
