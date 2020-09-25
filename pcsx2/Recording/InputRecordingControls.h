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

class InputRecordingControls
{
public:
	// Intended to be called at the end of each frame, but will no-op if the frame count has not
	// truly incremented
	//
	// Increments the input recording's frame counter and will pause emulation if:
	// - The InputRecordingControls::FrameAdvance was hit on the previous frame
	// - Emulation was explicitly paused using InputRecordingControls::TogglePause
	// - We are replaying an input recording and have hit the end
	void HandleFrameAdvanceAndPausing();
	// Called much more frequently than HandleFrameAdvanceAndPausing, instead of being per frame
	// this hooks into pcsx2's main App event handler as it has to be able to resume emulation
	// when drawing frames has compltely stopped
	// 
	// Resumes emulation if:
	// - CoreThread is currently open and paused
	// - We've signaled emulation to be resumed via TogglePause or FrameAdvancing
	void ResumeCoreThreadIfStarted();
	
	// Resume emulation (incase the emulation is currently paused) and pause after a single frame has passed
	void FrameAdvance();
	// Returns true if emulation is currently set up to frame advance.
	bool IsFrameAdvancing();
	// Returns true if the input recording has been paused, which can occur:
	// - After a single frame has passed after InputRecordingControls::FrameAdvance
	// - Explicitly paused via an InputRecordingControls function 
	bool IsPaused();
	// Pause emulation at the next available Vsync
	void Pause();
	// Pause emulation immediately, not waiting for the next Vsync
	void PauseImmediately();
	// Resume emulation when the next pcsx2 App event is handled
	void Resume();
	void SetFrameCountTracker(u32 newFrame);
	// Sets frameAdvancing variable to false
	// Used to restrict a frameAdvanceTracker value from transferring between recordings
	void DisableFrameAdvance();
	// Alternates emulation between a paused and unpaused state
	void TogglePause();
	// Switches between recording and replaying the active input recording file
	void RecordModeToggle();
	// Enables the frame locking mechanism so that when recordings are loaded
	// or when processing a reboot with a recording active that no frames are
	// lost in prior emulation
	void Lock(u32 frame);

private:
	// Indicates if the input recording controls have explicitly paused emulation or not
	bool emulationCurrentlyPaused = false;
	// Indicates on the next VSync if we are frame advancing, this value
	// and should be cleared once a single frame has passed
	bool frameAdvancing = false;
	// The input recording frame that frame advancing began on
	s32 frameAdvanceMarker = 0;
	// Used to detect if the internal PCSX2 g_FrameCount has changed
	u32 frameCountTracker = -1;
	// Indicates if we intend to call CoreThread.PauseSelf() on the current or next available vsync
	bool pauseEmulation = false;
	// Indicates if we intend to call CoreThread.Resume() when the next pcsx2 App event is handled
	bool resumeEmulation = false;
	// Indicates to switch to replay mode after the next vsync
	bool switchToReplay = false;
	// Used to stop recording frames from incrementing during a reset
	bool frameLock = false;
};

extern InputRecordingControls g_InputRecordingControls;

#endif
