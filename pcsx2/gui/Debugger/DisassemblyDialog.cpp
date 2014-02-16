#include "PrecompiledHeader.h"

#include "DisassemblyDialog.h"
#include "DebugTools/DebugInterface.h"
#include "DebugTools/DisassemblyManager.h"
#include "DebugTools/Breakpoints.h"

DisassemblyDialog::DisassemblyDialog(wxWindow* parent)
	: wxDialogWithHelpers( parent, L"Disassembler", pxDialogFlags().Resize().MinWidth( 460 ) )
{
	
	stopGoButton = new wxButton( this, wxID_ANY, L"Go" );
	Connect( stopGoButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DisassemblyDialog::onPauseResumeClicked ) );

	disassembly = new CtrlDisassemblyView(this,&debug);
	disassembly->SetSize(600,500);
	disassembly->Move(100,20);

	setDebugMode(true);
	SetMinSize(wxSize(800,600));
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
	} else {
		stopGoButton->SetLabel(L"Stop");
	}

	update();
}