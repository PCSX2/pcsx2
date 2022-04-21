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

#include "GSTexture11.h"
#include "GS/GSVector.h"
#include "GS/Renderers/Common/GSDevice.h"
#include "common/D3D11/ShaderCache.h"
#include <unordered_map>
#include <wil/com.h>
#include <dxgi1_3.h>

struct GSVertexShader11
{
	wil::com_ptr_nothrow<ID3D11VertexShader> vs;
	wil::com_ptr_nothrow<ID3D11InputLayout> il;
};

class GSDevice11 final : public GSDevice
{
public:
	using VSSelector = GSHWDrawConfig::VSSelector;
	using GSSelector = GSHWDrawConfig::GSSelector;
	using PSSelector = GSHWDrawConfig::PSSelector;
	using PSSamplerSelector = GSHWDrawConfig::SamplerSelector;
	using OMDepthStencilSelector = GSHWDrawConfig::DepthStencilSelector;

#pragma pack(push, 1)
	struct OMBlendSelector
	{
		union
		{
			struct
			{
				// Color mask
				u32 wr : 1;
				u32 wg : 1;
				u32 wb : 1;
				u32 wa : 1;
				// Alpha blending
				u32 blend_enable : 1;
				u32 blend_op : 2;
				u32 blend_src_factor : 4;
				u32 blend_dst_factor : 4;
			};

			struct
			{
				// Color mask
				u32 wrgba : 4;
			};

			u32 key;
		};

		operator u32() { return key & 0x7fff; }

		OMBlendSelector()
			: key(0)
		{
		}
	};

#pragma pack(pop)

	class ShaderMacro
	{
		struct mcstr
		{
			const char *name, *def;
			mcstr(const char* n, const char* d)
				: name(n)
				, def(d)
			{
			}
		};

		struct mstring
		{
			std::string name, def;
			mstring(const char* n, std::string d)
				: name(n)
				, def(d)
			{
			}
		};

		std::vector<mstring> mlist;
		std::vector<mcstr> mout;

	public:
		ShaderMacro(D3D_FEATURE_LEVEL fl);
		void AddMacro(const char* n, int d);
		D3D_SHADER_MACRO* GetPtr(void);
	};

private:
	// Increment this constant whenever shaders change, to invalidate user's shader cache.
	static constexpr u32 SHADER_VERSION = 1;

	static constexpr u32 MAX_TEXTURES = 3;
	static constexpr u32 MAX_SAMPLERS = 2;

	int m_d3d_texsize;

	void SetFeatures();

	GSTexture* CreateSurface(GSTexture::Type type, int width, int height, int levels, GSTexture::Format format) final;

	void DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect, const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, const GSVector4& c) final;
	void DoInterlace(GSTexture* sTex, GSTexture* dTex, int shader, bool linear, float yoffset = 0) final;
	void DoFXAA(GSTexture* sTex, GSTexture* dTex) final;
	void DoShadeBoost(GSTexture* sTex, GSTexture* dTex, const float params[4]) final;
	void DoExternalFX(GSTexture* sTex, GSTexture* dTex) final;

	wil::com_ptr_nothrow<ID3D11Device> m_dev;
	wil::com_ptr_nothrow<ID3D11DeviceContext> m_ctx;
	wil::com_ptr_nothrow<IDXGISwapChain1> m_swapchain;
	wil::com_ptr_nothrow<ID3D11Buffer> m_vb;
	wil::com_ptr_nothrow<ID3D11Buffer> m_ib;

	struct
	{
		ID3D11Buffer* vb;
		size_t vb_stride;
		ID3D11Buffer* ib;
		ID3D11InputLayout* layout;
		D3D11_PRIMITIVE_TOPOLOGY topology;
		ID3D11VertexShader* vs;
		ID3D11Buffer* vs_cb;
		ID3D11GeometryShader* gs;
		ID3D11Buffer* gs_cb;
		std::array<ID3D11ShaderResourceView*, MAX_TEXTURES> ps_sr_views;
		ID3D11PixelShader* ps;
		ID3D11Buffer* ps_cb;
		std::array<ID3D11SamplerState*, MAX_SAMPLERS> ps_ss;
		GSVector2i viewport;
		GSVector4i scissor;
		ID3D11DepthStencilState* dss;
		u8 sref;
		ID3D11BlendState* bs;
		float bf;
		ID3D11RenderTargetView* rt_view;
		ID3D11DepthStencilView* dsv;
	} m_state;


	struct
	{
		wil::com_ptr_nothrow<ID3D11InputLayout> il;
		wil::com_ptr_nothrow<ID3D11VertexShader> vs;
		wil::com_ptr_nothrow<ID3D11PixelShader> ps[static_cast<int>(ShaderConvert::Count)];
		wil::com_ptr_nothrow<ID3D11SamplerState> ln;
		wil::com_ptr_nothrow<ID3D11SamplerState> pt;
		wil::com_ptr_nothrow<ID3D11DepthStencilState> dss;
		wil::com_ptr_nothrow<ID3D11DepthStencilState> dss_write;
		wil::com_ptr_nothrow<ID3D11BlendState> bs;
	} m_convert;

	struct
	{
		wil::com_ptr_nothrow<ID3D11PixelShader> ps[2];
		wil::com_ptr_nothrow<ID3D11Buffer> cb;
		wil::com_ptr_nothrow<ID3D11BlendState> bs;
	} m_merge;

	struct
	{
		wil::com_ptr_nothrow<ID3D11PixelShader> ps[4];
		wil::com_ptr_nothrow<ID3D11Buffer> cb;
	} m_interlace;

	struct
	{
		wil::com_ptr_nothrow<ID3D11PixelShader> ps;
		wil::com_ptr_nothrow<ID3D11Buffer> cb;
	} m_shaderfx;

	wil::com_ptr_nothrow<ID3D11PixelShader> m_fxaa_ps;

	struct
	{
		wil::com_ptr_nothrow<ID3D11PixelShader> ps;
		wil::com_ptr_nothrow<ID3D11Buffer> cb;
	} m_shadeboost;

	struct
	{
		wil::com_ptr_nothrow<ID3D11DepthStencilState> dss;
		wil::com_ptr_nothrow<ID3D11BlendState> bs;
	} m_date;

	// Shaders...

	std::unordered_map<u32, GSVertexShader11> m_vs;
	wil::com_ptr_nothrow<ID3D11Buffer> m_vs_cb;
	std::unordered_map<u32, wil::com_ptr_nothrow<ID3D11GeometryShader>> m_gs;
	std::unordered_map<PSSelector, wil::com_ptr_nothrow<ID3D11PixelShader>, GSHWDrawConfig::PSSelectorHash> m_ps;
	wil::com_ptr_nothrow<ID3D11Buffer> m_ps_cb;
	std::unordered_map<u32, wil::com_ptr_nothrow<ID3D11SamplerState>> m_ps_ss;
	wil::com_ptr_nothrow<ID3D11SamplerState> m_palette_ss;
	std::unordered_map<u32, wil::com_ptr_nothrow<ID3D11DepthStencilState>> m_om_dss;
	std::unordered_map<u32, wil::com_ptr_nothrow<ID3D11BlendState>> m_om_bs;
	wil::com_ptr_nothrow<ID3D11RasterizerState> m_rs;

	GSHWDrawConfig::VSConstantBuffer m_vs_cb_cache;
	GSHWDrawConfig::PSConstantBuffer m_ps_cb_cache;

	std::unique_ptr<GSTexture11> m_download_tex;

	D3D11::ShaderCache m_shader_cache;
	std::string m_tfx_source;

public:
	GSDevice11();
	~GSDevice11() override;

	__fi static GSDevice11* GetInstance() { return static_cast<GSDevice11*>(g_gs_device.get()); }
	__fi ID3D11Device* GetD3DDevice() const { return m_dev.get(); }
	__fi ID3D11DeviceContext* GetD3DContext() const { return m_ctx.get(); }

	bool Create(HostDisplay* display);

	void ResetAPIState() override;
	void RestoreAPIState() override;

	void DrawPrimitive();
	void DrawIndexedPrimitive();
	void DrawIndexedPrimitive(int offset, int count);

	void ClearRenderTarget(GSTexture* t, const GSVector4& c) final;
	void ClearRenderTarget(GSTexture* t, u32 c) final;
	void ClearDepth(GSTexture* t) final;
	void ClearStencil(GSTexture* t, u8 c) final;

	bool DownloadTexture(GSTexture* src, const GSVector4i& rect, GSTexture::GSMap& out_map) final;
	void DownloadTextureComplete() final;

	void CloneTexture(GSTexture* src, GSTexture** dest, const GSVector4i& rect);

	void CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r, u32 destX, u32 destY) override;

	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ShaderConvert shader = ShaderConvert::COPY, bool linear = true) final;
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ID3D11PixelShader* ps, ID3D11Buffer* ps_cb, bool linear = true);
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, bool red, bool green, bool blue, bool alpha);
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ID3D11PixelShader* ps, ID3D11Buffer* ps_cb, ID3D11BlendState* bs, bool linear = true);

	void SetupDATE(GSTexture* rt, GSTexture* ds, const GSVertexPT1* vertices, bool datm);

	void IASetVertexBuffer(const void* vertex, size_t stride, size_t count);
	bool IAMapVertexBuffer(void** vertex, size_t stride, size_t count);
	void IAUnmapVertexBuffer();
	void IASetVertexBuffer(ID3D11Buffer* vb, size_t stride);
	void IASetIndexBuffer(const void* index, size_t count);
	void IASetIndexBuffer(ID3D11Buffer* ib);
	void IASetInputLayout(ID3D11InputLayout* layout);
	void IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY topology);

	void VSSetShader(ID3D11VertexShader* vs, ID3D11Buffer* vs_cb);
	void GSSetShader(ID3D11GeometryShader* gs, ID3D11Buffer* gs_cb = NULL);

	void PSSetShaderResources(GSTexture* sr0, GSTexture* sr1);
	void PSSetShaderResource(int i, GSTexture* sr);
	void PSSetShader(ID3D11PixelShader* ps, ID3D11Buffer* ps_cb);
	void PSUpdateShaderState();
	void PSSetSamplerState(ID3D11SamplerState* ss0, ID3D11SamplerState* ss1);

	void OMSetDepthStencilState(ID3D11DepthStencilState* dss, u8 sref);
	void OMSetBlendState(ID3D11BlendState* bs, float bf);
	void OMSetRenderTargets(GSTexture* rt, GSTexture* ds, const GSVector4i* scissor = NULL);

	bool CreateTextureFX();
	void SetupVS(VSSelector sel, const GSHWDrawConfig::VSConstantBuffer* cb);
	void SetupGS(GSSelector sel);
	void SetupPS(const PSSelector& sel, const GSHWDrawConfig::PSConstantBuffer* cb, PSSamplerSelector ssel);
	void SetupOM(OMDepthStencilSelector dssel, OMBlendSelector bsel, u8 afix);

	void RenderHW(GSHWDrawConfig& config) final;

	void ClearSamplerCache() final;

	ID3D11Device* operator->() { return m_dev.get(); }
	operator ID3D11Device*() { return m_dev.get(); }
	operator ID3D11DeviceContext*() { return m_ctx.get(); }
};
