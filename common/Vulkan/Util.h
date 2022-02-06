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
#include "common/StringUtil.h"
#include "common/Vulkan/Loader.h"
#include <algorithm>
#include <array>
#include <cstdarg>
#include <string_view>

namespace Vulkan
{
	namespace Util
	{
		bool IsDepthFormat(VkFormat format);
		bool IsDepthStencilFormat(VkFormat format);
		VkFormat GetLinearFormat(VkFormat format);
		u32 GetTexelSize(VkFormat format);

		// Safe destroy helpers
		void SafeDestroyFramebuffer(VkFramebuffer& fb);
		void SafeDestroyShaderModule(VkShaderModule& sm);
		void SafeDestroyPipeline(VkPipeline& p);
		void SafeDestroyPipelineLayout(VkPipelineLayout& pl);
		void SafeDestroyDescriptorSetLayout(VkDescriptorSetLayout& dsl);
		void SafeDestroyBufferView(VkBufferView& bv);
		void SafeDestroyImageView(VkImageView& iv);
		void SafeDestroySampler(VkSampler& samp);
		void SafeDestroySemaphore(VkSemaphore& sem);
		void SafeFreeGlobalDescriptorSet(VkDescriptorSet& ds);

		// Wrapper for creating an barrier on a buffer
		void BufferMemoryBarrier(VkCommandBuffer command_buffer, VkBuffer buffer, VkAccessFlags src_access_mask,
			VkAccessFlags dst_access_mask, VkDeviceSize offset, VkDeviceSize size, VkPipelineStageFlags src_stage_mask,
			VkPipelineStageFlags dst_stage_mask);

		// Adds a structure to a chain.
		void AddPointerToChain(void* head, const void* ptr);

		const char* VkResultToString(VkResult res);
		const char* PresentModeToString(VkPresentModeKHR mode);
		void LogVulkanResult(const char* func_name, VkResult res, const char* msg, ...) /*printflike(4, 5)*/;

#define LOG_VULKAN_ERROR(res, ...) ::Vulkan::Util::LogVulkanResult(__func__, res, __VA_ARGS__)

#if defined(_DEBUG)

// We can't use the templates below because they're all the same type on 32-bit.
#if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) && !defined(__ILP32__)) || defined(_M_X64) || \
	defined(__ia64) || defined(_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)
#define ENABLE_VULKAN_DEBUG_OBJECTS 1
#endif

#endif

#ifdef ENABLE_VULKAN_DEBUG_OBJECTS

		// Provides a compile-time mapping between a Vulkan-type into its matching VkObjectType
		template <typename T>
		struct VkObjectTypeMap;

		// clang-format off
template<> struct VkObjectTypeMap<VkInstance                > { using type = VkInstance                ; static constexpr VkObjectType value = VK_OBJECT_TYPE_INSTANCE;                   };
template<> struct VkObjectTypeMap<VkPhysicalDevice          > { using type = VkPhysicalDevice          ; static constexpr VkObjectType value = VK_OBJECT_TYPE_PHYSICAL_DEVICE;            };
template<> struct VkObjectTypeMap<VkDevice                  > { using type = VkDevice                  ; static constexpr VkObjectType value = VK_OBJECT_TYPE_DEVICE;                     };
template<> struct VkObjectTypeMap<VkQueue                   > { using type = VkQueue                   ; static constexpr VkObjectType value = VK_OBJECT_TYPE_QUEUE;                      };
template<> struct VkObjectTypeMap<VkSemaphore               > { using type = VkSemaphore               ; static constexpr VkObjectType value = VK_OBJECT_TYPE_SEMAPHORE;                  };
template<> struct VkObjectTypeMap<VkCommandBuffer           > { using type = VkCommandBuffer           ; static constexpr VkObjectType value = VK_OBJECT_TYPE_COMMAND_BUFFER;             };
template<> struct VkObjectTypeMap<VkFence                   > { using type = VkFence                   ; static constexpr VkObjectType value = VK_OBJECT_TYPE_FENCE;                      };
template<> struct VkObjectTypeMap<VkDeviceMemory            > { using type = VkDeviceMemory            ; static constexpr VkObjectType value = VK_OBJECT_TYPE_DEVICE_MEMORY;              };
template<> struct VkObjectTypeMap<VkBuffer                  > { using type = VkBuffer                  ; static constexpr VkObjectType value = VK_OBJECT_TYPE_BUFFER;                     };
template<> struct VkObjectTypeMap<VkImage                   > { using type = VkImage                   ; static constexpr VkObjectType value = VK_OBJECT_TYPE_IMAGE;                      };
template<> struct VkObjectTypeMap<VkEvent                   > { using type = VkEvent                   ; static constexpr VkObjectType value = VK_OBJECT_TYPE_EVENT;                      };
template<> struct VkObjectTypeMap<VkQueryPool               > { using type = VkQueryPool               ; static constexpr VkObjectType value = VK_OBJECT_TYPE_QUERY_POOL;                 };
template<> struct VkObjectTypeMap<VkBufferView              > { using type = VkBufferView              ; static constexpr VkObjectType value = VK_OBJECT_TYPE_BUFFER_VIEW;                };
template<> struct VkObjectTypeMap<VkImageView               > { using type = VkImageView               ; static constexpr VkObjectType value = VK_OBJECT_TYPE_IMAGE_VIEW;                 };
template<> struct VkObjectTypeMap<VkShaderModule            > { using type = VkShaderModule            ; static constexpr VkObjectType value = VK_OBJECT_TYPE_SHADER_MODULE;              };
template<> struct VkObjectTypeMap<VkPipelineCache           > { using type = VkPipelineCache           ; static constexpr VkObjectType value = VK_OBJECT_TYPE_PIPELINE_CACHE;             };
template<> struct VkObjectTypeMap<VkPipelineLayout          > { using type = VkPipelineLayout          ; static constexpr VkObjectType value = VK_OBJECT_TYPE_PIPELINE_LAYOUT;            };
template<> struct VkObjectTypeMap<VkRenderPass              > { using type = VkRenderPass              ; static constexpr VkObjectType value = VK_OBJECT_TYPE_RENDER_PASS;                };
template<> struct VkObjectTypeMap<VkPipeline                > { using type = VkPipeline                ; static constexpr VkObjectType value = VK_OBJECT_TYPE_PIPELINE;                   };
template<> struct VkObjectTypeMap<VkDescriptorSetLayout     > { using type = VkDescriptorSetLayout     ; static constexpr VkObjectType value = VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT;      };
template<> struct VkObjectTypeMap<VkSampler                 > { using type = VkSampler                 ; static constexpr VkObjectType value = VK_OBJECT_TYPE_SAMPLER;                    };
template<> struct VkObjectTypeMap<VkDescriptorPool          > { using type = VkDescriptorPool          ; static constexpr VkObjectType value = VK_OBJECT_TYPE_DESCRIPTOR_POOL;            };
template<> struct VkObjectTypeMap<VkDescriptorSet           > { using type = VkDescriptorSet           ; static constexpr VkObjectType value = VK_OBJECT_TYPE_DESCRIPTOR_SET;             };
template<> struct VkObjectTypeMap<VkFramebuffer             > { using type = VkFramebuffer             ; static constexpr VkObjectType value = VK_OBJECT_TYPE_FRAMEBUFFER;                };
template<> struct VkObjectTypeMap<VkCommandPool             > { using type = VkCommandPool             ; static constexpr VkObjectType value = VK_OBJECT_TYPE_COMMAND_POOL;               };
template<> struct VkObjectTypeMap<VkDescriptorUpdateTemplate> { using type = VkDescriptorUpdateTemplate; static constexpr VkObjectType value = VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE; };
template<> struct VkObjectTypeMap<VkSurfaceKHR              > { using type = VkSurfaceKHR              ; static constexpr VkObjectType value = VK_OBJECT_TYPE_SURFACE_KHR;                };
template<> struct VkObjectTypeMap<VkSwapchainKHR            > { using type = VkSwapchainKHR            ; static constexpr VkObjectType value = VK_OBJECT_TYPE_SWAPCHAIN_KHR;              };
template<> struct VkObjectTypeMap<VkDebugUtilsMessengerEXT  > { using type = VkDebugUtilsMessengerEXT  ; static constexpr VkObjectType value = VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT;  };
		// clang-format on

#endif

		static inline void SetObjectName(
			VkDevice device, void* object_handle, VkObjectType object_type, const char* format, va_list ap)
		{
#ifdef ENABLE_VULKAN_DEBUG_OBJECTS
			if (!vkSetDebugUtilsObjectNameEXT)
			{
				return;
			}

			const std::string str(StringUtil::StdStringFromFormatV(format, ap));
			const VkDebugUtilsObjectNameInfoEXT nameInfo{VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, nullptr,
				object_type, reinterpret_cast<uint64_t>(object_handle), str.c_str()};
			vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
#endif
		}

		template <typename T>
		static inline void SetObjectName(VkDevice device, T object_handle, const char* format, ...)
		{
#ifdef ENABLE_VULKAN_DEBUG_OBJECTS
			std::va_list ap;
			va_start(ap, format);
			SetObjectName(device, reinterpret_cast<void*>((typename VkObjectTypeMap<T>::type)object_handle),
				VkObjectTypeMap<T>::value, format, ap);
			va_end(ap);
#endif
		}
	} // namespace Util
} // namespace Vulkan
