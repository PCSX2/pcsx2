/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022 PCSX2 Dev Team
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

#include "HostDisplay.h"

#ifndef __OBJC__
	#error "This header is for use with Objective-C++ only.
#endif

#ifdef __APPLE__

#include "GS/Renderers/Metal/GSMTLDeviceInfo.h"
#include <mutex>
#include <AppKit/AppKit.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

class MetalHostDisplay final : public HostDisplay
{
	enum class UsePresentDrawable : u8
	{
		Never = 0,
		Always = 1,
		IfVsync = 2,
	};
	MRCOwned<NSView*> m_view;
	MRCOwned<CAMetalLayer*> m_layer;
	GSMTLDevice m_dev;
	MRCOwned<id<MTLCommandQueue>> m_queue;
	MRCOwned<id<MTLTexture>> m_font_tex;
	MRCOwned<id<CAMetalDrawable>> m_current_drawable;
	MRCOwned<MTLRenderPassDescriptor*> m_pass_desc;
	u32 m_capture_start_frame;
	UsePresentDrawable m_use_present_drawable;
	bool m_gpu_timing_enabled = false;
	double m_accumulated_gpu_time = 0;
	double m_last_gpu_time_end = 0;
	std::mutex m_mtx;

	void AttachSurfaceOnMainThread();
	void DetachSurfaceOnMainThread();

public:
	MetalHostDisplay();
	~MetalHostDisplay();
	RenderAPI GetRenderAPI() const override;
	void* GetDevice() const override;
	void* GetContext() const override;
	void* GetSurface() const override;

	bool HasDevice() const override;
	bool HasSurface() const override;
	bool CreateDevice(const WindowInfo& wi, VsyncMode vsync) override;
	bool SetupDevice() override;
	bool MakeCurrent() override;
	bool DoneCurrent() override;
	void DestroySurface() override;
	bool ChangeWindow(const WindowInfo& wi) override;
	bool SupportsFullscreen() const override;
	bool IsFullscreen() override;
	bool SetFullscreen(bool fullscreen, u32 width, u32 height, float refresh_rate) override;
	AdapterAndModeList GetAdapterAndModeList() override;
	std::string GetDriverInfo() const override;

	void ResizeWindow(s32 new_window_width, s32 new_window_height, float new_window_scale) override;

	std::unique_ptr<HostDisplayTexture> CreateTexture(u32 width, u32 height, const void* data, u32 data_stride, bool dynamic = false) override;
	void UpdateTexture(id<MTLTexture> texture, u32 x, u32 y, u32 width, u32 height, const void* data, u32 data_stride);
	void UpdateTexture(HostDisplayTexture* texture, u32 x, u32 y, u32 width, u32 height, const void* data, u32 data_stride) override;
	PresentResult BeginPresent(bool frame_skip) override;
	void EndPresent() override;
	void SetVSync(VsyncMode mode) override;

	bool CreateImGuiContext() override;
	void DestroyImGuiContext() override;
	bool UpdateImGuiFontTexture() override;

	bool GetHostRefreshRate(float* refresh_rate) override;

	bool SetGPUTimingEnabled(bool enabled) override;
	float GetAndResetAccumulatedGPUTime() override;
	void AccumulateCommandBufferTime(id<MTLCommandBuffer> buffer);
};

#endif
