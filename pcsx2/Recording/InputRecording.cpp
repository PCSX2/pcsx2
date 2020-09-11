/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2019  PCSX2 Dev Team
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
#include "Recording/RecordingControls.h"

#include <vector>


// Tag and save framecount along with savestate
void SaveStateBase::InputRecordingFreeze()
{
	FreezeTag("InputRecording");
	Freeze(g_FrameCount);

#ifndef DISABLE_RECORDING
	if (g_FrameCount > 0 && IsLoading())
	{
		g_InputRecordingData.IncrementUndoCount();
	}
#endif
}

#ifndef DISABLE_RECORDING
InputRecording g_InputRecording;

// Main func for handling controller input data
// - Called by Sio.cpp::sioWriteController
void InputRecording::ControllerInterrupt(u8& data, u8& port, u16& bufCount, u8 buf[])
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

	if (!fInterruptFrame || state == INPUT_RECORDING_MODE_NONE
		// We do not want to record or save the first two
		// bytes in the data returned from LilyPad
		|| bufCount < 3)
	{
		return;
	}

	// Read or Write
	const u8& nowBuf = buf[bufCount];
	if (state == INPUT_RECORDING_MODE_RECORD)
	{
		InputRecordingData.SetTotalFrames(g_FrameCount);
		InputRecordingData.WriteKeyBuffer(g_FrameCount, port, bufCount - 3, nowBuf);
	}
	else if (state == INPUT_RECORDING_MODE_REPLAY)
	{
		if (InputRecordingData.GetTotalFrames() <= g_FrameCount)
		{
			// Pause the emulation but the movie is not closed
			g_RecordingControls.Pause();
			return;
		}
		u8 tmp = 0;
		if (InputRecordingData.ReadKeyBuffer(tmp, g_FrameCount, port, bufCount - 3))
		{
			buf[bufCount] = tmp;
		}
	}
}


// GUI Handler - Stop recording
void InputRecording::Stop()
{
	state = INPUT_RECORDING_MODE_NONE;
	if (InputRecordingData.Close())
	{
		recordingConLog(L"[REC]: InputRecording Recording Stopped.\n");
	}
}

// GUI Handler - Start recording
bool InputRecording::Create(wxString FileName, bool fromSaveState, wxString authorName)
{
	if (!InputRecordingData.OpenNew(FileName, fromSaveState))
	{
		return false;
	}
	// Set emulator version
	InputRecordingData.GetHeader().SetEmulatorVersion();

	// Set author name
	if (!authorName.IsEmpty())
	{
		InputRecordingData.GetHeader().SetAuthor(authorName);
	}
	// Set Game Name
	InputRecordingData.GetHeader().SetGameName(resolveGameName());
	// Write header contents
	InputRecordingData.WriteHeader();
	state = INPUT_RECORDING_MODE_RECORD;
	recordingConLog(wxString::Format(L"[REC]: Started new recording - [%s]\n", FileName));

	// In every case, we reset the g_FrameCount
	g_FrameCount = 0;
	return true;
}

// GUI Handler - Play a recording
bool InputRecording::Play(wxString fileName)
{
	if (state != INPUT_RECORDING_MODE_NONE)
		Stop();

	// Open the file and verify if it can be played
	if (!InputRecordingData.OpenExisting(fileName))
	{
		return false;
	}
	// Either load the savestate, or restart the game
	if (InputRecordingData.FromSaveState())
	{
		if (!CoreThread.IsOpen())
		{
			recordingConLog(L"[REC]: Game is not open, aborting playing input recording which starts on a save-state.\n");
			InputRecordingData.Close();
			return false;
		}
		FILE* ssFileCheck = wxFopen(InputRecordingData.GetFilename() + "_SaveState.p2s", "r");
		if (ssFileCheck == NULL)
		{
			recordingConLog(wxString::Format("[REC]: Could not locate savestate file at location - %s_SaveState.p2s\n", InputRecordingData.GetFilename()));
			InputRecordingData.Close();
			return false;
		}
		fclose(ssFileCheck);
		StateCopy_LoadFromFile(InputRecordingData.GetFilename() + "_SaveState.p2s");
	}
	else
	{
		g_RecordingControls.Unpause();
		sApp.SysExecute();
	}

	// Check if the current game matches with the one used to make the original recording
	if (!g_Conf->CurrentIso.IsEmpty())
	{
		if (resolveGameName() != InputRecordingData.GetHeader().gameName)
		{
			recordingConLog(L"[REC]: Recording was possibly constructed for a different game.\n");
		}
	}
	state = INPUT_RECORDING_MODE_REPLAY;
	recordingConLog(wxString::Format(L"[REC]: Replaying input recording - [%s]\n", InputRecordingData.GetFilename()));
	recordingConLog(wxString::Format(L"[REC]: PCSX2 Version Used: %s\n", InputRecordingData.GetHeader().emu));
	recordingConLog(wxString::Format(L"[REC]: Recording File Version: %d\n", InputRecordingData.GetHeader().version));
	recordingConLog(wxString::Format(L"[REC]: Associated Game Name or ISO Filename: %s\n", InputRecordingData.GetHeader().gameName));
	recordingConLog(wxString::Format(L"[REC]: Author: %s\n", InputRecordingData.GetHeader().author));
	recordingConLog(wxString::Format(L"[REC]: Total Frames: %d\n", InputRecordingData.GetTotalFrames()));
	recordingConLog(wxString::Format(L"[REC]: Undo Count: %d\n", InputRecordingData.GetUndoCount()));
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

// Keybind Handler - Toggle between recording input and not
void InputRecording::RecordModeToggle()
{
	if (state == INPUT_RECORDING_MODE_REPLAY)
	{
		state = INPUT_RECORDING_MODE_RECORD;
		recordingConLog("[REC]: Record mode ON.\n");
	}
	else if (state == INPUT_RECORDING_MODE_RECORD)
	{
		state = INPUT_RECORDING_MODE_REPLAY;
		recordingConLog("[REC]: Replay mode ON.\n");
	}
}

INPUT_RECORDING_MODE InputRecording::GetModeState()
{
	return state;
}

InputRecordingFile& InputRecording::GetInputRecordingData()
{
	return InputRecordingData;
}

bool InputRecording::IsInterruptFrame()
{
	return fInterruptFrame;
}
#endif
