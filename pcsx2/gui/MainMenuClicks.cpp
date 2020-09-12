/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

#include "App.h"
#include "CDVD/CDVD.h"
#include "GS.h"
#include "GSFrame.h"

#include "ConsoleLogger.h"
#include "MainFrame.h"
#include "IsoDropTarget.h"

#include "Dialogs/ModalPopups.h"
#include "Dialogs/ConfigurationDialog.h"
#include "Debugger/DisassemblyDialog.h"

#include "Utilities/IniInterface.h"

#ifndef DISABLE_RECORDING
#	include "Recording/InputRecording.h"
#	include "Recording/InputRecordingControls.h"
#	include "Recording/VirtualPad.h"
#endif


using namespace Dialogs;

void MainEmuFrame::Menu_SysSettings_Click(wxCommandEvent &event)
{
	AppOpenDialog<SysConfigDialog>( this );
}

void MainEmuFrame::Menu_McdSettings_Click(wxCommandEvent &event)
{
	ScopedCoreThreadClose closed_core;
	closed_core.AllowResume();
	AppOpenModalDialog<McdConfigDialog>(wxEmptyString, this);
}

void MainEmuFrame::Menu_WindowSettings_Click(wxCommandEvent &event)
{
	wxCommandEvent evt( pxEvt_SetSettingsPage );
	evt.SetString( L"GS Window" );
	AppOpenDialog<SysConfigDialog>( this )->GetEventHandler()->ProcessEvent( evt );
}

void MainEmuFrame::Menu_GSSettings_Click(wxCommandEvent &event)
{
	wxCommandEvent evt( pxEvt_SetSettingsPage );
	evt.SetString( L"GS" );
	AppOpenDialog<SysConfigDialog>( this )->GetEventHandler()->ProcessEvent( evt );
}

void MainEmuFrame::Menu_SelectPluginsBios_Click(wxCommandEvent &event)
{
	AppOpenDialog<ComponentsConfigDialog>( this );
}

void MainEmuFrame::Menu_ChangeLang(wxCommandEvent &event) // Always in English
{
	AppOpenDialog<InterfaceLanguageDialog>(this);
}

static void WipeSettings()
{
	wxGetApp().CleanupRestartable();
	wxGetApp().CleanupResources();

	wxRemoveFile( GetUiSettingsFilename() );
	wxRemoveFile( GetVmSettingsFilename() );

	// FIXME: wxRmdir doesn't seem to work here for some reason (possible file sharing issue
	// with a plugin that leaves a file handle dangling maybe?).  But deleting the inis folder
	// manually from explorer does work.  Can't think of a good work-around at the moment. --air

	//wxRmdir( GetSettingsFolder().ToString() );

	wxGetApp().GetRecentIsoManager().Clear();
	g_Conf = std::unique_ptr<AppConfig>(new AppConfig());
	sMainFrame.RemoveCdvdMenu();

	sApp.WipeUserModeSettings();
}

void MainEmuFrame::RemoveCdvdMenu()
{
	// Delete() keeps the sub menu and delete the menu item.
	// Remove() does not delete the menu item.
	if (m_menuItem_RecentIsoMenu)
		m_menuCDVD.Delete(m_menuItem_RecentIsoMenu);
	m_menuItem_RecentIsoMenu = nullptr;
}

void MainEmuFrame::Menu_ResetAllSettings_Click(wxCommandEvent &event)
{
	if( IsBeingDeleted() || m_RestartEmuOnDelete ) return;

	{
		ScopedCoreThreadPopup suspender;
		if( !Msgbox::OkCancel( pxsFmt(
			pxE( L"This command clears %s settings and allows you to re-run the First-Time Wizard.  You will need to manually restart %s after this operation.\n\nWARNING!!  Click OK to delete *ALL* settings for %s and force-close the app, losing any current emulation progress.  Are you absolutely sure?\n\n(note: settings for plugins are unaffected)"
			), WX_STR(pxGetAppName()), WX_STR(pxGetAppName()), WX_STR(pxGetAppName()) ),
			_("Reset all settings?") ) )
		{
			suspender.AllowResume();
			return;
		}
	}

	WipeSettings();
	wxGetApp().PostMenuAction( MenuId_Exit );
}

// Return values:
//   wxID_CANCEL - User canceled the action outright.
//   wxID_RESET  - User wants to reset the emu in addition to swap discs
//   (anything else) - Standard swap, no reset.  (hotswap!)
wxWindowID SwapOrReset_Iso( wxWindow* owner, IScopedCoreThread& core_control, const wxString& isoFilename, const wxString& descpart1 )
{
	wxWindowID result = wxID_CANCEL;

	if( (g_Conf->CdvdSource == CDVD_SourceType::Iso) && (isoFilename == g_Conf->CurrentIso) )
	{
		core_control.AllowResume();
		return result;
	}

	if( SysHasValidState() )
	{
		core_control.DisallowResume();
		wxDialogWithHelpers dialog( owner, _("Confirm ISO image change") );

		dialog += dialog.Heading(descpart1);
		dialog += dialog.GetCharHeight();
		dialog += dialog.Text(isoFilename);
		dialog += dialog.GetCharHeight();
		dialog += dialog.Heading(_("Do you want to swap discs or boot the new image (via system reset)?"));

		result = pxIssueConfirmation( dialog, MsgButtons().Reset().Cancel().Custom(_("Swap Disc"), "swap"));
		if( result == wxID_CANCEL )
		{
			core_control.AllowResume();
			return result;
		}
	}

	g_Conf->CdvdSource = CDVD_SourceType::Iso;
	SysUpdateIsoSrcFile( isoFilename );

	if( result == wxID_RESET )
	{
		core_control.DisallowResume();
		sApp.SysExecute(CDVD_SourceType::Iso);
	}
	else
	{
		Console.Indent().WriteLn( "HotSwapping to new ISO src image!" );
		//g_Conf->CdvdSource = CDVDsrc_Iso;
		//CoreThread.ChangeCdvdSource();
		core_control.AllowResume();
	}

	return result;
}

// Return values:
//   wxID_CANCEL - User canceled the action outright.
//   wxID_RESET  - User wants to reset the emu in addition to swap discs
//   (anything else) - Standard swap, no reset.  (hotswap!)
wxWindowID SwapOrReset_Disc( wxWindow* owner, IScopedCoreThread& core, const wxString driveLetter)
{
	wxWindowID result = wxID_CANCEL;

	if ((g_Conf->CdvdSource == CDVD_SourceType::Disc) && (driveLetter == g_Conf->Folders.RunDisc.GetPath()))
	{
		core.AllowResume();
		return result;
	}

	if (SysHasValidState())
	{
		core.DisallowResume();
		wxDialogWithHelpers dialog(owner, _("Confirm disc change"));

		dialog += dialog.Heading("New drive selected: " + driveLetter);
		dialog += dialog.GetCharHeight();
		dialog += dialog.Heading(_("Do you want to swap discs or boot the new disc (via system reset)?"));

		result = pxIssueConfirmation(dialog, MsgButtons().Reset().Cancel().Custom(_("Swap Disc"), "swap"));
		if (result == wxID_CANCEL)
		{
			core.AllowResume();
			return result;
		}
	}

	g_Conf->CdvdSource = CDVD_SourceType::Disc;
	SysUpdateDiscSrcDrive(driveLetter);
	if (result == wxID_RESET)
	{
		core.DisallowResume();
		sApp.SysExecute(CDVD_SourceType::Disc);
	}
	else
	{
		Console.Indent().WriteLn("Hot swapping to new disc!");
		core.AllowResume();
	}

	return result;
}

wxWindowID SwapOrReset_CdvdSrc( wxWindow* owner, CDVD_SourceType newsrc )
{
	if(newsrc == g_Conf->CdvdSource) return wxID_CANCEL;
	wxWindowID result = wxID_CANCEL;
	ScopedCoreThreadPopup core;

	if( SysHasValidState() )
	{
		wxDialogWithHelpers dialog( owner, _("Confirm CDVD source change") );

		wxString changeMsg;
		changeMsg.Printf(_("You've selected to switch the CDVD source from %s to %s."),
			CDVD_SourceLabels[enum_cast(g_Conf->CdvdSource)], CDVD_SourceLabels[enum_cast(newsrc)] );

		dialog += dialog.Heading(changeMsg + L"\n\n" +
			_("Do you want to swap discs or boot the new image (system reset)?")
		);

		result = pxIssueConfirmation( dialog, MsgButtons().Reset().Cancel().Custom(_("Swap Disc"), "swap"));

		if( result == wxID_CANCEL )
		{
			core.AllowResume();
			sMainFrame.UpdateCdvdSrcSelection();
			return result;
		}
	}

	CDVD_SourceType oldsrc = g_Conf->CdvdSource;
	g_Conf->CdvdSource = newsrc;

	if( result != wxID_RESET )
	{
		Console.Indent().WriteLn(L"(CdvdSource) HotSwapping CDVD source types from %s to %s.",
			WX_STR(wxString(CDVD_SourceLabels[enum_cast(oldsrc)])),
			WX_STR(wxString(CDVD_SourceLabels[enum_cast(newsrc)])));
		//CoreThread.ChangeCdvdSource();
		sMainFrame.UpdateCdvdSrcSelection();
		core.AllowResume();
	}
	else
	{
		core.DisallowResume();
		sApp.SysExecute( g_Conf->CdvdSource );
	}

	return result;
}

static wxString JoinFiletypes( const wxChar** src )
{
	wxString dest;
	while( *src != NULL )
	{
		if( *src[0] == 0 ) continue;
		if( !dest.IsEmpty() )
			dest += L";";

		dest += pxsFmt(L"*.%ls", *src);

		if (wxFileName::IsCaseSensitive())
		{
			// omgosh!  the filesystem is CaSE SeNSiTiVE!!
			dest += pxsFmt(L";*.%ls", *src).ToUpper();
		}

		++src;
	}
	
	return dest;
}

// Returns FALSE if the user canceled the action.
bool MainEmuFrame::_DoSelectIsoBrowser( wxString& result )
{
	static const wxChar* isoSupportedTypes[] =
	{
		L"iso", L"mdf", L"nrg", L"bin", L"img", NULL
	};

	const wxString isoSupportedLabel( JoinString(isoSupportedTypes, L" ") );
	const wxString isoSupportedList( JoinFiletypes(isoSupportedTypes) );
	
	wxArrayString isoFilterTypes;

	isoFilterTypes.Add(pxsFmt(_("All Supported (%s)"), WX_STR((isoSupportedLabel + L" .dump" + L" .gz" + L" .cso"))));
	isoFilterTypes.Add(isoSupportedList + L";*.dump" + L";*.gz" + L";*.cso");

	isoFilterTypes.Add(pxsFmt(_("Disc Images (%s)"), WX_STR(isoSupportedLabel) ));
	isoFilterTypes.Add(isoSupportedList);

	isoFilterTypes.Add(pxsFmt(_("Blockdumps (%s)"), L".dump" ));
	isoFilterTypes.Add(L"*.dump");

	isoFilterTypes.Add(pxsFmt(_("Compressed (%s)"), L".gz .cso"));
	isoFilterTypes.Add(L"*.gz;*.cso");

	isoFilterTypes.Add(_("All Files (*.*)"));
	isoFilterTypes.Add(L"*.*");
	
	wxFileDialog ctrl( this, _("Select disc image, compressed disc image, or block-dump..."), g_Conf->Folders.RunIso.ToString(), wxEmptyString,
		JoinString(isoFilterTypes, L"|"), wxFD_OPEN | wxFD_FILE_MUST_EXIST );

	if( ctrl.ShowModal() != wxID_CANCEL )
	{
		result = ctrl.GetPath();
		g_Conf->Folders.RunIso = wxFileName( result ).GetPath();
		return true;
	}

	return false;
}

bool MainEmuFrame::_DoSelectELFBrowser()
{
	static const wxChar* elfFilterType = L"ELF Files (.elf)|*.elf;*.ELF";

	wxFileDialog ctrl( this, _("Select ELF file..."), g_Conf->Folders.RunELF.ToString(), wxEmptyString,
		(wxString)elfFilterType + L"|" + _("All Files (*.*)") + L"|*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST );

	if( ctrl.ShowModal() != wxID_CANCEL )
	{
		g_Conf->Folders.RunELF = wxFileName( ctrl.GetPath() ).GetPath();
		g_Conf->CurrentELF = ctrl.GetPath();
		return true;
	}

	return false;
}

void MainEmuFrame::_DoBootCdvd()
{
	ScopedCoreThreadPause paused_core;

	if( g_Conf->CdvdSource == CDVD_SourceType::Iso )
	{
		bool selector = g_Conf->CurrentIso.IsEmpty();

		if( !selector && !wxFileExists(g_Conf->CurrentIso) )
		{
			// User has an iso selected from a previous run, but it doesn't exist anymore.
			// Issue a courtesy popup and then an Iso Selector to choose a new one.

			wxDialogWithHelpers dialog( this, _("ISO file not found!") );
			dialog += dialog.Heading(
				_("An error occurred while trying to open the file:") + wxString(L"\n\n") + g_Conf->CurrentIso + L"\n\n" +
				_("Error: The configured ISO file does not exist.  Click OK to select a new ISO source for CDVD.")
			);

			pxIssueConfirmation( dialog, MsgButtons().OK() );

			selector = true;
		}

		if( selector || g_Conf->AskOnBoot)
		{
			wxString result;
			if( !_DoSelectIsoBrowser( result ) )
			{
				paused_core.AllowResume();
				return;
			}

			SysUpdateIsoSrcFile( result );
		}
	}

	if( SysHasValidState() )
	{
		wxDialogWithHelpers dialog( this, _("Confirm PS2 Reset") );
		dialog += dialog.Heading( GetMsg_ConfirmSysReset() );
		bool confirmed = (pxIssueConfirmation( dialog, MsgButtons().Yes().Cancel(), L"BootCdvd.ConfirmReset" ) != wxID_CANCEL);

		if( !confirmed )
		{
			paused_core.AllowResume();
			return;
		}
	}

	sApp.SysExecute( g_Conf->CdvdSource );
}

void MainEmuFrame::Menu_CdvdSource_Click( wxCommandEvent &event )
{
	CDVD_SourceType newsrc = CDVD_SourceType::NoDisc;

	switch( event.GetId() )
	{
		case MenuId_Src_Iso:	newsrc = CDVD_SourceType::Iso;		break;
		case MenuId_Src_Disc: newsrc = CDVD_SourceType::Disc;		break;
		case MenuId_Src_NoDisc: newsrc = CDVD_SourceType::NoDisc;	break;
		jNO_DEFAULT
	}

	SwapOrReset_CdvdSrc(this, newsrc);
}

void MainEmuFrame::Menu_BootCdvd_Click( wxCommandEvent &event )
{
	g_Conf->EmuOptions.UseBOOT2Injection = g_Conf->EnableFastBoot;
	_DoBootCdvd();
}

void MainEmuFrame::Menu_FastBoot_Click( wxCommandEvent &event )
{
	g_Conf->EnableFastBoot = GetMenuBar()->IsChecked( MenuId_Config_FastBoot );
	AppApplySettings();
	AppSaveSettings();
	UpdateStatusBar();
}

wxString GetMsg_IsoImageChanged()
{
	return _("You have selected the following ISO image into PCSX2:\n\n");
}

void MainEmuFrame::Menu_IsoBrowse_Click( wxCommandEvent &event )
{
	ScopedCoreThreadPopup core;
	wxString isofile;

	if( !_DoSelectIsoBrowser(isofile) )
	{
		core.AllowResume();
		return;
	}
	
	SwapOrReset_Iso(this, core, isofile, GetMsg_IsoImageChanged());
	AppSaveSettings();		// save the new iso selection; update menus!
}

void MainEmuFrame::Menu_IsoClear_Click(wxCommandEvent &event)
{
	wxDialogWithHelpers dialog(this, _("Confirm clearing ISO list"));
	dialog += dialog.Heading(_("This will clear the ISO list. If an ISO is running it will remain in the list. Continue?"));

	bool confirmed = pxIssueConfirmation(dialog, MsgButtons().YesNo()) == wxID_YES;

	if (confirmed)
	{
		// If the CDVD mode is not ISO, or the system isn't running, wipe the CurrentIso field in INI file
		if (g_Conf->CdvdSource != CDVD_SourceType::Iso || !SysHasValidState())
			SysUpdateIsoSrcFile("");
		wxGetApp().GetRecentIsoManager().Clear();
		AppSaveSettings();
	}
}

void MainEmuFrame::Menu_Ask_On_Boot_Click(wxCommandEvent &event)
{
	g_Conf->AskOnBoot = event.IsChecked();

	if (SysHasValidState())
		return;

	wxGetApp().GetRecentIsoManager().EnableItems(!event.IsChecked());
	FindItemInMenuBar(MenuId_IsoBrowse)->Enable(!event.IsChecked());
}

void MainEmuFrame::Menu_Debug_CreateBlockdump_Click(wxCommandEvent &event)
{
	g_Conf->EmuOptions.CdvdDumpBlocks = event.IsChecked();
	if (g_Conf->EmuOptions.CdvdDumpBlocks && SysHasValidState())
		Console.Warning("VM must be rebooted to create a useful block dump.");

	AppSaveSettings();
}

void MainEmuFrame::Menu_MultitapToggle_Click( wxCommandEvent& )
{
	g_Conf->EmuOptions.MultitapPort0_Enabled = GetMenuBar()->IsChecked( MenuId_Config_Multitap0Toggle );
	g_Conf->EmuOptions.MultitapPort1_Enabled = GetMenuBar()->IsChecked( MenuId_Config_Multitap1Toggle );
	AppApplySettings();
	AppSaveSettings();

	//evt.Skip();
}

void MainEmuFrame::Menu_EnableBackupStates_Click( wxCommandEvent& )
{
	g_Conf->EmuOptions.BackupSavestate = GetMenuBar()->IsChecked( MenuId_EnableBackupStates );
	
	//without the next line, after toggling this menu-checkbox, the change only applies from the 2nd save and onwards
	//  (1st save after the toggle keeps the old pre-toggle value)..
	//  wonder what that means for all the other menu checkboxes which only use AppSaveSettings... (avih)
	AppApplySettings();
	AppSaveSettings();
}

void MainEmuFrame::Menu_EnablePatches_Click( wxCommandEvent& )
{
	g_Conf->EmuOptions.EnablePatches = GetMenuBar()->IsChecked( MenuId_EnablePatches );
	AppApplySettings();
	AppSaveSettings();
}

void MainEmuFrame::Menu_EnableCheats_Click( wxCommandEvent& )
{
	g_Conf->EmuOptions.EnableCheats  = GetMenuBar()->IsChecked( MenuId_EnableCheats );
	AppApplySettings();
	AppSaveSettings();
}

void MainEmuFrame::Menu_EnableIPC_Click( wxCommandEvent& )
{
	g_Conf->EmuOptions.EnableIPC  = GetMenuBar()->IsChecked( MenuId_EnableIPC );
	AppApplySettings();
	AppSaveSettings();
}

void MainEmuFrame::Menu_EnableWideScreenPatches_Click( wxCommandEvent& )
{
	g_Conf->EmuOptions.EnableWideScreenPatches  = GetMenuBar()->IsChecked( MenuId_EnableWideScreenPatches );
	AppApplySettings();
	AppSaveSettings();
}

#ifndef DISABLE_RECORDING
void MainEmuFrame::Menu_EnableRecordingTools_Click(wxCommandEvent& event)
{
	bool checked = GetMenuBar()->IsChecked(MenuId_EnableInputRecording);
	// Confirm with User
	if (checked)
	{
		if (!Msgbox::OkCancel(_("Please be aware that PCSX2's input recording features are still very much a work-in-progress.\n"
			"As a result, there may be unforeseen bugs, performance implications and instability with certain games.\n\n"
			"These tools are provided as-is and should be enabled under your own discretion."), "Enabling Input Recording"))
		{
			checked = false;
			m_GameSettingsSubmenu.FindChildItem(MenuId_EnableInputRecording)->Check(false);
		}
	}

	// If still enabled, add the menu item, else, remove it
	if (checked)
	{
		GetMenuBar()->Insert(TopLevelMenu_InputRecording, &m_menuRecording, _("&Input Record"));
		// Enable Recording Keybindings
		if (GSFrame* gsFrame = wxGetApp().GetGsFramePtr())
		{
			if (GSPanel* viewport = gsFrame->GetViewport())
			{
				viewport->InitRecordingAccelerators();
			}
		}
	}
	else
	{
		//Properly close any currently loaded recording file before disabling
		if (g_InputRecording.IsActive())
			Menu_Recording_Stop_Click(event);
		GetMenuBar()->Remove(TopLevelMenu_InputRecording);
		// Always turn controller logs off, but never turn it on by default
		SysConsole.controlInfo.Enabled = checked;
		// Return Keybindings Back to Normal
		if (GSFrame* gsFrame = wxGetApp().GetGsFramePtr())
		{
			if (GSPanel* viewport = gsFrame->GetViewport())
			{
				viewport->InitDefaultAccelerators();
			}
		}
		if (g_InputRecordingControls.IsPaused())
			g_InputRecordingControls.Resume();
	}

	g_Conf->EmuOptions.EnableRecordingTools = checked;
	SysConsole.recordingConsole.Enabled = checked;
	// Enable Recording Logs
	ConsoleLogFrame* progLog = wxGetApp().GetProgramLog();
	progLog->UpdateLogList();
	AppApplySettings();
	AppSaveSettings();
}
#endif

void MainEmuFrame::Menu_EnableHostFs_Click( wxCommandEvent& )
{
	g_Conf->EmuOptions.HostFs = GetMenuBar()->IsChecked( MenuId_EnableHostFs );
	AppSaveSettings();
}

void MainEmuFrame::Menu_OpenELF_Click(wxCommandEvent&)
{
	ScopedCoreThreadClose stopped_core;
	if( _DoSelectELFBrowser() )
	{
		g_Conf->EmuOptions.UseBOOT2Injection = true;
		sApp.SysExecute( g_Conf->CdvdSource, g_Conf->CurrentELF );
	}

	stopped_core.AllowResume();
}

void MainEmuFrame::Menu_LoadStates_Click(wxCommandEvent &event)
{
	if( event.GetId() == MenuId_State_LoadBackup )
	{
		States_DefrostCurrentSlotBackup();
		return;
	}

	States_SetCurrentSlot( event.GetId() - MenuId_State_Load01 - 1 );
	States_DefrostCurrentSlot();
}

void MainEmuFrame::Menu_SaveStates_Click(wxCommandEvent &event)
{
	States_SetCurrentSlot( event.GetId() - MenuId_State_Save01 - 1 );
	States_FreezeCurrentSlot();
}

void MainEmuFrame::Menu_LoadStateFromFile_Click(wxCommandEvent &event)
{
	wxFileDialog loadStateDialog(this, _("Load State"), L"", L"",
		L"Savestate files (*.p2s)|*.p2s", wxFD_OPEN);

	if (loadStateDialog.ShowModal() == wxID_CANCEL)
	{
		return;
	}

	wxString path = loadStateDialog.GetPath();
	StateCopy_LoadFromFile(path);
}

void MainEmuFrame::Menu_SaveStateToFile_Click(wxCommandEvent &event)
{
	wxFileDialog saveStateDialog(this, _("Save State"), L"", L"",
		L"Savestate files (*.p2s)|*.p2s", wxFD_OPEN);

	if (saveStateDialog.ShowModal() == wxID_CANCEL)
	{
		return;
	}

	wxString path = saveStateDialog.GetPath();
	StateCopy_SaveToFile(path);
}

void MainEmuFrame::Menu_Exit_Click(wxCommandEvent &event)
{
	Close();
}

class SysExecEvent_ToggleSuspend : public SysExecEvent
{
public:
	virtual ~SysExecEvent_ToggleSuspend() = default;

	wxString GetEventName() const { return L"ToggleSuspendResume"; }

protected:
	void InvokeEvent()
	{
		if( CoreThread.IsOpen() )
			CoreThread.Suspend();
		else
			CoreThread.Resume();
	}
};

void MainEmuFrame::Menu_SuspendResume_Click(wxCommandEvent &event)
{
	if( !SysHasValidState() ) return;

	// Disable the menu item.  The state of the menu is indeterminate until the core thread
	// has responded (it updates status after the plugins are loaded and emulation has
	// engaged successfully).

	EnableMenuItem( MenuId_Sys_SuspendResume, false );
	GetSysExecutorThread().PostEvent( new SysExecEvent_ToggleSuspend() );
}

void MainEmuFrame::Menu_SysShutdown_Click(wxCommandEvent &event)
{
	UI_DisableSysShutdown();
	Console.SetTitle("PCSX2 Program Log");
	CoreThread.Reset();
}

void MainEmuFrame::Menu_ConfigPlugin_Click(wxCommandEvent& event)
{
	const int eventId = event.GetId() - MenuId_PluginBase_Settings;

	PluginsEnum_t pid = (PluginsEnum_t)(eventId / PluginMenuId_Interval);

	// Don't try to call the Patches config dialog until we write one.
	if (event.GetId() == MenuId_Config_Patches)
		return;

	if (!pxAssertDev((eventId >= 0) || (pid < PluginId_Count), "Invalid plugin identifier passed to ConfigPlugin event handler."))
		return;

	wxWindowDisabler disabler;
	ScopedCoreThreadPause paused_core(new SysExecEvent_SaveSinglePlugin(pid));

#ifndef DISABLE_RECORDING
	if (pid == PluginId_PAD)
	{
		// The recording features involve pausing emulation, and can be resumed ideally via key-bindings.
		//
		// However, since the PAD plugin is used to do so, if it's closed then there is nothing to read
		// the keybind resulting producing an unrecovable state.
		//
		// If the CoreThread is paused prior to opening the PAD plugin settings then when the settings 
		// are closed the PAD will not re-open. To avoid this, we resume emulation prior to the plugins 
		// configuration handler doing so.
		if (g_Conf->EmuOptions.EnableRecordingTools && g_InputRecordingControls.IsPaused())
		{
			g_InputRecordingControls.Resume();
			GetCorePlugins().Configure(pid);
			g_InputRecordingControls.Pause();
		}
		else
			GetCorePlugins().Configure(pid);
	}
	else
		GetCorePlugins().Configure(pid);
#else
	GetCorePlugins().Configure(pid);
#endif
}

void MainEmuFrame::Menu_Debug_Open_Click(wxCommandEvent &event)
{
	DisassemblyDialog* dlg = wxGetApp().GetDisassemblyPtr();
	if (dlg)
	{
		if (event.IsChecked())
			dlg->Show();
		else
			dlg->Hide();
	}
}

void MainEmuFrame::Menu_Debug_MemoryDump_Click(wxCommandEvent &event)
{
}

void MainEmuFrame::Menu_ShowConsole(wxCommandEvent &event)
{
	// Use messages to relay open/close commands (thread-safe)

	g_Conf->ProgLogBox.Visible = event.IsChecked();
	wxCommandEvent evt( wxEVT_MENU, g_Conf->ProgLogBox.Visible ? wxID_OPEN : wxID_CLOSE );
	wxGetApp().ProgramLog_PostEvent( evt );
}

void MainEmuFrame::Menu_ShowConsole_Stdio(wxCommandEvent &event)
{
	g_Conf->EmuOptions.ConsoleToStdio = GetMenuBar()->IsChecked( MenuId_Console_Stdio );
	AppSaveSettings();
}

void MainEmuFrame::Menu_GetStarted(wxCommandEvent &event)
{
	wxLaunchDefaultBrowser("https://pcsx2.net/getting-started.html");
}

void MainEmuFrame::Menu_Compatibility(wxCommandEvent &event)
{
	wxLaunchDefaultBrowser("https://pcsx2.net/compatibility-list.html");
}

void MainEmuFrame::Menu_Forums(wxCommandEvent &event)
{
	wxLaunchDefaultBrowser("https://forums.pcsx2.net/");
}

void MainEmuFrame::Menu_Website(wxCommandEvent &event)
{
	wxLaunchDefaultBrowser("https://pcsx2.net/");
}

void MainEmuFrame::Menu_Github(wxCommandEvent &event)
{
	wxLaunchDefaultBrowser("https://github.com/PCSX2/pcsx2");
}

void MainEmuFrame::Menu_Wiki(wxCommandEvent &event)
{
	wxLaunchDefaultBrowser("https://wiki.pcsx2.net/Main_Page");
}

void MainEmuFrame::Menu_ShowAboutBox(wxCommandEvent &event)
{
	AppOpenDialog<AboutBoxDialog>( this );
}

void MainEmuFrame::Menu_Capture_Video_Record_Click(wxCommandEvent &event)
{
	ScopedCoreThreadPause paused_core;
	paused_core.AllowResume();

	m_capturingVideo = true;
	VideoCaptureUpdate();
}

void MainEmuFrame::Menu_Capture_Video_Stop_Click(wxCommandEvent &event)
{
	ScopedCoreThreadPause paused_core;
	paused_core.AllowResume();

	m_capturingVideo = false;
	VideoCaptureUpdate();
}

void MainEmuFrame::VideoCaptureUpdate()
{
	GetMTGS().WaitGS();		// make sure GS is in sync with the audio stream when we start.
	if (m_capturingVideo)
	{
		// start recording

		// make the recording setup dialog[s] pseudo-modal also for the main PCSX2 window
		// (the GSdx dialog is already properly modal for the GS window)
		bool needsMainFrameEnable = false;
		if (GetMainFramePtr() && GetMainFramePtr()->IsEnabled())
		{
			needsMainFrameEnable = true;
			GetMainFramePtr()->Disable();
		}

		if (GSsetupRecording)
		{
			// GSsetupRecording can be aborted/canceled by the user. Don't go on to record the audio if that happens
			std::wstring* filename = nullptr;
			if (filename = GSsetupRecording(m_capturingVideo))
			{
				if (SPU2setupRecording)
					SPU2setupRecording(m_capturingVideo, filename);
				delete filename;
			}
			else
			{
				// recording dialog canceled by the user. align our state
				m_capturingVideo = false;
			}
		}
		else
		{
			// the GS doesn't support recording.
			if (SPU2setupRecording)
			{
				SPU2setupRecording(m_capturingVideo, nullptr);
			}
		}

		if (GetMainFramePtr() && needsMainFrameEnable)
			GetMainFramePtr()->Enable();

	}
	else
	{
		// stop recording
		if (GSsetupRecording)
			GSsetupRecording(m_capturingVideo);
		if (SPU2setupRecording)
			SPU2setupRecording(m_capturingVideo, nullptr);
	}

	if (m_capturingVideo)
	{
		m_submenuVideoCapture.FindItem(MenuId_Capture_Video_Record)->Enable(false);
		m_submenuVideoCapture.FindItem(MenuId_Capture_Video_Stop)->Enable(true);
	}
	else
	{
		m_submenuVideoCapture.FindItem(MenuId_Capture_Video_Record)->Enable(true);
		m_submenuVideoCapture.FindItem(MenuId_Capture_Video_Stop)->Enable(false);
	}
}

void MainEmuFrame::Menu_Capture_Screenshot_Screenshot_Click(wxCommandEvent & event)
{
	if (!CoreThread.IsOpen())
	{
		return;
	}
	GSmakeSnapshot(g_Conf->Folders.Snapshots.ToAscii());
}

#ifndef DISABLE_RECORDING
void MainEmuFrame::Menu_Recording_New_Click(wxCommandEvent &event)
{
	const bool initiallyPaused = g_InputRecordingControls.IsPaused();
	if (!initiallyPaused)
		g_InputRecordingControls.PauseImmediately();
	NewRecordingFrame* newRecordingFrame = wxGetApp().GetNewRecordingFramePtr();
	if (newRecordingFrame)
	{
		if (newRecordingFrame->ShowModal() == wxID_CANCEL)
		{
			if (!initiallyPaused)
				g_InputRecordingControls.Resume();
			return;
		}
		if (!g_InputRecording.Create(newRecordingFrame->GetFile(), !newRecordingFrame->GetFrom(), newRecordingFrame->GetAuthor()))
		{
			if (!initiallyPaused)
				g_InputRecordingControls.Resume();
			return;
		}
	}
	m_menuRecording.FindChildItem(MenuId_Recording_New)->Enable(false);
	m_menuRecording.FindChildItem(MenuId_Recording_Stop)->Enable(true);
}

void MainEmuFrame::Menu_Recording_Play_Click(wxCommandEvent &event)
{
	const bool initiallyPaused = g_InputRecordingControls.IsPaused();
	if (!initiallyPaused)
		g_InputRecordingControls.PauseImmediately();
	wxFileDialog openFileDialog(this, _("Select P2M2 record file."), L"", L"",
		L"p2m2 file(*.p2m2)|*.p2m2", wxFD_OPEN);
	if (openFileDialog.ShowModal() == wxID_CANCEL)
	{
		if (!initiallyPaused)
			g_InputRecordingControls.Resume();
		return;
	}

	wxString path = openFileDialog.GetPath();
	const bool recordingLoaded = g_InputRecording.IsActive();
	if (!g_InputRecording.Play(path))
	{
		if (recordingLoaded)
			Menu_Recording_Stop_Click(event);
		if (!initiallyPaused)
			g_InputRecordingControls.Resume();
		return;
	}
	if (!recordingLoaded)
	{
		m_menuRecording.FindChildItem(MenuId_Recording_New)->Enable(false);
		m_menuRecording.FindChildItem(MenuId_Recording_Stop)->Enable(true);
	}
}

void MainEmuFrame::Menu_Recording_Stop_Click(wxCommandEvent &event)
{
	g_InputRecording.Stop();
	m_menuRecording.FindChildItem(MenuId_Recording_New)->Enable(true);
	m_menuRecording.FindChildItem(MenuId_Recording_Stop)->Enable(false);
}

void MainEmuFrame::Menu_Recording_VirtualPad_Open_Click(wxCommandEvent &event)
{
	wxGetApp().GetVirtualPadPtr(event.GetId() - MenuId_Recording_VirtualPad_Port0)->Show();
}
#endif
