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

class InputRecordingControls
{
public:
	// Intended to be called at the end of each frame, but will no-op if frame lock is active
	//
	// Will set the pausing parameters for emulation if:
	// - The InputRecordingControls::FrameAdvance was hit on the previous frame
	// - Emulation was explicitly paused using InputRecordingControls::TogglePause
	// - We are replaying an input recording and have hit the end
	void CheckPauseStatus();

	// When loading a recording file or booting with a recording active, lock will be enabled.
	// Emulation will be forced into and remain in a paused state until the transition in progress
	// has completed - signaled when g_framecount and frameCountTracker are equal
	//
	// This function also handles actually pausing emulation when told to
	void HandlePausingAndLocking();

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
	void setFrameAdvanceAmount(int amount);
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
	/**
	 * @brief Resumes emulation immediately, don't wait until the next VSync
	*/
	void ResumeImmediately();
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
	u32 frame_advance_frame_counter = 0;
	u32 frames_per_frame_advance = 1;
	// Indicates if we intend to call CoreThread.PauseSelf() on the current or next available vsync
	bool pauseEmulation = false;
	// Indicates if we intend to call CoreThread.Resume() when the next pcsx2 App event is handled
	bool resumeEmulation = false;
	// Indicates to switch to replay mode after the next vsync
	bool switchToReplay = false;
	// Used to stop recording frames from incrementing during a reset
	bool frameLock = false;
	// The frame value to use as the frame lock reset point
	u32 frameLockTracker = 0;
	
	bool IsFinishedReplaying() const;
	// Calls mainEmuFrame's videoCaptureToggle to end a capture if active
	void StopCapture() const;
};

extern InputRecordingControls g_InputRecordingControls;

#else

class InputRecordingControls
{
public:
	// Intended to be called at the end of each frame, but will no-op if frame lock is active
	//
	// Will set the pausing parameters for emulation if:
	// - The InputRecordingControls::FrameAdvance was hit on the previous frame
	// - Emulation was explicitly paused using InputRecordingControls::TogglePause
	// - We are replaying an input recording and have hit the end
	void CheckPauseStatus();

	// When loading a recording file or booting with a recording active, lock will be enabled.
	// Emulation will be forced into and remain in a paused state until the transition in progress
	// has completed - signaled when g_framecount and frameCountTracker are equal
	//
	// This function also handles actually pausing emulation when told to
	void HandlePausingAndLocking();

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
	void setFrameAdvanceAmount(int amount);
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
	/**
	 * @brief Resumes emulation immediately, don't wait until the next VSync
	*/
	void ResumeImmediately();
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
	u32 frame_advance_frame_counter = 0;
	u32 frames_per_frame_advance = 1;
	// Indicates if we intend to call CoreThread.PauseSelf() on the current or next available vsync
	bool pauseEmulation = false;
	// Indicates if we intend to call CoreThread.Resume() when the next pcsx2 App event is handled
	bool resumeEmulation = false;
	// Indicates to switch to replay mode after the next vsync
	bool switchToReplay = false;
	// Used to stop recording frames from incrementing during a reset
	bool frameLock = false;
	// The frame value to use as the frame lock reset point
	u32 frameLockTracker = 0;

	bool IsFinishedReplaying() const;
	// Calls mainEmuFrame's videoCaptureToggle to end a capture if active
	void StopCapture() const;
};

extern InputRecordingControls g_InputRecordingControls;

#endif
