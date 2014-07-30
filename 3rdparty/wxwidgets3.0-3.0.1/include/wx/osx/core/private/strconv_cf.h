/////////////////////////////////////////////////////////////////////////////
// Name:        wx/osx/core/private/strconv_cf.h
// Purpose:     Unicode conversion classes
// Author:      David Elliott, Ryan Norton
// Modified by:
// Created:     2007-07-06
// Copyright:   (c) 2004 Ryan Norton
//              (c) 2007 David Elliott
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/strconv.h"

#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFStringEncodingExt.h>

// ============================================================================
// CoreFoundation conversion classes
// ============================================================================

inline CFStringEncoding wxCFStringEncFromFontEnc(wxFontEncoding encoding)
{
    CFStringEncoding enc = kCFStringEncodingInvalidId ;

    switch (encoding)
    {
        case wxFONTENCODING_DEFAULT :
            enc = CFStringGetSystemEncoding();
            break ;

        case wxFONTENCODING_ISO8859_1 :
            enc = kCFStringEncodingISOLatin1 ;
            break ;
        case wxFONTENCODING_ISO8859_2 :
            enc = kCFStringEncodingISOLatin2;
            break ;
        case wxFONTENCODING_ISO8859_3 :
            enc = kCFStringEncodingISOLatin3 ;
            break ;
        case wxFONTENCODING_ISO8859_4 :
            enc = kCFStringEncodingISOLatin4;
            break ;
        case wxFONTENCODING_ISO8859_5 :
            enc = kCFStringEncodingISOLatinCyrillic;
            break ;
        case wxFONTENCODING_ISO8859_6 :
            enc = kCFStringEncodingISOLatinArabic;
            break ;
        case wxFONTENCODING_ISO8859_7 :
            enc = kCFStringEncodingISOLatinGreek;
            break ;
        case wxFONTENCODING_ISO8859_8 :
            enc = kCFStringEncodingISOLatinHebrew;
            break ;
        case wxFONTENCODING_ISO8859_9 :
            enc = kCFStringEncodingISOLatin5;
            break ;
        case wxFONTENCODING_ISO8859_10 :
            enc = kCFStringEncodingISOLatin6;
            break ;
        case wxFONTENCODING_ISO8859_11 :
            enc = kCFStringEncodingISOLatinThai;
            break ;
        case wxFONTENCODING_ISO8859_13 :
            enc = kCFStringEncodingISOLatin7;
            break ;
        case wxFONTENCODING_ISO8859_14 :
            enc = kCFStringEncodingISOLatin8;
            break ;
        case wxFONTENCODING_ISO8859_15 :
            enc = kCFStringEncodingISOLatin9;
            break ;

        case wxFONTENCODING_KOI8 :
            enc = kCFStringEncodingKOI8_R;
            break ;
        case wxFONTENCODING_ALTERNATIVE : // MS-DOS CP866
            enc = kCFStringEncodingDOSRussian;
            break ;

//      case wxFONTENCODING_BULGARIAN :
//          enc = ;
//          break ;

        case wxFONTENCODING_CP437 :
            enc = kCFStringEncodingDOSLatinUS ;
            break ;
        case wxFONTENCODING_CP850 :
            enc = kCFStringEncodingDOSLatin1;
            break ;
        case wxFONTENCODING_CP852 :
            enc = kCFStringEncodingDOSLatin2;
            break ;
        case wxFONTENCODING_CP855 :
            enc = kCFStringEncodingDOSCyrillic;
            break ;
        case wxFONTENCODING_CP866 :
            enc = kCFStringEncodingDOSRussian ;
            break ;
        case wxFONTENCODING_CP874 :
            enc = kCFStringEncodingDOSThai;
            break ;
        case wxFONTENCODING_CP932 :
            enc = kCFStringEncodingDOSJapanese;
            break ;
        case wxFONTENCODING_CP936 :
            enc = kCFStringEncodingDOSChineseSimplif ;
            break ;
        case wxFONTENCODING_CP949 :
            enc = kCFStringEncodingDOSKorean;
            break ;
        case wxFONTENCODING_CP950 :
            enc = kCFStringEncodingDOSChineseTrad;
            break ;
        case wxFONTENCODING_CP1250 :
            enc = kCFStringEncodingWindowsLatin2;
            break ;
        case wxFONTENCODING_CP1251 :
            enc = kCFStringEncodingWindowsCyrillic ;
            break ;
        case wxFONTENCODING_CP1252 :
            enc = kCFStringEncodingWindowsLatin1 ;
            break ;
        case wxFONTENCODING_CP1253 :
            enc = kCFStringEncodingWindowsGreek;
            break ;
        case wxFONTENCODING_CP1254 :
            enc = kCFStringEncodingWindowsLatin5;
            break ;
        case wxFONTENCODING_CP1255 :
            enc = kCFStringEncodingWindowsHebrew ;
            break ;
        case wxFONTENCODING_CP1256 :
            enc = kCFStringEncodingWindowsArabic ;
            break ;
        case wxFONTENCODING_CP1257 :
            enc = kCFStringEncodingWindowsBalticRim;
            break ;
//   This only really encodes to UTF7 (if that) evidently
//        case wxFONTENCODING_UTF7 :
//            enc = kCFStringEncodingNonLossyASCII ;
//            break ;
        case wxFONTENCODING_UTF8 :
            enc = kCFStringEncodingUTF8 ;
            break ;
        case wxFONTENCODING_EUC_JP :
            enc = kCFStringEncodingEUC_JP;
            break ;
/* Don't support conversion to/from UTF16 as wxWidgets can do this better.
 * In particular, ToWChar would fail miserably using strlen on an input UTF16.
        case wxFONTENCODING_UTF16 :
            enc = kCFStringEncodingUnicode ;
            break ;
*/
        case wxFONTENCODING_MACROMAN :
            enc = kCFStringEncodingMacRoman ;
            break ;
        case wxFONTENCODING_MACJAPANESE :
            enc = kCFStringEncodingMacJapanese ;
            break ;
        case wxFONTENCODING_MACCHINESETRAD :
            enc = kCFStringEncodingMacChineseTrad ;
            break ;
        case wxFONTENCODING_MACKOREAN :
            enc = kCFStringEncodingMacKorean ;
            break ;
        case wxFONTENCODING_MACARABIC :
            enc = kCFStringEncodingMacArabic ;
            break ;
        case wxFONTENCODING_MACHEBREW :
            enc = kCFStringEncodingMacHebrew ;
            break ;
        case wxFONTENCODING_MACGREEK :
            enc = kCFStringEncodingMacGreek ;
            break ;
        case wxFONTENCODING_MACCYRILLIC :
            enc = kCFStringEncodingMacCyrillic ;
            break ;
        case wxFONTENCODING_MACDEVANAGARI :
            enc = kCFStringEncodingMacDevanagari ;
            break ;
        case wxFONTENCODING_MACGURMUKHI :
            enc = kCFStringEncodingMacGurmukhi ;
            break ;
        case wxFONTENCODING_MACGUJARATI :
            enc = kCFStringEncodingMacGujarati ;
            break ;
        case wxFONTENCODING_MACORIYA :
            enc = kCFStringEncodingMacOriya ;
            break ;
        case wxFONTENCODING_MACBENGALI :
            enc = kCFStringEncodingMacBengali ;
            break ;
        case wxFONTENCODING_MACTAMIL :
            enc = kCFStringEncodingMacTamil ;
            break ;
        case wxFONTENCODING_MACTELUGU :
            enc = kCFStringEncodingMacTelugu ;
            break ;
        case wxFONTENCODING_MACKANNADA :
            enc = kCFStringEncodingMacKannada ;
            break ;
        case wxFONTENCODING_MACMALAJALAM :
            enc = kCFStringEncodingMacMalayalam ;
            break ;
        case wxFONTENCODING_MACSINHALESE :
            enc = kCFStringEncodingMacSinhalese ;
            break ;
        case wxFONTENCODING_MACBURMESE :
            enc = kCFStringEncodingMacBurmese ;
            break ;
        case wxFONTENCODING_MACKHMER :
            enc = kCFStringEncodingMacKhmer ;
            break ;
        case wxFONTENCODING_MACTHAI :
            enc = kCFStringEncodingMacThai ;
            break ;
        case wxFONTENCODING_MACLAOTIAN :
            enc = kCFStringEncodingMacLaotian ;
            break ;
        case wxFONTENCODING_MACGEORGIAN :
            enc = kCFStringEncodingMacGeorgian ;
            break ;
        case wxFONTENCODING_MACARMENIAN :
            enc = kCFStringEncodingMacArmenian ;
            break ;
        case wxFONTENCODING_MACCHINESESIMP :
            enc = kCFStringEncodingMacChineseSimp ;
            break ;
        case wxFONTENCODING_MACTIBETAN :
            enc = kCFStringEncodingMacTibetan ;
            break ;
        case wxFONTENCODING_MACMONGOLIAN :
            enc = kCFStringEncodingMacMongolian ;
            break ;
        case wxFONTENCODING_MACETHIOPIC :
            enc = kCFStringEncodingMacEthiopic ;
            break ;
        case wxFONTENCODING_MACCENTRALEUR :
            enc = kCFStringEncodingMacCentralEurRoman ;
            break ;
        case wxFONTENCODING_MACVIATNAMESE :
            enc = kCFStringEncodingMacVietnamese ;
            break ;
        case wxFONTENCODING_MACARABICEXT :
            enc = kCFStringEncodingMacExtArabic ;
            break ;
        case wxFONTENCODING_MACSYMBOL :
            enc = kCFStringEncodingMacSymbol ;
            break ;
        case wxFONTENCODING_MACDINGBATS :
            enc = kCFStringEncodingMacDingbats ;
            break ;
        case wxFONTENCODING_MACTURKISH :
            enc = kCFStringEncodingMacTurkish ;
            break ;
        case wxFONTENCODING_MACCROATIAN :
            enc = kCFStringEncodingMacCroatian ;
            break ;
        case wxFONTENCODING_MACICELANDIC :
            enc = kCFStringEncodingMacIcelandic ;
            break ;
        case wxFONTENCODING_MACROMANIAN :
            enc = kCFStringEncodingMacRomanian ;
            break ;
        case wxFONTENCODING_MACCELTIC :
            enc = kCFStringEncodingMacCeltic ;
            break ;
        case wxFONTENCODING_MACGAELIC :
            enc = kCFStringEncodingMacGaelic ;
            break ;
        /* CFString is known to support this back to the original CarbonLib */
        /* http://developer.apple.com/samplecode/CarbonMDEF/listing2.html */
        case wxFONTENCODING_MACKEYBOARD :
            /* We don't wish to pollute the namespace too much, even though we're a private header. */
            /* The constant is well-defined as 41 and is not expected to change. */
            enc = 41 /*kTextEncodingMacKeyboardGlyphs*/ ;
            break ;

        default :
            // because gcc is picky
            break ;
    }

    return enc ;
}


class wxMBConv_cf : public wxMBConv
{
public:
    wxMBConv_cf()
    {
        Init(CFStringGetSystemEncoding()) ;
    }

    wxMBConv_cf(const wxMBConv_cf& conv) : wxMBConv()
    {
        m_encoding = conv.m_encoding;
    }

#if wxUSE_FONTMAP
    wxMBConv_cf(const char* name)
    {
        Init( wxCFStringEncFromFontEnc(wxFontMapperBase::Get()->CharsetToEncoding(name, false) ) ) ;
    }
#endif

    wxMBConv_cf(wxFontEncoding encoding)
    {
        Init( wxCFStringEncFromFontEnc(encoding) );
    }

    virtual ~wxMBConv_cf()
    {
    }

    void Init( CFStringEncoding encoding)
    {
        m_encoding = encoding ;
    }

    virtual size_t ToWChar(wchar_t * dst, size_t dstSize, const char * src, size_t srcSize = wxNO_LEN) const;
    virtual size_t FromWChar(char *dst, size_t dstSize, const wchar_t *src, size_t srcSize = wxNO_LEN) const;

    virtual wxMBConv *Clone() const { return new wxMBConv_cf(*this); }

    bool IsOk() const
    {
        return m_encoding != kCFStringEncodingInvalidId &&
              CFStringIsEncodingAvailable(m_encoding);
    }

private:
    CFStringEncoding m_encoding ;
};

