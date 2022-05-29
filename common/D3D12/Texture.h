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
#include "common/D3D12/DescriptorHeapManager.h"

#include <d3d12.h>
#include <wil/com.h>

namespace D3D12MA
{
class Allocation;
}

namespace D3D12
{
	class StreamBuffer;

	class Texture final
	{
	public:
		template <typename T>
		using ComPtr = wil::com_ptr_nothrow<T>;

		Texture();
		Texture(ID3D12Resource* resource, D3D12_RESOURCE_STATES state);
		Texture(Texture&& texture);
		Texture(const Texture&) = delete;
		~Texture();

		__fi ID3D12Resource* GetResource() const { return m_resource.get(); }
		__fi D3D12MA::Allocation* GetAllocation() const { return m_allocation.get(); }
		__fi const DescriptorHandle& GetSRVDescriptor() const { return m_srv_descriptor; }
		__fi const DescriptorHandle& GetRTVOrDSVDescriptor() const { return m_rtv_or_dsv_descriptor; }
		__fi D3D12_RESOURCE_STATES GetState() const { return m_state; }

		__fi u32 GetWidth() const { return m_width; }
		__fi u32 GetHeight() const { return m_height; }
		__fi u32 GetLevels() const { return m_levels; }
		__fi DXGI_FORMAT GetFormat() const { return m_format; }

		__fi operator ID3D12Resource*() const { return m_resource.get(); }
		__fi operator bool() const { return static_cast<bool>(m_resource); }

		bool Create(u32 width, u32 height, u32 levels, DXGI_FORMAT format, DXGI_FORMAT srv_format,
			DXGI_FORMAT rtv_format, DXGI_FORMAT dsv_format, D3D12_RESOURCE_FLAGS flags, u32 alloc_flags = 0);
		bool Adopt(ComPtr<ID3D12Resource> texture, DXGI_FORMAT srv_format, DXGI_FORMAT rtv_format, DXGI_FORMAT dsv_format,
			D3D12_RESOURCE_STATES state);

		D3D12_RESOURCE_DESC GetDesc() const;

		void Destroy(bool defer = true);

		void TransitionToState(ID3D12GraphicsCommandList* cmdlist, D3D12_RESOURCE_STATES state);
		void TransitionSubresourceToState(ID3D12GraphicsCommandList* cmdlist, u32 level,
			D3D12_RESOURCE_STATES before_state, D3D12_RESOURCE_STATES after_state) const;

		Texture& operator=(const Texture&) = delete;
		Texture& operator=(Texture&& texture);

		// NOTE: Does not handle compressed formats.
		bool BeginStreamUpdate(u32 level, u32 x, u32 y, u32 width, u32 height, void** out_data, u32* out_data_pitch);
		void EndStreamUpdate(u32 level, u32 x, u32 y, u32 width, u32 height);
		bool LoadData(u32 level, u32 x, u32 y, u32 width, u32 height, const void* data, u32 pitch);
		void CopyFromBuffer(u32 level, u32 x, u32 y, u32 width, u32 height, u32 pitch, ID3D12Resource* buffer, u32 buffer_offset);

	private:
		static bool CreateSRVDescriptor(ID3D12Resource* resource, u32 levels, DXGI_FORMAT format, DescriptorHandle* dh);
		static bool CreateRTVDescriptor(ID3D12Resource* resource, DXGI_FORMAT format, DescriptorHandle* dh);
		static bool CreateDSVDescriptor(ID3D12Resource* resource, DXGI_FORMAT format, DescriptorHandle* dh);

		ComPtr<ID3D12Resource> m_resource;
		ComPtr<D3D12MA::Allocation> m_allocation;
		DescriptorHandle m_srv_descriptor = {};
		DescriptorHandle m_rtv_or_dsv_descriptor = {};
		u32 m_width = 0;
		u32 m_height = 0;
		u32 m_levels = 0;
		DXGI_FORMAT m_format = DXGI_FORMAT_UNKNOWN;

		D3D12_RESOURCE_STATES m_state = D3D12_RESOURCE_STATE_COMMON;

		bool m_is_depth_view = false;
	};
} // namespace D3D12