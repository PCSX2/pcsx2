/*
 * Name:        wx/chkconf.h
 * Purpose:     check the config settings for consistency
 * Author:      Vadim Zeitlin
 * Modified by:
 * Created:     09.08.00
 * Copyright:   (c) 2000 Vadim Zeitlin <vadim@wxwidgets.org>
 * Licence:     wxWindows licence
 */

/* THIS IS A C FILE, DON'T USE C++ FEATURES (IN PARTICULAR COMMENTS) IN IT */
#ifndef _WX_CHKCONF_H_
#define _WX_CHKCONF_H_

/*
              **************************************************
              PLEASE READ THIS IF YOU GET AN ERROR IN THIS FILE!
              **************************************************

    If you get an error saying "wxUSE_FOO must be defined", it means that you
    are not using the correct up-to-date version of setup.h. This happens most
    often when using svn or daily snapshots and a new symbol was added to
    setup0.h and you haven't updated your local setup.h to reflect it. If
    this is the case, you need to propagate the changes from setup0.h to your
    setup.h and, if using makefiles under MSW, also remove setup.h under the
    build directory (lib/$(COMPILER)_{lib,dll}/msw[u][d][dll]/wx) so that
    the new setup.h is copied there.

    If you get an error of the form "wxFoo requires wxBar", then the settings
    in your setup.h are inconsistent. You have the choice between correcting
    them manually or commenting out #define wxABORT_ON_CONFIG_ERROR below to
    try to correct the problems automatically (not really recommended but
    might work).
 */

/*
   This file has the following sections:
    1. checks that all wxUSE_XXX symbols we use are defined
     a) first the non-GUI ones
     b) then the GUI-only ones
    2. platform-specific checks done in the platform headers
    3. generic consistency checks
     a) first the non-GUI ones
     b) then the GUI-only ones
 */

/*
   this global setting determines what should we do if the setting FOO
   requires BAR and BAR is not set: we can either silently unset FOO as well
   (do this if you're trying to build the smallest possible library) or give an
   error and abort (default as leads to least surprising behaviour)
 */
#define wxABORT_ON_CONFIG_ERROR

/*
   global features
 */

/*
    If we're compiling without support for threads/exceptions we have to
    disable the corresponding features.
 */
#ifdef wxNO_THREADS
#   undef wxUSE_THREADS
#   define wxUSE_THREADS 0
#endif /* wxNO_THREADS */

#ifdef wxNO_EXCEPTIONS
#   undef wxUSE_EXCEPTIONS
#   define wxUSE_EXCEPTIONS 0
#endif /* wxNO_EXCEPTIONS */

/* we also must disable exceptions if compiler doesn't support them */
#if defined(_MSC_VER) && !defined(_CPPUNWIND)
#   undef wxUSE_EXCEPTIONS
#   define wxUSE_EXCEPTIONS 0
#endif /* VC++ without exceptions support */


/*
   Section 1a: tests for non GUI features.

   please keep the options in alphabetical order!
 */

#ifndef wxUSE_ANY
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_ANY must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_ANY 0
#   endif
#endif /* wxUSE_ANY */

#ifndef wxUSE_COMPILER_TLS
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_COMPILER_TLS must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_COMPILER_TLS 0
#   endif
#endif /* !defined(wxUSE_COMPILER_TLS) */

#ifndef wxUSE_CONSOLE_EVENTLOOP
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_CONSOLE_EVENTLOOP must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_CONSOLE_EVENTLOOP 0
#   endif
#endif /* !defined(wxUSE_CONSOLE_EVENTLOOP) */

#ifndef wxUSE_DYNLIB_CLASS
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_DYNLIB_CLASS must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_DYNLIB_CLASS 0
#   endif
#endif /* !defined(wxUSE_DYNLIB_CLASS) */

#ifndef wxUSE_EXCEPTIONS
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_EXCEPTIONS must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_EXCEPTIONS 0
#   endif
#endif /* !defined(wxUSE_EXCEPTIONS) */

#ifndef wxUSE_FILE_HISTORY
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_FILE_HISTORY must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_FILE_HISTORY 0
#   endif
#endif /* !defined(wxUSE_FILE_HISTORY) */

#ifndef wxUSE_FILESYSTEM
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_FILESYSTEM must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_FILESYSTEM 0
#   endif
#endif /* !defined(wxUSE_FILESYSTEM) */

#ifndef wxUSE_FS_ARCHIVE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_FS_ARCHIVE must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_FS_ARCHIVE 0
#   endif
#endif /* !defined(wxUSE_FS_ARCHIVE) */

#ifndef wxUSE_FSVOLUME
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_FSVOLUME must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_FSVOLUME 0
#   endif
#endif /* !defined(wxUSE_FSVOLUME) */

#ifndef wxUSE_FSWATCHER
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_FSWATCHER must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_FSWATCHER 0
#   endif
#endif /* !defined(wxUSE_FSWATCHER) */

#ifndef wxUSE_DYNAMIC_LOADER
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_DYNAMIC_LOADER must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_DYNAMIC_LOADER 0
#   endif
#endif /* !defined(wxUSE_DYNAMIC_LOADER) */

#ifndef wxUSE_INTL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_INTL must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_INTL 0
#   endif
#endif /* !defined(wxUSE_INTL) */

#ifndef wxUSE_IPV6
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_IPV6 must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_IPV6 0
#   endif
#endif /* !defined(wxUSE_IPV6) */

#ifndef wxUSE_LOG
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_LOG must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_LOG 0
#   endif
#endif /* !defined(wxUSE_LOG) */

#ifndef wxUSE_LONGLONG
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_LONGLONG must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_LONGLONG 0
#   endif
#endif /* !defined(wxUSE_LONGLONG) */

#ifndef wxUSE_MIMETYPE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_MIMETYPE must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_MIMETYPE 0
#   endif
#endif /* !defined(wxUSE_MIMETYPE) */

#ifndef wxUSE_ON_FATAL_EXCEPTION
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_ON_FATAL_EXCEPTION must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_ON_FATAL_EXCEPTION 0
#   endif
#endif /* !defined(wxUSE_ON_FATAL_EXCEPTION) */

#ifndef wxUSE_PRINTF_POS_PARAMS
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_PRINTF_POS_PARAMS must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_PRINTF_POS_PARAMS 0
#   endif
#endif /* !defined(wxUSE_PRINTF_POS_PARAMS) */

#ifndef wxUSE_PROTOCOL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_PROTOCOL must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_PROTOCOL 0
#   endif
#endif /* !defined(wxUSE_PROTOCOL) */

/* we may not define wxUSE_PROTOCOL_XXX if wxUSE_PROTOCOL is set to 0 */
#if !wxUSE_PROTOCOL
#   undef wxUSE_PROTOCOL_HTTP
#   undef wxUSE_PROTOCOL_FTP
#   undef wxUSE_PROTOCOL_FILE
#   define wxUSE_PROTOCOL_HTTP 0
#   define wxUSE_PROTOCOL_FTP 0
#   define wxUSE_PROTOCOL_FILE 0
#endif /* wxUSE_PROTOCOL */

#ifndef wxUSE_PROTOCOL_HTTP
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_PROTOCOL_HTTP must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_PROTOCOL_HTTP 0
#   endif
#endif /* !defined(wxUSE_PROTOCOL_HTTP) */

#ifndef wxUSE_PROTOCOL_FTP
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_PROTOCOL_FTP must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_PROTOCOL_FTP 0
#   endif
#endif /* !defined(wxUSE_PROTOCOL_FTP) */

#ifndef wxUSE_PROTOCOL_FILE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_PROTOCOL_FILE must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_PROTOCOL_FILE 0
#   endif
#endif /* !defined(wxUSE_PROTOCOL_FILE) */

#ifndef wxUSE_REGEX
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_REGEX must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_REGEX 0
#   endif
#endif /* !defined(wxUSE_REGEX) */

#ifndef wxUSE_STDPATHS
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_STDPATHS must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_STDPATHS 1
#   endif
#endif /* !defined(wxUSE_STDPATHS) */

#ifndef wxUSE_XML
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_XML must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_XML 0
#   endif
#endif /* !defined(wxUSE_XML) */

#ifndef wxUSE_SOCKETS
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_SOCKETS must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_SOCKETS 0
#   endif
#endif /* !defined(wxUSE_SOCKETS) */

#ifndef wxUSE_STD_CONTAINERS
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_STD_CONTAINERS must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_STD_CONTAINERS 0
#   endif
#endif /* !defined(wxUSE_STD_CONTAINERS) */

#ifndef wxUSE_STD_STRING_CONV_IN_WXSTRING
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_STD_STRING_CONV_IN_WXSTRING must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_STD_STRING_CONV_IN_WXSTRING 0
#   endif
#endif /* !defined(wxUSE_STD_STRING_CONV_IN_WXSTRING) */

#ifndef wxUSE_STREAMS
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_STREAMS must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_STREAMS 0
#   endif
#endif /* !defined(wxUSE_STREAMS) */

#ifndef wxUSE_STOPWATCH
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_STOPWATCH must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_STOPWATCH 0
#   endif
#endif /* !defined(wxUSE_STOPWATCH) */

#ifndef wxUSE_TEXTBUFFER
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TEXTBUFFER must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_TEXTBUFFER 0
#   endif
#endif /* !defined(wxUSE_TEXTBUFFER) */

#ifndef wxUSE_TEXTFILE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TEXTFILE must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_TEXTFILE 0
#   endif
#endif /* !defined(wxUSE_TEXTFILE) */

#ifndef wxUSE_UNICODE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_UNICODE must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_UNICODE 0
#   endif
#endif /* !defined(wxUSE_UNICODE) */

#ifndef wxUSE_URL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_URL must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_URL 0
#   endif
#endif /* !defined(wxUSE_URL) */

#ifndef wxUSE_VARIANT
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_VARIANT must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_VARIANT 0
#   endif
#endif /* wxUSE_VARIANT */

#ifndef wxUSE_XLOCALE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_XLOCALE must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_XLOCALE 0
#   endif
#endif /* !defined(wxUSE_XLOCALE) */

/*
   Section 1b: all these tests are for GUI only.

   please keep the options in alphabetical order!
 */
#if wxUSE_GUI

/*
   all of the settings tested below must be defined or we'd get an error from
   preprocessor about invalid integer expression
 */

#ifndef wxUSE_ABOUTDLG
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_ABOUTDLG must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_ABOUTDLG 0
#   endif
#endif /* !defined(wxUSE_ABOUTDLG) */

#ifndef wxUSE_ACCEL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_ACCEL must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_ACCEL 0
#   endif
#endif /* !defined(wxUSE_ACCEL) */

#ifndef wxUSE_ACCESSIBILITY
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_ACCESSIBILITY must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_ACCESSIBILITY 0
#   endif
#endif /* !defined(wxUSE_ACCESSIBILITY) */

#ifndef wxUSE_ANIMATIONCTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_ANIMATIONCTRL must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_ANIMATIONCTRL 0
#   endif
#endif /* !defined(wxUSE_ANIMATIONCTRL) */

#ifndef wxUSE_ARTPROVIDER_STD
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_ARTPROVIDER_STD must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_ARTPROVIDER_STD 0
#   endif
#endif /* !defined(wxUSE_ARTPROVIDER_STD) */

#ifndef wxUSE_ARTPROVIDER_TANGO
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_ARTPROVIDER_TANGO must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_ARTPROVIDER_TANGO 0
#   endif
#endif /* !defined(wxUSE_ARTPROVIDER_TANGO) */

#ifndef wxUSE_AUTOID_MANAGEMENT
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_AUTOID_MANAGEMENT must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_AUTOID_MANAGEMENT 0
#   endif
#endif /* !defined(wxUSE_AUTOID_MANAGEMENT) */

#ifndef wxUSE_BITMAPCOMBOBOX
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_BITMAPCOMBOBOX must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_BITMAPCOMBOBOX 0
#   endif
#endif /* !defined(wxUSE_BITMAPCOMBOBOX) */

#ifndef wxUSE_BMPBUTTON
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_BMPBUTTON must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_BMPBUTTON 0
#   endif
#endif /* !defined(wxUSE_BMPBUTTON) */

#ifndef wxUSE_BUTTON
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_BUTTON must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_BUTTON 0
#   endif
#endif /* !defined(wxUSE_BUTTON) */

#ifndef wxUSE_CAIRO
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_CAIRO must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_CAIRO 0
#   endif
#endif /* !defined(wxUSE_CAIRO) */

#ifndef wxUSE_CALENDARCTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_CALENDARCTRL must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_CALENDARCTRL 0
#   endif
#endif /* !defined(wxUSE_CALENDARCTRL) */

#ifndef wxUSE_CARET
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_CARET must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_CARET 0
#   endif
#endif /* !defined(wxUSE_CARET) */

#ifndef wxUSE_CHECKBOX
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_CHECKBOX must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_CHECKBOX 0
#   endif
#endif /* !defined(wxUSE_CHECKBOX) */

#ifndef wxUSE_CHECKLISTBOX
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_CHECKLISTBOX must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_CHECKLISTBOX 0
#   endif
#endif /* !defined(wxUSE_CHECKLISTBOX) */

#ifndef wxUSE_CHOICE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_CHOICE must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_CHOICE 0
#   endif
#endif /* !defined(wxUSE_CHOICE) */

#ifndef wxUSE_CHOICEBOOK
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_CHOICEBOOK must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_CHOICEBOOK 0
#   endif
#endif /* !defined(wxUSE_CHOICEBOOK) */

#ifndef wxUSE_CHOICEDLG
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_CHOICEDLG must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_CHOICEDLG 0
#   endif
#endif /* !defined(wxUSE_CHOICEDLG) */

#ifndef wxUSE_CLIPBOARD
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_CLIPBOARD must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_CLIPBOARD 0
#   endif
#endif /* !defined(wxUSE_CLIPBOARD) */

#ifndef wxUSE_COLLPANE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_COLLPANE must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_COLLPANE 0
#   endif
#endif /* !defined(wxUSE_COLLPANE) */

#ifndef wxUSE_COLOURDLG
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_COLOURDLG must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_COLOURDLG 0
#   endif
#endif /* !defined(wxUSE_COLOURDLG) */

#ifndef wxUSE_COLOURPICKERCTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_COLOURPICKERCTRL must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_COLOURPICKERCTRL 0
#   endif
#endif /* !defined(wxUSE_COLOURPICKERCTRL) */

#ifndef wxUSE_COMBOBOX
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_COMBOBOX must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_COMBOBOX 0
#   endif
#endif /* !defined(wxUSE_COMBOBOX) */

#ifndef wxUSE_COMMANDLINKBUTTON
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_COMMANDLINKBUTTON must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_COMMANDLINKBUTTON 0
#   endif
#endif /* !defined(wxUSE_COMMANDLINKBUTTON) */

#ifndef wxUSE_COMBOCTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_COMBOCTRL must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_COMBOCTRL 0
#   endif
#endif /* !defined(wxUSE_COMBOCTRL) */

#ifndef wxUSE_DATAOBJ
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_DATAOBJ must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_DATAOBJ 0
#   endif
#endif /* !defined(wxUSE_DATAOBJ) */

#ifndef wxUSE_DATAVIEWCTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_DATAVIEWCTRL must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_DATAVIEWCTRL 0
#   endif
#endif /* !defined(wxUSE_DATAVIEWCTRL) */

#ifndef wxUSE_DATEPICKCTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_DATEPICKCTRL must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_DATEPICKCTRL 0
#   endif
#endif /* !defined(wxUSE_DATEPICKCTRL) */

#ifndef wxUSE_DC_TRANSFORM_MATRIX
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_DC_TRANSFORM_MATRIX must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_DC_TRANSFORM_MATRIX 1
#   endif
#endif /* wxUSE_DC_TRANSFORM_MATRIX */

#ifndef wxUSE_DIRPICKERCTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_DIRPICKERCTRL must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_DIRPICKERCTRL 0
#   endif
#endif /* !defined(wxUSE_DIRPICKERCTRL) */

#ifndef wxUSE_DISPLAY
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_DISPLAY must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_DISPLAY 0
#   endif
#endif /* !defined(wxUSE_DISPLAY) */

#ifndef wxUSE_DOC_VIEW_ARCHITECTURE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_DOC_VIEW_ARCHITECTURE must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_DOC_VIEW_ARCHITECTURE 0
#   endif
#endif /* !defined(wxUSE_DOC_VIEW_ARCHITECTURE) */

#ifndef wxUSE_FILECTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_FILECTRL must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_FILECTRL 0
#   endif
#endif /* !defined(wxUSE_FILECTRL) */

#ifndef wxUSE_FILEDLG
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_FILEDLG must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_FILEDLG 0
#   endif
#endif /* !defined(wxUSE_FILEDLG) */

#ifndef wxUSE_FILEPICKERCTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_FILEPICKERCTRL must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_FILEPICKERCTRL 0
#   endif
#endif /* !defined(wxUSE_FILEPICKERCTRL) */

#ifndef wxUSE_FONTDLG
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_FONTDLG must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_FONTDLG 0
#   endif
#endif /* !defined(wxUSE_FONTDLG) */

#ifndef wxUSE_FONTMAP
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_FONTMAP must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_FONTMAP 0
#   endif
#endif /* !defined(wxUSE_FONTMAP) */

#ifndef wxUSE_FONTPICKERCTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_FONTPICKERCTRL must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_FONTPICKERCTRL 0
#   endif
#endif /* !defined(wxUSE_FONTPICKERCTRL) */

#ifndef wxUSE_GAUGE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_GAUGE must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_GAUGE 0
#   endif
#endif /* !defined(wxUSE_GAUGE) */

#ifndef wxUSE_GRAPHICS_CONTEXT
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_GRAPHICS_CONTEXT must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_GRAPHICS_CONTEXT 0
#   endif
#endif /* !defined(wxUSE_GRAPHICS_CONTEXT) */


#ifndef wxUSE_GRID
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_GRID must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_GRID 0
#   endif
#endif /* !defined(wxUSE_GRID) */

#ifndef wxUSE_HEADERCTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_HEADERCTRL must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_HEADERCTRL 0
#   endif
#endif /* !defined(wxUSE_HEADERCTRL) */

#ifndef wxUSE_HELP
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_HELP must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_HELP 0
#   endif
#endif /* !defined(wxUSE_HELP) */

#ifndef wxUSE_HYPERLINKCTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_HYPERLINKCTRL must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_HYPERLINKCTRL 0
#   endif
#endif /* !defined(wxUSE_HYPERLINKCTRL) */

#ifndef wxUSE_HTML
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_HTML must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_HTML 0
#   endif
#endif /* !defined(wxUSE_HTML) */

#ifndef wxUSE_LIBMSPACK
#   if !defined(__UNIX__)
        /* set to 0 on platforms that don't have libmspack */
#       define wxUSE_LIBMSPACK 0
#   else
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxUSE_LIBMSPACK must be defined, please read comment near the top of this file."
#       else
#           define wxUSE_LIBMSPACK 0
#       endif
#   endif
#endif /* !defined(wxUSE_LIBMSPACK) */

#ifndef wxUSE_ICO_CUR
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_ICO_CUR must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_ICO_CUR 0
#   endif
#endif /* !defined(wxUSE_ICO_CUR) */

#ifndef wxUSE_IFF
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_IFF must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_IFF 0
#   endif
#endif /* !defined(wxUSE_IFF) */

#ifndef wxUSE_IMAGLIST
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_IMAGLIST must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_IMAGLIST 0
#   endif
#endif /* !defined(wxUSE_IMAGLIST) */

#ifndef wxUSE_INFOBAR
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_INFOBAR must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_INFOBAR 0
#   endif
#endif /* !defined(wxUSE_INFOBAR) */

#ifndef wxUSE_JOYSTICK
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_JOYSTICK must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_JOYSTICK 0
#   endif
#endif /* !defined(wxUSE_JOYSTICK) */

#ifndef wxUSE_LISTBOOK
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_LISTBOOK must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_LISTBOOK 0
#   endif
#endif /* !defined(wxUSE_LISTBOOK) */

#ifndef wxUSE_LISTBOX
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_LISTBOX must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_LISTBOX 0
#   endif
#endif /* !defined(wxUSE_LISTBOX) */

#ifndef wxUSE_LISTCTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_LISTCTRL must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_LISTCTRL 0
#   endif
#endif /* !defined(wxUSE_LISTCTRL) */

#ifndef wxUSE_LOGGUI
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_LOGGUI must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_LOGGUI 0
#   endif
#endif /* !defined(wxUSE_LOGGUI) */

#ifndef wxUSE_LOGWINDOW
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_LOGWINDOW must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_LOGWINDOW 0
#   endif
#endif /* !defined(wxUSE_LOGWINDOW) */

#ifndef wxUSE_LOG_DIALOG
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_LOG_DIALOG must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_LOG_DIALOG 0
#   endif
#endif /* !defined(wxUSE_LOG_DIALOG) */

#ifndef wxUSE_MARKUP
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_MARKUP must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_MARKUP 0
#   endif
#endif /* !defined(wxUSE_MARKUP) */

#ifndef wxUSE_MDI
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_MDI must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_MDI 0
#   endif
#endif /* !defined(wxUSE_MDI) */

#ifndef wxUSE_MDI_ARCHITECTURE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_MDI_ARCHITECTURE must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_MDI_ARCHITECTURE 0
#   endif
#endif /* !defined(wxUSE_MDI_ARCHITECTURE) */

#ifndef wxUSE_MENUS
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_MENUS must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_MENUS 0
#   endif
#endif /* !defined(wxUSE_MENUS) */

#ifndef wxUSE_MSGDLG
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_MSGDLG must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_MSGDLG 0
#   endif
#endif /* !defined(wxUSE_MSGDLG) */

#ifndef wxUSE_NOTEBOOK
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_NOTEBOOK must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_NOTEBOOK 0
#   endif
#endif /* !defined(wxUSE_NOTEBOOK) */

#ifndef wxUSE_NOTIFICATION_MESSAGE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_NOTIFICATION_MESSAGE must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_NOTIFICATION_MESSAGE 0
#   endif
#endif /* !defined(wxUSE_NOTIFICATION_MESSAGE) */

#ifndef wxUSE_ODCOMBOBOX
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_ODCOMBOBOX must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_ODCOMBOBOX 0
#   endif
#endif /* !defined(wxUSE_ODCOMBOBOX) */

#ifndef wxUSE_PALETTE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_PALETTE must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_PALETTE 0
#   endif
#endif /* !defined(wxUSE_PALETTE) */

#ifndef wxUSE_POPUPWIN
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_POPUPWIN must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_POPUPWIN 0
#   endif
#endif /* !defined(wxUSE_POPUPWIN) */

#ifndef wxUSE_PREFERENCES_EDITOR
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_PREFERENCES_EDITOR must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_PREFERENCES_EDITOR 0
#   endif
#endif /* !defined(wxUSE_PREFERENCES_EDITOR) */

#ifndef wxUSE_PRINTING_ARCHITECTURE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_PRINTING_ARCHITECTURE must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_PRINTING_ARCHITECTURE 0
#   endif
#endif /* !defined(wxUSE_PRINTING_ARCHITECTURE) */

#ifndef wxUSE_RADIOBOX
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_RADIOBOX must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_RADIOBOX 0
#   endif
#endif /* !defined(wxUSE_RADIOBOX) */

#ifndef wxUSE_RADIOBTN
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_RADIOBTN must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_RADIOBTN 0
#   endif
#endif /* !defined(wxUSE_RADIOBTN) */

#ifndef wxUSE_REARRANGECTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_REARRANGECTRL must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_REARRANGECTRL 0
#   endif
#endif /* !defined(wxUSE_REARRANGECTRL) */

#ifndef wxUSE_RIBBON
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_RIBBON must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_RIBBON 0
#   endif
#endif /* !defined(wxUSE_RIBBON) */

#ifndef wxUSE_RICHMSGDLG
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_RICHMSGDLG must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_RICHMSGDLG 0
#   endif
#endif /* !defined(wxUSE_RICHMSGDLG) */

#ifndef wxUSE_RICHTOOLTIP
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_RICHTOOLTIP must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_RICHTOOLTIP 0
#   endif
#endif /* !defined(wxUSE_RICHTOOLTIP) */

#ifndef wxUSE_SASH
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_SASH must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_SASH 0
#   endif
#endif /* !defined(wxUSE_SASH) */

#ifndef wxUSE_SCROLLBAR
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_SCROLLBAR must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_SCROLLBAR 0
#   endif
#endif /* !defined(wxUSE_SCROLLBAR) */

#ifndef wxUSE_SLIDER
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_SLIDER must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_SLIDER 0
#   endif
#endif /* !defined(wxUSE_SLIDER) */

#ifndef wxUSE_SOUND
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_SOUND must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_SOUND 0
#   endif
#endif /* !defined(wxUSE_SOUND) */

#ifndef wxUSE_SPINBTN
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_SPINBTN must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_SPINBTN 0
#   endif
#endif /* !defined(wxUSE_SPINBTN) */

#ifndef wxUSE_SPINCTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_SPINCTRL must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_SPINCTRL 0
#   endif
#endif /* !defined(wxUSE_SPINCTRL) */

#ifndef wxUSE_SPLASH
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_SPLASH must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_SPLASH 0
#   endif
#endif /* !defined(wxUSE_SPLASH) */

#ifndef wxUSE_SPLITTER
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_SPLITTER must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_SPLITTER 0
#   endif
#endif /* !defined(wxUSE_SPLITTER) */

#ifndef wxUSE_STATBMP
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_STATBMP must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_STATBMP 0
#   endif
#endif /* !defined(wxUSE_STATBMP) */

#ifndef wxUSE_STATBOX
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_STATBOX must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_STATBOX 0
#   endif
#endif /* !defined(wxUSE_STATBOX) */

#ifndef wxUSE_STATLINE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_STATLINE must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_STATLINE 0
#   endif
#endif /* !defined(wxUSE_STATLINE) */

#ifndef wxUSE_STATTEXT
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_STATTEXT must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_STATTEXT 0
#   endif
#endif /* !defined(wxUSE_STATTEXT) */

#ifndef wxUSE_STATUSBAR
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_STATUSBAR must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_STATUSBAR 0
#   endif
#endif /* !defined(wxUSE_STATUSBAR) */

#ifndef wxUSE_TASKBARICON
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TASKBARICON must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_TASKBARICON 0
#   endif
#endif /* !defined(wxUSE_TASKBARICON) */

#ifndef wxUSE_TEXTCTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TEXTCTRL must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_TEXTCTRL 0
#   endif
#endif /* !defined(wxUSE_TEXTCTRL) */

#ifndef wxUSE_TIMEPICKCTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TIMEPICKCTRL must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_TIMEPICKCTRL 0
#   endif
#endif /* !defined(wxUSE_TIMEPICKCTRL) */

#ifndef wxUSE_TIPWINDOW
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TIPWINDOW must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_TIPWINDOW 0
#   endif
#endif /* !defined(wxUSE_TIPWINDOW) */

#ifndef wxUSE_TOOLBAR
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TOOLBAR must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_TOOLBAR 0
#   endif
#endif /* !defined(wxUSE_TOOLBAR) */

#ifndef wxUSE_TOOLTIPS
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TOOLTIPS must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_TOOLTIPS 0
#   endif
#endif /* !defined(wxUSE_TOOLTIPS) */

#ifndef wxUSE_TREECTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TREECTRL must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_TREECTRL 0
#   endif
#endif /* !defined(wxUSE_TREECTRL) */

#ifndef wxUSE_TREELISTCTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TREELISTCTRL must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_TREELISTCTRL 0
#   endif
#endif /* !defined(wxUSE_TREELISTCTRL) */

#ifndef wxUSE_UIACTIONSIMULATOR
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_UIACTIONSIMULATOR must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_UIACTIONSIMULATOR 0
#   endif
#endif /* !defined(wxUSE_UIACTIONSIMULATOR) */

#ifndef wxUSE_VALIDATORS
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_VALIDATORS must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_VALIDATORS 0
#   endif
#endif /* !defined(wxUSE_VALIDATORS) */

#ifndef wxUSE_WEBVIEW
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_WEBVIEW must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_WEBVIEW 0
#   endif
#endif /* !defined(wxUSE_WEBVIEW) */

#ifndef wxUSE_WXHTML_HELP
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_WXHTML_HELP must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_WXHTML_HELP 0
#   endif
#endif /* !defined(wxUSE_WXHTML_HELP) */

#ifndef wxUSE_XRC
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_XRC must be defined, please read comment near the top of this file."
#   else
#       define wxUSE_XRC 0
#   endif
#endif /* !defined(wxUSE_XRC) */

#endif /* wxUSE_GUI */

/*
   Section 2: platform-specific checks.

   This must be done after checking that everything is defined as the platform
   checks use wxUSE_XXX symbols in #if tests.
 */

#if defined(__WXWINCE__)
#  include "wx/msw/wince/chkconf.h"
#elif defined(__WINDOWS__)
#  include "wx/msw/chkconf.h"
#  if defined(__WXGTK__)
#      include "wx/gtk/chkconf.h"
#  endif
#elif defined(__WXGTK__)
#  include "wx/gtk/chkconf.h"
#elif defined(__WXCOCOA__)
#  include "wx/cocoa/chkconf.h"
#elif defined(__WXMAC__)
#  include "wx/osx/chkconf.h"
#elif defined(__OS2__)
#  include "wx/os2/chkconf.h"
#elif defined(__WXDFB__)
#  include "wx/dfb/chkconf.h"
#elif defined(__WXMOTIF__)
#  include "wx/motif/chkconf.h"
#elif defined(__WXX11__)
#  include "wx/x11/chkconf.h"
#elif defined(__WXANDROID__)
#  include "wx/android/chkconf.h"
#endif

/*
    __UNIX__ is also defined under Cygwin but we shouldn't perform these checks
    there if we're building Windows ports.
 */
#if defined(__UNIX__) && !defined(__WINDOWS__)
#   include "wx/unix/chkconf.h"
#endif

#ifdef __WXUNIVERSAL__
#   include "wx/univ/chkconf.h"
#endif

/*
   Section 3a: check consistency of the non-GUI settings.
 */

#if WXWIN_COMPATIBILITY_2_6
#   if !WXWIN_COMPATIBILITY_2_8
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "2.6.X compatibility requires 2.8.X compatibility"
#       else
#           undef WXWIN_COMPATIBILITY_2_8
#           define WXWIN_COMPATIBILITY_2_8 1
#       endif
#   endif
#endif /* WXWIN_COMPATIBILITY_2_6 */

#if wxUSE_ARCHIVE_STREAMS
#   if !wxUSE_DATETIME
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxArchive requires wxUSE_DATETIME"
#       else
#           undef wxUSE_ARCHIVE_STREAMS
#           define wxUSE_ARCHIVE_STREAMS 0
#       endif
#   endif
#endif /* wxUSE_ARCHIVE_STREAMS */

#if wxUSE_PROTOCOL_FILE || wxUSE_PROTOCOL_FTP || wxUSE_PROTOCOL_HTTP
#   if !wxUSE_PROTOCOL
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_PROTOCOL_XXX requires wxUSE_PROTOCOL"
#        else
#            undef wxUSE_PROTOCOL
#            define wxUSE_PROTOCOL 1
#        endif
#   endif
#endif /* wxUSE_PROTOCOL_XXX */

#if wxUSE_URL
#   if !wxUSE_PROTOCOL
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_URL requires wxUSE_PROTOCOL"
#        else
#            undef wxUSE_PROTOCOL
#            define wxUSE_PROTOCOL 1
#        endif
#   endif
#endif /* wxUSE_URL */

#if wxUSE_PROTOCOL
#   if !wxUSE_SOCKETS
#       if wxUSE_PROTOCOL_HTTP || wxUSE_PROTOCOL_FTP
#           ifdef wxABORT_ON_CONFIG_ERROR
#               error "wxUSE_PROTOCOL_FTP/HTTP requires wxUSE_SOCKETS"
#           else
#               undef wxUSE_SOCKETS
#               define wxUSE_SOCKETS 1
#           endif
#       endif
#   endif

#   if !wxUSE_STREAMS
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxUSE_PROTOCOL requires wxUSE_STREAMS"
#       else
#           undef wxUSE_STREAMS
#           define wxUSE_STREAMS 1
#       endif
#   endif
#endif /* wxUSE_PROTOCOL */

/* have to test for wxUSE_HTML before wxUSE_FILESYSTEM */
#if wxUSE_HTML
#   if !wxUSE_FILESYSTEM
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxHTML requires wxFileSystem"
#       else
#           undef wxUSE_FILESYSTEM
#           define wxUSE_FILESYSTEM 1
#       endif
#   endif
#endif /* wxUSE_HTML */

#if wxUSE_FS_ARCHIVE
#   if !wxUSE_FILESYSTEM
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxArchiveFSHandler requires wxFileSystem"
#       else
#           undef wxUSE_FILESYSTEM
#           define wxUSE_FILESYSTEM 1
#       endif
#   endif
#   if !wxUSE_ARCHIVE_STREAMS
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxArchiveFSHandler requires wxArchive"
#       else
#           undef wxUSE_ARCHIVE_STREAMS
#           define wxUSE_ARCHIVE_STREAMS 1
#       endif
#   endif
#endif /* wxUSE_FS_ARCHIVE */

#if wxUSE_FILESYSTEM
#   if !wxUSE_STREAMS
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxUSE_FILESYSTEM requires wxUSE_STREAMS"
#       else
#           undef wxUSE_STREAMS
#           define wxUSE_STREAMS 1
#       endif
#   endif
#   if !wxUSE_FILE && !wxUSE_FFILE
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxUSE_FILESYSTEM requires either wxUSE_FILE or wxUSE_FFILE"
#       else
#           undef wxUSE_FILE
#           define wxUSE_FILE 1
#           undef wxUSE_FFILE
#           define wxUSE_FFILE 1
#       endif
#   endif
#endif /* wxUSE_FILESYSTEM */

#if wxUSE_FS_INET
#   if !wxUSE_PROTOCOL
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxUSE_FS_INET requires wxUSE_PROTOCOL"
#       else
#           undef wxUSE_PROTOCOL
#           define wxUSE_PROTOCOL 1
#       endif
#   endif
#endif /* wxUSE_FS_INET */

#if wxUSE_STOPWATCH || wxUSE_DATETIME
#    if !wxUSE_LONGLONG
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_STOPWATCH and wxUSE_DATETIME require wxUSE_LONGLONG"
#        else
#            undef wxUSE_LONGLONG
#            define wxUSE_LONGLONG 1
#        endif
#    endif
#endif /* wxUSE_STOPWATCH */

#if wxUSE_MIMETYPE && !wxUSE_TEXTFILE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_MIMETYPE requires wxUSE_TEXTFILE"
#   else
#       undef wxUSE_TEXTFILE
#       define wxUSE_TEXTFILE 1
#   endif
#endif /* wxUSE_MIMETYPE */

#if wxUSE_TEXTFILE && !wxUSE_TEXTBUFFER
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TEXTFILE requires wxUSE_TEXTBUFFER"
#   else
#       undef wxUSE_TEXTBUFFER
#       define wxUSE_TEXTBUFFER 1
#   endif
#endif /* wxUSE_TEXTFILE */

#if wxUSE_TEXTFILE && !wxUSE_FILE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TEXTFILE requires wxUSE_FILE"
#   else
#       undef wxUSE_FILE
#       define wxUSE_FILE 1
#   endif
#endif /* wxUSE_TEXTFILE */

#if !wxUSE_DYNLIB_CLASS
#   if wxUSE_DYNAMIC_LOADER
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxUSE_DYNAMIC_LOADER requires wxUSE_DYNLIB_CLASS."
#       else
#           define wxUSE_DYNLIB_CLASS 1
#       endif
#   endif
#endif  /* wxUSE_DYNLIB_CLASS */

#if wxUSE_ZIPSTREAM
#   if !wxUSE_ZLIB
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxZip requires wxZlib"
#       else
#           undef wxUSE_ZLIB
#           define wxUSE_ZLIB 1
#       endif
#   endif
#   if !wxUSE_ARCHIVE_STREAMS
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxZip requires wxArchive"
#       else
#           undef wxUSE_ARCHIVE_STREAMS
#           define wxUSE_ARCHIVE_STREAMS 1
#       endif
#   endif
#endif /* wxUSE_ZIPSTREAM */

#if wxUSE_TARSTREAM
#   if !wxUSE_ARCHIVE_STREAMS
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxTar requires wxArchive"
#       else
#           undef wxUSE_ARCHIVE_STREAMS
#           define wxUSE_ARCHIVE_STREAMS 1
#       endif
#   endif
#endif /* wxUSE_TARSTREAM */

/*
   Section 3b: the tests for the GUI settings only.
 */
#if wxUSE_GUI

#if wxUSE_ACCESSIBILITY && !defined(__WXMSW__)
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_ACCESSIBILITY is currently only supported under wxMSW"
#   else
#       undef wxUSE_ACCESSIBILITY
#       define wxUSE_ACCESSIBILITY 0
#   endif
#endif /* wxUSE_ACCESSIBILITY */

#if wxUSE_BUTTON || \
    wxUSE_CALENDARCTRL || \
    wxUSE_CARET || \
    wxUSE_COMBOBOX || \
    wxUSE_BMPBUTTON || \
    wxUSE_CHECKBOX || \
    wxUSE_CHECKLISTBOX || \
    wxUSE_CHOICE || \
    wxUSE_GAUGE || \
    wxUSE_GRID || \
    wxUSE_HEADERCTRL || \
    wxUSE_LISTBOX || \
    wxUSE_LISTCTRL || \
    wxUSE_NOTEBOOK || \
    wxUSE_RADIOBOX || \
    wxUSE_RADIOBTN || \
    wxUSE_REARRANGECTRL || \
    wxUSE_SCROLLBAR || \
    wxUSE_SLIDER || \
    wxUSE_SPINBTN || \
    wxUSE_SPINCTRL || \
    wxUSE_STATBMP || \
    wxUSE_STATBOX || \
    wxUSE_STATLINE || \
    wxUSE_STATTEXT || \
    wxUSE_STATUSBAR || \
    wxUSE_TEXTCTRL || \
    wxUSE_TOOLBAR || \
    wxUSE_TREECTRL || \
    wxUSE_TREELISTCTRL
#    if !wxUSE_CONTROLS
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_CONTROLS unset but some controls used"
#        else
#            undef wxUSE_CONTROLS
#            define wxUSE_CONTROLS 1
#        endif
#    endif
#endif /* controls */

#if wxUSE_BMPBUTTON
#    if !wxUSE_BUTTON
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_BMPBUTTON requires wxUSE_BUTTON"
#        else
#            undef wxUSE_BUTTON
#            define wxUSE_BUTTON 1
#        endif
#    endif
#endif /* wxUSE_BMPBUTTON */

#if wxUSE_COMMANDLINKBUTTON
#    if !wxUSE_BUTTON
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_COMMANDLINKBUTTON requires wxUSE_BUTTON"
#        else
#            undef wxUSE_BUTTON
#            define wxUSE_BUTTON 1
#        endif
#    endif
#endif /* wxUSE_COMMANDLINKBUTTON */

/*
   wxUSE_BOOKCTRL should be only used if any of the controls deriving from it
   are used
 */
#ifdef wxUSE_BOOKCTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_BOOKCTRL is defined automatically, don't define it"
#   else
#       undef wxUSE_BOOKCTRL
#   endif
#endif

#define wxUSE_BOOKCTRL (wxUSE_NOTEBOOK || \
                        wxUSE_LISTBOOK || \
                        wxUSE_CHOICEBOOK || \
                        wxUSE_TOOLBOOK || \
                        wxUSE_TREEBOOK)

#if wxUSE_COLLPANE
#   if !wxUSE_BUTTON || !wxUSE_STATLINE
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxUSE_COLLPANE requires wxUSE_BUTTON and wxUSE_STATLINE"
#       else
#           undef wxUSE_COLLPANE
#           define wxUSE_COLLPANE 0
#       endif
#   endif
#endif /* wxUSE_COLLPANE */

#if wxUSE_LISTBOOK
#   if !wxUSE_LISTCTRL
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxListbook requires wxListCtrl"
#       else
#           undef wxUSE_LISTCTRL
#           define wxUSE_LISTCTRL 1
#       endif
#   endif
#endif /* wxUSE_LISTBOOK */

#if wxUSE_CHOICEBOOK
#   if !wxUSE_CHOICE
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxChoicebook requires wxChoice"
#       else
#           undef wxUSE_CHOICE
#           define wxUSE_CHOICE 1
#       endif
#   endif
#endif /* wxUSE_CHOICEBOOK */

#if wxUSE_TOOLBOOK
#   if !wxUSE_TOOLBAR
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxToolbook requires wxToolBar"
#       else
#           undef wxUSE_TOOLBAR
#           define wxUSE_TOOLBAR 1
#       endif
#   endif
#endif /* wxUSE_TOOLBOOK */

#if !wxUSE_ODCOMBOBOX
#   if wxUSE_BITMAPCOMBOBOX
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxBitmapComboBox requires wxOwnerDrawnComboBox"
#       else
#           undef wxUSE_BITMAPCOMBOBOX
#           define wxUSE_BITMAPCOMBOBOX 0
#       endif
#   endif
#endif /* !wxUSE_ODCOMBOBOX */

#if !wxUSE_HEADERCTRL
#   if wxUSE_DATAVIEWCTRL || wxUSE_GRID
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxDataViewCtrl and wxGrid require wxHeaderCtrl"
#       else
#           undef wxUSE_HEADERCTRL
#           define wxUSE_HEADERCTRL 1
#       endif
#   endif
#endif /* !wxUSE_HEADERCTRL */

#if wxUSE_REARRANGECTRL
#   if !wxUSE_CHECKLISTBOX
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxRearrangeCtrl requires wxCheckListBox"
#       else
#           undef wxUSE_REARRANGECTRL
#           define wxUSE_REARRANGECTRL 0
#       endif
#   endif
#endif /* wxUSE_REARRANGECTRL */

#if wxUSE_RICHMSGDLG
#    if !wxUSE_MSGDLG
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_RICHMSGDLG requires wxUSE_MSGDLG"
#        else
#            undef wxUSE_MSGDLG
#            define wxUSE_MSGDLG 1
#        endif
#    endif
#endif /* wxUSE_RICHMSGDLG */

/* don't attempt to use native status bar on the platforms not having it */
#ifndef wxUSE_NATIVE_STATUSBAR
#   define wxUSE_NATIVE_STATUSBAR 0
#elif wxUSE_NATIVE_STATUSBAR
#   if defined(__WXUNIVERSAL__) || !(defined(__WXMSW__) || defined(__WXMAC__))
#       undef wxUSE_NATIVE_STATUSBAR
#       define wxUSE_NATIVE_STATUSBAR 0
#   endif
#endif

#if wxUSE_GRAPHICS_CONTEXT && !wxUSE_GEOMETRY
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_GRAPHICS_CONTEXT requires wxUSE_GEOMETRY"
#   else
#       undef wxUSE_GRAPHICS_CONTEXT
#       define wxUSE_GRAPHICS_CONTEXT 0
#   endif
#endif /* wxUSE_GRAPHICS_CONTEXT */


/* generic controls dependencies */
#if !defined(__WXMSW__) || defined(__WXUNIVERSAL__)
#   if wxUSE_FONTDLG || wxUSE_FILEDLG || wxUSE_CHOICEDLG
        /* all common controls are needed by these dialogs */
#       if !defined(wxUSE_CHOICE) || \
           !defined(wxUSE_TEXTCTRL) || \
           !defined(wxUSE_BUTTON) || \
           !defined(wxUSE_CHECKBOX) || \
           !defined(wxUSE_STATTEXT)
#           ifdef wxABORT_ON_CONFIG_ERROR
#               error "These common controls are needed by common dialogs"
#           else
#               undef wxUSE_CHOICE
#               define wxUSE_CHOICE 1
#               undef wxUSE_TEXTCTRL
#               define wxUSE_TEXTCTRL 1
#               undef wxUSE_BUTTON
#               define wxUSE_BUTTON 1
#               undef wxUSE_CHECKBOX
#               define wxUSE_CHECKBOX 1
#               undef wxUSE_STATTEXT
#               define wxUSE_STATTEXT 1
#           endif
#       endif
#   endif
#endif /* !wxMSW || wxUniv */

/* generic file dialog depends on (generic) file control */
#if wxUSE_FILEDLG && !wxUSE_FILECTRL && \
        (defined(__WXUNIVERSAL__) || defined(__WXGTK__))
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "Generic wxFileDialog requires wxFileCtrl"
#   else
#       undef wxUSE_FILECTRL
#       define wxUSE_FILECTRL 1
#   endif
#endif /* wxUSE_FILEDLG */

/* common dependencies */
#if wxUSE_ARTPROVIDER_TANGO
#   if !(wxUSE_STREAMS && wxUSE_IMAGE && wxUSE_LIBPNG)
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "Tango art provider requires wxImage with streams and PNG support"
#       else
#           undef wxUSE_ARTPROVIDER_TANGO
#           define wxUSE_ARTPROVIDER_TANGO 0
#       endif
#   endif
#endif /* wxUSE_ARTPROVIDER_TANGO */

#if wxUSE_CALENDARCTRL
#   if !(wxUSE_SPINBTN && wxUSE_COMBOBOX)
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxCalendarCtrl requires wxSpinButton and wxComboBox"
#       else
#           undef wxUSE_SPINBTN
#           undef wxUSE_COMBOBOX
#           define wxUSE_SPINBTN 1
#           define wxUSE_COMBOBOX 1
#       endif
#   endif

#   if !wxUSE_DATETIME
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxCalendarCtrl requires wxUSE_DATETIME"
#       else
#           undef wxUSE_DATETIME
#           define wxUSE_DATETIME 1
#       endif
#   endif
#endif /* wxUSE_CALENDARCTRL */

#if wxUSE_DATEPICKCTRL || wxUSE_TIMEPICKCTRL
#   if !wxUSE_DATETIME
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxDatePickerCtrl and wxTimePickerCtrl requires wxUSE_DATETIME"
#       else
#           undef wxUSE_DATETIME
#           define wxUSE_DATETIME 1
#       endif
#   endif
#endif /* wxUSE_DATEPICKCTRL || wxUSE_TIMEPICKCTRL */

#if wxUSE_CHECKLISTBOX
#   if !wxUSE_LISTBOX
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxCheckListBox requires wxListBox"
#        else
#            undef wxUSE_LISTBOX
#            define wxUSE_LISTBOX 1
#        endif
#   endif
#endif /* wxUSE_CHECKLISTBOX */

#if wxUSE_CHOICEDLG
#   if !wxUSE_LISTBOX
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "Choice dialogs requires wxListBox"
#        else
#            undef wxUSE_LISTBOX
#            define wxUSE_LISTBOX 1
#        endif
#   endif
#endif /* wxUSE_CHOICEDLG */

#if wxUSE_FILECTRL
#   if !wxUSE_DATETIME
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxFileCtrl requires wxDateTime"
#       else
#           undef wxUSE_DATETIME
#           define wxUSE_DATETIME 1
#       endif
#   endif
#endif /* wxUSE_FILECTRL */

#if wxUSE_HELP
#   if !wxUSE_BMPBUTTON
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxUSE_HELP requires wxUSE_BMPBUTTON"
#       else
#           undef wxUSE_BMPBUTTON
#           define wxUSE_BMPBUTTON 1
#       endif
#   endif

#   if !wxUSE_CHOICEDLG
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxUSE_HELP requires wxUSE_CHOICEDLG"
#       else
#           undef wxUSE_CHOICEDLG
#           define wxUSE_CHOICEDLG 1
#       endif
#   endif
#endif /* wxUSE_HELP */

#if wxUSE_MS_HTML_HELP
    /*
        this doesn't make sense for platforms other than MSW but we still
        define it in wx/setup_inc.h so don't complain if it happens to be
        defined under another platform but just silently fix it.
     */
#   ifndef __WXMSW__
#       undef wxUSE_MS_HTML_HELP
#       define wxUSE_MS_HTML_HELP 0
#   endif
#endif /* wxUSE_MS_HTML_HELP */

#if wxUSE_WXHTML_HELP
#   if !wxUSE_HELP || !wxUSE_HTML || !wxUSE_COMBOBOX || !wxUSE_NOTEBOOK || !wxUSE_SPINCTRL
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "Built in help controller can't be compiled"
#       else
#           undef wxUSE_HELP
#           define wxUSE_HELP 1
#           undef wxUSE_HTML
#           define wxUSE_HTML 1
#           undef wxUSE_COMBOBOX
#           define wxUSE_COMBOBOX 1
#           undef wxUSE_NOTEBOOK
#           define wxUSE_NOTEBOOK 1
#           undef wxUSE_SPINCTRL
#           define wxUSE_SPINCTRL 1
#       endif
#   endif
#endif /* wxUSE_WXHTML_HELP */

#if !wxUSE_IMAGE
/*
   The default wxUSE_IMAGE setting is 1, so if it's set to 0 we assume the
   user explicitly wants this and disable all other features that require
   wxUSE_IMAGE.
 */
#   if wxUSE_DRAGIMAGE
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_DRAGIMAGE requires wxUSE_IMAGE"
#        else
#            undef wxUSE_DRAGIMAGE
#            define wxUSE_DRAGIMAGE 0
#        endif
#   endif

#   if wxUSE_LIBPNG
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_LIBPNG requires wxUSE_IMAGE"
#        else
#            undef wxUSE_LIBPNG
#            define wxUSE_LIBPNG 0
#        endif
#   endif

#   if wxUSE_LIBJPEG
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_LIBJPEG requires wxUSE_IMAGE"
#        else
#            undef wxUSE_LIBJPEG
#            define wxUSE_LIBJPEG 0
#        endif
#   endif

#   if wxUSE_LIBTIFF
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_LIBTIFF requires wxUSE_IMAGE"
#        else
#            undef wxUSE_LIBTIFF
#            define wxUSE_LIBTIFF 0
#        endif
#   endif

#   if wxUSE_GIF
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_GIF requires wxUSE_IMAGE"
#        else
#            undef wxUSE_GIF
#            define wxUSE_GIF 0
#        endif
#   endif

#   if wxUSE_PNM
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_PNM requires wxUSE_IMAGE"
#        else
#            undef wxUSE_PNM
#            define wxUSE_PNM 0
#        endif
#   endif

#   if wxUSE_PCX
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_PCX requires wxUSE_IMAGE"
#        else
#            undef wxUSE_PCX
#            define wxUSE_PCX 0
#        endif
#   endif

#   if wxUSE_IFF
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_IFF requires wxUSE_IMAGE"
#        else
#            undef wxUSE_IFF
#            define wxUSE_IFF 0
#        endif
#   endif

#   if wxUSE_TOOLBAR
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_TOOLBAR requires wxUSE_IMAGE"
#        else
#            undef wxUSE_TOOLBAR
#            define wxUSE_TOOLBAR 0
#        endif
#   endif

#   if wxUSE_XPM
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_XPM requires wxUSE_IMAGE"
#        else
#            undef wxUSE_XPM
#            define wxUSE_XPM 0
#        endif
#   endif

#endif /* !wxUSE_IMAGE */

#if wxUSE_DOC_VIEW_ARCHITECTURE
#   if !wxUSE_MENUS
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "DocView requires wxUSE_MENUS"
#        else
#            undef wxUSE_MENUS
#            define wxUSE_MENUS 1
#        endif
#   endif

#   if !wxUSE_CHOICEDLG
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "DocView requires wxUSE_CHOICEDLG"
#        else
#            undef wxUSE_CHOICEDLG
#            define wxUSE_CHOICEDLG 1
#        endif
#   endif

#   if !wxUSE_STREAMS && !wxUSE_STD_IOSTREAM
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "DocView requires wxUSE_STREAMS or wxUSE_STD_IOSTREAM"
#        else
#            undef wxUSE_STREAMS
#            define wxUSE_STREAMS 1
#        endif
#   endif

#   if !wxUSE_FILE_HISTORY
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "DocView requires wxUSE_FILE_HISTORY"
#        else
#            undef wxUSE_FILE_HISTORY
#            define wxUSE_FILE_HISTORY 1
#        endif
#   endif
#endif /* wxUSE_DOC_VIEW_ARCHITECTURE */

#if wxUSE_PRINTING_ARCHITECTURE
#   if !wxUSE_COMBOBOX
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "Print dialog requires wxUSE_COMBOBOX"
#       else
#           undef wxUSE_COMBOBOX
#           define wxUSE_COMBOBOX 1
#       endif
#   endif
#endif /* wxUSE_PRINTING_ARCHITECTURE */

#if wxUSE_MDI_ARCHITECTURE
#   if !wxUSE_MDI
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "MDI requires wxUSE_MDI"
#        else
#            undef wxUSE_MDI
#            define wxUSE_MDI 1
#        endif
#   endif

#   if !wxUSE_DOC_VIEW_ARCHITECTURE
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_MDI_ARCHITECTURE requires wxUSE_DOC_VIEW_ARCHITECTURE"
#        else
#            undef wxUSE_DOC_VIEW_ARCHITECTURE
#            define wxUSE_DOC_VIEW_ARCHITECTURE 1
#        endif
#   endif
#endif /* wxUSE_MDI_ARCHITECTURE */

#if !wxUSE_FILEDLG
#   if wxUSE_DOC_VIEW_ARCHITECTURE || wxUSE_WXHTML_HELP
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxUSE_FILEDLG is required by wxUSE_DOC_VIEW_ARCHITECTURE and wxUSE_WXHTML_HELP!"
#       else
#           undef wxUSE_FILEDLG
#           define wxUSE_FILEDLG 1
#       endif
#   endif
#endif /* wxUSE_FILEDLG */

#if !wxUSE_GAUGE || !wxUSE_BUTTON
#   if wxUSE_PROGRESSDLG
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "Generic progress dialog requires wxUSE_GAUGE and wxUSE_BUTTON"
#       else
#           undef wxUSE_GAUGE
#           undef wxUSE_BUTTON
#           define wxUSE_GAUGE 1
#           define wxUSE_BUTTON 1
#       endif
#   endif
#endif /* !wxUSE_GAUGE */

#if !wxUSE_BUTTON
#   if wxUSE_FONTDLG || \
       wxUSE_FILEDLG || \
       wxUSE_CHOICEDLG || \
       wxUSE_NUMBERDLG || \
       wxUSE_TEXTDLG || \
       wxUSE_DIRDLG || \
       wxUSE_STARTUP_TIPS || \
       wxUSE_WIZARDDLG
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "Common and generic dialogs require wxUSE_BUTTON"
#       else
#           undef wxUSE_BUTTON
#           define wxUSE_BUTTON 1
#       endif
#   endif
#endif /* !wxUSE_BUTTON */

#if !wxUSE_TOOLBAR
#   if wxUSE_TOOLBAR_NATIVE
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_TOOLBAR is set to 0 but wxUSE_TOOLBAR_NATIVE is set to 1"
#        else
#            undef wxUSE_TOOLBAR_NATIVE
#            define wxUSE_TOOLBAR_NATIVE 0
#        endif
#   endif
#endif

#if !wxUSE_IMAGLIST
#   if wxUSE_TREECTRL || wxUSE_NOTEBOOK || wxUSE_LISTCTRL || wxUSE_TREELISTCTRL
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxImageList must be compiled as well"
#       else
#           undef wxUSE_IMAGLIST
#           define wxUSE_IMAGLIST 1
#       endif
#   endif
#endif /* !wxUSE_IMAGLIST */

#if wxUSE_RADIOBOX
#   if !wxUSE_RADIOBTN
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_RADIOBOX requires wxUSE_RADIOBTN"
#        else
#            undef wxUSE_RADIOBTN
#            define wxUSE_RADIOBTN 1
#        endif
#   endif
#   if !wxUSE_STATBOX
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_RADIOBOX requires wxUSE_STATBOX"
#        else
#            undef wxUSE_STATBOX
#            define wxUSE_STATBOX 1
#        endif
#   endif
#endif /* wxUSE_RADIOBOX */

#if wxUSE_LOGWINDOW
#    if !wxUSE_TEXTCTRL
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_LOGWINDOW requires wxUSE_TEXTCTRL"
#        else
#            undef wxUSE_TEXTCTRL
#            define wxUSE_TEXTCTRL 1
#        endif
#    endif
#endif /* wxUSE_LOGWINDOW */

#if wxUSE_LOG_DIALOG
#    if !wxUSE_LISTCTRL || !wxUSE_BUTTON
#        ifdef wxABORT_ON_CONFIG_ERROR
#            error "wxUSE_LOG_DIALOG requires wxUSE_LISTCTRL and wxUSE_BUTTON"
#        else
#            undef wxUSE_LISTCTRL
#            define wxUSE_LISTCTRL 1
#            undef wxUSE_BUTTON
#            define wxUSE_BUTTON 1
#        endif
#    endif
#endif /* wxUSE_LOG_DIALOG */

#if wxUSE_CLIPBOARD && !wxUSE_DATAOBJ
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxClipboard requires wxDataObject"
#   else
#       undef wxUSE_DATAOBJ
#       define wxUSE_DATAOBJ 1
#   endif
#endif /* wxUSE_CLIPBOARD */

#if wxUSE_XRC && !wxUSE_XML
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_XRC requires wxUSE_XML"
#   else
#       undef wxUSE_XRC
#       define wxUSE_XRC 0
#   endif
#endif /* wxUSE_XRC */

#if wxUSE_SOCKETS && !wxUSE_STOPWATCH
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_SOCKETS requires wxUSE_STOPWATCH"
#   else
#       undef wxUSE_SOCKETS
#       define wxUSE_SOCKETS 0
#   endif
#endif /* wxUSE_SOCKETS */

#if wxUSE_SVG && !wxUSE_STREAMS
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_SVG requires wxUSE_STREAMS"
#   else
#       undef wxUSE_SVG
#       define wxUSE_SVG 0
#   endif
#endif /* wxUSE_SVG */

#if wxUSE_SVG && !wxUSE_IMAGE
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_SVG requires wxUSE_IMAGE"
#   else
#       undef wxUSE_SVG
#       define wxUSE_SVG 0
#   endif
#endif /* wxUSE_SVG */

#if wxUSE_SVG && !wxUSE_LIBPNG
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_SVG requires wxUSE_LIBPNG"
#   else
#       undef wxUSE_SVG
#       define wxUSE_SVG 0
#   endif
#endif /* wxUSE_SVG */

#if wxUSE_TASKBARICON && !wxUSE_MENUS
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TASKBARICON requires wxUSE_MENUS"
#   else
#       undef wxUSE_TASKBARICON
#       define wxUSE_TASKBARICON 0
#   endif
#endif /* wxUSE_TASKBARICON */

#if !wxUSE_VARIANT
#   if wxUSE_DATAVIEWCTRL
#       ifdef wxABORT_ON_CONFIG_ERROR
#           error "wxDataViewCtrl requires wxVariant"
#       else
#           undef wxUSE_DATAVIEWCTRL
#           define wxUSE_DATAVIEWCTRL 0
#       endif
#   endif
#endif /* wxUSE_VARIANT */

#if wxUSE_TREELISTCTRL && !wxUSE_DATAVIEWCTRL
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_TREELISTCTRL requires wxDataViewCtrl"
#   else
#       undef wxUSE_TREELISTCTRL
#       define wxUSE_TREELISTCTRL 0
#   endif
#endif /* wxUSE_TREELISTCTRL */

#if wxUSE_WEBVIEW && !(wxUSE_WEBVIEW_WEBKIT || wxUSE_WEBVIEW_IE)
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxUSE_WEBVIEW requires at least one backend"
#   else
#       undef wxUSE_WEBVIEW
#       define wxUSE_WEBVIEW 0
#   endif
#endif /* wxUSE_WEBVIEW && !any web view backend */

#if wxUSE_PREFERENCES_EDITOR
    /*
        We can use either a generic implementation, using wxNotebook, or a
        native one under wxOSX/Cocoa but then we must be using the native
        toolbar.
    */
#   if !wxUSE_NOTEBOOK
#       ifdef __WXOSX_COCOA__
#           if !wxUSE_TOOLBAR || !wxOSX_USE_NATIVE_TOOLBAR
#               ifdef wxABORT_ON_CONFIG_ERROR
#                   error "wxUSE_PREFERENCES_EDITOR requires native toolbar in wxOSX"
#               else
#                   undef wxUSE_PREFERENCES_EDITOR
#                   define wxUSE_PREFERENCES_EDITOR 0
#               endif
#           endif
#       else
#           ifdef wxABORT_ON_CONFIG_ERROR
#               error "wxUSE_PREFERENCES_EDITOR requires wxNotebook"
#           else
#               undef wxUSE_PREFERENCES_EDITOR
#               define wxUSE_PREFERENCES_EDITOR 0
#           endif
#       endif
#   endif
#endif /* wxUSE_PREFERENCES_EDITOR */

#endif /* wxUSE_GUI */

#endif /* _WX_CHKCONF_H_ */
