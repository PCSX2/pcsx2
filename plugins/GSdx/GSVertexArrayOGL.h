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

#include "config.h"

#ifdef ENABLE_OGL_DEBUG_MEM_BW
extern uint64 g_vertex_upload_byte;
#endif

struct GSInputLayoutOGL {
	GLint   size;
	GLenum  type;
	GLboolean normalize;
	GLsizei stride;
	const GLvoid* offset;
};

class GSBufferOGL {
	const size_t m_stride;
	size_t m_start;
	size_t m_count;
	size_t m_limit;
	const  GLenum m_target;
	GLuint m_buffer_name;
	uint8*  m_buffer_ptr;
	const bool m_buffer_storage;
	GLsync m_fence[5];

	public:
	GSBufferOGL(GLenum target, size_t stride) :
		m_stride(stride)
		, m_start(0)
		, m_count(0)
		, m_limit(0)
		, m_target(target)
		, m_buffer_storage(GLLoader::found_GL_ARB_buffer_storage)
	{
		gl_GenBuffers(1, &m_buffer_name);
		// Opengl works best with 1-4MB buffer.
		// Warning m_limit is the number of object (not the size in Bytes)
		m_limit = 2 * 2 * 1024 * 1024 / m_stride;

		if (m_buffer_storage) {
			for (size_t i = 0; i < 5; i++) {
				m_fence[i] = 0;
			}

			// TODO: if we do manually the synchronization, I'm not sure size is important. It worths to investigate it.
			// => bigger buffer => less sync
			bind();
			// coherency will be done by flushing
			const GLbitfield common_flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;
			const GLbitfield map_flags = common_flags | GL_MAP_FLUSH_EXPLICIT_BIT;
			const GLbitfield create_flags = common_flags | GL_CLIENT_STORAGE_BIT;

			gl_BufferStorage(m_target, m_stride*m_limit, NULL, create_flags );
			m_buffer_ptr = (uint8*) gl_MapBufferRange(m_target, 0, m_stride*m_limit, map_flags);
			if (!m_buffer_ptr) {
				fprintf(stderr, "Failed to map buffer\n");
				throw GSDXError();
			}
		} else {
			m_buffer_ptr = NULL;
		}
	}

	~GSBufferOGL() {
		if (m_buffer_storage) {
			for (size_t i = 0; i < 5; i++) {
				gl_DeleteSync(m_fence[i]);
			}
			// Don't know if we must do it
			bind();
			gl_UnmapBuffer(m_target);
		}
		gl_DeleteBuffers(1, &m_buffer_name);
	}

	void allocate() { allocate(m_limit); }

	void allocate(size_t new_limit)
	{
		if (!m_buffer_storage) {
			m_start = 0;
			m_limit = new_limit;
			gl_BufferData(m_target,  m_limit * m_stride, NULL, GL_STREAM_DRAW);
		}
	}

	void bind()
	{
		gl_BindBuffer(m_target, m_buffer_name);
	}

	void subdata_upload(const void* src)
	{
		// Current GPU buffer is really too small need to allocate a new one
		if (m_count > m_limit) {
			//fprintf(stderr, "Allocate a new buffer\n %d", m_stride);
			allocate(std::max<int>(m_count * 3 / 2, m_limit));

		} else if (m_count > (m_limit - m_start) ) {
			//fprintf(stderr, "Orphan the buffer %d\n", m_stride);

			// Not enough left free room. Just go back at the beginning
			m_start = 0;
			// Orphan the buffer to avoid synchronization
			allocate(m_limit);
		}

		gl_BufferSubData(m_target,  m_stride * m_start,  m_stride * m_count, src);
	}

	void map_upload(const void* src)
	{
		ASSERT(m_count < m_limit);

		size_t offset = m_start*m_stride;
		size_t length = m_count*m_stride;

		if (m_count > (m_limit - m_start) ) {
			size_t current_chunk = offset >> 20;
#ifdef ENABLE_OGL_DEBUG_FENCE
			fprintf(stderr, "%x: Wrap buffer\n", m_target);
			fprintf(stderr, "%x: Insert a fence in chunk %d\n", m_target, current_chunk);
#endif
			ASSERT(current_chunk > 0 && current_chunk < 5);
			if (m_fence[current_chunk] == 0) {
				m_fence[current_chunk] = gl_FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
			}

			// Wrap at startup
			m_start = 0;
			offset = 0;

			// Only check first chunk
			if (m_fence[0]) {
#ifdef ENABLE_OGL_DEBUG_FENCE
				GLenum status = gl_ClientWaitSync(m_fence[0], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
				if (status != GL_ALREADY_SIGNALED) {
					fprintf(stderr, "%x: Sync Sync! Buffer too small\n", m_target);
				}
#else
				gl_ClientWaitSync(m_fence[0], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
#endif
				gl_DeleteSync(m_fence[0]);
				m_fence[0] = 0;
			}
		}

		// Protect buffer with fences
		size_t current_chunk = offset >> 20;
		size_t next_chunk = (offset + length) >> 20;
		for (size_t c = current_chunk + 1; c <= next_chunk; c++) {
#ifdef ENABLE_OGL_DEBUG_FENCE
			fprintf(stderr, "%x: Insert a fence in chunk %d\n", m_target, c-1);
#endif
			ASSERT(c > 0 && c < 5);
			m_fence[c-1] = gl_FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
			if (m_fence[c]) {
#ifdef ENABLE_OGL_DEBUG_FENCE
				GLenum status = gl_ClientWaitSync(m_fence[c], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
#else
				gl_ClientWaitSync(m_fence[c], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
#endif
				gl_DeleteSync(m_fence[c]);
				m_fence[c] = 0;

#ifdef ENABLE_OGL_DEBUG_FENCE
				if (status != GL_ALREADY_SIGNALED) {
					fprintf(stderr, "%x: Sync Sync! Buffer too small\n", m_target);
				}
#endif
			}
		}

		void* dst = m_buffer_ptr + offset;

		memcpy(dst, src, length);
		gl_FlushMappedBufferRange(m_target, offset, length);
	}

	void upload(const void* src, uint32 count)
	{
#ifdef ENABLE_OGL_DEBUG_MEM_BW
		g_vertex_upload_byte += count*m_stride;
#endif

		m_count = count;

		if (m_buffer_storage) {
			map_upload(src);
		} else {
			subdata_upload(src);
		}
	}

	void EndScene()
	{
		m_start += m_count;
		m_count = 0;
	}

	void Draw(GLenum mode)
	{
		glDrawArrays(mode, m_start, m_count);
	}

	void Draw(GLenum mode, GLint basevertex)
	{
		gl_DrawElementsBaseVertex(mode, m_count, GL_UNSIGNED_INT, (void*)(m_start * m_stride), basevertex);
	}

	void Draw(GLenum mode, GLint basevertex, int offset, int count)
	{
		gl_DrawElementsBaseVertex(mode, count, GL_UNSIGNED_INT, (void*)((m_start + offset) * m_stride), basevertex);
	}

	size_t GetStart() { return m_start; }

};

class GSVertexBufferStateOGL {
	GSBufferOGL *m_vb;
	GSBufferOGL *m_ib;

	GLuint m_va;
	GLenum m_topology;

public:
	GSVertexBufferStateOGL(size_t stride, GSInputLayoutOGL* layout, uint32 layout_nbr) : m_vb(NULL), m_ib(NULL)
	{
		gl_GenVertexArrays(1, &m_va);
		gl_BindVertexArray(m_va);

		m_vb = new GSBufferOGL(GL_ARRAY_BUFFER, stride);
		m_ib = new GSBufferOGL(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32));

		m_vb->bind();
		m_ib->bind();

		m_vb->allocate();
		m_ib->allocate();
		set_internal_format(layout, layout_nbr);
	}

	void bind()
	{
		// Note: index array are part of the VA state so it need to be bound only once.
		gl_BindVertexArray(m_va);
		if (m_vb)
			m_vb->bind();
	}

	void set_internal_format(GSInputLayoutOGL* layout, uint32 layout_nbr)
	{
		for (uint32 i = 0; i < layout_nbr; i++) {
			// Note this function need both a vertex array object and a GL_ARRAY_BUFFER buffer
			gl_EnableVertexAttribArray(i);
			switch (layout[i].type) {
				case GL_UNSIGNED_SHORT:
				case GL_UNSIGNED_INT:
					if (layout[i].normalize) {
						gl_VertexAttribPointer(i, layout[i].size, layout[i].type, layout[i].normalize,  layout[i].stride, layout[i].offset);
					} else {
						// Rule: when shader use integral (not normalized) you must use gl_VertexAttribIPointer (note the extra I)
						gl_VertexAttribIPointer(i, layout[i].size, layout[i].type, layout[i].stride, layout[i].offset);
					}
					break;
				default:
					gl_VertexAttribPointer(i, layout[i].size, layout[i].type, layout[i].normalize,  layout[i].stride, layout[i].offset);
					break;
			}
		}
	}

	void EndScene()
	{
		m_vb->EndScene();
		m_ib->EndScene();
	}

	void DrawPrimitive() { m_vb->Draw(m_topology); }

	void DrawIndexedPrimitive() { m_ib->Draw(m_topology, m_vb->GetStart() ); }

	void DrawIndexedPrimitive(int offset, int count) { m_ib->Draw(m_topology, m_vb->GetStart(), offset, count ); }

	void SetTopology(GLenum topology) { m_topology = topology; }

	void UploadVB(const void* vertices, size_t count) { m_vb->upload(vertices, count); }

	void UploadIB(const void* index, size_t count) {
		m_ib->upload(index, count);
	}

	~GSVertexBufferStateOGL()
	{
		gl_DeleteVertexArrays(1, &m_va);
		delete m_vb;
		delete m_ib;
	}

};
