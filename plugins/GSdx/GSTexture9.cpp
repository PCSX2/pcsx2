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
#include "GSTexture9.h"
#include "GSPng.h"

GSTexture9::GSTexture9(IDirect3DSurface9* surface)
{
	m_surface = surface;

	surface->GetDevice(&m_dev);
	surface->GetDesc(&m_desc);

	if(m_desc.Type != D3DRTYPE_SURFACE)
	{
		surface->GetContainer(__uuidof(IDirect3DTexture9), (void**)&m_texture);

		ASSERT(m_texture != NULL);
	}

	m_size.x = (int)m_desc.Width;
	m_size.y = (int)m_desc.Height;

	if(m_desc.Usage & D3DUSAGE_RENDERTARGET) m_type = RenderTarget;
	else if(m_desc.Usage & D3DUSAGE_DEPTHSTENCIL) m_type = DepthStencil;
	else if(m_desc.Pool == D3DPOOL_MANAGED) m_type = Texture;
	else if(m_desc.Pool == D3DPOOL_SYSTEMMEM) m_type = Offscreen;

	m_format = (int)m_desc.Format;

	m_msaa = m_desc.MultiSampleType != D3DMULTISAMPLE_NONE;
}

GSTexture9::GSTexture9(IDirect3DTexture9* texture)
{
	m_texture = texture;

	texture->GetDevice(&m_dev);
	texture->GetLevelDesc(0, &m_desc);
	texture->GetSurfaceLevel(0, &m_surface);

	ASSERT(m_surface != NULL);

	m_size.x = (int)m_desc.Width;
	m_size.y = (int)m_desc.Height;

	if(m_desc.Usage & D3DUSAGE_RENDERTARGET) m_type = RenderTarget;
	else if(m_desc.Usage & D3DUSAGE_DEPTHSTENCIL) m_type = DepthStencil;
	else if(m_desc.Pool == D3DPOOL_MANAGED) m_type = Texture;
	else if(m_desc.Pool == D3DPOOL_SYSTEMMEM) m_type = Offscreen;

	m_format = (int)m_desc.Format;

	m_msaa = m_desc.MultiSampleType > 1;
}

GSTexture9::~GSTexture9()
{
}

bool GSTexture9::Update(const GSVector4i& r, const void* data, int pitch)
{
	if(m_surface)
	{
		D3DLOCKED_RECT lr;

		if(SUCCEEDED(m_surface->LockRect(&lr, r, 0)))
		{
			uint8* src = (uint8*)data;
			uint8* dst = (uint8*)lr.pBits;

			int bytes = r.width() * sizeof(uint32);

			switch(m_desc.Format)
			{
			case D3DFMT_A8: bytes >>= 2; break;
			case D3DFMT_A1R5G5B5: bytes >>= 1; break;
			default: ASSERT(m_desc.Format == D3DFMT_A8R8G8B8); break;
			}

			bytes = min(bytes, pitch);
			bytes = min(bytes, lr.Pitch);

			for(int i = 0, j = r.height(); i < j; i++, src += pitch, dst += lr.Pitch)
			{
				memcpy(dst, src, bytes);
			}

			m_surface->UnlockRect();

			return true;
		}
	}

	return false;
}

bool GSTexture9::Map(GSMap& m, const GSVector4i* r)
{
	HRESULT hr;

	if(m_surface)
	{
		D3DLOCKED_RECT lr;

		if(SUCCEEDED(hr = m_surface->LockRect(&lr, (LPRECT)r, 0)))
		{
			m.bits = (uint8*)lr.pBits;
			m.pitch = (int)lr.Pitch;

			return true;
		}
	}

	return false;
}

void GSTexture9::Unmap()
{
	if(m_surface)
	{
		m_surface->UnlockRect();
	}
}

bool GSTexture9::Save(const string& fn, bool user_image, bool dds)
{
	bool rb_swapped = true;
	CComPtr<IDirect3DSurface9> surface;

	D3DSURFACE_DESC desc;
	m_surface->GetDesc(&desc);

	if (m_desc.Usage & D3DUSAGE_DEPTHSTENCIL && desc.Format != D3DFMT_D32F_LOCKABLE)
	{
		return false;
	}

	if (desc.Format == D3DFMT_A8 || desc.Pool == D3DPOOL_MANAGED || desc.Usage == D3DUSAGE_DEPTHSTENCIL)
	{
		surface = m_surface;
		rb_swapped = false;
	}
	else
	{
		HRESULT hr;

		hr = m_dev->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &surface, nullptr);
		if (FAILED(hr))
		{
			return false;
		}

		hr = m_dev->GetRenderTargetData(m_surface, surface);
		if (FAILED(hr))
		{
			return false;
		}
	}

	GSPng::Format format;
	switch (desc.Format)
	{
	case D3DFMT_A8:
		format = GSPng::R8I_PNG;
		break;
	case D3DFMT_A8R8G8B8:
		format = dds? GSPng::RGBA_PNG : GSPng::RGB_PNG;
		break;
	case D3DFMT_D32F_LOCKABLE:
		format = GSPng::RGB_A_PNG;
		break;
	default:
		fprintf(stderr, "D3DFMT %d not saved to image\n", desc.Format);
		return false;
	}

	D3DLOCKED_RECT slr;
	HRESULT hr = surface->LockRect(&slr, nullptr, 0);
	if (FAILED(hr))
	{
		return false;
	}

	int compression = user_image ? Z_BEST_COMPRESSION : theApp.GetConfig("png_compression_level", Z_BEST_SPEED);
	bool success = GSPng::Save(format, fn, static_cast<uint8*>(slr.pBits), desc.Width, desc.Height, slr.Pitch, compression, rb_swapped);

	surface->UnlockRect();
	return success;
}

GSTexture9::operator IDirect3DSurface9*()
{
	return m_surface;
}

GSTexture9::operator IDirect3DTexture9*()
{
	return m_texture;
}
