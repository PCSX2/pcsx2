///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/wince/chkconf.h
// Purpose:     WinCE-specific configuration options checks
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2005-03-07
// Copyright:   (c) 2005 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_WINCE_CHKCONF_H_
#define _WX_MSW_WINCE_CHKCONF_H_

// ----------------------------------------------------------------------------
// Disable features which don't work or don't make sense under CE
// ----------------------------------------------------------------------------

// please keep the list in alphabetic order except for closely related settings
// (e.g. wxUSE_ENH_METAFILE is put immediately after wxUSE_METAFILE)

#undef wxUSE_DEBUGREPORT
#define wxUSE_DEBUGREPORT 0

#if _WIN32_WCE < 400
    // not enough API and lack of ddraw.h
    #undef wxUSE_DISPLAY
    #define wxUSE_DISPLAY 0
#endif

// eVC doesn't have standard streams
#ifdef __EVC4__
    #undef wxUSE_STD_IOSTREAM
    #define wxUSE_STD_IOSTREAM 0
#endif

// wxFSVolume currently doesn't compile under CE and it's not clear if it makes
// sense at all there (the drives and their names are fixed on CE systems)
#undef wxUSE_FSVOLUME
#define wxUSE_FSVOLUME 0

// no .INI files API under CE
#undef wxUSE_INICONF
#define wxUSE_INICONF 0

// DDE doesn't exist under WinCE and wxIPC is DDE-based under MSW
#undef wxUSE_IPC
#define wxUSE_IPC 0

// doesn't make sense for CE devices and doesn't compile anyhow
#undef wxUSE_JOYSTICK
#define wxUSE_JOYSTICK 0

// libtiff doesn't build with eVC but is ok with VC8
#ifdef __EVC4__
    #undef wxUSE_LIBTIFF
    #define wxUSE_LIBTIFF 0
#endif

// no AUI under CE: it's unnecessary and currently doesn't compile
#undef wxUSE_AUI
#define wxUSE_AUI 0

// no MDI under CE
#undef wxUSE_MDI
#define wxUSE_MDI 0
#undef wxUSE_MDI_ARCHITECTURE
#define wxUSE_MDI_ARCHITECTURE 0

// metafiles are not supported neither
#undef wxUSE_METAFILE
#define wxUSE_METAFILE 0
#undef wxUSE_ENH_METAFILE
#define wxUSE_ENH_METAFILE 0

// not sure if this is supported by CE but it doesn't compile currently anyhow
#undef wxUSE_MS_HTML_HELP
#define wxUSE_MS_HTML_HELP 0

// eVC doesn't support SEH
#undef wxUSE_ON_FATAL_EXCEPTION
#define wxUSE_ON_FATAL_EXCEPTION 0

// no owner drawn controls (not sure if this is possible at all but in any case
// the code doesn't currently compile)
#undef wxUSE_OWNER_DRAWN
#define wxUSE_OWNER_DRAWN 0

#undef wxUSE_PRINTING_ARCHITECTURE
#define wxUSE_PRINTING_ARCHITECTURE 0

// regex doesn't build with eVC but is ok with VC8
#ifdef __EVC4__
    #undef wxUSE_REGEX
    #define wxUSE_REGEX 0
#endif

#undef wxUSE_RICHEDIT
#define wxUSE_RICHEDIT 0
#undef wxUSE_RICHEDIT2
#define wxUSE_RICHEDIT2 0

// Standard SDK lacks a few things, forcefully disable them
#ifdef WCE_PLATFORM_STANDARDSDK
    // no shell functions support
    #undef wxUSE_STDPATHS
    #define wxUSE_STDPATHS 0
#endif // WCE_PLATFORM_STANDARDSDK

// there is no support for balloon taskbar icons
#undef wxUSE_TASKBARICON_BALLOONS
#define wxUSE_TASKBARICON_BALLOONS 0

// not sure if this is supported by eVC but VC8 SDK lacks the tooltips control
// related declarations
#if wxCHECK_VISUALC_VERSION(8)
    #undef wxUSE_TOOLTIPS
    #define wxUSE_TOOLTIPS 0
#endif

#undef wxUSE_UNICODE_MSLU
#define wxUSE_UNICODE_MSLU 0

#undef wxUSE_UXTHEME
#define wxUSE_UXTHEME 0

#undef wxUSE_WXHTML_HELP
#define wxUSE_WXHTML_HELP 0


// Disable features which don't make sense for MS Smartphones
// (due to pointer device usage, limited controls or dialogs, file system)
#if defined(__SMARTPHONE__)
    #undef wxUSE_LISTBOOK
    #define wxUSE_LISTBOOK 0

    #undef wxUSE_NOTEBOOK
    #define wxUSE_NOTEBOOK 0

    #undef wxUSE_STATUSBAR
    #define wxUSE_STATUSBAR 0

    #undef wxUSE_COLOURPICKERCTRL
    #define wxUSE_COLOURPICKERCTRL 0

    #undef wxUSE_COLOURDLG
    #define wxUSE_COLOURDLG 0
#endif // __SMARTPHONE__

#endif // _WX_MSW_WINCE_CHKCONF_H_

