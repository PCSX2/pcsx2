/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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
	bool LoadVulkanLibrary();
	bool LoadVulkanInstanceFunctions(VkInstance instance);
	bool LoadVulkanDeviceFunctions(VkDevice device);
	void UnloadVulkanLibrary();
	void ResetVulkanLibraryFunctionPointers();
} // namespace Vulkan
