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

#include "common/Vulkan/SwapChain.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "common/Vulkan/Context.h"
#include "common/Vulkan/Util.h"
#include <algorithm>
#include <array>
#include <cmath>

#if defined(VK_USE_PLATFORM_XLIB_KHR)
#include <X11/Xlib.h>
#endif

#if defined(__APPLE__)
#include <objc/message.h>
#include <dispatch/dispatch.h>

#ifdef __i386__
typedef float CGFloat;
#else
typedef double CGFloat;
#endif

template <typename Ret, typename Self, typename... Args>
Ret msgsend(Self self, const char* sel, Args... args)
{
	void (*fn)(void) = objc_msgSend;
#ifdef __i386__
	if (std::is_same<Ret, float>::value || std::is_same<Ret, double>::value || std::is_same<Ret, long double>::value)
		fn = objc_msgSend_fpret;
#endif
#ifdef __x86_64__
	if (std::is_same<Ret, long double>::value)
		fn = objc_msgSend_fpret;
#endif
	return reinterpret_cast<Ret(*)(Self, SEL, Args...)>(fn)(self, sel_getUid(sel), args...);
}

static bool CreateMetalLayer(WindowInfo* wi)
{
	// if (![NSThread isMainThread])
	if (!msgsend<BOOL, Class>(objc_getClass("NSThread"), "isMainThread"))
	{
		__block bool ret;
		dispatch_sync(dispatch_get_main_queue(), ^{ ret = CreateMetalLayer(wi); });
		return ret;
	}

	id view = reinterpret_cast<id>(wi->window_handle);

	Class clsCAMetalLayer = objc_getClass("CAMetalLayer");
	if (!clsCAMetalLayer)
	{
		Console.Error("Failed to get CAMetalLayer class.");
		return false;
	}

	// [CAMetalLayer layer]
	id layer = msgsend<id, Class>(clsCAMetalLayer, "layer");
	if (!layer)
	{
		Console.Error("Failed to create Metal layer.");
		return false;
	}

	// This needs to be retained, otherwise we double release below.
	msgsend<void, id>(layer, "retain");

	// [view setWantsLayer:YES]
	msgsend<void, id, BOOL>(view, "setWantsLayer:", YES);

	// [view setLayer:layer]
	msgsend<void, id, id>(view, "setLayer:", layer);

	// NSScreen* screen = [NSScreen mainScreen]
	id screen = msgsend<id, Class>(objc_getClass("NSScreen"), "mainScreen");

	// CGFloat factor = [screen backingScaleFactor]
	CGFloat factor = msgsend<CGFloat, id>(screen, "backingScaleFactor");

	// layer.contentsScale = factor
	msgsend<void, id, CGFloat>(layer, "setContentsScale:", factor);

	// Store the layer pointer, that way MoltenVK doesn't call [NSView layer] outside the main thread.
	wi->surface_handle = layer;
	return true;
}

static void DestroyMetalLayer(WindowInfo* wi)
{
	id view = reinterpret_cast<id>(wi->window_handle);
	id layer = reinterpret_cast<id>(wi->surface_handle);
	if (layer == nil)
		return;

	reinterpret_cast<void (*)(id, SEL, id)>(objc_msgSend)(view, sel_getUid("setLayer:"), nil);
	reinterpret_cast<void (*)(id, SEL, BOOL)>(objc_msgSend)(view, sel_getUid("setWantsLayer:"), NO);
	reinterpret_cast<void (*)(id, SEL)>(objc_msgSend)(layer, sel_getUid("release"));
	wi->surface_handle = nullptr;
}

#endif

namespace Vulkan
{
	SwapChain::SwapChain(const WindowInfo& wi, VkSurfaceKHR surface, VkPresentModeKHR preferred_present_mode)
		: m_window_info(wi)
		, m_surface(surface)
		, m_preferred_present_mode(preferred_present_mode)
	{
	}

	SwapChain::~SwapChain()
	{
		DestroySemaphores();
		DestroySwapChainImages();
		DestroySwapChain();
		DestroySurface();
	}

	static VkSurfaceKHR CreateDisplaySurface(VkInstance instance, VkPhysicalDevice physical_device, WindowInfo* wi)
	{
		Console.WriteLn("Trying to create a VK_KHR_display surface of %ux%u", wi->surface_width, wi->surface_height);

		u32 num_displays;
		VkResult res = vkGetPhysicalDeviceDisplayPropertiesKHR(physical_device, &num_displays, nullptr);
		if (res != VK_SUCCESS || num_displays == 0)
		{
			LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceDisplayPropertiesKHR() failed:");
			return {};
		}

		std::vector<VkDisplayPropertiesKHR> displays(num_displays);
		res = vkGetPhysicalDeviceDisplayPropertiesKHR(physical_device, &num_displays, displays.data());
		if (res != VK_SUCCESS || num_displays != displays.size())
		{
			LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceDisplayPropertiesKHR() failed:");
			return {};
		}

		for (u32 display_index = 0; display_index < num_displays; display_index++)
		{
			const VkDisplayPropertiesKHR& props = displays[display_index];
			DevCon.WriteLn("Testing display '%s'", props.displayName);

			u32 num_modes;
			res = vkGetDisplayModePropertiesKHR(physical_device, props.display, &num_modes, nullptr);
			if (res != VK_SUCCESS || num_modes == 0)
			{
				LOG_VULKAN_ERROR(res, "vkGetDisplayModePropertiesKHR() failed:");
				continue;
			}

			std::vector<VkDisplayModePropertiesKHR> modes(num_modes);
			res = vkGetDisplayModePropertiesKHR(physical_device, props.display, &num_modes, modes.data());
			if (res != VK_SUCCESS || num_modes != modes.size())
			{
				LOG_VULKAN_ERROR(res, "vkGetDisplayModePropertiesKHR() failed:");
				continue;
			}

			const VkDisplayModePropertiesKHR* matched_mode = nullptr;
			for (const VkDisplayModePropertiesKHR& mode : modes)
			{
				const float refresh_rate = static_cast<float>(mode.parameters.refreshRate) / 1000.0f;
				DevCon.WriteLn("  Mode %ux%u @ %f", mode.parameters.visibleRegion.width,
					mode.parameters.visibleRegion.height, refresh_rate);

				if (!matched_mode && ((wi->surface_width == 0 && wi->surface_height == 0) ||
										 (mode.parameters.visibleRegion.width == wi->surface_width &&
											 mode.parameters.visibleRegion.height == wi->surface_height &&
											 (wi->surface_refresh_rate == 0.0f ||
												 std::abs(refresh_rate - wi->surface_refresh_rate) < 0.1f))))
				{
					matched_mode = &mode;
				}
			}

			if (!matched_mode)
			{
				DevCon.WriteLn("No modes matched on '%s'", props.displayName);
				continue;
			}

			u32 num_planes;
			res = vkGetPhysicalDeviceDisplayPlanePropertiesKHR(physical_device, &num_planes, nullptr);
			if (res != VK_SUCCESS)
			{
				LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceDisplayPlanePropertiesKHR() failed:");
				continue;
			}
			if (num_planes == 0)
				continue;

			std::vector<VkDisplayPlanePropertiesKHR> planes(num_planes);
			res = vkGetPhysicalDeviceDisplayPlanePropertiesKHR(physical_device, &num_planes, planes.data());
			if (res != VK_SUCCESS || num_planes != planes.size())
			{
				LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceDisplayPlanePropertiesKHR() failed:");
				continue;
			}

			u32 plane_index = 0;
			for (; plane_index < num_planes; plane_index++)
			{
				u32 supported_display_count;
				res = vkGetDisplayPlaneSupportedDisplaysKHR(
					physical_device, plane_index, &supported_display_count, nullptr);
				if (res != VK_SUCCESS)
				{
					LOG_VULKAN_ERROR(res, "vkGetDisplayPlaneSupportedDisplaysKHR() failed:");
					continue;
				}
				if (supported_display_count == 0)
					continue;

				std::vector<VkDisplayKHR> supported_displays(supported_display_count);
				res = vkGetDisplayPlaneSupportedDisplaysKHR(
					physical_device, plane_index, &supported_display_count, supported_displays.data());
				if (res != VK_SUCCESS)
				{
					LOG_VULKAN_ERROR(res, "vkGetDisplayPlaneSupportedDisplaysKHR() failed:");
					continue;
				}

				const bool is_supported = std::find(supported_displays.begin(), supported_displays.end(),
											  props.display) != supported_displays.end();
				if (!is_supported)
					continue;

				break;
			}

			if (plane_index == num_planes)
			{
				DevCon.WriteLn("No planes matched on '%s'", props.displayName);
				continue;
			}

			VkDisplaySurfaceCreateInfoKHR info = {};
			info.sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR;
			info.displayMode = matched_mode->displayMode;
			info.planeIndex = plane_index;
			info.planeStackIndex = planes[plane_index].currentStackIndex;
			info.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
			info.globalAlpha = 1.0f;
			info.alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
			info.imageExtent = matched_mode->parameters.visibleRegion;

			VkSurfaceKHR surface;
			res = vkCreateDisplayPlaneSurfaceKHR(instance, &info, nullptr, &surface);
			if (res != VK_SUCCESS)
			{
				LOG_VULKAN_ERROR(res, "vkCreateDisplayPlaneSurfaceKHR() failed: ");
				continue;
			}

			wi->surface_refresh_rate = static_cast<float>(matched_mode->parameters.refreshRate) / 1000.0f;
			return surface;
		}

		return VK_NULL_HANDLE;
	}

	static std::vector<SwapChain::FullscreenModeInfo> GetDisplayModes(
		VkInstance instance, VkPhysicalDevice physical_device, const WindowInfo& wi)
	{
		u32 num_displays;
		VkResult res = vkGetPhysicalDeviceDisplayPropertiesKHR(physical_device, &num_displays, nullptr);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceDisplayPropertiesKHR() failed:");
			return {};
		}
		if (num_displays == 0)
		{
			Console.Error("No displays were returned");
			return {};
		}

		std::vector<VkDisplayPropertiesKHR> displays(num_displays);
		res = vkGetPhysicalDeviceDisplayPropertiesKHR(physical_device, &num_displays, displays.data());
		if (res != VK_SUCCESS || num_displays != displays.size())
		{
			LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceDisplayPropertiesKHR() failed:");
			return {};
		}

		std::vector<SwapChain::FullscreenModeInfo> result;
		for (u32 display_index = 0; display_index < num_displays; display_index++)
		{
			const VkDisplayPropertiesKHR& props = displays[display_index];

			u32 num_modes;
			res = vkGetDisplayModePropertiesKHR(physical_device, props.display, &num_modes, nullptr);
			if (res != VK_SUCCESS || num_modes == 0)
			{
				LOG_VULKAN_ERROR(res, "vkGetDisplayModePropertiesKHR() failed:");
				continue;
			}

			std::vector<VkDisplayModePropertiesKHR> modes(num_modes);
			res = vkGetDisplayModePropertiesKHR(physical_device, props.display, &num_modes, modes.data());
			if (res != VK_SUCCESS || num_modes != modes.size())
			{
				LOG_VULKAN_ERROR(res, "vkGetDisplayModePropertiesKHR() failed:");
				continue;
			}

			for (const VkDisplayModePropertiesKHR& mode : modes)
			{
				const float refresh_rate = static_cast<float>(mode.parameters.refreshRate) / 1000.0f;
				if (std::find_if(
						result.begin(), result.end(), [&mode, refresh_rate](const SwapChain::FullscreenModeInfo& mi) {
							return (mi.width == mode.parameters.visibleRegion.width &&
									mi.height == mode.parameters.visibleRegion.height &&
									mode.parameters.refreshRate == refresh_rate);
						}) != result.end())
				{
					continue;
				}

				result.push_back(SwapChain::FullscreenModeInfo{static_cast<u32>(mode.parameters.visibleRegion.width),
					static_cast<u32>(mode.parameters.visibleRegion.height), refresh_rate});
			}
		}

		return result;
	}

	VkSurfaceKHR SwapChain::CreateVulkanSurface(VkInstance instance, VkPhysicalDevice physical_device, WindowInfo* wi)
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
			VkWaylandSurfaceCreateInfoKHR surface_create_info = {VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
				nullptr, 0, static_cast<struct wl_display*>(wi->display_connection),
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

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
		if (wi->type == WindowInfo::Type::Android)
		{
			VkAndroidSurfaceCreateInfoKHR surface_create_info = {
				VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR, // VkStructureType                sType
				nullptr, // const void*                    pNext
				0, // VkAndroidSurfaceCreateFlagsKHR flags
				reinterpret_cast<ANativeWindow*>(wi->window_handle) // ANativeWindow* window
			};

			VkSurfaceKHR surface;
			VkResult res = vkCreateAndroidSurfaceKHR(instance, &surface_create_info, nullptr, &surface);
			if (res != VK_SUCCESS)
			{
				LOG_VULKAN_ERROR(res, "vkCreateAndroidSurfaceKHR failed: ");
				return VK_NULL_HANDLE;
			}

			return surface;
		}
#endif

#if defined(VK_USE_PLATFORM_METAL_EXT)
		if (wi->type == WindowInfo::Type::MacOS)
		{
			if (!wi->surface_handle && !CreateMetalLayer(wi))
				return VK_NULL_HANDLE;

			VkMetalSurfaceCreateInfoEXT surface_create_info = {VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT, nullptr,
				0, static_cast<const CAMetalLayer*>(wi->surface_handle)};

			VkSurfaceKHR surface;
			VkResult res = vkCreateMetalSurfaceEXT(instance, &surface_create_info, nullptr, &surface);
			if (res != VK_SUCCESS)
			{
				LOG_VULKAN_ERROR(res, "vkCreateMetalSurfaceEXT failed: ");
				return VK_NULL_HANDLE;
			}

			return surface;
		}
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
		if (wi->type == WindowInfo::Type::MacOS)
		{
			VkMacOSSurfaceCreateInfoMVK surface_create_info = {
				VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK, nullptr, 0, wi->window_handle};

			VkSurfaceKHR surface;
			VkResult res = vkCreateMacOSSurfaceMVK(instance, &surface_create_info, nullptr, &surface);
			if (res != VK_SUCCESS)
			{
				LOG_VULKAN_ERROR(res, "vkCreateMacOSSurfaceMVK failed: ");
				return VK_NULL_HANDLE;
			}

			return surface;
		}
#endif

#if 0
		if (wi->type == WindowInfo::Type::Display)
			return CreateDisplaySurface(instance, physical_device, wi);
#endif

		return VK_NULL_HANDLE;
	}

	void SwapChain::DestroyVulkanSurface(VkInstance instance, WindowInfo* wi, VkSurfaceKHR surface)
	{
		vkDestroySurfaceKHR(g_vulkan_context->GetVulkanInstance(), surface, nullptr);

#if defined(__APPLE__)
		if (wi->type == WindowInfo::Type::MacOS && wi->surface_handle)
			DestroyMetalLayer(wi);
#endif
	}

	std::vector<SwapChain::FullscreenModeInfo> SwapChain::GetSurfaceFullscreenModes(
		VkInstance instance, VkPhysicalDevice physical_device, const WindowInfo& wi)
	{
#if 0
		if (wi.type == WindowInfo::Type::Display)
			return GetDisplayModes(instance, physical_device, wi);
#endif

		return {};
	}

	std::unique_ptr<SwapChain> SwapChain::Create(const WindowInfo& wi, VkSurfaceKHR surface,
		VkPresentModeKHR preferred_present_mode)
	{
		std::unique_ptr<SwapChain> swap_chain = std::make_unique<SwapChain>(wi, surface, preferred_present_mode);
		if (!swap_chain->CreateSwapChain() || !swap_chain->SetupSwapChainImages() || !swap_chain->CreateSemaphores())
			return nullptr;

		return swap_chain;
	}

	bool SwapChain::SelectSurfaceFormat()
	{
		u32 format_count;
		VkResult res = vkGetPhysicalDeviceSurfaceFormatsKHR(
			g_vulkan_context->GetPhysicalDevice(), m_surface, &format_count, nullptr);
		if (res != VK_SUCCESS || format_count == 0)
		{
			LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceSurfaceFormatsKHR failed: ");
			return false;
		}

		std::vector<VkSurfaceFormatKHR> surface_formats(format_count);
		res = vkGetPhysicalDeviceSurfaceFormatsKHR(
			g_vulkan_context->GetPhysicalDevice(), m_surface, &format_count, surface_formats.data());
		pxAssert(res == VK_SUCCESS);

		// If there is a single undefined surface format, the device doesn't care, so we'll just use RGBA
		if (surface_formats[0].format == VK_FORMAT_UNDEFINED)
		{
			m_surface_format.format = VK_FORMAT_R8G8B8A8_UNORM;
			m_surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
			return true;
		}

		// Try to find a suitable format.
		for (const VkSurfaceFormatKHR& surface_format : surface_formats)
		{
			// Some drivers seem to return a SRGB format here (Intel Mesa).
			// This results in gamma correction when presenting to the screen, which we don't want.
			// Use a linear format instead, if this is the case.
			m_surface_format.format = Util::GetLinearFormat(surface_format.format);
			m_surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
			return true;
		}

		pxFailRel("Failed to find a suitable format for swap chain buffers.");
		return false;
	}

	bool SwapChain::SelectPresentMode()
	{
		VkResult res;
		u32 mode_count;
		res = vkGetPhysicalDeviceSurfacePresentModesKHR(
			g_vulkan_context->GetPhysicalDevice(), m_surface, &mode_count, nullptr);
		if (res != VK_SUCCESS || mode_count == 0)
		{
			LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceSurfaceFormatsKHR failed: ");
			return false;
		}

		std::vector<VkPresentModeKHR> present_modes(mode_count);
		res = vkGetPhysicalDeviceSurfacePresentModesKHR(
			g_vulkan_context->GetPhysicalDevice(), m_surface, &mode_count, present_modes.data());
		pxAssert(res == VK_SUCCESS);

		// Checks if a particular mode is supported, if it is, returns that mode.
		auto CheckForMode = [&present_modes](VkPresentModeKHR check_mode) {
			auto it = std::find_if(present_modes.begin(), present_modes.end(),
				[check_mode](VkPresentModeKHR mode) { return check_mode == mode; });
			return it != present_modes.end();
		};

		// Use preferred mode if available.
		if (CheckForMode(m_preferred_present_mode))
		{
			m_present_mode = m_preferred_present_mode;
			return true;
		}

		// Prefer mailbox over fifo for adaptive vsync/no-vsync.
		if ((m_preferred_present_mode == VK_PRESENT_MODE_FIFO_RELAXED_KHR ||
				m_preferred_present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR) &&
			CheckForMode(VK_PRESENT_MODE_MAILBOX_KHR))
		{
			m_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
			return true;
		}

		// Fallback to FIFO if we're using any kind of vsync.
		if (m_preferred_present_mode == VK_PRESENT_MODE_FIFO_KHR || m_preferred_present_mode == VK_PRESENT_MODE_FIFO_RELAXED_KHR)
		{
			// This should never fail, FIFO is mandated.
			if (CheckForMode(VK_PRESENT_MODE_FIFO_KHR))
			{
				m_present_mode = VK_PRESENT_MODE_FIFO_KHR;
				return true;
			}
		}

		// Fall back to whatever is available.
		m_present_mode = present_modes[0];
		return true;
	}

	bool SwapChain::CreateSwapChain()
	{
		// Look up surface properties to determine image count and dimensions
		VkSurfaceCapabilitiesKHR surface_capabilities;
		VkResult res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
			g_vulkan_context->GetPhysicalDevice(), m_surface, &surface_capabilities);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed: ");
			return false;
		}

		// Select swap chain format and present mode
		if (!SelectSurfaceFormat() || !SelectPresentMode())
			return false;

		DevCon.WriteLn("(SwapChain) Preferred present mode: %s, selected: %s",
			Util::PresentModeToString(m_preferred_present_mode), Util::PresentModeToString(m_present_mode));

		// Select number of images in swap chain, we prefer one buffer in the background to work on
		u32 image_count = std::max(surface_capabilities.minImageCount + 1u, 2u);

		// maxImageCount can be zero, in which case there isn't an upper limit on the number of buffers.
		if (surface_capabilities.maxImageCount > 0)
			image_count = std::min(image_count, surface_capabilities.maxImageCount);

		// Determine the dimensions of the swap chain. Values of -1 indicate the size we specify here
		// determines window size?
		VkExtent2D size = surface_capabilities.currentExtent;
#ifndef ANDROID
		if (size.width == UINT32_MAX)
#endif
		{
			size.width = m_window_info.surface_width;
			size.height = m_window_info.surface_height;
		}
		size.width = std::clamp(
			size.width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
		size.height = std::clamp(
			size.height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);

		// Prefer identity transform if possible
		VkSurfaceTransformFlagBitsKHR transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		if (!(surface_capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR))
			transform = surface_capabilities.currentTransform;

		// Select swap chain flags, we only need a colour attachment
		VkImageUsageFlags image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		if (!(surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
		{
			Console.Error("Vulkan: Swap chain does not support usage as color attachment");
			return false;
		}

		// Store the old/current swap chain when recreating for resize
		VkSwapchainKHR old_swap_chain = m_swap_chain;
		m_swap_chain = VK_NULL_HANDLE;

		// Now we can actually create the swap chain
		VkSwapchainCreateInfoKHR swap_chain_info = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, nullptr, 0, m_surface,
			image_count, m_surface_format.format, m_surface_format.colorSpace, size, 1u, image_usage,
			VK_SHARING_MODE_EXCLUSIVE, 0, nullptr, transform, VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, m_present_mode,
			VK_TRUE, old_swap_chain};
		std::array<uint32_t, 2> indices = {{
			g_vulkan_context->GetGraphicsQueueFamilyIndex(),
			g_vulkan_context->GetPresentQueueFamilyIndex(),
		}};
		if (g_vulkan_context->GetGraphicsQueueFamilyIndex() != g_vulkan_context->GetPresentQueueFamilyIndex())
		{
			swap_chain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swap_chain_info.queueFamilyIndexCount = 2;
			swap_chain_info.pQueueFamilyIndices = indices.data();
		}

		if (m_swap_chain == VK_NULL_HANDLE)
		{
			res = vkCreateSwapchainKHR(g_vulkan_context->GetDevice(), &swap_chain_info, nullptr, &m_swap_chain);
		}
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateSwapchainKHR failed: ");
			return false;
		}

		// Now destroy the old swap chain, since it's been recreated.
		// We can do this immediately since all work should have been completed before calling resize.
		if (old_swap_chain != VK_NULL_HANDLE)
			vkDestroySwapchainKHR(g_vulkan_context->GetDevice(), old_swap_chain, nullptr);

		m_window_info.surface_width = std::max(1u, size.width);
		m_window_info.surface_height = std::max(1u, size.height);
		return true;
	}

	bool SwapChain::SetupSwapChainImages()
	{
		pxAssert(m_images.empty());

		u32 image_count;
		VkResult res = vkGetSwapchainImagesKHR(g_vulkan_context->GetDevice(), m_swap_chain, &image_count, nullptr);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkGetSwapchainImagesKHR failed: ");
			return false;
		}

		std::vector<VkImage> images(image_count);
		res = vkGetSwapchainImagesKHR(g_vulkan_context->GetDevice(), m_swap_chain, &image_count, images.data());
		pxAssert(res == VK_SUCCESS);

		m_load_render_pass =
			g_vulkan_context->GetRenderPass(m_surface_format.format, VK_FORMAT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_LOAD);
		m_clear_render_pass =
			g_vulkan_context->GetRenderPass(m_surface_format.format, VK_FORMAT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_CLEAR);
		if (m_load_render_pass == VK_NULL_HANDLE || m_clear_render_pass == VK_NULL_HANDLE)
		{
			pxFailRel("Failed to get swap chain render passes.");
			return false;
		}

		m_images.reserve(image_count);
		for (u32 i = 0; i < image_count; i++)
		{
			SwapChainImage image;
			image.image = images[i];

			// Create texture object, which creates a view of the backbuffer
			if (!image.texture.Adopt(image.image, VK_IMAGE_VIEW_TYPE_2D, m_window_info.surface_width,
					m_window_info.surface_height, 1, 1, m_surface_format.format, VK_SAMPLE_COUNT_1_BIT))
			{
				return false;
			}

			image.framebuffer = image.texture.CreateFramebuffer(m_load_render_pass);
			if (image.framebuffer == VK_NULL_HANDLE)
				return false;

			m_images.emplace_back(std::move(image));
		}

		return true;
	}

	void SwapChain::DestroySwapChainImages()
	{
		for (auto& it : m_images)
		{
			// Images themselves are cleaned up by the swap chain object
			vkDestroyFramebuffer(g_vulkan_context->GetDevice(), it.framebuffer, nullptr);
		}
		m_images.clear();
	}

	void SwapChain::DestroySwapChain()
	{
		if (m_swap_chain == VK_NULL_HANDLE)
			return;

		vkDestroySwapchainKHR(g_vulkan_context->GetDevice(), m_swap_chain, nullptr);
		m_swap_chain = VK_NULL_HANDLE;
	}

	VkResult SwapChain::AcquireNextImage()
	{
		if (!m_swap_chain)
			return VK_ERROR_SURFACE_LOST_KHR;

		return vkAcquireNextImageKHR(g_vulkan_context->GetDevice(), m_swap_chain, UINT64_MAX,
			m_image_available_semaphore, VK_NULL_HANDLE, &m_current_image);
	}

	bool SwapChain::ResizeSwapChain(u32 new_width /* = 0 */, u32 new_height /* = 0 */)
	{
		DestroySwapChainImages();
		DestroySemaphores();

		if (new_width != 0 && new_height != 0)
		{
			m_window_info.surface_width = new_width;
			m_window_info.surface_height = new_height;
		}

		if (!CreateSwapChain() || !SetupSwapChainImages() || !CreateSemaphores())
		{
			DestroySemaphores();
			DestroySwapChainImages();
			DestroySwapChain();
			return false;
		}

		return true;
	}

	bool SwapChain::RecreateSwapChain()
	{
		DestroySwapChainImages();
		DestroySemaphores();

		if (!CreateSwapChain() || !SetupSwapChainImages() || !CreateSemaphores())
		{
			DestroySemaphores();
			DestroySwapChainImages();
			DestroySwapChain();
			return false;
		}

		return true;
	}

	bool SwapChain::SetVSync(VkPresentModeKHR preferred_mode)
	{
		if (m_preferred_present_mode == preferred_mode)
			return true;

		// Recreate the swap chain with the new present mode.
		m_preferred_present_mode = preferred_mode;
		return RecreateSwapChain();
	}

	bool SwapChain::RecreateSurface(const WindowInfo& new_wi)
	{
		// Destroy the old swap chain, images, and surface.
		DestroySwapChainImages();
		DestroySwapChain();
		DestroySurface();
		DestroySemaphores();

		// Re-create the surface with the new native handle
		m_window_info = new_wi;
		m_surface = CreateVulkanSurface(
			g_vulkan_context->GetVulkanInstance(), g_vulkan_context->GetPhysicalDevice(), &m_window_info);
		if (m_surface == VK_NULL_HANDLE)
			return false;

		// The validation layers get angry at us if we don't call this before creating the swapchain.
		VkBool32 present_supported = VK_TRUE;
		VkResult res = vkGetPhysicalDeviceSurfaceSupportKHR(g_vulkan_context->GetPhysicalDevice(),
			g_vulkan_context->GetPresentQueueFamilyIndex(), m_surface, &present_supported);
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
		if (!CreateSwapChain() || !SetupSwapChainImages() || !CreateSemaphores())
			return false;

		return true;
	}

	void SwapChain::DestroySurface()
	{
		if (m_surface == VK_NULL_HANDLE)
			return;

		DestroyVulkanSurface(g_vulkan_context->GetVulkanInstance(), &m_window_info, m_surface);
		m_surface = VK_NULL_HANDLE;
	}

	bool SwapChain::CreateSemaphores()
	{
		// Create two semaphores, one that is triggered when the swapchain buffer is ready, another after
		// submit and before present
		VkSemaphoreCreateInfo semaphore_info = {
			VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, // VkStructureType          sType
			nullptr, // const void*              pNext
			0 // VkSemaphoreCreateFlags   flags
		};

		VkResult res;
		if ((res = vkCreateSemaphore(g_vulkan_context->GetDevice(), &semaphore_info, nullptr,
				 &m_image_available_semaphore)) != VK_SUCCESS ||
			(res = vkCreateSemaphore(g_vulkan_context->GetDevice(), &semaphore_info, nullptr,
				 &m_rendering_finished_semaphore)) != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateSemaphore failed: ");
			return false;
		}

		return true;
	}

	void SwapChain::DestroySemaphores()
	{
		if (m_image_available_semaphore != VK_NULL_HANDLE)
		{
			vkDestroySemaphore(g_vulkan_context->GetDevice(), m_image_available_semaphore, nullptr);
			m_image_available_semaphore = VK_NULL_HANDLE;
		}

		if (m_rendering_finished_semaphore != VK_NULL_HANDLE)
		{
			vkDestroySemaphore(g_vulkan_context->GetDevice(), m_rendering_finished_semaphore, nullptr);
			m_rendering_finished_semaphore = VK_NULL_HANDLE;
		}
	}
} // namespace Vulkan
