// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "GS/Renderers/Common/GSDevice.h"

#ifndef __OBJC__
	#error "This header is for use with Objective-C++ only.
#endif

#ifdef __APPLE__

#include "common/HashCombine.h"
#include "common/MRCHelpers.h"
#include "common/ReadbackSpinManager.h"
#include "GS/GS.h"
#include "GSMTLDeviceInfo.h"
#include "GSMTLSharedHeader.h"
#include <AppKit/AppKit.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>

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
			GSDevice::BlendFactor src_factor_alpha : 4;
			GSDevice::BlendFactor dst_factor_alpha : 4;
			GSDevice::BlendOp     blend_op : 2;
			bool blend_enable : 1;
			bool has_depth    : 1;
			bool has_stencil  : 1;
			bool has_rt1      : 1;
		};
		u32 fullkey;
	};

	PipelineSelectorExtrasMTL(): fullkey(0) {}
	PipelineSelectorExtrasMTL(GSHWDrawConfig::BlendState blend, GSTexture* rt, GSHWDrawConfig::ColorMaskSelector cms, bool has_depth, bool has_stencil, bool has_rt1)
		: fullkey(0)
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
		this->src_factor_alpha = static_cast<GSDevice::BlendFactor>(blend.src_factor_alpha);
		this->dst_factor_alpha = static_cast<GSDevice::BlendFactor>(blend.dst_factor_alpha);
		this->blend_enable = blend.enable;
		this->has_depth   = has_depth;
		this->has_stencil = has_stencil;
		this->has_rt1     = has_rt1;
	}
};
struct PipelineSelectorMTL
{
	GSHWDrawConfig::PSSelector ps;
	PipelineSelectorExtrasMTL extras;
	GSHWDrawConfig::VSSelector vs;
	u8 pad[3];
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

static_assert(sizeof(PipelineSelectorMTL) == 24);

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

struct ImDrawData;
class GSTextureMTL;
class GSDeviceMTL;

// Metal 4 objects are only ever created after checking @available(macOS 26.0, *) and device support (see
// GSDeviceMTL::Create), so code that only runs when they exist doesn't need its own availability checks.
#define GSMTL4_BEGIN \
	_Pragma("clang diagnostic push") \
	_Pragma("clang diagnostic ignored \"-Wunguarded-availability\"") \
	_Pragma("clang diagnostic ignored \"-Wunguarded-availability-new\"")
#define GSMTL4_END _Pragma("clang diagnostic pop")

GSMTL4_BEGIN

/// Render command encoder that is either a Metal 3 MTLRenderCommandEncoder or a Metal 4 MTL4RenderCommandEncoder.
/// On Metal 4, resources are bound through the device's argument tables and inline bytes go through the upload buffer.
class GSMTLRenderEncoder
{
	friend class GSDeviceMTL;
	GSDeviceMTL* m_dev = nullptr;
	MRCOwned<id<MTLRenderCommandEncoder>> m_enc;
	MRCOwned<id<MTL4RenderCommandEncoder>> m_enc4;

public:
	explicit operator bool() const { return m_enc || m_enc4; }
	bool operator!() const { return !m_enc && !m_enc4; }

	void SetLabel(NSString* label);
	void PushDebugGroup(NSString* name);
	void PopDebugGroup();
	void InsertDebugSignpost(NSString* name);
	void SetPipeline(id<MTLRenderPipelineState> pipe);
	void SetDepthStencilState(id<MTLDepthStencilState> dss);
	void SetStencilReferenceValue(u32 value);
	void SetScissor(const MTLScissorRect& rect);
	void SetBlendColor(float color);
	void SetVertexBytes(const void* data, size_t length, u32 index);
	void SetFragmentBytes(const void* data, size_t length, u32 index);
	void SetVertexBuffer(id<MTLBuffer> buffer, size_t offset, u32 index);
	/// Change the offset of an already bound vertex buffer (Metal 4 needs the buffer again to compute the address)
	void SetVertexBufferOffset(id<MTLBuffer> buffer, size_t offset, u32 index);
	void SetFragmentTexture(id<MTLTexture> texture, u32 index);
	void SetFragmentSampler(id<MTLSamplerState> sampler, u32 index);
	void Draw(MTLPrimitiveType type, size_t start, size_t count);
	void DrawIndexed(MTLPrimitiveType type, size_t count, MTLIndexType index_type, id<MTLBuffer> buffer, size_t offset, size_t base_vertex = 0);
	/// Barrier between fragment work of previous draws and subsequent draws (for reading the render target as a texture)
	void TextureBarrier();
	void WaitForFence(id<MTLFence> fence, MTLRenderStages stages);
	void UpdateFence(id<MTLFence> fence, MTLRenderStages stages);
	void End();
};

/// Blit encoder, which is a Metal 3 MTLBlitCommandEncoder or a Metal 4 MTL4ComputeCommandEncoder (used for copies only).
class GSMTLBlitEncoder
{
	friend class GSDeviceMTL;
	MRCOwned<id<MTLBlitCommandEncoder>> m_enc;
	MRCOwned<id<MTL4ComputeCommandEncoder>> m_enc4;
	u64 m_serial = 0;

public:
	explicit operator bool() const { return m_enc || m_enc4; }
	bool operator!() const { return !m_enc && !m_enc4; }
	/// Unique ID of this encoder, used to detect multiple writes to the same texture within one Metal 4 encoder
	u64 Serial() const { return m_serial; }

	void SetLabel(NSString* label);
	void CopyBufferToTexture(id<MTLBuffer> buffer, size_t offset, size_t bytes_per_row, size_t bytes_per_image, MTLSize size, id<MTLTexture> texture, u32 level, MTLOrigin origin);
	void CopyTextureToBuffer(id<MTLTexture> texture, u32 level, MTLOrigin origin, MTLSize size, id<MTLBuffer> buffer, size_t offset, size_t bytes_per_row, size_t bytes_per_image);
	void CopyTexture(id<MTLTexture> src, MTLOrigin src_origin, MTLSize size, id<MTLTexture> dst, MTLOrigin dst_origin);
	void GenerateMipmaps(id<MTLTexture> texture);
	/// Metal 4 has no hazard tracking within an encoder, this orders copies that touch the same resource
	void Barrier();
	void WaitForFence(id<MTLFence> fence);
	void UpdateFence(id<MTLFence> fence);
	void End();
};

GSMTL4_END

class GSDeviceMTL final : public GSDevice
{
public:
	using DepthStencilSelector = GSHWDrawConfig::DepthStencilSelector;
	using SamplerSelector = GSHWDrawConfig::SamplerSelector;
	enum class UsePresentDrawable : u8
	{
		Never = 0,
		Always = 1,
		IfVsync = 2,
	};
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
				GSShader::VSExpand expand : 3;
			};
			u8 key;
		};
		VSSelector(): key(0) {}
		VSSelector(u8 key): key(key) {}
	};

	using PSSelector = GSHWDrawConfig::PSSelector;

	// MARK: Permanent resources
	std::shared_ptr<std::pair<std::mutex, GSDeviceMTL*>> m_backref;
	GSMTLDevice m_dev;
	MRCOwned<id<MTLCommandQueue>> m_queue;
	MRCOwned<id<MTLFence>> m_draw_sync_fence;

	/// Whether the Metal 4 command queue is in use (all MTL4 objects below are nil otherwise)
	bool m_use_mtl4 = false;
GSMTL4_BEGIN
	struct MTL4State
	{
		struct Allocator
		{
			u64 draw; ///< Last draw that used this allocator, reset once it completes
			MRCOwned<id<MTL4CommandAllocator>> allocator;
		};
		struct DeferredRelease
		{
			u64 draw; ///< Release once this draw completes
			MRCOwned<id> object;
			bool resident;
		};
		MRCOwned<id<MTL4CommandQueue>> queue;
		MRCOwned<id<MTLSharedEvent>> event; ///< Signaled with the draw number after each submission
		MRCOwned<id<MTLResidencySet>> residency;
		MRCOwned<id<MTLResidencySet>> layer_residency; ///< CAMetalLayer drawables, attached to the queue
		MRCOwned<id<MTL4ArgumentTable>> vertex_table;
		MRCOwned<id<MTL4ArgumentTable>> fragment_table;
		MRCOwned<id<MTL4ArgumentTable>> compute_table;
		MRCOwned<MTL4RenderPassDescriptor*> pass_desc;
		MRCOwned<id<MTL4CommandBuffer>> render_cmdbuf;
		MRCOwned<id<MTL4CommandBuffer>> upload_cmdbuf;
		std::vector<Allocator> allocators_in_use;
		std::vector<MRCOwned<id<MTL4CommandAllocator>>> free_allocators;
		std::deque<DeferredRelease> deferred_releases;
		bool residency_dirty = false;
	} m_mtl4;
GSMTL4_END
	/// Blit encoders get a unique serial so Metal 4 texture uploads know when they need an intra-encoder barrier
	u64 m_blit_encoder_serial = 0;
	MRCOwned<MTLFunctionConstantValues*> m_fn_constants;
	MRCOwned<MTLVertexDescriptor*> m_hw_vertex;
	MTLResourceOptions m_resource_options_shared_wc;

	// Previously in MetalHostDisplay.
	MRCOwned<NSView*> m_view;
	MRCOwned<CAMetalLayer*> m_layer;
	MRCOwned<id<CAMetalDrawable>> m_current_drawable;
	MRCOwned<MTLRenderPassDescriptor*> m_pass_desc;
	u32 m_capture_start_frame;
	UsePresentDrawable m_use_present_drawable;
	bool m_gpu_timing_enabled = false;
	double m_accumulated_gpu_time = 0;
	double m_last_gpu_time_end = 0;
	std::mutex m_mtx;

	// Draw IDs are used to make sure we're not clobbering things
	u64 m_current_draw = 1;
	std::atomic<u64> m_last_finished_draw{0};

	// Spinning
	ReadbackSpinManager m_spin_manager;
	u32 m_encoders_in_current_cmdbuf;
	u32 m_spin_timer = 0;
	MRCOwned<id<MTLComputePipelineState>> m_spin_pipeline;
	MRCOwned<id<MTLBuffer>> m_spin_buffer;
	MRCOwned<id<MTLFence>> m_spin_fence;

	// Functions and Pipeline States
	MRCOwned<id<MTLRenderPipelineState>> m_cas_pipeline[2];
	std::vector<MRCOwned<id<MTLRenderPipelineState>>> m_convert_pipeline;
	MRCOwned<id<MTLRenderPipelineState>> m_present_pipeline[static_cast<int>(PresentShader::Count)];
	MRCOwned<id<MTLRenderPipelineState>> m_merge_pipeline[4];
	MRCOwned<id<MTLRenderPipelineState>> m_interlace_pipeline[NUM_INTERLACE_SHADERS];
	MRCOwned<id<MTLRenderPipelineState>> m_datm_pipeline[4];
	MRCOwned<id<MTLRenderPipelineState>> m_clut_pipeline[2];
	MRCOwned<id<MTLRenderPipelineState>> m_stencil_clear_pipeline;
	MRCOwned<id<MTLRenderPipelineState>> m_primid_init_pipeline[2][4];
	MRCOwned<id<MTLRenderPipelineState>> m_colclip_init_pipeline;
	MRCOwned<id<MTLRenderPipelineState>> m_colclip_clear_pipeline;
	MRCOwned<id<MTLRenderPipelineState>> m_colclip_resolve_pipeline;
	MRCOwned<id<MTLRenderPipelineState>> m_fxaa_pipeline;
	MRCOwned<id<MTLRenderPipelineState>> m_shadeboost_pipeline;
	MRCOwned<id<MTLRenderPipelineState>> m_imgui_pipeline;

	id<MTLRenderPipelineState> GetConvertPipeline(ShaderConvertSelector shader) const
	{
		return m_convert_pipeline[shader.Index()];
	}

	id<MTLRenderPipelineState> GetConvertPipeline(ShaderConvert shader) const
	{
		return m_convert_pipeline[ShaderConvertSelector(shader).Index()];
	}

	MRCOwned<id<MTLFunction>> m_hw_vs[6 << 3];
	std::unordered_map<PSSelector, MRCOwned<id<MTLFunction>>> m_hw_ps;
	std::unordered_map<PipelineSelectorMTL, MRCOwned<id<MTLRenderPipelineState>>> m_hw_pipeline;

	MRCOwned<MTLRenderPassDescriptor*> m_render_pass_desc[16];
	MRCOwned<MTLRenderPassDescriptor*> m_full_rov_render_pass_desc;

	MRCOwned<id<MTLSamplerState>> m_sampler_hw[1 << 8];

	MRCOwned<id<MTLDepthStencilState>> m_dss_stencil_zero;
	MRCOwned<id<MTLDepthStencilState>> m_dss_stencil_write;
	MRCOwned<id<MTLDepthStencilState>> m_dss_hw[1 << 5];

	MRCOwned<id<MTLBuffer>> m_expand_index_buffer;
	UploadBuffer m_texture_upload_buf;
	BufferPair m_vertex_upload_buf;

	// MARK: Ephemeral resources
	MRCOwned<id<MTLCommandBuffer>> m_current_render_cmdbuf;
	struct MainRenderEncoder
	{
		GSMTLRenderEncoder encoder;
		GSTexture* color_target = nullptr;
		GSTexture* depth_target = nullptr;
		GSTexture* stencil_target = nullptr;
		GSTexture* tex[GSMTLTextureIndexCount] = {};
		id<MTLBuffer> vertex_buffer = nullptr;
		id<MTLBuffer> vs_index_buffer = nullptr;
		void* name = nullptr;
		struct Has
		{
			bool cb_vs        : 1;
			bool cb_ps        : 1;
			bool scissor      : 1;
			bool blend_color  : 1;
			bool pipeline_sel : 1;
			bool sampler      : 1;
			bool rt1_depth    : 1;
		} has = {};
		DepthStencilSelector depth_sel = DepthStencilSelector::NoDepth();
		// Clear line (Things below here are tracked by `has` and don't need to be cleared to reset)
		SamplerSelector sampler_sel;
		u8 blend_color;
		struct { u32 w, h; } full_rov_size;
		GSVector4i scissor;
		PipelineSelectorMTL pipeline_sel;
		GSHWDrawConfig::VSConstantBuffer cb_vs;
		GSHWDrawConfig::PSConstantBuffer cb_ps;
		MainRenderEncoder(const MainRenderEncoder&) = delete;
		MainRenderEncoder() = default;
		bool is_full_rov() const { return encoder && !color_target && !depth_target && !stencil_target; }
	} m_current_render;
	MRCOwned<id<MTLCommandBuffer>> m_texture_upload_cmdbuf;
	GSMTLBlitEncoder m_texture_upload_encoder;
	GSMTLBlitEncoder m_late_texture_upload_encoder;
	MRCOwned<id<MTLCommandBuffer>> m_vertex_upload_cmdbuf;
	MRCOwned<id<MTLBlitCommandEncoder>> m_vertex_upload_encoder;
	id<MTLTexture> m_ds_as_rt_texture = nil;
	GSTexture* m_ds_as_rt_gstexture = nullptr;
	MRCOwned<id<MTLTexture>> m_rov_dummy_texture;
	struct { u32 w, h; } m_rov_dummy_texture_size = {};

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
	GSMTLBlitEncoder& GetTextureUploadEncoder();
	/// Get the late texture upload encoder, creating a new one if it doesn't exist
	GSMTLBlitEncoder& GetLateTextureUploadEncoder();
	/// Get the vertex upload encoder, creating a new one if it doesn't exist
	id<MTLBlitCommandEncoder> GetVertexUploadEncoder();
	/// Get the render command buffer, creating a new one if it doesn't exist (Metal 3 only)
	id<MTLCommandBuffer> GetRenderCmdBuf();
	/// Get the render command buffer, will not create a new one if it doesn't exist (Metal 3 only)
	id<MTLCommandBuffer> GetRenderCmdBufWithoutCreate();
	/// Make sure there's a render command buffer to encode into
	void EnsureRenderCmdBuf();
	/// Create a new render encoder on the render command buffer
	GSMTLRenderEncoder CreateRenderEncoder(MTLRenderPassDescriptor* desc);
	/// Create a new blit encoder on the render command buffer
	GSMTLBlitEncoder CreateBlitEncoder();
	/// Push a debug group on the render command buffer
	void PushCmdBufDebugGroup(NSString* name);
	/// Pop a debug group from the render command buffer
	void PopCmdBufDebugGroup();
	/// Get the spin fence if spinning is enabled.
	id<MTLFence> GetSpinFence();
	/// Get the texture to use as RT1 depth for the given depth texture
	id<MTLTexture> GetRT1DepthTexture(GSTextureMTL* depth);
	/// Called by command buffers when they finish
	void DrawCommandBufferFinished(u64 draw, double gpu_begin, double gpu_end);

	// MARK: Metal 4 helpers
	/// Check whether Metal 4 should be used for the given device
	static bool ShouldUseMetal4(const GSMTLDevice& dev, const char** reason);
	/// Create the Metal 4 queue and associated objects
	bool CreateMetal4();
	/// Release the Metal 4 objects (the GPU must be idle)
	void DestroyMetal4();
GSMTL4_BEGIN
	/// Get an MTL4CommandAllocator that isn't being used by the GPU, the returned allocator is marked as in use by the current draw
	id<MTL4CommandAllocator> GetMetal4Allocator();
	/// Create and begin a new Metal 4 command buffer
	MRCOwned<id<MTL4CommandBuffer>> NewMetal4CommandBuffer(NSString* label);
	/// Commit Metal 4 command buffers and signal the current draw on completion
	void CommitMetal4(id<MTL4CommandBuffer> const* cmdbufs, u32 count, MTL4CommitOptions* options);
	/// Metal 4 version of FlushEncoders
	void FlushEncodersMetal4();
GSMTL4_END
	/// Reset allocators and release resources the GPU is done with
	void ReclaimMetal4Resources();
	/// Make a resource resident for Metal 4 command buffers (no-op on Metal 3)
	void MakeResident(id<MTLResource> resource);
	/// Keep an object alive until the GPU is done with the current draw, removing it from the residency set if requested (no-op on Metal 3)
	void DeferRelease(id object, bool resident);
	/// Wait for the given draw to complete on the GPU
	void WaitForDraw(u64 draw, bool spin);
	/// Flush and wait for all submitted GPU work to complete
	void WaitForGPUIdle();
	/// Get the GPU address of the given offset within a Metal 4 upload allocation
	static u64 GPUAddress(id<MTLBuffer> buffer, size_t offset);
	/// Flush pending operations from all encoders to the GPU
	void FlushEncoders();
	/// Flush pending operations and spins the GPU for a download.
	void FlushEncodersForReadback();
	/// End current render pass without flushing
	void EndRenderPass();
	/// Prepare to begin a new render pass
	void PrepareBeginRenderPass();
	/// Begin a new render pass (may reuse existing)
	void BeginRenderPass(NSString* name, GSTexture* color, MTLLoadAction color_load, GSTexture* depth, MTLLoadAction depth_load, GSTexture* stencil = nullptr, MTLLoadAction stencil_load = MTLLoadActionDontCare, bool rt1 = false);
	/// Begin a new full-ROV render pass (may reuse existing)
	void BeginFullROV(NSString* name, uint32_t width, uint32_t height);
	/// Call at the end of each frame
	void FrameCompleted();

	GSTexture* CreateSurface(GSTexture::Usage usage, int width, int height, int levels, GSTexture::Format format) override;

	void DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect, const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, u32 c, const Filter filter) override;
	void DoInterlace(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ShaderInterlace shader, Filter filter, const InterlaceConstantBuffer& cb) override;
	void DoFXAA(GSTexture* sTex, GSTexture* dTex) override;
	void DoShadeBoost(GSTexture* sTex, GSTexture* dTex, const float params[4]) override;

	bool DoCAS(GSTexture* sTex, GSTexture* dTex, bool sharpen_only, const std::array<u32, NUM_CAS_CONSTANTS>& constants) override;

	MRCOwned<id<MTLFunction>> LoadShader(NSString* name);
	MRCOwned<id<MTLRenderPipelineState>> MakePipeline(MTLRenderPipelineDescriptor* desc, id<MTLFunction> vertex, id<MTLFunction> fragment, NSString* name);
	MRCOwned<id<MTLComputePipelineState>> MakeComputePipeline(id<MTLFunction> compute, NSString* name);
	bool Create(GSVSyncMode vsync_mode, bool allow_present_throttle) override;
	void Destroy() override;

	void AttachSurfaceOnMainThread();
	void DetachSurfaceOnMainThread();

	RenderAPI GetRenderAPI() const override;
	bool HasSurface() const override;
	void DestroySurface() override;
	bool UpdateWindow() override;
	bool SupportsExclusiveFullscreen() const override;
	std::string GetDriverInfo() const override;

	void ResizeWindow(u32 new_window_width, u32 new_window_height, float new_window_scale) override;

	void UpdateTexture(id<MTLTexture> texture, u32 x, u32 y, u32 width, u32 height, const void* data, u32 data_stride);

	PresentResult BeginPresent(bool frame_skip) override;
	void EndPresent() override;
	void SetVSyncMode(GSVSyncMode mode, bool allow_present_throttle) override;

	bool SetGPUTimingEnabled(bool enabled) override;
	float GetAndResetAccumulatedGPUTime() override;
	void AccumulateCommandBufferTime(double begin, double end);

	bool SetGPUPipelineStatisticsEnabled(bool enabled) override { return false; }
	GPUPipelineStatistics GetAndResetAccumulatedGPUPipelineStatistics() override { return {}; }

	std::unique_ptr<GSDownloadTexture> CreateDownloadTexture(u32 width, u32 height, GSTexture::Format format) override;

	void ClearSamplerCache() override;

	void CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r, u32 destX, u32 destY) override;
	void BeginStretchRect(NSString* name, GSTexture* dTex, MTLLoadAction action);
	void DoStretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, id<MTLRenderPipelineState> pipeline, std::optional<Filter> filter, LoadAction load_action, const void* frag_uniform, size_t frag_uniform_len);
	void DrawStretchRect(const GSVector4& sRect, const GSVector4& dRect, const GSVector2& ds);
	/// Copy from a position in sTex to the same position in the currently active render encoder using the given fs pipeline and rect
	void RenderCopy(GSTexture* sTex, id<MTLRenderPipelineState> pipeline, const GSVector4i& rect);
	void PresentRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, PresentShader shader, float shaderTime, Filter filter) override;
	void DrawMultiStretchRects(const MultiStretchRect* rects, u32 num_rects, GSTexture* dTex, ShaderConvertSelector shader) override;
	void UpdateCLUTTexture(GSTexture* sTex, float sScale, u32 offsetX, u32 offsetY, GSTexture* dTex, u32 dOffset, u32 dSize) override;
	void ConvertToIndexedTexture(GSTexture* sTex, float sScale, u32 offsetX, u32 offsetY, u32 SBW, u32 SPSM, GSTexture* dTex, u32 DBW, u32 DPSM) override;
	void FilteredDownsampleTexture(GSTexture* sTex, GSTexture* dTex, u32 downsample_factor, const GSVector2i& clamp_min, const GSVector4& dRect) override;
	void BeginDSAsRT(GSTexture* ds, const GSVector4i& drawarea) override;

	void FlushClears(GSTexture* tex);

	// MARK: Main Render Encoder operations
	void MRESetHWPipelineState(GSHWDrawConfig::VSSelector vs, GSHWDrawConfig::PSSelector ps, GSHWDrawConfig::BlendState blend, GSHWDrawConfig::ColorMaskSelector cms);
	void MRESetDSS(DepthStencilSelector sel);
	void MRESetDSS(id<MTLDepthStencilState> dss);
	void MRESetSampler(SamplerSelector sel);
	void MRESetTexture(GSTexture* tex, int pos);
	void MRESetTexture(id<MTLTexture> tex, int pos);
	void MRESetVertices(id<MTLBuffer> buffer, size_t offset);
	void MRESetVSIndices(id<MTLBuffer> buffer, size_t offset);
	void MRESetScissor(const GSVector4i& scissor);
	void MREClearScissor();
	void MRESetCB(const GSHWDrawConfig::VSConstantBuffer& cb_vs);
	void MRESetCB(const GSHWDrawConfig::PSConstantBuffer& cb_ps);
	void MRESetBlendColor(u8 blend_color);
	void MRESetPipeline(id<MTLRenderPipelineState> pipe);
	void MREInitHWDraw(GSHWDrawConfig& config, const Map& verts);

	// MARK: Render HW

	void SetupDestinationAlpha(GSTexture* rt, GSTexture* ds, const GSVector4i& r, SetDATM datm);
	void PrepareROVTexture(GSTexture** ptex);
	void RenderHW(GSHWDrawConfig& config) override;
	void SendHWDraw(GSHWDrawConfig& config, GSMTLRenderEncoder& enc, id<MTLBuffer> buffer, size_t off,
		bool one_barrier, bool full_barrier);

	// MARK: Debug

	void PushDebugGroup(const char* fmt, ...) override;
	void PopDebugGroup() override;
	void InsertDebugMessage(DebugMessageCategory category, const char* fmt, ...) override;
	void ProcessDebugEntry(GSMTLRenderEncoder& enc, const DebugEntry& entry);
	void FlushDebugEntries(GSMTLRenderEncoder& enc);
	void EndDebugGroup(GSMTLRenderEncoder& enc);

	// MARK: ImGui

	void RenderImGui(ImDrawData* data);
	u32 FrameNo() const { return m_frame; }

protected:
	using GSDevice::DoStretchRect; // Suppress overloaded virtual function warning
	virtual void DoStretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect,
		ShaderConvertSelector shader, Filter filter) override;
};

class GSScopedDebugGroupMTL
{
	GSDeviceMTL* m_dev;
public:
	GSScopedDebugGroupMTL(GSDeviceMTL* dev, NSString* name): m_dev(dev)
	{
		m_dev->PushCmdBufDebugGroup(name);
	}
	~GSScopedDebugGroupMTL()
	{
		m_dev->PopCmdBufDebugGroup();
	}
};

static constexpr bool IsCommandBufferCompleted(MTLCommandBufferStatus status)
{
	switch (status)
	{
		case MTLCommandBufferStatusNotEnqueued:
		case MTLCommandBufferStatusEnqueued:
		case MTLCommandBufferStatusCommitted:
		case MTLCommandBufferStatusScheduled:
			return false;
		case MTLCommandBufferStatusCompleted:
		case MTLCommandBufferStatusError:
			return true;
	}
}

#endif // __APPLE__
