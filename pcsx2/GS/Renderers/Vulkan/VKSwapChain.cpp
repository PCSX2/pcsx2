// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/Renderers/Vulkan/GSDeviceVK.h"
#include "GS/Renderers/Vulkan/VKBuilders.h"
#include "GS/Renderers/Vulkan/VKSwapChain.h"
#include "VMManager.h"

#include "common/Assertions.h"
#include "common/CocoaTools.h"
#include "common/Console.h"
#include "common/Timer.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

#if defined(VK_USE_PLATFORM_XLIB_KHR)
#include <X11/Xlib.h>
#endif

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
#include <android/native_window.h>
#endif

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
#include <android/native_window.h>
#endif

static_assert(VKSwapChain::NUM_SEMAPHORES == (GSDeviceVK::NUM_COMMAND_BUFFERS + 1));

namespace
{
	// Diagnostic counters for present/acquire stalls (esp. on tiler-class drivers).
	// Atomics so MTGS-thread acquire/present writes are race-free under reads from any thread.
	std::atomic<bool> s_stats_enabled{false};
	std::atomic<u64> s_acquire_count{0};
	std::atomic<u64> s_acquire_total_ns{0};
	std::atomic<u64> s_acquire_max_ns{0};
	std::atomic<u64> s_present_count{0};
	std::atomic<u64> s_present_total_ns{0};
	std::atomic<u64> s_present_max_ns{0};
	// Aggregate counts of SUBOPTIMAL / OUT_OF_DATE results across BOTH the
	// acquire and the present path (NoteAcquire + NotePresent both tick these).
	// A persistently-stale swapchain can therefore tick up to twice per frame —
	// these are "results observed", not "frames affected". Intentional: it keeps
	// both event sources visible in the overlay without a 4-counter schema.
	std::atomic<u64> s_suboptimal_count{0};
	std::atomic<u64> s_out_of_date_count{0};

	void UpdateMax(std::atomic<u64>& dst, u64 sample)
	{
		u64 prev = dst.load(std::memory_order_relaxed);
		while (sample > prev && !dst.compare_exchange_weak(prev, sample, std::memory_order_relaxed))
		{
		}
	}
} // namespace

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

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
	if (wi->type == WindowInfo::Type::Android)
	{
		VkAndroidSurfaceCreateInfoKHR surface_create_info = {VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR, nullptr,
			0, reinterpret_cast<ANativeWindow*>(wi->window_handle)};

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

	// VK_KHR_display direct-to-monitor (kmsdrm handhelds). No compositor,
	// no GBM, no native window handle from the frontend — the renderer
	// enumerates displays itself.
	if (wi->type == WindowInfo::Type::VulkanDirect)
	{
		u32 display_count = 0;
		VkResult res = vkGetPhysicalDeviceDisplayPropertiesKHR(physical_device, &display_count, nullptr);
		if (res != VK_SUCCESS || display_count == 0)
		{
			LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceDisplayPropertiesKHR (count) failed: ");
			Console.Error("VK_KHR_display: no displays reported by ICD.");
			return VK_NULL_HANDLE;
		}

		std::vector<VkDisplayPropertiesKHR> displays(display_count);
		res = vkGetPhysicalDeviceDisplayPropertiesKHR(physical_device, &display_count, displays.data());
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceDisplayPropertiesKHR (data) failed: ");
			return VK_NULL_HANDLE;
		}

		// Pick the first display. Multi-monitor handhelds are vanishingly
		// rare; revisit if needed.
		const VkDisplayKHR display = displays[0].display;
		INFO_LOG("VK_KHR_display: using display '{}', physical {}x{} mm",
			displays[0].displayName ? displays[0].displayName : "<unnamed>",
			displays[0].physicalDimensions.width, displays[0].physicalDimensions.height);

		u32 mode_count = 0;
		res = vkGetDisplayModePropertiesKHR(physical_device, display, &mode_count, nullptr);
		if (res != VK_SUCCESS || mode_count == 0)
		{
			LOG_VULKAN_ERROR(res, "vkGetDisplayModePropertiesKHR (count) failed: ");
			return VK_NULL_HANDLE;
		}

		std::vector<VkDisplayModePropertiesKHR> modes(mode_count);
		res = vkGetDisplayModePropertiesKHR(physical_device, display, &mode_count, modes.data());
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkGetDisplayModePropertiesKHR (data) failed: ");
			return VK_NULL_HANDLE;
		}

		// Index 0 is the display's preferred (native) mode per spec.
		// If the caller asked for a specific resolution, try to match it.
		u32 best_mode_idx = 0;
		if (wi->surface_width != 0 && wi->surface_height != 0)
		{
			for (u32 i = 0; i < mode_count; i++)
			{
				if (modes[i].parameters.visibleRegion.width == wi->surface_width &&
					modes[i].parameters.visibleRegion.height == wi->surface_height)
				{
					best_mode_idx = i;
					break;
				}
			}
		}
		const VkDisplayModeKHR mode = modes[best_mode_idx].displayMode;
		const VkExtent2D mode_extent = modes[best_mode_idx].parameters.visibleRegion;
		INFO_LOG("VK_KHR_display: selected mode {}x{}@{}.{:03} Hz",
			mode_extent.width, mode_extent.height,
			modes[best_mode_idx].parameters.refreshRate / 1000,
			modes[best_mode_idx].parameters.refreshRate % 1000);

		u32 plane_count = 0;
		res = vkGetPhysicalDeviceDisplayPlanePropertiesKHR(physical_device, &plane_count, nullptr);
		if (res != VK_SUCCESS || plane_count == 0)
		{
			LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceDisplayPlanePropertiesKHR (count) failed: ");
			return VK_NULL_HANDLE;
		}

		std::vector<VkDisplayPlanePropertiesKHR> planes(plane_count);
		res = vkGetPhysicalDeviceDisplayPlanePropertiesKHR(physical_device, &plane_count, planes.data());
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceDisplayPlanePropertiesKHR (data) failed: ");
			return VK_NULL_HANDLE;
		}

		u32 selected_plane = UINT32_MAX;
		for (u32 i = 0; i < plane_count; i++)
		{
			// Skip planes already bound to a different display.
			if (planes[i].currentDisplay != VK_NULL_HANDLE && planes[i].currentDisplay != display)
				continue;

			u32 supported_count = 0;
			if (vkGetDisplayPlaneSupportedDisplaysKHR(physical_device, i, &supported_count, nullptr) != VK_SUCCESS ||
				supported_count == 0)
				continue;

			std::vector<VkDisplayKHR> supported(supported_count);
			vkGetDisplayPlaneSupportedDisplaysKHR(physical_device, i, &supported_count, supported.data());
			if (std::find(supported.begin(), supported.end(), display) != supported.end())
			{
				selected_plane = i;
				break;
			}
		}

		if (selected_plane == UINT32_MAX)
		{
			Console.Error("VK_KHR_display: no compatible plane found for selected display.");
			return VK_NULL_HANDLE;
		}

		VkDisplaySurfaceCreateInfoKHR surface_create_info = {};
		surface_create_info.sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR;
		surface_create_info.displayMode = mode;
		surface_create_info.planeIndex = selected_plane;
		surface_create_info.planeStackIndex = planes[selected_plane].currentStackIndex;
		surface_create_info.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		surface_create_info.globalAlpha = 1.0f;
		surface_create_info.alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
		surface_create_info.imageExtent = mode_extent;

		VkSurfaceKHR surface;
		res = vkCreateDisplayPlaneSurfaceKHR(instance, &surface_create_info, nullptr, &surface);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateDisplayPlaneSurfaceKHR failed: ");
			return VK_NULL_HANDLE;
		}

		// Reflect the actual selected mode back to the caller so the
		// swapchain sizes correctly even when 0x0 was passed in.
		wi->surface_width = mode_extent.width;
		wi->surface_height = mode_extent.height;
		wi->surface_refresh_rate =
			static_cast<float>(modes[best_mode_idx].parameters.refreshRate) / 1000.0f;

		return surface;
	}

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

	// If there is a single undefined surface format, the device doesn't care, so we'll just use RGBA
	if (surface_formats[0].format == VK_FORMAT_UNDEFINED)
		return VkSurfaceFormatKHR{VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

	// Try to find a suitable format.
	for (const VkSurfaceFormatKHR& surface_format : surface_formats)
	{
		// Some drivers seem to return a SRGB format here (Intel Mesa).
		// This results in gamma correction when presenting to the screen, which we don't want.
		// Use a linear format instead, if this is the case.
		return VkSurfaceFormatKHR{GetLinearFormat(surface_format.format), VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
	}

	Console.Error("Failed to find a suitable format for swap chain buffers.");
	return std::nullopt;
}

VKSwapChain::PresentStats VKSwapChain::GetPresentStats()
{
	const u64 acquire_n = s_acquire_count.load(std::memory_order_relaxed);
	const u64 present_n = s_present_count.load(std::memory_order_relaxed);
	return PresentStats{
		acquire_n,
		static_cast<double>(s_acquire_total_ns.load(std::memory_order_relaxed)) / 1'000'000.0,
		static_cast<double>(s_acquire_max_ns.load(std::memory_order_relaxed)) / 1'000'000.0,
		present_n,
		static_cast<double>(s_present_total_ns.load(std::memory_order_relaxed)) / 1'000'000.0,
		static_cast<double>(s_present_max_ns.load(std::memory_order_relaxed)) / 1'000'000.0,
		s_suboptimal_count.load(std::memory_order_relaxed),
		s_out_of_date_count.load(std::memory_order_relaxed),
	};
}

void VKSwapChain::ResetPresentStats()
{
	s_acquire_count.store(0, std::memory_order_relaxed);
	s_acquire_total_ns.store(0, std::memory_order_relaxed);
	s_acquire_max_ns.store(0, std::memory_order_relaxed);
	s_present_count.store(0, std::memory_order_relaxed);
	s_present_total_ns.store(0, std::memory_order_relaxed);
	s_present_max_ns.store(0, std::memory_order_relaxed);
	s_suboptimal_count.store(0, std::memory_order_relaxed);
	s_out_of_date_count.store(0, std::memory_order_relaxed);
}

void VKSwapChain::SetPresentStatsEnabled(bool enabled)
{
	s_stats_enabled.store(enabled, std::memory_order_relaxed);
}

bool VKSwapChain::IsPresentStatsEnabled()
{
	return s_stats_enabled.load(std::memory_order_relaxed);
}

void VKSwapChain::NoteAcquire(double ms, VkResult res)
{
	if (!s_stats_enabled.load(std::memory_order_relaxed))
		return;
	const u64 ns = static_cast<u64>(ms * 1'000'000.0);
	s_acquire_count.fetch_add(1, std::memory_order_relaxed);
	s_acquire_total_ns.fetch_add(ns, std::memory_order_relaxed);
	UpdateMax(s_acquire_max_ns, ns);
	if (res == VK_SUBOPTIMAL_KHR)
		s_suboptimal_count.fetch_add(1, std::memory_order_relaxed);
	else if (res == VK_ERROR_OUT_OF_DATE_KHR)
		s_out_of_date_count.fetch_add(1, std::memory_order_relaxed);
}

void VKSwapChain::NotePresent(double ms, VkResult res)
{
	if (!s_stats_enabled.load(std::memory_order_relaxed))
		return;
	const u64 ns = static_cast<u64>(ms * 1'000'000.0);
	s_present_count.fetch_add(1, std::memory_order_relaxed);
	s_present_total_ns.fetch_add(ns, std::memory_order_relaxed);
	UpdateMax(s_present_max_ns, ns);
	if (res == VK_SUBOPTIMAL_KHR)
		s_suboptimal_count.fetch_add(1, std::memory_order_relaxed);
	else if (res == VK_ERROR_OUT_OF_DATE_KHR)
		s_out_of_date_count.fetch_add(1, std::memory_order_relaxed);
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
			// Prefer FIFO_RELAXED (adaptive vsync): behaves exactly like FIFO while the app
			// keeps pace, but a frame that misses its refresh interval is presented late
			// (with tearing) instead of stalling a whole interval. This avoids the hard
			// 60->30 fps cliff on borderline titles — a big felt win on weak devices. Plain
			// FIFO is always available as the fallback when the driver lacks relaxed support.
			if (CheckForMode(VK_PRESENT_MODE_FIFO_RELAXED_KHR))
			{
				*present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
			}
			else
			{
				*present_mode = VK_PRESENT_MODE_FIFO_KHR;
			}
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
	// VK_KHR_display (VulkanDirect) + FIFO + 2 images stalls vkAcquireNextImageKHR
	// for ~1.5 vsync intervals per frame waiting for the display engine to release
	// the previously-presented image (measured on some tiler-class drivers). A third
	// image lets the GPU work on N+2 while N is on-screen and N+1 is queued, recovering
	// ~33% throughput. Default to 3 images for VulkanDirect, and for MAILBOX
	// present mode regardless of WSI.
	const bool use_triple =
		(m_window_info.type == WindowInfo::Type::VulkanDirect) ||
		(m_present_mode == VK_PRESENT_MODE_MAILBOX_KHR);
	const u32 desired_image_count = use_triple ? 3 : 2;
	u32 image_count = std::clamp<u32>(
		desired_image_count, surface_capabilities.minImageCount,
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

	// PowerVR (Imagination) driver bug: older drivers heavily corrupt the output
	// unless the swap-chain width is a multiple of 32. Ported from PPSSPP
	// (VulkanContext::InitSwapchain, issues #11743/#15773) — round the width down
	// to a /32 boundary on affected PowerVR drivers. ARMSX2 had zero PowerVR
	// handling, so this can only help. Gated by vendorID (0x1010) + driver version.
	if (GSDeviceVK::GetInstance()->IsDevicePowerVR() &&
		GSDeviceVK::GetInstance()->GetDeviceProperties().driverVersion < 0x00582558u &&
		(size.width & ~31u) >= surface_capabilities.minImageExtent.width)
	{
		size.width &= ~31u;
	}

	// One-shot log of the resolved swapchain config — useful for WSI-path diagnosis
	// (e.g. comparing VK_KHR_display vs a Wayland surface on the same device).
	{
		const char* wsi_name = "?";
		switch (m_window_info.type)
		{
			case WindowInfo::Type::Surfaceless: wsi_name = "Surfaceless"; break;
			case WindowInfo::Type::Win32:       wsi_name = "Win32"; break;
			case WindowInfo::Type::X11:         wsi_name = "X11"; break;
			case WindowInfo::Type::Wayland:     wsi_name = "Wayland"; break;
			case WindowInfo::Type::MacOS:       wsi_name = "MacOS"; break;
			case WindowInfo::Type::VulkanDirect: wsi_name = "VulkanDirect"; break;
		}
		Console.WriteLnFmt(
			"Vulkan: Swapchain {}x{} fmt={} colorspace={} present={} images={} (desired={} min={} max={}) wsi={}",
			size.width, size.height, static_cast<unsigned>(surface_format->format),
			static_cast<unsigned>(surface_format->colorSpace),
			PresentModeToString(m_present_mode), image_count,
			desired_image_count, surface_capabilities.minImageCount, surface_capabilities.maxImageCount,
			wsi_name);
	}

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
	VkSwapchainKHR old_swap_chain;
	// RDNA4 experences a 2s delay in the following 2-3 vkAcquireNextImageKHR calls if we pass the old swapchain to the new one.
	// Instead, pass null. This requires us to have freed the old image, which we already do with the swapchain maintenance extension.
	if (GSDeviceVK::GetInstance()->IsDeviceAMD() && GSDeviceVK::GetInstance()->GetOptionalExtensions().vk_swapchain_maintenance1)
	{
		vkDestroySwapchainKHR(GSDeviceVK::GetInstance()->GetDevice(), m_swap_chain, nullptr);
		old_swap_chain = VK_NULL_HANDLE;
	}
	else
		old_swap_chain = m_swap_chain;

	m_swap_chain = VK_NULL_HANDLE;

	// VK_EXT_swapchain_maintenance1 types/enums are aliases of VK_KHR_swapchain_maintenance1 types/enums.
	const VkSwapchainPresentModesCreateInfoKHR modes_info{VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_KHR, nullptr, 1u, &m_present_mode};

	// Some ARM Mali Vulkan drivers advertise VK_EXT_swapchain_maintenance1 but
	// vkCreateSwapchainKHR errors VK_ERROR_INITIALIZATION_FAILED whenever this pNext is
	// attached, regardless of present mode. Keep the extension enabled — the rest of its
	// surface (present-fence-info, release-swapchain-images) works fine — and just skip
	// the create-time pNext on ARM Mali.
	const bool use_present_modes_pnext =
		GSDeviceVK::GetInstance()->GetOptionalExtensions().vk_swapchain_maintenance1 &&
		!GSDeviceVK::GetInstance()->IsDeviceMali();

	// Now we can actually create the swap chain
	VkSwapchainCreateInfoKHR swap_chain_info = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		use_present_modes_pnext ? &modes_info : nullptr, 0, m_surface,
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
	for (u32 i = 0; i < image_count; i++)
	{
		std::unique_ptr<GSTextureVK> texture =
			GSTextureVK::Adopt(images[i], GSTexture::RenderTarget, GSTexture::Format::Color,
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

	const bool stats = s_stats_enabled.load(std::memory_order_relaxed);
	const Common::Timer::Value t_start = stats ? Common::Timer::GetCurrentValue() : 0;
	const VkResult res = vkAcquireNextImageKHR(GSDeviceVK::GetInstance()->GetDevice(), m_swap_chain, UINT64_MAX,
		m_semaphores[m_current_semaphore].available_semaphore, VK_NULL_HANDLE, &m_current_image);
	if (stats)
	{
		const double elapsed_ms =
			Common::Timer::ConvertValueToMilliseconds(Common::Timer::GetCurrentValue() - t_start);
		NoteAcquire(elapsed_ms, res);
	}
	m_image_acquire_result = res;
	return res;
}

void VKSwapChain::ReleaseCurrentImage()
{
	if (!m_image_acquire_result.has_value())
		return;

	if ((m_image_acquire_result.value() == VK_SUCCESS || m_image_acquire_result.value() == VK_SUBOPTIMAL_KHR) &&
		GSDeviceVK::GetInstance()->GetOptionalExtensions().vk_swapchain_maintenance1)
	{
		GSDeviceVK::GetInstance()->WaitForGPUIdle();

		// VK_EXT_swapchain_maintenance1 types/enums are aliases of VK_KHR_swapchain_maintenance1 types/enums.
		const VkReleaseSwapchainImagesInfoKHR info = {.sType = VK_STRUCTURE_TYPE_RELEASE_SWAPCHAIN_IMAGES_INFO_KHR,
			.swapchain = m_swap_chain,
			.imageIndexCount = 1,
			.pImageIndices = &m_current_image};

		VkResult res;
		if (GSDeviceVK::GetInstance()->GetOptionalExtensions().vk_swapchain_maintenance1_is_khr)
			res = vkReleaseSwapchainImagesKHR(GSDeviceVK::GetInstance()->GetDevice(), &info);
		else
			res = vkReleaseSwapchainImagesEXT(GSDeviceVK::GetInstance()->GetDevice(), &info);
		if (res != VK_SUCCESS)
			LOG_VULKAN_ERROR(res, "vkReleaseSwapchainImages() failed: ");
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
