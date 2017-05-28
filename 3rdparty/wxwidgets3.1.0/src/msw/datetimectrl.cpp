///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/datetimectrl.cpp
// Purpose:     Implementation of wxDateTimePickerCtrl for MSW.
// Author:      Vadim Zeitlin
// Created:     2011-09-22 (extracted from src/msw/datectrl.cpp)
// Copyright:   (c) 2005-2011 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

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

#include "wx/datetimectrl.h"

#ifdef wxNEEDS_DATETIMEPICKCTRL

#ifndef WX_PRECOMP
    #include "wx/msw/wrapwin.h"
    #include "wx/msw/wrapcctl.h" // include <commctrl.h> "properly"
    #include "wx/msw/private.h"
    #include "wx/dcclient.h"
#endif // WX_PRECOMP

#include "wx/msw/private/datecontrols.h"

// apparently some versions of mingw define these macros erroneously
#ifndef DateTime_GetSystemtime
    #define DateTime_GetSystemtime DateTime_GetSystemTime
#endif

#ifndef DateTime_SetSystemtime
    #define DateTime_SetSystemtime DateTime_SetSystemTime
#endif

#ifndef DTM_GETIDEALSIZE
    #define DTM_GETIDEALSIZE 0x100f
#endif

// ============================================================================
// wxDateTimePickerCtrl implementation
// ============================================================================

bool
wxDateTimePickerCtrl::MSWCreateDateTimePicker(wxWindow *parent,
                                              wxWindowID id,
                                              const wxDateTime& dt,
                                              const wxPoint& pos,
                                              const wxSize& size,
                                              long style,
                                              const wxValidator& validator,
                                              const wxString& name)
{
    if ( !wxMSWDateControls::CheckInitialization() )
        return false;

    // initialize the base class
    if ( !CreateControl(parent, id, pos, size, style, validator, name) )
        return false;

    // create the native control
    if ( !MSWCreateControl(DATETIMEPICK_CLASS, wxString(), pos, size) )
        return false;

    if ( dt.IsValid() || MSWAllowsNone() )
        SetValue(dt);
    else
        SetValue(wxDateTime::Now());

    return true;
}

void wxDateTimePickerCtrl::SetValue(const wxDateTime& dt)
{
    wxCHECK_RET( dt.IsValid() || MSWAllowsNone(),
                    wxT("this control requires a valid date") );

    SYSTEMTIME st;
    if ( dt.IsValid() )
        dt.GetAsMSWSysTime(&st);

    if ( !DateTime_SetSystemtime(GetHwnd(),
                                 dt.IsValid() ? GDT_VALID : GDT_NONE,
                                 &st) )
    {
        // The only expected failure is when the date is out of range but we
        // already checked for this above.
        wxFAIL_MSG( wxT("Setting the calendar date unexpectedly failed.") );

        // In any case, skip updating m_date below.
        return;
    }

    m_date = dt;
}

wxDateTime wxDateTimePickerCtrl::GetValue() const
{
    return m_date;
}

wxSize wxDateTimePickerCtrl::DoGetBestSize() const
{
    // Since Vista, the control can compute its best size itself, just ask it.
    wxSize size;
    if ( wxGetWinVersion() >= wxWinVersion_Vista )
    {
        SIZE idealSize;
        ::SendMessage(m_hWnd, DTM_GETIDEALSIZE, 0, (LPARAM)&idealSize);

        size = wxSize(idealSize.cx, idealSize.cy);
    }
    else // Windows XP
    {
        wxClientDC dc(const_cast<wxDateTimePickerCtrl *>(this));

        // Use the same native format as the underlying native control.
#if wxUSE_INTL
        wxString s = wxDateTime::Now().Format(wxLocale::GetOSInfo(MSWGetFormat()));
#else // !wxUSE_INTL
        wxString s("XXX-YYY-ZZZZ");
#endif // wxUSE_INTL/!wxUSE_INTL

        // the best size for the control is bigger than just the string
        // representation of the current value because the control must accommodate
        // any date and while the widths of all digits are usually about the same,
        // the width of the month string varies a lot, so try to account for it
        s += wxS("W");

        size = dc.GetTextExtent(s);

        // account for the drop-down arrow or spin arrows
        size.x += wxSystemSettings::GetMetric(wxSYS_HSCROLL_ARROW_X);
    }

    // We need to account for the checkbox, if we have one, ourselves as
    // DTM_GETIDEALSIZE doesn't seem to take it into account, at least under
    // Windows 7.
    if ( MSWAllowsNone() )
        size.x += 3*GetCharWidth();

    // In any case, adjust the height to be the same as for the text controls.
    size.y = EDIT_HEIGHT_FROM_CHAR_HEIGHT(size.y);

    return size;
}

bool
wxDateTimePickerCtrl::MSWOnNotify(int idCtrl, WXLPARAM lParam, WXLPARAM *result)
{
    NMHDR* hdr = (NMHDR *)lParam;
    switch ( hdr->code )
    {
        case DTN_DATETIMECHANGE:
            if ( MSWOnDateTimeChange(*(NMDATETIMECHANGE*)(hdr)) )
            {
                *result = 0;
                return true;
            }
            break;
    }

    return wxDateTimePickerCtrlBase::MSWOnNotify(idCtrl, lParam, result);
}

#endif // wxNEEDS_DATETIMEPICKCTRL
