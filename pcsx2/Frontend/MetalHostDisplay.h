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
#include <AppKit/AppKit.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

class MetalHostDisplay final : public HostDisplay
{
	MRCOwned<NSView*> m_view;
	MRCOwned<CAMetalLayer*> m_layer;
	GSMTLDevice m_dev;
	MRCOwned<id<MTLCommandQueue>> m_queue;
	MRCOwned<id<MTLTexture>> m_font_tex;
	MRCOwned<id<CAMetalDrawable>> m_current_drawable;
	MRCOwned<MTLRenderPassDescriptor*> m_pass_desc;
	u32 m_capture_start_frame;

	void AttachSurfaceOnMainThread();
	void DetachSurfaceOnMainThread();

public:
	MetalHostDisplay();
	~MetalHostDisplay();
	RenderAPI GetRenderAPI() const override;
	void* GetRenderDevice() const override;
	void* GetRenderContext() const override;
	void* GetRenderSurface() const override;

	bool HasRenderDevice() const override;
	bool HasRenderSurface() const override;
	bool CreateRenderDevice(const WindowInfo& wi, std::string_view adapter_name, VsyncMode vsync, bool threaded_presentation, bool debug_device) override;
	bool InitializeRenderDevice(std::string_view shader_cache_directory, bool debug_device) override;
	bool MakeRenderContextCurrent() override;
	bool DoneRenderContextCurrent() override;
	void DestroyRenderDevice() override;
	void DestroyRenderSurface() override;
	bool ChangeRenderWindow(const WindowInfo& wi) override;
	bool SupportsFullscreen() const override;
	bool IsFullscreen() override;
	bool SetFullscreen(bool fullscreen, u32 width, u32 height, float refresh_rate) override;
	AdapterAndModeList GetAdapterAndModeList() override;
	std::string GetDriverInfo() const override;

	void ResizeRenderWindow(s32 new_window_width, s32 new_window_height, float new_window_scale) override;

	std::unique_ptr<HostDisplayTexture> CreateTexture(u32 width, u32 height, const void* data, u32 data_stride, bool dynamic = false) override;
	void UpdateTexture(id<MTLTexture> texture, u32 x, u32 y, u32 width, u32 height, const void* data, u32 data_stride);
	void UpdateTexture(HostDisplayTexture* texture, u32 x, u32 y, u32 width, u32 height, const void* data, u32 data_stride) override;
	bool BeginPresent(bool frame_skip) override;
	void EndPresent() override;
	void SetVSync(VsyncMode mode) override;

	bool CreateImGuiContext() override;
	void DestroyImGuiContext() override;
	bool UpdateImGuiFontTexture() override;

	bool GetHostRefreshRate(float* refresh_rate) override;
};

#endif
