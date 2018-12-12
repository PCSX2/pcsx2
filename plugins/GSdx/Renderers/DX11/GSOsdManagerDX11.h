/*
 *	Copyright (C) 2018 PCSX2 Dev Team
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

#include "Renderers/Common/GSOsdManager.h"

class GSOsdManagerDX11 final : public GSOsdManager {

	CComPtr<IDWriteTextFormat> m_osd_text_format;
	CComPtr<ID2D1SolidColorBrush> m_osd_color;
	CComPtr<ID2D1DeviceContext> m_ctx_2d;
	CComPtr<IDWriteFactory> m_dwrite_factory;
	CComPtr<ID2D1Device> m_dev_2d;

	struct log_info {
		std::wstring msg;
		std::chrono::system_clock::time_point created;
	};

	std::vector<log_info> m_log;
	std::map<std::wstring, std::wstring> m_monitor;

	void RenderMonitor();
	void RenderLog();

public:
	GSOsdManagerDX11::GSOsdManagerDX11(ID3D11Device *dev);

	void Log(const char *utf8, uint32 color);
	void Monitor(const char *key, const char *value, uint32 color);
	void Release();
	void SetBitmap(IDXGISurface *surface, DXGI_FORMAT format);

	void Render(GSTexture* dt);
};