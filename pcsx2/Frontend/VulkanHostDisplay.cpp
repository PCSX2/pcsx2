#include "PrecompiledHeader.h"

#include "VulkanHostDisplay.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "common/ScopedGuard.h"
#include "common/Vulkan/Builders.h"
#include "common/Vulkan/Context.h"
#include "common/Vulkan/ShaderCache.h"
#include "common/Vulkan/StreamBuffer.h"
#include "common/Vulkan/SwapChain.h"
#include "common/Vulkan/Util.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include <array>

static constexpr u32 SHADER_CACHE_VERSION = 4;

class VulkanHostDisplayTexture : public HostDisplayTexture
{
public:
	explicit VulkanHostDisplayTexture(Vulkan::Texture texture)
		: m_texture(std::move(texture))
	{
	}
	~VulkanHostDisplayTexture() override = default;

	void* GetHandle() const override { return const_cast<Vulkan::Texture*>(&m_texture); }
	u32 GetWidth() const override { return m_texture.GetWidth(); }
	u32 GetHeight() const override { return m_texture.GetHeight(); }

	const Vulkan::Texture& GetTexture() const { return m_texture; }
	Vulkan::Texture& GetTexture() { return m_texture; }

private:
	Vulkan::Texture m_texture;
};

static VkPresentModeKHR GetPreferredPresentModeForVsyncMode(VsyncMode mode)
{
	if (mode == VsyncMode::On)
		return VK_PRESENT_MODE_FIFO_KHR;
	else if (mode == VsyncMode::Adaptive)
		return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
	else
		return VK_PRESENT_MODE_IMMEDIATE_KHR;
}

VulkanHostDisplay::VulkanHostDisplay() = default;

VulkanHostDisplay::~VulkanHostDisplay()
{
	pxAssertRel(!g_vulkan_context, "Context should have been destroyed by now");
	pxAssertRel(!m_swap_chain, "Swap chain should have been destroyed by now");
}

HostDisplay::RenderAPI VulkanHostDisplay::GetRenderAPI() const { return HostDisplay::RenderAPI::Vulkan; }

void* VulkanHostDisplay::GetRenderDevice() const { return nullptr; }

void* VulkanHostDisplay::GetRenderContext() const { return nullptr; }

void* VulkanHostDisplay::GetRenderSurface() const { return m_swap_chain.get(); }

bool VulkanHostDisplay::ChangeRenderWindow(const WindowInfo& new_wi)
{
	g_vulkan_context->WaitForGPUIdle();

	if (new_wi.type == WindowInfo::Type::Surfaceless)
	{
		g_vulkan_context->ExecuteCommandBuffer(true);
		m_swap_chain.reset();
		m_window_info = new_wi;
		return true;
	}

	// recreate surface in existing swap chain if it already exists
	if (m_swap_chain)
	{
		if (m_swap_chain->RecreateSurface(new_wi))
		{
			m_window_info = m_swap_chain->GetWindowInfo();
			return true;
		}

		m_swap_chain.reset();
	}

	WindowInfo wi_copy(new_wi);
	VkSurfaceKHR surface = Vulkan::SwapChain::CreateVulkanSurface(
		g_vulkan_context->GetVulkanInstance(), g_vulkan_context->GetPhysicalDevice(), &wi_copy);
	if (surface == VK_NULL_HANDLE)
	{
		Console.Error("Failed to create new surface for swap chain");
		return false;
	}

	m_swap_chain = Vulkan::SwapChain::Create(wi_copy, surface, GetPreferredPresentModeForVsyncMode(m_vsync_mode));
	if (!m_swap_chain)
	{
		Console.Error("Failed to create swap chain");
		Vulkan::SwapChain::DestroyVulkanSurface(g_vulkan_context->GetVulkanInstance(), &wi_copy, surface);
		return false;
	}

	m_window_info = m_swap_chain->GetWindowInfo();
	return true;
}

void VulkanHostDisplay::ResizeRenderWindow(s32 new_window_width, s32 new_window_height, float new_window_scale)
{
	g_vulkan_context->WaitForGPUIdle();

	if (!m_swap_chain->ResizeSwapChain(new_window_width, new_window_height))
	{
		// AcquireNextImage() will fail, and we'll recreate the surface.
		Console.Error("Failed to resize swap chain. Next present will fail.");
		return;
	}

	m_window_info = m_swap_chain->GetWindowInfo();
}

bool VulkanHostDisplay::SupportsFullscreen() const { return false; }

bool VulkanHostDisplay::IsFullscreen() { return false; }

bool VulkanHostDisplay::SetFullscreen(bool fullscreen, u32 width, u32 height, float refresh_rate) { return false; }

HostDisplay::AdapterAndModeList VulkanHostDisplay::GetAdapterAndModeList()
{
	return StaticGetAdapterAndModeList(m_window_info.type != WindowInfo::Type::Surfaceless ? &m_window_info : nullptr);
}

void VulkanHostDisplay::DestroyRenderSurface()
{
	m_window_info = {};
	g_vulkan_context->WaitForGPUIdle();
	m_swap_chain.reset();
}

std::string VulkanHostDisplay::GetDriverInfo() const
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
			props.conformanceVersion.major, props.conformanceVersion.minor, props.conformanceVersion.subminor, props.conformanceVersion.patch,
			props.driverInfo, props.driverName,
			g_vulkan_context->GetDeviceProperties().deviceName);
	}
	else
	{
		ret = StringUtil::StdStringFromFormat(
			"Driver %u.%u.%u\nVulkan %u.%u.%u\n%s",
			VK_VERSION_MAJOR(driver_version), VK_VERSION_MINOR(driver_version), VK_VERSION_PATCH(driver_version),
			VK_API_VERSION_MAJOR(api_version), VK_API_VERSION_MINOR(api_version), VK_API_VERSION_PATCH(api_version),
			g_vulkan_context->GetDeviceProperties().deviceName);
	}

	return ret;
}

static bool UploadBufferToTexture(Vulkan::Texture* texture, u32 width, u32 height, const void* data, u32 data_stride)
{
	const u32 tight_stride = Vulkan::Util::GetTexelSize(texture->GetFormat()) * width;
	const u32 tight_size = tight_stride * height;

	Vulkan::StreamBuffer& buf = g_vulkan_context->GetTextureUploadBuffer();
	if (!buf.ReserveMemory(tight_size, g_vulkan_context->GetBufferImageGranularity()))
	{
		Console.WriteLn("Executing command buffer for UploadBufferToTexture()");
		g_vulkan_context->ExecuteCommandBuffer(false);
		if (!buf.ReserveMemory(tight_size, g_vulkan_context->GetBufferImageGranularity()))
		{
			Console.WriteLn("Failed to allocate %u bytes in stream buffer for UploadBufferToTexture()", tight_size);
			return false;
		}
	}

	const u32 buf_offset = buf.GetCurrentOffset();
	StringUtil::StrideMemCpy(buf.GetCurrentHostPointer(), tight_stride, data, data_stride, tight_stride, height);
	buf.CommitMemory(tight_size);

	texture->UpdateFromBuffer(
		g_vulkan_context->GetCurrentCommandBuffer(), 0, 0, 0, 0, width, height, width, buf.GetBuffer(), buf_offset);
	return true;
}

std::unique_ptr<HostDisplayTexture> VulkanHostDisplay::CreateTexture(u32 width, u32 height, const void* data, u32 data_stride, bool dynamic /* = false */)
{
	static constexpr VkFormat vk_format = VK_FORMAT_R8G8B8A8_UNORM;
	static constexpr VkImageUsageFlags usage =
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	Vulkan::Texture texture;
	if (!texture.Create(width, height, 1, 1, vk_format, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage))
		return {};

	texture.TransitionToLayout(g_vulkan_context->GetCurrentCommandBuffer(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	if (data)
	{
		if (!UploadBufferToTexture(&texture, width, height, data, data_stride))
			return {};
	}
	else
	{
		// clear it instead so we don't read uninitialized data (and keep the validation layer happy!)
		static constexpr VkClearColorValue ccv = {};
		static constexpr VkImageSubresourceRange isr = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
		vkCmdClearColorImage(
			g_vulkan_context->GetCurrentCommandBuffer(), texture.GetImage(), texture.GetLayout(), &ccv, 1u, &isr);
	}

	texture.TransitionToLayout(g_vulkan_context->GetCurrentCommandBuffer(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	return std::make_unique<VulkanHostDisplayTexture>(std::move(texture));
}

void VulkanHostDisplay::UpdateTexture(
	HostDisplayTexture* texture, u32 x, u32 y, u32 width, u32 height, const void* data, u32 data_stride)
{
	UploadBufferToTexture(
		&static_cast<VulkanHostDisplayTexture*>(texture)->GetTexture(), width, height, data, data_stride);
}

void VulkanHostDisplay::SetVSync(VsyncMode mode)
{
	if (!m_swap_chain || m_vsync_mode == mode)
		return;

	// This swap chain should not be used by the current buffer, thus safe to destroy.
	g_vulkan_context->WaitForGPUIdle();
	m_swap_chain->SetVSync(GetPreferredPresentModeForVsyncMode(mode));
	m_vsync_mode = mode;
}

bool VulkanHostDisplay::CreateRenderDevice(
	const WindowInfo& wi, std::string_view adapter_name, VsyncMode vsync, bool threaded_presentation, bool debug_device)
{
	// debug_device = true;

	WindowInfo local_wi(wi);
	if (!Vulkan::Context::Create(
			adapter_name, &local_wi, &m_swap_chain, GetPreferredPresentModeForVsyncMode(vsync),
			threaded_presentation, debug_device, debug_device))
	{
		Console.Error("Failed to create Vulkan context");
		m_window_info = {};
		return false;
	}

	// NOTE: This is assigned afterwards, because some platforms can modify the window info (e.g. Metal).
	m_window_info = m_swap_chain ? m_swap_chain->GetWindowInfo() : local_wi;
	m_vsync_mode = vsync;
	return true;
}

bool VulkanHostDisplay::InitializeRenderDevice(std::string_view shader_cache_directory, bool debug_device)
{
	Vulkan::ShaderCache::Create(shader_cache_directory, SHADER_CACHE_VERSION, debug_device);
	return true;
}

bool VulkanHostDisplay::HasRenderDevice() const { return static_cast<bool>(g_vulkan_context); }

bool VulkanHostDisplay::HasRenderSurface() const { return static_cast<bool>(m_swap_chain); }

bool VulkanHostDisplay::CreateImGuiContext()
{
	return ImGui_ImplVulkan_Init(m_swap_chain->GetClearRenderPass());
}

void VulkanHostDisplay::DestroyImGuiContext()
{
	g_vulkan_context->WaitForGPUIdle();
	ImGui_ImplVulkan_Shutdown();
}

bool VulkanHostDisplay::UpdateImGuiFontTexture()
{
	return ImGui_ImplVulkan_CreateFontsTexture();
}

void VulkanHostDisplay::DestroyRenderDevice()
{
	if (!g_vulkan_context)
		return;

	g_vulkan_context->WaitForGPUIdle();

	Vulkan::ShaderCache::Destroy();
	DestroyRenderSurface();
	Vulkan::Context::Destroy();
}

bool VulkanHostDisplay::MakeRenderContextCurrent() { return true; }

bool VulkanHostDisplay::DoneRenderContextCurrent() { return true; }

bool VulkanHostDisplay::BeginPresent(bool frame_skip)
{
	if (frame_skip || !m_swap_chain)
	{
		ImGui::EndFrame();
		return false;
	}

	// Previous frame needs to be presented before we can acquire the swap chain.
	g_vulkan_context->WaitForPresentComplete();

	VkResult res = m_swap_chain->AcquireNextImage();
	if (res != VK_SUCCESS)
	{
		if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
		{
			ResizeRenderWindow(0, 0, m_window_info.surface_scale);
			res = m_swap_chain->AcquireNextImage();
		}
		else if (res == VK_ERROR_SURFACE_LOST_KHR)
		{
			Console.Warning("Surface lost, attempting to recreate");
			if (!m_swap_chain->RecreateSurface(m_window_info))
			{
				Console.Error("Failed to recreate surface after loss");
				g_vulkan_context->ExecuteCommandBuffer(false);
				return false;
			}

			res = m_swap_chain->AcquireNextImage();
		}

		// This can happen when multiple resize events happen in quick succession.
		// In this case, just wait until the next frame to try again.
		if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR)
		{
			// Still submit the command buffer, otherwise we'll end up with several frames waiting.
			LOG_VULKAN_ERROR(res, "vkAcquireNextImageKHR() failed: ");
			g_vulkan_context->ExecuteCommandBuffer(false);
			return false;
		}
	}

	VkCommandBuffer cmdbuffer = g_vulkan_context->GetCurrentCommandBuffer();

	// Swap chain images start in undefined
	Vulkan::Texture& swap_chain_texture = m_swap_chain->GetCurrentTexture();
	swap_chain_texture.OverrideImageLayout(VK_IMAGE_LAYOUT_UNDEFINED);
	swap_chain_texture.TransitionToLayout(cmdbuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	const VkClearValue clear_value = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
	const VkRenderPassBeginInfo rp = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr,
		m_swap_chain->GetClearRenderPass(), m_swap_chain->GetCurrentFramebuffer(),
		{{0, 0}, {swap_chain_texture.GetWidth(), swap_chain_texture.GetHeight()}}, 1u, &clear_value};
	vkCmdBeginRenderPass(g_vulkan_context->GetCurrentCommandBuffer(), &rp, VK_SUBPASS_CONTENTS_INLINE);

	const VkViewport vp{0.0f, 0.0f, static_cast<float>(swap_chain_texture.GetWidth()),
		static_cast<float>(swap_chain_texture.GetHeight()), 0.0f, 1.0f};
	const VkRect2D scissor{
		{0, 0}, {static_cast<u32>(swap_chain_texture.GetWidth()), static_cast<u32>(swap_chain_texture.GetHeight())}};
	vkCmdSetViewport(g_vulkan_context->GetCurrentCommandBuffer(), 0, 1, &vp);
	vkCmdSetScissor(g_vulkan_context->GetCurrentCommandBuffer(), 0, 1, &scissor);
	return true;
}

void VulkanHostDisplay::EndPresent()
{
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData());

	VkCommandBuffer cmdbuffer = g_vulkan_context->GetCurrentCommandBuffer();
	vkCmdEndRenderPass(g_vulkan_context->GetCurrentCommandBuffer());
	m_swap_chain->GetCurrentTexture().TransitionToLayout(cmdbuffer, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	g_vulkan_context->SubmitCommandBuffer(m_swap_chain->GetImageAvailableSemaphore(),
		m_swap_chain->GetRenderingFinishedSemaphore(), m_swap_chain->GetSwapChain(),
		m_swap_chain->GetCurrentImageIndex(), !m_swap_chain->IsPresentModeSynchronizing());
	g_vulkan_context->MoveToNextCommandBuffer();
}

void VulkanHostDisplay::SetGPUTimingEnabled(bool enabled)
{
	g_vulkan_context->SetEnableGPUTiming(enabled);
}

float VulkanHostDisplay::GetAndResetAccumulatedGPUTime()
{
	return g_vulkan_context->GetAndResetAccumulatedGPUTime();
}

HostDisplay::AdapterAndModeList VulkanHostDisplay::StaticGetAdapterAndModeList(const WindowInfo* wi)
{
	AdapterAndModeList ret;
	std::vector<Vulkan::SwapChain::FullscreenModeInfo> fsmodes;

	if (g_vulkan_context)
	{
		ret.adapter_names = Vulkan::Context::EnumerateGPUNames(g_vulkan_context->GetVulkanInstance());
		if (wi)
		{
			fsmodes = Vulkan::SwapChain::GetSurfaceFullscreenModes(
				g_vulkan_context->GetVulkanInstance(), g_vulkan_context->GetPhysicalDevice(), *wi);
		}
	}
	else if (Vulkan::LoadVulkanLibrary())
	{
		ScopedGuard lib_guard([]() { Vulkan::UnloadVulkanLibrary(); });

		VkInstance instance = Vulkan::Context::CreateVulkanInstance(nullptr, false, false);
		if (instance != VK_NULL_HANDLE)
		{
			ScopedGuard instance_guard([&instance]() { vkDestroyInstance(instance, nullptr); });

			if (Vulkan::LoadVulkanInstanceFunctions(instance))
				ret.adapter_names = Vulkan::Context::EnumerateGPUNames(instance);
		}
	}

	if (!fsmodes.empty())
	{
		ret.fullscreen_modes.reserve(fsmodes.size());
		for (const Vulkan::SwapChain::FullscreenModeInfo& fmi : fsmodes)
		{
			ret.fullscreen_modes.push_back(GetFullscreenModeString(fmi.width, fmi.height, fmi.refresh_rate));
		}
	}

	return ret;
}
