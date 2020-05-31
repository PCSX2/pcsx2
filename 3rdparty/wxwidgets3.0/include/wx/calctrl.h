///////////////////////////////////////////////////////////////////////////////
// Name:        wx/calctrl.h
// Purpose:     date-picker control
// Author:      Vadim Zeitlin
// Modified by:
// Created:     29.12.99
// Copyright:   (c) 1999 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CALCTRL_H_
#define _WX_CALCTRL_H_

#include "wx/defs.h"

#if wxUSE_CALENDARCTRL

#include "wx/dateevt.h"
#include "wx/colour.h"
#include "wx/font.h"
#include "wx/control.h"

// ----------------------------------------------------------------------------
// wxCalendarCtrl flags
// ----------------------------------------------------------------------------

enum
{
    // show Sunday as the first day of the week (default)
    wxCAL_SUNDAY_FIRST               = 0x0000,

    // show Monday as the first day of the week
    wxCAL_MONDAY_FIRST               = 0x0001,

    // highlight holidays
    wxCAL_SHOW_HOLIDAYS              = 0x0002,

    // disable the year change control, show only the month change one
    // deprecated
    wxCAL_NO_YEAR_CHANGE             = 0x0004,

    // don't allow changing neither month nor year (implies
    // wxCAL_NO_YEAR_CHANGE)
    wxCAL_NO_MONTH_CHANGE            = 0x000c,

    // use MS-style month-selection instead of combo-spin combination
    wxCAL_SEQUENTIAL_MONTH_SELECTION = 0x0010,

    // show the neighbouring weeks in the previous and next month
    wxCAL_SHOW_SURROUNDING_WEEKS     = 0x0020,

    // show week numbers on the left side of the calendar.
    wxCAL_SHOW_WEEK_NUMBERS          = 0x0040
};

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// return values for the HitTest() method
enum wxCalendarHitTestResult
{
    wxCAL_HITTEST_NOWHERE,      // outside of anything
    wxCAL_HITTEST_HEADER,       // on the header (weekdays)
    wxCAL_HITTEST_DAY,          // on a day in the calendar
    wxCAL_HITTEST_INCMONTH,
    wxCAL_HITTEST_DECMONTH,
    wxCAL_HITTEST_SURROUNDING_WEEK,
    wxCAL_HITTEST_WEEK
};

// border types for a date
enum wxCalendarDateBorder
{
    wxCAL_BORDER_NONE,          // no border (default)
    wxCAL_BORDER_SQUARE,        // a rectangular border
    wxCAL_BORDER_ROUND          // a round border
};

// ----------------------------------------------------------------------------
// wxCalendarDateAttr: custom attributes for a calendar date
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxCalendarDateAttr
{
public:
    // ctors
    wxCalendarDateAttr(const wxColour& colText = wxNullColour,
                       const wxColour& colBack = wxNullColour,
                       const wxColour& colBorder = wxNullColour,
                       const wxFont& font = wxNullFont,
                       wxCalendarDateBorder border = wxCAL_BORDER_NONE)
        : m_colText(colText), m_colBack(colBack),
          m_colBorder(colBorder), m_font(font)
    {
        Init(border);
    }
    wxCalendarDateAttr(wxCalendarDateBorder border,
                       const wxColour& colBorder = wxNullColour)
        : m_colBorder(colBorder)
    {
        Init(border);
    }

    // setters
    void SetTextColour(const wxColour& colText) { m_colText = colText; }
    void SetBackgroundColour(const wxColour& colBack) { m_colBack = colBack; }
    void SetBorderColour(const wxColour& col) { m_colBorder = col; }
    void SetFont(const wxFont& font) { m_font = font; }
    void SetBorder(wxCalendarDateBorder border) { m_border = border; }
    void SetHoliday(bool holiday) { m_holiday = holiday; }

    // accessors
    bool HasTextColour() const { return m_colText.IsOk(); }
    bool HasBackgroundColour() const { return m_colBack.IsOk(); }
    bool HasBorderColour() const { return m_colBorder.IsOk(); }
    bool HasFont() const { return m_font.IsOk(); }
    bool HasBorder() const { return m_border != wxCAL_BORDER_NONE; }

    bool IsHoliday() const { return m_holiday; }

    const wxColour& GetTextColour() const { return m_colText; }
    const wxColour& GetBackgroundColour() const { return m_colBack; }
    const wxColour& GetBorderColour() const { return m_colBorder; }
    const wxFont& GetFont() const { return m_font; }
    wxCalendarDateBorder GetBorder() const { return m_border; }

    // get or change the "mark" attribute, i.e. the one used for the items
    // marked with wxCalendarCtrl::Mark()
    static const wxCalendarDateAttr& GetMark() { return m_mark; }
    static void SetMark(wxCalendarDateAttr const& m) { m_mark = m; }

protected:
    void Init(wxCalendarDateBorder border = wxCAL_BORDER_NONE)
    {
        m_border = border;
        m_holiday = false;
    }

private:
    static wxCalendarDateAttr m_mark;

    wxColour m_colText,
             m_colBack,
             m_colBorder;
    wxFont   m_font;
    wxCalendarDateBorder m_border;
    bool m_holiday;
};

// ----------------------------------------------------------------------------
// wxCalendarCtrl events
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_ADV wxCalendarCtrl;

class WXDLLIMPEXP_ADV wxCalendarEvent : public wxDateEvent
{
public:
    wxCalendarEvent() : m_wday(wxDateTime::Inv_WeekDay)  { }
    wxCalendarEvent(wxWindow *win, const wxDateTime& dt, wxEventType type)
        : wxDateEvent(win, dt, type),
          m_wday(wxDateTime::Inv_WeekDay) { }
    wxCalendarEvent(const wxCalendarEvent& event)
        : wxDateEvent(event), m_wday(event.m_wday) { }

    void SetWeekDay(const wxDateTime::WeekDay wd) { m_wday = wd; }
    wxDateTime::WeekDay GetWeekDay() const { return m_wday; }

    virtual wxEvent *Clone() const { return new wxCalendarEvent(*this); }

private:
    wxDateTime::WeekDay m_wday;

    DECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxCalendarEvent)
};

// ----------------------------------------------------------------------------
// wxCalendarCtrlBase
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxCalendarCtrlBase : public wxControl
{
public:
    // do we allow changing the month/year?
    bool AllowMonthChange() const { return !HasFlag(wxCAL_NO_MONTH_CHANGE); }

    // get/set the current date
    virtual wxDateTime GetDate() const = 0;
    virtual bool SetDate(const wxDateTime& date) = 0;


    // restricting the dates shown by the control to the specified range: only
    // implemented in the generic and MSW versions for now

    // if either date is set, the corresponding limit will be enforced and true
    // returned; if none are set, the existing restrictions are removed and
    // false is returned
    virtual bool
    SetDateRange(const wxDateTime& WXUNUSED(lowerdate) = wxDefaultDateTime,
                 const wxDateTime& WXUNUSED(upperdate) = wxDefaultDateTime)
    {
        return false;
    }

    // retrieves the limits currently in use (wxDefaultDateTime if none) in the
    // provided pointers (which may be NULL) and returns true if there are any
    // limits or false if none
    virtual bool
    GetDateRange(wxDateTime *lowerdate, wxDateTime *upperdate) const
    {
        if ( lowerdate )
            *lowerdate = wxDefaultDateTime;
        if ( upperdate )
            *upperdate = wxDefaultDateTime;
        return false;
    }

    // returns one of wxCAL_HITTEST_XXX constants and fills either date or wd
    // with the corresponding value (none for NOWHERE, the date for DAY and wd
    // for HEADER)
    //
    // notice that this is not implemented in all versions
    virtual wxCalendarHitTestResult
    HitTest(const wxPoint& WXUNUSED(pos),
            wxDateTime* WXUNUSED(date) = NULL,
            wxDateTime::WeekDay* WXUNUSED(wd) = NULL)
    {
        return wxCAL_HITTEST_NOWHERE;
    }

    // allow or disable changing the current month (and year), return true if
    // the value of this option really changed or false if it was already set
    // to the required value
    //
    // NB: we provide implementation for this pure virtual function, derived
    //     classes should call it
    virtual bool EnableMonthChange(bool enable = true) = 0;


    // an item without custom attributes is drawn with the default colours and
    // font and without border, setting custom attributes allows to modify this
    //
    // the day parameter should be in 1..31 range, for days 29, 30, 31 the
    // corresponding attribute is just unused if there is no such day in the
    // current month
    //
    // notice that currently arbitrary attributes are supported only in the
    // generic version, the native controls only support Mark() which assigns
    // some special appearance (which can be customized using SetMark() for the
    // generic version) to the given day

    virtual void Mark(size_t day, bool mark) = 0;

    virtual wxCalendarDateAttr *GetAttr(size_t WXUNUSED(day)) const
        { return NULL; }
    virtual void SetAttr(size_t WXUNUSED(day), wxCalendarDateAttr *attr)
        { delete attr; }
    virtual void ResetAttr(size_t WXUNUSED(day)) { }


    // holidays support
    //
    // currently only the generic version implements all functions in this
    // section; wxMSW implements simple support for holidays (they can be
    // just enabled or disabled) and wxGTK doesn't support them at all

    // equivalent to changing wxCAL_SHOW_HOLIDAYS flag but should be called
    // instead of just changing it
    virtual void EnableHolidayDisplay(bool display = true);

    // set/get the colours to use for holidays (if they're enabled)
    virtual void SetHolidayColours(const wxColour& WXUNUSED(colFg),
                                   const wxColour& WXUNUSED(colBg)) { }

    virtual const wxColour& GetHolidayColourFg() const { return wxNullColour; }
    virtual const wxColour& GetHolidayColourBg() const { return wxNullColour; }

    // mark the given day of the current month as being a holiday
    virtual void SetHoliday(size_t WXUNUSED(day)) { }


    // customizing the colours of the controls
    //
    // most of the methods in this section are only implemented by the native
    // version of the control and do nothing in the native ones

    // set/get the colours to use for the display of the week day names at the
    // top of the controls
    virtual void SetHeaderColours(const wxColour& WXUNUSED(colFg),
                                  const wxColour& WXUNUSED(colBg)) { }

    virtual const wxColour& GetHeaderColourFg() const { return wxNullColour; }
    virtual const wxColour& GetHeaderColourBg() const { return wxNullColour; }

    // set/get the colours used for the currently selected date
    virtual void SetHighlightColours(const wxColour& WXUNUSED(colFg),
                                     const wxColour& WXUNUSED(colBg)) { }

    virtual const wxColour& GetHighlightColourFg() const { return wxNullColour; }
    virtual const wxColour& GetHighlightColourBg() const { return wxNullColour; }


    // implementation only from now on

    // generate the given calendar event, return true if it was processed
    //
    // NB: this is public because it's used from GTK+ callbacks
    bool GenerateEvent(wxEventType type)
    {
        wxCalendarEvent event(this, GetDate(), type);
        return HandleWindowEvent(event);
    }

protected:
    // generate all the events for the selection change from dateOld to current
    // date: SEL_CHANGED, PAGE_CHANGED if necessary and also one of (deprecated)
    // YEAR/MONTH/DAY_CHANGED ones
    //
    // returns true if page changed event was generated, false if the new date
    // is still in the same month as before
    bool GenerateAllChangeEvents(const wxDateTime& dateOld);

    // call SetHoliday() for all holidays in the current month
    //
    // should be called on month change, does nothing if wxCAL_SHOW_HOLIDAYS is
    // not set and returns false in this case, true if we do show them
    bool SetHolidayAttrs();

    // called by SetHolidayAttrs() to forget the previously set holidays
    virtual void ResetHolidayAttrs() { }

    // called by EnableHolidayDisplay()
    virtual void RefreshHolidays() { }
};

// ----------------------------------------------------------------------------
// wxCalendarCtrl
// ----------------------------------------------------------------------------

#define wxCalendarNameStr "CalendarCtrl"

#ifndef __WXUNIVERSAL__
    #if defined(__WXGTK20__)
        #define wxHAS_NATIVE_CALENDARCTRL
        #include "wx/gtk/calctrl.h"
        #define wxCalendarCtrl wxGtkCalendarCtrl
    #elif defined(__WXMSW__)
        #define wxHAS_NATIVE_CALENDARCTRL
        #include "wx/msw/calctrl.h"
    #endif
#endif // !__WXUNIVERSAL__

#ifndef wxHAS_NATIVE_CALENDARCTRL
    #include "wx/generic/calctrlg.h"
    #define wxCalendarCtrl wxGenericCalendarCtrl
#endif

// ----------------------------------------------------------------------------
// calendar event types and macros for handling them
// ----------------------------------------------------------------------------

wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_CALENDAR_SEL_CHANGED, wxCalendarEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_CALENDAR_PAGE_CHANGED, wxCalendarEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_CALENDAR_DOUBLECLICKED, wxCalendarEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_CALENDAR_WEEKDAY_CLICKED, wxCalendarEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_CALENDAR_WEEK_CLICKED, wxCalendarEvent );

// deprecated events
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_CALENDAR_DAY_CHANGED, wxCalendarEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_CALENDAR_MONTH_CHANGED, wxCalendarEvent );
wxDECLARE_EXPORTED_EVENT( WXDLLIMPEXP_ADV, wxEVT_CALENDAR_YEAR_CHANGED, wxCalendarEvent );

typedef void (wxEvtHandler::*wxCalendarEventFunction)(wxCalendarEvent&);

#define wxCalendarEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxCalendarEventFunction, func)

#define wx__DECLARE_CALEVT(evt, id, fn) \
    wx__DECLARE_EVT1(wxEVT_CALENDAR_ ## evt, id, wxCalendarEventHandler(fn))

#define EVT_CALENDAR(id, fn) wx__DECLARE_CALEVT(DOUBLECLICKED, id, fn)
#define EVT_CALENDAR_SEL_CHANGED(id, fn) wx__DECLARE_CALEVT(SEL_CHANGED, id, fn)
#define EVT_CALENDAR_PAGE_CHANGED(id, fn) wx__DECLARE_CALEVT(PAGE_CHANGED, id, fn)
#define EVT_CALENDAR_WEEKDAY_CLICKED(id, fn) wx__DECLARE_CALEVT(WEEKDAY_CLICKED, id, fn)
#define EVT_CALENDAR_WEEK_CLICKED(id, fn) wx__DECLARE_CALEVT(WEEK_CLICKED, id, fn)

// deprecated events
#define EVT_CALENDAR_DAY(id, fn) wx__DECLARE_CALEVT(DAY_CHANGED, id, fn)
#define EVT_CALENDAR_MONTH(id, fn) wx__DECLARE_CALEVT(MONTH_CHANGED, id, fn)
#define EVT_CALENDAR_YEAR(id, fn) wx__DECLARE_CALEVT(YEAR_CHANGED, id, fn)

#endif // wxUSE_CALENDARCTRL

#endif // _WX_CALCTRL_H_

