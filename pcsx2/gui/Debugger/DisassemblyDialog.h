#pragma once
#include <wx/wx.h>
#include "App.h"

#include "CtrlDisassemblyView.h"
#include "DebugEvents.h"

class DisassemblyDialog : public wxFrame
{
public:
	DisassemblyDialog( wxWindow* parent=NULL );
	virtual ~DisassemblyDialog() throw() {}

	static wxString GetNameStatic() { return L"DisassemblyDialog"; }
	wxString GetDialogName() const { return GetNameStatic(); }
	

	void update();
	void reset() { disassembly->clearFunctions(); };
	void setDebugMode(bool debugMode);

	DECLARE_EVENT_TABLE()
protected:
	void onPauseResumeClicked(wxCommandEvent& evt);
	void onSetStatusBarText(wxCommandEvent& evt);
private:
	wxStatusBar* statusBar;
	wxButton* stopGoButton;
	CtrlDisassemblyView* disassembly;
};