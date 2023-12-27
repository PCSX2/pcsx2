// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Host.h"
#include "GS/Renderers/Metal/GSMetalCPPAccessible.h"
#include "GS/Renderers/Metal/GSDeviceMTL.h"
#include "GS/Renderers/Metal/GSTextureMTL.h"
#include "GS/GSPerfMon.h"

#include "common/Console.h"
#include "common/HostSys.h"

#include "imgui.h"

#ifdef __APPLE__
#include "GSMTLSharedHeader.h"

static constexpr simd::float2 ToSimd(const GSVector2& vec)
{
	return simd::make_float2(vec.x, vec.y);
}

GSDevice* MakeGSDeviceMTL()
{
	return new GSDeviceMTL();
}

std::vector<std::string> GetMetalAdapterList()
{ @autoreleasepool {
	std::vector<std::string> list;
	auto devs = MRCTransfer(MTLCopyAllDevices());
	for (id<MTLDevice> dev in devs.Get())
		list.push_back([[dev name] UTF8String]);
	return list;
}}

bool GSDeviceMTL::UsageTracker::PrepareForAllocation(u64 last_draw, size_t amt)
{
	auto removeme = std::find_if(m_usage.begin(), m_usage.end(), [last_draw](UsageEntry usage){ return usage.drawno > last_draw; });
	if (removeme != m_usage.begin())
		m_usage.erase(m_usage.begin(), removeme);

	bool still_in_use = false;
	bool needs_wrap = m_pos + amt > m_size;
	if (!m_usage.empty())
	{
		size_t used = m_usage.front().pos;
		if (needs_wrap)
			still_in_use = used >= m_pos || used < amt;
		else
			still_in_use = used >= m_pos && used < m_pos + amt;
	}
	if (needs_wrap)
		m_pos = 0;

	return still_in_use || amt > m_size;
}

size_t GSDeviceMTL::UsageTracker::Allocate(u64 current_draw, size_t amt)
{
	if (m_usage.empty() || m_usage.back().drawno != current_draw)
		m_usage.push_back({current_draw, m_pos});
	size_t ret = m_pos;
	m_pos += amt;
	return ret;
}

void GSDeviceMTL::UsageTracker::Reset(size_t new_size)
{
	m_usage.clear();
	m_size = new_size;
	m_pos = 0;
}

GSDeviceMTL::GSDeviceMTL()
	: m_backref(std::make_shared<std::pair<std::mutex, GSDeviceMTL*>>())
	, m_dev(nil)
{
	m_backref->second = this;
}

GSDeviceMTL::~GSDeviceMTL()
{
}

GSDeviceMTL::Map GSDeviceMTL::Allocate(UploadBuffer& buffer, size_t amt)
{
	amt = (amt + 31) & ~31ull;
	u64 last_draw = m_last_finished_draw.load(std::memory_order_acquire);
	bool needs_new = buffer.usage.PrepareForAllocation(last_draw, amt);
	if (needs_new) [[unlikely]]
	{
		// Orphan buffer
		size_t newsize = std::max<size_t>(buffer.usage.Size() * 2, 4096);
		while (newsize < amt)
			newsize *= 2;
		MTLResourceOptions options = MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined;
		buffer.mtlbuffer = MRCTransfer([m_dev.dev newBufferWithLength:newsize options:options]);
		pxAssertRel(buffer.mtlbuffer, "Failed to allocate MTLBuffer (out of memory?)");
		buffer.buffer = [buffer.mtlbuffer contents];
		buffer.usage.Reset(newsize);
	}

	size_t pos = buffer.usage.Allocate(m_current_draw, amt);

	Map ret = {buffer.mtlbuffer, pos, reinterpret_cast<char*>(buffer.buffer) + pos};
	pxAssertMsg(pos <= buffer.usage.Size(), "Previous code should have guaranteed there was enough space");
	return ret;
}

/// Allocate space in the given buffer for use with the given render command encoder
GSDeviceMTL::Map GSDeviceMTL::Allocate(BufferPair& buffer, size_t amt)
{
	amt = (amt + 31) & ~31ull;
	u64 last_draw = m_last_finished_draw.load(std::memory_order_acquire);
	size_t base_pos = buffer.usage.Pos();
	bool needs_new = buffer.usage.PrepareForAllocation(last_draw, amt);
	bool needs_upload = needs_new || buffer.usage.Pos() == 0;
	if (!m_dev.features.unified_memory && needs_upload)
	{
		if (base_pos != buffer.last_upload)
		{
			id<MTLBlitCommandEncoder> enc = GetVertexUploadEncoder();
			[enc copyFromBuffer:buffer.cpubuffer
			       sourceOffset:buffer.last_upload
			           toBuffer:buffer.gpubuffer
			  destinationOffset:buffer.last_upload
			               size:base_pos - buffer.last_upload];
		}
		buffer.last_upload = 0;
	}
	if (needs_new) [[unlikely]]
	{
		// Orphan buffer
		size_t newsize = std::max<size_t>(buffer.usage.Size() * 2, 4096);
		while (newsize < amt)
			newsize *= 2;
		MTLResourceOptions options = MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined;
		buffer.cpubuffer = MRCTransfer([m_dev.dev newBufferWithLength:newsize options:options]);
		pxAssertRel(buffer.cpubuffer, "Failed to allocate MTLBuffer (out of memory?)");
		buffer.buffer = [buffer.cpubuffer contents];
		buffer.usage.Reset(newsize);
		if (!m_dev.features.unified_memory)
		{
			options = MTLResourceStorageModePrivate | MTLResourceHazardTrackingModeUntracked;
			buffer.gpubuffer = MRCTransfer([m_dev.dev newBufferWithLength:newsize options:options]);
			pxAssertRel(buffer.gpubuffer, "Failed to allocate MTLBuffer (out of memory?)");
		}
	}

	size_t pos = buffer.usage.Allocate(m_current_draw, amt);
	Map ret = {nil, pos, reinterpret_cast<char*>(buffer.buffer) + pos};
	ret.gpu_buffer = m_dev.features.unified_memory ? buffer.cpubuffer : buffer.gpubuffer;
	pxAssertMsg(pos <= buffer.usage.Size(), "Previous code should have guaranteed there was enough space");
	return ret;
}

void GSDeviceMTL::Sync(BufferPair& buffer)
{
	if (m_dev.features.unified_memory || buffer.usage.Pos() == buffer.last_upload)
		return;

	id<MTLBlitCommandEncoder> enc = GetVertexUploadEncoder();
	[enc copyFromBuffer:buffer.cpubuffer
	       sourceOffset:buffer.last_upload
	           toBuffer:buffer.gpubuffer
	  destinationOffset:buffer.last_upload
	               size:buffer.usage.Pos() - buffer.last_upload];
	[enc updateFence:m_draw_sync_fence];
	buffer.last_upload = buffer.usage.Pos();
}

id<MTLBlitCommandEncoder> GSDeviceMTL::GetTextureUploadEncoder()
{
	if (!m_texture_upload_cmdbuf)
	{
		m_texture_upload_cmdbuf = MRCRetain([m_queue commandBuffer]);
		m_texture_upload_encoder = MRCRetain([m_texture_upload_cmdbuf blitCommandEncoder]);
		pxAssertRel(m_texture_upload_encoder, "Failed to create texture upload encoder!");
		[m_texture_upload_cmdbuf setLabel:@"Texture Upload"];
	}
	return m_texture_upload_encoder;
}

id<MTLBlitCommandEncoder> GSDeviceMTL::GetLateTextureUploadEncoder()
{
	if (!m_late_texture_upload_encoder)
	{
		EndRenderPass();
		m_late_texture_upload_encoder = MRCRetain([GetRenderCmdBuf() blitCommandEncoder]);
		pxAssertRel(m_late_texture_upload_encoder, "Failed to create late texture upload encoder!");
		[m_late_texture_upload_encoder setLabel:@"Late Texture Upload"];
		if (!m_dev.features.unified_memory)
			[m_late_texture_upload_encoder waitForFence:m_draw_sync_fence];
	}
	return m_late_texture_upload_encoder;
}

id<MTLBlitCommandEncoder> GSDeviceMTL::GetVertexUploadEncoder()
{
	if (!m_vertex_upload_cmdbuf)
	{
		m_vertex_upload_cmdbuf = MRCRetain([m_queue commandBuffer]);
		m_vertex_upload_encoder = MRCRetain([m_vertex_upload_cmdbuf blitCommandEncoder]);
		pxAssertRel(m_vertex_upload_encoder, "Failed to create vertex upload encoder!");
		[m_vertex_upload_cmdbuf setLabel:@"Vertex Upload"];
	}
	return m_vertex_upload_encoder;
}

/// Get the draw command buffer, creating a new one if it doesn't exist
id<MTLCommandBuffer> GSDeviceMTL::GetRenderCmdBuf()
{
	if (!m_current_render_cmdbuf)
	{
		m_encoders_in_current_cmdbuf = 0;
		m_current_render_cmdbuf = MRCRetain([m_queue commandBuffer]);
		pxAssertRel(m_current_render_cmdbuf, "Failed to create draw command buffer!");
		[m_current_render_cmdbuf setLabel:@"Draw"];
	}
	return m_current_render_cmdbuf;
}

id<MTLCommandBuffer> GSDeviceMTL::GetRenderCmdBufWithoutCreate()
{
	return m_current_render_cmdbuf;
}

id<MTLFence> GSDeviceMTL::GetSpinFence()
{
	return m_spin_timer ? m_spin_fence : nil;
}

void GSDeviceMTL::DrawCommandBufferFinished(u64 draw, id<MTLCommandBuffer> buffer)
{
	// We can do the update non-atomically because we only ever update under the lock
	u64 newval = std::max(draw, m_last_finished_draw.load(std::memory_order_relaxed));
	m_last_finished_draw.store(newval, std::memory_order_release);
	AccumulateCommandBufferTime(buffer);
}

void GSDeviceMTL::FlushEncoders()
{
	bool needs_submit = m_current_render_cmdbuf;
	if (needs_submit)
	{
		EndRenderPass();
		Sync(m_vertex_upload_buf);
	}
	if (m_dev.features.unified_memory)
	{
		pxAssertMsg(!m_vertex_upload_cmdbuf, "Should never be used!");
	}
	else if (m_vertex_upload_cmdbuf)
	{
		[m_vertex_upload_encoder endEncoding];
		[m_vertex_upload_cmdbuf commit];
		m_vertex_upload_encoder = nil;
		m_vertex_upload_cmdbuf = nil;
	}
	if (m_texture_upload_cmdbuf)
	{
		[m_texture_upload_encoder endEncoding];
		[m_texture_upload_cmdbuf commit];
		m_texture_upload_encoder = nil;
		m_texture_upload_cmdbuf = nil;
	}
	if (!needs_submit)
		return;
	if (m_late_texture_upload_encoder)
	{
		[m_late_texture_upload_encoder endEncoding];
		m_late_texture_upload_encoder = nil;
	}
	u32 spin_cycles = 0;
	constexpr double s_to_ns = 1000000000;
	if (m_spin_timer)
	{
		u32 spin_id;
		{
			std::lock_guard<std::mutex> guard(m_backref->first);
			auto draw = m_spin_manager.DrawSubmitted(m_encoders_in_current_cmdbuf);
			u32 constant_offset = 200000 * m_spin_manager.SpinsPerUnitTime(); // 200µs
			u32 minimum_spin = 2 * constant_offset; // 400µs (200µs after subtracting constant_offset)
			u32 maximum_spin = std::max<u32>(1024, 16000000 * m_spin_manager.SpinsPerUnitTime()); // 16ms
			if (draw.recommended_spin > minimum_spin)
				spin_cycles = std::min(draw.recommended_spin - constant_offset, maximum_spin);
			spin_id = draw.id;
		}
		[m_current_render_cmdbuf addCompletedHandler:[backref = m_backref, draw = m_current_draw, spin_id](id<MTLCommandBuffer> buf)
		{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"
			// Starting from kernelStartTime includes time the command buffer spent waiting to execute
			// This is useful for avoiding issues on GPUs without async compute (Intel) where spinning
			// delays the next command buffer start, which then makes the spin manager think it should spin more
			// (If a command buffer contains multiple encoders, the GPU will start before the kernel finishes,
			//  so we choose kernelStartTime over kernelEndTime)
			u64 begin = [buf kernelStartTime] * s_to_ns;
			u64 end = [buf GPUEndTime] * s_to_ns;
#pragma clang diagnostic pop
			std::lock_guard<std::mutex> guard(backref->first);
			if (GSDeviceMTL* dev = backref->second)
			{
				dev->DrawCommandBufferFinished(draw, buf);
				dev->m_spin_manager.DrawCompleted(spin_id, static_cast<u32>(begin), static_cast<u32>(end));
			}
		}];
	}
	else
	{
		[m_current_render_cmdbuf addCompletedHandler:[backref = m_backref, draw = m_current_draw](id<MTLCommandBuffer> buf)
		{
			std::lock_guard<std::mutex> guard(backref->first);
			if (GSDeviceMTL* dev = backref->second)
				dev->DrawCommandBufferFinished(draw, buf);
		}];
	}
	[m_current_render_cmdbuf commit];
	m_current_render_cmdbuf = nil;
	m_current_draw++;
	if (spin_cycles)
	{
		id<MTLCommandBuffer> spinCmdBuf = [m_queue commandBuffer];
		[spinCmdBuf setLabel:@"Spin"];
		id<MTLComputeCommandEncoder> spinCmdEncoder = [spinCmdBuf computeCommandEncoder];
		[spinCmdEncoder setLabel:@"Spin"];
		[spinCmdEncoder waitForFence:m_spin_fence];
		[spinCmdEncoder setComputePipelineState:m_spin_pipeline];
		[spinCmdEncoder setBytes:&spin_cycles length:sizeof(spin_cycles) atIndex:0];
		[spinCmdEncoder setBuffer:m_spin_buffer offset:0 atIndex:1];
		[spinCmdEncoder dispatchThreadgroups:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
		[spinCmdEncoder endEncoding];
		[spinCmdBuf addCompletedHandler:[backref = m_backref, spin_cycles](id<MTLCommandBuffer> buf)
		{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"
			u64 begin = [buf GPUStartTime] * s_to_ns;
			u64 end = [buf GPUEndTime] * s_to_ns;
#pragma clang diagnostic pop
			std::lock_guard<std::mutex> guard(backref->first);
			if (GSDeviceMTL* dev = backref->second)
				dev->m_spin_manager.SpinCompleted(spin_cycles, static_cast<u32>(begin), static_cast<u32>(end));
		}];
		[spinCmdBuf commit];
	}
}

void GSDeviceMTL::FlushEncodersForReadback()
{
	FlushEncoders();
	if (@available(macOS 10.15, iOS 10.3, *))
	{
		if (GSConfig.HWSpinGPUForReadbacks)
		{
			m_spin_manager.ReadbackRequested();
			m_spin_timer = 30;
		}
	}
}

void GSDeviceMTL::EndRenderPass()
{
	if (m_current_render.encoder)
	{
		EndDebugGroup(m_current_render.encoder);
		g_perfmon.Put(GSPerfMon::RenderPasses, 1);
		if (m_spin_timer)
			[m_current_render.encoder updateFence:m_spin_fence afterStages:MTLRenderStageFragment];
		[m_current_render.encoder endEncoding];
		m_current_render.encoder = nil;
		memset(&m_current_render, 0, offsetof(MainRenderEncoder, depth_sel));
		m_current_render.depth_sel = DepthStencilSelector::NoDepth();
	}
}

void GSDeviceMTL::BeginRenderPass(NSString* name, GSTexture* color, MTLLoadAction color_load, GSTexture* depth, MTLLoadAction depth_load, GSTexture* stencil, MTLLoadAction stencil_load)
{
	GSTextureMTL* mc = static_cast<GSTextureMTL*>(color);
	GSTextureMTL* md = static_cast<GSTextureMTL*>(depth);
	GSTextureMTL* ms = static_cast<GSTextureMTL*>(stencil);
	bool needs_new = color   != m_current_render.color_target
	              || depth   != m_current_render.depth_target
	              || stencil != m_current_render.stencil_target;
	GSVector4 color_clear;
	float depth_clear;
	// Depth and stencil might be the same, so do all invalidation checks before resetting invalidation
#define CHECK_CLEAR(tex, load_action, clear, ClearGetter) \
	if (tex) \
	{ \
		if (tex->GetState() == GSTexture::State::Invalidated) \
		{ \
			load_action = MTLLoadActionDontCare; \
		} \
		else if (tex->GetState() == GSTexture::State::Cleared && load_action != MTLLoadActionDontCare) \
		{ \
			clear = tex->ClearGetter(); \
			load_action = MTLLoadActionClear; \
		} \
	}

	CHECK_CLEAR(mc, color_load, color_clear, GetUNormClearColor)
	CHECK_CLEAR(md, depth_load, depth_clear, GetClearDepth)
#undef CHECK_CLEAR
	// Stencil and depth are one texture, stencil clears aren't supported
	if (ms && ms->GetState() == GSTexture::State::Invalidated)
		stencil_load = MTLLoadActionDontCare;
	needs_new |= mc && color_load   == MTLLoadActionClear;
	needs_new |= md && depth_load   == MTLLoadActionClear;

	// Reset texture state
	if (mc) mc->SetState(GSTexture::State::Dirty);
	if (md) md->SetState(GSTexture::State::Dirty);
	if (ms) ms->SetState(GSTexture::State::Dirty);

	if (!needs_new)
	{
		if (m_current_render.name != (__bridge void*)name)
		{
			m_current_render.name = (__bridge void*)name;
			[m_current_render.encoder setLabel:name];
		}
		return;
	}

	m_encoders_in_current_cmdbuf++;

	if (m_late_texture_upload_encoder)
	{
		[m_late_texture_upload_encoder endEncoding];
		m_late_texture_upload_encoder = nullptr;
	}

	int idx = 0;
	if (mc) idx |= 1;
	if (md) idx |= 2;
	if (ms) idx |= 4;

	MTLRenderPassDescriptor* desc = m_render_pass_desc[idx];
	if (mc)
	{
		mc->m_last_write = m_current_draw;
		desc.colorAttachments[0].texture = mc->GetTexture();
		if (color_load == MTLLoadActionClear)
			desc.colorAttachments[0].clearColor = MTLClearColorMake(color_clear.r, color_clear.g, color_clear.b, color_clear.a);
		desc.colorAttachments[0].loadAction = color_load;
	}
	if (md)
	{
		md->m_last_write = m_current_draw;
		desc.depthAttachment.texture = md->GetTexture();
		if (depth_load == MTLLoadActionClear)
			desc.depthAttachment.clearDepth = depth_clear;
		desc.depthAttachment.loadAction = depth_load;
	}
	if (ms)
	{
		ms->m_last_write = m_current_draw;
		desc.stencilAttachment.texture = ms->GetTexture();
		pxAssert(stencil_load != MTLLoadActionClear);
		desc.stencilAttachment.loadAction = stencil_load;
	}

	EndRenderPass();
	m_current_render.encoder = MRCRetain([GetRenderCmdBuf() renderCommandEncoderWithDescriptor:desc]);
	m_current_render.name = (__bridge void*)name;
	[m_current_render.encoder setLabel:name];
	if (!m_dev.features.unified_memory)
		[m_current_render.encoder waitForFence:m_draw_sync_fence
		                          beforeStages:MTLRenderStageVertex];
	m_current_render.color_target = color;
	m_current_render.depth_target = depth;
	m_current_render.stencil_target = stencil;
	pxAssertRel(m_current_render.encoder, "Failed to create render encoder!");
}

void GSDeviceMTL::FrameCompleted()
{
	if (m_spin_timer)
		m_spin_timer--;
	m_spin_manager.NextFrame();
}

static constexpr MTLPixelFormat ConvertPixelFormat(GSTexture::Format format)
{
	switch (format)
	{
		case GSTexture::Format::PrimID:       return MTLPixelFormatR32Float;
		case GSTexture::Format::UInt32:       return MTLPixelFormatR32Uint;
		case GSTexture::Format::UInt16:       return MTLPixelFormatR16Uint;
		case GSTexture::Format::UNorm8:       return MTLPixelFormatA8Unorm;
		case GSTexture::Format::Color:        return MTLPixelFormatRGBA8Unorm;
		case GSTexture::Format::HDRColor:     return MTLPixelFormatRGBA16Unorm;
		case GSTexture::Format::DepthStencil: return MTLPixelFormatDepth32Float_Stencil8;
		case GSTexture::Format::Invalid:      return MTLPixelFormatInvalid;
		case GSTexture::Format::BC1:          return MTLPixelFormatBC1_RGBA;
		case GSTexture::Format::BC2:          return MTLPixelFormatBC2_RGBA;
		case GSTexture::Format::BC3:          return MTLPixelFormatBC3_RGBA;
		case GSTexture::Format::BC7:          return MTLPixelFormatBC7_RGBAUnorm;
	}
}

GSTexture* GSDeviceMTL::CreateSurface(GSTexture::Type type, int width, int height, int levels, GSTexture::Format format)
{ @autoreleasepool {
	MTLPixelFormat fmt = ConvertPixelFormat(format);
	pxAssertRel(format != GSTexture::Format::Invalid, "Can't create surface of this format!");

	MTLTextureDescriptor* desc = [MTLTextureDescriptor
		texture2DDescriptorWithPixelFormat:fmt
		                             width:std::max(1, std::min(width,  m_dev.features.max_texsize))
		                            height:std::max(1, std::min(height, m_dev.features.max_texsize))
		                         mipmapped:levels > 1];

	if (levels > 1)
		[desc setMipmapLevelCount:levels];

	[desc setStorageMode:MTLStorageModePrivate];
	switch (type)
	{
		case GSTexture::Type::Texture:
			[desc setUsage:MTLTextureUsageShaderRead];
			break;
		case GSTexture::Type::RenderTarget:
			if (m_dev.features.slow_color_compression)
				[desc setUsage:MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget | MTLTextureUsagePixelFormatView]; // Force color compression off by including PixelFormatView
			else
				[desc setUsage:MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget];
			break;
		case GSTexture::Type::RWTexture:
			[desc setUsage:MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite];
			break;
		default:
			[desc setUsage:MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget];
	}

	MRCOwned<id<MTLTexture>> tex = MRCTransfer([m_dev.dev newTextureWithDescriptor:desc]);
	if (tex)
	{
		GSTextureMTL* t = new GSTextureMTL(this, tex, type, format);
		switch (type)
		{
			case GSTexture::Type::RenderTarget:
				ClearRenderTarget(t, 0);
				break;
			case GSTexture::Type::DepthStencil:
				ClearDepth(t, 0.0f);
				break;
			default:
				break;
		}
		return t;
	}
	else
	{
		return nullptr;
	}
}}

void GSDeviceMTL::DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect, const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, u32 c, const bool linear)
{ @autoreleasepool {
	id<MTLCommandBuffer> cmdbuf = GetRenderCmdBuf();
	GSScopedDebugGroupMTL dbg(cmdbuf, @"DoMerge");

	GSVector4 full_r(0.0f, 0.0f, 1.0f, 1.0f);
	bool feedback_write_2 = PMODE.EN2 && sTex[2] != nullptr && EXTBUF.FBIN == 1;
	bool feedback_write_1 = PMODE.EN1 && sTex[2] != nullptr && EXTBUF.FBIN == 0;
	bool feedback_write_2_but_blend_bg = feedback_write_2 && PMODE.SLBG == 1;

	ClearRenderTarget(dTex, c);

	const GSVector4 unorm_c = GSVector4::unorm8(c);
	vector_float4 cb_c = { unorm_c.r, unorm_c.g, unorm_c.b, unorm_c.a };
	GSMTLConvertPSUniform cb_yuv = {};
	cb_yuv.emoda = EXTBUF.EMODA;
	cb_yuv.emodc = EXTBUF.EMODC;

	if (sTex[1] && (PMODE.SLBG == 0 || feedback_write_2_but_blend_bg))
	{
		// 2nd output is enabled and selected. Copy it to destination so we can blend it with 1st output
		// Note: value outside of dRect must contains the background color (c)
		StretchRect(sTex[1], sRect[1], dTex, dRect[1], ShaderConvert::COPY, linear);
	}

	// Save 2nd output
	if (feedback_write_2) // FIXME I'm not sure dRect[1] is always correct
		DoStretchRect(dTex, full_r, sTex[2], dRect[1], m_convert_pipeline[static_cast<int>(ShaderConvert::YUV)], linear, LoadAction::DontCareIfFull, &cb_yuv, sizeof(cb_yuv));

	if (feedback_write_2_but_blend_bg)
		ClearRenderTarget(dTex, c);

	if (sTex[0])
	{
		int idx = (PMODE.AMOD << 1) | PMODE.MMOD;
		id<MTLRenderPipelineState> pipeline = m_merge_pipeline[idx];

		// 1st output is enabled. It must be blended
		if (PMODE.MMOD == 1)
		{
			// Blend with a constant alpha
			DoStretchRect(sTex[0], sRect[0], dTex, dRect[0], pipeline, linear, LoadAction::Load, &cb_c, sizeof(cb_c));
		}
		else
		{
			// Blend with 2 * input alpha
			DoStretchRect(sTex[0], sRect[0], dTex, dRect[0], pipeline, linear, LoadAction::Load, nullptr, 0);
		}
	}

	if (feedback_write_1) // FIXME I'm not sure dRect[0] is always correct
		StretchRect(dTex, full_r, sTex[2], dRect[0], ShaderConvert::YUV, linear);
}}

void GSDeviceMTL::DoInterlace(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ShaderInterlace shader, bool linear, const InterlaceConstantBuffer& cb)
{ @autoreleasepool {
	id<MTLCommandBuffer> cmdbuf = GetRenderCmdBuf();
	GSScopedDebugGroupMTL dbg(cmdbuf, @"DoInterlace");

	const bool can_discard = shader == ShaderInterlace::WEAVE || shader == ShaderInterlace::MAD_BUFFER;
	DoStretchRect(sTex, sRect, dTex, dRect, m_interlace_pipeline[static_cast<int>(shader)], linear, !can_discard ? LoadAction::DontCareIfFull : LoadAction::Load, &cb, sizeof(cb));
}}

void GSDeviceMTL::DoFXAA(GSTexture* sTex, GSTexture* dTex)
{
	BeginRenderPass(@"FXAA", dTex, MTLLoadActionDontCare, nullptr, MTLLoadActionDontCare);
	RenderCopy(sTex, m_fxaa_pipeline, GSVector4i(0, 0, dTex->GetSize().x, dTex->GetSize().y));
}

void GSDeviceMTL::DoShadeBoost(GSTexture* sTex, GSTexture* dTex, const float params[4])
{
	BeginRenderPass(@"ShadeBoost", dTex, MTLLoadActionDontCare, nullptr, MTLLoadActionDontCare);
	[m_current_render.encoder setFragmentBytes:params
	                                    length:sizeof(float) * 4
	                                   atIndex:GSMTLBufferIndexUniforms];
	RenderCopy(sTex, m_shadeboost_pipeline, GSVector4i(0, 0, dTex->GetSize().x, dTex->GetSize().y));
}

bool GSDeviceMTL::DoCAS(GSTexture* sTex, GSTexture* dTex, bool sharpen_only, const std::array<u32, NUM_CAS_CONSTANTS>& constants)
{ @autoreleasepool {
	static constexpr int threadGroupWorkRegionDim = 16;
	const int dispatchX = (dTex->GetWidth() + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
	const int dispatchY = (dTex->GetHeight() + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
	static_assert(sizeof(constants) == sizeof(GSMTLCASPSUniform));

	EndRenderPass();
	id<MTLComputeCommandEncoder> enc = [GetRenderCmdBuf() computeCommandEncoder];
	[enc setLabel:@"CAS"];
	[enc setComputePipelineState:m_cas_pipeline[sharpen_only]];
	[enc setTexture:static_cast<GSTextureMTL*>(sTex)->GetTexture() atIndex:0];
	[enc setTexture:static_cast<GSTextureMTL*>(dTex)->GetTexture() atIndex:1];
	[enc setBytes:&constants length:sizeof(constants) atIndex:GSMTLBufferIndexUniforms];
	[enc dispatchThreadgroups:MTLSizeMake(dispatchX, dispatchY, 1)
	    threadsPerThreadgroup:MTLSizeMake(64, 1, 1)];
	[enc endEncoding];
	return true;
}}

MRCOwned<id<MTLFunction>> GSDeviceMTL::LoadShader(NSString* name)
{
	NSError* err = nil;
	MRCOwned<id<MTLFunction>> fn = MRCTransfer([m_dev.shaders newFunctionWithName:name constantValues:m_fn_constants error:&err]);
	if (err) [[unlikely]]
	{
		NSString* msg = [NSString stringWithFormat:@"Failed to load shader %@: %@", name, [err localizedDescription]];
		Console.Error("%s", [msg UTF8String]);
		pxFailRel([msg UTF8String]);
	}
	return fn;
}

MRCOwned<id<MTLRenderPipelineState>> GSDeviceMTL::MakePipeline(MTLRenderPipelineDescriptor* desc, id<MTLFunction> vertex, id<MTLFunction> fragment, NSString* name)
{
	[desc setLabel:name];
	[desc setVertexFunction:vertex];
	[desc setFragmentFunction:fragment];
	NSError* err;
	MRCOwned<id<MTLRenderPipelineState>> res = MRCTransfer([m_dev.dev newRenderPipelineStateWithDescriptor:desc error:&err]);
	if (err) [[unlikely]]
	{
		NSString* msg = [NSString stringWithFormat:@"Failed to create pipeline %@: %@", name, [err localizedDescription]];
		Console.Error("%s", [msg UTF8String]);
		pxFailRel([msg UTF8String]);
	}
	return res;
}

MRCOwned<id<MTLComputePipelineState>> GSDeviceMTL::MakeComputePipeline(id<MTLFunction> compute, NSString* name)
{
	MRCOwned<MTLComputePipelineDescriptor*> desc = MRCTransfer([MTLComputePipelineDescriptor new]);
	[desc setLabel:name];
	[desc setComputeFunction:compute];
	NSError* err;
	MRCOwned<id<MTLComputePipelineState>> res = MRCTransfer([m_dev.dev
		newComputePipelineStateWithDescriptor:desc
		                              options:0
		                           reflection:nil
		                                error:&err]);
	if (err) [[unlikely]]
	{
		NSString* msg = [NSString stringWithFormat:@"Failed to create pipeline %@: %@", name, [err localizedDescription]];
		Console.Error("%s", [msg UTF8String]);
		pxFailRel([msg UTF8String]);
	}
	return res;
}

static void applyAttribute(MTLVertexDescriptor* desc, NSUInteger idx, MTLVertexFormat fmt, NSUInteger offset, NSUInteger buffer_index)
{
	MTLVertexAttributeDescriptor* attrs = desc.attributes[idx];
	attrs.format = fmt;
	attrs.offset = offset;
	attrs.bufferIndex = buffer_index;
}

static void setFnConstantB(MTLFunctionConstantValues* fc, bool value, GSMTLFnConstants constant)
{
	[fc setConstantValue:&value type:MTLDataTypeBool atIndex:constant];
}

static void setFnConstantI(MTLFunctionConstantValues* fc, unsigned int value, GSMTLFnConstants constant)
{
	[fc setConstantValue:&value type:MTLDataTypeUInt atIndex:constant];
}

template <typename Fn>
static void OnMainThread(Fn&& fn)
{
	if ([NSThread isMainThread])
		fn();
	else
		dispatch_sync(dispatch_get_main_queue(), fn);
}

RenderAPI GSDeviceMTL::GetRenderAPI() const
{
	return RenderAPI::Metal;
}

bool GSDeviceMTL::HasSurface()  const { return static_cast<bool>(m_layer);}

void GSDeviceMTL::AttachSurfaceOnMainThread()
{
	pxAssert([NSThread isMainThread]);
	m_layer = MRCRetain([CAMetalLayer layer]);
	[m_layer setDrawableSize:CGSizeMake(m_window_info.surface_width, m_window_info.surface_height)];
	[m_layer setDevice:m_dev.dev];
	m_view = MRCRetain((__bridge NSView*)m_window_info.window_handle);
	[m_view setWantsLayer:YES];
	[m_view setLayer:m_layer];
}

void GSDeviceMTL::DetachSurfaceOnMainThread()
{
	pxAssert([NSThread isMainThread]);
	[m_view setLayer:nullptr];
	[m_view setWantsLayer:NO];
	m_view = nullptr;
	m_layer = nullptr;
}

// Metal is fun and won't let you use newBufferWithBytes for private buffers
static MRCOwned<id<MTLBuffer>> CreatePrivateBufferWithContent(
	id<MTLDevice> dev, id<MTLCommandBuffer> cb,
	MTLResourceOptions options, NSUInteger length,
	std::function<void(void*)> fill)
{
	MRCOwned<id<MTLBuffer>> tmp = MRCTransfer([dev newBufferWithLength:length options:MTLResourceStorageModeShared]);
	MRCOwned<id<MTLBuffer>> actual = MRCTransfer([dev newBufferWithLength:length options:options|MTLResourceStorageModePrivate]);
	fill([tmp contents]);
	id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
	[blit copyFromBuffer:tmp sourceOffset:0 toBuffer:actual destinationOffset:0 size:length];
	[blit endEncoding];
	return actual;
}

static MRCOwned<id<MTLSamplerState>> CreateSampler(id<MTLDevice> dev, GSHWDrawConfig::SamplerSelector sel)
{
	MRCOwned<MTLSamplerDescriptor*> sdesc = MRCTransfer([MTLSamplerDescriptor new]);
	const char* minname = sel.biln ? "Ln" : "Pt";
	const char* magname = minname;
	[sdesc setMinFilter:sel.biln ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest];
	[sdesc setMagFilter:sel.biln ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest];
	switch (static_cast<GS_MIN_FILTER>(sel.triln))
	{
		case GS_MIN_FILTER::Nearest:
		case GS_MIN_FILTER::Linear:
			[sdesc setMipFilter:MTLSamplerMipFilterNotMipmapped];
			break;
		case GS_MIN_FILTER::Nearest_Mipmap_Nearest:
			minname = "PtPt";
			[sdesc setMinFilter:MTLSamplerMinMagFilterNearest];
			[sdesc setMipFilter:MTLSamplerMipFilterNearest];
			break;
		case GS_MIN_FILTER::Nearest_Mipmap_Linear:
			minname = "PtLn";
			[sdesc setMinFilter:MTLSamplerMinMagFilterNearest];
			[sdesc setMipFilter:MTLSamplerMipFilterLinear];
			break;
		case GS_MIN_FILTER::Linear_Mipmap_Nearest:
			minname = "LnPt";
			[sdesc setMinFilter:MTLSamplerMinMagFilterLinear];
			[sdesc setMipFilter:MTLSamplerMipFilterNearest];
			break;
		case GS_MIN_FILTER::Linear_Mipmap_Linear:
			minname = "LnLn";
			[sdesc setMinFilter:MTLSamplerMinMagFilterLinear];
			[sdesc setMipFilter:MTLSamplerMipFilterLinear];
			break;
	}

	const char* taudesc = sel.tau ? "Repeat" : "Clamp";
	const char* tavdesc = sel.tav == sel.tau ? "" : sel.tav ? "Repeat" : "Clamp";
	[sdesc setSAddressMode:sel.tau ? MTLSamplerAddressModeRepeat : MTLSamplerAddressModeClampToEdge];
	[sdesc setTAddressMode:sel.tav ? MTLSamplerAddressModeRepeat : MTLSamplerAddressModeClampToEdge];
	[sdesc setRAddressMode:MTLSamplerAddressModeClampToEdge];

	[sdesc setMaxAnisotropy:GSConfig.MaxAnisotropy && sel.aniso ? GSConfig.MaxAnisotropy : 1];
	[sdesc setLodMaxClamp:(sel.lodclamp || sel.UseMipmapFiltering()) ? 0.25f : FLT_MAX];

	[sdesc setLabel:[NSString stringWithFormat:@"%s%s %s%s", taudesc, tavdesc, magname, minname]];
	MRCOwned<id<MTLSamplerState>> ret = MRCTransfer([dev newSamplerStateWithDescriptor:sdesc]);
	pxAssertRel(ret, "Failed to create sampler!");
	return ret;
}

bool GSDeviceMTL::Create()
{ @autoreleasepool {
	if (!GSDevice::Create())
		return false;

	NSString* ns_adapter_name = [NSString stringWithUTF8String:GSConfig.Adapter.c_str()];
	auto devs = MRCTransfer(MTLCopyAllDevices());
	for (id<MTLDevice> dev in devs.Get())
	{
		if ([[dev name] isEqualToString:ns_adapter_name])
			m_dev = GSMTLDevice(MRCRetain(dev));
	}
	if (!m_dev.dev)
	{
		if (!GSConfig.Adapter.empty())
			Console.Warning("Metal: Couldn't find adapter %s, using default", GSConfig.Adapter.c_str());
		m_dev = GSMTLDevice(MRCTransfer(MTLCreateSystemDefaultDevice()));
		if (!m_dev.dev)
			Host::ReportErrorAsync("No Metal Devices Available", "No Metal-supporting GPUs were found.  PCSX2 requires a Metal GPU (available on all macs from 2012 onwards).");
	}
	m_queue = MRCTransfer([m_dev.dev newCommandQueue]);

	m_pass_desc = MRCTransfer([MTLRenderPassDescriptor new]);
	[m_pass_desc colorAttachments][0].loadAction = MTLLoadActionClear;
	[m_pass_desc colorAttachments][0].clearColor = MTLClearColorMake(0, 0, 0, 0);
	[m_pass_desc colorAttachments][0].storeAction = MTLStoreActionStore;

	if (char* env = getenv("MTL_USE_PRESENT_DRAWABLE"))
		m_use_present_drawable = static_cast<UsePresentDrawable>(atoi(env));
	else if (@available(macOS 13.0, *))
		m_use_present_drawable = UsePresentDrawable::Always;
	else // Before Ventura, presentDrawable acts like vsync is on when windowed
		m_use_present_drawable = UsePresentDrawable::IfVsync;

	m_capture_start_frame = 0;
	if (char* env = getenv("MTL_CAPTURE"))
	{
		m_capture_start_frame = atoi(env);
	}
	if (m_capture_start_frame)
	{
		Console.WriteLn("Metal will capture frame %u", m_capture_start_frame);
	}

	if (m_dev.IsOk() && m_queue)
	{
		// This is a little less than ideal, pinging back and forward between threads, but we don't really
		// have any other option, because Qt uses a blocking queued connection for window acquire.
		if (!AcquireWindow(true))
			return false;

		OnMainThread([this]
		{
			AttachSurfaceOnMainThread();
		});
		[m_layer setDisplaySyncEnabled:m_vsync_mode != VsyncMode::Off];
	}
	else
	{
		return false;
	}

	MTLPixelFormat layer_px_fmt = [m_layer pixelFormat];

	m_features.broken_point_sampler = [[m_dev.dev name] containsString:@"AMD"];
	m_features.vs_expand = !GSConfig.DisableVertexShaderExpand;
	m_features.primitive_id = m_dev.features.primid;
	m_features.texture_barrier = true;
	m_features.provoking_vertex_last = false;
	m_features.point_expand = true;
	m_features.line_expand = false;
	m_features.prefer_new_textures = true;
	m_features.dxt_textures = true;
	m_features.bptc_textures = true;
	m_features.framebuffer_fetch = m_dev.features.framebuffer_fetch && !GSConfig.DisableFramebufferFetch;
	m_features.dual_source_blend = true;
	m_features.clip_control = true;
	m_features.stencil_buffer = true;
	m_features.cas_sharpening = true;
	m_features.test_and_sample_depth = true;

	// Init metal stuff
	m_fn_constants = MRCTransfer([MTLFunctionConstantValues new]);
	setFnConstantB(m_fn_constants, m_dev.features.framebuffer_fetch, GSMTLConstantIndex_FRAMEBUFFER_FETCH);

	m_draw_sync_fence = MRCTransfer([m_dev.dev newFence]);
	[m_draw_sync_fence setLabel:@"Draw Sync Fence"];
	m_spin_fence = MRCTransfer([m_dev.dev newFence]);
	[m_spin_fence setLabel:@"Spin Fence"];
	constexpr MTLResourceOptions spin_opts = MTLResourceStorageModePrivate | MTLResourceHazardTrackingModeUntracked;
	m_spin_buffer = MRCTransfer([m_dev.dev newBufferWithLength:4 options:spin_opts]);
	[m_spin_buffer setLabel:@"Spin Buffer"];
	id<MTLCommandBuffer> initCommands = [m_queue commandBuffer];
	id<MTLBlitCommandEncoder> clearSpinBuffer = [initCommands blitCommandEncoder];
	[clearSpinBuffer fillBuffer:m_spin_buffer range:NSMakeRange(0, 4) value:0];
	[clearSpinBuffer updateFence:m_spin_fence];
	[clearSpinBuffer endEncoding];
	m_spin_pipeline = MakeComputePipeline(LoadShader(@"waste_time"), @"waste_time");

	for (int sharpen_only = 0; sharpen_only < 2; sharpen_only++)
	{
		setFnConstantB(m_fn_constants, sharpen_only, GSMTLConstantIndex_CAS_SHARPEN_ONLY);
		NSString* shader = m_dev.features.has_fast_half ? @"CASHalf" : @"CASFloat";
		m_cas_pipeline[sharpen_only] = MakeComputePipeline(LoadShader(shader), sharpen_only ? @"CAS Sharpen" : @"CAS Upscale");
	}

	m_expand_index_buffer = CreatePrivateBufferWithContent(m_dev.dev, initCommands, MTLResourceHazardTrackingModeUntracked, EXPAND_BUFFER_SIZE, GenerateExpansionIndexBuffer);
	[m_expand_index_buffer setLabel:@"Point/Sprite Expand Indices"];

	m_hw_vertex = MRCTransfer([MTLVertexDescriptor new]);
	[[[m_hw_vertex layouts] objectAtIndexedSubscript:GSMTLBufferIndexHWVertices] setStride:sizeof(GSVertex)];
	applyAttribute(m_hw_vertex, GSMTLAttributeIndexST, MTLVertexFormatFloat2,           offsetof(GSVertex, ST),      GSMTLBufferIndexHWVertices);
	applyAttribute(m_hw_vertex, GSMTLAttributeIndexC,  MTLVertexFormatUChar4,           offsetof(GSVertex, RGBAQ.R), GSMTLBufferIndexHWVertices);
	applyAttribute(m_hw_vertex, GSMTLAttributeIndexQ,  MTLVertexFormatFloat,            offsetof(GSVertex, RGBAQ.Q), GSMTLBufferIndexHWVertices);
	applyAttribute(m_hw_vertex, GSMTLAttributeIndexXY, MTLVertexFormatUShort2,          offsetof(GSVertex, XYZ.X),   GSMTLBufferIndexHWVertices);
	applyAttribute(m_hw_vertex, GSMTLAttributeIndexZ,  MTLVertexFormatUInt,             offsetof(GSVertex, XYZ.Z),   GSMTLBufferIndexHWVertices);
	applyAttribute(m_hw_vertex, GSMTLAttributeIndexUV, MTLVertexFormatUShort2,          offsetof(GSVertex, UV),      GSMTLBufferIndexHWVertices);
	applyAttribute(m_hw_vertex, GSMTLAttributeIndexF,  MTLVertexFormatUChar4Normalized, offsetof(GSVertex, FOG),     GSMTLBufferIndexHWVertices);

	for (auto& desc : m_render_pass_desc)
	{
		desc = MRCTransfer([MTLRenderPassDescriptor new]);
		[[desc   depthAttachment] setStoreAction:MTLStoreActionStore];
		[[desc stencilAttachment] setStoreAction:MTLStoreActionStore];
	}

	// Init samplers
	m_sampler_hw[SamplerSelector::Linear().key] = CreateSampler(m_dev.dev, SamplerSelector::Linear());
	m_sampler_hw[SamplerSelector::Point().key] = CreateSampler(m_dev.dev, SamplerSelector::Point());

	// Init depth stencil states
	MTLDepthStencilDescriptor* dssdesc = [[MTLDepthStencilDescriptor new] autorelease];
	MTLStencilDescriptor* stencildesc = [[MTLStencilDescriptor new] autorelease];
	stencildesc.stencilCompareFunction = MTLCompareFunctionAlways;
	stencildesc.depthFailureOperation = MTLStencilOperationKeep;
	stencildesc.stencilFailureOperation = MTLStencilOperationKeep;
	stencildesc.depthStencilPassOperation = MTLStencilOperationReplace;
	dssdesc.frontFaceStencil = stencildesc;
	dssdesc.backFaceStencil = stencildesc;
	[dssdesc setLabel:@"Stencil Write"];
	m_dss_stencil_write = MRCTransfer([m_dev.dev newDepthStencilStateWithDescriptor:dssdesc]);
	dssdesc.frontFaceStencil.depthStencilPassOperation = MTLStencilOperationZero;
	dssdesc.backFaceStencil.depthStencilPassOperation = MTLStencilOperationZero;
	[dssdesc setLabel:@"Stencil Zero"];
	m_dss_stencil_zero = MRCTransfer([m_dev.dev newDepthStencilStateWithDescriptor:dssdesc]);
	stencildesc.stencilCompareFunction = MTLCompareFunctionEqual;
	stencildesc.readMask = 1;
	stencildesc.writeMask = 1;
	for (size_t i = 0; i < std::size(m_dss_hw); i++)
	{
		GSHWDrawConfig::DepthStencilSelector sel;
		sel.key = i;
		if (sel.date)
		{
			if (sel.date_one)
				stencildesc.depthStencilPassOperation = MTLStencilOperationZero;
			else
				stencildesc.depthStencilPassOperation = MTLStencilOperationKeep;
			dssdesc.frontFaceStencil = stencildesc;
			dssdesc.backFaceStencil = stencildesc;
		}
		else
		{
			dssdesc.frontFaceStencil = nil;
			dssdesc.backFaceStencil = nil;
		}
		dssdesc.depthWriteEnabled = sel.zwe ? YES : NO;
		static constexpr MTLCompareFunction ztst[] =
		{
			MTLCompareFunctionNever,
			MTLCompareFunctionAlways,
			MTLCompareFunctionGreaterEqual,
			MTLCompareFunctionGreater,
		};
		static constexpr const char* ztstname[] =
		{
			"DepthNever",
			"DepthAlways",
			"DepthGEq",
			"DepthEq",
		};
		const char* datedesc = sel.date ? (sel.date_one ? " DATE_ONE" : " DATE") : "";
		const char* zwedesc = sel.zwe ? " ZWE" : "";
		dssdesc.depthCompareFunction = ztst[sel.ztst];
		[dssdesc setLabel:[NSString stringWithFormat:@"%s%s%s", ztstname[sel.ztst], zwedesc, datedesc]];
		m_dss_hw[i] = MRCTransfer([m_dev.dev newDepthStencilStateWithDescriptor:dssdesc]);
	}

	// Init HW Vertex Shaders
	for (size_t i = 0; i < std::size(m_hw_vs); i++)
	{
		VSSelector sel;
		sel.key = i;
		if (sel.point_size && sel.expand != GSMTLExpandType::None)
			continue;
		setFnConstantB(m_fn_constants, sel.fst,        GSMTLConstantIndex_FST);
		setFnConstantB(m_fn_constants, sel.iip,        GSMTLConstantIndex_IIP);
		setFnConstantB(m_fn_constants, sel.point_size, GSMTLConstantIndex_VS_POINT_SIZE);
		NSString* shader = @"vs_main";
		if (sel.expand != GSMTLExpandType::None)
		{
			setFnConstantI(m_fn_constants, static_cast<u32>(sel.expand), GSMTLConstantIndex_VS_EXPAND_TYPE);
			shader = @"vs_main_expand";
		}
		m_hw_vs[i] = LoadShader(shader);
	}

	// Init pipelines
	auto vs_convert = LoadShader(@"vs_convert");
	auto fs_triangle = LoadShader(@"fs_triangle");
	auto ps_copy = LoadShader(@"ps_copy");
	auto pdesc = [[MTLRenderPipelineDescriptor new] autorelease];
	// FS Triangle Pipelines
	pdesc.colorAttachments[0].pixelFormat = ConvertPixelFormat(GSTexture::Format::Color);
	m_hdr_resolve_pipeline = MakePipeline(pdesc, fs_triangle, LoadShader(@"ps_hdr_resolve"), @"HDR Resolve");
	m_fxaa_pipeline = MakePipeline(pdesc, fs_triangle, LoadShader(@"ps_fxaa"), @"fxaa");
	m_shadeboost_pipeline = MakePipeline(pdesc, fs_triangle, LoadShader(@"ps_shadeboost"), @"shadeboost");
	m_clut_pipeline[0] = MakePipeline(pdesc, fs_triangle, LoadShader(@"ps_convert_clut_4"), @"4-bit CLUT Update");
	m_clut_pipeline[1] = MakePipeline(pdesc, fs_triangle, LoadShader(@"ps_convert_clut_8"), @"8-bit CLUT Update");
	pdesc.colorAttachments[0].pixelFormat = ConvertPixelFormat(GSTexture::Format::HDRColor);
	m_hdr_init_pipeline = MakePipeline(pdesc, fs_triangle, LoadShader(@"ps_hdr_init"), @"HDR Init");
	pdesc.colorAttachments[0].pixelFormat = MTLPixelFormatInvalid;
	pdesc.stencilAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
	m_datm_pipeline[0] = MakePipeline(pdesc, fs_triangle, LoadShader(@"ps_datm0"), @"datm0");
	m_datm_pipeline[1] = MakePipeline(pdesc, fs_triangle, LoadShader(@"ps_datm1"), @"datm1");
	m_stencil_clear_pipeline = MakePipeline(pdesc, fs_triangle, nil, @"Stencil Clear");
	pdesc.colorAttachments[0].pixelFormat = ConvertPixelFormat(GSTexture::Format::PrimID);
	pdesc.stencilAttachmentPixelFormat = MTLPixelFormatInvalid;
	pdesc.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
	m_primid_init_pipeline[1][0] = MakePipeline(pdesc, fs_triangle, LoadShader(@"ps_primid_init_datm0"), @"PrimID DATM0 Clear");
	m_primid_init_pipeline[1][1] = MakePipeline(pdesc, fs_triangle, LoadShader(@"ps_primid_init_datm1"), @"PrimID DATM1 Clear");
	pdesc.depthAttachmentPixelFormat = MTLPixelFormatInvalid;
	m_primid_init_pipeline[0][0] = MakePipeline(pdesc, fs_triangle, LoadShader(@"ps_primid_init_datm0"), @"PrimID DATM0 Clear");
	m_primid_init_pipeline[0][1] = MakePipeline(pdesc, fs_triangle, LoadShader(@"ps_primid_init_datm1"), @"PrimID DATM1 Clear");

	pdesc.colorAttachments[0].pixelFormat = ConvertPixelFormat(GSTexture::Format::Color);
	applyAttribute(pdesc.vertexDescriptor, 0, MTLVertexFormatFloat2, offsetof(ConvertShaderVertex, pos),    0);
	applyAttribute(pdesc.vertexDescriptor, 1, MTLVertexFormatFloat2, offsetof(ConvertShaderVertex, texpos), 0);
	pdesc.vertexDescriptor.layouts[0].stride = sizeof(ConvertShaderVertex);

	for (size_t i = 0; i < std::size(m_interlace_pipeline); i++)
	{
		NSString* name = [NSString stringWithFormat:@"ps_interlace%zu", i];
		m_interlace_pipeline[i] = MakePipeline(pdesc, vs_convert, LoadShader(name), name);
	}
	for (size_t i = 0; i < std::size(m_convert_pipeline); i++)
	{
		ShaderConvert conv = static_cast<ShaderConvert>(i);
		NSString* name = [NSString stringWithCString:shaderName(conv) encoding:NSUTF8StringEncoding];
		switch (conv)
		{
			case ShaderConvert::Count:
			case ShaderConvert::DATM_0:
			case ShaderConvert::DATM_1:
			case ShaderConvert::CLUT_4:
			case ShaderConvert::CLUT_8:
			case ShaderConvert::HDR_INIT:
			case ShaderConvert::HDR_RESOLVE:
				continue;
			case ShaderConvert::FLOAT32_TO_32_BITS:
				pdesc.colorAttachments[0].pixelFormat = ConvertPixelFormat(GSTexture::Format::UInt32);
				pdesc.depthAttachmentPixelFormat = MTLPixelFormatInvalid;
				break;
			case ShaderConvert::FLOAT32_TO_16_BITS:
			case ShaderConvert::RGBA8_TO_16_BITS:
				pdesc.colorAttachments[0].pixelFormat = ConvertPixelFormat(GSTexture::Format::UInt16);
				pdesc.depthAttachmentPixelFormat = MTLPixelFormatInvalid;
				break;
			case ShaderConvert::DEPTH_COPY:
			case ShaderConvert::RGBA8_TO_FLOAT32:
			case ShaderConvert::RGBA8_TO_FLOAT24:
			case ShaderConvert::RGBA8_TO_FLOAT16:
			case ShaderConvert::RGB5A1_TO_FLOAT16:
			case ShaderConvert::RGBA8_TO_FLOAT32_BILN:
			case ShaderConvert::RGBA8_TO_FLOAT24_BILN:
			case ShaderConvert::RGBA8_TO_FLOAT16_BILN:
			case ShaderConvert::RGB5A1_TO_FLOAT16_BILN:
				pdesc.colorAttachments[0].pixelFormat = MTLPixelFormatInvalid;
				pdesc.depthAttachmentPixelFormat = ConvertPixelFormat(GSTexture::Format::DepthStencil);
				break;
			case ShaderConvert::COPY:
			case ShaderConvert::RGBA_TO_8I: // Yes really
			case ShaderConvert::TRANSPARENCY_FILTER:
			case ShaderConvert::FLOAT32_TO_RGBA8:
			case ShaderConvert::FLOAT32_TO_RGB8:
			case ShaderConvert::FLOAT16_TO_RGB5A1:
			case ShaderConvert::YUV:
				pdesc.colorAttachments[0].pixelFormat = ConvertPixelFormat(GSTexture::Format::Color);
				pdesc.depthAttachmentPixelFormat = MTLPixelFormatInvalid;
				break;
		}
		const u32 scmask = ShaderConvertWriteMask(conv);
		MTLColorWriteMask mask = MTLColorWriteMaskNone;
		if (scmask & 1) mask |= MTLColorWriteMaskRed;
		if (scmask & 2) mask |= MTLColorWriteMaskGreen;
		if (scmask & 4) mask |= MTLColorWriteMaskBlue;
		if (scmask & 8) mask |= MTLColorWriteMaskAlpha;
		pdesc.colorAttachments[0].writeMask = mask;
		m_convert_pipeline[i] = MakePipeline(pdesc, vs_convert, LoadShader(name), name);
	}
	pdesc.colorAttachments[0].writeMask = MTLColorWriteMaskAll;
	pdesc.depthAttachmentPixelFormat = MTLPixelFormatInvalid;
	for (size_t i = 0; i < std::size(m_present_pipeline); i++)
	{
		PresentShader conv = static_cast<PresentShader>(i);
		NSString* name = [NSString stringWithCString:shaderName(conv) encoding:NSUTF8StringEncoding];
		pdesc.colorAttachments[0].pixelFormat = layer_px_fmt;
		m_present_pipeline[i] = MakePipeline(pdesc, vs_convert, LoadShader(name), [NSString stringWithFormat:@"present_%s", shaderName(conv) + 3]);
	}

	pdesc.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
	for (size_t i = 0; i < std::size(m_convert_pipeline_copy_mask); i++)
	{
		MTLColorWriteMask mask = MTLColorWriteMaskNone;
		if (i & 1) mask |= MTLColorWriteMaskRed;
		if (i & 2) mask |= MTLColorWriteMaskGreen;
		if (i & 4) mask |= MTLColorWriteMaskBlue;
		if (i & 8) mask |= MTLColorWriteMaskAlpha;
		NSString* name = [NSString stringWithFormat:@"copy_%s%s%s%s", i & 1 ? "r" : "", i & 2 ? "g" : "", i & 4 ? "b" : "", i & 8 ? "a" : ""];
		pdesc.colorAttachments[0].writeMask = mask;
		m_convert_pipeline_copy_mask[i] = MakePipeline(pdesc, vs_convert, ps_copy, name);
	}

	pdesc.colorAttachments[0].blendingEnabled = YES;
	pdesc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
	pdesc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
	pdesc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
	for (size_t i = 0; i < std::size(m_merge_pipeline); i++)
	{
		bool mmod = i & 1;
		bool amod = i & 2;
		NSString* name = [NSString stringWithFormat:@"ps_merge%zu", mmod];
		NSString* pipename = [NSString stringWithFormat:@"Merge%s%s", mmod ? " MMOD" : "", amod ? " AMOD" : ""];
		pdesc.colorAttachments[0].writeMask = amod ? MTLColorWriteMaskRed | MTLColorWriteMaskGreen | MTLColorWriteMaskBlue : MTLColorWriteMaskAll;
		m_merge_pipeline[i] = MakePipeline(pdesc, vs_convert, LoadShader(name), pipename);
	}
	pdesc.colorAttachments[0].writeMask = MTLColorWriteMaskAll;

	applyAttribute(pdesc.vertexDescriptor, 0, MTLVertexFormatFloat2,           offsetof(ImDrawVert, pos), 0);
	applyAttribute(pdesc.vertexDescriptor, 1, MTLVertexFormatFloat2,           offsetof(ImDrawVert, uv),  0);
	applyAttribute(pdesc.vertexDescriptor, 2, MTLVertexFormatUChar4Normalized, offsetof(ImDrawVert, col), 0);
	pdesc.vertexDescriptor.layouts[0].stride = sizeof(ImDrawVert);
	pdesc.colorAttachments[0].pixelFormat = layer_px_fmt;
	m_imgui_pipeline = MakePipeline(pdesc, LoadShader(@"vs_imgui"), LoadShader(@"ps_imgui"), @"imgui");

	[initCommands commit];
	return true;
}}

void GSDeviceMTL::Destroy()
{ @autoreleasepool {
	FlushEncoders();
	std::lock_guard<std::mutex> guard(m_backref->first);
	m_backref->second = nullptr;

	GSDevice::Destroy();
	GSDeviceMTL::DestroySurface();
	m_queue = nullptr;
	m_dev.Reset();
}}

void GSDeviceMTL::DestroySurface()
{
	if (!m_layer)
		return;
	OnMainThread([this]{ DetachSurfaceOnMainThread(); });
	m_layer = nullptr;
}

bool GSDeviceMTL::UpdateWindow()
{
	DestroySurface();

	if (!AcquireWindow(false))
		return false;

	if (m_window_info.type == WindowInfo::Type::Surfaceless)
		return true;

	OnMainThread([this] { AttachSurfaceOnMainThread(); });
	return true;
}

bool GSDeviceMTL::SupportsExclusiveFullscreen() const { return false; }

std::string GSDeviceMTL::GetDriverInfo() const
{ @autoreleasepool {
	std::string desc([[m_dev.dev description] UTF8String]);
	desc += "\n    Texture Swizzle:   " + std::string(m_dev.features.texture_swizzle   ? "Supported" : "Unsupported");
	desc += "\n    Unified Memory:    " + std::string(m_dev.features.unified_memory    ? "Supported" : "Unsupported");
	desc += "\n    Framebuffer Fetch: " + std::string(m_dev.features.framebuffer_fetch ? "Supported" : "Unsupported");
	desc += "\n    Primitive ID:      " + std::string(m_dev.features.primid            ? "Supported" : "Unsupported");
	desc += "\n    Shader Version:    " + std::string(to_string(m_dev.features.shader_version));
	desc += "\n    Max Texture Size:  " + std::to_string(m_dev.features.max_texsize);
	return desc;
}}

void GSDeviceMTL::ResizeWindow(s32 new_window_width, s32 new_window_height, float new_window_scale)
{
	m_window_info.surface_scale = new_window_scale;
	if (!m_layer ||
		(m_window_info.surface_width == static_cast<u32>(new_window_width) && m_window_info.surface_height == static_cast<u32>(new_window_height)))
	{
		return;
	}

	m_window_info.surface_width = new_window_width;
	m_window_info.surface_height = new_window_height;
	@autoreleasepool
	{
		[m_layer setDrawableSize:CGSizeMake(new_window_width, new_window_height)];
	}
}

void GSDeviceMTL::UpdateTexture(id<MTLTexture> texture, u32 x, u32 y, u32 width, u32 height, const void* data, u32 data_stride)
{
	id<MTLCommandBuffer> cmdbuf = [m_queue commandBuffer];
	id<MTLBlitCommandEncoder> enc = [cmdbuf blitCommandEncoder];
	size_t bytes = data_stride * height;
	MRCOwned<id<MTLBuffer>> buf = MRCTransfer([m_dev.dev newBufferWithLength:bytes options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined]);
	memcpy([buf contents], data, bytes);
	[enc copyFromBuffer:buf
	       sourceOffset:0
	  sourceBytesPerRow:data_stride
	sourceBytesPerImage:bytes
	         sourceSize:MTLSizeMake(width, height, 1)
	          toTexture:texture
	   destinationSlice:0
	   destinationLevel:0
	  destinationOrigin:MTLOriginMake(0, 0, 0)];
	[enc endEncoding];
	[cmdbuf commit];
}

static bool s_capture_next = false;

GSDevice::PresentResult GSDeviceMTL::BeginPresent(bool frame_skip)
{ @autoreleasepool {
	if (m_capture_start_frame && FrameNo() == m_capture_start_frame)
		s_capture_next = true;
	if (frame_skip || m_window_info.type == WindowInfo::Type::Surfaceless || !g_gs_device)
	{
		ImGui::EndFrame();
		return PresentResult::FrameSkipped;
	}
	id<MTLCommandBuffer> buf = GetRenderCmdBuf();
	m_current_drawable = MRCRetain([m_layer nextDrawable]);
	EndRenderPass();
	if (!m_current_drawable)
	{
		[buf pushDebugGroup:@"Present Skipped"];
		[buf popDebugGroup];
		FlushEncoders();
		ImGui::EndFrame();
		return PresentResult::FrameSkipped;
	}
	[m_pass_desc colorAttachments][0].texture = [m_current_drawable texture];
	id<MTLRenderCommandEncoder> enc = [buf renderCommandEncoderWithDescriptor:m_pass_desc];
	[enc setLabel:@"Present"];
	m_current_render.encoder = MRCRetain(enc);
	return PresentResult::OK;
}}

void GSDeviceMTL::EndPresent()
{ @autoreleasepool {
	pxAssertMsg(m_current_render.encoder && m_current_render_cmdbuf, "BeginPresent cmdbuf was destroyed");
	ImGui::Render();
	RenderImGui(ImGui::GetDrawData());
	EndRenderPass();
	if (m_current_drawable)
	{
		const bool use_present_drawable = m_use_present_drawable == UsePresentDrawable::Always ||
			(m_use_present_drawable == UsePresentDrawable::IfVsync && m_vsync_mode != VsyncMode::Off);

		if (use_present_drawable)
			[m_current_render_cmdbuf presentDrawable:m_current_drawable];
		else
			[m_current_render_cmdbuf addScheduledHandler:[drawable = std::move(m_current_drawable)](id<MTLCommandBuffer>){
				[drawable present];
			}];
	}
	FlushEncoders();
	FrameCompleted();
	m_current_drawable = nullptr;
	if (m_capture_start_frame)
	{
		if (@available(macOS 10.15, iOS 13, *))
		{
			static NSString* const path = @"/tmp/PCSX2MTLCapture.gputrace";
			static u32 frames;
			if (frames)
			{
				--frames;
				if (!frames)
				{
					[[MTLCaptureManager sharedCaptureManager] stopCapture];
					Console.WriteLn("Metal Trace Capture to /tmp/PCSX2MTLCapture.gputrace finished");
					[[NSWorkspace sharedWorkspace] selectFile:path
					                 inFileViewerRootedAtPath:@"/tmp/"];
				}
			}
			else if (s_capture_next)
			{
				s_capture_next = false;
				MTLCaptureManager* mgr = [MTLCaptureManager sharedCaptureManager];
				if ([mgr supportsDestination:MTLCaptureDestinationGPUTraceDocument])
				{
					MTLCaptureDescriptor* desc = [[MTLCaptureDescriptor new] autorelease];
					[desc setCaptureObject:m_dev.dev];
					if ([[NSFileManager defaultManager] fileExistsAtPath:path])
						[[NSFileManager defaultManager] removeItemAtPath:path error:nil];
					[desc setOutputURL:[NSURL fileURLWithPath:path]];
					[desc setDestination:MTLCaptureDestinationGPUTraceDocument];
					NSError* err = nullptr;
					[mgr startCaptureWithDescriptor:desc error:&err];
					if (err)
					{
						Console.Error("Metal Trace Capture failed: %s", [[err localizedDescription] UTF8String]);
					}
					else
					{
						Console.WriteLn("Metal Trace Capture to /tmp/PCSX2MTLCapture.gputrace started");
						frames = 2;
					}
				}
				else
				{
					Console.Error("Metal Trace Capture Failed: MTLCaptureManager doesn't support GPU trace documents! (Did you forget to run with METAL_CAPTURE_ENABLED=1?)");
				}
			}
		}
	}
}}

void GSDeviceMTL::SetVSync(VsyncMode mode)
{
	if (m_vsync_mode == mode)
		return;

	[m_layer setDisplaySyncEnabled:mode != VsyncMode::Off];
	m_vsync_mode = mode;
}

bool GSDeviceMTL::GetHostRefreshRate(float* refresh_rate)
{
	OnMainThread([this, refresh_rate]
	{
		u32 did = [[[[[m_view window] screen] deviceDescription] valueForKey:@"NSScreenNumber"] unsignedIntValue];
		if (CGDisplayModeRef mode = CGDisplayCopyDisplayMode(did))
		{
			*refresh_rate = CGDisplayModeGetRefreshRate(mode);
			CGDisplayModeRelease(mode);
		}
		else
		{
			*refresh_rate = 0;
		}
	});
	return *refresh_rate != 0;
}

bool GSDeviceMTL::SetGPUTimingEnabled(bool enabled)
{
	if (enabled == m_gpu_timing_enabled)
		return true;
	if (@available(macOS 10.15, iOS 10.3, *))
	{
		std::lock_guard<std::mutex> l(m_mtx);
		m_gpu_timing_enabled = enabled;
		m_accumulated_gpu_time = 0;
		m_last_gpu_time_end = 0;
		return true;
	}
	return false;
}

float GSDeviceMTL::GetAndResetAccumulatedGPUTime()
{
	std::lock_guard<std::mutex> l(m_mtx);
	float time = m_accumulated_gpu_time * 1000;
	m_accumulated_gpu_time = 0;
	return time;
}

void GSDeviceMTL::AccumulateCommandBufferTime(id<MTLCommandBuffer> buffer)
{
	std::lock_guard<std::mutex> l(m_mtx);
	if (!m_gpu_timing_enabled)
		return;
	// We do the check before enabling m_gpu_timing_enabled
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"
	// It's unlikely, but command buffers can overlap or run out of order
	// This doesn't handle every case (fully out of order), but it should at least handle overlapping
	double begin = std::max(m_last_gpu_time_end, [buffer GPUStartTime]);
	double end = [buffer GPUEndTime];
	if (end > begin)
	{
		m_accumulated_gpu_time += end - begin;
		m_last_gpu_time_end = end;
	}
#pragma clang diagnostic pop
}

std::unique_ptr<GSDownloadTexture> GSDeviceMTL::CreateDownloadTexture(u32 width, u32 height, GSTexture::Format format)
{
	return GSDownloadTextureMTL::Create(this, width, height, format);
}

void GSDeviceMTL::ClearSamplerCache()
{ @autoreleasepool {
	std::fill(std::begin(m_sampler_hw), std::end(m_sampler_hw), nullptr);
	m_sampler_hw[SamplerSelector::Linear().key] = CreateSampler(m_dev.dev, SamplerSelector::Linear());
	m_sampler_hw[SamplerSelector::Point().key] = CreateSampler(m_dev.dev, SamplerSelector::Point());
}}

void GSDeviceMTL::CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r, u32 destX, u32 destY)
{ @autoreleasepool {
	g_perfmon.Put(GSPerfMon::TextureCopies, 1);

	GSTextureMTL* sT = static_cast<GSTextureMTL*>(sTex);
	GSTextureMTL* dT = static_cast<GSTextureMTL*>(dTex);

	// Process clears
	GSVector2i dsize = dTex->GetSize();
	if (r.width() < dsize.x || r.height() < dsize.y)
		dT->FlushClears();
	else
		dT->SetState(GSTexture::State::Dirty);

	EndRenderPass();

	sT->m_last_read  = m_current_draw;
	dT->m_last_write = m_current_draw;

	id<MTLCommandBuffer> cmdbuf = GetRenderCmdBuf();
	id<MTLBlitCommandEncoder> encoder = [cmdbuf blitCommandEncoder];
	[encoder setLabel:@"CopyRect"];
	[encoder copyFromTexture:sT->GetTexture()
	             sourceSlice:0
	             sourceLevel:0
	            sourceOrigin:MTLOriginMake(r.x, r.y, 0)
	              sourceSize:MTLSizeMake(r.width(), r.height(), 1)
	               toTexture:dT->GetTexture()
	        destinationSlice:0
	        destinationLevel:0
	       destinationOrigin:MTLOriginMake((int)destX, (int)destY, 0)];
	[encoder endEncoding];
}}

void GSDeviceMTL::BeginStretchRect(NSString* name, GSTexture* dTex, MTLLoadAction action)
{
	if (dTex->GetFormat() == GSTexture::Format::DepthStencil)
		BeginRenderPass(name, nullptr, MTLLoadActionDontCare, dTex, action);
	else
		BeginRenderPass(name, dTex, action, nullptr, MTLLoadActionDontCare);

	FlushDebugEntries(m_current_render.encoder);
	MREClearScissor();
	DepthStencilSelector dsel = DepthStencilSelector::NoDepth();
	dsel.zwe = dTex->GetFormat() == GSTexture::Format::DepthStencil;
	MRESetDSS(dsel);
}

void GSDeviceMTL::DoStretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, id<MTLRenderPipelineState> pipeline, bool linear, LoadAction load_action, const void* frag_uniform, size_t frag_uniform_len)
{
	FlushClears(sTex);

	GSVector2i ds = dTex->GetSize();

	bool covers_target = static_cast<int>(dRect.x) <= 0
	                  && static_cast<int>(dRect.y) <= 0
	                  && static_cast<int>(dRect.z) >= ds.x
	                  && static_cast<int>(dRect.w) >= ds.y;
	bool dontcare = load_action == LoadAction::DontCare || (load_action == LoadAction::DontCareIfFull && covers_target);
	MTLLoadAction action = dontcare ? MTLLoadActionDontCare : MTLLoadActionLoad;

	BeginStretchRect(@"StretchRect", dTex, action);

	MRESetPipeline(pipeline);
	MRESetTexture(sTex, GSMTLTextureIndexNonHW);

	if (frag_uniform && frag_uniform_len)
		[m_current_render.encoder setFragmentBytes:frag_uniform length:frag_uniform_len atIndex:GSMTLBufferIndexUniforms];

	MRESetSampler(linear ? SamplerSelector::Linear() : SamplerSelector::Point());

	DrawStretchRect(sRect, dRect, GSVector2(static_cast<float>(ds.x), static_cast<float>(ds.y)));
}

static std::array<GSVector4, 4> CalcStrechRectPoints(const GSVector4& sRect, const GSVector4& dRect, const GSVector2& ds)
{
	static_assert(sizeof(GSDeviceMTL::ConvertShaderVertex) == sizeof(GSVector4), "Using GSVector4 as a ConvertShaderVertex");
	GSVector4 dst = dRect;
	dst /= GSVector4(ds.x, ds.y, ds.x, ds.y);
	dst *= GSVector4(2, -2, 2, -2);
	dst += GSVector4(-1, 1, -1, 1);
	return {
		dst.xyxy(sRect),
		dst.zyzy(sRect),
		dst.xwxw(sRect),
		dst.zwzw(sRect)
	};
}

void GSDeviceMTL::DrawStretchRect(const GSVector4& sRect, const GSVector4& dRect, const GSVector2& ds)
{
	std::array<GSVector4, 4> vertices = CalcStrechRectPoints(sRect, dRect, ds);

	[m_current_render.encoder setVertexBytes:&vertices length:sizeof(vertices) atIndex:GSMTLBufferIndexVertices];

	[m_current_render.encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
	                             vertexStart:0
	                             vertexCount:4];
	g_perfmon.Put(GSPerfMon::DrawCalls, 1);
}

void GSDeviceMTL::RenderCopy(GSTexture* sTex, id<MTLRenderPipelineState> pipeline, const GSVector4i& rect)
{
	// FS Triangle encoder uses vertex ID alone to make a FS triangle, which we then scissor to the desired rectangle
	MRESetScissor(rect);
	MRESetPipeline(pipeline);
	MRESetTexture(sTex, GSMTLTextureIndexNonHW);
	[m_current_render.encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
	g_perfmon.Put(GSPerfMon::DrawCalls, 1);
}

void GSDeviceMTL::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ShaderConvert shader, bool linear)
{ @autoreleasepool {

	pxAssert(linear ? SupportsBilinear(shader) : SupportsNearest(shader));

	id<MTLRenderPipelineState> pipeline = m_convert_pipeline[static_cast<int>(shader)];
	pxAssertRel(pipeline, fmt::format("No pipeline for {}", shaderName(shader)).c_str());

	DoStretchRect(sTex, sRect, dTex, dRect, pipeline, linear, LoadAction::DontCareIfFull, nullptr, 0);
}}

void GSDeviceMTL::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, bool red, bool green, bool blue, bool alpha)
{ @autoreleasepool {
	int sel = 0;
	if (red)   sel |= 1;
	if (green) sel |= 2;
	if (blue)  sel |= 4;
	if (alpha) sel |= 8;

	id<MTLRenderPipelineState> pipeline = m_convert_pipeline_copy_mask[sel];

	DoStretchRect(sTex, sRect, dTex, dRect, pipeline, false, sel == 15 ? LoadAction::DontCareIfFull : LoadAction::Load, nullptr, 0);
}}

static_assert(sizeof(DisplayConstantBuffer) == sizeof(GSMTLPresentPSUniform));
static_assert(offsetof(DisplayConstantBuffer, SourceRect)          == offsetof(GSMTLPresentPSUniform, source_rect));
static_assert(offsetof(DisplayConstantBuffer, TargetRect)          == offsetof(GSMTLPresentPSUniform, target_rect));
static_assert(offsetof(DisplayConstantBuffer, TargetSize)          == offsetof(GSMTLPresentPSUniform, target_size));
static_assert(offsetof(DisplayConstantBuffer, TargetResolution)    == offsetof(GSMTLPresentPSUniform, target_resolution));
static_assert(offsetof(DisplayConstantBuffer, RcpTargetResolution) == offsetof(GSMTLPresentPSUniform, rcp_target_resolution));
static_assert(offsetof(DisplayConstantBuffer, SourceResolution)    == offsetof(GSMTLPresentPSUniform, source_resolution));
static_assert(offsetof(DisplayConstantBuffer, RcpSourceResolution) == offsetof(GSMTLPresentPSUniform, rcp_source_resolution));
static_assert(offsetof(DisplayConstantBuffer, TimeAndPad.x)        == offsetof(GSMTLPresentPSUniform, time));

void GSDeviceMTL::PresentRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, PresentShader shader, float shaderTime, bool linear)
{ @autoreleasepool {
	GSVector2i ds = dTex ? dTex->GetSize() : GetWindowSize();
	DisplayConstantBuffer cb;
	cb.SetSource(sRect, sTex->GetSize());
	cb.SetTarget(dRect, ds);
	cb.SetTime(shaderTime);
	id<MTLRenderPipelineState> pipe = m_present_pipeline[static_cast<int>(shader)];

	if (dTex)
	{
		DoStretchRect(sTex, sRect, dTex, dRect, pipe, linear, LoadAction::DontCareIfFull, &cb, sizeof(cb));
	}
	else
	{
		// !dTex → Use current draw encoder
		[m_current_render.encoder setRenderPipelineState:pipe];
		[m_current_render.encoder setFragmentSamplerState:m_sampler_hw[linear ? SamplerSelector::Linear().key : SamplerSelector::Point().key] atIndex:0];
		[m_current_render.encoder setFragmentTexture:static_cast<GSTextureMTL*>(sTex)->GetTexture() atIndex:0];
		[m_current_render.encoder setFragmentBytes:&cb length:sizeof(cb) atIndex:GSMTLBufferIndexUniforms];
		DrawStretchRect(sRect, dRect, GSVector2(static_cast<float>(ds.x), static_cast<float>(ds.y)));
	}
}}

void GSDeviceMTL::DrawMultiStretchRects(const MultiStretchRect* rects, u32 num_rects, GSTexture* dTex, ShaderConvert shader)
{ @autoreleasepool {
	BeginStretchRect(@"MultiStretchRect", dTex, MTLLoadActionLoad);

	id<MTLRenderPipelineState> pipeline = nullptr;
	GSTexture* sTex = rects[0].src;
	bool linear = rects[0].linear;
	u8 wmask = rects[0].wmask.wrgba;

	const GSVector2 ds(static_cast<float>(dTex->GetWidth()), static_cast<float>(dTex->GetHeight()));
	const Map allocation = Allocate(m_vertex_upload_buf, sizeof(ConvertShaderVertex) * 4 * num_rects);
	std::array<GSVector4, 4>* write = static_cast<std::array<GSVector4, 4>*>(allocation.cpu_buffer);
	const id<MTLRenderCommandEncoder> enc = m_current_render.encoder;
	[enc setVertexBuffer:allocation.gpu_buffer
	              offset:allocation.gpu_offset
	             atIndex:GSMTLBufferIndexVertices];
	u32 start = 0;

	auto flush = [&](u32 i) {
		const u32 end = i * 4;
		const u32 vertex_count = end - start;
		const u32 index_count = vertex_count + (vertex_count >> 1); // 6 indices per 4 vertices
		id<MTLRenderPipelineState> new_pipeline = wmask == 0xf ? m_convert_pipeline[static_cast<int>(shader)]
		                                                       : m_convert_pipeline_copy_mask[wmask];
		if (new_pipeline != pipeline)
		{
			pipeline = new_pipeline;
			pxAssertRel(pipeline, fmt::format("No pipeline for {}", shaderName(shader)).c_str());
			MRESetPipeline(pipeline);
		}
		MRESetSampler(linear ? SamplerSelector::Linear() : SamplerSelector::Point());
		MRESetTexture(sTex, GSMTLTextureIndexNonHW);
		[enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle
		                indexCount:index_count
		                 indexType:MTLIndexTypeUInt16
		               indexBuffer:m_expand_index_buffer
		         indexBufferOffset:0
		             instanceCount:1
		                baseVertex:start
		              baseInstance:0];
		start = end;
	};

	for (u32 i = 0; i < num_rects; i++)
	{
		const MultiStretchRect& rect = rects[i];
		if (rect.src != sTex || rect.linear != linear || rect.wmask.wrgba != wmask)
		{
			flush(i);
			sTex = rect.src;
			linear = rect.linear;
			wmask = rect.wmask.wrgba;
		}
		*write++ = CalcStrechRectPoints(rect.src_rect, rect.dst_rect, ds);
	}

	flush(num_rects);
}}

void GSDeviceMTL::UpdateCLUTTexture(GSTexture* sTex, float sScale, u32 offsetX, u32 offsetY, GSTexture* dTex, u32 dOffset, u32 dSize)
{
	GSMTLCLUTConvertPSUniform uniform = { sScale, {offsetX, offsetY}, dOffset };

	const bool is_clut4 = dSize == 16;
	const GSVector4i dRect(0, 0, dSize, 1);

	BeginRenderPass(@"CLUT Update", dTex, MTLLoadActionDontCare, nullptr, MTLLoadActionDontCare);
	[m_current_render.encoder setFragmentBytes:&uniform length:sizeof(uniform) atIndex:GSMTLBufferIndexUniforms];
	RenderCopy(sTex, m_clut_pipeline[!is_clut4], dRect);
}

void GSDeviceMTL::ConvertToIndexedTexture(GSTexture* sTex, float sScale, u32 offsetX, u32 offsetY, u32 SBW, u32 SPSM, GSTexture* dTex, u32 DBW, u32 DPSM)
{ @autoreleasepool {
	const ShaderConvert shader = ShaderConvert::RGBA_TO_8I;
	id<MTLRenderPipelineState> pipeline = m_convert_pipeline[static_cast<int>(shader)];
	if (!pipeline)
		[NSException raise:@"StretchRect Missing Pipeline" format:@"No pipeline for %d", static_cast<int>(shader)];

	GSMTLIndexedConvertPSUniform uniform = { sScale, SBW, DBW };

	const GSVector4 dRect(0, 0, dTex->GetWidth(), dTex->GetHeight());
	DoStretchRect(sTex, GSVector4::zero(), dTex, dRect, pipeline, false, LoadAction::DontCareIfFull, &uniform, sizeof(uniform));
}}

void GSDeviceMTL::FlushClears(GSTexture* tex)
{
	if (tex)
		static_cast<GSTextureMTL*>(tex)->FlushClears();
}

// MARK: - MainRenderEncoder Operations

static MTLBlendFactor ConvertBlendFactor(GSDevice::BlendFactor generic)
{
	switch (generic)
	{
		case GSDevice::SRC_COLOR:       return MTLBlendFactorSourceColor;
		case GSDevice::INV_SRC_COLOR:   return MTLBlendFactorOneMinusSourceColor;
		case GSDevice::DST_COLOR:       return MTLBlendFactorDestinationColor;
		case GSDevice::INV_DST_COLOR:   return MTLBlendFactorOneMinusBlendColor;
		case GSDevice::SRC1_COLOR:      return MTLBlendFactorSource1Color;
		case GSDevice::INV_SRC1_COLOR:  return MTLBlendFactorOneMinusSource1Color;
		case GSDevice::SRC_ALPHA:       return MTLBlendFactorSourceAlpha;
		case GSDevice::INV_SRC_ALPHA:   return MTLBlendFactorOneMinusSourceAlpha;
		case GSDevice::DST_ALPHA:       return MTLBlendFactorDestinationAlpha;
		case GSDevice::INV_DST_ALPHA:   return MTLBlendFactorOneMinusDestinationAlpha;
		case GSDevice::SRC1_ALPHA:      return MTLBlendFactorSource1Alpha;
		case GSDevice::INV_SRC1_ALPHA:  return MTLBlendFactorOneMinusSource1Alpha;
		case GSDevice::CONST_COLOR:     return MTLBlendFactorBlendColor;
		case GSDevice::INV_CONST_COLOR: return MTLBlendFactorOneMinusBlendColor;
		case GSDevice::CONST_ONE:       return MTLBlendFactorOne;
		case GSDevice::CONST_ZERO:      return MTLBlendFactorZero;
	}
}

static MTLBlendOperation ConvertBlendOp(GSDevice::BlendOp generic)
{
	switch (generic)
	{
		case GSDevice::OP_ADD:          return MTLBlendOperationAdd;
		case GSDevice::OP_SUBTRACT:     return MTLBlendOperationSubtract;
		case GSDevice::OP_REV_SUBTRACT: return MTLBlendOperationReverseSubtract;
	}
}

static constexpr MTLColorWriteMask MTLColorWriteMaskRGB = MTLColorWriteMaskRed | MTLColorWriteMaskGreen | MTLColorWriteMaskBlue;

static GSMTLExpandType ConvertVSExpand(GSHWDrawConfig::VSExpand generic)
{
	switch (generic)
	{
		case GSHWDrawConfig::VSExpand::None:   return GSMTLExpandType::None;
		case GSHWDrawConfig::VSExpand::Point:  return GSMTLExpandType::Point;
		case GSHWDrawConfig::VSExpand::Line:   return GSMTLExpandType::Line;
		case GSHWDrawConfig::VSExpand::Sprite: return GSMTLExpandType::Sprite;
	}
}

void GSDeviceMTL::MRESetHWPipelineState(GSHWDrawConfig::VSSelector vssel, GSHWDrawConfig::PSSelector pssel, GSHWDrawConfig::BlendState blend, GSHWDrawConfig::ColorMaskSelector cms)
{
	PipelineSelectorExtrasMTL extras(blend, m_current_render.color_target, cms, m_current_render.depth_target, m_current_render.stencil_target);
	PipelineSelectorMTL fullsel(vssel, pssel, extras);
	if (m_current_render.has.pipeline_sel && fullsel == m_current_render.pipeline_sel)
		return;
	m_current_render.pipeline_sel = fullsel;
	m_current_render.has.pipeline_sel = true;
	auto idx = m_hw_pipeline.find(fullsel);
	if (idx != m_hw_pipeline.end())
	{
		[m_current_render.encoder setRenderPipelineState:idx->second];
		return;
	}

	bool primid_tracking_init = pssel.date == 1 || pssel.date == 2;

	VSSelector vssel_mtl;
	vssel_mtl.fst = vssel.fst;
	vssel_mtl.iip = vssel.iip;
	vssel_mtl.point_size = vssel.point_size;
	vssel_mtl.expand = ConvertVSExpand(vssel.expand);
	id<MTLFunction> vs = m_hw_vs[vssel_mtl.key];

	id<MTLFunction> ps;
	auto idx2 = m_hw_ps.find(pssel);
	if (idx2 != m_hw_ps.end())
	{
		ps = idx2->second;
	}
	else
	{
		setFnConstantB(m_fn_constants, pssel.fst,                GSMTLConstantIndex_FST);
		setFnConstantB(m_fn_constants, pssel.iip,                GSMTLConstantIndex_IIP);
		setFnConstantI(m_fn_constants, pssel.aem_fmt,            GSMTLConstantIndex_PS_AEM_FMT);
		setFnConstantI(m_fn_constants, pssel.pal_fmt,            GSMTLConstantIndex_PS_PAL_FMT);
		setFnConstantI(m_fn_constants, pssel.dst_fmt,            GSMTLConstantIndex_PS_DST_FMT);
		setFnConstantI(m_fn_constants, pssel.depth_fmt,          GSMTLConstantIndex_PS_DEPTH_FMT);
		setFnConstantB(m_fn_constants, pssel.aem,                GSMTLConstantIndex_PS_AEM);
		setFnConstantB(m_fn_constants, pssel.fba,                GSMTLConstantIndex_PS_FBA);
		setFnConstantB(m_fn_constants, pssel.fog,                GSMTLConstantIndex_PS_FOG);
		setFnConstantI(m_fn_constants, pssel.date,               GSMTLConstantIndex_PS_DATE);
		setFnConstantI(m_fn_constants, pssel.atst,               GSMTLConstantIndex_PS_ATST);
		setFnConstantI(m_fn_constants, pssel.tfx,                GSMTLConstantIndex_PS_TFX);
		setFnConstantB(m_fn_constants, pssel.tcc,                GSMTLConstantIndex_PS_TCC);
		setFnConstantI(m_fn_constants, pssel.wms,                GSMTLConstantIndex_PS_WMS);
		setFnConstantI(m_fn_constants, pssel.wmt,                GSMTLConstantIndex_PS_WMT);
		setFnConstantB(m_fn_constants, pssel.adjs,               GSMTLConstantIndex_PS_ADJS);
		setFnConstantB(m_fn_constants, pssel.adjt,               GSMTLConstantIndex_PS_ADJT);
		setFnConstantB(m_fn_constants, pssel.ltf,                GSMTLConstantIndex_PS_LTF);
		setFnConstantB(m_fn_constants, pssel.shuffle,            GSMTLConstantIndex_PS_SHUFFLE);
		setFnConstantB(m_fn_constants, pssel.shuffle_same,       GSMTLConstantIndex_PS_SHUFFLE_SAME);
		setFnConstantB(m_fn_constants, pssel.read_ba,            GSMTLConstantIndex_PS_READ_BA);
		setFnConstantB(m_fn_constants, pssel.real16src,          GSMTLConstantIndex_PS_READ16_SRC);
		setFnConstantB(m_fn_constants, pssel.write_rg,           GSMTLConstantIndex_PS_WRITE_RG);
		setFnConstantB(m_fn_constants, pssel.fbmask,             GSMTLConstantIndex_PS_FBMASK);
		setFnConstantI(m_fn_constants, pssel.blend_a,            GSMTLConstantIndex_PS_BLEND_A);
		setFnConstantI(m_fn_constants, pssel.blend_b,            GSMTLConstantIndex_PS_BLEND_B);
		setFnConstantI(m_fn_constants, pssel.blend_c,            GSMTLConstantIndex_PS_BLEND_C);
		setFnConstantI(m_fn_constants, pssel.blend_d,            GSMTLConstantIndex_PS_BLEND_D);
		setFnConstantI(m_fn_constants, pssel.blend_hw,           GSMTLConstantIndex_PS_BLEND_HW);
		setFnConstantB(m_fn_constants, pssel.a_masked,           GSMTLConstantIndex_PS_A_MASKED);
		setFnConstantB(m_fn_constants, pssel.hdr,                GSMTLConstantIndex_PS_HDR);
		setFnConstantB(m_fn_constants, pssel.colclip,            GSMTLConstantIndex_PS_COLCLIP);
		setFnConstantI(m_fn_constants, pssel.blend_mix,          GSMTLConstantIndex_PS_BLEND_MIX);
		setFnConstantB(m_fn_constants, pssel.round_inv,          GSMTLConstantIndex_PS_ROUND_INV);
		setFnConstantB(m_fn_constants, pssel.fixed_one_a,        GSMTLConstantIndex_PS_FIXED_ONE_A);
		setFnConstantB(m_fn_constants, pssel.pabe,               GSMTLConstantIndex_PS_PABE);
		setFnConstantB(m_fn_constants, pssel.no_color,           GSMTLConstantIndex_PS_NO_COLOR);
		setFnConstantB(m_fn_constants, pssel.no_color1,          GSMTLConstantIndex_PS_NO_COLOR1);
		// no_ablend ignored for now (No Metal driver has had DSB so broken that it's needed to be disabled, though Intel's was pretty close)
		setFnConstantB(m_fn_constants, pssel.only_alpha,         GSMTLConstantIndex_PS_ONLY_ALPHA);
		setFnConstantI(m_fn_constants, pssel.channel,            GSMTLConstantIndex_PS_CHANNEL);
		setFnConstantI(m_fn_constants, pssel.dither,             GSMTLConstantIndex_PS_DITHER);
		setFnConstantB(m_fn_constants, pssel.zclamp,             GSMTLConstantIndex_PS_ZCLAMP);
		setFnConstantB(m_fn_constants, pssel.tcoffsethack,       GSMTLConstantIndex_PS_TCOFFSETHACK);
		setFnConstantB(m_fn_constants, pssel.urban_chaos_hle,    GSMTLConstantIndex_PS_URBAN_CHAOS_HLE);
		setFnConstantB(m_fn_constants, pssel.tales_of_abyss_hle, GSMTLConstantIndex_PS_TALES_OF_ABYSS_HLE);
		setFnConstantB(m_fn_constants, pssel.tex_is_fb,          GSMTLConstantIndex_PS_TEX_IS_FB);
		setFnConstantB(m_fn_constants, pssel.automatic_lod,      GSMTLConstantIndex_PS_AUTOMATIC_LOD);
		setFnConstantB(m_fn_constants, pssel.manual_lod,         GSMTLConstantIndex_PS_MANUAL_LOD);
		setFnConstantB(m_fn_constants, pssel.point_sampler,      GSMTLConstantIndex_PS_POINT_SAMPLER);
		setFnConstantB(m_fn_constants, pssel.region_rect,        GSMTLConstantIndex_PS_REGION_RECT);
		setFnConstantI(m_fn_constants, pssel.scanmsk,            GSMTLConstantIndex_PS_SCANMSK);
		auto newps = LoadShader(@"ps_main");
		ps = newps;
		m_hw_ps.insert(std::make_pair(pssel, std::move(newps)));
	}

	MRCOwned<MTLRenderPipelineDescriptor*> pdesc = MRCTransfer([MTLRenderPipelineDescriptor new]);
	if (vssel_mtl.point_size)
		[pdesc setInputPrimitiveTopology:MTLPrimitiveTopologyClassPoint];
	if (vssel_mtl.expand == GSMTLExpandType::None)
		[pdesc setVertexDescriptor:m_hw_vertex];
	else
		[pdesc setInputPrimitiveTopology:MTLPrimitiveTopologyClassTriangle];
	MTLRenderPipelineColorAttachmentDescriptor* color = [[pdesc colorAttachments] objectAtIndexedSubscript:0];
	color.pixelFormat = ConvertPixelFormat(extras.rt);
	[pdesc setDepthAttachmentPixelFormat:extras.has_depth ? MTLPixelFormatDepth32Float_Stencil8 : MTLPixelFormatInvalid];
	[pdesc setStencilAttachmentPixelFormat:extras.has_stencil ? MTLPixelFormatDepth32Float_Stencil8 : MTLPixelFormatInvalid];
	color.writeMask = extras.writemask;
	if (primid_tracking_init)
	{
		color.blendingEnabled = YES;
		color.rgbBlendOperation = MTLBlendOperationMin;
		color.sourceRGBBlendFactor = MTLBlendFactorOne;
		color.destinationRGBBlendFactor = MTLBlendFactorOne;
		color.writeMask = MTLColorWriteMaskRed;
	}
	else if (extras.blend_enable && (extras.writemask & MTLColorWriteMaskRGB))
	{
		color.blendingEnabled = YES;
		color.rgbBlendOperation = ConvertBlendOp(extras.blend_op);
		color.sourceRGBBlendFactor = ConvertBlendFactor(extras.src_factor);
		color.destinationRGBBlendFactor = ConvertBlendFactor(extras.dst_factor);
	}
	NSString* pname = [NSString stringWithFormat:@"HW Render %x.%x.%llx.%x", vssel_mtl.key, pssel.key_hi, pssel.key_lo, extras.fullkey()];
	auto pipeline = MakePipeline(pdesc, vs, ps, pname);

	[m_current_render.encoder setRenderPipelineState:pipeline];
	m_hw_pipeline.insert(std::make_pair(fullsel, std::move(pipeline)));
}

void GSDeviceMTL::MRESetDSS(DepthStencilSelector sel)
{
	if (!m_current_render.depth_target || m_current_render.depth_sel.key == sel.key)
		return;
	[m_current_render.encoder setDepthStencilState:m_dss_hw[sel.key]];
	m_current_render.depth_sel = sel;
}

void GSDeviceMTL::MRESetDSS(id<MTLDepthStencilState> dss)
{
	[m_current_render.encoder setDepthStencilState:dss];
	m_current_render.depth_sel.key = -1;
}

void GSDeviceMTL::MRESetSampler(SamplerSelector sel)
{
	if (m_current_render.has.sampler && m_current_render.sampler_sel.key == sel.key)
		return;
	if (!m_sampler_hw[sel.key]) [[unlikely]]
		m_sampler_hw[sel.key] = CreateSampler(m_dev.dev, sel);
	[m_current_render.encoder setFragmentSamplerState:m_sampler_hw[sel.key] atIndex:0];
	m_current_render.sampler_sel = sel;
	m_current_render.has.sampler = true;
}

static void textureBarrier(id<MTLRenderCommandEncoder> enc)
{
	[enc memoryBarrierWithScope:MTLBarrierScopeRenderTargets
	                afterStages:MTLRenderStageFragment
	               beforeStages:MTLRenderStageFragment];
}

void GSDeviceMTL::MRESetTexture(GSTexture* tex, int pos)
{
	if (tex == m_current_render.tex[pos])
		return;
	m_current_render.tex[pos] = tex;
	if (GSTextureMTL* mtex = static_cast<GSTextureMTL*>(tex))
	{
		[m_current_render.encoder setFragmentTexture:mtex->GetTexture() atIndex:pos];
		mtex->m_last_read = m_current_draw;
	}
}

void GSDeviceMTL::MRESetVertices(id<MTLBuffer> buffer, size_t offset)
{
	if (m_current_render.vertex_buffer != (__bridge void*)buffer)
	{
		m_current_render.vertex_buffer = (__bridge void*)buffer;
		[m_current_render.encoder setVertexBuffer:buffer offset:offset atIndex:GSMTLBufferIndexHWVertices];
	}
	else
	{
		[m_current_render.encoder setVertexBufferOffset:offset atIndex:GSMTLBufferIndexHWVertices];
	}
}

void GSDeviceMTL::MRESetScissor(const GSVector4i& scissor)
{
	if (m_current_render.has.scissor && (m_current_render.scissor == scissor).alltrue())
		return;
	MTLScissorRect r;
	r.x = scissor.x;
	r.y = scissor.y;
	r.width = scissor.width();
	r.height = scissor.height();
	[m_current_render.encoder setScissorRect:r];
	m_current_render.scissor = scissor;
	m_current_render.has.scissor = true;
}

void GSDeviceMTL::MREClearScissor()
{
	if (!m_current_render.has.scissor)
		return;
	m_current_render.has.scissor = false;
	GSVector4i size = GSVector4i(0);
	if (m_current_render.color_target)   size = size.max_u32(GSVector4i(m_current_render.color_target  ->GetSize()));
	if (m_current_render.depth_target)   size = size.max_u32(GSVector4i(m_current_render.depth_target  ->GetSize()));
	if (m_current_render.stencil_target) size = size.max_u32(GSVector4i(m_current_render.stencil_target->GetSize()));
	MTLScissorRect r;
	r.x = 0;
	r.y = 0;
	r.width = size.x;
	r.height = size.y;
	[m_current_render.encoder setScissorRect:r];
}

void GSDeviceMTL::MRESetCB(const GSHWDrawConfig::VSConstantBuffer& cb)
{
	if (m_current_render.has.cb_vs && m_current_render.cb_vs == cb)
		return;
	[m_current_render.encoder setVertexBytes:&cb length:sizeof(cb) atIndex:GSMTLBufferIndexHWUniforms];
	m_current_render.has.cb_vs = true;
	m_current_render.cb_vs = cb;
}

void GSDeviceMTL::MRESetCB(const GSHWDrawConfig::PSConstantBuffer& cb)
{
	if (m_current_render.has.cb_ps && m_current_render.cb_ps == cb)
		return;
	[m_current_render.encoder setFragmentBytes:&cb length:sizeof(cb) atIndex:GSMTLBufferIndexHWUniforms];
	m_current_render.has.cb_ps = true;
	m_current_render.cb_ps = cb;
}

void GSDeviceMTL::MRESetBlendColor(u8 color)
{
	if (m_current_render.has.blend_color && m_current_render.blend_color == color)
		return;
	float fc = static_cast<float>(color) / 128.f;
	[m_current_render.encoder setBlendColorRed:fc green:fc blue:fc alpha:fc];
	m_current_render.has.blend_color = true;
	m_current_render.blend_color = color;
}

void GSDeviceMTL::MRESetPipeline(id<MTLRenderPipelineState> pipe)
{
	[m_current_render.encoder setRenderPipelineState:pipe];
	m_current_render.has.pipeline_sel = false;
}

// MARK: - HW Render

// Metal can't import GSDevice.h, but we should make sure the structs are at least compatible
static_assert(sizeof(GSVertex) == sizeof(GSMTLMainVertex));
static_assert(offsetof(GSVertex, ST)      == offsetof(GSMTLMainVertex, st));
static_assert(offsetof(GSVertex, RGBAQ.R) == offsetof(GSMTLMainVertex, rgba));
static_assert(offsetof(GSVertex, RGBAQ.Q) == offsetof(GSMTLMainVertex, q));
static_assert(offsetof(GSVertex, XYZ.X)   == offsetof(GSMTLMainVertex, xy));
static_assert(offsetof(GSVertex, XYZ.Z)   == offsetof(GSMTLMainVertex, z));
static_assert(offsetof(GSVertex, UV)      == offsetof(GSMTLMainVertex, uv));
static_assert(offsetof(GSVertex, FOG)     == offsetof(GSMTLMainVertex, fog));

static_assert(sizeof(GSHWDrawConfig::VSConstantBuffer) == sizeof(GSMTLMainVSUniform));
static_assert(sizeof(GSHWDrawConfig::PSConstantBuffer) == sizeof(GSMTLMainPSUniform));
static_assert(offsetof(GSHWDrawConfig::VSConstantBuffer, vertex_scale)     == offsetof(GSMTLMainVSUniform, vertex_scale));
static_assert(offsetof(GSHWDrawConfig::VSConstantBuffer, vertex_offset)    == offsetof(GSMTLMainVSUniform, vertex_offset));
static_assert(offsetof(GSHWDrawConfig::VSConstantBuffer, texture_scale)    == offsetof(GSMTLMainVSUniform, texture_scale));
static_assert(offsetof(GSHWDrawConfig::VSConstantBuffer, texture_offset)   == offsetof(GSMTLMainVSUniform, texture_offset));
static_assert(offsetof(GSHWDrawConfig::VSConstantBuffer, point_size)       == offsetof(GSMTLMainVSUniform, point_size));
static_assert(offsetof(GSHWDrawConfig::VSConstantBuffer, max_depth)        == offsetof(GSMTLMainVSUniform, max_depth));
static_assert(offsetof(GSHWDrawConfig::PSConstantBuffer, FogColor_AREF.x)  == offsetof(GSMTLMainPSUniform, fog_color));
static_assert(offsetof(GSHWDrawConfig::PSConstantBuffer, FogColor_AREF.a)  == offsetof(GSMTLMainPSUniform, aref));
static_assert(offsetof(GSHWDrawConfig::PSConstantBuffer, WH)               == offsetof(GSMTLMainPSUniform, wh));
static_assert(offsetof(GSHWDrawConfig::PSConstantBuffer, TA_MaxDepth_Af.x) == offsetof(GSMTLMainPSUniform, ta));
static_assert(offsetof(GSHWDrawConfig::PSConstantBuffer, TA_MaxDepth_Af.z) == offsetof(GSMTLMainPSUniform, max_depth));
static_assert(offsetof(GSHWDrawConfig::PSConstantBuffer, TA_MaxDepth_Af.w) == offsetof(GSMTLMainPSUniform, alpha_fix));
static_assert(offsetof(GSHWDrawConfig::PSConstantBuffer, FbMask)           == offsetof(GSMTLMainPSUniform, fbmask));
static_assert(offsetof(GSHWDrawConfig::PSConstantBuffer, HalfTexel)        == offsetof(GSMTLMainPSUniform, half_texel));
static_assert(offsetof(GSHWDrawConfig::PSConstantBuffer, MinMax)           == offsetof(GSMTLMainPSUniform, uv_min_max));
static_assert(offsetof(GSHWDrawConfig::PSConstantBuffer, STRange)          == offsetof(GSMTLMainPSUniform, st_range));
static_assert(offsetof(GSHWDrawConfig::PSConstantBuffer, ChannelShuffle)   == offsetof(GSMTLMainPSUniform, channel_shuffle));
static_assert(offsetof(GSHWDrawConfig::PSConstantBuffer, TCOffsetHack)     == offsetof(GSMTLMainPSUniform, tc_offset));
static_assert(offsetof(GSHWDrawConfig::PSConstantBuffer, STScale)          == offsetof(GSMTLMainPSUniform, st_scale));
static_assert(offsetof(GSHWDrawConfig::PSConstantBuffer, DitherMatrix)     == offsetof(GSMTLMainPSUniform, dither_matrix));
static_assert(offsetof(GSHWDrawConfig::PSConstantBuffer, ScaleFactor)      == offsetof(GSMTLMainPSUniform, scale_factor));

void GSDeviceMTL::SetupDestinationAlpha(GSTexture* rt, GSTexture* ds, const GSVector4i& r, bool datm)
{
	FlushClears(rt);
	BeginRenderPass(@"Destination Alpha Setup", nullptr, MTLLoadActionDontCare, nullptr, MTLLoadActionDontCare, ds, MTLLoadActionDontCare);
	[m_current_render.encoder setStencilReferenceValue:1];
	MRESetDSS(m_dss_stencil_zero);
	RenderCopy(nullptr, m_stencil_clear_pipeline, r);
	MRESetDSS(m_dss_stencil_write);
	RenderCopy(rt, m_datm_pipeline[datm], r);
}

static id<MTLTexture> getTexture(GSTexture* tex)
{
	return tex ? static_cast<GSTextureMTL*>(tex)->GetTexture() : nil;
}

static bool usesStencil(GSHWDrawConfig::DestinationAlphaMode dstalpha)
{
	switch (dstalpha)
	{
		case GSHWDrawConfig::DestinationAlphaMode::Stencil:
		case GSHWDrawConfig::DestinationAlphaMode::StencilOne:
			return true;
		default:
			return false;
	}
}

void GSDeviceMTL::MREInitHWDraw(GSHWDrawConfig& config, const Map& verts)
{
	MRESetScissor(config.scissor);
	MRESetTexture(config.tex, GSMTLTextureIndexTex);
	MRESetTexture(config.pal, GSMTLTextureIndexPalette);
	MRESetSampler(config.sampler);
	MRESetCB(config.cb_vs);
	MRESetCB(config.cb_ps);
	MRESetVertices(verts.gpu_buffer, verts.gpu_offset);
}

void GSDeviceMTL::RenderHW(GSHWDrawConfig& config)
{ @autoreleasepool {
	if (config.tex && config.ds == config.tex)
		EndRenderPass(); // Barrier

	size_t vertsize = config.nverts * sizeof(*config.verts);
	size_t idxsize = config.vs.UseExpandIndexBuffer() ? 0 : (config.nindices * sizeof(*config.indices));
	Map allocation = Allocate(m_vertex_upload_buf, vertsize + idxsize);
	memcpy(allocation.cpu_buffer, config.verts, vertsize);

	id<MTLBuffer> index_buffer;
	size_t index_buffer_offset;
	if (!config.vs.UseExpandIndexBuffer())
	{
		memcpy(static_cast<u8*>(allocation.cpu_buffer) + vertsize, config.indices, idxsize);
		index_buffer = allocation.gpu_buffer;
		index_buffer_offset = allocation.gpu_offset + vertsize;
	}
	else
	{
		index_buffer = m_expand_index_buffer;
		index_buffer_offset = 0;
	}

	FlushClears(config.tex);
	FlushClears(config.pal);

	GSTexture* stencil = nullptr;
	GSTexture* primid_tex = nullptr;
	GSTexture* rt = config.rt;
	switch (config.destination_alpha)
	{
		case GSHWDrawConfig::DestinationAlphaMode::Off:
		case GSHWDrawConfig::DestinationAlphaMode::Full:
			break; // No setup
		case GSHWDrawConfig::DestinationAlphaMode::PrimIDTracking:
		{
			FlushClears(config.rt);
			GSVector2i size = config.rt->GetSize();
			primid_tex = CreateRenderTarget(size.x, size.y, GSTexture::Format::PrimID);
			DepthStencilSelector dsel = config.depth;
			dsel.zwe = 0;
			GSTexture* depth = dsel.key == DepthStencilSelector::NoDepth().key ? nullptr : config.ds;
			BeginRenderPass(@"PrimID Destination Alpha Init", primid_tex, MTLLoadActionDontCare, depth, MTLLoadActionLoad);
			RenderCopy(config.rt, m_primid_init_pipeline[static_cast<bool>(depth)][config.datm], config.drawarea);
			MRESetDSS(dsel);
			pxAssert(config.ps.date == 1 || config.ps.date == 2);
			if (config.ps.tex_is_fb)
				MRESetTexture(config.rt, GSMTLTextureIndexRenderTarget);
			config.require_one_barrier = false; // Ending render pass is our barrier
			pxAssert(config.require_full_barrier == false && config.drawlist == nullptr);
			MRESetHWPipelineState(config.vs, config.ps, {}, {});
			MREInitHWDraw(config, allocation);
			SendHWDraw(config, m_current_render.encoder, index_buffer, index_buffer_offset);
			config.ps.date = 3;
			break;
		}
		case GSHWDrawConfig::DestinationAlphaMode::StencilOne:
			BeginRenderPass(@"Destination Alpha Stencil Clear", nullptr, MTLLoadActionDontCare, nullptr, MTLLoadActionDontCare, config.ds, MTLLoadActionDontCare);
			[m_current_render.encoder setStencilReferenceValue:1];
			MRESetDSS(m_dss_stencil_write);
			RenderCopy(nullptr, m_stencil_clear_pipeline, config.drawarea);
			stencil = config.ds;
			break;
		case GSHWDrawConfig::DestinationAlphaMode::Stencil:
			SetupDestinationAlpha(config.rt, config.ds, config.drawarea, config.datm);
			stencil = config.ds;
			break;
	}

	GSTexture* hdr_rt = nullptr;
	if (config.ps.hdr)
	{
		GSVector2i size = config.rt->GetSize();
		hdr_rt = CreateRenderTarget(size.x, size.y, GSTexture::Format::HDRColor);
		BeginRenderPass(@"HDR Init", hdr_rt, MTLLoadActionDontCare, nullptr, MTLLoadActionDontCare);
		RenderCopy(config.rt, m_hdr_init_pipeline, config.drawarea);
		rt = hdr_rt;
		g_perfmon.Put(GSPerfMon::TextureCopies, 1);
	}

	// Try to reduce render pass restarts
	if (!config.ds && m_current_render.color_target == rt && stencil == m_current_render.stencil_target && m_current_render.depth_target != config.tex)
		config.ds = m_current_render.depth_target;
	if (!rt && config.ds == m_current_render.depth_target && m_current_render.color_target != config.tex)
		rt = m_current_render.color_target;
	if (!rt && !config.ds)
	{
		// If we were rendering depth-only and depth gets cleared by the above check, that turns into rendering nothing, which should be a no-op
		pxAssertMsg(0, "RenderHW was given a completely useless draw call!");
		[m_current_render.encoder insertDebugSignpost:@"Skipped no-color no-depth draw"];
		if (primid_tex)
			Recycle(primid_tex);
		return;
	}

	BeginRenderPass(@"RenderHW", rt, MTLLoadActionLoad, config.ds, MTLLoadActionLoad, stencil, MTLLoadActionLoad);
	id<MTLRenderCommandEncoder> mtlenc = m_current_render.encoder;
	FlushDebugEntries(mtlenc);
	if (usesStencil(config.destination_alpha))
		[mtlenc setStencilReferenceValue:1];
	MREInitHWDraw(config, allocation);
	if (config.require_one_barrier || config.require_full_barrier)
		MRESetTexture(config.rt, GSMTLTextureIndexRenderTarget);
	if (primid_tex)
		MRESetTexture(primid_tex, GSMTLTextureIndexPrimIDs);
	if (config.blend.constant_enable)
		MRESetBlendColor(config.blend.constant);
	MRESetHWPipelineState(config.vs, config.ps, config.blend, config.colormask);
	MRESetDSS(config.depth);

	SendHWDraw(config, mtlenc, index_buffer, index_buffer_offset);

	if (config.alpha_second_pass.enable)
	{
		if (config.alpha_second_pass.ps_aref != config.cb_ps.FogColor_AREF.a)
		{
			config.cb_ps.FogColor_AREF.a = config.alpha_second_pass.ps_aref;
			MRESetCB(config.cb_ps);
		}
		MRESetHWPipelineState(config.vs, config.alpha_second_pass.ps, config.blend, config.alpha_second_pass.colormask);
		MRESetDSS(config.alpha_second_pass.depth);
		SendHWDraw(config, mtlenc, index_buffer, index_buffer_offset);
	}

	if (hdr_rt)
	{
		BeginRenderPass(@"HDR Resolve", config.rt, MTLLoadActionLoad, nullptr, MTLLoadActionDontCare);
		RenderCopy(hdr_rt, m_hdr_resolve_pipeline, config.drawarea);
		g_perfmon.Put(GSPerfMon::TextureCopies, 1);

		Recycle(hdr_rt);
	}

	if (primid_tex)
		Recycle(primid_tex);
}}

void GSDeviceMTL::SendHWDraw(GSHWDrawConfig& config, id<MTLRenderCommandEncoder> enc, id<MTLBuffer> buffer, size_t off)
{
	MTLPrimitiveType topology;
	switch (config.topology)
	{
		case GSHWDrawConfig::Topology::Point:    topology = MTLPrimitiveTypePoint;    break;
		case GSHWDrawConfig::Topology::Line:     topology = MTLPrimitiveTypeLine;     break;
		case GSHWDrawConfig::Topology::Triangle: topology = MTLPrimitiveTypeTriangle; break;
	}

	if (config.drawlist)
	{
		[enc pushDebugGroup:[NSString stringWithFormat:@"Full barrier split draw (%d sprites in %d groups)", config.nindices / config.indices_per_prim, config.drawlist->size()]];
#if defined(_DEBUG)
		// Check how draw call is split.
		std::map<size_t, size_t> frequency;
		for (const auto& it : *config.drawlist)
			++frequency[it];

		std::string message;
		for (const auto& it : frequency)
			message += " " + std::to_string(it.first) + "(" + std::to_string(it.second) + ")";

		[enc insertDebugSignpost:[NSString stringWithFormat:@"Split single draw (%d sprites) into %zu draws: consecutive draws(frequency):%s",
			config.nindices / config.indices_per_prim, config.drawlist->size(), message.c_str()]];
#endif


		g_perfmon.Put(GSPerfMon::DrawCalls, config.drawlist->size());
		g_perfmon.Put(GSPerfMon::Barriers, config.drawlist->size());

		const u32 indices_per_prim = config.indices_per_prim;
		const u32 draw_list_size = static_cast<u32>(config.drawlist->size());

		for (u32 n = 0, p = 0; n < draw_list_size; n++)
		{
			const u32 count = (*config.drawlist)[n] * indices_per_prim;
			textureBarrier(enc);
			[enc drawIndexedPrimitives:topology
			                indexCount:count
			                 indexType:MTLIndexTypeUInt16
			               indexBuffer:buffer
			         indexBufferOffset:off + p * sizeof(*config.indices)];
			p += count;
		}

		[enc popDebugGroup];
		return;
	}
	else if (config.require_full_barrier)
	{
		const u32 indices_per_prim = config.indices_per_prim;
		const u32 ndraws = config.nindices / indices_per_prim;
		g_perfmon.Put(GSPerfMon::DrawCalls, ndraws);
		g_perfmon.Put(GSPerfMon::Barriers, ndraws);
		[enc pushDebugGroup:[NSString stringWithFormat:@"Full barrier split draw (%d prims)", ndraws]];

		for (u32 p = 0; p < config.nindices; p += indices_per_prim)
		{
			textureBarrier(enc);
			[enc drawIndexedPrimitives:topology
			                indexCount:config.indices_per_prim
			                 indexType:MTLIndexTypeUInt16
			               indexBuffer:buffer
			         indexBufferOffset:off + p * sizeof(*config.indices)];
		}

		[enc popDebugGroup];
		return;
	}
	else if (config.require_one_barrier)
	{
		// One barrier needed
		textureBarrier(enc);
		g_perfmon.Put(GSPerfMon::Barriers, 1);
	}

	[enc drawIndexedPrimitives:topology
	                indexCount:config.nindices
	                 indexType:MTLIndexTypeUInt16
	               indexBuffer:buffer
	         indexBufferOffset:off];

	g_perfmon.Put(GSPerfMon::DrawCalls, 1);
}

// tbh I'm not a fan of the current debug groups
// not much useful information and makes things harder to find
// good to turn on if you're debugging tc stuff though
#ifndef MTL_ENABLE_DEBUG
	#define MTL_ENABLE_DEBUG 0
#endif

void GSDeviceMTL::PushDebugGroup(const char* fmt, ...)
{
#if MTL_ENABLE_DEBUG
	va_list va;
	va_start(va, fmt);
	MRCOwned<NSString*> nsfmt = MRCTransfer([[NSString alloc] initWithUTF8String:fmt]);
	m_debug_entries.emplace_back(DebugEntry::Push, MRCTransfer([[NSString alloc] initWithFormat:nsfmt arguments:va]));
	va_end(va);
#endif
}

void GSDeviceMTL::PopDebugGroup()
{
#if MTL_ENABLE_DEBUG
	m_debug_entries.emplace_back(DebugEntry::Pop, nullptr);
#endif
}

void GSDeviceMTL::InsertDebugMessage(DebugMessageCategory category, const char* fmt, ...)
{
#if MTL_ENABLE_DEBUG
	va_list va;
	va_start(va, fmt);
	MRCOwned<NSString*> nsfmt = MRCTransfer([[NSString alloc] initWithUTF8String:fmt]);
	m_debug_entries.emplace_back(DebugEntry::Insert, MRCTransfer([[NSString alloc] initWithFormat:nsfmt arguments:va]));
	va_end(va);
#endif
}

void GSDeviceMTL::ProcessDebugEntry(id<MTLCommandEncoder> enc, const DebugEntry& entry)
{
	switch (entry.op)
	{
		case DebugEntry::Push:
			[enc pushDebugGroup:entry.str];
			m_debug_group_level++;
			break;
		case DebugEntry::Pop:
			[enc popDebugGroup];
			if (m_debug_group_level > 0)
				m_debug_group_level--;
			break;
		case DebugEntry::Insert:
			[enc insertDebugSignpost:entry.str];
			break;
	}
}

void GSDeviceMTL::FlushDebugEntries(id<MTLCommandEncoder> enc)
{
#if MTL_ENABLE_DEBUG
	if (!m_debug_entries.empty())
	{
		for (const DebugEntry& entry : m_debug_entries)
		{
			ProcessDebugEntry(enc, entry);
		}
		m_debug_entries.clear();
	}
#endif
}

void GSDeviceMTL::EndDebugGroup(id<MTLCommandEncoder> enc)
{
#if MTL_ENABLE_DEBUG
	if (!m_debug_entries.empty() && m_debug_group_level)
	{
		auto begin = m_debug_entries.begin();
		auto cur = begin;
		auto end = m_debug_entries.end();
		while (cur != end && m_debug_group_level)
		{
			ProcessDebugEntry(enc, *cur);
			cur++;
		}
		m_debug_entries.erase(begin, cur);
	}
#endif
}

static simd::float2 ToSimd(const ImVec2& vec)
{
	return simd::make_float2(vec.x, vec.y);
}

static simd::float4 ToSimd(const ImVec4& vec)
{
	return simd::make_float4(vec.x, vec.y, vec.z, vec.w);
}

void GSDeviceMTL::RenderImGui(ImDrawData* data)
{
	if (data->CmdListsCount == 0)
		return;
	simd::float4 transform;
	transform.xy = 2.f / simd::make_float2(data->DisplaySize.x, -data->DisplaySize.y);
	transform.zw = ToSimd(data->DisplayPos) * -transform.xy + simd::make_float2(-1, 1);
	id<MTLRenderCommandEncoder> enc = m_current_render.encoder;
	[enc pushDebugGroup:@"ImGui"];

	Map map = Allocate(m_vertex_upload_buf, data->TotalVtxCount * sizeof(ImDrawVert) + data->TotalIdxCount * sizeof(ImDrawIdx));
	size_t vtx_off = 0;
	size_t idx_off = data->TotalVtxCount * sizeof(ImDrawVert);

	[enc setRenderPipelineState:m_imgui_pipeline];
	[enc setVertexBuffer:map.gpu_buffer offset:map.gpu_offset atIndex:GSMTLBufferIndexVertices];
	[enc setVertexBytes:&transform length:sizeof(transform) atIndex:GSMTLBufferIndexUniforms];

	simd::uint4 last_scissor = simd::make_uint4(0, 0, GetWindowWidth(), GetWindowHeight());
	simd::float2 fb_size = simd_float(last_scissor.zw);
	simd::float2 clip_off   = ToSimd(data->DisplayPos);       // (0,0) unless using multi-viewports
	simd::float2 clip_scale = ToSimd(data->FramebufferScale); // (1,1) unless using retina display which are often (2,2)
	ImTextureID last_tex = nullptr;

	for (int i = 0; i < data->CmdListsCount; i++)
	{
		const ImDrawList* cmd_list = data->CmdLists[i];
		size_t vtx_size = cmd_list->VtxBuffer.Size * sizeof(ImDrawVert);
		size_t idx_size = cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);
		memcpy(static_cast<char*>(map.cpu_buffer) + vtx_off, cmd_list->VtxBuffer.Data, vtx_size);
		memcpy(static_cast<char*>(map.cpu_buffer) + idx_off, cmd_list->IdxBuffer.Data, idx_size);

		for (const ImDrawCmd& cmd : cmd_list->CmdBuffer)
		{
			if (cmd.UserCallback)
				[NSException raise:@"Unimplemented" format:@"UserCallback not implemented"];
			if (!cmd.ElemCount)
				continue;

			simd::float4 clip_rect = (ToSimd(cmd.ClipRect) - clip_off.xyxy) * clip_scale.xyxy;
			simd::float2 clip_min = clip_rect.xy;
			simd::float2 clip_max = clip_rect.zw;
			clip_min = simd::max(clip_min, simd::float2(0));
			clip_max = simd::min(clip_max, fb_size);
			if (simd::any(clip_min >= clip_max))
				continue;
			simd::uint4 scissor = simd::make_uint4(simd_uint(clip_min), simd_uint(clip_max - clip_min));
			ImTextureID tex = cmd.GetTexID();
			if (simd::any(scissor != last_scissor))
			{
				last_scissor = scissor;
				[enc setScissorRect:(MTLScissorRect){ .x = scissor.x, .y = scissor.y, .width = scissor.z, .height = scissor.w }];
			}
			if (tex != last_tex)
			{
				last_tex = tex;
				[enc setFragmentTexture:(__bridge id<MTLTexture>)tex atIndex:0];
			}

			[enc setVertexBufferOffset:map.gpu_offset + vtx_off + cmd.VtxOffset * sizeof(ImDrawVert) atIndex:0];
			[enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle
			                indexCount:cmd.ElemCount
			                 indexType:sizeof(ImDrawIdx) == 2 ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32
			               indexBuffer:map.gpu_buffer
			         indexBufferOffset:map.gpu_offset + idx_off + cmd.IdxOffset * sizeof(ImDrawIdx)];
		}

		vtx_off += vtx_size;
		idx_off += idx_size;
	}

	[enc popDebugGroup];
}

#endif // __APPLE__
