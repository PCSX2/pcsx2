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

#include "common/Vulkan/Texture.h"
#include "common/Vulkan/Context.h"
#include "common/Vulkan/Util.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include <algorithm>

static constexpr VkComponentMapping s_identity_swizzle{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
	VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};

namespace Vulkan
{
	Texture::Texture() = default;

	Texture::Texture(Texture&& move)
		: m_width(move.m_width)
		, m_height(move.m_height)
		, m_levels(move.m_levels)
		, m_layers(move.m_layers)
		, m_format(move.m_format)
		, m_samples(move.m_samples)
		, m_view_type(move.m_view_type)
		, m_layout(move.m_layout)
		, m_image(move.m_image)
		, m_allocation(move.m_allocation)
		, m_view(move.m_view)
	{
		move.m_width = 0;
		move.m_height = 0;
		move.m_levels = 0;
		move.m_layers = 0;
		move.m_format = VK_FORMAT_UNDEFINED;
		move.m_samples = VK_SAMPLE_COUNT_1_BIT;
		move.m_view_type = VK_IMAGE_VIEW_TYPE_2D;
		move.m_layout = VK_IMAGE_LAYOUT_UNDEFINED;
		move.m_image = VK_NULL_HANDLE;
		move.m_allocation = VK_NULL_HANDLE;
		move.m_view = VK_NULL_HANDLE;
	}

	Texture::~Texture()
	{
		if (IsValid())
			Destroy(true);
	}

	Vulkan::Texture& Texture::operator=(Texture&& move)
	{
		if (IsValid())
			Destroy(true);

		std::swap(m_width, move.m_width);
		std::swap(m_height, move.m_height);
		std::swap(m_levels, move.m_levels);
		std::swap(m_layers, move.m_layers);
		std::swap(m_format, move.m_format);
		std::swap(m_samples, move.m_samples);
		std::swap(m_view_type, move.m_view_type);
		std::swap(m_layout, move.m_layout);
		std::swap(m_image, move.m_image);
		std::swap(m_allocation, move.m_allocation);
		std::swap(m_view, move.m_view);

		return *this;
	}

	bool Texture::Create(u32 width, u32 height, u32 levels, u32 layers, VkFormat format, VkSampleCountFlagBits samples,
		VkImageViewType view_type, VkImageTiling tiling, VkImageUsageFlags usage,
		const VkComponentMapping* swizzle /* = nullptr*/)
	{
		const VkImageCreateInfo image_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, nullptr, 0, VK_IMAGE_TYPE_2D, format,
			{width, height, 1}, levels, layers, samples, tiling, usage, VK_SHARING_MODE_EXCLUSIVE, 0, nullptr,
			VK_IMAGE_LAYOUT_UNDEFINED};

		VmaAllocationCreateInfo aci = {};
		aci.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		aci.flags = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT;
		aci.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		VkImage image = VK_NULL_HANDLE;
		VmaAllocation allocation = VK_NULL_HANDLE;
		VkResult res =
			vmaCreateImage(g_vulkan_context->GetAllocator(), &image_info, &aci, &image, &allocation, nullptr);
		if (res == VK_ERROR_OUT_OF_DEVICE_MEMORY)
		{
			DevCon.WriteLn("Failed to allocate device memory for %ux%u texture", width, height);
			return false;
		}
		else if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vmaCreateImage failed: ");
			return false;
		}

		const VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr, 0, image, view_type,
			format, swizzle ? *swizzle : s_identity_swizzle,
			{Util::IsDepthFormat(format) ? static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT) :
                                           static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT),
				0, levels, 0, layers}};

		VkImageView view = VK_NULL_HANDLE;
		res = vkCreateImageView(g_vulkan_context->GetDevice(), &view_info, nullptr, &view);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateImageView failed: ");
			vmaDestroyImage(g_vulkan_context->GetAllocator(), image, allocation);
			return false;
		}

		if (IsValid())
			Destroy(true);

		m_width = width;
		m_height = height;
		m_levels = levels;
		m_layers = layers;
		m_format = format;
		m_samples = samples;
		m_view_type = view_type;
		m_image = image;
		m_allocation = allocation;
		m_view = view;
		return true;
	}

	bool Texture::Adopt(VkImage existing_image, VkImageViewType view_type, u32 width, u32 height, u32 levels,
		u32 layers, VkFormat format, VkSampleCountFlagBits samples, const VkComponentMapping* swizzle /* = nullptr*/)
	{
		// Only need to create the image view, this is mainly for swap chains.
		const VkImageViewCreateInfo view_info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr, 0, existing_image,
			view_type, format, swizzle ? *swizzle : s_identity_swizzle,
			{Util::IsDepthFormat(format) ? static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT) :
                                           static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT),
				0, levels, 0, layers}};

		// Memory is managed by the owner of the image.
		VkImageView view = VK_NULL_HANDLE;
		VkResult res = vkCreateImageView(g_vulkan_context->GetDevice(), &view_info, nullptr, &view);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateImageView failed: ");
			return false;
		}

		if (IsValid())
			Destroy(true);

		m_width = width;
		m_height = height;
		m_levels = levels;
		m_layers = layers;
		m_format = format;
		m_samples = samples;
		m_view_type = view_type;
		m_image = existing_image;
		m_view = view;
		return true;
	}

	void Texture::Destroy(bool defer /* = true */)
	{
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
			pxAssert(m_image != VK_NULL_HANDLE);
			if (defer)
				g_vulkan_context->DeferImageDestruction(m_image, m_allocation);
			else
				vmaDestroyImage(g_vulkan_context->GetAllocator(), m_image, m_allocation);
			m_image = VK_NULL_HANDLE;
			m_allocation = VK_NULL_HANDLE;
		}

		m_width = 0;
		m_height = 0;
		m_levels = 0;
		m_layers = 0;
		m_format = VK_FORMAT_UNDEFINED;
		m_samples = VK_SAMPLE_COUNT_1_BIT;
		m_view_type = VK_IMAGE_VIEW_TYPE_2D;
		m_layout = VK_IMAGE_LAYOUT_UNDEFINED;
	}

	void Texture::OverrideImageLayout(VkImageLayout new_layout) { m_layout = new_layout; }

	void Texture::TransitionToLayout(VkCommandBuffer command_buffer, VkImageLayout new_layout)
	{
		if (m_layout == new_layout)
			return;

		TransitionSubresourcesToLayout(command_buffer, 0, m_levels, 0, m_layers, m_layout, new_layout);

		m_layout = new_layout;
	}

	void Texture::TransitionSubresourcesToLayout(VkCommandBuffer command_buffer, u32 start_level, u32 num_levels,
		u32 start_layer, u32 num_layers, VkImageLayout old_layout, VkImageLayout new_layout)
	{
		VkImageAspectFlags aspect;
		if (Util::IsDepthStencilFormat(m_format))
			aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		else if (Util::IsDepthFormat(m_format))
			aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		else
			aspect = VK_IMAGE_ASPECT_COLOR_BIT;

		VkImageMemoryBarrier barrier = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, // VkStructureType            sType
			nullptr, // const void*                pNext
			0, // VkAccessFlags              srcAccessMask
			0, // VkAccessFlags              dstAccessMask
			old_layout, // VkImageLayout              oldLayout
			new_layout, // VkImageLayout              newLayout
			VK_QUEUE_FAMILY_IGNORED, // uint32_t                   srcQueueFamilyIndex
			VK_QUEUE_FAMILY_IGNORED, // uint32_t                   dstQueueFamilyIndex
			m_image, // VkImage                    image
			{aspect, start_level, num_levels, start_layer, num_layers} // VkImageSubresourceRange    subresourceRange
		};

		// srcStageMask -> Stages that must complete before the barrier
		// dstStageMask -> Stages that must wait for after the barrier before beginning
		VkPipelineStageFlags srcStageMask, dstStageMask;
		switch (old_layout)
		{
			case VK_IMAGE_LAYOUT_UNDEFINED:
				// Layout undefined therefore contents undefined, and we don't care what happens to it.
				barrier.srcAccessMask = 0;
				srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
				break;

			case VK_IMAGE_LAYOUT_PREINITIALIZED:
				// Image has been pre-initialized by the host, so ensure all writes have completed.
				barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
				srcStageMask = VK_PIPELINE_STAGE_HOST_BIT;
				break;

			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				// Image was being used as a color attachment, so ensure all writes have completed.
				barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				break;

			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				// Image was being used as a depthstencil attachment, so ensure all writes have completed.
				barrier.srcAccessMask =
					VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
				break;

			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				// Image was being used as a shader resource, make sure all reads have finished.
				barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
				srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				break;

			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				// Image was being used as a copy source, ensure all reads have finished.
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
				break;

			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				// Image was being used as a copy destination, ensure all writes have finished.
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
				break;

			case VK_IMAGE_LAYOUT_GENERAL:
				// General is used for feedback loops.
				barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
										VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
				srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				break;

			default:
				srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
				break;
		}

		switch (new_layout)
		{
			case VK_IMAGE_LAYOUT_UNDEFINED:
				barrier.dstAccessMask = 0;
				dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
				break;

			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				break;

			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				barrier.dstAccessMask =
					VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
				break;

			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				break;

			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
				break;

			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
				break;

			case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
				srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
				dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
				break;

			case VK_IMAGE_LAYOUT_GENERAL:
				// General is used for feedback loops.
				barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
										VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
				dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				break;

			default:
				dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
				break;
		}
		vkCmdPipelineBarrier(command_buffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	}

	VkFramebuffer Texture::CreateFramebuffer(VkRenderPass render_pass)
	{
		const VkFramebufferCreateInfo ci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, nullptr, 0u, render_pass, 1,
			&m_view, m_width, m_height, m_layers};
		VkFramebuffer fb = VK_NULL_HANDLE;
		VkResult res = vkCreateFramebuffer(g_vulkan_context->GetDevice(), &ci, nullptr, &fb);
		if (res != VK_SUCCESS)
		{
			LOG_VULKAN_ERROR(res, "vkCreateFramebuffer() failed: ");
			return VK_NULL_HANDLE;
		}

		return fb;
	}

	void Texture::UpdateFromBuffer(VkCommandBuffer cmdbuf, u32 level, u32 layer, u32 x, u32 y, u32 width, u32 height,
		u32 row_length, VkBuffer buffer, u32 buffer_offset)
	{
		const VkImageLayout old_layout = m_layout;
		if (old_layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
			TransitionSubresourcesToLayout(
				cmdbuf, level, 1, layer, 1, old_layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		const VkBufferImageCopy bic = {static_cast<VkDeviceSize>(buffer_offset), row_length, height,
			{VK_IMAGE_ASPECT_COLOR_BIT, level, layer, 1u}, {static_cast<int32_t>(x), static_cast<int32_t>(y), 0},
			{width, height, 1u}};

		vkCmdCopyBufferToImage(cmdbuf, buffer, m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bic);

		if (old_layout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
			TransitionSubresourcesToLayout(
				cmdbuf, level, 1, layer, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, old_layout);
	}
} // namespace Vulkan
