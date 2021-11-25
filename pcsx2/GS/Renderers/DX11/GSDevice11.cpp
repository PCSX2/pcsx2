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
#include "GS.h"
#include "GSDevice11.h"
#include "GS/Renderers/DX11/D3D.h"
#include "GS/GSExtra.h"
#include "GS/GSUtil.h"
#include "GS/resource.h"
#include <fstream>
#include <sstream>
#include <VersionHelpers.h>
#include <d3dcompiler.h>

GSDevice11::GSDevice11()
{
	memset(&m_state, 0, sizeof(m_state));

	m_state.topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	m_state.bf = -1;

	m_mipmap = theApp.GetConfigI("mipmap");
	m_upscale_multiplier = std::max(0, theApp.GetConfigI("upscale_multiplier"));

	const BiFiltering nearest_filter = static_cast<BiFiltering>(theApp.GetConfigI("filter"));
	const int aniso_level = theApp.GetConfigI("MaxAnisotropy");
	if ((nearest_filter != BiFiltering::Nearest && !theApp.GetConfigB("paltex") && aniso_level))
		m_aniso_filter = aniso_level;
	else
		m_aniso_filter = 0;
}

bool GSDevice11::SetFeatureLevel(D3D_FEATURE_LEVEL level, bool compat_mode)
{
	m_shader.level = level;

	switch (level)
	{
		case D3D_FEATURE_LEVEL_10_0:
			m_shader.model = "0x400";
			m_shader.vs = "vs_4_0";
			m_shader.gs = "gs_4_0";
			m_shader.ps = "ps_4_0";
			m_shader.cs = "cs_4_0";
			break;
		case D3D_FEATURE_LEVEL_10_1:
			m_shader.model = "0x401";
			m_shader.vs = "vs_4_1";
			m_shader.gs = "gs_4_1";
			m_shader.ps = "ps_4_1";
			m_shader.cs = "cs_4_1";
			break;
		case D3D_FEATURE_LEVEL_11_0:
			m_shader.model = "0x500";
			m_shader.vs = "vs_5_0";
			m_shader.gs = "gs_5_0";
			m_shader.ps = "ps_5_0";
			m_shader.cs = "cs_5_0";
			break;
		default:
			ASSERT(0);
			return false;
	}

	return true;
}

bool GSDevice11::Create(const WindowInfo& wi)
{
	if (!__super::Create(wi))
	{
		return false;
	}

	D3D11_BUFFER_DESC bd;
	D3D11_SAMPLER_DESC sd;
	D3D11_DEPTH_STENCIL_DESC dsd;
	D3D11_RASTERIZER_DESC rd;
	D3D11_BLEND_DESC bsd;

	const bool enable_debugging = theApp.GetConfigB("debug_d3d");

	auto factory = D3D::CreateFactory(enable_debugging);
	if (!factory)
		return false;

	// select adapter
	auto adapter = D3D::GetAdapterFromIndex(
		factory.get(), theApp.GetConfigI("adapter_index")
	);

	DXGI_ADAPTER_DESC1 adapter_desc = {};
	if (SUCCEEDED(adapter->GetDesc1(&adapter_desc)))
	{
		std::string adapter_name = convert_utf16_to_utf8(
			adapter_desc.Description
		);

		fprintf(stderr, "Selected DXGI Adapter\n"
			"\tName: %s\n"
			"\tVendor: %x\n", adapter_name.c_str(), adapter_desc.VendorId);
	}

	// device creation
	{
		u32 flags = D3D11_CREATE_DEVICE_SINGLETHREADED;

		if(enable_debugging)
			flags |= D3D11_CREATE_DEVICE_DEBUG;

		constexpr std::array<D3D_FEATURE_LEVEL, 3> supported_levels = {
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0,
		};

		D3D_FEATURE_LEVEL feature_level;
		HRESULT result = D3D11CreateDevice(
			adapter.get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags,
			supported_levels.data(), supported_levels.size(),
			D3D11_SDK_VERSION, m_dev.put(), &feature_level, m_ctx.put()
		);

		// if a debug device is requested but not supported, fallback to non-debug device
		if (FAILED(result) && enable_debugging)
		{
			fprintf(stderr, "D3D: failed to create debug device, trying without debugging\n");
			// clear the debug flag
			flags = D3D11_CREATE_DEVICE_SINGLETHREADED;

			result = D3D11CreateDevice(
				adapter.get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, flags,
				supported_levels.data(), supported_levels.size(),
				D3D11_SDK_VERSION, m_dev.put(), &feature_level, m_ctx.put()
			);
		}

		if (FAILED(result))
		{
			fprintf(stderr, "D3D: unable to create D3D11 device (reason %x)\n"
				"ensure that your gpu supports our minimum requirements:\n"
				"https://github.com/PCSX2/pcsx2#system-requirements\n", result);
			return false;
		}

		if (enable_debugging)
		{
			if (auto info_queue = m_dev.try_query<ID3D11InfoQueue>())
			{
				const int break_on = theApp.GetConfigI("dx_break_on_severity");

				info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, break_on & (1 << 0));
				info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, break_on & (1 << 1));
				info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, break_on & (1 << 2));
				info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_INFO, break_on & (1 << 3));
			}
			fprintf(stderr, "D3D: debugging enabled\n");
		}

		if (!SetFeatureLevel(feature_level, true))
		{
			fprintf(stderr, "D3D: adapter doesn't have a sufficient feature level\n");
			return false;
		}

		// Set maximum texture size limit based on supported feature level.
		if (feature_level >= D3D_FEATURE_LEVEL_11_0)
			m_d3d_texsize = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
		else
			m_d3d_texsize = D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION;
	}

	// swapchain creation
	{
		DXGI_SWAP_CHAIN_DESC1 swapchain_description = {};

		// let the runtime get window size
		swapchain_description.Width = 0;
		swapchain_description.Height = 0;

		swapchain_description.BufferCount = 2;
		swapchain_description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchain_description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapchain_description.SampleDesc.Count = 1;
		swapchain_description.SampleDesc.Quality = 0;

		// TODO: update swap effect
		swapchain_description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		const HRESULT result = factory->CreateSwapChainForHwnd(
			m_dev.get(), reinterpret_cast<HWND>(wi.window_handle),
			&swapchain_description, nullptr, nullptr, m_swapchain.put());

		if (FAILED(result))
		{
			fprintf(stderr, "D3D: Failed to create swapchain (reason: %x)\n", result);
			return false;
		}
	}

	{
		// HACK: check nVIDIA
		// Note: It can cause issues on several games such as SOTC, Fatal Frame, plus it adds border offset.
		bool disable_safe_features = theApp.GetConfigB("UserHacks") && theApp.GetConfigB("UserHacks_Disable_Safe_Features");
		m_hack_topleft_offset = (m_upscale_multiplier != 1 && D3D::IsNvidia(adapter.get()) && !disable_safe_features) ? -0.01f : 0.0f;
	}

	// convert

	D3D11_INPUT_ELEMENT_DESC il_convert[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	ShaderMacro sm_model(m_shader.model);

	std::vector<char> shader;
	theApp.LoadResource(IDR_CONVERT_FX, shader);
    CreateShader(shader, "convert.fx", nullptr, "vs_main", sm_model.GetPtr(), &m_convert.vs, il_convert, std::size(il_convert), m_convert.il.put());

	ShaderMacro sm_convert(m_shader.model);
	sm_convert.AddMacro("PS_SCALE_FACTOR", std::max(1, m_upscale_multiplier));

	D3D_SHADER_MACRO* sm_convert_ptr = sm_convert.GetPtr();

	for (size_t i = 0; i < std::size(m_convert.ps); i++)
	{
		CreateShader(shader, "convert.fx", nullptr, shaderName(static_cast<ShaderConvert>(i)), sm_convert_ptr, m_convert.ps[i].put());
	}

	memset(&dsd, 0, sizeof(dsd));

	m_dev->CreateDepthStencilState(&dsd, m_convert.dss.put());

	dsd.DepthEnable = true;
	dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dsd.DepthFunc = D3D11_COMPARISON_ALWAYS;

	m_dev->CreateDepthStencilState(&dsd, m_convert.dss_write.put());

	memset(&bsd, 0, sizeof(bsd));

	bsd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	m_dev->CreateBlendState(&bsd, m_convert.bs.put());

	// merge

	memset(&bd, 0, sizeof(bd));

	bd.ByteWidth = sizeof(MergeConstantBuffer);
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	m_dev->CreateBuffer(&bd, nullptr, m_merge.cb.put());

	theApp.LoadResource(IDR_MERGE_FX, shader);
	for (size_t i = 0; i < std::size(m_merge.ps); i++)
	{
		CreateShader(shader, "merge.fx", nullptr, format("ps_main%d", i).c_str(), sm_model.GetPtr(), m_merge.ps[i].put());
	}

	memset(&bsd, 0, sizeof(bsd));

	bsd.RenderTarget[0].BlendEnable = true;
	bsd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	bsd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	bsd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	bsd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	bsd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	bsd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	bsd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	m_dev->CreateBlendState(&bsd, m_merge.bs.put());

	// interlace

	memset(&bd, 0, sizeof(bd));

	bd.ByteWidth = sizeof(InterlaceConstantBuffer);
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	m_dev->CreateBuffer(&bd, nullptr, m_interlace.cb.put());

	theApp.LoadResource(IDR_INTERLACE_FX, shader);
	for (size_t i = 0; i < std::size(m_interlace.ps); i++)
	{
		CreateShader(shader, "interlace.fx", nullptr, format("ps_main%d", i).c_str(), sm_model.GetPtr(), m_interlace.ps[i].put());
	}

	// Shade Boost

	ShaderMacro sm_sboost(m_shader.model);

	sm_sboost.AddMacro("SB_CONTRAST", std::clamp(theApp.GetConfigI("ShadeBoost_Contrast"), 0, 100));
	sm_sboost.AddMacro("SB_BRIGHTNESS", std::clamp(theApp.GetConfigI("ShadeBoost_Brightness"), 0, 100));
	sm_sboost.AddMacro("SB_SATURATION", std::clamp(theApp.GetConfigI("ShadeBoost_Saturation"), 0, 100));

	memset(&bd, 0, sizeof(bd));

	bd.ByteWidth = sizeof(ShadeBoostConstantBuffer);
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	m_dev->CreateBuffer(&bd, nullptr, m_shadeboost.cb.put());

	theApp.LoadResource(IDR_SHADEBOOST_FX, shader);
	CreateShader(shader, "shadeboost.fx", nullptr, "ps_main", sm_sboost.GetPtr(), m_shadeboost.ps.put());

	// External fx shader

	memset(&bd, 0, sizeof(bd));

	bd.ByteWidth = sizeof(ExternalFXConstantBuffer);
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	m_dev->CreateBuffer(&bd, nullptr, m_shaderfx.cb.put());

	// Fxaa

	memset(&bd, 0, sizeof(bd));

	bd.ByteWidth = sizeof(FXAAConstantBuffer);
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	m_dev->CreateBuffer(&bd, nullptr, m_fxaa.cb.put());

	//

	memset(&rd, 0, sizeof(rd));

	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_NONE;
	rd.FrontCounterClockwise = false;
	rd.DepthBias = false;
	rd.DepthBiasClamp = 0;
	rd.SlopeScaledDepthBias = 0;
	rd.DepthClipEnable = false; // ???
	rd.ScissorEnable = true;
	rd.MultisampleEnable = true;
	rd.AntialiasedLineEnable = false;

	wil::com_ptr_nothrow<ID3D11RasterizerState> rs;
	m_dev->CreateRasterizerState(&rd, rs.put());

	m_ctx->RSSetState(rs.get());

	//

	memset(&sd, 0, sizeof(sd));

	sd.Filter = m_aniso_filter ? D3D11_FILTER_ANISOTROPIC : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.MinLOD = -FLT_MAX;
	sd.MaxLOD = FLT_MAX;
	sd.MaxAnisotropy = m_aniso_filter;
	sd.ComparisonFunc = D3D11_COMPARISON_NEVER;

	m_dev->CreateSamplerState(&sd, m_convert.ln.put());

	sd.Filter = m_aniso_filter ? D3D11_FILTER_ANISOTROPIC : D3D11_FILTER_MIN_MAG_MIP_POINT;

	m_dev->CreateSamplerState(&sd, m_convert.pt.put());

	//

	Reset(wi.surface_width, wi.surface_height);

	//

	CreateTextureFX();

	//

	memset(&dsd, 0, sizeof(dsd));

	dsd.DepthEnable = false;
	dsd.StencilEnable = true;
	dsd.StencilReadMask = 1;
	dsd.StencilWriteMask = 1;
	dsd.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	dsd.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
	dsd.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dsd.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	dsd.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	dsd.BackFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
	dsd.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	dsd.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;

	m_dev->CreateDepthStencilState(&dsd, m_date.dss.put());

	D3D11_BLEND_DESC blend;

	memset(&blend, 0, sizeof(blend));

	m_dev->CreateBlendState(&blend, m_date.bs.put());

	const GSVector2i tex_font = m_osd.get_texture_font_size();

	m_font = std::unique_ptr<GSTexture>(
		CreateSurface(GSTexture::Type::Texture, tex_font.x, tex_font.y, GSTexture::Format::UNorm8));

	return true;
}

bool GSDevice11::Reset(int w, int h)
{
	if (!__super::Reset(w, h))
		return false;

	if (m_swapchain)
	{
		DXGI_SWAP_CHAIN_DESC scd;

		memset(&scd, 0, sizeof(scd));

		m_swapchain->GetDesc(&scd);
		m_swapchain->ResizeBuffers(scd.BufferCount, w, h, scd.BufferDesc.Format, 0);

		wil::com_ptr_nothrow<ID3D11Texture2D> backbuffer;
		if (FAILED(m_swapchain->GetBuffer(0, IID_PPV_ARGS(backbuffer.put()))))
		{
			return false;
		}

		m_backbuffer = new GSTexture11(std::move(backbuffer), GSTexture::Format::Backbuffer);
	}

	return true;
}

void GSDevice11::SetVSync(int vsync)
{
	m_vsync = vsync ? 1 : 0;
}

void GSDevice11::Flip()
{
	m_swapchain->Present(m_vsync, 0);
}

void GSDevice11::BeforeDraw()
{
	// DX can't read from the FB
	// So let's copy it and send that to the shader instead

	auto bits = m_state.ps_sr_bitfield;
	m_state.ps_sr_bitfield = 0;

	unsigned long i;
	while (_BitScanForward(&i, bits))
	{
		GSTexture11* tex = m_state.ps_sr_texture[i];

		if (tex->Equal(m_state.rt_texture) || tex->Equal(m_state.rt_ds))
		{
#ifdef _DEBUG
			OutputDebugStringA(format("WARNING: Cleaning up copied texture on slot %i", i).c_str());
#endif
			GSTexture* cp = nullptr;

			CloneTexture(tex, &cp);

			PSSetShaderResource(i, cp);
		}

		bits ^= 1u << i;
	}

	PSUpdateShaderState();
}

void GSDevice11::AfterDraw()
{
	unsigned long i;
	while (_BitScanForward(&i, m_state.ps_sr_bitfield))
	{
#ifdef _DEBUG
		OutputDebugStringA(format("WARNING: FB read detected on slot %i, copying...", i).c_str());
#endif
		Recycle(m_state.ps_sr_texture[i]);
		PSSetShaderResource(i, nullptr);
	}
}

void GSDevice11::DrawPrimitive()
{
	BeforeDraw();

	m_ctx->Draw(m_vertex.count, m_vertex.start);

	AfterDraw();
}

void GSDevice11::DrawIndexedPrimitive()
{
	BeforeDraw();

	m_ctx->DrawIndexed(m_index.count, m_index.start, m_vertex.start);

	AfterDraw();
}

void GSDevice11::DrawIndexedPrimitive(int offset, int count)
{
	ASSERT(offset + count <= (int)m_index.count);

	BeforeDraw();

	m_ctx->DrawIndexed(count, m_index.start + offset, m_vertex.start);

	AfterDraw();
}

void GSDevice11::ClearRenderTarget(GSTexture* t, const GSVector4& c)
{
	if (!t)
		return;
	m_ctx->ClearRenderTargetView(*(GSTexture11*)t, c.v);
}

void GSDevice11::ClearRenderTarget(GSTexture* t, u32 c)
{
	if (!t)
		return;
	const GSVector4 color = GSVector4::rgba32(c) * (1.0f / 255);

	m_ctx->ClearRenderTargetView(*(GSTexture11*)t, color.v);
}

void GSDevice11::ClearDepth(GSTexture* t)
{
	if (!t)
		return;
	m_ctx->ClearDepthStencilView(*(GSTexture11*)t, D3D11_CLEAR_DEPTH, 0.0f, 0);
}

void GSDevice11::ClearStencil(GSTexture* t, u8 c)
{
	if (!t)
		return;
	m_ctx->ClearDepthStencilView(*(GSTexture11*)t, D3D11_CLEAR_STENCIL, 0, c);
}

GSTexture* GSDevice11::CreateSurface(GSTexture::Type type, int w, int h, GSTexture::Format format)
{
	D3D11_TEXTURE2D_DESC desc;

	memset(&desc, 0, sizeof(desc));

	DXGI_FORMAT dxformat;
	switch (format)
	{
		case GSTexture::Format::Color:        dxformat = DXGI_FORMAT_R8G8B8A8_UNORM;     break;
		case GSTexture::Format::FloatColor:   dxformat = DXGI_FORMAT_R32G32B32A32_FLOAT; break;
		case GSTexture::Format::DepthStencil: dxformat = DXGI_FORMAT_R32G8X24_TYPELESS;  break;
		case GSTexture::Format::UNorm8:       dxformat = DXGI_FORMAT_A8_UNORM;           break;
		case GSTexture::Format::UInt16:       dxformat = DXGI_FORMAT_R16_UINT;           break;
		case GSTexture::Format::UInt32:       dxformat = DXGI_FORMAT_R32_UINT;           break;
		case GSTexture::Format::Int32:        dxformat = DXGI_FORMAT_R32_SINT;           break;
		case GSTexture::Format::Invalid:
		case GSTexture::Format::Backbuffer:
			ASSERT(0);
			dxformat = DXGI_FORMAT_UNKNOWN;
	}

	// Texture limit for D3D10/11 min 1, max 8192 D3D10, max 16384 D3D11.
	desc.Width = std::max(1, std::min(w, m_d3d_texsize));
	desc.Height = std::max(1, std::min(h, m_d3d_texsize));
	desc.Format = dxformat;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;

	// mipmap = m_mipmap > 1 || m_filter != TriFiltering::None;
	const bool mipmap = m_mipmap > 1;
	const int layers = mipmap && format == GSTexture::Format::Color ? (int)log2(std::max(w, h)) : 1;

	switch (type)
	{
		case GSTexture::Type::RenderTarget:
			desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			break;
		case GSTexture::Type::DepthStencil:
			desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
			break;
		case GSTexture::Type::Texture:
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			desc.MipLevels = layers;
			break;
		case GSTexture::Type::Offscreen:
			desc.Usage = D3D11_USAGE_STAGING;
			desc.CPUAccessFlags |= D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
			break;
	}

	GSTexture11* t = NULL;

	wil::com_ptr_nothrow<ID3D11Texture2D> texture;
	HRESULT hr = m_dev->CreateTexture2D(&desc, nullptr, texture.put());

	if (SUCCEEDED(hr))
	{
		t = new GSTexture11(std::move(texture), format);

		switch (type)
		{
			case GSTexture::Type::RenderTarget:
				ClearRenderTarget(t, 0);
				break;
			case GSTexture::Type::DepthStencil:
				ClearDepth(t);
				break;
		}
	}
	else
	{
		throw std::bad_alloc();
	}

	return t;
}

GSTexture* GSDevice11::FetchSurface(GSTexture::Type type, int w, int h, GSTexture::Format format)
{
	return __super::FetchSurface(type, w, h, format);
}

bool GSDevice11::DownloadTexture(GSTexture* src, const GSVector4i& rect, GSTexture::GSMap& out_map)
{
	ASSERT(src);
	ASSERT(!m_download_tex);
	m_download_tex.reset(static_cast<GSTexture11*>(CreateOffscreen(rect.width(), rect.height(), src->GetFormat())));
	if (!m_download_tex)
		return false;
	m_ctx->CopyResource(*m_download_tex, *static_cast<GSTexture11*>(src));
	return m_download_tex->Map(out_map);
}

void GSDevice11::DownloadTextureComplete()
{
	if (m_download_tex)
	{
		m_download_tex->Unmap();
		Recycle(m_download_tex.release());
	}
}

void GSDevice11::CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r)
{
	if (!sTex || !dTex)
	{
		ASSERT(0);
		return;
	}

	D3D11_BOX box = {(UINT)r.left, (UINT)r.top, 0U, (UINT)r.right, (UINT)r.bottom, 1U};

	// DX api isn't happy if we pass a box for depth copy
	// It complains that depth/multisample must be a full copy
	// and asks us to use a NULL for the box
	const bool depth = (sTex->GetType() == GSTexture::Type::DepthStencil);
	auto pBox = depth ? nullptr : &box;

	m_ctx->CopySubresourceRegion(*(GSTexture11*)dTex, 0, 0, 0, 0, *(GSTexture11*)sTex, 0, pBox);
}

void GSDevice11::CloneTexture(GSTexture* src, GSTexture** dest)
{
	if (!src || !(src->GetType() == GSTexture::Type::DepthStencil || src->GetType() == GSTexture::Type::RenderTarget))
	{
		ASSERT(0);
		return;
	}

	const int w = src->GetWidth();
	const int h = src->GetHeight();

	if (src->GetType() == GSTexture::Type::DepthStencil)
		*dest = CreateDepthStencil(w, h, src->GetFormat());
	else
		*dest = CreateRenderTarget(w, h, src->GetFormat());

	CopyRect(src, *dest, GSVector4i(0, 0, w, h));
}

void GSDevice11::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ShaderConvert shader, bool linear)
{
	StretchRect(sTex, sRect, dTex, dRect, m_convert.ps[static_cast<int>(shader)].get(), nullptr, linear);
}

void GSDevice11::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ID3D11PixelShader* ps, ID3D11Buffer* ps_cb, bool linear)
{
	StretchRect(sTex, sRect, dTex, dRect, ps, ps_cb, m_convert.bs.get(), linear);
}

void GSDevice11::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, bool red, bool green, bool blue, bool alpha)
{
	D3D11_BLEND_DESC bd = {};

	u8 write_mask = 0;

	if (red)   write_mask |= D3D11_COLOR_WRITE_ENABLE_RED;
	if (green) write_mask |= D3D11_COLOR_WRITE_ENABLE_GREEN;
	if (blue)  write_mask |= D3D11_COLOR_WRITE_ENABLE_BLUE;
	if (alpha) write_mask |= D3D11_COLOR_WRITE_ENABLE_ALPHA;

	bd.RenderTarget[0].RenderTargetWriteMask = write_mask;

	wil::com_ptr_nothrow<ID3D11BlendState> bs;
	m_dev->CreateBlendState(&bd, bs.put());

	StretchRect(sTex, sRect, dTex, dRect, m_convert.ps[static_cast<int>(ShaderConvert::COPY)].get(), nullptr, bs.get(), false);
}

void GSDevice11::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ID3D11PixelShader* ps, ID3D11Buffer* ps_cb, ID3D11BlendState* bs, bool linear)
{
	if (!sTex || !dTex)
	{
		ASSERT(0);
		return;
	}

	const bool draw_in_depth = ps == m_convert.ps[static_cast<int>(ShaderConvert::RGBA8_TO_FLOAT32)]
	                        || ps == m_convert.ps[static_cast<int>(ShaderConvert::RGBA8_TO_FLOAT24)]
	                        || ps == m_convert.ps[static_cast<int>(ShaderConvert::RGBA8_TO_FLOAT16)]
	                        || ps == m_convert.ps[static_cast<int>(ShaderConvert::RGB5A1_TO_FLOAT16)];

	BeginScene();

	const GSVector2i ds = dTex->GetSize();

	// om


	if (draw_in_depth)
		OMSetDepthStencilState(m_convert.dss_write.get(), 0);
	else
		OMSetDepthStencilState(m_convert.dss.get(), 0);

	OMSetBlendState(bs, 0);

	if (draw_in_depth)
		OMSetRenderTargets(nullptr, dTex);
	else
		OMSetRenderTargets(dTex, nullptr);

	// ia

	const float left = dRect.x * 2 / ds.x - 1.0f;
	const float top = 1.0f - dRect.y * 2 / ds.y;
	const float right = dRect.z * 2 / ds.x - 1.0f;
	const float bottom = 1.0f - dRect.w * 2 / ds.y;

	GSVertexPT1 vertices[] =
	{
		{GSVector4(left, top, 0.5f, 1.0f), GSVector2(sRect.x, sRect.y)},
		{GSVector4(right, top, 0.5f, 1.0f), GSVector2(sRect.z, sRect.y)},
		{GSVector4(left, bottom, 0.5f, 1.0f), GSVector2(sRect.x, sRect.w)},
		{GSVector4(right, bottom, 0.5f, 1.0f), GSVector2(sRect.z, sRect.w)},
	};



    IASetVertexBuffer(vertices, sizeof(vertices[0]), std::size(vertices));
	IASetInputLayout(m_convert.il.get());
	IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// vs

	VSSetShader(m_convert.vs.get(), nullptr);


	// gs

	GSSetShader(nullptr, nullptr);


	// ps

	PSSetShaderResources(sTex, nullptr);
	PSSetSamplerState(linear ? m_convert.ln.get() : m_convert.pt.get(), nullptr);
	PSSetShader(ps, ps_cb);

	//

	DrawPrimitive();

	//

	EndScene();

	PSSetShaderResources(nullptr, nullptr);
}

void GSDevice11::RenderOsd(GSTexture* dt)
{
	BeginScene();

	// om
	OMSetDepthStencilState(m_convert.dss.get(), 0);
	OMSetBlendState(m_merge.bs.get(), 0);
	OMSetRenderTargets(dt, nullptr);

	if (m_osd.m_texture_dirty)
	{
		m_osd.upload_texture_atlas(m_font.get());
	}

	// ps
	PSSetShaderResource(0, m_font.get());
	PSSetSamplerState(m_convert.pt.get(), nullptr);
	PSSetShader(m_convert.ps[static_cast<int>(ShaderConvert::OSD)].get(), nullptr);

	// ia
	IASetInputLayout(m_convert.il.get());
	IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Note scaling could also be done in shader (require gl3/dx10)
	size_t count = m_osd.Size();
	void* dst = nullptr;

	IAMapVertexBuffer(&dst, sizeof(GSVertexPT1), count);
	count = m_osd.GeneratePrimitives((GSVertexPT1*)dst, count);
	IAUnmapVertexBuffer();

	// vs
	VSSetShader(m_convert.vs.get(), nullptr);

	// gs
	GSSetShader(nullptr, nullptr);

	DrawPrimitive();

	EndScene();
}

void GSDevice11::DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect, const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, const GSVector4& c)
{
	const bool slbg = PMODE.SLBG;
	const bool mmod = PMODE.MMOD;

	ClearRenderTarget(dTex, c);

	if (sTex[1] && !slbg)
	{
		StretchRect(sTex[1], sRect[1], dTex, dRect[1], m_merge.ps[0].get(), nullptr, true);
	}

	if (sTex[0])
	{
		m_ctx->UpdateSubresource(m_merge.cb.get(), 0, nullptr, &c, 0, 0);

		StretchRect(sTex[0], sRect[0], dTex, dRect[0], m_merge.ps[mmod ? 1 : 0].get(), m_merge.cb.get(), m_merge.bs.get(), true);
	}
}

void GSDevice11::DoInterlace(GSTexture* sTex, GSTexture* dTex, int shader, bool linear, float yoffset)
{
	const GSVector4 s = GSVector4(dTex->GetSize());

	const GSVector4 sRect(0, 0, 1, 1);
	const GSVector4 dRect(0.0f, yoffset, s.x, s.y + yoffset);

	InterlaceConstantBuffer cb;

	cb.ZrH = GSVector2(0, 1.0f / s.y);
	cb.hH = s.y / 2;

	m_ctx->UpdateSubresource(m_interlace.cb.get(), 0, nullptr, &cb, 0, 0);

	StretchRect(sTex, sRect, dTex, dRect, m_interlace.ps[shader].get(), m_interlace.cb.get(), linear);
}

void GSDevice11::DoExternalFX(GSTexture* sTex, GSTexture* dTex)
{
	const GSVector2i s = dTex->GetSize();

	const GSVector4 sRect(0, 0, 1, 1);
	const GSVector4 dRect(0, 0, s.x, s.y);

	ExternalFXConstantBuffer cb;

	if (m_shaderfx.ps == nullptr)
	{
		try
		{
			std::string config_name(theApp.GetConfigS("shaderfx_conf"));
			std::ifstream fconfig(config_name);
			std::stringstream shader;
			if (fconfig.good())
				shader << fconfig.rdbuf() << "\n";
			else
				fprintf(stderr, "GS: External shader config '%s' not loaded.\n", config_name.c_str());

			std::string shader_name(theApp.GetConfigS("shaderfx_glsl"));
			std::ifstream fshader(shader_name);
			if (!fshader.good())
			{
				fprintf(stderr, "GS: External shader '%s' not loaded and will be disabled!\n", shader_name.c_str());
				return;
			}

			shader << fshader.rdbuf();
			const std::string& s = shader.str();
			std::vector<char> buff(s.begin(), s.end());
			ShaderMacro sm(m_shader.model);
			CreateShader(buff, shader_name.c_str(), D3D_COMPILE_STANDARD_FILE_INCLUDE, "ps_main", sm.GetPtr(), m_shaderfx.ps.put());
		}
		catch (GSRecoverableError)
		{
			printf("GS: Failed to compile external post-processing shader.\n");
		}
	}

	cb.xyFrame = GSVector2((float)s.x, (float)s.y);
	cb.rcpFrame = GSVector4(1.0f / (float)s.x, 1.0f / (float)s.y, 0.0f, 0.0f);
	cb.rcpFrameOpt = GSVector4::zero();

	m_ctx->UpdateSubresource(m_shaderfx.cb.get(), 0, nullptr, &cb, 0, 0);

	StretchRect(sTex, sRect, dTex, dRect, m_shaderfx.ps.get(), m_shaderfx.cb.get(), true);
}

void GSDevice11::DoFXAA(GSTexture* sTex, GSTexture* dTex)
{
	const GSVector2i s = dTex->GetSize();

	const GSVector4 sRect(0, 0, 1, 1);
	const GSVector4 dRect(0, 0, s.x, s.y);

	FXAAConstantBuffer cb;

	if (m_fxaa.ps == nullptr)
	{
		try
		{
			std::vector<char> shader;
			theApp.LoadResource(IDR_FXAA_FX, shader);
			ShaderMacro sm(m_shader.model);
			CreateShader(shader, "fxaa.fx", nullptr, "ps_main", sm.GetPtr(), m_fxaa.ps.put());
		}
		catch (GSRecoverableError)
		{
			printf("GS: Failed to compile fxaa shader.\n");
		}
	}

	cb.rcpFrame = GSVector4(1.0f / s.x, 1.0f / s.y, 0.0f, 0.0f);
	cb.rcpFrameOpt = GSVector4::zero();

	m_ctx->UpdateSubresource(m_fxaa.cb.get(), 0, nullptr, &cb, 0, 0);

	StretchRect(sTex, sRect, dTex, dRect, m_fxaa.ps.get(), m_fxaa.cb.get(), true);

	//sTex->Save("c:\\temp1\\1.bmp");
	//dTex->Save("c:\\temp1\\2.bmp");
}

void GSDevice11::DoShadeBoost(GSTexture* sTex, GSTexture* dTex)
{
	const GSVector2i s = dTex->GetSize();

	const GSVector4 sRect(0, 0, 1, 1);
	const GSVector4 dRect(0, 0, s.x, s.y);

	ShadeBoostConstantBuffer cb;

	cb.rcpFrame = GSVector4(1.0f / s.x, 1.0f / s.y, 0.0f, 0.0f);
	cb.rcpFrameOpt = GSVector4::zero();

	m_ctx->UpdateSubresource(m_shadeboost.cb.get(), 0, nullptr, &cb, 0, 0);

	StretchRect(sTex, sRect, dTex, dRect, m_shadeboost.ps.get(), m_shadeboost.cb.get(), true);
}

void GSDevice11::SetupDATE(GSTexture* rt, GSTexture* ds, const GSVertexPT1* vertices, bool datm)
{
	// sfex3 (after the capcom logo), vf4 (first menu fading in), ffxii shadows, rumble roses shadows, persona4 shadows

	BeginScene();

	ClearStencil(ds, 0);

	// om

	OMSetDepthStencilState(m_date.dss.get(), 1);
	OMSetBlendState(m_date.bs.get(), 0);
	OMSetRenderTargets(nullptr, ds);

	// ia

	IASetVertexBuffer(vertices, sizeof(vertices[0]), 4);
	IASetInputLayout(m_convert.il.get());
	IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// vs

	VSSetShader(m_convert.vs.get(), nullptr);

	// gs

	GSSetShader(nullptr, nullptr);

	// ps
	PSSetShaderResources(rt, nullptr);
	PSSetSamplerState(m_convert.pt.get(), nullptr);
	PSSetShader(m_convert.ps[static_cast<int>(datm ? ShaderConvert::DATM_1 : ShaderConvert::DATM_0)].get(), nullptr);

	//

	DrawPrimitive();

	//

	EndScene();
}

void GSDevice11::IASetVertexBuffer(const void* vertex, size_t stride, size_t count)
{
	void* ptr = nullptr;

	if (IAMapVertexBuffer(&ptr, stride, count))
	{
		GSVector4i::storent(ptr, vertex, count * stride);

		IAUnmapVertexBuffer();
	}
}

bool GSDevice11::IAMapVertexBuffer(void** vertex, size_t stride, size_t count)
{
	ASSERT(m_vertex.count == 0);

	if (count * stride > m_vertex.limit * m_vertex.stride)
	{
		m_vb.reset();

		m_vertex.start = 0;
		m_vertex.limit = std::max<int>(count * 3 / 2, 11000);
	}

	if (m_vb == nullptr)
	{
		D3D11_BUFFER_DESC bd;

		memset(&bd, 0, sizeof(bd));

		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = m_vertex.limit * stride;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		const HRESULT hr = m_dev->CreateBuffer(&bd, nullptr, m_vb.put());

		if (FAILED(hr))
			return false;
	}

	D3D11_MAP type = D3D11_MAP_WRITE_NO_OVERWRITE;

	if (m_vertex.start + count > m_vertex.limit || stride != m_vertex.stride)
	{
		m_vertex.start = 0;

		type = D3D11_MAP_WRITE_DISCARD;
	}

	D3D11_MAPPED_SUBRESOURCE m;

	if (FAILED(m_ctx->Map(m_vb.get(), 0, type, 0, &m)))
	{
		return false;
	}

	*vertex = (u8*)m.pData + m_vertex.start * stride;

	m_vertex.count = count;
	m_vertex.stride = stride;

	return true;
}

void GSDevice11::IAUnmapVertexBuffer()
{
	m_ctx->Unmap(m_vb.get(), 0);

	IASetVertexBuffer(m_vb.get(), m_vertex.stride);
}

void GSDevice11::IASetVertexBuffer(ID3D11Buffer* vb, size_t stride)
{
	if (m_state.vb != vb || m_state.vb_stride != stride)
	{
		m_state.vb = vb;
		m_state.vb_stride = stride;

		const u32 stride2 = stride;
		const u32 offset = 0;

		m_ctx->IASetVertexBuffers(0, 1, &vb, &stride2, &offset);
	}
}

void GSDevice11::IASetIndexBuffer(const void* index, size_t count)
{
	ASSERT(m_index.count == 0);

	if (count > m_index.limit)
	{
		m_ib.reset();

		m_index.start = 0;
		m_index.limit = std::max<int>(count * 3 / 2, 11000);
	}

	if (m_ib == nullptr)
	{
		D3D11_BUFFER_DESC bd;

		memset(&bd, 0, sizeof(bd));

		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = m_index.limit * sizeof(u32);
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		HRESULT hr = m_dev->CreateBuffer(&bd, nullptr, m_ib.put());

		if (FAILED(hr))
			return;
	}

	D3D11_MAP type = D3D11_MAP_WRITE_NO_OVERWRITE;

	if (m_index.start + count > m_index.limit)
	{
		m_index.start = 0;

		type = D3D11_MAP_WRITE_DISCARD;
	}

	D3D11_MAPPED_SUBRESOURCE m;

	if (SUCCEEDED(m_ctx->Map(m_ib.get(), 0, type, 0, &m)))
	{
		memcpy((u8*)m.pData + m_index.start * sizeof(u32), index, count * sizeof(u32));

		m_ctx->Unmap(m_ib.get(), 0);
	}

	m_index.count = count;

	IASetIndexBuffer(m_ib.get());
}

void GSDevice11::IASetIndexBuffer(ID3D11Buffer* ib)
{
	if (m_state.ib != ib)
	{
		m_state.ib = ib;

		m_ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
	}
}

void GSDevice11::IASetInputLayout(ID3D11InputLayout* layout)
{
	if (m_state.layout != layout)
	{
		m_state.layout = layout;

		m_ctx->IASetInputLayout(layout);
	}
}

void GSDevice11::IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY topology)
{
	if (m_state.topology != topology)
	{
		m_state.topology = topology;

		m_ctx->IASetPrimitiveTopology(topology);
	}
}

void GSDevice11::VSSetShader(ID3D11VertexShader* vs, ID3D11Buffer* vs_cb)
{
	if (m_state.vs != vs)
	{
		m_state.vs = vs;

		m_ctx->VSSetShader(vs, nullptr, 0);
	}

	if (m_state.vs_cb != vs_cb)
	{
		m_state.vs_cb = vs_cb;

		m_ctx->VSSetConstantBuffers(0, 1, &vs_cb);
	}
}

void GSDevice11::GSSetShader(ID3D11GeometryShader* gs, ID3D11Buffer* gs_cb)
{
	if (m_state.gs != gs)
	{
		m_state.gs = gs;

		m_ctx->GSSetShader(gs, nullptr, 0);
	}

	if (m_state.gs_cb != gs_cb)
	{
		m_state.gs_cb = gs_cb;

		m_ctx->GSSetConstantBuffers(0, 1, &gs_cb);
	}
}

void GSDevice11::PSSetShaderResources(GSTexture* sr0, GSTexture* sr1)
{
	PSSetShaderResource(0, sr0);
	PSSetShaderResource(1, sr1);

	for (size_t i = 2; i < m_state.ps_sr_views.size(); i++)
	{
		PSSetShaderResource(i, nullptr);
	}
}

void GSDevice11::PSSetShaderResource(int i, GSTexture* sr)
{
	ID3D11ShaderResourceView* srv = nullptr;

	if (sr)
		srv = *(GSTexture11*)sr;

	PSSetShaderResourceView(i, srv, sr);
}

void GSDevice11::PSSetShaderResourceView(int i, ID3D11ShaderResourceView* srv, GSTexture* sr)
{
	ASSERT(i < (int)m_state.ps_sr_views.size());

	if (m_state.ps_sr_views[i] != srv)
	{
		m_state.ps_sr_views[i] = srv;
		m_state.ps_sr_texture[i] = (GSTexture11*)sr;
		srv ? m_state.ps_sr_bitfield |= 1u << i : m_state.ps_sr_bitfield &= ~(1u << i);
	}
}

void GSDevice11::PSSetSamplerState(ID3D11SamplerState* ss0, ID3D11SamplerState* ss1)
{
	if (m_state.ps_ss[0] != ss0 || m_state.ps_ss[1] != ss1)
	{
		m_state.ps_ss[0] = ss0;
		m_state.ps_ss[1] = ss1;
	}
}

void GSDevice11::PSSetShader(ID3D11PixelShader* ps, ID3D11Buffer* ps_cb)
{
	if (m_state.ps != ps)
	{
		m_state.ps = ps;

		m_ctx->PSSetShader(ps, nullptr, 0);
	}

	if (m_state.ps_cb != ps_cb)
	{
		m_state.ps_cb = ps_cb;

		m_ctx->PSSetConstantBuffers(0, 1, &ps_cb);
	}
}

void GSDevice11::PSUpdateShaderState()
{
	m_ctx->PSSetShaderResources(0, m_state.ps_sr_views.size(), m_state.ps_sr_views.data());
	m_ctx->PSSetSamplers(0, std::size(m_state.ps_ss), m_state.ps_ss);
}

void GSDevice11::OMSetDepthStencilState(ID3D11DepthStencilState* dss, u8 sref)
{
	if (m_state.dss != dss || m_state.sref != sref)
	{
		m_state.dss = dss;
		m_state.sref = sref;

		m_ctx->OMSetDepthStencilState(dss, sref);
	}
}

void GSDevice11::OMSetBlendState(ID3D11BlendState* bs, float bf)
{
	if (m_state.bs != bs || m_state.bf != bf)
	{
		m_state.bs = bs;
		m_state.bf = bf;

		const float BlendFactor[] = {bf, bf, bf, 0};

		m_ctx->OMSetBlendState(bs, BlendFactor, 0xffffffff);
	}
}

void GSDevice11::OMSetRenderTargets(GSTexture* rt, GSTexture* ds, const GSVector4i* scissor)
{
	ID3D11RenderTargetView* rtv = nullptr;
	ID3D11DepthStencilView* dsv = nullptr;

	if (!rt && !ds)
		throw GSRecoverableError();

	if (rt) rtv = *(GSTexture11*)rt;
	if (ds) dsv = *(GSTexture11*)ds;

	if (m_state.rt_view != rtv || m_state.dsv != dsv)
	{
		m_state.rt_view = rtv;
		m_state.rt_texture = static_cast<GSTexture11*>(rt);
		m_state.dsv = dsv;
		m_state.rt_ds = static_cast<GSTexture11*>(ds);

		m_ctx->OMSetRenderTargets(1, &rtv, dsv);
	}

	const GSVector2i size = rt ? rt->GetSize() : ds->GetSize();
	if (m_state.viewport != size)
	{
		m_state.viewport = size;

		D3D11_VIEWPORT vp;
		memset(&vp, 0, sizeof(vp));

		vp.TopLeftX = m_hack_topleft_offset;
		vp.TopLeftY = m_hack_topleft_offset;
		vp.Width = (float)size.x;
		vp.Height = (float)size.y;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;

		m_ctx->RSSetViewports(1, &vp);
	}

	GSVector4i r = scissor ? *scissor : GSVector4i(size).zwxy();

	if (!m_state.scissor.eq(r))
	{
		m_state.scissor = r;

		m_ctx->RSSetScissorRects(1, r);
	}
}

GSDevice11::ShaderMacro::ShaderMacro(std::string& smodel)
{
	mlist.emplace_back("SHADER_MODEL", smodel);
}

void GSDevice11::ShaderMacro::AddMacro(const char* n, int d)
{
	mlist.emplace_back(n, std::to_string(d));
}

D3D_SHADER_MACRO* GSDevice11::ShaderMacro::GetPtr(void)
{
	mout.clear();

	for (auto& i : mlist)
		mout.emplace_back(i.name.c_str(), i.def.c_str());

	mout.emplace_back(nullptr, nullptr);
	return (D3D_SHADER_MACRO*)mout.data();
}

void GSDevice11::CreateShader(const std::vector<char>& source, const char* fn, ID3DInclude* include, const char* entry, D3D_SHADER_MACRO* macro, ID3D11VertexShader** vs, D3D11_INPUT_ELEMENT_DESC* layout, int count, ID3D11InputLayout** il)
{
	HRESULT hr;

	wil::com_ptr_nothrow<ID3DBlob> shader;

	CompileShader(source, fn, include, entry, macro, shader.put(), m_shader.vs);

	hr = m_dev->CreateVertexShader(shader->GetBufferPointer(), shader->GetBufferSize(), nullptr, vs);

	if (FAILED(hr))
	{
		throw GSRecoverableError();
	}

	hr = m_dev->CreateInputLayout(layout, count, shader->GetBufferPointer(), shader->GetBufferSize(), il);

	if (FAILED(hr))
	{
		throw GSRecoverableError();
	}
}

void GSDevice11::CreateShader(const std::vector<char>& source, const char* fn, ID3DInclude* include, const char* entry, D3D_SHADER_MACRO* macro, ID3D11GeometryShader** gs)
{
	wil::com_ptr_nothrow<ID3DBlob> shader;

	CompileShader(source, fn, include, entry, macro, shader.put(), m_shader.gs);

	HRESULT hr = m_dev->CreateGeometryShader(shader->GetBufferPointer(), shader->GetBufferSize(), nullptr, gs);

	if (FAILED(hr))
	{
		throw GSRecoverableError();
	}
}

void GSDevice11::CreateShader(const std::vector<char>& source, const char* fn, ID3DInclude* include, const char* entry, D3D_SHADER_MACRO* macro, ID3D11PixelShader** ps)
{
	wil::com_ptr_nothrow<ID3DBlob> shader;

	CompileShader(source, fn, include, entry, macro, shader.put(), m_shader.ps);

	HRESULT hr = m_dev->CreatePixelShader(shader->GetBufferPointer(), shader->GetBufferSize(), nullptr, ps);

	if (FAILED(hr))
	{
		throw GSRecoverableError();
	}
}

void GSDevice11::CompileShader(const std::vector<char>& source, const char* fn, ID3DInclude* include, const char* entry, D3D_SHADER_MACRO* macro, ID3DBlob** shader, const std::string& shader_model)
{
	wil::com_ptr_nothrow<ID3DBlob> error;

	UINT flags = 0;

#ifdef _DEBUG
	flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_AVOID_FLOW_CONTROL;
#endif

	const HRESULT hr = D3DCompile(
		source.data(), source.size(), fn, macro,
		include, entry, shader_model.c_str(),
		flags, 0, shader, error.put());

	if (error)
		fprintf(stderr, "%s\n", (const char*)error->GetBufferPointer());

	if (FAILED(hr))
		throw GSRecoverableError();
}

u16 GSDevice11::ConvertBlendEnum(u16 generic)
{
	switch (generic)
	{
		case SRC_COLOR:       return D3D11_BLEND_SRC_COLOR;
		case INV_SRC_COLOR:   return D3D11_BLEND_INV_SRC_COLOR;
		case DST_COLOR:       return D3D11_BLEND_DEST_COLOR;
		case INV_DST_COLOR:   return D3D11_BLEND_INV_DEST_COLOR;
		case SRC1_COLOR:      return D3D11_BLEND_SRC1_COLOR;
		case INV_SRC1_COLOR:  return D3D11_BLEND_INV_SRC1_COLOR;
		case SRC_ALPHA:       return D3D11_BLEND_SRC_ALPHA;
		case INV_SRC_ALPHA:   return D3D11_BLEND_INV_SRC_ALPHA;
		case DST_ALPHA:       return D3D11_BLEND_DEST_ALPHA;
		case INV_DST_ALPHA:   return D3D11_BLEND_INV_DEST_ALPHA;
		case SRC1_ALPHA:      return D3D11_BLEND_SRC1_ALPHA;
		case INV_SRC1_ALPHA:  return D3D11_BLEND_INV_SRC1_ALPHA;
		case CONST_COLOR:     return D3D11_BLEND_BLEND_FACTOR;
		case INV_CONST_COLOR: return D3D11_BLEND_INV_BLEND_FACTOR;
		case CONST_ONE:       return D3D11_BLEND_ONE;
		case CONST_ZERO:      return D3D11_BLEND_ZERO;
		case OP_ADD:          return D3D11_BLEND_OP_ADD;
		case OP_SUBTRACT:     return D3D11_BLEND_OP_SUBTRACT;
		case OP_REV_SUBTRACT: return D3D11_BLEND_OP_REV_SUBTRACT;
		default:              ASSERT(0); return 0;
	}
}
