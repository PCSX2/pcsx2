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
#include "GSDeviceVK.h"
#include "GSTextureVK.h"
#include "common/Align.h"
#include "common/Assertions.h"
#include "common/Vulkan/Builders.h"
#include "common/Vulkan/Context.h"
#include "common/Vulkan/Util.h"
#include "GS/GSPerfMon.h"
#include "GS/GSGL.h"

GSTextureVK::GSTextureVK(Type type, Format format, Vulkan::Texture texture)
	: m_texture(std::move(texture))
{
	m_type = type;
	m_format = format;
	m_size.x = m_texture.GetWidth();
	m_size.y = m_texture.GetHeight();
	m_mipmap_levels = m_texture.GetLevels();
}

GSTextureVK::~GSTextureVK()
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

			g_vulkan_context->DeferFramebufferDestruction(fb);
		}
	}
}

std::unique_ptr<GSTextureVK> GSTextureVK::Create(Type type, u32 width, u32 height, u32 levels, Format format, VkFormat vk_format)
{
	switch (type)
	{
		case Type::Texture:
		{
			VkImageUsageFlags usage =
				VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			const VkComponentMapping* swizzle = nullptr;
			if (format == Format::UNorm8)
			{
				// for r8 textures, swizzle it across all 4 components. the shaders depend on it being in alpha.. why?
				static constexpr VkComponentMapping r8_swizzle = {
					VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_R};
				swizzle = &r8_swizzle;
			}

			Vulkan::Texture texture;
			if (!texture.Create(width, height, levels, 1, vk_format, VK_SAMPLE_COUNT_1_BIT,
					VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, swizzle))
			{
				return {};
			}

			Vulkan::Util::SetObjectName(
				g_vulkan_context->GetDevice(), texture.GetImage(), "%ux%u texture", width, height);
			return std::make_unique<GSTextureVK>(type, format, std::move(texture));
		}

		case Type::RenderTarget:
		{
			pxAssert(levels == 1);

			Vulkan::Texture texture;
			if (!texture.Create(width, height, levels, 1, vk_format, VK_SAMPLE_COUNT_1_BIT,
					VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
					VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
						VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
						VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT))
			{
				return {};
			}

			Vulkan::Util::SetObjectName(
				g_vulkan_context->GetDevice(), texture.GetImage(), "%ux%u render target", width, height);
			return std::make_unique<GSTextureVK>(type, format, std::move(texture));
		}

		case Type::DepthStencil:
		{
			pxAssert(levels == 1);

			Vulkan::Texture texture;
			if (!texture.Create(width, height, levels, 1, vk_format, VK_SAMPLE_COUNT_1_BIT,
					VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
					VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
						VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT))
			{
				return {};
			}

			Vulkan::Util::SetObjectName(
				g_vulkan_context->GetDevice(), texture.GetImage(), "%ux%u depth stencil", width, height);
			return std::make_unique<GSTextureVK>(type, format, std::move(texture));
		}

		case Type::RWTexture:
		{
			pxAssert(levels == 1);

			Vulkan::Texture texture;
			if (!texture.Create(width, height, levels, 1, vk_format, VK_SAMPLE_COUNT_1_BIT,
					VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
					VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
						VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT))
			{
				return {};
			}

			Vulkan::Util::SetObjectName(
				g_vulkan_context->GetDevice(), texture.GetImage(), "%ux%u RW texture", width, height);
			return std::make_unique<GSTextureVK>(type, format, std::move(texture));
		}

		default:
			return {};
	}
}

void* GSTextureVK::GetNativeHandle() const { return const_cast<Vulkan::Texture*>(&m_texture); }

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
	const VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0,
		static_cast<VkDeviceSize>(size), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, nullptr};

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
		Vulkan::StreamBuffer& sbuffer = g_vulkan_context->GetTextureUploadBuffer();
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
	if (m_texture.GetLayout() == VK_IMAGE_LAYOUT_UNDEFINED)
		m_texture.TransitionToLayout(cmdbuf, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// if we're an rt and have been cleared, and the full rect isn't being uploaded, do the clear
	if (m_type == Type::RenderTarget)
	{
		if (!r.eq(GSVector4i(0, 0, m_size.x, m_size.y)))
			CommitClear(cmdbuf);
		else
			m_state = State::Dirty;
	}

	m_texture.UpdateFromBuffer(cmdbuf, layer, 0, r.x, r.y, width, height,
		Common::AlignUpPow2(height, GetCompressedBlockSize()),
		CalcUploadRowLengthFromPitch(upload_pitch), buffer, buffer_offset);
	m_texture.TransitionToLayout(cmdbuf, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	if (m_type == Type::Texture)
		m_needs_mipmaps_generated |= (layer == 0);

	return true;
}

bool GSTextureVK::Map(GSMap& m, const GSVector4i* r, int layer)
{
	if (layer >= m_mipmap_levels || IsCompressedFormat())
		return false;

	// map for writing
	m_map_area = r ? *r : GSVector4i(0, 0, m_texture.GetWidth(), m_texture.GetHeight());
	m_map_level = layer;

	m.pitch = Common::AlignUpPow2(m_map_area.width() * Vulkan::Util::GetTexelSize(m_texture.GetFormat()),
		g_vulkan_context->GetBufferCopyRowPitchAlignment());

	// see note in Update() for the reason why.
	const u32 required_size = m.pitch * m_map_area.height();
	Vulkan::StreamBuffer& buffer = g_vulkan_context->GetTextureUploadBuffer();
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
	pxAssert(m_map_level < m_texture.GetLevels() && !IsCompressedFormat());
	g_perfmon.Put(GSPerfMon::TextureUploads, 1);

	// TODO: non-tightly-packed formats
	const u32 width = static_cast<u32>(m_map_area.width());
	const u32 height = static_cast<u32>(m_map_area.height());
	const u32 pitch = Common::AlignUpPow2(m_map_area.width() * Vulkan::Util::GetTexelSize(m_texture.GetFormat()),
		g_vulkan_context->GetBufferCopyRowPitchAlignment());
	const u32 required_size = pitch * height;
	Vulkan::StreamBuffer& buffer = g_vulkan_context->GetTextureUploadBuffer();
	const u32 buffer_offset = buffer.GetCurrentOffset();
	buffer.CommitMemory(required_size);

	const VkCommandBuffer cmdbuf = GetCommandBufferForUpdate();
	GL_PUSH("GSTextureVK::Update({%d,%d} %dx%d Lvl:%u", m_map_area.x, m_map_area.y, m_map_area.width(),
		m_map_area.height(), m_map_level);

	// first time the texture is used? don't leave it undefined
	if (m_texture.GetLayout() == VK_IMAGE_LAYOUT_UNDEFINED)
		m_texture.TransitionToLayout(cmdbuf, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// if we're an rt and have been cleared, and the full rect isn't being uploaded, do the clear
	if (m_type == Type::RenderTarget)
	{
		if (!m_map_area.eq(GSVector4i(0, 0, m_size.x, m_size.y)))
			CommitClear(cmdbuf);
		else
			m_state = State::Dirty;
	}

	m_texture.UpdateFromBuffer(cmdbuf, m_map_level, 0, m_map_area.x, m_map_area.y, width, height,
		Common::AlignUpPow2(height, GetCompressedBlockSize()),
		CalcUploadRowLengthFromPitch(pitch), buffer.GetBuffer(), buffer_offset);
	m_texture.TransitionToLayout(cmdbuf, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

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

		m_texture.TransitionSubresourcesToLayout(
			cmdbuf, src_level, 1, 0, 1, m_texture.GetLayout(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		m_texture.TransitionSubresourcesToLayout(
			cmdbuf, dst_level, 1, 0, 1, m_texture.GetLayout(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		const VkImageBlit blit = {
			{VK_IMAGE_ASPECT_COLOR_BIT, static_cast<u32>(src_level), 0u, 1u}, // srcSubresource
			{{0, 0, 0}, {src_width, src_height, 1}}, // srcOffsets
			{VK_IMAGE_ASPECT_COLOR_BIT, static_cast<u32>(dst_level), 0u, 1u}, // dstSubresource
			{{0, 0, 0}, {dst_width, dst_height, 1}} // dstOffsets
		};

		vkCmdBlitImage(cmdbuf, m_texture.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_texture.GetImage(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

		m_texture.TransitionSubresourcesToLayout(
			cmdbuf, src_level, 1, 0, 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_texture.GetLayout());
		m_texture.TransitionSubresourcesToLayout(
			cmdbuf, dst_level, 1, 0, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_texture.GetLayout());
	}
}

void GSTextureVK::Swap(GSTexture* tex)
{
	GSTexture::Swap(tex);
	std::swap(m_texture, static_cast<GSTextureVK*>(tex)->m_texture);
	std::swap(m_use_fence_counter, static_cast<GSTextureVK*>(tex)->m_use_fence_counter);
	std::swap(m_clear_value, static_cast<GSTextureVK*>(tex)->m_clear_value);
	std::swap(m_map_area, static_cast<GSTextureVK*>(tex)->m_map_area);
	std::swap(m_map_level, static_cast<GSTextureVK*>(tex)->m_map_level);
	std::swap(m_framebuffers, static_cast<GSTextureVK*>(tex)->m_framebuffers);
}

void GSTextureVK::TransitionToLayout(VkImageLayout layout)
{
	m_texture.TransitionToLayout(g_vulkan_context->GetCurrentCommandBuffer(), layout);
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
	m_texture.TransitionToLayout(cmdbuf, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	if (IsDepthStencil())
	{
		const VkClearDepthStencilValue cv = { m_clear_value.depth };
		const VkImageSubresourceRange srr = { VK_IMAGE_ASPECT_DEPTH_BIT, 0u, 1u, 0u, 1u };
		vkCmdClearDepthStencilImage(cmdbuf, m_texture.GetImage(), m_texture.GetLayout(), &cv, 1, &srr);
	}
	else
	{
		alignas(16) VkClearColorValue cv;
		GSVector4::store<true>(cv.float32, GetClearColor());
		const VkImageSubresourceRange srr = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
		vkCmdClearColorImage(cmdbuf, m_texture.GetImage(), m_texture.GetLayout(), &cv, 1, &srr);
	}

	SetState(GSTexture::State::Dirty);
}

VkFramebuffer GSTextureVK::GetFramebuffer(bool feedback_loop) { return GetLinkedFramebuffer(nullptr, feedback_loop); }

VkFramebuffer GSTextureVK::GetLinkedFramebuffer(GSTextureVK* depth_texture, bool feedback_loop)
{
	pxAssertRel(m_type != Type::Texture, "Texture is a render target");

	for (const auto& [other_tex, fb, other_feedback_loop] : m_framebuffers)
	{
		if (other_tex == depth_texture && other_feedback_loop == feedback_loop)
			return fb;
	}

	VkRenderPass rp = g_vulkan_context->GetRenderPass(
		(m_type != GSTexture::Type::DepthStencil) ? GetNativeFormat() : VK_FORMAT_UNDEFINED,
		(m_type != GSTexture::Type::DepthStencil) ?
            (depth_texture ? depth_texture->GetNativeFormat() : VK_FORMAT_UNDEFINED) :
            GetNativeFormat(),
		VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_LOAD,
		VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, feedback_loop);
	if (!rp)
		return VK_NULL_HANDLE;

	Vulkan::FramebufferBuilder fbb;
	fbb.AddAttachment(m_texture.GetView());
	if (depth_texture)
		fbb.AddAttachment(depth_texture->m_texture.GetView());
	fbb.SetSize(m_texture.GetWidth(), m_texture.GetHeight(), m_texture.GetLayers());
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

	const VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0u, buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_SHARING_MODE_EXCLUSIVE, 0u, nullptr};

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

	std::unique_ptr<GSDownloadTextureVK> tex = std::unique_ptr<GSDownloadTextureVK>(new GSDownloadTextureVK(width, height, format));
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
	m_current_pitch =
		GetTransferPitch(use_transfer_pitch ? static_cast<u32>(drc.width()) : m_width, g_vulkan_context->GetBufferCopyRowPitchAlignment());
	GetTransferSize(drc, &copy_offset, &copy_size, &copy_rows);

	g_perfmon.Put(GSPerfMon::Readbacks, 1);
	GSDeviceVK::GetInstance()->EndRenderPass();
	vkTex->CommitClear();

	const VkCommandBuffer cmdbuf = g_vulkan_context->GetCurrentCommandBuffer();
	GL_INS("GSDownloadTextureVK::CopyFromTexture: {%d,%d} %ux%u", src.left, src.top, src.width(), src.height());

	VkImageLayout old_layout = vkTex->GetTexture().GetLayout();
	if (old_layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
	{
		vkTex->GetTexture().TransitionSubresourcesToLayout(cmdbuf, src_level, 1, 0, 1, old_layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	}

	VkBufferImageCopy image_copy = {};
	const VkImageAspectFlags aspect =
		Vulkan::Util::IsDepthFormat(static_cast<VkFormat>(vkTex->GetFormat())) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	image_copy.bufferOffset = copy_offset;
	image_copy.bufferRowLength = GSTexture::CalcUploadRowLengthFromPitch(m_format, m_current_pitch);
	image_copy.bufferImageHeight = 0;
	image_copy.imageSubresource = {aspect, src_level, 0u, 1u};
	image_copy.imageOffset = {src.left, src.top, 0};
	image_copy.imageExtent = {static_cast<u32>(src.width()), static_cast<u32>(src.height()), 1u};

	// do the copy
	vkCmdCopyImageToBuffer(cmdbuf, vkTex->GetTexture().GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_buffer, 1, &image_copy);

	// flush gpu cache
	Vulkan::Util::BufferMemoryBarrier(cmdbuf, m_buffer, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT, 0, copy_size,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_HOST_BIT);

	if (old_layout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
	{
		vkTex->GetTexture().TransitionSubresourcesToLayout(cmdbuf, src_level, 1, 0, 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, old_layout);
	}

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
