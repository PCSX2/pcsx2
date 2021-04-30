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

#pragma once

#include "GSDialog.h"
#include "GSSetting.h"

class GSShaderDlg : public GSDialog
{
	int m_saturation;
	int m_brightness;
	int m_contrast;

	void UpdateControls();

protected:
	void OnInit();
	bool OnMessage(UINT message, WPARAM wParam, LPARAM lParam);

public:
	GSShaderDlg();
};

class GSHacksDlg : public GSDialog
{
	int m_old_skipdraw_offset;
	int m_old_skipdraw;

	void UpdateControls();

protected:
	void OnInit();
	bool OnMessage(UINT message, WPARAM wParam, LPARAM lParam);

public:
	GSHacksDlg();
};

class GSOSDDlg : public GSDialog
{
	struct
	{
		int r;
		int g;
		int b;
		int a;
	} m_color;

	void UpdateControls();

protected:
	void OnInit();
	bool OnMessage(UINT message, WPARAM wParam, LPARAM lParam);

public:
	GSOSDDlg();
};

class GSSettingsDlg : public GSDialog
{

	struct Adapter
	{
		std::string name;
		std::string id;
		D3D_FEATURE_LEVEL level;
		Adapter(const std::string& n, const std::string& i, const D3D_FEATURE_LEVEL& l)
			: name(n)
			, id(i)
			, level(l)
		{
		}
	};

	std::vector<GSSetting> m_renderers;
	std::vector<Adapter> m_d3d11_adapters;
	std::vector<Adapter>* m_current_adapters;
	std::string m_last_selected_adapter_id;

	std::vector<Adapter> EnumerateD3D11Adapters();

	void UpdateAdapters();
	void UpdateControls();

protected:
	void OnInit();
	bool OnCommand(HWND hWnd, UINT id, UINT code);

public:
	GSSettingsDlg();
};
