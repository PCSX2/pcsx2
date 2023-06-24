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

#include "DebugTools/Debug.h"
#include "MemoryTypes.h"

#include "InputRecording.h"
#include "InputRecordingControls.h"
#include "Utilities/InputRecordingLogger.h"

#include "MTGS.h"
#include "VMManager.h"

void InputRecordingControls::toggleRecordMode()
{
	if (isReplaying())
	{
		setRecordMode();
	}
	else
	{
		setReplayMode();
	}
}

void InputRecordingControls::setRecordMode(bool waitForFrameToEnd)
{
	if (!waitForFrameToEnd || VMManager::GetState() == VMState::Paused)
	{
		m_state = Mode::Recording;
		InputRec::log("Record mode ON");
		MTGS::PresentCurrentFrame();
	}
	else
	{
		m_controlQueue.push([&]() {
			m_state = Mode::Recording;
			InputRec::log("Record mode ON");
		});
	}
}

void InputRecordingControls::setReplayMode(bool waitForFrameToEnd)
{
	if (!waitForFrameToEnd || VMManager::GetState() == VMState::Paused)
	{
		m_state = Mode::Replaying;
		InputRec::log("Replay mode ON");
		MTGS::PresentCurrentFrame();
	}
	else
	{
		m_controlQueue.push([&]() {
			m_state = Mode::Replaying;
			InputRec::log("Replay mode ON");
		});
	}
}

bool InputRecordingControls::isReplaying() const
{
	return m_state == Mode::Replaying;
}

void InputRecordingControls::processControlQueue()
{
	if (!m_controlQueue.empty())
	{

		while (!m_controlQueue.empty())
		{
			m_controlQueue.front()();
			m_controlQueue.pop();
		}
		MTGS::PresentCurrentFrame();
	}
}

bool InputRecordingControls::isRecording() const
{
	return m_state == Mode::Recording;
}


// TODO - Once there is GS Capture support again
//void InputRecordingControls::StopCapture() const
//{
//	// TODO - Vaser - Is capturing supported in Qt yet - Check
//	/*if (MainEmuFrame* mainFrame = GetMainFramePtr())
//	{
//		if (mainFrame->IsCapturing())
//		{
//			mainFrame->VideoCaptureToggle();
//			inputRec::log("Capture completed");
//		}
//	}*/
//}
