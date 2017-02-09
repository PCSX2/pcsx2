/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/intl.cpp
// Purpose:     Internationalization and localisation for wxWidgets
// Author:      Vadim Zeitlin
// Modified by: Michael N. Filippov <michael@idisys.iae.nsk.su>
//              (2003/09/30 - PluralForms support)
// Created:     29/01/98
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declaration
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifdef __EMX__
// The following define is needed by Innotek's libc to
// make the definition of struct localeconv available.
#define __INTERNAL_DEFS
#endif

#if wxUSE_INTL

#ifndef WX_PRECOMP
    #include "wx/dynarray.h"
    #include "wx/string.h"
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/utils.h"
    #include "wx/app.h"
    #include "wx/hashmap.h"
    #include "wx/module.h"
#endif // WX_PRECOMP

#ifndef __WXWINCE__
    #include <locale.h>
#endif

// standard headers
#include <ctype.h>
#include <stdlib.h>
#ifdef HAVE_LANGINFO_H
    #include <langinfo.h>
#endif

#ifdef __WIN32__
    #include "wx/dynlib.h"
    #include "wx/msw/private.h"

    #ifndef LOCALE_SNAME
    #define LOCALE_SNAME 0x5c
    #endif
#endif

#include "wx/file.h"
#include "wx/filename.h"
#include "wx/tokenzr.h"
#include "wx/fontmap.h"
#include "wx/scopedptr.h"
#include "wx/apptrait.h"
#include "wx/stdpaths.h"
#include "wx/hashset.h"

#if defined(__WXOSX__)
    #include "wx/osx/core/cfref.h"
    #include <CoreFoundation/CFLocale.h>
    #include <CoreFoundation/CFDateFormatter.h>
    #include "wx/osx/core/cfstring.h"
#endif

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

#define TRACE_I18N wxS("i18n")

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// global functions
// ----------------------------------------------------------------------------

static wxLocale *wxSetLocale(wxLocale *pLocale);

namespace
{

// get just the language part ("en" in "en_GB")
inline wxString ExtractLang(const wxString& langFull)
{
    return langFull.BeforeFirst('_');
}

// helper functions of GetSystemLanguage()
#ifdef __UNIX__

// get everything else (including the leading '_')
inline wxString ExtractNotLang(const wxString& langFull)
{
    size_t pos = langFull.find('_');
    if ( pos != wxString::npos )
        return langFull.substr(pos);
    else
        return wxString();
}

#endif // __UNIX__

} // anonymous namespace

// ----------------------------------------------------------------------------
// wxLanguageInfo
// ----------------------------------------------------------------------------

#ifdef __WINDOWS__

// helper used by wxLanguageInfo::GetLocaleName() and elsewhere to determine
// whether the locale is Unicode-only (it is if this function returns empty
// string)
static wxString wxGetANSICodePageForLocale(LCID lcid)
{
    wxString cp;

    wxChar buffer[16];
    if ( ::GetLocaleInfo(lcid, LOCALE_IDEFAULTANSICODEPAGE,
                        buffer, WXSIZEOF(buffer)) > 0 )
    {
        if ( buffer[0] != wxT('0') || buffer[1] != wxT('\0') )
            cp = buffer;
        //else: this locale doesn't use ANSI code page
    }

    return cp;
}

wxUint32 wxLanguageInfo::GetLCID() const
{
    return MAKELCID(MAKELANGID(WinLang, WinSublang), SORT_DEFAULT);
}

wxString wxLanguageInfo::GetLocaleName() const
{
    wxString locale;

    const LCID lcid = GetLCID();

    wxChar buffer[256];
    buffer[0] = wxT('\0');
    if ( wxGetWinVersion() >= wxWinVersion_Vista )
    {
        if ( ::GetLocaleInfo(lcid, LOCALE_SNAME, buffer, WXSIZEOF(buffer)) )
        {
            locale << buffer;
        }
        else
        {
            wxLogLastError(wxT("GetLocaleInfo(LOCALE_SNAME)"));
        }
        return locale;
    }

    if ( !::GetLocaleInfo(lcid, LOCALE_SENGLANGUAGE, buffer, WXSIZEOF(buffer)) )
    {
        wxLogLastError(wxT("GetLocaleInfo(LOCALE_SENGLANGUAGE)"));
        return locale;
    }

    locale << buffer;
    if ( ::GetLocaleInfo(lcid, LOCALE_SENGCOUNTRY,
                        buffer, WXSIZEOF(buffer)) > 0 )
    {
        locale << wxT('_') << buffer;
    }

    const wxString cp = wxGetANSICodePageForLocale(lcid);
    if ( !cp.empty() )
    {
        locale << wxT('.') << cp;
    }

    return locale;
}

#endif // __WINDOWS__

// ----------------------------------------------------------------------------
// wxLocale
// ----------------------------------------------------------------------------

#include "wx/arrimpl.cpp"
WX_DECLARE_EXPORTED_OBJARRAY(wxLanguageInfo, wxLanguageInfoArray);
WX_DEFINE_OBJARRAY(wxLanguageInfoArray)

wxLanguageInfoArray *wxLocale::ms_languagesDB = NULL;

/*static*/ void wxLocale::CreateLanguagesDB()
{
    if (ms_languagesDB == NULL)
    {
        ms_languagesDB = new wxLanguageInfoArray;
        InitLanguagesDB();
    }
}

/*static*/ void wxLocale::DestroyLanguagesDB()
{
    wxDELETE(ms_languagesDB);
}


void wxLocale::DoCommonInit()
{
    // Store the current locale in order to be able to restore it in the dtor.
    m_pszOldLocale = wxSetlocale(LC_ALL, NULL);
    if ( m_pszOldLocale )
        m_pszOldLocale = wxStrdup(m_pszOldLocale);


    m_pOldLocale = wxSetLocale(this);

    // Set translations object, but only if the user didn't do so yet.
    // This is to preserve compatibility with wx-2.8 where wxLocale was
    // the only API for translations. wxLocale works as a stack, with
    // latest-created one being the active one:
    //     wxLocale loc_fr(wxLANGUAGE_FRENCH);
    //     // _() returns French
    //     {
    //         wxLocale loc_cs(wxLANGUAGE_CZECH);
    //         // _() returns Czech
    //     }
    //     // _() returns French again
    wxTranslations *oldTrans = wxTranslations::Get();
    if ( !oldTrans ||
         (m_pOldLocale && oldTrans == &m_pOldLocale->m_translations) )
    {
        wxTranslations::SetNonOwned(&m_translations);
    }

    m_language = wxLANGUAGE_UNKNOWN;
    m_initialized = false;
}

// NB: this function has (desired) side effect of changing current locale
bool wxLocale::Init(const wxString& name,
                    const wxString& shortName,
                    const wxString& locale,
                    bool            bLoadDefault
#if WXWIN_COMPATIBILITY_2_8
                   ,bool            WXUNUSED_UNLESS_DEBUG(bConvertEncoding)
#endif
                    )
{
#if WXWIN_COMPATIBILITY_2_8
    wxASSERT_MSG( bConvertEncoding,
                  wxS("wxLocale::Init with bConvertEncoding=false is no longer supported, add charset to your catalogs") );
#endif

    bool ret = DoInit(name, shortName, locale);

    // NB: don't use 'lang' here, 'language' may be wxLANGUAGE_DEFAULT
    wxTranslations *t = wxTranslations::Get();
    if ( t )
    {
        t->SetLanguage(shortName);

        if ( bLoadDefault )
            t->AddStdCatalog();
    }

    return ret;
}

bool wxLocale::DoInit(const wxString& name,
                      const wxString& shortName,
                      const wxString& locale)
{
    wxASSERT_MSG( !m_initialized,
                    wxS("you can't call wxLocale::Init more than once") );

    m_initialized = true;
    m_strLocale = name;
    m_strShort = shortName;
    m_language = wxLANGUAGE_UNKNOWN;

    // change current locale (default: same as long name)
    wxString szLocale(locale);
    if ( szLocale.empty() )
    {
        // the argument to setlocale()
        szLocale = shortName;

        wxCHECK_MSG( !szLocale.empty(), false,
                    wxS("no locale to set in wxLocale::Init()") );
    }

    if ( !wxSetlocale(LC_ALL, szLocale) )
    {
        wxLogError(_("locale '%s' cannot be set."), szLocale);
    }

    // the short name will be used to look for catalog files as well,
    // so we need something here
    if ( m_strShort.empty() ) {
        // FIXME I don't know how these 2 letter abbreviations are formed,
        //       this wild guess is surely wrong
        if ( !szLocale.empty() )
        {
            m_strShort += (wxChar)wxTolower(szLocale[0]);
            if ( szLocale.length() > 1 )
                m_strShort += (wxChar)wxTolower(szLocale[1]);
        }
    }

    return true;
}


#if defined(__UNIX__) && wxUSE_UNICODE && !defined(__WXMAC__)
static const char *wxSetlocaleTryUTF8(int c, const wxString& lc)
{
    const char *l = NULL;

    // NB: We prefer to set UTF-8 locale if it's possible and only fall back to
    //     non-UTF-8 locale if it fails

    if ( !lc.empty() )
    {
        wxString buf(lc);
        wxString buf2;
        buf2 = buf + wxS(".UTF-8");
        l = wxSetlocale(c, buf2);
        if ( !l )
        {
            buf2 = buf + wxS(".utf-8");
            l = wxSetlocale(c, buf2);
        }
        if ( !l )
        {
            buf2 = buf + wxS(".UTF8");
            l = wxSetlocale(c, buf2);
        }
        if ( !l )
        {
            buf2 = buf + wxS(".utf8");
            l = wxSetlocale(c, buf2);
        }
    }

    // if we can't set UTF-8 locale, try non-UTF-8 one:
    if ( !l )
        l = wxSetlocale(c, lc);

    return l;
}
#else
#define wxSetlocaleTryUTF8(c, lc)  wxSetlocale(c, lc)
#endif

bool wxLocale::Init(int language, int flags)
{
#if WXWIN_COMPATIBILITY_2_8
    wxASSERT_MSG( !(flags & wxLOCALE_CONV_ENCODING),
                  wxS("wxLOCALE_CONV_ENCODING is no longer supported, add charset to your catalogs") );
#endif

    bool ret = true;

    int lang = language;
    if (lang == wxLANGUAGE_DEFAULT)
    {
        // auto detect the language
        lang = GetSystemLanguage();
    }

    // We failed to detect system language, so we will use English:
    if (lang == wxLANGUAGE_UNKNOWN)
    {
        return false;
    }

    const wxLanguageInfo *info = GetLanguageInfo(lang);

    // Unknown language:
    if (info == NULL)
    {
        wxLogError(wxS("Unknown language %i."), lang);
        return false;
    }

    wxString name = info->Description;
    wxString canonical = info->CanonicalName;
    wxString locale;

    // Set the locale:
#if defined(__OS2__)
    const char *retloc = wxSetlocale(LC_ALL , wxEmptyString);
#elif defined(__UNIX__) && !defined(__WXMAC__)
    if (language != wxLANGUAGE_DEFAULT)
        locale = info->CanonicalName;

    const char *retloc = wxSetlocaleTryUTF8(LC_ALL, locale);

    const wxString langOnly = ExtractLang(locale);
    if ( !retloc )
    {
        // Some C libraries don't like xx_YY form and require xx only
        retloc = wxSetlocaleTryUTF8(LC_ALL, langOnly);
    }

#if wxUSE_FONTMAP
    // some systems (e.g. FreeBSD and HP-UX) don't have xx_YY aliases but
    // require the full xx_YY.encoding form, so try using UTF-8 because this is
    // the only thing we can do generically
    //
    // TODO: add encodings applicable to each language to the lang DB and try
    //       them all in turn here
    if ( !retloc )
    {
        const wxChar **names =
            wxFontMapperBase::GetAllEncodingNames(wxFONTENCODING_UTF8);
        while ( *names )
        {
            retloc = wxSetlocale(LC_ALL, locale + wxS('.') + *names++);
            if ( retloc )
                break;
        }
    }
#endif // wxUSE_FONTMAP

    if ( !retloc )
    {
        // Some C libraries (namely glibc) still use old ISO 639,
        // so will translate the abbrev for them
        wxString localeAlt;
        if ( langOnly == wxS("he") )
            localeAlt = wxS("iw") + ExtractNotLang(locale);
        else if ( langOnly == wxS("id") )
            localeAlt = wxS("in") + ExtractNotLang(locale);
        else if ( langOnly == wxS("yi") )
            localeAlt = wxS("ji") + ExtractNotLang(locale);
        else if ( langOnly == wxS("nb") )
            localeAlt = wxS("no_NO");
        else if ( langOnly == wxS("nn") )
            localeAlt = wxS("no_NY");

        if ( !localeAlt.empty() )
        {
            retloc = wxSetlocaleTryUTF8(LC_ALL, localeAlt);
            if ( !retloc )
                retloc = wxSetlocaleTryUTF8(LC_ALL, ExtractLang(localeAlt));
        }
    }

    if ( !retloc )
        ret = false;

#ifdef __AIX__
    // at least in AIX 5.2 libc is buggy and the string returned from
    // setlocale(LC_ALL) can't be passed back to it because it returns 6
    // strings (one for each locale category), i.e. for C locale we get back
    // "C C C C C C"
    //
    // this contradicts IBM own docs but this is not of much help, so just work
    // around it in the crudest possible manner
    char* p = const_cast<char*>(wxStrchr(retloc, ' '));
    if ( p )
        *p = '\0';
#endif // __AIX__

#elif defined(__WIN32__)
    const char *retloc = "C";
    if ( language != wxLANGUAGE_DEFAULT )
    {
        if ( info->WinLang == 0 )
        {
            wxLogWarning(wxS("Locale '%s' not supported by OS."), name.c_str());
            // retloc already set to "C"
        }
        else // language supported by Windows
        {
            // Windows CE doesn't have SetThreadLocale() and there doesn't seem
            // to be any equivalent
#ifndef __WXWINCE__
            const wxUint32 lcid = info->GetLCID();

            // change locale used by Windows functions
            ::SetThreadLocale(lcid);

            // SetThreadUILanguage() may be available on XP, but with unclear
            // behavior, so avoid calling it there.
            if ( wxGetWinVersion() >= wxWinVersion_Vista )
            {
                wxLoadedDLL dllKernel32(wxS("kernel32.dll"));
                typedef LANGID(WINAPI *SetThreadUILanguage_t)(LANGID);
                SetThreadUILanguage_t pfnSetThreadUILanguage = NULL;
                wxDL_INIT_FUNC(pfn, SetThreadUILanguage, dllKernel32);
                if (pfnSetThreadUILanguage)
                    pfnSetThreadUILanguage(LANGIDFROMLCID(lcid));
            }
#endif

            // and also call setlocale() to change locale used by the CRT
            locale = info->GetLocaleName();
            if ( locale.empty() )
            {
                ret = false;
            }
            else // have a valid locale
            {
                retloc = wxSetlocale(LC_ALL, locale);
            }
        }
    }
    else // language == wxLANGUAGE_DEFAULT
    {
        retloc = wxSetlocale(LC_ALL, wxEmptyString);
    }

#if wxUSE_UNICODE && (defined(__VISUALC__) || defined(__MINGW32__))
    // VC++ setlocale() (also used by Mingw) can't set locale to languages that
    // can only be written using Unicode, therefore wxSetlocale() call fails
    // for such languages but we don't want to report it as an error -- so that
    // at least message catalogs can be used.
    if ( !retloc )
    {
        if ( wxGetANSICodePageForLocale(LOCALE_USER_DEFAULT).empty() )
        {
            // we set the locale to a Unicode-only language, don't treat the
            // inability of CRT to use it as an error
            retloc = "C";
        }
    }
#endif // CRT not handling Unicode-only languages

    if ( !retloc )
        ret = false;
#elif defined(__WXMAC__)
    if (lang == wxLANGUAGE_DEFAULT)
        locale = wxEmptyString;
    else
        locale = info->CanonicalName;

    const char *retloc = wxSetlocale(LC_ALL, locale);

    if ( !retloc )
    {
        // Some C libraries don't like xx_YY form and require xx only
        retloc = wxSetlocale(LC_ALL, ExtractLang(locale));
    }
#else
    wxUnusedVar(flags);
    return false;
    #define WX_NO_LOCALE_SUPPORT
#endif

#ifndef WX_NO_LOCALE_SUPPORT
    if ( !ret )
    {
        wxLogWarning(_("Cannot set locale to language \"%s\"."), name.c_str());

        // As we failed to change locale, there is no need to restore the
        // previous one: it's still valid.
        free(const_cast<char *>(m_pszOldLocale));
        m_pszOldLocale = NULL;

        // continue nevertheless and try to load at least the translations for
        // this language
    }

    if ( !DoInit(name, canonical, retloc) )
    {
        ret = false;
    }

    if (IsOk()) // setlocale() succeeded
        m_language = lang;

    // NB: don't use 'lang' here, 'language'
    wxTranslations *t = wxTranslations::Get();
    if ( t )
    {
        t->SetLanguage(static_cast<wxLanguage>(language));

        if ( flags & wxLOCALE_LOAD_DEFAULT )
            t->AddStdCatalog();
    }

    return ret;
#endif // !WX_NO_LOCALE_SUPPORT
}

namespace
{

#ifndef __WXOSX__
// Small helper function: get the value of the given environment variable and
// return true only if the variable was found and has non-empty value.
inline bool wxGetNonEmptyEnvVar(const wxString& name, wxString* value)
{
    return wxGetEnv(name, value) && !value->empty();
}
#endif

} // anonymous namespace

/*static*/ int wxLocale::GetSystemLanguage()
{
    CreateLanguagesDB();

    // init i to avoid compiler warning
    size_t i = 0,
        count = ms_languagesDB->GetCount();

#if defined(__UNIX__)
    // first get the string identifying the language from the environment
    wxString langFull;
#ifdef __WXOSX__
    wxCFRef<CFLocaleRef> userLocaleRef(CFLocaleCopyCurrent());

    // because the locale identifier (kCFLocaleIdentifier) is formatted a little bit differently, eg
    // az_Cyrl_AZ@calendar=buddhist;currency=JPY we just recreate the base info as expected by wx here

    wxCFStringRef str(wxCFRetain((CFStringRef)CFLocaleGetValue(userLocaleRef, kCFLocaleLanguageCode)));
    langFull = str.AsString()+"_";
    str.reset(wxCFRetain((CFStringRef)CFLocaleGetValue(userLocaleRef, kCFLocaleCountryCode)));
    langFull += str.AsString();
#else
    if (!wxGetNonEmptyEnvVar(wxS("LC_ALL"), &langFull) &&
        !wxGetNonEmptyEnvVar(wxS("LC_MESSAGES"), &langFull) &&
        !wxGetNonEmptyEnvVar(wxS("LANG"), &langFull))
    {
        // no language specified, treat it as English
        return wxLANGUAGE_ENGLISH_US;
    }

#endif

    // the language string has the following form
    //
    //      lang[_LANG][.encoding][@modifier]
    //
    // (see environ(5) in the Open Unix specification)
    //
    // where lang is the primary language, LANG is a sublang/territory,
    // encoding is the charset to use and modifier "allows the user to select
    // a specific instance of localization data within a single category"
    //
    // for example, the following strings are valid:
    //      fr
    //      fr_FR
    //      de_DE.iso88591
    //      de_DE@euro
    //      de_DE.iso88591@euro

    // for now we don't use the encoding, although we probably should (doing
    // translations of the msg catalogs on the fly as required) (TODO)
    //
    // we need the modified for languages like Valencian: ca_ES@valencia
    // though, remember it
    wxString modifier;
    size_t posModifier = langFull.find_first_of(wxS("@"));
    if ( posModifier != wxString::npos )
        modifier = langFull.Mid(posModifier);

    size_t posEndLang = langFull.find_first_of(wxS("@."));
    if ( posEndLang != wxString::npos )
    {
        langFull.Truncate(posEndLang);
    }

    if ( langFull == wxS("C") || langFull == wxS("POSIX") )
    {
        // default C locale is English too
        return wxLANGUAGE_ENGLISH_US;
    }

    // do we have just the language (or sublang too)?
    const bool justLang = langFull.find('_') == wxString::npos;

    // 0. Make sure the lang is according to latest ISO 639
    //    (this is necessary because glibc uses iw and in instead
    //    of he and id respectively).

    // the language itself (second part is the dialect/sublang)
    wxString langOrig = ExtractLang(langFull);

    wxString lang;
    if ( langOrig == wxS("iw"))
        lang = wxS("he");
    else if (langOrig == wxS("in"))
        lang = wxS("id");
    else if (langOrig == wxS("ji"))
        lang = wxS("yi");
    else if (langOrig == wxS("no_NO"))
        lang = wxS("nb_NO");
    else if (langOrig == wxS("no_NY"))
        lang = wxS("nn_NO");
    else if (langOrig == wxS("no"))
        lang = wxS("nb_NO");
    else
        lang = langOrig;

    // did we change it?
    if ( lang != langOrig )
    {
        langFull = lang + ExtractNotLang(langFull);
    }

    // 1. Try to find the language either as is:
    // a) With modifier if set
    if ( !modifier.empty() )
    {
        wxString langFullWithModifier = langFull + modifier;
        for ( i = 0; i < count; i++ )
        {
            if ( ms_languagesDB->Item(i).CanonicalName == langFullWithModifier )
                break;
        }
    }

    // b) Without modifier
    if ( modifier.empty() || i == count )
    {
        for ( i = 0; i < count; i++ )
        {
            if ( ms_languagesDB->Item(i).CanonicalName == langFull )
                break;
        }
    }

    // 2. If langFull is of the form xx_YY, try to find xx:
    if ( i == count && !justLang )
    {
        for ( i = 0; i < count; i++ )
        {
            if ( ms_languagesDB->Item(i).CanonicalName == lang )
            {
                break;
            }
        }
    }

    // 3. If langFull is of the form xx, try to find any xx_YY record:
    if ( i == count && justLang )
    {
        for ( i = 0; i < count; i++ )
        {
            if ( ExtractLang(ms_languagesDB->Item(i).CanonicalName)
                    == langFull )
            {
                break;
            }
        }
    }


    if ( i == count )
    {
        // In addition to the format above, we also can have full language
        // names in LANG env var - for example, SuSE is known to use
        // LANG="german" - so check for use of non-standard format and try to
        // find the name in verbose description.
        for ( i = 0; i < count; i++ )
        {
            if (ms_languagesDB->Item(i).Description.CmpNoCase(langFull) == 0)
            {
                break;
            }
        }
    }
#elif defined(__WIN32__)
    LCID lcid = GetUserDefaultLCID();
    if ( lcid != 0 )
    {
        wxUint32 lang = PRIMARYLANGID(LANGIDFROMLCID(lcid));
        wxUint32 sublang = SUBLANGID(LANGIDFROMLCID(lcid));

        for ( i = 0; i < count; i++ )
        {
            if (ms_languagesDB->Item(i).WinLang == lang &&
                ms_languagesDB->Item(i).WinSublang == sublang)
            {
                break;
            }
        }
    }
    //else: leave wxlang == wxLANGUAGE_UNKNOWN
#endif // Unix/Win32

    if ( i < count )
    {
        // we did find a matching entry, use it
        return ms_languagesDB->Item(i).Language;
    }

    // no info about this language in the database
    return wxLANGUAGE_UNKNOWN;
}

// ----------------------------------------------------------------------------
// encoding stuff
// ----------------------------------------------------------------------------

// this is a bit strange as under Windows we get the encoding name using its
// numeric value and under Unix we do it the other way round, but this just
// reflects the way different systems provide the encoding info

/* static */
wxString wxLocale::GetSystemEncodingName()
{
    wxString encname;

#if defined(__WIN32__) && !defined(__WXMICROWIN__)
    // FIXME: what is the error return value for GetACP()?
    UINT codepage = ::GetACP();
    encname.Printf(wxS("windows-%u"), codepage);
#elif defined(__WXMAC__)
    encname = wxCFStringRef::AsString(
                CFStringGetNameOfEncoding(CFStringGetSystemEncoding())
              );
#elif defined(__UNIX_LIKE__)

#if defined(HAVE_LANGINFO_H) && defined(CODESET)
    // GNU libc provides current character set this way (this conforms
    // to Unix98)
    char *oldLocale = strdup(setlocale(LC_CTYPE, NULL));
    setlocale(LC_CTYPE, "");
    const char *alang = nl_langinfo(CODESET);
    setlocale(LC_CTYPE, oldLocale);
    free(oldLocale);

    if ( alang )
    {
        encname = wxString::FromAscii( alang );
    }
    else // nl_langinfo() failed
#endif // HAVE_LANGINFO_H
    {
        // if we can't get at the character set directly, try to see if it's in
        // the environment variables (in most cases this won't work, but I was
        // out of ideas)
        char *lang = getenv( "LC_ALL");
        char *dot = lang ? strchr(lang, '.') : NULL;
        if (!dot)
        {
            lang = getenv( "LC_CTYPE" );
            if ( lang )
                dot = strchr(lang, '.' );
        }
        if (!dot)
        {
            lang = getenv( "LANG");
            if ( lang )
                dot = strchr(lang, '.');
        }

        if ( dot )
        {
            encname = wxString::FromAscii( dot+1 );
        }
    }
#endif // Win32/Unix

    return encname;
}

/* static */
wxFontEncoding wxLocale::GetSystemEncoding()
{
#if defined(__WIN32__) && !defined(__WXMICROWIN__)
    UINT codepage = ::GetACP();

    // wxWidgets only knows about CP1250-1257, 874, 932, 936, 949, 950
    if ( codepage >= 1250 && codepage <= 1257 )
    {
        return (wxFontEncoding)(wxFONTENCODING_CP1250 + codepage - 1250);
    }

    if ( codepage == 874 )
    {
        return wxFONTENCODING_CP874;
    }

    if ( codepage == 932 )
    {
        return wxFONTENCODING_CP932;
    }

    if ( codepage == 936 )
    {
        return wxFONTENCODING_CP936;
    }

    if ( codepage == 949 )
    {
        return wxFONTENCODING_CP949;
    }

    if ( codepage == 950 )
    {
        return wxFONTENCODING_CP950;
    }
#elif defined(__WXMAC__)
    CFStringEncoding encoding = 0 ;
    encoding = CFStringGetSystemEncoding() ;
    return wxMacGetFontEncFromSystemEnc( encoding ) ;
#elif defined(__UNIX_LIKE__) && wxUSE_FONTMAP
    const wxString encname = GetSystemEncodingName();
    if ( !encname.empty() )
    {
        wxFontEncoding enc = wxFontMapperBase::GetEncodingFromName(encname);

        // on some modern Linux systems (RedHat 8) the default system locale
        // is UTF8 -- but it isn't supported by wxGTK1 in ANSI build at all so
        // don't even try to use it in this case
#if !wxUSE_UNICODE && \
        ((defined(__WXGTK__) && !defined(__WXGTK20__)) || defined(__WXMOTIF__))
        if ( enc == wxFONTENCODING_UTF8 )
        {
            // the most similar supported encoding...
            enc = wxFONTENCODING_ISO8859_1;
        }
#endif // !wxUSE_UNICODE

        // GetEncodingFromName() returns wxFONTENCODING_DEFAULT for C locale
        // (a.k.a. US-ASCII) which is arguably a bug but keep it like this for
        // backwards compatibility and just take care to not return
        // wxFONTENCODING_DEFAULT from here as this surely doesn't make sense
        if ( enc == wxFONTENCODING_DEFAULT )
        {
            // we don't have wxFONTENCODING_ASCII, so use the closest one
            return wxFONTENCODING_ISO8859_1;
        }

        if ( enc != wxFONTENCODING_MAX )
        {
            return enc;
        }
        //else: return wxFONTENCODING_SYSTEM below
    }
#endif // Win32/Unix

    return wxFONTENCODING_SYSTEM;
}

/* static */
void wxLocale::AddLanguage(const wxLanguageInfo& info)
{
    CreateLanguagesDB();
    ms_languagesDB->Add(info);
}

/* static */
const wxLanguageInfo *wxLocale::GetLanguageInfo(int lang)
{
    CreateLanguagesDB();

    // calling GetLanguageInfo(wxLANGUAGE_DEFAULT) is a natural thing to do, so
    // make it work
    if ( lang == wxLANGUAGE_DEFAULT )
        lang = GetSystemLanguage();

    const size_t count = ms_languagesDB->GetCount();
    for ( size_t i = 0; i < count; i++ )
    {
        if ( ms_languagesDB->Item(i).Language == lang )
        {
            // We need to create a temporary here in order to make this work with BCC in final build mode
            wxLanguageInfo *ptr = &ms_languagesDB->Item(i);
            return ptr;
        }
    }

    return NULL;
}

/* static */
wxString wxLocale::GetLanguageName(int lang)
{
    if ( lang == wxLANGUAGE_DEFAULT || lang == wxLANGUAGE_UNKNOWN )
        return wxEmptyString;

    const wxLanguageInfo *info = GetLanguageInfo(lang);
    if ( !info )
        return wxEmptyString;
    else
        return info->Description;
}

/* static */
wxString wxLocale::GetLanguageCanonicalName(int lang)
{
    if ( lang == wxLANGUAGE_DEFAULT || lang == wxLANGUAGE_UNKNOWN )
        return wxEmptyString;

    const wxLanguageInfo *info = GetLanguageInfo(lang);
    if ( !info )
        return wxEmptyString;
    else
        return info->CanonicalName;
}

/* static */
const wxLanguageInfo *wxLocale::FindLanguageInfo(const wxString& locale)
{
    CreateLanguagesDB();

    const wxLanguageInfo *infoRet = NULL;

    const size_t count = ms_languagesDB->GetCount();
    for ( size_t i = 0; i < count; i++ )
    {
        const wxLanguageInfo *info = &ms_languagesDB->Item(i);

        if ( wxStricmp(locale, info->CanonicalName) == 0 ||
                wxStricmp(locale, info->Description) == 0 )
        {
            // exact match, stop searching
            infoRet = info;
            break;
        }

        if ( wxStricmp(locale, info->CanonicalName.BeforeFirst(wxS('_'))) == 0 )
        {
            // a match -- but maybe we'll find an exact one later, so continue
            // looking
            //
            // OTOH, maybe we had already found a language match and in this
            // case don't overwrite it because the entry for the default
            // country always appears first in ms_languagesDB
            if ( !infoRet )
                infoRet = info;
        }
    }

    return infoRet;
}

wxString wxLocale::GetSysName() const
{
    return wxSetlocale(LC_ALL, NULL);
}

// clean up
wxLocale::~wxLocale()
{
    // Restore old translations object.
    // See DoCommonInit() for explanation of why this is needed for backward
    // compatibility.
    if ( wxTranslations::Get() == &m_translations )
    {
        if ( m_pOldLocale )
            wxTranslations::SetNonOwned(&m_pOldLocale->m_translations);
        else
            wxTranslations::Set(NULL);
    }

    // restore old locale pointer
    wxSetLocale(m_pOldLocale);

    if ( m_pszOldLocale )
    {
        wxSetlocale(LC_ALL, m_pszOldLocale);
        free(const_cast<char *>(m_pszOldLocale));
    }
}


// check if the given locale is provided by OS and C run time
/* static */
bool wxLocale::IsAvailable(int lang)
{
    const wxLanguageInfo *info = wxLocale::GetLanguageInfo(lang);
    if ( !info )
    {
        // The language is unknown (this normally only happens when we're
        // passed wxLANGUAGE_DEFAULT), so we can't support it.
        wxASSERT_MSG( lang == wxLANGUAGE_DEFAULT,
                      wxS("No info for a valid language?") );
        return false;
    }

#if defined(__WIN32__)
    if ( !info->WinLang )
        return false;

    if ( !::IsValidLocale(info->GetLCID(), LCID_INSTALLED) )
        return false;

#elif defined(__UNIX__)

    // Test if setting the locale works, then set it back.
    char * const oldLocale = wxStrdupA(setlocale(LC_ALL, NULL));

    // Some platforms don't like xx_YY form and require xx only so test for
    // it too.
    const bool
        available = wxSetlocaleTryUTF8(LC_ALL, info->CanonicalName) ||
                    wxSetlocaleTryUTF8(LC_ALL, ExtractLang(info->CanonicalName));

    // restore the original locale
    wxSetlocale(LC_ALL, oldLocale);

    free(oldLocale);

    if ( !available )
        return false;
#endif

    return true;
}


bool wxLocale::AddCatalog(const wxString& domain)
{
    wxTranslations *t = wxTranslations::Get();
    if ( !t )
        return false;
    return t->AddCatalog(domain);
}

bool wxLocale::AddCatalog(const wxString& domain, wxLanguage msgIdLanguage)
{
    wxTranslations *t = wxTranslations::Get();
    if ( !t )
        return false;
    return t->AddCatalog(domain, msgIdLanguage);
}

// add a catalog to our linked list
bool wxLocale::AddCatalog(const wxString& szDomain,
                        wxLanguage      msgIdLanguage,
                        const wxString& msgIdCharset)
{
    wxTranslations *t = wxTranslations::Get();
    if ( !t )
        return false;
#if wxUSE_UNICODE
    wxUnusedVar(msgIdCharset);
    return t->AddCatalog(szDomain, msgIdLanguage);
#else
    return t->AddCatalog(szDomain, msgIdLanguage, msgIdCharset);
#endif
}

bool wxLocale::IsLoaded(const wxString& domain) const
{
    wxTranslations *t = wxTranslations::Get();
    if ( !t )
        return false;
    return t->IsLoaded(domain);
}

wxString wxLocale::GetHeaderValue(const wxString& header,
                                  const wxString& domain) const
{
    wxTranslations *t = wxTranslations::Get();
    if ( !t )
        return wxEmptyString;
    return t->GetHeaderValue(header, domain);
}

// ----------------------------------------------------------------------------
// accessors for locale-dependent data
// ----------------------------------------------------------------------------

#if defined(__WINDOWS__) || defined(__WXOSX__)

namespace
{

// This function translates from Unicode date formats described at
//
//      http://unicode.org/reports/tr35/tr35-6.html#Date_Format_Patterns
//
// to strftime()-like syntax. This translation is not lossless but we try to do
// our best.

static wxString TranslateFromUnicodeFormat(const wxString& fmt)
{
    wxString fmtWX;
    fmtWX.reserve(fmt.length());

    char chLast = '\0';
    size_t lastCount = 0;

    const char* formatchars =
        "dghHmMsSy"
#ifdef __WINDOWS__
        "t"
#else
        "EawD"
#endif
        ;
    for ( wxString::const_iterator p = fmt.begin(); /* end handled inside */; ++p )
    {
        if ( p != fmt.end() )
        {
            if ( *p == chLast )
            {
                lastCount++;
                continue;
            }

            const wxUniChar ch = (*p).GetValue();
            if ( ch.IsAscii() && strchr(formatchars, ch) )
            {
                // these characters come in groups, start counting them
                chLast = ch;
                lastCount = 1;
                continue;
            }
        }

        // interpret any special characters we collected so far
        if ( lastCount )
        {
            switch ( chLast )
            {
                case 'd':
                    switch ( lastCount )
                    {
                        case 1: // d
                        case 2: // dd
                            // these two are the same as we don't distinguish
                            // between 1 and 2 digits for days
                            fmtWX += "%d";
                            break;
#ifdef __WINDOWS__
                        case 3: // ddd
                            fmtWX += "%a";
                            break;

                        case 4: // dddd
                            fmtWX += "%A";
                            break;
#endif
                        default:
                            wxFAIL_MSG( "too many 'd's" );
                    }
                    break;
#ifndef __WINDOWS__
                case 'D':
                    switch ( lastCount )
                    {
                        case 1: // D
                        case 2: // DD
                        case 3: // DDD
                            fmtWX += "%j";
                            break;

                        default:
                            wxFAIL_MSG( "wrong number of 'D's" );
                    }
                    break;
               case 'w':
                    switch ( lastCount )
                    {
                        case 1: // w
                        case 2: // ww
                            fmtWX += "%W";
                            break;

                        default:
                            wxFAIL_MSG( "wrong number of 'w's" );
                    }
                    break;
                case 'E':
                   switch ( lastCount )
                    {
                        case 1: // E
                        case 2: // EE
                        case 3: // EEE
                            fmtWX += "%a";
                            break;
                        case 4: // EEEE
                            fmtWX += "%A";
                            break;
                        case 5: // EEEEE
                        case 6: // EEEEEE
                            // no "narrow form" in strftime(), use abbrev.
                            fmtWX += "%a";
                            break;

                        default:
                            wxFAIL_MSG( "wrong number of 'E's" );
                    }
                    break;
#endif
                case 'M':
                    switch ( lastCount )
                    {
                        case 1: // M
                        case 2: // MM
                            // as for 'd' and 'dd' above
                            fmtWX += "%m";
                            break;

                        case 3:
                            fmtWX += "%b";
                            break;

                        case 4:
                            fmtWX += "%B";
                            break;

                        case 5:
                            // no "narrow form" in strftime(), use abbrev.
                            fmtWX += "%b";
                            break;

                        default:
                            wxFAIL_MSG( "too many 'M's" );
                    }
                    break;

                case 'y':
                    switch ( lastCount )
                    {
                        case 1: // y
                        case 2: // yy
                            fmtWX += "%y";
                            break;

                        case 4: // yyyy
                            fmtWX += "%Y";
                            break;

                        default:
                            wxFAIL_MSG( "wrong number of 'y's" );
                    }
                    break;

                case 'H':
                    switch ( lastCount )
                    {
                        case 1: // H
                        case 2: // HH
                            fmtWX += "%H";
                            break;

                        default:
                            wxFAIL_MSG( "wrong number of 'H's" );
                    }
                    break;

               case 'h':
                    switch ( lastCount )
                    {
                        case 1: // h
                        case 2: // hh
                            fmtWX += "%I";
                            break;

                        default:
                            wxFAIL_MSG( "wrong number of 'h's" );
                    }
                    break;

               case 'm':
                    switch ( lastCount )
                    {
                        case 1: // m
                        case 2: // mm
                            fmtWX += "%M";
                            break;

                        default:
                            wxFAIL_MSG( "wrong number of 'm's" );
                    }
                    break;

               case 's':
                    switch ( lastCount )
                    {
                        case 1: // s
                        case 2: // ss
                            fmtWX += "%S";
                            break;

                        default:
                            wxFAIL_MSG( "wrong number of 's's" );
                    }
                    break;

                case 'g':
                    // strftime() doesn't have era string,
                    // ignore this format
                    wxASSERT_MSG( lastCount <= 2, "too many 'g's" );

                    break;
#ifndef __WINDOWS__
                case 'a':
                    fmtWX += "%p";
                    break;
#endif
#ifdef __WINDOWS__
                case 't':
                    switch ( lastCount )
                    {
                        case 1: // t
                        case 2: // tt
                            fmtWX += "%p";
                            break;

                        default:
                            wxFAIL_MSG( "too many 't's" );
                    }
                    break;
#endif
                default:
                    wxFAIL_MSG( "unreachable" );
            }

            chLast = '\0';
            lastCount = 0;
        }

        if ( p == fmt.end() )
            break;

        // not a special character so must be just a separator, treat as is
        if ( *p == wxT('%') )
        {
            // this one needs to be escaped
            fmtWX += wxT('%');
        }

        fmtWX += *p;
    }

    return fmtWX;
}

} // anonymous namespace

#endif // __WINDOWS__ || __WXOSX__

#if defined(__WINDOWS__)

namespace
{

LCTYPE GetLCTYPEFormatFromLocalInfo(wxLocaleInfo index)
{
    switch ( index )
    {
        case wxLOCALE_SHORT_DATE_FMT:
            return LOCALE_SSHORTDATE;

        case wxLOCALE_LONG_DATE_FMT:
            return LOCALE_SLONGDATE;

        case wxLOCALE_TIME_FMT:
            return LOCALE_STIMEFORMAT;

        default:
            wxFAIL_MSG( "no matching LCTYPE" );
    }

    return 0;
}

} // anonymous namespace

/* static */
wxString wxLocale::GetInfo(wxLocaleInfo index, wxLocaleCategory cat)
{
    const wxLanguageInfo * const
        info = wxGetLocale() ? GetLanguageInfo(wxGetLocale()->GetLanguage())
                             : NULL;
    if ( !info )
    {
        // wxSetLocale() hadn't been called yet of failed, hence CRT must be
        // using "C" locale -- but check it to detect bugs that would happen if
        // this were not the case.
        wxASSERT_MSG( strcmp(setlocale(LC_ALL, NULL), "C") == 0,
                      wxS("You probably called setlocale() directly instead ")
                      wxS("of using wxLocale and now there is a ")
                      wxS("mismatch between C/C++ and Windows locale.\n")
                      wxS("Things are going to break, please only change ")
                      wxS("locale by creating wxLocale objects to avoid this!") );


        // Return the hard coded values for C locale. This is really the right
        // thing to do as there is no LCID we can use in the code below in this
        // case, even LOCALE_INVARIANT is not quite the same as C locale (the
        // only difference is that it uses %Y instead of %y in the date format
        // but this difference is significant enough).
        switch ( index )
        {
            case wxLOCALE_THOUSANDS_SEP:
                return wxString();

            case wxLOCALE_DECIMAL_POINT:
                return ".";

            case wxLOCALE_SHORT_DATE_FMT:
                return "%m/%d/%y";

            case wxLOCALE_LONG_DATE_FMT:
                return "%A, %B %d, %Y";

            case wxLOCALE_TIME_FMT:
                return "%H:%M:%S";

            case wxLOCALE_DATE_TIME_FMT:
                return "%m/%d/%y %H:%M:%S";

            default:
                wxFAIL_MSG( "unknown wxLocaleInfo" );
        }
    }

    const wxUint32 lcid = info->GetLCID();

    wxString str;

    wxChar buf[256];
    buf[0] = wxT('\0');

    switch ( index )
    {
        case wxLOCALE_THOUSANDS_SEP:
            if ( ::GetLocaleInfo(lcid, LOCALE_STHOUSAND, buf, WXSIZEOF(buf)) )
                str = buf;
            break;

        case wxLOCALE_DECIMAL_POINT:
            if ( ::GetLocaleInfo(lcid,
                                 cat == wxLOCALE_CAT_MONEY
                                     ? LOCALE_SMONDECIMALSEP
                                     : LOCALE_SDECIMAL,
                                 buf,
                                 WXSIZEOF(buf)) )
            {
                str = buf;

                // As we get our decimal point separator from Win32 and not the
                // CRT there is a possibility of mismatch between them and this
                // can easily happen if the user code called setlocale()
                // instead of using wxLocale to change the locale. And this can
                // result in very strange bugs elsewhere in the code as the
                // assumptions that formatted strings do use the decimal
                // separator actually fail, so check for it here.
                wxASSERT_MSG
                (
                    wxString::Format("%.3f", 1.23).find(str) != wxString::npos,
                    "Decimal separator mismatch -- did you use setlocale()?"
                    "If so, use wxLocale to change the locale instead."
                );
            }
            break;

        case wxLOCALE_SHORT_DATE_FMT:
        case wxLOCALE_LONG_DATE_FMT:
        case wxLOCALE_TIME_FMT:
            if ( ::GetLocaleInfo(lcid, GetLCTYPEFormatFromLocalInfo(index),
                                 buf, WXSIZEOF(buf)) )
            {
                return TranslateFromUnicodeFormat(buf);
            }
            break;

        case wxLOCALE_DATE_TIME_FMT:
            // there doesn't seem to be any specific setting for this, so just
            // combine date and time ones
            //
            // we use the short date because this is what "%c" uses by default
            // ("%#c" uses long date but we have no way to specify the
            // alternate representation here)
            {
                const wxString datefmt = GetInfo(wxLOCALE_SHORT_DATE_FMT);
                if ( datefmt.empty() )
                    break;

                const wxString timefmt = GetInfo(wxLOCALE_TIME_FMT);
                if ( timefmt.empty() )
                    break;

                str << datefmt << ' ' << timefmt;
            }
            break;

        default:
            wxFAIL_MSG( "unknown wxLocaleInfo" );
    }

    return str;
}

#elif defined(__WXOSX__)

/* static */
wxString wxLocale::GetInfo(wxLocaleInfo index, wxLocaleCategory WXUNUSED(cat))
{
    CFLocaleRef userLocaleRefRaw;
    if ( wxGetLocale() )
    {
        userLocaleRefRaw = CFLocaleCreate
                        (
                                kCFAllocatorDefault,
                                wxCFStringRef(wxGetLocale()->GetCanonicalName())
                        );
    }
    else // no current locale, use the default one
    {
        userLocaleRefRaw = CFLocaleCopyCurrent();
    }

    wxCFRef<CFLocaleRef> userLocaleRef(userLocaleRefRaw);

    CFStringRef cfstr = 0;
    switch ( index )
    {
        case wxLOCALE_THOUSANDS_SEP:
            cfstr = (CFStringRef) CFLocaleGetValue(userLocaleRef, kCFLocaleGroupingSeparator);
            break;

        case wxLOCALE_DECIMAL_POINT:
            cfstr = (CFStringRef) CFLocaleGetValue(userLocaleRef, kCFLocaleDecimalSeparator);
            break;

        case wxLOCALE_SHORT_DATE_FMT:
        case wxLOCALE_LONG_DATE_FMT:
        case wxLOCALE_DATE_TIME_FMT:
        case wxLOCALE_TIME_FMT:
            {
                CFDateFormatterStyle dateStyle = kCFDateFormatterNoStyle;
                CFDateFormatterStyle timeStyle = kCFDateFormatterNoStyle;
                switch (index )
                {
                    case wxLOCALE_SHORT_DATE_FMT:
                        dateStyle = kCFDateFormatterShortStyle;
                        break;
                    case wxLOCALE_LONG_DATE_FMT:
                        dateStyle = kCFDateFormatterFullStyle;
                        break;
                    case wxLOCALE_DATE_TIME_FMT:
                        dateStyle = kCFDateFormatterFullStyle;
                        timeStyle = kCFDateFormatterMediumStyle;
                        break;
                    case wxLOCALE_TIME_FMT:
                        timeStyle = kCFDateFormatterMediumStyle;
                        break;
                    default:
                        wxFAIL_MSG( "unexpected time locale" );
                        return wxString();
                }
                wxCFRef<CFDateFormatterRef> dateFormatter( CFDateFormatterCreate
                    (NULL, userLocaleRef, dateStyle, timeStyle));
                wxCFStringRef cfs = wxCFRetain( CFDateFormatterGetFormat(dateFormatter ));
                wxString format = TranslateFromUnicodeFormat(cfs.AsString());
                // we always want full years
                format.Replace("%y","%Y");
                return format;
            }
            break;

        default:
            wxFAIL_MSG( "Unknown locale info" );
            return wxString();
    }

    wxCFStringRef str(wxCFRetain(cfstr));
    return str.AsString();
}

#else // !__WINDOWS__ && !__WXOSX__, assume generic POSIX

namespace
{

wxString GetDateFormatFromLangInfo(wxLocaleInfo index)
{
#ifdef HAVE_LANGINFO_H
    // array containing parameters for nl_langinfo() indexes by offset of index
    // from wxLOCALE_SHORT_DATE_FMT
    static const nl_item items[] =
    {
        D_FMT, D_T_FMT, D_T_FMT, T_FMT,
    };

    const int nlidx = index - wxLOCALE_SHORT_DATE_FMT;
    if ( nlidx < 0 || nlidx >= (int)WXSIZEOF(items) )
    {
        wxFAIL_MSG( "logic error in GetInfo() code" );
        return wxString();
    }

    const wxString fmt(nl_langinfo(items[nlidx]));

    // just return the format returned by nl_langinfo() except for long date
    // format which we need to recover from date/time format ourselves (but not
    // if we failed completely)
    if ( fmt.empty() || index != wxLOCALE_LONG_DATE_FMT )
        return fmt;

    // this is not 100% precise but the idea is that a typical date/time format
    // under POSIX systems is a combination of a long date format with time one
    // so we should be able to get just the long date format by removing all
    // time-specific format specifiers
    static const char *timeFmtSpecs = "HIklMpPrRsSTXzZ";
    static const char *timeSep = " :./-";

    wxString fmtDateOnly;
    const wxString::const_iterator end = fmt.end();
    wxString::const_iterator lastSep = end;
    for ( wxString::const_iterator p = fmt.begin(); p != end; ++p )
    {
        if ( strchr(timeSep, *p) )
        {
            if ( lastSep == end )
                lastSep = p;

            // skip it for now, we'll discard it if it's followed by a time
            // specifier later or add it to fmtDateOnly if it is not
            continue;
        }

        if ( *p == '%' &&
                (p + 1 != end) && strchr(timeFmtSpecs, p[1]) )
        {
            // time specified found: skip it and any preceding separators
            ++p;
            lastSep = end;
            continue;
        }

        if ( lastSep != end )
        {
            fmtDateOnly += wxString(lastSep, p);
            lastSep = end;
        }

        fmtDateOnly += *p;
    }

    return fmtDateOnly;
#else // !HAVE_LANGINFO_H
    wxUnusedVar(index);

    // no fallback, let the application deal with unavailability of
    // nl_langinfo() itself as there is no good way for us to do it (well, we
    // could try to reverse engineer the format from strftime() output but this
    // looks like too much trouble considering the relatively small number of
    // systems without nl_langinfo() still in use)
    return wxString();
#endif // HAVE_LANGINFO_H/!HAVE_LANGINFO_H
}

} // anonymous namespace

/* static */
wxString wxLocale::GetInfo(wxLocaleInfo index, wxLocaleCategory cat)
{
    lconv * const lc = localeconv();
    if ( !lc )
        return wxString();

    switch ( index )
    {
        case wxLOCALE_THOUSANDS_SEP:
            if ( cat == wxLOCALE_CAT_NUMBER )
                return lc->thousands_sep;
            else if ( cat == wxLOCALE_CAT_MONEY )
                return lc->mon_thousands_sep;

            wxFAIL_MSG( "invalid wxLocaleCategory" );
            break;


        case wxLOCALE_DECIMAL_POINT:
            if ( cat == wxLOCALE_CAT_NUMBER )
                return lc->decimal_point;
            else if ( cat == wxLOCALE_CAT_MONEY )
                return lc->mon_decimal_point;

            wxFAIL_MSG( "invalid wxLocaleCategory" );
            break;

        case wxLOCALE_SHORT_DATE_FMT:
        case wxLOCALE_LONG_DATE_FMT:
        case wxLOCALE_DATE_TIME_FMT:
        case wxLOCALE_TIME_FMT:
            if ( cat != wxLOCALE_CAT_DATE && cat != wxLOCALE_CAT_DEFAULT )
            {
                wxFAIL_MSG( "invalid wxLocaleCategory" );
                break;
            }

            return GetDateFormatFromLangInfo(index);


        default:
            wxFAIL_MSG( "unknown wxLocaleInfo value" );
    }

    return wxString();
}

#endif // platform

// ----------------------------------------------------------------------------
// global functions and variables
// ----------------------------------------------------------------------------

// retrieve/change current locale
// ------------------------------

// the current locale object
static wxLocale *g_pLocale = NULL;

wxLocale *wxGetLocale()
{
    return g_pLocale;
}

wxLocale *wxSetLocale(wxLocale *pLocale)
{
    wxLocale *pOld = g_pLocale;
    g_pLocale = pLocale;
    return pOld;
}



// ----------------------------------------------------------------------------
// wxLocale module (for lazy destruction of languagesDB)
// ----------------------------------------------------------------------------

class wxLocaleModule: public wxModule
{
    DECLARE_DYNAMIC_CLASS(wxLocaleModule)
    public:
        wxLocaleModule() {}

        bool OnInit()
        {
            return true;
        }

        void OnExit()
        {
            wxLocale::DestroyLanguagesDB();
        }
};

IMPLEMENT_DYNAMIC_CLASS(wxLocaleModule, wxModule)

#endif // wxUSE_INTL
