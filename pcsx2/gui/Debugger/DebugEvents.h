/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2014  PCSX2 Dev Team
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
#include <wx/wx.h>
#include "DebugTools/DebugInterface.h"

wxDECLARE_EVENT(debEVT_SETSTATUSBARTEXT, wxCommandEvent);
wxDECLARE_EVENT(debEVT_UPDATELAYOUT, wxCommandEvent);
wxDECLARE_EVENT(debEVT_GOTOADDRESS, wxCommandEvent);
wxDECLARE_EVENT(debEVT_GOTOINMEMORYVIEW, wxCommandEvent);
wxDECLARE_EVENT(debEVT_REFERENCEMEMORYVIEW, wxCommandEvent);
wxDECLARE_EVENT(debEVT_GOTOINDISASM, wxCommandEvent);
wxDECLARE_EVENT(debEVT_RUNTOPOS, wxCommandEvent);
wxDECLARE_EVENT(debEVT_MAPLOADED, wxCommandEvent);
wxDECLARE_EVENT(debEVT_STEPOVER, wxCommandEvent);
wxDECLARE_EVENT(debEVT_STEPINTO, wxCommandEvent);
wxDECLARE_EVENT(debEVT_STEPOUT, wxCommandEvent);
wxDECLARE_EVENT(debEVT_UPDATE, wxCommandEvent);
wxDECLARE_EVENT(debEVT_BREAKPOINTWINDOW, wxCommandEvent);

bool executeExpressionWindow(wxWindow* parent, DebugInterface* cpu, u64& dest, const wxString& defaultValue = wxEmptyString);
