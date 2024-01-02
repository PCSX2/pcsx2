// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "GS/GS.h"
#include "GS/Renderers/Common/GSTexture.h"
#include "common/RedtapeWindows.h"
#include "common/RedtapeWilCom.h"
#include <d3d11.h>
#include <memory>

class GSTexture11 final : public GSTexture
{
	wil::com_ptr_nothrow<ID3D11Texture2D> m_texture;
	wil::com_ptr_nothrow<ID3D11ShaderResourceView> m_srv;
	wil::com_ptr_nothrow<ID3D11RenderTargetView> m_rtv;
	wil::com_ptr_nothrow<ID3D11DepthStencilView> m_dsv;
	wil::com_ptr_nothrow<ID3D11UnorderedAccessView> m_uav;
	D3D11_TEXTURE2D_DESC m_desc;
	int m_mapped_subresource;

public:
	explicit GSTexture11(wil::com_ptr_nothrow<ID3D11Texture2D> texture, const D3D11_TEXTURE2D_DESC& desc,
		GSTexture::Type type, GSTexture::Format format);

	static DXGI_FORMAT GetDXGIFormat(Format format);

	void* GetNativeHandle() const override;

	bool Update(const GSVector4i& r, const void* data, int pitch, int layer = 0) override;
	bool Map(GSMap& m, const GSVector4i* r = NULL, int layer = 0) override;
	void Unmap() override;
	void GenerateMipmap() override;

#ifdef PCSX2_DEVBUILD
	void SetDebugName(std::string_view name) override;
#endif

	operator ID3D11Texture2D*();
	operator ID3D11ShaderResourceView*();
	operator ID3D11RenderTargetView*();
	operator ID3D11DepthStencilView*();
	operator ID3D11UnorderedAccessView*();
};

class GSDownloadTexture11 final : public GSDownloadTexture
{
public:
	~GSDownloadTexture11() override;

	static std::unique_ptr<GSDownloadTexture11> Create(u32 width, u32 height, GSTexture::Format format);

	void CopyFromTexture(
		const GSVector4i& drc, GSTexture* stex, const GSVector4i& src, u32 src_level, bool use_transfer_pitch) override;

	bool Map(const GSVector4i& rc) override;
	void Unmap() override;

	void Flush() override;

#ifdef PCSX2_DEVBUILD
	void SetDebugName(std::string_view name) override;
#endif

private:
	GSDownloadTexture11(wil::com_ptr_nothrow<ID3D11Texture2D> tex, u32 width, u32 height, GSTexture::Format format);

	wil::com_ptr_nothrow<ID3D11Texture2D> m_texture;
};
