/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#include "StreamBuffer.h"
#include "common/Align.h"
#include "common/Assertions.h"
#include <array>
#include <cstring>

namespace GL
{
	StreamBuffer::StreamBuffer(GLenum target, GLuint buffer_id, u32 size)
		: m_target(target)
		, m_buffer_id(buffer_id)
		, m_size(size)
	{
	}

	StreamBuffer::~StreamBuffer()
	{
		glDeleteBuffers(1, &m_buffer_id);
	}

	void StreamBuffer::Bind()
	{
		glBindBuffer(m_target, m_buffer_id);
	}

	void StreamBuffer::Unbind()
	{
		glBindBuffer(m_target, 0);
	}

	namespace detail
	{
		// Uses glBufferSubData() to update. Preferred for drivers which don't support {ARB,EXT}_buffer_storage.
		class BufferSubDataStreamBuffer final : public StreamBuffer
		{
		public:
			~BufferSubDataStreamBuffer() override = default;

			MappingResult Map(u32 alignment, u32 min_size) override
			{
				return MappingResult{static_cast<void*>(m_cpu_buffer.data()), 0, 0, m_size / alignment};
			}

			void Unmap(u32 used_size) override
			{
				if (used_size == 0)
					return;

				glBindBuffer(m_target, m_buffer_id);
				glBufferSubData(m_target, 0, used_size, m_cpu_buffer.data());
			}

			static std::unique_ptr<StreamBuffer> Create(GLenum target, u32 size)
			{
				glGetError();

				GLuint buffer_id;
				glGenBuffers(1, &buffer_id);
				glBindBuffer(target, buffer_id);
				glBufferData(target, size, nullptr, GL_STREAM_DRAW);

				GLenum err = glGetError();
				if (err != GL_NO_ERROR)
				{
					glBindBuffer(target, 0);
					glDeleteBuffers(1, &buffer_id);
					return {};
				}

				return std::unique_ptr<StreamBuffer>(new BufferSubDataStreamBuffer(target, buffer_id, size));
			}

		private:
			BufferSubDataStreamBuffer(GLenum target, GLuint buffer_id, u32 size)
				: StreamBuffer(target, buffer_id, size)
				, m_cpu_buffer(size)
			{
			}

			std::vector<u8> m_cpu_buffer;
		};

		// Uses BufferData() to orphan the buffer after every update. Used on Mali where BufferSubData forces a sync.
		class BufferDataStreamBuffer final : public StreamBuffer
		{
		public:
			~BufferDataStreamBuffer() override = default;

			MappingResult Map(u32 alignment, u32 min_size) override
			{
				return MappingResult{static_cast<void*>(m_cpu_buffer.data()), 0, 0, m_size / alignment};
			}

			void Unmap(u32 used_size) override
			{
				if (used_size == 0)
					return;

				glBindBuffer(m_target, m_buffer_id);
				glBufferData(m_target, used_size, m_cpu_buffer.data(), GL_STREAM_DRAW);
			}

			static std::unique_ptr<StreamBuffer> Create(GLenum target, u32 size)
			{
				glGetError();

				GLuint buffer_id;
				glGenBuffers(1, &buffer_id);
				glBindBuffer(target, buffer_id);
				glBufferData(target, size, nullptr, GL_STREAM_DRAW);

				GLenum err = glGetError();
				if (err != GL_NO_ERROR)
				{
					glBindBuffer(target, 0);
					glDeleteBuffers(1, &buffer_id);
					return {};
				}

				return std::unique_ptr<StreamBuffer>(new BufferDataStreamBuffer(target, buffer_id, size));
			}

		private:
			BufferDataStreamBuffer(GLenum target, GLuint buffer_id, u32 size)
				: StreamBuffer(target, buffer_id, size)
				, m_cpu_buffer(size)
			{
			}

			std::vector<u8> m_cpu_buffer;
		};

		// Base class for implementations which require syncing.
		class SyncingStreamBuffer : public StreamBuffer
		{
		public:
			enum : u32
			{
				NUM_SYNC_POINTS = 16
			};

			virtual ~SyncingStreamBuffer() override
			{
				for (u32 i = m_available_block_index; i <= m_used_block_index; i++)
				{
					pxAssert(m_sync_objects[i]);
					glDeleteSync(m_sync_objects[i]);
				}
			}

		protected:
			SyncingStreamBuffer(GLenum target, GLuint buffer_id, u32 size)
				: StreamBuffer(target, buffer_id, size)
				, m_bytes_per_block((size + (NUM_SYNC_POINTS)-1) / NUM_SYNC_POINTS)
			{
			}

			__fi u32 GetSyncIndexForOffset(u32 offset) { return offset / m_bytes_per_block; }

			__fi void AddSyncsForOffset(u32 offset)
			{
				const u32 end = GetSyncIndexForOffset(offset);
				for (; m_used_block_index < end; m_used_block_index++)
				{
					pxAssert(!m_sync_objects[m_used_block_index]);
					m_sync_objects[m_used_block_index] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
				}
			}

			__fi void WaitForSync(GLsync& sync)
			{
				glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
				glDeleteSync(sync);
				sync = nullptr;
			}

			__fi void EnsureSyncsWaitedForOffset(u32 offset)
			{
				const u32 end = std::min<u32>(GetSyncIndexForOffset(offset) + 1, NUM_SYNC_POINTS);
				for (; m_available_block_index < end; m_available_block_index++)
				{
					pxAssert(m_sync_objects[m_available_block_index]);
					WaitForSync(m_sync_objects[m_available_block_index]);
				}
			}

			void AllocateSpace(u32 size)
			{
				// add sync objects for writes since the last allocation
				AddSyncsForOffset(m_position);

				// wait for sync objects for the space we want to use
				EnsureSyncsWaitedForOffset(m_position + size);

				// wrap-around?
				if ((m_position + size) > m_size)
				{
					// current position ... buffer end
					AddSyncsForOffset(m_size);

					// rewind, and try again
					m_position = 0;

					// wait for the sync at the start of the buffer
					WaitForSync(m_sync_objects[0]);
					m_available_block_index = 1;

					// and however much more we need to satisfy the allocation
					EnsureSyncsWaitedForOffset(size);
					m_used_block_index = 0;
				}
			}

			u32 m_position = 0;
			u32 m_used_block_index = 0;
			u32 m_available_block_index = NUM_SYNC_POINTS;
			u32 m_bytes_per_block;
			std::array<GLsync, NUM_SYNC_POINTS> m_sync_objects{};
		};

		class BufferStorageStreamBuffer : public SyncingStreamBuffer
		{
		public:
			~BufferStorageStreamBuffer() override
			{
				glBindBuffer(m_target, m_buffer_id);
				glUnmapBuffer(m_target);
				glBindBuffer(m_target, 0);
			}

			MappingResult Map(u32 alignment, u32 min_size) override
			{
				if (m_position > 0)
					m_position = Common::AlignUp(m_position, alignment);

				AllocateSpace(min_size);
				pxAssert((m_position + min_size) <= (m_available_block_index * m_bytes_per_block));

				const u32 free_space_in_block = ((m_available_block_index * m_bytes_per_block) - m_position);
				return MappingResult{static_cast<void*>(m_mapped_ptr + m_position), m_position, m_position / alignment,
					free_space_in_block / alignment};
			}

			void Unmap(u32 used_size) override
			{
				pxAssert((m_position + used_size) <= m_size);
				if (!m_coherent)
				{
					Bind();
					glFlushMappedBufferRange(m_target, m_position, used_size);
				}

				m_position += used_size;
			}

			static std::unique_ptr<StreamBuffer> Create(GLenum target, u32 size, bool coherent = true)
			{
				glGetError();

				GLuint buffer_id;
				glGenBuffers(1, &buffer_id);
				glBindBuffer(target, buffer_id);

				const u32 flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | (coherent ? GL_MAP_COHERENT_BIT : 0);
				const u32 map_flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | (coherent ? 0 : GL_MAP_FLUSH_EXPLICIT_BIT);
				if (GLAD_GL_VERSION_4_4 || GLAD_GL_ARB_buffer_storage)
					glBufferStorage(target, size, nullptr, flags);
				else if (GLAD_GL_EXT_buffer_storage)
					glBufferStorageEXT(target, size, nullptr, flags);

				GLenum err = glGetError();
				if (err != GL_NO_ERROR)
				{
					glBindBuffer(target, 0);
					glDeleteBuffers(1, &buffer_id);
					return {};
				}

				u8* mapped_ptr = static_cast<u8*>(glMapBufferRange(target, 0, size, map_flags));
				pxAssertRel(mapped_ptr, "Persistent buffer was mapped");

				return std::unique_ptr<StreamBuffer>(new BufferStorageStreamBuffer(target, buffer_id, size, mapped_ptr, coherent));
			}

		private:
			BufferStorageStreamBuffer(GLenum target, GLuint buffer_id, u32 size, u8* mapped_ptr, bool coherent)
				: SyncingStreamBuffer(target, buffer_id, size)
				, m_mapped_ptr(mapped_ptr)
				, m_coherent(coherent)
			{
			}

			u8* m_mapped_ptr;
			bool m_coherent;
		};

	} // namespace detail

	std::unique_ptr<StreamBuffer> StreamBuffer::Create(GLenum target, u32 size)
	{
		std::unique_ptr<StreamBuffer> buf;
		if (GLAD_GL_VERSION_4_4 || GLAD_GL_ARB_buffer_storage || GLAD_GL_EXT_buffer_storage)
		{
			buf = detail::BufferStorageStreamBuffer::Create(target, size);
			if (buf)
				return buf;
		}

		// BufferSubData is slower on all drivers except NVIDIA...
		const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
		if (std::strstr(vendor, "NVIDIA"))
			return detail::BufferSubDataStreamBuffer::Create(target, size);
		else
			return detail::BufferDataStreamBuffer::Create(target, size);
	}
} // namespace GL
