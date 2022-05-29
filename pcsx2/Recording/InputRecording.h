/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#ifndef PCSX2_CORE
// TODO - Vaser - kill with wxWidgets

#include "Recording/InputRecordingFile.h"
#include "Recording/VirtualPad/VirtualPad.h"

class InputRecording
{
public:
	// Initializes all VirtualPad windows with "parent" as their base
	void InitVirtualPadWindows(wxWindow* parent);

	// Save or load PCSX2's global frame counter (g_FrameCount) along with each full/fast boot
	//
	// This is to prevent any inaccuracy issues caused by having a different
	// internal emulation frame count than what it was at the beginning of the
	// original recording
	void RecordingReset();

	// Main handler for ingesting input data and either saving it to the recording file (recording)
	// or mutating it to the contents of the recording file (replaying)
	void ControllerInterrupt(u8& data, u8& port, u16& BufCount, u8 buf[]);

	// The running frame counter for the input recording
	s32 GetFrameCounter();

	InputRecordingFile& GetInputRecordingData();

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

	// Sets up all values and prints console logs pertaining to the start of a recording
	void SetupInitialState(u32 newStartingFrame);

	/// Functions called from GUI

	// Create a new input recording file
	bool Create(wxString filename, const bool fromSaveState, wxString authorName);
	// Play an existing input recording from a file
	// Calls a file dialog if it fails to locate the default base savestate
	bool Play(wxWindow* parent, wxString filename);
	// Stop the active input recording
	void Stop();
	// Displays the VirtualPad window for the chosen pad
	void ShowVirtualPad(const int port);
	// Logs the padData and redraws the virtualPad windows of active pads
	void LogAndRedraw();
	// Resets emulation to the beginning of a recording
	// Calls a file dialog if it fails to locate the base savestate
	void GoToFirstFrame(wxWindow* parent);
	// Resets a recording if the base savestate could not be loaded at the start
	void FailedSavestate();

private:
	enum class InputRecordingMode
	{
		NotActive,
		Recording,
		Replaying,
	};

	static const int CONTROLLER_PORT_ONE = 0;
	static const int CONTROLLER_PORT_TWO = 1;

	// 0x42 is the magic number to indicate the default controller read query
	// See - PAD.cpp::PADpoll - https://github.com/PCSX2/pcsx2/blob/master/pcsx2/PAD/Windows/PAD.cpp#L1255
	static const u8 READ_DATA_AND_VIBRATE_FIRST_BYTE = 0x42;
	// 0x5A is always the second byte in the buffer when the normal READ_DATA_AND_VIBRATE (0x42) query is executed.
	// See - PAD.cpp::PADpoll - https://github.com/PCSX2/pcsx2/blob/master/pcsx2/PAD/Windows/PAD.cpp#L1256
	static const u8 READ_DATA_AND_VIBRATE_SECOND_BYTE = 0x5A;

	// DEPRECATED: Slated for removal
	bool fInterruptFrame = false;
	InputRecordingFile inputRecordingData;
	bool initialLoad = false;
	u32 startingFrame = 0;
	s32 frameCounter = 0;
	bool incrementUndo = false;
	InputRecordingMode state = InputRecording::InputRecordingMode::NotActive;
	wxString savestate;

	// Array of usable pads (currently, only 2)
	struct InputRecordingPad
	{
		// Controller Data
		PadData* padData;
		// VirtualPad
		VirtualPad* virtualPad;
		InputRecordingPad();
		~InputRecordingPad();
	} pads[2];

	// Resolve the name and region of the game currently loaded using the GameDB
	// If the game cannot be found in the DB, the fallback is the ISO filename
	wxString resolveGameName();
};

extern InputRecording g_InputRecording;

#else

#include "Recording/InputRecordingFile.h"

class InputRecording
{
public:
	enum class Type
	{
		POWER_ON,
		FROM_SAVESTATE
	};

	// Save or load PCSX2's global frame counter (g_FrameCount) along with each full/fast boot
	//
	// This is to prevent any inaccuracy issues caused by having a different
	// internal emulation frame count than what it was at the beginning of the
	// original recording
	void RecordingReset();

	// Main handler for ingesting input data and either saving it to the recording file (recording)
	// or mutating it to the contents of the recording file (replaying)
	void ControllerInterrupt(u8& data, u8& port, u16& BufCount, u8 buf[]);

	// The running frame counter for the input recording
	s32 GetFrameCounter();

	InputRecordingFile& GetInputRecordingData();

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

	// Sets input recording to Record Mode
	void SetToRecordMode();

	// Sets input recording to Replay Mode
	void SetToReplayMode();

	// Set the running frame counter for the input recording to an arbitrary value
	void SetFrameCounter(u32 newGFrameCount);

	// Sets up all values and prints console logs pertaining to the start of a recording
	void SetupInitialState(u32 newStartingFrame);

	/// Functions called from GUI

	// Create a new input recording file
	bool Create(const std::string_view& filename, const bool fromSaveState, const std::string_view& authorName);
	// Play an existing input recording from a file
	// TODO - Vaser - Calls a file dialog if it fails to locate the default base savestate
	bool Play(const std::string_view& path);
	// Stop the active input recording
	void Stop();
	// Logs the padData and redraws the virtualPad windows of active pads
	void LogAndRedraw();
	// Resets a recording if the base savestate could not be loaded at the start
	void FailedSavestate();

private:
	enum class InputRecordingMode
	{
		NotActive,
		Recording,
		Replaying,
	};

	static const int CONTROLLER_PORT_ONE = 0;
	static const int CONTROLLER_PORT_TWO = 1;

	// 0x42 is the magic number to indicate the default controller read query
	// See - PAD.cpp::PADpoll - https://github.com/PCSX2/pcsx2/blob/master/pcsx2/PAD/Windows/PAD.cpp#L1255
	static const u8 READ_DATA_AND_VIBRATE_FIRST_BYTE = 0x42;
	// 0x5A is always the second byte in the buffer when the normal READ_DATA_AND_VIBRATE (0x42) query is executed.
	// See - PAD.cpp::PADpoll - https://github.com/PCSX2/pcsx2/blob/master/pcsx2/PAD/Windows/PAD.cpp#L1256
	static const u8 READ_DATA_AND_VIBRATE_SECOND_BYTE = 0x5A;

	// DEPRECATED: Slated for removal
	bool fInterruptFrame = false;
	InputRecordingFile inputRecordingData;
	bool initialLoad = false;
	u32 startingFrame = 0;
	s32 frameCounter = 0;
	bool incrementUndo = false;
	InputRecordingMode state = InputRecording::InputRecordingMode::NotActive;
	std::string savestate;

	// Array of usable pads (currently, only 2)
	struct InputRecordingPad
	{
		// Controller Data
		PadData* padData;
		InputRecordingPad();
		~InputRecordingPad();
	} pads[2];

	// Resolve the name and region of the game currently loaded using the GameDB
	// If the game cannot be found in the DB, the fallback is the ISO filename
	std::string resolveGameName();
};

extern InputRecording g_InputRecording;

#endif