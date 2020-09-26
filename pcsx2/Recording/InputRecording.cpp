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

#include "AppGameDatabase.h"
#include "DebugTools/Debug.h"
#include "Counters.h"
#include "MainFrame.h"

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
	// As to not delete the pointers before WX attempts to
	m_newRecordingFrame.release();
	m_openFileDialog.release();
}

InputRecording::InputRecordingPad::InputRecordingPad()
{
	m_padData = std::make_unique<PadData>();
	m_state = InputRecordingMode::NotActive;
	m_seekOffset = 0;
}

InputRecording::InputRecordingPad::~InputRecordingPad()
{
	// As to not delete the pointer before WX attempts to
	m_virtualPad.release();
}

void InputRecording::InitInputRecordingWindows(wxWindow* parent)
{
	// Only init once per pcsx2 session
	if (m_newRecordingFrame.get() != nullptr)
		return;

	m_newRecordingFrame = std::make_unique<NewRecordingFrame>(parent);
	m_openFileDialog = std::make_unique<wxFileDialog>(parent, _("Select PIREC/P2M2 record file."), L"", L"",
													L"pcsx2 recording file (*.pirec,*.p2m2)|*.pirec;*.p2m2", wxFD_OPEN);
	for (u8 port = 0; port < s_NUM_PORTS; port++)
	{
		for (u8 slot = 0; slot < s_NUM_SLOTS; slot++)
		{
			InputRecordingPad& pad = GetPad(port, slot);
			pad.m_padData = std::make_unique<PadData>();
			pad.m_virtualPad = std::make_unique<VirtualPad>(parent, port, slot, g_Conf->inputRecording);
		}
	}
}

void InputRecording::ShowVirtualPad(const int arrayPosition)
{
	GetPad(arrayPosition >> 2, arrayPosition & 3).m_virtualPad->Show();
}

void InputRecording::OnBoot()
{
	// Booting is an asynchronous task. If we are playing a recording
	// that starts from power-on and the starting (pcsx2 internal) frame
	// marker has not been set, we initialize it.
	if (m_initialLoad)
		SetStartingFrame(0);
	else if (IsActive())
	{
		SetFrameCounter(0);
		g_InputRecordingControls.Lock(0);
	}
	else
		g_InputRecordingControls.Resume();
}

void InputRecording::ControllerInterrupt(const u8 data, const u8 port, const u8 slot, const u16 bufCount, u8& bufVal)
{
	const InputRecordingPad& pad = GetPad(port, slot);
	if (bufCount == 1)
		m_fInterruptFrame = data == s_READ_DATA_AND_VIBRATE_FIRST_BYTE;
	else if (bufCount == 2)
	{
		if (bufVal != s_READ_DATA_AND_VIBRATE_SECOND_BYTE)
			m_fInterruptFrame = false;
	}
	else if (m_fInterruptFrame && m_frameCounter >= 0 && m_frameCounter < INT_MAX)
	{
		const u16 bufIndex = bufCount - 3;

		if (pad.m_state == InputRecordingMode::Replaying)
		{
			u8 tmp = 0;
			if (m_inputRecordingData.ReadKeyBuffer(tmp, m_frameCounter, pad.m_seekOffset + bufIndex))
			{
				// Overwrite value originally provided by the PAD plugin
				bufVal = tmp;
			}
		}

		// Update controller data state for future VirtualPad / logging usage.
		pad.m_padData->UpdateControllerData(bufIndex, bufVal);

		if (pad.m_virtualPad->IsShown() &&
			pad.m_virtualPad->UpdateControllerData(bufIndex, pad.m_padData.get()) &&
			pad.m_state != InputRecordingMode::Replaying)
		{
			// If the VirtualPad updated the PadData, we have to update the buffer
			// before committing it to the recording / sending it to the game
			// - Do not do this if we are in replay mode!
			bufVal = pad.m_padData->PollControllerData(bufIndex);
		}

		// If we have reached the end of the pad data, log it out
		if (bufIndex == PadData::s_END_INDEX_CONTROLLER_BUFFER)
		{
			pad.m_padData->LogPadData(port);
			// As well as re-render the virtual pad UI, if applicable
			// - Don't render if it's minimized
			if (pad.m_virtualPad->IsShown() && !pad.m_virtualPad->IsIconized())
				pad.m_virtualPad->Redraw();
		}

		// Finally, commit the byte to the movie file if we are recording
		if (pad.m_state == InputRecordingMode::Recording)
		{
			if (m_incrementRedo)
			{
				m_inputRecordingData.IncrementRedoCount();
				m_incrementRedo = false;
			}
			m_inputRecordingData.WriteKeyBuffer(bufVal, m_frameCounter, pad.m_seekOffset + bufIndex);
		}
	}
}

s32 InputRecording::GetFrameCounter() const noexcept
{
	return m_frameCounter;
}

InputRecordingFile& InputRecording::GetInputRecordingData() noexcept
{
	return m_inputRecordingData;
}

u32 InputRecording::GetStartingFrame() const noexcept
{
	return m_startingFrame;
}

void InputRecording::IncrementFrameCounter() noexcept
{
	m_frameCounter++;
	if (m_state == InputRecordingMode::Recording)
		if (m_inputRecordingData.SetTotalFrames(m_frameCounter)) // Don't increment if we're at the last frame
			m_incrementRedo = false;
}

bool InputRecording::IsInterruptFrame() const noexcept
{
	return m_fInterruptFrame;
}

bool InputRecording::IsActive() const noexcept
{
	return m_state != InputRecordingMode::NotActive;
}

bool InputRecording::IsInitialLoad() const noexcept
{
	return m_initialLoad;
}

bool InputRecording::IsReplaying() const noexcept
{
	return m_state == InputRecordingMode::Replaying;
}

bool InputRecording::IsRecording() const noexcept
{
	return m_state == InputRecordingMode::Recording;
}

wxString InputRecording::RecordingModeTitleSegment() const noexcept
{
	switch (m_state)
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
	m_state = InputRecordingMode::Recording;
	// Set active VirtualPads to record mode
	{
		for (u8 port = 0; port < s_NUM_PORTS; port++)
		{
			for (u8 slot = 0; slot < s_NUM_SLOTS; slot++)
			{
				InputRecordingPad& pad = GetPad(port, slot);
				if (pad.m_state == InputRecordingMode::Replaying)
				{
					pad.m_state = InputRecordingMode::Recording;
					pad.m_virtualPad->SetReadOnlyMode(false);
				}
			}
		}
	}
	if (m_inputRecordingData.GetPadCount() == 1)
		inputRec::log("Record mode ON");
	else
		inputRec::log("All pads set to Record mode");
}

void InputRecording::SetToReplayMode()
{
	m_state = InputRecordingMode::Replaying;
	// Set active VirtualPads to record mode
	{
		for (u8 port = 0; port < s_NUM_PORTS; port++)
		{
			for (u8 slot = 0; slot < s_NUM_SLOTS; slot++)
			{
				InputRecordingPad& pad = GetPad(port, slot);
				if (pad.m_state == InputRecordingMode::Recording)
				{
					pad.m_state = InputRecordingMode::Replaying;
					pad.m_virtualPad->SetReadOnlyMode(true);
				}
			}
		}
	}
	if (m_inputRecordingData.GetPadCount() == 1)
		inputRec::log("Replay mode ON");
	else
		inputRec::log("All pads set to Replay mode");
}

void InputRecording::SetFrameCounter(u32 newGFrameCount)
{
	const u32 endFrame = m_startingFrame + m_inputRecordingData.GetTotalFrames();
	if (newGFrameCount >= endFrame)
	{
		if (newGFrameCount > endFrame)
		{
			inputRec::consoleLog("Warning, you've loaded PCSX2 emulation to a point after the end of the original recording. This should be avoided.");
			inputRec::consoleLog("Savestate's framecount has been ignored.");
		}
		if (m_state == InputRecordingMode::Replaying)
			SetToRecordMode();
		m_frameCounter = m_inputRecordingData.GetTotalFrames();
		m_incrementRedo = false;
	}
	else
	{
		if (newGFrameCount < m_startingFrame)
		{
			inputRec::consoleLog("Warning, you've loaded PCSX2 emulation to a point before the start of the original recording. This should be avoided.");
			if (m_state == InputRecordingMode::Recording)
				SetToReplayMode();
		}
		else if (newGFrameCount == 0 && m_state == InputRecordingMode::Recording)
			SetToReplayMode();
		m_frameCounter = static_cast<s32>(newGFrameCount) - m_startingFrame;
		m_incrementRedo = true;
	}
}

void InputRecording::SetStartingFrame(u32 newStartingFrame) noexcept
{
	m_startingFrame = newStartingFrame;
	// TODO - make a function of my own to simplify working with the logging macros
	if (m_inputRecordingData.FromSavestate())
		inputRec::consoleLog(fmt::format("Internal Starting Frame: {}", m_startingFrame));
	m_frameCounter = 0;
	m_initialLoad = false;
	g_InputRecordingControls.Lock(m_startingFrame);
}

void InputRecording::Stop()
{
	m_state = InputRecordingMode::NotActive;
	m_incrementRedo = false;
	if (m_inputRecordingData.Close())
	{
		if (!m_inputRecordingData.FromSavestate())
			g_Conf->EnableFastBoot = m_buffers.fastBoot;
		g_Conf->EmuOptions.MultitapPort0_Enabled = m_buffers.multitaps[0];
		g_Conf->EmuOptions.MultitapPort1_Enabled = m_buffers.multitaps[1];

		int menuId = MenuId_Recording_VirtualPad_Port0_0;
		for (u8 port = 0; port < s_NUM_PORTS; port++)
		{
			sMainFrame.enableRecordingMenuItem(MenuId_Recording_VirtualPad_Port0 + port, true);
			for (u8 slot = 0; slot < s_NUM_SLOTS; slot++, menuId++)
			{
				InputRecordingPad& pad = GetPad(port, slot);
				if (pad.m_state != InputRecordingMode::NotActive)
				{
					pad.m_state = InputRecordingMode::NotActive;
					pad.m_virtualPad->SetReadOnlyMode(false);
					pad.m_seekOffset = 0;
				}
				sMainFrame.enableRecordingMenuItem(menuId, (slot == 0 || m_buffers.multitaps[port]));
			}
		}
		if (PADSetupInputRecording)
			PADSetupInputRecording(false, 0);
		inputRec::log("Input recording stopped");
	}
}

bool InputRecording::Create()
{
	if (m_newRecordingFrame->ShowModal(CoreThread.IsOpen()) == wxID_CANCEL)
		return false;

	if (!m_inputRecordingData.OpenNew(m_newRecordingFrame->GetFile(), m_newRecordingFrame->GetStartType(), m_newRecordingFrame->GetPads()))
		return false;

	// Set emulator version
	m_inputRecordingData.GetHeader().SetEmulatorVersion();

	// Set author name
	{
		const wxString author = m_newRecordingFrame->GetAuthor();
		if (!author.IsEmpty())
			m_inputRecordingData.GetHeader().SetAuthor(author);
	}

	// Set Game Name
	m_inputRecordingData.GetHeader().SetGameName(resolveGameName());

	// Write header contents
	m_inputRecordingData.WriteHeader();

	m_state = InputRecordingMode::Recording;
	g_InputRecordingControls.DisableFrameAdvance();
	if (PADSetupInputRecording)
		PADSetupInputRecording(true, m_inputRecordingData.GetPads());
	SetPads(true);
	inputRec::log("Started new input recording");
	inputRec::consoleLog(fmt::format("Filename {}", std::string(m_inputRecordingData.GetFilename())));
	m_initialLoad = true;
	if (m_inputRecordingData.FromSavestate())
	{
		const wxString& filename = m_newRecordingFrame->GetFile();
		if (wxFileExists(filename + "_SaveState.p2s"))
			wxCopyFile(filename + "_SaveState.p2s", filename + "_SaveState.p2s.bak", true);
		StateCopy_SaveToFile(filename + "_SaveState.p2s");
	}
	else
	{
		const bool fastBoot = m_inputRecordingData.GetStartType() == InputRecordingStartType::FastBoot;
		if (fastBoot || m_inputRecordingData.GetStartType() == InputRecordingStartType::FullBoot)
		{
			if (g_Conf->EnableFastBoot != fastBoot)
			{
				m_buffers.fastBoot = g_Conf->EnableFastBoot;
				g_Conf->EnableFastBoot = fastBoot;
				AppApplySettings();
			}
			g_Conf->EmuOptions.UseBOOT2Injection = fastBoot;
		}
		sApp.SysExecute(g_Conf->CdvdSource);
	}
	return true;
}

int InputRecording::Play()
{
	if (m_openFileDialog->ShowModal() == wxID_CANCEL)
		return 0;

	if (IsActive())
		Stop();

	if (!m_inputRecordingData.OpenExisting(m_openFileDialog->GetPath()))
		return 1;

	// Either load the savestate, or restart emulation
	switch (m_inputRecordingData.GetStartType())
	{
	case InputRecordingStartType::Savestate:
		if (CoreThread.IsClosed())
		{
			recordingConLog(L"[REC]: Game is not open, aborting playing input recording which starts on a savestate.\n");
			m_inputRecordingData.Close();
			return 1;
		}
		if (!wxFileExists(m_inputRecordingData.GetFilename() + "_SaveState.p2s"))
		{
			inputRec::consoleLog(fmt::format("Could not locate savestate file at location - {}_SaveState.p2s",
											m_inputRecordingData.GetFilename()));
			m_inputRecordingData.Close();
			return 1;
		}
		m_initialLoad = true;
		StateCopy_LoadFromFile(m_inputRecordingData.GetFilename() + "_SaveState.p2s");

		if (m_initialLoad) // If the value is still true, the savestate failed to load for some reason
		{
			recordingConLog(wxString::Format("[REC]: %s_SaveState.p2s is not compatible with this version of PCSX2.\n", m_inputRecordingData.GetFilename()));
			recordingConLog(wxString::Format("[REC]: Original PCSX2 version used: %s\n", m_inputRecordingData.GetEmulatorVersion()));
			m_inputRecordingData.Close();
			m_initialLoad = false;
			return 1;
		}
		break;
	case InputRecordingStartType::FullBoot:
	case InputRecordingStartType::FastBoot:
	{
		const bool fastBoot = m_inputRecordingData.GetStartType() == InputRecordingStartType::FastBoot;
		if (g_Conf->EnableFastBoot != fastBoot)
		{
			m_buffers.fastBoot = g_Conf->EnableFastBoot;
			g_Conf->EnableFastBoot = fastBoot;
			AppApplySettings();
		}
		g_Conf->EmuOptions.UseBOOT2Injection = fastBoot;
	}
		[[fallthrough]];
	default:
		m_initialLoad = true;
		sApp.SysExecute(g_Conf->CdvdSource);
	}

	// Check if the current game matches with the one used to make the original recording
	if (!g_Conf->CurrentIso.IsEmpty())
		if (resolveGameName() != m_inputRecordingData.GetGameName())
			inputRec::consoleLog("Input recording was possibly constructed for a different game.");

	m_incrementRedo = true;
	m_state = InputRecordingMode::Replaying;
	inputRec::log("Playing input recording");
	g_InputRecordingControls.DisableFrameAdvance();
	inputRec::consoleMultiLog({fmt::format("Replaying input recording - [{}]", std::string(m_inputRecordingData.GetFilename())),
							   fmt::format("PCSX2 Version Used: {}", std::string(m_inputRecordingData.GetEmulatorVersion())),
							   fmt::format("Recording File Version: {}", m_inputRecordingData.GetFileVersion()),
							   fmt::format("Associated Game Name or ISO Filename: {}", std::string(m_inputRecordingData.GetGameName())),
							   fmt::format("Author: {}", m_inputRecordingData.GetAuthor()),
							   fmt::format("Total Frames: {}", m_inputRecordingData.GetTotalFrames()),
							   fmt::format("Redo Count: {}", m_inputRecordingData.GetRedoCount())});
	if (PADSetupInputRecording)
		PADSetupInputRecording(true, m_inputRecordingData.GetPads());
	SetPads(false);
	return 2;
}

bool InputRecording::GoToFirstFrame()
{
	if (m_inputRecordingData.FromSavestate())
	{
		if (!wxFileExists(m_inputRecordingData.GetFilename() + "_SaveState.p2s"))
		{
			recordingConLog(wxString::Format("[REC]: Could not locate savestate file at location - %s_SaveState.p2s\n",
											 m_inputRecordingData.GetFilename()));
			Stop();
			return false;
		}
		StateCopy_LoadFromFile(m_inputRecordingData.GetFilename() + "_SaveState.p2s");
	}
	else
		sApp.SysExecute(g_Conf->CdvdSource);

	if (IsRecording())
		SetToReplayMode();
	return true;
}


void InputRecording::SetPads(const bool newRecording)
{
	m_buffers.multitaps[0] = g_Conf->EmuOptions.MultitapPort0_Enabled;
	m_buffers.multitaps[1] = g_Conf->EmuOptions.MultitapPort1_Enabled;
	g_Conf->EmuOptions.MultitapPort0_Enabled |= m_inputRecordingData.IsMultitapUsed(0);
	g_Conf->EmuOptions.MultitapPort1_Enabled |= m_inputRecordingData.IsMultitapUsed(1);

	recordingConLog(L"[REC]: Pads Used: ");
	int menuId = MenuId_Recording_VirtualPad_Port0_0;
	for (u8 port = 0, padsUsed = 0; port < s_NUM_PORTS; port++)
	{
		if (!m_inputRecordingData.IsPortUsed(port))
		{
			sMainFrame.enableRecordingMenuItem(MenuId_Recording_VirtualPad_Port0 + port, false);
			menuId += 4;
			continue;
		}
		for (u8 slot = 0; slot < s_NUM_SLOTS; slot++, menuId++)
		{
			if (m_inputRecordingData.IsSlotUsed(port, slot))
			{
				recordingConLog(wxString::Format(L"%s%d%c", padsUsed > 0 ? ", " : "", port + 1, 'A' + slot));
				sMainFrame.enableRecordingMenuItem(menuId, true); // Enables the bound VirtualPad option
				InputRecordingPad& pad = GetPad(port, slot);
				pad.m_state = m_state;
				pad.m_virtualPad->SetReadOnlyMode(!newRecording);
				pad.m_seekOffset = InputRecordingFile::s_controllerInputBytes * padsUsed;
				padsUsed++;
			}
			else
			{
				sMainFrame.enableRecordingMenuItem(menuId, false); // Disables the bound VirtualPad option
				GetPad(port, slot).m_virtualPad->Close();
			}
		}
	}
	recordingConLog(L"\n");
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
