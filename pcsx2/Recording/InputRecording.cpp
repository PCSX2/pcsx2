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

#include "AppSaveStates.h"
#include "AppGameDatabase.h"
#include "Common.h"
#include "Counters.h"
#include "MemoryTypes.h"
#include "SaveState.h"

#include "InputRecording.h"
#include "InputRecordingControls.h"

#include <vector>


// Save or load PCSX2's global frame counter (g_FrameCount) along with each savestate
//
// This is to prevent any inaccuracy issues caused by having a different
// internal emulation frame count than what it was at the beginning of the
// original recording
void SaveStateBase::InputRecordingFreeze()
{
	FreezeTag("InputRecording");
	Freeze(g_FrameCount);

#ifndef DISABLE_RECORDING
	if (g_InputRecording.IsActive())
	{
		// Explicitly set the frame change tracking variable as to not
		// detect loading a savestate as a frame being drawn
		g_InputRecordingControls.SetFrameCountTracker(g_FrameCount);

		// If the starting savestate has been loaded (on a current-frame recording) and a save-state is loaded while recording 
		// or replaying the movie it is an undo operation that needs to be tracked.
		//
		// The reason it's also an undo operation when replaying is because the user can switch modes at any time
		// and begin undoing.  While this isn't impossible to resolve, it's a separate issue and not of the utmost importance (this is just interesting metadata)
		if (IsLoading() && !g_InputRecording.IsSavestateInitializing())
		{
			g_InputRecording.GetInputRecordingData().IncrementUndoCount();
			// Reloading a save-state means the internal recording frame counter may need to be adjusted
			// Since we persist the g_FrameCount of the beginning of the movie, we can use it to recalculate it
			u32 newFrameCounter = g_FrameCount - (g_InputRecording.GetStartingFrame());
			// It is possible for someone to load a savestate outside of the original recording's context
			// this should be avoided (hence the log) but I don't think there is a mechanism to reverse loading
			// the save-state
			// Therefore, the best we can do is limit the frame counter within the min/max of the recording
			if (newFrameCounter < 0)
			{
				newFrameCounter = 0;
				recordingConLog(L"[REC]: Warning, you loaded a savestate outside of the bounds of the original recording. This should be avoided. Savestate's framecount has been ignored.\n");
			} 
			else if (newFrameCounter >= g_InputRecording.GetInputRecordingData().GetTotalFrames())
			{
				newFrameCounter = g_InputRecording.GetInputRecordingData().GetTotalFrames();
				recordingConLog(L"[REC]: Warning, you loaded a savestate outside of the bounds of the original recording. This should be avoided. Savestate's framecount has been ignored.\n");
			}
			g_InputRecording.SetFrameCounter(newFrameCounter);
		}
	}
	// Loading a save-state is an asynchronous task, if we are playing a recording
	// that starts from a savestate (not power-on) and the starting (pcsx2 internal) frame
	// marker has not been set (which comes from the save-state), we initialize it.
	if (g_InputRecording.IsSavestateInitializing())
	{
		g_InputRecording.SetStartingFrame(g_FrameCount);
		g_InputRecording.SavestateInitialized();
		// TODO - make a function of my own to simplify working with the logging macros
		recordingConLog(wxString::Format(L"[REC]: Internal Starting Frame: %d\n", g_InputRecording.GetStartingFrame()));
	}
#endif
}

#ifndef DISABLE_RECORDING

InputRecording g_InputRecording;

void InputRecording::ControllerInterrupt(u8 &data, u8 &port, u16 &bufCount, u8 buf[])
{
	// TODO - Multi-Tap Support

	/*
		This appears to try to ensure that we are only paying attention
		to the frames that matter, the ones that are reading from
		the controller.

		See - Lilypad.cpp::PADpoll - https://github.com/PCSX2/pcsx2/blob/v1.5.0-dev/plugins/LilyPad/LilyPad.cpp#L1193
		0x42 is the magic number for the default read query
	*/
	if (bufCount == 1)
	{
		fInterruptFrame = data == 0x42;
		if (!fInterruptFrame)
		{
			return;
		}
	}
	else if (bufCount == 2)
	{
		/*
			See - LilyPad.cpp::PADpoll - https://github.com/PCSX2/pcsx2/blob/v1.5.0-dev/plugins/LilyPad/LilyPad.cpp#L1194
			0x5A is always the second byte in the buffer
			when the normal READ_DATA_AND_VIBRRATE (0x42)
			query is executed, this looks like a sanity check
		*/
		if (buf[bufCount] != 0x5A)
		{
			fInterruptFrame = false;
			return;
		}
	}

	// We do not want to record or save the first two bytes in the data returned from the PAD plugin
	if (!fInterruptFrame || state == InputRecordingMode::NotActive || bufCount < 3)
	{
		return;
	}

	// Read or Write
	const u8 &nowBuf = buf[bufCount];
	if (state == InputRecordingMode::Recording)
	{
		inputRecordingData.WriteKeyBuffer(frameCounter, port, bufCount - 3, nowBuf);
	}
	else if (state == InputRecordingMode::Replaying)
	{
		u8 tmp = 0;
		if (inputRecordingData.ReadKeyBuffer(tmp, frameCounter, port, bufCount - 3))
		{
			buf[bufCount] = tmp;
		}
	}
}

u32 InputRecording::GetFrameCounter()
{
	return frameCounter;
}

InputRecordingFile &InputRecording::GetInputRecordingData()
{
	return inputRecordingData;
}

u32 InputRecording::GetStartingFrame()
{
	return startingFrame;
}

void InputRecording::IncrementFrameCounter()
{
	frameCounter++;
	if (state == InputRecordingMode::Recording)
	{
		GetInputRecordingData().SetTotalFrames(frameCounter);
	}
}

bool InputRecording::IsInterruptFrame()
{
	return fInterruptFrame;
}

bool InputRecording::IsActive()
{
	return state != InputRecordingMode::NotActive;
}

bool InputRecording::IsSavestateInitializing()
{
	return savestateInitializing;
}

bool InputRecording::IsReplaying()
{
	return state == InputRecordingMode::Replaying;
}

bool InputRecording::IsRecording()
{
	return state == InputRecordingMode::Recording;
}

wxString InputRecording::RecordingModeTitleSegment()
{
	switch (state)
	{
		case InputRecordingMode::Recording:
			return wxString("Recording");
			break;
		case InputRecordingMode::Replaying:
			return wxString("Replaying");
			break;
		default:
			return wxString("No Movie");
			break;
	}
}

void InputRecording::SetToRecordMode()
{
	state = InputRecordingMode::Recording;
	recordingConLog("[REC]: Record mode ON.\n");
}

void InputRecording::SetToReplayMode()
{
	state = InputRecordingMode::Replaying;
	recordingConLog("[REC]: Replay mode ON.\n");
}

void InputRecording::RecordModeToggle()
{
	if (state == InputRecordingMode::Replaying)
	{
		SetToRecordMode();
	}
	else if (state == InputRecordingMode::Recording)
	{
		SetToReplayMode();
	}
}

void InputRecording::SavestateInitialized()
{
	savestateInitializing = false;
}

void InputRecording::SetFrameCounter(u32 newFrameCounter)
{
	frameCounter = newFrameCounter;
	if (state == InputRecordingMode::Recording)
	{
		GetInputRecordingData().SetTotalFrames(frameCounter);
	}
}

void InputRecording::SetStartingFrame(u32 newStartingFrame)
{
	startingFrame = newStartingFrame;
}

void InputRecording::Stop()
{
	// Reset the frame counter when starting a new recording
	frameCounter = 0;
	startingFrame = 0;
	savestateInitializing = false;
	state = InputRecordingMode::NotActive;
	if (inputRecordingData.Close())
	{
		recordingConLog(L"[REC]: InputRecording Recording Stopped.\n");
	}
}

bool InputRecording::Create(wxString FileName, bool fromSaveState, wxString authorName)
{
	savestateInitializing = fromSaveState;
	if (!inputRecordingData.OpenNew(FileName, fromSaveState))
	{
		return false;
	}
	// Set emulator version
	inputRecordingData.GetHeader().SetEmulatorVersion();

	// Set author name
	if (!authorName.IsEmpty())
	{
		inputRecordingData.GetHeader().SetAuthor(authorName);
	}

	// Set Game Name
	inputRecordingData.GetHeader().SetGameName(resolveGameName());
	// Write header contents
	inputRecordingData.WriteHeader();
	state = InputRecordingMode::Recording;
	recordingConLog(wxString::Format(L"[REC]: Started new recording - [%s]\n", FileName));
	return true;
}

bool InputRecording::Play(wxString fileName)
{
	if (IsActive())
		Stop();

	if (!inputRecordingData.OpenExisting(fileName))
	{
		return false;
	}
	// Either load the savestate, or restart the game
	if (inputRecordingData.FromSaveState())
	{
		if (!CoreThread.IsOpen())
		{
			recordingConLog(L"[REC]: Game is not open, aborting playing input recording which starts on a save-state.\n");
			inputRecordingData.Close();
			return false;
		}
		if (!wxFileExists(inputRecordingData.GetFilename() + "_SaveState.p2s"))
		{
			recordingConLog(wxString::Format("[REC]: Could not locate savestate file at location - %s_SaveState.p2s\n", inputRecordingData.GetFilename()));
			inputRecordingData.Close();
			return false;
		}
		savestateInitializing = true;
		StateCopy_LoadFromFile(inputRecordingData.GetFilename() + "_SaveState.p2s");
	}
	else
		sApp.SysExecute();

	// Check if the current game matches with the one used to make the original recording
	if (!g_Conf->CurrentIso.IsEmpty())
	{
		if (resolveGameName() != inputRecordingData.GetHeader().gameName)
		{
			recordingConLog(L"[REC]: Recording was possibly constructed for a different game.\n");
		}
	}
	state = InputRecordingMode::Replaying;
	recordingConLog(wxString::Format(L"[REC]: Replaying input recording - [%s]\n", inputRecordingData.GetFilename()));
	recordingConLog(wxString::Format(L"[REC]: PCSX2 Version Used: %s\n", inputRecordingData.GetHeader().emu));
	recordingConLog(wxString::Format(L"[REC]: Recording File Version: %d\n", inputRecordingData.GetHeader().version));
	recordingConLog(wxString::Format(L"[REC]: Associated Game Name or ISO Filename: %s\n", inputRecordingData.GetHeader().gameName));
	recordingConLog(wxString::Format(L"[REC]: Author: %s\n", inputRecordingData.GetHeader().author));
	recordingConLog(wxString::Format(L"[REC]: Total Frames: %d\n", inputRecordingData.GetTotalFrames()));
	recordingConLog(wxString::Format(L"[REC]: Undo Count: %d\n", inputRecordingData.GetUndoCount()));
	return true;
}

wxString InputRecording::resolveGameName()
{
	// Code loosely taken from AppCoreThread::_ApplySettings to resolve the Game Name
	wxString gameName;
	const wxString gameKey(SysGetDiscID());
	if (!gameKey.IsEmpty())
	{
		if (IGameDatabase* GameDB = AppHost_GetGameDatabase())
		{
			Game_Data game;
			if (GameDB->findGame(game, gameKey))
			{
				gameName = game.getString("Name");
				gameName += L" (" + game.getString("Region") + L")";
			}
		}
	}
	return !gameName.IsEmpty() ? gameName : Path::GetFilename(g_Conf->CurrentIso);
}

#endif
