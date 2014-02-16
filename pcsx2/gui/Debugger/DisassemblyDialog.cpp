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

	stopGoButton = new wxButton( panel, wxID_ANY, L"Go" );
	Connect( stopGoButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DisassemblyDialog::onPauseResumeClicked ) );

	disassembly = new CtrlDisassemblyView(panel,&debug);
	disassembly->SetSize(600,500);
	disassembly->Move(100,20);

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
	stopGoButton->Enable(running);

	if (debugMode)
	{
		stopGoButton->SetLabel(L"Go");

		disassembly->gotoAddress(debug.getPC());
		disassembly->SetFocus();
	} else {
		stopGoButton->SetLabel(L"Stop");
	}

	update();
}