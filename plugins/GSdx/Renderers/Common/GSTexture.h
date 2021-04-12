/*
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
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

#include "GSVector.h"

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
		ASSERT(0);
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
