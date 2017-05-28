/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/wrapwin.h
// Purpose:     Wrapper around <windows.h>, to be included instead of it
// Author:      Vaclav Slavik
// Created:     2003/07/22
// Copyright:   (c) 2003 Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_WRAPWIN_H_
#define _WX_WRAPWIN_H_

#include "wx/platform.h"

// strict type checking to detect conversion from HFOO to HBAR at compile-time
#ifndef STRICT
    #define STRICT 1
#endif

// this macro tells windows.h to not define min() and max() as macros: we need
// this as otherwise they conflict with standard C++ functions
#ifndef NOMINMAX
    #define NOMINMAX
#endif // NOMINMAX


// before including windows.h, define version macros at (currently) maximal
// values because we do all our checks at run-time anyhow
#ifndef WINVER
    #define WINVER 0x0600
#endif

// define _WIN32_WINNT and _WIN32_IE to the highest possible values because we
// always check for the version of installed DLLs at runtime anyway (see
// wxGetWinVersion() and wxApp::GetComCtl32Version()) unless the user really
// doesn't want to use APIs only available on later OS versions and had defined
// them to (presumably lower) values
#ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600
#endif

#ifndef _WIN32_IE
    #define _WIN32_IE 0x0700
#endif

/* Deal with clash with __WINDOWS__ include guard */
#if defined(__WXWINCE__) && defined(__WINDOWS__)
#undef __WINDOWS__
#endif

// For IPv6 support, we must include winsock2.h before winsock.h, and
// windows.h include winsock.h so do it before including it
#if wxUSE_IPV6
    #include <winsock2.h>
#endif

#include <windows.h>

#if defined(__WXWINCE__) && !defined(__WINDOWS__)
#define __WINDOWS__
#endif

// #undef the macros defined in winsows.h which conflict with code elsewhere
#include "wx/msw/winundef.h"

#endif // _WX_WRAPWIN_H_
