// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GS/Renderers/Vulkan/VKBuilders.h"

#include "common/Assertions.h"
#include "common/Console.h"

#include <limits>

void Vulkan::AddPointerToChain(void* head, const void* ptr)
{
	VkBaseInStructure* last_st = static_cast<VkBaseInStructure*>(head);
	while (last_st->pNext)
	{
		if (last_st->pNext == ptr)
			return;

		last_st = const_cast<VkBaseInStructure*>(last_st->pNext);
	}

	last_st->pNext = static_cast<const VkBaseInStructure*>(ptr);
}


const char* Vulkan::VkResultToString(VkResult res)
{
	switch (res)
	{
		case VK_SUCCESS:
			return "VK_SUCCESS";

		case VK_NOT_READY:
			return "VK_NOT_READY";

		case VK_TIMEOUT:
			return "VK_TIMEOUT";

		case VK_EVENT_SET:
			return "VK_EVENT_SET";

		case VK_EVENT_RESET:
			return "VK_EVENT_RESET";

		case VK_INCOMPLETE:
			return "VK_INCOMPLETE";

		case VK_ERROR_OUT_OF_HOST_MEMORY:
			return "VK_ERROR_OUT_OF_HOST_MEMORY";

		case VK_ERROR_OUT_OF_DEVICE_MEMORY:
			return "VK_ERROR_OUT_OF_DEVICE_MEMORY";

		case VK_ERROR_INITIALIZATION_FAILED:
			return "VK_ERROR_INITIALIZATION_FAILED";

		case VK_ERROR_DEVICE_LOST:
			return "VK_ERROR_DEVICE_LOST";

		case VK_ERROR_MEMORY_MAP_FAILED:
			return "VK_ERROR_MEMORY_MAP_FAILED";

		case VK_ERROR_LAYER_NOT_PRESENT:
			return "VK_ERROR_LAYER_NOT_PRESENT";

		case VK_ERROR_EXTENSION_NOT_PRESENT:
			return "VK_ERROR_EXTENSION_NOT_PRESENT";

		case VK_ERROR_FEATURE_NOT_PRESENT:
			return "VK_ERROR_FEATURE_NOT_PRESENT";

		case VK_ERROR_INCOMPATIBLE_DRIVER:
			return "VK_ERROR_INCOMPATIBLE_DRIVER";

		case VK_ERROR_TOO_MANY_OBJECTS:
			return "VK_ERROR_TOO_MANY_OBJECTS";

		case VK_ERROR_FORMAT_NOT_SUPPORTED:
			return "VK_ERROR_FORMAT_NOT_SUPPORTED";

		case VK_ERROR_SURFACE_LOST_KHR:
			return "VK_ERROR_SURFACE_LOST_KHR";

		case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
			return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";

		case VK_SUBOPTIMAL_KHR:
			return "VK_SUBOPTIMAL_KHR";

		case VK_ERROR_OUT_OF_DATE_KHR:
			return "VK_ERROR_OUT_OF_DATE_KHR";

		case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
			return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";

		case VK_ERROR_VALIDATION_FAILED_EXT:
			return "VK_ERROR_VALIDATION_FAILED_EXT";

		case VK_ERROR_INVALID_SHADER_NV:
			return "VK_ERROR_INVALID_SHADER_NV";

		default:
			return "UNKNOWN_VK_RESULT";
	}
}

void Vulkan::LogVulkanResult(const char* func_name, VkResult res, const char* msg, ...)
{
	std::va_list ap;
	va_start(ap, msg);
	std::string real_msg = StringUtil::StdStringFromFormatV(msg, ap);
	va_end(ap);

	Console.Error("(%s) %s (%d: %s)", func_name, real_msg.c_str(), static_cast<int>(res), VkResultToString(res));
}

Vulkan::DescriptorSetLayoutBuilder::DescriptorSetLayoutBuilder()
{
	Clear();
}

void Vulkan::DescriptorSetLayoutBuilder::Clear()
{
	m_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	m_ci.pNext = nullptr;
	m_ci.flags = 0;
	m_ci.pBindings = nullptr;
	m_ci.bindingCount = 0;
}

void Vulkan::DescriptorSetLayoutBuilder::SetPushFlag()
{
	m_ci.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
}

VkDescriptorSetLayout Vulkan::DescriptorSetLayoutBuilder::Create(VkDevice device)
{
	VkDescriptorSetLayout layout;
	VkResult res = vkCreateDescriptorSetLayout(device, &m_ci, nullptr, &layout);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateDescriptorSetLayout() failed: ");
		return VK_NULL_HANDLE;
	}

	Clear();
	return layout;
}

void Vulkan::DescriptorSetLayoutBuilder::AddBinding(
	u32 binding, VkDescriptorType dtype, u32 dcount, VkShaderStageFlags stages)
{
	pxAssert(m_ci.bindingCount < MAX_BINDINGS);

	VkDescriptorSetLayoutBinding& b = m_bindings[m_ci.bindingCount];
	b.binding = binding;
	b.descriptorType = dtype;
	b.descriptorCount = dcount;
	b.stageFlags = stages;
	b.pImmutableSamplers = nullptr;

	m_ci.pBindings = m_bindings.data();
	m_ci.bindingCount++;
}

Vulkan::PipelineLayoutBuilder::PipelineLayoutBuilder()
{
	Clear();
}

void Vulkan::PipelineLayoutBuilder::Clear()
{
	m_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	m_ci.pNext = nullptr;
	m_ci.flags = 0;
	m_ci.pSetLayouts = nullptr;
	m_ci.setLayoutCount = 0;
	m_ci.pPushConstantRanges = nullptr;
	m_ci.pushConstantRangeCount = 0;
}

VkPipelineLayout Vulkan::PipelineLayoutBuilder::Create(VkDevice device)
{
	VkPipelineLayout layout;
	VkResult res = vkCreatePipelineLayout(device, &m_ci, nullptr, &layout);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreatePipelineLayout() failed: ");
		return VK_NULL_HANDLE;
	}

	Clear();
	return layout;
}

void Vulkan::PipelineLayoutBuilder::AddDescriptorSet(VkDescriptorSetLayout layout)
{
	pxAssert(m_ci.setLayoutCount < MAX_SETS);

	m_sets[m_ci.setLayoutCount] = layout;

	m_ci.setLayoutCount++;
	m_ci.pSetLayouts = m_sets.data();
}

void Vulkan::PipelineLayoutBuilder::AddPushConstants(VkShaderStageFlags stages, u32 offset, u32 size)
{
	pxAssert(m_ci.pushConstantRangeCount < MAX_PUSH_CONSTANTS);

	VkPushConstantRange& r = m_push_constants[m_ci.pushConstantRangeCount];
	r.stageFlags = stages;
	r.offset = offset;
	r.size = size;

	m_ci.pushConstantRangeCount++;
	m_ci.pPushConstantRanges = m_push_constants.data();
}

Vulkan::GraphicsPipelineBuilder::GraphicsPipelineBuilder()
{
	Clear();
}

void Vulkan::GraphicsPipelineBuilder::Clear()
{
	m_ci = {};
	m_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

	m_shader_stages = {};

	m_vertex_input_state = {};
	m_vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	m_ci.pVertexInputState = &m_vertex_input_state;
	m_vertex_attributes = {};
	m_vertex_buffers = {};

	m_input_assembly = {};
	m_input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;

	m_rasterization_state = {};
	m_rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	m_rasterization_state.lineWidth = 1.0f;
	m_depth_state = {};
	m_depth_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	m_blend_state = {};
	m_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	m_blend_attachments = {};

	m_viewport_state = {};
	m_viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	m_viewport = {};
	m_scissor = {};

	m_dynamic_state = {};
	m_dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	m_dynamic_state_values = {};

	m_multisample_state = {};
	m_multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;

	m_provoking_vertex = {};
	m_provoking_vertex.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_PROVOKING_VERTEX_STATE_CREATE_INFO_EXT;

	m_line_rasterization_state = {};
	m_line_rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_LINE_STATE_CREATE_INFO_EXT;

	// set defaults
	SetNoCullRasterizationState();
	SetNoDepthTestState();
	SetNoBlendingState();
	SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

	// have to be specified even if dynamic
	SetViewport(0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f);
	SetScissorRect(0, 0, 1, 1);
	SetMultisamples(VK_SAMPLE_COUNT_1_BIT);
}

VkPipeline Vulkan::GraphicsPipelineBuilder::Create(
	VkDevice device, VkPipelineCache pipeline_cache, bool clear /* = true */)
{
	VkPipeline pipeline;
	VkResult res = vkCreateGraphicsPipelines(device, pipeline_cache, 1, &m_ci, nullptr, &pipeline);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateGraphicsPipelines() failed: ");
		return VK_NULL_HANDLE;
	}

	if (clear)
		Clear();

	return pipeline;
}

void Vulkan::GraphicsPipelineBuilder::SetShaderStage(
	VkShaderStageFlagBits stage, VkShaderModule module, const char* entry_point)
{
	pxAssert(m_ci.stageCount < MAX_SHADER_STAGES);

	u32 index = 0;
	for (; index < m_ci.stageCount; index++)
	{
		if (m_shader_stages[index].stage == stage)
			break;
	}
	if (index == m_ci.stageCount)
	{
		m_ci.stageCount++;
		m_ci.pStages = m_shader_stages.data();
	}

	VkPipelineShaderStageCreateInfo& s = m_shader_stages[index];
	s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	s.stage = stage;
	s.module = module;
	s.pName = entry_point;
}

void Vulkan::GraphicsPipelineBuilder::AddVertexBuffer(
	u32 binding, u32 stride, VkVertexInputRate input_rate /*= VK_VERTEX_INPUT_RATE_VERTEX*/)
{
	pxAssert(m_vertex_input_state.vertexAttributeDescriptionCount < MAX_VERTEX_BUFFERS);

	VkVertexInputBindingDescription& b = m_vertex_buffers[m_vertex_input_state.vertexBindingDescriptionCount];
	b.binding = binding;
	b.stride = stride;
	b.inputRate = input_rate;

	m_vertex_input_state.vertexBindingDescriptionCount++;
	m_vertex_input_state.pVertexBindingDescriptions = m_vertex_buffers.data();
	m_ci.pVertexInputState = &m_vertex_input_state;
}

void Vulkan::GraphicsPipelineBuilder::AddVertexAttribute(u32 location, u32 binding, VkFormat format, u32 offset)
{
	pxAssert(m_vertex_input_state.vertexAttributeDescriptionCount < MAX_VERTEX_BUFFERS);

	VkVertexInputAttributeDescription& a = m_vertex_attributes[m_vertex_input_state.vertexAttributeDescriptionCount];
	a.location = location;
	a.binding = binding;
	a.format = format;
	a.offset = offset;

	m_vertex_input_state.vertexAttributeDescriptionCount++;
	m_vertex_input_state.pVertexAttributeDescriptions = m_vertex_attributes.data();
	m_ci.pVertexInputState = &m_vertex_input_state;
}

void Vulkan::GraphicsPipelineBuilder::SetPrimitiveTopology(
	VkPrimitiveTopology topology, bool enable_primitive_restart /*= false*/)
{
	m_input_assembly.topology = topology;
	m_input_assembly.primitiveRestartEnable = enable_primitive_restart;

	m_ci.pInputAssemblyState = &m_input_assembly;
}

void Vulkan::GraphicsPipelineBuilder::SetRasterizationState(
	VkPolygonMode polygon_mode, VkCullModeFlags cull_mode, VkFrontFace front_face)
{
	m_rasterization_state.polygonMode = polygon_mode;
	m_rasterization_state.cullMode = cull_mode;
	m_rasterization_state.frontFace = front_face;

	m_ci.pRasterizationState = &m_rasterization_state;
}

void Vulkan::GraphicsPipelineBuilder::SetLineWidth(float width)
{
	m_rasterization_state.lineWidth = width;
}

void Vulkan::GraphicsPipelineBuilder::SetLineRasterizationMode(VkLineRasterizationModeEXT mode)
{
	AddPointerToChain(&m_rasterization_state, &m_line_rasterization_state);

	m_line_rasterization_state.lineRasterizationMode = mode;
}

void Vulkan::GraphicsPipelineBuilder::SetMultisamples(u32 multisamples, bool per_sample_shading)
{
	m_multisample_state.rasterizationSamples = static_cast<VkSampleCountFlagBits>(multisamples);
	m_multisample_state.sampleShadingEnable = per_sample_shading;
	m_multisample_state.minSampleShading = (multisamples > 1) ? 1.0f : 0.0f;
}

void Vulkan::GraphicsPipelineBuilder::SetNoCullRasterizationState()
{
	SetRasterizationState(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
}

void Vulkan::GraphicsPipelineBuilder::SetDepthState(bool depth_test, bool depth_write, VkCompareOp compare_op)
{
	m_depth_state.depthTestEnable = depth_test;
	m_depth_state.depthWriteEnable = depth_write;
	m_depth_state.depthCompareOp = compare_op;

	m_ci.pDepthStencilState = &m_depth_state;
}

void Vulkan::GraphicsPipelineBuilder::SetStencilState(
	bool stencil_test, const VkStencilOpState& front, const VkStencilOpState& back)
{
	m_depth_state.stencilTestEnable = stencil_test;
	m_depth_state.front = front;
	m_depth_state.back = back;
}

void Vulkan::GraphicsPipelineBuilder::SetNoStencilState()
{
	m_depth_state.stencilTestEnable = VK_FALSE;
	m_depth_state.front = {};
	m_depth_state.back = {};
}

void Vulkan::GraphicsPipelineBuilder::SetNoDepthTestState()
{
	SetDepthState(false, false, VK_COMPARE_OP_ALWAYS);
}

void Vulkan::GraphicsPipelineBuilder::SetBlendConstants(float r, float g, float b, float a)
{
	m_blend_state.blendConstants[0] = r;
	m_blend_state.blendConstants[1] = g;
	m_blend_state.blendConstants[2] = b;
	m_blend_state.blendConstants[3] = a;
	m_ci.pColorBlendState = &m_blend_state;
}

void Vulkan::GraphicsPipelineBuilder::AddBlendAttachment(bool blend_enable, VkBlendFactor src_factor,
	VkBlendFactor dst_factor, VkBlendOp op, VkBlendFactor alpha_src_factor, VkBlendFactor alpha_dst_factor,
	VkBlendOp alpha_op,
	VkColorComponentFlags
		write_mask /* = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT */)
{
	pxAssert(m_blend_state.attachmentCount < MAX_ATTACHMENTS);

	VkPipelineColorBlendAttachmentState& bs = m_blend_attachments[m_blend_state.attachmentCount];
	bs.blendEnable = blend_enable;
	bs.srcColorBlendFactor = src_factor;
	bs.dstColorBlendFactor = dst_factor;
	bs.colorBlendOp = op;
	bs.srcAlphaBlendFactor = alpha_src_factor;
	bs.dstAlphaBlendFactor = alpha_dst_factor;
	bs.alphaBlendOp = alpha_op;
	bs.colorWriteMask = write_mask;

	m_blend_state.attachmentCount++;
	m_blend_state.pAttachments = m_blend_attachments.data();
	m_ci.pColorBlendState = &m_blend_state;
}

void Vulkan::GraphicsPipelineBuilder::SetBlendAttachment(u32 attachment, bool blend_enable, VkBlendFactor src_factor,
	VkBlendFactor dst_factor, VkBlendOp op, VkBlendFactor alpha_src_factor, VkBlendFactor alpha_dst_factor,
	VkBlendOp alpha_op,
	VkColorComponentFlags
		write_mask /*= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT*/)
{
	pxAssert(attachment < MAX_ATTACHMENTS);

	VkPipelineColorBlendAttachmentState& bs = m_blend_attachments[attachment];
	bs.blendEnable = blend_enable;
	bs.srcColorBlendFactor = src_factor;
	bs.dstColorBlendFactor = dst_factor;
	bs.colorBlendOp = op;
	bs.srcAlphaBlendFactor = alpha_src_factor;
	bs.dstAlphaBlendFactor = alpha_dst_factor;
	bs.alphaBlendOp = alpha_op;
	bs.colorWriteMask = write_mask;

	if (attachment >= m_blend_state.attachmentCount)
	{
		m_blend_state.attachmentCount = attachment + 1u;
		m_blend_state.pAttachments = m_blend_attachments.data();
		m_ci.pColorBlendState = &m_blend_state;
	}
}

void Vulkan::GraphicsPipelineBuilder::SetColorWriteMask(u32 attachment, VkColorComponentFlags write_mask)
{
	pxAssert(attachment < MAX_ATTACHMENTS);

	VkPipelineColorBlendAttachmentState& bs = m_blend_attachments[attachment];
	bs.colorWriteMask = write_mask;
}

void Vulkan::GraphicsPipelineBuilder::AddBlendFlags(u32 flags)
{
	m_blend_state.flags |= flags;
}

void Vulkan::GraphicsPipelineBuilder::ClearBlendAttachments()
{
	m_blend_attachments = {};
	m_blend_state.attachmentCount = 0;
}

void Vulkan::GraphicsPipelineBuilder::SetNoBlendingState()
{
	ClearBlendAttachments();
	SetBlendAttachment(0, false, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
}

void Vulkan::GraphicsPipelineBuilder::AddDynamicState(VkDynamicState state)
{
	pxAssert(m_dynamic_state.dynamicStateCount < MAX_DYNAMIC_STATE);

	m_dynamic_state_values[m_dynamic_state.dynamicStateCount] = state;
	m_dynamic_state.dynamicStateCount++;
	m_dynamic_state.pDynamicStates = m_dynamic_state_values.data();
	m_ci.pDynamicState = &m_dynamic_state;
}

void Vulkan::GraphicsPipelineBuilder::SetDynamicViewportAndScissorState()
{
	AddDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
	AddDynamicState(VK_DYNAMIC_STATE_SCISSOR);
}

void Vulkan::GraphicsPipelineBuilder::SetViewport(
	float x, float y, float width, float height, float min_depth, float max_depth)
{
	m_viewport.x = x;
	m_viewport.y = y;
	m_viewport.width = width;
	m_viewport.height = height;
	m_viewport.minDepth = min_depth;
	m_viewport.maxDepth = max_depth;

	m_viewport_state.pViewports = &m_viewport;
	m_viewport_state.viewportCount = 1u;
	m_ci.pViewportState = &m_viewport_state;
}

void Vulkan::GraphicsPipelineBuilder::SetScissorRect(s32 x, s32 y, u32 width, u32 height)
{
	m_scissor.offset.x = x;
	m_scissor.offset.y = y;
	m_scissor.extent.width = width;
	m_scissor.extent.height = height;

	m_viewport_state.pScissors = &m_scissor;
	m_viewport_state.scissorCount = 1u;
	m_ci.pViewportState = &m_viewport_state;
}

void Vulkan::GraphicsPipelineBuilder::SetMultisamples(VkSampleCountFlagBits samples)
{
	m_multisample_state.rasterizationSamples = samples;
	m_ci.pMultisampleState = &m_multisample_state;
}

void Vulkan::GraphicsPipelineBuilder::SetPipelineLayout(VkPipelineLayout layout)
{
	m_ci.layout = layout;
}

void Vulkan::GraphicsPipelineBuilder::SetRenderPass(VkRenderPass render_pass, u32 subpass)
{
	m_ci.renderPass = render_pass;
	m_ci.subpass = subpass;
}

void Vulkan::GraphicsPipelineBuilder::SetProvokingVertex(VkProvokingVertexModeEXT mode)
{
	AddPointerToChain(&m_rasterization_state, &m_provoking_vertex);

	m_provoking_vertex.provokingVertexMode = mode;
}

Vulkan::ComputePipelineBuilder::ComputePipelineBuilder()
{
	Clear();
}

void Vulkan::ComputePipelineBuilder::Clear()
{
	m_ci = {};
	m_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	m_si = {};
	m_smap_entries = {};
	m_smap_constants = {};
}

VkPipeline Vulkan::ComputePipelineBuilder::Create(
	VkDevice device, VkPipelineCache pipeline_cache /*= VK_NULL_HANDLE*/, bool clear /*= true*/)
{
	VkPipeline pipeline;
	VkResult res = vkCreateComputePipelines(device, pipeline_cache, 1, &m_ci, nullptr, &pipeline);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateComputePipelines() failed: ");
		return VK_NULL_HANDLE;
	}

	if (clear)
		Clear();

	return pipeline;
}

void Vulkan::ComputePipelineBuilder::SetShader(VkShaderModule module, const char* entry_point)
{
	m_ci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	m_ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	m_ci.stage.module = module;
	m_ci.stage.pName = entry_point;
}

void Vulkan::ComputePipelineBuilder::SetPipelineLayout(VkPipelineLayout layout)
{
	m_ci.layout = layout;
}

void Vulkan::ComputePipelineBuilder::SetSpecializationBool(u32 index, bool value)
{
	const u32 u32_value = static_cast<u32>(value);
	SetSpecializationValue(index, u32_value);
}

void Vulkan::ComputePipelineBuilder::SetSpecializationValue(u32 index, u32 value)
{
	if (m_si.mapEntryCount == 0)
	{
		m_si.pMapEntries = m_smap_entries.data();
		m_si.pData = m_smap_constants.data();
		m_ci.stage.pSpecializationInfo = &m_si;
	}

	m_smap_entries[m_si.mapEntryCount++] = {index, index * SPECIALIZATION_CONSTANT_SIZE, SPECIALIZATION_CONSTANT_SIZE};
	m_si.dataSize += SPECIALIZATION_CONSTANT_SIZE;
}

Vulkan::SamplerBuilder::SamplerBuilder()
{
	Clear();
}

void Vulkan::SamplerBuilder::Clear()
{
	m_ci = {};
	m_ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
}

VkSampler Vulkan::SamplerBuilder::Create(VkDevice device, bool clear /* = true */)
{
	VkSampler sampler;
	VkResult res = vkCreateSampler(device, &m_ci, nullptr, &sampler);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateSampler() failed: ");
		return VK_NULL_HANDLE;
	}

	return sampler;
}

void Vulkan::SamplerBuilder::SetFilter(VkFilter mag_filter, VkFilter min_filter, VkSamplerMipmapMode mip_filter)
{
	m_ci.magFilter = mag_filter;
	m_ci.minFilter = min_filter;
	m_ci.mipmapMode = mip_filter;
}

void Vulkan::SamplerBuilder::SetAddressMode(VkSamplerAddressMode u, VkSamplerAddressMode v, VkSamplerAddressMode w)
{
	m_ci.addressModeU = u;
	m_ci.addressModeV = v;
	m_ci.addressModeW = w;
}

void Vulkan::SamplerBuilder::SetPointSampler(
	VkSamplerAddressMode address_mode /* = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER */)
{
	Clear();
	SetFilter(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST);
	SetAddressMode(address_mode, address_mode, address_mode);
}

void Vulkan::SamplerBuilder::SetLinearSampler(
	bool mipmaps, VkSamplerAddressMode address_mode /* = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER */)
{
	Clear();
	SetFilter(
		VK_FILTER_LINEAR, VK_FILTER_LINEAR, mipmaps ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST);
	SetAddressMode(address_mode, address_mode, address_mode);

	if (mipmaps)
	{
		m_ci.minLod = std::numeric_limits<float>::min();
		m_ci.maxLod = std::numeric_limits<float>::max();
	}
}

Vulkan::DescriptorSetUpdateBuilder::DescriptorSetUpdateBuilder()
{
	Clear();
}

void Vulkan::DescriptorSetUpdateBuilder::Clear()
{
	m_writes = {};
	m_num_writes = 0;
}

void Vulkan::DescriptorSetUpdateBuilder::Update(VkDevice device, bool clear /*= true*/)
{
	pxAssert(m_num_writes > 0);

	vkUpdateDescriptorSets(device, m_num_writes, (m_num_writes > 0) ? m_writes.data() : nullptr, 0, nullptr);

	if (clear)
		Clear();
}

void Vulkan::DescriptorSetUpdateBuilder::PushUpdate(
	VkCommandBuffer cmdbuf, VkPipelineBindPoint bind_point, VkPipelineLayout layout, u32 set, bool clear /*= true*/)
{
	pxAssert(m_num_writes > 0);

	vkCmdPushDescriptorSetKHR(cmdbuf, bind_point, layout, set, m_num_writes, m_writes.data());

	if (clear)
		Clear();
}

void Vulkan::DescriptorSetUpdateBuilder::AddImageDescriptorWrite(VkDescriptorSet set, u32 binding, VkImageView view,
	VkImageLayout layout /*= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL*/)
{
	pxAssert(m_num_writes < MAX_WRITES && m_num_image_infos < MAX_IMAGE_INFOS);

	VkDescriptorImageInfo& ii = m_image_infos[m_num_image_infos++];
	ii.imageView = view;
	ii.imageLayout = layout;
	ii.sampler = VK_NULL_HANDLE;

	VkWriteDescriptorSet& dw = m_writes[m_num_writes++];
	dw.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	dw.dstSet = set;
	dw.dstBinding = binding;
	dw.descriptorCount = 1;
	dw.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	dw.pImageInfo = &ii;
}

void Vulkan::DescriptorSetUpdateBuilder::AddSamplerDescriptorWrite(VkDescriptorSet set, u32 binding, VkSampler sampler)
{
	pxAssert(m_num_writes < MAX_WRITES && m_num_image_infos < MAX_IMAGE_INFOS);

	VkDescriptorImageInfo& ii = m_image_infos[m_num_image_infos++];
	ii.imageView = VK_NULL_HANDLE;
	ii.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ii.sampler = sampler;

	VkWriteDescriptorSet& dw = m_writes[m_num_writes++];
	dw.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	dw.dstSet = set;
	dw.dstBinding = binding;
	dw.descriptorCount = 1;
	dw.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	dw.pImageInfo = &ii;
}

void Vulkan::DescriptorSetUpdateBuilder::AddSamplerDescriptorWrites(
	VkDescriptorSet set, u32 binding, const VkSampler* samplers, u32 num_samplers)
{
	pxAssert(m_num_writes < MAX_WRITES && (m_num_image_infos + num_samplers) < MAX_IMAGE_INFOS);

	VkWriteDescriptorSet& dw = m_writes[m_num_writes++];
	dw.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	dw.dstSet = set;
	dw.dstBinding = binding;
	dw.descriptorCount = num_samplers;
	dw.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	dw.pImageInfo = &m_image_infos[m_num_image_infos];

	for (u32 i = 0; i < num_samplers; i++)
	{
		VkDescriptorImageInfo& ii = m_image_infos[m_num_image_infos++];
		ii.imageView = VK_NULL_HANDLE;
		ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		ii.sampler = samplers[i];
	}
}

void Vulkan::DescriptorSetUpdateBuilder::AddCombinedImageSamplerDescriptorWrite(VkDescriptorSet set, u32 binding,
	VkImageView view, VkSampler sampler, VkImageLayout layout /*= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL*/)
{
	pxAssert(m_num_writes < MAX_WRITES && m_num_image_infos < MAX_IMAGE_INFOS);

	VkDescriptorImageInfo& ii = m_image_infos[m_num_image_infos++];
	ii.imageView = view;
	ii.imageLayout = layout;
	ii.sampler = sampler;

	VkWriteDescriptorSet& dw = m_writes[m_num_writes++];
	dw.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	dw.dstSet = set;
	dw.dstBinding = binding;
	dw.descriptorCount = 1;
	dw.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	dw.pImageInfo = &ii;
}

void Vulkan::DescriptorSetUpdateBuilder::AddCombinedImageSamplerDescriptorWrites(VkDescriptorSet set, u32 binding,
	const VkImageView* views, const VkSampler* samplers, u32 num_views,
	VkImageLayout layout /* = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL */)
{
	pxAssert(m_num_writes < MAX_WRITES && (m_num_image_infos + num_views) < MAX_IMAGE_INFOS);

	VkWriteDescriptorSet& dw = m_writes[m_num_writes++];
	dw.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	dw.dstSet = set;
	dw.dstBinding = binding;
	dw.descriptorCount = num_views;
	dw.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	dw.pImageInfo = &m_image_infos[m_num_image_infos];

	for (u32 i = 0; i < num_views; i++)
	{
		VkDescriptorImageInfo& ii = m_image_infos[m_num_image_infos++];
		ii.imageView = views[i];
		ii.sampler = samplers[i];
		ii.imageLayout = layout;
	}
}

void Vulkan::DescriptorSetUpdateBuilder::AddBufferDescriptorWrite(
	VkDescriptorSet set, u32 binding, VkDescriptorType dtype, VkBuffer buffer, u32 offset, u32 size)
{
	pxAssert(m_num_writes < MAX_WRITES && m_num_buffer_infos < MAX_BUFFER_INFOS);

	VkDescriptorBufferInfo& bi = m_buffer_infos[m_num_buffer_infos++];
	bi.buffer = buffer;
	bi.offset = offset;
	bi.range = size;

	VkWriteDescriptorSet& dw = m_writes[m_num_writes++];
	dw.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	dw.dstSet = set;
	dw.dstBinding = binding;
	dw.descriptorCount = 1;
	dw.descriptorType = dtype;
	dw.pBufferInfo = &bi;
}

void Vulkan::DescriptorSetUpdateBuilder::AddBufferViewDescriptorWrite(
	VkDescriptorSet set, u32 binding, VkDescriptorType dtype, VkBufferView view)
{
	pxAssert(m_num_writes < MAX_WRITES && m_num_views < MAX_VIEWS);

	VkBufferView& bi = m_views[m_num_views++];
	bi = view;

	VkWriteDescriptorSet& dw = m_writes[m_num_writes++];
	dw.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	dw.dstSet = set;
	dw.dstBinding = binding;
	dw.descriptorCount = 1;
	dw.descriptorType = dtype;
	dw.pTexelBufferView = &bi;
}

void Vulkan::DescriptorSetUpdateBuilder::AddInputAttachmentDescriptorWrite(
	VkDescriptorSet set, u32 binding, VkImageView view, VkImageLayout layout /*= VK_IMAGE_LAYOUT_GENERAL*/)
{
	pxAssert(m_num_writes < MAX_WRITES && m_num_image_infos < MAX_IMAGE_INFOS);

	VkWriteDescriptorSet& dw = m_writes[m_num_writes++];
	dw.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	dw.dstSet = set;
	dw.dstBinding = binding;
	dw.descriptorCount = 1;
	dw.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	dw.pImageInfo = &m_image_infos[m_num_image_infos];

	VkDescriptorImageInfo& ii = m_image_infos[m_num_image_infos++];
	ii.imageView = view;
	ii.imageLayout = layout;
	ii.sampler = VK_NULL_HANDLE;
}

void Vulkan::DescriptorSetUpdateBuilder::AddStorageImageDescriptorWrite(
	VkDescriptorSet set, u32 binding, VkImageView view, VkImageLayout layout /*= VK_IMAGE_LAYOUT_GENERAL*/)
{
	pxAssert(m_num_writes < MAX_WRITES && m_num_image_infos < MAX_IMAGE_INFOS);

	VkWriteDescriptorSet& dw = m_writes[m_num_writes++];
	dw.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	dw.dstSet = set;
	dw.dstBinding = binding;
	dw.descriptorCount = 1;
	dw.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	dw.pImageInfo = &m_image_infos[m_num_image_infos];

	VkDescriptorImageInfo& ii = m_image_infos[m_num_image_infos++];
	ii.imageView = view;
	ii.imageLayout = layout;
	ii.sampler = VK_NULL_HANDLE;
}

Vulkan::FramebufferBuilder::FramebufferBuilder()
{
	Clear();
}

void Vulkan::FramebufferBuilder::Clear()
{
	m_ci = {};
	m_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	m_images = {};
}

VkFramebuffer Vulkan::FramebufferBuilder::Create(VkDevice device, bool clear /*= true*/)
{
	VkFramebuffer fb;
	VkResult res = vkCreateFramebuffer(device, &m_ci, nullptr, &fb);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateFramebuffer() failed: ");
		return VK_NULL_HANDLE;
	}

	if (clear)
		Clear();

	return fb;
}

void Vulkan::FramebufferBuilder::AddAttachment(VkImageView image)
{
	pxAssert(m_ci.attachmentCount < MAX_ATTACHMENTS);

	m_images[m_ci.attachmentCount] = image;

	m_ci.attachmentCount++;
	m_ci.pAttachments = m_images.data();
}

void Vulkan::FramebufferBuilder::SetSize(u32 width, u32 height, u32 layers)
{
	m_ci.width = width;
	m_ci.height = height;
	m_ci.layers = layers;
}

void Vulkan::FramebufferBuilder::SetRenderPass(VkRenderPass render_pass)
{
	m_ci.renderPass = render_pass;
}

Vulkan::RenderPassBuilder::RenderPassBuilder()
{
	Clear();
}

void Vulkan::RenderPassBuilder::Clear()
{
	m_ci = {};
	m_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	m_attachments = {};
	m_attachment_references = {};
	m_num_attachment_references = 0;
	m_subpasses = {};
}

VkRenderPass Vulkan::RenderPassBuilder::Create(VkDevice device, bool clear /*= true*/)
{
	VkRenderPass rp;
	VkResult res = vkCreateRenderPass(device, &m_ci, nullptr, &rp);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateRenderPass() failed: ");
		return VK_NULL_HANDLE;
	}

	return rp;
}

u32 Vulkan::RenderPassBuilder::AddAttachment(VkFormat format, VkSampleCountFlagBits samples, VkAttachmentLoadOp load_op,
	VkAttachmentStoreOp store_op, VkImageLayout initial_layout, VkImageLayout final_layout)
{
	pxAssert(m_ci.attachmentCount < MAX_ATTACHMENTS);

	const u32 index = m_ci.attachmentCount;
	VkAttachmentDescription& ad = m_attachments[index];
	ad.format = format;
	ad.samples = samples;
	ad.loadOp = load_op;
	ad.storeOp = store_op;
	ad.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	ad.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	ad.initialLayout = initial_layout;
	ad.finalLayout = final_layout;

	m_ci.attachmentCount++;
	m_ci.pAttachments = m_attachments.data();

	return index;
}

u32 Vulkan::RenderPassBuilder::AddSubpass()
{
	pxAssert(m_ci.subpassCount < MAX_SUBPASSES);

	const u32 index = m_ci.subpassCount;
	VkSubpassDescription& sp = m_subpasses[index];
	sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	m_ci.subpassCount++;
	m_ci.pSubpasses = m_subpasses.data();

	return index;
}

void Vulkan::RenderPassBuilder::AddSubpassColorAttachment(u32 subpass, u32 attachment, VkImageLayout layout)
{
	pxAssert(subpass < m_ci.subpassCount && m_num_attachment_references < MAX_ATTACHMENT_REFERENCES);

	VkAttachmentReference& ar = m_attachment_references[m_num_attachment_references++];
	ar.attachment = attachment;
	ar.layout = layout;

	VkSubpassDescription& sp = m_subpasses[subpass];
	if (sp.colorAttachmentCount == 0)
		sp.pColorAttachments = &ar;
	sp.colorAttachmentCount++;
}

void Vulkan::RenderPassBuilder::AddSubpassDepthAttachment(u32 subpass, u32 attachment, VkImageLayout layout)
{
	pxAssert(subpass < m_ci.subpassCount && m_num_attachment_references < MAX_ATTACHMENT_REFERENCES);

	VkAttachmentReference& ar = m_attachment_references[m_num_attachment_references++];
	ar.attachment = attachment;
	ar.layout = layout;

	VkSubpassDescription& sp = m_subpasses[subpass];
	sp.pDepthStencilAttachment = &ar;
}

Vulkan::BufferViewBuilder::BufferViewBuilder()
{
	Clear();
}

void Vulkan::BufferViewBuilder::Clear()
{
	m_ci = {};
	m_ci.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
}

VkBufferView Vulkan::BufferViewBuilder::Create(VkDevice device, bool clear /*= true*/)
{
	VkBufferView bv;
	VkResult res = vkCreateBufferView(device, &m_ci, nullptr, &bv);
	if (res != VK_SUCCESS)
	{
		LOG_VULKAN_ERROR(res, "vkCreateBufferView() failed: ");
		return VK_NULL_HANDLE;
	}

	return bv;
}

void Vulkan::BufferViewBuilder::Set(VkBuffer buffer, VkFormat format, u32 offset, u32 size)
{
	m_ci.buffer = buffer;
	m_ci.format = format;
	m_ci.offset = offset;
	m_ci.range = size;
}
