#pragma once
#include <wx/wx.h>
#include "App.h"


class DisassemblyDialog : public wxDialogWithHelpers
{
public:
	DisassemblyDialog( wxWindow* parent=NULL );
	virtual ~DisassemblyDialog() throw() {}

	static wxString GetNameStatic() { return L"DisassemblyDialog"; }
	wxString GetDialogName() const { return GetNameStatic(); }

	void update();
	void setDebugMode(bool debug);
protected:
	void onPauseResumeClicked(wxCommandEvent& evt);

private:
	wxButton* stopGoButton;
};