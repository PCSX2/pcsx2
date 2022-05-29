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
public:
	struct GSMap
	{
		u8* bits;
		int pitch;
	};

	enum class Type : u8
	{
		Invalid = 0,
		RenderTarget = 1,
		DepthStencil,
		Texture,
		Offscreen,
		SparseRenderTarget,
		SparseDepthStencil,
	};

	enum class Format : u8
	{
		Invalid = 0,  ///< Used for initialization
		Color,        ///< Standard (RGBA8) color texture
		FloatColor,   ///< Float-based color texture for colclip emulation (RGBA32F)
		DepthStencil, ///< Depth stencil texture
		UNorm8,       ///< A8UNorm texture for paletted textures and the OSD font
		UInt16,       ///< UInt16 texture for reading back 16-bit depth
		UInt32,       ///< UInt32 texture for reading back 24 and 32-bit depth
		PrimID,       ///< Prim ID tracking texture for date emulation
		BC1,          ///< BC1, aka DXT1 compressed texture for replacements
		BC2,          ///< BC2, aka DXT2/3 compressed texture for replacements
		BC3,          ///< BC3, aka DXT4/5 compressed texture for replacements
		BC7,          ///< BC7, aka BPTC compressed texture for replacements
	};

	enum class State : u8
	{
		Dirty,
		Cleared,
		Invalidated
	};

protected:
	GSVector2 m_scale;
	GSVector2i m_size;
	GSVector2i m_committed_size;
	GSVector2i m_gpu_page_size;
	int m_mipmap_levels;
	Type m_type;
	Format m_format;
	State m_state;
	bool m_sparse;
	bool m_needs_mipmaps_generated;

public:
	GSTexture();
	virtual ~GSTexture() {}

	virtual operator bool()
	{
		pxAssert(0);
		return false;
	}

	// Returns the native handle of a texture.
	virtual void* GetNativeHandle() const = 0;

	virtual bool Update(const GSVector4i& r, const void* data, int pitch, int layer = 0) = 0;
	virtual bool Map(GSMap& m, const GSVector4i* r = NULL, int layer = 0) = 0;
	virtual void Unmap() = 0;
	virtual void GenerateMipmap() {}
	virtual bool Save(const std::string& fn);
	virtual void Swap(GSTexture* tex);
	virtual u32 GetID() { return 0; }

	GSVector2 GetScale() const { return m_scale; }
	void SetScale(const GSVector2& scale) { m_scale = scale; }

	int GetWidth() const { return m_size.x; }
	int GetHeight() const { return m_size.y; }
	GSVector2i GetSize() const { return m_size; }
	int GetMipmapLevels() const { return m_mipmap_levels; }
	bool IsMipmap() const { return m_mipmap_levels > 1; }

	Type GetType() const { return m_type; }
	Format GetFormat() const { return m_format; }
	bool IsCompressedFormat() const { return IsCompressedFormat(m_format); }

	u32 GetCompressedBytesPerBlock() const;
	u32 GetCompressedBlockSize() const;
	u32 CalcUploadRowLengthFromPitch(u32 pitch) const;
	u32 CalcUploadSize(u32 height, u32 pitch) const;

	bool IsRenderTargetOrDepthStencil() const
	{
		return (m_type >= Type::RenderTarget && m_type <= Type::DepthStencil) ||
			(m_type >= Type::SparseRenderTarget && m_type <= Type::SparseDepthStencil);
	}
	bool IsRenderTarget() const
	{
		return (m_type == Type::RenderTarget || m_type == Type::SparseRenderTarget);
	}
	bool IsDepthStencil() const
	{
		return (m_type == Type::DepthStencil || m_type == Type::SparseDepthStencil);
	}

	State GetState() const { return m_state; }
	void SetState(State state) { m_state = state; }

	void GenerateMipmapsIfNeeded();
	void ClearMipmapGenerationFlag() { m_needs_mipmaps_generated = false; }

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

	float OffsetHack_modxy;

	// Typical size of a RGBA texture
	virtual u32 GetMemUsage() { return m_size.x * m_size.y * (m_format == Format::UNorm8 ? 1 : 4); }

	// Helper routines for formats/types
	static bool IsCompressedFormat(Format format) { return (format >= Format::BC1 && format <= Format::BC7); }
};
