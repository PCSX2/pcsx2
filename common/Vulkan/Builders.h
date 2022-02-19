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
#include <array>

namespace Vulkan
{
	class DescriptorSetLayoutBuilder
	{
	public:
		enum : u32
		{
			MAX_BINDINGS = 16,
		};

		DescriptorSetLayoutBuilder();

		void Clear();

		VkDescriptorSetLayout Create(VkDevice device);

		void AddBinding(u32 binding, VkDescriptorType dtype, u32 dcount, VkShaderStageFlags stages);

	private:
		VkDescriptorSetLayoutCreateInfo m_ci{};
		std::array<VkDescriptorSetLayoutBinding, MAX_BINDINGS> m_bindings{};
	};

	class PipelineLayoutBuilder
	{
	public:
		enum : u32
		{
			MAX_SETS = 8,
			MAX_PUSH_CONSTANTS = 1
		};

		PipelineLayoutBuilder();

		void Clear();

		VkPipelineLayout Create(VkDevice device);

		void AddDescriptorSet(VkDescriptorSetLayout layout);

		void AddPushConstants(VkShaderStageFlags stages, u32 offset, u32 size);

	private:
		VkPipelineLayoutCreateInfo m_ci{};
		std::array<VkDescriptorSetLayout, MAX_SETS> m_sets{};
		std::array<VkPushConstantRange, MAX_PUSH_CONSTANTS> m_push_constants{};
	};

	class GraphicsPipelineBuilder
	{
	public:
		enum : u32
		{
			MAX_SHADER_STAGES = 3,
			MAX_VERTEX_ATTRIBUTES = 16,
			MAX_VERTEX_BUFFERS = 8,
			MAX_ATTACHMENTS = 2,
			MAX_DYNAMIC_STATE = 8
		};

		GraphicsPipelineBuilder();

		void Clear();

		VkPipeline Create(VkDevice device, VkPipelineCache pipeline_cache = VK_NULL_HANDLE, bool clear = true);

		void SetShaderStage(VkShaderStageFlagBits stage, VkShaderModule module, const char* entry_point);
		void SetVertexShader(VkShaderModule module) { SetShaderStage(VK_SHADER_STAGE_VERTEX_BIT, module, "main"); }
		void SetGeometryShader(VkShaderModule module) { SetShaderStage(VK_SHADER_STAGE_GEOMETRY_BIT, module, "main"); }
		void SetFragmentShader(VkShaderModule module) { SetShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, module, "main"); }

		void AddVertexBuffer(u32 binding, u32 stride, VkVertexInputRate input_rate = VK_VERTEX_INPUT_RATE_VERTEX);
		void AddVertexAttribute(u32 location, u32 binding, VkFormat format, u32 offset);

		void SetPrimitiveTopology(VkPrimitiveTopology topology, bool enable_primitive_restart = false);

		void SetRasterizationState(VkPolygonMode polygon_mode, VkCullModeFlags cull_mode, VkFrontFace front_face);
		void SetLineWidth(float width);
		void SetMultisamples(u32 multisamples, bool per_sample_shading);
		void SetNoCullRasterizationState();

		void SetDepthState(bool depth_test, bool depth_write, VkCompareOp compare_op);
		void SetStencilState(bool stencil_test, const VkStencilOpState& front, const VkStencilOpState& back);
		void SetNoDepthTestState();
		void SetNoStencilState();

		void AddBlendAttachment(bool blend_enable, VkBlendFactor src_factor, VkBlendFactor dst_factor, VkBlendOp op,
			VkBlendFactor alpha_src_factor, VkBlendFactor alpha_dst_factor, VkBlendOp alpha_op,
			VkColorComponentFlags write_mask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
											   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
		void SetBlendAttachment(u32 attachment, bool blend_enable, VkBlendFactor src_factor, VkBlendFactor dst_factor,
			VkBlendOp op, VkBlendFactor alpha_src_factor, VkBlendFactor alpha_dst_factor, VkBlendOp alpha_op,
			VkColorComponentFlags write_mask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
											   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
		void AddBlendFlags(u32 flags);
		void ClearBlendAttachments();

		void SetBlendConstants(float r, float g, float b, float a);
		void SetNoBlendingState();

		void AddDynamicState(VkDynamicState state);

		void SetDynamicViewportAndScissorState();
		void SetViewport(float x, float y, float width, float height, float min_depth, float max_depth);
		void SetScissorRect(s32 x, s32 y, u32 width, u32 height);

		void SetMultisamples(VkSampleCountFlagBits samples);

		void SetPipelineLayout(VkPipelineLayout layout);
		void SetRenderPass(VkRenderPass render_pass, u32 subpass);

		void SetProvokingVertex(VkProvokingVertexModeEXT mode);

	private:
		VkGraphicsPipelineCreateInfo m_ci;
		std::array<VkPipelineShaderStageCreateInfo, MAX_SHADER_STAGES> m_shader_stages;

		VkPipelineVertexInputStateCreateInfo m_vertex_input_state;
		std::array<VkVertexInputBindingDescription, MAX_VERTEX_BUFFERS> m_vertex_buffers;
		std::array<VkVertexInputAttributeDescription, MAX_VERTEX_ATTRIBUTES> m_vertex_attributes;

		VkPipelineInputAssemblyStateCreateInfo m_input_assembly;

		VkPipelineRasterizationStateCreateInfo m_rasterization_state;
		VkPipelineDepthStencilStateCreateInfo m_depth_state;

		VkPipelineColorBlendStateCreateInfo m_blend_state;
		std::array<VkPipelineColorBlendAttachmentState, MAX_ATTACHMENTS> m_blend_attachments;

		VkPipelineViewportStateCreateInfo m_viewport_state;
		VkViewport m_viewport;
		VkRect2D m_scissor;

		VkPipelineDynamicStateCreateInfo m_dynamic_state;
		std::array<VkDynamicState, MAX_DYNAMIC_STATE> m_dynamic_state_values;

		VkPipelineMultisampleStateCreateInfo m_multisample_state;

		VkPipelineRasterizationProvokingVertexStateCreateInfoEXT m_provoking_vertex;
	};

	class SamplerBuilder
	{
	public:
		SamplerBuilder();

		void Clear();

		VkSampler Create(VkDevice device, bool clear = true);

		void SetFilter(VkFilter mag_filter, VkFilter min_filter, VkSamplerMipmapMode mip_filter);
		void SetAddressMode(VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w);

		void SetPointSampler(VkSamplerAddressMode address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);
		void SetLinearSampler(
			bool mipmaps, VkSamplerAddressMode address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);

	private:
		VkSamplerCreateInfo m_ci;
	};

	class DescriptorSetUpdateBuilder
	{
		enum : u32
		{
			MAX_WRITES = 16,
			MAX_IMAGE_INFOS = 8,
			MAX_BUFFER_INFOS = 4,
			MAX_VIEWS = 4,
		};

	public:
		DescriptorSetUpdateBuilder();

		void Clear();

		void Update(VkDevice device, bool clear = true);

		void AddImageDescriptorWrite(VkDescriptorSet set, u32 binding, VkImageView view,
			VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		void AddImageDescriptorWrites(VkDescriptorSet set, u32 binding, const VkImageView* views, u32 num_views,
			VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		void AddSamplerDescriptorWrite(VkDescriptorSet set, u32 binding, VkSampler sampler);
		void AddSamplerDescriptorWrites(VkDescriptorSet set, u32 binding, const VkSampler* samplers, u32 num_samplers);
		void AddCombinedImageSamplerDescriptorWrite(VkDescriptorSet set, u32 binding, VkImageView view,
			VkSampler sampler, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		void AddCombinedImageSamplerDescriptorWrites(VkDescriptorSet set, u32 binding, const VkImageView* views,
			const VkSampler* samplers, u32 num_views, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		void AddBufferDescriptorWrite(
			VkDescriptorSet set, u32 binding, VkDescriptorType dtype, VkBuffer buffer, u32 offset, u32 size);
		void AddBufferViewDescriptorWrite(VkDescriptorSet set, u32 binding, VkDescriptorType dtype, VkBufferView view);
		void AddInputAttachmentDescriptorWrite(
			VkDescriptorSet set, u32 binding, VkImageView view, VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL);
		void AddStorageImageDescriptorWrite(
			VkDescriptorSet set, u32 binding, VkImageView view, VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL);

	private:
		std::array<VkWriteDescriptorSet, MAX_WRITES> m_writes;
		u32 m_num_writes = 0;

		std::array<VkDescriptorBufferInfo, MAX_BUFFER_INFOS> m_buffer_infos;
		std::array<VkDescriptorImageInfo, MAX_IMAGE_INFOS> m_image_infos;
		std::array<VkBufferView, MAX_VIEWS> m_views;
		u32 m_num_buffer_infos = 0;
		u32 m_num_image_infos = 0;
		u32 m_num_views = 0;
	};

	class FramebufferBuilder
	{
		enum : u32
		{
			MAX_ATTACHMENTS = 2,
		};

	public:
		FramebufferBuilder();

		void Clear();

		VkFramebuffer Create(VkDevice device, bool clear = true);

		void AddAttachment(VkImageView image);

		void SetSize(u32 width, u32 height, u32 layers);

		void SetRenderPass(VkRenderPass render_pass);

	private:
		VkFramebufferCreateInfo m_ci;
		std::array<VkImageView, MAX_ATTACHMENTS> m_images;
	};

	class RenderPassBuilder
	{
		enum : u32
		{
			MAX_ATTACHMENTS = 2,
			MAX_ATTACHMENT_REFERENCES = 2,
			MAX_SUBPASSES = 1,
		};

	public:
		RenderPassBuilder();

		void Clear();

		VkRenderPass Create(VkDevice device, bool clear = true);

		u32 AddAttachment(VkFormat format, VkSampleCountFlagBits samples, VkAttachmentLoadOp load_op,
			VkAttachmentStoreOp store_op, VkImageLayout initial_layout, VkImageLayout final_layout);

		u32 AddSubpass();
		void AddSubpassColorAttachment(u32 subpass, u32 attachment, VkImageLayout layout);
		void AddSubpassDepthAttachment(u32 subpass, u32 attachment, VkImageLayout layout);

	private:
		VkRenderPassCreateInfo m_ci;
		std::array<VkAttachmentDescription, MAX_ATTACHMENTS> m_attachments;
		std::array<VkAttachmentReference, MAX_ATTACHMENT_REFERENCES> m_attachment_references;
		u32 m_num_attachment_references = 0;
		std::array<VkSubpassDescription, MAX_SUBPASSES> m_subpasses;
	};

	class BufferViewBuilder
	{
	public:
		BufferViewBuilder();

		void Clear();

		VkBufferView Create(VkDevice device, bool clear = true);

		void Set(VkBuffer buffer, VkFormat format, u32 offset, u32 size);

	private:
		VkBufferViewCreateInfo m_ci;
	};

} // namespace Vulkan