#include "PrecompiledHeader.h"

#include "GSFrame.h"

#include "MemoryTypes.h"
#include "App.h"
#include "Counters.h"

#include "RecordingControls.h"


RecordingControls g_RecordingControls;

// TODO - I think these functions could be named a lot better...

//-----------------------------------------------
// Status on whether or not the current recording is stopped
//-----------------------------------------------
bool RecordingControls::isStop()
{
	return (fPauseState && CoreThread.IsOpen() && CoreThread.IsPaused());
}
//-----------------------------------------------
// Called after inputs are recorded for that frame, places lock on input recording, effectively releasing resources and resuming CoreThread.
//-----------------------------------------------
void RecordingControls::StartCheck()
{
	if (fStart && CoreThread.IsOpen() && CoreThread.IsPaused()) {
		CoreThread.Resume();
		fStart = false;
		fPauseState = false;
	}
}

//-----------------------------------------------
// Called at VSYNC End / VRender Begin, updates everything recording related for the next frame, 
// toggles RecordingControl flags back to enable input recording for the next frame.
//-----------------------------------------------
void RecordingControls::StopCheck()
{
	if (fFrameAdvance) {
		if (stopFrameCount < g_FrameCount) {
			fFrameAdvance = false;
			fStop = true;
			stopFrameCount = g_FrameCount;

			// We force the frame counter in the title bar to change
			wxString oldTitle = wxGetApp().GetGsFrame().GetTitle();
			wxString title = g_Conf->Templates.RecordingTemplate;
			wxString frameCount = wxString::Format("%d", g_FrameCount);

			title.Replace(L"${frame}", frameCount);
			int frameIndex = title.find(wxString::Format(L"%d", g_FrameCount));
			frameIndex += frameCount.length();

			title.replace(frameIndex, oldTitle.length() - frameIndex, oldTitle.c_str().AsChar() + frameIndex);

			wxGetApp().GetGsFrame().SetTitle(title);
		}
	}
	if (fStop && CoreThread.IsOpen() && CoreThread.IsRunning())
	{
		fPauseState = true;
		CoreThread.PauseSelf();
	}
}



//----------------------------------
// shortcut key
//----------------------------------
void RecordingControls::FrameAdvance()
{
	stopFrameCount = g_FrameCount;
	fFrameAdvance = true;
	fStop = false;
	fStart = true;
}
void RecordingControls::TogglePause()
{
	fStop = !fStop;
	if (fStop == false) {
		fStart = true;
	}
}
void RecordingControls::Pause()
{
	fStop = true;
	fFrameAdvance = true;
}
void RecordingControls::UnPause()
{
	fStop = false;
	fStart = true;
	fFrameAdvance = true;
}

