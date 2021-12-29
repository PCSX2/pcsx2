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

#pragma once

#include "common/Pcsx2Defs.h"
#include "common/Vulkan/Loader.h"
#include "vk_mem_alloc.h"
#include <deque>
#include <memory>

namespace Vulkan
{
	class StreamBuffer
	{
	public:
		StreamBuffer();
		StreamBuffer(StreamBuffer&& move);
		StreamBuffer(const StreamBuffer&) = delete;
		~StreamBuffer();

		StreamBuffer& operator=(StreamBuffer&& move);
		StreamBuffer& operator=(const StreamBuffer&) = delete;

		__fi bool IsValid() const { return (m_buffer != VK_NULL_HANDLE); }
		__fi VkBuffer GetBuffer() const { return m_buffer; }
		__fi u8* GetHostPointer() const { return m_host_pointer; }
		__fi u8* GetCurrentHostPointer() const { return m_host_pointer + m_current_offset; }
		__fi u32 GetCurrentSize() const { return m_size; }
		__fi u32 GetCurrentSpace() const { return m_current_space; }
		__fi u32 GetCurrentOffset() const { return m_current_offset; }

		bool Create(VkBufferUsageFlags usage, u32 size);
		void Destroy(bool defer);

		bool ReserveMemory(u32 num_bytes, u32 alignment);
		void CommitMemory(u32 final_num_bytes);

	private:
		bool AllocateBuffer(VkBufferUsageFlags usage, u32 size);
		void UpdateCurrentFencePosition();
		void UpdateGPUPosition();

		// Waits for as many fences as needed to allocate num_bytes bytes from the buffer.
		bool WaitForClearSpace(u32 num_bytes);

		u32 m_size = 0;
		u32 m_current_offset = 0;
		u32 m_current_space = 0;
		u32 m_current_gpu_position = 0;

		VmaAllocation m_allocation = VK_NULL_HANDLE;
		VkBuffer m_buffer = VK_NULL_HANDLE;
		u8* m_host_pointer = nullptr;

		// List of fences and the corresponding positions in the buffer
		std::deque<std::pair<u64, u32>> m_tracked_fences;
	};
} // namespace Vulkan
