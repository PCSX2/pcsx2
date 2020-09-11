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

#include "InputRecordingFile.h"


#ifndef DISABLE_RECORDING
enum INPUT_RECORDING_MODE
{
	INPUT_RECORDING_MODE_NONE,
	INPUT_RECORDING_MODE_RECORD,
	INPUT_RECORDING_MODE_REPLAY,
};

class InputRecording
{
public:
	InputRecording() {}
	~InputRecording() {}

	void ControllerInterrupt(u8& data, u8& port, u16& BufCount, u8 buf[]);

	void RecordModeToggle();

	INPUT_RECORDING_MODE GetModeState();
	InputRecordingFile& GetInputRecordingData();
	bool IsInterruptFrame();

	void Stop();
	bool Create(wxString filename, bool fromSaveState, wxString authorName);
	bool Play(wxString filename);

private:
	InputRecordingFile InputRecordingData;
	INPUT_RECORDING_MODE state = INPUT_RECORDING_MODE_NONE;
	bool fInterruptFrame = false;
	// Resolve the name and region of the game currently loaded using the GameDB
	// If the game cannot be found in the DB, the fallback is the ISO filename
	wxString resolveGameName();
};

extern InputRecording g_InputRecording;
static InputRecordingFile& g_InputRecordingData = g_InputRecording.GetInputRecordingData();
static InputRecordingFileHeader& g_InputRecordingHeader = g_InputRecording.GetInputRecordingData().GetHeader();
#endif
