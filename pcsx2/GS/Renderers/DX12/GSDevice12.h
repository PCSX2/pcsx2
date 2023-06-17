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

#include "GS/GSVector.h"
#include "GS/Renderers/Common/GSDevice.h"
#include "GS/Renderers/DX12/GSTexture12.h"
#include "GS/Renderers/DX12/D3D12ShaderCache.h"
#include "GS/Renderers/DX12/D3D12StreamBuffer.h"
#include "common/HashCombine.h"
#include <array>
#include <unordered_map>

namespace D3D12MA
{
	class Allocation;
}

class GSDevice12 final : public GSDevice
{
public:
	template <typename T>
	using ComPtr = wil::com_ptr_nothrow<T>;

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

	class ShaderMacro
	{
		struct mcstr
		{
			const char *name, *def;
			mcstr(const char* n, const char* d)
				: name(n)
				, def(d)
			{
			}
		};

		struct mstring
		{
			std::string name, def;
			mstring(const char* n, std::string d)
				: name(n)
				, def(d)
			{
			}
		};

		std::vector<mstring> mlist;
		std::vector<mcstr> mout;

	public:
		ShaderMacro(D3D_FEATURE_LEVEL fl);
		void AddMacro(const char* n, int d);
		void AddMacro(const char* n, std::string d);
		D3D_SHADER_MACRO* GetPtr(void);
	};

	enum : u32
	{
		NUM_TFX_CONSTANT_BUFFERS = 2,
		NUM_TFX_TEXTURES = 2,
		NUM_TFX_RT_TEXTURES = 2,
		NUM_TOTAL_TFX_TEXTURES = NUM_TFX_TEXTURES + NUM_TFX_RT_TEXTURES,
		NUM_TFX_SAMPLERS = 1,
		NUM_UTILITY_TEXTURES = 1,
		NUM_UTILITY_SAMPLERS = 1,
		CONVERT_PUSH_CONSTANTS_SIZE = 96,

		VERTEX_BUFFER_SIZE = 32 * 1024 * 1024,
		INDEX_BUFFER_SIZE = 16 * 1024 * 1024,
		VERTEX_UNIFORM_BUFFER_SIZE = 8 * 1024 * 1024,
		FRAGMENT_UNIFORM_BUFFER_SIZE = 8 * 1024 * 1024,

		TFX_ROOT_SIGNATURE_PARAM_VS_CBV = 0,
		TFX_ROOT_SIGNATURE_PARAM_PS_CBV = 1,
		TFX_ROOT_SIGNATURE_PARAM_VS_SRV = 2,
		TFX_ROOT_SIGNATURE_PARAM_PS_TEXTURES = 3,
		TFX_ROOT_SIGNATURE_PARAM_PS_SAMPLERS = 4,
		TFX_ROOT_SIGNATURE_PARAM_PS_RT_TEXTURES = 5,

		UTILITY_ROOT_SIGNATURE_PARAM_PUSH_CONSTANTS = 0,
		UTILITY_ROOT_SIGNATURE_PARAM_PS_TEXTURES = 1,
		UTILITY_ROOT_SIGNATURE_PARAM_PS_SAMPLERS = 2,

		CAS_ROOT_SIGNATURE_PARAM_PUSH_CONSTANTS = 0,
		CAS_ROOT_SIGNATURE_PARAM_SRC_TEXTURE = 1,
		CAS_ROOT_SIGNATURE_PARAM_DST_TEXTURE = 2
	};

private:
	ComPtr<IDXGIFactory5> m_dxgi_factory;
	ComPtr<IDXGISwapChain1> m_swap_chain;
	std::vector<std::unique_ptr<GSTexture12>> m_swap_chain_buffers;
	u32 m_current_swap_chain_buffer = 0;

	bool m_allow_tearing_supported = false;
	bool m_using_allow_tearing = false;
	bool m_is_exclusive_fullscreen = false;
	bool m_device_lost = false;

	ComPtr<ID3D12RootSignature> m_tfx_root_signature;
	ComPtr<ID3D12RootSignature> m_utility_root_signature;

	D3D12StreamBuffer m_vertex_stream_buffer;
	D3D12StreamBuffer m_index_stream_buffer;
	D3D12StreamBuffer m_vertex_constant_buffer;
	D3D12StreamBuffer m_pixel_constant_buffer;
	ComPtr<ID3D12Resource> m_expand_index_buffer;
	ComPtr<D3D12MA::Allocation> m_expand_index_buffer_allocation;

	D3D12DescriptorHandle m_point_sampler_cpu;
	D3D12DescriptorHandle m_linear_sampler_cpu;

	std::unordered_map<u32, D3D12DescriptorHandle> m_samplers;

	std::array<ComPtr<ID3D12PipelineState>, static_cast<int>(ShaderConvert::Count)> m_convert{};
	std::array<ComPtr<ID3D12PipelineState>, static_cast<int>(PresentShader::Count)> m_present{};
	std::array<ComPtr<ID3D12PipelineState>, 16> m_color_copy{};
	std::array<ComPtr<ID3D12PipelineState>, 2> m_merge{};
	std::array<ComPtr<ID3D12PipelineState>, NUM_INTERLACE_SHADERS> m_interlace{};
	std::array<ComPtr<ID3D12PipelineState>, 2> m_hdr_setup_pipelines{}; // [depth]
	std::array<ComPtr<ID3D12PipelineState>, 2> m_hdr_finish_pipelines{}; // [depth]
	std::array<std::array<ComPtr<ID3D12PipelineState>, 2>, 2> m_date_image_setup_pipelines{}; // [depth][datm]
	ComPtr<ID3D12PipelineState> m_fxaa_pipeline;
	ComPtr<ID3D12PipelineState> m_shadeboost_pipeline;
	ComPtr<ID3D12PipelineState> m_imgui_pipeline;

	std::unordered_map<u32, ComPtr<ID3DBlob>> m_tfx_vertex_shaders;
	std::unordered_map<GSHWDrawConfig::PSSelector, ComPtr<ID3DBlob>, GSHWDrawConfig::PSSelectorHash>
		m_tfx_pixel_shaders;
	std::unordered_map<PipelineSelector, ComPtr<ID3D12PipelineState>, PipelineSelectorHash> m_tfx_pipelines;

	ComPtr<ID3D12RootSignature> m_cas_root_signature;
	ComPtr<ID3D12PipelineState> m_cas_upscale_pipeline;
	ComPtr<ID3D12PipelineState> m_cas_sharpen_pipeline;

	GSHWDrawConfig::VSConstantBuffer m_vs_cb_cache;
	GSHWDrawConfig::PSConstantBuffer m_ps_cb_cache;

	D3D12ShaderCache m_shader_cache;
	ComPtr<ID3DBlob> m_convert_vs;
	std::string m_tfx_source;

	void LookupNativeFormat(GSTexture::Format format, DXGI_FORMAT* d3d_format, DXGI_FORMAT* srv_format,
		DXGI_FORMAT* rtv_format, DXGI_FORMAT* dsv_format) const;

	bool CreateSwapChain();
	bool CreateSwapChainRTV();
	void DestroySwapChainRTVs();
	void DestroySwapChain();

	GSTexture* CreateSurface(
		GSTexture::Type type, int width, int height, int levels, GSTexture::Format format) override;

	void DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect, const GSRegPMODE& PMODE,
		const GSRegEXTBUF& EXTBUF, u32 c, const bool linear) final;
	void DoInterlace(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect,
		ShaderInterlace shader, bool linear, const InterlaceConstantBuffer& cb) final;
	void DoShadeBoost(GSTexture* sTex, GSTexture* dTex, const float params[4]) final;
	void DoFXAA(GSTexture* sTex, GSTexture* dTex) final;

	bool DoCAS(
		GSTexture* sTex, GSTexture* dTex, bool sharpen_only, const std::array<u32, NUM_CAS_CONSTANTS>& constants) final;

	bool GetSampler(D3D12DescriptorHandle* cpu_handle, GSHWDrawConfig::SamplerSelector ss);
	void ClearSamplerCache() final;
	bool GetTextureGroupDescriptors(
		D3D12DescriptorHandle* gpu_handle, const D3D12DescriptorHandle* cpu_handles, u32 count);

	const ID3DBlob* GetTFXVertexShader(GSHWDrawConfig::VSSelector sel);
	const ID3DBlob* GetTFXPixelShader(const GSHWDrawConfig::PSSelector& sel);
	ComPtr<ID3D12PipelineState> CreateTFXPipeline(const PipelineSelector& p);
	const ID3D12PipelineState* GetTFXPipeline(const PipelineSelector& p);

	ComPtr<ID3DBlob> GetUtilityVertexShader(const std::string& source, const char* entry_point);
	ComPtr<ID3DBlob> GetUtilityPixelShader(const std::string& source, const char* entry_point);

	bool CheckFeatures();
	bool CreateNullTexture();
	bool CreateBuffers();
	bool CreateRootSignatures();

	bool CompileConvertPipelines();
	bool CompilePresentPipelines();
	bool CompileInterlacePipelines();
	bool CompileMergePipelines();
	bool CompilePostProcessingPipelines();
	bool CompileCASPipelines();

	bool CompileImGuiPipeline();
	void RenderImGui();

	void DestroyResources();

public:
	GSDevice12();
	~GSDevice12() override;

	__fi static GSDevice12* GetInstance() { return static_cast<GSDevice12*>(g_gs_device.get()); }

	RenderAPI GetRenderAPI() const override;
	bool HasSurface() const override;

	bool Create() override;
	void Destroy() override;

	bool UpdateWindow() override;
	void ResizeWindow(s32 new_window_width, s32 new_window_height, float new_window_scale) override;
	bool SupportsExclusiveFullscreen() const override;
	void DestroySurface() override;
	std::string GetDriverInfo() const override;

	bool GetHostRefreshRate(float* refresh_rate) override;

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

	void ClearRenderTarget(GSTexture* t, u32 c) override;
	void InvalidateRenderTarget(GSTexture* t) override;
	void ClearDepth(GSTexture* t, float d) override;

	std::unique_ptr<GSDownloadTexture> CreateDownloadTexture(u32 width, u32 height, GSTexture::Format format) override;

	void CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r, u32 destX, u32 destY) override;

	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect,
		ShaderConvert shader = ShaderConvert::COPY, bool linear = true) override;
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, bool red,
		bool green, bool blue, bool alpha) override;
	void PresentRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect,
		PresentShader shader, float shaderTime, bool linear) override;
	void UpdateCLUTTexture(
		GSTexture* sTex, float sScale, u32 offsetX, u32 offsetY, GSTexture* dTex, u32 dOffset, u32 dSize) override;
	void ConvertToIndexedTexture(GSTexture* sTex, float sScale, u32 offsetX, u32 offsetY, u32 SBW, u32 SPSM,
		GSTexture* dTex, u32 DBW, u32 DPSM) override;

	void DrawMultiStretchRects(
		const MultiStretchRect* rects, u32 num_rects, GSTexture* dTex, ShaderConvert shader) override;
	void DoMultiStretchRects(const MultiStretchRect* rects, u32 num_rects, GSTexture12* dTex, ShaderConvert shader);

	void BeginRenderPassForStretchRect(
		GSTexture12* dTex, const GSVector4i& dtex_rc, const GSVector4i& dst_rc, bool allow_discard = true);
	void DoStretchRect(GSTexture12* sTex, const GSVector4& sRect, GSTexture12* dTex, const GSVector4& dRect,
		const ID3D12PipelineState* pipeline, bool linear, bool allow_discard);
	void DrawStretchRect(const GSVector4& sRect, const GSVector4& dRect, const GSVector2i& ds);

	void SetupDATE(GSTexture* rt, GSTexture* ds, bool datm, const GSVector4i& bbox);
	GSTexture12* SetupPrimitiveTrackingDATE(GSHWDrawConfig& config, PipelineSelector& pipe);

	void IASetVertexBuffer(const void* vertex, size_t stride, size_t count);
	void IASetIndexBuffer(const void* index, size_t count);

	void PSSetShaderResource(int i, GSTexture* sr, bool check_state);
	void PSSetSampler(GSHWDrawConfig::SamplerSelector sel);

	void OMSetRenderTargets(GSTexture* rt, GSTexture* ds, const GSVector4i& scissor);

	void SetVSConstantBuffer(const GSHWDrawConfig::VSConstantBuffer& cb);
	void SetPSConstantBuffer(const GSHWDrawConfig::PSConstantBuffer& cb);
	bool BindDrawPipeline(const PipelineSelector& p);

	void RenderHW(GSHWDrawConfig& config) override;
	void UpdateHWPipelineSelector(GSHWDrawConfig& config);
	void UploadHWDrawVerticesAndIndices(const GSHWDrawConfig& config);

public:
	/// Ends any render pass, executes the command buffer, and invalidates cached state.
	void ExecuteCommandList(bool wait_for_completion);
	void ExecuteCommandList(bool wait_for_completion, const char* reason, ...);
	void ExecuteCommandListAndRestartRenderPass(bool wait_for_completion, const char* reason);
	void ExecuteCommandListForReadback();

	/// Set dirty flags on everything to force re-bind at next draw time.
	void InvalidateCachedState();

	/// Binds all dirty state to the command buffer.
	bool ApplyUtilityState(bool already_execed = false);
	bool ApplyTFXState(bool already_execed = false);

	void SetVertexBuffer(D3D12_GPU_VIRTUAL_ADDRESS buffer, size_t size, size_t stride);
	void SetIndexBuffer(D3D12_GPU_VIRTUAL_ADDRESS buffer, size_t size, DXGI_FORMAT type);
	void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topology);
	void SetBlendConstants(u8 color);
	void SetStencilRef(u8 ref);

	void SetUtilityRootSignature();
	void SetUtilityTexture(GSTexture* tex, const D3D12DescriptorHandle& sampler);
	void SetUtilityPushConstants(const void* data, u32 size);
	void UnbindTexture(GSTexture12* tex);

	// Assumes that the previous level has been transitioned to PS resource,
	// and the current level has been transitioned to RT.
	void RenderTextureMipmap(GSTexture12* texture, u32 dst_level, u32 dst_width, u32 dst_height, u32 src_level,
		u32 src_width, u32 src_height);

	// Ends a render pass if we're currently in one.
	// When Bind() is next called, the pass will be restarted.
	// Calling this function is allowed even if a pass has not begun.
	bool InRenderPass();
	void BeginRenderPass(
		D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE color_begin = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS,
		D3D12_RENDER_PASS_ENDING_ACCESS_TYPE color_end = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
		D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE depth_begin = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS,
		D3D12_RENDER_PASS_ENDING_ACCESS_TYPE depth_end = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
		D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE stencil_begin = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS,
		D3D12_RENDER_PASS_ENDING_ACCESS_TYPE stencil_end = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
		const u32 clear_color = 0, float clear_depth = 0.0f, u8 clear_stencil = 0);
	void EndRenderPass();

	void SetViewport(const D3D12_VIEWPORT& viewport);
	void SetScissor(const GSVector4i& scissor);
	void SetPipeline(const ID3D12PipelineState* pipeline);

private:
	enum DIRTY_FLAG : u32
	{
		DIRTY_FLAG_VS_CONSTANT_BUFFER = (1 << 0),
		DIRTY_FLAG_PS_CONSTANT_BUFFER = (1 << 1),
		DIRTY_FLAG_TFX_TEXTURES = (1 << 2),
		DIRTY_FLAG_TFX_SAMPLERS = (1 << 3),
		DIRTY_FLAG_TFX_RT_TEXTURES = (1 << 4),

		DIRTY_FLAG_VS_CONSTANT_BUFFER_BINDING = (1 << 5),
		DIRTY_FLAG_PS_CONSTANT_BUFFER_BINDING = (1 << 6),
		DIRTY_FLAG_VS_VERTEX_BUFFER_BINDING = (1 << 7),
		DIRTY_FLAG_TEXTURES_DESCRIPTOR_TABLE = (1 << 8),
		DIRTY_FLAG_SAMPLERS_DESCRIPTOR_TABLE = (1 << 9),
		DIRTY_FLAG_TEXTURES_DESCRIPTOR_TABLE_2 = (1 << 10),

		DIRTY_FLAG_VERTEX_BUFFER = (1 << 11),
		DIRTY_FLAG_INDEX_BUFFER = (1 << 12),
		DIRTY_FLAG_PRIMITIVE_TOPOLOGY = (1 << 13),
		DIRTY_FLAG_VIEWPORT = (1 << 14),
		DIRTY_FLAG_SCISSOR = (1 << 15),
		DIRTY_FLAG_RENDER_TARGET = (1 << 16),
		DIRTY_FLAG_PIPELINE = (1 << 17),
		DIRTY_FLAG_BLEND_CONSTANTS = (1 << 18),
		DIRTY_FLAG_STENCIL_REF = (1 << 19),

		DIRTY_BASE_STATE = DIRTY_FLAG_VS_CONSTANT_BUFFER_BINDING | DIRTY_FLAG_PS_CONSTANT_BUFFER_BINDING |
						   DIRTY_FLAG_VS_VERTEX_BUFFER_BINDING | DIRTY_FLAG_TEXTURES_DESCRIPTOR_TABLE |
						   DIRTY_FLAG_SAMPLERS_DESCRIPTOR_TABLE | DIRTY_FLAG_TEXTURES_DESCRIPTOR_TABLE_2 |
						   DIRTY_FLAG_VERTEX_BUFFER | DIRTY_FLAG_INDEX_BUFFER | DIRTY_FLAG_PRIMITIVE_TOPOLOGY |
						   DIRTY_FLAG_VIEWPORT | DIRTY_FLAG_SCISSOR | DIRTY_FLAG_RENDER_TARGET | DIRTY_FLAG_PIPELINE |
						   DIRTY_FLAG_BLEND_CONSTANTS | DIRTY_FLAG_STENCIL_REF,

		DIRTY_TFX_STATE =
			DIRTY_BASE_STATE | DIRTY_FLAG_TFX_TEXTURES | DIRTY_FLAG_TFX_SAMPLERS | DIRTY_FLAG_TFX_RT_TEXTURES,
		DIRTY_UTILITY_STATE = DIRTY_BASE_STATE,
		DIRTY_CONSTANT_BUFFER_STATE = DIRTY_FLAG_VS_CONSTANT_BUFFER | DIRTY_FLAG_PS_CONSTANT_BUFFER,
	};

	enum class RootSignature
	{
		Undefined,
		TFX,
		Utility
	};

	void InitializeState();
	void InitializeSamplers();

	void ApplyBaseState(u32 flags, ID3D12GraphicsCommandList* cmdlist);

	// Which bindings/state has to be updated before the next draw.
	u32 m_dirty_flags = 0;

	// input assembly
	D3D12_VERTEX_BUFFER_VIEW m_vertex_buffer = {};
	D3D12_INDEX_BUFFER_VIEW m_index_buffer = {};
	D3D12_PRIMITIVE_TOPOLOGY m_primitive_topology = {};

	GSTexture12* m_current_render_target = nullptr;
	GSTexture12* m_current_depth_target = nullptr;

	D3D12_VIEWPORT m_viewport = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
	GSVector4i m_scissor = GSVector4i::zero();
	u8 m_blend_constant_color = 0;
	u8 m_stencil_ref = 0;
	bool m_in_render_pass = false;

	std::array<D3D12_GPU_VIRTUAL_ADDRESS, NUM_TFX_CONSTANT_BUFFERS> m_tfx_constant_buffers{};
	std::array<D3D12DescriptorHandle, NUM_TOTAL_TFX_TEXTURES> m_tfx_textures{};
	D3D12DescriptorHandle m_tfx_sampler;
	u32 m_tfx_sampler_sel = 0;
	D3D12DescriptorHandle m_tfx_textures_handle_gpu;
	D3D12DescriptorHandle m_tfx_samplers_handle_gpu;
	D3D12DescriptorHandle m_tfx_rt_textures_handle_gpu;

	D3D12DescriptorHandle m_utility_texture_cpu;
	D3D12DescriptorHandle m_utility_texture_gpu;
	D3D12DescriptorHandle m_utility_sampler_cpu;
	D3D12DescriptorHandle m_utility_sampler_gpu;

	RootSignature m_current_root_signature = RootSignature::Undefined;
	const ID3D12PipelineState* m_current_pipeline = nullptr;

	std::unique_ptr<GSTexture12> m_null_texture;

	// current pipeline selector - we save this in the struct to avoid re-zeroing it every draw
	PipelineSelector m_pipeline_selector = {};
};
