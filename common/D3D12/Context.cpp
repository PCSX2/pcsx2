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

#include "common/D3D12/Context.h"
#include "common/Assertions.h"
#include "common/ScopedGuard.h"
#include "common/Console.h"
#include "D3D12MemAlloc.h"

#include <algorithm>
#include <array>
#include <dxgi1_4.h>
#include <queue>
#include <vector>

std::unique_ptr<D3D12::Context> g_d3d12_context;

using namespace D3D12;

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)

// Private D3D12 state
static HMODULE s_d3d12_library;
static PFN_D3D12_CREATE_DEVICE s_d3d12_create_device;
static PFN_D3D12_GET_DEBUG_INTERFACE s_d3d12_get_debug_interface;
static PFN_D3D12_SERIALIZE_ROOT_SIGNATURE s_d3d12_serialize_root_signature;

static bool LoadD3D12Library()
{
	if ((s_d3d12_library = LoadLibraryW(L"d3d12.dll")) == nullptr ||
		(s_d3d12_create_device =
				reinterpret_cast<PFN_D3D12_CREATE_DEVICE>(GetProcAddress(s_d3d12_library, "D3D12CreateDevice"))) == nullptr ||
		(s_d3d12_get_debug_interface = reinterpret_cast<PFN_D3D12_GET_DEBUG_INTERFACE>(
			 GetProcAddress(s_d3d12_library, "D3D12GetDebugInterface"))) == nullptr ||
		(s_d3d12_serialize_root_signature = reinterpret_cast<PFN_D3D12_SERIALIZE_ROOT_SIGNATURE>(
			 GetProcAddress(s_d3d12_library, "D3D12SerializeRootSignature"))) == nullptr)
	{
		Console.Error("d3d12.dll could not be loaded.");
		s_d3d12_create_device = nullptr;
		s_d3d12_get_debug_interface = nullptr;
		s_d3d12_serialize_root_signature = nullptr;
		if (s_d3d12_library)
			FreeLibrary(s_d3d12_library);
		s_d3d12_library = nullptr;
		return false;
	}

	return true;
}

static void UnloadD3D12Library()
{
	s_d3d12_serialize_root_signature = nullptr;
	s_d3d12_get_debug_interface = nullptr;
	s_d3d12_create_device = nullptr;
	if (s_d3d12_library)
	{
		FreeLibrary(s_d3d12_library);
		s_d3d12_library = nullptr;
	}
}

#else

static const PFN_D3D12_CREATE_DEVICE s_d3d12_create_device = D3D12CreateDevice;
static const PFN_D3D12_GET_DEBUG_INTERFACE s_d3d12_get_debug_interface = D3D12GetDebugInterface;
static const PFN_D3D12_SERIALIZE_ROOT_SIGNATURE s_d3d12_serialize_root_signature = D3D12SerializeRootSignature;

static bool LoadD3D12Library()
{
	return true;
}

static void UnloadD3D12Library() {}

#endif

Context::Context() = default;

Context::~Context()
{
	DestroyResources();
}

Context::ComPtr<ID3DBlob> Context::SerializeRootSignature(const D3D12_ROOT_SIGNATURE_DESC* desc)
{
	ComPtr<ID3DBlob> blob;
	ComPtr<ID3DBlob> error_blob;
	const HRESULT hr = s_d3d12_serialize_root_signature(desc, D3D_ROOT_SIGNATURE_VERSION_1, blob.put(),
		error_blob.put());
	if (FAILED(hr))
	{
		Console.Error("D3D12SerializeRootSignature() failed: %08X", hr);
		if (error_blob)
			Console.Error("%s", error_blob->GetBufferPointer());

		return {};
	}

	return blob;
}

D3D12::Context::ComPtr<ID3D12RootSignature> Context::CreateRootSignature(const D3D12_ROOT_SIGNATURE_DESC* desc)
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

bool Context::SupportsTextureFormat(DXGI_FORMAT format)
{
	constexpr u32 required = D3D12_FORMAT_SUPPORT1_TEXTURE2D | D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE;

	D3D12_FEATURE_DATA_FORMAT_SUPPORT support = {format};
	return SUCCEEDED(m_device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support))) &&
		   (support.Support1 & required) == required;
}

bool Context::Create(IDXGIFactory* dxgi_factory, u32 adapter_index, bool enable_debug_layer)
{
	pxAssertRel(!g_d3d12_context, "No context exists");

	if (!LoadD3D12Library())
		return false;

	g_d3d12_context.reset(new Context());
	if (!g_d3d12_context->CreateDevice(dxgi_factory, adapter_index, enable_debug_layer) ||
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

void Context::Destroy()
{
	if (g_d3d12_context)
		g_d3d12_context.reset();

	UnloadD3D12Library();
}

u32 Context::GetAdapterVendorID() const
{
	if (!m_adapter)
		return 0;

	DXGI_ADAPTER_DESC desc;
	if (FAILED(m_adapter->GetDesc(&desc)))
		return 0;

	return desc.VendorId;
}

bool Context::CreateDevice(IDXGIFactory* dxgi_factory, u32 adapter_index, bool enable_debug_layer)
{
	ComPtr<IDXGIAdapter> adapter;
	HRESULT hr = dxgi_factory->EnumAdapters(adapter_index, &adapter);
	if (FAILED(hr))
	{
		Console.Error("Adapter %u not found, using default", adapter_index);
		adapter = nullptr;
	}
	else
	{
		DXGI_ADAPTER_DESC adapter_desc;
		if (SUCCEEDED(adapter->GetDesc(&adapter_desc)))
		{
			char adapter_name_buffer[128];
			const int name_length = WideCharToMultiByte(CP_UTF8, 0, adapter_desc.Description,
				static_cast<int>(std::wcslen(adapter_desc.Description)),
				adapter_name_buffer, std::size(adapter_name_buffer), 0, nullptr);
			if (name_length >= 0)
			{
				adapter_name_buffer[name_length] = 0;
				Console.WriteLn("D3D Adapter: %s", adapter_name_buffer);
			}
		}
	}

	// Enabling the debug layer will fail if the Graphics Tools feature is not installed.
	if (enable_debug_layer)
	{
		hr = s_d3d12_get_debug_interface(IID_PPV_ARGS(&m_debug_interface));
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
	hr = s_d3d12_create_device(adapter.get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
	pxAssertRel(SUCCEEDED(hr), "Create D3D12 device");
	if (FAILED(hr))
		return false;

	// get adapter
	ComPtr<IDXGIFactory4> dxgi_factory4;
	if (SUCCEEDED(dxgi_factory->QueryInterface<IDXGIFactory4>(dxgi_factory4.put())))
	{
		const LUID luid(m_device->GetAdapterLuid());
		if (FAILED(dxgi_factory4->EnumAdapterByLuid(luid, IID_PPV_ARGS(m_adapter.put()))))
			Console.Error("Failed to get lookup adapter by device LUID");
	}

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

bool Context::CreateCommandQueue()
{
	const D3D12_COMMAND_QUEUE_DESC queue_desc = {D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
		D3D12_COMMAND_QUEUE_FLAG_NONE};
	HRESULT hr = m_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_command_queue));
	pxAssertRel(SUCCEEDED(hr), "Create command queue");
	return SUCCEEDED(hr);
}

bool Context::CreateAllocator()
{
	D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
	allocatorDesc.pDevice = m_device.get();
	allocatorDesc.pAdapter = m_adapter.get();
	allocatorDesc.Flags = D3D12MA::ALLOCATOR_FLAG_SINGLETHREADED | D3D12MA::ALLOCATOR_FLAG_DEFAULT_POOLS_NOT_ZEROED /* | D3D12MA::ALLOCATOR_FLAG_ALWAYS_COMMITTED*/;

	const HRESULT hr = D3D12MA::CreateAllocator(&allocatorDesc, m_allocator.put());
	if (FAILED(hr))
	{
		Console.Error("D3D12MA::CreateAllocator() failed with HRESULT %08X", hr);
		return false;
	}

	return true;
}

bool Context::CreateFence()
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

bool Context::CreateDescriptorHeaps()
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
	constexpr D3D12_SHADER_RESOURCE_VIEW_DESC null_srv_desc = {DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_SRV_DIMENSION_TEXTURE2D,
		D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING};

	if (!m_descriptor_heap_manager.Allocate(&m_null_srv_descriptor))
	{
		pxFailRel("Failed to allocate null descriptor");
		return false;
	}

	m_device->CreateShaderResourceView(nullptr, &null_srv_desc, m_null_srv_descriptor.cpu_handle);
	return true;
}

bool Context::CreateCommandLists()
{
	static constexpr size_t MAX_GPU_SRVS = 32768;
	static constexpr size_t MAX_GPU_SAMPLERS = 2048;

	for (u32 i = 0; i < NUM_COMMAND_LISTS; i++)
	{
		CommandListResources& res = m_command_lists[i];
		HRESULT hr;

		for (u32 i = 0; i < 2; i++)
		{
			hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
				IID_PPV_ARGS(res.command_allocators[i].put()));
			pxAssertRel(SUCCEEDED(hr), "Create command allocator");
			if (FAILED(hr))
				return false;

			hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
				res.command_allocators[i].get(), nullptr,
				IID_PPV_ARGS(res.command_lists[i].put()));
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

bool Context::CreateTextureStreamBuffer()
{
	return m_texture_stream_buffer.Create(TEXTURE_UPLOAD_BUFFER_SIZE);
}

void Context::MoveToNextCommandList()
{
	m_current_command_list = (m_current_command_list + 1) % NUM_COMMAND_LISTS;
	m_current_fence_value++;

	// We may have to wait if this command list hasn't finished on the GPU.
	CommandListResources& res = m_command_lists[m_current_command_list];
	WaitForFence(res.ready_fence_value);
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
			m_accumulated_gpu_time += static_cast<float>(static_cast<double>(timestamps[1] - timestamps[0]) / m_timestamp_frequency);

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

	ID3D12DescriptorHeap* heaps[2] = {res.descriptor_allocator.GetDescriptorHeap(), res.sampler_allocator.GetDescriptorHeap()};
	res.command_lists[1]->SetDescriptorHeaps(std::size(heaps), heaps);

	m_allocator->SetCurrentFrameIndex(static_cast<UINT>(m_current_fence_value));
}

ID3D12GraphicsCommandList4* Context::GetInitCommandList()
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

void Context::ExecuteCommandList(bool wait_for_completion)
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
		pxAssertRel(SUCCEEDED(hr), "Close init command list");
	}

	// Close and queue command list.
	hr = res.command_lists[1]->Close();
	pxAssertRel(SUCCEEDED(hr), "Close command list");
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
	hr = m_command_queue->Signal(m_fence.get(), m_current_fence_value);
	pxAssertRel(SUCCEEDED(hr), "Signal fence");

	MoveToNextCommandList();
	if (wait_for_completion)
		WaitForFence(res.ready_fence_value);
}

void Context::InvalidateSamplerGroups()
{
	for (CommandListResources& res : m_command_lists)
		res.sampler_allocator.InvalidateCache();
}

void Context::DeferObjectDestruction(ID3D12DeviceChild* resource)
{
	if (!resource)
		return;

	resource->AddRef();
	m_command_lists[m_current_command_list].pending_resources.emplace_back(nullptr, resource);
}

void Context::DeferResourceDestruction(D3D12MA::Allocation* allocation, ID3D12Resource* resource)
{
	if (!resource)
		return;

	if (allocation)
		allocation->AddRef();

	resource->AddRef();
	m_command_lists[m_current_command_list].pending_resources.emplace_back(allocation, resource);
}

void Context::DeferDescriptorDestruction(DescriptorHeapManager& manager, u32 index)
{
	m_command_lists[m_current_command_list].pending_descriptors.emplace_back(manager, index);
}

void Context::DeferDescriptorDestruction(DescriptorHeapManager& manager, DescriptorHandle* handle)
{
	if (handle->index == DescriptorHandle::INVALID_INDEX)
		return;

	m_command_lists[m_current_command_list].pending_descriptors.emplace_back(manager, handle->index);
	handle->Clear();
}

void Context::DestroyPendingResources(CommandListResources& cmdlist)
{
	for (const auto& dd : cmdlist.pending_descriptors)
		dd.first.Free(dd.second);
	cmdlist.pending_descriptors.clear();

	for (const auto& it : cmdlist.pending_resources)
	{
		if (it.first)
			it.first->Release();
		it.second->Release();
	}
	cmdlist.pending_resources.clear();
}

void Context::DestroyResources()
{
	ExecuteCommandList(true);

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

void Context::WaitForFence(u64 fence)
{
	if (m_completed_fence_value >= fence)
		return;

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

void Context::WaitForGPUIdle()
{
	u32 index = (m_current_command_list + 1) % NUM_COMMAND_LISTS;
	for (u32 i = 0; i < (NUM_COMMAND_LISTS - 1); i++)
	{
		WaitForFence(m_command_lists[index].ready_fence_value);
		index = (index + 1) % NUM_COMMAND_LISTS;
	}
}

bool Context::CreateTimestampQuery()
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
	const D3D12_RESOURCE_DESC resource_desc = {
		D3D12_RESOURCE_DIMENSION_BUFFER, 0, BUFFER_SIZE, 1, 1, 1, DXGI_FORMAT_UNKNOWN, {1, 0}, D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		D3D12_RESOURCE_FLAG_NONE};
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

float Context::GetAndResetAccumulatedGPUTime()
{
	const float time = m_accumulated_gpu_time;
	m_accumulated_gpu_time = 0.0f;
	return time;
}

void Context::SetEnableGPUTiming(bool enabled)
{
	m_gpu_timing_enabled = enabled;
}
