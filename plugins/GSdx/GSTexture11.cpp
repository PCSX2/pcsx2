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
#include "GSTexture11.h"
#include "GSPng.h"

GSTexture11::GSTexture11(ID3D11Texture2D* texture)
	: m_texture(texture)
{
	ASSERT(m_texture);

	m_texture->GetDevice(&m_dev);
	m_texture->GetDesc(&m_desc);

	m_dev->GetImmediateContext(&m_ctx);

	m_size.x = (int)m_desc.Width;
	m_size.y = (int)m_desc.Height;

	if(m_desc.BindFlags & D3D11_BIND_RENDER_TARGET) m_type = RenderTarget;
	else if(m_desc.BindFlags & D3D11_BIND_DEPTH_STENCIL) m_type = DepthStencil;
	else if(m_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) m_type = Texture;
	else if(m_desc.Usage == D3D11_USAGE_STAGING) m_type = Offscreen;

	m_format = (int)m_desc.Format;

	m_msaa = m_desc.SampleDesc.Count > 1;
}

bool GSTexture11::Update(const GSVector4i& r, const void* data, int pitch)
{
	if(m_dev && m_texture)
	{
		D3D11_BOX box = { (UINT)r.left, (UINT)r.top, 0U, (UINT)r.right, (UINT)r.bottom, 1U };

		m_ctx->UpdateSubresource(m_texture, 0, &box, data, pitch, 0);

		return true;
	}

	return false;
}

bool GSTexture11::Map(GSMap& m, const GSVector4i* r)
{
	if(r != NULL)
	{
		// ASSERT(0); // not implemented

		return false;
	}

	if(m_texture && m_desc.Usage == D3D11_USAGE_STAGING)
	{
		D3D11_MAPPED_SUBRESOURCE map;

		if(SUCCEEDED(m_ctx->Map(m_texture, 0, D3D11_MAP_READ_WRITE, 0, &map)))
		{
			m.bits = (uint8*)map.pData;
			m.pitch = (int)map.RowPitch;

			return true;
		}
	}

	return false;
}

void GSTexture11::Unmap()
{
	if(m_texture)
	{
		m_ctx->Unmap(m_texture, 0);
	}
}

bool GSTexture11::Save(const string& fn, bool user_image, bool dds)
{
	CComPtr<ID3D11Texture2D> res;
	D3D11_TEXTURE2D_DESC desc;

	m_texture->GetDesc(&desc);

	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	HRESULT hr = m_dev->CreateTexture2D(&desc, nullptr, &res);
	if (FAILED(hr))
	{
		return false;
	}

	m_ctx->CopyResource(res, m_texture);

	if (m_desc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
	{
		CComPtr<ID3D11Texture2D> dst;

		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;

		hr = m_dev->CreateTexture2D(&desc, nullptr, &dst);
		if (FAILED(hr))
		{
			return false;
		}

		D3D11_MAPPED_SUBRESOURCE sm, dm;

		hr = m_ctx->Map(res, 0, D3D11_MAP_READ, 0, &sm);
		if (FAILED(hr))
		{
			return false;
		}
		hr = m_ctx->Map(dst, 0, D3D11_MAP_WRITE, 0, &dm);
		if (FAILED(hr))
		{
			m_ctx->Unmap(res, 0);
			return false;
		}

		uint8* s = static_cast<uint8*>(sm.pData);
		uint8* d = static_cast<uint8*>(dm.pData);

		for (uint32 y = 0; y < desc.Height; y++, s += sm.RowPitch, d += dm.RowPitch)
		{
			for (uint32 x = 0; x < desc.Width; x++)
			{
				reinterpret_cast<uint32*>(d)[x] = static_cast<uint32>(ldexpf(reinterpret_cast<float*>(s)[x*2], 32));
			}
		}

		m_ctx->Unmap(res, 0);
		m_ctx->Unmap(dst, 0);

		res = dst;
	}

	res->GetDesc(&desc);

	GSPng::Format format;
	switch (desc.Format)
	{
	case DXGI_FORMAT_A8_UNORM:
		format = GSPng::R8I_PNG;
		break;
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		format = dds ? GSPng::RGBA_PNG : (m_desc.BindFlags & D3D11_BIND_DEPTH_STENCIL ? GSPng::RGB_A_PNG : GSPng::RGB_PNG);
		break;
	default:
		fprintf(stderr, "DXGI_FORMAT %d not saved to image\n", desc.Format);
		return false;
	}

	D3D11_MAPPED_SUBRESOURCE sm;
	hr = m_ctx->Map(res, 0, D3D11_MAP_READ, 0, &sm);
	if (FAILED(hr))
	{
		return false;
	}

	int compression = user_image ? Z_BEST_COMPRESSION : theApp.GetConfig("png_compression_level", Z_BEST_SPEED);
	bool success = GSPng::Save(format, fn, static_cast<uint8*>(sm.pData), desc.Width, desc.Height, sm.RowPitch, compression);

	m_ctx->Unmap(res, 0);

	return success;
}

GSTexture11::operator ID3D11Texture2D*()
{
	return m_texture;
}

GSTexture11::operator ID3D11ShaderResourceView*()
{
	if(!m_srv && m_dev && m_texture)
	{
		ASSERT(!m_msaa);

		m_dev->CreateShaderResourceView(m_texture, NULL, &m_srv);
	}

	return m_srv;
}

GSTexture11::operator ID3D11UnorderedAccessView*()
{
	if(!m_uav && m_dev && m_texture)
	{
		ASSERT(!m_msaa);

		m_dev->CreateUnorderedAccessView(m_texture, NULL, &m_uav);
	}

	return m_uav;
}

GSTexture11::operator ID3D11RenderTargetView*()
{
	ASSERT(m_dev);

	if(!m_rtv && m_dev && m_texture)
	{
		m_dev->CreateRenderTargetView(m_texture, NULL, &m_rtv);
	}

	return m_rtv;
}

GSTexture11::operator ID3D11DepthStencilView*()
{
	if(!m_dsv && m_dev && m_texture)
	{
		m_dev->CreateDepthStencilView(m_texture, NULL, &m_dsv);
	}

	return m_dsv;
}
