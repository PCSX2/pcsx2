// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "GSDevice11.h"
#include "GSTexture11.h"
#include "GS/GSPng.h"
#include "GS/GSPerfMon.h"

#include "common/Console.h"
#include "common/BitUtils.h"

#include "fmt/format.h"

GSTexture11::GSTexture11(wil::com_ptr_nothrow<ID3D11Texture2D> texture, const D3D11_TEXTURE2D_DESC& desc,
	GSTexture::Type type, GSTexture::Format format)
	: m_texture(std::move(texture))
	, m_desc(desc)
{
	m_size.x = static_cast<int>(desc.Width);
	m_size.y = static_cast<int>(desc.Height);
	m_type = type;
	m_format = format;
	m_mipmap_levels = static_cast<int>(desc.MipLevels);
}

DXGI_FORMAT GSTexture11::GetDXGIFormat(Format format)
{
	// clang-format off
	switch (format)
	{
	case GSTexture::Format::Color:        return DXGI_FORMAT_R8G8B8A8_UNORM;
	case GSTexture::Format::HDRColor:     return DXGI_FORMAT_R16G16B16A16_UNORM;
	case GSTexture::Format::DepthStencil: return DXGI_FORMAT_R32G8X24_TYPELESS;
	case GSTexture::Format::UNorm8:       return DXGI_FORMAT_A8_UNORM;
	case GSTexture::Format::UInt16:       return DXGI_FORMAT_R16_UINT;
	case GSTexture::Format::UInt32:       return DXGI_FORMAT_R32_UINT;
	case GSTexture::Format::PrimID:       return DXGI_FORMAT_R32_FLOAT;
	case GSTexture::Format::BC1:          return DXGI_FORMAT_BC1_UNORM;
	case GSTexture::Format::BC2:          return DXGI_FORMAT_BC2_UNORM;
	case GSTexture::Format::BC3:          return DXGI_FORMAT_BC3_UNORM;
	case GSTexture::Format::BC7:          return DXGI_FORMAT_BC7_UNORM;
	case GSTexture::Format::Invalid:
	default:
		pxAssert(0);
		return DXGI_FORMAT_UNKNOWN;
	}
	// clang-format on
}

void* GSTexture11::GetNativeHandle() const
{
	return static_cast<ID3D11ShaderResourceView*>(*const_cast<GSTexture11*>(this));
}

bool GSTexture11::Update(const GSVector4i& r, const void* data, int pitch, int layer)
{
	if (layer >= m_mipmap_levels)
		return false;

	GSDevice11::GetInstance()->CommitClear(this);
	g_perfmon.Put(GSPerfMon::TextureUploads, 1);

	const u32 bs = GetCompressedBlockSize();
	
	const D3D11_BOX box = {Common::AlignDownPow2((u32)r.left, bs), Common::AlignDownPow2((u32)r.top, bs), 0U,
		Common::AlignUpPow2((u32)r.right, bs), Common::AlignUpPow2((u32)r.bottom, bs), 1U};
	const UINT subresource = layer; // MipSlice + (ArraySlice * MipLevels).

	GSDevice11::GetInstance()->GetD3DContext()->UpdateSubresource(m_texture.get(), subresource, &box, data, pitch, 0);
	m_needs_mipmaps_generated |= (layer == 0);
	return true;
}

bool GSTexture11::Map(GSMap& m, const GSVector4i* r, int layer)
{
	// Not supported
	return false;
}

void GSTexture11::Unmap()
{
	pxFailRel("Should not be called.");
}

void GSTexture11::GenerateMipmap()
{
	GSDevice11::GetInstance()->GetD3DContext()->GenerateMips(operator ID3D11ShaderResourceView*());
}

#ifdef PCSX2_DEVBUILD

void GSTexture11::SetDebugName(std::string_view name)
{
	if (name.empty())
		return;

	GSDevice11::SetD3DDebugObjectName(m_texture.get(), name);
	if (m_srv)
		GSDevice11::SetD3DDebugObjectName(m_srv.get(), fmt::format("{} SRV", name));
	if (m_rtv)
		GSDevice11::SetD3DDebugObjectName(m_rtv.get(), fmt::format("{} RTV", name));
}

#endif

GSTexture11::operator ID3D11Texture2D*()
{
	return m_texture.get();
}

GSTexture11::operator ID3D11ShaderResourceView*()
{
	if (!m_srv)
	{
		if (m_desc.Format == DXGI_FORMAT_R32G8X24_TYPELESS)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};

			srvd.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
			srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvd.Texture2D.MipLevels = 1;

			GSDevice11::GetInstance()->GetD3DDevice()->CreateShaderResourceView(m_texture.get(), &srvd, m_srv.put());
		}
		else
		{
			GSDevice11::GetInstance()->GetD3DDevice()->CreateShaderResourceView(m_texture.get(), nullptr, m_srv.put());
		}
	}

	return m_srv.get();
}

GSTexture11::operator ID3D11RenderTargetView*()
{
	if (!m_rtv)
	{
		GSDevice11::GetInstance()->GetD3DDevice()->CreateRenderTargetView(m_texture.get(), nullptr, m_rtv.put());
	}

	return m_rtv.get();
}

GSTexture11::operator ID3D11DepthStencilView*()
{
	if (!m_dsv)
	{
		if (m_desc.Format == DXGI_FORMAT_R32G8X24_TYPELESS)
		{
			D3D11_DEPTH_STENCIL_VIEW_DESC dsvd = {};

			dsvd.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
			dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

			GSDevice11::GetInstance()->GetD3DDevice()->CreateDepthStencilView(m_texture.get(), &dsvd, m_dsv.put());
		}
		else
		{
			GSDevice11::GetInstance()->GetD3DDevice()->CreateDepthStencilView(m_texture.get(), nullptr, m_dsv.put());
		}
	}

	return m_dsv.get();
}

GSTexture11::operator ID3D11UnorderedAccessView*()
{
	if (!m_uav)
		GSDevice11::GetInstance()->GetD3DDevice()->CreateUnorderedAccessView(m_texture.get(), nullptr, m_uav.put());

	return m_uav.get();
}

GSDownloadTexture11::GSDownloadTexture11(wil::com_ptr_nothrow<ID3D11Texture2D> tex, u32 width, u32 height, GSTexture::Format format)
	: GSDownloadTexture(width, height, format)
	, m_texture(std::move(tex))
{
}

GSDownloadTexture11::~GSDownloadTexture11()
{
	if (IsMapped())
		GSDownloadTexture11::Unmap();
}

std::unique_ptr<GSDownloadTexture11> GSDownloadTexture11::Create(u32 width, u32 height, GSTexture::Format format)
{
	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.Format = GSTexture11::GetDXGIFormat(format);
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	wil::com_ptr_nothrow<ID3D11Texture2D> tex;
	HRESULT hr = GSDevice11::GetInstance()->GetD3DDevice()->CreateTexture2D(&desc, nullptr, tex.put());
	if (FAILED(hr))
	{
		Console.Error("GSDownloadTexture11: CreateTexture2D() failed: %08X", hr);
		return {};
	}

	return std::unique_ptr<GSDownloadTexture11>(new GSDownloadTexture11(std::move(tex), width, height, format));
}

void GSDownloadTexture11::CopyFromTexture(
	const GSVector4i& drc, GSTexture* stex, const GSVector4i& src, u32 src_level, bool use_transfer_pitch)
{
	pxAssert(stex->GetFormat() == m_format);
	pxAssert(drc.width() == src.width() && drc.height() == src.height());
	pxAssert(src.z <= stex->GetWidth() && src.w <= stex->GetHeight());
	pxAssert(static_cast<u32>(drc.z) <= m_width && static_cast<u32>(drc.w) <= m_height);
	pxAssert(src_level < static_cast<u32>(stex->GetMipmapLevels()));

	GSDevice11::GetInstance()->CommitClear(stex);

	g_perfmon.Put(GSPerfMon::Readbacks, 1);

	if (IsMapped())
		Unmap();

	// depth textures need to copy the whole thing..
	if (m_format == GSTexture::Format::DepthStencil)
	{
		GSDevice11::GetInstance()->GetD3DContext()->CopySubresourceRegion(
			m_texture.get(), 0, 0, 0, 0, *static_cast<GSTexture11*>(stex), src_level, nullptr);
	}
	else
	{
		const CD3D11_BOX sbox(src.left, src.top, 0, src.right, src.bottom, 1);
		GSDevice11::GetInstance()->GetD3DContext()->CopySubresourceRegion(
			m_texture.get(), 0, drc.x, drc.y, 0, *static_cast<GSTexture11*>(stex), src_level, &sbox);
	}

	m_needs_flush = true;
}

bool GSDownloadTexture11::Map(const GSVector4i& rc)
{
	if (IsMapped())
		return true;

	D3D11_MAPPED_SUBRESOURCE sr;
	HRESULT hr = GSDevice11::GetInstance()->GetD3DContext()->Map(m_texture.get(), 0, D3D11_MAP_READ, 0, &sr);
	if (FAILED(hr))
	{
		Console.Error("GSDownloadTexture11: Map() failed: %08X", hr);
		return false;
	}

	m_map_pointer = static_cast<u8*>(sr.pData);
	m_current_pitch = sr.RowPitch;
	return true;
}

void GSDownloadTexture11::Unmap()
{
	if (!IsMapped())
		return;

	GSDevice11::GetInstance()->GetD3DContext()->Unmap(m_texture.get(), 0);
	m_map_pointer = nullptr;
}

void GSDownloadTexture11::Flush()
{
	if (!m_needs_flush)
		return;

	if (IsMapped())
		Unmap();

	// Handled when mapped.
}

#ifdef PCSX2_DEVBUILD

void GSDownloadTexture11::SetDebugName(std::string_view name)
{
	if (name.empty())
		return;

	GSDevice11::SetD3DDebugObjectName(m_texture.get(), name);
}

#endif
