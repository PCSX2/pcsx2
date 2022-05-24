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

#include "PrecompiledHeader.h"

#include "gui/App.h"
#include "gui/MainFrame.h"
#include "gui/PathDefs.h"
#include "DisassemblyDialog.h"
#include "DebugTools/DebugInterface.h"
#include "DebugTools/DisassemblyManager.h"
#include "DebugTools/Breakpoints.h"
#include "DebugTools/MipsStackWalk.h"
#include "BreakpointWindow.h"
#include "wx/busyinfo.h"

#ifdef _WIN32
#include <Windows.h>
#endif

wxBEGIN_EVENT_TABLE(DisassemblyDialog, wxFrame)
   EVT_COMMAND( wxID_ANY, debEVT_SETSTATUSBARTEXT, DisassemblyDialog::onDebuggerEvent )
   EVT_COMMAND( wxID_ANY, debEVT_UPDATELAYOUT, DisassemblyDialog::onDebuggerEvent )
   EVT_COMMAND( wxID_ANY, debEVT_GOTOADDRESS, DisassemblyDialog::onDebuggerEvent )
   EVT_COMMAND( wxID_ANY, debEVT_GOTOINMEMORYVIEW, DisassemblyDialog::onDebuggerEvent )
   EVT_COMMAND( wxID_ANY, debEVT_REFERENCEMEMORYVIEW, DisassemblyDialog::onDebuggerEvent )
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
wxEND_EVENT_TABLE()

wxBEGIN_EVENT_TABLE(CpuTabPage, wxPanel)
	EVT_LISTBOX_DCLICK( wxID_ANY, CpuTabPage::listBoxHandler)
wxEND_EVENT_TABLE()

DebuggerHelpDialog::DebuggerHelpDialog(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, L"Help")
{
	wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

	auto fileName = Path::CombineWx(PathDefs::GetDocs(), wxFileName(L"debugger.txt"));

	wxTextFile file(fileName);
	wxString text(L"");

	if (file.Open())
	{
		text = file.GetFirstLine();
		while (!file.Eof())
		{
			text += file.GetNextLine() + L"\r\n";
		}
	}

	wxTextCtrl* textControl = new wxTextCtrl(this, wxID_ANY, text, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
	textControl->SetMinSize(wxSize(650, 610));
	sizer->Add(textControl, 1, wxEXPAND);

	SetSizerAndFit(sizer);
}


CpuTabPage::CpuTabPage(wxWindow* parent, DebugInterface* _cpu)
	: wxPanel(parent), cpu(_cpu)
{
	wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
	SetSizer(mainSizer);

	leftTabs = new wxNotebook(this, wxID_ANY);
	bottomTabs = new wxNotebook(this, wxID_ANY);
	disassembly = new CtrlDisassemblyView(this, cpu);
	registerList = new CtrlRegisterList(leftTabs, cpu);
	functionList = new wxListBox(leftTabs, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, NULL, wxBORDER_NONE | wxLB_SORT);

	wxPanel* memoryPanel = new wxPanel(bottomTabs);
	memory = new CtrlMemView(memoryPanel, cpu);
	memorySearch = new CtrlMemSearch(memoryPanel, cpu);

	// create register list and disassembly section
	wxBoxSizer* middleSizer = new wxBoxSizer(wxHORIZONTAL);

	wxBoxSizer* miscStuffSizer = new wxBoxSizer(wxHORIZONTAL);
	cyclesText = new wxStaticText(this, wxID_ANY, L"");
	miscStuffSizer->Add(cyclesText, 0, wxLEFT | wxTOP | wxBOTTOM, 2);

	leftTabs->AddPage(registerList, L"Registers");
	leftTabs->AddPage(functionList, L"Functions");

	wxBoxSizer* registerSizer = new wxBoxSizer(wxVERTICAL);
	registerSizer->Add(miscStuffSizer, 0);
	registerSizer->Add(leftTabs, 1);

	middleSizer->Add(registerSizer, 0, wxEXPAND | wxRIGHT, 2);
	middleSizer->Add(disassembly, 2, wxEXPAND);
	mainSizer->Add(middleSizer, 3, wxEXPAND | wxBOTTOM, 3);

	wxBoxSizer *memorySizer = new wxBoxSizer(wxHORIZONTAL);
	memorySizer->Add(memory, 1, wxEXPAND);
#ifdef _WIN32
	memorySearch->SetMaxSize(wxSize(310 * MSW_GetDPIScale(), -1));
#else
	memorySearch->SetMaxSize(wxSize(330, -1));
#endif
	memorySizer->Add(memorySearch, 1, wxEXPAND);
	memoryPanel->SetSizer(memorySizer);
	memoryPanel->SetBackgroundColour(wxTransparentColor);

	// create bottom section
	bottomTabs->AddPage(memoryPanel, L"Memory");

	breakpointList = new BreakpointList(bottomTabs, cpu, disassembly);
	bottomTabs->AddPage(breakpointList, L"Breakpoints");

	threadList = NULL;
	stackFrames = NULL;
	if (cpu == &r5900Debug)
	{
		threadList = new ThreadList(bottomTabs, cpu);
		bottomTabs->AddPage(threadList, L"Threads");

		stackFrames = new StackFramesList(bottomTabs, cpu, disassembly);
		bottomTabs->AddPage(stackFrames, L"Stack frames");
	}

	mainSizer->Add(bottomTabs, 1, wxEXPAND);

	mainSizer->Layout();

	symbolCount = 0;

	lastCycles = 0;
	loadCycles();
}

void CpuTabPage::clearSymbolMap()
{
	symbolCount = 0;
	functionList->Clear();
}

void CpuTabPage::reloadSymbolMap()
{
	if (!symbolCount)
	{
		auto funcs = cpu->GetSymbolMap().GetAllSymbols(ST_FUNCTION);
		symbolCount = funcs.size();
		for (size_t i = 0; i < funcs.size(); i++)
		{
			wxString name = wxString(funcs[i].name.c_str(), wxConvUTF8);
			functionList->Append(name, reinterpret_cast<void*>(static_cast<uintptr_t>(funcs[i].address)));
		}
	}
}

void CpuTabPage::listBoxHandler(wxCommandEvent& event)
{
	int index = functionList->GetSelection();
	if (event.GetEventObject() == functionList && index >= 0)
	{
		uptr pos = (uptr)functionList->GetClientData(index);
		postEvent(debEVT_GOTOINDISASM, pos);
	}
}

void CpuTabPage::postEvent(wxEventType type, int value)
{
	wxCommandEvent event(type, GetId());
	event.SetEventObject(this);
	event.SetClientData(cpu);
	event.SetInt(value);
	wxPostEvent(this, event);
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
	swprintf(str, 64, L"Ctr:  %u", cycles - lastCycles);
	cyclesText->SetLabel(str);
	lastCycles = cycles;
}

u32 CpuTabPage::getStepOutAddress()
{
	if (threadList == NULL)
		return (u32)-1;

	EEThread currentThread = threadList->getRunningThread();
	std::vector<MipsStackWalk::StackFrame> frames =
		MipsStackWalk::Walk(cpu, cpu->getPC(), cpu->getRegister(0, 31), cpu->getRegister(0, 29),
			currentThread.data.entry_init, currentThread.data.stack);

	if (frames.size() < 2)
		return (u32)-1;

	return frames[1].pc;
}

DisassemblyDialog::DisassemblyDialog(wxWindow* parent) :
	wxFrame(parent, wxID_ANY, L"Debugger", wxDefaultPosition, wxDefaultSize, wxRESIZE_BORDER | wxCLOSE_BOX | wxCAPTION | wxSYSTEM_MENU | wxMINIMIZE_BOX | wxMAXIMIZE_BOX),
	currentCpu(NULL)
{
	int width = g_Conf->EmuOptions.Debugger.WindowWidth;
	int height = g_Conf->EmuOptions.Debugger.WindowHeight;

	topSizer = new wxBoxSizer(wxVERTICAL);
	wxPanel* panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, _("panel"));
	panel->SetSizer(topSizer);
	SetIcons(wxGetApp().GetIconBundle());

	// create top row
	wxBoxSizer* topRowSizer = new wxBoxSizer(wxHORIZONTAL);

	breakRunButton = new wxButton(panel, wxID_ANY, L"Run");
	Bind(wxEVT_BUTTON, &DisassemblyDialog::onBreakRunClicked, this, breakRunButton->GetId());
	breakRunButton->Enable(false);
	topRowSizer->Add(breakRunButton);

	stepIntoButton = new wxButton(panel, wxID_ANY, L"Step Into" + wxString(" (F11)"));
	stepIntoButton->Enable(false);
	Bind(wxEVT_BUTTON, &DisassemblyDialog::onStepIntoClicked, this, stepIntoButton->GetId());
	topRowSizer->Add(stepIntoButton);

	stepOverButton = new wxButton(panel, wxID_ANY, L"Step Over" + wxString(" (F10)"));
	stepOverButton->Enable(false);
	Bind(wxEVT_BUTTON, &DisassemblyDialog::onStepOverClicked, this, stepOverButton->GetId());
	topRowSizer->Add(stepOverButton);

	stepOutButton = new wxButton(panel, wxID_ANY, L"Step Out");
	stepOutButton->Enable(false);
	Bind(wxEVT_BUTTON, &DisassemblyDialog::onStepOutClicked, this, stepOutButton->GetId());
	topRowSizer->Add(stepOutButton);

	breakpointButton = new wxButton(panel, wxID_ANY, L"Breakpoint");
	Bind(wxEVT_BUTTON, &DisassemblyDialog::onBreakpointClicked, this, breakpointButton->GetId());
	topRowSizer->Add(breakpointButton);

	helpButton = new wxButton(panel, wxID_ANY, L"Help");
	Bind(wxEVT_BUTTON, &DisassemblyDialog::onHelpClicked, this, helpButton->GetId());
	topRowSizer->Add(helpButton);

	topSizer->Add(topRowSizer, 0, wxLEFT | wxRIGHT | wxTOP, 3);

	// create middle part of the window
	middleBook = new wxNotebook(panel, wxID_ANY);
	middleBook->SetBackgroundColour(wxColour(0xFFF0F0F0));
	eeTab = new CpuTabPage(middleBook, &r5900Debug);
	iopTab = new CpuTabPage(middleBook, &r3000Debug);
	middleBook->AddPage(eeTab, L"R5900");
	middleBook->AddPage(iopTab, L"R3000");
	Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, &DisassemblyDialog::onPageChanging, this, middleBook->GetId());
	topSizer->Add(middleBook, 3, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 3);
	currentCpu = eeTab;

	CreateStatusBar(1);

	SetMinSize(wxSize(1000, 600));
	panel->GetSizer()->Fit(this);

	if (width != 0 && height != 0)
		SetSize(width, height);

	setDebugMode(true, true);
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

void DisassemblyDialog::onBreakRunClicked(wxCommandEvent& evt)
{
	if (r5900Debug.isCpuPaused())
	{
		// If the current PC is on a breakpoint, the user doesn't want to do nothing.
		CBreakPoints::SetSkipFirst(BREAKPOINT_EE, r5900Debug.getPC());
		CBreakPoints::SetSkipFirst(BREAKPOINT_IOP, r3000Debug.getPC());
		r5900Debug.resumeCpu();
	}
	else 
	{
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

void DisassemblyDialog::onHelpClicked(wxCommandEvent& evt)
{
	DebuggerHelpDialog help(this);
	help.ShowModal();
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
	DebugInterface* debug = currentCpu->getCpu();

	CtrlDisassemblyView* disassembly = currentCpu->getDisassembly();

	// If the current PC is on a breakpoint, the user doesn't want to do nothing.
	CBreakPoints::SetSkipFirst(debug->getCpuType(), debug->getPC());
	u32 currentPc = debug->getPC();

	MIPSAnalyst::MipsOpcodeInfo info = MIPSAnalyst::GetOpcodeInfo(debug, debug->getPC());
	u32 breakpointAddress = currentPc + disassembly->getInstructionSizeAt(currentPc);
	if (info.isBranch)
	{
		if (!info.isConditional)
		{
			if (info.isLinkedBranch)	// jal, jalr
			{
				// it's a function call with a delay slot - skip that too
				breakpointAddress += 4;
			}
			else						// j, ...
			{					
			 // in case of absolute branches, set the breakpoint at the branch target
				breakpointAddress = info.branchTarget;
			}
		}
		else 							// beq, ...
		{						
			if (info.conditionMet)
			{
				breakpointAddress = info.branchTarget;
			}
			else
			{
				breakpointAddress = currentPc + 2 * 4;
				disassembly->scrollStepping(breakpointAddress);
			}
		}
	}
	else 
	{
		disassembly->scrollStepping(breakpointAddress);
	}

	CBreakPoints::AddBreakPoint(debug->getCpuType(), breakpointAddress, true);
	r5900Debug.resumeCpu();
}


void DisassemblyDialog::stepInto()
{
	if (!r5900Debug.isAlive() || !r5900Debug.isCpuPaused() || currentCpu == NULL)
		return;

	DebugInterface* debug = currentCpu->getCpu();
	CtrlDisassemblyView* disassembly = currentCpu->getDisassembly();

	// If the current PC is on a breakpoint, the user doesn't want to do nothing.
	CBreakPoints::SetSkipFirst(debug->getCpuType(), debug->getPC());
	u32 currentPc = debug->getPC();

	MIPSAnalyst::MipsOpcodeInfo info = MIPSAnalyst::GetOpcodeInfo(debug, debug->getPC());
	u32 breakpointAddress = currentPc + disassembly->getInstructionSizeAt(currentPc);
	if (info.isBranch)
	{
		if (!info.isConditional)
		{
			breakpointAddress = info.branchTarget;
		}
		else 
		{
			if (info.conditionMet)
			{
				breakpointAddress = info.branchTarget;
			}
			else
			{
				breakpointAddress = currentPc + 2 * 4;
				disassembly->scrollStepping(breakpointAddress);
			}
		}
	}

	if (info.isSyscall)
		breakpointAddress = info.branchTarget;

	CBreakPoints::AddBreakPoint(debug->getCpuType(), breakpointAddress, true);
	r5900Debug.resumeCpu();
}

void DisassemblyDialog::stepOut()
{
	if (!r5900Debug.isAlive() || !r5900Debug.isCpuPaused() || currentCpu == NULL)
		return;
	DebugInterface* debug = currentCpu->getCpu();
	// If the current PC is on a breakpoint, the user doesn't want to do nothing.
	CBreakPoints::SetSkipFirst(debug->getCpuType(), debug->getPC());

	u32 addr = currentCpu->getStepOutAddress();
	if (addr == (u32)-1)
		return;

	CBreakPoints::AddBreakPoint(debug->getCpuType(), addr, true);
	r5900Debug.resumeCpu();
}

void DisassemblyDialog::onBreakpointClicked(wxCommandEvent& evt)
{
	if (currentCpu == NULL)
		return;

	BreakpointWindow bpw(this, currentCpu->getCpu());
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
	}
	else if (type == debEVT_UPDATELAYOUT)
	{
		if (currentCpu != NULL)
			currentCpu->GetSizer()->Layout();
		topSizer->Layout();
		update();
	}
	else if (type == debEVT_GOTOADDRESS)
	{
		DebugInterface* cpu = reinterpret_cast<DebugInterface*>(evt.GetClientData());
		u64 addr;
		if (!executeExpressionWindow(this, cpu, addr))
			return;

		if (currentCpu != NULL) 
		{
			// GetInt() is 0 when called by the disassembly view, 1 when called by the memory view
			if (!evt.GetInt())
				currentCpu->getDisassembly()->gotoAddress(addr);
			else
				currentCpu->getMemoryView()->gotoAddress(addr);
		}
		update();
	}
	else if (type == debEVT_GOTOINMEMORYVIEW)
	{
		if (currentCpu != NULL)
		{
			currentCpu->showMemoryView();

			currentCpu->getMemoryView()->gotoAddress(evt.GetInt(), true);
			currentCpu->getDisassembly()->SetFocus();
		}
	}
	else if (type == debEVT_REFERENCEMEMORYVIEW)
	{
		if (currentCpu != NULL)
		{
			currentCpu->getMemoryView()->updateReference(evt.GetInt());
		}
	}
	else if (type == debEVT_RUNTOPOS)
	{
		CBreakPoints::AddBreakPoint(currentCpu->getCpu()->getCpuType(), evt.GetInt(), true);
		currentCpu->getCpu()->resumeCpu();
	}
	else if (type == debEVT_GOTOINDISASM)
	{
		if (currentCpu != NULL)
		{
			u32 pos = evt.GetInt();
			currentCpu->getDisassembly()->gotoAddress(pos);
			currentCpu->getDisassembly()->SetFocus();
			update();
		}
	}
	else if (type == debEVT_STEPOVER)
	{
		if (currentCpu != NULL)
			stepOver();
	}
	else if (type == debEVT_STEPINTO)
	{
		if (currentCpu != NULL)
			stepInto();
	}
	else if (type == debEVT_UPDATE)
	{
		update();
	}
	else if (type == debEVT_BREAKPOINTWINDOW)
	{
		wxCommandEvent evt;
		onBreakpointClicked(evt);
	}
	else if (type == debEVT_MAPLOADED)
	{
		wxBusyInfo wait("Please wait, Reloading ELF functions");
		eeTab->clearSymbolMap();
		iopTab->clearSymbolMap();
		eeTab->reloadSymbolMap();
		iopTab->reloadSymbolMap();
	}
	else if (type == debEVT_STEPOUT)
	{
		if (currentCpu != NULL)
			stepOut();
	}
}

void DisassemblyDialog::onClose(wxCloseEvent& evt)
{
	auto* mainFrame = GetMainFramePtr();

	Hide();
	mainFrame->CheckMenuItem(MenuId_Debug_Open, false);
}

void DisassemblyDialog::update()
{
	if (currentCpu != NULL)
	{
		stepOverButton->Enable(true);
		breakpointButton->Enable(true);
		currentCpu->update();
	}
	else 
	{
		stepOverButton->Enable(false);
		breakpointButton->Enable(false);
	}
}

void DisassemblyDialog::reset()
{
	eeTab->getDisassembly()->clearFunctions();
	eeTab->clearSymbolMap();
	iopTab->getDisassembly()->clearFunctions();
	iopTab->clearSymbolMap();
}

void DisassemblyDialog::populate()
{
	eeTab->reloadSymbolMap();
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
		breakRunButton->Enable(true);

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
			if (!CBreakPoints::GetBreakpointTriggered())
			{
				wxBusyInfo wait("Please wait, Reading ELF functions");
				populate();
			}
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
				CBreakPoints::SetSkipFirst(BREAKPOINT_EE, 0);
				CBreakPoints::SetSkipFirst(BREAKPOINT_IOP, 0);
			}

			if (currentCpu != NULL)
				currentCpu->loadCycles();
		}
		else 
		{
			breakRunButton->SetLabel(L"Break");

			stepIntoButton->Enable(false);
			stepOverButton->Enable(false);
			stepOutButton->Enable(false);
		}
	}
	else
	{
		breakRunButton->SetLabel(L"Run");
		stepIntoButton->Enable(false);
		stepOverButton->Enable(false);
		stepOutButton->Enable(false);
		currentCpu = NULL;
	}

	update();
}
