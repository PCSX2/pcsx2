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

#include "PrecompiledHeader.h"
#include "DebugEvents.h"

wxDEFINE_EVENT(debEVT_SETSTATUSBARTEXT, wxCommandEvent);
wxDEFINE_EVENT(debEVT_UPDATELAYOUT, wxCommandEvent);
wxDEFINE_EVENT(debEVT_GOTOADDRESS, wxCommandEvent);
wxDEFINE_EVENT(debEVT_GOTOINMEMORYVIEW, wxCommandEvent);
wxDEFINE_EVENT(debEVT_REFERENCEMEMORYVIEW, wxCommandEvent);
wxDEFINE_EVENT(debEVT_GOTOINDISASM, wxCommandEvent);
wxDEFINE_EVENT(debEVT_RUNTOPOS, wxCommandEvent);
wxDEFINE_EVENT(debEVT_MAPLOADED, wxCommandEvent);
wxDEFINE_EVENT(debEVT_STEPOVER, wxCommandEvent);
wxDEFINE_EVENT(debEVT_STEPINTO, wxCommandEvent);
wxDEFINE_EVENT(debEVT_STEPOUT, wxCommandEvent);
wxDEFINE_EVENT(debEVT_UPDATE, wxCommandEvent);
wxDEFINE_EVENT(debEVT_BREAKPOINTWINDOW, wxCommandEvent);

bool parseExpression(const char* exp, DebugInterface* cpu, u64& dest)
{
	PostfixExpression postfix;
	if (!cpu->initExpression(exp,postfix)) return false;
	return cpu->parseExpression(postfix,dest);
}

void displayExpressionError(wxWindow* parent)
{
	wxMessageBox(wxString(getExpressionError(),wxConvUTF8),L"Invalid expression",wxICON_ERROR);
}

bool executeExpressionWindow(wxWindow* parent, DebugInterface* cpu, u64& dest, const wxString& defaultValue)
{
	wxString result = wxGetTextFromUser(L"Enter expression",L"Expression",defaultValue,parent);
	if (result.empty())
		return false;

	wxCharBuffer expression = result.ToUTF8();
	if (!parseExpression(expression, cpu, dest))
	{
		displayExpressionError(parent);
		return false;
	}

	return true;
}
