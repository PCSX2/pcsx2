#include "PrecompiledHeader.h"

#include "NewRecordingFrame.h"

enum {
	File,
	Author,
	From
};

NewRecordingFrame::NewRecordingFrame(wxWindow *parent)
	: wxDialog(parent, wxID_ANY, "New Input Recording", wxDefaultPosition, wxDefaultSize, wxSTAY_ON_TOP | wxCAPTION)
{
	wxPanel *panel = new wxPanel(this, wxID_ANY,
		wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, _("panel"));

	wxFlexGridSizer *fgs = new wxFlexGridSizer(4, 2, 20, 20);
	wxBoxSizer *container = new wxBoxSizer(wxVERTICAL);

	m_fileLabel = new wxStaticText(panel, wxID_ANY, _("File Path"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
	m_authorLabel = new wxStaticText(panel, wxID_ANY, _("Author"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
	m_fromLabel = new wxStaticText(panel, wxID_ANY, _("Record From"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);

	m_filePicker = new wxFilePickerCtrl(panel, File, wxEmptyString, "File", L"p2m2 file(*.p2m2)|*.p2m2", wxDefaultPosition, wxDefaultSize, wxFLP_SAVE | wxFLP_OVERWRITE_PROMPT | wxFLP_USE_TEXTCTRL);
	m_authorInput = new wxTextCtrl(panel, Author, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	wxArrayString choices;
	choices.Add("Current Frame");
	choices.Add("Power-On");
	m_fromChoice = new wxChoice(panel, From, wxDefaultPosition, wxDefaultSize, choices);
	m_fromChoice->SetSelection(0);

	m_startRecording = new wxButton(panel, wxID_OK, _("Ok"), wxDefaultPosition, wxDefaultSize);
	m_cancelRecording = new wxButton(panel, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize);

	fgs->Add(m_fileLabel, 1);
	fgs->Add(m_filePicker, 1);

	fgs->Add(m_authorLabel, 1);
	fgs->Add(m_authorInput, 1, wxEXPAND);

	fgs->Add(m_fromLabel, 1);
	fgs->Add(m_fromChoice, 1, wxEXPAND);

	fgs->Add(m_startRecording, 1);
	fgs->Add(m_cancelRecording, 1);

	container->Add(fgs, 1, wxALL | wxEXPAND, 15);
	panel->SetSizer(container);
	panel->GetSizer()->Fit(this);
	Centre();
}

wxString NewRecordingFrame::getFile() const
{
	return m_filePicker->GetPath();
}

wxString NewRecordingFrame::getAuthor() const
{
	return m_authorInput->GetValue();
}

int NewRecordingFrame::getFrom() const
{
	return m_fromChoice->GetSelection();
}
