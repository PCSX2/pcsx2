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

#include "common/Vulkan/Context.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "common/StringUtil.h"
#include "common/Vulkan/SwapChain.h"
#include "common/Vulkan/Util.h"
#include <algorithm>
#include <array>
#include <cstring>

std::unique_ptr<Vulkan::Context> g_vulkan_context;

// Tweakables
enum : u32
{
	MAX_DRAW_CALLS_PER_FRAME = 8192,
	MAX_COMBINED_IMAGE_SAMPLER_DESCRIPTORS_PER_FRAME = 2 * MAX_DRAW_CALLS_PER_FRAME,
	MAX_SAMPLED_IMAGE_DESCRIPTORS_PER_FRAME =
		MAX_DRAW_CALLS_PER_FRAME, // assume at least half our draws aren't going to be shuffle/blending
	MAX_INPUT_ATTACHMENT_IMAGE_DESCRIPTORS_PER_FRAME = MAX_DRAW_CALLS_PER_FRAME,
	MAX_DESCRIPTOR_SETS_PER_FRAME = MAX_DRAW_CALLS_PER_FRAME * 2
};

namespace Vulkan
{
	Context::Context(VkInstance instance, VkPhysicalDevice physical_device)
		: m_instance(instance)
		, m_physical_device(physical_device)
	{
		// Read device physical memory properties, we need it for allocating buffers
		vkGetPhysicalDeviceProperties(physical_device, &m_device_properties);
		vkGetPhysicalDeviceMemoryProperties(physical_device, &m_device_memory_properties);

		// Would any drivers be this silly? I hope not...
		m_device_properties.limits.minUniformBufferOffsetAlignment =
			std::max(m_device_properties.limits.minUniformBufferOffsetAlignment, static_cast<VkDeviceSize>(1));
		m_device_properties.limits.minTexelBufferOffsetAlignment =
			std::max(m_device_properties.limits.minTexelBufferOffsetAlignment, static_cast<VkDeviceSize>(1));
		m_device_properties.limits.optimalBufferCopyOffsetAlignment =
			std::max(m_device_properties.limits.optimalBufferCopyOffsetAlignment, static_cast<VkDeviceSize>(1));
		m_device_properties.limits.optimalBufferCopyRowPitchAlignment =
			std::max(m_device_properties.limits.optimalBufferCopyRowPitchAlignment, static_cast<VkDeviceSize>(1));
	}

	Context::~Context() = default;

	VkInstance Context::CreateVulkanInstance(
		const WindowInfo* wi, bool enable_debug_utils, bool enable_validation_layer)
	{
		ExtensionList enabled_extensions;
		if (!SelectInstanceExtensions(&enabled_extensions, wi, enable_debug_utils))
			return VK_NULL_HANDLE;

		// Remember to manually update this every release. We don't pull in svnrev.h here, because
		// it's only the major/minor version, and rebuilding the file every time something else changes
		// is unnecessary.
		VkApplicationInfo app_info = {};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pNext = nullptr;
		app_info.pApplicationName = "PCSX2";
		app_info.applicationVersion = VK_MAKE_VERSION(1, 7, 0);
		app_info.pEngineName = "PCSX2";
		app_info.engineVersion = VK_MAKE_VERSION(1, 7, 0);
		app_info.apiVersion = VK_API_VERSION_1_1;

		VkInstanceCreateInfo instance_create_info = {};
		instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instance_create_info.pNext = nullptr;
		instance_create_info.flags = 0;
		instance_create_info.pApplicationInfo = &app_info;
		instance_create_info.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size());
		instance_create_info.ppEnabledExtensionNames = enabled_extensions.data();
		instance_create_info.enabledLayerCount = 0;
		instance_create_info.ppEnabledLayerNames = nullptr;

		// Enable debug layer on debug builds
		if (enable_validation_layer)
		{
			static const char* layer_names[] = {"VK_LAYER_KHRONOS_validation"};
			instance_create_info.enabledLayerCount = 1;
			instance_create_info.ppEnabledLayerNames = layer_names;
		}

		VkInstance instance;
		VkResult res = vkCreateInstance(&instance_create_info, nullptr, &instance);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateInstance failed: ");
			return nullptr;
		}

		return instance;
	}

	bool Context::SelectInstanceExtensions(ExtensionList* extension_list, const WindowInfo* wi, bool enable_debug_utils)
	{
		u32 extension_count = 0;
		VkResult res = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkEnumerateInstanceExtensionProperties failed: ");
			return false;
		}

		if (extension_count == 0)
		{
			Console.Error("Vulkan: No extensions supported by instance.");
			return false;
		}

		std::vector<VkExtensionProperties> available_extension_list(extension_count);
		res = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, available_extension_list.data());
		pxAssert(res == VK_SUCCESS);

		auto SupportsExtension = [&](const char* name, bool required) {
			if (std::find_if(available_extension_list.begin(), available_extension_list.end(),
					[&](const VkExtensionProperties& properties) { return !strcmp(name, properties.extensionName); }) !=
				available_extension_list.end())
			{
				DevCon.WriteLn("Enabling extension: %s", name);
				extension_list->push_back(name);
				return true;
			}

			if (required)
				Console.Error("Vulkan: Missing required extension %s.", name);

			return false;
		};

		// Common extensions
		if (wi && wi->type != WindowInfo::Type::Surfaceless && !SupportsExtension(VK_KHR_SURFACE_EXTENSION_NAME, true))
			return false;

#if defined(VK_USE_PLATFORM_WIN32_KHR)
		if (wi && wi->type == WindowInfo::Type::Win32 && !SupportsExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME, true))
			return false;
#endif
#if defined(VK_USE_PLATFORM_XLIB_KHR)
		if (wi && wi->type == WindowInfo::Type::X11 && !SupportsExtension(VK_KHR_XLIB_SURFACE_EXTENSION_NAME, true))
			return false;
#endif
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
		if (wi && wi->type == WindowInfo::Type::Wayland &&
			!SupportsExtension(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME, true))
			return false;
#endif
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
		if (wi && wi->type == WindowInfo::Type::Android &&
			!SupportsExtension(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME, true))
			return false;
#endif
#if defined(VK_USE_PLATFORM_METAL_EXT)
		if (wi && wi->type == WindowInfo::Type::MacOS && !SupportsExtension(VK_EXT_METAL_SURFACE_EXTENSION_NAME, true))
			return false;
#endif

#if 0
	if (wi && wi->type == WindowInfo::Type::Display && !SupportsExtension(VK_KHR_DISPLAY_EXTENSION_NAME, true))
		return false;
#endif

		// VK_EXT_debug_utils
		if (enable_debug_utils && !SupportsExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, false))
			Console.Warning("Vulkan: Debug report requested, but extension is not available.");

		return true;
	}

	Context::GPUList Context::EnumerateGPUs(VkInstance instance)
	{
		u32 gpu_count = 0;
		VkResult res = vkEnumeratePhysicalDevices(instance, &gpu_count, nullptr);
		if (res != VK_SUCCESS || gpu_count == 0)
		{
			LOG_VULKAN_ERROR(res, "vkEnumeratePhysicalDevices failed: ");
			return {};
		}

		GPUList gpus;
		gpus.resize(gpu_count);

		res = vkEnumeratePhysicalDevices(instance, &gpu_count, gpus.data());
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkEnumeratePhysicalDevices failed: ");
			return {};
		}

		return gpus;
	}

	Context::GPUNameList Context::EnumerateGPUNames(VkInstance instance)
	{
		u32 gpu_count = 0;
		VkResult res = vkEnumeratePhysicalDevices(instance, &gpu_count, nullptr);
		if (res != VK_SUCCESS || gpu_count == 0)
		{
			LOG_VULKAN_ERROR(res, "vkEnumeratePhysicalDevices failed: ");
			return {};
		}

		GPUList gpus;
		gpus.resize(gpu_count);

		res = vkEnumeratePhysicalDevices(instance, &gpu_count, gpus.data());
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkEnumeratePhysicalDevices failed: ");
			return {};
		}

		GPUNameList gpu_names;
		gpu_names.reserve(gpu_count);
		for (u32 i = 0; i < gpu_count; i++)
		{
			VkPhysicalDeviceProperties props = {};
			vkGetPhysicalDeviceProperties(gpus[i], &props);

			std::string gpu_name(props.deviceName);

			// handle duplicate adapter names
			if (std::any_of(gpu_names.begin(), gpu_names.end(),
					[&gpu_name](const std::string& other) { return (gpu_name == other); }))
			{
				std::string original_adapter_name = std::move(gpu_name);

				u32 current_extra = 2;
				do
				{
					gpu_name = StringUtil::StdStringFromFormat("%s (%u)", original_adapter_name.c_str(), current_extra);
					current_extra++;
				} while (std::any_of(gpu_names.begin(), gpu_names.end(),
					[&gpu_name](const std::string& other) { return (gpu_name == other); }));
			}

			gpu_names.push_back(std::move(gpu_name));
		}

		return gpu_names;
	}

	bool Context::Create(std::string_view gpu_name, const WindowInfo* wi, std::unique_ptr<SwapChain>* out_swap_chain,
		VkPresentModeKHR preferred_present_mode, bool threaded_presentation, bool enable_debug_utils,
		bool enable_validation_layer)
	{
		pxAssertMsg(!g_vulkan_context, "Has no current context");

		if (!Vulkan::LoadVulkanLibrary())
		{
			Console.Error("Failed to load Vulkan library");
			return false;
		}

		const bool enable_surface = (wi && wi->type != WindowInfo::Type::Surfaceless);
		VkInstance instance = CreateVulkanInstance(wi, enable_debug_utils, enable_validation_layer);
		if (instance == VK_NULL_HANDLE)
		{
			if (enable_debug_utils || enable_validation_layer)
			{
				// Try again without the validation layer.
				enable_debug_utils = false;
				enable_validation_layer = false;
				instance = CreateVulkanInstance(wi, enable_debug_utils, enable_validation_layer);
				if (instance == VK_NULL_HANDLE)
				{
					Vulkan::UnloadVulkanLibrary();
					return false;
				}

				Console.Error("Vulkan validation/debug layers requested but are unavailable. Creating non-debug device.");
			}
		}

		if (!Vulkan::LoadVulkanInstanceFunctions(instance))
		{
			Console.Error("Failed to load Vulkan instance functions");
			vkDestroyInstance(instance, nullptr);
			Vulkan::UnloadVulkanLibrary();
			return false;
		}

		GPUList gpus = EnumerateGPUs(instance);
		if (gpus.empty())
		{
			vkDestroyInstance(instance, nullptr);
			Vulkan::UnloadVulkanLibrary();
			return false;
		}

		u32 gpu_index = 0;
		GPUNameList gpu_names = EnumerateGPUNames(instance);
		if (!gpu_name.empty())
		{
			for (; gpu_index < static_cast<u32>(gpu_names.size()); gpu_index++)
			{
				Console.WriteLn("GPU %u: %s", static_cast<u32>(gpu_index), gpu_names[gpu_index].c_str());
				if (gpu_names[gpu_index] == gpu_name)
					break;
			}

			if (gpu_index == static_cast<u32>(gpu_names.size()))
			{
				Console.Warning("Requested GPU '%s' not found, using first (%s)", std::string(gpu_name).c_str(),
					gpu_names[0].c_str());
				gpu_index = 0;
			}
		}
		else
		{
			Console.WriteLn("No GPU requested, using first (%s)", gpu_names[0].c_str());
		}

		VkSurfaceKHR surface = VK_NULL_HANDLE;
		WindowInfo wi_copy;
		if (wi)
			wi_copy = *wi;

		if (enable_surface &&
			(surface = SwapChain::CreateVulkanSurface(instance, gpus[gpu_index], &wi_copy)) == VK_NULL_HANDLE)
		{
			vkDestroyInstance(instance, nullptr);
			Vulkan::UnloadVulkanLibrary();
			return false;
		}

		g_vulkan_context.reset(new Context(instance, gpus[gpu_index]));

		if (enable_debug_utils)
			g_vulkan_context->EnableDebugUtils();

		// Attempt to create the device.
		if (!g_vulkan_context->CreateDevice(surface, enable_validation_layer, nullptr, 0, nullptr, 0, nullptr) ||
			!g_vulkan_context->CreateAllocator() || !g_vulkan_context->CreateGlobalDescriptorPool() ||
			!g_vulkan_context->CreateCommandBuffers() || !g_vulkan_context->CreateTextureStreamBuffer() ||
			(enable_surface && (*out_swap_chain = SwapChain::Create(wi_copy, surface, preferred_present_mode)) == nullptr))
		{
			// Since we are destroying the instance, we're also responsible for destroying the surface.
			if (surface != VK_NULL_HANDLE)
				vkDestroySurfaceKHR(instance, surface, nullptr);

			g_vulkan_context.reset();
			return false;
		}

		if (threaded_presentation)
			g_vulkan_context->StartPresentThread();

		return true;
	}

	void Context::Destroy()
	{
		pxAssertMsg(g_vulkan_context, "Has context");

		g_vulkan_context->StopPresentThread();

		if (g_vulkan_context->m_device != VK_NULL_HANDLE)
			g_vulkan_context->WaitForGPUIdle();

		g_vulkan_context->m_texture_upload_buffer.Destroy(false);

		g_vulkan_context->DestroyRenderPassCache();
		g_vulkan_context->DestroyGlobalDescriptorPool();
		g_vulkan_context->DestroyCommandBuffers();
		g_vulkan_context->DestroyAllocator();

		if (g_vulkan_context->m_device != VK_NULL_HANDLE)
			vkDestroyDevice(g_vulkan_context->m_device, nullptr);

		if (g_vulkan_context->m_debug_messenger_callback != VK_NULL_HANDLE)
			g_vulkan_context->DisableDebugUtils();

		if (g_vulkan_context->m_instance != VK_NULL_HANDLE)
			vkDestroyInstance(g_vulkan_context->m_instance, nullptr);

		Vulkan::UnloadVulkanLibrary();

		g_vulkan_context.reset();
	}

	bool Context::SelectDeviceExtensions(ExtensionList* extension_list, bool enable_surface)
	{
		u32 extension_count = 0;
		VkResult res = vkEnumerateDeviceExtensionProperties(m_physical_device, nullptr, &extension_count, nullptr);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkEnumerateDeviceExtensionProperties failed: ");
			return false;
		}

		if (extension_count == 0)
		{
			Console.Error("Vulkan: No extensions supported by device.");
			return false;
		}

		std::vector<VkExtensionProperties> available_extension_list(extension_count);
		res = vkEnumerateDeviceExtensionProperties(
			m_physical_device, nullptr, &extension_count, available_extension_list.data());
		pxAssert(res == VK_SUCCESS);

		auto SupportsExtension = [&](const char* name, bool required) {
			if (std::find_if(available_extension_list.begin(), available_extension_list.end(),
					[&](const VkExtensionProperties& properties) { return !strcmp(name, properties.extensionName); }) !=
				available_extension_list.end())
			{
				if (std::none_of(extension_list->begin(), extension_list->end(),
						[&](const char* existing_name) { return (std::strcmp(existing_name, name) == 0); }))
				{
					DevCon.WriteLn("Enabling extension: %s", name);
					extension_list->push_back(name);
				}

				return true;
			}

			if (required)
				Console.Error("Vulkan: Missing required extension %s.", name);

			return false;
		};

		if (enable_surface && !SupportsExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME, true))
			return false;

		m_optional_extensions.vk_ext_provoking_vertex =
			SupportsExtension(VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME, false);
		m_optional_extensions.vk_ext_memory_budget =
			SupportsExtension(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME, false);
		m_optional_extensions.vk_khr_driver_properties =
			SupportsExtension(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME, false);
		m_optional_extensions.vk_arm_rasterization_order_attachment_access =
			SupportsExtension(VK_ARM_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_EXTENSION_NAME, false);

		return true;
	}

	bool Context::SelectDeviceFeatures(const VkPhysicalDeviceFeatures* required_features)
	{
		VkPhysicalDeviceFeatures available_features;
		vkGetPhysicalDeviceFeatures(m_physical_device, &available_features);

		if (required_features)
			std::memcpy(&m_device_features, required_features, sizeof(m_device_features));

		// Enable the features we use.
		m_device_features.dualSrcBlend = available_features.dualSrcBlend;
		m_device_features.geometryShader = available_features.geometryShader;
		m_device_features.largePoints = available_features.largePoints;
		m_device_features.wideLines = available_features.wideLines;
		m_device_features.fragmentStoresAndAtomics = available_features.fragmentStoresAndAtomics;
		m_device_features.textureCompressionBC = available_features.textureCompressionBC;

		return true;
	}

	bool Context::CreateDevice(VkSurfaceKHR surface, bool enable_validation_layer,
		const char** required_device_extensions, u32 num_required_device_extensions,
		const char** required_device_layers, u32 num_required_device_layers,
		const VkPhysicalDeviceFeatures* required_features)
	{
		u32 queue_family_count;
		vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, nullptr);
		if (queue_family_count == 0)
		{
			Console.Error("No queue families found on specified vulkan physical device.");
			return false;
		}

		std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(
			m_physical_device, &queue_family_count, queue_family_properties.data());
		Console.WriteLn("%u vulkan queue families", queue_family_count);

		// Find graphics and present queues.
		m_graphics_queue_family_index = queue_family_count;
		m_present_queue_family_index = queue_family_count;
		for (uint32_t i = 0; i < queue_family_count; i++)
		{
			VkBool32 graphics_supported = queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
			if (graphics_supported)
			{
				m_graphics_queue_family_index = i;
				// Quit now, no need for a present queue.
				if (!surface)
				{
					break;
				}
			}

			if (surface)
			{
				VkBool32 present_supported;
				VkResult res = vkGetPhysicalDeviceSurfaceSupportKHR(m_physical_device, i, surface, &present_supported);
				if (res != VK_SUCCESS)
				{
					LOG_VULKAN_ERROR(res, "vkGetPhysicalDeviceSurfaceSupportKHR failed: ");
					return false;
				}

				if (present_supported)
				{
					m_present_queue_family_index = i;
				}

				// Prefer one queue family index that does both graphics and present.
				if (graphics_supported && present_supported)
				{
					break;
				}
			}
		}
		if (m_graphics_queue_family_index == queue_family_count)
		{
			Console.Error("Vulkan: Failed to find an acceptable graphics queue.");
			return false;
		}
		if (surface && m_present_queue_family_index == queue_family_count)
		{
			Console.Error("Vulkan: Failed to find an acceptable present queue.");
			return false;
		}

		VkDeviceCreateInfo device_info = {};
		device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_info.pNext = nullptr;
		device_info.flags = 0;

		static constexpr float queue_priorities[] = {1.0f};
		VkDeviceQueueCreateInfo graphics_queue_info = {};
		graphics_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		graphics_queue_info.pNext = nullptr;
		graphics_queue_info.flags = 0;
		graphics_queue_info.queueFamilyIndex = m_graphics_queue_family_index;
		graphics_queue_info.queueCount = 1;
		graphics_queue_info.pQueuePriorities = queue_priorities;

		VkDeviceQueueCreateInfo present_queue_info = {};
		present_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		present_queue_info.pNext = nullptr;
		present_queue_info.flags = 0;
		present_queue_info.queueFamilyIndex = m_present_queue_family_index;
		present_queue_info.queueCount = 1;
		present_queue_info.pQueuePriorities = queue_priorities;

		std::array<VkDeviceQueueCreateInfo, 2> queue_infos = {{
			graphics_queue_info,
			present_queue_info,
		}};

		device_info.queueCreateInfoCount = 1;
		if (m_graphics_queue_family_index != m_present_queue_family_index)
		{
			device_info.queueCreateInfoCount = 2;
		}
		device_info.pQueueCreateInfos = queue_infos.data();

		ExtensionList enabled_extensions;
		for (u32 i = 0; i < num_required_device_extensions; i++)
			enabled_extensions.emplace_back(required_device_extensions[i]);
		if (!SelectDeviceExtensions(&enabled_extensions, surface != VK_NULL_HANDLE))
			return false;

		device_info.enabledLayerCount = num_required_device_layers;
		device_info.ppEnabledLayerNames = required_device_layers;
		device_info.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size());
		device_info.ppEnabledExtensionNames = enabled_extensions.data();

		// Check for required features before creating.
		if (!SelectDeviceFeatures(required_features))
			return false;

		device_info.pEnabledFeatures = &m_device_features;

		// Enable debug layer on debug builds
		if (enable_validation_layer)
		{
			static const char* layer_names[] = {"VK_LAYER_LUNARG_standard_validation"};
			device_info.enabledLayerCount = 1;
			device_info.ppEnabledLayerNames = layer_names;
		}

		// provoking vertex
		VkPhysicalDeviceProvokingVertexFeaturesEXT provoking_vertex_feature = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_FEATURES_EXT};
		VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesARM rasterization_order_access_feature = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_FEATURES_ARM};

		if (m_optional_extensions.vk_ext_provoking_vertex)
		{
			provoking_vertex_feature.provokingVertexLast = VK_TRUE;
			Util::AddPointerToChain(&device_info, &provoking_vertex_feature);
		}
		if (m_optional_extensions.vk_arm_rasterization_order_attachment_access)
		{
			rasterization_order_access_feature.rasterizationOrderColorAttachmentAccess = VK_TRUE;
			Util::AddPointerToChain(&device_info, &rasterization_order_access_feature);
		}

		VkResult res = vkCreateDevice(m_physical_device, &device_info, nullptr, &m_device);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateDevice failed: ");
			return false;
		}

		// With the device created, we can fill the remaining entry points.
		if (!LoadVulkanDeviceFunctions(m_device))
			return false;

		// Grab the graphics and present queues.
		vkGetDeviceQueue(m_device, m_graphics_queue_family_index, 0, &m_graphics_queue);
		if (surface)
		{
			vkGetDeviceQueue(m_device, m_present_queue_family_index, 0, &m_present_queue);
		}

		m_gpu_timing_supported = (queue_family_properties[m_graphics_queue_family_index].timestampValidBits > 0);
		DevCon.WriteLn("GPU timing is %s", m_gpu_timing_supported ? "supported" : "not supported");

		ProcessDeviceExtensions();
		return true;
	}

	void Context::ProcessDeviceExtensions()
	{
		// advanced feature checks
		VkPhysicalDeviceFeatures2 features2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
		VkPhysicalDeviceProvokingVertexFeaturesEXT provoking_vertex_features = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_FEATURES_EXT};
		VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesARM rasterization_order_access_feature = {
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_FEATURES_ARM};

		// add in optional feature structs
		if (m_optional_extensions.vk_ext_provoking_vertex)
			Util::AddPointerToChain(&features2, &provoking_vertex_features);
		if (m_optional_extensions.vk_arm_rasterization_order_attachment_access)
			Util::AddPointerToChain(&features2, &rasterization_order_access_feature);

		// query
		vkGetPhysicalDeviceFeatures2(m_physical_device, &features2);

		// confirm we actually support it
		m_optional_extensions.vk_ext_provoking_vertex &= (provoking_vertex_features.provokingVertexLast == VK_TRUE);
		m_optional_extensions.vk_arm_rasterization_order_attachment_access &= (rasterization_order_access_feature.rasterizationOrderColorAttachmentAccess == VK_TRUE);

		VkPhysicalDeviceProperties2 properties2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
		void** pNext = &properties2.pNext;

		if (m_optional_extensions.vk_khr_driver_properties)
		{
			m_device_driver_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
			*pNext = &m_device_driver_properties;
			pNext = &m_device_driver_properties.pNext;
		}

		// query
		vkGetPhysicalDeviceProperties2(m_physical_device, &properties2);

		Console.WriteLn("VK_EXT_provoking_vertex is %s",
			m_optional_extensions.vk_ext_provoking_vertex ? "supported" : "NOT supported");
		Console.WriteLn("VK_ARM_rasterization_order_attachment_access is %s",
			m_optional_extensions.vk_arm_rasterization_order_attachment_access ? "supported" : "NOT supported");
	}

	bool Context::CreateAllocator()
	{
		VmaAllocatorCreateInfo ci = {};
		ci.vulkanApiVersion = VK_API_VERSION_1_1;
		ci.flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT;
		ci.physicalDevice = m_physical_device;
		ci.device = m_device;
		ci.instance = m_instance;

		if (m_optional_extensions.vk_ext_memory_budget)
			ci.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;

		VkResult res = vmaCreateAllocator(&ci, &m_allocator);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vmaCreateAllocator failed: ");
			return false;
		}

		return true;
	}

	void Context::DestroyAllocator()
	{
		if (m_allocator == VK_NULL_HANDLE)
			return;

		vmaDestroyAllocator(m_allocator);
		m_allocator = VK_NULL_HANDLE;
	}

	bool Context::CreateCommandBuffers()
	{
		VkResult res;

		uint32_t frame_index = 0;
		for (FrameResources& resources : m_frame_resources)
		{
			resources.needs_fence_wait = false;

			VkCommandPoolCreateInfo pool_info = {
				VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, 0, m_graphics_queue_family_index};
			res = vkCreateCommandPool(m_device, &pool_info, nullptr, &resources.command_pool);
			if (res != VK_SUCCESS)
			{
				LOG_VULKAN_ERROR(res, "vkCreateCommandPool failed: ");
				return false;
			}
			Vulkan::Util::SetObjectName(
				g_vulkan_context->GetDevice(), resources.command_pool, "Frame Command Pool %u", frame_index);

			VkCommandBufferAllocateInfo buffer_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr,
				resources.command_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				static_cast<u32>(resources.command_buffers.size())};

			res = vkAllocateCommandBuffers(m_device, &buffer_info, resources.command_buffers.data());
			if (res != VK_SUCCESS)
			{
				LOG_VULKAN_ERROR(res, "vkAllocateCommandBuffers failed: ");
				return false;
			}
			for (u32 i = 0; i < resources.command_buffers.size(); i++)
			{
				Vulkan::Util::SetObjectName(g_vulkan_context->GetDevice(), resources.command_buffers[i],
					"Frame %u %sCommand Buffer", frame_index, (i == 0) ? "Init" : "");
			}

			VkFenceCreateInfo fence_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};

			res = vkCreateFence(m_device, &fence_info, nullptr, &resources.fence);
			if (res != VK_SUCCESS)
			{
				LOG_VULKAN_ERROR(res, "vkCreateFence failed: ");
				return false;
			}
			Vulkan::Util::SetObjectName(g_vulkan_context->GetDevice(), resources.fence, "Frame Fence %u", frame_index);
			// TODO: A better way to choose the number of descriptors.
			VkDescriptorPoolSize pool_sizes[] = {
				{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_COMBINED_IMAGE_SAMPLER_DESCRIPTORS_PER_FRAME},
				{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_SAMPLED_IMAGE_DESCRIPTORS_PER_FRAME},
				{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, MAX_INPUT_ATTACHMENT_IMAGE_DESCRIPTORS_PER_FRAME},
			};

			VkDescriptorPoolCreateInfo pool_create_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr, 0,
				MAX_DESCRIPTOR_SETS_PER_FRAME, static_cast<u32>(std::size(pool_sizes)), pool_sizes};

			res = vkCreateDescriptorPool(m_device, &pool_create_info, nullptr, &resources.descriptor_pool);
			if (res != VK_SUCCESS)
			{
				LOG_VULKAN_ERROR(res, "vkCreateDescriptorPool failed: ");
				return false;
			}
			Vulkan::Util::SetObjectName(
				g_vulkan_context->GetDevice(), resources.descriptor_pool, "Frame Descriptor Pool %u", frame_index);

			++frame_index;
		}

		ActivateCommandBuffer(0);
		return true;
	}

	void Context::DestroyCommandBuffers()
	{
		for (FrameResources& resources : m_frame_resources)
		{
			for (auto& it : resources.cleanup_resources)
				it();
			resources.cleanup_resources.clear();

			if (resources.fence != VK_NULL_HANDLE)
			{
				vkDestroyFence(m_device, resources.fence, nullptr);
				resources.fence = VK_NULL_HANDLE;
			}
			if (resources.descriptor_pool != VK_NULL_HANDLE)
			{
				vkDestroyDescriptorPool(m_device, resources.descriptor_pool, nullptr);
				resources.descriptor_pool = VK_NULL_HANDLE;
			}
			if (resources.command_buffers[0] != VK_NULL_HANDLE)
			{
				vkFreeCommandBuffers(m_device, resources.command_pool,
					static_cast<u32>(resources.command_buffers.size()), resources.command_buffers.data());
				resources.command_buffers.fill(VK_NULL_HANDLE);
			}
			if (resources.command_pool != VK_NULL_HANDLE)
			{
				vkDestroyCommandPool(m_device, resources.command_pool, nullptr);
				resources.command_pool = VK_NULL_HANDLE;
			}
		}
	}

	bool Context::CreateGlobalDescriptorPool()
	{
		// TODO: A better way to choose the number of descriptors.
		VkDescriptorPoolSize pool_sizes[] = {
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1024},
			{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024},
		};

		VkDescriptorPoolCreateInfo pool_create_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr,
			VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
			1024, // TODO: tweak this
			static_cast<u32>(std::size(pool_sizes)), pool_sizes};

		VkResult res = vkCreateDescriptorPool(m_device, &pool_create_info, nullptr, &m_global_descriptor_pool);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateDescriptorPool failed: ");
			return false;
		}
		Vulkan::Util::SetObjectName(g_vulkan_context->GetDevice(), m_global_descriptor_pool, "Global Descriptor Pool");

		if (m_gpu_timing_supported)
		{
			const VkQueryPoolCreateInfo query_create_info = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, nullptr,
				0, VK_QUERY_TYPE_TIMESTAMP, NUM_COMMAND_BUFFERS * 2, 0};
			res = vkCreateQueryPool(m_device, &query_create_info, nullptr, &m_timestamp_query_pool);
			if (res != VK_SUCCESS)
			{
				LOG_VULKAN_ERROR(res, "vkCreateQueryPool failed: ");
				return false;
			}
		}

		return true;
	}

	void Context::DestroyGlobalDescriptorPool()
	{
		if (m_timestamp_query_pool != VK_NULL_HANDLE)
		{
			vkDestroyQueryPool(m_device, m_timestamp_query_pool, nullptr);
			m_timestamp_query_pool = VK_NULL_HANDLE;
		}

		if (m_global_descriptor_pool != VK_NULL_HANDLE)
		{
			vkDestroyDescriptorPool(m_device, m_global_descriptor_pool, nullptr);
			m_global_descriptor_pool = VK_NULL_HANDLE;
		}
	}

	bool Context::CreateTextureStreamBuffer()
	{
		if (!m_texture_upload_buffer.Create(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, TEXTURE_BUFFER_SIZE))
		{
			Console.Error("Failed to allocate texture upload buffer");
			return false;
		}

		return true;
	}

	VkCommandBuffer Context::GetCurrentInitCommandBuffer()
	{
		FrameResources& res = m_frame_resources[m_current_frame];
		VkCommandBuffer buf = res.command_buffers[0];
		if (res.init_buffer_used)
			return buf;

		VkCommandBufferBeginInfo bi{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr};
		vkBeginCommandBuffer(buf, &bi);
		res.init_buffer_used = true;
		return buf;
	}

	VkDescriptorSet Context::AllocateDescriptorSet(VkDescriptorSetLayout set_layout)
	{
		VkDescriptorSetAllocateInfo allocate_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr,
			m_frame_resources[m_current_frame].descriptor_pool, 1, &set_layout};

		VkDescriptorSet descriptor_set;
		VkResult res = vkAllocateDescriptorSets(m_device, &allocate_info, &descriptor_set);
		if (res != VK_SUCCESS)
		{
			// Failing to allocate a descriptor set is not a fatal error, we can
			// recover by moving to the next command buffer.
			return VK_NULL_HANDLE;
		}

		return descriptor_set;
	}

	VkDescriptorSet Context::AllocatePersistentDescriptorSet(VkDescriptorSetLayout set_layout)
	{
		VkDescriptorSetAllocateInfo allocate_info = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, m_global_descriptor_pool, 1, &set_layout};

		VkDescriptorSet descriptor_set;
		VkResult res = vkAllocateDescriptorSets(m_device, &allocate_info, &descriptor_set);
		if (res != VK_SUCCESS)
			return VK_NULL_HANDLE;

		return descriptor_set;
	}

	void Context::FreeGlobalDescriptorSet(VkDescriptorSet set)
	{
		vkFreeDescriptorSets(m_device, m_global_descriptor_pool, 1, &set);
	}

	void Context::WaitForFenceCounter(u64 fence_counter)
	{
		if (m_completed_fence_counter >= fence_counter)
			return;

		// Find the first command buffer which covers this counter value.
		u32 index = (m_current_frame + 1) % NUM_COMMAND_BUFFERS;
		while (index != m_current_frame)
		{
			if (m_frame_resources[index].fence_counter >= fence_counter)
				break;

			index = (index + 1) % NUM_COMMAND_BUFFERS;
		}

		pxAssert(index != m_current_frame);
		WaitForCommandBufferCompletion(index);
	}

	void Context::WaitForGPUIdle()
	{
		WaitForPresentComplete();
		vkDeviceWaitIdle(m_device);
	}

	float Context::GetAndResetAccumulatedGPUTime()
	{
		const float time = m_accumulated_gpu_time;
		m_accumulated_gpu_time = 0.0f;
		return time;
	}

	void Context::SetEnableGPUTiming(bool enabled)
	{
		m_gpu_timing_enabled = enabled && m_gpu_timing_supported;
	}

	void Context::WaitForCommandBufferCompletion(u32 index)
	{
		// Wait for this command buffer to be completed.
		VkResult res = vkWaitForFences(m_device, 1, &m_frame_resources[index].fence, VK_TRUE, UINT64_MAX);
		if (res != VK_SUCCESS)
			LOG_VULKAN_ERROR(res, "vkWaitForFences failed: ");

		// Clean up any resources for command buffers between the last known completed buffer and this
		// now-completed command buffer. If we use >2 buffers, this may be more than one buffer.
		const u64 now_completed_counter = m_frame_resources[index].fence_counter;
		u32 cleanup_index = (m_current_frame + 1) % NUM_COMMAND_BUFFERS;
		while (cleanup_index != m_current_frame)
		{
			FrameResources& resources = m_frame_resources[cleanup_index];
			if (resources.fence_counter > now_completed_counter)
				break;

			if (resources.fence_counter > m_completed_fence_counter)
			{
				for (auto& it : resources.cleanup_resources)
					it();
				resources.cleanup_resources.clear();
			}

			cleanup_index = (cleanup_index + 1) % NUM_COMMAND_BUFFERS;
		}

		m_completed_fence_counter = now_completed_counter;
	}

	void Context::SubmitCommandBuffer(VkSemaphore wait_semaphore /* = VK_NULL_HANDLE */,
		VkSemaphore signal_semaphore /* = VK_NULL_HANDLE */, VkSwapchainKHR present_swap_chain /* = VK_NULL_HANDLE */,
		uint32_t present_image_index /* = 0xFFFFFFFF */, bool submit_on_thread /* = false */)
	{
		FrameResources& resources = m_frame_resources[m_current_frame];

		// End the current command buffer.
		VkResult res;
		if (resources.init_buffer_used)
		{
			res = vkEndCommandBuffer(resources.command_buffers[0]);
			if (res != VK_SUCCESS)
			{
				LOG_VULKAN_ERROR(res, "vkEndCommandBuffer failed: ");
				pxFailRel("Failed to end command buffer");
			}
		}

		if (m_gpu_timing_enabled && resources.timestamp_written)
		{
			vkCmdWriteTimestamp(m_current_command_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_timestamp_query_pool, m_current_frame * 2 + 1);
		}

		res = vkEndCommandBuffer(resources.command_buffers[1]);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkEndCommandBuffer failed: ");
			pxFailRel("Failed to end command buffer");
		}

		// This command buffer now has commands, so can't be re-used without waiting.
		resources.needs_fence_wait = true;

		std::unique_lock<std::mutex> lock(m_present_mutex);
		WaitForPresentComplete(lock);

		if (!submit_on_thread || !m_present_thread.joinable())
		{
			DoSubmitCommandBuffer(m_current_frame, wait_semaphore, signal_semaphore);
			if (present_swap_chain != VK_NULL_HANDLE)
				DoPresent(signal_semaphore, present_swap_chain, present_image_index);
			return;
		}

		m_queued_present.command_buffer_index = m_current_frame;
		m_queued_present.present_swap_chain = present_swap_chain;
		m_queued_present.present_image_index = present_image_index;
		m_queued_present.wait_semaphore = wait_semaphore;
		m_queued_present.signal_semaphore = signal_semaphore;
		m_present_done.store(false);
		m_present_queued_cv.notify_one();
	}

	void Context::DoSubmitCommandBuffer(u32 index, VkSemaphore wait_semaphore, VkSemaphore signal_semaphore)
	{
		FrameResources& resources = m_frame_resources[index];

		uint32_t wait_bits = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, &wait_bits,
			resources.init_buffer_used ? 2u : 1u,
			resources.init_buffer_used ? resources.command_buffers.data() : &resources.command_buffers[1], 0, nullptr};

		if (wait_semaphore != VK_NULL_HANDLE)
		{
			submit_info.pWaitSemaphores = &wait_semaphore;
			submit_info.waitSemaphoreCount = 1;
		}

		if (signal_semaphore != VK_NULL_HANDLE)
		{
			submit_info.signalSemaphoreCount = 1;
			submit_info.pSignalSemaphores = &signal_semaphore;
		}

		VkResult res = vkQueueSubmit(m_graphics_queue, 1, &submit_info, resources.fence);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkQueueSubmit failed: ");
			pxFailRel("Failed to submit command buffer.");
		}
	}

	void Context::DoPresent(VkSemaphore wait_semaphore, VkSwapchainKHR present_swap_chain, uint32_t present_image_index)
	{
		// Should have a signal semaphore.
		pxAssert(wait_semaphore != VK_NULL_HANDLE);
		VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, nullptr, 1, &wait_semaphore, 1,
			&present_swap_chain, &present_image_index, nullptr};

		VkResult res = vkQueuePresentKHR(m_present_queue, &present_info);
		if (res != VK_SUCCESS)
		{
			// VK_ERROR_OUT_OF_DATE_KHR is not fatal, just means we need to recreate our swap chain.
			if (res != VK_ERROR_OUT_OF_DATE_KHR && res != VK_SUBOPTIMAL_KHR)
				LOG_VULKAN_ERROR(res, "vkQueuePresentKHR failed: ");

			m_last_present_failed.store(true);
		}
	}

	void Context::WaitForPresentComplete()
	{
		if (m_present_done.load())
			return;

		std::unique_lock<std::mutex> lock(m_present_mutex);
		WaitForPresentComplete(lock);
	}

	void Context::WaitForPresentComplete(std::unique_lock<std::mutex>& lock)
	{
		if (m_present_done.load())
			return;

		m_present_done_cv.wait(lock, [this]() { return m_present_done.load(); });
	}

	void Context::PresentThread()
	{
		std::unique_lock<std::mutex> lock(m_present_mutex);
		while (!m_present_thread_done.load())
		{
			m_present_queued_cv.wait(lock, [this]() { return !m_present_done.load() || m_present_thread_done.load(); });

			if (m_present_done.load())
				continue;

			DoSubmitCommandBuffer(m_queued_present.command_buffer_index, m_queued_present.wait_semaphore,
				m_queued_present.signal_semaphore);
			DoPresent(m_queued_present.signal_semaphore, m_queued_present.present_swap_chain,
				m_queued_present.present_image_index);
			m_present_done.store(true);
			m_present_done_cv.notify_one();
		}
	}

	void Context::StartPresentThread()
	{
		pxAssert(!m_present_thread.joinable());
		m_present_thread_done.store(false);
		m_present_thread = std::thread(&Context::PresentThread, this);
	}

	void Context::StopPresentThread()
	{
		if (!m_present_thread.joinable())
			return;

		{
			std::unique_lock<std::mutex> lock(m_present_mutex);
			WaitForPresentComplete(lock);
			m_present_thread_done.store(true);
			m_present_queued_cv.notify_one();
		}

		m_present_thread.join();
	}

	void Context::MoveToNextCommandBuffer() { ActivateCommandBuffer((m_current_frame + 1) % NUM_COMMAND_BUFFERS); }

	void Context::ActivateCommandBuffer(u32 index)
	{
		FrameResources& resources = m_frame_resources[index];

		if (!m_present_done.load() && m_queued_present.command_buffer_index == index)
			WaitForPresentComplete();

		// Wait for the GPU to finish with all resources for this command buffer.
		if (resources.fence_counter > m_completed_fence_counter)
			WaitForCommandBufferCompletion(index);

		// Reset fence to unsignaled before starting.
		VkResult res = vkResetFences(m_device, 1, &resources.fence);
		if (res != VK_SUCCESS)
			LOG_VULKAN_ERROR(res, "vkResetFences failed: ");

		// Reset command pools to beginning since we can re-use the memory now
		res = vkResetCommandPool(m_device, resources.command_pool, 0);
		if (res != VK_SUCCESS)
			LOG_VULKAN_ERROR(res, "vkResetCommandPool failed: ");

		// Enable commands to be recorded to the two buffers again.
		VkCommandBufferBeginInfo begin_info = {
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr};
		res = vkBeginCommandBuffer(resources.command_buffers[1], &begin_info);
		if (res != VK_SUCCESS)
			LOG_VULKAN_ERROR(res, "vkBeginCommandBuffer failed: ");

		// Also can do the same for the descriptor pools
		res = vkResetDescriptorPool(m_device, resources.descriptor_pool, 0);
		if (res != VK_SUCCESS)
			LOG_VULKAN_ERROR(res, "vkResetDescriptorPool failed: ");

		if (m_gpu_timing_enabled)
		{
			if (resources.timestamp_written)
			{
				std::array<u64, 2> timestamps;
				res = vkGetQueryPoolResults(m_device, m_timestamp_query_pool, index * 2, static_cast<u32>(timestamps.size()),
					sizeof(u64) * timestamps.size(), timestamps.data(), sizeof(u64), VK_QUERY_RESULT_64_BIT);
				if (res == VK_SUCCESS)
				{
					// if we didn't write the timestamp at the start of the cmdbuffer (just enabled timing), the first TS will be zero
					if (timestamps[0] > 0)
					{
						const u64 ns_diff = (timestamps[1] - timestamps[0]) * static_cast<u64>(m_device_properties.limits.timestampPeriod);
						m_accumulated_gpu_time += static_cast<double>(ns_diff) / 1000000.0;
					}
				}
				else
				{
					LOG_VULKAN_ERROR(res, "vkGetQueryPoolResults failed: ");
				}
			}

			vkCmdResetQueryPool(resources.command_buffers[1], m_timestamp_query_pool, index * 2, 2);
			vkCmdWriteTimestamp(resources.command_buffers[1], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_timestamp_query_pool, index * 2);
		}

		resources.fence_counter = m_next_fence_counter++;
		resources.init_buffer_used = false;
		resources.timestamp_written = m_gpu_timing_enabled;

		m_current_frame = index;
		m_current_command_buffer = resources.command_buffers[1];

		// using the lower 32 bits of the fence index should be sufficient here, I hope...
		vmaSetCurrentFrameIndex(m_allocator, static_cast<u32>(m_next_fence_counter));
	}

	void Context::ExecuteCommandBuffer(bool wait_for_completion)
	{
		// If we're waiting for completion, don't bother waking the worker thread.
		const u32 current_frame = m_current_frame;
		SubmitCommandBuffer();
		MoveToNextCommandBuffer();

		if (wait_for_completion)
			WaitForCommandBufferCompletion(current_frame);
	}

	bool Context::CheckLastPresentFail()
	{
		bool res = m_last_present_failed;
		m_last_present_failed = false;
		return res;
	}

	void Context::DeferBufferDestruction(VkBuffer object)
	{
		FrameResources& resources = m_frame_resources[m_current_frame];
		resources.cleanup_resources.push_back([this, object]() { vkDestroyBuffer(m_device, object, nullptr); });
	}

	void Context::DeferBufferDestruction(VkBuffer object, VmaAllocation allocation)
	{
		FrameResources& resources = m_frame_resources[m_current_frame];
		resources.cleanup_resources.push_back(
			[this, object, allocation]() { vmaDestroyBuffer(m_allocator, object, allocation); });
	}

	void Context::DeferBufferViewDestruction(VkBufferView object)
	{
		FrameResources& resources = m_frame_resources[m_current_frame];
		resources.cleanup_resources.push_back([this, object]() { vkDestroyBufferView(m_device, object, nullptr); });
	}

	void Context::DeferDeviceMemoryDestruction(VkDeviceMemory object)
	{
		FrameResources& resources = m_frame_resources[m_current_frame];
		resources.cleanup_resources.push_back([this, object]() { vkFreeMemory(m_device, object, nullptr); });
	}

	void Context::DeferFramebufferDestruction(VkFramebuffer object)
	{
		FrameResources& resources = m_frame_resources[m_current_frame];
		resources.cleanup_resources.push_back([this, object]() { vkDestroyFramebuffer(m_device, object, nullptr); });
	}

	void Context::DeferImageDestruction(VkImage object)
	{
		FrameResources& resources = m_frame_resources[m_current_frame];
		resources.cleanup_resources.push_back([this, object]() { vkDestroyImage(m_device, object, nullptr); });
	}

	void Context::DeferImageDestruction(VkImage object, VmaAllocation allocation)
	{
		FrameResources& resources = m_frame_resources[m_current_frame];
		resources.cleanup_resources.push_back(
			[this, object, allocation]() { vmaDestroyImage(m_allocator, object, allocation); });
	}

	void Context::DeferImageViewDestruction(VkImageView object)
	{
		FrameResources& resources = m_frame_resources[m_current_frame];
		resources.cleanup_resources.push_back([this, object]() { vkDestroyImageView(m_device, object, nullptr); });
	}

	void Context::DeferPipelineDestruction(VkPipeline pipeline)
	{
		FrameResources& resources = m_frame_resources[m_current_frame];
		resources.cleanup_resources.push_back([this, pipeline]() { vkDestroyPipeline(m_device, pipeline, nullptr); });
	}

	void Context::DeferSamplerDestruction(VkSampler sampler)
	{
		FrameResources& resources = m_frame_resources[m_current_frame];
		resources.cleanup_resources.push_back([this, sampler]() { vkDestroySampler(m_device, sampler, nullptr); });
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
		VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{
		if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		{
			Console.Error("Vulkan debug report: (%s) %s",
				pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "", pCallbackData->pMessage);
		}
		else if (severity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT))
		{
			Console.Warning("Vulkan debug report: (%s) %s",
				pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "", pCallbackData->pMessage);
		}
		else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		{
			Console.WriteLn("Vulkan debug report: (%s) %s",
				pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "", pCallbackData->pMessage);
		}
		else
		{
			DevCon.WriteLn("Vulkan debug report: (%s) %s",
				pCallbackData->pMessageIdName ? pCallbackData->pMessageIdName : "", pCallbackData->pMessage);
		}

		return VK_FALSE;
	}

	bool Context::EnableDebugUtils()
	{
		// Already enabled?
		if (m_debug_messenger_callback != VK_NULL_HANDLE)
			return true;

		// Check for presence of the functions before calling
		if (!vkCreateDebugUtilsMessengerEXT || !vkDestroyDebugUtilsMessengerEXT || !vkSubmitDebugUtilsMessageEXT)
		{
			return false;
		}

		VkDebugUtilsMessengerCreateInfoEXT messenger_info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			nullptr, 0,
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
			DebugMessengerCallback, nullptr};

		VkResult res =
			vkCreateDebugUtilsMessengerEXT(m_instance, &messenger_info, nullptr, &m_debug_messenger_callback);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateDebugUtilsMessengerEXT failed: ");
			return false;
		}

		return true;
	}

	void Context::DisableDebugUtils()
	{
		if (m_debug_messenger_callback != VK_NULL_HANDLE)
		{
			vkDestroyDebugUtilsMessengerEXT(m_instance, m_debug_messenger_callback, nullptr);
			m_debug_messenger_callback = VK_NULL_HANDLE;
		}
	}

	VkRenderPass Context::CreateCachedRenderPass(RenderPassCacheKey key)
	{
		VkAttachmentReference color_reference;
		VkAttachmentReference* color_reference_ptr = nullptr;
		VkAttachmentReference depth_reference;
		VkAttachmentReference* depth_reference_ptr = nullptr;
		VkAttachmentReference input_reference;
		VkAttachmentReference* input_reference_ptr = nullptr;
		VkSubpassDependency subpass_dependency;
		VkSubpassDependency* subpass_dependency_ptr = nullptr;
		std::array<VkAttachmentDescription, 2> attachments;
		u32 num_attachments = 0;
		if (key.color_format != VK_FORMAT_UNDEFINED)
		{
			attachments[num_attachments] = {0, static_cast<VkFormat>(key.color_format), VK_SAMPLE_COUNT_1_BIT,
				static_cast<VkAttachmentLoadOp>(key.color_load_op),
				static_cast<VkAttachmentStoreOp>(key.color_store_op), VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				VK_ATTACHMENT_STORE_OP_DONT_CARE,
				key.color_feedback_loop ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				key.color_feedback_loop ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
			color_reference.attachment = num_attachments;
			color_reference.layout =
				key.color_feedback_loop ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			color_reference_ptr = &color_reference;

			if (key.color_feedback_loop)
			{
				input_reference.attachment = num_attachments;
				input_reference.layout = VK_IMAGE_LAYOUT_GENERAL;
				input_reference_ptr = &input_reference;

				if (!g_vulkan_context->GetOptionalExtensions().vk_arm_rasterization_order_attachment_access)
				{
					// don't need the framebuffer-local dependency when we have rasterization order attachment access
					subpass_dependency.srcSubpass = 0;
					subpass_dependency.dstSubpass = 0;
					subpass_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
					subpass_dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
					subpass_dependency.srcAccessMask =
						VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
					subpass_dependency.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
					subpass_dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
					subpass_dependency_ptr = &subpass_dependency;
				}
			}

			num_attachments++;
		}
		if (key.depth_format != VK_FORMAT_UNDEFINED)
		{
			attachments[num_attachments] = {0, static_cast<VkFormat>(key.depth_format), VK_SAMPLE_COUNT_1_BIT,
				static_cast<VkAttachmentLoadOp>(key.depth_load_op),
				static_cast<VkAttachmentStoreOp>(key.depth_store_op),
				static_cast<VkAttachmentLoadOp>(key.stencil_load_op),
				static_cast<VkAttachmentStoreOp>(key.stencil_store_op),
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
			depth_reference.attachment = num_attachments;
			depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			depth_reference_ptr = &depth_reference;
			num_attachments++;
		}

		const VkSubpassDescriptionFlags subpass_flags =
			(key.color_feedback_loop && g_vulkan_context->GetOptionalExtensions().vk_arm_rasterization_order_attachment_access)
			? VK_SUBPASS_DESCRIPTION_RASTERIZATION_ORDER_ATTACHMENT_COLOR_ACCESS_BIT_ARM : 0;
		const VkSubpassDescription subpass = {subpass_flags, VK_PIPELINE_BIND_POINT_GRAPHICS, input_reference_ptr ? 1u : 0u,
			input_reference_ptr ? input_reference_ptr : nullptr, color_reference_ptr ? 1u : 0u,
			color_reference_ptr ? color_reference_ptr : nullptr, nullptr, depth_reference_ptr, 0, nullptr};
		const VkRenderPassCreateInfo pass_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr, 0u,
			num_attachments, attachments.data(), 1u, &subpass, subpass_dependency_ptr ? 1u : 0u,
			subpass_dependency_ptr};

		VkRenderPass pass;
		VkResult res = vkCreateRenderPass(m_device, &pass_info, nullptr, &pass);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateRenderPass failed: ");
			return VK_NULL_HANDLE;
		}

		m_render_pass_cache.emplace(key.key, pass);
		return pass;
	}

	void Context::DestroyRenderPassCache()
	{
		for (auto& it : m_render_pass_cache)
			vkDestroyRenderPass(m_device, it.second, nullptr);

		m_render_pass_cache.clear();
	}
} // namespace Vulkan
