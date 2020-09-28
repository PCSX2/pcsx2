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

#ifndef DISABLE_RECORDING

#include <vector>

#include "AppGameDatabase.h"
#include "DebugTools/Debug.h"
#include "Counters.h"

#include "InputRecording.h"
#include "InputRecordingControls.h"

#endif

void SaveStateBase::InputRecordingFreeze()
{
	FreezeTag("InputRecording");
	Freeze(g_FrameCount);

#ifndef DISABLE_RECORDING
	// Loading a save-state is an asynchronous task. If we are playing a recording
	// that starts from a savestate (not power-on) and the starting (pcsx2 internal) frame
	// marker has not been set (which comes from the save-state), we initialize it.
	if (g_InputRecording.IsInitialLoad())
		g_InputRecording.SetStartingFrame(g_FrameCount);
	else if (g_InputRecording.IsActive())
	{
		// Explicitly set the frame change tracking variable as to not
		// detect saving or loading a savestate as a frame being drawn
		g_InputRecordingControls.SetFrameCountTracker(g_FrameCount);

		if (IsLoading())
			g_InputRecording.SetFrameCounter(g_FrameCount);
	}
#endif
}

#ifndef DISABLE_RECORDING

InputRecording g_InputRecording;

InputRecording::InputRecording()
{
	// NOTE - No multi-tap support, only two controllers
	padData[CONTROLLER_PORT_ONE] = new PadData();
	padData[CONTROLLER_PORT_TWO] = new PadData();
}

void InputRecording::setVirtualPadPtr(VirtualPad* ptr, int const port)
{
	virtualPads[port] = ptr;
}

void InputRecording::RecordingReset()
{
	// Booting is an asynchronous task. If we are playing a recording
	// that starts from power-on and the starting (pcsx2 internal) frame
	// marker has not been set, we initialize it.
	if (g_InputRecording.IsInitialLoad())
		g_InputRecording.SetStartingFrame(0);
	else if (g_InputRecording.IsActive())
	{
		g_InputRecording.SetFrameCounter(0);
		g_InputRecordingControls.Lock(0);
	}
	else
		g_InputRecordingControls.Resume();
}

void InputRecording::ControllerInterrupt(u8& data, u8& port, u16& bufCount, u8 buf[])
{
	// TODO - Multi-Tap Support

	if (bufCount == 1)
		fInterruptFrame = data == READ_DATA_AND_VIBRATE_FIRST_BYTE;
	else if (bufCount == 2 && buf[bufCount] != READ_DATA_AND_VIBRATE_SECOND_BYTE)
		fInterruptFrame = false;

	// We do not want to record or save the first two bytes in the data returned from the PAD plugin
	else if (fInterruptFrame && bufCount >= 3 && frameCounter >= 0 && frameCounter < INT_MAX)
	{
		u8& bufVal = buf[bufCount];
		const u16 bufIndex = bufCount - 3;

		if (state == InputRecordingMode::Replaying)
		{
			u8 tmp = 0;
			if (inputRecordingData.ReadKeyBuffer(tmp, frameCounter, port, bufIndex))
			{
				// Overwrite value originally provided by the PAD plugin
				bufVal = tmp;
			}
		}

		// Update controller data state for future VirtualPad / logging usage.
		padData[port]->UpdateControllerData(bufIndex, bufVal);

		if (virtualPads[port] &&
			virtualPads[port]->IsShown() &&
			virtualPads[port]->UpdateControllerData(bufIndex, padData[port]) &&
			state != InputRecordingMode::Replaying)
		{
			// If the VirtualPad updated the PadData, we have to update the buffer
			// before committing it to the recording / sending it to the game
			// - Do not do this if we are in replay mode!
			bufVal = padData[port]->PollControllerData(bufIndex);
		}

		// If we have reached the end of the pad data, log it out
		if (bufIndex == PadData::END_INDEX_CONTROLLER_BUFFER)
		{
			padData[port]->LogPadData(port);
			// As well as re-render the virtual pad UI, if applicable
			// - Don't render if it's minimized
			if (virtualPads[port] && virtualPads[port]->IsShown() && !virtualPads[port]->IsIconized())
				virtualPads[port]->Redraw();
		}

		// Finally, commit the byte to the movie file if we are recording
		if (state == InputRecordingMode::Recording)
		{
			if (incrementUndo)
			{
				inputRecordingData.IncrementUndoCount();
				incrementUndo = false;
			}
			inputRecordingData.WriteKeyBuffer(frameCounter, port, bufIndex, bufVal);
		}
	}
}

s32 InputRecording::GetFrameCounter()
{
	return frameCounter;
}

InputRecordingFile& InputRecording::GetInputRecordingData()
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
		if (frameCounter == inputRecordingData.GetTotalFrames())
			incrementUndo = false;
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

bool InputRecording::IsInitialLoad()
{
	return initialLoad;
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
		case InputRecordingMode::Replaying:
			return wxString("Replaying");
		default:
			return wxString("No Movie");
	}
}

void InputRecording::SetToRecordMode()
{
	state = InputRecordingMode::Recording;
	virtualPads[CONTROLLER_PORT_ONE]->SetReadOnlyMode(false);
	virtualPads[CONTROLLER_PORT_TWO]->SetReadOnlyMode(false);
	recordingConLog("[REC]: Record mode ON.\n");
}

void InputRecording::SetToReplayMode()
{
	state = InputRecordingMode::Replaying;
	virtualPads[CONTROLLER_PORT_ONE]->SetReadOnlyMode(true);
	virtualPads[CONTROLLER_PORT_TWO]->SetReadOnlyMode(true);
	recordingConLog("[REC]: Replay mode ON.\n");
}

void InputRecording::SetFrameCounter(u32 newGFrameCount)
{
	if (newGFrameCount > startingFrame + (u32)g_InputRecording.GetInputRecordingData().GetTotalFrames())
	{
		recordingConLog(L"[REC]: Warning, you've loaded PCSX2 emulation to a point after the end of the original recording. This should be avoided.\n");
		recordingConLog(L"[REC]: Savestate's framecount has been ignored.\n");
		frameCounter = g_InputRecording.GetInputRecordingData().GetTotalFrames();
		if (state == InputRecordingMode::Replaying)
			SetToRecordMode();
		incrementUndo = false;
	}
	else
	{
		if (newGFrameCount < startingFrame)
		{
			recordingConLog(L"[REC]: Warning, you've loaded PCSX2 emulation to a point before the start of the original recording. This should be avoided.\n");
			if (state == InputRecordingMode::Recording)
				SetToReplayMode();
		}
		else if (newGFrameCount == 0 && state == InputRecordingMode::Recording)
			SetToReplayMode();
		frameCounter = static_cast<s32>(newGFrameCount - startingFrame);
		incrementUndo = true;
	}
}

void InputRecording::SetStartingFrame(u32 newStartingFrame)
{
	startingFrame = newStartingFrame;
	// TODO - make a function of my own to simplify working with the logging macros
	if (inputRecordingData.FromSaveState())
		recordingConLog(wxString::Format(L"[REC]: Internal Starting Frame: %d\n", startingFrame));
	frameCounter = 0;
	initialLoad = false;
	g_InputRecordingControls.Lock(startingFrame);
}

void InputRecording::Stop()
{
	state = InputRecordingMode::NotActive;
	virtualPads[CONTROLLER_PORT_ONE]->SetReadOnlyMode(false);
	virtualPads[CONTROLLER_PORT_TWO]->SetReadOnlyMode(false);
	incrementUndo = false;
	if (inputRecordingData.Close())
		recordingConLog(L"[REC]: InputRecording Recording Stopped.\n");
}

bool InputRecording::Create(wxString FileName, bool fromSaveState, wxString authorName)
{
	if (!inputRecordingData.OpenNew(FileName, fromSaveState))
		return false;

	initialLoad = true;
	if (fromSaveState)
	{
		if (wxFileExists(FileName + "_SaveState.p2s"))
			wxCopyFile(FileName + "_SaveState.p2s", FileName + "_SaveState.p2s.bak", true);
		StateCopy_SaveToFile(FileName + "_SaveState.p2s");
	}
	else
		sApp.SysExecute(g_Conf->CdvdSource);

	// Set emulator version
	inputRecordingData.GetHeader().SetEmulatorVersion();

	// Set author name
	if (!authorName.IsEmpty())
		inputRecordingData.GetHeader().SetAuthor(authorName);

	// Set Game Name
	inputRecordingData.GetHeader().SetGameName(resolveGameName());
	// Write header contents
	inputRecordingData.WriteHeader();
	SetToRecordMode();
	g_InputRecordingControls.DisableFrameAdvance();
	recordingConLog(wxString::Format(L"[REC]: Started new recording - [%s]\n", FileName));
	return true;
}

bool InputRecording::Play(wxString fileName)
{
	if (IsActive())
		Stop();

	if (!inputRecordingData.OpenExisting(fileName))
		return false;

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
			recordingConLog(wxString::Format("[REC]: Could not locate savestate file at location - %s_SaveState.p2s\n",
											 inputRecordingData.GetFilename()));
			inputRecordingData.Close();
			return false;
		}
		initialLoad = true;
		StateCopy_LoadFromFile(inputRecordingData.GetFilename() + "_SaveState.p2s");
	}
	else
	{
		initialLoad = true;
		sApp.SysExecute(g_Conf->CdvdSource);
	}

	// Check if the current game matches with the one used to make the original recording
	if (!g_Conf->CurrentIso.IsEmpty())
		if (resolveGameName() != inputRecordingData.GetHeader().gameName)
			recordingConLog(L"[REC]: Recording was possibly constructed for a different game.\n");

	incrementUndo = true;
	SetToReplayMode();
	g_InputRecordingControls.DisableFrameAdvance();
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
