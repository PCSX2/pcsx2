/////////////////////////////////////////////////////////////////////////////
// Name:        wx/config.h
// Purpose:     wxConfig base header
// Author:      Julian Smart
// Modified by:
// Created:
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_CONFIG_H_BASE_
#define _WX_CONFIG_H_BASE_

#include "wx/confbase.h"

#if wxUSE_CONFIG

// ----------------------------------------------------------------------------
// define the native wxConfigBase implementation
// ----------------------------------------------------------------------------

// under Windows we prefer to use the native implementation but can be forced
// to use the file-based one
#if defined(__WINDOWS__) && wxUSE_CONFIG_NATIVE
    #include "wx/msw/regconf.h"
    #define wxConfig  wxRegConfig
#elif defined(__WXOS2__) && wxUSE_CONFIG_NATIVE
    #include "wx/os2/iniconf.h"
    #define wxConfig wxIniConfig
#else // either we're under Unix or wish to always use config files
    #include "wx/fileconf.h"
    #define wxConfig wxFileConfig
#endif

#endif // wxUSE_CONFIG

#endif // _WX_CONFIG_H_BASE_
