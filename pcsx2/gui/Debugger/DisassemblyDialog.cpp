#include "PrecompiledHeader.h"

#include "DisassemblyDialog.h"
#include "DebugTools/DebugInterface.h"
#include "DebugTools/DisassemblyManager.h"

DisassemblyDialog::DisassemblyDialog(wxWindow* parent)
	: wxDialogWithHelpers( parent, L"Disassembler", pxDialogFlags().Resize().MinWidth( 460 ) )
{
	
	stopGoButton = new wxButton( this, wxID_ANY, L"Go" );
	Connect( stopGoButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( DisassemblyDialog::onPauseResumeClicked ) );

	setDebugMode(true);
	SetMinSize(wxSize(800,600));
}

void DisassemblyDialog::onPauseResumeClicked(wxCommandEvent& evt)
{	
	if (debug.isCpuPaused())
		debug.resumeCpu();
	else
		debug.pauseCpu();
}

void DisassemblyDialog::update()
{

}

void DisassemblyDialog::setDebugMode(bool debug)
{
	if (debug)
	{
		stopGoButton->SetLabel(L"Go");
	} else {
		stopGoButton->SetLabel(L"Stop");
	}
}