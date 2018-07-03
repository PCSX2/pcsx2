#pragma once
#ifndef __KEY_MOVIE_H__
#define __KEY_MOVIE_H__

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
	void Start(wxString filename, bool fReadOnly, VmStateBuffer* ss = nullptr);

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


#endif
