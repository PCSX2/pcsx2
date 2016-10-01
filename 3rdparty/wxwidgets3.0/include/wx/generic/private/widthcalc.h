/////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/private/widthcalc.h
// Purpose:     wxMaxWidthCalculatorBase helper class.
// Author:      Václav Slavík, Kinaou Hervé
// Copyright:   (c) 2015 wxWidgets team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GENERIC_PRIVATE_WIDTHCALC_H_
#define _WX_GENERIC_PRIVATE_WIDTHCALC_H_

#include "wx/defs.h"

#if wxUSE_DATAVIEWCTRL || wxUSE_LISTCTRL

#include "wx/log.h"
#include "wx/timer.h"

// ----------------------------------------------------------------------------
// wxMaxWidthCalculatorBase: base class for calculating max column width
// ----------------------------------------------------------------------------

class wxMaxWidthCalculatorBase
{
public:
    // column of which calculate the width
    explicit wxMaxWidthCalculatorBase(size_t column)
        : m_column(column),
          m_width(0)
    {
    }

    void UpdateWithWidth(int width)
    {
        m_width = wxMax(m_width, width);
    }

    // Update the max with for the expected row
    virtual void UpdateWithRow(int row) = 0;

    int GetMaxWidth() const { return m_width; }
    size_t GetColumn() const { return m_column; }

    void
    ComputeBestColumnWidth(size_t count,
                           size_t first_visible,
                           size_t last_visible)
    {
        // The code below deserves some explanation. For very large controls, we
        // simply can't afford to calculate sizes for all items, it takes too
        // long. So the best we can do is to check the first and the last N/2
        // items in the control for some sufficiently large N and calculate best
        // sizes from that. That can result in the calculated best width being too
        // small for some outliers, but it's better to get slightly imperfect
        // result than to wait several seconds after every update. To avoid highly
        // visible miscalculations, we also include all currently visible items
        // no matter what.  Finally, the value of N is determined dynamically by
        // measuring how much time we spent on the determining item widths so far.

#if wxUSE_STOPWATCH
        size_t top_part_end = count;
        static const long CALC_TIMEOUT = 20/*ms*/;
        // don't call wxStopWatch::Time() too often
        static const unsigned CALC_CHECK_FREQ = 100;
        wxStopWatch timer;
#else
        // use some hard-coded limit, that's the best we can do without timer
        size_t top_part_end = wxMin(500, count);
#endif // wxUSE_STOPWATCH/!wxUSE_STOPWATCH

        size_t row = 0;

        for ( row = 0; row < top_part_end; row++ )
        {
#if wxUSE_STOPWATCH
            if ( row % CALC_CHECK_FREQ == CALC_CHECK_FREQ-1 &&
                 timer.Time() > CALC_TIMEOUT )
                break;
#endif // wxUSE_STOPWATCH
            UpdateWithRow(row);
        }

        // row is the first unmeasured item now; that's our value of N/2
        if ( row < count )
        {
            top_part_end = row;

            // add bottom N/2 items now:
            const size_t bottom_part_start = wxMax(row, count - row);
            for ( row = bottom_part_start; row < count; row++ )
            {
                UpdateWithRow(row);
            }

            // finally, include currently visible items in the calculation:
            first_visible = wxMax(first_visible, top_part_end);
            last_visible = wxMin(bottom_part_start, last_visible);

            for ( row = first_visible; row < last_visible; row++ )
            {
                UpdateWithRow(row);
            }

            wxLogTrace("items container",
                       "determined best size from %zu top, %zu bottom "
                       "plus %zu more visible items out of %zu total",
                       top_part_end,
                       count - bottom_part_start,
                       wxMax(0, last_visible - first_visible),
                       count);
        }
    }

private:
    const size_t m_column;
    int m_width;

    wxDECLARE_NO_COPY_CLASS(wxMaxWidthCalculatorBase);
};

#endif // wxUSE_DATAVIEWCTRL || wxUSE_LISTCTRL

#endif // _WX_GENERIC_PRIVATE_WIDTHCALC_H_
