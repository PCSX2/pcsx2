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

#include "GS/GSVector.h"

class GSTexture
{
protected:
	GSVector2 m_scale;
	GSVector2i m_size;
	GSVector2i m_committed_size;
	GSVector2i m_gpu_page_size;
	int m_type;
	int m_format;
	bool m_sparse;

public:
	struct GSMap
	{
		uint8* bits;
		int pitch;
	};

	enum
	{
		RenderTarget = 1,
		DepthStencil,
		Texture,
		Offscreen,
		Backbuffer,
		SparseRenderTarget,
		SparseDepthStencil
	};

public:
	GSTexture();
	virtual ~GSTexture() {}

	virtual operator bool()
	{
		pxAssert(0);
		return false;
	}

	virtual bool Update(const GSVector4i& r, const void* data, int pitch, int layer = 0) = 0;
	virtual bool Map(GSMap& m, const GSVector4i* r = NULL, int layer = 0) = 0;
	virtual void Unmap() = 0;
	virtual void GenerateMipmap() {}
	virtual bool Save(const std::string& fn) = 0;
	virtual uint32 GetID() { return 0; }

	GSVector2 GetScale() const { return m_scale; }
	void SetScale(const GSVector2& scale) { m_scale = scale; }

	int GetWidth() const { return m_size.x; }
	int GetHeight() const { return m_size.y; }
	GSVector2i GetSize() const { return m_size; }

	int GetType() const { return m_type; }
	int GetFormat() const { return m_format; }

	virtual void CommitPages(const GSVector2i& region, bool commit) {}
	void CommitRegion(const GSVector2i& region);
	void Commit();
	void Uncommit();
	GSVector2i GetCommittedSize() const { return m_committed_size; }
	void SetGpuPageSize(const GSVector2i& page_size);
	GSVector2i RoundUpPage(GSVector2i v);

	// frame number (arbitrary base) the texture was recycled on
	// different purpose than texture cache ages, do not attempt to merge
	unsigned last_frame_used;

	bool LikelyOffset;
	float OffsetHack_modx;
	float OffsetHack_mody;

	// Typical size of a RGBA texture
	virtual uint32 GetMemUsage() { return m_size.x * m_size.y * 4; }
};
