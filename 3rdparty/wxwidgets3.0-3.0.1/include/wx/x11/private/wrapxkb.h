///////////////////////////////////////////////////////////////////////////////
// Name:        wx/x11/private/wrapxkb.h
// Purpose:     Private header wrapping X11/XKBlib.h inclusion.
// Author:      Vadim Zeitlin
// Created:     2012-05-07
// Copyright:   (c) 2012 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _X11_PRIVATE_WRAPXKB_H_
#define _X11_PRIVATE_WRAPXKB_H_

#ifdef HAVE_X11_XKBLIB_H
    /* under HP-UX and Solaris 2.6, at least, XKBlib.h defines structures with
     * field named "explicit" - which is, of course, an error for a C++
     * compiler. To be on the safe side, just redefine it everywhere. */
    #define explicit __wx_explicit

    #include <X11/XKBlib.h>

    #undef explicit
#endif // HAVE_X11_XKBLIB_H

#endif // _X11_PRIVATE_WRAPXKB_H_
