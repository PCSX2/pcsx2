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
#include "GS/resource.h"
#include "GS/GSTables.h"

bool GSDevice11::CreateTextureFX()
{
	HRESULT hr;

	D3D11_BUFFER_DESC bd;

	memset(&bd, 0, sizeof(bd));

	bd.ByteWidth = sizeof(VSConstantBuffer);
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	hr = m_dev->CreateBuffer(&bd, NULL, &m_vs_cb);

	if (FAILED(hr))
		return false;

	memset(&bd, 0, sizeof(bd));

	bd.ByteWidth = sizeof(GSConstantBuffer);
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	hr = m_dev->CreateBuffer(&bd, NULL, &m_gs_cb);

	if (FAILED(hr))
		return false;

	memset(&bd, 0, sizeof(bd));

	bd.ByteWidth = sizeof(PSConstantBuffer);
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	hr = m_dev->CreateBuffer(&bd, NULL, &m_ps_cb);

	if (FAILED(hr))
		return false;

	D3D11_SAMPLER_DESC sd;

	memset(&sd, 0, sizeof(sd));

	sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.MinLOD = -FLT_MAX;
	sd.MaxLOD = FLT_MAX;
	sd.MaxAnisotropy = D3D11_MIN_MAXANISOTROPY;
	sd.ComparisonFunc = D3D11_COMPARISON_NEVER;

	hr = m_dev->CreateSamplerState(&sd, &m_palette_ss);

	if (FAILED(hr))
		return false;

	// create layout

	VSSelector sel;
	VSConstantBuffer cb;

	SetupVS(sel, &cb);

	GSConstantBuffer gcb;

	SetupGS(GSSelector(1), &gcb);

	//

	return true;
}

void GSDevice11::SetupVS(VSSelector sel, const VSConstantBuffer* cb)
{
	auto i = std::as_const(m_vs).find(sel);

	if (i == m_vs.end())
	{
		ShaderMacro sm(m_shader.model);

		sm.AddMacro("VS_TME", sel.tme);
		sm.AddMacro("VS_FST", sel.fst);

		D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"POSITION", 0, DXGI_FORMAT_R16G16_UINT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"POSITION", 1, DXGI_FORMAT_R32_UINT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 2, DXGI_FORMAT_R16G16_UINT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"COLOR", 1, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
		};

		GSVertexShader11 vs;

		std::vector<char> shader;
		theApp.LoadResource(IDR_TFX_FX, shader);
		CreateShader(shader, "tfx.fx", nullptr, "vs_main", sm.GetPtr(), &vs.vs, layout, countof(layout), &vs.il);

		m_vs[sel] = vs;

		i = m_vs.find(sel);
	}

	if (m_vs_cb_cache.Update(cb))
	{
		ID3D11DeviceContext* ctx = m_ctx;

		ctx->UpdateSubresource(m_vs_cb, 0, NULL, cb, 0, 0);
	}

	VSSetShader(i->second.vs, m_vs_cb);

	IASetInputLayout(i->second.il);
}

void GSDevice11::SetupGS(GSSelector sel, const GSConstantBuffer* cb)
{
	CComPtr<ID3D11GeometryShader> gs;

	const bool unscale_pt_ln = (sel.point == 1 || sel.line == 1);
	// Geometry shader is disabled if sprite conversion is done on the cpu (sel.cpu_sprite).
	if ((sel.prim > 0 && sel.cpu_sprite == 0 && (sel.iip == 0 || sel.prim == 3)) || unscale_pt_ln)
	{
		const auto i = std::as_const(m_gs).find(sel);

		if (i != m_gs.end())
		{
			gs = i->second;
		}
		else
		{
			ShaderMacro sm(m_shader.model);

			sm.AddMacro("GS_IIP", sel.iip);
			sm.AddMacro("GS_PRIM", sel.prim);
			sm.AddMacro("GS_POINT", sel.point);
			sm.AddMacro("GS_LINE", sel.line);

			std::vector<char> shader;
			theApp.LoadResource(IDR_TFX_FX, shader);
			CreateShader(shader, "tfx.fx", nullptr, "gs_main", sm.GetPtr(), &gs);

			m_gs[sel] = gs;
		}
	}


	if (m_gs_cb_cache.Update(cb))
	{
		ID3D11DeviceContext* ctx = m_ctx;

		ctx->UpdateSubresource(m_gs_cb, 0, NULL, cb, 0, 0);
	}

	GSSetShader(gs, m_gs_cb);
}

void GSDevice11::SetupPS(PSSelector sel, const PSConstantBuffer* cb, PSSamplerSelector ssel)
{
	auto i = std::as_const(m_ps).find(sel);

	if (i == m_ps.end())
	{
		ShaderMacro sm(m_shader.model);

		sm.AddMacro("PS_SCALE_FACTOR", std::max(1, m_upscale_multiplier));
		sm.AddMacro("PS_FST", sel.fst);
		sm.AddMacro("PS_WMS", sel.wms);
		sm.AddMacro("PS_WMT", sel.wmt);
		sm.AddMacro("PS_FMT", sel.fmt);
		sm.AddMacro("PS_AEM", sel.aem);
		sm.AddMacro("PS_TFX", sel.tfx);
		sm.AddMacro("PS_TCC", sel.tcc);
		sm.AddMacro("PS_ATST", sel.atst);
		sm.AddMacro("PS_FOG", sel.fog);
		sm.AddMacro("PS_CLR1", sel.clr1);
		sm.AddMacro("PS_FBA", sel.fba);
		sm.AddMacro("PS_FBMASK", sel.fbmask);
		sm.AddMacro("PS_LTF", sel.ltf);
		sm.AddMacro("PS_TCOFFSETHACK", sel.tcoffsethack);
		sm.AddMacro("PS_POINT_SAMPLER", sel.point_sampler);
		sm.AddMacro("PS_SHUFFLE", sel.shuffle);
		sm.AddMacro("PS_READ_BA", sel.read_ba);
		sm.AddMacro("PS_CHANNEL_FETCH", sel.channel);
		sm.AddMacro("PS_TALES_OF_ABYSS_HLE", sel.tales_of_abyss_hle);
		sm.AddMacro("PS_URBAN_CHAOS_HLE", sel.urban_chaos_hle);
		sm.AddMacro("PS_DFMT", sel.dfmt);
		sm.AddMacro("PS_DEPTH_FMT", sel.depth_fmt);
		sm.AddMacro("PS_PAL_FMT", sel.fmt >> 2);
		sm.AddMacro("PS_INVALID_TEX0", sel.invalid_tex0);
		sm.AddMacro("PS_HDR", sel.hdr);
		sm.AddMacro("PS_COLCLIP", sel.colclip);
		sm.AddMacro("PS_BLEND_A", sel.blend_a);
		sm.AddMacro("PS_BLEND_B", sel.blend_b);
		sm.AddMacro("PS_BLEND_C", sel.blend_c);
		sm.AddMacro("PS_BLEND_D", sel.blend_d);
		sm.AddMacro("PS_PABE", sel.pabe);
		sm.AddMacro("PS_DITHER", sel.dither);
		sm.AddMacro("PS_ZCLAMP", sel.zclamp);

		CComPtr<ID3D11PixelShader> ps;

		std::vector<char> shader;
		theApp.LoadResource(IDR_TFX_FX, shader);
		CreateShader(shader, "tfx.fx", nullptr, "ps_main", sm.GetPtr(), &ps);

		m_ps[sel] = ps;

		i = m_ps.find(sel);
	}

	if (m_ps_cb_cache.Update(cb))
	{
		ID3D11DeviceContext* ctx = m_ctx;

		ctx->UpdateSubresource(m_ps_cb, 0, NULL, cb, 0, 0);
	}

	CComPtr<ID3D11SamplerState> ss0, ss1;

	if (sel.tfx != 4)
	{
		if (!(sel.fmt < 3 && sel.wms < 3 && sel.wmt < 3))
		{
			ssel.ltf = 0;
		}

		auto i = std::as_const(m_ps_ss).find(ssel);

		if (i != m_ps_ss.end())
		{
			ss0 = i->second;
		}
		else
		{
			D3D11_SAMPLER_DESC sd, af;

			memset(&sd, 0, sizeof(sd));

			af.Filter = m_aniso_filter ? D3D11_FILTER_ANISOTROPIC : D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			sd.Filter = ssel.ltf ? af.Filter : D3D11_FILTER_MIN_MAG_MIP_POINT;

			sd.AddressU = ssel.tau ? D3D11_TEXTURE_ADDRESS_WRAP : D3D11_TEXTURE_ADDRESS_CLAMP;
			sd.AddressV = ssel.tav ? D3D11_TEXTURE_ADDRESS_WRAP : D3D11_TEXTURE_ADDRESS_CLAMP;
			sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			sd.MinLOD = -FLT_MAX;
			sd.MaxLOD = FLT_MAX;
			sd.MaxAnisotropy = m_aniso_filter;
			sd.ComparisonFunc = D3D11_COMPARISON_NEVER;

			m_dev->CreateSamplerState(&sd, &ss0);

			m_ps_ss[ssel] = ss0;
		}

		if (sel.fmt >= 3)
		{
			ss1 = m_palette_ss;
		}
	}

	PSSetSamplerState(ss0, ss1);

	PSSetShader(i->second, m_ps_cb);
}

void GSDevice11::SetupOM(OMDepthStencilSelector dssel, OMBlendSelector bsel, uint8 afix)
{
	auto i = std::as_const(m_om_dss).find(dssel);

	if (i == m_om_dss.end())
	{
		D3D11_DEPTH_STENCIL_DESC dsd;

		memset(&dsd, 0, sizeof(dsd));

		if (dssel.date)
		{
			dsd.StencilEnable = true;
			dsd.StencilReadMask = 1;
			dsd.StencilWriteMask = 1;
			dsd.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
			dsd.FrontFace.StencilPassOp = dssel.date_one ? D3D11_STENCIL_OP_ZERO : D3D11_STENCIL_OP_KEEP;
			dsd.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
			dsd.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
			dsd.BackFace.StencilFunc = D3D11_COMPARISON_EQUAL;
			dsd.BackFace.StencilPassOp = dssel.date_one ? D3D11_STENCIL_OP_ZERO : D3D11_STENCIL_OP_KEEP;
			dsd.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
			dsd.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		}

		if (dssel.ztst != ZTST_ALWAYS || dssel.zwe)
		{
			static const D3D11_COMPARISON_FUNC ztst[] =
			{
				D3D11_COMPARISON_NEVER,
				D3D11_COMPARISON_ALWAYS,
				D3D11_COMPARISON_GREATER_EQUAL,
				D3D11_COMPARISON_GREATER
			};

			dsd.DepthEnable = true;
			dsd.DepthWriteMask = dssel.zwe ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
			dsd.DepthFunc = ztst[dssel.ztst];
		}

		CComPtr<ID3D11DepthStencilState> dss;

		m_dev->CreateDepthStencilState(&dsd, &dss);

		m_om_dss[dssel] = dss;

		i = m_om_dss.find(dssel);
	}

	OMSetDepthStencilState(i->second, 1);

	auto j = std::as_const(m_om_bs).find(bsel);

	if (j == m_om_bs.end())
	{
		D3D11_BLEND_DESC bd;

		memset(&bd, 0, sizeof(bd));

		bd.RenderTarget[0].BlendEnable = bsel.abe;

		if (bsel.abe)
		{
			HWBlend blend = GetBlend(bsel.blend_index);
			bd.RenderTarget[0].BlendOp = (D3D11_BLEND_OP)blend.op;
			bd.RenderTarget[0].SrcBlend = (D3D11_BLEND)blend.src;
			bd.RenderTarget[0].DestBlend = (D3D11_BLEND)blend.dst;
			bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;

			if (bsel.accu_blend)
			{
				bd.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
				bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
			}
		}

		if (bsel.wr) bd.RenderTarget[0].RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_RED;
		if (bsel.wg) bd.RenderTarget[0].RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_GREEN;
		if (bsel.wb) bd.RenderTarget[0].RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_BLUE;
		if (bsel.wa) bd.RenderTarget[0].RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_ALPHA;

		CComPtr<ID3D11BlendState> bs;

		m_dev->CreateBlendState(&bd, &bs);

		m_om_bs[bsel] = bs;

		j = m_om_bs.find(bsel);
	}

	OMSetBlendState(j->second, (float)(int)afix / 0x80);
}
