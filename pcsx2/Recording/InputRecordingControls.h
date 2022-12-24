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

