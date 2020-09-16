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

#pragma once

#include "InputRecordingFile.h"


#ifndef DISABLE_RECORDING
class InputRecording
{
public:
	// Save or load PCSX2's global frame counter (g_FrameCount) along with each full/fast boot
	//
	// This is to prevent any inaccuracy issues caused by having a different
	// internal emulation frame count than what it was at the beginning of the
	// original recording
	void RecordingReset();

	// Main handler for ingesting input data and either saving it to the recording file (recording)
	// or mutating it to the contents of the recording file (replaying)
	void ControllerInterrupt(u8 &data, u8 &port, u16 &BufCount, u8 buf[]);

	// The running frame counter for the input recording
	s32 GetFrameCounter();

	InputRecordingFile &GetInputRecordingData();

	// The internal PCSX2 g_FrameCount value on the first frame of the recording
	u32 GetStartingFrame();

	void IncrementFrameCounter();

	// DEPRECATED: Slated for removal 
	// If the current frame contains controller / input data
	bool IsInterruptFrame();

	// If there is currently an input recording being played back or actively being recorded
	bool IsActive();

	// Whether or not the recording's initial state has yet to be loaded or saved and 
	// the rest of the recording can be initialized
	// This is not applicable to recordings from a "power-on" state
	bool IsInitialLoad();

	// If there is currently an input recording being played back
	bool IsReplaying();

	// If there are inputs currently being recorded to a file
	bool IsRecording();

	// String representation of the current recording mode to be interpolated into the title
	wxString RecordingModeTitleSegment();

	// Sets input recording to Record Mode
	void SetToRecordMode();

	// Sets input recording to Replay Mode
	void SetToReplayMode();

	// Set the running frame counter for the input recording to an arbitrary value
	void SetFrameCounter(u32 newGFrameCount);

	// Store the starting internal PCSX2 g_FrameCount value
	void SetStartingFrame(u32 newStartingFrame);
	
	/// Functions called from GUI

	// Create a new input recording file
	bool Create(wxString filename, bool fromSaveState, wxString authorName);
	// Play an existing input recording from a file
	bool Play(wxString filename);
	// Stop the active input recording
	void Stop();

private:
	enum class InputRecordingMode
	{
		NotActive,
		Recording,
		Replaying,
	};

	// DEPRECATED: Slated for removal 
	bool fInterruptFrame = false;
	InputRecordingFile inputRecordingData;
	bool initialLoad = false;
	u32 startingFrame = 0;
	s32 frameCounter = 0;
	bool incrementUndo = false;
	InputRecordingMode state = InputRecording::InputRecordingMode::NotActive;
	

	// Resolve the name and region of the game currently loaded using the GameDB
	// If the game cannot be found in the DB, the fallback is the ISO filename
	wxString resolveGameName();
};

extern InputRecording g_InputRecording;
#endif
