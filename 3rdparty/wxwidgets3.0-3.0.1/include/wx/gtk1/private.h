///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk1/private.h
// Purpose:     wxGTK private macros, functions &c
// Author:      Vadim Zeitlin
// Modified by:
// Created:     12.03.02
// Copyright:   (c) 2002 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_PRIVATE_H_
#define _WX_GTK_PRIVATE_H_

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "wx/event.h"

// fail all version tests if the GTK+ version is so ancient that it doesn't
// even have GTK_CHECK_VERSION
#ifndef GTK_CHECK_VERSION
    #define GTK_CHECK_VERSION(a, b, c) 0
#endif

#define wxGTK_CONV(s) s.c_str()
#define wxGTK_CONV_BACK(s) s


// child is not a member of GTK_BUTTON() any more in GTK+ 2.0
#define BUTTON_CHILD(w) GTK_BUTTON((w))->child

// event_window has disappeared from GtkToggleButton in GTK+ 2.0
#define TOGGLE_BUTTON_EVENT_WIN(w) GTK_TOGGLE_BUTTON((w))->event_window

// gtk_editable_{copy|cut|paste}_clipboard() had an extra argument under
// previous GTK+ versions but no more
#if defined(__WXGTK20__) || (GTK_MINOR_VERSION > 0)
    #define DUMMY_CLIPBOARD_ARG
#else
    #define DUMMY_CLIPBOARD_ARG  ,0
#endif

// _GtkEditable is private in GTK2
#define GET_EDITABLE_POS(w) GTK_EDITABLE((w))->current_pos
#define SET_EDITABLE_POS(w, pos) \
    GTK_EDITABLE((w))->current_pos = (pos)

// this GtkNotebook struct field has been renamed in GTK2
#define NOTEBOOK_PANEL(nb)  GTK_NOTEBOOK(nb)->panel

#define SCROLLBAR_CBACK_ARG
#define GET_SCROLL_TYPE(w)   GTK_RANGE((w))->scroll_type

// translate a GTK+ scroll type to a wxEventType
inline wxEventType GtkScrollTypeToWx(guint scrollType)
{
    wxEventType command;
    switch ( scrollType )
    {
        case GTK_SCROLL_STEP_BACKWARD:
            command = wxEVT_SCROLL_LINEUP;
            break;

        case GTK_SCROLL_STEP_FORWARD:
            command = wxEVT_SCROLL_LINEDOWN;
            break;

        case GTK_SCROLL_PAGE_BACKWARD:
            command = wxEVT_SCROLL_PAGEUP;
            break;

        case GTK_SCROLL_PAGE_FORWARD:
            command = wxEVT_SCROLL_PAGEDOWN;
            break;

        default:
            command = wxEVT_SCROLL_THUMBTRACK;
    }

    return command;
}

inline wxEventType GtkScrollWinTypeToWx(guint scrollType)
{
    // GtkScrollTypeToWx() returns SCROLL_XXX, not SCROLLWIN_XXX as we need
    return GtkScrollTypeToWx(scrollType) +
            wxEVT_SCROLLWIN_TOP - wxEVT_SCROLL_TOP;
}

// Needed for implementing e.g. combobox on wxGTK within a modal dialog.
void wxAddGrab(wxWindow* window);
void wxRemoveGrab(wxWindow* window);

#endif // _WX_GTK_PRIVATE_H_

