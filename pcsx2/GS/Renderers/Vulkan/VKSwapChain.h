// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "Config.h"
#include "GS/Renderers/Vulkan/GSTextureVK.h"

#include "common/WindowInfo.h"

#include <memory>
#include <optional>
#include <vector>

class VKSwapChain
{
public:
	~VKSwapChain();

	// Creates a vulkan-renderable surface for the specified window handle.
	static VkSurfaceKHR CreateVulkanSurface(VkInstance instance, VkPhysicalDevice physical_device, WindowInfo* wi);

	// Destroys a previously-created surface.
	static void DestroyVulkanSurface(VkInstance instance, WindowInfo* wi, VkSurfaceKHR surface);

	// Create a new swap chain from a pre-existing surface.
	static std::unique_ptr<VKSwapChain> Create(
		const WindowInfo& wi, VkSurfaceKHR surface, VsyncMode vsync, std::optional<bool> exclusive_fullscreen_control);

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

	// Returns true if the current present mode is synchronizing (adaptive or hard).
	__fi bool IsPresentModeSynchronizing() const { return (m_vsync_mode != VsyncMode::Off); }

	VkFormat GetTextureFormat() const;
	VkResult AcquireNextImage();
	void ReleaseCurrentImage();

	bool RecreateSurface(const WindowInfo& new_wi);
	bool ResizeSwapChain(u32 new_width = 0, u32 new_height = 0, float new_scale = 1.0f);

	// Change vsync enabled state. This may fail as it causes a swapchain recreation.
	bool SetVSync(VsyncMode mode);

private:
	VKSwapChain(
		const WindowInfo& wi, VkSurfaceKHR surface, VsyncMode vsync, std::optional<bool> exclusive_fullscreen_control);

	static std::optional<VkSurfaceFormatKHR> SelectSurfaceFormat(VkSurfaceKHR surface);
	static std::optional<VkPresentModeKHR> SelectPresentMode(VkSurfaceKHR surface, VsyncMode vsync);

	bool CreateSwapChain();
	void DestroySwapChain();

	bool SetupSwapChainImages(VkFormat image_format);
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
	std::vector<ImageSemaphores> m_semaphores;

	VsyncMode m_vsync_mode = VsyncMode::Off;
	u32 m_current_image = 0;
	u32 m_current_semaphore = 0;

	std::optional<VkResult> m_image_acquire_result;
	std::optional<bool> m_exclusive_fullscreen_control;
};
