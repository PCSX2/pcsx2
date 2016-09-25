/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/spinbutt.cpp
// Purpose:     wxSpinButton
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
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

#ifndef WX_PRECOMP
    #include "wx/msw/wrapcctl.h" // include <commctrl.h> "properly"
    #include "wx/app.h"
#endif

#if wxUSE_SPINBTN

#include "wx/spinbutt.h"

#include "wx/msw/private.h"

#ifndef UDM_SETRANGE32
    #define UDM_SETRANGE32 (WM_USER+111)
#endif

#ifndef UDM_SETPOS32
    #define UDM_SETPOS32 (WM_USER+113)
    #define UDM_GETPOS32 (WM_USER+114)
#endif

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxWin macros
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// wxSpinButton
// ----------------------------------------------------------------------------

bool wxSpinButton::Create(wxWindow *parent,
                          wxWindowID id,
                          const wxPoint& pos,
                          const wxSize& size,
                          long style,
                          const wxString& name)
{
    // basic initialization
    m_windowId = (id == wxID_ANY) ? NewControlId() : id;

    SetName(name);

    int x = pos.x;
    int y = pos.y;
    int width = size.x;
    int height = size.y;

    m_windowStyle = style;

    SetParent(parent);

    // get the right size for the control
    if ( width <= 0 || height <= 0 )
    {
        wxSize bestSize = DoGetBestSize();
        if ( width <= 0 )
            width = bestSize.x;
        if ( height <= 0 )
            height = bestSize.y;
    }

    if ( x < 0 )
        x = 0;
    if ( y < 0 )
        y = 0;

    // translate the styles
    DWORD wstyle = WS_VISIBLE | WS_CHILD | WS_TABSTOP | /*  WS_CLIPSIBLINGS | */
                   UDS_NOTHOUSANDS | // never useful, sometimes harmful
                   UDS_SETBUDDYINT;  // it doesn't harm if we don't have buddy

    if ( m_windowStyle & wxCLIP_SIBLINGS )
        wstyle |= WS_CLIPSIBLINGS;
    if ( m_windowStyle & wxSP_HORIZONTAL )
        wstyle |= UDS_HORZ;
    if ( m_windowStyle & wxSP_ARROW_KEYS )
        wstyle |= UDS_ARROWKEYS;
    if ( m_windowStyle & wxSP_WRAP )
        wstyle |= UDS_WRAP;

    // create the UpDown control.
    m_hWnd = (WXHWND)CreateUpDownControl
                     (
                       wstyle,
                       x, y, width, height,
                       GetHwndOf(parent),
                       m_windowId,
                       wxGetInstance(),
                       NULL, // no buddy
                       m_max, m_min,
                       m_min // initial position
                     );

    if ( !m_hWnd )
    {
        wxLogLastError(wxT("CreateUpDownControl"));

        return false;
    }

    if ( parent )
    {
        parent->AddChild(this);
    }

    SubclassWin(m_hWnd);

    SetInitialSize(size);

    return true;
}

wxSpinButton::~wxSpinButton()
{
}

// ----------------------------------------------------------------------------
// size calculation
// ----------------------------------------------------------------------------

wxSize wxSpinButton::DoGetBestSize() const
{
    return GetBestSpinnerSize( (GetWindowStyle() & wxSP_VERTICAL) != 0 );
}

// ----------------------------------------------------------------------------
// Attributes
// ----------------------------------------------------------------------------

int wxSpinButton::GetValue() const
{
    int n;
#ifdef UDM_GETPOS32
    if ( wxApp::GetComCtl32Version() >= 580 )
    {
        // use the full 32 bit range if available
        n = ::SendMessage(GetHwnd(), UDM_GETPOS32, 0, 0);
    }
    else
#endif // UDM_GETPOS32
    {
        // we're limited to 16 bit
        n = (short)LOWORD(::SendMessage(GetHwnd(), UDM_GETPOS, 0, 0));
    }

    if (n < m_min) n = m_min;
    if (n > m_max) n = m_max;

    return n;
}

void wxSpinButton::SetValue(int val)
{
    // wxSpinButtonBase::SetValue(val); -- no, it is pure virtual

#ifdef UDM_SETPOS32
    if ( wxApp::GetComCtl32Version() >= 580 )
    {
        // use the full 32 bit range if available
        ::SendMessage(GetHwnd(), UDM_SETPOS32, 0, val);
    }
    else // we're limited to 16 bit
#endif // UDM_SETPOS32
    {
        ::SendMessage(GetHwnd(), UDM_SETPOS, 0, MAKELONG((short) val, 0));
    }
}

void wxSpinButton::NormalizeValue()
{
    SetValue( GetValue() );
}

void wxSpinButton::SetRange(int minVal, int maxVal)
{
    const bool hadRange = m_min < m_max;

    wxSpinButtonBase::SetRange(minVal, maxVal);

#ifdef UDM_SETRANGE32
    if ( wxApp::GetComCtl32Version() >= 471 )
    {
        // use the full 32 bit range if available
        ::SendMessage(GetHwnd(), UDM_SETRANGE32, minVal, maxVal);
    }
    else // we're limited to 16 bit
#endif // UDM_SETRANGE32
    {
        ::SendMessage(GetHwnd(), UDM_SETRANGE, 0,
                      (LPARAM) MAKELONG((short)maxVal, (short)minVal));
    }

    // the current value might be out of the new range, force it to be in it
    NormalizeValue();

    // if range was valid but becomes degenerated (min == max) now or vice
    // versa then the spin buttons are automatically disabled/enabled back
    // but don't update themselves for some reason, so do it manually
    if ( hadRange != (m_min < m_max) )
    {
        // update the visual state of the button
        Refresh();
    }
}

bool wxSpinButton::MSWOnScroll(int WXUNUSED(orientation), WXWORD wParam,
                               WXWORD WXUNUSED(pos), WXHWND control)
{
    wxCHECK_MSG( control, false, wxT("scrolling what?") );

    if ( wParam != SB_THUMBPOSITION )
    {
        // probable SB_ENDSCROLL - we don't react to it
        return false;
    }

    wxSpinEvent event(wxEVT_SCROLL_THUMBTRACK, m_windowId);
    // We can't use 16 bit position provided in this message for spin buttons
    // using 32 bit range.
    event.SetPosition(GetValue());
    event.SetEventObject(this);

    return HandleWindowEvent(event);
}

bool wxSpinButton::MSWOnNotify(int WXUNUSED(idCtrl), WXLPARAM lParam, WXLPARAM *result)
{
    NM_UPDOWN *lpnmud = (NM_UPDOWN *)lParam;

    if (lpnmud->hdr.hwndFrom != GetHwnd()) // make sure it is the right control
        return false;

    wxSpinEvent event(lpnmud->iDelta > 0 ? wxEVT_SCROLL_LINEUP
                                         : wxEVT_SCROLL_LINEDOWN,
                      m_windowId);
    event.SetPosition(lpnmud->iPos + lpnmud->iDelta);
    event.SetEventObject(this);

    bool processed = HandleWindowEvent(event);

    *result = event.IsAllowed() ? 0 : 1;

    return processed;
}

bool wxSpinButton::MSWCommand(WXUINT WXUNUSED(cmd), WXWORD WXUNUSED(id))
{
    // No command messages
    return false;
}

#endif // wxUSE_SPINBTN
