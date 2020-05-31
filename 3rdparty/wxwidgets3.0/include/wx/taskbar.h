/////////////////////////////////////////////////////////////////////////////
// Name:        wx/taskbar.h
// Purpose:     wxTaskBarIcon base header and class
// Author:      Julian Smart
// Modified by:
// Created:
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TASKBAR_H_BASE_
#define _WX_TASKBAR_H_BASE_

#include "wx/defs.h"

#if wxUSE_TASKBARICON

#include "wx/event.h"

class WXDLLIMPEXP_FWD_ADV wxTaskBarIconEvent;

// ----------------------------------------------------------------------------

// type of taskbar item to create.  Only applicable in wxOSX_COCOA
enum wxTaskBarIconType
{
    wxTBI_DOCK,
    wxTBI_CUSTOM_STATUSITEM,
#if defined(wxOSX_USE_COCOA) && wxOSX_USE_COCOA
    wxTBI_DEFAULT_TYPE = wxTBI_CUSTOM_STATUSITEM
#else
    wxTBI_DEFAULT_TYPE = wxTBI_DOCK
#endif
};


// ----------------------------------------------------------------------------
// wxTaskBarIconBase: define wxTaskBarIcon interface
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxTaskBarIconBase : public wxEvtHandler
{
public:
    wxTaskBarIconBase() { }

#if defined(__WXGTK__) || defined(__WXX11__) || defined(__WXMOTIF__)
    static bool IsAvailable();
#else
    static bool IsAvailable() { return true; }
#endif

    // Operations:
    virtual bool SetIcon(const wxIcon& icon,
                         const wxString& tooltip = wxEmptyString) = 0;
    virtual bool RemoveIcon() = 0;
    virtual bool PopupMenu(wxMenu *menu) = 0;

    // delayed destruction (similarly to wxWindow::Destroy())
    void Destroy();

protected:
    // creates menu to be displayed when user clicks on the icon
    virtual wxMenu *CreatePopupMenu() { return NULL; }

private:
    // default events handling, calls CreatePopupMenu:
    void OnRightButtonDown(wxTaskBarIconEvent& event);

    DECLARE_EVENT_TABLE()
    wxDECLARE_NO_COPY_CLASS(wxTaskBarIconBase);
};


// ----------------------------------------------------------------------------
// now include the actual class declaration
// ----------------------------------------------------------------------------

#if defined(__WXMSW__)
    #include "wx/msw/taskbar.h"
#elif defined(__WXGTK20__)
    #include "wx/gtk/taskbar.h"
#elif defined(__WXGTK__) || defined(__WXX11__) || defined(__WXMOTIF__)
    #include "wx/unix/taskbarx11.h"
#elif defined (__WXMAC__)
    #include "wx/osx/taskbarosx.h"
#elif defined (__WXCOCOA__)
    #include "wx/cocoa/taskbar.h"
#endif

// ----------------------------------------------------------------------------
// wxTaskBarIcon events
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxTaskBarIconEvent : public wxEvent
{
public:
    wxTaskBarIconEvent(wxEventType evtType, wxTaskBarIcon *tbIcon)
        : wxEvent(wxID_ANY, evtType)
    {
        SetEventObject(tbIcon);
    }

    virtual wxEvent *Clone() const { return new wxTaskBarIconEvent(*this); }

private:
    wxDECLARE_NO_ASSIGN_CLASS(wxTaskBarIconEvent);
};

typedef void (wxEvtHandler::*wxTaskBarIconEventFunction)(wxTaskBarIconEvent&);

wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_TASKBAR_MOVE, wxTaskBarIconEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_TASKBAR_LEFT_DOWN, wxTaskBarIconEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_TASKBAR_LEFT_UP, wxTaskBarIconEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_TASKBAR_RIGHT_DOWN, wxTaskBarIconEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_TASKBAR_RIGHT_UP, wxTaskBarIconEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_TASKBAR_LEFT_DCLICK, wxTaskBarIconEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_TASKBAR_RIGHT_DCLICK, wxTaskBarIconEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_TASKBAR_BALLOON_TIMEOUT, wxTaskBarIconEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_TASKBAR_BALLOON_CLICK, wxTaskBarIconEvent );

#define wxTaskBarIconEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxTaskBarIconEventFunction, func)

#define wx__DECLARE_TASKBAREVT(evt, fn) \
    wx__DECLARE_EVT0(wxEVT_TASKBAR_ ## evt, wxTaskBarIconEventHandler(fn))

#define EVT_TASKBAR_MOVE(fn)         wx__DECLARE_TASKBAREVT(MOVE, fn)
#define EVT_TASKBAR_LEFT_DOWN(fn)    wx__DECLARE_TASKBAREVT(LEFT_DOWN, fn)
#define EVT_TASKBAR_LEFT_UP(fn)      wx__DECLARE_TASKBAREVT(LEFT_UP, fn)
#define EVT_TASKBAR_RIGHT_DOWN(fn)   wx__DECLARE_TASKBAREVT(RIGHT_DOWN, fn)
#define EVT_TASKBAR_RIGHT_UP(fn)     wx__DECLARE_TASKBAREVT(RIGHT_UP, fn)
#define EVT_TASKBAR_LEFT_DCLICK(fn)  wx__DECLARE_TASKBAREVT(LEFT_DCLICK, fn)
#define EVT_TASKBAR_RIGHT_DCLICK(fn) wx__DECLARE_TASKBAREVT(RIGHT_DCLICK, fn)

// taskbar menu is shown on right button press under all platforms except MSW
// where it's shown on right button release, using this event type and macro
// allows to write code which works correctly on all platforms
#ifdef __WXMSW__
    #define wxEVT_TASKBAR_CLICK wxEVT_TASKBAR_RIGHT_UP
#else
    #define wxEVT_TASKBAR_CLICK wxEVT_TASKBAR_RIGHT_DOWN
#endif
#define EVT_TASKBAR_CLICK(fn)        wx__DECLARE_TASKBAREVT(CLICK, fn)

// these events are currently generated only under wxMSW and only after (MSW-
// specific) ShowBalloon() had been called, don't use them in portable code
#define EVT_TASKBAR_BALLOON_TIMEOUT(fn) \
    wx__DECLARE_TASKBAREVT(BALLOON_TIMEOUT, fn)
#define EVT_TASKBAR_BALLOON_CLICK(fn) \
    wx__DECLARE_TASKBAREVT(BALLOON_CLICK, fn)

#endif // wxUSE_TASKBARICON

#endif // _WX_TASKBAR_H_BASE_
