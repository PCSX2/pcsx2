// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <queue>

// TODO:
// - configure frame advance amount

class InputRecordingControls
{
public:
	enum class Mode
	{
		Recording,
		Replaying,
	};

	void toggleRecordMode();
	void setRecordMode(bool waitForFrameToEnd = true);
	void setReplayMode(bool waitForFrameToEnd = true);

	bool isRecording() const;
	bool isReplaying() const;

	void processControlQueue();

private:
	Mode m_state = Mode::Replaying;
	std::queue<std::function<void()>> m_controlQueue;
};

