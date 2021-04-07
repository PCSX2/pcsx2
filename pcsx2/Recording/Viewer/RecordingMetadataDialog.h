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

#pragma once

#include <wx/wx.h>

class RecordingMetadataDialog : public wxDialog
{
public:
	RecordingMetadataDialog(wxWindow* parent, const std::string& author, const std::string& game_name);

	std::string getAuthor();
	std::string getGameName();

	bool ok() { return m_ok; }

private:
	bool m_ok = false;

	std::string m_author;
	std::string m_game_name;

	wxStaticText* m_author_label;
	wxStaticText* gameNameLabel;

	wxTextCtrl* m_author_field;
	wxTextCtrl* m_game_name_field;

	wxButton* m_confirm_btn;
	wxButton* m_cancel_btn;

	void onConfirm(wxCommandEvent& event);
};