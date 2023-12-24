// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
