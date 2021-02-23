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

#ifndef DISABLE_RECORDING

#include "App.h"
#include "Counters.h"
#include "DebugTools/Debug.h"
#include "MainFrame.h"
#include "MemoryTypes.h"

#include "InputRecording.h"
#include "InputRecordingControls.h"
#include "Utilities/InputRecordingLogger.h"

InputRecordingControls g_InputRecordingControls;

void InputRecordingControls::CheckPauseStatus()
{
	if (frameAdvancing)
	{
		frameAdvancing = false;
		pauseEmulation = true;
	}

	if (g_InputRecording.IsActive())
	{
		g_InputRecording.IncrementFrameCounter();

		if (switchToReplay)
		{
			g_InputRecording.SetToReplayMode();
			switchToReplay = false;
		}

		if (!pauseEmulation &&
			g_InputRecording.IsReplaying() &&
			g_InputRecording.GetFrameCounter() >= g_InputRecording.GetInputRecordingData().GetTotalFrames())
		{
			pauseEmulation = true;
		}
	}
	g_InputRecording.LogAndRedraw();
}

void InputRecordingControls::HandlePausingAndLocking()
{
	// Explicit frame locking
	if (frameLock)
	{
		if (g_FrameCount == frameLockTracker)
		{
			frameLock = false;
			Resume();
		}
		else if (!emulationCurrentlyPaused && CoreThread.IsOpen() && CoreThread.IsRunning())
		{
			emulationCurrentlyPaused = true;
			CoreThread.PauseSelf();
		}
	}
	else if (pauseEmulation && CoreThread.IsOpen() && CoreThread.IsRunning())
	{
		emulationCurrentlyPaused = true;
		CoreThread.PauseSelf();
	}
}

void InputRecordingControls::ResumeCoreThreadIfStarted()
{
	if (resumeEmulation && CoreThread.IsOpen())
	{
		CoreThread.Resume();
		resumeEmulation = false;
		emulationCurrentlyPaused = false;
	}
}

void InputRecordingControls::FrameAdvance()
{
	if (g_InputRecording.IsReplaying() &&
		g_InputRecording.GetFrameCounter() >= g_InputRecording.GetInputRecordingData().GetTotalFrames())
	{
		g_InputRecording.SetToRecordMode();
		return;
	}
	frameAdvancing = true;
	Resume();
}

bool InputRecordingControls::IsFrameAdvancing()
{
	return frameAdvancing;
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
	if (g_InputRecording.IsReplaying() &&
		g_InputRecording.GetFrameCounter() >= g_InputRecording.GetInputRecordingData().GetTotalFrames())
	{
		g_InputRecording.SetToRecordMode();
		return;
	}
	pauseEmulation = false;
	resumeEmulation = true;
}

void InputRecordingControls::TogglePause()
{
	if (pauseEmulation &&
		g_InputRecording.IsReplaying() &&
		g_InputRecording.GetFrameCounter() >= g_InputRecording.GetInputRecordingData().GetTotalFrames())
	{
		g_InputRecording.SetToRecordMode();
		return;
	}
	resumeEmulation = pauseEmulation;
	pauseEmulation = !pauseEmulation;
	inputRec::log(pauseEmulation ? "Paused Emulation" : "Resumed Emulation");
}

void InputRecordingControls::RecordModeToggle()
{
	if (IsPaused() ||
		g_InputRecording.IsReplaying() ||
		g_InputRecording.GetFrameCounter() < g_InputRecording.GetInputRecordingData().GetTotalFrames())
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
	frameLockTracker = frame;
	frameAdvancing = false;
	//Ensures that g_frameCount can be used to resume emulation after a fast/full boot
	if (!g_InputRecording.GetInputRecordingData().FromSaveState())
		g_FrameCount = frame + 1;
	else
		sMainFrame.StartInputRecording();
}
#endif
