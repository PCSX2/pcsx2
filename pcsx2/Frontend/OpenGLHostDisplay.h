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

#include <glad.h>

#include "HostDisplay.h"
#include "common/GL/Context.h"
#include "common/WindowInfo.h"
#include <memory>

class OpenGLHostDisplay final : public HostDisplay
{
public:
	OpenGLHostDisplay();
	~OpenGLHostDisplay();

	RenderAPI GetRenderAPI() const override;
	void* GetRenderDevice() const override;
	void* GetRenderContext() const override;
	void* GetRenderSurface() const override;

	bool HasRenderDevice() const override;
	bool HasRenderSurface() const override;

	bool CreateRenderDevice(const WindowInfo& wi, std::string_view adapter_name, bool threaded_presentation, bool debug_device) override;
	bool InitializeRenderDevice(std::string_view shader_cache_directory, bool debug_device) override;
	void DestroyRenderDevice() override;

	bool MakeRenderContextCurrent() override;
	bool DoneRenderContextCurrent() override;

	bool ChangeRenderWindow(const WindowInfo& new_wi) override;
	void ResizeRenderWindow(s32 new_window_width, s32 new_window_height, float new_window_scale) override;
	bool SupportsFullscreen() const override;
	bool IsFullscreen() override;
	bool SetFullscreen(bool fullscreen, u32 width, u32 height, float refresh_rate) override;
	AdapterAndModeList GetAdapterAndModeList() override;
	void DestroyRenderSurface() override;
	std::string GetDriverInfo() const override;

	std::unique_ptr<HostDisplayTexture> CreateTexture(u32 width, u32 height, u32 layers, u32 levels, const void* data,
		u32 data_stride, bool dynamic) override;
	void UpdateTexture(HostDisplayTexture* texture, u32 x, u32 y, u32 width, u32 height, const void* texture_data,
		u32 texture_data_stride) override;

	void SetVSync(VsyncMode mode) override;

	bool BeginPresent(bool frame_skip) override;
	void EndPresent() override;

protected:
	const char* GetGLSLVersionString() const;
	std::string GetGLSLVersionHeader() const;

	bool CreateImGuiContext() override;
	void DestroyImGuiContext() override;
	bool UpdateImGuiFontTexture() override;

	std::unique_ptr<GL::Context> m_gl_context;

	VsyncMode m_vsync_mode = VsyncMode::Off;
};

