
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

#pragma once

#include "AppCommon.h"
#include "common/WindowInfo.h"
#include <memory>
#include <optional>

#ifdef WAYLAND_API
#include <wayland-client.h>
#endif

// --------------------------------------------------------------------------------------
//  GSPanel
// --------------------------------------------------------------------------------------
class GSPanel : public wxWindow
	, public EventListener_AppStatus
	, public EventListener_CoreThread
{
	typedef wxWindow _parent;

protected:
	std::unique_ptr<AcceleratorDictionary> m_Accels;

	wxTimer					m_HideMouseTimer;
	bool					m_CursorShown;
	bool					m_HasFocus;
	bool					m_coreRunning;

public:
	GSPanel( wxWindow* parent );
	virtual ~GSPanel();

	std::optional<WindowInfo> GetWindowInfo();

	void DoShowMouse();
	void DirectKeyCommand( wxKeyEvent& evt );
	void DirectKeyCommand( const KeyAcceleratorCode& kac );
	void InitDefaultAccelerators();
	wxString GetAssociatedKeyCode(const char* id);
	void InitRecordingAccelerators();
	void RemoveRecordingAccelerators();

protected:
	void AppStatusEvent_OnSettingsApplied();

	void OnResize(wxEvent& event);
	void OnMouseEvent( wxMouseEvent& evt );
	void OnHideMouseTimeout( wxTimerEvent& evt );
	void OnKeyDownOrUp( wxKeyEvent& evt );
	void OnFocus( wxFocusEvent& evt );
	void OnFocusLost( wxFocusEvent& evt );
	void CoreThread_OnResumed();
	void CoreThread_OnSuspended();

	void OnLeftDclick( wxMouseEvent& evt );

	void UpdateScreensaver();

private:
#ifdef WAYLAND_API
  static void WaylandGlobalRegistryAddHandler(void* data, wl_registry* registry, uint32_t id, const char* interface, uint32_t version);
  static void WaylandGlobalRegistryRemoveHandler(void* data, wl_registry* registry, uint32_t id);

  bool WaylandCreateSubsurface(wl_display* display, wl_surface* parent_surface);
  void WaylandDestroySubsurface();

  wl_compositor* m_wl_compositor = nullptr;
  wl_subcompositor* m_wl_subcompositor = nullptr;
  wl_surface* m_wl_child_surface = nullptr;
  wl_subsurface* m_wl_child_subsurface = nullptr;
#endif
};


// --------------------------------------------------------------------------------------
//  GSFrame
// --------------------------------------------------------------------------------------
class GSFrame : public wxFrame
	, public EventListener_AppStatus
	, public EventListener_CoreThread
{
	typedef wxFrame _parent;

protected:
	wxTimer					m_timer_UpdateTitle;
	wxWindowID				m_id_gspanel;
	wxStatusBar*			m_statusbar;

public:
	GSFrame( const wxString& title);
	virtual ~GSFrame() = default;

	GSPanel* GetViewport();
	void SetFocus();
	bool Show( bool shown=true );

	bool ShowFullScreen(bool show, bool updateConfig = true);
	void UpdateTitleUpdateFreq();

protected:
	void OnCloseWindow( wxCloseEvent& evt );
	void OnDestroyWindow( wxWindowDestroyEvent& evt );
	void OnMove( wxMoveEvent& evt );
	void OnResize( wxSizeEvent& evt );
	void OnFocus( wxFocusEvent& evt );
	void OnUpdateTitle( wxTimerEvent& evt );

	void AppStatusEvent_OnSettingsApplied();
	void CoreThread_OnResumed();
	void CoreThread_OnSuspended();
	void CoreThread_OnStopped();
};

// --------------------------------------------------------------------------------------
//  s* macros!  ['s' stands for 'shortcut']
// --------------------------------------------------------------------------------------
// Use these for "silent fail" invocation of PCSX2 Application-related constructs.  If the
// construct (albeit wxApp, MainFrame, CoreThread, etc) is null, the requested method will
// not be invoked, and an optional "else" clause can be affixed for handling the end case.
//
// See App.h (sApp) for more details.
//
#define sGSFrame \
	if( GSFrame* __gsframe_ = wxGetApp().GetGsFramePtr() ) (*__gsframe_)
