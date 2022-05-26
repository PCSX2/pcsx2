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
#include "common/D3D12/Builders.h"
#include "common/D3D12/Context.h"
#include "common/D3D12/ShaderCache.h"
#include "common/D3D12/Util.h"
#include "common/Align.h"
#include "common/ScopedGuard.h"
#include "common/StringUtil.h"
#include "D3D12MemAlloc.h"
#include "GS.h"
#include "GSDevice12.h"
#include "GS/GSGL.h"
#include "GS/GSPerfMon.h"
#include "GS/GSUtil.h"
#include "Host.h"
#include "HostDisplay.h"
#include <sstream>
#include <limits>

static bool IsDepthConvertShader(ShaderConvert i)
{
	return (i == ShaderConvert::DEPTH_COPY || i == ShaderConvert::RGBA8_TO_FLOAT32 ||
			i == ShaderConvert::RGBA8_TO_FLOAT24 || i == ShaderConvert::RGBA8_TO_FLOAT16 ||
			i == ShaderConvert::RGB5A1_TO_FLOAT16 || i == ShaderConvert::DATM_0 ||
			i == ShaderConvert::DATM_1);
}

static bool IsIntConvertShader(ShaderConvert i)
{
	return (i == ShaderConvert::RGBA8_TO_16_BITS || i == ShaderConvert::FLOAT32_TO_16_BITS ||
			i == ShaderConvert::FLOAT32_TO_32_BITS);
}

static bool IsDATMConvertShader(ShaderConvert i) { return (i == ShaderConvert::DATM_0 || i == ShaderConvert::DATM_1); }

static bool IsPresentConvertShader(ShaderConvert i)
{
	return (i == ShaderConvert::COPY || (i >= ShaderConvert::SCANLINE && i <= ShaderConvert::COMPLEX_FILTER));
}

static D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE GetLoadOpForTexture(GSTexture12* tex)
{
	if (!tex)
		return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;

	// clang-format off
	switch (tex->GetState())
	{
	case GSTexture12::State::Cleared:       tex->SetState(GSTexture::State::Dirty); return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
	case GSTexture12::State::Invalidated:   tex->SetState(GSTexture::State::Dirty); return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
	case GSTexture12::State::Dirty:         return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
	default:                                return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
	}
	// clang-format on
}

GSDevice12::ShaderMacro::ShaderMacro(D3D_FEATURE_LEVEL fl)
{
	switch (fl)
	{
		case D3D_FEATURE_LEVEL_10_0:
			mlist.emplace_back("SHADER_MODEL", "0x400");
			break;
		case D3D_FEATURE_LEVEL_10_1:
			mlist.emplace_back("SHADER_MODEL", "0x401");
			break;
		case D3D_FEATURE_LEVEL_11_0:
		default:
			mlist.emplace_back("SHADER_MODEL", "0x500");
			break;
	}
	mlist.emplace_back("DX12", "1");
}

void GSDevice12::ShaderMacro::AddMacro(const char* n, int d)
{
	mlist.emplace_back(n, std::to_string(d));
}

D3D_SHADER_MACRO* GSDevice12::ShaderMacro::GetPtr(void)
{
	mout.clear();

	for (auto& i : mlist)
		mout.emplace_back(i.name.c_str(), i.def.c_str());

	mout.emplace_back(nullptr, nullptr);
	return (D3D_SHADER_MACRO*)mout.data();
}

GSDevice12::GSDevice12()
{
	std::memset(&m_pipeline_selector, 0, sizeof(m_pipeline_selector));
}

GSDevice12::~GSDevice12() {}

bool GSDevice12::Create(HostDisplay* display)
{
	if (!GSDevice::Create(display) || !CheckFeatures())
		return false;

	{
		std::optional<std::string> shader = Host::ReadResourceFileToString("shaders/dx11/tfx.fx");
		if (!shader.has_value())
		{
			Host::ReportErrorAsync("GS", "Failed to read shaders/dx11/tfx.fxf.");
			return false;
		}

		m_tfx_source = std::move(*shader);
	}

	if (!GSConfig.DisableShaderCache)
	{
		if (!m_shader_cache.Open(EmuFolders::Cache, g_d3d12_context->GetFeatureLevel(), SHADER_VERSION, GSConfig.UseDebugDevice))
		{
			Console.Warning("Shader cache failed to open.");
		}
	}
	else
	{
		m_shader_cache.Open({}, g_d3d12_context->GetFeatureLevel(), SHADER_VERSION, GSConfig.UseDebugDevice);
		Console.WriteLn("Not using shader cache.");
	}

	// reset stuff in case it was used by a previous device
	g_d3d12_context->GetSamplerAllocator().Reset();

	if (!CreateNullTexture())
	{
		Host::ReportErrorAsync("GS", "Failed to create dummy texture");
		return false;
	}

	if (!CreateRootSignatures())
	{
		Host::ReportErrorAsync("GS", "Failed to create pipeline layouts");
		return false;
	}

	if (!CreateBuffers())
		return false;

	if (!CompileConvertPipelines() || !CompileInterlacePipelines() ||
		!CompileMergePipelines() || !CompilePostProcessingPipelines())
	{
		Host::ReportErrorAsync("GS", "Failed to compile utility pipelines");
		return false;
	}

	InitializeState();
	InitializeSamplers();
	return true;
}

void GSDevice12::Destroy()
{
	if (!g_d3d12_context)
		return;

	EndRenderPass();
	ExecuteCommandList(true);
	DestroyResources();
	GSDevice::Destroy();
}

void GSDevice12::ResetAPIState()
{
	EndRenderPass();
}

void GSDevice12::RestoreAPIState()
{
	InvalidateCachedState();
}

void GSDevice12::PushDebugGroup(const char* fmt, ...)
{
}

void GSDevice12::PopDebugGroup()
{
}

void GSDevice12::InsertDebugMessage(DebugMessageCategory category, const char* fmt, ...)
{
}

bool GSDevice12::CheckFeatures()
{
	const u32 vendorID = g_d3d12_context->GetAdapterVendorID();
	const bool isAMD = (vendorID == 0x1002 || vendorID == 0x1022);

	m_features.texture_barrier = false;
	m_features.broken_point_sampler = isAMD;
	m_features.geometry_shader = true;
	m_features.image_load_store = true;
	m_features.prefer_new_textures = true;
	m_features.provoking_vertex_last = false;
	m_features.point_expand = false;
	m_features.line_expand = false;
	m_features.framebuffer_fetch = false;
	m_features.dual_source_blend = true;
	m_features.stencil_buffer = true;

	m_features.dxt_textures = g_d3d12_context->SupportsTextureFormat(DXGI_FORMAT_BC1_UNORM) &&
							  g_d3d12_context->SupportsTextureFormat(DXGI_FORMAT_BC2_UNORM) &&
							  g_d3d12_context->SupportsTextureFormat(DXGI_FORMAT_BC3_UNORM);
	m_features.bptc_textures = g_d3d12_context->SupportsTextureFormat(DXGI_FORMAT_BC7_UNORM);

	return true;
}

void GSDevice12::DrawPrimitive()
{
	g_perfmon.Put(GSPerfMon::DrawCalls, 1);
	g_d3d12_context->GetCommandList()->DrawInstanced(m_vertex.count, 1, m_vertex.start, 0);
}

void GSDevice12::DrawIndexedPrimitive()
{
	g_perfmon.Put(GSPerfMon::DrawCalls, 1);
	g_d3d12_context->GetCommandList()->DrawIndexedInstanced(m_index.count, 1, m_index.start, m_vertex.start, 0);
}

void GSDevice12::DrawIndexedPrimitive(int offset, int count)
{
	ASSERT(offset + count <= (int)m_index.count);
	g_perfmon.Put(GSPerfMon::DrawCalls, 1);
	g_d3d12_context->GetCommandList()->DrawIndexedInstanced(count, 1, m_index.start + offset, m_vertex.start, 0);
}

void GSDevice12::ClearRenderTarget(GSTexture* t, const GSVector4& c)
{
	if (!t)
		return;

	if (m_current_render_target == t)
		EndRenderPass();

	static_cast<GSTexture12*>(t)->SetClearColor(c);
}

void GSDevice12::ClearRenderTarget(GSTexture* t, u32 c) { ClearRenderTarget(t, GSVector4::rgba32(c) * (1.0f / 255)); }

void GSDevice12::InvalidateRenderTarget(GSTexture* t)
{
	if (!t)
		return;

	if (m_current_render_target == t || m_current_depth_target == t)
		EndRenderPass();

	t->SetState(GSTexture::State::Invalidated);
}

void GSDevice12::ClearDepth(GSTexture* t)
{
	if (!t)
		return;

	if (m_current_depth_target == t)
		EndRenderPass();

	static_cast<GSTexture12*>(t)->SetClearDepth(0.0f);
}

void GSDevice12::ClearStencil(GSTexture* t, u8 c)
{
	if (!t)
		return;

	EndRenderPass();

	GSTexture12* dxt = static_cast<GSTexture12*>(t);
	dxt->TransitionToState(D3D12_RESOURCE_STATE_DEPTH_WRITE);
	g_d3d12_context->GetCommandList()->ClearDepthStencilView(dxt->GetRTVOrDSVHandle(), D3D12_CLEAR_FLAG_STENCIL, 0.0f, c, 0, nullptr);
}

void GSDevice12::LookupNativeFormat(GSTexture::Format format, DXGI_FORMAT* d3d_format, DXGI_FORMAT* srv_format, DXGI_FORMAT* rtv_format, DXGI_FORMAT* dsv_format) const
{
	static constexpr std::array<std::array<DXGI_FORMAT, 4>, static_cast<int>(GSTexture::Format::BC7) + 1> s_format_mapping = {{
		{DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN}, // Invalid
		{DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN}, // Color
		{DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT, DXGI_FORMAT_UNKNOWN}, // FloatColor
		{DXGI_FORMAT_D32_FLOAT_S8X24_UINT, DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_D32_FLOAT_S8X24_UINT}, // DepthStencil
		{DXGI_FORMAT_A8_UNORM, DXGI_FORMAT_A8_UNORM, DXGI_FORMAT_A8_UNORM, DXGI_FORMAT_UNKNOWN}, // UNorm8
		{DXGI_FORMAT_R16_UINT, DXGI_FORMAT_R16_UINT, DXGI_FORMAT_R16_UINT, DXGI_FORMAT_UNKNOWN}, // UInt16
		{DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32_UINT, DXGI_FORMAT_UNKNOWN}, // UInt32
		{DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_UNKNOWN}, // Int32
		{DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_BC1_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN}, // BC1
		{DXGI_FORMAT_BC2_UNORM, DXGI_FORMAT_BC2_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN}, // BC2
		{DXGI_FORMAT_BC3_UNORM, DXGI_FORMAT_BC3_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN}, // BC3
		{DXGI_FORMAT_BC7_UNORM, DXGI_FORMAT_BC7_UNORM, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN}, // BC7
	}};

	const auto& mapping = s_format_mapping[static_cast<int>(format)];
	if (d3d_format)
		*d3d_format = mapping[0];
	if (srv_format)
		*srv_format = mapping[1];
	if (rtv_format)
		*rtv_format = mapping[2];
	if (dsv_format)
		*dsv_format = mapping[3];
}

GSTexture* GSDevice12::CreateSurface(GSTexture::Type type, int width, int height, int levels, GSTexture::Format format)
{
	pxAssert(type != GSTexture::Type::Offscreen && type != GSTexture::Type::SparseRenderTarget &&
			 type != GSTexture::Type::SparseDepthStencil);

	const u32 clamped_width = static_cast<u32>(std::clamp<int>(1, width, D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION));
	const u32 clamped_height = static_cast<u32>(std::clamp<int>(1, height, D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION));

	DXGI_FORMAT d3d_format, srv_format, rtv_format, dsv_format;
	LookupNativeFormat(format, &d3d_format, &srv_format, &rtv_format, &dsv_format);

	return GSTexture12::Create(type, clamped_width, clamped_height, levels, format, d3d_format, srv_format, rtv_format, dsv_format).release();
}

bool GSDevice12::DownloadTexture(GSTexture* src, const GSVector4i& rect, GSTexture::GSMap& out_map)
{
	const u32 width = rect.width();
	const u32 height = rect.height();
	const u32 pitch = Common::AlignUpPow2(width * D3D12::GetTexelSize(static_cast<GSTexture12*>(src)->GetNativeFormat()), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	const u32 size = pitch * height;
	constexpr u32 level = 0;
	if (!CheckStagingBufferSize(size))
	{
		Console.Error("Can't read back %ux%u", width, height);
		return false;
	}

	g_perfmon.Put(GSPerfMon::Readbacks, 1);
	EndRenderPass();
	UnmapStagingBuffer();

	{
		ID3D12GraphicsCommandList* cmdlist = g_d3d12_context->GetCommandList();
		GSTexture12* dsrc = static_cast<GSTexture12*>(src);
		GL_INS("ReadbackTexture: {%d,%d} %ux%u", rect.left, rect.top, width, height);

		D3D12_TEXTURE_COPY_LOCATION srcloc;
		srcloc.pResource = dsrc->GetResource();
		srcloc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		srcloc.SubresourceIndex = level;

		D3D12_TEXTURE_COPY_LOCATION dstloc;
		dstloc.pResource = m_readback_staging_buffer.get();
		dstloc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		dstloc.PlacedFootprint.Offset = 0;
		dstloc.PlacedFootprint.Footprint.Format = dsrc->GetNativeFormat();
		dstloc.PlacedFootprint.Footprint.Width = width;
		dstloc.PlacedFootprint.Footprint.Height = height;
		dstloc.PlacedFootprint.Footprint.Depth = 1;
		dstloc.PlacedFootprint.Footprint.RowPitch = pitch;

		const D3D12_RESOURCE_STATES old_layout = dsrc->GetResourceState();
		if (old_layout != D3D12_RESOURCE_STATE_COPY_SOURCE)
			dsrc->GetTexture().TransitionSubresourceToState(cmdlist, level, old_layout, D3D12_RESOURCE_STATE_COPY_SOURCE);

		const D3D12_BOX srcbox{static_cast<UINT>(rect.left), static_cast<UINT>(rect.top), 0u,
			static_cast<UINT>(rect.right), static_cast<UINT>(rect.bottom), 1u};
		cmdlist->CopyTextureRegion(&dstloc, 0, 0, 0, &srcloc, &srcbox);

		if (old_layout != D3D12_RESOURCE_STATE_COPY_SOURCE)
			dsrc->GetTexture().TransitionSubresourceToState(cmdlist, level, D3D12_RESOURCE_STATE_COPY_SOURCE, old_layout);
	}

	// exec and wait
	ExecuteCommandList(true);

	if (!MapStagingBuffer(size))
		return false;

	out_map.bits = reinterpret_cast<u8*>(m_readback_staging_buffer_map);
	out_map.pitch = pitch;
	return true;
}

void GSDevice12::DownloadTextureComplete()
{
	UnmapStagingBuffer();
}

void GSDevice12::CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r, u32 destX, u32 destY)
{
	g_perfmon.Put(GSPerfMon::TextureCopies, 1);

	GSTexture12* const sTexVK = static_cast<GSTexture12*>(sTex);
	GSTexture12* const dTexVK = static_cast<GSTexture12*>(dTex);
	const GSVector4i dtex_rc(0, 0, dTexVK->GetWidth(), dTexVK->GetHeight());

	if (sTexVK->GetState() == GSTexture::State::Cleared)
	{
		// source is cleared. if destination is a render target, we can carry the clear forward
		if (dTexVK->IsRenderTargetOrDepthStencil())
		{
			if (dtex_rc.eq(r))
			{
				// pass it forward if we're clearing the whole thing
				if (sTexVK->IsDepthStencil())
					dTexVK->SetClearDepth(sTexVK->GetClearDepth());
				else
					dTexVK->SetClearColor(sTexVK->GetClearColor());

				return;
			}
			else
			{
				// otherwise we need to do an attachment clear
				EndRenderPass();

				if (dTexVK->GetType() != GSTexture::Type::DepthStencil)
				{
					dTexVK->TransitionToState(D3D12_RESOURCE_STATE_RENDER_TARGET);
					g_d3d12_context->GetCommandList()->ClearRenderTargetView(dTexVK->GetRTVOrDSVHandle(),
						sTexVK->GetClearColor().v, 0, nullptr);
				}
				else
				{
					dTexVK->TransitionToState(D3D12_RESOURCE_STATE_DEPTH_WRITE);
					g_d3d12_context->GetCommandList()->ClearDepthStencilView(dTexVK->GetRTVOrDSVHandle(),
						D3D12_CLEAR_FLAG_DEPTH, sTexVK->GetClearDepth(), 0, 0, nullptr);
				}

				return;
			}
		}

		// commit the clear to the source first, then do normal copy
		sTexVK->CommitClear();
	}

	// if the destination has been cleared, and we're not overwriting the whole thing, commit the clear first
	// (the area outside of where we're copying to)
	if (dTexVK->GetState() == GSTexture::State::Cleared && !dtex_rc.eq(r))
		dTexVK->CommitClear();

	EndRenderPass();
	sTexVK->TransitionToState(D3D12_RESOURCE_STATE_COPY_SOURCE);
	dTexVK->SetState(GSTexture::State::Dirty);
	dTexVK->TransitionToState(D3D12_RESOURCE_STATE_COPY_DEST);

	D3D12_TEXTURE_COPY_LOCATION srcloc;
	srcloc.pResource = sTexVK->GetResource();
	srcloc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	srcloc.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION dstloc;
	dstloc.pResource = dTexVK->GetResource();
	dstloc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dstloc.SubresourceIndex = 0;

	const D3D12_BOX srcbox{static_cast<UINT>(r.left), static_cast<UINT>(r.top), 0u,
		static_cast<UINT>(r.right), static_cast<UINT>(r.bottom), 1u};
	g_d3d12_context->GetCommandList()->CopyTextureRegion(
		&dstloc, destX, destY, 0,
		&srcloc, &srcbox);

	dTexVK->SetState(GSTexture::State::Dirty);
}

void GSDevice12::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect,
	ShaderConvert shader /* = ShaderConvert::COPY */, bool linear /* = true */)
{
	pxAssert(IsDepthConvertShader(shader) == (dTex && dTex->GetType() == GSTexture::Type::DepthStencil));

	GL_INS("StretchRect(%d) {%d,%d} %dx%d -> {%d,%d) %dx%d", shader, int(sRect.left), int(sRect.top),
		int(sRect.right - sRect.left), int(sRect.bottom - sRect.top), int(dRect.left), int(dRect.top),
		int(dRect.right - dRect.left), int(dRect.bottom - dRect.top));

	DoStretchRect(static_cast<GSTexture12*>(sTex), sRect, static_cast<GSTexture12*>(dTex), dRect,
		dTex ? m_convert[static_cast<int>(shader)].get() : m_present[static_cast<int>(shader)].get(), linear);
}

void GSDevice12::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, bool red,
	bool green, bool blue, bool alpha)
{
	GL_PUSH("ColorCopy Red:%d Green:%d Blue:%d Alpha:%d", red, green, blue, alpha);

	const u32 index = (red ? 1 : 0) | (green ? 2 : 0) | (blue ? 4 : 0) | (alpha ? 8 : 0);
	DoStretchRect(
		static_cast<GSTexture12*>(sTex), sRect, static_cast<GSTexture12*>(dTex), dRect, m_color_copy[index].get(), false);
}

void GSDevice12::BeginRenderPassForStretchRect(GSTexture12* dTex, const GSVector4i& dtex_rc, const GSVector4i& dst_rc)
{
	const bool is_whole_target = dst_rc.eq(dtex_rc);
	const D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE load_op = is_whole_target ? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD : GetLoadOpForTexture(dTex);
	dTex->SetState(GSTexture::State::Dirty);

	if (dTex->GetType() != GSTexture::Type::DepthStencil)
	{
		const GSVector4 clear_color(dTex->GetClearColor());
		BeginRenderPass(load_op, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE,
			D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
			D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
			clear_color);
	}
	else
	{
		const float clear_depth = dTex->GetClearDepth();
		BeginRenderPass(D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
			load_op, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE,
			D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
			GSVector4::zero(), clear_depth);
	}
}

void GSDevice12::DoStretchRect(GSTexture12* sTex, const GSVector4& sRect, GSTexture12* dTex, const GSVector4& dRect, const ID3D12PipelineState* pipeline, bool linear)
{
	if (sTex->GetResourceState() != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
	{
		// can't transition in a render pass
		EndRenderPass();
		sTex->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	SetUtilityRootSignature();
	SetUtilityTexture(sTex, linear ? m_linear_sampler_cpu : m_point_sampler_cpu);
	SetPipeline(pipeline);

	const bool is_present = (!dTex);
	const bool depth = (dTex && dTex->GetType() == GSTexture::Type::DepthStencil);
	const GSVector2i size(
		is_present ? GSVector2i(m_display->GetWindowWidth(), m_display->GetWindowHeight()) : dTex->GetSize());
	const GSVector4i dtex_rc(0, 0, size.x, size.y);
	const GSVector4i dst_rc(GSVector4i(dRect).rintersect(dtex_rc));

	// switch rts (which might not end the render pass), so check the bounds
	if (!is_present)
	{
		OMSetRenderTargets(depth ? nullptr : dTex, depth ? dTex : nullptr, dst_rc);
	}
	else
	{
		// this is for presenting, we don't want to screw with the viewport/scissor set by display
		m_dirty_flags &= ~(DIRTY_FLAG_RENDER_TARGET | DIRTY_FLAG_VIEWPORT | DIRTY_FLAG_SCISSOR);
	}

	const bool drawing_to_current_rt = (is_present || InRenderPass());
	if (!drawing_to_current_rt)
		BeginRenderPassForStretchRect(dTex, dtex_rc, dst_rc);

	DrawStretchRect(sRect, dRect, size);

	if (!drawing_to_current_rt)
	{
		EndRenderPass();
		static_cast<GSTexture12*>(dTex)->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
}

void GSDevice12::DrawStretchRect(const GSVector4& sRect, const GSVector4& dRect, const GSVector2i& ds)
{
	// ia
	const float left = dRect.x * 2 / ds.x - 1.0f;
	const float top = 1.0f - dRect.y * 2 / ds.y;
	const float right = dRect.z * 2 / ds.x - 1.0f;
	const float bottom = 1.0f - dRect.w * 2 / ds.y;

	GSVertexPT1 vertices[] = {
		{GSVector4(left, top, 0.5f, 1.0f), GSVector2(sRect.x, sRect.y)},
		{GSVector4(right, top, 0.5f, 1.0f), GSVector2(sRect.z, sRect.y)},
		{GSVector4(left, bottom, 0.5f, 1.0f), GSVector2(sRect.x, sRect.w)},
		{GSVector4(right, bottom, 0.5f, 1.0f), GSVector2(sRect.z, sRect.w)},
	};
	IASetVertexBuffer(vertices, sizeof(vertices[0]), std::size(vertices));
	SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	if (ApplyUtilityState())
		DrawPrimitive();
}

void GSDevice12::DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect,
	const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, const GSVector4& c)
{
	GL_PUSH("DoMerge");

	const GSVector4 full_r(0.0f, 0.0f, 1.0f, 1.0f);
	const bool feedback_write_2 = PMODE.EN2 && sTex[2] != nullptr && EXTBUF.FBIN == 1;
	const bool feedback_write_1 = PMODE.EN1 && sTex[2] != nullptr && EXTBUF.FBIN == 0;
	const bool feedback_write_2_but_blend_bg = feedback_write_2 && PMODE.SLBG == 1;

	// Merge the 2 source textures (sTex[0],sTex[1]). Final results go to dTex. Feedback write will go to sTex[2].
	// If either 2nd output is disabled or SLBG is 1, a background color will be used.
	// Note: background color is also used when outside of the unit rectangle area
	EndRenderPass();

	// transition everything before starting the new render pass
	static_cast<GSTexture12*>(dTex)->TransitionToState(D3D12_RESOURCE_STATE_RENDER_TARGET);
	if (sTex[0])
		static_cast<GSTexture12*>(sTex[0])->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	// Upload constant to select YUV algo, but skip constant buffer update if we don't need it
	if (feedback_write_2 || feedback_write_1 || sTex[0])
	{
		SetUtilityRootSignature();
		const MergeConstantBuffer uniforms = {c, EXTBUF.EMODA, EXTBUF.EMODC};
		SetUtilityPushConstants(&uniforms, sizeof(uniforms));
	}

	const GSVector2i dsize(dTex->GetSize());
	const GSVector4i darea(0, 0, dsize.x, dsize.y);
	bool dcleared = false;
	if (sTex[1] && (PMODE.SLBG == 0 || feedback_write_2_but_blend_bg))
	{
		// 2nd output is enabled and selected. Copy it to destination so we can blend it with 1st output
		// Note: value outside of dRect must contains the background color (c)
		if (sTex[1]->GetState() == GSTexture::State::Dirty)
		{
			static_cast<GSTexture12*>(sTex[1])->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			OMSetRenderTargets(dTex, nullptr, darea);
			SetUtilityTexture(sTex[1], m_linear_sampler_cpu);
			BeginRenderPass(D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE,
				D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
				D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS, c);
			SetUtilityRootSignature();
			SetPipeline(m_convert[static_cast<int>(ShaderConvert::COPY)].get());
			DrawStretchRect(sRect[1], PMODE.SLBG ? dRect[2] : dRect[1], dsize);
			dTex->SetState(GSTexture::State::Dirty);
			dcleared = true;
		}
	}

	// Upload constant to select YUV algo
	const GSVector2i fbsize(sTex[2] ? sTex[2]->GetSize() : GSVector2i(0, 0));
	const GSVector4i fbarea(0, 0, fbsize.x, fbsize.y);
	if (feedback_write_2) // FIXME I'm not sure dRect[1] is always correct
	{
		EndRenderPass();
		OMSetRenderTargets(sTex[2], nullptr, fbarea);
		if (dcleared)
			SetUtilityTexture(dTex, m_linear_sampler_cpu);

		// sTex[2] can be sTex[0], in which case it might be cleared (e.g. Xenosaga).
		BeginRenderPassForStretchRect(static_cast<GSTexture12*>(sTex[2]), fbarea, GSVector4i(dRect[2]));
		if (dcleared)
		{
			SetUtilityRootSignature();
			SetPipeline(m_convert[static_cast<int>(ShaderConvert::YUV)].get());
			DrawStretchRect(full_r, dRect[2], fbsize);
		}
		EndRenderPass();

		if (sTex[0] == sTex[2])
		{
			// need a barrier here because of the render pass
			static_cast<GSTexture12*>(sTex[2])->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
	}

	// Restore background color to process the normal merge
	if (feedback_write_2_but_blend_bg || !dcleared)
	{
		EndRenderPass();
		OMSetRenderTargets(dTex, nullptr, darea);
		BeginRenderPass(D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE,
			D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
			D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS, c);
	}
	else if (!InRenderPass())
	{
		OMSetRenderTargets(dTex, nullptr, darea);
		BeginRenderPass(D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE);
	}

	if (sTex[0] && sTex[0]->GetState() == GSTexture::State::Dirty)
	{
		// 1st output is enabled. It must be blended
		SetUtilityRootSignature();
		SetUtilityTexture(sTex[0], m_linear_sampler_cpu);
		SetPipeline(m_merge[PMODE.MMOD].get());
		DrawStretchRect(sRect[0], dRect[0], dTex->GetSize());
	}

	if (feedback_write_1) // FIXME I'm not sure dRect[0] is always correct
	{
		EndRenderPass();
		SetUtilityRootSignature();
		SetPipeline(m_convert[static_cast<int>(ShaderConvert::YUV)].get());
		SetUtilityTexture(dTex, m_linear_sampler_cpu);
		OMSetRenderTargets(sTex[2], nullptr, fbarea);
		BeginRenderPass(D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE);
		DrawStretchRect(full_r, dRect[2], dsize);
	}

	EndRenderPass();

	// this texture is going to get used as an input, so make sure we don't read undefined data
	static_cast<GSTexture12*>(dTex)->CommitClear();
	static_cast<GSTexture12*>(dTex)->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void GSDevice12::DoInterlace(GSTexture* sTex, GSTexture* dTex, int shader, bool linear, float yoffset)
{
	const GSVector2i size(dTex->GetSize());
	const GSVector4 s = GSVector4(size);

	const GSVector4 sRect(0, 0, 1, 1);
	const GSVector4 dRect(0.0f, yoffset, s.x, s.y + yoffset);

	InterlaceConstantBuffer cb;
	cb.ZrH = GSVector2(0, 1.0f / s.y);

	GL_PUSH("DoInterlace %dx%d Shader:%d Linear:%d", size.x, size.y, shader, linear);

	static_cast<GSTexture12*>(dTex)->TransitionToState(D3D12_RESOURCE_STATE_RENDER_TARGET);

	const GSVector4i rc(0, 0, size.x, size.y);
	EndRenderPass();
	OMSetRenderTargets(dTex, nullptr, rc);
	SetUtilityRootSignature();
	SetUtilityTexture(sTex, linear ? m_linear_sampler_cpu : m_point_sampler_cpu);
	BeginRenderPass(D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE,
		D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS);
	SetPipeline(m_interlace[shader].get());
	SetUtilityPushConstants(&cb, sizeof(cb));
	DrawStretchRect(sRect, dRect, dTex->GetSize());
	EndRenderPass();

	// this texture is going to get used as an input, so make sure we don't read undefined data
	static_cast<GSTexture12*>(dTex)->CommitClear();
	static_cast<GSTexture12*>(dTex)->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void GSDevice12::DoShadeBoost(GSTexture* sTex, GSTexture* dTex, const float params[4])
{
	const GSVector4 sRect(0.0f, 0.0f, 1.0f, 1.0f);
	const GSVector4i dRect(0, 0, dTex->GetWidth(), dTex->GetHeight());
	EndRenderPass();
	OMSetRenderTargets(dTex, nullptr, dRect);
	SetUtilityTexture(sTex, m_point_sampler_cpu);
	BeginRenderPass(D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE,
		D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS);
	SetPipeline(m_shadeboost_pipeline.get());
	SetUtilityPushConstants(params, sizeof(float) * 4);
	DrawStretchRect(sRect, GSVector4(dRect), dTex->GetSize());
	EndRenderPass();

	static_cast<GSTexture12*>(dTex)->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void GSDevice12::DoFXAA(GSTexture* sTex, GSTexture* dTex)
{
	const GSVector4 sRect(0.0f, 0.0f, 1.0f, 1.0f);
	const GSVector4i dRect(0, 0, dTex->GetWidth(), dTex->GetHeight());
	EndRenderPass();
	OMSetRenderTargets(dTex, nullptr, dRect);
	SetUtilityTexture(sTex, m_linear_sampler_cpu);
	BeginRenderPass(D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE,
		D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS);
	SetPipeline(m_fxaa_pipeline.get());
	DrawStretchRect(sRect, GSVector4(dRect), dTex->GetSize());
	EndRenderPass();

	static_cast<GSTexture12*>(dTex)->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void GSDevice12::IASetVertexBuffer(const void* vertex, size_t stride, size_t count)
{
	const u32 size = static_cast<u32>(stride) * static_cast<u32>(count);
	if (!m_vertex_stream_buffer.ReserveMemory(size, static_cast<u32>(stride)))
	{
		ExecuteCommandListAndRestartRenderPass("Uploading to vertex buffer");
		if (!m_vertex_stream_buffer.ReserveMemory(size, static_cast<u32>(stride)))
			pxFailRel("Failed to reserve space for vertices");
	}

	m_vertex.start = m_vertex_stream_buffer.GetCurrentOffset() / stride;
	m_vertex.limit = count;
	m_vertex.stride = stride;
	m_vertex.count = count;
	SetVertexBuffer(m_vertex_stream_buffer.GetGPUPointer(), m_vertex_stream_buffer.GetSize(), stride);

	GSVector4i::storent(m_vertex_stream_buffer.GetCurrentHostPointer(), vertex, count * stride);
	m_vertex_stream_buffer.CommitMemory(size);
}

bool GSDevice12::IAMapVertexBuffer(void** vertex, size_t stride, size_t count)
{
	const u32 size = static_cast<u32>(stride) * static_cast<u32>(count);
	if (!m_vertex_stream_buffer.ReserveMemory(size, static_cast<u32>(stride)))
	{
		ExecuteCommandListAndRestartRenderPass("Mapping bytes to vertex buffer");
		if (!m_vertex_stream_buffer.ReserveMemory(size, static_cast<u32>(stride)))
			pxFailRel("Failed to reserve space for vertices");
	}

	m_vertex.start = m_vertex_stream_buffer.GetCurrentOffset() / stride;
	m_vertex.limit = m_vertex_stream_buffer.GetCurrentSpace() / stride;
	m_vertex.stride = stride;
	m_vertex.count = count;
	SetVertexBuffer(m_vertex_stream_buffer.GetGPUPointer(), m_vertex_stream_buffer.GetSize(), stride);

	*vertex = m_vertex_stream_buffer.GetCurrentHostPointer();
	return true;
}

void GSDevice12::IAUnmapVertexBuffer()
{
	const u32 size = static_cast<u32>(m_vertex.stride) * static_cast<u32>(m_vertex.count);
	m_vertex_stream_buffer.CommitMemory(size);
}

void GSDevice12::IASetIndexBuffer(const void* index, size_t count)
{
	const u32 size = sizeof(u32) * static_cast<u32>(count);
	if (!m_index_stream_buffer.ReserveMemory(size, sizeof(u32)))
	{
		ExecuteCommandListAndRestartRenderPass("Uploading bytes to index buffer");
		if (!m_index_stream_buffer.ReserveMemory(size, sizeof(u32)))
			pxFailRel("Failed to reserve space for vertices");
	}

	m_index.start = m_index_stream_buffer.GetCurrentOffset() / sizeof(u32);
	m_index.limit = count;
	m_index.count = count;
	SetIndexBuffer(m_index_stream_buffer.GetGPUPointer(), m_index_stream_buffer.GetSize(), DXGI_FORMAT_R32_UINT);

	std::memcpy(m_index_stream_buffer.GetCurrentHostPointer(), index, size);
	m_index_stream_buffer.CommitMemory(size);
}

void GSDevice12::OMSetRenderTargets(GSTexture* rt, GSTexture* ds, const GSVector4i& scissor)
{
	GSTexture12* vkRt = static_cast<GSTexture12*>(rt);
	GSTexture12* vkDs = static_cast<GSTexture12*>(ds);
	pxAssert(vkRt || vkDs);

	if (m_current_render_target != vkRt || m_current_depth_target != vkDs)
	{
		// framebuffer change
		EndRenderPass();
	}

	m_current_render_target = vkRt;
	m_current_depth_target = vkDs;

	if (!InRenderPass())
	{
		if (vkRt)
			vkRt->TransitionToState(D3D12_RESOURCE_STATE_RENDER_TARGET);
		if (vkDs)
			vkDs->TransitionToState(D3D12_RESOURCE_STATE_DEPTH_WRITE);
	}

	// This is used to set/initialize the framebuffer for tfx rendering.
	const GSVector2i size = vkRt ? vkRt->GetSize() : vkDs->GetSize();
	const D3D12_VIEWPORT vp{0.0f, 0.0f, static_cast<float>(size.x), static_cast<float>(size.y), 0.0f, 1.0f};

	SetViewport(vp);
	SetScissor(scissor);
}

bool GSDevice12::GetSampler(D3D12::DescriptorHandle* cpu_handle, GSHWDrawConfig::SamplerSelector ss)
{
	const auto it = m_samplers.find(ss.key);
	if (it != m_samplers.end())
	{
		*cpu_handle = it->second;
		return true;
	}

	D3D12_SAMPLER_DESC sd = {};
	const int anisotropy = GSConfig.MaxAnisotropy;
	if (GSConfig.MaxAnisotropy > 1 && ss.aniso)
	{
		sd.Filter = D3D12_FILTER_ANISOTROPIC;
	}
	else
	{
		static constexpr std::array<D3D12_FILTER, 8> filters = {{
			D3D12_FILTER_MIN_MAG_MIP_POINT, // 000 / min=point,mag=point,mip=point
			D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT, // 001 / min=linear,mag=point,mip=point
			D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT, // 010 / min=point,mag=linear,mip=point
			D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT, // 011 / min=linear,mag=linear,mip=point
			D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR, // 100 / min=point,mag=point,mip=linear
			D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR, // 101 / min=linear,mag=point,mip=linear
			D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR, // 110 / min=point,mag=linear,mip=linear
			D3D12_FILTER_MIN_MAG_MIP_LINEAR, // 111 / min=linear,mag=linear,mip=linear
		}};

		const u8 index = (static_cast<u8>(ss.IsMipFilterLinear()) << 2) |
						 (static_cast<u8>(ss.IsMagFilterLinear()) << 1) |
						 static_cast<u8>(ss.IsMinFilterLinear());
		sd.Filter = filters[index];
	}

	sd.AddressU = ss.tau ? D3D12_TEXTURE_ADDRESS_MODE_WRAP : D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sd.AddressV = ss.tav ? D3D12_TEXTURE_ADDRESS_MODE_WRAP : D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sd.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sd.MinLOD = 0.0f;
	sd.MaxLOD = ss.lodclamp ? 0.0f : FLT_MAX;
	sd.MaxAnisotropy = std::clamp(GSConfig.MaxAnisotropy, 1, 16);
	sd.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

	if (!g_d3d12_context->GetSamplerHeapManager().Allocate(cpu_handle))
		return false;

	g_d3d12_context->GetDevice()->CreateSampler(&sd, *cpu_handle);
	m_samplers.emplace(ss.key, *cpu_handle);
	return true;
}

void GSDevice12::ClearSamplerCache()
{
	for (auto& it : m_samplers)
		g_d3d12_context->DeferDescriptorDestruction(g_d3d12_context->GetSamplerHeapManager(), &it.second);
	m_samplers.clear();
	g_d3d12_context->InvalidateSamplerGroups();
	InitializeSamplers();

	m_utility_sampler_gpu = m_point_sampler_cpu;
	m_tfx_samplers_handle_gpu.Clear();
	m_dirty_flags |= DIRTY_FLAG_TFX_SAMPLERS;
}

bool GSDevice12::GetTextureGroupDescriptors(D3D12::DescriptorHandle* gpu_handle, const D3D12::DescriptorHandle* cpu_handles, u32 count)
{
	if (!g_d3d12_context->GetDescriptorAllocator().Allocate(count, gpu_handle))
		return false;

	if (count == 1)
	{
		g_d3d12_context->GetDevice()->CopyDescriptorsSimple(1, *gpu_handle, cpu_handles[0], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		return true;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE dst_handle = *gpu_handle;
	D3D12_CPU_DESCRIPTOR_HANDLE src_handles[NUM_TFX_TEXTURES];
	UINT src_sizes[NUM_TFX_TEXTURES];
	pxAssert(count <= NUM_TFX_TEXTURES);
	for (u32 i = 0; i < count; i++)
	{
		src_handles[i] = cpu_handles[i];
		src_sizes[i] = 1;
	}
	g_d3d12_context->GetDevice()->CopyDescriptors(1, &dst_handle, &count, count, src_handles, src_sizes, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	return true;
}

static void AddUtilityVertexAttributes(D3D12::GraphicsPipelineBuilder& gpb)
{
	gpb.AddVertexAttribute("POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0);
	gpb.AddVertexAttribute("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16);
	gpb.AddVertexAttribute("COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 28);
	gpb.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
}

GSDevice12::ComPtr<ID3DBlob> GSDevice12::GetUtilityVertexShader(const std::string& source, const char* entry_point)
{
	ShaderMacro sm_model(m_shader_cache.GetFeatureLevel());
	return m_shader_cache.GetVertexShader(source, sm_model.GetPtr(), entry_point);
}

GSDevice12::ComPtr<ID3DBlob> GSDevice12::GetUtilityPixelShader(const std::string& source, const char* entry_point)
{
	ShaderMacro sm_model(m_shader_cache.GetFeatureLevel());
	sm_model.AddMacro("PS_SCALE_FACTOR", std::max(1u, GSConfig.UpscaleMultiplier));
	return m_shader_cache.GetPixelShader(source, sm_model.GetPtr(), entry_point);
}

bool GSDevice12::CreateNullTexture()
{
	if (!m_null_texture.Create(1, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM,
			DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, D3D12_RESOURCE_FLAG_NONE))
	{
		return false;
	}

	m_null_texture.TransitionToState(g_d3d12_context->GetCommandList(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	D3D12::SetObjectName(m_null_texture.GetResource(), "Null texture");
	return true;
}

bool GSDevice12::CreateBuffers()
{
	if (!m_vertex_stream_buffer.Create(VERTEX_BUFFER_SIZE))
	{
		Host::ReportErrorAsync("GS", "Failed to allocate vertex buffer");
		return false;
	}

	if (!m_index_stream_buffer.Create(INDEX_BUFFER_SIZE))
	{
		Host::ReportErrorAsync("GS", "Failed to allocate index buffer");
		return false;
	}

	if (!m_vertex_constant_buffer.Create(VERTEX_UNIFORM_BUFFER_SIZE))
	{
		Host::ReportErrorAsync("GS", "Failed to allocate vertex uniform buffer");
		return false;
	}

	if (!m_pixel_constant_buffer.Create(FRAGMENT_UNIFORM_BUFFER_SIZE))
	{
		Host::ReportErrorAsync("GS", "Failed to allocate fragment uniform buffer");
		return false;
	}

	return true;
}

bool GSDevice12::CreateRootSignatures()
{
	D3D12::RootSignatureBuilder rsb;

	//////////////////////////////////////////////////////////////////////////
	// Convert Pipeline Layout
	//////////////////////////////////////////////////////////////////////////
	rsb.SetInputAssemblerFlag();
	rsb.Add32BitConstants(0, CONVERT_PUSH_CONSTANTS_SIZE / sizeof(u32), static_cast<D3D12_SHADER_VISIBILITY>(D3D12_SHADER_VISIBILITY_VERTEX | D3D12_SHADER_VISIBILITY_PIXEL));
	rsb.AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, NUM_UTILITY_SAMPLERS, D3D12_SHADER_VISIBILITY_PIXEL);
	rsb.AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 0, NUM_UTILITY_SAMPLERS, D3D12_SHADER_VISIBILITY_PIXEL);
	if (!(m_utility_root_signature = rsb.Create()))
		return false;
	D3D12::SetObjectName(m_utility_root_signature.get(), "Convert root signature");

	//////////////////////////////////////////////////////////////////////////
	// Draw/TFX Pipeline Layout
	//////////////////////////////////////////////////////////////////////////
	rsb.SetInputAssemblerFlag();
	rsb.AddCBVParameter(0, D3D12_SHADER_VISIBILITY_ALL);
	rsb.AddCBVParameter(1, D3D12_SHADER_VISIBILITY_PIXEL);
	rsb.AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 2, D3D12_SHADER_VISIBILITY_PIXEL);
	rsb.AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 0, 2, D3D12_SHADER_VISIBILITY_PIXEL);
	rsb.AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 2, D3D12_SHADER_VISIBILITY_PIXEL);
	if (!(m_tfx_root_signature = rsb.Create()))
		return false;
	D3D12::SetObjectName(m_tfx_root_signature.get(), "TFX root signature");
	return true;
}

bool GSDevice12::CompileConvertPipelines()
{
	std::optional<std::string> shader = Host::ReadResourceFileToString("shaders/dx11/convert.fx");
	if (!shader)
	{
		Host::ReportErrorAsync("GS", "Failed to read shaders/dx11/convert.fx.");
		return false;
	}

	m_convert_vs = GetUtilityVertexShader(*shader, "vs_main");
	if (!m_convert_vs)
		return false;

	D3D12::GraphicsPipelineBuilder gpb;
	gpb.SetRootSignature(m_utility_root_signature.get());
	AddUtilityVertexAttributes(gpb);
	gpb.SetNoCullRasterizationState();
	gpb.SetNoBlendingState();
	gpb.SetVertexShader(m_convert_vs.get());

	for (ShaderConvert i = ShaderConvert::COPY; static_cast<int>(i) < static_cast<int>(ShaderConvert::Count);
		 i = static_cast<ShaderConvert>(static_cast<int>(i) + 1))
	{
		const bool depth = IsDepthConvertShader(i);
		const int index = static_cast<int>(i);

		switch (i)
		{
			case ShaderConvert::RGBA8_TO_16_BITS:
			case ShaderConvert::FLOAT32_TO_16_BITS:
			{
				gpb.SetRenderTarget(0, DXGI_FORMAT_R16_UINT);
				gpb.SetDepthStencilFormat(DXGI_FORMAT_UNKNOWN);
			}
			break;
			case ShaderConvert::FLOAT32_TO_32_BITS:
			{
				gpb.SetRenderTarget(0, DXGI_FORMAT_R32_UINT);
				gpb.SetDepthStencilFormat(DXGI_FORMAT_UNKNOWN);
			}
			break;
			case ShaderConvert::DATM_0:
			case ShaderConvert::DATM_1:
			{
				gpb.ClearRenderTargets();
				gpb.SetDepthStencilFormat(DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
			}
			break;
			default:
			{
				depth ? gpb.ClearRenderTargets() : gpb.SetRenderTarget(0, DXGI_FORMAT_R8G8B8A8_UNORM);
				gpb.SetDepthStencilFormat(depth ? DXGI_FORMAT_D32_FLOAT_S8X24_UINT : DXGI_FORMAT_UNKNOWN);
			}
			break;
		}

		if (IsDATMConvertShader(i))
		{
			const D3D12_DEPTH_STENCILOP_DESC sos = {
				D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_REPLACE, D3D12_COMPARISON_FUNC_ALWAYS};
			gpb.SetStencilState(true, 1, 1, sos, sos);
			gpb.SetDepthState(false, false, D3D12_COMPARISON_FUNC_ALWAYS);
		}
		else
		{
			gpb.SetDepthState(depth, depth, D3D12_COMPARISON_FUNC_ALWAYS);
			gpb.SetNoStencilState();
		}

		ComPtr<ID3DBlob> ps(GetUtilityPixelShader(*shader, shaderName(i)));
		if (!ps)
			return false;

		gpb.SetPixelShader(ps.get());

		m_convert[index] = gpb.Create(g_d3d12_context->GetDevice(), m_shader_cache, false);
		if (!m_convert[index])
			return false;

		D3D12::SetObjectNameFormatted(m_convert[index].get(), "Convert pipeline %d", i);

		if (/*swapchain && */ IsPresentConvertShader(i))
		{
			// TODO: compile a present variant too
			gpb.SetRenderTarget(0, DXGI_FORMAT_R8G8B8A8_UNORM);
			m_present[index] = gpb.Create(g_d3d12_context->GetDevice(), m_shader_cache, false);
			if (!m_present[index])
				return false;

			D3D12::SetObjectNameFormatted(m_present[index].get(), "Convert pipeline %d (Present)", i);
		}

		if (i == ShaderConvert::COPY)
		{
			// compile the variant for setting up hdr rendering
			for (u32 ds = 0; ds < 2; ds++)
			{
				gpb.SetRenderTarget(0, DXGI_FORMAT_R32G32B32A32_FLOAT);
				gpb.SetDepthStencilFormat(ds ? DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS : DXGI_FORMAT_UNKNOWN);
				m_hdr_setup_pipelines[ds] = gpb.Create(g_d3d12_context->GetDevice(), m_shader_cache, false);
				if (!m_hdr_setup_pipelines[ds])
					return false;

				D3D12::SetObjectNameFormatted(m_hdr_setup_pipelines[ds].get(), "HDR setup/copy pipeline (ds=%u)", i, ds);
			}

			// compile color copy pipelines
			gpb.SetRenderTarget(0, DXGI_FORMAT_R8G8B8A8_UNORM);
			gpb.SetDepthStencilFormat(DXGI_FORMAT_UNKNOWN);
			for (u32 i = 0; i < 16; i++)
			{
				pxAssert(!m_color_copy[i]);
				gpb.SetBlendState(0, false, D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
					D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD, static_cast<u8>(i));
				m_color_copy[i] = gpb.Create(g_d3d12_context->GetDevice(), m_shader_cache, false);
				if (!m_color_copy[i])
					return false;

				D3D12::SetObjectNameFormatted(m_color_copy[i].get(),
					"Color copy pipeline (r=%u, g=%u, b=%u, a=%u)", i & 1u, (i >> 1) & 1u, (i >> 2) & 1u,
					(i >> 3) & 1u);
			}
		}
		else if (i == ShaderConvert::MOD_256)
		{
			for (u32 ds = 0; ds < 2; ds++)
			{
				pxAssert(!m_hdr_finish_pipelines[ds]);

				gpb.SetDepthStencilFormat(ds ? DXGI_FORMAT_D32_FLOAT_S8X24_UINT : DXGI_FORMAT_UNKNOWN);
				m_hdr_finish_pipelines[ds] = gpb.Create(g_d3d12_context->GetDevice(), m_shader_cache, false);
				if (!m_hdr_finish_pipelines[ds])
					return false;

				D3D12::SetObjectNameFormatted(m_hdr_setup_pipelines[ds].get(), "HDR finish/copy pipeline (ds=%u)", ds);
			}
		}
	}

	for (u32 datm = 0; datm < 2; datm++)
	{
		ComPtr<ID3DBlob> ps(GetUtilityPixelShader(*shader, datm ? "ps_stencil_image_init_1" : "ps_stencil_image_init_0"));
		if (!ps)
			return false;

		gpb.SetRootSignature(m_utility_root_signature.get());
		gpb.SetRenderTarget(0, DXGI_FORMAT_R32_FLOAT);
		gpb.SetPixelShader(ps.get());
		gpb.SetNoDepthTestState();
		gpb.SetNoStencilState();
		gpb.SetBlendState(0, true, D3D12_BLEND_ONE, D3D12_BLEND_ONE, D3D12_BLEND_OP_MIN,
			D3D12_BLEND_ZERO, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD, D3D12_COLOR_WRITE_ENABLE_RED);

		for (u32 ds = 0; ds < 2; ds++)
		{
			gpb.SetDepthStencilFormat(ds ? DXGI_FORMAT_D32_FLOAT_S8X24_UINT : DXGI_FORMAT_UNKNOWN);
			m_date_image_setup_pipelines[ds][datm] = gpb.Create(g_d3d12_context->GetDevice(), m_shader_cache, false);
			if (!m_date_image_setup_pipelines[ds][datm])
				return false;

			D3D12::SetObjectNameFormatted(m_date_image_setup_pipelines[ds][datm].get(), "DATE image clear pipeline (ds=%u, datm=%u)", ds, datm);
		}
	}

	return true;
}

bool GSDevice12::CompileInterlacePipelines()
{
	std::optional<std::string> source = Host::ReadResourceFileToString("shaders/dx11/interlace.fx");
	if (!source)
	{
		Host::ReportErrorAsync("GS", "Failed to read shaders/dx11/interlace.fx.");
		return false;
	}

	D3D12::GraphicsPipelineBuilder gpb;
	AddUtilityVertexAttributes(gpb);
	gpb.SetRootSignature(m_utility_root_signature.get());
	gpb.SetNoCullRasterizationState();
	gpb.SetNoDepthTestState();
	gpb.SetNoBlendingState();
	gpb.SetRenderTarget(0, DXGI_FORMAT_R8G8B8A8_UNORM);
	gpb.SetVertexShader(m_convert_vs.get());

	for (int i = 0; i < static_cast<int>(m_interlace.size()); i++)
	{
		ComPtr<ID3DBlob> ps(GetUtilityPixelShader(*source, StringUtil::StdStringFromFormat("ps_main%d", i).c_str()));
		if (!ps)
			return false;

		gpb.SetPixelShader(ps.get());

		m_interlace[i] = gpb.Create(g_d3d12_context->GetDevice(), m_shader_cache, false);
		if (!m_interlace[i])
			return false;

		D3D12::SetObjectNameFormatted(m_convert[i].get(), "Interlace pipeline %d", i);
	}

	return true;
}

bool GSDevice12::CompileMergePipelines()
{
	std::optional<std::string> shader = Host::ReadResourceFileToString("shaders/dx11/merge.fx");
	if (!shader)
	{
		Host::ReportErrorAsync("GS", "Failed to read shaders/dx11/merge.fx.");
		return false;
	}

	D3D12::GraphicsPipelineBuilder gpb;
	AddUtilityVertexAttributes(gpb);
	gpb.SetRootSignature(m_utility_root_signature.get());
	gpb.SetNoCullRasterizationState();
	gpb.SetNoDepthTestState();
	gpb.SetRenderTarget(0, DXGI_FORMAT_R8G8B8A8_UNORM);
	gpb.SetVertexShader(m_convert_vs.get());

	for (int i = 0; i < static_cast<int>(m_merge.size()); i++)
	{
		ComPtr<ID3DBlob> ps(GetUtilityPixelShader(*shader, StringUtil::StdStringFromFormat("ps_main%d", i).c_str()));
		if (!ps)
			return false;

		gpb.SetPixelShader(ps.get());
		gpb.SetBlendState(0, true, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA,
			D3D12_BLEND_OP_ADD, D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD);

		m_merge[i] = gpb.Create(g_d3d12_context->GetDevice(), m_shader_cache, false);
		if (!m_merge[i])
			return false;

		D3D12::SetObjectNameFormatted(m_convert[i].get(), "Merge pipeline %d", i);
	}

	return true;
}

bool GSDevice12::CompilePostProcessingPipelines()
{
	D3D12::GraphicsPipelineBuilder gpb;
	AddUtilityVertexAttributes(gpb);
	gpb.SetRootSignature(m_utility_root_signature.get());
	gpb.SetNoCullRasterizationState();
	gpb.SetNoDepthTestState();
	gpb.SetNoBlendingState();
	gpb.SetRenderTarget(0, DXGI_FORMAT_R8G8B8A8_UNORM);
	gpb.SetVertexShader(m_convert_vs.get());

	{
		std::optional<std::string> shader = Host::ReadResourceFileToString("shaders/common/fxaa.fx");
		if (!shader)
		{
			Host::ReportErrorAsync("GS", "Failed to read shaders/common/fxaa.fx.");
			return false;
		}

		ComPtr<ID3DBlob> ps(GetUtilityPixelShader(*shader, "ps_main"));
		if (!ps)
			return false;

		gpb.SetPixelShader(ps.get());

		m_fxaa_pipeline = gpb.Create(g_d3d12_context->GetDevice(), m_shader_cache, false);
		if (!m_fxaa_pipeline)
			return false;

		D3D12::SetObjectName(m_fxaa_pipeline.get(), "FXAA pipeline");
	}

	{
		std::optional<std::string> shader = Host::ReadResourceFileToString("shaders/dx11/shadeboost.fx");
		if (!shader)
		{
			Host::ReportErrorAsync("GS", "Failed to read shaders/dx11/shadeboost.fx.");
			return false;
		}

		ComPtr<ID3DBlob> ps(GetUtilityPixelShader(*shader, "ps_main"));
		if (!ps)
			return false;

		gpb.SetPixelShader(ps.get());

		m_shadeboost_pipeline = gpb.Create(g_d3d12_context->GetDevice(), m_shader_cache, false);
		if (!m_shadeboost_pipeline)
			return false;

		D3D12::SetObjectName(m_shadeboost_pipeline.get(), "Shadeboost pipeline");
	}

	return true;
}

bool GSDevice12::CheckStagingBufferSize(u32 required_size)
{
	if (m_readback_staging_buffer_size >= required_size)
		return true;

	DestroyStagingBuffer();

	D3D12MA::ALLOCATION_DESC allocation_desc = {};
	allocation_desc.HeapType = D3D12_HEAP_TYPE_READBACK;

	const D3D12_RESOURCE_DESC resource_desc = {
		D3D12_RESOURCE_DIMENSION_BUFFER, 0, required_size, 1, 1, 1, DXGI_FORMAT_UNKNOWN, {1, 0}, D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		D3D12_RESOURCE_FLAG_NONE};

	HRESULT hr = g_d3d12_context->GetAllocator()->CreateResource(&allocation_desc, &resource_desc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, m_readback_staging_allocation.put(), IID_PPV_ARGS(m_readback_staging_buffer.put()));
	if (FAILED(hr))
	{
		Console.Error("(GSDevice12::CheckStagingBufferSize) CreateResource() failed with HRESULT %08X", hr);
		return false;
	}

	return true;
}

bool GSDevice12::MapStagingBuffer(u32 size_to_read)
{
	if (m_readback_staging_buffer_map)
		return true;

	const D3D12_RANGE range = {0, size_to_read};
	const HRESULT hr = m_readback_staging_buffer->Map(0, &range, &m_readback_staging_buffer_map);
	if (FAILED(hr))
	{
		Console.Error("(GSDevice12::MapStagingBuffer) Map() failed with HRESULT %08X", hr);
		return false;
	}

	return true;
}

void GSDevice12::UnmapStagingBuffer()
{
	if (!m_readback_staging_buffer_map)
		return;

	const D3D12_RANGE write_range = {};
	m_readback_staging_buffer->Unmap(0, &write_range);
	m_readback_staging_buffer_map = nullptr;
}

void GSDevice12::DestroyStagingBuffer()
{
	UnmapStagingBuffer();

	// safe to immediately destroy, since the GPU doesn't write to it without a copy+exec.
	m_readback_staging_buffer_size = 0;
	m_readback_staging_allocation.reset();
	m_readback_staging_buffer.reset();
}

void GSDevice12::DestroyResources()
{
	g_d3d12_context->ExecuteCommandList(true);

	for (auto& it : m_tfx_pipelines)
		g_d3d12_context->DeferObjectDestruction(it.second.get());
	m_tfx_pipelines.clear();
	m_tfx_pixel_shaders.clear();
	m_tfx_geometry_shaders.clear();
	m_tfx_vertex_shaders.clear();
	m_interlace = {};
	m_merge = {};
	m_color_copy = {};
	m_present = {};
	m_convert = {};
	m_hdr_setup_pipelines = {};
	m_hdr_finish_pipelines = {};
	m_date_image_setup_pipelines = {};
	m_fxaa_pipeline.reset();
	m_shadeboost_pipeline.reset();

	m_linear_sampler_cpu.Clear();
	m_point_sampler_cpu.Clear();

	for (auto& it : m_samplers)
		g_d3d12_context->DeferDescriptorDestruction(g_d3d12_context->GetSamplerHeapManager(), &it.second);
	g_d3d12_context->DeferDescriptorDestruction(g_d3d12_context->GetSamplerHeapManager(), &m_linear_sampler_cpu);
	g_d3d12_context->DeferDescriptorDestruction(g_d3d12_context->GetSamplerHeapManager(), &m_point_sampler_cpu);
	g_d3d12_context->InvalidateSamplerGroups();

	DestroyStagingBuffer();

	m_pixel_constant_buffer.Destroy(false);
	m_vertex_constant_buffer.Destroy(false);
	m_index_stream_buffer.Destroy(false);
	m_vertex_stream_buffer.Destroy(false);

	m_utility_root_signature.reset();
	m_tfx_root_signature.reset();

	m_null_texture.Destroy(false);
}

const ID3DBlob* GSDevice12::GetTFXVertexShader(GSHWDrawConfig::VSSelector sel)
{
	auto it = m_tfx_vertex_shaders.find(sel.key);
	if (it != m_tfx_vertex_shaders.end())
		return it->second.get();

	ShaderMacro sm(m_shader_cache.GetFeatureLevel());
	sm.AddMacro("VS_TME", sel.tme);
	sm.AddMacro("VS_FST", sel.fst);
	sm.AddMacro("VS_IIP", sel.iip);

	ComPtr<ID3DBlob> vs(m_shader_cache.GetVertexShader(m_tfx_source, sm.GetPtr(), "vs_main"));
	it = m_tfx_vertex_shaders.emplace(sel.key, std::move(vs)).first;
	return it->second.get();
}

const ID3DBlob* GSDevice12::GetTFXGeometryShader(GSHWDrawConfig::GSSelector sel)
{
	auto it = m_tfx_geometry_shaders.find(sel.key);
	if (it != m_tfx_geometry_shaders.end())
		return it->second.get();

	ShaderMacro sm(m_shader_cache.GetFeatureLevel());
	sm.AddMacro("GS_IIP", sel.iip);
	sm.AddMacro("GS_PRIM", static_cast<int>(sel.topology));
	sm.AddMacro("GS_EXPAND", sel.expand);

	ComPtr<ID3DBlob> gs(m_shader_cache.GetGeometryShader(m_tfx_source, sm.GetPtr(), "gs_main"));
	it = m_tfx_geometry_shaders.emplace(sel.key, std::move(gs)).first;
	return it->second.get();
}

const ID3DBlob* GSDevice12::GetTFXPixelShader(const GSHWDrawConfig::PSSelector& sel)
{
	auto it = m_tfx_pixel_shaders.find(sel);
	if (it != m_tfx_pixel_shaders.end())
		return it->second.get();

	ShaderMacro sm(m_shader_cache.GetFeatureLevel());
	sm.AddMacro("PS_SCALE_FACTOR", std::max(1u, GSConfig.UpscaleMultiplier));
	sm.AddMacro("PS_FST", sel.fst);
	sm.AddMacro("PS_WMS", sel.wms);
	sm.AddMacro("PS_WMT", sel.wmt);
	sm.AddMacro("PS_AEM_FMT", sel.aem_fmt);
	sm.AddMacro("PS_AEM", sel.aem);
	sm.AddMacro("PS_TFX", sel.tfx);
	sm.AddMacro("PS_TCC", sel.tcc);
	sm.AddMacro("PS_DATE", sel.date);
	sm.AddMacro("PS_ATST", sel.atst);
	sm.AddMacro("PS_FOG", sel.fog);
	sm.AddMacro("PS_IIP", sel.iip);
	sm.AddMacro("PS_CLR_HW", sel.clr_hw);
	sm.AddMacro("PS_FBA", sel.fba);
	sm.AddMacro("PS_FBMASK", sel.fbmask);
	sm.AddMacro("PS_LTF", sel.ltf);
	sm.AddMacro("PS_TCOFFSETHACK", sel.tcoffsethack);
	sm.AddMacro("PS_POINT_SAMPLER", sel.point_sampler);
	sm.AddMacro("PS_SHUFFLE", sel.shuffle);
	sm.AddMacro("PS_READ_BA", sel.read_ba);
	sm.AddMacro("PS_CHANNEL_FETCH", sel.channel);
	sm.AddMacro("PS_TALES_OF_ABYSS_HLE", sel.tales_of_abyss_hle);
	sm.AddMacro("PS_URBAN_CHAOS_HLE", sel.urban_chaos_hle);
	sm.AddMacro("PS_DFMT", sel.dfmt);
	sm.AddMacro("PS_DEPTH_FMT", sel.depth_fmt);
	sm.AddMacro("PS_PAL_FMT", sel.pal_fmt);
	sm.AddMacro("PS_INVALID_TEX0", sel.invalid_tex0);
	sm.AddMacro("PS_HDR", sel.hdr);
	sm.AddMacro("PS_COLCLIP", sel.colclip);
	sm.AddMacro("PS_BLEND_A", sel.blend_a);
	sm.AddMacro("PS_BLEND_B", sel.blend_b);
	sm.AddMacro("PS_BLEND_C", sel.blend_c);
	sm.AddMacro("PS_BLEND_D", sel.blend_d);
	sm.AddMacro("PS_BLEND_MIX", sel.blend_mix);
	sm.AddMacro("PS_PABE", sel.pabe);
	sm.AddMacro("PS_DITHER", sel.dither);
	sm.AddMacro("PS_ZCLAMP", sel.zclamp);
	sm.AddMacro("PS_SCANMSK", sel.scanmsk);
	sm.AddMacro("PS_AUTOMATIC_LOD", sel.automatic_lod);
	sm.AddMacro("PS_MANUAL_LOD", sel.manual_lod);
	sm.AddMacro("PS_TEX_IS_FB", sel.tex_is_fb);
	sm.AddMacro("PS_NO_COLOR", sel.no_color);
	sm.AddMacro("PS_NO_COLOR1", sel.no_color1);
	sm.AddMacro("PS_NO_ABLEND", sel.no_ablend);
	sm.AddMacro("PS_ONLY_ALPHA", sel.only_alpha);

	ComPtr<ID3DBlob> ps(m_shader_cache.GetPixelShader(m_tfx_source, sm.GetPtr(), "ps_main"));
	it = m_tfx_pixel_shaders.emplace(sel, std::move(ps)).first;
	return it->second.get();
}

GSDevice12::ComPtr<ID3D12PipelineState> GSDevice12::CreateTFXPipeline(const PipelineSelector& p)
{
	static constexpr std::array<D3D12_PRIMITIVE_TOPOLOGY_TYPE, 3> topology_lookup = {{
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT, // Point
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE, // Line
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE, // Triangle
	}};

	GSHWDrawConfig::BlendState pbs{p.bs};
	GSHWDrawConfig::PSSelector pps{p.ps};
	if ((p.cms.wrgba & 0x7) == 0)
	{
		// disable blending when colours are masked
		pbs = {};
		pps.no_color1 = true;
	}

	const ID3DBlob* vs = GetTFXVertexShader(p.vs);
	const ID3DBlob* gs = p.gs.expand ? GetTFXGeometryShader(p.gs) : nullptr;
	const ID3DBlob* ps = GetTFXPixelShader(pps);
	if (!vs || (p.gs.expand && !gs) || !ps)
		return nullptr;

	// Common state
	D3D12::GraphicsPipelineBuilder gpb;
	gpb.SetRootSignature(m_tfx_root_signature.get());
	gpb.SetPrimitiveTopologyType(topology_lookup[p.topology]);
	gpb.SetRasterizationState(D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_NONE, false);
	if (p.rt)
	{
		gpb.SetRenderTarget(0,
			(p.ps.date >= 10) ? DXGI_FORMAT_R32_FLOAT :
                                (p.ps.hdr ? DXGI_FORMAT_R32G32B32A32_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM));
	}
	if (p.ds)
		gpb.SetDepthStencilFormat(DXGI_FORMAT_D32_FLOAT_S8X24_UINT);

	// Shaders
	gpb.SetVertexShader(vs);
	if (gs)
		gpb.SetGeometryShader(gs);
	gpb.SetPixelShader(ps);

	// IA
	gpb.AddVertexAttribute("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0);
	gpb.AddVertexAttribute("COLOR", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 8);
	gpb.AddVertexAttribute("TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT, 0, 12);
	gpb.AddVertexAttribute("POSITION", 0, DXGI_FORMAT_R16G16_UINT, 0, 16);
	gpb.AddVertexAttribute("POSITION", 1, DXGI_FORMAT_R32_UINT, 0, 20);
	gpb.AddVertexAttribute("TEXCOORD", 2, DXGI_FORMAT_R16G16_UINT, 0, 24);
	gpb.AddVertexAttribute("COLOR", 1, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 28);

	// DepthStencil
	if (p.ds)
	{
		static const D3D12_COMPARISON_FUNC ztst[] = {
			D3D12_COMPARISON_FUNC_NEVER, D3D12_COMPARISON_FUNC_ALWAYS, D3D12_COMPARISON_FUNC_GREATER_EQUAL, D3D12_COMPARISON_FUNC_GREATER};
		gpb.SetDepthState((p.dss.ztst != ZTST_ALWAYS || p.dss.zwe), p.dss.zwe, ztst[p.dss.ztst]);
		if (p.dss.date)
		{
			const D3D12_DEPTH_STENCILOP_DESC sos{D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP,
				p.dss.date_one ? D3D12_STENCIL_OP_ZERO : D3D12_STENCIL_OP_KEEP,
				D3D12_COMPARISON_FUNC_EQUAL};
			gpb.SetStencilState(true, 1, 1, sos, sos);
		}
	}
	else
	{
		gpb.SetNoDepthTestState();
	}

	// Blending
	if (p.ps.date >= 10)
	{
		// image DATE prepass
		gpb.SetBlendState(0, true, D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_MIN, D3D12_BLEND_ONE,
			D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD, D3D12_COLOR_WRITE_ENABLE_RED);
	}
	else if (pbs.enable)
	{
		// clang-format off
		static constexpr std::array<D3D12_BLEND, 16> d3d_blend_factors = { {
			D3D12_BLEND_SRC_COLOR, D3D12_BLEND_INV_SRC_COLOR, D3D12_BLEND_DEST_COLOR, D3D12_BLEND_INV_DEST_COLOR,
			D3D12_BLEND_SRC1_COLOR, D3D12_BLEND_INV_SRC1_COLOR, D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA,
			D3D12_BLEND_DEST_ALPHA, D3D12_BLEND_INV_DEST_ALPHA, D3D12_BLEND_SRC1_ALPHA, D3D12_BLEND_INV_SRC1_ALPHA,
			D3D12_BLEND_BLEND_FACTOR, D3D12_BLEND_INV_BLEND_FACTOR, D3D12_BLEND_ONE, D3D12_BLEND_ZERO
		} };
		static constexpr std::array<D3D12_BLEND_OP, 3> d3d_blend_ops = { {
			D3D12_BLEND_OP_ADD, D3D12_BLEND_OP_SUBTRACT, D3D12_BLEND_OP_REV_SUBTRACT
		} };
		// clang-format on

		gpb.SetBlendState(0, true, d3d_blend_factors[pbs.src_factor], d3d_blend_factors[pbs.dst_factor],
			d3d_blend_ops[pbs.op], D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD, p.cms.wrgba);
	}
	else
	{
		gpb.SetBlendState(0, false, D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD, p.cms.wrgba);
	}

	ComPtr<ID3D12PipelineState> pipeline(gpb.Create(g_d3d12_context->GetDevice(), m_shader_cache));
	if (pipeline)
	{
		D3D12::SetObjectNameFormatted(
			pipeline.get(), "TFX Pipeline %08X/%08X/%" PRIX64 "%08X", p.vs.key, p.gs.key, p.ps.key_hi, p.ps.key_lo);
	}

	return pipeline;
}

const ID3D12PipelineState* GSDevice12::GetTFXPipeline(const PipelineSelector& p)
{
	auto it = m_tfx_pipelines.find(p);
	if (it != m_tfx_pipelines.end())
		return it->second.get();

	ComPtr<ID3D12PipelineState> pipeline(CreateTFXPipeline(p));
	it = m_tfx_pipelines.emplace(p, std::move(pipeline)).first;
	return it->second.get();
}

bool GSDevice12::BindDrawPipeline(const PipelineSelector& p)
{
	const ID3D12PipelineState* pipeline = GetTFXPipeline(p);
	if (!pipeline)
		return false;

	SetPipeline(pipeline);

	return ApplyTFXState();
}

void GSDevice12::InitializeState()
{
	for (u32 i = 0; i < NUM_TOTAL_TFX_TEXTURES; i++)
		m_tfx_textures[i] = m_null_texture.GetSRVDescriptor();
	for (u32 i = 0; i < NUM_TFX_SAMPLERS; i++)
		m_tfx_sampler_sel[i] = GSHWDrawConfig::SamplerSelector::Point().key;

	InvalidateCachedState();
}

void GSDevice12::InitializeSamplers()
{
	bool result = GetSampler(&m_point_sampler_cpu, GSHWDrawConfig::SamplerSelector::Point());
	result = result && GetSampler(&m_linear_sampler_cpu, GSHWDrawConfig::SamplerSelector::Linear());

	for (u32 i = 0; i < NUM_TFX_SAMPLERS; i++)
		result = result && GetSampler(&m_tfx_samplers[i], m_tfx_sampler_sel[i]);

	if (!result)
		pxFailRel("Failed to initialize samplers");
}

void GSDevice12::ExecuteCommandList(bool wait_for_completion)
{
	EndRenderPass();
	g_d3d12_context->ExecuteCommandList(wait_for_completion);
	InvalidateCachedState();
}

void GSDevice12::ExecuteCommandList(bool wait_for_completion, const char* reason, ...)
{
	std::va_list ap;
	va_start(ap, reason);
	const std::string reason_str(StringUtil::StdStringFromFormatV(reason, ap));
	va_end(ap);

	Console.Warning("D3D12: Executing command buffer due to '%s'", reason_str.c_str());
	ExecuteCommandList(wait_for_completion);
}

void GSDevice12::ExecuteCommandListAndRestartRenderPass(const char* reason)
{
	Console.Warning("Vulkan: Executing command buffer due to '%s'", reason);

	const bool was_in_render_pass = m_in_render_pass;
	EndRenderPass();
	g_d3d12_context->ExecuteCommandList(false);
	InvalidateCachedState();

	if (was_in_render_pass)
	{
		// rebind everything except RT, because the RP does that for us
		ApplyBaseState(m_dirty_flags & ~DIRTY_FLAG_RENDER_TARGET, g_d3d12_context->GetCommandList());
		m_dirty_flags &= ~DIRTY_BASE_STATE;

		// restart render pass
		BeginRenderPass(
			m_current_render_target ? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS,
			m_current_render_target ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
			m_current_depth_target ? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS,
			m_current_depth_target ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS);
	}
}

void GSDevice12::InvalidateCachedState()
{
	m_dirty_flags |= DIRTY_BASE_STATE | DIRTY_TFX_STATE | DIRTY_UTILITY_STATE | DIRTY_CONSTANT_BUFFER_STATE;
	m_current_root_signature = RootSignature::Undefined;
	m_utility_texture_cpu.Clear();
	m_utility_texture_gpu.Clear();
	m_utility_sampler_cpu.Clear();
	m_utility_sampler_gpu.Clear();
	m_tfx_textures_handle_gpu.Clear();
	m_tfx_samplers_handle_gpu.Clear();
	m_tfx_rt_textures_handle_gpu.Clear();
}

void GSDevice12::SetVertexBuffer(D3D12_GPU_VIRTUAL_ADDRESS buffer, size_t size, size_t stride)
{
	if (m_vertex_buffer.BufferLocation == buffer && m_vertex_buffer.SizeInBytes == size && m_vertex_buffer.StrideInBytes == stride)
		return;

	m_vertex_buffer.BufferLocation = buffer;
	m_vertex_buffer.SizeInBytes = size;
	m_vertex_buffer.StrideInBytes = stride;
	m_dirty_flags |= DIRTY_FLAG_VERTEX_BUFFER;
}

void GSDevice12::SetIndexBuffer(D3D12_GPU_VIRTUAL_ADDRESS buffer, size_t size, DXGI_FORMAT type)
{
	if (m_index_buffer.BufferLocation == buffer && m_index_buffer.SizeInBytes == size && m_index_buffer.Format == type)
		return;

	m_index_buffer.BufferLocation = buffer;
	m_index_buffer.SizeInBytes = size;
	m_index_buffer.Format = type;
	m_dirty_flags |= DIRTY_FLAG_INDEX_BUFFER;
}

void GSDevice12::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY topology)
{
	if (m_primitive_topology == topology)
		return;

	m_primitive_topology = topology;
	m_dirty_flags |= DIRTY_FLAG_PRIMITIVE_TOPOLOGY;
}

void GSDevice12::SetBlendConstants(u8 color)
{
	if (m_blend_constant_color == color)
		return;

	m_blend_constant_color = color;
	m_dirty_flags |= DIRTY_FLAG_BLEND_CONSTANTS;
}

void GSDevice12::SetStencilRef(u8 ref)
{
	if (m_stencil_ref == ref)
		return;

	m_stencil_ref = ref;
	m_dirty_flags |= DIRTY_FLAG_STENCIL_REF;
}

void GSDevice12::PSSetShaderResource(int i, GSTexture* sr, bool check_state)
{
	D3D12::DescriptorHandle handle;
	if (sr)
	{
		GSTexture12* dtex = static_cast<GSTexture12*>(sr);
		if (check_state)
		{
			if (dtex->GetTexture().GetState() != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE && InRenderPass())
			{
				// Console.Warning("Ending render pass due to resource transition");
				EndRenderPass();
			}

			dtex->CommitClear();
			dtex->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
		dtex->SetUsedThisCommandBuffer();
		handle = dtex->GetSRVDescriptor();
	}
	else
	{
		handle = m_null_texture.GetSRVDescriptor();
	}

	if (m_tfx_textures[i] == handle)
		return;

	m_tfx_textures[i] = handle;
	m_dirty_flags |= (i < 2) ? DIRTY_FLAG_TFX_TEXTURES : DIRTY_FLAG_TFX_RT_TEXTURES;
}

void GSDevice12::PSSetSampler(u32 index, GSHWDrawConfig::SamplerSelector sel)
{
	if (m_tfx_sampler_sel[index] == sel.key)
		return;

	GetSampler(&m_tfx_samplers[index], sel);
	m_tfx_sampler_sel[index] = sel.key;
	m_dirty_flags |= DIRTY_FLAG_TFX_SAMPLERS;
}

void GSDevice12::SetUtilityRootSignature()
{
	if (m_current_root_signature == RootSignature::Utility)
		return;

	m_current_root_signature = RootSignature::Utility;
	m_dirty_flags |= DIRTY_FLAG_TEXTURES_DESCRIPTOR_TABLE | DIRTY_FLAG_SAMPLERS_DESCRIPTOR_TABLE | DIRTY_FLAG_PIPELINE;
	g_d3d12_context->GetCommandList()->SetGraphicsRootSignature(m_utility_root_signature.get());
}

void GSDevice12::SetUtilityTexture(GSTexture* dtex, const D3D12::DescriptorHandle& sampler)
{
	D3D12::DescriptorHandle handle;
	if (dtex)
	{
		GSTexture12* d12tex = static_cast<GSTexture12*>(dtex);
		d12tex->CommitClear();
		d12tex->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		d12tex->SetUsedThisCommandBuffer();
		handle = d12tex->GetSRVDescriptor();
	}
	else
	{
		handle = m_null_texture.GetSRVDescriptor();
	}

	if (m_utility_texture_cpu != handle)
	{
		m_utility_texture_cpu = handle;
		m_dirty_flags |= DIRTY_FLAG_TEXTURES_DESCRIPTOR_TABLE;

		if (!GetTextureGroupDescriptors(&m_utility_texture_gpu, &handle, 1))
		{
			ExecuteCommandListAndRestartRenderPass("Ran out of utility texture descriptors");
			SetUtilityTexture(dtex, sampler);
			return;
		}
	}

	if (m_utility_sampler_gpu != sampler)
	{
		m_utility_sampler_cpu = sampler;
		m_dirty_flags |= DIRTY_FLAG_SAMPLERS_DESCRIPTOR_TABLE;

		if (!g_d3d12_context->GetSamplerAllocator().LookupSingle(&m_utility_sampler_gpu, sampler))
		{
			ExecuteCommandListAndRestartRenderPass("Ran out of utility sampler descriptors");
			SetUtilityTexture(dtex, sampler);
			return;
		}
	}
}

void GSDevice12::SetUtilityPushConstants(const void* data, u32 size)
{
	g_d3d12_context->GetCommandList()->SetGraphicsRoot32BitConstants(UTILITY_ROOT_SIGNATURE_PARAM_PUSH_CONSTANTS, (size + 3) / sizeof(u32), data, 0);
}

void GSDevice12::UnbindTexture(GSTexture12* tex)
{
	for (u32 i = 0; i < NUM_TOTAL_TFX_TEXTURES; i++)
	{
		if (m_tfx_textures[i] == tex->GetSRVDescriptor())
		{
			m_tfx_textures[i] = m_null_texture.GetSRVDescriptor();
			m_dirty_flags |= DIRTY_FLAG_TFX_TEXTURES;
		}
	}
	if (m_current_render_target == tex)
	{
		EndRenderPass();
		m_current_render_target = nullptr;
	}
	if (m_current_depth_target == tex)
	{
		EndRenderPass();
		m_current_depth_target = nullptr;
	}
}

void GSDevice12::RenderTextureMipmap(const D3D12::Texture& texture,
	u32 dst_level, u32 dst_width, u32 dst_height, u32 src_level, u32 src_width, u32 src_height)
{
	EndRenderPass();

	// we need a temporary SRV and RTV for each mip level
	// Safe to use the init buffer after exec, because everything will be done with the texture.
	D3D12::DescriptorHandle rtv_handle;
	while (!g_d3d12_context->GetRTVHeapManager().Allocate(&rtv_handle))
		ExecuteCommandList(false);

	D3D12::DescriptorHandle srv_handle;
	while (!g_d3d12_context->GetDescriptorHeapManager().Allocate(&srv_handle))
		ExecuteCommandList(false);

	// Setup views. This will be a partial view for the SRV.
	D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {texture.GetFormat(), D3D12_RTV_DIMENSION_TEXTURE2D};
	rtv_desc.Texture2D = {dst_level, 0u};
	g_d3d12_context->GetDevice()->CreateRenderTargetView(texture.GetResource(), &rtv_desc, rtv_handle);

	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {texture.GetFormat(), D3D12_SRV_DIMENSION_TEXTURE2D, D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING};
	srv_desc.Texture2D = {src_level, 1u, 0u, 0.0f};
	g_d3d12_context->GetDevice()->CreateShaderResourceView(texture.GetResource(), &srv_desc, srv_handle);

	// We need to set the descriptors up manually, because we're not going through GSTexture.
	if (!GetTextureGroupDescriptors(&m_utility_texture_gpu, &srv_handle, 1))
		ExecuteCommandList(false);
	if (m_utility_sampler_cpu != m_linear_sampler_cpu)
	{
		m_dirty_flags |= DIRTY_FLAG_SAMPLERS_DESCRIPTOR_TABLE;
		if (!g_d3d12_context->GetSamplerAllocator().LookupSingle(&m_utility_sampler_gpu, m_linear_sampler_cpu))
			ExecuteCommandList(false);
	}

	// *now* we don't have to worry about running out of anything.
	ID3D12GraphicsCommandList* cmdlist = g_d3d12_context->GetCommandList();
	if (texture.GetState() != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		texture.TransitionSubresourceToState(cmdlist, src_level, texture.GetState(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	if (texture.GetState() != D3D12_RESOURCE_STATE_RENDER_TARGET)
		texture.TransitionSubresourceToState(cmdlist, dst_level, texture.GetState(), D3D12_RESOURCE_STATE_RENDER_TARGET);

	// We set the state directly here.
	constexpr u32 MODIFIED_STATE = DIRTY_FLAG_VIEWPORT | DIRTY_FLAG_SCISSOR | DIRTY_FLAG_RENDER_TARGET;
	m_dirty_flags &= ~MODIFIED_STATE;

	// Using a render pass is probably a bit overkill.
	const D3D12_DISCARD_REGION discard_region = {0u, nullptr, dst_level, 1u};
	cmdlist->DiscardResource(texture.GetResource(), &discard_region);
	cmdlist->OMSetRenderTargets(1, &rtv_handle.cpu_handle, FALSE, nullptr);

	const D3D12_VIEWPORT vp = {0.0f, 0.0f, static_cast<float>(dst_width), static_cast<float>(dst_height), 0.0f, 1.0f};
	cmdlist->RSSetViewports(1, &vp);

	const D3D12_RECT scissor = {0, 0, static_cast<LONG>(dst_width), static_cast<LONG>(dst_height)};
	cmdlist->RSSetScissorRects(1, &scissor);

	SetUtilityRootSignature();
	SetPipeline(m_convert[static_cast<int>(ShaderConvert::COPY)].get());
	DrawStretchRect(GSVector4(0.0f, 0.0f, 1.0f, 1.0f),
		GSVector4(0.0f, 0.0f, static_cast<float>(dst_width), static_cast<float>(dst_height)),
		GSVector2i(dst_width, dst_height));

	if (texture.GetState() != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		texture.TransitionSubresourceToState(cmdlist, src_level, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, texture.GetState());
	if (texture.GetState() != D3D12_RESOURCE_STATE_RENDER_TARGET)
		texture.TransitionSubresourceToState(cmdlist, dst_level, D3D12_RESOURCE_STATE_RENDER_TARGET, texture.GetState());

	// Must destroy after current cmdlist.
	g_d3d12_context->DeferDescriptorDestruction(g_d3d12_context->GetDescriptorHeapManager(), &srv_handle);
	g_d3d12_context->DeferDescriptorDestruction(g_d3d12_context->GetRTVHeapManager(), &rtv_handle);

	// Restore for next normal draw.
	m_dirty_flags |= MODIFIED_STATE;
}

bool GSDevice12::InRenderPass()
{
	return m_in_render_pass;
}

void GSDevice12::BeginRenderPass(
	D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE color_begin, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE color_end,
	D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE depth_begin, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE depth_end,
	D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE stencil_begin, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE stencil_end,
	const GSVector4& clear_color /* = GSVector4::zero() */, float clear_depth /* = 0.0f */,
	u8 clear_stencil /* = 0 */)
{
	if (m_in_render_pass)
		EndRenderPass();

	// we're setting the RT here.
	m_dirty_flags &= ~DIRTY_FLAG_RENDER_TARGET;
	m_in_render_pass = true;

	D3D12_RENDER_PASS_RENDER_TARGET_DESC rt = {};
	if (m_current_render_target)
	{
		rt.cpuDescriptor = m_current_render_target->GetRTVOrDSVHandle();
		rt.EndingAccess.Type = color_end;
		rt.BeginningAccess.Type = color_begin;
		if (color_begin == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR)
		{
			LookupNativeFormat(m_current_render_target->GetFormat(), nullptr, &rt.BeginningAccess.Clear.ClearValue.Format, nullptr, nullptr);
			GSVector4::store<false>(rt.BeginningAccess.Clear.ClearValue.Color, clear_color);
		}
	}

	D3D12_RENDER_PASS_DEPTH_STENCIL_DESC ds = {};
	if (m_current_depth_target)
	{
		ds.cpuDescriptor = m_current_depth_target->GetRTVOrDSVHandle();
		ds.DepthEndingAccess.Type = depth_end;
		ds.DepthBeginningAccess.Type = depth_begin;
		if (depth_begin == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR)
		{
			LookupNativeFormat(m_current_depth_target->GetFormat(), nullptr, nullptr, nullptr, &ds.DepthBeginningAccess.Clear.ClearValue.Format);
			ds.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth = clear_depth;
		}
		ds.StencilEndingAccess.Type = stencil_end;
		ds.StencilBeginningAccess.Type = stencil_begin;
		if (stencil_begin == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR)
		{
			LookupNativeFormat(m_current_depth_target->GetFormat(), nullptr, nullptr, nullptr, &ds.StencilBeginningAccess.Clear.ClearValue.Format);
			ds.StencilBeginningAccess.Clear.ClearValue.DepthStencil.Stencil = clear_stencil;
		}
	}

	g_d3d12_context->GetCommandList()->BeginRenderPass(
		m_current_render_target ? 1 : 0, m_current_render_target ? &rt : nullptr,
		m_current_depth_target ? &ds : nullptr, D3D12_RENDER_PASS_FLAG_NONE);
}

void GSDevice12::EndRenderPass()
{
	if (!m_in_render_pass)
		return;

	g_d3d12_context->GetCommandList()->EndRenderPass();
	m_in_render_pass = false;

	// to render again, we need to reset OM
	m_dirty_flags |= DIRTY_FLAG_RENDER_TARGET;
}

void GSDevice12::SetViewport(const D3D12_VIEWPORT& viewport)
{
	if (std::memcmp(&viewport, &m_viewport, sizeof(m_viewport)) == 0)
		return;

	std::memcpy(&m_viewport, &viewport, sizeof(m_viewport));
	m_dirty_flags |= DIRTY_FLAG_VIEWPORT;
}

void GSDevice12::SetScissor(const GSVector4i& scissor)
{
	if (m_scissor.eq(scissor))
		return;

	m_scissor = scissor;
	m_dirty_flags |= DIRTY_FLAG_SCISSOR;
}

void GSDevice12::SetPipeline(const ID3D12PipelineState* pipeline)
{
	if (m_current_pipeline == pipeline)
		return;

	m_current_pipeline = pipeline;
	m_dirty_flags |= DIRTY_FLAG_PIPELINE;
}

__ri void GSDevice12::ApplyBaseState(u32 flags, ID3D12GraphicsCommandList* cmdlist)
{
	if (flags & DIRTY_FLAG_VERTEX_BUFFER)
		cmdlist->IASetVertexBuffers(0, 1, &m_vertex_buffer);

	if (flags & DIRTY_FLAG_INDEX_BUFFER)
		cmdlist->IASetIndexBuffer(&m_index_buffer);

	if (flags & DIRTY_FLAG_PRIMITIVE_TOPOLOGY)
		cmdlist->IASetPrimitiveTopology(m_primitive_topology);

	if (flags & DIRTY_FLAG_PIPELINE)
		cmdlist->SetPipelineState(const_cast<ID3D12PipelineState*>(m_current_pipeline));

	if (flags & DIRTY_FLAG_VIEWPORT)
		cmdlist->RSSetViewports(1, &m_viewport);

	if (flags & DIRTY_FLAG_SCISSOR)
	{
		const D3D12_RECT rc{m_scissor.x, m_scissor.y, m_scissor.z, m_scissor.w};
		cmdlist->RSSetScissorRects(1, &rc);
	}

	if (flags & DIRTY_FLAG_BLEND_CONSTANTS)
	{
		const GSVector4 col(static_cast<float>(m_blend_constant_color) / 128.0f);
		cmdlist->OMSetBlendFactor(col.v);
	}

	if (flags & DIRTY_FLAG_STENCIL_REF)
		cmdlist->OMSetStencilRef(m_stencil_ref);

	if (flags & DIRTY_FLAG_RENDER_TARGET)
	{
		if (m_current_render_target)
		{
			cmdlist->OMSetRenderTargets(1, &m_current_render_target->GetRTVOrDSVHandle().cpu_handle, FALSE,
				m_current_depth_target ? &m_current_depth_target->GetRTVOrDSVHandle().cpu_handle : nullptr);
		}
		else if (m_current_depth_target)
		{
			cmdlist->OMSetRenderTargets(0, nullptr, FALSE, &m_current_depth_target->GetRTVOrDSVHandle().cpu_handle);
		}
	}
}

bool GSDevice12::ApplyTFXState(bool already_execed)
{
	if (m_current_root_signature == RootSignature::TFX && m_dirty_flags == 0)
		return true;

	u32 flags = m_dirty_flags;
	m_dirty_flags &= ~DIRTY_TFX_STATE | DIRTY_CONSTANT_BUFFER_STATE;

	// do cbuffer first, because it's the most likely to cause an exec
	if (flags & DIRTY_FLAG_VS_CONSTANT_BUFFER)
	{
		if (!m_vertex_constant_buffer.ReserveMemory(
				sizeof(m_vs_cb_cache), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT))
		{
			if (already_execed)
			{
				Console.Error("Failed to reserve vertex uniform space");
				return false;
			}

			ExecuteCommandListAndRestartRenderPass("Ran out of vertex uniform space");
			return ApplyTFXState(true);
		}

		std::memcpy(m_vertex_constant_buffer.GetCurrentHostPointer(), &m_vs_cb_cache, sizeof(m_vs_cb_cache));
		m_tfx_constant_buffers[0] = m_vertex_constant_buffer.GetCurrentGPUPointer();
		m_vertex_constant_buffer.CommitMemory(sizeof(m_vs_cb_cache));
		flags |= DIRTY_FLAG_VS_CONSTANT_BUFFER_BINDING;
	}

	if (flags & DIRTY_FLAG_PS_CONSTANT_BUFFER)
	{
		if (!m_pixel_constant_buffer.ReserveMemory(
				sizeof(m_ps_cb_cache), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT))
		{
			if (already_execed)
			{
				Console.Error("Failed to reserve pixel uniform space");
				return false;
			}

			ExecuteCommandListAndRestartRenderPass("Ran out of pixel uniform space");
			return ApplyTFXState(true);
		}

		std::memcpy(m_pixel_constant_buffer.GetCurrentHostPointer(), &m_ps_cb_cache, sizeof(m_ps_cb_cache));
		m_tfx_constant_buffers[1] = m_pixel_constant_buffer.GetCurrentGPUPointer();
		m_pixel_constant_buffer.CommitMemory(sizeof(m_ps_cb_cache));
		flags |= DIRTY_FLAG_PS_CONSTANT_BUFFER_BINDING;
	}

	if (flags & DIRTY_FLAG_TFX_SAMPLERS)
	{
		if (!g_d3d12_context->GetSamplerAllocator().LookupGroup(&m_tfx_samplers_handle_gpu, m_tfx_samplers.data()))
		{
			ExecuteCommandListAndRestartRenderPass("Ran out of sampler groups");
			return ApplyTFXState(true);
		}

		flags |= DIRTY_FLAG_SAMPLERS_DESCRIPTOR_TABLE;
	}

	if (flags & DIRTY_FLAG_TFX_TEXTURES)
	{
		if (!GetTextureGroupDescriptors(&m_tfx_textures_handle_gpu, m_tfx_textures.data(), 2))
		{
			ExecuteCommandListAndRestartRenderPass("Ran out of TFX texture descriptor groups");
			return ApplyTFXState(true);
		}

		flags |= DIRTY_FLAG_TEXTURES_DESCRIPTOR_TABLE;
	}

	if (flags & DIRTY_FLAG_TFX_RT_TEXTURES)
	{
		if (!GetTextureGroupDescriptors(&m_tfx_rt_textures_handle_gpu, m_tfx_textures.data() + 2, 2))
		{
			ExecuteCommandListAndRestartRenderPass("Ran out of TFX RT descriptor descriptor groups");
			return ApplyTFXState(true);
		}

		flags |= DIRTY_FLAG_TEXTURES_DESCRIPTOR_TABLE_2;
	}

	ID3D12GraphicsCommandList* cmdlist = g_d3d12_context->GetCommandList();

	if (m_current_root_signature != RootSignature::TFX)
	{
		m_current_root_signature = RootSignature::TFX;
		flags |= DIRTY_FLAG_VS_CONSTANT_BUFFER_BINDING | DIRTY_FLAG_PS_CONSTANT_BUFFER_BINDING |
				 DIRTY_FLAG_TEXTURES_DESCRIPTOR_TABLE | DIRTY_FLAG_SAMPLERS_DESCRIPTOR_TABLE | DIRTY_FLAG_TEXTURES_DESCRIPTOR_TABLE_2 |
				 DIRTY_FLAG_PIPELINE;
		cmdlist->SetGraphicsRootSignature(m_tfx_root_signature.get());
	}

	if (flags & DIRTY_FLAG_VS_CONSTANT_BUFFER_BINDING)
		cmdlist->SetGraphicsRootConstantBufferView(TFX_ROOT_SIGNATURE_PARAM_VS_CBV, m_tfx_constant_buffers[0]);
	if (flags & DIRTY_FLAG_PS_CONSTANT_BUFFER_BINDING)
		cmdlist->SetGraphicsRootConstantBufferView(TFX_ROOT_SIGNATURE_PARAM_PS_CBV, m_tfx_constant_buffers[1]);
	if (flags & DIRTY_FLAG_TEXTURES_DESCRIPTOR_TABLE)
		cmdlist->SetGraphicsRootDescriptorTable(TFX_ROOT_SIGNATURE_PARAM_PS_TEXTURES, m_tfx_textures_handle_gpu);
	if (flags & DIRTY_FLAG_SAMPLERS_DESCRIPTOR_TABLE)
		cmdlist->SetGraphicsRootDescriptorTable(TFX_ROOT_SIGNATURE_PARAM_PS_SAMPLERS, m_tfx_samplers_handle_gpu);
	if (flags & DIRTY_FLAG_TEXTURES_DESCRIPTOR_TABLE_2)
		cmdlist->SetGraphicsRootDescriptorTable(TFX_ROOT_SIGNATURE_PARAM_PS_RT_TEXTURES, m_tfx_rt_textures_handle_gpu);

	ApplyBaseState(flags, cmdlist);
	return true;
}

bool GSDevice12::ApplyUtilityState(bool already_execed)
{
	if (m_current_root_signature == RootSignature::Utility && m_dirty_flags == 0)
		return true;

	u32 flags = m_dirty_flags;
	m_dirty_flags &= ~DIRTY_UTILITY_STATE;

	ID3D12GraphicsCommandList* cmdlist = g_d3d12_context->GetCommandList();

	if (m_current_root_signature != RootSignature::Utility)
	{
		m_current_root_signature = RootSignature::Utility;
		flags |= DIRTY_FLAG_TEXTURES_DESCRIPTOR_TABLE | DIRTY_FLAG_SAMPLERS_DESCRIPTOR_TABLE | DIRTY_FLAG_PIPELINE;
		cmdlist->SetGraphicsRootSignature(m_utility_root_signature.get());
	}

	if (flags & DIRTY_FLAG_TEXTURES_DESCRIPTOR_TABLE)
		cmdlist->SetGraphicsRootDescriptorTable(UTILITY_ROOT_SIGNATURE_PARAM_PS_TEXTURES, m_utility_texture_gpu);
	if (flags & DIRTY_FLAG_SAMPLERS_DESCRIPTOR_TABLE)
		cmdlist->SetGraphicsRootDescriptorTable(UTILITY_ROOT_SIGNATURE_PARAM_PS_SAMPLERS, m_utility_sampler_gpu);

	ApplyBaseState(flags, cmdlist);
	return true;
}

void GSDevice12::SetVSConstantBuffer(const GSHWDrawConfig::VSConstantBuffer& cb)
{
	if (m_vs_cb_cache.Update(cb))
		m_dirty_flags |= DIRTY_FLAG_VS_CONSTANT_BUFFER;
}

void GSDevice12::SetPSConstantBuffer(const GSHWDrawConfig::PSConstantBuffer& cb)
{
	if (m_ps_cb_cache.Update(cb))
		m_dirty_flags |= DIRTY_FLAG_PS_CONSTANT_BUFFER;
}

void GSDevice12::SetupDATE(GSTexture* rt, GSTexture* ds, bool datm, const GSVector4i& bbox)
{
	GL_PUSH("SetupDATE {%d,%d} %dx%d", bbox.left, bbox.top, bbox.width(), bbox.height());

	const GSVector2i size(ds->GetSize());
	const GSVector4 src = GSVector4(bbox) / GSVector4(size).xyxy();
	const GSVector4 dst = src * 2.0f - 1.0f;
	const GSVertexPT1 vertices[] = {
		{GSVector4(dst.x, -dst.y, 0.5f, 1.0f), GSVector2(src.x, src.y)},
		{GSVector4(dst.z, -dst.y, 0.5f, 1.0f), GSVector2(src.z, src.y)},
		{GSVector4(dst.x, -dst.w, 0.5f, 1.0f), GSVector2(src.x, src.w)},
		{GSVector4(dst.z, -dst.w, 0.5f, 1.0f), GSVector2(src.z, src.w)},
	};

	// sfex3 (after the capcom logo), vf4 (first menu fading in), ffxii shadows, rumble roses shadows, persona4 shadows
	EndRenderPass();
	SetUtilityTexture(rt, m_point_sampler_cpu);
	OMSetRenderTargets(nullptr, ds, bbox);
	IASetVertexBuffer(vertices, sizeof(vertices[0]), 4);
	SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	SetPipeline(m_convert[static_cast<int>(datm ? ShaderConvert::DATM_1 : ShaderConvert::DATM_0)].get());
	SetStencilRef(1);
	BeginRenderPass(D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
		D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE,
		D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE,
		GSVector4::zero(), 0.0f, 0);
	if (ApplyUtilityState())
		DrawPrimitive();

	EndRenderPass();
}

GSTexture12* GSDevice12::SetupPrimitiveTrackingDATE(GSHWDrawConfig& config, PipelineSelector& pipe)
{
	// How this is done:
	// - can't put a barrier for the image in the middle of the normal render pass, so that's out
	// - so, instead of just filling the int texture with INT_MAX, we sample the RT and use -1 for failing values
	// - then, instead of sampling the RT with DATE=1/2, we just do a min() without it, the -1 gets preserved
	// - then, the DATE=3 draw is done as normal
	GL_INS("Setup DATE Primitive ID Image for {%d,%d}-{%d,%d}", config.drawarea.left, config.drawarea.top,
		config.drawarea.right, config.drawarea.bottom);

	const GSVector2i rtsize(config.rt->GetSize());
	GSTexture12* image =
		static_cast<GSTexture12*>(CreateRenderTarget(rtsize.x, rtsize.y, GSTexture::Format::PrimID, false));
	if (!image)
		return nullptr;

	EndRenderPass();

	// setup the fill quad to prefill with existing alpha values
	SetUtilityTexture(config.rt, m_point_sampler_cpu);
	OMSetRenderTargets(image, config.ds, config.drawarea);

	// if the depth target has been cleared, we need to preserve that clear
	BeginRenderPass(
		D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE,
		GetLoadOpForTexture(static_cast<GSTexture12*>(config.ds)),
		config.ds ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
		D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
		GSVector4(static_cast<float>(std::numeric_limits<s32>::max()), 0.0f, 0.0f, 0.0f),
		static_cast<GSTexture12*>(config.ds)->GetClearDepth());

	// draw the quad to prefill the image
	const GSVector4 src = GSVector4(config.drawarea) / GSVector4(rtsize).xyxy();
	const GSVector4 dst = src * 2.0f - 1.0f;
	const GSVertexPT1 vertices[] = {
		{GSVector4(dst.x, -dst.y, 0.5f, 1.0f), GSVector2(src.x, src.y)},
		{GSVector4(dst.z, -dst.y, 0.5f, 1.0f), GSVector2(src.z, src.y)},
		{GSVector4(dst.x, -dst.w, 0.5f, 1.0f), GSVector2(src.x, src.w)},
		{GSVector4(dst.z, -dst.w, 0.5f, 1.0f), GSVector2(src.z, src.w)},
	};
	SetUtilityRootSignature();
	SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	SetPipeline(m_date_image_setup_pipelines[pipe.ds][config.datm].get());
	IASetVertexBuffer(vertices, sizeof(vertices[0]), std::size(vertices));
	if (ApplyUtilityState())
		DrawPrimitive();

	// image is now filled with either -1 or INT_MAX, so now we can do the prepass
	IASetVertexBuffer(config.verts, sizeof(GSVertex), config.nverts);
	IASetIndexBuffer(config.indices, config.nindices);

	// cut down the configuration for the prepass, we don't need blending or any feedback loop
	PipelineSelector init_pipe(m_pipeline_selector);
	init_pipe.dss.zwe = false;
	init_pipe.cms.wrgba = 0;
	init_pipe.bs = {};
	init_pipe.rt = true;
	init_pipe.ps.blend_a = init_pipe.ps.blend_b = init_pipe.ps.blend_c = init_pipe.ps.blend_d = false;
	init_pipe.ps.date += 10;
	init_pipe.ps.no_color = false;
	init_pipe.ps.no_color1 = true;
	if (BindDrawPipeline(init_pipe))
		DrawIndexedPrimitive();

	// image is initialized/prepass is done, so finish up and get ready to do the "real" draw
	EndRenderPass();

	// .. by setting it to DATE=3
	pipe.ps.date = 3;
	config.alpha_second_pass.ps.date = 3;

	// and bind the image to the primitive sampler
	image->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	PSSetShaderResource(3, image, false);
	return image;
}

void GSDevice12::RenderHW(GSHWDrawConfig& config)
{
	static constexpr std::array<D3D12_PRIMITIVE_TOPOLOGY, 3> primitive_topologies =
		{{D3D_PRIMITIVE_TOPOLOGY_POINTLIST, D3D_PRIMITIVE_TOPOLOGY_LINELIST, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST}};

	// Destination Alpha Setup
	const bool stencil_DATE =
		(config.destination_alpha == GSHWDrawConfig::DestinationAlphaMode::Stencil ||
			config.destination_alpha == GSHWDrawConfig::DestinationAlphaMode::StencilOne);
	if (stencil_DATE)
		SetupDATE(config.rt, config.ds, config.datm, config.drawarea);

	// stream buffer in first, in case we need to exec
	SetVSConstantBuffer(config.cb_vs);
	SetPSConstantBuffer(config.cb_ps);

	// figure out the pipeline
	UpdateHWPipelineSelector(config);

	// bind textures before checking the render pass, in case we need to transition them
	PipelineSelector& pipe = m_pipeline_selector;
	if (config.tex)
	{
		PSSetShaderResource(0, config.tex, config.tex != config.rt);
		PSSetSampler(0, config.sampler);
	}
	if (config.pal)
		PSSetShaderResource(1, config.pal, true);
	if (config.blend.constant_enable)
		SetBlendConstants(config.blend.constant);

	// Primitive ID tracking DATE setup.
	GSTexture12* date_image = nullptr;
	if (config.destination_alpha == GSHWDrawConfig::DestinationAlphaMode::PrimIDTracking)
	{
		date_image = SetupPrimitiveTrackingDATE(config, pipe);
		if (!date_image)
		{
			Console.WriteLn("Failed to allocate DATE image, aborting draw.");
			return;
		}
	}

	// Align the render area to 128x128, hopefully avoiding render pass restarts for small render area changes (e.g. Ratchet and Clank).
	const int render_area_alignment = 128 * GSConfig.UpscaleMultiplier;
	const GSVector2i rtsize(config.rt ? config.rt->GetSize() : config.ds->GetSize());
	const GSVector4i render_area(
		config.ps.hdr ? config.drawarea :
                        GSVector4i(Common::AlignDownPow2(config.scissor.left, render_area_alignment),
							Common::AlignDownPow2(config.scissor.top, render_area_alignment),
							std::min(Common::AlignUpPow2(config.scissor.right, render_area_alignment), rtsize.x),
							std::min(Common::AlignUpPow2(config.scissor.bottom, render_area_alignment), rtsize.y)));

	GSTexture12* draw_rt = static_cast<GSTexture12*>(config.rt);
	GSTexture12* draw_ds = static_cast<GSTexture12*>(config.ds);
	GSTexture12* draw_rt_clone = nullptr;
	GSTexture12* hdr_rt = nullptr;
	GSTexture12* copy_ds = nullptr;

	// Switch to hdr target for colclip rendering
	if (pipe.ps.hdr)
	{
		EndRenderPass();

		GL_PUSH_("HDR Render Target Setup");
		hdr_rt = static_cast<GSTexture12*>(CreateRenderTarget(rtsize.x, rtsize.y, GSTexture::Format::FloatColor, false));
		if (!hdr_rt)
		{
			Console.WriteLn("Failed to allocate HDR render target, aborting draw.");
			if (date_image)
				Recycle(date_image);
			return;
		}

		// propagate clear value through if the hdr render is the first
		if (draw_rt->GetState() == GSTexture::State::Cleared)
		{
			hdr_rt->SetClearColor(draw_rt->GetClearColor());
		}
		else
		{
			hdr_rt->SetState(GSTexture::State::Invalidated);
			draw_rt->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}

		// we're not drawing to the RT, so we can use it as a source
		if (config.require_one_barrier)
			PSSetShaderResource(2, draw_rt, true);

		draw_rt = hdr_rt;
	}
	else if (config.require_one_barrier)
	{
		// requires a copy of the RT
		draw_rt_clone = static_cast<GSTexture12*>(CreateTexture(rtsize.x, rtsize.y, false, GSTexture::Format::Color, false));
		if (draw_rt_clone)
		{
			EndRenderPass();

			GL_PUSH("Copy RT to temp texture for fbmask {%d,%d %dx%d}",
				config.drawarea.left, config.drawarea.top,
				config.drawarea.width(), config.drawarea.height());

			draw_rt_clone->SetState(GSTexture::State::Invalidated);
			CopyRect(draw_rt, draw_rt_clone, config.drawarea, config.drawarea.left, config.drawarea.top);
			PSSetShaderResource(2, draw_rt_clone, true);
		}
	}

	if (config.tex && config.tex == config.ds)
	{
		// requires a copy of the depth buffer. this is mainly for ico.
		copy_ds = static_cast<GSTexture12*>(CreateDepthStencil(rtsize.x, rtsize.y, GSTexture::Format::DepthStencil, false));
		if (copy_ds)
		{
			EndRenderPass();

			GL_PUSH("Copy depth to temp texture for shuffle {%d,%d %dx%d}",
				config.drawarea.left, config.drawarea.top,
				config.drawarea.width(), config.drawarea.height());

			copy_ds->SetState(GSTexture::State::Invalidated);
			CopyRect(config.ds, copy_ds, config.drawarea, config.drawarea.left, config.drawarea.top);
			PSSetShaderResource(0, copy_ds, true);
		}
	}

	// avoid restarting the render pass just to switch from rt+depth to rt and vice versa
	if (m_in_render_pass && !hdr_rt && !draw_ds && m_current_depth_target && m_current_render_target == draw_rt && config.tex != m_current_depth_target)
	{
		draw_ds = m_current_depth_target;
		m_pipeline_selector.ds = true;
		m_pipeline_selector.dss.ztst = ZTST_ALWAYS;
		m_pipeline_selector.dss.zwe = false;
	}

	OMSetRenderTargets(draw_rt, draw_ds, config.scissor);

	// Begin render pass if new target or out of the area.
	if (!m_in_render_pass)
	{
		BeginRenderPass(
			GetLoadOpForTexture(draw_rt),
			draw_rt ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
			GetLoadOpForTexture(draw_ds),
			draw_ds ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
			stencil_DATE ? D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS,
			stencil_DATE ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
			draw_rt ? draw_rt->GetClearColor() : GSVector4::zero(), draw_ds ? draw_ds->GetClearDepth() : 0.0f, 1);
	}

	// rt -> hdr blit if enabled
	if (hdr_rt && config.rt->GetState() == GSTexture::State::Dirty)
	{
		SetUtilityTexture(static_cast<GSTexture12*>(config.rt), m_point_sampler_cpu);
		SetPipeline(m_hdr_setup_pipelines[pipe.ds].get());

		const GSVector4 sRect(GSVector4(render_area) / GSVector4(rtsize.x, rtsize.y).xyxy());
		DrawStretchRect(sRect, GSVector4(render_area), rtsize);
		g_perfmon.Put(GSPerfMon::TextureCopies);

		GL_POP();
	}

	// VB/IB upload, if we did DATE setup and it's not HDR this has already been done
	SetPrimitiveTopology(primitive_topologies[static_cast<u8>(config.topology)]);
	if (!date_image || hdr_rt)
	{
		IASetVertexBuffer(config.verts, sizeof(GSVertex), config.nverts);
		IASetIndexBuffer(config.indices, config.nindices);
	}

	// now we can do the actual draw
	if (BindDrawPipeline(pipe))
	{
		DrawIndexedPrimitive();

		if (config.second_separate_alpha_pass)
		{
			SetHWDrawConfigForAlphaPass(&pipe.ps, &pipe.cms, &pipe.bs, &pipe.dss);
			if (BindDrawPipeline(pipe))
				DrawIndexedPrimitive();
		}
	}

	// and the alpha pass
	if (config.alpha_second_pass.enable)
	{
		// cbuffer will definitely be dirty if aref changes, no need to check it
		if (config.cb_ps.FogColor_AREF.a != config.alpha_second_pass.ps_aref)
		{
			config.cb_ps.FogColor_AREF.a = config.alpha_second_pass.ps_aref;
			SetPSConstantBuffer(config.cb_ps);
		}

		pipe.ps = config.alpha_second_pass.ps;
		pipe.cms = config.alpha_second_pass.colormask;
		pipe.dss = config.alpha_second_pass.depth;
		pipe.bs = config.blend;
		if (BindDrawPipeline(pipe))
		{
			DrawIndexedPrimitive();

			if (config.second_separate_alpha_pass)
			{
				SetHWDrawConfigForAlphaPass(&pipe.ps, &pipe.cms, &pipe.bs, &pipe.dss);
				if (BindDrawPipeline(pipe))
					DrawIndexedPrimitive();
			}
		}
	}

	if (copy_ds)
		Recycle(copy_ds);

	if (draw_rt_clone)
		Recycle(draw_rt_clone);

	if (date_image)
		Recycle(date_image);

	EndScene();

	// now blit the hdr texture back to the original target
	if (hdr_rt)
	{
		GL_INS("Blit HDR back to RT");

		EndRenderPass();
		hdr_rt->TransitionToState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		draw_rt = static_cast<GSTexture12*>(config.rt);
		OMSetRenderTargets(draw_rt, draw_ds, config.scissor);

		// if this target was cleared and never drawn to, perform the clear as part of the resolve here.
		BeginRenderPass(GetLoadOpForTexture(draw_rt), D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE,
			GetLoadOpForTexture(draw_ds), draw_ds ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
			D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS, D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS,
			draw_rt->GetClearColor(), 0.0f, 0);

		const GSVector4 sRect(GSVector4(render_area) / GSVector4(rtsize.x, rtsize.y).xyxy());
		SetPipeline(m_hdr_finish_pipelines[pipe.ds].get());
		SetUtilityTexture(hdr_rt, m_point_sampler_cpu);
		DrawStretchRect(sRect, GSVector4(render_area), rtsize);
		g_perfmon.Put(GSPerfMon::TextureCopies);

		Recycle(hdr_rt);
	}
}

void GSDevice12::UpdateHWPipelineSelector(GSHWDrawConfig& config)
{
	m_pipeline_selector.vs.key = config.vs.key;
	m_pipeline_selector.gs.key = config.gs.key;
	m_pipeline_selector.ps.key_hi = config.ps.key_hi;
	m_pipeline_selector.ps.key_lo = config.ps.key_lo;
	m_pipeline_selector.dss.key = config.depth.key;
	m_pipeline_selector.bs.key = config.blend.key;
	m_pipeline_selector.bs.constant = 0; // don't dupe states with different alpha values
	m_pipeline_selector.cms.key = config.colormask.key;
	m_pipeline_selector.topology = static_cast<u32>(config.topology);
	m_pipeline_selector.rt = config.rt != nullptr;
	m_pipeline_selector.ds = config.ds != nullptr;
}
