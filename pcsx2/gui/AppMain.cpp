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
#include "MainFrame.h"
#include "GSFrame.h"
#include "GS.h"
#include "AppSaveStates.h"
#include "AppGameDatabase.h"
#include "AppAccelerators.h"

#include "Plugins.h"
#include "ps2/BiosTools.h"

#include "Dialogs/ModalPopups.h"
#include "Dialogs/ConfigurationDialog.h"
#include "Dialogs/LogOptionsDialog.h"

#include "Debugger/DisassemblyDialog.h"

#ifndef DISABLE_RECORDING
#	include "Recording/InputRecordingControls.h"
#	include "Recording/InputRecording.h"
#endif

#include "Utilities/IniInterface.h"
#include "Utilities/AppTrait.h"

#include <wx/stdpaths.h>

#ifdef __WXMSW__
#	include <wx/msw/wrapwin.h>		// needed to implement the app!
#endif

#ifdef __WXGTK__
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
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

AspectRatioType iniAR;
bool switchAR;

static bool HandlePluginError( BaseException& ex )
{
	if (!pxDialogExists(L"Dialog:" + Dialogs::ComponentsConfigDialog::GetNameStatic()))
	{
		if( !Msgbox::OkCancel( ex.FormatDisplayMessage() +
				_("\n\nPress Ok to go to the Plugin Configuration Panel.")
			) )
		return false;
	}
	else
	{
		Msgbox::Alert(ex.FormatDisplayMessage());
	}

	g_Conf->ComponentsTabName = L"Plugins";

	// TODO: Send a message to the panel to select the failed plugin.

	return AppOpenModalDialog<Dialogs::ComponentsConfigDialog>(L"Plugins") != wxID_CANCEL;
}

class PluginErrorEvent : public pxExceptionEvent
{
	typedef pxExceptionEvent _parent;

public:
	PluginErrorEvent( BaseException* ex=NULL ) : _parent( ex ) {}
	PluginErrorEvent( const BaseException& ex ) : _parent( ex ) {}

	virtual ~PluginErrorEvent() = default;
	virtual PluginErrorEvent *Clone() const { return new PluginErrorEvent(*this); }

protected:
	void InvokeEvent();
};

class PluginInitErrorEvent : public pxExceptionEvent
{
	typedef pxExceptionEvent _parent;

public:
	PluginInitErrorEvent( BaseException* ex=NULL ) : _parent( ex ) {}
	PluginInitErrorEvent( const BaseException& ex ) : _parent( ex ) {}

	virtual ~PluginInitErrorEvent() = default;
	virtual PluginInitErrorEvent *Clone() const { return new PluginInitErrorEvent(*this); }

protected:
	void InvokeEvent();

};

void PluginErrorEvent::InvokeEvent()
{
	if( !m_except ) return;

	ScopedExcept deleteMe( m_except );
	m_except = NULL;

	if( !HandlePluginError( *deleteMe ) )
	{
		Console.Error( L"User-canceled plugin configuration; Plugins not loaded!" );
		Msgbox::Alert( _("Warning!  System plugins have not been loaded.  PCSX2 may be inoperable.") );
	}
}

void PluginInitErrorEvent::InvokeEvent()
{
	if( !m_except ) return;

	ScopedExcept deleteMe( m_except );
	m_except = NULL;

	if( !HandlePluginError( *deleteMe ) )
	{
		Console.Error( L"User-canceled plugin configuration after plugin initialization failure.  Plugins unloaded." );
		Msgbox::Alert( _("Warning!  System plugins have not been loaded.  PCSX2 may be inoperable.") );
	}
}

// Returns a string message telling the user to consult guides for obtaining a legal BIOS.
// This message is in a function because it's used as part of several dialogs in PCSX2 (there
// are multiple variations on the BIOS and BIOS folder checks).
wxString BIOS_GetMsg_Required()
{
	return pxE(L"PCSX2 requires a PS2 BIOS in order to run.  For legal reasons, you *must* obtain a BIOS from an actual PS2 unit that you own (borrowing doesn't count).  Please consult the FAQs and Guides for further instructions."
		);
}

class BIOSLoadErrorEvent : public pxExceptionEvent
{
	typedef pxExceptionEvent _parent;

public:
	BIOSLoadErrorEvent(BaseException* ex = NULL) : _parent(ex) {}
	BIOSLoadErrorEvent(const BaseException& ex) : _parent(ex) {}

	virtual ~BIOSLoadErrorEvent() = default;
	virtual BIOSLoadErrorEvent *Clone() const { return new BIOSLoadErrorEvent(*this); }

protected:
	void InvokeEvent();

};

static bool HandleBIOSError(BaseException& ex)
{
	if (!pxDialogExists(L"Dialog:" + Dialogs::ComponentsConfigDialog::GetNameStatic()))
	{
		if (!Msgbox::OkCancel(ex.FormatDisplayMessage() + L"\n\n" + BIOS_GetMsg_Required()
			+ L"\n\n" + _("Press Ok to go to the BIOS Configuration Panel."), _("PS2 BIOS Error")))
			return false;
	}
	else
	{
		Msgbox::Alert(ex.FormatDisplayMessage() + L"\n\n" + BIOS_GetMsg_Required(), _("PS2 BIOS Error"));
	}

	g_Conf->ComponentsTabName = L"BIOS";

	return AppOpenModalDialog<Dialogs::ComponentsConfigDialog>(L"BIOS") != wxID_CANCEL;
}

void BIOSLoadErrorEvent::InvokeEvent()
{
	if (!m_except) return;

	ScopedExcept deleteMe(m_except);
	m_except = NULL;

	if (!HandleBIOSError(*deleteMe))
	{
		Console.Warning("User canceled BIOS configuration.");
		Msgbox::Alert(_("Warning! Valid BIOS has not been selected. PCSX2 may be inoperable."));
	}
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
extern int TranslateVKToWXK( u32 keysym );
#elif defined( __WXGTK__ )
extern int TranslateGDKtoWXK( u32 keysym );
#endif

void Pcsx2App::PadKeyDispatch( const keyEvent& ev )
{
	m_kevt.SetEventType( ( ev.evt == KEYPRESS ) ? wxEVT_KEY_DOWN : wxEVT_KEY_UP );

//returns 0 for normal keys and a WXK_* value for special keys
#ifdef __WXMSW__
	const int vkey = TranslateVKToWXK(ev.key);
#elif defined( __WXMAC__ )
	const int vkey = wxCharCodeWXToOSX( (wxKeyCode) ev.key );
#elif defined( __WXGTK__ )
	const int vkey = TranslateGDKtoWXK( ev.key );
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

		Console.WriteLn(wxString(L"> Key: %s (Code: %ld)"),	WX_STR(strFromCode), m_kevt.m_keyCode);
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
class Pcsx2StandardPaths : public wxStandardPaths
{
public:
	wxString GetResourcesDir() const
	{
		return Path::Combine( GetDataDir(), L"Langs" );
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
		wxDirName default_config_dir = (wxDirName)Path::Combine( L".config", pxGetAppName() );
		wxString xdg_home_value;
		if( wxGetEnv(L"XDG_CONFIG_HOME", &xdg_home_value) ) {
			if ( xdg_home_value.IsEmpty() ) {
				// variable exist but it is empty. So use the default value
				user_local_dir = (wxDirName)Path::Combine( GetUserConfigDir() , default_config_dir);
			} else {
				user_local_dir = (wxDirName)Path::Combine( xdg_home_value, pxGetAppName());
			}
		} else {
			// variable do not exist
			user_local_dir = (wxDirName)Path::Combine( GetUserConfigDir() , default_config_dir);
		}

		cache_dir = user_local_dir.ToString();

		return cache_dir;
	}
#endif

};

wxStandardPaths& Pcsx2AppTraits::GetStandardPaths()
{
	static Pcsx2StandardPaths stdPaths;
	return stdPaths;
}
#endif

wxAppTraits* Pcsx2App::CreateTraits()
{
	return new Pcsx2AppTraits;
}

// --------------------------------------------------------------------------------------
//  FramerateManager  (implementations)
// --------------------------------------------------------------------------------------
void FramerateManager::Reset()
{
	//memzero( m_fpsqueue );
	m_initpause = FramerateQueueDepth;
	m_fpsqueue_writepos = 0;

	for( uint i=0; i<FramerateQueueDepth; ++i )
		m_fpsqueue[i] = GetCPUTicks();

	Resume();
}

// 
void FramerateManager::Resume()
{
}

void FramerateManager::DoFrame()
{
	m_fpsqueue_writepos = (m_fpsqueue_writepos + 1) % FramerateQueueDepth;
	m_fpsqueue[m_fpsqueue_writepos] = GetCPUTicks();

	// intentionally leave 1 on the counter here, since ultimately we want to divide the 
	// final result (in GetFramerate() by QueueDepth-1.
	if( m_initpause > 1 ) --m_initpause;
}

double FramerateManager::GetFramerate() const
{
	if( m_initpause > (FramerateQueueDepth/2) ) return 0.0;
	const u64 delta = m_fpsqueue[m_fpsqueue_writepos] - m_fpsqueue[(m_fpsqueue_writepos + 1) % FramerateQueueDepth];
	const u32 ticks_per_frame = (u32)(delta / (FramerateQueueDepth-m_initpause));
	return (double)GetTickFrequency() / (double)ticks_per_frame;
}

// ----------------------------------------------------------------------------
//         Pcsx2App Event Handlers
// ----------------------------------------------------------------------------

// LogicalVsync - Event received from the AppCoreThread (EEcore) for each vsync,
// roughly 50/60 times a second when frame limiting is enabled, and up to 10,000 
// times a second if not (ok, not quite, but you get the idea... I hope.)
extern uint eecount_on_last_vdec;
extern bool FMVstarted;
extern bool EnableFMV;
extern bool renderswitch;
extern uint renderswitch_delay;

void DoFmvSwitch(bool on)
{
	if (g_Conf->GSWindow.FMVAspectRatioSwitch != FMV_AspectRatio_Switch_Off) {
		if (on) {
			switchAR = true;
			iniAR = g_Conf->GSWindow.AspectRatio;
		} else {
			switchAR = false;
		}
		if (GSFrame* gsFrame = wxGetApp().GetGsFramePtr())
			if (GSPanel* viewport = gsFrame->GetViewport())
				viewport->DoResize();
	}

	if (EmuConfig.Gamefixes.FMVinSoftwareHack) {
		ScopedCoreThreadPause paused_core(new SysExecEvent_SaveSinglePlugin(PluginId_GS));
		renderswitch = !renderswitch;
		paused_core.AllowResume();
	}
}

void Pcsx2App::LogicalVsync()
{
	if( AppRpc_TryInvokeAsync( &Pcsx2App::LogicalVsync ) ) return;

	if( !SysHasValidState() ) return;

	// Update / Calculate framerate!

	FpsManager.DoFrame();

	if (EmuConfig.Gamefixes.FMVinSoftwareHack || g_Conf->GSWindow.FMVAspectRatioSwitch != FMV_AspectRatio_Switch_Off) {
		if (EnableFMV) {
			DevCon.Warning("FMV on");
			DoFmvSwitch(true);
			EnableFMV = false;
		}

		if (FMVstarted) {
			int diff = cpuRegs.cycle - eecount_on_last_vdec;
			if (diff > 60000000 ) {
				DevCon.Warning("FMV off");
				DoFmvSwitch(false);
				FMVstarted = false;
			}
		}
	}

	renderswitch_delay >>= 1;

	// Only call PADupdate here if we're using GSopen2.  Legacy GSopen plugins have the
	// GS window belonging to the MTGS thread.
	if( (PADupdate != NULL) && (GSopen2 != NULL) && (wxGetApp().GetGsFramePtr() != NULL) )
		PADupdate(0);

	while( const keyEvent* ev = PADkeyEvent() )
	{
		if( ev->key == 0 ) break;

		// Give plugins first try to handle keys.  If none of them handles the key, it will
		// be passed to the main user interface.

		if( !GetCorePlugins().KeyEvent( *ev ) )
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
#ifndef DISABLE_RECORDING
		if (g_Conf->EmuOptions.EnableRecordingTools)
		{
			if (g_InputRecordingControls.IsPaused())
			{
				// When the GSFrame CoreThread is paused, so is the logical VSync
				// Meaning that we have to grab the user-input through here to potentially
				// resume emulation.
				if (const keyEvent* ev = PADkeyEvent() )
				{
					if( ev->key != 0 )
					{
						PadKeyDispatch( *ev );
					}
				}
			}
			g_InputRecordingControls.ResumeCoreThreadIfStarted();
		}
#endif
		(handler->*func)(event);
	}
	// ----------------------------------------------------------------------------
	catch( Exception::StartupAborted& ex )		// user-aborted, no popups needed.
	{
		Console.Warning( ex.FormatDiagnosticMessage() );
		Exit();
	}
	// ----------------------------------------------------------------------------
	catch( Exception::BiosLoadFailed& ex )
	{
		// Commandline 'nogui' users will not receive an error message, but at least PCSX2 will
		// terminate properly.
		GSFrame* gsframe = wxGetApp().GetGsFramePtr();
		gsframe->Close();

		Console.Error(ex.FormatDiagnosticMessage());

		if (wxGetApp().HasGUI())
			AddIdleEvent(BIOSLoadErrorEvent(ex));
	}
	// ----------------------------------------------------------------------------
	catch( Exception::SaveStateLoadError& ex)
	{
		// Saved state load failed prior to the system getting corrupted (ie, file not found
		// or some zipfile error) -- so log it and resume emulation.
		Console.Warning( ex.FormatDiagnosticMessage() );
		CoreThread.Resume();
	}
	// ----------------------------------------------------------------------------
	catch( Exception::PluginOpenError& ex )
	{
		// It might be possible for there to be no GS Frame, but I don't really know. This does
		// prevent PCSX2 from locking up on a Windows wxWidgets 3.0 build. My conjecture is this:
		// 1. Messagebox appears
		// 2. Either a close or hide signal for gsframe gets sent to messagebox.
		// 3. Message box hides itself without exiting the modal event loop, therefore locking up
		// PCSX2. This probably happened in the BIOS error case above as well.
		// So the idea is to explicitly close the gsFrame before the modal MessageBox appears and
		// intercepts the close message. Only for wx3.0 though - it sometimes breaks linux wx2.8.

		if (GSFrame* gsframe = wxGetApp().GetGsFramePtr())
			gsframe->Close();

		Console.Error(ex.FormatDiagnosticMessage());

		// Make sure it terminates properly for nogui users.
		if (wxGetApp().HasGUI())
			AddIdleEvent(PluginInitErrorEvent(ex));
	}
	// ----------------------------------------------------------------------------
	catch( Exception::PluginInitError& ex )
	{
		ShutdownPlugins();

		Console.Error( ex.FormatDiagnosticMessage() );
		AddIdleEvent( PluginInitErrorEvent(ex) );
	}
	// ----------------------------------------------------------------------------
	catch( Exception::PluginError& ex )
	{
		UnloadPlugins();

		Console.Error( ex.FormatDiagnosticMessage() );
		AddIdleEvent( PluginErrorEvent(ex) );
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
			Msgbox::Alert( ex.FormatDisplayMessage() );
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

// NOTE: Plugins are *not* applied by this function.  Changes to plugins need to handled
// manually.  The PluginSelectorPanel does this, for example.
void AppApplySettings( const AppConfig* oldconf )
{
	AffinityAssert_AllowFrom_MainUI();

	ScopedCoreThreadPause paused_core;

	g_Conf->Folders.ApplyDefaults();

	// Ensure existence of necessary documents folders.  Plugins and other parts
	// of PCSX2 rely on them.

	g_Conf->Folders.MemoryCards.Mkdir();
	g_Conf->Folders.Savestates.Mkdir();
	g_Conf->Folders.Snapshots.Mkdir();
	g_Conf->Folders.Cheats.Mkdir();
	g_Conf->Folders.CheatsWS.Mkdir();

	g_Conf->EmuOptions.BiosFilename = g_Conf->FullpathToBios();

	RelocateLogfile();

	if( (oldconf == NULL) || (oldconf->LanguageCode.CmpNoCase(g_Conf->LanguageCode)) )
	{
		wxDoNotLogInThisScope please;
		i18n_SetLanguage( g_Conf->LanguageId, g_Conf->LanguageCode );
	}
	
	CorePlugins.SetSettingsFolder( GetSettingsFolder().ToString() );

	// Update the compression attribute on the Memcards folder.
	// Memcards generally compress very well via NTFS compression.

	#ifdef __WXMSW__
	NTFS_CompressFile( g_Conf->Folders.MemoryCards.ToString(), g_Conf->McdCompressNTFS );
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
		// FIXME: Gsdx memory leaks in DX10 have been fixed.  This code may not be needed
		// anymore.
		
		const wxSize oldsize( gsFrame->GetSize() );
		wxSize newsize( oldsize );
		newsize.DecBy(1);

		gsFrame->SetSize( newsize );
		gsFrame->SetSize( oldsize );
	}
	
	pxAssertDev( !GetCorePlugins().IsOpen( PluginId_GS ), "GS Plugin must be closed prior to opening a new Gs Panel!" );

#ifdef __WXGTK__
	// The x window/display are actually very deeper in the widget. You need both display and window
	// because unlike window there are unrelated. One could think it would be easier to send directly the GdkWindow.
	// Unfortunately there is a race condition between gui and gs threads when you called the
	// GDK_WINDOW_* macro. To be safe I think it is best to do here. It only cost a slight
	// extension (fully compatible) of the plugins API. -- Gregory

	// GTK_PIZZA is an internal interface of wx, therefore they decide to
	// remove it on wx 3. I tryed to replace it with gtk_widget_get_window but
	// unfortunately it creates a gray box in the middle of the window on some
	// users.

	GtkWidget *child_window = GTK_WIDGET(gsFrame->GetViewport()->GetHandle());

	gtk_widget_realize(child_window); // create the widget to allow to use GDK_WINDOW_* macro
	gtk_widget_set_double_buffered(child_window, false); // Disable the widget double buffer, you will use the opengl one

	GdkWindow* draw_window = gtk_widget_get_window(child_window);

#if GTK_MAJOR_VERSION < 3
	Window Xwindow = GDK_WINDOW_XWINDOW(draw_window);
#else
	Window Xwindow = GDK_WINDOW_XID(draw_window);
#endif
	Display* XDisplay = GDK_WINDOW_XDISPLAY(draw_window);

	pDsp[0] = (uptr)XDisplay;
	pDsp[1] = (uptr)Xwindow;
#else
	pDsp[0] = (uptr)gsFrame->GetViewport()->GetHandle();
	pDsp[1] = NULL;
#endif

	gsFrame->ShowFullScreen( g_Conf->GSWindow.IsFullscreen );

#ifndef DISABLE_RECORDING
	// Disable recording controls that only make sense if the game is running
	sMainFrame.enableRecordingMenuItem(MenuId_Recording_FrameAdvance, true);
	sMainFrame.enableRecordingMenuItem(MenuId_Recording_TogglePause, true);
	sMainFrame.enableRecordingMenuItem(MenuId_Recording_ToggleRecordingMode, g_InputRecording.IsActive());
#endif
}

void Pcsx2App::CloseGsPanel()
{
	if( AppRpc_TryInvoke( &Pcsx2App::CloseGsPanel ) ) return;

	if (CloseViewportWithPlugins)
	{
		if (GSFrame* gsFrame = GetGsFramePtr())
		if (GSPanel* woot = gsFrame->GetViewport())
			woot->Destroy();
	}
#ifndef DISABLE_RECORDING
	// Disable recording controls that only make sense if the game is running
	sMainFrame.enableRecordingMenuItem(MenuId_Recording_FrameAdvance, false);
	sMainFrame.enableRecordingMenuItem(MenuId_Recording_TogglePause, false);
	sMainFrame.enableRecordingMenuItem(MenuId_Recording_ToggleRecordingMode, false);
#endif
}

void Pcsx2App::OnGsFrameClosed( wxWindowID id )
{
	if( (m_id_GsFrame == wxID_ANY) || (m_id_GsFrame != id) ) return;

	CoreThread.Suspend();

	if( !m_UseGUI )
	{
		// The user is prompted before suspending (at Sys_Suspend() ), because
		// right now there's no way to resume from suspend without GUI.
		PrepForExit();
	}
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
#ifndef DISABLE_RECORDING
	if (g_InputRecording.IsActive())
	{
		g_InputRecording.Stop();
	}
#endif

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

		// if something unloaded plugins since this messages was queued then it's best to ignore
		// it, because apparently too much stuff is going on and the emulation states are wonky.
		if( !CorePlugins.AreLoaded() ) return;

		DbgCon.WriteLn( Color_Gray, "(SysExecute) received." );

		CoreThread.ResetQuick();
		symbolMap.Clear();
		CBreakPoints::SetSkipFirst(0);

		CDVDsys_SetFile(CDVD_SourceType::Iso, g_Conf->CurrentIso );
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
	SysExecutorThread.PostEvent( new SysExecEvent_Execute() );
}

// Executes the specified cdvd source and optional elf file.  This command performs a
// full closure of any existing VM state and starts a fresh VM with the requested
// sources.
void Pcsx2App::SysExecute( CDVD_SourceType cdvdsrc, const wxString& elf_override )
{
	SysExecutorThread.PostEvent( new SysExecEvent_Execute(cdvdsrc, elf_override) );
#ifndef DISABLE_RECORDING
	g_InputRecording.RecordingReset();
#endif
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
	Console.WriteLn( WX_STR(text) );
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
#if defined(_WIN32)
	g_Conf->Folders.RunDisc = wxFileName::DirName(newDiscDrive);
#else
	g_Conf->Folders.RunDisc = wxFileName(newDiscDrive);
#endif
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

SysCoreThread& GetCoreThread()
{
	return CoreThread;
}

SysMtgsThread& GetMTGS()
{
	return mtgsThread;
}

SysCpuProviderPack& GetCpuProviders()
{
	return *wxGetApp().m_CpuProviders;
}
