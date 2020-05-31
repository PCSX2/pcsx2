/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/dialog.cpp
// Purpose:     wxDialog class
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

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

#include "wx/dialog.h"
#include "wx/modalhook.h"

#ifndef WX_PRECOMP
    #include "wx/msw/wrapcdlg.h"
    #include "wx/utils.h"
    #include "wx/frame.h"
    #include "wx/app.h"
    #include "wx/button.h"
    #include "wx/settings.h"
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/toolbar.h"
#endif

#include "wx/msw/private.h"
#include "wx/evtloop.h"
#include "wx/scopedptr.h"

#if defined(__SMARTPHONE__) && defined(__WXWINCE__)
    #include "wx/msw/wince/resources.h"
#endif // __SMARTPHONE__ && __WXWINCE__

// ----------------------------------------------------------------------------
// wxWin macros
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// wxDialogModalData
// ----------------------------------------------------------------------------

// this is simply a container for any data we need to implement modality which
// allows us to avoid changing wxDialog each time the implementation changes
class wxDialogModalData
{
public:
    wxDialogModalData(wxDialog *dialog) : m_evtLoop(dialog) { }

    void RunLoop()
    {
        m_evtLoop.Run();
    }

    void ExitLoop()
    {
        m_evtLoop.Exit();
    }

private:
    wxModalEventLoop m_evtLoop;
};

wxDEFINE_TIED_SCOPED_PTR_TYPE(wxDialogModalData)

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxDialog construction
// ----------------------------------------------------------------------------

void wxDialog::Init()
{
    m_isShown = false;
    m_modalData = NULL;
#if wxUSE_TOOLBAR && defined(__POCKETPC__)
    m_dialogToolBar = NULL;
#endif
#if wxUSE_DIALOG_SIZEGRIP
    m_hGripper = 0;
#endif // wxUSE_DIALOG_SIZEGRIP
}

bool wxDialog::Create(wxWindow *parent,
                      wxWindowID id,
                      const wxString& title,
                      const wxPoint& pos,
                      const wxSize& size,
                      long style,
                      const wxString& name)
{
    SetExtraStyle(GetExtraStyle() | wxTOPLEVEL_EX_DIALOG);

    // All dialogs should really have this style
    style |= wxTAB_TRAVERSAL;

    if ( !wxTopLevelWindow::Create(parent, id, title, pos, size, style, name) )
        return false;

    if ( !m_hasFont )
        SetFont(wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT));

#if defined(__SMARTPHONE__) && defined(__WXWINCE__)
    SetLeftMenu(wxID_OK, _("OK"));
#endif
#if wxUSE_TOOLBAR && defined(__POCKETPC__)
    CreateToolBar();
#endif

#if wxUSE_DIALOG_SIZEGRIP
    if ( HasFlag(wxRESIZE_BORDER) )
    {
        CreateGripper();

        Connect(wxEVT_CREATE,
                wxWindowCreateEventHandler(wxDialog::OnWindowCreate));
    }
#endif // wxUSE_DIALOG_SIZEGRIP

    return true;
}

wxDialog::~wxDialog()
{
    // this will also reenable all the other windows for a modal dialog
    Show(false);

#if wxUSE_DIALOG_SIZEGRIP
    DestroyGripper();
#endif // wxUSE_DIALOG_SIZEGRIP
}

// ----------------------------------------------------------------------------
// showing the dialogs
// ----------------------------------------------------------------------------

bool wxDialog::Show(bool show)
{
    if ( show == IsShown() )
        return false;

    if ( !show && m_modalData )
    {
        // we need to do this before calling wxDialogBase version because if we
        // had disabled other app windows, they must be reenabled right now as
        // if they stay disabled Windows will activate another window (one
        // which is enabled, anyhow) when we're hidden in the base class Show()
        // and we will lose activation
        m_modalData->ExitLoop();
    }

    if ( show )
    {
        if (CanDoLayoutAdaptation())
            DoLayoutAdaptation();

        // this usually will result in TransferDataToWindow() being called
        // which will change the controls values so do it before showing as
        // otherwise we could have some flicker
        InitDialog();
    }

    wxDialogBase::Show(show);

    if ( show )
    {
        // dialogs don't get WM_SIZE message from ::ShowWindow() for some
        // reason so generate it ourselves for consistency with frames and
        // dialogs in other ports
        //
        // NB: normally we should call it just the first time but doing it
        //     every time is simpler than keeping a flag
        const wxSize size = GetClientSize();
        ::SendMessage(GetHwnd(), WM_SIZE,
                      SIZE_RESTORED, MAKELPARAM(size.x, size.y));
    }

    return true;
}

// show dialog modally
int wxDialog::ShowModal()
{
    WX_HOOK_MODAL_DIALOG();

    wxASSERT_MSG( !IsModal(), wxT("ShowModal() can't be called twice") );

    Show();

    // EndModal may have been called from InitDialog handler (called from
    // inside Show()) and hidden the dialog back again
    if ( IsShown() )
    {
        // enter and run the modal loop
        wxDialogModalDataTiedPtr modalData(&m_modalData,
                                           new wxDialogModalData(this));
        modalData->RunLoop();
    }

    return GetReturnCode();
}

void wxDialog::EndModal(int retCode)
{
    wxASSERT_MSG( IsModal(), wxT("EndModal() called for non modal dialog") );

    SetReturnCode(retCode);

    Hide();
}

// ----------------------------------------------------------------------------
// wxDialog gripper handling
// ----------------------------------------------------------------------------

#if wxUSE_DIALOG_SIZEGRIP

void wxDialog::SetWindowStyleFlag(long style)
{
    wxDialogBase::SetWindowStyleFlag(style);

    if ( HasFlag(wxRESIZE_BORDER) )
        CreateGripper();
    else
        DestroyGripper();
}

void wxDialog::CreateGripper()
{
    if ( !m_hGripper )
    {
        // just create it here, it will be positioned and shown later
        m_hGripper = (WXHWND)::CreateWindow
                               (
                                    wxT("SCROLLBAR"),
                                    wxT(""),
                                    WS_CHILD |
                                    WS_CLIPSIBLINGS |
                                    SBS_SIZEGRIP |
                                    SBS_SIZEBOX |
                                    SBS_SIZEBOXBOTTOMRIGHTALIGN,
                                    0, 0, 0, 0,
                                    GetHwnd(),
                                    0,
                                    wxGetInstance(),
                                    NULL
                               );
    }
}

void wxDialog::DestroyGripper()
{
    if ( m_hGripper )
    {
        // we used to have trouble with gripper appearing on top (and hence
        // overdrawing) the other, real, dialog children -- check that this
        // isn't the case automatically (but notice that this could be false if
        // we're not shown at all as in this case ResizeGripper() might not
        // have been called yet)
        wxASSERT_MSG( !IsShown() ||
                      ::GetWindow((HWND)m_hGripper, GW_HWNDNEXT) == 0,
            wxT("Bug in wxWidgets: gripper should be at the bottom of Z-order") );
        ::DestroyWindow((HWND) m_hGripper);
        m_hGripper = 0;
    }
}

void wxDialog::ShowGripper(bool show)
{
    wxASSERT_MSG( m_hGripper, wxT("shouldn't be called if we have no gripper") );

    if ( show )
        ResizeGripper();

    ::ShowWindow((HWND)m_hGripper, show ? SW_SHOW : SW_HIDE);
}

void wxDialog::ResizeGripper()
{
    wxASSERT_MSG( m_hGripper, wxT("shouldn't be called if we have no gripper") );

    HWND hwndGripper = (HWND)m_hGripper;

    const wxRect rectGripper = wxRectFromRECT(wxGetWindowRect(hwndGripper));
    const wxSize size = GetClientSize() - rectGripper.GetSize();

    ::SetWindowPos(hwndGripper, HWND_BOTTOM,
                   size.x, size.y,
                   rectGripper.width, rectGripper.height,
                   SWP_NOACTIVATE);
}

void wxDialog::OnWindowCreate(wxWindowCreateEvent& event)
{
    if ( m_hGripper && IsShown() &&
            event.GetWindow() && event.GetWindow()->GetParent() == this )
    {
        // Put gripper below the newly created child window
        ::SetWindowPos((HWND)m_hGripper, HWND_BOTTOM, 0, 0, 0, 0,
                       SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
    }

    event.Skip();
}

#endif // wxUSE_DIALOG_SIZEGRIP

// ----------------------------------------------------------------------------
// wxWin event handlers
// ----------------------------------------------------------------------------

#ifdef __POCKETPC__
// Responds to the OK button in a PocketPC titlebar. This
// can be overridden, or you can change the id used for
// sending the event, by calling SetAffirmativeId.
bool wxDialog::DoOK()
{
    const int idOk = GetAffirmativeId();
    if ( EmulateButtonClickIfPresent(idOk) )
        return true;

    wxCommandEvent event(wxEVT_BUTTON, GetAffirmativeId());
    event.SetEventObject(this);

    return HandleWindowEvent(event);
}
#endif // __POCKETPC__

#if wxUSE_TOOLBAR && defined(__POCKETPC__)
// create main toolbar by calling OnCreateToolBar()
wxToolBar* wxDialog::CreateToolBar(long style, wxWindowID winid, const wxString& name)
{
    m_dialogToolBar = OnCreateToolBar(style, winid, name);

    return m_dialogToolBar;
}

// return a new toolbar
wxToolBar *wxDialog::OnCreateToolBar(long style,
                                       wxWindowID winid,
                                       const wxString& name)
{
    return new wxToolMenuBar(this, winid,
                         wxDefaultPosition, wxDefaultSize,
                         style, name);
}
#endif

// ---------------------------------------------------------------------------
// dialog Windows messages processing
// ---------------------------------------------------------------------------

WXLRESULT wxDialog::MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam)
{
    WXLRESULT rc = 0;
    bool processed = false;

    switch ( message )
    {
#ifdef __WXWINCE__
        // react to pressing the OK button in the title
        case WM_COMMAND:
        {
            switch ( LOWORD(wParam) )
            {
#ifdef __POCKETPC__
                case IDOK:
                    processed = DoOK();
                    if (!processed)
                        processed = !Close();
#endif
#ifdef __SMARTPHONE__
                case IDM_LEFT:
                case IDM_RIGHT:
                    processed = HandleCommand( LOWORD(wParam) , 0 , NULL );
                    break;
#endif // __SMARTPHONE__
            }
            break;
        }
#endif
        case WM_CLOSE:
            // if we can't close, tell the system that we processed the
            // message - otherwise it would close us
            processed = !Close();
            break;

        case WM_SIZE:
#if wxUSE_DIALOG_SIZEGRIP
            if ( m_hGripper )
            {
                switch ( wParam )
                {
                    case SIZE_MAXIMIZED:
                        ShowGripper(false);
                        break;

                    case SIZE_RESTORED:
                        ShowGripper(true);
                }
            }
#endif // wxUSE_DIALOG_SIZEGRIP

            // the Windows dialogs unfortunately are not meant to be resizable
            // at all and their standard class doesn't include CS_[VH]REDRAW
            // styles which means that the window is not refreshed properly
            // after the resize and no amount of WS_CLIPCHILDREN/SIBLINGS can
            // help with it - so we have to refresh it manually which certainly
            // creates flicker but at least doesn't show garbage on the screen
            rc = wxWindow::MSWWindowProc(message, wParam, lParam);
            processed = true;
            if ( HasFlag(wxFULL_REPAINT_ON_RESIZE) )
            {
                ::InvalidateRect(GetHwnd(), NULL, false /* erase bg */);
            }
            break;
    }

    if ( !processed )
        rc = wxDialogBase::MSWWindowProc(message, wParam, lParam);

    return rc;
}
