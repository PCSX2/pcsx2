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

#include "common/PrecompiledHeader.h"

#include "common/D3D12/Texture.h"
#include "common/D3D12/Context.h"
#include "common/D3D12/Util.h"
#include "common/Align.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "common/StringUtil.h"
#include "D3D12MemAlloc.h"

using namespace D3D12;

// Somehow NOMINMAX isn't getting set for CMake builds...
#ifdef min
#undef min
#endif

Texture::Texture() = default;

Texture::Texture(ID3D12Resource* resource, D3D12_RESOURCE_STATES state)
	: m_resource(std::move(resource))
{
	const D3D12_RESOURCE_DESC desc = GetDesc();
	m_width = static_cast<u32>(desc.Width);
	m_height = desc.Height;
	m_levels = desc.MipLevels;
	m_format = desc.Format;
}

Texture::Texture(Texture&& texture)
	: m_resource(std::move(texture.m_resource))
	, m_allocation(std::move(texture.m_allocation))
	, m_srv_descriptor(texture.m_srv_descriptor)
	, m_rtv_or_dsv_descriptor(texture.m_rtv_or_dsv_descriptor)
	, m_width(texture.m_width)
	, m_height(texture.m_height)
	, m_levels(texture.m_levels)
	, m_format(texture.m_format)
	, m_state(texture.m_state)
	, m_is_depth_view(texture.m_is_depth_view)
{
	texture.m_srv_descriptor = {};
	texture.m_rtv_or_dsv_descriptor = {};
	texture.m_width = 0;
	texture.m_height = 0;
	texture.m_levels = 0;
	texture.m_format = DXGI_FORMAT_UNKNOWN;
	texture.m_state = D3D12_RESOURCE_STATE_COMMON;
	texture.m_is_depth_view = false;
}

Texture::~Texture()
{
	Destroy();
}

Texture& Texture::operator=(Texture&& texture)
{
	Destroy();
	m_resource = std::move(texture.m_resource);
	m_allocation = std::move(texture.m_allocation);
	m_srv_descriptor = texture.m_srv_descriptor;
	m_rtv_or_dsv_descriptor = texture.m_rtv_or_dsv_descriptor;
	m_width = texture.m_width;
	m_height = texture.m_height;
	m_levels = texture.m_levels;
	m_format = texture.m_format;
	m_state = texture.m_state;
	m_is_depth_view = texture.m_is_depth_view;
	texture.m_srv_descriptor = {};
	texture.m_rtv_or_dsv_descriptor = {};
	texture.m_width = 0;
	texture.m_height = 0;
	texture.m_levels = 0;
	texture.m_format = DXGI_FORMAT_UNKNOWN;
	texture.m_state = D3D12_RESOURCE_STATE_COMMON;
	texture.m_is_depth_view = false;
	return *this;
}

D3D12_RESOURCE_DESC Texture::GetDesc() const
{
	return m_resource->GetDesc();
}

bool Texture::Create(u32 width, u32 height, u32 levels, DXGI_FORMAT format, DXGI_FORMAT srv_format,
	DXGI_FORMAT rtv_format, DXGI_FORMAT dsv_format, D3D12_RESOURCE_FLAGS flags, u32 alloc_flags)
{
	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = width;
	desc.Height = height;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = levels;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = flags;

	D3D12_CLEAR_VALUE optimized_clear_value = {};
	D3D12_RESOURCE_STATES state;
	if (rtv_format != DXGI_FORMAT_UNKNOWN)
	{
		optimized_clear_value.Format = rtv_format;
		state = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}
	else if (dsv_format != DXGI_FORMAT_UNKNOWN)
	{
		optimized_clear_value.Format = dsv_format;
		state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}
	else
	{
		state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}

	D3D12MA::ALLOCATION_DESC allocationDesc = {};
	allocationDesc.Flags = static_cast<D3D12MA::ALLOCATION_FLAGS>(alloc_flags) | D3D12MA::ALLOCATION_FLAG_WITHIN_BUDGET;
	allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

	ComPtr<ID3D12Resource> resource;
	ComPtr<D3D12MA::Allocation> allocation;
	HRESULT hr = g_d3d12_context->GetAllocator()->CreateResource(
		&allocationDesc, &desc, state,
		(rtv_format != DXGI_FORMAT_UNKNOWN || dsv_format != DXGI_FORMAT_UNKNOWN) ? &optimized_clear_value : nullptr,
		allocation.put(), IID_PPV_ARGS(resource.put()));
	if (FAILED(hr))
	{
		// OOM isn't fatal.
		if (hr != E_OUTOFMEMORY)
			Console.Error("Create texture failed: 0x%08X", hr);

		return false;
	}

	DescriptorHandle srv_descriptor, rtv_descriptor;
	bool is_depth_view = false;
	if (srv_format != DXGI_FORMAT_UNKNOWN)
	{
		if (!CreateSRVDescriptor(resource.get(), levels, srv_format, &srv_descriptor))
			return false;
	}

	if (rtv_format != DXGI_FORMAT_UNKNOWN)
	{
		pxAssert(dsv_format == DXGI_FORMAT_UNKNOWN);
		if (!CreateRTVDescriptor(resource.get(), rtv_format, &rtv_descriptor))
		{
			g_d3d12_context->GetDescriptorHeapManager().Free(&srv_descriptor);
			return false;
		}
	}
	else if (dsv_format != DXGI_FORMAT_UNKNOWN)
	{
		if (!CreateDSVDescriptor(resource.get(), dsv_format, &rtv_descriptor))
		{
			g_d3d12_context->GetDescriptorHeapManager().Free(&srv_descriptor);
			return false;
		}

		is_depth_view = true;
	}

	Destroy(true);

	m_resource = std::move(resource);
	m_allocation = std::move(allocation);
	m_srv_descriptor = std::move(srv_descriptor);
	m_rtv_or_dsv_descriptor = std::move(rtv_descriptor);
	m_width = width;
	m_height = height;
	m_levels = levels;
	m_format = format;
	m_state = state;
	m_is_depth_view = is_depth_view;
	return true;
}

bool Texture::Adopt(ComPtr<ID3D12Resource> texture, DXGI_FORMAT srv_format, DXGI_FORMAT rtv_format,
	DXGI_FORMAT dsv_format, D3D12_RESOURCE_STATES state)
{
	const D3D12_RESOURCE_DESC desc(texture->GetDesc());

	DescriptorHandle srv_descriptor, rtv_descriptor;
	if (srv_format != DXGI_FORMAT_UNKNOWN)
	{
		if (!CreateSRVDescriptor(texture.get(), desc.MipLevels, srv_format, &srv_descriptor))
			return false;
	}

	if (rtv_format != DXGI_FORMAT_UNKNOWN)
	{
		pxAssert(dsv_format == DXGI_FORMAT_UNKNOWN);
		if (!CreateRTVDescriptor(texture.get(), rtv_format, &rtv_descriptor))
		{
			g_d3d12_context->GetDescriptorHeapManager().Free(&srv_descriptor);
			return false;
		}
	}
	else if (dsv_format != DXGI_FORMAT_UNKNOWN)
	{
		if (!CreateDSVDescriptor(texture.get(), dsv_format, &rtv_descriptor))
		{
			g_d3d12_context->GetDescriptorHeapManager().Free(&srv_descriptor);
			return false;
		}
	}

	m_resource = std::move(texture);
	m_allocation.reset();
	m_srv_descriptor = std::move(srv_descriptor);
	m_rtv_or_dsv_descriptor = std::move(rtv_descriptor);
	m_width = static_cast<u32>(desc.Width);
	m_height = desc.Height;
	m_levels = desc.MipLevels;
	m_format = desc.Format;
	m_state = state;
	return true;
}

void Texture::Destroy(bool defer /* = true */)
{
	if (defer)
	{
		g_d3d12_context->DeferDescriptorDestruction(g_d3d12_context->GetDescriptorHeapManager(), &m_srv_descriptor);
		if (m_is_depth_view)
			g_d3d12_context->DeferDescriptorDestruction(g_d3d12_context->GetDSVHeapManager(), &m_rtv_or_dsv_descriptor);
		else
			g_d3d12_context->DeferDescriptorDestruction(g_d3d12_context->GetRTVHeapManager(), &m_rtv_or_dsv_descriptor);
		g_d3d12_context->DeferResourceDestruction(m_allocation.get(), m_resource.get());
		m_resource.reset();
		m_allocation.reset();
	}
	else
	{
		g_d3d12_context->GetDescriptorHeapManager().Free(&m_srv_descriptor);
		if (m_is_depth_view)
			g_d3d12_context->GetDSVHeapManager().Free(&m_rtv_or_dsv_descriptor);
		else
			g_d3d12_context->GetRTVHeapManager().Free(&m_rtv_or_dsv_descriptor);

		m_resource.reset();
		m_allocation.reset();
	}

	m_width = 0;
	m_height = 0;
	m_levels = 0;
	m_format = DXGI_FORMAT_UNKNOWN;
	m_is_depth_view = false;
}

void Texture::TransitionToState(ID3D12GraphicsCommandList* cmdlist, D3D12_RESOURCE_STATES state)
{
	if (m_state == state)
		return;

	ResourceBarrier(cmdlist, m_resource.get(), m_state, state);
	m_state = state;
}

void Texture::TransitionSubresourceToState(ID3D12GraphicsCommandList* cmdlist, u32 level,
	D3D12_RESOURCE_STATES before_state, D3D12_RESOURCE_STATES after_state) const
{
	const D3D12_RESOURCE_BARRIER barrier = {D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		D3D12_RESOURCE_BARRIER_FLAG_NONE,
		{{m_resource.get(), level, before_state, after_state}}};
	cmdlist->ResourceBarrier(1, &barrier);
}

bool Texture::BeginStreamUpdate(u32 level, u32 x, u32 y, u32 width, u32 height, void** out_data, u32* out_data_pitch)
{
	const u32 copy_pitch = Common::AlignUpPow2(width * GetTexelSize(m_format), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	const u32 upload_size = copy_pitch * height;

	if (!g_d3d12_context->GetTextureStreamBuffer().ReserveMemory(upload_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT))
	{
		DevCon.WriteLn("Executing command buffer while waiting for %u bytes (%ux%u) in upload buffer", upload_size, width,
			height);
		g_d3d12_context->ExecuteCommandList(false);
		if (!g_d3d12_context->GetTextureStreamBuffer().ReserveMemory(upload_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT))
		{
			Console.Error("Failed to reserve %u bytes for %ux%u upload", upload_size, width, height);
			return false;
		}
	}

	*out_data = g_d3d12_context->GetTextureStreamBuffer().GetCurrentHostPointer();
	*out_data_pitch = copy_pitch;
	return true;
}

void Texture::EndStreamUpdate(u32 level, u32 x, u32 y, u32 width, u32 height)
{
	const u32 copy_pitch = Common::AlignUpPow2(width * GetTexelSize(m_format), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	const u32 upload_size = copy_pitch * height;

	StreamBuffer& sb = g_d3d12_context->GetTextureStreamBuffer();
	const u32 sb_offset = sb.GetCurrentOffset();
	sb.CommitMemory(upload_size);

	CopyFromBuffer(level, x, y, width, height, copy_pitch, sb.GetBuffer(), sb_offset);
}

void Texture::CopyFromBuffer(u32 level, u32 x, u32 y, u32 width, u32 height, u32 pitch, ID3D12Resource* buffer, u32 buffer_offset)
{
	D3D12_TEXTURE_COPY_LOCATION src;
	src.pResource = buffer;
	src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	src.PlacedFootprint.Offset = buffer_offset;
	src.PlacedFootprint.Footprint.Width = width;
	src.PlacedFootprint.Footprint.Height = height;
	src.PlacedFootprint.Footprint.Depth = 1;
	src.PlacedFootprint.Footprint.RowPitch = pitch;
	src.PlacedFootprint.Footprint.Format = m_format;

	D3D12_TEXTURE_COPY_LOCATION dst;
	dst.pResource = m_resource.get();
	dst.SubresourceIndex = level;
	dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

	const D3D12_BOX src_box{0u, 0u, 0u, width, height, 1u};
	const D3D12_RESOURCE_STATES old_state = m_state;
	ID3D12GraphicsCommandList* cmdlist = g_d3d12_context->GetCommandList();
	TransitionToState(cmdlist, D3D12_RESOURCE_STATE_COPY_DEST);
	cmdlist->CopyTextureRegion(&dst, x, y, 0, &src, &src_box);
	TransitionToState(cmdlist, old_state);
}

static ID3D12Resource* CreateStagingBuffer(u32 height, const void* data, u32 pitch, u32 upload_pitch, u32 upload_size)
{
	wil::com_ptr_nothrow<ID3D12Resource> resource;
	wil::com_ptr_nothrow<D3D12MA::Allocation> allocation;

	const D3D12MA::ALLOCATION_DESC allocation_desc = {D3D12MA::ALLOCATION_FLAG_NONE, D3D12_HEAP_TYPE_UPLOAD};
	const D3D12_RESOURCE_DESC resource_desc = {
		D3D12_RESOURCE_DIMENSION_BUFFER, 0, upload_size, 1, 1, 1, DXGI_FORMAT_UNKNOWN, {1, 0}, D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		D3D12_RESOURCE_FLAG_NONE};
	HRESULT hr = g_d3d12_context->GetAllocator()->CreateResource(&allocation_desc, &resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr, allocation.put(), IID_PPV_ARGS(resource.put()));
	if (FAILED(hr))
	{
		Console.Error("CreateResource() for upload staging buffer failed: %08X", hr);
		return nullptr;
	}

	void* map;
	const D3D12_RANGE read_range = {};
	hr = resource->Map(0, &read_range, &map);
	if (FAILED(hr))
	{
		Console.Error("Map() for upload staging buffer failed: %08X", hr);
		return nullptr;
	}

	StringUtil::StrideMemCpy(map, upload_pitch, data, pitch, std::min(pitch, upload_pitch), height);

	const D3D12_RANGE write_range = {0u, upload_size};
	resource->Unmap(0, &write_range);

	// queue them for destruction, since the upload happens in this cmdlist
	g_d3d12_context->DeferResourceDestruction(allocation.get(), resource.get());

	// AddRef()'ed by the defer above.
	return resource.get();
}

bool Texture::LoadData(u32 level, u32 x, u32 y, u32 width, u32 height, const void* data, u32 pitch)
{
	const u32 texel_size = GetTexelSize(m_format);
	const u32 upload_pitch = Common::AlignUpPow2(width * texel_size, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	const u32 upload_size = upload_pitch * height;
	if (upload_size >= g_d3d12_context->GetTextureStreamBuffer().GetSize())
	{
		ID3D12Resource* staging_buffer = CreateStagingBuffer(height, data, pitch, upload_pitch, upload_size);
		if (!staging_buffer)
			return false;

		CopyFromBuffer(level, x, y, width, height, upload_pitch, staging_buffer, 0);
		return true;
	}

	void* write_ptr;
	u32 write_pitch;
	if (!BeginStreamUpdate(level, x, y, width, height, &write_ptr, &write_pitch))
		return false;

	StringUtil::StrideMemCpy(write_ptr, write_pitch, data, pitch, std::min(pitch, upload_pitch), height);
	EndStreamUpdate(level, x, y, width, height);
	return true;
}

bool Texture::CreateSRVDescriptor(ID3D12Resource* resource, u32 levels, DXGI_FORMAT format, DescriptorHandle* dh)
{
	if (!g_d3d12_context->GetDescriptorHeapManager().Allocate(dh))
	{
		Console.Error("Failed to allocate SRV descriptor");
		return false;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {format, D3D12_SRV_DIMENSION_TEXTURE2D, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING};
	desc.Texture2D.MipLevels = levels;

	g_d3d12_context->GetDevice()->CreateShaderResourceView(resource, &desc, dh->cpu_handle);
	return true;
}

bool Texture::CreateRTVDescriptor(ID3D12Resource* resource, DXGI_FORMAT format, DescriptorHandle* dh)
{
	if (!g_d3d12_context->GetRTVHeapManager().Allocate(dh))
	{
		Console.Error("Failed to allocate SRV descriptor");
		return false;
	}

	const D3D12_RENDER_TARGET_VIEW_DESC desc = {format, D3D12_RTV_DIMENSION_TEXTURE2D};
	g_d3d12_context->GetDevice()->CreateRenderTargetView(resource, &desc, dh->cpu_handle);
	return true;
}

bool Texture::CreateDSVDescriptor(ID3D12Resource* resource, DXGI_FORMAT format, DescriptorHandle* dh)
{
	if (!g_d3d12_context->GetDSVHeapManager().Allocate(dh))
	{
		Console.Error("Failed to allocate SRV descriptor");
		return false;
	}

	const D3D12_DEPTH_STENCIL_VIEW_DESC desc = {format, D3D12_DSV_DIMENSION_TEXTURE2D, D3D12_DSV_FLAG_NONE};
	g_d3d12_context->GetDevice()->CreateDepthStencilView(resource, &desc, dh->cpu_handle);
	return true;
}
