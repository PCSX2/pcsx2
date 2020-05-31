/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/fontutilcmn.cpp
// Purpose:     Font helper functions common for all ports
// Author:      Vaclav Slavik
// Modified by:
// Created:     2006-12-20
// Copyright:   (c) Vadim Zeitlin, Vaclav Slavik
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/fontutil.h"
#include "wx/encinfo.h"

// ============================================================================
// implementation
// ============================================================================

#ifdef wxHAS_UTF8_FONTS

// ----------------------------------------------------------------------------
// wxNativeEncodingInfo
// ----------------------------------------------------------------------------

bool wxNativeEncodingInfo::FromString(const wxString& WXUNUSED(s))
{
    return false;
}

wxString wxNativeEncodingInfo::ToString() const
{
    return wxEmptyString;
}

bool wxTestFontEncoding(const wxNativeEncodingInfo& WXUNUSED(info))
{
    return true;
}

bool wxGetNativeFontEncoding(wxFontEncoding encoding,
                             wxNativeEncodingInfo *info)
{
    // all encodings are available because we translate text in any encoding to
    // UTF-8 internally anyhow
    info->facename.clear();
    info->encoding = encoding;

    return true;
}

#endif // wxHAS_UTF8_FONTS
