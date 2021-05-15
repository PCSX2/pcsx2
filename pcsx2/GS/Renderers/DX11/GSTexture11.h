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

#pragma once

#include "GS.h"
#include "GS/Renderers/Common/GSTexture.h"

class GSTexture11 : public GSTexture
{
	CComPtr<ID3D11Device> m_dev;
	CComPtr<ID3D11DeviceContext> m_ctx;
	CComPtr<ID3D11Texture2D> m_texture;
	D3D11_TEXTURE2D_DESC m_desc;
	CComPtr<ID3D11ShaderResourceView> m_srv;
	CComPtr<ID3D11RenderTargetView> m_rtv;
	CComPtr<ID3D11DepthStencilView> m_dsv;

	int m_layer;
	int m_max_layer;

public:
	explicit GSTexture11(ID3D11Texture2D* texture);

	bool Update(const GSVector4i& r, const void* data, int pitch, int layer = 0);
	bool Map(GSMap& m, const GSVector4i* r = NULL, int layer = 0);
	void Unmap();
	bool Save(const std::string& fn);
	bool Equal(GSTexture11* tex);

	operator ID3D11Texture2D*();
	operator ID3D11ShaderResourceView*();
	operator ID3D11RenderTargetView*();
	operator ID3D11DepthStencilView*();
};
