#include "PrecompiledHeader.h"

#include "DisassemblyDialog.h"
#include "DebugTools/DebugInterface.h"
#include "DebugTools/DisassemblyManager.h"
#include "DebugTools/Breakpoints.h"

BEGIN_EVENT_TABLE(DisassemblyDialog, wxFrame)
   EVT_COMMAND( wxID_ANY, debEVT_SETSTATUSBARTEXT, DisassemblyDialog::onSetStatusBarText )
   EVT_COMMAND( wxID_ANY, debEVT_UPDATELAYOUT, DisassemblyDialog::onUpdateLayout )
END_EVENT_TABLE()

DisassemblyDialog::DisassemblyDialog(wxWindow* parent):
	wxFrame( parent, wxID_ANY, L"Disassembler", wxDefaultPosition,wxDefaultSize,wxRESIZE_BORDER|wxCLOSE_BOX|wxCAPTION )
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

	topSizer->Add(topRowSizer);
	
	// create middle part of the window
	wxBoxSizer* middleSizer = new wxBoxSizer(wxHORIZONTAL);

	registerList = new CtrlRegisterList(panel,&debug);
	middleSizer->Add(registerList,1,wxEXPAND|wxRIGHT,4);

	disassembly = new CtrlDisassemblyView(panel,&debug);
	middleSizer->Add(disassembly,2,wxEXPAND);

	topSizer->Add(middleSizer,1,wxEXPAND|wxBOTTOM,2);

	// todo: create bottom part

	CreateStatusBar(1);
	
	SetMinSize(wxSize(800,600));
	panel->GetSizer()->Fit(this);

	setDebugMode(true);
}

void DisassemblyDialog::onPauseResumeClicked(wxCommandEvent& evt)
{	
	if (debug.isCpuPaused())
	{
		if (CBreakPoints::IsAddressBreakPoint(debug.getPC()))
			CBreakPoints::SetSkipFirst(debug.getPC());
		debug.resumeCpu();
	} else
		debug.pauseCpu();
}

void DisassemblyDialog::onStepOverClicked(wxCommandEvent& evt)
{	
	stepOver();
}


void DisassemblyDialog::stepOver()
{
	if (!debug.isRunning() || !debug.isCpuPaused())
		return;
	
	// If the current PC is on a breakpoint, the user doesn't want to do nothing.
	CBreakPoints::SetSkipFirst(debug.getPC());
	u32 currentPc = debug.getPC();

	MIPSAnalyst::MipsOpcodeInfo info = MIPSAnalyst::GetOpcodeInfo(&debug,debug.getPC());
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
	debug.resumeCpu();
}


void DisassemblyDialog::onSetStatusBarText(wxCommandEvent& evt)
{
	GetStatusBar()->SetLabel(evt.GetString());
}

void DisassemblyDialog::onUpdateLayout(wxCommandEvent& evt)
{
	topSizer->Layout();
}


void DisassemblyDialog::update()
{
	disassembly->Refresh();
	registerList->Refresh();
}

void DisassemblyDialog::setDebugMode(bool debugMode)
{
	bool running = debug.isRunning();
	breakResumeButton->Enable(running);
	disassembly->Enable(running);

	if (running)
	{
		if (debugMode)
		{
			CBreakPoints::ClearTemporaryBreakPoints();
			breakResumeButton->SetLabel(L"Resume");

			stepOverButton->Enable(true);

			disassembly->gotoAddress(debug.getPC());
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