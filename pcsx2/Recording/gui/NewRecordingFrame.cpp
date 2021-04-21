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

#ifndef DISABLE_RECORDING

#include "NewRecordingFrame.h"
#include "Utilities/StringUtils.h"

NewRecordingFrame::NewRecordingFrame(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, "New Input Recording", wxDefaultPosition, wxDefaultSize, wxSTAY_ON_TOP | wxCAPTION)
{
	wxPanel* panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, _("panel"));

	wxFlexGridSizer* fgs = new wxFlexGridSizer(4, 2, 20, 20);
	wxBoxSizer* container = new wxBoxSizer(wxVERTICAL);

	m_file_label = new wxStaticText(panel, wxID_ANY, _("File Path"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
	m_author_label = new wxStaticText(panel, wxID_ANY, _("Author"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
	m_from_label = new wxStaticText(panel, wxID_ANY, _("Record From"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);

	m_file_picker = new wxFilePickerCtrl(panel, wxID_ANY, wxEmptyString, "Save new input recording...",
										 InputRecordingFileV2::s_extension_filter, wxDefaultPosition, wxDefaultSize, wxFLP_SAVE | wxFLP_OVERWRITE_PROMPT | wxFLP_USE_TEXTCTRL);
	m_author_input = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	m_from_choice = new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, NULL);

	m_start_recording = new wxButton(panel, wxID_OK, _("Browse Required"), wxDefaultPosition, wxDefaultSize);
	m_start_recording->Enable(false);
	m_cancel_recording = new wxButton(panel, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize);

	fgs->Add(m_file_label, 1);
	fgs->Add(m_file_picker, 1);

	fgs->Add(m_author_label, 1);
	fgs->Add(m_author_input, 1, wxEXPAND);

	fgs->Add(m_from_label, 1);
	fgs->Add(m_from_choice, 1, wxEXPAND);

	fgs->Add(m_start_recording, 1);
	fgs->Add(m_cancel_recording, 1);

	container->Add(fgs, 1, wxALL | wxEXPAND, 15);
	panel->SetSizer(container);
	panel->GetSizer()->Fit(this);
	Centre();

	m_file_browsed = false;
	m_file_picker->GetPickerCtrl()->Bind(wxEVT_FILEPICKER_CHANGED, &NewRecordingFrame::onFileDirChanged, this);
	m_file_picker->Bind(wxEVT_FILEPICKER_CHANGED, &NewRecordingFrame::onFileChanged, this);
}

int NewRecordingFrame::showModal(const bool isCoreThreadOpen)
{
	wxArrayString choices;
	for (int i = 0; i < s_recording_type_options.size(); i++)
	{
		// Only allow power-on recordings if the game hasn't already been booted.
		if (!isCoreThreadOpen && i != 0)
		{
			continue;
		}
		choices.Add(s_recording_type_options.at(i).first);
	}
	m_from_choice->Set(choices);
	m_from_choice->SetSelection(0);
	// Select save-state by default if the game is currently running
	if (isCoreThreadOpen)
	{
		// Assumes vector's position, not great but, using a map would be better here, but worse in wx's event handler.
		m_from_choice->SetSelection(1);
	}

	return wxDialog::ShowModal();
}

void NewRecordingFrame::onFileDirChanged(wxFileDirPickerEvent& event)
{
	m_file_picker->wxFileDirPickerCtrlBase::OnFileDirChange(event);
	m_file_browsed = true;
	enableOkBox();
}

void NewRecordingFrame::onFileChanged(wxFileDirPickerEvent& event)
{
	enableOkBox();
}

void NewRecordingFrame::enableOkBox()
{
	if (m_file_picker->GetPath().length() == 0)
	{
		m_file_browsed = false;
		m_start_recording->SetLabel(_("Browse Required"));
		m_start_recording->Enable(false);
	}
	else if (m_file_browsed)
	{
		m_start_recording->SetLabel(_("Start"));
		m_start_recording->Enable(true);
	}
}

// TODO - i removed the wxWidgets fix here, double check in a linux environment!
fs::path NewRecordingFrame::getFile() const
{
	return FileUtils::wxStringToPath(m_file_picker->GetPath());
}

std::string NewRecordingFrame::getAuthorUTF8() const
{
	return StringUtils::UTF8::fromWxString(m_author_input->GetValue());
}

InputRecordingFileV2::InputRecordingType NewRecordingFrame::getRecordingType() const
{
	return s_recording_type_options.at(m_from_choice->GetSelection()).second;
}
#endif
