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

#include "PrecompiledHeader.h"

#include "GS/Renderers/Vulkan/VKContext.h"
#include "GS/Renderers/Vulkan/VKShaderCache.h"
#include "GS/Renderers/Vulkan/VKSwapChain.h"
#include "GS/Renderers/Vulkan/VKUtil.h"

#include "common/Align.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "common/General.h"
#include "common/StringUtil.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "fmt/format.h"

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#else
#include <time.h>
#endif

std::unique_ptr<VKContext> g_vulkan_context;

// Tweakables
enum : u32
{
	MAX_DRAW_CALLS_PER_FRAME = 8192,
	MAX_COMBINED_IMAGE_SAMPLER_DESCRIPTORS_PER_FRAME = 2 * MAX_DRAW_CALLS_PER_FRAME,
	MAX_SAMPLED_IMAGE_DESCRIPTORS_PER_FRAME =
		MAX_DRAW_CALLS_PER_FRAME, // assume at least half our draws aren't going to be shuffle/blending
	MAX_STORAGE_IMAGE_DESCRIPTORS_PER_FRAME = 4, // Currently used by CAS only
	MAX_INPUT_ATTACHMENT_IMAGE_DESCRIPTORS_PER_FRAME = MAX_DRAW_CALLS_PER_FRAME,
	MAX_DESCRIPTOR_SETS_PER_FRAME = MAX_DRAW_CALLS_PER_FRAME * 2
};

VKContext::VKContext(VkInstance instance, VkPhysicalDevice physical_device)
	: m_instance(instance)
	, m_physical_device(physical_device)
{
	// Read device physical memory properties, we need it for allocating buffers
	vkGetPhysicalDeviceProperties(physical_device, &m_device_properties);
	vkGetPhysicalDeviceMemoryProperties(physical_device, &m_device_memory_properties);

	// We need this to be at least 32 byte aligned for AVX2 stores.
	m_device_properties.limits.minUniformBufferOffsetAlignment =
		std::max(m_device_properties.limits.minUniformBufferOffsetAlignment, static_cast<VkDeviceSize>(32));
	m_device_properties.limits.minTexelBufferOffsetAlignment =
		std::max(m_device_properties.limits.minTexelBufferOffsetAlignment, static_cast<VkDeviceSize>(32));
	m_device_properties.limits.optimalBufferCopyOffsetAlignment =
		std::max(m_device_properties.limits.optimalBufferCopyOffsetAlignment, static_cast<VkDeviceSize>(32));
	m_device_properties.limits.optimalBufferCopyRowPitchAlignment = Common::NextPow2(
		std::max(m_device_properties.limits.optimalBufferCopyRowPitchAlignment, static_cast<VkDeviceSize>(32)));
	m_device_properties.limits.bufferImageGranularity =
		std::max(m_device_properties.limits.bufferImageGranularity, static_cast<VkDeviceSize>(32));
}

VKContext::~VKContext() = default;

VkInstance VKContext::CreateVulkanInstance(const WindowInfo& wi, bool enable_debug_utils, bool enable_validation_layer)
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

bool VKContext::SelectInstanceExtensions(ExtensionList* extension_list, const WindowInfo& wi, bool enable_debug_utils)
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
	if (wi.type != WindowInfo::Type::Surfaceless && !SupportsExtension(VK_KHR_SURFACE_EXTENSION_NAME, true))
		return false;

#if defined(VK_USE_PLATFORM_WIN32_KHR)
	if (wi.type == WindowInfo::Type::Win32 && !SupportsExtension(VK_KHR_WIN32_SURFACE_EXTENSION_NAME, true))
		return false;
#endif
#if defined(VK_USE_PLATFORM_XLIB_KHR)
	if (wi.type == WindowInfo::Type::X11 && !SupportsExtension(VK_KHR_XLIB_SURFACE_EXTENSION_NAME, true))
		return false;
#endif
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
	if (wi.type == WindowInfo::Type::Wayland && !SupportsExtension(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME, true))
		return false;
#endif
#if defined(VK_USE_PLATFORM_METAL_EXT)
	if (wi.type == WindowInfo::Type::MacOS && !SupportsExtension(VK_EXT_METAL_SURFACE_EXTENSION_NAME, true))
		return false;
#endif

	// VK_EXT_debug_utils
	if (enable_debug_utils && !SupportsExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, false))
		Console.Warning("Vulkan: Debug report requested, but extension is not available.");

	// Needed for exclusive fullscreen control.
	SupportsExtension(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME, false);

	return true;
}

VKContext::GPUList VKContext::EnumerateGPUs(VkInstance instance)
{
	GPUList gpus;

	u32 gpu_count = 0;
	VkResult res = vkEnumeratePhysicalDevices(instance, &gpu_count, nullptr);
	if ((res != VK_SUCCESS && res != VK_INCOMPLETE) || gpu_count == 0)
	{
		LOG_VULKAN_ERROR(res, "vkEnumeratePhysicalDevices (1) failed: ");
		return gpus;
	}

	std::vector<VkPhysicalDevice> physical_devices(gpu_count);
	res = vkEnumeratePhysicalDevices(instance, &gpu_count, physical_devices.data());
	if (res == VK_INCOMPLETE)
	{
		Console.Warning("First vkEnumeratePhysicalDevices() call returned %zu devices, but second returned %u",
			physical_devices.size(), gpu_count);
	}
	else if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkEnumeratePhysicalDevices (2) failed: ");
		return gpus;
	}

	// Maybe we lost a GPU?
	if (gpu_count < physical_devices.size())
		physical_devices.resize(gpu_count);

	gpus.reserve(physical_devices.size());
	for (VkPhysicalDevice device : physical_devices)
	{
		VkPhysicalDeviceProperties props = {};
		vkGetPhysicalDeviceProperties(device, &props);

		std::string gpu_name = props.deviceName;

		// handle duplicate adapter names
		if (std::any_of(
				gpus.begin(), gpus.end(), [&gpu_name](const auto& other) { return (gpu_name == other.second); }))
		{
			std::string original_adapter_name = std::move(gpu_name);

			u32 current_extra = 2;
			do
			{
				gpu_name = fmt::format("{} ({})", original_adapter_name, current_extra);
				current_extra++;
			} while (std::any_of(
				gpus.begin(), gpus.end(), [&gpu_name](const auto& other) { return (gpu_name == other.second); }));
		}

		gpus.emplace_back(device, std::move(gpu_name));
	}

	return gpus;
}

bool VKContext::Create(VkInstance instance, VkSurfaceKHR surface, VkPhysicalDevice physical_device,
	bool threaded_presentation, bool enable_debug_utils, bool enable_validation_layer)
{
	pxAssertMsg(!g_vulkan_context, "Has no current context");
	g_vulkan_context.reset(new VKContext(instance, physical_device));

	if (enable_debug_utils)
		g_vulkan_context->EnableDebugUtils();

	// Attempt to create the device.
	if (!g_vulkan_context->CreateDevice(surface, enable_validation_layer, nullptr, 0, nullptr, 0, nullptr) ||
		!VKShaderCache::Create() || !g_vulkan_context->CreateAllocator() ||
		!g_vulkan_context->CreateGlobalDescriptorPool() || !g_vulkan_context->CreateCommandBuffers() ||
		!g_vulkan_context->CreateTextureStreamBuffer() || !g_vulkan_context->InitSpinResources())
	{
		// Since we are destroying the instance, we're also responsible for destroying the surface.
		if (surface != VK_NULL_HANDLE)
			vkDestroySurfaceKHR(instance, surface, nullptr);

		// TODO: Move this to the destructor instead..
		g_vulkan_context->m_texture_upload_buffer.Destroy(false);
		g_vulkan_context->DestroySpinResources();
		g_vulkan_context->DestroyRenderPassCache();
		g_vulkan_context->DestroyGlobalDescriptorPool();
		g_vulkan_context->DestroyCommandBuffers();
		g_vulkan_context->DestroyAllocator();
		VKShaderCache::Destroy();

		if (g_vulkan_context->m_device != VK_NULL_HANDLE)
			vkDestroyDevice(g_vulkan_context->m_device, nullptr);

		if (g_vulkan_context->m_debug_messenger_callback != VK_NULL_HANDLE)
			g_vulkan_context->DisableDebugUtils();

		if (g_vulkan_context->m_instance != VK_NULL_HANDLE)
			vkDestroyInstance(g_vulkan_context->m_instance, nullptr);

		g_vulkan_context.reset();
		return false;
	}

	if (threaded_presentation)
		g_vulkan_context->StartPresentThread();

	return true;
}

void VKContext::Destroy()
{
	pxAssertMsg(g_vulkan_context, "Has context");

	g_vulkan_context->StopPresentThread();

	if (g_vulkan_context->m_device != VK_NULL_HANDLE)
		g_vulkan_context->WaitForGPUIdle();

	g_vulkan_context->m_texture_upload_buffer.Destroy(false);

	g_vulkan_context->DestroySpinResources();
	g_vulkan_context->DestroyRenderPassCache();
	g_vulkan_context->DestroyGlobalDescriptorPool();
	g_vulkan_context->DestroyCommandBuffers();
	g_vulkan_context->DestroyAllocator();

	VKShaderCache::Destroy();

	if (g_vulkan_context->m_device != VK_NULL_HANDLE)
		vkDestroyDevice(g_vulkan_context->m_device, nullptr);

	if (g_vulkan_context->m_debug_messenger_callback != VK_NULL_HANDLE)
		g_vulkan_context->DisableDebugUtils();

	if (g_vulkan_context->m_instance != VK_NULL_HANDLE)
		vkDestroyInstance(g_vulkan_context->m_instance, nullptr);

	Vulkan::UnloadVulkanLibrary();

	g_vulkan_context.reset();
}

bool VKContext::SelectDeviceExtensions(ExtensionList* extension_list, bool enable_surface)
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

	m_optional_extensions.vk_ext_provoking_vertex = SupportsExtension(VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME, false);
	m_optional_extensions.vk_ext_memory_budget = SupportsExtension(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME, false);
	m_optional_extensions.vk_ext_calibrated_timestamps =
		SupportsExtension(VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME, false);
	m_optional_extensions.vk_ext_line_rasterization =
		SupportsExtension(VK_EXT_LINE_RASTERIZATION_EXTENSION_NAME, false);
	m_optional_extensions.vk_ext_rasterization_order_attachment_access =
		SupportsExtension(VK_EXT_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_EXTENSION_NAME, false) ||
		SupportsExtension(VK_ARM_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_EXTENSION_NAME, false);
	m_optional_extensions.vk_khr_driver_properties = SupportsExtension(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME, false);
	m_optional_extensions.vk_khr_fragment_shader_barycentric =
		SupportsExtension(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME, false);
	m_optional_extensions.vk_khr_shader_draw_parameters =
		SupportsExtension(VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME, false);

#ifdef _WIN32
	m_optional_extensions.vk_ext_full_screen_exclusive =
		enable_surface && SupportsExtension(VK_EXT_FULL_SCREEN_EXCLUSIVE_EXTENSION_NAME, false);
#endif

	return true;
}

bool VKContext::SelectDeviceFeatures(const VkPhysicalDeviceFeatures* required_features)
{
	VkPhysicalDeviceFeatures available_features;
	vkGetPhysicalDeviceFeatures(m_physical_device, &available_features);

	if (required_features)
		std::memcpy(&m_device_features, required_features, sizeof(m_device_features));

	// Enable the features we use.
	m_device_features.dualSrcBlend = available_features.dualSrcBlend;
	m_device_features.largePoints = available_features.largePoints;
	m_device_features.wideLines = available_features.wideLines;
	m_device_features.fragmentStoresAndAtomics = available_features.fragmentStoresAndAtomics;
	m_device_features.textureCompressionBC = available_features.textureCompressionBC;
	m_device_features.samplerAnisotropy = available_features.samplerAnisotropy;
	m_device_features.geometryShader = available_features.geometryShader;

	return true;
}

bool VKContext::CreateDevice(VkSurfaceKHR surface, bool enable_validation_layer,
	const char** required_device_extensions, u32 num_required_device_extensions, const char** required_device_layers,
	u32 num_required_device_layers, const VkPhysicalDeviceFeatures* required_features)
{
	u32 queue_family_count;
	vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, nullptr);
	if (queue_family_count == 0)
	{
		Console.Error("No queue families found on specified vulkan physical device.");
		return false;
	}

	std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(m_physical_device, &queue_family_count, queue_family_properties.data());
	Console.WriteLn("%u vulkan queue families", queue_family_count);

	// Find graphics and present queues.
	m_graphics_queue_family_index = queue_family_count;
	m_present_queue_family_index = queue_family_count;
	m_spin_queue_family_index = queue_family_count;
	u32 spin_queue_index = 0;
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
	for (uint32_t i = 0; i < queue_family_count; i++)
	{
		// Pick a queue for spinning
		if (!(queue_family_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT))
			continue; // We need compute
		if (queue_family_properties[i].timestampValidBits == 0)
			continue; // We need timing
		const bool queue_is_used = i == m_graphics_queue_family_index || i == m_present_queue_family_index;
		if (queue_is_used && m_spin_queue_family_index != queue_family_count)
			continue; // Found a non-graphics queue to use
		spin_queue_index = 0;
		m_spin_queue_family_index = i;
		if (queue_is_used && queue_family_properties[i].queueCount > 1)
			spin_queue_index = 1;
		if (!(queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
			break; // Async compute queue, definitely pick this one
	}
	if (m_graphics_queue_family_index == queue_family_count)
	{
		Console.Error("Vulkan: Failed to find an acceptable graphics queue.");
		return false;
	}
	if (surface != VK_NULL_HANDLE && m_present_queue_family_index == queue_family_count)
	{
		Console.Error("Vulkan: Failed to find an acceptable present queue.");
		return false;
	}

	VkDeviceCreateInfo device_info = {};
	device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_info.pNext = nullptr;
	device_info.flags = 0;
	device_info.queueCreateInfoCount = 0;

	static constexpr float queue_priorities[] = {1.0f, 0.0f}; // Low priority for the spin queue
	std::array<VkDeviceQueueCreateInfo, 3> queue_infos;
	VkDeviceQueueCreateInfo& graphics_queue_info = queue_infos[device_info.queueCreateInfoCount++];
	graphics_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	graphics_queue_info.pNext = nullptr;
	graphics_queue_info.flags = 0;
	graphics_queue_info.queueFamilyIndex = m_graphics_queue_family_index;
	graphics_queue_info.queueCount = 1;
	graphics_queue_info.pQueuePriorities = queue_priorities;

	if (surface != VK_NULL_HANDLE && m_graphics_queue_family_index != m_present_queue_family_index)
	{
		VkDeviceQueueCreateInfo& present_queue_info = queue_infos[device_info.queueCreateInfoCount++];
		present_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		present_queue_info.pNext = nullptr;
		present_queue_info.flags = 0;
		present_queue_info.queueFamilyIndex = m_present_queue_family_index;
		present_queue_info.queueCount = 1;
		present_queue_info.pQueuePriorities = queue_priorities;
	}

	if (m_spin_queue_family_index == m_graphics_queue_family_index)
	{
		if (spin_queue_index != 0)
			graphics_queue_info.queueCount = 2;
	}
	else if (m_spin_queue_family_index == m_present_queue_family_index)
	{
		if (spin_queue_index != 0)
			queue_infos[1].queueCount = 2; // present queue
	}
	else
	{
		VkDeviceQueueCreateInfo& spin_queue_info = queue_infos[device_info.queueCreateInfoCount++];
		spin_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		spin_queue_info.pNext = nullptr;
		spin_queue_info.flags = 0;
		spin_queue_info.queueFamilyIndex = m_spin_queue_family_index;
		spin_queue_info.queueCount = 1;
		spin_queue_info.pQueuePriorities = queue_priorities + 1;
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
	VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT rasterization_order_access_feature = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_FEATURES_EXT};
	VkPhysicalDeviceLineRasterizationFeaturesEXT line_rasterization_feature = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT};

	if (m_optional_extensions.vk_ext_provoking_vertex)
	{
		provoking_vertex_feature.provokingVertexLast = VK_TRUE;
		Vulkan::AddPointerToChain(&device_info, &provoking_vertex_feature);
	}
	if (m_optional_extensions.vk_ext_line_rasterization)
	{
		line_rasterization_feature.bresenhamLines = VK_TRUE;
		Vulkan::AddPointerToChain(&device_info, &line_rasterization_feature);
	}
	if (m_optional_extensions.vk_ext_rasterization_order_attachment_access)
	{
		rasterization_order_access_feature.rasterizationOrderColorAttachmentAccess = VK_TRUE;
		Vulkan::AddPointerToChain(&device_info, &rasterization_order_access_feature);
	}

	VkResult res = vkCreateDevice(m_physical_device, &device_info, nullptr, &m_device);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateDevice failed: ");
		return false;
	}

	// With the device created, we can fill the remaining entry points.
	if (!Vulkan::LoadVulkanDeviceFunctions(m_device))
		return false;

	// Grab the graphics and present queues.
	vkGetDeviceQueue(m_device, m_graphics_queue_family_index, 0, &m_graphics_queue);
	if (surface)
	{
		vkGetDeviceQueue(m_device, m_present_queue_family_index, 0, &m_present_queue);
	}
	m_spinning_supported = m_spin_queue_family_index != queue_family_count &&
						   queue_family_properties[m_graphics_queue_family_index].timestampValidBits > 0 &&
						   m_device_properties.limits.timestampPeriod > 0;
	m_spin_queue_is_graphics_queue =
		m_spin_queue_family_index == m_graphics_queue_family_index && spin_queue_index == 0;

	m_gpu_timing_supported = (m_device_properties.limits.timestampComputeAndGraphics != 0 &&
							  queue_family_properties[m_graphics_queue_family_index].timestampValidBits > 0 &&
							  m_device_properties.limits.timestampPeriod > 0);
	DevCon.WriteLn("GPU timing is %s (TS=%u TS valid bits=%u, TS period=%f)",
		m_gpu_timing_supported ? "supported" : "not supported",
		static_cast<u32>(m_device_properties.limits.timestampComputeAndGraphics),
		queue_family_properties[m_graphics_queue_family_index].timestampValidBits,
		m_device_properties.limits.timestampPeriod);

	ProcessDeviceExtensions();

	if (m_spinning_supported)
	{
		vkGetDeviceQueue(m_device, m_spin_queue_family_index, spin_queue_index, &m_spin_queue);

		m_spin_timestamp_scale = m_device_properties.limits.timestampPeriod;
		if (m_optional_extensions.vk_ext_calibrated_timestamps)
		{
#ifdef _WIN32
			LARGE_INTEGER Freq;
			QueryPerformanceFrequency(&Freq);
			m_queryperfcounter_to_ns = 1000000000.0 / static_cast<double>(Freq.QuadPart);
#endif
			CalibrateSpinTimestamp();
		}
	}

	return true;
}

void VKContext::ProcessDeviceExtensions()
{
	// advanced feature checks
	VkPhysicalDeviceFeatures2 features2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
	VkPhysicalDeviceProvokingVertexFeaturesEXT provoking_vertex_features = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROVOKING_VERTEX_FEATURES_EXT};
	VkPhysicalDeviceLineRasterizationFeaturesEXT line_rasterization_feature = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_LINE_RASTERIZATION_FEATURES_EXT};
	VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT rasterization_order_access_feature = {
		VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_FEATURES_EXT};

	// add in optional feature structs
	if (m_optional_extensions.vk_ext_provoking_vertex)
		Vulkan::AddPointerToChain(&features2, &provoking_vertex_features);
	if (m_optional_extensions.vk_ext_line_rasterization)
		Vulkan::AddPointerToChain(&features2, &line_rasterization_feature);
	if (m_optional_extensions.vk_ext_rasterization_order_attachment_access)
		Vulkan::AddPointerToChain(&features2, &rasterization_order_access_feature);

	// query
	vkGetPhysicalDeviceFeatures2(m_physical_device, &features2);

	// confirm we actually support it
	m_optional_extensions.vk_ext_provoking_vertex &= (provoking_vertex_features.provokingVertexLast == VK_TRUE);
	m_optional_extensions.vk_ext_rasterization_order_attachment_access &=
		(rasterization_order_access_feature.rasterizationOrderColorAttachmentAccess == VK_TRUE);
	m_optional_extensions.vk_ext_line_rasterization &= (line_rasterization_feature.bresenhamLines == VK_TRUE);

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

	// VK_EXT_calibrated_timestamps checking
	if (m_optional_extensions.vk_ext_calibrated_timestamps)
	{
		u32 count = 0;
		vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(m_physical_device, &count, nullptr);
		std::unique_ptr<VkTimeDomainEXT[]> time_domains = std::make_unique<VkTimeDomainEXT[]>(count);
		vkGetPhysicalDeviceCalibrateableTimeDomainsEXT(m_physical_device, &count, time_domains.get());
		const VkTimeDomainEXT* begin = &time_domains[0];
		const VkTimeDomainEXT* end = &time_domains[count];
		if (std::find(begin, end, VK_TIME_DOMAIN_DEVICE_EXT) == end)
			m_optional_extensions.vk_ext_calibrated_timestamps = false;
		VkTimeDomainEXT preferred_types[] = {
#ifdef _WIN32
			VK_TIME_DOMAIN_QUERY_PERFORMANCE_COUNTER_EXT,
#else
#ifdef CLOCK_MONOTONIC_RAW
			VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_EXT,
#endif
			VK_TIME_DOMAIN_CLOCK_MONOTONIC_EXT,
#endif
		};
		m_calibrated_timestamp_type = VK_TIME_DOMAIN_DEVICE_EXT;
		for (VkTimeDomainEXT type : preferred_types)
		{
			if (std::find(begin, end, type) != end)
			{
				m_calibrated_timestamp_type = type;
				break;
			}
		}
		if (m_calibrated_timestamp_type == VK_TIME_DOMAIN_DEVICE_EXT)
			m_optional_extensions.vk_ext_calibrated_timestamps = false;
	}

	Console.WriteLn(
		"VK_EXT_provoking_vertex is %s", m_optional_extensions.vk_ext_provoking_vertex ? "supported" : "NOT supported");
	Console.WriteLn("VK_EXT_line_rasterization is %s",
		m_optional_extensions.vk_ext_line_rasterization ? "supported" : "NOT supported");
	Console.WriteLn("VK_EXT_calibrated_timestamps is %s",
		m_optional_extensions.vk_ext_calibrated_timestamps ? "supported" : "NOT supported");
	Console.WriteLn("VK_EXT_rasterization_order_attachment_access is %s",
		m_optional_extensions.vk_ext_rasterization_order_attachment_access ? "supported" : "NOT supported");
}

bool VKContext::CreateAllocator()
{
	VmaAllocatorCreateInfo ci = {};
	ci.vulkanApiVersion = VK_API_VERSION_1_1;
	ci.flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT;
	ci.physicalDevice = m_physical_device;
	ci.device = m_device;
	ci.instance = m_instance;

	if (m_optional_extensions.vk_ext_memory_budget)
		ci.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;

	// Limit usage of the DEVICE_LOCAL upload heap when we're using a debug device.
	// On NVIDIA drivers, it results in frequently running out of device memory when trying to
	// play back captures in RenderDoc, making life very painful. Re-BAR GPUs should be fine.
	constexpr VkDeviceSize UPLOAD_HEAP_SIZE_THRESHOLD = 512 * 1024 * 1024;
	constexpr VkMemoryPropertyFlags UPLOAD_HEAP_PROPERTIES =
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	std::array<VkDeviceSize, VK_MAX_MEMORY_HEAPS> heap_size_limits;
	if (GSConfig.UseDebugDevice)
	{
		bool has_upload_heap = false;
		heap_size_limits.fill(VK_WHOLE_SIZE);
		for (u32 i = 0; i < m_device_memory_properties.memoryTypeCount; i++)
		{
			// Look for any memory types which are upload-like.
			const VkMemoryType& type = m_device_memory_properties.memoryTypes[i];
			if ((type.propertyFlags & UPLOAD_HEAP_PROPERTIES) != UPLOAD_HEAP_PROPERTIES)
				continue;

			const VkMemoryHeap& heap = m_device_memory_properties.memoryHeaps[type.heapIndex];
			if (heap.size >= UPLOAD_HEAP_SIZE_THRESHOLD)
				continue;

			if (heap_size_limits[type.heapIndex] == VK_WHOLE_SIZE)
			{
				Console.Warning("Disabling allocation from upload heap #%u (%.2f MB) due to debug device.",
					type.heapIndex, static_cast<float>(heap.size) / 1048576.0f);
				heap_size_limits[type.heapIndex] = 0;
				has_upload_heap = true;
			}
		}

		if (has_upload_heap)
			ci.pHeapSizeLimit = heap_size_limits.data();
	}

	VkResult res = vmaCreateAllocator(&ci, &m_allocator);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vmaCreateAllocator failed: ");
		return false;
	}

	return true;
}

void VKContext::DestroyAllocator()
{
	if (m_allocator == VK_NULL_HANDLE)
		return;

	vmaDestroyAllocator(m_allocator);
	m_allocator = VK_NULL_HANDLE;
}

bool VKContext::CreateCommandBuffers()
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
		Vulkan::SetObjectName(
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
			Vulkan::SetObjectName(g_vulkan_context->GetDevice(), resources.command_buffers[i],
				"Frame %u %sCommand Buffer", frame_index, (i == 0) ? "Init" : "");
		}

		VkFenceCreateInfo fence_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};

		res = vkCreateFence(m_device, &fence_info, nullptr, &resources.fence);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateFence failed: ");
			return false;
		}
		Vulkan::SetObjectName(g_vulkan_context->GetDevice(), resources.fence, "Frame Fence %u", frame_index);
		// TODO: A better way to choose the number of descriptors.
		VkDescriptorPoolSize pool_sizes[] = {
			{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_COMBINED_IMAGE_SAMPLER_DESCRIPTORS_PER_FRAME},
			{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_SAMPLED_IMAGE_DESCRIPTORS_PER_FRAME},
			{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_STORAGE_IMAGE_DESCRIPTORS_PER_FRAME},
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
		Vulkan::SetObjectName(
			g_vulkan_context->GetDevice(), resources.descriptor_pool, "Frame Descriptor Pool %u", frame_index);

		++frame_index;
	}

	ActivateCommandBuffer(0);
	return true;
}

void VKContext::DestroyCommandBuffers()
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
			vkFreeCommandBuffers(m_device, resources.command_pool, static_cast<u32>(resources.command_buffers.size()),
				resources.command_buffers.data());
			resources.command_buffers.fill(VK_NULL_HANDLE);
		}
		if (resources.command_pool != VK_NULL_HANDLE)
		{
			vkDestroyCommandPool(m_device, resources.command_pool, nullptr);
			resources.command_pool = VK_NULL_HANDLE;
		}
	}
}

bool VKContext::CreateGlobalDescriptorPool()
{
	static constexpr const VkDescriptorPoolSize pool_sizes[] = {
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 2},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2},
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
	Vulkan::SetObjectName(g_vulkan_context->GetDevice(), m_global_descriptor_pool, "Global Descriptor Pool");

	if (m_gpu_timing_supported)
	{
		const VkQueryPoolCreateInfo query_create_info = {
			VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, nullptr, 0, VK_QUERY_TYPE_TIMESTAMP, NUM_COMMAND_BUFFERS * 4, 0};
		res = vkCreateQueryPool(m_device, &query_create_info, nullptr, &m_timestamp_query_pool);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateQueryPool failed: ");
			m_gpu_timing_supported = false;
			return false;
		}
	}

	return true;
}

void VKContext::DestroyGlobalDescriptorPool()
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

bool VKContext::CreateTextureStreamBuffer()
{
	if (!m_texture_upload_buffer.Create(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, TEXTURE_BUFFER_SIZE))
	{
		Console.Error("Failed to allocate texture upload buffer");
		return false;
	}

	return true;
}

VkRenderPass VKContext::GetRenderPassForRestarting(VkRenderPass pass)
{
	for (const auto& it : m_render_pass_cache)
	{
		if (it.second != pass)
			continue;

		RenderPassCacheKey modified_key;
		modified_key.key = it.first;
		if (modified_key.color_load_op == VK_ATTACHMENT_LOAD_OP_CLEAR)
			modified_key.color_load_op = VK_ATTACHMENT_LOAD_OP_LOAD;
		if (modified_key.depth_load_op == VK_ATTACHMENT_LOAD_OP_CLEAR)
			modified_key.depth_load_op = VK_ATTACHMENT_LOAD_OP_LOAD;
		if (modified_key.stencil_load_op == VK_ATTACHMENT_LOAD_OP_CLEAR)
			modified_key.stencil_load_op = VK_ATTACHMENT_LOAD_OP_LOAD;

		if (modified_key.key == it.first)
			return pass;

		auto fit = m_render_pass_cache.find(modified_key.key);
		if (fit != m_render_pass_cache.end())
			return fit->second;

		return CreateCachedRenderPass(modified_key);
	}

	return pass;
}

VkCommandBuffer VKContext::GetCurrentInitCommandBuffer()
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

VkDescriptorSet VKContext::AllocateDescriptorSet(VkDescriptorSetLayout set_layout)
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

VkDescriptorSet VKContext::AllocatePersistentDescriptorSet(VkDescriptorSetLayout set_layout)
{
	VkDescriptorSetAllocateInfo allocate_info = {
		VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, m_global_descriptor_pool, 1, &set_layout};

	VkDescriptorSet descriptor_set;
	VkResult res = vkAllocateDescriptorSets(m_device, &allocate_info, &descriptor_set);
	if (res != VK_SUCCESS)
		return VK_NULL_HANDLE;

	return descriptor_set;
}

void VKContext::FreeGlobalDescriptorSet(VkDescriptorSet set)
{
	vkFreeDescriptorSets(m_device, m_global_descriptor_pool, 1, &set);
}

void VKContext::WaitForFenceCounter(u64 fence_counter)
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

void VKContext::WaitForGPUIdle()
{
	WaitForPresentComplete();
	vkDeviceWaitIdle(m_device);
}

float VKContext::GetAndResetAccumulatedGPUTime()
{
	const float time = m_accumulated_gpu_time;
	m_accumulated_gpu_time = 0.0f;
	return time;
}

bool VKContext::SetEnableGPUTiming(bool enabled)
{
	m_gpu_timing_enabled = enabled && m_gpu_timing_supported;
	return (enabled == m_gpu_timing_enabled);
}

void VKContext::ScanForCommandBufferCompletion()
{
	for (u32 check_index = (m_current_frame + 1) % NUM_COMMAND_BUFFERS; check_index != m_current_frame;
		 check_index = (check_index + 1) % NUM_COMMAND_BUFFERS)
	{
		FrameResources& resources = m_frame_resources[check_index];
		if (resources.fence_counter <= m_completed_fence_counter)
			continue; // Already completed
		if (vkGetFenceStatus(m_device, resources.fence) != VK_SUCCESS)
			break; // Fence not signaled, later fences won't be either
		CommandBufferCompleted(check_index);
		m_completed_fence_counter = resources.fence_counter;
	}
	for (SpinResources& resources : m_spin_resources)
	{
		if (!resources.in_progress)
			continue;
		if (vkGetFenceStatus(m_device, resources.fence) != VK_SUCCESS)
			continue;
		SpinCommandCompleted(&resources - &m_spin_resources[0]);
	}
}

void VKContext::WaitForCommandBufferCompletion(u32 index)
{
	// Wait for this command buffer to be completed.
	const VkResult res = vkWaitForFences(m_device, 1, &m_frame_resources[index].fence, VK_TRUE, UINT64_MAX);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkWaitForFences failed: ");
		m_last_submit_failed.store(true, std::memory_order_release);
		return;
	}

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
			CommandBufferCompleted(cleanup_index);

		cleanup_index = (cleanup_index + 1) % NUM_COMMAND_BUFFERS;
	}

	m_completed_fence_counter = now_completed_counter;
}

void VKContext::SubmitCommandBuffer(
	VKSwapChain* present_swap_chain /* = nullptr */, bool submit_on_thread /* = false */)
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

	bool wants_timestamp = m_gpu_timing_enabled || m_spin_timer;
	if (wants_timestamp && resources.timestamp_written)
	{
		vkCmdWriteTimestamp(m_current_command_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_timestamp_query_pool,
			m_current_frame * 2 + 1);
	}

	res = vkEndCommandBuffer(resources.command_buffers[1]);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkEndCommandBuffer failed: ");
		pxFailRel("Failed to end command buffer");
	}

	// This command buffer now has commands, so can't be re-used without waiting.
	resources.needs_fence_wait = true;

	u32 spin_cycles = 0;
	const bool spin_enabled = m_spin_timer;
	if (spin_enabled)
	{
		ScanForCommandBufferCompletion();
		auto draw = m_spin_manager.DrawSubmitted(m_command_buffer_render_passes);
		u32 constant_offset =
			400000 * m_spin_manager.SpinsPerUnitTime(); // 400µs, just to be safe since going over gets really bad
		if (m_optional_extensions.vk_ext_calibrated_timestamps)
			constant_offset /=
				2; // Safety factor isn't as important here, going over just hurts this one submission a bit
		u32 minimum_spin = 200000 * m_spin_manager.SpinsPerUnitTime();
		u32 maximum_spin = std::max<u32>(1024, 16000000 * m_spin_manager.SpinsPerUnitTime()); // 16ms
		if (draw.recommended_spin > minimum_spin + constant_offset)
			spin_cycles = std::min(draw.recommended_spin - constant_offset, maximum_spin);
		resources.spin_id = draw.id;
	}
	else
	{
		resources.spin_id = -1;
	}
	m_command_buffer_render_passes = 0;

	if (present_swap_chain != VK_NULL_HANDLE && m_spinning_supported)
	{
		m_spin_manager.NextFrame();
		if (m_spin_timer)
			m_spin_timer--;
		// Calibrate a max of once per frame
		m_wants_new_timestamp_calibration = m_optional_extensions.vk_ext_calibrated_timestamps;
	}

	if (spin_cycles != 0)
		WaitForSpinCompletion(m_current_frame);

	std::unique_lock<std::mutex> lock(m_present_mutex);
	WaitForPresentComplete(lock);

	if (spin_enabled && m_optional_extensions.vk_ext_calibrated_timestamps)
		resources.submit_timestamp = GetCPUTimestamp();

	if (!submit_on_thread || !m_present_thread.joinable())
	{
		DoSubmitCommandBuffer(m_current_frame, present_swap_chain, spin_cycles);
		if (present_swap_chain)
			DoPresent(present_swap_chain);
		return;
	}

	m_queued_present.command_buffer_index = m_current_frame;
	m_queued_present.swap_chain = present_swap_chain;
	m_queued_present.spin_cycles = spin_cycles;
	m_present_done.store(false);
	m_present_queued_cv.notify_one();
}

void VKContext::DoSubmitCommandBuffer(u32 index, VKSwapChain* present_swap_chain, u32 spin_cycles)
{
	FrameResources& resources = m_frame_resources[index];

	uint32_t wait_bits = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSemaphore semas[2];
	VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submit_info.commandBufferCount = resources.init_buffer_used ? 2u : 1u;
	submit_info.pCommandBuffers =
		resources.init_buffer_used ? resources.command_buffers.data() : &resources.command_buffers[1];

	if (present_swap_chain)
	{
		submit_info.pWaitSemaphores = present_swap_chain->GetImageAvailableSemaphorePtr();
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitDstStageMask = &wait_bits;

		if (spin_cycles != 0)
		{
			semas[0] = present_swap_chain->GetRenderingFinishedSemaphore();
			semas[1] = m_spin_resources[index].semaphore;
			submit_info.signalSemaphoreCount = 2;
			submit_info.pSignalSemaphores = semas;
		}
		else
		{
			submit_info.pSignalSemaphores = present_swap_chain->GetRenderingFinishedSemaphorePtr();
			submit_info.signalSemaphoreCount = 1;
		}
	}
	else if (spin_cycles != 0)
	{
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &m_spin_resources[index].semaphore;
	}

	const VkResult res = vkQueueSubmit(m_graphics_queue, 1, &submit_info, resources.fence);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkQueueSubmit failed: ");
		m_last_submit_failed.store(true, std::memory_order_release);
		return;
	}

	if (spin_cycles != 0)
		SubmitSpinCommand(index, spin_cycles);
}

void VKContext::DoPresent(VKSwapChain* present_swap_chain)
{
	const VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, nullptr, 1,
		present_swap_chain->GetRenderingFinishedSemaphorePtr(), 1, present_swap_chain->GetSwapChainPtr(),
		present_swap_chain->GetCurrentImageIndexPtr(), nullptr};

	present_swap_chain->ReleaseCurrentImage();

	const VkResult res = vkQueuePresentKHR(m_present_queue, &present_info);
	if (res != VK_SUCCESS)
	{
		// VK_ERROR_OUT_OF_DATE_KHR is not fatal, just means we need to recreate our swap chain.
		if (res != VK_ERROR_OUT_OF_DATE_KHR && res != VK_SUBOPTIMAL_KHR)
			LOG_VULKAN_ERROR(res, "vkQueuePresentKHR failed: ");

		m_last_present_failed.store(true, std::memory_order_release);
		return;
	}

	// Grab the next image as soon as possible, that way we spend less time blocked on the next
	// submission. Don't care if it fails, we'll deal with that at the presentation call site.
	// Credit to dxvk for the idea.
	present_swap_chain->AcquireNextImage();
}

void VKContext::WaitForPresentComplete()
{
	if (m_present_done.load())
		return;

	std::unique_lock<std::mutex> lock(m_present_mutex);
	WaitForPresentComplete(lock);
}

void VKContext::WaitForPresentComplete(std::unique_lock<std::mutex>& lock)
{
	if (m_present_done.load())
		return;

	m_present_done_cv.wait(lock, [this]() { return m_present_done.load(); });
}

void VKContext::PresentThread()
{
	std::unique_lock<std::mutex> lock(m_present_mutex);
	while (!m_present_thread_done.load())
	{
		m_present_queued_cv.wait(lock, [this]() { return !m_present_done.load() || m_present_thread_done.load(); });

		if (m_present_done.load())
			continue;

		DoSubmitCommandBuffer(
			m_queued_present.command_buffer_index, m_queued_present.swap_chain, m_queued_present.spin_cycles);
		if (m_queued_present.swap_chain)
			DoPresent(m_queued_present.swap_chain);
		m_present_done.store(true);
		m_present_done_cv.notify_one();
	}
}

void VKContext::StartPresentThread()
{
	pxAssert(!m_present_thread.joinable());
	m_present_thread_done.store(false);
	m_present_thread = std::thread(&VKContext::PresentThread, this);
}

void VKContext::StopPresentThread()
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

void VKContext::CommandBufferCompleted(u32 index)
{
	FrameResources& resources = m_frame_resources[index];

	for (auto& it : resources.cleanup_resources)
		it();
	resources.cleanup_resources.clear();

	bool wants_timestamps = m_gpu_timing_enabled || resources.spin_id >= 0;

	if (wants_timestamps && resources.timestamp_written)
	{
		std::array<u64, 2> timestamps;
		VkResult res =
			vkGetQueryPoolResults(m_device, m_timestamp_query_pool, index * 2, static_cast<u32>(timestamps.size()),
				sizeof(u64) * timestamps.size(), timestamps.data(), sizeof(u64), VK_QUERY_RESULT_64_BIT);
		if (res == VK_SUCCESS)
		{
			// if we didn't write the timestamp at the start of the cmdbuffer (just enabled timing), the first TS will be zero
			if (timestamps[0] > 0 && m_gpu_timing_enabled)
			{
				const double ns_diff =
					(timestamps[1] - timestamps[0]) * static_cast<double>(m_device_properties.limits.timestampPeriod);
				m_accumulated_gpu_time += ns_diff / 1000000.0;
			}
			if (resources.spin_id >= 0)
			{
				if (m_optional_extensions.vk_ext_calibrated_timestamps && timestamps[1] > 0)
				{
					u64 end = timestamps[1] * m_spin_timestamp_scale + m_spin_timestamp_offset;
					m_spin_manager.DrawCompleted(resources.spin_id, resources.submit_timestamp, end);
				}
				else if (!m_optional_extensions.vk_ext_calibrated_timestamps && timestamps[0] > 0)
				{
					u64 begin = timestamps[0] * m_spin_timestamp_scale;
					u64 end = timestamps[1] * m_spin_timestamp_scale;
					m_spin_manager.DrawCompleted(resources.spin_id, begin, end);
				}
			}
		}
		else
		{
			LOG_VULKAN_ERROR(res, "vkGetQueryPoolResults failed: ");
		}
	}
}

void VKContext::MoveToNextCommandBuffer()
{
	ActivateCommandBuffer((m_current_frame + 1) % NUM_COMMAND_BUFFERS);
}

void VKContext::ActivateCommandBuffer(u32 index)
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

	bool wants_timestamp = m_gpu_timing_enabled || m_spin_timer;
	if (wants_timestamp)
	{
		vkCmdResetQueryPool(resources.command_buffers[1], m_timestamp_query_pool, index * 2, 2);
		vkCmdWriteTimestamp(
			resources.command_buffers[1], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_timestamp_query_pool, index * 2);
	}

	resources.fence_counter = m_next_fence_counter++;
	resources.init_buffer_used = false;
	resources.timestamp_written = wants_timestamp;

	m_current_frame = index;
	m_current_command_buffer = resources.command_buffers[1];

	// using the lower 32 bits of the fence index should be sufficient here, I hope...
	vmaSetCurrentFrameIndex(m_allocator, static_cast<u32>(m_next_fence_counter));
}

void VKContext::ExecuteCommandBuffer(WaitType wait_for_completion)
{
	if (m_last_submit_failed.load(std::memory_order_acquire))
		return;

	// If we're waiting for completion, don't bother waking the worker thread.
	const u32 current_frame = m_current_frame;
	SubmitCommandBuffer();
	MoveToNextCommandBuffer();

	if (wait_for_completion != WaitType::None)
	{
		// Calibrate while we wait
		if (m_wants_new_timestamp_calibration)
			CalibrateSpinTimestamp();
		if (wait_for_completion == WaitType::Spin)
		{
			while (vkGetFenceStatus(m_device, m_frame_resources[current_frame].fence) == VK_NOT_READY)
				ShortSpin();
		}
		WaitForCommandBufferCompletion(current_frame);
	}
}

bool VKContext::CheckLastPresentFail()
{
	return m_last_present_failed.exchange(false, std::memory_order_acq_rel);
}

bool VKContext::CheckLastSubmitFail()
{
	return m_last_submit_failed.load(std::memory_order_acquire);
}

void VKContext::DeferBufferDestruction(VkBuffer object)
{
	FrameResources& resources = m_frame_resources[m_current_frame];
	resources.cleanup_resources.push_back([this, object]() { vkDestroyBuffer(m_device, object, nullptr); });
}

void VKContext::DeferBufferDestruction(VkBuffer object, VmaAllocation allocation)
{
	FrameResources& resources = m_frame_resources[m_current_frame];
	resources.cleanup_resources.push_back(
		[this, object, allocation]() { vmaDestroyBuffer(m_allocator, object, allocation); });
}

void VKContext::DeferBufferViewDestruction(VkBufferView object)
{
	FrameResources& resources = m_frame_resources[m_current_frame];
	resources.cleanup_resources.push_back([this, object]() { vkDestroyBufferView(m_device, object, nullptr); });
}

void VKContext::DeferDeviceMemoryDestruction(VkDeviceMemory object)
{
	FrameResources& resources = m_frame_resources[m_current_frame];
	resources.cleanup_resources.push_back([this, object]() { vkFreeMemory(m_device, object, nullptr); });
}

void VKContext::DeferFramebufferDestruction(VkFramebuffer object)
{
	FrameResources& resources = m_frame_resources[m_current_frame];
	resources.cleanup_resources.push_back([this, object]() { vkDestroyFramebuffer(m_device, object, nullptr); });
}

void VKContext::DeferImageDestruction(VkImage object)
{
	FrameResources& resources = m_frame_resources[m_current_frame];
	resources.cleanup_resources.push_back([this, object]() { vkDestroyImage(m_device, object, nullptr); });
}

void VKContext::DeferImageDestruction(VkImage object, VmaAllocation allocation)
{
	FrameResources& resources = m_frame_resources[m_current_frame];
	resources.cleanup_resources.push_back(
		[this, object, allocation]() { vmaDestroyImage(m_allocator, object, allocation); });
}

void VKContext::DeferImageViewDestruction(VkImageView object)
{
	FrameResources& resources = m_frame_resources[m_current_frame];
	resources.cleanup_resources.push_back([this, object]() { vkDestroyImageView(m_device, object, nullptr); });
}

void VKContext::DeferPipelineDestruction(VkPipeline pipeline)
{
	FrameResources& resources = m_frame_resources[m_current_frame];
	resources.cleanup_resources.push_back([this, pipeline]() { vkDestroyPipeline(m_device, pipeline, nullptr); });
}

void VKContext::DeferSamplerDestruction(VkSampler sampler)
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

bool VKContext::EnableDebugUtils()
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

	const VkResult res =
		vkCreateDebugUtilsMessengerEXT(m_instance, &messenger_info, nullptr, &m_debug_messenger_callback);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateDebugUtilsMessengerEXT failed: ");
		return false;
	}

	return true;
}

void VKContext::DisableDebugUtils()
{
	if (m_debug_messenger_callback != VK_NULL_HANDLE)
	{
		vkDestroyDebugUtilsMessengerEXT(m_instance, m_debug_messenger_callback, nullptr);
		m_debug_messenger_callback = VK_NULL_HANDLE;
	}
}

VkRenderPass VKContext::CreateCachedRenderPass(RenderPassCacheKey key)
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
			static_cast<VkAttachmentLoadOp>(key.color_load_op), static_cast<VkAttachmentStoreOp>(key.color_store_op),
			VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
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

			if (!g_vulkan_context->GetOptionalExtensions().vk_ext_rasterization_order_attachment_access)
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
		const VkImageLayout layout =
			key.depth_sampling ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments[num_attachments] = {0, static_cast<VkFormat>(key.depth_format), VK_SAMPLE_COUNT_1_BIT,
			static_cast<VkAttachmentLoadOp>(key.depth_load_op), static_cast<VkAttachmentStoreOp>(key.depth_store_op),
			static_cast<VkAttachmentLoadOp>(key.stencil_load_op),
			static_cast<VkAttachmentStoreOp>(key.stencil_store_op), layout, layout};
		depth_reference.attachment = num_attachments;
		depth_reference.layout = layout;
		depth_reference_ptr = &depth_reference;
		num_attachments++;
	}

	const VkSubpassDescriptionFlags subpass_flags =
		(key.color_feedback_loop &&
			g_vulkan_context->GetOptionalExtensions().vk_ext_rasterization_order_attachment_access) ?
			VK_SUBPASS_DESCRIPTION_RASTERIZATION_ORDER_ATTACHMENT_COLOR_ACCESS_BIT_EXT :
			0;
	const VkSubpassDescription subpass = {subpass_flags, VK_PIPELINE_BIND_POINT_GRAPHICS, input_reference_ptr ? 1u : 0u,
		input_reference_ptr ? input_reference_ptr : nullptr, color_reference_ptr ? 1u : 0u,
		color_reference_ptr ? color_reference_ptr : nullptr, nullptr, depth_reference_ptr, 0, nullptr};
	const VkRenderPassCreateInfo pass_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr, 0u, num_attachments,
		attachments.data(), 1u, &subpass, subpass_dependency_ptr ? 1u : 0u, subpass_dependency_ptr};

	VkRenderPass pass;
	const VkResult res = vkCreateRenderPass(m_device, &pass_info, nullptr, &pass);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateRenderPass failed: ");
		return VK_NULL_HANDLE;
	}

	m_render_pass_cache.emplace(key.key, pass);
	return pass;
}

void VKContext::DestroyRenderPassCache()
{
	for (auto& it : m_render_pass_cache)
		vkDestroyRenderPass(m_device, it.second, nullptr);

	m_render_pass_cache.clear();
}

static constexpr std::string_view SPIN_SHADER = R"(
#version 460 core

layout(std430, set=0, binding=0) buffer SpinBuffer { uint spin[]; };
layout(push_constant) uniform constants { uint cycles; };
layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

void main()
{
	uint value = spin[0];
	// The compiler doesn't know, but spin[0] == 0, so this loop won't actually go anywhere
	for (uint i = 0; i < cycles; i++)
		value = spin[value];
	// Store the result back to the buffer so the compiler can't optimize it away
	spin[0] = value;
}
)";

bool VKContext::InitSpinResources()
{
	if (!m_spinning_supported)
		return true;

	// TODO: Move to safe destroy functions, use scoped guard.

	VkResult res;
#define CHECKED_CREATE(create_fn, create_struct, output_struct) \
	do \
	{ \
		if ((res = create_fn(m_device, create_struct, nullptr, output_struct)) != VK_SUCCESS) \
		{ \
			LOG_VULKAN_ERROR(res, #create_fn " failed: "); \
			return false; \
		} \
	} while (0)

	VkDescriptorSetLayoutBinding set_layout_binding = {};
	set_layout_binding.binding = 0;
	set_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	set_layout_binding.descriptorCount = 1;
	set_layout_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	VkDescriptorSetLayoutCreateInfo desc_set_layout_create = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	desc_set_layout_create.bindingCount = 1;
	desc_set_layout_create.pBindings = &set_layout_binding;
	CHECKED_CREATE(vkCreateDescriptorSetLayout, &desc_set_layout_create, &m_spin_descriptor_set_layout);

	const VkPushConstantRange push_constant_range = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(u32)};
	VkPipelineLayoutCreateInfo pl_layout_create = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	pl_layout_create.setLayoutCount = 1;
	pl_layout_create.pSetLayouts = &m_spin_descriptor_set_layout;
	pl_layout_create.pushConstantRangeCount = 1;
	pl_layout_create.pPushConstantRanges = &push_constant_range;
	CHECKED_CREATE(vkCreatePipelineLayout, &pl_layout_create, &m_spin_pipeline_layout);

	VkShaderModule shader_module = g_vulkan_shader_cache->GetComputeShader(SPIN_SHADER);
	if (shader_module == VK_NULL_HANDLE)
		return false;

	Vulkan::SetObjectName(m_device, shader_module, "Spin Shader");

	VkComputePipelineCreateInfo pl_create = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
	pl_create.layout = m_spin_pipeline_layout;
	pl_create.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pl_create.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	pl_create.stage.pName = "main";
	pl_create.stage.module = shader_module;
	res = vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pl_create, nullptr, &m_spin_pipeline);
	vkDestroyShaderModule(m_device, shader_module, nullptr);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateComputePipelines failed: ");
		return false;
	}
	Vulkan::SetObjectName(m_device, m_spin_pipeline, "Spin Pipeline");

	VmaAllocationCreateInfo buf_vma_create = {};
	buf_vma_create.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	VkBufferCreateInfo buf_create = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	buf_create.size = 4;
	buf_create.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	if ((res = vmaCreateBuffer(m_allocator, &buf_create, &buf_vma_create, &m_spin_buffer, &m_spin_buffer_allocation,
			 nullptr)) != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vmaCreateBuffer failed: ");
		return false;
	}
	Vulkan::SetObjectName(m_device, m_spin_buffer, "Spin Buffer");

	VkDescriptorSetAllocateInfo desc_set_allocate = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	desc_set_allocate.descriptorPool = m_global_descriptor_pool;
	desc_set_allocate.descriptorSetCount = 1;
	desc_set_allocate.pSetLayouts = &m_spin_descriptor_set_layout;
	if ((res = vkAllocateDescriptorSets(m_device, &desc_set_allocate, &m_spin_descriptor_set)) != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkAllocateDescriptorSets failed: ");
		return false;
	}
	const VkDescriptorBufferInfo desc_buffer_info = {m_spin_buffer, 0, VK_WHOLE_SIZE};
	VkWriteDescriptorSet desc_set_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	desc_set_write.dstSet = m_spin_descriptor_set;
	desc_set_write.dstBinding = 0;
	desc_set_write.descriptorCount = 1;
	desc_set_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	desc_set_write.pBufferInfo = &desc_buffer_info;
	vkUpdateDescriptorSets(m_device, 1, &desc_set_write, 0, nullptr);

	for (SpinResources& resources : m_spin_resources)
	{
		u32 index = &resources - &m_spin_resources[0];
		VkCommandPoolCreateInfo pool_info = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
		pool_info.queueFamilyIndex = m_spin_queue_family_index;
		CHECKED_CREATE(vkCreateCommandPool, &pool_info, &resources.command_pool);
		Vulkan::SetObjectName(m_device, resources.command_pool, "Spin Command Pool %u", index);

		VkCommandBufferAllocateInfo buffer_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
		buffer_info.commandPool = resources.command_pool;
		buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		buffer_info.commandBufferCount = 1;
		res = vkAllocateCommandBuffers(m_device, &buffer_info, &resources.command_buffer);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkAllocateCommandBuffers failed: ");
			return false;
		}
		Vulkan::SetObjectName(m_device, resources.command_buffer, "Spin Command Buffer %u", index);

		VkFenceCreateInfo fence_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
		fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		CHECKED_CREATE(vkCreateFence, &fence_info, &resources.fence);
		Vulkan::SetObjectName(m_device, resources.fence, "Spin Fence %u", index);

		if (!m_spin_queue_is_graphics_queue)
		{
			VkSemaphoreCreateInfo sem_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
			CHECKED_CREATE(vkCreateSemaphore, &sem_info, &resources.semaphore);
			Vulkan::SetObjectName(m_device, resources.semaphore, "Draw to Spin Semaphore %u", index);
		}
	}

#undef CHECKED_CREATE
	return true;
}

void VKContext::DestroySpinResources()
{
#define CHECKED_DESTROY(destructor, obj) \
	do \
	{ \
		if (obj != VK_NULL_HANDLE) \
		{ \
			destructor(m_device, obj, nullptr); \
			obj = VK_NULL_HANDLE; \
		} \
	} while (0)

	if (m_spin_buffer)
	{
		vmaDestroyBuffer(m_allocator, m_spin_buffer, m_spin_buffer_allocation);
		m_spin_buffer = VK_NULL_HANDLE;
		m_spin_buffer_allocation = VK_NULL_HANDLE;
	}
	CHECKED_DESTROY(vkDestroyPipeline, m_spin_pipeline);
	CHECKED_DESTROY(vkDestroyPipelineLayout, m_spin_pipeline_layout);
	CHECKED_DESTROY(vkDestroyDescriptorSetLayout, m_spin_descriptor_set_layout);
	if (m_spin_descriptor_set != VK_NULL_HANDLE)
	{
		vkFreeDescriptorSets(m_device, m_global_descriptor_pool, 1, &m_spin_descriptor_set);
		m_spin_descriptor_set = VK_NULL_HANDLE;
	}
	for (SpinResources& resources : m_spin_resources)
	{
		CHECKED_DESTROY(vkDestroySemaphore, resources.semaphore);
		CHECKED_DESTROY(vkDestroyFence, resources.fence);
		if (resources.command_buffer != VK_NULL_HANDLE)
		{
			vkFreeCommandBuffers(m_device, resources.command_pool, 1, &resources.command_buffer);
			resources.command_buffer = VK_NULL_HANDLE;
		}
		CHECKED_DESTROY(vkDestroyCommandPool, resources.command_pool);
	}
#undef CHECKED_DESTROY
}

void VKContext::WaitForSpinCompletion(u32 index)
{
	SpinResources& resources = m_spin_resources[index];
	if (!resources.in_progress)
		return;

	const VkResult res = vkWaitForFences(m_device, 1, &resources.fence, VK_TRUE, UINT64_MAX);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkWaitForFences failed: ");
		m_last_submit_failed.store(true, std::memory_order_release);
		return;
	}
	SpinCommandCompleted(index);
}

void VKContext::SpinCommandCompleted(u32 index)
{
	SpinResources& resources = m_spin_resources[index];
	resources.in_progress = false;
	const u32 timestamp_base = (index + NUM_COMMAND_BUFFERS) * 2;
	std::array<u64, 2> timestamps;
	const VkResult res =
		vkGetQueryPoolResults(m_device, m_timestamp_query_pool, timestamp_base, static_cast<u32>(timestamps.size()),
			sizeof(timestamps), timestamps.data(), sizeof(u64), VK_QUERY_RESULT_64_BIT);
	if (res == VK_SUCCESS)
	{
		u64 begin, end;
		if (m_optional_extensions.vk_ext_calibrated_timestamps)
		{
			begin = timestamps[0] * m_spin_timestamp_scale + m_spin_timestamp_offset;
			end = timestamps[1] * m_spin_timestamp_scale + m_spin_timestamp_offset;
		}
		else
		{
			begin = timestamps[0] * m_spin_timestamp_scale;
			end = timestamps[1] * m_spin_timestamp_scale;
		}
		m_spin_manager.SpinCompleted(resources.cycles, begin, end);
	}
	else
	{
		LOG_VULKAN_ERROR(res, "vkGetQueryPoolResults failed: ");
	}
}

void VKContext::SubmitSpinCommand(u32 index, u32 cycles)
{
	SpinResources& resources = m_spin_resources[index];
	VkResult res;

	// Reset fence to unsignaled before starting.
	if ((res = vkResetFences(m_device, 1, &resources.fence)) != VK_SUCCESS)
		LOG_VULKAN_ERROR(res, "vkResetFences failed: ");

	// Reset command pools to beginning since we can re-use the memory now
	if ((res = vkResetCommandPool(m_device, resources.command_pool, 0)) != VK_SUCCESS)
		LOG_VULKAN_ERROR(res, "vkResetCommandPool failed: ");

	// Enable commands to be recorded to the two buffers again.
	VkCommandBufferBeginInfo begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if ((res = vkBeginCommandBuffer(resources.command_buffer, &begin_info)) != VK_SUCCESS)
		LOG_VULKAN_ERROR(res, "vkBeginCommandBuffer failed: ");

	if (!m_spin_buffer_initialized)
	{
		m_spin_buffer_initialized = true;
		vkCmdFillBuffer(resources.command_buffer, m_spin_buffer, 0, VK_WHOLE_SIZE, 0);
		VkBufferMemoryBarrier barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.srcQueueFamilyIndex = m_spin_queue_family_index;
		barrier.dstQueueFamilyIndex = m_spin_queue_family_index;
		barrier.buffer = m_spin_buffer;
		barrier.offset = 0;
		barrier.size = VK_WHOLE_SIZE;
		vkCmdPipelineBarrier(resources.command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
	}

	if (m_spin_queue_is_graphics_queue)
		vkCmdPipelineBarrier(resources.command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 0, nullptr);

	const u32 timestamp_base = (index + NUM_COMMAND_BUFFERS) * 2;
	vkCmdResetQueryPool(resources.command_buffer, m_timestamp_query_pool, timestamp_base, 2);
	vkCmdWriteTimestamp(
		resources.command_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_timestamp_query_pool, timestamp_base);
	vkCmdPushConstants(
		resources.command_buffer, m_spin_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(u32), &cycles);
	vkCmdBindPipeline(resources.command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_spin_pipeline);
	vkCmdBindDescriptorSets(resources.command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_spin_pipeline_layout, 0, 1,
		&m_spin_descriptor_set, 0, nullptr);
	vkCmdDispatch(resources.command_buffer, 1, 1, 1);
	vkCmdWriteTimestamp(
		resources.command_buffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, m_timestamp_query_pool, timestamp_base + 1);

	if ((res = vkEndCommandBuffer(resources.command_buffer)) != VK_SUCCESS)
		LOG_VULKAN_ERROR(res, "vkEndCommandBuffer failed: ");

	VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &resources.command_buffer;
	VkPipelineStageFlags sema_waits[] = {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};
	if (!m_spin_queue_is_graphics_queue)
	{
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &resources.semaphore;
		submit_info.pWaitDstStageMask = sema_waits;
	}
	vkQueueSubmit(m_spin_queue, 1, &submit_info, resources.fence);
	resources.in_progress = true;
	resources.cycles = cycles;
}

void VKContext::NotifyOfReadback()
{
	if (!m_spinning_supported)
		return;
	m_spin_timer = 30;
	m_spin_manager.ReadbackRequested();
}

void VKContext::CalibrateSpinTimestamp()
{
	if (!m_optional_extensions.vk_ext_calibrated_timestamps)
		return;
	VkCalibratedTimestampInfoEXT infos[2] = {
		{VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT, nullptr, VK_TIME_DOMAIN_DEVICE_EXT},
		{VK_STRUCTURE_TYPE_CALIBRATED_TIMESTAMP_INFO_EXT, nullptr, m_calibrated_timestamp_type},
	};
	u64 timestamps[2];
	u64 maxDeviation;
	constexpr u64 MAX_MAX_DEVIATION = 100000; // 100µs
	for (int i = 0; i < 4; i++) // 4 tries to get under MAX_MAX_DEVIATION
	{
		const VkResult res = vkGetCalibratedTimestampsEXT(m_device, std::size(infos), infos, timestamps, &maxDeviation);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkGetCalibratedTimestampsEXT failed: ");
			return;
		}
		if (maxDeviation < MAX_MAX_DEVIATION)
			break;
	}
	if (maxDeviation >= MAX_MAX_DEVIATION)
		Console.Warning("vkGetCalibratedTimestampsEXT returned high max deviation of %lluµs", maxDeviation / 1000);
	const double gpu_time = timestamps[0] * m_spin_timestamp_scale;
#ifdef _WIN32
	const double cpu_time = timestamps[1] * m_queryperfcounter_to_ns;
#else
	const double cpu_time = timestamps[1];
#endif
	m_spin_timestamp_offset = cpu_time - gpu_time;
}

u64 VKContext::GetCPUTimestamp()
{
#ifdef _WIN32
	LARGE_INTEGER value = {};
	QueryPerformanceCounter(&value);
	return static_cast<u64>(static_cast<double>(value.QuadPart) * m_queryperfcounter_to_ns);
#else
#ifdef CLOCK_MONOTONIC_RAW
	const bool use_raw = m_calibrated_timestamp_type == VK_TIME_DOMAIN_CLOCK_MONOTONIC_RAW_EXT;
	const clockid_t clock = use_raw ? CLOCK_MONOTONIC_RAW : CLOCK_MONOTONIC;
#else
	const clockid_t clock = CLOCK_MONOTONIC;
#endif
	timespec ts = {};
	clock_gettime(clock, &ts);
	return static_cast<u64>(ts.tv_sec) * 1000000000 + ts.tv_nsec;
#endif
}

bool VKContext::AllocatePreinitializedGPUBuffer(u32 size, VkBuffer* gpu_buffer, VmaAllocation* gpu_allocation,
	VkBufferUsageFlags gpu_usage, const std::function<void(void*)>& fill_callback)
{
	// Try to place the fixed index buffer in GPU local memory.
	// Use the staging buffer to copy into it.

	const VkBufferCreateInfo cpu_bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE};
	const VmaAllocationCreateInfo cpu_aci = {VMA_ALLOCATION_CREATE_MAPPED_BIT, VMA_MEMORY_USAGE_CPU_ONLY, 0, 0};
	VkBuffer cpu_buffer;
	VmaAllocation cpu_allocation;
	VmaAllocationInfo cpu_ai;
	VkResult res = vmaCreateBuffer(m_allocator, &cpu_bci, &cpu_aci, &cpu_buffer, &cpu_allocation, &cpu_ai);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vmaCreateBuffer() for CPU expand buffer failed: ");
		return false;
	}

	const VkBufferCreateInfo gpu_bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, size,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE};
	const VmaAllocationCreateInfo gpu_aci = {0, VMA_MEMORY_USAGE_GPU_ONLY, 0, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};
	VmaAllocationInfo ai;
	res = vmaCreateBuffer(m_allocator, &gpu_bci, &gpu_aci, gpu_buffer, gpu_allocation, &ai);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vmaCreateBuffer() for expand buffer failed: ");
		vmaDestroyBuffer(m_allocator, cpu_buffer, cpu_allocation);
		return false;
	}

	const VkBufferCopy buf_copy = {0u, 0u, size};
	fill_callback(cpu_ai.pMappedData);
	vmaFlushAllocation(m_allocator, cpu_allocation, 0, size);
	vkCmdCopyBuffer(GetCurrentInitCommandBuffer(), cpu_buffer, *gpu_buffer, 1, &buf_copy);
	DeferBufferDestruction(cpu_buffer, cpu_allocation);
	return true;
}
