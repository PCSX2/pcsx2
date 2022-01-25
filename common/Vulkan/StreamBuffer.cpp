/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#include "common/Vulkan/StreamBuffer.h"
#include "common/Vulkan/Context.h"
#include "common/Vulkan/Util.h"
#include "common/Align.h"
#include "common/Assertions.h"
#include "common/Console.h"

namespace Vulkan
{
	StreamBuffer::StreamBuffer() = default;

	StreamBuffer::StreamBuffer(StreamBuffer&& move)
		: m_size(move.m_size)
		, m_current_offset(move.m_current_offset)
		, m_current_space(move.m_current_space)
		, m_current_gpu_position(move.m_current_gpu_position)
		, m_allocation(move.m_allocation)
		, m_buffer(move.m_buffer)
		, m_host_pointer(move.m_host_pointer)
		, m_tracked_fences(std::move(move.m_tracked_fences))
	{
		move.m_size = 0;
		move.m_current_offset = 0;
		move.m_current_space = 0;
		move.m_current_gpu_position = 0;
		move.m_allocation = VK_NULL_HANDLE;
		move.m_buffer = VK_NULL_HANDLE;
		move.m_host_pointer = nullptr;
	}

	StreamBuffer::~StreamBuffer()
	{
		if (IsValid())
			Destroy(true);
	}

	StreamBuffer& StreamBuffer::operator=(StreamBuffer&& move)
	{
		if (IsValid())
			Destroy(true);

		std::swap(m_size, move.m_size);
		std::swap(m_current_offset, move.m_current_offset);
		std::swap(m_current_space, move.m_current_space);
		std::swap(m_current_gpu_position, move.m_current_gpu_position);
		std::swap(m_buffer, move.m_buffer);
		std::swap(m_host_pointer, move.m_host_pointer);
		std::swap(m_tracked_fences, move.m_tracked_fences);

		return *this;
	}

	bool StreamBuffer::Create(VkBufferUsageFlags usage, u32 size)
	{
		const VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0,
			static_cast<VkDeviceSize>(size), usage, VK_SHARING_MODE_EXCLUSIVE, 0, nullptr};

		VmaAllocationCreateInfo aci = {};
		aci.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
		aci.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
		aci.preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		VmaAllocationInfo ai = {};
		VkBuffer new_buffer = VK_NULL_HANDLE;
		VmaAllocation new_allocation = VK_NULL_HANDLE;
		VkResult res = vmaCreateBuffer(g_vulkan_context->GetAllocator(), &bci, &aci, &new_buffer, &new_allocation, &ai);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateBuffer failed: ");
			return false;
		}

		if (IsValid())
			Destroy(true);

		// Replace with the new buffer
		m_size = size;
		m_current_offset = 0;
		m_current_gpu_position = 0;
		m_tracked_fences.clear();
		m_allocation = new_allocation;
		m_buffer = new_buffer;
		m_host_pointer = static_cast<u8*>(ai.pMappedData);
		return true;
	}

	void StreamBuffer::Destroy(bool defer)
	{
		if (m_buffer != VK_NULL_HANDLE)
		{
			if (defer)
				g_vulkan_context->DeferBufferDestruction(m_buffer, m_allocation);
			else
				vmaDestroyBuffer(g_vulkan_context->GetAllocator(), m_buffer, m_allocation);
		}

		m_size = 0;
		m_current_offset = 0;
		m_current_gpu_position = 0;
		m_tracked_fences.clear();
		m_buffer = VK_NULL_HANDLE;
		m_allocation = VK_NULL_HANDLE;
		m_host_pointer = nullptr;
	}

	bool StreamBuffer::ReserveMemory(u32 num_bytes, u32 alignment)
	{
		const u32 required_bytes = num_bytes + alignment;

		// Check for sane allocations
		if (required_bytes > m_size)
		{
			Console.Error("Attempting to allocate %u bytes from a %u byte stream buffer", static_cast<u32>(num_bytes),
				static_cast<u32>(m_size));
			pxFailRel("Stream buffer overflow");
			return false;
		}

		UpdateGPUPosition();

		// Is the GPU behind or up to date with our current offset?
		if (m_current_offset >= m_current_gpu_position)
		{
			const u32 remaining_bytes = m_size - m_current_offset;
			if (required_bytes <= remaining_bytes)
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
				m_current_space = m_current_gpu_position - 1;
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
				m_current_space = m_current_gpu_position - m_current_offset - 1;
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

		// For non-coherent mappings, flush the memory range
		vmaFlushAllocation(g_vulkan_context->GetAllocator(), m_allocation, m_current_offset, final_num_bytes);

		m_current_offset += final_num_bytes;
		m_current_space -= final_num_bytes;
		UpdateCurrentFencePosition();
	}

	void StreamBuffer::UpdateCurrentFencePosition()
	{
		// Has the offset changed since the last fence?
		const u64 counter = g_vulkan_context->GetCurrentFenceCounter();
		if (!m_tracked_fences.empty() && m_tracked_fences.back().first == counter)
		{
			// Still haven't executed a command buffer, so just update the offset.
			m_tracked_fences.back().second = m_current_offset;
			return;
		}

		// New buffer, so update the GPU position while we're at it.
		m_tracked_fences.emplace_back(counter, m_current_offset);
	}

	void StreamBuffer::UpdateGPUPosition()
	{
		auto start = m_tracked_fences.begin();
		auto end = start;

		const u64 completed_counter = g_vulkan_context->GetCompletedFenceCounter();
		while (end != m_tracked_fences.end() && completed_counter >= end->first)
		{
			m_current_gpu_position = end->second;
			++end;
		}

		if (start != end)
		{
			m_tracked_fences.erase(start, end);
			if (m_current_offset == m_current_gpu_position)
			{
				// GPU is all caught up now.
				m_current_offset = 0;
				m_current_gpu_position = 0;
				m_current_space = m_size;
			}
		}
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
					new_space = gpu_position - 1;
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
					new_space = available_space_inbetween - 1;
					new_gpu_position = gpu_position;
					break;
				}
			}
		}

		// Did any fences satisfy this condition?
		// Has the command buffer been executed yet? If not, the caller should execute it.
		if (iter == m_tracked_fences.end() || iter->first == g_vulkan_context->GetCurrentFenceCounter())
			return false;

		// Wait until this fence is signaled. This will fire the callback, updating the GPU position.
		g_vulkan_context->WaitForFenceCounter(iter->first);
		m_tracked_fences.erase(
			m_tracked_fences.begin(), m_current_offset == iter->second ? m_tracked_fences.end() : ++iter);
		m_current_offset = new_offset;
		m_current_space = new_space;
		m_current_gpu_position = new_gpu_position;
		return true;
	}

} // namespace Vulkan
