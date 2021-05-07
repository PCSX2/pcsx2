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

#include <string>

#include "Utilities/FileUtils.h"
#include "Recording/file/v2/InputRecordingFileV2.h"


#ifndef DISABLE_RECORDING

// The Dialog to pop-up when recording a new input recording
class NewRecordingFrame : public wxDialog
{
public:
	NewRecordingFrame(wxWindow* parent);
	int showModal(const bool is_core_thread_open);

	fs::path getFile() const;
	std::string getAuthorUTF8() const;
	InputRecordingFileV2::InputRecordingType getRecordingType() const;

protected:
	void onFileDirChanged(wxFileDirPickerEvent& event);
	void onFileChanged(wxFileDirPickerEvent& event);
	void enableOkBox();

private:
	inline static const std::vector<std::pair<std::string, InputRecordingFileV2::InputRecordingType>> s_recording_type_options = {
		{"System Boot / Power-On", InputRecordingFileV2::InputRecordingType::INPUT_RECORDING_POWER_ON},
		{"Current Frame", InputRecordingFileV2::InputRecordingType::INPUT_RECORDING_SAVESTATE},
		// TODO - implement fully, menu needs to use a different file extension for MACROs as well
		// {"Input Macro", InputRecordingFileV2::InputRecordingType::INPUT_RECORDING_MACRO},
	};

	wxStaticText* m_file_label;
	wxFilePickerCtrl* m_file_picker;
	bool m_file_browsed;
	wxStaticText* m_author_label;
	wxTextCtrl* m_author_input;
	wxStaticText* m_from_label;
	wxChoice* m_from_choice;
	wxButton* m_start_recording;
	wxButton* m_cancel_recording;
};
#endif
