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
#include "common/Vulkan/Texture.h"
#include "common/Vulkan/Loader.h"
#include <memory>
#include <vector>

namespace Vulkan
{
	class SwapChain
	{
	public:
		SwapChain(const WindowInfo& wi, VkSurfaceKHR surface, VkPresentModeKHR preferred_present_mode);
		~SwapChain();

		// Creates a vulkan-renderable surface for the specified window handle.
		static VkSurfaceKHR CreateVulkanSurface(VkInstance instance, VkPhysicalDevice physical_device, WindowInfo* wi);

		// Destroys a previously-created surface.
		static void DestroyVulkanSurface(VkInstance instance, WindowInfo* wi, VkSurfaceKHR surface);

		// Enumerates fullscreen modes for window info.
		struct FullscreenModeInfo
		{
			u32 width;
			u32 height;
			float refresh_rate;
		};
		static std::vector<FullscreenModeInfo> GetSurfaceFullscreenModes(
			VkInstance instance, VkPhysicalDevice physical_device, const WindowInfo& wi);

		// Create a new swap chain from a pre-existing surface.
		static std::unique_ptr<SwapChain> Create(const WindowInfo& wi, VkSurfaceKHR surface,
			VkPresentModeKHR preferred_present_mode);

		__fi VkSurfaceKHR GetSurface() const { return m_surface; }
		__fi VkSurfaceFormatKHR GetSurfaceFormat() const { return m_surface_format; }
		__fi VkFormat GetTextureFormat() const { return m_surface_format.format; }
		__fi VkPresentModeKHR GetPreferredPresentMode() const { return m_preferred_present_mode; }
		__fi VkSwapchainKHR GetSwapChain() const { return m_swap_chain; }
		__fi const WindowInfo& GetWindowInfo() const { return m_window_info; }
		__fi u32 GetWidth() const { return m_window_info.surface_width; }
		__fi u32 GetHeight() const { return m_window_info.surface_height; }
		__fi u32 GetCurrentImageIndex() const { return m_current_image; }
		__fi u32 GetImageCount() const { return static_cast<u32>(m_images.size()); }
		__fi VkImage GetCurrentImage() const { return m_images[m_current_image].image; }
		__fi const Texture& GetCurrentTexture() const { return m_images[m_current_image].texture; }
		__fi Texture& GetCurrentTexture() { return m_images[m_current_image].texture; }
		__fi VkFramebuffer GetCurrentFramebuffer() const { return m_images[m_current_image].framebuffer; }
		__fi VkRenderPass GetLoadRenderPass() const { return m_load_render_pass; }
		__fi VkRenderPass GetClearRenderPass() const { return m_clear_render_pass; }
		__fi VkSemaphore GetImageAvailableSemaphore() const { return m_image_available_semaphore; }
		__fi VkSemaphore GetRenderingFinishedSemaphore() const { return m_rendering_finished_semaphore; }
		VkResult AcquireNextImage();

		bool RecreateSurface(const WindowInfo& new_wi);
		bool ResizeSwapChain(u32 new_width = 0, u32 new_height = 0);
		bool RecreateSwapChain();

		// Change vsync enabled state. This may fail as it causes a swapchain recreation.
		bool SetVSync(VkPresentModeKHR preferred_mode);

		// Returns true if the current present mode is synchronizing (adaptive or hard).
		bool IsPresentModeSynchronizing() const
		{
			return (m_present_mode == VK_PRESENT_MODE_FIFO_KHR || m_present_mode == VK_PRESENT_MODE_FIFO_RELAXED_KHR);
		}

	private:
		bool SelectSurfaceFormat();
		bool SelectPresentMode();

		bool CreateSwapChain();
		void DestroySwapChain();

		bool SetupSwapChainImages();
		void DestroySwapChainImages();

		void DestroySurface();

		bool CreateSemaphores();
		void DestroySemaphores();

		struct SwapChainImage
		{
			VkImage image;
			Texture texture;
			VkFramebuffer framebuffer;
		};

		WindowInfo m_window_info;

		VkSurfaceKHR m_surface = VK_NULL_HANDLE;
		VkSurfaceFormatKHR m_surface_format = {};
		VkPresentModeKHR m_preferred_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
		VkPresentModeKHR m_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;

		VkRenderPass m_load_render_pass = VK_NULL_HANDLE;
		VkRenderPass m_clear_render_pass = VK_NULL_HANDLE;

		VkSemaphore m_image_available_semaphore = VK_NULL_HANDLE;
		VkSemaphore m_rendering_finished_semaphore = VK_NULL_HANDLE;

		VkSwapchainKHR m_swap_chain = VK_NULL_HANDLE;
		std::vector<SwapChainImage> m_images;
		u32 m_current_image = 0;
	};
} // namespace Vulkan
