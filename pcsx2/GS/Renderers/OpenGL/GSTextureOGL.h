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

#include "GS/Renderers/Common/GSTexture.h"
#include "GS/Renderers/OpenGL/GLLoader.h"
#include "common/AlignedMalloc.h"

namespace PboPool
{
	inline void BindPbo();
	inline void UnbindPbo();
	inline void Sync();

	inline char* Map(u32 size);
	inline void Unmap();
	inline uptr Offset();
	inline void EndTransfer();

	void Init();
	void Destroy();
} // namespace PboPool

class GSTextureOGL final : public GSTexture
{
private:
	GLuint m_texture_id; // the texture id
	GLuint m_fbo_read;
	bool m_clean;

	// Avoid alignment constrain
	//GSVector4i m_r;
	int m_r_x;
	int m_r_y;
	int m_r_w;
	int m_r_h;
	int m_layer;

	// internal opengl format/type/alignment
	GLenum m_int_format;
	GLenum m_int_type;
	u32 m_int_shift;

	// Allow to track size of allocated memory
	u32 m_mem_usage;

public:
	explicit GSTextureOGL(Type type, int width, int height, int levels, Format format, GLuint fbo_read);
	virtual ~GSTextureOGL();

	void* GetNativeHandle() const override;

	bool Update(const GSVector4i& r, const void* data, int pitch, int layer = 0) final;
	bool Map(GSMap& m, const GSVector4i* r = NULL, int layer = 0) final;
	void Unmap() final;
	void GenerateMipmap() final;
	bool Save(const std::string& fn) final;
	void Swap(GSTexture* tex) final;

	GSMap Read(const GSVector4i& r, AlignedBuffer<u8, 32>& buffer);
	bool IsDepth() { return (m_type == Type::DepthStencil || m_type == Type::SparseDepthStencil); }
	bool IsIntegerFormat() const
	{
		return (m_int_format == GL_RED_INTEGER || m_int_format == GL_RGBA_INTEGER);
	}
	bool IsUnsignedFormat() const
	{
		return (m_int_type == GL_UNSIGNED_BYTE || m_int_type == GL_UNSIGNED_SHORT || m_int_type == GL_UNSIGNED_INT);
	}

	u32 GetID() final { return m_texture_id; }
	bool HasBeenCleaned() { return m_clean; }
	void WasAttached() { m_clean = false; }
	void WasCleaned() { m_clean = true; }

	void Clear(const void* data);
	void Clear(const void* data, const GSVector4i& area);

	void CommitPages(const GSVector2i& region, bool commit) final;

	u32 GetMemUsage() final;
};
