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
				u32 blend_index : 7;
				u32 abe : 1;
				u32 accu_blend : 1;
				u32 blend_mix : 1;
			};

			struct
			{
				// Color mask
				u32 wrgba : 4;
			};

			u32 key;
		};

		operator u32() { return key & 0x3fff; }

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
		ShaderMacro(std::string& smodel);
		void AddMacro(const char* n, int d);
		D3D_SHADER_MACRO* GetPtr(void);
	};

private:
	float m_hack_topleft_offset;
	int m_upscale_multiplier;
	int m_d3d_texsize;

	GSTexture* CreateSurface(GSTexture::Type type, int w, int h, bool mipmap, GSTexture::Format format) final;

	void DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect, const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, const GSVector4& c) final;
	void DoInterlace(GSTexture* sTex, GSTexture* dTex, int shader, bool linear, float yoffset = 0) final;
	void DoFXAA(GSTexture* sTex, GSTexture* dTex) final;
	void DoShadeBoost(GSTexture* sTex, GSTexture* dTex) final;
	void DoExternalFX(GSTexture* sTex, GSTexture* dTex) final;
	void BeforeDraw();
	void AfterDraw();

	u16 ConvertBlendEnum(u16 generic) final;

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
		std::array<ID3D11ShaderResourceView*, 16> ps_sr_views;
		std::array<GSTexture11*, 16> ps_sr_texture;
		ID3D11PixelShader* ps;
		ID3D11Buffer* ps_cb;
		ID3D11SamplerState* ps_ss[3];
		GSVector2i viewport;
		GSVector4i scissor;
		ID3D11DepthStencilState* dss;
		u8 sref;
		ID3D11BlendState* bs;
		float bf;
		ID3D11RenderTargetView* rt_view;
		GSTexture11* rt_texture;
		GSTexture11* rt_ds;
		ID3D11DepthStencilView* dsv;
		uint16_t ps_sr_bitfield;
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
	std::unordered_map<u64, wil::com_ptr_nothrow<ID3D11PixelShader>> m_ps;
	wil::com_ptr_nothrow<ID3D11Buffer> m_ps_cb;
	std::unordered_map<u32, wil::com_ptr_nothrow<ID3D11SamplerState>> m_ps_ss;
	wil::com_ptr_nothrow<ID3D11SamplerState> m_palette_ss;
	std::unordered_map<u32, wil::com_ptr_nothrow<ID3D11DepthStencilState>> m_om_dss;
	std::unordered_map<u32, wil::com_ptr_nothrow<ID3D11BlendState>> m_om_bs;
	wil::com_ptr_nothrow<ID3D11RasterizerState> m_rs;

	GSHWDrawConfig::VSConstantBuffer m_vs_cb_cache;
	GSHWDrawConfig::PSConstantBuffer m_ps_cb_cache;

	std::unique_ptr<GSTexture11> m_download_tex;

	std::string m_tfx_source;

protected:
	struct
	{
		D3D_FEATURE_LEVEL level;
		std::string model, vs, gs, ps, cs;
	} m_shader;

public:
	GSDevice11();
	virtual ~GSDevice11() {}

	bool SetFeatureLevel(D3D_FEATURE_LEVEL level, bool compat_mode);
	void GetFeatureLevel(D3D_FEATURE_LEVEL& level) const { level = m_shader.level; }

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

	void CloneTexture(GSTexture* src, GSTexture** dest);

	void CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r);

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
	void PSSetShaderResourceView(int i, ID3D11ShaderResourceView* srv, GSTexture* sr);
	void PSSetShader(ID3D11PixelShader* ps, ID3D11Buffer* ps_cb);
	void PSUpdateShaderState();
	void PSSetSamplerState(ID3D11SamplerState* ss0, ID3D11SamplerState* ss1);

	void OMSetDepthStencilState(ID3D11DepthStencilState* dss, u8 sref);
	void OMSetBlendState(ID3D11BlendState* bs, float bf);
	void OMSetRenderTargets(GSTexture* rt, GSTexture* ds, const GSVector4i* scissor = NULL);

	bool CreateTextureFX();
	void SetupVS(VSSelector sel, const GSHWDrawConfig::VSConstantBuffer* cb);
	void SetupGS(GSSelector sel);
	void SetupPS(PSSelector sel, const GSHWDrawConfig::PSConstantBuffer* cb, PSSamplerSelector ssel);
	void SetupOM(OMDepthStencilSelector dssel, OMBlendSelector bsel, u8 afix);

	void RenderHW(GSHWDrawConfig& config) final;

	ID3D11Device* operator->() { return m_dev.get(); }
	operator ID3D11Device*() { return m_dev.get(); }
	operator ID3D11DeviceContext*() { return m_ctx.get(); }

	void CreateShader(const std::string& source, const char* fn, ID3DInclude* include, const char* entry, D3D_SHADER_MACRO* macro, ID3D11VertexShader** vs, D3D11_INPUT_ELEMENT_DESC* layout, int count, ID3D11InputLayout** il);
	void CreateShader(const std::string& source, const char* fn, ID3DInclude* include, const char* entry, D3D_SHADER_MACRO* macro, ID3D11GeometryShader** gs);
	void CreateShader(const std::string& source, const char* fn, ID3DInclude* include, const char* entry, D3D_SHADER_MACRO* macro, ID3D11PixelShader** ps);

	void CompileShader(const std::string& source, const char* fn, ID3DInclude* include, const char* entry, D3D_SHADER_MACRO* macro, ID3DBlob** shader, const std::string& shader_model);
};
