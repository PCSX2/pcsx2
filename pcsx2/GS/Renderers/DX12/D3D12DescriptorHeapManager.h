// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"
#include "common/HashCombine.h"
#include "common/RedtapeWindows.h"
#include "common/RedtapeWilCom.h"

#include <bitset>
#include <cstring>
#include <d3d12.h>
#include <unordered_map>
#include <vector>

// This class provides an abstraction for D3D12 descriptor heaps.
struct D3D12DescriptorHandle final
{
	enum : u32
	{
		INVALID_INDEX = 0xFFFFFFFF
	};

	D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle{};
	D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle{};
	u32 index = INVALID_INDEX;

	__fi operator bool() const { return index != INVALID_INDEX; }

	__fi operator D3D12_CPU_DESCRIPTOR_HANDLE() const { return cpu_handle; }
	__fi operator D3D12_GPU_DESCRIPTOR_HANDLE() const { return gpu_handle; }

	__fi bool operator==(const D3D12DescriptorHandle& rhs) const { return (index == rhs.index); }
	__fi bool operator!=(const D3D12DescriptorHandle& rhs) const { return (index != rhs.index); }
	__fi bool operator<(const D3D12DescriptorHandle& rhs) const { return (index < rhs.index); }
	__fi bool operator<=(const D3D12DescriptorHandle& rhs) const { return (index <= rhs.index); }
	__fi bool operator>(const D3D12DescriptorHandle& rhs) const { return (index > rhs.index); }
	__fi bool operator>=(const D3D12DescriptorHandle& rhs) const { return (index >= rhs.index); }

	__fi void Clear()
	{
		cpu_handle = {};
		gpu_handle = {};
		index = INVALID_INDEX;
	}
};

class D3D12DescriptorHeapManager final
{
public:
	D3D12DescriptorHeapManager();
	~D3D12DescriptorHeapManager();

	ID3D12DescriptorHeap* GetDescriptorHeap() const { return m_descriptor_heap.get(); }
	u32 GetDescriptorIncrementSize() const { return m_descriptor_increment_size; }

	bool Create(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 num_descriptors, bool shader_visible);
	void Destroy();

	bool Allocate(D3D12DescriptorHandle* handle);
	void Free(D3D12DescriptorHandle* handle);
	void Free(u32 index);

private:
	wil::com_ptr_nothrow<ID3D12DescriptorHeap> m_descriptor_heap;
	u32 m_num_descriptors = 0;
	u32 m_descriptor_increment_size = 0;
	bool m_shader_visible = false;

	D3D12_CPU_DESCRIPTOR_HANDLE m_heap_base_cpu = {};
	D3D12_GPU_DESCRIPTOR_HANDLE m_heap_base_gpu = {};

	static constexpr u32 BITSET_SIZE = 1024;
	using BitSetType = std::bitset<BITSET_SIZE>;
	std::vector<BitSetType> m_free_slots = {};
};

class D3D12DescriptorAllocator
{
public:
	D3D12DescriptorAllocator();
	~D3D12DescriptorAllocator();

	__fi ID3D12DescriptorHeap* GetDescriptorHeap() const { return m_descriptor_heap.get(); }
	__fi u32 GetDescriptorIncrementSize() const { return m_descriptor_increment_size; }

	bool Create(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 num_descriptors);
	void Destroy();

	bool Allocate(u32 num_handles, D3D12DescriptorHandle* out_base_handle);
	void Reset();

private:
	wil::com_ptr_nothrow<ID3D12DescriptorHeap> m_descriptor_heap;
	u32 m_descriptor_increment_size = 0;
	u32 m_num_descriptors = 0;
	u32 m_current_offset = 0;

	D3D12_CPU_DESCRIPTOR_HANDLE m_heap_base_cpu = {};
	D3D12_GPU_DESCRIPTOR_HANDLE m_heap_base_gpu = {};
};

template <u32 NumSamplers>
class D3D12GroupedSamplerAllocator : private D3D12DescriptorAllocator
{
	struct Key
	{
		u32 idx[NumSamplers];

		__fi bool operator==(const Key& rhs) const { return (std::memcmp(idx, rhs.idx, sizeof(idx)) == 0); }
		__fi bool operator!=(const Key& rhs) const { return (std::memcmp(idx, rhs.idx, sizeof(idx)) != 0); }
	};

	struct KeyHash
	{
		__fi std::size_t operator()(const Key& key) const
		{
			size_t seed = 0;
			for (u32 key : key.idx)
				HashCombine(seed, key);
			return seed;
		}
	};


public:
	D3D12GroupedSamplerAllocator();
	~D3D12GroupedSamplerAllocator();

	using D3D12DescriptorAllocator::GetDescriptorHeap;
	using D3D12DescriptorAllocator::GetDescriptorIncrementSize;

	bool Create(ID3D12Device* device, u32 num_descriptors);
	void Destroy();

	bool LookupSingle(D3D12DescriptorHandle* gpu_handle, const D3D12DescriptorHandle& cpu_handle);
	bool LookupGroup(D3D12DescriptorHandle* gpu_handle, const D3D12DescriptorHandle* cpu_handles);

	// Clears cache but doesn't reset allocator.
	void InvalidateCache();

	void Reset();
	bool ShouldReset() const;

private:
	wil::com_ptr_nothrow<ID3D12Device> m_device;
	std::unordered_map<Key, D3D12DescriptorHandle, KeyHash> m_groups;
};

template <u32 NumSamplers>
D3D12GroupedSamplerAllocator<NumSamplers>::D3D12GroupedSamplerAllocator() = default;

template <u32 NumSamplers>
D3D12GroupedSamplerAllocator<NumSamplers>::~D3D12GroupedSamplerAllocator() = default;

template <u32 NumSamplers>
bool D3D12GroupedSamplerAllocator<NumSamplers>::Create(ID3D12Device* device, u32 num_descriptors)
{
	if (!D3D12DescriptorAllocator::Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, num_descriptors))
		return false;

	m_device = device;
	return true;
}

template <u32 NumSamplers>
void D3D12GroupedSamplerAllocator<NumSamplers>::Destroy()
{
	D3D12DescriptorAllocator::Destroy();
	m_device.reset();
}

template <u32 NumSamplers>
void D3D12GroupedSamplerAllocator<NumSamplers>::Reset()
{
	m_groups.clear();
	D3D12DescriptorAllocator::Reset();
}

template <u32 NumSamplers>
void D3D12GroupedSamplerAllocator<NumSamplers>::InvalidateCache()
{
	m_groups.clear();
}

template <u32 NumSamplers>
bool D3D12GroupedSamplerAllocator<NumSamplers>::LookupSingle(
	D3D12DescriptorHandle* gpu_handle, const D3D12DescriptorHandle& cpu_handle)
{
	Key key;
	key.idx[0] = cpu_handle.index;
	for (u32 i = 1; i < NumSamplers; i++)
		key.idx[i] = 0;

	auto it = m_groups.find(key);
	if (it != m_groups.end())
	{
		*gpu_handle = it->second;
		return true;
	}

	if (!Allocate(1, gpu_handle))
		return false;

	m_device->CopyDescriptorsSimple(1, *gpu_handle, cpu_handle, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	m_groups.emplace(key, *gpu_handle);
	return true;
}

template <u32 NumSamplers>
bool D3D12GroupedSamplerAllocator<NumSamplers>::LookupGroup(
	D3D12DescriptorHandle* gpu_handle, const D3D12DescriptorHandle* cpu_handles)
{
	Key key;
	for (u32 i = 0; i < NumSamplers; i++)
		key.idx[i] = cpu_handles[i].index;

	auto it = m_groups.find(key);
	if (it != m_groups.end())
	{
		*gpu_handle = it->second;
		return true;
	}

	if (!Allocate(NumSamplers, gpu_handle))
		return false;

	D3D12_CPU_DESCRIPTOR_HANDLE dst_handle = *gpu_handle;
	UINT dst_size = NumSamplers;
	D3D12_CPU_DESCRIPTOR_HANDLE src_handles[NumSamplers];
	UINT src_sizes[NumSamplers];
	for (u32 i = 0; i < NumSamplers; i++)
	{
		src_handles[i] = cpu_handles[i];
		src_sizes[i] = 1;
	}
	m_device->CopyDescriptors(
		1, &dst_handle, &dst_size, NumSamplers, src_handles, src_sizes, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

	m_groups.emplace(key, *gpu_handle);
	return true;
}

template <u32 NumSamplers>
bool D3D12GroupedSamplerAllocator<NumSamplers>::ShouldReset() const
{
	// We only reset the sampler heap if more than half of the descriptors are used.
	// This saves descriptor copying when there isn't a large number of sampler configs per frame.
	return m_groups.size() >= (D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE / 2);
}
