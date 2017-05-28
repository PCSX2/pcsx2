/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/calctrl.cpp
// Purpose:     wxCalendarCtrl implementation
// Author:      Vadim Zeitlin
// Created:     2008-04-04
// Copyright:   (C) 2008 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_CALENDARCTRL

#ifndef WX_PRECOMP
    #include "wx/msw/wrapwin.h"
    #include "wx/msw/wrapcctl.h" // include <commctrl.h> "properly"
    #include "wx/msw/private.h"
#endif

#include "wx/calctrl.h"

#include "wx/msw/private/datecontrols.h"

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

namespace
{

// values of week days used by the native control
enum
{
    MonthCal_Monday,
    MonthCal_Tuesday,
    MonthCal_Wednesday,
    MonthCal_Thursday,
    MonthCal_Friday,
    MonthCal_Saturday,
    MonthCal_Sunday
};

} // anonymous namespace

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxCalendarCtrl creation
// ----------------------------------------------------------------------------

void wxCalendarCtrl::Init()
{
    m_marks =
    m_holidays = 0;
}

bool
wxCalendarCtrl::Create(wxWindow *parent,
                       wxWindowID id,
                       const wxDateTime& dt,
                       const wxPoint& pos,
                       const wxSize& size,
                       long style,
                       const wxString& name)
{
    if ( !wxMSWDateControls::CheckInitialization() )
        return false;

    // we need the arrows for the navigation
    style |= wxWANTS_CHARS;

    // initialize the base class
    if ( !CreateControl(parent, id, pos, size, style, wxDefaultValidator, name) )
        return false;

    // create the native control: this is a bit tricky as we want to receive
    // double click events but the MONTHCAL_CLASS doesn't use CS_DBLCLKS style
    // and so we create our own copy of it which does
    static ClassRegistrar s_clsMonthCal;
    if ( !s_clsMonthCal.IsInitialized() )
    {
        // get a copy of standard class and modify it
        WNDCLASS wc;
        if ( ::GetClassInfo(NULL, MONTHCAL_CLASS, &wc) )
        {
            wc.lpszClassName = wxT("_wx_SysMonthCtl32");
            wc.style |= CS_DBLCLKS;
            s_clsMonthCal.Register(wc);
        }
        else
        {
            wxLogLastError(wxT("GetClassInfoEx(SysMonthCal32)"));
        }
    }

    const wxChar * const clsname = s_clsMonthCal.IsRegistered()
        ? static_cast<const wxChar*>(s_clsMonthCal.GetName().t_str())
        : MONTHCAL_CLASS;

    if ( !MSWCreateControl(clsname, wxEmptyString, pos, size) )
        return false;

    // initialize the control
    UpdateFirstDayOfWeek();

    SetDate(dt.IsValid() ? dt : wxDateTime::Today());

    SetHolidayAttrs();
    UpdateMarks();

    Connect(wxEVT_LEFT_DOWN,
            wxMouseEventHandler(wxCalendarCtrl::MSWOnClick));
    Connect(wxEVT_LEFT_DCLICK,
            wxMouseEventHandler(wxCalendarCtrl::MSWOnDoubleClick));

    return true;
}

WXDWORD wxCalendarCtrl::MSWGetStyle(long style, WXDWORD *exstyle) const
{
    WXDWORD styleMSW = wxCalendarCtrlBase::MSWGetStyle(style, exstyle);

    // right now we don't support all native styles but we should add wx styles
    // corresponding to MCS_NOTODAY and MCS_NOTODAYCIRCLE probably (TODO)

    // for compatibility with the other versions, just turn off today display
    // unconditionally for now
    styleMSW |= MCS_NOTODAY;

    // we also need this style for Mark() to work
    styleMSW |= MCS_DAYSTATE;

    if ( style & wxCAL_SHOW_WEEK_NUMBERS )
       styleMSW |= MCS_WEEKNUMBERS;

    return styleMSW;
}

void wxCalendarCtrl::SetWindowStyleFlag(long style)
{
    const bool hadMondayFirst = HasFlag(wxCAL_MONDAY_FIRST);

    wxCalendarCtrlBase::SetWindowStyleFlag(style);

    if ( HasFlag(wxCAL_MONDAY_FIRST) != hadMondayFirst )
        UpdateFirstDayOfWeek();
}

// ----------------------------------------------------------------------------
// wxCalendarCtrl geometry
// ----------------------------------------------------------------------------

// TODO: handle WM_WININICHANGE
wxSize wxCalendarCtrl::DoGetBestSize() const
{
    RECT rc;
    if ( !GetHwnd() || !MonthCal_GetMinReqRect(GetHwnd(), &rc) )
    {
        return wxCalendarCtrlBase::DoGetBestSize();
    }

    const wxSize best = wxRectFromRECT(rc).GetSize() + GetWindowBorderSize();
    CacheBestSize(best);
    return best;
}

wxCalendarHitTestResult
wxCalendarCtrl::HitTest(const wxPoint& pos,
                        wxDateTime *date,
                        wxDateTime::WeekDay *wd)
{
    WinStruct<MCHITTESTINFO> hti;

    // Vista and later SDKs add a few extra fields to MCHITTESTINFO which are
    // not supported by the previous versions, as we don't use them anyhow we
    // should pretend that we always use the old struct format to make the call
    // below work on pre-Vista systems (see #11057)
#ifdef MCHITTESTINFO_V1_SIZE
    hti.cbSize = MCHITTESTINFO_V1_SIZE;
#endif

    hti.pt.x = pos.x;
    hti.pt.y = pos.y;
    switch ( MonthCal_HitTest(GetHwnd(), &hti) )
    {
        default:
        case MCHT_CALENDARWEEKNUM:
            wxFAIL_MSG( "unexpected" );
            // fall through

        case MCHT_NOWHERE:
        case MCHT_CALENDARBK:
        case MCHT_TITLEBK:
        case MCHT_TITLEMONTH:
        case MCHT_TITLEYEAR:
            return wxCAL_HITTEST_NOWHERE;

        case MCHT_CALENDARDATE:
            if ( date )
                date->SetFromMSWSysDate(hti.st);
            return wxCAL_HITTEST_DAY;

        case MCHT_CALENDARDAY:
            if ( wd )
            {
                int day = hti.st.wDayOfWeek;

                // the native control returns incorrect day of the week when
                // the first day isn't Monday, i.e. the first column is always
                // "Monday" even if its label is "Sunday", compensate for it
                const int first = LOWORD(MonthCal_GetFirstDayOfWeek(GetHwnd()));
                if ( first == MonthCal_Monday )
                {
                    // as MonthCal_Monday is 0 while wxDateTime::Mon is 1,
                    // normally we need to do this to transform from MSW
                    // convention to wx one
                    day++;
                    day %= 7;
                }
                //else: but when the first day is MonthCal_Sunday, the native
                //      control still returns 0 (i.e. MonthCal_Monday) for the
                //      first column which looks like a bug in it but to work
                //      around it it's enough to not apply the correction above

                *wd = static_cast<wxDateTime::WeekDay>(day);
            }
            return wxCAL_HITTEST_HEADER;

        case MCHT_TITLEBTNNEXT:
            return wxCAL_HITTEST_INCMONTH;

        case MCHT_TITLEBTNPREV:
            return wxCAL_HITTEST_DECMONTH;

        case MCHT_CALENDARDATENEXT:
        case MCHT_CALENDARDATEPREV:
            return wxCAL_HITTEST_SURROUNDING_WEEK;
    }
}

// ----------------------------------------------------------------------------
// wxCalendarCtrl operations
// ----------------------------------------------------------------------------

bool wxCalendarCtrl::SetDate(const wxDateTime& dt)
{
    wxCHECK_MSG( dt.IsValid(), false, "invalid date" );

    SYSTEMTIME st;
    dt.GetAsMSWSysDate(&st);
    if ( !MonthCal_SetCurSel(GetHwnd(), &st) )
    {
        wxLogDebug(wxT("DateTime_SetSystemtime() failed"));

        return false;
    }

    m_date = dt.GetDateOnly();

    return true;
}

wxDateTime wxCalendarCtrl::GetDate() const
{
#if wxDEBUG_LEVEL
    SYSTEMTIME st;

    if ( !MonthCal_GetCurSel(GetHwnd(), &st) )
    {
        wxASSERT_MSG( !m_date.IsValid(), "mismatch between data and control" );

        return wxDefaultDateTime;
    }

    wxDateTime dt;
    dt.SetFromMSWSysDate(st);

    // Windows XP and earlier didn't include the time component into the
    // returned date but Windows 7 does, so we can't compare the full objects
    // in the same way under all the Windows versions, just compare their date
    // parts
    wxASSERT_MSG( dt.IsSameDate(m_date), "mismatch between data and control" );
#endif // wxDEBUG_LEVEL

    return m_date;
}

bool wxCalendarCtrl::SetDateRange(const wxDateTime& dt1, const wxDateTime& dt2)
{
    SYSTEMTIME st[2];

    DWORD flags = 0;
    if ( dt1.IsValid() )
    {
        dt1.GetAsMSWSysTime(st + 0);
        flags |= GDTR_MIN;
    }

    if ( dt2.IsValid() )
    {
        dt2.GetAsMSWSysTime(st + 1);
        flags |= GDTR_MAX;
    }

    if ( !MonthCal_SetRange(GetHwnd(), flags, st) )
    {
        wxLogDebug(wxT("MonthCal_SetRange() failed"));
    }

    return flags != 0;
}

bool wxCalendarCtrl::GetDateRange(wxDateTime *dt1, wxDateTime *dt2) const
{
    SYSTEMTIME st[2];

    DWORD flags = MonthCal_GetRange(GetHwnd(), st);
    if ( dt1 )
    {
        if ( flags & GDTR_MIN )
            dt1->SetFromMSWSysDate(st[0]);
        else
            *dt1 = wxDefaultDateTime;
    }

    if ( dt2 )
    {
        if ( flags & GDTR_MAX )
            dt2->SetFromMSWSysDate(st[1]);
        else
            *dt2 = wxDefaultDateTime;
    }

    return flags != 0;
}

// ----------------------------------------------------------------------------
// other wxCalendarCtrl operations
// ----------------------------------------------------------------------------

bool wxCalendarCtrl::EnableMonthChange(bool enable)
{
    if ( !wxCalendarCtrlBase::EnableMonthChange(enable) )
        return false;

    wxDateTime dtStart, dtEnd;
    if ( !enable )
    {
        dtStart = GetDate();
        dtStart.SetDay(1);

        dtEnd = dtStart.GetLastMonthDay();
    }
    //else: leave them invalid to remove the restriction

    SetDateRange(dtStart, dtEnd);

    return true;
}

void wxCalendarCtrl::Mark(size_t day, bool mark)
{
    wxCHECK_RET( day > 0 && day < 32, "invalid day" );

    int mask = 1 << (day - 1);
    if ( mark )
        m_marks |= mask;
    else
        m_marks &= ~mask;

    // calling Refresh() here is not enough to change the day appearance
    UpdateMarks();
}

void wxCalendarCtrl::SetHoliday(size_t day)
{
    wxCHECK_RET( day > 0 && day < 32, "invalid day" );

    m_holidays |= 1 << (day - 1);
}

void wxCalendarCtrl::UpdateMarks()
{
    // Currently the native control may show more than one month if its size is
    // big enough. Ideal would be to prevent this from happening but there
    // doesn't seem to be any obvious way to do it, so for now just handle the
    // possibility that we can display several of them: one before the current
    // one and up to 12 after it.
    MONTHDAYSTATE states[14] = { 0 };
    const DWORD nMonths = MonthCal_GetMonthRange(GetHwnd(), GMR_DAYSTATE, NULL);

    // although in principle the calendar might not show any days from the
    // preceding months, it seems like it always does, consider e.g. Feb 2010
    // which starts on Monday and ends on Sunday and so could fit on 4 lines
    // without showing any subsequent months -- the standard control still
    // shows it on 6 lines and the number of visible months is still 3
    //
    // OTOH Windows 7 control can show all 12 months or even years or decades
    // in its window if you "zoom out" of it by double clicking on free areas
    // so the return value can be (much, in case of decades view) greater than
    // 3 but in this case marks are not visible anyhow so simply ignore it
    if ( nMonths >= 2 && nMonths <= WXSIZEOF(states) )
    {
        // The current, fully visible month is always the second one.
        states[1] = m_marks | m_holidays;

        if ( !MonthCal_SetDayState(GetHwnd(), nMonths, states) )
        {
            wxLogLastError(wxT("MonthCal_SetDayState"));
        }
    }
    //else: not a month view at all
}

void wxCalendarCtrl::UpdateFirstDayOfWeek()
{
    MonthCal_SetFirstDayOfWeek(GetHwnd(),
                               HasFlag(wxCAL_MONDAY_FIRST) ? MonthCal_Monday
                                                           : MonthCal_Sunday);
}

// ----------------------------------------------------------------------------
// wxCalendarCtrl events
// ----------------------------------------------------------------------------

bool wxCalendarCtrl::MSWOnNotify(int idCtrl, WXLPARAM lParam, WXLPARAM *result)
{
    NMHDR* hdr = (NMHDR *)lParam;
    switch ( hdr->code )
    {
        case MCN_SELCHANGE:
            {
                // we need to update m_date first, before calling the user code
                // which expects GetDate() to return the new date
                const wxDateTime dateOld = m_date;
                const NMSELCHANGE * const sch = (NMSELCHANGE *)lParam;
                m_date.SetFromMSWSysDate(sch->stSelStart);

                // changing the year or the month results in a second dummy
                // MCN_SELCHANGE event on this system which doesn't really
                // change anything -- filter it out
                if ( m_date != dateOld )
                {
                    if ( GenerateAllChangeEvents(dateOld) )
                    {
                        // month changed, need to update the holidays if we use
                        // them
                        SetHolidayAttrs();
                        UpdateMarks();
                    }
                }
            }
            break;

        case MCN_GETDAYSTATE:
            {
                const NMDAYSTATE * const ds = (NMDAYSTATE *)lParam;

                wxDateTime startDate;
                startDate.SetFromMSWSysDate(ds->stStart);

                // Ensure we have a valid date to work with.
                wxDateTime currentDate = m_date.IsValid() ? m_date : startDate;

                // Set to the start of month for comparison with startDate to
                // work correctly.
                currentDate.SetDay(1);

                for ( int i = 0; i < ds->cDayState; i++ )
                {
                    // set holiday/marks only for the "current" month
                    if ( startDate == currentDate )
                        ds->prgDayState[i] = m_marks | m_holidays;
                    else
                        ds->prgDayState[i] = 0;

                    startDate += wxDateSpan::Month();
                }
            }
            break;

        default:
            return wxCalendarCtrlBase::MSWOnNotify(idCtrl, lParam, result);
    }

    *result = 0;
    return true;
}

void wxCalendarCtrl::MSWOnDoubleClick(wxMouseEvent& event)
{
    if ( HitTest(event.GetPosition()) == wxCAL_HITTEST_DAY )
    {
        if ( GenerateEvent(wxEVT_CALENDAR_DOUBLECLICKED) )
            return; // skip event.Skip() below
    }

    event.Skip();
}

void wxCalendarCtrl::MSWOnClick(wxMouseEvent& event)
{
    // for some reason, the control doesn't get focus on its own when the user
    // clicks in it
    SetFocus();

    event.Skip();
}

#endif // wxUSE_CALENDARCTRL
