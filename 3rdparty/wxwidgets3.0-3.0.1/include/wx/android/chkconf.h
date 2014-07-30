///////////////////////////////////////////////////////////////////////////////
// Name:        wx/android/chkconf.h
// Purpose:     Android-specific configuration options checks
// Author:      Zsolt Bakcsi
// Modified by:
// Created:     2011-12-08
// RCS-ID:
// Copyright:   (c) wxWidgets team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_ANDROID_CHKCONF_H_
#define _WX_ANDROID_CHKCONF_H_

// ----------------------------------------------------------------------------
// Disable features which don't work (yet) or don't make sense under Android.
// ----------------------------------------------------------------------------

// please keep the list in alphabetic order except for closely related settings
// (e.g. wxUSE_ENH_METAFILE is put immediately after wxUSE_METAFILE)


// ----------------------------------------------------------------------------
// These are disabled because they are TODO. Or to decide whether to do or not.
// ----------------------------------------------------------------------------

#undef wxUSE_CONFIG
#define wxUSE_CONFIG 0

// This compiles, but not yet tested, so:
#undef wxUSE_CONSOLE_EVENTLOOP
#define wxUSE_CONSOLE_EVENTLOOP 0

#undef wxUSE_DEBUGREPORT
#define wxUSE_DEBUGREPORT 0

#undef wxUSE_DIALUP_MANAGER
#define wxUSE_DIALUP_MANAGER 0

#undef wxUSE_DISPLAY
#define wxUSE_DISPLAY 0

#undef wxUSE_DYNAMIC_LOADER
#define wxUSE_DYNAMIC_LOADER 0

#undef wxUSE_DYNLIB_CLASS
#define wxUSE_DYNLIB_CLASS 0

#undef wxUSE_FSVOLUME
#define wxUSE_FSVOLUME 0

// Compile-time errors when last tried (wxHAS_INOTIFY, wxHAS_KQUEUE)
#undef wxUSE_FSWATCHER
#define wxUSE_FSWATCHER 0

// Seems like Android lacks locale support. TODO: check!
// Hint:
// http://groups.google.com/group/android-ndk/browse_thread/thread/ffd012a047ec2392?pli=1
//  "Android doesn't provide locale support in its C and C++ runtimes.
//  This is handled at a higher-level in the application stack, using ICU
//  (which is not exposed by the NDK, since the ABI is very volatile, and the
//  set of built-in tables varies from device to device, based on customization
//  / size reasons).
//  You might want to use a different locale implementation. The STLport and GNU
//  libstdc++ do provide then if you're using C++."
#undef wxUSE_INTL
#define wxUSE_INTL 0
#undef wxUSE_XLOCALE
#define wxUSE_XLOCALE 0

#undef wxUSE_IPC
#define wxUSE_IPC 0

#undef wxUSE_MEDIACTRL
#define wxUSE_MEDIACTRL 0

#undef wxUSE_ON_FATAL_EXCEPTION
#define wxUSE_ON_FATAL_EXCEPTION 0

#undef wxUSE_REGEX
#define wxUSE_REGEX 0

#undef wxUSE_STDPATHS
#define wxUSE_STDPATHS 0

#undef wxUSE_STACKWALKER
#define wxUSE_STACKWALKER 0

#undef wxUSE_MIMETYPE
#define wxUSE_MIMETYPE 0

#undef wxUSE_REGEX
#define wxUSE_REGEX 0

#undef wxUSE_REGKEY
#define wxUSE_REGKEY 0

#undef wxUSE_SNGLINST_CHECKER
#define wxUSE_SNGLINST_CHECKER 0

#undef wxUSE_SOUND
#define wxUSE_SOUND 0

#undef wxUSE_SYSTEM_OPTIONS
#define wxUSE_SYSTEM_OPTIONS 0

#undef wxUSE_XRC
#define wxUSE_XRC 0


// ----------------------------------------------------------------------------
// GUI is completely TODO.
// ----------------------------------------------------------------------------

#undef wxUSE_COLOURPICKERCTRL
#define wxUSE_COLOURPICKERCTRL 0

#undef wxUSE_COLOURDLG
#define wxUSE_COLOURDLG 0

#undef wxUSE_FONTENUM
#define wxUSE_FONTENUM 0

#undef wxUSE_FONTMAP
#define wxUSE_FONTMAP 0

#undef wxUSE_HELP
#define wxUSE_HELP 0

#undef wxUSE_HTML
#define wxUSE_HTML 0

#undef wxUSE_LISTBOOK
#define wxUSE_LISTBOOK 0

#undef wxUSE_OWNER_DRAWN
#define wxUSE_OWNER_DRAWN 0

#undef wxUSE_NOTEBOOK
#define wxUSE_NOTEBOOK 0

#undef wxUSE_RICHEDIT
#define wxUSE_RICHEDIT 0
#undef wxUSE_RICHEDIT2
#define wxUSE_RICHEDIT2 0

#undef wxUSE_STATUSBAR
#define wxUSE_STATUSBAR 0

// Are tooltips useful at all on a touch screen?
#undef wxUSE_TOOLTIPS
#define wxUSE_TOOLTIPS 0

#undef wxUSE_WXHTML_HELP
#define wxUSE_WXHTML_HELP 0


// ----------------------------------------------------------------------------
// All image classes are TODO.
// ----------------------------------------------------------------------------

#undef wxUSE_IMAGE
#define wxUSE_IMAGE         0

#undef wxUSE_LIBPNG
#define wxUSE_LIBPNG        0

#undef wxUSE_LIBJPEG
#define wxUSE_LIBJPEG       0

#undef wxUSE_LIBTIFF
#define wxUSE_LIBTIFF       0

#undef wxUSE_TGA
#define wxUSE_TGA           0

#undef wxUSE_GIF
#define wxUSE_GIF           0

#undef wxUSE_PNM
#define wxUSE_PNM           0

#undef wxUSE_PCX
#define wxUSE_PCX           0

#undef wxUSE_IFF
#define wxUSE_IFF           0

#undef wxUSE_XPM
#define wxUSE_XPM           0

#undef wxUSE_ICO_CUR
#define wxUSE_ICO_CUR       0

#undef wxUSE_PALETTE
#define wxUSE_PALETTE       0



// ----------------------------------------------------------------------------
// These are disabled because they don't make sense, are not supported, or
// would require too much effort.
// ----------------------------------------------------------------------------

// Unnecessary under Android, probably it doesn't even compile.
#undef wxUSE_AUI
#define wxUSE_AUI 0

// No command line on Android.
#undef wxUSE_CMDLINE_PARSER
#define wxUSE_CMDLINE_PARSER 0

// No joystick on Android devices.
// (What about using the direction sensor or the accelerometer?)
#undef wxUSE_JOYSTICK
#define wxUSE_JOYSTICK 0

// No MDI under Android. Well, no GUI at all (yet).
#undef wxUSE_MDI
#define wxUSE_MDI 0
#undef wxUSE_MDI_ARCHITECTURE
#define wxUSE_MDI_ARCHITECTURE 0

// No printing support on Android (2011).
// Although 3rd party SDKs may exist (I know of one payware).
#undef wxUSE_PRINTING_ARCHITECTURE
#define wxUSE_PRINTING_ARCHITECTURE 0


#endif // _WX_ANDROID_CHKCONF_H_
