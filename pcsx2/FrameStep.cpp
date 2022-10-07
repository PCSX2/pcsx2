#include "PrecompiledHeader.h"

#include "Counters.h"
#include "DebugTools/Debug.h"
#include "MemoryTypes.h"
#include "gui/MainFrame.h"
#include "FrameStep.h"

FrameStep g_FrameStep;

void FrameStep::CheckPauseStatus()
{
	frame_advance_frame_counter++;
	if (frameAdvancing && frame_advance_frame_counter >= frames_per_frame_advance)
	{
		frameAdvancing = false;
		pauseEmulation = true;
	}
}

void FrameStep::HandlePausing()
{
	if (pauseEmulation && GetCoreThread().IsOpen() && GetCoreThread().IsRunning())
	{
		emulationCurrentlyPaused = true;
		while (emulationCurrentlyPaused && !resumeEmulation) {
			//Sleep(1); // sleep until resumeEmulation is true
			unsigned volatile char i, j;
			i++;
		}
		resumeEmulation = false;
		emulationCurrentlyPaused = false;
	}
}

void FrameStep::FrameAdvance()
{
	frameAdvancing = true;
	frame_advance_frame_counter = 0;
	Resume();
}

bool FrameStep::IsPaused()
{
	return emulationCurrentlyPaused;
}

void FrameStep::Pause()
{
	pauseEmulation = true;
	resumeEmulation = false;
}

void FrameStep::Resume()
{
	pauseEmulation = false;
	resumeEmulation = true;
}
