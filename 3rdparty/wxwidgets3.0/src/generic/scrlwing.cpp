/////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/scrlwing.cpp
// Purpose:     wxScrolledWindow implementation
// Author:      Julian Smart
// Modified by: Vadim Zeitlin on 31.08.00: wxScrollHelper allows to implement.
//              Ron Lee on 10.4.02:  virtual size / auto scrollbars et al.
// Created:     01/02/97
// Copyright:   (c) wxWidgets team
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

#include "wx/scrolwin.h"

#ifndef WX_PRECOMP
    #include "wx/utils.h"
    #include "wx/panel.h"
    #include "wx/dcclient.h"
    #include "wx/timer.h"
    #include "wx/sizer.h"
    #include "wx/settings.h"
#endif

#ifdef __WXMAC__
#include "wx/scrolbar.h"
#endif

#ifdef __WXMSW__
    #include <windows.h> // for DLGC_WANTARROWS
    #include "wx/msw/winundef.h"
#endif

#ifdef __WXMOTIF__
// For wxRETAINED implementation
#ifdef __VMS__ //VMS's Xm.h is not (yet) compatible with C++
               //This code switches off the compiler warnings
# pragma message disable nosimpint
#endif
#include <Xm/Xm.h>
#ifdef __VMS__
# pragma message enable nosimpint
#endif
#endif

typedef wxVector<wxScrollHelper *> wxScrollHelperRecGuardFlags;
class wxScrollHelperRecGuard
{
public:
    wxScrollHelperRecGuard(wxScrollHelper *instance, wxScrollHelperRecGuardFlags& flags)
        : m_flags(flags), m_instance(instance), m_inside(false)
    {
        // Determine if our instance is already inside a guard
        for ( wxScrollHelperRecGuardFlags::iterator iter = flags.begin();
              iter != flags.end();
              ++iter )
        {
            if ( *iter == instance )
            {
                m_inside = true;
                return;
            }
        }

        // Not inside so add it to the back
        flags.push_back(instance);
    }

    ~wxScrollHelperRecGuard()
    {
        if ( IsInside() )
            return;

        for ( wxScrollHelperRecGuardFlags::iterator iter = m_flags.begin();
              iter != m_flags.end();
              ++iter )
        {
            if ( *iter == m_instance )
            {
                m_flags.erase(iter);
                break;
            }
        }
    }

    bool IsInside() const { return m_inside; }

private:
    wxScrollHelperRecGuardFlags& m_flags;
    wxScrollHelper* m_instance;

    // true if the flag had been already set when we were created
    bool m_inside;

    wxDECLARE_NO_COPY_CLASS(wxScrollHelperRecGuard);
};

/*
    TODO PROPERTIES
        style wxHSCROLL | wxVSCROLL
*/

// ----------------------------------------------------------------------------
// wxScrollHelperEvtHandler: intercept the events from the window and forward
// them to wxScrollHelper
// ----------------------------------------------------------------------------

class WXDLLEXPORT wxScrollHelperEvtHandler : public wxEvtHandler
{
public:
    wxScrollHelperEvtHandler(wxScrollHelperBase *scrollHelper)
    {
        m_scrollHelper = scrollHelper;
    }

    virtual bool ProcessEvent(wxEvent& event);

private:
    wxScrollHelperBase *m_scrollHelper;

    wxDECLARE_NO_COPY_CLASS(wxScrollHelperEvtHandler);
};

#if wxUSE_TIMER
// ----------------------------------------------------------------------------
// wxAutoScrollTimer: the timer used to generate a stream of scroll events when
// a captured mouse is held outside the window
// ----------------------------------------------------------------------------

class wxAutoScrollTimer : public wxTimer
{
public:
    wxAutoScrollTimer(wxWindow *winToScroll,
                      wxScrollHelperBase *scroll,
                      wxEventType eventTypeToSend,
                      int pos, int orient);

    virtual void Notify();

private:
    wxWindow *m_win;
    wxScrollHelperBase *m_scrollHelper;
    wxEventType m_eventType;
    int m_pos,
        m_orient;

    wxDECLARE_NO_COPY_CLASS(wxAutoScrollTimer);
};

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxAutoScrollTimer
// ----------------------------------------------------------------------------

wxAutoScrollTimer::wxAutoScrollTimer(wxWindow *winToScroll,
                                     wxScrollHelperBase *scroll,
                                     wxEventType eventTypeToSend,
                                     int pos, int orient)
{
    m_win = winToScroll;
    m_scrollHelper = scroll;
    m_eventType = eventTypeToSend;
    m_pos = pos;
    m_orient = orient;
}

void wxAutoScrollTimer::Notify()
{
    // only do all this as long as the window is capturing the mouse
    if ( wxWindow::GetCapture() != m_win )
    {
        Stop();
    }
    else // we still capture the mouse, continue generating events
    {
        // first scroll the window if we are allowed to do it
        wxScrollWinEvent event1(m_eventType, m_pos, m_orient);
        event1.SetEventObject(m_win);
        event1.SetId(m_win->GetId());
        if ( m_scrollHelper->SendAutoScrollEvents(event1) &&
                m_win->GetEventHandler()->ProcessEvent(event1) )
        {
            // and then send a pseudo mouse-move event to refresh the selection
            wxMouseEvent event2(wxEVT_MOTION);
            event2.SetPosition(wxGetMousePosition());

            // the mouse event coordinates should be client, not screen as
            // returned by wxGetMousePosition
            wxWindow *parentTop = m_win;
            while ( parentTop->GetParent() )
                parentTop = parentTop->GetParent();
            wxPoint ptOrig = parentTop->GetPosition();
            event2.m_x -= ptOrig.x;
            event2.m_y -= ptOrig.y;

            event2.SetEventObject(m_win);

            wxMouseState mouseState = wxGetMouseState();

            event2.m_leftDown = mouseState.LeftIsDown();
            event2.m_middleDown = mouseState.MiddleIsDown();
            event2.m_rightDown = mouseState.RightIsDown();

            event2.m_shiftDown = mouseState.ShiftDown();
            event2.m_controlDown = mouseState.ControlDown();
            event2.m_altDown = mouseState.AltDown();
            event2.m_metaDown = mouseState.MetaDown();

            m_win->GetEventHandler()->ProcessEvent(event2);
        }
        else // can't scroll further, stop
        {
            Stop();
        }
    }
}
#endif

// ----------------------------------------------------------------------------
// wxScrollHelperEvtHandler
// ----------------------------------------------------------------------------

// Notice that this method is currently duplicated in the method with the same
// name in wxVarScrollHelperEvtHandler class, until this is fixed, the other
// copy of the method needs to be modified every time this version is.
bool wxScrollHelperEvtHandler::ProcessEvent(wxEvent& event)
{
    wxEventType evType = event.GetEventType();

    // Pass it on to the real handler: notice that we must not call
    // ProcessEvent() on this object itself as it wouldn't pass it to the next
    // handler (i.e. the real window) if we're called from a previous handler
    // (as indicated by "process here only" flag being set) and we do want to
    // execute the handler defined in the window we're associated with right
    // now, without waiting until TryAfter() is called from wxEvtHandler.
    bool processed = m_nextHandler->ProcessEvent(event);

    // always process the size events ourselves, even if the user code handles
    // them as well, as we need to AdjustScrollbars()
    //
    // NB: it is important to do it after processing the event in the normal
    //     way as HandleOnSize() may generate a wxEVT_SIZE itself if the
    //     scrollbar[s] (dis)appear and it should be seen by the user code
    //     after this one
    if ( evType == wxEVT_SIZE )
    {
        m_scrollHelper->HandleOnSize((wxSizeEvent &)event);
        return true;
    }

    // For wxEVT_PAINT the user code can either handle this event as usual or
    // override virtual OnDraw(), so if the event hasn't been handled we need
    // to call this virtual function ourselves.
    if (
#ifndef __WXUNIVERSAL__
          // in wxUniversal "processed" will always be true, because
          // all windows use the paint event to draw themselves.
          // In this case we can't use this flag to determine if a custom
          // paint event handler already drew our window and we just
          // call OnDraw() anyway.
          !processed &&
#endif // !__WXUNIVERSAL__
            evType == wxEVT_PAINT )
    {
        m_scrollHelper->HandleOnPaint((wxPaintEvent &)event);
        return true;
    }

    // If the user code handled this event, it should prevent the default
    // handling from taking place, so don't do anything else in this case.
    if ( processed )
        return true;

    if ( evType == wxEVT_CHILD_FOCUS )
    {
        m_scrollHelper->HandleOnChildFocus((wxChildFocusEvent &)event);
        return true;
    }

    // reset the skipped flag (which might have been set to true in
    // ProcessEvent() above) to be able to test it below
    bool wasSkipped = event.GetSkipped();
    if ( wasSkipped )
        event.Skip(false);

    if ( evType == wxEVT_SCROLLWIN_TOP ||
         evType == wxEVT_SCROLLWIN_BOTTOM ||
         evType == wxEVT_SCROLLWIN_LINEUP ||
         evType == wxEVT_SCROLLWIN_LINEDOWN ||
         evType == wxEVT_SCROLLWIN_PAGEUP ||
         evType == wxEVT_SCROLLWIN_PAGEDOWN ||
         evType == wxEVT_SCROLLWIN_THUMBTRACK ||
         evType == wxEVT_SCROLLWIN_THUMBRELEASE )
    {
        m_scrollHelper->HandleOnScroll((wxScrollWinEvent &)event);
        if ( !event.GetSkipped() )
        {
            // it makes sense to indicate that we processed the message as we
            // did scroll the window (and also notice that wxAutoScrollTimer
            // relies on our return value to stop scrolling when we are at top
            // or bottom already)
            processed = true;
            wasSkipped = false;
        }
    }

    if ( evType == wxEVT_ENTER_WINDOW )
    {
        m_scrollHelper->HandleOnMouseEnter((wxMouseEvent &)event);
    }
    else if ( evType == wxEVT_LEAVE_WINDOW )
    {
        m_scrollHelper->HandleOnMouseLeave((wxMouseEvent &)event);
    }
#if wxUSE_MOUSEWHEEL
    // Use GTK's own scroll wheel handling in GtkScrolledWindow
#ifndef __WXGTK20__
    else if ( evType == wxEVT_MOUSEWHEEL )
    {
        m_scrollHelper->HandleOnMouseWheel((wxMouseEvent &)event);
        return true;
    }
#endif
#endif // wxUSE_MOUSEWHEEL
    else if ( evType == wxEVT_CHAR )
    {
        m_scrollHelper->HandleOnChar((wxKeyEvent &)event);
        if ( !event.GetSkipped() )
        {
            processed = true;
            wasSkipped = false;
        }
    }

    event.Skip(wasSkipped);

    // We called ProcessEvent() on the next handler, meaning that we explicitly
    // worked around the request to process the event in this handler only. As
    // explained above, this is unfortunately really necessary but the trouble
    // is that the event will continue to be post-processed by the previous
    // handler resulting in duplicate calls to event handlers. Call the special
    // function below to prevent this from happening, base class DoTryChain()
    // will check for it and behave accordingly.
    //
    // And if we're not called from DoTryChain(), this won't do anything anyhow.
    event.DidntHonourProcessOnlyIn();

    return processed;
}

// ============================================================================
// wxAnyScrollHelperBase and wxScrollHelperBase implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxAnyScrollHelperBase
// ----------------------------------------------------------------------------

wxAnyScrollHelperBase::wxAnyScrollHelperBase(wxWindow* win)
{
    wxASSERT_MSG( win, wxT("associated window can't be NULL in wxScrollHelper") );

    m_win = win;
    m_targetWindow = NULL;

    m_kbdScrollingEnabled = true;
}

// ----------------------------------------------------------------------------
// wxScrollHelperBase construction
// ----------------------------------------------------------------------------

wxScrollHelperBase::wxScrollHelperBase(wxWindow *win)
    : wxAnyScrollHelperBase(win)
{
    m_xScrollPixelsPerLine =
    m_yScrollPixelsPerLine =
    m_xScrollPosition =
    m_yScrollPosition =
    m_xScrollLines =
    m_yScrollLines =
    m_xScrollLinesPerPage =
    m_yScrollLinesPerPage = 0;

    m_xScrollingEnabled =
    m_yScrollingEnabled = true;

    m_scaleX =
    m_scaleY = 1.0;
#if wxUSE_MOUSEWHEEL
    m_wheelRotation = 0;
#endif

    m_timerAutoScroll = NULL;

    m_handler = NULL;

    m_win->SetScrollHelper(static_cast<wxScrollHelper *>(this));

    // by default, the associated window is also the target window
    DoSetTargetWindow(win);
}

wxScrollHelperBase::~wxScrollHelperBase()
{
    StopAutoScrolling();

    DeleteEvtHandler();
}

// ----------------------------------------------------------------------------
// setting scrolling parameters
// ----------------------------------------------------------------------------

void wxScrollHelperBase::SetScrollbars(int pixelsPerUnitX,
                                       int pixelsPerUnitY,
                                       int noUnitsX,
                                       int noUnitsY,
                                       int xPos,
                                       int yPos,
                                       bool noRefresh)
{
    // Convert positions expressed in scroll units to positions in pixels.
    int xPosInPixels = (xPos + m_xScrollPosition)*m_xScrollPixelsPerLine,
        yPosInPixels = (yPos + m_yScrollPosition)*m_yScrollPixelsPerLine;

    bool do_refresh =
    (
      (noUnitsX != 0 && m_xScrollLines == 0) ||
      (noUnitsX < m_xScrollLines && xPosInPixels > pixelsPerUnitX * noUnitsX) ||

      (noUnitsY != 0 && m_yScrollLines == 0) ||
      (noUnitsY < m_yScrollLines && yPosInPixels > pixelsPerUnitY * noUnitsY) ||
      (xPos != m_xScrollPosition) ||
      (yPos != m_yScrollPosition)
    );

    m_xScrollPixelsPerLine = pixelsPerUnitX;
    m_yScrollPixelsPerLine = pixelsPerUnitY;
    m_xScrollPosition = xPos;
    m_yScrollPosition = yPos;

    int w = noUnitsX * pixelsPerUnitX;
    int h = noUnitsY * pixelsPerUnitY;

    // For better backward compatibility we set persisting limits
    // here not just the size.  It makes SetScrollbars 'sticky'
    // emulating the old non-autoscroll behaviour.
    //   m_targetWindow->SetVirtualSizeHints( w, h );

    // The above should arguably be deprecated, this however we still need.

    // take care not to set 0 virtual size, 0 means that we don't have any
    // scrollbars and hence we should use the real size instead of the virtual
    // one which is indicated by using wxDefaultCoord
    m_targetWindow->SetVirtualSize( w ? w : wxDefaultCoord,
                                    h ? h : wxDefaultCoord);

    if (do_refresh && !noRefresh)
        m_targetWindow->Refresh(true, GetScrollRect());

#ifndef __WXUNIVERSAL__
    // If the target is not the same as the window with the scrollbars,
    // then we need to update the scrollbars here, since they won't have
    // been updated by SetVirtualSize().
    if ( m_targetWindow != m_win )
#endif // !__WXUNIVERSAL__
    {
        AdjustScrollbars();
    }
#ifndef __WXUNIVERSAL__
    else
    {
        // otherwise this has been done by AdjustScrollbars, above
    }
#endif // !__WXUNIVERSAL__
}

// ----------------------------------------------------------------------------
// [target] window handling
// ----------------------------------------------------------------------------

void wxScrollHelperBase::DeleteEvtHandler()
{
    // search for m_handler in the handler list
    if ( m_win && m_handler )
    {
        if ( m_win->RemoveEventHandler(m_handler) )
        {
            delete m_handler;
        }
        //else: something is very wrong, so better [maybe] leak memory than
        //      risk a crash because of double deletion

        m_handler = NULL;
    }
}

void wxScrollHelperBase::DoSetTargetWindow(wxWindow *target)
{
    m_targetWindow = target;
#ifdef __WXMAC__
    target->MacSetClipChildren( true ) ;
#endif

    // install the event handler which will intercept the events we're
    // interested in (but only do it for our real window, not the target window
    // which we scroll - we don't need to hijack its events)
    if ( m_targetWindow == m_win )
    {
        // if we already have a handler, delete it first
        DeleteEvtHandler();

        m_handler = new wxScrollHelperEvtHandler(this);
        m_targetWindow->PushEventHandler(m_handler);
    }
}

void wxScrollHelperBase::SetTargetWindow(wxWindow *target)
{
    wxCHECK_RET( target, wxT("target window must not be NULL") );

    if ( target == m_targetWindow )
        return;

    DoSetTargetWindow(target);
}

// ----------------------------------------------------------------------------
// scrolling implementation itself
// ----------------------------------------------------------------------------

void wxScrollHelperBase::HandleOnScroll(wxScrollWinEvent& event)
{
    int nScrollInc = CalcScrollInc(event);
    if ( nScrollInc == 0 )
    {
        // can't scroll further
        event.Skip();

        return;
    }

    bool needsRefresh = false;
    int dx = 0,
        dy = 0;
    int orient = event.GetOrientation();
    if (orient == wxHORIZONTAL)
    {
       if ( m_xScrollingEnabled )
       {
           dx = -m_xScrollPixelsPerLine * nScrollInc;
       }
       else
       {
           needsRefresh = true;
       }
    }
    else
    {
        if ( m_yScrollingEnabled )
        {
            dy = -m_yScrollPixelsPerLine * nScrollInc;
        }
        else
        {
            needsRefresh = true;
        }
    }

    if ( !needsRefresh )
    {
        // flush all pending repaints before we change m_{x,y}ScrollPosition, as
        // otherwise invalidated area could be updated incorrectly later when
        // ScrollWindow() makes sure they're repainted before scrolling them
#ifdef __WXMAC__
        // wxWindowMac is taking care of making sure the update area is correctly
        // set up, while not forcing an immediate redraw
#else
        m_targetWindow->Update();
#endif
    }

    if (orient == wxHORIZONTAL)
    {
        m_xScrollPosition += nScrollInc;
        m_win->SetScrollPos(wxHORIZONTAL, m_xScrollPosition);
    }
    else
    {
        m_yScrollPosition += nScrollInc;
        m_win->SetScrollPos(wxVERTICAL, m_yScrollPosition);
    }

    if ( needsRefresh )
    {
        m_targetWindow->Refresh(true, GetScrollRect());
    }
    else
    {
        m_targetWindow->ScrollWindow(dx, dy, GetScrollRect());
    }
}

int wxScrollHelperBase::CalcScrollInc(wxScrollWinEvent& event)
{
    int pos = event.GetPosition();
    int orient = event.GetOrientation();

    int nScrollInc = 0;
    if (event.GetEventType() == wxEVT_SCROLLWIN_TOP)
    {
            if (orient == wxHORIZONTAL)
                nScrollInc = - m_xScrollPosition;
            else
                nScrollInc = - m_yScrollPosition;
    } else
    if (event.GetEventType() == wxEVT_SCROLLWIN_BOTTOM)
    {
            if (orient == wxHORIZONTAL)
                nScrollInc = m_xScrollLines - m_xScrollPosition;
            else
                nScrollInc = m_yScrollLines - m_yScrollPosition;
    } else
    if (event.GetEventType() == wxEVT_SCROLLWIN_LINEUP)
    {
            nScrollInc = -1;
    } else
    if (event.GetEventType() == wxEVT_SCROLLWIN_LINEDOWN)
    {
            nScrollInc = 1;
    } else
    if (event.GetEventType() == wxEVT_SCROLLWIN_PAGEUP)
    {
            if (orient == wxHORIZONTAL)
                nScrollInc = -GetScrollPageSize(wxHORIZONTAL);
            else
                nScrollInc = -GetScrollPageSize(wxVERTICAL);
    } else
    if (event.GetEventType() == wxEVT_SCROLLWIN_PAGEDOWN)
    {
            if (orient == wxHORIZONTAL)
                nScrollInc = GetScrollPageSize(wxHORIZONTAL);
            else
                nScrollInc = GetScrollPageSize(wxVERTICAL);
    } else
    if ((event.GetEventType() == wxEVT_SCROLLWIN_THUMBTRACK) ||
        (event.GetEventType() == wxEVT_SCROLLWIN_THUMBRELEASE))
    {
            if (orient == wxHORIZONTAL)
                nScrollInc = pos - m_xScrollPosition;
            else
                nScrollInc = pos - m_yScrollPosition;
    }

    if (orient == wxHORIZONTAL)
    {
        if ( m_xScrollPosition + nScrollInc < 0 )
        {
            // As -ve as we can go
            nScrollInc = -m_xScrollPosition;
        }
        else // check for the other bound
        {
            const int posMax = m_xScrollLines - m_xScrollLinesPerPage;
            if ( m_xScrollPosition + nScrollInc > posMax )
            {
                // As +ve as we can go
                nScrollInc = posMax - m_xScrollPosition;
            }
        }
    }
    else // wxVERTICAL
    {
        if ( m_yScrollPosition + nScrollInc < 0 )
        {
            // As -ve as we can go
            nScrollInc = -m_yScrollPosition;
        }
        else // check for the other bound
        {
            const int posMax = m_yScrollLines - m_yScrollLinesPerPage;
            if ( m_yScrollPosition + nScrollInc > posMax )
            {
                // As +ve as we can go
                nScrollInc = posMax - m_yScrollPosition;
            }
        }
    }

    return nScrollInc;
}

void wxScrollHelperBase::DoPrepareDC(wxDC& dc)
{
    wxPoint pt = dc.GetDeviceOrigin();
#ifdef __WXGTK__
    // It may actually be correct to always query
    // the m_sign from the DC here, but I leave the
    // #ifdef GTK for now.
    if (m_win->GetLayoutDirection() == wxLayout_RightToLeft)
        dc.SetDeviceOrigin( pt.x + m_xScrollPosition * m_xScrollPixelsPerLine,
                            pt.y - m_yScrollPosition * m_yScrollPixelsPerLine );
    else
#endif
        dc.SetDeviceOrigin( pt.x - m_xScrollPosition * m_xScrollPixelsPerLine,
                            pt.y - m_yScrollPosition * m_yScrollPixelsPerLine );
    dc.SetUserScale( m_scaleX, m_scaleY );
}

void wxScrollHelperBase::SetScrollRate( int xstep, int ystep )
{
    int old_x = m_xScrollPixelsPerLine * m_xScrollPosition;
    int old_y = m_yScrollPixelsPerLine * m_yScrollPosition;

    m_xScrollPixelsPerLine = xstep;
    m_yScrollPixelsPerLine = ystep;

    int new_x = m_xScrollPixelsPerLine * m_xScrollPosition;
    int new_y = m_yScrollPixelsPerLine * m_yScrollPosition;

    m_win->SetScrollPos( wxHORIZONTAL, m_xScrollPosition );
    m_win->SetScrollPos( wxVERTICAL, m_yScrollPosition );
    m_targetWindow->ScrollWindow( old_x - new_x, old_y - new_y );

    AdjustScrollbars();
}

void wxScrollHelperBase::GetScrollPixelsPerUnit (int *x_unit, int *y_unit) const
{
    if ( x_unit )
        *x_unit = m_xScrollPixelsPerLine;
    if ( y_unit )
        *y_unit = m_yScrollPixelsPerLine;
}


int wxScrollHelperBase::GetScrollLines( int orient ) const
{
    if ( orient == wxHORIZONTAL )
        return m_xScrollLines;
    else
        return m_yScrollLines;
}

int wxScrollHelperBase::GetScrollPageSize(int orient) const
{
    if ( orient == wxHORIZONTAL )
        return m_xScrollLinesPerPage;
    else
        return m_yScrollLinesPerPage;
}

void wxScrollHelperBase::SetScrollPageSize(int orient, int pageSize)
{
    if ( orient == wxHORIZONTAL )
        m_xScrollLinesPerPage = pageSize;
    else
        m_yScrollLinesPerPage = pageSize;
}

void wxScrollHelperBase::EnableScrolling (bool x_scroll, bool y_scroll)
{
    m_xScrollingEnabled = x_scroll;
    m_yScrollingEnabled = y_scroll;
}

// Where the current view starts from
void wxScrollHelperBase::DoGetViewStart (int *x, int *y) const
{
    if ( x )
        *x = m_xScrollPosition;
    if ( y )
        *y = m_yScrollPosition;
}

void wxScrollHelperBase::DoCalcScrolledPosition(int x, int y,
                                                int *xx, int *yy) const
{
    if ( xx )
        *xx = x - m_xScrollPosition * m_xScrollPixelsPerLine;
    if ( yy )
        *yy = y - m_yScrollPosition * m_yScrollPixelsPerLine;
}

void wxScrollHelperBase::DoCalcUnscrolledPosition(int x, int y,
                                                  int *xx, int *yy) const
{
    if ( xx )
        *xx = x + m_xScrollPosition * m_xScrollPixelsPerLine;
    if ( yy )
        *yy = y + m_yScrollPosition * m_yScrollPixelsPerLine;
}

// ----------------------------------------------------------------------------
// geometry
// ----------------------------------------------------------------------------

bool wxScrollHelperBase::ScrollLayout()
{
    if ( m_win->GetSizer() && m_targetWindow == m_win )
    {
        // If we're the scroll target, take into account the
        // virtual size and scrolled position of the window.

        int x = 0, y = 0, w = 0, h = 0;
        CalcScrolledPosition(0,0, &x,&y);
        m_win->GetVirtualSize(&w, &h);
        m_win->GetSizer()->SetDimension(x, y, w, h);
        return true;
    }

    // fall back to default for LayoutConstraints
    return m_win->wxWindow::Layout();
}

void wxScrollHelperBase::ScrollDoSetVirtualSize(int x, int y)
{
    m_win->wxWindow::DoSetVirtualSize( x, y );
    AdjustScrollbars();

    if (m_win->GetAutoLayout())
        m_win->Layout();
}

// wxWindow's GetBestVirtualSize returns the actual window size,
// whereas we want to return the virtual size
wxSize wxScrollHelperBase::ScrollGetBestVirtualSize() const
{
    wxSize clientSize(m_win->GetClientSize());
    if ( m_win->GetSizer() )
        clientSize.IncTo(m_win->GetSizer()->CalcMin());

    return clientSize;
}

// ----------------------------------------------------------------------------
// event handlers
// ----------------------------------------------------------------------------

// Default OnSize resets scrollbars, if any
void wxScrollHelperBase::HandleOnSize(wxSizeEvent& WXUNUSED(event))
{
    if ( m_targetWindow->GetAutoLayout() )
    {
        wxSize size = m_targetWindow->GetBestVirtualSize();

        // This will call ::Layout() and ::AdjustScrollbars()
        m_win->SetVirtualSize( size );
    }
    else
    {
        AdjustScrollbars();
    }
}

// This calls OnDraw, having adjusted the origin according to the current
// scroll position
void wxAnyScrollHelperBase::HandleOnPaint(wxPaintEvent& WXUNUSED(event))
{
    // don't use m_targetWindow here, this is always called for ourselves
    wxPaintDC dc(m_win);
    DoPrepareDC(dc);

    OnDraw(dc);
}

// kbd handling: notice that we use OnChar() and not OnKeyDown() for
// compatibility here - if we used OnKeyDown(), the programs which process
// arrows themselves in their OnChar() would never get the message and like
// this they always have the priority
void wxAnyScrollHelperBase::HandleOnChar(wxKeyEvent& event)
{
    if ( !m_kbdScrollingEnabled )
    {
        event.Skip();
        return;
    }

    // prepare the event this key press maps to
    wxScrollWinEvent newEvent;

    newEvent.SetPosition(0);
    newEvent.SetEventObject(m_win);
    newEvent.SetId(m_win->GetId());

    // this is the default, it's changed to wxHORIZONTAL below if needed
    newEvent.SetOrientation(wxVERTICAL);

    // some key events result in scrolling in both horizontal and vertical
    // direction, e.g. Ctrl-{Home,End}, if this flag is true we should generate
    // a second event in horizontal direction in addition to the primary one
    bool sendHorizontalToo = false;

    switch ( event.GetKeyCode() )
    {
        case WXK_PAGEUP:
            newEvent.SetEventType(wxEVT_SCROLLWIN_PAGEUP);
            break;

        case WXK_PAGEDOWN:
            newEvent.SetEventType(wxEVT_SCROLLWIN_PAGEDOWN);
            break;

        case WXK_HOME:
            newEvent.SetEventType(wxEVT_SCROLLWIN_TOP);

            sendHorizontalToo = event.ControlDown();
            break;

        case WXK_END:
            newEvent.SetEventType(wxEVT_SCROLLWIN_BOTTOM);

            sendHorizontalToo = event.ControlDown();
            break;

        case WXK_LEFT:
            newEvent.SetOrientation(wxHORIZONTAL);
            // fall through

        case WXK_UP:
            newEvent.SetEventType(wxEVT_SCROLLWIN_LINEUP);
            break;

        case WXK_RIGHT:
            newEvent.SetOrientation(wxHORIZONTAL);
            // fall through

        case WXK_DOWN:
            newEvent.SetEventType(wxEVT_SCROLLWIN_LINEDOWN);
            break;

        default:
            // not a scrolling key
            event.Skip();
            return;
    }

    m_win->ProcessWindowEvent(newEvent);

    if ( sendHorizontalToo )
    {
        newEvent.SetOrientation(wxHORIZONTAL);
        m_win->ProcessWindowEvent(newEvent);
    }
}

// ----------------------------------------------------------------------------
// autoscroll stuff: these functions deal with sending fake scroll events when
// a captured mouse is being held outside the window
// ----------------------------------------------------------------------------

bool wxScrollHelperBase::SendAutoScrollEvents(wxScrollWinEvent& event) const
{
    // only send the event if the window is scrollable in this direction
    wxWindow *win = (wxWindow *)event.GetEventObject();
    return win->HasScrollbar(event.GetOrientation());
}

void wxScrollHelperBase::StopAutoScrolling()
{
#if wxUSE_TIMER
    wxDELETE(m_timerAutoScroll);
#endif
}

void wxScrollHelperBase::HandleOnMouseEnter(wxMouseEvent& event)
{
    StopAutoScrolling();

    event.Skip();
}

void wxScrollHelperBase::HandleOnMouseLeave(wxMouseEvent& event)
{
    // don't prevent the usual processing of the event from taking place
    event.Skip();

    // when a captured mouse leave a scrolled window we start generate
    // scrolling events to allow, for example, extending selection beyond the
    // visible area in some controls
    if ( wxWindow::GetCapture() == m_targetWindow )
    {
        // where is the mouse leaving?
        int pos, orient;
        wxPoint pt = event.GetPosition();
        if ( pt.x < 0 )
        {
            orient = wxHORIZONTAL;
            pos = 0;
        }
        else if ( pt.y < 0 )
        {
            orient = wxVERTICAL;
            pos = 0;
        }
        else // we're lower or to the right of the window
        {
            wxSize size = m_targetWindow->GetClientSize();
            if ( pt.x > size.x )
            {
                orient = wxHORIZONTAL;
                pos = m_xScrollLines;
            }
            else if ( pt.y > size.y )
            {
                orient = wxVERTICAL;
                pos = m_yScrollLines;
            }
            else // this should be impossible
            {
                // but seems to happen sometimes under wxMSW - maybe it's a bug
                // there but for now just ignore it

                //wxFAIL_MSG( wxT("can't understand where has mouse gone") );

                return;
            }
        }

        // only start the auto scroll timer if the window can be scrolled in
        // this direction
        if ( !m_targetWindow->HasScrollbar(orient) )
            return;

#if wxUSE_TIMER
        delete m_timerAutoScroll;
        m_timerAutoScroll = new wxAutoScrollTimer
                                (
                                    m_targetWindow, this,
                                    pos == 0 ? wxEVT_SCROLLWIN_LINEUP
                                             : wxEVT_SCROLLWIN_LINEDOWN,
                                    pos,
                                    orient
                                );
        m_timerAutoScroll->Start(50); // FIXME: make configurable
#else
        wxUnusedVar(pos);
#endif
    }
}

#if wxUSE_MOUSEWHEEL

void wxScrollHelperBase::HandleOnMouseWheel(wxMouseEvent& event)
{
    m_wheelRotation += event.GetWheelRotation();
    int lines = m_wheelRotation / event.GetWheelDelta();
    m_wheelRotation -= lines * event.GetWheelDelta();

    if (lines != 0)
    {

        wxScrollWinEvent newEvent;

        newEvent.SetPosition(0);
        newEvent.SetOrientation( event.GetWheelAxis() == 0 ? wxVERTICAL : wxHORIZONTAL);
        newEvent.SetEventObject(m_win);

        if ( event.GetWheelAxis() == wxMOUSE_WHEEL_HORIZONTAL )
            lines = -lines;

        if (event.IsPageScroll())
        {
            if (lines > 0)
                newEvent.SetEventType(wxEVT_SCROLLWIN_PAGEUP);
            else
                newEvent.SetEventType(wxEVT_SCROLLWIN_PAGEDOWN);

            m_win->GetEventHandler()->ProcessEvent(newEvent);
        }
        else
        {
            lines *= event.GetLinesPerAction();
            if (lines > 0)
                newEvent.SetEventType(wxEVT_SCROLLWIN_LINEUP);
            else
                newEvent.SetEventType(wxEVT_SCROLLWIN_LINEDOWN);

            int times = abs(lines);
            for (; times > 0; times--)
                m_win->GetEventHandler()->ProcessEvent(newEvent);
        }
    }
}

#endif // wxUSE_MOUSEWHEEL

void wxScrollHelperBase::HandleOnChildFocus(wxChildFocusEvent& event)
{
    // this event should be processed by all windows in parenthood chain,
    // e.g. so that nested wxScrolledWindows work correctly
    event.Skip();

    // find the immediate child under which the window receiving focus is:
    wxWindow *win = event.GetWindow();

    if ( win == m_targetWindow )
        return; // nothing to do

#if defined( __WXOSX__ ) && wxUSE_SCROLLBAR
    if (wxDynamicCast(win, wxScrollBar))
        return;
#endif

    // Fixing ticket: http://trac.wxwidgets.org/ticket/9563
    // When a child inside a wxControlContainer receives a focus, the
    // wxControlContainer generates an artificial wxChildFocusEvent for
    // itself, telling its parent that 'it' received the focus. The effect is
    // that this->HandleOnChildFocus is called twice, first with the
    // artificial wxChildFocusEvent and then with the original event.  We need
    // to ignore the artificial event here or otherwise HandleOnChildFocus
    // would first scroll the target window to make the entire
    // wxControlContainer visible and immediately afterwards scroll the target
    // window again to make the child widget visible. This leads to ugly
    // flickering when using nested wxPanels/wxScrolledWindows.
    //
    // Ignore this event if 'win' is derived from wxControlContainer AND its
    // parent is the m_targetWindow AND 'win' is not actually reciving the
    // focus (win != FindFocus).  TODO: This affects all wxControlContainer
    // objects, but wxControlContainer is not part of the wxWidgets RTTI and
    // so wxDynamicCast(win, wxControlContainer) does not compile.  Find a way
    // to determine if 'win' derives from wxControlContainer. Until then,
    // testing if 'win' derives from wxPanel will probably get >90% of all
    // cases.

    wxWindow *actual_focus=wxWindow::FindFocus();
    if (win != actual_focus &&
        wxDynamicCast(win, wxPanel) != 0 &&
        win->GetParent() == m_targetWindow)
        // if win is a wxPanel and receives the focus, it should not be
        // scrolled into view
        return;

    const wxRect viewRect(m_targetWindow->GetClientRect());

    // For composite controls such as wxComboCtrl we should try to fit the
    // entire control inside the visible area of the target window, not just
    // the focused child of the control. Otherwise we'd make only the textctrl
    // part of a wxComboCtrl visible and the button would still be outside the
    // scrolled area.  But do so only if the parent fits *entirely* inside the
    // scrolled window. In other situations, such as nested wxPanel or
    // wxScrolledWindows, the parent might be way too big to fit inside the
    // scrolled window. If that is the case, then make only the focused window
    // visible
    if ( win->GetParent() != m_targetWindow)
    {
        wxWindow *parent=win->GetParent();
        wxSize parent_size=parent->GetSize();
        if (parent_size.GetWidth() <= viewRect.GetWidth() &&
            parent_size.GetHeight() <= viewRect.GetHeight())
            // make the immediate parent visible instead of the focused control
            win=parent;
    }

    // make win position relative to the m_targetWindow viewing area instead of
    // its parent
    const wxRect
        winRect(m_targetWindow->ScreenToClient(win->GetScreenPosition()),
                win->GetSize());

    // check if it's fully visible
    if ( viewRect.Contains(winRect) )
    {
        // it is, nothing to do
        return;
    }

    // check if we can make it fully visible: this is only possible if it's not
    // larger than our view area
    if ( winRect.GetWidth() > viewRect.GetWidth() ||
            winRect.GetHeight() > viewRect.GetHeight() )
    {
        // we can't make it fit so avoid scrolling it at all, this is only
        // going to be confusing and not helpful
        return;
    }


    // do make the window fit inside the view area by scrolling to it
    int stepx, stepy;
    GetScrollPixelsPerUnit(&stepx, &stepy);

    int startx, starty;
    GetViewStart(&startx, &starty);

    // first in vertical direction:
    if ( stepy > 0 )
    {
        int diff = 0;

        if ( winRect.GetTop() < 0 )
        {
            diff = winRect.GetTop();
        }
        else if ( winRect.GetBottom() > viewRect.GetHeight() )
        {
            diff = winRect.GetBottom() - viewRect.GetHeight() + 1;
            // round up to next scroll step if we can't get exact position,
            // so that the window is fully visible:
            diff += stepy - 1;
        }

        starty = (starty * stepy + diff) / stepy;
    }

    // then horizontal:
    if ( stepx > 0 )
    {
        int diff = 0;

        if ( winRect.GetLeft() < 0 )
        {
            diff = winRect.GetLeft();
        }
        else if ( winRect.GetRight() > viewRect.GetWidth() )
        {
            diff = winRect.GetRight() - viewRect.GetWidth() + 1;
            // round up to next scroll step if we can't get exact position,
            // so that the window is fully visible:
            diff += stepx - 1;
        }

        startx = (startx * stepx + diff) / stepx;
    }

    Scroll(startx, starty);
}


#ifdef wxHAS_GENERIC_SCROLLWIN

// ----------------------------------------------------------------------------
// wxScrollHelper implementation
// ----------------------------------------------------------------------------

wxScrollHelper::wxScrollHelper(wxWindow *winToScroll)
    : wxScrollHelperBase(winToScroll)
{
    m_xVisibility =
    m_yVisibility = wxSHOW_SB_DEFAULT;
}

bool wxScrollHelper::IsScrollbarShown(int orient) const
{
    wxScrollbarVisibility visibility = orient == wxHORIZONTAL ? m_xVisibility
                                                              : m_yVisibility;

    return visibility != wxSHOW_SB_NEVER;
}

void wxScrollHelper::DoShowScrollbars(wxScrollbarVisibility horz,
                                      wxScrollbarVisibility vert)
{
    if ( horz != m_xVisibility || vert != m_yVisibility )
    {
        m_xVisibility = horz;
        m_yVisibility = vert;

        AdjustScrollbars();
    }
}

void
wxScrollHelper::DoAdjustScrollbar(int orient,
                                  int clientSize,
                                  int virtSize,
                                  int pixelsPerUnit,
                                  int& scrollUnits,
                                  int& scrollPosition,
                                  int& scrollLinesPerPage,
                                  wxScrollbarVisibility visibility)
{
    // scroll lines per page: if 0, no scrolling is needed
    // check if we need scrollbar in this direction at all
    if ( pixelsPerUnit == 0 || clientSize >= virtSize )
    {
        // scrolling is disabled or unnecessary
        scrollUnits =
        scrollPosition = 0;
        scrollLinesPerPage = 0;
    }
    else // might need scrolling
    {
        // Round up integer division to catch any "leftover" client space.
        scrollUnits = (virtSize + pixelsPerUnit - 1) / pixelsPerUnit;

        // Calculate the number of fully scroll units
        scrollLinesPerPage = clientSize / pixelsPerUnit;

        if ( scrollLinesPerPage >= scrollUnits )
        {
            // we're big enough to not need scrolling
            scrollUnits =
            scrollPosition = 0;
            scrollLinesPerPage = 0;
        }
        else // we do need a scrollbar
        {
            if ( scrollLinesPerPage < 1 )
                scrollLinesPerPage = 1;

            // Correct position if greater than extent of canvas minus
            // the visible portion of it or if below zero
            const int posMax = scrollUnits - scrollLinesPerPage;
            if ( scrollPosition > posMax )
                scrollPosition = posMax;
            else if ( scrollPosition < 0 )
                scrollPosition = 0;
        }
    }

    // in wxSHOW_SB_NEVER case don't show the scrollbar even if it's needed, in
    // wxSHOW_SB_ALWAYS case show the scrollbar even if it's not needed by
    // passing a special range value to SetScrollbar()
    int range;
    switch ( visibility )
    {
        case wxSHOW_SB_NEVER:
            range = 0;
            break;

        case wxSHOW_SB_ALWAYS:
            range = scrollUnits ? scrollUnits : -1;
            break;

        default:
            wxFAIL_MSG( wxS("unknown scrollbar visibility") );
            // fall through

        case wxSHOW_SB_DEFAULT:
            range = scrollUnits;
            break;

    }

    m_win->SetScrollbar(orient, scrollPosition, scrollLinesPerPage, range);
}

void wxScrollHelper::AdjustScrollbars()
{
    static wxScrollHelperRecGuardFlags s_flagReentrancy;
    wxScrollHelperRecGuard guard(this, s_flagReentrancy);
    if ( guard.IsInside() )
    {
        // don't reenter AdjustScrollbars() while another call to
        // AdjustScrollbars() is in progress because this may lead to calling
        // ScrollWindow() twice and this can really happen under MSW if
        // SetScrollbar() call below adds or removes the scrollbar which
        // changes the window size and hence results in another
        // AdjustScrollbars() call
        return;
    }

    int oldXScroll = m_xScrollPosition;
    int oldYScroll = m_yScrollPosition;

    // we may need to readjust the scrollbars several times as enabling one of
    // them reduces the area available for the window contents and so can make
    // the other scrollbar necessary now although it wasn't necessary before
    //
    // VZ: normally this loop should be over in at most 2 iterations, I don't
    //     know why do we need 5 of them
    for ( int iterationCount = 0; iterationCount < 5; iterationCount++ )
    {
        wxSize clientSize = GetTargetSize();
        const wxSize virtSize = m_targetWindow->GetVirtualSize();

        // this block of code tries to work around the following problem: the
        // window could have been just resized to have enough space to show its
        // full contents without the scrollbars, but its client size could be
        // not big enough because it does have the scrollbars right now and so
        // the scrollbars would remain even though we don't need them any more
        //
        // to prevent this from happening, check if we have enough space for
        // everything without the scrollbars and explicitly disable them then
        const wxSize availSize = GetSizeAvailableForScrollTarget(
            m_win->GetSize() - m_win->GetWindowBorderSize());
        if ( availSize != clientSize )
        {
            if ( availSize.x >= virtSize.x && availSize.y >= virtSize.y )
            {
                // this will be enough to make the scrollbars disappear below
                // and then the client size will indeed become equal to the
                // full available size
                clientSize = availSize;
            }
        }


        DoAdjustScrollbar(wxHORIZONTAL,
                          clientSize.x,
                          virtSize.x,
                          m_xScrollPixelsPerLine,
                          m_xScrollLines,
                          m_xScrollPosition,
                          m_xScrollLinesPerPage,
                          m_xVisibility);

        DoAdjustScrollbar(wxVERTICAL,
                          clientSize.y,
                          virtSize.y,
                          m_yScrollPixelsPerLine,
                          m_yScrollLines,
                          m_yScrollPosition,
                          m_yScrollLinesPerPage,
                          m_yVisibility);


        // If a scrollbar (dis)appeared as a result of this, we need to adjust
        // them again but if the client size didn't change, then we're done
        if ( GetTargetSize() == clientSize )
            break;
    }

#ifdef __WXMOTIF__
    // Sorry, some Motif-specific code to implement a backing pixmap
    // for the wxRETAINED style. Implementing a backing store can't
    // be entirely generic because it relies on the wxWindowDC implementation
    // to duplicate X drawing calls for the backing pixmap.

    if ( m_targetWindow->GetWindowStyle() & wxRETAINED )
    {
        Display* dpy = XtDisplay((Widget)m_targetWindow->GetMainWidget());

        int totalPixelWidth = m_xScrollLines * m_xScrollPixelsPerLine;
        int totalPixelHeight = m_yScrollLines * m_yScrollPixelsPerLine;
        if (m_targetWindow->GetBackingPixmap() &&
           !((m_targetWindow->GetPixmapWidth() == totalPixelWidth) &&
             (m_targetWindow->GetPixmapHeight() == totalPixelHeight)))
        {
            XFreePixmap (dpy, (Pixmap) m_targetWindow->GetBackingPixmap());
            m_targetWindow->SetBackingPixmap((WXPixmap) 0);
        }

        if (!m_targetWindow->GetBackingPixmap() &&
           (m_xScrollLines != 0) && (m_yScrollLines != 0))
        {
            int depth = wxDisplayDepth();
            m_targetWindow->SetPixmapWidth(totalPixelWidth);
            m_targetWindow->SetPixmapHeight(totalPixelHeight);
            m_targetWindow->SetBackingPixmap((WXPixmap) XCreatePixmap (dpy, RootWindow (dpy, DefaultScreen (dpy)),
              m_targetWindow->GetPixmapWidth(), m_targetWindow->GetPixmapHeight(), depth));
        }

    }
#endif // Motif

    if (oldXScroll != m_xScrollPosition)
    {
       if (m_xScrollingEnabled)
            m_targetWindow->ScrollWindow( m_xScrollPixelsPerLine * (oldXScroll - m_xScrollPosition), 0,
                                          GetScrollRect() );
       else
            m_targetWindow->Refresh(true, GetScrollRect());
    }

    if (oldYScroll != m_yScrollPosition)
    {
        if (m_yScrollingEnabled)
            m_targetWindow->ScrollWindow( 0, m_yScrollPixelsPerLine * (oldYScroll-m_yScrollPosition),
                                          GetScrollRect() );
        else
            m_targetWindow->Refresh(true, GetScrollRect());
    }
}

void wxScrollHelper::DoScroll( int x_pos, int y_pos )
{
    if (!m_targetWindow)
        return;

    if (((x_pos == -1) || (x_pos == m_xScrollPosition)) &&
        ((y_pos == -1) || (y_pos == m_yScrollPosition))) return;

    int w = 0, h = 0;
    GetTargetSize(&w, &h);

    // compute new position:
    int new_x = m_xScrollPosition;
    int new_y = m_yScrollPosition;

    if ((x_pos != -1) && (m_xScrollPixelsPerLine))
    {
        new_x = x_pos;

        // Calculate page size i.e. number of scroll units you get on the
        // current client window
        int noPagePositions = w/m_xScrollPixelsPerLine;
        if (noPagePositions < 1) noPagePositions = 1;

        // Correct position if greater than extent of canvas minus
        // the visible portion of it or if below zero
        new_x = wxMin( m_xScrollLines-noPagePositions, new_x );
        new_x = wxMax( 0, new_x );
    }
    if ((y_pos != -1) && (m_yScrollPixelsPerLine))
    {
        new_y = y_pos;

        // Calculate page size i.e. number of scroll units you get on the
        // current client window
        int noPagePositions = h/m_yScrollPixelsPerLine;
        if (noPagePositions < 1) noPagePositions = 1;

        // Correct position if greater than extent of canvas minus
        // the visible portion of it or if below zero
        new_y = wxMin( m_yScrollLines-noPagePositions, new_y );
        new_y = wxMax( 0, new_y );
    }

    if ( new_x == m_xScrollPosition && new_y == m_yScrollPosition )
        return; // nothing to do, the position didn't change

    // flush all pending repaints before we change m_{x,y}ScrollPosition, as
    // otherwise invalidated area could be updated incorrectly later when
    // ScrollWindow() makes sure they're repainted before scrolling them
    m_targetWindow->Update();

    // update the position and scroll the window now:
    if (m_xScrollPosition != new_x)
    {
        int old_x = m_xScrollPosition;
        m_xScrollPosition = new_x;
        m_win->SetScrollPos( wxHORIZONTAL, new_x );
        m_targetWindow->ScrollWindow( (old_x-new_x)*m_xScrollPixelsPerLine, 0,
                                      GetScrollRect() );
    }

    if (m_yScrollPosition != new_y)
    {
        int old_y = m_yScrollPosition;
        m_yScrollPosition = new_y;
        m_win->SetScrollPos( wxVERTICAL, new_y );
        m_targetWindow->ScrollWindow( 0, (old_y-new_y)*m_yScrollPixelsPerLine,
                                      GetScrollRect() );
    }
}

#endif // wxHAS_GENERIC_SCROLLWIN

// ----------------------------------------------------------------------------
// wxScrolled<T> and wxScrolledWindow implementation
// ----------------------------------------------------------------------------

wxSize wxScrolledT_Helper::FilterBestSize(const wxWindow *win,
                                          const wxScrollHelper *helper,
                                          const wxSize& origBest)
{
    // NB: We don't do this in WX_FORWARD_TO_SCROLL_HELPER, because not
    //     all scrollable windows should behave like this, only those that
    //     contain children controls within scrollable area
    //     (i.e., wxScrolledWindow) and other some scrollable windows may
    //     have different DoGetBestSize() implementation (e.g. wxTreeCtrl).

    wxSize best = origBest;

    if ( win->GetAutoLayout() )
    {
        // Only use the content to set the window size in the direction
        // where there's no scrolling; otherwise we're going to get a huge
        // window in the direction in which scrolling is enabled
        int ppuX, ppuY;
        helper->GetScrollPixelsPerUnit(&ppuX, &ppuY);

        // NB: This code used to use *current* size if min size wasn't
        //     specified, presumably to get some reasonable (i.e., larger than
        //     minimal) size.  But that's a wrong thing to do in GetBestSize(),
        //     so we use minimal size as specified. If the app needs some
        //     minimal size for its scrolled window, it should set it and put
        //     the window into sizer as expandable so that it can use all space
        //     available to it.
        //
        //     See also http://svn.wxwidgets.org/viewvc/wx?view=rev&revision=45864

        wxSize minSize = win->GetMinSize();

        if ( ppuX > 0 )
            best.x = minSize.x + wxSystemSettings::GetMetric(wxSYS_VSCROLL_X);

        if ( ppuY > 0 )
            best.y = minSize.y + wxSystemSettings::GetMetric(wxSYS_HSCROLL_Y);
    }

    return best;
}

#ifdef __WXMSW__
WXLRESULT wxScrolledT_Helper::FilterMSWWindowProc(WXUINT nMsg, WXLRESULT rc)
{
#ifndef __WXWINCE__
    // we need to process arrows ourselves for scrolling
    if ( nMsg == WM_GETDLGCODE )
    {
        rc |= DLGC_WANTARROWS;
    }
#endif
    return rc;
}
#endif // __WXMSW__

// NB: skipping wxScrolled<T> in wxRTTI information because being a templte,
//     it doesn't and can't implement wxRTTI support
IMPLEMENT_DYNAMIC_CLASS(wxScrolledWindow, wxPanel)
