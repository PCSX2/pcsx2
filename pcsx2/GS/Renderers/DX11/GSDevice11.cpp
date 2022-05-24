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
#include "GS/GSPerfMon.h"
#include "GS/GSUtil.h"
#include "Host.h"
#include "HostDisplay.h"
#include "common/StringUtil.h"
#include <fstream>
#include <sstream>
#include <VersionHelpers.h>
#include <d3dcompiler.h>

static bool SupportsTextureFormat(ID3D11Device* dev, DXGI_FORMAT format)
{
	UINT support;
	if (FAILED(dev->CheckFormatSupport(format, &support)))
		return false;

	return (support & D3D11_FORMAT_SUPPORT_TEXTURE2D) != 0;
}

GSDevice11::GSDevice11()
{
	memset(&m_state, 0, sizeof(m_state));

	m_state.topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	m_state.bf = -1;

	m_features.geometry_shader = true;
	m_features.image_load_store = false;
	m_features.texture_barrier = false;
	m_features.provoking_vertex_last = false;
	m_features.point_expand = false;
	m_features.line_expand = false;
	m_features.prefer_new_textures = false;
	m_features.dxt_textures = false;
	m_features.bptc_textures = false;
	m_features.framebuffer_fetch = false;
	m_features.dual_source_blend = true;
	m_features.stencil_buffer = true;
}

GSDevice11::~GSDevice11()
{
	if (m_state.rt_view)
		m_state.rt_view->Release();
	if (m_state.dsv)
		m_state.dsv->Release();
}

bool GSDevice11::Create(HostDisplay* display)
{
	if (!__super::Create(display))
		return false;

	D3D11_BUFFER_DESC bd;
	D3D11_SAMPLER_DESC sd;
	D3D11_DEPTH_STENCIL_DESC dsd;
	D3D11_RASTERIZER_DESC rd;
	D3D11_BLEND_DESC bsd;

	D3D_FEATURE_LEVEL level;

	if (display->GetRenderAPI() != HostDisplay::RenderAPI::D3D11)
	{
		fprintf(stderr, "Render API is incompatible with D3D11\n");
		return false;
	}

	m_dev = static_cast<ID3D11Device*>(display->GetRenderDevice());
	m_ctx = static_cast<ID3D11DeviceContext*>(display->GetRenderContext());
	level = m_dev->GetFeatureLevel();

	if (!GSConfig.DisableShaderCache)
	{
		if (!m_shader_cache.Open(EmuFolders::Cache, m_dev->GetFeatureLevel(), SHADER_VERSION, GSConfig.UseDebugDevice))
		{
			Console.Warning("Shader cache failed to open.");
		}
	}
	else
	{
		m_shader_cache.Open({}, m_dev->GetFeatureLevel(), SHADER_VERSION, GSConfig.UseDebugDevice);
		Console.WriteLn("Not using shader cache.");
	}

	// Set maximum texture size limit based on supported feature level.
	if (level >= D3D_FEATURE_LEVEL_11_0)
		m_d3d_texsize = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
	else
		m_d3d_texsize = D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION;

	{
		// HACK: check AMD
		// Broken point sampler should be enabled only on AMD.
		m_features.broken_point_sampler = (D3D::Vendor() == D3D::VendorID::AMD);
	}

	SetFeatures();

	std::optional<std::string> shader = Host::ReadResourceFileToString("shaders/dx11/tfx.fx");
	if (!shader.has_value())
		return false;
	m_tfx_source = std::move(*shader);

	// convert

	D3D11_INPUT_ELEMENT_DESC il_convert[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	ShaderMacro sm_model(m_shader_cache.GetFeatureLevel());

	shader = Host::ReadResourceFileToString("shaders/dx11/convert.fx");
	if (!shader.has_value())
		return false;
	if (!m_shader_cache.GetVertexShaderAndInputLayout(m_dev.get(), m_convert.vs.put(), m_convert.il.put(),
			il_convert, std::size(il_convert), *shader, sm_model.GetPtr(), "vs_main"))
	{
		return false;
	}

	ShaderMacro sm_convert(m_shader_cache.GetFeatureLevel());
	sm_convert.AddMacro("PS_SCALE_FACTOR", GSConfig.UpscaleMultiplier);

	D3D_SHADER_MACRO* sm_convert_ptr = sm_convert.GetPtr();

	for (size_t i = 0; i < std::size(m_convert.ps); i++)
	{
		m_convert.ps[i] = m_shader_cache.GetPixelShader(m_dev.get(), *shader, sm_convert_ptr, shaderName(static_cast<ShaderConvert>(i)));
		if (!m_convert.ps[i])
			return false;
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

	shader = Host::ReadResourceFileToString("shaders/dx11/merge.fx");
	if (!shader.has_value())
		return false;

	for (size_t i = 0; i < std::size(m_merge.ps); i++)
	{
		const std::string entry_point(StringUtil::StdStringFromFormat("ps_main%d", i));
		m_merge.ps[i] = m_shader_cache.GetPixelShader(m_dev.get(), *shader, sm_model.GetPtr(), entry_point.c_str());
		if (!m_merge.ps[i])
			return false;
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

	shader = Host::ReadResourceFileToString("shaders/dx11/interlace.fx");
	if (!shader.has_value())
		return false;
	for (size_t i = 0; i < std::size(m_interlace.ps); i++)
	{
		const std::string entry_point(StringUtil::StdStringFromFormat("ps_main%d", i));
		m_interlace.ps[i] = m_shader_cache.GetPixelShader(m_dev.get(), *shader, sm_model.GetPtr(), entry_point.c_str());
		if (!m_interlace.ps[i])
			return false;
	}

	// Shade Boost

	memset(&bd, 0, sizeof(bd));
	bd.ByteWidth = sizeof(float) * 4;
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	m_dev->CreateBuffer(&bd, nullptr, m_shadeboost.cb.put());

	shader = Host::ReadResourceFileToString("shaders/dx11/shadeboost.fx");
	if (!shader.has_value())
		return false;
	m_shadeboost.ps = m_shader_cache.GetPixelShader(m_dev.get(), *shader, sm_model.GetPtr(), "ps_main");
	if (!m_shadeboost.ps)
		return false;

	// External fx shader

	memset(&bd, 0, sizeof(bd));

	bd.ByteWidth = sizeof(ExternalFXConstantBuffer);
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	m_dev->CreateBuffer(&bd, nullptr, m_shaderfx.cb.put());

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

	m_dev->CreateRasterizerState(&rd, m_rs.put());
	m_ctx->RSSetState(m_rs.get());

	//

	memset(&sd, 0, sizeof(sd));

	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.MinLOD = -FLT_MAX;
	sd.MaxLOD = FLT_MAX;
	sd.MaxAnisotropy = 1;
	sd.ComparisonFunc = D3D11_COMPARISON_NEVER;

	m_dev->CreateSamplerState(&sd, m_convert.ln.put());

	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;

	m_dev->CreateSamplerState(&sd, m_convert.pt.put());

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

	return true;
}

void GSDevice11::SetFeatures()
{
	// Check all three formats, since the feature means any can be used.
	m_features.dxt_textures = SupportsTextureFormat(m_dev.get(), DXGI_FORMAT_BC1_UNORM) &&
							  SupportsTextureFormat(m_dev.get(), DXGI_FORMAT_BC2_UNORM) &&
							  SupportsTextureFormat(m_dev.get(), DXGI_FORMAT_BC3_UNORM);

	m_features.bptc_textures = SupportsTextureFormat(m_dev.get(), DXGI_FORMAT_BC7_UNORM);
}

void GSDevice11::ResetAPIState()
{
	// Clear out the GS, since the imgui draw doesn't get rid of it.
	m_ctx->GSSetShader(nullptr, nullptr, 0);
}

void GSDevice11::RestoreAPIState()
{
	const UINT vb_stride = static_cast<UINT>(m_state.vb_stride);
	const UINT vb_offset = 0;
	m_ctx->IASetVertexBuffers(0, 1, &m_state.vb, &vb_stride, &vb_offset);
	m_ctx->IASetIndexBuffer(m_state.ib, DXGI_FORMAT_R32_UINT, 0);
	m_ctx->IASetInputLayout(m_state.layout);
	m_ctx->IASetPrimitiveTopology(m_state.topology);
	m_ctx->VSSetShader(m_state.vs, nullptr, 0);
	m_ctx->VSSetConstantBuffers(0, 1, &m_state.vs_cb);
	m_ctx->GSSetShader(m_state.gs, nullptr, 0);
	m_ctx->GSSetConstantBuffers(0, 1, &m_state.gs_cb);
	m_ctx->PSSetShader(m_state.ps, nullptr, 0);
	m_ctx->PSSetConstantBuffers(0, 1, &m_state.ps_cb);

	const CD3D11_VIEWPORT vp(0.0f, 0.0f,
		static_cast<float>(m_state.viewport.x), static_cast<float>(m_state.viewport.y),
		0.0f, 1.0f);
	m_ctx->RSSetViewports(1, &vp);
	m_ctx->RSSetScissorRects(1, reinterpret_cast<const D3D11_RECT*>(&m_state.scissor));
	m_ctx->RSSetState(m_rs.get());

	m_ctx->OMSetDepthStencilState(m_state.dss, m_state.sref);

	const float blend_factors[4] = { m_state.bf, m_state.bf, m_state.bf, m_state.bf };
	m_ctx->OMSetBlendState(m_state.bs, blend_factors, 0xFFFFFFFFu);

	PSUpdateShaderState();

	if (m_state.rt_view)
		m_ctx->OMSetRenderTargets(1, &m_state.rt_view, m_state.dsv);
	else
		m_ctx->OMSetRenderTargets(0, nullptr, m_state.dsv);
}

void GSDevice11::DrawPrimitive()
{
	g_perfmon.Put(GSPerfMon::DrawCalls, 1);
	PSUpdateShaderState();
	m_ctx->Draw(m_vertex.count, m_vertex.start);
}

void GSDevice11::DrawIndexedPrimitive()
{
	g_perfmon.Put(GSPerfMon::DrawCalls, 1);
	PSUpdateShaderState();
	m_ctx->DrawIndexed(m_index.count, m_index.start, m_vertex.start);
}

void GSDevice11::DrawIndexedPrimitive(int offset, int count)
{
	ASSERT(offset + count <= (int)m_index.count);
	g_perfmon.Put(GSPerfMon::DrawCalls, 1);
	PSUpdateShaderState();
	m_ctx->DrawIndexed(count, m_index.start + offset, m_vertex.start);
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

GSTexture* GSDevice11::CreateSurface(GSTexture::Type type, int width, int height, int levels, GSTexture::Format format)
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
		case GSTexture::Format::PrimID:       dxformat = DXGI_FORMAT_R32_SINT;           break;
		case GSTexture::Format::BC1:          dxformat = DXGI_FORMAT_BC1_UNORM;          break;
		case GSTexture::Format::BC2:          dxformat = DXGI_FORMAT_BC2_UNORM;          break;
		case GSTexture::Format::BC3:          dxformat = DXGI_FORMAT_BC3_UNORM;          break;
		case GSTexture::Format::BC7:          dxformat = DXGI_FORMAT_BC7_UNORM;          break;
		case GSTexture::Format::Invalid:
			ASSERT(0);
			dxformat = DXGI_FORMAT_UNKNOWN;
	}

	// Texture limit for D3D10/11 min 1, max 8192 D3D10, max 16384 D3D11.
	desc.Width = std::clamp(width, 1, m_d3d_texsize);
	desc.Height = std::clamp(height, 1, m_d3d_texsize);
	desc.Format = dxformat;
	desc.MipLevels = levels;
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;

	switch (type)
	{
		case GSTexture::Type::RenderTarget:
			desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			break;
		case GSTexture::Type::DepthStencil:
			desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
			break;
		case GSTexture::Type::Texture:
			desc.BindFlags = (levels > 1 && !GSTexture::IsCompressedFormat(format)) ? (D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE) : D3D11_BIND_SHADER_RESOURCE;
			desc.MiscFlags = (levels > 1 && !GSTexture::IsCompressedFormat(format)) ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;
			break;
		case GSTexture::Type::Offscreen:
			desc.Usage = D3D11_USAGE_STAGING;
			desc.CPUAccessFlags |= D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
			break;
	}

	GSTexture11* t = nullptr;

	wil::com_ptr_nothrow<ID3D11Texture2D> texture;
	HRESULT hr = m_dev->CreateTexture2D(&desc, nullptr, texture.put());

	if (SUCCEEDED(hr))
	{
		t = new GSTexture11(std::move(texture), desc, type, format);
		assert(type == t->GetType());
	}
	else
	{
		throw std::bad_alloc();
	}

	return t;
}

bool GSDevice11::DownloadTexture(GSTexture* src, const GSVector4i& rect, GSTexture::GSMap& out_map)
{
	ASSERT(src);
	ASSERT(!m_download_tex);
	g_perfmon.Put(GSPerfMon::Readbacks, 1);

	m_download_tex.reset(static_cast<GSTexture11*>(CreateOffscreen(rect.width(), rect.height(), src->GetFormat())));
	if (!m_download_tex)
		return false;
	CopyRect(src, m_download_tex.get(), rect, 0, 0);
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

void GSDevice11::CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r, u32 destX, u32 destY)
{
	g_perfmon.Put(GSPerfMon::TextureCopies, 1);

	D3D11_BOX box = {(UINT)r.left, (UINT)r.top, 0U, (UINT)r.right, (UINT)r.bottom, 1U};

	// DX api isn't happy if we pass a box for depth copy
	// It complains that depth/multisample must be a full copy
	// and asks us to use a NULL for the box
	const bool depth = (sTex->GetType() == GSTexture::Type::DepthStencil);
	auto pBox = depth ? nullptr : &box;

	m_ctx->CopySubresourceRegion(*(GSTexture11*)dTex, 0, destX, destY, 0, *(GSTexture11*)sTex, 0, pBox);
}

void GSDevice11::CloneTexture(GSTexture* src, GSTexture** dest, const GSVector4i& rect)
{
	pxAssertMsg(src->GetType() == GSTexture::Type::DepthStencil || src->GetType() == GSTexture::Type::RenderTarget, "Source is RT or DS.");

	const int w = src->GetWidth();
	const int h = src->GetHeight();

	if (src->GetType() == GSTexture::Type::DepthStencil)
	{
		// DX11 requires that you copy the entire depth buffer.
		*dest = CreateDepthStencil(w, h, src->GetFormat(), false);
		CopyRect(src, *dest, GSVector4i(0, 0, w, h), 0, 0);
	}
	else
	{
		*dest = CreateRenderTarget(w, h, src->GetFormat(), false);
		CopyRect(src, *dest, rect, rect.left, rect.top);
	}
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
	ASSERT(sTex);

	const bool draw_in_depth = ps == m_convert.ps[static_cast<int>(ShaderConvert::DEPTH_COPY)]
	                        || ps == m_convert.ps[static_cast<int>(ShaderConvert::RGBA8_TO_FLOAT32)]
	                        || ps == m_convert.ps[static_cast<int>(ShaderConvert::RGBA8_TO_FLOAT24)]
	                        || ps == m_convert.ps[static_cast<int>(ShaderConvert::RGBA8_TO_FLOAT16)]
	                        || ps == m_convert.ps[static_cast<int>(ShaderConvert::RGB5A1_TO_FLOAT16)];

	BeginScene();

	GSVector2i ds;
	if (dTex)
	{
		ds = dTex->GetSize();
		if (draw_in_depth)
			OMSetRenderTargets(nullptr, dTex);
		else
			OMSetRenderTargets(dTex, nullptr);
	}
	else
	{
		ds = GSVector2i(m_display->GetWindowWidth(), m_display->GetWindowHeight());

	}

	// om
	if (draw_in_depth)
		OMSetDepthStencilState(m_convert.dss_write.get(), 0);
	else
		OMSetDepthStencilState(m_convert.dss.get(), 0);

	OMSetBlendState(bs, 0);



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

void GSDevice11::DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect, const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, const GSVector4& c)
{
	const GSVector4 full_r(0.0f, 0.0f, 1.0f, 1.0f);
	const bool feedback_write_2 = PMODE.EN2 && sTex[2] != nullptr && EXTBUF.FBIN == 1;
	const bool feedback_write_1 = PMODE.EN1 && sTex[2] != nullptr && EXTBUF.FBIN == 0;
	const bool feedback_write_2_but_blend_bg = feedback_write_2 && PMODE.SLBG == 1;

	// Merge the 2 source textures (sTex[0],sTex[1]). Final results go to dTex. Feedback write will go to sTex[2].
	// If either 2nd output is disabled or SLBG is 1, a background color will be used.
	// Note: background color is also used when outside of the unit rectangle area
	ClearRenderTarget(dTex, c);

	// Upload constant to select YUV algo, but skip constant buffer update if we don't need it
	if (feedback_write_2 || feedback_write_1 || sTex[0])
	{
		const MergeConstantBuffer cb = {c, EXTBUF.EMODA, EXTBUF.EMODC};
		m_ctx->UpdateSubresource(m_merge.cb.get(), 0, nullptr, &cb, 0, 0);
	}

	if (sTex[1] && (PMODE.SLBG == 0 || feedback_write_2_but_blend_bg))
	{
		// 2nd output is enabled and selected. Copy it to destination so we can blend it with 1st output
		// Note: value outside of dRect must contains the background color (c)
		StretchRect(sTex[1], sRect[1], dTex, PMODE.SLBG ? dRect[2] : dRect[1], ShaderConvert::COPY);
	}

	// Save 2nd output
	if (feedback_write_2)
	{
		StretchRect(dTex, full_r, sTex[2], dRect[2], m_convert.ps[static_cast<int>(ShaderConvert::YUV)].get(),
			m_merge.cb.get(), nullptr, true);
	}

	// Restore background color to process the normal merge
	if (feedback_write_2_but_blend_bg)
		ClearRenderTarget(dTex, c);

	if (sTex[0])
	{
		// 1st output is enabled. It must be blended
		StretchRect(sTex[0], sRect[0], dTex, dRect[0], m_merge.ps[PMODE.MMOD].get(), m_merge.cb.get(), m_merge.bs.get(), true);
	}

	if (feedback_write_1)
	{
		StretchRect(sTex[0], full_r, sTex[2], dRect[2], m_convert.ps[static_cast<int>(ShaderConvert::YUV)].get(),
			m_merge.cb.get(), nullptr, true);
	}
}

void GSDevice11::DoInterlace(GSTexture* sTex, GSTexture* dTex, int shader, bool linear, float yoffset)
{
	const GSVector4 s = GSVector4(dTex->GetSize());

	const GSVector4 sRect(0, 0, 1, 1);
	const GSVector4 dRect(0.0f, yoffset, s.x, s.y + yoffset);

	InterlaceConstantBuffer cb;

	cb.ZrH = GSVector2(0, 1.0f / s.y);

	m_ctx->UpdateSubresource(m_interlace.cb.get(), 0, nullptr, &cb, 0, 0);

	StretchRect(sTex, sRect, dTex, dRect, m_interlace.ps[shader].get(), m_interlace.cb.get(), linear);
}

void GSDevice11::DoExternalFX(GSTexture* sTex, GSTexture* dTex)
{
	const GSVector2i s = dTex->GetSize();

	const GSVector4 sRect(0, 0, 1, 1);
	const GSVector4 dRect(0, 0, s.x, s.y);

	ExternalFXConstantBuffer cb;

	if (!m_shaderfx.ps)
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
		ShaderMacro sm(m_shader_cache.GetFeatureLevel());
		m_shaderfx.ps = m_shader_cache.GetPixelShader(m_dev.get(), shader.str(), sm.GetPtr(), "ps_main");
		if (!m_shaderfx.ps)
		{
			printf("GS: Failed to compile external post-processing shader.\n");
			return;
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

	if (!m_fxaa_ps)
	{
		std::optional<std::string> shader = Host::ReadResourceFileToString("shaders/common/fxaa.fx");
		if (!shader.has_value())
		{
			Console.Error("FXAA shader is missing");
			return;
		}
		
		ShaderMacro sm(m_shader_cache.GetFeatureLevel());
		m_fxaa_ps = m_shader_cache.GetPixelShader(m_dev.get(), *shader, sm.GetPtr(), "ps_main");
		if (!m_fxaa_ps)
			return;
	}

	StretchRect(sTex, sRect, dTex, dRect, m_fxaa_ps.get(), nullptr, true);

	//sTex->Save("c:\\temp1\\1.bmp");
	//dTex->Save("c:\\temp1\\2.bmp");
}

void GSDevice11::DoShadeBoost(GSTexture* sTex, GSTexture* dTex, const float params[4])
{
	const GSVector2i s = dTex->GetSize();

	const GSVector4 sRect(0, 0, 1, 1);
	const GSVector4 dRect(0, 0, s.x, s.y);

	m_ctx->UpdateSubresource(m_shadeboost.cb.get(), 0, nullptr, params, 0, 0);

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
	PSSetShaderResource(2, nullptr);
}

void GSDevice11::PSSetShaderResource(int i, GSTexture* sr)
{
	m_state.ps_sr_views[i] = sr ? static_cast<ID3D11ShaderResourceView*>(*static_cast<GSTexture11*>(sr)) : nullptr;
}

void GSDevice11::PSSetSamplerState(ID3D11SamplerState* ss0, ID3D11SamplerState* ss1)
{
	m_state.ps_ss[0] = ss0;
	m_state.ps_ss[1] = ss1;
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
	m_ctx->PSSetSamplers(0, m_state.ps_ss.size(), m_state.ps_ss.data());
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

	const bool changed = (m_state.rt_view != rtv || m_state.dsv != dsv);
	if (m_state.rt_view != rtv)
	{
		if (m_state.rt_view)
			m_state.rt_view->Release();
		if (rtv)
			rtv->AddRef();
		m_state.rt_view = rtv;
	}
	if (m_state.dsv != dsv)
	{
		if (m_state.dsv)
			m_state.dsv->Release();
		if (dsv)
			dsv->AddRef();
		m_state.dsv = dsv;
	}
	if (changed)
		m_ctx->OMSetRenderTargets(1, &rtv, dsv);

	const GSVector2i size = rt ? rt->GetSize() : ds->GetSize();
	if (m_state.viewport != size)
	{
		m_state.viewport = size;

		D3D11_VIEWPORT vp;
		memset(&vp, 0, sizeof(vp));

		vp.TopLeftX = 0.0f;
		vp.TopLeftY = 0.0f;
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

		m_ctx->RSSetScissorRects(1, reinterpret_cast<const D3D11_RECT*>(&r));
	}
}

GSDevice11::ShaderMacro::ShaderMacro(D3D_FEATURE_LEVEL fl)
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

static GSDevice11::OMBlendSelector convertSel(GSHWDrawConfig::ColorMaskSelector cm, GSHWDrawConfig::BlendState blend)
{
	GSDevice11::OMBlendSelector out;
	out.wrgba = cm.wrgba;
	if (blend.enable)
	{
		out.blend_enable = true;
		out.blend_src_factor = blend.src_factor;
		out.blend_dst_factor = blend.dst_factor;
		out.blend_op = blend.op;
	}

	return out;
}

/// Checks that we weren't sent things we declared we don't support
/// Clears things we don't support that can be quietly disabled
static void preprocessSel(GSDevice11::PSSelector& sel)
{
	ASSERT(sel.date      == 0); // In-shader destination alpha not supported and shouldn't be sent
	ASSERT(sel.write_rg  == 0); // Not supported, shouldn't be sent
}

void GSDevice11::RenderHW(GSHWDrawConfig& config)
{
	ASSERT(!config.require_full_barrier); // We always specify no support so it shouldn't request this
	preprocessSel(config.ps);

	if (config.destination_alpha != GSHWDrawConfig::DestinationAlphaMode::Off)
	{
		const GSVector4 src = GSVector4(config.drawarea) / GSVector4(config.ds->GetSize()).xyxy();
		const GSVector4 dst = src * 2.0f - 1.0f;

		GSVertexPT1 vertices[] =
		{
			{GSVector4(dst.x, -dst.y, 0.5f, 1.0f), GSVector2(src.x, src.y)},
			{GSVector4(dst.z, -dst.y, 0.5f, 1.0f), GSVector2(src.z, src.y)},
			{GSVector4(dst.x, -dst.w, 0.5f, 1.0f), GSVector2(src.x, src.w)},
			{GSVector4(dst.z, -dst.w, 0.5f, 1.0f), GSVector2(src.z, src.w)},
		};

		SetupDATE(config.rt, config.ds, vertices, config.datm);
	}

	GSTexture* hdr_rt = nullptr;
	if (config.ps.hdr)
	{
		const GSVector2i size = config.rt->GetSize();
		const GSVector4 dRect(config.drawarea);
		const GSVector4 sRect = dRect / GSVector4(size.x, size.y).xyxy();
		hdr_rt = CreateRenderTarget(size.x, size.y, GSTexture::Format::FloatColor);
		hdr_rt->CommitRegion(GSVector2i(config.drawarea.z, config.drawarea.w));
		// Warning: StretchRect must be called before BeginScene otherwise
		// vertices will be overwritten. Trust me you don't want to do that.
		StretchRect(config.rt, sRect, hdr_rt, dRect, ShaderConvert::COPY, false);
	}

	BeginScene();

	void* ptr = nullptr;
	if (IAMapVertexBuffer(&ptr, sizeof(*config.verts), config.nverts))
	{
		GSVector4i::storent(ptr, config.verts, config.nverts * sizeof(*config.verts));
		IAUnmapVertexBuffer();
	}
	IASetIndexBuffer(config.indices, config.nindices);
	D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	switch (config.topology)
	{
		case GSHWDrawConfig::Topology::Point:    topology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;    break;
		case GSHWDrawConfig::Topology::Line:     topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;     break;
		case GSHWDrawConfig::Topology::Triangle: topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
	}
	IASetPrimitiveTopology(topology);

	OMSetRenderTargets(hdr_rt ? hdr_rt : config.rt, config.ds, &config.scissor);

	PSSetShaderResources(config.tex, config.pal);

	GSTexture* rt_copy = nullptr;
	GSTexture* ds_copy = nullptr;
	if (config.require_one_barrier || (config.tex && config.tex == config.rt)) // Used as "bind rt" flag when texture barrier is unsupported
	{
		// Bind the RT.This way special effect can use it.
		// Do not always bind the rt when it's not needed,
		// only bind it when effects use it such as fbmask emulation currently
		// because we copy the frame buffer and it is quite slow.
		CloneTexture(config.rt, &rt_copy, config.drawarea);
		if (rt_copy)
		{
			if (config.require_one_barrier)
				PSSetShaderResource(2, rt_copy);
			if (config.tex && config.tex == config.rt)
				PSSetShaderResource(0, rt_copy);
		}
	}

	if (config.tex && config.tex == config.ds)
	{
		// mainly for ico (depth buffer used as texture)
		// binding to 0 here is safe, because config.tex can't equal both tex and rt
		CloneTexture(config.ds, &ds_copy, config.drawarea);
		if (ds_copy)
			PSSetShaderResource(0, ds_copy);
	}

	SetupOM(config.depth, convertSel(config.colormask, config.blend), config.blend.constant);
	SetupVS(config.vs, &config.cb_vs);
	SetupGS(config.gs);
	SetupPS(config.ps, &config.cb_ps, config.sampler);

	DrawIndexedPrimitive();

	if (config.separate_alpha_pass)
	{
		GSHWDrawConfig::BlendState sap_blend = {};
		SetHWDrawConfigForAlphaPass(&config.ps, &config.colormask, &sap_blend, &config.depth);
		SetupOM(config.depth, convertSel(config.colormask, sap_blend), config.blend.constant);
		SetupPS(config.ps, &config.cb_ps, config.sampler);

		DrawIndexedPrimitive();
	}

	if (config.alpha_second_pass.enable)
	{
		preprocessSel(config.alpha_second_pass.ps);
		if (config.cb_ps.FogColor_AREF.a != config.alpha_second_pass.ps_aref)
		{
			config.cb_ps.FogColor_AREF.a = config.alpha_second_pass.ps_aref;
			SetupPS(config.alpha_second_pass.ps, &config.cb_ps, config.sampler);
		}
		else
		{
			// ps cbuffer hasn't changed, so don't bother checking
			SetupPS(config.alpha_second_pass.ps, nullptr, config.sampler);
		}

		SetupOM(config.alpha_second_pass.depth, convertSel(config.alpha_second_pass.colormask, config.blend), config.blend.constant);

		DrawIndexedPrimitive();

		if (config.second_separate_alpha_pass)
		{
			GSHWDrawConfig::BlendState sap_blend = {};
			SetHWDrawConfigForAlphaPass(&config.alpha_second_pass.ps, &config.alpha_second_pass.colormask, &sap_blend, &config.alpha_second_pass.depth);
			SetupOM(config.alpha_second_pass.depth, convertSel(config.alpha_second_pass.colormask, sap_blend), config.blend.constant);
			SetupPS(config.alpha_second_pass.ps, &config.cb_ps, config.sampler);

			DrawIndexedPrimitive();
		}
	}

	EndScene();

	if (rt_copy)
		Recycle(rt_copy);
	if (ds_copy)
		Recycle(ds_copy);

	if (hdr_rt)
	{
		const GSVector2i size = config.rt->GetSize();
		const GSVector4 dRect(config.drawarea);
		const GSVector4 sRect = dRect / GSVector4(size.x, size.y).xyxy();
		StretchRect(hdr_rt, sRect, config.rt, dRect, ShaderConvert::MOD_256, false);
		Recycle(hdr_rt);
	}
}
