///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/calctrlcmn.cpp
// Author:      Marcin Wojdyr
// Created:     2008-03-26
// Copyright:   (C) Marcin Wojdyr
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
#endif //WX_PRECOMP

#if wxUSE_CALENDARCTRL || wxUSE_DATEPICKCTRL || wxUSE_TIMEPICKCTRL

#include "wx/dateevt.h"
IMPLEMENT_DYNAMIC_CLASS(wxDateEvent, wxCommandEvent)
wxDEFINE_EVENT(wxEVT_DATE_CHANGED, wxDateEvent);
wxDEFINE_EVENT(wxEVT_TIME_CHANGED, wxDateEvent);

#endif // wxUSE_CALENDARCTRL || wxUSE_DATEPICKCTRL || wxUSE_TIMEPICKCTRL


#if wxUSE_CALENDARCTRL

#include "wx/calctrl.h"

// ----------------------------------------------------------------------------
// XTI
// ----------------------------------------------------------------------------

wxDEFINE_FLAGS( wxCalendarCtrlStyle )
wxBEGIN_FLAGS( wxCalendarCtrlStyle )
// new style border flags, we put them first to
// use them for streaming out
wxFLAGS_MEMBER(wxBORDER_SIMPLE)
wxFLAGS_MEMBER(wxBORDER_SUNKEN)
wxFLAGS_MEMBER(wxBORDER_DOUBLE)
wxFLAGS_MEMBER(wxBORDER_RAISED)
wxFLAGS_MEMBER(wxBORDER_STATIC)
wxFLAGS_MEMBER(wxBORDER_NONE)

// old style border flags
wxFLAGS_MEMBER(wxSIMPLE_BORDER)
wxFLAGS_MEMBER(wxSUNKEN_BORDER)
wxFLAGS_MEMBER(wxDOUBLE_BORDER)
wxFLAGS_MEMBER(wxRAISED_BORDER)
wxFLAGS_MEMBER(wxSTATIC_BORDER)
wxFLAGS_MEMBER(wxBORDER)

// standard window styles
wxFLAGS_MEMBER(wxTAB_TRAVERSAL)
wxFLAGS_MEMBER(wxCLIP_CHILDREN)
wxFLAGS_MEMBER(wxTRANSPARENT_WINDOW)
wxFLAGS_MEMBER(wxWANTS_CHARS)
wxFLAGS_MEMBER(wxFULL_REPAINT_ON_RESIZE)
wxFLAGS_MEMBER(wxALWAYS_SHOW_SB )
wxFLAGS_MEMBER(wxVSCROLL)
wxFLAGS_MEMBER(wxHSCROLL)

wxFLAGS_MEMBER(wxCAL_SUNDAY_FIRST)
wxFLAGS_MEMBER(wxCAL_MONDAY_FIRST)
wxFLAGS_MEMBER(wxCAL_SHOW_HOLIDAYS)
wxFLAGS_MEMBER(wxCAL_NO_YEAR_CHANGE)
wxFLAGS_MEMBER(wxCAL_NO_MONTH_CHANGE)
wxFLAGS_MEMBER(wxCAL_SEQUENTIAL_MONTH_SELECTION)
wxFLAGS_MEMBER(wxCAL_SHOW_SURROUNDING_WEEKS)

wxEND_FLAGS( wxCalendarCtrlStyle )

wxIMPLEMENT_DYNAMIC_CLASS_XTI(wxCalendarCtrl, wxControl, "wx/calctrl.h")

wxBEGIN_PROPERTIES_TABLE(wxCalendarCtrl)
wxEVENT_RANGE_PROPERTY( Updated, wxEVT_CALENDAR_SEL_CHANGED, \
                       wxEVT_CALENDAR_WEEKDAY_CLICKED, wxCalendarEvent )

wxHIDE_PROPERTY( Children )

wxPROPERTY( Date,wxDateTime, SetDate, GetDate, wxEMPTY_PARAMETER_VALUE, \
           0 /*flags*/, wxT("Helpstring"), wxT("group"))
wxPROPERTY_FLAGS( WindowStyle, wxCalendarCtrlStyle, long, \
                 SetWindowStyleFlag, GetWindowStyleFlag, \
                 wxEMPTY_PARAMETER_VALUE, 0 /*flags*/, wxT("Helpstring"), \
                 wxT("group")) // style
wxEND_PROPERTIES_TABLE()

wxEMPTY_HANDLERS_TABLE(wxCalendarCtrl)

wxCONSTRUCTOR_6( wxCalendarCtrl, wxWindow*, Parent, wxWindowID, Id, \
                wxDateTime, Date, wxPoint, Position, wxSize, Size, long, WindowStyle )

// ----------------------------------------------------------------------------
// events
// ----------------------------------------------------------------------------
IMPLEMENT_DYNAMIC_CLASS(wxCalendarEvent, wxDateEvent)

wxDEFINE_EVENT( wxEVT_CALENDAR_SEL_CHANGED, wxCalendarEvent );
wxDEFINE_EVENT( wxEVT_CALENDAR_PAGE_CHANGED, wxCalendarEvent );
wxDEFINE_EVENT( wxEVT_CALENDAR_DOUBLECLICKED, wxCalendarEvent );
wxDEFINE_EVENT( wxEVT_CALENDAR_WEEKDAY_CLICKED, wxCalendarEvent );
wxDEFINE_EVENT( wxEVT_CALENDAR_WEEK_CLICKED, wxCalendarEvent );

// deprecated events
wxDEFINE_EVENT( wxEVT_CALENDAR_DAY_CHANGED, wxCalendarEvent );
wxDEFINE_EVENT( wxEVT_CALENDAR_MONTH_CHANGED, wxCalendarEvent );
wxDEFINE_EVENT( wxEVT_CALENDAR_YEAR_CHANGED, wxCalendarEvent );


wxCalendarDateAttr wxCalendarDateAttr::m_mark(wxCAL_BORDER_SQUARE);

bool wxCalendarCtrlBase::EnableMonthChange(bool enable)
{
    const long styleOrig = GetWindowStyle();
    long style = enable ? styleOrig & ~wxCAL_NO_MONTH_CHANGE
                        : styleOrig | wxCAL_NO_MONTH_CHANGE;
    if ( style == styleOrig )
        return false;

    SetWindowStyle(style);

    return true;
}

bool wxCalendarCtrlBase::GenerateAllChangeEvents(const wxDateTime& dateOld)
{
    const wxDateTime::Tm tm1 = dateOld.GetTm(),
                         tm2 = GetDate().GetTm();

    bool pageChanged = false;

    GenerateEvent(wxEVT_CALENDAR_SEL_CHANGED);
    if ( tm1.year != tm2.year || tm1.mon != tm2.mon )
    {
        GenerateEvent(wxEVT_CALENDAR_PAGE_CHANGED);

        pageChanged = true;
    }

    // send also one of the deprecated events
    if ( tm1.year != tm2.year )
        GenerateEvent(wxEVT_CALENDAR_YEAR_CHANGED);
    else if ( tm1.mon != tm2.mon )
        GenerateEvent(wxEVT_CALENDAR_MONTH_CHANGED);
    else
        GenerateEvent(wxEVT_CALENDAR_DAY_CHANGED);

    return pageChanged;
}

void wxCalendarCtrlBase::EnableHolidayDisplay(bool display)
{
    long style = GetWindowStyle();
    if ( display )
        style |= wxCAL_SHOW_HOLIDAYS;
    else
        style &= ~wxCAL_SHOW_HOLIDAYS;

    if ( style == GetWindowStyle() )
        return;

    SetWindowStyle(style);

    if ( display )
        SetHolidayAttrs();
    else
        ResetHolidayAttrs();

    RefreshHolidays();
}

bool wxCalendarCtrlBase::SetHolidayAttrs()
{
    if ( !HasFlag(wxCAL_SHOW_HOLIDAYS) )
        return false;

    ResetHolidayAttrs();

    wxDateTime::Tm tm = GetDate().GetTm();
    wxDateTime dtStart(1, tm.mon, tm.year),
               dtEnd = dtStart.GetLastMonthDay();

    wxDateTimeArray hol;
    wxDateTimeHolidayAuthority::GetHolidaysInRange(dtStart, dtEnd, hol);

    const size_t count = hol.GetCount();
    for ( size_t n = 0; n < count; n++ )
    {
        SetHoliday(hol[n].GetDay());
    }

    return true;
}

#endif // wxUSE_CALENDARCTRL

