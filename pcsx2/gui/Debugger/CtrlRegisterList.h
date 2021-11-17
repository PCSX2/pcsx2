/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#include "gui/MSWstuff.h" // Required for MSW_GetDPIScale()
#include "DebugTools/DebugInterface.h"
#include "DebugTools/DisassemblyManager.h"

class CtrlRegisterList : public wxScrolledWindow
{
public:
	CtrlRegisterList(wxWindow* parent, DebugInterface* _cpu);
	~CtrlRegisterList();

	void mouseEvent(wxMouseEvent& evt);
	void keydownEvent(wxKeyEvent& evt);
	void onPopupClick(wxCommandEvent& evt);
	void sizeEvent(wxSizeEvent& evt);
	void redraw();
	wxDECLARE_EVENT_TABLE();

	virtual wxSize GetMinClientSize() const
	{
		wxSize optimalSize = getOptimalSize();
		if (GetWindowStyle() & wxVSCROLL)
			optimalSize.x += wxSystemSettings::GetMetric(wxSYS_VSCROLL_X);

		return wxSize(optimalSize.x * MSW_GetDPIScale(), 0);
	}

	virtual wxSize DoGetBestClientSize() const
	{
		return GetMinClientSize();
	}

private:
	enum RegisterChangeMode
	{
		LOWER64,
		UPPER64,
		CHANGE32
	};

	void OnDraw(wxDC& dc);
	void refreshChangedRegs();
	void setCurrentRow(int row);
	void ensureVisible(int index);
	void changeValue(RegisterChangeMode mode);
	wxSize getOptimalSize() const;

	void postEvent(wxEventType type, wxString text);
	void postEvent(wxEventType type, int value);

	struct ChangedReg
	{
		u128 oldValue;
		bool changed[4];
	};

	std::vector<ChangedReg*> changedCategories;
	std::vector<int> startPositions;
	std::vector<int> currentRows;

	DebugInterface* cpu;
	int rowHeight, charWidth;
	u32 lastPc;
	int category;
	bool resolvePointerStrings;
	bool displayVU0FAsFloat;
};
