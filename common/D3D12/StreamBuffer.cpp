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

#include "common/D3D12/StreamBuffer.h"
#include "common/D3D12/Context.h"
#include "common/Align.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "D3D12MemAlloc.h"

#include <algorithm>
#include <functional>

using namespace D3D12;

StreamBuffer::StreamBuffer() = default;

StreamBuffer::~StreamBuffer()
{
	Destroy();
}

bool StreamBuffer::Create(u32 size)
{
	const D3D12_RESOURCE_DESC resource_desc = {
		D3D12_RESOURCE_DIMENSION_BUFFER, 0, size, 1, 1, 1, DXGI_FORMAT_UNKNOWN, {1, 0}, D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		D3D12_RESOURCE_FLAG_NONE};

	D3D12MA::ALLOCATION_DESC allocationDesc = {};
	allocationDesc.Flags = D3D12MA::ALLOCATION_FLAG_COMMITTED;
	allocationDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

	wil::com_ptr_nothrow<ID3D12Resource> buffer;
	wil::com_ptr_nothrow<D3D12MA::Allocation> allocation;
	HRESULT hr = g_d3d12_context->GetAllocator()->CreateResource(&allocationDesc,
		&resource_desc, D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr, allocation.put(), IID_PPV_ARGS(buffer.put()));
	pxAssertMsg(SUCCEEDED(hr), "Allocate buffer");
	if (FAILED(hr))
		return false;

	static const D3D12_RANGE read_range = {};
	u8* host_pointer;
	hr = buffer->Map(0, &read_range, reinterpret_cast<void**>(&host_pointer));
	pxAssertMsg(SUCCEEDED(hr), "Map buffer");
	if (FAILED(hr))
		return false;

	Destroy(true);

	m_buffer = std::move(buffer);
	m_allocation = std::move(allocation);
	m_host_pointer = host_pointer;
	m_size = size;
	m_gpu_pointer = m_buffer->GetGPUVirtualAddress();
	return true;
}

bool StreamBuffer::ReserveMemory(u32 num_bytes, u32 alignment)
{
	const u32 required_bytes = num_bytes + alignment;

	// Check for sane allocations
	if (num_bytes > m_size)
	{
		Console.Error("Attempting to allocate %u bytes from a %u byte stream buffer", static_cast<u32>(num_bytes),
			static_cast<u32>(m_size));
		pxFailRel("Stream buffer overflow");
		return false;
	}

	// Is the GPU behind or up to date with our current offset?
	UpdateCurrentFencePosition();
	if (m_current_offset >= m_current_gpu_position)
	{
		const u32 aligned_required_bytes = (m_current_offset > 0) ? required_bytes : num_bytes;
		const u32 remaining_bytes = m_size - m_current_offset;
		if (aligned_required_bytes <= remaining_bytes)
		{
			// Place at the current position, after the GPU position.
			m_current_offset = Common::AlignUp(m_current_offset, alignment);
			m_current_space = m_size - m_current_offset;
			return true;
		}

		// Check for space at the start of the buffer
		// We use < here because we don't want to have the case of m_current_offset ==
		// m_current_gpu_position. That would mean the code above would assume the
		// GPU has caught up to us, which it hasn't.
		if (required_bytes < m_current_gpu_position)
		{
			// Reset offset to zero, since we're allocating behind the gpu now
			m_current_offset = 0;
			m_current_space = m_current_gpu_position;
			return true;
		}
	}

	// Is the GPU ahead of our current offset?
	if (m_current_offset < m_current_gpu_position)
	{
		// We have from m_current_offset..m_current_gpu_position space to use.
		const u32 remaining_bytes = m_current_gpu_position - m_current_offset;
		if (required_bytes < remaining_bytes)
		{
			// Place at the current position, since this is still behind the GPU.
			m_current_offset = Common::AlignUp(m_current_offset, alignment);
			m_current_space = m_current_gpu_position - m_current_offset;
			return true;
		}
	}

	// Can we find a fence to wait on that will give us enough memory?
	if (WaitForClearSpace(required_bytes))
	{
		const u32 align_diff = Common::AlignUp(m_current_offset, alignment) - m_current_offset;
		m_current_offset += align_diff;
		m_current_space -= align_diff;
		return true;
	}

	// We tried everything we could, and still couldn't get anything. This means that too much space
	// in the buffer is being used by the command buffer currently being recorded. Therefore, the
	// only option is to execute it, and wait until it's done.
	return false;
}

void StreamBuffer::CommitMemory(u32 final_num_bytes)
{
	pxAssert((m_current_offset + final_num_bytes) <= m_size);
	pxAssert(final_num_bytes <= m_current_space);
	m_current_offset += final_num_bytes;
	m_current_space -= final_num_bytes;
}

void StreamBuffer::Destroy(bool defer)
{
	if (m_host_pointer)
	{
		const D3D12_RANGE written_range = {0, m_size};
		m_buffer->Unmap(0, &written_range);
		m_host_pointer = nullptr;
	}

	if (m_buffer && defer)
		g_d3d12_context->DeferResourceDestruction(m_allocation.get(), m_buffer.get());
	m_buffer.reset();
	m_allocation.reset();

	m_current_offset = 0;
	m_current_space = 0;
	m_current_gpu_position = 0;
	m_tracked_fences.clear();
}

void StreamBuffer::UpdateCurrentFencePosition()
{
	// Don't create a tracking entry if the GPU is caught up with the buffer.
	if (m_current_offset == m_current_gpu_position)
		return;

	// Has the offset changed since the last fence?
	const u64 fence = g_d3d12_context->GetCurrentFenceValue();
	if (!m_tracked_fences.empty() && m_tracked_fences.back().first == fence)
	{
		// Still haven't executed a command buffer, so just update the offset.
		m_tracked_fences.back().second = m_current_offset;
		return;
	}

	UpdateGPUPosition();
	m_tracked_fences.emplace_back(fence, m_current_offset);
}

void StreamBuffer::UpdateGPUPosition()
{
	auto start = m_tracked_fences.begin();
	auto end = start;

	const u64 completed_counter = g_d3d12_context->GetCompletedFenceValue();
	while (end != m_tracked_fences.end() && completed_counter >= end->first)
	{
		m_current_gpu_position = end->second;
		++end;
	}

	if (start != end)
		m_tracked_fences.erase(start, end);
}

bool StreamBuffer::WaitForClearSpace(u32 num_bytes)
{
	u32 new_offset = 0;
	u32 new_space = 0;
	u32 new_gpu_position = 0;

	auto iter = m_tracked_fences.begin();
	for (; iter != m_tracked_fences.end(); ++iter)
	{
		// Would this fence bring us in line with the GPU?
		// This is the "last resort" case, where a command buffer execution has been forced
		// after no additional data has been written to it, so we can assume that after the
		// fence has been signaled the entire buffer is now consumed.
		u32 gpu_position = iter->second;
		if (m_current_offset == gpu_position)
		{
			new_offset = 0;
			new_space = m_size;
			new_gpu_position = 0;
			break;
		}

		// Assuming that we wait for this fence, are we allocating in front of the GPU?
		if (m_current_offset > gpu_position)
		{
			// This would suggest the GPU has now followed us and wrapped around, so we have from
			// m_current_position..m_size free, as well as and 0..gpu_position.
			const u32 remaining_space_after_offset = m_size - m_current_offset;
			if (remaining_space_after_offset >= num_bytes)
			{
				// Switch to allocating in front of the GPU, using the remainder of the buffer.
				new_offset = m_current_offset;
				new_space = m_size - m_current_offset;
				new_gpu_position = gpu_position;
				break;
			}

			// We can wrap around to the start, behind the GPU, if there is enough space.
			// We use > here because otherwise we'd end up lining up with the GPU, and then the
			// allocator would assume that the GPU has consumed what we just wrote.
			if (gpu_position > num_bytes)
			{
				new_offset = 0;
				new_space = gpu_position;
				new_gpu_position = gpu_position;
				break;
			}
		}
		else
		{
			// We're currently allocating behind the GPU. This would give us between the current
			// offset and the GPU position worth of space to work with. Again, > because we can't
			// align the GPU position with the buffer offset.
			u32 available_space_inbetween = gpu_position - m_current_offset;
			if (available_space_inbetween > num_bytes)
			{
				// Leave the offset as-is, but update the GPU position.
				new_offset = m_current_offset;
				new_space = gpu_position - m_current_offset;
				new_gpu_position = gpu_position;
				break;
			}
		}
	}

	// Did any fences satisfy this condition?
	// Has the command buffer been executed yet? If not, the caller should execute it.
	if (iter == m_tracked_fences.end() || iter->first == g_d3d12_context->GetCurrentFenceValue())
		return false;

	// Wait until this fence is signaled. This will fire the callback, updating the GPU position.
	g_d3d12_context->WaitForFence(iter->first);
	m_tracked_fences.erase(m_tracked_fences.begin(), m_current_offset == iter->second ? m_tracked_fences.end() : ++iter);
	m_current_offset = new_offset;
	m_current_space = new_space;
	m_current_gpu_position = new_gpu_position;
	return true;
}
