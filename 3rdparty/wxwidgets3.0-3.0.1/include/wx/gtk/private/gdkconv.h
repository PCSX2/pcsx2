///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/private/gdkconv.h
// Purpose:     Helper functions for converting between GDK and wx types
// Author:      Vadim Zeitlin
// Created:     2009-11-10
// Copyright:   (c) 2009 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _GTK_PRIVATE_GDKCONV_H_
#define _GTK_PRIVATE_GDKCONV_H_

namespace wxGTKImpl
{

inline wxRect wxRectFromGDKRect(const GdkRectangle *r)
{
    return wxRect(r->x, r->y, r->width, r->height);
}

inline void wxRectToGDKRect(const wxRect& rect, GdkRectangle& r)
{
    r.x = rect.x;
    r.y = rect.y;
    r.width = rect.width;
    r.height = rect.height;
}

} // namespace wxGTKImpl

#endif // _GTK_PRIVATE_GDKCONV_H_

