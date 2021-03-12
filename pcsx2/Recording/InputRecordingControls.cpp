/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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
	frame_advance_frame_counter++;
	if (frameAdvancing && frame_advance_frame_counter >= frames_per_frame_advance)
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
		
		if (IsFinishedReplaying() || g_InputRecording.GetFrameCounter() == INT_MAX)
		{
			if (!pauseEmulation)
				pauseEmulation = true;
			StopCapture();
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
	if (!IsFinishedReplaying())
	{
		frameAdvancing = true;
		frame_advance_frame_counter = 0;
		Resume();
	}
	else
		g_InputRecording.SetToRecordMode();
}

void InputRecordingControls::setFrameAdvanceAmount(int amount)
{
	frames_per_frame_advance = amount;
}

bool InputRecordingControls::IsFrameAdvancing()
{
	return frameAdvancing;
}

bool InputRecordingControls::IsPaused()
{
	return emulationCurrentlyPaused && CoreThread.IsOpen() && CoreThread.IsPaused();
}

void InputRecordingControls::Pause()
{
	pauseEmulation = true;
	resumeEmulation = false;
}

void InputRecordingControls::PauseImmediately()
{
	if (!CoreThread.IsPaused())
	{
		Pause();
		if (CoreThread.IsOpen() && CoreThread.IsRunning())
		{
			emulationCurrentlyPaused = true;
			CoreThread.PauseSelf();
		}
	}
}

void InputRecordingControls::Resume()
{
	if (!IsFinishedReplaying())
	{
		pauseEmulation = false;
		resumeEmulation = true;
	}
	else
		g_InputRecording.SetToRecordMode();
}

void InputRecordingControls::ResumeImmediately()
{
	if (CoreThread.IsPaused())
	{
		Resume();
		if (CoreThread.IsRunning())
		{
			emulationCurrentlyPaused = false;
			CoreThread.Resume();
		}
	}
}

void InputRecordingControls::TogglePause()
{
	if (!pauseEmulation || !IsFinishedReplaying())
	{
		resumeEmulation = pauseEmulation;
		pauseEmulation = !pauseEmulation;
		inputRec::log(pauseEmulation ? "Paused Emulation" : "Resumed Emulation");
	}
	else
		g_InputRecording.SetToRecordMode();
}

void InputRecordingControls::RecordModeToggle()
{
	if (g_InputRecording.IsReplaying())
		g_InputRecording.SetToRecordMode();
	else if (g_InputRecording.IsRecording())
	{
		if (IsPaused() || g_InputRecording.GetFrameCounter() < g_InputRecording.GetInputRecordingData().GetTotalFrames())
			g_InputRecording.SetToReplayMode();
		else
			switchToReplay = true;
	}
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

bool InputRecordingControls::IsFinishedReplaying() const
{
	return g_InputRecording.IsReplaying() &&
			g_InputRecording.GetFrameCounter() >= g_InputRecording.GetInputRecordingData().GetTotalFrames();
}

void InputRecordingControls::StopCapture() const
{
	if (MainEmuFrame* mainFrame = GetMainFramePtr())
	{
		if (mainFrame->IsCapturing())
		{
			mainFrame->VideoCaptureToggle();
			inputRec::log("Capture completed");
		}
	}
}
#endif
