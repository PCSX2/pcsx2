/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/slider.cpp
// Purpose:     wxSlider, using the Win95 (and later) trackbar control
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// Copyright:   (c) Julian Smart 1998
//                  Vadim Zeitlin 2004
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

#if wxUSE_SLIDER

#include "wx/slider.h"

#ifndef WX_PRECOMP
    #include "wx/msw/wrapcctl.h" // include <commctrl.h> "properly"
    #include "wx/brush.h"
#endif

#include "wx/msw/subwin.h"

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

namespace
{

// indices of labels in wxSlider::m_labels
enum
{
    SliderLabel_Min,
    SliderLabel_Max,
    SliderLabel_Value,
    SliderLabel_Last
};

// the gaps between the slider and the labels, in pixels
const int HGAP = 5;
const int VGAP = 4;
// the width of the borders including white space
const int BORDERPAD = 8;
// these 2 values are arbitrary:
const int THUMB = 24;
const int TICK = 8;

} // anonymous namespace

// ============================================================================
// wxSlider implementation
// ============================================================================

// ----------------------------------------------------------------------------
// construction
// ----------------------------------------------------------------------------

void wxSlider::Init()
{
    m_labels = NULL;

    m_pageSize = 1;
    m_lineSize = 1;
    m_rangeMax = 0;
    m_rangeMin = 0;
    m_tickFreq = 0;

    m_isDragging = false;
}

bool wxSlider::Create(wxWindow *parent,
                      wxWindowID id,
                      int value,
                      int minValue,
                      int maxValue,
                      const wxPoint& pos,
                      const wxSize& size,
                      long style,
                      const wxValidator& validator,
                      const wxString& name)
{
    wxCHECK_MSG( minValue < maxValue, false,
        wxT("Slider minimum must be strictly less than the maximum.") );

    // our styles are redundant: wxSL_LEFT/RIGHT imply wxSL_VERTICAL and
    // wxSL_TOP/BOTTOM imply wxSL_HORIZONTAL, but for backwards compatibility
    // reasons we can't really change it, instead try to infer the orientation
    // from the flags given to us here
    switch ( style & (wxSL_LEFT | wxSL_RIGHT | wxSL_TOP | wxSL_BOTTOM) )
    {
        case wxSL_LEFT:
        case wxSL_RIGHT:
            style |= wxSL_VERTICAL;
            break;

        case wxSL_TOP:
        case wxSL_BOTTOM:
            style |= wxSL_HORIZONTAL;
            break;

        case 0:
            // no specific direction, do we have at least the orientation?
            if ( !(style & (wxSL_HORIZONTAL | wxSL_VERTICAL)) )
            {
                // no, choose default
                style |= wxSL_BOTTOM | wxSL_HORIZONTAL;
            }
    };

    wxASSERT_MSG( !(style & wxSL_VERTICAL) || !(style & wxSL_HORIZONTAL),
                    wxT("incompatible slider direction and orientation") );


    // initialize everything
    if ( !CreateControl(parent, id, pos, size, style, validator, name) )
        return false;

    // ensure that we have correct values for GetLabelsSize()
    m_rangeMin = minValue;
    m_rangeMax = maxValue;

    // create the labels first, so that our DoGetBestSize() could take them
    // into account
    //
    // note that we could simply create 3 wxStaticTexts here but it could
    // result in some observable side effects at wx level (e.g. the parent of
    // wxSlider would have 3 more children than expected) and so we prefer not
    // to do it like this
    if ( m_windowStyle & wxSL_LABELS )
    {
        m_labels = new wxSubwindows(SliderLabel_Last);

        HWND hwndParent = GetHwndOf(parent);
        for ( size_t n = 0; n < SliderLabel_Last; n++ )
        {
            wxWindowIDRef lblid = NewControlId();

            HWND wnd = ::CreateWindow
                         (
                            wxT("STATIC"),
                            NULL,
                            WS_CHILD | WS_VISIBLE | SS_CENTER,
                            0, 0, 0, 0,
                            hwndParent,
                            (HMENU)wxUIntToPtr(lblid.GetValue()),
                            wxGetInstance(),
                            NULL
                         );

            m_labels->Set(n, wnd, lblid);
        }
        m_labels->SetFont(GetFont());
    }

    // now create the main control too
    if ( !MSWCreateControl(TRACKBAR_CLASS, wxEmptyString, pos, size) )
        return false;

    // and initialize everything
    SetRange(minValue, maxValue);
    SetValue(value);
    SetPageSize( wxMax(1, (maxValue - minValue)/10) );

    // we need to position the labels correctly if we have them and if
    // SetSize() hadn't been called before (when best size was determined by
    // MSWCreateControl()) as in this case they haven't been put in place yet
    if ( m_labels && size.x != wxDefaultCoord && size.y != wxDefaultCoord )
    {
        SetSize(size);
    }

    return true;
}

WXDWORD wxSlider::MSWGetStyle(long style, WXDWORD *exstyle) const
{
    WXDWORD msStyle = wxControl::MSWGetStyle(style, exstyle);

    // TBS_HORZ, TBS_RIGHT and TBS_BOTTOM are 0 but do include them for clarity
    msStyle |= style & wxSL_VERTICAL ? TBS_VERT : TBS_HORZ;

    if ( style & wxSL_BOTH )
    {
        // this fully specifies the style combined with TBS_VERT/HORZ above
        msStyle |= TBS_BOTH;
    }
    else // choose one direction
    {
        if ( style & wxSL_LEFT )
            msStyle |= TBS_LEFT;
        else if ( style & wxSL_RIGHT )
            msStyle |= TBS_RIGHT;
        else if ( style & wxSL_TOP )
            msStyle |= TBS_TOP;
        else if ( style & wxSL_BOTTOM )
            msStyle |= TBS_BOTTOM;
    }

    if ( style & wxSL_AUTOTICKS )
        msStyle |= TBS_AUTOTICKS;
    else
        msStyle |= TBS_NOTICKS;

    if ( style & wxSL_SELRANGE )
        msStyle |= TBS_ENABLESELRANGE;

    return msStyle;
}

wxSlider::~wxSlider()
{
    delete m_labels;
}

// ----------------------------------------------------------------------------
// event handling
// ----------------------------------------------------------------------------

bool wxSlider::MSWOnScroll(int WXUNUSED(orientation),
                           WXWORD wParam,
                           WXWORD WXUNUSED(pos),
                           WXHWND control)
{
    wxEventType scrollEvent;
    switch ( wParam )
    {
        case SB_TOP:
            scrollEvent = wxEVT_SCROLL_TOP;
            break;

        case SB_BOTTOM:
            scrollEvent = wxEVT_SCROLL_BOTTOM;
            break;

        case SB_LINEUP:
            scrollEvent = wxEVT_SCROLL_LINEUP;
            break;

        case SB_LINEDOWN:
            scrollEvent = wxEVT_SCROLL_LINEDOWN;
            break;

        case SB_PAGEUP:
            scrollEvent = wxEVT_SCROLL_PAGEUP;
            break;

        case SB_PAGEDOWN:
            scrollEvent = wxEVT_SCROLL_PAGEDOWN;
            break;

        case SB_THUMBTRACK:
            scrollEvent = wxEVT_SCROLL_THUMBTRACK;
            m_isDragging = true;
            break;

        case SB_THUMBPOSITION:
            if ( m_isDragging )
            {
                scrollEvent = wxEVT_SCROLL_THUMBRELEASE;
                m_isDragging = false;
            }
            else
            {
                // this seems to only happen when the mouse wheel is used: in
                // this case, as it might be unexpected to get THUMBRELEASE
                // without preceding THUMBTRACKs, we don't generate it at all
                // but generate CHANGED event because the control itself does
                // not send us SB_ENDSCROLL for whatever reason when mouse
                // wheel is used
                scrollEvent = wxEVT_SCROLL_CHANGED;
            }
            break;

        case SB_ENDSCROLL:
            scrollEvent = wxEVT_SCROLL_CHANGED;
            break;

        default:
            // unknown scroll event?
            return false;
    }

    int newPos = ValueInvertOrNot((int) ::SendMessage((HWND) control, TBM_GETPOS, 0, 0));
    if ( (newPos < GetMin()) || (newPos > GetMax()) )
    {
        // out of range - but we did process it
        return true;
    }

    SetValue(newPos);

    wxScrollEvent event(scrollEvent, m_windowId);
    event.SetPosition(newPos);
    event.SetEventObject( this );
    HandleWindowEvent(event);

    wxCommandEvent cevent( wxEVT_SLIDER, GetId() );
    cevent.SetInt( newPos );
    cevent.SetEventObject( this );

    return HandleWindowEvent( cevent );
}

void wxSlider::Command (wxCommandEvent & event)
{
    SetValue (event.GetInt());
    ProcessCommand (event);
}

// ----------------------------------------------------------------------------
// geometry stuff
// ----------------------------------------------------------------------------

wxRect wxSlider::GetBoundingBox() const
{
    // take care not to call our own functions which would call us recursively
    int x, y, w, h;
    wxSliderBase::DoGetPosition(&x, &y);
    wxSliderBase::DoGetSize(&w, &h);

    wxRect rect(x, y, w, h);
    if ( m_labels )
    {
        wxRect lrect = m_labels->GetBoundingBox();
        GetParent()->ScreenToClient(&lrect.x, &lrect.y);
        rect.Union(lrect);
    }

    return rect;
}

void wxSlider::DoGetSize(int *width, int *height) const
{
    wxRect rect = GetBoundingBox();

    if ( width )
        *width = rect.width;
    if ( height )
        *height = rect.height;
}

void wxSlider::DoGetPosition(int *x, int *y) const
{
    wxRect rect = GetBoundingBox();

    if ( x )
        *x = rect.x;
    if ( y )
        *y = rect.y;
}

int wxSlider::GetLabelsSize(int *widthMin, int *widthMax) const
{
    if ( widthMin && widthMax )
    {
        *widthMin = GetTextExtent(Format(m_rangeMin)).x;
        *widthMax = GetTextExtent(Format(m_rangeMax)).x;

        if ( HasFlag(wxSL_INVERSE) )
        {
            wxSwap(*widthMin, *widthMax);
        }
    }

    return HasFlag(wxSL_LABELS) ? GetCharHeight() : 0;
}

void wxSlider::DoMoveWindow(int x, int y, int width, int height)
{
    // all complications below are because we need to position the labels,
    // without them everything is easy
    if ( !m_labels )
    {
        wxSliderBase::DoMoveWindow(x, y, width, height);
        return;
    }

    int minLabelWidth,
        maxLabelWidth;
    const int labelHeight = GetLabelsSize(&minLabelWidth, &maxLabelWidth);
    const int longestLabelWidth = wxMax(minLabelWidth, maxLabelWidth);
    if ( !HasFlag(wxSL_MIN_MAX_LABELS) )
    {
        minLabelWidth =
        maxLabelWidth = 0;
    }

    int tickOffset = 0;
    if ( HasFlag(wxSL_TICKS))
       tickOffset = TICK;
    if ( HasFlag(wxSL_BOTH))
       tickOffset *= 2;

    // be careful to position the slider itself after moving the labels as
    // otherwise our GetBoundingBox(), which is called from WM_SIZE handler,
    // would return a wrong result and wrong size would be cached internally
    if ( HasFlag(wxSL_VERTICAL) )
    {
        int labelOffset =  0;
        int holdTopX;
        int holdBottomX;
        int xLabel = (wxMax((THUMB + (BORDERPAD * 2)), longestLabelWidth) / 2) -
            (longestLabelWidth / 2) + x;
        if ( HasFlag(wxSL_LEFT) )
        {
            holdTopX = xLabel;
            holdBottomX = xLabel - (abs(maxLabelWidth - minLabelWidth) / 2);
        }
        else // wxSL_RIGHT
        {
            holdTopX = xLabel + longestLabelWidth + (abs(maxLabelWidth - minLabelWidth) / 2);
            holdBottomX = xLabel + longestLabelWidth;

            labelOffset = longestLabelWidth + HGAP;
        }

        if ( HasFlag(wxSL_MIN_MAX_LABELS) )
        {
            if ( HasFlag(wxSL_INVERSE) )
            {
                wxSwap(holdTopX, holdBottomX);
            }

            DoMoveSibling((HWND)(*m_labels)[SliderLabel_Min],
                holdTopX,
                y,
                minLabelWidth, labelHeight);
            DoMoveSibling((HWND)(*m_labels)[SliderLabel_Max],
                holdBottomX,
                y + height - labelHeight,
                maxLabelWidth, labelHeight);
        }

        if ( HasFlag(wxSL_VALUE_LABEL) )
        {
            DoMoveSibling((HWND)(*m_labels)[SliderLabel_Value],
                x + ( HasFlag(wxSL_LEFT) ? THUMB + tickOffset + HGAP : 0 ),
                y + (height - labelHeight)/2,
                longestLabelWidth, labelHeight);
        }

        // position the slider itself along the left/right edge
        wxSliderBase::DoMoveWindow(
            x + labelOffset,
            y + labelHeight,
            THUMB + tickOffset + HGAP,
            height - (labelHeight * 2));
    }
    else // horizontal
    {
        int yLabelMinMax =
            (y + ((THUMB + tickOffset) / 2)) - (labelHeight / 2);
        int xLabelValue =
            x + minLabelWidth +
            ((width  - (minLabelWidth + maxLabelWidth)) / 2) -
            (longestLabelWidth / 2);

        int ySlider = y;

        if ( HasFlag(wxSL_VALUE_LABEL) )
        {
            DoMoveSibling((HWND)(*m_labels)[SliderLabel_Value],
                xLabelValue,
                y + (HasFlag(wxSL_BOTTOM) ? 0 : THUMB + tickOffset),
                longestLabelWidth, labelHeight);

            if ( HasFlag(wxSL_BOTTOM) )
            {
                ySlider += labelHeight;
                yLabelMinMax += labelHeight;
            }
        }

        if ( HasFlag(wxSL_MIN_MAX_LABELS) )
        {
            DoMoveSibling((HWND)(*m_labels)[SliderLabel_Min],
                x,
                yLabelMinMax,
                minLabelWidth, labelHeight);
            DoMoveSibling((HWND)(*m_labels)[SliderLabel_Max],
                x + width - maxLabelWidth,
                yLabelMinMax,
                maxLabelWidth, labelHeight);
        }

        // position the slider itself along the top/bottom edge
        wxSliderBase::DoMoveWindow(
            x + minLabelWidth + VGAP,
            ySlider,
            width  - (minLabelWidth + maxLabelWidth  + (VGAP*2)),
            THUMB + tickOffset);
    }
}

wxSize wxSlider::DoGetBestSize() const
{
    // this value is arbitrary:
    static const int length = 100;

    int *width;
    wxSize size;
    if ( HasFlag(wxSL_VERTICAL) )
    {
        size.x = THUMB;
        size.y = length;
        width = &size.x;

        if ( m_labels )
        {
            int widthMin,
                widthMax;
            int hLabel = GetLabelsSize(&widthMin, &widthMax);

            // account for the labels
            if ( HasFlag(wxSL_MIN_MAX_LABELS) )
                size.x += HGAP + wxMax(widthMin, widthMax);

            // labels are indented relative to the slider itself
            size.y += hLabel;
        }
    }
    else // horizontal
    {
        size.x = length;
        size.y = THUMB;
        width = &size.y;

        if ( m_labels )
        {
            int labelSize = GetLabelsSize();

            // Min/max labels are compensated by the thumb so we don't need
            // extra space for them

            // The value label is always on top of the control and so does need
            // extra space in any case.
            if ( HasFlag(wxSL_VALUE_LABEL) )
                size.y += labelSize;
        }
    }

    // need extra space to show ticks
    if ( HasFlag(wxSL_TICKS) )
    {
        *width += TICK;
        // and maybe twice as much if we show them on both sides
        if ( HasFlag(wxSL_BOTH) )
            *width += TICK;
    }
    return size;
}

// ----------------------------------------------------------------------------
// slider-specific methods
// ----------------------------------------------------------------------------

int wxSlider::GetValue() const
{
    return ValueInvertOrNot(::SendMessage(GetHwnd(), TBM_GETPOS, 0, 0));
}

void wxSlider::SetValue(int value)
{
    ::SendMessage(GetHwnd(), TBM_SETPOS, (WPARAM)TRUE, (LPARAM)ValueInvertOrNot(value));

    if ( m_labels )
    {
        ::SetWindowText((*m_labels)[SliderLabel_Value], Format(value).t_str());
    }
}

void wxSlider::SetRange(int minValue, int maxValue)
{
    // Remember the old logical value if we need to update the physical control
    // value after changing its range in wxSL_INVERSE case (and avoid an
    // unnecessary call to GetValue() otherwise as it's just not needed).
    const int valueOld = HasFlag(wxSL_INVERSE) ? GetValue() : 0;

    m_rangeMin = minValue;
    m_rangeMax = maxValue;

    ::SendMessage(GetHwnd(), TBM_SETRANGEMIN, TRUE, m_rangeMin);
    ::SendMessage(GetHwnd(), TBM_SETRANGEMAX, TRUE, m_rangeMax);

    if ( m_labels )
    {
        ::SetWindowText((*m_labels)[SliderLabel_Min],
                        Format(ValueInvertOrNot(m_rangeMin)).t_str());
        ::SetWindowText((*m_labels)[SliderLabel_Max],
                        Format(ValueInvertOrNot(m_rangeMax)).t_str());
    }

    // When emulating wxSL_INVERSE style in wxWidgets, we need to update the
    // value after changing the range to ensure that the value seen by the user
    // code, i.e. the one returned by GetValue(), does not change.
    if ( HasFlag(wxSL_INVERSE) )
    {
        ::SendMessage(GetHwnd(), TBM_SETPOS, TRUE, ValueInvertOrNot(valueOld));
    }
}

void wxSlider::DoSetTickFreq(int n)
{
    m_tickFreq = n;
    ::SendMessage( GetHwnd(), TBM_SETTICFREQ, (WPARAM) n, (LPARAM) 0 );
}

void wxSlider::SetPageSize(int pageSize)
{
    ::SendMessage( GetHwnd(), TBM_SETPAGESIZE, (WPARAM) 0, (LPARAM) pageSize );
    m_pageSize = pageSize;
}

int wxSlider::GetPageSize() const
{
    return m_pageSize;
}

void wxSlider::ClearSel()
{
    ::SendMessage(GetHwnd(), TBM_CLEARSEL, (WPARAM) TRUE, (LPARAM) 0);
}

void wxSlider::ClearTicks()
{
    ::SendMessage(GetHwnd(), TBM_CLEARTICS, (WPARAM) TRUE, (LPARAM) 0);
}

void wxSlider::SetLineSize(int lineSize)
{
    m_lineSize = lineSize;
    ::SendMessage(GetHwnd(), TBM_SETLINESIZE, (WPARAM) 0, (LPARAM) lineSize);
}

int wxSlider::GetLineSize() const
{
    return (int)::SendMessage(GetHwnd(), TBM_GETLINESIZE, 0, 0);
}

int wxSlider::GetSelEnd() const
{
    return (int)::SendMessage(GetHwnd(), TBM_GETSELEND, 0, 0);
}

int wxSlider::GetSelStart() const
{
    return (int)::SendMessage(GetHwnd(), TBM_GETSELSTART, 0, 0);
}

void wxSlider::SetSelection(int minPos, int maxPos)
{
    ::SendMessage(GetHwnd(), TBM_SETSEL,
                  (WPARAM) TRUE /* redraw */,
                  (LPARAM) MAKELONG( minPos, maxPos) );
}

void wxSlider::SetThumbLength(int len)
{
    ::SendMessage(GetHwnd(), TBM_SETTHUMBLENGTH, (WPARAM) len, (LPARAM) 0);
}

int wxSlider::GetThumbLength() const
{
    return (int)::SendMessage( GetHwnd(), TBM_GETTHUMBLENGTH, 0, 0);
}

void wxSlider::SetTick(int tickPos)
{
    ::SendMessage( GetHwnd(), TBM_SETTIC, (WPARAM) 0, (LPARAM) tickPos );
}

// ----------------------------------------------------------------------------
// composite control methods
// ----------------------------------------------------------------------------

WXHWND wxSlider::GetStaticMin() const
{
    return m_labels ? (WXHWND)(*m_labels)[SliderLabel_Min] : NULL;
}

WXHWND wxSlider::GetStaticMax() const
{
    return m_labels ? (WXHWND)(*m_labels)[SliderLabel_Max] : NULL;
}

WXHWND wxSlider::GetEditValue() const
{
    return m_labels ? (WXHWND)(*m_labels)[SliderLabel_Value] : NULL;
}

WX_FORWARD_STD_METHODS_TO_SUBWINDOWS(wxSlider, wxSliderBase, m_labels)

#endif // wxUSE_SLIDER
