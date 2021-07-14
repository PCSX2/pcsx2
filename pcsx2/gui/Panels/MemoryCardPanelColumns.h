/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

#include "AppCommon.h"

#include <wx/listctrl.h>

#include <vector>

struct ListViewColumnInfo
{
	const std::string key;
	const wxChar* name;
	int width;
	wxListColumnFormat align;
};

// TODO - config - Please avoid spaces or '/'s in the key (first arg) as it makes the wxIni keys a nightmare
static const std::vector<ListViewColumnInfo> LIST_VIEW_DEFAULT_COLUMN_INFO =
{
	{"PS2", _("PS2 Port"), 160, wxLIST_FORMAT_LEFT},
	//{"PortStatus", _("Port status"), 80, wxLIST_FORMAT_LEFT},
	{"MemoryCard", _("Memory card"), 145, wxLIST_FORMAT_LEFT},
	{"CardSize", _("Card size"), 60, wxLIST_FORMAT_LEFT},
	{"UsableFormatted", _("Usable / Formatted"), 115, wxLIST_FORMAT_LEFT},
	{"Type", _("Type"), 45, wxLIST_FORMAT_LEFT},
	{"LastModified", _("Last Modified"), 90, wxLIST_FORMAT_LEFT},
	{"CreatedOn", _("Created on"), 80, wxLIST_FORMAT_LEFT},
};
