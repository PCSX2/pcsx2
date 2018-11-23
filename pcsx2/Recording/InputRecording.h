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


//----------------------------
// InputRecording
//----------------------------
class InputRecording {
public:
	InputRecording() {}
	~InputRecording(){}
public:
	// controller
	void ControllerInterrupt(u8 &data, u8 &port, u16 & BufCount, u8 buf[]);

	// menu bar
	void Stop();
	void Create(wxString filename, bool fromSaveState, wxString authorName);
	void Play(wxString filename, bool fromSaveState);

	// shortcut key
	void RecordModeToggle();


public:
	enum KEY_MOVIE_MODE {
		NONE,
		RECORD,
		REPLAY,
	};

public:
	// getter
	KEY_MOVIE_MODE getModeState() { return state; }
	InputRecordingFile & getInputRecordingData() {return InputRecordingData;}
	bool isInterruptFrame() { return fInterruptFrame; }

private:
	InputRecordingFile InputRecordingData;
	KEY_MOVIE_MODE state = NONE;
	bool fInterruptFrame = false;


};
extern InputRecording g_InputRecording;
#define g_InputRecordingData (g_InputRecording.getInputRecordingData())
#define g_InputRecordingHeader (g_InputRecording.getInputRecordingData().getHeader())
