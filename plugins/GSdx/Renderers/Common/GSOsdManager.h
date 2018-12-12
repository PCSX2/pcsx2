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

#include "stdafx.h"
#include "GSdx.h"
#include "Renderers/Common/GSVertex.h"
#include "Renderers/Common/GSTexture.h"

class GSOsdManager
{
protected:
	bool m_log_enabled;
	bool m_monitor_enabled;

	float m_opacity;

	int m_max_onscreen_messages;
	int m_log_timeout;
	int m_size;

	uint32 m_color;

	GSVector2i m_real_size;

public:
	GSOsdManager();
	virtual ~GSOsdManager() { };

	virtual void Log(const char *utf8, uint32 color) { };
	virtual void Monitor(const char *key, const char *value, uint32 color) { };

	void SetSize(int x, int y);

	virtual void Release() { };
#ifdef _WIN32
	virtual void SetBitmap(IDXGISurface *surface, DXGI_FORMAT format) { };
#endif

	virtual void Render(GSTexture* dt) { };
};