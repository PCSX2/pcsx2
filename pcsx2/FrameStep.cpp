#include "PrecompiledHeader.h"

#include "Counters.h"
#include "DebugTools/Debug.h"
#include "MemoryTypes.h"
#include "FrameStep.h"
#include "VMManager.h"
#include "Host.h"
#include <common/Threading.h>

FrameStep g_FrameStep;

void FrameStep::CheckPauseStatus()
{
	frame_advance_frame_counter++;
	if (frameAdvancing && frame_advance_frame_counter >= frames_per_frame_advance)
	{
		frameAdvancing = false;
		pauseEmulation = true;
		resumeEmulation = false;
	}
}

void FrameStep::HandlePausing()
{
	if (pauseEmulation && VMManager::GetState() == VMState::Running)
	{
		emulationCurrentlyPaused = true;
		while (emulationCurrentlyPaused && !resumeEmulation) {
			if (sleepWhileWaiting) { Threading::Sleep(1); } // sleep until resumeEmulation is true
			//else Threading::Sleep(1); // sleep until resumeEmulation is true
			// otherwise just eat cycle until we can
			//volatile int i = 0;
			//i++;
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

void FrameStep::SetSleepWait(bool sleep)
{
	sleepWhileWaiting = sleep;
}
