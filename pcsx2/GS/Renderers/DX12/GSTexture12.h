/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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

#include "GS/GS.h"
#include "GS/Renderers/Common/GSTexture.h"
#include "GS/Renderers/DX12/D3D12DescriptorHeapManager.h"

#include "common/RedtapeWilCom.h"

#include <limits>

namespace D3D12MA
{
	class Allocation;
}

class GSTexture12 final : public GSTexture
{
public:
	~GSTexture12() override;

	static std::unique_ptr<GSTexture12> Create(Type type, Format format, int width, int height, int levels,
		DXGI_FORMAT dxgi_format, DXGI_FORMAT srv_format, DXGI_FORMAT rtv_format, DXGI_FORMAT dsv_format,
		DXGI_FORMAT uav_format);
	static std::unique_ptr<GSTexture12> Adopt(wil::com_ptr_nothrow<ID3D12Resource> resource, Type type, Format format,
		int width, int height, int levels, DXGI_FORMAT dxgi_format, DXGI_FORMAT srv_format, DXGI_FORMAT rtv_format,
		DXGI_FORMAT dsv_format, DXGI_FORMAT uav_format, D3D12_RESOURCE_STATES resource_state);

	__fi const D3D12DescriptorHandle& GetSRVDescriptor() const { return m_srv_descriptor; }
	__fi const D3D12DescriptorHandle& GetWriteDescriptor() const { return m_write_descriptor; }
	__fi const D3D12DescriptorHandle& GetUAVDescriptor() const { return m_uav_descriptor; }
	__fi D3D12_RESOURCE_STATES GetResourceState() const { return m_resource_state; }
	__fi DXGI_FORMAT GetDXGIFormat() const { return m_dxgi_format; }
	__fi ID3D12Resource* GetResource() const { return m_resource.get(); }

	void* GetNativeHandle() const override;

	bool Update(const GSVector4i& r, const void* data, int pitch, int layer = 0) override;
	bool Map(GSMap& m, const GSVector4i* r = NULL, int layer = 0) override;
	void Unmap() override;
	void GenerateMipmap() override;
	void Swap(GSTexture* tex) override;

	void TransitionToState(D3D12_RESOURCE_STATES state);
	void CommitClear();
	void CommitClear(ID3D12GraphicsCommandList* cmdlist);

	void Destroy(bool defer = true);

	void TransitionToState(ID3D12GraphicsCommandList* cmdlist, D3D12_RESOURCE_STATES state);
	void TransitionSubresourceToState(ID3D12GraphicsCommandList* cmdlist, int level, D3D12_RESOURCE_STATES before_state,
		D3D12_RESOURCE_STATES after_state) const;

	// Call when the texture is bound to the pipeline, or read from in a copy.
	__fi void SetUseFenceCounter(u64 val) { m_use_fence_counter = val; }

private:
	enum class WriteDescriptorType : u8
	{
		None,
		RTV,
		DSV
	};

	GSTexture12(Type type, Format format, int width, int height, int levels, DXGI_FORMAT dxgi_format,
		wil::com_ptr_nothrow<ID3D12Resource> resource, wil::com_ptr_nothrow<D3D12MA::Allocation> allocation,
		const D3D12DescriptorHandle& srv_descriptor, const D3D12DescriptorHandle& write_descriptor,
		const D3D12DescriptorHandle& uav_descriptor, WriteDescriptorType wdtype, D3D12_RESOURCE_STATES resource_state);

	static bool CreateSRVDescriptor(
		ID3D12Resource* resource, u32 levels, DXGI_FORMAT format, D3D12DescriptorHandle* dh);
	static bool CreateRTVDescriptor(ID3D12Resource* resource, DXGI_FORMAT format, D3D12DescriptorHandle* dh);
	static bool CreateDSVDescriptor(ID3D12Resource* resource, DXGI_FORMAT format, D3D12DescriptorHandle* dh);
	static bool CreateUAVDescriptor(ID3D12Resource* resource, DXGI_FORMAT format, D3D12DescriptorHandle* dh);

	ID3D12GraphicsCommandList* GetCommandBufferForUpdate();
	ID3D12Resource* AllocateUploadStagingBuffer(const void* data, u32 pitch, u32 upload_pitch, u32 height) const;
	void CopyTextureDataForUpload(void* dst, const void* src, u32 pitch, u32 upload_pitch, u32 height) const;

	wil::com_ptr_nothrow<ID3D12Resource> m_resource;
	wil::com_ptr_nothrow<D3D12MA::Allocation> m_allocation;

	D3D12DescriptorHandle m_srv_descriptor = {};
	D3D12DescriptorHandle m_write_descriptor = {};
	D3D12DescriptorHandle m_uav_descriptor = {};
	WriteDescriptorType m_write_descriptor_type = WriteDescriptorType::None;

	DXGI_FORMAT m_dxgi_format = DXGI_FORMAT_UNKNOWN;
	D3D12_RESOURCE_STATES m_resource_state = D3D12_RESOURCE_STATE_COMMON;

	// Contains the fence counter when the texture was last used.
	// When this matches the current fence counter, the texture was used this command buffer.
	u64 m_use_fence_counter = 0;

	int m_map_level = std::numeric_limits<int>::max();
	GSVector4i m_map_area = GSVector4i::zero();
};

class GSDownloadTexture12 final : public GSDownloadTexture
{
public:
	~GSDownloadTexture12() override;

	static std::unique_ptr<GSDownloadTexture12> Create(u32 width, u32 height, GSTexture::Format format);

	void CopyFromTexture(
		const GSVector4i& drc, GSTexture* stex, const GSVector4i& src, u32 src_level, bool use_transfer_pitch) override;

	bool Map(const GSVector4i& read_rc) override;
	void Unmap() override;

	void Flush() override;

private:
	GSDownloadTexture12(u32 width, u32 height, GSTexture::Format format);

	wil::com_ptr_nothrow<D3D12MA::Allocation> m_allocation;
	wil::com_ptr_nothrow<ID3D12Resource> m_buffer;

	u64 m_copy_fence_value = 0;
	u32 m_buffer_size = 0;
};
