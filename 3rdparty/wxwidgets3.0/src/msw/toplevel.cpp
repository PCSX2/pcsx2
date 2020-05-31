///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/toplevel.cpp
// Purpose:     implements wxTopLevelWindow for MSW
// Author:      Vadim Zeitlin
// Modified by:
// Created:     24.09.01
// Copyright:   (c) 2001 SciTech Software, Inc. (www.scitechsoft.com)
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/toplevel.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/dialog.h"
    #include "wx/string.h"
    #include "wx/log.h"
    #include "wx/intl.h"
    #include "wx/frame.h"
    #include "wx/menu.h"
    #include "wx/containr.h"        // wxSetFocusToChild()
    #include "wx/module.h"
#endif //WX_PRECOMP

#include "wx/dynlib.h"

#include "wx/msw/private.h"
#if defined(__WXWINCE__) && !defined(__HANDHELDPC__)
    #include <ole2.h>
    #include <shellapi.h>
    // Standard SDK doesn't have aygshell.dll: see include/wx/msw/wince/libraries.h
    #if _WIN32_WCE < 400 || !defined(__WINCE_STANDARDSDK__)
        #include <aygshell.h>
    #endif
#endif

#include "wx/msw/winundef.h"
#include "wx/msw/missing.h"

#include "wx/display.h"

#ifndef ICON_BIG
    #define ICON_BIG 1
#endif

#ifndef ICON_SMALL
    #define ICON_SMALL 0
#endif

// FIXME-VC6: Only VC6 doesn't have this in its standard headers so this
//            could be removed once support for it is dropped.
#ifndef WM_UNINITMENUPOPUP
    #define WM_UNINITMENUPOPUP 0x0125
#endif

// ----------------------------------------------------------------------------
// globals
// ----------------------------------------------------------------------------

#if wxUSE_MENUS || wxUSE_MENUS_NATIVE
    extern wxMenu *wxCurrentPopupMenu;
#endif // wxUSE_MENUS || wxUSE_MENUS_NATIVE


// ----------------------------------------------------------------------------
// stubs for missing functions under MicroWindows
// ----------------------------------------------------------------------------

#ifdef __WXMICROWIN__

// static inline bool IsIconic(HWND WXUNUSED(hwnd)) { return false; }
static inline bool IsZoomed(HWND WXUNUSED(hwnd)) { return false; }

#endif // __WXMICROWIN__

// NB: wxDlgProc must be defined here and not in dialog.cpp because the latter
//     is not included by wxUniv build which does need wxDlgProc
LONG APIENTRY _EXPORT
wxDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// ----------------------------------------------------------------------------
// wxTLWHiddenParentModule: used to manage the hidden parent window (we need a
// module to ensure that the window is always deleted)
// ----------------------------------------------------------------------------

class wxTLWHiddenParentModule : public wxModule
{
public:
    // module init/finalize
    virtual bool OnInit();
    virtual void OnExit();

    // get the hidden window (creates on demand)
    static HWND GetHWND();

private:
    // the HWND of the hidden parent
    static HWND ms_hwnd;

    // the class used to create it
    static const wxChar *ms_className;

    DECLARE_DYNAMIC_CLASS(wxTLWHiddenParentModule)
};

IMPLEMENT_DYNAMIC_CLASS(wxTLWHiddenParentModule, wxModule)

// ============================================================================
// wxTopLevelWindowMSW implementation
// ============================================================================

BEGIN_EVENT_TABLE(wxTopLevelWindowMSW, wxTopLevelWindowBase)
    EVT_ACTIVATE(wxTopLevelWindowMSW::OnActivate)
END_EVENT_TABLE()

// ----------------------------------------------------------------------------
// wxTopLevelWindowMSW creation
// ----------------------------------------------------------------------------

void wxTopLevelWindowMSW::Init()
{
    m_iconized =
    m_maximizeOnShow = false;

    // Data to save/restore when calling ShowFullScreen
    m_fsStyle = 0;
    m_fsOldWindowStyle = 0;
    m_fsIsMaximized = false;
    m_fsIsShowing = false;

    m_winLastFocused = NULL;

#if defined(__SMARTPHONE__) && defined(__WXWINCE__)
    m_MenuBarHWND = 0;
#endif

#if defined(__SMARTPHONE__) || defined(__POCKETPC__)
    SHACTIVATEINFO* info = new SHACTIVATEINFO;
    wxZeroMemory(*info);
    info->cbSize = sizeof(SHACTIVATEINFO);

    m_activateInfo = (void*) info;
#endif

    m_menuSystem = NULL;
    m_menuDepth = 0;
}

WXDWORD wxTopLevelWindowMSW::MSWGetStyle(long style, WXDWORD *exflags) const
{
    // let the base class deal with the common styles but fix the ones which
    // don't make sense for us (we also deal with the borders ourselves)
    WXDWORD msflags = wxWindow::MSWGetStyle
                      (
                        (style & ~wxBORDER_MASK) | wxBORDER_NONE, exflags
                      ) & ~WS_CHILD & ~WS_VISIBLE;

    // For some reason, WS_VISIBLE needs to be defined on creation for
    // SmartPhone 2003. The title can fail to be displayed otherwise.
#if defined(__SMARTPHONE__) || (defined(__WXWINCE__) && _WIN32_WCE < 400)
    msflags |= WS_VISIBLE;
    ((wxTopLevelWindowMSW*)this)->wxWindowBase::Show(true);
#endif

    // first select the kind of window being created
    //
    // note that if we don't set WS_POPUP, Windows assumes WS_OVERLAPPED and
    // creates a window with both caption and border, hence we need to use
    // WS_POPUP in a few cases just to avoid having caption/border which we
    // don't want

    // border and caption styles
    if ( ( style & wxRESIZE_BORDER ) && !IsAlwaysMaximized())
        msflags |= WS_THICKFRAME;
    else if ( exflags && ((style & wxBORDER_DOUBLE) || (style & wxBORDER_RAISED)) )
        *exflags |= WS_EX_DLGMODALFRAME;
    else if ( !(style & wxBORDER_NONE) )
        msflags |= WS_BORDER;
#ifndef __POCKETPC__
    else
        msflags |= WS_POPUP;
#endif

    // normally we consider that all windows without a caption must be popups,
    // but CE is an exception: there windows normally do not have the caption
    // but shouldn't be made popups as popups can't have menus and don't look
    // like normal windows anyhow

    // TODO: Smartphone appears to like wxCAPTION, but we should check that
    // we need it.
#if defined(__SMARTPHONE__) || !defined(__WXWINCE__)
    if ( style & wxCAPTION )
        msflags |= WS_CAPTION;
#ifndef __WXWINCE__
    else
        msflags |= WS_POPUP;
#endif // !__WXWINCE__
#endif

    // next translate the individual flags

    // WS_EX_CONTEXTHELP is incompatible with WS_MINIMIZEBOX and WS_MAXIMIZEBOX
    // and is ignored if we specify both of them, but chances are that if we
    // use wxWS_EX_CONTEXTHELP, we really do want to have the context help
    // button while wxMINIMIZE/wxMAXIMIZE are included by default, so the help
    // takes precedence
    if ( !(GetExtraStyle() & wxWS_EX_CONTEXTHELP) )
    {
        if ( style & wxMINIMIZE_BOX )
            msflags |= WS_MINIMIZEBOX;
        if ( style & wxMAXIMIZE_BOX )
            msflags |= WS_MAXIMIZEBOX;
    }

#ifndef __WXWINCE__
    // notice that if wxCLOSE_BOX is specified we need to use WS_SYSMENU too as
    // otherwise the close box doesn't appear
    if ( style & (wxSYSTEM_MENU | wxCLOSE_BOX) )
        msflags |= WS_SYSMENU;
#endif // !__WXWINCE__

    // NB: under CE these 2 styles are not supported currently, we should
    //     call Minimize()/Maximize() "manually" if we want to support them
    if ( style & wxMINIMIZE )
        msflags |= WS_MINIMIZE;

    if ( style & wxMAXIMIZE )
        msflags |= WS_MAXIMIZE;

    // Keep this here because it saves recoding this function in wxTinyFrame
    if ( style & wxTINY_CAPTION )
        msflags |= WS_CAPTION;

    if ( exflags )
    {
        // there is no taskbar under CE, so omit all this
#if !defined(__WXWINCE__)
        if ( !(GetExtraStyle() & wxTOPLEVEL_EX_DIALOG) )
        {
            if ( style & wxFRAME_TOOL_WINDOW )
            {
                // create the palette-like window
                *exflags |= WS_EX_TOOLWINDOW;

                // tool windows shouldn't appear on the taskbar (as documented)
                style |= wxFRAME_NO_TASKBAR;
            }

            // We have to solve 2 different problems here:
            //
            // 1. frames with wxFRAME_NO_TASKBAR flag shouldn't appear in the
            //    taskbar even if they don't have a parent
            //
            // 2. frames without this style should appear in the taskbar even
            //    if they're owned (Windows only puts non owned windows into
            //    the taskbar normally)
            //
            // The second one is solved here by using WS_EX_APPWINDOW flag, the
            // first one is dealt with in our MSWGetParent() method
            // implementation
            if ( !(style & wxFRAME_NO_TASKBAR) && GetParent() )
            {
                // need to force the frame to appear in the taskbar
                *exflags |= WS_EX_APPWINDOW;
            }
            //else: nothing to do [here]
        }

        if ( GetExtraStyle() & wxWS_EX_CONTEXTHELP )
            *exflags |= WS_EX_CONTEXTHELP;
#endif // !__WXWINCE__

        if ( style & wxSTAY_ON_TOP )
            *exflags |= WS_EX_TOPMOST;
    }

    return msflags;
}

WXHWND wxTopLevelWindowMSW::MSWGetParent() const
{
    // for the frames without wxFRAME_FLOAT_ON_PARENT style we should use NULL
    // parent HWND or it would be always on top of its parent which is not what
    // we usually want (in fact, we only want it for frames with the
    // wxFRAME_FLOAT_ON_PARENT flag)
    HWND hwndParent = NULL;
    if ( HasFlag(wxFRAME_FLOAT_ON_PARENT) )
    {
        const wxWindow *parent = GetParent();

        if ( !parent )
        {
            // this flag doesn't make sense then and will be ignored
            wxFAIL_MSG( wxT("wxFRAME_FLOAT_ON_PARENT but no parent?") );
        }
        else
        {
            hwndParent = GetHwndOf(parent);
        }
    }
    //else: don't float on parent, must not be owned

    // now deal with the 2nd taskbar-related problem (see comments above in
    // MSWGetStyle())
    if ( HasFlag(wxFRAME_NO_TASKBAR) && !hwndParent )
    {
        // use hidden parent
        hwndParent = wxTLWHiddenParentModule::GetHWND();
    }

    return (WXHWND)hwndParent;
}

#if defined(__SMARTPHONE__) || defined(__POCKETPC__)
bool wxTopLevelWindowMSW::HandleSettingChange(WXWPARAM wParam, WXLPARAM lParam)
{
    SHACTIVATEINFO *info = (SHACTIVATEINFO*) m_activateInfo;
    if ( info )
    {
        SHHandleWMSettingChange(GetHwnd(), wParam, lParam, info);
    }

    return wxWindowMSW::HandleSettingChange(wParam, lParam);
}
#endif

WXLRESULT wxTopLevelWindowMSW::MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam)
{
    WXLRESULT rc = 0;
    bool processed = false;

    switch ( message )
    {
#if defined(__SMARTPHONE__) || defined(__POCKETPC__)
        case WM_ACTIVATE:
        {
            SHACTIVATEINFO* info = (SHACTIVATEINFO*) m_activateInfo;
            if (info)
            {
                DWORD flags = 0;
                if (GetExtraStyle() & wxTOPLEVEL_EX_DIALOG) flags = SHA_INPUTDIALOG;
                SHHandleWMActivate(GetHwnd(), wParam, lParam, info, flags);
            }

            // This implicitly sends a wxEVT_ACTIVATE_APP event
            if (wxTheApp)
                wxTheApp->SetActive(wParam != 0, FindFocus());

            break;
        }
        case WM_HIBERNATE:
        {
            if (wxTheApp)
            {
                wxActivateEvent event(wxEVT_HIBERNATE, true, wxID_ANY);
                event.SetEventObject(wxTheApp);
                processed = wxTheApp->ProcessEvent(event);
            }
            break;
        }
#endif // __SMARTPHONE__ || __POCKETPC__

        case WM_SYSCOMMAND:
            {
                // From MSDN:
                //
                //      ... the four low-order bits of the wParam parameter are
                //      used internally by the system. To obtain the correct
                //      result when testing the value of wParam, an application
                //      must combine the value 0xFFF0 with the wParam value by
                //      using the bitwise AND operator.
                unsigned id = wParam & 0xfff0;

                // Preserve the focus when minimizing/restoring the window: we
                // need to do it manually as DefWindowProc() doesn't appear to
                // do this for us for some reason (perhaps because we don't use
                // WM_NEXTDLGCTL for setting focus?). Moreover, our code in
                // OnActivate() doesn't work in this case as we receive the
                // deactivation event too late when the window is being
                // minimized and the focus is already NULL by then. Similarly,
                // we receive the activation event too early and restoring
                // focus in it fails because the window is still minimized. So
                // we need to do it here.
                if ( id == SC_MINIMIZE )
                {
                    // For minimization, it's simple enough: just save the
                    // focus as usual. The important thing is that we're not
                    // minimized yet, so this works correctly.
                    DoSaveLastFocus();
                }
                else if ( id == SC_RESTORE )
                {
                    // For restoring, it's trickier as DefWindowProc() sets
                    // focus to the window itself. So run it first and restore
                    // our saved focus only afterwards.
                    processed = true;
                    rc = wxTopLevelWindowBase::MSWWindowProc(message,
                                                             wParam, lParam);

                    DoRestoreLastFocus();
                }

#ifndef __WXUNIVERSAL__
                // We need to generate events for the custom items added to the
                // system menu if it had been created (and presumably modified).
                // As SC_SIZE is the first of the system-defined commands, we
                // only do this for the custom commands before it and leave
                // SC_SIZE and everything after it to DefWindowProc().
                if ( m_menuSystem && id < SC_SIZE )
                {
                    if ( m_menuSystem->MSWCommand(0 /* unused anyhow */, id) )
                        processed = true;
                }
#endif // #ifndef __WXUNIVERSAL__
            }
            break;

#if !defined(__WXMICROWIN__) && !defined(__WXWINCE__)
#if wxUSE_MENUS
        case WM_INITMENUPOPUP:
            processed = HandleMenuPopup(wxEVT_MENU_OPEN, (WXHMENU)wParam);
            break;

        case WM_MENUSELECT:
            {
                WXWORD item, flags;
                WXHMENU hmenu;
                UnpackMenuSelect(wParam, lParam, &item, &flags, &hmenu);

                processed = HandleMenuSelect(item, flags, hmenu);
            }
            break;

        case WM_EXITMENULOOP:
            // Under Windows 98 and 2000 and later we're going to get
            // WM_UNINITMENUPOPUP which will be used to generate this event
            // with more information (notably the menu that was closed) so we
            // only need this one under old Windows systems where the newer
            // event is never sent.
            if ( wxGetWinVersion() < wxWinVersion_98 )
                processed = HandleExitMenuLoop(wParam);
            break;

        case WM_UNINITMENUPOPUP:
            processed = HandleMenuPopup(wxEVT_MENU_CLOSE, (WXHMENU)wParam);
            break;
#endif // wxUSE_MENUS
#endif // !__WXMICROWIN__
    }

    if ( !processed )
        rc = wxTopLevelWindowBase::MSWWindowProc(message, wParam, lParam);

    return rc;
}

bool wxTopLevelWindowMSW::CreateDialog(const void *dlgTemplate,
                                       const wxString& title,
                                       const wxPoint& pos,
                                       const wxSize& size)
{
#ifdef __WXMICROWIN__
    // no dialogs support under MicroWin yet
    return CreateFrame(title, pos, size);
#else // !__WXMICROWIN__
    // static cast is valid as we're only ever called for dialogs
    wxWindow * const
        parent = static_cast<wxDialog *>(this)->GetParentForModalDialog();

    m_hWnd = (WXHWND)::CreateDialogIndirect
                       (
                        wxGetInstance(),
                        (DLGTEMPLATE*)dlgTemplate,
                        parent ? GetHwndOf(parent) : NULL,
                        (DLGPROC)wxDlgProc
                       );

    if ( !m_hWnd )
    {
        wxFAIL_MSG(wxT("Failed to create dialog. Incorrect DLGTEMPLATE?"));

        wxLogSysError(wxT("Can't create dialog using memory template"));

        return false;
    }

#if !defined(__WXWINCE__)
    // For some reason, the system menu is activated when we use the
    // WS_EX_CONTEXTHELP style, so let's set a reasonable icon
    if ( HasExtraStyle(wxWS_EX_CONTEXTHELP) )
    {
        wxFrame *winTop = wxDynamicCast(wxTheApp->GetTopWindow(), wxFrame);
        if ( winTop )
        {
            wxIcon icon = winTop->GetIcon();
            if ( icon.IsOk() )
            {
                ::SendMessage(GetHwnd(), WM_SETICON,
                              (WPARAM)TRUE,
                              (LPARAM)GetHiconOf(icon));
            }
        }
    }
#endif // !__WXWINCE__

    if ( !title.empty() )
    {
        ::SetWindowText(GetHwnd(), title.t_str());
    }

    SubclassWin(m_hWnd);

#if !defined(__WXWINCE__) || defined(__WINCE_STANDARDSDK__)
    // move the dialog to its initial position without forcing repainting
    int x, y, w, h;
    (void)MSWGetCreateWindowCoords(pos, size, x, y, w, h);

    if ( x == (int)CW_USEDEFAULT )
    {
        // Let the system position the window, just set its size.
        ::SetWindowPos(GetHwnd(), 0,
                       0, 0, w, h,
                       SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    else // Move the window to the desired location and set its size too.
    {
        if ( !::MoveWindow(GetHwnd(), x, y, w, h, FALSE) )
        {
            wxLogLastError(wxT("MoveWindow"));
        }
    }
#endif // !__WXWINCE__

#ifdef __SMARTPHONE__
    // Work around title non-display glitch
    Show(false);
#endif

    return true;
#endif // __WXMICROWIN__/!__WXMICROWIN__
}

bool wxTopLevelWindowMSW::CreateFrame(const wxString& title,
                                      const wxPoint& pos,
                                      const wxSize& size)
{
    WXDWORD exflags;
    WXDWORD flags = MSWGetCreateWindowFlags(&exflags);

    const wxSize sz = IsAlwaysMaximized() ? wxDefaultSize : size;

#ifndef __WXWINCE__
    if ( wxTheApp->GetLayoutDirection() == wxLayout_RightToLeft )
        exflags |= WS_EX_LAYOUTRTL;
#endif

    return MSWCreate(MSWGetRegisteredClassName(),
                     title.t_str(), pos, sz, flags, exflags);
}

bool wxTopLevelWindowMSW::Create(wxWindow *parent,
                                 wxWindowID id,
                                 const wxString& title,
                                 const wxPoint& pos,
                                 const wxSize& size,
                                 long style,
                                 const wxString& name)
{
    wxSize sizeReal = size;
    if ( !sizeReal.IsFullySpecified() )
    {
        sizeReal.SetDefaults(GetDefaultSize());
    }

    // notice that we should append this window to wxTopLevelWindows list
    // before calling CreateBase() as it behaves differently for TLW and
    // non-TLW windows
    wxTopLevelWindows.Append(this);

    bool ret = CreateBase(parent, id, pos, sizeReal, style, name);
    if ( !ret )
        return false;

    if ( parent )
        parent->AddChild(this);

    if ( GetExtraStyle() & wxTOPLEVEL_EX_DIALOG )
    {
        // we have different dialog templates to allows creation of dialogs
        // with & without captions under MSWindows, resizable or not (but a
        // resizable dialog always has caption - otherwise it would look too
        // strange)

        // we need 3 additional WORDs for dialog menu, class and title (as we
        // don't use DS_SETFONT we don't need the fourth WORD for the font)
        static const int dlgsize = sizeof(DLGTEMPLATE) + (sizeof(WORD) * 3);
        DLGTEMPLATE *dlgTemplate = (DLGTEMPLATE *)malloc(dlgsize);
        memset(dlgTemplate, 0, dlgsize);

        // these values are arbitrary, they won't be used normally anyhow
        const LONG baseUnits = ::GetDialogBaseUnits();
        dlgTemplate->x = 34;
        dlgTemplate->y  = 22;
        dlgTemplate->cx = ::MulDiv(sizeReal.x, 4, LOWORD(baseUnits));
        dlgTemplate->cy = ::MulDiv(sizeReal.y, 8, HIWORD(baseUnits));

        // reuse the code in MSWGetStyle() but correct the results slightly for
        // the dialog
        //
        // NB: we need a temporary variable as we can't pass pointer to
        //     dwExtendedStyle directly, it's not aligned correctly for 64 bit
        //     architectures
        WXDWORD dwExtendedStyle;
        dlgTemplate->style = MSWGetStyle(style, &dwExtendedStyle);
        dlgTemplate->dwExtendedStyle = dwExtendedStyle;

        // all dialogs are popups
        dlgTemplate->style |= WS_POPUP;

#ifndef __WXWINCE__
        if ( wxTheApp->GetLayoutDirection() == wxLayout_RightToLeft )
        {
            dlgTemplate->dwExtendedStyle |= WS_EX_LAYOUTRTL;
        }

        // force 3D-look if necessary, it looks impossibly ugly otherwise
        if ( style & (wxRESIZE_BORDER | wxCAPTION) )
            dlgTemplate->style |= DS_MODALFRAME;
#endif

        ret = CreateDialog(dlgTemplate, title, pos, sizeReal);
        free(dlgTemplate);
    }
    else // !dialog
    {
        ret = CreateFrame(title, pos, sizeReal);
    }

#ifndef __WXWINCE__
    if ( ret && !(GetWindowStyleFlag() & wxCLOSE_BOX) )
    {
        EnableCloseButton(false);
    }
#endif

    // for standard dialogs the dialog manager generates WM_CHANGEUISTATE
    // itself but for custom windows we have to do it ourselves in order to
    // make the keyboard indicators (such as underlines for accelerators and
    // focus rectangles) work under Win2k+
    if ( ret )
    {
        MSWUpdateUIState(UIS_INITIALIZE);
    }

    // Note: if we include PocketPC in this test, dialogs can fail to show up,
    // for example the text entry dialog in the dialogs sample. Problem with Maximise()?
#if defined(__WXWINCE__) && (defined(__SMARTPHONE__) || defined(__WINCE_STANDARDSDK__))
    if ( ( style & wxMAXIMIZE ) || IsAlwaysMaximized() )
    {
        this->Maximize();
    }
#endif

#if defined(__SMARTPHONE__) && defined(__WXWINCE__)
    SetRightMenu(); // to nothing for initialization
#endif

    return ret;
}

wxTopLevelWindowMSW::~wxTopLevelWindowMSW()
{
    delete m_menuSystem;

    SendDestroyEvent();

#if defined(__SMARTPHONE__) || defined(__POCKETPC__)
    SHACTIVATEINFO* info = (SHACTIVATEINFO*) m_activateInfo;
    delete info;
    m_activateInfo = NULL;
#endif

    // after destroying an owned window, Windows activates the next top level
    // window in Z order but it may be different from our owner (to reproduce
    // this simply Alt-TAB to another application and back before closing the
    // owned frame) whereas we always want to yield activation to our parent
    if ( HasFlag(wxFRAME_FLOAT_ON_PARENT) )
    {
        wxWindow *parent = GetParent();
        if ( parent )
        {
            ::BringWindowToTop(GetHwndOf(parent));
        }
    }
}

// ----------------------------------------------------------------------------
// wxTopLevelWindowMSW showing
// ----------------------------------------------------------------------------

void wxTopLevelWindowMSW::DoShowWindow(int nShowCmd)
{
    ::ShowWindow(GetHwnd(), nShowCmd);

    // Hiding the window doesn't change its iconized state.
    if ( nShowCmd != SW_HIDE )
    {
        // Otherwise restoring, maximizing or showing the window normally also
        // makes it not iconized and only minimizing it does make it iconized.
        m_iconized = nShowCmd == SW_MINIMIZE;
    }
}

void wxTopLevelWindowMSW::ShowWithoutActivating()
{
    if ( !wxWindowBase::Show(true) )
        return;

    DoShowWindow(SW_SHOWNA);
}

bool wxTopLevelWindowMSW::Show(bool show)
{
    // don't use wxWindow version as we want to call DoShowWindow() ourselves
    if ( !wxWindowBase::Show(show) )
        return false;

    int nShowCmd;
    if ( show )
    {
        if ( m_maximizeOnShow )
        {
            // show and maximize
            nShowCmd = SW_MAXIMIZE;

            // This is necessary, or no window appears
#if defined( __WINCE_STANDARDSDK__) || defined(__SMARTPHONE__)
            DoShowWindow(SW_SHOW);
#endif

            m_maximizeOnShow = false;
        }
        else if ( m_iconized )
        {
            // We were iconized while we were hidden, so now we need to show
            // the window in iconized state.
            nShowCmd = SW_MINIMIZE;
        }
        else if ( ::IsIconic(GetHwnd()) )
        {
            // We were restored while we were hidden, so now we need to show
            // the window in its normal state.
            //
            // As below, don't activate some kinds of windows.
            if ( HasFlag(wxFRAME_TOOL_WINDOW) || !IsEnabled() )
                nShowCmd = SW_SHOWNOACTIVATE;
            else
                nShowCmd = SW_RESTORE;
        }
        else // just show
        {
            // we shouldn't use SW_SHOW which also activates the window for
            // tool frames (as they shouldn't steal focus from the main window)
            // nor for the currently disabled windows as they would be enabled
            // as a side effect
            if ( HasFlag(wxFRAME_TOOL_WINDOW) || !IsEnabled() )
                nShowCmd = SW_SHOWNA;
            else
                nShowCmd = SW_SHOW;
        }
    }
    else // hide
    {
        nShowCmd = SW_HIDE;
    }

#if wxUSE_DEFERRED_SIZING
    // we only set pending size if we're maximized before being shown, now that
    // we're shown we don't need it any more (it is reset in size event handler
    // for child windows but we have to do it ourselves for this parent window)
    //
    // make sure to reset it before actually showing the window as this will
    // generate WM_SIZE events and we want to use the correct client size from
    // them, not the size returned by WM_NCCALCSIZE in DoGetClientSize() which
    // turns out to be wrong for maximized windows (see #11762)
    m_pendingSize = wxDefaultSize;
#endif // wxUSE_DEFERRED_SIZING

    DoShowWindow(nShowCmd);

#if defined(__WXWINCE__) && (_WIN32_WCE >= 400 && !defined(__POCKETPC__) && !defined(__SMARTPHONE__))
    // Addornments have to be added when the frame is the correct size
    wxFrame* frame = wxDynamicCast(this, wxFrame);
    if (frame && frame->GetMenuBar())
        frame->GetMenuBar()->AddAdornments(GetWindowStyleFlag());
#endif

    return true;
}

void wxTopLevelWindowMSW::Raise()
{
    ::SetForegroundWindow(GetHwnd());
}

// ----------------------------------------------------------------------------
// wxTopLevelWindowMSW maximize/minimize
// ----------------------------------------------------------------------------

void wxTopLevelWindowMSW::Maximize(bool maximize)
{
    if ( IsShown() )
    {
        // just maximize it directly
        DoShowWindow(maximize ? SW_MAXIMIZE : SW_RESTORE);
    }
    else // hidden
    {
        // we can't maximize the hidden frame because it shows it as well,
        // so just remember that we should do it later in this case
        m_maximizeOnShow = maximize;

#if wxUSE_DEFERRED_SIZING
        // after calling Maximize() the client code expects to get the frame
        // "real" size and doesn't want to know that, because of implementation
        // details, the frame isn't really maximized yet but will be only once
        // it's shown, so return our size as it will be then in this case
        if ( maximize )
        {
            // we must only change pending size here, and not call SetSize()
            // because otherwise Windows would think that this (full screen)
            // size is the natural size for the frame and so would use it when
            // the user clicks on "restore" title bar button instead of the
            // correct initial frame size
            //
            // NB: unfortunately we don't know which display we're on yet so we
            //     have to use the default one
            m_pendingSize = wxGetClientDisplayRect().GetSize();
        }
        //else: can't do anything in this case, we don't have the old size
#endif // wxUSE_DEFERRED_SIZING
    }
}

bool wxTopLevelWindowMSW::IsMaximized() const
{
    return IsAlwaysMaximized() ||
#if !defined(__SMARTPHONE__) && !defined(__POCKETPC__) && !defined(__WINCE_STANDARDSDK__)

           (::IsZoomed(GetHwnd()) != 0) ||
#endif
           m_maximizeOnShow;
}

void wxTopLevelWindowMSW::Iconize(bool iconize)
{
    if ( iconize == m_iconized )
    {
        // Do nothing, in particular don't restore non-iconized windows when
        // Iconize(false) is called as this would wrongly un-maximize them.
        return;
    }

    if ( IsShown() )
    {
        // change the window state immediately
        DoShowWindow(iconize ? SW_MINIMIZE : SW_RESTORE);
    }
    else // hidden
    {
        // iconizing the window shouldn't show it so just update the internal
        // state (otherwise it's done by DoShowWindow() itself)
        m_iconized = iconize;
    }
}

bool wxTopLevelWindowMSW::IsIconized() const
{
#ifdef __WXWINCE__
    return false;
#else
    if ( !IsShown() )
        return m_iconized;

    // don't use m_iconized, it may be briefly out of sync with the real state
    // as it's only modified when we receive a WM_SIZE and we could be called
    // from an event handler from one of the messages we receive before it,
    // such as WM_MOVE
    return ::IsIconic(GetHwnd()) != 0;
#endif
}

void wxTopLevelWindowMSW::Restore()
{
    DoShowWindow(SW_RESTORE);
}

void wxTopLevelWindowMSW::SetLayoutDirection(wxLayoutDirection dir)
{
    if ( dir == wxLayout_Default )
        dir = wxTheApp->GetLayoutDirection();

    if ( dir != wxLayout_Default )
        wxTopLevelWindowBase::SetLayoutDirection(dir);
}

// ----------------------------------------------------------------------------
// wxTopLevelWindowMSW geometry
// ----------------------------------------------------------------------------

#ifndef __WXWINCE__

void wxTopLevelWindowMSW::DoGetPosition(int *x, int *y) const
{
    if ( IsIconized() )
    {
        WINDOWPLACEMENT wp;
        wp.length = sizeof(WINDOWPLACEMENT);
        if ( ::GetWindowPlacement(GetHwnd(), &wp) )
        {
            RECT& rc = wp.rcNormalPosition;

            // the position returned by GetWindowPlacement() is in workspace
            // coordinates except for windows with WS_EX_TOOLWINDOW style
            if ( !HasFlag(wxFRAME_TOOL_WINDOW) )
            {
                // we must use the correct display for the translation as the
                // task bar might be shown on one display but not the other one
                int n = wxDisplay::GetFromWindow(this);
                wxDisplay dpy(n == wxNOT_FOUND ? 0 : n);
                const wxPoint ptOfs = dpy.GetClientArea().GetPosition() -
                                      dpy.GetGeometry().GetPosition();

                rc.left += ptOfs.x;
                rc.top += ptOfs.y;
            }

            if ( x )
                *x = rc.left;
            if ( y )
                *y = rc.top;

            return;
        }

        wxLogLastError(wxT("GetWindowPlacement"));
    }
    //else: normal case

    wxTopLevelWindowBase::DoGetPosition(x, y);
}

void wxTopLevelWindowMSW::DoGetSize(int *width, int *height) const
{
    if ( IsIconized() )
    {
        WINDOWPLACEMENT wp;
        wp.length = sizeof(WINDOWPLACEMENT);
        if ( ::GetWindowPlacement(GetHwnd(), &wp) )
        {
            const RECT& rc = wp.rcNormalPosition;

            if ( width )
                *width = rc.right - rc.left;
            if ( height )
                *height = rc.bottom - rc.top;

            return;
        }

        wxLogLastError(wxT("GetWindowPlacement"));
    }
    //else: normal case

    wxTopLevelWindowBase::DoGetSize(width, height);
}

#endif // __WXWINCE__

void
wxTopLevelWindowMSW::MSWGetCreateWindowCoords(const wxPoint& pos,
                                              const wxSize& size,
                                              int& x, int& y,
                                              int& w, int& h) const
{
    // let the system position the window if no explicit position was specified
    if ( pos.x == wxDefaultCoord )
    {
        // if x is set to CW_USEDEFAULT, y parameter is ignored anyhow so we
        // can just as well set it to CW_USEDEFAULT as well
        x =
        y = CW_USEDEFAULT;
    }
    else
    {
        // OTOH, if x is not set to CW_USEDEFAULT, y shouldn't be set to it
        // neither because it is not handled as a special value by Windows then
        // and so we have to choose some default value for it, even if a
        // completely arbitrary one
        static const int DEFAULT_Y = 200;

        x = pos.x;
        y = pos.y == wxDefaultCoord ? DEFAULT_Y : pos.y;
    }

    if ( size.x == wxDefaultCoord || size.y == wxDefaultCoord )
    {
        // We don't use CW_USEDEFAULT here for several reasons:
        //
        //  1. It results in huge frames on modern screens (1000*800 is not
        //     uncommon on my 1280*1024 screen) which is way too big for a half
        //     empty frame of most of wxWidgets samples for example)
        //
        //  2. It is buggy for frames with wxFRAME_TOOL_WINDOW style for which
        //     the default is for whatever reason 8*8 which breaks client <->
        //     window size calculations (it would be nice if it didn't, but it
        //     does and the simplest way to fix it seemed to change the broken
        //     default size anyhow)
        //
        //  3. There is just no advantage in doing it: with x and y it is
        //     possible that [future versions of] Windows position the new top
        //     level window in some smart way which we can't do, but we can
        //     guess a reasonably good size for a new window just as well
        //     ourselves
        //
        // The only exception is for the Windows CE platform where the system
        // does know better than we how should the windows be sized
#ifdef _WIN32_WCE
        w =
        h = CW_USEDEFAULT;
#else // !_WIN32_WCE
        wxSize sizeReal = size;
        sizeReal.SetDefaults(GetDefaultSize());

        w = sizeReal.x;
        h = sizeReal.y;
#endif // _WIN32_WCE/!_WIN32_WCE
    }
    else
    {
        w = size.x;
        h = size.y;
    }
}

// ----------------------------------------------------------------------------
// wxTopLevelWindowMSW fullscreen
// ----------------------------------------------------------------------------

bool wxTopLevelWindowMSW::ShowFullScreen(bool show, long style)
{
    if ( show == IsFullScreen() )
    {
        // nothing to do
        return true;
    }

    m_fsIsShowing = show;

    if ( show )
    {
        m_fsStyle = style;

        // zap the frame borders

        // save the 'normal' window style
        m_fsOldWindowStyle = GetWindowLong(GetHwnd(), GWL_STYLE);

        // save the old position, width & height, maximize state
        m_fsOldSize = GetRect();
        m_fsIsMaximized = IsMaximized();

        // decide which window style flags to turn off
        LONG newStyle = m_fsOldWindowStyle;
        LONG offFlags = 0;

        if (style & wxFULLSCREEN_NOBORDER)
        {
            offFlags |= WS_BORDER;
#ifndef __WXWINCE__
            offFlags |= WS_THICKFRAME;
#endif
        }
        if (style & wxFULLSCREEN_NOCAPTION)
            offFlags |= WS_CAPTION | WS_SYSMENU;

        newStyle &= ~offFlags;

        // Full screen windows should logically be popups as they don't have
        // decorations (and are definitely not children) and while not using
        // this style doesn't seem to make any difference for most windows, it
        // breaks wxGLCanvas in some cases, see #15434, so just always use it.

        // PCSX2: WS_POPUP causes fullscreen tearing when using OpenGL and the
        // GSPanel rendering area exactly covers the full screen. (always
        // affects stretch mode, but most people have 16:9 monitors, so in
        // general the widescreen 16:9 mode is also affected). Let's remove the
        // window style.
        if (style & WS_POPUP)
            newStyle |= WS_POPUP;

        // change our window style to be compatible with full-screen mode
        ::SetWindowLong(GetHwnd(), GWL_STYLE, newStyle);

        wxRect rect;
#if wxUSE_DISPLAY
        // resize to the size of the display containing us
        int dpy = wxDisplay::GetFromWindow(this);
        if ( dpy != wxNOT_FOUND )
        {
            rect = wxDisplay(dpy).GetGeometry();
        }
        else // fall back to the main desktop
#endif // wxUSE_DISPLAY
        {
            // resize to the size of the desktop
            wxCopyRECTToRect(wxGetWindowRect(::GetDesktopWindow()), rect);
#ifdef __WXWINCE__
            // FIXME: size of the bottom menu (toolbar)
            // should be taken in account
            rect.height += rect.y;
            rect.y       = 0;
#endif
        }

        SetSize(rect);

        // now flush the window style cache and actually go full-screen
        long flags = SWP_FRAMECHANGED;

        // showing the frame full screen should also show it if it's still
        // hidden
        if ( !IsShown() )
        {
            // don't call wxWindow version to avoid flicker from calling
            // ::ShowWindow() -- we're going to show the window at the correct
            // location directly below -- but do call the wxWindowBase version
            // to sync the internal m_isShown flag
            wxWindowBase::Show();

            flags |= SWP_SHOWWINDOW;
        }

        SetWindowPos(GetHwnd(), HWND_TOP,
                     rect.x, rect.y, rect.width, rect.height,
                     flags);

#if !defined(__HANDHELDPC__) && (defined(__WXWINCE__) && (_WIN32_WCE < 400))
        ::SHFullScreen(GetHwnd(), SHFS_HIDETASKBAR | SHFS_HIDESIPBUTTON);
#endif

        // finally send an event allowing the window to relayout itself &c
        wxSizeEvent event(rect.GetSize(), GetId());
        event.SetEventObject(this);
        HandleWindowEvent(event);
    }
    else // stop showing full screen
    {
#if !defined(__HANDHELDPC__) && (defined(__WXWINCE__) && (_WIN32_WCE < 400))
        ::SHFullScreen(GetHwnd(), SHFS_SHOWTASKBAR | SHFS_SHOWSIPBUTTON);
#endif
        Maximize(m_fsIsMaximized);
        SetWindowLong(GetHwnd(),GWL_STYLE, m_fsOldWindowStyle);
        SetWindowPos(GetHwnd(),HWND_TOP,m_fsOldSize.x, m_fsOldSize.y,
            m_fsOldSize.width, m_fsOldSize.height, SWP_FRAMECHANGED);
    }

    return true;
}

// ----------------------------------------------------------------------------
// wxTopLevelWindowMSW misc
// ----------------------------------------------------------------------------

void wxTopLevelWindowMSW::SetTitle( const wxString& title)
{
    SetLabel(title);
}

wxString wxTopLevelWindowMSW::GetTitle() const
{
    return GetLabel();
}

bool wxTopLevelWindowMSW::DoSelectAndSetIcon(const wxIconBundle& icons,
                                             int smX,
                                             int smY,
                                             int i)
{
    const wxSize size(::GetSystemMetrics(smX), ::GetSystemMetrics(smY));

    wxIcon icon = icons.GetIcon(size, wxIconBundle::FALLBACK_NEAREST_LARGER);

    if ( !icon.IsOk() )
        return false;

    ::SendMessage(GetHwnd(), WM_SETICON, i, (LPARAM)GetHiconOf(icon));
    return true;
}

void wxTopLevelWindowMSW::SetIcons(const wxIconBundle& icons)
{
    wxTopLevelWindowBase::SetIcons(icons);

    if ( icons.IsEmpty() )
    {
        // FIXME: SetIcons(wxNullIconBundle) should unset existing icons,
        //        but we currently don't do that
        wxASSERT_MSG( m_icons.IsEmpty(), "unsetting icons doesn't work" );
        return;
    }

    DoSelectAndSetIcon(icons, SM_CXSMICON, SM_CYSMICON, ICON_SMALL);
    DoSelectAndSetIcon(icons, SM_CXICON, SM_CYICON, ICON_BIG);
}

bool wxTopLevelWindowMSW::EnableCloseButton(bool enable)
{
#if !defined(__WXMICROWIN__)
    // get system (a.k.a. window) menu
    HMENU hmenu = GetSystemMenu(GetHwnd(), FALSE /* get it */);
    if ( !hmenu )
    {
        // no system menu at all -- ok if we want to remove the close button
        // anyhow, but bad if we want to show it
        return !enable;
    }

    // enabling/disabling the close item from it also automatically
    // disables/enables the close title bar button
    if ( ::EnableMenuItem(hmenu, SC_CLOSE,
                          MF_BYCOMMAND |
                          (enable ? MF_ENABLED : MF_GRAYED)) == -1 )
    {
        wxLogLastError(wxT("EnableMenuItem(SC_CLOSE)"));

        return false;
    }
#ifndef __WXWINCE__
    // update appearance immediately
    if ( !::DrawMenuBar(GetHwnd()) )
    {
        wxLogLastError(wxT("DrawMenuBar"));
    }
#endif
#endif // !__WXMICROWIN__

    return true;
}

void wxTopLevelWindowMSW::RequestUserAttention(int flags)
{
    // check if we can use FlashWindowEx(): unfortunately a simple test for
    // FLASHW_STOP doesn't work because MSVC6 headers do #define it but don't
    // provide FlashWindowEx() declaration, so try to detect whether we have
    // real headers for WINVER 0x0500 by checking for existence of a symbol not
    // declated in MSVC6 header
#if defined(FLASHW_STOP) && defined(VK_XBUTTON1) && wxUSE_DYNLIB_CLASS
    // available in the headers, check if it is supported by the system
    typedef BOOL (WINAPI *FlashWindowEx_t)(FLASHWINFO *pfwi);
    static FlashWindowEx_t s_pfnFlashWindowEx = NULL;
    if ( !s_pfnFlashWindowEx )
    {
        wxDynamicLibrary dllUser32(wxT("user32.dll"));
        s_pfnFlashWindowEx = (FlashWindowEx_t)
                                dllUser32.GetSymbol(wxT("FlashWindowEx"));

        // we can safely unload user32.dll here, it's going to remain loaded as
        // long as the program is running anyhow
    }

    if ( s_pfnFlashWindowEx )
    {
        WinStruct<FLASHWINFO> fwi;
        fwi.hwnd = GetHwnd();
        fwi.dwFlags = FLASHW_ALL;
        if ( flags & wxUSER_ATTENTION_INFO )
        {
            // just flash a few times
            fwi.uCount = 3;
        }
        else // wxUSER_ATTENTION_ERROR
        {
            // flash until the user notices it
            fwi.dwFlags |= FLASHW_TIMERNOFG;
        }

        s_pfnFlashWindowEx(&fwi);
    }
    else // FlashWindowEx() not available
#endif // FlashWindowEx() defined
    {
        wxUnusedVar(flags);
#ifndef __WXWINCE__
        ::FlashWindow(GetHwnd(), TRUE);
#endif // __WXWINCE__
    }
}

wxMenu *wxTopLevelWindowMSW::MSWGetSystemMenu() const
{
#ifndef __WXUNIVERSAL__
    if ( !m_menuSystem )
    {
        HMENU hmenu = ::GetSystemMenu(GetHwnd(), FALSE);
        if ( !hmenu )
        {
            wxLogLastError(wxT("GetSystemMenu()"));
            return NULL;
        }

        wxTopLevelWindowMSW * const
            self = const_cast<wxTopLevelWindowMSW *>(this);

        self->m_menuSystem = wxMenu::MSWNewFromHMENU(hmenu);

        // We need to somehow associate this menu with this window to ensure
        // that we get events from it. A natural idea would be to pretend that
        // it's attached to our menu bar but this wouldn't work if we don't
        // have any menu bar which is a common case for applications using
        // custom items in the system menu (they mostly do it exactly because
        // they don't have any other menus).
        //
        // So reuse the invoking window pointer instead, this is not exactly
        // correct but doesn't seem to have any serious drawbacks.
        m_menuSystem->SetInvokingWindow(self);
    }
#endif // #ifndef __WXUNIVERSAL__

    return m_menuSystem;
}

// ----------------------------------------------------------------------------
// Transparency support
// ---------------------------------------------------------------------------

bool wxTopLevelWindowMSW::SetTransparent(wxByte alpha)
{
#if wxUSE_DYNLIB_CLASS
    typedef DWORD (WINAPI *PSETLAYEREDWINDOWATTR)(HWND, DWORD, BYTE, DWORD);
    static PSETLAYEREDWINDOWATTR
        pSetLayeredWindowAttributes = (PSETLAYEREDWINDOWATTR)-1;

    if ( pSetLayeredWindowAttributes == (PSETLAYEREDWINDOWATTR)-1 )
    {
        wxDynamicLibrary dllUser32(wxT("user32.dll"));

        // use RawGetSymbol() and not GetSymbol() to avoid error messages under
        // Windows 95: there is nothing the user can do about this anyhow
        pSetLayeredWindowAttributes = (PSETLAYEREDWINDOWATTR)
            dllUser32.RawGetSymbol(wxT("SetLayeredWindowAttributes"));

        // it's ok to destroy dllUser32 here, we link statically to user32.dll
        // anyhow so it won't be unloaded
    }

    if ( !pSetLayeredWindowAttributes )
        return false;
#endif // wxUSE_DYNLIB_CLASS

    LONG exstyle = GetWindowLong(GetHwnd(), GWL_EXSTYLE);

    // if setting alpha to fully opaque then turn off the layered style
    if (alpha == 255)
    {
        SetWindowLong(GetHwnd(), GWL_EXSTYLE, exstyle & ~WS_EX_LAYERED);
        Refresh();
        return true;
    }

#if wxUSE_DYNLIB_CLASS
    // Otherwise, set the layered style if needed and set the alpha value
    if ((exstyle & WS_EX_LAYERED) == 0 )
        SetWindowLong(GetHwnd(), GWL_EXSTYLE, exstyle | WS_EX_LAYERED);

    if ( pSetLayeredWindowAttributes(GetHwnd(), 0, (BYTE)alpha, LWA_ALPHA) )
        return true;
#endif // wxUSE_DYNLIB_CLASS

    return false;
}

bool wxTopLevelWindowMSW::CanSetTransparent()
{
    // The API is available on win2k and above

    static int os_type = -1;
    static int ver_major = -1;

    if (os_type == -1)
        os_type = ::wxGetOsVersion(&ver_major);

    return (os_type == wxOS_WINDOWS_NT && ver_major >= 5);
}

void wxTopLevelWindowMSW::DoFreeze()
{
    // do nothing: freezing toplevel window causes paint and mouse events
    // to go through it any TLWs under it, so the best we can do is to freeze
    // all children -- and wxWindowBase::Freeze() does that
}

void wxTopLevelWindowMSW::DoThaw()
{
    // intentionally empty -- see DoFreeze()
}


// ----------------------------------------------------------------------------
// wxTopLevelWindow event handling
// ----------------------------------------------------------------------------

void wxTopLevelWindowMSW::DoSaveLastFocus()
{
    if ( m_iconized )
        return;

    // remember the last focused child if it is our child
    m_winLastFocused = FindFocus();

    if ( m_winLastFocused )
    {
        // and don't remember it if it's a child from some other frame
        if ( wxGetTopLevelParent(m_winLastFocused) != this )
        {
            m_winLastFocused = NULL;
        }
    }
}

void wxTopLevelWindowMSW::DoRestoreLastFocus()
{
    wxWindow *parent = m_winLastFocused ? m_winLastFocused->GetParent()
                                        : NULL;
    if ( !parent )
    {
        parent = this;
    }

    wxSetFocusToChild(parent, &m_winLastFocused);
}

void wxTopLevelWindowMSW::OnActivate(wxActivateEvent& event)
{
    if ( event.GetActive() )
    {
        // We get WM_ACTIVATE before being restored from iconized state, so we
        // can be still iconized here. In this case, avoid restoring the focus
        // as it doesn't work anyhow and we will do when we're really restored.
        if ( m_iconized )
        {
            event.Skip();
            return;
        }

        // restore focus to the child which was last focused unless we already
        // have it
        wxLogTrace(wxT("focus"), wxT("wxTLW %p activated."), m_hWnd);

        wxWindow *winFocus = FindFocus();
        if ( !winFocus || wxGetTopLevelParent(winFocus) != this )
            DoRestoreLastFocus();
    }
    else // deactivating
    {
        DoSaveLastFocus();

        wxLogTrace(wxT("focus"),
                   wxT("wxTLW %p deactivated, last focused: %p."),
                   m_hWnd,
                   m_winLastFocused ? GetHwndOf(m_winLastFocused) : NULL);

        event.Skip();
    }
}

#if wxUSE_MENUS

bool
wxTopLevelWindowMSW::HandleMenuSelect(WXWORD nItem, WXWORD flags, WXHMENU hMenu)
{
    // Ignore the special messages generated when the menu is closed (this is
    // the only case when the flags are set to -1), in particular don't clear
    // the help string in the status bar when this happens as it had just been
    // restored by the base class code.
    if ( !hMenu && flags == 0xffff )
        return false;

    // Unfortunately we also need to ignore another message which is sent after
    // closing the currently active submenu of the menu bar by pressing Escape:
    // in this case we get WM_UNINITMENUPOPUP, from which we generate
    // wxEVT_MENU_CLOSE, and _then_ we get WM_MENUSELECT for the top level menu
    // from which we overwrite the help string just restored by OnMenuClose()
    // handler in wxFrameBase. To prevent this from happening we discard these
    // messages but only in the case it's really the top level menu as we still
    // need to clear the help string when a submenu is selected in a menu.
    if ( flags == (MF_POPUP | MF_HILITE) && !m_menuDepth )
        return false;

    // sign extend to int from unsigned short we get from Windows
    int item = (signed short)nItem;

    // WM_MENUSELECT is generated for both normal items and menus, including
    // the top level menus of the menu bar, which can't be represented using
    // any valid identifier in wxMenuEvent so use an otherwise unused value for
    // them
    if ( flags & (MF_POPUP | MF_SEPARATOR) )
        item = wxID_NONE;

    wxMenuEvent event(wxEVT_MENU_HIGHLIGHT, item);
    event.SetEventObject(this);

    if ( HandleWindowEvent(event) )
        return true;

    // by default, i.e. if the event wasn't handled above, clear the status bar
    // text when an item which can't have any associated help string in wx API
    // is selected
    if ( item == wxID_NONE )
        DoGiveHelp(wxEmptyString, true);

    return false;
}

bool
wxTopLevelWindowMSW::DoSendMenuOpenCloseEvent(wxEventType evtType, wxMenu* menu, bool popup)
{
    // Update the menu depth when dealing with the top level menus.
    if ( !popup )
    {
        if ( evtType == wxEVT_MENU_OPEN )
        {
            m_menuDepth++;
        }
        else if ( evtType == wxEVT_MENU_CLOSE )
        {
            wxASSERT_MSG( m_menuDepth > 0, wxS("No open menus?") );

            m_menuDepth--;
        }
        else
        {
            wxFAIL_MSG( wxS("Unexpected menu event type") );
        }
    }

    wxMenuEvent event(evtType, popup ? wxID_ANY : 0, menu);
    event.SetEventObject(menu);

    return HandleWindowEvent(event);
}

bool wxTopLevelWindowMSW::HandleExitMenuLoop(WXWORD isPopup)
{
    return DoSendMenuOpenCloseEvent(wxEVT_MENU_CLOSE,
                                    isPopup ? wxCurrentPopupMenu : NULL,
                                    isPopup != 0);
}

bool wxTopLevelWindowMSW::HandleMenuPopup(wxEventType evtType, WXHMENU hMenu)
{
    bool isPopup = false;
    wxMenu* menu = NULL;
    if ( wxCurrentPopupMenu && wxCurrentPopupMenu->GetHMenu() == hMenu )
    {
        menu = wxCurrentPopupMenu;
        isPopup = true;
    }
    else
    {
        menu = MSWFindMenuFromHMENU(hMenu);
    }


    return DoSendMenuOpenCloseEvent(evtType, menu, isPopup);
}

wxMenu* wxTopLevelWindowMSW::MSWFindMenuFromHMENU(WXHMENU WXUNUSED(hMenu))
{
    // We don't have any menus at this level.
    return NULL;
}

#endif // wxUSE_MENUS



// the DialogProc for all wxWidgets dialogs
LONG APIENTRY _EXPORT
wxDlgProc(HWND hDlg,
          UINT message,
          WPARAM WXUNUSED(wParam),
          LPARAM WXUNUSED(lParam))
{
    switch ( message )
    {
        case WM_INITDIALOG:
        {
            // under CE, add a "Ok" button in the dialog title bar and make it full
            // screen
            //
            // TODO: find the window for this HWND, and take into account
            // wxMAXIMIZE and wxCLOSE_BOX. For now, assume both are present.
            //
            // Standard SDK doesn't have aygshell.dll: see
            // include/wx/msw/wince/libraries.h
#if defined(__WXWINCE__) && !defined(__WINCE_STANDARDSDK__) && !defined(__HANDHELDPC__)
            SHINITDLGINFO shidi;
            shidi.dwMask = SHIDIM_FLAGS;
            shidi.dwFlags = SHIDIF_SIZEDLG // take account of the SIP or menubar
#ifndef __SMARTPHONE__
                            | SHIDIF_DONEBUTTON
#endif
                        ;
            shidi.hDlg = hDlg;
            SHInitDialog( &shidi );
#else // no SHInitDialog()
            wxUnusedVar(hDlg);
#endif
            // for WM_INITDIALOG, returning TRUE tells system to set focus to
            // the first control in the dialog box, but as we set the focus
            // ourselves, we return FALSE for it as well
            return FALSE;
        }
    }

    // for almost all messages, returning FALSE means that we didn't process
    // the message
    return FALSE;
}

// ============================================================================
// wxTLWHiddenParentModule implementation
// ============================================================================

HWND wxTLWHiddenParentModule::ms_hwnd = NULL;

const wxChar *wxTLWHiddenParentModule::ms_className = NULL;

bool wxTLWHiddenParentModule::OnInit()
{
    ms_hwnd = NULL;
    ms_className = NULL;

    return true;
}

void wxTLWHiddenParentModule::OnExit()
{
    if ( ms_hwnd )
    {
        if ( !::DestroyWindow(ms_hwnd) )
        {
            wxLogLastError(wxT("DestroyWindow(hidden TLW parent)"));
        }

        ms_hwnd = NULL;
    }

    if ( ms_className )
    {
        if ( !::UnregisterClass(ms_className, wxGetInstance()) )
        {
            wxLogLastError(wxT("UnregisterClass(\"wxTLWHiddenParent\")"));
        }

        ms_className = NULL;
    }
}

/* static */
HWND wxTLWHiddenParentModule::GetHWND()
{
    if ( !ms_hwnd )
    {
        if ( !ms_className )
        {
            static const wxChar *HIDDEN_PARENT_CLASS = wxT("wxTLWHiddenParent");

            WNDCLASS wndclass;
            wxZeroMemory(wndclass);

            wndclass.lpfnWndProc   = DefWindowProc;
            wndclass.hInstance     = wxGetInstance();
            wndclass.lpszClassName = HIDDEN_PARENT_CLASS;

            if ( !::RegisterClass(&wndclass) )
            {
                wxLogLastError(wxT("RegisterClass(\"wxTLWHiddenParent\")"));
            }
            else
            {
                ms_className = HIDDEN_PARENT_CLASS;
            }
        }

        ms_hwnd = ::CreateWindow(ms_className, wxEmptyString, 0, 0, 0, 0, 0, NULL,
                                 (HMENU)NULL, wxGetInstance(), NULL);
        if ( !ms_hwnd )
        {
            wxLogLastError(wxT("CreateWindow(hidden TLW parent)"));
        }
    }

    return ms_hwnd;
}
