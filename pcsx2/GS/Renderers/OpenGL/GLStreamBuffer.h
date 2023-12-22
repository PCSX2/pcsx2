// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#include "glad.h"

#include <memory>
#include <tuple>
#include <vector>

/// Provides a buffer for streaming data to the GPU, ideally in write-combined memory.
class GLStreamBuffer
{
public:
	virtual ~GLStreamBuffer();

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

	/// Returns the minimum granularity of blocks which sync objects will be created around.
	virtual u32 GetChunkSize() const = 0;

	static std::unique_ptr<GLStreamBuffer> Create(GLenum target, u32 size);

protected:
	GLStreamBuffer(GLenum target, GLuint buffer_id, u32 size);

	GLenum m_target;
	GLuint m_buffer_id;
	u32 m_size;
};
