/////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/statusbr.cpp
// Purpose:     wxStatusBarGeneric class implementation
// Author:      Julian Smart
// Modified by: Francesco Montorsi
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_STATUSBAR

#include "wx/statusbr.h"

#ifndef WX_PRECOMP
    #include "wx/settings.h"
    #include "wx/dcclient.h"
    #include "wx/toplevel.h"
    #include "wx/control.h"
#endif

#ifdef __WXGTK20__
    #include <gtk/gtk.h>
    #include "wx/gtk/private.h"
    #include "wx/gtk/private/gtk2-compat.h"
#endif

// we only have to do it here when we use wxStatusBarGeneric in addition to the
// standard wxStatusBar class, if wxStatusBarGeneric is the same as
// wxStatusBar, then the corresponding IMPLEMENT_DYNAMIC_CLASS is already in
// common/statbar.cpp
#if defined(__WXMAC__) || \
    (defined(wxUSE_NATIVE_STATUSBAR) && wxUSE_NATIVE_STATUSBAR)
    #include "wx/generic/statusbr.h"

    IMPLEMENT_DYNAMIC_CLASS(wxStatusBarGeneric, wxWindow)
#endif // wxUSE_NATIVE_STATUSBAR

// Default status border dimensions
#define wxTHICK_LINE_BORDER 2

// Margin between the field text and the field rect
#define wxFIELD_TEXT_MARGIN 2

// ----------------------------------------------------------------------------
// GTK+ signal handler
// ----------------------------------------------------------------------------

#if defined( __WXGTK20__ )
#if GTK_CHECK_VERSION(2,12,0)
extern "C" {
static
gboolean statusbar_query_tooltip(GtkWidget*   WXUNUSED(widget),
                                 gint        x,
                                 gint        y,
                                 gboolean     WXUNUSED(keyboard_mode),
                                 GtkTooltip *tooltip,
                                 wxStatusBar* statbar)
{
    int n = statbar->GetFieldFromPoint(wxPoint(x,y));
    if (n == wxNOT_FOUND)
        return FALSE;

    // should we show the tooltip for the n-th pane of the statusbar?
    if (!statbar->GetField(n).IsEllipsized())
        return FALSE;   // no, it's not useful

    const wxString& str = statbar->GetStatusText(n);
    if (str.empty())
        return FALSE;

    gtk_tooltip_set_text(tooltip, wxGTK_CONV_SYS(str));
    return TRUE;
}
}
#endif
#endif

// ----------------------------------------------------------------------------
// wxStatusBarGeneric
// ----------------------------------------------------------------------------

BEGIN_EVENT_TABLE(wxStatusBarGeneric, wxWindow)
    EVT_PAINT(wxStatusBarGeneric::OnPaint)
    EVT_SIZE(wxStatusBarGeneric::OnSize)
#ifdef __WXGTK20__
    EVT_LEFT_DOWN(wxStatusBarGeneric::OnLeftDown)
    EVT_RIGHT_DOWN(wxStatusBarGeneric::OnRightDown)
#endif
    EVT_SYS_COLOUR_CHANGED(wxStatusBarGeneric::OnSysColourChanged)
END_EVENT_TABLE()

void wxStatusBarGeneric::Init()
{
    m_borderX = wxTHICK_LINE_BORDER;
    m_borderY = wxTHICK_LINE_BORDER;
}

wxStatusBarGeneric::~wxStatusBarGeneric()
{
}

bool wxStatusBarGeneric::Create(wxWindow *parent,
                                wxWindowID id,
                                long style,
                                const wxString& name)
{
    style |= wxTAB_TRAVERSAL | wxFULL_REPAINT_ON_RESIZE;
    if ( !wxWindow::Create(parent, id,
                           wxDefaultPosition, wxDefaultSize,
                           style, name) )
        return false;

    // The status bar should have a themed background
    SetThemeEnabled( true );

    InitColours();

#ifdef __WXPM__
    SetFont(*wxSMALL_FONT);
#endif

    int height = (int)((11*GetCharHeight())/10 + 2*GetBorderY());
    SetSize(wxDefaultCoord, wxDefaultCoord, wxDefaultCoord, height);

    SetFieldsCount(1);

#if defined( __WXGTK20__ )
#if GTK_CHECK_VERSION(2,12,0)
    if (HasFlag(wxSTB_SHOW_TIPS)
#ifndef __WXGTK3__
        && gtk_check_version(2,12,0) == NULL
#endif
        )
    {
        g_object_set(m_widget, "has-tooltip", TRUE, NULL);
        g_signal_connect(m_widget, "query-tooltip",
                         G_CALLBACK(statusbar_query_tooltip), this);
    }
#endif
#endif

    return true;
}

wxSize wxStatusBarGeneric::DoGetBestSize() const
{
    int width, height;

    // best width is the width of the parent
    if (GetParent())
        GetParent()->GetClientSize(&width, NULL);
    else
        width = 80;     // a dummy value

    // best height is as calculated above in Create()
    height = (int)((11*GetCharHeight())/10 + 2*GetBorderY());

    return wxSize(width, height);
}

void wxStatusBarGeneric::DoUpdateStatusText(int number)
{
    wxRect rect;
    GetFieldRect(number, rect);

    Refresh(true, &rect);

    // it's common to show some text in the status bar before starting a
    // relatively lengthy operation, ensure that the text is shown to the
    // user immediately and not after the lengthy operation end
    Update();
}

void wxStatusBarGeneric::SetStatusWidths(int n, const int widths_field[])
{
    // only set status widths when n == number of statuswindows
    wxCHECK_RET( (size_t)n == m_panes.GetCount(), wxT("status bar field count mismatch") );

    wxStatusBarBase::SetStatusWidths(n, widths_field);

    // update cache
    DoUpdateFieldWidths();
}

void wxStatusBarGeneric::DoUpdateFieldWidths()
{
    m_lastClientSize = GetClientSize();

    // recompute the cache of the field widths if the status bar width has changed
    m_widthsAbs = CalculateAbsWidths(m_lastClientSize.x);
}

bool wxStatusBarGeneric::ShowsSizeGrip() const
{
    if ( !HasFlag(wxSTB_SIZEGRIP) )
        return false;

    wxTopLevelWindow * const
        tlw = wxDynamicCast(wxGetTopLevelParent(GetParent()), wxTopLevelWindow);
    return tlw && !tlw->IsMaximized() && tlw->HasFlag(wxRESIZE_BORDER);
}

void wxStatusBarGeneric::DrawFieldText(wxDC& dc, const wxRect& rect, int i, int textHeight)
{
    wxString text(GetStatusText(i));
    if (text.empty())
        return;     // optimization

    int xpos = rect.x + wxFIELD_TEXT_MARGIN,
        maxWidth = rect.width - 2*wxFIELD_TEXT_MARGIN,
        ypos = (int) (((rect.height - textHeight) / 2) + rect.y + 0.5);

    if (ShowsSizeGrip())
    {
        // don't write text over the size grip:
        // NOTE: overloading DoGetClientSize() and GetClientAreaOrigin() wouldn't
        //       work because the adjustment needs to be done only when drawing
        //       the field text and not also when drawing the background, the
        //       size grip itself, etc
        if ((GetLayoutDirection() == wxLayout_RightToLeft && i == 0) ||
            (GetLayoutDirection() != wxLayout_RightToLeft &&
                 i == (int)m_panes.GetCount()-1))
        {
            const wxRect& gripRc = GetSizeGripRect();

            // NOTE: we don't need any special treatment wrt to the layout direction
            //       since DrawText() will automatically adjust the origin of the
            //       text accordingly to the layout in use

            maxWidth -= gripRc.width;
        }
    }

    // eventually ellipsize the text so that it fits the field width

    wxEllipsizeMode ellmode = (wxEllipsizeMode)-1;
    if (HasFlag(wxSTB_ELLIPSIZE_START)) ellmode = wxELLIPSIZE_START;
    else if (HasFlag(wxSTB_ELLIPSIZE_MIDDLE)) ellmode = wxELLIPSIZE_MIDDLE;
    else if (HasFlag(wxSTB_ELLIPSIZE_END)) ellmode = wxELLIPSIZE_END;

    if (ellmode == (wxEllipsizeMode)-1)
    {
        // if we have the wxSTB_SHOW_TIPS we must set the ellipsized flag even if
        // we don't ellipsize the text but just truncate it
        if (HasFlag(wxSTB_SHOW_TIPS))
            SetEllipsizedFlag(i, dc.GetTextExtent(text).GetWidth() > maxWidth);

        dc.SetClippingRegion(rect);
    }
    else
    {
        text = wxControl::Ellipsize(text, dc,
                                    ellmode,
                                    maxWidth,
                                    wxELLIPSIZE_FLAGS_EXPAND_TABS);
            // Ellipsize() will do something only if necessary

        // update the ellipsization status for this pane; this is used later to
        // decide whether a tooltip should be shown or not for this pane
        // (if we have wxSTB_SHOW_TIPS)
        SetEllipsizedFlag(i, text != GetStatusText(i));
    }

#if defined( __WXGTK__ ) || defined(__WXMAC__)
    xpos++;
    ypos++;
#endif

    // draw the text
    dc.DrawText(text, xpos, ypos);

    if (ellmode == (wxEllipsizeMode)-1)
        dc.DestroyClippingRegion();
}

void wxStatusBarGeneric::DrawField(wxDC& dc, int i, int textHeight)
{
    wxRect rect;
    GetFieldRect(i, rect);

    if (rect.GetWidth() <= 0)
        return;     // happens when the status bar is shrunk in a very small area!

    int style = m_panes[i].GetStyle();
    if (style == wxSB_RAISED || style == wxSB_SUNKEN)
    {
        // Draw border
        // For wxSB_SUNKEN: paint a grey background, plus 3-d border (one black rectangle)
        // Inside this, left and top sides (dark grey). Bottom and right (white).
        // Reverse it for wxSB_RAISED

        dc.SetPen((style == wxSB_RAISED) ? m_mediumShadowPen : m_hilightPen);

#ifndef __WXPM__

        // Right and bottom lines
        dc.DrawLine(rect.x + rect.width, rect.y,
                    rect.x + rect.width, rect.y + rect.height);
        dc.DrawLine(rect.x + rect.width, rect.y + rect.height,
                    rect.x, rect.y + rect.height);

        dc.SetPen((style == wxSB_RAISED) ? m_hilightPen : m_mediumShadowPen);

        // Left and top lines
        dc.DrawLine(rect.x, rect.y + rect.height,
               rect.x, rect.y);
        dc.DrawLine(rect.x, rect.y,
            rect.x + rect.width, rect.y);
#else

        dc.DrawLine(rect.x + rect.width, rect.height + 2,
                    rect.x, rect.height + 2);
        dc.DrawLine(rect.x + rect.width, rect.y,
                    rect.x + rect.width, rect.y + rect.height);

        dc.SetPen((style == wxSB_RAISED) ? m_hilightPen : m_mediumShadowPen);
        dc.DrawLine(rect.x, rect.y,
                    rect.x + rect.width, rect.y);
        dc.DrawLine(rect.x, rect.y + rect.height,
                    rect.x, rect.y);
#endif
    }

    DrawFieldText(dc, rect, i, textHeight);
}

bool wxStatusBarGeneric::GetFieldRect(int n, wxRect& rect) const
{
    wxCHECK_MSG( (n >= 0) && ((size_t)n < m_panes.GetCount()), false,
                 wxT("invalid status bar field index") );

    // We can be called from the user-defined EVT_SIZE handler in which case
    // the widths haven't been updated yet and we need to do it now. This is
    // not very efficient as we keep testing the size but there is no other way
    // to make the code needing the up-to-date fields sizes in its EVT_SIZE to
    // work.
    if ( GetClientSize().x != m_lastClientSize.x )
    {
        const_cast<wxStatusBarGeneric*>(this)->DoUpdateFieldWidths();
    }

    if (m_widthsAbs.IsEmpty())
        return false;

    rect.x = 0;
    for ( int i = 0; i < n; i++ )
        rect.x += m_widthsAbs[i];
    rect.x += m_borderX;

    rect.y = m_borderY;
    rect.width = m_widthsAbs[n] - 2*m_borderX;
    rect.height = m_lastClientSize.y - 2*m_borderY;

    return true;
}

int wxStatusBarGeneric::GetFieldFromPoint(const wxPoint& pt) const
{
    if (m_widthsAbs.IsEmpty())
        return wxNOT_FOUND;

    // NOTE: we explicitly don't take in count the borders since they are only
    //       useful when rendering the status text, not for hit-test computations

    if (pt.y <= 0 || pt.y >= m_lastClientSize.y)
        return wxNOT_FOUND;

    int x = 0;
    for ( size_t i = 0; i < m_panes.GetCount(); i++ )
    {
        if (pt.x > x && pt.x < x+m_widthsAbs[i])
            return i;

        x += m_widthsAbs[i];
    }

    return wxNOT_FOUND;
}

void wxStatusBarGeneric::InitColours()
{
#if defined(__WXPM__)
    m_mediumShadowPen = wxPen(wxColour(127, 127, 127));
    m_hilightPen = *wxWHITE_PEN;

    SetBackgroundColour(*wxLIGHT_GREY);
    SetForegroundColour(*wxBLACK);
#else // !__WXPM__
    m_mediumShadowPen = wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DSHADOW));
    m_hilightPen = wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DHILIGHT));
#endif // __WXPM__/!__WXPM__
}

void wxStatusBarGeneric::SetMinHeight(int height)
{
    // check that this min height is not less than minimal height for the
    // current font (min height is as calculated above in Create() except for border)
    int minHeight = (int)((11*GetCharHeight())/10);

    if ( height > minHeight )
        SetSize(wxDefaultCoord, wxDefaultCoord, wxDefaultCoord, height + 2*m_borderY);
}

wxRect wxStatusBarGeneric::GetSizeGripRect() const
{
    int width, height;
    wxWindow::DoGetClientSize(&width, &height);

    if (GetLayoutDirection() == wxLayout_RightToLeft)
        return wxRect(2, 2, height-2, height-4);
    else
        return wxRect(width-height-2, 2, height-2, height-4);
}

// ----------------------------------------------------------------------------
// wxStatusBarGeneric - event handlers
// ----------------------------------------------------------------------------

void wxStatusBarGeneric::OnPaint(wxPaintEvent& WXUNUSED(event) )
{
    wxPaintDC dc(this);

#ifdef __WXGTK20__
    // Draw grip first
    if ( ShowsSizeGrip() )
    {
        const wxRect& rc = GetSizeGripRect();
#ifdef __WXGTK3__
        GtkWidget* toplevel = gtk_widget_get_toplevel(m_widget);
        GdkRectangle rect;
        if (toplevel && (!gtk_window_get_resize_grip_area(GTK_WINDOW(toplevel), &rect) ||
            rect.width == 0 || rect.height == 0))
        {
            GtkStyleContext* sc = gtk_widget_get_style_context(toplevel);
            gtk_style_context_save(sc);
            gtk_style_context_add_class(sc, GTK_STYLE_CLASS_GRIP);
            GtkJunctionSides sides = GTK_JUNCTION_CORNER_BOTTOMRIGHT;
            if (GetLayoutDirection() == wxLayout_RightToLeft)
                sides = GTK_JUNCTION_CORNER_BOTTOMLEFT;
            gtk_style_context_set_junction_sides(sc, sides);
            gtk_render_handle(sc,
                static_cast<cairo_t*>(dc.GetImpl()->GetCairoContext()),
                rc.x, rc.y, rc.width, rc.height);
            gtk_style_context_restore(sc);
        }
#else
        GdkWindowEdge edge =
            GetLayoutDirection() == wxLayout_RightToLeft ? GDK_WINDOW_EDGE_SOUTH_WEST :
                                                           GDK_WINDOW_EDGE_SOUTH_EAST;
        gtk_paint_resize_grip(gtk_widget_get_style(m_widget),
                            GTKGetDrawingWindow(),
                            gtk_widget_get_state(m_widget),
                            NULL,
                            m_widget,
                            "statusbar",
                            edge,
                            rc.x, rc.y, rc.width, rc.height );
#endif
    }
#endif // __WXGTK20__

    if (GetFont().IsOk())
        dc.SetFont(GetFont());

    // compute char height only once for all panes:
    int textHeight = dc.GetCharHeight();

    dc.SetBackgroundMode(wxBRUSHSTYLE_TRANSPARENT);
    for (size_t i = 0; i < m_panes.GetCount(); i ++)
        DrawField(dc, i, textHeight);
}

void wxStatusBarGeneric::OnSysColourChanged(wxSysColourChangedEvent& event)
{
    InitColours();

    // Propagate the event to the non-top-level children
    wxWindow::OnSysColourChanged(event);
}

#ifdef __WXGTK20__
void wxStatusBarGeneric::OnLeftDown(wxMouseEvent& event)
{
    int width, height;
    GetClientSize(&width, &height);

    GtkWidget* ancestor = gtk_widget_get_toplevel(m_widget);
#ifdef __WXGTK3__
    GdkRectangle rect;
    if (ancestor && gtk_window_get_resize_grip_area(GTK_WINDOW(ancestor), &rect) &&
        rect.width && rect.height)
    {
        ancestor = NULL;
    }
#endif

    if (ancestor && ShowsSizeGrip() && event.GetX() > width - height)
    {
        GdkWindow *source = GTKGetDrawingWindow();

        int org_x = 0;
        int org_y = 0;
        gdk_window_get_origin( source, &org_x, &org_y );

        if (GetLayoutDirection() == wxLayout_RightToLeft)
        {
            gtk_window_begin_resize_drag (GTK_WINDOW (ancestor),
                                  GDK_WINDOW_EDGE_SOUTH_WEST,
                                  1,
                                  org_x - event.GetX() + GetSize().x ,
                                  org_y + event.GetY(),
                                  0);
        }
        else
        {
            gtk_window_begin_resize_drag (GTK_WINDOW (ancestor),
                                  GDK_WINDOW_EDGE_SOUTH_EAST,
                                  1,
                                  org_x + event.GetX(),
                                  org_y + event.GetY(),
                                  0);
        }
    }
    else
    {
        event.Skip( true );
    }
}

void wxStatusBarGeneric::OnRightDown(wxMouseEvent& event)
{
    int width, height;
    GetClientSize(&width, &height);

    GtkWidget* ancestor = gtk_widget_get_toplevel(m_widget);
#ifdef __WXGTK3__
    GdkRectangle rect;
    if (ancestor && gtk_window_get_resize_grip_area(GTK_WINDOW(ancestor), &rect) &&
        rect.width && rect.height)
    {
        ancestor = NULL;
    }
#endif

    if (ancestor && ShowsSizeGrip() && event.GetX() > width - height)
    {
        GdkWindow *source = GTKGetDrawingWindow();

        int org_x = 0;
        int org_y = 0;
        gdk_window_get_origin( source, &org_x, &org_y );

        gtk_window_begin_move_drag (GTK_WINDOW (ancestor),
                                2,
                                org_x + event.GetX(),
                                org_y + event.GetY(),
                                0);
    }
    else
    {
        event.Skip( true );
    }
}
#endif // __WXGTK20__

void wxStatusBarGeneric::OnSize(wxSizeEvent& event)
{
    DoUpdateFieldWidths();

    event.Skip();
}

#endif // wxUSE_STATUSBAR
