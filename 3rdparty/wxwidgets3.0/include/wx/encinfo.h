///////////////////////////////////////////////////////////////////////////////
// Name:        wx/encinfo.h
// Purpose:     declares wxNativeEncodingInfo struct
// Author:      Vadim Zeitlin
// Modified by:
// Created:     19.09.2003 (extracted from wx/fontenc.h)
// Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_ENCINFO_H_
#define _WX_ENCINFO_H_

#include "wx/string.h"

// ----------------------------------------------------------------------------
// wxNativeEncodingInfo contains all encoding parameters for this platform
// ----------------------------------------------------------------------------

// This private structure specifies all the parameters needed to create a font
// with the given encoding on this platform.
//
// Under X, it contains the last 2 elements of the font specifications
// (registry and encoding).
//
// Under Windows, it contains a number which is one of predefined CHARSET_XXX
// values.
//
// Under all platforms it also contains a facename string which should be
// used, if not empty, to create fonts in this encoding (this is the only way
// to create a font of non-standard encoding (like KOI8) under Windows - the
// facename specifies the encoding then)

struct WXDLLIMPEXP_CORE wxNativeEncodingInfo
{
    wxString facename;          // may be empty meaning "any"
    wxFontEncoding encoding;    // so that we know what this struct represents

#if defined(__WXMSW__) || \
    defined(__WXPM__)  || \
    defined(__WXMAC__) || \
    defined(__WXCOCOA__) // FIXME: __WXCOCOA__

    wxNativeEncodingInfo()
        : facename()
        , encoding(wxFONTENCODING_SYSTEM)
        , charset(0) /* ANSI_CHARSET */
    { }

    int      charset;
#elif defined(_WX_X_FONTLIKE)
    wxString xregistry,
             xencoding;
#elif defined(wxHAS_UTF8_FONTS)
    // ports using UTF-8 for text don't need encoding information for fonts
#else
    #error "Unsupported toolkit"
#endif
    // this struct is saved in config by wxFontMapper, so it should know to
    // serialise itself (implemented in platform-specific code)
    bool FromString(const wxString& s);
    wxString ToString() const;
};

#endif // _WX_ENCINFO_H_

