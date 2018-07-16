#pragma once

class RecordingControls {
public:

	// Movie controls main functions
	bool isStop();
	void StartCheck();
	void StopCheck();

	// Shortcut Keys
	void FrameAdvance();
	void TogglePause();

	// Setters
	void Pause();
	void UnPause();

	// Getters
	bool getStopFlag() { return (fStop || fFrameAdvance); }

private:
	uint stopFrameCount = false;
	
	bool fStop = false;
	bool fStart = false;
	bool fFrameAdvance = false;
	bool fPauseState = false;

};
extern RecordingControls g_RecordingControls;
