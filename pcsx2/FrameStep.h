#pragma once


class FrameStep
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
	void HandlePausing();

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
	// Resume emulation when the next pcsx2 App event is handled
	void Resume();
	// Whether or not to sleep while waiting for resume or just eat cycles
	void SetSleepWait(bool sleep);

private:
	// Indicates if the input recording controls have explicitly paused emulation or not
	bool emulationCurrentlyPaused = false;
	// Indicates on the next VSync if we are frame advancing, this value
	// and should be cleared once a single frame has passed
	bool frameAdvancing = false;
	bool sleepWhileWaiting = false;
	u32 frame_advance_frame_counter = 0;
	u32 frames_per_frame_advance = 1;
	// Indicates if we intend to call CoreThread.PauseSelf() on the current or next available vsync
	bool pauseEmulation = false;
	// Indicates if we intend to call CoreThread.Resume() when the next pcsx2 App event is handled
	bool resumeEmulation = false;
};

extern FrameStep g_FrameStep;
