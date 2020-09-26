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

#ifndef DISABLE_RECORDING

#include "Recording/InputRecordingFile.h"
#include "Recording/NewRecordingFrame.h"

class InputRecording
{
public:
	~InputRecording();
	// Initializes Create()'s NewRecordingFrame's, Play()'s wxFileDialog's, and both VirtualPads' windows
	void InitInputRecordingWindows(wxWindow* parent);

	// Save or load PCSX2's global frame counter (g_FrameCount) along with each full/fast boot
	//
	// This is to prevent any inaccuracy issues caused by having a different
	// internal emulation frame count than what it was at the beginning of the
	// original recording
	void OnBoot();

	// Main handler for ingesting input data and either saving it to the recording file (recording)
	// or mutating it to the contents of the recording file (replaying)
	void ControllerInterrupt(const u8 data, const u8 port, const u8 slot, const u16 bufCount, u8& bufVal);

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

	// Store the starting internal PCSX2 g_FrameCount value
	void SetStartingFrame(u32 newStartingFrame);

	/// Functions called from GUI

	// Create a new input recording file
	bool Create();
	// Play an existing input recording from a file
	int Play();
	// Stop the active input recording
	void Stop();
	// Resets emulation to the beginning of a recording
	bool GoToFirstFrame();
	// Displays the VirtualPad window for the chosen pad
	void ShowVirtualPad(int const arrayPosition);

private:
	enum class InputRecordingMode
	{
		NotActive,
		Recording,
		Replaying,
	};

	static const u8 NUM_PORTS = 2;
	static const u8 NUM_SLOTS = 4;

	// 0x42 is the magic number to indicate the default controller read query
	// See - Lilypad.cpp::PADpoll - https://github.com/PCSX2/pcsx2/blob/v1.5.0-dev/plugins/LilyPad/LilyPad.cpp#L1193
	static const u8 READ_DATA_AND_VIBRATE_FIRST_BYTE = 0x42;
	// 0x5A is always the second byte in the buffer when the normal READ_DATA_AND_VIBRATE (0x42) query is executed.
	// See - LilyPad.cpp::PADpoll - https://github.com/PCSX2/pcsx2/blob/v1.5.0-dev/plugins/LilyPad/LilyPad.cpp#L1194
	static const u8 READ_DATA_AND_VIBRATE_SECOND_BYTE = 0x5A;

	std::unique_ptr<NewRecordingFrame> newRecordingFrame;
	std::unique_ptr<wxFileDialog> openFileDialog;
	// DEPRECATED: Slated for removal
	bool fInterruptFrame = false;
	InputRecordingFile inputRecordingData;
	bool initialLoad = false;
	u32 startingFrame = 0;
	s32 frameCounter = 0;
	bool incrementRedo = false;
	InputRecordingMode state = InputRecordingMode::NotActive;

	struct InputRecordingPad
	{
		// Controller data
		std::unique_ptr<PadData> padData;
		// VirtualPad
		std::unique_ptr<VirtualPad> virtualPad;
		// Recording Mode
		InputRecordingMode state;
		// File seek offset
		u8 seekOffset;
		InputRecordingPad();
		~InputRecordingPad();
	} pads[NUM_PORTS][NUM_SLOTS];

	InputRecordingPad& GetPad(const int port, const int slot) noexcept { return pads[port][slot]; }
	void SetPads(const bool newRecording);

	// Holds the multitap and fastboot settings from before loading a recording
	struct Buffer
	{
		bool multitaps[NUM_PORTS] = {false, false};
		bool fastBoot = false;
	} buffers;

	// Resolve the name and region of the game currently loaded using the GameDB
	// If the game cannot be found in the DB, the fallback is the ISO filename
	wxString resolveGameName();
};

extern InputRecording g_InputRecording;

#endif
