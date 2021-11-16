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

	// In Metal clears happen as a part of render passes instead of as separate steps, but the GSDevice API has it as a separate step
	// To deal with that, store the fact that a clear was requested here and it'll be applied on the next render pass
	bool m_needs_color_clear = false;
	bool m_needs_depth_clear = false;
	bool m_needs_stencil_clear = false;
	GSVector4 m_clear_color;
	float m_clear_depth;
	int m_clear_stencil;

public:
	u64 m_last_read = 0;  ///< Last time this texture was read by a draw
	u64 m_last_write = 0; ///< Last time this texture was written by a draw
	GSTextureMTL(GSDeviceMTL* dev, MRCOwned<id<MTLTexture>> texture, Type type, Format format);
	~GSTextureMTL();

	/// For making fake backbuffers
	void SetSize(GSVector2i size) { m_size = size; }

	/// Requests the texture be cleared the next time a color render is done
	void RequestColorClear(GSVector4 color);
	/// Requests the texture be cleared the next time a depth render is done
	void RequestDepthClear(float depth);
	/// Requests the texture be cleared the next time a stencil render is done
	void RequestStencilClear(int stencil);
	/// Reads whether a color clear was requested, then clears the request
	bool GetResetNeedsColorClear(GSVector4& colorOut);
	/// Reads whether a depth clear was requested, then clears the request
	bool GetResetNeedsDepthClear(float& depthOut);
	/// Reads whether a stencil clear was requested, then clears the request
	bool GetResetNeedsStencilClear(int& stencilOut);
	/// Flushes requested clears to the texture
	void FlushClears();
	/// Marks pending clears as done (e.g. if the whole texture is about to be overwritten)
	void InvalidateClears();

	void* GetNativeHandle() const override;
	bool Update(const GSVector4i& r, const void* data, int pitch, int layer = 0) override;
	bool Map(GSMap& m, const GSVector4i* r = NULL, int layer = 0) override;
	void* MapWithPitch(const GSVector4i& r, int pitch, int layer);
	void Unmap() override;
	void GenerateMipmap() override;
	bool Save(const std::string& fn) override;
	void Swap(GSTexture* tex) override;
	id<MTLTexture> GetTexture() { return m_texture; }
};

#endif
