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

	bd.ByteWidth = sizeof(GSHWDrawConfig::VSConstantBuffer);
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	hr = m_dev->CreateBuffer(&bd, nullptr, m_vs_cb.put());

	if (FAILED(hr))
		return false;

	memset(&bd, 0, sizeof(bd));

	bd.ByteWidth = sizeof(GSHWDrawConfig::PSConstantBuffer);
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	hr = m_dev->CreateBuffer(&bd, nullptr, m_ps_cb.put());

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
	sd.MaxAnisotropy = 1;
	sd.ComparisonFunc = D3D11_COMPARISON_NEVER;

	hr = m_dev->CreateSamplerState(&sd, m_palette_ss.put());

	if (FAILED(hr))
		return false;

	// create layout

	VSSelector sel;
	GSHWDrawConfig::VSConstantBuffer cb;

	SetupVS(sel, &cb);

	SetupGS(GSSelector(1));

	//

	return true;
}

void GSDevice11::SetupVS(VSSelector sel, const GSHWDrawConfig::VSConstantBuffer* cb)
{
	auto i = std::as_const(m_vs).find(sel.key);

	if (i == m_vs.end())
	{
		ShaderMacro sm(m_shader_cache.GetFeatureLevel());

		sm.AddMacro("VS_TME", sel.tme);
		sm.AddMacro("VS_FST", sel.fst);
		sm.AddMacro("VS_IIP", sel.iip);

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
		m_shader_cache.GetVertexShaderAndInputLayout(m_dev.get(),
			vs.vs.put(), vs.il.put(), layout, std::size(layout), m_tfx_source, sm.GetPtr(), "vs_main");
		i = m_vs.try_emplace(sel.key, std::move(vs)).first;
	}

	if (m_vs_cb_cache.Update(*cb))
	{
		m_ctx->UpdateSubresource(m_vs_cb.get(), 0, NULL, cb, 0, 0);
	}

	VSSetShader(i->second.vs.get(), m_vs_cb.get());

	IASetInputLayout(i->second.il.get());
}

void GSDevice11::SetupGS(GSSelector sel)
{
	wil::com_ptr_nothrow<ID3D11GeometryShader> gs;

	// Geometry shader is disabled if sprite conversion is done on the cpu (sel.cpu_sprite).
	if (sel.expand)
	{
		const auto i = std::as_const(m_gs).find(sel.key);

		if (i != m_gs.end())
		{
			gs = i->second;
		}
		else
		{
			ShaderMacro sm(m_shader_cache.GetFeatureLevel());

			sm.AddMacro("GS_IIP", sel.iip);
			sm.AddMacro("GS_PRIM", static_cast<int>(sel.topology));
			sm.AddMacro("GS_EXPAND", sel.expand);

			gs = m_shader_cache.GetGeometryShader(m_dev.get(), m_tfx_source, sm.GetPtr(), "gs_main");

			m_gs[sel.key] = gs;
		}
	}

	GSSetShader(gs.get(), m_vs_cb.get());
}

void GSDevice11::SetupPS(const PSSelector& sel, const GSHWDrawConfig::PSConstantBuffer* cb, PSSamplerSelector ssel)
{
	auto i = std::as_const(m_ps).find(sel);

	if (i == m_ps.end())
	{
		ShaderMacro sm(m_shader_cache.GetFeatureLevel());

		sm.AddMacro("PS_SCALE_FACTOR", GSConfig.UpscaleMultiplier);
		sm.AddMacro("PS_FST", sel.fst);
		sm.AddMacro("PS_WMS", sel.wms);
		sm.AddMacro("PS_WMT", sel.wmt);
		sm.AddMacro("PS_AEM_FMT", sel.aem_fmt);
		sm.AddMacro("PS_AEM", sel.aem);
		sm.AddMacro("PS_TFX", sel.tfx);
		sm.AddMacro("PS_TCC", sel.tcc);
		sm.AddMacro("PS_ATST", sel.atst);
		sm.AddMacro("PS_FOG", sel.fog);
		sm.AddMacro("PS_IIP", sel.iip);
		sm.AddMacro("PS_CLR_HW", sel.clr_hw);
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
		sm.AddMacro("PS_PAL_FMT", sel.pal_fmt);
		sm.AddMacro("PS_INVALID_TEX0", sel.invalid_tex0);
		sm.AddMacro("PS_HDR", sel.hdr);
		sm.AddMacro("PS_COLCLIP", sel.colclip);
		sm.AddMacro("PS_BLEND_A", sel.blend_a);
		sm.AddMacro("PS_BLEND_B", sel.blend_b);
		sm.AddMacro("PS_BLEND_C", sel.blend_c);
		sm.AddMacro("PS_BLEND_D", sel.blend_d);
		sm.AddMacro("PS_BLEND_MIX", sel.blend_mix);
		sm.AddMacro("PS_PABE", sel.pabe);
		sm.AddMacro("PS_DITHER", sel.dither);
		sm.AddMacro("PS_ZCLAMP", sel.zclamp);
		sm.AddMacro("PS_SCANMSK", sel.scanmsk);
		sm.AddMacro("PS_AUTOMATIC_LOD", sel.automatic_lod);
		sm.AddMacro("PS_MANUAL_LOD", sel.manual_lod);
		sm.AddMacro("PS_TEX_IS_FB", sel.tex_is_fb);
		sm.AddMacro("PS_NO_COLOR", sel.no_color);
		sm.AddMacro("PS_NO_COLOR1", sel.no_color1);
		sm.AddMacro("PS_NO_ABLEND", sel.no_ablend);
		sm.AddMacro("PS_ONLY_ALPHA", sel.only_alpha);

		wil::com_ptr_nothrow<ID3D11PixelShader> ps = m_shader_cache.GetPixelShader(m_dev.get(), m_tfx_source, sm.GetPtr(), "ps_main");
		i = m_ps.try_emplace(sel, std::move(ps)).first;
	}

	if (cb && m_ps_cb_cache.Update(*cb))
	{
		m_ctx->UpdateSubresource(m_ps_cb.get(), 0, NULL, cb, 0, 0);
	}

	wil::com_ptr_nothrow<ID3D11SamplerState> ss0, ss1;

	if (sel.tfx != 4)
	{
		if (sel.pal_fmt || sel.wms >= 3 || sel.wmt >= 3)
		{
			ASSERT(ssel.biln == 0);
		}

		auto i = std::as_const(m_ps_ss).find(ssel.key);

		if (i != m_ps_ss.end())
		{
			ss0 = i->second;
		}
		else
		{
			D3D11_SAMPLER_DESC sd = {};

			const int anisotropy = GSConfig.MaxAnisotropy;
			if (anisotropy && ssel.aniso)
			{
				sd.Filter = D3D11_FILTER_ANISOTROPIC;
			}
			else
			{
				static constexpr std::array<D3D11_FILTER, 8> filters = {{
					D3D11_FILTER_MIN_MAG_MIP_POINT, // 000 / min=point,mag=point,mip=point
					D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT, // 001 / min=linear,mag=point,mip=point
					D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT, // 010 / min=point,mag=linear,mip=point
					D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT, // 011 / min=linear,mag=linear,mip=point
					D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR, // 100 / min=point,mag=point,mip=linear
					D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR, // 101 / min=linear,mag=point,mip=linear
					D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR, // 110 / min=point,mag=linear,mip=linear
					D3D11_FILTER_MIN_MAG_MIP_LINEAR, // 111 / min=linear,mag=linear,mip=linear
				}};

				const u8 index = (static_cast<u8>(ssel.IsMipFilterLinear()) << 2) |
								 (static_cast<u8>(ssel.IsMagFilterLinear()) << 1) |
								 static_cast<u8>(ssel.IsMinFilterLinear());
				sd.Filter = filters[index];
			}

			sd.AddressU = ssel.tau ? D3D11_TEXTURE_ADDRESS_WRAP : D3D11_TEXTURE_ADDRESS_CLAMP;
			sd.AddressV = ssel.tav ? D3D11_TEXTURE_ADDRESS_WRAP : D3D11_TEXTURE_ADDRESS_CLAMP;
			sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			sd.MinLOD = 0.0f;
			sd.MaxLOD = ssel.lodclamp ? 0.0f : FLT_MAX;
			sd.MaxAnisotropy = std::clamp(anisotropy, 1, 16);
			sd.ComparisonFunc = D3D11_COMPARISON_NEVER;

			m_dev->CreateSamplerState(&sd, &ss0);

			m_ps_ss[ssel.key] = ss0;
		}

		if (sel.pal_fmt)
		{
			ss1 = m_palette_ss;
		}
	}

	PSSetSamplerState(ss0.get(), ss1.get());

	PSSetShader(i->second.get(), m_ps_cb.get());
}

void GSDevice11::ClearSamplerCache()
{
	m_ps_ss.clear();
}

// clang-format off
static constexpr std::array<D3D11_BLEND, 16> s_d3d11_blend_factors = { {
	D3D11_BLEND_SRC_COLOR, D3D11_BLEND_INV_SRC_COLOR, D3D11_BLEND_DEST_COLOR, D3D11_BLEND_INV_DEST_COLOR,
	D3D11_BLEND_SRC1_COLOR, D3D11_BLEND_INV_SRC1_COLOR, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA,
	D3D11_BLEND_DEST_ALPHA, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_SRC1_ALPHA, D3D11_BLEND_INV_SRC1_ALPHA,
	D3D11_BLEND_BLEND_FACTOR, D3D11_BLEND_INV_BLEND_FACTOR, D3D11_BLEND_ONE, D3D11_BLEND_ZERO
} };
static constexpr std::array<D3D11_BLEND_OP, 3> s_d3d11_blend_ops = { {
	D3D11_BLEND_OP_ADD, D3D11_BLEND_OP_SUBTRACT, D3D11_BLEND_OP_REV_SUBTRACT
} };
// clang-format on

void GSDevice11::SetupOM(OMDepthStencilSelector dssel, OMBlendSelector bsel, u8 afix)
{
	auto i = std::as_const(m_om_dss).find(dssel.key);

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

		wil::com_ptr_nothrow<ID3D11DepthStencilState> dss;
		m_dev->CreateDepthStencilState(&dsd, dss.put());

		i = m_om_dss.try_emplace(dssel.key, std::move(dss)).first;
	}

	OMSetDepthStencilState(i->second.get(), 1);

	auto j = std::as_const(m_om_bs).find(bsel);

	if (j == m_om_bs.end())
	{
		D3D11_BLEND_DESC bd;

		memset(&bd, 0, sizeof(bd));

		if (bsel.blend_enable)
		{
			bd.RenderTarget[0].BlendEnable = TRUE;
			bd.RenderTarget[0].BlendOp = s_d3d11_blend_ops[bsel.blend_op];
			bd.RenderTarget[0].SrcBlend = s_d3d11_blend_factors[bsel.blend_src_factor];
			bd.RenderTarget[0].DestBlend = s_d3d11_blend_factors[bsel.blend_dst_factor];
			bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		}

		if (bsel.wr) bd.RenderTarget[0].RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_RED;
		if (bsel.wg) bd.RenderTarget[0].RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_GREEN;
		if (bsel.wb) bd.RenderTarget[0].RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_BLUE;
		if (bsel.wa) bd.RenderTarget[0].RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_ALPHA;

		wil::com_ptr_nothrow<ID3D11BlendState> bs;
		m_dev->CreateBlendState(&bd, bs.put());

		j = m_om_bs.try_emplace(bsel, std::move(bs)).first;
	}

	OMSetBlendState(j->second.get(), (float)(int)afix / 0x80);
}
