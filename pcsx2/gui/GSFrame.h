
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
#include "CpuUsageProvider.h"

#include "wx/dcbuffer.h"
#include "wx/dcgraph.h"

#include <memory>


enum LimiterModeType
{
	Limit_Nominal,
	Limit_Turbo,
	Limit_Slomo,
};

extern LimiterModeType g_LimiterMode;

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

	// TAS
	int						m_frameAdvanceKey; // To allow event repeat of this key

public:
	GSPanel( wxWindow* parent );
	virtual ~GSPanel();

	virtual void DoResize();
	void DoShowMouse();
	void DirectKeyCommand( wxKeyEvent& evt );
	virtual void DirectKeyCommand( const KeyAcceleratorCode& kac );

protected:
	void AppStatusEvent_OnSettingsApplied();
	void InitDefaultAccelerators();

	void OnCloseWindow( wxCloseEvent& evt );
	void OnResize(wxSizeEvent& event);
	void OnMouseEvent( wxMouseEvent& evt );
	void OnHideMouseTimeout( wxTimerEvent& evt );
	void OnKeyDownOrUp( wxKeyEvent& evt );
	void OnFocus( wxFocusEvent& evt );
	void OnFocusLost( wxFocusEvent& evt );
	void CoreThread_OnResumed();
	void CoreThread_OnSuspended();

	void OnLeftDclick( wxMouseEvent& evt );

	void UpdateScreensaver();
};


// --------------------------------------------------------------------------------------
//  GSGUIPanel
//  This panel is intented to be used by Lua scripts
//  TODO: If anyone knows how to get wxBufferedDC to work without having a black screen...
// --------------------------------------------------------------------------------------
class GSGUIPanel : public GSPanel
{
	typedef GSPanel _parent;

protected:
	wxGraphicsContext *m_gc;
	wxGCDC *m_dc;

public:
	GSGUIPanel(wxFrame* parent);
	virtual ~GSGUIPanel() throw();

	void DoResize() override;			// Overload to re-create dc
	void DirectKeyCommand(const KeyAcceleratorCode& kac) override;

	void BeginFrame();			// Must be called at the beginning of the Lua boundary frame (clears the screen from the previous drawings)
	void EndFrame();			// Must be called at the end of the Lua boundary frame

	void Clear();				// Erases everything from the screen
	void DrawLine(int x1, int y1, int x2, int y2, wxColor color);
	void DrawText(int x, int y, wxString text, wxColor foreground, wxColor background,
		int fontSize, int family, int style, int weight);
	void DrawBox(int x1, int y1, int x2, int y2, wxColor line, wxColor background);
	void DrawRectangle(int x, int y, int width, int height, wxColor line, wxColor background);
	void DrawPixel(int x, int y, wxColor color);
	void DrawEllipse(int x, int y, int width, int height, wxColor line, wxColor background);
	void DrawCircle(int x, int y, int radius, wxColor line, wxColor background);

protected:
	void OnEraseBackground(wxEraseEvent &event) {}
	void OnPaint(wxPaintEvent &event);

	void Create();				// Initialises pointers
};


// --------------------------------------------------------------------------------------
//  GSFrame
// --------------------------------------------------------------------------------------
class GSFrame : public wxFrame
	, public EventListener_AppStatus
	, public EventListener_CoreThread
	, public EventListener_Plugins
{
	typedef wxFrame _parent;

protected:
	wxTimer					m_timer_UpdateTitle;
	wxWindowID				m_id_gspanel;
	wxWindowID				m_id_gsguipanel;
	wxStatusBar*			m_statusbar;

	CpuUsageProvider		m_CpuUsage;

public:
	GSFrame( const wxString& title);
	virtual ~GSFrame() = default;

	GSPanel* GetViewport();
	GSGUIPanel* GetGui();
	void SetFocus();
	bool Show( bool shown=true );

	bool ShowFullScreen(bool show, bool updateConfig = true);

protected:
	void OnCloseWindow( wxCloseEvent& evt );
	void OnMove( wxMoveEvent& evt );
	void OnResize( wxSizeEvent& evt );
	void OnActivate( wxActivateEvent& evt );
	void OnUpdateTitle( wxTimerEvent& evt );

	void AppStatusEvent_OnSettingsApplied();
	void CoreThread_OnResumed();
	void CoreThread_OnSuspended();
	void CoreThread_OnStopped();
	void CorePlugins_OnShutdown();
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
