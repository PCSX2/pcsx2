// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "Config.h"
#include "GS/Renderers/Vulkan/GSTextureVK.h"

#include "common/WindowInfo.h"

#include <array>
#include <memory>
#include <optional>
#include <vector>

class VKSwapChain
{
public:
	// We don't actually need +1 semaphores, or, more than one really.
	// But, the validation layer gets cranky if we don't fence wait before the next image acquire.
	// So, add an additional semaphore to ensure that we're never acquiring before fence waiting.
	static constexpr u32 NUM_SEMAPHORES = 4; // Should be command buffers + 1

	~VKSwapChain();

	// Creates a vulkan-renderable surface for the specified window handle.
	static VkSurfaceKHR CreateVulkanSurface(VkInstance instance, VkPhysicalDevice physical_device, WindowInfo* wi);

	// Destroys a previously-created surface.
	static void DestroyVulkanSurface(VkInstance instance, WindowInfo* wi, VkSurfaceKHR surface);

	// Create a new swap chain from a pre-existing surface.
	static std::unique_ptr<VKSwapChain> Create(const WindowInfo& wi, VkSurfaceKHR surface, VkPresentModeKHR present_mode,
		std::optional<bool> exclusive_fullscreen_control);

	/// Returns the Vulkan present mode for a given vsync mode that is compatible with this device.
	static bool SelectPresentMode(VkSurfaceKHR surface, GSVSyncMode* vsync_mode, VkPresentModeKHR* present_mode);

	__fi VkSurfaceKHR GetSurface() const { return m_surface; }
	__fi VkSwapchainKHR GetSwapChain() const { return m_swap_chain; }
	__fi const VkSwapchainKHR* GetSwapChainPtr() const { return &m_swap_chain; }
	__fi const WindowInfo& GetWindowInfo() const { return m_window_info; }
	__fi u32 GetWidth() const { return m_window_info.surface_width; }
	__fi u32 GetHeight() const { return m_window_info.surface_height; }
	__fi float GetScale() const { return m_window_info.surface_scale; }
	__fi u32 GetCurrentImageIndex() const { return m_current_image; }
	__fi const u32* GetCurrentImageIndexPtr() const { return &m_current_image; }
	__fi u32 GetImageCount() const { return static_cast<u32>(m_images.size()); }
	__fi const GSTextureVK* GetCurrentTexture() const { return m_images[m_current_image].get(); }
	__fi GSTextureVK* GetCurrentTexture() { return m_images[m_current_image].get(); }
	__fi VkSemaphore GetImageAvailableSemaphore() const
	{
		return m_semaphores[m_current_semaphore].available_semaphore;
	}
	__fi const VkSemaphore* GetImageAvailableSemaphorePtr() const
	{
		return &m_semaphores[m_current_semaphore].available_semaphore;
	}
	__fi VkSemaphore GetRenderingFinishedSemaphore() const
	{
		return m_semaphores[m_current_semaphore].rendering_finished_semaphore;
	}
	__fi const VkSemaphore* GetRenderingFinishedSemaphorePtr() const
	{
		return &m_semaphores[m_current_semaphore].rendering_finished_semaphore;
	}

	VkFormat GetTextureFormat() const;
	VkResult AcquireNextImage();
	void ReleaseCurrentImage();

	bool RecreateSurface(const WindowInfo& new_wi);
	bool ResizeSwapChain(u32 new_width = 0, u32 new_height = 0, float new_scale = 1.0f);

	// Change vsync enabled state. This may fail as it causes a swapchain recreation.
	bool SetPresentMode(VkPresentModeKHR present_mode);

private:
	VKSwapChain(const WindowInfo& wi, VkSurfaceKHR surface, VkPresentModeKHR present_mode,
		std::optional<bool> exclusive_fullscreen_control);

	static std::optional<VkSurfaceFormatKHR> SelectSurfaceFormat(VkSurfaceKHR surface);

	bool CreateSwapChain();
	void DestroySwapChain();
	void DestroySwapChainImages();

	void DestroySurface();

	struct ImageSemaphores
	{
		VkSemaphore available_semaphore;
		VkSemaphore rendering_finished_semaphore;
	};

	WindowInfo m_window_info;

	VkSurfaceKHR m_surface = VK_NULL_HANDLE;
	VkSwapchainKHR m_swap_chain = VK_NULL_HANDLE;

	std::vector<std::unique_ptr<GSTextureVK>> m_images;
	std::array<ImageSemaphores, NUM_SEMAPHORES> m_semaphores = {};

	u32 m_current_image = 0;
	u32 m_current_semaphore = 0;

	VkPresentModeKHR m_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;

	std::optional<VkResult> m_image_acquire_result;
	std::optional<bool> m_exclusive_fullscreen_control;
};
