/*
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"
#include "GSdx.h"
#include "GSDevice11.h"
#include "GSUtil.h"
#include "resource.h"
#include <fstream>
#include <VersionHelpers.h>

HMODULE GSDevice11::s_d3d_compiler_dll = nullptr;
decltype(&D3DCompile) GSDevice11::s_pD3DCompile = nullptr;
bool GSDevice11::s_old_d3d_compiler_dll;

GSDevice11::GSDevice11()
{
	memset(&m_state, 0, sizeof(m_state));
	memset(&m_vs_cb_cache, 0, sizeof(m_vs_cb_cache));
	memset(&m_gs_cb_cache, 0, sizeof(m_gs_cb_cache));
	memset(&m_ps_cb_cache, 0, sizeof(m_ps_cb_cache));

	FXAA_Compiled = false;
	ExShader_Compiled = false;

	m_state.topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	m_state.bf = -1;

	m_mipmap = theApp.GetConfigI("mipmap");
	m_upscale_multiplier = theApp.GetConfigI("upscale_multiplier");
}

bool GSDevice11::LoadD3DCompiler()
{
	// Windows 8.1 and later come with the latest d3dcompiler_47.dll, but
	// Windows 7 devs might also have the dll available for use (which will
	// have to be placed in the application directory)
	s_d3d_compiler_dll = LoadLibraryEx(D3DCOMPILER_DLL, nullptr, LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);

	// Windows Vista and 7 can use the older version. If the previous LoadLibrary
	// call fails on Windows 8.1 and later, then the user's system is likely
	// broken.
	if (s_d3d_compiler_dll)
	{
		s_old_d3d_compiler_dll = false;
	}
	else
	{
		if (!IsWindows8Point1OrGreater())
			// Use LoadLibrary instead of LoadLibraryEx, some Windows 7 systems
			// have issues with it.
			s_d3d_compiler_dll = LoadLibrary("D3DCompiler_43.dll");

		if (s_d3d_compiler_dll == nullptr)
			return false;

		s_old_d3d_compiler_dll = true;
	}

	s_pD3DCompile = reinterpret_cast<decltype(&D3DCompile)>(GetProcAddress(s_d3d_compiler_dll, "D3DCompile"));
	if (s_pD3DCompile)
		return true;

	FreeLibrary(s_d3d_compiler_dll);
	s_d3d_compiler_dll = nullptr;
	return false;
}

void GSDevice11::FreeD3DCompiler()
{
	s_pD3DCompile = nullptr;
	if (s_d3d_compiler_dll)
		FreeLibrary(s_d3d_compiler_dll);
	s_d3d_compiler_dll = nullptr;
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

bool GSDevice11::Create(const std::shared_ptr<GSWnd> &wnd)
{
	if(!__super::Create(wnd))
	{
		return false;
	}

	HRESULT hr = E_FAIL;

	DXGI_SWAP_CHAIN_DESC scd;
	D3D11_BUFFER_DESC bd;
	D3D11_SAMPLER_DESC sd;
	D3D11_DEPTH_STENCIL_DESC dsd;
	D3D11_RASTERIZER_DESC rd;
	D3D11_BLEND_DESC bsd;

	CComPtr<IDXGIAdapter1> adapter;
	D3D_DRIVER_TYPE driver_type = D3D_DRIVER_TYPE_HARDWARE;

	std::string adapter_id = theApp.GetConfigS("Adapter");

	if (adapter_id == "default")
		;
	else if (adapter_id == "ref")
	{
		driver_type = D3D_DRIVER_TYPE_REFERENCE;
	}
	else
	{
		CComPtr<IDXGIFactory1> dxgi_factory;
		CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&dxgi_factory);
		if (dxgi_factory)
			for (int i = 0;; i++)
			{
				CComPtr<IDXGIAdapter1> enum_adapter;
				if (S_OK != dxgi_factory->EnumAdapters1(i, &enum_adapter))
					break;
				DXGI_ADAPTER_DESC1 desc;
				hr = enum_adapter->GetDesc1(&desc);
				if (S_OK == hr && GSAdapter(desc) == adapter_id)
				{
					adapter = enum_adapter;
					driver_type = D3D_DRIVER_TYPE_UNKNOWN;
					break;
				}
			}
	}

	memset(&scd, 0, sizeof(scd));

	scd.BufferCount = 2;
	scd.BufferDesc.Width = 1;
	scd.BufferDesc.Height = 1;
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	//scd.BufferDesc.RefreshRate.Numerator = 60;
	//scd.BufferDesc.RefreshRate.Denominator = 1;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.OutputWindow = (HWND)m_wnd->GetHandle();
	scd.SampleDesc.Count = 1;
	scd.SampleDesc.Quality = 0;

	// Always start in Windowed mode.  According to MS, DXGI just "prefers" this, and it's more or less
	// required if we want to add support for dual displays later on.  The fullscreen/exclusive flip
	// will be issued after all other initializations are complete.

	scd.Windowed = TRUE;

	// NOTE : D3D11_CREATE_DEVICE_SINGLETHREADED
	//   This flag is safe as long as the DXGI's internal message pump is disabled or is on the
	//   same thread as the GS window (which the emulator makes sure of, if it utilizes a
	//   multithreaded GS).  Setting the flag is a nice and easy 5% speedup on GS-intensive scenes.

	uint32 flags = D3D11_CREATE_DEVICE_SINGLETHREADED;

#ifdef DEBUG
	flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_FEATURE_LEVEL level;

	const D3D_FEATURE_LEVEL levels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};

	hr = D3D11CreateDeviceAndSwapChain(adapter, driver_type, NULL, flags, levels, countof(levels), D3D11_SDK_VERSION, &scd, &m_swapchain, &m_dev, &level, &m_ctx);

	if(FAILED(hr)) return false;

	if(!SetFeatureLevel(level, true))
	{
		return false;
	}

	{	// HACK: check nVIDIA
		bool nvidia_gpu = false;
		IDXGIDevice *dxd;

		if(SUCCEEDED(m_dev->QueryInterface(IID_PPV_ARGS(&dxd))))
		{
			IDXGIAdapter *dxa;

			if(SUCCEEDED(dxd->GetAdapter(&dxa)))
			{
				DXGI_ADAPTER_DESC dxad;

				if(SUCCEEDED(dxa->GetDesc(&dxad)))
					nvidia_gpu = dxad.VendorId == 0x10DE;

				dxa->Release();
			}
			dxd->Release();
		}

		bool spritehack_enabled = theApp.GetConfigB("UserHacks") && theApp.GetConfigI("UserHacks_SpriteHack");

		m_hack_topleft_offset = (!nvidia_gpu || m_upscale_multiplier == 1 || spritehack_enabled) ? 0.0f : -0.01f;
	}

	D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS options;

	hr = m_dev->CheckFeatureSupport(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &options, sizeof(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS));

	// convert

	D3D11_INPUT_ELEMENT_DESC il_convert[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	std::vector<char> shader;
	theApp.LoadResource(IDR_CONVERT_FX, shader);
	CreateShader(shader, "convert.fx", nullptr, "vs_main", nullptr, &m_convert.vs, il_convert, countof(il_convert), &m_convert.il);

	std::string convert_mstr[1];

	convert_mstr[0] = format("%d", m_upscale_multiplier ? m_upscale_multiplier : 1);

	D3D_SHADER_MACRO convert_macro[] =
	{
		{"PS_SCALE_FACTOR", convert_mstr[0].c_str()},
		{NULL, NULL},
	};

	for(size_t i = 0; i < countof(m_convert.ps); i++)
	{
		CreateShader(shader, "convert.fx", nullptr, format("ps_main%d", i).c_str(), convert_macro, &m_convert.ps[i]);
	}

	memset(&dsd, 0, sizeof(dsd));

	hr = m_dev->CreateDepthStencilState(&dsd, &m_convert.dss);

	dsd.DepthEnable = true;
	dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dsd.DepthFunc = D3D11_COMPARISON_ALWAYS;

	hr = m_dev->CreateDepthStencilState(&dsd, &m_convert.dss_write);

	memset(&bsd, 0, sizeof(bsd));

	bsd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	hr = m_dev->CreateBlendState(&bsd, &m_convert.bs);

	// merge

	memset(&bd, 0, sizeof(bd));

	bd.ByteWidth = sizeof(MergeConstantBuffer);
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	hr = m_dev->CreateBuffer(&bd, NULL, &m_merge.cb);

	theApp.LoadResource(IDR_MERGE_FX, shader);
	for(size_t i = 0; i < countof(m_merge.ps); i++)
	{
		CreateShader(shader, "merge.fx", nullptr, format("ps_main%d", i).c_str(), nullptr, &m_merge.ps[i]);
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

	hr = m_dev->CreateBlendState(&bsd, &m_merge.bs);

	// interlace

	memset(&bd, 0, sizeof(bd));

	bd.ByteWidth = sizeof(InterlaceConstantBuffer);
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	hr = m_dev->CreateBuffer(&bd, NULL, &m_interlace.cb);

	theApp.LoadResource(IDR_INTERLACE_FX, shader);
	for(size_t i = 0; i < countof(m_interlace.ps); i++)
	{
		CreateShader(shader, "interlace.fx", nullptr, format("ps_main%d", i).c_str(), nullptr, &m_interlace.ps[i]);
	}

	// Shade Boos

	int ShadeBoost_Contrast = theApp.GetConfigI("ShadeBoost_Contrast");
	int ShadeBoost_Brightness = theApp.GetConfigI("ShadeBoost_Brightness");
	int ShadeBoost_Saturation = theApp.GetConfigI("ShadeBoost_Saturation");

	std::string str[3];

	str[0] = format("%d", ShadeBoost_Saturation);
	str[1] = format("%d", ShadeBoost_Brightness);
	str[2] = format("%d", ShadeBoost_Contrast);

	D3D_SHADER_MACRO macro[] =
	{
		{"SB_SATURATION", str[0].c_str()},
		{"SB_BRIGHTNESS", str[1].c_str()},
		{"SB_CONTRAST", str[2].c_str()},
		{NULL, NULL},
	};

	memset(&bd, 0, sizeof(bd));

	bd.ByteWidth = sizeof(ShadeBoostConstantBuffer);
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	hr = m_dev->CreateBuffer(&bd, NULL, &m_shadeboost.cb);

	theApp.LoadResource(IDR_SHADEBOOST_FX, shader);
	CreateShader(shader, "shadeboost.fx", nullptr, "ps_main", macro, &m_shadeboost.ps);

	// External fx shader

	memset(&bd, 0, sizeof(bd));

	bd.ByteWidth = sizeof(ExternalFXConstantBuffer);
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	hr = m_dev->CreateBuffer(&bd, NULL, &m_shaderfx.cb);

	// Fxaa

	memset(&bd, 0, sizeof(bd));

	bd.ByteWidth = sizeof(FXAAConstantBuffer);
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	hr = m_dev->CreateBuffer(&bd, NULL, &m_fxaa.cb);

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

	hr = m_dev->CreateRasterizerState(&rd, &m_rs);

	m_ctx->RSSetState(m_rs);

	//

	memset(&sd, 0, sizeof(sd));

	sd.Filter = theApp.GetConfigI("MaxAnisotropy") && !theApp.GetConfigB("paltex") ? D3D11_FILTER_ANISOTROPIC : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.MinLOD = -FLT_MAX;
	sd.MaxLOD = FLT_MAX;
	sd.MaxAnisotropy = theApp.GetConfigI("MaxAnisotropy");
	sd.ComparisonFunc = D3D11_COMPARISON_NEVER;

	hr = m_dev->CreateSamplerState(&sd, &m_convert.ln);

	sd.Filter = theApp.GetConfigI("MaxAnisotropy") && !theApp.GetConfigB("paltex") ? D3D11_FILTER_ANISOTROPIC : D3D11_FILTER_MIN_MAG_MIP_POINT;

	hr = m_dev->CreateSamplerState(&sd, &m_convert.pt);

	//

	Reset(1, 1);

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

	m_dev->CreateDepthStencilState(&dsd, &m_date.dss);

	D3D11_BLEND_DESC blend;

	memset(&blend, 0, sizeof(blend));

	m_dev->CreateBlendState(&blend, &m_date.bs);

	// Exclusive/Fullscreen flip, issued for legacy (managed) windows only.  GSopen2 style
	// emulators will issue the flip themselves later on.

	if(m_wnd->IsManaged())
	{
		SetExclusive(!theApp.GetConfigB("windowed"));
	}

	GSVector2i tex_font = m_osd.get_texture_font_size();

	m_font = std::unique_ptr<GSTexture>(
		CreateSurface(GSTexture::Texture, tex_font.x, tex_font.y, DXGI_FORMAT_R8_UNORM)
	);

	return true;
}

bool GSDevice11::Reset(int w, int h)
{
	if(!__super::Reset(w, h))
		return false;

	if(m_swapchain)
	{
		DXGI_SWAP_CHAIN_DESC scd;

		memset(&scd, 0, sizeof(scd));

		m_swapchain->GetDesc(&scd);
		m_swapchain->ResizeBuffers(scd.BufferCount, w, h, scd.BufferDesc.Format, 0);

		CComPtr<ID3D11Texture2D> backbuffer;

		if(FAILED(m_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backbuffer)))
		{
			return false;
		}

		m_backbuffer = new GSTexture11(backbuffer);
	}

	return true;
}

void GSDevice11::SetExclusive(bool isExcl)
{
	if(!m_swapchain) return;

	// TODO : Support for alternative display modes, by finishing this code below:
	//  Video mode info should be pulled form config/ini.

	/*DXGI_MODE_DESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.RefreshRate = 0;		// must be zero for best results.

	m_swapchain->ResizeTarget(&desc);
	*/

	HRESULT hr = m_swapchain->SetFullscreenState(isExcl, NULL);

	if(hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
	{
		fprintf(stderr, "(GSdx10) SetExclusive(%s) failed; request unavailable.", isExcl ? "true" : "false");
	}
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
			OutputDebugString(format("WARNING: FB read detected on slot %i, copying...", i).c_str());
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
		OutputDebugString(format("WARNING: Cleaning up copied texture on slot %i", i).c_str());
#endif
		Recycle(m_state.ps_sr_texture[i]);
		PSSetShaderResource(i, NULL);
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

void GSDevice11::Dispatch(uint32 x, uint32 y, uint32 z)
{
	m_ctx->Dispatch(x, y, z);
}

void GSDevice11::ClearRenderTarget(GSTexture* t, const GSVector4& c)
{
	if (!t) return;
	m_ctx->ClearRenderTargetView(*(GSTexture11*)t, c.v);
}

void GSDevice11::ClearRenderTarget(GSTexture* t, uint32 c)
{
	if (!t) return;
	GSVector4 color = GSVector4::rgba32(c) * (1.0f / 255);

	m_ctx->ClearRenderTargetView(*(GSTexture11*)t, color.v);
}

void GSDevice11::ClearDepth(GSTexture* t)
{
	if (!t) return;
	m_ctx->ClearDepthStencilView(*(GSTexture11*)t, D3D11_CLEAR_DEPTH, 0.0f, 0);
}

void GSDevice11::ClearStencil(GSTexture* t, uint8 c)
{
	if (!t) return;
	m_ctx->ClearDepthStencilView(*(GSTexture11*)t, D3D11_CLEAR_STENCIL, 0, c);
}

GSTexture* GSDevice11::CreateSurface(int type, int w, int h, int format)
{
	HRESULT hr;

	D3D11_TEXTURE2D_DESC desc;

	memset(&desc, 0, sizeof(desc));

	desc.Width = w;
	desc.Height = h;
	desc.Format = (DXGI_FORMAT)format;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;

	// mipmap = m_mipmap > 1 || m_filter != TriFiltering::None;
	bool mipmap = m_mipmap > 1;
	int layers = mipmap && format == DXGI_FORMAT_R8G8B8A8_UNORM ? (int)log2(std::max(w,h)) : 1;

	switch(type)
	{
	case GSTexture::RenderTarget:
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		break;
	case GSTexture::DepthStencil:
		desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		break;
	case GSTexture::Texture:
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.MipLevels = layers;
		break;
	case GSTexture::Offscreen:
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags |= D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		break;
	}

	GSTexture11* t = NULL;

	CComPtr<ID3D11Texture2D> texture;

	hr = m_dev->CreateTexture2D(&desc, NULL, &texture);

	if(SUCCEEDED(hr))
	{
		t = new GSTexture11(texture);

		switch(type)
		{
		case GSTexture::RenderTarget:
			ClearRenderTarget(t, 0);
			break;
		case GSTexture::DepthStencil:
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

GSTexture* GSDevice11::FetchSurface(int type, int w, int h, int format)
{
	if (format == 0)
		format = (type == GSTexture::DepthStencil || type == GSTexture::SparseDepthStencil) ? DXGI_FORMAT_R32G8X24_TYPELESS : DXGI_FORMAT_R8G8B8A8_UNORM;

	return __super::FetchSurface(type, w, h, format);
}

GSTexture* GSDevice11::CopyOffscreen(GSTexture* src, const GSVector4& sRect, int w, int h, int format, int ps_shader)
{
	GSTexture* dst = NULL;

	if(format == 0)
	{
		format = DXGI_FORMAT_R8G8B8A8_UNORM;
	}

	ASSERT(format == DXGI_FORMAT_R8G8B8A8_UNORM || format == DXGI_FORMAT_R16_UINT || format == DXGI_FORMAT_R32_UINT);

	if(GSTexture* rt = CreateRenderTarget(w, h, format))
	{
		GSVector4 dRect(0, 0, w, h);

		StretchRect(src, sRect, rt, dRect, m_convert.ps[ps_shader], NULL);

		dst = CreateOffscreen(w, h, format);

		if(dst)
		{
			m_ctx->CopyResource(*(GSTexture11*)dst, *(GSTexture11*)rt);
		}

		Recycle(rt);
	}

	return dst;
}

void GSDevice11::CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r)
{
	if (!sTex || !dTex)
	{
		ASSERT(0);
		return;
	}

	D3D11_BOX box = { (UINT)r.left, (UINT)r.top, 0U, (UINT)r.right, (UINT)r.bottom, 1U };

	// DX api isn't happy if we pass a box for depth copy
	// It complains that depth/multisample must be a full copy
	// and asks us to use a NULL for the box
	bool depth = (sTex->GetType() == GSTexture::DepthStencil);
	auto pBox = depth ? nullptr : &box;

	m_ctx->CopySubresourceRegion(*(GSTexture11*)dTex, 0, 0, 0, 0, *(GSTexture11*)sTex, 0, pBox);
}

void GSDevice11::CloneTexture(GSTexture* src, GSTexture** dest)
{
	if (!src || !(src->GetType() == GSTexture::DepthStencil || src->GetType() == GSTexture::RenderTarget))
	{
		ASSERT(0);
		return;
	}

	int w = src->GetWidth();
	int h = src->GetHeight();

	if (src->GetType() == GSTexture::DepthStencil)
		*dest = CreateDepthStencil(w, h, src->GetFormat());
	else
		*dest = CreateRenderTarget(w, h, src->GetFormat());

	CopyRect(src, *dest, GSVector4i(0, 0, w, h));
}

void GSDevice11::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, int shader, bool linear)
{
	StretchRect(sTex, sRect, dTex, dRect, m_convert.ps[shader], NULL, linear);
}

void GSDevice11::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ID3D11PixelShader* ps, ID3D11Buffer* ps_cb, bool linear)
{
	StretchRect(sTex, sRect, dTex, dRect, ps, ps_cb, m_convert.bs, linear);
}

void GSDevice11::StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ID3D11PixelShader* ps, ID3D11Buffer* ps_cb, ID3D11BlendState* bs, bool linear)
{
	if(!sTex || !dTex)
	{
		ASSERT(0);
		return;
	}

	bool draw_in_depth = (ps == m_convert.ps[ShaderConvert_RGBA8_TO_FLOAT32] || ps == m_convert.ps[ShaderConvert_RGBA8_TO_FLOAT24] ||
		ps == m_convert.ps[ShaderConvert_RGBA8_TO_FLOAT16] || ps == m_convert.ps[ShaderConvert_RGB5A1_TO_FLOAT16]);

	BeginScene();

	GSVector2i ds = dTex->GetSize();

	// om

	
	if (draw_in_depth)
		OMSetDepthStencilState(m_convert.dss_write, 0);
	else
		OMSetDepthStencilState(m_convert.dss, 0);

	OMSetBlendState(bs, 0);

	if (draw_in_depth)
		OMSetRenderTargets(NULL, dTex);
	else
		OMSetRenderTargets(dTex, NULL);

	// ia

	float left = dRect.x * 2 / ds.x - 1.0f;
	float top = 1.0f - dRect.y * 2 / ds.y;
	float right = dRect.z * 2 / ds.x - 1.0f;
	float bottom = 1.0f - dRect.w * 2 / ds.y;

	GSVertexPT1 vertices[] =
	{
		{GSVector4(left, top, 0.5f, 1.0f), GSVector2(sRect.x, sRect.y)},
		{GSVector4(right, top, 0.5f, 1.0f), GSVector2(sRect.z, sRect.y)},
		{GSVector4(left, bottom, 0.5f, 1.0f), GSVector2(sRect.x, sRect.w)},
		{GSVector4(right, bottom, 0.5f, 1.0f), GSVector2(sRect.z, sRect.w)},
	};



	IASetVertexBuffer(vertices, sizeof(vertices[0]), countof(vertices));
	IASetInputLayout(m_convert.il);
	IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// vs

	VSSetShader(m_convert.vs, NULL);


	// gs
	/* NVIDIA HACK!!!!
	Not sure why, but having the Geometry shader disabled causes the strange stretching in recent drivers*/

	GSSelector sel;
	//Don't use shading for stretching, we're just passing through - Note: With Win10 it seems to cause other bugs when shading is off if any of the coords is greater than 0
	//I really don't know whats going on there, but this seems to resolve it mostly (if not all, not tester a lot of games, only BIOS, FFXII and VP2)
	//sel.iip = (sRect.y > 0.0f || sRect.w > 0.0f) ? 1 : 0;
	//sel.prim = 2; //Triangle Strip
	//SetupGS(sel);

	GSSetShader(NULL, NULL);

	/*END OF HACK*/

	//

	// ps

	PSSetShaderResources(sTex, NULL);
	PSSetSamplerState(linear ? m_convert.ln : m_convert.pt, NULL);
	PSSetShader(ps, ps_cb);

	//

	DrawPrimitive();

	//

	EndScene();

	PSSetShaderResources(NULL, NULL);
}

void GSDevice11::RenderOsd(GSTexture* dt)
{
	BeginScene();

	// om
	OMSetDepthStencilState(m_convert.dss, 0);
	OMSetBlendState(m_merge.bs, 0);
	OMSetRenderTargets(dt, NULL);

	if(m_osd.m_texture_dirty) {
		m_osd.upload_texture_atlas(m_font.get());
	}

	// ps
	PSSetShaderResource(0, m_font.get());
	PSSetSamplerState(m_convert.pt, NULL);
	PSSetShader(m_convert.ps[ShaderConvert_OSD], NULL);

	// ia
	IASetInputLayout(m_convert.il);
	IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Note scaling could also be done in shader (require gl3/dx10)
	size_t count = m_osd.Size();
	void* dst = NULL;

	IAMapVertexBuffer(&dst, sizeof(GSVertexPT1), count);
	count = m_osd.GeneratePrimitives((GSVertexPT1*)dst, count);
	IAUnmapVertexBuffer();

	// vs
	VSSetShader(m_convert.vs, NULL);

	// gs
	GSSetShader(NULL, NULL);

	DrawPrimitive();

	EndScene();
}

void GSDevice11::DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect, const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, const GSVector4& c)
{
	bool slbg = PMODE.SLBG;
	bool mmod = PMODE.MMOD;

	ClearRenderTarget(dTex, c);

	if(sTex[1] && !slbg)
	{
		StretchRect(sTex[1], sRect[1], dTex, dRect[1], m_merge.ps[0], NULL, true);
	}

	if(sTex[0])
	{
		m_ctx->UpdateSubresource(m_merge.cb, 0, NULL, &c, 0, 0);

		StretchRect(sTex[0], sRect[0], dTex, dRect[0], m_merge.ps[mmod ? 1 : 0], m_merge.cb, m_merge.bs, true);
	}
}

void GSDevice11::DoInterlace(GSTexture* sTex, GSTexture* dTex, int shader, bool linear, float yoffset)
{
	GSVector4 s = GSVector4(dTex->GetSize());

	GSVector4 sRect(0, 0, 1, 1);
	GSVector4 dRect(0.0f, yoffset, s.x, s.y + yoffset);

	InterlaceConstantBuffer cb;

	cb.ZrH = GSVector2(0, 1.0f / s.y);
	cb.hH = s.y / 2;

	m_ctx->UpdateSubresource(m_interlace.cb, 0, NULL, &cb, 0, 0);

	StretchRect(sTex, sRect, dTex, dRect, m_interlace.ps[shader], m_interlace.cb, linear);
}

//Included an init function for this also. Just to be safe.
void GSDevice11::InitExternalFX()
{
	if (!ExShader_Compiled)
	{
		try {
			std::string config_name(theApp.GetConfigS("shaderfx_conf"));
			std::ifstream fconfig(config_name);
			std::stringstream shader;
			if (fconfig.good())
				shader << fconfig.rdbuf() << "\n";
			else
				fprintf(stderr, "GSdx: External shader config '%s' not loaded.\n", config_name.c_str());

			std::string shader_name(theApp.GetConfigS("shaderfx_glsl"));
			std::ifstream fshader(shader_name);
			if (fshader.good())
			{
				shader << fshader.rdbuf();
				const std::string& s = shader.str();
				std::vector<char> buff(s.begin(), s.end());

				CreateShader(buff, shader_name.c_str(), D3D_COMPILE_STANDARD_FILE_INCLUDE, "ps_main", nullptr, &m_shaderfx.ps);
			}
			else
			{
				fprintf(stderr, "GSdx: External shader '%s' not loaded and will be disabled!\n", shader_name.c_str());
			}
		}
		catch (GSDXRecoverableError) {
			printf("GSdx: failed to compile external post-processing shader. \n");
		}
		ExShader_Compiled = true;
	}
}

void GSDevice11::DoExternalFX(GSTexture* sTex, GSTexture* dTex)
{
	GSVector2i s = dTex->GetSize();

	GSVector4 sRect(0, 0, 1, 1);
	GSVector4 dRect(0, 0, s.x, s.y);

	ExternalFXConstantBuffer cb;

	InitExternalFX();

	cb.xyFrame = GSVector2((float)s.x, (float)s.y);
	cb.rcpFrame = GSVector4(1.0f / (float)s.x, 1.0f / (float)s.y, 0.0f, 0.0f);
	cb.rcpFrameOpt = GSVector4::zero();

	m_ctx->UpdateSubresource(m_shaderfx.cb, 0, NULL, &cb, 0, 0);

	StretchRect(sTex, sRect, dTex, dRect, m_shaderfx.ps, m_shaderfx.cb, true);
}

// This shouldn't be necessary, we have some bug corrupting memory
// and for some reason isolating this code makes the plugin not crash
void GSDevice11::InitFXAA()
{
	if (!FXAA_Compiled)
	{
		try {
			std::vector<char> shader;
			theApp.LoadResource(IDR_FXAA_FX, shader);
			CreateShader(shader, "fxaa.fx", nullptr, "ps_main", nullptr, &m_fxaa.ps);
		}
		catch (GSDXRecoverableError) {
			printf("GSdx: failed to compile fxaa shader.\n");
		}
		FXAA_Compiled = true;
	}
}

void GSDevice11::DoFXAA(GSTexture* sTex, GSTexture* dTex)
{
	GSVector2i s = dTex->GetSize();

	GSVector4 sRect(0, 0, 1, 1);
	GSVector4 dRect(0, 0, s.x, s.y);

	FXAAConstantBuffer cb;

	InitFXAA();

	cb.rcpFrame = GSVector4(1.0f / s.x, 1.0f / s.y, 0.0f, 0.0f);
	cb.rcpFrameOpt = GSVector4::zero();

	m_ctx->UpdateSubresource(m_fxaa.cb, 0, NULL, &cb, 0, 0);

	StretchRect(sTex, sRect, dTex, dRect, m_fxaa.ps, m_fxaa.cb, true);

	//sTex->Save("c:\\temp1\\1.bmp");
	//dTex->Save("c:\\temp1\\2.bmp");
}

void GSDevice11::DoShadeBoost(GSTexture* sTex, GSTexture* dTex)
{
	GSVector2i s = dTex->GetSize();

	GSVector4 sRect(0, 0, 1, 1);
	GSVector4 dRect(0, 0, s.x, s.y);

	ShadeBoostConstantBuffer cb;

	cb.rcpFrame = GSVector4(1.0f / s.x, 1.0f / s.y, 0.0f, 0.0f);
	cb.rcpFrameOpt = GSVector4::zero();

	m_ctx->UpdateSubresource(m_shadeboost.cb, 0, NULL, &cb, 0, 0);

	StretchRect(sTex, sRect, dTex, dRect, m_shadeboost.ps, m_shadeboost.cb, true);
}

void GSDevice11::SetupDATE(GSTexture* rt, GSTexture* ds, const GSVertexPT1* vertices, bool datm)
{
	// sfex3 (after the capcom logo), vf4 (first menu fading in), ffxii shadows, rumble roses shadows, persona4 shadows

	BeginScene();

	ClearStencil(ds, 0);

	// om

	OMSetDepthStencilState(m_date.dss, 1);
	OMSetBlendState(m_date.bs, 0);
	OMSetRenderTargets(NULL, ds);

	// ia

	IASetVertexBuffer(vertices, sizeof(vertices[0]), 4);
	IASetInputLayout(m_convert.il);
	IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// vs

	VSSetShader(m_convert.vs, NULL);

	// gs

	GSSetShader(NULL, NULL);

	// ps
	PSSetShaderResources(rt, NULL);
	PSSetSamplerState(m_convert.pt, NULL);
	PSSetShader(m_convert.ps[datm ? ShaderConvert_DATM_1 : ShaderConvert_DATM_0], NULL);

	//

	DrawPrimitive();

	//

	EndScene();
}

void GSDevice11::IASetVertexBuffer(const void* vertex, size_t stride, size_t count)
{
	void* ptr = NULL;

	if(IAMapVertexBuffer(&ptr, stride, count))
	{
		GSVector4i::storent(ptr, vertex, count * stride);

		IAUnmapVertexBuffer();
	}
}

bool GSDevice11::IAMapVertexBuffer(void** vertex, size_t stride, size_t count)
{
	ASSERT(m_vertex.count == 0);

	if(count * stride > m_vertex.limit * m_vertex.stride)
	{
		m_vb_old = m_vb;
		m_vb = NULL;

		m_vertex.start = 0;
		m_vertex.limit = std::max<int>(count * 3 / 2, 11000);
	}

	if(m_vb == NULL)
	{
		D3D11_BUFFER_DESC bd;

		memset(&bd, 0, sizeof(bd));

		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = m_vertex.limit * stride;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		HRESULT hr;

		hr = m_dev->CreateBuffer(&bd, NULL, &m_vb);

		if(FAILED(hr)) return false;
	}

	D3D11_MAP type = D3D11_MAP_WRITE_NO_OVERWRITE;

	if(m_vertex.start + count > m_vertex.limit || stride != m_vertex.stride)
	{
		m_vertex.start = 0;

		type = D3D11_MAP_WRITE_DISCARD;
	}

	D3D11_MAPPED_SUBRESOURCE m;

	if(FAILED(m_ctx->Map(m_vb, 0, type, 0, &m)))
	{
		return false;
	}

	*vertex = (uint8*)m.pData + m_vertex.start * stride;

	m_vertex.count = count;
	m_vertex.stride = stride;

	return true;
}

void GSDevice11::IAUnmapVertexBuffer()
{
	m_ctx->Unmap(m_vb, 0);

	IASetVertexBuffer(m_vb, m_vertex.stride);
}

void GSDevice11::IASetVertexBuffer(ID3D11Buffer* vb, size_t stride)
{
	if(m_state.vb != vb || m_state.vb_stride != stride)
	{
		m_state.vb = vb;
		m_state.vb_stride = stride;

		uint32 stride2 = stride;
		uint32 offset = 0;

		m_ctx->IASetVertexBuffers(0, 1, &vb, &stride2, &offset);
	}
}

void GSDevice11::IASetIndexBuffer(const void* index, size_t count)
{
	ASSERT(m_index.count == 0);

	if(count > m_index.limit)
	{
		m_ib_old = m_ib;
		m_ib = NULL;

		m_index.start = 0;
		m_index.limit = std::max<int>(count * 3 / 2, 11000);
	}

	if(m_ib == NULL)
	{
		D3D11_BUFFER_DESC bd;

		memset(&bd, 0, sizeof(bd));

		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = m_index.limit * sizeof(uint32);
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		HRESULT hr;

		hr = m_dev->CreateBuffer(&bd, NULL, &m_ib);

		if(FAILED(hr)) return;
	}

	D3D11_MAP type = D3D11_MAP_WRITE_NO_OVERWRITE;

	if(m_index.start + count > m_index.limit)
	{
		m_index.start = 0;

		type = D3D11_MAP_WRITE_DISCARD;
	}

	D3D11_MAPPED_SUBRESOURCE m;

	if(SUCCEEDED(m_ctx->Map(m_ib, 0, type, 0, &m)))
	{
		memcpy((uint8*)m.pData + m_index.start * sizeof(uint32), index, count * sizeof(uint32));

		m_ctx->Unmap(m_ib, 0);
	}

	m_index.count = count;

	IASetIndexBuffer(m_ib);
}

void GSDevice11::IASetIndexBuffer(ID3D11Buffer* ib)
{
	if(m_state.ib != ib)
	{
		m_state.ib = ib;

		m_ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
	}
}

void GSDevice11::IASetInputLayout(ID3D11InputLayout* layout)
{
	if(m_state.layout != layout)
	{
		m_state.layout = layout;

		m_ctx->IASetInputLayout(layout);
	}
}

void GSDevice11::IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY topology)
{
	if(m_state.topology != topology)
	{
		m_state.topology = topology;

		m_ctx->IASetPrimitiveTopology(topology);
	}
}

void GSDevice11::VSSetShader(ID3D11VertexShader* vs, ID3D11Buffer* vs_cb)
{
	if(m_state.vs != vs)
	{
		m_state.vs = vs;

		m_ctx->VSSetShader(vs, NULL, 0);
	}

	if(m_state.vs_cb != vs_cb)
	{
		m_state.vs_cb = vs_cb;

		m_ctx->VSSetConstantBuffers(0, 1, &vs_cb);
	}
}

void GSDevice11::GSSetShader(ID3D11GeometryShader* gs, ID3D11Buffer* gs_cb)
{
	if(m_state.gs != gs)
	{
		m_state.gs = gs;

		m_ctx->GSSetShader(gs, NULL, 0);
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

	for(size_t i = 2; i < m_state.ps_sr_views.size(); i++)
	{
		PSSetShaderResource(i, NULL);
	}
}

void GSDevice11::PSSetShaderResource(int i, GSTexture* sr)
{
	ID3D11ShaderResourceView* srv = NULL;

	if(sr) srv = *(GSTexture11*)sr;

	PSSetShaderResourceView(i, srv, sr);
}

void GSDevice11::PSSetShaderResourceView(int i, ID3D11ShaderResourceView* srv, GSTexture* sr)
{
	ASSERT(i < (int)m_state.ps_sr_views.size());

	if(m_state.ps_sr_views[i] != srv)
	{
		m_state.ps_sr_views[i] = srv;
		m_state.ps_sr_texture[i] = (GSTexture11*)sr;
		srv ? m_state.ps_sr_bitfield |= 1u << i : m_state.ps_sr_bitfield &= ~(1u << i);
	}
}

void GSDevice11::PSSetSamplerState(ID3D11SamplerState* ss0, ID3D11SamplerState* ss1)
{
	if(m_state.ps_ss[0] != ss0 || m_state.ps_ss[1] != ss1)
	{
		m_state.ps_ss[0] = ss0;
		m_state.ps_ss[1] = ss1;
	}
}

void GSDevice11::PSSetShader(ID3D11PixelShader* ps, ID3D11Buffer* ps_cb)
{
	if(m_state.ps != ps)
	{
		m_state.ps = ps;

		m_ctx->PSSetShader(ps, NULL, 0);
	}

	if(m_state.ps_cb != ps_cb)
	{
		m_state.ps_cb = ps_cb;

		m_ctx->PSSetConstantBuffers(0, 1, &ps_cb);
	}
}

void GSDevice11::PSUpdateShaderState()
{
	m_ctx->PSSetShaderResources(0, m_state.ps_sr_views.size(), m_state.ps_sr_views.data());
	m_ctx->PSSetSamplers(0, countof(m_state.ps_ss), m_state.ps_ss);
}

void GSDevice11::OMSetDepthStencilState(ID3D11DepthStencilState* dss, uint8 sref)
{
	if(m_state.dss != dss || m_state.sref != sref)
	{
		m_state.dss = dss;
		m_state.sref = sref;

		m_ctx->OMSetDepthStencilState(dss, sref);
	}
}

void GSDevice11::OMSetBlendState(ID3D11BlendState* bs, float bf)
{
	if(m_state.bs != bs || m_state.bf != bf)
	{
		m_state.bs = bs;
		m_state.bf = bf;

		float BlendFactor[] = {bf, bf, bf, 0};

		m_ctx->OMSetBlendState(bs, BlendFactor, 0xffffffff);
	}
}

void GSDevice11::OMSetRenderTargets(GSTexture* rt, GSTexture* ds, const GSVector4i* scissor)
{
	ID3D11RenderTargetView* rtv = NULL;
	ID3D11DepthStencilView* dsv = NULL;

	if (!rt && !ds)
		throw GSDXRecoverableError();

	if(rt) rtv = *(GSTexture11*)rt;
	if(ds) dsv = *(GSTexture11*)ds;

	if(m_state.rt_view != rtv || m_state.dsv != dsv)
	{
		m_state.rt_view = rtv;
		m_state.rt_texture = static_cast<GSTexture11*>(rt);
		m_state.dsv = dsv;
		m_state.rt_ds = static_cast<GSTexture11*>(ds);

		m_ctx->OMSetRenderTargets(1, &rtv, dsv);
	}

	GSVector2i size = rt ? rt->GetSize() : ds->GetSize();
	if(m_state.viewport != size)
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

	if(!m_state.scissor.eq(r))
	{
		m_state.scissor = r;

		m_ctx->RSSetScissorRects(1, r);
	}
}

void GSDevice11::CreateShader(std::vector<char> source, const char* fn, ID3DInclude *include, const char* entry, D3D_SHADER_MACRO* macro, ID3D11VertexShader** vs, D3D11_INPUT_ELEMENT_DESC* layout, int count, ID3D11InputLayout** il)
{
	HRESULT hr;

	CComPtr<ID3DBlob> shader;

	CompileShader(source, fn, include, entry, macro, &shader, m_shader.vs);

	hr = m_dev->CreateVertexShader((void*)shader->GetBufferPointer(), shader->GetBufferSize(), NULL, vs);

	if(FAILED(hr))
	{
		throw GSDXRecoverableError();
	}

	hr = m_dev->CreateInputLayout(layout, count, shader->GetBufferPointer(), shader->GetBufferSize(), il);

	if(FAILED(hr))
	{
		throw GSDXRecoverableError();
	}
}

void GSDevice11::CreateShader(std::vector<char> source, const char* fn, ID3DInclude *include, const char* entry, D3D_SHADER_MACRO* macro, ID3D11GeometryShader** gs)
{
	HRESULT hr;

	CComPtr<ID3DBlob> shader;

	CompileShader(source, fn, include, entry, macro, &shader, m_shader.gs);

	hr = m_dev->CreateGeometryShader((void*)shader->GetBufferPointer(), shader->GetBufferSize(), NULL, gs);

	if(FAILED(hr))
	{
		throw GSDXRecoverableError();
	}
}

void GSDevice11::CreateShader(std::vector<char> source, const char* fn, ID3DInclude *include, const char* entry, D3D_SHADER_MACRO* macro, ID3D11PixelShader** ps)
{
	HRESULT hr;

	CComPtr<ID3DBlob> shader;

	CompileShader(source, fn, include, entry, macro, &shader, m_shader.ps);

	hr = m_dev->CreatePixelShader((void*)shader->GetBufferPointer(), shader->GetBufferSize(), NULL, ps);

	if(FAILED(hr))
	{
		throw GSDXRecoverableError();
	}
}

void GSDevice11::CompileShader(std::vector<char> source, const char* fn, ID3DInclude *include, const char* entry, D3D_SHADER_MACRO* macro, ID3DBlob** shader, std::string shader_model)
{
	HRESULT hr;

	std::vector<D3D_SHADER_MACRO> m;

	PrepareShaderMacro(m, macro);

	CComPtr<ID3DBlob> error;

	UINT flags = 0;

#ifdef _DEBUG
	flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_AVOID_FLOW_CONTROL;
#endif

	hr = s_pD3DCompile(source.data(), source.size(), fn, &m[0], include, entry, shader_model.c_str(), flags, 0, shader, &error);

	if(error)
	{
		fprintf(stderr, "%s\n", (const char*)error->GetBufferPointer());
	}

	if(FAILED(hr))
	{
		throw GSDXRecoverableError();
	}
}

// (A - B) * C + D
// A: Cs/Cd/0
// B: Cs/Cd/0
// C: As/Ad/FIX
// D: Cs/Cd/0

// bogus: 0100, 0110, 0120, 0200, 0210, 0220, 1001, 1011, 1021
// tricky: 1201, 1211, 1221

// Source.rgb = float3(1, 1, 1);
// 1201 Cd*(1 + As) => Source * Dest color + Dest * Source alpha
// 1211 Cd*(1 + Ad) => Source * Dest color + Dest * Dest alpha
// 1221 Cd*(1 + F) => Source * Dest color + Dest * Factor

// Special blending method table:
// # (tricky) => 1 * Cd + Cd * F => Use (Cd, F) as factor of color (1, Cd)
// * (bogus) => C * (1 + F ) + ... => factor is always bigger than 1 (except above case)

const GSDevice11::D3D11Blend GSDevice11::m_blendMapD3D11[3*3*3*3] =
{
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ONE              , D3D11_BLEND_ZERO }             , // 0000: (Cs - Cs)*As + Cs ==> Cs
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ZERO             , D3D11_BLEND_ONE }              , // 0001: (Cs - Cs)*As + Cd ==> Cd
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ZERO             , D3D11_BLEND_ZERO }             , // 0002: (Cs - Cs)*As +  0 ==> 0
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ONE              , D3D11_BLEND_ZERO }             , // 0010: (Cs - Cs)*Ad + Cs ==> Cs
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ZERO             , D3D11_BLEND_ONE }              , // 0011: (Cs - Cs)*Ad + Cd ==> Cd
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ZERO             , D3D11_BLEND_ZERO }             , // 0012: (Cs - Cs)*Ad +  0 ==> 0
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ONE              , D3D11_BLEND_ZERO }             , // 0020: (Cs - Cs)*F  + Cs ==> Cs
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ZERO             , D3D11_BLEND_ONE }              , // 0021: (Cs - Cs)*F  + Cd ==> Cd
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ZERO             , D3D11_BLEND_ZERO }             , // 0022: (Cs - Cs)*F  +  0 ==> 0
	{ 1, D3D11_BLEND_OP_SUBTRACT     , D3D11_BLEND_ONE              , D3D11_BLEND_SRC1_ALPHA }       , //*0100: (Cs - Cd)*As + Cs ==> Cs*(As + 1) - Cd*As
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_SRC1_ALPHA       , D3D11_BLEND_INV_SRC1_ALPHA }   , // 0101: (Cs - Cd)*As + Cd ==> Cs*As + Cd*(1 - As)
	{ 0, D3D11_BLEND_OP_SUBTRACT     , D3D11_BLEND_SRC1_ALPHA       , D3D11_BLEND_SRC1_ALPHA }       , // 0102: (Cs - Cd)*As +  0 ==> Cs*As - Cd*As
	{ 1, D3D11_BLEND_OP_SUBTRACT     , D3D11_BLEND_ONE              , D3D11_BLEND_DEST_ALPHA }       , //*0110: (Cs - Cd)*Ad + Cs ==> Cs*(Ad + 1) - Cd*Ad
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_DEST_ALPHA       , D3D11_BLEND_INV_DEST_ALPHA }   , // 0111: (Cs - Cd)*Ad + Cd ==> Cs*Ad + Cd*(1 - Ad)
	{ 0, D3D11_BLEND_OP_SUBTRACT     , D3D11_BLEND_DEST_ALPHA       , D3D11_BLEND_DEST_ALPHA }       , // 0112: (Cs - Cd)*Ad +  0 ==> Cs*Ad - Cd*Ad
	{ 1, D3D11_BLEND_OP_SUBTRACT     , D3D11_BLEND_ONE              , D3D11_BLEND_BLEND_FACTOR }     , //*0120: (Cs - Cd)*F  + Cs ==> Cs*(F + 1) - Cd*F
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_BLEND_FACTOR     , D3D11_BLEND_INV_BLEND_FACTOR } , // 0121: (Cs - Cd)*F  + Cd ==> Cs*F + Cd*(1 - F)
	{ 0, D3D11_BLEND_OP_SUBTRACT     , D3D11_BLEND_BLEND_FACTOR     , D3D11_BLEND_BLEND_FACTOR }     , // 0122: (Cs - Cd)*F  +  0 ==> Cs*F - Cd*F
	{ 1, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ONE              , D3D11_BLEND_ZERO }             , //*0200: (Cs -  0)*As + Cs ==> Cs*(As + 1)
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_SRC1_ALPHA       , D3D11_BLEND_ONE }              , // 0201: (Cs -  0)*As + Cd ==> Cs*As + Cd
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_SRC1_ALPHA       , D3D11_BLEND_ZERO }             , // 0202: (Cs -  0)*As +  0 ==> Cs*As
	{ 1, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ONE              , D3D11_BLEND_ZERO }             , //*0210: (Cs -  0)*Ad + Cs ==> Cs*(Ad + 1)
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_DEST_ALPHA       , D3D11_BLEND_ONE }              , // 0211: (Cs -  0)*Ad + Cd ==> Cs*Ad + Cd
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_DEST_ALPHA       , D3D11_BLEND_ZERO }             , // 0212: (Cs -  0)*Ad +  0 ==> Cs*Ad
	{ 1, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ONE              , D3D11_BLEND_ZERO }             , //*0220: (Cs -  0)*F  + Cs ==> Cs*(F + 1)
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_BLEND_FACTOR     , D3D11_BLEND_ONE }              , // 0221: (Cs -  0)*F  + Cd ==> Cs*F + Cd
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_BLEND_FACTOR     , D3D11_BLEND_ZERO }             , // 0222: (Cs -  0)*F  +  0 ==> Cs*F
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_INV_SRC1_ALPHA   , D3D11_BLEND_SRC1_ALPHA }       , // 1000: (Cd - Cs)*As + Cs ==> Cd*As + Cs*(1 - As)
	{ 1, D3D11_BLEND_OP_REV_SUBTRACT , D3D11_BLEND_SRC1_ALPHA       , D3D11_BLEND_ONE }              , //*1001: (Cd - Cs)*As + Cd ==> Cd*(As + 1) - Cs*As
	{ 0, D3D11_BLEND_OP_REV_SUBTRACT , D3D11_BLEND_SRC1_ALPHA       , D3D11_BLEND_SRC1_ALPHA }       , // 1002: (Cd - Cs)*As +  0 ==> Cd*As - Cs*As
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_INV_DEST_ALPHA   , D3D11_BLEND_DEST_ALPHA }       , // 1010: (Cd - Cs)*Ad + Cs ==> Cd*Ad + Cs*(1 - Ad)
	{ 1, D3D11_BLEND_OP_REV_SUBTRACT , D3D11_BLEND_DEST_ALPHA       , D3D11_BLEND_ONE }              , //*1011: (Cd - Cs)*Ad + Cd ==> Cd*(Ad + 1) - Cs*Ad
	{ 0, D3D11_BLEND_OP_REV_SUBTRACT , D3D11_BLEND_DEST_ALPHA       , D3D11_BLEND_DEST_ALPHA }       , // 1012: (Cd - Cs)*Ad +  0 ==> Cd*Ad - Cs*Ad
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_INV_BLEND_FACTOR , D3D11_BLEND_BLEND_FACTOR }     , // 1020: (Cd - Cs)*F  + Cs ==> Cd*F + Cs*(1 - F)
	{ 1, D3D11_BLEND_OP_REV_SUBTRACT , D3D11_BLEND_BLEND_FACTOR     , D3D11_BLEND_ONE }              , //*1021: (Cd - Cs)*F  + Cd ==> Cd*(F + 1) - Cs*F
	{ 0, D3D11_BLEND_OP_REV_SUBTRACT , D3D11_BLEND_BLEND_FACTOR     , D3D11_BLEND_BLEND_FACTOR }     , // 1022: (Cd - Cs)*F  +  0 ==> Cd*F - Cs*F
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ONE              , D3D11_BLEND_ZERO }             , // 1100: (Cd - Cd)*As + Cs ==> Cs
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ZERO             , D3D11_BLEND_ONE }              , // 1101: (Cd - Cd)*As + Cd ==> Cd
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ZERO             , D3D11_BLEND_ZERO }             , // 1102: (Cd - Cd)*As +  0 ==> 0
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ONE              , D3D11_BLEND_ZERO }             , // 1110: (Cd - Cd)*Ad + Cs ==> Cs
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ZERO             , D3D11_BLEND_ONE }              , // 1111: (Cd - Cd)*Ad + Cd ==> Cd
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ZERO             , D3D11_BLEND_ZERO }             , // 1112: (Cd - Cd)*Ad +  0 ==> 0
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ONE              , D3D11_BLEND_ZERO }             , // 1120: (Cd - Cd)*F  + Cs ==> Cs
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ZERO             , D3D11_BLEND_ONE }              , // 1121: (Cd - Cd)*F  + Cd ==> Cd
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ZERO             , D3D11_BLEND_ZERO }             , // 1122: (Cd - Cd)*F  +  0 ==> 0
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ONE              , D3D11_BLEND_SRC1_ALPHA }       , // 1200: (Cd -  0)*As + Cs ==> Cs + Cd*As
	{ 2, D3D11_BLEND_OP_ADD          , D3D11_BLEND_DEST_COLOR       , D3D11_BLEND_SRC1_ALPHA }       , //#1201: (Cd -  0)*As + Cd ==> Cd*(1 + As) // ffxii main menu background glow effect
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ZERO             , D3D11_BLEND_SRC1_ALPHA }       , // 1202: (Cd -  0)*As +  0 ==> Cd*As
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ONE              , D3D11_BLEND_DEST_ALPHA }       , // 1210: (Cd -  0)*Ad + Cs ==> Cs + Cd*Ad
	{ 2, D3D11_BLEND_OP_ADD          , D3D11_BLEND_DEST_COLOR       , D3D11_BLEND_DEST_ALPHA }       , //#1211: (Cd -  0)*Ad + Cd ==> Cd*(1 + Ad)
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ZERO             , D3D11_BLEND_DEST_ALPHA }       , // 1212: (Cd -  0)*Ad +  0 ==> Cd*Ad
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ONE              , D3D11_BLEND_BLEND_FACTOR }     , // 1220: (Cd -  0)*F  + Cs ==> Cs + Cd*F
	{ 2, D3D11_BLEND_OP_ADD          , D3D11_BLEND_DEST_COLOR       , D3D11_BLEND_BLEND_FACTOR }     , //#1221: (Cd -  0)*F  + Cd ==> Cd*(1 + F)
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ZERO             , D3D11_BLEND_BLEND_FACTOR }     , // 1222: (Cd -  0)*F  +  0 ==> Cd*F
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_INV_SRC1_ALPHA   , D3D11_BLEND_ZERO }             , // 2000: (0  - Cs)*As + Cs ==> Cs*(1 - As)
	{ 0, D3D11_BLEND_OP_REV_SUBTRACT , D3D11_BLEND_SRC1_ALPHA       , D3D11_BLEND_ONE }              , // 2001: (0  - Cs)*As + Cd ==> Cd - Cs*As
	{ 0, D3D11_BLEND_OP_REV_SUBTRACT , D3D11_BLEND_SRC1_ALPHA       , D3D11_BLEND_ZERO }             , // 2002: (0  - Cs)*As +  0 ==> 0 - Cs*As
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_INV_DEST_ALPHA   , D3D11_BLEND_ZERO }             , // 2010: (0  - Cs)*Ad + Cs ==> Cs*(1 - Ad)
	{ 0, D3D11_BLEND_OP_REV_SUBTRACT , D3D11_BLEND_DEST_ALPHA       , D3D11_BLEND_ONE }              , // 2011: (0  - Cs)*Ad + Cd ==> Cd - Cs*Ad
	{ 0, D3D11_BLEND_OP_REV_SUBTRACT , D3D11_BLEND_DEST_ALPHA       , D3D11_BLEND_ZERO }             , // 2012: (0  - Cs)*Ad +  0 ==> 0 - Cs*Ad
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_INV_BLEND_FACTOR , D3D11_BLEND_ZERO }             , // 2020: (0  - Cs)*F  + Cs ==> Cs*(1 - F)
	{ 0, D3D11_BLEND_OP_REV_SUBTRACT , D3D11_BLEND_BLEND_FACTOR     , D3D11_BLEND_ONE }              , // 2021: (0  - Cs)*F  + Cd ==> Cd - Cs*F
	{ 0, D3D11_BLEND_OP_REV_SUBTRACT , D3D11_BLEND_BLEND_FACTOR     , D3D11_BLEND_ZERO }             , // 2022: (0  - Cs)*F  +  0 ==> 0 - Cs*F
	{ 0, D3D11_BLEND_OP_SUBTRACT     , D3D11_BLEND_ONE              , D3D11_BLEND_SRC1_ALPHA }       , // 2100: (0  - Cd)*As + Cs ==> Cs - Cd*As
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ZERO             , D3D11_BLEND_INV_SRC1_ALPHA }   , // 2101: (0  - Cd)*As + Cd ==> Cd*(1 - As)
	{ 0, D3D11_BLEND_OP_SUBTRACT     , D3D11_BLEND_ZERO             , D3D11_BLEND_SRC1_ALPHA }       , // 2102: (0  - Cd)*As +  0 ==> 0 - Cd*As
	{ 0, D3D11_BLEND_OP_SUBTRACT     , D3D11_BLEND_ONE              , D3D11_BLEND_DEST_ALPHA }       , // 2110: (0  - Cd)*Ad + Cs ==> Cs - Cd*Ad
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ZERO             , D3D11_BLEND_INV_DEST_ALPHA }   , // 2111: (0  - Cd)*Ad + Cd ==> Cd*(1 - Ad)
	{ 0, D3D11_BLEND_OP_SUBTRACT     , D3D11_BLEND_ONE              , D3D11_BLEND_DEST_ALPHA }       , // 2112: (0  - Cd)*Ad +  0 ==> 0 - Cd*Ad
	{ 0, D3D11_BLEND_OP_SUBTRACT     , D3D11_BLEND_ONE              , D3D11_BLEND_BLEND_FACTOR }     , // 2120: (0  - Cd)*F  + Cs ==> Cs - Cd*F
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ZERO             , D3D11_BLEND_INV_BLEND_FACTOR } , // 2121: (0  - Cd)*F  + Cd ==> Cd*(1 - F)
	{ 0, D3D11_BLEND_OP_SUBTRACT     , D3D11_BLEND_ONE              , D3D11_BLEND_BLEND_FACTOR }     , // 2122: (0  - Cd)*F  +  0 ==> 0 - Cd*F
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ONE              , D3D11_BLEND_ZERO }             , // 2200: (0  -  0)*As + Cs ==> Cs
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ZERO             , D3D11_BLEND_ONE }              , // 2201: (0  -  0)*As + Cd ==> Cd
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ZERO             , D3D11_BLEND_ZERO }             , // 2202: (0  -  0)*As +  0 ==> 0
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ONE              , D3D11_BLEND_ZERO }             , // 2210: (0  -  0)*Ad + Cs ==> Cs
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ZERO             , D3D11_BLEND_ONE }              , // 2211: (0  -  0)*Ad + Cd ==> Cd
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ZERO             , D3D11_BLEND_ZERO }             , // 2212: (0  -  0)*Ad +  0 ==> 0
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ONE              , D3D11_BLEND_ZERO }             , // 2220: (0  -  0)*F  + Cs ==> Cs
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ZERO             , D3D11_BLEND_ONE }              , // 2221: (0  -  0)*F  + Cd ==> Cd
	{ 0, D3D11_BLEND_OP_ADD          , D3D11_BLEND_ZERO             , D3D11_BLEND_ZERO }             , // 2222: (0  -  0)*F  +  0 ==> 0
};