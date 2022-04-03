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
#include "App.h"
#include "MainFrame.h"
#include "GSFrame.h"
#include "GS.h"
#include "Host.h"
#include "AppSaveStates.h"
#include "AppAccelerators.h"
#include "IniInterface.h"
#include "PAD/Gamepad.h"

#include "ps2/BiosTools.h"

#include "Dialogs/ModalPopups.h"
#include "Dialogs/ConfigurationDialog.h"
#include "Dialogs/LogOptionsDialog.h"

#include "Debugger/DisassemblyDialog.h"

#include "Recording/InputRecordingControls.h"
#include "Recording/InputRecording.h"

#include "common/FileSystem.h"
#include "common/StringUtil.h"
#include "AppTrait.h"

#include <wx/stdpaths.h>

#ifdef __WXMSW__
#	include <wx/msw/wrapwin.h>		// needed to implement the app!
#endif

#ifdef __WXGTK__
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif
#endif

// Safe to remove these lines when this is handled properly.
#ifdef __WXMAC__
// Great joy....
#undef EBP
#undef ESP
#undef EDI
#undef ESI
#undef EDX
#undef EAX
#undef EBX
#undef ECX
#include <wx/osx/private.h>		// needed to implement the app!
#endif
wxIMPLEMENT_APP(Pcsx2App);

std::unique_ptr<AppConfig> g_Conf;

WindowInfo g_gs_window_info;

static bool CheckForBIOS()
{
	if (IsBIOSAvailable(g_Conf->EmuOptions.FullpathToBios()))
		return true;

	wxString error = pxE(L"PCSX2 requires a PS2 BIOS in order to run.  For legal reasons, you *must* obtain a BIOS from an actual PS2 unit that you own (borrowing doesn't count).  Please consult the FAQs and Guides for further instructions.");

	Msgbox::Alert(error, _("PS2 BIOS Error"));
	return false;
}

// Allows for activating menu actions from anywhere in PCSX2.
// And it's Thread Safe!
void Pcsx2App::PostMenuAction( MenuIdentifiers menu_id ) const
{
	MainEmuFrame* mainFrame = GetMainFramePtr();
	if( mainFrame == NULL ) return;

	wxCommandEvent joe( wxEVT_MENU, menu_id );
	if( wxThread::IsMain() )
		mainFrame->GetEventHandler()->ProcessEvent( joe );
	else
		mainFrame->GetEventHandler()->AddPendingEvent( joe );
}

// --------------------------------------------------------------------------------------
//  Pcsx2AppMethodEvent
// --------------------------------------------------------------------------------------
// Unlike pxPingEvent, the Semaphore belonging to this event is typically posted when the
// invoked method is completed.  If the method can be executed in non-blocking fashion then
// it should leave the semaphore postback NULL.
//
class Pcsx2AppMethodEvent : public pxActionEvent
{
	typedef pxActionEvent _parent;
	wxDECLARE_DYNAMIC_CLASS_NO_ASSIGN(Pcsx2AppMethodEvent);

protected:
	FnPtr_Pcsx2App	m_Method;

public:
	virtual ~Pcsx2AppMethodEvent() = default;
	virtual Pcsx2AppMethodEvent *Clone() const { return new Pcsx2AppMethodEvent(*this); }

	explicit Pcsx2AppMethodEvent( FnPtr_Pcsx2App method=NULL, SynchronousActionState* sema=NULL )
		: pxActionEvent( sema )
	{
		m_Method = method;
	}

	explicit Pcsx2AppMethodEvent( FnPtr_Pcsx2App method, SynchronousActionState& sema )
		: pxActionEvent( sema )
	{
		m_Method = method;
	}
	
	Pcsx2AppMethodEvent( const Pcsx2AppMethodEvent& src )
		: pxActionEvent( src )
	{
		m_Method = src.m_Method;
	}
		
	void SetMethod( FnPtr_Pcsx2App method )
	{
		m_Method = method;
	}
	
protected:
	void InvokeEvent()
	{
		if( m_Method ) (wxGetApp().*m_Method)();
	}
};


wxIMPLEMENT_DYNAMIC_CLASS( Pcsx2AppMethodEvent, pxActionEvent );

#ifdef __WXMSW__
extern int TranslateVKToWXK(u32 keysym);
#elif defined(__WXGTK__)
extern int TranslateGDKtoWXK(u32 keysym);
#elif defined(__APPLE__)
extern int TranslateOSXtoWXK(u32 keysym);
#endif

void Pcsx2App::PadKeyDispatch(const HostKeyEvent& ev)
{
	m_kevt.SetEventType( ( ev.type == HostKeyEvent::Type::KeyPressed ) ? wxEVT_KEY_DOWN : wxEVT_KEY_UP );

//returns 0 for normal keys and a WXK_* value for special keys
#ifdef __WXMSW__
	const int vkey = TranslateVKToWXK(ev.key);
#elif defined( __WXMAC__ )
	const int vkey = TranslateOSXtoWXK(ev.key);
#elif defined( __WXGTK__ )
	const int vkey = TranslateGDKtoWXK(ev.key);
#else
#	error Unsupported Target Platform.
#endif

	// Don't rely on current event handling to get the state of those specials keys.
	// Typical linux bug: hit ctrl-alt key to switch the desktop. Key will be released
	// outside of the window so the app isn't aware of the current key state.
	m_kevt.m_shiftDown = wxGetKeyState(WXK_SHIFT);
	m_kevt.m_controlDown = wxGetKeyState(WXK_CONTROL);
	m_kevt.m_altDown = wxGetKeyState(WXK_MENU) || wxGetKeyState(WXK_ALT);

	m_kevt.m_keyCode = vkey? vkey : ev.key;

	if (DevConWriterEnabled && m_kevt.GetEventType() == wxEVT_KEY_DOWN) {
		wxString strFromCode = wxAcceleratorEntry(
			(m_kevt.m_shiftDown ? wxACCEL_SHIFT : 0) | (m_kevt.m_controlDown ? wxACCEL_CTRL : 0) | (m_kevt.m_altDown ? wxACCEL_ALT : 0),
			m_kevt.m_keyCode
			).ToString();

		if (strFromCode.EndsWith(L"\\"))
			strFromCode += L"\\"; // If copied into PCSX2_keys.ini, \ needs escaping

		Console.WriteLn(StringUtil::wxStringToUTF8String(wxString::Format("> Key: %s (Code: %ld)",	WX_STR(strFromCode), m_kevt.m_keyCode)));
	}

	if( m_kevt.GetEventType() == wxEVT_KEY_DOWN )
	{
		if( GSFrame* gsFrame = wxGetApp().GetGsFramePtr() )
		{
			gsFrame->GetViewport()->DirectKeyCommand( m_kevt );
		}
		else
		{
			m_kevt.SetId( pxID_PadHandler_Keydown );
			wxGetApp().ProcessEvent( m_kevt );
		}
	}
}

// --------------------------------------------------------------------------------------
//  Pcsx2AppTraits (implementations)  [includes pxMessageOutputMessageBox]
// --------------------------------------------------------------------------------------
// This is here to override pxMessageOutputMessageBox behavior, which itself is ONLY used
// by wxWidgets' command line processor.  The default edition is totally inadequate for
// displaying a readable --help command line list, so I replace it here with a custom one
// that formats things nicer.
//

// This is only used in Windows. It's not possible to have wxWidgets show a localised
// command line help message in cmd/powershell/mingw bash. It can be done in English
// locales ( using AttachConsole, WriteConsole, FreeConsole combined with
// wxMessageOutputStderr), but completely fails for some other languages (i.e. Japanese).
#ifdef _WIN32
class pxMessageOutputMessageBox : public wxMessageOutput
{
public:
	pxMessageOutputMessageBox() { }

	// DoPrintf in wxMessageOutputBase (wxWidgets 3.0) uses this.
	virtual void Output(const wxString &out);
};

// EXTRAORDINARY HACK!  wxWidgets does not provide a clean way of overriding the commandline options
// display dialog.  The default one uses operating system built-in message/notice windows, which are
// appaling, ugly, and not at all suited to a large number of command line options.  Fortunately,
// wxMessageOutputMessageBox::PrintF is only used in like two places, so we can just check for the
// commandline window using an identifier we know is contained in it, and then format our own window
// display. :D  --air

void pxMessageOutputMessageBox::Output(const wxString& out)
{
	using namespace pxSizerFlags;

	wxString isoFormatted;
	isoFormatted.Printf(L"[%s]", _("IsoFile"));

	int pos = out.Find(isoFormatted.c_str());

	// I've no idea when this is true.
	if (pos == wxNOT_FOUND)
	{
		Msgbox::Alert( out ); return;
	}

	pos += isoFormatted.Length();

	wxDialogWithHelpers popup( NULL, AddAppName(_("%s Commandline Options")) );
	popup.SetMinWidth( 640 );
	popup += popup.Heading(out.Mid(0, pos));
	//popup += ;
	//popup += popup.Text(out.Mid(pos, out.Length())).Align( wxALIGN_LEFT ) | pxExpand.Border(wxALL, StdPadding*3);

	wxTextCtrl* traceArea = new wxTextCtrl(
		&popup, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
		wxTE_READONLY | wxTE_MULTILINE | wxTE_RICH2 | wxHSCROLL
	);

	traceArea->SetDefaultStyle( wxTextAttr( wxNullColour, wxNullColour, pxGetFixedFont(9) ) );
	traceArea->SetFont( pxGetFixedFont(9) );

	int fonty = traceArea->GetCharHeight();

	traceArea->SetMinSize( wxSize( traceArea->GetMinWidth(), (fonty+1)*18 ) );
	traceArea->WriteText( pxTextWrapper(wxString(L' ', 18)).Wrap(traceArea, out.Mid(pos, out.Length()), 600).GetResult() );
	traceArea->SetInsertionPoint( 0 );
	traceArea->ShowPosition( 0 );

	popup += traceArea	| pxExpand.Border(wxALL, StdPadding*3);

	pxIssueConfirmation(popup, MsgButtons().Close() );
}
#endif

wxMessageOutput* Pcsx2AppTraits::CreateMessageOutput()
{
#ifdef __UNIX__
	return _parent::CreateMessageOutput();
#else
	return new pxMessageOutputMessageBox;
#endif
}

// --------------------------------------------------------------------------------------
//  Pcsx2StandardPaths
// --------------------------------------------------------------------------------------
#ifdef wxUSE_STDPATHS
#ifndef __APPLE__ // macOS uses wx's defaults
class Pcsx2StandardPaths : public wxStandardPaths
{
public:
	wxString GetResourcesDir() const
	{
		return Path::CombineWx( GetDataDir(), L"Langs" );
	}

#ifdef __POSIX__
	wxString GetUserLocalDataDir() const
	{
		// I got memory corruption inside wxGetEnv when I heavily toggle the GS renderer (F9). It seems wxGetEnv
		// isn't thread safe? To avoid any issue on this read only variable, I cache the result.
		static wxString cache_dir;
		if (!cache_dir.IsEmpty()) return cache_dir;

		// Note: GetUserLocalDataDir() on linux return $HOME/.pcsx2 unfortunately it does not follow the XDG standard
		// So we re-implement it, to follow the standard.
		wxDirName user_local_dir;
		wxDirName default_config_dir = (wxDirName)Path::CombineWx( L".config", pxGetAppName() );
		wxString xdg_home_value;
		if( wxGetEnv(L"XDG_CONFIG_HOME", &xdg_home_value) ) {
			if ( xdg_home_value.IsEmpty() ) {
				// variable exist but it is empty. So use the default value
				user_local_dir = (wxDirName)Path::CombineWx( GetUserConfigDir() , default_config_dir);
			} else {
				user_local_dir = (wxDirName)Path::CombineWx( xdg_home_value, pxGetAppName());
			}
		} else {
			// variable do not exist
			user_local_dir = (wxDirName)Path::CombineWx( GetUserConfigDir() , default_config_dir);
		}

		cache_dir = user_local_dir.ToString();

		return cache_dir;
	}
#endif

};
#endif // ifdef __APPLE__

wxStandardPaths& Pcsx2AppTraits::GetStandardPaths()
{
#ifdef __APPLE__
	return _parent::GetStandardPaths();
#else
	static Pcsx2StandardPaths stdPaths;
	return stdPaths;
#endif
}
#endif

wxAppTraits* Pcsx2App::CreateTraits()
{
	return new Pcsx2AppTraits;
}

// ----------------------------------------------------------------------------
//         Pcsx2App Event Handlers
// ----------------------------------------------------------------------------

// LogicalVsync - Event received from the AppCoreThread (EEcore) for each vsync,
// roughly 50/60 times a second when frame limiting is enabled, and up to 10,000 
// times a second if not (ok, not quite, but you get the idea... I hope.)
void Pcsx2App::LogicalVsync()
{
	if( AppRpc_TryInvokeAsync( &Pcsx2App::LogicalVsync ) ) return;

	if( !SysHasValidState() ) return;

	if( (wxGetApp().GetGsFramePtr() != NULL) )
		PADupdate(0);

	while( const HostKeyEvent* ev = PADkeyEvent() )
	{
		if( ev->key == 0 ) break;

		// in the past, in the plugin api, all plugins would have a first chance at treating the 
		// input here, with the ui eventually dealing with it otherwise. Obviously this solution
		// sucked and we had multiple components battling for input processing. I managed to make
		// most of them go away during the plugin merge but GS still needs to process the inputs,
		// we might want to move all the input handling in a frontend-specific file in the future -- govanify
		GSkeyEvent(*ev);
		PadKeyDispatch( *ev );
	}
}

void Pcsx2App::OnEmuKeyDown( wxKeyEvent& evt )
{
	const GlobalCommandDescriptor* cmd = NULL;
	if (GlobalAccels)
	{
		std::unordered_map<int, const GlobalCommandDescriptor*>::const_iterator iter(GlobalAccels->find(KeyAcceleratorCode(evt).val32));
		if (iter != GlobalAccels->end())
			cmd = iter->second;
	}

	if( cmd == NULL )
	{
		evt.Skip();
		return;
	}

	DbgCon.WriteLn( "(app) Invoking command: %s", cmd->Id );
	cmd->Invoke();
}

void Pcsx2App::HandleEvent(wxEvtHandler* handler, wxEventFunction func, wxEvent& event) const
{
	const_cast<Pcsx2App*>(this)->HandleEvent( handler, func, event );
}

void Pcsx2App::HandleEvent(wxEvtHandler* handler, wxEventFunction func, wxEvent& event)
{
	try
	{
		if (g_Conf->EmuOptions.EnableRecordingTools)
		{
			if (g_InputRecordingControls.IsPaused())
			{
				// When the GSFrame CoreThread is paused, so is the logical VSync
				// Meaning that we have to grab the user-input through here to potentially
				// resume emulation.
				if (const HostKeyEvent* ev = PADkeyEvent() )
				{
					if( ev->key != 0 )
					{
						PadKeyDispatch( *ev );
					}
				}
			}
			g_InputRecordingControls.ResumeCoreThreadIfStarted();
		}
		(handler->*func)(event);
	}
	// ----------------------------------------------------------------------------
	catch( Exception::StartupAborted& ex )		// user-aborted, no popups needed.
	{
		Console.Warning( ex.FormatDiagnosticMessage() );
		Exit();
	}
	// ----------------------------------------------------------------------------
	catch( Exception::SaveStateLoadError& ex)
	{
		// Saved state load failed prior to the system getting corrupted (ie, file not found
		// or some zipfile error) -- so log it and resume emulation.
		Console.Warning( ex.FormatDiagnosticMessage() );
		if (g_InputRecording.IsInitialLoad())
			g_InputRecording.FailedSavestate();

		CoreThread.Resume();
	}
	// ----------------------------------------------------------------------------
	#if 0
	catch( Exception::ThreadDeadlock& ex )
	{
		// [TODO]  Bind a listener to the CoreThread status, and automatically close the dialog
		// if the thread starts responding while we're waiting (not hard in fact, but I'm getting
		// a little tired, so maybe later!)  --air
	
		Console.Warning( ex.FormatDiagnosticMessage() );
		wxDialogWithHelpers dialog( NULL, _("PCSX2 Unresponsive Thread"), wxVERTICAL );
		
		dialog += dialog.Heading( ex.FormatDisplayMessage() + L"\n\n" +
			pxE( L"'Ignore' to continue waiting for the thread to respond.\n'Cancel' to attempt to cancel the thread.\n'Terminate' to quit PCSX2 immediately.\n"
			)
		);

		int result = pxIssueConfirmation( dialog, MsgButtons().Ignore().Cancel().Custom( _("Terminate") ) );
		
		if( result == pxID_CUSTOM )
		{
			// fastest way to kill the process! (works in Linux and win32, thanks to windows having very
			// limited Posix Signals support)
			//
			// (note: SIGTERM is a "handled" kill that performs shutdown stuff, which typically just crashes anyway)
			wxKill( wxGetProcessId(), wxSIGKILL );
		}
		else if( result == wxID_CANCEL )
		{
			// Attempt to cancel the thread:
			ex.Thread().Cancel();
		}

		// Ignore does nothing...
	}
	#endif
	// ----------------------------------------------------------------------------
	catch( Exception::CancelEvent& ex )
	{
		Console.Warning( ex.FormatDiagnosticMessage() );
	}
	catch( Exception::RuntimeError& ex )
	{
		// Runtime errors which have been unhandled should still be safe to recover from,
		// so lets issue a message to the user and then continue the message pump.

		// Test case (Windows only, Linux has an uncaught exception for some
		// reason): Run PSX ISO using fast boot
		if (GSFrame* gsframe = wxGetApp().GetGsFramePtr())
			gsframe->Close();

		Console.Error( ex.FormatDiagnosticMessage() );
		// I should probably figure out how to have the error message as well.
		if (wxGetApp().HasGUI())
			Msgbox::Alert(StringUtil::UTF8StringToWxString(ex.FormatDisplayMessage()));
	}
}

bool Pcsx2App::HasPendingSaves() const
{
	AffinityAssert_AllowFrom_MainUI();
	return !!m_PendingSaves;
}

// A call to this method informs the app that there is a pending save operation that must be
// finished prior to exiting the app, or else data loss will occur.  Any call to this method
// should be matched by a call to ClearPendingSave().
void Pcsx2App::StartPendingSave()
{
	if( AppRpc_TryInvokeAsync(&Pcsx2App::StartPendingSave) ) return;
	++m_PendingSaves;
}

// If this method is called inappropriately then the entire pending save system will become
// unreliable and data loss can occur on app exit.  Devel and debug builds will assert if
// such calls are detected (though the detection is far from fool-proof).
void Pcsx2App::ClearPendingSave()
{
	if( AppRpc_TryInvokeAsync(&Pcsx2App::ClearPendingSave) ) return;

	--m_PendingSaves;
	pxAssertDev( m_PendingSaves >= 0, "Pending saves count mismatch (pending count is less than 0)" );

	if( (m_PendingSaves == 0) && m_ScheduledTermination )
	{
		Console.WriteLn( "App: All pending saves completed; exiting!" );
		Exit();
	}
}

// This method generates debug assertions if the MainFrame handle is NULL (typically
// indicating that PCSX2 is running in NoGUI mode, or that the main frame has been
// closed).  In most cases you'll want to use HasMainFrame() to test for thread
// validity first, or use GetMainFramePtr() and manually check for NULL (choice
// is a matter of programmer preference).
MainEmuFrame& Pcsx2App::GetMainFrame() const
{
	MainEmuFrame* mainFrame = GetMainFramePtr();

	pxAssert(mainFrame != NULL);
	pxAssert(((uptr)GetTopWindow()) == ((uptr)mainFrame));
	return  *mainFrame;
}

GSFrame& Pcsx2App::GetGsFrame() const
{
	GSFrame* gsFrame  = (GSFrame*)wxWindow::FindWindowById( m_id_GsFrame );
	pxAssert(gsFrame != NULL);
	return  *gsFrame;
}

void Pcsx2App::enterDebugMode()
{
	DisassemblyDialog* dlg = GetDisassemblyPtr();
	if (dlg)
		dlg->setDebugMode(true,false);
}
	
void Pcsx2App::leaveDebugMode()
{
	DisassemblyDialog* dlg = GetDisassemblyPtr();
	if (dlg)
		dlg->setDebugMode(false,false);
}

void Pcsx2App::resetDebugger()
{
	DisassemblyDialog* dlg = GetDisassemblyPtr();
	if (dlg)
		dlg->reset();
}

void AppApplySettings( const AppConfig* oldconf )
{
	AffinityAssert_AllowFrom_MainUI();

	ScopedCoreThreadPause paused_core;

	g_Conf->Folders.ApplyDefaults();

	// Ensure existence of necessary documents folders.
	// Other parts of PCSX2 rely on them.

	g_Conf->Folders.MemoryCards.Mkdir();
	g_Conf->Folders.Savestates.Mkdir();
	g_Conf->Folders.Snapshots.Mkdir();
	g_Conf->Folders.Cheats.Mkdir();
	g_Conf->Folders.CheatsWS.Mkdir();
	g_Conf->Folders.Textures.Mkdir();

	RelocateLogfile();

	if( (oldconf == NULL) || (oldconf->LanguageCode.CmpNoCase(g_Conf->LanguageCode)) )
	{
		wxDoNotLogInThisScope please;
		i18n_SetLanguage( g_Conf->LanguageId, g_Conf->LanguageCode );
	}

	// Update the compression attribute on the Memcards folder.
	// Memcards generally compress very well via NTFS compression.

	#ifdef _WIN32
	FileSystem::SetPathCompression( g_Conf->Folders.MemoryCards.ToUTF8(), g_Conf->EmuOptions.McdCompressNTFS );
	#endif
	sApp.DispatchEvent( AppStatus_SettingsApplied );

	paused_core.AllowResume();
}

// Invokes the specified Pcsx2App method, or posts the method to the main thread if the calling
// thread is not Main.  Action is blocking.  For non-blocking method execution, use
// AppRpc_TryInvokeAsync.
//
// This function works something like setjmp/longjmp, in that the return value indicates if the
// function actually executed the specified method or not.
//
// Returns:
//   FALSE if the method was not posted to the main thread (meaning this IS the main thread!)
//   TRUE if the method was posted.
//
bool Pcsx2App::AppRpc_TryInvoke( FnPtr_Pcsx2App method )
{
	if( wxThread::IsMain() ) return false;

	SynchronousActionState sync;
	PostEvent( Pcsx2AppMethodEvent( method, sync ) );
	sync.WaitForResult();

	return true;
}

// Invokes the specified Pcsx2App method, or posts the method to the main thread if the calling
// thread is not Main.  Action is non-blocking.  For blocking method execution, use
// AppRpc_TryInvoke.
//
// This function works something like setjmp/longjmp, in that the return value indicates if the
// function actually executed the specified method or not.
//
// Returns:
//   FALSE if the method was not posted to the main thread (meaning this IS the main thread!)
//   TRUE if the method was posted.
//
bool Pcsx2App::AppRpc_TryInvokeAsync( FnPtr_Pcsx2App method )
{
	if( wxThread::IsMain() ) return false;
	PostEvent( Pcsx2AppMethodEvent( method ) );
	return true;
}

// Posts a method to the main thread; non-blocking.  Post occurs even when called from the
// main thread.
void Pcsx2App::PostAppMethod( FnPtr_Pcsx2App method )
{
	PostEvent( Pcsx2AppMethodEvent( method ) );
}

// Posts a method to the main thread; non-blocking.  Post occurs even when called from the
// main thread.
void Pcsx2App::PostIdleAppMethod( FnPtr_Pcsx2App method )
{
	Pcsx2AppMethodEvent evt( method );
	AddIdleEvent( evt );
}

SysMainMemory& Pcsx2App::GetVmReserve()
{
	if (!m_VmReserve) m_VmReserve = std::unique_ptr<SysMainMemory>(new SysMainMemory());
	return *m_VmReserve;
}

void Pcsx2App::OpenGsPanel()
{
	if( AppRpc_TryInvoke( &Pcsx2App::OpenGsPanel ) ) return;

	GSFrame* gsFrame = GetGsFramePtr();
	if( gsFrame == NULL )
	{
		gsFrame = new GSFrame(GetAppName() );
		m_id_GsFrame = gsFrame->GetId();

		switch( wxGetApp().Overrides.GsWindowMode )
		{
			case GsWinMode_Windowed:
				g_Conf->GSWindow.IsFullscreen = false;
			break;

			case GsWinMode_Fullscreen:
				g_Conf->GSWindow.IsFullscreen = true;
			break;

			case GsWinMode_Unspecified:
				g_Conf->GSWindow.IsFullscreen = g_Conf->GSWindow.DefaultToFullscreen;
			break;
		}
	}
	else
	{
		// This is an attempt to hackfix a bug in nvidia's 195.xx drivers: When using
		// Aero and DX10, the driver fails to update the window after the device has changed,
		// until some event like a hide/show or resize event is posted to the window.
		// Presumably this forces the driver to re-cache the visibility info.
		// Notes:
		//   Doing an immediate hide/show didn't work.  So now I'm trying a resize.  Because
		//   wxWidgets is "clever" (grr!) it optimizes out just force-setting the same size
		//   over again, so instead I resize it to size-1 and then back to the original size.
		//
		// FIXME: GS memory leaks in DX10 have been fixed.  This code may not be needed
		// anymore.
		
		const wxSize oldsize( gsFrame->GetSize() );
		wxSize newsize( oldsize );
		newsize.DecBy(1);

		gsFrame->SetSize( newsize );
		gsFrame->SetSize( oldsize );
	}

	gsFrame->ShowFullScreen(g_Conf->GSWindow.IsFullscreen);
	wxApp::ProcessPendingEvents();

	std::optional<WindowInfo> wi = gsFrame->GetViewport()->GetWindowInfo();
	pxAssertDev(wi.has_value(), "GS frame has a valid native window");
	g_gs_window_info = std::move(*wi);

	// Enable New & Play after the first game load of the session
	sMainFrame.enableRecordingMenuItem(MenuId_Recording_New, !g_InputRecording.IsActive());
	sMainFrame.enableRecordingMenuItem(MenuId_Recording_Play, true);

	// Enable recording menu options as the game is now running
	sMainFrame.enableRecordingMenuItem(MenuId_Recording_FrameAdvance, true);
	sMainFrame.enableRecordingMenuItem(MenuId_Recording_TogglePause, true);
	sMainFrame.enableRecordingMenuItem(MenuId_Recording_ToggleRecordingMode, g_InputRecording.IsActive());
}


void Pcsx2App::CloseGsPanel()
{
	if (AppRpc_TryInvoke(&Pcsx2App::CloseGsPanel))
		return;

	GSFrame* gsFrame = GetGsFramePtr();
	if (gsFrame)
	{
		// we unreference the window first, that way it doesn't try to suspend on close and deadlock
		OnGsFrameDestroyed(gsFrame->GetId());
		gsFrame->Destroy();
	}
}

void Pcsx2App::OnGsFrameClosed(wxWindowID id)
{
	if ((m_id_GsFrame == wxID_ANY) || (m_id_GsFrame != id))
		return;

	CoreThread.Suspend();

	if (!m_UseGUI)
	{
		// The user is prompted before suspending (at Sys_Suspend() ), because
		// right now there's no way to resume from suspend without GUI.
		PrepForExit();
	}
}

void Pcsx2App::OnGsFrameDestroyed(wxWindowID id)
{
	if ((m_id_GsFrame == wxID_ANY) || (m_id_GsFrame != id))
		return;

	m_id_GsFrame = wxID_ANY;
	g_gs_window_info = {};

	// Disable recording controls that only make sense if the game is running
	sMainFrame.enableRecordingMenuItem(MenuId_Recording_FrameAdvance, false);
	sMainFrame.enableRecordingMenuItem(MenuId_Recording_TogglePause, false);
	sMainFrame.enableRecordingMenuItem(MenuId_Recording_ToggleRecordingMode, false);
}

void Pcsx2App::OnProgramLogClosed( wxWindowID id )
{
	if( (m_id_ProgramLogBox == wxID_ANY) || (m_id_ProgramLogBox != id) ) return;

	ScopedLock lock( m_mtx_ProgramLog );
	m_id_ProgramLogBox = wxID_ANY;
	DisableWindowLogging();
}

void Pcsx2App::OnMainFrameClosed( wxWindowID id )
{
	if (g_InputRecording.IsActive())
	{
		g_InputRecording.Stop();
	}

	// Nothing threaded depends on the mainframe (yet) -- it all passes through the main wxApp
	// message handler.  But that might change in the future.
	if( m_id_MainFrame != id ) return;
	m_id_MainFrame = wxID_ANY;
}

// --------------------------------------------------------------------------------------
//  SysExecEvent_Execute
// --------------------------------------------------------------------------------------
class SysExecEvent_Execute : public SysExecEvent
{
protected:
	bool				m_UseCDVDsrc;
	bool				m_UseELFOverride;
	CDVD_SourceType		m_cdvdsrc_type;
	wxString			m_elf_override;

public:
	virtual ~SysExecEvent_Execute() = default;
	SysExecEvent_Execute* Clone() const { return new SysExecEvent_Execute(*this); }

	wxString GetEventName() const
	{
		return L"SysExecute";
	}

	wxString GetEventMessage() const
	{
		return _("Executing PS2 Virtual Machine...");
	}
	
	SysExecEvent_Execute()
		: m_UseCDVDsrc(false)
		, m_UseELFOverride(false)
		, m_cdvdsrc_type(CDVD_SourceType::Iso)
	{
	}

	SysExecEvent_Execute( CDVD_SourceType srctype, const wxString& elf_override )
		: m_UseCDVDsrc(true)
		, m_UseELFOverride(true)
		, m_cdvdsrc_type(srctype)
		, m_elf_override( elf_override )
	{
	}

protected:
	void InvokeEvent()
	{
		wxGetApp().ProcessMethod( AppSaveSettings );

		DbgCon.WriteLn( Color_Gray, "(SysExecute) received." );

		CoreThread.ResetQuick();
		R5900SymbolMap.Clear();
		R3000SymbolMap.Clear();
		CBreakPoints::SetSkipFirst(BREAKPOINT_EE, 0);
		CBreakPoints::SetSkipFirst(BREAKPOINT_IOP, 0);
		// This function below gets called again from AppCoreThread.cpp and will pass the current ISO regardless if we
		// are starting an ELF. In terms of symbol loading this doesn't matter because AppCoreThread.cpp doesn't clear the symbol map
		// and we _only_ read symbols if the map is empty
		CDVDsys_SetFile(CDVD_SourceType::Disc, StringUtil::wxStringToUTF8String(g_Conf->Folders.RunDisc) );
		CDVDsys_SetFile(CDVD_SourceType::Iso, StringUtil::wxStringToUTF8String(m_UseELFOverride ? m_elf_override : g_Conf->CurrentIso) );
		if( m_UseCDVDsrc )
			CDVDsys_ChangeSource( m_cdvdsrc_type );
		else if( CDVD == NULL )
			CDVDsys_ChangeSource(CDVD_SourceType::NoDisc);

		if( m_UseELFOverride && !CoreThread.HasActiveMachine() )
			CoreThread.SetElfOverride( m_elf_override );

		CoreThread.Resume();
	}
};

// This command performs a full closure of any existing VM state and starts a
// fresh VM with the requested sources.
void Pcsx2App::SysExecute()
{
	if (!CheckForBIOS())
		return;

	SysExecutorThread.PostEvent( new SysExecEvent_Execute() );
}

// Executes the specified cdvd source and optional elf file.  This command performs a
// full closure of any existing VM state and starts a fresh VM with the requested
// sources.
void Pcsx2App::SysExecute( CDVD_SourceType cdvdsrc, const wxString& elf_override )
{
	if (!CheckForBIOS())
		return;

	SysExecutorThread.PostEvent( new SysExecEvent_Execute(cdvdsrc, elf_override) );
	if (g_Conf->EmuOptions.EnableRecordingTools)
	{
		g_InputRecording.RecordingReset();
	}
}

// Returns true if there is a "valid" virtual machine state from the user's perspective.  This
// means the user has started the emulator and not issued a full reset.
// Thread Safety: The state of the system can change in parallel to execution of the
// main thread.  If you need to perform an extended length activity on the execution
// state (such as saving it), you *must* suspend the Corethread first!
__fi bool SysHasValidState()
{
	return CoreThread.HasActiveMachine();
}

// Writes text to console and updates the window status bar and/or HUD or whateverness.
// FIXME: This probably isn't thread safe. >_<
void SysStatus( const wxString& text )
{
	// mirror output to the console!
	Console.WriteLn( text.ToStdString() );
	sMainFrame.SetStatusText( text );
}

// Applies a new active iso source file
void SysUpdateIsoSrcFile( const wxString& newIsoFile )
{
	g_Conf->CurrentIso = newIsoFile;
	sMainFrame.UpdateStatusBar();
	sMainFrame.UpdateCdvdSrcSelection();
}

void SysUpdateDiscSrcDrive( const wxString& newDiscDrive )
{
	g_Conf->Folders.RunDisc = newDiscDrive;
	AppSaveSettings();
	sMainFrame.UpdateCdvdSrcSelection();
}

bool HasMainFrame()
{
	return wxTheApp && wxGetApp().HasMainFrame();
}

// This method generates debug assertions if either the wxApp or MainFrame handles are
// NULL (typically indicating that PCSX2 is running in NoGUI mode, or that the main
// frame has been closed).  In most cases you'll want to use HasMainFrame() to test
// for gui validity first, or use GetMainFramePtr() and manually check for NULL (choice
// is a matter of programmer preference).
MainEmuFrame& GetMainFrame()
{
	return wxGetApp().GetMainFrame();
}

// Returns a pointer to the main frame of the GUI (frame may be hidden from view), or
// NULL if no main frame exists (NoGUI mode and/or the frame has been destroyed).  If
// the wxApp is NULL then this will also return NULL.
MainEmuFrame* GetMainFramePtr()
{
	return wxTheApp ? wxGetApp().GetMainFramePtr() : NULL;
}

SysMainMemory& GetVmMemory()
{
	return wxGetApp().GetVmReserve();
}

SysCpuProviderPack& GetCpuProviders()
{
	return *wxGetApp().m_CpuProviders;
}
