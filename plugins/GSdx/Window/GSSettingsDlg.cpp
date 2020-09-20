/*
 *	Copyright (C) 2007-2015 Gabest
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
#include "GSdx.h"
#include "GSSettingsDlg.h"
#include "GSUtil.h"
#include "Renderers/DX11/GSDevice11.h"
#include "resource.h"
#include "GSSetting.h"
#include <algorithm>

GSSettingsDlg::GSSettingsDlg()
	: GSDialog(IDD_CONFIG)
	, m_renderers{theApp.m_gs_renderers}
	, m_d3d11_adapters{EnumerateD3D11Adapters()}
	, m_current_adapters{nullptr}
	, m_last_selected_adapter_id{theApp.GetConfigS("Adapter")}
{
	if (m_d3d11_adapters.empty())
	{
		auto is_d3d11_renderer = [](const auto &renderer) {
			const GSRendererType type = static_cast<GSRendererType>(renderer.value);
			return type == GSRendererType::DX1011_HW || type == GSRendererType::DX1011_SW;
		};
		m_renderers.erase(std::remove_if(m_renderers.begin(), m_renderers.end(), is_d3d11_renderer), m_renderers.end());
	}
}

std::vector<GSSettingsDlg::Adapter> GSSettingsDlg::EnumerateD3D11Adapters()
{
	CComPtr<IDXGIFactory1> dxgi_factory;
	CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory));
	if (dxgi_factory == nullptr)
		return {};

	std::vector<Adapter> adapters {
		{"Default Hardware Device", "default", GSUtil::CheckDirect3D11Level(nullptr, D3D_DRIVER_TYPE_HARDWARE)},
#ifdef _DEBUG
		{"Reference Device", "ref", GSUtil::CheckDirect3D11Level(nullptr, D3D_DRIVER_TYPE_REFERENCE)},
#endif
	};

	CComPtr<IDXGIAdapter1> adapter;
	for (int i = 0; dxgi_factory->EnumAdapters1(i, &adapter) == S_OK; ++i, adapter = nullptr)
	{
		DXGI_ADAPTER_DESC1 desc;
		if (adapter->GetDesc1(&desc) != S_OK)
			continue;

		D3D_FEATURE_LEVEL level = GSUtil::CheckDirect3D11Level(adapter, D3D_DRIVER_TYPE_UNKNOWN);

		const int size = WideCharToMultiByte(CP_ACP, 0, desc.Description, sizeof(desc.Description), nullptr, 0, nullptr, nullptr);
		std::vector<char> buf(size);
		WideCharToMultiByte(CP_ACP, 0, desc.Description, sizeof(desc.Description), buf.data(), size, nullptr, nullptr);
		adapters.push_back({buf.data(), GSAdapter(desc), level});
	}

	auto unsupported_adapter = [](const auto &adapter) { return adapter.level < D3D_FEATURE_LEVEL_10_0; };
	adapters.erase(std::remove_if(adapters.begin(), adapters.end(), unsupported_adapter), adapters.end());

	return adapters;
}

void GSSettingsDlg::OnInit()
{
	__super::OnInit();

	GSRendererType renderer = GSRendererType(theApp.GetConfigI("Renderer"));
	const bool dx11 = renderer == GSRendererType::DX1011_HW || renderer == GSRendererType::DX1011_SW;
	if (renderer == GSRendererType::Undefined || m_d3d11_adapters.empty() && dx11)
		renderer = GSUtil::GetBestRenderer();
	ComboBoxInit(IDC_RENDERER, m_renderers, static_cast<int32_t>(renderer));
	UpdateAdapters();

	ComboBoxInit(IDC_MIPMAP_HW, theApp.m_gs_hw_mipmapping, theApp.GetConfigI("mipmap_hw"));

	ComboBoxInit(IDC_INTERLACE, theApp.m_gs_interlace, theApp.GetConfigI("interlace"));
	ComboBoxInit(IDC_UPSCALE_MULTIPLIER, theApp.m_gs_upscale_multiplier, theApp.GetConfigI("upscale_multiplier"));
	ComboBoxInit(IDC_AFCOMBO, theApp.m_gs_max_anisotropy, theApp.GetConfigI("MaxAnisotropy"));
	ComboBoxInit(IDC_FILTER, theApp.m_gs_bifilter, theApp.GetConfigI("filter"));
	ComboBoxInit(IDC_ACCURATE_DATE, theApp.m_gs_acc_date_level, theApp.GetConfigI("accurate_date"));
	ComboBoxInit(IDC_ACCURATE_BLEND_UNIT, theApp.m_gs_acc_blend_level, theApp.GetConfigI("accurate_blending_unit"));
	ComboBoxInit(IDC_ACCURATE_BLEND_UNIT_D3D11, theApp.m_gs_acc_blend_level_d3d11, theApp.GetConfigI("accurate_blending_unit_d3d11"));
	ComboBoxInit(IDC_CRC_LEVEL, theApp.m_gs_crc_level, theApp.GetConfigI("crc_hack_level"));
	ComboBoxInit(IDC_DITHERING, theApp.m_gs_dithering, theApp.GetConfigI("dithering_ps2"));

	CheckDlgButton(m_hWnd, IDC_PALTEX, theApp.GetConfigB("paltex"));
	CheckDlgButton(m_hWnd, IDC_LARGE_FB, theApp.GetConfigB("large_framebuffer"));
	CheckDlgButton(m_hWnd, IDC_MIPMAP_SW, theApp.GetConfigB("mipmap"));
	CheckDlgButton(m_hWnd, IDC_AA1, theApp.GetConfigB("aa1"));
	CheckDlgButton(m_hWnd, IDC_AUTO_FLUSH_SW, theApp.GetConfigB("autoflush_sw"));

	// Hacks
	CheckDlgButton(m_hWnd, IDC_HACKS_ENABLED, theApp.GetConfigB("UserHacks"));

	SendMessage(GetDlgItem(m_hWnd, IDC_SWTHREADS), UDM_SETRANGE, 0, MAKELPARAM(16, 0));
	SendMessage(GetDlgItem(m_hWnd, IDC_SWTHREADS), UDM_SETPOS, 0, MAKELPARAM(theApp.GetConfigI("extrathreads"), 0));

	AddTooltip(IDC_FILTER);
	AddTooltip(IDC_CRC_LEVEL);
	AddTooltip(IDC_PALTEX);
	AddTooltip(IDC_ACCURATE_DATE);
	AddTooltip(IDC_ACCURATE_BLEND_UNIT);
	AddTooltip(IDC_ACCURATE_BLEND_UNIT_D3D11);
	AddTooltip(IDC_AFCOMBO);
	AddTooltip(IDC_AA1);
	AddTooltip(IDC_MIPMAP_HW);
	AddTooltip(IDC_MIPMAP_SW);
	AddTooltip(IDC_SWTHREADS);
	AddTooltip(IDC_SWTHREADS_EDIT);
	AddTooltip(IDC_AUTO_FLUSH_SW);
	AddTooltip(IDC_LARGE_FB);

	UpdateControls();
}

bool GSSettingsDlg::OnCommand(HWND hWnd, UINT id, UINT code)
{
	switch (id)
	{
		case IDC_ADAPTER:
			if (code == CBN_SELCHANGE)
			{
				INT_PTR data;
				if (ComboBoxGetSelData(IDC_ADAPTER, data))
					m_last_selected_adapter_id = (*m_current_adapters)[data].id;
			}
			break;
		case IDC_RENDERER:
			if (code == CBN_SELCHANGE)
			{
				UpdateAdapters();
				UpdateControls();
			}
			break;
		case IDC_UPSCALE_MULTIPLIER:
		case IDC_FILTER:
			if (code == CBN_SELCHANGE)
				UpdateControls();
			break;
		case IDC_PALTEX:
		case IDC_HACKS_ENABLED:
			if (code == BN_CLICKED)
				UpdateControls();
			break;
		case IDC_SHADEBUTTON:
			if (code == BN_CLICKED)
				GSShaderDlg().DoModal();
			break;
		case IDC_OSDBUTTON:
			if (code == BN_CLICKED)
				GSOSDDlg().DoModal();
			break;
		case IDC_HACKSBUTTON:
			if (code == BN_CLICKED)
			{
				INT_PTR data;
				std::string adapter_id;
				if (ComboBoxGetSelData(IDC_ADAPTER, data))
					adapter_id = (*m_current_adapters)[data].id;
				GSHacksDlg().DoModal();
			}
			break;
		case IDC_SWTHREADS_EDIT:
			if (code == EN_CHANGE)
				UpdateControls();
			break;
		case IDOK:
		{
			INT_PTR data;

			if(ComboBoxGetSelData(IDC_ADAPTER, data))
			{
				theApp.SetConfig("Adapter", (*m_current_adapters)[data].id.c_str());
			}

			if(ComboBoxGetSelData(IDC_RENDERER, data))
			{
				theApp.SetConfig("Renderer", (int)data);
			}

			if(ComboBoxGetSelData(IDC_INTERLACE, data))
			{
				theApp.SetConfig("interlace", (int)data);
			}

			if (ComboBoxGetSelData(IDC_MIPMAP_HW, data))
			{
				theApp.SetConfig("mipmap_hw", (int)data);
			}

			if(ComboBoxGetSelData(IDC_UPSCALE_MULTIPLIER, data))
			{
				theApp.SetConfig("upscale_multiplier", (int)data);
			}
			else
			{
				theApp.SetConfig("upscale_multiplier", 1);
			}

			if (ComboBoxGetSelData(IDC_FILTER, data))
			{
				theApp.SetConfig("filter", (int)data);
			}

			if(ComboBoxGetSelData(IDC_ACCURATE_DATE, data))
			{
				theApp.SetConfig("accurate_date", (int)data);
			}

			if(ComboBoxGetSelData(IDC_ACCURATE_BLEND_UNIT, data))
			{
				theApp.SetConfig("accurate_blending_unit", (int)data);
			}

			if(ComboBoxGetSelData(IDC_ACCURATE_BLEND_UNIT_D3D11, data))
			{
				theApp.SetConfig("accurate_blending_unit_d3d11", (int)data);
			}

			if (ComboBoxGetSelData(IDC_CRC_LEVEL, data))
			{
				theApp.SetConfig("crc_hack_level", (int)data);
			}

			if(ComboBoxGetSelData(IDC_AFCOMBO, data))
			{
				theApp.SetConfig("MaxAnisotropy", (int)data);
			}

			if (ComboBoxGetSelData(IDC_DITHERING, data))
			{
				theApp.SetConfig("dithering_ps2", (int)data);
			}

			theApp.SetConfig("mipmap", (int)IsDlgButtonChecked(m_hWnd, IDC_MIPMAP_SW));
			theApp.SetConfig("paltex", (int)IsDlgButtonChecked(m_hWnd, IDC_PALTEX));
			theApp.SetConfig("large_framebuffer", (int)IsDlgButtonChecked(m_hWnd, IDC_LARGE_FB));
			theApp.SetConfig("aa1", (int)IsDlgButtonChecked(m_hWnd, IDC_AA1));
			theApp.SetConfig("autoflush_sw", (int)IsDlgButtonChecked(m_hWnd, IDC_AUTO_FLUSH_SW));
			theApp.SetConfig("UserHacks", (int)IsDlgButtonChecked(m_hWnd, IDC_HACKS_ENABLED));

			// The LOWORD returned by UDM_GETPOS automatically restricts the value to its input range.
			theApp.SetConfig("extrathreads", LOWORD(SendMessage(GetDlgItem(m_hWnd, IDC_SWTHREADS), UDM_GETPOS, 0, 0)));
		}
		break;
	}

	return __super::OnCommand(hWnd, id, code);
}

void GSSettingsDlg::UpdateAdapters()
{
	INT_PTR data;
	if (!ComboBoxGetSelData(IDC_RENDERER, data))
		return;

	const GSRendererType renderer = static_cast<GSRendererType>(data);
	const bool dx11 = renderer == GSRendererType::DX1011_HW || renderer == GSRendererType::DX1011_SW;

	EnableWindow(GetDlgItem(m_hWnd, IDC_ADAPTER), dx11);
	EnableWindow(GetDlgItem(m_hWnd, IDC_ADAPTER_TEXT), dx11);

	if (!dx11)
	{
		SendMessage(GetDlgItem(m_hWnd, IDC_ADAPTER), CB_RESETCONTENT, 0, 0);
		return;
	}

	m_current_adapters = &m_d3d11_adapters;

	std::vector<GSSetting> adapter_settings;
	unsigned int adapter_sel = 0;
	for (unsigned int i = 0; i < m_current_adapters->size(); i++)
	{
		if ((*m_current_adapters)[i].id == m_last_selected_adapter_id)
			adapter_sel = i;

		adapter_settings.push_back(GSSetting{i, (*m_current_adapters)[i].name.c_str(), ""});
	}

	ComboBoxInit(IDC_ADAPTER, adapter_settings, adapter_sel);
}

void GSSettingsDlg::UpdateControls()
{
	INT_PTR i;

	int integer_scaling = 0; // in case reading the combo doesn't work, enable the custom res control anyway

	if(ComboBoxGetSelData(IDC_UPSCALE_MULTIPLIER, i))
	{
		integer_scaling = (int)i;
	}

	if(ComboBoxGetSelData(IDC_RENDERER, i))
	{
		const GSRendererType renderer = static_cast<GSRendererType>(i);

		const bool dx11 = renderer == GSRendererType::DX1011_HW || renderer == GSRendererType::DX1011_SW;
		const bool ogl = renderer == GSRendererType::OGL_HW || renderer == GSRendererType::OGL_SW;

		const bool hw =  renderer == GSRendererType::DX1011_HW || renderer == GSRendererType::OGL_HW;
		const bool sw =  renderer == GSRendererType::DX1011_SW || renderer == GSRendererType::OGL_SW;
		const bool null = renderer == GSRendererType::Null;

		const int sw_threads = SendMessage(GetDlgItem(m_hWnd, IDC_SWTHREADS), UDM_GETPOS, 0, 0);
		SendMessage(GetDlgItem(m_hWnd, IDC_SWTHREADS), UDM_SETPOS, 0, MAKELPARAM(sw_threads, 0));

		ShowWindow(GetDlgItem(m_hWnd, IDC_LOGO11), dx11 ? SW_SHOW : SW_HIDE);
		ShowWindow(GetDlgItem(m_hWnd, IDC_NULL), null ? SW_SHOW : SW_HIDE);
		ShowWindow(GetDlgItem(m_hWnd, IDC_LOGOGL), ogl ? SW_SHOW : SW_HIDE);

		EnableWindow(GetDlgItem(m_hWnd, IDC_INTERLACE), !null);
		EnableWindow(GetDlgItem(m_hWnd, IDC_INTERLACE_TEXT), !null);
		EnableWindow(GetDlgItem(m_hWnd, IDC_FILTER), !null);
		EnableWindow(GetDlgItem(m_hWnd, IDC_FILTER_TEXT), !null);

		EnableWindow(GetDlgItem(m_hWnd, IDC_MIPMAP_HW), hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_MIPMAP_HW_TEXT), hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_CRC_LEVEL), hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_LARGE_FB), integer_scaling > 1 && hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_CRC_LEVEL_TEXT), hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_UPSCALE_MULTIPLIER), hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_UPSCALE_MULTIPLIER_TEXT), hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_PALTEX), hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_DITHERING), hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_DITHERING_TEXT), hw);

		INT_PTR filter;
		if (ComboBoxGetSelData(IDC_FILTER, filter))
		{
			EnableWindow(GetDlgItem(m_hWnd, IDC_AFCOMBO), hw && filter && (ogl || !IsDlgButtonChecked(m_hWnd, IDC_PALTEX)));
			EnableWindow(GetDlgItem(m_hWnd, IDC_AFCOMBO_TEXT), hw && filter && (ogl || !IsDlgButtonChecked(m_hWnd, IDC_PALTEX)));
		}
		EnableWindow(GetDlgItem(m_hWnd, IDC_ACCURATE_DATE), hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_ACCURATE_DATE_TEXT), hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_ACCURATE_BLEND_UNIT), hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_ACCURATE_BLEND_UNIT_D3D11), hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_ACCURATE_BLEND_UNIT_TEXT), hw);
		ShowWindow(GetDlgItem(m_hWnd, IDC_ACCURATE_BLEND_UNIT), ogl ? SW_SHOW : SW_HIDE);
		ShowWindow(GetDlgItem(m_hWnd, IDC_ACCURATE_BLEND_UNIT_D3D11), dx11 || null ? SW_SHOW : SW_HIDE);

		// Software mode settings
		EnableWindow(GetDlgItem(m_hWnd, IDC_MIPMAP_SW), sw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_AA1), sw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_SWTHREADS_TEXT), sw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_SWTHREADS_EDIT), sw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_SWTHREADS), sw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_AUTO_FLUSH_SW), sw);

		// Hacks
		EnableWindow(GetDlgItem(m_hWnd, IDC_HACKS_ENABLED), hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_HACKSBUTTON), hw && (ogl || IsDlgButtonChecked(m_hWnd, IDC_HACKS_ENABLED)));

		// OSD Configuration
		EnableWindow(GetDlgItem(m_hWnd, IDC_OSDBUTTON), !null);

		// Shader Configuration
		EnableWindow(GetDlgItem(m_hWnd, IDC_SHADEBUTTON), !null);
	}
}

// Shader Configuration Dialog

GSShaderDlg::GSShaderDlg() :
	GSDialog(IDD_SHADER)
{}

void GSShaderDlg::OnInit()
{
	//TV Shader
	ComboBoxInit(IDC_TVSHADER, theApp.m_gs_tv_shaders, theApp.GetConfigI("TVShader"));

	//Shade Boost
	CheckDlgButton(m_hWnd, IDC_SHADEBOOST, theApp.GetConfigB("ShadeBoost"));
	m_contrast = theApp.GetConfigI("ShadeBoost_Contrast");
	m_brightness = theApp.GetConfigI("ShadeBoost_Brightness");
	m_saturation = theApp.GetConfigI("ShadeBoost_Saturation");

	// External FX shader
	CheckDlgButton(m_hWnd, IDC_SHADER_FX, theApp.GetConfigB("shaderfx"));
	SendMessage(GetDlgItem(m_hWnd, IDC_SHADER_FX_EDIT), WM_SETTEXT, 0, (LPARAM)theApp.GetConfigS("shaderfx_glsl").c_str());
	SendMessage(GetDlgItem(m_hWnd, IDC_SHADER_FX_CONF_EDIT), WM_SETTEXT, 0, (LPARAM)theApp.GetConfigS("shaderfx_conf").c_str());

	// FXAA shader
	CheckDlgButton(m_hWnd, IDC_FXAA, theApp.GetConfigB("fxaa"));

	// Texture Filtering Of Display
	CheckDlgButton(m_hWnd, IDC_LINEAR_PRESENT, theApp.GetConfigB("linear_present"));

	AddTooltip(IDC_SHADEBOOST);
	AddTooltip(IDC_SHADER_FX);
	AddTooltip(IDC_FXAA);
	AddTooltip(IDC_LINEAR_PRESENT);

	UpdateControls();
}

void GSShaderDlg::UpdateControls()
{
	SendMessage(GetDlgItem(m_hWnd, IDC_SATURATION_SLIDER), TBM_SETRANGE, TRUE, MAKELONG(0, 100));
	SendMessage(GetDlgItem(m_hWnd, IDC_BRIGHTNESS_SLIDER), TBM_SETRANGE, TRUE, MAKELONG(0, 100));
	SendMessage(GetDlgItem(m_hWnd, IDC_CONTRAST_SLIDER), TBM_SETRANGE, TRUE, MAKELONG(0, 100));

	SendMessage(GetDlgItem(m_hWnd, IDC_SATURATION_SLIDER), TBM_SETPOS, TRUE, m_saturation);
	SendMessage(GetDlgItem(m_hWnd, IDC_BRIGHTNESS_SLIDER), TBM_SETPOS, TRUE, m_brightness);
	SendMessage(GetDlgItem(m_hWnd, IDC_CONTRAST_SLIDER), TBM_SETPOS, TRUE, m_contrast);

	char text[8] = {0};

	sprintf(text, "%d", m_saturation);
	SetDlgItemText(m_hWnd, IDC_SATURATION_VALUE, text);
	sprintf(text, "%d", m_brightness);
	SetDlgItemText(m_hWnd, IDC_BRIGHTNESS_VALUE, text);
	sprintf(text, "%d", m_contrast);
	SetDlgItemText(m_hWnd, IDC_CONTRAST_VALUE, text);

	// Shader Settings
	const bool external_shader_selected = IsDlgButtonChecked(m_hWnd, IDC_SHADER_FX) == BST_CHECKED;
	const bool shadeboost_selected = IsDlgButtonChecked(m_hWnd, IDC_SHADEBOOST) == BST_CHECKED;
	EnableWindow(GetDlgItem(m_hWnd, IDC_SATURATION_SLIDER), shadeboost_selected);
	EnableWindow(GetDlgItem(m_hWnd, IDC_BRIGHTNESS_SLIDER), shadeboost_selected);
	EnableWindow(GetDlgItem(m_hWnd, IDC_CONTRAST_SLIDER), shadeboost_selected);
	EnableWindow(GetDlgItem(m_hWnd, IDC_SATURATION_TEXT), shadeboost_selected);
	EnableWindow(GetDlgItem(m_hWnd, IDC_BRIGHTNESS_TEXT), shadeboost_selected);
	EnableWindow(GetDlgItem(m_hWnd, IDC_CONTRAST_TEXT), shadeboost_selected);
	EnableWindow(GetDlgItem(m_hWnd, IDC_SATURATION_VALUE), shadeboost_selected);
	EnableWindow(GetDlgItem(m_hWnd, IDC_BRIGHTNESS_VALUE), shadeboost_selected);
	EnableWindow(GetDlgItem(m_hWnd, IDC_CONTRAST_VALUE), shadeboost_selected);
	EnableWindow(GetDlgItem(m_hWnd, IDC_SHADER_FX_TEXT), external_shader_selected);
	EnableWindow(GetDlgItem(m_hWnd, IDC_SHADER_FX_EDIT), external_shader_selected);
	EnableWindow(GetDlgItem(m_hWnd, IDC_SHADER_FX_BUTTON), external_shader_selected);
	EnableWindow(GetDlgItem(m_hWnd, IDC_SHADER_FX_CONF_TEXT), external_shader_selected);
	EnableWindow(GetDlgItem(m_hWnd, IDC_SHADER_FX_CONF_EDIT), external_shader_selected);
	EnableWindow(GetDlgItem(m_hWnd, IDC_SHADER_FX_CONF_BUTTON), external_shader_selected);
}

bool GSShaderDlg::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
	{
	case WM_HSCROLL:
	{
		if((HWND)lParam == GetDlgItem(m_hWnd, IDC_SATURATION_SLIDER)) 
		{
			char text[8] = {0};

			m_saturation = SendMessage(GetDlgItem(m_hWnd, IDC_SATURATION_SLIDER),TBM_GETPOS,0,0);

			sprintf(text, "%d", m_saturation);
			SetDlgItemText(m_hWnd, IDC_SATURATION_VALUE, text);
		}
		else if((HWND)lParam == GetDlgItem(m_hWnd, IDC_BRIGHTNESS_SLIDER)) 
		{
			char text[8] = {0};

			m_brightness = SendMessage(GetDlgItem(m_hWnd, IDC_BRIGHTNESS_SLIDER),TBM_GETPOS,0,0);

			sprintf(text, "%d", m_brightness);
			SetDlgItemText(m_hWnd, IDC_BRIGHTNESS_VALUE, text);
		}
		else if((HWND)lParam == GetDlgItem(m_hWnd, IDC_CONTRAST_SLIDER)) 
		{
			char text[8] = {0};

			m_contrast = SendMessage(GetDlgItem(m_hWnd, IDC_CONTRAST_SLIDER),TBM_GETPOS,0,0);

			sprintf(text, "%d", m_contrast);
			SetDlgItemText(m_hWnd, IDC_CONTRAST_VALUE, text);
		}
	} break;

	case WM_COMMAND:
	{
		const int id = LOWORD(wParam);

		switch(id)
		{
		case IDOK:
		{
			INT_PTR data;
			//TV Shader
			if (ComboBoxGetSelData(IDC_TVSHADER, data))
			{
				theApp.SetConfig("TVShader", (int)data);
			}
			// Shade Boost
			theApp.SetConfig("ShadeBoost", (int)IsDlgButtonChecked(m_hWnd, IDC_SHADEBOOST));
			theApp.SetConfig("ShadeBoost_Contrast", m_contrast);
			theApp.SetConfig("ShadeBoost_Brightness", m_brightness);
			theApp.SetConfig("ShadeBoost_Saturation", m_saturation);

			// FXAA shader
			theApp.SetConfig("fxaa", (int)IsDlgButtonChecked(m_hWnd, IDC_FXAA));

			// Texture Filtering Of Display
			theApp.SetConfig("linear_present", (int)IsDlgButtonChecked(m_hWnd, IDC_LINEAR_PRESENT));

			// External FX Shader
			theApp.SetConfig("shaderfx", (int)IsDlgButtonChecked(m_hWnd, IDC_SHADER_FX));

			// External FX Shader(OpenGL)
			const int shader_fx_length = (int)SendMessage(GetDlgItem(m_hWnd, IDC_SHADER_FX_EDIT), WM_GETTEXTLENGTH, 0, 0);
			const int shader_fx_conf_length = (int)SendMessage(GetDlgItem(m_hWnd, IDC_SHADER_FX_CONF_EDIT), WM_GETTEXTLENGTH, 0, 0);
			const int length = std::max(shader_fx_length, shader_fx_conf_length) + 1;
			std::unique_ptr<char[]> buffer(new char[length]);

			SendMessage(GetDlgItem(m_hWnd, IDC_SHADER_FX_EDIT), WM_GETTEXT, (WPARAM)length, (LPARAM)buffer.get());
			theApp.SetConfig("shaderfx_glsl", buffer.get()); // Not really glsl only ;)
			SendMessage(GetDlgItem(m_hWnd, IDC_SHADER_FX_CONF_EDIT), WM_GETTEXT, (WPARAM)length, (LPARAM)buffer.get());
			theApp.SetConfig("shaderfx_conf", buffer.get());

			EndDialog(m_hWnd, id);
		} break;
		case IDC_SHADEBOOST:
			UpdateControls();
		case IDC_SHADER_FX:
			if (HIWORD(wParam) == BN_CLICKED)
				UpdateControls();
			break;
		case IDC_SHADER_FX_BUTTON:
			if (HIWORD(wParam) == BN_CLICKED)
				OpenFileDialog(IDC_SHADER_FX_EDIT, "Select External Shader");
			break;

		case IDC_SHADER_FX_CONF_BUTTON:
			if (HIWORD(wParam) == BN_CLICKED)
				OpenFileDialog(IDC_SHADER_FX_CONF_EDIT, "Select External Shader Config");
			break;

		case IDCANCEL:
		{
			EndDialog(m_hWnd, IDCANCEL);
		} break;
		}

	} break;

	case WM_CLOSE:EndDialog(m_hWnd, IDCANCEL); break;

	default: return false;
	}
	

	return true;
}

// Hacks Dialog

GSHacksDlg::GSHacksDlg()
	: GSDialog{IDD_HACKS}
	, m_old_skipdraw_offset{0}
	, m_old_skipdraw{0}
{
}

void GSHacksDlg::OnInit()
{
	HWND hwnd_renderer = GetDlgItem(GetParent(m_hWnd), IDC_RENDERER);
	HWND hwnd_upscaling = GetDlgItem(GetParent(m_hWnd), IDC_UPSCALE_MULTIPLIER);
	const GSRendererType renderer = static_cast<GSRendererType>(SendMessage(hwnd_renderer, CB_GETITEMDATA, SendMessage(hwnd_renderer, CB_GETCURSEL, 0, 0), 0));
	const unsigned short upscaling_multiplier = static_cast<unsigned short>(SendMessage(hwnd_upscaling, CB_GETITEMDATA, SendMessage(hwnd_upscaling, CB_GETCURSEL, 0, 0), 0));

	// It can only be accessed with a HW renderer, so this is sufficient.
	const bool hwhacks = IsDlgButtonChecked(GetParent(m_hWnd), IDC_HACKS_ENABLED) == BST_CHECKED;
	const bool ogl = renderer == GSRendererType::OGL_HW;
	const bool native = upscaling_multiplier == 1;

	CheckDlgButton(m_hWnd, IDC_WILDHACK, theApp.GetConfigB("UserHacks_WildHack"));
	CheckDlgButton(m_hWnd, IDC_PRELOAD_GS, theApp.GetConfigB("preload_frame_with_gs_data"));
	CheckDlgButton(m_hWnd, IDC_ALIGN_SPRITE, theApp.GetConfigB("UserHacks_align_sprite_X"));
	CheckDlgButton(m_hWnd, IDC_TC_DEPTH, theApp.GetConfigB("UserHacks_DisableDepthSupport"));
	CheckDlgButton(m_hWnd, IDC_CPU_FB_CONVERSION, theApp.GetConfigB("UserHacks_CPU_FB_Conversion"));
	CheckDlgButton(m_hWnd, IDC_FAST_TC_INV, theApp.GetConfigB("UserHacks_DisablePartialInvalidation"));
	CheckDlgButton(m_hWnd, IDC_AUTO_FLUSH_HW, theApp.GetConfigB("UserHacks_AutoFlush"));
	CheckDlgButton(m_hWnd, IDC_SAFE_FEATURES, theApp.GetConfigB("UserHacks_Disable_Safe_Features"));
	CheckDlgButton(m_hWnd, IDC_MEMORY_WRAPPING, theApp.GetConfigB("wrap_gs_mem"));
	CheckDlgButton(m_hWnd, IDC_MERGE_PP_SPRITE, theApp.GetConfigB("UserHacks_merge_pp_sprite"));

	ComboBoxInit(IDC_HALF_SCREEN_TS, theApp.m_gs_generic_list, theApp.GetConfigI("UserHacks_Half_Bottom_Override"));
	ComboBoxInit(IDC_TRI_FILTER, theApp.m_gs_trifilter, theApp.GetConfigI("UserHacks_TriFilter"));
	ComboBoxInit(IDC_OFFSETHACK, theApp.m_gs_offset_hack, theApp.GetConfigI("UserHacks_HalfPixelOffset"));
	ComboBoxInit(IDC_ROUND_SPRITE, theApp.m_gs_hack, theApp.GetConfigI("UserHacks_round_sprite_offset"));
	ComboBoxInit(IDC_GEOMETRY_SHADER_OVERRIDE, theApp.m_gs_generic_list, theApp.GetConfigI("override_geometry_shader"));
	ComboBoxInit(IDC_IMAGE_LOAD_STORE, theApp.m_gs_generic_list, theApp.GetConfigI("override_GL_ARB_shader_image_load_store"));
	ComboBoxInit(IDC_SPARSE_TEXTURE, theApp.m_gs_generic_list, theApp.GetConfigI("override_GL_ARB_sparse_texture"));

	SendMessage(GetDlgItem(m_hWnd, IDC_SKIPDRAWOFFSET), UDM_SETRANGE, 0, MAKELPARAM(10000, 0));
	SendMessage(GetDlgItem(m_hWnd, IDC_SKIPDRAWOFFSET), UDM_SETPOS, 0, MAKELPARAM(theApp.GetConfigI("UserHacks_SkipDraw_Offset"), 0));
	SendMessage(GetDlgItem(m_hWnd, IDC_SKIPDRAWHACK), UDM_SETRANGE, 0, MAKELPARAM(10000, 0));
	SendMessage(GetDlgItem(m_hWnd, IDC_SKIPDRAWHACK), UDM_SETPOS, 0, MAKELPARAM(theApp.GetConfigI("UserHacks_SkipDraw"), 0));

	SendMessage(GetDlgItem(m_hWnd, IDC_TCOFFSETX), UDM_SETRANGE, 0, MAKELPARAM(10000, 0));
	SendMessage(GetDlgItem(m_hWnd, IDC_TCOFFSETX), UDM_SETPOS, 0, MAKELPARAM(theApp.GetConfigI("UserHacks_TCOffsetX"), 0));

	SendMessage(GetDlgItem(m_hWnd, IDC_TCOFFSETY), UDM_SETRANGE, 0, MAKELPARAM(10000, 0));
	SendMessage(GetDlgItem(m_hWnd, IDC_TCOFFSETY), UDM_SETPOS, 0, MAKELPARAM(theApp.GetConfigI("UserHacks_TCOffsetY"), 0));

	EnableWindow(GetDlgItem(m_hWnd, IDC_PRELOAD_GS), hwhacks);
	EnableWindow(GetDlgItem(m_hWnd, IDC_TC_DEPTH), hwhacks);
	EnableWindow(GetDlgItem(m_hWnd, IDC_CPU_FB_CONVERSION), hwhacks);
	EnableWindow(GetDlgItem(m_hWnd, IDC_FAST_TC_INV), hwhacks);
	EnableWindow(GetDlgItem(m_hWnd, IDC_AUTO_FLUSH_HW), hwhacks);
	EnableWindow(GetDlgItem(m_hWnd, IDC_SAFE_FEATURES), hwhacks);
	EnableWindow(GetDlgItem(m_hWnd, IDC_MEMORY_WRAPPING), hwhacks);

	// Half-screen bottom hack:
	EnableWindow(GetDlgItem(m_hWnd, IDC_HALF_SCREEN_TS), hwhacks);
	EnableWindow(GetDlgItem(m_hWnd, IDC_HALF_SCREEN_TS_TEXT), hwhacks);

	// Skipdraw hack:
	EnableWindow(GetDlgItem(m_hWnd, IDC_SKIPDRAWHACK_TEXT), hwhacks);
	EnableWindow(GetDlgItem(m_hWnd, IDC_SKIPDRAWOFFSETEDIT), hwhacks);
	EnableWindow(GetDlgItem(m_hWnd, IDC_SKIPDRAWOFFSET), hwhacks);
	EnableWindow(GetDlgItem(m_hWnd, IDC_SKIPDRAWHACKEDIT), hwhacks);
	EnableWindow(GetDlgItem(m_hWnd, IDC_SKIPDRAWHACK), hwhacks);

	// Texture offsets hack:
	EnableWindow(GetDlgItem(m_hWnd, IDC_TCOFFSET_TEXT), hwhacks);
	EnableWindow(GetDlgItem(m_hWnd, IDC_TCOFFSETX_TEXT), hwhacks);
	EnableWindow(GetDlgItem(m_hWnd, IDC_TCOFFSETX), hwhacks);
	EnableWindow(GetDlgItem(m_hWnd, IDC_TCOFFSETX2), hwhacks);
	EnableWindow(GetDlgItem(m_hWnd, IDC_TCOFFSETY_TEXT), hwhacks);
	EnableWindow(GetDlgItem(m_hWnd, IDC_TCOFFSETY), hwhacks);
	EnableWindow(GetDlgItem(m_hWnd, IDC_TCOFFSETY2), hwhacks);

	// OpenGL-only hacks:
	EnableWindow(GetDlgItem(m_hWnd, IDC_TRI_FILTER), hwhacks && ogl);
	EnableWindow(GetDlgItem(m_hWnd, IDC_TRI_FILTER_TEXT), hwhacks && ogl);

	// Upscaling hacks:
	EnableWindow(GetDlgItem(m_hWnd, IDC_WILDHACK), hwhacks && !native);
	EnableWindow(GetDlgItem(m_hWnd, IDC_ALIGN_SPRITE), hwhacks && !native);
	EnableWindow(GetDlgItem(m_hWnd, IDC_ROUND_SPRITE), hwhacks && !native);
	EnableWindow(GetDlgItem(m_hWnd, IDC_ROUND_SPRITE_TEXT), hwhacks && !native);
	EnableWindow(GetDlgItem(m_hWnd, IDC_OFFSETHACK_TEXT), hwhacks && !native);
	EnableWindow(GetDlgItem(m_hWnd, IDC_OFFSETHACK), hwhacks && !native);
	EnableWindow(GetDlgItem(m_hWnd, IDC_MERGE_PP_SPRITE), hwhacks && !native);

	// OpenGL Very Advanced Custom Settings:
	EnableWindow(GetDlgItem(m_hWnd, IDC_GEOMETRY_SHADER_OVERRIDE), ogl);
	EnableWindow(GetDlgItem(m_hWnd, IDC_GEOMETRY_SHADER_TEXT), ogl);
	EnableWindow(GetDlgItem(m_hWnd, IDC_IMAGE_LOAD_STORE), ogl);
	EnableWindow(GetDlgItem(m_hWnd, IDC_IMAGE_LOAD_STORE_TEXT), ogl);
	EnableWindow(GetDlgItem(m_hWnd, IDC_SPARSE_TEXTURE), ogl);
	EnableWindow(GetDlgItem(m_hWnd, IDC_SPARSE_TEXTURE_TEXT), ogl);

	AddTooltip(IDC_SKIPDRAWHACKEDIT);
	AddTooltip(IDC_SKIPDRAWHACK);
	AddTooltip(IDC_SKIPDRAWOFFSETEDIT);
	AddTooltip(IDC_SKIPDRAWOFFSET);
	AddTooltip(IDC_OFFSETHACK);
	AddTooltip(IDC_WILDHACK);
	AddTooltip(IDC_ALIGN_SPRITE);
	AddTooltip(IDC_ROUND_SPRITE);
	AddTooltip(IDC_TCOFFSETX);
	AddTooltip(IDC_TCOFFSETX2);
	AddTooltip(IDC_TCOFFSETY);
	AddTooltip(IDC_TCOFFSETY2);
	AddTooltip(IDC_PRELOAD_GS);
	AddTooltip(IDC_TC_DEPTH);
	AddTooltip(IDC_CPU_FB_CONVERSION);
	AddTooltip(IDC_FAST_TC_INV);
	AddTooltip(IDC_AUTO_FLUSH_HW);
	AddTooltip(IDC_SAFE_FEATURES);
	AddTooltip(IDC_MEMORY_WRAPPING);
	AddTooltip(IDC_HALF_SCREEN_TS);
	AddTooltip(IDC_TRI_FILTER);
	AddTooltip(IDC_MERGE_PP_SPRITE);
	AddTooltip(IDC_GEOMETRY_SHADER_OVERRIDE);
	AddTooltip(IDC_IMAGE_LOAD_STORE);
	AddTooltip(IDC_SPARSE_TEXTURE);

	UpdateControls();
}

void GSHacksDlg::UpdateControls()
{
	int skipdraw_offset = SendMessage(GetDlgItem(m_hWnd, IDC_SKIPDRAWOFFSET), UDM_GETPOS, 0, 0);
	int skipdraw = SendMessage(GetDlgItem(m_hWnd, IDC_SKIPDRAWHACK), UDM_GETPOS, 0, 0);

	const bool skipdraw_offset_changed = skipdraw_offset != m_old_skipdraw_offset;
	const bool skipdraw_changed = skipdraw != m_old_skipdraw;

	if (skipdraw_offset == 0 && skipdraw_offset_changed || skipdraw == 0 && skipdraw_changed)
	{
		skipdraw_offset = 0;
		skipdraw = 0;
	}
	else if (skipdraw_offset > skipdraw)
	{
		if (skipdraw_offset_changed)
			skipdraw = skipdraw_offset;
	}
	else if (skipdraw > 0 && skipdraw_offset == 0)
	{
		skipdraw_offset = 1;
	}

	const int tc_offset_x = SendMessage(GetDlgItem(m_hWnd, IDC_TCOFFSETX), UDM_GETPOS, 0, 0);
	const int tc_offset_y = SendMessage(GetDlgItem(m_hWnd, IDC_TCOFFSETY), UDM_GETPOS, 0, 0);
	SendMessage(GetDlgItem(m_hWnd, IDC_TCOFFSETX), UDM_SETPOS, 0, MAKELPARAM(tc_offset_x, 0));
	SendMessage(GetDlgItem(m_hWnd, IDC_TCOFFSETY), UDM_SETPOS, 0, MAKELPARAM(tc_offset_y, 0));
	SendMessage(GetDlgItem(m_hWnd, IDC_SKIPDRAWOFFSET), UDM_SETPOS, 0, MAKELPARAM(skipdraw_offset, 0));
	SendMessage(GetDlgItem(m_hWnd, IDC_SKIPDRAWHACK), UDM_SETPOS, 0, MAKELPARAM(skipdraw, 0));
	m_old_skipdraw_offset = skipdraw_offset;
	m_old_skipdraw = skipdraw;
}

bool GSHacksDlg::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
	{
	case WM_COMMAND:
	{
		const int id = LOWORD(wParam);

		switch(id)
		{
		case IDC_SKIPDRAWHACKEDIT:
		case IDC_SKIPDRAWOFFSETEDIT:
		case IDC_TCOFFSETX2:
		case IDC_TCOFFSETY2:
			if (HIWORD(wParam) == EN_CHANGE)
				UpdateControls();
			break;
		case IDOK:
		{
			INT_PTR data;
			if (ComboBoxGetSelData(IDC_HALF_SCREEN_TS, data))
			{
				theApp.SetConfig("UserHacks_Half_Bottom_Override", (int)data);
			}
			if (ComboBoxGetSelData(IDC_TRI_FILTER, data))
			{
				theApp.SetConfig("UserHacks_TriFilter", (int)data);
			}
			if (ComboBoxGetSelData(IDC_ROUND_SPRITE, data))
			{
				theApp.SetConfig("UserHacks_round_sprite_offset", (int)data);
			}
			if (ComboBoxGetSelData(IDC_OFFSETHACK, data))
			{
				theApp.SetConfig("UserHacks_HalfPixelOffset", (int)data);
			}
			if (ComboBoxGetSelData(IDC_GEOMETRY_SHADER_OVERRIDE, data))
			{
				theApp.SetConfig("override_geometry_shader", (int)data);
			}
			if (ComboBoxGetSelData(IDC_IMAGE_LOAD_STORE, data))
			{
				theApp.SetConfig("override_GL_ARB_shader_image_load_store", (int)data);
			}
			if (ComboBoxGetSelData(IDC_SPARSE_TEXTURE, data))
			{
				theApp.SetConfig("override_GL_ARB_sparse_texture", (int)data);
			}

			// It's more user friendly to lower the skipdraw offset value here - it prevents the skipdraw offset
			// value from decreasing unnecessarily if the user types a skipdraw value that is temporarily lower
			// than the skipdraw offset value.
			const int skipdraw_offset = SendMessage(GetDlgItem(m_hWnd, IDC_SKIPDRAWOFFSET), UDM_GETPOS, 0, 0);
			const int skipdraw = SendMessage(GetDlgItem(m_hWnd, IDC_SKIPDRAWHACK), UDM_GETPOS, 0, 0);
			theApp.SetConfig("UserHacks_SkipDraw_Offset", std::min(skipdraw_offset, skipdraw));
			theApp.SetConfig("UserHacks_SkipDraw", skipdraw);

			theApp.SetConfig("UserHacks_WildHack", (int)IsDlgButtonChecked(m_hWnd, IDC_WILDHACK));
			theApp.SetConfig("preload_frame_with_gs_data", (int)IsDlgButtonChecked(m_hWnd, IDC_PRELOAD_GS));
			theApp.SetConfig("UserHacks_align_sprite_X", (int)IsDlgButtonChecked(m_hWnd, IDC_ALIGN_SPRITE));
			theApp.SetConfig("UserHacks_DisableDepthSupport", (int)IsDlgButtonChecked(m_hWnd, IDC_TC_DEPTH));
			theApp.SetConfig("UserHacks_CPU_FB_Conversion", (int)IsDlgButtonChecked(m_hWnd, IDC_CPU_FB_CONVERSION));
			theApp.SetConfig("UserHacks_DisablePartialInvalidation", (int)IsDlgButtonChecked(m_hWnd, IDC_FAST_TC_INV));
			theApp.SetConfig("UserHacks_AutoFlush", (int)IsDlgButtonChecked(m_hWnd, IDC_AUTO_FLUSH_HW));
			theApp.SetConfig("UserHacks_Disable_Safe_Features", (int)IsDlgButtonChecked(m_hWnd, IDC_SAFE_FEATURES));
			theApp.SetConfig("wrap_gs_mem", (int)IsDlgButtonChecked(m_hWnd, IDC_MEMORY_WRAPPING));
			theApp.SetConfig("UserHacks_merge_pp_sprite", (int)IsDlgButtonChecked(m_hWnd, IDC_MERGE_PP_SPRITE));
			theApp.SetConfig("UserHacks_TCOffsetX", SendMessage(GetDlgItem(m_hWnd, IDC_TCOFFSETX), UDM_GETPOS, 0, 0));
			theApp.SetConfig("UserHacks_TCOffsetY", SendMessage(GetDlgItem(m_hWnd, IDC_TCOFFSETY), UDM_GETPOS, 0, 0));

			EndDialog(m_hWnd, id);
		} break;

		case IDCANCEL:
		{
			EndDialog(m_hWnd, IDCANCEL);
		} break;
		}

	} break;

	case WM_CLOSE:EndDialog(m_hWnd, IDCANCEL); break;

	default: return false;
	}

	return true;
}

// OSD Configuration Dialog

GSOSDDlg::GSOSDDlg() :
	GSDialog(IDD_OSD)
{}

void GSOSDDlg::OnInit()
{
	CheckDlgButton(m_hWnd, IDC_OSD_LOG, theApp.GetConfigB("osd_log_enabled"));
	CheckDlgButton(m_hWnd, IDC_OSD_MONITOR, theApp.GetConfigB("osd_monitor_enabled"));

	m_color.a = theApp.GetConfigI("osd_color_opacity");
	m_color.r = theApp.GetConfigI("osd_color_r");
	m_color.g = theApp.GetConfigI("osd_color_g");
	m_color.b = theApp.GetConfigI("osd_color_b");

	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_OPACITY_SLIDER), TBM_SETRANGE, TRUE, MAKELONG(0, 100));
	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_COLOR_RED_SLIDER), TBM_SETRANGE, TRUE, MAKELONG(0, 255));
	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_COLOR_GREEN_SLIDER), TBM_SETRANGE, TRUE, MAKELONG(0, 255));
	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_COLOR_BLUE_SLIDER), TBM_SETRANGE, TRUE, MAKELONG(0, 255));

	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_SIZE), UDM_SETRANGE, 0, MAKELPARAM(100, 1));
	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_SIZE), UDM_SETPOS, 0, MAKELPARAM(theApp.GetConfigI("osd_fontsize"), 0));

	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_TIMEOUT), UDM_SETRANGE, 0, MAKELPARAM(10, 2));
	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_TIMEOUT), UDM_SETPOS, 0, MAKELPARAM(theApp.GetConfigI("osd_log_timeout"), 0));

	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_MAX_LOG), UDM_SETRANGE, 0, MAKELPARAM(20, 1));
	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_MAX_LOG), UDM_SETPOS, 0, MAKELPARAM(theApp.GetConfigI("osd_max_log_messages"), 0));

	AddTooltip(IDC_OSD_MAX_LOG);
	AddTooltip(IDC_OSD_MAX_LOG_EDIT);
	AddTooltip(IDC_OSD_MONITOR);

	UpdateControls();
}

void GSOSDDlg::UpdateControls()
{
	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_OPACITY_SLIDER), TBM_SETPOS, TRUE, m_color.a);
	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_COLOR_RED_SLIDER), TBM_SETPOS, TRUE, m_color.r);
	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_COLOR_GREEN_SLIDER), TBM_SETPOS, TRUE, m_color.g);
	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_COLOR_BLUE_SLIDER), TBM_SETPOS, TRUE, m_color.b);

	char text[8] = { 0 };
	sprintf(text, "%d", m_color.a);
	SetDlgItemText(m_hWnd, IDC_OSD_OPACITY_AMOUNT, text);

	sprintf(text, "%d", m_color.r);
	SetDlgItemText(m_hWnd, IDC_OSD_COLOR_RED_AMOUNT, text);

	sprintf(text, "%d", m_color.g);
	SetDlgItemText(m_hWnd, IDC_OSD_COLOR_GREEN_AMOUNT, text);

	sprintf(text, "%d", m_color.b);
	SetDlgItemText(m_hWnd, IDC_OSD_COLOR_BLUE_AMOUNT, text);

	const bool monitor_enabled = IsDlgButtonChecked(m_hWnd, IDC_OSD_MONITOR) == BST_CHECKED;
	const bool log_enabled = IsDlgButtonChecked(m_hWnd, IDC_OSD_LOG) == BST_CHECKED;

	const int osd_size = SendMessage(GetDlgItem(m_hWnd, IDC_OSD_SIZE), UDM_GETPOS, 0, 0);
	const int osd_timeout = SendMessage(GetDlgItem(m_hWnd, IDC_OSD_TIMEOUT), UDM_GETPOS, 0, 0);
	const int osd_max_log = SendMessage(GetDlgItem(m_hWnd, IDC_OSD_MAX_LOG), UDM_GETPOS, 0, 0);
	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_SIZE), UDM_SETPOS, 0, MAKELPARAM(osd_size, 0));
	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_TIMEOUT), UDM_SETPOS, 0, MAKELPARAM(osd_timeout, 0));
	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_MAX_LOG), UDM_SETPOS, 0, MAKELPARAM(osd_max_log, 0));

	// Font
	EnableWindow(GetDlgItem(m_hWnd, IDC_OSD_COLOR_RED_SLIDER), monitor_enabled || log_enabled);
	EnableWindow(GetDlgItem(m_hWnd, IDC_OSD_COLOR_RED_TEXT), monitor_enabled || log_enabled);
	EnableWindow(GetDlgItem(m_hWnd, IDC_OSD_COLOR_RED_AMOUNT), monitor_enabled || log_enabled);
	EnableWindow(GetDlgItem(m_hWnd, IDC_OSD_COLOR_GREEN_SLIDER), monitor_enabled || log_enabled);
	EnableWindow(GetDlgItem(m_hWnd, IDC_OSD_COLOR_GREEN_TEXT), monitor_enabled || log_enabled);
	EnableWindow(GetDlgItem(m_hWnd, IDC_OSD_COLOR_GREEN_AMOUNT), monitor_enabled || log_enabled);
	EnableWindow(GetDlgItem(m_hWnd, IDC_OSD_COLOR_BLUE_SLIDER), monitor_enabled || log_enabled);
	EnableWindow(GetDlgItem(m_hWnd, IDC_OSD_COLOR_BLUE_TEXT), monitor_enabled || log_enabled);
	EnableWindow(GetDlgItem(m_hWnd, IDC_OSD_COLOR_BLUE_AMOUNT), monitor_enabled || log_enabled);
	EnableWindow(GetDlgItem(m_hWnd, IDC_OSD_SIZE_EDIT), monitor_enabled || log_enabled);
	EnableWindow(GetDlgItem(m_hWnd, IDC_OSD_SIZE), monitor_enabled || log_enabled);
	EnableWindow(GetDlgItem(m_hWnd, IDC_OSD_OPACITY_SLIDER), monitor_enabled || log_enabled);
	EnableWindow(GetDlgItem(m_hWnd, IDC_OSD_OPACITY_AMOUNT), monitor_enabled || log_enabled);
	EnableWindow(GetDlgItem(m_hWnd, IDC_OSD_SIZE_TEXT), monitor_enabled || log_enabled);
	EnableWindow(GetDlgItem(m_hWnd, IDC_OSD_OPACITY_TEXT), monitor_enabled || log_enabled);

	// Log
	EnableWindow(GetDlgItem(m_hWnd, IDC_OSD_TIMEOUT), log_enabled);
	EnableWindow(GetDlgItem(m_hWnd, IDC_OSD_TIMEOUT_EDIT), log_enabled);
	EnableWindow(GetDlgItem(m_hWnd, IDC_OSD_TIMEOUT_TEXT), log_enabled);
	EnableWindow(GetDlgItem(m_hWnd, IDC_OSD_MAX_LOG), log_enabled);
	EnableWindow(GetDlgItem(m_hWnd, IDC_OSD_MAX_LOG_EDIT), log_enabled);
	EnableWindow(GetDlgItem(m_hWnd, IDC_OSD_MAX_LOG_TEXT), log_enabled);
}

bool GSOSDDlg::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_HSCROLL:
	{
		if ((HWND)lParam == GetDlgItem(m_hWnd, IDC_OSD_OPACITY_SLIDER))
		{
			char text[8] = { 0 };

			m_color.a = SendMessage(GetDlgItem(m_hWnd, IDC_OSD_OPACITY_SLIDER), TBM_GETPOS, 0, 0);

			sprintf(text, "%d", m_color.a);
			SetDlgItemText(m_hWnd, IDC_OSD_OPACITY_AMOUNT, text);
		}
		else if ((HWND)lParam == GetDlgItem(m_hWnd, IDC_OSD_COLOR_RED_SLIDER))
		{
			char text[8] = { 0 };

			m_color.r = SendMessage(GetDlgItem(m_hWnd, IDC_OSD_COLOR_RED_SLIDER), TBM_GETPOS, 0, 0);

			sprintf(text, "%d", m_color.r);
			SetDlgItemText(m_hWnd, IDC_OSD_COLOR_RED_AMOUNT, text);
		}
		else if ((HWND)lParam == GetDlgItem(m_hWnd, IDC_OSD_COLOR_GREEN_SLIDER))
		{
			char text[8] = { 0 };

			m_color.g = SendMessage(GetDlgItem(m_hWnd, IDC_OSD_COLOR_GREEN_SLIDER), TBM_GETPOS, 0, 0);

			sprintf(text, "%d", m_color.g);
			SetDlgItemText(m_hWnd, IDC_OSD_COLOR_GREEN_AMOUNT, text);
		}
		else if ((HWND)lParam == GetDlgItem(m_hWnd, IDC_OSD_COLOR_BLUE_SLIDER))
		{
			char text[8] = { 0 };

			m_color.b = SendMessage(GetDlgItem(m_hWnd, IDC_OSD_COLOR_BLUE_SLIDER), TBM_GETPOS, 0, 0);

			sprintf(text, "%d", m_color.b);
			SetDlgItemText(m_hWnd, IDC_OSD_COLOR_BLUE_AMOUNT, text);
		}
	} break;

	case WM_COMMAND:
	{
		const int id = LOWORD(wParam);

		switch (id)
		{
		case IDOK:
		{
			theApp.SetConfig("osd_color_opacity", m_color.a);
			theApp.SetConfig("osd_color_r", m_color.r);
			theApp.SetConfig("osd_color_g", m_color.g);
			theApp.SetConfig("osd_color_b", m_color.b);

			theApp.SetConfig("osd_fontsize", (int)SendMessage(GetDlgItem(m_hWnd, IDC_OSD_SIZE), UDM_GETPOS, 0, 0));
			theApp.SetConfig("osd_log_timeout", (int)SendMessage(GetDlgItem(m_hWnd, IDC_OSD_TIMEOUT), UDM_GETPOS, 0, 0));
			theApp.SetConfig("osd_max_log_messages", (int)SendMessage(GetDlgItem(m_hWnd, IDC_OSD_MAX_LOG), UDM_GETPOS, 0, 0));

			theApp.SetConfig("osd_log_enabled", (int)IsDlgButtonChecked(m_hWnd, IDC_OSD_LOG));
			theApp.SetConfig("osd_monitor_enabled", (int)IsDlgButtonChecked(m_hWnd, IDC_OSD_MONITOR));

			EndDialog(m_hWnd, id);
		} break;
		case IDC_OSD_LOG:
			if (HIWORD(wParam) == BN_CLICKED)
				UpdateControls();
			break;
		case IDC_OSD_MONITOR:
			if (HIWORD(wParam) == BN_CLICKED)
				UpdateControls();
			break;
		case IDC_OSD_SIZE_EDIT:
		case IDC_OSD_TIMEOUT_EDIT:
		case IDC_OSD_MAX_LOG_EDIT:
			if (HIWORD(wParam) == EN_CHANGE)
				UpdateControls();
			break;
		case IDCANCEL:
		{
			EndDialog(m_hWnd, IDCANCEL);
		} break;
		}

	} break;

	case WM_CLOSE:EndDialog(m_hWnd, IDCANCEL); break;

	default: return false;
	}


	return true;
}
