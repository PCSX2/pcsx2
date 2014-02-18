#pragma once
#include <wx/wx.h>
#include <wx/notebook.h>
#include "App.h"

#include "CtrlDisassemblyView.h"
#include "CtrlRegisterList.h"
#include "CtrlMemView.h"
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
	void onUpdateLayout(wxCommandEvent& evt);
	void onStepOverClicked(wxCommandEvent& evt);

	void stepOver();
private:
	wxBoxSizer* topSizer;
	wxNotebook* bottomTabs;
	wxStatusBar* statusBar;
	wxButton* breakResumeButton;
	wxButton* stepIntoButton;
	wxButton* stepOverButton;
	wxButton* stepOutButton;
	wxButton* breakpointButton;
	CtrlDisassemblyView* disassembly;
	CtrlMemView* memory;
	CtrlRegisterList* registerList;
};