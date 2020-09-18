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

#include "PrecompiledHeader.h"

#include "App.h"
#include "Counters.h"
#include "DebugTools/Debug.h"
#include "GSFrame.h"
#include "MemoryTypes.h"

#include "InputRecording.h"
#include "InputRecordingControls.h"


#ifndef DISABLE_RECORDING

InputRecordingControls g_InputRecordingControls;


void InputRecordingControls::HandleFrameAdvanceAndPausing()
{
	// This function can be called multiple times per frame via Counters::rcntUpdate_vSync,
	// often this is twice per frame but this may vary as Counters.cpp mentions the
	// vsync timing can change.
	//
	// When loading a recording file or rebooting with a recording active, lock will be enabled.
	// Emulation will be forced into and remain in a paused state until the transition in progress
	// has completed - signaled when g_framecount and frameCountTracker are equal.
	if (frameLock)
	{
		if (g_FrameCount == frameCountTracker)
		{
			frameLock = false;
			g_InputRecordingControls.Resume();
		}
		else if (!emulationCurrentlyPaused && CoreThread.IsOpen() && CoreThread.IsRunning())
		{
			pauseEmulation = true;
			resumeEmulation = false;
			emulationCurrentlyPaused = true;
			CoreThread.PauseSelf();
		}
		return;
	}
	// As a safeguard, use the global g_FrameCount to know when the frame counter has truly changed.
	//
	// NOTE - Do not mutate g_FrameCount or use it's value to set the input recording's internal frame counter
	else if (frameCountTracker != g_FrameCount)
	{
		frameCountTracker = g_FrameCount;
		if (g_InputRecording.GetFrameCounter() < INT_MAX)
			g_InputRecording.IncrementFrameCounter();
	} 
	else
	{
		if (pauseEmulation)
		{
			emulationCurrentlyPaused = true;
			CoreThread.PauseSelf();
		}
		return;
	}

	if (switchToReplay)
	{
		g_InputRecording.SetToReplayMode();
		switchToReplay = false;
	}

	if ((g_InputRecording.IsReplaying() && g_InputRecording.GetFrameCounter() >= g_InputRecording.GetInputRecordingData().GetTotalFrames())
		|| g_InputRecording.GetFrameCounter() == INT_MAX)
		pauseEmulation = true;

	// If we haven't yet advanced atleast a single frame from when we paused, setup things to be paused
	if (frameAdvancing && frameAdvanceMarker < g_InputRecording.GetFrameCounter()) 
	{
		frameAdvancing = false;
		pauseEmulation = true;
	}
	
	// Pause emulation if we need to (either due to frame advancing, or pause has been explicitly toggled on)
	if (pauseEmulation && CoreThread.IsOpen() && CoreThread.IsRunning())
	{
		emulationCurrentlyPaused = true;
		CoreThread.PauseSelf();
	}
}

void InputRecordingControls::ResumeCoreThreadIfStarted()
{
	if (resumeEmulation && CoreThread.IsOpen() && CoreThread.IsPaused())
	{
		CoreThread.Resume();
		resumeEmulation = false;
		emulationCurrentlyPaused = false;
	}
}

void InputRecordingControls::FrameAdvance()
{
	if (g_InputRecording.IsReplaying()
		&& g_InputRecording.GetFrameCounter() >= g_InputRecording.GetInputRecordingData().GetTotalFrames())
	{
		g_InputRecording.SetToRecordMode();
		return;
	}
	frameAdvanceMarker = g_InputRecording.GetFrameCounter();
	frameAdvancing = true;
	Resume();
}

bool InputRecordingControls::IsPaused()
{
	return (emulationCurrentlyPaused && CoreThread.IsOpen() && CoreThread.IsPaused());
}

void InputRecordingControls::Pause()
{
	pauseEmulation = true;
	resumeEmulation = false;
}

void InputRecordingControls::PauseImmediately()
{
	if (CoreThread.IsPaused())
		return;
	Pause();
	if (CoreThread.IsOpen() && CoreThread.IsRunning())
	{
		emulationCurrentlyPaused = true;
		CoreThread.PauseSelf();
	}
}

void InputRecordingControls::Resume()
{
	if (g_InputRecording.IsReplaying()
		&& g_InputRecording.GetFrameCounter() >= g_InputRecording.GetInputRecordingData().GetTotalFrames())
	{
		g_InputRecording.SetToRecordMode();
		return;
	}
	pauseEmulation = false;
	resumeEmulation = true;
}

void InputRecordingControls::SetFrameCountTracker(u32 newFrame)
{
	frameCountTracker = newFrame;
}

void InputRecordingControls::DisableFrameAdvance()
{
	frameAdvancing = false;
}

void InputRecordingControls::TogglePause()
{
	if (pauseEmulation && g_InputRecording.IsReplaying()
		&& g_InputRecording.GetFrameCounter() >= g_InputRecording.GetInputRecordingData().GetTotalFrames())
	{
		g_InputRecording.SetToRecordMode();
		return;
	}
	pauseEmulation = !pauseEmulation;
	resumeEmulation = !pauseEmulation;
}

void InputRecordingControls::RecordModeToggle()
{
	if (IsPaused() || g_InputRecording.IsReplaying()
		|| g_InputRecording.GetFrameCounter() < g_InputRecording.GetInputRecordingData().GetTotalFrames())
	{
		if (g_InputRecording.IsReplaying())
			g_InputRecording.SetToRecordMode();
		else if (g_InputRecording.IsRecording())
			g_InputRecording.SetToReplayMode();
	}
	else if (g_InputRecording.IsRecording())
		switchToReplay = true;
}

void InputRecordingControls::Lock(u32 frame)
{
	frameLock = true;
	frameCountTracker = frame;
	resumeEmulation = false;
	//Ensures that g_frameCount can be used to resume emulation after a fast/full boot
	if (!g_InputRecording.GetInputRecordingData().FromSaveState())
		g_FrameCount = frame + 1;
}
#endif
