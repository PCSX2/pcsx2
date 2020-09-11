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
#include "Utilities/InputRecordingLogger.h"

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
			g_InputRecording.SetStartingFrame(g_FrameCount);
		else if (g_InputRecording.IsActive())
		{
			// Explicitly set the frame change tracking variable as to not
			// detect saving or loading a savestate as a frame being drawn
			g_InputRecordingControls.SetFrameCountTracker(g_FrameCount);

			if (IsLoading())
				g_InputRecording.SetFrameCounter(g_FrameCount);
		}
	}
#endif
}

#ifndef DISABLE_RECORDING

InputRecording g_InputRecording;

InputRecording::~InputRecording()
{
	// As to not attempt a double delete from WX
	newRecordingFrame.release();
	openFileDialog.release();
}

InputRecording::InputRecordingPad::InputRecordingPad()
{
	padData = std::make_unique<PadData>();
}

InputRecording::InputRecordingPad::~InputRecordingPad()
{
	// As to not attempt a double delete from WX
	virtualPad.release();
}

void InputRecording::InitInputRecordingWindows(wxWindow* parent)
{
	if (newRecordingFrame.get() != nullptr)
		return;
	newRecordingFrame = std::make_unique<NewRecordingFrame>(parent);
	openFileDialog = std::make_unique<wxFileDialog>(parent, _("Select P2M2 record file."), L"", L"",
								L"p2m2 file(*.p2m2)|*.p2m2", wxFD_OPEN);
	pads[CONTROLLER_PORT_ONE].virtualPad = std::make_unique<VirtualPad>(parent, 0, g_Conf->inputRecording);
	pads[CONTROLLER_PORT_TWO].virtualPad = std::make_unique<VirtualPad>(parent, 1, g_Conf->inputRecording);
}


void InputRecording::ShowVirtualPad(int const port)
{
	pads[port].virtualPad->Show();
}

void InputRecording::OnBoot()
{
	// Booting is an asynchronous task. If we are playing a recording
	// that starts from power-on and the starting (pcsx2 internal) frame
	// marker has not been set, we initialize it.
	if (initialLoad)
		SetStartingFrame(0);
	else if (IsActive())
	{
		SetFrameCounter(0);
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
	else if (fInterruptFrame && frameCounter >= 0 && frameCounter < INT_MAX)
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
		pads[port].padData->UpdateControllerData(bufIndex, bufVal);

		if (pads[port].virtualPad->IsShown() &&
			pads[port].virtualPad->UpdateControllerData(bufIndex, pads[port].padData.get()) &&
			state != InputRecordingMode::Replaying)
		{
			// If the VirtualPad updated the PadData, we have to update the buffer
			// before committing it to the recording / sending it to the game
			// - Do not do this if we are in replay mode!
			bufVal = pads[port].padData->PollControllerData(bufIndex);
		}

		// If we have reached the end of the pad data, log it out
		if (bufIndex == PadData::END_INDEX_CONTROLLER_BUFFER)
		{
			pads[port].padData->LogPadData(port);
			// As well as re-render the virtual pad UI, if applicable
			// - Don't render if it's minimized
			if (pads[port].virtualPad->IsShown() && !pads[port].virtualPad->IsIconized())
				pads[port].virtualPad->Redraw();
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
		if (inputRecordingData.SetTotalFrames(frameCounter)) // Don't increment if we're at the last frame
			incrementUndo = false;
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
	const u32 endFrame = startingFrame + inputRecordingData.GetTotalFrames();
	if (newGFrameCount >= endFrame)
	{
		if (newGFrameCount > endFrame)
		{
			inputRec::consoleLog("Warning, you've loaded PCSX2 emulation to a point after the end of the original recording. This should be avoided.");
			inputRec::consoleLog("Savestate's framecount has been ignored.");
		}
		if (state == InputRecordingMode::Replaying)
			SetToRecordMode();
		frameCounter = inputRecordingData.GetTotalFrames();
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
		frameCounter = static_cast<s32>(newGFrameCount - startingFrame);
		incrementUndo = true;
	}
}

void InputRecording::SetStartingFrame(u32 newStartingFrame)
{
	startingFrame = newStartingFrame;
	if (inputRecordingData.FromSaveState())
		inputRec::consoleLog(fmt::format("Internal Starting Frame: {}", startingFrame));
	frameCounter = 0;
	initialLoad = false;
	g_InputRecordingControls.Lock(startingFrame);
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

bool InputRecording::Create()
{
	if (newRecordingFrame->ShowModal(CoreThread.IsOpen()) == wxID_CANCEL)
		return false;

	if (!inputRecordingData.OpenNew(newRecordingFrame->GetFile(), newRecordingFrame->GetFrom()))
		return false;

	initialLoad = true;
	if (inputRecordingData.FromSaveState())
	{
		if (wxFileExists(inputRecordingData.GetFilename() + "_SaveState.p2s"))
			wxCopyFile(inputRecordingData.GetFilename() + "_SaveState.p2s", inputRecordingData.GetFilename() + "_SaveState.p2s.bak", true);
		StateCopy_SaveToFile(inputRecordingData.GetFilename() + "_SaveState.p2s");
	}
	else
		sApp.SysExecute(g_Conf->CdvdSource);

	// Set emulator version
	inputRecordingData.GetHeader().SetEmulatorVersion();

	// Set author name
	{
		const wxString authorName = newRecordingFrame->GetAuthor();
		if (!authorName.IsEmpty())
			inputRecordingData.GetHeader().SetAuthor(authorName);
	}

	// Set Game Name
	inputRecordingData.GetHeader().SetGameName(resolveGameName());
	// Write header contents
	inputRecordingData.WriteHeader();
	SetToRecordMode();
	g_InputRecordingControls.DisableFrameAdvance();
	inputRec::log("Started new input recording");
	inputRec::consoleLog(fmt::format("Filename {}", std::string(inputRecordingData.GetFilename())));
	return true;
}

int InputRecording::Play()
{
	if (openFileDialog->ShowModal() == wxID_CANCEL)
		return 0;

	if (IsActive())
		Stop();

	if (!inputRecordingData.OpenExisting(openFileDialog->GetPath()))
		return 1;

	// Either load the savestate, or restart the game
	if (inputRecordingData.FromSaveState())
	{
		if (!CoreThread.IsOpen())
		{
			inputRec::consoleLog("Game is not open, aborting playing input recording which starts on a save-state.");
			inputRecordingData.Close();
			return 1;
		}
		if (!wxFileExists(inputRecordingData.GetFilename() + "_SaveState.p2s"))
		{
			inputRec::consoleLog(fmt::format("Could not locate savestate file at location - {}_SaveState.p2s",
											 inputRecordingData.GetFilename()));
			inputRecordingData.Close();
			return 1;
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
			inputRec::consoleLog("Input recording was possibly constructed for a different game.");

	incrementUndo = true;
	SetToReplayMode();
	inputRec::log("Playing input recording");
	g_InputRecordingControls.DisableFrameAdvance();
	inputRec::consoleMultiLog({fmt::format("Replaying input recording - [{}]", std::string(inputRecordingData.GetFilename())),
							   fmt::format("PCSX2 Version Used: {}", std::string(inputRecordingData.GetHeader().emu)),
							   fmt::format("Recording File Version: {}", inputRecordingData.GetHeader().version),
							   fmt::format("Associated Game Name or ISO Filename: {}", std::string(inputRecordingData.GetHeader().gameName)),
							   fmt::format("Author: {}", inputRecordingData.GetHeader().author),
							   fmt::format("Total Frames: {}", inputRecordingData.GetTotalFrames()),
							   fmt::format("Undo Count: {}", inputRecordingData.GetUndoCount())});
	return 2;
}

bool InputRecording::GoToFirstFrame()
{
	if (inputRecordingData.FromSaveState())
	{
		if (!wxFileExists(inputRecordingData.GetFilename() + "_SaveState.p2s"))
		{
			recordingConLog(wxString::Format("[REC]: Could not locate savestate file at location - %s_SaveState.p2s\n",
												inputRecordingData.GetFilename()));
			Stop();
			return false;
		}
		StateCopy_LoadFromFile(inputRecordingData.GetFilename() + "_SaveState.p2s");
	}
	else
		sApp.SysExecute(g_Conf->CdvdSource);

	if (IsRecording())
		SetToReplayMode();
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
