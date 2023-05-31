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

#pragma once

#include "GS/Renderers/Common/GSDevice.h"
#include "GS/Renderers/Vulkan/GSTextureVK.h"
#include "GS/Renderers/Vulkan/VKLoader.h"
#include "GS/Renderers/Vulkan/VKStreamBuffer.h"
#include "GS/GSVector.h"

#include "common/HashCombine.h"

#include <array>
#include <unordered_map>

namespace Vulkan
{
class VKSwapChain;
}

class GSDeviceVK final : public GSDevice
{
public:
	enum FeedbackLoopFlag : u8
	{
		FeedbackLoopFlag_None = 0,
		FeedbackLoopFlag_ReadAndWriteRT = 1,
		FeedbackLoopFlag_ReadDS = 2,
	};

	struct alignas(8) PipelineSelector
	{
		GSHWDrawConfig::PSSelector ps;

		union
		{
			struct
			{
				u32 topology : 2;
				u32 rt : 1;
				u32 ds : 1;
				u32 line_width : 1;
				u32 feedback_loop_flags : 2;
			};

			u32 key;
		};

		GSHWDrawConfig::BlendState bs;
		GSHWDrawConfig::VSSelector vs;
		GSHWDrawConfig::DepthStencilSelector dss;
		GSHWDrawConfig::ColorMaskSelector cms;
		u8 pad;

		__fi bool operator==(const PipelineSelector& p) const { return BitEqual(*this, p); }
		__fi bool operator!=(const PipelineSelector& p) const { return !BitEqual(*this, p); }

		__fi PipelineSelector() { std::memset(this, 0, sizeof(*this)); }

		__fi bool IsRTFeedbackLoop() const { return ((feedback_loop_flags & FeedbackLoopFlag_ReadAndWriteRT) != 0); }
		__fi bool IsTestingAndSamplingDepth() const { return ((feedback_loop_flags & FeedbackLoopFlag_ReadDS) != 0); }
	};
	static_assert(sizeof(PipelineSelector) == 24, "Pipeline selector is 24 bytes");

	struct PipelineSelectorHash
	{
		std::size_t operator()(const PipelineSelector& e) const noexcept
		{
			std::size_t hash = 0;
			HashCombine(hash, e.vs.key, e.ps.key_hi, e.ps.key_lo, e.dss.key, e.cms.key, e.bs.key, e.key);
			return hash;
		}
	};

	enum : u32
	{
		NUM_TFX_DYNAMIC_OFFSETS = 2,
		NUM_TFX_DRAW_TEXTURES = 2,
		NUM_TFX_RT_TEXTURES = 2,
		NUM_TFX_TEXTURES = NUM_TFX_DRAW_TEXTURES + NUM_TFX_RT_TEXTURES,
		NUM_CONVERT_TEXTURES = 1,
		NUM_CONVERT_SAMPLERS = 1,
		CONVERT_PUSH_CONSTANTS_SIZE = 96,

		VERTEX_BUFFER_SIZE = 32 * 1024 * 1024,
		INDEX_BUFFER_SIZE = 16 * 1024 * 1024,
		VERTEX_UNIFORM_BUFFER_SIZE = 8 * 1024 * 1024,
		FRAGMENT_UNIFORM_BUFFER_SIZE = 8 * 1024 * 1024,

		NUM_CAS_PIPELINES = 2,
	};
	enum TFX_DESCRIPTOR_SET : u32
	{
		TFX_DESCRIPTOR_SET_UBO,
		TFX_DESCRIPTOR_SET_TEXTURES,
		TFX_DESCRIPTOR_SET_RT,

		NUM_TFX_DESCRIPTOR_SETS,
	};
	enum DATE_RENDER_PASS : u32
	{
		DATE_RENDER_PASS_NONE = 0,
		DATE_RENDER_PASS_STENCIL = 1,
		DATE_RENDER_PASS_STENCIL_ONE = 2,
	};

private:
	std::unique_ptr<VKSwapChain> m_swap_chain;

	VkDescriptorSetLayout m_utility_ds_layout = VK_NULL_HANDLE;
	VkPipelineLayout m_utility_pipeline_layout = VK_NULL_HANDLE;

	VkDescriptorSetLayout m_tfx_ubo_ds_layout = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_tfx_sampler_ds_layout = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_tfx_rt_texture_ds_layout = VK_NULL_HANDLE;
	VkPipelineLayout m_tfx_pipeline_layout = VK_NULL_HANDLE;

	VKStreamBuffer m_vertex_stream_buffer;
	VKStreamBuffer m_index_stream_buffer;
	VKStreamBuffer m_vertex_uniform_stream_buffer;
	VKStreamBuffer m_fragment_uniform_stream_buffer;
	VkBuffer m_expand_index_buffer = VK_NULL_HANDLE;
	VmaAllocation m_expand_index_buffer_allocation = VK_NULL_HANDLE;

	VkSampler m_point_sampler = VK_NULL_HANDLE;
	VkSampler m_linear_sampler = VK_NULL_HANDLE;

	std::unordered_map<u32, VkSampler> m_samplers;

	std::array<VkPipeline, static_cast<int>(ShaderConvert::Count)> m_convert{};
	std::array<VkPipeline, static_cast<int>(PresentShader::Count)> m_present{};
	std::array<VkPipeline, 16> m_color_copy{};
	std::array<VkPipeline, 2> m_merge{};
	std::array<VkPipeline, NUM_INTERLACE_SHADERS> m_interlace{};
	VkPipeline m_hdr_setup_pipelines[2][2] = {}; // [depth][feedback_loop]
	VkPipeline m_hdr_finish_pipelines[2][2] = {}; // [depth][feedback_loop]
	VkRenderPass m_date_image_setup_render_passes[2][2] = {}; // [depth][clear]
	VkPipeline m_date_image_setup_pipelines[2][2] = {}; // [depth][datm]
	VkPipeline m_fxaa_pipeline = {};
	VkPipeline m_shadeboost_pipeline = {};

	std::unordered_map<u32, VkShaderModule> m_tfx_vertex_shaders;
	std::unordered_map<GSHWDrawConfig::PSSelector, VkShaderModule, GSHWDrawConfig::PSSelectorHash> m_tfx_fragment_shaders;
	std::unordered_map<PipelineSelector, VkPipeline, PipelineSelectorHash> m_tfx_pipelines;

	VkRenderPass m_utility_color_render_pass_load = VK_NULL_HANDLE;
	VkRenderPass m_utility_color_render_pass_clear = VK_NULL_HANDLE;
	VkRenderPass m_utility_color_render_pass_discard = VK_NULL_HANDLE;
	VkRenderPass m_utility_depth_render_pass_load = VK_NULL_HANDLE;
	VkRenderPass m_utility_depth_render_pass_clear = VK_NULL_HANDLE;
	VkRenderPass m_utility_depth_render_pass_discard = VK_NULL_HANDLE;
	VkRenderPass m_date_setup_render_pass = VK_NULL_HANDLE;
	VkRenderPass m_swap_chain_render_pass = VK_NULL_HANDLE;

	VkRenderPass m_tfx_render_pass[2][2][2][3][2][2][3][3] = {}; // [rt][ds][hdr][date][fbl][dsp][rt_op][ds_op]

	VkDescriptorSetLayout m_cas_ds_layout = VK_NULL_HANDLE;
	VkPipelineLayout m_cas_pipeline_layout = VK_NULL_HANDLE;
	std::array<VkPipeline, NUM_CAS_PIPELINES> m_cas_pipelines = {};
	VkPipeline m_imgui_pipeline = VK_NULL_HANDLE;

	GSHWDrawConfig::VSConstantBuffer m_vs_cb_cache;
	GSHWDrawConfig::PSConstantBuffer m_ps_cb_cache;

	std::string m_tfx_source;

	GSTexture* CreateSurface(GSTexture::Type type, int width, int height, int levels, GSTexture::Format format) override;

	void DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect, const GSRegPMODE& PMODE,
		const GSRegEXTBUF& EXTBUF, const GSVector4& c, const bool linear) final;
	void DoInterlace(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ShaderInterlace shader, bool linear, const InterlaceConstantBuffer& cb) final;
	void DoShadeBoost(GSTexture* sTex, GSTexture* dTex, const float params[4]) final;
	void DoFXAA(GSTexture* sTex, GSTexture* dTex) final;

	bool DoCAS(GSTexture* sTex, GSTexture* dTex, bool sharpen_only, const std::array<u32, NUM_CAS_CONSTANTS>& constants) final;

	VkSampler GetSampler(GSHWDrawConfig::SamplerSelector ss);
	void ClearSamplerCache() final;

	VkShaderModule GetTFXVertexShader(GSHWDrawConfig::VSSelector sel);
	VkShaderModule GetTFXFragmentShader(const GSHWDrawConfig::PSSelector& sel);
	VkPipeline CreateTFXPipeline(const PipelineSelector& p);
	VkPipeline GetTFXPipeline(const PipelineSelector& p);

	VkShaderModule GetUtilityVertexShader(const std::string& source, const char* replace_main);
	VkShaderModule GetUtilityFragmentShader(const std::string& source, const char* replace_main);

	bool CreateDeviceAndSwapChain();
	bool CheckFeatures();
	bool CreateNullTexture();
	bool CreateBuffers();
	bool CreatePipelineLayouts();
	bool CreateRenderPasses();

	bool CompileConvertPipelines();
	bool CompilePresentPipelines();
	bool CompileInterlacePipelines();
	bool CompileMergePipelines();
	bool CompilePostProcessingPipelines();
	bool CompileCASPipelines();

	bool CompileImGuiPipeline();
	void RenderImGui();
	void RenderBlankFrame();

	void DestroyResources();

public:
	GSDeviceVK();
	~GSDeviceVK() override;

	__fi static GSDeviceVK* GetInstance() { return static_cast<GSDeviceVK*>(g_gs_device.get()); }

	static void GetAdaptersAndFullscreenModes(std::vector<std::string>* adapters, std::vector<std::string>* fullscreen_modes);

	/// Returns true if Vulkan is suitable as a default for the devices in the system.
	static bool IsSuitableDefaultRenderer();

	__fi VkRenderPass GetTFXRenderPass(bool rt, bool ds, bool hdr, DATE_RENDER_PASS date, bool fbl, bool dsp,
		VkAttachmentLoadOp rt_op, VkAttachmentLoadOp ds_op) const
	{
		return m_tfx_render_pass[rt][ds][hdr][date][fbl][dsp][rt_op][ds_op];
	}
	__fi VkSampler GetPointSampler() const { return m_point_sampler; }
	__fi VkSampler GetLinearSampler() const { return m_linear_sampler; }

	RenderAPI GetRenderAPI() const override;
	bool HasSurface() const override;

	bool Create() override;
	void Destroy() override;

	bool UpdateWindow() override;
	void ResizeWindow(s32 new_window_width, s32 new_window_height, float new_window_scale) override;
	bool SupportsExclusiveFullscreen() const override;
	void DestroySurface() override;
	std::string GetDriverInfo() const override;

	void SetVSync(VsyncMode mode) override;

	PresentResult BeginPresent(bool frame_skip) override;
	void EndPresent() override;

	bool SetGPUTimingEnabled(bool enabled) override;
	float GetAndResetAccumulatedGPUTime() override;

	void PushDebugGroup(const char* fmt, ...) override;
	void PopDebugGroup() override;
	void InsertDebugMessage(DebugMessageCategory category, const char* fmt, ...) override;

	void DrawPrimitive();
	void DrawIndexedPrimitive();
	void DrawIndexedPrimitive(int offset, int count);

	void ClearRenderTarget(GSTexture* t, const GSVector4& c) override;
	void ClearRenderTarget(GSTexture* t, u32 c) override;
	void InvalidateRenderTarget(GSTexture* t) override;
	void ClearDepth(GSTexture* t) override;

	std::unique_ptr<GSDownloadTexture> CreateDownloadTexture(u32 width, u32 height, GSTexture::Format format) override;

	void CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r, u32 destX, u32 destY) override;

	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect,
		ShaderConvert shader = ShaderConvert::COPY, bool linear = true) override;
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, bool red,
		bool green, bool blue, bool alpha) override;
	void PresentRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect,
		PresentShader shader, float shaderTime, bool linear) override;
	void DrawMultiStretchRects(const MultiStretchRect* rects, u32 num_rects, GSTexture* dTex, ShaderConvert shader) override;
	void DoMultiStretchRects(const MultiStretchRect* rects, u32 num_rects, GSTextureVK* dTex, ShaderConvert shader);

	void BeginRenderPassForStretchRect(
		GSTextureVK* dTex, const GSVector4i& dtex_rc, const GSVector4i& dst_rc, bool allow_discard = true);
	void DoStretchRect(GSTextureVK* sTex, const GSVector4& sRect, GSTextureVK* dTex, const GSVector4& dRect,
		VkPipeline pipeline, bool linear, bool allow_discard);
	void DrawStretchRect(const GSVector4& sRect, const GSVector4& dRect, const GSVector2i& ds);

	void BlitRect(GSTexture* sTex, const GSVector4i& sRect, u32 sLevel, GSTexture* dTex, const GSVector4i& dRect,
		u32 dLevel, bool linear);

	void UpdateCLUTTexture(GSTexture* sTex, float sScale, u32 offsetX, u32 offsetY, GSTexture* dTex, u32 dOffset, u32 dSize) override;
	void ConvertToIndexedTexture(GSTexture* sTex, float sScale, u32 offsetX, u32 offsetY, u32 SBW, u32 SPSM, GSTexture* dTex, u32 DBW, u32 DPSM) override;

	void SetupDATE(GSTexture* rt, GSTexture* ds, bool datm, const GSVector4i& bbox);
	GSTextureVK* SetupPrimitiveTrackingDATE(GSHWDrawConfig& config);

	void IASetVertexBuffer(const void* vertex, size_t stride, size_t count);
	void IASetIndexBuffer(const void* index, size_t count);

	void PSSetShaderResource(int i, GSTexture* sr, bool check_state);
	void PSSetSampler(GSHWDrawConfig::SamplerSelector sel);

	void OMSetRenderTargets(GSTexture* rt, GSTexture* ds, const GSVector4i& scissor,
		FeedbackLoopFlag feedback_loop = FeedbackLoopFlag_None);

	void SetVSConstantBuffer(const GSHWDrawConfig::VSConstantBuffer& cb);
	void SetPSConstantBuffer(const GSHWDrawConfig::PSConstantBuffer& cb);
	bool BindDrawPipeline(const PipelineSelector& p);

	void RenderHW(GSHWDrawConfig& config) override;
	void UpdateHWPipelineSelector(GSHWDrawConfig& config, PipelineSelector& pipe);
	void UploadHWDrawVerticesAndIndices(const GSHWDrawConfig& config);
	void SendHWDraw(const GSHWDrawConfig& config, GSTextureVK* draw_rt, bool skip_first_barrier);

	//////////////////////////////////////////////////////////////////////////
	// Vulkan State
	//////////////////////////////////////////////////////////////////////////

public:
	VkFormat LookupNativeFormat(GSTexture::Format format) const;

	__fi VkFramebuffer GetCurrentFramebuffer() const { return m_current_framebuffer; }

	/// Ends any render pass, executes the command buffer, and invalidates cached state.
	void ExecuteCommandBuffer(bool wait_for_completion);
	void ExecuteCommandBuffer(bool wait_for_completion, const char* reason, ...);
	void ExecuteCommandBufferAndRestartRenderPass(bool wait_for_completion, const char* reason);
	void ExecuteCommandBufferForReadback();

	/// Set dirty flags on everything to force re-bind at next draw time.
	void InvalidateCachedState();

	/// Binds all dirty state to the command buffer.
	bool ApplyUtilityState(bool already_execed = false);
	bool ApplyTFXState(bool already_execed = false);

	void SetVertexBuffer(VkBuffer buffer, VkDeviceSize offset);
	void SetIndexBuffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType type);
	void SetBlendConstants(u8 color);

	void SetUtilityTexture(GSTexture* tex, VkSampler sampler);
	void SetUtilityPushConstants(const void* data, u32 size);
	void UnbindTexture(GSTextureVK* tex);

	// Ends a render pass if we're currently in one.
	// When Bind() is next called, the pass will be restarted.
	// Calling this function is allowed even if a pass has not begun.
	bool InRenderPass();
	void BeginRenderPass(VkRenderPass rp, const GSVector4i& rect);
	void BeginClearRenderPass(VkRenderPass rp, const GSVector4i& rect, const VkClearValue* cv, u32 cv_count);
	void BeginClearRenderPass(VkRenderPass rp, const GSVector4i& rect, const GSVector4& clear_color);
	void BeginClearRenderPass(VkRenderPass rp, const GSVector4i& rect, float depth, u8 stencil);
	void EndRenderPass();

	void SetViewport(const VkViewport& viewport);
	void SetScissor(const GSVector4i& scissor);
	void SetPipeline(VkPipeline pipeline);

private:
	enum DIRTY_FLAG : u32
	{
		DIRTY_FLAG_TFX_SAMPLERS_DS = (1 << 0),
		DIRTY_FLAG_TFX_RT_TEXTURE_DS = (1 << 1),
		DIRTY_FLAG_TFX_DYNAMIC_OFFSETS = (1 << 2),
		DIRTY_FLAG_UTILITY_TEXTURE = (1 << 3),
		DIRTY_FLAG_BLEND_CONSTANTS = (1 << 4),
		DIRTY_FLAG_VERTEX_BUFFER = (1 << 5),
		DIRTY_FLAG_INDEX_BUFFER = (1 << 6),
		DIRTY_FLAG_VIEWPORT = (1 << 7),
		DIRTY_FLAG_SCISSOR = (1 << 8),
		DIRTY_FLAG_PIPELINE = (1 << 9),
		DIRTY_FLAG_VS_CONSTANT_BUFFER = (1 << 10),
		DIRTY_FLAG_PS_CONSTANT_BUFFER = (1 << 11),

		DIRTY_BASE_STATE = DIRTY_FLAG_VERTEX_BUFFER | DIRTY_FLAG_INDEX_BUFFER | DIRTY_FLAG_PIPELINE |
						   DIRTY_FLAG_VIEWPORT | DIRTY_FLAG_SCISSOR | DIRTY_FLAG_BLEND_CONSTANTS,
		DIRTY_TFX_STATE = DIRTY_BASE_STATE | DIRTY_FLAG_TFX_SAMPLERS_DS | DIRTY_FLAG_TFX_RT_TEXTURE_DS,
		DIRTY_UTILITY_STATE = DIRTY_BASE_STATE | DIRTY_FLAG_UTILITY_TEXTURE,
		DIRTY_CONSTANT_BUFFER_STATE = DIRTY_FLAG_VS_CONSTANT_BUFFER | DIRTY_FLAG_PS_CONSTANT_BUFFER,
	};

	enum class PipelineLayout
	{
		Undefined,
		TFX,
		Utility
	};

	void InitializeState();
	bool CreatePersistentDescriptorSets();

	void ApplyBaseState(u32 flags, VkCommandBuffer cmdbuf);

	// Which bindings/state has to be updated before the next draw.
	u32 m_dirty_flags = 0;
	FeedbackLoopFlag m_current_framebuffer_feedback_loop = FeedbackLoopFlag_None;
	bool m_warned_slow_spin = false;

	// input assembly
	VkBuffer m_vertex_buffer = VK_NULL_HANDLE;
	VkDeviceSize m_vertex_buffer_offset = 0;
	VkBuffer m_index_buffer = VK_NULL_HANDLE;
	VkDeviceSize m_index_buffer_offset = 0;
	VkIndexType m_index_type = VK_INDEX_TYPE_UINT16;

	GSTextureVK* m_current_render_target = nullptr;
	GSTextureVK* m_current_depth_target = nullptr;
	VkFramebuffer m_current_framebuffer = VK_NULL_HANDLE;
	VkRenderPass m_current_render_pass = VK_NULL_HANDLE;
	GSVector4i m_current_render_pass_area = GSVector4i::zero();

	VkViewport m_viewport = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
	GSVector4i m_scissor = GSVector4i::zero();
	u8 m_blend_constant_color = 0;

	std::array<const GSTextureVK*, NUM_TFX_TEXTURES> m_tfx_textures{};
	VkSampler m_tfx_sampler = VK_NULL_HANDLE;
	u32 m_tfx_sampler_sel = 0;
	VkDescriptorSet m_tfx_ubo_descriptor_set = VK_NULL_HANDLE;
	VkDescriptorSet m_tfx_texture_descriptor_set = VK_NULL_HANDLE;
	VkDescriptorSet m_tfx_rt_descriptor_set = VK_NULL_HANDLE;
	std::array<u32, NUM_TFX_DYNAMIC_OFFSETS> m_tfx_dynamic_offsets{};

	const GSTextureVK* m_utility_texture = nullptr;
	VkSampler m_utility_sampler = VK_NULL_HANDLE;
	VkDescriptorSet m_utility_descriptor_set = VK_NULL_HANDLE;

	PipelineLayout m_current_pipeline_layout = PipelineLayout::Undefined;
	VkPipeline m_current_pipeline = VK_NULL_HANDLE;

	std::unique_ptr<GSTextureVK> m_null_texture;

	// current pipeline selector - we save this in the struct to avoid re-zeroing it every draw
	PipelineSelector m_pipeline_selector = {};
};
