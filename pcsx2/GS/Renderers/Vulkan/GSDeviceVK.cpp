/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

#include "GS/Renderers/Vulkan/GSDeviceVK.h"
#include "GS/Renderers/Vulkan/VKBuilders.h"
#include "GS/Renderers/Vulkan/VKShaderCache.h"
#include "GS/Renderers/Vulkan/VKSwapChain.h"
#include "GS/Renderers/Vulkan/VKUtil.h"
#include "GS/GS.h"
#include "GS/GSGL.h"
#include "GS/GSPerfMon.h"
#include "GS/GSUtil.h"

#include "Host.h"

#include "common/Align.h"
#include "common/Path.h"
#include "common/ScopedGuard.h"

#include "imgui.h"

#include <sstream>
#include <limits>

#ifdef ENABLE_OGL_DEBUG
static u32 s_debug_scope_depth = 0;
#endif

static bool IsDATMConvertShader(ShaderConvert i) { return (i == ShaderConvert::DATM_0 || i == ShaderConvert::DATM_1); }
static bool IsDATEModePrimIDInit(u32 flag) { return flag == 1 || flag == 2; }

static VkAttachmentLoadOp GetLoadOpForTexture(GSTextureVK* tex)
{
	if (!tex)
		return VK_ATTACHMENT_LOAD_OP_DONT_CARE;

	// clang-format off
	switch (tex->GetState())
	{
	case GSTextureVK::State::Cleared:       tex->SetState(GSTexture::State::Dirty); return VK_ATTACHMENT_LOAD_OP_CLEAR;
	case GSTextureVK::State::Invalidated:   tex->SetState(GSTexture::State::Dirty); return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	case GSTextureVK::State::Dirty:         return VK_ATTACHMENT_LOAD_OP_LOAD;
	default:                                return VK_ATTACHMENT_LOAD_OP_LOAD;
	}
	// clang-format on
}

static constexpr VkClearValue s_present_clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};

GSDeviceVK::GSDeviceVK()
{
#ifdef ENABLE_OGL_DEBUG
	s_debug_scope_depth = 0;
#endif

	std::memset(&m_pipeline_selector, 0, sizeof(m_pipeline_selector));
}

GSDeviceVK::~GSDeviceVK()
{
	pxAssert(!g_vulkan_context);
}

static void GPUListToAdapterNames(std::vector<std::string>* dest, VkInstance instance)
{
	VKContext::GPUList gpus = VKContext::EnumerateGPUs(instance);
	dest->clear();
	dest->reserve(gpus.size());
	for (auto& [gpu, name] : gpus)
		dest->push_back(std::move(name));
}

void GSDeviceVK::GetAdaptersAndFullscreenModes(
	std::vector<std::string>* adapters, std::vector<std::string>* fullscreen_modes)
{
	if (g_vulkan_context)
	{
		if (adapters)
			GPUListToAdapterNames(adapters, g_vulkan_context->GetVulkanInstance());
	}
	else
	{
		if (Vulkan::LoadVulkanLibrary())
		{
			ScopedGuard lib_guard([]() { Vulkan::UnloadVulkanLibrary(); });
			const VkInstance instance = VKContext::CreateVulkanInstance(WindowInfo(), false, false);
			if (instance != VK_NULL_HANDLE)
			{
				if (Vulkan::LoadVulkanInstanceFunctions(instance))
					GPUListToAdapterNames(adapters, instance);

				vkDestroyInstance(instance, nullptr);
			}
		}
	}
}

bool GSDeviceVK::IsSuitableDefaultRenderer()
{
	std::vector<std::string> adapters;
	GetAdaptersAndFullscreenModes(&adapters, nullptr);
	if (adapters.empty())
	{
		// No adapters, not gonna be able to use VK.
		return false;
	}

	// Check the first GPU, should be enough.
	const std::string& name = adapters.front();
	Console.WriteLn(fmt::format("Using Vulkan GPU '{}' for automatic renderer check.", name));

	// Any software rendering (LLVMpipe, SwiftShader).
	if (StringUtil::StartsWithNoCase(name, "llvmpipe") ||
		StringUtil::StartsWithNoCase(name, "SwiftShader"))
	{
		Console.WriteLn(Color_StrongOrange, "Not using Vulkan for software renderer.");
		return false;
	}

	// For Intel, OpenGL usually ends up faster on Linux, because of fbfetch.
	// Plus, the Ivy Bridge and Haswell drivers are incomplete.
	if (StringUtil::StartsWithNoCase(name, "Intel"))
	{
		Console.WriteLn(Color_StrongOrange, "Not using Vulkan for Intel GPU.");
		return false;
	}

	Console.WriteLn(Color_StrongGreen, "Allowing Vulkan as default renderer.");
	return true;
}

RenderAPI GSDeviceVK::GetRenderAPI() const
{
	return RenderAPI::Vulkan;
}

bool GSDeviceVK::HasSurface() const
{
	return static_cast<bool>(m_swap_chain);
}

bool GSDeviceVK::Create()
{
	if (!GSDevice::Create())
		return false;

	if (!CreateDeviceAndSwapChain())
		return false;

	if (!CheckFeatures())
	{
		Console.Error("Your GPU does not support the required Vulkan features.");
		return false;
	}

	{
		std::optional<std::string> shader = Host::ReadResourceFileToString("shaders/vulkan/tfx.glsl");
		if (!shader.has_value())
		{
			Host::ReportErrorAsync("GS", "Failed to read shaders/vulkan/tfx.glsl.");
			return false;
		}

		m_tfx_source = std::move(*shader);
	}

	if (!CreateNullTexture())
	{
		Host::ReportErrorAsync("GS", "Failed to create dummy texture");
		return false;
	}

	if (!CreatePipelineLayouts())
	{
		Host::ReportErrorAsync("GS", "Failed to create pipeline layouts");
		return false;
	}

	if (!CreateRenderPasses())
	{
		Host::ReportErrorAsync("GS", "Failed to create render passes");
		return false;
	}

	if (!CreateBuffers())
		return false;

	if (!CompileConvertPipelines() || !CompilePresentPipelines() ||
		!CompileInterlacePipelines() || !CompileMergePipelines() ||
		!CompilePostProcessingPipelines())
	{
		Host::ReportErrorAsync("GS", "Failed to compile utility pipelines");
		return false;
	}

	if (!CreatePersistentDescriptorSets())
	{
		Host::ReportErrorAsync("GS", "Failed to create persistent descriptor sets");
		return false;
	}

	CompileCASPipelines();

	if (!CompileImGuiPipeline())
		return false;

	InitializeState();
	return true;
}

void GSDeviceVK::Destroy()
{
	GSDevice::Destroy();

	if (g_vulkan_context)
	{
		EndRenderPass();
		ExecuteCommandBuffer(true);
		DestroyResources();

		g_vulkan_context->WaitForGPUIdle();
		m_swap_chain.reset();

		VKContext::Destroy();
	}
}

bool GSDeviceVK::UpdateWindow()
{
	DestroySurface();

	if (!AcquireWindow(false))
		return false;

	if (m_window_info.type == WindowInfo::Type::Surfaceless)
		return true;

	// make sure previous frames are presented
	ExecuteCommandBuffer(false);
	g_vulkan_context->WaitForGPUIdle();

	// recreate surface in existing swap chain if it already exists
	if (m_swap_chain)
	{
		if (m_swap_chain->RecreateSurface(m_window_info))
		{
			m_window_info = m_swap_chain->GetWindowInfo();
			return true;
		}

		m_swap_chain.reset();
	}

	VkSurfaceKHR surface = VKSwapChain::CreateVulkanSurface(
		g_vulkan_context->GetVulkanInstance(), g_vulkan_context->GetPhysicalDevice(), &m_window_info);
	if (surface == VK_NULL_HANDLE)
	{
		Console.Error("Failed to create new surface for swap chain");
		return false;
	}

	m_swap_chain = VKSwapChain::Create(m_window_info, surface, m_vsync_mode,
		Pcsx2Config::GSOptions::TriStateToOptionalBoolean(GSConfig.ExclusiveFullscreenControl));
	if (!m_swap_chain)
	{
		Console.Error("Failed to create swap chain");
		VKSwapChain::DestroyVulkanSurface(g_vulkan_context->GetVulkanInstance(), &m_window_info, surface);
		return false;
	}

	m_window_info = m_swap_chain->GetWindowInfo();
	RenderBlankFrame();
	return true;
}

void GSDeviceVK::ResizeWindow(s32 new_window_width, s32 new_window_height, float new_window_scale)
{
	if (m_swap_chain->GetWidth() == static_cast<u32>(new_window_width) &&
		m_swap_chain->GetHeight() == static_cast<u32>(new_window_height))
	{
		// skip unnecessary resizes
		m_window_info.surface_scale = new_window_scale;
		return;
	}

	// make sure previous frames are presented
	g_vulkan_context->WaitForGPUIdle();

	if (!m_swap_chain->ResizeSwapChain(new_window_width, new_window_height, new_window_scale))
	{
		// AcquireNextImage() will fail, and we'll recreate the surface.
		Console.Error("Failed to resize swap chain. Next present will fail.");
		return;
	}

	m_window_info = m_swap_chain->GetWindowInfo();
}

bool GSDeviceVK::SupportsExclusiveFullscreen() const
{
	return false;
}

void GSDeviceVK::DestroySurface()
{
	g_vulkan_context->WaitForGPUIdle();
	m_swap_chain.reset();
}

std::string GSDeviceVK::GetDriverInfo() const
{
	std::string ret;
	const u32 api_version = g_vulkan_context->GetDeviceProperties().apiVersion;
	const u32 driver_version = g_vulkan_context->GetDeviceProperties().driverVersion;
	if (g_vulkan_context->GetOptionalExtensions().vk_khr_driver_properties)
	{
		const VkPhysicalDeviceDriverProperties& props = g_vulkan_context->GetDeviceDriverProperties();
		ret = StringUtil::StdStringFromFormat(
			"Driver %u.%u.%u\nVulkan %u.%u.%u\nConformance Version %u.%u.%u.%u\n%s\n%s\n%s",
			VK_VERSION_MAJOR(driver_version), VK_VERSION_MINOR(driver_version), VK_VERSION_PATCH(driver_version),
			VK_API_VERSION_MAJOR(api_version), VK_API_VERSION_MINOR(api_version), VK_API_VERSION_PATCH(api_version),
			props.conformanceVersion.major, props.conformanceVersion.minor, props.conformanceVersion.subminor,
			props.conformanceVersion.patch, props.driverInfo, props.driverName,
			g_vulkan_context->GetDeviceProperties().deviceName);
	}
	else
	{
		ret = StringUtil::StdStringFromFormat("Driver %u.%u.%u\nVulkan %u.%u.%u\n%s", VK_VERSION_MAJOR(driver_version),
			VK_VERSION_MINOR(driver_version), VK_VERSION_PATCH(driver_version), VK_API_VERSION_MAJOR(api_version),
			VK_API_VERSION_MINOR(api_version), VK_API_VERSION_PATCH(api_version),
			g_vulkan_context->GetDeviceProperties().deviceName);
	}

	return ret;
}

void GSDeviceVK::SetVSync(VsyncMode mode)
{
	if (!m_swap_chain || m_vsync_mode == mode)
		return;

	// This swap chain should not be used by the current buffer, thus safe to destroy.
	g_vulkan_context->WaitForGPUIdle();
	if (!m_swap_chain->SetVSync(mode))
	{
		// Try switching back to the old mode..
		if (!m_swap_chain->SetVSync(m_vsync_mode))
		{
			pxFailRel("Failed to reset old vsync mode after failure");
			m_swap_chain.reset();
		}
	}

	m_vsync_mode = mode;
}

GSDevice::PresentResult GSDeviceVK::BeginPresent(bool frame_skip)
{
	EndRenderPass();

	if (frame_skip)
		return PresentResult::FrameSkipped;

	// If we're running surfaceless, kick the command buffer so we don't run out of descriptors.
	if (!m_swap_chain)
	{
		ExecuteCommandBuffer(false);
		return PresentResult::FrameSkipped;
	}

	// Previous frame needs to be presented before we can acquire the swap chain.
	g_vulkan_context->WaitForPresentComplete();

	// Check if the device was lost.
	if (g_vulkan_context->CheckLastSubmitFail())
		return PresentResult::DeviceLost;

	VkResult res = m_swap_chain->AcquireNextImage();
	if (res != VK_SUCCESS)
	{
		m_swap_chain->ReleaseCurrentImage();

		if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
		{
			ResizeWindow(0, 0, m_window_info.surface_scale);
			res = m_swap_chain->AcquireNextImage();
		}
		else if (res == VK_ERROR_SURFACE_LOST_KHR)
		{
			Console.Warning("Surface lost, attempting to recreate");
			if (!m_swap_chain->RecreateSurface(m_window_info))
			{
				Console.Error("Failed to recreate surface after loss");
				ExecuteCommandBuffer(false);
				return PresentResult::FrameSkipped;
			}

			res = m_swap_chain->AcquireNextImage();
		}

		// This can happen when multiple resize events happen in quick succession.
		// In this case, just wait until the next frame to try again.
		if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
		{
			// Still submit the command buffer, otherwise we'll end up with several frames waiting.
			LOG_VULKAN_ERROR(res, "vkAcquireNextImageKHR() failed: ");
			ExecuteCommandBuffer(false);
			return PresentResult::FrameSkipped;
		}
	}

	VkCommandBuffer cmdbuffer = g_vulkan_context->GetCurrentCommandBuffer();

	// Swap chain images start in undefined
	GSTextureVK* swap_chain_texture = m_swap_chain->GetCurrentTexture();
	swap_chain_texture->OverrideImageLayout(GSTextureVK::Layout::Undefined);
	swap_chain_texture->TransitionToLayout(cmdbuffer, GSTextureVK::Layout::ColorAttachment);

	const VkFramebuffer fb = swap_chain_texture->GetFramebuffer(false);
	if (fb == VK_NULL_HANDLE)
		return GSDevice::PresentResult::FrameSkipped;

	const VkRenderPassBeginInfo rp = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr,
		g_vulkan_context->GetRenderPass(swap_chain_texture->GetVkFormat(), VK_FORMAT_UNDEFINED,
			VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE),
		fb,
		{{0, 0}, {static_cast<u32>(swap_chain_texture->GetWidth()), static_cast<u32>(swap_chain_texture->GetHeight())}},
		1u, &s_present_clear_color};
	vkCmdBeginRenderPass(g_vulkan_context->GetCurrentCommandBuffer(), &rp, VK_SUBPASS_CONTENTS_INLINE);

	const VkViewport vp{0.0f, 0.0f, static_cast<float>(swap_chain_texture->GetWidth()),
		static_cast<float>(swap_chain_texture->GetHeight()), 0.0f, 1.0f};
	const VkRect2D scissor{
		{0, 0}, {static_cast<u32>(swap_chain_texture->GetWidth()), static_cast<u32>(swap_chain_texture->GetHeight())}};
	vkCmdSetViewport(g_vulkan_context->GetCurrentCommandBuffer(), 0, 1, &vp);
	vkCmdSetScissor(g_vulkan_context->GetCurrentCommandBuffer(), 0, 1, &scissor);
	return PresentResult::OK;
}

void GSDeviceVK::EndPresent()
{
	RenderImGui();

	VkCommandBuffer cmdbuffer = g_vulkan_context->GetCurrentCommandBuffer();
	vkCmdEndRenderPass(g_vulkan_context->GetCurrentCommandBuffer());
	m_swap_chain->GetCurrentTexture()->TransitionToLayout(cmdbuffer, GSTextureVK::Layout::PresentSrc);
	g_perfmon.Put(GSPerfMon::RenderPasses, 1);

	g_vulkan_context->SubmitCommandBuffer(m_swap_chain.get(), !m_swap_chain->IsPresentModeSynchronizing());
	g_vulkan_context->MoveToNextCommandBuffer();

	InvalidateCachedState();
}

bool GSDeviceVK::SetGPUTimingEnabled(bool enabled)
{
	return g_vulkan_context->SetEnableGPUTiming(enabled);
}

float GSDeviceVK::GetAndResetAccumulatedGPUTime()
{
	return g_vulkan_context->GetAndResetAccumulatedGPUTime();
}

#ifdef ENABLE_OGL_DEBUG
static std::array<float, 3> Palette(float phase, const std::array<float, 3>& a, const std::array<float, 3>& b,
	const std::array<float, 3>& c, const std::array<float, 3>& d)
{
	std::array<float, 3> result;
	result[0] = a[0] + b[0] * std::cos(6.28318f * (c[0] * phase + d[0]));
	result[1] = a[1] + b[1] * std::cos(6.28318f * (c[1] * phase + d[1]));
	result[2] = a[2] + b[2] * std::cos(6.28318f * (c[2] * phase + d[2]));
	return result;
}
#endif

void GSDeviceVK::PushDebugGroup(const char* fmt, ...)
{
#ifdef ENABLE_OGL_DEBUG
	if (!vkCmdBeginDebugUtilsLabelEXT || !GSConfig.UseDebugDevice)
		return;

	std::va_list ap;
	va_start(ap, fmt);
	const std::string buf(StringUtil::StdStringFromFormatV(fmt, ap));
	va_end(ap);

	const std::array<float, 3> color = Palette(
		++s_debug_scope_depth, {0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 0.5f}, {0.8f, 0.90f, 0.30f});

	const VkDebugUtilsLabelEXT label = {
		VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
		nullptr,
		buf.c_str(),
		{color[0], color[1], color[2], 1.0f},
	};
	vkCmdBeginDebugUtilsLabelEXT(g_vulkan_context->GetCurrentCommandBuffer(), &label);
#endif
}

void GSDeviceVK::PopDebugGroup()
{
#ifdef ENABLE_OGL_DEBUG
	if (!vkCmdEndDebugUtilsLabelEXT || !GSConfig.UseDebugDevice)
		return;

	s_debug_scope_depth = (s_debug_scope_depth == 0) ? 0 : (s_debug_scope_depth - 1u);

	vkCmdEndDebugUtilsLabelEXT(g_vulkan_context->GetCurrentCommandBuffer());
#endif
}

void GSDeviceVK::InsertDebugMessage(DebugMessageCategory category, const char* fmt, ...)
{
#ifdef ENABLE_OGL_DEBUG
	if (!vkCmdInsertDebugUtilsLabelEXT || !GSConfig.UseDebugDevice)
		return;

	std::va_list ap;
	va_start(ap, fmt);
	const std::string buf(StringUtil::StdStringFromFormatV(fmt, ap));
	va_end(ap);

	if (buf.empty())
		return;

	static constexpr float colors[][3] = {
		{0.1f, 0.1f, 0.0f}, // Cache
		{0.1f, 0.1f, 0.0f}, // Reg
		{0.5f, 0.0f, 0.5f}, // Debug
		{0.0f, 0.5f, 0.5f}, // Message
		{0.0f, 0.2f, 0.0f} // Performance
	};

	const VkDebugUtilsLabelEXT label = {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, nullptr, buf.c_str(),
		{colors[static_cast<int>(category)][0], colors[static_cast<int>(category)][1],
			colors[static_cast<int>(category)][2], 1.0f}};
	vkCmdInsertDebugUtilsLabelEXT(g_vulkan_context->GetCurrentCommandBuffer(), &label);
#endif
}

bool GSDeviceVK::CreateDeviceAndSwapChain()
{
	bool enable_debug_utils = GSConfig.UseDebugDevice;
	bool enable_validation_layer = GSConfig.UseDebugDevice;

	if (!Vulkan::LoadVulkanLibrary())
	{
		Host::ReportErrorAsync("Error", "Failed to load Vulkan library. Does your GPU and/or driver support Vulkan?");
		return false;
	}

	ScopedGuard library_cleanup(&Vulkan::UnloadVulkanLibrary);

	if (!AcquireWindow(true))
		return false;

	VkInstance instance = VKContext::CreateVulkanInstance(m_window_info, enable_debug_utils, enable_validation_layer);
	if (instance == VK_NULL_HANDLE)
	{
		if (enable_debug_utils || enable_validation_layer)
		{
			// Try again without the validation layer.
			enable_debug_utils = false;
			enable_validation_layer = false;
			instance = VKContext::CreateVulkanInstance(m_window_info, enable_debug_utils, enable_validation_layer);
			if (instance == VK_NULL_HANDLE)
			{
				Host::ReportErrorAsync(
					"Error", "Failed to create Vulkan instance. Does your GPU and/or driver support Vulkan?");
				return false;
			}

			Console.Error("Vulkan validation/debug layers requested but are unavailable. Creating non-debug device.");
		}
	}

	ScopedGuard instance_cleanup = [&instance]() { vkDestroyInstance(instance, nullptr); };
	if (!Vulkan::LoadVulkanInstanceFunctions(instance))
	{
		Console.Error("Failed to load Vulkan instance functions");
		return false;
	}

	VKContext::GPUList gpus = VKContext::EnumerateGPUs(instance);
	if (gpus.empty())
	{
		Host::ReportErrorAsync("Error", "No physical devices found. Does your GPU and/or driver support Vulkan?");
		return false;
	}

	u32 gpu_index = 0;
	if (!GSConfig.Adapter.empty())
	{
		for (; gpu_index < static_cast<u32>(gpus.size()); gpu_index++)
		{
			Console.WriteLn(fmt::format("GPU {}: {}", gpu_index, gpus[gpu_index].second));
			if (gpus[gpu_index].second == GSConfig.Adapter)
				break;
		}

		if (gpu_index == static_cast<u32>(gpus.size()))
		{
			Console.Warning(
				fmt::format("Requested GPU '{}' not found, using first ({})", GSConfig.Adapter, gpus[0].second));
			gpu_index = 0;
		}
	}
	else
	{
		Console.WriteLn(fmt::format("No GPU requested, using first ({})", gpus[0].second));
	}

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	ScopedGuard surface_cleanup = [&instance, &surface]() {
		if (surface != VK_NULL_HANDLE)
			vkDestroySurfaceKHR(instance, surface, nullptr);
	};
	if (m_window_info.type != WindowInfo::Type::Surfaceless)
	{
		surface = VKSwapChain::CreateVulkanSurface(instance, gpus[gpu_index].first, &m_window_info);
		if (surface == VK_NULL_HANDLE)
			return false;
	}

	if (!VKContext::Create(instance, surface, gpus[gpu_index].first, !GSConfig.DisableThreadedPresentation,
			enable_debug_utils, enable_validation_layer))
	{
		Console.Error("Failed to create Vulkan context");
		return false;
	}

	// NOTE: This is assigned afterwards, because some platforms can modify the window info (e.g. Metal).
	if (surface != VK_NULL_HANDLE)
	{
		m_swap_chain = VKSwapChain::Create(m_window_info, surface, m_vsync_mode,
			Pcsx2Config::GSOptions::TriStateToOptionalBoolean(GSConfig.ExclusiveFullscreenControl));
		if (!m_swap_chain)
		{
			Console.Error("Failed to create swap chain");
			return false;
		}

		m_window_info = m_swap_chain->GetWindowInfo();
	}

	surface_cleanup.Cancel();
	instance_cleanup.Cancel();
	library_cleanup.Cancel();

	// Render a frame as soon as possible to clear out whatever was previously being displayed.
	if (m_window_info.type != WindowInfo::Type::Surfaceless)
		RenderBlankFrame();

	return true;
}

bool GSDeviceVK::CheckFeatures()
{
	const VkPhysicalDeviceProperties& properties = g_vulkan_context->GetDeviceProperties();
	const VkPhysicalDeviceFeatures& features = g_vulkan_context->GetDeviceFeatures();
	const VkPhysicalDeviceLimits& limits = g_vulkan_context->GetDeviceLimits();
	const u32 vendorID = properties.vendorID;
	const bool isAMD = (vendorID == 0x1002 || vendorID == 0x1022);
	// const bool isNVIDIA = (vendorID == 0x10DE);

	m_features.framebuffer_fetch = g_vulkan_context->GetOptionalExtensions().vk_ext_rasterization_order_attachment_access && !GSConfig.DisableFramebufferFetch;
	m_features.texture_barrier = GSConfig.OverrideTextureBarriers != 0;
	m_features.broken_point_sampler = isAMD;

	// geometryShader is needed because gl_PrimitiveID is part of the Geometry SPIR-V Execution Model.
	m_features.primitive_id = g_vulkan_context->GetDeviceFeatures().geometryShader;

#ifdef __APPLE__
	// On Metal (MoltenVK), primid is sometimes available, but broken on some older GPUs and MacOS versions.
	// Officially, it's available on GPUs that support barycentric coordinates (Newer AMD and Apple)
	// Unofficially, it seems to work on older Intel GPUs (but breaks other things on newer Intel GPUs, see GSMTLDeviceInfo.mm for details)
	m_features.primitive_id &= g_vulkan_context->GetOptionalExtensions().vk_khr_fragment_shader_barycentric;
#endif

	m_features.prefer_new_textures = true;
	m_features.provoking_vertex_last = g_vulkan_context->GetOptionalExtensions().vk_ext_provoking_vertex;
	m_features.dual_source_blend = features.dualSrcBlend && !GSConfig.DisableDualSourceBlend;
	m_features.clip_control = true;
	m_features.vs_expand =
		!GSConfig.DisableVertexShaderExpand && g_vulkan_context->GetOptionalExtensions().vk_khr_shader_draw_parameters;

	if (!m_features.dual_source_blend)
		Console.Warning("Vulkan driver is missing dual-source blending. This will have an impact on performance.");

	if (!m_features.texture_barrier)
		Console.Warning("Texture buffers are disabled. This may break some graphical effects.");

	if (!g_vulkan_context->GetOptionalExtensions().vk_ext_line_rasterization)
		Console.WriteLn("VK_EXT_line_rasterization or the BRESENHAM mode is not supported, this may cause rendering inaccuracies.");

	// Test for D32S8 support.
	{
		VkFormatProperties props = {};
		vkGetPhysicalDeviceFormatProperties(g_vulkan_context->GetPhysicalDevice(), VK_FORMAT_D32_SFLOAT_S8_UINT, &props);
		m_features.stencil_buffer = ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0);
	}

	// Fbfetch is useless if we don't have barriers enabled.
	m_features.framebuffer_fetch &= m_features.texture_barrier;

	// Buggy drivers with broken barriers probably have no chance using GENERAL layout for depth either...
	m_features.test_and_sample_depth = m_features.texture_barrier;

	// Use D32F depth instead of D32S8 when we have framebuffer fetch.
	m_features.stencil_buffer &= !m_features.framebuffer_fetch;

	// whether we can do point/line expand depends on the range of the device
	const float f_upscale = static_cast<float>(GSConfig.UpscaleMultiplier);
	m_features.point_expand =
		(features.largePoints && limits.pointSizeRange[0] <= f_upscale && limits.pointSizeRange[1] >= f_upscale);
	m_features.line_expand =
		(features.wideLines && limits.lineWidthRange[0] <= f_upscale && limits.lineWidthRange[1] >= f_upscale);

	DevCon.WriteLn("Optional features:%s%s%s%s%s%s", m_features.primitive_id ? " primitive_id" : "",
		m_features.texture_barrier ? " texture_barrier" : "", m_features.framebuffer_fetch ? " framebuffer_fetch" : "",
		m_features.dual_source_blend ? " dual_source_blend" : "",
		m_features.provoking_vertex_last ? " provoking_vertex_last" : "", m_features.vs_expand ? " vs_expand" : "");
	DevCon.WriteLn("Using %s for point expansion and %s for line expansion.",
		m_features.point_expand ? "hardware" : "vertex expanding",
		m_features.line_expand ? "hardware" : "vertex expanding");

	// Check texture format support before we try to create them.
	for (u32 fmt = static_cast<u32>(GSTexture::Format::Color); fmt < static_cast<u32>(GSTexture::Format::PrimID); fmt++)
	{
		const VkFormat vkfmt = LookupNativeFormat(static_cast<GSTexture::Format>(fmt));
		const VkFormatFeatureFlags bits = (static_cast<GSTexture::Format>(fmt) == GSTexture::Format::DepthStencil) ?
		                                   (VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) :
		                                   (VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);

		VkFormatProperties props = {};
		vkGetPhysicalDeviceFormatProperties(g_vulkan_context->GetPhysicalDevice(), vkfmt, &props);
		if ((props.optimalTilingFeatures & bits) != bits)
		{
			Host::ReportFormattedErrorAsync("Vulkan Renderer Unavailable",
				"Required format %u is missing bits, you may need to update your driver. (vk:%u, has:0x%x, needs:0x%x)",
				fmt, static_cast<unsigned>(vkfmt), props.optimalTilingFeatures, bits);
			return false;
		}
	}

	m_features.dxt_textures = g_vulkan_context->GetDeviceFeatures().textureCompressionBC;
	m_features.bptc_textures = g_vulkan_context->GetDeviceFeatures().textureCompressionBC;

	if (!m_features.texture_barrier && !m_features.stencil_buffer)
	{
		Host::AddKeyedOSDMessage("GSDeviceVK_NoTextureBarrierOrStencilBuffer",
			"Stencil buffers and texture barriers are both unavailable, this will break some graphical effects.",
			Host::OSD_WARNING_DURATION);
	}

	return true;
}

void GSDeviceVK::DrawPrimitive()
{
	g_perfmon.Put(GSPerfMon::DrawCalls, 1);
	vkCmdDraw(g_vulkan_context->GetCurrentCommandBuffer(), m_vertex.count, 1, m_vertex.start, 0);
}

void GSDeviceVK::DrawIndexedPrimitive()
{
	g_perfmon.Put(GSPerfMon::DrawCalls, 1);
	vkCmdDrawIndexed(g_vulkan_context->GetCurrentCommandBuffer(), m_index.count, 1, m_index.start, m_vertex.start, 0);
}

void GSDeviceVK::DrawIndexedPrimitive(int offset, int count)
{
	ASSERT(offset + count <= (int)m_index.count);
	g_perfmon.Put(GSPerfMon::DrawCalls, 1);
	vkCmdDrawIndexed(g_vulkan_context->GetCurrentCommandBuffer(), count, 1, m_index.start + offset, m_vertex.start, 0);
}

void GSDeviceVK::ClearRenderTarget(GSTexture* t, const GSVector4& c)
{
	if (!t)
		return;

	if (m_current_render_target == t)
		EndRenderPass();

	static_cast<GSTextureVK*>(t)->SetClearColor(c);
}

void GSDeviceVK::ClearRenderTarget(GSTexture* t, u32 c) { ClearRenderTarget(t, GSVector4::rgba32(c) * (1.0f / 255)); }

void GSDeviceVK::InvalidateRenderTarget(GSTexture* t)
{
	if (!t)
		return;

	if (m_current_render_target == t || m_current_depth_target == t)
		EndRenderPass();

	t->SetState(GSTexture::State::Invalidated);
}

void GSDeviceVK::ClearDepth(GSTexture* t)
{
	if (!t)
		return;

	if (m_current_depth_target == t)
		EndRenderPass();

	static_cast<GSTextureVK*>(t)->SetClearDepth(0.0f);
}

VkFormat GSDeviceVK::LookupNativeFormat(GSTexture::Format format) const
{
	static constexpr std::array<VkFormat, static_cast<int>(GSTexture::Format::BC7) + 1> s_format_mapping = {{
		VK_FORMAT_UNDEFINED, // Invalid
		VK_FORMAT_R8G8B8A8_UNORM, // Color
		VK_FORMAT_R16G16B16A16_UNORM, // HDRColor
		VK_FORMAT_D32_SFLOAT_S8_UINT, // DepthStencil
		VK_FORMAT_R8_UNORM, // UNorm8
		VK_FORMAT_R16_UINT, // UInt16
		VK_FORMAT_R32_UINT, // UInt32
		VK_FORMAT_R32_SFLOAT, // Int32
		VK_FORMAT_BC1_RGBA_UNORM_BLOCK, // BC1
		VK_FORMAT_BC2_UNORM_BLOCK, // BC2
		VK_FORMAT_BC3_UNORM_BLOCK, // BC3
		VK_FORMAT_BC7_UNORM_BLOCK, // BC7
	}};

	return (format != GSTexture::Format::DepthStencil || m_features.stencil_buffer) ?
               s_format_mapping[static_cast<int>(format)] :
               VK_FORMAT_D32_SFLOAT;
}

GSTexture* GSDeviceVK::CreateSurface(GSTexture::Type type, int width, int height, int levels, GSTexture::Format format)
{
	const u32 clamped_width = static_cast<u32>(std::clamp<int>(width, 1, g_vulkan_context->GetMaxImageDimension2D()));
	const u32 clamped_height = static_cast<u32>(std::clamp<int>(height, 1, g_vulkan_context->GetMaxImageDimension2D()));

	std::unique_ptr<GSTexture> tex(GSTextureVK::Create(type, format, clamped_width, clamped_height, levels));
	if (!tex)
	{
		// We're probably out of vram, try flushing the command buffer to release pending textures.
		PurgePool();
		ExecuteCommandBufferAndRestartRenderPass(true, "Couldn't allocate texture.");
		tex = GSTextureVK::Create(type, format, clamped_width, clamped_height, levels);
	}

	return tex.release();
}

std::unique_ptr<GSDownloadTexture> GSDeviceVK::CreateDownloadTexture(u32 width, u32 height, GSTexture::Format format)
{
	return GSDownloadTextureVK::Create(width, height, format);
}

void GSDeviceVK::CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r, u32 destX, u32 destY)
{
	g_perfmon.Put(GSPerfMon::TextureCopies, 1);

	GSTextureVK* const sTexVK = static_cast<GSTextureVK*>(sTex);
	GSTextureVK* const dTexVK = static_cast<GSTextureVK*>(dTex);
	const GSVector4i dtex_rc(0, 0, dTexVK->GetWidth(), dTexVK->GetHeight());

	if (sTexVK->GetState() == GSTexture::State::Cleared)
	{
		// source is cleared. if destination is a render target, we can carry the clear forward
		if (dTexVK->IsRenderTargetOrDepthStencil())
		{
			if (dtex_rc.eq(r))
			{
				// pass it forward if we're clearing the whole thing
				if (sTexVK->IsDepthStencil())
					dTexVK->SetClearDepth(sTexVK->GetClearDepth());
				else
					dTexVK->SetClearColor(sTexVK->GetClearColor());

				return;
			}

			if (dTexVK->GetState() == GSTexture::State::Cleared)
			{
				// destination is cleared, if it's the same colour and rect, we can just avoid this entirely
				if (dTexVK->IsDepthStencil())
				{
					if (dTexVK->GetClearDepth() == sTexVK->GetClearDepth())
						return;
				}
				else
				{
					if ((dTexVK->GetClearColor() == (sTexVK->GetClearColor())).alltrue())
						return;
				}
			}

			// otherwise we need to do an attachment clear
			const bool depth = (dTexVK->GetType() == GSTexture::Type::DepthStencil);
			OMSetRenderTargets(depth ? nullptr : dTexVK, depth ? dTexVK : nullptr, dtex_rc);
			BeginRenderPassForStretchRect(dTexVK, dtex_rc, GSVector4i(destX, destY, destX + r.width(), destY + r.height()));

			// so use an attachment clear
			VkClearAttachment ca;
			ca.aspectMask = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
			GSVector4::store<false>(ca.clearValue.color.float32, sTexVK->GetClearColor());
			ca.clearValue.depthStencil.depth = sTexVK->GetClearDepth();
			ca.clearValue.depthStencil.stencil = 0;
			ca.colorAttachment = 0;

			const VkClearRect cr = { {{0, 0}, {static_cast<u32>(r.width()), static_cast<u32>(r.height())}}, 0u, 1u };
			vkCmdClearAttachments(g_vulkan_context->GetCurrentCommandBuffer(), 1, &ca, 1, &cr);
			return;
		}

		// commit the clear to the source first, then do normal copy
		sTexVK->CommitClear();
	}

	// if the destination has been cleared, and we're not overwriting the whole thing, commit the clear first
	// (the area outside of where we're copying to)
	if (dTexVK->GetState() == GSTexture::State::Cleared && !dtex_rc.eq(r))
		dTexVK->CommitClear();

	// *now* we can do a normal image copy.
	const VkImageAspectFlags src_aspect = (sTexVK->IsDepthStencil()) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	const VkImageAspectFlags dst_aspect = (dTexVK->IsDepthStencil()) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	const VkImageCopy ic = {{src_aspect, 0u, 0u, 1u}, {r.left, r.top, 0u}, {dst_aspect, 0u, 0u, 1u},
		{static_cast<s32>(destX), static_cast<s32>(destY), 0u},
		{static_cast<u32>(r.width()), static_cast<u32>(r.height()), 1u}};

	EndRenderPass();

	sTexVK->SetUsedThisCommandBuffer();
	dTexVK->SetUsedThisCommandBuffer();
	sTexVK->TransitionToLayout(
		(dTexVK == sTexVK) ? GSTextureVK::Layout::TransferSelf : GSTextureVK::Layout::TransferSrc);
	dTexVK->TransitionToLayout(
		(dTexVK == sTexVK) ? GSTextureVK::Layout::TransferSelf : GSTextureVK::Layout::TransferDst);

	vkCmdCopyImage(g_vulkan_context->GetCurrentCommandBuffer(), sTexVK->GetImage(),
		sTexVK->GetVkLayout(), dTexVK->GetImage(), dTexVK->GetVkLayout(), 1, &ic);

	dTexVK->SetState(GSTexture::State::Dirty);
}

void GSDeviceVK::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect,
	ShaderConvert shader /* = ShaderConvert::COPY */, bool linear /* = true */)
{
	pxAssert(HasDepthOutput(shader) == (dTex && dTex->GetType() == GSTexture::Type::DepthStencil));
	pxAssert(linear ? SupportsBilinear(shader) : SupportsNearest(shader));

	GL_INS("StretchRect(%d) {%d,%d} %dx%d -> {%d,%d) %dx%d", shader, int(sRect.left), int(sRect.top),
		int(sRect.right - sRect.left), int(sRect.bottom - sRect.top), int(dRect.left), int(dRect.top),
		int(dRect.right - dRect.left), int(dRect.bottom - dRect.top));

	DoStretchRect(static_cast<GSTextureVK*>(sTex), sRect, static_cast<GSTextureVK*>(dTex), dRect,
		dTex ? m_convert[static_cast<int>(shader)] : m_present[static_cast<int>(shader)], linear, true);
}

void GSDeviceVK::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, bool red,
	bool green, bool blue, bool alpha)
{
	GL_PUSH("ColorCopy Red:%d Green:%d Blue:%d Alpha:%d", red, green, blue, alpha);

	const u32 index = (red ? 1 : 0) | (green ? 2 : 0) | (blue ? 4 : 0) | (alpha ? 8 : 0);
	const bool allow_discard = (index == 0xf);
	DoStretchRect(static_cast<GSTextureVK*>(sTex), sRect, static_cast<GSTextureVK*>(dTex), dRect, m_color_copy[index],
		false, allow_discard);
}

void GSDeviceVK::PresentRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect,
	PresentShader shader, float shaderTime, bool linear)
{
	DisplayConstantBuffer cb;
	cb.SetSource(sRect, sTex->GetSize());
	cb.SetTarget(dRect, dTex ? dTex->GetSize() : GSVector2i(GetWindowWidth(), GetWindowHeight()));
	cb.SetTime(shaderTime);
	SetUtilityPushConstants(&cb, sizeof(cb));

	DoStretchRect(static_cast<GSTextureVK*>(sTex), sRect, static_cast<GSTextureVK*>(dTex), dRect,
		m_present[static_cast<int>(shader)], linear, true);
}

void GSDeviceVK::DrawMultiStretchRects(
	const MultiStretchRect* rects, u32 num_rects, GSTexture* dTex, ShaderConvert shader)
{
	GSTexture* last_tex = rects[0].src;
	bool last_linear = rects[0].linear;
	u8 last_wmask = rects[0].wmask.wrgba;

	u32 first = 0;
	u32 count = 1;

	// Make sure all textures are in shader read only layout, so we don't need to break
	// the render pass to transition.
	for (u32 i = 0; i < num_rects; i++)
	{
		GSTextureVK* const stex = static_cast<GSTextureVK*>(rects[i].src);
		stex->CommitClear();
		if (stex->GetLayout() != GSTextureVK::Layout::ShaderReadOnly)
		{
			EndRenderPass();
			stex->TransitionToLayout(GSTextureVK::Layout::ShaderReadOnly);
		}
	}

	for (u32 i = 1; i < num_rects; i++)
	{
		if (rects[i].src == last_tex && rects[i].linear == last_linear && rects[i].wmask.wrgba == last_wmask)
		{
			count++;
			continue;
		}

		DoMultiStretchRects(rects + first, count, static_cast<GSTextureVK*>(dTex), shader);
		last_tex = rects[i].src;
		last_linear = rects[i].linear;
		last_wmask = rects[i].wmask.wrgba;
		first += count;
		count = 1;
	}

	DoMultiStretchRects(rects + first, count, static_cast<GSTextureVK*>(dTex), shader);
}

void GSDeviceVK::DoMultiStretchRects(
	const MultiStretchRect* rects, u32 num_rects, GSTextureVK* dTex, ShaderConvert shader)
{
	// Set up vertices first.
	const u32 vertex_reserve_size = num_rects * 4 * sizeof(GSVertexPT1);
	const u32 index_reserve_size = num_rects * 6 * sizeof(u16);
	if (!m_vertex_stream_buffer.ReserveMemory(vertex_reserve_size, sizeof(GSVertexPT1)) ||
		!m_index_stream_buffer.ReserveMemory(index_reserve_size, sizeof(u16)))
	{
		ExecuteCommandBufferAndRestartRenderPass(false, "Uploading bytes to vertex buffer");
		if (!m_vertex_stream_buffer.ReserveMemory(vertex_reserve_size, sizeof(GSVertexPT1)) ||
			!m_index_stream_buffer.ReserveMemory(index_reserve_size, sizeof(u16)))
		{
			pxFailRel("Failed to reserve space for vertices");
		}
	}

	// Pain in the arse because the primitive topology for the pipelines is all triangle strips.
	// Don't use primitive restart here, it ends up slower on some drivers.
	const GSVector2 ds(static_cast<float>(dTex->GetWidth()), static_cast<float>(dTex->GetHeight()));
	GSVertexPT1* verts = reinterpret_cast<GSVertexPT1*>(m_vertex_stream_buffer.GetCurrentHostPointer());
	u16* idx = reinterpret_cast<u16*>(m_index_stream_buffer.GetCurrentHostPointer());
	u32 icount = 0;
	u32 vcount = 0;
	for (u32 i = 0; i < num_rects; i++)
	{
		const GSVector4& sRect = rects[i].src_rect;
		const GSVector4& dRect = rects[i].dst_rect;
		const float left = dRect.x * 2 / ds.x - 1.0f;
		const float top = 1.0f - dRect.y * 2 / ds.y;
		const float right = dRect.z * 2 / ds.x - 1.0f;
		const float bottom = 1.0f - dRect.w * 2 / ds.y;

		const u32 vstart = vcount;
		verts[vcount++] = {GSVector4(left, top, 0.5f, 1.0f), GSVector2(sRect.x, sRect.y)};
		verts[vcount++] = {GSVector4(right, top, 0.5f, 1.0f), GSVector2(sRect.z, sRect.y)};
		verts[vcount++] = {GSVector4(left, bottom, 0.5f, 1.0f), GSVector2(sRect.x, sRect.w)};
		verts[vcount++] = {GSVector4(right, bottom, 0.5f, 1.0f), GSVector2(sRect.z, sRect.w)};

		if (i > 0)
			idx[icount++] = vstart;

		idx[icount++] = vstart;
		idx[icount++] = vstart + 1;
		idx[icount++] = vstart + 2;
		idx[icount++] = vstart + 3;
		idx[icount++] = vstart + 3;
	};

	m_vertex.start = m_vertex_stream_buffer.GetCurrentOffset() / sizeof(GSVertexPT1);
	m_vertex.count = vcount;
	m_index.start = m_index_stream_buffer.GetCurrentOffset() / sizeof(u16);
	m_index.count = icount;
	m_vertex_stream_buffer.CommitMemory(vcount * sizeof(GSVertexPT1));
	m_index_stream_buffer.CommitMemory(icount * sizeof(u16));
	SetIndexBuffer(m_index_stream_buffer.GetBuffer(), 0, VK_INDEX_TYPE_UINT16);

	// Even though we're batching, a cmdbuffer submit could've messed this up.
	const GSVector4i rc(dTex->GetRect());
	OMSetRenderTargets(dTex->IsRenderTarget() ? dTex : nullptr, dTex->IsDepthStencil() ? dTex : nullptr, rc);
	if (!InRenderPass())
		BeginRenderPassForStretchRect(dTex, rc, rc, false);
	SetUtilityTexture(rects[0].src, rects[0].linear ? m_linear_sampler : m_point_sampler);

	pxAssert(shader == ShaderConvert::COPY || rects[0].wmask.wrgba == 0xf);
	SetPipeline((rects[0].wmask.wrgba != 0xf) ? m_color_copy[rects[0].wmask.wrgba] : m_convert[static_cast<int>(shader)]);

	if (ApplyUtilityState())
		DrawIndexedPrimitive();
}

void GSDeviceVK::BeginRenderPassForStretchRect(
	GSTextureVK* dTex, const GSVector4i& dtex_rc, const GSVector4i& dst_rc, bool allow_discard)
{
	pxAssert(dst_rc.x >= 0 && dst_rc.y >= 0 && dst_rc.z <= dTex->GetWidth() && dst_rc.w <= dTex->GetHeight());

	const VkAttachmentLoadOp load_op =
		(allow_discard && dst_rc.eq(dtex_rc)) ? VK_ATTACHMENT_LOAD_OP_DONT_CARE : GetLoadOpForTexture(dTex);
	dTex->SetState(GSTexture::State::Dirty);

	if (dTex->GetType() == GSTexture::Type::DepthStencil)
	{
		if (load_op == VK_ATTACHMENT_LOAD_OP_CLEAR)
			BeginClearRenderPass(m_utility_depth_render_pass_clear, dtex_rc, dTex->GetClearDepth(), 0);
		else
			BeginRenderPass((load_op == VK_ATTACHMENT_LOAD_OP_DONT_CARE) ? m_utility_depth_render_pass_discard :
                                                                           m_utility_depth_render_pass_load,
				dtex_rc);
	}
	else if (dTex->GetFormat() == GSTexture::Format::Color)
	{
		if (load_op == VK_ATTACHMENT_LOAD_OP_CLEAR)
			BeginClearRenderPass(m_utility_color_render_pass_clear, dtex_rc, dTex->GetClearColor());
		else
			BeginRenderPass((load_op == VK_ATTACHMENT_LOAD_OP_DONT_CARE) ? m_utility_color_render_pass_discard :
                                                                           m_utility_color_render_pass_load,
				dtex_rc);
	}
	else
	{
		// integer formats, etc
		const VkRenderPass rp = g_vulkan_context->GetRenderPass(dTex->GetVkFormat(), VK_FORMAT_UNDEFINED,
			load_op, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE);
		if (load_op == VK_ATTACHMENT_LOAD_OP_CLEAR)
		{
			BeginClearRenderPass(rp, dtex_rc, dTex->GetClearColor());
		}
		else
		{
			BeginRenderPass(rp, dtex_rc);
		}
	}
}

void GSDeviceVK::DoStretchRect(GSTextureVK* sTex, const GSVector4& sRect, GSTextureVK* dTex, const GSVector4& dRect,
	VkPipeline pipeline, bool linear, bool allow_discard)
{
	if (sTex->GetLayout() != GSTextureVK::Layout::ShaderReadOnly)
	{
		// can't transition in a render pass
		EndRenderPass();
		sTex->TransitionToLayout(GSTextureVK::Layout::ShaderReadOnly);
	}

	SetUtilityTexture(sTex, linear ? m_linear_sampler : m_point_sampler);
	SetPipeline(pipeline);

	const bool is_present = (!dTex);
	const bool depth = (dTex && dTex->GetType() == GSTexture::Type::DepthStencil);
	const GSVector2i size(
		is_present ? GSVector2i(GetWindowWidth(), GetWindowHeight()) : dTex->GetSize());
	const GSVector4i dtex_rc(0, 0, size.x, size.y);
	const GSVector4i dst_rc(GSVector4i(dRect).rintersect(dtex_rc));

	// switch rts (which might not end the render pass), so check the bounds
	if (!is_present)
	{
		OMSetRenderTargets(depth ? nullptr : dTex, depth ? dTex : nullptr, dst_rc);
		if (InRenderPass() && dTex->GetState() == GSTexture::State::Cleared)
			EndRenderPass();
	}
	else
	{
		// this is for presenting, we don't want to screw with the viewport/scissor set by display
		m_dirty_flags &= ~(DIRTY_FLAG_VIEWPORT | DIRTY_FLAG_SCISSOR);
	}

	if (!is_present && !InRenderPass())
		BeginRenderPassForStretchRect(dTex, dtex_rc, dst_rc, allow_discard);

	DrawStretchRect(sRect, dRect, size);
}

void GSDeviceVK::DrawStretchRect(const GSVector4& sRect, const GSVector4& dRect, const GSVector2i& ds)
{
	// ia
	const float left = dRect.x * 2 / ds.x - 1.0f;
	const float top = 1.0f - dRect.y * 2 / ds.y;
	const float right = dRect.z * 2 / ds.x - 1.0f;
	const float bottom = 1.0f - dRect.w * 2 / ds.y;

	GSVertexPT1 vertices[] = {
		{GSVector4(left, top, 0.5f, 1.0f), GSVector2(sRect.x, sRect.y)},
		{GSVector4(right, top, 0.5f, 1.0f), GSVector2(sRect.z, sRect.y)},
		{GSVector4(left, bottom, 0.5f, 1.0f), GSVector2(sRect.x, sRect.w)},
		{GSVector4(right, bottom, 0.5f, 1.0f), GSVector2(sRect.z, sRect.w)},
	};
	IASetVertexBuffer(vertices, sizeof(vertices[0]), std::size(vertices));

	if (ApplyUtilityState())
		DrawPrimitive();
}

void GSDeviceVK::BlitRect(GSTexture* sTex, const GSVector4i& sRect, u32 sLevel, GSTexture* dTex,
	const GSVector4i& dRect, u32 dLevel, bool linear)
{
	GSTextureVK* sTexVK = static_cast<GSTextureVK*>(sTex);
	GSTextureVK* dTexVK = static_cast<GSTextureVK*>(dTex);

	EndRenderPass();

	sTexVK->TransitionToLayout(GSTextureVK::Layout::TransferSrc);
	dTexVK->TransitionToLayout(GSTextureVK::Layout::TransferDst);

	// ensure we don't leave this bound later on
	if (m_tfx_textures[0] == sTexVK)
		PSSetShaderResource(0, nullptr, false);

	pxAssert(
		(sTexVK->GetType() == GSTexture::Type::DepthStencil) == (dTexVK->GetType() == GSTexture::Type::DepthStencil));
	const VkImageAspectFlags aspect =
		(sTexVK->GetType() == GSTexture::Type::DepthStencil) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	const VkImageBlit ib{{aspect, sLevel, 0u, 1u}, {{sRect.left, sRect.top, 0}, {sRect.right, sRect.bottom, 1}},
		{aspect, dLevel, 0u, 1u}, {{dRect.left, dRect.top, 0}, {dRect.right, dRect.bottom, 1}}};

	vkCmdBlitImage(g_vulkan_context->GetCurrentCommandBuffer(), sTexVK->GetImage(),
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dTexVK->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &ib,
		linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST);
}

void GSDeviceVK::UpdateCLUTTexture(GSTexture* sTex, float sScale, u32 offsetX, u32 offsetY, GSTexture* dTex, u32 dOffset, u32 dSize)
{
	// Super annoying, but apparently NVIDIA doesn't like floats/ints packed together in the same vec4?
	struct Uniforms
	{
		u32 offsetX, offsetY, dOffset, pad1;
		float scale;
		float pad2[3];
	};

	const Uniforms uniforms = {offsetX, offsetY, dOffset, 0, sScale, {}};
	SetUtilityPushConstants(&uniforms, sizeof(uniforms));

	const GSVector4 dRect(0, 0, dSize, 1);
	const ShaderConvert shader = (dSize == 16) ? ShaderConvert::CLUT_4 : ShaderConvert::CLUT_8;
	DoStretchRect(static_cast<GSTextureVK*>(sTex), GSVector4::zero(), static_cast<GSTextureVK*>(dTex), dRect,
		m_convert[static_cast<int>(shader)], false, true);
}

void GSDeviceVK::ConvertToIndexedTexture(GSTexture* sTex, float sScale, u32 offsetX, u32 offsetY, u32 SBW, u32 SPSM, GSTexture* dTex, u32 DBW, u32 DPSM)
{
	struct Uniforms
	{
		u32 SBW;
		u32 DBW;
		u32 pad1[2];
		float ScaleFactor;
		float pad2[3];
	};

	const Uniforms uniforms = {SBW, DBW, {}, sScale, {}};
	SetUtilityPushConstants(&uniforms, sizeof(uniforms));

	const ShaderConvert shader = ShaderConvert::RGBA_TO_8I;
	const GSVector4 dRect(0, 0, dTex->GetWidth(), dTex->GetHeight());
	DoStretchRect(static_cast<GSTextureVK*>(sTex), GSVector4::zero(), static_cast<GSTextureVK*>(dTex), dRect,
		m_convert[static_cast<int>(shader)], false, true);
}

void GSDeviceVK::DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect,
	const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, const GSVector4& c, const bool linear)
{
	GL_PUSH("DoMerge");

	const GSVector4 full_r(0.0f, 0.0f, 1.0f, 1.0f);
	const u32 yuv_constants[4] = {EXTBUF.EMODA, EXTBUF.EMODC};
	const bool feedback_write_2 = PMODE.EN2 && sTex[2] != nullptr && EXTBUF.FBIN == 1;
	const bool feedback_write_1 = PMODE.EN1 && sTex[2] != nullptr && EXTBUF.FBIN == 0;
	const bool feedback_write_2_but_blend_bg = feedback_write_2 && PMODE.SLBG == 1;
	const VkSampler& sampler = linear? m_linear_sampler : m_point_sampler;
	// Merge the 2 source textures (sTex[0],sTex[1]). Final results go to dTex. Feedback write will go to sTex[2].
	// If either 2nd output is disabled or SLBG is 1, a background color will be used.
	// Note: background color is also used when outside of the unit rectangle area
	EndRenderPass();

	// transition everything before starting the new render pass
	static_cast<GSTextureVK*>(dTex)->TransitionToLayout(GSTextureVK::Layout::ColorAttachment);
	if (sTex[0])
		static_cast<GSTextureVK*>(sTex[0])->TransitionToLayout(GSTextureVK::Layout::ShaderReadOnly);

	const GSVector2i dsize(dTex->GetSize());
	const GSVector4i darea(0, 0, dsize.x, dsize.y);
	bool dcleared = false;
	if (sTex[1] && (PMODE.SLBG == 0 || feedback_write_2_but_blend_bg))
	{
		// 2nd output is enabled and selected. Copy it to destination so we can blend it with 1st output
		// Note: value outside of dRect must contains the background color (c)
		if (sTex[1]->GetState() == GSTexture::State::Dirty)
		{
			static_cast<GSTextureVK*>(sTex[1])->TransitionToLayout(GSTextureVK::Layout::ShaderReadOnly);
			OMSetRenderTargets(dTex, nullptr, darea);
			SetUtilityTexture(sTex[1], sampler);
			BeginClearRenderPass(m_utility_color_render_pass_clear, darea, c);
			SetPipeline(m_convert[static_cast<int>(ShaderConvert::COPY)]);
			DrawStretchRect(sRect[1], PMODE.SLBG ? dRect[2] : dRect[1], dsize);
			dTex->SetState(GSTexture::State::Dirty);
			dcleared = true;
		}
	}

	// Upload constant to select YUV algo
	const GSVector2i fbsize(sTex[2] ? sTex[2]->GetSize() : GSVector2i(0, 0));
	const GSVector4i fbarea(0, 0, fbsize.x, fbsize.y);
	if (feedback_write_2)
	{
		EndRenderPass();
		OMSetRenderTargets(sTex[2], nullptr, fbarea);
		if (dcleared)
			SetUtilityTexture(dTex, sampler);
		// sTex[2] can be sTex[0], in which case it might be cleared (e.g. Xenosaga).
		BeginRenderPassForStretchRect(static_cast<GSTextureVK*>(sTex[2]), fbarea, GSVector4i(dRect[2]));
		if (dcleared)
		{
			SetPipeline(m_convert[static_cast<int>(ShaderConvert::YUV)]);
			SetUtilityPushConstants(yuv_constants, sizeof(yuv_constants));
			DrawStretchRect(full_r, dRect[2], fbsize);
		}
		EndRenderPass();

		if (sTex[0] == sTex[2])
		{
			// need a barrier here because of the render pass
			static_cast<GSTextureVK*>(sTex[2])->TransitionToLayout(GSTextureVK::Layout::ShaderReadOnly);
		}
	}

	// Restore background color to process the normal merge
	if (feedback_write_2_but_blend_bg || !dcleared)
	{
		EndRenderPass();
		OMSetRenderTargets(dTex, nullptr, darea);
		BeginClearRenderPass(m_utility_color_render_pass_clear, darea, c);
		dTex->SetState(GSTexture::State::Dirty);
	}
	else if (!InRenderPass())
	{
		OMSetRenderTargets(dTex, nullptr, darea);
		BeginRenderPass(m_utility_color_render_pass_load, darea);
	}

	if (sTex[0] && sTex[0]->GetState() == GSTexture::State::Dirty)
	{
		// 1st output is enabled. It must be blended
		SetUtilityTexture(sTex[0], sampler);
		SetPipeline(m_merge[PMODE.MMOD]);
		SetUtilityPushConstants(&c, sizeof(c));
		DrawStretchRect(sRect[0], dRect[0], dTex->GetSize());
	}

	if (feedback_write_1)
	{
		EndRenderPass();
		SetPipeline(m_convert[static_cast<int>(ShaderConvert::YUV)]);
		SetUtilityTexture(dTex, sampler);
		SetUtilityPushConstants(yuv_constants, sizeof(yuv_constants));
		OMSetRenderTargets(sTex[2], nullptr, fbarea);
		BeginRenderPass(m_utility_color_render_pass_load, fbarea);
		DrawStretchRect(full_r, dRect[2], dsize);
	}

	EndRenderPass();

	// this texture is going to get used as an input, so make sure we don't read undefined data
	static_cast<GSTextureVK*>(dTex)->CommitClear();
	static_cast<GSTextureVK*>(dTex)->TransitionToLayout(GSTextureVK::Layout::ShaderReadOnly);
}

void GSDeviceVK::DoInterlace(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ShaderInterlace shader, bool linear, const InterlaceConstantBuffer& cb)
{
	static_cast<GSTextureVK*>(dTex)->TransitionToLayout(GSTextureVK::Layout::ColorAttachment);

	const GSVector4i rc = GSVector4i(dRect);
	const GSVector4i dtex_rc = dTex->GetRect();
	const GSVector4i clamped_rc = rc.rintersect(dtex_rc);
	EndRenderPass();
	OMSetRenderTargets(dTex, nullptr, clamped_rc);
	SetUtilityTexture(sTex, linear ? m_linear_sampler : m_point_sampler);
	BeginRenderPassForStretchRect(static_cast<GSTextureVK*>(dTex), dTex->GetRect(), clamped_rc, false);
	SetPipeline(m_interlace[static_cast<int>(shader)]);
	SetUtilityPushConstants(&cb, sizeof(cb));
	DrawStretchRect(sRect, dRect, dTex->GetSize());
	EndRenderPass();

	static_cast<GSTextureVK*>(dTex)->TransitionToLayout(GSTextureVK::Layout::ShaderReadOnly);
}

void GSDeviceVK::DoShadeBoost(GSTexture* sTex, GSTexture* dTex, const float params[4])
{
	const GSVector4 sRect = GSVector4(0.0f, 0.0f, 1.0f, 1.0f);
	const GSVector4i dRect = dTex->GetRect();
	EndRenderPass();
	OMSetRenderTargets(dTex, nullptr, dRect);
	SetUtilityTexture(sTex, m_point_sampler);
	BeginRenderPass(m_utility_color_render_pass_discard, dRect);
	dTex->SetState(GSTexture::State::Dirty);
	SetPipeline(m_shadeboost_pipeline);
	SetUtilityPushConstants(params, sizeof(float) * 4);
	DrawStretchRect(sRect, GSVector4(dRect), dTex->GetSize());
	EndRenderPass();

	static_cast<GSTextureVK*>(dTex)->TransitionToLayout(GSTextureVK::Layout::ShaderReadOnly);
}

void GSDeviceVK::DoFXAA(GSTexture* sTex, GSTexture* dTex)
{
	const GSVector4 sRect = GSVector4(0.0f, 0.0f, 1.0f, 1.0f);
	const GSVector4i dRect = dTex->GetRect();
	EndRenderPass();
	OMSetRenderTargets(dTex, nullptr, dRect);
	SetUtilityTexture(sTex, m_linear_sampler);
	BeginRenderPass(m_utility_color_render_pass_discard, dRect);
	dTex->SetState(GSTexture::State::Dirty);
	SetPipeline(m_fxaa_pipeline);
	DrawStretchRect(sRect, GSVector4(dRect), dTex->GetSize());
	EndRenderPass();

	static_cast<GSTextureVK*>(dTex)->TransitionToLayout(GSTextureVK::Layout::ShaderReadOnly);
}

void GSDeviceVK::IASetVertexBuffer(const void* vertex, size_t stride, size_t count)
{
	const u32 size = static_cast<u32>(stride) * static_cast<u32>(count);
	if (!m_vertex_stream_buffer.ReserveMemory(size, static_cast<u32>(stride)))
	{
		ExecuteCommandBufferAndRestartRenderPass(false, "Uploading bytes to vertex buffer");
		if (!m_vertex_stream_buffer.ReserveMemory(size, static_cast<u32>(stride)))
			pxFailRel("Failed to reserve space for vertices");
	}

	m_vertex.start = m_vertex_stream_buffer.GetCurrentOffset() / stride;
	m_vertex.count = count;

	GSVector4i::storent(m_vertex_stream_buffer.GetCurrentHostPointer(), vertex, count * stride);
	m_vertex_stream_buffer.CommitMemory(size);
}

void GSDeviceVK::IASetIndexBuffer(const void* index, size_t count)
{
	const u32 size = sizeof(u16) * static_cast<u32>(count);
	if (!m_index_stream_buffer.ReserveMemory(size, sizeof(u16)))
	{
		ExecuteCommandBufferAndRestartRenderPass(false, "Uploading bytes to index buffer");
		if (!m_index_stream_buffer.ReserveMemory(size, sizeof(u16)))
			pxFailRel("Failed to reserve space for vertices");
	}

	m_index.start = m_index_stream_buffer.GetCurrentOffset() / sizeof(u16);
	m_index.count = count;

	std::memcpy(m_index_stream_buffer.GetCurrentHostPointer(), index, size);
	m_index_stream_buffer.CommitMemory(size);

	SetIndexBuffer(m_index_stream_buffer.GetBuffer(), 0, VK_INDEX_TYPE_UINT16);
}

void GSDeviceVK::OMSetRenderTargets(GSTexture* rt, GSTexture* ds, const GSVector4i& scissor, FeedbackLoopFlag feedback_loop)
{
	GSTextureVK* vkRt = static_cast<GSTextureVK*>(rt);
	GSTextureVK* vkDs = static_cast<GSTextureVK*>(ds);
	pxAssert(vkRt || vkDs);

	if (m_current_render_target != vkRt || m_current_depth_target != vkDs ||
		m_current_framebuffer_feedback_loop != feedback_loop)
	{
		// framebuffer change or feedback loop enabled/disabled
		EndRenderPass();

		if (vkRt)
		{
			m_current_framebuffer = vkRt->GetLinkedFramebuffer(vkDs, (feedback_loop & FeedbackLoopFlag_ReadAndWriteRT) != 0);
		}
		else
		{
			pxAssert(!(feedback_loop & FeedbackLoopFlag_ReadAndWriteRT));
			m_current_framebuffer = vkDs->GetLinkedFramebuffer(nullptr, false);
		}
	}

	m_current_render_target = vkRt;
	m_current_depth_target = vkDs;
	m_current_framebuffer_feedback_loop = feedback_loop;

	if (!InRenderPass())
	{
		if (vkRt)
		{
			vkRt->TransitionToLayout((feedback_loop & FeedbackLoopFlag_ReadAndWriteRT) ?
				GSTextureVK::Layout::FeedbackLoop :
										 GSTextureVK::Layout::ColorAttachment);
		}
		if (vkDs)
		{
			// need to update descriptors to reflect the new layout
			if ((feedback_loop & FeedbackLoopFlag_ReadDS) && vkDs->GetLayout() != GSTextureVK::Layout::FeedbackLoop)
				m_dirty_flags |= DIRTY_FLAG_TFX_SAMPLERS_DS;

			vkDs->TransitionToLayout((feedback_loop & FeedbackLoopFlag_ReadDS) ?
										 GSTextureVK::Layout::FeedbackLoop :
										 GSTextureVK::Layout::DepthStencilAttachment);
		}
	}

	// This is used to set/initialize the framebuffer for tfx rendering.
	const GSVector2i size = vkRt ? vkRt->GetSize() : vkDs->GetSize();
	const VkViewport vp{0.0f, 0.0f, static_cast<float>(size.x), static_cast<float>(size.y), 0.0f, 1.0f};

	SetViewport(vp);
	SetScissor(scissor);
}

VkSampler GSDeviceVK::GetSampler(GSHWDrawConfig::SamplerSelector ss)
{
	const auto it = m_samplers.find(ss.key);
	if (it != m_samplers.end())
		return it->second;

	const bool aniso = (ss.aniso && GSConfig.MaxAnisotropy > 1 && g_vulkan_context->GetDeviceFeatures().samplerAnisotropy);

	// See https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkSamplerCreateInfo.html#_description
	// for the reasoning behind 0.25f here.
	const VkSamplerCreateInfo ci = {
		VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, nullptr, 0,
		ss.IsMagFilterLinear() ? VK_FILTER_LINEAR : VK_FILTER_NEAREST, // min
		ss.IsMinFilterLinear() ? VK_FILTER_LINEAR : VK_FILTER_NEAREST, // mag
		ss.IsMipFilterLinear() ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST, // mip
		static_cast<VkSamplerAddressMode>(
			ss.tau ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE), // u
		static_cast<VkSamplerAddressMode>(
			ss.tav ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE), // v
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, // w
		0.0f, // lod bias
		static_cast<VkBool32>(aniso), // anisotropy enable
		aniso ? static_cast<float>(GSConfig.MaxAnisotropy) : 1.0f, // anisotropy
		VK_FALSE, // compare enable
		VK_COMPARE_OP_ALWAYS, // compare op
		0.0f, // min lod
		(ss.lodclamp || !ss.UseMipmapFiltering()) ? 0.25f : VK_LOD_CLAMP_NONE, // max lod
		VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK, // border
		VK_FALSE // unnormalized coordinates
	};
	VkSampler sampler = VK_NULL_HANDLE;
	VkResult res = vkCreateSampler(g_vulkan_context->GetDevice(), &ci, nullptr, &sampler);
	if (res != VK_SUCCESS)
		LOG_VULKAN_ERROR(res, "vkCreateSampler() failed: ");

	m_samplers.emplace(ss.key, sampler);
	return sampler;
}

void GSDeviceVK::ClearSamplerCache()
{
	for (const auto& it : m_samplers)
		g_vulkan_context->DeferSamplerDestruction(it.second);
	m_samplers.clear();
	m_point_sampler = GetSampler(GSHWDrawConfig::SamplerSelector::Point());
	m_linear_sampler = GetSampler(GSHWDrawConfig::SamplerSelector::Linear());
	m_utility_sampler = m_point_sampler;
	m_tfx_sampler = m_point_sampler;
}

static void AddMacro(std::stringstream& ss, const char* name, int value)
{
	ss << "#define " << name << " " << value << "\n";
}

static void AddShaderHeader(std::stringstream& ss)
{
	const GSDevice::FeatureSupport features(g_gs_device->Features());

	ss << "#version 460 core\n";
	ss << "#extension GL_EXT_samplerless_texture_functions : require\n";

	if (features.vs_expand)
		ss << "#extension GL_ARB_shader_draw_parameters : require\n";

	if (!features.texture_barrier)
		ss << "#define DISABLE_TEXTURE_BARRIER 1\n";
	if (!features.dual_source_blend)
		ss << "#define DISABLE_DUAL_SOURCE 1\n";
}

static void AddShaderStageMacro(std::stringstream& ss, bool vs, bool gs, bool fs)
{
	if (vs)
		ss << "#define VERTEX_SHADER 1\n";
	else if (gs)
		ss << "#define GEOMETRY_SHADER 1\n";
	else if (fs)
		ss << "#define FRAGMENT_SHADER 1\n";
}

static void AddUtilityVertexAttributes(Vulkan::GraphicsPipelineBuilder& gpb)
{
	gpb.AddVertexBuffer(0, sizeof(GSVertexPT1));
	gpb.AddVertexAttribute(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0);
	gpb.AddVertexAttribute(1, 0, VK_FORMAT_R32G32_SFLOAT, 16);
	gpb.SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
}

static void SetPipelineProvokingVertex(const GSDevice::FeatureSupport& features, Vulkan::GraphicsPipelineBuilder& gpb)
{
	// We enable provoking vertex here anyway, in case it doesn't support multiple modes in the same pass.
	// Normally we wouldn't enable it on the present/swap chain, but apparently the rule is it applies to the last
	// pipeline bound before the render pass begun, and in this case, we can't bind null.
	if (features.provoking_vertex_last)
		gpb.SetProvokingVertex(VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT);
}

VkShaderModule GSDeviceVK::GetUtilityVertexShader(const std::string& source, const char* replace_main = nullptr)
{
	std::stringstream ss;
	AddShaderHeader(ss);
	AddShaderStageMacro(ss, true, false, false);
	if (replace_main)
		ss << "#define " << replace_main << " main\n";
	ss << source;

	return g_vulkan_shader_cache->GetVertexShader(ss.str());
}

VkShaderModule GSDeviceVK::GetUtilityFragmentShader(const std::string& source, const char* replace_main = nullptr)
{
	std::stringstream ss;
	AddShaderHeader(ss);
	AddShaderStageMacro(ss, false, false, true);
	if (replace_main)
		ss << "#define " << replace_main << " main\n";
	ss << source;

	return g_vulkan_shader_cache->GetFragmentShader(ss.str());
}

bool GSDeviceVK::CreateNullTexture()
{
	m_null_texture = GSTextureVK::Create(GSTexture::Type::RenderTarget, GSTexture::Format::Color, 1, 1, 1);
	if (!m_null_texture)
		return false;

	const VkCommandBuffer cmdbuf = g_vulkan_context->GetCurrentCommandBuffer();
	const VkImageSubresourceRange srr{VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
	const VkClearColorValue ccv{};
	m_null_texture->TransitionToLayout(cmdbuf, GSTextureVK::Layout::ClearDst);
	vkCmdClearColorImage(cmdbuf, m_null_texture->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &ccv, 1, &srr);
	m_null_texture->TransitionToLayout(cmdbuf, GSTextureVK::Layout::General);
	Vulkan::SetObjectName(g_vulkan_context->GetDevice(), m_null_texture->GetImage(), "Null texture");
	Vulkan::SetObjectName(g_vulkan_context->GetDevice(), m_null_texture->GetView(), "Null texture view");

	return true;
}

bool GSDeviceVK::CreateBuffers()
{
	if (!m_vertex_stream_buffer.Create(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | (m_features.vs_expand ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : 0),
			VERTEX_BUFFER_SIZE))
	{
		Host::ReportErrorAsync("GS", "Failed to allocate vertex buffer");
		return false;
	}

	if (!m_index_stream_buffer.Create(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, INDEX_BUFFER_SIZE))
	{
		Host::ReportErrorAsync("GS", "Failed to allocate index buffer");
		return false;
	}

	if (!m_vertex_uniform_stream_buffer.Create(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VERTEX_UNIFORM_BUFFER_SIZE))
	{
		Host::ReportErrorAsync("GS", "Failed to allocate vertex uniform buffer");
		return false;
	}

	if (!m_fragment_uniform_stream_buffer.Create(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, FRAGMENT_UNIFORM_BUFFER_SIZE))
	{
		Host::ReportErrorAsync("GS", "Failed to allocate fragment uniform buffer");
		return false;
	}

	SetVertexBuffer(m_vertex_stream_buffer.GetBuffer(), 0);

	if (!g_vulkan_context->AllocatePreinitializedGPUBuffer(EXPAND_BUFFER_SIZE, &m_expand_index_buffer,
			&m_expand_index_buffer_allocation, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			&GSDevice::GenerateExpansionIndexBuffer))
	{
		Host::ReportErrorAsync("GS", "Failed to allocate expansion index buffer");
		return false;
	}

	return true;
}

bool GSDeviceVK::CreatePipelineLayouts()
{
	VkDevice dev = g_vulkan_context->GetDevice();
	Vulkan::DescriptorSetLayoutBuilder dslb;
	Vulkan::PipelineLayoutBuilder plb;

	//////////////////////////////////////////////////////////////////////////
	// Convert Pipeline Layout
	//////////////////////////////////////////////////////////////////////////

	dslb.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, NUM_CONVERT_SAMPLERS, VK_SHADER_STAGE_FRAGMENT_BIT);
	if ((m_utility_ds_layout = dslb.Create(dev)) == VK_NULL_HANDLE)
		return false;
	Vulkan::SetObjectName(dev, m_utility_ds_layout, "Convert descriptor layout");

	plb.AddPushConstants(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, CONVERT_PUSH_CONSTANTS_SIZE);
	plb.AddDescriptorSet(m_utility_ds_layout);
	if ((m_utility_pipeline_layout = plb.Create(dev)) == VK_NULL_HANDLE)
		return false;
	Vulkan::SetObjectName(dev, m_utility_ds_layout, "Convert pipeline layout");

	//////////////////////////////////////////////////////////////////////////
	// Draw/TFX Pipeline Layout
	//////////////////////////////////////////////////////////////////////////
	dslb.AddBinding(
		0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);
	dslb.AddBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	if (m_features.vs_expand)
		dslb.AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT);
	if ((m_tfx_ubo_ds_layout = dslb.Create(dev)) == VK_NULL_HANDLE)
		return false;
	Vulkan::SetObjectName(dev, m_tfx_ubo_ds_layout, "TFX UBO descriptor layout");
	dslb.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	dslb.AddBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	if ((m_tfx_sampler_ds_layout = dslb.Create(dev)) == VK_NULL_HANDLE)
		return false;
	Vulkan::SetObjectName(dev, m_tfx_sampler_ds_layout, "TFX sampler descriptor layout");
	dslb.AddBinding(0, m_features.texture_barrier ? VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	dslb.AddBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	if ((m_tfx_rt_texture_ds_layout = dslb.Create(dev)) == VK_NULL_HANDLE)
		return false;
	Vulkan::SetObjectName(dev, m_tfx_rt_texture_ds_layout, "TFX RT texture descriptor layout");

	plb.AddDescriptorSet(m_tfx_ubo_ds_layout);
	plb.AddDescriptorSet(m_tfx_sampler_ds_layout);
	plb.AddDescriptorSet(m_tfx_rt_texture_ds_layout);
	if ((m_tfx_pipeline_layout = plb.Create(dev)) == VK_NULL_HANDLE)
		return false;
	Vulkan::SetObjectName(dev, m_tfx_pipeline_layout, "TFX pipeline layout");
	return true;
}

bool GSDeviceVK::CreateRenderPasses()
{
#define GET(dest, rt, depth, fbl, dsp, opa, opb, opc) \
	do \
	{ \
		dest = g_vulkan_context->GetRenderPass( \
			(rt), (depth), ((rt) != VK_FORMAT_UNDEFINED) ? (opa) : VK_ATTACHMENT_LOAD_OP_DONT_CARE, /* color load */ \
			((rt) != VK_FORMAT_UNDEFINED) ? VK_ATTACHMENT_STORE_OP_STORE : \
                                            VK_ATTACHMENT_STORE_OP_DONT_CARE, /* color store */ \
			((depth) != VK_FORMAT_UNDEFINED) ? (opb) : VK_ATTACHMENT_LOAD_OP_DONT_CARE, /* depth load */ \
			((depth) != VK_FORMAT_UNDEFINED) ? VK_ATTACHMENT_STORE_OP_STORE : \
                                               VK_ATTACHMENT_STORE_OP_DONT_CARE, /* depth store */ \
			((depth) != VK_FORMAT_UNDEFINED) ? (opc) : VK_ATTACHMENT_LOAD_OP_DONT_CARE, /* stencil load */ \
			VK_ATTACHMENT_STORE_OP_DONT_CARE, /* stencil store */ \
			(fbl), /* feedback loop */ \
			(dsp) /* depth sampling */ \
		); \
		if (dest == VK_NULL_HANDLE) \
			return false; \
	} while (0)

	const VkFormat rt_format = LookupNativeFormat(GSTexture::Format::Color);
	const VkFormat hdr_rt_format = LookupNativeFormat(GSTexture::Format::HDRColor);
	const VkFormat depth_format = LookupNativeFormat(GSTexture::Format::DepthStencil);

	for (u32 rt = 0; rt < 2; rt++)
	{
		for (u32 ds = 0; ds < 2; ds++)
		{
			for (u32 hdr = 0; hdr < 2; hdr++)
			{
				for (u32 date = DATE_RENDER_PASS_NONE; date <= DATE_RENDER_PASS_STENCIL_ONE; date++)
				{
					for (u32 fbl = 0; fbl < 2; fbl++)
					{
						for (u32 dsp = 0; dsp < 2; dsp++)
						{
							for (u32 opa = VK_ATTACHMENT_LOAD_OP_LOAD; opa <= VK_ATTACHMENT_LOAD_OP_DONT_CARE; opa++)
							{
								for (u32 opb = VK_ATTACHMENT_LOAD_OP_LOAD; opb <= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
									 opb++)
								{
									const VkFormat rp_rt_format =
										(rt != 0) ? ((hdr != 0) ? hdr_rt_format : rt_format) : VK_FORMAT_UNDEFINED;
									const VkFormat rp_depth_format = (ds != 0) ? depth_format : VK_FORMAT_UNDEFINED;
									const VkAttachmentLoadOp opc =
										((date == DATE_RENDER_PASS_NONE || !m_features.stencil_buffer) ?
												VK_ATTACHMENT_LOAD_OP_DONT_CARE :
												(date == DATE_RENDER_PASS_STENCIL_ONE ? VK_ATTACHMENT_LOAD_OP_CLEAR :
																						VK_ATTACHMENT_LOAD_OP_LOAD));
									GET(m_tfx_render_pass[rt][ds][hdr][date][fbl][dsp][opa][opb], rp_rt_format,
										rp_depth_format, (fbl != 0), (dsp != 0), static_cast<VkAttachmentLoadOp>(opa),
										static_cast<VkAttachmentLoadOp>(opb), static_cast<VkAttachmentLoadOp>(opc));
								}
							}
						}
					}
				}
			}
		}
	}

	GET(m_utility_color_render_pass_load, rt_format, VK_FORMAT_UNDEFINED, false, false, VK_ATTACHMENT_LOAD_OP_LOAD,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
	GET(m_utility_color_render_pass_clear, rt_format, VK_FORMAT_UNDEFINED, false, false, VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
	GET(m_utility_color_render_pass_discard, rt_format, VK_FORMAT_UNDEFINED, false, false,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
	GET(m_utility_depth_render_pass_load, VK_FORMAT_UNDEFINED, depth_format, false, false,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
	GET(m_utility_depth_render_pass_clear, VK_FORMAT_UNDEFINED, depth_format, false, false,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
	GET(m_utility_depth_render_pass_discard, VK_FORMAT_UNDEFINED, depth_format, false, false,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE);

	m_date_setup_render_pass = g_vulkan_context->GetRenderPass(VK_FORMAT_UNDEFINED, depth_format,
		VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
		m_features.stencil_buffer ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		m_features.stencil_buffer ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE);
	if (m_date_setup_render_pass == VK_NULL_HANDLE)
		return false;

#undef GET

	return true;
}

bool GSDeviceVK::CompileConvertPipelines()
{
	std::optional<std::string> shader = Host::ReadResourceFileToString("shaders/vulkan/convert.glsl");
	if (!shader)
	{
		Host::ReportErrorAsync("GS", "Failed to read shaders/vulkan/convert.glsl.");
		return false;
	}

	VkShaderModule vs = GetUtilityVertexShader(*shader);
	if (vs == VK_NULL_HANDLE)
		return false;
	ScopedGuard vs_guard([&vs]() { Vulkan::SafeDestroyShaderModule(vs); });

	Vulkan::GraphicsPipelineBuilder gpb;
	SetPipelineProvokingVertex(m_features, gpb);
	AddUtilityVertexAttributes(gpb);
	gpb.SetPipelineLayout(m_utility_pipeline_layout);
	gpb.SetDynamicViewportAndScissorState();
	gpb.AddDynamicState(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
	gpb.SetNoCullRasterizationState();
	gpb.SetNoBlendingState();
	gpb.SetVertexShader(vs);

	for (ShaderConvert i = ShaderConvert::COPY; static_cast<int>(i) < static_cast<int>(ShaderConvert::Count);
		 i = static_cast<ShaderConvert>(static_cast<int>(i) + 1))
	{
		const bool depth = HasDepthOutput(i);
		const int index = static_cast<int>(i);

		VkRenderPass rp;
		switch (i)
		{
			case ShaderConvert::RGBA8_TO_16_BITS:
			case ShaderConvert::FLOAT32_TO_16_BITS:
			{
				rp = g_vulkan_context->GetRenderPass(LookupNativeFormat(GSTexture::Format::UInt16),
					VK_FORMAT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
			}
			break;
			case ShaderConvert::FLOAT32_TO_32_BITS:
			{
				rp = g_vulkan_context->GetRenderPass(LookupNativeFormat(GSTexture::Format::UInt32),
					VK_FORMAT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
			}
			break;
			case ShaderConvert::DATM_0:
			case ShaderConvert::DATM_1:
			{
				rp = m_date_setup_render_pass;
			}
			break;
			default:
			{
				rp = g_vulkan_context->GetRenderPass(
					LookupNativeFormat(depth ? GSTexture::Format::Invalid : GSTexture::Format::Color),
					LookupNativeFormat(
						depth ? GSTexture::Format::DepthStencil : GSTexture::Format::Invalid),
					VK_ATTACHMENT_LOAD_OP_DONT_CARE);
			}
			break;
		}
		if (!rp)
			return false;

		gpb.SetRenderPass(rp, 0);

		if (IsDATMConvertShader(i))
		{
			const VkStencilOpState sos = {
				VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 1u, 1u, 1u};
			gpb.SetDepthState(false, false, VK_COMPARE_OP_ALWAYS);
			gpb.SetStencilState(true, sos, sos);
		}
		else
		{
			gpb.SetDepthState(depth, depth, VK_COMPARE_OP_ALWAYS);
			gpb.SetNoStencilState();
		}

		VkShaderModule ps = GetUtilityFragmentShader(*shader, shaderName(i));
		if (ps == VK_NULL_HANDLE)
			return false;

		ScopedGuard ps_guard([&ps]() { Vulkan::SafeDestroyShaderModule(ps); });
		gpb.SetFragmentShader(ps);

		m_convert[index] =
			gpb.Create(g_vulkan_context->GetDevice(), g_vulkan_shader_cache->GetPipelineCache(true), false);
		if (!m_convert[index])
			return false;

		Vulkan::SetObjectName(g_vulkan_context->GetDevice(), m_convert[index], "Convert pipeline %d", i);

		if (i == ShaderConvert::COPY)
		{
			// compile color copy pipelines
			gpb.SetRenderPass(m_utility_color_render_pass_discard, 0);
			for (u32 i = 0; i < 16; i++)
			{
				pxAssert(!m_color_copy[i]);
				gpb.ClearBlendAttachments();
				gpb.SetBlendAttachment(0, false, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
					VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, static_cast<VkColorComponentFlags>(i));
				m_color_copy[i] =
					gpb.Create(g_vulkan_context->GetDevice(), g_vulkan_shader_cache->GetPipelineCache(true), false);
				if (!m_color_copy[i])
					return false;

				Vulkan::SetObjectName(g_vulkan_context->GetDevice(), m_color_copy[i],
					"Color copy pipeline (r=%u, g=%u, b=%u, a=%u)", i & 1u, (i >> 1) & 1u, (i >> 2) & 1u,
					(i >> 3) & 1u);
			}
		}
		else if (i == ShaderConvert::HDR_INIT || i == ShaderConvert::HDR_RESOLVE)
		{
			const bool is_setup = i == ShaderConvert::HDR_INIT;
			VkPipeline (&arr)[2][2] = *(is_setup ? &m_hdr_setup_pipelines : &m_hdr_finish_pipelines);
			for (u32 ds = 0; ds < 2; ds++)
			{
				for (u32 fbl = 0; fbl < 2; fbl++)
				{
					pxAssert(!arr[ds][fbl]);

					gpb.SetRenderPass(
						GetTFXRenderPass(true, ds != 0, is_setup, DATE_RENDER_PASS_NONE, fbl != 0, false,
							VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE),
						0);
					arr[ds][fbl] = gpb.Create(g_vulkan_context->GetDevice(), g_vulkan_shader_cache->GetPipelineCache(true), false);
					if (!arr[ds][fbl])
						return false;

					Vulkan::SetObjectName(g_vulkan_context->GetDevice(), arr[ds][fbl],
						"HDR %s/copy pipeline (ds=%u, fbl=%u)", is_setup ? "setup" : "finish", i, ds, fbl);
				}
			}
		}
	}

	// date image setup
	for (u32 ds = 0; ds < 2; ds++)
	{
		for (u32 clear = 0; clear < 2; clear++)
		{
			m_date_image_setup_render_passes[ds][clear] =
				g_vulkan_context->GetRenderPass(LookupNativeFormat(GSTexture::Format::PrimID),
					ds ? LookupNativeFormat(GSTexture::Format::DepthStencil) : VK_FORMAT_UNDEFINED,
					VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE,
					ds ? (clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD) : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
					ds ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE);
		}
	}

	for (u32 datm = 0; datm < 2; datm++)
	{
		VkShaderModule ps =
			GetUtilityFragmentShader(*shader, datm ? "ps_stencil_image_init_1" : "ps_stencil_image_init_0");
		if (ps == VK_NULL_HANDLE)
			return false;

		ScopedGuard ps_guard([&ps]() { Vulkan::SafeDestroyShaderModule(ps); });
		gpb.SetPipelineLayout(m_utility_pipeline_layout);
		gpb.SetFragmentShader(ps);
		gpb.SetNoDepthTestState();
		gpb.SetNoStencilState();
		gpb.ClearBlendAttachments();
		gpb.SetBlendAttachment(0, false, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
			VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_COLOR_COMPONENT_R_BIT);

		for (u32 ds = 0; ds < 2; ds++)
		{
			gpb.SetRenderPass(m_date_image_setup_render_passes[ds][0], 0);
			m_date_image_setup_pipelines[ds][datm] =
				gpb.Create(g_vulkan_context->GetDevice(), g_vulkan_shader_cache->GetPipelineCache(true), false);
			if (!m_date_image_setup_pipelines[ds][datm])
				return false;

			Vulkan::SetObjectName(g_vulkan_context->GetDevice(), m_date_image_setup_pipelines[ds][datm],
				"DATE image clear pipeline (ds=%u, datm=%u)", ds, datm);
		}
	}

	return true;
}

bool GSDeviceVK::CompilePresentPipelines()
{
	// we may not have a swap chain if running in headless mode.
	m_swap_chain_render_pass = g_vulkan_context->GetRenderPass(
		m_swap_chain ? m_swap_chain->GetTextureFormat() : VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_UNDEFINED);
	if (m_swap_chain_render_pass == VK_NULL_HANDLE)
		return false;

	std::optional<std::string> shader = Host::ReadResourceFileToString("shaders/vulkan/present.glsl");
	if (!shader)
	{
		Host::ReportErrorAsync("GS", "Failed to read shaders/vulkan/present.glsl.");
		return false;
	}

	VkShaderModule vs = GetUtilityVertexShader(*shader);
	if (vs == VK_NULL_HANDLE)
		return false;
	ScopedGuard vs_guard([&vs]() { Vulkan::SafeDestroyShaderModule(vs); });

	Vulkan::GraphicsPipelineBuilder gpb;
	SetPipelineProvokingVertex(m_features, gpb);
	AddUtilityVertexAttributes(gpb);
	gpb.SetPipelineLayout(m_utility_pipeline_layout);
	gpb.SetDynamicViewportAndScissorState();
	gpb.AddDynamicState(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
	gpb.SetNoCullRasterizationState();
	gpb.SetNoBlendingState();
	gpb.SetVertexShader(vs);
	gpb.SetDepthState(false, false, VK_COMPARE_OP_ALWAYS);
	gpb.SetNoStencilState();
	gpb.SetRenderPass(m_swap_chain_render_pass, 0);

	for (PresentShader i = PresentShader::COPY; static_cast<int>(i) < static_cast<int>(PresentShader::Count);
		i = static_cast<PresentShader>(static_cast<int>(i) + 1))
	{
		const int index = static_cast<int>(i);

		VkShaderModule ps = GetUtilityFragmentShader(*shader, shaderName(i));
		if (ps == VK_NULL_HANDLE)
			return false;

		ScopedGuard ps_guard([&ps]() { Vulkan::SafeDestroyShaderModule(ps); });
		gpb.SetFragmentShader(ps);

		m_present[index] =
			gpb.Create(g_vulkan_context->GetDevice(), g_vulkan_shader_cache->GetPipelineCache(true), false);
		if (!m_present[index])
			return false;

		Vulkan::SetObjectName(g_vulkan_context->GetDevice(), m_present[index], "Present pipeline %d", i);
	}

	return true;
}


bool GSDeviceVK::CompileInterlacePipelines()
{
	std::optional<std::string> shader = Host::ReadResourceFileToString("shaders/vulkan/interlace.glsl");
	if (!shader)
	{
		Host::ReportErrorAsync("GS", "Failed to read shaders/vulkan/interlace.glsl.");
		return false;
	}

	VkRenderPass rp = g_vulkan_context->GetRenderPass(
		LookupNativeFormat(GSTexture::Format::Color), VK_FORMAT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_LOAD);
	if (!rp)
		return false;

	VkShaderModule vs = GetUtilityVertexShader(*shader);
	if (vs == VK_NULL_HANDLE)
		return false;
	ScopedGuard vs_guard([&vs]() { Vulkan::SafeDestroyShaderModule(vs); });

	Vulkan::GraphicsPipelineBuilder gpb;
	SetPipelineProvokingVertex(m_features, gpb);
	AddUtilityVertexAttributes(gpb);
	gpb.SetPipelineLayout(m_utility_pipeline_layout);
	gpb.SetDynamicViewportAndScissorState();
	gpb.AddDynamicState(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
	gpb.SetNoCullRasterizationState();
	gpb.SetNoDepthTestState();
	gpb.SetNoBlendingState();
	gpb.SetRenderPass(rp, 0);
	gpb.SetVertexShader(vs);

	for (int i = 0; i < static_cast<int>(m_interlace.size()); i++)
	{
		VkShaderModule ps = GetUtilityFragmentShader(*shader, StringUtil::StdStringFromFormat("ps_main%d", i).c_str());
		if (ps == VK_NULL_HANDLE)
			return false;

		gpb.SetFragmentShader(ps);

		m_interlace[i] =
			gpb.Create(g_vulkan_context->GetDevice(), g_vulkan_shader_cache->GetPipelineCache(true), false);
		Vulkan::SafeDestroyShaderModule(ps);
		if (!m_interlace[i])
			return false;

		Vulkan::SetObjectName(g_vulkan_context->GetDevice(), m_convert[i], "Interlace pipeline %d", i);
	}

	return true;
}

bool GSDeviceVK::CompileMergePipelines()
{
	std::optional<std::string> shader = Host::ReadResourceFileToString("shaders/vulkan/merge.glsl");
	if (!shader)
	{
		Host::ReportErrorAsync("GS", "Failed to read shaders/vulkan/merge.glsl.");
		return false;
	}

	VkRenderPass rp = g_vulkan_context->GetRenderPass(
		LookupNativeFormat(GSTexture::Format::Color), VK_FORMAT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_LOAD);
	if (!rp)
		return false;

	VkShaderModule vs = GetUtilityVertexShader(*shader);
	if (vs == VK_NULL_HANDLE)
		return false;
	ScopedGuard vs_guard([&vs]() { Vulkan::SafeDestroyShaderModule(vs); });

	Vulkan::GraphicsPipelineBuilder gpb;
	SetPipelineProvokingVertex(m_features, gpb);
	AddUtilityVertexAttributes(gpb);
	gpb.SetPipelineLayout(m_utility_pipeline_layout);
	gpb.SetDynamicViewportAndScissorState();
	gpb.AddDynamicState(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
	gpb.SetNoCullRasterizationState();
	gpb.SetNoDepthTestState();
	gpb.SetRenderPass(rp, 0);
	gpb.SetVertexShader(vs);

	for (int i = 0; i < static_cast<int>(m_merge.size()); i++)
	{
		VkShaderModule ps = GetUtilityFragmentShader(*shader, StringUtil::StdStringFromFormat("ps_main%d", i).c_str());
		if (ps == VK_NULL_HANDLE)
			return false;

		gpb.SetFragmentShader(ps);
		gpb.SetBlendAttachment(0, true, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD);

		m_merge[i] = gpb.Create(g_vulkan_context->GetDevice(), g_vulkan_shader_cache->GetPipelineCache(true), false);
		Vulkan::SafeDestroyShaderModule(ps);
		if (!m_merge[i])
			return false;

		Vulkan::SetObjectName(g_vulkan_context->GetDevice(), m_convert[i], "Merge pipeline %d", i);
	}

	return true;
}

bool GSDeviceVK::CompilePostProcessingPipelines()
{
	VkRenderPass rp = g_vulkan_context->GetRenderPass(
		LookupNativeFormat(GSTexture::Format::Color), VK_FORMAT_UNDEFINED, VK_ATTACHMENT_LOAD_OP_LOAD);
	if (!rp)
		return false;

	Vulkan::GraphicsPipelineBuilder gpb;
	SetPipelineProvokingVertex(m_features, gpb);
	AddUtilityVertexAttributes(gpb);
	gpb.SetPipelineLayout(m_utility_pipeline_layout);
	gpb.SetDynamicViewportAndScissorState();
	gpb.AddDynamicState(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
	gpb.SetNoCullRasterizationState();
	gpb.SetNoDepthTestState();
	gpb.SetNoBlendingState();
	gpb.SetRenderPass(rp, 0);

	{
		std::optional<std::string> vshader = Host::ReadResourceFileToString("shaders/vulkan/convert.glsl");
		if (!vshader)
		{
			Host::ReportErrorAsync("GS", "Failed to read shaders/vulkan/convert.glsl.");
			return false;
		}

		std::optional<std::string> pshader = Host::ReadResourceFileToString("shaders/common/fxaa.fx");
		if (!pshader)
		{
			Host::ReportErrorAsync("GS", "Failed to read shaders/common/fxaa.fx.");
			return false;
		}

		const std::string psource = "#define FXAA_GLSL_VK 1\n" + *pshader;

		VkShaderModule vs = GetUtilityVertexShader(*vshader);
		VkShaderModule ps = GetUtilityFragmentShader(psource, "ps_main");
		ScopedGuard shader_guard([&vs, &ps]() {
			Vulkan::SafeDestroyShaderModule(vs);
			Vulkan::SafeDestroyShaderModule(ps);
		});
		if (vs == VK_NULL_HANDLE || ps == VK_NULL_HANDLE)
			return false;

		gpb.SetVertexShader(vs);
		gpb.SetFragmentShader(ps);

		m_fxaa_pipeline = gpb.Create(g_vulkan_context->GetDevice(), g_vulkan_shader_cache->GetPipelineCache(true), false);
		if (!m_fxaa_pipeline)
			return false;

		Vulkan::SetObjectName(g_vulkan_context->GetDevice(), m_fxaa_pipeline, "FXAA pipeline");
	}

	{
		std::optional<std::string> shader = Host::ReadResourceFileToString("shaders/vulkan/shadeboost.glsl");
		if (!shader)
		{
			Host::ReportErrorAsync("GS", "Failed to read shaders/vulkan/shadeboost.glsl.");
			return false;
		}

		VkShaderModule vs = GetUtilityVertexShader(*shader);
		VkShaderModule ps = GetUtilityFragmentShader(*shader);
		ScopedGuard shader_guard([&vs, &ps]() {
			Vulkan::SafeDestroyShaderModule(vs);
			Vulkan::SafeDestroyShaderModule(ps);
		});
		if (vs == VK_NULL_HANDLE || ps == VK_NULL_HANDLE)
			return false;

		gpb.SetVertexShader(vs);
		gpb.SetFragmentShader(ps);

		m_shadeboost_pipeline = gpb.Create(g_vulkan_context->GetDevice(), g_vulkan_shader_cache->GetPipelineCache(true), false);
		if (!m_shadeboost_pipeline)
			return false;

		Vulkan::SetObjectName(g_vulkan_context->GetDevice(), m_shadeboost_pipeline, "Shadeboost pipeline");
	}

	return true;
}

bool GSDeviceVK::CompileCASPipelines()
{
	VkDevice dev = g_vulkan_context->GetDevice();
	Vulkan::DescriptorSetLayoutBuilder dslb;
	Vulkan::PipelineLayoutBuilder plb;

	dslb.AddBinding(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	dslb.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT);
	if ((m_cas_ds_layout = dslb.Create(dev)) == VK_NULL_HANDLE)
		return false;
	Vulkan::SetObjectName(dev, m_cas_ds_layout, "CAS descriptor layout");

	plb.AddPushConstants(VK_SHADER_STAGE_COMPUTE_BIT, 0, NUM_CAS_CONSTANTS * sizeof(u32));
	plb.AddDescriptorSet(m_cas_ds_layout);
	if ((m_cas_pipeline_layout = plb.Create(dev)) == VK_NULL_HANDLE)
		return false;
	Vulkan::SetObjectName(dev, m_cas_pipeline_layout, "CAS pipeline layout");

	// we use specialization constants to avoid compiling it twice
	std::optional<std::string> cas_source(Host::ReadResourceFileToString("shaders/vulkan/cas.glsl"));
	if (!cas_source.has_value() || !GetCASShaderSource(&cas_source.value()))
		return false;

	VkShaderModule mod = g_vulkan_shader_cache->GetComputeShader(cas_source->c_str());
	ScopedGuard mod_guard = [&mod]() { Vulkan::SafeDestroyShaderModule(mod); };
	if (mod == VK_NULL_HANDLE)
		return false;

	for (u8 sharpen_only = 0; sharpen_only < 2; sharpen_only++)
	{
		Vulkan::ComputePipelineBuilder cpb;
		cpb.SetPipelineLayout(m_cas_pipeline_layout);
		cpb.SetShader(mod, "main");
		cpb.SetSpecializationBool(0, sharpen_only != 0);
		m_cas_pipelines[sharpen_only] = cpb.Create(dev, g_vulkan_shader_cache->GetPipelineCache(true), false);
		if (!m_cas_pipelines[sharpen_only])
			return false;
	}

	m_features.cas_sharpening = true;
	return true;
}

bool GSDeviceVK::CompileImGuiPipeline()
{
	const std::optional<std::string> glsl = Host::ReadResourceFileToString("shaders/vulkan/imgui.glsl");
	if (!glsl.has_value())
	{
		Console.Error("Failed to read imgui.glsl");
		return false;
	}
	
	VkShaderModule vs = GetUtilityVertexShader(glsl.value(), "vs_main");
	if (vs == VK_NULL_HANDLE)
	{
		Console.Error("Failed to compile ImGui vertex shader");
		return false;
	}
	ScopedGuard vs_guard([&vs]() { Vulkan::SafeDestroyShaderModule(vs); });

	VkShaderModule ps = GetUtilityFragmentShader(glsl.value(), "ps_main");
	if (ps == VK_NULL_HANDLE)
	{
		Console.Error("Failed to compile ImGui pixel shader");
		return false;
	}
	ScopedGuard ps_guard([&ps]() { Vulkan::SafeDestroyShaderModule(ps); });

	Vulkan::GraphicsPipelineBuilder gpb;
	SetPipelineProvokingVertex(m_features, gpb);
	gpb.SetPipelineLayout(m_utility_pipeline_layout);
	gpb.SetRenderPass(m_swap_chain_render_pass, 0);
	gpb.AddVertexBuffer(0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX);
	gpb.AddVertexAttribute(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos));
	gpb.AddVertexAttribute(1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv));
	gpb.AddVertexAttribute(2, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col));
	gpb.SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	gpb.SetVertexShader(vs);
	gpb.SetFragmentShader(ps);
	gpb.SetNoCullRasterizationState();
	gpb.SetNoDepthTestState();
	gpb.SetBlendAttachment(0, true, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD,
		VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD);
	gpb.SetDynamicViewportAndScissorState();
	gpb.AddDynamicState(VK_DYNAMIC_STATE_BLEND_CONSTANTS);

	m_imgui_pipeline = gpb.Create(g_vulkan_context->GetDevice(), g_vulkan_shader_cache->GetPipelineCache(), false);
	if (!m_imgui_pipeline)
	{
		Console.Error("Failed to compile ImGui pipeline");
		return false;
	}

	Vulkan::SetObjectName(g_vulkan_context->GetDevice(), m_imgui_pipeline, "ImGui pipeline");
	return true;
}

void GSDeviceVK::RenderImGui()
{
	ImGui::Render();
	const ImDrawData* draw_data = ImGui::GetDrawData();
	if (draw_data->CmdListsCount == 0)
		return;

	const float uniforms[2][2] = {{
									  2.0f / static_cast<float>(m_window_info.surface_width),
									  2.0f / static_cast<float>(m_window_info.surface_height),
								  },
		{
			-1.0f,
			-1.0f,
		}};

	SetUtilityPushConstants(uniforms, sizeof(uniforms));
	SetPipeline(m_imgui_pipeline);

	if (m_utility_sampler != m_linear_sampler)
	{
		m_utility_sampler = m_linear_sampler;
		m_dirty_flags |= DIRTY_FLAG_UTILITY_TEXTURE;
	}

	// this is for presenting, we don't want to screw with the viewport/scissor set by display
	m_dirty_flags &= ~(DIRTY_FLAG_VIEWPORT | DIRTY_FLAG_SCISSOR);

	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];

		u32 vertex_offset;
		{
			const u32 size = sizeof(ImDrawVert) * static_cast<u32>(cmd_list->VtxBuffer.Size);
			if (!m_vertex_stream_buffer.ReserveMemory(size, sizeof(ImDrawVert)))
			{
				Console.Warning("Skipping ImGui draw because of no vertex buffer space");
				return;
			}

			vertex_offset = m_vertex_stream_buffer.GetCurrentOffset() / sizeof(ImDrawVert);
			std::memcpy(m_vertex_stream_buffer.GetCurrentHostPointer(), cmd_list->VtxBuffer.Data, size);
			m_vertex_stream_buffer.CommitMemory(size);
		}

		static_assert(sizeof(ImDrawIdx) == sizeof(u16));
		IASetIndexBuffer(cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size);

		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			pxAssert(!pcmd->UserCallback);

			const GSVector4 clip = GSVector4::load<false>(&pcmd->ClipRect);
			if ((clip.zwzw() <= clip.xyxy()).mask() != 0)
				continue;

			SetScissor(GSVector4i(clip).max_i32(GSVector4i::zero()));

			// Since we don't have the GSTexture...
			GSTextureVK* tex = static_cast<GSTextureVK*>(pcmd->GetTexID());
			if (tex)
				SetUtilityTexture(tex, m_linear_sampler);

			if (ApplyUtilityState())
			{
				vkCmdDrawIndexed(g_vulkan_context->GetCurrentCommandBuffer(), pcmd->ElemCount, 1,
					m_index.start + pcmd->IdxOffset, vertex_offset + pcmd->VtxOffset, 0);
			}
		}

		g_perfmon.Put(GSPerfMon::DrawCalls, cmd_list->CmdBuffer.Size);
	}
}

void GSDeviceVK::RenderBlankFrame()
{
	VkResult res = m_swap_chain->AcquireNextImage();
	if (res != VK_SUCCESS)
	{
		Console.Error("Failed to acquire image for blank frame present");
		return;
	}

	VkCommandBuffer cmdbuffer = g_vulkan_context->GetCurrentCommandBuffer();
	GSTextureVK* sctex = m_swap_chain->GetCurrentTexture();
	sctex->TransitionToLayout(cmdbuffer, GSTextureVK::Layout::TransferDst);

	constexpr VkImageSubresourceRange srr = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
	vkCmdClearColorImage(cmdbuffer, sctex->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &s_present_clear_color.color, 1, &srr);

	m_swap_chain->GetCurrentTexture()->TransitionToLayout(cmdbuffer, GSTextureVK::Layout::PresentSrc);
	g_vulkan_context->SubmitCommandBuffer(m_swap_chain.get(), !m_swap_chain->IsPresentModeSynchronizing());
	g_vulkan_context->MoveToNextCommandBuffer();

	InvalidateCachedState();
}

bool GSDeviceVK::DoCAS(GSTexture* sTex, GSTexture* dTex, bool sharpen_only, const std::array<u32, NUM_CAS_CONSTANTS>& constants)
{
	EndRenderPass();

	VkDescriptorSet ds = g_vulkan_context->AllocateDescriptorSet(m_cas_ds_layout);
	if (ds == VK_NULL_HANDLE)
		return false;

	GSTextureVK* const sTexVK = static_cast<GSTextureVK*>(sTex);
	GSTextureVK* const dTexVK = static_cast<GSTextureVK*>(dTex);
	VkCommandBuffer cmdbuf = g_vulkan_context->GetCurrentCommandBuffer();

	sTexVK->TransitionToLayout(cmdbuf, GSTextureVK::Layout::ShaderReadOnly);
	dTexVK->TransitionToLayout(cmdbuf, GSTextureVK::Layout::ComputeReadWriteImage);

	// only happening once a frame, so the update isn't a huge deal.
	Vulkan::DescriptorSetUpdateBuilder dsub;
	dsub.AddImageDescriptorWrite(ds, 0, sTexVK->GetView(), sTexVK->GetVkLayout());
	dsub.AddStorageImageDescriptorWrite(ds, 1, dTexVK->GetView(), dTexVK->GetVkLayout());
	dsub.Update(g_vulkan_context->GetDevice(), false);

	// the actual meat and potatoes! only four commands.
	static const int threadGroupWorkRegionDim = 16;
	const int dispatchX = (dTex->GetWidth() + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
	const int dispatchY = (dTex->GetHeight() + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;

	vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_cas_pipeline_layout, 0, 1, &ds, 0, nullptr);
	vkCmdPushConstants(cmdbuf, m_cas_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, NUM_CAS_CONSTANTS * sizeof(u32), constants.data());
	vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_cas_pipelines[static_cast<u8>(sharpen_only)]);
	vkCmdDispatch(cmdbuf, dispatchX, dispatchY, 1);

	dTexVK->TransitionToLayout(GSTextureVK::Layout::ShaderReadOnly);

	// all done!
	return true;
}

void GSDeviceVK::DestroyResources()
{
	g_vulkan_context->ExecuteCommandBuffer(VKContext::WaitType::Sleep);
	if (m_tfx_ubo_descriptor_set != VK_NULL_HANDLE)
		g_vulkan_context->FreeGlobalDescriptorSet(m_tfx_ubo_descriptor_set);

	for (auto& it : m_tfx_pipelines)
		Vulkan::SafeDestroyPipeline(it.second);
	for (auto& it : m_tfx_fragment_shaders)
		Vulkan::SafeDestroyShaderModule(it.second);
	for (auto& it : m_tfx_vertex_shaders)
		Vulkan::SafeDestroyShaderModule(it.second);
	for (VkPipeline& it : m_interlace)
		Vulkan::SafeDestroyPipeline(it);
	for (VkPipeline& it : m_merge)
		Vulkan::SafeDestroyPipeline(it);
	for (VkPipeline& it : m_color_copy)
		Vulkan::SafeDestroyPipeline(it);
	for (VkPipeline& it : m_present)
		Vulkan::SafeDestroyPipeline(it);
	for (VkPipeline& it : m_convert)
		Vulkan::SafeDestroyPipeline(it);
	for (u32 ds = 0; ds < 2; ds++)
	{
		for (u32 fbl = 0; fbl < 2; fbl++)
		{
			Vulkan::SafeDestroyPipeline(m_hdr_setup_pipelines[ds][fbl]);
			Vulkan::SafeDestroyPipeline(m_hdr_finish_pipelines[ds][fbl]);
		}
	}
	for (u32 ds = 0; ds < 2; ds++)
	{
		for (u32 datm = 0; datm < 2; datm++)
		{
			Vulkan::SafeDestroyPipeline(m_date_image_setup_pipelines[ds][datm]);
		}
	}
	Vulkan::SafeDestroyPipeline(m_fxaa_pipeline);
	Vulkan::SafeDestroyPipeline(m_shadeboost_pipeline);

	for (VkPipeline& it : m_cas_pipelines)
		Vulkan::SafeDestroyPipeline(it);
	Vulkan::SafeDestroyPipelineLayout(m_cas_pipeline_layout);
	Vulkan::SafeDestroyDescriptorSetLayout(m_cas_ds_layout);
	Vulkan::SafeDestroyPipeline(m_imgui_pipeline);

	for (auto& it : m_samplers)
		Vulkan::SafeDestroySampler(it.second);

	m_linear_sampler = VK_NULL_HANDLE;
	m_point_sampler = VK_NULL_HANDLE;

	m_utility_color_render_pass_load = VK_NULL_HANDLE;
	m_utility_color_render_pass_clear = VK_NULL_HANDLE;
	m_utility_color_render_pass_discard = VK_NULL_HANDLE;
	m_utility_depth_render_pass_load = VK_NULL_HANDLE;
	m_utility_depth_render_pass_clear = VK_NULL_HANDLE;
	m_utility_depth_render_pass_discard = VK_NULL_HANDLE;
	m_date_setup_render_pass = VK_NULL_HANDLE;
	m_swap_chain_render_pass = VK_NULL_HANDLE;

	m_fragment_uniform_stream_buffer.Destroy(false);
	m_vertex_uniform_stream_buffer.Destroy(false);
	m_index_stream_buffer.Destroy(false);
	m_vertex_stream_buffer.Destroy(false);
	if (m_expand_index_buffer != VK_NULL_HANDLE)
	{
		vmaDestroyBuffer(g_vulkan_context->GetAllocator(), m_expand_index_buffer, m_expand_index_buffer_allocation);
		m_expand_index_buffer = VK_NULL_HANDLE;
		m_expand_index_buffer_allocation = VK_NULL_HANDLE;
	}

	Vulkan::SafeDestroyPipelineLayout(m_tfx_pipeline_layout);
	Vulkan::SafeDestroyDescriptorSetLayout(m_tfx_rt_texture_ds_layout);
	Vulkan::SafeDestroyDescriptorSetLayout(m_tfx_sampler_ds_layout);
	Vulkan::SafeDestroyDescriptorSetLayout(m_tfx_ubo_ds_layout);
	Vulkan::SafeDestroyPipelineLayout(m_utility_pipeline_layout);
	Vulkan::SafeDestroyDescriptorSetLayout(m_utility_ds_layout);

	if (m_null_texture)
	{
		m_null_texture->Destroy(false);
		m_null_texture.reset();
	}
}

VkShaderModule GSDeviceVK::GetTFXVertexShader(GSHWDrawConfig::VSSelector sel)
{
	const auto it = m_tfx_vertex_shaders.find(sel.key);
	if (it != m_tfx_vertex_shaders.end())
		return it->second;

	std::stringstream ss;
	AddShaderHeader(ss);
	AddShaderStageMacro(ss, true, false, false);
	AddMacro(ss, "VS_TME", sel.tme);
	AddMacro(ss, "VS_FST", sel.fst);
	AddMacro(ss, "VS_IIP", sel.iip);
	AddMacro(ss, "VS_POINT_SIZE", sel.point_size);
	AddMacro(ss, "VS_EXPAND", static_cast<int>(sel.expand));
	AddMacro(ss, "VS_PROVOKING_VERTEX_LAST", static_cast<int>(m_features.provoking_vertex_last));
	ss << m_tfx_source;

	VkShaderModule mod = g_vulkan_shader_cache->GetVertexShader(ss.str());
	if (mod)
		Vulkan::SetObjectName(g_vulkan_context->GetDevice(), mod, "TFX Vertex %08X", sel.key);

	m_tfx_vertex_shaders.emplace(sel.key, mod);
	return mod;
}

VkShaderModule GSDeviceVK::GetTFXFragmentShader(const GSHWDrawConfig::PSSelector& sel)
{
	const auto it = m_tfx_fragment_shaders.find(sel);
	if (it != m_tfx_fragment_shaders.end())
		return it->second;

	std::stringstream ss;
	AddShaderHeader(ss);
	AddShaderStageMacro(ss, false, false, true);
	AddMacro(ss, "PS_FST", sel.fst);
	AddMacro(ss, "PS_WMS", sel.wms);
	AddMacro(ss, "PS_WMT", sel.wmt);
	AddMacro(ss, "PS_ADJS", sel.adjs);
	AddMacro(ss, "PS_ADJT", sel.adjt);
	AddMacro(ss, "PS_AEM_FMT", sel.aem_fmt);
	AddMacro(ss, "PS_PAL_FMT", sel.pal_fmt);
	AddMacro(ss, "PS_DFMT", sel.dfmt);
	AddMacro(ss, "PS_DEPTH_FMT", sel.depth_fmt);
	AddMacro(ss, "PS_CHANNEL_FETCH", sel.channel);
	AddMacro(ss, "PS_URBAN_CHAOS_HLE", sel.urban_chaos_hle);
	AddMacro(ss, "PS_TALES_OF_ABYSS_HLE", sel.tales_of_abyss_hle);
	AddMacro(ss, "PS_AEM", sel.aem);
	AddMacro(ss, "PS_TFX", sel.tfx);
	AddMacro(ss, "PS_TCC", sel.tcc);
	AddMacro(ss, "PS_ATST", sel.atst);
	AddMacro(ss, "PS_FOG", sel.fog);
	AddMacro(ss, "PS_BLEND_HW", sel.blend_hw);
	AddMacro(ss, "PS_A_MASKED", sel.a_masked);
	AddMacro(ss, "PS_FBA", sel.fba);
	AddMacro(ss, "PS_LTF", sel.ltf);
	AddMacro(ss, "PS_AUTOMATIC_LOD", sel.automatic_lod);
	AddMacro(ss, "PS_MANUAL_LOD", sel.manual_lod);
	AddMacro(ss, "PS_COLCLIP", sel.colclip);
	AddMacro(ss, "PS_DATE", sel.date);
	AddMacro(ss, "PS_TCOFFSETHACK", sel.tcoffsethack);
	AddMacro(ss, "PS_POINT_SAMPLER", sel.point_sampler);
	AddMacro(ss, "PS_REGION_RECT", sel.region_rect);
	AddMacro(ss, "PS_BLEND_A", sel.blend_a);
	AddMacro(ss, "PS_BLEND_B", sel.blend_b);
	AddMacro(ss, "PS_BLEND_C", sel.blend_c);
	AddMacro(ss, "PS_BLEND_D", sel.blend_d);
	AddMacro(ss, "PS_BLEND_MIX", sel.blend_mix);
	AddMacro(ss, "PS_ROUND_INV", sel.round_inv);
	AddMacro(ss, "PS_FIXED_ONE_A", sel.fixed_one_a);
	AddMacro(ss, "PS_IIP", sel.iip);
	AddMacro(ss, "PS_SHUFFLE", sel.shuffle);
	AddMacro(ss, "PS_READ_BA", sel.read_ba);
	AddMacro(ss, "PS_READ16_SRC", sel.real16src);
	AddMacro(ss, "PS_WRITE_RG", sel.write_rg);
	AddMacro(ss, "PS_FBMASK", sel.fbmask);
	AddMacro(ss, "PS_HDR", sel.hdr);
	AddMacro(ss, "PS_DITHER", sel.dither);
	AddMacro(ss, "PS_ZCLAMP", sel.zclamp);
	AddMacro(ss, "PS_PABE", sel.pabe);
	AddMacro(ss, "PS_SCANMSK", sel.scanmsk);
	AddMacro(ss, "PS_TEX_IS_FB", sel.tex_is_fb);
	AddMacro(ss, "PS_NO_COLOR", sel.no_color);
	AddMacro(ss, "PS_NO_COLOR1", sel.no_color1);
	AddMacro(ss, "PS_NO_ABLEND", sel.no_ablend);
	AddMacro(ss, "PS_ONLY_ALPHA", sel.only_alpha);
	ss << m_tfx_source;

	VkShaderModule mod = g_vulkan_shader_cache->GetFragmentShader(ss.str());
	if (mod)
		Vulkan::SetObjectName(g_vulkan_context->GetDevice(), mod, "TFX Fragment %" PRIX64 "%08X", sel.key_hi, sel.key_lo);

	m_tfx_fragment_shaders.emplace(sel, mod);
	return mod;
}

VkPipeline GSDeviceVK::CreateTFXPipeline(const PipelineSelector& p)
{
	static constexpr std::array<VkPrimitiveTopology, 3> topology_lookup = {{
		VK_PRIMITIVE_TOPOLOGY_POINT_LIST, // Point
		VK_PRIMITIVE_TOPOLOGY_LINE_LIST, // Line
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, // Triangle
	}};

	GSHWDrawConfig::BlendState pbs{p.bs};
	GSHWDrawConfig::PSSelector pps{p.ps};
	if ((p.cms.wrgba & 0x7) == 0)
	{
		// disable blending when colours are masked
		pbs = {};
		pps.no_color1 = true;
	}

	VkShaderModule vs = GetTFXVertexShader(p.vs);
	VkShaderModule fs = GetTFXFragmentShader(pps);
	if (vs == VK_NULL_HANDLE || fs == VK_NULL_HANDLE)
		return VK_NULL_HANDLE;

	Vulkan::GraphicsPipelineBuilder gpb;
	SetPipelineProvokingVertex(m_features, gpb);

	// Common state
	gpb.SetPipelineLayout(m_tfx_pipeline_layout);
	if (IsDATEModePrimIDInit(p.ps.date))
	{
		// DATE image prepass
		gpb.SetRenderPass(m_date_image_setup_render_passes[p.ds][0], 0);
	}
	else
	{
		gpb.SetRenderPass(
			GetTFXRenderPass(p.rt, p.ds, p.ps.hdr, p.dss.date ? DATE_RENDER_PASS_STENCIL : DATE_RENDER_PASS_NONE,
				p.IsRTFeedbackLoop(), p.IsTestingAndSamplingDepth(),
				p.rt ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				p.ds ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE),
			0);
	}
	gpb.SetPrimitiveTopology(topology_lookup[p.topology]);
	gpb.SetRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	if (p.line_width)
		gpb.SetLineWidth(static_cast<float>(GSConfig.UpscaleMultiplier));
	if (p.topology == static_cast<u8>(GSHWDrawConfig::Topology::Line) && g_vulkan_context->GetOptionalExtensions().vk_ext_line_rasterization)
		gpb.SetLineRasterizationMode(VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT);
	gpb.SetDynamicViewportAndScissorState();
	gpb.AddDynamicState(VK_DYNAMIC_STATE_BLEND_CONSTANTS);

	// Shaders
	gpb.SetVertexShader(vs);
	gpb.SetFragmentShader(fs);

	// IA
	if (p.vs.expand == GSHWDrawConfig::VSExpand::None)
	{
		gpb.AddVertexBuffer(0, sizeof(GSVertex));
		gpb.AddVertexAttribute(0, 0, VK_FORMAT_R32G32_SFLOAT, 0); // ST
		gpb.AddVertexAttribute(1, 0, VK_FORMAT_R8G8B8A8_UINT, 8); // RGBA
		gpb.AddVertexAttribute(2, 0, VK_FORMAT_R32_SFLOAT, 12); // Q
		gpb.AddVertexAttribute(3, 0, VK_FORMAT_R16G16_UINT, 16); // XY
		gpb.AddVertexAttribute(4, 0, VK_FORMAT_R32_UINT, 20); // Z
		gpb.AddVertexAttribute(5, 0, VK_FORMAT_R16G16_UINT, 24); // UV
		gpb.AddVertexAttribute(6, 0, VK_FORMAT_R8G8B8A8_UNORM, 28); // FOG
	}

	// DepthStencil
	static const VkCompareOp ztst[] = {
		VK_COMPARE_OP_NEVER, VK_COMPARE_OP_ALWAYS, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_GREATER};
	gpb.SetDepthState((p.dss.ztst != ZTST_ALWAYS || p.dss.zwe), p.dss.zwe, ztst[p.dss.ztst]);
	if (p.dss.date)
	{
		const VkStencilOpState sos{VK_STENCIL_OP_KEEP, p.dss.date_one ? VK_STENCIL_OP_ZERO : VK_STENCIL_OP_KEEP,
			VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL, 1u, 1u, 1u};
		gpb.SetStencilState(true, sos, sos);
	}

	// Blending
	if (IsDATEModePrimIDInit(p.ps.date))
	{
		// image DATE prepass
		gpb.SetBlendAttachment(0, true, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_MIN, VK_BLEND_FACTOR_ONE,
			VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_COLOR_COMPONENT_R_BIT);
	}
	else if (pbs.enable)
	{
		// clang-format off
		static constexpr std::array<VkBlendFactor, 16> vk_blend_factors = { {
			VK_BLEND_FACTOR_SRC_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR, VK_BLEND_FACTOR_DST_COLOR, VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
			VK_BLEND_FACTOR_SRC1_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, VK_BLEND_FACTOR_SRC1_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,
			VK_BLEND_FACTOR_CONSTANT_COLOR, VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO
		}};
		static constexpr std::array<VkBlendOp, 3> vk_blend_ops = {{
				VK_BLEND_OP_ADD, VK_BLEND_OP_SUBTRACT, VK_BLEND_OP_REVERSE_SUBTRACT
		}};
		// clang-format on

		gpb.SetBlendAttachment(0, true, vk_blend_factors[pbs.src_factor], vk_blend_factors[pbs.dst_factor],
			vk_blend_ops[pbs.op], VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, p.cms.wrgba);
	}
	else
	{
		gpb.SetBlendAttachment(0, false, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
			VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, p.cms.wrgba);
	}

	// Tests have shown that it's faster to just enable rast order on the entire pass, rather than alternating
	// between turning it on and off for different draws, and adding the required barrier between non-rast-order
	// and rast-order draws.
	if (m_features.framebuffer_fetch && p.IsRTFeedbackLoop())
		gpb.AddBlendFlags(VK_PIPELINE_COLOR_BLEND_STATE_CREATE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_BIT_EXT);

	VkPipeline pipeline = gpb.Create(g_vulkan_context->GetDevice(), g_vulkan_shader_cache->GetPipelineCache(true));
	if (pipeline)
	{
		Vulkan::SetObjectName(
			g_vulkan_context->GetDevice(), pipeline, "TFX Pipeline %08X/%" PRIX64 "%08X", p.vs.key, p.ps.key_hi, p.ps.key_lo);
	}

	return pipeline;
}

VkPipeline GSDeviceVK::GetTFXPipeline(const PipelineSelector& p)
{
	const auto it = m_tfx_pipelines.find(p);
	if (it != m_tfx_pipelines.end())
		return it->second;

	VkPipeline pipeline = CreateTFXPipeline(p);
	m_tfx_pipelines.emplace(p, pipeline);
	return pipeline;
}

bool GSDeviceVK::BindDrawPipeline(const PipelineSelector& p)
{
	VkPipeline pipeline = GetTFXPipeline(p);
	if (pipeline == VK_NULL_HANDLE)
		return false;

	SetPipeline(pipeline);

	return ApplyTFXState();
}

void GSDeviceVK::InitializeState()
{
	m_vertex_buffer = m_vertex_stream_buffer.GetBuffer();
	m_vertex_buffer_offset = 0;
	m_index_buffer = m_index_stream_buffer.GetBuffer();
	m_index_buffer_offset = 0;
	m_index_type = VK_INDEX_TYPE_UINT16;
	m_current_framebuffer = VK_NULL_HANDLE;
	m_current_render_pass = VK_NULL_HANDLE;

	for (u32 i = 0; i < NUM_TFX_TEXTURES; i++)
		m_tfx_textures[i] = m_null_texture.get();

	m_utility_texture = m_null_texture.get();

	m_point_sampler = GetSampler(GSHWDrawConfig::SamplerSelector::Point());
	if (m_point_sampler)
		Vulkan::SetObjectName(g_vulkan_context->GetDevice(), m_point_sampler, "Point sampler");
	m_linear_sampler = GetSampler(GSHWDrawConfig::SamplerSelector::Linear());
	if (m_linear_sampler)
		Vulkan::SetObjectName(g_vulkan_context->GetDevice(), m_point_sampler, "Linear sampler");

	m_tfx_sampler_sel = GSHWDrawConfig::SamplerSelector::Point().key;
	m_tfx_sampler = m_point_sampler;

	InvalidateCachedState();
}

bool GSDeviceVK::CreatePersistentDescriptorSets()
{
	const VkDevice dev = g_vulkan_context->GetDevice();
	Vulkan::DescriptorSetUpdateBuilder dsub;

	// Allocate UBO descriptor sets for TFX.
	m_tfx_ubo_descriptor_set = g_vulkan_context->AllocatePersistentDescriptorSet(m_tfx_ubo_ds_layout);
	if (m_tfx_ubo_descriptor_set == VK_NULL_HANDLE)
		return false;
	dsub.AddBufferDescriptorWrite(m_tfx_ubo_descriptor_set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
		m_vertex_uniform_stream_buffer.GetBuffer(), 0, sizeof(GSHWDrawConfig::VSConstantBuffer));
	dsub.AddBufferDescriptorWrite(m_tfx_ubo_descriptor_set, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
		m_fragment_uniform_stream_buffer.GetBuffer(), 0, sizeof(GSHWDrawConfig::PSConstantBuffer));
	if (m_features.vs_expand)
	{
		dsub.AddBufferDescriptorWrite(m_tfx_ubo_descriptor_set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			m_vertex_stream_buffer.GetBuffer(), 0, VERTEX_BUFFER_SIZE);
	}
	dsub.Update(dev);
	Vulkan::SetObjectName(dev, m_tfx_ubo_descriptor_set, "Persistent TFX UBO set");
	return true;
}

static VKContext::WaitType GetWaitType(bool wait, bool spin)
{
	if (!wait)
		return VKContext::WaitType::None;
	if (spin)
		return VKContext::WaitType::Spin;
	else
		return VKContext::WaitType::Sleep;
}

void GSDeviceVK::ExecuteCommandBuffer(bool wait_for_completion)
{
	EndRenderPass();
	g_vulkan_context->ExecuteCommandBuffer(GetWaitType(wait_for_completion, GSConfig.HWSpinCPUForReadbacks));
	InvalidateCachedState();
}

void GSDeviceVK::ExecuteCommandBuffer(bool wait_for_completion, const char* reason, ...)
{
	std::va_list ap;
	va_start(ap, reason);
	const std::string reason_str(StringUtil::StdStringFromFormatV(reason, ap));
	va_end(ap);

	Console.Warning("Vulkan: Executing command buffer due to '%s'", reason_str.c_str());
	ExecuteCommandBuffer(wait_for_completion);
}

void GSDeviceVK::ExecuteCommandBufferAndRestartRenderPass(bool wait_for_completion, const char* reason)
{
	Console.Warning("Vulkan: Executing command buffer due to '%s'", reason);

	const VkRenderPass render_pass = m_current_render_pass;
	const GSVector4i render_pass_area = m_current_render_pass_area;
	const GSVector4i scissor = m_scissor;
	GSTexture* const current_rt = m_current_render_target;
	GSTexture* const current_ds = m_current_depth_target;
	const FeedbackLoopFlag current_feedback_loop = m_current_framebuffer_feedback_loop;

	EndRenderPass();
	g_vulkan_context->ExecuteCommandBuffer(GetWaitType(wait_for_completion, GSConfig.HWSpinCPUForReadbacks));
	InvalidateCachedState();

	if (render_pass != VK_NULL_HANDLE)
	{
		// rebind framebuffer
		OMSetRenderTargets(current_rt, current_ds, scissor, current_feedback_loop);

		// restart render pass
		BeginRenderPass(g_vulkan_context->GetRenderPassForRestarting(render_pass), render_pass_area);
	}
}

void GSDeviceVK::ExecuteCommandBufferForReadback()
{
	ExecuteCommandBuffer(true);
	if (GSConfig.HWSpinGPUForReadbacks)
	{
		g_vulkan_context->NotifyOfReadback();
		if (!g_vulkan_context->GetOptionalExtensions().vk_ext_calibrated_timestamps && !m_warned_slow_spin)
		{
			m_warned_slow_spin = true;
			Host::AddKeyedOSDMessage("GSDeviceVK_NoCalibratedTimestamps",
				"Spin GPU During Readbacks is enabled, but calibrated timestamps are unavailable.  This might be really slow.",
				Host::OSD_WARNING_DURATION);
		}
	}
}

void GSDeviceVK::InvalidateCachedState()
{
	m_dirty_flags |= DIRTY_FLAG_TFX_SAMPLERS_DS | DIRTY_FLAG_TFX_RT_TEXTURE_DS | DIRTY_FLAG_TFX_DYNAMIC_OFFSETS |
					 DIRTY_FLAG_UTILITY_TEXTURE | DIRTY_FLAG_BLEND_CONSTANTS | DIRTY_FLAG_VERTEX_BUFFER |
					 DIRTY_FLAG_INDEX_BUFFER | DIRTY_FLAG_VIEWPORT | DIRTY_FLAG_SCISSOR | DIRTY_FLAG_PIPELINE |
					 DIRTY_FLAG_VS_CONSTANT_BUFFER | DIRTY_FLAG_PS_CONSTANT_BUFFER;
	if (m_vertex_buffer != VK_NULL_HANDLE)
		m_dirty_flags |= DIRTY_FLAG_VERTEX_BUFFER;
	if (m_index_buffer != VK_NULL_HANDLE)
		m_dirty_flags |= DIRTY_FLAG_INDEX_BUFFER;

	for (u32 i = 0; i < NUM_TFX_TEXTURES; i++)
		m_tfx_textures[i] = m_null_texture.get();
	m_utility_texture = m_null_texture.get();
	m_current_framebuffer = VK_NULL_HANDLE;
	m_current_render_target = nullptr;
	m_current_depth_target = nullptr;
	m_current_framebuffer_feedback_loop = FeedbackLoopFlag_None;

	m_current_pipeline_layout = PipelineLayout::Undefined;
	m_tfx_texture_descriptor_set = VK_NULL_HANDLE;
	m_tfx_rt_descriptor_set = VK_NULL_HANDLE;
	m_utility_descriptor_set = VK_NULL_HANDLE;
}

void GSDeviceVK::SetVertexBuffer(VkBuffer buffer, VkDeviceSize offset)
{
	if (m_vertex_buffer == buffer && m_vertex_buffer_offset == offset)
		return;

	m_vertex_buffer = buffer;
	m_vertex_buffer_offset = offset;
	m_dirty_flags |= DIRTY_FLAG_VERTEX_BUFFER;
}

void GSDeviceVK::SetIndexBuffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType type)
{
	if (m_index_buffer == buffer && m_index_buffer_offset == offset && m_index_type == type)
		return;

	m_index_buffer = buffer;
	m_index_buffer_offset = offset;
	m_index_type = type;
	m_dirty_flags |= DIRTY_FLAG_INDEX_BUFFER;
}

void GSDeviceVK::SetBlendConstants(u8 color)
{
	if (m_blend_constant_color == color)
		return;

	m_blend_constant_color = color;
	m_dirty_flags |= DIRTY_FLAG_BLEND_CONSTANTS;
}

void GSDeviceVK::PSSetShaderResource(int i, GSTexture* sr, bool check_state)
{
	GSTextureVK* vkTex = static_cast<GSTextureVK*>(sr);
	if (vkTex)
	{
		if (check_state)
		{
			if (vkTex->GetLayout() != GSTextureVK::Layout::ShaderReadOnly && InRenderPass())
			{
				GL_INS("Ending render pass due to resource transition");
				EndRenderPass();
			}

			vkTex->CommitClear();
			vkTex->TransitionToLayout(GSTextureVK::Layout::ShaderReadOnly);
		}
		vkTex->SetUsedThisCommandBuffer();
	}
	else
	{
		vkTex = m_null_texture.get();
	}

	if (m_tfx_textures[i] == vkTex)
		return;

	m_tfx_textures[i] = vkTex;

	m_dirty_flags |= (i < 2) ? DIRTY_FLAG_TFX_SAMPLERS_DS : DIRTY_FLAG_TFX_RT_TEXTURE_DS;
}

void GSDeviceVK::PSSetSampler(GSHWDrawConfig::SamplerSelector sel)
{
	if (m_tfx_sampler_sel == sel.key)
		return;

	m_tfx_sampler_sel = sel.key;
	m_tfx_sampler = GetSampler(sel);
	m_dirty_flags |= DIRTY_FLAG_TFX_SAMPLERS_DS;
}

void GSDeviceVK::SetUtilityTexture(GSTexture* tex, VkSampler sampler)
{
	GSTextureVK* vkTex = static_cast<GSTextureVK*>(tex);
	if (vkTex)
	{
		vkTex->CommitClear();
		vkTex->TransitionToLayout(GSTextureVK::Layout::ShaderReadOnly);
		vkTex->SetUsedThisCommandBuffer();
	}
	else
	{
		vkTex = m_null_texture.get();
	}

	if (m_utility_texture == vkTex && m_utility_sampler == sampler)
		return;

	m_utility_texture = vkTex;
	m_utility_sampler = sampler;
	m_dirty_flags |= DIRTY_FLAG_UTILITY_TEXTURE;
}

void GSDeviceVK::SetUtilityPushConstants(const void* data, u32 size)
{
	vkCmdPushConstants(g_vulkan_context->GetCurrentCommandBuffer(), m_utility_pipeline_layout,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, size, data);
}

void GSDeviceVK::UnbindTexture(GSTextureVK* tex)
{
	for (u32 i = 0; i < NUM_TFX_TEXTURES; i++)
	{
		if (m_tfx_textures[i] == tex)
		{
			m_tfx_textures[i] = m_null_texture.get();
			m_dirty_flags |= (i < 2) ? DIRTY_FLAG_TFX_SAMPLERS_DS : DIRTY_FLAG_TFX_RT_TEXTURE_DS;
		}
	}
	if (m_utility_texture == tex)
	{
		m_utility_texture = m_null_texture.get();
		m_dirty_flags |= DIRTY_FLAG_UTILITY_TEXTURE;
	}
	if (m_current_render_target == tex || m_current_depth_target == tex)
	{
		EndRenderPass();
		m_current_framebuffer = VK_NULL_HANDLE;
		m_current_render_target = nullptr;
		m_current_depth_target = nullptr;
	}
}

bool GSDeviceVK::InRenderPass() { return m_current_render_pass != VK_NULL_HANDLE; }

void GSDeviceVK::BeginRenderPass(VkRenderPass rp, const GSVector4i& rect)
{
	if (m_current_render_pass != VK_NULL_HANDLE)
		EndRenderPass();

	m_current_render_pass = rp;
	m_current_render_pass_area = rect;

	const VkRenderPassBeginInfo begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr, m_current_render_pass,
		m_current_framebuffer, {{rect.x, rect.y}, {static_cast<u32>(rect.width()), static_cast<u32>(rect.height())}}, 0,
		nullptr};

	g_vulkan_context->CountRenderPass();
	vkCmdBeginRenderPass(g_vulkan_context->GetCurrentCommandBuffer(), &begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void GSDeviceVK::BeginClearRenderPass(VkRenderPass rp, const GSVector4i& rect, const VkClearValue* cv, u32 cv_count)
{
	if (m_current_render_pass != VK_NULL_HANDLE)
		EndRenderPass();

	m_current_render_pass = rp;
	m_current_render_pass_area = rect;

	const VkRenderPassBeginInfo begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr, m_current_render_pass,
		m_current_framebuffer, {{rect.x, rect.y}, {static_cast<u32>(rect.width()), static_cast<u32>(rect.height())}},
		cv_count, cv};

	vkCmdBeginRenderPass(g_vulkan_context->GetCurrentCommandBuffer(), &begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void GSDeviceVK::BeginClearRenderPass(VkRenderPass rp, const GSVector4i& rect, const GSVector4& clear_color)
{
	alignas(16) VkClearValue cv;
	GSVector4::store<true>((void*)cv.color.float32, clear_color);
	BeginClearRenderPass(rp, rect, &cv, 1);
}

void GSDeviceVK::BeginClearRenderPass(VkRenderPass rp, const GSVector4i& rect, float depth, u8 stencil)
{
	VkClearValue cv;
	cv.depthStencil.depth = depth;
	cv.depthStencil.stencil = stencil;
	BeginClearRenderPass(rp, rect, &cv, 1);
}

void GSDeviceVK::EndRenderPass()
{
	if (m_current_render_pass == VK_NULL_HANDLE)
		return;

	m_current_render_pass = VK_NULL_HANDLE;
	g_perfmon.Put(GSPerfMon::RenderPasses, 1);

	vkCmdEndRenderPass(g_vulkan_context->GetCurrentCommandBuffer());
}

void GSDeviceVK::SetViewport(const VkViewport& viewport)
{
	if (std::memcmp(&viewport, &m_viewport, sizeof(VkViewport)) == 0)
		return;

	std::memcpy(&m_viewport, &viewport, sizeof(VkViewport));
	m_dirty_flags |= DIRTY_FLAG_VIEWPORT;
}

void GSDeviceVK::SetScissor(const GSVector4i& scissor)
{
	if (m_scissor.eq(scissor))
		return;

	m_scissor = scissor;
	m_dirty_flags |= DIRTY_FLAG_SCISSOR;
}

void GSDeviceVK::SetPipeline(VkPipeline pipeline)
{
	if (m_current_pipeline == pipeline)
		return;

	m_current_pipeline = pipeline;
	m_dirty_flags |= DIRTY_FLAG_PIPELINE;
}

__ri void GSDeviceVK::ApplyBaseState(u32 flags, VkCommandBuffer cmdbuf)
{
	if (flags & DIRTY_FLAG_VERTEX_BUFFER)
		vkCmdBindVertexBuffers(cmdbuf, 0, 1, &m_vertex_buffer, &m_vertex_buffer_offset);

	if (flags & DIRTY_FLAG_INDEX_BUFFER)
		vkCmdBindIndexBuffer(cmdbuf, m_index_buffer, m_index_buffer_offset, m_index_type);

	if (flags & DIRTY_FLAG_PIPELINE)
		vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_current_pipeline);

	if (flags & DIRTY_FLAG_VIEWPORT)
		vkCmdSetViewport(cmdbuf, 0, 1, &m_viewport);

	if (flags & DIRTY_FLAG_SCISSOR)
	{
		const VkRect2D vscissor{
			{m_scissor.x, m_scissor.y}, {static_cast<u32>(m_scissor.width()), static_cast<u32>(m_scissor.height())}};
		vkCmdSetScissor(cmdbuf, 0, 1, &vscissor);
	}

	if (flags & DIRTY_FLAG_BLEND_CONSTANTS)
	{
		const GSVector4 col(static_cast<float>(m_blend_constant_color) / 128.0f);
		vkCmdSetBlendConstants(cmdbuf, col.v);
	}
}

bool GSDeviceVK::ApplyTFXState(bool already_execed)
{
	if (m_current_pipeline_layout == PipelineLayout::TFX && m_dirty_flags == 0)
		return true;

	const VkDevice dev = g_vulkan_context->GetDevice();
	const VkCommandBuffer cmdbuf = g_vulkan_context->GetCurrentCommandBuffer();
	u32 flags = m_dirty_flags;
	m_dirty_flags &= ~(DIRTY_TFX_STATE | DIRTY_CONSTANT_BUFFER_STATE | DIRTY_FLAG_TFX_DYNAMIC_OFFSETS);

	// do cbuffer first, because it's the most likely to cause an exec
	if (flags & DIRTY_FLAG_VS_CONSTANT_BUFFER)
	{
		if (!m_vertex_uniform_stream_buffer.ReserveMemory(
				sizeof(m_vs_cb_cache), g_vulkan_context->GetUniformBufferAlignment()))
		{
			if (already_execed)
			{
				Console.Error("Failed to reserve vertex uniform space");
				return false;
			}

			ExecuteCommandBufferAndRestartRenderPass(false, "Ran out of vertex uniform space");
			return ApplyTFXState(true);
		}

		std::memcpy(m_vertex_uniform_stream_buffer.GetCurrentHostPointer(), &m_vs_cb_cache, sizeof(m_vs_cb_cache));
		m_tfx_dynamic_offsets[0] = m_vertex_uniform_stream_buffer.GetCurrentOffset();
		m_vertex_uniform_stream_buffer.CommitMemory(sizeof(m_vs_cb_cache));
		flags |= DIRTY_FLAG_TFX_DYNAMIC_OFFSETS;
	}

	if (flags & DIRTY_FLAG_PS_CONSTANT_BUFFER)
	{
		if (!m_fragment_uniform_stream_buffer.ReserveMemory(
				sizeof(m_ps_cb_cache), g_vulkan_context->GetUniformBufferAlignment()))
		{
			if (already_execed)
			{
				Console.Error("Failed to reserve pixel uniform space");
				return false;
			}

			ExecuteCommandBufferAndRestartRenderPass(false, "Ran out of pixel uniform space");
			return ApplyTFXState(true);
		}

		std::memcpy(m_fragment_uniform_stream_buffer.GetCurrentHostPointer(), &m_ps_cb_cache, sizeof(m_ps_cb_cache));
		m_tfx_dynamic_offsets[1] = m_fragment_uniform_stream_buffer.GetCurrentOffset();
		m_fragment_uniform_stream_buffer.CommitMemory(sizeof(m_ps_cb_cache));
		flags |= DIRTY_FLAG_TFX_DYNAMIC_OFFSETS;
	}

	Vulkan::DescriptorSetUpdateBuilder dsub;

	std::array<VkDescriptorSet, NUM_TFX_DESCRIPTOR_SETS> dsets;
	u32 num_dsets = 0;
	u32 start_dset = 0;
	const bool layout_changed = (m_current_pipeline_layout != PipelineLayout::TFX);

	if (!layout_changed && flags & DIRTY_FLAG_TFX_DYNAMIC_OFFSETS)
		dsets[num_dsets++] = m_tfx_ubo_descriptor_set;

	if ((flags & DIRTY_FLAG_TFX_SAMPLERS_DS) || m_tfx_texture_descriptor_set == VK_NULL_HANDLE)
	{
		m_tfx_texture_descriptor_set = g_vulkan_context->AllocateDescriptorSet(m_tfx_sampler_ds_layout);
		if (m_tfx_texture_descriptor_set == VK_NULL_HANDLE)
		{
			if (already_execed)
			{
				Console.Error("Failed to allocate TFX texture descriptors");
				return false;
			}

			ExecuteCommandBufferAndRestartRenderPass(false, "Ran out of TFX texture descriptors");
			return ApplyTFXState(true);
		}

		dsub.AddCombinedImageSamplerDescriptorWrite(m_tfx_texture_descriptor_set, 0, m_tfx_textures[0]->GetView(),
			m_tfx_sampler, m_tfx_textures[0]->GetVkLayout());
		dsub.AddImageDescriptorWrite(
			m_tfx_texture_descriptor_set, 1, m_tfx_textures[1]->GetView(), m_tfx_textures[1]->GetVkLayout());
		dsub.Update(dev);

		if (!layout_changed)
		{
			start_dset = (num_dsets == 0) ? TFX_DESCRIPTOR_SET_TEXTURES : start_dset;
			dsets[num_dsets++] = m_tfx_texture_descriptor_set;
		}
	}

	if ((flags & DIRTY_FLAG_TFX_RT_TEXTURE_DS) || m_tfx_rt_descriptor_set == VK_NULL_HANDLE)
	{
		m_tfx_rt_descriptor_set = g_vulkan_context->AllocateDescriptorSet(m_tfx_rt_texture_ds_layout);
		if (m_tfx_rt_descriptor_set == VK_NULL_HANDLE)
		{
			if (already_execed)
			{
				Console.Error("Failed to allocate TFX sampler descriptors");
				return false;
			}

			ExecuteCommandBufferAndRestartRenderPass(false, "Ran out of TFX sampler descriptors");
			return ApplyTFXState(true);
		}

		if (m_features.texture_barrier)
		{
			dsub.AddInputAttachmentDescriptorWrite(
				m_tfx_rt_descriptor_set, 0, m_tfx_textures[NUM_TFX_DRAW_TEXTURES]->GetView(), VK_IMAGE_LAYOUT_GENERAL);
		}
		else
		{
			dsub.AddImageDescriptorWrite(m_tfx_rt_descriptor_set, 0, m_tfx_textures[NUM_TFX_DRAW_TEXTURES]->GetView(),
				m_tfx_textures[NUM_TFX_DRAW_TEXTURES]->GetVkLayout());
		}
		dsub.AddImageDescriptorWrite(m_tfx_rt_descriptor_set, 1, m_tfx_textures[NUM_TFX_DRAW_TEXTURES + 1]->GetView(),
			m_tfx_textures[NUM_TFX_DRAW_TEXTURES + 1]->GetVkLayout());
		dsub.Update(dev);

		if (!layout_changed)
		{
			// need to add textures in, can't leave a gap
			if (start_dset == TFX_DESCRIPTOR_SET_UBO && num_dsets == 1)
				dsets[num_dsets++] = m_tfx_texture_descriptor_set;
			else
				start_dset = (num_dsets == 0) ? TFX_DESCRIPTOR_SET_RT : start_dset;

			dsets[num_dsets++] = m_tfx_rt_descriptor_set;
		}
	}

	if (layout_changed)
	{
		m_current_pipeline_layout = PipelineLayout::TFX;

		dsets[0] = m_tfx_ubo_descriptor_set;
		dsets[1] = m_tfx_texture_descriptor_set;
		dsets[2] = m_tfx_rt_descriptor_set;

		vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_tfx_pipeline_layout, 0,
			NUM_TFX_DESCRIPTOR_SETS, dsets.data(), NUM_TFX_DYNAMIC_OFFSETS, m_tfx_dynamic_offsets.data());
	}
	else if (num_dsets > 0)
	{
		u32 dynamic_count;
		const u32* dynamic_offsets;
		if (start_dset == TFX_DESCRIPTOR_SET_UBO)
		{
			dynamic_count = NUM_TFX_DYNAMIC_OFFSETS;
			dynamic_offsets = m_tfx_dynamic_offsets.data();
		}
		else
		{
			dynamic_count = 0;
			dynamic_offsets = nullptr;
		}

		vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_tfx_pipeline_layout, start_dset, num_dsets,
			dsets.data(), dynamic_count, dynamic_offsets);
	}

	ApplyBaseState(flags, cmdbuf);
	return true;
}

bool GSDeviceVK::ApplyUtilityState(bool already_execed)
{
	if (m_current_pipeline_layout == PipelineLayout::Utility && m_dirty_flags == 0)
		return true;

	const VkDevice dev = g_vulkan_context->GetDevice();
	const VkCommandBuffer cmdbuf = g_vulkan_context->GetCurrentCommandBuffer();
	u32 flags = m_dirty_flags;
	m_dirty_flags &= ~DIRTY_UTILITY_STATE;

	bool rebind = (m_current_pipeline_layout != PipelineLayout::Utility);

	if ((flags & DIRTY_FLAG_UTILITY_TEXTURE) || m_utility_descriptor_set == VK_NULL_HANDLE)
	{
		m_utility_descriptor_set = g_vulkan_context->AllocateDescriptorSet(m_utility_ds_layout);
		if (m_utility_descriptor_set == VK_NULL_HANDLE)
		{
			if (already_execed)
			{
				Console.Error("Failed to allocate utility descriptors");
				return false;
			}

			ExecuteCommandBufferAndRestartRenderPass(false, "Ran out of utility descriptors");
			return ApplyUtilityState(true);
		}

		Vulkan::DescriptorSetUpdateBuilder dsub;
		dsub.AddCombinedImageSamplerDescriptorWrite(m_utility_descriptor_set, 0, m_utility_texture->GetView(),
			m_utility_sampler, m_utility_texture->GetVkLayout());
		dsub.Update(dev);
		rebind = true;
	}

	if (rebind)
	{
		vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_utility_pipeline_layout, 0, 1,
			&m_utility_descriptor_set, 0, nullptr);
	}

	m_current_pipeline_layout = PipelineLayout::Utility;

	ApplyBaseState(flags, cmdbuf);
	return true;
}

void GSDeviceVK::SetVSConstantBuffer(const GSHWDrawConfig::VSConstantBuffer& cb)
{
	if (m_vs_cb_cache.Update(cb))
		m_dirty_flags |= DIRTY_FLAG_VS_CONSTANT_BUFFER;
}

void GSDeviceVK::SetPSConstantBuffer(const GSHWDrawConfig::PSConstantBuffer& cb)
{
	if (m_ps_cb_cache.Update(cb))
		m_dirty_flags |= DIRTY_FLAG_PS_CONSTANT_BUFFER;
}

static void ColorBufferBarrier(GSTextureVK* rt)
{
	const VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		rt->GetImage(), {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u}};

	vkCmdPipelineBarrier(g_vulkan_context->GetCurrentCommandBuffer(), VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &barrier);
}

void GSDeviceVK::SetupDATE(GSTexture* rt, GSTexture* ds, bool datm, const GSVector4i& bbox)
{
	GL_PUSH("SetupDATE {%d,%d} %dx%d", bbox.left, bbox.top, bbox.width(), bbox.height());

	const GSVector2i size(ds->GetSize());
	const GSVector4 src = GSVector4(bbox) / GSVector4(size).xyxy();
	const GSVector4 dst = src * 2.0f - 1.0f;
	const GSVertexPT1 vertices[] = {
		{GSVector4(dst.x, -dst.y, 0.5f, 1.0f), GSVector2(src.x, src.y)},
		{GSVector4(dst.z, -dst.y, 0.5f, 1.0f), GSVector2(src.z, src.y)},
		{GSVector4(dst.x, -dst.w, 0.5f, 1.0f), GSVector2(src.x, src.w)},
		{GSVector4(dst.z, -dst.w, 0.5f, 1.0f), GSVector2(src.z, src.w)},
	};

	// sfex3 (after the capcom logo), vf4 (first menu fading in), ffxii shadows, rumble roses shadows, persona4 shadows
	EndRenderPass();
	SetUtilityTexture(rt, m_point_sampler);
	OMSetRenderTargets(nullptr, ds, bbox);
	IASetVertexBuffer(vertices, sizeof(vertices[0]), 4);
	SetPipeline(m_convert[static_cast<int>(datm ? ShaderConvert::DATM_1 : ShaderConvert::DATM_0)]);
	BeginClearRenderPass(m_date_setup_render_pass, bbox, 0.0f, 0);
	if (ApplyUtilityState())
		DrawPrimitive();

	EndRenderPass();
}

GSTextureVK* GSDeviceVK::SetupPrimitiveTrackingDATE(GSHWDrawConfig& config)
{
	// How this is done:
	// - can't put a barrier for the image in the middle of the normal render pass, so that's out
	// - so, instead of just filling the int texture with INT_MAX, we sample the RT and use -1 for failing values
	// - then, instead of sampling the RT with DATE=1/2, we just do a min() without it, the -1 gets preserved
	// - then, the DATE=3 draw is done as normal
	GL_INS("Setup DATE Primitive ID Image for {%d,%d}-{%d,%d}", config.drawarea.left, config.drawarea.top,
		config.drawarea.right, config.drawarea.bottom);

	const GSVector2i rtsize(config.rt->GetSize());
	GSTextureVK* image =
		static_cast<GSTextureVK*>(CreateRenderTarget(rtsize.x, rtsize.y, GSTexture::Format::PrimID, false));
	if (!image)
		return nullptr;

	EndRenderPass();

	// setup the fill quad to prefill with existing alpha values
	SetUtilityTexture(config.rt, m_point_sampler);
	OMSetRenderTargets(image, config.ds, config.drawarea);

	// if the depth target has been cleared, we need to preserve that clear
	const VkAttachmentLoadOp ds_load_op = GetLoadOpForTexture(static_cast<GSTextureVK*>(config.ds));
	const u32 ds = (config.ds ? 1 : 0);

	if (ds_load_op == VK_ATTACHMENT_LOAD_OP_CLEAR)
	{
		VkClearValue cv[2] = {};
		cv[1].depthStencil.depth = static_cast<GSTextureVK*>(config.ds)->GetClearDepth();
		cv[1].depthStencil.stencil = 1;
		BeginClearRenderPass(m_date_image_setup_render_passes[ds][1], GSVector4i::loadh(rtsize), cv, 2);
	}
	else
	{
		BeginRenderPass(m_date_image_setup_render_passes[ds][0], config.drawarea);
	}

	// draw the quad to prefill the image
	const GSVector4 src = GSVector4(config.drawarea) / GSVector4(rtsize).xyxy();
	const GSVector4 dst = src * 2.0f - 1.0f;
	const GSVertexPT1 vertices[] = {
		{GSVector4(dst.x, -dst.y, 0.5f, 1.0f), GSVector2(src.x, src.y)},
		{GSVector4(dst.z, -dst.y, 0.5f, 1.0f), GSVector2(src.z, src.y)},
		{GSVector4(dst.x, -dst.w, 0.5f, 1.0f), GSVector2(src.x, src.w)},
		{GSVector4(dst.z, -dst.w, 0.5f, 1.0f), GSVector2(src.z, src.w)},
	};
	const VkPipeline pipeline = m_date_image_setup_pipelines[ds][config.datm];
	SetPipeline(pipeline);
	IASetVertexBuffer(vertices, sizeof(vertices[0]), std::size(vertices));
	if (ApplyUtilityState())
		DrawPrimitive();

	// image is now filled with either -1 or INT_MAX, so now we can do the prepass
	UploadHWDrawVerticesAndIndices(config);

	// cut down the configuration for the prepass, we don't need blending or any feedback loop
	PipelineSelector& pipe = m_pipeline_selector;
	UpdateHWPipelineSelector(config, pipe);
	pipe.dss.zwe = false;
	pipe.cms.wrgba = 0;
	pipe.bs = {};
	pipe.feedback_loop_flags = FeedbackLoopFlag_None;
	pipe.rt = true;
	pipe.ps.blend_a = pipe.ps.blend_b = pipe.ps.blend_c = pipe.ps.blend_d = false;
	pipe.ps.no_color = false;
	pipe.ps.no_color1 = true;
	if (BindDrawPipeline(pipe))
		DrawIndexedPrimitive();

	// image is initialized/prepass is done, so finish up and get ready to do the "real" draw
	EndRenderPass();

	// .. by setting it to DATE=3
	config.ps.date = 3;
	config.alpha_second_pass.ps.date = 3;

	// and bind the image to the primitive sampler
	image->TransitionToLayout(GSTextureVK::Layout::ShaderReadOnly);
	PSSetShaderResource(3, image, false);
	return image;
}

void GSDeviceVK::RenderHW(GSHWDrawConfig& config)
{
	// Destination Alpha Setup
	DATE_RENDER_PASS DATE_rp = DATE_RENDER_PASS_NONE;
	switch (config.destination_alpha)
	{
		case GSHWDrawConfig::DestinationAlphaMode::Off: // No setup
		case GSHWDrawConfig::DestinationAlphaMode::Full: // No setup
		case GSHWDrawConfig::DestinationAlphaMode::PrimIDTracking: // Setup is done below
			break;
		case GSHWDrawConfig::DestinationAlphaMode::StencilOne: // setup is done below
		{
			// we only need to do the setup here if we don't have barriers, in which case do full DATE.
			if (!m_features.texture_barrier)
			{
				SetupDATE(config.rt, config.ds, config.datm, config.drawarea);
				DATE_rp = DATE_RENDER_PASS_STENCIL;
			}
			else
			{
				DATE_rp = DATE_RENDER_PASS_STENCIL_ONE;
			}
		}
		break;

		case GSHWDrawConfig::DestinationAlphaMode::Stencil:
			SetupDATE(config.rt, config.ds, config.datm, config.drawarea);
			DATE_rp = DATE_RENDER_PASS_STENCIL;
			break;
	}

	// stream buffer in first, in case we need to exec
	SetVSConstantBuffer(config.cb_vs);
	SetPSConstantBuffer(config.cb_ps);

	// bind textures before checking the render pass, in case we need to transition them
	if (config.tex)
	{
		PSSetShaderResource(0, config.tex, config.tex != config.rt && config.tex != config.ds);
		PSSetSampler(config.sampler);
	}
	if (config.pal)
		PSSetShaderResource(1, config.pal, true);

	if (config.blend.constant_enable)
		SetBlendConstants(config.blend.constant);

	// Primitive ID tracking DATE setup.
	GSTextureVK* date_image = nullptr;
	if (config.destination_alpha == GSHWDrawConfig::DestinationAlphaMode::PrimIDTracking)
	{
		date_image = SetupPrimitiveTrackingDATE(config);
		if (!date_image)
		{
			Console.WriteLn("Failed to allocate DATE image, aborting draw.");
			return;
		}
	}

	// figure out the pipeline
	PipelineSelector& pipe = m_pipeline_selector;
	UpdateHWPipelineSelector(config, pipe);

	const GSVector2i rtsize(config.rt ? config.rt->GetSize() : config.ds->GetSize());
	GSTextureVK* draw_rt = static_cast<GSTextureVK*>(config.rt);
	GSTextureVK* draw_ds = static_cast<GSTextureVK*>(config.ds);
	GSTextureVK* draw_rt_clone = nullptr;
	GSTextureVK* hdr_rt = nullptr;

	// Switch to hdr target for colclip rendering
	if (pipe.ps.hdr)
	{
		EndRenderPass();

		GL_PUSH_("HDR Render Target Setup");
		hdr_rt = static_cast<GSTextureVK*>(CreateRenderTarget(rtsize.x, rtsize.y, GSTexture::Format::HDRColor, false));
		if (!hdr_rt)
		{
			Console.WriteLn("Failed to allocate HDR render target, aborting draw.");
			if (date_image)
				Recycle(date_image);
			return;
		}

		// propagate clear value through if the hdr render is the first
		if (draw_rt->GetState() == GSTexture::State::Cleared)
		{
			hdr_rt->SetClearColor(draw_rt->GetClearColor());
		}
		else
		{
			hdr_rt->SetState(GSTexture::State::Invalidated);
			draw_rt->TransitionToLayout(GSTextureVK::Layout::ShaderReadOnly);
		}

		// we're not drawing to the RT, so we can use it as a source
		if (config.require_one_barrier && !m_features.texture_barrier)
			PSSetShaderResource(2, draw_rt, true);

		draw_rt = hdr_rt;
	}
	else if (config.require_one_barrier && !m_features.texture_barrier)
	{
		// requires a copy of the RT
		draw_rt_clone = static_cast<GSTextureVK*>(CreateTexture(rtsize.x, rtsize.y, 1, GSTexture::Format::Color, true));
		if (draw_rt_clone)
		{
			EndRenderPass();

			GL_PUSH("Copy RT to temp texture for fbmask {%d,%d %dx%d}",
				config.drawarea.left, config.drawarea.top,
				config.drawarea.width(), config.drawarea.height());

			CopyRect(draw_rt, draw_rt_clone, config.drawarea, config.drawarea.left, config.drawarea.top);
			PSSetShaderResource(2, draw_rt_clone, true);
		}
	}

	// clear texture binding when it's bound to RT or DS.
	if (!config.tex && ((config.rt && static_cast<GSTextureVK*>(config.rt) == m_tfx_textures[0]) ||
						   (config.ds && static_cast<GSTextureVK*>(config.ds) == m_tfx_textures[0])))
	{
		PSSetShaderResource(0, nullptr, false);
	}

	// render pass restart optimizations
	if (hdr_rt || DATE_rp == DATE_RENDER_PASS_STENCIL_ONE)
	{
		// DATE/HDR require clearing/blitting respectively.
		EndRenderPass();
	}
	else if (InRenderPass() && (m_current_render_target == draw_rt || m_current_depth_target == draw_ds))
	{
		// avoid restarting the render pass just to switch from rt+depth to rt and vice versa
		// keep the depth even if doing HDR draws, because the next draw will probably re-enable depth
		if (!draw_rt && m_current_render_target && config.tex != m_current_render_target &&
			m_current_render_target->GetSize() == draw_ds->GetSize())
		{
			draw_rt = m_current_render_target;
			m_pipeline_selector.rt = true;
		}
		else if (!draw_ds && m_current_depth_target && config.tex != m_current_depth_target &&
				 m_current_depth_target->GetSize() == draw_rt->GetSize())
		{
			draw_ds = m_current_depth_target;
			m_pipeline_selector.ds = true;
		}

		// Prefer keeping feedback loop enabled, that way we're not constantly restarting render passes
		pipe.feedback_loop_flags |= m_current_framebuffer_feedback_loop;
	}

	// We don't need the very first barrier if this is the first draw after switching to feedback loop,
	// because the layout change in itself enforces the execution dependency. HDR needs a barrier between
	// setup and the first draw to read it. TODO: Make HDR use subpasses instead.
	const bool skip_first_barrier = (draw_rt && draw_rt->GetLayout() != GSTextureVK::Layout::FeedbackLoop && !pipe.ps.hdr);

	OMSetRenderTargets(draw_rt, draw_ds, config.scissor, static_cast<FeedbackLoopFlag>(pipe.feedback_loop_flags));
	if (pipe.IsRTFeedbackLoop())
	{
		pxAssertMsg(m_features.texture_barrier, "Texture barriers enabled");
		PSSetShaderResource(2, draw_rt, false);
	}

	// Begin render pass if new target or out of the area.
	if (!InRenderPass())
	{
		const VkAttachmentLoadOp rt_op = GetLoadOpForTexture(draw_rt);
		const VkAttachmentLoadOp ds_op = GetLoadOpForTexture(draw_ds);
		const VkRenderPass rp = GetTFXRenderPass(pipe.rt, pipe.ds, pipe.ps.hdr, DATE_rp, pipe.IsRTFeedbackLoop(),
			pipe.IsTestingAndSamplingDepth(), rt_op, ds_op);
		const bool is_clearing_rt = (rt_op == VK_ATTACHMENT_LOAD_OP_CLEAR || ds_op == VK_ATTACHMENT_LOAD_OP_CLEAR);
		const GSVector4i render_area = GSVector4i::loadh(rtsize);

		if (is_clearing_rt || DATE_rp == DATE_RENDER_PASS_STENCIL_ONE)
		{
			// when we're clearing, we set the draw area to the whole fb, otherwise part of it will be undefined
			alignas(16) VkClearValue cvs[2];
			u32 cv_count = 0;
			if (draw_rt)
				GSVector4::store<true>(&cvs[cv_count++].color, draw_rt->GetClearColor());

			// the only time the stencil value is used here is DATE_one, so setting it to 1 is fine (not used otherwise)
			if (draw_ds)
				cvs[cv_count++].depthStencil = {draw_ds->GetClearDepth(), 1};

			BeginClearRenderPass(rp, render_area, cvs, cv_count);
		}
		else
		{
			BeginRenderPass(rp, render_area);
		}
	}

	// rt -> hdr blit if enabled
	if (hdr_rt && config.rt->GetState() == GSTexture::State::Dirty)
	{
		SetUtilityTexture(static_cast<GSTextureVK*>(config.rt), m_point_sampler);
		SetPipeline(m_hdr_setup_pipelines[pipe.ds][pipe.IsRTFeedbackLoop()]);

		const GSVector4 drawareaf = GSVector4(config.drawarea);
		const GSVector4 sRect(drawareaf / GSVector4(rtsize).xyxy());
		DrawStretchRect(sRect, drawareaf, rtsize);
		g_perfmon.Put(GSPerfMon::TextureCopies, 1);

		GL_POP();
	}

	// VB/IB upload, if we did DATE setup and it's not HDR this has already been done
	if (!date_image || hdr_rt)
		UploadHWDrawVerticesAndIndices(config);

	// now we can do the actual draw
	if (BindDrawPipeline(pipe))
	{
		SendHWDraw(config, draw_rt, skip_first_barrier);
		if (config.separate_alpha_pass)
		{
			SetHWDrawConfigForAlphaPass(&pipe.ps, &pipe.cms, &pipe.bs, &pipe.dss);
			if (BindDrawPipeline(pipe))
				SendHWDraw(config, draw_rt, false);
		}
	}

	// and the alpha pass
	if (config.alpha_second_pass.enable)
	{
		// cbuffer will definitely be dirty if aref changes, no need to check it
		if (config.cb_ps.FogColor_AREF.a != config.alpha_second_pass.ps_aref)
		{
			config.cb_ps.FogColor_AREF.a = config.alpha_second_pass.ps_aref;
			SetPSConstantBuffer(config.cb_ps);
		}

		pipe.ps = config.alpha_second_pass.ps;
		pipe.cms = config.alpha_second_pass.colormask;
		pipe.dss = config.alpha_second_pass.depth;
		pipe.bs = config.blend;
		if (BindDrawPipeline(pipe))
		{
			SendHWDraw(config, draw_rt, false);
			if (config.second_separate_alpha_pass)
			{
				SetHWDrawConfigForAlphaPass(&pipe.ps, &pipe.cms, &pipe.bs, &pipe.dss);
				if (BindDrawPipeline(pipe))
					SendHWDraw(config, draw_rt, false);
			}
		}
	}

	if (draw_rt_clone)
		Recycle(draw_rt_clone);

	if (date_image)
		Recycle(date_image);

	// now blit the hdr texture back to the original target
	if (hdr_rt)
	{
		GL_INS("Blit HDR back to RT");

		EndRenderPass();
		hdr_rt->TransitionToLayout(GSTextureVK::Layout::ShaderReadOnly);

		draw_rt = static_cast<GSTextureVK*>(config.rt);
		OMSetRenderTargets(draw_rt, draw_ds, config.scissor, static_cast<FeedbackLoopFlag>(pipe.feedback_loop_flags));

		// if this target was cleared and never drawn to, perform the clear as part of the resolve here.
		if (draw_rt->GetState() == GSTexture::State::Cleared)
		{
			alignas(16) VkClearValue cvs[2];
			u32 cv_count = 0;
			GSVector4::store<true>(&cvs[cv_count++].color, draw_rt->GetClearColor());
			if (draw_ds)
				cvs[cv_count++].depthStencil = {draw_ds->GetClearDepth(), 1};

			BeginClearRenderPass(GetTFXRenderPass(true, pipe.ds, false, DATE_RENDER_PASS_NONE, pipe.IsRTFeedbackLoop(),
									 pipe.IsTestingAndSamplingDepth(), VK_ATTACHMENT_LOAD_OP_CLEAR,
									 pipe.ds ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE),
				draw_rt->GetRect(), cvs, cv_count);
			draw_rt->SetState(GSTexture::State::Dirty);
		}
		else
		{
			BeginRenderPass(GetTFXRenderPass(true, pipe.ds, false, DATE_RENDER_PASS_NONE, pipe.IsRTFeedbackLoop(),
								pipe.IsTestingAndSamplingDepth(), VK_ATTACHMENT_LOAD_OP_LOAD,
								pipe.ds ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE),
				draw_rt->GetRect());
		}

		const GSVector4 drawareaf = GSVector4(config.drawarea);
		const GSVector4 sRect(drawareaf / GSVector4(rtsize).xyxy());
		SetPipeline(m_hdr_finish_pipelines[pipe.ds][pipe.IsRTFeedbackLoop()]);
		SetUtilityTexture(hdr_rt, m_point_sampler);
		DrawStretchRect(sRect, drawareaf, rtsize);
		g_perfmon.Put(GSPerfMon::TextureCopies, 1);

		Recycle(hdr_rt);
	}
}

void GSDeviceVK::UpdateHWPipelineSelector(GSHWDrawConfig& config, PipelineSelector& pipe)
{
	pipe.vs.key = config.vs.key;
	pipe.ps.key_hi = config.ps.key_hi;
	pipe.ps.key_lo = config.ps.key_lo;
	pipe.dss.key = config.depth.key;
	pipe.bs.key = config.blend.key;
	pipe.bs.constant = 0; // don't dupe states with different alpha values
	pipe.cms.key = config.colormask.key;
	pipe.topology = static_cast<u32>(config.topology);
	pipe.rt = config.rt != nullptr;
	pipe.ds = config.ds != nullptr;
	pipe.line_width = config.line_expand;
	pipe.feedback_loop_flags =
		(m_features.texture_barrier &&
			(config.ps.IsFeedbackLoop() || config.require_one_barrier || config.require_full_barrier)) ?
			FeedbackLoopFlag_ReadAndWriteRT :
			FeedbackLoopFlag_None;
	pipe.feedback_loop_flags |=
		(config.tex && config.tex == config.ds) ? FeedbackLoopFlag_ReadDS : FeedbackLoopFlag_None;

	// enable point size in the vertex shader if we're rendering points regardless of upscaling.
	pipe.vs.point_size |= (config.topology == GSHWDrawConfig::Topology::Point);
}

void GSDeviceVK::UploadHWDrawVerticesAndIndices(const GSHWDrawConfig& config)
{
	IASetVertexBuffer(config.verts, sizeof(GSVertex), config.nverts);

	if (config.vs.UseExpandIndexBuffer())
	{
		m_index.start = 0;
		m_index.count = config.nindices;
		SetIndexBuffer(m_expand_index_buffer, 0, VK_INDEX_TYPE_UINT16);
	}
	else
	{
		IASetIndexBuffer(config.indices, config.nindices);
	}
}

void GSDeviceVK::SendHWDraw(const GSHWDrawConfig& config, GSTextureVK* draw_rt, bool skip_first_barrier)
{
	if (config.drawlist)
	{
		GL_PUSH("Split the draw (SPRITE)");
		g_perfmon.Put(GSPerfMon::Barriers, static_cast<u32>(config.drawlist->size()) - static_cast<u32>(skip_first_barrier));

		const u32 indices_per_prim = config.indices_per_prim;
		const u32 draw_list_size = static_cast<u32>(config.drawlist->size());
		u32 p = 0;
		u32 n = 0;

		if (skip_first_barrier)
		{
			const u32 count = (*config.drawlist)[n] * indices_per_prim;
			DrawIndexedPrimitive(p, count);
			p += count;
			++n;
		}

		for (; n < draw_list_size; n++)
		{
			const u32 count = (*config.drawlist)[n] * indices_per_prim;
			ColorBufferBarrier(draw_rt);
			DrawIndexedPrimitive(p, count);
			p += count;
		}

		return;
	}

	if (m_features.texture_barrier && m_pipeline_selector.ps.IsFeedbackLoop())
	{
		if (config.require_full_barrier)
		{
			const u32 indices_per_prim = config.indices_per_prim;

			GL_PUSH("Split single draw in %d draw", config.nindices / indices_per_prim);
			g_perfmon.Put(GSPerfMon::Barriers, (config.nindices / indices_per_prim) - static_cast<u32>(skip_first_barrier));

			u32 p = 0;
			if (skip_first_barrier)
			{
				DrawIndexedPrimitive(p, indices_per_prim);
				p += indices_per_prim;
			}

			for (; p < config.nindices; p += indices_per_prim)
			{
				ColorBufferBarrier(draw_rt);
				DrawIndexedPrimitive(p, indices_per_prim);
			}

			return;
		}

		if (config.require_one_barrier && !skip_first_barrier)
		{
			g_perfmon.Put(GSPerfMon::Barriers, 1);
			ColorBufferBarrier(draw_rt);
		}
	}

	DrawIndexedPrimitive();
}
