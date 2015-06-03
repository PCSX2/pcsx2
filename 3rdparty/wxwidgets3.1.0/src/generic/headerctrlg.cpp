///////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/headerctrlg.cpp
// Purpose:     generic wxHeaderCtrl implementation
// Author:      Vadim Zeitlin
// Created:     2008-12-03
// Copyright:   (c) 2008 Vadim Zeitlin <vadim@wxwidgets.org>
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

#if wxUSE_HEADERCTRL

#include "wx/headerctrl.h"

#ifdef wxHAS_GENERIC_HEADERCTRL

#include "wx/dcbuffer.h"
#include "wx/renderer.h"

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

namespace
{

const unsigned COL_NONE = (unsigned)-1;

} // anonymous namespace

// ============================================================================
// wxHeaderCtrl implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxHeaderCtrl creation
// ----------------------------------------------------------------------------

void wxHeaderCtrl::Init()
{
    m_numColumns = 0;
    m_hover =
    m_colBeingResized =
    m_colBeingReordered = COL_NONE;
    m_dragOffset = 0;
    m_scrollOffset = 0;
}

bool wxHeaderCtrl::Create(wxWindow *parent,
                          wxWindowID id,
                          const wxPoint& pos,
                          const wxSize& size,
                          long style,
                          const wxString& name)
{
    if ( !wxHeaderCtrlBase::Create(parent, id, pos, size,
                                   style, wxDefaultValidator, name) )
        return false;

    // tell the system to not paint the background at all to avoid flicker as
    // we paint the entire window area in our OnPaint()
    SetBackgroundStyle(wxBG_STYLE_CUSTOM);

    return true;
}

wxHeaderCtrl::~wxHeaderCtrl()
{
}

// ----------------------------------------------------------------------------
// wxHeaderCtrl columns manipulation
// ----------------------------------------------------------------------------

void wxHeaderCtrl::DoSetCount(unsigned int count)
{
    // update the column indices order array before changing m_numColumns
    DoResizeColumnIndices(m_colIndices, count);

    m_numColumns = count;

    InvalidateBestSize();
    Refresh();
}

unsigned int wxHeaderCtrl::DoGetCount() const
{
    return m_numColumns;
}

void wxHeaderCtrl::DoUpdate(unsigned int idx)
{
    InvalidateBestSize();

    // we need to refresh not only this column but also the ones after it in
    // case it was shown or hidden or its width changed -- it would be nice to
    // avoid doing this unnecessary by storing the old column width (TODO)
    RefreshColsAfter(idx);
}

// ----------------------------------------------------------------------------
// wxHeaderCtrl scrolling
// ----------------------------------------------------------------------------

void wxHeaderCtrl::DoScrollHorz(int dx)
{
    m_scrollOffset += dx;

    // don't call our own version which calls this function!
    wxControl::ScrollWindow(dx, 0);
}

// ----------------------------------------------------------------------------
// wxHeaderCtrl geometry
// ----------------------------------------------------------------------------

wxSize wxHeaderCtrl::DoGetBestSize() const
{
    wxWindow *win = GetParent();
    int height = wxRendererNative::Get().GetHeaderButtonHeight( win );

    // the vertical size is rather arbitrary but it looks better if we leave
    // some space around the text
    const wxSize size(IsEmpty() ? wxHeaderCtrlBase::DoGetBestSize().x
                                : GetColEnd(GetColumnCount() - 1),
                                height ); // (7*GetCharHeight())/4);
    CacheBestSize(size);
    return size;
}

int wxHeaderCtrl::GetColStart(unsigned int idx) const
{
    int pos = m_scrollOffset;
    for ( unsigned n = 0; ; n++ )
    {
        const unsigned i = m_colIndices[n];
        if ( i == idx )
            break;

        const wxHeaderColumn& col = GetColumn(i);
        if ( col.IsShown() )
            pos += col.GetWidth();
    }

    return pos;
}

int wxHeaderCtrl::GetColEnd(unsigned int idx) const
{
    int x = GetColStart(idx);

    return x + GetColumn(idx).GetWidth();
}

unsigned int wxHeaderCtrl::FindColumnAtPoint(int x, bool *onSeparator) const
{
    int pos = 0;
    const unsigned count = GetColumnCount();
    for ( unsigned n = 0; n < count; n++ )
    {
        const unsigned idx = m_colIndices[n];
        const wxHeaderColumn& col = GetColumn(idx);
        if ( col.IsHidden() )
            continue;

        pos += col.GetWidth();

        // if the column is resizable, check if we're approximatively over the
        // line separating it from the next column
        //
        // TODO: don't hardcode sensitivity
        if ( col.IsResizeable() && abs(x - pos) < 8 )
        {
            if ( onSeparator )
                *onSeparator = true;
            return idx;
        }

        // inside this column?
        if ( x < pos )
        {
            if ( onSeparator )
                *onSeparator = false;
            return idx;
        }
    }

    if ( onSeparator )
        *onSeparator = false;
    return COL_NONE;
}

// ----------------------------------------------------------------------------
// wxHeaderCtrl repainting
// ----------------------------------------------------------------------------

void wxHeaderCtrl::RefreshCol(unsigned int idx)
{
    wxRect rect = GetClientRect();
    rect.x += GetColStart(idx);
    rect.width = GetColumn(idx).GetWidth();

    RefreshRect(rect);
}

void wxHeaderCtrl::RefreshColIfNotNone(unsigned int idx)
{
    if ( idx != COL_NONE )
        RefreshCol(idx);
}

void wxHeaderCtrl::RefreshColsAfter(unsigned int idx)
{
    wxRect rect = GetClientRect();
    const int ofs = GetColStart(idx);
    rect.x += ofs;
    rect.width -= ofs;

    RefreshRect(rect);
}

// ----------------------------------------------------------------------------
// wxHeaderCtrl dragging/resizing/reordering
// ----------------------------------------------------------------------------

bool wxHeaderCtrl::IsResizing() const
{
    return m_colBeingResized != COL_NONE;
}

bool wxHeaderCtrl::IsReordering() const
{
    return m_colBeingReordered != COL_NONE;
}

void wxHeaderCtrl::ClearMarkers()
{
    wxClientDC dc(this);

    wxDCOverlay dcover(m_overlay, &dc);
    dcover.Clear();
}

void wxHeaderCtrl::EndDragging()
{
    // We currently only use markers for reordering, not for resizing
    if (IsReordering())
    {
        ClearMarkers();
        m_overlay.Reset();
    }

    // don't use the special dragging cursor any more
    SetCursor(wxNullCursor);
}

void wxHeaderCtrl::CancelDragging()
{
    wxASSERT_MSG( IsDragging(),
                  "shouldn't be called if we're not dragging anything" );

    EndDragging();

    unsigned int& col = IsResizing() ? m_colBeingResized : m_colBeingReordered;

    wxHeaderCtrlEvent event(wxEVT_HEADER_DRAGGING_CANCELLED, GetId());
    event.SetEventObject(this);
    event.SetColumn(col);

    GetEventHandler()->ProcessEvent(event);

    col = COL_NONE;
}

int wxHeaderCtrl::ConstrainByMinWidth(unsigned int col, int& xPhysical)
{
    const int xStart = GetColStart(col);

    // notice that GetMinWidth() returns 0 if there is no minimal width so it
    // still makes sense to use it even in this case
    const int xMinEnd = xStart + GetColumn(col).GetMinWidth();

    if ( xPhysical < xMinEnd )
        xPhysical = xMinEnd;

    return xPhysical - xStart;
}

void wxHeaderCtrl::StartOrContinueResizing(unsigned int col, int xPhysical)
{
    wxHeaderCtrlEvent event(IsResizing() ? wxEVT_HEADER_RESIZING
                                         : wxEVT_HEADER_BEGIN_RESIZE,
                            GetId());
    event.SetEventObject(this);
    event.SetColumn(col);

    event.SetWidth(ConstrainByMinWidth(col, xPhysical));

    if ( GetEventHandler()->ProcessEvent(event) && !event.IsAllowed() )
    {
        if ( IsResizing() )
        {
            ReleaseMouse();
            CancelDragging();
        }
        //else: nothing to do -- we just don't start to resize
    }
    else // go ahead with resizing
    {
        if ( !IsResizing() )
        {
            m_colBeingResized = col;
            SetCursor(wxCursor(wxCURSOR_SIZEWE));
            CaptureMouse();
        }
        //else: we had already done the above when we started

    }
}

void wxHeaderCtrl::EndResizing(int xPhysical)
{
    wxASSERT_MSG( IsResizing(), "shouldn't be called if we're not resizing" );

    EndDragging();

    ReleaseMouse();

    wxHeaderCtrlEvent event(wxEVT_HEADER_END_RESIZE, GetId());
    event.SetEventObject(this);
    event.SetColumn(m_colBeingResized);
    event.SetWidth(ConstrainByMinWidth(m_colBeingResized, xPhysical));

    GetEventHandler()->ProcessEvent(event);

    m_colBeingResized = COL_NONE;
}

void wxHeaderCtrl::UpdateReorderingMarker(int xPhysical)
{
    wxClientDC dc(this);

    wxDCOverlay dcover(m_overlay, &dc);
    dcover.Clear();

    dc.SetPen(*wxBLUE);
    dc.SetBrush(*wxTRANSPARENT_BRUSH);

    // draw the phantom position of the column being dragged
    int x = xPhysical - m_dragOffset;
    int y = GetClientSize().y;
    dc.DrawRectangle(x, 0,
                     GetColumn(m_colBeingReordered).GetWidth(), y);

    // and also a hint indicating where it is going to be inserted if it's
    // dropped now
    unsigned int col = FindColumnAtPoint(xPhysical);
    if ( col != COL_NONE )
    {
        static const int DROP_MARKER_WIDTH = 4;

        dc.SetBrush(*wxBLUE);
        dc.DrawRectangle(GetColEnd(col) - DROP_MARKER_WIDTH/2, 0,
                         DROP_MARKER_WIDTH, y);
    }
}

void wxHeaderCtrl::StartReordering(unsigned int col, int xPhysical)
{
    wxHeaderCtrlEvent event(wxEVT_HEADER_BEGIN_REORDER, GetId());
    event.SetEventObject(this);
    event.SetColumn(col);

    if ( GetEventHandler()->ProcessEvent(event) && !event.IsAllowed() )
    {
        // don't start dragging it, nothing to do otherwise
        return;
    }

    m_dragOffset = xPhysical - GetColStart(col);

    m_colBeingReordered = col;
    SetCursor(wxCursor(wxCURSOR_HAND));
    CaptureMouse();

    // do not call UpdateReorderingMarker() here: we don't want to give
    // feedback for reordering until the user starts to really move the mouse
    // as he might want to just click on the column and not move it at all
}

bool wxHeaderCtrl::EndReordering(int xPhysical)
{
    wxASSERT_MSG( IsReordering(), "shouldn't be called if we're not reordering" );

    EndDragging();

    ReleaseMouse();

    const int colOld = m_colBeingReordered,
              colNew = FindColumnAtPoint(xPhysical);

    m_colBeingReordered = COL_NONE;

    if ( xPhysical - GetColStart(colOld) == m_dragOffset )
        return false;

    if ( colNew != colOld )
    {
        wxHeaderCtrlEvent event(wxEVT_HEADER_END_REORDER, GetId());
        event.SetEventObject(this);
        event.SetColumn(colOld);

        const unsigned pos = GetColumnPos(FindColumnAtPoint(xPhysical));
        event.SetNewOrder(pos);

        if ( !GetEventHandler()->ProcessEvent(event) || event.IsAllowed() )
        {
            // do reorder the columns
            DoMoveCol(colOld, pos);
        }
    }

    // whether we moved the column or not, the user did move the mouse and so
    // did try to do it so return true
    return true;
}

// ----------------------------------------------------------------------------
// wxHeaderCtrl column reordering
// ----------------------------------------------------------------------------

void wxHeaderCtrl::DoSetColumnsOrder(const wxArrayInt& order)
{
    m_colIndices = order;
    Refresh();
}

wxArrayInt wxHeaderCtrl::DoGetColumnsOrder() const
{
    return m_colIndices;
}

void wxHeaderCtrl::DoMoveCol(unsigned int idx, unsigned int pos)
{
    MoveColumnInOrderArray(m_colIndices, idx, pos);

    Refresh();
}

// ----------------------------------------------------------------------------
// wxHeaderCtrl event handlers
// ----------------------------------------------------------------------------

wxBEGIN_EVENT_TABLE(wxHeaderCtrl, wxHeaderCtrlBase)
    EVT_PAINT(wxHeaderCtrl::OnPaint)

    EVT_MOUSE_EVENTS(wxHeaderCtrl::OnMouse)

    EVT_MOUSE_CAPTURE_LOST(wxHeaderCtrl::OnCaptureLost)

    EVT_KEY_DOWN(wxHeaderCtrl::OnKeyDown)
wxEND_EVENT_TABLE()

void wxHeaderCtrl::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    int w, h;
    GetClientSize(&w, &h);

#ifdef __WXGTK__
//    int vw;
//    GetVirtualSize(&vw, NULL);
#endif

    wxAutoBufferedPaintDC dc(this);

    dc.SetBackground(GetBackgroundColour());
    dc.Clear();

    // account for the horizontal scrollbar offset in the parent window
    dc.SetDeviceOrigin(m_scrollOffset, 0);

    const unsigned int count = m_numColumns;
    int xpos = 0;
    for ( unsigned int i = 0; i < count; i++ )
    {
        const unsigned idx = m_colIndices[i];
        const wxHeaderColumn& col = GetColumn(idx);
        if ( col.IsHidden() )
            continue;

        int colWidth = col.GetWidth();

        wxHeaderSortIconType sortArrow;
        if ( col.IsSortKey() )
        {
            sortArrow = col.IsSortOrderAscending() ? wxHDR_SORT_ICON_UP
                                                   : wxHDR_SORT_ICON_DOWN;
        }
        else // not sorting by this column
        {
            sortArrow = wxHDR_SORT_ICON_NONE;
        }

        int state = 0;
        if ( IsEnabled() )
        {
            if ( idx == m_hover )
                state = wxCONTROL_CURRENT;
        }
        else // disabled
        {
            state = wxCONTROL_DISABLED;
        }

        if (i == 0)
           state |= wxCONTROL_SPECIAL;

        wxHeaderButtonParams params;
        params.m_labelText = col.GetTitle();
        params.m_labelBitmap = col.GetBitmap();
        params.m_labelAlignment = col.GetAlignment();

#ifdef __WXGTK__
        if (i == count-1)
        {
//            colWidth = wxMax( colWidth, vw - xpos );
            state |= wxCONTROL_DIRTY;
        }
#endif

        wxRendererNative::Get().DrawHeaderButton
                                (
                                    this,
                                    dc,
                                    wxRect(xpos, 0, colWidth, h),
                                    state,
                                    sortArrow,
                                    &params
                                );

        xpos += colWidth;
    }
}

void wxHeaderCtrl::OnCaptureLost(wxMouseCaptureLostEvent& WXUNUSED(event))
{
    if ( IsDragging() )
        CancelDragging();
}

void wxHeaderCtrl::OnKeyDown(wxKeyEvent& event)
{
    if ( event.GetKeyCode() == WXK_ESCAPE )
    {
        if ( IsDragging() )
        {
            ReleaseMouse();
            CancelDragging();

            return;
        }
    }

    event.Skip();
}

void wxHeaderCtrl::OnMouse(wxMouseEvent& mevent)
{
    // do this in advance to allow simply returning if we're not interested,
    // we'll undo it if we do handle the event below
    mevent.Skip();


    // account for the control displacement
    const int xPhysical = mevent.GetX();
    const int xLogical = xPhysical - m_scrollOffset;

    // first deal with the [continuation of any] dragging operations in
    // progress
    if ( IsResizing() )
    {
        if ( mevent.LeftUp() )
            EndResizing(xPhysical);
        else // update the live separator position
            StartOrContinueResizing(m_colBeingResized, xPhysical);

        return;
    }

    if ( IsReordering() )
    {
        if ( !mevent.LeftUp() )
        {
            // update the column position
            UpdateReorderingMarker(xPhysical);

            return;
        }

        // finish reordering and continue to generate a click event below if we
        // didn't really reorder anything
        if ( EndReordering(xPhysical) )
            return;
    }


    // find if the event is over a column at all
    bool onSeparator;
    const unsigned col = mevent.Leaving()
                            ? (onSeparator = false, COL_NONE)
                            : FindColumnAtPoint(xLogical, &onSeparator);


    // update the highlighted column if it changed
    if ( col != m_hover )
    {
        const unsigned hoverOld = m_hover;
        m_hover = col;

        RefreshColIfNotNone(hoverOld);
        RefreshColIfNotNone(m_hover);
    }

    // update mouse cursor as it moves around
    if ( mevent.Moving() )
    {
        SetCursor(onSeparator ? wxCursor(wxCURSOR_SIZEWE) : wxNullCursor);
        return;
    }

    // all the other events only make sense when they happen over a column
    if ( col == COL_NONE )
        return;


    // enter various dragging modes on left mouse press
    if ( mevent.LeftDown() )
    {
        if ( onSeparator )
        {
            // start resizing the column
            wxASSERT_MSG( !IsResizing(), "reentering column resize mode?" );
            StartOrContinueResizing(col, xPhysical);
        }
        else // on column itself
        {
            // start dragging the column
            wxASSERT_MSG( !IsReordering(), "reentering column move mode?" );

            StartReordering(col, xPhysical);
        }

        return;
    }

    // determine the type of header event corresponding to click events
    wxEventType evtType = wxEVT_NULL;
    const bool click = mevent.ButtonUp(),
               dblclk = mevent.ButtonDClick();
    if ( click || dblclk )
    {
        switch ( mevent.GetButton() )
        {
            case wxMOUSE_BTN_LEFT:
                // treat left double clicks on separator specially
                if ( onSeparator && dblclk )
                {
                    evtType = wxEVT_HEADER_SEPARATOR_DCLICK;
                }
                else // not double click on separator
                {
                    evtType = click ? wxEVT_HEADER_CLICK
                                    : wxEVT_HEADER_DCLICK;
                }
                break;

            case wxMOUSE_BTN_RIGHT:
                evtType = click ? wxEVT_HEADER_RIGHT_CLICK
                                : wxEVT_HEADER_RIGHT_DCLICK;
                break;

            case wxMOUSE_BTN_MIDDLE:
                evtType = click ? wxEVT_HEADER_MIDDLE_CLICK
                                : wxEVT_HEADER_MIDDLE_DCLICK;
                break;

            default:
                // ignore clicks from other mouse buttons
                ;
        }
    }

    if ( evtType == wxEVT_NULL )
        return;

    wxHeaderCtrlEvent event(evtType, GetId());
    event.SetEventObject(this);
    event.SetColumn(col);

    if ( GetEventHandler()->ProcessEvent(event) )
        mevent.Skip(false);
}

#endif // wxHAS_GENERIC_HEADERCTRL

#endif // wxUSE_HEADERCTRL
