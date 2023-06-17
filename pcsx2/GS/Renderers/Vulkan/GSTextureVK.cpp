/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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
#include "GS/Renderers/Vulkan/GSTextureVK.h"
#include "GS/Renderers/Vulkan/VKBuilders.h"
#include "GS/Renderers/Vulkan/VKContext.h"
#include "GS/Renderers/Vulkan/VKUtil.h"
#include "GS/GSPerfMon.h"
#include "GS/GSGL.h"

#include "common/Align.h"
#include "common/Assertions.h"

static constexpr const VkComponentMapping s_identity_swizzle{VK_COMPONENT_SWIZZLE_IDENTITY,
	VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};

static constexpr std::array<VkImageLayout, static_cast<u32>(GSTextureVK::Layout::Count)> s_vk_layout_mapping = {{
	VK_IMAGE_LAYOUT_UNDEFINED, // Undefined
	VK_IMAGE_LAYOUT_PREINITIALIZED, // Preinitialized
	VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // ColorAttachment
	VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, // DepthStencilAttachment
	VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // ShaderReadOnly
	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // ClearDst
	VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // TransferSrc
	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // TransferDst
	VK_IMAGE_LAYOUT_GENERAL, // TransferSelf
	VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, // PresentSrc
	VK_IMAGE_LAYOUT_GENERAL, // FeedbackLoop
	VK_IMAGE_LAYOUT_GENERAL, // ReadWriteImage
	VK_IMAGE_LAYOUT_GENERAL, // ComputeReadWriteImage
	VK_IMAGE_LAYOUT_GENERAL, // General
}};

GSTextureVK::GSTextureVK(Type type, Format format, int width, int height, int levels, VkImage image,
	VmaAllocation allocation, VkImageView view, VkFormat vk_format)
	: GSTexture()
	, m_image(image)
	, m_allocation(allocation)
	, m_view(view)
	, m_vk_format(vk_format)
{
	m_type = type;
	m_format = format;
	m_size.x = width;
	m_size.y = height;
	m_mipmap_levels = levels;
}

GSTextureVK::~GSTextureVK()
{
	Destroy(true);
}

std::unique_ptr<GSTextureVK> GSTextureVK::Create(Type type, Format format, int width, int height, int levels)
{
	const VkFormat vk_format = GSDeviceVK::GetInstance()->LookupNativeFormat(format);

	VkImageCreateInfo ici = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, nullptr, 0, VK_IMAGE_TYPE_2D, vk_format,
		{static_cast<u32>(width), static_cast<u32>(height), 1}, static_cast<u32>(levels), 1, VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_TILING_OPTIMAL};

	VmaAllocationCreateInfo aci = {};
	aci.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	aci.flags = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT;
	aci.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	VkImageViewCreateInfo vci = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr, 0, VK_NULL_HANDLE,
		VK_IMAGE_VIEW_TYPE_2D, vk_format, s_identity_swizzle,
		{VK_IMAGE_ASPECT_COLOR_BIT, 0, static_cast<u32>(levels), 0, 1}};

	switch (type)
	{
		case Type::Texture:
		{
			ici.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

			if (format == Format::UNorm8)
			{
				// for r8 textures, swizzle it across all 4 components. the shaders depend on it being in alpha.. why?
				static constexpr const VkComponentMapping r8_swizzle = {
					VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R};
				vci.components = r8_swizzle;
			}
		}
		break;

		case Type::RenderTarget:
		{
			pxAssert(levels == 1);
			ici.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
						VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
						VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
		}
		break;

		case Type::DepthStencil:
		{
			pxAssert(levels == 1);
			ici.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
						VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		}
		break;

		case Type::RWTexture:
		{
			pxAssert(levels == 1);
			ici.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
						VK_IMAGE_USAGE_SAMPLED_BIT;
		}
		break;

		default:
			return {};
	}

	// Use dedicated allocations for typical RT size
	if ((type == Type::RenderTarget || type == Type::DepthStencil) && width >= 512 && height >= 448)
		aci.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

	VkImage image = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
	VkResult res = vmaCreateImage(g_vulkan_context->GetAllocator(), &ici, &aci, &image, &allocation, nullptr);
	if (aci.flags & VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT && res != VK_SUCCESS)
	{
		// try without dedicated allocation
		aci.flags &= ~VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
		res = vmaCreateImage(g_vulkan_context->GetAllocator(), &ici, &aci, &image, &allocation, nullptr);
	}
	if (res == VK_ERROR_OUT_OF_DEVICE_MEMORY)
	{
		DevCon.WriteLn("Failed to allocate device memory for %ux%u texture", width, height);
		return {};
	}
	else if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vmaCreateImage failed: ");
		return {};
	}

	VkImageView view = VK_NULL_HANDLE;
	vci.image = image;
	res = vkCreateImageView(g_vulkan_context->GetDevice(), &vci, nullptr, &view);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateImageView failed: ");
		vmaDestroyImage(g_vulkan_context->GetAllocator(), image, allocation);
		return {};
	}

	switch (type)
	{
		case Type::Texture:
			Vulkan::SetObjectName(g_vulkan_context->GetDevice(), image, "%dx%d texture", width, height);
			break;

		case Type::RenderTarget:
			Vulkan::SetObjectName(g_vulkan_context->GetDevice(), image, "%dx%d render target", width, height);
			break;

		case Type::DepthStencil:
			Vulkan::SetObjectName(g_vulkan_context->GetDevice(), image, "%dx%d depth stencil", width, height);
			break;

		case Type::RWTexture:
			Vulkan::SetObjectName(g_vulkan_context->GetDevice(), image, "%dx%d RW texture", width, height);
			break;

		default:
			break;
	}

	return std::unique_ptr<GSTextureVK>(
		new GSTextureVK(type, format, width, height, levels, image, allocation, view, vk_format));
}

std::unique_ptr<GSTextureVK> GSTextureVK::Adopt(
	VkImage image, Type type, Format format, int width, int height, int levels, VkFormat vk_format)
{
	// Only need to create the image view, this is mainly for swap chains.
	const VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr, 0, image,
		VK_IMAGE_VIEW_TYPE_2D, vk_format, s_identity_swizzle,
		{(type == Type::DepthStencil) ? static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT) :
										static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT),
			0u, static_cast<u32>(levels), 0u, 1u}};

	// Memory is managed by the owner of the image.
	VkImageView view = VK_NULL_HANDLE;
	VkResult res = vkCreateImageView(g_vulkan_context->GetDevice(), &view_info, nullptr, &view);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateImageView failed: ");
		return {};
	}

	return std::unique_ptr<GSTextureVK>(
		new GSTextureVK(type, format, width, height, levels, image, VK_NULL_HANDLE, view, vk_format));
}

void GSTextureVK::Destroy(bool defer)
{
	GSDeviceVK::GetInstance()->UnbindTexture(this);

	if (m_type == Type::RenderTarget || m_type == Type::DepthStencil)
	{
		for (const auto& [other_tex, fb, feedback] : m_framebuffers)
		{
			if (other_tex)
			{
				for (auto other_it = other_tex->m_framebuffers.begin(); other_it != other_tex->m_framebuffers.end();
					 ++other_it)
				{
					if (std::get<0>(*other_it) == this)
					{
						other_tex->m_framebuffers.erase(other_it);
						break;
					}
				}
			}

			if (defer)
				g_vulkan_context->DeferFramebufferDestruction(fb);
			else
				vkDestroyFramebuffer(g_vulkan_context->GetDevice(), fb, nullptr);
		}
		m_framebuffers.clear();
	}

	if (m_view != VK_NULL_HANDLE)
	{
		if (defer)
			g_vulkan_context->DeferImageViewDestruction(m_view);
		else
			vkDestroyImageView(g_vulkan_context->GetDevice(), m_view, nullptr);
		m_view = VK_NULL_HANDLE;
	}

	// If we don't have device memory allocated, the image is not owned by us (e.g. swapchain)
	if (m_allocation != VK_NULL_HANDLE)
	{
		if (defer)
			g_vulkan_context->DeferImageDestruction(m_image, m_allocation);
		else
			vmaDestroyImage(g_vulkan_context->GetAllocator(), m_image, m_allocation);
		m_image = VK_NULL_HANDLE;
		m_allocation = VK_NULL_HANDLE;
	}
}

VkImageLayout GSTextureVK::GetVkLayout() const
{
	return s_vk_layout_mapping[static_cast<u32>(m_layout)];
}

void* GSTextureVK::GetNativeHandle() const
{
	return const_cast<GSTextureVK*>(this);
}

VkCommandBuffer GSTextureVK::GetCommandBufferForUpdate()
{
	if (m_type != Type::Texture || m_use_fence_counter == g_vulkan_context->GetCurrentFenceCounter())
	{
		// Console.WriteLn("Texture update within frame, can't use do beforehand");
		GSDeviceVK::GetInstance()->EndRenderPass();
		return g_vulkan_context->GetCurrentCommandBuffer();
	}

	return g_vulkan_context->GetCurrentInitCommandBuffer();
}

void GSTextureVK::CopyTextureDataForUpload(void* dst, const void* src, u32 pitch, u32 upload_pitch, u32 height) const
{
	const u32 block_size = GetCompressedBlockSize();
	const u32 count = (height + (block_size - 1)) / block_size;
	StringUtil::StrideMemCpy(dst, upload_pitch, src, pitch, std::min(upload_pitch, pitch), count);
}

VkBuffer GSTextureVK::AllocateUploadStagingBuffer(const void* data, u32 pitch, u32 upload_pitch, u32 height) const
{
	const u32 size = upload_pitch * height;
	const VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, static_cast<VkDeviceSize>(size),
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, nullptr};

	// Don't worry about setting the coherent bit for this upload, the main reason we had
	// that set in StreamBuffer was for MoltenVK, which would upload the whole buffer on
	// smaller uploads, but we're writing to the whole thing anyway.
	VmaAllocationCreateInfo aci = {};
	aci.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
	aci.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

	VmaAllocationInfo ai;
	VkBuffer buffer;
	VmaAllocation allocation;
	VkResult res = vmaCreateBuffer(g_vulkan_context->GetAllocator(), &bci, &aci, &buffer, &allocation, &ai);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "(AllocateUploadStagingBuffer) vmaCreateBuffer() failed: ");
		return VK_NULL_HANDLE;
	}

	// Immediately queue it for freeing after the command buffer finishes, since it's only needed for the copy.
	g_vulkan_context->DeferBufferDestruction(buffer, allocation);

	// And write the data.
	CopyTextureDataForUpload(ai.pMappedData, data, pitch, upload_pitch, height);
	vmaFlushAllocation(g_vulkan_context->GetAllocator(), allocation, 0, size);
	return buffer;
}

void GSTextureVK::UpdateFromBuffer(VkCommandBuffer cmdbuf, int level, u32 x, u32 y, u32 width, u32 height,
	u32 buffer_height, u32 row_length, VkBuffer buffer, u32 buffer_offset)
{
	const Layout old_layout = m_layout;
	if (old_layout != Layout::TransferDst)
		TransitionSubresourcesToLayout(cmdbuf, level, 1, old_layout, Layout::TransferDst);

	const VkBufferImageCopy bic = {static_cast<VkDeviceSize>(buffer_offset), row_length, buffer_height,
		{VK_IMAGE_ASPECT_COLOR_BIT, static_cast<u32>(level), 0u, 1u}, {static_cast<s32>(x), static_cast<s32>(y), 0},
		{width, height, 1u}};

	vkCmdCopyBufferToImage(cmdbuf, buffer, m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bic);

	if (old_layout != Layout::TransferDst)
		TransitionSubresourcesToLayout(cmdbuf, level, 1, Layout::TransferDst, old_layout);
}

bool GSTextureVK::Update(const GSVector4i& r, const void* data, int pitch, int layer)
{
	if (layer >= m_mipmap_levels)
		return false;

	g_perfmon.Put(GSPerfMon::TextureUploads, 1);

	const u32 width = r.width();
	const u32 height = r.height();
	const u32 upload_pitch = Common::AlignUpPow2(pitch, g_vulkan_context->GetBufferCopyRowPitchAlignment());
	const u32 required_size = CalcUploadSize(height, upload_pitch);

	// If the texture is larger than half our streaming buffer size, use a separate buffer.
	// Otherwise allocation will either fail, or require lots of cmdbuffer submissions.
	VkBuffer buffer;
	u32 buffer_offset;
	if (required_size > (g_vulkan_context->GetTextureUploadBuffer().GetCurrentSize() / 2))
	{
		buffer_offset = 0;
		buffer = AllocateUploadStagingBuffer(data, pitch, upload_pitch, height);
		if (buffer == VK_NULL_HANDLE)
			return false;
	}
	else
	{
		VKStreamBuffer& sbuffer = g_vulkan_context->GetTextureUploadBuffer();
		if (!sbuffer.ReserveMemory(required_size, g_vulkan_context->GetBufferCopyOffsetAlignment()))
		{
			GSDeviceVK::GetInstance()->ExecuteCommandBuffer(
				false, "While waiting for %u bytes in texture upload buffer", required_size);
			if (!sbuffer.ReserveMemory(required_size, g_vulkan_context->GetBufferCopyOffsetAlignment()))
			{
				Console.Error("Failed to reserve texture upload memory (%u bytes).", required_size);
				return false;
			}
		}

		buffer = sbuffer.GetBuffer();
		buffer_offset = sbuffer.GetCurrentOffset();
		CopyTextureDataForUpload(sbuffer.GetCurrentHostPointer(), data, pitch, upload_pitch, height);
		sbuffer.CommitMemory(required_size);
	}

	const VkCommandBuffer cmdbuf = GetCommandBufferForUpdate();
	GL_PUSH("GSTextureVK::Update({%d,%d} %dx%d Lvl:%u", r.x, r.y, r.width(), r.height(), layer);

	// first time the texture is used? don't leave it undefined
	if (m_layout == Layout::Undefined)
		TransitionToLayout(cmdbuf, Layout::TransferDst);

	// if we're an rt and have been cleared, and the full rect isn't being uploaded, do the clear
	if (m_type == Type::RenderTarget)
	{
		if (!r.eq(GSVector4i(0, 0, m_size.x, m_size.y)))
			CommitClear(cmdbuf);
		else
			m_state = State::Dirty;
	}

	UpdateFromBuffer(cmdbuf, layer, r.x, r.y, width, height, Common::AlignUpPow2(height, GetCompressedBlockSize()),
		CalcUploadRowLengthFromPitch(upload_pitch), buffer, buffer_offset);
	TransitionToLayout(cmdbuf, Layout::ShaderReadOnly);

	if (m_type == Type::Texture)
		m_needs_mipmaps_generated |= (layer == 0);

	return true;
}

bool GSTextureVK::Map(GSMap& m, const GSVector4i* r, int layer)
{
	if (layer >= m_mipmap_levels || IsCompressedFormat())
		return false;

	// map for writing
	m_map_area = r ? *r : GetRect();
	m_map_level = layer;

	m.pitch =
		Common::AlignUpPow2(CalcUploadPitch(m_map_area.width()), g_vulkan_context->GetBufferCopyRowPitchAlignment());

	// see note in Update() for the reason why.
	const u32 required_size = CalcUploadSize(m_map_area.height(), m.pitch);
	VKStreamBuffer& buffer = g_vulkan_context->GetTextureUploadBuffer();
	if (required_size >= (buffer.GetCurrentSize() / 2))
		return false;

	if (!buffer.ReserveMemory(required_size, g_vulkan_context->GetBufferCopyOffsetAlignment()))
	{
		GSDeviceVK::GetInstance()->ExecuteCommandBuffer(
			false, "While waiting for %u bytes in texture upload buffer", required_size);
		if (!buffer.ReserveMemory(required_size, g_vulkan_context->GetBufferCopyOffsetAlignment()))
			pxFailRel("Failed to reserve texture upload memory");
	}

	m.bits = static_cast<u8*>(buffer.GetCurrentHostPointer());
	return true;
}

void GSTextureVK::Unmap()
{
	// this can't handle blocks/compressed formats at the moment.
	pxAssert(m_map_level < m_mipmap_levels && !IsCompressedFormat());
	g_perfmon.Put(GSPerfMon::TextureUploads, 1);

	const u32 width = m_map_area.width();
	const u32 height = m_map_area.height();
	const u32 pitch = Common::AlignUpPow2(CalcUploadPitch(width), g_vulkan_context->GetBufferCopyRowPitchAlignment());
	const u32 required_size = CalcUploadSize(height, pitch);
	VKStreamBuffer& buffer = g_vulkan_context->GetTextureUploadBuffer();
	const u32 buffer_offset = buffer.GetCurrentOffset();
	buffer.CommitMemory(required_size);

	const VkCommandBuffer cmdbuf = GetCommandBufferForUpdate();
	GL_PUSH("GSTextureVK::Update({%d,%d} %dx%d Lvl:%u", m_map_area.x, m_map_area.y, m_map_area.width(),
		m_map_area.height(), m_map_level);

	// first time the texture is used? don't leave it undefined
	if (m_layout == Layout::Undefined)
		TransitionToLayout(cmdbuf, Layout::TransferDst);

	// if we're an rt and have been cleared, and the full rect isn't being uploaded, do the clear
	if (m_type == Type::RenderTarget)
	{
		if (!m_map_area.eq(GSVector4i(0, 0, m_size.x, m_size.y)))
			CommitClear(cmdbuf);
		else
			m_state = State::Dirty;
	}

	UpdateFromBuffer(cmdbuf, m_map_level, m_map_area.x, m_map_area.y, width, height,
		Common::AlignUpPow2(height, GetCompressedBlockSize()), CalcUploadRowLengthFromPitch(pitch), buffer.GetBuffer(),
		buffer_offset);
	TransitionToLayout(cmdbuf, Layout::ShaderReadOnly);

	if (m_type == Type::Texture)
		m_needs_mipmaps_generated |= (m_map_level == 0);
}

void GSTextureVK::GenerateMipmap()
{
	const VkCommandBuffer cmdbuf = GetCommandBufferForUpdate();

	for (int dst_level = 1; dst_level < m_mipmap_levels; dst_level++)
	{
		const int src_level = dst_level - 1;
		const int src_width = std::max<int>(m_size.x >> src_level, 1);
		const int src_height = std::max<int>(m_size.y >> src_level, 1);
		const int dst_width = std::max<int>(m_size.x >> dst_level, 1);
		const int dst_height = std::max<int>(m_size.y >> dst_level, 1);

		TransitionSubresourcesToLayout(cmdbuf, src_level, 1, m_layout, Layout::TransferSrc);
		TransitionSubresourcesToLayout(cmdbuf, dst_level, 1, m_layout, Layout::TransferDst);

		const VkImageBlit blit = {
			{VK_IMAGE_ASPECT_COLOR_BIT, static_cast<u32>(src_level), 0u, 1u}, // srcSubresource
			{{0, 0, 0}, {src_width, src_height, 1}}, // srcOffsets
			{VK_IMAGE_ASPECT_COLOR_BIT, static_cast<u32>(dst_level), 0u, 1u}, // dstSubresource
			{{0, 0, 0}, {dst_width, dst_height, 1}} // dstOffsets
		};

		vkCmdBlitImage(cmdbuf, m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

		TransitionSubresourcesToLayout(cmdbuf, src_level, 1, Layout::TransferSrc, m_layout);
		TransitionSubresourcesToLayout(cmdbuf, dst_level, 1, Layout::TransferDst, m_layout);
	}
}

void GSTextureVK::Swap(GSTexture* tex)
{
	GSTexture::Swap(tex);
	std::swap(m_image, static_cast<GSTextureVK*>(tex)->m_image);
	std::swap(m_allocation, static_cast<GSTextureVK*>(tex)->m_allocation);
	std::swap(m_view, static_cast<GSTextureVK*>(tex)->m_view);
	std::swap(m_vk_format, static_cast<GSTextureVK*>(tex)->m_vk_format);
	std::swap(m_layout, static_cast<GSTextureVK*>(tex)->m_layout);
	std::swap(m_use_fence_counter, static_cast<GSTextureVK*>(tex)->m_use_fence_counter);
	std::swap(m_clear_value, static_cast<GSTextureVK*>(tex)->m_clear_value);
	std::swap(m_map_area, static_cast<GSTextureVK*>(tex)->m_map_area);
	std::swap(m_map_level, static_cast<GSTextureVK*>(tex)->m_map_level);
	std::swap(m_framebuffers, static_cast<GSTextureVK*>(tex)->m_framebuffers);
}

void GSTextureVK::CommitClear()
{
	if (m_state != GSTexture::State::Cleared)
		return;

	GSDeviceVK::GetInstance()->EndRenderPass();

	CommitClear(g_vulkan_context->GetCurrentCommandBuffer());
}

void GSTextureVK::CommitClear(VkCommandBuffer cmdbuf)
{
	TransitionToLayout(cmdbuf, Layout::ClearDst);

	if (IsDepthStencil())
	{
		const VkClearDepthStencilValue cv = {m_clear_value.depth};
		const VkImageSubresourceRange srr = {VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u};
		vkCmdClearDepthStencilImage(cmdbuf, m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &cv, 1, &srr);
	}
	else
	{
		alignas(16) VkClearColorValue cv;
		GSVector4::store<true>(cv.float32, GetUNormClearColor());
		const VkImageSubresourceRange srr = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
		vkCmdClearColorImage(cmdbuf, m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &cv, 1, &srr);
	}

	SetState(GSTexture::State::Dirty);
}

void GSTextureVK::OverrideImageLayout(Layout new_layout)
{
	m_layout = new_layout;
}

void GSTextureVK::TransitionToLayout(Layout layout)
{
	TransitionToLayout(g_vulkan_context->GetCurrentCommandBuffer(), layout);
}

void GSTextureVK::TransitionToLayout(VkCommandBuffer command_buffer, Layout new_layout)
{
	if (m_layout == new_layout)
		return;

	TransitionSubresourcesToLayout(command_buffer, 0, m_mipmap_levels, m_layout, new_layout);

	m_layout = new_layout;
}

void GSTextureVK::TransitionSubresourcesToLayout(
	VkCommandBuffer command_buffer, int start_level, int num_levels, Layout old_layout, Layout new_layout)
{
	VkImageAspectFlags aspect;
	if (m_type == Type::DepthStencil)
	{
		aspect = g_gs_device->Features().stencil_buffer ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT) :
														  VK_IMAGE_ASPECT_DEPTH_BIT;
	}
	else
	{
		aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, 0, 0,
		s_vk_layout_mapping[static_cast<u32>(old_layout)], s_vk_layout_mapping[static_cast<u32>(new_layout)],
		VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, m_image,
		{aspect, static_cast<u32>(start_level), static_cast<u32>(num_levels), 0u, 1u}};

	// srcStageMask -> Stages that must complete before the barrier
	// dstStageMask -> Stages that must wait for after the barrier before beginning
	VkPipelineStageFlags srcStageMask, dstStageMask;
	switch (old_layout)
	{
		case Layout::Undefined:
			// Layout undefined therefore contents undefined, and we don't care what happens to it.
			barrier.srcAccessMask = 0;
			srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			break;

		case Layout::Preinitialized:
			// Image has been pre-initialized by the host, so ensure all writes have completed.
			barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			srcStageMask = VK_PIPELINE_STAGE_HOST_BIT;
			break;

		case Layout::ColorAttachment:
			// Image was being used as a color attachment, so ensure all writes have completed.
			barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			break;

		case Layout::DepthStencilAttachment:
			// Image was being used as a depthstencil attachment, so ensure all writes have completed.
			barrier.srcAccessMask =
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			break;

		case Layout::ShaderReadOnly:
			// Image was being used as a shader resource, make sure all reads have finished.
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			break;

		case Layout::ClearDst:
			// Image was being used as a clear destination, ensure all writes have finished.
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;

		case Layout::TransferSrc:
			// Image was being used as a copy source, ensure all reads have finished.
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;

		case Layout::TransferDst:
			// Image was being used as a copy destination, ensure all writes have finished.
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;

		case Layout::TransferSelf:
			// Image was being used as a copy source and destination, ensure all reads and writes have finished.
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
			srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;

		case Layout::FeedbackLoop:
			barrier.srcAccessMask = (aspect == VK_IMAGE_ASPECT_COLOR_BIT) ?
										(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
											VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) :
										(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
											VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT);
			srcStageMask = (aspect == VK_IMAGE_ASPECT_COLOR_BIT) ?
							   (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) :
							   (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
								   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
			break;

		case Layout::ReadWriteImage:
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			break;

		case Layout::ComputeReadWriteImage:
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			break;

		case Layout::General:
		default:
			srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			break;
	}

	switch (new_layout)
	{
		case Layout::Undefined:
			barrier.dstAccessMask = 0;
			dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			break;

		case Layout::ColorAttachment:
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			break;

		case Layout::DepthStencilAttachment:
			barrier.dstAccessMask =
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			break;

		case Layout::ShaderReadOnly:
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			break;

		case Layout::ClearDst:
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;

		case Layout::TransferSrc:
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;

		case Layout::TransferDst:
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;

		case Layout::TransferSelf:
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
			dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;

		case Layout::PresentSrc:
			srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			break;

		case Layout::FeedbackLoop:
			barrier.dstAccessMask = (aspect == VK_IMAGE_ASPECT_COLOR_BIT) ?
										(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
											VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) :
										(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
											VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT);
			dstStageMask = (aspect == VK_IMAGE_ASPECT_COLOR_BIT) ?
							   (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT) :
							   (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
								   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
			break;

		case Layout::ReadWriteImage:
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			break;

		case Layout::ComputeReadWriteImage:
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			break;

		case Layout::General:
		default:
			dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			break;
	}
	vkCmdPipelineBarrier(command_buffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

VkFramebuffer GSTextureVK::GetFramebuffer(bool feedback_loop)
{
	return GetLinkedFramebuffer(nullptr, feedback_loop);
}

VkFramebuffer GSTextureVK::GetLinkedFramebuffer(GSTextureVK* depth_texture, bool feedback_loop)
{
	pxAssertRel(m_type != Type::Texture, "Texture is a render target");

	for (const auto& [other_tex, fb, other_feedback_loop] : m_framebuffers)
	{
		if (other_tex == depth_texture && other_feedback_loop == feedback_loop)
			return fb;
	}

	const VkRenderPass rp = g_vulkan_context->GetRenderPass(
		(m_type != GSTexture::Type::DepthStencil) ? m_vk_format : VK_FORMAT_UNDEFINED,
		(m_type != GSTexture::Type::DepthStencil) ? (depth_texture ? depth_texture->m_vk_format : VK_FORMAT_UNDEFINED) :
													m_vk_format,
		VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_LOAD,
		VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, feedback_loop);
	if (!rp)
		return VK_NULL_HANDLE;

	Vulkan::FramebufferBuilder fbb;
	fbb.AddAttachment(m_view);
	if (depth_texture)
		fbb.AddAttachment(depth_texture->m_view);
	fbb.SetSize(m_size.x, m_size.y, 1);
	fbb.SetRenderPass(rp);

	VkFramebuffer fb = fbb.Create(g_vulkan_context->GetDevice());
	if (!fb)
		return VK_NULL_HANDLE;

	m_framebuffers.emplace_back(depth_texture, fb, feedback_loop);
	if (depth_texture)
		depth_texture->m_framebuffers.emplace_back(this, fb, feedback_loop);
	return fb;
}

GSDownloadTextureVK::GSDownloadTextureVK(u32 width, u32 height, GSTexture::Format format)
	: GSDownloadTexture(width, height, format)
{
}

GSDownloadTextureVK::~GSDownloadTextureVK()
{
	// Buffer was created mapped, no need to manually unmap.
	if (m_buffer != VK_NULL_HANDLE)
		g_vulkan_context->DeferBufferDestruction(m_buffer, m_allocation);
}

std::unique_ptr<GSDownloadTextureVK> GSDownloadTextureVK::Create(u32 width, u32 height, GSTexture::Format format)
{
	const u32 buffer_size = GetBufferSize(width, height, format, g_vulkan_context->GetBufferCopyRowPitchAlignment());

	const VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0u, buffer_size,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_SHARING_MODE_EXCLUSIVE, 0u, nullptr};

	VmaAllocationCreateInfo aci = {};
	aci.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
	aci.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
	aci.preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;

	VmaAllocationInfo ai = {};
	VmaAllocation allocation;
	VkBuffer buffer;
	VkResult res = vmaCreateBuffer(g_vulkan_context->GetAllocator(), &bci, &aci, &buffer, &allocation, &ai);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vmaCreateBuffer() failed: ");
		return {};
	}

	std::unique_ptr<GSDownloadTextureVK> tex =
		std::unique_ptr<GSDownloadTextureVK>(new GSDownloadTextureVK(width, height, format));
	tex->m_allocation = allocation;
	tex->m_buffer = buffer;
	tex->m_buffer_size = buffer_size;
	tex->m_map_pointer = static_cast<const u8*>(ai.pMappedData);
	return tex;
}

void GSDownloadTextureVK::CopyFromTexture(
	const GSVector4i& drc, GSTexture* stex, const GSVector4i& src, u32 src_level, bool use_transfer_pitch)
{
	GSTextureVK* const vkTex = static_cast<GSTextureVK*>(stex);

	pxAssert(vkTex->GetFormat() == m_format);
	pxAssert(drc.width() == src.width() && drc.height() == src.height());
	pxAssert(src.z <= vkTex->GetWidth() && src.w <= vkTex->GetHeight());
	pxAssert(static_cast<u32>(drc.z) <= m_width && static_cast<u32>(drc.w) <= m_height);
	pxAssert(src_level < static_cast<u32>(vkTex->GetMipmapLevels()));
	pxAssert((drc.left == 0 && drc.top == 0) || !use_transfer_pitch);

	u32 copy_offset, copy_size, copy_rows;
	m_current_pitch = GetTransferPitch(use_transfer_pitch ? static_cast<u32>(drc.width()) : m_width,
		g_vulkan_context->GetBufferCopyRowPitchAlignment());
	GetTransferSize(drc, &copy_offset, &copy_size, &copy_rows);

	g_perfmon.Put(GSPerfMon::Readbacks, 1);
	GSDeviceVK::GetInstance()->EndRenderPass();
	vkTex->CommitClear();

	const VkCommandBuffer cmdbuf = g_vulkan_context->GetCurrentCommandBuffer();
	GL_INS("GSDownloadTextureVK::CopyFromTexture: {%d,%d} %ux%u", src.left, src.top, src.width(), src.height());

	GSTextureVK::Layout old_layout = vkTex->GetLayout();
	if (old_layout != GSTextureVK::Layout::TransferSrc)
		vkTex->TransitionSubresourcesToLayout(cmdbuf, src_level, 1, old_layout, GSTextureVK::Layout::TransferSrc);

	VkBufferImageCopy image_copy = {};
	const VkImageAspectFlags aspect = vkTex->IsDepthStencil() ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	image_copy.bufferOffset = copy_offset;
	image_copy.bufferRowLength = GSTexture::CalcUploadRowLengthFromPitch(m_format, m_current_pitch);
	image_copy.bufferImageHeight = 0;
	image_copy.imageSubresource = {aspect, src_level, 0u, 1u};
	image_copy.imageOffset = {src.left, src.top, 0};
	image_copy.imageExtent = {static_cast<u32>(src.width()), static_cast<u32>(src.height()), 1u};

	// do the copy
	vkCmdCopyImageToBuffer(cmdbuf, vkTex->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_buffer, 1, &image_copy);

	// flush gpu cache
	Vulkan::BufferMemoryBarrier(cmdbuf, m_buffer, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, 0, copy_size,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT);

	if (old_layout != GSTextureVK::Layout::TransferSrc)
		vkTex->TransitionSubresourcesToLayout(cmdbuf, src_level, 1, GSTextureVK::Layout::TransferSrc, old_layout);

	m_copy_fence_counter = g_vulkan_context->GetCurrentFenceCounter();
	m_needs_cache_invalidate = true;
	m_needs_flush = true;
}

bool GSDownloadTextureVK::Map(const GSVector4i& read_rc)
{
	// Always mapped, but we might need to invalidate the cache.
	if (m_needs_cache_invalidate)
	{
		u32 copy_offset, copy_size, copy_rows;
		GetTransferSize(read_rc, &copy_offset, &copy_size, &copy_rows);
		vmaInvalidateAllocation(g_vulkan_context->GetAllocator(), m_allocation, copy_offset, copy_size);
		m_needs_cache_invalidate = false;
	}

	return true;
}

void GSDownloadTextureVK::Unmap()
{
	// Always mapped.
}

void GSDownloadTextureVK::Flush()
{
	if (!m_needs_flush)
		return;

	m_needs_flush = false;

	if (g_vulkan_context->GetCompletedFenceCounter() >= m_copy_fence_counter)
		return;

	// Need to execute command buffer.
	if (g_vulkan_context->GetCurrentFenceCounter() == m_copy_fence_counter)
		GSDeviceVK::GetInstance()->ExecuteCommandBufferForReadback();
	else
		g_vulkan_context->WaitForFenceCounter(m_copy_fence_counter);
}
