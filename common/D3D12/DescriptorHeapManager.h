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
#include "common/HashCombine.h"
#include "common/RedtapeWindows.h"

#include <bitset>
#include <cstring>
#include <d3d12.h>
#include <unordered_map>
#include <vector>
#include <wil/com.h>

namespace D3D12
{
	// This class provides an abstraction for D3D12 descriptor heaps.
	struct DescriptorHandle final
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

		__fi bool operator==(const DescriptorHandle& rhs) const { return (index == rhs.index); }
		__fi bool operator!=(const DescriptorHandle& rhs) const { return (index != rhs.index); }
		__fi bool operator<(const DescriptorHandle& rhs) const { return (index < rhs.index); }
		__fi bool operator<=(const DescriptorHandle& rhs) const { return (index <= rhs.index); }
		__fi bool operator>(const DescriptorHandle& rhs) const { return (index > rhs.index); }
		__fi bool operator>=(const DescriptorHandle& rhs) const { return (index >= rhs.index); }

		__fi void Clear()
		{
			cpu_handle = {};
			gpu_handle = {};
			index = INVALID_INDEX;
		}
	};

	class DescriptorHeapManager final
	{
	public:
		DescriptorHeapManager();
		~DescriptorHeapManager();

		ID3D12DescriptorHeap* GetDescriptorHeap() const { return m_descriptor_heap.get(); }
		u32 GetDescriptorIncrementSize() const { return m_descriptor_increment_size; }

		bool Create(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 num_descriptors, bool shader_visible);
		void Destroy();

		bool Allocate(DescriptorHandle* handle);
		void Free(DescriptorHandle* handle);
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

	class DescriptorAllocator
	{
	public:
		DescriptorAllocator();
		~DescriptorAllocator();

		__fi ID3D12DescriptorHeap* GetDescriptorHeap() const { return m_descriptor_heap.get(); }
		__fi u32 GetDescriptorIncrementSize() const { return m_descriptor_increment_size; }

		bool Create(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 num_descriptors);
		void Destroy();

		bool Allocate(u32 num_handles, DescriptorHandle* out_base_handle);
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
	class GroupedSamplerAllocator : private DescriptorAllocator
	{
		struct Key
		{
			u32 idx[NumSamplers];

			__fi bool operator==(const Key& rhs) const
			{
				return (std::memcmp(idx, rhs.idx, sizeof(idx)) == 0);
			}
			__fi bool operator!=(const Key& rhs) const
			{
				return (std::memcmp(idx, rhs.idx, sizeof(idx)) != 0);
			}
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
		GroupedSamplerAllocator();
		~GroupedSamplerAllocator();

		using DescriptorAllocator::GetDescriptorHeap;
		using DescriptorAllocator::GetDescriptorIncrementSize;

		bool Create(ID3D12Device* device, u32 num_descriptors);
		void Destroy();

		bool LookupSingle(DescriptorHandle* gpu_handle, const DescriptorHandle& cpu_handle);
		bool LookupGroup(DescriptorHandle* gpu_handle, const DescriptorHandle* cpu_handles);

		// Clears cache but doesn't reset allocator.
		void InvalidateCache();

		void Reset();
		bool ShouldReset() const;

	private:
		wil::com_ptr_nothrow<ID3D12Device> m_device;
		std::unordered_map<Key, D3D12::DescriptorHandle, KeyHash> m_groups;
	};

	template <u32 NumSamplers>
	GroupedSamplerAllocator<NumSamplers>::GroupedSamplerAllocator() = default;

	template <u32 NumSamplers>
	GroupedSamplerAllocator<NumSamplers>::~GroupedSamplerAllocator() = default;

	template <u32 NumSamplers>
	bool GroupedSamplerAllocator<NumSamplers>::Create(ID3D12Device* device, u32 num_descriptors)
	{
		if (!DescriptorAllocator::Create(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, num_descriptors))
			return false;

		m_device = device;
		return true;
	}

	template <u32 NumSamplers>
	void GroupedSamplerAllocator<NumSamplers>::Destroy()
	{
		DescriptorAllocator::Destroy();
		m_device.reset();
	}

	template <u32 NumSamplers>
	void GroupedSamplerAllocator<NumSamplers>::Reset()
	{
		m_groups.clear();
		DescriptorAllocator::Reset();
	}

	template <u32 NumSamplers>
	void GroupedSamplerAllocator<NumSamplers>::InvalidateCache()
	{
		m_groups.clear();
	}

	template <u32 NumSamplers>
	bool GroupedSamplerAllocator<NumSamplers>::LookupSingle(DescriptorHandle* gpu_handle, const DescriptorHandle& cpu_handle)
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
	bool GroupedSamplerAllocator<NumSamplers>::LookupGroup(DescriptorHandle* gpu_handle, const DescriptorHandle* cpu_handles)
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
		m_device->CopyDescriptors(1, &dst_handle, &dst_size, NumSamplers, src_handles, src_sizes, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

		m_groups.emplace(key, *gpu_handle);
		return true;
	}

	template <u32 NumSamplers>
	bool GroupedSamplerAllocator<NumSamplers>::ShouldReset() const
	{
		// We only reset the sampler heap if more than half of the descriptors are used.
		// This saves descriptor copying when there isn't a large number of sampler configs per frame.
		return m_groups.size() >= (D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE / 2);
	}
} // namespace D3D12
