///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/fmapbase.cpp
// Purpose:     wxFontMapperBase class implementation
// Author:      Vadim Zeitlin
// Modified by:
// Created:     21.06.2003 (extracted from common/fontmap.cpp)
// Copyright:   (c) 1999-2003 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_FONTMAP

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/log.h"
    #include "wx/intl.h"
    #include "wx/module.h"
    #include "wx/wxcrtvararg.h"
#endif //WX_PRECOMP

#if defined(__WINDOWS__)
    #include  "wx/msw/private.h"  // includes windows.h for LOGFONT
    #include  "wx/msw/winundef.h"
#endif

#include "wx/fontmap.h"
#include "wx/fmappriv.h"

#include "wx/apptrait.h"

// wxMemoryConfig uses wxFileConfig
#if wxUSE_CONFIG && wxUSE_FILECONFIG
    #include "wx/config.h"
    #include "wx/memconf.h"
#endif

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// encodings supported by GetEncodingDescription
static const wxFontEncoding gs_encodings[] =
{
    wxFONTENCODING_ISO8859_1,
    wxFONTENCODING_ISO8859_2,
    wxFONTENCODING_ISO8859_3,
    wxFONTENCODING_ISO8859_4,
    wxFONTENCODING_ISO8859_5,
    wxFONTENCODING_ISO8859_6,
    wxFONTENCODING_ISO8859_7,
    wxFONTENCODING_ISO8859_8,
    wxFONTENCODING_ISO8859_9,
    wxFONTENCODING_ISO8859_10,
    wxFONTENCODING_ISO8859_11,
    wxFONTENCODING_ISO8859_12,
    wxFONTENCODING_ISO8859_13,
    wxFONTENCODING_ISO8859_14,
    wxFONTENCODING_ISO8859_15,
    wxFONTENCODING_KOI8,
    wxFONTENCODING_KOI8_U,
    wxFONTENCODING_CP866,
    wxFONTENCODING_CP874,
    wxFONTENCODING_CP932,
    wxFONTENCODING_CP936,
    wxFONTENCODING_CP949,
    wxFONTENCODING_CP950,
    wxFONTENCODING_CP1250,
    wxFONTENCODING_CP1251,
    wxFONTENCODING_CP1252,
    wxFONTENCODING_CP1253,
    wxFONTENCODING_CP1254,
    wxFONTENCODING_CP1255,
    wxFONTENCODING_CP1256,
    wxFONTENCODING_CP1257,
    wxFONTENCODING_CP1258,
    wxFONTENCODING_CP1361,
    wxFONTENCODING_CP437,
    wxFONTENCODING_UTF7,
    wxFONTENCODING_UTF8,
    wxFONTENCODING_UTF16BE,
    wxFONTENCODING_UTF16LE,
    wxFONTENCODING_UTF32BE,
    wxFONTENCODING_UTF32LE,
    wxFONTENCODING_EUC_JP,
    wxFONTENCODING_DEFAULT,
    wxFONTENCODING_ISO2022_JP,

    wxFONTENCODING_MACROMAN,
    wxFONTENCODING_MACJAPANESE,
    wxFONTENCODING_MACCHINESETRAD,
    wxFONTENCODING_MACKOREAN,
    wxFONTENCODING_MACARABIC,
    wxFONTENCODING_MACHEBREW,
    wxFONTENCODING_MACGREEK,
    wxFONTENCODING_MACCYRILLIC,
    wxFONTENCODING_MACDEVANAGARI,
    wxFONTENCODING_MACGURMUKHI,
    wxFONTENCODING_MACGUJARATI,
    wxFONTENCODING_MACORIYA,
    wxFONTENCODING_MACBENGALI,
    wxFONTENCODING_MACTAMIL,
    wxFONTENCODING_MACTELUGU,
    wxFONTENCODING_MACKANNADA,
    wxFONTENCODING_MACMALAJALAM,
    wxFONTENCODING_MACSINHALESE,
    wxFONTENCODING_MACBURMESE,
    wxFONTENCODING_MACKHMER,
    wxFONTENCODING_MACTHAI,
    wxFONTENCODING_MACLAOTIAN,
    wxFONTENCODING_MACGEORGIAN,
    wxFONTENCODING_MACARMENIAN,
    wxFONTENCODING_MACCHINESESIMP,
    wxFONTENCODING_MACTIBETAN,
    wxFONTENCODING_MACMONGOLIAN,
    wxFONTENCODING_MACETHIOPIC,
    wxFONTENCODING_MACCENTRALEUR,
    wxFONTENCODING_MACVIATNAMESE,
    wxFONTENCODING_MACARABICEXT,
    wxFONTENCODING_MACSYMBOL,
    wxFONTENCODING_MACDINGBATS,
    wxFONTENCODING_MACTURKISH,
    wxFONTENCODING_MACCROATIAN,
    wxFONTENCODING_MACICELANDIC,
    wxFONTENCODING_MACROMANIAN,
    wxFONTENCODING_MACCELTIC,
    wxFONTENCODING_MACGAELIC,
    wxFONTENCODING_MACKEYBOARD
};

// the descriptions for them
static const char* const gs_encodingDescs[] =
{
    wxTRANSLATE( "Western European (ISO-8859-1)" ),
    wxTRANSLATE( "Central European (ISO-8859-2)" ),
    wxTRANSLATE( "Esperanto (ISO-8859-3)" ),
    wxTRANSLATE( "Baltic (old) (ISO-8859-4)" ),
    wxTRANSLATE( "Cyrillic (ISO-8859-5)" ),
    wxTRANSLATE( "Arabic (ISO-8859-6)" ),
    wxTRANSLATE( "Greek (ISO-8859-7)" ),
    wxTRANSLATE( "Hebrew (ISO-8859-8)" ),
    wxTRANSLATE( "Turkish (ISO-8859-9)" ),
    wxTRANSLATE( "Nordic (ISO-8859-10)" ),
    wxTRANSLATE( "Thai (ISO-8859-11)" ),
    wxTRANSLATE( "Indian (ISO-8859-12)" ),
    wxTRANSLATE( "Baltic (ISO-8859-13)" ),
    wxTRANSLATE( "Celtic (ISO-8859-14)" ),
    wxTRANSLATE( "Western European with Euro (ISO-8859-15)" ),
    wxTRANSLATE( "KOI8-R" ),
    wxTRANSLATE( "KOI8-U" ),
    wxTRANSLATE( "Windows/DOS OEM Cyrillic (CP 866)" ),
    wxTRANSLATE( "Windows Thai (CP 874)" ),
    wxTRANSLATE( "Windows Japanese (CP 932) or Shift-JIS" ),
    wxTRANSLATE( "Windows Chinese Simplified (CP 936) or GB-2312" ),
    wxTRANSLATE( "Windows Korean (CP 949)" ),
    wxTRANSLATE( "Windows Chinese Traditional (CP 950) or Big-5" ),
    wxTRANSLATE( "Windows Central European (CP 1250)" ),
    wxTRANSLATE( "Windows Cyrillic (CP 1251)" ),
    wxTRANSLATE( "Windows Western European (CP 1252)" ),
    wxTRANSLATE( "Windows Greek (CP 1253)" ),
    wxTRANSLATE( "Windows Turkish (CP 1254)" ),
    wxTRANSLATE( "Windows Hebrew (CP 1255)" ),
    wxTRANSLATE( "Windows Arabic (CP 1256)" ),
    wxTRANSLATE( "Windows Baltic (CP 1257)" ),
    wxTRANSLATE( "Windows Vietnamese (CP 1258)" ),
    wxTRANSLATE( "Windows Johab (CP 1361)" ),
    wxTRANSLATE( "Windows/DOS OEM (CP 437)" ),
    wxTRANSLATE( "Unicode 7 bit (UTF-7)" ),
    wxTRANSLATE( "Unicode 8 bit (UTF-8)" ),
#ifdef WORDS_BIGENDIAN
    wxTRANSLATE( "Unicode 16 bit (UTF-16)" ),
    wxTRANSLATE( "Unicode 16 bit Little Endian (UTF-16LE)" ),
    wxTRANSLATE( "Unicode 32 bit (UTF-32)" ),
    wxTRANSLATE( "Unicode 32 bit Little Endian (UTF-32LE)" ),
#else // WORDS_BIGENDIAN
    wxTRANSLATE( "Unicode 16 bit Big Endian (UTF-16BE)" ),
    wxTRANSLATE( "Unicode 16 bit (UTF-16)" ),
    wxTRANSLATE( "Unicode 32 bit Big Endian (UTF-32BE)" ),
    wxTRANSLATE( "Unicode 32 bit (UTF-32)" ),
#endif // WORDS_BIGENDIAN
    wxTRANSLATE( "Extended Unix Codepage for Japanese (EUC-JP)" ),
    wxTRANSLATE( "US-ASCII" ),
    wxTRANSLATE( "ISO-2022-JP" ),

    wxTRANSLATE( "MacRoman" ),
    wxTRANSLATE( "MacJapanese" ),
    wxTRANSLATE( "MacChineseTrad" ),
    wxTRANSLATE( "MacKorean" ),
    wxTRANSLATE( "MacArabic" ),
    wxTRANSLATE( "MacHebrew" ),
    wxTRANSLATE( "MacGreek" ),
    wxTRANSLATE( "MacCyrillic" ),
    wxTRANSLATE( "MacDevanagari" ),
    wxTRANSLATE( "MacGurmukhi" ),
    wxTRANSLATE( "MacGujarati" ),
    wxTRANSLATE( "MacOriya" ),
    wxTRANSLATE( "MacBengali" ),
    wxTRANSLATE( "MacTamil" ),
    wxTRANSLATE( "MacTelugu" ),
    wxTRANSLATE( "MacKannada" ),
    wxTRANSLATE( "MacMalayalam" ),
    wxTRANSLATE( "MacSinhalese" ),
    wxTRANSLATE( "MacBurmese" ),
    wxTRANSLATE( "MacKhmer" ),
    wxTRANSLATE( "MacThai" ),
    wxTRANSLATE( "MacLaotian" ),
    wxTRANSLATE( "MacGeorgian" ),
    wxTRANSLATE( "MacArmenian" ),
    wxTRANSLATE( "MacChineseSimp" ),
    wxTRANSLATE( "MacTibetan" ),
    wxTRANSLATE( "MacMongolian" ),
    wxTRANSLATE( "MacEthiopic" ),
    wxTRANSLATE( "MacCentralEurRoman" ),
    wxTRANSLATE( "MacVietnamese" ),
    wxTRANSLATE( "MacExtArabic" ),
    wxTRANSLATE( "MacSymbol" ),
    wxTRANSLATE( "MacDingbats" ),
    wxTRANSLATE( "MacTurkish" ),
    wxTRANSLATE( "MacCroatian" ),
    wxTRANSLATE( "MacIcelandic" ),
    wxTRANSLATE( "MacRomanian" ),
    wxTRANSLATE( "MacCeltic" ),
    wxTRANSLATE( "MacGaelic" ),
    wxTRANSLATE( "MacKeyboardGlyphs" )
};

// and the internal names (these are not translated on purpose!)
static const wxChar* const gs_encodingNames[][9] =
{
    // names from the columns correspond to these OS:
    //      Linux        Solaris and IRIX       HP-UX             AIX
    { wxT("ISO-8859-1"),  wxT("ISO8859-1"),  wxT("iso88591"),  wxT("8859-1"), wxT("iso_8859_1"), NULL },
    { wxT("ISO-8859-2"),  wxT("ISO8859-2"),  wxT("iso88592"),  wxT("8859-2"), NULL },
    { wxT("ISO-8859-3"),  wxT("ISO8859-3"),  wxT("iso88593"),  wxT("8859-3"), NULL },
    { wxT("ISO-8859-4"),  wxT("ISO8859-4"),  wxT("iso88594"),  wxT("8859-4"), NULL },
    { wxT("ISO-8859-5"),  wxT("ISO8859-5"),  wxT("iso88595"),  wxT("8859-5"), NULL },
    { wxT("ISO-8859-6"),  wxT("ISO8859-6"),  wxT("iso88596"),  wxT("8859-6"), NULL },
    { wxT("ISO-8859-7"),  wxT("ISO8859-7"),  wxT("iso88597"),  wxT("8859-7"), NULL },
    { wxT("ISO-8859-8"),  wxT("ISO8859-8"),  wxT("iso88598"),  wxT("8859-8"), NULL },
    { wxT("ISO-8859-9"),  wxT("ISO8859-9"),  wxT("iso88599"),  wxT("8859-9"), NULL },
    { wxT("ISO-8859-10"), wxT("ISO8859-10"), wxT("iso885910"), wxT("8859-10"), NULL },
    { wxT("ISO-8859-11"), wxT("ISO8859-11"), wxT("iso885911"), wxT("8859-11"), NULL },
    { wxT("ISO-8859-12"), wxT("ISO8859-12"), wxT("iso885912"), wxT("8859-12"), NULL },
    { wxT("ISO-8859-13"), wxT("ISO8859-13"), wxT("iso885913"), wxT("8859-13"), NULL },
    { wxT("ISO-8859-14"), wxT("ISO8859-14"), wxT("iso885914"), wxT("8859-14"), NULL },
    { wxT("ISO-8859-15"), wxT("ISO8859-15"), wxT("iso885915"), wxT("8859-15"), NULL },

    // although koi8-ru is not strictly speaking the same as koi8-r,
    // they are similar enough to make mapping it to koi8 better than
    // not recognizing it at all
    { wxT( "KOI8-R" ), wxT( "KOI8-RU" ), NULL },
    { wxT( "KOI8-U" ), NULL },

    { wxT( "WINDOWS-866" ), wxT( "CP866" ), NULL },

    { wxT( "WINDOWS-874" ), wxT( "CP874" ), wxT( "MS874" ), wxT( "IBM-874" ), NULL },
    { wxT( "WINDOWS-932" ), wxT( "CP932" ), wxT( "MS932" ), wxT( "IBM-932" ), wxT( "SJIS" ), wxT( "SHIFT-JIS" ), wxT( "SHIFT_JIS" ), NULL },
    { wxT( "WINDOWS-936" ), wxT( "CP936" ), wxT( "MS936" ), wxT( "IBM-936" ), wxT( "GB2312" ), wxT( "gbk" ),wxT( "GBK" ), NULL },
    { wxT( "WINDOWS-949" ), wxT( "CP949" ), wxT( "MS949" ), wxT( "IBM-949" ), wxT( "EUC-KR" ), wxT( "eucKR" ), wxT( "euc_kr" ), NULL },
    { wxT( "WINDOWS-950" ), wxT( "CP950" ), wxT( "MS950" ), wxT( "IBM-950" ), wxT( "BIG5" ), wxT( "BIG-5" ), wxT( "BIG-FIVE" ), NULL },
    { wxT( "WINDOWS-1250" ),wxT( "CP1250" ),wxT( "MS1250" ),wxT( "IBM-1250" ),NULL },
    { wxT( "WINDOWS-1251" ),wxT( "CP1251" ),wxT( "MS1251" ),wxT( "IBM-1251" ),NULL },
    { wxT( "WINDOWS-1252" ),wxT( "CP1252" ),wxT( "MS1252" ),wxT( "IBM-1252" ),NULL },
    { wxT( "WINDOWS-1253" ),wxT( "CP1253" ),wxT( "MS1253" ),wxT( "IBM-1253" ),NULL },
    { wxT( "WINDOWS-1254" ),wxT( "CP1254" ),wxT( "MS1254" ),wxT( "IBM-1254" ),NULL },
    { wxT( "WINDOWS-1255" ),wxT( "CP1255" ),wxT( "MS1255" ),wxT( "IBM-1255" ),NULL },
    { wxT( "WINDOWS-1256" ),wxT( "CP1256" ),wxT( "MS1256" ),wxT( "IBM-1256" ),NULL },
    { wxT( "WINDOWS-1257" ),wxT( "CP1257" ),wxT( "MS1257" ),wxT( "IBM-1257" ),NULL },
    { wxT( "WINDOWS-1258" ),wxT( "CP1258" ),wxT( "MS1258" ),wxT( "IBM-1258" ),NULL },
    { wxT( "WINDOWS-1361" ),wxT( "CP1361" ),wxT( "MS1361" ),wxT( "IBM-1361" ), wxT( "JOHAB" ), NULL },
    { wxT( "WINDOWS-437" ), wxT( "CP437" ), wxT( "MS437" ), wxT( "IBM-437" ), NULL },

    { wxT( "UTF-7" ), wxT("UTF7"), NULL },
    { wxT( "UTF-8" ), wxT("UTF8"), NULL },
#ifdef WORDS_BIGENDIAN
    { wxT( "UTF-16BE" ), wxT("UTF16BE"), wxT("UCS-2BE"), wxT("UCS2BE"), wxT("UTF-16"), wxT("UTF16"), wxT("UCS-2"), wxT("UCS2"), NULL },
    { wxT( "UTF-16LE" ), wxT("UTF16LE"), wxT("UCS-2LE"), wxT("UCS2LE"), NULL },
    { wxT( "UTF-32BE" ), wxT("UTF32BE"), wxT("UCS-4BE" ), wxT("UTF-32"), wxT("UTF32"), wxT("UCS-4"), wxT("UCS4"), NULL },
    { wxT( "UTF-32LE" ), wxT("UTF32LE"), wxT("UCS-4LE"), wxT("UCS4LE"), NULL },
#else // WORDS_BIGENDIAN
    { wxT("UTF-16BE"), wxT("UTF16BE"), wxT("UCS-2BE"), wxT("UCS2BE"), NULL },
    { wxT("UTF-16LE"), wxT("UTF16LE"), wxT("UCS-2LE"), wxT("UTF-16"), wxT("UTF16"), wxT("UCS-2"), wxT("UCS2"), NULL },
    { wxT("UTF-32BE"), wxT("UTF32BE"), wxT("UCS-4BE"), wxT("UCS4BE"), NULL },
    { wxT("UTF-32LE"), wxT("UTF32LE"), wxT("UCS-4LE"), wxT("UCS4LE"), wxT("UTF-32"), wxT("UTF32"), wxT("UCS-4"), wxT("UCS4"), NULL },
#endif // WORDS_BIGENDIAN

    { wxT( "EUC-JP" ), wxT( "eucJP" ), wxT( "euc_jp" ), wxT( "IBM-eucJP" ), NULL },

    // 646 is for Solaris, roman8 -- for HP-UX
    { wxT( "US-ASCII" ), wxT( "ASCII" ), wxT("C"), wxT("POSIX"), wxT("ANSI_X3.4-1968"),
      wxT("646"), wxT("roman8"), wxT( "" ), NULL },

    { wxT( "ISO-2022-JP" ), NULL },


    { wxT( "MacRoman" ), NULL },
    { wxT( "MacJapanese" ), NULL },
    { wxT( "MacChineseTrad" ), NULL },
    { wxT( "MacKorean" ), NULL },
    { wxT( "MacArabic" ), NULL },
    { wxT( "MacHebrew" ), NULL },
    { wxT( "MacGreek" ), NULL },
    { wxT( "MacCyrillic" ), NULL },
    { wxT( "MacDevanagari" ), NULL },
    { wxT( "MacGurmukhi" ), NULL },
    { wxT( "MacGujarati" ), NULL },
    { wxT( "MacOriya" ), NULL },
    { wxT( "MacBengali" ), NULL },
    { wxT( "MacTamil" ), NULL },
    { wxT( "MacTelugu" ), NULL },
    { wxT( "MacKannada" ), NULL },
    { wxT( "MacMalayalam" ), NULL },
    { wxT( "MacSinhalese" ), NULL },
    { wxT( "MacBurmese" ), NULL },
    { wxT( "MacKhmer" ), NULL },
    { wxT( "MacThai" ), NULL },
    { wxT( "MacLaotian" ), NULL },
    { wxT( "MacGeorgian" ), NULL },
    { wxT( "MacArmenian" ), NULL },
    { wxT( "MacChineseSimp" ), NULL },
    { wxT( "MacTibetan" ), NULL },
    { wxT( "MacMongolian" ), NULL },
    { wxT( "MacEthiopic" ), NULL },
    { wxT( "MacCentralEurRoman" ), NULL },
    { wxT( "MacVietnamese" ), NULL },
    { wxT( "MacExtArabic" ), NULL },
    { wxT( "MacSymbol" ), NULL },
    { wxT( "MacDingbats" ), NULL },
    { wxT( "MacTurkish" ), NULL },
    { wxT( "MacCroatian" ), NULL },
    { wxT( "MacIcelandic" ), NULL },
    { wxT( "MacRomanian" ), NULL },
    { wxT( "MacCeltic" ), NULL },
    { wxT( "MacGaelic" ), NULL },
    { wxT( "MacKeyboardGlyphs" ), NULL }
};

wxCOMPILE_TIME_ASSERT( WXSIZEOF(gs_encodingDescs) == WXSIZEOF(gs_encodings), EncodingsArraysNotInSync );
wxCOMPILE_TIME_ASSERT( WXSIZEOF(gs_encodingNames) == WXSIZEOF(gs_encodings), EncodingsArraysNotInSync );

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

// clean up the font mapper object
class wxFontMapperModule : public wxModule
{
public:
    wxFontMapperModule() : wxModule() { }

    virtual bool OnInit()
    {
        // a dummy wxFontMapperBase object could have been created during the
        // program startup before wxApp was created, we have to delete it to
        // allow creating the real font mapper next time it is needed now that
        // we can create it (when the modules are initialized, wxApp object
        // already exists)
        wxFontMapperBase *fm = wxFontMapperBase::Get();
        if ( fm && fm->IsDummy() )
            wxFontMapperBase::Reset();

        return true;
    }

    virtual void OnExit()
    {
        wxFontMapperBase::Reset();
    }

    DECLARE_DYNAMIC_CLASS(wxFontMapperModule)
};

IMPLEMENT_DYNAMIC_CLASS(wxFontMapperModule, wxModule)


// ============================================================================
// wxFontMapperBase implementation
// ============================================================================

wxFontMapper *wxFontMapperBase::sm_instance = NULL;

// ----------------------------------------------------------------------------
// ctor and dtor
// ----------------------------------------------------------------------------

wxFontMapperBase::wxFontMapperBase()
{
#if wxUSE_CONFIG && wxUSE_FILECONFIG
    m_configDummy = NULL;
#endif // wxUSE_CONFIG
}

wxFontMapperBase::~wxFontMapperBase()
{
#if wxUSE_CONFIG && wxUSE_FILECONFIG
    if ( m_configDummy )
        delete m_configDummy;
#endif // wxUSE_CONFIG
}

/* static */
wxFontMapperBase *wxFontMapperBase::Get()
{
    if ( !sm_instance )
    {
        wxAppTraits *traits = wxTheApp ? wxTheApp->GetTraits() : NULL;
        if ( traits )
        {
            sm_instance = traits->CreateFontMapper();

            wxASSERT_MSG( sm_instance,
                            wxT("wxAppTraits::CreateFontMapper() failed") );
        }

        if ( !sm_instance )
        {
            // last resort: we must create something because the existing code
            // relies on always having a valid font mapper object
            sm_instance = (wxFontMapper *)new wxFontMapperBase;
        }
    }

    return (wxFontMapperBase*)sm_instance;
}

/* static */
wxFontMapper *wxFontMapperBase::Set(wxFontMapper *mapper)
{
    wxFontMapper *old = sm_instance;
    sm_instance = mapper;
    return old;
}

/* static */
void wxFontMapperBase::Reset()
{
    if ( sm_instance )
    {
        // we need a cast as wxFontMapper is not fully declared here and so the
        // compiler can't know that it derives from wxFontMapperBase (but
        // run-time behaviour will be correct because the dtor is virtual)
        delete (wxFontMapperBase *)sm_instance;
        sm_instance = NULL;
    }
}

#if wxUSE_CONFIG && wxUSE_FILECONFIG

// ----------------------------------------------------------------------------
// config usage customisation
// ----------------------------------------------------------------------------


static wxString gs_defaultConfigPath(FONTMAPPER_ROOT_PATH);

/* static */
const wxString& wxFontMapperBase::GetDefaultConfigPath()
{
    // NB: we return const wxString& and not wxString for compatibility
    //     with 2.8 that returned const wxChar*
    return gs_defaultConfigPath;
}

void wxFontMapperBase::SetConfigPath(const wxString& prefix)
{
    wxCHECK_RET( !prefix.empty() && prefix[0] == wxCONFIG_PATH_SEPARATOR,
                 wxT("an absolute path should be given to wxFontMapper::SetConfigPath()") );

    m_configRootPath = prefix;
}

// ----------------------------------------------------------------------------
// get config object and path for it
// ----------------------------------------------------------------------------

wxConfigBase *wxFontMapperBase::GetConfig()
{
    wxConfigBase *config = wxConfig::Get(false);

    // If there is no global configuration, use an internal memory configuration
    if ( !config )
    {
        if ( !m_configDummy )
            m_configDummy = new wxMemoryConfig;
        config = m_configDummy;

        // FIXME: ideally, we should add keys from dummy config to a real one later,
        //        but it is a low-priority task because typical wxWin application
        //        either doesn't use wxConfig at all or creates wxConfig object in
        //        wxApp::OnInit(), before any real interaction with the user takes
        //        place...
    }

    return config;
}

const wxString& wxFontMapperBase::GetConfigPath()
{
    if ( !m_configRootPath )
    {
        // use the default
        m_configRootPath = GetDefaultConfigPath();
    }

    return m_configRootPath;
}

// ----------------------------------------------------------------------------
// config helpers
// ----------------------------------------------------------------------------

bool wxFontMapperBase::ChangePath(const wxString& pathNew, wxString *pathOld)
{
    wxConfigBase *config = GetConfig();
    if ( !config )
        return false;

    *pathOld = config->GetPath();

    wxString path = GetConfigPath();
    if ( path.empty() || path.Last() != wxCONFIG_PATH_SEPARATOR )
    {
        path += wxCONFIG_PATH_SEPARATOR;
    }

    wxASSERT_MSG( !pathNew || (pathNew[0] != wxCONFIG_PATH_SEPARATOR),
                  wxT("should be a relative path") );

    path += pathNew;

    config->SetPath(path);

    return true;
}

void wxFontMapperBase::RestorePath(const wxString& pathOld)
{
    GetConfig()->SetPath(pathOld);
}

#endif

// ----------------------------------------------------------------------------
// charset/encoding correspondence
// ----------------------------------------------------------------------------

wxFontEncoding
wxFontMapperBase::CharsetToEncoding(const wxString& charset,
                                    bool WXUNUSED(interactive))
{
    int enc = NonInteractiveCharsetToEncoding(charset);
    if ( enc == wxFONTENCODING_UNKNOWN )
    {
        // we should return wxFONTENCODING_SYSTEM from here for unknown
        // encodings
        enc = wxFONTENCODING_SYSTEM;
    }

    return (wxFontEncoding)enc;
}

int
wxFontMapperBase::NonInteractiveCharsetToEncoding(const wxString& charset)
{
    wxFontEncoding encoding = wxFONTENCODING_SYSTEM;

    // we're going to modify it, make a copy
    wxString cs = charset;

#if wxUSE_CONFIG && wxUSE_FILECONFIG
    // first try the user-defined settings
    wxFontMapperPathChanger path(this, FONTMAPPER_CHARSET_PATH);
    if ( path.IsOk() )
    {
        wxConfigBase *config = GetConfig();

        // do we have an encoding for this charset?
        long value = config->Read(charset, -1l);
        if ( value != -1 )
        {
            if ( value == wxFONTENCODING_UNKNOWN )
            {
                // don't try to find it, in particular don't ask the user
                return value;
            }

            if ( value >= 0 && value <= wxFONTENCODING_MAX )
            {
                encoding = (wxFontEncoding)value;
            }
            else
            {
                wxLogDebug(wxT("corrupted config data: invalid encoding %ld for charset '%s' ignored"),
                           value, charset.c_str());
            }
        }

        if ( encoding == wxFONTENCODING_SYSTEM )
        {
            // may be we have an alias?
            config->SetPath(FONTMAPPER_CHARSET_ALIAS_PATH);

            wxString alias = config->Read(charset);
            if ( !alias.empty() )
            {
                // yes, we do - use it instead
                cs = alias;
            }
        }
    }
#endif // wxUSE_CONFIG

    // if didn't find it there, try to recognize it ourselves
    if ( encoding == wxFONTENCODING_SYSTEM )
    {
        // trim any spaces
        cs.Trim(true);
        cs.Trim(false);

        // discard the optional quotes
        if ( !cs.empty() )
        {
            if ( cs[0u] == wxT('"') && cs.Last() == wxT('"') )
            {
                cs = wxString(cs.c_str(), cs.length() - 1);
            }
        }

        for ( size_t i = 0; i < WXSIZEOF(gs_encodingNames); ++i )
        {
            for ( const wxChar* const* encName = gs_encodingNames[i]; *encName; ++encName )
            {
                if ( cs.CmpNoCase(*encName) == 0 )
                    return gs_encodings[i];
            }
        }

        cs.MakeUpper();

        if ( cs.Left(3) == wxT("ISO") )
        {
            // the dash is optional (or, to be exact, it is not, but
            // several broken programs "forget" it)
            const wxChar *p = cs.c_str() + 3;
            if ( *p == wxT('-') )
                p++;

            unsigned int value;
            if ( wxSscanf(p, wxT("8859-%u"), &value) == 1 )
            {
                // make it 0 based and check that it is strictly positive in
                // the process (no such thing as iso8859-0 encoding)
                if ( (value-- > 0) &&
                     (value < wxFONTENCODING_ISO8859_MAX -
                              wxFONTENCODING_ISO8859_1) )
                {
                    // it's a valid ISO8859 encoding
                    value += wxFONTENCODING_ISO8859_1;
                    encoding = (wxFontEncoding)value;
                }
            }
        }
        else if ( cs.Left(4) == wxT("8859") )
        {
            const wxChar *p = cs.c_str();

            unsigned int value;
            if ( wxSscanf(p, wxT("8859-%u"), &value) == 1 )
            {
                // make it 0 based and check that it is strictly positive in
                // the process (no such thing as iso8859-0 encoding)
                if ( (value-- > 0) &&
                     (value < wxFONTENCODING_ISO8859_MAX -
                              wxFONTENCODING_ISO8859_1) )
                {
                    // it's a valid ISO8859 encoding
                    value += wxFONTENCODING_ISO8859_1;
                    encoding = (wxFontEncoding)value;
                }
            }
        }
        else // check for Windows charsets
        {
            size_t len;
            if ( cs.Left(7) == wxT("WINDOWS") )
            {
                len = 7;
            }
            else if ( cs.Left(2) == wxT("CP") )
            {
                len = 2;
            }
            else // not a Windows encoding
            {
                len = 0;
            }

            if ( len )
            {
                const wxChar *p = cs.c_str() + len;
                if ( *p == wxT('-') )
                    p++;

                unsigned int value;
                if ( wxSscanf(p, wxT("%u"), &value) == 1 )
                {
                    if ( value >= 1250 )
                    {
                        value -= 1250;
                        if ( value < wxFONTENCODING_CP12_MAX -
                                     wxFONTENCODING_CP1250 )
                        {
                            // a valid Windows code page
                            value += wxFONTENCODING_CP1250;
                            encoding = (wxFontEncoding)value;
                        }
                    }

                    switch ( value )
                    {
                        case 866:
                            encoding = wxFONTENCODING_CP866;
                            break;

                        case 874:
                            encoding = wxFONTENCODING_CP874;
                            break;

                        case 932:
                            encoding = wxFONTENCODING_CP932;
                            break;

                        case 936:
                            encoding = wxFONTENCODING_CP936;
                            break;

                        case 949:
                            encoding = wxFONTENCODING_CP949;
                            break;

                        case 950:
                            encoding = wxFONTENCODING_CP950;
                            break;

                        case 1258:
                            encoding = wxFONTENCODING_CP1258;
                            break;

                        case 1361:
                            encoding = wxFONTENCODING_CP1361;
                            break;
                    }
                }
            }
        }
        //else: unknown
    }

    return encoding;
}

/* static */
size_t wxFontMapperBase::GetSupportedEncodingsCount()
{
    return WXSIZEOF(gs_encodings);
}

/* static */
wxFontEncoding wxFontMapperBase::GetEncoding(size_t n)
{
    wxCHECK_MSG( n < WXSIZEOF(gs_encodings), wxFONTENCODING_SYSTEM,
                    wxT("wxFontMapper::GetEncoding(): invalid index") );

    return gs_encodings[n];
}

/* static */
wxString wxFontMapperBase::GetEncodingDescription(wxFontEncoding encoding)
{
    if ( encoding == wxFONTENCODING_DEFAULT )
    {
        return _("Default encoding");
    }

    const size_t count = WXSIZEOF(gs_encodingDescs);

    for ( size_t i = 0; i < count; i++ )
    {
        if ( gs_encodings[i] == encoding )
        {
            return wxGetTranslation(gs_encodingDescs[i]);
        }
    }

    wxString str;
    str.Printf(_("Unknown encoding (%d)"), encoding);

    return str;
}

/* static */
wxString wxFontMapperBase::GetEncodingName(wxFontEncoding encoding)
{
    if ( encoding == wxFONTENCODING_DEFAULT )
    {
        return _("default");
    }

    const size_t count = WXSIZEOF(gs_encodingNames);

    for ( size_t i = 0; i < count; i++ )
    {
        if ( gs_encodings[i] == encoding )
        {
            return gs_encodingNames[i][0];
        }
    }

    wxString str;
    str.Printf(_("unknown-%d"), encoding);

    return str;
}

/* static */
const wxChar** wxFontMapperBase::GetAllEncodingNames(wxFontEncoding encoding)
{
    static const wxChar* const dummy[] = { NULL };

    for ( size_t i = 0; i < WXSIZEOF(gs_encodingNames); i++ )
    {
        if ( gs_encodings[i] == encoding )
        {
            return const_cast<const wxChar**>(gs_encodingNames[i]);
        }
    }

    return const_cast<const wxChar**>(dummy);
}

/* static */
wxFontEncoding wxFontMapperBase::GetEncodingFromName(const wxString& name)
{
    const size_t count = WXSIZEOF(gs_encodingNames);

    for ( size_t i = 0; i < count; i++ )
    {
        for ( const wxChar* const* encName = gs_encodingNames[i]; *encName; ++encName )
        {
            if ( name.CmpNoCase(*encName) == 0 )
                return gs_encodings[i];
        }
    }

    return wxFONTENCODING_MAX;
}

#endif // wxUSE_FONTMAP
