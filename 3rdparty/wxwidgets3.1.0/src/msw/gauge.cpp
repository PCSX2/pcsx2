/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/gauge.cpp
// Purpose:     wxGauge class
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

#if wxUSE_GAUGE

#include "wx/gauge.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"

    #include "wx/msw/wrapcctl.h" // include <commctrl.h> "properly"
#endif

#include "wx/appprogress.h"
#include "wx/msw/private.h"

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// old commctrl.h (< 4.71) don't have those
#ifndef PBS_SMOOTH
    #define PBS_SMOOTH 0x01
#endif

#ifndef PBS_VERTICAL
    #define PBS_VERTICAL 0x04
#endif

#ifndef PBM_SETBARCOLOR
    #define PBM_SETBARCOLOR         (WM_USER+9)
#endif

#ifndef PBM_SETBKCOLOR
    #define PBM_SETBKCOLOR          0x2001
#endif

#ifndef PBS_MARQUEE
    #define PBS_MARQUEE             0x08
#endif

#ifndef PBM_SETMARQUEE
    #define PBM_SETMARQUEE          (WM_USER+10)
#endif

// ----------------------------------------------------------------------------
// wxWin macros
// ----------------------------------------------------------------------------

// ============================================================================
// wxGauge implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxGauge creation
// ----------------------------------------------------------------------------

bool wxGauge::Create(wxWindow *parent,
                     wxWindowID id,
                     int range,
                     const wxPoint& pos,
                     const wxSize& size,
                     long style,
                     const wxValidator& validator,
                     const wxString& name)
{
    if ( !CreateControl(parent, id, pos, size, style, validator, name) )
        return false;

    if ( !MSWCreateControl(PROGRESS_CLASS, wxEmptyString, pos, size) )
        return false;

    // in case we need to emulate indeterminate mode...
    m_nDirection = wxRIGHT;

    SetRange(range);

    return true;
}

wxGauge::~wxGauge()
{
}

WXDWORD wxGauge::MSWGetStyle(long style, WXDWORD *exstyle) const
{
    WXDWORD msStyle = wxControl::MSWGetStyle(style, exstyle);

    if ( style & wxGA_VERTICAL )
        msStyle |= PBS_VERTICAL;

    if ( style & wxGA_SMOOTH )
        msStyle |= PBS_SMOOTH;

    return msStyle;
}

// ----------------------------------------------------------------------------
// wxGauge geometry
// ----------------------------------------------------------------------------

wxSize wxGauge::DoGetBestSize() const
{
    // Windows HIG (http://msdn.microsoft.com/en-us/library/aa511279.aspx)
    // suggest progress bar size of "107 or 237 x 8 dialog units". Let's use
    // the smaller one.

    if (HasFlag(wxGA_VERTICAL))
        return ConvertDialogToPixels(wxSize(8, 107));
    else
        return ConvertDialogToPixels(wxSize(107, 8));
}

// ----------------------------------------------------------------------------
// wxGauge setters
// ----------------------------------------------------------------------------

void wxGauge::SetRange(int r)
{
    // Changing range implicitly means we're using determinate mode.
    if ( IsInIndeterminateMode() )
        SetDeterminateMode();

    m_rangeMax = r;

#ifdef PBM_SETRANGE32
    ::SendMessage(GetHwnd(), PBM_SETRANGE32, 0, r);
#else // !PBM_SETRANGE32
    // fall back to PBM_SETRANGE (limited to 16 bits)
    ::SendMessage(GetHwnd(), PBM_SETRANGE, 0, MAKELPARAM(0, r));
#endif // PBM_SETRANGE32/!PBM_SETRANGE32

    if ( m_appProgressIndicator )
        m_appProgressIndicator->SetRange(m_rangeMax);
}

void wxGauge::SetValue(int pos)
{
    // Setting the value implicitly means that we're using determinate mode.
    if ( IsInIndeterminateMode() )
        SetDeterminateMode();

    if ( GetValue() != pos )
    {
        m_gaugePos = pos;

        ::SendMessage(GetHwnd(), PBM_SETPOS, pos, 0);

        if ( m_appProgressIndicator )
        {
            m_appProgressIndicator->SetValue(pos);
            if ( pos == 0 )
            {
                m_appProgressIndicator->Reset();
            }
        }
    }
}

bool wxGauge::SetForegroundColour(const wxColour& col)
{
    if ( !wxControl::SetForegroundColour(col) )
        return false;

    ::SendMessage(GetHwnd(), PBM_SETBARCOLOR, 0, (LPARAM)wxColourToRGB(col));

    return true;
}

bool wxGauge::SetBackgroundColour(const wxColour& col)
{
    if ( !wxControl::SetBackgroundColour(col) )
        return false;

    ::SendMessage(GetHwnd(), PBM_SETBKCOLOR, 0, (LPARAM)wxColourToRGB(col));

    return true;
}

bool wxGauge::IsInIndeterminateMode() const
{
    return (::GetWindowLong(GetHwnd(), GWL_STYLE) & PBS_MARQUEE) != 0;
}

void wxGauge::SetIndeterminateMode()
{
    // Switch the control into indeterminate mode if necessary.
    if ( !IsInIndeterminateMode() )
    {
        const long style = ::GetWindowLong(GetHwnd(), GWL_STYLE);
        ::SetWindowLong(GetHwnd(), GWL_STYLE, style | PBS_MARQUEE);
        ::SendMessage(GetHwnd(), PBM_SETMARQUEE, TRUE, 0);
    }
}

void wxGauge::SetDeterminateMode()
{
    if ( IsInIndeterminateMode() )
    {
        const long style = ::GetWindowLong(GetHwnd(), GWL_STYLE);
        ::SendMessage(GetHwnd(), PBM_SETMARQUEE, FALSE, 0);
        ::SetWindowLong(GetHwnd(), GWL_STYLE, style & ~PBS_MARQUEE);
    }
}

void wxGauge::Pulse()
{
    if (wxApp::GetComCtl32Version() >= 600)
    {
        // switch to indeterminate mode if required
        SetIndeterminateMode();

        SendMessage(GetHwnd(), PBM_STEPIT, 0, 0);
    }
    else
    {
        // emulate indeterminate mode
        wxGaugeBase::Pulse();
    }

    if ( m_appProgressIndicator )
        m_appProgressIndicator->Pulse();
}

#endif // wxUSE_GAUGE
