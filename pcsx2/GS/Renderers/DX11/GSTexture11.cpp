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
#include "GSDevice11.h"
#include "GSTexture11.h"
#include "GS/GSPng.h"
#include "GS/GSPerfMon.h"

GSTexture11::GSTexture11(wil::com_ptr_nothrow<ID3D11Texture2D> texture, const D3D11_TEXTURE2D_DESC& desc,
	GSTexture::Type type, GSTexture::Format format)
	: m_texture(std::move(texture))
	, m_desc(desc)
	, m_mapped_subresource(0)
{
	m_size.x = static_cast<int>(desc.Width);
	m_size.y = static_cast<int>(desc.Height);
	m_type = type;
	m_format = format;
	m_mipmap_levels = static_cast<int>(desc.MipLevels);
}

void* GSTexture11::GetNativeHandle() const
{
	return static_cast<ID3D11ShaderResourceView*>(*const_cast<GSTexture11*>(this));
}

bool GSTexture11::Update(const GSVector4i& r, const void* data, int pitch, int layer)
{
	if (layer >= m_mipmap_levels)
		return false;

	g_perfmon.Put(GSPerfMon::TextureUploads, 1);

	const D3D11_BOX box = {(UINT)r.left, (UINT)r.top, 0U, (UINT)r.right, (UINT)r.bottom, 1U};
	const UINT subresource = layer; // MipSlice + (ArraySlice * MipLevels).

	GSDevice11::GetInstance()->GetD3DContext()->UpdateSubresource(m_texture.get(), subresource, &box, data, pitch, 0);
	m_needs_mipmaps_generated |= (layer == 0);
	return true;
}

bool GSTexture11::Map(GSMap& m, const GSVector4i* r, int layer)
{
	if (r != NULL)
	{
		// ASSERT(0); // not implemented
		return false;
	}

	if (layer >= m_mipmap_levels)
		return false;

	if (m_texture && m_desc.Usage == D3D11_USAGE_STAGING)
	{
		D3D11_MAPPED_SUBRESOURCE map;
		UINT subresource = layer;

		if (SUCCEEDED(GSDevice11::GetInstance()->GetD3DContext()->Map(m_texture.get(), subresource, D3D11_MAP_READ_WRITE, 0, &map)))
		{
			m.bits = (u8*)map.pData;
			m.pitch = (int)map.RowPitch;
			m_mapped_subresource = layer;
			return true;
		}
	}

	return false;
}

void GSTexture11::Unmap()
{
	const UINT subresource = m_mapped_subresource;
	m_needs_mipmaps_generated |= (m_mapped_subresource == 0);
	GSDevice11::GetInstance()->GetD3DContext()->Unmap(m_texture.get(), subresource);
}

bool GSTexture11::Save(const std::string& fn)
{
	D3D11_TEXTURE2D_DESC desc = m_desc;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	wil::com_ptr_nothrow<ID3D11Texture2D> res;
	HRESULT hr = GSDevice11::GetInstance()->GetD3DDevice()->CreateTexture2D(&desc, nullptr, res.put());
	if (FAILED(hr))
	{
		return false;
	}

	GSDevice11::GetInstance()->GetD3DContext()->CopyResource(res.get(), m_texture.get());

	if (m_desc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
	{
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;

		wil::com_ptr_nothrow<ID3D11Texture2D> dst;
		hr = GSDevice11::GetInstance()->GetD3DDevice()->CreateTexture2D(&desc, nullptr, dst.put());
		if (FAILED(hr))
		{
			return false;
		}

		D3D11_MAPPED_SUBRESOURCE sm, dm;

		hr = GSDevice11::GetInstance()->GetD3DContext()->Map(res.get(), 0, D3D11_MAP_READ, 0, &sm);
		if (FAILED(hr))
		{
			return false;
		}
		auto unmap_res = wil::scope_exit([res]{ // Capture by value to preserve the original pointer
			GSDevice11::GetInstance()->GetD3DContext()->Unmap(res.get(), 0);
		});

		hr = GSDevice11::GetInstance()->GetD3DContext()->Map(dst.get(), 0, D3D11_MAP_WRITE, 0, &dm);
		if (FAILED(hr))
		{
			return false;
		}
		auto unmap_dst = wil::scope_exit([dst]{ // Capture by value to preserve the original pointer
			GSDevice11::GetInstance()->GetD3DContext()->Unmap(dst.get(), 0);
		});

		const u8* s = static_cast<const u8*>(sm.pData);
		u8* d = static_cast<u8*>(dm.pData);

		for (u32 y = 0; y < desc.Height; y++, s += sm.RowPitch, d += dm.RowPitch)
		{
			for (u32 x = 0; x < desc.Width; x++)
			{
				reinterpret_cast<u32*>(d)[x] = static_cast<u32>(ldexpf(reinterpret_cast<const float*>(s)[x * 2], 32));
			}
		}

		res = std::move(dst);
	}

	res->GetDesc(&desc);

#ifdef PCSX2_DEVBUILD
	GSPng::Format format = GSPng::RGB_A_PNG;
#else
	GSPng::Format format = GSPng::RGB_PNG;
#endif
	switch (desc.Format)
	{
		case DXGI_FORMAT_A8_UNORM:
			format = GSPng::R8I_PNG;
			break;
		case DXGI_FORMAT_R8G8B8A8_UNORM:
			break;
		default:
			fprintf(stderr, "DXGI_FORMAT %d not saved to image\n", desc.Format);
			return false;
	}

	D3D11_MAPPED_SUBRESOURCE sm;
	hr = GSDevice11::GetInstance()->GetD3DContext()->Map(res.get(), 0, D3D11_MAP_READ, 0, &sm);
	if (FAILED(hr))
	{
		return false;
	}

	int compression = theApp.GetConfigI("png_compression_level");
	bool success = GSPng::Save(format, fn, static_cast<u8*>(sm.pData), desc.Width, desc.Height, sm.RowPitch, compression);

	GSDevice11::GetInstance()->GetD3DContext()->Unmap(res.get(), 0);

	return success;
}

void GSTexture11::GenerateMipmap()
{
	GSDevice11::GetInstance()->GetD3DContext()->GenerateMips(operator ID3D11ShaderResourceView*());
}

void GSTexture11::Swap(GSTexture* tex)
{
	GSTexture::Swap(tex);
	std::swap(m_texture, static_cast<GSTexture11*>(tex)->m_texture);
	std::swap(m_srv, static_cast<GSTexture11*>(tex)->m_srv);
	std::swap(m_rtv, static_cast<GSTexture11*>(tex)->m_rtv);
	std::swap(m_desc, static_cast<GSTexture11*>(tex)->m_desc);
	std::swap(m_mapped_subresource, static_cast<GSTexture11*>(tex)->m_mapped_subresource);
}

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

bool GSTexture11::Equal(GSTexture11* tex)
{
	return tex && m_texture == tex->m_texture;
}
