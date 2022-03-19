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
#include "common/WindowInfo.h"

#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "Host.h"
#include "Config.h"

/// An abstracted RGBA8 texture.
class HostDisplayTexture
{
public:
	virtual ~HostDisplayTexture();

	virtual void* GetHandle() const = 0;
	virtual u32 GetWidth() const = 0;
	virtual u32 GetHeight() const = 0;
};

/// Interface to the frontend's renderer.
class HostDisplay
{
public:
	enum class RenderAPI
	{
		None,
		D3D11,
		Metal,
		D3D12,
		Vulkan,
		OpenGL,
		OpenGLES
	};

	enum class Alignment
	{
		LeftOrTop,
		Center,
		RightOrBottom
	};

	struct AdapterAndModeList
	{
		std::vector<std::string> adapter_names;
		std::vector<std::string> fullscreen_modes;
	};

	virtual ~HostDisplay();

	/// Returns a string representing the specified API.
	static const char* RenderAPIToString(RenderAPI api);

	/// Creates a display for the specified API.
	static std::unique_ptr<HostDisplay> CreateDisplayForAPI(RenderAPI api);

	/// Parses a fullscreen mode into its components (width * height @ refresh hz)
	static bool ParseFullscreenMode(const std::string_view& mode, u32* width, u32* height, float* refresh_rate);

	/// Converts a fullscreen mode to a string.
	static std::string GetFullscreenModeString(u32 width, u32 height, float refresh_rate);

	__fi const WindowInfo& GetWindowInfo() const { return m_window_info; }
	__fi s32 GetWindowWidth() const { return static_cast<s32>(m_window_info.surface_width); }
	__fi s32 GetWindowHeight() const { return static_cast<s32>(m_window_info.surface_height); }
	__fi float GetWindowScale() const { return m_window_info.surface_scale; }

	/// Changes the alignment for this display (screen positioning).
	__fi Alignment GetDisplayAlignment() const { return m_display_alignment; }
	__fi void SetDisplayAlignment(Alignment alignment) { m_display_alignment = alignment; }

	virtual RenderAPI GetRenderAPI() const = 0;
	virtual void* GetRenderDevice() const = 0;
	virtual void* GetRenderContext() const = 0;
	virtual void* GetRenderSurface() const = 0;

	virtual bool HasRenderDevice() const = 0;
	virtual bool HasRenderSurface() const = 0;

	virtual bool CreateRenderDevice(const WindowInfo& wi, std::string_view adapter_name, VsyncMode vsync, bool threaded_presentation, bool debug_device) = 0;
	virtual bool InitializeRenderDevice(std::string_view shader_cache_directory, bool debug_device) = 0;
	virtual bool MakeRenderContextCurrent() = 0;
	virtual bool DoneRenderContextCurrent() = 0;
	virtual void DestroyRenderDevice() = 0;
	virtual void DestroyRenderSurface() = 0;
	virtual bool ChangeRenderWindow(const WindowInfo& wi) = 0;
	virtual bool SupportsFullscreen() const = 0;
	virtual bool IsFullscreen() = 0;
	virtual bool SetFullscreen(bool fullscreen, u32 width, u32 height, float refresh_rate) = 0;
	virtual AdapterAndModeList GetAdapterAndModeList() = 0;
	virtual std::string GetDriverInfo() const = 0;

	/// Call when the window size changes externally to recreate any resources.
	virtual void ResizeRenderWindow(s32 new_window_width, s32 new_window_height, float new_window_scale) = 0;

	/// Creates an abstracted RGBA8 texture. If dynamic, the texture can be updated with UpdateTexture() below.
	virtual std::unique_ptr<HostDisplayTexture> CreateTexture(u32 width, u32 height, const void* data, u32 data_stride, bool dynamic = false) = 0;
	virtual void UpdateTexture(HostDisplayTexture* texture, u32 x, u32 y, u32 width, u32 height, const void* data, u32 data_stride) = 0;

	/// Returns false if the window was completely occluded. If frame_skip is set, the frame won't be
	/// displayed, but the GPU command queue will still be flushed.
	virtual bool BeginPresent(bool frame_skip) = 0;

	/// Presents the frame to the display, and renders OSD elements.
	virtual void EndPresent() = 0;

	/// Changes vsync mode for this display.
	virtual void SetVSync(VsyncMode mode) = 0;

	/// ImGui context management, usually called by derived classes.
	virtual bool CreateImGuiContext() = 0;
	virtual void DestroyImGuiContext() = 0;
	virtual bool UpdateImGuiFontTexture() = 0;

	/// Returns the effective refresh rate of this display.
	virtual bool GetHostRefreshRate(float* refresh_rate);

	/// Enables/disables GPU frame timing.
	virtual void SetGPUTimingEnabled(bool enabled);

	/// Returns the amount of GPU time utilized since the last time this method was called.
	virtual float GetAndResetAccumulatedGPUTime();

	/// Returns true if it's an OpenGL-based renderer.
	bool UsesLowerLeftOrigin() const;

protected:
	WindowInfo m_window_info;
	Alignment m_display_alignment = Alignment::Center;
	VsyncMode m_vsync_mode = VsyncMode::Off;
};

namespace Host
{
	/// Creates the host display. This may create a new window. The API used depends on the current configuration.
	HostDisplay* AcquireHostDisplay(HostDisplay::RenderAPI api);

	/// Destroys the host display. This may close the display window.
	void ReleaseHostDisplay();

	/// Returns a pointer to the current host display abstraction. Assumes AcquireHostDisplay() has been caled.
	HostDisplay* GetHostDisplay();

	/// Returns false if the window was completely occluded. If frame_skip is set, the frame won't be
	/// displayed, but the GPU command queue will still be flushed.
	bool BeginPresentFrame(bool frame_skip);

	/// Presents the frame to the display, and renders OSD elements.
	void EndPresentFrame();

	/// Called on the MTGS thread when a resize request is received.
	void ResizeHostDisplay(u32 new_window_width, u32 new_window_height, float new_window_scale);

	/// Called on the MTGS thread when a request to update the display is received.
	/// This could be a fullscreen transition, for example.
	void UpdateHostDisplay();
} // namespace Host

