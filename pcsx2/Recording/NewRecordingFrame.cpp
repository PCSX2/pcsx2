/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"

#include "NewRecordingFrame.h"


#ifndef DISABLE_RECORDING
NewRecordingFrame::NewRecordingFrame(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, "New Input Recording", wxDefaultPosition, wxDefaultSize, wxSTAY_ON_TOP | wxCAPTION)
{
	wxPanel* panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, _("panel"));

	wxFlexGridSizer* fgs = new wxFlexGridSizer(4, 2, 20, 20);
	wxBoxSizer* container = new wxBoxSizer(wxVERTICAL);

	m_fileLabel = new wxStaticText(panel, wxID_ANY, _("File Path"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
	m_authorLabel = new wxStaticText(panel, wxID_ANY, _("Author"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
	m_fromLabel = new wxStaticText(panel, wxID_ANY, _("Record From"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);

	m_filePicker = new wxFilePickerCtrl(panel, MenuIds_New_Recording_Frame_File, wxEmptyString, "File", L"p2m2 file(*.p2m2)|*.p2m2", wxDefaultPosition, wxDefaultSize, wxFLP_SAVE | wxFLP_OVERWRITE_PROMPT | wxFLP_USE_TEXTCTRL);
	m_authorInput = new wxTextCtrl(panel, MenuIds_New_Recording_Frame_Author, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	m_fromChoice = new wxChoice(panel, MenuIds_New_Recording_Frame_From, wxDefaultPosition, wxDefaultSize, NULL);

	m_startRecording = new wxButton(panel, wxID_OK, _("Browse Required"), wxDefaultPosition, wxDefaultSize);
	m_startRecording->Enable(false);
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

	m_fileBrowsed = false;
	m_filePicker->GetPickerCtrl()->Bind(wxEVT_FILEPICKER_CHANGED, &NewRecordingFrame::OnFileDirChange, this);
	m_filePicker->Bind(wxEVT_FILEPICKER_CHANGED, &NewRecordingFrame::OnFileChanged, this);
}

int NewRecordingFrame::ShowModal(const bool isCoreThreadOpen)
{
	static const char* choices[2] = {"Boot", "Current Frame"};
	m_fromChoice->Set(wxArrayString(1 + isCoreThreadOpen, &choices[0]));
	m_fromChoice->SetSelection(isCoreThreadOpen);
	return wxDialog::ShowModal();
}

void NewRecordingFrame::OnFileDirChange(wxFileDirPickerEvent& event)
{
	m_filePicker->wxFileDirPickerCtrlBase::OnFileDirChange(event);
	m_fileBrowsed = true;
	EnableOkBox();
}

void NewRecordingFrame::OnFileChanged(wxFileDirPickerEvent& event)
{
	EnableOkBox();
}

void NewRecordingFrame::EnableOkBox()
{
	if (m_filePicker->GetPath().length() == 0)
	{
		m_fileBrowsed = false;
		m_startRecording->SetLabel(_("Browse Required"));
		m_startRecording->Enable(false);
	}
	else if (m_fileBrowsed)
	{
		m_startRecording->SetLabel(_("Start"));
		m_startRecording->Enable(true);
	}
}

wxString NewRecordingFrame::GetFile() const
{
	wxString path = m_filePicker->GetPath();
	// wxWidget's removes the extension if it contains wildcards
	// on wxGTK https://trac.wxwidgets.org/ticket/15285
	if (!path.EndsWith(".p2m2"))
	{
		return wxString::Format("%s.p2m2", path);
	}
	return path;
}

wxString NewRecordingFrame::GetAuthor() const
{
	return m_authorInput->GetValue();
}

int NewRecordingFrame::GetFrom() const
{
	return m_fromChoice->GetSelection();
}
#endif
