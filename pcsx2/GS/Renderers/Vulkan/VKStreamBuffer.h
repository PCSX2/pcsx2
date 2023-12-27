// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GS/Renderers/Vulkan/VKLoader.h"

#include "vk_mem_alloc.h"

#include <deque>
#include <memory>

class VKStreamBuffer
{
public:
	VKStreamBuffer();
	VKStreamBuffer(VKStreamBuffer&& move);
	VKStreamBuffer(const VKStreamBuffer&) = delete;
	~VKStreamBuffer();

	VKStreamBuffer& operator=(VKStreamBuffer&& move);
	VKStreamBuffer& operator=(const VKStreamBuffer&) = delete;

	__fi bool IsValid() const { return (m_buffer != VK_NULL_HANDLE); }
	__fi VkBuffer GetBuffer() const { return m_buffer; }
	__fi const VkBuffer* GetBufferPtr() const { return &m_buffer; }
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
