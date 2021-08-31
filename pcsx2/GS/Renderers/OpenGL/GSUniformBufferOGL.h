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

#pragma once

#include "GLState.h"

#ifdef ENABLE_OGL_DEBUG_MEM_BW
extern u64 g_uniform_upload_byte;
#endif


class GSUniformBufferOGL
{
	GLuint m_buffer; // data object
	GLuint m_index;  // GLSL slot
	u32 m_size;   // size of the data
	u8* m_cache;  // content of the previous upload

public:
	GSUniformBufferOGL(const std::string& pretty_name, GLuint index, u32 size)
		: m_index(index), m_size(size)
	{
		glGenBuffers(1, &m_buffer);
		bind();
		glObjectLabel(GL_BUFFER, m_buffer, pretty_name.size(), pretty_name.c_str());
		allocate();
		attach();
		m_cache = (u8*)_aligned_malloc(m_size, 32);
		memset(m_cache, 0, m_size);
	}

	void bind()
	{
		glBindBuffer(GL_UNIFORM_BUFFER, m_buffer);
	}

	void allocate()
	{
		glBufferData(GL_UNIFORM_BUFFER, m_size, NULL, GL_DYNAMIC_DRAW);
	}

	void attach()
	{
		// From the opengl manpage:
		// glBindBufferBase also binds buffer to the generic buffer binding point specified by target
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
