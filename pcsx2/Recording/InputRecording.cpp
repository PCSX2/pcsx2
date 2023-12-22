// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Counters.h"
#include "MTGS.h"
#include "SaveState.h"

bool SaveStateBase::InputRecordingFreeze()
{
	// NOTE - BE CAREFUL
	// CHANGING THIS WILL BREAK BACKWARDS COMPATIBILITY ON SAVESTATES
	if (!FreezeTag("InputRecording"))
		return false;

	Freeze(g_FrameCount);
	return IsOkay();
}

#include "InputRecording.h"

#include "InputRecordingControls.h"
#include "Utilities/InputRecordingLogger.h"

#include "common/FileSystem.h"
#include "common/StringUtil.h"
#include "Counters.h"
#include "SaveState.h"
#include "VMManager.h"
#include "DebugTools/Debug.h"
#include "GameDatabase.h"
#include "fmt/format.h"
#include "GS.h"

InputRecording g_InputRecording;

bool InputRecording::create(const std::string& fileName, const bool fromSaveState, const std::string& authorName)
{
	if (!m_file.openNew(fileName, fromSaveState))
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
		m_type = Type::FROM_SAVESTATE;
		m_is_active = true;
		m_initial_load_complete = true;
		m_watching_for_rerecords = true;
		setStartingFrame(g_FrameCount);
		// TODO - error handling
		VMManager::SaveState(savestatePath.c_str());
	}
	else
	{
		m_starting_frame = 0;
		m_type = Type::POWER_ON;
		m_initial_load_complete = false;
		m_is_active = true;
		// TODO - should this be an explicit [full] boot instead of a reset?
		VMManager::Reset();
	}

	m_file.setEmulatorVersion();
	m_file.setAuthor(authorName);
	m_file.setGameName(VMManager::GetTitle(false));
	m_file.writeHeader();
	initializeState();
	InputRec::log("Started new input recording");
	InputRec::consoleLog(fmt::format("Filename {}", m_file.getFilename()));
	return true;
}

bool InputRecording::play(const std::string& filename)
{
	if (!m_file.openExisting(filename))
	{
		return false;
	}

	// Either load the savestate, or restart the game
	if (m_file.fromSaveState())
	{
		std::string savestatePath = fmt::format("{}_SaveState.p2s", m_file.getFilename());
		if (!FileSystem::FileExists(savestatePath.c_str()))
		{
			InputRec::consoleLog(fmt::format("Could not locate savestate file at location - {}", savestatePath));
			InputRec::log("Savestate load failed");
			m_file.close();
			return false;
		}
		m_type = Type::FROM_SAVESTATE;
		m_initial_load_complete = false;
		m_is_active = true;
		const auto loaded = VMManager::LoadState(savestatePath.c_str());
		if (!loaded)
		{
			InputRec::log("Savestate load failed, unsupported version?");
			m_file.close();
			m_is_active = false;
			return false;
		}
	}
	else
	{
		m_starting_frame = 0;
		m_type = Type::POWER_ON;
		m_initial_load_complete = false;
		m_is_active = true;
		// TODO - should this be an explicit [full] boot instead of a reset?
		VMManager::Reset();
	}
	m_controls.setReplayMode();
	initializeState();
	InputRec::log("Replaying input recording");
	m_file.logRecordingMetadata();
	if (VMManager::GetTitle(false) != m_file.getGameName())
	{
		InputRec::consoleLog(fmt::format("Input recording was possibly constructed for a different game. Expected: {}, Actual: {}", m_file.getGameName(), VMManager::GetTitle(false)));
	}
	return true;
}

void InputRecording::closeActiveFile()
{
	if (!m_is_active)
	{
		return;
	}
	if (m_file.close())
	{
		m_is_active = false;
		InputRec::log("Input recording stopped");
		MTGS::PresentCurrentFrame();
	}
	else
	{
		InputRec::log("Unable to stop input recording");
	}
}

void InputRecording::stop()
{
	if (VMManager::GetState() == VMState::Paused)
	{
		closeActiveFile();
	}
	else
	{
		// Don't stop immediately, close the file after the current frame completes
		m_recordingQueue.push([&]() {
			closeActiveFile();
		});
	}
}

void InputRecording::handleControllerDataUpdate()
{
	// TODO - multi-tap support with new file format, for now just controller 0 and 1
	for (int i = 0; i < 2; i++)
	{
		// Fetch the current frame's data
		PadData frameData(i, 0);
		if (m_is_active)
		{
			if (m_controls.isRecording())
			{
				saveControllerData(frameData, i, 0);
			}
			else if (m_controls.isReplaying())
			{
				const auto& modifiedFrameData = updateControllerData(i, 0);
				if (modifiedFrameData)
				{
					frameData = modifiedFrameData.value();
				}
			}
		}
		// Log the data we have gathered, useful for debugging our use-case
		frameData.LogPadData();
	}
}

void InputRecording::saveControllerData(const PadData& data, const int port, const int slot)
{
	// Save the frame's data to the file
	if (!m_file.writePadData(m_frame_counter, data))
	{
		InputRec::consoleLog(fmt::format("Failed to write input data at [{}:{}:{}]", m_frame_counter, port, slot));
	}
}
std::optional<PadData> InputRecording::updateControllerData(const int port, const int slot)
{
	// Get the PadData from the file
	const auto frameData = m_file.readPadData(m_frame_counter, port, slot);
	if (frameData)
	{
		// Update the g_key_status appropriately
		frameData->OverrideActualController();
	}
	else
	{
		InputRec::consoleLog(fmt::format("Failed to read input data at [{}:{}:{}]", m_frame_counter, port, slot));
	}
	return frameData;
}

void InputRecording::processRecordQueue()
{
	while (!m_recordingQueue.empty())
	{
		m_recordingQueue.front()();
		m_recordingQueue.pop();
	}
}

void InputRecording::incFrameCounter()
{
	if (!m_is_active)
	{
		return;
	}

	if (m_frame_counter == std::numeric_limits<u32>::max())
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
		m_file.setTotalFrames(m_frame_counter);
		// If we've been in record mode and moved to the next frame, we've overrote something
		// if this was following a save-state loading, this is considered a re-record, a.k.a an undo
		if (m_watching_for_rerecords)
		{
			m_file.incrementUndoCount();
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

void InputRecording::handleReset()
{
	if (m_initial_load_complete)
	{
		adjustFrameCounterOnReRecord(0);
	}
	m_initial_load_complete = true;
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
	if (isTypeSavestate() && !m_initial_load_complete)
	{
		setStartingFrame(g_FrameCount);
		m_initial_load_complete = true;
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

void InputRecording::setStartingFrame(u32 startingFrame)
{
	if (m_type == Type::POWER_ON)
	{
		return;
	}
	InputRec::consoleLog(fmt::format("Internal Starting Frame: {}", startingFrame));
	m_starting_frame = startingFrame;
}

void InputRecording::adjustFrameCounterOnReRecord(u32 newFrameCounter)
{
	if (newFrameCounter > m_starting_frame + m_file.getTotalFrames())
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
