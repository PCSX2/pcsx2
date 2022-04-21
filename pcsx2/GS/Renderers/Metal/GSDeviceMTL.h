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

#ifndef __OBJC__
	#error "This header is for use with Objective-C++ only.
#endif

#ifdef __APPLE__

#include "common/HashCombine.h"
#include "common/MRCHelpers.h"
#include "GS/GS.h"
#include "GSMTLDeviceInfo.h"
#include "GSMTLSharedHeader.h"
#include <AppKit/AppKit.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>
#include <atomic>
#include <memory>
#include <unordered_map>

struct PipelineSelectorExtrasMTL
{
	union
	{
		struct
		{
			GSTexture::Format rt : 4;
			u8 writemask : 4;
			GSDevice::BlendFactor src_factor : 4;
			GSDevice::BlendFactor dst_factor : 4;
			GSDevice::BlendOp     blend_op : 2;
			bool blend_enable : 1;
			bool has_depth : 1;
			bool has_stencil : 1;
		};
		u8 _key[3];
	};
	u32 fullkey() { return _key[0] | (_key[1] << 8) | (_key[2] << 16); }

	PipelineSelectorExtrasMTL(): _key{} {}
	PipelineSelectorExtrasMTL(GSHWDrawConfig::BlendState blend, GSTexture* rt, GSHWDrawConfig::ColorMaskSelector cms, bool has_depth, bool has_stencil)
		: _key{}
	{
		this->rt = rt ? rt->GetFormat() : GSTexture::Format::Invalid;
		MTLColorWriteMask mask = MTLColorWriteMaskNone;
		if (cms.wr) mask |= MTLColorWriteMaskRed;
		if (cms.wg) mask |= MTLColorWriteMaskGreen;
		if (cms.wb) mask |= MTLColorWriteMaskBlue;
		if (cms.wa) mask |= MTLColorWriteMaskAlpha;
		this->writemask = mask;
		this->src_factor = static_cast<GSDevice::BlendFactor>(blend.src_factor);
		this->dst_factor = static_cast<GSDevice::BlendFactor>(blend.dst_factor);
		this->blend_op = static_cast<GSDevice::BlendOp>(blend.op);
		this->blend_enable = blend.enable;
		this->has_depth   = has_depth;
		this->has_stencil = has_stencil;
	}
};
struct PipelineSelectorMTL
{
	GSHWDrawConfig::PSSelector ps;
	PipelineSelectorExtrasMTL extras;
	GSHWDrawConfig::VSSelector vs;
	PipelineSelectorMTL()
	{
		memset(this, 0, sizeof(*this));
	}
	PipelineSelectorMTL(GSHWDrawConfig::VSSelector vs, GSHWDrawConfig::PSSelector ps, PipelineSelectorExtrasMTL extras)
	{
		memset(this, 0, sizeof(*this));
		this->vs = vs;
		this->ps = ps;
		this->extras = extras;
	}
	PipelineSelectorMTL(const PipelineSelectorMTL& other)
	{
		memcpy(this, &other, sizeof(other));
	}
	PipelineSelectorMTL& operator=(const PipelineSelectorMTL& other)
	{
		memcpy(this, &other, sizeof(other));
		return *this;
	}
	bool operator==(const PipelineSelectorMTL& other) const
	{
		return BitEqual(*this, other);
	}
};

static_assert(sizeof(PipelineSelectorMTL) == 16);

template <>
struct std::hash<PipelineSelectorMTL>
{
	size_t operator()(const PipelineSelectorMTL& sel) const
	{
		size_t h = 0;
		size_t pieces[(sizeof(PipelineSelectorMTL) + sizeof(size_t) - 1) / sizeof(size_t)] = {};
		memcpy(pieces, &sel, sizeof(PipelineSelectorMTL));
		for (auto& piece : pieces)
			HashCombine(h, piece);
		return h;
	}
};

class GSScopedDebugGroupMTL
{
	id<MTLCommandBuffer> m_buffer;
public:
	GSScopedDebugGroupMTL(id<MTLCommandBuffer> buffer, NSString* name): m_buffer(buffer)
	{
		[m_buffer pushDebugGroup:name];
	}
	~GSScopedDebugGroupMTL()
	{
		[m_buffer popDebugGroup];
	}
};

struct ImDrawData;
class GSTextureMTL;

class GSDeviceMTL final : public GSDevice
{
public:
	using DepthStencilSelector = GSHWDrawConfig::DepthStencilSelector;
	using SamplerSelector = GSHWDrawConfig::SamplerSelector;
	enum class LoadAction
	{
		DontCare,
		DontCareIfFull,
		Load,
	};
	class UsageTracker
	{
		struct UsageEntry
		{
			u64 drawno;
			size_t pos;
		};
		std::vector<UsageEntry> m_usage;
		size_t m_size = 0;
		size_t m_pos = 0;
	public:
		size_t Size() { return m_size; }
		size_t Pos() { return m_pos; }
		bool PrepareForAllocation(u64 last_draw, size_t amt);
		size_t Allocate(u64 current_draw, size_t amt);
		void Reset(size_t new_size);
	};
	struct Map
	{
		id<MTLBuffer> gpu_buffer;
		size_t gpu_offset;
		void* cpu_buffer;
	};
	struct UploadBuffer
	{
		UsageTracker usage;
		MRCOwned<id<MTLBuffer>> mtlbuffer;
		void* buffer = nullptr;
	};
	struct BufferPair
	{
		UsageTracker usage;
		MRCOwned<id<MTLBuffer>> cpubuffer;
		MRCOwned<id<MTLBuffer>> gpubuffer;
		void* buffer = nullptr;
		size_t last_upload = 0;
	};

	struct ConvertShaderVertex
	{
		simd_float2 pos;
		simd_float2 texpos;
	};

	struct VSSelector
	{
		union
		{
			struct
			{
				bool iip        : 1;
				bool fst        : 1;
				bool point_size : 1;
			};
			u8 key;
		};
		VSSelector(): key(0) {}
		VSSelector(u8 key): key(key) {}
	};

	using PSSelector = GSHWDrawConfig::PSSelector;

	// MARK: Configuration
	int m_mipmap;

	// MARK: Permanent resources
	std::shared_ptr<std::pair<std::mutex, GSDeviceMTL*>> m_backref;
	GSMTLDevice m_dev;
	MRCOwned<id<MTLCommandQueue>> m_queue;
	MRCOwned<id<MTLFence>> m_draw_sync_fence;
	MRCOwned<MTLFunctionConstantValues*> m_fn_constants;
	MRCOwned<MTLVertexDescriptor*> m_hw_vertex;
	std::unique_ptr<GSTextureMTL> m_font;

	// Draw IDs are used to make sure we're not clobbering things
	u64 m_current_draw = 1;
	std::atomic<u64> m_last_finished_draw{0};

	// Functions and Pipeline States
	MRCOwned<id<MTLRenderPipelineState>> m_convert_pipeline[static_cast<int>(ShaderConvert::Count)];
	MRCOwned<id<MTLRenderPipelineState>> m_present_pipeline[static_cast<int>(ShaderConvert::Count)];
	MRCOwned<id<MTLRenderPipelineState>> m_convert_pipeline_copy[2];
	MRCOwned<id<MTLRenderPipelineState>> m_convert_pipeline_copy_mask[1 << 4];
	MRCOwned<id<MTLRenderPipelineState>> m_merge_pipeline[4];
	MRCOwned<id<MTLRenderPipelineState>> m_interlace_pipeline[4];
	MRCOwned<id<MTLRenderPipelineState>> m_datm_pipeline[2];
	MRCOwned<id<MTLRenderPipelineState>> m_stencil_clear_pipeline;
	MRCOwned<id<MTLRenderPipelineState>> m_primid_init_pipeline[2][2];
	MRCOwned<id<MTLRenderPipelineState>> m_hdr_init_pipeline;
	MRCOwned<id<MTLRenderPipelineState>> m_hdr_resolve_pipeline;
	MRCOwned<id<MTLRenderPipelineState>> m_fxaa_pipeline;
	MRCOwned<id<MTLRenderPipelineState>> m_shadeboost_pipeline;
	MRCOwned<id<MTLRenderPipelineState>> m_imgui_pipeline;
	MRCOwned<id<MTLRenderPipelineState>> m_imgui_pipeline_a8;

	MRCOwned<id<MTLFunction>> m_hw_vs[1 << 3];
	std::unordered_map<PSSelector, MRCOwned<id<MTLFunction>>> m_hw_ps;
	std::unordered_map<PipelineSelectorMTL, MRCOwned<id<MTLRenderPipelineState>>> m_hw_pipeline;

	MRCOwned<MTLRenderPassDescriptor*> m_render_pass_desc[8];

	MRCOwned<id<MTLSamplerState>> m_sampler_hw[1 << 8];

	MRCOwned<id<MTLDepthStencilState>> m_dss_stencil_zero;
	MRCOwned<id<MTLDepthStencilState>> m_dss_stencil_write;
	MRCOwned<id<MTLDepthStencilState>> m_dss_hw[1 << 5];

	MRCOwned<id<MTLBuffer>> m_texture_download_buf;
	UploadBuffer m_texture_upload_buf;
	BufferPair m_vertex_upload_buf;

	// MARK: Ephemeral resources
	MRCOwned<id<MTLCommandBuffer>> m_current_render_cmdbuf;
	struct MainRenderEncoder
	{
		MRCOwned<id<MTLRenderCommandEncoder>> encoder;
		GSTexture* color_target = nullptr;
		GSTexture* depth_target = nullptr;
		GSTexture* stencil_target = nullptr;
		GSTexture* tex[8] = {};
		void* vertex_buffer = nullptr;
		void* name = nullptr;
		struct Has
		{
			bool cb_vs        : 1;
			bool cb_ps        : 1;
			bool scissor      : 1;
			bool blend_color  : 1;
			bool pipeline_sel : 1;
			bool sampler      : 1;
		} has;
		DepthStencilSelector depth_sel = DepthStencilSelector::NoDepth();
		// Clear line (Things below here are tracked by `has` and don't need to be cleared to reset)
		SamplerSelector sampler_sel;
		u8 blend_color;
		GSVector4i scissor;
		PipelineSelectorMTL pipeline_sel;
		GSHWDrawConfig::VSConstantBuffer cb_vs;
		GSHWDrawConfig::PSConstantBuffer cb_ps;
		MainRenderEncoder(const MainRenderEncoder&) = delete;
		MainRenderEncoder() = default;
	} m_current_render;
	MRCOwned<id<MTLCommandBuffer>> m_texture_upload_cmdbuf;
	MRCOwned<id<MTLBlitCommandEncoder>> m_texture_upload_encoder;
	MRCOwned<id<MTLBlitCommandEncoder>> m_late_texture_upload_encoder;
	MRCOwned<id<MTLCommandBuffer>> m_vertex_upload_cmdbuf;
	MRCOwned<id<MTLBlitCommandEncoder>> m_vertex_upload_encoder;

	struct DebugEntry
	{
		enum Op { Push, Insert, Pop } op;
		MRCOwned<NSString*> str;
		DebugEntry(Op op, MRCOwned<NSString*> str): op(op), str(std::move(str)) {}
	};

	std::vector<DebugEntry> m_debug_entries;
	u32 m_debug_group_level = 0;

	GSDeviceMTL();
	~GSDeviceMTL() override;

	/// Allocate space in the given buffer
	Map Allocate(UploadBuffer& buffer, size_t amt);
	/// Allocate space in the given buffer for use with the given render command encoder
	Map Allocate(BufferPair& buffer, size_t amt);
	/// Enqueue upload of any outstanding data
	void Sync(BufferPair& buffer);
	/// Get the texture upload encoder, creating a new one if it doesn't exist
	id<MTLBlitCommandEncoder> GetTextureUploadEncoder();
	/// Get the late texture upload encoder, creating a new one if it doesn't exist
	id<MTLBlitCommandEncoder> GetLateTextureUploadEncoder();
	/// Get the vertex upload encoder, creating a new one if it doesn't exist
	id<MTLBlitCommandEncoder> GetVertexUploadEncoder();
	/// Get the render command buffer, creating a new one if it doesn't exist
	id<MTLCommandBuffer> GetRenderCmdBuf();
	/// Flush pending operations from all encoders to the GPU
	void FlushEncoders();
	/// End current render pass without flushing
	void EndRenderPass();
	/// Begin a new render pass (may reuse existing)
	void BeginRenderPass(NSString* name, GSTexture* color, MTLLoadAction color_load, GSTexture* depth, MTLLoadAction depth_load, GSTexture* stencil = nullptr, MTLLoadAction stencil_load = MTLLoadActionDontCare);

	GSTexture* CreateSurface(GSTexture::Type type, int width, int height, int levels, GSTexture::Format format) override;

	void DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect, const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, const GSVector4& c) override;
	void DoInterlace(GSTexture* sTex, GSTexture* dTex, int shader, bool linear, float yoffset) override;
	void DoFXAA(GSTexture* sTex, GSTexture* dTex) override;
	void DoShadeBoost(GSTexture* sTex, GSTexture* dTex, const float params[4]) override;
	void DoExternalFX(GSTexture* sTex, GSTexture* dTex) override;

	MRCOwned<id<MTLFunction>> LoadShader(NSString* name);
	MRCOwned<id<MTLRenderPipelineState>> MakePipeline(MTLRenderPipelineDescriptor* desc, id<MTLFunction> vertex, id<MTLFunction> fragment, NSString* name);
	bool Create(HostDisplay* display) override;

	void ClearRenderTarget(GSTexture* t, const GSVector4& c) override;
	void ClearRenderTarget(GSTexture* t, u32 c) override;
	void ClearDepth(GSTexture* t) override;
	void ClearStencil(GSTexture* t, u8 c) override;

	bool DownloadTexture(GSTexture* src, const GSVector4i& rect, GSTexture::GSMap& out_map) override;

	void CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r, u32 destX, u32 destY) override;
	void DoStretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, id<MTLRenderPipelineState> pipeline, bool linear, LoadAction load_action, void* frag_uniform, size_t frag_uniform_len);
	void DrawStretchRect(const GSVector4& sRect, const GSVector4& dRect, const GSVector2i& ds);
	/// Copy from a position in sTex to the same position in the currently active render encoder using the given fs pipeline and rect
	void RenderCopy(GSTexture* sTex, id<MTLRenderPipelineState> pipeline, const GSVector4i& rect);
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ShaderConvert shader = ShaderConvert::COPY, bool linear = true) override;
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, bool red, bool green, bool blue, bool alpha) override;

	void FlushClears(GSTexture* tex);

	// MARK: Main Render Encoder operations
	void MRESetHWPipelineState(GSHWDrawConfig::VSSelector vs, GSHWDrawConfig::PSSelector ps, GSHWDrawConfig::BlendState blend, GSHWDrawConfig::ColorMaskSelector cms);
	void MRESetDSS(DepthStencilSelector sel);
	void MRESetDSS(id<MTLDepthStencilState> dss);
	void MRESetSampler(SamplerSelector sel);
	void MRESetTexture(GSTexture* tex, int pos);
	void MRESetVertices(id<MTLBuffer> buffer, size_t offset);
	void MRESetScissor(const GSVector4i& scissor);
	void MREClearScissor();
	void MRESetCB(const GSHWDrawConfig::VSConstantBuffer& cb_vs);
	void MRESetCB(const GSHWDrawConfig::PSConstantBuffer& cb_ps);
	void MRESetBlendColor(u8 blend_color);
	void MRESetPipeline(id<MTLRenderPipelineState> pipe);
	void MREInitHWDraw(GSHWDrawConfig& config, const Map& verts);

	// MARK: Render HW

	void SetupDestinationAlpha(GSTexture* rt, GSTexture* ds, const GSVector4i& r, bool datm);
	void RenderHW(GSHWDrawConfig& config) override;
	void SendHWDraw(GSHWDrawConfig& config, id<MTLRenderCommandEncoder> enc, id<MTLBuffer> buffer, size_t off);

	// MARK: Debug

	void PushDebugGroup(const char* fmt, ...) override;
	void PopDebugGroup() override;
	void InsertDebugMessage(DebugMessageCategory category, const char* fmt, ...) override;
	void ProcessDebugEntry(id<MTLCommandEncoder> enc, const DebugEntry& entry);
	void FlushDebugEntries(id<MTLCommandEncoder> enc);
	void EndDebugGroup(id<MTLCommandEncoder> enc);

	// MARK: ImGui

	void RenderImGui(ImDrawData* data);
	u32 FrameNo() const { return m_frame; }
};

#endif // __APPLE__
