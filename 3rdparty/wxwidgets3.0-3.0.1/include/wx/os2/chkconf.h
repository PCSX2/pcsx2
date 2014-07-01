/*
 * Name:        wx/os2/chkconf.h
 * Purpose:     Compiler-specific configuration checking
 * Author:      Julian Smart
 * Modified by:
 * Created:     01/02/97
 * Copyright:   (c) Julian Smart
 * Licence:     wxWindows licence
 */

/* THIS IS A C FILE, DON'T USE C++ FEATURES (IN PARTICULAR COMMENTS) IN IT */

#ifndef _WX_OS2_CHKCONF_H_
#define _WX_OS2_CHKCONF_H_

/*
   wxDisplay is not implemented for OS/2, use stub common version instead.
 */
#if wxUSE_DISPLAY
#   undef wxUSE_DISPLAY
#   define wxUSE_DISPLAY 0
#endif /* wxUSE_DISPLAY */

/* Watcom builds for OS/2 port are setup.h driven and setup.h is
   automatically generated from include/wx/setup_inc.h so we have
   to disable here features not supported currently or enable
   features required */
#ifdef __WATCOMC__

#if wxUSE_STACKWALKER
#   undef wxUSE_STACKWALKER
#   define wxUSE_STACKWALKER 0
#endif /* wxUSE_STACKWALKER */

#if !wxUSE_POSTSCRIPT
#   undef wxUSE_POSTSCRIPT
#   define wxUSE_POSTSCRIPT 1
#endif

#if wxUSE_MS_HTML_HELP
#   undef wxUSE_MS_HTML_HELP
#   define wxUSE_MS_HTML_HELP 0
#endif

#endif /* __WATCOM__ */

#endif /* _WX_OS2_CHKCONF_H_ */
