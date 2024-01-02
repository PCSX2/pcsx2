// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GS/Renderers/Common/GSTexture.h"

#include <string_view>

#ifndef __OBJC__
	#error "This header is for use with Objective-C++ only.
#endif

#ifdef __APPLE__
#include "common/MRCHelpers.h"
#include <Metal/Metal.h>

class GSDeviceMTL;

class GSTextureMTL : public GSTexture
{
	GSDeviceMTL* m_dev;
	MRCOwned<id<MTLTexture>> m_texture;
	bool m_has_mipmaps = false;

public:
	u64 m_last_read = 0;  ///< Last time this texture was read by a draw
	u64 m_last_write = 0; ///< Last time this texture was written by a draw
	GSTextureMTL(GSDeviceMTL* dev, MRCOwned<id<MTLTexture>> texture, Type type, Format format);
	~GSTextureMTL();

	/// For making fake backbuffers
	void SetSize(GSVector2i size) { m_size = size; }

	/// Flushes requested clears to the texture
	void FlushClears();

	void* GetNativeHandle() const override;
	bool Update(const GSVector4i& r, const void* data, int pitch, int layer = 0) override;
	bool Map(GSMap& m, const GSVector4i* r = NULL, int layer = 0) override;
	void* MapWithPitch(const GSVector4i& r, int pitch, int layer);
	void Unmap() override;
	void GenerateMipmap() override;
	id<MTLTexture> GetTexture() { return m_texture; }

#ifdef PCSX2_DEVBUILD
	void SetDebugName(std::string_view name) override;
#endif
};

class GSDownloadTextureMTL final : public GSDownloadTexture
{
public:
	~GSDownloadTextureMTL() override;

	static std::unique_ptr<GSDownloadTextureMTL> Create(GSDeviceMTL* dev, u32 width, u32 height, GSTexture::Format format);

	void CopyFromTexture(const GSVector4i& drc, GSTexture* stex, const GSVector4i& src, u32 src_level, bool use_transfer_pitch) override;

	bool Map(const GSVector4i& read_rc) override;
	void Unmap() override;

	void Flush() override;

#ifdef PCSX2_DEVBUILD
	void SetDebugName(std::string_view name) override;
#endif

private:
	GSDownloadTextureMTL(GSDeviceMTL* dev, MRCOwned<id<MTLBuffer>> buffer, u32 width, u32 height, GSTexture::Format format);

	GSDeviceMTL* m_dev;
	MRCOwned<id<MTLBuffer>> m_buffer;
	MRCOwned<id<MTLCommandBuffer>> m_copy_cmdbuffer = nil;
};

#endif
