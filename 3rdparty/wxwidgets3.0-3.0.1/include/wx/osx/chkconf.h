/*
 * Name:        wx/osx/chkconf.h
 * Purpose:     Mac-specific config settings checks
 * Author:      Vadim Zeitlin
 * Modified by:
 * Created:     2005-04-05 (extracted from wx/chkconf.h)
 * Copyright:   (c) 2005 Vadim Zeitlin <vadim@wxwidgets.org>
 * Licence:     wxWindows licence
 */

/* THIS IS A C FILE, DON'T USE C++ FEATURES (IN PARTICULAR COMMENTS) IN IT */

#ifndef _WX_OSX_CHKCONF_H_
#define _WX_OSX_CHKCONF_H_

/*
 * check graphics context option, must be on for every os x platform
 * we only use core graphics now on all builds, try to catch attempts
 * to configure the build otherwise and give error messages
 */

#if wxUSE_GUI && (!wxUSE_GRAPHICS_CONTEXT || \
    ( defined( wxMAC_USE_CORE_GRAPHICS ) && !wxMAC_USE_CORE_GRAPHICS ))
#   error "OS X builds use CoreGraphics in this wx version, you cannot turn back to QuickDraw completely"
#endif

/*
 * using mixins of cocoa functionality
 */

#ifdef __WXOSX_COCOA__
    #define wxOSX_USE_COCOA 1
#else
    #define wxOSX_USE_COCOA 0
#endif

#ifdef __WXOSX_CARBON__
    #define wxOSX_USE_CARBON 1
#else
    #define wxOSX_USE_CARBON 0
#endif

/*
 * setting flags according to the platform
 */

#ifdef __LP64__
    #if wxOSX_USE_COCOA == 0
        #undef wxOSX_USE_COCOA
        #define wxOSX_USE_COCOA 1
    #endif
    #if wxOSX_USE_CARBON
        #error "Carbon does not support 64bit"
    #endif
    #define wxOSX_USE_IPHONE 0
#else
    #ifdef __WXOSX_IPHONE__
        #define wxOSX_USE_IPHONE 1
    #else
        #define wxOSX_USE_IPHONE 0
    #endif
#endif

/*
 * combination flags
 */

#if wxOSX_USE_COCOA || wxOSX_USE_CARBON
    #define wxOSX_USE_COCOA_OR_CARBON 1
#else
    #define wxOSX_USE_COCOA_OR_CARBON 0
#endif

#if wxOSX_USE_COCOA || wxOSX_USE_IPHONE
    #define wxOSX_USE_COCOA_OR_IPHONE 1
#else
    #define wxOSX_USE_COCOA_OR_IPHONE 0
#endif

#if wxOSX_USE_IPHONE
    #include "wx/osx/iphone/chkconf.h"
#elif wxOSX_USE_CARBON
    #include "wx/osx/carbon/chkconf.h"
#elif wxOSX_USE_COCOA
    #include "wx/osx/cocoa/chkconf.h"
#endif

#endif /* _WX_OSX_CHKCONF_H_ */
