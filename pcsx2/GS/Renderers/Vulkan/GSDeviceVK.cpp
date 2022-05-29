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
#include "common/Vulkan/Builders.h"
#include "common/Vulkan/Context.h"
#include "common/Vulkan/ShaderCache.h"
#include "common/Vulkan/SwapChain.h"
#include "common/Vulkan/Util.h"
#include "common/Align.h"
#include "common/ScopedGuard.h"
#include "GS.h"
#include "GSDeviceVK.h"
#include "GS/GSGL.h"
#include "GS/GSPerfMon.h"
#include "GS/GSUtil.h"
#include "Host.h"
#include "HostDisplay.h"
#include <sstream>
#include <limits>

#ifdef ENABLE_OGL_DEBUG
static u32 s_debug_scope_depth = 0;
#endif

static bool IsDepthConvertShader(ShaderConvert i)
{
	return (i == ShaderConvert::DEPTH_COPY || i == ShaderConvert::RGBA8_TO_FLOAT32 ||
			i == ShaderConvert::RGBA8_TO_FLOAT24 || i == ShaderConvert::RGBA8_TO_FLOAT16 ||
			i == ShaderConvert::RGB5A1_TO_FLOAT16 || i == ShaderConvert::DATM_0 ||
			i == ShaderConvert::DATM_1);
}

static bool IsIntConvertShader(ShaderConvert i)
{
	return (i == ShaderConvert::RGBA8_TO_16_BITS || i == ShaderConvert::FLOAT32_TO_16_BITS ||
			i == ShaderConvert::FLOAT32_TO_32_BITS);
}

static bool IsDATMConvertShader(ShaderConvert i) { return (i == ShaderConvert::DATM_0 || i == ShaderConvert::DATM_1); }

static bool IsPresentConvertShader(ShaderConvert i)
{
	return (i == ShaderConvert::COPY || (i >= ShaderConvert::SCANLINE && i <= ShaderConvert::COMPLEX_FILTER));
}

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

GSDeviceVK::GSDeviceVK()
{
#ifdef ENABLE_OGL_DEBUG
	s_debug_scope_depth = 0;
#endif

	std::memset(&m_pipeline_selector, 0, sizeof(m_pipeline_selector));
}

GSDeviceVK::~GSDeviceVK() {}

bool GSDeviceVK::Create(HostDisplay* display)
{
	if (!GSDevice::Create(display) || !CheckFeatures())
		return false;

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

	if (!CompileConvertPipelines() || !CompileInterlacePipelines() ||
		!CompileMergePipelines() || !CompilePostProcessingPipelines())
	{
		Host::ReportErrorAsync("GS", "Failed to compile utility pipelines");
		return false;
	}

	if (!CreatePersistentDescriptorSets())
	{
		Host::ReportErrorAsync("GS", "Failed to create persistent descriptor sets");
		return false;
	}

	InitializeState();
	return true;
}

void GSDeviceVK::Destroy()
{
	if (!g_vulkan_context)
		return;

	EndRenderPass();
	ExecuteCommandBuffer(true);
	DestroyResources();
	GSDevice::Destroy();
}

void GSDeviceVK::ResetAPIState()
{
	EndRenderPass();
}

void GSDeviceVK::RestoreAPIState()
{
	InvalidateCachedState();
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
	if (!vkCmdBeginDebugUtilsLabelEXT)
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
	if (!vkCmdEndDebugUtilsLabelEXT)
		return;

	s_debug_scope_depth = (s_debug_scope_depth == 0) ? 0 : (s_debug_scope_depth - 1u);

	vkCmdEndDebugUtilsLabelEXT(g_vulkan_context->GetCurrentCommandBuffer());
#endif
}

void GSDeviceVK::InsertDebugMessage(DebugMessageCategory category, const char* fmt, ...)
{
#ifdef ENABLE_OGL_DEBUG
	if (!vkCmdInsertDebugUtilsLabelEXT)
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

bool GSDeviceVK::CheckFeatures()
{
	const VkPhysicalDeviceProperties& properties = g_vulkan_context->GetDeviceProperties();
	const VkPhysicalDeviceFeatures& features = g_vulkan_context->GetDeviceFeatures();
	const VkPhysicalDeviceLimits& limits = g_vulkan_context->GetDeviceLimits();
	const u32 vendorID = properties.vendorID;
	const bool isAMD = (vendorID == 0x1002 || vendorID == 0x1022);
	// const bool isNVIDIA = (vendorID == 0x10DE);

	m_features.framebuffer_fetch = g_vulkan_context->GetOptionalExtensions().vk_arm_rasterization_order_attachment_access && !GSConfig.DisableFramebufferFetch;
	m_features.texture_barrier = GSConfig.OverrideTextureBarriers != 0;
	m_features.broken_point_sampler = isAMD;
	m_features.geometry_shader = features.geometryShader && GSConfig.OverrideGeometryShaders != 0;
	m_features.image_load_store = features.fragmentStoresAndAtomics;
	m_features.prefer_new_textures = true;
	m_features.provoking_vertex_last = g_vulkan_context->GetOptionalExtensions().vk_ext_provoking_vertex;
	m_features.dual_source_blend = features.dualSrcBlend && !GSConfig.DisableDualSourceBlend;

	if (!m_features.dual_source_blend)
		Console.Warning("Vulkan driver is missing dual-source blending. This will have an impact on performance.");

	if (!m_features.texture_barrier)
		Console.Warning("Texture buffers are disabled. This may break some graphical effects.");

	// Test for D32S8 support.
	{
		VkFormatProperties props = {};
		vkGetPhysicalDeviceFormatProperties(g_vulkan_context->GetPhysicalDevice(), VK_FORMAT_D32_SFLOAT_S8_UINT, &props);
		m_features.stencil_buffer = ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0);
	}

	// Fbfetch is useless if we don't have barriers enabled.
	m_features.framebuffer_fetch &= m_features.texture_barrier;

	// Use D32F depth instead of D32S8 when we have framebuffer fetch.
	m_features.stencil_buffer &= !m_features.framebuffer_fetch;

	// whether we can do point/line expand depends on the range of the device
	const float f_upscale = static_cast<float>(GSConfig.UpscaleMultiplier);
	m_features.point_expand =
		(features.largePoints && limits.pointSizeRange[0] <= f_upscale && limits.pointSizeRange[1] >= f_upscale);
	m_features.line_expand =
		(features.wideLines && limits.lineWidthRange[0] <= f_upscale && limits.lineWidthRange[1] >= f_upscale);
	Console.WriteLn("Using %s for point expansion and %s for line expansion.",
		m_features.point_expand ? "hardware" : "geometry shaders",
		m_features.line_expand ? "hardware" : "geometry shaders");

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
			"Stencil buffers and texture barriers are both unavailable, this will break some graphical effects.", 10.0f);
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

void GSDeviceVK::ClearStencil(GSTexture* t, u8 c)
{
	if (!t)
		return;

	EndRenderPass();

	static_cast<GSTextureVK*>(t)->TransitionToLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	const VkClearDepthStencilValue dsv{0.0f, static_cast<u32>(c)};
	const VkImageSubresourceRange srr{VK_IMAGE_ASPECT_STENCIL_BIT, 0u, 1u, 0u, 1u};

	vkCmdClearDepthStencilImage(g_vulkan_context->GetCurrentCommandBuffer(), static_cast<GSTextureVK*>(t)->GetImage(),
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &dsv, 1, &srr);

	static_cast<GSTextureVK*>(t)->TransitionToLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

VkFormat GSDeviceVK::LookupNativeFormat(GSTexture::Format format) const
{
	static constexpr std::array<VkFormat, static_cast<int>(GSTexture::Format::BC7) + 1> s_format_mapping = {{
		VK_FORMAT_UNDEFINED, // Invalid
		VK_FORMAT_R8G8B8A8_UNORM, // Color
		VK_FORMAT_R32G32B32A32_SFLOAT, // FloatColor
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
	pxAssert(type != GSTexture::Type::Offscreen && type != GSTexture::Type::SparseRenderTarget &&
			 type != GSTexture::Type::SparseDepthStencil);

	const u32 clamped_width = static_cast<u32>(std::clamp<int>(1, width, g_vulkan_context->GetMaxImageDimension2D()));
	const u32 clamped_height = static_cast<u32>(std::clamp<int>(1, height, g_vulkan_context->GetMaxImageDimension2D()));

	return GSTextureVK::Create(type, clamped_width, clamped_height, levels, format, LookupNativeFormat(format)).release();
}

bool GSDeviceVK::DownloadTexture(GSTexture* src, const GSVector4i& rect, GSTexture::GSMap& out_map)
{
	const u32 width = rect.width();
	const u32 height = rect.height();
	const u32 pitch = width * Vulkan::Util::GetTexelSize(static_cast<GSTextureVK*>(src)->GetNativeFormat());
	const u32 size = pitch * height;
	const u32 level = 0;
	if (!CheckStagingBufferSize(size))
	{
		Console.Error("Can't read back %ux%u", width, height);
		return false;
	}

	g_perfmon.Put(GSPerfMon::Readbacks, 1);
	EndRenderPass();
	{
		const VkCommandBuffer cmdbuf = g_vulkan_context->GetCurrentCommandBuffer();
		GL_INS("ReadbackTexture: {%d,%d} %ux%u", rect.left, rect.top, width, height);

		GSTextureVK* vkSrc = static_cast<GSTextureVK*>(src);
		VkImageLayout old_layout = vkSrc->GetTexture().GetLayout();
		if (old_layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
			vkSrc->GetTexture().TransitionSubresourcesToLayout(
				cmdbuf, level, 1, 0, 1, old_layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		VkBufferImageCopy image_copy = {};
		const VkImageAspectFlags aspect = Vulkan::Util::IsDepthFormat(static_cast<VkFormat>(vkSrc->GetFormat())) ?
                                              VK_IMAGE_ASPECT_DEPTH_BIT :
                                              VK_IMAGE_ASPECT_COLOR_BIT;
		image_copy.bufferOffset = 0;
		image_copy.bufferRowLength = width;
		image_copy.bufferImageHeight = 0;
		image_copy.imageSubresource = {aspect, level, 0u, 1u};
		image_copy.imageOffset = {rect.left, rect.top, 0};
		image_copy.imageExtent = {width, height, 1u};

		// invalidate gpu cache
		// TODO: Needed?
		Vulkan::Util::BufferMemoryBarrier(cmdbuf, m_readback_staging_buffer, 0, VK_ACCESS_TRANSFER_WRITE_BIT, 0, size,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

		// do the copy
		vkCmdCopyImageToBuffer(cmdbuf, vkSrc->GetTexture().GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			m_readback_staging_buffer, 1, &image_copy);

		// flush gpu cache
		Vulkan::Util::BufferMemoryBarrier(cmdbuf, m_readback_staging_buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_HOST_READ_BIT, 0, size, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT);

		if (old_layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			vkSrc->GetTexture().TransitionSubresourcesToLayout(
				cmdbuf, level, 1, 0, 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, old_layout);
		}
	}

	ExecuteCommandBuffer(true);

	// invalidate cpu cache before reading
	VkResult res = vmaInvalidateAllocation(g_vulkan_context->GetAllocator(), m_readback_staging_allocation, 0, size);
	if (res != VK_SUCCESS)
		LOG_VULKAN_ERROR(res, "vmaInvalidateAllocation() failed, readback may be incorrect: ");

	out_map.bits = reinterpret_cast<u8*>(m_readback_staging_buffer_map);
	out_map.pitch = pitch;

	return true;
}

void GSDeviceVK::DownloadTextureComplete() {}

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
			else
			{
				// otherwise we need to do an attachment clear
				const bool depth = (dTexVK->GetType() == GSTexture::Type::DepthStencil);
				OMSetRenderTargets(depth ? nullptr : dTexVK, depth ? dTexVK : nullptr, dtex_rc, false);
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

	dTexVK->TransitionToLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	sTexVK->TransitionToLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	sTexVK->SetUsedThisCommandBuffer();

	vkCmdCopyImage(g_vulkan_context->GetCurrentCommandBuffer(), sTexVK->GetImage(),
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dTexVK->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &ic);

	dTexVK->SetState(GSTexture::State::Dirty);
}

void GSDeviceVK::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect,
	ShaderConvert shader /* = ShaderConvert::COPY */, bool linear /* = true */)
{
	pxAssert(IsDepthConvertShader(shader) == (dTex && dTex->GetType() == GSTexture::Type::DepthStencil));

	GL_INS("StretchRect(%d) {%d,%d} %dx%d -> {%d,%d) %dx%d", shader, int(sRect.left), int(sRect.top),
		int(sRect.right - sRect.left), int(sRect.bottom - sRect.top), int(dRect.left), int(dRect.top),
		int(dRect.right - dRect.left), int(dRect.bottom - dRect.top));

	DoStretchRect(static_cast<GSTextureVK*>(sTex), sRect, static_cast<GSTextureVK*>(dTex), dRect,
		dTex ? m_convert[static_cast<int>(shader)] : m_present[static_cast<int>(shader)], linear);
}

void GSDeviceVK::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, bool red,
	bool green, bool blue, bool alpha)
{
	GL_PUSH("ColorCopy Red:%d Green:%d Blue:%d Alpha:%d", red, green, blue, alpha);

	const u32 index = (red ? 1 : 0) | (green ? 2 : 0) | (blue ? 4 : 0) | (alpha ? 8 : 0);
	DoStretchRect(
		static_cast<GSTextureVK*>(sTex), sRect, static_cast<GSTextureVK*>(dTex), dRect, m_color_copy[index], false);
}

void GSDeviceVK::BeginRenderPassForStretchRect(GSTextureVK* dTex, const GSVector4i& dtex_rc, const GSVector4i& dst_rc)
{
	const bool is_whole_target = dst_rc.eq(dtex_rc);
	const VkAttachmentLoadOp load_op =
		is_whole_target ? VK_ATTACHMENT_LOAD_OP_DONT_CARE : GetLoadOpForTexture(dTex);
	dTex->SetState(GSTexture::State::Dirty);

	if (dTex->GetType() == GSTexture::Type::DepthStencil)
	{
		if (load_op == VK_ATTACHMENT_LOAD_OP_CLEAR)
			BeginClearRenderPass(m_utility_depth_render_pass_clear, dtex_rc, dTex->GetClearDepth(), 0);
		else
			BeginRenderPass((load_op == VK_ATTACHMENT_LOAD_OP_DONT_CARE) ? m_utility_depth_render_pass_discard :
                                                                           m_utility_depth_render_pass_load,
				dst_rc);
	}
	else if (dTex->GetFormat() == GSTexture::Format::Color)
	{
		if (load_op == VK_ATTACHMENT_LOAD_OP_CLEAR)
			BeginClearRenderPass(m_utility_color_render_pass_clear, dtex_rc, dTex->GetClearColor());
		else
			BeginRenderPass((load_op == VK_ATTACHMENT_LOAD_OP_DONT_CARE) ? m_utility_color_render_pass_discard :
                                                                           m_utility_color_render_pass_load,
				dst_rc);
	}
	else
	{
		// integer formats, etc
		const VkRenderPass rp = g_vulkan_context->GetRenderPass(dTex->GetNativeFormat(), VK_FORMAT_UNDEFINED,
			load_op, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE);
		if (load_op == VK_ATTACHMENT_LOAD_OP_CLEAR)
		{
			BeginClearRenderPass(rp, dtex_rc, dTex->GetClearColor());
		}
		else
		{
			BeginRenderPass(rp, dst_rc);
		}
	}
}

void GSDeviceVK::DoStretchRect(GSTextureVK* sTex, const GSVector4& sRect, GSTextureVK* dTex, const GSVector4& dRect,
	VkPipeline pipeline, bool linear)
{
	if (sTex->GetLayout() != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		// can't transition in a render pass
		EndRenderPass();
		sTex->TransitionToLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	SetUtilityTexture(sTex, linear ? m_linear_sampler : m_point_sampler);
	SetPipeline(pipeline);

	const bool is_present = (!dTex);
	const bool depth = (dTex && dTex->GetType() == GSTexture::Type::DepthStencil);
	const GSVector2i size(
		is_present ? GSVector2i(m_display->GetWindowWidth(), m_display->GetWindowHeight()) : dTex->GetSize());
	const GSVector4i dtex_rc(0, 0, size.x, size.y);
	const GSVector4i dst_rc(GSVector4i(dRect).rintersect(dtex_rc));

	// switch rts (which might not end the render pass), so check the bounds
	if (!is_present)
	{
		OMSetRenderTargets(depth ? nullptr : dTex, depth ? dTex : nullptr, dst_rc, false);
		if (InRenderPass() && !CheckRenderPassArea(dst_rc))
			EndRenderPass();
	}
	else
	{
		// this is for presenting, we don't want to screw with the viewport/scissor set by display
		m_dirty_flags &= ~(DIRTY_FLAG_VIEWPORT | DIRTY_FLAG_SCISSOR);
	}

	const bool drawing_to_current_rt = (is_present || InRenderPass());
	if (!drawing_to_current_rt)
		BeginRenderPassForStretchRect(dTex, dtex_rc, dst_rc);

	DrawStretchRect(sRect, dRect, size);

	if (!drawing_to_current_rt)
	{
		EndRenderPass();
		static_cast<GSTextureVK*>(dTex)->TransitionToLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}
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

	//const VkImageLayout old_src_layout = sTexVK->GetTexture().GetLayout();
	//const VkImageLayout old_dst_layout = dTexVK->GetTexture().GetLayout();

	EndRenderPass();

	sTexVK->TransitionToLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	dTexVK->TransitionToLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	pxAssert(
		(sTexVK->GetType() == GSTexture::Type::DepthStencil) == (dTexVK->GetType() == GSTexture::Type::DepthStencil));
	const VkImageAspectFlags aspect =
		(sTexVK->GetType() == GSTexture::Type::DepthStencil) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	const VkImageBlit ib{{aspect, sLevel, 0u, 1u}, {{sRect.left, sRect.top, 0}, {sRect.right, sRect.bottom, 1}},
		{aspect, dLevel, 0u, 1u}, {{dRect.left, dRect.top, 0}, {dRect.right, dRect.bottom, 1}}};

	vkCmdBlitImage(g_vulkan_context->GetCurrentCommandBuffer(), sTexVK->GetTexture().GetImage(),
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dTexVK->GetTexture().GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
		&ib, linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST);
}

void GSDeviceVK::DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect,
	const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, const GSVector4& c)
{
	GL_PUSH("DoMerge");

	const GSVector4 full_r(0.0f, 0.0f, 1.0f, 1.0f);
	const u32 yuv_constants[4] = {EXTBUF.EMODA, EXTBUF.EMODC};
	const bool feedback_write_2 = PMODE.EN2 && sTex[2] != nullptr && EXTBUF.FBIN == 1;
	const bool feedback_write_1 = PMODE.EN1 && sTex[2] != nullptr && EXTBUF.FBIN == 0;
	const bool feedback_write_2_but_blend_bg = feedback_write_2 && PMODE.SLBG == 1;

	// Merge the 2 source textures (sTex[0],sTex[1]). Final results go to dTex. Feedback write will go to sTex[2].
	// If either 2nd output is disabled or SLBG is 1, a background color will be used.
	// Note: background color is also used when outside of the unit rectangle area
	EndRenderPass();

	// transition everything before starting the new render pass
	static_cast<GSTextureVK*>(dTex)->TransitionToLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	if (sTex[0])
		static_cast<GSTextureVK*>(sTex[0])->TransitionToLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	const GSVector2i dsize(dTex->GetSize());
	const GSVector4i darea(0, 0, dsize.x, dsize.y);
	bool dcleared = false;
	if (sTex[1] && (PMODE.SLBG == 0 || feedback_write_2_but_blend_bg))
	{
		// 2nd output is enabled and selected. Copy it to destination so we can blend it with 1st output
		// Note: value outside of dRect must contains the background color (c)
		if (sTex[1]->GetState() == GSTexture::State::Dirty)
		{
			static_cast<GSTextureVK*>(sTex[1])->TransitionToLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			OMSetRenderTargets(dTex, nullptr, darea, false);
			SetUtilityTexture(sTex[1], m_linear_sampler);
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
		OMSetRenderTargets(sTex[2], nullptr, fbarea, false);
		if (dcleared)
			SetUtilityTexture(dTex, m_linear_sampler);
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
			static_cast<GSTextureVK*>(sTex[2])->TransitionToLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
	}

	// Restore background color to process the normal merge
	if (feedback_write_2_but_blend_bg || !dcleared)
	{
		EndRenderPass();
		OMSetRenderTargets(dTex, nullptr, darea, false);
		BeginClearRenderPass(m_utility_color_render_pass_clear, darea, c);
	}
	else if (!InRenderPass())
	{
		OMSetRenderTargets(dTex, nullptr, darea, false);
		BeginRenderPass(m_utility_color_render_pass_load, darea);
	}

	if (sTex[0] && sTex[0]->GetState() == GSTexture::State::Dirty)
	{
		// 1st output is enabled. It must be blended
		SetUtilityTexture(sTex[0], m_linear_sampler);
		SetPipeline(m_merge[PMODE.MMOD]);
		SetUtilityPushConstants(&c, sizeof(c));
		DrawStretchRect(sRect[0], dRect[0], dTex->GetSize());
	}

	if (feedback_write_1)
	{
		EndRenderPass();
		SetPipeline(m_convert[static_cast<int>(ShaderConvert::YUV)]);
		SetUtilityTexture(dTex, m_linear_sampler);
		SetUtilityPushConstants(yuv_constants, sizeof(yuv_constants));
		OMSetRenderTargets(sTex[2], nullptr, fbarea, false);
		BeginRenderPass(m_utility_color_render_pass_load, fbarea);
		DrawStretchRect(full_r, dRect[2], dsize);
	}

	EndRenderPass();

	// this texture is going to get used as an input, so make sure we don't read undefined data
	static_cast<GSTextureVK*>(dTex)->CommitClear();
	static_cast<GSTextureVK*>(dTex)->TransitionToLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void GSDeviceVK::DoInterlace(GSTexture* sTex, GSTexture* dTex, int shader, bool linear, float yoffset)
{
	const GSVector2i size(dTex->GetSize());
	const GSVector4 s = GSVector4(size);

	const GSVector4 sRect(0, 0, 1, 1);
	const GSVector4 dRect(0.0f, yoffset, s.x, s.y + yoffset);

	InterlaceConstantBuffer cb;
	cb.ZrH = GSVector2(0, 1.0f / s.y);

	GL_PUSH("DoInterlace %dx%d Shader:%d Linear:%d", size.x, size.y, shader, linear);

	static_cast<GSTextureVK*>(dTex)->TransitionToLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	const GSVector4i rc(0, 0, size.x, size.y);
	EndRenderPass();
	OMSetRenderTargets(dTex, nullptr, rc, false);
	SetUtilityTexture(sTex, linear ? m_linear_sampler : m_point_sampler);
	BeginRenderPass(m_utility_color_render_pass_load, rc);
	SetPipeline(m_interlace[shader]);
	SetUtilityPushConstants(&cb, sizeof(cb));
	DrawStretchRect(sRect, dRect, dTex->GetSize());
	EndRenderPass();

	// this texture is going to get used as an input, so make sure we don't read undefined data
	static_cast<GSTextureVK*>(dTex)->CommitClear();
	static_cast<GSTextureVK*>(dTex)->TransitionToLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void GSDeviceVK::DoShadeBoost(GSTexture* sTex, GSTexture* dTex, const float params[4])
{
	const GSVector4 sRect(0.0f, 0.0f, 1.0f, 1.0f);
	const GSVector4i dRect(0, 0, dTex->GetWidth(), dTex->GetHeight());
	EndRenderPass();
	OMSetRenderTargets(dTex, nullptr, dRect, false);
	SetUtilityTexture(sTex, m_point_sampler);
	BeginRenderPass(m_utility_color_render_pass_discard, dRect);
	SetPipeline(m_shadeboost_pipeline);
	SetUtilityPushConstants(params, sizeof(float) * 4);
	DrawStretchRect(sRect, GSVector4(dRect), dTex->GetSize());
	EndRenderPass();

	static_cast<GSTextureVK*>(dTex)->TransitionToLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void GSDeviceVK::DoFXAA(GSTexture* sTex, GSTexture* dTex)
{
	const GSVector4 sRect(0.0f, 0.0f, 1.0f, 1.0f);
	const GSVector4i dRect(0, 0, dTex->GetWidth(), dTex->GetHeight());
	EndRenderPass();
	OMSetRenderTargets(dTex, nullptr, dRect, false);
	SetUtilityTexture(sTex, m_linear_sampler);
	BeginRenderPass(m_utility_color_render_pass_discard, dRect);
	SetPipeline(m_fxaa_pipeline);
	DrawStretchRect(sRect, GSVector4(dRect), dTex->GetSize());
	EndRenderPass();

	static_cast<GSTextureVK*>(dTex)->TransitionToLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void GSDeviceVK::IASetVertexBuffer(const void* vertex, size_t stride, size_t count)
{
	const u32 size = static_cast<u32>(stride) * static_cast<u32>(count);
	if (!m_vertex_stream_buffer.ReserveMemory(size, static_cast<u32>(stride)))
	{
		ExecuteCommandBufferAndRestartRenderPass("Uploading bytes to vertex buffer");
		if (!m_vertex_stream_buffer.ReserveMemory(size, static_cast<u32>(stride)))
			pxFailRel("Failed to reserve space for vertices");
	}

	m_vertex.start = m_vertex_stream_buffer.GetCurrentOffset() / stride;
	m_vertex.limit = count;
	m_vertex.stride = stride;
	m_vertex.count = count;
	SetVertexBuffer(m_vertex_stream_buffer.GetBuffer(), 0);

	GSVector4i::storent(m_vertex_stream_buffer.GetCurrentHostPointer(), vertex, count * stride);
	m_vertex_stream_buffer.CommitMemory(size);
}

bool GSDeviceVK::IAMapVertexBuffer(void** vertex, size_t stride, size_t count)
{
	const u32 size = static_cast<u32>(stride) * static_cast<u32>(count);
	if (!m_vertex_stream_buffer.ReserveMemory(size, static_cast<u32>(stride)))
	{
		ExecuteCommandBufferAndRestartRenderPass("Mapping bytes to vertex buffer");
		if (!m_vertex_stream_buffer.ReserveMemory(size, static_cast<u32>(stride)))
			pxFailRel("Failed to reserve space for vertices");
	}

	m_vertex.start = m_vertex_stream_buffer.GetCurrentOffset() / stride;
	m_vertex.limit = m_vertex_stream_buffer.GetCurrentSpace() / stride;
	m_vertex.stride = stride;
	m_vertex.count = count;
	SetVertexBuffer(m_vertex_stream_buffer.GetBuffer(), 0);

	*vertex = m_vertex_stream_buffer.GetCurrentHostPointer();
	return true;
}

void GSDeviceVK::IAUnmapVertexBuffer()
{
	const u32 size = static_cast<u32>(m_vertex.stride) * static_cast<u32>(m_vertex.count);
	m_vertex_stream_buffer.CommitMemory(size);
}

void GSDeviceVK::IASetIndexBuffer(const void* index, size_t count)
{
	const u32 size = sizeof(u32) * static_cast<u32>(count);
	if (!m_index_stream_buffer.ReserveMemory(size, sizeof(u32)))
	{
		ExecuteCommandBufferAndRestartRenderPass("Uploading bytes to index buffer");
		if (!m_index_stream_buffer.ReserveMemory(size, sizeof(u32)))
			pxFailRel("Failed to reserve space for vertices");
	}

	m_index.start = m_index_stream_buffer.GetCurrentOffset() / sizeof(u32);
	m_index.limit = count;
	m_index.count = count;
	SetIndexBuffer(m_index_stream_buffer.GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

	std::memcpy(m_index_stream_buffer.GetCurrentHostPointer(), index, size);
	m_index_stream_buffer.CommitMemory(size);
}

void GSDeviceVK::OMSetRenderTargets(GSTexture* rt, GSTexture* ds, const GSVector4i& scissor, bool feedback_loop)
{
	GSTextureVK* vkRt = static_cast<GSTextureVK*>(rt);
	GSTextureVK* vkDs = static_cast<GSTextureVK*>(ds);
	pxAssert(vkRt || vkDs);

	if (m_current_render_target != vkRt || m_current_depth_target != vkDs ||
		m_current_framebuffer_has_feedback_loop != feedback_loop)
	{
		// framebuffer change or feedback loop enabled/disabled
		EndRenderPass();

		if (vkRt)
			m_current_framebuffer = vkRt->GetLinkedFramebuffer(vkDs, feedback_loop);
		else
			m_current_framebuffer = vkDs->GetLinkedFramebuffer(nullptr, feedback_loop);
	}

	m_current_render_target = vkRt;
	m_current_depth_target = vkDs;
	m_current_framebuffer_has_feedback_loop = feedback_loop;

	if (!InRenderPass())
	{
		if (vkRt)
			vkRt->TransitionToLayout(
				feedback_loop ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		if (vkDs)
			vkDs->TransitionToLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
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

	const bool aniso = (ss.aniso && GSConfig.MaxAnisotropy > 1);

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
		ss.lodclamp ? 0.25f : VK_LOD_CLAMP_NONE, // max lod
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

	for (u32 i = 0; i < std::size(m_tfx_samplers); i++)
		m_tfx_samplers[i] = GetSampler(m_tfx_sampler_sel[i]);
}

static void AddMacro(std::stringstream& ss, const char* name, const char* value)
{
	ss << "#define " << name << " " << value << "\n";
}

static void AddMacro(std::stringstream& ss, const char* name, int value)
{
	ss << "#define " << name << " " << value << "\n";
}

static void AddShaderHeader(std::stringstream& ss)
{
	ss << "#version 460 core\n";
	ss << "#extension GL_EXT_samplerless_texture_functions : require\n";

	const GSDevice::FeatureSupport features(g_gs_device->Features());
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
	gpb.AddVertexAttribute(2, 0, VK_FORMAT_R8G8B8A8_UNORM, 28);
	gpb.SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
}

VkShaderModule GSDeviceVK::GetUtilityVertexShader(const std::string& source, const char* replace_main = nullptr)
{
	std::stringstream ss;
	AddShaderHeader(ss);
	AddShaderStageMacro(ss, true, false, false);
	AddMacro(ss, "PS_SCALE_FACTOR", GSConfig.UpscaleMultiplier);
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
	AddMacro(ss, "PS_SCALE_FACTOR", GSConfig.UpscaleMultiplier);
	if (replace_main)
		ss << "#define " << replace_main << " main\n";
	ss << source;

	return g_vulkan_shader_cache->GetFragmentShader(ss.str());
}

bool GSDeviceVK::CreateNullTexture()
{
	if (!m_null_texture.Create(1, 1, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_VIEW_TYPE_2D,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT))
	{
		return false;
	}

	const VkCommandBuffer cmdbuf = g_vulkan_context->GetCurrentCommandBuffer();
	const VkImageSubresourceRange srr{VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
	const VkClearColorValue ccv{};
	m_null_texture.TransitionToLayout(cmdbuf, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	vkCmdClearColorImage(cmdbuf, m_null_texture.GetImage(), m_null_texture.GetLayout(), &ccv, 1, &srr);
	m_null_texture.TransitionToLayout(cmdbuf, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	Vulkan::Util::SetObjectName(g_vulkan_context->GetDevice(), m_null_texture.GetImage(), "Null texture");
	Vulkan::Util::SetObjectName(g_vulkan_context->GetDevice(), m_null_texture.GetView(), "Null texture view");

	return true;
}

bool GSDeviceVK::CreateBuffers()
{
	if (!m_vertex_stream_buffer.Create(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VERTEX_BUFFER_SIZE))
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
	Vulkan::Util::SetObjectName(dev, m_utility_ds_layout, "Convert descriptor layout");

	plb.AddPushConstants(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, CONVERT_PUSH_CONSTANTS_SIZE);
	plb.AddDescriptorSet(m_utility_ds_layout);
	if ((m_utility_pipeline_layout = plb.Create(dev)) == VK_NULL_HANDLE)
		return false;
	Vulkan::Util::SetObjectName(dev, m_utility_ds_layout, "Convert pipeline layout");

	//////////////////////////////////////////////////////////////////////////
	// Draw/TFX Pipeline Layout
	//////////////////////////////////////////////////////////////////////////
	dslb.AddBinding(
		0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);
	dslb.AddBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	if ((m_tfx_ubo_ds_layout = dslb.Create(dev)) == VK_NULL_HANDLE)
		return false;
	Vulkan::Util::SetObjectName(dev, m_tfx_ubo_ds_layout, "TFX UBO descriptor layout");
	for (u32 i = 0; i < NUM_TFX_SAMPLERS; i++)
		dslb.AddBinding(i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	if ((m_tfx_sampler_ds_layout = dslb.Create(dev)) == VK_NULL_HANDLE)
		return false;
	Vulkan::Util::SetObjectName(dev, m_tfx_sampler_ds_layout, "TFX sampler descriptor layout");
	dslb.AddBinding(0, m_features.texture_barrier ? VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	dslb.AddBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	if ((m_tfx_rt_texture_ds_layout = dslb.Create(dev)) == VK_NULL_HANDLE)
		return false;
	Vulkan::Util::SetObjectName(dev, m_tfx_rt_texture_ds_layout, "TFX RT texture descriptor layout");

	plb.AddDescriptorSet(m_tfx_ubo_ds_layout);
	plb.AddDescriptorSet(m_tfx_sampler_ds_layout);
	plb.AddDescriptorSet(m_tfx_rt_texture_ds_layout);
	if ((m_tfx_pipeline_layout = plb.Create(dev)) == VK_NULL_HANDLE)
		return false;
	Vulkan::Util::SetObjectName(dev, m_tfx_pipeline_layout, "TFX pipeline layout");
	return true;
}

bool GSDeviceVK::CreateRenderPasses()
{
#define GET(dest, rt, depth, fbl, opa, opb, opc) \
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
			(fbl) /* feedback loop */ \
		); \
		if (dest == VK_NULL_HANDLE) \
			return false; \
	} while (0)

	const VkFormat rt_format = LookupNativeFormat(GSTexture::Format::Color);
	const VkFormat hdr_rt_format = LookupNativeFormat(GSTexture::Format::FloatColor);
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
						for (u32 opa = VK_ATTACHMENT_LOAD_OP_LOAD; opa <= VK_ATTACHMENT_LOAD_OP_DONT_CARE; opa++)
						{
							for (u32 opb = VK_ATTACHMENT_LOAD_OP_LOAD; opb <= VK_ATTACHMENT_LOAD_OP_DONT_CARE; opb++)
							{
								const VkFormat rp_rt_format =
									(rt != 0) ? ((hdr != 0) ? hdr_rt_format : rt_format) : VK_FORMAT_UNDEFINED;
								const VkFormat rp_depth_format = (ds != 0) ? depth_format : VK_FORMAT_UNDEFINED;
								const VkAttachmentLoadOp opc =
									((date == DATE_RENDER_PASS_NONE || !m_features.stencil_buffer) ?
                                            VK_ATTACHMENT_LOAD_OP_DONT_CARE :
                                            (date == DATE_RENDER_PASS_STENCIL_ONE ? VK_ATTACHMENT_LOAD_OP_CLEAR :
                                                                                    VK_ATTACHMENT_LOAD_OP_LOAD));
								GET(m_tfx_render_pass[rt][ds][hdr][date][fbl][opa][opb], rp_rt_format, rp_depth_format,
									(fbl != 0), static_cast<VkAttachmentLoadOp>(opa),
									static_cast<VkAttachmentLoadOp>(opb), static_cast<VkAttachmentLoadOp>(opc));
							}
						}
					}
				}
			}
		}
	}

	GET(m_utility_color_render_pass_load, rt_format, VK_FORMAT_UNDEFINED, false, VK_ATTACHMENT_LOAD_OP_LOAD,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
	GET(m_utility_color_render_pass_clear, rt_format, VK_FORMAT_UNDEFINED, false, VK_ATTACHMENT_LOAD_OP_CLEAR,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
	GET(m_utility_color_render_pass_discard, rt_format, VK_FORMAT_UNDEFINED, false, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
	GET(m_utility_depth_render_pass_load, VK_FORMAT_UNDEFINED, depth_format, false, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
	GET(m_utility_depth_render_pass_clear, VK_FORMAT_UNDEFINED, depth_format, false, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
	GET(m_utility_depth_render_pass_discard, VK_FORMAT_UNDEFINED, depth_format, false, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE);

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
	// we may not have a swap chain if running in headless mode.
	Vulkan::SwapChain* swapchain = static_cast<Vulkan::SwapChain*>(m_display->GetRenderSurface());
	if (swapchain)
	{
		m_swap_chain_render_pass =
			g_vulkan_context->GetRenderPass(swapchain->GetSurfaceFormat().format, VK_FORMAT_UNDEFINED);
		if (!m_swap_chain_render_pass)
			return false;
	}

	std::optional<std::string> shader = Host::ReadResourceFileToString("shaders/vulkan/convert.glsl");
	if (!shader)
	{
		Host::ReportErrorAsync("GS", "Failed to read shaders/vulkan/convert.glsl.");
		return false;
	}

	VkShaderModule vs = GetUtilityVertexShader(*shader);
	if (vs == VK_NULL_HANDLE)
		return false;
	ScopedGuard vs_guard([&vs]() { Vulkan::Util::SafeDestroyShaderModule(vs); });

	Vulkan::GraphicsPipelineBuilder gpb;
	AddUtilityVertexAttributes(gpb);
	gpb.SetPipelineLayout(m_utility_pipeline_layout);
	gpb.SetDynamicViewportAndScissorState();
	gpb.AddDynamicState(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
	gpb.SetNoCullRasterizationState();
	gpb.SetNoBlendingState();
	gpb.SetVertexShader(vs);

	// we enable provoking vertex here anyway, in case it doesn't support multiple modes in the same pass
	if (m_features.provoking_vertex_last)
		gpb.SetProvokingVertex(VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT);

	for (ShaderConvert i = ShaderConvert::COPY; static_cast<int>(i) < static_cast<int>(ShaderConvert::Count);
		 i = static_cast<ShaderConvert>(static_cast<int>(i) + 1))
	{
		const bool depth = IsDepthConvertShader(i);
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

		ScopedGuard ps_guard([&ps]() { Vulkan::Util::SafeDestroyShaderModule(ps); });
		gpb.SetFragmentShader(ps);

		m_convert[index] =
			gpb.Create(g_vulkan_context->GetDevice(), g_vulkan_shader_cache->GetPipelineCache(true), false);
		if (!m_convert[index])
			return false;

		Vulkan::Util::SetObjectName(g_vulkan_context->GetDevice(), m_convert[index], "Convert pipeline %d", i);

		if (swapchain && IsPresentConvertShader(i))
		{
			// compile a present variant too
			gpb.SetRenderPass(m_swap_chain_render_pass, 0);
			m_present[index] =
				gpb.Create(g_vulkan_context->GetDevice(), g_vulkan_shader_cache->GetPipelineCache(true), false);
			if (!m_present[index])
				return false;

			Vulkan::Util::SetObjectName(
				g_vulkan_context->GetDevice(), m_present[index], "Convert pipeline %d (Present)", i);
		}

		if (i == ShaderConvert::COPY)
		{
			// compile the variant for setting up hdr rendering
			for (u32 ds = 0; ds < 2; ds++)
			{
				for (u32 fbl = 0; fbl < 2; fbl++)
				{
					pxAssert(!m_hdr_setup_pipelines[ds][fbl]);

					gpb.SetRenderPass(GetTFXRenderPass(true, ds != 0, true, DATE_RENDER_PASS_NONE, fbl != 0,
										  VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE),
						0);
					m_hdr_setup_pipelines[ds][fbl] =
						gpb.Create(g_vulkan_context->GetDevice(), g_vulkan_shader_cache->GetPipelineCache(true), false);
					if (!m_hdr_setup_pipelines[ds][fbl])
						return false;

					Vulkan::Util::SetObjectName(g_vulkan_context->GetDevice(), m_hdr_setup_pipelines[ds][fbl],
						"HDR setup/copy pipeline (ds=%u, fbl=%u)", i, ds, fbl);
				}
			}

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

				Vulkan::Util::SetObjectName(g_vulkan_context->GetDevice(), m_color_copy[i],
					"Color copy pipeline (r=%u, g=%u, b=%u, a=%u)", i & 1u, (i >> 1) & 1u, (i >> 2) & 1u,
					(i >> 3) & 1u);
			}
		}
		else if (i == ShaderConvert::MOD_256)
		{
			for (u32 ds = 0; ds < 2; ds++)
			{
				for (u32 fbl = 0; fbl < 2; fbl++)
				{
					pxAssert(!m_hdr_finish_pipelines[ds][fbl]);

					gpb.SetRenderPass(GetTFXRenderPass(true, ds != 0, false, DATE_RENDER_PASS_NONE, fbl != 0,
										  VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE),
						0);
					m_hdr_finish_pipelines[ds][fbl] =
						gpb.Create(g_vulkan_context->GetDevice(), g_vulkan_shader_cache->GetPipelineCache(true), false);
					if (!m_hdr_finish_pipelines[ds][fbl])
						return false;

					Vulkan::Util::SetObjectName(g_vulkan_context->GetDevice(), m_hdr_setup_pipelines[ds][fbl],
						"HDR finish/copy pipeline (ds=%u, fbl=%u)", i, ds, fbl);
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
					VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
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

		ScopedGuard ps_guard([&ps]() { Vulkan::Util::SafeDestroyShaderModule(ps); });
		gpb.SetPipelineLayout(m_utility_pipeline_layout);
		gpb.SetFragmentShader(ps);
		gpb.SetNoDepthTestState();
		gpb.SetNoStencilState();
		gpb.ClearBlendAttachments();
		gpb.SetBlendAttachment(0, true, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_MIN,
			VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_COLOR_COMPONENT_R_BIT);

		for (u32 ds = 0; ds < 2; ds++)
		{
			gpb.SetRenderPass(m_date_image_setup_render_passes[ds][0], 0);
			m_date_image_setup_pipelines[ds][datm] =
				gpb.Create(g_vulkan_context->GetDevice(), g_vulkan_shader_cache->GetPipelineCache(true), false);
			if (!m_date_image_setup_pipelines[ds][datm])
				return false;

			Vulkan::Util::SetObjectName(g_vulkan_context->GetDevice(), m_date_image_setup_pipelines[ds][datm],
				"DATE image clear pipeline (ds=%u, datm=%u)", ds, datm);
		}
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
	ScopedGuard vs_guard([&vs]() { Vulkan::Util::SafeDestroyShaderModule(vs); });

	Vulkan::GraphicsPipelineBuilder gpb;
	AddUtilityVertexAttributes(gpb);
	gpb.SetPipelineLayout(m_utility_pipeline_layout);
	gpb.SetDynamicViewportAndScissorState();
	gpb.AddDynamicState(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
	gpb.SetNoCullRasterizationState();
	gpb.SetNoDepthTestState();
	gpb.SetNoBlendingState();
	gpb.SetRenderPass(rp, 0);
	gpb.SetVertexShader(vs);

	// we enable provoking vertex here anyway, in case it doesn't support multiple modes in the same pass
	if (m_features.provoking_vertex_last)
		gpb.SetProvokingVertex(VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT);

	for (int i = 0; i < static_cast<int>(m_interlace.size()); i++)
	{
		VkShaderModule ps = GetUtilityFragmentShader(*shader, StringUtil::StdStringFromFormat("ps_main%d", i).c_str());
		if (ps == VK_NULL_HANDLE)
			return false;

		gpb.SetFragmentShader(ps);

		m_interlace[i] =
			gpb.Create(g_vulkan_context->GetDevice(), g_vulkan_shader_cache->GetPipelineCache(true), false);
		Vulkan::Util::SafeDestroyShaderModule(ps);
		if (!m_interlace[i])
			return false;

		Vulkan::Util::SetObjectName(g_vulkan_context->GetDevice(), m_convert[i], "Interlace pipeline %d", i);
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
	ScopedGuard vs_guard([&vs]() { Vulkan::Util::SafeDestroyShaderModule(vs); });

	Vulkan::GraphicsPipelineBuilder gpb;
	AddUtilityVertexAttributes(gpb);
	gpb.SetPipelineLayout(m_utility_pipeline_layout);
	gpb.SetDynamicViewportAndScissorState();
	gpb.AddDynamicState(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
	gpb.SetNoCullRasterizationState();
	gpb.SetNoDepthTestState();
	gpb.SetRenderPass(rp, 0);
	gpb.SetVertexShader(vs);

	// we enable provoking vertex here anyway, in case it doesn't support multiple modes in the same pass
	if (m_features.provoking_vertex_last)
		gpb.SetProvokingVertex(VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT);

	for (int i = 0; i < static_cast<int>(m_merge.size()); i++)
	{
		VkShaderModule ps = GetUtilityFragmentShader(*shader, StringUtil::StdStringFromFormat("ps_main%d", i).c_str());
		if (ps == VK_NULL_HANDLE)
			return false;

		gpb.SetFragmentShader(ps);
		gpb.SetBlendAttachment(0, true, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD);

		m_merge[i] = gpb.Create(g_vulkan_context->GetDevice(), g_vulkan_shader_cache->GetPipelineCache(true), false);
		Vulkan::Util::SafeDestroyShaderModule(ps);
		if (!m_merge[i])
			return false;

		Vulkan::Util::SetObjectName(g_vulkan_context->GetDevice(), m_convert[i], "Merge pipeline %d", i);
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
	AddUtilityVertexAttributes(gpb);
	gpb.SetPipelineLayout(m_utility_pipeline_layout);
	gpb.SetDynamicViewportAndScissorState();
	gpb.AddDynamicState(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
	gpb.SetNoCullRasterizationState();
	gpb.SetNoDepthTestState();
	gpb.SetNoBlendingState();
	gpb.SetRenderPass(rp, 0);

	// we enable provoking vertex here anyway, in case it doesn't support multiple modes in the same pass
	if (m_features.provoking_vertex_last)
		gpb.SetProvokingVertex(VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT);

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
			Vulkan::Util::SafeDestroyShaderModule(vs);
			Vulkan::Util::SafeDestroyShaderModule(ps);
		});
		if (vs == VK_NULL_HANDLE || ps == VK_NULL_HANDLE)
			return false;

		gpb.SetVertexShader(vs);
		gpb.SetFragmentShader(ps);

		m_fxaa_pipeline = gpb.Create(g_vulkan_context->GetDevice(), g_vulkan_shader_cache->GetPipelineCache(true), false);
		if (!m_fxaa_pipeline)
			return false;

		Vulkan::Util::SetObjectName(g_vulkan_context->GetDevice(), m_fxaa_pipeline, "FXAA pipeline");
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
			Vulkan::Util::SafeDestroyShaderModule(vs);
			Vulkan::Util::SafeDestroyShaderModule(ps);
		});
		if (vs == VK_NULL_HANDLE || ps == VK_NULL_HANDLE)
			return false;

		gpb.SetVertexShader(vs);
		gpb.SetFragmentShader(ps);

		m_shadeboost_pipeline = gpb.Create(g_vulkan_context->GetDevice(), g_vulkan_shader_cache->GetPipelineCache(true), false);
		if (!m_shadeboost_pipeline)
			return false;

		Vulkan::Util::SetObjectName(g_vulkan_context->GetDevice(), m_shadeboost_pipeline, "Shadeboost pipeline");
	}

	return true;
}

bool GSDeviceVK::CheckStagingBufferSize(u32 required_size)
{
	if (m_readback_staging_buffer_size >= required_size)
		return true;

	DestroyStagingBuffer();

	const VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0u, required_size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, 0u, nullptr};

	VmaAllocationCreateInfo aci = {};
	aci.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
	aci.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
	aci.preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

	VmaAllocationInfo ai = {};
	VkResult res = vmaCreateBuffer(
		g_vulkan_context->GetAllocator(), &bci, &aci, &m_readback_staging_buffer, &m_readback_staging_allocation, &ai);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vmaCreateBuffer() failed: ");
		return false;
	}

	m_readback_staging_buffer_map = ai.pMappedData;
	return true;
}

void GSDeviceVK::DestroyStagingBuffer()
{
	// unmapped as part of the buffer destroy
	m_readback_staging_buffer_map = nullptr;
	m_readback_staging_buffer_size = 0;

	if (m_readback_staging_buffer != VK_NULL_HANDLE)
	{
		vmaDestroyBuffer(g_vulkan_context->GetAllocator(), m_readback_staging_buffer, m_readback_staging_allocation);
		m_readback_staging_buffer = VK_NULL_HANDLE;
		m_readback_staging_allocation = VK_NULL_HANDLE;
		m_readback_staging_buffer_size = 0;
	}
}

void GSDeviceVK::DestroyResources()
{
	g_vulkan_context->ExecuteCommandBuffer(true);
	if (m_tfx_descriptor_sets[0] != VK_NULL_HANDLE)
		g_vulkan_context->FreeGlobalDescriptorSet(m_tfx_descriptor_sets[0]);

	for (auto& it : m_tfx_pipelines)
		Vulkan::Util::SafeDestroyPipeline(it.second);
	for (auto& it : m_tfx_fragment_shaders)
		Vulkan::Util::SafeDestroyShaderModule(it.second);
	for (auto& it : m_tfx_geometry_shaders)
		Vulkan::Util::SafeDestroyShaderModule(it.second);
	for (auto& it : m_tfx_vertex_shaders)
		Vulkan::Util::SafeDestroyShaderModule(it.second);
	for (VkPipeline& it : m_interlace)
		Vulkan::Util::SafeDestroyPipeline(it);
	for (VkPipeline& it : m_merge)
		Vulkan::Util::SafeDestroyPipeline(it);
	for (VkPipeline& it : m_color_copy)
		Vulkan::Util::SafeDestroyPipeline(it);
	for (VkPipeline& it : m_present)
		Vulkan::Util::SafeDestroyPipeline(it);
	for (VkPipeline& it : m_convert)
		Vulkan::Util::SafeDestroyPipeline(it);
	for (u32 ds = 0; ds < 2; ds++)
	{
		for (u32 fbl = 0; fbl < 2; fbl++)
		{
			Vulkan::Util::SafeDestroyPipeline(m_hdr_setup_pipelines[ds][fbl]);
			Vulkan::Util::SafeDestroyPipeline(m_hdr_finish_pipelines[ds][fbl]);
		}
	}
	for (u32 ds = 0; ds < 2; ds++)
	{
		for (u32 datm = 0; datm < 2; datm++)
		{
			Vulkan::Util::SafeDestroyPipeline(m_date_image_setup_pipelines[ds][datm]);
		}
	}
	Vulkan::Util::SafeDestroyPipeline(m_fxaa_pipeline);
	Vulkan::Util::SafeDestroyPipeline(m_shadeboost_pipeline);

	for (auto& it : m_samplers)
		Vulkan::Util::SafeDestroySampler(it.second);

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

	DestroyStagingBuffer();

	m_fragment_uniform_stream_buffer.Destroy(false);
	m_vertex_uniform_stream_buffer.Destroy(false);
	m_index_stream_buffer.Destroy(false);
	m_vertex_stream_buffer.Destroy(false);

	Vulkan::Util::SafeDestroyPipelineLayout(m_tfx_pipeline_layout);
	Vulkan::Util::SafeDestroyDescriptorSetLayout(m_tfx_rt_texture_ds_layout);
	Vulkan::Util::SafeDestroyDescriptorSetLayout(m_tfx_sampler_ds_layout);
	Vulkan::Util::SafeDestroyDescriptorSetLayout(m_tfx_ubo_ds_layout);
	Vulkan::Util::SafeDestroyPipelineLayout(m_utility_pipeline_layout);
	Vulkan::Util::SafeDestroyDescriptorSetLayout(m_utility_ds_layout);

	m_null_texture.Destroy(false);
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
	if (sel.point_size)
		AddMacro(ss, "VS_POINT_SIZE_VALUE", GSConfig.UpscaleMultiplier);
	ss << m_tfx_source;

	VkShaderModule mod = g_vulkan_shader_cache->GetVertexShader(ss.str());
	if (mod)
		Vulkan::Util::SetObjectName(g_vulkan_context->GetDevice(), mod, "TFX Vertex %08X", sel.key);

	m_tfx_vertex_shaders.emplace(sel.key, mod);
	return mod;
}

VkShaderModule GSDeviceVK::GetTFXGeometryShader(GSHWDrawConfig::GSSelector sel)
{
	const auto it = m_tfx_geometry_shaders.find(sel.key);
	if (it != m_tfx_geometry_shaders.end())
		return it->second;

	std::stringstream ss;
	AddShaderHeader(ss);
	AddShaderStageMacro(ss, false, true, false);
	AddMacro(ss, "GS_IIP", sel.iip);
	AddMacro(ss, "GS_PRIM", static_cast<int>(sel.topology));
	AddMacro(ss, "GS_EXPAND", sel.expand);
	ss << m_tfx_source;

	VkShaderModule mod = g_vulkan_shader_cache->GetGeometryShader(ss.str());
	if (mod)
		Vulkan::Util::SetObjectName(g_vulkan_context->GetDevice(), mod, "TFX Geometry %08X", sel.key);

	m_tfx_geometry_shaders.emplace(sel.key, mod);
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
	AddMacro(ss, "PS_AEM_FMT", sel.aem_fmt);
	AddMacro(ss, "PS_PAL_FMT", sel.pal_fmt);
	AddMacro(ss, "PS_DFMT", sel.dfmt);
	AddMacro(ss, "PS_DEPTH_FMT", sel.depth_fmt);
	AddMacro(ss, "PS_CHANNEL_FETCH", sel.channel);
	AddMacro(ss, "PS_URBAN_CHAOS_HLE", sel.urban_chaos_hle);
	AddMacro(ss, "PS_TALES_OF_ABYSS_HLE", sel.tales_of_abyss_hle);
	AddMacro(ss, "PS_TEX_IS_FB", sel.tex_is_fb);
	AddMacro(ss, "PS_INVALID_TEX0", sel.invalid_tex0);
	AddMacro(ss, "PS_AEM", sel.aem);
	AddMacro(ss, "PS_TFX", sel.tfx);
	AddMacro(ss, "PS_TCC", sel.tcc);
	AddMacro(ss, "PS_ATST", sel.atst);
	AddMacro(ss, "PS_FOG", sel.fog);
	AddMacro(ss, "PS_CLR_HW", sel.clr_hw);
	AddMacro(ss, "PS_FBA", sel.fba);
	AddMacro(ss, "PS_LTF", sel.ltf);
	AddMacro(ss, "PS_AUTOMATIC_LOD", sel.automatic_lod);
	AddMacro(ss, "PS_MANUAL_LOD", sel.manual_lod);
	AddMacro(ss, "PS_COLCLIP", sel.colclip);
	AddMacro(ss, "PS_DATE", sel.date);
	AddMacro(ss, "PS_TCOFFSETHACK", sel.tcoffsethack);
	AddMacro(ss, "PS_POINT_SAMPLER", sel.point_sampler);
	AddMacro(ss, "PS_BLEND_A", sel.blend_a);
	AddMacro(ss, "PS_BLEND_B", sel.blend_b);
	AddMacro(ss, "PS_BLEND_C", sel.blend_c);
	AddMacro(ss, "PS_BLEND_D", sel.blend_d);
	AddMacro(ss, "PS_BLEND_MIX", sel.blend_mix);
	AddMacro(ss, "PS_IIP", sel.iip);
	AddMacro(ss, "PS_SHUFFLE", sel.shuffle);
	AddMacro(ss, "PS_READ_BA", sel.read_ba);
	AddMacro(ss, "PS_WRITE_RG", sel.write_rg);
	AddMacro(ss, "PS_FBMASK", sel.fbmask);
	AddMacro(ss, "PS_HDR", sel.hdr);
	AddMacro(ss, "PS_DITHER", sel.dither);
	AddMacro(ss, "PS_ZCLAMP", sel.zclamp);
	AddMacro(ss, "PS_PABE", sel.pabe);
	AddMacro(ss, "PS_SCANMSK", sel.scanmsk);
	AddMacro(ss, "PS_SCALE_FACTOR", GSConfig.UpscaleMultiplier);
	AddMacro(ss, "PS_TEX_IS_FB", sel.tex_is_fb);
	AddMacro(ss, "PS_NO_COLOR", sel.no_color);
	AddMacro(ss, "PS_NO_COLOR1", sel.no_color1);
	AddMacro(ss, "PS_NO_ABLEND", sel.no_ablend);
	AddMacro(ss, "PS_ONLY_ALPHA", sel.only_alpha);
	ss << m_tfx_source;

	VkShaderModule mod = g_vulkan_shader_cache->GetFragmentShader(ss.str());
	if (mod)
		Vulkan::Util::SetObjectName(g_vulkan_context->GetDevice(), mod, "TFX Fragment %" PRIX64 "%08X", sel.key_hi, sel.key_lo);

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
	VkShaderModule gs = p.gs.expand ? GetTFXGeometryShader(p.gs) : VK_NULL_HANDLE;
	VkShaderModule fs = GetTFXFragmentShader(pps);
	if (vs == VK_NULL_HANDLE || (p.gs.expand && gs == VK_NULL_HANDLE) || fs == VK_NULL_HANDLE)
		return VK_NULL_HANDLE;

	Vulkan::GraphicsPipelineBuilder gpb;

	// Common state
	gpb.SetPipelineLayout(m_tfx_pipeline_layout);
	if (p.ps.date >= 10)
	{
		// DATE image prepass
		gpb.SetRenderPass(m_date_image_setup_render_passes[p.ds][0], 0);
	}
	else
	{
		gpb.SetRenderPass(
			GetTFXRenderPass(p.rt, p.ds, p.ps.hdr, p.dss.date ? DATE_RENDER_PASS_STENCIL : DATE_RENDER_PASS_NONE,
				p.feedback_loop, p.rt ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				p.ds ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE),
			0);
	}
	gpb.SetPrimitiveTopology(topology_lookup[p.topology]);
	gpb.SetRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	if (p.line_width)
		gpb.SetLineWidth(static_cast<float>(GSConfig.UpscaleMultiplier));
	gpb.SetDynamicViewportAndScissorState();
	gpb.AddDynamicState(VK_DYNAMIC_STATE_BLEND_CONSTANTS);

	// Shaders
	gpb.SetVertexShader(vs);
	if (gs != VK_NULL_HANDLE)
		gpb.SetGeometryShader(gs);
	gpb.SetFragmentShader(fs);

	// IA
	gpb.AddVertexBuffer(0, sizeof(GSVertex));
	gpb.AddVertexAttribute(0, 0, VK_FORMAT_R32G32_SFLOAT, 0); // ST
	gpb.AddVertexAttribute(1, 0, VK_FORMAT_R8G8B8A8_UINT, 8); // RGBA
	gpb.AddVertexAttribute(2, 0, VK_FORMAT_R32_SFLOAT, 12); // Q
	gpb.AddVertexAttribute(3, 0, VK_FORMAT_R16G16_UINT, 16); // XY
	gpb.AddVertexAttribute(4, 0, VK_FORMAT_R32_UINT, 20); // Z
	gpb.AddVertexAttribute(5, 0, VK_FORMAT_R16G16_UINT, 24); // UV
	gpb.AddVertexAttribute(6, 0, VK_FORMAT_R8G8B8A8_UNORM, 28); // FOG

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
	if (p.ps.date >= 10)
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

	if (m_features.provoking_vertex_last)
		gpb.SetProvokingVertex(VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT);

	// Tests have shown that it's faster to just enable rast order on the entire pass, rather than alternating
	// between turning it on and off for different draws, and adding the required barrier between non-rast-order
	// and rast-order draws.
	if (m_features.framebuffer_fetch && p.feedback_loop)
		gpb.AddBlendFlags(VK_PIPELINE_COLOR_BLEND_STATE_CREATE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_BIT_ARM);

	VkPipeline pipeline = gpb.Create(g_vulkan_context->GetDevice(), g_vulkan_shader_cache->GetPipelineCache(true));
	if (pipeline)
	{
		Vulkan::Util::SetObjectName(
			g_vulkan_context->GetDevice(), pipeline, "TFX Pipeline %08X/%08X/%" PRIX64 "%08X", p.vs.key, p.gs.key, p.ps.key_hi, p.ps.key_lo);
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
	m_index_type = VK_INDEX_TYPE_UINT32;
	m_current_framebuffer = VK_NULL_HANDLE;
	m_current_render_pass = VK_NULL_HANDLE;

	for (u32 i = 0; i < NUM_TFX_TEXTURES; i++)
		m_tfx_textures[i] = m_null_texture.GetView();

	m_utility_texture = m_null_texture.GetView();

	m_point_sampler = GetSampler(GSHWDrawConfig::SamplerSelector::Point());
	if (m_point_sampler)
		Vulkan::Util::SetObjectName(g_vulkan_context->GetDevice(), m_point_sampler, "Point sampler");
	m_linear_sampler = GetSampler(GSHWDrawConfig::SamplerSelector::Linear());
	if (m_linear_sampler)
		Vulkan::Util::SetObjectName(g_vulkan_context->GetDevice(), m_point_sampler, "Linear sampler");

	for (u32 i = 0; i < NUM_TFX_SAMPLERS; i++)
	{
		m_tfx_sampler_sel[i] = GSHWDrawConfig::SamplerSelector::Point().key;
		m_tfx_samplers[i] = m_point_sampler;
	}

	InvalidateCachedState();
}

bool GSDeviceVK::CreatePersistentDescriptorSets()
{
	const VkDevice dev = g_vulkan_context->GetDevice();
	Vulkan::DescriptorSetUpdateBuilder dsub;

	// Allocate UBO descriptor sets for TFX.
	m_tfx_descriptor_sets[0] = g_vulkan_context->AllocatePersistentDescriptorSet(m_tfx_ubo_ds_layout);
	if (m_tfx_descriptor_sets[0] == VK_NULL_HANDLE)
		return false;
	dsub.AddBufferDescriptorWrite(m_tfx_descriptor_sets[0], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
		m_vertex_uniform_stream_buffer.GetBuffer(), 0, sizeof(GSHWDrawConfig::VSConstantBuffer));
	dsub.AddBufferDescriptorWrite(m_tfx_descriptor_sets[0], 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
		m_fragment_uniform_stream_buffer.GetBuffer(), 0, sizeof(GSHWDrawConfig::PSConstantBuffer));
	dsub.Update(dev);
	Vulkan::Util::SetObjectName(dev, m_tfx_descriptor_sets[0], "Persistent TFX UBO set");
	return true;
}

void GSDeviceVK::ExecuteCommandBuffer(bool wait_for_completion)
{
	EndRenderPass();
	g_vulkan_context->ExecuteCommandBuffer(wait_for_completion);
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

void GSDeviceVK::ExecuteCommandBufferAndRestartRenderPass(const char* reason)
{
	Console.Warning("Vulkan: Executing command buffer due to '%s'", reason);

	const VkRenderPass render_pass = m_current_render_pass;
	const GSVector4i render_pass_area(m_current_render_pass_area);
	EndRenderPass();
	g_vulkan_context->ExecuteCommandBuffer(false);
	InvalidateCachedState();

	if (render_pass != VK_NULL_HANDLE)
	{
		// rebind framebuffer
		ApplyBaseState(m_dirty_flags, g_vulkan_context->GetCurrentCommandBuffer());
		m_dirty_flags &= ~DIRTY_BASE_STATE;

		// restart render pass
		BeginRenderPass(render_pass, render_pass_area);
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
	m_current_pipeline_layout = PipelineLayout::Undefined;
	m_tfx_descriptor_sets[1] = VK_NULL_HANDLE;
	m_tfx_descriptor_sets[2] = VK_NULL_HANDLE;
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
	VkImageView view;
	if (sr)
	{
		GSTextureVK* vkTex = static_cast<GSTextureVK*>(sr);
		if (check_state)
		{
			if (vkTex->GetTexture().GetLayout() != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && InRenderPass())
			{
				// Console.Warning("Ending render pass due to resource transition");
				EndRenderPass();
			}

			vkTex->CommitClear();
			vkTex->TransitionToLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
		vkTex->SetUsedThisCommandBuffer();
		view = vkTex->GetView();
	}
	else
	{
		view = m_null_texture.GetView();
	}

	if (m_tfx_textures[i] == view)
		return;

	m_tfx_textures[i] = view;

	m_dirty_flags |= (i < 2) ? DIRTY_FLAG_TFX_SAMPLERS_DS : DIRTY_FLAG_TFX_RT_TEXTURE_DS;
}

void GSDeviceVK::PSSetSampler(u32 index, GSHWDrawConfig::SamplerSelector sel)
{
	if (m_tfx_sampler_sel[index] == sel.key)
		return;

	m_tfx_sampler_sel[index] = sel.key;
	m_tfx_samplers[index] = GetSampler(sel);
	m_dirty_flags |= DIRTY_FLAG_TFX_SAMPLERS_DS;
}

void GSDeviceVK::SetUtilityTexture(GSTexture* tex, VkSampler sampler)
{
	VkImageView view;
	if (tex)
	{
		GSTextureVK* vkTex = static_cast<GSTextureVK*>(tex);
		vkTex->CommitClear();
		vkTex->TransitionToLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		vkTex->SetUsedThisCommandBuffer();
		view = vkTex->GetView();
	}
	else
	{
		view = m_null_texture.GetView();
	}

	if (m_utility_texture == view && m_utility_sampler == sampler)
		return;

	m_utility_texture = view;
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
	const VkImageView view = tex->GetView();
	for (u32 i = 0; i < NUM_TFX_TEXTURES; i++)
	{
		if (m_tfx_textures[i] == view)
		{
			m_tfx_textures[i] = m_null_texture.GetView();
			m_dirty_flags |= (i < 2) ? DIRTY_FLAG_TFX_SAMPLERS_DS : DIRTY_FLAG_TFX_RT_TEXTURE_DS;
		}
	}
	if (m_utility_texture == view)
	{
		m_utility_texture = m_null_texture.GetView();
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

bool GSDeviceVK::CheckRenderPassArea(const GSVector4i& rect)
{
	if (!InRenderPass())
		return false;

	// TODO: Is there a way to do this with GSVector?
	if (rect.left < m_current_render_pass_area.left || rect.top < m_current_render_pass_area.top ||
		rect.right > m_current_render_pass_area.right || rect.bottom > m_current_render_pass_area.bottom)
	{
#ifdef _DEBUG
		Console.Error("RP check failed: {%d,%d %dx%d} vs {%d,%d %dx%d}", rect.left, rect.top, rect.width(),
			rect.height(), m_current_render_pass_area.left, m_current_render_pass_area.top,
			m_current_render_pass_area.width(), m_current_render_pass_area.height());
#endif
		return false;
	}

	return true;
}

void GSDeviceVK::EndRenderPass()
{
	if (m_current_render_pass == VK_NULL_HANDLE)
		return;

	vkCmdEndRenderPass(g_vulkan_context->GetCurrentCommandBuffer());

	m_current_render_pass = VK_NULL_HANDLE;
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
	m_dirty_flags &= ~DIRTY_TFX_STATE | DIRTY_CONSTANT_BUFFER_STATE;

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

			ExecuteCommandBufferAndRestartRenderPass("Ran out of vertex uniform space");
			return ApplyTFXState(true);
		}

		std::memcpy(m_vertex_uniform_stream_buffer.GetCurrentHostPointer(), &m_vs_cb_cache, sizeof(m_vs_cb_cache));
		m_tfx_dynamic_offsets[0] = m_vertex_uniform_stream_buffer.GetCurrentOffset();
		m_vertex_uniform_stream_buffer.CommitMemory(sizeof(m_vs_cb_cache));
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

			ExecuteCommandBufferAndRestartRenderPass("Ran out of pixel uniform space");
			return ApplyTFXState(true);
		}

		std::memcpy(m_fragment_uniform_stream_buffer.GetCurrentHostPointer(), &m_ps_cb_cache, sizeof(m_ps_cb_cache));
		m_tfx_dynamic_offsets[1] = m_fragment_uniform_stream_buffer.GetCurrentOffset();
		m_fragment_uniform_stream_buffer.CommitMemory(sizeof(m_ps_cb_cache));
	}

	Vulkan::DescriptorSetUpdateBuilder dsub;

	u32 dirty_descriptor_set_start = NUM_TFX_DESCRIPTOR_SETS;
	u32 dirty_descriptor_set_end = 0;

	if (flags & DIRTY_FLAG_TFX_DYNAMIC_OFFSETS)
	{
		dirty_descriptor_set_start = 0;
	}

	if ((flags & DIRTY_FLAG_TFX_SAMPLERS_DS) || m_tfx_descriptor_sets[1] == VK_NULL_HANDLE)
	{
		VkDescriptorSet ds = g_vulkan_context->AllocateDescriptorSet(m_tfx_sampler_ds_layout);
		if (ds == VK_NULL_HANDLE)
		{
			if (already_execed)
			{
				Console.Error("Failed to allocate TFX texture descriptors");
				return false;
			}

			ExecuteCommandBufferAndRestartRenderPass("Ran out of TFX texture descriptors");
			return ApplyTFXState(true);
		}

		dsub.AddCombinedImageSamplerDescriptorWrites(
			ds, 0, m_tfx_textures.data(), m_tfx_samplers.data(), NUM_TFX_SAMPLERS);
		dsub.Update(dev);

		m_tfx_descriptor_sets[1] = ds;
		dirty_descriptor_set_start = std::min(dirty_descriptor_set_start, 1u);
		dirty_descriptor_set_end = 1u;
	}

	if ((flags & DIRTY_FLAG_TFX_RT_TEXTURE_DS) || m_tfx_descriptor_sets[2] == VK_NULL_HANDLE)
	{
		VkDescriptorSet ds = g_vulkan_context->AllocateDescriptorSet(m_tfx_rt_texture_ds_layout);
		if (ds == VK_NULL_HANDLE)
		{
			if (already_execed)
			{
				Console.Error("Failed to allocate TFX sampler descriptors");
				return false;
			}

			ExecuteCommandBufferAndRestartRenderPass("Ran out of TFX sampler descriptors");
			return ApplyTFXState(true);
		}

		if (m_features.texture_barrier)
			dsub.AddInputAttachmentDescriptorWrite(ds, 0, m_tfx_textures[NUM_TFX_SAMPLERS]);
		else
			dsub.AddImageDescriptorWrite(ds, 0, m_tfx_textures[NUM_TFX_SAMPLERS]);
		dsub.AddImageDescriptorWrite(ds, 1, m_tfx_textures[NUM_TFX_SAMPLERS + 1]);
		dsub.Update(dev);

		m_tfx_descriptor_sets[2] = ds;
		dirty_descriptor_set_start = std::min(dirty_descriptor_set_start, 2u);
		dirty_descriptor_set_end = 2u;
	}

	if (m_current_pipeline_layout != PipelineLayout::TFX)
	{
		m_current_pipeline_layout = PipelineLayout::TFX;

		vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_tfx_pipeline_layout, 0,
			NUM_TFX_DESCRIPTOR_SETS, m_tfx_descriptor_sets.data(), NUM_TFX_DYNAMIC_OFFSETS,
			m_tfx_dynamic_offsets.data());
	}
	else if (dirty_descriptor_set_start <= dirty_descriptor_set_end)
	{
		u32 dynamic_count;
		const u32* dynamic_offsets;
		if (dirty_descriptor_set_start == 0)
		{
			dynamic_count = NUM_TFX_DYNAMIC_OFFSETS;
			dynamic_offsets = m_tfx_dynamic_offsets.data();
		}
		else
		{
			dynamic_count = 0;
			dynamic_offsets = nullptr;
		}

		const u32 count = dirty_descriptor_set_end - dirty_descriptor_set_start + 1;

		vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_tfx_pipeline_layout,
			dirty_descriptor_set_start, count, &m_tfx_descriptor_sets[dirty_descriptor_set_start], dynamic_count,
			dynamic_offsets);
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

			ExecuteCommandBufferAndRestartRenderPass("Ran out of utility descriptors");
			return ApplyTFXState(true);
		}

		Vulkan::DescriptorSetUpdateBuilder dsub;
		dsub.AddCombinedImageSamplerDescriptorWrite(m_utility_descriptor_set, 0, m_utility_texture, m_utility_sampler);
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

static void ImageBarrier(GSTextureVK* tex, VkAccessFlags src_mask, VkAccessFlags dst_mask, VkImageLayout src_layout,
	VkImageLayout dst_layout, VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage, bool pixel_local)
{
	const VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, src_mask, dst_mask,
		src_layout, dst_layout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, tex->GetTexture().GetImage(),
		{VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u}};

	vkCmdPipelineBarrier(g_vulkan_context->GetCurrentCommandBuffer(), src_stage, dst_stage,
		pixel_local ? VK_DEPENDENCY_BY_REGION_BIT : 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

static void ColorBufferBarrier(GSTextureVK* rt)
{
	const VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr,
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
		rt->GetTexture().GetImage(), {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u}};

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
	OMSetRenderTargets(nullptr, ds, bbox, false);
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
	OMSetRenderTargets(image, config.ds, config.drawarea, false);

	// if the depth target has been cleared, we need to preserve that clear
	const VkAttachmentLoadOp ds_load_op = GetLoadOpForTexture(static_cast<GSTextureVK*>(config.ds));
	const u32 ds = (config.ds ? 1 : 0);

	VkClearValue cv[2] = {};
	cv[0].color.float32[0] = static_cast<float>(std::numeric_limits<int>::max());
	if (ds_load_op == VK_ATTACHMENT_LOAD_OP_CLEAR)
	{
		cv[1].depthStencil.depth = static_cast<GSTextureVK*>(config.ds)->GetClearDepth();
		cv[1].depthStencil.stencil = 1;
		BeginClearRenderPass(m_date_image_setup_render_passes[ds][1], GSVector4i(0, 0, rtsize.x, rtsize.y), cv, 2);
	}
	else
	{
		BeginClearRenderPass(m_date_image_setup_render_passes[ds][0], config.drawarea, cv, 1);
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
	IASetVertexBuffer(config.verts, sizeof(GSVertex), config.nverts);
	IASetIndexBuffer(config.indices, config.nindices);

	// cut down the configuration for the prepass, we don't need blending or any feedback loop
	PipelineSelector& pipe = m_pipeline_selector;
	UpdateHWPipelineSelector(config, pipe);
	pipe.dss.zwe = false;
	pipe.cms.wrgba = 0;
	pipe.bs = {};
	pipe.feedback_loop = false;
	pipe.rt = true;
	pipe.ps.blend_a = pipe.ps.blend_b = pipe.ps.blend_c = pipe.ps.blend_d = false;
	pipe.ps.date += 10;
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
	image->TransitionToLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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
			break;
	}

	// stream buffer in first, in case we need to exec
	SetVSConstantBuffer(config.cb_vs);
	SetPSConstantBuffer(config.cb_ps);

	// bind textures before checking the render pass, in case we need to transition them
	if (config.tex)
	{
		PSSetShaderResource(0, config.tex, config.tex != config.rt);
		PSSetSampler(0, config.sampler);
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

	// Align the render area to 128x128, hopefully avoiding render pass restarts for small render area changes (e.g. Ratchet and Clank).
	const int render_area_alignment = 128 * GSConfig.UpscaleMultiplier;
	const GSVector2i rtsize(config.rt ? config.rt->GetSize() : config.ds->GetSize());
	const GSVector4i render_area(
		config.ps.hdr ? config.drawarea :
                        GSVector4i(Common::AlignDownPow2(config.scissor.left, render_area_alignment),
							Common::AlignDownPow2(config.scissor.top, render_area_alignment),
							std::min(Common::AlignUpPow2(config.scissor.right, render_area_alignment), rtsize.x),
							std::min(Common::AlignUpPow2(config.scissor.bottom, render_area_alignment), rtsize.y)));

	GSTextureVK* draw_rt = static_cast<GSTextureVK*>(config.rt);
	GSTextureVK* draw_ds = static_cast<GSTextureVK*>(config.ds);
	GSTextureVK* draw_rt_clone = nullptr;
	GSTextureVK* hdr_rt = nullptr;
	GSTextureVK* copy_ds = nullptr;

	// Switch to hdr target for colclip rendering
	if (pipe.ps.hdr)
	{
		EndRenderPass();

		GL_PUSH_("HDR Render Target Setup");
		hdr_rt = static_cast<GSTextureVK*>(CreateRenderTarget(rtsize.x, rtsize.y, GSTexture::Format::FloatColor, false));
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
			draw_rt->TransitionToLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}

		// we're not drawing to the RT, so we can use it as a source
		if (config.require_one_barrier && !m_features.texture_barrier)
			PSSetShaderResource(2, draw_rt, true);

		draw_rt = hdr_rt;
	}
	else if (config.require_one_barrier && !m_features.texture_barrier)
	{
		// requires a copy of the RT
		draw_rt_clone = static_cast<GSTextureVK*>(CreateTexture(rtsize.x, rtsize.y, false, GSTexture::Format::Color, false));
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

	if (config.tex && config.tex == config.ds)
	{
		// requires a copy of the depth buffer. this is mainly for ico.
		copy_ds = static_cast<GSTextureVK*>(CreateDepthStencil(rtsize.x, rtsize.y, GSTexture::Format::DepthStencil, false));
		if (copy_ds)
		{
			EndRenderPass();

			GL_PUSH("Copy depth to temp texture for shuffle {%d,%d %dx%d}",
				config.drawarea.left, config.drawarea.top,
				config.drawarea.width(), config.drawarea.height());

			CopyRect(config.ds, copy_ds, config.drawarea, config.drawarea.left, config.drawarea.top);
			PSSetShaderResource(0, copy_ds, true);
		}
	}

	const bool render_area_okay =
		(!hdr_rt && DATE_rp != DATE_RENDER_PASS_STENCIL_ONE && CheckRenderPassArea(render_area));

	// render pass restart optimizations
	if (render_area_okay)
	{
		// avoid restarting the render pass just to switch from rt+depth to rt and vice versa
		if (!draw_ds && m_current_depth_target && m_current_render_target == draw_rt &&
			config.tex != m_current_depth_target && !(pipe.feedback_loop && !CurrentFramebufferHasFeedbackLoop()))
		{
			draw_ds = m_current_depth_target;
			m_pipeline_selector.ds = true;
			m_pipeline_selector.dss.ztst = ZTST_ALWAYS;
			m_pipeline_selector.dss.zwe = false;
		}

		// Prefer keeping feedback loop enabled, that way we're not constantly restarting render passes
		pipe.feedback_loop |= m_current_render_target == draw_rt && m_current_depth_target == draw_ds &&
							  CurrentFramebufferHasFeedbackLoop();
	}

	OMSetRenderTargets(draw_rt, draw_ds, config.scissor, pipe.feedback_loop);
	if (pipe.feedback_loop)
	{
		pxAssertMsg(m_features.texture_barrier, "Texture barriers enabled");
		PSSetShaderResource(2, draw_rt, false);
	}

	// Begin render pass if new target or out of the area.
	if (!render_area_okay || !InRenderPass())
	{
		const VkAttachmentLoadOp rt_op = GetLoadOpForTexture(draw_rt);
		const VkAttachmentLoadOp ds_op = GetLoadOpForTexture(draw_ds);
		const VkRenderPass rp =
			GetTFXRenderPass(pipe.rt, pipe.ds, pipe.ps.hdr, DATE_rp, pipe.feedback_loop, rt_op, ds_op);
		const bool is_clearing_rt = (rt_op == VK_ATTACHMENT_LOAD_OP_CLEAR || ds_op == VK_ATTACHMENT_LOAD_OP_CLEAR);

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

			BeginClearRenderPass(
				rp, is_clearing_rt ? GSVector4i(0, 0, rtsize.x, rtsize.y) : render_area, cvs, cv_count);
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
		SetPipeline(m_hdr_setup_pipelines[pipe.ds][pipe.feedback_loop]);

		const GSVector4 sRect(GSVector4(render_area) / GSVector4(rtsize.x, rtsize.y).xyxy());
		DrawStretchRect(sRect, GSVector4(render_area), rtsize);
		g_perfmon.Put(GSPerfMon::TextureCopies);

		GL_POP();
	}

	// VB/IB upload, if we did DATE setup and it's not HDR this has already been done
	if (!date_image || hdr_rt)
	{
		IASetVertexBuffer(config.verts, sizeof(GSVertex), config.nverts);
		IASetIndexBuffer(config.indices, config.nindices);
	}

	// now we can do the actual draw
	if (BindDrawPipeline(pipe))
	{
		SendHWDraw(config, draw_rt);
		if (config.separate_alpha_pass)
		{
			SetHWDrawConfigForAlphaPass(&pipe.ps, &pipe.cms, &pipe.bs, &pipe.dss);
			if (BindDrawPipeline(pipe))
				SendHWDraw(config, draw_rt);
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
			SendHWDraw(config, draw_rt);
			if (config.second_separate_alpha_pass)
			{
				SetHWDrawConfigForAlphaPass(&pipe.ps, &pipe.cms, &pipe.bs, &pipe.dss);
				if (BindDrawPipeline(pipe))
					SendHWDraw(config, draw_rt);
			}
		}
	}

	if (copy_ds)
		Recycle(copy_ds);

	if (draw_rt_clone)
		Recycle(draw_rt_clone);

	if (date_image)
		Recycle(date_image);

	EndScene();

	// now blit the hdr texture back to the original target
	if (hdr_rt)
	{
		GL_INS("Blit HDR back to RT");

		EndRenderPass();
		hdr_rt->TransitionToLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		draw_rt = static_cast<GSTextureVK*>(config.rt);
		OMSetRenderTargets(draw_rt, draw_ds, config.scissor, pipe.feedback_loop);

		// if this target was cleared and never drawn to, perform the clear as part of the resolve here.
		if (draw_rt->GetState() == GSTexture::State::Cleared)
		{
			alignas(16) VkClearValue cvs[2];
			u32 cv_count = 0;
			GSVector4::store<true>(&cvs[cv_count++].color, draw_rt->GetClearColor());
			if (draw_ds)
				cvs[cv_count++].depthStencil = {draw_ds->GetClearDepth(), 1};

			BeginClearRenderPass(
				GetTFXRenderPass(true, pipe.ds, false, DATE_RENDER_PASS_NONE, pipe.feedback_loop, VK_ATTACHMENT_LOAD_OP_CLEAR,
					pipe.ds ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE),
				GSVector4i(0, 0, draw_rt->GetWidth(), draw_rt->GetHeight()),
				cvs, cv_count);
			draw_rt->SetState(GSTexture::State::Dirty);
		}
		else
		{
			BeginRenderPass(
				GetTFXRenderPass(true, pipe.ds, false, DATE_RENDER_PASS_NONE, pipe.feedback_loop, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
					pipe.ds ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_DONT_CARE),
				render_area);
		}

		const GSVector4 sRect(GSVector4(render_area) / GSVector4(rtsize.x, rtsize.y).xyxy());
		SetPipeline(m_hdr_finish_pipelines[pipe.ds][pipe.feedback_loop]);
		SetUtilityTexture(hdr_rt, m_point_sampler);
		DrawStretchRect(sRect, GSVector4(render_area), rtsize);
		g_perfmon.Put(GSPerfMon::TextureCopies);

		Recycle(hdr_rt);
	}
}

void GSDeviceVK::UpdateHWPipelineSelector(GSHWDrawConfig& config, PipelineSelector& pipe)
{
	pipe.vs.key = config.vs.key;
	pipe.gs.key = config.gs.key;
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
	pipe.feedback_loop = m_features.texture_barrier &&
										(config.ps.IsFeedbackLoop() || config.require_one_barrier || config.require_full_barrier);

	// enable point size in the vertex shader if we're rendering points regardless of upscaling.
	pipe.vs.point_size |= (config.topology == GSHWDrawConfig::Topology::Point);
}

void GSDeviceVK::SendHWDraw(const GSHWDrawConfig& config, GSTextureVK* draw_rt)
{
	if (config.drawlist)
	{
		GL_PUSH("Split the draw (SPRITE)");
		g_perfmon.Put(GSPerfMon::Barriers, static_cast<u32>(config.drawlist->size()));

		for (u32 count = 0, p = 0, n = 0; n < static_cast<u32>(config.drawlist->size()); p += count, ++n)
		{
			count = (*config.drawlist)[n] * config.indices_per_prim;
			ColorBufferBarrier(draw_rt);
			DrawIndexedPrimitive(p, count);
		}

		return;
	}

	if (m_features.texture_barrier && m_pipeline_selector.ps.IsFeedbackLoop())
	{
		if (config.require_full_barrier)
		{
			GL_PUSH("Split single draw in %d draw", config.nindices / config.indices_per_prim);
			g_perfmon.Put(GSPerfMon::Barriers, config.nindices / config.indices_per_prim);

			for (u32 p = 0; p < config.nindices; p += config.indices_per_prim)
			{
				ColorBufferBarrier(draw_rt);
				DrawIndexedPrimitive(p, config.indices_per_prim);
			}

			return;
		}

		if (config.require_one_barrier)
		{
			g_perfmon.Put(GSPerfMon::Barriers, 1);
			ColorBufferBarrier(draw_rt);
			DrawIndexedPrimitive();
			return;
		}
	}

	// Don't need any barrier
	DrawIndexedPrimitive();
}
