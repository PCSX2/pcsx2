/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2019  PCSX2 Dev Team
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
#include <wx/filepicker.h>


#ifndef DISABLE_RECORDING
enum MenuIds_New_Recording_Frame
{
	MenuIds_New_Recording_Frame_File = 0,
	MenuIds_New_Recording_Frame_Author,
	MenuIds_New_Recording_Frame_From
};

// The Dialog to pop-up when recording a new movie
class NewRecordingFrame : public wxDialog
{
public:
	NewRecordingFrame(wxWindow* parent);

	wxString GetFile() const;
	wxString GetAuthor() const;
	int GetFrom() const;

private:
	wxStaticText* m_fileLabel;
	wxFilePickerCtrl* m_filePicker;
	wxStaticText* m_authorLabel;
	wxTextCtrl* m_authorInput;
	wxStaticText* m_fromLabel;
	wxChoice* m_fromChoice;
	wxButton* m_startRecording;
	wxButton* m_cancelRecording;
};
#endif
