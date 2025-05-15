// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/Renderers/Vulkan/GSDeviceVK.h"
#include "GS/Renderers/Vulkan/VKBuilders.h"
#include "GS/Renderers/Vulkan/VKSwapChain.h"
#include "VMManager.h"

#include "common/Assertions.h"
#include "common/CocoaTools.h"
#include "common/Console.h"

#include <algorithm>
#include <array>
#include <cmath>

#if defined(VK_USE_PLATFORM_XLIB_KHR)
#include <X11/Xlib.h>
#endif

static_assert(VKSwapChain::NUM_SEMAPHORES == (GSDeviceVK::NUM_COMMAND_BUFFERS + 1));

VKSwapChain::VKSwapChain(const WindowInfo& wi, VkSurfaceKHR surface, VkPresentModeKHR present_mode,
	std::optional<bool> exclusive_fullscreen_control)
	: m_window_info(wi)
	, m_surface(surface)
	, m_present_mode(present_mode)
	, m_exclusive_fullscreen_control(exclusive_fullscreen_control)
{
}

VKSwapChain::~VKSwapChain()
{
	DestroySwapChainImages();
	DestroySwapChain();
	DestroySurface();
}

VkSurfaceKHR VKSwapChain::CreateVulkanSurface(VkInstance instance, VkPhysicalDevice physical_device, WindowInfo* wi)
{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	if (wi->type == WindowInfo::Type::Win32)
	{
		VkWin32SurfaceCreateInfoKHR surface_create_info = {
			VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR, // VkStructureType               sType
			nullptr, // const void*                   pNext
			0, // VkWin32SurfaceCreateFlagsKHR  flags
			nullptr, // HINSTANCE                     hinstance
			reinterpret_cast<HWND>(wi->window_handle) // HWND                          hwnd
		};

		VkSurfaceKHR surface;
		VkResult res = vkCreateWin32SurfaceKHR(instance, &surface_create_info, nullptr, &surface);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateWin32SurfaceKHR failed: ");
			return VK_NULL_HANDLE;
		}

		return surface;
	}
#endif

#if defined(VK_USE_PLATFORM_XLIB_KHR)
	if (wi->type == WindowInfo::Type::X11)
	{
		VkXlibSurfaceCreateInfoKHR surface_create_info = {
			VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR, // VkStructureType               sType
			nullptr, // const void*                   pNext
			0, // VkXlibSurfaceCreateFlagsKHR   flags
			static_cast<Display*>(wi->display_connection), // Display*                      dpy
			reinterpret_cast<Window>(wi->window_handle) // Window                        window
		};

		VkSurfaceKHR surface;
		VkResult res = vkCreateXlibSurfaceKHR(instance, &surface_create_info, nullptr, &surface);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateXlibSurfaceKHR failed: ");
			return VK_NULL_HANDLE;
		}

		return surface;
	}
#endif

#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
	if (wi->type == WindowInfo::Type::Wayland)
	{
		VkWaylandSurfaceCreateInfoKHR surface_create_info = {VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR, nullptr,
			0, static_cast<struct wl_display*>(wi->display_connection),
			static_cast<struct wl_surface*>(wi->window_handle)};

		VkSurfaceKHR surface;
		VkResult res = vkCreateWaylandSurfaceKHR(instance, &surface_create_info, nullptr, &surface);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateWaylandSurfaceEXT failed: ");
			return VK_NULL_HANDLE;
		}

		return surface;
	}
#endif

#if defined(VK_USE_PLATFORM_METAL_EXT)
	if (wi->type == WindowInfo::Type::MacOS)
	{
		if (!wi->surface_handle && !CocoaTools::CreateMetalLayer(wi))
			return VK_NULL_HANDLE;

		VkMetalSurfaceCreateInfoEXT surface_create_info = {VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT, nullptr, 0,
			static_cast<const CAMetalLayer*>(wi->surface_handle)};

		VkSurfaceKHR surface;
		VkResult res = vkCreateMetalSurfaceEXT(instance, &surface_create_info, nullptr, &surface);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateMetalSurfaceEXT failed: ");
			return VK_NULL_HANDLE;
		}

		return surface;
	}
#endif

	return VK_NULL_HANDLE;
}

void VKSwapChain::DestroyVulkanSurface(VkInstance instance, WindowInfo* wi, VkSurfaceKHR surface)
{
	vkDestroySurfaceKHR(GSDeviceVK::GetInstance()->GetVulkanInstance(), surface, nullptr);

#if defined(__APPLE__)
	if (wi->type == WindowInfo::Type::MacOS && wi->surface_handle)
		CocoaTools::DestroyMetalLayer(wi);
#endif
}

std::unique_ptr<VKSwapChain> VKSwapChain::Create(const WindowInfo& wi, VkSurfaceKHR surface,
	VkPresentModeKHR present_mode, std::optional<bool> exclusive_fullscreen_control)
{
	std::unique_ptr<VKSwapChain> swap_chain =
		std::unique_ptr<VKSwapChain>(new VKSwapChain(wi, surface, present_mode, exclusive_fullscreen_control));
	if (!swap_chain->CreateSwapChain())
		return nullptr;

	return swap_chain;
}

static VkFormat GetLinearFormat(VkFormat format)
{
	switch (format)
	{
		case VK_FORMAT_R8_SRGB:
			return VK_FORMAT_R8_UNORM;
		case VK_FORMAT_R8G8_SRGB:
			return VK_FORMAT_R8G8_UNORM;
		case VK_FORMAT_R8G8B8_SRGB:
			return VK_FORMAT_R8G8B8_UNORM;
		case VK_FORMAT_R8G8B8A8_SRGB:
			return VK_FORMAT_R8G8B8A8_UNORM;
		case VK_FORMAT_B8G8R8_SRGB:
			return VK_FORMAT_B8G8R8_UNORM;
		case VK_FORMAT_B8G8R8A8_SRGB:
			return VK_FORMAT_B8G8R8A8_UNORM;
		default:
			return format;
	}
}

std::optional<VkSurfaceFormatKHR> VKSwapChain::SelectSurfaceFormat(VkSurfaceKHR surface)
{
	u32 format_count;
	VkResult res = vkGetPhysicalDeviceSurfaceFormatsKHR(
		GSDeviceVK::GetInstance()->GetPhysicalDevice(), surface, &format_count, nullptr);
	if (res != VK_SUCCESS || format_count == 0)
	{
		LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceSurfaceFormatsKHR failed: ");
		return std::nullopt;
	}

	std::vector<VkSurfaceFormatKHR> surface_formats(format_count);
	res = vkGetPhysicalDeviceSurfaceFormatsKHR(
		GSDeviceVK::GetInstance()->GetPhysicalDevice(), surface, &format_count, surface_formats.data());
	pxAssert(res == VK_SUCCESS);

	constexpr VkFormat default_sdr_format = VK_FORMAT_R8G8B8A8_UNORM;

	// If there is a single undefined surface format, the device doesn't care, so we'll just use RGBA
	if (surface_formats.empty() || surface_formats[0].format == VK_FORMAT_UNDEFINED)
		return VkSurfaceFormatKHR{default_sdr_format, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

	// HDR if enabled and supported
	if (EmuConfig.HDROutput)
	{
		for (const VkSurfaceFormatKHR& surface_format : surface_formats)
		{
			if (surface_format.format == VK_FORMAT_R16G16B16A16_SFLOAT && surface_format.colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT)
			{
				return surface_format;
			}
		}
		// Disable it as it's not supported
		EmuConfig.HDROutput = false;
		WARNING_LOG("Vulkan swapchain doesn't support HDR formats.");
	}

	// Use RGB10A2 if available (higher quality than RGBA8)
	for (const VkSurfaceFormatKHR& surface_format : surface_formats)
	{
		INFO_LOG("Vulkan swapchain supports format {} color space {}.", (u32)surface_format.format, (u32)surface_format.colorSpace);
		if (surface_format.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 && surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return surface_format;
		}
	}

	// Try to find a suitable format.
	for (const VkSurfaceFormatKHR& surface_format : surface_formats)
	{
		// We could easily support "VK_COLOR_SPACE_BT709_LINEAR_EXT" and "VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT" for SDR if we linearized on presentation,
		// but at the moment we don't.
		if (surface_format.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			continue;
		}
		// Some drivers seem to return a SRGB format here (Intel Mesa).
		// This results in gamma correction when presenting to the screen, which we don't want.
		// Use a non sRGB format instead, if this is the case.
		return VkSurfaceFormatKHR{GetLinearFormat(surface_format.format), VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
	}

	Console.Error("Failed to find a suitable format for swap chain buffers.");
	return std::nullopt;
}

static const char* PresentModeToString(VkPresentModeKHR mode)
{
	switch (mode)
	{
		case VK_PRESENT_MODE_IMMEDIATE_KHR:
			return "VK_PRESENT_MODE_IMMEDIATE_KHR";

		case VK_PRESENT_MODE_MAILBOX_KHR:
			return "VK_PRESENT_MODE_MAILBOX_KHR";

		case VK_PRESENT_MODE_FIFO_KHR:
			return "VK_PRESENT_MODE_FIFO_KHR";

		case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
			return "VK_PRESENT_MODE_FIFO_RELAXED_KHR";

		case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR:
			return "VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR";

		case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR:
			return "VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR";

		default:
			return "UNKNOWN_VK_PRESENT_MODE";
	}
}

bool VKSwapChain::SelectPresentMode(VkSurfaceKHR surface, GSVSyncMode* vsync_mode, VkPresentModeKHR* present_mode)
{
	VkResult res;
	u32 mode_count;
	res = vkGetPhysicalDeviceSurfacePresentModesKHR(
		GSDeviceVK::GetInstance()->GetPhysicalDevice(), surface, &mode_count, nullptr);
	if (res != VK_SUCCESS || mode_count == 0)
	{
		LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceSurfaceFormatsKHR failed: ");
		return false;
	}

	std::vector<VkPresentModeKHR> present_modes(mode_count);
	res = vkGetPhysicalDeviceSurfacePresentModesKHR(
		GSDeviceVK::GetInstance()->GetPhysicalDevice(), surface, &mode_count, present_modes.data());
	pxAssert(res == VK_SUCCESS);

	// Checks if a particular mode is supported, if it is, returns that mode.
	const auto CheckForMode = [&present_modes](VkPresentModeKHR check_mode) {
		auto it = std::find_if(present_modes.begin(), present_modes.end(),
			[check_mode](VkPresentModeKHR mode) { return check_mode == mode; });
		return it != present_modes.end();
	};

	switch (*vsync_mode)
	{
		case GSVSyncMode::Disabled:
		{
			// Prefer immediate > mailbox > fifo.
			if (CheckForMode(VK_PRESENT_MODE_IMMEDIATE_KHR))
			{
				*present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			}
			else if (CheckForMode(VK_PRESENT_MODE_MAILBOX_KHR))
			{
				WARNING_LOG("Immediate not supported for vsync-disabled, using mailbox.");
				*present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
				*vsync_mode = GSVSyncMode::Mailbox;
			}
			else
			{
				WARNING_LOG("Mailbox not supported for vsync-disabled, using FIFO.");
				*present_mode = VK_PRESENT_MODE_FIFO_KHR;
				*vsync_mode = GSVSyncMode::FIFO;
			}
		}
		break;

		case GSVSyncMode::FIFO:
		{
			// FIFO is always available.
			*present_mode = VK_PRESENT_MODE_FIFO_KHR;
		}
		break;

		case GSVSyncMode::Mailbox:
		{
			// Mailbox > fifo.
			if (CheckForMode(VK_PRESENT_MODE_MAILBOX_KHR))
			{
				*present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
			}
			else
			{
				WARNING_LOG("Mailbox not supported for vsync-mailbox, using FIFO.");
				*present_mode = VK_PRESENT_MODE_FIFO_KHR;
				*vsync_mode = GSVSyncMode::FIFO;
			}
		}
		break;

			jNO_DEFAULT
	}

	return true;
}

bool VKSwapChain::CreateSwapChain()
{
	// Select swap chain format
	std::optional<VkSurfaceFormatKHR> surface_format = SelectSurfaceFormat(m_surface);
	if (!surface_format.has_value())
		return false;

	// Look up surface properties to determine image count and dimensions
	VkSurfaceCapabilitiesKHR surface_capabilities;
	VkResult res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		GSDeviceVK::GetInstance()->GetPhysicalDevice(), m_surface, &surface_capabilities);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed: ");
		return false;
	}

	// Select number of images in swap chain, we prefer one buffer in the background to work on in triple-buffered mode.
	// maxImageCount can be zero, in which case there isn't an upper limit on the number of buffers.
	u32 image_count = std::clamp<u32>(
		(m_present_mode == VK_PRESENT_MODE_MAILBOX_KHR) ? 3 : 2, surface_capabilities.minImageCount,
		(surface_capabilities.maxImageCount == 0) ? std::numeric_limits<u32>::max() : surface_capabilities.maxImageCount);
	DEV_LOG("Creating a swap chain with {} images in present mode {}", image_count, PresentModeToString(m_present_mode));

	// Determine the dimensions of the swap chain. Values of -1 indicate the size we specify here
	// determines window size?
	VkExtent2D size = surface_capabilities.currentExtent;
	if (size.width == UINT32_MAX)
	{
		size.width = m_window_info.surface_width;
		size.height = m_window_info.surface_height;
	}
	size.width =
		std::clamp(size.width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
	size.height =
		std::clamp(size.height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);

	// Prefer identity transform if possible
	VkSurfaceTransformFlagBitsKHR transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	if (!(surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR))
		transform = surface_capabilities.currentTransform;

	VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	if (!(surface_capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR))
	{
		// If we only support pre-multiplied/post-multiplied... :/
		if (surface_capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
			alpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
	}

	// Select swap chain flags, we only need a colour attachment
	VkImageUsageFlags image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if ((surface_capabilities.supportedUsageFlags & image_usage) != image_usage)
	{
		Console.Error("Vulkan: Swap chain does not support usage as color attachment");
		return false;
	}

	// Store the old/current swap chain when recreating for resize
	// Old swap chain is destroyed regardless of whether the create call succeeds
	VkSwapchainKHR old_swap_chain = m_swap_chain;
	m_swap_chain = VK_NULL_HANDLE;

	// Now we can actually create the swap chain
	VkSwapchainCreateInfoKHR swap_chain_info = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, nullptr, 0, m_surface,
		image_count, surface_format->format, surface_format->colorSpace, size, 1u, image_usage,
		VK_SHARING_MODE_EXCLUSIVE, 0, nullptr, transform, alpha, m_present_mode, VK_TRUE, old_swap_chain};
	std::array<uint32_t, 2> indices = {{
		GSDeviceVK::GetInstance()->GetGraphicsQueueFamilyIndex(),
		GSDeviceVK::GetInstance()->GetPresentQueueFamilyIndex(),
	}};
	if (GSDeviceVK::GetInstance()->GetGraphicsQueueFamilyIndex() !=
		GSDeviceVK::GetInstance()->GetPresentQueueFamilyIndex())
	{
		swap_chain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swap_chain_info.queueFamilyIndexCount = 2;
		swap_chain_info.pQueueFamilyIndices = indices.data();
	}

#ifdef _WIN32
	VkSurfaceFullScreenExclusiveInfoEXT exclusive_info = {VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_INFO_EXT};
	VkSurfaceFullScreenExclusiveWin32InfoEXT exclusive_win32_info = {
		VK_STRUCTURE_TYPE_SURFACE_FULL_SCREEN_EXCLUSIVE_WIN32_INFO_EXT};
	if (m_exclusive_fullscreen_control.has_value())
	{
		if (GSDeviceVK::GetInstance()->GetOptionalExtensions().vk_ext_full_screen_exclusive)
		{
			exclusive_info.fullScreenExclusive =
				(m_exclusive_fullscreen_control.value() ? VK_FULL_SCREEN_EXCLUSIVE_ALLOWED_EXT :
														  VK_FULL_SCREEN_EXCLUSIVE_DISALLOWED_EXT);

			exclusive_win32_info.hmonitor =
				MonitorFromWindow(reinterpret_cast<HWND>(m_window_info.window_handle), MONITOR_DEFAULTTONEAREST);
			if (!exclusive_win32_info.hmonitor)
				Console.Error("MonitorFromWindow() for exclusive fullscreen exclusive override failed.");

			Vulkan::AddPointerToChain(&swap_chain_info, &exclusive_info);
			Vulkan::AddPointerToChain(&swap_chain_info, &exclusive_win32_info);
		}
		else
		{
			Console.Error("Exclusive fullscreen control requested, but VK_EXT_full_screen_exclusive is not supported.");
		}
	}
#else
	if (m_exclusive_fullscreen_control.has_value())
		Console.Error("Exclusive fullscreen control requested, but is not supported on this platform.");
#endif

	res = vkCreateSwapchainKHR(GSDeviceVK::GetInstance()->GetDevice(), &swap_chain_info, nullptr, &m_swap_chain);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateSwapchainKHR failed: ");
		return false;
	}

	// Now destroy the old swap chain, since it's been recreated.
	// We can do this immediately since all work should have been completed before calling resize.
	if (old_swap_chain != VK_NULL_HANDLE)
		vkDestroySwapchainKHR(GSDeviceVK::GetInstance()->GetDevice(), old_swap_chain, nullptr);

	m_window_info.surface_width = std::max(1u, size.width);
	m_window_info.surface_height = std::max(1u, size.height);

	// Get and create images.
	pxAssert(m_images.empty());

	res = vkGetSwapchainImagesKHR(GSDeviceVK::GetInstance()->GetDevice(), m_swap_chain, &image_count, nullptr);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkGetSwapchainImagesKHR failed: ");
		return false;
	}

	std::vector<VkImage> images(image_count);
	res = vkGetSwapchainImagesKHR(GSDeviceVK::GetInstance()->GetDevice(), m_swap_chain, &image_count, images.data());
	pxAssert(res == VK_SUCCESS);

	m_images.reserve(image_count);
	m_current_image = 0;
	// It's too early to call "GSDeviceVK::LookupNativeFormat()"
	GSTexture::Format format = GSTexture::Format::Color;
	switch (surface_format->format)
	{
		case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
			format = GSTexture::Format::ColorHQ;
			break;
		case VK_FORMAT_R16G16B16A16_SFLOAT:
			format = GSTexture::Format::ColorHDR;
			break;
	}
	for (u32 i = 0; i < image_count; i++)
	{
		std::unique_ptr<GSTextureVK> texture =
			GSTextureVK::Adopt(images[i], GSTexture::Type::RenderTarget, format,
				m_window_info.surface_width, m_window_info.surface_height, 1, surface_format->format);
		if (!texture)
			return false;

		m_images.push_back(std::move(texture));
	}

	m_current_semaphore = 0;
	for (u32 i = 0; i < NUM_SEMAPHORES; i++)
	{
		ImageSemaphores& sema = m_semaphores[i];

		const VkSemaphoreCreateInfo semaphore_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0};
		res = vkCreateSemaphore(
			GSDeviceVK::GetInstance()->GetDevice(), &semaphore_info, nullptr, &sema.available_semaphore);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateSemaphore failed: ");
			return false;
		}

		res = vkCreateSemaphore(
			GSDeviceVK::GetInstance()->GetDevice(), &semaphore_info, nullptr, &sema.rendering_finished_semaphore);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateSemaphore failed: ");
			vkDestroySemaphore(GSDeviceVK::GetInstance()->GetDevice(), sema.available_semaphore, nullptr);
			sema.available_semaphore = VK_NULL_HANDLE;
			return false;
		}
	}

	return true;
}

void VKSwapChain::DestroySwapChainImages()
{
	for (auto& it : m_images)
	{
		// don't defer view destruction, images are no longer valid
		it->Destroy(false);
	}
	m_images.clear();
	for (auto& it : m_semaphores)
	{
		if (it.rendering_finished_semaphore != VK_NULL_HANDLE)
			vkDestroySemaphore(GSDeviceVK::GetInstance()->GetDevice(), it.rendering_finished_semaphore, nullptr);
		if (it.available_semaphore != VK_NULL_HANDLE)
			vkDestroySemaphore(GSDeviceVK::GetInstance()->GetDevice(), it.available_semaphore, nullptr);
	}
	m_semaphores = {};

	m_image_acquire_result.reset();
}

void VKSwapChain::DestroySwapChain()
{
	DestroySwapChainImages();

	if (m_swap_chain == VK_NULL_HANDLE)
		return;

	vkDestroySwapchainKHR(GSDeviceVK::GetInstance()->GetDevice(), m_swap_chain, nullptr);
	m_swap_chain = VK_NULL_HANDLE;
	m_window_info.surface_width = 0;
	m_window_info.surface_height = 0;
}

VkFormat VKSwapChain::GetTextureFormat() const
{
	if (m_images.empty())
		return VK_FORMAT_UNDEFINED;

	return m_images[m_current_image]->GetVkFormat();
}

VkResult VKSwapChain::AcquireNextImage()
{
	if (m_image_acquire_result.has_value())
		return m_image_acquire_result.value();

	if (!m_swap_chain)
		return VK_ERROR_SURFACE_LOST_KHR;

	// Use a different semaphore for each image.
	m_current_semaphore = (m_current_semaphore + 1) % static_cast<u32>(m_semaphores.size());

	const VkResult res = vkAcquireNextImageKHR(GSDeviceVK::GetInstance()->GetDevice(), m_swap_chain, UINT64_MAX,
		m_semaphores[m_current_semaphore].available_semaphore, VK_NULL_HANDLE, &m_current_image);
	m_image_acquire_result = res;
	return res;
}

void VKSwapChain::ReleaseCurrentImage()
{
	if (!m_image_acquire_result.has_value())
		return;

	if ((m_image_acquire_result.value() == VK_SUCCESS || m_image_acquire_result.value() == VK_SUBOPTIMAL_KHR) &&
		GSDeviceVK::GetInstance()->GetOptionalExtensions().vk_ext_swapchain_maintenance1)
	{
		GSDeviceVK::GetInstance()->WaitForGPUIdle();

		const VkReleaseSwapchainImagesInfoEXT info = {.sType = VK_STRUCTURE_TYPE_RELEASE_SWAPCHAIN_IMAGES_INFO_EXT,
			.swapchain = m_swap_chain,
			.imageIndexCount = 1,
			.pImageIndices = &m_current_image};
		VkResult res = vkReleaseSwapchainImagesEXT(GSDeviceVK::GetInstance()->GetDevice(), &info);
		if (res != VK_SUCCESS)
			LOG_VULKAN_ERROR(res, "vkReleaseSwapchainImagesEXT() failed: ");
	}

	m_image_acquire_result.reset();
}

void VKSwapChain::ResetImageAcquireResult()
{
	m_image_acquire_result.reset();
}

bool VKSwapChain::ResizeSwapChain(u32 new_width, u32 new_height, float new_scale)
{
	ReleaseCurrentImage();
	DestroySwapChainImages();

	if (new_width != 0 && new_height != 0)
	{
		m_window_info.surface_width = new_width;
		m_window_info.surface_height = new_height;
	}

	m_window_info.surface_scale = new_scale;

	if (!CreateSwapChain())
	{
		DestroySwapChain();
		return false;
	}

	return true;
}

bool VKSwapChain::SetPresentMode(VkPresentModeKHR present_mode)
{
	if (m_present_mode == present_mode)
		return true;

	m_present_mode = present_mode;

	// Recreate the swap chain with the new present mode.
	INFO_LOG("Recreating swap chain to change present mode.");
	ReleaseCurrentImage();
	DestroySwapChainImages();
	if (!CreateSwapChain())
	{
		DestroySwapChain();
		return false;
	}

	return true;
}

bool VKSwapChain::RecreateSurface(const WindowInfo& new_wi)
{
	// Destroy the old swap chain, images, and surface.
	DestroySwapChain();
	DestroySurface();

	// Re-create the surface with the new native handle
	m_window_info = new_wi;
	m_surface = CreateVulkanSurface(
		GSDeviceVK::GetInstance()->GetVulkanInstance(), GSDeviceVK::GetInstance()->GetPhysicalDevice(), &m_window_info);
	if (m_surface == VK_NULL_HANDLE)
		return false;

	// The validation layers get angry at us if we don't call this before creating the swapchain.
	VkBool32 present_supported = VK_TRUE;
	VkResult res = vkGetPhysicalDeviceSurfaceSupportKHR(GSDeviceVK::GetInstance()->GetPhysicalDevice(),
		GSDeviceVK::GetInstance()->GetPresentQueueFamilyIndex(), m_surface, &present_supported);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceSurfaceSupportKHR failed: ");
		return false;
	}
	if (!present_supported)
	{
		pxFailRel("Recreated surface does not support presenting.");
		return false;
	}

	// Finally re-create the swap chain
	if (!CreateSwapChain())
	{
		DestroySwapChain();
		return false;
	}

	return true;
}

void VKSwapChain::DestroySurface()
{
	if (m_surface == VK_NULL_HANDLE)
		return;

	DestroyVulkanSurface(GSDeviceVK::GetInstance()->GetVulkanInstance(), &m_window_info, m_surface);
	m_surface = VK_NULL_HANDLE;
}
