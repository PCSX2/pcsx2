// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <functional>
#include <queue>

#include "Recording/InputRecordingFile.h"
#include "Recording/InputRecordingControls.h"

class InputRecording
{
public:
	enum class Type
	{
		POWER_ON,
		FROM_SAVESTATE
	};

	bool create(const std::string& filename, const bool fromSaveState, const std::string& authorName);
	bool play(const std::string& path);
	void stop();

	void handleControllerDataUpdate();
	void saveControllerData(const PadData& data, const int port, const int slot);
	std::optional<PadData> updateControllerData(const int port, const int slot);
	void incFrameCounter();
	u64 getFrameCounter() const;
	bool isActive() const;
	void processRecordQueue();

	void handleExceededFrameCounter();
	void handleReset();
	void handleLoadingSavestate();
	bool isTypeSavestate() const;
	void adjustFrameCounterOnReRecord(u32 newFrameCounter);

	InputRecordingControls& getControls();
	const InputRecordingFile& getData() const;

private:
	InputRecordingControls m_controls;
	InputRecordingFile m_file;

	Type m_type;

	bool m_initial_load_complete = false;
	bool m_is_active = false;
	bool m_watching_for_rerecords = false;

	// A consistent way to run actions at the end of the each frame (ie. stop the recording)
	std::queue<std::function<void()>> m_recordingQueue;

	u32 m_frame_counter = 0;
	// Either 0 for a power-on movie, or the g_FrameCount that is stored on the starting frame
	u32 m_starting_frame = 0;

	void initializeState();
	void setStartingFrame(u32 startingFrame);
	void closeActiveFile();
};

extern InputRecording g_InputRecording;
