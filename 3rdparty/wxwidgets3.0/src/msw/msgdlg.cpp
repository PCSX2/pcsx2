/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/msgdlg.cpp
// Purpose:     wxMessageDialog
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_MSGDLG

// there is no hook support under CE so we can't use the code for message box
// positioning there
#ifndef __WXWINCE__
    #define wxUSE_MSGBOX_HOOK 1
#else
    #define wxUSE_MSGBOX_HOOK 0
#endif

#ifndef WX_PRECOMP
    #include "wx/msgdlg.h"
    #include "wx/app.h"
    #include "wx/intl.h"
    #include "wx/utils.h"
    #include "wx/msw/private.h"
    #if wxUSE_MSGBOX_HOOK
        #include "wx/hashmap.h"
    #endif
#endif

#include "wx/ptr_scpd.h"
#include "wx/dynlib.h"
#include "wx/msw/private/button.h"
#include "wx/msw/private/metrics.h"
#include "wx/msw/private/msgdlg.h"
#include "wx/modalhook.h"
#include "wx/fontutil.h"

#if wxUSE_MSGBOX_HOOK
    #include "wx/textbuf.h"
    #include "wx/display.h"
#endif

// For MB_TASKMODAL
#ifdef __WXWINCE__
    #include "wx/msw/wince/missing.h"
#endif

// Interestingly, this symbol currently seems to be absent from Platform SDK
// headers but it is documented at MSDN.
#ifndef TDF_SIZE_TO_CONTENT
    #define TDF_SIZE_TO_CONTENT 0x1000000
#endif

using namespace wxMSWMessageDialog;

IMPLEMENT_CLASS(wxMessageDialog, wxDialog)

#if wxUSE_MSGBOX_HOOK

// there can potentially be one message box per thread so we use a hash map
// with thread ids as keys and (currently shown) message boxes as values
//
// TODO: replace this with wxTLS once it's available
WX_DECLARE_HASH_MAP(unsigned long, wxMessageDialog *,
                    wxIntegerHash, wxIntegerEqual,
                    wxMessageDialogMap);

// the order in this array is the one in which buttons appear in the
// message box
const wxMessageDialog::ButtonAccessors wxMessageDialog::ms_buttons[] =
{
    { IDYES,    &wxMessageDialog::GetYesLabel    },
    { IDNO,     &wxMessageDialog::GetNoLabel     },
    { IDOK,     &wxMessageDialog::GetOKLabel     },
    { IDCANCEL, &wxMessageDialog::GetCancelLabel },
};

namespace
{

wxMessageDialogMap& HookMap()
{
    static wxMessageDialogMap s_Map;

    return s_Map;
}

/*
    All this code is used for adjusting the message box layout when we mess
    with its contents. It's rather complicated because we try hard to avoid
    assuming much about the standard layout details and so, instead of just
    laying out everything ourselves (which would have been so much simpler!)
    we try to only modify the existing controls positions by offsetting them
    from their default ones in the hope that this will continue to work with
    the future Windows versions.
 */

// convert the given RECT from screen to client coordinates in place
void ScreenRectToClient(HWND hwnd, RECT& rc)
{
    // map from desktop (i.e. screen) coordinates to ones of this window
    //
    // notice that a RECT is laid out as 2 consecutive POINTs so the cast is
    // valid
    ::MapWindowPoints(HWND_DESKTOP, hwnd, reinterpret_cast<POINT *>(&rc), 2);
}

// set window position to the given rect
inline void SetWindowRect(HWND hwnd, const RECT& rc)
{
    ::MoveWindow(hwnd,
                 rc.left, rc.top,
                 rc.right - rc.left, rc.bottom - rc.top,
                 FALSE);
}

// set window position expressed in screen coordinates, whether the window is
// child or top level
void MoveWindowToScreenRect(HWND hwnd, RECT rc)
{
    ScreenRectToClient(::GetParent(hwnd), rc);

    SetWindowRect(hwnd, rc);
}

} // anonymous namespace

/* static */
WXLRESULT wxCALLBACK
wxMessageDialog::HookFunction(int code, WXWPARAM wParam, WXLPARAM lParam)
{
    // Find the thread-local instance of wxMessageDialog
    const DWORD tid = ::GetCurrentThreadId();
    wxMessageDialogMap::iterator node = HookMap().find(tid);
    wxCHECK_MSG( node != HookMap().end(), false,
                    wxT("bogus thread id in wxMessageDialog::Hook") );

    wxMessageDialog *  const wnd = node->second;

    const HHOOK hhook = (HHOOK)wnd->m_hook;
    const LRESULT rc = ::CallNextHookEx(hhook, code, wParam, lParam);

    if ( code == HCBT_ACTIVATE )
    {
        // we won't need this hook any longer
        ::UnhookWindowsHookEx(hhook);
        wnd->m_hook = NULL;
        HookMap().erase(tid);

        wnd->SetHWND((HWND)wParam);

        // replace the static text with an edit control if the message box is
        // too big to fit the display
        wnd->ReplaceStaticWithEdit();

        // update the labels if necessary: we need to do it before centering
        // the dialog as this can change its size
        if ( wnd->HasCustomLabels() )
            wnd->AdjustButtonLabels();

        // centre the message box on its parent if requested
        if ( wnd->GetMessageDialogStyle() & wxCENTER )
            wnd->Center(); // center on parent
        //else: default behaviour, center on screen

        // there seems to be no reason to leave it set
        wnd->SetHWND(NULL);
    }

    return rc;
}

void wxMessageDialog::ReplaceStaticWithEdit()
{
    // check if the message box fits the display
    int nDisplay = wxDisplay::GetFromWindow(this);
    if ( nDisplay == wxNOT_FOUND )
        nDisplay = 0;
    const wxRect rectDisplay = wxDisplay(nDisplay).GetClientArea();

    if ( rectDisplay.Contains(GetRect()) )
    {
        // nothing to do
        return;
    }


    // find the static control to replace: normally there are two of them, the
    // icon and the text itself so search for all of them and ignore the icon
    // ones
    HWND hwndStatic = ::FindWindowEx(GetHwnd(), NULL, wxT("STATIC"), NULL);
    if ( ::GetWindowLong(hwndStatic, GWL_STYLE) & SS_ICON )
        hwndStatic = ::FindWindowEx(GetHwnd(), hwndStatic, wxT("STATIC"), NULL);

    if ( !hwndStatic )
    {
        wxLogDebug("Failed to find the static text control in message box.");
        return;
    }

    // set the right font for GetCharHeight() call below
    wxWindowBase::SetFont(GetMessageFont());

    // put the new edit control at the same place
    RECT rc = wxGetWindowRect(hwndStatic);
    ScreenRectToClient(GetHwnd(), rc);

    // but make it less tall so that the message box fits on the screen: we try
    // to make the message box take no more than 7/8 of the screen to leave
    // some space above and below it
    const int hText = (7*rectDisplay.height)/8 -
                      (
                         2*::GetSystemMetrics(SM_CYFIXEDFRAME) +
                         ::GetSystemMetrics(SM_CYCAPTION) +
                         5*GetCharHeight() // buttons + margins
                      );
    const int dh = (rc.bottom - rc.top) - hText; // vertical space we save
    rc.bottom -= dh;

    // and it also must be wider as it needs a vertical scrollbar (in order
    // to preserve the word wrap, otherwise the number of lines would change
    // and we want the control to look as similar as possible to the original)
    //
    // NB: you would have thought that 2*SM_CXEDGE would be enough but it
    //     isn't, somehow, and the text control breaks lines differently from
    //     the static one so fudge by adding some extra space
    const int dw = ::GetSystemMetrics(SM_CXVSCROLL) +
                        4*::GetSystemMetrics(SM_CXEDGE);
    rc.right += dw;


    // chop of the trailing new line(s) from the message box text, they are
    // ignored by the static control but result in extra lines and hence extra
    // scrollbar position in the edit one
    wxString text(wxGetWindowText(hwndStatic));
    for ( wxString::reverse_iterator i = text.rbegin(); i != text.rend(); ++i )
    {
        if ( *i != '\n' )
        {
            // found last non-newline char, remove anything after it if
            // necessary and stop in any case
            if ( i != text.rbegin() )
                text.erase(i.base() + 1, text.end());
            break;
        }
    }

    // do create the new control
    HWND hwndEdit = ::CreateWindow
                      (
                        wxT("EDIT"),
                        wxTextBuffer::Translate(text).t_str(),
                        WS_CHILD | WS_VSCROLL | WS_VISIBLE |
                        ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                        rc.left, rc.top,
                        rc.right - rc.left, rc.bottom - rc.top,
                        GetHwnd(),
                        NULL,
                        wxGetInstance(),
                        NULL
                      );

    if ( !hwndEdit )
    {
        wxLogDebug("Creation of replacement edit control failed in message box");
        return;
    }

    // copy the font from the original control
    LRESULT hfont = ::SendMessage(hwndStatic, WM_GETFONT, 0, 0);
    ::SendMessage(hwndEdit, WM_SETFONT, hfont, 0);

    // and get rid of it
    ::DestroyWindow(hwndStatic);


    // shrink and centre the message box vertically and widen it box to account
    // for the extra scrollbar
    RECT rcBox = wxGetWindowRect(GetHwnd());
    const int hMsgBox = rcBox.bottom - rcBox.top - dh;
    rcBox.top = (rectDisplay.height - hMsgBox)/2;
    rcBox.bottom = rcBox.top + hMsgBox + (rectDisplay.height - hMsgBox)%2;
    rcBox.left -= dw/2;
    rcBox.right += dw - dw/2;
    SetWindowRect(GetHwnd(), rcBox);

    // and adjust all the buttons positions
    for ( unsigned n = 0; n < WXSIZEOF(ms_buttons); n++ )
    {
        const HWND hwndBtn = ::GetDlgItem(GetHwnd(), ms_buttons[n].id);
        if ( !hwndBtn )
            continue;   // it's ok, not all buttons are always present

        rc = wxGetWindowRect(hwndBtn);
        rc.top -= dh;
        rc.bottom -= dh;
        rc.left += dw/2;
        rc.right += dw/2;
        MoveWindowToScreenRect(hwndBtn, rc);
    }
}

void wxMessageDialog::AdjustButtonLabels()
{
    // changing the button labels is the easy part but we also need to ensure
    // that the buttons are big enough for the label strings and increase their
    // size (and maybe the size of the message box itself) if they are not

    // TODO-RTL: check whether this works correctly in RTL

    // we want to use this font in GetTextExtent() calls below but we don't
    // want to send WM_SETFONT to the message box, who knows how is it going to
    // react to it (right now it doesn't seem to do anything but what if this
    // changes)
    wxWindowBase::SetFont(GetMessageFont());

    // first iteration: find the widest button and update the buttons labels
    int wBtnOld = 0,            // current buttons width
        wBtnNew = 0;            // required new buttons width
    RECT rcBtn;                 // stores the button height and y positions
    unsigned numButtons = 0;    // total number of buttons in the message box
    unsigned n;
    for ( n = 0; n < WXSIZEOF(ms_buttons); n++ )
    {
        const HWND hwndBtn = ::GetDlgItem(GetHwnd(), ms_buttons[n].id);
        if ( !hwndBtn )
            continue;   // it's ok, not all buttons are always present

        numButtons++;

        const wxString label = (this->*ms_buttons[n].getter)();
        const wxSize sizeLabel = wxWindowBase::GetTextExtent(label);

        // check if the button is big enough for this label
        const RECT rc = wxGetWindowRect(hwndBtn);
        if ( !wBtnOld )
        {
            // initialize wBtnOld using the first button width, all the other
            // ones should have the same one
            wBtnOld = rc.right - rc.left;

            rcBtn = rc; // remember for use below when we reposition the buttons
        }
        else
        {
            wxASSERT_MSG( wBtnOld == rc.right - rc.left,
                          "all buttons are supposed to be of same width" );
        }

        const int widthNeeded = wxMSWButton::GetFittingSize(this, sizeLabel).x;
        if ( widthNeeded > wBtnNew )
            wBtnNew = widthNeeded;

        ::SetWindowText(hwndBtn, label.t_str());
    }

    if ( wBtnNew <= wBtnOld )
    {
        // all buttons fit, nothing else to do
        return;
    }

    // resize the message box to be wider if needed
    const int wBoxOld = wxGetClientRect(GetHwnd()).right;

    const int CHAR_WIDTH = GetCharWidth();
    const int MARGIN_OUTER = 2*CHAR_WIDTH;  // margin between box and buttons
    const int MARGIN_INNER = CHAR_WIDTH;    // margin between buttons

    RECT rcBox = wxGetWindowRect(GetHwnd());

    const int wAllButtons = numButtons*(wBtnNew + MARGIN_INNER) - MARGIN_INNER;
    int wBoxNew = 2*MARGIN_OUTER + wAllButtons;
    if ( wBoxNew > wBoxOld )
    {
        const int dw = wBoxNew - wBoxOld;
        rcBox.left -= dw/2;
        rcBox.right += dw - dw/2;

        SetWindowRect(GetHwnd(), rcBox);

        // surprisingly, we don't need to resize the static text control, it
        // seems to adjust itself to the new size, at least under Windows 2003
        // (TODO: test if this happens on older Windows versions)
    }
    else // the current width is big enough
    {
        wBoxNew = wBoxOld;
    }


    // finally position all buttons

    // notice that we have to take into account the difference between window
    // and client width
    rcBtn.left = (rcBox.left + rcBox.right - wxGetClientRect(GetHwnd()).right +
                  wBoxNew - wAllButtons) / 2;
    rcBtn.right = rcBtn.left + wBtnNew;

    for ( n = 0; n < WXSIZEOF(ms_buttons); n++ )
    {
        const HWND hwndBtn = ::GetDlgItem(GetHwnd(), ms_buttons[n].id);
        if ( !hwndBtn )
            continue;

        MoveWindowToScreenRect(hwndBtn, rcBtn);

        rcBtn.left += wBtnNew + MARGIN_INNER;
        rcBtn.right += wBtnNew + MARGIN_INNER;
    }
}

#endif // wxUSE_MSGBOX_HOOK

/* static */
wxFont wxMessageDialog::GetMessageFont()
{
    const NONCLIENTMETRICS& ncm = wxMSWImpl::GetNonClientMetrics();
    return wxNativeFontInfo(ncm.lfMessageFont);
}

int wxMessageDialog::ShowMessageBox()
{
    if ( !wxTheApp->GetTopWindow() )
    {
        // when the message box is shown from wxApp::OnInit() (i.e. before the
        // message loop is entered), this must be done or the next message box
        // will never be shown - just try putting 2 calls to wxMessageBox() in
        // OnInit() to see it
        while ( wxTheApp->Pending() )
            wxTheApp->Dispatch();
    }

    // use the top level window as parent if none specified
    m_parent = GetParentForModalDialog();
    HWND hWnd = m_parent ? GetHwndOf(m_parent) : NULL;

#if wxUSE_INTL
    // native message box always uses the current user locale but the program
    // may be using a different one and in this case we need to manually
    // translate the default button labels (if they're non default we have no
    // way to translate them and so we must assume they were already
    // translated) to avoid mismatch between the language of the message box
    // text and its buttons
    wxLocale * const loc = wxGetLocale();
    if ( loc && loc->GetLanguage() != wxLocale::GetSystemLanguage() )
    {
        if ( m_dialogStyle & wxYES_NO &&
                (GetCustomYesLabel().empty() && GetCustomNoLabel().empty()) )

        {
            // use the strings with mnemonics here as the native message box
            // does
            SetYesNoLabels(_("&Yes"), _("&No"));
        }

        // we may or not have the Ok/Cancel buttons but either we do have them
        // or we already made the labels custom because we called
        // SetYesNoLabels() above so doing this does no harm -- and is
        // necessary in wxYES_NO | wxCANCEL case
        //
        // note that we don't use mnemonics here for consistency with the
        // native message box (which probably doesn't use them because
        // Enter/Esc keys can be already used to dismiss the message box
        // using keyboard)
        if ( GetCustomOKLabel().empty() && GetCustomCancelLabel().empty() )
            SetOKCancelLabels(_("OK"), _("Cancel"));
    }
#endif // wxUSE_INTL

    // translate wx style in MSW
    unsigned int msStyle;
    const long wxStyle = GetMessageDialogStyle();
    if ( wxStyle & wxYES_NO )
    {
#if !(defined(__SMARTPHONE__) && defined(__WXWINCE__))
        if (wxStyle & wxCANCEL)
            msStyle = MB_YESNOCANCEL;
        else
#endif // !(__SMARTPHONE__ && __WXWINCE__)
            msStyle = MB_YESNO;

        if ( wxStyle & wxNO_DEFAULT )
            msStyle |= MB_DEFBUTTON2;
        else if ( wxStyle & wxCANCEL_DEFAULT )
            msStyle |= MB_DEFBUTTON3;
    }
    else // without Yes/No we're going to have an OK button
    {
        if ( wxStyle & wxCANCEL )
        {
            msStyle = MB_OKCANCEL;

            if ( wxStyle & wxCANCEL_DEFAULT )
                msStyle |= MB_DEFBUTTON2;
        }
        else // just "OK"
        {
            msStyle = MB_OK;
        }
    }

    if ( wxStyle & wxHELP )
    {
        msStyle |= MB_HELP;
    }

    // set the icon style
    switch ( GetEffectiveIcon() )
    {
        case wxICON_ERROR:
            msStyle |= MB_ICONHAND;
            break;

        case wxICON_WARNING:
            msStyle |= MB_ICONEXCLAMATION;
            break;

        case wxICON_QUESTION:
            msStyle |= MB_ICONQUESTION;
            break;

        case wxICON_INFORMATION:
            msStyle |= MB_ICONINFORMATION;
            break;
    }

    if ( wxStyle & wxSTAY_ON_TOP )
        msStyle |= MB_TOPMOST;

#ifndef __WXWINCE__
    if ( wxTheApp->GetLayoutDirection() == wxLayout_RightToLeft )
        msStyle |= MB_RTLREADING | MB_RIGHT;
#endif

    if (hWnd)
        msStyle |= MB_APPLMODAL;
    else
        msStyle |= MB_TASKMODAL;

    // per MSDN documentation for MessageBox() we can prefix the message with 2
    // right-to-left mark characters to tell the function to use RTL layout
    // (unfortunately this only works in Unicode builds)
    wxString message = GetFullMessage();
#if wxUSE_UNICODE
    if ( wxTheApp->GetLayoutDirection() == wxLayout_RightToLeft )
    {
        // NB: not all compilers support \u escapes
        static const wchar_t wchRLM = 0x200f;
        message.Prepend(wxString(wchRLM, 2));
    }
#endif // wxUSE_UNICODE

#if wxUSE_MSGBOX_HOOK
    // install the hook in any case as we don't know in advance if the message
    // box is not going to be too big (requiring the replacement of the static
    // control with an edit one)
    const DWORD tid = ::GetCurrentThreadId();
    m_hook = ::SetWindowsHookEx(WH_CBT,
                                &wxMessageDialog::HookFunction, NULL, tid);
    HookMap()[tid] = this;
#endif // wxUSE_MSGBOX_HOOK

    // do show the dialog
    int msAns = MessageBox(hWnd, message.t_str(), m_caption.t_str(), msStyle);

    return MSWTranslateReturnCode(msAns);
}

int wxMessageDialog::ShowModal()
{
    WX_HOOK_MODAL_DIALOG();

#ifdef wxHAS_MSW_TASKDIALOG
    if ( HasNativeTaskDialog() )
    {
        TaskDialogIndirect_t taskDialogIndirect = GetTaskDialogIndirectFunc();
        wxCHECK_MSG( taskDialogIndirect, wxID_CANCEL, wxS("no task dialog?") );

        WinStruct<TASKDIALOGCONFIG> tdc;
        wxMSWTaskDialogConfig wxTdc( *this );
        wxTdc.MSWCommonTaskDialogInit( tdc );

        int msAns;
        HRESULT hr = taskDialogIndirect( &tdc, &msAns, NULL, NULL );
        if ( FAILED(hr) )
        {
            wxLogApiError( "TaskDialogIndirect", hr );
            return wxID_CANCEL;
        }

        // In case only an "OK" button was specified we actually created a
        // "Cancel" button (see comment in MSWCommonTaskDialogInit). This
        // results in msAns being IDCANCEL while we want IDOK (just like
        // how the native MessageBox function does with only an "OK" button).
        if ( (msAns == IDCANCEL)
            && !(GetMessageDialogStyle() & (wxYES_NO|wxCANCEL)) )
        {
            msAns = IDOK;
        }

        return MSWTranslateReturnCode( msAns );
    }
#endif // wxHAS_MSW_TASKDIALOG

    return ShowMessageBox();
}

long wxMessageDialog::GetEffectiveIcon() const
{
    // only use the auth needed icon if available, otherwise fallback to the default logic
    if ( (m_dialogStyle & wxICON_AUTH_NEEDED) &&
        wxMSWMessageDialog::HasNativeTaskDialog() )
    {
        return wxICON_AUTH_NEEDED;
    }

    return wxMessageDialogBase::GetEffectiveIcon();
}

void wxMessageDialog::DoCentre(int dir)
{
#ifdef wxHAS_MSW_TASKDIALOG
    // Task dialog is always centered on its parent window and trying to center
    // it manually doesn't work because its HWND is not created yet so don't
    // even try as this would only result in (debug) error messages.
    if ( HasNativeTaskDialog() )
        return;
#endif // wxHAS_MSW_TASKDIALOG

    wxMessageDialogBase::DoCentre(dir);
}

// ----------------------------------------------------------------------------
// Helpers of the wxMSWMessageDialog namespace
// ----------------------------------------------------------------------------

#ifdef wxHAS_MSW_TASKDIALOG

wxMSWTaskDialogConfig::wxMSWTaskDialogConfig(const wxMessageDialogBase& dlg)
                     : buttons(new TASKDIALOG_BUTTON[MAX_BUTTONS])
{
    parent = dlg.GetParentForModalDialog();
    caption = dlg.GetCaption();
    message = dlg.GetMessage();
    extendedMessage = dlg.GetExtendedMessage();

    // Before wxMessageDialog added support for extended message it was common
    // practice to have long multiline texts in the message box with the first
    // line playing the role of the main message and the rest of the extended
    // one. Try to detect such usage automatically here by synthesizing the
    // extended message on our own if it wasn't given.
    if ( extendedMessage.empty() )
    {
        // Check if there is a blank separating line after the first line (this
        // is not the same as searching for "\n\n" as we want the automatically
        // recognized main message be single line to avoid embarrassing false
        // positives).
        const size_t posNL = message.find('\n');
        if ( posNL != wxString::npos &&
                posNL < message.length() - 1 &&
                    message[posNL + 1 ] == '\n' )
        {
            extendedMessage.assign(message, posNL + 2, wxString::npos);
            message.erase(posNL);
        }
    }

    iconId = dlg.GetEffectiveIcon();
    style = dlg.GetMessageDialogStyle();
    useCustomLabels = dlg.HasCustomLabels();
    btnYesLabel = dlg.GetYesLabel();
    btnNoLabel = dlg.GetNoLabel();
    btnOKLabel = dlg.GetOKLabel();
    btnCancelLabel = dlg.GetCancelLabel();
    btnHelpLabel = dlg.GetHelpLabel();
}

void wxMSWTaskDialogConfig::MSWCommonTaskDialogInit(TASKDIALOGCONFIG &tdc)
{
    // Use TDF_SIZE_TO_CONTENT to try to prevent Windows from truncating or
    // ellipsizing the message text. This doesn't always work as Windows will
    // still do it if the message contains too long "words" (i.e. runs of the
    // text without spaces) but at least it ensures that the message text is
    // fully shown for reasonably-sized words whereas without it using almost
    // any file system path in a message box would result in truncation.
    tdc.dwFlags = TDF_EXPAND_FOOTER_AREA |
                  TDF_POSITION_RELATIVE_TO_WINDOW |
                  TDF_SIZE_TO_CONTENT;
    tdc.hInstance = wxGetInstance();
    tdc.pszWindowTitle = caption.t_str();

    // use the top level window as parent if none specified
    tdc.hwndParent = parent ? GetHwndOf(parent) : NULL;

    if ( wxTheApp->GetLayoutDirection() == wxLayout_RightToLeft )
        tdc.dwFlags |= TDF_RTL_LAYOUT;

    // If we have both the main and extended messages, just use them as
    // intended. However if only one message is given we normally use it as the
    // content and not as the main instruction because the latter is supposed
    // to stand out compared to the former and doesn't look good if there is
    // nothing for it to contrast with. Finally, notice that the extended
    // message we use here might be automatically extracted from the main
    // message in our ctor, see comment there.
    if ( !extendedMessage.empty() )
    {
        tdc.pszMainInstruction = message.t_str();
        tdc.pszContent = extendedMessage.t_str();
    }
    else
    {
        tdc.pszContent = message.t_str();
    }

    // set an icon to be used, if possible
    switch ( iconId )
    {
        case wxICON_ERROR:
            tdc.pszMainIcon = TD_ERROR_ICON;
            break;

        case wxICON_WARNING:
            tdc.pszMainIcon = TD_WARNING_ICON;
            break;

        case wxICON_INFORMATION:
            tdc.pszMainIcon = TD_INFORMATION_ICON;
            break;

        case wxICON_AUTH_NEEDED:
            tdc.pszMainIcon = TD_SHIELD_ICON;
            break;
    }

    // custom label button array that can hold all buttons in use
    tdc.pButtons = buttons.get();

    if ( style & wxYES_NO )
    {
        AddTaskDialogButton(tdc, IDYES, TDCBF_YES_BUTTON, btnYesLabel);
        AddTaskDialogButton(tdc, IDNO,  TDCBF_NO_BUTTON,  btnNoLabel);

        if (style & wxCANCEL)
            AddTaskDialogButton(tdc, IDCANCEL,
                                TDCBF_CANCEL_BUTTON, btnCancelLabel);

        if ( style & wxNO_DEFAULT )
            tdc.nDefaultButton = IDNO;
        else if ( style & wxCANCEL_DEFAULT )
            tdc.nDefaultButton = IDCANCEL;
    }
    else // without Yes/No we're going to have an OK button
    {
        if ( style & wxCANCEL )
        {
            AddTaskDialogButton(tdc, IDOK, TDCBF_OK_BUTTON, btnOKLabel);
            AddTaskDialogButton(tdc, IDCANCEL,
                                TDCBF_CANCEL_BUTTON, btnCancelLabel);

            if ( style & wxCANCEL_DEFAULT )
                tdc.nDefaultButton = IDCANCEL;
        }
        else // Only "OK"
        {
            // We actually create a "Cancel" button instead because we want to
            // allow closing the dialog box with Escape (and also Alt-F4 or
            // clicking the close button in the title bar) which wouldn't work
            // without a Cancel button.
            if ( !useCustomLabels )
            {
                useCustomLabels = true;
                btnOKLabel = _("OK");
            }

            AddTaskDialogButton(tdc, IDCANCEL, TDCBF_CANCEL_BUTTON, btnOKLabel);
        }
    }

    if ( style & wxHELP )
    {
        // There is no support for "Help" button in the task dialog, it can
        // only show "Retry" or "Close" ones.
        useCustomLabels = true;

        AddTaskDialogButton(tdc, IDHELP, 0 /* not used */, btnHelpLabel);
    }
}

void wxMSWTaskDialogConfig::AddTaskDialogButton(TASKDIALOGCONFIG &tdc,
                                                int btnCustomId,
                                                int btnCommonId,
                                                const wxString& customLabel)
{
    if ( useCustomLabels )
    {
        // use custom buttons to implement custom labels
        TASKDIALOG_BUTTON &tdBtn = buttons[tdc.cButtons];

        tdBtn.nButtonID = btnCustomId;
        tdBtn.pszButtonText = customLabel.t_str();
        tdc.cButtons++;

        // We should never have more than 4 buttons currently as this is the
        // maximal number of buttons supported by the message dialog.
        wxASSERT_MSG( tdc.cButtons <= MAX_BUTTONS, wxT("Too many buttons") );
    }
    else
    {
        tdc.dwCommonButtons |= btnCommonId;
    }
}

// Task dialog can be used from different threads (and wxProgressDialog always
// uses it from another thread in fact) so protect access to the static
// variable below with a critical section.
wxCRIT_SECT_DECLARE(gs_csTaskDialogIndirect);

TaskDialogIndirect_t wxMSWMessageDialog::GetTaskDialogIndirectFunc()
{
    // Initialize the function pointer to an invalid value different from NULL
    // to avoid reloading comctl32.dll and trying to resolve it every time
    // we're called if task dialog is not available (notice that this may
    // happen even under Vista+ if we don't use comctl32.dll v6).
    static const TaskDialogIndirect_t
        INVALID_TASKDIALOG_FUNC = reinterpret_cast<TaskDialogIndirect_t>(-1);
    static TaskDialogIndirect_t s_TaskDialogIndirect = INVALID_TASKDIALOG_FUNC;

    wxCRIT_SECT_LOCKER(lock, gs_csTaskDialogIndirect);

    if ( s_TaskDialogIndirect == INVALID_TASKDIALOG_FUNC )
    {
        wxLoadedDLL dllComCtl32("comctl32.dll");
        wxDL_INIT_FUNC(s_, TaskDialogIndirect, dllComCtl32);
    }

    return s_TaskDialogIndirect;
}

#endif // wxHAS_MSW_TASKDIALOG

bool wxMSWMessageDialog::HasNativeTaskDialog()
{
#ifdef wxHAS_MSW_TASKDIALOG
    if ( wxGetWinVersion() >= wxWinVersion_6 )
    {
        if ( wxMSWMessageDialog::GetTaskDialogIndirectFunc() )
            return true;
    }
#endif // wxHAS_MSW_TASKDIALOG

    return false;
}

int wxMSWMessageDialog::MSWTranslateReturnCode(int msAns)
{
    int ans;
    switch (msAns)
    {
        default:
            wxFAIL_MSG(wxT("unexpected return code"));
            // fall through

        case IDCANCEL:
            ans = wxID_CANCEL;
            break;
        case IDOK:
            ans = wxID_OK;
            break;
        case IDYES:
            ans = wxID_YES;
            break;
        case IDNO:
            ans = wxID_NO;
            break;
        case IDHELP:
            ans = wxID_HELP;
            break;
    }

    return ans;
}

#endif // wxUSE_MSGDLG
