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

#include "stdafx.h"
#include "GSOsdManagerDX11.h"

GSOsdManagerDX11::GSOsdManagerDX11(ID3D11Device *dev)
{
	// NOTE: workaround for Fraps/Afterburner
	// Having dwrite/d2d enabled prevents them
	// from properly detecting the window.
	// Disable log and monitor to fix.
	if (!m_log_enabled && !m_monitor_enabled)
		return;

	HRESULT hr = E_FAIL;

	D2D1_FACTORY_OPTIONS d2d_factory_options = {};

#ifdef DEBUG
	d2d_factory_options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

	CComPtr<IDXGIDevice> dxgid;
	CComPtr<ID2D1Factory1> d2d_factory;

	hr = D2D1CreateFactory(
		D2D1_FACTORY_TYPE_SINGLE_THREADED,
		__uuidof(ID2D1Factory1),
		&d2d_factory_options,
		(void **)&d2d_factory
	);

	if (FAILED(hr))
	{
		fprintf(stderr, "NOTE: DX11 OSD can only be enabled on Windows 7 with the Platform Update.\n");
		fprintf(stderr, "Disabling OSD will remove this warning.\n");
		fprintf(stderr, "https://support.microsoft.com/en-us/help/2670838/platform-update-for-windows-7-sp1-and-windows-server-2008-r2-sp1\n");
		return;
	}

	hr = dev->QueryInterface(IID_PPV_ARGS(&dxgid));

	if (FAILED(hr)) return;

	hr = DWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(IDWriteFactory),
		reinterpret_cast<IUnknown **>(&m_dwrite_factory)
	);

	if (FAILED(hr)) return;

	hr = d2d_factory->CreateDevice(dxgid, &m_dev_2d);

	if (FAILED(hr)) return;

	m_dev_2d->CreateDeviceContext(
		D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
		&m_ctx_2d
	);

	int r = theApp.GetConfigI("osd_color_r");
	int g = theApp.GetConfigI("osd_color_g");
	int b = theApp.GetConfigI("osd_color_b");

	uint32 color = b | (g << 8) | (r << 16);

	m_ctx_2d->CreateSolidColorBrush(
		D2D1::ColorF(color, m_opacity),
		&m_osd_color
	);

	m_dwrite_factory->CreateTextFormat(
		L"tahoma",
		nullptr,
		DWRITE_FONT_WEIGHT_LIGHT,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		(float)m_size,
		L"en-US", // TODO
		&m_osd_text_format
	);
}

void GSOsdManagerDX11::Log(const char *utf8, uint32 color)
{
	std::wostringstream s;

	s << utf8;

	m_log.push_back(
		log_info {
			s.str(), std::chrono::system_clock::now()
		}
	);
}

void GSOsdManagerDX11::Monitor(const char *key, const char *value, uint32 color)
{
	std::wostringstream k;
	std::wostringstream v;

	k << key;
	v << value;

	m_monitor[k.str()] = v.str();
}

void GSOsdManagerDX11::Release()
{
	if (m_dev_2d == NULL)
		return;

	m_ctx_2d->SetTarget(nullptr);
}

void GSOsdManagerDX11::SetBitmap(IDXGISurface *surface, DXGI_FORMAT format)
{
	if (m_dev_2d == NULL)
		return;

	D2D1_BITMAP_PROPERTIES1 bitmap_props = {};

	bitmap_props.pixelFormat.format = format;
	bitmap_props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
	bitmap_props.dpiX = 96.0f;
	bitmap_props.dpiY = 96.0f;
	bitmap_props.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
	bitmap_props.colorContext = nullptr;

	CComPtr<ID2D1Bitmap1> bitmap;

	HRESULT hr = E_FAIL;
	hr = m_ctx_2d->CreateBitmapFromDxgiSurface(
		surface, &bitmap_props, &bitmap
	);

	if (FAILED(hr))
		return;

	m_ctx_2d->SetTarget(bitmap);
	m_ctx_2d->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
}

void GSOsdManagerDX11::RenderMonitor()
{
	if (!m_monitor_enabled)
		return;

	std::wostringstream key_out;
	std::wostringstream val_out;

	CComPtr<IDWriteTextLayout> key_layout;
	CComPtr<IDWriteTextLayout> val_layout;

	// loop backwards to match OGL
	for (auto it = m_monitor.rbegin(); it != m_monitor.rend();)
	{
		std::wstring key = it->first;
		std::wstring val = it->second;

		key_out << key;
		val_out << val;

		++it;

		if (it != m_monitor.rend())
		{
			key_out << std::endl;
			val_out << std::endl;
		}
	}

	m_dwrite_factory->CreateTextLayout(
		val_out.str().c_str(),
		val_out.str().size(),
		m_osd_text_format,
		(float)m_real_size.x,
		(float)m_real_size.y,
		&val_layout
	);

	m_dwrite_factory->CreateTextLayout(
		key_out.str().c_str(),
		key_out.str().size(),
		m_osd_text_format,
		(float)m_real_size.x,
		(float)m_real_size.y,
		&key_layout
	);

	DWRITE_TEXT_METRICS val_metrics;
	val_layout->GetMetrics(&val_metrics);

	DWRITE_TEXT_METRICS key_metrics;
	key_layout->GetMetrics(&key_metrics);

	// offsets are arbitrary and only exist
	// to match OGL a bit better
	float val_position_x = m_real_size.x - val_metrics.width  - 10.0f;
	float val_position_y = m_real_size.y - val_metrics.height - 30.0f;

	m_ctx_2d->DrawTextLayout(
		D2D1::Point2F(
			val_position_x,
			val_position_y
		),
		val_layout,
		m_osd_color
	);

	m_ctx_2d->DrawTextLayout(
		D2D1::Point2F(
			val_position_x - key_metrics.width - 10.0f,
			val_position_y
		),
		key_layout,
		m_osd_color
	);
}

void GSOsdManagerDX11::RenderLog()
{
	if (!m_log_enabled)
		return;

	std::wostringstream out;
	CComPtr<IDWriteTextLayout> text_layout;

	size_t messages = m_log.size();
	for (auto it = m_log.begin(); it != m_log.end();)
	{

		std::chrono::seconds elapsed =
			std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::system_clock::now() - it->created
			);

		if (elapsed > std::chrono::seconds(m_log_timeout) || messages > m_max_onscreen_messages)
		{
			it = m_log.erase(it);
			--messages;
			continue;
		}

		out << it->msg << std::endl;
		++it;
	}

	m_dwrite_factory->CreateTextLayout(
		out.str().c_str(),
		out.str().size(),
		m_osd_text_format,
		(float)m_real_size.x,
		(float)m_real_size.y,
		&text_layout
	);

	m_ctx_2d->DrawTextLayout(
		D2D1::Point2F(5.0f, 1.0f),
		text_layout,
		m_osd_color
	);
}

void GSOsdManagerDX11::Render(GSTexture* dt)
{
	if (m_dev_2d == NULL)
		return;

	m_ctx_2d->BeginDraw();

	RenderLog();
	RenderMonitor();

	m_ctx_2d->EndDraw();
}