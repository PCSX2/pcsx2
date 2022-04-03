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
#include "AppHost.h"
#include "AppSaveStates.h"
#include "Counters.h"
#include "GS.h"
#include "GS/GS.h"
#include "MainFrame.h"
#include "MSWstuff.h"
#include "PAD/Gamepad.h"
#include "PerformanceMetrics.h"
#include "common/StringUtil.h"

#include "gui/Dialogs/ModalPopups.h"

#include "ConsoleLogger.h"

#include "Recording/InputRecording.h"
#include "Recording/Utilities/InputRecordingLogger.h"

#include <wx/utils.h>
#include <wx/graphics.h>
#include <memory>
#include <sstream>
#include <iomanip>

#ifdef __WXGTK__
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

// Junk X11 macros...
#ifdef DisableScreenSaver
#undef DisableScreenSaver
#endif
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

//#define GSWindowScaleDebug

static const KeyAcceleratorCode FULLSCREEN_TOGGLE_ACCELERATOR_GSPANEL=KeyAcceleratorCode( WXK_RETURN ).Alt();

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
	// m_Accels->Map( AAC( WXK_F11 ),				"Sys_FreezeGS" );
	m_Accels->Map( AAC( WXK_F12 ),				"Sys_RecordingToggle" );

	m_Accels->Map( FULLSCREEN_TOGGLE_ACCELERATOR_GSPANEL,		"FullscreenToggle" );
}

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
	recordingConLog("Disabled Input Recording Key Bindings\n");
}

GSPanel::GSPanel( wxWindow* parent )
	: wxWindow()
	, m_HideMouseTimer( this )
	, m_coreRunning(false)
{
	m_CursorShown	= true;
	m_HasFocus		= false;

	if ( !wxWindow::Create(parent, wxID_ANY) )
		throw Exception::RuntimeError().SetDiagMsg( "GSPanel constructor explode!!" );

	SetName( L"GSPanel" );

	InitDefaultAccelerators();

	if (g_Conf->EmuOptions.EnableRecordingTools)
	{
		InitRecordingAccelerators();
	}

	SetBackgroundColour(wxColour((unsigned long)0));
	if( g_Conf->GSWindow.AlwaysHideMouse )
	{
		SetCursor( wxCursor(wxCURSOR_BLANK) );
		m_CursorShown = false;
	}

	Bind(wxEVT_SIZE, &GSPanel::OnResize, this);
#if wxCHECK_VERSION(3, 1, 3)
	Bind(wxEVT_DPI_CHANGED, &GSPanel::OnResize, this);
#endif
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

#ifdef WAYLAND_API
	WaylandDestroySubsurface();
#endif
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

#ifdef _WIN32
static float GetDpiScaleForWxWindow(wxWindow* window)
{
	static UINT(WINAPI * get_dpi_for_window)(HWND hwnd);
	if (!get_dpi_for_window)
	{
		HMODULE mod = GetModuleHandle(L"user32.dll");
		if (mod)
			get_dpi_for_window = reinterpret_cast<decltype(get_dpi_for_window)>(GetProcAddress(mod, "GetDpiForWindow"));
	}
	if (!get_dpi_for_window)
		return 1.0f;

	// less than 100% scaling seems unlikely.
	const UINT dpi = get_dpi_for_window(window->GetHandle());
	return (dpi > 0) ? std::max(1.0f, static_cast<float>(dpi) / 96.0f) : 1.0f;
}
#endif

std::optional<WindowInfo> GSPanel::GetWindowInfo()
{
	WindowInfo ret;

	const wxSize gs_vp_size(GetClientSize());
	ret.surface_scale = static_cast<float>(GetContentScaleFactor());
	ret.surface_width = static_cast<u32>(gs_vp_size.GetWidth());
	ret.surface_height = static_cast<u32>(gs_vp_size.GetHeight());

#if defined(_WIN32)
	ret.type = WindowInfo::Type::Win32;
	ret.window_handle = GetHandle();

	// Windows DPI internally uses the higher pixel count, so work out by how much.
	ret.surface_scale = GetDpiScaleForWxWindow(this);
#elif defined(__WXGTK__)
	GtkWidget* child_window = GTK_WIDGET(GetHandle());

	// create the widget to allow to use GDK_WINDOW_* macro
	gtk_widget_realize(child_window);

	// Disable the widget double buffer, you will use the opengl one
	gtk_widget_set_double_buffered(child_window, false);

	GdkWindow* draw_window = gtk_widget_get_window(child_window);

	// TODO: Do we even want to support GTK2?
#if GTK_MAJOR_VERSION < 3
	ret.type = WindowInfo::Type::X11;
	ret.display_connection = GDK_WINDOW_XDISPLAY(draw_window);
	ret.window_handle = reinterpret_cast<void*>(GDK_WINDOW_XWINDOW(draw_window));
#else
#ifdef GDK_WINDOWING_X11
	if (GDK_IS_X11_WINDOW(draw_window))
	{
		ret.type = WindowInfo::Type::X11;
		ret.display_connection = GDK_WINDOW_XDISPLAY(draw_window);
		ret.window_handle = reinterpret_cast<void*>(GDK_WINDOW_XID(draw_window));

		// GTK/X11 seems to not scale coordinates?
		ret.surface_width = static_cast<u32>(ret.surface_width * ret.surface_scale);
		ret.surface_height = static_cast<u32>(ret.surface_height * ret.surface_scale);
	}
#endif // GDK_WINDOWING_X11
#if defined(GDK_WINDOWING_WAYLAND) && defined(WAYLAND_API)
	if (GDK_IS_WAYLAND_WINDOW(draw_window))
	{
		wl_display* display = gdk_wayland_display_get_wl_display(gdk_window_get_display(draw_window));
		wl_surface* parent_surface = gdk_wayland_window_get_wl_surface(draw_window);
		if (!m_wl_child_surface && !WaylandCreateSubsurface(display, parent_surface))
			return std::nullopt;

		ret.type = WindowInfo::Type::Wayland;
		ret.display_connection = display;
		ret.window_handle = m_wl_child_surface;
	}
#endif	// GDK_WINDOWING_WAYLAND
#endif // GTK_MAJOR_VERSION >= 3
#elif defined(__WXOSX__)
	ret.type = WindowInfo::Type::MacOS;
	ret.surface_width  = static_cast<u32>(ret.surface_width  * ret.surface_scale);
	ret.surface_height = static_cast<u32>(ret.surface_height * ret.surface_scale);
	ret.window_handle = GetHandle();
#endif

	if (ret.type == WindowInfo::Type::Surfaceless)
	{
		Console.Error("Unknown window type for GSFrame.");
		return std::nullopt;
	}

	return ret;
}

void GSPanel::OnResize(wxEvent& event)
{
	if( IsBeingDeleted() ) return;
	event.Skip();

	if (g_gs_window_info.type == WindowInfo::Type::Surfaceless)
		return;

	const wxSize gs_vp_size(GetClientSize());
#ifdef _WIN32
	const float scale = GetDpiScaleForWxWindow(this);
#else
	const float scale = GetContentScaleFactor();
#endif
	int width = gs_vp_size.GetWidth();
	int height = gs_vp_size.GetHeight();

	if (false
#ifdef __WXGTK__
		|| g_gs_window_info.type == WindowInfo::Type::X11
#endif
#ifdef __APPLE__
		|| g_gs_window_info.type == WindowInfo::Type::MacOS
#endif
	)
	{
		width = static_cast<int>(width * scale);
		height = static_cast<int>(height * scale);
	}

	g_gs_window_info.surface_width = width;
	g_gs_window_info.surface_height = height;
	g_gs_window_info.surface_scale = scale;

	Host::GSWindowResized(width, height, scale);
}

void GSPanel::OnMouseEvent( wxMouseEvent& evt )
{
	if( IsBeingDeleted() ) return;

	// Do nothing for left-button event
	if (!evt.Button(wxMOUSE_BTN_LEFT)) {
		evt.Skip();
		DoShowMouse();
	}

#if defined(__unix__) || defined(__APPLE__)
	// HACK2: In gsopen2 there is one event buffer read by both wx/gui and pad. Wx deletes
	// the event before the pad see it. So you send key event directly to the pad.
	HostKeyEvent event;
	// FIXME how to handle double click ???
	if (evt.ButtonDown())
	{
		event.type = HostKeyEvent::Type::MousePressed;
		event.key = evt.GetButton() | 0x10000;
	}
	else if (evt.ButtonUp())
	{
		event.type = HostKeyEvent::Type::MouseReleased;
		event.key = evt.GetButton() | 0x10000;
	}
	else if (evt.Moving() || evt.Dragging())
	{
		event.type = HostKeyEvent::Type::MouseMove;
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

#if defined(__unix__) || defined(__APPLE__)
	// HACK2: In gsopen2 there is one event buffer read by both wx/gui and pad. Wx deletes
	// the event before the pad see it. So you send key event directly to the pad.
	HostKeyEvent event;
	event.key = evt.GetRawKeyCode();
	if (evt.GetEventType() == wxEVT_KEY_UP)
		event.type = HostKeyEvent::Type::KeyReleased;
	else if (evt.GetEventType() == wxEVT_KEY_DOWN)
		event.type = HostKeyEvent::Type::KeyPressed;
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

#if defined(__unix__) || defined(__APPLE__)
	// HACK2: In gsopen2 there is one event buffer read by both wx/gui and pad. Wx deletes
	// the event before the pad see it. So you send key event directly to the pad.
	HostKeyEvent event = {HostKeyEvent::Type::FocusGained, 0};
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
#if defined(__unix__) || defined(__APPLE__)
	// HACK2: In gsopen2 there is one event buffer read by both wx/gui and pad. Wx deletes
	// the event before the pad see it. So you send key event directly to the pad.
	HostKeyEvent event = {HostKeyEvent::Type::FocustLost, 0};
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

#if defined(__WXGTK__) && defined(GDK_WINDOWING_WAYLAND) && defined(WAYLAND_API)

void GSPanel::WaylandGlobalRegistryAddHandler(void* data, wl_registry* registry, uint32_t id, const char* interface, uint32_t version)
{
	GSPanel* pnl = static_cast<GSPanel*>(data);
	if (std::strcmp(interface, wl_compositor_interface.name) == 0)
	{
		pnl->m_wl_compositor = static_cast<wl_compositor*>(wl_registry_bind(registry, id, &wl_compositor_interface, wl_compositor_interface.version));
	}
	else if (std::strcmp(interface, wl_subcompositor_interface.name) == 0)
	{
		pnl->m_wl_subcompositor = static_cast<wl_subcompositor*>(wl_registry_bind(registry, id, &wl_subcompositor_interface, wl_subcompositor_interface.version));
	}
}

void GSPanel::WaylandGlobalRegistryRemoveHandler(void* data, wl_registry* registry, uint32_t id)
{
}

bool GSPanel::WaylandCreateSubsurface(wl_display* display, wl_surface* surface)
{
	static constexpr wl_registry_listener registry_listener = {
		&GSPanel::WaylandGlobalRegistryAddHandler,
		&GSPanel::WaylandGlobalRegistryRemoveHandler};

	wl_registry* registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, this);
	wl_display_flush(display);
	wl_display_roundtrip(display);
	if (!m_wl_compositor || !m_wl_subcompositor)
	{
		Console.Error("Missing wl_compositor or wl_subcompositor");
		return false;
	}

	wl_registry_destroy(registry);

	m_wl_child_surface = wl_compositor_create_surface(m_wl_compositor);
	if (!m_wl_child_surface)
	{
		Console.Error("Failed to create compositor surface");
		return false;
	}

	wl_region* input_region = wl_compositor_create_region(m_wl_compositor);
	if (!input_region)
	{
		Console.Error("Failed to create input region");
		return false;
	}
	wl_surface_set_input_region(m_wl_child_surface, input_region);
	wl_region_destroy(input_region);

	m_wl_child_subsurface = wl_subcompositor_get_subsurface(m_wl_subcompositor, m_wl_child_surface, surface);
	if (!m_wl_child_subsurface)
	{
		Console.Error("Failed to create subsurface");
		return false;
	}

	// we want to present asynchronously to the surrounding window
	wl_subsurface_set_desync(m_wl_child_subsurface);
	return true;
}

void GSPanel::WaylandDestroySubsurface()
{
	if (m_wl_child_subsurface)
	{
		wl_subsurface_destroy(m_wl_child_subsurface);
		m_wl_child_subsurface = nullptr;
	}

	if (m_wl_child_surface)
	{
		wl_surface_destroy(m_wl_child_surface);
		m_wl_child_surface = nullptr;
	}

	if (m_wl_subcompositor)
	{
		wl_subcompositor_destroy(m_wl_subcompositor);
		m_wl_subcompositor = nullptr;
	}

	if (m_wl_compositor)
	{
		wl_compositor_destroy(m_wl_compositor);
		m_wl_compositor = nullptr;
	}
}

#endif


// --------------------------------------------------------------------------------------
//  GSFrame Implementation
// --------------------------------------------------------------------------------------

static const uint TitleBarUpdateMs = 333;
static const uint TitleBarUpdateMsWhenRecording = 50;

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
	// if a gs dump is running, it cleans up the window once it's hidden.
	if (GSDump::isRunning)
	{
		Hide();
		return;
	}

	// but under normal operation, we want to suspend the core thread, which will hide us
	// (except if hide-on-escape is enabled, in which case we want to force hide ourself)
	sApp.OnGsFrameClosed( GetId() );
	if (!IsShown())
		Hide();
}

void GSFrame::OnDestroyWindow(wxWindowDestroyEvent& evt)
{
	sApp.OnGsFrameDestroyed(GetId());
	evt.Skip();
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
	if (g_Conf->EmuOptions.EnableRecordingTools)
	{
		m_timer_UpdateTitle.Start(TitleBarUpdateMsWhenRecording);
	}
	else
	{
		m_timer_UpdateTitle.Start(TitleBarUpdateMs);
	}
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
			if (g_Conf->EmuOptions.EnableRecordingTools)
			{
				m_timer_UpdateTitle.Start(TitleBarUpdateMsWhenRecording);
			}
			else
			{
				m_timer_UpdateTitle.Start(TitleBarUpdateMs);
			}
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

	FastFormatUnicode cpuUsage;
	if (!IsFullScreen()) {
		cpuUsage.Write(L"EE: %3.0f%%", PerformanceMetrics::GetCPUThreadUsage());
		cpuUsage.Write(L" | GS: %3.0f%%", PerformanceMetrics::GetGSThreadUsage());

		if (THREAD_VU1)
			cpuUsage.Write(L" | VU: %3.0f%%", PerformanceMetrics::GetVUThreadUsage());
	}

#ifdef __linux__
	// Important Linux note: When the title is set in fullscreen the window is redrawn. Unfortunately
	// an intermediate white screen appears too which leads to a very annoying flickering.
	if (IsFullScreen()) return;
#endif

	AppConfig::UiTemplateOptions& templates = g_Conf->Templates;

	wxString limiterStr = templates.LimiterUnlimited;

	if( g_Conf->EmuOptions.GS.FrameLimitEnable )
	{
		switch( EmuConfig.LimiterMode )
		{
			case LimiterModeType::Nominal:	limiterStr = templates.LimiterNormal; break;
			case LimiterModeType::Turbo:	limiterStr = templates.LimiterTurbo; break;
			case LimiterModeType::Slomo:	limiterStr = templates.LimiterSlowmo; break;
			case LimiterModeType::Unlimited: limiterStr = templates.LimiterUnlimited; break;
		}
	}

	const u64& smode2 = *(u64*)PS2GS_BASE(GS_SMODE2);
	wxString omodef = (smode2 & 2) ? templates.OutputFrame : templates.OutputField;
	wxString omodei = (smode2 & 1) ? templates.OutputInterlaced : templates.OutputProgressive;
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

	std::string gsStats;
	GSgetTitleStats(gsStats);

	title.Replace(L"${slot}",		pxsFmt(L"%d", States_GetCurrentSlot()));
	title.Replace(L"${limiter}",	limiterStr);
	title.Replace(L"${speed}",		pxsFmt(L"%3d%%", lround(PerformanceMetrics::GetSpeed())));
	title.Replace(L"${vfps}",		pxsFmt(L"%.02f", PerformanceMetrics::GetFPS()));
	title.Replace(L"${cpuusage}",	cpuUsage);
	title.Replace(L"${omodef}",		omodef);
	title.Replace(L"${omodei}",		omodei);
	title.Replace(L"${gsdx}", StringUtil::UTF8StringToWxString(gsStats));
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
