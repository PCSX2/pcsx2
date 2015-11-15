///////////////////////////////////////////////////////////////////////////////
// Name:        wx/toplevel.h
// Purpose:     declares wxTopLevelWindow class, the base class for all
//              top level windows (such as frames and dialogs)
// Author:      Vadim Zeitlin, Vaclav Slavik
// Modified by:
// Created:     06.08.01
// Copyright:   (c) 2001 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
//                       Vaclav Slavik <vaclav@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TOPLEVEL_BASE_H_
#define _WX_TOPLEVEL_BASE_H_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/nonownedwnd.h"
#include "wx/iconbndl.h"
#include "wx/weakref.h"

// the default names for various classes
extern WXDLLIMPEXP_DATA_CORE(const char) wxFrameNameStr[];

class WXDLLIMPEXP_FWD_CORE wxTopLevelWindowBase;

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

/*
    Summary of the bits used (some of them are defined in wx/frame.h and
    wx/dialog.h and not here):

    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
    |15|14|13|12|11|10| 9| 8| 7| 6| 5| 4| 3| 2| 1| 0|
    +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
      |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |
      |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  \_ wxCENTRE
      |  |  |  |  |  |  |  |  |  |  |  |  |  |  \____ wxFRAME_NO_TASKBAR
      |  |  |  |  |  |  |  |  |  |  |  |  |  \_______ wxFRAME_TOOL_WINDOW
      |  |  |  |  |  |  |  |  |  |  |  |  \__________ wxFRAME_FLOAT_ON_PARENT
      |  |  |  |  |  |  |  |  |  |  |  \_____________ wxFRAME_SHAPED
      |  |  |  |  |  |  |  |  |  |  \________________ wxDIALOG_NO_PARENT
      |  |  |  |  |  |  |  |  |  \___________________ wxRESIZE_BORDER
      |  |  |  |  |  |  |  |  \______________________ wxTINY_CAPTION_VERT
      |  |  |  |  |  |  |  \_________________________
      |  |  |  |  |  |  \____________________________ wxMAXIMIZE_BOX
      |  |  |  |  |  \_______________________________ wxMINIMIZE_BOX
      |  |  |  |  \__________________________________ wxSYSTEM_MENU
      |  |  |  \_____________________________________ wxCLOSE_BOX
      |  |  \________________________________________ wxMAXIMIZE
      |  \___________________________________________ wxMINIMIZE
      \______________________________________________ wxSTAY_ON_TOP


    Notice that the 8 lower bits overlap with wxCENTRE and the button selection
    bits (wxYES, wxOK wxNO, wxCANCEL, wxAPPLY, wxCLOSE and wxNO_DEFAULT) which
    can be combined with the dialog style for several standard dialogs and
    hence shouldn't overlap with any styles which can be used for the dialogs.
    Additionally, wxCENTRE can be used with frames also.
 */

// style common to both wxFrame and wxDialog
#define wxSTAY_ON_TOP           0x8000
#define wxICONIZE               0x4000
#define wxMINIMIZE              wxICONIZE
#define wxMAXIMIZE              0x2000
#define wxCLOSE_BOX             0x1000  // == wxHELP so can't be used with it

#define wxSYSTEM_MENU           0x0800
#define wxMINIMIZE_BOX          0x0400
#define wxMAXIMIZE_BOX          0x0200

#define wxTINY_CAPTION          0x0080  // clashes with wxNO_DEFAULT
#define wxRESIZE_BORDER         0x0040  // == wxCLOSE

#if WXWIN_COMPATIBILITY_2_8
    // HORIZ and VERT styles are equivalent anyhow so don't use different names
    // for them
    #define wxTINY_CAPTION_HORIZ    wxTINY_CAPTION
    #define wxTINY_CAPTION_VERT     wxTINY_CAPTION
#endif

#if WXWIN_COMPATIBILITY_2_6

    // deprecated versions defined for compatibility reasons
    #define wxRESIZE_BOX            wxMAXIMIZE_BOX
    #define wxTHICK_FRAME           wxRESIZE_BORDER

    // obsolete styles, unused any more
    #define wxDIALOG_MODAL          0
    #define wxDIALOG_MODELESS       0
    #define wxNO_3D                 0
    #define wxUSER_COLOURS          0

#endif // WXWIN_COMPATIBILITY_2_6

// default style
//
// under Windows CE (at least when compiling with eVC 4) we should create
// top level windows without any styles at all for them to appear
// "correctly", i.e. as full screen windows with a "hide" button (same as
// "close" but round instead of squared and just hides the applications
// instead of closing it) in the title bar
#if defined(__WXWINCE__)
    #if defined(__SMARTPHONE__)
        #define wxDEFAULT_FRAME_STYLE (wxMAXIMIZE)
    #elif defined(__WINCE_STANDARDSDK__)
        #define wxDEFAULT_FRAME_STYLE (wxMAXIMIZE|wxCLOSE_BOX)
    #else
        #define wxDEFAULT_FRAME_STYLE (wxNO_BORDER)
    #endif
#else // !__WXWINCE__
    #define wxDEFAULT_FRAME_STYLE \
            (wxSYSTEM_MENU | \
             wxRESIZE_BORDER | \
             wxMINIMIZE_BOX | \
             wxMAXIMIZE_BOX | \
             wxCLOSE_BOX | \
             wxCAPTION | \
             wxCLIP_CHILDREN)
#endif


// Dialogs are created in a special way
#define wxTOPLEVEL_EX_DIALOG        0x00000008

// Styles for ShowFullScreen
// (note that wxTopLevelWindow only handles wxFULLSCREEN_NOBORDER and
//  wxFULLSCREEN_NOCAPTION; the rest is handled by wxTopLevelWindow)
enum
{
    wxFULLSCREEN_NOMENUBAR   = 0x0001,
    wxFULLSCREEN_NOTOOLBAR   = 0x0002,
    wxFULLSCREEN_NOSTATUSBAR = 0x0004,
    wxFULLSCREEN_NOBORDER    = 0x0008,
    wxFULLSCREEN_NOCAPTION   = 0x0010,

    wxFULLSCREEN_ALL         = wxFULLSCREEN_NOMENUBAR | wxFULLSCREEN_NOTOOLBAR |
                               wxFULLSCREEN_NOSTATUSBAR | wxFULLSCREEN_NOBORDER |
                               wxFULLSCREEN_NOCAPTION
};

// Styles for RequestUserAttention
enum
{
    wxUSER_ATTENTION_INFO = 1,
    wxUSER_ATTENTION_ERROR = 2
};

// ----------------------------------------------------------------------------
// wxTopLevelWindow: a top level (as opposed to child) window
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxTopLevelWindowBase : public wxNonOwnedWindow
{
public:
    // construction
    wxTopLevelWindowBase();
    virtual ~wxTopLevelWindowBase();

    // top level wnd state
    // --------------------

    // maximize = true => maximize, otherwise - restore
    virtual void Maximize(bool maximize = true) = 0;

    // undo Maximize() or Iconize()
    virtual void Restore() = 0;

    // iconize = true => iconize, otherwise - restore
    virtual void Iconize(bool iconize = true) = 0;

    // return true if the frame is maximized
    virtual bool IsMaximized() const = 0;

    // return true if the frame is always maximized
    // due to native guidelines or current policy
    virtual bool IsAlwaysMaximized() const;

    // return true if the frame is iconized
    virtual bool IsIconized() const = 0;

    // get the frame icon
    wxIcon GetIcon() const;

    // get the frame icons
    const wxIconBundle& GetIcons() const { return m_icons; }

    // set the frame icon: implemented in terms of SetIcons()
    void SetIcon(const wxIcon& icon);

    // set the frame icons
    virtual void SetIcons(const wxIconBundle& icons) { m_icons = icons; }

    // maximize the window to cover entire screen
    virtual bool ShowFullScreen(bool show, long style = wxFULLSCREEN_ALL) = 0;

    // shows the window, but doesn't activate it. If the base code is being run,
    // it means the port doesn't implement this method yet and so alert the user.
    virtual void ShowWithoutActivating() {
        wxFAIL_MSG("ShowWithoutActivating not implemented on this platform.");
    }

    // return true if the frame is in fullscreen mode
    virtual bool IsFullScreen() const = 0;

    // the title of the top level window: the text which the
    // window shows usually at the top of the frame/dialog in dedicated bar
    virtual void SetTitle(const wxString& title) = 0;
    virtual wxString GetTitle() const = 0;

    // enable/disable close button [x]
    virtual bool EnableCloseButton(bool WXUNUSED(enable) ) { return false; }

    // Attracts the users attention to this window if the application is
    // inactive (should be called when a background event occurs)
    virtual void RequestUserAttention(int flags = wxUSER_ATTENTION_INFO);

    // Is this the active frame (highlighted in the taskbar)?
    //
    // A TLW is active only if it contains the currently focused window.
    virtual bool IsActive() { return IsDescendant(FindFocus()); }

    // this function may be overridden to return false to allow closing the
    // application even when this top level window is still open
    //
    // notice that the window is still closed prior to the application exit and
    // so it can still veto it even if it returns false from here
    virtual bool ShouldPreventAppExit() const { return true; }


#if defined(__SMARTPHONE__)
    virtual void SetLeftMenu(int id = wxID_ANY, const wxString& label = wxEmptyString, wxMenu *subMenu = NULL) = 0;
    virtual void SetRightMenu(int id = wxID_ANY, const wxString& label = wxEmptyString, wxMenu *subMenu = NULL) = 0;
#endif // __SMARTPHONE__

    // centre the window on screen: this is just a shortcut
    void CentreOnScreen(int dir = wxBOTH) { DoCentre(dir | wxCENTRE_ON_SCREEN); }
    void CenterOnScreen(int dir = wxBOTH) { CentreOnScreen(dir); }

    // Get the default size for a new top level window. This is used when
    // creating a wxTLW under some platforms if no explicit size given.
    static wxSize GetDefaultSize();


    // default item access: we have a permanent default item which is the one
    // set by the user code but we may also have a temporary default item which
    // would be chosen if the user pressed "Enter" now but the default action
    // reverts to the "permanent" default as soon as this temporary default
    // item loses focus

    // get the default item, temporary or permanent
    wxWindow *GetDefaultItem() const
        { return m_winTmpDefault ? m_winTmpDefault : m_winDefault; }

    // set the permanent default item, return the old default
    wxWindow *SetDefaultItem(wxWindow *win)
        { wxWindow *old = GetDefaultItem(); m_winDefault = win; return old; }

    // return the temporary default item, can be NULL
    wxWindow *GetTmpDefaultItem() const { return m_winTmpDefault; }

    // set a temporary default item, SetTmpDefaultItem(NULL) should be called
    // soon after a call to SetTmpDefaultItem(window), return the old default
    wxWindow *SetTmpDefaultItem(wxWindow *win)
        { wxWindow *old = GetDefaultItem(); m_winTmpDefault = win; return old; }

    // implementation only from now on
    // -------------------------------

    // override some base class virtuals
    virtual bool Destroy();
    virtual bool IsTopLevel() const { return true; }
    virtual bool IsTopNavigationDomain() const { return true; }
    virtual bool IsVisible() const { return IsShown(); }

    // event handlers
    void OnCloseWindow(wxCloseEvent& event);
    void OnSize(wxSizeEvent& WXUNUSED(event)) { DoLayout(); }

    // Get rect to be used to center top-level children
    virtual void GetRectForTopLevelChildren(int *x, int *y, int *w, int *h);

    // this should go away, but for now it's called from docview.cpp,
    // so should be there for all platforms
    void OnActivate(wxActivateEvent &WXUNUSED(event)) { }

    // do the window-specific processing after processing the update event
    virtual void DoUpdateWindowUI(wxUpdateUIEvent& event) ;

    // a different API for SetSizeHints
    virtual void SetMinSize(const wxSize& minSize);
    virtual void SetMaxSize(const wxSize& maxSize);

    virtual void OSXSetModified(bool modified) { m_modified = modified; }
    virtual bool OSXIsModified() const { return m_modified; }

    virtual void SetRepresentedFilename(const wxString& WXUNUSED(filename)) { }

#if wxUSE_MENUS || wxUSE_TOOLBAR
    // show help text for the currently selected menu or toolbar item
    // (typically in the status bar) or hide it and restore the status bar text
    // originally shown before the menu was opened if show == false
    virtual void DoGiveHelp(const wxString& WXUNUSED(text), bool WXUNUSED(show))  {}
#endif

protected:
    // the frame client to screen translation should take account of the
    // toolbar which may shift the origin of the client area
    virtual void DoClientToScreen(int *x, int *y) const;
    virtual void DoScreenToClient(int *x, int *y) const;

    // add support for wxCENTRE_ON_SCREEN
    virtual void DoCentre(int dir);

    // no need to do client to screen translation to get our position in screen
    // coordinates: this is already the case
    virtual void DoGetScreenPosition(int *x, int *y) const
    {
        DoGetPosition(x, y);
    }

    // test whether this window makes part of the frame
    // (menubar, toolbar and statusbar are excluded from automatic layout)
    virtual bool IsOneOfBars(const wxWindow *WXUNUSED(win)) const
        { return false; }

    // check if we should exit the program after deleting this window
    bool IsLastBeforeExit() const;

    // send the iconize event, return true if processed
    bool SendIconizeEvent(bool iconized = true);

    // do TLW-specific layout: we resize our unique child to fill the entire
    // client area
    void DoLayout();

    static int WidthDefault(int w) { return w == wxDefaultCoord ? GetDefaultSize().x : w; }
    static int HeightDefault(int h) { return h == wxDefaultCoord ? GetDefaultSize().y : h; }


    // the frame icon
    wxIconBundle m_icons;

    // a default window (usually a button) or NULL
    wxWindowRef m_winDefault;

    // a temporary override of m_winDefault, use the latter if NULL
    wxWindowRef m_winTmpDefault;

    bool m_modified;

    wxDECLARE_NO_COPY_CLASS(wxTopLevelWindowBase);
    DECLARE_EVENT_TABLE()
};


// include the real class declaration
#if defined(__WXMSW__)
    #include "wx/msw/toplevel.h"
    #define wxTopLevelWindowNative wxTopLevelWindowMSW
#elif defined(__WXGTK20__)
    #include "wx/gtk/toplevel.h"
    #define wxTopLevelWindowNative wxTopLevelWindowGTK
#elif defined(__WXGTK__)
    #include "wx/gtk1/toplevel.h"
    #define wxTopLevelWindowNative wxTopLevelWindowGTK
#elif defined(__WXX11__)
    #include "wx/x11/toplevel.h"
    #define wxTopLevelWindowNative wxTopLevelWindowX11
#elif defined(__WXDFB__)
    #include "wx/dfb/toplevel.h"
    #define wxTopLevelWindowNative wxTopLevelWindowDFB
#elif defined(__WXMAC__)
    #include "wx/osx/toplevel.h"
    #define wxTopLevelWindowNative wxTopLevelWindowMac
#elif defined(__WXCOCOA__)
    #include "wx/cocoa/toplevel.h"
    #define wxTopLevelWindowNative wxTopLevelWindowCocoa
#elif defined(__WXPM__)
    #include "wx/os2/toplevel.h"
    #define wxTopLevelWindowNative wxTopLevelWindowOS2
#elif defined(__WXMOTIF__)
    #include "wx/motif/toplevel.h"
    #define wxTopLevelWindowNative wxTopLevelWindowMotif
#endif

#ifdef __WXUNIVERSAL__
    #include "wx/univ/toplevel.h"
#else // !__WXUNIVERSAL__
    class WXDLLIMPEXP_CORE wxTopLevelWindow : public wxTopLevelWindowNative
    {
    public:
        // construction
        wxTopLevelWindow() { }
        wxTopLevelWindow(wxWindow *parent,
                   wxWindowID winid,
                   const wxString& title,
                   const wxPoint& pos = wxDefaultPosition,
                   const wxSize& size = wxDefaultSize,
                   long style = wxDEFAULT_FRAME_STYLE,
                   const wxString& name = wxFrameNameStr)
            : wxTopLevelWindowNative(parent, winid, title,
                                     pos, size, style, name)
        {
        }

        DECLARE_DYNAMIC_CLASS_NO_COPY(wxTopLevelWindow)
    };
#endif // __WXUNIVERSAL__/!__WXUNIVERSAL__

#endif // _WX_TOPLEVEL_BASE_H_
