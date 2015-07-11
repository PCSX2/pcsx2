///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/setup_inc.h
// Purpose:     MSW-specific setup.h options
// Author:      Vadim Zeitlin
// Created:     2007-07-21 (extracted from wx/msw/setup0.h)
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ----------------------------------------------------------------------------
// Windows-only settings
// ----------------------------------------------------------------------------

// Set wxUSE_UNICODE_MSLU to 1 if you're compiling wxWidgets in Unicode mode
// and want to run your programs under Windows 9x and not only NT/2000/XP.
// This setting enables use of unicows.dll from MSLU (MS Layer for Unicode, see
// http://www.microsoft.com/globaldev/handson/dev/mslu_announce.mspx). Note
// that you will have to modify the makefiles to include unicows.lib import
// library as the first library (see installation instructions in install.txt
// to learn how to do it when building the library or samples).
//
// If your compiler doesn't have unicows.lib, you can get a version of it at
// http://libunicows.sourceforge.net
//
// Default is 0
//
// Recommended setting: 0 (1 if you want to deploy Unicode apps on 9x systems)
#ifndef wxUSE_UNICODE_MSLU
    #define wxUSE_UNICODE_MSLU 0
#endif

// Set this to 1 if you want to use wxWidgets and MFC in the same program. This
// will override some other settings (see below)
//
// Default is 0.
//
// Recommended setting: 0 unless you really have to use MFC
#define wxUSE_MFC           0

// Set this to 1 for generic OLE support: this is required for drag-and-drop,
// clipboard, OLE Automation. Only set it to 0 if your compiler is very old and
// can't compile/doesn't have the OLE headers.
//
// Default is 1.
//
// Recommended setting: 1
#define wxUSE_OLE           1

// Set this to 1 to enable wxAutomationObject class.
//
// Default is 1.
//
// Recommended setting: 1 if you need to control other applications via OLE
// Automation, can be safely set to 0 otherwise
#define wxUSE_OLE_AUTOMATION 1

// Set this to 1 to enable wxActiveXContainer class allowing to embed OLE
// controls in wx.
//
// Default is 1.
//
// Recommended setting: 1, required by wxMediaCtrl
#define wxUSE_ACTIVEX 1

// wxDC caching implementation
#define wxUSE_DC_CACHEING 1

// Set this to 1 to enable wxDIB class used internally for manipulating
// wxBitmap data.
//
// Default is 1, set it to 0 only if you don't use wxImage neither
//
// Recommended setting: 1 (without it conversion to/from wxImage won't work)
#define wxUSE_WXDIB 1

// Set to 0 to disable PostScript print/preview architecture code under Windows
// (just use Windows printing).
#define wxUSE_POSTSCRIPT_ARCHITECTURE_IN_MSW 1

// Set this to 1 to compile in wxRegKey class.
//
// Default is 1
//
// Recommended setting: 1, this is used internally by wx in a few places
#define wxUSE_REGKEY 1

// Set this to 1 to use RICHEDIT controls for wxTextCtrl with style wxTE_RICH
// which allows to put more than ~32Kb of text in it even under Win9x (NT
// doesn't have such limitation).
//
// Default is 1 for compilers which support it
//
// Recommended setting: 1, only set it to 0 if your compiler doesn't have
//                      or can't compile <richedit.h>
#define wxUSE_RICHEDIT  1

// Set this to 1 to use extra features of richedit v2 and later controls
//
// Default is 1 for compilers which support it
//
// Recommended setting: 1
#define wxUSE_RICHEDIT2 1

// Set this to 1 to enable support for the owner-drawn menu and listboxes. This
// is required by wxUSE_CHECKLISTBOX.
//
// Default is 1.
//
// Recommended setting: 1, set to 0 for a small library size reduction
#define wxUSE_OWNER_DRAWN 1

// Set this to 1 to enable MSW-specific wxTaskBarIcon::ShowBalloon() method. It
// is required by native wxNotificationMessage implementation.
//
// Default is 1 but disabled in wx/msw/chkconf.h if SDK is too old to contain
// the necessary declarations.
//
// Recommended setting: 1, set to 0 for a tiny library size reduction
#define wxUSE_TASKBARICON_BALLOONS 1

// Set to 1 to compile MS Windows XP theme engine support
#define wxUSE_UXTHEME           1

// Set to 1 to use InkEdit control (Tablet PC), if available
#define wxUSE_INKEDIT  0

// Set to 1 to enable .INI files based wxConfig implementation (wxIniConfig)
//
// Default is 0.
//
// Recommended setting: 0, nobody uses .INI files any more
#define wxUSE_INICONF 0

// ----------------------------------------------------------------------------
// Generic versions of native controls
// ----------------------------------------------------------------------------

// Set this to 1 to be able to use wxDatePickerCtrlGeneric in addition to the
// native wxDatePickerCtrl
//
// Default is 0.
//
// Recommended setting: 0, this is mainly used for testing
#define wxUSE_DATEPICKCTRL_GENERIC 0

// Set this to 1 to be able to use wxTimePickerCtrlGeneric in addition to the
// native wxTimePickerCtrl for the platforms that have the latter (MSW).
//
// Default is 0.
//
// Recommended setting: 0, this is mainly used for testing
#define wxUSE_TIMEPICKCTRL_GENERIC 0

// ----------------------------------------------------------------------------
// Crash debugging helpers
// ----------------------------------------------------------------------------

// Set this to 1 to be able to use wxCrashReport::Generate() to create mini
// dumps of your program when it crashes (or at any other moment)
//
// Default is 1 if supported by the compiler (VC++ and recent BC++ only).
//
// Recommended setting: 1, set to 0 if your programs never crash
#define wxUSE_CRASHREPORT 1
