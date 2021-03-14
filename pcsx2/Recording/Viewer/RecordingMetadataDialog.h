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
	RecordingMetadataDialog(wxWindow* parent, const std::string& author, const std::string& gameName, int undoCount);

	std::string GetAuthor();
	std::string GetGameName();
	int GetUndoCount();

	bool Ok() { return ok; }

private:
	bool ok = false;

	std::string m_author;
	std::string m_gameName;
	int m_undoCount;

	wxStaticText* authorLabel;
	wxStaticText* gameNameLabel;
	wxStaticText* undoCountLabel;

	wxTextCtrl* authorField;
	wxTextCtrl* gameNameField;
	wxSpinCtrl* undoCountField;

	wxButton* confirmBtn;
	wxButton* cancelBtn;

	void OnConfirm(wxCommandEvent& event);
};