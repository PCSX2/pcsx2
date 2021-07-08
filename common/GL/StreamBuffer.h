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

#pragma once
#include "common/Pcsx2Defs.h"
#include "glad.h"
#include <memory>
#include <tuple>
#include <vector>

namespace GL
{
	/// Provides a buffer for streaming data to the GPU, ideally in write-combined memory.
	class StreamBuffer
	{
	public:
		virtual ~StreamBuffer();

		__fi GLuint GetGLBufferId() const { return m_buffer_id; }
		__fi GLenum GetGLTarget() const { return m_target; }
		__fi u32 GetSize() const { return m_size; }

		void Bind();
		void Unbind();

		struct MappingResult
		{
			void* pointer;
			u32 buffer_offset;
			u32 index_aligned; // offset / alignment, suitable for base vertex
			u32 space_aligned; // remaining space / alignment
		};

		virtual MappingResult Map(u32 alignment, u32 min_size) = 0;
		virtual void Unmap(u32 used_size) = 0;

		static std::unique_ptr<StreamBuffer> Create(GLenum target, u32 size);

	protected:
		StreamBuffer(GLenum target, GLuint buffer_id, u32 size);

		GLenum m_target;
		GLuint m_buffer_id;
		u32 m_size;
	};
} // namespace GL