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
#include "GSOsdManager.h"

GSOsdManager::GSOsdManager()
{
	m_log_enabled = theApp.GetConfigB("osd_log_enabled");
	m_log_timeout = std::max(2, std::min(theApp.GetConfigI("osd_log_timeout"), 10));
	m_monitor_enabled = theApp.GetConfigB("osd_monitor_enabled");
	m_opacity = std::max(0, std::min(theApp.GetConfigI("osd_color_opacity"), 100)) * 0.01f;
	m_max_onscreen_messages = theApp.GetConfigI("osd_max_log_messages");
	m_size = theApp.GetConfigI("osd_fontsize");
}

void GSOsdManager::SetSize(int x, int y)
{
	m_real_size.x = x;
	m_real_size.y = y;
}