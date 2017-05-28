/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/fdrepdlg.cpp
// Purpose:     wxFindReplaceDialog class
// Author:      Markus Greither and Vadim Zeitlin
// Modified by:
// Created:     23/03/2001
// Copyright:   (c) Markus Greither
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

#if wxUSE_FINDREPLDLG

#ifndef WX_PRECOMP
    #include "wx/msw/wrapcdlg.h"
    #include "wx/intl.h"
    #include "wx/log.h"
#endif

#include "wx/fdrepdlg.h"

// ----------------------------------------------------------------------------
// functions prototypes
// ----------------------------------------------------------------------------

UINT_PTR CALLBACK wxFindReplaceDialogHookProc(HWND hwnd,
                                              UINT uiMsg,
                                              WPARAM wParam,
                                              LPARAM lParam);

// ----------------------------------------------------------------------------
// wxWin macros
// ----------------------------------------------------------------------------

wxIMPLEMENT_DYNAMIC_CLASS(wxFindReplaceDialog, wxDialog);

// ----------------------------------------------------------------------------
// wxFindReplaceDialogImpl: the internals of wxFindReplaceDialog
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxFindReplaceDialogImpl
{
public:
    wxFindReplaceDialogImpl(wxFindReplaceDialog *dialog, int flagsWX);
    ~wxFindReplaceDialogImpl();

    void InitFindWhat(const wxString& str);
    void InitReplaceWith(const wxString& str);

    // only for passing to ::FindText or ::ReplaceText
    FINDREPLACE *GetPtrFindReplace() { return &m_findReplace; }

    // set/query the "closed by user" flag
    void SetClosedByUser() { m_wasClosedByUser = true; }
    bool WasClosedByUser() const { return m_wasClosedByUser; }

private:
    // called from window procedure for ms_msgFindDialog
    static bool FindMessageHandler(wxWindow *win,
                                   WXUINT nMsg,
                                   WPARAM wParam,
                                   LPARAM lParam);

    // copy string str contents to ppStr and fill pLen with its length
    void InitString(const wxString& str, LPTSTR *ppStr, WORD *pLen);


    // the find replace data used by the dialog
    FINDREPLACE m_findReplace;

    // true if the user closed us, false otherwise
    bool m_wasClosedByUser;

    // registered Message for Dialog
    static UINT ms_msgFindDialog;

    wxDECLARE_NO_COPY_CLASS(wxFindReplaceDialogImpl);
};

UINT wxFindReplaceDialogImpl::ms_msgFindDialog = 0;

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxFindReplaceDialogImpl
// ----------------------------------------------------------------------------

wxFindReplaceDialogImpl::wxFindReplaceDialogImpl(wxFindReplaceDialog *dialog,
                                                 int flagsWX)
{
    // get the identifier for the find dialog message if we don't have it yet
    if ( !ms_msgFindDialog )
    {
        ms_msgFindDialog = ::RegisterWindowMessage(FINDMSGSTRING);

        if ( !ms_msgFindDialog )
        {
            wxLogLastError(wxT("RegisterWindowMessage(FINDMSGSTRING)"));
        }

        wxWindow::MSWRegisterMessageHandler
                  (
                    ms_msgFindDialog,
                    &wxFindReplaceDialogImpl::FindMessageHandler
                  );
    }

    m_wasClosedByUser = false;

    wxZeroMemory(m_findReplace);

    // translate the flags: first the dialog creation flags

    // always set this to be able to set the title
    int flags = FR_ENABLEHOOK;

    int flagsDialog = dialog->GetWindowStyle();
    if ( flagsDialog & wxFR_NOMATCHCASE)
        flags |= FR_NOMATCHCASE;
    if ( flagsDialog & wxFR_NOWHOLEWORD)
        flags |= FR_NOWHOLEWORD;
    if ( flagsDialog & wxFR_NOUPDOWN)
        flags |= FR_NOUPDOWN;

    // and now the flags governing the initial values of the dialogs controls
    if ( flagsWX & wxFR_DOWN)
        flags |= FR_DOWN;
    if ( flagsWX & wxFR_MATCHCASE)
        flags |= FR_MATCHCASE;
    if ( flagsWX & wxFR_WHOLEWORD )
        flags |= FR_WHOLEWORD;

    m_findReplace.lStructSize = sizeof(FINDREPLACE);
    m_findReplace.hwndOwner = GetHwndOf(dialog->GetParent());
    m_findReplace.Flags = flags;

    m_findReplace.lCustData = (LPARAM)dialog;
    m_findReplace.lpfnHook = wxFindReplaceDialogHookProc;
}

void wxFindReplaceDialogImpl::InitString(const wxString& str,
                                         LPTSTR *ppStr, WORD *pLen)
{
    size_t len = str.length() + 1;
    if ( len < 80 )
    {
        // MSDN docs say that the buffer must be at least 80 chars
        len = 80;
    }

    *ppStr = new wxChar[len];
    wxStrcpy(*ppStr, str);
    *pLen = (WORD)len;
}

void wxFindReplaceDialogImpl::InitFindWhat(const wxString& str)
{
    InitString(str, &m_findReplace.lpstrFindWhat, &m_findReplace.wFindWhatLen);
}

void wxFindReplaceDialogImpl::InitReplaceWith(const wxString& str)
{
    InitString(str,
               &m_findReplace.lpstrReplaceWith,
               &m_findReplace.wReplaceWithLen);
}

wxFindReplaceDialogImpl::~wxFindReplaceDialogImpl()
{
    delete [] m_findReplace.lpstrFindWhat;
    delete [] m_findReplace.lpstrReplaceWith;
}

// ----------------------------------------------------------------------------
// handler for FINDMSGSTRING message
// ----------------------------------------------------------------------------

bool
wxFindReplaceDialogImpl::FindMessageHandler(wxWindow * WXUNUSED(win),
                                            WXUINT WXUNUSED_UNLESS_DEBUG(nMsg),
                                            WPARAM WXUNUSED(wParam),
                                            LPARAM lParam)
{
    wxASSERT_MSG( nMsg == ms_msgFindDialog, wxT("unexpected message received") );

    FINDREPLACE *pFR = (FINDREPLACE *)lParam;

    wxFindReplaceDialog *dialog = (wxFindReplaceDialog *)pFR->lCustData;

    // map flags from Windows
    wxEventType evtType;

    bool replace = false;
    if ( pFR->Flags & FR_DIALOGTERM )
    {
        // we have to notify the dialog that it's being closed by user and
        // not deleted programmatically as it behaves differently in these
        // 2 cases
        dialog->GetImpl()->SetClosedByUser();

        evtType = wxEVT_FIND_CLOSE;
    }
    else if ( pFR->Flags & FR_FINDNEXT )
    {
        evtType = wxEVT_FIND_NEXT;
    }
    else if ( pFR->Flags & FR_REPLACE )
    {
        evtType = wxEVT_FIND_REPLACE;

        replace = true;
    }
    else if ( pFR->Flags & FR_REPLACEALL )
    {
        evtType = wxEVT_FIND_REPLACE_ALL;

        replace = true;
    }
    else
    {
        wxFAIL_MSG( wxT("unknown find dialog event") );

        return 0;
    }

    wxUint32 flags = 0;
    if ( pFR->Flags & FR_DOWN )
        flags |= wxFR_DOWN;
    if ( pFR->Flags & FR_WHOLEWORD )
        flags |= wxFR_WHOLEWORD;
    if ( pFR->Flags & FR_MATCHCASE )
        flags |= wxFR_MATCHCASE;

    wxFindDialogEvent event(evtType, dialog->GetId());
    event.SetEventObject(dialog);
    event.SetFlags(flags);
    event.SetFindString(pFR->lpstrFindWhat);
    if ( replace )
    {
        event.SetReplaceString(pFR->lpstrReplaceWith);
    }

    dialog->Send(event);

    return true;
}

// ----------------------------------------------------------------------------
// Find/replace dialog hook proc
// ----------------------------------------------------------------------------

UINT_PTR CALLBACK
wxFindReplaceDialogHookProc(HWND hwnd,
                            UINT uiMsg,
                            WPARAM WXUNUSED(wParam),
                            LPARAM lParam)
{
    if ( uiMsg == WM_INITDIALOG )
    {
        FINDREPLACE *pFR = (FINDREPLACE *)lParam;
        wxFindReplaceDialog *dialog = (wxFindReplaceDialog *)pFR->lCustData;

        ::SetWindowText(hwnd, dialog->GetTitle().t_str());

        // don't return FALSE from here or the dialog won't be shown
        return TRUE;
    }

    return 0;
}

// ============================================================================
// wxFindReplaceDialog implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxFindReplaceDialog ctors/dtor
// ----------------------------------------------------------------------------

void wxFindReplaceDialog::Init()
{
    m_impl = NULL;
    m_FindReplaceData = NULL;

    // as we're created in the hidden state, bring the internal flag in sync
    m_isShown = false;
}

wxFindReplaceDialog::wxFindReplaceDialog(wxWindow *parent,
                                         wxFindReplaceData *data,
                                         const wxString &title,
                                         int flags)
                   : wxFindReplaceDialogBase(parent, data, title, flags)
{
    Init();

    (void)Create(parent, data, title, flags);
}

wxFindReplaceDialog::~wxFindReplaceDialog()
{
    if ( m_impl )
    {
        // the dialog might have been already deleted if the user closed it
        // manually but in this case we should have got a notification about it
        // and the flag must have been set
        if ( !m_impl->WasClosedByUser() )
        {
            // if it wasn't, delete the dialog ourselves
            if ( !::DestroyWindow(GetHwnd()) )
            {
                wxLogLastError(wxT("DestroyWindow(find dialog)"));
            }
        }

        // unsubclass the parent
        delete m_impl;
    }

    // prevent the base class dtor from trying to hide us!
    m_isShown = false;

    // and from destroying our window [again]
    m_hWnd = (WXHWND)NULL;
}

bool wxFindReplaceDialog::Create(wxWindow *parent,
                                 wxFindReplaceData *data,
                                 const wxString &title,
                                 int flags)
{
    m_windowStyle = flags;
    m_FindReplaceData = data;
    m_parent = parent;

    SetTitle(title);

    // we must have a parent as it will get the messages from us
    return parent != NULL;
}

// ----------------------------------------------------------------------------
// wxFindReplaceData show/hide
// ----------------------------------------------------------------------------

bool wxFindReplaceDialog::Show(bool show)
{
    if ( !wxWindowBase::Show(show) )
    {
        // visibility status didn't change
        return false;
    }

    // do we already have the dialog window?
    if ( m_hWnd )
    {
        // yes, just use it
        (void)::ShowWindow(GetHwnd(), show ? SW_SHOW : SW_HIDE);

        return true;
    }

    if ( !show )
    {
        // well, it doesn't exist which is as good as being hidden
        return true;
    }

    wxCHECK_MSG( m_FindReplaceData, false, wxT("call Create() first!") );

    wxASSERT_MSG( !m_impl, wxT("why don't we have the window then?") );

    m_impl = new wxFindReplaceDialogImpl(this, m_FindReplaceData->GetFlags());

    m_impl->InitFindWhat(m_FindReplaceData->GetFindString());

    bool replace = HasFlag(wxFR_REPLACEDIALOG);
    if ( replace )
    {
        m_impl->InitReplaceWith(m_FindReplaceData->GetReplaceString());
    }

    // call the right function to show the dialog which does what we want
    FINDREPLACE *pFR = m_impl->GetPtrFindReplace();
    HWND hwnd;
    if ( replace )
        hwnd = ::ReplaceText(pFR);
    else
        hwnd = ::FindText(pFR);

    if ( !hwnd )
    {
        wxLogError(_("Failed to create the standard find/replace dialog (error code %d)"),
                   ::CommDlgExtendedError());

        wxDELETE(m_impl);

        return false;
    }

    if ( !::ShowWindow(hwnd, SW_SHOW) )
    {
        wxLogLastError(wxT("ShowWindow(find dialog)"));
    }

    m_hWnd = (WXHWND)hwnd;

    return true;
}

// ----------------------------------------------------------------------------
// wxFindReplaceDialog title handling
// ----------------------------------------------------------------------------

// we set the title of this dialog in our jook proc but for now don't crash in
// the base class version because of m_hWnd == 0

void wxFindReplaceDialog::SetTitle( const wxString& title)
{
    m_title = title;
}

wxString wxFindReplaceDialog::GetTitle() const
{
    return m_title;
}

// ----------------------------------------------------------------------------
// wxFindReplaceDialog position/size
// ----------------------------------------------------------------------------

void wxFindReplaceDialog::DoSetSize(int WXUNUSED(x), int WXUNUSED(y),
                                    int WXUNUSED(width), int WXUNUSED(height),
                                    int WXUNUSED(sizeFlags))
{
    // ignore - we can't change the size of this standard dialog
    return;
}

// NB: of course, both of these functions are completely bogus, but it's better
//     than nothing
void wxFindReplaceDialog::DoGetSize(int *width, int *height) const
{
    // the standard dialog size
    if ( width )
        *width = 225;
    if ( height )
        *height = 324;
}

void wxFindReplaceDialog::DoGetClientSize(int *width, int *height) const
{
    // the standard dialog size
    if ( width )
        *width = 219;
    if ( height )
        *height = 299;
}

#endif // wxUSE_FINDREPLDLG
