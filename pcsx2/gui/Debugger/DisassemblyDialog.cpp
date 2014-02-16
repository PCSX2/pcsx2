#include "PrecompiledHeader.h"

#include "DisassemblyDialog.h"
#include "DebugTools/DebugInterface.h"
#include "DebugTools/DisassemblyManager.h"
#include "DebugTools/Breakpoints.h"

BEGIN_EVENT_TABLE(DisassemblyDialog, wxFrame)
   EVT_COMMAND( wxID_ANY, debEVT_SETSTATUSBARTEXT, DisassemblyDialog::onSetStatusBarText )
END_EVENT_TABLE()

DisassemblyDialog::DisassemblyDialog(wxWindow* parent):
	wxFrame( parent, wxID_ANY, L"Disassembler", wxDefaultPosition,wxDefaultSize,wxRESIZE_BORDER|wxCLOSE_BOX|wxCAPTION )
{

	wxBoxSizer* topSizer = new wxBoxSizer( wxEXPAND );
	wxPanel *panel = new wxPanel(this, wxID_ANY, 
		wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, _("panel"));
	panel->SetSizer(topSizer);

	breakResumeButton = new wxButton(panel, wxID_ANY, L"Go");
	Connect(breakResumeButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(DisassemblyDialog::onPauseResumeClicked));
	
	wxButton* stepOverButton = new wxButton( panel, wxID_ANY, L"Step Over" );
	Connect( stepOverButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DisassemblyDialog::onStepOverClicked ) );

	stepOverButton->Move(80,0);

	disassembly = new CtrlDisassemblyView(panel,&debug);
	disassembly->SetSize(600,500);
	disassembly->Move(100,25);

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

void DisassemblyDialog::update()
{
	disassembly->Refresh();
}

void DisassemblyDialog::setDebugMode(bool debugMode)
{
	bool running = debug.isRunning();
	breakResumeButton->Enable(running);

	if (debugMode)
	{
		CBreakPoints::ClearTemporaryBreakPoints();
		breakResumeButton->SetLabel(L"Resume");

		disassembly->gotoAddress(debug.getPC());
		disassembly->SetFocus();
	} else {
		breakResumeButton->SetLabel(L"Break");
	}

	update();
}