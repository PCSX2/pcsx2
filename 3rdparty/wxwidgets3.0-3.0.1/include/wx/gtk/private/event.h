///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/private/event.h
// Purpose:     Helper functions for working with GDK and wx events
// Author:      Vaclav Slavik
// Created:     2011-10-14
// Copyright:   (c) 2011 Vaclav Slavik
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _GTK_PRIVATE_EVENT_H_
#define _GTK_PRIVATE_EVENT_H_

#if !GTK_CHECK_VERSION(2,10,0)
    // GTK+ can reliably detect Meta key state only since 2.10 when
    // GDK_META_MASK was introduced -- there wasn't any way to detect it
    // in older versions. wxGTK used GDK_MOD2_MASK for this purpose, but
    // GDK_MOD2_MASK is documented as:
    //
    //     the fifth modifier key (it depends on the modifier mapping of the X
    //     server which key is interpreted as this modifier)
    //
    // In other words, it isn't guaranteed to map to Meta. This is a real
    // problem: it is common to map NumLock to it (in fact, it's an exception
    // if the X server _doesn't_ use it for NumLock).  So the old code caused
    // wxKeyEvent::MetaDown() to always return true as long as NumLock was on
    // on many systems, which broke all applications using
    // wxKeyEvent::GetModifiers() to check modifiers state (see e.g.  here:
    // http://tinyurl.com/56lsk2).
    //
    // Because of this, it's better to not detect Meta key state at all than
    // to detect it incorrectly. Hence the following #define, which causes
    // m_metaDown to be always set to false.
    #define GDK_META_MASK 0
#endif

namespace wxGTKImpl
{

// init wxMouseEvent with the info from GdkEventXXX struct
template<typename T> void InitMouseEvent(wxWindowGTK *win,
                                         wxMouseEvent& event,
                                         T *gdk_event)
{
    event.m_shiftDown = (gdk_event->state & GDK_SHIFT_MASK) != 0;
    event.m_controlDown = (gdk_event->state & GDK_CONTROL_MASK) != 0;
    event.m_altDown = (gdk_event->state & GDK_MOD1_MASK) != 0;
    event.m_metaDown = (gdk_event->state & GDK_META_MASK) != 0;
    event.m_leftDown = (gdk_event->state & GDK_BUTTON1_MASK) != 0;
    event.m_middleDown = (gdk_event->state & GDK_BUTTON2_MASK) != 0;
    event.m_rightDown = (gdk_event->state & GDK_BUTTON3_MASK) != 0;

    // In gdk/win32 VK_XBUTTON1 is translated to GDK_BUTTON4_MASK
    // and VK_XBUTTON2 to GDK_BUTTON5_MASK. In x11/gdk buttons 4/5
    // are wheel rotation and buttons 8/9 don't change the state.
    event.m_aux1Down = (gdk_event->state & GDK_BUTTON4_MASK) != 0;
    event.m_aux2Down = (gdk_event->state & GDK_BUTTON5_MASK) != 0;

    wxPoint pt = win->GetClientAreaOrigin();
    event.m_x = (wxCoord)gdk_event->x - pt.x;
    event.m_y = (wxCoord)gdk_event->y - pt.y;

    if ((win->m_wxwindow) && (win->GetLayoutDirection() == wxLayout_RightToLeft))
    {
        // origin in the upper right corner
        GtkAllocation a;
        gtk_widget_get_allocation(win->m_wxwindow, &a);
        int window_width = a.width;
        event.m_x = window_width - event.m_x;
    }

    event.SetEventObject( win );
    event.SetId( win->GetId() );
    event.SetTimestamp( gdk_event->time );
}

} // namespace wxGTKImpl

#endif // _GTK_PRIVATE_EVENT_H_

