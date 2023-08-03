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
#include "GS/Renderers/DX11/D3D11ShaderCache.h"
#include <unordered_map>
#include <wil/com.h>
#include <dxgi1_5.h>
#include <d3d11_1.h>

struct GSVertexShader11
{
	wil::com_ptr_nothrow<ID3D11VertexShader> vs;
	wil::com_ptr_nothrow<ID3D11InputLayout> il;
};

class GSDevice11 final : public GSDevice
{
public:
	using VSSelector = GSHWDrawConfig::VSSelector;
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
		void AddMacro(const char* n, int d);
		void AddMacro(const char* n, std::string d);
		D3D_SHADER_MACRO* GetPtr();
	};

private:
	enum : u32
	{
		MAX_TEXTURES = 4,
		MAX_SAMPLERS = 1,
		VERTEX_BUFFER_SIZE = 32 * 1024 * 1024,
		INDEX_BUFFER_SIZE = 16 * 1024 * 1024,
		NUM_TIMESTAMP_QUERIES = 5,
	};

	void SetFeatures(IDXGIAdapter1* adapter);

	bool CreateSwapChain();
	bool CreateSwapChainRTV();
	void DestroySwapChain();

	bool CreateTimestampQueries();
	void DestroyTimestampQueries();
	void PopTimestampQuery();
	void KickTimestampQuery();

	void DoMerge(GSTexture* sTex[3], GSVector4* sRect, GSTexture* dTex, GSVector4* dRect, const GSRegPMODE& PMODE, const GSRegEXTBUF& EXTBUF, u32 c, const bool linear) override;
	void DoInterlace(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ShaderInterlace shader, bool linear, const InterlaceConstantBuffer& cb) override;
	void DoFXAA(GSTexture* sTex, GSTexture* dTex) override;
	void DoShadeBoost(GSTexture* sTex, GSTexture* dTex, const float params[4]) override;

	bool CreateCASShaders();
	bool DoCAS(GSTexture* sTex, GSTexture* dTex, bool sharpen_only, const std::array<u32, NUM_CAS_CONSTANTS>& constants) override;

	bool CreateImGuiResources();
	void RenderImGui();

	wil::com_ptr_nothrow<IDXGIFactory5> m_dxgi_factory;
	wil::com_ptr_nothrow<ID3D11Device1> m_dev;
	wil::com_ptr_nothrow<ID3D11DeviceContext1> m_ctx;
	wil::com_ptr_nothrow<ID3DUserDefinedAnnotation> m_annotation;

	wil::com_ptr_nothrow<IDXGISwapChain1> m_swap_chain;
	wil::com_ptr_nothrow<ID3D11RenderTargetView> m_swap_chain_rtv;

	wil::com_ptr_nothrow<ID3D11Buffer> m_vb;
	wil::com_ptr_nothrow<ID3D11Buffer> m_ib;
	wil::com_ptr_nothrow<ID3D11Buffer> m_expand_vb;
	wil::com_ptr_nothrow<ID3D11Buffer> m_expand_ib;
	wil::com_ptr_nothrow<ID3D11ShaderResourceView> m_expand_vb_srv;
	u32 m_vb_pos = 0; // bytes
	u32 m_ib_pos = 0; // indices/sizeof(u32)
	u32 m_structured_vb_pos = 0; // bytes

	bool m_allow_tearing_supported = false;
	bool m_using_flip_model_swap_chain = true;
	bool m_using_allow_tearing = false;
	bool m_is_exclusive_fullscreen = false;

	struct
	{
		ID3D11InputLayout* layout;
		D3D11_PRIMITIVE_TOPOLOGY topology;
		ID3D11Buffer* index_buffer;
		ID3D11VertexShader* vs;
		ID3D11Buffer* vs_cb;
		std::array<ID3D11ShaderResourceView*, MAX_TEXTURES> ps_sr_views;
		ID3D11PixelShader* ps;
		ID3D11Buffer* ps_cb;
		std::array<ID3D11SamplerState*, MAX_SAMPLERS> ps_ss;
		GSVector2i viewport;
		GSVector4i scissor;
		u32 vb_stride;
		ID3D11DepthStencilState* dss;
		u8 sref;
		ID3D11BlendState* bs;
		float bf;
		ID3D11RenderTargetView* rt_view;
		ID3D11DepthStencilView* dsv;
	} m_state;

	std::array<std::array<wil::com_ptr_nothrow<ID3D11Query>, 3>, NUM_TIMESTAMP_QUERIES> m_timestamp_queries = {};
	float m_accumulated_gpu_time = 0.0f;
	u8 m_read_timestamp_query = 0;
	u8 m_write_timestamp_query = 0;
	u8 m_waiting_timestamp_queries = 0;
	bool m_timestamp_query_started = false;
	bool m_gpu_timing_enabled = false;

	struct
	{
		wil::com_ptr_nothrow<ID3D11InputLayout> il;
		wil::com_ptr_nothrow<ID3D11VertexShader> vs;
		wil::com_ptr_nothrow<ID3D11PixelShader> ps[static_cast<int>(ShaderConvert::Count)];
		wil::com_ptr_nothrow<ID3D11SamplerState> ln;
		wil::com_ptr_nothrow<ID3D11SamplerState> pt;
		wil::com_ptr_nothrow<ID3D11DepthStencilState> dss;
		wil::com_ptr_nothrow<ID3D11DepthStencilState> dss_write;
		std::array<wil::com_ptr_nothrow<ID3D11BlendState>, 16> bs;
	} m_convert;

	struct
	{
		wil::com_ptr_nothrow<ID3D11InputLayout> il;
		wil::com_ptr_nothrow<ID3D11VertexShader> vs;
		wil::com_ptr_nothrow<ID3D11PixelShader> ps[static_cast<int>(PresentShader::Count)];
		wil::com_ptr_nothrow<ID3D11Buffer> ps_cb;
	} m_present;

	struct
	{
		wil::com_ptr_nothrow<ID3D11PixelShader> ps[2];
		wil::com_ptr_nothrow<ID3D11Buffer> cb;
		wil::com_ptr_nothrow<ID3D11BlendState> bs;
	} m_merge;

	struct
	{
		wil::com_ptr_nothrow<ID3D11PixelShader> ps[NUM_INTERLACE_SHADERS];
		wil::com_ptr_nothrow<ID3D11Buffer> cb;
	} m_interlace;

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
		wil::com_ptr_nothrow<ID3D11PixelShader> primid_init_ps[2];
	} m_date;

	struct
	{
		wil::com_ptr_nothrow<ID3D11Buffer> cb;
		wil::com_ptr_nothrow<ID3D11ComputeShader> cs_upscale;
		wil::com_ptr_nothrow<ID3D11ComputeShader> cs_sharpen;
	} m_cas;

	struct
	{
		wil::com_ptr_nothrow<ID3D11InputLayout> il;
		wil::com_ptr_nothrow<ID3D11VertexShader> vs;
		wil::com_ptr_nothrow<ID3D11PixelShader> ps;
		wil::com_ptr_nothrow<ID3D11BlendState> bs;
		wil::com_ptr_nothrow<ID3D11Buffer> vs_cb;
	} m_imgui;

	// Shaders...

	std::unordered_map<u32, GSVertexShader11> m_vs;
	wil::com_ptr_nothrow<ID3D11Buffer> m_vs_cb;
	std::unordered_map<u32, wil::com_ptr_nothrow<ID3D11GeometryShader>> m_gs;
	std::unordered_map<PSSelector, wil::com_ptr_nothrow<ID3D11PixelShader>, GSHWDrawConfig::PSSelectorHash> m_ps;
	wil::com_ptr_nothrow<ID3D11Buffer> m_ps_cb;
	std::unordered_map<u32, wil::com_ptr_nothrow<ID3D11SamplerState>> m_ps_ss;
	std::unordered_map<u32, wil::com_ptr_nothrow<ID3D11DepthStencilState>> m_om_dss;
	std::unordered_map<u32, wil::com_ptr_nothrow<ID3D11BlendState>> m_om_bs;
	wil::com_ptr_nothrow<ID3D11RasterizerState> m_rs;

	GSHWDrawConfig::VSConstantBuffer m_vs_cb_cache;
	GSHWDrawConfig::PSConstantBuffer m_ps_cb_cache;

	D3D11ShaderCache m_shader_cache;
	std::string m_tfx_source;

public:
	GSDevice11();
	~GSDevice11() override;

	__fi static GSDevice11* GetInstance() { return static_cast<GSDevice11*>(g_gs_device.get()); }
	__fi ID3D11Device1* GetD3DDevice() const { return m_dev.get(); }
	__fi ID3D11DeviceContext1* GetD3DContext() const { return m_ctx.get(); }

	bool Create() override;
	void Destroy() override;

	RenderAPI GetRenderAPI() const override;

	bool UpdateWindow() override;
	void ResizeWindow(s32 new_window_width, s32 new_window_height, float new_window_scale) override;
	bool SupportsExclusiveFullscreen() const override;
	bool HasSurface() const override;
	void DestroySurface() override;
	std::string GetDriverInfo() const override;

	bool GetHostRefreshRate(float* refresh_rate) override;

	void SetVSync(VsyncMode mode) override;

	PresentResult BeginPresent(bool frame_skip) override;
	void EndPresent() override;

	bool SetGPUTimingEnabled(bool enabled) override;
	float GetAndResetAccumulatedGPUTime() override;

	void DrawPrimitive();
	void DrawIndexedPrimitive();
	void DrawIndexedPrimitive(int offset, int count);

	void PushDebugGroup(const char* fmt, ...) override;
	void PopDebugGroup() override;
	void InsertDebugMessage(DebugMessageCategory category, const char* fmt, ...) override;

	GSTexture* CreateSurface(GSTexture::Type type, int width, int height, int levels, GSTexture::Format format) override;
	std::unique_ptr<GSDownloadTexture> CreateDownloadTexture(u32 width, u32 height, GSTexture::Format format) override;

	void CommitClear(GSTexture* t);
	void CloneTexture(GSTexture* src, GSTexture** dest, const GSVector4i& rect);

	void CopyRect(GSTexture* sTex, GSTexture* dTex, const GSVector4i& r, u32 destX, u32 destY) override;

	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ShaderConvert shader = ShaderConvert::COPY, bool linear = true) override;
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ID3D11PixelShader* ps, ID3D11Buffer* ps_cb, bool linear = true);
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, bool red, bool green, bool blue, bool alpha) override;
	void StretchRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, ID3D11PixelShader* ps, ID3D11Buffer* ps_cb, ID3D11BlendState* bs, bool linear = true);
	void PresentRect(GSTexture* sTex, const GSVector4& sRect, GSTexture* dTex, const GSVector4& dRect, PresentShader shader, float shaderTime, bool linear) override;
	void UpdateCLUTTexture(GSTexture* sTex, float sScale, u32 offsetX, u32 offsetY, GSTexture* dTex, u32 dOffset, u32 dSize) override;
	void ConvertToIndexedTexture(GSTexture* sTex, float sScale, u32 offsetX, u32 offsetY, u32 SBW, u32 SPSM, GSTexture* dTex, u32 DBW, u32 DPSM) override;
	void DrawMultiStretchRects(const MultiStretchRect* rects, u32 num_rects, GSTexture* dTex, ShaderConvert shader) override;
	void DoMultiStretchRects(const MultiStretchRect* rects, u32 num_rects, const GSVector2& ds);

	void SetupDATE(GSTexture* rt, GSTexture* ds, const GSVertexPT1* vertices, bool datm);

	void* IAMapVertexBuffer(u32 stride, u32 count);
	void IAUnmapVertexBuffer(u32 stride, u32 count);
	bool IASetVertexBuffer(const void* vertex, u32 stride, u32 count);
	bool IASetExpandVertexBuffer(const void* vertex, u32 stride, u32 count);

	u16* IAMapIndexBuffer(u32 count);
	void IAUnmapIndexBuffer(u32 count);
	bool IASetIndexBuffer(const void* index, u32 count);
	void IASetIndexBuffer(ID3D11Buffer* buffer);

	void IASetInputLayout(ID3D11InputLayout* layout);
	void IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY topology);

	void VSSetShader(ID3D11VertexShader* vs, ID3D11Buffer* vs_cb);

	void PSSetShaderResource(int i, GSTexture* sr);
	void PSSetShader(ID3D11PixelShader* ps, ID3D11Buffer* ps_cb);
	void PSUpdateShaderState();
	void PSSetSamplerState(ID3D11SamplerState* ss0);

	void OMSetDepthStencilState(ID3D11DepthStencilState* dss, u8 sref);
	void OMSetBlendState(ID3D11BlendState* bs, float bf);
	void OMSetRenderTargets(GSTexture* rt, GSTexture* ds, const GSVector4i* scissor = nullptr);
	void SetViewport(const GSVector2i& viewport);
	void SetScissor(const GSVector4i& scissor);

	bool CreateTextureFX();
	void SetupVS(VSSelector sel, const GSHWDrawConfig::VSConstantBuffer* cb);
	void SetupPS(const PSSelector& sel, const GSHWDrawConfig::PSConstantBuffer* cb, PSSamplerSelector ssel);
	void SetupOM(OMDepthStencilSelector dssel, OMBlendSelector bsel, u8 afix);

	void RenderHW(GSHWDrawConfig& config) override;

	void ClearSamplerCache() override;

	ID3D11Device1* operator->() { return m_dev.get(); }
	operator ID3D11Device1*() { return m_dev.get(); }
	operator ID3D11DeviceContext1*() { return m_ctx.get(); }
};
