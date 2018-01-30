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
#include "GSDevice9.h"
#include "GSDevice11.h"
#include "resource.h"
#include "GSSetting.h"


GSSettingsDlg::GSSettingsDlg()
	: GSDialog(IDD_CONFIG)

{
#ifdef ENABLE_OPENCL
	std::list<OCLDeviceDesc> ocldevs;

	GSUtil::GetDeviceDescs(ocldevs);

	int index = 0;

	for(auto dev : ocldevs)
	{
		m_ocl_devs.push_back(GSSetting(index++, dev.name.c_str(), ""));
	}
#endif
}

void GSSettingsDlg::OnInit()
{
	__super::OnInit();

	CComPtr<IDirect3D9> d3d9;

	d3d9.Attach(Direct3DCreate9(D3D_SDK_VERSION));

	CComPtr<IDXGIFactory1> dxgi_factory;
	
	if(GSUtil::CheckDXGI())
	{
		CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&dxgi_factory);
	}
	adapters.clear();
	adapters.push_back(Adapter("Default Hardware Device", "default", GSUtil::CheckDirect3D11Level(NULL, D3D_DRIVER_TYPE_HARDWARE)));
	adapters.push_back(Adapter("Reference Device", "ref", GSUtil::CheckDirect3D11Level(NULL, D3D_DRIVER_TYPE_REFERENCE)));

	if(dxgi_factory)
	{
		for(int i = 0;; i++)
		{
			CComPtr<IDXGIAdapter1> adapter;

			if(S_OK != dxgi_factory->EnumAdapters1(i, &adapter))
				break;

			DXGI_ADAPTER_DESC1 desc;

			HRESULT hr = adapter->GetDesc1(&desc);

			if(S_OK == hr)
			{
				D3D_FEATURE_LEVEL level = GSUtil::CheckDirect3D11Level(adapter, D3D_DRIVER_TYPE_UNKNOWN);
				// GSDX isn't unicode!?
#if 1
				int size = WideCharToMultiByte(CP_ACP, 0, desc.Description, sizeof(desc.Description), NULL, 0, NULL, NULL);
				char *buf = new char[size];
				WideCharToMultiByte(CP_ACP, 0, desc.Description, sizeof(desc.Description), buf, size, NULL, NULL);
				adapters.push_back(Adapter(buf, GSAdapter(desc), level));
				delete[] buf;
#else
				adapters.push_back(Adapter(desc.Description, GSAdapter(desc), level));
#endif
			}
		}
	}
	else if(d3d9)
	{
		int n = d3d9->GetAdapterCount();
		for(int i = 0; i < n; i++)
		{
			D3DADAPTER_IDENTIFIER9 desc;

			if(D3D_OK != d3d9->GetAdapterIdentifier(i, 0, &desc))
				break;

			// GSDX isn't unicode!?
#if 0
			wchar_t buf[sizeof desc.Description * sizeof(WCHAR)];
			MultiByteToWideChar(CP_ACP /* I have no idea if this is right */, 0, desc.Description, sizeof(desc.Description), buf, sizeof buf / sizeof *buf);
			adapters.push_back(Adapter(buf, GSAdapter(desc), (D3D_FEATURE_LEVEL)0));
#else
			adapters.push_back(Adapter(desc.Description, GSAdapter(desc), (D3D_FEATURE_LEVEL)0));
#endif
		}
	}

	std::string adapter_setting = theApp.GetConfigS("Adapter");
	std::vector<GSSetting> adapter_settings;
	unsigned int adapter_sel = 0;

	for(unsigned int i = 0; i < adapters.size(); i++)
	{
		if(adapters[i].id == adapter_setting)
		{
			adapter_sel = i;
		}

		adapter_settings.push_back(GSSetting(i, adapters[i].name.c_str(), ""));
	}

	std::string ocldev = theApp.GetConfigS("ocldev");

	unsigned int ocl_sel = 0;

	for(unsigned int i = 0; i < m_ocl_devs.size(); i++)
	{
		if(ocldev == m_ocl_devs[i].name)
		{
			ocl_sel = i;

			break;
		}
	}

	ComboBoxInit(IDC_ADAPTER, adapter_settings, adapter_sel);
	ComboBoxInit(IDC_OPENCL_DEVICE, m_ocl_devs, ocl_sel);
	ComboBoxInit(IDC_MIPMAP_HW, theApp.m_gs_hw_mipmapping, theApp.GetConfigI("mipmap_hw"));
	UpdateRenderers();

	ComboBoxInit(IDC_INTERLACE, theApp.m_gs_interlace, theApp.GetConfigI("interlace"));
	ComboBoxInit(IDC_UPSCALE_MULTIPLIER, theApp.m_gs_upscale_multiplier, theApp.GetConfigI("upscale_multiplier"));
	ComboBoxInit(IDC_AFCOMBO, theApp.m_gs_max_anisotropy, theApp.GetConfigI("MaxAnisotropy"));
	ComboBoxInit(IDC_FILTER, theApp.m_gs_bifilter, theApp.GetConfigI("filter"));
	ComboBoxInit(IDC_ACCURATE_BLEND_UNIT, theApp.m_gs_acc_blend_level, theApp.GetConfigI("accurate_blending_unit"));
	ComboBoxInit(IDC_CRC_LEVEL, theApp.m_gs_crc_level, theApp.GetConfigI("crc_hack_level"));

	CheckDlgButton(m_hWnd, IDC_PALTEX, theApp.GetConfigB("paltex"));
	CheckDlgButton(m_hWnd, IDC_LARGE_FB, theApp.GetConfigB("large_framebuffer"));
	CheckDlgButton(m_hWnd, IDC_LOGZ, theApp.GetConfigB("logz"));
	CheckDlgButton(m_hWnd, IDC_FBA, theApp.GetConfigB("fba"));
	CheckDlgButton(m_hWnd, IDC_MIPMAP_SW, theApp.GetConfigB("mipmap"));
	CheckDlgButton(m_hWnd, IDC_AA1, theApp.GetConfigB("aa1"));
	CheckDlgButton(m_hWnd, IDC_ACCURATE_DATE, theApp.GetConfigB("accurate_date"));

	// Hacks
	CheckDlgButton(m_hWnd, IDC_HACKS_ENABLED, theApp.GetConfigB("UserHacks"));

	SendMessage(GetDlgItem(m_hWnd, IDC_RESX), UDM_SETRANGE, 0, MAKELPARAM(8192, 256));
	SendMessage(GetDlgItem(m_hWnd, IDC_RESX), UDM_SETPOS, 0, MAKELPARAM(theApp.GetConfigI("resx"), 0));

	SendMessage(GetDlgItem(m_hWnd, IDC_RESY), UDM_SETRANGE, 0, MAKELPARAM(8192, 256));
	SendMessage(GetDlgItem(m_hWnd, IDC_RESY), UDM_SETPOS, 0, MAKELPARAM(theApp.GetConfigI("resy"), 0));

	SendMessage(GetDlgItem(m_hWnd, IDC_SWTHREADS), UDM_SETRANGE, 0, MAKELPARAM(16, 0));
	SendMessage(GetDlgItem(m_hWnd, IDC_SWTHREADS), UDM_SETPOS, 0, MAKELPARAM(theApp.GetConfigI("extrathreads"), 0));

	AddTooltip(IDC_FILTER);
	AddTooltip(IDC_CRC_LEVEL);
	AddTooltip(IDC_PALTEX);
	AddTooltip(IDC_ACCURATE_DATE);
	AddTooltip(IDC_ACCURATE_BLEND_UNIT);
	AddTooltip(IDC_AFCOMBO);
	AddTooltip(IDC_AA1);
	AddTooltip(IDC_MIPMAP_HW);
	AddTooltip(IDC_MIPMAP_SW);
	AddTooltip(IDC_SWTHREADS);
	AddTooltip(IDC_SWTHREADS_EDIT);
	AddTooltip(IDC_FBA);
	AddTooltip(IDC_LOGZ);
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
				UpdateRenderers();
				UpdateControls();
			}
			break;
		case IDC_RENDERER:
			if (code == CBN_SELCHANGE)
			{
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
				ShaderDlg.DoModal();
			break;
		case IDC_OSDBUTTON:
			if (code == BN_CLICKED)
				OSDDlg.DoModal();
			break;
		case IDC_HACKSBUTTON:
			if (code == BN_CLICKED)
				HacksDlg.DoModal();
			break;
		case IDOK:
		{
			INT_PTR data;

			if(ComboBoxGetSelData(IDC_ADAPTER, data))
			{
				theApp.SetConfig("Adapter", adapters[(int)data].id.c_str());
			}

			if(ComboBoxGetSelData(IDC_OPENCL_DEVICE, data))
			{
				if ((UINT)data < m_ocl_devs.size()) {
					theApp.SetConfig("ocldev", m_ocl_devs[(int)data].name.c_str());
				}
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

			if(ComboBoxGetSelData(IDC_ACCURATE_BLEND_UNIT, data))
			{
				theApp.SetConfig("accurate_blending_unit", (int)data);
			}

			if (ComboBoxGetSelData(IDC_CRC_LEVEL, data))
			{
				theApp.SetConfig("crc_hack_level", (int)data);
			}

			if(ComboBoxGetSelData(IDC_AFCOMBO, data))
			{
				theApp.SetConfig("MaxAnisotropy", (int)data);
			}

			theApp.SetConfig("mipmap", (int)IsDlgButtonChecked(m_hWnd, IDC_MIPMAP_SW));
			theApp.SetConfig("paltex", (int)IsDlgButtonChecked(m_hWnd, IDC_PALTEX));
			theApp.SetConfig("large_framebuffer", (int)IsDlgButtonChecked(m_hWnd, IDC_LARGE_FB));
			theApp.SetConfig("logz", (int)IsDlgButtonChecked(m_hWnd, IDC_LOGZ));
			theApp.SetConfig("fba", (int)IsDlgButtonChecked(m_hWnd, IDC_FBA));
			theApp.SetConfig("aa1", (int)IsDlgButtonChecked(m_hWnd, IDC_AA1));
			theApp.SetConfig("accurate_date", (int)IsDlgButtonChecked(m_hWnd, IDC_ACCURATE_DATE));
			theApp.SetConfig("UserHacks", (int)IsDlgButtonChecked(m_hWnd, IDC_HACKS_ENABLED));

			// The LOWORD returned by UDM_GETPOS automatically restricts the value to its input range.
			theApp.SetConfig("extrathreads", LOWORD(SendMessage(GetDlgItem(m_hWnd, IDC_SWTHREADS), UDM_GETPOS, 0, 0)));
			theApp.SetConfig("resx", LOWORD(SendMessage(GetDlgItem(m_hWnd, IDC_RESX), UDM_GETPOS, 0, 0)));
			theApp.SetConfig("resy", LOWORD(SendMessage(GetDlgItem(m_hWnd, IDC_RESY), UDM_GETPOS, 0, 0)));
		}
		break;
	}

	return __super::OnCommand(hWnd, id, code);
}

void GSSettingsDlg::UpdateRenderers()
{
	INT_PTR i;

	if (!ComboBoxGetSelData(IDC_ADAPTER, i))
		return;

	// Ugggh
	HacksDlg.SetAdapter(adapters[(int)i].id);

	D3D_FEATURE_LEVEL level = adapters[(int)i].level;

	std::vector<GSSetting> renderers;

	GSRendererType renderer_setting;

	if (ComboBoxGetSelData(IDC_RENDERER, i))
	{
		renderer_setting = static_cast<GSRendererType>(i);
	}
	else
	{
		renderer_setting = GSRendererType(theApp.GetConfigI("Renderer"));
		if (renderer_setting == GSRendererType::Undefined)
			renderer_setting = GSUtil::GetBestRenderer();
	}

	GSRendererType renderer_sel = GSRendererType::Default;

	for(size_t i = 0; i < theApp.m_gs_renderers.size(); i++)
	{
		GSSetting r = theApp.m_gs_renderers[i];

		GSRendererType renderer = static_cast<GSRendererType>(r.value);

		if(renderer == GSRendererType::DX1011_HW || renderer == GSRendererType::DX1011_SW || renderer == GSRendererType::DX1011_OpenCL)
		{
			if(level < D3D_FEATURE_LEVEL_10_0) continue;
			r.name += (level >= D3D_FEATURE_LEVEL_11_0 ? "11" : "10");
		}

		renderers.push_back(r);

		if (static_cast<GSRendererType>(r.value) == renderer_setting)
		{
			renderer_sel = renderer_setting;
		}
	}

	ComboBoxInit(IDC_RENDERER, renderers, static_cast<int32_t>(renderer_sel));
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
		GSRendererType renderer = static_cast<GSRendererType>(i);
		D3D_FEATURE_LEVEL level = GSUtil::CheckDirect3D11Level();

		bool dx9 = renderer == GSRendererType::DX9_HW || renderer == GSRendererType::DX9_SW || renderer == GSRendererType::DX9_OpenCL;
		bool dx11 = renderer == GSRendererType::DX1011_HW || renderer == GSRendererType::DX1011_SW || renderer == GSRendererType::DX1011_OpenCL;
		bool ogl = renderer == GSRendererType::OGL_HW || renderer == GSRendererType::OGL_SW || renderer == GSRendererType::OGL_OpenCL;

		bool hw = renderer == GSRendererType::DX9_HW || renderer == GSRendererType::DX1011_HW || renderer == GSRendererType::OGL_HW;
		bool sw = renderer == GSRendererType::DX9_SW || renderer == GSRendererType::DX1011_SW || renderer == GSRendererType::OGL_SW;
		bool ocl = renderer == GSRendererType::DX9_OpenCL || renderer == GSRendererType::DX1011_OpenCL || renderer == GSRendererType::OGL_OpenCL;
		bool null = renderer == GSRendererType::Null;

		ShowWindow(GetDlgItem(m_hWnd, IDC_LOGO9), dx9 ? SW_SHOW : SW_HIDE);
		ShowWindow(GetDlgItem(m_hWnd, IDC_LOGO11), (dx11 && level >= D3D_FEATURE_LEVEL_11_0) ? SW_SHOW : SW_HIDE);
		ShowWindow(GetDlgItem(m_hWnd, IDC_LOGO10), (dx11 && level < D3D_FEATURE_LEVEL_11_0) ? SW_SHOW : SW_HIDE);
		ShowWindow(GetDlgItem(m_hWnd, IDC_NULL), null ? SW_SHOW : SW_HIDE);
		ShowWindow(GetDlgItem(m_hWnd, IDC_LOGOGL), ogl ? SW_SHOW : SW_HIDE);
#ifndef ENABLE_OPENCL
		ShowWindow(GetDlgItem(m_hWnd, IDC_OPENCL_DEVICE), SW_HIDE);
		ShowWindow(GetDlgItem(m_hWnd, IDC_OPENCL_TEXT), SW_HIDE);
#endif

		ShowWindow(GetDlgItem(m_hWnd, IDC_LOGZ), dx9 ? SW_SHOW: SW_HIDE);
		ShowWindow(GetDlgItem(m_hWnd, IDC_FBA), dx9 ? SW_SHOW : SW_HIDE);

		ShowWindow(GetDlgItem(m_hWnd, IDC_ACCURATE_DATE), ogl ? SW_SHOW : SW_HIDE);
		ShowWindow(GetDlgItem(m_hWnd, IDC_ACCURATE_BLEND_UNIT), ogl ? SW_SHOW : SW_HIDE);
		ShowWindow(GetDlgItem(m_hWnd, IDC_ACCURATE_BLEND_UNIT_TEXT), ogl ? SW_SHOW : SW_HIDE);

		EnableWindow(GetDlgItem(m_hWnd, IDC_MIPMAP_HW), hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_MIPMAP_HW_TEXT), hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_CRC_LEVEL), hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_LARGE_FB), integer_scaling != 1 && hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_CRC_LEVEL_TEXT), hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_OPENCL_DEVICE), ocl);
		EnableWindow(GetDlgItem(m_hWnd, IDC_RESX), hw && !integer_scaling);
		EnableWindow(GetDlgItem(m_hWnd, IDC_RESX_EDIT), hw && !integer_scaling);
		EnableWindow(GetDlgItem(m_hWnd, IDC_RESY), hw && !integer_scaling);
		EnableWindow(GetDlgItem(m_hWnd, IDC_RESY_EDIT), hw && !integer_scaling);
		EnableWindow(GetDlgItem(m_hWnd, IDC_CUSTOM_TEXT), hw && !integer_scaling);
		EnableWindow(GetDlgItem(m_hWnd, IDC_UPSCALE_MULTIPLIER), hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_UPSCALE_MULTIPLIER_TEXT), hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_PALTEX), hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_LOGZ), dx9 && hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_FBA), dx9 && hw);

		INT_PTR filter;
		if (ComboBoxGetSelData(IDC_FILTER, filter))
		{
			EnableWindow(GetDlgItem(m_hWnd, IDC_AFCOMBO), hw && filter && !IsDlgButtonChecked(m_hWnd, IDC_PALTEX));
		}
		EnableWindow(GetDlgItem(m_hWnd, IDC_AFCOMBO_TEXT), hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_ACCURATE_DATE), ogl && hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_ACCURATE_BLEND_UNIT), ogl && hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_ACCURATE_BLEND_UNIT_TEXT), ogl && hw);

		// Software mode settings
		EnableWindow(GetDlgItem(m_hWnd, IDC_MIPMAP_SW), sw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_AA1), sw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_SWTHREADS_TEXT), sw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_SWTHREADS_EDIT), sw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_SWTHREADS), sw);

		// Hacks
		EnableWindow(GetDlgItem(m_hWnd, IDC_HACKS_ENABLED), hw);
		EnableWindow(GetDlgItem(m_hWnd, IDC_HACKSBUTTON), hw && IsDlgButtonChecked(m_hWnd, IDC_HACKS_ENABLED));

		// OSD Configuration
		EnableWindow(GetDlgItem(m_hWnd, IDC_OSDBUTTON), ogl);

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
	bool external_shader_selected = IsDlgButtonChecked(m_hWnd, IDC_SHADER_FX) == BST_CHECKED;
	bool shadeboost_selected = IsDlgButtonChecked(m_hWnd, IDC_SHADEBOOST) == BST_CHECKED;
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
		int id = LOWORD(wParam);

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
			theApp.SetConfig("Fxaa", (int)IsDlgButtonChecked(m_hWnd, IDC_FXAA));

			// Texture Filtering Of Display
			theApp.SetConfig("linear_present", (int)IsDlgButtonChecked(m_hWnd, IDC_LINEAR_PRESENT));

			// External FX Shader
			theApp.SetConfig("shaderfx", (int)IsDlgButtonChecked(m_hWnd, IDC_SHADER_FX));

			// External FX Shader(OpenGL)
			int shader_fx_length = (int)SendMessage(GetDlgItem(m_hWnd, IDC_SHADER_FX_EDIT), WM_GETTEXTLENGTH, 0, 0);
			int shader_fx_conf_length = (int)SendMessage(GetDlgItem(m_hWnd, IDC_SHADER_FX_CONF_EDIT), WM_GETTEXTLENGTH, 0, 0);
			int length = std::max(shader_fx_length, shader_fx_conf_length) + 1;
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

GSHacksDlg::GSHacksDlg() :
	GSDialog(IDD_HACKS)
{
	memset(msaa2cb, 0, sizeof(msaa2cb));
	memset(cb2msaa, 0, sizeof(cb2msaa));
}

void GSHacksDlg::OnInit()
{
	HWND hwnd_renderer = GetDlgItem(GetParent(m_hWnd), IDC_RENDERER);
	HWND hwnd_upscaling = GetDlgItem(GetParent(m_hWnd), IDC_UPSCALE_MULTIPLIER);
	GSRendererType renderer = static_cast<GSRendererType>(SendMessage(hwnd_renderer, CB_GETITEMDATA, SendMessage(hwnd_renderer, CB_GETCURSEL, 0, 0), 0));
	unsigned short upscaling_multiplier = static_cast<unsigned short>(SendMessage(hwnd_upscaling, CB_GETITEMDATA, SendMessage(hwnd_upscaling, CB_GETCURSEL, 0, 0), 0));
	unsigned short cb = 0;

	// It can only be accessed with a HW renderer, so this is sufficient.
	bool dx9 = renderer == GSRendererType::DX9_HW;
	bool ogl = renderer == GSRendererType::OGL_HW;
	bool native = upscaling_multiplier == 1;

	if(dx9) for(unsigned short i = 0; i < 17; i++)
	{
		if( i == 1) continue;

		int depth = GSDevice9::GetMaxDepth(i, adapter_id);

		if(depth)
		{
			char text[32] = {0};
			sprintf(text, depth == 32 ? "%dx Z-32" : "%dx Z-24", i);
			SendMessage(GetDlgItem(m_hWnd, IDC_MSAACB), CB_ADDSTRING, 0, (LPARAM)text);

			msaa2cb[i] = cb;
			cb2msaa[cb] = i;
			cb++;
		}
	}
	else for(unsigned short j = 0; j < 5; j++) // TODO: Make the same kind of check for d3d11, eventually....
	{
		unsigned short i = j == 0 ? 0 : 1 << j;

		msaa2cb[i] = j;
		cb2msaa[j] = i;

		char text[32] = {0};
		sprintf(text, "%dx ", i);

		SendMessage(GetDlgItem(m_hWnd, IDC_MSAACB), CB_ADDSTRING, 0, (LPARAM)text);
	}

	SendMessage(GetDlgItem(m_hWnd, IDC_MSAACB), CB_SETCURSEL, msaa2cb[std::min(theApp.GetConfigI("UserHacks_MSAA"), 16)], 0);

	CheckDlgButton(m_hWnd, IDC_ALPHAHACK, theApp.GetConfigB("UserHacks_AlphaHack"));
	CheckDlgButton(m_hWnd, IDC_WILDHACK, theApp.GetConfigI("UserHacks_WildHack"));
	CheckDlgButton(m_hWnd, IDC_ALPHASTENCIL, theApp.GetConfigB("UserHacks_AlphaStencil"));
	CheckDlgButton(m_hWnd, IDC_PRELOAD_GS, theApp.GetConfigB("preload_frame_with_gs_data"));
	CheckDlgButton(m_hWnd, IDC_ALIGN_SPRITE, theApp.GetConfigB("UserHacks_align_sprite_X"));
	CheckDlgButton(m_hWnd, IDC_TC_DEPTH, theApp.GetConfigB("UserHacks_DisableDepthSupport"));
	CheckDlgButton(m_hWnd, IDC_CPU_FB_CONVERSION, theApp.GetConfigB("UserHacks_CPU_FB_Conversion"));
	CheckDlgButton(m_hWnd, IDC_FAST_TC_INV, theApp.GetConfigB("UserHacks_DisablePartialInvalidation"));
	CheckDlgButton(m_hWnd, IDC_AUTO_FLUSH, theApp.GetConfigB("UserHacks_AutoFlush"));
	CheckDlgButton(m_hWnd, IDC_UNSCALE_POINT_LINE, theApp.GetConfigB("UserHacks_unscale_point_line"));
	CheckDlgButton(m_hWnd, IDC_MEMORY_WRAPPING, theApp.GetConfigB("wrap_gs_mem"));
	CheckDlgButton(m_hWnd, IDC_MERGE_PP_SPRITE, theApp.GetConfigB("UserHacks_merge_pp_sprite"));

	ComboBoxInit(IDC_TRI_FILTER, theApp.m_gs_trifilter, theApp.GetConfigI("UserHacks_TriFilter"));
	ComboBoxInit(IDC_OFFSETHACK, theApp.m_gs_offset_hack, theApp.GetConfigI("UserHacks_HalfPixelOffset"));
	ComboBoxInit(IDC_ROUND_SPRITE, theApp.m_gs_hack, theApp.GetConfigI("UserHacks_round_sprite_offset"));
	ComboBoxInit(IDC_SPRITEHACK, theApp.m_gs_hack, theApp.GetConfigI("UserHacks_SpriteHack"));
	ComboBoxInit(IDC_GEOMETRY_SHADER_OVERRIDE, theApp.m_gs_gl_ext, theApp.GetConfigI("override_geometry_shader"));
	ComboBoxInit(IDC_IMAGE_LOAD_STORE, theApp.m_gs_gl_ext, theApp.GetConfigI("override_GL_ARB_shader_image_load_store"));

	SendMessage(GetDlgItem(m_hWnd, IDC_SKIPDRAWHACK), UDM_SETRANGE, 0, MAKELPARAM(10000, 0));
	SendMessage(GetDlgItem(m_hWnd, IDC_SKIPDRAWHACK), UDM_SETPOS, 0, MAKELPARAM(theApp.GetConfigI("UserHacks_SkipDraw"), 0));

	SendMessage(GetDlgItem(m_hWnd, IDC_TCOFFSETX), UDM_SETRANGE, 0, MAKELPARAM(10000, 0));
	SendMessage(GetDlgItem(m_hWnd, IDC_TCOFFSETX), UDM_SETPOS, 0, MAKELPARAM(theApp.GetConfigI("UserHacks_TCOffset") & 0xFFFF, 0));

	SendMessage(GetDlgItem(m_hWnd, IDC_TCOFFSETY), UDM_SETRANGE, 0, MAKELPARAM(10000, 0));
	SendMessage(GetDlgItem(m_hWnd, IDC_TCOFFSETY), UDM_SETPOS, 0, MAKELPARAM((theApp.GetConfigI("UserHacks_TCOffset") >> 16) & 0xFFFF, 0));

	// Direct3D-only hacks:
	EnableWindow(GetDlgItem(m_hWnd, IDC_ALPHASTENCIL), !ogl);
	EnableWindow(GetDlgItem(m_hWnd, IDC_ALPHAHACK), !ogl);
	EnableWindow(GetDlgItem(m_hWnd, IDC_MSAACB), !ogl);
	EnableWindow(GetDlgItem(m_hWnd, IDC_MSAA_TEXT), !ogl);

	// OpenGL-only hacks:
	EnableWindow(GetDlgItem(m_hWnd, IDC_TC_DEPTH), ogl);
	EnableWindow(GetDlgItem(m_hWnd, IDC_TRI_FILTER), ogl);
	EnableWindow(GetDlgItem(m_hWnd, IDC_TRI_FILTER_TEXT), ogl);

	// Upscaling hacks:
	EnableWindow(GetDlgItem(m_hWnd, IDC_UNSCALE_POINT_LINE), !dx9 && !native);
	EnableWindow(GetDlgItem(m_hWnd, IDC_SPRITEHACK), !native);
	EnableWindow(GetDlgItem(m_hWnd, IDC_WILDHACK), !native);
	EnableWindow(GetDlgItem(m_hWnd, IDC_ALIGN_SPRITE), !native);
	EnableWindow(GetDlgItem(m_hWnd, IDC_ROUND_SPRITE), !native);
	EnableWindow(GetDlgItem(m_hWnd, IDC_SPRITEHACK_TEXT), !native);
	EnableWindow(GetDlgItem(m_hWnd, IDC_ROUND_SPRITE_TEXT), !native);
	EnableWindow(GetDlgItem(m_hWnd, IDC_OFFSETHACK_TEXT), !native);
	EnableWindow(GetDlgItem(m_hWnd, IDC_OFFSETHACK), !native);
	EnableWindow(GetDlgItem(m_hWnd, IDC_MERGE_PP_SPRITE), !native);

	// OpenGL Very Advanced Custom Settings:
	EnableWindow(GetDlgItem(m_hWnd, IDC_GEOMETRY_SHADER_OVERRIDE), ogl);
	EnableWindow(GetDlgItem(m_hWnd, IDC_GEOMETRY_SHADER_TEXT), ogl);
	EnableWindow(GetDlgItem(m_hWnd, IDC_IMAGE_LOAD_STORE), ogl);
	EnableWindow(GetDlgItem(m_hWnd, IDC_IMAGE_LOAD_STORE_TEXT), ogl);

	AddTooltip(IDC_SKIPDRAWHACKEDIT);
	AddTooltip(IDC_SKIPDRAWHACK);
	AddTooltip(IDC_ALPHAHACK);
	AddTooltip(IDC_OFFSETHACK);
	AddTooltip(IDC_SPRITEHACK);
	AddTooltip(IDC_WILDHACK);
	AddTooltip(IDC_MSAACB);
	AddTooltip(IDC_ALPHASTENCIL);
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
	AddTooltip(IDC_AUTO_FLUSH);
	AddTooltip(IDC_UNSCALE_POINT_LINE);
	AddTooltip(IDC_MEMORY_WRAPPING);
	AddTooltip(IDC_TRI_FILTER);
	AddTooltip(IDC_MERGE_PP_SPRITE);
	AddTooltip(IDC_GEOMETRY_SHADER_OVERRIDE);
	AddTooltip(IDC_IMAGE_LOAD_STORE);
}

void GSHacksDlg::UpdateControls()
{}

bool GSHacksDlg::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
	{
	case WM_COMMAND:
	{
		int id = LOWORD(wParam);

		switch(id)
		{
		case IDOK: 
		{
			INT_PTR data;
			if (ComboBoxGetSelData(IDC_TRI_FILTER, data))
			{
				theApp.SetConfig("UserHacks_TriFilter", (int)data);
			}
			if (ComboBoxGetSelData(IDC_ROUND_SPRITE, data))
			{
				theApp.SetConfig("UserHacks_round_sprite_offset", (int)data);
			}
			if (ComboBoxGetSelData(IDC_SPRITEHACK, data))
			{
				theApp.SetConfig("UserHacks_SpriteHack", (int)data);
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
			theApp.SetConfig("UserHacks_MSAA", cb2msaa[(int)SendMessage(GetDlgItem(m_hWnd, IDC_MSAACB), CB_GETCURSEL, 0, 0)]);
			theApp.SetConfig("UserHacks_AlphaHack", (int)IsDlgButtonChecked(m_hWnd, IDC_ALPHAHACK));
			theApp.SetConfig("UserHacks_SkipDraw", (int)SendMessage(GetDlgItem(m_hWnd, IDC_SKIPDRAWHACK), UDM_GETPOS, 0, 0));
			theApp.SetConfig("UserHacks_WildHack", (int)IsDlgButtonChecked(m_hWnd, IDC_WILDHACK));
			theApp.SetConfig("UserHacks_AlphaStencil", (int)IsDlgButtonChecked(m_hWnd, IDC_ALPHASTENCIL));
			theApp.SetConfig("preload_frame_with_gs_data", (int)IsDlgButtonChecked(m_hWnd, IDC_PRELOAD_GS));
			theApp.SetConfig("Userhacks_align_sprite_X", (int)IsDlgButtonChecked(m_hWnd, IDC_ALIGN_SPRITE));
			theApp.SetConfig("UserHacks_DisableDepthSupport", (int)IsDlgButtonChecked(m_hWnd, IDC_TC_DEPTH));
			theApp.SetConfig("UserHacks_CPU_FB_Conversion", (int)IsDlgButtonChecked(m_hWnd, IDC_CPU_FB_CONVERSION));
			theApp.SetConfig("UserHacks_DisablePartialInvalidation", (int)IsDlgButtonChecked(m_hWnd, IDC_FAST_TC_INV));
			theApp.SetConfig("UserHacks_AutoFlush", (int)IsDlgButtonChecked(m_hWnd, IDC_AUTO_FLUSH));
			theApp.SetConfig("UserHacks_unscale_point_line", (int)IsDlgButtonChecked(m_hWnd, IDC_UNSCALE_POINT_LINE));
			theApp.SetConfig("wrap_gs_mem", (int)IsDlgButtonChecked(m_hWnd, IDC_MEMORY_WRAPPING));
			theApp.SetConfig("UserHacks_merge_pp_sprite", (int)IsDlgButtonChecked(m_hWnd, IDC_MERGE_PP_SPRITE));

			unsigned int TCOFFSET  =  SendMessage(GetDlgItem(m_hWnd, IDC_TCOFFSETX), UDM_GETPOS, 0, 0) & 0xFFFF;
						 TCOFFSET |= (SendMessage(GetDlgItem(m_hWnd, IDC_TCOFFSETY), UDM_GETPOS, 0, 0) & 0xFFFF) << 16;

			theApp.SetConfig("UserHacks_TCOffset", TCOFFSET);

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
	// OSD Indicator is greyed out because it's currently an empty shell.
	EnableWindow(GetDlgItem(m_hWnd, IDC_OSD_INDICATOR), false);

	CheckDlgButton(m_hWnd, IDC_OSD_LOG, theApp.GetConfigB("osd_log_enabled"));
	CheckDlgButton(m_hWnd, IDC_OSD_MONITOR, theApp.GetConfigB("osd_monitor_enabled"));
	CheckDlgButton(m_hWnd, IDC_OSD_INDICATOR, theApp.GetConfigB("osd_indicator_enabled"));
	m_transparency = theApp.GetConfigI("osd_transparency");

	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_TRANSPARENCY_SLIDER), TBM_SETRANGE, TRUE, MAKELONG(0, 100));

	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_SIZE), UDM_SETRANGE, 0, MAKELPARAM(100, 1));
	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_SIZE), UDM_SETPOS, 0, MAKELPARAM(theApp.GetConfigI("osd_fontsize"), 0));

	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_SPEED), UDM_SETRANGE, 0, MAKELPARAM(10, 2));
	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_SPEED), UDM_SETPOS, 0, MAKELPARAM(theApp.GetConfigI("osd_log_speed"), 0));

	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_MAX_LOG), UDM_SETRANGE, 0, MAKELPARAM(20, 1));
	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_MAX_LOG), UDM_SETPOS, 0, MAKELPARAM(theApp.GetConfigI("osd_max_log_messages"), 0));

	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_FONT_EDIT), WM_SETTEXT, 0, (LPARAM)theApp.GetConfigS("osd_fontname").c_str());

	AddTooltip(IDC_OSD_MAX_LOG);
	AddTooltip(IDC_OSD_MAX_LOG_EDIT);
	AddTooltip(IDC_OSD_MONITOR);

	UpdateControls();
}

void GSOSDDlg::UpdateControls()
{
	SendMessage(GetDlgItem(m_hWnd, IDC_OSD_TRANSPARENCY_SLIDER), TBM_SETPOS, TRUE, m_transparency);

	char text[8] = { 0 };
	sprintf(text, "%d", m_transparency);
	SetDlgItemText(m_hWnd, IDC_OSD_TRANSPARENCY_TEXT, text);
}

bool GSOSDDlg::OnMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_HSCROLL:
	{
		if ((HWND)lParam == GetDlgItem(m_hWnd, IDC_OSD_TRANSPARENCY_SLIDER))
		{
			char text[8] = { 0 };

			m_transparency = SendMessage(GetDlgItem(m_hWnd, IDC_OSD_TRANSPARENCY_SLIDER), TBM_GETPOS, 0, 0);

			sprintf(text, "%d", m_transparency);
			SetDlgItemText(m_hWnd, IDC_OSD_TRANSPARENCY_TEXT, text);
		}
	} break;

	case WM_COMMAND:
	{
		int id = LOWORD(wParam);

		switch (id)
		{
		case IDOK:
		{
			INT_PTR data;

			theApp.SetConfig("osd_fontsize", (int)SendMessage(GetDlgItem(m_hWnd, IDC_OSD_SIZE), UDM_GETPOS, 0, 0));
			theApp.SetConfig("osd_log_speed", (int)SendMessage(GetDlgItem(m_hWnd, IDC_OSD_SPEED), UDM_GETPOS, 0, 0));
			theApp.SetConfig("osd_max_log_messages", (int)SendMessage(GetDlgItem(m_hWnd, IDC_OSD_MAX_LOG), UDM_GETPOS, 0, 0));

			theApp.SetConfig("osd_log_enabled", (int)IsDlgButtonChecked(m_hWnd, IDC_OSD_LOG));
			theApp.SetConfig("osd_monitor_enabled", (int)IsDlgButtonChecked(m_hWnd, IDC_OSD_MONITOR));
			theApp.SetConfig("osd_indicator_enabled", (int)IsDlgButtonChecked(m_hWnd, IDC_OSD_INDICATOR));

			theApp.SetConfig("osd_transparency", m_transparency);

			// OSD Font
			int length = ((int)SendMessage(GetDlgItem(m_hWnd, IDC_OSD_FONT_EDIT), WM_GETTEXTLENGTH, 0, 0)) + 1;
			std::unique_ptr<char[]> buffer(new char[length]);
			SendMessage(GetDlgItem(m_hWnd, IDC_OSD_FONT_EDIT), WM_GETTEXT, (WPARAM)length, (LPARAM)buffer.get());
			theApp.SetConfig("osd_fontname", buffer.get());

			EndDialog(m_hWnd, id);
		} break;
		case IDC_OSD_FONT_BUTTON:
			if (HIWORD(wParam) == BN_CLICKED)
				OpenFileDialog(IDC_OSD_FONT_EDIT, "Select External Font");
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
