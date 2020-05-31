/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/spinctrl.cpp
// Purpose:     wxSpinCtrl class implementation for Win32
// Author:      Vadim Zeitlin
// Modified by:
// Created:     22.07.99
// Copyright:   (c) 1999-2005 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_SPINCTRL

#include "wx/spinctrl.h"

#ifndef WX_PRECOMP
    #include "wx/hashmap.h"
    #include "wx/msw/wrapcctl.h" // include <commctrl.h> "properly"
    #include "wx/event.h"
    #include "wx/textctrl.h"
    #include "wx/wxcrtvararg.h"
#endif

#include "wx/msw/private.h"

#if wxUSE_TOOLTIPS
    #include "wx/tooltip.h"
#endif // wxUSE_TOOLTIPS

#include <limits.h>         // for INT_MIN

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(wxSpinCtrl, wxSpinButton)
    EVT_CHAR(wxSpinCtrl::OnChar)
    EVT_SET_FOCUS(wxSpinCtrl::OnSetFocus)
    EVT_KILL_FOCUS(wxSpinCtrl::OnKillFocus)
END_EVENT_TABLE()

#define GetBuddyHwnd()      (HWND)(m_hwndBuddy)

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// the margin between the up-down control and its buddy (can be arbitrary,
// choose what you like - or may be decide during run-time depending on the
// font size?)
static const int MARGIN_BETWEEN = 1;


// ---------------------------------------------------------------------------
// global vars
// ---------------------------------------------------------------------------

namespace
{

// Global hash used to find the spin control corresponding to the given buddy
// text control HWND.
WX_DECLARE_HASH_MAP(HWND, wxSpinCtrl *,
                    wxPointerHash, wxPointerEqual,
                    SpinForTextCtrl);

SpinForTextCtrl gs_spinForTextCtrl;

} // anonymous namespace

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wnd proc for the buddy text ctrl
// ----------------------------------------------------------------------------

LRESULT APIENTRY _EXPORT wxBuddyTextWndProc(HWND hwnd,
                                            UINT message,
                                            WPARAM wParam,
                                            LPARAM lParam)
{
    wxSpinCtrl * const spin = wxSpinCtrl::GetSpinForTextCtrl(hwnd);

    // forward some messages (mostly the key and focus ones) to the spin ctrl
    switch ( message )
    {
        case WM_SETFOCUS:
            // if the focus comes from the spin control itself, don't set it
            // back to it -- we don't want to go into an infinite loop
            if ( (WXHWND)wParam == spin->GetHWND() )
                break;
            //else: fall through

        case WM_KILLFOCUS:
        case WM_CHAR:
        case WM_DEADCHAR:
        case WM_KEYUP:
        case WM_KEYDOWN:
#ifdef WM_HELP
        // we need to forward WM_HELP too to ensure that the context help
        // associated with wxSpinCtrl is shown when the text control part of it
        // is clicked with the "?" cursor
        case WM_HELP:
#endif
            {
                WXLRESULT result;
                if ( spin->MSWHandleMessage(&result, message, wParam, lParam) )
                {
                    // Do not let the message be processed by the window proc
                    // of the text control if it had been already handled at wx
                    // level, this is consistent with what happens for normal,
                    // non-composite controls.
                    return 0;
                }

                // The control may have been deleted at this point, so check.
                if ( !::IsWindow(hwnd) )
                    return 0;
            }
            break;

        case WM_GETDLGCODE:
            if ( spin->HasFlag(wxTE_PROCESS_ENTER) )
            {
                long dlgCode = ::CallWindowProc
                                 (
                                    CASTWNDPROC spin->GetBuddyWndProc(),
                                    hwnd,
                                    message,
                                    wParam,
                                    lParam
                                 );
                dlgCode |= DLGC_WANTMESSAGE;
                return dlgCode;
            }
            break;
    }

    return ::CallWindowProc(CASTWNDPROC spin->GetBuddyWndProc(),
                            hwnd, message, wParam, lParam);
}

/* static */
wxSpinCtrl *wxSpinCtrl::GetSpinForTextCtrl(WXHWND hwndBuddy)
{
    const SpinForTextCtrl::const_iterator
        it = gs_spinForTextCtrl.find(hwndBuddy);
    if ( it == gs_spinForTextCtrl.end() )
        return NULL;

    wxSpinCtrl * const spin = it->second;

    // sanity check
    wxASSERT_MSG( spin->m_hwndBuddy == hwndBuddy,
                  wxT("wxSpinCtrl has incorrect buddy HWND!") );

    return spin;
}

// process a WM_COMMAND generated by the buddy text control
bool wxSpinCtrl::ProcessTextCommand(WXWORD cmd, WXWORD WXUNUSED(id))
{
    if ( (cmd == EN_CHANGE) && (!m_blockEvent ))
    {
        wxCommandEvent event(wxEVT_TEXT, GetId());
        event.SetEventObject(this);
        wxString val = wxGetWindowText(m_hwndBuddy);
        event.SetString(val);
        event.SetInt(GetValue());
        return HandleWindowEvent(event);
    }

    // not processed
    return false;
}

void wxSpinCtrl::OnChar(wxKeyEvent& event)
{
    switch ( event.GetKeyCode() )
    {
        case WXK_RETURN:
            {
                wxCommandEvent evt(wxEVT_TEXT_ENTER, m_windowId);
                InitCommandEvent(evt);
                wxString val = wxGetWindowText(m_hwndBuddy);
                evt.SetString(val);
                evt.SetInt(GetValue());
                if ( HandleWindowEvent(evt) )
                    return;
                break;
            }

        case WXK_TAB:
            // always produce navigation event - even if we process TAB
            // ourselves the fact that we got here means that the user code
            // decided to skip processing of this TAB - probably to let it
            // do its default job.
            {
                wxNavigationKeyEvent eventNav;
                eventNav.SetDirection(!event.ShiftDown());
                eventNav.SetWindowChange(event.ControlDown());
                eventNav.SetEventObject(this);

                if ( GetParent()->HandleWindowEvent(eventNav) )
                    return;
            }
            break;
    }

    // no, we didn't process it
    event.Skip();
}

void wxSpinCtrl::OnKillFocus(wxFocusEvent& event)
{
    // ensure that a correct value is shown by the control
    NormalizeValue();
    event.Skip();
}

void wxSpinCtrl::OnSetFocus(wxFocusEvent& event)
{
    // when we get focus, give it to our buddy window as it needs it more than
    // we do
    ::SetFocus((HWND)m_hwndBuddy);

    event.Skip();
}

void wxSpinCtrl::NormalizeValue()
{
    const int value = GetValue();
    const bool changed = value != m_oldValue;

    // notice that we have to call SetValue() even if the value didn't change
    // because otherwise we could be left with empty buddy control when value
    // is 0, see comment in SetValue()
    SetValue(value);

    if ( changed )
    {
        SendSpinUpdate(value);
    }
}

// ----------------------------------------------------------------------------
// construction
// ----------------------------------------------------------------------------

void wxSpinCtrl::Init()
{
    m_blockEvent = false;
    m_hwndBuddy = NULL;
    m_wndProcBuddy = NULL;
    m_oldValue = INT_MIN;
}

bool wxSpinCtrl::Create(wxWindow *parent,
                        wxWindowID id,
                        const wxString& value,
                        const wxPoint& pos,
                        const wxSize& size,
                        long style,
                        int min, int max, int initial,
                        const wxString& name)
{
    // before using DoGetBestSize(), have to set style to let the base class
    // know whether this is a horizontal or vertical control (we're always
    // vertical)
    style |= wxSP_VERTICAL;

    if ( (style & wxBORDER_MASK) == wxBORDER_DEFAULT )
#ifdef __WXWINCE__
        style |= wxBORDER_SIMPLE;
#else
        style |= wxBORDER_SUNKEN;
#endif

    SetWindowStyle(style);

    WXDWORD exStyle = 0;
    WXDWORD msStyle = MSWGetStyle(GetWindowStyle(), & exStyle) ;

    // Scroll text automatically if there is not enough space to show all of
    // it, this is better than not allowing to enter more digits at all.
    msStyle |= ES_AUTOHSCROLL;

    // propagate text alignment style to text ctrl
    if ( style & wxALIGN_RIGHT )
        msStyle |= ES_RIGHT;
    else if ( style & wxALIGN_CENTER )
        msStyle |= ES_CENTER;

    // calculate the sizes: the size given is the total size for both controls
    // and we need to fit them both in the given width (height is the same)
    wxSize sizeText(size), sizeBtn(size);
    sizeBtn.x = wxSpinButton::DoGetBestSize().x;
    if ( sizeText.x <= 0 )
    {
        // DEFAULT_ITEM_WIDTH is the default width for the text control
        sizeText.x = DEFAULT_ITEM_WIDTH + MARGIN_BETWEEN + sizeBtn.x;
    }

    sizeText.x -= sizeBtn.x + MARGIN_BETWEEN;
    if ( sizeText.x <= 0 )
    {
        wxLogDebug(wxS("wxSpinCtrl \"%s\": initial width %d is too small, ")
                   wxS("at least %d pixels needed."),
                   name, size.x, sizeBtn.x + MARGIN_BETWEEN + 1);
    }

    wxPoint posBtn(pos);
    posBtn.x += sizeText.x + MARGIN_BETWEEN;

    // we must create the text control before the spin button for the purpose
    // of the dialog navigation: if there is a static text just before the spin
    // control, activating it by Alt-letter should give focus to the text
    // control, not the spin and the dialog navigation code will give focus to
    // the next control (at Windows level), not the one after it

    // create the text window

    m_hwndBuddy = (WXHWND)::CreateWindowEx
                    (
                     exStyle,                // sunken border
                     wxT("EDIT"),             // window class
                     NULL,                   // no window title
                     msStyle,                // style (will be shown later)
                     pos.x, pos.y,           // position
                     0, 0,                   // size (will be set later)
                     GetHwndOf(parent),      // parent
                     (HMENU)-1,              // control id
                     wxGetInstance(),        // app instance
                     NULL                    // unused client data
                    );

    if ( !m_hwndBuddy )
    {
        wxLogLastError(wxT("CreateWindow(buddy text window)"));

        return false;
    }


    // create the spin button
    if ( !wxSpinButton::Create(parent, id, posBtn, sizeBtn, style, name) )
    {
        return false;
    }

    wxSpinButtonBase::SetRange(min, max);

    // subclass the text ctrl to be able to intercept some events
    gs_spinForTextCtrl[GetBuddyHwnd()] = this;

    m_wndProcBuddy = (WXFARPROC)wxSetWindowProc(GetBuddyHwnd(),
                                                wxBuddyTextWndProc);

    // set up fonts and colours  (This is nomally done in MSWCreateControl)
    InheritAttributes();
    if (!m_hasFont)
        SetFont(GetDefaultAttributes().font);

    // set the size of the text window - can do it only now, because we
    // couldn't call DoGetBestSize() before as font wasn't set
    if ( sizeText.y <= 0 )
    {
        int cx, cy;
        wxGetCharSize(GetHWND(), &cx, &cy, GetFont());

        sizeText.y = EDIT_HEIGHT_FROM_CHAR_HEIGHT(cy);
    }

    SetInitialSize(size);

    (void)::ShowWindow(GetBuddyHwnd(), SW_SHOW);

    // associate the text window with the spin button
    (void)::SendMessage(GetHwnd(), UDM_SETBUDDY, (WPARAM)m_hwndBuddy, 0);

    // If the initial text value is actually a number, it overrides the
    // "initial" argument specified later.
    long initialFromText;
    if ( value.ToLong(&initialFromText) )
        initial = initialFromText;

    // Set the range in the native control: notice that we must do it before
    // calling SetValue() to use the correct validity checks for the initial
    // value.
    SetRange(min, max);
    SetValue(initial);

    // Also set the text part of the control if it was specified independently
    // but don't generate an event for this, it would be unexpected.
    m_blockEvent = true;
    if ( !value.empty() )
        SetValue(value);
    m_blockEvent = false;

    return true;
}

wxSpinCtrl::~wxSpinCtrl()
{
    // destroy the buddy window because this pointer which wxBuddyTextWndProc
    // uses will not soon be valid any more
    ::DestroyWindow( GetBuddyHwnd() );

    gs_spinForTextCtrl.erase(GetBuddyHwnd());
}

// ----------------------------------------------------------------------------
// wxSpinCtrl-specific methods
// ----------------------------------------------------------------------------

int wxSpinCtrl::GetBase() const
{
    return ::SendMessage(GetHwnd(), UDM_GETBASE, 0, 0);
}

bool wxSpinCtrl::SetBase(int base)
{
    if ( !::SendMessage(GetHwnd(), UDM_SETBASE, base, 0) )
        return false;

    // Whether we need to be able enter "x" or not influences whether we should
    // use ES_NUMBER for the buddy control.
    UpdateBuddyStyle();

    return true;
}

// ----------------------------------------------------------------------------
// wxTextCtrl-like methods
// ----------------------------------------------------------------------------

void wxSpinCtrl::SetValue(const wxString& text)
{
    if ( !::SetWindowText(GetBuddyHwnd(), text.c_str()) )
    {
        wxLogLastError(wxT("SetWindowText(buddy)"));
    }
}

void  wxSpinCtrl::SetValue(int val)
{
    m_blockEvent = true;

    wxSpinButton::SetValue(val);

    // Normally setting the value of the spin button is enough as it updates
    // its buddy control automatically but in a couple of situations it doesn't
    // do it, for whatever reason, do it explicitly then:
    const wxString text = wxGetWindowText(m_hwndBuddy);

    // First case is when the text control is empty and the value is 0: the
    // spin button just leaves it empty in this case, while we want to show 0
    // in it.
    if ( text.empty() && !val )
    {
        ::SetWindowText(GetBuddyHwnd(), wxT("0"));
    }

    // Another one is when we're using hexadecimal base but the user input
    // doesn't start with "0x" -- we prefer to show it to avoid ambiguity
    // between decimal and hexadecimal.
    if ( GetBase() == 16 &&
            (text.length() < 3 || text[0] != '0' ||
                (text[1] != 'x' && text[1] != 'X')) )
    {
        ::SetWindowText(GetBuddyHwnd(),
                        wxPrivate::wxSpinCtrlFormatAsHex(val, m_max).t_str());
    }

    m_oldValue = GetValue();

    m_blockEvent = false;
}

int wxSpinCtrl::GetValue() const
{
    const wxString val = wxGetWindowText(m_hwndBuddy);

    long n;
    if ( !val.ToLong(&n, GetBase()) )
        n = INT_MIN;

    if ( n < m_min )
        n = m_min;
    if ( n > m_max )
        n = m_max;

    return n;
}

void wxSpinCtrl::SetSelection(long from, long to)
{
    // if from and to are both -1, it means (in wxWidgets) that all text should
    // be selected - translate into Windows convention
    if ( (from == -1) && (to == -1) )
    {
        from = 0;
    }

    ::SendMessage(GetBuddyHwnd(), EM_SETSEL, (WPARAM)from, (LPARAM)to);
}

// ----------------------------------------------------------------------------
// wxSpinButton methods
// ----------------------------------------------------------------------------

void wxSpinCtrl::SetRange(int minVal, int maxVal)
{
    // Manually adjust the old value to avoid an event being sent from
    // NormalizeValue() called from inside the base class SetRange() as we're
    // not supposed to generate any events from here.
    if ( m_oldValue < minVal )
        m_oldValue = minVal;
    else if ( m_oldValue > maxVal )
        m_oldValue = maxVal;

    wxSpinButton::SetRange(minVal, maxVal);

    UpdateBuddyStyle();
}

void wxSpinCtrl::UpdateBuddyStyle()
{
    // this control is used for numeric entry so restrict the input to numeric
    // keys only -- but only if we don't need to be able to enter "-" in it as
    // otherwise this would become impossible and also if we don't use
    // hexadecimal as entering "x" of the "0x" prefix wouldn't be allowed
    // neither then
    const DWORD styleOld = ::GetWindowLong(GetBuddyHwnd(), GWL_STYLE);
    DWORD styleNew;
    if ( m_min < 0 || GetBase() != 10 )
        styleNew = styleOld & ~ES_NUMBER;
    else
        styleNew = styleOld | ES_NUMBER;

    if ( styleNew != styleOld )
        ::SetWindowLong(GetBuddyHwnd(), GWL_STYLE, styleNew);
}

// ----------------------------------------------------------------------------
// forward some methods to subcontrols
// ----------------------------------------------------------------------------

bool wxSpinCtrl::SetFont(const wxFont& font)
{
    if ( !wxWindowBase::SetFont(font) )
    {
        // nothing to do
        return false;
    }

    WXHANDLE hFont = GetFont().GetResourceHandle();
    (void)::SendMessage(GetBuddyHwnd(), WM_SETFONT, (WPARAM)hFont, TRUE);

    return true;
}

bool wxSpinCtrl::Show(bool show)
{
    if ( !wxControl::Show(show) )
    {
        return false;
    }

    ::ShowWindow(GetBuddyHwnd(), show ? SW_SHOW : SW_HIDE);

    return true;
}

bool wxSpinCtrl::Reparent(wxWindowBase *newParent)
{
    // Reparenting both the updown control and its buddy does not seem to work:
    // they continue to be connected somehow, but visually there is no feedback
    // on the buddy edit control. To avoid this problem, we reparent the buddy
    // window normally, but we recreate the updown control and reassign its
    // buddy.

    // Get the position before changing the parent as it would be offset after
    // changing it.
    const wxRect rect = GetRect();

    if ( !wxWindowBase::Reparent(newParent) )
        return false;

    newParent->GetChildren().DeleteObject(this);

    // destroy the old spin button after detaching it from this wxWindow object
    // (notice that m_hWnd will be reset by UnsubclassWin() so save it first)
    const HWND hwndOld = GetHwnd();
    UnsubclassWin();
    if ( !::DestroyWindow(hwndOld) )
    {
        wxLogLastError(wxT("DestroyWindow"));
    }

    // create and initialize the new one
    if ( !wxSpinButton::Create(GetParent(), GetId(),
                               rect.GetPosition(), rect.GetSize(),
                               GetWindowStyle(), GetName()) )
        return false;

    // reapply our values to wxSpinButton
    wxSpinButton::SetValue(GetValue());
    SetRange(m_min, m_max);

    // also set the size again with wxSIZE_ALLOW_MINUS_ONE flag: this is
    // necessary if our original position used -1 for either x or y
    SetSize(rect, wxSIZE_ALLOW_MINUS_ONE);

    // associate it with the buddy control again
    ::SetParent(GetBuddyHwnd(), GetHwndOf(GetParent()));
    (void)::SendMessage(GetHwnd(), UDM_SETBUDDY, (WPARAM)GetBuddyHwnd(), 0);

    return true;
}

bool wxSpinCtrl::Enable(bool enable)
{
    if ( !wxControl::Enable(enable) )
    {
        return false;
    }

    MSWEnableHWND(GetBuddyHwnd(), enable);

    return true;
}

void wxSpinCtrl::SetFocus()
{
    ::SetFocus(GetBuddyHwnd());
}

#if wxUSE_TOOLTIPS

void wxSpinCtrl::DoSetToolTip(wxToolTip *tip)
{
    wxSpinButton::DoSetToolTip(tip);

    if ( tip )
        tip->AddOtherWindow(m_hwndBuddy);
}

#endif // wxUSE_TOOLTIPS

// ----------------------------------------------------------------------------
// events processing and generation
// ----------------------------------------------------------------------------

void wxSpinCtrl::SendSpinUpdate(int value)
{
    wxSpinEvent event(wxEVT_SPINCTRL, GetId());
    event.SetEventObject(this);
    event.SetInt(value);

    (void)HandleWindowEvent(event);

    m_oldValue = value;
}

bool wxSpinCtrl::MSWOnScroll(int WXUNUSED(orientation), WXWORD wParam,
                             WXWORD WXUNUSED(pos), WXHWND control)
{
    wxCHECK_MSG( control, false, wxT("scrolling what?") );

    if ( wParam != SB_THUMBPOSITION )
    {
        // probable SB_ENDSCROLL - we don't react to it
        return false;
    }

    // Notice that we can't use "pos" from WM_VSCROLL as it is 16 bit and we
    // might be using 32 bit range.
    int new_value = GetValue();
    if (m_oldValue != new_value)
       SendSpinUpdate( new_value );

    return true;
}

bool wxSpinCtrl::MSWOnNotify(int WXUNUSED(idCtrl), WXLPARAM lParam, WXLPARAM *result)
{
    NM_UPDOWN *lpnmud = (NM_UPDOWN *)lParam;

    if (lpnmud->hdr.hwndFrom != GetHwnd()) // make sure it is the right control
        return false;

    *result = 0;  // never reject UP and DOWN events

    return TRUE;
}


// ----------------------------------------------------------------------------
// size calculations
// ----------------------------------------------------------------------------

wxSize wxSpinCtrl::DoGetBestSize() const
{
    return DoGetSizeFromTextSize(DEFAULT_ITEM_WIDTH);
}

wxSize wxSpinCtrl::DoGetSizeFromTextSize(int xlen, int ylen) const
{
    wxSize sizeBtn = wxSpinButton::DoGetBestSize();

    int y;
    wxGetCharSize(GetHWND(), NULL, &y, GetFont());
    // JACS: we should always use the height calculated
    // from above, because otherwise we'll get a spin control
    // that's too big. So never use the height calculated
    // from wxSpinButton::DoGetBestSize().

    wxSize tsize(xlen + sizeBtn.x + MARGIN_BETWEEN + 3*y/10 + 10,
                 EDIT_HEIGHT_FROM_CHAR_HEIGHT(y));

    // Check if the user requested a non-standard height.
    if ( ylen > 0 )
        tsize.IncBy(0, ylen - y);

    return tsize;
}

void wxSpinCtrl::DoMoveWindow(int x, int y, int width, int height)
{
    int widthBtn = wxSpinButton::DoGetBestSize().x;
    int widthText = width - widthBtn - MARGIN_BETWEEN;
    if ( widthText < 0 )
    {
        // This can happen during the initial window layout when it's total
        // size is too small to accommodate all the controls and usually is not
        // a problem because the window will be relaid out with enough space
        // later. Of course, if it isn't and this is our final size, then we
        // have a real problem but as we don't know if this is going to be the
        // case or not, just hope for the best -- we used to give a debug
        // warning here and this was annoying as it could result in dozens of
        // perfectly harmless warnings.
        widthText = 0;
    }

    // 1) The buddy window
    DoMoveSibling(m_hwndBuddy, x, y, widthText, height);

    // 2) The button window
    if ( widthText > 0 )
        x += widthText + MARGIN_BETWEEN;
    wxSpinButton::DoMoveWindow(x, y, widthBtn, height);
}

// get total size of the control
void wxSpinCtrl::DoGetSize(int *x, int *y) const
{
    RECT spinrect, textrect, ctrlrect;
    GetWindowRect(GetHwnd(), &spinrect);
    GetWindowRect(GetBuddyHwnd(), &textrect);
    UnionRect(&ctrlrect,&textrect, &spinrect);

    if ( x )
        *x = ctrlrect.right - ctrlrect.left;
    if ( y )
        *y = ctrlrect.bottom - ctrlrect.top;
}

void wxSpinCtrl::DoGetClientSize(int *x, int *y) const
{
    RECT spinrect = wxGetClientRect(GetHwnd());
    RECT textrect = wxGetClientRect(GetBuddyHwnd());
    RECT ctrlrect;
    UnionRect(&ctrlrect,&textrect, &spinrect);

    if ( x )
        *x = ctrlrect.right - ctrlrect.left;
    if ( y )
        *y = ctrlrect.bottom - ctrlrect.top;
}

void wxSpinCtrl::DoGetPosition(int *x, int *y) const
{
    // hack: pretend that our HWND is the text control just for a moment
    WXHWND hWnd = GetHWND();
    wxConstCast(this, wxSpinCtrl)->m_hWnd = m_hwndBuddy;

    wxSpinButton::DoGetPosition(x, y);

    wxConstCast(this, wxSpinCtrl)->m_hWnd = hWnd;
}

#endif // wxUSE_SPINCTRL
