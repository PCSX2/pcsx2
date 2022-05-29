/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

// This file dies along with wxWidgets
#ifndef PCSX2_CORE

#include "PrecompiledHeader.h"

#include "NewRecordingFrame.h"

#include <wx/gbsizer.h>

NewRecordingFrame::NewRecordingFrame(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, "New Input Recording", wxDefaultPosition, wxDefaultSize, wxSTAY_ON_TOP | wxCLOSE_BOX | wxCAPTION)
{
	m_panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, _("panel"));

	wxGridBagSizer* gbs = new wxGridBagSizer(20, 20);
	gbs->SetFlexibleDirection(wxBOTH);
	gbs->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
	wxBoxSizer* container = new wxBoxSizer(wxVERTICAL);

	m_fileLabel = new wxStaticText(m_panel, wxID_ANY, _("File Path"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
	m_authorLabel = new wxStaticText(m_panel, wxID_ANY, _("Author"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
	m_fromLabel = new wxStaticText(m_panel, wxID_ANY, _("Record From"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);

	m_filePicker = new wxFilePickerCtrl(m_panel, MenuIds_New_Recording_Frame_File, wxEmptyString, "File", L"p2m2 file(*.p2m2)|*.p2m2", wxDefaultPosition, wxDefaultSize, wxFLP_SAVE | wxFLP_OVERWRITE_PROMPT | wxFLP_USE_TEXTCTRL);
	m_authorInput = new wxTextCtrl(m_panel, MenuIds_New_Recording_Frame_Author, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	m_fromChoice = new wxChoice(m_panel, MenuIds_New_Recording_Frame_From, wxDefaultPosition, wxDefaultSize);

	m_savestate_label =
		_("Be Warned! Basing an input recording off a savestate can be a mistake as savestates can break across emulator versions. Be prepared to be stuck to an emulator version or have to re-create your starting savestate in a later version.");
	m_warning_label = new wxStaticText(m_panel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
	m_warning_label->SetForegroundColour(wxColor(*wxRED));

	m_startRecording = new wxButton(m_panel, wxID_OK, _("Browse Required"), wxDefaultPosition, wxDefaultSize);
	m_startRecording->Enable(false);
	m_cancelRecording = new wxButton(m_panel, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize);

	gbs->Add(m_fileLabel, wxGBPosition(0, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	gbs->Add(m_filePicker, wxGBPosition(0, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);

	gbs->Add(m_authorLabel, wxGBPosition(1, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	gbs->Add(m_authorInput, wxGBPosition(1, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);

	gbs->Add(m_fromLabel, wxGBPosition(2, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	gbs->Add(m_fromChoice, wxGBPosition(2, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxEXPAND);

	gbs->Add(m_warning_label, wxGBPosition(3, 0), wxGBSpan(1, 2), wxALIGN_CENTER_VERTICAL);

	gbs->Add(m_startRecording, wxGBPosition(4, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	gbs->Add(m_cancelRecording, wxGBPosition(4, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);

	gbs->AddGrowableCol(0);
	gbs->AddGrowableCol(1);
	gbs->AddGrowableRow(3);

	container->Add(gbs, 1, wxALL | wxEXPAND, 15);
	m_panel->SetSizer(container);
	m_panel->GetSizer()->Fit(this);
	Centre();

	m_fileBrowsed = false;
	m_filePicker->GetPickerCtrl()->Bind(wxEVT_FILEPICKER_CHANGED, &NewRecordingFrame::OnFileDirChange, this);
	m_filePicker->Bind(wxEVT_FILEPICKER_CHANGED, &NewRecordingFrame::OnFileChanged, this);
	m_fromChoice->Bind(wxEVT_CHOICE, &NewRecordingFrame::OnRecordingTypeChoiceChanged, this);
}

int NewRecordingFrame::ShowModal(const bool isCoreThreadOpen)
{
	static const char* choices[2] = {"Boot", "Current Frame"};
	m_fromChoice->Set(wxArrayString(1 + isCoreThreadOpen, &choices[0]));
	m_fromChoice->SetSelection(isCoreThreadOpen);
	if (m_fromChoice->GetSelection() == 1)
	{
		m_warning_label->SetLabel(m_savestate_label);
	}
	else
	{
		m_warning_label->SetLabel("");
	}
	m_warning_label->Wrap(GetClientSize().GetWidth());
	m_panel->GetSizer()->Fit(this);
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

void NewRecordingFrame::OnRecordingTypeChoiceChanged(wxCommandEvent& event)
{
	if (m_fromChoice->GetSelection() == 1)
	{
		m_warning_label->SetLabel(m_savestate_label);
	}
	else
	{
		m_warning_label->SetLabel("");
	}
	m_warning_label->Wrap(GetClientSize().GetWidth());
	m_panel->GetSizer()->Fit(this);
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