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

#include "PrecompiledHeader.h"

#include "GS/Renderers/DX12/GSTexture12.h"
#include "GS/Renderers/DX12/D3D12Builders.h"
#include "GS/Renderers/DX12/GSDevice12.h"
#include "GS/GSPerfMon.h"
#include "GS/GSGL.h"

#include "common/Assertions.h"
#include "common/BitUtils.h"
#include "common/StringUtil.h"

#include "D3D12MemAlloc.h"

GSTexture12::GSTexture12(Type type, Format format, int width, int height, int levels, DXGI_FORMAT dxgi_format,
	wil::com_ptr_nothrow<ID3D12Resource> resource, wil::com_ptr_nothrow<D3D12MA::Allocation> allocation,
	const D3D12DescriptorHandle& srv_descriptor, const D3D12DescriptorHandle& write_descriptor,
	const D3D12DescriptorHandle& uav_descriptor, WriteDescriptorType wdtype, D3D12_RESOURCE_STATES resource_state)
	: m_resource(std::move(resource))
	, m_allocation(std::move(allocation))
	, m_srv_descriptor(srv_descriptor)
	, m_write_descriptor(write_descriptor)
	, m_uav_descriptor(uav_descriptor)
	, m_write_descriptor_type(wdtype)
	, m_dxgi_format(dxgi_format)
	, m_resource_state(resource_state)
{
	m_type = type;
	m_format = format;
	m_size.x = width;
	m_size.y = height;
	m_mipmap_levels = levels;
}

GSTexture12::~GSTexture12()
{
	Destroy(true);
}

void GSTexture12::Destroy(bool defer)
{
	GSDevice12* const dev = GSDevice12::GetInstance();
	dev->UnbindTexture(this);

	if (defer)
	{
		dev->DeferDescriptorDestruction(dev->GetDescriptorHeapManager(), &m_srv_descriptor);

		switch (m_write_descriptor_type)
		{
			case WriteDescriptorType::RTV:
				dev->DeferDescriptorDestruction(dev->GetRTVHeapManager(), &m_write_descriptor);
				break;
			case WriteDescriptorType::DSV:
				dev->DeferDescriptorDestruction(dev->GetDSVHeapManager(), &m_write_descriptor);
				break;
			case WriteDescriptorType::None:
			default:
				break;
		}

		if (m_uav_descriptor)
			dev->DeferDescriptorDestruction(dev->GetDescriptorHeapManager(), &m_uav_descriptor);

		dev->DeferResourceDestruction(m_allocation.get(), m_resource.get());
		m_resource.reset();
		m_allocation.reset();
	}
	else
	{
		dev->GetDescriptorHeapManager().Free(&m_srv_descriptor);

		switch (m_write_descriptor_type)
		{
			case WriteDescriptorType::RTV:
				dev->GetRTVHeapManager().Free(&m_write_descriptor);
				break;
			case WriteDescriptorType::DSV:
				dev->GetDSVHeapManager().Free(&m_write_descriptor);
				break;
			case WriteDescriptorType::None:
			default:
				break;
		}

		if (m_uav_descriptor)
			dev->GetDescriptorHeapManager().Free(&m_uav_descriptor);

		m_resource.reset();
		m_allocation.reset();
	}

	m_write_descriptor_type = WriteDescriptorType::None;
}

std::unique_ptr<GSTexture12> GSTexture12::Create(Type type, Format format, int width, int height, int levels,
	DXGI_FORMAT dxgi_format, DXGI_FORMAT srv_format, DXGI_FORMAT rtv_format, DXGI_FORMAT dsv_format,
	DXGI_FORMAT uav_format)
{
	GSDevice12* const dev = GSDevice12::GetInstance();

	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = width;
	desc.Height = height;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = levels;
	desc.Format = dxgi_format;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	D3D12MA::ALLOCATION_DESC allocationDesc = {};
	allocationDesc.Flags = D3D12MA::ALLOCATION_FLAG_WITHIN_BUDGET;
	allocationDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CLEAR_VALUE optimized_clear_value = {};
	D3D12_RESOURCE_STATES state;

	switch (type)
	{
		case Type::Texture:
		{
			// This is a little annoying. basically, to do mipmap generation, we need to be a render target.
			// If it's a compressed texture, we won't be generating mips anyway, so this should be fine.
			desc.Flags = (levels > 1 && !IsCompressedFormat(format)) ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET :
																	   D3D12_RESOURCE_FLAG_NONE;
			state = D3D12_RESOURCE_STATE_COPY_DEST;
		}
		break;

		case Type::RenderTarget:
		{
			// RT's tend to be larger, so we'll keep them committed for speed.
			pxAssert(levels == 1);
			allocationDesc.Flags |= D3D12MA::ALLOCATION_FLAG_COMMITTED;
			desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
			optimized_clear_value.Format = rtv_format;
			state = D3D12_RESOURCE_STATE_RENDER_TARGET;
		}
		break;

		case Type::DepthStencil:
		{
			pxAssert(levels == 1);
			allocationDesc.Flags |= D3D12MA::ALLOCATION_FLAG_COMMITTED;
			desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			optimized_clear_value.Format = dsv_format;
			state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		}
		break;

		case Type::RWTexture:
		{
			pxAssert(levels == 1);
			allocationDesc.Flags |= D3D12MA::ALLOCATION_FLAG_COMMITTED;
			state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		}
		break;

		default:
			return {};
	}

	if (uav_format != DXGI_FORMAT_UNKNOWN)
		desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	wil::com_ptr_nothrow<ID3D12Resource> resource;
	wil::com_ptr_nothrow<D3D12MA::Allocation> allocation;
	HRESULT hr = dev->GetAllocator()->CreateResource(&allocationDesc, &desc, state,
		(type == Type::RenderTarget || type == Type::DepthStencil) ? &optimized_clear_value : nullptr, allocation.put(),
		IID_PPV_ARGS(resource.put()));
	if (FAILED(hr))
	{
		// OOM isn't fatal.
		if (hr != E_OUTOFMEMORY)
			Console.Error("Create texture failed: 0x%08X", hr);

		return {};
	}

	D3D12DescriptorHandle srv_descriptor, write_descriptor, uav_descriptor;
	WriteDescriptorType write_descriptor_type = WriteDescriptorType::None;
	if (srv_format != DXGI_FORMAT_UNKNOWN)
	{
		if (!CreateSRVDescriptor(resource.get(), levels, srv_format, &srv_descriptor))
			return {};
	}

	switch (type)
	{
		case Type::Texture:
		{
			D3D12::SetObjectNameFormatted(resource.get(), "%dx%d texture", width, height);
		}
		break;

		case Type::RenderTarget:
		{
			D3D12::SetObjectNameFormatted(resource.get(), "%dx%d render target", width, height);
			write_descriptor_type = WriteDescriptorType::RTV;
			if (!CreateRTVDescriptor(resource.get(), rtv_format, &write_descriptor))
			{
				dev->GetRTVHeapManager().Free(&srv_descriptor);
				return {};
			}
		}
		break;

		case Type::DepthStencil:
		{
			D3D12::SetObjectNameFormatted(resource.get(), "%dx%d depth stencil", width, height);
			write_descriptor_type = WriteDescriptorType::DSV;
			if (!CreateDSVDescriptor(resource.get(), dsv_format, &write_descriptor))
			{
				dev->GetDSVHeapManager().Free(&srv_descriptor);
				return {};
			}
		}
		break;

		case Type::RWTexture:
		{
			D3D12::SetObjectNameFormatted(resource.get(), "%dx%d RW texture", width, height);
		}
		break;

		default:
			break;
	}

	if (uav_format != DXGI_FORMAT_UNKNOWN && !CreateUAVDescriptor(resource.get(), dsv_format, &uav_descriptor))
	{
		dev->GetDescriptorHeapManager().Free(&write_descriptor);
		dev->GetDescriptorHeapManager().Free(&srv_descriptor);
		return {};
	}

	return std::unique_ptr<GSTexture12>(
		new GSTexture12(type, format, width, height, levels, dxgi_format, std::move(resource), std::move(allocation),
			srv_descriptor, write_descriptor, uav_descriptor, write_descriptor_type, state));
}

std::unique_ptr<GSTexture12> GSTexture12::Adopt(wil::com_ptr_nothrow<ID3D12Resource> resource, Type type, Format format,
	int width, int height, int levels, DXGI_FORMAT dxgi_format, DXGI_FORMAT srv_format, DXGI_FORMAT rtv_format,
	DXGI_FORMAT dsv_format, DXGI_FORMAT uav_format, D3D12_RESOURCE_STATES resource_state)
{
	const D3D12_RESOURCE_DESC desc = resource->GetDesc();

	D3D12DescriptorHandle srv_descriptor, write_descriptor, uav_descriptor;
	WriteDescriptorType write_descriptor_type = WriteDescriptorType::None;
	if (srv_format != DXGI_FORMAT_UNKNOWN)
	{
		if (!CreateSRVDescriptor(resource.get(), desc.MipLevels, srv_format, &srv_descriptor))
			return {};
	}

	if (type == Type::RenderTarget)
	{
		write_descriptor_type = WriteDescriptorType::RTV;
		if (!CreateRTVDescriptor(resource.get(), rtv_format, &write_descriptor))
		{
			GSDevice12::GetInstance()->GetRTVHeapManager().Free(&srv_descriptor);
			return {};
		}
	}
	else if (type == Type::DepthStencil)
	{
		write_descriptor_type = WriteDescriptorType::DSV;
		if (!CreateDSVDescriptor(resource.get(), dsv_format, &write_descriptor))
		{
			GSDevice12::GetInstance()->GetDSVHeapManager().Free(&srv_descriptor);
			return {};
		}
	}

	if (uav_format != DXGI_FORMAT_UNKNOWN)
	{
		if (!CreateUAVDescriptor(resource.get(), srv_format, &uav_descriptor))
		{
			GSDevice12::GetInstance()->GetDescriptorHeapManager().Free(&write_descriptor);
			GSDevice12::GetInstance()->GetDescriptorHeapManager().Free(&srv_descriptor);
			return {};
		}
	}

	return std::unique_ptr<GSTexture12>(new GSTexture12(type, format, static_cast<u32>(desc.Width), desc.Height,
		desc.MipLevels, desc.Format, std::move(resource), {}, srv_descriptor, write_descriptor, uav_descriptor,
		write_descriptor_type, resource_state));
}

bool GSTexture12::CreateSRVDescriptor(
	ID3D12Resource* resource, u32 levels, DXGI_FORMAT format, D3D12DescriptorHandle* dh)
{
	if (!GSDevice12::GetInstance()->GetDescriptorHeapManager().Allocate(dh))
	{
		Console.Error("Failed to allocate SRV descriptor");
		return false;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC desc = {
		format, D3D12_SRV_DIMENSION_TEXTURE2D, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING};
	desc.Texture2D.MipLevels = levels;

	GSDevice12::GetInstance()->GetDevice()->CreateShaderResourceView(resource, &desc, dh->cpu_handle);
	return true;
}

bool GSTexture12::CreateRTVDescriptor(ID3D12Resource* resource, DXGI_FORMAT format, D3D12DescriptorHandle* dh)
{
	if (!GSDevice12::GetInstance()->GetRTVHeapManager().Allocate(dh))
	{
		Console.Error("Failed to allocate SRV descriptor");
		return false;
	}

	const D3D12_RENDER_TARGET_VIEW_DESC desc = {format, D3D12_RTV_DIMENSION_TEXTURE2D};
	GSDevice12::GetInstance()->GetDevice()->CreateRenderTargetView(resource, &desc, dh->cpu_handle);
	return true;
}

bool GSTexture12::CreateDSVDescriptor(ID3D12Resource* resource, DXGI_FORMAT format, D3D12DescriptorHandle* dh)
{
	if (!GSDevice12::GetInstance()->GetDSVHeapManager().Allocate(dh))
	{
		Console.Error("Failed to allocate SRV descriptor");
		return false;
	}

	const D3D12_DEPTH_STENCIL_VIEW_DESC desc = {format, D3D12_DSV_DIMENSION_TEXTURE2D, D3D12_DSV_FLAG_NONE};
	GSDevice12::GetInstance()->GetDevice()->CreateDepthStencilView(resource, &desc, dh->cpu_handle);
	return true;
}

bool GSTexture12::CreateUAVDescriptor(ID3D12Resource* resource, DXGI_FORMAT format, D3D12DescriptorHandle* dh)
{
	if (!GSDevice12::GetInstance()->GetDescriptorHeapManager().Allocate(dh))
	{
		Console.Error("Failed to allocate UAV descriptor");
		return false;
	}

	const D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {format, D3D12_UAV_DIMENSION_TEXTURE2D};
	GSDevice12::GetInstance()->GetDevice()->CreateUnorderedAccessView(resource, nullptr, &desc, dh->cpu_handle);
	return true;
}

void* GSTexture12::GetNativeHandle() const
{
	return const_cast<GSTexture12*>(this);
}

ID3D12GraphicsCommandList* GSTexture12::GetCommandBufferForUpdate()
{
	GSDevice12* const dev = GSDevice12::GetInstance();
	if (m_type != Type::Texture || m_use_fence_counter == dev->GetCurrentFenceValue())
	{
		// Console.WriteLn("Texture update within frame, can't use do beforehand");
		GSDevice12::GetInstance()->EndRenderPass();
		return dev->GetCommandList();
	}

	return dev->GetInitCommandList();
}

ID3D12Resource* GSTexture12::AllocateUploadStagingBuffer(
	const void* data, u32 pitch, u32 upload_pitch, u32 height) const
{
	const u32 buffer_size = CalcUploadSize(height, upload_pitch);
	wil::com_ptr_nothrow<ID3D12Resource> resource;
	wil::com_ptr_nothrow<D3D12MA::Allocation> allocation;

	const D3D12MA::ALLOCATION_DESC allocation_desc = {D3D12MA::ALLOCATION_FLAG_NONE, D3D12_HEAP_TYPE_UPLOAD};
	const D3D12_RESOURCE_DESC resource_desc = {D3D12_RESOURCE_DIMENSION_BUFFER, 0, buffer_size, 1, 1, 1,
		DXGI_FORMAT_UNKNOWN, {1, 0}, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, D3D12_RESOURCE_FLAG_NONE};
	HRESULT hr = GSDevice12::GetInstance()->GetAllocator()->CreateResource(&allocation_desc, &resource_desc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, allocation.put(), IID_PPV_ARGS(resource.put()));
	if (FAILED(hr))
	{
		Console.WriteLn("(AllocateUploadStagingBuffer) CreateCommittedResource() failed with %08X", hr);
		return nullptr;
	}

	void* map_ptr;
	hr = resource->Map(0, nullptr, &map_ptr);
	if (FAILED(hr))
	{
		Console.WriteLn("(AllocateUploadStagingBuffer) Map() failed with %08X", hr);
		return nullptr;
	}

	CopyTextureDataForUpload(map_ptr, data, pitch, upload_pitch, height);

	const D3D12_RANGE write_range = {0, buffer_size};
	resource->Unmap(0, &write_range);

	// Immediately queue it for freeing after the command buffer finishes, since it's only needed for the copy.
	// This adds the reference needed to keep the buffer alive.
	GSDevice12::GetInstance()->DeferResourceDestruction(allocation.get(), resource.get());
	return resource.get();
}

void GSTexture12::CopyTextureDataForUpload(void* dst, const void* src, u32 pitch, u32 upload_pitch, u32 height) const
{
	const u32 block_size = GetCompressedBlockSize();
	const u32 count = (height + (block_size - 1)) / block_size;
	StringUtil::StrideMemCpy(dst, upload_pitch, src, pitch, std::min(upload_pitch, pitch), count);
}

bool GSTexture12::Update(const GSVector4i& r, const void* data, int pitch, int layer)
{
	if (layer >= m_mipmap_levels)
		return false;

	g_perfmon.Put(GSPerfMon::TextureUploads, 1);

	// Footprint and box must be block aligned for compressed textures.
	const u32 block_size = GetCompressedBlockSize();
	const u32 width = Common::AlignUpPow2(r.width(), block_size);
	const u32 height = Common::AlignUpPow2(r.height(), block_size);
	const u32 upload_pitch = Common::AlignUpPow2<u32>(pitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	const u32 required_size = CalcUploadSize(r.height(), upload_pitch);

	D3D12_TEXTURE_COPY_LOCATION srcloc;
	srcloc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	srcloc.PlacedFootprint.Footprint.Width = width;
	srcloc.PlacedFootprint.Footprint.Height = height;
	srcloc.PlacedFootprint.Footprint.Depth = 1;
	srcloc.PlacedFootprint.Footprint.Format = m_dxgi_format;
	srcloc.PlacedFootprint.Footprint.RowPitch = upload_pitch;

	// If the texture is larger than half our streaming buffer size, use a separate buffer.
	// Otherwise allocation will either fail, or require lots of cmdbuffer submissions.
	if (required_size > (GSDevice12::GetInstance()->GetTextureStreamBuffer().GetSize() / 2))
	{
		srcloc.pResource = AllocateUploadStagingBuffer(data, pitch, upload_pitch, height);
		if (!srcloc.pResource)
			return false;

		srcloc.PlacedFootprint.Offset = 0;
	}
	else
	{
		D3D12StreamBuffer& sbuffer = GSDevice12::GetInstance()->GetTextureStreamBuffer();
		if (!sbuffer.ReserveMemory(required_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT))
		{
			GSDevice12::GetInstance()->ExecuteCommandList(
				false, "While waiting for %u bytes in texture upload buffer", required_size);
			if (!sbuffer.ReserveMemory(required_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT))
			{
				Console.Error("Failed to reserve texture upload memory (%u bytes).", required_size);
				return false;
			}
		}

		srcloc.pResource = sbuffer.GetBuffer();
		srcloc.PlacedFootprint.Offset = sbuffer.GetCurrentOffset();
		CopyTextureDataForUpload(sbuffer.GetCurrentHostPointer(), data, pitch, upload_pitch, height);
		sbuffer.CommitMemory(required_size);
	}

	ID3D12GraphicsCommandList* cmdlist = GetCommandBufferForUpdate();
	GL_PUSH("GSTexture12::Update({%d,%d} %dx%d Lvl:%u", r.x, r.y, r.width(), r.height(), layer);

	// first time the texture is used? don't leave it undefined
	if (m_resource_state == D3D12_RESOURCE_STATE_COMMON)
		TransitionToState(cmdlist, D3D12_RESOURCE_STATE_COPY_DEST);
	else if (m_resource_state != D3D12_RESOURCE_STATE_COPY_DEST)
		TransitionSubresourceToState(cmdlist, layer, m_resource_state, D3D12_RESOURCE_STATE_COPY_DEST);

	// if we're an rt and have been cleared, and the full rect isn't being uploaded, do the clear
	if (m_type == Type::RenderTarget)
	{
		if (!r.eq(GSVector4i(0, 0, m_size.x, m_size.y)))
			CommitClear(cmdlist);
		else
			m_state = State::Dirty;
	}

	D3D12_TEXTURE_COPY_LOCATION dstloc;
	dstloc.pResource = m_resource.get();
	dstloc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dstloc.SubresourceIndex = layer;

	const D3D12_BOX srcbox{0u, 0u, 0u, width, height, 1u};
	cmdlist->CopyTextureRegion(&dstloc, Common::AlignDownPow2((u32)r.x, block_size),
		Common::AlignDownPow2((u32)r.y, block_size), 0, &srcloc, &srcbox);

	if (m_resource_state != D3D12_RESOURCE_STATE_COPY_DEST)
		TransitionSubresourceToState(cmdlist, layer, D3D12_RESOURCE_STATE_COPY_DEST, m_resource_state);

	if (m_type == Type::Texture)
		m_needs_mipmaps_generated |= (layer == 0);

	return true;
}

bool GSTexture12::Map(GSMap& m, const GSVector4i* r, int layer)
{
	if (layer >= m_mipmap_levels || IsCompressedFormat())
		return false;

	// map for writing
	m_map_area = r ? *r : GetRect();
	m_map_level = layer;
	m.pitch = Common::AlignUpPow2(CalcUploadPitch(m_map_area.width()), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

	// see note in Update() for the reason why.
	const u32 required_size = CalcUploadSize(m_map_area.height(), m.pitch);
	D3D12StreamBuffer& buffer = GSDevice12::GetInstance()->GetTextureStreamBuffer();
	if (required_size >= (buffer.GetSize() / 2))
		return false;

	if (!buffer.ReserveMemory(required_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT))
	{
		GSDevice12::GetInstance()->ExecuteCommandList(
			false, "While waiting for %u bytes in texture upload buffer", required_size);
		if (!buffer.ReserveMemory(required_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT))
			pxFailRel("Failed to reserve texture upload memory");
	}

	m.bits = static_cast<u8*>(buffer.GetCurrentHostPointer());
	return true;
}

void GSTexture12::Unmap()
{
	// this can't handle blocks/compressed formats at the moment.
	pxAssert(m_map_level < m_mipmap_levels && !IsCompressedFormat());
	g_perfmon.Put(GSPerfMon::TextureUploads, 1);

	const u32 width = m_map_area.width();
	const u32 height = m_map_area.height();
	const u32 pitch = Common::AlignUpPow2(CalcUploadPitch(width), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	const u32 required_size = CalcUploadSize(height, pitch);
	D3D12StreamBuffer& buffer = GSDevice12::GetInstance()->GetTextureStreamBuffer();
	const u32 buffer_offset = buffer.GetCurrentOffset();
	buffer.CommitMemory(required_size);

	ID3D12GraphicsCommandList* cmdlist = GetCommandBufferForUpdate();
	GL_PUSH("GSTexture12::Update({%d,%d} %dx%d Lvl:%u", m_map_area.x, m_map_area.y, m_map_area.width(),
		m_map_area.height(), m_map_level);

	// first time the texture is used? don't leave it undefined
	if (m_resource_state == D3D12_RESOURCE_STATE_COMMON)
		TransitionToState(cmdlist, D3D12_RESOURCE_STATE_COPY_DEST);
	else if (m_resource_state != D3D12_RESOURCE_STATE_COPY_DEST)
		TransitionSubresourceToState(cmdlist, m_map_level, m_resource_state, D3D12_RESOURCE_STATE_COPY_DEST);

	// if we're an rt and have been cleared, and the full rect isn't being uploaded, do the clear
	if (m_type == Type::RenderTarget)
	{
		if (!m_map_area.eq(GSVector4i(0, 0, m_size.x, m_size.y)))
			CommitClear(cmdlist);
		else
			m_state = State::Dirty;
	}

	D3D12_TEXTURE_COPY_LOCATION srcloc;
	srcloc.pResource = buffer.GetBuffer();
	srcloc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	srcloc.PlacedFootprint.Offset = buffer_offset;
	srcloc.PlacedFootprint.Footprint.Width = width;
	srcloc.PlacedFootprint.Footprint.Height = height;
	srcloc.PlacedFootprint.Footprint.Depth = 1;
	srcloc.PlacedFootprint.Footprint.Format = m_dxgi_format;
	srcloc.PlacedFootprint.Footprint.RowPitch = pitch;

	D3D12_TEXTURE_COPY_LOCATION dstloc;
	dstloc.pResource = m_resource.get();
	dstloc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dstloc.SubresourceIndex = m_map_level;

	const D3D12_BOX srcbox{0u, 0u, 0u, width, height, 1};
	cmdlist->CopyTextureRegion(&dstloc, m_map_area.x, m_map_area.y, 0, &srcloc, &srcbox);

	if (m_resource_state != D3D12_RESOURCE_STATE_COPY_DEST)
		TransitionSubresourceToState(cmdlist, m_map_level, D3D12_RESOURCE_STATE_COPY_DEST, m_resource_state);

	if (m_type == Type::Texture)
		m_needs_mipmaps_generated |= (m_map_level == 0);
}

void GSTexture12::GenerateMipmap()
{
	pxAssert(!IsCompressedFormat(m_format));

	for (int dst_level = 1; dst_level < m_mipmap_levels; dst_level++)
	{
		const int src_level = dst_level - 1;
		const int src_width = std::max<int>(m_size.x >> src_level, 1);
		const int src_height = std::max<int>(m_size.y >> src_level, 1);
		const int dst_width = std::max<int>(m_size.x >> dst_level, 1);
		const int dst_height = std::max<int>(m_size.y >> dst_level, 1);

		GSDevice12::GetInstance()->RenderTextureMipmap(
			this, dst_level, dst_width, dst_height, src_level, src_width, src_height);
	}

	SetUseFenceCounter(GSDevice12::GetInstance()->GetCurrentFenceValue());
}

void GSTexture12::Swap(GSTexture* tex)
{
	GSTexture::Swap(tex);
	std::swap(m_resource, static_cast<GSTexture12*>(tex)->m_resource);
	std::swap(m_allocation, static_cast<GSTexture12*>(tex)->m_allocation);
	std::swap(m_srv_descriptor, static_cast<GSTexture12*>(tex)->m_srv_descriptor);
	std::swap(m_write_descriptor, static_cast<GSTexture12*>(tex)->m_write_descriptor);
	std::swap(m_write_descriptor_type, static_cast<GSTexture12*>(tex)->m_write_descriptor_type);
	std::swap(m_dxgi_format, static_cast<GSTexture12*>(tex)->m_dxgi_format);
	std::swap(m_resource_state, static_cast<GSTexture12*>(tex)->m_resource_state);
	std::swap(m_use_fence_counter, static_cast<GSTexture12*>(tex)->m_use_fence_counter);
	std::swap(m_clear_value, static_cast<GSTexture12*>(tex)->m_clear_value);
	std::swap(m_map_level, static_cast<GSTexture12*>(tex)->m_map_level);
	std::swap(m_map_area, static_cast<GSTexture12*>(tex)->m_map_area);
}

void GSTexture12::TransitionToState(D3D12_RESOURCE_STATES state)
{
	TransitionToState(GSDevice12::GetInstance()->GetCommandList(), state);
}

void GSTexture12::TransitionToState(ID3D12GraphicsCommandList* cmdlist, D3D12_RESOURCE_STATES state)
{
	if (m_resource_state == state)
		return;

	TransitionSubresourceToState(cmdlist, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, m_resource_state, state);
	m_resource_state = state;
}

void GSTexture12::TransitionSubresourceToState(ID3D12GraphicsCommandList* cmdlist, int level,
	D3D12_RESOURCE_STATES before_state, D3D12_RESOURCE_STATES after_state) const
{
	const D3D12_RESOURCE_BARRIER barrier = {D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, D3D12_RESOURCE_BARRIER_FLAG_NONE,
		{{m_resource.get(), static_cast<u32>(level), before_state, after_state}}};
	cmdlist->ResourceBarrier(1, &barrier);
}

void GSTexture12::CommitClear()
{
	if (m_state != GSTexture::State::Cleared)
		return;

	GSDevice12::GetInstance()->EndRenderPass();

	CommitClear(GSDevice12::GetInstance()->GetCommandList());
}

void GSTexture12::CommitClear(ID3D12GraphicsCommandList* cmdlist)
{
	if (IsDepthStencil())
	{
		TransitionToState(cmdlist, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		cmdlist->ClearDepthStencilView(
			GetWriteDescriptor(), D3D12_CLEAR_FLAG_DEPTH, m_clear_value.depth, 0, 0, nullptr);
	}
	else
	{
		TransitionToState(cmdlist, D3D12_RESOURCE_STATE_RENDER_TARGET);
		cmdlist->ClearRenderTargetView(GetWriteDescriptor(), GSVector4::unorm8(m_clear_value.color).v, 0, nullptr);
	}

	SetState(GSTexture::State::Dirty);
}

GSDownloadTexture12::GSDownloadTexture12(u32 width, u32 height, GSTexture::Format format)
	: GSDownloadTexture(width, height, format)
{
}

GSDownloadTexture12::~GSDownloadTexture12()
{
	if (IsMapped())
		GSDownloadTexture12::Unmap();

	if (m_buffer)
		GSDevice12::GetInstance()->DeferResourceDestruction(m_allocation.get(), m_buffer.get());
}

std::unique_ptr<GSDownloadTexture12> GSDownloadTexture12::Create(u32 width, u32 height, GSTexture::Format format)
{
	const u32 buffer_size = GetBufferSize(width, height, format, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

	D3D12MA::ALLOCATION_DESC allocation_desc = {};
	allocation_desc.HeapType = D3D12_HEAP_TYPE_READBACK;

	const D3D12_RESOURCE_DESC resource_desc = {D3D12_RESOURCE_DIMENSION_BUFFER, 0, buffer_size, 1, 1, 1,
		DXGI_FORMAT_UNKNOWN, {1, 0}, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, D3D12_RESOURCE_FLAG_NONE};

	wil::com_ptr_nothrow<D3D12MA::Allocation> allocation;
	wil::com_ptr_nothrow<ID3D12Resource> buffer;

	HRESULT hr = GSDevice12::GetInstance()->GetAllocator()->CreateResource(&allocation_desc, &resource_desc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, allocation.put(), IID_PPV_ARGS(buffer.put()));
	if (FAILED(hr))
	{
		Console.Error("(GSDownloadTexture12::Create) CreateResource() failed with HRESULT %08X", hr);
		return {};
	}

	std::unique_ptr<GSDownloadTexture12> tex(new GSDownloadTexture12(width, height, format));
	tex->m_allocation = std::move(allocation);
	tex->m_buffer = std::move(buffer);
	tex->m_buffer_size = buffer_size;
	return tex;
}

void GSDownloadTexture12::CopyFromTexture(
	const GSVector4i& drc, GSTexture* stex, const GSVector4i& src, u32 src_level, bool use_transfer_pitch)
{
	GSTexture12* const tex12 = static_cast<GSTexture12*>(stex);

	pxAssert(tex12->GetFormat() == m_format);
	pxAssert(drc.width() == src.width() && drc.height() == src.height());
	pxAssert(src.z <= tex12->GetWidth() && src.w <= tex12->GetHeight());
	pxAssert(static_cast<u32>(drc.z) <= m_width && static_cast<u32>(drc.w) <= m_height);
	pxAssert(src_level < static_cast<u32>(tex12->GetMipmapLevels()));
	pxAssert((drc.left == 0 && drc.top == 0) || !use_transfer_pitch);

	u32 copy_offset, copy_size, copy_rows;
	m_current_pitch = GetTransferPitch(
		use_transfer_pitch ? static_cast<u32>(drc.width()) : m_width, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	GetTransferSize(drc, &copy_offset, &copy_size, &copy_rows);

	g_perfmon.Put(GSPerfMon::Readbacks, 1);
	GSDevice12::GetInstance()->EndRenderPass();
	tex12->CommitClear();

	if (IsMapped())
		Unmap();

	ID3D12GraphicsCommandList* cmdlist = GSDevice12::GetInstance()->GetCommandList();
	GL_INS("ReadbackTexture: {%d,%d} %ux%u", src.left, src.top, src.width(), src.height());

	D3D12_TEXTURE_COPY_LOCATION srcloc;
	srcloc.pResource = tex12->GetResource();
	srcloc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	srcloc.SubresourceIndex = src_level;

	D3D12_TEXTURE_COPY_LOCATION dstloc;
	dstloc.pResource = m_buffer.get();
	dstloc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dstloc.PlacedFootprint.Offset = copy_offset;
	dstloc.PlacedFootprint.Footprint.Format = tex12->GetDXGIFormat();
	dstloc.PlacedFootprint.Footprint.Width = drc.width();
	dstloc.PlacedFootprint.Footprint.Height = drc.height();
	dstloc.PlacedFootprint.Footprint.Depth = 1;
	dstloc.PlacedFootprint.Footprint.RowPitch = m_current_pitch;

	const D3D12_RESOURCE_STATES old_layout = tex12->GetResourceState();
	if (old_layout != D3D12_RESOURCE_STATE_COPY_SOURCE)
		tex12->TransitionSubresourceToState(cmdlist, src_level, old_layout, D3D12_RESOURCE_STATE_COPY_SOURCE);

	// TODO: Rules for depth buffers here?
	const D3D12_BOX srcbox{static_cast<UINT>(src.left), static_cast<UINT>(src.top), 0u, static_cast<UINT>(src.right),
		static_cast<UINT>(src.bottom), 1u};
	cmdlist->CopyTextureRegion(&dstloc, 0, 0, 0, &srcloc, &srcbox);

	if (old_layout != D3D12_RESOURCE_STATE_COPY_SOURCE)
		tex12->TransitionSubresourceToState(cmdlist, src_level, D3D12_RESOURCE_STATE_COPY_SOURCE, old_layout);

	m_copy_fence_value = GSDevice12::GetInstance()->GetCurrentFenceValue();
	m_needs_flush = true;
}

bool GSDownloadTexture12::Map(const GSVector4i& read_rc)
{
	if (IsMapped())
		return true;

	// Never populated?
	if (!m_current_pitch)
		return false;

	u32 copy_offset, copy_size, copy_rows;
	GetTransferSize(read_rc, &copy_offset, &copy_size, &copy_rows);

	const D3D12_RANGE read_range{copy_offset, copy_offset + copy_size};
	const HRESULT hr = m_buffer->Map(0, &read_range, reinterpret_cast<void**>(const_cast<u8**>(&m_map_pointer)));
	if (FAILED(hr))
	{
		Console.Error("(GSDownloadTexture12::Map) Map() failed with HRESULT %08X", hr);
		return false;
	}

	return true;
}

void GSDownloadTexture12::Unmap()
{
	if (!IsMapped())
		return;

	const D3D12_RANGE write_range = {};
	m_buffer->Unmap(0, &write_range);
	m_map_pointer = nullptr;
}

void GSDownloadTexture12::Flush()
{
	if (!m_needs_flush)
		return;

	m_needs_flush = false;

	GSDevice12* dev = GSDevice12::GetInstance();
	if (dev->GetCompletedFenceValue() >= m_copy_fence_value)
		return;

	// Need to execute command buffer.
	if (dev->GetCurrentFenceValue() == m_copy_fence_value)
		dev->ExecuteCommandListForReadback();
	else
		dev->WaitForFence(m_copy_fence_value, GSConfig.HWSpinGPUForReadbacks);
}
