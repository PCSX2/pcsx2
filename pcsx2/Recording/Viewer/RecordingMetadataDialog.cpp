/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#include <wx/spinctrl.h>

#include "RecordingMetadataDialog.h"

RecordingMetadataDialog::RecordingMetadataDialog(wxWindow* parent, const std::string& author, const std::string& gameName, int undoCount)
	: m_author(author)
	, m_gameName(gameName)
	, m_undoCount(undoCount)
	, wxDialog(parent, wxID_ANY, wxString("Change Recording Metadata"))
{
	wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* widgetSizer = new wxBoxSizer(wxVERTICAL);

	authorLabel = new wxStaticText(this, wxID_ANY, _("Author"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
	authorField = new wxTextCtrl(this, wxID_ANY, m_author);
	gameNameLabel = new wxStaticText(this, wxID_ANY, _("Game Name"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
	gameNameField = new wxTextCtrl(this, wxID_ANY, m_gameName);
	undoCountLabel = new wxStaticText(this, wxID_ANY, _("Undo Count"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
	undoCountField = new wxSpinCtrl(this, wxID_ANY, wxString::Format(wxT("%i"), m_undoCount), wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, LONG_MAX);

	wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
	confirmBtn = new wxButton(this, wxID_OK);
	cancelBtn = new wxButton(this, wxID_CLOSE);

	widgetSizer->Add(authorLabel, 0, wxALL, 5);
	widgetSizer->Add(authorField, 0, wxALL, 5);
	widgetSizer->Add(gameNameLabel, 0, wxALL, 5);
	widgetSizer->Add(gameNameField, 0, wxALL, 5);
	widgetSizer->Add(undoCountLabel, 0, wxALL, 5);
	widgetSizer->Add(undoCountField, 0, wxALL, 5);
	buttonSizer->Add(confirmBtn, 0, wxALL, 5);
	buttonSizer->Add(cancelBtn, 0, wxALL, 5);
	widgetSizer->Add(buttonSizer, 0, wxALL, 5);

	mainSizer->Add(widgetSizer, 0, wxALL, 20);

	Bind(wxEVT_BUTTON, &RecordingMetadataDialog::OnConfirm, this, confirmBtn->GetId());

	SetSizerAndFit(mainSizer);
	SetEscapeId(wxID_CLOSE);
}

void RecordingMetadataDialog::OnConfirm(wxCommandEvent& event)
{
	EndModal(wxID_OK);
}

std::string RecordingMetadataDialog::GetAuthor()
{
	return authorField->GetValue().ToStdString();
}

std::string RecordingMetadataDialog::GetGameName()
{
	return gameNameField->GetValue().ToStdString();
}

int RecordingMetadataDialog::GetUndoCount()
{
	return undoCountField->GetValue();
}