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

#include "common/Vulkan/Loader.h"
#include "common/Vulkan/StreamBuffer.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct WindowInfo;

namespace Vulkan
{
	class SwapChain;

	class Context
	{
	public:
		enum : u32
		{
			NUM_COMMAND_BUFFERS = 3,
			TEXTURE_BUFFER_SIZE = 64 * 1024 * 1024,
		};

		struct OptionalExtensions
		{
			bool vk_ext_provoking_vertex : 1;
			bool vk_ext_memory_budget : 1;
			bool vk_khr_driver_properties : 1;
			bool vk_arm_rasterization_order_attachment_access : 1;
		};

		~Context();

		// Helper method to create a Vulkan instance.
		static VkInstance CreateVulkanInstance(
			const WindowInfo* wi, bool enable_debug_utils, bool enable_validation_layer);

		// Returns a list of Vulkan-compatible GPUs.
		using GPUList = std::vector<VkPhysicalDevice>;
		using GPUNameList = std::vector<std::string>;
		static GPUList EnumerateGPUs(VkInstance instance);
		static GPUNameList EnumerateGPUNames(VkInstance instance);

		// Creates a new context and sets it up as global.
		static bool Create(std::string_view gpu_name, const WindowInfo* wi, std::unique_ptr<SwapChain>* out_swap_chain,
			VkPresentModeKHR preferred_present_mode, bool threaded_presentation, bool enable_debug_utils,
			bool enable_validation_layer);

		// Destroys context.
		static void Destroy();

		// Enable/disable debug message runtime.
		bool EnableDebugUtils();
		void DisableDebugUtils();

		// Global state accessors
		__fi VkInstance GetVulkanInstance() const { return m_instance; }
		__fi VkPhysicalDevice GetPhysicalDevice() const { return m_physical_device; }
		__fi VkDevice GetDevice() const { return m_device; }
		__fi VmaAllocator GetAllocator() const { return m_allocator; }
		__fi VkQueue GetGraphicsQueue() const { return m_graphics_queue; }
		__fi u32 GetGraphicsQueueFamilyIndex() const { return m_graphics_queue_family_index; }
		__fi VkQueue GetPresentQueue() const { return m_present_queue; }
		__fi u32 GetPresentQueueFamilyIndex() const { return m_present_queue_family_index; }
		__fi const VkQueueFamilyProperties& GetGraphicsQueueProperties() const { return m_graphics_queue_properties; }
		__fi const VkPhysicalDeviceMemoryProperties& GetDeviceMemoryProperties() const
		{
			return m_device_memory_properties;
		}
		__fi const VkPhysicalDeviceProperties& GetDeviceProperties() const { return m_device_properties; }
		__fi const VkPhysicalDeviceFeatures& GetDeviceFeatures() const { return m_device_features; }
		__fi const VkPhysicalDeviceLimits& GetDeviceLimits() const { return m_device_properties.limits; }
		__fi const VkPhysicalDeviceDriverProperties& GetDeviceDriverProperties() const { return m_device_driver_properties; }
		__fi const OptionalExtensions& GetOptionalExtensions() const { return m_optional_extensions; }

		// Helpers for getting constants
		__fi VkDeviceSize GetUniformBufferAlignment() const
		{
			return m_device_properties.limits.minUniformBufferOffsetAlignment;
		}
		__fi VkDeviceSize GetTexelBufferAlignment() const
		{
			return m_device_properties.limits.minTexelBufferOffsetAlignment;
		}
		__fi VkDeviceSize GetStorageBufferAlignment() const
		{
			return m_device_properties.limits.minStorageBufferOffsetAlignment;
		}
		__fi VkDeviceSize GetBufferImageGranularity() const
		{
			return m_device_properties.limits.bufferImageGranularity;
		}
		__fi VkDeviceSize GetMaxImageDimension2D() const { return m_device_properties.limits.maxImageDimension2D; }

		// Creates a simple render pass.
		__ri VkRenderPass GetRenderPass(VkFormat color_format, VkFormat depth_format,
			VkAttachmentLoadOp color_load_op = VK_ATTACHMENT_LOAD_OP_LOAD,
			VkAttachmentStoreOp color_store_op = VK_ATTACHMENT_STORE_OP_STORE,
			VkAttachmentLoadOp depth_load_op = VK_ATTACHMENT_LOAD_OP_LOAD,
			VkAttachmentStoreOp depth_store_op = VK_ATTACHMENT_STORE_OP_STORE,
			VkAttachmentLoadOp stencil_load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			VkAttachmentStoreOp stencil_store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE, bool color_feedback_loop = false)
		{
			RenderPassCacheKey key = {};
			key.color_format = color_format;
			key.depth_format = depth_format;
			key.color_load_op = color_load_op;
			key.color_store_op = color_store_op;
			key.depth_load_op = depth_load_op;
			key.depth_store_op = depth_store_op;
			key.stencil_load_op = stencil_load_op;
			key.stencil_store_op = stencil_store_op;
			key.color_feedback_loop = color_feedback_loop;

			auto it = m_render_pass_cache.find(key.key);
			if (it != m_render_pass_cache.end())
				return it->second;

			return CreateCachedRenderPass(key);
		}

		// These command buffers are allocated per-frame. They are valid until the command buffer
		// is submitted, after that you should call these functions again.
		__fi u32 GetCurrentCommandBufferIndex() const { return m_current_frame; }
		__fi VkDescriptorPool GetGlobalDescriptorPool() const { return m_global_descriptor_pool; }
		__fi VkCommandBuffer GetCurrentCommandBuffer() const { return m_current_command_buffer; }
		__fi StreamBuffer& GetTextureUploadBuffer() { return m_texture_upload_buffer; }
		__fi VkDescriptorPool GetCurrentDescriptorPool() const
		{
			return m_frame_resources[m_current_frame].descriptor_pool;
		}
		VkCommandBuffer GetCurrentInitCommandBuffer();

		/// Allocates a descriptor set from the pool reserved for the current frame.
		VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout set_layout);

		/// Allocates a descriptor set from the pool reserved for the current frame.
		VkDescriptorSet AllocatePersistentDescriptorSet(VkDescriptorSetLayout set_layout);

		/// Frees a descriptor set allocated from the global pool.
		void FreeGlobalDescriptorSet(VkDescriptorSet set);

		// Gets the fence that will be signaled when the currently executing command buffer is
		// queued and executed. Do not wait for this fence before the buffer is executed.
		__fi VkFence GetCurrentCommandBufferFence() const { return m_frame_resources[m_current_frame].fence; }

		// Fence "counters" are used to track which commands have been completed by the GPU.
		// If the last completed fence counter is greater or equal to N, it means that the work
		// associated counter N has been completed by the GPU. The value of N to associate with
		// commands can be retreived by calling GetCurrentFenceCounter().
		u64 GetCompletedFenceCounter() const { return m_completed_fence_counter; }

		// Gets the fence that will be signaled when the currently executing command buffer is
		// queued and executed. Do not wait for this fence before the buffer is executed.
		u64 GetCurrentFenceCounter() const { return m_frame_resources[m_current_frame].fence_counter; }

		void SubmitCommandBuffer(VkSemaphore wait_semaphore = VK_NULL_HANDLE,
			VkSemaphore signal_semaphore = VK_NULL_HANDLE, VkSwapchainKHR present_swap_chain = VK_NULL_HANDLE,
			uint32_t present_image_index = 0xFFFFFFFF, bool submit_on_thread = false);
		void MoveToNextCommandBuffer();

		void ExecuteCommandBuffer(bool wait_for_completion);
		void WaitForPresentComplete();

		// Was the last present submitted to the queue a failure? If so, we must recreate our swapchain.
		bool CheckLastPresentFail();

		// Schedule a vulkan resource for destruction later on. This will occur when the command buffer
		// is next re-used, and the GPU has finished working with the specified resource.
		void DeferBufferDestruction(VkBuffer object);
		void DeferBufferDestruction(VkBuffer object, VmaAllocation allocation);
		void DeferBufferViewDestruction(VkBufferView object);
		void DeferDeviceMemoryDestruction(VkDeviceMemory object);
		void DeferFramebufferDestruction(VkFramebuffer object);
		void DeferImageDestruction(VkImage object);
		void DeferImageDestruction(VkImage object, VmaAllocation allocation);
		void DeferImageViewDestruction(VkImageView object);
		void DeferPipelineDestruction(VkPipeline pipeline);
		void DeferSamplerDestruction(VkSampler sampler);

		// Wait for a fence to be completed.
		// Also invokes callbacks for completion.
		void WaitForFenceCounter(u64 fence_counter);

		void WaitForGPUIdle();

		float GetAndResetAccumulatedGPUTime();
		void SetEnableGPUTiming(bool enabled);

	private:
		Context(VkInstance instance, VkPhysicalDevice physical_device);

		union RenderPassCacheKey
		{
			struct
			{
				u32 color_format : 8;
				u32 depth_format : 8;
				u32 color_load_op : 2;
				u32 color_store_op : 1;
				u32 depth_load_op : 2;
				u32 depth_store_op : 1;
				u32 stencil_load_op : 2;
				u32 stencil_store_op : 1;
				u32 color_feedback_loop : 1;
			};

			u32 key;
		};

		using ExtensionList = std::vector<const char*>;
		static bool SelectInstanceExtensions(
			ExtensionList* extension_list, const WindowInfo* wi, bool enable_debug_utils);
		bool SelectDeviceExtensions(ExtensionList* extension_list, bool enable_surface);
		bool SelectDeviceFeatures(const VkPhysicalDeviceFeatures* required_features);
		bool CreateDevice(VkSurfaceKHR surface, bool enable_validation_layer, const char** required_device_extensions,
			u32 num_required_device_extensions, const char** required_device_layers, u32 num_required_device_layers,
			const VkPhysicalDeviceFeatures* required_features);
		void ProcessDeviceExtensions();

		bool CreateAllocator();
		void DestroyAllocator();
		bool CreateCommandBuffers();
		void DestroyCommandBuffers();
		bool CreateGlobalDescriptorPool();
		void DestroyGlobalDescriptorPool();
		bool CreateTextureStreamBuffer();

		VkRenderPass CreateCachedRenderPass(RenderPassCacheKey key);
		void DestroyRenderPassCache();

		void ActivateCommandBuffer(u32 index);
		void WaitForCommandBufferCompletion(u32 index);

		void DoSubmitCommandBuffer(u32 index, VkSemaphore wait_semaphore, VkSemaphore signal_semaphore);
		void DoPresent(VkSemaphore wait_semaphore, VkSwapchainKHR present_swap_chain, uint32_t present_image_index);
		void WaitForPresentComplete(std::unique_lock<std::mutex>& lock);
		void PresentThread();
		void StartPresentThread();
		void StopPresentThread();

		struct FrameResources
		{
			// [0] - Init (upload) command buffer, [1] - draw command buffer
			VkCommandPool command_pool = VK_NULL_HANDLE;
			std::array<VkCommandBuffer, 2> command_buffers{VK_NULL_HANDLE, VK_NULL_HANDLE};
			VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
			VkFence fence = VK_NULL_HANDLE;
			u64 fence_counter = 0;
			bool init_buffer_used = false;
			bool needs_fence_wait = false;
			bool timestamp_written = false;

			std::vector<std::function<void()>> cleanup_resources;
		};

		VkInstance m_instance = VK_NULL_HANDLE;
		VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
		VkDevice m_device = VK_NULL_HANDLE;
		VmaAllocator m_allocator = VK_NULL_HANDLE;

		VkCommandBuffer m_current_command_buffer = VK_NULL_HANDLE;

		VkDescriptorPool m_global_descriptor_pool = VK_NULL_HANDLE;

		VkQueue m_graphics_queue = VK_NULL_HANDLE;
		VkQueue m_present_queue = VK_NULL_HANDLE;
		u32 m_graphics_queue_family_index = 0;
		u32 m_present_queue_family_index = 0;

		VkQueryPool m_timestamp_query_pool = VK_NULL_HANDLE;
		float m_accumulated_gpu_time = 0.0f;
		bool m_gpu_timing_enabled = false;
		bool m_gpu_timing_supported = false;

		std::array<FrameResources, NUM_COMMAND_BUFFERS> m_frame_resources;
		u64 m_next_fence_counter = 1;
		u64 m_completed_fence_counter = 0;
		u32 m_current_frame = 0;

		StreamBuffer m_texture_upload_buffer;

		std::atomic_bool m_last_present_failed{false};
		std::atomic_bool m_present_done{true};
		std::mutex m_present_mutex;
		std::condition_variable m_present_queued_cv;
		std::condition_variable m_present_done_cv;
		std::thread m_present_thread;
		std::atomic_bool m_present_thread_done{false};

		struct QueuedPresent
		{
			VkSemaphore wait_semaphore;
			VkSemaphore signal_semaphore;
			VkSwapchainKHR present_swap_chain;
			u32 command_buffer_index;
			u32 present_image_index;
		};

		QueuedPresent m_queued_present = {};

		std::map<u32, VkRenderPass> m_render_pass_cache;

		VkDebugUtilsMessengerEXT m_debug_messenger_callback = VK_NULL_HANDLE;

		VkQueueFamilyProperties m_graphics_queue_properties = {};
		VkPhysicalDeviceFeatures m_device_features = {};
		VkPhysicalDeviceProperties m_device_properties = {};
		VkPhysicalDeviceMemoryProperties m_device_memory_properties = {};
		VkPhysicalDeviceDriverPropertiesKHR m_device_driver_properties = {};
		OptionalExtensions m_optional_extensions = {};
	};

} // namespace Vulkan

extern std::unique_ptr<Vulkan::Context> g_vulkan_context;
