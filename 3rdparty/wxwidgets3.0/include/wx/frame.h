/////////////////////////////////////////////////////////////////////////////
// Name:        wx/frame.h
// Purpose:     wxFrame class interface
// Author:      Vadim Zeitlin
// Modified by:
// Created:     15.11.99
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FRAME_H_BASE_
#define _WX_FRAME_H_BASE_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/toplevel.h"      // the base class
#include "wx/statusbr.h"

// the default names for various classs
extern WXDLLIMPEXP_DATA_CORE(const char) wxStatusLineNameStr[];
extern WXDLLIMPEXP_DATA_CORE(const char) wxToolBarNameStr[];

class WXDLLIMPEXP_FWD_CORE wxFrame;
class WXDLLIMPEXP_FWD_CORE wxMenuBar;
class WXDLLIMPEXP_FWD_CORE wxMenuItem;
class WXDLLIMPEXP_FWD_CORE wxStatusBar;
class WXDLLIMPEXP_FWD_CORE wxToolBar;

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// wxFrame-specific (i.e. not for wxDialog) styles
//
// Also see the bit summary table in wx/toplevel.h.
#define wxFRAME_NO_TASKBAR      0x0002  // No taskbar button (MSW only)
#define wxFRAME_TOOL_WINDOW     0x0004  // No taskbar button, no system menu
#define wxFRAME_FLOAT_ON_PARENT 0x0008  // Always above its parent

// ----------------------------------------------------------------------------
// wxFrame is a top-level window with optional menubar, statusbar and toolbar
//
// For each of *bars, a frame may have several of them, but only one is
// managed by the frame, i.e. resized/moved when the frame is and whose size
// is accounted for in client size calculations - all others should be taken
// care of manually. The CreateXXXBar() functions create this, main, XXXBar,
// but the actual creation is done in OnCreateXXXBar() functions which may be
// overridden to create custom objects instead of standard ones when
// CreateXXXBar() is called.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxFrameBase : public wxTopLevelWindow
{
public:
    // construction
    wxFrameBase();
    virtual ~wxFrameBase();

    wxFrame *New(wxWindow *parent,
                 wxWindowID winid,
                 const wxString& title,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxDefaultSize,
                 long style = wxDEFAULT_FRAME_STYLE,
                 const wxString& name = wxFrameNameStr);

    // frame state
    // -----------

    // get the origin of the client area (which may be different from (0, 0)
    // if the frame has a toolbar) in client coordinates
    virtual wxPoint GetClientAreaOrigin() const;


    // menu bar functions
    // ------------------

#if wxUSE_MENUS
    virtual void SetMenuBar(wxMenuBar *menubar);
    virtual wxMenuBar *GetMenuBar() const { return m_frameMenuBar; }

    // find the item by id in the frame menu bar: this is an internal function
    // and exists mainly in order to be overridden in the MDI parent frame
    // which also looks at its active child menu bar
    virtual wxMenuItem *FindItemInMenuBar(int menuId) const;

    // generate menu command corresponding to the given menu item
    //
    // returns true if processed
    bool ProcessCommand(wxMenuItem *item);

    // generate menu command corresponding to the given menu command id
    //
    // returns true if processed
    bool ProcessCommand(int winid);
#else
    bool ProcessCommand(int WXUNUSED(winid)) { return false; }
#endif // wxUSE_MENUS

    // status bar functions
    // --------------------
#if wxUSE_STATUSBAR
    // create the main status bar by calling OnCreateStatusBar()
    virtual wxStatusBar* CreateStatusBar(int number = 1,
                                         long style = wxSTB_DEFAULT_STYLE,
                                         wxWindowID winid = 0,
                                         const wxString& name = wxStatusLineNameStr);
    // return a new status bar
    virtual wxStatusBar *OnCreateStatusBar(int number,
                                           long style,
                                           wxWindowID winid,
                                           const wxString& name);
    // get the main status bar
    virtual wxStatusBar *GetStatusBar() const { return m_frameStatusBar; }

    // sets the main status bar
    virtual void SetStatusBar(wxStatusBar *statBar);

    // forward these to status bar
    virtual void SetStatusText(const wxString &text, int number = 0);
    virtual void SetStatusWidths(int n, const int widths_field[]);
    void PushStatusText(const wxString &text, int number = 0);
    void PopStatusText(int number = 0);

    // set the status bar pane the help will be shown in
    void SetStatusBarPane(int n) { m_statusBarPane = n; }
    int GetStatusBarPane() const { return m_statusBarPane; }
#endif // wxUSE_STATUSBAR

    // toolbar functions
    // -----------------

#if wxUSE_TOOLBAR
    // create main toolbar bycalling OnCreateToolBar()
    virtual wxToolBar* CreateToolBar(long style = -1,
                                     wxWindowID winid = wxID_ANY,
                                     const wxString& name = wxToolBarNameStr);
    // return a new toolbar
    virtual wxToolBar *OnCreateToolBar(long style,
                                       wxWindowID winid,
                                       const wxString& name );

    // get/set the main toolbar
    virtual wxToolBar *GetToolBar() const { return m_frameToolBar; }
    virtual void SetToolBar(wxToolBar *toolbar);
#endif // wxUSE_TOOLBAR

    // implementation only from now on
    // -------------------------------

    // event handlers
#if wxUSE_MENUS
#if wxUSE_STATUSBAR
    void OnMenuOpen(wxMenuEvent& event);
    void OnMenuClose(wxMenuEvent& event);
    void OnMenuHighlight(wxMenuEvent& event);
#endif // wxUSE_STATUSBAR

    // send wxUpdateUIEvents for all menu items in the menubar,
    // or just for menu if non-NULL
    virtual void DoMenuUpdates(wxMenu* menu = NULL);
#endif // wxUSE_MENUS

    // do the UI update processing for this window
    virtual void UpdateWindowUI(long flags = wxUPDATE_UI_NONE);

    // Implement internal behaviour (menu updating on some platforms)
    virtual void OnInternalIdle();

#if wxUSE_MENUS || wxUSE_TOOLBAR
    // show help text for the currently selected menu or toolbar item
    // (typically in the status bar) or hide it and restore the status bar text
    // originally shown before the menu was opened if show == false
    virtual void DoGiveHelp(const wxString& text, bool show);
#endif

    virtual bool IsClientAreaChild(const wxWindow *child) const
    {
        return !IsOneOfBars(child) && wxTopLevelWindow::IsClientAreaChild(child);
    }

protected:
    // the frame main menu/status/tool bars
    // ------------------------------------

    // this (non virtual!) function should be called from dtor to delete the
    // main menubar, statusbar and toolbar (if any)
    void DeleteAllBars();

    // test whether this window makes part of the frame
    virtual bool IsOneOfBars(const wxWindow *win) const;

#if wxUSE_MENUS
    // override to update menu bar position when the frame size changes
    virtual void PositionMenuBar() { }

    // override to do something special when the menu bar is being removed
    // from the frame
    virtual void DetachMenuBar();

    // override to do something special when the menu bar is attached to the
    // frame
    virtual void AttachMenuBar(wxMenuBar *menubar);

    // Return true if we should update the menu item state from idle event
    // handler or false if we should delay it until the menu is opened.
    static bool ShouldUpdateMenuFromIdle();

    wxMenuBar *m_frameMenuBar;
#endif // wxUSE_MENUS

#if wxUSE_STATUSBAR && (wxUSE_MENUS || wxUSE_TOOLBAR)
    // the saved status bar text overwritten by DoGiveHelp()
    wxString m_oldStatusText;

    // the last help string we have shown in the status bar
    wxString m_lastHelpShown;
#endif

#if wxUSE_STATUSBAR
    // override to update status bar position (or anything else) when
    // something changes
    virtual void PositionStatusBar() { }

    // show the help string for the given menu item using DoGiveHelp() if the
    // given item does have a help string (as determined by FindInMenuBar()),
    // return false if there is no help for such item
    bool ShowMenuHelp(int helpid);

    wxStatusBar *m_frameStatusBar;
#endif // wxUSE_STATUSBAR


    int m_statusBarPane;

#if wxUSE_TOOLBAR
    // override to update status bar position (or anything else) when
    // something changes
    virtual void PositionToolBar() { }

    wxToolBar *m_frameToolBar;
#endif // wxUSE_TOOLBAR

#if wxUSE_MENUS && wxUSE_STATUSBAR
    DECLARE_EVENT_TABLE()
#endif // wxUSE_MENUS && wxUSE_STATUSBAR

    wxDECLARE_NO_COPY_CLASS(wxFrameBase);
};

// include the real class declaration
#if defined(__WXUNIVERSAL__) // && !defined(__WXMICROWIN__)
    #include "wx/univ/frame.h"
#else // !__WXUNIVERSAL__
    #if defined(__WXMSW__)
        #include "wx/msw/frame.h"
    #elif defined(__WXGTK20__)
        #include "wx/gtk/frame.h"
    #elif defined(__WXGTK__)
        #include "wx/gtk1/frame.h"
    #elif defined(__WXMOTIF__)
        #include "wx/motif/frame.h"
    #elif defined(__WXMAC__)
        #include "wx/osx/frame.h"
    #elif defined(__WXCOCOA__)
        #include "wx/cocoa/frame.h"
    #elif defined(__WXPM__)
        #include "wx/os2/frame.h"
    #endif
#endif

#endif
    // _WX_FRAME_H_BASE_
