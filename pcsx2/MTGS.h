/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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

#include "GS.h"

#include "common/Threading.h"

#include <functional>

/////////////////////////////////////////////////////////////////////////////
// MTGS Threaded Class Declaration

// Uncomment this to enable the MTGS debug stack, which tracks to ensure reads
// and writes stay synchronized.  Warning: the debug stack is VERY slow.
//#define RINGBUF_DEBUG_STACK

namespace MTGS
{
	using AsyncCallType = std::function<void()>;

	enum class Command : u32
	{
		GIFPath1,
		GIFPath2,
		GIFPath3,
		VSync,
		Freeze,
		Reset, // issues a GSreset() command.
		SoftReset, // issues a soft reset for the GIF
		GSPacket,
		MTVUGSPacket,
		InitAndReadFIFO,
		AsyncCall,
	};

	struct FreezeData
	{
		freezeData* fdata;
		s32 retval; // value returned from the call, valid only after an mtgsWaitGS()
	};

	const Threading::ThreadHandle& GetThreadHandle();
	bool IsOpen();

	/// Starts the thread, if it hasn't already been started.
	void StartThread();

	/// Fully stops the thread, closing in the process if needed.
	void ShutdownThread();

	/// Re-presents the current frame. Call when things like window resizes happen to re-display
	/// the current frame with the correct proportions. Should only be called from the CPU thread.
	void PresentCurrentFrame();

	// Waits for the GS to empty out the entire ring buffer contents.
	void WaitGS(bool syncRegs = true, bool weakWait = false, bool isMTVU = false);
	void ResetGS(bool hardware_reset);

	bool WaitForOpen();
	void WaitForClose();
	void Freeze(FreezeAction mode, FreezeData& data);

	void PostVsyncStart(bool registers_written);
	void InitAndReadFIFO(u8* mem, u32 qwc);

	void RunOnGSThread(AsyncCallType func);
	void GameChanged();
	void ApplySettings();
	void ResizeDisplayWindow(int width, int height, float scale);
	void UpdateDisplayWindow();
	void SetVSyncMode(VsyncMode mode);
	void UpdateVSyncMode();
	void SwitchRenderer(GSRendererType renderer, bool display_message = true);
	void SetSoftwareRendering(bool software, bool display_message = true);
	void ToggleSoftwareRendering();
	bool SaveMemorySnapshot(u32 window_width, u32 window_height, bool apply_aspect, bool crop_borders,
		u32* width, u32* height, std::vector<u32>* pixels);
	void SetRunIdle(bool enabled);

	// Size of the ringbuffer as a power of 2 -- size is a multiple of simd128s.
	// (actual size is 1<<m_RingBufferSizeFactor simd vectors [128-bit values])
	// A value of 19 is a 8meg ring buffer.  18 would be 4 megs, and 20 would be 16 megs.
	// Default was 2mb, but some games with lots of MTGS activity want 8mb to run fast (rama)
	static const uint RingBufferSizeFactor = 19;

	// size of the ringbuffer in simd128's.
	static const uint RingBufferSize = 1 << RingBufferSizeFactor;

	// Mask to apply to ring buffer indices to wrap the pointer from end to
	// start (the wrapping is what makes it a ringbuffer, yo!)
	static const uint RingBufferMask = RingBufferSize - 1;
}
