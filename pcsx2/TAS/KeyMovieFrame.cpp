#include "PrecompiledHeader.h"
#include "KeyMovieFrame.h"

enum {
	File,
	Author,
	From
};

KeyMovieFrame::KeyMovieFrame(wxWindow *parent)
	: wxDialog(parent, wxID_ANY, "Movie file", wxDefaultPosition, wxSize(500, 175), wxSTAY_ON_TOP | wxCAPTION)
{
	wxPanel *panel = new wxPanel(this, wxID_ANY);

	wxBoxSizer *vbox = new wxBoxSizer(wxVERTICAL);

	wxPoint p;
	wxSize s = wxDefaultSize;

	// File Picker
	p.x = 15;
	p.y = 10;
	m_fileLabel = new wxStaticText(panel, wxID_ANY, _("File:"), p, wxDefaultSize, wxALIGN_RIGHT);
	p.x += 80;
	p.y = 5;
	s.x = 300;
	m_filePicker = new wxFilePickerCtrl(panel, File, wxEmptyString, "File", L"p2m2 file(*.p2m2)|*.p2m2", p, s,
		wxFLP_SAVE | wxFLP_OVERWRITE_PROMPT | wxFLP_USE_TEXTCTRL);

	// Author
	p.x = 15;
	p.y = 40;
	m_authorLabel = new wxStaticText(panel, wxID_ANY, _("Author:"), p, wxDefaultSize, wxALIGN_RIGHT);
	p.x += 80;
	s.x = 150;
	m_author = new wxTextCtrl(panel, Author, wxEmptyString, p, s);

	// Record From
	p.x = 15;
	p.y = 70;
	m_fromLabel = new wxStaticText(panel, wxID_ANY, _("Record From:"), p, wxDefaultSize, wxALIGN_RIGHT);
	p.x += 80;
	wxArrayString choices;
	choices.Add("Power-On");
	choices.Add("Now");
	m_fromChoice = new wxChoice(panel, From, p, wxDefaultSize, choices);
	m_fromChoice->SetSelection(0);
	
	auto* buttons = CreateButtonSizer(wxOK | wxCANCEL);

	vbox->Add(panel, wxSizerFlags().Expand().Center());
	if (buttons)
		vbox->Add(buttons, wxSizerFlags().Right().Bottom());

	SetSizer(vbox);
}

wxString KeyMovieFrame::getFile() const
{
	return m_filePicker->GetPath();
}

wxString KeyMovieFrame::getAuthor() const
{
	return m_author->GetValue();
}

int KeyMovieFrame::getFrom() const
{
	return m_fromChoice->GetSelection();
}
