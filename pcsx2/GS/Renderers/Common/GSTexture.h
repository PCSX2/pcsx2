// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "GS/GSVector.h"

#include <string>
#include <string_view>

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
		Texture,   // Generic texture (usually is color textures loaded by the game)
		RWTexture, // UAV
	};

	enum class Format : u8
	{
		Invalid = 0,  ///< Used for initialization
		Color,        ///< Standard (RGBA8) color texture (used to store most of PS2's textures)
		ColorHQ,      ///< High quality (RGB10A2) color texture (no proper alpha)
		ColorHDR,     ///< High dynamic range (RGBA16F) color texture
		ColorClip,    ///< Color texture with more bits for colclip (wrap) emulation, given that blending requires 9bpc (RGBA16Unorm)
		DepthStencil, ///< Depth stencil texture
		UNorm8,       ///< A8UNorm texture for paletted textures and the OSD font
		UInt16,       ///< UInt16 texture for reading back 16-bit depth
		UInt32,       ///< UInt32 texture for reading back 24 and 32-bit depth
		PrimID,       ///< Prim ID tracking texture for date emulation
		BC1,          ///< BC1, aka DXT1 compressed texture for replacements
		BC2,          ///< BC2, aka DXT2/3 compressed texture for replacements
		BC3,          ///< BC3, aka DXT4/5 compressed texture for replacements
		BC7,          ///< BC7, aka BPTC compressed texture for replacements
		Last = BC7,
	};

	enum class State : u8
	{
		Dirty,
		Cleared,
		Invalidated
	};

	union ClearValue
	{
		u32 color;
		float depth;
	};

protected:
	GSVector2i m_size{};
	int m_mipmap_levels = 0;
	Type m_type = Type::Invalid;
	Format m_format = Format::Invalid;
	State m_state = State::Dirty;

	// frame number (arbitrary base) the texture was recycled on
	// different purpose than texture cache ages, do not attempt to merge
	u32 m_last_frame_used = 0;

	bool m_needs_mipmaps_generated = true;
	ClearValue m_clear_value = {};

public:
	GSTexture();
	virtual ~GSTexture();

	// Returns the native handle of a texture.
	virtual void* GetNativeHandle() const = 0;

	virtual bool Update(const GSVector4i& r, const void* data, int pitch, int layer = 0) = 0;
	virtual bool Map(GSMap& m, const GSVector4i* r = nullptr, int layer = 0) = 0;
	virtual void Unmap() = 0;
	virtual void GenerateMipmap() = 0;

#ifdef PCSX2_DEVBUILD
	virtual void SetDebugName(std::string_view name) = 0;
#endif

	bool Save(const std::string& fn);

	__fi int GetWidth() const { return m_size.x; }
	__fi int GetHeight() const { return m_size.y; }
	__fi const GSVector2i& GetSize() const { return m_size; }
	__fi GSVector4i GetRect() const { return GSVector4i::loadh(m_size); }

	__fi int GetMipmapLevels() const { return m_mipmap_levels; }
	__fi bool IsMipmap() const { return m_mipmap_levels > 1; }

	__fi Type GetType() const { return m_type; }
	__fi Format GetFormat() const { return m_format; }
	__fi bool IsCompressedFormat() const { return IsCompressedFormat(m_format); }

	static const char* GetFormatName(Format format);
	static u32 GetCompressedBytesPerBlock(Format format);
	static u32 GetCompressedBlockSize(Format format);
	static u32 CalcUploadPitch(Format format, u32 width);
	static u32 CalcUploadRowLengthFromPitch(Format format, u32 pitch);
	static u32 CalcUploadSize(Format format, u32 height, u32 pitch);

	u32 GetCompressedBytesPerBlock() const;
	u32 GetCompressedBlockSize() const;
	u32 CalcUploadPitch(u32 width) const;
	u32 CalcUploadRowLengthFromPitch(u32 pitch) const;
	u32 CalcUploadSize(u32 height, u32 pitch) const;

	__fi bool IsRenderTargetOrDepthStencil() const
	{
		return (m_type >= Type::RenderTarget && m_type <= Type::DepthStencil);
	}
	__fi bool IsRenderTarget() const
	{
		return (m_type == Type::RenderTarget);
	}
	__fi bool IsDepthStencil() const
	{
		return (m_type == Type::DepthStencil);
	}
	__fi bool IsTexture() const
	{
		return (m_type == Type::Texture);
	}

	__fi State GetState() const { return m_state; }
	__fi void SetState(State state) { m_state = state; }

	__fi u32 GetLastFrameUsed() const { return m_last_frame_used; }
	void SetLastFrameUsed(u32 frame) { m_last_frame_used = frame; }

	__fi u32 GetClearColor() const { return m_clear_value.color; }
	__fi float GetClearDepth() const { return m_clear_value.depth; }
	__fi GSVector4 GetUNormClearColor() const { return GSVector4::unorm8(m_clear_value.color); }

	__fi void SetClearColor(u32 color)
	{
		m_state = State::Cleared;
		m_clear_value.color = color;
	}
	__fi void SetClearDepth(float depth)
	{
		m_state = State::Cleared;
		m_clear_value.depth = depth;
	}

	void GenerateMipmapsIfNeeded();
	void ClearMipmapGenerationFlag() { m_needs_mipmaps_generated = false; }

	// Typical size of a RGBA texture
	u32 GetMemUsage() const { return m_size.x * m_size.y * (m_format == Format::UNorm8 ? 1 : 4); }

	// Helper routines for formats/types
	static bool IsCompressedFormat(Format format) { return (format >= Format::BC1 && format <= Format::BC7); }
};

class GSDownloadTexture
{
public:
	GSDownloadTexture(u32 width, u32 height, GSTexture::Format format);
	virtual ~GSDownloadTexture();

	/// Basically, this has dimensions only because of DX11.
	__fi u32 GetWidth() const { return m_width; }
	__fi u32 GetHeight() const { return m_height; }
	__fi GSTexture::Format GetFormat() const { return m_format; }
	__fi bool NeedsFlush() const { return m_needs_flush; }
	__fi bool IsMapped() const { return (m_map_pointer != nullptr); }
	__fi const u8* GetMapPointer() const { return m_map_pointer; }
	__fi u32 GetMapPitch() const { return m_current_pitch; }

	/// Calculates the pitch of a transfer.
	u32 GetTransferPitch(u32 width, u32 pitch_align) const;

	/// Calculates the size of the data you should transfer.
	void GetTransferSize(const GSVector4i& rc, u32* copy_offset, u32* copy_size, u32* copy_rows) const;

	/// Queues a copy from the specified texture to this buffer.
	/// Does not complete immediately, you should flush before accessing the buffer.
	/// use_transfer_pitch should be true if there's only a single texture being copied to this buffer before
	/// it will be used. This allows the image to be packed tighter together, and buffer reuse.
	virtual void CopyFromTexture(
		const GSVector4i& drc, GSTexture* stex, const GSVector4i& src, u32 src_level, bool use_transfer_pitch = true) = 0;

	/// Maps the texture into the CPU address space, enabling it to read the contents.
	/// The Map call may not perform synchronization. If the contents of the staging texture
	/// has been updated by a CopyFromTexture() call, you must call Flush() first.
	/// If persistent mapping is supported in the backend, this may be a no-op.
	virtual bool Map(const GSVector4i& read_rc) = 0;

	/// Unmaps the CPU-readable copy of the texture. May be a no-op on backends which
	/// support persistent-mapped buffers.
	virtual void Unmap() = 0;

	/// Flushes pending writes from the CPU to the GPU, and reads from the GPU to the CPU.
	/// This may cause a command buffer submit depending on if one has occurred between the last
	/// call to CopyFromTexture() and the Flush() call.
	virtual void Flush() = 0;

#ifdef PCSX2_DEVBUILD
	/// Sets object name that will be displayed in graphics debuggers.
	virtual void SetDebugName(std::string_view name) = 0;
#endif

	/// Reads the specified rectangle from the staging texture to out_ptr, with the specified stride
	/// (length in bytes of each row). CopyFromTexture() must be called first. The contents of any
	/// texels outside of the rectangle used for CopyFromTexture is undefined.
	bool ReadTexels(const GSVector4i& rc, void* out_ptr, u32 out_stride);

	/// Returns what the size of the specified texture would be, in bytes.
	static u32 GetBufferSize(u32 width, u32 height, GSTexture::Format format, u32 pitch_align = 1);

protected:
	u32 m_width;
	u32 m_height;
	GSTexture::Format m_format;

	const u8* m_map_pointer = nullptr;
	u32 m_current_pitch = 0;

	bool m_needs_flush = false;
};
