#include "PrecompiledHeader.h"

#include "DisassemblyDialog.h"
#include "DebugTools/DebugInterface.h"
#include "DebugTools/DisassemblyManager.h"
#include "DebugTools/Breakpoints.h"

BEGIN_EVENT_TABLE(DisassemblyDialog, wxFrame)
   EVT_COMMAND( wxID_ANY, debEVT_SETSTATUSBARTEXT, DisassemblyDialog::onDebuggerEvent )
   EVT_COMMAND( wxID_ANY, debEVT_UPDATELAYOUT, DisassemblyDialog::onDebuggerEvent )
   EVT_COMMAND( wxID_ANY, debEVT_GOTOINMEMORYVIEW, DisassemblyDialog::onDebuggerEvent )
   EVT_COMMAND( wxID_ANY, debEVT_RUNTOPOS, DisassemblyDialog::onDebuggerEvent )
   EVT_COMMAND( wxID_ANY, debEVT_GOTOINDISASM, DisassemblyDialog::onDebuggerEvent )
END_EVENT_TABLE()

DisassemblyDialog::DisassemblyDialog(wxWindow* parent):
	wxFrame( parent, wxID_ANY, L"Disassembler", wxDefaultPosition,wxDefaultSize,wxRESIZE_BORDER|wxCLOSE_BOX|wxCAPTION|wxSYSTEM_MENU )
{

	topSizer = new wxBoxSizer( wxVERTICAL );
	wxPanel *panel = new wxPanel(this, wxID_ANY, 
		wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, _("panel"));
	panel->SetSizer(topSizer);

	// create top row
	wxBoxSizer* topRowSizer = new wxBoxSizer(wxHORIZONTAL);

	breakResumeButton = new wxButton(panel, wxID_ANY, L"Resume");
	Connect(breakResumeButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(DisassemblyDialog::onPauseResumeClicked));
	topRowSizer->Add(breakResumeButton,0,wxRIGHT,8);

	stepIntoButton = new wxButton( panel, wxID_ANY, L"Step Into" );
	stepIntoButton->Enable(false);
	topRowSizer->Add(stepIntoButton,0,wxBOTTOM,2);

	stepOverButton = new wxButton( panel, wxID_ANY, L"Step Over" );
	stepOverButton->Enable(false);
	Connect( stepOverButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DisassemblyDialog::onStepOverClicked ) );
	topRowSizer->Add(stepOverButton);
	
	stepOutButton = new wxButton( panel, wxID_ANY, L"Step Out" );
	stepOutButton->Enable(false);
	topRowSizer->Add(stepOutButton,0,wxRIGHT,8);
	
	breakpointButton = new wxButton( panel, wxID_ANY, L"Breakpoint" );
	breakpointButton->Enable(false);
	topRowSizer->Add(breakpointButton);

	topSizer->Add(topRowSizer,0,wxLEFT|wxRIGHT|wxTOP,3);
	
	// create middle part of the window
	wxBoxSizer* middleSizer = new wxBoxSizer(wxHORIZONTAL);

	wxBoxSizer* registerSizer = new wxBoxSizer(wxVERTICAL);
	registerList = new CtrlRegisterList(panel,&r5900Debug);
	registerSizer->Add(registerList,1);
	middleSizer->Add(registerSizer,0,wxEXPAND|wxRIGHT,4);

	disassembly = new CtrlDisassemblyView(panel,&r5900Debug);
	middleSizer->Add(disassembly,2,wxEXPAND);

	topSizer->Add(middleSizer,3,wxEXPAND|wxALL,3);

	// create bottom part
	bottomTabs = new wxNotebook(panel,wxID_ANY);
	memory = new CtrlMemView(bottomTabs,&r5900Debug);
	bottomTabs->AddPage(memory,L"Memory");

	topSizer->Add(bottomTabs,1,wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM,3);

	CreateStatusBar(1);
	
	SetMinSize(wxSize(1000,600));
	panel->GetSizer()->Fit(this);

	setDebugMode(true);
}

void DisassemblyDialog::onPauseResumeClicked(wxCommandEvent& evt)
{	
	if (r5900Debug.isCpuPaused())
	{
		if (CBreakPoints::IsAddressBreakPoint(r5900Debug.getPC()))
			CBreakPoints::SetSkipFirst(r5900Debug.getPC());
		r5900Debug.resumeCpu();
	} else
		r5900Debug.pauseCpu();
}

void DisassemblyDialog::onStepOverClicked(wxCommandEvent& evt)
{	
	stepOver();
}


void DisassemblyDialog::stepOver()
{
	if (!r5900Debug.isAlive() || !r5900Debug.isCpuPaused())
		return;
	
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

void DisassemblyDialog::onDebuggerEvent(wxCommandEvent& evt)
{
	wxEventType type = evt.GetEventType();
	if (type == debEVT_SETSTATUSBARTEXT)
	{
		GetStatusBar()->SetLabel(evt.GetString());
	} else if (type == debEVT_UPDATELAYOUT)
	{
		topSizer->Layout();
		update();
	} else if (type == debEVT_GOTOINMEMORYVIEW)
	{
		memory->gotoAddress(evt.GetInt());
	} else if (type == debEVT_RUNTOPOS)
	{
		CBreakPoints::AddBreakPoint(evt.GetInt(),true);
		r5900Debug.resumeCpu();
	} else if (type == debEVT_GOTOINDISASM)
	{
		disassembly->gotoAddress(evt.GetInt());
		disassembly->SetFocus();
		update();
	}
}

void DisassemblyDialog::update()
{
	disassembly->Refresh();
	registerList->Refresh();
	bottomTabs->Refresh();
}

void DisassemblyDialog::setDebugMode(bool debugMode)
{
	bool running = r5900Debug.isAlive();
	breakResumeButton->Enable(running);
	disassembly->Enable(running);

	if (running)
	{
		if (debugMode)
		{
			CBreakPoints::ClearTemporaryBreakPoints();
			breakResumeButton->SetLabel(L"Resume");

			stepOverButton->Enable(true);

			disassembly->gotoAddress(r5900Debug.getPC());
			disassembly->SetFocus();
		} else {
			breakResumeButton->SetLabel(L"Break");

			stepIntoButton->Enable(false);
			stepOverButton->Enable(false);
			stepOutButton->Enable(false);
		}
	}

	update();
}