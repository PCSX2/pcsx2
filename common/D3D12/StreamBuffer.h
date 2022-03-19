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
#include "common/RedtapeWindows.h"

#include <d3d12.h>
#include <deque>
#include <utility>
#include <wil/com.h>

namespace D3D12MA
{
	class Allocation;
}

namespace D3D12
{
	class StreamBuffer
	{
	public:
		StreamBuffer();
		~StreamBuffer();

		bool Create(u32 size);

		__fi bool IsValid() const { return static_cast<bool>(m_buffer); }
		__fi ID3D12Resource* GetBuffer() const { return m_buffer.get(); }
		__fi D3D12_GPU_VIRTUAL_ADDRESS GetGPUPointer() const { return m_gpu_pointer; }
		__fi void* GetHostPointer() const { return m_host_pointer; }
		__fi void* GetCurrentHostPointer() const { return m_host_pointer + m_current_offset; }
		__fi D3D12_GPU_VIRTUAL_ADDRESS GetCurrentGPUPointer() const { return m_gpu_pointer + m_current_offset; }
		__fi u32 GetSize() const { return m_size; }
		__fi u32 GetCurrentOffset() const { return m_current_offset; }
		__fi u32 GetCurrentSpace() const { return m_current_space; }

		bool ReserveMemory(u32 num_bytes, u32 alignment);
		void CommitMemory(u32 final_num_bytes);

		void Destroy(bool defer = true);

	private:
		void UpdateCurrentFencePosition();
		void UpdateGPUPosition();

		// Waits for as many fences as needed to allocate num_bytes bytes from the buffer.
		bool WaitForClearSpace(u32 num_bytes);

		u32 m_size = 0;
		u32 m_current_offset = 0;
		u32 m_current_space = 0;
		u32 m_current_gpu_position = 0;

		wil::com_ptr_nothrow<ID3D12Resource> m_buffer;
		wil::com_ptr_nothrow<D3D12MA::Allocation> m_allocation;
		D3D12_GPU_VIRTUAL_ADDRESS m_gpu_pointer = {};
		u8* m_host_pointer = nullptr;

		// List of fences and the corresponding positions in the buffer
		std::deque<std::pair<u64, u32>> m_tracked_fences;
	};

} // namespace D3D12
