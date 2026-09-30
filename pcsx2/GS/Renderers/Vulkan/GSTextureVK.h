// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "GS/Renderers/Common/GSTexture.h"
#include "GS/GS.h"
#include "GS/Renderers/Vulkan/VKLoader.h"

#include <limits>
#include <tuple>

enum class FeedbackLoopFlagsVK : u8
{
	None = 0,
	ReadAndWriteRT = 1,
	ReadOnlyDepth = 2,
	ReadAndWriteDepth = 4,
};

MARK_ENUM_AS_FLAGS(FeedbackLoopFlagsVK);

__forceinline_odr bool IsRTFeedbackLoop(FeedbackLoopFlagsVK f)
{
	return f & FeedbackLoopFlagsVK::ReadAndWriteRT;
}

__forceinline_odr bool IsDepthFeedbackLoop(FeedbackLoopFlagsVK f)
{
	return f & FeedbackLoopFlagsVK::ReadAndWriteDepth;
}

__forceinline_odr bool IsTestingAndSamplingDepth(FeedbackLoopFlagsVK f)
{
	return f & (FeedbackLoopFlagsVK::ReadOnlyDepth | FeedbackLoopFlagsVK::ReadAndWriteDepth);
}

class GSTextureVK final : public GSTexture
{
public:
	enum class Layout : u32
	{
		Undefined,
		Preinitialized,
		ColorAttachment,
		DepthStencilAttachment,
		ShaderReadOnly,
		ClearDst,
		CopySrc,
		CopyDst,
		CopySelf,
		BlitSrc,
		BlitDst,
		BlitSelf,
		PresentSrc,
		FeedbackLoop,
		ReadWriteImage,
		ComputeReadWriteImage,
		General,
		Count
	};

	struct FramebufferInfo
	{
		GSTextureVK* rt;
		GSTextureVK* ds_as_rt;
		GSTextureVK* ds;
		FeedbackLoopFlagsVK feedback_loop_flags;
		VkFramebuffer framebuffer;

		FramebufferInfo(GSTexture* rt, GSTexture* ds_as_rt, GSTexture* ds,
			bool rt_feedback = false, bool depth_feedback = false)
			: rt(static_cast<GSTextureVK*>(rt))
			, ds_as_rt(static_cast<GSTextureVK*>(ds_as_rt))
			, ds(static_cast<GSTextureVK*>(ds))
			, feedback_loop_flags(FeedbackLoopFlagsVK::None)
			, framebuffer(VK_NULL_HANDLE)
		{
		}

		FramebufferInfo(GSTexture* rt, GSTexture* ds, bool rt_feedback = false, bool depth_feedback = false)
			: FramebufferInfo(rt, nullptr, ds, rt_feedback, depth_feedback)
		{
		}

		FramebufferInfo() : FramebufferInfo(nullptr, nullptr, nullptr, false, false)
		{
		}

		__fi bool IsRTFeedbackLoop() const { return ::IsRTFeedbackLoop(feedback_loop_flags); }
		__fi bool IsDepthFeedbackLoop() const { return ::IsDepthFeedbackLoop(feedback_loop_flags); }
		__fi bool IsTestingAndSamplingDepth() const { return ::IsTestingAndSamplingDepth(feedback_loop_flags); }

		bool Matches(const FramebufferInfo& other) const
		{
			return rt == other.rt &&
				ds_as_rt == other.ds_as_rt &&
				ds == other.ds &&
				feedback_loop_flags == other.feedback_loop_flags;
		}

		u32 NumAttachments() const
		{
			return (rt ? 1 : 0) + (ds_as_rt ? 1 : 0) + (ds ? 1 : 0);
		}

		GSTextureVK* FirstAttachment() const
		{
			return rt ? rt : (ds_as_rt ? ds_as_rt : ds);
		}

		std::array<GSTextureVK*, 3> Attachments() const
		{
			return std::array{ rt, ds_as_rt, ds };
		}

		std::array<VkClearValue, 3> GetClearValues() const
		{
			alignas(16) std::array<VkClearValue, 3> clear_values;
			u32 count = 0;
			if (rt)
				GSVector4::store<true>(&clear_values[count++].color, rt->GetClearForFormat());
			if (ds_as_rt)
				GSVector4::store<true>(&clear_values[count++].color, ds_as_rt->GetClearForFormat());
			if (ds)
			{
				clear_values[count].depthStencil.depth =  ds->GetClearDepth();
				clear_values[count].depthStencil.stencil = 1;
				count++;
			}
			return clear_values;
		}

		bool HasAttachment(GSTextureVK* tex) const
		{
			for (GSTextureVK* t : Attachments())
			{
				if (t == tex)
					return true;
			}
			return false;
		}

		GSVector2i GetSize() const
		{
			return (rt ? rt->GetSize() : (ds_as_rt ? ds_as_rt->GetSize() : (ds ? ds->GetSize() : GSVector2i(0, 0))));
		}

		GSVector4i GetRect() const
		{
			return GSVector4i::loadh(GetSize());
		}
	};

	~GSTextureVK() override;

	static VkImageLayout GetVkImageLayout(Layout layout);

	static std::unique_ptr<GSTextureVK> Create(Usage usage, Format format, int width, int height, int levels);
	static std::unique_ptr<GSTextureVK> Adopt(
		VkImage image, Usage usage, Format format, int width, int height, int levels, VkFormat vk_format);

	void Destroy(bool defer);

	__fi VkImage GetImage() const { return m_image; }
	__fi VkImageView GetView() const { return m_view; }
	__fi Layout GetLayout() const { return m_layout; }
	bool IsShaderWriteMode() const override { return GetLayout() == Layout::ReadWriteImage; }

	__fi VkFormat GetVkFormat() const { return m_vk_format; }

	VkImageLayout GetVkLayout() const;

	void* GetNativeHandle() const override;

	bool Update(const GSVector4i& r, const void* data, int pitch, int layer = 0) override;
	bool Map(GSMap& m, const GSVector4i* r = NULL, int layer = 0) override;
	void Unmap() override;
	void GenerateMipmap() override;

#ifdef PCSX2_DEVBUILD
	void SetDebugName(std::string_view name) override;
#endif

	void TransitionToLayout(Layout layout);
	void CommitClear();
	void CommitClear(VkCommandBuffer cmdbuf);

	// Used when the render pass is changing the image layout, or to force it to
	// VK_IMAGE_LAYOUT_UNDEFINED, if the existing contents of the image is
	// irrelevant and will not be loaded.
	void OverrideImageLayout(Layout new_layout);

	void TransitionToLayout(VkCommandBuffer command_buffer, Layout new_layout);
	void TransitionSubresourcesToLayout(
		VkCommandBuffer command_buffer, int start_level, int num_levels, Layout old_layout, Layout new_layout);

	static VkFramebuffer CreateNullFramebuffer(u32 w, u32 h);

	/// Framebuffers are lazily allocated.
	VkFramebuffer GetFramebuffer(bool feedback_loop);

	void GetLinkedFramebuffer(FramebufferInfo& info);

	// Call when the texture is bound to the pipeline, or read from in a copy.
	__fi void SetUseFenceCounter(u64 counter) { m_use_fence_counter = counter; }

private:
	GSTextureVK(Usage usage, Format format, int width, int height, int levels, VkImage image, VmaAllocation allocation,
		VkImageView view, VkFormat vk_format);

	VkCommandBuffer GetCommandBufferForUpdate();
	void CopyTextureDataForUpload(void* dst, const void* src, u32 pitch, u32 upload_pitch, u32 height) const;
	VkBuffer AllocateUploadStagingBuffer(const void* data, u32 pitch, u32 upload_pitch, u32 height) const;
	void UpdateFromBuffer(VkCommandBuffer cmdbuf, int level, u32 x, u32 y, u32 width, u32 height, u32 buffer_height,
		u32 row_length, VkBuffer buffer, u32 buffer_offset);

	void RemoveFramebuffer(VkFramebuffer fb);

	VkImage m_image = VK_NULL_HANDLE;
	VmaAllocation m_allocation = VK_NULL_HANDLE;
	VkImageView m_view = VK_NULL_HANDLE;
	VkFormat m_vk_format = VK_FORMAT_UNDEFINED;
	Layout m_layout = Layout::Undefined;

	// Contains the fence counter when the texture was last used.
	// When this matches the current fence counter, the texture was used this command buffer.
	u64 m_use_fence_counter = 0;

	int m_map_level = std::numeric_limits<int>::max();
	GSVector4i m_map_area = GSVector4i::zero();

	// linked framebuffer is combined with depth texture
	// list of color textures this depth texture is linked to or vice versa
	std::vector<FramebufferInfo> m_framebuffers;
};

class GSDownloadTextureVK final : public GSDownloadTexture
{
public:
	~GSDownloadTextureVK() override;

	static std::unique_ptr<GSDownloadTextureVK> Create(u32 width, u32 height, GSTexture::Format format);

	void CopyFromTexture(
		const GSVector4i& drc, GSTexture* stex, const GSVector4i& src, u32 src_level, bool use_transfer_pitch) override;

	bool Map(const GSVector4i& read_rc) override;
	void Unmap() override;

	void Flush() override;

#ifdef PCSX2_DEVBUILD
	void SetDebugName(std::string_view name) override;
#endif

private:
	GSDownloadTextureVK(u32 width, u32 height, GSTexture::Format format);

	VmaAllocation m_allocation = VK_NULL_HANDLE;
	VkBuffer m_buffer = VK_NULL_HANDLE;

	u64 m_copy_fence_counter = 0;
	u32 m_buffer_size = 0;

	bool m_needs_cache_invalidate = false;
};
