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

// Expands the VULKAN_ENTRY_POINT macro for each function when this file is included.
// Parameters: Function name, is required
// VULKAN_MODULE_ENTRY_POINT is for functions in vulkan-1.dll
// VULKAN_INSTANCE_ENTRY_POINT is for instance-specific functions.
// VULKAN_DEVICE_ENTRY_POINT is for device-specific functions.

#ifdef VULKAN_MODULE_ENTRY_POINT

VULKAN_MODULE_ENTRY_POINT(vkCreateInstance, true)
VULKAN_MODULE_ENTRY_POINT(vkGetInstanceProcAddr, true)
VULKAN_MODULE_ENTRY_POINT(vkEnumerateInstanceExtensionProperties, true)
VULKAN_MODULE_ENTRY_POINT(vkEnumerateInstanceLayerProperties, true)
VULKAN_MODULE_ENTRY_POINT(vkEnumerateInstanceVersion, false)
VULKAN_MODULE_ENTRY_POINT(vkDestroyInstance, true)

#endif // VULKAN_MODULE_ENTRY_POINT

#ifdef VULKAN_INSTANCE_ENTRY_POINT

VULKAN_INSTANCE_ENTRY_POINT(vkGetDeviceProcAddr, true)
VULKAN_INSTANCE_ENTRY_POINT(vkEnumeratePhysicalDevices, true)
VULKAN_INSTANCE_ENTRY_POINT(vkGetPhysicalDeviceFeatures, true)
VULKAN_INSTANCE_ENTRY_POINT(vkGetPhysicalDeviceFormatProperties, true)
VULKAN_INSTANCE_ENTRY_POINT(vkGetPhysicalDeviceImageFormatProperties, true)
VULKAN_INSTANCE_ENTRY_POINT(vkGetPhysicalDeviceProperties, true)
VULKAN_INSTANCE_ENTRY_POINT(vkGetPhysicalDeviceQueueFamilyProperties, true)
VULKAN_INSTANCE_ENTRY_POINT(vkGetPhysicalDeviceMemoryProperties, true)
VULKAN_INSTANCE_ENTRY_POINT(vkCreateDevice, true)
VULKAN_INSTANCE_ENTRY_POINT(vkEnumerateDeviceExtensionProperties, true)
VULKAN_INSTANCE_ENTRY_POINT(vkEnumerateDeviceLayerProperties, true)
VULKAN_INSTANCE_ENTRY_POINT(vkGetPhysicalDeviceSparseImageFormatProperties, true)
VULKAN_INSTANCE_ENTRY_POINT(vkDestroySurfaceKHR, false)
VULKAN_INSTANCE_ENTRY_POINT(vkGetPhysicalDeviceSurfaceSupportKHR, false)
VULKAN_INSTANCE_ENTRY_POINT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR, false)
VULKAN_INSTANCE_ENTRY_POINT(vkGetPhysicalDeviceSurfaceFormatsKHR, false)
VULKAN_INSTANCE_ENTRY_POINT(vkGetPhysicalDeviceSurfacePresentModesKHR, false)

#if defined(VK_USE_PLATFORM_WIN32_KHR)
VULKAN_INSTANCE_ENTRY_POINT(vkCreateWin32SurfaceKHR, false)
VULKAN_INSTANCE_ENTRY_POINT(vkGetPhysicalDeviceWin32PresentationSupportKHR, false)
#endif

#if defined(VK_USE_PLATFORM_XLIB_KHR)
VULKAN_INSTANCE_ENTRY_POINT(vkCreateXlibSurfaceKHR, false)
VULKAN_INSTANCE_ENTRY_POINT(vkGetPhysicalDeviceXlibPresentationSupportKHR, false)
#endif

#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
VULKAN_INSTANCE_ENTRY_POINT(vkCreateWaylandSurfaceKHR, false)
#endif

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
VULKAN_INSTANCE_ENTRY_POINT(vkCreateAndroidSurfaceKHR, false)
#endif

#if defined(VK_USE_PLATFORM_MACOS_MVK)
VULKAN_INSTANCE_ENTRY_POINT(vkCreateMacOSSurfaceMVK, false)
#endif

#if defined(VK_USE_PLATFORM_METAL_EXT)
VULKAN_INSTANCE_ENTRY_POINT(vkCreateMetalSurfaceEXT, false)
#endif

// VK_EXT_debug_utils
VULKAN_INSTANCE_ENTRY_POINT(vkCmdBeginDebugUtilsLabelEXT, false)
VULKAN_INSTANCE_ENTRY_POINT(vkCmdEndDebugUtilsLabelEXT, false)
VULKAN_INSTANCE_ENTRY_POINT(vkCmdInsertDebugUtilsLabelEXT, false)
VULKAN_INSTANCE_ENTRY_POINT(vkCreateDebugUtilsMessengerEXT, false)
VULKAN_INSTANCE_ENTRY_POINT(vkDestroyDebugUtilsMessengerEXT, false)
VULKAN_INSTANCE_ENTRY_POINT(vkQueueBeginDebugUtilsLabelEXT, false)
VULKAN_INSTANCE_ENTRY_POINT(vkQueueEndDebugUtilsLabelEXT, false)
VULKAN_INSTANCE_ENTRY_POINT(vkQueueInsertDebugUtilsLabelEXT, false)
VULKAN_INSTANCE_ENTRY_POINT(vkSetDebugUtilsObjectNameEXT, false)
VULKAN_INSTANCE_ENTRY_POINT(vkSetDebugUtilsObjectTagEXT, false)
VULKAN_INSTANCE_ENTRY_POINT(vkSubmitDebugUtilsMessageEXT, false)

VULKAN_INSTANCE_ENTRY_POINT(vkGetPhysicalDeviceSurfaceCapabilities2KHR, false)

VULKAN_INSTANCE_ENTRY_POINT(vkGetPhysicalDeviceDisplayPropertiesKHR, false)
VULKAN_INSTANCE_ENTRY_POINT(vkGetPhysicalDeviceDisplayPlanePropertiesKHR, false)
VULKAN_INSTANCE_ENTRY_POINT(vkGetDisplayPlaneSupportedDisplaysKHR, false)
VULKAN_INSTANCE_ENTRY_POINT(vkGetDisplayModePropertiesKHR, false)
VULKAN_INSTANCE_ENTRY_POINT(vkCreateDisplayModeKHR, false)
VULKAN_INSTANCE_ENTRY_POINT(vkGetDisplayPlaneCapabilitiesKHR, false)
VULKAN_INSTANCE_ENTRY_POINT(vkCreateDisplayPlaneSurfaceKHR, false)

// Vulkan 1.1 functions.
VULKAN_INSTANCE_ENTRY_POINT(vkGetPhysicalDeviceFeatures2, true)
VULKAN_INSTANCE_ENTRY_POINT(vkGetPhysicalDeviceProperties2, true)
VULKAN_INSTANCE_ENTRY_POINT(vkGetPhysicalDeviceMemoryProperties2, true)

#endif // VULKAN_INSTANCE_ENTRY_POINT

#ifdef VULKAN_DEVICE_ENTRY_POINT

VULKAN_DEVICE_ENTRY_POINT(vkDestroyDevice, true)
VULKAN_DEVICE_ENTRY_POINT(vkGetDeviceQueue, true)
VULKAN_DEVICE_ENTRY_POINT(vkQueueSubmit, true)
VULKAN_DEVICE_ENTRY_POINT(vkQueueWaitIdle, true)
VULKAN_DEVICE_ENTRY_POINT(vkDeviceWaitIdle, true)
VULKAN_DEVICE_ENTRY_POINT(vkAllocateMemory, true)
VULKAN_DEVICE_ENTRY_POINT(vkFreeMemory, true)
VULKAN_DEVICE_ENTRY_POINT(vkMapMemory, true)
VULKAN_DEVICE_ENTRY_POINT(vkUnmapMemory, true)
VULKAN_DEVICE_ENTRY_POINT(vkFlushMappedMemoryRanges, true)
VULKAN_DEVICE_ENTRY_POINT(vkInvalidateMappedMemoryRanges, true)
VULKAN_DEVICE_ENTRY_POINT(vkGetDeviceMemoryCommitment, true)
VULKAN_DEVICE_ENTRY_POINT(vkBindBufferMemory, true)
VULKAN_DEVICE_ENTRY_POINT(vkBindImageMemory, true)
VULKAN_DEVICE_ENTRY_POINT(vkGetBufferMemoryRequirements, true)
VULKAN_DEVICE_ENTRY_POINT(vkGetImageMemoryRequirements, true)
VULKAN_DEVICE_ENTRY_POINT(vkGetImageSparseMemoryRequirements, true)
VULKAN_DEVICE_ENTRY_POINT(vkQueueBindSparse, true)
VULKAN_DEVICE_ENTRY_POINT(vkCreateFence, true)
VULKAN_DEVICE_ENTRY_POINT(vkDestroyFence, true)
VULKAN_DEVICE_ENTRY_POINT(vkResetFences, true)
VULKAN_DEVICE_ENTRY_POINT(vkGetFenceStatus, true)
VULKAN_DEVICE_ENTRY_POINT(vkWaitForFences, true)
VULKAN_DEVICE_ENTRY_POINT(vkCreateSemaphore, true)
VULKAN_DEVICE_ENTRY_POINT(vkDestroySemaphore, true)
VULKAN_DEVICE_ENTRY_POINT(vkCreateEvent, true)
VULKAN_DEVICE_ENTRY_POINT(vkDestroyEvent, true)
VULKAN_DEVICE_ENTRY_POINT(vkGetEventStatus, true)
VULKAN_DEVICE_ENTRY_POINT(vkSetEvent, true)
VULKAN_DEVICE_ENTRY_POINT(vkResetEvent, true)
VULKAN_DEVICE_ENTRY_POINT(vkCreateQueryPool, true)
VULKAN_DEVICE_ENTRY_POINT(vkDestroyQueryPool, true)
VULKAN_DEVICE_ENTRY_POINT(vkGetQueryPoolResults, true)
VULKAN_DEVICE_ENTRY_POINT(vkCreateBuffer, true)
VULKAN_DEVICE_ENTRY_POINT(vkDestroyBuffer, true)
VULKAN_DEVICE_ENTRY_POINT(vkCreateBufferView, true)
VULKAN_DEVICE_ENTRY_POINT(vkDestroyBufferView, true)
VULKAN_DEVICE_ENTRY_POINT(vkCreateImage, true)
VULKAN_DEVICE_ENTRY_POINT(vkDestroyImage, true)
VULKAN_DEVICE_ENTRY_POINT(vkGetImageSubresourceLayout, true)
VULKAN_DEVICE_ENTRY_POINT(vkCreateImageView, true)
VULKAN_DEVICE_ENTRY_POINT(vkDestroyImageView, true)
VULKAN_DEVICE_ENTRY_POINT(vkCreateShaderModule, true)
VULKAN_DEVICE_ENTRY_POINT(vkDestroyShaderModule, true)
VULKAN_DEVICE_ENTRY_POINT(vkCreatePipelineCache, true)
VULKAN_DEVICE_ENTRY_POINT(vkDestroyPipelineCache, true)
VULKAN_DEVICE_ENTRY_POINT(vkGetPipelineCacheData, true)
VULKAN_DEVICE_ENTRY_POINT(vkMergePipelineCaches, true)
VULKAN_DEVICE_ENTRY_POINT(vkCreateGraphicsPipelines, true)
VULKAN_DEVICE_ENTRY_POINT(vkCreateComputePipelines, true)
VULKAN_DEVICE_ENTRY_POINT(vkDestroyPipeline, true)
VULKAN_DEVICE_ENTRY_POINT(vkCreatePipelineLayout, true)
VULKAN_DEVICE_ENTRY_POINT(vkDestroyPipelineLayout, true)
VULKAN_DEVICE_ENTRY_POINT(vkCreateSampler, true)
VULKAN_DEVICE_ENTRY_POINT(vkDestroySampler, true)
VULKAN_DEVICE_ENTRY_POINT(vkCreateDescriptorSetLayout, true)
VULKAN_DEVICE_ENTRY_POINT(vkDestroyDescriptorSetLayout, true)
VULKAN_DEVICE_ENTRY_POINT(vkCreateDescriptorPool, true)
VULKAN_DEVICE_ENTRY_POINT(vkDestroyDescriptorPool, true)
VULKAN_DEVICE_ENTRY_POINT(vkResetDescriptorPool, true)
VULKAN_DEVICE_ENTRY_POINT(vkAllocateDescriptorSets, true)
VULKAN_DEVICE_ENTRY_POINT(vkFreeDescriptorSets, true)
VULKAN_DEVICE_ENTRY_POINT(vkUpdateDescriptorSets, true)
VULKAN_DEVICE_ENTRY_POINT(vkCreateFramebuffer, true)
VULKAN_DEVICE_ENTRY_POINT(vkDestroyFramebuffer, true)
VULKAN_DEVICE_ENTRY_POINT(vkCreateRenderPass, true)
VULKAN_DEVICE_ENTRY_POINT(vkDestroyRenderPass, true)
VULKAN_DEVICE_ENTRY_POINT(vkGetRenderAreaGranularity, true)
VULKAN_DEVICE_ENTRY_POINT(vkCreateCommandPool, true)
VULKAN_DEVICE_ENTRY_POINT(vkDestroyCommandPool, true)
VULKAN_DEVICE_ENTRY_POINT(vkResetCommandPool, true)
VULKAN_DEVICE_ENTRY_POINT(vkAllocateCommandBuffers, true)
VULKAN_DEVICE_ENTRY_POINT(vkFreeCommandBuffers, true)
VULKAN_DEVICE_ENTRY_POINT(vkBeginCommandBuffer, true)
VULKAN_DEVICE_ENTRY_POINT(vkEndCommandBuffer, true)
VULKAN_DEVICE_ENTRY_POINT(vkResetCommandBuffer, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdBindPipeline, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdSetViewport, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdSetScissor, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdSetLineWidth, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdSetDepthBias, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdSetBlendConstants, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdSetDepthBounds, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdSetStencilCompareMask, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdSetStencilWriteMask, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdSetStencilReference, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdBindDescriptorSets, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdBindIndexBuffer, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdBindVertexBuffers, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdDraw, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdDrawIndexed, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdDrawIndirect, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdDrawIndexedIndirect, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdDispatch, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdDispatchIndirect, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdCopyBuffer, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdCopyImage, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdBlitImage, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdCopyBufferToImage, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdCopyImageToBuffer, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdUpdateBuffer, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdFillBuffer, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdClearColorImage, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdClearDepthStencilImage, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdClearAttachments, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdResolveImage, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdSetEvent, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdResetEvent, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdWaitEvents, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdPipelineBarrier, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdBeginQuery, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdEndQuery, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdResetQueryPool, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdWriteTimestamp, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdCopyQueryPoolResults, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdPushConstants, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdBeginRenderPass, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdNextSubpass, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdEndRenderPass, true)
VULKAN_DEVICE_ENTRY_POINT(vkCmdExecuteCommands, true)
VULKAN_DEVICE_ENTRY_POINT(vkCreateSwapchainKHR, false)
VULKAN_DEVICE_ENTRY_POINT(vkDestroySwapchainKHR, false)
VULKAN_DEVICE_ENTRY_POINT(vkGetSwapchainImagesKHR, false)
VULKAN_DEVICE_ENTRY_POINT(vkAcquireNextImageKHR, false)
VULKAN_DEVICE_ENTRY_POINT(vkQueuePresentKHR, false)

// Vulkan 1.1 functions.
VULKAN_DEVICE_ENTRY_POINT(vkGetBufferMemoryRequirements2, true)
VULKAN_DEVICE_ENTRY_POINT(vkGetImageMemoryRequirements2, true)
VULKAN_DEVICE_ENTRY_POINT(vkBindBufferMemory2, true)
VULKAN_DEVICE_ENTRY_POINT(vkBindImageMemory2, true)

#ifdef SUPPORTS_VULKAN_EXCLUSIVE_FULLSCREEN
VULKAN_DEVICE_ENTRY_POINT(vkAcquireFullScreenExclusiveModeEXT, false)
VULKAN_DEVICE_ENTRY_POINT(vkReleaseFullScreenExclusiveModeEXT, false)
#endif

#endif // VULKAN_DEVICE_ENTRY_POINT
