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
#include "DebugTools/DebugInterface.h"
#include "DebugTools/DisassemblyManager.h"
#include "gui/PersistentThread.h"

enum class SEARCHTYPE
{
	BYTE,
	WORD,
	DWORD,
	QWORD,
	FLOAT,
	DOUBLE,
	STRING,
};

class CtrlMemSearch;

class SearchThread : public Threading::pxThread
{
	CtrlMemSearch* m_parent;
	DebugInterface* m_cpu;
public:
	virtual ~SearchThread()
	{
		try
		{
			pxThread::Cancel();
		}
		DESTRUCTOR_CATCHALL
	}
	SearchThread(CtrlMemSearch* parent, DebugInterface* cpu) { m_parent = parent; m_cpu = cpu; };

	SEARCHTYPE m_type;
	// Only guaranteed to be valid if m_type == STRING
	std::string m_value_string;
	u64 m_value;
	bool m_signed;
	u64 m_start,m_end;
protected:
	void ExecuteTaskInThread();
};

class CtrlMemSearch : public wxWindow
{
public:
	CtrlMemSearch(wxWindow* parent, DebugInterface* _cpu);

	std::vector<u32> m_searchResults;
	std::mutex m_searchResultsMutex;
	wxDECLARE_EVENT_TABLE();


private:
	void setRowSize(int bytesInRow);
	void postEvent(wxEventType type, wxString text);
	void postEvent(wxEventType type, int value);
	void onPopupClick(wxCommandEvent& evt);
	void focusEvent(wxFocusEvent& evt) { Refresh(); };
	void sizeEvent(wxSizeEvent& evt)
	{
		// Why in the world do I have to do this.
		// Without this the window isn't redrawn during a size event
		Refresh(); // Reloads the window, fixes the imporper drawing but ruins the layout
		Layout(); // Fix the layout of the window
	};
	void onSearchFinished(wxCommandEvent& evt);
	void onSearchNext(wxCommandEvent& evt);
	void onSearchPrev(wxCommandEvent& evt);
	void onSearchTypeChanged(wxCommandEvent& evt);

	void Search(wxCommandEvent& evt);
	const u8 SEARCHTYPEBITS[8] = {8,16,32,64,32,64,0};
	
	DebugInterface* cpu;
	wxFont font, underlineFont;
	wxTextCtrl* txtSearch;
	wxButton* btnSearch;
	wxComboBox* cmbSearchType;
	wxCheckBox* chkHexadecimal;
	wxTextCtrl* txtMemoryStart;
	wxTextCtrl* txtMemoryEnd;
	wxRadioButton* radBigEndian;
	wxRadioButton* radLittleEndian;
	wxStaticText* lblSearchHits;
	wxStaticText* lblSearchHitSelected;
	wxButton* btnNext;
	wxButton* btnPrev;

	u64 m_searchIter = -1;
	std::unique_ptr<SearchThread> m_SearchThread;
	wxMenu menu;
};
