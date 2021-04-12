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

#include "Renderers/Common/GSTexture.h"

namespace PboPool
{
	inline void BindPbo();
	inline void UnbindPbo();
	inline void Sync();

	inline char* Map(uint32 size);
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
	bool m_generate_mipmap;

	uint8* m_local_buffer;
	// Avoid alignment constrain
	//GSVector4i m_r;
	int m_r_x;
	int m_r_y;
	int m_r_w;
	int m_r_h;
	int m_layer;
	int m_max_layer;

	// internal opengl format/type/alignment
	GLenum m_int_format;
	GLenum m_int_type;
	uint32 m_int_shift;

	// Allow to track size of allocated memory
	uint32 m_mem_usage;

public:
	explicit GSTextureOGL(int type, int w, int h, int format, GLuint fbo_read, bool mipmap);
	virtual ~GSTextureOGL();

	bool Update(const GSVector4i& r, const void* data, int pitch, int layer = 0) final;
	bool Map(GSMap& m, const GSVector4i* r = NULL, int layer = 0) final;
	void Unmap() final;
	void GenerateMipmap() final;
	bool Save(const std::string& fn) final;

	bool IsBackbuffer() { return (m_type == GSTexture::Backbuffer); }
	bool IsDss() { return (m_type == GSTexture::DepthStencil || m_type == GSTexture::SparseDepthStencil); }

	uint32 GetID() final { return m_texture_id; }
	bool HasBeenCleaned() { return m_clean; }
	void WasAttached() { m_clean = false; }
	void WasCleaned() { m_clean = true; }

	void Clear(const void* data);
	void Clear(const void* data, const GSVector4i& area);

	void CommitPages(const GSVector2i& region, bool commit) final;

	uint32 GetMemUsage();
};
