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
#include "GSFrame.h"
#include "AppAccelerators.h"
#include "AppSaveStates.h"
#include "Counters.h"
#include "GS.h"
#include "MainFrame.h"
#include "MSWstuff.h"
#ifdef _WIN32
#include "PAD/Windows/PAD.h"
#else
#include "PAD/Linux/PAD.h"
#endif

#include "gui/Dialogs/ModalPopups.h"

#include "ConsoleLogger.h"

#ifndef DISABLE_RECORDING
#	include "Recording/InputRecording.h"
#	include "Recording/Utilities/InputRecordingLogger.h"
#endif

#include <wx/utils.h>
#include <wx/graphics.h>
#include <memory>
#include <sstream>
#include <iomanip>

//#define GSWindowScaleDebug

static const KeyAcceleratorCode FULLSCREEN_TOGGLE_ACCELERATOR_GSPANEL=KeyAcceleratorCode( WXK_RETURN ).Alt();

extern std::atomic_bool init_gspanel;

void GSPanel::InitDefaultAccelerators()
{
	// Note: these override GlobalAccels ( Pcsx2App::InitDefaultGlobalAccelerators() )
	// For plain letters or symbols, replace e.g. WXK_F1 with e.g. wxKeyCode('q') or wxKeyCode('-')
	// For plain letter keys with shift, use e.g. AAC( wxKeyCode('q') ).Shift() and NOT wxKeyCode('Q')
	// For a symbol with shift (e.g. '_' which is '-' with shift) use AAC( wxKeyCode('-') ).Shift()

	typedef KeyAcceleratorCode AAC;

	if (!m_Accels) m_Accels = std::unique_ptr<AcceleratorDictionary>(new AcceleratorDictionary);

	m_Accels->Map( AAC( WXK_F1 ),				"States_FreezeCurrentSlot" );
	m_Accels->Map( AAC( WXK_F3 ),				"States_DefrostCurrentSlot");
	m_Accels->Map( AAC( WXK_F3 ).Shift(),		"States_DefrostCurrentSlotBackup");
	m_Accels->Map( AAC( WXK_F2 ),				"States_CycleSlotForward" );
	m_Accels->Map( AAC( WXK_F2 ).Shift(),		"States_CycleSlotBackward" );

	m_Accels->Map( AAC( WXK_F4 ),				"Framelimiter_MasterToggle");
	m_Accels->Map( AAC( WXK_F4 ).Shift(),		"Frameskip_Toggle");
	m_Accels->Map( AAC( WXK_TAB ),				"Framelimiter_TurboToggle" );
	m_Accels->Map( AAC( WXK_TAB ).Shift(),		"Framelimiter_SlomoToggle" );

	m_Accels->Map( AAC( WXK_F6 ),				"GSwindow_CycleAspectRatio" );

	m_Accels->Map( AAC( WXK_NUMPAD_ADD ).Cmd(),			"GSwindow_ZoomIn" );	//CTRL on Windows/linux, CMD on OSX
	m_Accels->Map( AAC( WXK_NUMPAD_SUBTRACT ).Cmd(),	"GSwindow_ZoomOut" );
	m_Accels->Map( AAC( WXK_NUMPAD_MULTIPLY ).Cmd(),	"GSwindow_ZoomToggle" );

	m_Accels->Map( AAC( WXK_NUMPAD_ADD ).Cmd().Alt(),			"GSwindow_ZoomInY" );	//CTRL on Windows/linux, CMD on OSX
	m_Accels->Map( AAC( WXK_NUMPAD_SUBTRACT ).Cmd().Alt(),	"GSwindow_ZoomOutY" );
	m_Accels->Map( AAC( WXK_NUMPAD_MULTIPLY ).Cmd().Alt(),	"GSwindow_ZoomResetY" );

	m_Accels->Map( AAC( WXK_UP ).Cmd().Alt(),	"GSwindow_OffsetYminus" );
	m_Accels->Map( AAC( WXK_DOWN ).Cmd().Alt(),	"GSwindow_OffsetYplus" );
	m_Accels->Map( AAC( WXK_LEFT ).Cmd().Alt(),	"GSwindow_OffsetXminus" );
	m_Accels->Map( AAC( WXK_RIGHT ).Cmd().Alt(),	"GSwindow_OffsetXplus" );
	m_Accels->Map( AAC( WXK_NUMPAD_DIVIDE ).Cmd().Alt(),	"GSwindow_OffsetReset" );

	m_Accels->Map( AAC( WXK_ESCAPE ),			"Sys_SuspendResume" );
	m_Accels->Map( AAC( WXK_F8 ),				"Sys_TakeSnapshot" ); // also shift and ctrl-shift will be added automatically
	m_Accels->Map( AAC( WXK_F9 ),				"Sys_RenderswitchToggle");

	m_Accels->Map( AAC( WXK_F10 ),				"Sys_LoggingToggle" );
	m_Accels->Map( AAC( WXK_F11 ),				"Sys_FreezeGS" );
	m_Accels->Map( AAC( WXK_F12 ),				"Sys_RecordingToggle" );

	m_Accels->Map( FULLSCREEN_TOGGLE_ACCELERATOR_GSPANEL,		"FullscreenToggle" );
}

#ifndef DISABLE_RECORDING
void GSPanel::InitRecordingAccelerators()
{
	// Note: these override GlobalAccels ( Pcsx2App::InitDefaultGlobalAccelerators() )
	// For plain letters or symbols, replace e.g. WXK_F1 with e.g. wxKeyCode('q') or wxKeyCode('-')
	// For plain letter keys with shift, use e.g. AAC( wxKeyCode('q') ).Shift() and NOT wxKeyCode('Q')
	// For a symbol with shift (e.g. '_' which is '-' with shift) use AAC( wxKeyCode('-') ).Shift()

	typedef KeyAcceleratorCode AAC;

	if (!m_Accels) m_Accels = std::unique_ptr<AcceleratorDictionary>(new AcceleratorDictionary);

	m_Accels->Map(AAC(WXK_SPACE), "FrameAdvance");
	m_Accels->Map(AAC(wxKeyCode('p')).Shift(), "TogglePause");
	m_Accels->Map(AAC(wxKeyCode('r')).Shift(), "InputRecordingModeToggle");
	m_Accels->Map(AAC(wxKeyCode('l')).Shift(), "GoToFirstFrame");
#if defined(__unix__)
	// Shift+P (80) and Shift+p (112) have two completely different codes 
	// On Linux the former is sometimes fired so define bindings for both
	m_Accels->Map(AAC(wxKeyCode('P')).Shift(), "TogglePause");
	m_Accels->Map(AAC(wxKeyCode('R')).Shift(), "InputRecordingModeToggle");
	m_Accels->Map(AAC(wxKeyCode('L')).Shift(), "GoToFirstFrame");
#endif

	m_Accels->Map(AAC(WXK_NUMPAD0).Shift(), "States_SaveSlot0");
	m_Accels->Map(AAC(WXK_NUMPAD1).Shift(), "States_SaveSlot1");
	m_Accels->Map(AAC(WXK_NUMPAD2).Shift(), "States_SaveSlot2");
	m_Accels->Map(AAC(WXK_NUMPAD3).Shift(), "States_SaveSlot3");
	m_Accels->Map(AAC(WXK_NUMPAD4).Shift(), "States_SaveSlot4");
	m_Accels->Map(AAC(WXK_NUMPAD5).Shift(), "States_SaveSlot5");
	m_Accels->Map(AAC(WXK_NUMPAD6).Shift(), "States_SaveSlot6");
	m_Accels->Map(AAC(WXK_NUMPAD7).Shift(), "States_SaveSlot7");
	m_Accels->Map(AAC(WXK_NUMPAD8).Shift(), "States_SaveSlot8");
	m_Accels->Map(AAC(WXK_NUMPAD9).Shift(), "States_SaveSlot9");
	m_Accels->Map(AAC(WXK_NUMPAD0), "States_LoadSlot0");
	m_Accels->Map(AAC(WXK_NUMPAD1), "States_LoadSlot1");
	m_Accels->Map(AAC(WXK_NUMPAD2), "States_LoadSlot2");
	m_Accels->Map(AAC(WXK_NUMPAD3), "States_LoadSlot3");
	m_Accels->Map(AAC(WXK_NUMPAD4), "States_LoadSlot4");
	m_Accels->Map(AAC(WXK_NUMPAD5), "States_LoadSlot5");
	m_Accels->Map(AAC(WXK_NUMPAD6), "States_LoadSlot6");
	m_Accels->Map(AAC(WXK_NUMPAD7), "States_LoadSlot7");
	m_Accels->Map(AAC(WXK_NUMPAD8), "States_LoadSlot8");
	m_Accels->Map(AAC(WXK_NUMPAD9), "States_LoadSlot9");

	GetMainFramePtr()->initializeRecordingMenuItem(
		MenuId_Recording_FrameAdvance,
		GetAssociatedKeyCode("FrameAdvance"));
	GetMainFramePtr()->initializeRecordingMenuItem(
		MenuId_Recording_TogglePause,
		GetAssociatedKeyCode("TogglePause"));
	GetMainFramePtr()->initializeRecordingMenuItem(
		MenuId_Recording_ToggleRecordingMode,
		GetAssociatedKeyCode("InputRecordingModeToggle"),
		g_InputRecording.IsActive());

	inputRec::consoleLog("Initialized Input Recording Key Bindings");
}

wxString GSPanel::GetAssociatedKeyCode(const char* id)
{
	return m_Accels->findKeycodeWithCommandId(id).toTitleizedString();
}

void GSPanel::RemoveRecordingAccelerators()
{
	m_Accels.reset(new AcceleratorDictionary);
	InitDefaultAccelerators();
	recordingConLog(L"Disabled Input Recording Key Bindings\n");
}
#endif

GSPanel::GSPanel( wxWindow* parent )
	: wxWindow()
	, m_HideMouseTimer( this )
	, m_coreRunning(false)
{
	m_CursorShown	= true;
	m_HasFocus		= false;

	if ( !wxWindow::Create(parent, wxID_ANY) )
		throw Exception::RuntimeError().SetDiagMsg( L"GSPanel constructor explode!!" );

	SetName( L"GSPanel" );

	InitDefaultAccelerators();

#ifndef DISABLE_RECORDING
	if (g_Conf->EmuOptions.EnableRecordingTools)
	{
		InitRecordingAccelerators();
	}
#endif

	SetBackgroundColour(wxColour((unsigned long)0));
	if( g_Conf->GSWindow.AlwaysHideMouse )
	{
		SetCursor( wxCursor(wxCURSOR_BLANK) );
		m_CursorShown = false;
	}

	Bind(wxEVT_CLOSE_WINDOW, &GSPanel::OnCloseWindow, this);
	Bind(wxEVT_SIZE, &GSPanel::OnResize, this);
	Bind(wxEVT_KEY_UP, &GSPanel::OnKeyDownOrUp, this);
	Bind(wxEVT_KEY_DOWN, &GSPanel::OnKeyDownOrUp, this);

	Bind(wxEVT_SET_FOCUS, &GSPanel::OnFocus, this);
	Bind(wxEVT_KILL_FOCUS, &GSPanel::OnFocusLost, this);

	Bind(wxEVT_TIMER, &GSPanel::OnHideMouseTimeout, this, m_HideMouseTimer.GetId());

	// Any and all events which should result in the mouse cursor being made visible
	// are connected here.  If I missed one, feel free to add it in! --air
	Bind(wxEVT_LEFT_DOWN, &GSPanel::OnMouseEvent, this);
	Bind(wxEVT_LEFT_UP, &GSPanel::OnMouseEvent, this);
	Bind(wxEVT_MIDDLE_DOWN, &GSPanel::OnMouseEvent, this);
	Bind(wxEVT_MIDDLE_UP, &GSPanel::OnMouseEvent, this);
	Bind(wxEVT_RIGHT_DOWN, &GSPanel::OnMouseEvent, this);
	Bind(wxEVT_RIGHT_UP, &GSPanel::OnMouseEvent, this);
	Bind(wxEVT_MOTION, &GSPanel::OnMouseEvent, this);
	Bind(wxEVT_LEFT_DCLICK, &GSPanel::OnMouseEvent, this);
	Bind(wxEVT_MIDDLE_DCLICK, &GSPanel::OnMouseEvent, this);
	Bind(wxEVT_RIGHT_DCLICK, &GSPanel::OnMouseEvent, this);
	Bind(wxEVT_MOUSEWHEEL, &GSPanel::OnMouseEvent, this);

	Bind(wxEVT_LEFT_DCLICK, &GSPanel::OnLeftDclick, this);
}

GSPanel::~GSPanel()
{
	//CoreThread.Suspend( false );		// Just in case...!
}

void GSPanel::DoShowMouse()
{
	if( g_Conf->GSWindow.AlwaysHideMouse ) return;

	if( !m_CursorShown )
	{
		SetCursor( wxCursor( wxCURSOR_DEFAULT ) );
		m_CursorShown = true;
	}
	m_HideMouseTimer.Start( 1750, true );
}


void GSPanel::OnResize(wxSizeEvent& event)
{
	if( IsBeingDeleted() ) return;
	event.Skip();
}

void GSPanel::OnCloseWindow(wxCloseEvent& evt)
{
	// CoreThread pausing calls MTGS suspend which calls GSPanel close on
	// the main thread leading to event starvation. This prevents regenerating
	// a frame handle when the user closes the window, which prevents this race
	// condition. -- govanify
	init_gspanel = false;
	CoreThread.Suspend();
	evt.Skip();		// and close it.
}

void GSPanel::OnMouseEvent( wxMouseEvent& evt )
{
	if( IsBeingDeleted() ) return;

	// Do nothing for left-button event
	if (!evt.Button(wxMOUSE_BTN_LEFT)) {
		evt.Skip();
		DoShowMouse();
	}

#if defined(__unix__)
	// HACK2: In gsopen2 there is one event buffer read by both wx/gui and pad. Wx deletes
	// the event before the pad see it. So you send key event directly to the pad.
	HostKeyEvent event;
	// FIXME how to handle double click ???
	if (evt.ButtonDown())
	{
		event.type = static_cast<HostKeyEvent::Type>(4); // X equivalent of ButtonPress
		event.key = evt.GetButton();
	}
	else if (evt.ButtonUp())
	{
		event.type = static_cast<HostKeyEvent::Type>(5); // X equivalent of ButtonRelease
		event.key = evt.GetButton();
	}
	else if (evt.Moving() || evt.Dragging())
	{
		event.type = static_cast<HostKeyEvent::Type>(6); // X equivalent of MotionNotify
		long x, y;
		evt.GetPosition(&x, &y);

		wxCoord w, h;
		wxWindowDC dc(this);
		dc.GetSize(&w, &h);

		// Special case to allow continuous mouvement near the border
		if (x < 10)
			x = 0;
		else if (x > (w - 10))
			x = 0xFFFF;

		if (y < 10)
			y = 0;
		else if (y > (w - 10))
			y = 0xFFFF;

		// For compatibility purpose with the existing structure. I decide to reduce
		// the position to 16 bits.
		event.key = ((y & 0xFFFF) << 16) | (x & 0xFFFF);
	}
	else
	{
		event.key = 0;
		event.type = HostKeyEvent::Type::NoEvent;
	}

	PADWriteEvent(event);
#endif
}

void GSPanel::OnHideMouseTimeout( wxTimerEvent& evt )
{
	if( IsBeingDeleted() || !m_HasFocus ) return;
	if( CoreThread.GetExecutionMode() != SysThreadBase::ExecMode_Opened ) return;

	SetCursor( wxCursor( wxCURSOR_BLANK ) );
	m_CursorShown = false;
}

void GSPanel::OnKeyDownOrUp( wxKeyEvent& evt )
{

	// HACK: PAD expect PCSX2 to ignore keyboard messages on the GS Window while
	// it is open, so ignore here (PCSX2 will direct messages routed from PAD directly
	// to the APP level message handler, which in turn routes them right back here -- yes it's
	// silly, but oh well).

#if defined(__unix__)
	// HACK2: In gsopen2 there is one event buffer read by both wx/gui and pad. Wx deletes
	// the event before the pad see it. So you send key event directly to the pad.
	HostKeyEvent event;
	event.key = evt.GetRawKeyCode();
	if (evt.GetEventType() == wxEVT_KEY_UP)
		event.type = static_cast<HostKeyEvent::Type>(3); // X equivalent of KEYRELEASE;
	else if (evt.GetEventType() == wxEVT_KEY_DOWN)
		event.type = static_cast<HostKeyEvent::Type>(2); // X equivalent of KEYPRESS;
	else
		event.type = HostKeyEvent::Type::NoEvent;

	PADWriteEvent(event);
#endif

#ifdef __WXMSW__
	// Not sure what happens on Linux, but on windows this method is called only when emulation
	// is paused and the GS window is not hidden (and therefore the event doesn't arrive from
	// pad and doesn't go through Pcsx2App::PadKeyDispatch). On such case (paused).
	// It needs to handle two issues:
	// 1. It's called both for key down and key up (linux apparently needs it this way) - but we
	//    don't want to execute the command twice (normally commands execute on key down only).
	// 2. It has wx keycode which is upper case for ascii chars, but our command handlers expect
	//    lower case for non-special keys.

	// ignore key up events
	if (evt.GetEventType() == wxEVT_KEY_UP)
		return;

	// Make ascii keys lower case - this apparently works correctly also with modifiers (shift included)
	if (evt.m_keyCode >= 'A' && evt.m_keyCode <= 'Z')
		evt.m_keyCode += (int)'a' - 'A';
#endif

	if (CoreThread.IsOpen())
	{
		return;
	}

	DirectKeyCommand( evt );
}

void GSPanel::DirectKeyCommand( const KeyAcceleratorCode& kac )
{
	const GlobalCommandDescriptor* cmd = NULL;

	std::unordered_map<int, const GlobalCommandDescriptor*>::const_iterator iter(m_Accels->find(kac.val32));
	if (iter == m_Accels->end())
		return;

	cmd = iter->second;

	DbgCon.WriteLn( "(gsFrame) Invoking command: %s", cmd->Id );
	cmd->Invoke();

	if( cmd->AlsoApplyToGui && !g_ConfigPanelChanged)
		AppApplySettings();
}

void GSPanel::DirectKeyCommand( wxKeyEvent& evt )
{
	DirectKeyCommand(KeyAcceleratorCode( evt ));
}

void GSPanel::UpdateScreensaver()
{
    bool prevent = g_Conf->GSWindow.DisableScreenSaver && m_HasFocus && m_coreRunning;
	ScreensaverAllow(!prevent);
}

void GSPanel::OnFocus( wxFocusEvent& evt )
{
	evt.Skip();
	m_HasFocus = true;

	if( g_Conf->GSWindow.AlwaysHideMouse )
	{
		SetCursor( wxCursor(wxCURSOR_BLANK) );
		m_CursorShown = false;
	}
	else
		DoShowMouse();

#if defined(__unix__)
	// HACK2: In gsopen2 there is one event buffer read by both wx/gui and pad. Wx deletes
	// the event before the pad see it. So you send key event directly to the pad.
	HostKeyEvent event = {static_cast<HostKeyEvent::Type>(9), 0}; // X equivalent of FocusIn;
	PADWriteEvent(event);
#endif
	//Console.Warning("GS frame > focus set");

	UpdateScreensaver();
}

void GSPanel::OnFocusLost( wxFocusEvent& evt )
{
	evt.Skip();
	m_HasFocus = false;
	DoShowMouse();
#if defined(__unix__)
	// HACK2: In gsopen2 there is one event buffer read by both wx/gui and pad. Wx deletes
	// the event before the pad see it. So you send key event directly to the pad.
	HostKeyEvent event = {static_cast<HostKeyEvent::Type>(9), 0}; // X equivalent of FocusOut
	PADWriteEvent(event);
#endif
	//Console.Warning("GS frame > focus lost");

	UpdateScreensaver();
}

void GSPanel::CoreThread_OnResumed()
{
	m_coreRunning = true;
	UpdateScreensaver();
}

void GSPanel::CoreThread_OnSuspended()
{
	m_coreRunning = false;
	UpdateScreensaver();
}

void GSPanel::AppStatusEvent_OnSettingsApplied()
{
	if( IsBeingDeleted() ) return;
	DoShowMouse();
}

void GSPanel::OnLeftDclick(wxMouseEvent& evt)
{
	if( !g_Conf->GSWindow.IsToggleFullscreenOnDoubleClick )
		return;

	//Console.WriteLn("GSPanel::OnDoubleClick: Invoking Fullscreen-Toggle accelerator.");
	DirectKeyCommand(FULLSCREEN_TOGGLE_ACCELERATOR_GSPANEL);
}

// --------------------------------------------------------------------------------------
//  GSFrame Implementation
// --------------------------------------------------------------------------------------

static const uint TitleBarUpdateMs = 333;
#ifndef DISABLE_RECORDING
static const uint TitleBarUpdateMsWhenRecording = 50;
#endif

GSFrame::GSFrame( const wxString& title)
	: wxFrame(NULL, wxID_ANY, title, g_Conf->GSWindow.WindowPos)
	, m_timer_UpdateTitle( this )
{
	SetIcons( wxGetApp().GetIconBundle() );
	SetBackgroundColour( *wxBLACK );

	AppStatusEvent_OnSettingsApplied();

	GSPanel* gsPanel = new GSPanel( this );
	m_id_gspanel = gsPanel->GetId();
	gsPanel->SetPosition(wxPoint(0, 0));
	gsPanel->SetSize(GetClientSize());

	// TODO -- Implement this GS window status window!  Whee.
	// (main concern is retaining proper client window sizes when closing/re-opening the window).
	//m_statusbar = CreateStatusBar( 2 );

	Bind(wxEVT_CLOSE_WINDOW, &GSFrame::OnCloseWindow, this);
	Bind(wxEVT_MOVE, &GSFrame::OnMove, this);
	Bind(wxEVT_SIZE, &GSFrame::OnResize, this);
	Bind(wxEVT_SET_FOCUS, &GSFrame::OnFocus, this);

	Bind(wxEVT_TIMER, &GSFrame::OnUpdateTitle, this, m_timer_UpdateTitle.GetId());
}

void GSFrame::OnCloseWindow(wxCloseEvent& evt)
{
	// see GSPanel::OnCloseWindow
	init_gspanel = false;
	sApp.OnGsFrameClosed( GetId() );
	Hide();		// and don't close it.
}

bool GSFrame::ShowFullScreen(bool show, bool updateConfig)
{
	/*if( show != IsFullScreen() )
		Console.WriteLn( Color_StrongMagenta, "(gsFrame) Switching to %s mode...", show ? "Fullscreen" : "Windowed" );*/

	if (updateConfig && g_Conf->GSWindow.IsFullscreen != show)
	{
		g_Conf->GSWindow.IsFullscreen = show;
		wxGetApp().PostIdleMethod( AppSaveSettings );
	}

	// IMPORTANT!  On MSW platforms you must ALWAYS show the window prior to calling
	// ShowFullscreen(), otherwise the window will be oddly unstable (lacking input and unable
	// to properly flip back into fullscreen mode after alt-enter).  I don't know if that
	// also happens on Linux.

	if( !IsShown() ) Show();

	uint flags = wxFULLSCREEN_ALL;
#ifdef _WIN32
	flags |= g_Conf->GSWindow.EnableVsyncWindowFlag ? WS_POPUP : 0;
#endif
	bool retval = _parent::ShowFullScreen( show, flags );

	return retval;
}

void GSFrame::UpdateTitleUpdateFreq()
{
#ifndef DISABLE_RECORDING
	if (g_Conf->EmuOptions.EnableRecordingTools)
	{
		m_timer_UpdateTitle.Start(TitleBarUpdateMsWhenRecording);
	}
	else
	{
		m_timer_UpdateTitle.Start(TitleBarUpdateMs);
	}
#else
	m_timer_UpdateTitle.Start(TitleBarUpdateMs);
#endif
}

void GSFrame::CoreThread_OnResumed()
{
	UpdateTitleUpdateFreq();
	if (!IsShown())
	{
		Show();
	}
}

void GSFrame::CoreThread_OnSuspended()
{
	if( !IsBeingDeleted() && g_Conf->GSWindow.CloseOnEsc ) Hide();
}

void GSFrame::CoreThread_OnStopped()
{
	//if( !IsBeingDeleted() ) Destroy();
}

// overrides base Show behavior.
bool GSFrame::Show( bool shown )
{
	if( shown )
	{
		GSPanel* gsPanel = GetViewport();

		if( !gsPanel || gsPanel->IsBeingDeleted() )
		{
			gsPanel = new GSPanel( this );
			m_id_gspanel = gsPanel->GetId();
		}

		gsPanel->SetFocus();

		if (!m_timer_UpdateTitle.IsRunning())
		{
#ifndef DISABLE_RECORDING
			if (g_Conf->EmuOptions.EnableRecordingTools)
			{
				m_timer_UpdateTitle.Start(TitleBarUpdateMsWhenRecording);
			}
			else
			{
				m_timer_UpdateTitle.Start(TitleBarUpdateMs);
			}
#else
			m_timer_UpdateTitle.Start(TitleBarUpdateMs);
#endif
		}
	}
	else
	{
		m_timer_UpdateTitle.Stop();
	}

	return _parent::Show( shown );
}

void GSFrame::AppStatusEvent_OnSettingsApplied()
{
	if( IsBeingDeleted() ) return;

	SetWindowStyle((g_Conf->GSWindow.DisableResizeBorders ? 0 : wxRESIZE_BORDER) | wxCAPTION | wxCLIP_CHILDREN |
			wxSYSTEM_MENU | wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxCLOSE_BOX);
	if (!IsFullScreen() && !IsMaximized())
		SetClientSize(g_Conf->GSWindow.WindowSize);
	Refresh();

	if( g_Conf->GSWindow.CloseOnEsc )
	{
		if (IsShown() && !gsopen_done)
			Show( false );
	}
}

GSPanel* GSFrame::GetViewport()
{
	return (GSPanel*)FindWindowById( m_id_gspanel );
}

void GSFrame::OnUpdateTitle( wxTimerEvent& evt )
{
	// Update the title only after the completion of at least a single Vsync, it's pointless to display the fps
	// when there are have been no frames rendered and SMODE2 register seems to fresh start with 0 on all the bitfields which
	// leads to the value of INT bit as 0 initially and the games are mentioned as progressive which is a bit misleading,
	// to prevent such issues, let's update the title after the actual frame rendering starts.
	if (g_FrameCount == 0)
		return;

	double fps = wxGetApp().FpsManager.GetFramerate();

	FastFormatUnicode cpuUsage;
	if (m_CpuUsage.IsImplemented()) {
		m_CpuUsage.UpdateStats();

		if (!IsFullScreen()) {
			cpuUsage.Write(L"EE: %3d%%", m_CpuUsage.GetEEcorePct());
			cpuUsage.Write(L" | GS: %3d%%", m_CpuUsage.GetGsPct());

			if (THREAD_VU1)
				cpuUsage.Write(L" | VU: %3d%%", m_CpuUsage.GetVUPct());

			pxNonReleaseCode(cpuUsage.Write(L" | UI: %3d%%", m_CpuUsage.GetGuiPct()));
		}

		if (THREAD_VU1)
			OSDmonitor(Color_StrongGreen, "VU:", std::to_string(m_CpuUsage.GetVUPct()).c_str());

		OSDmonitor(Color_StrongGreen, "EE:", std::to_string(m_CpuUsage.GetEEcorePct()).c_str());
		OSDmonitor(Color_StrongGreen, "GS:", std::to_string(m_CpuUsage.GetGsPct()).c_str());
		pxNonReleaseCode(OSDmonitor(Color_StrongGreen, "UI:", std::to_string(m_CpuUsage.GetGuiPct()).c_str()));
	}

	std::ostringstream out;
	out << std::fixed << std::setprecision(2) << fps;
	OSDmonitor(Color_StrongGreen, "FPS:", out.str());

#ifdef __linux__
	// Important Linux note: When the title is set in fullscreen the window is redrawn. Unfortunately
	// an intermediate white screen appears too which leads to a very annoying flickering.
	if (IsFullScreen()) return;
#endif

	AppConfig::UiTemplateOptions& templates = g_Conf->Templates;

	const float percentage = (fps * 100) / GetVerticalFrequency();

	char gsDest[128];
	gsDest[0] = 0; // No need to set whole array to NULL.
	GSgetTitleInfo2( gsDest, sizeof(gsDest) );

	wxString limiterStr = templates.LimiterUnlimited;

	if( g_Conf->EmuOptions.GS.FrameLimitEnable )
	{
		switch( EmuConfig.LimiterMode )
		{
			case LimiterModeType::Nominal:	limiterStr = templates.LimiterNormal; break;
			case LimiterModeType::Turbo:	limiterStr = templates.LimiterTurbo; break;
			case LimiterModeType::Slomo:	limiterStr = templates.LimiterSlowmo; break;
		}
	}

	const u64& smode2 = *(u64*)PS2GS_BASE(GS_SMODE2);
	wxString omodef = (smode2 & 2) ? templates.OutputFrame : templates.OutputField;
	wxString omodei = (smode2 & 1) ? templates.OutputInterlaced : templates.OutputProgressive;
#ifndef DISABLE_RECORDING
	wxString title;
	wxString movieMode;
	if (g_InputRecording.IsActive()) 
	{
		title = templates.RecordingTemplate;
		title.Replace(L"${frame}", pxsFmt(L"%d", g_InputRecording.GetFrameCounter()));
		title.Replace(L"${maxFrame}", pxsFmt(L"%d", g_InputRecording.GetInputRecordingData().GetTotalFrames()));
		title.Replace(L"${mode}", g_InputRecording.RecordingModeTitleSegment());
	} else {
		title = templates.TitleTemplate;
	}
#else
	wxString title = templates.TitleTemplate;
#endif
	
	title.Replace(L"${slot}",		pxsFmt(L"%d", States_GetCurrentSlot()));
	title.Replace(L"${limiter}",	limiterStr);
	title.Replace(L"${speed}",		pxsFmt(L"%3d%%", lround(percentage)));
	title.Replace(L"${vfps}",		pxsFmt(L"%.02f", fps));
	title.Replace(L"${cpuusage}",	cpuUsage);
	title.Replace(L"${omodef}",		omodef);
	title.Replace(L"${omodei}",		omodei);
	title.Replace(L"${gsdx}",		fromUTF8(gsDest));
	title.Replace(L"${videomode}",	ReportVideoMode());
	if (CoreThread.IsPaused() && !GSDump::isRunning)
		title = templates.Paused + title;

	SetTitle(title);
}

void GSFrame::OnFocus( wxFocusEvent& evt )
{
	if( IsBeingDeleted() ) return;

	evt.Skip(false); // Reject the focus message, as we pass focus to the child
	if( wxWindow* gsPanel = GetViewport() ) gsPanel->SetFocus();
}

void GSFrame::OnMove( wxMoveEvent& evt )
{
	if( IsBeingDeleted() ) return;

	evt.Skip();

	g_Conf->GSWindow.IsMaximized = IsMaximized();

	// evt.GetPosition() returns the client area position, not the window frame position.
	if( !g_Conf->GSWindow.IsMaximized && !IsFullScreen() && !IsIconized() && IsVisible() )
		g_Conf->GSWindow.WindowPos = GetScreenPosition();

	// wxGTK note: X sends gratuitous amounts of OnMove messages for various crap actions
	// like selecting or deselecting a window, which muck up docking logic.  We filter them
	// out using 'lastpos' here. :)

	//static wxPoint lastpos( wxDefaultCoord, wxDefaultCoord );
	//if( lastpos == evt.GetPosition() ) return;
	//lastpos = evt.GetPosition();
}

void GSFrame::SetFocus()
{
	_parent::SetFocus();
	if( GSPanel* gsPanel = GetViewport() )
		gsPanel->SetFocusFromKbd();
}

void GSFrame::OnResize( wxSizeEvent& evt )
{
	if( IsBeingDeleted() ) return;

	if( !IsFullScreen() && !IsMaximized() && IsVisible() )
	{
		g_Conf->GSWindow.WindowSize	= GetClientSize();
	}

	// Ensure we're always in sync with the parent size.
	if( GSPanel* gsPanel = GetViewport() )
		gsPanel->SetSize(evt.GetSize());

	evt.Skip();
}
