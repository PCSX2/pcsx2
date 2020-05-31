///////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/calctrlg.h
// Purpose:     generic implementation of date-picker control
// Author:      Vadim Zeitlin
// Modified by:
// Created:     29.12.99
// Copyright:   (c) 1999 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_CALCTRLG_H
#define _WX_GENERIC_CALCTRLG_H

#include "wx/control.h"         // the base class
#include "wx/dcclient.h"        // for wxPaintDC

class WXDLLIMPEXP_FWD_CORE wxComboBox;
class WXDLLIMPEXP_FWD_CORE wxStaticText;
class WXDLLIMPEXP_FWD_CORE wxSpinCtrl;
class WXDLLIMPEXP_FWD_CORE wxSpinEvent;

// ----------------------------------------------------------------------------
// wxGenericCalendarCtrl
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxGenericCalendarCtrl : public wxCalendarCtrlBase
{
public:
    // construction
    wxGenericCalendarCtrl() { Init(); }
    wxGenericCalendarCtrl(wxWindow *parent,
                          wxWindowID id,
                          const wxDateTime& date = wxDefaultDateTime,
                          const wxPoint& pos = wxDefaultPosition,
                          const wxSize& size = wxDefaultSize,
                          long style = wxCAL_SHOW_HOLIDAYS,
                          const wxString& name = wxCalendarNameStr);

    bool Create(wxWindow *parent,
                wxWindowID id,
                const wxDateTime& date = wxDefaultDateTime,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxCAL_SHOW_HOLIDAYS,
                const wxString& name = wxCalendarNameStr);

    virtual ~wxGenericCalendarCtrl();

    virtual bool Destroy();

    // set/get the current date
    // ------------------------

    virtual bool SetDate(const wxDateTime& date);
    virtual wxDateTime GetDate() const { return m_date; }


    // set/get the range in which selection can occur
    // ---------------------------------------------

    virtual bool SetDateRange(const wxDateTime& lowerdate = wxDefaultDateTime,
                              const wxDateTime& upperdate = wxDefaultDateTime);

    virtual bool GetDateRange(wxDateTime *lowerdate, wxDateTime *upperdate) const;

    // these functions are for generic version only, don't use them but use the
    // Set/GetDateRange() above instead
    bool SetLowerDateLimit(const wxDateTime& date = wxDefaultDateTime);
    const wxDateTime& GetLowerDateLimit() const { return m_lowdate; }
    bool SetUpperDateLimit(const wxDateTime& date = wxDefaultDateTime);
    const wxDateTime& GetUpperDateLimit() const { return m_highdate; }


    // calendar mode
    // -------------

    // some calendar styles can't be changed after the control creation by
    // just using SetWindowStyle() and Refresh() and the functions below
    // should be used instead for them

    // corresponds to wxCAL_NO_MONTH_CHANGE bit
    virtual bool EnableMonthChange(bool enable = true);

    // corresponds to wxCAL_NO_YEAR_CHANGE bit, deprecated, generic only
    void EnableYearChange(bool enable = true);


    // customization
    // -------------

    virtual void Mark(size_t day, bool mark);

    // all other functions in this section are for generic version only

    // header colours are used for painting the weekdays at the top
    virtual void SetHeaderColours(const wxColour& colFg, const wxColour& colBg)
    {
        m_colHeaderFg = colFg;
        m_colHeaderBg = colBg;
    }

    virtual const wxColour& GetHeaderColourFg() const { return m_colHeaderFg; }
    virtual const wxColour& GetHeaderColourBg() const { return m_colHeaderBg; }

    // highlight colour is used for the currently selected date
    virtual void SetHighlightColours(const wxColour& colFg, const wxColour& colBg)
    {
        m_colHighlightFg = colFg;
        m_colHighlightBg = colBg;
    }

    virtual const wxColour& GetHighlightColourFg() const { return m_colHighlightFg; }
    virtual const wxColour& GetHighlightColourBg() const { return m_colHighlightBg; }

    // holiday colour is used for the holidays (if style & wxCAL_SHOW_HOLIDAYS)
    virtual void SetHolidayColours(const wxColour& colFg, const wxColour& colBg)
    {
        m_colHolidayFg = colFg;
        m_colHolidayBg = colBg;
    }

    virtual const wxColour& GetHolidayColourFg() const { return m_colHolidayFg; }
    virtual const wxColour& GetHolidayColourBg() const { return m_colHolidayBg; }

    virtual wxCalendarDateAttr *GetAttr(size_t day) const
    {
        wxCHECK_MSG( day > 0 && day < 32, NULL, wxT("invalid day") );

        return m_attrs[day - 1];
    }

    virtual void SetAttr(size_t day, wxCalendarDateAttr *attr)
    {
        wxCHECK_RET( day > 0 && day < 32, wxT("invalid day") );

        delete m_attrs[day - 1];
        m_attrs[day - 1] = attr;
    }

    virtual void ResetAttr(size_t day) { SetAttr(day, NULL); }

    virtual void SetHoliday(size_t day);

    virtual wxCalendarHitTestResult HitTest(const wxPoint& pos,
                                            wxDateTime *date = NULL,
                                            wxDateTime::WeekDay *wd = NULL);

    // implementation only from now on
    // -------------------------------

    // forward these functions to all subcontrols
    virtual bool Enable(bool enable = true);
    virtual bool Show(bool show = true);

    virtual void SetWindowStyleFlag(long style);

    virtual wxVisualAttributes GetDefaultAttributes() const
        { return GetClassDefaultAttributes(GetWindowVariant()); }

    static wxVisualAttributes
    GetClassDefaultAttributes(wxWindowVariant variant = wxWINDOW_VARIANT_NORMAL);

    void OnSysColourChanged(wxSysColourChangedEvent& event);

protected:
    // override some base class virtuals
    virtual wxSize DoGetBestSize() const;
    virtual void DoMoveWindow(int x, int y, int width, int height);
    virtual void DoGetSize(int *width, int *height) const;

private:
    // common part of all ctors
    void Init();

    // startup colours and reinitialization after colour changes in system
    void InitColours();

    // event handlers
    void OnPaint(wxPaintEvent& event);
    void OnClick(wxMouseEvent& event);
    void OnDClick(wxMouseEvent& event);
    void OnWheel(wxMouseEvent& event);
    void OnChar(wxKeyEvent& event);
    void OnMonthChange(wxCommandEvent& event);

    void HandleYearChange(wxCommandEvent& event);
    void OnYearChange(wxSpinEvent& event);
    void OnYearTextChange(wxCommandEvent& event);

    // (re)calc m_widthCol and m_heightRow
    void RecalcGeometry();

    // set the date and send the notification
    void SetDateAndNotify(const wxDateTime& date);

    // get the week (row, in range 1..6) for the given date
    size_t GetWeek(const wxDateTime& date) const;

    // get the date from which we start drawing days
    wxDateTime GetStartDate() const;

    // get the first/last days of the week corresponding to the current style
    wxDateTime::WeekDay GetWeekStart() const
    {
        return HasFlag(wxCAL_MONDAY_FIRST) ? wxDateTime::Mon
                                           : wxDateTime::Sun;
    }

    wxDateTime::WeekDay GetWeekEnd() const
    {
        return HasFlag(wxCAL_MONDAY_FIRST) ? wxDateTime::Sun
                                           : wxDateTime::Sat;
    }


    // is this date shown?
    bool IsDateShown(const wxDateTime& date) const;

    // is this date in the currently allowed range?
    bool IsDateInRange(const wxDateTime& date) const;

    // adjust the date to the currently allowed range, return true if it was
    // changed
    bool AdjustDateToRange(wxDateTime *date) const;

    // redraw the given date
    void RefreshDate(const wxDateTime& date);

    // change the date inside the same month/year
    void ChangeDay(const wxDateTime& date);

    // deprecated
    bool AllowYearChange() const
    {
        return !(GetWindowStyle() & wxCAL_NO_YEAR_CHANGE);
    }

    // show the correct controls
    void ShowCurrentControls();

    // create the month combo and year spin controls
    void CreateMonthComboBox();
    void CreateYearSpinCtrl();

public:
    // get the currently shown control for month/year
    wxControl *GetMonthControl() const;
    wxControl *GetYearControl() const;

private:
    virtual void ResetHolidayAttrs();
    virtual void RefreshHolidays() { Refresh(); }

    // OnPaint helper-methods

    // Highlight the [fromdate : todate] range using pen and brush
    void HighlightRange(wxPaintDC* dc, const wxDateTime& fromdate, const wxDateTime& todate, const wxPen* pen, const wxBrush* brush);

    // Get the "coordinates" for the date relative to the month currently displayed.
    // using (day, week): upper left coord is (1, 1), lower right coord is (7, 6)
    // if the date isn't visible (-1, -1) is put in (day, week) and false is returned
    bool GetDateCoord(const wxDateTime& date, int *day, int *week) const;

    // Set the flag for SetDate(): otherwise it would overwrite the year
    // typed in by the user
    void SetUserChangedYear() { m_userChangedYear = true; }


    // the subcontrols
    wxStaticText *m_staticMonth;
    wxComboBox *m_comboMonth;

    wxStaticText *m_staticYear;
    wxSpinCtrl *m_spinYear;

    // the current selection
    wxDateTime m_date;

    // the date-range
    wxDateTime m_lowdate;
    wxDateTime m_highdate;

    // default attributes
    wxColour m_colHighlightFg,
             m_colHighlightBg,
             m_colHolidayFg,
             m_colHolidayBg,
             m_colHeaderFg,
             m_colHeaderBg,
             m_colBackground,
             m_colSurrounding;

    // the attributes for each of the month days
    wxCalendarDateAttr *m_attrs[31];

    // the width and height of one column/row in the calendar
    wxCoord m_widthCol,
            m_heightRow,
            m_rowOffset,
            m_calendarWeekWidth;

    wxRect m_leftArrowRect,
           m_rightArrowRect;

    // the week day names
    wxString m_weekdays[7];

    // true if SetDate() is being called as the result of changing the year in
    // the year control
    bool m_userChangedYear;

    DECLARE_DYNAMIC_CLASS(wxGenericCalendarCtrl)
    DECLARE_EVENT_TABLE()
    wxDECLARE_NO_COPY_CLASS(wxGenericCalendarCtrl);
};

#endif // _WX_GENERIC_CALCTRLG_H
