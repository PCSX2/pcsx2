/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#ifndef PCSX2_CORE
// TODO - Vaser - kill with wxWidgets

#include "common/StringUtil.h"
#include "SaveState.h"
#include "Counters.h"
#include "SaveState.h"

#include "gui/App.h"
#include "gui/AppSaveStates.h"
#include "DebugTools/Debug.h"
#include "GameDatabase.h"

#include "InputRecording.h"
#include "InputRecordingControls.h"
#include "Utilities/InputRecordingLogger.h"

#include <fmt/format.h>

void SaveStateBase::InputRecordingFreeze()
{
	// NOTE - BE CAREFUL
	// CHANGING THIS WILL BREAK BACKWARDS COMPATIBILITY ON SAVESTATES
	FreezeTag("InputRecording");
	Freeze(g_FrameCount);

	if (g_Conf->EmuOptions.EnableRecordingTools)
	{
		// Loading a save-state is an asynchronous task. If we are playing a recording
		// that starts from a savestate (not power-on) and the starting (pcsx2 internal) frame
		// marker has not been set (which comes from the save-state), we initialize it.
		if (g_InputRecording.IsInitialLoad())
			g_InputRecording.SetupInitialState(g_FrameCount);
		else if (g_InputRecording.IsActive() && IsLoading())
			g_InputRecording.SetFrameCounter(g_FrameCount);
	}
}

InputRecording g_InputRecording;

InputRecording::InputRecordingPad::InputRecordingPad()
{
	padData = new PadData;
}

InputRecording::InputRecordingPad::~InputRecordingPad()
{
	delete padData;
}

void InputRecording::InitVirtualPadWindows(wxWindow* parent)
{
	for (int port = 0; port < 2; ++port)
		if (!pads[port].virtualPad)
			pads[port].virtualPad = new VirtualPad(parent, port, g_Conf->inputRecording);
}

void InputRecording::ShowVirtualPad(const int port)
{
	pads[port].virtualPad->Show();
}

void InputRecording::RecordingReset()
{
	// Booting is an asynchronous task. If we are playing a recording
	// that starts from power-on and the starting (pcsx2 internal) frame
	// marker has not been set, we initialize it.
	if (g_InputRecording.IsInitialLoad())
		g_InputRecording.SetupInitialState(0);
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
	else if (bufCount == 2)
	{
		if (buf[bufCount] != READ_DATA_AND_VIBRATE_SECOND_BYTE)
			fInterruptFrame = false;
	}
	else if (fInterruptFrame)
	{
		u8& bufVal = buf[bufCount];
		const u16 bufIndex = bufCount - 3;
		if (state == InputRecordingMode::Replaying)
		{
			if (frameCounter >= 0 && frameCounter < INT_MAX)
			{
				if (!inputRecordingData.ReadKeyBuffer(bufVal, frameCounter, port, bufIndex))
					inputRec::consoleLog(fmt::format("Failed to read input data at frame {}", frameCounter));

				// Update controller data state for future VirtualPad / logging usage.
				pads[port].padData->UpdateControllerData(bufIndex, bufVal);

				if (pads[port].virtualPad->IsShown())
					pads[port].virtualPad->UpdateControllerData(bufIndex, pads[port].padData);
			}
		}
		else
		{
			// Update controller data state for future VirtualPad / logging usage.
			pads[port].padData->UpdateControllerData(bufIndex, bufVal);

			// Commit the byte to the movie file if we are recording
			if (state == InputRecordingMode::Recording)
			{
				if (frameCounter >= 0)
				{
					// If the VirtualPad updated the PadData, we have to update the buffer
					// before committing it to the recording / sending it to the game
					if (pads[port].virtualPad->IsShown() && pads[port].virtualPad->UpdateControllerData(bufIndex, pads[port].padData))
						bufVal = pads[port].padData->PollControllerData(bufIndex);

					if (incrementUndo)
					{
						inputRecordingData.IncrementUndoCount();
						incrementUndo = false;
					}

					if (frameCounter < INT_MAX && !inputRecordingData.WriteKeyBuffer(frameCounter, port, bufIndex, bufVal))
						inputRec::consoleLog(fmt::format("Failed to write input data at frame {}", frameCounter));
				}
			}
			// If the VirtualPad updated the PadData, we have to update the buffer
			// before sending it to the game
			else if (pads[port].virtualPad && pads[port].virtualPad->IsShown() && pads[port].virtualPad->UpdateControllerData(bufIndex, pads[port].padData))
				bufVal = pads[port].padData->PollControllerData(bufIndex);
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
	if (frameCounter < INT_MAX)
	{
		frameCounter++;
		switch (state)
		{
			case InputRecordingMode::Recording:
				inputRecordingData.SetTotalFrames(frameCounter);
				[[fallthrough]];
			case InputRecordingMode::Replaying:
				if (frameCounter == inputRecordingData.GetTotalFrames())
					incrementUndo = false;
				break;
			case InputRecordingMode::NotActive: // Does nothing but keep GCC happy.
				break;
		}
	}
}

void InputRecording::LogAndRedraw()
{
	for (u8 port = 0; port < 2; port++)
	{
		pads[port].padData->LogPadData(port);
		// As well as re-render the virtual pad UI, if applicable
		// - Don't render if it's minimized
		if (pads[port].virtualPad->IsShown() && !pads[port].virtualPad->IsIconized())
			pads[port].virtualPad->Redraw();
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
	pads[CONTROLLER_PORT_ONE].virtualPad->SetReadOnlyMode(false);
	pads[CONTROLLER_PORT_TWO].virtualPad->SetReadOnlyMode(false);
	inputRec::log("Record mode ON");
}

void InputRecording::SetToReplayMode()
{
	state = InputRecordingMode::Replaying;
	pads[CONTROLLER_PORT_ONE].virtualPad->SetReadOnlyMode(true);
	pads[CONTROLLER_PORT_TWO].virtualPad->SetReadOnlyMode(true);
	inputRec::log("Replay mode ON");
}

void InputRecording::SetFrameCounter(u32 newGFrameCount)
{
	if (newGFrameCount > startingFrame + (u32)inputRecordingData.GetTotalFrames())
	{
		inputRec::consoleLog("Warning, you've loaded PCSX2 emulation to a point after the end of the original recording. This should be avoided.");
		inputRec::consoleLog("Savestate's framecount has been ignored.");
		frameCounter = inputRecordingData.GetTotalFrames();
		if (state == InputRecordingMode::Replaying)
			SetToRecordMode();
		incrementUndo = false;
	}
	else
	{
		if (newGFrameCount < startingFrame)
		{
			inputRec::consoleLog("Warning, you've loaded PCSX2 emulation to a point before the start of the original recording. This should be avoided.");
			if (state == InputRecordingMode::Recording)
				SetToReplayMode();
		}
		else if (newGFrameCount == 0 && state == InputRecordingMode::Recording)
			SetToReplayMode();
		frameCounter = newGFrameCount - (s32)startingFrame;
		incrementUndo = true;
	}
}

void InputRecording::SetupInitialState(u32 newStartingFrame)
{
	startingFrame = newStartingFrame;
	if (state != InputRecordingMode::Replaying)
	{
		inputRec::log("Started new input recording");
		inputRec::consoleLog(fmt::format("Filename {}", inputRecordingData.GetFilename().ToUTF8()));
		SetToRecordMode();
	}
	else
	{
		// Check if the current game matches with the one used to make the original recording
		if (!g_Conf->CurrentIso.IsEmpty())
			if (resolveGameName() != inputRecordingData.GetHeader().gameName)
				inputRec::consoleLog("Input recording was possibly constructed for a different game.");

		incrementUndo = true;
		inputRec::log("Replaying input recording");
		inputRec::consoleMultiLog({fmt::format("File: {}", inputRecordingData.GetFilename().ToUTF8()),
			fmt::format("PCSX2 Version Used: {}", std::string(inputRecordingData.GetHeader().emu)),
			fmt::format("Recording File Version: {}", inputRecordingData.GetHeader().version),
			fmt::format("Associated Game Name or ISO Filename: {}", std::string(inputRecordingData.GetHeader().gameName)),
			fmt::format("Author: {}", inputRecordingData.GetHeader().author),
			fmt::format("Total Frames: {}", inputRecordingData.GetTotalFrames()),
			fmt::format("Undo Count: {}", inputRecordingData.GetUndoCount())});
		SetToReplayMode();
	}

	if (inputRecordingData.FromSaveState())
		inputRec::consoleLog(fmt::format("Internal Starting Frame: {}", startingFrame));
	frameCounter = 0;
	initialLoad = false;
	g_InputRecordingControls.Lock(startingFrame);
}

void InputRecording::FailedSavestate()
{
	inputRec::consoleLog(fmt::format("{} is not compatible with this version of PCSX2", savestate.ToUTF8()));
	inputRec::consoleLog(fmt::format("Original PCSX2 version used: {}", inputRecordingData.GetHeader().emu));
	inputRecordingData.Close();
	initialLoad = false;
	state = InputRecordingMode::NotActive;
	g_InputRecordingControls.Resume();
}

void InputRecording::Stop()
{
	state = InputRecordingMode::NotActive;
	pads[CONTROLLER_PORT_ONE].virtualPad->SetReadOnlyMode(false);
	pads[CONTROLLER_PORT_TWO].virtualPad->SetReadOnlyMode(false);
	incrementUndo = false;
	if (inputRecordingData.Close())
		inputRec::log("Input recording stopped");
}

bool InputRecording::Create(wxString fileName, const bool fromSaveState, wxString authorName)
{
	if (!inputRecordingData.OpenNew(fileName, fromSaveState))
		return false;

	initialLoad = true;
	state = InputRecordingMode::Recording;
	if (fromSaveState)
	{
		savestate = fileName + "_SaveState.p2s";
		if (wxFileExists(savestate))
			wxCopyFile(savestate, savestate + ".bak", true);
		StateCopy_SaveToFile(savestate);
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
	return true;
}

bool InputRecording::Play(wxWindow* parent, wxString filename)
{
	if (!inputRecordingData.OpenExisting(filename))
		return false;

	// Either load the savestate, or restart the game
	if (inputRecordingData.FromSaveState())
	{
		if (!GetCoreThread().IsOpen())
		{
			inputRec::consoleLog("Game is not open, aborting playing input recording which starts on a save-state.");
			inputRecordingData.Close();
			return false;
		}

		savestate = inputRecordingData.GetFilename() + "_SaveState.p2s";
		if (!wxFileExists(savestate))
		{
			wxFileDialog loadStateDialog(parent, _("Select the savestate that will accompany this recording"), L"", L"",
				L"Savestate files (*.p2s)|*.p2s", wxFD_OPEN);
			if (loadStateDialog.ShowModal() == wxID_CANCEL)
			{
				inputRec::consoleLog(fmt::format("Could not locate savestate file at location - {}", savestate.ToUTF8()));
				inputRec::log("Savestate load failed");
				inputRecordingData.Close();
				return false;
			}

			savestate = loadStateDialog.GetPath();
			inputRec::consoleLog(fmt::format("Base savestate set to {}", savestate.ToUTF8()));
		}
		state = InputRecordingMode::Replaying;
		initialLoad = true;
		StateCopy_LoadFromFile(savestate);
	}
	else
	{
		state = InputRecordingMode::Replaying;
		initialLoad = true;
		sApp.SysExecute(g_Conf->CdvdSource);
	}
	return true;
}

void InputRecording::GoToFirstFrame(wxWindow* parent)
{
	if (inputRecordingData.FromSaveState())
	{
		if (!wxFileExists(savestate))
		{
			const bool initiallyPaused = g_InputRecordingControls.IsPaused();

			if (!initiallyPaused)
				g_InputRecordingControls.PauseImmediately();

			inputRec::consoleLog(fmt::format("Could not locate savestate file at location - {}\n", savestate.ToUTF8()));
			wxFileDialog loadStateDialog(parent, _("Select a savestate to accompany the recording with"), L"", L"",
				L"Savestate files (*.p2s)|*.p2s", wxFD_OPEN);
			int result = loadStateDialog.ShowModal();
			if (!initiallyPaused)
				g_InputRecordingControls.Resume();

			if (result == wxID_CANCEL)
			{
				inputRec::log("Savestate load cancelled");
				return;
			}
			savestate = loadStateDialog.GetPath();
			inputRec::consoleLog(fmt::format("Base savestate swapped to {}", savestate.ToUTF8()));
		}
		StateCopy_LoadFromFile(savestate);
	}
	else
		sApp.SysExecute(g_Conf->CdvdSource);

	if (IsRecording())
		SetToReplayMode();
}

wxString InputRecording::resolveGameName()
{
	std::string gameName;
	const std::string gameKey(SysGetDiscID());
	if (!gameKey.empty())
	{
		auto game = GameDatabase::findGame(gameKey);
		if (game)
		{
			gameName = game->name;
			gameName += " (" + game->region + ")";
		}
	}
	return !gameName.empty() ? StringUtil::UTF8StringToWxString(gameName) : Path::GetFilename(g_Conf->CurrentIso);
}

#else

#include "common/FileSystem.h"
#include "common/StringUtil.h"
#include "SaveState.h"
#include "Counters.h"
#include "SaveState.h"

#include "VMManager.h"

#include "DebugTools/Debug.h"
#include "GameDatabase.h"

#include "InputRecording.h"
#include "InputRecordingControls.h"
#include "Utilities/InputRecordingLogger.h"

#include <fmt/format.h>

void SaveStateBase::InputRecordingFreeze()
{
	// NOTE - BE CAREFUL
	// CHANGING THIS WILL BREAK BACKWARDS COMPATIBILITY ON SAVESTATES
	FreezeTag("InputRecording");
	Freeze(g_FrameCount);

	// Loading a save-state is an asynchronous task. If we are playing a recording
	// that starts from a savestate (not power-on) and the starting (pcsx2 internal) frame
	// marker has not been set (which comes from the save-state), we initialize it.
	if (g_InputRecording.IsInitialLoad())
		g_InputRecording.SetupInitialState(g_FrameCount);
	else if (g_InputRecording.IsActive() && IsLoading())
		g_InputRecording.SetFrameCounter(g_FrameCount);
}

InputRecording g_InputRecording;

InputRecording::InputRecordingPad::InputRecordingPad()
{
	padData = new PadData;
}

InputRecording::InputRecordingPad::~InputRecordingPad()
{
	delete padData;
}

void InputRecording::RecordingReset()
{
	// Booting is an asynchronous task. If we are playing a recording
	// that starts from power-on and the starting (pcsx2 internal) frame
	// marker has not been set, we initialize it.
	if (g_InputRecording.IsInitialLoad())
		g_InputRecording.SetupInitialState(0);
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
	else if (bufCount == 2)
	{
		if (buf[bufCount] != READ_DATA_AND_VIBRATE_SECOND_BYTE)
			fInterruptFrame = false;
	}
	else if (fInterruptFrame)
	{
		u8& bufVal = buf[bufCount];
		const u16 bufIndex = bufCount - 3;
		if (state == InputRecordingMode::Replaying)
		{
			if (frameCounter >= 0 && frameCounter < INT_MAX)
			{
				if (!inputRecordingData.ReadKeyBuffer(bufVal, frameCounter, port, bufIndex))
					inputRec::consoleLog(fmt::format("Failed to read input data at frame {}", frameCounter));

				// Update controller data state for future VirtualPad / logging usage.
				pads[port].padData->UpdateControllerData(bufIndex, bufVal);
			}
		}
		else
		{
			// Update controller data state for future VirtualPad / logging usage.
			pads[port].padData->UpdateControllerData(bufIndex, bufVal);

			// Commit the byte to the movie file if we are recording
			if (state == InputRecordingMode::Recording)
			{
				if (frameCounter >= 0)
				{
					if (incrementUndo)
					{
						inputRecordingData.IncrementUndoCount();
						incrementUndo = false;
					}

					if (frameCounter < INT_MAX && !inputRecordingData.WriteKeyBuffer(frameCounter, port, bufIndex, bufVal))
						inputRec::consoleLog(fmt::format("Failed to write input data at frame {}", frameCounter));
				}
			}
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
	if (frameCounter < INT_MAX)
	{
		frameCounter++;
		switch (state)
		{
			case InputRecordingMode::Recording:
				inputRecordingData.SetTotalFrames(frameCounter);
				[[fallthrough]];
			case InputRecordingMode::Replaying:
				if (frameCounter == inputRecordingData.GetTotalFrames())
					incrementUndo = false;
			default:
				break;
		}
	}
}

void InputRecording::LogAndRedraw()
{
	for (u8 port = 0; port < 2; port++)
	{
		pads[port].padData->LogPadData(port);
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

void InputRecording::SetToRecordMode()
{
	state = InputRecordingMode::Recording;
	inputRec::log("Record mode ON");
}

void InputRecording::SetToReplayMode()
{
	state = InputRecordingMode::Replaying;
	inputRec::log("Replay mode ON");
}

void InputRecording::SetFrameCounter(u32 newGFrameCount)
{
	if (newGFrameCount > startingFrame + (u32)inputRecordingData.GetTotalFrames())
	{
		inputRec::consoleLog("Warning, you've loaded PCSX2 emulation to a point after the end of the original recording. This should be avoided.");
		inputRec::consoleLog("Savestate's framecount has been ignored.");
		frameCounter = inputRecordingData.GetTotalFrames();
		if (state == InputRecordingMode::Replaying)
			SetToRecordMode();
		incrementUndo = false;
	}
	else
	{
		if (newGFrameCount < startingFrame)
		{
			inputRec::consoleLog("Warning, you've loaded PCSX2 emulation to a point before the start of the original recording. This should be avoided.");
			if (state == InputRecordingMode::Recording)
				SetToReplayMode();
		}
		else if (newGFrameCount == 0 && state == InputRecordingMode::Recording)
			SetToReplayMode();
		frameCounter = newGFrameCount - (s32)startingFrame;
		incrementUndo = true;
	}
}

void InputRecording::SetupInitialState(u32 newStartingFrame)
{
	startingFrame = newStartingFrame;
	if (state != InputRecordingMode::Replaying)
	{
		inputRec::log("Started new input recording");
		inputRec::consoleLog(fmt::format("Filename {}", inputRecordingData.GetFilename()));
		SetToRecordMode();
	}
	else
	{
		// Check if the current game matches with the one used to make the original recording
		// TODO - Vaser - this should be the CRC in hindsight anyway
		if (!VMManager::GetDiscPath().empty())
			if (resolveGameName() != inputRecordingData.GetHeader().gameName)
				inputRec::consoleLog("Input recording was possibly constructed for a different game.");

		incrementUndo = true;
		inputRec::log("Replaying input recording");
		inputRec::consoleMultiLog({fmt::format("File: {}", inputRecordingData.GetFilename()),
			fmt::format("PCSX2 Version Used: {}", std::string(inputRecordingData.GetHeader().emu)),
			fmt::format("Recording File Version: {}", inputRecordingData.GetHeader().version),
			fmt::format("Associated Game Name or ISO Filename: {}", std::string(inputRecordingData.GetHeader().gameName)),
			fmt::format("Author: {}", inputRecordingData.GetHeader().author),
			fmt::format("Total Frames: {}", inputRecordingData.GetTotalFrames()),
			fmt::format("Undo Count: {}", inputRecordingData.GetUndoCount())});
		SetToReplayMode();
	}

	if (inputRecordingData.FromSaveState())
		inputRec::consoleLog(fmt::format("Internal Starting Frame: {}", startingFrame));
	frameCounter = 0;
	initialLoad = false;
	g_InputRecordingControls.Lock(startingFrame);
}

void InputRecording::FailedSavestate()
{
	inputRec::consoleLog(fmt::format("{} is not compatible with this version of PCSX2", savestate));
	inputRec::consoleLog(fmt::format("Original PCSX2 version used: {}", inputRecordingData.GetHeader().emu));
	inputRecordingData.Close();
	initialLoad = false;
	state = InputRecordingMode::NotActive;
	g_InputRecordingControls.Resume();
}

void InputRecording::Stop()
{
	state = InputRecordingMode::NotActive;
	incrementUndo = false;
	if (inputRecordingData.Close())
		inputRec::log("Input recording stopped");
}

bool InputRecording::Create(const std::string_view& fileName, const bool fromSaveState, const std::string_view& authorName)
{
	if (!inputRecordingData.OpenNew(fileName, fromSaveState))
		return false;

	initialLoad = true;
	state = InputRecordingMode::Recording;
	if (fromSaveState)
	{
		savestate = fmt::format("{}_SaveState.p2s", fileName);
		if (FileSystem::FileExists(savestate.c_str()))
		{
			FileSystem::CopyFilePath(savestate.c_str(), fmt::format("{}.bak", savestate).c_str(), true);
		}
		VMManager::SaveState(savestate.c_str());
	}
	else
	{
		// Vaser - CHECK - don't need to specify a source anymore? (cdvd/etc?)
		VMManager::Execute();
	}

	// Set emulator version
	inputRecordingData.GetHeader().SetEmulatorVersion();

	// Set author name
	if (!authorName.empty())
		inputRecordingData.GetHeader().SetAuthor(authorName);

	// Set Game Name
	inputRecordingData.GetHeader().SetGameName(resolveGameName());
	// Write header contents
	inputRecordingData.WriteHeader();
	return true;
}

bool InputRecording::Play(const std::string_view& filename)
{
	if (!inputRecordingData.OpenExisting(filename))
		return false;

	// Either load the savestate, or restart the game
	if (inputRecordingData.FromSaveState())
	{
		// TODO - Vaser - VM State is atomic, be careful.
		if (VMManager::GetState() != VMState::Running && VMManager::GetState() != VMState::Paused)
		{
			inputRec::consoleLog("Game is not open, aborting playing input recording which starts on a save-state.");
			inputRecordingData.Close();
			return false;
		}

		savestate = fmt::format("{}_SaveState.p2s", inputRecordingData.GetFilename());
		if (!FileSystem::FileExists(savestate.c_str()))
		{
			inputRec::consoleLog(fmt::format("Could not locate savestate file at location - {}", savestate));
			inputRec::log("Savestate load failed");
			inputRecordingData.Close();
			return false;
		}
		state = InputRecordingMode::Replaying;
		initialLoad = true;
		VMManager::LoadState(savestate.c_str());
	}
	else
	{
		state = InputRecordingMode::Replaying;
		initialLoad = true;
		// Vaser - CHECK - don't need to specify a source anymore? (cdvd/etc?)
		VMManager::Execute();
	}
	return true;
}

std::string InputRecording::resolveGameName()
{
	std::string gameName;
	const std::string gameKey = SysGetDiscID();
	if (!gameKey.empty())
	{
		auto game = GameDatabase::findGame(gameKey);
		if (game)
		{
			gameName = game->name;
			gameName += " (" + game->region + ")";
		}
	}
	return !gameName.empty() ? gameName : VMManager::GetGameName();
}

#endif
