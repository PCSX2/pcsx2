// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Libretro Vulkan context sharing (the lrps2-libretro pattern).
//
// When a libretro frontend drives the GS, the VkInstance (and optionally the
// VkPhysicalDevice) come from the frontend's context-negotiation interface,
// and the VkDevice we create must (a) enable every extension/layer/feature
// the frontend requires and (b) serialise vkQueueSubmit against the
// frontend's own use of the shared graphics queue. Rather than teach
// GSDeviceVK about any of this, the loader's global vkGetInstanceProcAddr is
// swapped for a wrapper that transparently intercepts vkCreateDevice (to
// merge the frontend requirements) and vkQueueSubmit (to lock the queue via
// the retro_hw_render_interface_vulkan). GSDeviceVK only needs two explicit
// branches: adopt the provided instance/gpu instead of creating its own, and
// skip surface/swapchain creation (presentation is handed off through
// set_image from the frontend side).

#pragma once

#include "VKLoader.h"

#include "common/Pcsx2Types.h"

namespace VKLibretro
{
	// True when the libretro frontend owns the display; checked by
	// GSDeviceVK during init/present.
	extern bool Active;

	// Largest output canvas the core will produce: 4x-upscaled PAL expanded
	// to 4:3. Advertised to the frontend as retro_game_geometry
	// max_width/max_height and enforced when the present path sizes the
	// canvas to the merged frame.
	static constexpr u32 kMaxCanvasWidth = 2732;
	static constexpr u32 kMaxCanvasHeight = 2048;

	struct InitInfo
	{
		VkInstance instance = VK_NULL_HANDLE;
		VkPhysicalDevice gpu = VK_NULL_HANDLE;
		VkDevice device = VK_NULL_HANDLE; // filled by the wrapped vkCreateDevice
		PFN_vkGetInstanceProcAddr get_instance_proc_addr = nullptr;
		const char** required_device_extensions = nullptr;
		unsigned num_required_device_extensions = 0;
		const char** required_device_layers = nullptr;
		unsigned num_required_device_layers = 0;
		const VkPhysicalDeviceFeatures* required_features = nullptr;
	};
	extern InitInfo Init;

	// Swap the loader's global vkGetInstanceProcAddr for the intercepting
	// wrapper. Call after Vulkan::LoadVulkanLibrary(), before GS opens.
	void InstallWraps();

	// The retro_hw_render_interface_vulkan acquired after context_reset
	// (stored as void* so this header stays free of libretro headers).
	void SetHWRenderInterface(void* iface);
	void* GetHWRenderInterface();

	// GS-thread -> retro_run frame handoff. The GS present path publishes the
	// current display texture; retro_run consumes it (at most one per call)
	// and forwards it to the frontend via set_image + video_cb.
	struct Frame
	{
		VkImage image = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
		VkFormat format = VK_FORMAT_UNDEFINED;
		u32 width = 0;
		u32 height = 0;
	};
	void PublishFrame(const Frame& frame);
	bool ConsumeFrame(Frame* out_frame); // true if a new frame arrived since the last consume

	// Frame pacing: when enabled, PublishFrame blocks the GS thread until
	// retro_run consumes the frame — the frontend's retro_run cadence becomes
	// the emulation's vsync. Abort before shutdown so the GS thread can't be
	// left parked.
	void SetPacing(bool enabled);
	void AbortPacing();

	void Shutdown();
} // namespace VKLibretro
