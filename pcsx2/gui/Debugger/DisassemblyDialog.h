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
#include <wx/notebook.h>
#include "gui/App.h"

#include "CtrlDisassemblyView.h"
#include "CtrlRegisterList.h"
#include "CtrlMemView.h"
#include "CtrlMemSearch.h"
#include "DebugEvents.h"
#include "DebuggerLists.h"
#include "gui/MSWstuff.h"

class DebuggerHelpDialog: public wxDialog
{
public:
	DebuggerHelpDialog(wxWindow* parent);
};

inline int getDebugFontWidth()
{
	return (int) ceil(g_Conf->EmuOptions.Debugger.FontWidth*MSW_GetDPIScale());
}

inline int getDebugFontHeight()
{
	return (int)ceil(g_Conf->EmuOptions.Debugger.FontHeight*MSW_GetDPIScale());
}

class CpuTabPage: public wxPanel
{
public:
	CpuTabPage(wxWindow* parent, DebugInterface* _cpu);
	DebugInterface* getCpu() { return cpu; };
	CtrlDisassemblyView* getDisassembly() { return disassembly; };
	CtrlRegisterList* getRegisterList() { return registerList; };
	CtrlMemView* getMemoryView() { return memory; };
	CtrlMemSearch* getMemorySearch() { return memorySearch; };
	wxNotebook* getBottomTabs() { return bottomTabs; };
	void update();
	void showMemoryView() { setBottomTabPage(memory); };
	void loadCycles();
	void reloadSymbolMap();
	void clearSymbolMap();
	u32 getStepOutAddress();

	void listBoxHandler(wxCommandEvent& event);
	wxDECLARE_EVENT_TABLE();
private:
	void setBottomTabPage(wxWindow* win);
	void postEvent(wxEventType type, int value);

	DebugInterface* cpu;
	CtrlDisassemblyView* disassembly;
	CtrlRegisterList* registerList;
	wxListBox* functionList;
	CtrlMemView* memory;
	CtrlMemSearch* memorySearch;
	wxNotebook* bottomTabs;
	wxNotebook* leftTabs;
	BreakpointList* breakpointList;
	wxStaticText* cyclesText;
	ThreadList* threadList;
	StackFramesList* stackFrames;
	u32 lastCycles;
	u32 symbolCount;
};

class DisassemblyDialog : public wxFrame
{
public:
	DisassemblyDialog( wxWindow* parent=NULL );
	virtual ~DisassemblyDialog() = default;

	static wxString GetNameStatic() { return L"DisassemblyDialog"; }
	wxString GetDialogName() const { return GetNameStatic(); }
	
	void update();
	void reset();
	void populate();
	void setDebugMode(bool debugMode, bool switchPC);

	wxDECLARE_EVENT_TABLE();
protected:
	void onBreakRunClicked(wxCommandEvent& evt);
	void onStepOverClicked(wxCommandEvent& evt);
	void onStepIntoClicked(wxCommandEvent& evt);
	void onStepOutClicked(wxCommandEvent& evt);
	void onHelpClicked(wxCommandEvent& evt);
	void onDebuggerEvent(wxCommandEvent& evt);
	void onPageChanging(wxCommandEvent& evt);
	void onBreakpointClicked(wxCommandEvent& evt);
	void onSizeEvent(wxSizeEvent& event);
	void onClose(wxCloseEvent& evt);
	void stepOver();
	void stepInto();
	void stepOut();
	void gotoPc();
private:
	CpuTabPage* eeTab;
	CpuTabPage* iopTab;
	CpuTabPage* currentCpu;
	wxNotebook* middleBook;

	wxBoxSizer* topSizer;
	wxButton *breakRunButton, *stepIntoButton, *stepOverButton, *stepOutButton, *breakpointButton, *helpButton;
};
