///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/fontutil.cpp
// Purpose:     font-related helper functions for wxMSW
// Author:      Vadim Zeitlin
// Modified by:
// Created:     05.11.99
// Copyright:   (c) 1999 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

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

#ifndef WX_PRECOMP
    #include "wx/string.h"
    #include "wx/log.h"
    #include "wx/intl.h"
    #include "wx/wxcrtvararg.h"
    #include "wx/msw/private.h"
#endif //WX_PRECOMP

#include "wx/encinfo.h"
#include "wx/fontmap.h"
#include "wx/tokenzr.h"

// for MSVC5 and old w32api
#ifndef HANGUL_CHARSET
#    define HANGUL_CHARSET  129
#endif

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxNativeEncodingInfo
// ----------------------------------------------------------------------------

// convert to/from the string representation: format is
//      encodingid;facename[;charset]

bool wxNativeEncodingInfo::FromString(const wxString& s)
{
    wxStringTokenizer tokenizer(s, wxT(";"));

    wxString encid = tokenizer.GetNextToken();

    // we support 2 formats: the old one (and still used if !wxUSE_FONTMAP)
    // used the raw encoding values but the new one uses the encoding names
    long enc;
    if ( encid.ToLong(&enc) )
    {
        // old format, intepret as encoding -- but after minimal checks
        if ( enc < 0 || enc >= wxFONTENCODING_MAX )
            return false;

        encoding = (wxFontEncoding)enc;
    }
    else // not a number, interpret as an encoding name
    {
#if wxUSE_FONTMAP
        encoding = wxFontMapper::GetEncodingFromName(encid);
        if ( encoding == wxFONTENCODING_MAX )
#endif // wxUSE_FONTMAP
        {
            // failed to parse the name (or couldn't even try...)
            return false;
        }
    }

    facename = tokenizer.GetNextToken();

    wxString tmp = tokenizer.GetNextToken();
    if ( tmp.empty() )
    {
        // default charset: but don't use DEFAULT_CHARSET here because it might
        // be different from the machine on which the file we had read this
        // encoding desc from was created
        charset = ANSI_CHARSET;
    }
    else
    {
        if ( wxSscanf(tmp, wxT("%u"), &charset) != 1 )
        {
            // should be a number!
            return false;
        }
    }

    return true;
}

wxString wxNativeEncodingInfo::ToString() const
{
    wxString s;

    s
#if wxUSE_FONTMAP
      // use the encoding names as this is safer than using the numerical
      // values which may change with time (because new encodings are
      // inserted...)
      << wxFontMapper::GetEncodingName(encoding)
#else // !wxUSE_FONTMAP
      // we don't have any choice but to use the raw value
      << (long)encoding
#endif // wxUSE_FONTMAP/!wxUSE_FONTMAP
      << wxT(';') << facename;

    // ANSI_CHARSET is assumed anyhow
    if ( charset != ANSI_CHARSET )
    {
         s << wxT(';') << charset;
    }

    return s;
}

// ----------------------------------------------------------------------------
// helper functions
// ----------------------------------------------------------------------------

bool wxGetNativeFontEncoding(wxFontEncoding encoding,
                             wxNativeEncodingInfo *info)
{
    wxCHECK_MSG( info, false, wxT("bad pointer in wxGetNativeFontEncoding") );

    if ( encoding == wxFONTENCODING_DEFAULT )
    {
        encoding = wxFont::GetDefaultEncoding();
    }

    extern WXDLLIMPEXP_BASE long wxEncodingToCharset(wxFontEncoding encoding);
    info->charset = wxEncodingToCharset(encoding);
    if ( info->charset == -1 )
        return false;

    info->encoding = encoding;

    return true;
}

bool wxTestFontEncoding(const wxNativeEncodingInfo& info)
{
    // try to create such font
    LOGFONT lf;
    wxZeroMemory(lf);       // all default values

    lf.lfCharSet = (BYTE)info.charset;
    wxStrlcpy(lf.lfFaceName, info.facename.c_str(), WXSIZEOF(lf.lfFaceName));

    HFONT hfont = ::CreateFontIndirect(&lf);
    if ( !hfont )
    {
        // no such font
        return false;
    }

    ::DeleteObject((HGDIOBJ)hfont);

    return true;
}

// ----------------------------------------------------------------------------
// wxFontEncoding <-> CHARSET_XXX
// ----------------------------------------------------------------------------

wxFontEncoding wxGetFontEncFromCharSet(int cs)
{
    wxFontEncoding fontEncoding;

    switch ( cs )
    {
        default:
            wxFAIL_MSG( wxT("unexpected Win32 charset") );
            // fall through and assume the system charset

        case DEFAULT_CHARSET:
            fontEncoding = wxFONTENCODING_SYSTEM;
            break;

        case ANSI_CHARSET:
            fontEncoding = wxFONTENCODING_CP1252;
            break;

        case SYMBOL_CHARSET:
            // what can we do here?
            fontEncoding = wxFONTENCODING_MAX;
            break;

#if defined(__WIN32__) && !defined(__WXMICROWIN__)
        case EASTEUROPE_CHARSET:
            fontEncoding = wxFONTENCODING_CP1250;
            break;

        case BALTIC_CHARSET:
            fontEncoding = wxFONTENCODING_CP1257;
            break;

        case RUSSIAN_CHARSET:
            fontEncoding = wxFONTENCODING_CP1251;
            break;

        case ARABIC_CHARSET:
            fontEncoding = wxFONTENCODING_CP1256;
            break;

        case GREEK_CHARSET:
            fontEncoding = wxFONTENCODING_CP1253;
            break;

        case HEBREW_CHARSET:
            fontEncoding = wxFONTENCODING_CP1255;
            break;

        case TURKISH_CHARSET:
            fontEncoding = wxFONTENCODING_CP1254;
            break;

        case THAI_CHARSET:
            fontEncoding = wxFONTENCODING_CP874;
            break;

        case SHIFTJIS_CHARSET:
            fontEncoding = wxFONTENCODING_CP932;
            break;

        case GB2312_CHARSET:
            fontEncoding = wxFONTENCODING_CP936;
            break;

        case HANGUL_CHARSET:
            fontEncoding = wxFONTENCODING_CP949;
            break;

        case CHINESEBIG5_CHARSET:
            fontEncoding = wxFONTENCODING_CP950;
            break;

        case VIETNAMESE_CHARSET:
            fontEncoding = wxFONTENCODING_CP1258;
            break;

        case JOHAB_CHARSET:
            fontEncoding = wxFONTENCODING_CP1361;
            break;

        case MAC_CHARSET:
            fontEncoding = wxFONTENCODING_MACROMAN;
            break;

#endif // Win32

        case OEM_CHARSET:
            fontEncoding = wxFONTENCODING_CP437;
            break;
    }

    return fontEncoding;
}

// ----------------------------------------------------------------------------
// wxFont <-> LOGFONT conversion
// ----------------------------------------------------------------------------

void wxFillLogFont(LOGFONT *logFont, const wxFont *font)
{
    wxNativeFontInfo fi;

    // maybe we already have LOGFONT for this font?
    const wxNativeFontInfo *pFI = font->GetNativeFontInfo();
    if ( !pFI )
    {
        // use wxNativeFontInfo methods to build a LOGFONT for this font
        fi.InitFromFont(*font);

        pFI = &fi;
    }

    // transfer all the data to LOGFONT
    *logFont = pFI->lf;
}

wxFont wxCreateFontFromLogFont(const LOGFONT *logFont)
{
    wxNativeFontInfo info;

    info.lf = *logFont;

    return wxFont(info);
}

