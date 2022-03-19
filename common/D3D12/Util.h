/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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
#include "common/RedtapeWindows.h"

#include <array>
#include <d3d12.h>

namespace D3D12
{
	static inline void ResourceBarrier(ID3D12GraphicsCommandList* cmdlist, ID3D12Resource* resource,
		D3D12_RESOURCE_STATES from_state, D3D12_RESOURCE_STATES to_state)
	{
		const D3D12_RESOURCE_BARRIER barrier = {D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
			D3D12_RESOURCE_BARRIER_FLAG_NONE,
			{{resource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, from_state, to_state}}};
		cmdlist->ResourceBarrier(1, &barrier);
	}

	static inline void SetViewport(ID3D12GraphicsCommandList* cmdlist, int x, int y, int width, int height,
		float min_depth = 0.0f, float max_depth = 1.0f)
	{
		const D3D12_VIEWPORT vp{static_cast<float>(x),
			static_cast<float>(y),
			static_cast<float>(width),
			static_cast<float>(height),
			min_depth,
			max_depth};
		cmdlist->RSSetViewports(1, &vp);
	}

	static inline void SetScissor(ID3D12GraphicsCommandList* cmdlist, int x, int y, int width, int height)
	{
		const D3D12_RECT r{x, y, x + width, y + height};
		cmdlist->RSSetScissorRects(1, &r);
	}

	static inline void SetViewportAndScissor(ID3D12GraphicsCommandList* cmdlist, int x, int y, int width, int height,
		float min_depth = 0.0f, float max_depth = 1.0f)
	{
		SetViewport(cmdlist, x, y, width, height, min_depth, max_depth);
		SetScissor(cmdlist, x, y, width, height);
	}

	u32 GetTexelSize(DXGI_FORMAT format);

	void SetDefaultSampler(D3D12_SAMPLER_DESC* desc);

#ifdef _DEBUG

	void SetObjectName(ID3D12Object* object, const char* name);
	void SetObjectNameFormatted(ID3D12Object* object, const char* format, ...);

#else

	static inline void SetObjectName(ID3D12Object* object, const char* name)
	{
	}
	static inline void SetObjectNameFormatted(ID3D12Object* object, const char* format, ...) {}

#endif
} // namespace D3D12