/////////////////////////////////////////////////////////////////////////////
// Name:        wx/unix/utilsx11.h
// Purpose:     Miscellaneous X11 functions
// Author:      Mattia Barbon, Vaclav Slavik, Vadim Zeitlin
// Modified by:
// Created:     25.03.02
// Copyright:   (c) wxWidgets team
//              (c) 2010 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIX_UTILSX11_H_
#define _WX_UNIX_UTILSX11_H_

#include "wx/defs.h"
#include "wx/gdicmn.h"

#include <X11/Xlib.h>

// NB: Content of this header is for wxWidgets' private use! It is not
//     part of public API and may be modified or even disappear in the future!

#if defined(__WXMOTIF__) || defined(__WXGTK__) || defined(__WXX11__)

#if defined(__WXGTK__)
typedef void WXDisplay;
typedef void* WXWindow;
#endif
typedef unsigned long WXKeySym;

int wxCharCodeXToWX(WXKeySym keySym);
WXKeySym wxCharCodeWXToX(int id);
int wxUnicodeCharXToWX(WXKeySym keySym);

// Xlib keysym-unicode charactor pair
struct CodePair {
    unsigned int keySym;
    int uniChar;
};

class wxIconBundle;

void wxSetIconsX11( WXDisplay* display, WXWindow window,
                    const wxIconBundle& ib );


enum wxX11FullScreenMethod
{
    wxX11_FS_AUTODETECT = 0,
    wxX11_FS_WMSPEC,
    wxX11_FS_KDE,
    wxX11_FS_GENERIC
};

wxX11FullScreenMethod wxGetFullScreenMethodX11(WXDisplay* display,
                                               WXWindow rootWindow);

void wxSetFullScreenStateX11(WXDisplay* display, WXWindow rootWindow,
                             WXWindow window, bool show, wxRect *origSize,
                             wxX11FullScreenMethod method);


// Class wrapping X11 Display: it opens it in ctor and closes it in dtor.
class wxX11Display
{
public:
    wxX11Display() { m_dpy = XOpenDisplay(NULL); }
    ~wxX11Display() { if ( m_dpy ) XCloseDisplay(m_dpy); }

    operator Display *() const { return m_dpy; }

    // Using DefaultRootWindow() with an object of wxX11Display class doesn't
    // compile because it is a macro which tries to cast wxX11Display so
    // provide a convenient helper.
    Window DefaultRoot() const { return DefaultRootWindow(m_dpy); }

private:
    Display *m_dpy;

    wxDECLARE_NO_COPY_CLASS(wxX11Display);
};

#endif // __WXMOTIF__, __WXGTK__, __WXX11__

#endif // _WX_UNIX_UTILSX11_H_
