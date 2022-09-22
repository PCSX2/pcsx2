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

void InputRecording::ControllerInterrupt(u8 port, size_t fifoSize, u8 dataIn, u8 dataOut)
{
	// TODO - Multi-Tap Support

	if (fifoSize == 1)
		fInterruptFrame = dataIn == READ_DATA_AND_VIBRATE_FIRST_BYTE;
	else if (fifoSize == 2)
	{
		if (dataOut != READ_DATA_AND_VIBRATE_SECOND_BYTE)
			fInterruptFrame = false;
	}
	else if (fInterruptFrame)
	{
		u8& bufVal = dataOut;
		const u16 bufIndex = fifoSize - 3;
		if (state == InputRecordingMode::Replaying)
		{
			if (frameCounter >= 0 && frameCounter < INT_MAX)
			{
				if (!inputRecordingData.ReadKeyBuffer(bufVal, frameCounter, port, bufIndex))
					InputRec::consoleLog(fmt::format("Failed to read input data at frame {}", frameCounter));

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
						InputRec::consoleLog(fmt::format("Failed to write input data at frame {}", frameCounter));
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
	InputRec::log("Record mode ON");
}

void InputRecording::SetToReplayMode()
{
	state = InputRecordingMode::Replaying;
	pads[CONTROLLER_PORT_ONE].virtualPad->SetReadOnlyMode(true);
	pads[CONTROLLER_PORT_TWO].virtualPad->SetReadOnlyMode(true);
	InputRec::log("Replay mode ON");
}

void InputRecording::SetFrameCounter(u32 newGFrameCount)
{
	if (newGFrameCount > startingFrame + (u32)inputRecordingData.GetTotalFrames())
	{
		InputRec::consoleLog("Warning, you've loaded PCSX2 emulation to a point after the end of the original recording. This should be avoided.");
		InputRec::consoleLog("Savestate's framecount has been ignored.");
		frameCounter = inputRecordingData.GetTotalFrames();
		if (state == InputRecordingMode::Replaying)
			SetToRecordMode();
		incrementUndo = false;
	}
	else
	{
		if (newGFrameCount < startingFrame)
		{
			InputRec::consoleLog("Warning, you've loaded PCSX2 emulation to a point before the start of the original recording. This should be avoided.");
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
		InputRec::log("Started new input recording");
		InputRec::consoleLog(fmt::format("Filename {}", inputRecordingData.GetFilename().ToUTF8()));
		SetToRecordMode();
	}
	else
	{
		// Check if the current game matches with the one used to make the original recording
		if (!g_Conf->CurrentIso.IsEmpty())
			if (resolveGameName() != inputRecordingData.GetHeader().gameName)
				InputRec::consoleLog("Input recording was possibly constructed for a different game.");

		incrementUndo = true;
		InputRec::log("Replaying input recording");
		InputRec::consoleMultiLog({fmt::format("File: {}", inputRecordingData.GetFilename().ToUTF8()),
			fmt::format("PCSX2 Version Used: {}", std::string(inputRecordingData.GetHeader().emu)),
			fmt::format("Recording File Version: {}", inputRecordingData.GetHeader().version),
			fmt::format("Associated Game Name or ISO Filename: {}", std::string(inputRecordingData.GetHeader().gameName)),
			fmt::format("Author: {}", inputRecordingData.GetHeader().author),
			fmt::format("Total Frames: {}", inputRecordingData.GetTotalFrames()),
			fmt::format("Undo Count: {}", inputRecordingData.GetUndoCount())});
		SetToReplayMode();
	}

	if (inputRecordingData.FromSaveState())
		InputRec::consoleLog(fmt::format("Internal Starting Frame: {}", startingFrame));
	frameCounter = 0;
	initialLoad = false;
	g_InputRecordingControls.Lock(startingFrame);
}

void InputRecording::FailedSavestate()
{
	InputRec::consoleLog(fmt::format("{} is not compatible with this version of PCSX2", savestate.ToUTF8()));
	InputRec::consoleLog(fmt::format("Original PCSX2 version used: {}", inputRecordingData.GetHeader().emu));
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
		InputRec::log("Input recording stopped");
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
			InputRec::consoleLog("Game is not open, aborting playing input recording which starts on a save-state.");
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
				InputRec::consoleLog(fmt::format("Could not locate savestate file at location - {}", savestate.ToUTF8()));
				InputRec::log("Savestate load failed");
				inputRecordingData.Close();
				return false;
			}

			savestate = loadStateDialog.GetPath();
			InputRec::consoleLog(fmt::format("Base savestate set to {}", savestate.ToUTF8()));
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

			InputRec::consoleLog(fmt::format("Could not locate savestate file at location - {}\n", savestate.ToUTF8()));
			wxFileDialog loadStateDialog(parent, _("Select a savestate to accompany the recording with"), L"", L"",
				L"Savestate files (*.p2s)|*.p2s", wxFD_OPEN);
			int result = loadStateDialog.ShowModal();
			if (!initiallyPaused)
				g_InputRecordingControls.Resume();

			if (result == wxID_CANCEL)
			{
				InputRec::log("Savestate load cancelled");
				return;
			}
			savestate = loadStateDialog.GetPath();
			InputRec::consoleLog(fmt::format("Base savestate swapped to {}", savestate.ToUTF8()));
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

#include "InputRecording.h"

#include "InputRecordingControls.h"
#include "Utilities/InputRecordingLogger.h"

#include "common/FileSystem.h"
#include "common/StringUtil.h"
#include "SaveState.h"
#include "Counters.h"
#include "SaveState.h"
#include "VMManager.h"
#include "DebugTools/Debug.h"
#include "GameDatabase.h"
#include "fmt/format.h"

// Future TODOs
// - restart
// - tooltips on GUI options
// - Controller Logging (virtual pad related)
// - persist last browsed IR path
// - logs are weirdly formatted
// - force OSD updates since a lot of input recording occurs during a paused state
// - differentiating OSD logs somehow would be nice (color / a preceding icon?)

#include <queue>
#include <fmt/format.h>

void SaveStateBase::InputRecordingFreeze()
{
	// NOTE - BE CAREFUL
	// CHANGING THIS WILL BREAK BACKWARDS COMPATIBILITY ON SAVESTATES
	FreezeTag("InputRecording");
	Freeze(g_FrameCount);
}

InputRecording g_InputRecording;

bool InputRecording::create(const std::string& fileName, const bool fromSaveState, const std::string& authorName)
{
	if (!m_file.OpenNew(fileName, fromSaveState))
	{
		return false;
	}

	m_controls.setRecordMode();
	if (fromSaveState)
	{
		std::string savestatePath = fmt::format("{}_SaveState.p2s", fileName);
		if (FileSystem::FileExists(savestatePath.c_str()))
		{
			FileSystem::CopyFilePath(savestatePath.c_str(), fmt::format("{}.bak", savestatePath).c_str(), true);
		}
		m_initial_savestate_load_complete = false;
		m_type = Type::FROM_SAVESTATE;
		m_is_active = true;
		// TODO - error handling
		VMManager::SaveState(savestatePath.c_str());
	}
	else
	{
		m_starting_frame = 0;
		m_type = Type::POWER_ON;
		m_is_active = true;
		// TODO - should this be an explicit [full] boot instead of a reset?
		VMManager::Reset();
	}

	m_file.getHeader().SetEmulatorVersion();
	m_file.getHeader().SetAuthor(authorName);
	m_file.getHeader().SetGameName(resolveGameName());
	m_file.WriteHeader();
	initializeState();
	InputRec::log("Started new input recording");
	InputRec::consoleLog(fmt::format("Filename {}", m_file.getFilename()));
	return true;
}

bool InputRecording::play(const std::string& filename)
{
	if (!m_file.OpenExisting(filename))
	{
		return false;
	}

	// Either load the savestate, or restart the game
	if (m_file.FromSaveState())
	{
		std::string savestatePath = fmt::format("{}_SaveState.p2s", m_file.getFilename());
		if (!FileSystem::FileExists(savestatePath.c_str()))
		{
			InputRec::consoleLog(fmt::format("Could not locate savestate file at location - {}", savestatePath));
			InputRec::log("Savestate load failed");
			m_file.Close();
			return false;
		}
		m_type = Type::FROM_SAVESTATE;
		m_initial_savestate_load_complete = false;
		m_is_active = true;
		const auto loaded = VMManager::LoadState(savestatePath.c_str());
		if (!loaded)
		{
			InputRec::log("Savestate load failed, unsupported version?");
			m_file.Close();
			m_is_active = false;
			return false;
		}
	}
	else
	{
		m_starting_frame = 0;
		m_type = Type::POWER_ON;
		m_is_active = true;
		// TODO - should this be an explicit [full] boot instead of a reset?
		VMManager::Reset();
	}
	m_controls.setReplayMode();
	initializeState();
	InputRec::log("Replaying input recording");
	m_file.logRecordingMetadata();
	if (resolveGameName() != m_file.getHeader().m_gameName)
	{
		InputRec::consoleLog(fmt::format("Input recording was possibly constructed for a different game. Expected: {}, Actual: {}", m_file.getHeader().m_gameName, resolveGameName()));
	}

	return true;
}

void InputRecording::stop()
{
	m_is_active = false;
	if (m_file.Close())
	{
		InputRec::log("Input recording stopped");
	}
}

// TODO: Refactor this
void InputRecording::ControllerInterrupt(u8 port, size_t fifoSize, u8 dataIn, u8 dataOut)
{
	// TODO - Multi-Tap Support
	if (fifoSize == 1)
		fInterruptFrame = dataIn == READ_DATA_AND_VIBRATE_FIRST_BYTE;
	else if (fifoSize == 2)
	{
		if (dataOut != READ_DATA_AND_VIBRATE_SECOND_BYTE)
			fInterruptFrame = false;
	}

	// If there is data to read (previous two bytes looked correct)
	if (bufCount >= 3 && m_pad_data_available)
	{
		u8& bufVal = dataOut;
		const u16 bufIndex = fifoSize - 3;
		if (state == InputRecordingMode::Replaying)
		{
			if (!m_file.ReadKeyBuffer(bufVal, m_frame_counter, port, bufIndex))
			{
				InputRec::consoleLog(fmt::format("Failed to read input data at frame {}", m_frame_counter));
			}
			// Update controller data state for future VirtualPad / logging usage.
			//pads[port].padData->UpdateControllerData(bufIndex, bufVal);
		}
		else
		{
			// Update controller data state for future VirtualPad / logging usage.
			//pads[port].padData->UpdateControllerData(bufIndex, bufVal);

			// Commit the byte to the movie file if we are recording
			if (m_controls.isRecording())
			{
				if (!m_file.WriteKeyBuffer(m_frame_counter, port, bufIndex, bufVal))
				{
					InputRec::consoleLog(fmt::format("Failed to write input data at frame {}", m_frame_counter));
				}
			}
		}
	}
	if (bufCount > 20)
	{
		m_pad_data_available = false;
	}
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
			gameName = fmt::format("{} ({})", game->name, game->region);
		}
	}
	return !gameName.empty() ? gameName : VMManager::GetGameName();
}

void InputRecording::incFrameCounter()
{
	if (m_frame_counter >= std::numeric_limits<u64>::max())
	{
		// TODO - log the incredible achievment of playing for longer than 4 billion years, and end the recording
		stop();
		return;
	}
	m_frame_counter++;

	if (m_controls.isReplaying())
	{
		// If we've reached the end of the recording while replaying, pause
		if (m_frame_counter == m_file.getTotalFrames() - 1)
		{
			VMManager::SetPaused(true);
			// Can also stop watching for re-records, they've watched to the end of the recording
			m_watching_for_rerecords = false;
		}
	}
	if (m_controls.isRecording())
	{
		m_file.SetTotalFrames(m_frame_counter);
		// If we've been in record mode and moved to the next frame, we've overrote something
		// if this was following a save-state loading, this is considered a re-record, a.k.a an undo
		if (m_watching_for_rerecords)
		{
			m_file.IncrementUndoCount();
			m_watching_for_rerecords = false;
		}
	}
}

u64 InputRecording::getFrameCounter() const
{
	return m_frame_counter;
}

bool InputRecording::isActive() const
{
	return m_is_active;
}

void InputRecording::handleExceededFrameCounter()
{
	// if we go past the end, switch to recording mode so nothing is lost
	if (m_frame_counter >= m_file.getTotalFrames() && m_controls.isReplaying())
	{
		m_controls.setRecordMode(false);
	}
}

void InputRecording::handleLoadingSavestate()
{
	// We need to keep track of the starting internal frame of the recording
	// - For a power-on recording this should already be done - it starts at 0
	// - For save state recordings, this is stored inside the initial save-state
	//
	// Why?
	// - When you re-record you load another save-state which has it's own frame counter
	//   stored within, we use this to adjust the frame we are replaying/recording to
	if (isTypeSavestate() && !m_initial_savestate_load_complete)
	{
		setStartingFrame(g_FrameCount);
		m_initial_savestate_load_complete = true;
	}
	else
	{
		adjustFrameCounterOnReRecord(g_FrameCount);
		m_watching_for_rerecords = true;
	}
}

bool InputRecording::isTypeSavestate() const
{
	return m_type == Type::FROM_SAVESTATE;
}

void InputRecording::setStartingFrame(u64 startingFrame)
{
	if (m_type == Type::POWER_ON)
	{
		return;
	}
	InputRec::consoleLog(fmt::format("Internal Starting Frame: {}", startingFrame));
	m_starting_frame = startingFrame;
}

void InputRecording::adjustFrameCounterOnReRecord(u64 newFrameCounter)
{
	if (newFrameCounter > m_starting_frame + (u64)m_file.getTotalFrames())
	{
		InputRec::consoleLog("Warning, you've loaded PCSX2 emulation to a point after the end of the original recording. This should be avoided.");
		InputRec::consoleLog("Savestate's framecount has been ignored, using the max length of the recording instead.");
		m_frame_counter = m_file.getTotalFrames();
		if (getControls().isReplaying())
		{
			getControls().setRecordMode();
		}
		return;
	}
	if (newFrameCounter < m_starting_frame)
	{
		InputRec::consoleLog("Warning, you've loaded PCSX2 emulation to a point before the start of the original recording. This should be avoided.");
		InputRec::consoleLog("Savestate's framecount has been ignored, starting from the beginning in replay mode.");
		m_frame_counter = 0;
		if (getControls().isRecording())
		{
			getControls().setReplayMode();
		}
		return;
	}
	else if (newFrameCounter == 0 && getControls().isRecording())
	{
		getControls().setReplayMode();
	}
	m_frame_counter = newFrameCounter - m_starting_frame;
}

InputRecordingControls& InputRecording::getControls()
{
	return m_controls;
}

const InputRecordingFile& InputRecording::getData() const
{
	return m_file;
}

void InputRecording::initializeState()
{
	m_frame_counter = 0;
	m_watching_for_rerecords = false;
}

#endif
