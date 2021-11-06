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
#include <algorithm>
#include <memory>

namespace Vulkan
{
	class Texture
	{
	public:
		Texture();
		Texture(Texture&& move);
		Texture(const Texture&) = delete;
		~Texture();

		Texture& operator=(Texture&& move);
		Texture& operator=(const Texture&) = delete;

		__fi bool IsValid() const { return (m_image != VK_NULL_HANDLE); }

		/// An image is considered owned/managed if we control the memory.
		__fi bool IsOwned() const { return (m_allocation != VK_NULL_HANDLE); }

		__fi u32 GetWidth() const { return m_width; }
		__fi u32 GetHeight() const { return m_height; }
		__fi u32 GetLevels() const { return m_levels; }
		__fi u32 GetLayers() const { return m_layers; }
		__fi u32 GetMipWidth(u32 level) const { return std::max<u32>(m_width >> level, 1u); }
		__fi u32 GetMipHeight(u32 level) const { return std::max<u32>(m_height >> level, 1u); }
		__fi VkFormat GetFormat() const { return m_format; }
		__fi VkSampleCountFlagBits GetSamples() const { return m_samples; }
		__fi VkImageLayout GetLayout() const { return m_layout; }
		__fi VkImageViewType GetViewType() const { return m_view_type; }
		__fi VkImage GetImage() const { return m_image; }
		__fi VmaAllocation GetAllocation() const { return m_allocation; }
		__fi VkImageView GetView() const { return m_view; }

		bool Create(u32 width, u32 height, u32 levels, u32 layers, VkFormat format, VkSampleCountFlagBits samples,
			VkImageViewType view_type, VkImageTiling tiling, VkImageUsageFlags usage,
			const VkComponentMapping* swizzle = nullptr);

		bool Adopt(VkImage existing_image, VkImageViewType view_type, u32 width, u32 height, u32 levels, u32 layers,
			VkFormat format, VkSampleCountFlagBits samples, const VkComponentMapping* swizzle = nullptr);

		void Destroy(bool defer = true);

		// Used when the render pass is changing the image layout, or to force it to
		// VK_IMAGE_LAYOUT_UNDEFINED, if the existing contents of the image is
		// irrelevant and will not be loaded.
		void OverrideImageLayout(VkImageLayout new_layout);

		void TransitionToLayout(VkCommandBuffer command_buffer, VkImageLayout new_layout);
		void TransitionSubresourcesToLayout(VkCommandBuffer command_buffer, u32 start_level, u32 num_levels,
			u32 start_layer, u32 num_layers, VkImageLayout old_layout, VkImageLayout new_layout);

		VkFramebuffer CreateFramebuffer(VkRenderPass render_pass);

		void UpdateFromBuffer(VkCommandBuffer cmdbuf, u32 level, u32 layer, u32 x, u32 y, u32 width, u32 height,
			u32 row_length, VkBuffer buffer, u32 buffer_offset);

	private:
		u32 m_width = 0;
		u32 m_height = 0;
		u32 m_levels = 0;
		u32 m_layers = 0;
		VkFormat m_format = VK_FORMAT_UNDEFINED;
		VkSampleCountFlagBits m_samples = VK_SAMPLE_COUNT_1_BIT;
		VkImageViewType m_view_type = VK_IMAGE_VIEW_TYPE_2D;
		VkImageLayout m_layout = VK_IMAGE_LAYOUT_UNDEFINED;

		VkImage m_image = VK_NULL_HANDLE;
		VmaAllocation m_allocation = VK_NULL_HANDLE;
		VkImageView m_view = VK_NULL_HANDLE;
	};

} // namespace Vulkan
