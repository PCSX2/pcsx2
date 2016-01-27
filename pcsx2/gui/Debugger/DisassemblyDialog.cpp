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

#include "DisassemblyDialog.h"
#include "DebugTools/DebugInterface.h"
#include "DebugTools/DisassemblyManager.h"
#include "DebugTools/Breakpoints.h"
#include "DebugTools/MipsStackWalk.h"
#include "BreakpointWindow.h"
#include "PathDefs.h"

#ifdef _WIN32
#include <Windows.h>
#endif

BEGIN_EVENT_TABLE(DisassemblyDialog, wxFrame)
   EVT_COMMAND( wxID_ANY, debEVT_SETSTATUSBARTEXT, DisassemblyDialog::onDebuggerEvent )
   EVT_COMMAND( wxID_ANY, debEVT_UPDATELAYOUT, DisassemblyDialog::onDebuggerEvent )
   EVT_COMMAND( wxID_ANY, debEVT_GOTOINMEMORYVIEW, DisassemblyDialog::onDebuggerEvent )
   EVT_COMMAND( wxID_ANY, debEVT_RUNTOPOS, DisassemblyDialog::onDebuggerEvent )
   EVT_COMMAND( wxID_ANY, debEVT_GOTOINDISASM, DisassemblyDialog::onDebuggerEvent )
   EVT_COMMAND( wxID_ANY, debEVT_STEPOVER, DisassemblyDialog::onDebuggerEvent )
   EVT_COMMAND( wxID_ANY, debEVT_STEPINTO, DisassemblyDialog::onDebuggerEvent )
   EVT_COMMAND( wxID_ANY, debEVT_STEPOUT, DisassemblyDialog::onDebuggerEvent )
   EVT_COMMAND( wxID_ANY, debEVT_UPDATE, DisassemblyDialog::onDebuggerEvent )
   EVT_COMMAND( wxID_ANY, debEVT_BREAKPOINTWINDOW, DisassemblyDialog::onDebuggerEvent )
   EVT_COMMAND( wxID_ANY, debEVT_MAPLOADED, DisassemblyDialog::onDebuggerEvent )
   EVT_SIZE(DisassemblyDialog::onSizeEvent)
   EVT_CLOSE( DisassemblyDialog::onClose )
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(CpuTabPage, wxPanel)
	EVT_LISTBOX_DCLICK( wxID_ANY, CpuTabPage::listBoxHandler)
END_EVENT_TABLE()

DebuggerHelpDialog::DebuggerHelpDialog(wxWindow* parent)
	: wxDialog(parent,wxID_ANY,L"Help")
{
	wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

	wxTextCtrl* textControl = new wxTextCtrl(this,wxID_ANY,L"",wxDefaultPosition,wxDefaultSize,
		wxTE_MULTILINE|wxTE_READONLY);
	textControl->SetMinSize(wxSize(400,300));
	auto fileName = PathDefs::GetDocs().ToString()+L"/debugger.txt";
	wxTextFile file(fileName);
	if (file.Open())
	{
		wxString text = file.GetFirstLine();
		while (!file.Eof())
		{
			text += file.GetNextLine()+L"\r\n";
		}

		textControl->SetLabel(text);
		textControl->SetSelection(0,0);
	}

	sizer->Add(textControl,1,wxEXPAND);
	SetSizerAndFit(sizer);
}


CpuTabPage::CpuTabPage(wxWindow* parent, DebugInterface* _cpu)
	: wxPanel(parent), cpu(_cpu)
{
	wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(mainSizer);
	
	leftTabs = new wxNotebook(this,wxID_ANY);
	bottomTabs = new wxNotebook(this,wxID_ANY);
	disassembly = new CtrlDisassemblyView(this,cpu);
	registerList = new CtrlRegisterList(leftTabs,cpu);
	functionList = new wxListBox(leftTabs,wxID_ANY,wxDefaultPosition,wxDefaultSize,0,NULL,wxBORDER_NONE|wxLB_SORT);
	memory = new CtrlMemView(bottomTabs,cpu);

	// create register list and disassembly section
	wxBoxSizer* middleSizer = new wxBoxSizer(wxHORIZONTAL);

	wxBoxSizer* miscStuffSizer = new wxBoxSizer(wxHORIZONTAL);
	cyclesText = new wxStaticText(this,wxID_ANY,L"");
	miscStuffSizer->Add(cyclesText,0,wxLEFT|wxTOP|wxBOTTOM,2);
	
	leftTabs->AddPage(registerList,L"Registers");
	leftTabs->AddPage(functionList,L"Functions");

	wxBoxSizer* registerSizer = new wxBoxSizer(wxVERTICAL);
	registerSizer->Add(miscStuffSizer,0);
	registerSizer->Add(leftTabs,1);
	
	middleSizer->Add(registerSizer,0,wxEXPAND|wxRIGHT,2);
	middleSizer->Add(disassembly,2,wxEXPAND);
	mainSizer->Add(middleSizer,3,wxEXPAND|wxBOTTOM,3);

	// create bottom section
	bottomTabs->AddPage(memory,L"Memory");

	breakpointList = new BreakpointList(bottomTabs,cpu,disassembly);
	bottomTabs->AddPage(breakpointList,L"Breakpoints");
	
	threadList = NULL;
	stackFrames = NULL;
	if (cpu == &r5900Debug)
	{
		threadList = new ThreadList(bottomTabs,cpu);
		bottomTabs->AddPage(threadList,L"Threads");

		stackFrames = new StackFramesList(bottomTabs,cpu,disassembly);
		bottomTabs->AddPage(stackFrames,L"Stack frames");
	}

	mainSizer->Add(bottomTabs,1,wxEXPAND);

	mainSizer->Layout();

	lastCycles = 0;
	loadCycles();
}

void CpuTabPage::reloadSymbolMap()
{
	functionList->Clear();

	auto funcs = symbolMap.GetAllSymbols(ST_FUNCTION);
	for (size_t i = 0; i < funcs.size(); i++)
	{
		wxString name = wxString(funcs[i].name.c_str(),wxConvUTF8);
		functionList->Append(name,(void*)funcs[i].address);
	}
}

void CpuTabPage::listBoxHandler(wxCommandEvent& event)
{
	int index = functionList->GetSelection();
	if (event.GetEventObject() == functionList && index >= 0)
	{
		uptr pos = (uptr) functionList->GetClientData(index);
		postEvent(debEVT_GOTOINDISASM,pos);
	}
}

void CpuTabPage::postEvent(wxEventType type, int value)
{
   wxCommandEvent event( type, GetId() );
   event.SetEventObject(this);
   event.SetClientData(cpu);
   event.SetInt(value);
   wxPostEvent(this,event);
}

void CpuTabPage::setBottomTabPage(wxWindow* win)
{
	for (size_t i = 0; i < bottomTabs->GetPageCount(); i++)
	{
		if (bottomTabs->GetPage(i) == win)
		{
			bottomTabs->SetSelection(i);
			break;
		}
	}
}

void CpuTabPage::update()
{
	breakpointList->reloadBreakpoints();

	if (threadList != NULL && cpu->isAlive())
	{
		threadList->reloadThreads();

		EEThread thread = threadList->getRunningThread();
		if (thread.tid != -1)
			stackFrames->loadStackFrames(thread);
	}
	Refresh();
}

void CpuTabPage::loadCycles()
{
	u32 cycles = cpu->getCycles();

	wchar_t str[64];
	swprintf(str,64,L"Ctr:  %u",cycles-lastCycles);
	cyclesText->SetLabel(str);
	lastCycles = cycles;
}

u32 CpuTabPage::getStepOutAddress()
{
	if (threadList == NULL)
		return (u32)-1;

	EEThread currentThread = threadList->getRunningThread();
	std::vector<MipsStackWalk::StackFrame> frames =
		MipsStackWalk::Walk(cpu,cpu->getPC(),cpu->getRegister(0,31),cpu->getRegister(0,29),
			currentThread.data.entry_init,currentThread.data.stack);

	if (frames.size() < 2)
		return (u32)-1;

	return frames[1].pc;
}

DisassemblyDialog::DisassemblyDialog(wxWindow* parent):
	wxFrame( parent, wxID_ANY, L"Debugger", wxDefaultPosition,wxDefaultSize,wxRESIZE_BORDER|wxCLOSE_BOX|wxCAPTION|wxSYSTEM_MENU ),
	currentCpu(NULL)
{
	int width = g_Conf->EmuOptions.Debugger.WindowWidth;
	int height = g_Conf->EmuOptions.Debugger.WindowHeight;

	topSizer = new wxBoxSizer( wxVERTICAL );
	wxPanel *panel = new wxPanel(this, wxID_ANY, 
		wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, _("panel"));
	panel->SetSizer(topSizer);

	// create top row
	wxBoxSizer* topRowSizer = new wxBoxSizer(wxHORIZONTAL);

	breakRunButton = new wxButton(panel, wxID_ANY, L"Run");
	Connect(breakRunButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(DisassemblyDialog::onBreakRunClicked));
	topRowSizer->Add(breakRunButton,0,wxRIGHT,8);

	stepIntoButton = new wxButton( panel, wxID_ANY, L"Step Into" );
	stepIntoButton->Enable(false);
	Connect( stepIntoButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DisassemblyDialog::onStepIntoClicked ) );
	topRowSizer->Add(stepIntoButton,0,wxBOTTOM,2);

	stepOverButton = new wxButton( panel, wxID_ANY, L"Step Over" );
	stepOverButton->Enable(false);
	Connect( stepOverButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DisassemblyDialog::onStepOverClicked ) );
	topRowSizer->Add(stepOverButton);
	
	stepOutButton = new wxButton( panel, wxID_ANY, L"Step Out" );
	stepOutButton->Enable(false);
	Connect( stepOutButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DisassemblyDialog::onStepOutClicked ) );
	topRowSizer->Add(stepOutButton,0,wxRIGHT,8);
	
	breakpointButton = new wxButton( panel, wxID_ANY, L"Breakpoint" );
	Connect( breakpointButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DisassemblyDialog::onBreakpointClick ) );
	topRowSizer->Add(breakpointButton);

	topSizer->Add(topRowSizer,0,wxLEFT|wxRIGHT|wxTOP,3);

	// create middle part of the window
	middleBook = new wxNotebook(panel,wxID_ANY);  
	middleBook->SetBackgroundColour(wxColour(0xFFF0F0F0));
	eeTab = new CpuTabPage(middleBook,&r5900Debug);
	iopTab = new CpuTabPage(middleBook,&r3000Debug);
	middleBook->AddPage(eeTab,L"R5900");
	middleBook->AddPage(iopTab,L"R3000");
	Connect(middleBook->GetId(),wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGED,wxCommandEventHandler( DisassemblyDialog::onPageChanging));
	topSizer->Add(middleBook,3,wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM,3);
	currentCpu = eeTab;

	CreateStatusBar(1);
	
	SetMinSize(wxSize(1000,600));
	panel->GetSizer()->Fit(this);

	if (width != 0 && height != 0)
		SetSize(width,height);

	setDebugMode(true,true);
}

void DisassemblyDialog::onSizeEvent(wxSizeEvent& event)
{
	if (event.GetEventType() == wxEVT_SIZE)
	{
		g_Conf->EmuOptions.Debugger.WindowWidth = event.GetSize().x;
		g_Conf->EmuOptions.Debugger.WindowHeight = event.GetSize().y;
	}

	event.Skip();
}

#ifdef _WIN32
WXLRESULT DisassemblyDialog::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
	switch (nMsg)
	{
	case WM_SHOWWINDOW:
		{
			WXHWND hwnd = GetHWND();

			u32 style = GetWindowLong((HWND)hwnd,GWL_STYLE);
			style &= ~(WS_MINIMIZEBOX|WS_MAXIMIZEBOX);
			SetWindowLong((HWND)hwnd,GWL_STYLE,style);

			u32 exStyle = GetWindowLong((HWND)hwnd,GWL_EXSTYLE);
			exStyle |= (WS_EX_CONTEXTHELP);
			SetWindowLong((HWND)hwnd,GWL_EXSTYLE,exStyle);
		}
		break;
	case WM_SYSCOMMAND:
		if (wParam == SC_CONTEXTHELP)
		{
			DebuggerHelpDialog help(this);
			help.ShowModal();
			return 0;
		}
		break;
	}

	return wxFrame::MSWWindowProc(nMsg,wParam,lParam);
}
#endif

void DisassemblyDialog::onBreakRunClicked(wxCommandEvent& evt)
{	
	if (r5900Debug.isCpuPaused())
	{
		// If the current PC is on a breakpoint, the user doesn't want to do nothing.
		CBreakPoints::SetSkipFirst(r5900Debug.getPC());
		r5900Debug.resumeCpu();
	} else {
		r5900Debug.pauseCpu();
		gotoPc();
	}
}

void DisassemblyDialog::onStepOverClicked(wxCommandEvent& evt)
{	
	stepOver();
}

void DisassemblyDialog::onStepIntoClicked(wxCommandEvent& evt)
{	
	stepInto();
}

void DisassemblyDialog::onStepOutClicked(wxCommandEvent& evt)
{
	stepOut();
}

void DisassemblyDialog::onPageChanging(wxCommandEvent& evt)
{
	wxNotebook* notebook = (wxNotebook*)wxWindow::FindWindowById(evt.GetId());
	wxWindow* currentPage = notebook->GetCurrentPage();

	if (currentPage == eeTab)
		currentCpu = eeTab;
	else if (currentPage == iopTab)
		currentCpu = iopTab;
	else
		currentCpu = NULL;

	if (currentCpu != NULL)
	{
		currentCpu->getDisassembly()->SetFocus();
		currentCpu->update();
	}
}

void DisassemblyDialog::stepOver()
{
	if (!r5900Debug.isAlive() || !r5900Debug.isCpuPaused() || currentCpu == NULL)
		return;
	

	// todo: breakpoints for iop
	if (currentCpu != eeTab)
		return;

	CtrlDisassemblyView* disassembly = currentCpu->getDisassembly();

	// If the current PC is on a breakpoint, the user doesn't want to do nothing.
	CBreakPoints::SetSkipFirst(r5900Debug.getPC());
	u32 currentPc = r5900Debug.getPC();

	MIPSAnalyst::MipsOpcodeInfo info = MIPSAnalyst::GetOpcodeInfo(&r5900Debug,r5900Debug.getPC());
	u32 breakpointAddress = currentPc+disassembly->getInstructionSizeAt(currentPc);
	if (info.isBranch)
	{
		if (info.isConditional == false)
		{
			if (info.isLinkedBranch)	// jal, jalr
			{
				// it's a function call with a delay slot - skip that too
				breakpointAddress += 4;
			} else {					// j, ...
				// in case of absolute branches, set the breakpoint at the branch target
				breakpointAddress = info.branchTarget;
			}
		} else {						// beq, ...
			if (info.conditionMet)
			{
				breakpointAddress = info.branchTarget;
			} else {
				breakpointAddress = currentPc+2*4;
				disassembly->scrollStepping(breakpointAddress);
			}
		}
	} else {
		disassembly->scrollStepping(breakpointAddress);
	}

	CBreakPoints::AddBreakPoint(breakpointAddress,true);
	r5900Debug.resumeCpu();
}


void DisassemblyDialog::stepInto()
{
	if (!r5900Debug.isAlive() || !r5900Debug.isCpuPaused() || currentCpu == NULL)
		return;
	
	// todo: breakpoints for iop
	if (currentCpu != eeTab)
		return;

	CtrlDisassemblyView* disassembly = currentCpu->getDisassembly();

	// If the current PC is on a breakpoint, the user doesn't want to do nothing.
	CBreakPoints::SetSkipFirst(r5900Debug.getPC());
	u32 currentPc = r5900Debug.getPC();

	MIPSAnalyst::MipsOpcodeInfo info = MIPSAnalyst::GetOpcodeInfo(&r5900Debug,r5900Debug.getPC());
	u32 breakpointAddress = currentPc+disassembly->getInstructionSizeAt(currentPc);
	if (info.isBranch)
	{
		if (info.isConditional == false)
		{
			breakpointAddress = info.branchTarget;
		} else {
			if (info.conditionMet)
			{
				breakpointAddress = info.branchTarget;
			} else {
				breakpointAddress = currentPc+2*4;
				disassembly->scrollStepping(breakpointAddress);
			}
		}
	}

	if (info.isSyscall)
		breakpointAddress = info.branchTarget;

	CBreakPoints::AddBreakPoint(breakpointAddress,true);
	r5900Debug.resumeCpu();
}

void DisassemblyDialog::stepOut()
{
	if (!r5900Debug.isAlive() || !r5900Debug.isCpuPaused() || currentCpu == NULL)
		return;

	u32 addr = currentCpu->getStepOutAddress();
	if (addr == (u32)-1)
		return;

	CBreakPoints::AddBreakPoint(addr,true);
	r5900Debug.resumeCpu();
}

void DisassemblyDialog::onBreakpointClick(wxCommandEvent& evt)
{
	if (currentCpu == NULL)
		return;

	BreakpointWindow bpw(this,currentCpu->getCpu());
	if (bpw.ShowModal() == wxID_OK)
	{
		bpw.addBreakpoint();
		update();
	}
}

void DisassemblyDialog::onDebuggerEvent(wxCommandEvent& evt)
{
	wxEventType type = evt.GetEventType();
	if (type == debEVT_SETSTATUSBARTEXT)
	{
		DebugInterface* cpu = reinterpret_cast<DebugInterface*>(evt.GetClientData());
		if (cpu != NULL && currentCpu != NULL && cpu == currentCpu->getCpu())
			GetStatusBar()->SetLabel(evt.GetString());
	} else if (type == debEVT_UPDATELAYOUT)
	{
		if (currentCpu != NULL)
			currentCpu->GetSizer()->Layout();
		topSizer->Layout();
		update();
	} else if (type == debEVT_GOTOINMEMORYVIEW)
	{
		if (currentCpu != NULL)
		{
			currentCpu->showMemoryView();
			currentCpu->getMemoryView()->gotoAddress(evt.GetInt());
			currentCpu->getDisassembly()->SetFocus();
		}
	} else if (type == debEVT_RUNTOPOS)
	{
		// todo: breakpoints for iop
		if (currentCpu != eeTab)
			return;
		CBreakPoints::AddBreakPoint(evt.GetInt(),true);
		currentCpu->getCpu()->resumeCpu();
	} else if (type == debEVT_GOTOINDISASM)
	{
		if (currentCpu != NULL)
		{
			u32 pos = evt.GetInt();
			currentCpu->getDisassembly()->gotoAddress(pos);
			currentCpu->getDisassembly()->SetFocus();
			update();
		}
	} else if (type == debEVT_STEPOVER)
	{
		if (currentCpu != NULL)
			stepOver();
	} else if (type == debEVT_STEPINTO)
	{
		if (currentCpu != NULL)
			stepInto();
	} else if (type == debEVT_UPDATE)
	{
		update();
	} else if (type == debEVT_BREAKPOINTWINDOW)
	{
		wxCommandEvent evt;
		onBreakpointClick(evt);
	} else if (type == debEVT_MAPLOADED)
	{
		eeTab->reloadSymbolMap();
		iopTab->reloadSymbolMap();
	} else if (type == debEVT_STEPOUT)
	{
		if (currentCpu != NULL)
			stepOut();
	}
}

void DisassemblyDialog::onClose(wxCloseEvent& evt)
{
	Hide();
}

void DisassemblyDialog::update()
{
	if (currentCpu != NULL)
	{
		stepOverButton->Enable(true);
		breakpointButton->Enable(true);
		currentCpu->update();
	} else {
		stepOverButton->Enable(false);
		breakpointButton->Enable(false);
	}
}

void DisassemblyDialog::reset()
{
	eeTab->getDisassembly()->clearFunctions();
	eeTab->reloadSymbolMap();
	iopTab->getDisassembly()->clearFunctions();
	iopTab->reloadSymbolMap();
}

void DisassemblyDialog::gotoPc()
{
	eeTab->getDisassembly()->gotoPc();
	iopTab->getDisassembly()->gotoPc();
}

void DisassemblyDialog::setDebugMode(bool debugMode, bool switchPC)
{
	bool running = r5900Debug.isAlive();
	
	eeTab->Enable(running);
	iopTab->Enable(running);

	if (running)
	{
		if (currentCpu == NULL)
		{
			wxWindow* currentPage = middleBook->GetCurrentPage();

			if (currentPage == eeTab)
				currentCpu = eeTab;
			else if (currentPage == iopTab)
				currentCpu = iopTab;

			if (currentCpu != NULL)
				currentCpu->update();
		}

		if (debugMode)
		{
			CBreakPoints::ClearTemporaryBreakPoints();
			breakRunButton->SetLabel(L"Run");

			stepOverButton->Enable(true);
			stepIntoButton->Enable(true);
			stepOutButton->Enable(currentCpu == eeTab);

			if (switchPC || CBreakPoints::GetBreakpointTriggered())
				gotoPc();
			
			if (CBreakPoints::GetBreakpointTriggered())
			{
				if (currentCpu != NULL)
					currentCpu->getDisassembly()->SetFocus();
				CBreakPoints::SetBreakpointTriggered(false);
			}

			if (currentCpu != NULL)
				currentCpu->loadCycles();
		} else {
			breakRunButton->SetLabel(L"Break");

			stepIntoButton->Enable(false);
			stepOverButton->Enable(false);
			stepOutButton->Enable(false);
		}
	} else {
		breakRunButton->SetLabel(L"Run");
		stepIntoButton->Enable(false);
		stepOverButton->Enable(false);
		stepOutButton->Enable(false);
		currentCpu = NULL;
	}

	update();
}
