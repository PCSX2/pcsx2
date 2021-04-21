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
#include "Counters.h"

#ifndef DISABLE_RECORDING

#include "AppGameDatabase.h"
#include "DebugTools/Debug.h"

#include "InputRecording.h"
#include "InputRecordingControls.h"
#include "Utilities/StringUtils.h"
#include "Utilities/InputRecordingLogger.h"

#include "Recording/file/v1/InputRecordingFileV1.h"
#include "Recording/file/v2/InputRecordingFileV2.h"

#endif

void SaveStateBase::InputRecordingFreeze()
{
	// NOTE - BE CAREFUL
	// CHANGING THIS WILL BREAK BACKWARDS COMPATIBILITY ON SAVESTATES
	FreezeTag("InputRecording");
	Freeze(g_FrameCount);

#ifndef DISABLE_RECORDING
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
#endif
}

#ifndef DISABLE_RECORDING

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
				if (!inputRecordingData.readInputIntoBuffer(bufVal, frameCounter, port, bufIndex))
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
						inputRecordingData.incrementRedoCount();
						incrementUndo = false;
					}

					if (frameCounter < INT_MAX)
					{
						if (inputRecordingData.isMacro() && !inputRecordingData.writeDefaultMacroInputFromBuffer(frameCounter, port, bufIndex, bufVal))
						{
							inputRec::consoleLog(fmt::format("Failed to write macro input data at port {} and frame {}[{}]", port, frameCounter, bufIndex));
						}
						else if (!inputRecordingData.isMacro() && !inputRecordingData.writeInputFromBuffer(frameCounter, port, bufIndex, bufVal))
						{
							inputRec::consoleLog(fmt::format("Failed to write input data at port {} and frame {}[{}]", port, frameCounter, bufIndex));
						}
					}
				}
			}
			// If the VirtualPad updated the PadData, we have to update the buffer
			// before sending it to the game
			else if (pads[port].virtualPad->IsShown() && pads[port].virtualPad->UpdateControllerData(bufIndex, pads[port].padData))
				bufVal = pads[port].padData->PollControllerData(bufIndex);
		}
	}
}

s32 InputRecording::GetFrameCounter()
{
	return frameCounter;
}

InputRecordingFileV2& InputRecording::GetInputRecordingData()
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
				inputRecordingData.setFrameCounter(frameCounter);
				[[fallthrough]];
			case InputRecordingMode::Replaying:
				if (frameCounter == inputRecordingData.getTotalFrames())
					incrementUndo = false;
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
	if (newGFrameCount > startingFrame + (u32)inputRecordingData.getTotalFrames())
	{
		inputRec::consoleLog("Warning, you've loaded PCSX2 emulation to a point after the end of the original recording. This should be avoided.");
		inputRec::consoleLog("Savestate's framecount has been ignored.");
		frameCounter = inputRecordingData.getTotalFrames();
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
		inputRec::consoleLog(fmt::format("Filename {}", inputRecordingData.getFileName()));
		SetToRecordMode();
	}
	else
	{
		// Check if the current game matches with the one used to make the original recording
		if (!g_Conf->CurrentIso.IsEmpty())
			if (resolveGameName() != inputRecordingData.getGameName())
				inputRec::consoleLog("Input recording was possibly constructed for a different game.");

		incrementUndo = true;
		inputRec::log("Replaying input recording");
		inputRec::consoleMultiLog({fmt::format("File: {}", inputRecordingData.getFileName()),
								   fmt::format("PCSX2 Version Used: {}", inputRecordingData.getEmulatorVersion()),
								   fmt::format("Recording File Version: {}", inputRecordingData.getRecordingFileVersion()),
								   fmt::format("Associated Game Name or ISO Filename: {}", inputRecordingData.getGameName()),
								   fmt::format("Author: {}", inputRecordingData.getAuthor()),
								   fmt::format("Total Frames: {}", inputRecordingData.getTotalFrames()),
								   fmt::format("Redo Count: {}", inputRecordingData.getRedoCount())});
		SetToReplayMode();
	}

	if (inputRecordingData.isFromSavestate())
		inputRec::consoleLog(fmt::format("Internal Starting Frame: {}", startingFrame));
	frameCounter = 0;
	initialLoad = false;
	g_InputRecordingControls.Lock(startingFrame);
}

void InputRecording::FailedSavestate()
{
	inputRec::consoleLog(fmt::format("{} is not compatible with this version of PCSX2", savestate_path));
	inputRec::consoleLog(fmt::format("Original PCSX2 version used: {}", inputRecordingData.getEmulatorVersion()));
	inputRecordingData.closeFile();
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
	if (inputRecordingData.closeFile())
		inputRec::log("Input recording stopped");
}

bool InputRecording::Create(fs::path file_path, std::string author_name_utf8, InputRecordingFileV2::InputRecordingType recording_type)
{
	if (!inputRecordingData.createNewFile(file_path, author_name_utf8, resolveGameName(), recording_type))
		return false;

	initialLoad = true;
	state = InputRecordingMode::Recording;
	if (inputRecordingData.isFromSavestate())
	{
		savestate_path = FileUtils::appendToFilename(file_path, "_SaveState").replace_extension("p2s");
		FileUtils::backupFileIfExists(savestate_path);
		StateCopy_SaveToFile(FileUtils::wxStringFromPath(savestate_path));
	}
	else if (inputRecordingData.isFromBoot())
	{
		sApp.SysExecute(g_Conf->CdvdSource);
	}

	return true;
}

bool InputRecording::Play(wxWindow* parent, fs::path file_path)
{
	// Detect if file_path is a legacy file, and if so convert it
	if (file_path.extension() == ".p2m2")
	{
		inputRec::log("Legacy recording selected, converting it to the latest format");
		InputRecordingFileV1 legacy_file;
		legacy_file.OpenExisting(FileUtils::wxStringFromPath(file_path));
		bool success = inputRecordingData.convertFromV1(legacy_file, file_path);
		if (!success)
		{
			inputRec::log("Legacy recording conversion failed, aborting.");
		}
		file_path.replace_extension("pir");
		inputRec::log(fmt::format("Legacy recording conversion succeeded playing back converted file - {}", file_path));
	}
	else if (!inputRecordingData.openExistingFile(file_path))
	{
		return false;
	}

	if (inputRecordingData.isMacro())
	{
		inputRec::log("Macros are not intended for typical playback");
		return false;
	}

	// Either load the savestate, or restart the game
	if (inputRecordingData.isFromSavestate())
	{
		if (!CoreThread.IsOpen())
		{
			inputRec::consoleLog("Game is not open, aborting playing input recording which starts on a save-state.");
			inputRecordingData.closeFile();
			return false;
		}

		savestate_path = FileUtils::appendToFilename(file_path, "_SaveState").replace_extension("p2s");
		if (!fs::exists(savestate_path))
		{
			wxFileDialog loadStateDialog(parent, _("Select the savestate that will accompany this recording"), "", "",
										 "Savestate files (*.p2s)|*.p2s", wxFD_OPEN);
			if (loadStateDialog.ShowModal() == wxID_CANCEL)
			{
				inputRec::consoleLog(fmt::format("Could not locate savestate file at location - {}", savestate_path));
				inputRec::log("Savestate load failed");
				inputRecordingData.closeFile();
				return false;
			}

			savestate_path = FileUtils::wxStringToPath(loadStateDialog.GetPath());
			inputRec::consoleLog(fmt::format("Base savestate set to {}", savestate_path));
		}
		state = InputRecordingMode::Replaying;
		initialLoad = true;
		StateCopy_LoadFromFile(FileUtils::wxStringFromPath(savestate_path));
	}
	else if (inputRecordingData.isFromBoot())
	{
		state = InputRecordingMode::Replaying;
		initialLoad = true;
		sApp.SysExecute(g_Conf->CdvdSource);
	}
	return true;
}

void InputRecording::GoToFirstFrame(wxWindow* parent)
{
	if (inputRecordingData.isFromSavestate())
	{
		// TODO - possibly refactor opportunity, shares a lot of code with the above function
		if (!fs::exists(savestate_path))
		{
			const bool initiallyPaused = g_InputRecordingControls.IsPaused();

			if (!initiallyPaused)
				g_InputRecordingControls.PauseImmediately();

			inputRec::consoleLog(fmt::format("Could not locate savestate file at location - {}\n", savestate_path));
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
			savestate_path = FileUtils::wxStringToPath(loadStateDialog.GetPath());
			inputRec::consoleLog(fmt::format("Base savestate swapped to {}", savestate_path));
		}
		StateCopy_LoadFromFile(FileUtils::wxStringFromPath(savestate_path));
	}
	else if (inputRecordingData.isFromBoot())
	{
		sApp.SysExecute(g_Conf->CdvdSource);
	}

	if (IsRecording())
		SetToReplayMode();
}

std::string InputRecording::resolveGameName()
{
	const wxString gameKey(SysGetDiscID());
	if (!gameKey.IsEmpty())
	{
		if (IGameDatabase* gameDB = AppHost_GetGameDatabase())
		{
			GameDatabaseSchema::GameEntry game = gameDB->findGame(std::string(gameKey));
			if (game.isValid)
			{
				return fmt::format("{} ({})", game.name, game.region);
			}
		}
	}
	return Path::GetFilename(g_Conf->CurrentIso).ToStdString();
}

#endif
