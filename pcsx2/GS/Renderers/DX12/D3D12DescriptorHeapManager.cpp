// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "PrecompiledHeader.h"

#include "GS/Renderers/DX12/D3D12DescriptorHeapManager.h"
#include "common/Assertions.h"

D3D12DescriptorHeapManager::D3D12DescriptorHeapManager() = default;
D3D12DescriptorHeapManager::~D3D12DescriptorHeapManager() = default;

bool D3D12DescriptorHeapManager::Create(
	ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 num_descriptors, bool shader_visible)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {type, static_cast<UINT>(num_descriptors),
		shader_visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE};

	HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(m_descriptor_heap.put()));
	pxAssertRel(SUCCEEDED(hr), "Create descriptor heap");
	if (FAILED(hr))
		return false;

	m_heap_base_cpu = m_descriptor_heap->GetCPUDescriptorHandleForHeapStart();
	if (shader_visible)
		m_heap_base_gpu = m_descriptor_heap->GetGPUDescriptorHandleForHeapStart();

	m_num_descriptors = num_descriptors;
	m_descriptor_increment_size = device->GetDescriptorHandleIncrementSize(type);
	m_shader_visible = shader_visible;

	// Set all slots to unallocated (1)
	const u32 bitset_count = num_descriptors / BITSET_SIZE + (((num_descriptors % BITSET_SIZE) != 0) ? 1 : 0);
	m_free_slots.resize(bitset_count);
	for (BitSetType& bs : m_free_slots)
		bs.flip();

	return true;
}

void D3D12DescriptorHeapManager::Destroy()
{
#ifdef PCSX2_DEVBUILD
	for (BitSetType& bs : m_free_slots)
	{
		pxAssertMsg(bs.all(), "Not all descriptors were freed");
	}
#endif

	m_shader_visible = false;
	m_num_descriptors = 0;
	m_descriptor_increment_size = 0;
	m_heap_base_cpu = {};
	m_heap_base_gpu = {};
	m_descriptor_heap.reset();
	m_free_slots.clear();
}

bool D3D12DescriptorHeapManager::Allocate(D3D12DescriptorHandle* handle)
{
	// Start past the temporary slots, no point in searching those.
	for (u32 group = 0; group < m_free_slots.size(); group++)
	{
		BitSetType& bs = m_free_slots[group];
		if (bs.none())
			continue;

		u32 bit = 0;
		for (; bit < BITSET_SIZE; bit++)
		{
			if (bs[bit])
				break;
		}

		u32 index = group * BITSET_SIZE + bit;
		bs[bit] = false;

		handle->index = index;
		handle->cpu_handle.ptr = m_heap_base_cpu.ptr + index * m_descriptor_increment_size;
		handle->gpu_handle.ptr = m_shader_visible ? (m_heap_base_gpu.ptr + index * m_descriptor_increment_size) : 0;
		return true;
	}

	pxFailRel("Out of fixed descriptors");
	return false;
}

void D3D12DescriptorHeapManager::Free(u32 index)
{
	pxAssert(index < m_num_descriptors);

	u32 group = index / BITSET_SIZE;
	u32 bit = index % BITSET_SIZE;
	m_free_slots[group][bit] = true;
}

void D3D12DescriptorHeapManager::Free(D3D12DescriptorHandle* handle)
{
	if (handle->index == D3D12DescriptorHandle::INVALID_INDEX)
		return;

	Free(handle->index);
	handle->Clear();
}

D3D12DescriptorAllocator::D3D12DescriptorAllocator() = default;
D3D12DescriptorAllocator::~D3D12DescriptorAllocator() = default;

bool D3D12DescriptorAllocator::Create(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 num_descriptors)
{
	const D3D12_DESCRIPTOR_HEAP_DESC desc = {
		type, static_cast<UINT>(num_descriptors), D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE};
	const HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_descriptor_heap));
	pxAssertRel(SUCCEEDED(hr), "Creating descriptor heap for linear allocator");
	if (FAILED(hr))
		return false;

	m_num_descriptors = num_descriptors;
	m_descriptor_increment_size = device->GetDescriptorHandleIncrementSize(type);
	m_heap_base_cpu = m_descriptor_heap->GetCPUDescriptorHandleForHeapStart();
	m_heap_base_gpu = m_descriptor_heap->GetGPUDescriptorHandleForHeapStart();
	return true;
}

void D3D12DescriptorAllocator::Destroy()
{
	m_descriptor_heap.reset();
	m_descriptor_increment_size = 0;
	m_num_descriptors = 0;
	m_current_offset = 0;
	m_heap_base_cpu = {};
	m_heap_base_gpu = {};
}

bool D3D12DescriptorAllocator::Allocate(u32 num_handles, D3D12DescriptorHandle* out_base_handle)
{
	if ((m_current_offset + num_handles) > m_num_descriptors)
		return false;

	out_base_handle->index = m_current_offset;
	out_base_handle->cpu_handle.ptr = m_heap_base_cpu.ptr + m_current_offset * m_descriptor_increment_size;
	out_base_handle->gpu_handle.ptr = m_heap_base_gpu.ptr + m_current_offset * m_descriptor_increment_size;
	m_current_offset += num_handles;
	return true;
}

void D3D12DescriptorAllocator::Reset()
{
	m_current_offset = 0;
}
