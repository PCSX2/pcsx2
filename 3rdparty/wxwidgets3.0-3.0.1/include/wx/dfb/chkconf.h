/*
 * Name:        wx/dfb/chkconf.h
 * Author:      Vaclav Slavik
 * Purpose:     Compiler-specific configuration checking
 * Created:     2006-08-10
 * Copyright:   (c) 2006 REA Elektronik GmbH
 * Licence:     wxWindows licence
 */

/* THIS IS A C FILE, DON'T USE C++ FEATURES (IN PARTICULAR COMMENTS) IN IT */

#ifndef _WX_DFB_CHKCONF_H_
#define _WX_DFB_CHKCONF_H_

#ifndef __WXUNIVERSAL__
#   error "wxDirectFB cannot be built without wxUniversal"
#endif

#if !wxUSE_CONFIG
#   error "wxFileConfig is required by wxDFB port"
#endif

#if wxUSE_SOCKETS && !wxUSE_CONSOLE_EVENTLOOP
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxSocket requires wxSelectDispatcher in wxDFB"
#   else
#       undef wxUSE_CONSOLE_EVENTLOOP
#       define wxUSE_CONSOLE_EVENTLOOP 1
#   endif
#endif

#if wxUSE_DATAOBJ
#   ifdef wxABORT_ON_CONFIG_ERROR
#       error "wxDataObject not supported in wxDFB"
#   else
#       undef wxUSE_DATAOBJ
#       define wxUSE_DATAOBJ 0
#   endif
#endif

#endif /* _WX_DFB_CHKCONF_H_ */
