/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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
#include "MainFrame.h"
#include "AppSaveStates.h"
#include "ConsoleLogger.h"
#include "MSWstuff.h"

#include "Dialogs/ModalPopups.h"
#include "IsoDropTarget.h"

#include "fmt/core.h"
#include <wx/iconbndl.h>

#include <unordered_map>
#include "AppAccelerators.h"

#include "svnrev.h"
#include "Saveslots.h"

#include "Recording/InputRecording.h"
#include "Recording/InputRecordingControls.h"

// ------------------------------------------------------------------------
wxMenu* MainEmuFrame::MakeStatesSubMenu(int baseid, int loadBackupId) const
{
	wxMenu* mnuSubstates = new wxMenu();

	for (int i = 0; i < 10; i++)
	{
		// Will be changed once an iso is loaded.
		wxMenuItem* m = mnuSubstates->Append(baseid + i + 1, wxsFormat(_("Slot %d"), i), wxEmptyString, wxITEM_CHECK);
		m->Check(i == 0); // 0 is the default selected slot
	}

	if (loadBackupId >= 0)
	{
		mnuSubstates->AppendSeparator();

		wxMenuItem* m = mnuSubstates->Append(loadBackupId, _("Backup"));
		m->Enable(false);
	}
	// Implement custom hotkeys (F2) + (Shift + F2) with translatable string intact + not blank in GUI.
	// baseid in the negatives will order in a different section, so if you want to increase more slots you can still easily do this, as -1 it will have the same function as opening file for savestates, which is bad
	// For safety i also made them inactive aka grayed out to signify that's it's only for informational purposes
	// Fixme: In the future this can still be expanded to actually cycle savestates in the GUI.
	mnuSubstates->Append(baseid - 1, _("File..."));
	wxMenuItem* CycleNext = mnuSubstates->Append(baseid - 2, _("Cycle to next slot") + wxString("\t") + wxGetApp().GlobalAccels->findKeycodeWithCommandId("States_CycleSlotForward").toTitleizedString());
	CycleNext->Enable(false);
	wxMenuItem* CycleBack = mnuSubstates->Append(baseid - 3, _("Cycle to previous slot") + wxString("\t") + wxGetApp().GlobalAccels->findKeycodeWithCommandId("States_CycleSlotBackward").toTitleizedString());
	CycleBack->Enable(false);
	return mnuSubstates;
}

void MainEmuFrame::UpdateStatusBar()
{
	wxString temp(wxEmptyString);

	if (g_InputRecording.IsActive() && g_InputRecording.GetInputRecordingData().FromSaveState())
		temp += "Base Savestate - " + g_InputRecording.GetInputRecordingData().GetFilename() + "_SaveState.p2s";
	else
	{
		if (g_Conf->EnableFastBoot)
			temp += "Fast Boot - ";

		if (g_Conf->CdvdSource == CDVD_SourceType::Iso)
			temp += "Load: '" + wxFileName(g_Conf->CurrentIso).GetFullName() + "' ";
	}

	m_statusbar.SetStatusText(temp, 0);

	if (g_Conf->EnablePresets)
		m_statusbar.SetStatusText(wxString::Format(L"P:%d", g_Conf->PresetIndex + 1), 1);
	else
		m_statusbar.SetStatusText("---", 1);

	m_statusbar.SetStatusText(CDVD_SourceLabels[enum_cast(g_Conf->CdvdSource)], 2);

	m_statusbar.SetStatusText("x64", 3);
}

void MainEmuFrame::UpdateCdvdSrcSelection()
{
	MenuIdentifiers cdsrc = MenuId_Src_Iso;

	switch (g_Conf->CdvdSource)
	{
		case CDVD_SourceType::Iso:
			cdsrc = MenuId_Src_Iso;
			break;
		case CDVD_SourceType::Disc:
			cdsrc = MenuId_Src_Disc;
			break;
		case CDVD_SourceType::NoDisc:
			cdsrc = MenuId_Src_NoDisc;
			break;

			jNO_DEFAULT
	}
	sMenuBar.Check(cdsrc, true);
	if (!g_InputRecording.IsActive())
	{
		ApplyCDVDStatus();
	}
	UpdateStatusBar();
}

bool MainEmuFrame::Destroy()
{
	// Sigh: wxWidgets doesn't issue Destroy() calls for children windows when the parent
	// is destroyed (it just deletes them, quite suddenly).  So let's do it for them, since
	// our children have configuration stuff they like to do when they're closing.

	for (
		wxWindowList::const_iterator
			i = wxTopLevelWindows.begin(),
			end = wxTopLevelWindows.end();
		i != end; ++i)
	{
		wxTopLevelWindow* const win = wx_static_cast(wxTopLevelWindow*, *i);
		if (win == this)
			continue;
		if (win->GetParent() != this)
			continue;

		win->Destroy();
	}

	return _parent::Destroy();
}

// ------------------------------------------------------------------------
//     MainFrame OnEvent Handlers
// ------------------------------------------------------------------------

// Close out the console log windows along with the main emu window.
// Note: This event only happens after a close event has occurred and was *not* veto'd.  Ie,
// it means it's time to provide an unconditional closure of said window.
//
void MainEmuFrame::OnCloseWindow(wxCloseEvent& evt)
{
	// the main thread is busy suspending everything, so let's not try to call it 
	// when closing the emulator
	//init_gspanel = false;

	if (IsBeingDeleted())
		return;

	CoreThread.Suspend();

	//bool isClosing = false;

	if (!evt.CanVeto())
	{
		// Mandatory destruction...
		//isClosing = true;
	}
	else
	{
		// TODO : Add confirmation prior to exit here!
		// Problem: Suspend is often slow because it needs to wait until the current EE frame
		// has finished processing (if the GS or logging has incurred severe overhead this makes
		// closing PCSX2 difficult).  A non-blocking suspend with modal dialog might suffice
		// however. --air

		//evt.Veto( true );
	}

	sApp.OnMainFrameClosed(GetId());

	RemoveCdvdMenu();

	RemoveEventHandler(&wxGetApp().GetRecentIsoManager());
	wxGetApp().PostIdleAppMethod(&Pcsx2App::PrepForExit);

	evt.Skip();
}

void MainEmuFrame::OnMoveAround(wxMoveEvent& evt)
{
	if (IsBeingDeleted() || !IsVisible() || IsIconized())
		return;

	// Uncomment this when doing logger stress testing (and then move the window around
	// while the logger spams itself)
	// ... makes for a good test of the message pump's responsiveness.
	if (EnableThreadedLoggingTest)
		Console.Warning("Threaded Logging Test!  (a window move event)");

	// evt.GetPosition() returns the client area position, not the window frame position.
	// So read the window's screen-relative position directly.
	g_Conf->MainGuiPosition = GetScreenPosition();

	// wxGTK note: X sends gratuitous amounts of OnMove messages for various crap actions
	// like selecting or deselecting a window, which muck up docking logic.  We filter them
	// out using 'lastpos' here. :)

	static wxPoint lastpos(wxDefaultCoord, wxDefaultCoord);
	if (lastpos == evt.GetPosition())
		return;
	lastpos = evt.GetPosition();

	if (g_Conf->ProgLogBox.AutoDock)
	{
		if (ConsoleLogFrame* proglog = wxGetApp().GetProgramLog())
		{
			if (!proglog->IsMaximized())
			{
				g_Conf->ProgLogBox.DisplayPosition = GetRect().GetTopRight();
				proglog->SetPosition(g_Conf->ProgLogBox.DisplayPosition);
			}
		}
	}

	evt.Skip();
}

void MainEmuFrame::OnLogBoxHidden()
{
	g_Conf->ProgLogBox.Visible = false;
	m_MenuItem_Console.Check(false);
}

// ------------------------------------------------------------------------
void MainEmuFrame::ConnectMenus()
{
	// System
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_BootCdvd_Click, this, MenuId_Boot_CDVD);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_FastBoot_Click, this, MenuId_Config_FastBoot);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_OpenELF_Click, this, MenuId_Boot_ELF);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_SuspendResume_Click, this, MenuId_Sys_SuspendResume);

	Bind(wxEVT_MENU, &MainEmuFrame::Menu_LoadStates_Click, this, MenuId_State_Load01 + 1, MenuId_State_Load01 + 10);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_LoadStates_Click, this, MenuId_State_LoadBackup);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_LoadStateFromFile_Click, this, MenuId_State_LoadFromFile);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_SaveStates_Click, this, MenuId_State_Save01 + 1, MenuId_State_Save01 + 10);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_SaveStateToFile_Click, this, MenuId_State_SaveToFile);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_EnableBackupStates_Click, this, MenuId_EnableBackupStates);

	Bind(wxEVT_MENU, &MainEmuFrame::Menu_EnablePatches_Click, this, MenuId_EnablePatches);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_EnableCheats_Click, this, MenuId_EnableCheats);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_PINE_Enable_Click, this, MenuId_PINE_Enable);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_PINE_Settings_Click, this, MenuId_PINE_Settings);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_EnableWideScreenPatches_Click, this, MenuId_EnableWideScreenPatches);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_EnableRecordingTools_Click, this, MenuId_EnableInputRecording);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_EnableHostFs_Click, this, MenuId_EnableHostFs);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_SysShutdown_Click, this, MenuId_Sys_Shutdown);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_Exit_Click, this, MenuId_Exit);

	// CDVD
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_IsoBrowse_Click, this, MenuId_IsoBrowse);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_IsoClear_Click, this, MenuId_IsoClear);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_IsoClearMissing_Click, this, MenuId_IsoClearMissing);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_CdvdSource_Click, this, MenuId_Src_Iso);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_CdvdSource_Click, this, MenuId_Src_Disc);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_CdvdSource_Click, this, MenuId_Src_NoDisc);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_Ask_On_Boot_Click, this, MenuId_Ask_On_Booting);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_Debug_CreateBlockdump_Click, this, MenuId_Debug_CreateBlockdump);

	// Config
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_SysSettings_Click, this, MenuId_Config_SysSettings);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_McdSettings_Click, this, MenuId_Config_McdSettings);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_AudioSettings_Click, this, MenuId_Config_SPU2);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_NetworkSettings_Click, this, MenuId_Config_DEV9);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_USBSettings_Click, this, MenuId_Config_USB);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_PADSettings_Click, this, MenuId_Config_PAD);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_GSSettings_Click, this, MenuId_Config_GS);

	Bind(wxEVT_MENU, &MainEmuFrame::Menu_GSSettings_Click, this, MenuId_Video_CoreSettings);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_WindowSettings_Click, this, MenuId_Video_WindowSettings);

	Bind(wxEVT_MENU, &MainEmuFrame::Menu_MultitapToggle_Click, this, MenuId_Config_Multitap0Toggle);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_MultitapToggle_Click, this, MenuId_Config_Multitap1Toggle);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_ResetAllSettings_Click, this, MenuId_Config_ResetAll);

	// Misc
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_ShowConsole, this, MenuId_Console);
#if defined(__POSIX__)
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_ShowConsole_Stdio, this, MenuId_Console_Stdio);
#endif

	Bind(wxEVT_MENU, &MainEmuFrame::Menu_GetStarted, this, MenuId_Help_GetStarted);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_Compatibility, this, MenuId_Help_Compatibility);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_Forums, this, MenuId_Help_Forums);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_Website, this, MenuId_Help_Website);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_Github, this, MenuId_Help_Github);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_Wiki, this, MenuId_Help_Wiki);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_ShowAboutBox, this, MenuId_About);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_ShowGSDump, this, MenuId_GSDump);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_ChangeLang, this, MenuId_ChangeLang);

	// Debug
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_Debug_Open_Click, this, MenuId_Debug_Open);

	// Capture
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_Capture_Video_ToggleCapture_Click, this, MenuId_Capture_Video_Record);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_Capture_Video_ToggleCapture_Click, this, MenuId_Capture_Video_Stop);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_Capture_Video_IncludeAudio_Click, this, MenuId_Capture_Video_IncludeAudio);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_Capture_Screenshot_Screenshot_Click, this, MenuId_Capture_Screenshot_Screenshot);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_Capture_Screenshot_Screenshot_As_Click, this, MenuId_Capture_Screenshot_Screenshot_As);

	// Recording
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_Recording_New_Click, this, MenuId_Recording_New);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_Recording_Play_Click, this, MenuId_Recording_Play);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_Recording_Stop_Click, this, MenuId_Recording_Stop);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_Recording_Config_FrameAdvance, this, MenuId_Recording_Config_FrameAdvance);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_Recording_TogglePause_Click, this, MenuId_Recording_TogglePause);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_Recording_FrameAdvance_Click, this, MenuId_Recording_FrameAdvance);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_Recording_ToggleRecordingMode_Click, this, MenuId_Recording_ToggleRecordingMode);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_Recording_VirtualPad_Open_Click, this, MenuId_Recording_VirtualPad_Port0);
	Bind(wxEVT_MENU, &MainEmuFrame::Menu_Recording_VirtualPad_Open_Click, this, MenuId_Recording_VirtualPad_Port1);
}

void MainEmuFrame::InitLogBoxPosition(AppConfig::ConsoleLogOptions& conf)
{
	conf.DisplaySize.Set(
		std::min(std::max(conf.DisplaySize.GetWidth(), 160), wxGetDisplayArea().GetWidth()),
		std::min(std::max(conf.DisplaySize.GetHeight(), 160), wxGetDisplayArea().GetHeight()));

	if (conf.AutoDock)
	{
		conf.DisplayPosition = GetScreenPosition() + wxSize(GetSize().x, 0);
	}
	else if (conf.DisplayPosition != wxDefaultPosition)
	{
		if (!wxGetDisplayArea().Contains(wxRect(conf.DisplayPosition, conf.DisplaySize)))
			conf.DisplayPosition = wxDefaultPosition;
	}
}

void MainEmuFrame::DispatchEvent(const CoreThreadStatus& status)
{
	if (!pxAssertMsg(GetMenuBar() != NULL, "Mainframe menu bar is NULL!"))
		return;
	ApplySuspendStatus();
}

void MainEmuFrame::AppStatusEvent_OnSettingsApplied()
{
	ApplySettings();
}

void MainEmuFrame::CreatePcsx2Menu()
{
	// ------------------------------------------------------------------------
	// Some of the items in the System menu are configured by the UpdateCoreStatus() method.

	m_menuSys.Append(MenuId_Boot_CDVD, _("Initializing..."));

	m_menuSys.Append(MenuId_Sys_SuspendResume, _("Initializing..."));

	m_menuSys.Append(MenuId_Sys_Shutdown, _("Shut&down"),
		_("Wipes all internal VM states."));
	m_menuSys.FindItem(MenuId_Sys_Shutdown)->Enable(false);

	m_menuSys.Append(MenuId_Boot_ELF, _("&Run ELF..."),
		_("For running raw PS2 binaries directly."));

	m_menuSys.AppendSeparator();

	m_menuSys.Append(MenuId_Config_FastBoot, _("Fast Boot"),
		_("Skips PS2 splash screens when booting from ISO or DVD media"), wxITEM_CHECK);

	m_menuSys.Append(MenuId_GameSettingsSubMenu, _("&Game Settings"), &m_GameSettingsSubmenu);

	m_GameSettingsSubmenu.Append(MenuId_EnablePatches, _("Automatic &Gamefixes"),
		_("Automatically applies needed Gamefixes to known problematic games."), wxITEM_CHECK);

	m_GameSettingsSubmenu.Append(MenuId_EnableCheats, _("Enable &Cheats"),
		_("Use cheats otherwise known as pnachs from the cheats folder."), wxITEM_CHECK);

	m_GameSettingsSubmenu.Append(MenuId_PINE, _("Configure &PINE"), &m_submenuPINE);

	m_submenuPINE.Append(MenuId_PINE_Enable, _("&Enable PINE"),
		wxEmptyString, wxITEM_CHECK);

	m_submenuPINE.Append(MenuId_PINE_Settings, _("PINE &Settings"));

	m_GameSettingsSubmenu.Append(MenuId_EnableWideScreenPatches, _("Enable &Widescreen Patches"),
		_("Enabling Widescreen Patches may occasionally cause issues."), wxITEM_CHECK);

	m_GameSettingsSubmenu.Append(MenuId_EnableInputRecording, _("Enable &Input Recording"),
		_("Input Recording for controller/keyboard presses, tools for automation and playback."), wxITEM_CHECK);

	m_GameSettingsSubmenu.Append(MenuId_EnableHostFs, _("Enable &Host Filesystem"),
		wxEmptyString, wxITEM_CHECK);

	m_menuSys.AppendSeparator();
	// Implement custom hotkeys (F3) with translatable string intact + not blank in GUI.
	wxMenuItem* sysLoadStateItem = m_menuSys.Append(MenuId_Sys_LoadStates, _("&Load state"), &m_LoadStatesSubmenu);
	AppendShortcutToMenuOption(*sysLoadStateItem, wxGetApp().GlobalAccels->findKeycodeWithCommandId("States_DefrostCurrentSlot").toTitleizedString());
	// Implement custom hotkeys (F1) with translatable string intact + not blank in GUI.
	wxMenuItem* sysSaveStateItem = m_menuSys.Append(MenuId_Sys_SaveStates, _("&Save state"), &m_SaveStatesSubmenu);
	AppendShortcutToMenuOption(*sysSaveStateItem, wxGetApp().GlobalAccels->findKeycodeWithCommandId("States_FreezeCurrentSlot").toTitleizedString());

	m_menuSys.Append(MenuId_EnableBackupStates, _("&Backup before save"), wxEmptyString, wxITEM_CHECK);

	m_menuSys.AppendSeparator();

	m_menuSys.Append(MenuId_Exit, _("E&xit"),
		AddAppName(_("Closing %s may be hazardous to your health.")));
}

void MainEmuFrame::CreateCdvdMenu()
{
	// ------------------------------------------------------------------------
	wxMenu& isoRecents(wxGetApp().GetRecentIsoMenu());
	wxMenu& driveList(wxGetApp().GetDriveListMenu());

	m_menuItem_RecentIsoMenu = m_menuCDVD.AppendSubMenu(&isoRecents, _("ISO &Selector"));
	m_menuItem_DriveListMenu = m_menuCDVD.AppendSubMenu(&driveList, _("D&rive Selector"));

	m_menuCDVD.AppendSeparator();
	m_menuCDVD.Append(MenuId_Src_Iso, _("&ISO"), _("Makes the specified ISO image the CDVD source."), wxITEM_RADIO);
	m_menuCDVD.Append(MenuId_Src_Disc, _("&Disc"), _("Uses a disc drive as the CDVD source."), wxITEM_RADIO);
	m_menuCDVD.Append(MenuId_Src_NoDisc, _("&No disc"), _("Use this to boot into your virtual PS2's BIOS configuration."), wxITEM_RADIO);

#if defined(__FREEBSD__) || defined(__APPLE__)
	m_menuItem_DriveListMenu->Enable(false);
	m_menuCDVD.Enable(MenuId_Src_Disc, false);
#endif
}


void MainEmuFrame::CreateConfigMenu()
{
	m_menuConfig.Append(MenuId_Config_SysSettings, _("General &Settings"));
	m_menuConfig.Append(MenuId_Config_McdSettings, _("&Memory Cards"));
	m_menuConfig.AppendSeparator();
	m_menuConfig.Append(MenuId_Config_GS, _("&Graphics Settings"));
	m_menuConfig.Append(MenuId_Config_SPU2, _("&Audio Settings"));
	m_menuConfig.Append(MenuId_Config_PAD, _("Game&pad Settings"));
	m_menuConfig.Append(MenuId_Config_DEV9, _("&Network and HDD Settings"));
	m_menuConfig.Append(MenuId_Config_USB, _("&USB Settings"));
	m_menuConfig.AppendSeparator();

	m_menuConfig.Append(MenuId_Config_Multitap0Toggle, _("Multitap &1"), wxEmptyString, wxITEM_CHECK);
	m_menuConfig.Append(MenuId_Config_Multitap1Toggle, _("Multitap &2"), wxEmptyString, wxITEM_CHECK);

	m_menuConfig.AppendSeparator();

	m_menuConfig.Append(MenuId_ChangeLang, L"Change &Language"); // Always in English
	m_menuConfig.Append(MenuId_Config_ResetAll, _("&Clear All Settings"),
		AddAppName(_("Clears all %s settings and re-runs the startup wizard.")));
}

void MainEmuFrame::CreateWindowsMenu()
{
	m_menuWindow.Append(MenuId_Debug_CreateBlockdump, _("Create &Blockdump"), _("Creates a block dump for debugging purposes."), wxITEM_CHECK);
	m_menuWindow.Append(MenuId_Debug_Open, _("&Show Debugger"), wxEmptyString, wxITEM_CHECK);

#ifndef PCSX2_CI
	if (IsDevBuild || g_Conf->DevMode)
#endif
		m_menuWindow.Append(MenuId_GSDump, _("Show &GS Debugger"));

	m_menuWindow.Append(&m_MenuItem_Console);
#if defined(__POSIX__)
	m_menuWindow.AppendSeparator();
	m_menuWindow.Append(&m_MenuItem_Console_Stdio);
#endif
}

void MainEmuFrame::CreateCaptureMenu()
{
	m_menuCapture.Append(MenuId_Capture_Video, _("Video"), &m_submenuVideoCapture);
	// Implement custom hotkeys (F12) with translatable string intact + not blank in GUI.
	wxMenuItem* sysVideoCaptureItem = m_submenuVideoCapture.Append(MenuId_Capture_Video_Record, _("Start Video Capture"));
	AppendShortcutToMenuOption(*sysVideoCaptureItem, wxGetApp().GlobalAccels->findKeycodeWithCommandId("Sys_RecordingToggle").toTitleizedString());
	sysVideoCaptureItem = m_submenuVideoCapture.Append(MenuId_Capture_Video_Stop, _("Stop Video Capture"));
	sysVideoCaptureItem->Enable(false);
	AppendShortcutToMenuOption(*sysVideoCaptureItem, wxGetApp().GlobalAccels->findKeycodeWithCommandId("Sys_RecordingToggle").toTitleizedString());
	m_submenuVideoCapture.AppendSeparator();
	m_submenuVideoCapture.Append(MenuId_Capture_Video_IncludeAudio, _("Include Audio"),
		_("Enables/disables the creation of a synchronized wav audio file when capturing video footage."), wxITEM_CHECK);
	// Implement custom hotkeys (F8) + (Shift + F8) + (Ctrl + Shift + F8) with translatable string intact + not blank in GUI.
	// Fixme: GlobalCommands.cpp L1029-L1031 is having issues because FrameForGS already maps the hotkey first.
	// Fixme: When you uncomment L1029-L1031 on that file; Linux says that Ctrl is already used for something else and will append (Shift + F8) while Windows will (Ctrl + Shift + F8)
	m_menuCapture.Append(MenuId_Capture_Screenshot, _("Screenshot"), &m_submenuScreenshot);
	wxMenuItem* sysScreenShotItem = m_submenuScreenshot.Append(MenuId_Capture_Screenshot_Screenshot, _("Take Screenshot"));
	// HACK: in AcceleratorDictionary::Map the Sys_TakeSnapshot entry gets Shift and Cmd (Ctrl) hardcoded to it because it is similarly hardcoded in GS
	// So... remove such modifiers as the GUI menu entry is only for the base keybinding without modifiers.
	// We can be confident in doing so, as if a user adds these modifiers themselves, the same function rejects it.
	KeyAcceleratorCode keyCode = wxGetApp().GlobalAccels->findKeycodeWithCommandId("Sys_TakeSnapshot");
	keyCode.Shift(false);
	keyCode.Cmd(false);
	AppendShortcutToMenuOption(*sysScreenShotItem, keyCode.toTitleizedString());
	m_submenuScreenshot.Append(MenuId_Capture_Screenshot_Screenshot_As, _("Save Screenshot As..."));
}

void MainEmuFrame::CreateInputRecordingMenu()
{
	m_menuRecording.Append(MenuId_Recording_New, _("New"), _("Create a new input recording."))->Enable(false);
	m_menuRecording.Append(MenuId_Recording_Stop, _("Stop"), _("Stop the active input recording."))->Enable(false);
	m_menuRecording.Append(MenuId_Recording_Play, _("Play"), _("Playback an existing input recording."))->Enable(false);
	m_menuRecording.AppendSeparator();

	m_menuRecording.Append(MenuId_Recording_Settings, _("Settings"), &m_submenu_recording_settings);
	wxString frame_advance_label = wxString(_("Configure Frame Advance"));
	frame_advance_label.Append(wxString::Format(" (%d)", g_Conf->inputRecording.m_frame_advance_amount));
	m_submenu_recording_settings.Append(MenuId_Recording_Config_FrameAdvance, frame_advance_label, _("Change the amount of frames advanced each time"));
	m_menuRecording.AppendSeparator();

	m_menuRecording.Append(MenuId_Recording_TogglePause, _("Toggle Pause"), _("Pause or resume emulation on the fly."))->Enable(false);
	m_menuRecording.Append(MenuId_Recording_FrameAdvance, _("Frame Advance"), _("Advance emulation forward by a single frame at a time."))->Enable(false);
	m_menuRecording.Append(MenuId_Recording_ToggleRecordingMode, _("Toggle Recording Mode"), _("Save/playback inputs to/from the recording file."))->Enable(false);
	m_menuRecording.AppendSeparator();

	m_menuRecording.Append(MenuId_Recording_VirtualPad_Port0, _("Virtual Pad (Port 1)"));
	m_menuRecording.Append(MenuId_Recording_VirtualPad_Port1, _("Virtual Pad (Port 2)"));
}

void MainEmuFrame::CreateHelpMenu()
{
	m_menuHelp.Append(MenuId_Help_GetStarted, _("&Getting Started"));
	m_menuHelp.Append(MenuId_Help_Compatibility, _("&Compatibility"));
	m_menuHelp.AppendSeparator();
	m_menuHelp.Append(MenuId_Help_Website, _("&Website"));
	m_menuHelp.Append(MenuId_Help_Wiki, _("&Wiki"));
	m_menuHelp.Append(MenuId_Help_Forums, _("&Support Forums"));
	m_menuHelp.Append(MenuId_Help_Github, _("&GitHub Repository"));
	m_menuHelp.AppendSeparator();
	m_menuHelp.Append(MenuId_About, _("&About"));
}

// ------------------------------------------------------------------------
MainEmuFrame::MainEmuFrame(wxWindow* parent, const wxString& title)
	: wxFrame(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE & ~(wxMAXIMIZE_BOX | wxRESIZE_BORDER))

	, m_statusbar(*CreateStatusBar(2, 0))
	, m_background(new wxStaticBitmap(this, wxID_ANY, wxGetApp().GetLogoBitmap()))

	// All menu components must be created on the heap!

	, m_menubar(*new wxMenuBar())

	, m_menuCDVD(*new wxMenu())
	, m_menuSys(*new wxMenu())
	, m_menuConfig(*new wxMenu())
	, m_menuWindow(*new wxMenu())
	, m_menuCapture(*new wxMenu())
	, m_submenuVideoCapture(*new wxMenu())
	, m_submenuPINE(*new wxMenu())
	, m_submenuScreenshot(*new wxMenu())
	, m_menuRecording(*new wxMenu())
	, m_submenu_recording_settings(*new wxMenu())
	, m_menuHelp(*new wxMenu())
	, m_LoadStatesSubmenu(*MakeStatesSubMenu(MenuId_State_Load01, MenuId_State_LoadBackup))
	, m_SaveStatesSubmenu(*MakeStatesSubMenu(MenuId_State_Save01))
	, m_GameSettingsSubmenu(*new wxMenu())

	, m_MenuItem_Console(*new wxMenuItem(&m_menuWindow, MenuId_Console, _("&Show Program Log"), wxEmptyString, wxITEM_CHECK))
#if defined(__POSIX__)
	, m_MenuItem_Console_Stdio(*new wxMenuItem(&m_menuWindow, MenuId_Console_Stdio, _("&Program Log to Stdio"), wxEmptyString, wxITEM_CHECK))
#endif

{
	m_RestartEmuOnDelete = false;
	m_capturingVideo = false;

	// ------------------------------------------------------------------------
	// Initial menubar setup.  This needs to be done first so that the menu bar's visible size
	// can be factored into the window size (which ends up being background+status+menus)

	m_menubar.Append(&m_menuSys, _("&System"));
	m_menubar.Append(&m_menuCDVD, _("CD&VD"));
	m_menubar.Append(&m_menuConfig, _("&Config"));
	m_menubar.Append(&m_menuWindow, _("&Debug"));
	m_menubar.Append(&m_menuCapture, _("Captu&re"));

	SetMenuBar(&m_menubar);

	// Append the Recording options if previously enabled and setting has been picked up from ini
	if (g_Conf->EmuOptions.EnableRecordingTools)
	{
		m_menubar.Append(&m_menuRecording, _("&Input Record"));
	}
	m_menubar.Append(&m_menuHelp, _("&Help"));

	// ------------------------------------------------------------------------

	// The background logo and its window size are different on Windows. Use the
	// background logo size, which is what it'll eventually be resized to.
	wxSize backsize(m_background->GetBitmap().GetWidth(), m_background->GetBitmap().GetHeight());

	wxString wintitle;
	if (PCSX2_isReleaseVersion)
	{
		// stable releases, with a simple title.
		wintitle.Printf(L"%s  %d.%d.%d", pxGetAppName().c_str(), PCSX2_VersionHi, PCSX2_VersionMid, PCSX2_VersionLo);
	}
	else if (GIT_TAGGED_COMMIT) // Nightly builds
	{
		// tagged commit - more modern implementation of dev build versioning
		// - there is no need to include the commit - that is associated with the tag, git is implied
		wintitle.Printf(L"%s Nightly - %s", pxGetAppName().c_str(), GIT_TAG);
	}
	else
	{
		// beta / development editions, which feature revision number and compile date.
		if (strlen(GIT_REV) > 5)
		{
			wintitle.Printf(L"%s %s", pxGetAppName().c_str(), GIT_REV);
		}
		else
		{
			wintitle.Printf(L"%s  %d.%d.%d-%lld (git)", pxGetAppName().c_str(), PCSX2_VersionHi, PCSX2_VersionMid,
				PCSX2_VersionLo, SVN_REV);
		}
	}

	SetTitle(wintitle);

	// Ideally the __WXMSW__ port should use the embedded IDI_ICON2 icon, because wxWidgets sucks and
	// loses the transparency information when loading bitmaps into icons.  But for some reason
	// I cannot get it to work despite following various examples to the letter.
	SetIcons(wxGetApp().GetIconBundle());

	int m_statusbar_widths[] = {(int)-20, (int)-3, (int)-3, (int)-3};
	m_statusbar.SetFieldsCount(4);
	m_statusbar.SetStatusWidths(4, m_statusbar_widths);
	m_statusbar.SetStatusText(wxEmptyString, 0);

	wxBoxSizer& joe(*new wxBoxSizer(wxVERTICAL));
	joe.Add(m_background);
	SetSizerAndFit(&joe);

	// Makes no sense, but this is needed for the window size to be correct for
	// 200% DPI on Windows. The SetSizerAndFit is supposed to be doing the exact
	// same thing.
	GetSizer()->SetSizeHints(this);

	// Use default window position if the configured windowpos is invalid (partially offscreen)
	if (g_Conf->MainGuiPosition == wxDefaultPosition || !pxIsValidWindowPosition(*this, g_Conf->MainGuiPosition))
		g_Conf->MainGuiPosition = GetScreenPosition();
	else
		SetPosition(g_Conf->MainGuiPosition);

	// Updating console log positions after the main window has been fitted to its sizer ensures
	// proper docked positioning, since the main window's size is invalid until after the sizer
	// has been set/fit.

	InitLogBoxPosition(g_Conf->ProgLogBox);
	CreatePcsx2Menu();
	CreateCdvdMenu();
	CreateConfigMenu();
	CreateWindowsMenu();
	CreateCaptureMenu();
	CreateInputRecordingMenu();
	CreateHelpMenu();

	m_MenuItem_Console.Check(g_Conf->ProgLogBox.Visible);

	ConnectMenus();
	Bind(wxEVT_MOVE, &MainEmuFrame::OnMoveAround, this);
	Bind(wxEVT_CLOSE_WINDOW, &MainEmuFrame::OnCloseWindow, this);
	Bind(wxEVT_SET_FOCUS, &MainEmuFrame::OnFocus, this);
	Bind(wxEVT_ACTIVATE, &MainEmuFrame::OnActivate, this);

	PushEventHandler(&wxGetApp().GetRecentIsoManager());
	SetDropTarget(new IsoDropTarget(this));

	ApplyCoreStatus();
	ApplySettings();
}

MainEmuFrame::~MainEmuFrame()
{
	try
	{
		if (m_RestartEmuOnDelete)
		{
			sApp.SetExitOnFrameDelete(false);
			sApp.PostAppMethod(&Pcsx2App::DetectCpuAndUserMode);
			sApp.WipeUserModeSettings();
		}
	}
	DESTRUCTOR_CATCHALL
}

void MainEmuFrame::DoGiveHelp(const wxString& text, bool show)
{
	_parent::DoGiveHelp(text, show);
	wxGetApp().GetProgramLog()->DoGiveHelp(text, show);
}

// ----------------------------------------------------------------------------
// OnFocus / OnActivate : Special implementation to "connect" the console log window
// with the main frame window.  When one is clicked, the other is assured to be brought
// to the foreground with it.  (Currently only MSW only, as wxWidgets appears to have no
// equivalent to this).  Both OnFocus and OnActivate are handled because Focus events do
// not propagate up the window hierarchy, and on Activate events don't always get sent
// on the first focusing event after PCSX2 starts.

void MainEmuFrame::OnFocus(wxFocusEvent& evt)
{
	if (ConsoleLogFrame* logframe = wxGetApp().GetProgramLog())
		MSW_SetWindowAfter(logframe->GetHandle(), GetHandle());

	evt.Skip();
}

void MainEmuFrame::OnActivate(wxActivateEvent& evt)
{
	if (ConsoleLogFrame* logframe = wxGetApp().GetProgramLog())
		MSW_SetWindowAfter(logframe->GetHandle(), GetHandle());

	evt.Skip();
}
// ----------------------------------------------------------------------------

void MainEmuFrame::ApplyCoreStatus()
{
	ApplySuspendStatus();
	ApplyCDVDStatus();
}

void MainEmuFrame::ApplySuspendStatus()
{
	// [TODO] : Ideally each of these items would bind a listener instance to the AppCoreThread
	// dispatcher, and modify their states accordingly.  This is just a hack (for now) -- air

	if (wxMenuItem* susres = GetMenuBar()->FindItem(MenuId_Sys_SuspendResume))
	{
		if (!CoreThread.IsClosing())
		{
			susres->Enable();
			susres->SetItemLabel(_("Paus&e"));
			susres->SetHelp(_("Safely pauses emulation and preserves the PS2 state."));
		}
		else
		{
			bool ActiveVM = SysHasValidState();
			susres->Enable(ActiveVM);
			if (ActiveVM)
			{
				susres->SetItemLabel(_("R&esume"));
				susres->SetHelp(_("Resumes the suspended emulation state."));
			}
			else
			{
				susres->SetItemLabel(_("Pause/Resume"));
				susres->SetHelp(_("No emulation state is active; cannot suspend or resume."));
			}
		}
		// Re-init keybinding after changing the label.
		AppendShortcutToMenuOption(*susres, wxGetApp().GlobalAccels->findKeycodeWithCommandId("Sys_SuspendResume").toTitleizedString());
	}
}

void MainEmuFrame::ApplyCDVDStatus()
{
	const CDVD_SourceType Source = g_Conf->CdvdSource;

	wxMenuItem* cdvd_menu = GetMenuBar()->FindItem(MenuId_Boot_CDVD);

	wxString label;
	wxString help_text = _("Use fast boot to skip PS2 startup and splash screens.");

	switch (Source)
	{
		case CDVD_SourceType::Iso:
			label = _("Boot ISO");
			break;
		case CDVD_SourceType::Disc:
			label = _("Boot CDVD");
			break;
		case CDVD_SourceType::NoDisc:
			label = _("Boot BIOS");
			break;
		default:
			label = _("Boot BIOS");
			break;
	}

	cdvd_menu->SetItemLabel(label);
	cdvd_menu->SetHelp(help_text);
}

//Apply a config to the menu such that the menu reflects it properly
void MainEmuFrame::ApplySettings()
{
	ApplyConfigToGui(*g_Conf);
}

//MainEmuFrame needs to be aware which items are affected by presets if AppConfig::APPLY_FLAG_FROM_PRESET is on.
//currently only EnablePatches is affected when the settings come from a preset.
void MainEmuFrame::ApplyConfigToGui(AppConfig& configToApply, int flags)
{
	wxMenuBar& menubar(*GetMenuBar());

	menubar.Check(MenuId_EnablePatches, configToApply.EmuOptions.EnablePatches);
	menubar.Enable(MenuId_EnablePatches, !configToApply.EnablePresets);

	if (!(flags & AppConfig::APPLY_FLAG_FROM_PRESET))
	{ //these should not be affected by presets
		menubar.Check(MenuId_EnableBackupStates, configToApply.EmuOptions.BackupSavestate);
		menubar.Check(MenuId_EnableCheats, configToApply.EmuOptions.EnableCheats);
		menubar.Check(MenuId_PINE_Enable, configToApply.EmuOptions.EnablePINE);
		menubar.Check(MenuId_EnableWideScreenPatches, configToApply.EmuOptions.EnableWideScreenPatches);
		menubar.Check(MenuId_Capture_Video_IncludeAudio, configToApply.AudioCapture.EnableAudio);

		menubar.Check(MenuId_EnableInputRecording, configToApply.EmuOptions.EnableRecordingTools);
		wxString frame_advance_label = wxString(_("Configure Frame Advance"));
		frame_advance_label.Append(wxString::Format(" (%d)", configToApply.inputRecording.m_frame_advance_amount));
		m_submenu_recording_settings.SetLabel(MenuId_Recording_Config_FrameAdvance, frame_advance_label);
		g_InputRecordingControls.setFrameAdvanceAmount(configToApply.inputRecording.m_frame_advance_amount);

		menubar.Check(MenuId_EnableHostFs, configToApply.EmuOptions.HostFs);
		menubar.Check(MenuId_Debug_CreateBlockdump, configToApply.EmuOptions.CdvdDumpBlocks);
#if defined(__POSIX__)
		menubar.Check(MenuId_Console_Stdio, configToApply.EmuOptions.ConsoleToStdio);
#endif

		menubar.Check(MenuId_Config_Multitap0Toggle, configToApply.EmuOptions.MultitapPort0_Enabled);
		menubar.Check(MenuId_Config_Multitap1Toggle, configToApply.EmuOptions.MultitapPort1_Enabled);
		menubar.Check(MenuId_Config_FastBoot, configToApply.EnableFastBoot);
	}

	UpdateCdvdSrcSelection(); //shouldn't be affected by presets but updates from g_Conf anyway and not from configToApply, so no problem here.
}

//write pending preset settings from the gui to g_Conf,
//	without triggering an overall "settingsApplied" event.
void MainEmuFrame::CommitPreset_noTrigger()
{
	wxMenuBar& menubar(*GetMenuBar());
	g_Conf->EmuOptions.EnablePatches = menubar.IsChecked(MenuId_EnablePatches);
}

void MainEmuFrame::AppendShortcutToMenuOption(wxMenuItem& item, wxString keyCodeStr)
{
	wxString text = item.GetItemLabel();
	const size_t tabPos = text.rfind(L'\t');
	item.SetItemLabel(text.Mid(0, tabPos) + L"\t" + keyCodeStr);
}

void MainEmuFrame::initializeRecordingMenuItem(MenuIdentifiers menuId, wxString keyCodeStr, bool enable)
{
	wxMenuItem& item = *m_menuRecording.FindChildItem(menuId);
	wxString text = item.GetItemLabel();
	const size_t tabPos = text.rfind(L'\t');
	item.SetItemLabel(text.Mid(0, tabPos) + L"\t" + keyCodeStr);
	item.Enable(enable);
}

void MainEmuFrame::enableRecordingMenuItem(MenuIdentifiers menuId, bool enable)
{
	wxMenuItem& item = *m_menuRecording.FindChildItem(menuId);
	item.Enable(enable);
}
