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

RecordingMetadataDialog::RecordingMetadataDialog(wxWindow* parent, const std::string& author, const std::string& gameName)
	: m_author(author)
	, m_game_name(gameName)
	, wxDialog(parent, wxID_ANY, wxString("Change Recording Metadata"))
{
	wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* widget_sizer = new wxBoxSizer(wxVERTICAL);

	m_author_label = new wxStaticText(this, wxID_ANY, _("Author"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
	m_author_field = new wxTextCtrl(this, wxID_ANY, m_author);
	gameNameLabel = new wxStaticText(this, wxID_ANY, _("Game Name"), wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
	m_game_name_field = new wxTextCtrl(this, wxID_ANY, m_game_name);

	wxBoxSizer* button_sizer = new wxBoxSizer(wxHORIZONTAL);
	m_confirm_btn = new wxButton(this, wxID_OK);
	m_cancel_btn = new wxButton(this, wxID_CLOSE);

	widget_sizer->Add(m_author_label, 0, wxALL, 5);
	widget_sizer->Add(m_author_field, 0, wxALL, 5);
	widget_sizer->Add(gameNameLabel, 0, wxALL, 5);
	widget_sizer->Add(m_game_name_field, 0, wxALL, 5);
	button_sizer->Add(m_confirm_btn, 0, wxALL, 5);
	button_sizer->Add(m_cancel_btn, 0, wxALL, 5);
	widget_sizer->Add(button_sizer, 0, wxALL, 5);

	main_sizer->Add(widget_sizer, 0, wxALL, 20);

	Bind(wxEVT_BUTTON, &RecordingMetadataDialog::onConfirm, this, m_confirm_btn->GetId());

	SetSizerAndFit(main_sizer);
	SetEscapeId(wxID_CLOSE);
}

void RecordingMetadataDialog::onConfirm(wxCommandEvent& event)
{
	EndModal(wxID_OK);
}

std::string RecordingMetadataDialog::getAuthor()
{
	return m_author_field->GetValue().ToStdString();
}

std::string RecordingMetadataDialog::getGameName()
{
	return m_game_name_field->GetValue().ToStdString();
}