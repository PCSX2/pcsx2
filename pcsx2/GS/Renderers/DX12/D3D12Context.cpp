/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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

#include "GS/Renderers/DX12/D3D12Context.h"
#include "common/Assertions.h"
#include "common/General.h"
#include "common/ScopedGuard.h"
#include "common/Console.h"
#include "D3D12MemAlloc.h"

#include <algorithm>
#include <array>
#include <dxgi1_4.h>
#include <queue>
#include <vector>

std::unique_ptr<D3D12Context> g_d3d12_context;

D3D12Context::D3D12Context() = default;

D3D12Context::~D3D12Context()
{
	DestroyResources();
}

D3D12Context::ComPtr<ID3DBlob> D3D12Context::SerializeRootSignature(const D3D12_ROOT_SIGNATURE_DESC* desc)
{
	ComPtr<ID3DBlob> blob;
	ComPtr<ID3DBlob> error_blob;
	const HRESULT hr = D3D12SerializeRootSignature(desc, D3D_ROOT_SIGNATURE_VERSION_1, blob.put(), error_blob.put());
	if (FAILED(hr))
	{
		Console.Error("D3D12SerializeRootSignature() failed: %08X", hr);
		if (error_blob)
			Console.Error("%s", error_blob->GetBufferPointer());

		return {};
	}

	return blob;
}

D3D12Context::ComPtr<ID3D12RootSignature> D3D12Context::CreateRootSignature(const D3D12_ROOT_SIGNATURE_DESC* desc)
{
	ComPtr<ID3DBlob> blob = SerializeRootSignature(desc);
	if (!blob)
		return {};

	ComPtr<ID3D12RootSignature> rs;
	const HRESULT hr =
		m_device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(rs.put()));
	if (FAILED(hr))
	{
		Console.Error("CreateRootSignature() failed: %08X", hr);
		return {};
	}

	return rs;
}

bool D3D12Context::SupportsTextureFormat(DXGI_FORMAT format)
{
	constexpr u32 required = D3D12_FORMAT_SUPPORT1_TEXTURE2D | D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE;

	D3D12_FEATURE_DATA_FORMAT_SUPPORT support = {format};
	return SUCCEEDED(m_device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support))) &&
		   (support.Support1 & required) == required;
}

bool D3D12Context::Create(IDXGIFactory5* dxgi_factory, IDXGIAdapter1* adapter, bool enable_debug_layer)
{
	pxAssertRel(!g_d3d12_context, "No context exists");

	g_d3d12_context.reset(new D3D12Context());
	if (!g_d3d12_context->CreateDevice(dxgi_factory, adapter, enable_debug_layer) ||
		!g_d3d12_context->CreateCommandQueue() || !g_d3d12_context->CreateAllocator() ||
		!g_d3d12_context->CreateFence() || !g_d3d12_context->CreateDescriptorHeaps() ||
		!g_d3d12_context->CreateCommandLists() || !g_d3d12_context->CreateTimestampQuery() ||
		!g_d3d12_context->CreateTextureStreamBuffer())
	{
		Destroy();
		return false;
	}

	return true;
}

void D3D12Context::Destroy()
{
	if (g_d3d12_context)
		g_d3d12_context.reset();
}

u32 D3D12Context::GetAdapterVendorID() const
{
	if (!m_adapter)
		return 0;

	DXGI_ADAPTER_DESC desc;
	if (FAILED(m_adapter->GetDesc(&desc)))
		return 0;

	return desc.VendorId;
}

bool D3D12Context::CreateDevice(IDXGIFactory5* dxgi_factory, IDXGIAdapter1* adapter, bool enable_debug_layer)
{
	HRESULT hr;

	// Enabling the debug layer will fail if the Graphics Tools feature is not installed.
	if (enable_debug_layer)
	{
		hr = D3D12GetDebugInterface(IID_PPV_ARGS(&m_debug_interface));
		if (SUCCEEDED(hr))
		{
			m_debug_interface->EnableDebugLayer();
		}
		else
		{
			Console.Error("Debug layer requested but not available.");
			enable_debug_layer = false;
		}
	}

	// Create the actual device.
	hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
	if (FAILED(hr))
	{
		Console.Error("Failed to create D3D12 device: %08X", hr);
		return false;
	}

	// get adapter
	const LUID luid(m_device->GetAdapterLuid());
	if (FAILED(dxgi_factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(m_adapter.put()))))
		Console.Error("Failed to get lookup adapter by device LUID");

	if (enable_debug_layer)
	{
		ComPtr<ID3D12InfoQueue> info_queue = m_device.try_query<ID3D12InfoQueue>();
		if (info_queue)
		{
			if (IsDebuggerPresent())
			{
				info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
				info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
			}

			D3D12_INFO_QUEUE_FILTER filter = {};
			std::array<D3D12_MESSAGE_ID, 5> id_list{
				D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
				D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
				D3D12_MESSAGE_ID_CREATEGRAPHICSPIPELINESTATE_RENDERTARGETVIEW_NOT_SET,
				D3D12_MESSAGE_ID_CREATEINPUTLAYOUT_TYPE_MISMATCH,
				D3D12_MESSAGE_ID_DRAW_EMPTY_SCISSOR_RECTANGLE,
			};
			filter.DenyList.NumIDs = static_cast<UINT>(id_list.size());
			filter.DenyList.pIDList = id_list.data();
			info_queue->PushStorageFilter(&filter);
		}
	}

	return true;
}

bool D3D12Context::CreateCommandQueue()
{
	const D3D12_COMMAND_QUEUE_DESC queue_desc = {
		D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_QUEUE_PRIORITY_NORMAL, D3D12_COMMAND_QUEUE_FLAG_NONE};
	HRESULT hr = m_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_command_queue));
	pxAssertRel(SUCCEEDED(hr), "Create command queue");
	return SUCCEEDED(hr);
}

bool D3D12Context::CreateAllocator()
{
	D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
	allocatorDesc.pDevice = m_device.get();
	allocatorDesc.pAdapter = m_adapter.get();
	allocatorDesc.Flags =
		D3D12MA::ALLOCATOR_FLAG_SINGLETHREADED |
		D3D12MA::ALLOCATOR_FLAG_DEFAULT_POOLS_NOT_ZEROED /* | D3D12MA::ALLOCATOR_FLAG_ALWAYS_COMMITTED*/;

	const HRESULT hr = D3D12MA::CreateAllocator(&allocatorDesc, m_allocator.put());
	if (FAILED(hr))
	{
		Console.Error("D3D12MA::CreateAllocator() failed with HRESULT %08X", hr);
		return false;
	}

	return true;
}

bool D3D12Context::CreateFence()
{
	HRESULT hr = m_device->CreateFence(m_completed_fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
	pxAssertRel(SUCCEEDED(hr), "Create fence");
	if (FAILED(hr))
		return false;

	m_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	pxAssertRel(m_fence_event != NULL, "Create fence event");
	if (!m_fence_event)
		return false;

	return true;
}

bool D3D12Context::CreateDescriptorHeaps()
{
	static constexpr size_t MAX_SRVS = 32768;
	static constexpr size_t MAX_RTVS = 16384;
	static constexpr size_t MAX_DSVS = 16384;
	static constexpr size_t MAX_CPU_SAMPLERS = 1024;

	if (!m_descriptor_heap_manager.Create(m_device.get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MAX_SRVS, false) ||
		!m_rtv_heap_manager.Create(m_device.get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, MAX_RTVS, false) ||
		!m_dsv_heap_manager.Create(m_device.get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, MAX_DSVS, false) ||
		!m_sampler_heap_manager.Create(m_device.get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, MAX_CPU_SAMPLERS, false))
	{
		return false;
	}

	// Allocate null SRV descriptor for unbound textures.
	constexpr D3D12_SHADER_RESOURCE_VIEW_DESC null_srv_desc = {
		DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_SRV_DIMENSION_TEXTURE2D, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING};

	if (!m_descriptor_heap_manager.Allocate(&m_null_srv_descriptor))
	{
		pxFailRel("Failed to allocate null descriptor");
		return false;
	}

	m_device->CreateShaderResourceView(nullptr, &null_srv_desc, m_null_srv_descriptor.cpu_handle);
	return true;
}

bool D3D12Context::CreateCommandLists()
{
	static constexpr size_t MAX_GPU_SRVS = 32768;
	static constexpr size_t MAX_GPU_SAMPLERS = 2048;

	for (u32 i = 0; i < NUM_COMMAND_LISTS; i++)
	{
		CommandListResources& res = m_command_lists[i];
		HRESULT hr;

		for (u32 i = 0; i < 2; i++)
		{
			hr = m_device->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(res.command_allocators[i].put()));
			pxAssertRel(SUCCEEDED(hr), "Create command allocator");
			if (FAILED(hr))
				return false;

			hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, res.command_allocators[i].get(),
				nullptr, IID_PPV_ARGS(res.command_lists[i].put()));
			if (FAILED(hr))
			{
				Console.Error("Failed to create command list: %08X", hr);
				return false;
			}

			// Close the command lists, since the first thing we do is reset them.
			hr = res.command_lists[i]->Close();
			pxAssertRel(SUCCEEDED(hr), "Closing new command list failed");
			if (FAILED(hr))
				return false;
		}

		if (!res.descriptor_allocator.Create(m_device.get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MAX_GPU_SRVS))
		{
			Console.Error("Failed to create per frame descriptor allocator");
			return false;
		}

		if (!res.sampler_allocator.Create(m_device.get(), MAX_GPU_SAMPLERS))
		{
			Console.Error("Failed to create per frame sampler allocator");
			return false;
		}
	}

	MoveToNextCommandList();
	return true;
}

bool D3D12Context::CreateTextureStreamBuffer()
{
	return m_texture_stream_buffer.Create(TEXTURE_UPLOAD_BUFFER_SIZE);
}

void D3D12Context::MoveToNextCommandList()
{
	m_current_command_list = (m_current_command_list + 1) % NUM_COMMAND_LISTS;
	m_current_fence_value++;

	// We may have to wait if this command list hasn't finished on the GPU.
	CommandListResources& res = m_command_lists[m_current_command_list];
	WaitForFence(res.ready_fence_value, false);
	res.ready_fence_value = m_current_fence_value;
	res.init_command_list_used = false;

	// Begin command list.
	res.command_allocators[1]->Reset();
	res.command_lists[1]->Reset(res.command_allocators[1].get(), nullptr);
	res.descriptor_allocator.Reset();
	if (res.sampler_allocator.ShouldReset())
		res.sampler_allocator.Reset();

	if (res.has_timestamp_query)
	{
		// readback timestamp from the last time this cmdlist was used.
		// we don't need to worry about disjoint in dx12, the frequency is reliable within a single cmdlist.
		const u32 offset = (m_current_command_list * (sizeof(u64) * NUM_TIMESTAMP_QUERIES_PER_CMDLIST));
		const D3D12_RANGE read_range = {offset, offset + (sizeof(u64) * NUM_TIMESTAMP_QUERIES_PER_CMDLIST)};
		void* map;
		HRESULT hr = m_timestamp_query_buffer->Map(0, &read_range, &map);
		if (SUCCEEDED(hr))
		{
			u64 timestamps[2];
			std::memcpy(timestamps, static_cast<const u8*>(map) + offset, sizeof(timestamps));
			m_accumulated_gpu_time +=
				static_cast<float>(static_cast<double>(timestamps[1] - timestamps[0]) / m_timestamp_frequency);

			const D3D12_RANGE write_range = {};
			m_timestamp_query_buffer->Unmap(0, &write_range);
		}
		else
		{
			Console.Warning("Map() for timestamp query failed: %08X", hr);
		}
	}

	res.has_timestamp_query = m_gpu_timing_enabled;
	if (m_gpu_timing_enabled)
	{
		res.command_lists[1]->EndQuery(m_timestamp_query_heap.get(), D3D12_QUERY_TYPE_TIMESTAMP,
			m_current_command_list * NUM_TIMESTAMP_QUERIES_PER_CMDLIST);
	}

	ID3D12DescriptorHeap* heaps[2] = {
		res.descriptor_allocator.GetDescriptorHeap(), res.sampler_allocator.GetDescriptorHeap()};
	res.command_lists[1]->SetDescriptorHeaps(std::size(heaps), heaps);

	m_allocator->SetCurrentFrameIndex(static_cast<UINT>(m_current_fence_value));
}

ID3D12GraphicsCommandList4* D3D12Context::GetInitCommandList()
{
	CommandListResources& res = m_command_lists[m_current_command_list];
	if (!res.init_command_list_used)
	{
		HRESULT hr = res.command_allocators[0]->Reset();
		pxAssertMsg(SUCCEEDED(hr), "Reset init command allocator failed");

		res.command_lists[0]->Reset(res.command_allocators[0].get(), nullptr);
		pxAssertMsg(SUCCEEDED(hr), "Reset init command list failed");
		res.init_command_list_used = true;
	}

	return res.command_lists[0].get();
}

bool D3D12Context::ExecuteCommandList(WaitType wait_for_completion)
{
	CommandListResources& res = m_command_lists[m_current_command_list];
	HRESULT hr;

	if (res.has_timestamp_query)
	{
		// write the timestamp back at the end of the cmdlist
		res.command_lists[1]->EndQuery(m_timestamp_query_heap.get(), D3D12_QUERY_TYPE_TIMESTAMP,
			(m_current_command_list * NUM_TIMESTAMP_QUERIES_PER_CMDLIST) + 1);
		res.command_lists[1]->ResolveQueryData(m_timestamp_query_heap.get(), D3D12_QUERY_TYPE_TIMESTAMP,
			m_current_command_list * NUM_TIMESTAMP_QUERIES_PER_CMDLIST, NUM_TIMESTAMP_QUERIES_PER_CMDLIST,
			m_timestamp_query_buffer.get(), m_current_command_list * (sizeof(u64) * NUM_TIMESTAMP_QUERIES_PER_CMDLIST));
	}

	if (res.init_command_list_used)
	{
		hr = res.command_lists[0]->Close();
		if (FAILED(hr))
		{
			Console.Error("Closing init command list failed with HRESULT %08X", hr);
			return false;
		}
	}

	// Close and queue command list.
	hr = res.command_lists[1]->Close();
	if (FAILED(hr))
	{
		Console.Error("Closing main command list failed with HRESULT %08X", hr);
		return false;
	}

	if (res.init_command_list_used)
	{
		const std::array<ID3D12CommandList*, 2> execute_lists{res.command_lists[0].get(), res.command_lists[1].get()};
		m_command_queue->ExecuteCommandLists(static_cast<UINT>(execute_lists.size()), execute_lists.data());
	}
	else
	{
		const std::array<ID3D12CommandList*, 1> execute_lists{res.command_lists[1].get()};
		m_command_queue->ExecuteCommandLists(static_cast<UINT>(execute_lists.size()), execute_lists.data());
	}

	// Update fence when GPU has completed.
	hr = m_command_queue->Signal(m_fence.get(), res.ready_fence_value);
	pxAssertRel(SUCCEEDED(hr), "Signal fence");

	MoveToNextCommandList();
	if (wait_for_completion != WaitType::None)
		WaitForFence(res.ready_fence_value, wait_for_completion == WaitType::Spin);

	return true;
}

void D3D12Context::InvalidateSamplerGroups()
{
	for (CommandListResources& res : m_command_lists)
		res.sampler_allocator.InvalidateCache();
}

void D3D12Context::DeferObjectDestruction(ID3D12DeviceChild* resource)
{
	if (!resource)
		return;

	resource->AddRef();
	m_command_lists[m_current_command_list].pending_resources.emplace_back(nullptr, resource);
}

void D3D12Context::DeferResourceDestruction(D3D12MA::Allocation* allocation, ID3D12Resource* resource)
{
	if (!resource)
		return;

	if (allocation)
		allocation->AddRef();

	resource->AddRef();
	m_command_lists[m_current_command_list].pending_resources.emplace_back(allocation, resource);
}

void D3D12Context::DeferDescriptorDestruction(D3D12DescriptorHeapManager& manager, u32 index)
{
	m_command_lists[m_current_command_list].pending_descriptors.emplace_back(manager, index);
}

void D3D12Context::DeferDescriptorDestruction(D3D12DescriptorHeapManager& manager, D3D12DescriptorHandle* handle)
{
	if (handle->index == D3D12DescriptorHandle::INVALID_INDEX)
		return;

	m_command_lists[m_current_command_list].pending_descriptors.emplace_back(manager, handle->index);
	handle->Clear();
}

void D3D12Context::DestroyPendingResources(CommandListResources& cmdlist)
{
	for (const auto& dd : cmdlist.pending_descriptors)
		dd.first.Free(dd.second);
	cmdlist.pending_descriptors.clear();

	for (const auto& it : cmdlist.pending_resources)
	{
		it.second->Release();
		if (it.first)
			it.first->Release();
	}
	cmdlist.pending_resources.clear();
}

void D3D12Context::DestroyResources()
{
	if (m_command_queue)
	{
		ExecuteCommandList(WaitType::Sleep);
		WaitForGPUIdle();
	}

	m_texture_stream_buffer.Destroy(false);
	m_descriptor_heap_manager.Free(&m_null_srv_descriptor);
	m_timestamp_query_buffer.reset();
	m_timestamp_query_allocation.reset();
	m_sampler_heap_manager.Destroy();
	m_dsv_heap_manager.Destroy();
	m_rtv_heap_manager.Destroy();
	m_descriptor_heap_manager.Destroy();
	m_command_lists = {};
	m_current_command_list = 0;
	m_completed_fence_value = 0;
	m_current_fence_value = 0;
	if (m_fence_event)
	{
		CloseHandle(m_fence_event);
		m_fence_event = {};
	}

	m_allocator.reset();
	m_command_queue.reset();
	m_debug_interface.reset();
	m_device.reset();
}

void D3D12Context::WaitForFence(u64 fence, bool spin)
{
	if (m_completed_fence_value >= fence)
		return;

	if (spin)
	{
		u64 value;
		while ((value = m_fence->GetCompletedValue()) < fence)
			ShortSpin();
		m_completed_fence_value = value;
	}
	else
	{
		// Try non-blocking check.
		m_completed_fence_value = m_fence->GetCompletedValue();
		if (m_completed_fence_value < fence)
		{
			// Fall back to event.
			HRESULT hr = m_fence->SetEventOnCompletion(fence, m_fence_event);
			pxAssertRel(SUCCEEDED(hr), "Set fence event on completion");
			WaitForSingleObject(m_fence_event, INFINITE);
			m_completed_fence_value = m_fence->GetCompletedValue();
		}
	}

	// Release resources for as many command lists which have completed.
	u32 index = (m_current_command_list + 1) % NUM_COMMAND_LISTS;
	for (u32 i = 0; i < NUM_COMMAND_LISTS; i++)
	{
		CommandListResources& res = m_command_lists[index];
		if (m_completed_fence_value < res.ready_fence_value)
			break;

		DestroyPendingResources(res);
		index = (index + 1) % NUM_COMMAND_LISTS;
	}
}

void D3D12Context::WaitForGPUIdle()
{
	u32 index = (m_current_command_list + 1) % NUM_COMMAND_LISTS;
	for (u32 i = 0; i < (NUM_COMMAND_LISTS - 1); i++)
	{
		WaitForFence(m_command_lists[index].ready_fence_value, false);
		index = (index + 1) % NUM_COMMAND_LISTS;
	}
}

bool D3D12Context::CreateTimestampQuery()
{
	constexpr u32 QUERY_COUNT = NUM_TIMESTAMP_QUERIES_PER_CMDLIST * NUM_COMMAND_LISTS;
	constexpr u32 BUFFER_SIZE = sizeof(u64) * QUERY_COUNT;

	const D3D12_QUERY_HEAP_DESC desc = {D3D12_QUERY_HEAP_TYPE_TIMESTAMP, QUERY_COUNT};
	HRESULT hr = m_device->CreateQueryHeap(&desc, IID_PPV_ARGS(m_timestamp_query_heap.put()));
	if (FAILED(hr))
	{
		Console.Error("CreateQueryHeap() for timestamp failed with %08X", hr);
		return false;
	}

	const D3D12MA::ALLOCATION_DESC allocation_desc = {D3D12MA::ALLOCATION_FLAG_NONE, D3D12_HEAP_TYPE_READBACK};
	const D3D12_RESOURCE_DESC resource_desc = {D3D12_RESOURCE_DIMENSION_BUFFER, 0, BUFFER_SIZE, 1, 1, 1,
		DXGI_FORMAT_UNKNOWN, {1, 0}, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, D3D12_RESOURCE_FLAG_NONE};
	hr = m_allocator->CreateResource(&allocation_desc, &resource_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
		m_timestamp_query_allocation.put(), IID_PPV_ARGS(m_timestamp_query_buffer.put()));
	if (FAILED(hr))
	{
		Console.Error("CreateResource() for timestamp failed with %08X", hr);
		return false;
	}

	u64 frequency;
	hr = m_command_queue->GetTimestampFrequency(&frequency);
	if (FAILED(hr))
	{
		Console.Error("GetTimestampFrequency() failed: %08X", hr);
		return false;
	}

	m_timestamp_frequency = static_cast<double>(frequency) / 1000.0;
	return true;
}

float D3D12Context::GetAndResetAccumulatedGPUTime()
{
	const float time = m_accumulated_gpu_time;
	m_accumulated_gpu_time = 0.0f;
	return time;
}

void D3D12Context::SetEnableGPUTiming(bool enabled)
{
	m_gpu_timing_enabled = enabled;
}

bool D3D12Context::AllocatePreinitializedGPUBuffer(u32 size, ID3D12Resource** gpu_buffer,
	D3D12MA::Allocation** gpu_allocation, const std::function<void(void*)>& fill_callback)
{
	// Try to place the fixed index buffer in GPU local memory.
	// Use the staging buffer to copy into it.
	const D3D12_RESOURCE_DESC rd = {D3D12_RESOURCE_DIMENSION_BUFFER, 0, size, 1, 1, 1, DXGI_FORMAT_UNKNOWN, {1, 0},
		D3D12_TEXTURE_LAYOUT_ROW_MAJOR, D3D12_RESOURCE_FLAG_NONE};

	const D3D12MA::ALLOCATION_DESC cpu_ad = {D3D12MA::ALLOCATION_FLAG_NONE, D3D12_HEAP_TYPE_UPLOAD};

	ComPtr<ID3D12Resource> cpu_buffer;
	ComPtr<D3D12MA::Allocation> cpu_allocation;
	HRESULT hr = m_allocator->CreateResource(
		&cpu_ad, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, cpu_allocation.put(), IID_PPV_ARGS(cpu_buffer.put()));
	pxAssertMsg(SUCCEEDED(hr), "Allocate CPU buffer");
	if (FAILED(hr))
		return false;

	static constexpr const D3D12_RANGE read_range = {};
	const D3D12_RANGE write_range = {0, size};
	void* mapped;
	hr = cpu_buffer->Map(0, &read_range, &mapped);
	pxAssertMsg(SUCCEEDED(hr), "Map CPU buffer");
	if (FAILED(hr))
		return false;
	fill_callback(mapped);
	cpu_buffer->Unmap(0, &write_range);

	const D3D12MA::ALLOCATION_DESC gpu_ad = {D3D12MA::ALLOCATION_FLAG_COMMITTED, D3D12_HEAP_TYPE_DEFAULT};

	hr = m_allocator->CreateResource(
		&gpu_ad, &rd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, gpu_allocation, IID_PPV_ARGS(gpu_buffer));
	pxAssertMsg(SUCCEEDED(hr), "Allocate GPU buffer");
	if (FAILED(hr))
		return false;

	GetInitCommandList()->CopyBufferRegion(*gpu_buffer, 0, cpu_buffer.get(), 0, size);

	D3D12_RESOURCE_BARRIER rb = {D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, D3D12_RESOURCE_BARRIER_FLAG_NONE};
	rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	rb.Transition.pResource = *gpu_buffer;
	rb.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	rb.Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
	GetInitCommandList()->ResourceBarrier(1, &rb);

	DeferResourceDestruction(cpu_allocation.get(), cpu_buffer.get());
	return true;
}

u32 D3D12::GetTexelSize(DXGI_FORMAT format)
{
	switch (format)
	{
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC7_UNORM:
			return 16;

		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
			return 4;

		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_SNORM:
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		case DXGI_FORMAT_R32_UINT:
		case DXGI_FORMAT_R32_SINT:
			return 4;

		case DXGI_FORMAT_B5G5R5A1_UNORM:
		case DXGI_FORMAT_B5G6R5_UNORM:
		case DXGI_FORMAT_R16_UINT:
		case DXGI_FORMAT_R16_SINT:
			return 2;

		case DXGI_FORMAT_A8_UNORM:
		case DXGI_FORMAT_R8_UNORM:
			return 1;

		default:
			pxFailRel("Unknown format");
			return 1;
	}
}

#ifdef _DEBUG

void D3D12::SetObjectName(ID3D12Object* object, const char* name)
{
	object->SetName(StringUtil::UTF8StringToWideString(name).c_str());
}

void D3D12::SetObjectNameFormatted(ID3D12Object* object, const char* format, ...)
{
	std::va_list ap;
	va_start(ap, format);
	SetObjectName(object, StringUtil::StdStringFromFormatV(format, ap).c_str());
	va_end(ap);
}

#endif
