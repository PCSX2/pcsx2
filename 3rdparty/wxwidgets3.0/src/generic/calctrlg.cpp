///////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/calctrlg.cpp
// Purpose:     implementation of the wxGenericCalendarCtrl
// Author:      Vadim Zeitlin
// Modified by:
// Created:     29.12.99
// Copyright:   (c) 1999 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

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
    #include "wx/dcclient.h"
    #include "wx/settings.h"
    #include "wx/brush.h"
    #include "wx/combobox.h"
    #include "wx/listbox.h"
    #include "wx/stattext.h"
    #include "wx/textctrl.h"
#endif //WX_PRECOMP


#if wxUSE_CALENDARCTRL

#include "wx/spinctrl.h"
#include "wx/calctrl.h"
#include "wx/generic/calctrlg.h"

#define DEBUG_PAINT 0

// ----------------------------------------------------------------------------
// wxWin macros
// ----------------------------------------------------------------------------

#ifdef wxHAS_NATIVE_CALENDARCTRL

wxIMPLEMENT_DYNAMIC_CLASS_XTI(wxGenericCalendarCtrl, wxControl,"wx/calctrl.h")

#endif

BEGIN_EVENT_TABLE(wxGenericCalendarCtrl, wxControl)
    EVT_PAINT(wxGenericCalendarCtrl::OnPaint)

    EVT_CHAR(wxGenericCalendarCtrl::OnChar)

    EVT_LEFT_DOWN(wxGenericCalendarCtrl::OnClick)
    EVT_LEFT_DCLICK(wxGenericCalendarCtrl::OnDClick)
    EVT_MOUSEWHEEL(wxGenericCalendarCtrl::OnWheel)

    EVT_SYS_COLOUR_CHANGED(wxGenericCalendarCtrl::OnSysColourChanged)
END_EVENT_TABLE()

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// helper functions
// ----------------------------------------------------------------------------

namespace
{

// add attributes that are set in attr
void AddAttr(wxCalendarDateAttr *self, const wxCalendarDateAttr& attr)
{
    if (attr.HasTextColour())
        self->SetTextColour(attr.GetTextColour());
    if (attr.HasBackgroundColour())
        self->SetBackgroundColour(attr.GetBackgroundColour());
    if (attr.HasBorderColour())
        self->SetBorderColour(attr.GetBorderColour());
    if (attr.HasFont())
        self->SetFont(attr.GetFont());
    if (attr.HasBorder())
        self->SetBorder(attr.GetBorder());
    if (attr.IsHoliday())
        self->SetHoliday(true);
}

// remove attributes that are set in attr
void DelAttr(wxCalendarDateAttr *self, const wxCalendarDateAttr &attr)
{
    if (attr.HasTextColour())
        self->SetTextColour(wxNullColour);
    if (attr.HasBackgroundColour())
        self->SetBackgroundColour(wxNullColour);
    if (attr.HasBorderColour())
        self->SetBorderColour(wxNullColour);
    if (attr.HasFont())
        self->SetFont(wxNullFont);
    if (attr.HasBorder())
        self->SetBorder(wxCAL_BORDER_NONE);
    if (attr.IsHoliday())
        self->SetHoliday(false);
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// wxGenericCalendarCtrl
// ----------------------------------------------------------------------------

wxGenericCalendarCtrl::wxGenericCalendarCtrl(wxWindow *parent,
                                             wxWindowID id,
                                             const wxDateTime& date,
                                             const wxPoint& pos,
                                             const wxSize& size,
                                             long style,
                                             const wxString& name)
{
    Init();

    (void)Create(parent, id, date, pos, size, style, name);
}

void wxGenericCalendarCtrl::Init()
{
    m_comboMonth = NULL;
    m_spinYear = NULL;
    m_staticYear = NULL;
    m_staticMonth = NULL;

    m_userChangedYear = false;

    m_widthCol =
    m_heightRow =
    m_calendarWeekWidth = 0;

    wxDateTime::WeekDay wd;
    for ( wd = wxDateTime::Sun; wd < wxDateTime::Inv_WeekDay; wxNextWDay(wd) )
    {
        m_weekdays[wd] = wxDateTime::GetWeekDayName(wd, wxDateTime::Name_Abbr);
    }

    for ( size_t n = 0; n < WXSIZEOF(m_attrs); n++ )
    {
        m_attrs[n] = NULL;
    }

    InitColours();
}

void wxGenericCalendarCtrl::InitColours()
{
    m_colHighlightFg = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT);
    m_colHighlightBg = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT);
    m_colBackground = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    m_colSurrounding = wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);

    m_colHolidayFg = *wxRED;
    // don't set m_colHolidayBg - by default, same as our bg colour

    m_colHeaderFg = *wxBLUE;
    m_colHeaderBg = *wxLIGHT_GREY;
}

bool wxGenericCalendarCtrl::Create(wxWindow *parent,
                                   wxWindowID id,
                                   const wxDateTime& date,
                                   const wxPoint& pos,
                                   const wxSize& size,
                                   long style,
                                   const wxString& name)
{
    if ( !wxControl::Create(parent, id, pos, size,
                            style | wxCLIP_CHILDREN | wxWANTS_CHARS | wxFULL_REPAINT_ON_RESIZE,
                            wxDefaultValidator, name) )
    {
        return false;
    }

    // needed to get the arrow keys normally used for the dialog navigation
    SetWindowStyle(style | wxWANTS_CHARS);

    m_date = date.IsValid() ? date : wxDateTime::Today();

    m_lowdate = wxDefaultDateTime;
    m_highdate = wxDefaultDateTime;

    if ( !HasFlag(wxCAL_SEQUENTIAL_MONTH_SELECTION) )
    {
        CreateYearSpinCtrl();
        m_staticYear = new wxStaticText(GetParent(), wxID_ANY, m_date.Format(wxT("%Y")),
                                        wxDefaultPosition, wxDefaultSize,
                                        wxALIGN_CENTRE);
        CreateMonthComboBox();
        m_staticMonth = new wxStaticText(GetParent(), wxID_ANY, m_date.Format(wxT("%B")),
                                         wxDefaultPosition, wxDefaultSize,
                                         wxALIGN_CENTRE);
    }

    ShowCurrentControls();

    // we need to set the position as well because the main control position
    // is not the same as the one specified in pos if we have the controls
    // above it
    SetInitialSize(size);
    SetPosition(pos);

    // Since we don't paint the whole background make sure that the platform
    // will use the right one.
    SetBackgroundColour(m_colBackground);

    SetHolidayAttrs();

    return true;
}

wxGenericCalendarCtrl::~wxGenericCalendarCtrl()
{
    for ( size_t n = 0; n < WXSIZEOF(m_attrs); n++ )
    {
        delete m_attrs[n];
    }

    if ( !HasFlag(wxCAL_SEQUENTIAL_MONTH_SELECTION) )
    {
        delete m_comboMonth;
        delete m_staticMonth;
        delete m_spinYear;
        delete m_staticYear;
    }
}

void wxGenericCalendarCtrl::SetWindowStyleFlag(long style)
{
    // changing this style doesn't work because the controls are not
    // created/shown/hidden accordingly
    wxASSERT_MSG( (style & wxCAL_SEQUENTIAL_MONTH_SELECTION) ==
                    (m_windowStyle & wxCAL_SEQUENTIAL_MONTH_SELECTION),
                  wxT("wxCAL_SEQUENTIAL_MONTH_SELECTION can't be changed after creation") );

    wxControl::SetWindowStyleFlag(style);
}

// ----------------------------------------------------------------------------
// Create the wxComboBox and wxSpinCtrl
// ----------------------------------------------------------------------------

void wxGenericCalendarCtrl::CreateMonthComboBox()
{
    m_comboMonth = new wxComboBox(GetParent(), wxID_ANY,
                                  wxEmptyString,
                                  wxDefaultPosition,
                                  wxDefaultSize,
                                  0, NULL,
                                  wxCB_READONLY | wxCLIP_SIBLINGS);

    wxDateTime::Month m;
    for ( m = wxDateTime::Jan; m < wxDateTime::Inv_Month; wxNextMonth(m) )
    {
        m_comboMonth->Append(wxDateTime::GetMonthName(m));
    }

    m_comboMonth->SetSelection(GetDate().GetMonth());
    m_comboMonth->SetSize(wxDefaultCoord,
                          wxDefaultCoord,
                          wxDefaultCoord,
                          wxDefaultCoord,
                          wxSIZE_AUTO_WIDTH|wxSIZE_AUTO_HEIGHT);

    m_comboMonth->Connect(m_comboMonth->GetId(), wxEVT_COMBOBOX,
                          wxCommandEventHandler(wxGenericCalendarCtrl::OnMonthChange),
                          NULL, this);
}

void wxGenericCalendarCtrl::CreateYearSpinCtrl()
{
    m_spinYear = new wxSpinCtrl(GetParent(), wxID_ANY,
                                GetDate().Format(wxT("%Y")),
                                wxDefaultPosition,
                                wxDefaultSize,
                                wxSP_ARROW_KEYS | wxCLIP_SIBLINGS,
                                -4300, 10000, GetDate().GetYear());

    m_spinYear->Connect(m_spinYear->GetId(), wxEVT_TEXT,
                        wxCommandEventHandler(wxGenericCalendarCtrl::OnYearTextChange),
                        NULL, this);

    m_spinYear->Connect(m_spinYear->GetId(), wxEVT_SPINCTRL,
                        wxSpinEventHandler(wxGenericCalendarCtrl::OnYearChange),
                        NULL, this);
}

// ----------------------------------------------------------------------------
// forward wxWin functions to subcontrols
// ----------------------------------------------------------------------------

bool wxGenericCalendarCtrl::Destroy()
{
    if ( m_staticYear )
        m_staticYear->Destroy();
    if ( m_spinYear )
        m_spinYear->Destroy();
    if ( m_comboMonth )
        m_comboMonth->Destroy();
    if ( m_staticMonth )
        m_staticMonth->Destroy();

    m_staticYear = NULL;
    m_spinYear = NULL;
    m_comboMonth = NULL;
    m_staticMonth = NULL;

    return wxControl::Destroy();
}

bool wxGenericCalendarCtrl::Show(bool show)
{
    if ( !wxControl::Show(show) )
    {
        return false;
    }

    if ( !(GetWindowStyle() & wxCAL_SEQUENTIAL_MONTH_SELECTION) )
    {
        if ( GetMonthControl() )
        {
            GetMonthControl()->Show(show);
            GetYearControl()->Show(show);
        }
    }

    return true;
}

bool wxGenericCalendarCtrl::Enable(bool enable)
{
    if ( !wxControl::Enable(enable) )
    {
        return false;
    }

    if ( !(GetWindowStyle() & wxCAL_SEQUENTIAL_MONTH_SELECTION) )
    {
        GetMonthControl()->Enable(enable);
        GetYearControl()->Enable(enable);
    }

    return true;
}

// ----------------------------------------------------------------------------
// enable/disable month/year controls
// ----------------------------------------------------------------------------

void wxGenericCalendarCtrl::ShowCurrentControls()
{
    if ( !HasFlag(wxCAL_SEQUENTIAL_MONTH_SELECTION) )
    {
        if ( AllowMonthChange() )
        {
            m_comboMonth->Show();
            m_staticMonth->Hide();

            if ( AllowYearChange() )
            {
                m_spinYear->Show();
                m_staticYear->Hide();

                // skip the rest
                return;
            }
        }
        else
        {
            m_comboMonth->Hide();
            m_staticMonth->Show();
        }

        // year change not allowed here
        m_spinYear->Hide();
        m_staticYear->Show();
    }
    //else: these controls are not even created, don't show/hide them
}

wxControl *wxGenericCalendarCtrl::GetMonthControl() const
{
    return AllowMonthChange() ? (wxControl *)m_comboMonth : (wxControl *)m_staticMonth;
}

wxControl *wxGenericCalendarCtrl::GetYearControl() const
{
    return AllowYearChange() ? (wxControl *)m_spinYear : (wxControl *)m_staticYear;
}

void wxGenericCalendarCtrl::EnableYearChange(bool enable)
{
    if ( enable != AllowYearChange() )
    {
        long style = GetWindowStyle();
        if ( enable )
            style &= ~wxCAL_NO_YEAR_CHANGE;
        else
            style |= wxCAL_NO_YEAR_CHANGE;
        SetWindowStyle(style);

        ShowCurrentControls();
        if ( GetWindowStyle() & wxCAL_SEQUENTIAL_MONTH_SELECTION )
        {
            Refresh();
        }
    }
}

bool wxGenericCalendarCtrl::EnableMonthChange(bool enable)
{
    if ( !wxCalendarCtrlBase::EnableMonthChange(enable) )
        return false;

    ShowCurrentControls();
    if ( GetWindowStyle() & wxCAL_SEQUENTIAL_MONTH_SELECTION )
        Refresh();

    return true;
}

// ----------------------------------------------------------------------------
// changing date
// ----------------------------------------------------------------------------

bool wxGenericCalendarCtrl::SetDate(const wxDateTime& date)
{
    bool retval = true;

    bool sameMonth = m_date.GetMonth() == date.GetMonth(),
         sameYear = m_date.GetYear() == date.GetYear();

    if ( IsDateInRange(date) )
    {
        if ( sameMonth && sameYear )
        {
            // just change the day
            ChangeDay(date);
        }
        else
        {
            if ( AllowMonthChange() && (AllowYearChange() || sameYear) )
            {
                // change everything
                m_date = date;

                if ( !(GetWindowStyle() & wxCAL_SEQUENTIAL_MONTH_SELECTION) )
                {
                    // update the controls
                    m_comboMonth->SetSelection(m_date.GetMonth());

                    if ( AllowYearChange() )
                    {
                        if ( !m_userChangedYear )
                            m_spinYear->SetValue(m_date.Format(wxT("%Y")));
                    }
                }

                // as the month changed, holidays did too
                SetHolidayAttrs();

                // update the calendar
                Refresh();
            }
            else
            {
                // forbidden
                retval = false;
            }
        }
    }

    m_userChangedYear = false;

    return retval;
}

void wxGenericCalendarCtrl::ChangeDay(const wxDateTime& date)
{
    if ( m_date != date )
    {
        // we need to refresh the row containing the old date and the one
        // containing the new one
        wxDateTime dateOld = m_date;
        m_date = date;

        RefreshDate(dateOld);

        // if the date is in the same row, it was already drawn correctly
        if ( GetWeek(m_date) != GetWeek(dateOld) )
        {
            RefreshDate(m_date);
        }
    }
}

void wxGenericCalendarCtrl::SetDateAndNotify(const wxDateTime& date)
{
    const wxDateTime dateOld = GetDate();
    if ( date != dateOld && SetDate(date) )
    {
        GenerateAllChangeEvents(dateOld);
    }
}

// ----------------------------------------------------------------------------
// date range
// ----------------------------------------------------------------------------

bool wxGenericCalendarCtrl::SetLowerDateLimit(const wxDateTime& date /* = wxDefaultDateTime */)
{
    bool retval = true;

    if ( !(date.IsValid()) || ( ( m_highdate.IsValid() ) ? ( date <= m_highdate ) : true ) )
    {
        m_lowdate = date;
    }
    else
    {
        retval = false;
    }

    return retval;
}

bool wxGenericCalendarCtrl::SetUpperDateLimit(const wxDateTime& date /* = wxDefaultDateTime */)
{
    bool retval = true;

    if ( !(date.IsValid()) || ( ( m_lowdate.IsValid() ) ? ( date >= m_lowdate ) : true ) )
    {
        m_highdate = date;
    }
    else
    {
        retval = false;
    }

    return retval;
}

bool wxGenericCalendarCtrl::SetDateRange(const wxDateTime& lowerdate /* = wxDefaultDateTime */, const wxDateTime& upperdate /* = wxDefaultDateTime */)
{
    bool retval = true;

    if (
        ( !( lowerdate.IsValid() ) || ( ( upperdate.IsValid() ) ? ( lowerdate <= upperdate ) : true ) ) &&
        ( !( upperdate.IsValid() ) || ( ( lowerdate.IsValid() ) ? ( upperdate >= lowerdate ) : true ) ) )
    {
        m_lowdate = lowerdate;
        m_highdate = upperdate;
    }
    else
    {
        retval = false;
    }

    return retval;
}

bool wxGenericCalendarCtrl::GetDateRange(wxDateTime *lowerdate,
                                         wxDateTime *upperdate) const
{
    if ( lowerdate )
        *lowerdate = m_lowdate;
    if ( upperdate )
        *upperdate = m_highdate;

    return m_lowdate.IsValid() || m_highdate.IsValid();
}

// ----------------------------------------------------------------------------
// date helpers
// ----------------------------------------------------------------------------

wxDateTime wxGenericCalendarCtrl::GetStartDate() const
{
    wxDateTime::Tm tm = m_date.GetTm();

    wxDateTime date = wxDateTime(1, tm.mon, tm.year);

    // rewind back
    date.SetToPrevWeekDay(GetWeekStart());

    if ( GetWindowStyle() & wxCAL_SHOW_SURROUNDING_WEEKS )
    {
        // We want to offset the calendar if we start on the first..
        if ( date.GetDay() == 1 )
        {
            date -= wxDateSpan::Week();
        }
    }

    return date;
}

bool wxGenericCalendarCtrl::IsDateShown(const wxDateTime& date) const
{
    if ( !(GetWindowStyle() & wxCAL_SHOW_SURROUNDING_WEEKS) )
    {
        return date.GetMonth() == m_date.GetMonth();
    }
    else
    {
        return true;
    }
}

bool wxGenericCalendarCtrl::IsDateInRange(const wxDateTime& date) const
{
    // Check if the given date is in the range specified
    return ( ( ( m_lowdate.IsValid() ) ? ( date >= m_lowdate ) : true )
        && ( ( m_highdate.IsValid() ) ? ( date <= m_highdate ) : true ) );
}

bool wxGenericCalendarCtrl::AdjustDateToRange(wxDateTime *date) const
{
    if ( m_lowdate.IsValid() && *date < m_lowdate )
    {
        *date = m_lowdate;
        return true;
    }

    if ( m_highdate.IsValid() && *date > m_highdate )
    {
        *date = m_highdate;
        return true;
    }

    return false;
}

size_t wxGenericCalendarCtrl::GetWeek(const wxDateTime& date) const
{
    size_t retval = date.GetWeekOfMonth(HasFlag(wxCAL_MONDAY_FIRST)
                                   ? wxDateTime::Monday_First
                                   : wxDateTime::Sunday_First);

    if ( (GetWindowStyle() & wxCAL_SHOW_SURROUNDING_WEEKS) )
    {
        // we need to offset an extra week if we "start" on the 1st of the month
        wxDateTime::Tm tm = date.GetTm();

        wxDateTime datetest = wxDateTime(1, tm.mon, tm.year);

        // rewind back
        datetest.SetToPrevWeekDay(GetWeekStart());

        if ( datetest.GetDay() == 1 )
        {
            retval += 1;
        }
    }

    return retval;
}

// ----------------------------------------------------------------------------
// size management
// ----------------------------------------------------------------------------

// this is a composite control and it must arrange its parts each time its
// size or position changes: the combobox and spinctrl are along the top of
// the available area and the calendar takes up the rest of the space

// the static controls are supposed to be always smaller than combo/spin so we
// always use the latter for size calculations and position the static to take
// the same space

// the constants used for the layout
#define VERT_MARGIN    5           // distance between combo and calendar
#define HORZ_MARGIN    5           //                            spin

wxSize wxGenericCalendarCtrl::DoGetBestSize() const
{
    // calc the size of the calendar
    const_cast<wxGenericCalendarCtrl *>(this)->RecalcGeometry();

    wxCoord width = 7*m_widthCol + m_calendarWeekWidth,
            height = 7*m_heightRow + m_rowOffset + VERT_MARGIN;

    if ( !HasFlag(wxCAL_SEQUENTIAL_MONTH_SELECTION) )
    {
        const wxSize bestSizeCombo = m_comboMonth->GetBestSize();

        height += wxMax(bestSizeCombo.y, m_spinYear->GetBestSize().y)
                    + VERT_MARGIN;

        wxCoord w2 = bestSizeCombo.x + HORZ_MARGIN + GetCharWidth()*8;
        if ( width < w2 )
            width = w2;
    }

    wxSize best(width, height);
    if ( !HasFlag(wxBORDER_NONE) )
    {
        best += GetWindowBorderSize();
    }

    CacheBestSize(best);

    return best;
}

void wxGenericCalendarCtrl::DoMoveWindow(int x, int y, int width, int height)
{
    int yDiff;

    if ( !HasFlag(wxCAL_SEQUENTIAL_MONTH_SELECTION) && m_staticMonth )
    {
        wxSize sizeCombo = m_comboMonth->GetEffectiveMinSize();
        wxSize sizeStatic = m_staticMonth->GetSize();
        wxSize sizeSpin = m_spinYear->GetSize();

        int maxHeight = wxMax(sizeSpin.y, sizeCombo.y);
        int dy = (maxHeight - sizeStatic.y) / 2;
        m_comboMonth->Move(x, y + (maxHeight - sizeCombo.y)/2);
        m_staticMonth->SetSize(x, y + dy, sizeCombo.x, -1);

        int xDiff = sizeCombo.x + HORZ_MARGIN;

        m_spinYear->SetSize(x + xDiff, y + (maxHeight - sizeSpin.y)/2, width - xDiff, maxHeight);
        m_staticYear->SetSize(x + xDiff, y + dy, width - xDiff, sizeStatic.y);

        yDiff = maxHeight + VERT_MARGIN;
    }
    else // no controls on the top
    {
        yDiff = 0;
    }

    wxControl::DoMoveWindow(x, y + yDiff, width, height - yDiff);
}

void wxGenericCalendarCtrl::DoGetSize(int *width, int *height) const
{
    wxControl::DoGetSize( width, height );
}

void wxGenericCalendarCtrl::RecalcGeometry()
{
    wxClientDC dc(this);

    dc.SetFont(GetFont());

    // determine the column width (weekday names are not necessarily wider
    // than the numbers (in some languages), so let's not assume that they are)
    m_widthCol = 0;
    for ( int day = 10; day <= 31; day++)
    {
        wxCoord width;
        dc.GetTextExtent(wxString::Format(wxT("%d"), day), &width, &m_heightRow);
        if ( width > m_widthCol )
        {
            // 1.5 times the width gives nice margins even if the weekday
            // names are short
            m_widthCol = width+width/2;
        }
    }
    wxDateTime::WeekDay wd;
    for ( wd = wxDateTime::Sun; wd < wxDateTime::Inv_WeekDay; wxNextWDay(wd) )
    {
        wxCoord width;
        dc.GetTextExtent(m_weekdays[wd], &width, &m_heightRow);
        if ( width > m_widthCol )
        {
            m_widthCol = width;
        }
    }

    m_calendarWeekWidth = HasFlag( wxCAL_SHOW_WEEK_NUMBERS )
        ? dc.GetTextExtent( wxString::Format( wxT( "%d" ), 42 )).GetWidth() + 4 : 0;

    // leave some margins
    m_widthCol += 2;
    m_heightRow += 2;

    m_rowOffset = HasFlag(wxCAL_SEQUENTIAL_MONTH_SELECTION) ? m_heightRow : 0; // conditional in relation to style
}

// ----------------------------------------------------------------------------
// drawing
// ----------------------------------------------------------------------------

void wxGenericCalendarCtrl::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc(this);

    dc.SetFont(GetFont());

    RecalcGeometry();

#if DEBUG_PAINT
    wxLogDebug("--- starting to paint, selection: %s, week %u\n",
           m_date.Format("%a %d-%m-%Y %H:%M:%S").c_str(),
           GetWeek(m_date));
#endif

    wxCoord y = 0;
    wxCoord x0 = m_calendarWeekWidth;

    if ( HasFlag(wxCAL_SEQUENTIAL_MONTH_SELECTION) )
    {
        // draw the sequential month-selector

        dc.SetBackgroundMode(wxBRUSHSTYLE_TRANSPARENT);
        dc.SetTextForeground(*wxBLACK);
        dc.SetBrush(wxBrush(m_colHeaderBg, wxBRUSHSTYLE_SOLID));
        dc.SetPen(wxPen(m_colHeaderBg, 1, wxPENSTYLE_SOLID));
        dc.DrawRectangle(0, y, GetClientSize().x, m_heightRow);

        // Get extent of month-name + year
        wxCoord monthw, monthh;
        wxString headertext = m_date.Format(wxT("%B %Y"));
        dc.GetTextExtent(headertext, &monthw, &monthh);

        // draw month-name centered above weekdays
        wxCoord monthx = ((m_widthCol * 7) - monthw) / 2 + x0;
        wxCoord monthy = ((m_heightRow - monthh) / 2) + y;
        dc.DrawText(headertext, monthx,  monthy);

        // calculate the "month-arrows"
        wxPoint leftarrow[3];
        wxPoint rightarrow[3];

        int arrowheight = monthh / 2;

        leftarrow[0] = wxPoint(0, arrowheight / 2);
        leftarrow[1] = wxPoint(arrowheight / 2, 0);
        leftarrow[2] = wxPoint(arrowheight / 2, arrowheight - 1);

        rightarrow[0] = wxPoint(0,0);
        rightarrow[1] = wxPoint(arrowheight / 2, arrowheight / 2);
        rightarrow[2] = wxPoint(0, arrowheight - 1);

        // draw the "month-arrows"

        wxCoord arrowy = (m_heightRow - arrowheight) / 2;
        wxCoord larrowx = (m_widthCol - (arrowheight / 2)) / 2 + x0;
        wxCoord rarrowx = ((m_widthCol - (arrowheight / 2)) / 2) + m_widthCol*6 + x0;
        m_leftArrowRect = m_rightArrowRect = wxRect(0,0,0,0);

        if ( AllowMonthChange() )
        {
            wxDateTime ldpm = wxDateTime(1,m_date.GetMonth(), m_date.GetYear()) - wxDateSpan::Day(); // last day prev month
            // Check if range permits change
            if ( IsDateInRange(ldpm) && ( ( ldpm.GetYear() == m_date.GetYear() ) ? true : AllowYearChange() ) )
            {
                m_leftArrowRect = wxRect(larrowx - 3, arrowy - 3, (arrowheight / 2) + 8, (arrowheight + 6));
                dc.SetBrush(*wxBLACK_BRUSH);
                dc.SetPen(*wxBLACK_PEN);
                dc.DrawPolygon(3, leftarrow, larrowx , arrowy, wxWINDING_RULE);
                dc.SetBrush(*wxTRANSPARENT_BRUSH);
                dc.DrawRectangle(m_leftArrowRect);
            }
            wxDateTime fdnm = wxDateTime(1,m_date.GetMonth(), m_date.GetYear()) + wxDateSpan::Month(); // first day next month
            if ( IsDateInRange(fdnm) && ( ( fdnm.GetYear() == m_date.GetYear() ) ? true : AllowYearChange() ) )
            {
                m_rightArrowRect = wxRect(rarrowx - 4, arrowy - 3, (arrowheight / 2) + 8, (arrowheight + 6));
                dc.SetBrush(*wxBLACK_BRUSH);
                dc.SetPen(*wxBLACK_PEN);
                dc.DrawPolygon(3, rightarrow, rarrowx , arrowy, wxWINDING_RULE);
                dc.SetBrush(*wxTRANSPARENT_BRUSH);
                dc.DrawRectangle(m_rightArrowRect);
            }
        }

        y += m_heightRow;
    }

    // first draw the week days
    if ( IsExposed(x0, y, x0 + 7*m_widthCol, m_heightRow) )
    {
#if DEBUG_PAINT
        wxLogDebug("painting the header");
#endif

        dc.SetBackgroundMode(wxBRUSHSTYLE_TRANSPARENT);
        dc.SetTextForeground(m_colHeaderFg);
        dc.SetBrush(wxBrush(m_colHeaderBg, wxBRUSHSTYLE_SOLID));
        dc.SetPen(wxPen(m_colHeaderBg, 1, wxPENSTYLE_SOLID));
        dc.DrawRectangle(0, y, GetClientSize().x, m_heightRow);

        bool startOnMonday = HasFlag(wxCAL_MONDAY_FIRST);
        for ( int wd = 0; wd < 7; wd++ )
        {
            size_t n;
            if ( startOnMonday )
                n = wd == 6 ? 0 : wd + 1;
            else
                n = wd;
            wxCoord dayw, dayh;
            dc.GetTextExtent(m_weekdays[n], &dayw, &dayh);
            dc.DrawText(m_weekdays[n], x0 + (wd*m_widthCol) + ((m_widthCol- dayw) / 2), y); // center the day-name
        }
    }

    // then the calendar itself
    dc.SetTextForeground(*wxBLACK);
    //dc.SetFont(*wxNORMAL_FONT);

    y += m_heightRow;

    // draw column with calendar week nr
    if ( HasFlag( wxCAL_SHOW_WEEK_NUMBERS ) && IsExposed( 0, y, m_calendarWeekWidth, m_heightRow * 6 ))
    {
        dc.SetBackgroundMode(wxTRANSPARENT);
        dc.SetBrush(wxBrush(m_colHeaderBg, wxSOLID));
        dc.SetPen(wxPen(m_colHeaderBg, 1, wxSOLID));
        dc.DrawRectangle( 0, y, m_calendarWeekWidth, m_heightRow * 6 );
        wxDateTime date = GetStartDate();
        for ( size_t i = 0; i < 6; ++i )
        {
            const int weekNr = date.GetWeekOfYear();
            wxString text = wxString::Format( wxT( "%d" ), weekNr );
            dc.DrawText( text, m_calendarWeekWidth - dc.GetTextExtent( text ).GetWidth() - 2, y + m_heightRow * i );
            date += wxDateSpan::Week();
        }
    }

    wxDateTime date = GetStartDate();

#if DEBUG_PAINT
    wxLogDebug("starting calendar from %s\n",
            date.Format("%a %d-%m-%Y %H:%M:%S").c_str());
#endif

    dc.SetBackgroundMode(wxBRUSHSTYLE_SOLID);
    for ( size_t nWeek = 1; nWeek <= 6; nWeek++, y += m_heightRow )
    {
        // if the update region doesn't intersect this row, don't paint it
        if ( !IsExposed(x0, y, x0 + 7*m_widthCol, m_heightRow - 1) )
        {
            date += wxDateSpan::Week();

            continue;
        }

#if DEBUG_PAINT
        wxLogDebug("painting week %d at y = %d\n", nWeek, y);
#endif

        for ( int wd = 0; wd < 7; wd++ )
        {
            dc.SetTextBackground(m_colBackground);
            if ( IsDateShown(date) )
            {
                // don't use wxDate::Format() which prepends 0s
                unsigned int day = date.GetDay();
                wxString dayStr = wxString::Format(wxT("%u"), day);
                wxCoord width;
                dc.GetTextExtent(dayStr, &width, NULL);

                bool changedColours = false,
                     changedFont = false;

                bool isSel = false;
                wxCalendarDateAttr *attr = NULL;

                if ( date.GetMonth() != m_date.GetMonth() || !IsDateInRange(date) )
                {
                    // draw the days of adjacent months in different colour
                    dc.SetTextForeground(m_colSurrounding);
                    changedColours = true;
                }
                else
                {
                    isSel = date.IsSameDate(m_date);
                    attr = m_attrs[day - 1];

                    if ( isSel )
                    {
                        dc.SetTextForeground(m_colHighlightFg);
                        dc.SetTextBackground(m_colHighlightBg);

                        changedColours = true;
                    }
                    else if ( attr )
                    {
                        wxColour colFg, colBg;

                        if ( attr->IsHoliday() )
                        {
                            colFg = m_colHolidayFg;
                            colBg = m_colHolidayBg;
                        }
                        else
                        {
                            colFg = attr->GetTextColour();
                            colBg = attr->GetBackgroundColour();
                        }

                        if ( colFg.IsOk() )
                        {
                            dc.SetTextForeground(colFg);
                            changedColours = true;
                        }

                        if ( colBg.IsOk() )
                        {
                            dc.SetTextBackground(colBg);
                            changedColours = true;
                        }

                        if ( attr->HasFont() )
                        {
                            dc.SetFont(attr->GetFont());
                            changedFont = true;
                        }
                    }
                }

                wxCoord x = wd*m_widthCol + (m_widthCol - width) / 2 + x0;
                dc.DrawText(dayStr, x, y + 1);

                if ( !isSel && attr && attr->HasBorder() )
                {
                    wxColour colBorder;
                    if ( attr->HasBorderColour() )
                    {
                        colBorder = attr->GetBorderColour();
                    }
                    else
                    {
                        colBorder = GetForegroundColour();
                    }

                    wxPen pen(colBorder, 1, wxPENSTYLE_SOLID);
                    dc.SetPen(pen);
                    dc.SetBrush(*wxTRANSPARENT_BRUSH);

                    switch ( attr->GetBorder() )
                    {
                        case wxCAL_BORDER_SQUARE:
                            dc.DrawRectangle(x - 2, y,
                                             width + 4, m_heightRow);
                            break;

                        case wxCAL_BORDER_ROUND:
                            dc.DrawEllipse(x - 2, y,
                                           width + 4, m_heightRow);
                            break;

                        default:
                            wxFAIL_MSG(wxT("unknown border type"));
                    }
                }

                if ( changedColours )
                {
                    dc.SetTextForeground(GetForegroundColour());
                    dc.SetTextBackground(GetBackgroundColour());
                }

                if ( changedFont )
                {
                    dc.SetFont(GetFont());
                }
            }
            //else: just don't draw it

            date += wxDateSpan::Day();
        }
    }

    // Greying out out-of-range background
    bool showSurrounding = (GetWindowStyle() & wxCAL_SHOW_SURROUNDING_WEEKS) != 0;

    date = ( showSurrounding ) ? GetStartDate() : wxDateTime(1, m_date.GetMonth(), m_date.GetYear());
    if ( !IsDateInRange(date) )
    {
        wxDateTime firstOOR = GetLowerDateLimit() - wxDateSpan::Day(); // first out-of-range

        wxBrush oorbrush = *wxLIGHT_GREY_BRUSH;
        oorbrush.SetStyle(wxBRUSHSTYLE_FDIAGONAL_HATCH);

        HighlightRange(&dc, date, firstOOR, wxTRANSPARENT_PEN, &oorbrush);
    }

    date = ( showSurrounding ) ? GetStartDate() + wxDateSpan::Weeks(6) - wxDateSpan::Day() : wxDateTime().SetToLastMonthDay(m_date.GetMonth(), m_date.GetYear());
    if ( !IsDateInRange(date) )
    {
        wxDateTime firstOOR = GetUpperDateLimit() + wxDateSpan::Day(); // first out-of-range

        wxBrush oorbrush = *wxLIGHT_GREY_BRUSH;
        oorbrush.SetStyle(wxBRUSHSTYLE_FDIAGONAL_HATCH);

        HighlightRange(&dc, firstOOR, date, wxTRANSPARENT_PEN, &oorbrush);
    }

#if DEBUG_PAINT
    wxLogDebug("+++ finished painting");
#endif
}

void wxGenericCalendarCtrl::RefreshDate(const wxDateTime& date)
{
    RecalcGeometry();

    wxRect rect;

    // always refresh the whole row at once because our OnPaint() will draw
    // the whole row anyhow - and this allows the small optimization in
    // OnClick() below to work
    rect.x = m_calendarWeekWidth;

    rect.y = (m_heightRow * GetWeek(date)) + m_rowOffset;

    rect.width = 7*m_widthCol;
    rect.height = m_heightRow;

#ifdef __WXMSW__
    // VZ: for some reason, the selected date seems to occupy more space under
    //     MSW - this is probably some bug in the font size calculations, but I
    //     don't know where exactly. This fix is ugly and leads to more
    //     refreshes than really needed, but without it the selected days
    //     leaves even more ugly underscores on screen.
    rect.Inflate(0, 1);
#endif // MSW

#if DEBUG_PAINT
    wxLogDebug("*** refreshing week %d at (%d, %d)-(%d, %d)\n",
           GetWeek(date),
           rect.x, rect.y,
           rect.x + rect.width, rect.y + rect.height);
#endif

    Refresh(true, &rect);
}

void wxGenericCalendarCtrl::HighlightRange(wxPaintDC* pDC, const wxDateTime& fromdate, const wxDateTime& todate, const wxPen* pPen, const wxBrush* pBrush)
{
    // Highlights the given range using pen and brush
    // Does nothing if todate < fromdate


#if DEBUG_PAINT
    wxLogDebug("+++ HighlightRange: (%s) - (%s) +++", fromdate.Format("%d %m %Y"), todate.Format("%d %m %Y"));
#endif

    if ( todate >= fromdate )
    {
        // do stuff
        // date-coordinates
        int fd, fw;
        int td, tw;

        // implicit: both dates must be currently shown - checked by GetDateCoord
        if ( GetDateCoord(fromdate, &fd, &fw) && GetDateCoord(todate, &td, &tw) )
        {
#if DEBUG_PAINT
            wxLogDebug("Highlight range: (%i, %i) - (%i, %i)", fd, fw, td, tw);
#endif
            if ( ( (tw - fw) == 1 ) && ( td < fd ) )
            {
                // special case: interval 7 days or less not in same week
                // split in two separate intervals
                wxDateTime tfd = fromdate + wxDateSpan::Days(7-fd);
                wxDateTime ftd = tfd + wxDateSpan::Day();
#if DEBUG_PAINT
                wxLogDebug("Highlight: Separate segments");
#endif
                // draw separately
                HighlightRange(pDC, fromdate, tfd, pPen, pBrush);
                HighlightRange(pDC, ftd, todate, pPen, pBrush);
            }
            else
            {
                int numpoints;
                wxPoint corners[8]; // potentially 8 corners in polygon
                wxCoord x0 = m_calendarWeekWidth;

                if ( fw == tw )
                {
                    // simple case: same week
                    numpoints = 4;
                    corners[0] = wxPoint(x0 + (fd - 1) * m_widthCol, (fw * m_heightRow) + m_rowOffset);
                    corners[1] = wxPoint(x0 + (fd - 1) * m_widthCol, ((fw + 1 ) * m_heightRow) + m_rowOffset);
                    corners[2] = wxPoint(x0 + td * m_widthCol, ((tw + 1) * m_heightRow) + m_rowOffset);
                    corners[3] = wxPoint(x0 + td * m_widthCol, (tw * m_heightRow) + m_rowOffset);
                }
                else
                {
                    int cidx = 0;
                    // "complex" polygon
                    corners[cidx] = wxPoint(x0 + (fd - 1) * m_widthCol, (fw * m_heightRow) + m_rowOffset); cidx++;

                    if ( fd > 1 )
                    {
                        corners[cidx] = wxPoint(x0 + (fd - 1) * m_widthCol, ((fw + 1) * m_heightRow) + m_rowOffset); cidx++;
                        corners[cidx] = wxPoint(x0, ((fw + 1) * m_heightRow) + m_rowOffset); cidx++;
                    }

                    corners[cidx] = wxPoint(x0, ((tw + 1) * m_heightRow) + m_rowOffset); cidx++;
                    corners[cidx] = wxPoint(x0 + td * m_widthCol, ((tw + 1) * m_heightRow) + m_rowOffset); cidx++;

                    if ( td < 7 )
                    {
                        corners[cidx] = wxPoint(x0 + td * m_widthCol, (tw * m_heightRow) + m_rowOffset); cidx++;
                        corners[cidx] = wxPoint(x0 + 7 * m_widthCol, (tw * m_heightRow) + m_rowOffset); cidx++;
                    }

                    corners[cidx] = wxPoint(x0 + 7 * m_widthCol, (fw * m_heightRow) + m_rowOffset); cidx++;

                    numpoints = cidx;
                }

                // draw the polygon
                pDC->SetBrush(*pBrush);
                pDC->SetPen(*pPen);
                pDC->DrawPolygon(numpoints, corners);
            }
        }
    }
    // else do nothing
#if DEBUG_PAINT
    wxLogDebug("--- HighlightRange ---");
#endif
}

bool wxGenericCalendarCtrl::GetDateCoord(const wxDateTime& date, int *day, int *week) const
{
    bool retval = true;

#if DEBUG_PAINT
    wxLogDebug("+++ GetDateCoord: (%s) +++", date.Format("%d %m %Y"));
#endif

    if ( IsDateShown(date) )
    {
        bool startOnMonday = HasFlag(wxCAL_MONDAY_FIRST);

        // Find day
        *day = date.GetWeekDay();

        if ( *day == 0 ) // sunday
        {
            *day = ( startOnMonday ) ? 7 : 1;
        }
        else
        {
            *day += ( startOnMonday ) ? 0 : 1;
        }

        int targetmonth = date.GetMonth() + (12 * date.GetYear());
        int thismonth = m_date.GetMonth() + (12 * m_date.GetYear());

        // Find week
        if ( targetmonth == thismonth )
        {
            *week = GetWeek(date);
        }
        else
        {
            if ( targetmonth < thismonth )
            {
                *week = 1; // trivial
            }
            else // targetmonth > thismonth
            {
                wxDateTime ldcm;
                int lastweek;
                int lastday;

                // get the datecoord of the last day in the month currently shown
#if DEBUG_PAINT
                wxLogDebug("     +++ LDOM +++");
#endif
                GetDateCoord(ldcm.SetToLastMonthDay(m_date.GetMonth(), m_date.GetYear()), &lastday, &lastweek);
#if DEBUG_PAINT
                wxLogDebug("     --- LDOM ---");
#endif

                wxTimeSpan span = date - ldcm;

                int daysfromlast = span.GetDays();
#if DEBUG_PAINT
                wxLogDebug("daysfromlast: %i", daysfromlast);
#endif
                if ( daysfromlast + lastday > 7 ) // past week boundary
                {
                    int wholeweeks = (daysfromlast / 7);
                    *week = wholeweeks + lastweek;
                    if ( (daysfromlast - (7 * wholeweeks) + lastday) > 7 )
                    {
                        *week += 1;
                    }
                }
                else
                {
                    *week = lastweek;
                }
            }
        }
    }
    else
    {
        *day = -1;
        *week = -1;
        retval = false;
    }

#if DEBUG_PAINT
    wxLogDebug("--- GetDateCoord: (%s) = (%i, %i) ---", date.Format("%d %m %Y"), *day, *week);
#endif

    return retval;
}

// ----------------------------------------------------------------------------
// mouse handling
// ----------------------------------------------------------------------------

void wxGenericCalendarCtrl::OnDClick(wxMouseEvent& event)
{
    if ( HitTest(event.GetPosition()) != wxCAL_HITTEST_DAY )
    {
        event.Skip();
    }
    else
    {
        GenerateEvent(wxEVT_CALENDAR_DOUBLECLICKED);
    }
}

void wxGenericCalendarCtrl::OnClick(wxMouseEvent& event)
{
    wxDateTime date;
    wxDateTime::WeekDay wday;
    switch ( HitTest(event.GetPosition(), &date, &wday) )
    {
        case wxCAL_HITTEST_DAY:
            if ( IsDateInRange(date) )
            {
                ChangeDay(date);

                GenerateEvent(wxEVT_CALENDAR_SEL_CHANGED);

                // we know that the month/year never change when the user
                // clicks on the control so there is no need to call
                // GenerateAllChangeEvents() here, we know which event to send
                GenerateEvent(wxEVT_CALENDAR_DAY_CHANGED);
            }
        break;

        case wxCAL_HITTEST_WEEK:
        {
            wxCalendarEvent send( this, date, wxEVT_CALENDAR_WEEK_CLICKED );
            HandleWindowEvent( send );
        }
        break;

        case wxCAL_HITTEST_HEADER:
            {
                wxCalendarEvent eventWd(this, GetDate(),
                                        wxEVT_CALENDAR_WEEKDAY_CLICKED);
                eventWd.SetWeekDay(wday);
                (void)GetEventHandler()->ProcessEvent(eventWd);
            }
            break;

        case wxCAL_HITTEST_DECMONTH:
        case wxCAL_HITTEST_INCMONTH:
        case wxCAL_HITTEST_SURROUNDING_WEEK:
            SetDateAndNotify(date); // we probably only want to refresh the control. No notification.. (maybe as an option?)
            break;

        default:
            wxFAIL_MSG(wxT("unknown hittest code"));
            // fall through

        case wxCAL_HITTEST_NOWHERE:
            event.Skip();
            break;
    }

    // as we don't (always) skip the message, we're not going to receive the
    // focus on click by default if we don't do it ourselves
    SetFocus();
}

wxCalendarHitTestResult wxGenericCalendarCtrl::HitTest(const wxPoint& pos,
                                                wxDateTime *date,
                                                wxDateTime::WeekDay *wd)
{
    RecalcGeometry();

    // the position where the calendar really begins
    wxCoord x0 = m_calendarWeekWidth;

    if ( HasFlag(wxCAL_SEQUENTIAL_MONTH_SELECTION) )
    {
        // Header: month

        // we need to find out if the hit is on left arrow, on month or on right arrow
        // left arrow?
        if ( m_leftArrowRect.Contains(pos) )
        {
            if ( date )
            {
                if ( IsDateInRange(m_date - wxDateSpan::Month()) )
                {
                    *date = m_date - wxDateSpan::Month();
                }
                else
                {
                    *date = GetLowerDateLimit();
                }
            }

            return wxCAL_HITTEST_DECMONTH;
        }

        if ( m_rightArrowRect.Contains(pos) )
        {
            if ( date )
            {
                if ( IsDateInRange(m_date + wxDateSpan::Month()) )
                {
                    *date = m_date + wxDateSpan::Month();
                }
                else
                {
                    *date = GetUpperDateLimit();
                }
            }

            return wxCAL_HITTEST_INCMONTH;
        }
    }

    if ( pos.x - x0 < 0 )
    {
        if ( pos.x >= 0 && pos.y > m_rowOffset + m_heightRow && pos.y <= m_rowOffset + m_heightRow * 7 )
        {
            if ( date )
            {
                *date = GetStartDate();
                *date += wxDateSpan::Week() * (( pos.y - m_rowOffset ) / m_heightRow - 1 );
            }
            if ( wd )
                *wd = GetWeekStart();
            return wxCAL_HITTEST_WEEK;
        }
        else    // early exit -> the rest of the function checks for clicks on days
            return wxCAL_HITTEST_NOWHERE;
    }

    // header: week days
    int wday = (pos.x - x0) / m_widthCol;
    if ( wday > 6 )
        return wxCAL_HITTEST_NOWHERE;
    if ( pos.y < (m_heightRow + m_rowOffset))
    {
        if ( pos.y > m_rowOffset )
        {
            if ( wd )
            {
                if ( HasFlag(wxCAL_MONDAY_FIRST) )
                {
                    wday = wday == 6 ? 0 : wday + 1;
                }

                *wd = (wxDateTime::WeekDay)wday;
            }

            return wxCAL_HITTEST_HEADER;
        }
        else
        {
            return wxCAL_HITTEST_NOWHERE;
        }
    }

    int week = (pos.y - (m_heightRow + m_rowOffset)) / m_heightRow;
    if ( week >= 6 || wday >= 7 )
    {
        return wxCAL_HITTEST_NOWHERE;
    }

    wxDateTime dt = GetStartDate() + wxDateSpan::Days(7*week + wday);

    if ( IsDateShown(dt) )
    {
        if ( date )
            *date = dt;

        if ( dt.GetMonth() == m_date.GetMonth() )
        {

            return wxCAL_HITTEST_DAY;
        }
        else
        {
            return wxCAL_HITTEST_SURROUNDING_WEEK;
        }
    }
    else
    {
        return wxCAL_HITTEST_NOWHERE;
    }
}

void wxGenericCalendarCtrl::OnWheel(wxMouseEvent& event)
{
    wxDateSpan span;
    switch ( event.GetWheelAxis() )
    {
        case wxMOUSE_WHEEL_VERTICAL:
            // For consistency with the native controls, scrolling upwards
            // should go to the past, even if the rotation is positive and
            // could be normally expected to increase the date.
            span = -wxDateSpan::Month();
            break;

        case wxMOUSE_WHEEL_HORIZONTAL:
            span = wxDateSpan::Year();
            break;
    }

    // Currently we only take into account the rotation direction, not its
    // magnitude.
    if ( event.GetWheelRotation() < 0 )
        span = -span;

    SetDateAndNotify(m_date + span);
}

// ----------------------------------------------------------------------------
// subcontrols events handling
// ----------------------------------------------------------------------------

void wxGenericCalendarCtrl::OnMonthChange(wxCommandEvent& event)
{
    wxDateTime::Tm tm = m_date.GetTm();

    wxDateTime::Month mon = (wxDateTime::Month)event.GetInt();
    if ( tm.mday > wxDateTime::GetNumberOfDays(mon, tm.year) )
    {
        tm.mday = wxDateTime::GetNumberOfDays(mon, tm.year);
    }

    wxDateTime dt(tm.mday, mon, tm.year);
    if ( AdjustDateToRange(&dt) )
    {
        // The date must have been changed to ensure it's in valid range,
        // reflect this in the month choice control.
        m_comboMonth->SetSelection(dt.GetMonth());
    }

    SetDateAndNotify(dt);
}

void wxGenericCalendarCtrl::HandleYearChange(wxCommandEvent& event)
{
    int year = (int)event.GetInt();
    if ( year == INT_MIN )
    {
        // invalid year in the spin control, ignore it
        return;
    }

    wxDateTime::Tm tm = m_date.GetTm();

    if ( tm.mday > wxDateTime::GetNumberOfDays(tm.mon, year) )
    {
        tm.mday = wxDateTime::GetNumberOfDays(tm.mon, year);
    }

    wxDateTime dt(tm.mday, tm.mon, year);
    if ( AdjustDateToRange(&dt) )
    {
        // As above, if the date was changed to keep it in valid range, its
        // possibly changed year must be shown in the GUI.
        m_spinYear->SetValue(dt.GetYear());
    }

    SetDateAndNotify(dt);
}

void wxGenericCalendarCtrl::OnYearChange(wxSpinEvent& event)
{
    HandleYearChange( event );
}

void wxGenericCalendarCtrl::OnYearTextChange(wxCommandEvent& event)
{
    SetUserChangedYear();
    HandleYearChange(event);
}

// Responds to colour changes, and passes event on to children.
void wxGenericCalendarCtrl::OnSysColourChanged(wxSysColourChangedEvent& event)
{
    // reinit colours
    InitColours();

    // Propagate the event to the children
    wxControl::OnSysColourChanged(event);

    // Redraw control area
    SetBackgroundColour(m_colBackground);
    Refresh();
}

// ----------------------------------------------------------------------------
// keyboard interface
// ----------------------------------------------------------------------------

void wxGenericCalendarCtrl::OnChar(wxKeyEvent& event)
{
    switch ( event.GetKeyCode() )
    {
        case wxT('+'):
        case WXK_ADD:
            SetDateAndNotify(m_date + wxDateSpan::Year());
            break;

        case wxT('-'):
        case WXK_SUBTRACT:
            SetDateAndNotify(m_date - wxDateSpan::Year());
            break;

        case WXK_PAGEUP:
            SetDateAndNotify(m_date - wxDateSpan::Month());
            break;

        case WXK_PAGEDOWN:
            SetDateAndNotify(m_date + wxDateSpan::Month());
            break;

        case WXK_RIGHT:
            if ( event.ControlDown() )
            {
                wxDateTime target = m_date.SetToNextWeekDay(GetWeekEnd());
                AdjustDateToRange(&target);
                SetDateAndNotify(target);
            }
            else
                SetDateAndNotify(m_date + wxDateSpan::Day());
            break;

        case WXK_LEFT:
            if ( event.ControlDown() )
            {
                wxDateTime target = m_date.SetToPrevWeekDay(GetWeekStart());
                AdjustDateToRange(&target);
                SetDateAndNotify(target);
            }
            else
                SetDateAndNotify(m_date - wxDateSpan::Day());
            break;

        case WXK_UP:
            SetDateAndNotify(m_date - wxDateSpan::Week());
            break;

        case WXK_DOWN:
            SetDateAndNotify(m_date + wxDateSpan::Week());
            break;

        case WXK_HOME:
            if ( event.ControlDown() )
                SetDateAndNotify(wxDateTime::Today());
            else
                SetDateAndNotify(wxDateTime(1, m_date.GetMonth(), m_date.GetYear()));
            break;

        case WXK_END:
            SetDateAndNotify(wxDateTime(m_date).SetToLastMonthDay());
            break;

        case WXK_RETURN:
            GenerateEvent(wxEVT_CALENDAR_DOUBLECLICKED);
            break;

        default:
            event.Skip();
    }
}

// ----------------------------------------------------------------------------
// holidays handling
// ----------------------------------------------------------------------------

void wxGenericCalendarCtrl::SetHoliday(size_t day)
{
    wxCHECK_RET( day > 0 && day < 32, wxT("invalid day in SetHoliday") );

    wxCalendarDateAttr *attr = GetAttr(day);
    if ( !attr )
    {
        attr = new wxCalendarDateAttr;
    }

    attr->SetHoliday(true);

    // can't use SetAttr() because it would delete this pointer
    m_attrs[day - 1] = attr;
}

void wxGenericCalendarCtrl::ResetHolidayAttrs()
{
    for ( size_t day = 0; day < 31; day++ )
    {
        if ( m_attrs[day] )
        {
            m_attrs[day]->SetHoliday(false);
        }
    }
}

void wxGenericCalendarCtrl::Mark(size_t day, bool mark)
{
    wxCHECK_RET( day > 0 && day < 32, wxT("invalid day in Mark") );

    const wxCalendarDateAttr& m = wxCalendarDateAttr::GetMark();
    if (mark) {
        if ( m_attrs[day - 1] )
            AddAttr(m_attrs[day - 1], m);
        else
            SetAttr(day, new wxCalendarDateAttr(m));
    }
    else {
        if ( m_attrs[day - 1] )
            DelAttr(m_attrs[day - 1], m);
    }
}

//static
wxVisualAttributes
wxGenericCalendarCtrl::GetClassDefaultAttributes(wxWindowVariant variant)
{
    // Use the same color scheme as wxListBox
    return wxListBox::GetClassDefaultAttributes(variant);
}

#endif // wxUSE_CALENDARCTRL
