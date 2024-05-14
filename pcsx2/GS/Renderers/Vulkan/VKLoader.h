// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

class Error;

#define VK_NO_PROTOTYPES

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR

// vulkan.h pulls in windows.h on Windows, so we need to include our replacement header first
#include "common/RedtapeWindows.h"
#endif

#if defined(X11_API)
#define VK_USE_PLATFORM_XLIB_KHR
#endif

#if defined(WAYLAND_API)
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif

#if defined(__APPLE__)
#define VK_USE_PLATFORM_METAL_EXT
#endif

#include "vulkan/vulkan.h"

#if defined(X11_API)

// This breaks a bunch of our code. They shouldn't be #defines in the first place.
#ifdef None
#undef None
#endif
#ifdef Status
#undef Status
#endif
#ifdef CursorShape
#undef CursorShape
#endif
#ifdef KeyPress
#undef KeyPress
#endif
#ifdef KeyRelease
#undef KeyRelease
#endif
#ifdef FocusIn
#undef FocusIn
#endif
#ifdef FocusOut
#undef FocusOut
#endif
#ifdef FontChange
#undef FontChange
#endif
#ifdef Expose
#undef Expose
#endif
#ifdef Unsorted
#undef Unsorted
#endif
#ifdef Bool
#undef Bool
#endif

#endif

#include "VKEntryPoints.h"

// We include vk_mem_alloc globally, so we don't accidentally include it before the vulkan header somewhere.
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#pragma clang diagnostic ignored "-Wunused-variable"
#endif

#define VMA_STATIC_VULKAN_FUNCTIONS 1
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#define VMA_STATS_STRING_ENABLED 0
#include "vk_mem_alloc.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace Vulkan
{
	bool IsVulkanLibraryLoaded();
	bool LoadVulkanLibrary(Error* error);
	bool LoadVulkanInstanceFunctions(VkInstance instance);
	bool LoadVulkanDeviceFunctions(VkDevice device);
	void UnloadVulkanLibrary();
	void ResetVulkanLibraryFunctionPointers();
} // namespace Vulkan
