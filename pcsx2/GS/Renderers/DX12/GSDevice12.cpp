// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/GS.h"
#include "GS/GSGL.h"
#include "GS/GSPerfMon.h"
#include "GS/GSUtil.h"
#include "GS/Renderers/DX11/D3D.h"
#include "GS/Renderers/DX12/GSDevice12.h"
#include "GS/Renderers/DX12/D3D12Builders.h"
#include "GS/Renderers/DX12/D3D12ShaderCache.h"
#include "Host.h"
#include "ShaderCacheVersion.h"

#include "common/Console.h"
#include "common/BitUtils.h"
#include "common/Error.h"
#include "common/HostSys.h"
#include "common/ScopedGuard.h"
#include "common/SmallString.h"
#include "common/StringUtil.h"

#include "D3D12MemAlloc.h"
#include "imgui.h"

#include <sstream>
#include <limits>

#ifdef ENABLE_OGL_DEBUG
#define USE_PIX
#include "WinPixEventRuntime/pix3.h"

static u32 s_debug_scope_depth = 0;
#endif

static bool IsDATMConvertShader(ShaderConvert i)
{
	return (i == ShaderConvert::DATM_0 || i == ShaderConvert::DATM_1 || i == ShaderConvert::DATM_0_RTA_CORRECTION || i == ShaderConvert::DATM_1_RTA_CORRECTION);
}
static bool IsDATEModePrimIDInit(u32 flag)
{
	return flag == 1 || flag == 2;
}

static constexpr std::array<D3D12_PRIMITIVE_TOPOLOGY, 3> s_primitive_topology_mapping = {
	{D3D_PRIMITIVE_TOPOLOGY_POINTLIST, D3D_PRIMITIVE_TOPOLOGY_LINELIST, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST}};

static constexpr std::array<float, 4> s_present_clear_color = {};

static D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE GetLoadOpForTexture(GSTexture12* tex)
{
	if (!tex)
		return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;

	// clang-format off
	switch (tex->GetState())
	{
	case GSTexture12::State::Cleared:       tex->SetState(GSTexture::State::Dirty); return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
	case GSTexture12::State::Invalidated:   tex->SetState(GSTexture::State::Dirty); return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
	case GSTexture12::State::Dirty:         return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
	default:                                return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
	}
	// clang-format on
}

GSDevice12::ShaderMacro::ShaderMacro()
{
	mlist.emplace_back("DX12", "1");
}

void GSDevice12::ShaderMacro::AddMacro(const char* n, int d)
{
	AddMacro(n, std::to_string(d));
}

void GSDevice12::ShaderMacro::AddMacro(const char* n, std::string d)
{
	mlist.emplace_back(n, std::move(d));
}

D3D_SHADER_MACRO* GSDevice12::ShaderMacro::GetPtr(void)
{
	mout.clear();

	for (auto& i : mlist)
		mout.emplace_back(i.name.c_str(), i.def.c_str());

	mout.emplace_back(nullptr, nullptr);
	return (D3D_SHADER_MACRO*)mout.data();
}

GSDevice12::GSDevice12() = default;

GSDevice12::~GSDevice12() = default;

GSDevice12::ComPtr<ID3DBlob> GSDevice12::SerializeRootSignature(const D3D12_ROOT_SIGNATURE_DESC* desc)
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

GSDevice12::ComPtr<ID3D12RootSignature> GSDevice12::CreateRootSignature(const D3D12_ROOT_SIGNATURE_DESC* desc)
{
	ComPtr<ID3DBlob> blob = SerializeRootSignature(desc);
	if (!blob)
		return {};

	ComPtr<ID3D12RootSignature> rs;
	const HRESULT hr =
		m_device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(rs.put()));
	if (FAILED(hr))
	{
		Console.Error("D3D12: CreateRootSignature() failed: %08X", hr);
		return {};
	}

	return rs;
}

bool GSDevice12::SupportsTextureFormat(DXGI_FORMAT format)
{
	constexpr u32 required = D3D12_FORMAT_SUPPORT1_TEXTURE2D | D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE;

	D3D12_FEATURE_DATA_FORMAT_SUPPORT support = {format};
	return SUCCEEDED(m_device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support))) &&
	       (support.Support1 & required) == required;
}

u32 GSDevice12::GetAdapterVendorID() const
{
	if (!m_adapter)
		return 0;

	DXGI_ADAPTER_DESC desc;
	if (FAILED(m_adapter->GetDesc(&desc)))
		return 0;

	return desc.VendorId;
}

bool GSDevice12::CreateDevice(u32& vendor_id)
{
	bool enable_debug_layer = GSConfig.UseDebugDevice;

	m_dxgi_factory = D3D::CreateFactory(GSConfig.UseDebugDevice);
	if (!m_dxgi_factory)
		return false;

	m_adapter = D3D::GetAdapterByName(m_dxgi_factory.get(), GSConfig.Adapter);
	vendor_id = GetAdapterVendorID();

	HRESULT hr;

	// Enabling the debug layer will fail if the Graphics Tools feature is not installed.
	if (enable_debug_layer)
	{
		ComPtr<ID3D12Debug> debug12;
		hr = D3D12GetDebugInterface(IID_PPV_ARGS(debug12.put()));
		if (SUCCEEDED(hr))
		{
			debug12->EnableDebugLayer();
		}
		else
		{
			Console.Error("D3D12: Debug layer requested but not available.");
			enable_debug_layer = false;
		}
	}

	// Intel Haswell doesn't actually support DX12 even tho the device is created which results in a crash,
	// to get around this check if device can be created using feature level 12 (skylake+).
	const bool isIntel = (vendor_id == 0x163C || vendor_id == 0x8086 || vendor_id == 0x8087);
	// Create the actual device.
	hr = D3D12CreateDevice(m_adapter.get(), isIntel ? D3D_FEATURE_LEVEL_12_0 : D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
	if (FAILED(hr))
	{
		Console.Error("D3D12: Failed to create device: %08X", hr);
		return false;
	}

	if (!m_adapter)
	{
		const LUID luid(m_device->GetAdapterLuid());
		if (FAILED(m_dxgi_factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(m_adapter.put()))))
			Console.Error("D3D12: Failed to get lookup adapter by device LUID");
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

	const D3D12_COMMAND_QUEUE_DESC queue_desc = {
		D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_QUEUE_PRIORITY_NORMAL, D3D12_COMMAND_QUEUE_FLAG_NONE};
	hr = m_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&m_command_queue));
	if (FAILED(hr))
	{
		Console.Error("D3D12: Failed to create command queue: %08X", hr);
		return false;
	}

	D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
	allocatorDesc.pDevice = m_device.get();
	allocatorDesc.pAdapter = m_adapter.get();
	allocatorDesc.Flags =
		D3D12MA::ALLOCATOR_FLAG_SINGLETHREADED |
		D3D12MA::ALLOCATOR_FLAG_DEFAULT_POOLS_NOT_ZEROED /* | D3D12MA::ALLOCATOR_FLAG_ALWAYS_COMMITTED*/;

	hr = D3D12MA::CreateAllocator(&allocatorDesc, m_allocator.put());
	if (FAILED(hr))
	{
		Console.Error("D3D12: CreateAllocator() failed with HRESULT %08X", hr);
		return false;
	}

	hr = m_device->CreateFence(m_completed_fence_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
	if (FAILED(hr))
	{
		Console.Error("D3D12: Failed to create fence: %08X", hr);
		return false;
	}

	m_fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (m_fence_event == NULL)
	{
		Console.Error("D3D12: Failed to create fence event: %08X", GetLastError());
		return false;
	}

	return true;
}

bool GSDevice12::CreateDescriptorHeaps()
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

bool GSDevice12::CreateCommandLists()
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
				Console.Error("D3D12: Failed to create command list: %08X", hr);
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
			Console.Error("D3D12: Failed to create per frame descriptor allocator");
			return false;
		}

		if (!res.sampler_allocator.Create(m_device.get(), MAX_GPU_SAMPLERS))
		{
			Console.Error("D3D12: Failed to create per frame sampler allocator");
			return false;
		}
	}

	MoveToNextCommandList();
	return true;
}

void GSDevice12::MoveToNextCommandList()
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
			Console.Warning("D3D12: Map() for timestamp query failed: %08X", hr);
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

ID3D12GraphicsCommandList4* GSDevice12::GetInitCommandList()
{
	CommandListResources& res = m_command_lists[m_current_command_list];
	if (!res.init_command_list_used)
	{
		[[maybe_unused]] HRESULT hr = res.command_allocators[0]->Reset();
		pxAssertMsg(SUCCEEDED(hr), "Reset init command allocator failed");

		res.command_lists[0]->Reset(res.command_allocators[0].get(), nullptr);
		pxAssertMsg(SUCCEEDED(hr), "Reset init command list failed");
		res.init_command_list_used = true;
	}

	return res.command_lists[0].get();
}

bool GSDevice12::ExecuteCommandList(WaitType wait_for_completion)
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
			Console.Error("D3D12: Closing init command list failed with HRESULT %08X", hr);
			return false;
		}
	}

	// Close and queue command list.
	hr = res.command_lists[1]->Close();
	if (FAILED(hr))
	{
		Console.Error("D3D12: Closing main command list failed with HRESULT %08X", hr);
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

void GSDevice12::InvalidateSamplerGroups()
{
	for (CommandListResources& res : m_command_lists)
		res.sampler_allocator.InvalidateCache();
}

void GSDevice12::DeferObjectDestruction(ID3D12DeviceChild* resource)
{
	if (!resource)
		return;

	resource->AddRef();
	m_command_lists[m_current_command_list].pending_resources.emplace_back(nullptr, resource);
}

void GSDevice12::DeferResourceDestruction(D3D12MA::Allocation* allocation, ID3D12Resource* resource)
{
	if (!resource)
		return;

	if (allocation)
		allocation->AddRef();

	resource->AddRef();
	m_command_lists[m_current_command_list].pending_resources.emplace_back(allocation, resource);
}

void GSDevice12::DeferDescriptorDestruction(D3D12DescriptorHeapManager& manager, u32 index)
{
	m_command_lists[m_current_command_list].pending_descriptors.emplace_back(manager, index);
}

void GSDevice12::DeferDescriptorDestruction(D3D12DescriptorHeapManager& manager, D3D12DescriptorHandle* handle)
{
	if (handle->index == D3D12DescriptorHandle::INVALID_INDEX)
		return;

	m_command_lists[m_current_command_list].pending_descriptors.emplace_back(manager, handle->index);
	handle->Clear();
}

void GSDevice12::DestroyPendingResources(CommandListResources& cmdlist)
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

void GSDevice12::WaitForFence(u64 fence, bool spin)
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

void GSDevice12::WaitForGPUIdle()
{
	u32 index = (m_current_command_list + 1) % NUM_COMMAND_LISTS;
	for (u32 i = 0; i < (NUM_COMMAND_LISTS - 1); i++)
	{
		WaitForFence(m_command_lists[index].ready_fence_value, false);
		index = (index + 1) % NUM_COMMAND_LISTS;
	}
}

bool GSDevice12::CreateTimestampQuery()
{
	constexpr u32 QUERY_COUNT = NUM_TIMESTAMP_QUERIES_PER_CMDLIST * NUM_COMMAND_LISTS;
	constexpr u32 BUFFER_SIZE = sizeof(u64) * QUERY_COUNT;

	const D3D12_QUERY_HEAP_DESC desc = {D3D12_QUERY_HEAP_TYPE_TIMESTAMP, QUERY_COUNT};
	HRESULT hr = m_device->CreateQueryHeap(&desc, IID_PPV_ARGS(m_timestamp_query_heap.put()));
	if (FAILED(hr))
	{
		Console.Error("D3D12: CreateQueryHeap() for timestamp failed with %08X", hr);
		return false;
	}

	const D3D12MA::ALLOCATION_DESC allocation_desc = {D3D12MA::ALLOCATION_FLAG_NONE, D3D12_HEAP_TYPE_READBACK};
	const D3D12_RESOURCE_DESC resource_desc = {D3D12_RESOURCE_DIMENSION_BUFFER, 0, BUFFER_SIZE, 1, 1, 1,
		DXGI_FORMAT_UNKNOWN, {1, 0}, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, D3D12_RESOURCE_FLAG_NONE};
	hr = m_allocator->CreateResource(&allocation_desc, &resource_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
		m_timestamp_query_allocation.put(), IID_PPV_ARGS(m_timestamp_query_buffer.put()));
	if (FAILED(hr))
	{
		Console.Error("D3D12: CreateResource() for timestamp failed with %08X", hr);
		return false;
	}

	u64 frequency;
	hr = m_command_queue->GetTimestampFrequency(&frequency);
	if (FAILED(hr))
	{
		Console.Error("D3D12: GetTimestampFrequency() failed: %08X", hr);
		return false;
	}

	m_timestamp_frequency = static_cast<double>(frequency) / 1000.0;
	return true;
}

float GSDevice12::GetAndResetAccumulatedGPUTime()
{
	const float time = m_accumulated_gpu_time;
	m_accumulated_gpu_time = 0.0f;
	return time;
}

bool GSDevice12::SetGPUTimingEnabled(bool enabled)
{
	m_gpu_timing_enabled = enabled;
	return true;
}

bool GSDevice12::AllocatePreinitializedGPUBuffer(u32 size, ID3D12Resource** gpu_buffer,
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
		&gpu_ad, &rd, D3D12_RESOURCE_STATE_COMMON, nullptr, gpu_allocation, IID_PPV_ARGS(gpu_buffer));
	pxAssertMsg(SUCCEEDED(hr), "Allocate GPU buffer");
	if (FAILED(hr))
		return false;

	GetInitCommandList()->CopyBufferRegion(*gpu_buffer, 0, cpu_buffer.get(), 0, size);

	D3D12_RESOURCE_BARRIER rb = {D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, D3D12_RESOURCE_BARRIER_FLAG_NONE};
	rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	rb.Transition.pResource = *gpu_buffer;
	rb.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST; // COMMON -> COPY_DEST at first use.
	rb.Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
	GetInitCommandList()->ResourceBarrier(1, &rb);

	DeferResourceDestruction(cpu_allocation.get(), cpu_buffer.get());
	return true;
}

RenderAPI GSDevice12::GetRenderAPI() const
{
	return RenderAPI::D3D12;
}

bool GSDevice12::HasSurface() const
{
	return static_cast<bool>(m_swap_chain);
}

bool GSDevice12::Create(GSVSyncMode vsync_mode, bool allow_present_throttle)
{
	if (!GSDevice::Create(vsync_mode, allow_present_throttle))
		return false;

	u32 vendor_id = 0;
	if (!CreateDevice(vendor_id))
		return false;

	if (!CheckFeatures(vendor_id))
	{
		Console.Error("D3D12: Your GPU does not support the required D3D12 features.");
		return false;
	}

	m_name = D3D::GetAdapterName(m_adapter.get());

	if (!CreateDescriptorHeaps() || !CreateCommandLists() || !CreateTimestampQuery())
		return false;

	if (!AcquireWindow(true) || (m_window_info.type != WindowInfo::Type::Surfaceless && !CreateSwapChain()))
		return false;

	if (!CreateNullTexture())
	{
		Host::ReportErrorAsync("GS", "Failed to create dummy texture");
		return false;
	}

	{
		std::optional<std::string> shader = ReadShaderSource("shaders/dx11/tfx.fx");
		if (!shader.has_value())
		{
			Host::ReportErrorAsync("GS", "Failed to read shaders/dx11/tfx.fx.");
			return false;
		}

		m_tfx_source = std::move(*shader);
	}

	if (!m_shader_cache.Open(m_feature_level, GSConfig.UseDebugDevice))
		Console.Warning("D3D12: Shader cache failed to open.");

	if (!CreateRootSignatures())
	{
		Host::ReportErrorAsync("GS", "Failed to create pipeline layouts");
		return false;
	}

	if (!CreateBuffers())
		return false;

	if (!CompileConvertPipelines() || !CompilePresentPipelines() || !CompileInterlacePipelines() ||
		!CompileMergePipelines() || !CompilePostProcessingPipelines())
	{
		Host::ReportErrorAsync("GS", "Failed to compile utility pipelines");
		return false;
	}

	if (!CompileCASPipelines())
		return false;

	if (!CompileImGuiPipeline())
		return false;

	InitializeState();
	InitializeSamplers();
	return true;
}

void GSDevice12::Destroy()
{
	GSDevice::Destroy();

	if (GetCommandList())
	{
		EndRenderPass();
		ExecuteCommandList(true);
	}

	DestroySwapChain();
	DestroyResources();
}

void GSDevice12::SetVSyncMode(GSVSyncMode mode, bool allow_present_throttle)
{
	m_allow_present_throttle = allow_present_throttle;

	// Using mailbox-style no-allow-tearing causes tearing in exclusive fullscreen.
	if (mode == GSVSyncMode::Mailbox && m_is_exclusive_fullscreen)
	{
		WARNING_LOG("D3D12: Using FIFO instead of Mailbox vsync due to exclusive fullscreen.");
		mode = GSVSyncMode::FIFO;
	}

	if (m_vsync_mode == mode)
		return;

	const u32 old_buffer_count = GetSwapChainBufferCount();
	m_vsync_mode = mode;
	if (!m_swap_chain)
		return;

	if (GetSwapChainBufferCount() != old_buffer_count)
	{
		DestroySwapChain();
		if (!CreateSwapChain())
			pxFailRel("Failed to recreate swap chain after vsync change.");
	}
}

u32 GSDevice12::GetSwapChainBufferCount() const
{
	// With vsync off, we only need two buffers. Same for blocking vsync.
	// With triple buffering, we need three.
	return (m_vsync_mode == GSVSyncMode::Mailbox) ? 3 : 2;
}

bool GSDevice12::CreateSwapChain()
{
	constexpr DXGI_FORMAT swap_chain_format = DXGI_FORMAT_R8G8B8A8_UNORM;

	if (m_window_info.type != WindowInfo::Type::Win32)
		return false;

	const HWND window_hwnd = reinterpret_cast<HWND>(m_window_info.window_handle);
	RECT client_rc{};
	GetClientRect(window_hwnd, &client_rc);

	DXGI_MODE_DESC fullscreen_mode;
	wil::com_ptr_nothrow<IDXGIOutput> fullscreen_output;
	if (Host::IsFullscreen())
	{
		u32 fullscreen_width, fullscreen_height;
		float fullscreen_refresh_rate;
		m_is_exclusive_fullscreen =
			GetRequestedExclusiveFullscreenMode(&fullscreen_width, &fullscreen_height, &fullscreen_refresh_rate) &&
			D3D::GetRequestedExclusiveFullscreenModeDesc(m_dxgi_factory.get(), client_rc, fullscreen_width,
				fullscreen_height, fullscreen_refresh_rate, swap_chain_format, &fullscreen_mode,
				fullscreen_output.put());

		// Using mailbox-style no-allow-tearing causes tearing in exclusive fullscreen.
		if (m_vsync_mode == GSVSyncMode::Mailbox && m_is_exclusive_fullscreen)
		{
			WARNING_LOG("D3D12: Using FIFO instead of Mailbox vsync due to exclusive fullscreen.");
			m_vsync_mode = GSVSyncMode::FIFO;
		}
	}
	else
	{
		m_is_exclusive_fullscreen = false;
	}

	DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
	swap_chain_desc.Width = static_cast<u32>(client_rc.right - client_rc.left);
	swap_chain_desc.Height = static_cast<u32>(client_rc.bottom - client_rc.top);
	swap_chain_desc.Format = swap_chain_format;
	swap_chain_desc.SampleDesc.Count = 1;
	swap_chain_desc.BufferCount = GetSwapChainBufferCount();
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	m_using_allow_tearing = (m_allow_tearing_supported && !m_is_exclusive_fullscreen);
	if (m_using_allow_tearing)
		swap_chain_desc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

	HRESULT hr = S_OK;

	if (m_is_exclusive_fullscreen)
	{
		DXGI_SWAP_CHAIN_DESC1 fs_sd_desc = swap_chain_desc;
		DXGI_SWAP_CHAIN_FULLSCREEN_DESC fs_desc = {};

		fs_sd_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		fs_sd_desc.Width = fullscreen_mode.Width;
		fs_sd_desc.Height = fullscreen_mode.Height;
		fs_desc.RefreshRate = fullscreen_mode.RefreshRate;
		fs_desc.ScanlineOrdering = fullscreen_mode.ScanlineOrdering;
		fs_desc.Scaling = fullscreen_mode.Scaling;
		fs_desc.Windowed = FALSE;

		Console.WriteLn("D3D12: Creating a %dx%d exclusive fullscreen swap chain", fs_sd_desc.Width, fs_sd_desc.Height);
		hr = m_dxgi_factory->CreateSwapChainForHwnd(m_command_queue.get(), window_hwnd, &fs_sd_desc,
			&fs_desc, fullscreen_output.get(), m_swap_chain.put());
		if (FAILED(hr))
		{
			Console.Warning("D3D12: Failed to create fullscreen swap chain, trying windowed.");
			m_is_exclusive_fullscreen = false;
			m_using_allow_tearing = m_allow_tearing_supported;
		}
	}

	if (!m_is_exclusive_fullscreen)
	{
		Console.WriteLn("D3D12: Creating a %dx%d windowed swap chain", swap_chain_desc.Width, swap_chain_desc.Height);
		hr = m_dxgi_factory->CreateSwapChainForHwnd(
			m_command_queue.get(), window_hwnd, &swap_chain_desc, nullptr, nullptr, m_swap_chain.put());

		if (FAILED(hr))
			Console.Warning("D3D12: Failed to create windowed swap chain.");
	}

	// MWA needs to be called on the correct factory.
	wil::com_ptr_nothrow<IDXGIFactory> swap_chain_factory;
	hr = m_swap_chain->GetParent(IID_PPV_ARGS(swap_chain_factory.put()));
	if (SUCCEEDED(hr))
	{
		hr = swap_chain_factory->MakeWindowAssociation(window_hwnd, DXGI_MWA_NO_WINDOW_CHANGES);
		if (FAILED(hr))
			Console.ErrorFmt("D3D12: MakeWindowAssociation() to disable ALT+ENTER failed: {}", Error::CreateHResult(hr).GetDescription());
	}
	else
	{
		Console.ErrorFmt("D3D12: GetParent() on swap chain to get factory failed: {}", Error::CreateHResult(hr).GetDescription());
	}

	if (!CreateSwapChainRTV())
	{
		DestroySwapChain();
		return false;
	}

	// Render a frame as soon as possible to clear out whatever was previously being displayed.
	EndRenderPass();
	GSTexture12* swap_chain_buf = m_swap_chain_buffers[m_current_swap_chain_buffer].get();
	ID3D12GraphicsCommandList4* cmdlist = GetCommandList();
	m_current_swap_chain_buffer = ((m_current_swap_chain_buffer + 1) % static_cast<u32>(m_swap_chain_buffers.size()));
	swap_chain_buf->TransitionToState(cmdlist, D3D12_RESOURCE_STATE_RENDER_TARGET);
	cmdlist->ClearRenderTargetView(swap_chain_buf->GetWriteDescriptor(), s_present_clear_color.data(), 0, nullptr);
	swap_chain_buf->TransitionToState(cmdlist, D3D12_RESOURCE_STATE_PRESENT);
	ExecuteCommandList(false);
	m_swap_chain->Present(0, m_using_allow_tearing ? DXGI_PRESENT_ALLOW_TEARING : 0);
	return true;
}

bool GSDevice12::CreateSwapChainRTV()
{
	DXGI_SWAP_CHAIN_DESC swap_chain_desc;
	HRESULT hr = m_swap_chain->GetDesc(&swap_chain_desc);
	if (FAILED(hr))
		return false;

	for (u32 i = 0; i < swap_chain_desc.BufferCount; i++)
	{
		ComPtr<ID3D12Resource> backbuffer;
		hr = m_swap_chain->GetBuffer(i, IID_PPV_ARGS(backbuffer.put()));
		if (FAILED(hr))
		{
			Console.Error("D3D12: GetBuffer for RTV failed: 0x%08X", hr);
			m_swap_chain_buffers.clear();
			return false;
		}

		std::unique_ptr<GSTexture12> tex = GSTexture12::Adopt(std::move(backbuffer), GSTexture::Type::RenderTarget,
			GSTexture::Format::Color, swap_chain_desc.BufferDesc.Width, swap_chain_desc.BufferDesc.Height, 1,
			swap_chain_desc.BufferDesc.Format, DXGI_FORMAT_UNKNOWN, swap_chain_desc.BufferDesc.Format,
			DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, D3D12_RESOURCE_STATE_COMMON);
		if (!tex)
		{
			m_swap_chain_buffers.clear();
			return false;
		}

		m_swap_chain_buffers.push_back(std::move(tex));
	}

	m_window_info.surface_width = swap_chain_desc.BufferDesc.Width;
	m_window_info.surface_height = swap_chain_desc.BufferDesc.Height;
	DevCon.WriteLn("D3D12: Swap chain buffer size: %ux%u", m_window_info.surface_width, m_window_info.surface_height);

	if (m_window_info.type == WindowInfo::Type::Win32)
	{
		BOOL fullscreen = FALSE;
		DXGI_SWAP_CHAIN_DESC desc;
		if (SUCCEEDED(m_swap_chain->GetFullscreenState(&fullscreen, nullptr)) && fullscreen &&
			SUCCEEDED(m_swap_chain->GetDesc(&desc)))
		{
			m_window_info.surface_refresh_rate = static_cast<float>(desc.BufferDesc.RefreshRate.Numerator) /
			                                     static_cast<float>(desc.BufferDesc.RefreshRate.Denominator);
		}
	}

	m_current_swap_chain_buffer = 0;
	return true;
}

void GSDevice12::DestroySwapChainRTVs()
{
	for (std::unique_ptr<GSTexture12>& buffer : m_swap_chain_buffers)
		buffer->Destroy(false);
	m_swap_chain_buffers.clear();
	m_current_swap_chain_buffer = 0;
}

void GSDevice12::DestroySwapChain()
{
	if (!m_swap_chain)
		return;

	DestroySwapChainRTVs();

	// switch out of fullscreen before destroying
	BOOL is_fullscreen;
	if (SUCCEEDED(m_swap_chain->GetFullscreenState(&is_fullscreen, nullptr)) && is_fullscreen)
		m_swap_chain->SetFullscreenState(FALSE, nullptr);

	m_swap_chain.reset();
	m_is_exclusive_fullscreen = false;
}

bool GSDevice12::UpdateWindow()
{
	ExecuteCommandList(true);
	DestroySwapChain();

	if (!AcquireWindow(false))
		return false;

	if (m_window_info.type != WindowInfo::Type::Surfaceless && !CreateSwapChain())
	{
		Console.WriteLn("D3D12: Failed to create swap chain on updated window");
		return false;
	}

	return true;
}

void GSDevice12::DestroySurface()
{
	ExecuteCommandList(true);
	DestroySwapChain();
}

std::string GSDevice12::GetDriverInfo() const
{
	std::string ret = "Unknown Feature Level";

	static constexpr std::array<std::tuple<D3D_FEATURE_LEVEL, const char*>, 2> feature_level_names = {{
		{D3D_FEATURE_LEVEL_11_0, "D3D_FEATURE_LEVEL_11_0"},
		{D3D_FEATURE_LEVEL_11_1, "D3D_FEATURE_LEVEL_11_1"},
	}};

	for (size_t i = 0; i < std::size(feature_level_names); i++)
	{
		if (m_feature_level == std::get<0>(feature_level_names[i]))
		{
			ret = std::get<1>(feature_level_names[i]);
			break;
		}
	}

	ret += "\n";

	DXGI_ADAPTER_DESC desc;
	if (m_adapter && SUCCEEDED(m_adapter->GetDesc(&desc)))
	{
		ret += StringUtil::StdStringFromFormat("VID: 0x%04X PID: 0x%04X\n", desc.VendorId, desc.DeviceId);
		ret += StringUtil::WideStringToUTF8String(desc.Description);
		ret += "\n";

		const std::string driver_version(D3D::GetDriverVersionFromLUID(desc.AdapterLuid));
		if (!driver_version.empty())
		{
			ret += "Driver Version: ";
			ret += driver_version;
		}
	}

	return ret;
}

void GSDevice12::ResizeWindow(s32 new_window_width, s32 new_window_height, float new_window_scale)
{
	if (!m_swap_chain)
		return;

	m_window_info.surface_scale = new_window_scale;

	if (m_window_info.surface_width == new_window_width && m_window_info.surface_height == new_window_height)
		return;

	ExecuteCommandList(true);

	DestroySwapChainRTVs();

	HRESULT hr = m_swap_chain->ResizeBuffers(
		0, 0, 0, DXGI_FORMAT_UNKNOWN, m_using_allow_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
	if (FAILED(hr))
		Console.Error("D3D12: ResizeBuffers() failed: 0x%08X", hr);

	if (!CreateSwapChainRTV())
		pxFailRel("Failed to recreate swap chain RTV after resize");
}

bool GSDevice12::SupportsExclusiveFullscreen() const
{
	return true;
}

GSDevice::PresentResult GSDevice12::BeginPresent(bool frame_skip)
{
	EndRenderPass();

	if (m_device_lost)
		return PresentResult::DeviceLost;

	if (frame_skip || !m_swap_chain)
	{
		if (!m_swap_chain)
		{
			ExecuteCommandList(WaitType::None);
			InvalidateCachedState();
		}
		return PresentResult::FrameSkipped;
	}

	// Check if we lost exclusive fullscreen. If so, notify the host, so it can switch to windowed mode.
	// This might get called repeatedly if it takes a while to switch back, that's the host's problem.
	BOOL is_fullscreen;
	if (m_is_exclusive_fullscreen &&
		(FAILED(m_swap_chain->GetFullscreenState(&is_fullscreen, nullptr)) || !is_fullscreen))
	{
		Host::RunOnCPUThread([]() { Host::SetFullscreen(false); });
		return PresentResult::FrameSkipped;
	}

	GSTexture12* swap_chain_buf = m_swap_chain_buffers[m_current_swap_chain_buffer].get();

	ID3D12GraphicsCommandList* cmdlist = GetCommandList();
	swap_chain_buf->TransitionToState(cmdlist, D3D12_RESOURCE_STATE_RENDER_TARGET);
	cmdlist->ClearRenderTargetView(swap_chain_buf->GetWriteDescriptor(), s_present_clear_color.data(), 0, nullptr);
	cmdlist->OMSetRenderTargets(1, &swap_chain_buf->GetWriteDescriptor().cpu_handle, FALSE, nullptr);
	g_perfmon.Put(GSPerfMon::RenderPasses, 1);

	const D3D12_VIEWPORT vp{0.0f, 0.0f, static_cast<float>(m_window_info.surface_width),
		static_cast<float>(m_window_info.surface_height), 0.0f, 1.0f};
	const D3D12_RECT scissor{
		0, 0, static_cast<LONG>(m_window_info.surface_width), static_cast<LONG>(m_window_info.surface_height)};
	cmdlist->RSSetViewports(1, &vp);
	cmdlist->RSSetScissorRects(1, &scissor);
	return PresentResult::OK;
}

void GSDevice12::EndPresent()
{
	RenderImGui();

	GSTexture12* swap_chain_buf = m_swap_chain_buffers[m_current_swap_chain_buffer].get();
	m_current_swap_chain_buffer = ((m_current_swap_chain_buffer + 1) % static_cast<u32>(m_swap_chain_buffers.size()));

	swap_chain_buf->TransitionToState(GetCommandList(), D3D12_RESOURCE_STATE_PRESENT);
	if (!ExecuteCommandList(WaitType::None))
	{
		m_device_lost = true;
		InvalidateCachedState();
		return;
	}

	const UINT sync_interval = static_cast<UINT>(m_vsync_mode == GSVSyncMode::FIFO);
	const UINT flags = (m_vsync_mode == GSVSyncMode::Disabled && m_using_allow_tearing) ? DXGI_PRESENT_ALLOW_TEARING : 0;
	m_swap_chain->Present(sync_interval, flags);

	InvalidateCachedState();
}

#ifdef ENABLE_OGL_DEBUG
static UINT Palette(float phase, const std::array<float, 3>& a, const std::array<float, 3>& b,
	const std::array<float, 3>& c, const std::array<float, 3>& d)
{
	std::array<float, 3> result;
	result[0] = a[0] + b[0] * std::cos(6.28318f * (c[0] * phase + d[0]));
	result[1] = a[1] + b[1] * std::cos(6.28318f * (c[1] * phase + d[1]));
	result[2] = a[2] + b[2] * std::cos(6.28318f * (c[2] * phase + d[2]));
	return PIX_COLOR(static_cast<BYTE>(result[0] * 255.0f),
		static_cast<BYTE>(result[1] * 255.0f),
		static_cast<BYTE>(result[2] * 255.0f));
}
#endif

void GSDevice12::PushDebugGroup(const char* fmt, ...)
{
#ifdef ENABLE_OGL_DEBUG
	if (!GSConfig.UseDebugDevice)
		return;

	std::va_list ap;
	va_start(ap, fmt);
	const std::string buf(StringUtil::StdStringFromFormatV(fmt, ap));
	va_end(ap);

	const UINT color = Palette(
		++s_debug_scope_depth, {0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 0.5f}, {0.8f, 0.90f, 0.30f});

	PIXBeginEvent(GetCommandList(), color, "%s", buf.c_str());
#endif
}

void GSDevice12::PopDebugGroup()
{
#ifdef ENABLE_OGL_DEBUG
	if (!GSConfig.UseDebugDevice)
		return;

	s_debug_scope_depth = (s_debug_scope_depth == 0) ? 0 : (s_debug_scope_depth - 1u);

	PIXEndEvent(GetCommandList());
#endif
}

void GSDevice12::InsertDebugMessage(DebugMessageCategory category, const char* fmt, ...)
{
#ifdef ENABLE_OGL_DEBUG
	if (!GSConfig.UseDebugDevice)
		return;

	std::va_list ap;
	va_start(ap, fmt);
	const std::string buf(StringUtil::StdStringFromFormatV(fmt, ap));
	va_end(ap);

	if (buf.empty())
		return;

	static constexpr float colors[][3] = {
		{0.1f, 0.1f, 0.0f}, // Cache
		{0.1f, 0.1f, 0.0f}, // Reg
		{0.5f, 0.0f, 0.5f}, // Debug
		{0.0f, 0.5f, 0.5f}, // Message
		{0.0f, 0.2f, 0.0f} // Performance
	};

	const float* fcolor = colors[static_cast<int>(category)];
	const UINT color = PIX_COLOR(static_cast<BYTE>(fcolor[0] * 255.0f),
		static_cast<BYTE>(fcolor[1] * 255.0f),
		static_cast<BYTE>(fcolor[2] * 255.0f));

	PIXSetMarker(GetCommandList(), color, "%s", buf.c_str());
#endif
}

bool GSDevice12::CheckFeatures(const u32& vendor_id)
{
	//const bool isAMD = (vendor_id == 0x1002 || vendor_id == 0x1022);

	m_features.texture_barrier = false;
	m_features.multidraw_fb_copy = GSConfig.OverrideTextureBarriers != 0;
	m_features.broken_point_sampler = false;
	m_features.primitive_id = true;
	m_features.prefer_new_textures = true;
	m_features.provoking_vertex_last = false;
	m_features.point_expand = false;
	m_features.line_expand = false;
	m_features.framebuffer_fetch = false;
	m_features.stencil_buffer = true;
	m_features.cas_sharpening = true;
	m_features.test_and_sample_depth = true;
	m_features.vs_expand = !GSConfig.DisableVertexShaderExpand;

	m_features.dxt_textures = SupportsTextureFormat(DXGI_FORMAT_BC1_UNORM) &&
	                          SupportsTextureFormat(DXGI_FORMAT_BC2_UNORM) &&
	                          SupportsTextureFormat(DXGI_FORMAT_BC3_UNORM);
	m_features.bptc_textures = SupportsTextureFormat(DXGI_FORMAT_BC7_UNORM);

	m_max_texture_size = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;

	BOOL allow_tearing_supported = false;
	const HRESULT hr = m_dxgi_factory->CheckFeatureSupport(
		DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing_supported, sizeof(allow_tearing_supported));
	m_allow_tearing_supported = (SUCCEEDED(hr) && allow_tearing_supported == TRUE);

	return true;
}

void GSDevice12::DrawPrimitive()
{
	g_perfmon.Put(GSPerfMon::DrawCalls, 1);
	GetCommandList()->DrawInstanced(m_vertex.count, 1, m_vertex.start, 0);
}

void GSDevice12::DrawIndexedPrimitive()
{
	g_perfmon.Put(GSPerfMon::DrawCalls, 1);
	GetCommandList()->DrawIndexedInstanced(m_index.count, 1, m_index.start, m_vertex.start, 0);
}

void GSDevice12::DrawIndexedPrimitive(int offset, int count)
{
	pxAssert(offset + count <= (int)m_index.count);
	g_perfmon.Put(GSPerfMon::DrawCalls, 1);
	GetCommandList()->DrawIndexedInstanced(count, 1, m_index.start + offset, m_vertex.start, 0);
}

void GSDevice12::LookupNativeFormat(GSTexture::Format format, DXGI_FORMAT* d3d_format, DXGI_FORMAT* srv_format,
	DXGI_FORMAT* rtv_format, DXGI_FORMAT* dsv_format) const
{
	static constexpr std::array<std::array<DXGI_FORMAT, 4>, static_cast<int>(GSTexture::Format::Last) + 1>
		s_format_mapping = {{
			{DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN}, // Invalid
			{DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM,
				DXGI_FORMAT_UNKNOWN}, // Color
			{DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R10G10B10A2_UNORM,
				DXGI_FORMAT_UNKNOWN}, // ColorHQ
			{DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT,
				DXGI_FORMAT_UNKNOWN}, // ColorHDR
			{DXGI_FORMAT_R16G16B16A16_UNORM, DXGI_FORMAT_R16G16B16A16_UNORM, DXGI_FORMAT_R16G16B16A16_UNORM,
				DXGI_FORMAT_UNKNOWN}, // ColorClip
			{DXGI_FORMAT_D32_FLOAT_S8X24_UINT, DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS, DXGI_FORMAT_UNKNOWN,
				DXGI_FORMAT_D32_FLOAT_S8X24_UINT}, // DepthStencil
			{DXGI_FORMAT_A8_UNORM, DXGI_FORMAT_A8_UNORM, DXGI_FORMAT_A8_UNORM, DXGI_FORMAT_UNKNOWN}, // UNorm8
			{DXGI_FORMAT_R16_UINT, DXGI_FORMAT_R16_UINT, DXGI_FORMAT_R16_UINT, DXGI_FORMAT_UNKNOWN}, // UInt16
			{DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32_UINT, DXGI_FORMAT_UNKNOWN}, // UInt32
			{DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_UNKNOWN}, // Int32
			{DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN}, // BC1
			{DXGI_FORMAT_BC2_UNORM, DXGI_FORMAT_BC2_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN}, // BC2
			{DXGI_FORMAT_BC3_UNORM, DXGI_FORMAT_BC3_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN}, // BC3
			{DXGI_FORMAT_BC7_UNORM, DXGI_FORMAT_BC7_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN}, // BC7
		}};

	const auto& mapping = s_format_mapping[static_cast<int>(format)];
	if (d3d_format)
		*d3d_format = mapping[0];
	if (srv_format)
		*srv_format = mapping[1];
	if (rtv_format)
		*rtv_format = mapping[2];
	if (dsv_format)
		*dsv_format = mapping[3];
}

GSTexture* GSDevice12::CreateSurface(GSTexture::Type type, int width, int height, int levels, GSTexture::Format format)
{
	DXGI_FORMAT dxgi_format, srv_format, rtv_format, dsv_format;
	LookupNativeFormat(format, &dxgi_format, &srv_format, &rtv_format, &dsv_format);

	const DXGI_FORMAT uav_format = (type == GSTexture::Type::RWTexture) ? dxgi_format : DXGI_FORMAT_UNKNOWN;

	std::unique_ptr<GSTexture12> tex(GSTexture12::Create(type, format, width, height, levels,
		dxgi_format, srv_format, rtv_format, dsv_format, uav_format));
	if (!tex)
	{
		// We're probably out of vram, try flushing the command buffer to release pending textures.
		PurgePool();
		ExecuteCommandListAndRestartRenderPass(true, "Couldn't allocate texture.");
		tex = GSTexture12::Create(type, format, width, height, levels, dxgi_format, srv_format,
			rtv_format, dsv_format, uav_format);
	}

	return tex.release();
}

std::unique_ptr<GSDownloadTexture> GSDevice12::CreateDownloadTexture(u32 width, u32 height, GSTexture::Format format)
{
	return GSDownloadTexture12::Create(width, height, format);
}

void GSDevice12::CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r, u32 destX, u32 destY)
{
	// Empty rect, abort copy.
	if (r.rempty())
	{
		GL_INS("D3D12: CopyRect rect empty.");
		return;
	}

	GSTexture12* const sTex12 = static_cast<GSTexture12*>(sTex);
	GSTexture12* const dTex12 = static_cast<GSTexture12*>(dTex);
	const GSVector4i dst_rect(0, 0, dTex12->GetWidth(), dTex12->GetHeight());
	const bool full_draw_copy = dst_rect.eq(r);

	// Source is cleared, if destination is a render target, we can carry the clear forward.
	if (sTex12->GetState() == GSTexture::State::Cleared)
	{
		if (dTex12->IsRenderTargetOrDepthStencil())
		{
			if (ProcessClearsBeforeCopy(sTex, dTex, full_draw_copy))
				return;

			// Do an attachment clear.
			EndRenderPass();

			dTex12->SetState(GSTexture::State::Dirty);

			if (dTex12->GetType() != GSTexture::Type::DepthStencil)
			{
				dTex12->TransitionToState(D3D12_RESOURCE_STATE_RENDER_TARGET);
				GetCommandList()->ClearRenderTargetView(
					dTex12->GetWriteDescriptor(), sTex12->GetUNormClearColor().v, 0, nullptr);
			}
			else
			{
				dTex12->TransitionToState(D3D12_RESOURCE_STATE_DEPTH_WRITE);
				GetCommandList()->ClearDepthStencilView(
					dTex12->GetWriteDescriptor(), D3D12_CLEAR_FLAG_DEPTH, sTex12->GetClearDepth(), 0, 0, nullptr);
			}

			return;
		}

		// commit the clear to the source first, then do normal copy
		sTex12->CommitClear();
	}

	g_perfmon.Put(GSPerfMon::TextureCopies, 1);

	// if the destination has been cleared, and we're not overwriting the whole thing, commit the clear first
	// (the area outside of where we're copying to)
	if (dTex12->GetState() == GSTexture::State::Cleared && !full_draw_copy)
		dTex12->CommitClear();

	EndRenderPass();

	sTex12->TransitionToState(D3D12_RESOURCE_STATE_COPY_SOURCE);
	sTex12->SetUseFenceCounter(GetCurrentFenceValue());
	if (m_tfx_textures[0] && sTex12->GetSRVDescriptor() == m_tfx_textures[0])
		PSSetShaderResource(0, nullptr, false);

	dTex12->TransitionToState(D3D12_RESOURCE_STATE_COPY_DEST);
	dTex12->SetUseFenceCounter(GetCurrentFenceValue());

	D3D12_TEXTURE_COPY_LOCATION srcloc;
	srcloc.pResource = sTex12->GetResource();
	srcloc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	srcloc.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION dstloc;
	dstloc.pResource = dTex12->GetResource();
	dstloc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dstloc.SubresourceIndex = 0;

	const GSVector4i src_rect(0, 0, sTex->GetWidth(), sTex->GetHeight());
	const bool full_rt_copy = destX == 0 && destY == 0 && r.eq(src_rect) && src_rect.eq(dst_rect);
	if (full_rt_copy)
	{
		GetCommandList()->CopyResource(dTex12->GetResource(), sTex12->GetResource());
	}
	else
	{
		const D3D12_BOX srcbox{static_cast<UINT>(r.left), static_cast<UINT>(r.top), 0u, static_cast<UINT>(r.right),
			static_cast<UINT>(r.bottom), 1u};
		GetCommandList()->CopyTextureRegion(&dstloc, destX, destY, 0, &srcloc, &srcbox);
	}

	dTex12->SetState(GSTexture::State::Dirty);
}

void GSDevice12::DoStretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect,
	GSHWDrawConfig::ColorMaskSelector cms, ShaderConvert shader, bool linear)
{
	const bool allow_discard = (cms.wrgba == 0xf);
	const ID3D12PipelineState* state;
	if (HasVariableWriteMask(shader))
		state = m_color_copy[GetShaderIndexForMask(shader, cms.wrgba)].get();
	else
		state = dTex ? m_convert[static_cast<int>(shader)].get() : m_present[static_cast<int>(shader)].get();
	DoStretchRect(static_cast<GSTexture12*>(sTex), sRect, static_cast<GSTexture12*>(dTex), dRect,
		state, linear, allow_discard);
}

void GSDevice12::PresentRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect,
	PresentShader shader, float shaderTime, bool linear)
{
	DisplayConstantBuffer cb;
	cb.SetSource(sRect, sTex->GetSize());
	cb.SetTarget(dRect, dTex ? dTex->GetSize() : GSVector2i(GetWindowWidth(), GetWindowHeight()));
	cb.SetTime(shaderTime);
	SetUtilityRootSignature();
	SetUtilityPushConstants(&cb, sizeof(cb));

	DoStretchRect(static_cast<GSTexture12*>(sTex), sRect, static_cast<GSTexture12*>(dTex), dRect,
		m_present[static_cast<int>(shader)].get(), linear, true);
}

void GSDevice12::UpdateCLUTTexture(
	GSTexture* sTex, float sScale, u32 offsetX, u32 offsetY, GSTexture* dTex, u32 dOffset, u32 dSize)
{
	// match merge cb
	struct Uniforms
	{
		float scale;
		float pad1[3];
		u32 offsetX, offsetY, dOffset;
		u32 pad2;
	};
	const Uniforms cb = {sScale, {}, offsetX, offsetY, dOffset};
	SetUtilityRootSignature();
	SetUtilityPushConstants(&cb, sizeof(cb));

	const GSVector4 dRect(0, 0, dSize, 1);
	const ShaderConvert shader = (dSize == 16) ? ShaderConvert::CLUT_4 : ShaderConvert::CLUT_8;
	DoStretchRect(static_cast<GSTexture12*>(sTex), GSVector4::zero(), static_cast<GSTexture12*>(dTex), dRect,
		m_convert[static_cast<int>(shader)].get(), false, true);
}

void GSDevice12::ConvertToIndexedTexture(
	GSTexture* sTex, float sScale, u32 offsetX, u32 offsetY, u32 SBW, u32 SPSM, GSTexture* dTex, u32 DBW, u32 DPSM)
{
	// match merge cb
	struct Uniforms
	{
		float scale;
		float pad1[3];
		u32 SBW, DBW, SPSM;
	};

	const Uniforms cb = {sScale, {}, SBW, DBW, SPSM};
	SetUtilityRootSignature();
	SetUtilityPushConstants(&cb, sizeof(cb));

	const GSVector4 dRect(0, 0, dTex->GetWidth(), dTex->GetHeight());
	const ShaderConvert shader = ((SPSM & 0xE) == 0) ? ShaderConvert::RGBA_TO_8I : ShaderConvert::RGB5A1_TO_8I;
	DoStretchRect(static_cast<GSTexture12*>(sTex), GSVector4::zero(), static_cast<GSTexture12*>(dTex), dRect,
		m_convert[static_cast<int>(shader)].get(), false, true);
}

void GSDevice12::FilteredDownsampleTexture(GSTexture* sTex, GSTexture* dTex, u32 downsample_factor, const GSVector2i& clamp_min, const GSVector4& dRect)
{
	struct Uniforms
	{
		float weight;
		float step_multiplier;
		float pad0[2];
		GSVector2i clamp_min;
		int downsample_factor;
		int pad1;
	};

	const Uniforms cb = {
		static_cast<float>(downsample_factor * downsample_factor), (GSConfig.UserHacks_NativeScaling > GSNativeScaling::Aggressive) ? 2.0f : 1.0f, {}, clamp_min, static_cast<int>(downsample_factor), 0};
	SetUtilityRootSignature();
	SetUtilityPushConstants(&cb, sizeof(cb));

	//const GSVector4 dRect = GSVector4(dTex->GetRect());
	const ShaderConvert shader = ShaderConvert::DOWNSAMPLE_COPY;
	DoStretchRect(static_cast<GSTexture12*>(sTex), GSVector4::zero(), static_cast<GSTexture12*>(dTex), dRect,
		m_convert[static_cast<int>(shader)].get(), false, true);
}

void GSDevice12::DrawMultiStretchRects(
	const MultiStretchRect* rects, u32 num_rects, GSTexture* dTex, ShaderConvert shader)
{
	GSTexture* last_tex = rects[0].src;
	bool last_linear = rects[0].linear;
	u8 last_wmask = rects[0].wmask.wrgba;

	u32 first = 0;
	u32 count = 1;

	// Make sure all textures are in shader read only layout, so we don't need to break
	// the render pass to transition.
	for (u32 i = 0; i < num_rects; i++)
	{
		GSTexture12* const stex = static_cast<GSTexture12*>(rects[i].src);
		stex->CommitClear();
		if (stex->GetResourceState() != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		{
			EndRenderPass();
			stex->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
	}

	for (u32 i = 1; i < num_rects; i++)
	{
		if (rects[i].src == last_tex && rects[i].linear == last_linear && rects[i].wmask.wrgba == last_wmask)
		{
			count++;
			continue;
		}

		DoMultiStretchRects(rects + first, count, static_cast<GSTexture12*>(dTex), shader);
		last_tex = rects[i].src;
		last_linear = rects[i].linear;
		last_wmask = rects[i].wmask.wrgba;
		first += count;
		count = 1;
	}

	DoMultiStretchRects(rects + first, count, static_cast<GSTexture12*>(dTex), shader);
}

void GSDevice12::DoMultiStretchRects(
	const MultiStretchRect* rects, u32 num_rects, GSTexture12* dTex, ShaderConvert shader)
{
	// Set up vertices first.
	const u32 vertex_reserve_size = num_rects * 4 * sizeof(GSVertexPT1);
	const u32 index_reserve_size = num_rects * 6 * sizeof(u16);
	if (!m_vertex_stream_buffer.ReserveMemory(vertex_reserve_size, sizeof(GSVertexPT1)) ||
		!m_index_stream_buffer.ReserveMemory(index_reserve_size, sizeof(u16)))
	{
		ExecuteCommandListAndRestartRenderPass(false, "Uploading bytes to vertex buffer");
		if (!m_vertex_stream_buffer.ReserveMemory(vertex_reserve_size, sizeof(GSVertexPT1)) ||
			!m_index_stream_buffer.ReserveMemory(index_reserve_size, sizeof(u16)))
		{
			pxFailRel("Failed to reserve space for vertices");
		}
	}

	// Pain in the arse because the primitive topology for the pipelines is all triangle strips.
	// Don't use primitive restart here, it ends up slower on some drivers.
	const GSVector2 ds(static_cast<float>(dTex->GetWidth()), static_cast<float>(dTex->GetHeight()));
	GSVertexPT1* verts = reinterpret_cast<GSVertexPT1*>(m_vertex_stream_buffer.GetCurrentHostPointer());
	u16* idx = reinterpret_cast<u16*>(m_index_stream_buffer.GetCurrentHostPointer());
	u32 icount = 0;
	u32 vcount = 0;
	for (u32 i = 0; i < num_rects; i++)
	{
		const GSVector4& sRect = rects[i].src_rect;
		const GSVector4& dRect = rects[i].dst_rect;

		const float left = dRect.x * 2 / ds.x - 1.0f;
		const float top = 1.0f - dRect.y * 2 / ds.y;
		const float right = dRect.z * 2 / ds.x - 1.0f;
		const float bottom = 1.0f - dRect.w * 2 / ds.y;

		const u32 vstart = vcount;
		verts[vcount++] = {GSVector4(left, top, 0.5f, 1.0f), GSVector2(sRect.x, sRect.y)};
		verts[vcount++] = {GSVector4(right, top, 0.5f, 1.0f), GSVector2(sRect.z, sRect.y)};
		verts[vcount++] = {GSVector4(left, bottom, 0.5f, 1.0f), GSVector2(sRect.x, sRect.w)};
		verts[vcount++] = {GSVector4(right, bottom, 0.5f, 1.0f), GSVector2(sRect.z, sRect.w)};

		if (i > 0)
			idx[icount++] = vstart;

		idx[icount++] = vstart;
		idx[icount++] = vstart + 1;
		idx[icount++] = vstart + 2;
		idx[icount++] = vstart + 3;
		idx[icount++] = vstart + 3;
	};

	m_vertex.start = m_vertex_stream_buffer.GetCurrentOffset() / sizeof(GSVertexPT1);
	m_vertex.count = vcount;
	m_index.start = m_index_stream_buffer.GetCurrentOffset() / sizeof(u16);
	m_index.count = icount;
	m_vertex_stream_buffer.CommitMemory(vcount * sizeof(GSVertexPT1));
	m_index_stream_buffer.CommitMemory(icount * sizeof(u16));
	SetVertexBuffer(m_vertex_stream_buffer.GetGPUPointer(), m_vertex_stream_buffer.GetSize(), sizeof(GSVertexPT1));
	SetIndexBuffer(m_index_stream_buffer.GetGPUPointer(), m_index_stream_buffer.GetSize(), DXGI_FORMAT_R16_UINT);

	// Even though we're batching, a cmdbuffer submit could've messed this up.
	const GSVector4i rc(dTex->GetRect());
	OMSetRenderTargets(dTex->IsRenderTarget() ? dTex : nullptr, dTex->IsDepthStencil() ? dTex : nullptr, rc);
	if (!InRenderPass())
		BeginRenderPassForStretchRect(dTex, rc, rc, false);
	SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	SetUtilityTexture(rects[0].src, rects[0].linear ? m_linear_sampler_cpu : m_point_sampler_cpu);

	pxAssert(HasVariableWriteMask(shader) || rects[0].wmask.wrgba == 0xf);
	SetPipeline((rects[0].wmask.wrgba != 0xf) ?
		m_color_copy[GetShaderIndexForMask(shader, rects[0].wmask.wrgba)].get() :
		m_convert[static_cast<int>(shader)].get());

	if (ApplyUtilityState())
		DrawIndexedPrimitive();
}

void GSDevice12::BeginRenderPassForStretchRect(
	GSTexture12* dTex, const GSVector4i& dtex_rc, const GSVector4i& dst_rc, bool allow_discard)
{
	const D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE load_op = (allow_discard && dst_rc.eq(dtex_rc)) ?
	                                                            D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD :
	                                                            GetLoadOpForTexture(dTex);
	dTex->SetState(GSTexture::State::Dirty);

	if (dTex->GetType() != GSTexture::Type::DepthStencil)
	{
		BeginRenderPass(load_op, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE,
			D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
			D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
			dTex->GetUNormClearColor());
	}
	else
	{
		BeginRenderPass(D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS,
			D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS, load_op, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE,
			D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
			GSVector4::zero(), dTex->GetClearDepth());
	}
}

void GSDevice12::DoStretchRect(GSTexture12* sTex, const GSVector4& sRect, GSTexture12* dTex, const GSVector4& dRect,
	const ID3D12PipelineState* pipeline, bool linear, bool allow_discard)
{
	if (sTex->GetResourceState() != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
	{
		// can't transition in a render pass
		EndRenderPass();
		sTex->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	SetUtilityRootSignature();
	SetUtilityTexture(sTex, linear ? m_linear_sampler_cpu : m_point_sampler_cpu);
	SetPipeline(pipeline);

	const bool is_present = (!dTex);
	const bool depth = (dTex && dTex->GetType() == GSTexture::Type::DepthStencil);
	const GSVector2i size(is_present ? GSVector2i(GetWindowWidth(), GetWindowHeight()) : dTex->GetSize());
	const GSVector4i dtex_rc(0, 0, size.x, size.y);
	const GSVector4i dst_rc(GSVector4i(dRect).rintersect(dtex_rc));

	// switch rts (which might not end the render pass), so check the bounds
	if (!is_present)
	{
		OMSetRenderTargets(depth ? nullptr : dTex, depth ? dTex : nullptr, dst_rc);
	}
	else
	{
		// this is for presenting, we don't want to screw with the viewport/scissor set by display
		m_dirty_flags &= ~(DIRTY_FLAG_RENDER_TARGET | DIRTY_FLAG_VIEWPORT | DIRTY_FLAG_SCISSOR);
	}

	const bool drawing_to_current_rt = (is_present || InRenderPass());
	if (!drawing_to_current_rt)
		BeginRenderPassForStretchRect(dTex, dtex_rc, dst_rc, allow_discard);

	DrawStretchRect(sRect, dRect, size);
}

void GSDevice12::DrawStretchRect(const GSVector4& sRect, const GSVector4& dRect, const GSVector2i& ds)
{
	// ia
	const float left = dRect.x * 2 / ds.x - 1.0f;
	const float top = 1.0f - dRect.y * 2 / ds.y;
	const float right = dRect.z * 2 / ds.x - 1.0f;
	const float bottom = 1.0f - dRect.w * 2 / ds.y;

	GSVertexPT1 vertices[] = {
		{GSVector4(left, top, 0.5f, 1.0f), GSVector2(sRect.x, sRect.y)},
		{GSVector4(right, top, 0.5f, 1.0f), GSVector2(sRect.z, sRect.y)},
		{GSVector4(left, bottom, 0.5f, 1.0f), GSVector2(sRect.x, sRect.w)},
		{GSVector4(right, bottom, 0.5f, 1.0f), GSVector2(sRect.z, sRect.w)},
	};
	IASetVertexBuffer(vertices, sizeof(vertices[0]), std::size(vertices));
	SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	if (ApplyUtilityState())
		DrawPrimitive();
}

void GSDevice12::DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect,
	const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, u32 c, const bool linear)
{
	GL_PUSH("DoMerge");

	const GSVector4 full_r(0.0f, 0.0f, 1.0f, 1.0f);
	const bool feedback_write_2 = PMODE.EN2 && sTex[2] != nullptr && EXTBUF.FBIN == 1;
	const bool feedback_write_1 = PMODE.EN1 && sTex[2] != nullptr && EXTBUF.FBIN == 0;
	const bool feedback_write_2_but_blend_bg = feedback_write_2 && PMODE.SLBG == 1;
	const D3D12DescriptorHandle& sampler = linear ? m_linear_sampler_cpu : m_point_sampler_cpu;
	// Merge the 2 source textures (sTex[0],sTex[1]). Final results go to dTex. Feedback write will go to sTex[2].
	// If either 2nd output is disabled or SLBG is 1, a background color will be used.
	// Note: background color is also used when outside of the unit rectangle area
	EndRenderPass();

	// transition everything before starting the new render pass
	const bool has_input_0 =
		(sTex[0] && (sTex[0]->GetState() == GSTexture::State::Dirty ||
						(sTex[0]->GetState() == GSTexture::State::Cleared || sTex[0]->GetClearColor() != 0)));
	const bool has_input_1 = (PMODE.SLBG == 0 || feedback_write_2_but_blend_bg) && sTex[1] &&
	                         (sTex[1]->GetState() == GSTexture::State::Dirty ||
	                             (sTex[1]->GetState() == GSTexture::State::Cleared || sTex[1]->GetClearColor() != 0));
	if (has_input_0)
	{
		static_cast<GSTexture12*>(sTex[0])->CommitClear();
		static_cast<GSTexture12*>(sTex[0])->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
	if (has_input_1)
	{
		static_cast<GSTexture12*>(sTex[1])->CommitClear();
		static_cast<GSTexture12*>(sTex[1])->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
	static_cast<GSTexture12*>(dTex)->TransitionToState(D3D12_RESOURCE_STATE_RENDER_TARGET);

	// Upload constant to select YUV algo, but skip constant buffer update if we don't need it
	if (feedback_write_2 || feedback_write_1 || sTex[0])
	{
		SetUtilityRootSignature();
		const MergeConstantBuffer uniforms = {GSVector4::unorm8(c), EXTBUF.EMODA, EXTBUF.EMODC};
		SetUtilityPushConstants(&uniforms, sizeof(uniforms));
	}

	const GSVector2i dsize(dTex->GetSize());
	const GSVector4i darea(0, 0, dsize.x, dsize.y);
	bool dcleared = false;
	if (has_input_1 && (PMODE.SLBG == 0 || feedback_write_2_but_blend_bg))
	{
		// 2nd output is enabled and selected. Copy it to destination so we can blend it with 1st output
		// Note: value outside of dRect must contains the background color (c)
		OMSetRenderTargets(dTex, nullptr, darea);
		SetUtilityTexture(sTex[1], sampler);
		BeginRenderPass(D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR,
			D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE, D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS,
			D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS,
			D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS, GSVector4::unorm8(c));
		SetUtilityRootSignature();
		SetPipeline(m_convert[static_cast<int>(ShaderConvert::COPY)].get());
		DrawStretchRect(sRect[1], PMODE.SLBG ? dRect[2] : dRect[1], dsize);
		dTex->SetState(GSTexture::State::Dirty);
		dcleared = true;
	}

	// Upload constant to select YUV algo
	const GSVector2i fbsize(sTex[2] ? sTex[2]->GetSize() : GSVector2i(0, 0));
	const GSVector4i fbarea(0, 0, fbsize.x, fbsize.y);
	if (feedback_write_2) // FIXME I'm not sure dRect[1] is always correct
	{
		EndRenderPass();
		OMSetRenderTargets(sTex[2], nullptr, fbarea);
		if (dcleared)
			SetUtilityTexture(dTex, sampler);

		// sTex[2] can be sTex[0], in which case it might be cleared (e.g. Xenosaga).
		BeginRenderPassForStretchRect(static_cast<GSTexture12*>(sTex[2]), fbarea, GSVector4i(dRect[2]));
		if (dcleared)
		{
			SetUtilityRootSignature();
			SetPipeline(m_convert[static_cast<int>(ShaderConvert::YUV)].get());
			DrawStretchRect(full_r, dRect[2], fbsize);
		}
		EndRenderPass();

		if (sTex[0] == sTex[2])
		{
			// need a barrier here because of the render pass
			static_cast<GSTexture12*>(sTex[2])->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
	}

	// Restore background color to process the normal merge
	if (feedback_write_2_but_blend_bg || !dcleared)
	{
		EndRenderPass();
		OMSetRenderTargets(dTex, nullptr, darea);
		BeginRenderPass(D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE,
			D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
			D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
			GSVector4::unorm8(c));
		dTex->SetState(GSTexture::State::Dirty);
	}
	else if (!InRenderPass())
	{
		OMSetRenderTargets(dTex, nullptr, darea);
		BeginRenderPass(
			D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE);
	}

	if (has_input_0)
	{
		// 1st output is enabled. It must be blended
		SetUtilityRootSignature();
		SetUtilityTexture(sTex[0], sampler);
		SetPipeline(m_merge[PMODE.MMOD].get());
		DrawStretchRect(sRect[0], dRect[0], dTex->GetSize());
	}

	if (feedback_write_1) // FIXME I'm not sure dRect[0] is always correct
	{
		EndRenderPass();
		SetUtilityRootSignature();
		SetPipeline(m_convert[static_cast<int>(ShaderConvert::YUV)].get());
		SetUtilityTexture(dTex, sampler);
		OMSetRenderTargets(sTex[2], nullptr, fbarea);
		BeginRenderPass(
			D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE);
		DrawStretchRect(full_r, dRect[2], dsize);
	}

	EndRenderPass();

	// this texture is going to get used as an input, so make sure we don't read undefined data
	static_cast<GSTexture12*>(dTex)->CommitClear();
	static_cast<GSTexture12*>(dTex)->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void GSDevice12::DoInterlace(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect,
	ShaderInterlace shader, bool linear, const InterlaceConstantBuffer& cb)
{
	static_cast<GSTexture12*>(dTex)->TransitionToState(D3D12_RESOURCE_STATE_RENDER_TARGET);

	const GSVector4i rc = GSVector4i(dRect);
	const GSVector4i dtex_rc = dTex->GetRect();
	const GSVector4i clamped_rc = rc.rintersect(dtex_rc);
	EndRenderPass();
	OMSetRenderTargets(dTex, nullptr, clamped_rc);
	SetUtilityRootSignature();
	SetUtilityTexture(sTex, linear ? m_linear_sampler_cpu : m_point_sampler_cpu);
	BeginRenderPassForStretchRect(static_cast<GSTexture12*>(dTex), dTex->GetRect(), clamped_rc, false);
	SetPipeline(m_interlace[static_cast<int>(shader)].get());
	SetUtilityPushConstants(&cb, sizeof(cb));
	DrawStretchRect(sRect, dRect, dTex->GetSize());
	EndRenderPass();

	// this texture is going to get used as an input, so make sure we don't read undefined data
	static_cast<GSTexture12*>(dTex)->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void GSDevice12::DoShadeBoost(GSTexture* sTex, GSTexture* dTex, const float params[4])
{
	const GSVector4 sRect = GSVector4(0.0f, 0.0f, 1.0f, 1.0f);
	const GSVector4i dRect = dTex->GetRect();
	EndRenderPass();
	OMSetRenderTargets(dTex, nullptr, dRect);
	SetUtilityRootSignature();
	SetUtilityTexture(sTex, m_point_sampler_cpu);
	BeginRenderPass(D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE,
		D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS);
	dTex->SetState(GSTexture::State::Dirty);
	SetPipeline(m_shadeboost_pipeline.get());
	SetUtilityPushConstants(params, sizeof(float) * 4);
	DrawStretchRect(sRect, GSVector4(dRect), dTex->GetSize());
	EndRenderPass();

	static_cast<GSTexture12*>(dTex)->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void GSDevice12::DoFXAA(GSTexture* sTex, GSTexture* dTex)
{
	const GSVector4 sRect = GSVector4(0.0f, 0.0f, 1.0f, 1.0f);
	const GSVector4i dRect = dTex->GetRect();
	EndRenderPass();
	OMSetRenderTargets(dTex, nullptr, dRect);
	SetUtilityRootSignature();
	SetUtilityTexture(sTex, m_linear_sampler_cpu);
	BeginRenderPass(D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE,
		D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS);
	dTex->SetState(GSTexture::State::Dirty);
	SetPipeline(m_fxaa_pipeline.get());
	DrawStretchRect(sRect, GSVector4(dRect), dTex->GetSize());
	EndRenderPass();

	static_cast<GSTexture12*>(dTex)->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

bool GSDevice12::CompileCASPipelines()
{
	D3D12::RootSignatureBuilder rsb;
	rsb.Add32BitConstants(0, NUM_CAS_CONSTANTS, D3D12_SHADER_VISIBILITY_ALL);
	rsb.AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
	rsb.AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
	m_cas_root_signature = rsb.Create(false);
	if (!m_cas_root_signature)
		return false;

	std::optional<std::string> cas_source = ReadShaderSource("shaders/dx11/cas.hlsl");
	if (!cas_source.has_value() || !GetCASShaderSource(&cas_source.value()))
		return false;

	static constexpr D3D_SHADER_MACRO sharpen_only_macros[] = {{"CAS_SHARPEN_ONLY", "1"}, {nullptr, nullptr}};

	const ComPtr<ID3DBlob> cs_upscale(m_shader_cache.GetComputeShader(cas_source.value(), nullptr, "main"));
	const ComPtr<ID3DBlob> cs_sharpen(m_shader_cache.GetComputeShader(cas_source.value(), sharpen_only_macros, "main"));
	if (!cs_upscale || !cs_sharpen)
		return false;

	D3D12::ComputePipelineBuilder cpb;
	cpb.SetRootSignature(m_cas_root_signature.get());
	cpb.SetShader(cs_upscale->GetBufferPointer(), cs_upscale->GetBufferSize());
	m_cas_upscale_pipeline = cpb.Create(m_device.get(), m_shader_cache, false);
	cpb.SetShader(cs_sharpen->GetBufferPointer(), cs_sharpen->GetBufferSize());
	m_cas_sharpen_pipeline = cpb.Create(m_device.get(), m_shader_cache, false);
	if (!m_cas_upscale_pipeline || !m_cas_sharpen_pipeline)
	{
		Console.Error("D3D12: Failed to create CAS pipelines");
		return false;
	}

	return true;
}

bool GSDevice12::CompileImGuiPipeline()
{
	const std::optional<std::string> hlsl = ReadShaderSource("shaders/dx11/imgui.fx");
	if (!hlsl.has_value())
	{
		Console.Error("D3D12: Failed to read imgui.fx");
		return false;
	}

	const ComPtr<ID3DBlob> vs = m_shader_cache.GetVertexShader(hlsl.value(), nullptr, "vs_main");
	const ComPtr<ID3DBlob> ps = m_shader_cache.GetPixelShader(hlsl.value(), nullptr, "ps_main");
	if (!vs || !ps)
	{
		Console.Error("D3D12: Failed to compile ImGui shaders");
		return false;
	}

	D3D12::GraphicsPipelineBuilder gpb;
	gpb.SetRootSignature(m_utility_root_signature.get());
	gpb.AddVertexAttribute("POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(ImDrawVert, pos));
	gpb.AddVertexAttribute("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(ImDrawVert, uv));
	gpb.AddVertexAttribute("COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(ImDrawVert, col));
	gpb.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	gpb.SetVertexShader(vs.get());
	gpb.SetPixelShader(ps.get());
	gpb.SetNoCullRasterizationState();
	gpb.SetNoDepthTestState();
	gpb.SetBlendState(0, true, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD, D3D12_BLEND_ONE,
		D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD);
	gpb.SetRenderTarget(0, DXGI_FORMAT_R8G8B8A8_UNORM);

	m_imgui_pipeline = gpb.Create(m_device.get(), m_shader_cache, false);
	if (!m_imgui_pipeline)
	{
		Console.Error("D3D12: Failed to compile ImGui pipeline");
		return false;
	}

	D3D12::SetObjectName(m_imgui_pipeline.get(), "ImGui pipeline");
	return true;
}

void GSDevice12::RenderImGui()
{
	ImGui::Render();
	const ImDrawData* draw_data = ImGui::GetDrawData();
	if (draw_data->CmdListsCount == 0)
		return;

	UpdateImGuiTextures();

	const float L = 0.0f;
	const float R = static_cast<float>(m_window_info.surface_width);
	const float T = 0.0f;
	const float B = static_cast<float>(m_window_info.surface_height);

	// clang-format off
  const float ortho_projection[4][4] =
	{
		{ 2.0f/(R-L),   0.0f,           0.0f,       0.0f },
		{ 0.0f,         2.0f/(T-B),     0.0f,       0.0f },
		{ 0.0f,         0.0f,           0.5f,       0.0f },
		{ (R+L)/(L-R),  (T+B)/(B-T),    0.5f,       1.0f },
	};
	// clang-format on

	SetUtilityRootSignature();
	SetUtilityPushConstants(ortho_projection, sizeof(ortho_projection));
	SetPipeline(m_imgui_pipeline.get());
	SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	if (m_utility_sampler_cpu != m_linear_sampler_cpu)
	{
		m_utility_sampler_cpu = m_linear_sampler_cpu;
		m_dirty_flags |= DIRTY_FLAG_SAMPLERS_DESCRIPTOR_TABLE;

		// just skip if we run out.. we can't resume the present render pass :/
		if (!GetSamplerAllocator().LookupSingle(&m_utility_sampler_gpu, m_linear_sampler_cpu))
		{
			Console.Warning("D3D12: Skipping ImGui draw because of no descriptors");
			return;
		}
	}

	// this is for presenting, we don't want to screw with the viewport/scissor set by display
	m_dirty_flags &= ~(DIRTY_FLAG_RENDER_TARGET | DIRTY_FLAG_VIEWPORT | DIRTY_FLAG_SCISSOR);

	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];

		u32 vertex_offset;
		{
			const u32 size = sizeof(ImDrawVert) * static_cast<u32>(cmd_list->VtxBuffer.Size);
			if (!m_vertex_stream_buffer.ReserveMemory(size, sizeof(ImDrawVert)))
			{
				Console.Warning("D3D12: Skipping ImGui draw because of no vertex buffer space");
				return;
			}

			vertex_offset = m_vertex_stream_buffer.GetCurrentOffset() / sizeof(ImDrawVert);
			std::memcpy(m_vertex_stream_buffer.GetCurrentHostPointer(), cmd_list->VtxBuffer.Data, size);
			m_vertex_stream_buffer.CommitMemory(size);
		}

		SetVertexBuffer(m_vertex_stream_buffer.GetGPUPointer(), m_vertex_stream_buffer.GetSize(), sizeof(ImDrawVert));

		static_assert(sizeof(ImDrawIdx) == sizeof(u16));
		IASetIndexBuffer(cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size);

		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			pxAssert(!pcmd->UserCallback);

			const GSVector4 clip = GSVector4::load<false>(&pcmd->ClipRect);
			if ((clip.zwzw() <= clip.xyxy()).mask() != 0)
				continue;

			SetScissor(GSVector4i(clip));

			GSTexture12* tex = reinterpret_cast<GSTexture12*>(pcmd->GetTexID());
			D3D12DescriptorHandle handle = m_null_texture->GetSRVDescriptor();
			if (tex)
			{
				tex->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				handle = tex->GetSRVDescriptor();
			}

			if (m_utility_texture_cpu != handle)
			{
				m_utility_texture_cpu = handle;
				m_dirty_flags |= DIRTY_FLAG_TEXTURES_DESCRIPTOR_TABLE;

				if (!GetTextureGroupDescriptors(&m_utility_texture_gpu, &handle, 1))
				{
					Console.Warning("D3D12: Skipping ImGui draw because of no descriptors");
					return;
				}
			}

			if (ApplyUtilityState())
			{
				GetCommandList()->DrawIndexedInstanced(
					pcmd->ElemCount, 1, m_index.start + pcmd->IdxOffset, vertex_offset + pcmd->VtxOffset, 0);
			}
		}

		g_perfmon.Put(GSPerfMon::DrawCalls, cmd_list->CmdBuffer.Size);
	}
}

bool GSDevice12::DoCAS(
	GSTexture* sTex, GSTexture* dTex, bool sharpen_only, const std::array<u32, NUM_CAS_CONSTANTS>& constants)
{
	EndRenderPass();

	GSTexture12* const sTex12 = static_cast<GSTexture12*>(sTex);
	GSTexture12* const dTex12 = static_cast<GSTexture12*>(dTex);
	D3D12DescriptorHandle sTexDH, dTexDH;
	if (!GetTextureGroupDescriptors(&sTexDH, &sTex12->GetSRVDescriptor(), 1) ||
		!GetTextureGroupDescriptors(&dTexDH, &dTex12->GetUAVDescriptor(), 1))
	{
		ExecuteCommandList(false, "Ran out of descriptors for CAS");
		if (!GetTextureGroupDescriptors(&sTexDH, &sTex12->GetSRVDescriptor(), 1) ||
			!GetTextureGroupDescriptors(&dTexDH, &dTex12->GetUAVDescriptor(), 1))
		{
			Console.Error("D3D12: Failed to allocate CAS descriptors.");
			return false;
		}
	}

	ID3D12GraphicsCommandList* const cmdlist = GetCommandList();
	const D3D12_RESOURCE_STATES old_state = sTex12->GetResourceState();
	sTex12->TransitionToState(cmdlist, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	dTex12->TransitionToState(cmdlist, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	cmdlist->SetComputeRootSignature(m_cas_root_signature.get());
	cmdlist->SetComputeRoot32BitConstants(
		CAS_ROOT_SIGNATURE_PARAM_PUSH_CONSTANTS, NUM_CAS_CONSTANTS, constants.data(), 0);
	cmdlist->SetComputeRootDescriptorTable(CAS_ROOT_SIGNATURE_PARAM_SRC_TEXTURE, sTexDH);
	cmdlist->SetComputeRootDescriptorTable(CAS_ROOT_SIGNATURE_PARAM_DST_TEXTURE, dTexDH);
	cmdlist->SetPipelineState(sharpen_only ? m_cas_sharpen_pipeline.get() : m_cas_upscale_pipeline.get());
	m_dirty_flags |= DIRTY_FLAG_PIPELINE;

	static const int threadGroupWorkRegionDim = 16;
	const int dispatchX = (dTex->GetWidth() + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
	const int dispatchY = (dTex->GetHeight() + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
	cmdlist->Dispatch(dispatchX, dispatchY, 1);

	sTex12->TransitionToState(cmdlist, old_state);
	return true;
}

void GSDevice12::IASetVertexBuffer(const void* vertex, size_t stride, size_t count)
{
	const u32 size = static_cast<u32>(stride) * static_cast<u32>(count);
	if (!m_vertex_stream_buffer.ReserveMemory(size, static_cast<u32>(stride)))
	{
		ExecuteCommandListAndRestartRenderPass(false, "Uploading to vertex buffer");
		if (!m_vertex_stream_buffer.ReserveMemory(size, static_cast<u32>(stride)))
			pxFailRel("Failed to reserve space for vertices");
	}

	m_vertex.start = m_vertex_stream_buffer.GetCurrentOffset() / stride;
	m_vertex.count = count;
	SetVertexBuffer(m_vertex_stream_buffer.GetGPUPointer(), m_vertex_stream_buffer.GetSize(), stride);

	GSVector4i::storent(m_vertex_stream_buffer.GetCurrentHostPointer(), vertex, count * stride);
	m_vertex_stream_buffer.CommitMemory(size);
}

void GSDevice12::IASetIndexBuffer(const void* index, size_t count)
{
	const u32 size = sizeof(u16) * static_cast<u32>(count);
	if (!m_index_stream_buffer.ReserveMemory(size, sizeof(u16)))
	{
		ExecuteCommandListAndRestartRenderPass(false, "Uploading bytes to index buffer");
		if (!m_index_stream_buffer.ReserveMemory(size, sizeof(u16)))
			pxFailRel("Failed to reserve space for vertices");
	}

	m_index.start = m_index_stream_buffer.GetCurrentOffset() / sizeof(u16);
	m_index.count = count;
	SetIndexBuffer(m_index_stream_buffer.GetGPUPointer(), m_index_stream_buffer.GetSize(), DXGI_FORMAT_R16_UINT);

	std::memcpy(m_index_stream_buffer.GetCurrentHostPointer(), index, size);
	m_index_stream_buffer.CommitMemory(size);
}

void GSDevice12::OMSetRenderTargets(GSTexture* rt, GSTexture* ds, const GSVector4i& scissor)
{
	GSTexture12* vkRt = static_cast<GSTexture12*>(rt);
	GSTexture12* vkDs = static_cast<GSTexture12*>(ds);
	pxAssert(vkRt || vkDs);

	if (m_current_render_target != vkRt || m_current_depth_target != vkDs)
	{
		// framebuffer change
		EndRenderPass();
	}
	else if (InRenderPass())
	{
		// Framebuffer unchanged, but check for clears. Have to restart render pass, unlike Vulkan.
		// We'll take care of issuing the actual clear there, because we have to start one anyway.
		if (vkRt && vkRt->GetState() != GSTexture::State::Dirty)
		{
			if (vkRt->GetState() == GSTexture::State::Cleared)
				EndRenderPass();
			else
				vkRt->SetState(GSTexture::State::Dirty);
		}
		if (vkDs && vkDs->GetState() != GSTexture::State::Dirty)
		{
			if (vkDs->GetState() == GSTexture::State::Cleared)
				EndRenderPass();
			else
				vkDs->SetState(GSTexture::State::Dirty);
		}
	}

	m_current_render_target = vkRt;
	m_current_depth_target = vkDs;

	if (!InRenderPass())
	{
		if (vkRt)
			vkRt->TransitionToState(D3D12_RESOURCE_STATE_RENDER_TARGET);
		if (vkDs)
			vkDs->TransitionToState(D3D12_RESOURCE_STATE_DEPTH_WRITE);
	}

	// This is used to set/initialize the framebuffer for tfx rendering.
	const GSVector2i size = vkRt ? vkRt->GetSize() : vkDs->GetSize();
	const D3D12_VIEWPORT vp{0.0f, 0.0f, static_cast<float>(size.x), static_cast<float>(size.y), 0.0f, 1.0f};

	SetViewport(vp);
	SetScissor(scissor);
}

bool GSDevice12::GetSampler(D3D12DescriptorHandle* cpu_handle, GSHWDrawConfig::SamplerSelector ss)
{
	const auto it = m_samplers.find(ss.key);
	if (it != m_samplers.end())
	{
		*cpu_handle = it->second;
		return true;
	}

	D3D12_SAMPLER_DESC sd = {};
	const int anisotropy = GSConfig.MaxAnisotropy;
	if (anisotropy > 1 && ss.aniso)
	{
		sd.Filter = D3D12_FILTER_ANISOTROPIC;
	}
	else
	{
		static constexpr std::array<D3D12_FILTER, 8> filters = {{
			D3D12_FILTER_MIN_MAG_MIP_POINT, // 000 / min=point,mag=point,mip=point
			D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT, // 001 / min=linear,mag=point,mip=point
			D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT, // 010 / min=point,mag=linear,mip=point
			D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT, // 011 / min=linear,mag=linear,mip=point
			D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR, // 100 / min=point,mag=point,mip=linear
			D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR, // 101 / min=linear,mag=point,mip=linear
			D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR, // 110 / min=point,mag=linear,mip=linear
			D3D12_FILTER_MIN_MAG_MIP_LINEAR, // 111 / min=linear,mag=linear,mip=linear
		}};

		const u8 index = (static_cast<u8>(ss.IsMipFilterLinear()) << 2) |
		                 (static_cast<u8>(ss.IsMagFilterLinear()) << 1) | static_cast<u8>(ss.IsMinFilterLinear());
		sd.Filter = filters[index];
	}

	sd.AddressU = ss.tau ? D3D12_TEXTURE_ADDRESS_MODE_WRAP : D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sd.AddressV = ss.tav ? D3D12_TEXTURE_ADDRESS_MODE_WRAP : D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sd.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sd.MinLOD = 0.0f;
	sd.MaxLOD = (ss.lodclamp || !ss.UseMipmapFiltering()) ? 0.25f : FLT_MAX;
	sd.MaxAnisotropy = std::clamp<u8>(GSConfig.MaxAnisotropy, 1, 16);
	sd.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

	if (!GetSamplerHeapManager().Allocate(cpu_handle))
		return false;

	m_device.get()->CreateSampler(&sd, *cpu_handle);
	m_samplers.emplace(ss.key, *cpu_handle);
	return true;
}

void GSDevice12::ClearSamplerCache()
{
	ExecuteCommandList(false);
	for (const auto& it : m_samplers)
		m_sampler_heap_manager.Free(it.second.index);
	m_samplers.clear();
	InvalidateSamplerGroups();
	InitializeSamplers();

	m_utility_sampler_gpu = m_point_sampler_cpu;
	m_tfx_samplers_handle_gpu.Clear();
	m_dirty_flags |= DIRTY_FLAG_TFX_SAMPLERS;
}

bool GSDevice12::GetTextureGroupDescriptors(
	D3D12DescriptorHandle* gpu_handle, const D3D12DescriptorHandle* cpu_handles, u32 count)
{
	if (!GetDescriptorAllocator().Allocate(count, gpu_handle))
		return false;

	if (count == 1)
	{
		m_device.get()->CopyDescriptorsSimple(
			1, *gpu_handle, cpu_handles[0], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		return true;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE dst_handle = *gpu_handle;
	D3D12_CPU_DESCRIPTOR_HANDLE src_handles[NUM_TFX_TEXTURES];
	UINT src_sizes[NUM_TFX_TEXTURES];
	pxAssert(count <= NUM_TFX_TEXTURES);
	for (u32 i = 0; i < count; i++)
	{
		src_handles[i] = cpu_handles[i];
		src_sizes[i] = 1;
	}
	m_device.get()->CopyDescriptors(
		1, &dst_handle, &count, count, src_handles, src_sizes, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	return true;
}

static void AddUtilityVertexAttributes(D3D12::GraphicsPipelineBuilder& gpb)
{
	gpb.AddVertexAttribute("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0);
	gpb.AddVertexAttribute("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16);
	gpb.AddVertexAttribute("COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 28);
	gpb.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
}

GSDevice12::ComPtr<ID3DBlob> GSDevice12::GetUtilityVertexShader(const std::string& source, const char* entry_point)
{
	ShaderMacro sm_model;
	return m_shader_cache.GetVertexShader(source, sm_model.GetPtr(), entry_point);
}

GSDevice12::ComPtr<ID3DBlob> GSDevice12::GetUtilityPixelShader(const std::string& source, const char* entry_point)
{
	ShaderMacro sm_model;
	return m_shader_cache.GetPixelShader(source, sm_model.GetPtr(), entry_point);
}

bool GSDevice12::CreateNullTexture()
{
	m_null_texture =
		GSTexture12::Create(GSTexture::Type::Texture, GSTexture::Format::Color, 1, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN);
	if (!m_null_texture)
		return false;

	m_null_texture->TransitionToState(GetCommandList(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	D3D12::SetObjectName(m_null_texture->GetResource(), "Null texture");
	return true;
}

bool GSDevice12::CreateBuffers()
{
	if (!m_vertex_stream_buffer.Create(VERTEX_BUFFER_SIZE))
	{
		Host::ReportErrorAsync("GS", "Failed to allocate vertex buffer");
		return false;
	}

	if (!m_index_stream_buffer.Create(INDEX_BUFFER_SIZE))
	{
		Host::ReportErrorAsync("GS", "Failed to allocate index buffer");
		return false;
	}

	if (!m_vertex_constant_buffer.Create(VERTEX_UNIFORM_BUFFER_SIZE))
	{
		Host::ReportErrorAsync("GS", "Failed to allocate vertex uniform buffer");
		return false;
	}

	if (!m_pixel_constant_buffer.Create(FRAGMENT_UNIFORM_BUFFER_SIZE))
	{
		Host::ReportErrorAsync("GS", "Failed to allocate fragment uniform buffer");
		return false;
	}

	if (!m_texture_stream_buffer.Create(TEXTURE_UPLOAD_BUFFER_SIZE))
	{
		Host::ReportErrorAsync("GS", "Failed to allocate texture stream buffer");
		return false;
	}

	if (!AllocatePreinitializedGPUBuffer(EXPAND_BUFFER_SIZE, &m_expand_index_buffer,
			&m_expand_index_buffer_allocation, &GSDevice::GenerateExpansionIndexBuffer))
	{
		Host::ReportErrorAsync("GS", "Failed to allocate expansion index buffer");
		return false;
	}

	return true;
}

bool GSDevice12::CreateRootSignatures()
{
	D3D12::RootSignatureBuilder rsb;

	//////////////////////////////////////////////////////////////////////////
	// Convert Pipeline Layout
	//////////////////////////////////////////////////////////////////////////
	rsb.SetInputAssemblerFlag();
	rsb.Add32BitConstants(0, CONVERT_PUSH_CONSTANTS_SIZE / sizeof(u32), D3D12_SHADER_VISIBILITY_ALL);
	rsb.AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, NUM_UTILITY_SAMPLERS, D3D12_SHADER_VISIBILITY_PIXEL);
	rsb.AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 0, NUM_UTILITY_SAMPLERS, D3D12_SHADER_VISIBILITY_PIXEL);
	if (!(m_utility_root_signature = rsb.Create()))
		return false;
	D3D12::SetObjectName(m_utility_root_signature.get(), "Convert root signature");

	//////////////////////////////////////////////////////////////////////////
	// Draw/TFX Pipeline Layout
	//////////////////////////////////////////////////////////////////////////
	rsb.SetInputAssemblerFlag();
	rsb.AddCBVParameter(0, D3D12_SHADER_VISIBILITY_ALL);
	rsb.AddCBVParameter(1, D3D12_SHADER_VISIBILITY_PIXEL);
	rsb.AddSRVParameter(0, D3D12_SHADER_VISIBILITY_VERTEX);
	rsb.AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 2, D3D12_SHADER_VISIBILITY_PIXEL);
	rsb.AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 0, NUM_TFX_SAMPLERS, D3D12_SHADER_VISIBILITY_PIXEL);
	rsb.AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 2, D3D12_SHADER_VISIBILITY_PIXEL);
	if (!(m_tfx_root_signature = rsb.Create()))
		return false;
	D3D12::SetObjectName(m_tfx_root_signature.get(), "TFX root signature");
	return true;
}

bool GSDevice12::CompileConvertPipelines()
{
	std::optional<std::string> shader = ReadShaderSource("shaders/dx11/convert.fx");
	if (!shader)
	{
		Host::ReportErrorAsync("GS", "Failed to read shaders/dx11/convert.fx.");
		return false;
	}

	m_convert_vs = GetUtilityVertexShader(*shader, "vs_main");
	if (!m_convert_vs)
		return false;

	D3D12::GraphicsPipelineBuilder gpb;
	gpb.SetRootSignature(m_utility_root_signature.get());
	AddUtilityVertexAttributes(gpb);
	gpb.SetNoCullRasterizationState();
	gpb.SetNoBlendingState();
	gpb.SetVertexShader(m_convert_vs.get());

	for (ShaderConvert i = ShaderConvert::COPY; i < ShaderConvert::Count; i = static_cast<ShaderConvert>(static_cast<int>(i) + 1))
	{
		const bool depth = HasDepthOutput(i);
		const int index = static_cast<int>(i);

		switch (i)
		{
			case ShaderConvert::RGBA8_TO_16_BITS:
			case ShaderConvert::FLOAT32_TO_16_BITS:
			{
				gpb.SetRenderTarget(0, DXGI_FORMAT_R16_UINT);
				gpb.SetDepthStencilFormat(DXGI_FORMAT_UNKNOWN);
			}
			break;
			case ShaderConvert::FLOAT32_TO_32_BITS:
			{
				gpb.SetRenderTarget(0, DXGI_FORMAT_R32_UINT);
				gpb.SetDepthStencilFormat(DXGI_FORMAT_UNKNOWN);
			}
			break;
			case ShaderConvert::DATM_0:
			case ShaderConvert::DATM_1:
			case ShaderConvert::DATM_0_RTA_CORRECTION:
			case ShaderConvert::DATM_1_RTA_CORRECTION:
			{
				gpb.ClearRenderTargets();
				gpb.SetDepthStencilFormat(DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
			}
			break;
			default:
			{
				depth ? gpb.ClearRenderTargets() : gpb.SetRenderTarget(0, DXGI_FORMAT_R8G8B8A8_UNORM);
				gpb.SetDepthStencilFormat(depth ? DXGI_FORMAT_D32_FLOAT_S8X24_UINT : DXGI_FORMAT_UNKNOWN);
			}
			break;
		}

		if (IsDATMConvertShader(i))
		{
			const D3D12_DEPTH_STENCILOP_DESC sos = {
				D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_REPLACE, D3D12_COMPARISON_FUNC_ALWAYS};
			gpb.SetStencilState(true, 1, 1, sos, sos);
			gpb.SetDepthState(false, false, D3D12_COMPARISON_FUNC_ALWAYS);
		}
		else
		{
			gpb.SetDepthState(depth, depth, D3D12_COMPARISON_FUNC_ALWAYS);
			gpb.SetNoStencilState();
		}

		gpb.SetColorWriteMask(0, ShaderConvertWriteMask(i));

		ComPtr<ID3DBlob> ps(GetUtilityPixelShader(*shader, shaderName(i)));
		if (!ps)
			return false;

		gpb.SetPixelShader(ps.get());

		m_convert[index] = gpb.Create(m_device.get(), m_shader_cache, false);
		if (!m_convert[index])
			return false;

		D3D12::SetObjectName(m_convert[index].get(), TinyString::from_format("Convert pipeline {}", static_cast<int>(i)));

		if (i == ShaderConvert::COPY)
		{
			// compile color copy pipelines
			gpb.SetRenderTarget(0, DXGI_FORMAT_R8G8B8A8_UNORM);
			gpb.SetDepthStencilFormat(DXGI_FORMAT_UNKNOWN);
			for (u32 j = 0; j < 16; j++)
			{
				pxAssert(!m_color_copy[j]);
				gpb.SetBlendState(0, false, D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD, D3D12_BLEND_ONE,
					D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD, static_cast<u8>(j));
				m_color_copy[j] = gpb.Create(m_device.get(), m_shader_cache, false);
				if (!m_color_copy[j])
					return false;

				D3D12::SetObjectName(m_color_copy[j].get(), TinyString::from_format("Color copy pipeline (r={}, g={}, b={}, a={})",
					j & 1u, (j >> 1) & 1u, (j >> 2) & 1u, (j >> 3) & 1u));
			}
		}
		else if (i == ShaderConvert::RTA_CORRECTION)
		{
			// compile color copy pipelines
			gpb.SetRenderTarget(0, DXGI_FORMAT_R8G8B8A8_UNORM);
			gpb.SetDepthStencilFormat(DXGI_FORMAT_UNKNOWN);
			for (u32 j = 16; j < 32; j++)
			{
				pxAssert(!m_color_copy[j]);
				gpb.SetBlendState(0, false, D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD, D3D12_BLEND_ONE,
					D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD, static_cast<u8>(j - 16));
				m_color_copy[j] = gpb.Create(m_device.get(), m_shader_cache, false);
				if (!m_color_copy[j])
					return false;

				D3D12::SetObjectName(m_color_copy[j].get(), TinyString::from_format("Color copy pipeline (r={}, g={}, b={}, a={})",
																j & 1u, (j >> 1) & 1u, (j >> 2) & 1u, (j >> 3) & 1u));
			}
		}
		else if (i == ShaderConvert::COLCLIP_INIT || i == ShaderConvert::COLCLIP_RESOLVE)
		{
			const bool is_setup = i == ShaderConvert::COLCLIP_INIT;
			std::array<ComPtr<ID3D12PipelineState>, 2>& arr = is_setup ? m_colclip_setup_pipelines : m_colclip_finish_pipelines;
			for (u32 ds = 0; ds < 2; ds++)
			{
				pxAssert(!arr[ds]);

				gpb.SetRenderTarget(0, is_setup ? DXGI_FORMAT_R16G16B16A16_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM);
				gpb.SetDepthStencilFormat(ds ? DXGI_FORMAT_D32_FLOAT_S8X24_UINT : DXGI_FORMAT_UNKNOWN);
				arr[ds] = gpb.Create(m_device.get(), m_shader_cache, false);
				if (!arr[ds])
					return false;

				D3D12::SetObjectName(arr[ds].get(), TinyString::from_format("ColorClip {}/copy pipeline (ds={})", is_setup ? "setup" : "finish", ds));
			}
		}
	}

	for (u32 datm = 0; datm < 4; datm++)
	{
		const std::string entry_point(StringUtil::StdStringFromFormat("ps_stencil_image_init_%d", datm));
		ComPtr<ID3DBlob> ps(GetUtilityPixelShader(*shader, entry_point.c_str()));
		if (!ps)
			return false;

		gpb.SetRootSignature(m_utility_root_signature.get());
		gpb.SetRenderTarget(0, DXGI_FORMAT_R32_FLOAT);
		gpb.SetPixelShader(ps.get());
		gpb.SetNoDepthTestState();
		gpb.SetNoStencilState();
		gpb.SetBlendState(0, false, D3D12_BLEND_ONE, D3D12_BLEND_ONE, D3D12_BLEND_OP_ADD, D3D12_BLEND_ZERO,
			D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD, D3D12_COLOR_WRITE_ENABLE_RED);

		for (u32 ds = 0; ds < 2; ds++)
		{
			gpb.SetDepthStencilFormat(ds ? DXGI_FORMAT_D32_FLOAT_S8X24_UINT : DXGI_FORMAT_UNKNOWN);
			m_date_image_setup_pipelines[ds][datm] = gpb.Create(m_device.get(), m_shader_cache, false);
			if (!m_date_image_setup_pipelines[ds][datm])
				return false;

			D3D12::SetObjectName(m_date_image_setup_pipelines[ds][datm].get(),
				TinyString::from_format("DATE image clear pipeline (ds={}, datm={})", ds, (datm == 1 || datm == 3)));
		}
	}

	return true;
}

bool GSDevice12::CompilePresentPipelines()
{
	const std::optional<std::string> shader = ReadShaderSource("shaders/dx11/present.fx");
	if (!shader)
	{
		Host::ReportErrorAsync("GS", "Failed to read shaders/dx11/present.fx.");
		return false;
	}

	ComPtr<ID3DBlob> vs = GetUtilityVertexShader(*shader, "vs_main");
	if (!vs)
		return false;

	D3D12::GraphicsPipelineBuilder gpb;
	gpb.SetRootSignature(m_utility_root_signature.get());
	AddUtilityVertexAttributes(gpb);
	gpb.SetNoCullRasterizationState();
	gpb.SetNoBlendingState();
	gpb.SetVertexShader(vs.get());
	gpb.SetDepthState(false, false, D3D12_COMPARISON_FUNC_ALWAYS);
	gpb.SetNoStencilState();
	gpb.SetRenderTarget(0, DXGI_FORMAT_R8G8B8A8_UNORM);

	for (PresentShader i = PresentShader::COPY; i < PresentShader::Count; i = static_cast<PresentShader>(static_cast<int>(i) + 1))
	{
		const int index = static_cast<int>(i);

		ComPtr<ID3DBlob> ps(GetUtilityPixelShader(*shader, shaderName(i)));
		if (!ps)
			return false;

		gpb.SetPixelShader(ps.get());

		m_present[index] = gpb.Create(m_device.get(), m_shader_cache, false);
		if (!m_present[index])
			return false;

		D3D12::SetObjectName(m_present[index].get(), TinyString::from_format("Present pipeline {}", static_cast<int>(i)));
	}

	return true;
}

bool GSDevice12::CompileInterlacePipelines()
{
	const std::optional<std::string> source = ReadShaderSource("shaders/dx11/interlace.fx");
	if (!source)
	{
		Host::ReportErrorAsync("GS", "Failed to read shaders/dx11/interlace.fx.");
		return false;
	}

	D3D12::GraphicsPipelineBuilder gpb;
	AddUtilityVertexAttributes(gpb);
	gpb.SetRootSignature(m_utility_root_signature.get());
	gpb.SetNoCullRasterizationState();
	gpb.SetNoDepthTestState();
	gpb.SetNoBlendingState();
	gpb.SetRenderTarget(0, DXGI_FORMAT_R8G8B8A8_UNORM);
	gpb.SetVertexShader(m_convert_vs.get());

	for (int i = 0; i < static_cast<int>(m_interlace.size()); i++)
	{
		ComPtr<ID3DBlob> ps(GetUtilityPixelShader(*source, StringUtil::StdStringFromFormat("ps_main%d", i).c_str()));
		if (!ps)
			return false;

		gpb.SetPixelShader(ps.get());

		m_interlace[i] = gpb.Create(m_device.get(), m_shader_cache, false);
		if (!m_interlace[i])
			return false;

		D3D12::SetObjectName(m_interlace[i].get(), TinyString::from_format("Interlace pipeline {}", static_cast<int>(i)));
	}

	return true;
}

bool GSDevice12::CompileMergePipelines()
{
	const std::optional<std::string> shader = ReadShaderSource("shaders/dx11/merge.fx");
	if (!shader)
	{
		Host::ReportErrorAsync("GS", "Failed to read shaders/dx11/merge.fx.");
		return false;
	}

	D3D12::GraphicsPipelineBuilder gpb;
	AddUtilityVertexAttributes(gpb);
	gpb.SetRootSignature(m_utility_root_signature.get());
	gpb.SetNoCullRasterizationState();
	gpb.SetNoDepthTestState();
	gpb.SetRenderTarget(0, DXGI_FORMAT_R8G8B8A8_UNORM);
	gpb.SetVertexShader(m_convert_vs.get());

	for (int i = 0; i < static_cast<int>(m_merge.size()); i++)
	{
		ComPtr<ID3DBlob> ps(GetUtilityPixelShader(*shader, StringUtil::StdStringFromFormat("ps_main%d", i).c_str()));
		if (!ps)
			return false;

		gpb.SetPixelShader(ps.get());
		gpb.SetBlendState(0, true, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD);

		m_merge[i] = gpb.Create(m_device.get(), m_shader_cache, false);
		if (!m_merge[i])
			return false;

		D3D12::SetObjectName(m_merge[i].get(), TinyString::from_format("Merge pipeline {}", i));
	}

	return true;
}

bool GSDevice12::CompilePostProcessingPipelines()
{
	D3D12::GraphicsPipelineBuilder gpb;
	AddUtilityVertexAttributes(gpb);
	gpb.SetRootSignature(m_utility_root_signature.get());
	gpb.SetNoCullRasterizationState();
	gpb.SetNoDepthTestState();
	gpb.SetNoBlendingState();
	gpb.SetRenderTarget(0, DXGI_FORMAT_R8G8B8A8_UNORM);
	gpb.SetVertexShader(m_convert_vs.get());

	{
		const std::optional<std::string> shader = ReadShaderSource("shaders/common/fxaa.fx");
		if (!shader)
		{
			Host::ReportErrorAsync("GS", "Failed to read shaders/common/fxaa.fx.");
			return false;
		}

		ShaderMacro sm;
		sm.AddMacro("FXAA_HLSL", "1");
		ComPtr<ID3DBlob> ps = m_shader_cache.GetPixelShader(*shader, sm.GetPtr());
		if (!ps)
			return false;

		gpb.SetPixelShader(ps.get());

		m_fxaa_pipeline = gpb.Create(m_device.get(), m_shader_cache, false);
		if (!m_fxaa_pipeline)
			return false;

		D3D12::SetObjectName(m_fxaa_pipeline.get(), "FXAA pipeline");
	}

	{
		const std::optional<std::string> shader = ReadShaderSource("shaders/dx11/shadeboost.fx");
		if (!shader)
		{
			Host::ReportErrorAsync("GS", "Failed to read shaders/dx11/shadeboost.fx.");
			return false;
		}

		ComPtr<ID3DBlob> ps(GetUtilityPixelShader(*shader, "ps_main"));
		if (!ps)
			return false;

		gpb.SetPixelShader(ps.get());

		m_shadeboost_pipeline = gpb.Create(m_device.get(), m_shader_cache, false);
		if (!m_shadeboost_pipeline)
			return false;

		D3D12::SetObjectName(m_shadeboost_pipeline.get(), "Shadeboost pipeline");
	}

	return true;
}

void GSDevice12::DestroyResources()
{
	m_convert_vs.reset();

	m_cas_sharpen_pipeline.reset();
	m_cas_upscale_pipeline.reset();
	m_cas_root_signature.reset();

	m_tfx_pipelines.clear();
	m_tfx_pixel_shaders.clear();
	m_tfx_vertex_shaders.clear();
	m_interlace = {};
	m_merge = {};
	m_color_copy = {};
	m_present = {};
	m_convert = {};
	m_colclip_setup_pipelines = {};
	m_colclip_finish_pipelines = {};
	m_date_image_setup_pipelines = {};
	m_fxaa_pipeline.reset();
	m_shadeboost_pipeline.reset();
	m_imgui_pipeline.reset();

	for (const auto& it : m_samplers)
	{
		if (it.second)
			m_sampler_heap_manager.Free(it.second.index);
	}
	m_samplers.clear();
	InvalidateSamplerGroups();

	m_expand_index_buffer.reset();
	m_expand_index_buffer_allocation.reset();
	m_texture_stream_buffer.Destroy(false);
	m_pixel_constant_buffer.Destroy(false);
	m_vertex_constant_buffer.Destroy(false);
	m_index_stream_buffer.Destroy(false);
	m_vertex_stream_buffer.Destroy(false);

	m_utility_root_signature.reset();
	m_tfx_root_signature.reset();

	if (m_null_texture)
	{
		m_null_texture->Destroy(false);
		m_null_texture.reset();
	}

	m_shader_cache.Close();

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
	m_device.reset();
}

const ID3DBlob* GSDevice12::GetTFXVertexShader(GSHWDrawConfig::VSSelector sel)
{
	auto it = m_tfx_vertex_shaders.find(sel.key);
	if (it != m_tfx_vertex_shaders.end())
		return it->second.get();

	ShaderMacro sm;
	sm.AddMacro("VERTEX_SHADER", 1);
	sm.AddMacro("VS_TME", sel.tme);
	sm.AddMacro("VS_FST", sel.fst);
	sm.AddMacro("VS_IIP", sel.iip);
	sm.AddMacro("VS_EXPAND", static_cast<int>(sel.expand));

	const char* entry_point = (sel.expand != GSHWDrawConfig::VSExpand::None) ? "vs_main_expand" : "vs_main";
	ComPtr<ID3DBlob> vs(m_shader_cache.GetVertexShader(m_tfx_source, sm.GetPtr(), entry_point));
	it = m_tfx_vertex_shaders.emplace(sel.key, std::move(vs)).first;
	return it->second.get();
}

const ID3DBlob* GSDevice12::GetTFXPixelShader(const GSHWDrawConfig::PSSelector& sel)
{
	auto it = m_tfx_pixel_shaders.find(sel);
	if (it != m_tfx_pixel_shaders.end())
		return it->second.get();

	ShaderMacro sm;
	sm.AddMacro("PIXEL_SHADER", 1);
	sm.AddMacro("PS_FST", sel.fst);
	sm.AddMacro("PS_WMS", sel.wms);
	sm.AddMacro("PS_WMT", sel.wmt);
	sm.AddMacro("PS_ADJS", sel.adjs);
	sm.AddMacro("PS_ADJT", sel.adjt);
	sm.AddMacro("PS_AEM_FMT", sel.aem_fmt);
	sm.AddMacro("PS_AEM", sel.aem);
	sm.AddMacro("PS_TFX", sel.tfx);
	sm.AddMacro("PS_TCC", sel.tcc);
	sm.AddMacro("PS_DATE", sel.date);
	sm.AddMacro("PS_ATST", sel.atst);
	sm.AddMacro("PS_AFAIL", sel.afail);
	sm.AddMacro("PS_FOG", sel.fog);
	sm.AddMacro("PS_IIP", sel.iip);
	sm.AddMacro("PS_BLEND_HW", sel.blend_hw);
	sm.AddMacro("PS_A_MASKED", sel.a_masked);
	sm.AddMacro("PS_FBA", sel.fba);
	sm.AddMacro("PS_FBMASK", sel.fbmask);
	sm.AddMacro("PS_LTF", sel.ltf);
	sm.AddMacro("PS_TCOFFSETHACK", sel.tcoffsethack);
	sm.AddMacro("PS_POINT_SAMPLER", sel.point_sampler);
	sm.AddMacro("PS_REGION_RECT", sel.region_rect);
	sm.AddMacro("PS_SHUFFLE", sel.shuffle);
	sm.AddMacro("PS_SHUFFLE_SAME", sel.shuffle_same);
	sm.AddMacro("PS_PROCESS_BA", sel.process_ba);
	sm.AddMacro("PS_PROCESS_RG", sel.process_rg);
	sm.AddMacro("PS_SHUFFLE_ACROSS", sel.shuffle_across);
	sm.AddMacro("PS_READ16_SRC", sel.real16src);
	sm.AddMacro("PS_WRITE_RG", sel.write_rg);
	sm.AddMacro("PS_CHANNEL_FETCH", sel.channel);
	sm.AddMacro("PS_TALES_OF_ABYSS_HLE", sel.tales_of_abyss_hle);
	sm.AddMacro("PS_URBAN_CHAOS_HLE", sel.urban_chaos_hle);
	sm.AddMacro("PS_DST_FMT", sel.dst_fmt);
	sm.AddMacro("PS_DEPTH_FMT", sel.depth_fmt);
	sm.AddMacro("PS_PAL_FMT", sel.pal_fmt);
	sm.AddMacro("PS_COLCLIP_HW", sel.colclip_hw);
	sm.AddMacro("PS_RTA_CORRECTION", sel.rta_correction);
	sm.AddMacro("PS_RTA_SRC_CORRECTION", sel.rta_source_correction);
	sm.AddMacro("PS_COLCLIP", sel.colclip);
	sm.AddMacro("PS_BLEND_A", sel.blend_a);
	sm.AddMacro("PS_BLEND_B", sel.blend_b);
	sm.AddMacro("PS_BLEND_C", sel.blend_c);
	sm.AddMacro("PS_BLEND_D", sel.blend_d);
	sm.AddMacro("PS_BLEND_MIX", sel.blend_mix);
	sm.AddMacro("PS_ROUND_INV", sel.round_inv);
	sm.AddMacro("PS_FIXED_ONE_A", sel.fixed_one_a);
	sm.AddMacro("PS_PABE", sel.pabe);
	sm.AddMacro("PS_DITHER", sel.dither);
	sm.AddMacro("PS_DITHER_ADJUST", sel.dither_adjust);
	sm.AddMacro("PS_ZCLAMP", sel.zclamp);
	sm.AddMacro("PS_SCANMSK", sel.scanmsk);
	sm.AddMacro("PS_AUTOMATIC_LOD", sel.automatic_lod);
	sm.AddMacro("PS_MANUAL_LOD", sel.manual_lod);
	sm.AddMacro("PS_TEX_IS_FB", sel.tex_is_fb);
	sm.AddMacro("PS_NO_COLOR", sel.no_color);
	sm.AddMacro("PS_NO_COLOR1", sel.no_color1);

	ComPtr<ID3DBlob> ps(m_shader_cache.GetPixelShader(m_tfx_source, sm.GetPtr(), "ps_main"));
	it = m_tfx_pixel_shaders.emplace(sel, std::move(ps)).first;
	return it->second.get();
}

GSDevice12::ComPtr<ID3D12PipelineState> GSDevice12::CreateTFXPipeline(const PipelineSelector& p)
{
	static constexpr std::array<D3D12_PRIMITIVE_TOPOLOGY_TYPE, 3> topology_lookup = {{
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT, // Point
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE, // Line
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE, // Triangle
	}};

	GSHWDrawConfig::BlendState pbs{p.bs};
	GSHWDrawConfig::PSSelector pps{p.ps};
	if (!p.bs.IsEffective(p.cms))
	{
		// disable blending when colours are masked
		pbs = {};
		pps.no_color1 = true;
	}

	const ID3DBlob* vs = GetTFXVertexShader(p.vs);
	const ID3DBlob* ps = GetTFXPixelShader(pps);
	if (!vs || !ps)
		return nullptr;

	// Common state
	D3D12::GraphicsPipelineBuilder gpb;
	gpb.SetRootSignature(m_tfx_root_signature.get());
	gpb.SetPrimitiveTopologyType(topology_lookup[p.topology]);
	gpb.SetRasterizationState(D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_NONE, false);
	if (p.rt)
	{
		const GSTexture::Format format = IsDATEModePrimIDInit(p.ps.date) ?
		                                     GSTexture::Format::PrimID :
		                                     (p.ps.colclip_hw ? GSTexture::Format::ColorClip : GSTexture::Format::Color);

		DXGI_FORMAT native_format;
		LookupNativeFormat(format, nullptr, nullptr, &native_format, nullptr);
		gpb.SetRenderTarget(0, native_format);
	}
	if (p.ds)
		gpb.SetDepthStencilFormat(DXGI_FORMAT_D32_FLOAT_S8X24_UINT);

	// Shaders
	gpb.SetVertexShader(vs);
	gpb.SetPixelShader(ps);

	// IA
	if (p.vs.expand == GSHWDrawConfig::VSExpand::None)
	{
		gpb.AddVertexAttribute("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0);
		gpb.AddVertexAttribute("COLOR", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 8);
		gpb.AddVertexAttribute("TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT, 0, 12);
		gpb.AddVertexAttribute("POSITION", 0, DXGI_FORMAT_R16G16_UINT, 0, 16);
		gpb.AddVertexAttribute("POSITION", 1, DXGI_FORMAT_R32_UINT, 0, 20);
		gpb.AddVertexAttribute("TEXCOORD", 2, DXGI_FORMAT_R16G16_UINT, 0, 24);
		gpb.AddVertexAttribute("COLOR", 1, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 28);
	}

	// DepthStencil
	if (p.ds)
	{
		static const D3D12_COMPARISON_FUNC ztst[] = {D3D12_COMPARISON_FUNC_NEVER, D3D12_COMPARISON_FUNC_ALWAYS,
			D3D12_COMPARISON_FUNC_GREATER_EQUAL, D3D12_COMPARISON_FUNC_GREATER};
		gpb.SetDepthState((p.dss.ztst != ZTST_ALWAYS || p.dss.zwe), p.dss.zwe, ztst[p.dss.ztst]);
		if (p.dss.date)
		{
			const D3D12_DEPTH_STENCILOP_DESC sos{D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP,
				p.dss.date_one ? D3D12_STENCIL_OP_ZERO : D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_EQUAL};
			gpb.SetStencilState(true, 1, 1, sos, sos);
		}
	}
	else
	{
		gpb.SetNoDepthTestState();
	}

	// Blending
	if (IsDATEModePrimIDInit(p.ps.date))
	{
		// image DATE prepass
		gpb.SetBlendState(0, true, D3D12_BLEND_ONE, D3D12_BLEND_ONE, D3D12_BLEND_OP_MIN, D3D12_BLEND_ONE,
			D3D12_BLEND_ONE, D3D12_BLEND_OP_ADD, D3D12_COLOR_WRITE_ENABLE_RED);
	}
	else if (pbs.enable)
	{
		// clang-format off
		static constexpr std::array<D3D12_BLEND, 16> d3d_blend_factors = { {
			D3D12_BLEND_SRC_COLOR, D3D12_BLEND_INV_SRC_COLOR, D3D12_BLEND_DEST_COLOR, D3D12_BLEND_INV_DEST_COLOR,
			D3D12_BLEND_SRC1_COLOR, D3D12_BLEND_INV_SRC1_COLOR, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA,
			D3D12_BLEND_DEST_ALPHA, D3D12_BLEND_INV_DEST_ALPHA, D3D12_BLEND_SRC1_ALPHA, D3D12_BLEND_INV_SRC1_ALPHA,
			D3D12_BLEND_BLEND_FACTOR, D3D12_BLEND_INV_BLEND_FACTOR, D3D12_BLEND_ONE, D3D12_BLEND_ZERO
		} };
		static constexpr std::array<D3D12_BLEND_OP, 3> d3d_blend_ops = { {
			D3D12_BLEND_OP_ADD, D3D12_BLEND_OP_SUBTRACT, D3D12_BLEND_OP_REV_SUBTRACT
		} };
		// clang-format on

		gpb.SetBlendState(0, true, d3d_blend_factors[pbs.src_factor], d3d_blend_factors[pbs.dst_factor],
			d3d_blend_ops[pbs.op], d3d_blend_factors[pbs.src_factor_alpha], d3d_blend_factors[pbs.dst_factor_alpha],
			D3D12_BLEND_OP_ADD, p.cms.wrgba);
	}
	else
	{
		gpb.SetBlendState(0, false, D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD, D3D12_BLEND_ONE,
			D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD, p.cms.wrgba);
	}

	ComPtr<ID3D12PipelineState> pipeline(gpb.Create(m_device.get(), m_shader_cache));
	if (pipeline)
	{
		D3D12::SetObjectName(
			pipeline.get(), TinyString::from_format("TFX Pipeline {:08X}/{:08X}{:016X}", p.vs.key, p.ps.key_hi, p.ps.key_lo));
	}

	return pipeline;
}

const ID3D12PipelineState* GSDevice12::GetTFXPipeline(const PipelineSelector& p)
{
	auto it = m_tfx_pipelines.find(p);
	if (it != m_tfx_pipelines.end())
		return it->second.get();

	ComPtr<ID3D12PipelineState> pipeline(CreateTFXPipeline(p));
	it = m_tfx_pipelines.emplace(p, std::move(pipeline)).first;
	return it->second.get();
}

bool GSDevice12::BindDrawPipeline(const PipelineSelector& p)
{
	const ID3D12PipelineState* pipeline = GetTFXPipeline(p);
	if (!pipeline)
		return false;

	SetPipeline(pipeline);

	return ApplyTFXState();
}

void GSDevice12::InitializeState()
{
	for (u32 i = 0; i < NUM_TOTAL_TFX_TEXTURES; i++)
		m_tfx_textures[i] = m_null_texture->GetSRVDescriptor();
	m_tfx_sampler_sel = GSHWDrawConfig::SamplerSelector::Point().key;

	InvalidateCachedState();
}

void GSDevice12::InitializeSamplers()
{
	bool result = GetSampler(&m_point_sampler_cpu, GSHWDrawConfig::SamplerSelector::Point());
	result = result && GetSampler(&m_linear_sampler_cpu, GSHWDrawConfig::SamplerSelector::Linear());
	result = result && GetSampler(&m_tfx_sampler, m_tfx_sampler_sel);

	if (!result)
		pxFailRel("Failed to initialize samplers");
}

GSDevice12::WaitType GSDevice12::GetWaitType(bool wait, bool spin)
{
	if (!wait)
		return WaitType::None;
	if (spin)
		return WaitType::Spin;
	else
		return WaitType::Sleep;
}

void GSDevice12::ExecuteCommandList(bool wait_for_completion)
{
	EndRenderPass();
	ExecuteCommandList(GetWaitType(wait_for_completion, GSConfig.HWSpinCPUForReadbacks));
	InvalidateCachedState();
}

void GSDevice12::ExecuteCommandList(bool wait_for_completion, const char* reason, ...)
{
	std::va_list ap;
	va_start(ap, reason);
	const std::string reason_str(StringUtil::StdStringFromFormatV(reason, ap));
	va_end(ap);

	Console.Warning("D3D12: Executing command buffer due to '%s'", reason_str.c_str());
	ExecuteCommandList(wait_for_completion);
}

void GSDevice12::ExecuteCommandListAndRestartRenderPass(bool wait_for_completion, const char* reason)
{
	Console.Warning("D3D12: Executing command buffer due to '%s'", reason);

	const bool was_in_render_pass = m_in_render_pass;
	EndRenderPass();
	ExecuteCommandList(GetWaitType(wait_for_completion, GSConfig.HWSpinCPUForReadbacks));
	InvalidateCachedState();

	if (was_in_render_pass)
	{
		// rebind everything except RT, because the RP does that for us
		ApplyBaseState(m_dirty_flags & ~DIRTY_FLAG_RENDER_TARGET, GetCommandList());
		m_dirty_flags &= ~DIRTY_BASE_STATE;

		// restart render pass
		BeginRenderPass(m_current_render_target ? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE :
												  D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS,
			m_current_render_target ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE :
									  D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
			m_current_depth_target ? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE :
									 D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS,
			m_current_depth_target ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE :
									 D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS);
	}
}

void GSDevice12::ExecuteCommandListForReadback()
{
	ExecuteCommandList(true);
}

void GSDevice12::InvalidateCachedState()
{
	m_dirty_flags |= DIRTY_BASE_STATE | DIRTY_TFX_STATE | DIRTY_UTILITY_STATE | DIRTY_CONSTANT_BUFFER_STATE;
	m_current_root_signature = RootSignature::Undefined;
	m_utility_texture_cpu.Clear();
	m_utility_texture_gpu.Clear();
	m_utility_sampler_cpu.Clear();
	m_utility_sampler_gpu.Clear();
	m_tfx_textures_handle_gpu.Clear();
	m_tfx_samplers_handle_gpu.Clear();
	m_tfx_rt_textures_handle_gpu.Clear();
}

void GSDevice12::SetVertexBuffer(D3D12_GPU_VIRTUAL_ADDRESS buffer, size_t size, size_t stride)
{
	if (m_vertex_buffer.BufferLocation == buffer && m_vertex_buffer.SizeInBytes == size &&
		m_vertex_buffer.StrideInBytes == stride)
		return;

	m_vertex_buffer.BufferLocation = buffer;
	m_vertex_buffer.SizeInBytes = size;
	m_vertex_buffer.StrideInBytes = stride;
	m_dirty_flags |= DIRTY_FLAG_VERTEX_BUFFER;
}

void GSDevice12::SetIndexBuffer(D3D12_GPU_VIRTUAL_ADDRESS buffer, size_t size, DXGI_FORMAT type)
{
	if (m_index_buffer.BufferLocation == buffer && m_index_buffer.SizeInBytes == size && m_index_buffer.Format == type)
		return;

	m_index_buffer.BufferLocation = buffer;
	m_index_buffer.SizeInBytes = size;
	m_index_buffer.Format = type;
	m_dirty_flags |= DIRTY_FLAG_INDEX_BUFFER;
}

void GSDevice12::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topology)
{
	if (m_primitive_topology == topology)
		return;

	m_primitive_topology = topology;
	m_dirty_flags |= DIRTY_FLAG_PRIMITIVE_TOPOLOGY;
}

void GSDevice12::SetBlendConstants(u8 color)
{
	if (m_blend_constant_color == color)
		return;

	m_blend_constant_color = color;
	m_dirty_flags |= DIRTY_FLAG_BLEND_CONSTANTS;
}

void GSDevice12::SetStencilRef(u8 ref)
{
	if (m_stencil_ref == ref)
		return;

	m_stencil_ref = ref;
	m_dirty_flags |= DIRTY_FLAG_STENCIL_REF;
}

void GSDevice12::PSSetShaderResource(int i, GSTexture* sr, bool check_state)
{
	D3D12DescriptorHandle handle;
	if (sr)
	{
		GSTexture12* dtex = static_cast<GSTexture12*>(sr);
		if (check_state)
		{
			if (dtex->GetResourceState() != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE && InRenderPass())
			{
				GL_INS("Ending render pass due to resource transition");
				EndRenderPass();
			}

			dtex->CommitClear();
			dtex->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
		dtex->SetUseFenceCounter(GetCurrentFenceValue());
		handle = dtex->GetSRVDescriptor();
	}
	else
	{
		handle = m_null_texture->GetSRVDescriptor();
	}

	if (m_tfx_textures[i] == handle)
		return;

	m_tfx_textures[i] = handle;
	m_dirty_flags |= (i < 2) ? DIRTY_FLAG_TFX_TEXTURES : DIRTY_FLAG_TFX_RT_TEXTURES;
}

void GSDevice12::PSSetSampler(GSHWDrawConfig::SamplerSelector sel)
{
	if (m_tfx_sampler_sel == sel.key)
		return;

	GetSampler(&m_tfx_sampler, sel);
	m_tfx_sampler_sel = sel.key;
	m_dirty_flags |= DIRTY_FLAG_TFX_SAMPLERS;
}

void GSDevice12::SetUtilityRootSignature()
{
	if (m_current_root_signature == RootSignature::Utility)
		return;

	m_current_root_signature = RootSignature::Utility;
	m_dirty_flags |= DIRTY_FLAG_TEXTURES_DESCRIPTOR_TABLE | DIRTY_FLAG_SAMPLERS_DESCRIPTOR_TABLE | DIRTY_FLAG_PIPELINE;
	GetCommandList()->SetGraphicsRootSignature(m_utility_root_signature.get());
}

void GSDevice12::SetUtilityTexture(GSTexture* dtex, const D3D12DescriptorHandle& sampler)
{
	D3D12DescriptorHandle handle;
	if (dtex)
	{
		GSTexture12* d12tex = static_cast<GSTexture12*>(dtex);
		d12tex->CommitClear();
		d12tex->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		d12tex->SetUseFenceCounter(GetCurrentFenceValue());
		handle = d12tex->GetSRVDescriptor();
	}
	else
	{
		handle = m_null_texture->GetSRVDescriptor();
	}

	if (m_utility_texture_cpu != handle)
	{
		m_utility_texture_cpu = handle;
		m_dirty_flags |= DIRTY_FLAG_TEXTURES_DESCRIPTOR_TABLE;

		if (!GetTextureGroupDescriptors(&m_utility_texture_gpu, &handle, 1))
		{
			ExecuteCommandListAndRestartRenderPass(false, "Ran out of utility texture descriptors");
			SetUtilityTexture(dtex, sampler);
			return;
		}
	}

	if (m_utility_sampler_cpu != sampler)
	{
		m_utility_sampler_cpu = sampler;
		m_dirty_flags |= DIRTY_FLAG_SAMPLERS_DESCRIPTOR_TABLE;

		if (!GetSamplerAllocator().LookupSingle(&m_utility_sampler_gpu, sampler))
		{
			ExecuteCommandListAndRestartRenderPass(false, "Ran out of utility sampler descriptors");
			SetUtilityTexture(dtex, sampler);
			return;
		}
	}
}

void GSDevice12::SetUtilityPushConstants(const void* data, u32 size)
{
	GetCommandList()->SetGraphicsRoot32BitConstants(
		UTILITY_ROOT_SIGNATURE_PARAM_PUSH_CONSTANTS, (size + 3) / sizeof(u32), data, 0);
}

void GSDevice12::UnbindTexture(GSTexture12* tex)
{
	for (u32 i = 0; i < NUM_TOTAL_TFX_TEXTURES; i++)
	{
		if (m_tfx_textures[i] == tex->GetSRVDescriptor())
		{
			m_tfx_textures[i] = m_null_texture->GetSRVDescriptor();
			m_dirty_flags |= DIRTY_FLAG_TFX_TEXTURES;
		}
	}
	if (m_current_render_target == tex)
	{
		EndRenderPass();
		m_current_render_target = nullptr;
	}
	if (m_current_depth_target == tex)
	{
		EndRenderPass();
		m_current_depth_target = nullptr;
	}
}

void GSDevice12::RenderTextureMipmap(
	GSTexture12* texture, u32 dst_level, u32 dst_width, u32 dst_height, u32 src_level, u32 src_width, u32 src_height)
{
	EndRenderPass();

	// we need a temporary SRV and RTV for each mip level
	// Safe to use the init buffer after exec, because everything will be done with the texture.
	D3D12DescriptorHandle rtv_handle;
	while (!GetRTVHeapManager().Allocate(&rtv_handle))
		ExecuteCommandList(false);

	D3D12DescriptorHandle srv_handle;
	while (!GetDescriptorHeapManager().Allocate(&srv_handle))
		ExecuteCommandList(false);

	// Setup views. This will be a partial view for the SRV.
	D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {texture->GetDXGIFormat(), D3D12_RTV_DIMENSION_TEXTURE2D};
	rtv_desc.Texture2D = {dst_level, 0u};
	m_device.get()->CreateRenderTargetView(texture->GetResource(), &rtv_desc, rtv_handle);

	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {
		texture->GetDXGIFormat(), D3D12_SRV_DIMENSION_TEXTURE2D, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING};
	srv_desc.Texture2D = {src_level, 1u, 0u, 0.0f};
	m_device.get()->CreateShaderResourceView(texture->GetResource(), &srv_desc, srv_handle);

	// We need to set the descriptors up manually, because we're not going through GSTexture.
	if (!GetTextureGroupDescriptors(&m_utility_texture_gpu, &srv_handle, 1))
		ExecuteCommandList(false);
	if (m_utility_sampler_cpu != m_linear_sampler_cpu)
	{
		m_dirty_flags |= DIRTY_FLAG_SAMPLERS_DESCRIPTOR_TABLE;
		if (!GetSamplerAllocator().LookupSingle(&m_utility_sampler_gpu, m_linear_sampler_cpu))
			ExecuteCommandList(false);
	}

	// *now* we don't have to worry about running out of anything.
	ID3D12GraphicsCommandList* cmdlist = GetCommandList();
	if (texture->GetResourceState() != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		texture->TransitionSubresourceToState(
			cmdlist, src_level, texture->GetResourceState(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	if (texture->GetResourceState() != D3D12_RESOURCE_STATE_RENDER_TARGET)
		texture->TransitionSubresourceToState(
			cmdlist, dst_level, texture->GetResourceState(), D3D12_RESOURCE_STATE_RENDER_TARGET);

	// We set the state directly here.
	constexpr u32 MODIFIED_STATE = DIRTY_FLAG_VIEWPORT | DIRTY_FLAG_SCISSOR | DIRTY_FLAG_RENDER_TARGET;
	m_dirty_flags &= ~MODIFIED_STATE;

	// Using a render pass is probably a bit overkill.
	const D3D12_DISCARD_REGION discard_region = {0u, nullptr, dst_level, 1u};
	cmdlist->DiscardResource(texture->GetResource(), &discard_region);
	cmdlist->OMSetRenderTargets(1, &rtv_handle.cpu_handle, FALSE, nullptr);

	const D3D12_VIEWPORT vp = {0.0f, 0.0f, static_cast<float>(dst_width), static_cast<float>(dst_height), 0.0f, 1.0f};
	cmdlist->RSSetViewports(1, &vp);

	const D3D12_RECT scissor = {0, 0, static_cast<LONG>(dst_width), static_cast<LONG>(dst_height)};
	cmdlist->RSSetScissorRects(1, &scissor);

	SetUtilityRootSignature();
	SetPipeline(m_convert[static_cast<int>(ShaderConvert::COPY)].get());
	DrawStretchRect(GSVector4(0.0f, 0.0f, 1.0f, 1.0f),
		GSVector4(0.0f, 0.0f, static_cast<float>(dst_width), static_cast<float>(dst_height)),
		GSVector2i(dst_width, dst_height));

	if (texture->GetResourceState() != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		texture->TransitionSubresourceToState(
			cmdlist, src_level, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, texture->GetResourceState());
	if (texture->GetResourceState() != D3D12_RESOURCE_STATE_RENDER_TARGET)
		texture->TransitionSubresourceToState(
			cmdlist, dst_level, D3D12_RESOURCE_STATE_RENDER_TARGET, texture->GetResourceState());

	// Must destroy after current cmdlist.
	DeferDescriptorDestruction(m_descriptor_heap_manager, &srv_handle);
	DeferDescriptorDestruction(m_rtv_heap_manager, &rtv_handle);

	// Restore for next normal draw.
	m_dirty_flags |= MODIFIED_STATE;
}

bool GSDevice12::InRenderPass()
{
	return m_in_render_pass;
}

void GSDevice12::BeginRenderPass(D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE color_begin,
	D3D12_RENDER_PASS_ENDING_ACCESS_TYPE color_end, D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE depth_begin,
	D3D12_RENDER_PASS_ENDING_ACCESS_TYPE depth_end, D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE stencil_begin,
	D3D12_RENDER_PASS_ENDING_ACCESS_TYPE stencil_end, GSVector4 clear_color, float clear_depth, u8 clear_stencil)
{
	if (m_in_render_pass)
		EndRenderPass();

	// we're setting the RT here.
	m_dirty_flags &= ~DIRTY_FLAG_RENDER_TARGET;
	m_in_render_pass = true;

	if (stencil_end == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD)
		GL_INS("D3D12: BeginRenderPass() end stencil is DISCARDED.");

	D3D12_RENDER_PASS_RENDER_TARGET_DESC rt = {};
	if (m_current_render_target)
	{
		rt.cpuDescriptor = m_current_render_target->GetWriteDescriptor();
		rt.EndingAccess.Type = color_end;
		rt.BeginningAccess.Type = color_begin;
		if (color_begin == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR)
		{
			LookupNativeFormat(m_current_render_target->GetFormat(), nullptr,
				&rt.BeginningAccess.Clear.ClearValue.Format, nullptr, nullptr);
			GSVector4::store<false>(rt.BeginningAccess.Clear.ClearValue.Color, clear_color);
		}
	}

	D3D12_RENDER_PASS_DEPTH_STENCIL_DESC ds = {};
	if (m_current_depth_target)
	{
		ds.cpuDescriptor = m_current_depth_target->GetWriteDescriptor();
		ds.DepthEndingAccess.Type = depth_end;
		ds.DepthBeginningAccess.Type = depth_begin;
		if (depth_begin == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR)
		{
			LookupNativeFormat(m_current_depth_target->GetFormat(), nullptr, nullptr, nullptr,
				&ds.DepthBeginningAccess.Clear.ClearValue.Format);
			ds.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth = clear_depth;
		}
		ds.StencilEndingAccess.Type = stencil_end;
		ds.StencilBeginningAccess.Type = stencil_begin;
		if (stencil_begin == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR)
		{
			LookupNativeFormat(m_current_depth_target->GetFormat(), nullptr, nullptr, nullptr,
				&ds.StencilBeginningAccess.Clear.ClearValue.Format);
			ds.StencilBeginningAccess.Clear.ClearValue.DepthStencil.Stencil = clear_stencil;
		}
	}

	GetCommandList()->BeginRenderPass(m_current_render_target ? 1 : 0,
		m_current_render_target ? &rt : nullptr, m_current_depth_target ? &ds : nullptr, D3D12_RENDER_PASS_FLAG_NONE);
}

void GSDevice12::EndRenderPass()
{
	if (!m_in_render_pass)
		return;

	m_in_render_pass = false;

	// to render again, we need to reset OM
	m_dirty_flags |= DIRTY_FLAG_RENDER_TARGET;

	g_perfmon.Put(GSPerfMon::RenderPasses, 1);

	GetCommandList()->EndRenderPass();
}

void GSDevice12::SetViewport(const D3D12_VIEWPORT& viewport)
{
	if (std::memcmp(&viewport, &m_viewport, sizeof(m_viewport)) == 0)
		return;

	std::memcpy(&m_viewport, &viewport, sizeof(m_viewport));
	m_dirty_flags |= DIRTY_FLAG_VIEWPORT;
}

void GSDevice12::SetScissor(const GSVector4i& scissor)
{
	if (m_scissor.eq(scissor))
		return;

	m_scissor = scissor;
	m_dirty_flags |= DIRTY_FLAG_SCISSOR;
}

void GSDevice12::SetPipeline(const ID3D12PipelineState* pipeline)
{
	if (m_current_pipeline == pipeline)
		return;

	m_current_pipeline = pipeline;
	m_dirty_flags |= DIRTY_FLAG_PIPELINE;
}

__ri void GSDevice12::ApplyBaseState(u32 flags, ID3D12GraphicsCommandList* cmdlist)
{
	if (flags & DIRTY_FLAG_VERTEX_BUFFER)
		cmdlist->IASetVertexBuffers(0, 1, &m_vertex_buffer);

	if (flags & DIRTY_FLAG_INDEX_BUFFER)
		cmdlist->IASetIndexBuffer(&m_index_buffer);

	if (flags & DIRTY_FLAG_PRIMITIVE_TOPOLOGY)
		cmdlist->IASetPrimitiveTopology(m_primitive_topology);

	if (flags & DIRTY_FLAG_PIPELINE)
		cmdlist->SetPipelineState(const_cast<ID3D12PipelineState*>(m_current_pipeline));

	if (flags & DIRTY_FLAG_VIEWPORT)
		cmdlist->RSSetViewports(1, &m_viewport);

	if (flags & DIRTY_FLAG_SCISSOR)
	{
		const D3D12_RECT rc{m_scissor.x, m_scissor.y, m_scissor.z, m_scissor.w};
		cmdlist->RSSetScissorRects(1, &rc);
	}

	if (flags & DIRTY_FLAG_BLEND_CONSTANTS)
	{
		const GSVector4 col(static_cast<float>(m_blend_constant_color) / 128.0f);
		cmdlist->OMSetBlendFactor(col.v);
	}

	if (flags & DIRTY_FLAG_STENCIL_REF)
		cmdlist->OMSetStencilRef(m_stencil_ref);

	if (flags & DIRTY_FLAG_RENDER_TARGET)
	{
		if (m_current_render_target)
		{
			cmdlist->OMSetRenderTargets(1, &m_current_render_target->GetWriteDescriptor().cpu_handle, FALSE,
				m_current_depth_target ? &m_current_depth_target->GetWriteDescriptor().cpu_handle : nullptr);
		}
		else if (m_current_depth_target)
		{
			cmdlist->OMSetRenderTargets(0, nullptr, FALSE, &m_current_depth_target->GetWriteDescriptor().cpu_handle);
		}
	}
}

bool GSDevice12::ApplyTFXState(bool already_execed)
{
	if (m_current_root_signature == RootSignature::TFX && m_dirty_flags == 0)
		return true;

	u32 flags = m_dirty_flags;
	m_dirty_flags &= ~(DIRTY_TFX_STATE | DIRTY_CONSTANT_BUFFER_STATE);

	// do cbuffer first, because it's the most likely to cause an exec
	if (flags & DIRTY_FLAG_VS_CONSTANT_BUFFER)
	{
		if (!m_vertex_constant_buffer.ReserveMemory(
				sizeof(m_vs_cb_cache), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT))
		{
			if (already_execed)
			{
				Console.Error("D3D12: Failed to reserve vertex uniform space");
				return false;
			}

			ExecuteCommandListAndRestartRenderPass(false, "Ran out of vertex uniform space");
			return ApplyTFXState(true);
		}

		std::memcpy(m_vertex_constant_buffer.GetCurrentHostPointer(), &m_vs_cb_cache, sizeof(m_vs_cb_cache));
		m_tfx_constant_buffers[0] = m_vertex_constant_buffer.GetCurrentGPUPointer();
		m_vertex_constant_buffer.CommitMemory(sizeof(m_vs_cb_cache));
		flags |= DIRTY_FLAG_VS_CONSTANT_BUFFER_BINDING;
	}

	if (flags & DIRTY_FLAG_PS_CONSTANT_BUFFER)
	{
		if (!m_pixel_constant_buffer.ReserveMemory(
				sizeof(m_ps_cb_cache), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT))
		{
			if (already_execed)
			{
				Console.Error("D3D12: Failed to reserve pixel uniform space");
				return false;
			}

			ExecuteCommandListAndRestartRenderPass(false, "Ran out of pixel uniform space");
			return ApplyTFXState(true);
		}

		std::memcpy(m_pixel_constant_buffer.GetCurrentHostPointer(), &m_ps_cb_cache, sizeof(m_ps_cb_cache));
		m_tfx_constant_buffers[1] = m_pixel_constant_buffer.GetCurrentGPUPointer();
		m_pixel_constant_buffer.CommitMemory(sizeof(m_ps_cb_cache));
		flags |= DIRTY_FLAG_PS_CONSTANT_BUFFER_BINDING;
	}

	if (flags & DIRTY_FLAG_TFX_SAMPLERS)
	{
		if (!GetSamplerAllocator().LookupSingle(&m_tfx_samplers_handle_gpu, m_tfx_sampler))
		{
			ExecuteCommandListAndRestartRenderPass(false, "Ran out of sampler groups");
			return ApplyTFXState(true);
		}

		flags |= DIRTY_FLAG_SAMPLERS_DESCRIPTOR_TABLE;
	}

	if (flags & DIRTY_FLAG_TFX_TEXTURES)
	{
		if (!GetTextureGroupDescriptors(&m_tfx_textures_handle_gpu, m_tfx_textures.data(), 2))
		{
			ExecuteCommandListAndRestartRenderPass(false, "Ran out of TFX texture descriptor groups");
			return ApplyTFXState(true);
		}

		flags |= DIRTY_FLAG_TEXTURES_DESCRIPTOR_TABLE;
	}

	if (flags & DIRTY_FLAG_TFX_RT_TEXTURES)
	{
		if (!GetTextureGroupDescriptors(&m_tfx_rt_textures_handle_gpu, m_tfx_textures.data() + 2, 2))
		{
			ExecuteCommandListAndRestartRenderPass(false, "Ran out of TFX RT descriptor descriptor groups");
			return ApplyTFXState(true);
		}

		flags |= DIRTY_FLAG_TEXTURES_DESCRIPTOR_TABLE_2;
	}

	ID3D12GraphicsCommandList* cmdlist = GetCommandList();

	if (m_current_root_signature != RootSignature::TFX)
	{
		m_current_root_signature = RootSignature::TFX;
		flags |= DIRTY_FLAG_VS_CONSTANT_BUFFER_BINDING | DIRTY_FLAG_PS_CONSTANT_BUFFER_BINDING |
		         DIRTY_FLAG_TEXTURES_DESCRIPTOR_TABLE | DIRTY_FLAG_SAMPLERS_DESCRIPTOR_TABLE |
		         DIRTY_FLAG_TEXTURES_DESCRIPTOR_TABLE_2 | DIRTY_FLAG_PIPELINE;
		cmdlist->SetGraphicsRootSignature(m_tfx_root_signature.get());
	}

	if (flags & DIRTY_FLAG_VS_CONSTANT_BUFFER_BINDING)
		cmdlist->SetGraphicsRootConstantBufferView(TFX_ROOT_SIGNATURE_PARAM_VS_CBV, m_tfx_constant_buffers[0]);
	if (flags & DIRTY_FLAG_PS_CONSTANT_BUFFER_BINDING)
		cmdlist->SetGraphicsRootConstantBufferView(TFX_ROOT_SIGNATURE_PARAM_PS_CBV, m_tfx_constant_buffers[1]);
	if (flags & DIRTY_FLAG_VS_VERTEX_BUFFER_BINDING)
	{
		cmdlist->SetGraphicsRootShaderResourceView(TFX_ROOT_SIGNATURE_PARAM_VS_SRV,
			m_vertex_stream_buffer.GetGPUPointer() + m_vertex.start * sizeof(GSVertex));
	}
	if (flags & DIRTY_FLAG_TEXTURES_DESCRIPTOR_TABLE)
		cmdlist->SetGraphicsRootDescriptorTable(TFX_ROOT_SIGNATURE_PARAM_PS_TEXTURES, m_tfx_textures_handle_gpu);
	if (flags & DIRTY_FLAG_SAMPLERS_DESCRIPTOR_TABLE)
		cmdlist->SetGraphicsRootDescriptorTable(TFX_ROOT_SIGNATURE_PARAM_PS_SAMPLERS, m_tfx_samplers_handle_gpu);
	if (flags & DIRTY_FLAG_TEXTURES_DESCRIPTOR_TABLE_2)
		cmdlist->SetGraphicsRootDescriptorTable(TFX_ROOT_SIGNATURE_PARAM_PS_RT_TEXTURES, m_tfx_rt_textures_handle_gpu);

	ApplyBaseState(flags, cmdlist);
	return true;
}

bool GSDevice12::ApplyUtilityState(bool already_execed)
{
	if (m_current_root_signature == RootSignature::Utility && m_dirty_flags == 0)
		return true;

	u32 flags = m_dirty_flags;
	m_dirty_flags &= ~DIRTY_UTILITY_STATE;

	ID3D12GraphicsCommandList* cmdlist = GetCommandList();

	if (m_current_root_signature != RootSignature::Utility)
	{
		m_current_root_signature = RootSignature::Utility;
		flags |= DIRTY_FLAG_TEXTURES_DESCRIPTOR_TABLE | DIRTY_FLAG_SAMPLERS_DESCRIPTOR_TABLE | DIRTY_FLAG_PIPELINE;
		cmdlist->SetGraphicsRootSignature(m_utility_root_signature.get());
	}

	if (flags & DIRTY_FLAG_TEXTURES_DESCRIPTOR_TABLE)
		cmdlist->SetGraphicsRootDescriptorTable(UTILITY_ROOT_SIGNATURE_PARAM_PS_TEXTURES, m_utility_texture_gpu);
	if (flags & DIRTY_FLAG_SAMPLERS_DESCRIPTOR_TABLE)
		cmdlist->SetGraphicsRootDescriptorTable(UTILITY_ROOT_SIGNATURE_PARAM_PS_SAMPLERS, m_utility_sampler_gpu);

	ApplyBaseState(flags, cmdlist);
	return true;
}

void GSDevice12::SetVSConstantBuffer(const GSHWDrawConfig::VSConstantBuffer& cb)
{
	if (m_vs_cb_cache.Update(cb))
		m_dirty_flags |= DIRTY_FLAG_VS_CONSTANT_BUFFER;
}

void GSDevice12::SetPSConstantBuffer(const GSHWDrawConfig::PSConstantBuffer& cb)
{
	if (m_ps_cb_cache.Update(cb))
		m_dirty_flags |= DIRTY_FLAG_PS_CONSTANT_BUFFER;
}

void GSDevice12::SetupDATE(GSTexture* rt, GSTexture* ds, SetDATM datm, const GSVector4i& bbox)
{
	GL_PUSH("SetupDATE {%d,%d} %dx%d", bbox.left, bbox.top, bbox.width(), bbox.height());

	const GSVector2i size(ds->GetSize());
	const GSVector4 src = GSVector4(bbox) / GSVector4(size).xyxy();
	const GSVector4 dst = src * 2.0f - 1.0f;
	const GSVertexPT1 vertices[] = {
		{GSVector4(dst.x, -dst.y, 0.5f, 1.0f), GSVector2(src.x, src.y)},
		{GSVector4(dst.z, -dst.y, 0.5f, 1.0f), GSVector2(src.z, src.y)},
		{GSVector4(dst.x, -dst.w, 0.5f, 1.0f), GSVector2(src.x, src.w)},
		{GSVector4(dst.z, -dst.w, 0.5f, 1.0f), GSVector2(src.z, src.w)},
	};

	// sfex3 (after the capcom logo), vf4 (first menu fading in), ffxii shadows, rumble roses shadows, persona4 shadows
	EndRenderPass();
	SetUtilityTexture(rt, m_point_sampler_cpu);
	OMSetRenderTargets(nullptr, ds, bbox);
	IASetVertexBuffer(vertices, sizeof(vertices[0]), 4);
	SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	SetPipeline(m_convert[SetDATMShader(datm)].get());
	SetStencilRef(1);
	BeginRenderPass(D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
		D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE,
		D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE,
		GSVector4::zero(), 0.0f, 0);
	if (ApplyUtilityState())
		DrawPrimitive();

	EndRenderPass();
}

GSTexture12* GSDevice12::SetupPrimitiveTrackingDATE(GSHWDrawConfig& config, PipelineSelector& pipe)
{
	// How this is done:
	// - can't put a barrier for the image in the middle of the normal render pass, so that's out
	// - so, instead of just filling the int texture with INT_MAX, we sample the RT and use -1 for failing values
	// - then, instead of sampling the RT with DATE=1/2, we just do a min() without it, the -1 gets preserved
	// - then, the DATE=3 draw is done as normal
	GL_INS("Setup DATE Primitive ID Image for {%d,%d}-{%d,%d}", config.drawarea.left, config.drawarea.top,
		config.drawarea.right, config.drawarea.bottom);

	const GSVector2i rtsize(config.rt->GetSize());
	GSTexture12* image =
		static_cast<GSTexture12*>(CreateRenderTarget(rtsize.x, rtsize.y, GSTexture::Format::PrimID, false));
	if (!image)
		return nullptr;

	EndRenderPass();

	// setup the fill quad to prefill with existing alpha values
	SetUtilityTexture(config.rt, m_point_sampler_cpu);
	OMSetRenderTargets(image, config.ds, config.drawarea);

	// if the depth target has been cleared, we need to preserve that clear
	BeginRenderPass(D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE,
		config.ds ? GetLoadOpForTexture(static_cast<GSTexture12*>(config.ds)) :
					D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS,
		config.ds ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
		D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
		GSVector4::zero(), config.ds ? config.ds->GetClearDepth() : 0.0f);

	// draw the quad to prefill the image
	const GSVector4 src = GSVector4(config.drawarea) / GSVector4(rtsize).xyxy();
	const GSVector4 dst = src * 2.0f - 1.0f;
	const GSVertexPT1 vertices[] = {
		{GSVector4(dst.x, -dst.y, 0.5f, 1.0f), GSVector2(src.x, src.y)},
		{GSVector4(dst.z, -dst.y, 0.5f, 1.0f), GSVector2(src.z, src.y)},
		{GSVector4(dst.x, -dst.w, 0.5f, 1.0f), GSVector2(src.x, src.w)},
		{GSVector4(dst.z, -dst.w, 0.5f, 1.0f), GSVector2(src.z, src.w)},
	};
	SetUtilityRootSignature();
	SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	SetPipeline(m_date_image_setup_pipelines[pipe.ds][static_cast<u8>(config.datm)].get());
	IASetVertexBuffer(vertices, sizeof(vertices[0]), std::size(vertices));
	if (ApplyUtilityState())
		DrawPrimitive();

	// image is now filled with either -1 or INT_MAX, so now we can do the prepass
	SetPrimitiveTopology(s_primitive_topology_mapping[static_cast<u8>(config.topology)]);
	UploadHWDrawVerticesAndIndices(config);

	// cut down the configuration for the prepass, we don't need blending or any feedback loop
	PipelineSelector init_pipe(m_pipeline_selector);
	init_pipe.dss.zwe = false;
	init_pipe.cms.wrgba = 0;
	init_pipe.bs = {};
	init_pipe.rt = true;
	init_pipe.ps.blend_a = init_pipe.ps.blend_b = init_pipe.ps.blend_c = init_pipe.ps.blend_d = false;
	init_pipe.ps.no_color = false;
	init_pipe.ps.no_color1 = true;
	if (BindDrawPipeline(init_pipe))
		DrawIndexedPrimitive();

	// image is initialized/prepass is done, so finish up and get ready to do the "real" draw
	EndRenderPass();

	// .. by setting it to DATE=3
	pipe.ps.date = 3;
	config.alpha_second_pass.ps.date = 3;

	// and bind the image to the primitive sampler
	image->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	PSSetShaderResource(3, image, false);
	return image;
}

void GSDevice12::RenderHW(GSHWDrawConfig& config)
{
	// Destination Alpha Setup
	const bool stencil_DATE_One = config.destination_alpha == GSHWDrawConfig::DestinationAlphaMode::StencilOne;
	const bool stencil_DATE = (config.destination_alpha == GSHWDrawConfig::DestinationAlphaMode::Stencil || stencil_DATE_One);

	// TODO: Backport from vk.
	if (stencil_DATE_One)
		config.ps.date = 0;

	GSTexture12* colclip_rt = static_cast<GSTexture12*>(g_gs_device->GetColorClipTexture());
	GSTexture12* draw_rt = static_cast<GSTexture12*>(config.rt);
	GSTexture12* draw_ds = static_cast<GSTexture12*>(config.ds);
	GSTexture12* draw_rt_clone = nullptr;

	// Align the render area to 128x128, hopefully avoiding render pass restarts for small render area changes (e.g. Ratchet and Clank).
	const GSVector2i rtsize(config.rt ? config.rt->GetSize() : config.ds->GetSize());

	PipelineSelector& pipe = m_pipeline_selector;

	// figure out the pipeline
	UpdateHWPipelineSelector(config);

	// now blit the colclip texture back to the original target
	if (colclip_rt)
	{
		if (config.colclip_mode == GSHWDrawConfig::ColClipMode::EarlyResolve)
		{
			GL_PUSH("Blit ColorClip back to RT");

			EndRenderPass();
			colclip_rt->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			draw_rt = static_cast<GSTexture12*>(config.rt);
			OMSetRenderTargets(draw_rt, draw_ds, config.colclip_update_area);

			// if this target was cleared and never drawn to, perform the clear as part of the resolve here.
			BeginRenderPass(GetLoadOpForTexture(draw_rt), D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE,
				GetLoadOpForTexture(draw_ds),
				draw_ds ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
				D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
				draw_rt->GetUNormClearColor(), 0.0f, 0);

			const GSVector4 sRect(GSVector4(config.colclip_update_area) / GSVector4(rtsize.x, rtsize.y).xyxy());
			SetPipeline(m_colclip_finish_pipelines[pipe.ds].get());
			SetUtilityTexture(colclip_rt, m_point_sampler_cpu);
			DrawStretchRect(sRect, GSVector4(config.colclip_update_area), rtsize);
			g_perfmon.Put(GSPerfMon::TextureCopies, 1);

			Recycle(colclip_rt);
			g_gs_device->SetColorClipTexture(nullptr);
		}
		else
		{
			draw_rt = colclip_rt;
			pipe.ps.colclip_hw = 1;
		}
	}

	if (stencil_DATE)
		SetupDATE(draw_rt, config.ds, config.datm, config.drawarea);

	// stream buffer in first, in case we need to exec
	SetVSConstantBuffer(config.cb_vs);
	SetPSConstantBuffer(config.cb_ps);

	// bind textures before checking the render pass, in case we need to transition them
	if (config.tex)
	{
		PSSetShaderResource(0, config.tex, config.tex != config.rt && config.tex != config.ds);
		PSSetSampler(config.sampler);
	}
	if (config.pal)
		PSSetShaderResource(1, config.pal, true);

	if (config.blend.constant_enable)
		SetBlendConstants(config.blend.constant);

	// Depth testing and sampling, bind resource as dsv read only and srv at the same time without the need of a copy.
	if (config.tex && config.tex == config.ds)
	{
		EndRenderPass();

		// Transition dsv as read only.
		draw_ds->TransitionToState(D3D12_RESOURCE_STATE_DEPTH_READ);
	}

	// Primitive ID tracking DATE setup.
	GSTexture12* date_image = nullptr;
	if (config.destination_alpha == GSHWDrawConfig::DestinationAlphaMode::PrimIDTracking)
	{
		GSTexture* backup_rt = config.rt;
		config.rt = draw_rt;
		date_image = SetupPrimitiveTrackingDATE(config, pipe);
		config.rt = backup_rt;
		if (!date_image)
		{
			Console.Warning("D3D12: Failed to allocate DATE image, aborting draw.");
			return;
		}
	}

	// Switch to colclip target for colclip hw rendering
	if (pipe.ps.colclip_hw)
	{
		if (!colclip_rt)
		{
			config.colclip_update_area = config.drawarea;

			EndRenderPass();

			colclip_rt = static_cast<GSTexture12*>(CreateRenderTarget(rtsize.x, rtsize.y, GSTexture::Format::ColorClip, false));
			if (!colclip_rt)
			{
				Console.Warning("D3D12: Failed to allocate ColorClip render target, aborting draw.");

				if (date_image)
					Recycle(date_image);

				return;
			}

			g_gs_device->SetColorClipTexture(static_cast<GSTexture*>(colclip_rt));

			// propagate clear value through if the colclip render is the first
			if (draw_rt->GetState() == GSTexture::State::Cleared)
			{
				colclip_rt->SetState(GSTexture::State::Cleared);
				colclip_rt->SetClearColor(draw_rt->GetClearColor());
			}
			else if (draw_rt->GetState() == GSTexture::State::Dirty)
			{
				GL_PUSH_("ColorClip Render Target Setup");
				draw_rt->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			}

			// we're not drawing to the RT, so we can use it as a source
			if (config.require_one_barrier && !m_features.multidraw_fb_copy)
				PSSetShaderResource(2, draw_rt, true);
		}

		draw_rt = colclip_rt;
	}

	// Clear texture binding when it's bound to RT or DS.
	if (!config.tex && ((draw_rt && static_cast<GSTexture12*>(draw_rt)->GetSRVDescriptor() == m_tfx_textures[0]) ||
		(draw_ds && static_cast<GSTexture12*>(draw_ds)->GetSRVDescriptor() == m_tfx_textures[0])))
		PSSetShaderResource(0, nullptr, false);

	if (m_in_render_pass && (m_current_render_target == draw_rt || m_current_depth_target == draw_ds))
	{
		// avoid restarting the render pass just to switch from rt+depth to rt and vice versa
		// keep the depth even if doing colclip hw draws, because the next draw will probably re-enable depth
		if (!draw_rt && m_current_render_target && config.tex != m_current_render_target &&
			m_current_render_target->GetSize() == draw_ds->GetSize())
		{
			draw_rt = m_current_render_target;
			m_pipeline_selector.rt = true;
		}
	}
	else if (!draw_ds && m_current_depth_target && config.tex != m_current_depth_target &&
			 m_current_depth_target->GetSize() == draw_rt->GetSize())
	{
		draw_ds = m_current_depth_target;
		m_pipeline_selector.ds = true;
	}

	if (draw_rt && (config.require_one_barrier || (config.require_full_barrier && m_features.multidraw_fb_copy) || (config.tex && config.tex == config.rt)))
	{
		// Requires a copy of the RT.
		// Used as "bind rt" flag when texture barrier is unsupported for tex is fb.
		draw_rt_clone = static_cast<GSTexture12*>(CreateTexture(rtsize.x, rtsize.y, 1, draw_rt->GetFormat(), true));
		if (!draw_rt_clone)
			Console.Warning("D3D12: Failed to allocate temp texture for RT copy.");
	}

	OMSetRenderTargets(draw_rt, draw_ds, config.scissor);

	// Begin render pass if new target or out of the area.
	if (!m_in_render_pass)
	{
		GSVector4 clear_color = draw_rt ? draw_rt->GetUNormClearColor() : GSVector4::zero();
		if (pipe.ps.colclip_hw)
		{
			// Denormalize clear color for hw colclip.
			clear_color *= GSVector4::cxpr(255.0f / 65535.0f, 255.0f / 65535.0f, 255.0f / 65535.0f, 1.0f);
		}
		BeginRenderPass(GetLoadOpForTexture(draw_rt),
			draw_rt ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
			GetLoadOpForTexture(draw_ds),
			draw_ds ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
			stencil_DATE ? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE :
						   D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS,
			stencil_DATE ? (draw_rt_clone ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE :
											D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD) :
						   D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
			clear_color, draw_ds ? draw_ds->GetClearDepth() : 0.0f, 1);
	}

	// rt -> colclip hw blit if enabled
	if (colclip_rt && (config.colclip_mode == GSHWDrawConfig::ColClipMode::ConvertOnly || config.colclip_mode == GSHWDrawConfig::ColClipMode::ConvertAndResolve) && config.rt->GetState() == GSTexture::State::Dirty)
	{
		OMSetRenderTargets(draw_rt, draw_ds, GSVector4i::loadh(rtsize));
		SetUtilityTexture(static_cast<GSTexture12*>(config.rt), m_point_sampler_cpu);
		SetPipeline(m_colclip_setup_pipelines[pipe.ds].get());

		const GSVector4 drawareaf = GSVector4((config.colclip_mode == GSHWDrawConfig::ColClipMode::ConvertOnly) ? GSVector4i::loadh(rtsize) : config.drawarea);
		const GSVector4 sRect(drawareaf / GSVector4(rtsize.x, rtsize.y).xyxy());
		DrawStretchRect(sRect, GSVector4(drawareaf), rtsize);
		g_perfmon.Put(GSPerfMon::TextureCopies, 1);

		GL_POP();

		// Restore original scissor, not sure if needed since the render pass has already been started. But to be safe.
		OMSetRenderTargets(draw_rt, draw_ds, config.scissor);
	}
	// VB/IB upload, if we did DATE setup and it's not colclip hw this has already been done
	SetPrimitiveTopology(s_primitive_topology_mapping[static_cast<u8>(config.topology)]);
	if (!date_image || colclip_rt)
		UploadHWDrawVerticesAndIndices(config);

	// now we can do the actual draw
	SendHWDraw(pipe, config, draw_rt_clone, draw_rt, config.require_one_barrier, config.require_full_barrier, false);

	// blend second pass
	if (config.blend_multi_pass.enable)
	{
		if (config.blend_multi_pass.blend.constant_enable)
			SetBlendConstants(config.blend_multi_pass.blend.constant);

		pipe.bs = config.blend_multi_pass.blend;
		pipe.ps.no_color1 = config.blend_multi_pass.no_color1;
		pipe.ps.blend_hw = config.blend_multi_pass.blend_hw;
		pipe.ps.dither = config.blend_multi_pass.dither;
		if (BindDrawPipeline(pipe))
			DrawIndexedPrimitive();
	}

	// and the alpha pass
	if (config.alpha_second_pass.enable)
	{
		// cbuffer will definitely be dirty if aref changes, no need to check it
		if (config.cb_ps.FogColor_AREF.a != config.alpha_second_pass.ps_aref)
		{
			config.cb_ps.FogColor_AREF.a = config.alpha_second_pass.ps_aref;
			SetPSConstantBuffer(config.cb_ps);
		}

		pipe.ps = config.alpha_second_pass.ps;
		pipe.cms = config.alpha_second_pass.colormask;
		pipe.dss = config.alpha_second_pass.depth;
		pipe.bs = config.blend;
		SendHWDraw(pipe, config, draw_rt_clone, draw_rt, config.alpha_second_pass.require_one_barrier, config.alpha_second_pass.require_full_barrier, true);
	}

	if (draw_rt_clone)
		Recycle(draw_rt_clone);

	if (date_image)
		Recycle(date_image);

	// now blit the colclip texture back to the original target
	if (colclip_rt)
	{
		config.colclip_update_area = config.colclip_update_area.runion(config.drawarea);

		if ((config.colclip_mode == GSHWDrawConfig::ColClipMode::ResolveOnly || config.colclip_mode == GSHWDrawConfig::ColClipMode::ConvertAndResolve))
		{
			GL_PUSH("Blit ColorClip back to RT");

			EndRenderPass();
			colclip_rt->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			draw_rt = static_cast<GSTexture12*>(config.rt);
			OMSetRenderTargets(draw_rt, draw_ds, config.colclip_update_area);

			// if this target was cleared and never drawn to, perform the clear as part of the resolve here.
			BeginRenderPass(GetLoadOpForTexture(draw_rt), D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE,
				GetLoadOpForTexture(draw_ds),
				draw_ds ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
				D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
				draw_rt->GetUNormClearColor(), 0.0f, 0);

			const GSVector4 sRect(GSVector4(config.colclip_update_area) / GSVector4(rtsize.x, rtsize.y).xyxy());
			SetPipeline(m_colclip_finish_pipelines[pipe.ds].get());
			SetUtilityTexture(colclip_rt, m_point_sampler_cpu);
			DrawStretchRect(sRect, GSVector4(config.colclip_update_area), rtsize);
			g_perfmon.Put(GSPerfMon::TextureCopies, 1);

			Recycle(colclip_rt);
			g_gs_device->SetColorClipTexture(nullptr);
		}
	}
}

void GSDevice12::SendHWDraw(const PipelineSelector& pipe, const GSHWDrawConfig& config, GSTexture12* draw_rt_clone, GSTexture12* draw_rt, const bool one_barrier, const bool full_barrier, const bool skip_first_barrier)
{
	if (draw_rt_clone)
	{

#ifdef PCSX2_DEVBUILD
		if ((one_barrier || full_barrier) && !config.ps.IsFeedbackLoop()) [[unlikely]]
			Console.Warning("D3D12: Possible unnecessary copy detected.");
#endif
		auto CopyAndBind = [&](GSVector4i drawarea) {
			EndRenderPass();

			CopyRect(draw_rt, draw_rt_clone, drawarea, drawarea.left, drawarea.top);
			draw_rt->TransitionToState(D3D12_RESOURCE_STATE_RENDER_TARGET);

			if (one_barrier || full_barrier)
				PSSetShaderResource(2, draw_rt_clone, true);
			if (config.tex && config.tex == config.rt)
				PSSetShaderResource(0, draw_rt_clone, true);
		};

		if (m_features.multidraw_fb_copy && full_barrier)
		{
			const u32 draw_list_size = static_cast<u32>(config.drawlist->size());
			const u32 indices_per_prim = config.indices_per_prim;

			pxAssert(config.drawlist && !config.drawlist->empty());
			pxAssert(config.drawlist_bbox && static_cast<u32>(config.drawlist_bbox->size()) == draw_list_size);

			for (u32 n = 0, p = 0; n < draw_list_size; n++)
			{
				const u32 count = (*config.drawlist)[n] * indices_per_prim;

				GSVector4i bbox = (*config.drawlist_bbox)[n].rintersect(config.drawarea);

				// Copy only the part needed by the draw.
				CopyAndBind(bbox);
				if (BindDrawPipeline(pipe))
					DrawIndexedPrimitive(p, count);
				p += count;
			}

			return;
		}


		// Optimization: For alpha second pass we can reuse the copy snapshot from the first pass.
		if (!skip_first_barrier)
			CopyAndBind(config.drawarea);
	}

	if (BindDrawPipeline(pipe))
		DrawIndexedPrimitive();
}

void GSDevice12::UpdateHWPipelineSelector(GSHWDrawConfig& config)
{
	m_pipeline_selector.vs.key = config.vs.key;
	m_pipeline_selector.ps.key_hi = config.ps.key_hi;
	m_pipeline_selector.ps.key_lo = config.ps.key_lo;
	m_pipeline_selector.dss.key = config.depth.key;
	m_pipeline_selector.bs.key = config.blend.key;
	m_pipeline_selector.bs.constant = 0; // don't dupe states with different alpha values
	m_pipeline_selector.cms.key = config.colormask.key;
	m_pipeline_selector.topology = static_cast<u32>(config.topology);
	m_pipeline_selector.rt = config.rt != nullptr;
	m_pipeline_selector.ds = config.ds != nullptr;
}

void GSDevice12::UploadHWDrawVerticesAndIndices(const GSHWDrawConfig& config)
{
	IASetVertexBuffer(config.verts, sizeof(GSVertex), config.nverts);

	// Update SRV in root signature directly, rather than using a uniform for base vertex.
	if (config.vs.expand != GSHWDrawConfig::VSExpand::None)
		m_dirty_flags |= DIRTY_FLAG_VS_VERTEX_BUFFER_BINDING;

	if (config.vs.UseExpandIndexBuffer())
	{
		m_index.start = 0;
		m_index.count = config.nindices;
		SetIndexBuffer(m_expand_index_buffer->GetGPUVirtualAddress(), EXPAND_BUFFER_SIZE, DXGI_FORMAT_R16_UINT);
	}
	else
	{
		IASetIndexBuffer(config.indices, config.nindices);
	}
}
