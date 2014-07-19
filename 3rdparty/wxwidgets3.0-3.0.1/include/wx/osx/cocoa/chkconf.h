/*
 * Name:        wx/osx/cocoa/chkconf.h
 * Purpose:     Compiler-specific configuration checking
 * Author:      Stefan Csomor
 * Modified by:
 * Created:     2008-07-30
 * Copyright:   (c) Stefan Csomor
 * Licence:     wxWindows licence
 */

#ifndef _WX_OSX_COCOA_CHKCONF_H_
#define _WX_OSX_COCOA_CHKCONF_H_

/* Many wchar functions (and also strnlen(), for some reason) are only
   available since 10.7 so don't use them if we want to build the applications
   that would run under 10.6 and earlier. */
#if MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_7
#define HAVE_STRNLEN 1
#define HAVE_WCSDUP 1
#define HAVE_WCSNLEN 1
#define HAVE_WCSCASECMP 1
#define HAVE_WCSNCASECMP 1
#endif

/*
 * native (1) or emulated (0) toolbar
 */

#ifndef wxOSX_USE_NATIVE_TOOLBAR
    #define wxOSX_USE_NATIVE_TOOLBAR 1
#endif

/*
 * leave is isFlipped and don't override
 */
#ifndef wxOSX_USE_NATIVE_FLIPPED 
    #define wxOSX_USE_NATIVE_FLIPPED 1
#endif

/*
 * text rendering system
 */

#define wxOSX_USE_ATSU_TEXT 0

/*
 * Audio System
 */

#define wxOSX_USE_QUICKTIME 0
#define wxOSX_USE_AUDIOTOOLBOX 1

/*
 * turning off capabilities that don't work under cocoa yet
 */

#endif
    /* _WX_MAC_CHKCONF_H_ */

