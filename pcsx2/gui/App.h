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

#pragma once

#include "wxAppWithHelpers.h"
#include "common/WindowInfo.h"

#include <wx/fileconf.h>
#include <wx/apptrait.h>
#include <memory>

#include "SysThreads.h"
#include "pxEventThread.h"

#include "AppCommon.h"
#include "AppCoreThread.h"
#include "RecentIsoList.h"
#include "DriveList.h"

#include "Recording/NewRecordingFrame.h"

class DisassemblyDialog;
struct HostKeyEvent;

#include "GS.h"
#include "System.h"

// TODO: Not the best location for this, but it needs to be accessed by MTGS etc.
extern WindowInfo g_gs_window_info;

typedef void FnType_OnThreadComplete(const wxCommandEvent& evt);
typedef void (Pcsx2App::*FnPtr_Pcsx2App)();

wxDECLARE_EVENT(pxEvt_SetSettingsPage, wxCommandEvent);

// This is used when GS is handling its own window.  Messages from the PAD
// are piped through to an app-level message handler, which dispatches them through
// the universal Accelerator table.
static const int pxID_PadHandler_Keydown = 8030;

// ID and return code used for modal popups that have a custom button.
static const wxWindowID pxID_CUSTOM = wxID_LOWEST - 1;

// Return code used by first time wizard if the dialog needs to be automatically recreated
// (assigned an arbitrary value)
static const wxWindowID pxID_RestartWizard = wxID_LOWEST - 100;


// ------------------------------------------------------------------------
// All Menu Options for the Main Window! :D
// ------------------------------------------------------------------------

enum TopLevelMenuIndices
{
	TopLevelMenu_Pcsx2 = 0,
	TopLevelMenu_Cdvd,
	TopLevelMenu_Config,
	TopLevelMenu_Window,
	TopLevelMenu_Capture,
	TopLevelMenu_InputRecording,
	TopLevelMenu_Help
};

enum MenuIdentifiers
{
	// Main Menu Section
	MenuId_Boot = 1,
	MenuId_Emulation,
	MenuId_Config, // General config.
	MenuId_Video,  // Video options filled in by GS.
	MenuId_Audio,  // audio options filled in by SPU2.
	MenuId_Misc,   // Misc options and help!

	MenuId_Exit = wxID_EXIT,
	MenuId_About = wxID_ABOUT,

	MenuId_EndTopLevel = 20,

	// Run SubSection
	MenuId_Cdvd_Source,
	MenuId_Src_Iso,
	MenuId_Src_Disc,
	MenuId_Src_NoDisc,
	MenuId_Boot_Iso, // Opens submenu with Iso browser, and recent isos.
	MenuId_RecentIsos_reservedStart,
	MenuId_IsoBrowse = MenuId_RecentIsos_reservedStart + 100, // Open dialog, runs selected iso.
	MenuId_IsoClear,
	MenuId_IsoClearMissing,
	MenuId_DriveSelector,
	MenuId_DriveListRefresh,
	MenuId_Ask_On_Booting,
	MenuId_Boot_CDVD,
	MenuId_Boot_CDVD2,
	MenuId_Boot_ELF,
	//MenuId_Boot_Recent,			// Menu populated with recent source bootings


	MenuId_Sys_SuspendResume,  // suspends/resumes active emulation
	MenuId_Sys_Shutdown,       // Closes virtual machine, wipes states
	MenuId_Sys_LoadStates,     // Opens load states submenu
	MenuId_Sys_SaveStates,     // Opens save states submenu
	MenuId_EnableBackupStates, // Checkbox to enable/disables savestates backup
	MenuId_GameSettingsSubMenu,
	MenuId_EnablePatches,
	MenuId_EnableCheats,
	MenuId_GSDump,
	MenuId_EnableWideScreenPatches,
	MenuId_EnableInputRecording,
	MenuId_EnableLuaTools,
	MenuId_EnableHostFs,

	MenuId_State_Load,
	MenuId_State_LoadFromFile,
	MenuId_State_Load01, // first of many load slots
	MenuId_State_LoadBackup = MenuId_State_Load01 + 20,
	MenuId_State_Save,
	MenuId_State_SaveToFile,
	MenuId_State_Save01, // first of many save slots

	MenuId_State_EndSlotSection = MenuId_State_Save01 + 20,

	// Config Subsection
	MenuId_Config_SysSettings,
	MenuId_Config_McdSettings,
	MenuId_Config_AppSettings,
	MenuId_Config_Language,

	MenuId_Config_GS,
	MenuId_Config_PAD,
	MenuId_Config_SPU2,
	MenuId_Config_CDVD,
	MenuId_Config_USB,
	MenuId_Config_FireWire,
	MenuId_Config_DEV9,
	MenuId_Config_Patches,

	MenuId_Config_Multitap0Toggle,
	MenuId_Config_Multitap1Toggle,
	MenuId_Config_FastBoot,

	MenuId_Help_GetStarted,
	MenuId_Help_Compatibility,
	MenuId_Help_Forums,
	MenuId_Help_Website,
	MenuId_Help_Wiki,
	MenuId_Help_Github,

	MenuId_Video_CoreSettings = 0x200, // includes frame timings and skippings settings
	MenuId_Video_WindowSettings,

	// Miscellaneous Menu!  (Misc)
	MenuId_Console,       // Enable console
	MenuId_ChangeLang,    // Change language (resets first time wizard to show on next start)
	MenuId_Console_Stdio, // Enable Stdio

	// Debug Subsection
	MenuId_Debug_Open, // opens the debugger window / starts a debug session
	MenuId_Debug_MemoryDump,
	MenuId_Debug_Logging, // dialog for selection additional log options
	MenuId_Debug_CreateBlockdump,
	MenuId_Config_ResetAll,

	// Capture Subsection
	MenuId_Capture_Video,
	MenuId_Capture_Video_Record,
	MenuId_Capture_Video_Stop,
	MenuId_Capture_Video_IncludeAudio,
	MenuId_Capture_Screenshot,
	MenuId_Capture_Screenshot_Screenshot,
	MenuId_Capture_Screenshot_Screenshot_As,

	// Input Recording Subsection
	MenuId_Recording_New,
	MenuId_Recording_Play,
	MenuId_Recording_Stop,
	MenuId_Recording_Settings,
	MenuId_Recording_Config_FrameAdvance,
	MenuId_Recording_TogglePause,
	MenuId_Recording_FrameAdvance,
	MenuId_Recording_ToggleRecordingMode,
	MenuId_Recording_VirtualPad_Port0,
	MenuId_Recording_VirtualPad_Port1,

	//  Subsection
	MenuId_PINE,
	MenuId_PINE_Enable,
	MenuId_PINE_Settings,

};

namespace Exception
{
	// --------------------------------------------------------------------------
	// Exception used to perform an "errorless" termination of the app during OnInit
	// procedures.  This happens when a user cancels out of startup prompts/wizards.
	//
	class StartupAborted : public CancelEvent
	{
		DEFINE_RUNTIME_EXCEPTION(StartupAborted, CancelEvent, "Startup initialization was aborted by the user.")

	public:
		StartupAborted(std::string reason)
		{
			m_message_diag = "Startup aborted: " + reason;
		}
	};

} // namespace Exception

// --------------------------------------------------------------------------------------
//  AppImageIds  - Config and Toolbar Images and Icons
// --------------------------------------------------------------------------------------
struct AppImageIds
{
	struct ConfigIds
	{
		int Paths,
			Speedhacks,
			Gamefixes,
			MemoryCard,
			Video,
			Cpu;

		ConfigIds()
		{
			Paths =
				Speedhacks = Gamefixes =
					Video = Cpu =
						MemoryCard = -1;
		}
	} Config;

	struct ToolbarIds
	{
		int Settings,
			Play,
			Resume;

		ToolbarIds()
		{
			Settings = -1;
			Play = -1;
			Resume = -1;
		}
	} Toolbars;
};

// -------------------------------------------------------------------------------------------
//  pxAppResources
// -------------------------------------------------------------------------------------------
// Container class for resources that should (or must) be unloaded prior to the ~wxApp() destructor.
// (typically this object is deleted at OnExit() or just prior to OnExit()).
//
class pxAppResources
{
public:
	AppImageIds ImageId;

	std::unique_ptr<wxImageList> ConfigImages;
	std::unique_ptr<wxImageList> ToolbarImages;
	std::unique_ptr<wxIconBundle> IconBundle;
	std::unique_ptr<wxBitmap> Bitmap_Logo;
	std::unique_ptr<wxBitmap> ScreenshotBitmap;

	pxAppResources();
	virtual ~pxAppResources();
};

class StartupOptions
{
public:
	bool ForceWizard;
	bool ForceConsole;
	bool PortableMode;

	// Disables the fast boot option when auto-running games.  This option only applies
	// if SysAutoRun is also true.
	bool NoFastBoot;

	// Specifies the Iso file to boot; used only if SysAutoRun is enabled and CdvdSource
	// is set to ISO.
	wxString IsoFile;

	wxString ElfFile;

	wxString GameLaunchArgs;

	// Specifies the CDVD source type to use when AutoRunning
	CDVD_SourceType CdvdSource;

	// Indicates if PCSX2 should autorun the configured CDVD source and/or ISO file.
	bool SysAutoRun;
	bool SysAutoRunElf;
	bool SysAutoRunIrx;

	StartupOptions()
	{
		ForceWizard = false;
		ForceConsole = false;
		PortableMode = false;
		NoFastBoot = false;
		SysAutoRun = false;
		SysAutoRunElf = false;
		SysAutoRunIrx = false;
		CdvdSource = CDVD_SourceType::NoDisc;
	}
};

enum GsWindowMode_t
{
	GsWinMode_Unspecified = 0,
	GsWinMode_Windowed,
	GsWinMode_Fullscreen,
};

class CommandlineOverrides
{
public:
	wxDirName SettingsFolder;
	wxFileName VmSettingsFile;

	bool DisableSpeedhacks;
	bool ProfilingMode;

	// Note that gamefixes in this array should only be honored if the
	// "HasCustomGamefixes" boolean is also enabled.
	Pcsx2Config::GamefixOptions Gamefixes;
	bool ApplyCustomGamefixes;

	GsWindowMode_t GsWindowMode;

public:
	CommandlineOverrides()
	{
		DisableSpeedhacks = false;
		ApplyCustomGamefixes = false;
		GsWindowMode = GsWinMode_Unspecified;
		ProfilingMode = false;
	}

	// Returns TRUE if either speedhacks or gamefixes are being overridden.
	bool HasCustomHacks() const
	{
		return DisableSpeedhacks || ApplyCustomGamefixes;
	}

	void RemoveCustomHacks()
	{
		DisableSpeedhacks = false;
		ApplyCustomGamefixes = false;
	}

	bool HasSettingsOverride() const
	{
		return SettingsFolder.IsOk() || VmSettingsFile.IsOk();
	}

};

// =====================================================================================================
//  Pcsx2App  -  main wxApp class
// =====================================================================================================
class Pcsx2App : public wxAppWithHelpers
{
	typedef wxAppWithHelpers _parent;

	// ----------------------------------------------------------------------------
	// Event Sources!
	// These need to be at the top of the App class, because a lot of other things depend
	// on them and they are, themselves, fairly self-contained.

protected:
	EventSource<IEventListener_CoreThread> m_evtsrc_CoreThreadStatus;
	EventSource<IEventListener_AppStatus> m_evtsrc_AppStatus;

public:
	void AddListener(IEventListener_CoreThread& listener)
	{
		m_evtsrc_CoreThreadStatus.Add(listener);
	}

	void AddListener(IEventListener_AppStatus& listener)
	{
		m_evtsrc_AppStatus.Add(listener);
	}

	void RemoveListener(IEventListener_CoreThread& listener)
	{
		m_evtsrc_CoreThreadStatus.Remove(listener);
	}

	void RemoveListener(IEventListener_AppStatus& listener)
	{
		m_evtsrc_AppStatus.Remove(listener);
	}

	void AddListener(IEventListener_CoreThread* listener)
	{
		m_evtsrc_CoreThreadStatus.Add(listener);
	}

	void AddListener(IEventListener_AppStatus* listener)
	{
		m_evtsrc_AppStatus.Add(listener);
	}

	void RemoveListener(IEventListener_CoreThread* listener)
	{
		m_evtsrc_CoreThreadStatus.Remove(listener);
	}

	void RemoveListener(IEventListener_AppStatus* listener)
	{
		m_evtsrc_AppStatus.Remove(listener);
	}

	void DispatchEvent(AppEventType evt);
	void DispatchEvent(CoreThreadStatus evt);
	void DispatchUiSettingsEvent(IniInterface& ini);
	void DispatchVmSettingsEvent(IniInterface& ini);

	bool HasGUI() { return m_UseGUI; };
	bool ExitPromptWithNoGUI() { return m_NoGuiExitPrompt; };

	// ----------------------------------------------------------------------------
protected:
	int m_PendingSaves;
	bool m_ScheduledTermination;
	bool m_UseGUI;
	bool m_NoGuiExitPrompt;

	Threading::Mutex m_mtx_Resources;
	Threading::Mutex m_mtx_LoadingGameDB;

public:
	std::unique_ptr<CommandDictionary> GlobalCommands;
	std::unique_ptr<AcceleratorDictionary> GlobalAccels;

	StartupOptions Startup;
	CommandlineOverrides Overrides;

	std::unique_ptr<wxTimer> m_timer_Termination;

protected:
	std::unique_ptr<PipeRedirectionBase> m_StdoutRedirHandle;
	std::unique_ptr<PipeRedirectionBase> m_StderrRedirHandle;

	std::unique_ptr<RecentIsoList> m_RecentIsoList;
	std::unique_ptr<DriveList> m_DriveList;
	std::unique_ptr<pxAppResources> m_Resources;

public:
	// Executor Thread for complex VM/System tasks.  This thread is used to execute such tasks
	// in parallel to the main message pump, to allow the main pump to run without fear of
	// blocked threads stalling the GUI.
	ExecutorThread SysExecutorThread;
	std::unique_ptr<SysCpuProviderPack> m_CpuProviders;
	std::unique_ptr<SysMainMemory> m_VmReserve;

protected:
	wxWindowID m_id_MainFrame;
	wxWindowID m_id_GsFrame;
	wxWindowID m_id_ProgramLogBox;
	wxWindowID m_id_Disassembler;
	wxWindowID m_id_NewRecordingFrame;

	wxKeyEvent m_kevt;

public:
	Pcsx2App();
	virtual ~Pcsx2App();

	void PostMenuAction(MenuIdentifiers menu_id) const;
	void PostAppMethod(FnPtr_Pcsx2App method);
	void PostIdleAppMethod(FnPtr_Pcsx2App method);

	void SysApplySettings();
	void SysExecute();
	void SysExecute(CDVD_SourceType cdvdsrc, const wxString& elf_override = wxEmptyString);
	void LogicalVsync();

	SysMainMemory& GetVmReserve();

	GSFrame& GetGsFrame() const;
	MainEmuFrame& GetMainFrame() const;

	GSFrame* GetGsFramePtr() const { return (GSFrame*)wxWindow::FindWindowById(m_id_GsFrame); }
	MainEmuFrame* GetMainFramePtr() const { return (MainEmuFrame*)wxWindow::FindWindowById(m_id_MainFrame); }
	DisassemblyDialog* GetDisassemblyPtr() const { return (DisassemblyDialog*)wxWindow::FindWindowById(m_id_Disassembler); }

#ifndef PCSX2_CORE
	NewRecordingFrame* GetNewRecordingFramePtr() const
	{
		return (NewRecordingFrame*)wxWindow::FindWindowById(m_id_NewRecordingFrame);
	}
#endif

	void enterDebugMode();
	void leaveDebugMode();
	void resetDebugger();

	bool HasMainFrame() const { return GetMainFramePtr() != NULL; }

	void OpenGsPanel();
	void CloseGsPanel();
	void OnGsFrameClosed(wxWindowID id);
	void OnGsFrameDestroyed(wxWindowID id);
	void OnMainFrameClosed(wxWindowID id);

	// --------------------------------------------------------------------------
	//  Startup / Shutdown Helpers
	// --------------------------------------------------------------------------

	void DetectCpuAndUserMode();
	void OpenProgramLog();
	void OpenMainFrame();
	void PrepForExit();
	void CleanupRestartable();
	void CleanupResources();
	void WipeUserModeSettings();
	bool TestUserPermissionsRights(const wxDirName& testFolder, wxString& createFailedStr, wxString& accessFailedStr);
	void EstablishAppUserMode();
	void ForceFirstTimeWizardOnNextRun();

	wxConfigBase* OpenInstallSettingsFile();
	wxConfigBase* TestForPortableInstall();

	bool HasPendingSaves() const;
	void StartPendingSave();
	void ClearPendingSave();

	// --------------------------------------------------------------------------
	//  App-wide Resources
	// --------------------------------------------------------------------------
	// All of these accessors cache the resources on first use and retain them in
	// memory until the program exits.

	wxMenu& GetRecentIsoMenu();
	RecentIsoManager& GetRecentIsoManager();
	wxMenu& GetDriveListMenu();

	pxAppResources& GetResourceCache();
	const wxIconBundle& GetIconBundle();
	const wxBitmap& GetLogoBitmap();
	const wxBitmap& GetScreenshotBitmap();
	wxImageList& GetImgList_Config();
	wxImageList& GetImgList_Toolbars();

	const AppImageIds& GetImgId() const;

	// --------------------------------------------------------------------------
	//  Overrides of wxApp virtuals:
	// --------------------------------------------------------------------------
	wxAppTraits* CreateTraits();
	bool OnInit();
	int OnExit();
	void CleanUp();

	void OnInitCmdLine(wxCmdLineParser& parser);
	bool OnCmdLineParsed(wxCmdLineParser& parser);
	bool OnCmdLineError(wxCmdLineParser& parser);
	bool ParseOverrides(wxCmdLineParser& parser);

#ifdef __WXDEBUG__
	void OnAssertFailure(const wxChar* file, int line, const wxChar* func, const wxChar* cond, const wxChar* msg);
#endif

	Threading::MutexRecursive m_mtx_ProgramLog;
	ConsoleLogFrame* m_ptr_ProgramLog;

	// ----------------------------------------------------------------------------
	//   Console / Program Logging Helpers
	// ----------------------------------------------------------------------------
	ConsoleLogFrame* GetProgramLog();
	const ConsoleLogFrame* GetProgramLog() const;
	void ProgramLog_PostEvent(wxEvent& evt);
	Threading::Mutex& GetProgramLogLock();

	void EnableAllLogging();
	void DisableWindowLogging() const;
	void DisableDiskLogging() const;
	void OnProgramLogClosed(wxWindowID id);

protected:
	bool AppRpc_TryInvoke(FnPtr_Pcsx2App method);
	bool AppRpc_TryInvokeAsync(FnPtr_Pcsx2App method);

	void AllocateCoreStuffs();
	void InitDefaultGlobalAccelerators();
	void BuildCommandHash();
	bool TryOpenConfigCwd();
	void CleanupOnExit();
	void OpenWizardConsole();
	void PadKeyDispatch(const HostKeyEvent& ev);

protected:
	void HandleEvent(wxEvtHandler* handler, wxEventFunction func, wxEvent& event) const;
	void HandleEvent(wxEvtHandler* handler, wxEventFunction func, wxEvent& event);

	void OnScheduledTermination(wxTimerEvent& evt);
	void OnEmuKeyDown(wxKeyEvent& evt);
	void OnSysExecutorTaskTimeout(wxTimerEvent& evt);
	void OnDestroyWindow(wxWindowDestroyEvent& evt);

	// ----------------------------------------------------------------------------
	//      Override wx default exception handling behavior
	// ----------------------------------------------------------------------------

	// Just rethrow exceptions in the main loop, so that we can handle them properly in our
	// custom catch clauses in OnRun().  (ranting note: wtf is the point of this functionality
	// in wx?  Why would anyone ever want a generic catch-all exception handler that *isn't*
	// the unhandled exception handler?  Using this as anything besides a re-throw is terrible
	// program design and shouldn't even be allowed -- air)
	bool OnExceptionInMainLoop() { throw; }

	// Just rethrow unhandled exceptions to cause immediate debugger fail.
	void OnUnhandledException() { throw; }
};


wxDECLARE_APP(Pcsx2App);

// --------------------------------------------------------------------------------------
//  s* macros!  ['s' stands for 'shortcut']
// --------------------------------------------------------------------------------------
// Use these for "silent fail" invocation of PCSX2 Application-related constructs.  If the
// construct (albeit wxApp, MainFrame, CoreThread, etc) is null, the requested method will
// not be invoked, and an optional "else" clause can be affixed for handling the end case.
//
// Usage Examples:
//   sMainFrame.ApplySettings();
//   sMainFrame.ApplySettings(); else Console.WriteLn( "Judge Wapner" );	// 'else' clause for handling NULL scenarios.
//
// Note!  These macros are not "syntax complete", which means they could generate unexpected
// syntax errors in some situations, and more importantly, they cannot be used for invoking
// functions with return values.
//
// Rationale: There are a lot of situations where we want to invoke a void-style method on
// various volatile object pointers (App, Corethread, MainFrame, etc).  Typically if these
// objects are NULL the most intuitive response is to simply ignore the call request and
// continue running silently.  These macros make that possible without any extra boilerplate
// conditionals or temp variable defines in the code.
//
#define sApp                                                \
	if (Pcsx2App* __app_ = (Pcsx2App*)wxApp::GetInstance()) \
	(*__app_)

#define sLogFrame                                                  \
	if (ConsoleLogFrame* __conframe_ = wxGetApp().GetProgramLog()) \
	(*__conframe_)

#define sMainFrame                                  \
	if (MainEmuFrame* __frame_ = GetMainFramePtr()) \
	(*__frame_)

// Use this within the scope of a wxWindow (wxDialog or wxFrame).  If the window has a valid menu
// bar, the command will run, otherwise it will be silently ignored. :)
#define sMenuBar                              \
	if (wxMenuBar* __menubar_ = GetMenuBar()) \
	(*__menubar_)

// --------------------------------------------------------------------------------------
//  AppOpenDialog
// --------------------------------------------------------------------------------------
// Returns a wxWindow handle to the opened window.
//
template <typename DialogType>
wxWindow* AppOpenDialog(wxWindow* parent = NULL)
{
	wxWindow* window = wxFindWindowByName(L"Dialog:" + DialogType::GetNameStatic());

	if (!window)
		window = new DialogType(parent);

	window->Show();
	window->SetFocus();
	return window;
}

// --------------------------------------------------------------------------------------
//  AppOpenModalDialog
// --------------------------------------------------------------------------------------
// Returns the ID of the button used to close the dialog.
//
template <typename DialogType>
int AppOpenModalDialog(wxString panel_name, wxWindow* parent = NULL)
{
	if (wxWindow* window = wxFindWindowByName(L"Dialog:" + DialogType::GetNameStatic()))
	{
		window->SetFocus();
		if (wxDialog* dialog = wxDynamicCast(window, wxDialog))
		{
			// Switch to the requested panel.
			if (panel_name != wxEmptyString)
			{
				wxCommandEvent evt(pxEvt_SetSettingsPage);
				evt.SetString(panel_name);
				dialog->GetEventHandler()->ProcessEvent(evt);
			}

			// It's legal to call ShowModal on a non-modal dialog, therefore making
			// it modal in nature for the needs of whatever other thread of action wants
			// to block against it:
			if (!dialog->IsModal())
			{
				int result = dialog->ShowModal();
				dialog->Destroy();
				return result;
			}
		}
		pxFailDev("Can only show wxDialog class windows as modal!");
		return wxID_CANCEL;
	}
	else
		return DialogType(parent).ShowModal();
}

// --------------------------------------------------------------------------------------
//  External App-related Globals and Shortcuts
// --------------------------------------------------------------------------------------

extern bool SysHasValidState();
extern void SysUpdateIsoSrcFile(const wxString& newIsoFile);
extern void SysUpdateDiscSrcDrive(const wxString& newDiscDrive);
extern void SysStatus(const wxString& text);

extern bool HasMainFrame();
extern MainEmuFrame& GetMainFrame();
extern MainEmuFrame* GetMainFramePtr();

alignas(16) extern AppCoreThread CoreThread;
alignas(16) extern SysMtgsThread mtgsThread;

extern void UI_UpdateSysControls();

extern void UI_DisableStateActions();
extern void UI_EnableStateActions();

extern void UI_DisableSysActions();
extern void UI_EnableSysActions();

extern void UI_DisableSysShutdown();

#define AffinityAssert_AllowFrom_SysExecutor() \
	pxAssertMsg(wxGetApp().SysExecutorThread.IsSelf(), "Thread affinity violation: Call allowed from SysExecutor thread only.")

#define AffinityAssert_DisallowFrom_SysExecutor() \
	pxAssertMsg(!wxGetApp().SysExecutorThread.IsSelf(), "Thread affinity violation: Call is *not* allowed from SysExecutor thread.")

extern ExecutorThread& GetSysExecutorThread();

extern bool g_ConfigPanelChanged; //Indicates that the main config panel is open and holds unapplied changes.
