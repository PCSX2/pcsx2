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
#include "AppAccelerators.h"
#include "ConsoleLogger.h"
#include "MSWstuff.h"
#include "MTVU.h" // for thread cancellation on shutdown
#include "IniInterface.h"
#include "StringHelpers.h"

#include "common/FileSystem.h"
#include "common/StringUtil.h"
#include "DebugTools/Debug.h"
#include "Dialogs/ModalPopups.h"

#include "Debugger/DisassemblyDialog.h"

#include "Recording/InputRecording.h"

#include <wx/cmdline.h>
#include <wx/intl.h>
#include <wx/stdpaths.h>
#include <memory>

#ifdef __WXGTK__
#include <gdk/gdkx.h>
#ifdef GDK_WINDOWING_X11
#include <X11/Xlib.h>
#endif
#endif	// __WXGTK__

#ifdef SDL_BUILD
#include <SDL.h>
#endif

using namespace pxSizerFlags;

void Pcsx2App::DetectCpuAndUserMode()
{
	AffinityAssert_AllowFrom_MainUI();

#ifdef _M_X86
	x86caps.Identify();
	x86caps.CountCores();
	x86caps.SIMD_EstablishMXCSRmask();

	if (!x86caps.hasStreamingSIMD4Extensions)
	{
		// This code will probably never run if the binary was correctly compiled for SSE4
		// SSE4 is required for any decent speed and is supported by more than decade old x86 CPUs
		throw Exception::HardwareDeficiency()
			.SetDiagMsg("Critical Failure: SSE4.1 Extensions not available.")
			.SetUserMsg("SSE4 extensions are not available.  PCSX2 requires a cpu that supports the SSE4.1 instruction set.");
	}
#endif

	EstablishAppUserMode();

	// Check that the resources directory exists and contains our data files.
	if (!FileSystem::DirectoryExists(EmuFolders::Resources.c_str()))
	{
		throw Exception::RuntimeError()
			.SetDiagMsg("Resources directory does not exist.")
			.SetUserMsg("Resources directory does not exist. Your installation is incomplete.");
	}
}

void Pcsx2App::OpenMainFrame()
{
	if (AppRpc_TryInvokeAsync(&Pcsx2App::OpenMainFrame))
		return;

	if (GetMainFramePtr() != NULL)
		return;

	MainEmuFrame* mainFrame = new MainEmuFrame(NULL, pxGetAppName());
	m_id_MainFrame = mainFrame->GetId();

	DisassemblyDialog* disassembly = new DisassemblyDialog(mainFrame);
	m_id_Disassembler = disassembly->GetId();

	NewRecordingFrame* newRecordingFrame = new NewRecordingFrame(mainFrame);
	m_id_NewRecordingFrame = newRecordingFrame->GetId();
	if (g_Conf->EmuOptions.EnableRecordingTools)
		g_InputRecording.InitVirtualPadWindows(mainFrame);

	if (g_Conf->EmuOptions.Debugger.ShowDebuggerOnStart)
		disassembly->Show();

	PostIdleAppMethod(&Pcsx2App::OpenProgramLog);

	SetTopWindow(mainFrame);     // not really needed...
	SetExitOnFrameDelete(false); // but being explicit doesn't hurt...
	mainFrame->Show();
}

void Pcsx2App::OpenProgramLog()
{
	if (AppRpc_TryInvokeAsync(&Pcsx2App::OpenProgramLog))
		return;

	if (/*ConsoleLogFrame* frame =*/GetProgramLog())
	{
		//pxAssume( );
		return;
	}

	wxWindow* m_current_focus = wxGetActiveWindow();

	ScopedLock lock(m_mtx_ProgramLog);
	m_ptr_ProgramLog = new ConsoleLogFrame(GetMainFramePtr(), L"PCSX2 Program Log", g_Conf->ProgLogBox);
	m_id_ProgramLogBox = m_ptr_ProgramLog->GetId();
	EnableAllLogging();

	if (m_current_focus)
		m_current_focus->SetFocus();

	// This is test code for printing out all supported languages and their canonical names in wiki-fied
	// format.  I might use it again soon, so I'm leaving it in for now... --air
	/*
	for( int li=wxLANGUAGE_UNKNOWN+1; li<wxLANGUAGE_USER_DEFINED; ++li )
	{
		if (const wxLanguageInfo* info = wxLocale::GetLanguageInfo( li ))
		{			
			if (i18n_IsLegacyLanguageId((wxLanguage)info->Language)) continue;			
			Console.WriteLn( L"|| %-30s || %-8s ||", info->Description.c_str(), info->CanonicalName.c_str() );
		}
	}
	*/
}

void Pcsx2App::AllocateCoreStuffs()
{
	if (AppRpc_TryInvokeAsync(&Pcsx2App::AllocateCoreStuffs))
		return;

	SysLogMachineCaps();
	AppApplySettings();

	GetVmReserve().ReserveAll();

	if (!m_CpuProviders)
	{
		// FIXME : Some or all of SysCpuProviderPack should be run from the SysExecutor thread,
		// so that the thread is safely blocked from being able to start emulation.

		m_CpuProviders = std::make_unique<SysCpuProviderPack>();

		if (m_CpuProviders->HadSomeFailures(g_Conf->EmuOptions.Cpu.Recompiler))
		{
			// HadSomeFailures only returns 'true' if an *enabled* cpu type fails to init.  If
			// the user already has all interps configured, for example, then no point in
			// popping up this dialog.

			wxDialogWithHelpers exconf(NULL, _("PCSX2 Recompiler Error(s)"));

			wxTextCtrl* scrollableTextArea = new wxTextCtrl(
				&exconf, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
				wxTE_READONLY | wxTE_MULTILINE | wxTE_WORDWRAP);

			exconf += 12;
			exconf += exconf.Heading(pxE(L"Warning: Some of the configured PS2 recompilers failed to initialize and have been disabled:"));

			exconf += 6;
			exconf += scrollableTextArea | pxExpand.Border(wxALL, 16);

			Pcsx2Config::RecompilerOptions& recOps = g_Conf->EmuOptions.Cpu.Recompiler;

			if (BaseException* ex = m_CpuProviders->GetException_EE())
			{
				scrollableTextArea->AppendText(StringUtil::UTF8StringToWxString("* R5900 (EE)\n\t" + ex->FormatDisplayMessage() + "\n\n"));
				recOps.EnableEE = false;
			}

			if (BaseException* ex = m_CpuProviders->GetException_IOP())
			{
				scrollableTextArea->AppendText(StringUtil::UTF8StringToWxString("* R3000A (IOP)\n\t" + ex->FormatDisplayMessage() + "\n\n"));
				recOps.EnableIOP = false;
			}

			if (BaseException* ex = m_CpuProviders->GetException_MicroVU0())
			{
				scrollableTextArea->AppendText(StringUtil::UTF8StringToWxString("* microVU0\n\t" + ex->FormatDisplayMessage() + "\n\n"));
				recOps.EnableVU0 = false;
			}

			if (BaseException* ex = m_CpuProviders->GetException_MicroVU1())
			{
				scrollableTextArea->AppendText(StringUtil::UTF8StringToWxString("* microVU1\n\t" + ex->FormatDisplayMessage() + "\n\n"));
				recOps.EnableVU1 = false;
			}

			exconf += exconf.Heading(pxE(L"Note: Recompilers are not necessary for PCSX2 to run, however they typically improve emulation speed substantially. You may have to manually re-enable the recompilers listed above, if you resolve the errors."));

			pxIssueConfirmation(exconf, MsgButtons().OK());
		}
	}
}


void Pcsx2App::OnInitCmdLine(wxCmdLineParser& parser)
{
	parser.SetLogo(AddAppName(" >>  %s  --  A PlayStation 2 Emulator for the PC  <<") + L"\n\n" +
				   _("All options are for the current session only and will not be saved.\n"));

	wxString fixlist(L" ");
	for (GamefixId i = GamefixId_FIRST; i < pxEnumEnd; ++i)
	{
		if (i != GamefixId_FIRST)
			fixlist += L",";
		fixlist += EnumToString(i);
	}

	parser.AddParam(_("IsoFile"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);
	parser.AddSwitch(L"h", L"help", _("displays this list of command line options"), wxCMD_LINE_OPTION_HELP);
	parser.AddSwitch(wxEmptyString, L"console", _("forces the program log/console to be visible"), wxCMD_LINE_VAL_STRING);
	parser.AddSwitch(wxEmptyString, L"fullscreen", _("use fullscreen GS mode"));
	parser.AddSwitch(wxEmptyString, L"windowed", _("use windowed GS mode"));

	parser.AddSwitch(wxEmptyString, L"nogui", _("disables display of the gui while running games"));
	parser.AddSwitch(wxEmptyString, L"noguiprompt", _("when nogui - prompt before exiting on suspend"));

	parser.AddOption(wxEmptyString, L"elf", _("executes an ELF image"), wxCMD_LINE_VAL_STRING);
	parser.AddOption(wxEmptyString, L"irx", _("executes an IRX image"), wxCMD_LINE_VAL_STRING);
	parser.AddSwitch(wxEmptyString, L"nodisc", _("boots an empty DVD tray; use to enter the PS2 system menu"));
	parser.AddSwitch(wxEmptyString, L"usecd", _("boots from the disc drive (overrides IsoFile parameter)"));

	parser.AddSwitch(wxEmptyString, L"nohacks", _("disables all speedhacks"));
	parser.AddOption(wxEmptyString, L"gamefixes", _("use the specified comma or pipe-delimited list of gamefixes.") + fixlist, wxCMD_LINE_VAL_STRING);
	parser.AddSwitch(wxEmptyString, L"fullboot", _("disables fast booting"));
	parser.AddOption(wxEmptyString, L"gameargs", _("passes the specified space-delimited string of launch arguments to the game"), wxCMD_LINE_VAL_STRING);

	parser.AddOption(wxEmptyString, L"cfgpath", _("changes the configuration file path"), wxCMD_LINE_VAL_STRING);
	parser.AddOption(wxEmptyString, L"cfg", _("specifies the PCSX2 configuration file to use"), wxCMD_LINE_VAL_STRING);
	parser.AddSwitch(wxEmptyString, L"forcewiz", AddAppName(_("forces %s to start the First-time Wizard")));
	parser.AddSwitch(wxEmptyString, L"portable", _("enables portable mode operation (requires admin/root access)"));

	parser.AddSwitch(wxEmptyString, L"profiling", _("update options to ease profiling (debug)"));

	parser.SetSwitchChars(L"-");
}

bool Pcsx2App::OnCmdLineError(wxCmdLineParser& parser)
{
	wxApp::OnCmdLineError(parser);
	return false;
}

static void SetGameFixOptionsFromString(Pcsx2Config::GamefixOptions& dest, const wxString& list, bool enabled)
{
	wxStringTokenizer izer(list, L",|", wxTOKEN_STRTOK);

	while (izer.HasMoreTokens())
	{
		wxString token(izer.GetNextToken());

		GamefixId i;
		for (i = GamefixId_FIRST; i < pxEnumEnd; ++i)
		{
			if (token.CmpNoCase(EnumToString(i)) == 0)
				break;
		}
		if (i < pxEnumEnd)
			dest.Set(i);
	}
}

bool Pcsx2App::ParseOverrides(wxCmdLineParser& parser)
{
	wxString dest;
	bool parsed = true;

	if (parser.Found(L"cfgpath", &dest) && !dest.IsEmpty())
	{
		Console.Warning("Config path override: %ls", WX_STR(dest));
		Overrides.SettingsFolder = dest;
	}

	if (parser.Found(L"cfg", &dest) && !dest.IsEmpty())
	{
		Console.Warning("Config file override: %ls", WX_STR(dest));
		Overrides.VmSettingsFile = dest;
	}

	Overrides.DisableSpeedhacks = parser.Found(L"nohacks");

	Overrides.ProfilingMode = parser.Found(L"profiling");

	if (parser.Found(L"gamefixes", &dest))
	{
		Overrides.ApplyCustomGamefixes = true;
		SetGameFixOptionsFromString(Overrides.Gamefixes, dest, true);
	}

	if (parser.Found(L"fullscreen"))
		Overrides.GsWindowMode = GsWinMode_Fullscreen;
	if (parser.Found(L"windowed"))
		Overrides.GsWindowMode = GsWinMode_Windowed;

	return parsed;
}

bool Pcsx2App::OnCmdLineParsed(wxCmdLineParser& parser)
{
	if (parser.Found(L"console"))
	{
		Startup.ForceConsole = true;
		OpenProgramLog();
	}

	// Suppress wxWidgets automatic options parsing since none of them pertain to PCSX2 needs.
	//wxApp::OnCmdLineParsed( parser );

	m_UseGUI = !parser.Found(L"nogui");
	m_NoGuiExitPrompt = parser.Found(L"noguiprompt"); // by default no prompt for exit with nogui.

	if (!ParseOverrides(parser))
		return false;

	// --- Parse Startup/Autoboot options ---

	Startup.NoFastBoot = parser.Found(L"fullboot");
	Startup.ForceWizard = parser.Found(L"forcewiz");
	Startup.PortableMode = parser.Found(L"portable");

	if (parser.GetParamCount() >= 1)
	{
		Startup.IsoFile = parser.GetParam(0);
		Startup.CdvdSource = CDVD_SourceType::Iso;
		Startup.SysAutoRun = true;
	}
	else
	{
		wxString elf_file;
		if (parser.Found(L"elf", &elf_file) && !elf_file.IsEmpty())
			Startup.SysAutoRunElf = true;
		else if (parser.Found(L"irx", &elf_file) && !elf_file.IsEmpty())
			Startup.SysAutoRunIrx = true;

		if (!elf_file.IsEmpty())
		{
			auto path = wxFileName(elf_file);
			path.Normalize();
			Startup.ElfFile = path.GetFullPath();
		}
	}

	wxString game_args;
	if (parser.Found(L"gameargs", &game_args) && !game_args.IsEmpty())
		Startup.GameLaunchArgs = game_args;

	if (parser.Found(L"usecd"))
	{
		Startup.CdvdSource = CDVD_SourceType::Disc;
		Startup.SysAutoRun = true;
	}

	if (parser.Found(L"nodisc"))
	{
		Startup.CdvdSource = CDVD_SourceType::NoDisc;
		Startup.SysAutoRun = true;
	}

	return true;
}

typedef void (wxEvtHandler::*pxInvokeAppMethodEventFunction)(Pcsx2AppMethodEvent&);
typedef void (wxEvtHandler::*pxStuckThreadEventHandler)(pxMessageBoxEvent&);

bool Pcsx2App::OnInit()
{
	EnableAllLogging();
	Console.WriteLn("Interface is initializing.  Entering Pcsx2App::OnInit!");

	InitCPUTicks();

	g_Conf = std::make_unique<AppConfig>();
	wxInitAllImageHandlers();

	Console.WriteLn("Applying operating system default language...");
	{
		// The PCSX2 log system hasn't been set up yet, so error messages might
		// pop up that could cause some alarm amongst users. Let's avoid that.
		wxDoNotLogInThisScope please;
		i18n_SetLanguage(wxLANGUAGE_DEFAULT);
	}

	Console.WriteLn("Command line parsing...");
	if (!_parent::OnInit())
		return false;
	Console.WriteLn("Command line parsed!");

	i18n_SetLanguagePath();

	Bind(wxEVT_KEY_DOWN, &Pcsx2App::OnEmuKeyDown, this, pxID_PadHandler_Keydown);
	Bind(wxEVT_DESTROY, &Pcsx2App::OnDestroyWindow, this);

	// User/Admin Mode Dual Setup:
	//   PCSX2 now supports two fundamental modes of operation.  The default is Classic mode,
	//   which uses the Current Working Directory (CWD) for all user data files, and requires
	//   Admin access on Vista (and some Linux as well).  The second mode is the Vista-
	//   compatible \documents folder usage.  The mode is determined by the presence and
	//   contents of a usermode.ini file in the CWD.  If the ini file is missing, we assume
	//   the user is setting up a classic install.  If the ini is present, we read the value of
	//   the UserMode and SettingsPath vars.
	//
	//   Conveniently this dual mode setup applies equally well to most modern Linux distros.

	try
	{
		InitDefaultGlobalAccelerators();
		delete wxLog::SetActiveTarget(new pxLogConsole());

		SysExecutorThread.Start();
		DetectCpuAndUserMode();

		//   Set Manual Exit Handling
		// ----------------------------
		// PCSX2 has a lot of event handling logistics, so we *cannot* depend on wxWidgets automatic event
		// loop termination code.  We have a much safer system in place that continues to process messages
		// until all "important" threads are closed out -- not just until the main frame is closed(-ish).
		m_timer_Termination = std::make_unique<wxTimer>(this, wxID_ANY);
		Bind(wxEVT_TIMER, &Pcsx2App::OnScheduledTermination, this, m_timer_Termination->GetId());
		SetExitOnFrameDelete(false);


		//   Start GUI and/or Direct Emulation
		// -------------------------------------
		pxSizerFlags::SetBestPadding();
		if (Startup.ForceConsole)
			g_Conf->ProgLogBox.Visible = true;
		OpenProgramLog();
		AllocateCoreStuffs();
		if (m_UseGUI)
			OpenMainFrame();


		// By default no IRX injection
		EmuConfig.CurrentIRX.clear();

		if (Startup.SysAutoRun)
		{
			g_Conf->EmuOptions.UseBOOT2Injection = !Startup.NoFastBoot;
			g_Conf->CdvdSource = Startup.CdvdSource;
			if (Startup.CdvdSource == CDVD_SourceType::Iso)
				SysUpdateIsoSrcFile(Startup.IsoFile);
			sApp.SysExecute(Startup.CdvdSource);
			EmuConfig.CurrentGameArgs = StringUtil::wxStringToUTF8String(Startup.GameLaunchArgs);
		}
		else if (Startup.SysAutoRunElf)
		{
			g_Conf->EmuOptions.UseBOOT2Injection = true;

			// wxPATH_NATIVE is broken on msw, it can delete the first directory after the volume
			// EX: P://dir1/dir2/elf.elf -> P://dir2/ ???
#ifdef _WIN32
			wxFileName elfFile = wxFileName(Startup.ElfFile, wxPATH_WIN);
#else
			wxFileName elfFile = wxFileName(Startup.ElfFile, wxPATH_NATIVE);
#endif

			if (!elfFile.FileExists())
			{
				wxMessageBox(wxString::Format(_("Specified elf file %s does not exist!"), Startup.ElfFile), "PCSX2", wxICON_ERROR);
			}
			else
			{
				g_Conf->Folders.RunELF = elfFile.GetPath();
				sApp.SysExecute(Startup.CdvdSource, Startup.ElfFile);
				if (Startup.ElfFile.Find(' ') == -1)
					EmuConfig.CurrentGameArgs = StringUtil::wxStringToUTF8String(Startup.GameLaunchArgs);
			}
		}
		else if (Startup.SysAutoRunIrx)
		{
			g_Conf->EmuOptions.UseBOOT2Injection = true;

			EmuConfig.CurrentIRX = StringUtil::wxStringToUTF8String(Startup.ElfFile);

			// FIXME: ElfFile is an irx it will crash
			sApp.SysExecute(Startup.CdvdSource, Startup.ElfFile);
			if (Startup.ElfFile.Find(' ') == -1)
				EmuConfig.CurrentGameArgs = StringUtil::wxStringToUTF8String(Startup.GameLaunchArgs);
		}
	}
	// ----------------------------------------------------------------------------
	catch (Exception::StartupAborted& ex) // user-aborted, no popups needed.
	{
		Console.Warning(ex.FormatDiagnosticMessage());
		CleanupOnExit();
		return false;
	}
	catch (Exception::HardwareDeficiency& ex)
	{
		Msgbox::Alert(StringUtil::UTF8StringToWxString(ex.FormatDisplayMessage()) + L"\n\n" + AddAppName(_("Press OK to close %s.")), _("PCSX2 Error: Hardware Deficiency."));
		CleanupOnExit();
		return false;
	}
	// ----------------------------------------------------------------------------
	// Failures on the core initialization procedure (typically OutOfMemory errors) are bad,
	// since it means the emulator is completely non-functional.  Let's pop up an error and
	// exit gracefully-ish.
	//
	catch (Exception::RuntimeError& ex)
	{
		Console.Error(ex.FormatDiagnosticMessage());
		Msgbox::Alert(StringUtil::UTF8StringToWxString(ex.FormatDisplayMessage()) + L"\n\n" + AddAppName(_("Press OK to close %s.")),
					  AddAppName(_("%s Critical Error")), wxICON_ERROR);
		CleanupOnExit();
		return false;
	}

#ifdef SDL_BUILD
	// MacOS Game Controller framework requires a few runs of the main event loop after interest in game controllers is first indicated to connect controllers
	// Since OnePad doesn't currently handle connection/disconnection events and requires controllers to be connected on start, we need to initialize SDL before OnePad looks at the controller list
	SDL_Init(SDL_INIT_GAMECONTROLLER);
#endif
	return true;
}

static int m_term_threshold = 20;

void Pcsx2App::OnScheduledTermination(wxTimerEvent& evt)
{
	if (!pxAssertDev(m_ScheduledTermination, "Scheduled Termination check is inconsistent with ScheduledTermination status."))
	{
		m_timer_Termination->Stop();
		return;
	}

	if (m_PendingSaves != 0)
	{
		if (--m_term_threshold > 0)
		{
			Console.WriteLn("(App) %d saves are still pending; exit postponed...", m_PendingSaves);
			return;
		}

		Console.Error("(App) %s pending saves have exceeded OnExit threshold and are being prematurely terminated!", m_PendingSaves);
	}

	m_timer_Termination->Stop();
	Exit();
}


// Common exit handler which can be called from any event (though really it should
// be called only from CloseWindow handlers since that's the more appropriate way
// to handle cancelable window closures)
//
// returns true if the app can close, or false if the close event was canceled by
// the glorious user, whomever (s)he-it might be.
void Pcsx2App::PrepForExit()
{
	if (m_ScheduledTermination)
		return;
	m_ScheduledTermination = true;

	DispatchEvent(AppStatus_Exiting);

	CoreThread.Cancel();
	SysExecutorThread.ShutdownQueue();

	m_timer_Termination->Start(500);
}

// This cleanup procedure can only be called when the App message pump is still active.
// OnExit() must use CleanupOnExit instead.
void Pcsx2App::CleanupRestartable()
{
	AffinityAssert_AllowFrom_MainUI();

	CoreThread.Cancel();
	SysExecutorThread.ShutdownQueue();
	IdleEventDispatcher(L"Cleanup");

	if (g_Conf)
		AppSaveSettings();
}

// This cleanup handler can be called from OnExit (it doesn't need a running message pump),
// but should not be called from the App destructor.  It's needed because wxWidgets doesn't
// always call OnExit(), so I had to make CleanupRestartable, and then encapsulate it here
// to be friendly to the OnExit scenario (no message pump).
void Pcsx2App::CleanupOnExit()
{
	AffinityAssert_AllowFrom_MainUI();

	try
	{
		CleanupRestartable();
		CleanupResources();
	}
	catch (Exception::CancelEvent&)
	{
		throw;
	}
	catch (Exception::RuntimeError& ex)
	{
		// Handle runtime errors gracefully during shutdown.  Mostly these are things
		// that we just don't care about by now, and just want to "get 'er done!" so
		// we can exit the app. ;)

		Console.Error("Runtime exception handled during CleanupOnExit:\n");
		Console.Indent().Error(ex.FormatDiagnosticMessage());
	}

	// FIXME: performing a wxYield() here may fix that problem. -- air

	Console_SetActiveHandler(ConsoleWriter_Stdout);
}

void Pcsx2App::CleanupResources()
{
	ScopedBusyCursor cursor(Cursor_ReallyBusy);
	//delete wxConfigBase::Set( NULL );

	while (wxGetLocale() != NULL)
		delete wxGetLocale();

	m_mtx_LoadingGameDB.Wait();
	ScopedLock lock(m_mtx_Resources);
	m_Resources = NULL;
}

int Pcsx2App::OnExit()
{
	CleanupOnExit();
	return wxApp::OnExit();
}

void Pcsx2App::OnDestroyWindow(wxWindowDestroyEvent& evt)
{
	// Precautions:
	//  * Whenever windows are destroyed, make sure to check if it matches our "active"
	//    console logger.  If so, we need to disable logging to the console window, or else
	//    it'll crash.  (this is because the console log system uses a cached window handle
	//    instead of looking the window up via it's ID -- fast but potentially unsafe).


	OnProgramLogClosed(evt.GetId());
	OnGsFrameClosed(evt.GetId());
	evt.Skip();
}

// --------------------------------------------------------------------------------------
//  SysEventHandler
// --------------------------------------------------------------------------------------
class SysEvtHandler : public pxEvtQueue
{
public:
	wxString GetEvtHandlerName() const { return L"SysExecutor"; }

protected:
	// When the SysExec message queue is finally empty, we should check the state of
	// the menus and make sure they're all consistent to the current emulation states.
	void _DoIdle()
	{
		UI_UpdateSysControls();
	}
};


Pcsx2App::Pcsx2App()
	: SysExecutorThread(new SysEvtHandler())
{
// Warning: Do not delete this comment block! Gettext will parse it to allow
// the translation of some wxWidget internal strings. -- greg
#if 0
	{
		// Some common labels provided by wxWidgets.  wxWidgets translation files are chucked full
		// of worthless crap, and tally more than 200k each.  We only need these couple.

		_("OK");
		_("&OK");
		_("Cancel");
		_("&Cancel");
		_("&Apply");
		_("&Next >");
		_("< &Back");
		_("&Back");
		_("&Finish");
		_("&Yes");
		_("&No");
		_("Browse");
		_("&Save");
		_("Save &As...");
		_("&Help");
		_("&Home");
		_("&Window");

		_("Show about dialog.")
	}
#endif

#ifdef GDK_WINDOWING_X11
	// This *must* be done in the constructor, before wx starts making X calls.
	XInitThreads();
#endif

	m_PendingSaves = 0;
	m_ScheduledTermination = false;
	m_UseGUI = true;
	m_NoGuiExitPrompt = true;

	m_id_MainFrame = wxID_ANY;
	m_id_GsFrame = wxID_ANY;
	m_id_ProgramLogBox = wxID_ANY;
	m_id_Disassembler = wxID_ANY;
	m_ptr_ProgramLog = NULL;

	SetAppName(L"PCSX2");
	BuildCommandHash();
}

Pcsx2App::~Pcsx2App()
{
}

void Pcsx2App::CleanUp()
{
	CleanupResources();
	m_Resources = NULL;
	m_RecentIsoList = NULL;

	DisableDiskLogging();

	if (emuLog != NULL)
	{
		fclose(emuLog);
		emuLog = NULL;
	}

	_parent::CleanUp();
}

__fi wxString AddAppName(const wxString& fmt)
{
	return pxsFmt(fmt, WX_STR(pxGetAppName()));
}

__fi wxString AddAppName(const wxChar* fmt)
{
	return pxsFmt(fmt, WX_STR(pxGetAppName()));
}

__fi wxString AddAppName(const char* fmt)
{
	return pxsFmt(fromUTF8(fmt), WX_STR(pxGetAppName()));
}

// ------------------------------------------------------------------------------------------
//  Using the MSVCRT to track memory leaks:
// ------------------------------------------------------------------------------------------
// When exiting PCSX2 normally, the CRT will make a list of all memory that's leaked.  The
// number inside {} can be pasted into the line below to cause MSVC to breakpoint on that
// allocation at the time it's made.  And then using a stacktrace you can figure out what
// leaked! :D
//
// Limitations: Unfortunately, wxWidgets gui uses a lot of heap allocations while handling
// messages, and so any mouse movements will pretty much screw up the leak value.  So to use
// this feature you need to execute pcsx in no-gui mode, and then not move the mouse or use
// the keyboard until you get to the leak. >_<
//
// (but this tool is still better than nothing!)

#ifdef PCSX2_DEBUG
struct CrtDebugBreak
{
	CrtDebugBreak(int spot)
	{
#ifdef __WXMSW__
		_CrtSetBreakAlloc(spot);
#endif
	}
};

//CrtDebugBreak breakAt( 11549 );

#endif
