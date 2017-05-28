/////////////////////////////////////////////////////////////////////////////
// Name:        wx/intl.h
// Purpose:     Internationalization and localisation for wxWidgets
// Author:      Vadim Zeitlin
// Modified by: Michael N. Filippov <michael@idisys.iae.nsk.su>
//              (2003/09/30 - plural forms support)
// Created:     29/01/98
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_INTL_H_
#define _WX_INTL_H_

#include "wx/defs.h"
#include "wx/string.h"
#include "wx/translation.h"

// Make wxLayoutDirection enum available without need for wxUSE_INTL so wxWindow, wxApp
// and other classes are not distrubed by wxUSE_INTL

enum wxLayoutDirection
{
    wxLayout_Default,
    wxLayout_LeftToRight,
    wxLayout_RightToLeft
};

#if wxUSE_INTL

#include "wx/fontenc.h"
#include "wx/language.h"

// ============================================================================
// global decls
// ============================================================================

// ----------------------------------------------------------------------------
// macros
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// forward decls
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_FWD_BASE wxLocale;
class WXDLLIMPEXP_FWD_BASE wxLanguageInfoArray;

// ============================================================================
// locale support
// ============================================================================

// ----------------------------------------------------------------------------
// wxLanguageInfo: encapsulates wxLanguage to OS native lang.desc.
//                 translation information
// ----------------------------------------------------------------------------

struct WXDLLIMPEXP_BASE wxLanguageInfo
{
    int Language;                   // wxLanguage id
    wxString CanonicalName;         // Canonical name, e.g. fr_FR
#ifdef __WINDOWS__
    wxUint32 WinLang,               // Win32 language identifiers
             WinSublang;
#endif // __WINDOWS__
    wxString Description;           // human-readable name of the language
    wxLayoutDirection LayoutDirection;

#ifdef __WINDOWS__
    // return the LCID corresponding to this language
    wxUint32 GetLCID() const;
#endif // __WINDOWS__

    // return the locale name corresponding to this language usable with
    // setlocale() on the current system
    wxString GetLocaleName() const;
};

// for Unix systems GetLocaleName() is trivial so implement it inline here, for
// MSW it's implemented in intl.cpp
#ifndef __WINDOWS__
inline wxString wxLanguageInfo::GetLocaleName() const { return CanonicalName; }
#endif // !__WINDOWS__


// ----------------------------------------------------------------------------
// wxLocaleCategory: the category of locale settings
// ----------------------------------------------------------------------------

enum wxLocaleCategory
{
    // (any) numbers
    wxLOCALE_CAT_NUMBER,

    // date/time
    wxLOCALE_CAT_DATE,

    // monetary value
    wxLOCALE_CAT_MONEY,

    // default category for wxLocaleInfo values which only apply to a single
    // category (e.g. wxLOCALE_SHORT_DATE_FMT)
    wxLOCALE_CAT_DEFAULT,

    wxLOCALE_CAT_MAX
};

// ----------------------------------------------------------------------------
// wxLocaleInfo: the items understood by wxLocale::GetInfo()
// ----------------------------------------------------------------------------

enum wxLocaleInfo
{
    // the thousands separator (for wxLOCALE_CAT_NUMBER or MONEY)
    wxLOCALE_THOUSANDS_SEP,

    // the character used as decimal point (for wxLOCALE_CAT_NUMBER or MONEY)
    wxLOCALE_DECIMAL_POINT,

    // the stftime()-formats used for short/long date and time representations
    // (under some platforms short and long date formats are the same)
    //
    // NB: these elements should appear in this order, code in GetInfo() relies
    //     on it
    wxLOCALE_SHORT_DATE_FMT,
    wxLOCALE_LONG_DATE_FMT,
    wxLOCALE_DATE_TIME_FMT,
    wxLOCALE_TIME_FMT

};

// ----------------------------------------------------------------------------
// wxLocale: encapsulates all language dependent settings, including current
//           message catalogs, date, time and currency formats (TODO) &c
// ----------------------------------------------------------------------------

enum wxLocaleInitFlags
{
    wxLOCALE_DONT_LOAD_DEFAULT = 0x0000,     // don't load wxwin.mo
    wxLOCALE_LOAD_DEFAULT      = 0x0001      // load wxwin.mo?
#if WXWIN_COMPATIBILITY_2_8
   ,wxLOCALE_CONV_ENCODING     = 0x0002      // no longer used, simply remove
                                             // it from the existing code
#endif
};

class WXDLLIMPEXP_BASE wxLocale
{
public:
    // ctor & dtor
    // -----------

        // call Init() if you use this ctor
    wxLocale() { DoCommonInit(); }

        // the ctor has a side effect of changing current locale
    wxLocale(const wxString& name,                               // name (for messages)
             const wxString& shortName = wxEmptyString,      // dir prefix (for msg files)
             const wxString& locale = wxEmptyString,     // locale (for setlocale)
             bool bLoadDefault = true                            // preload wxstd.mo?
#if WXWIN_COMPATIBILITY_2_8
             ,bool bConvertEncoding = true                      // convert Win<->Unix if necessary?
#endif
             )
        {
            DoCommonInit();

#if WXWIN_COMPATIBILITY_2_8
            Init(name, shortName, locale, bLoadDefault, bConvertEncoding);
#else
            Init(name, shortName, locale, bLoadDefault);
#endif
        }

    wxLocale(int language, // wxLanguage id or custom language
             int flags = wxLOCALE_LOAD_DEFAULT)
        {
            DoCommonInit();

            Init(language, flags);
        }

        // the same as a function (returns true on success)
    bool Init(const wxString& name,
              const wxString& shortName = wxEmptyString,
              const wxString& locale = wxEmptyString,
              bool bLoadDefault = true
#if WXWIN_COMPATIBILITY_2_8
              ,bool bConvertEncoding = true
#endif
              );

        // same as second ctor (returns true on success)
    bool Init(int language = wxLANGUAGE_DEFAULT,
              int flags = wxLOCALE_LOAD_DEFAULT);

        // restores old locale
    virtual ~wxLocale();

    // Try to get user's (or OS's) preferred language setting.
    // Return wxLANGUAGE_UNKNOWN if language-guessing algorithm failed
    static int GetSystemLanguage();

    // get the encoding used by default for text on this system, returns
    // wxFONTENCODING_SYSTEM if it couldn't be determined
    static wxFontEncoding GetSystemEncoding();

    // get the string describing the system encoding, return empty string if
    // couldn't be determined
    static wxString GetSystemEncodingName();

    // get the values of the given locale-dependent datum: the current locale
    // is used, the US default value is returned if everything else fails
    static wxString GetInfo(wxLocaleInfo index,
                            wxLocaleCategory cat = wxLOCALE_CAT_DEFAULT);

    // Same as GetInfo() but uses current locale at the OS level to retrieve
    // the information. Normally it should be the same as the one used by
    // GetInfo() but there are two exceptions: the most important one is that
    // if no locale had been set, GetInfo() would fall back to "C" locale,
    // while this one uses the default OS locale. Another, more rare, one is
    // that some locales might not supported by the OS.
    //
    // Currently this is the same as GetInfo() under non-MSW platforms.
    static wxString GetOSInfo(wxLocaleInfo index,
                              wxLocaleCategory cat = wxLOCALE_CAT_DEFAULT);

    // return true if the locale was set successfully
    bool IsOk() const { return m_pszOldLocale != NULL; }

    // returns locale name
    const wxString& GetLocale() const { return m_strLocale; }

    // return current locale wxLanguage value
    int GetLanguage() const { return m_language; }

    // return locale name to be passed to setlocale()
    wxString GetSysName() const;

    // return 'canonical' name, i.e. in the form of xx[_YY], where xx is
    // language code according to ISO 639 and YY is country name
    // as specified by ISO 3166.
    wxString GetCanonicalName() const { return m_strShort; }

    // add a prefix to the catalog lookup path: the message catalog files will be
    // looked up under prefix/<lang>/LC_MESSAGES, prefix/LC_MESSAGES and prefix
    // (in this order).
    //
    // This only applies to subsequent invocations of AddCatalog()!
    static void AddCatalogLookupPathPrefix(const wxString& prefix)
        { wxFileTranslationsLoader::AddCatalogLookupPathPrefix(prefix); }

    // add a catalog: it's searched for in standard places (current directory
    // first, system one after), but the you may prepend additional directories to
    // the search path with AddCatalogLookupPathPrefix().
    //
    // The loaded catalog will be used for message lookup by GetString().
    //
    // Returns 'true' if it was successfully loaded
    bool AddCatalog(const wxString& domain);
    bool AddCatalog(const wxString& domain, wxLanguage msgIdLanguage);
    bool AddCatalog(const wxString& domain,
                    wxLanguage msgIdLanguage, const wxString& msgIdCharset);

    // check if the given locale is provided by OS and C run time
    static bool IsAvailable(int lang);

    // check if the given catalog is loaded
    bool IsLoaded(const wxString& domain) const;

    // Retrieve the language info struct for the given language
    //
    // Returns NULL if no info found, pointer must *not* be deleted by caller
    static const wxLanguageInfo *GetLanguageInfo(int lang);

    // Returns language name in English or empty string if the language
    // is not in database
    static wxString GetLanguageName(int lang);

    // Returns ISO code ("canonical name") of language or empty string if the
    // language is not in database
    static wxString GetLanguageCanonicalName(int lang);

    // Find the language for the given locale string which may be either a
    // canonical ISO 2 letter language code ("xx"), a language code followed by
    // the country code ("xx_XX") or a Windows full language name ("Xxxxx...")
    //
    // Returns NULL if no info found, pointer must *not* be deleted by caller
    static const wxLanguageInfo *FindLanguageInfo(const wxString& locale);

    // Add custom language to the list of known languages.
    // Notes: 1) wxLanguageInfo contains platform-specific data
    //        2) must be called before Init to have effect
    static void AddLanguage(const wxLanguageInfo& info);

    // retrieve the translation for a string in all loaded domains unless
    // the szDomain parameter is specified (and then only this domain is
    // searched)
    // n - additional parameter for PluralFormsParser
    //
    // return original string if translation is not available
    // (in this case an error message is generated the first time
    //  a string is not found; use wxLogNull to suppress it)
    //
    // domains are searched in the last to first order, i.e. catalogs
    // added later override those added before.
    const wxString& GetString(const wxString& origString,
                              const wxString& domain = wxEmptyString) const
    {
        return wxGetTranslation(origString, domain);
    }
    // plural form version of the same:
    const wxString& GetString(const wxString& origString,
                              const wxString& origString2,
                              unsigned n,
                              const wxString& domain = wxEmptyString) const
    {
        return wxGetTranslation(origString, origString2, n, domain);
    }

    // Returns the current short name for the locale
    const wxString& GetName() const { return m_strShort; }

    // return the contents of .po file header
    wxString GetHeaderValue(const wxString& header,
                            const wxString& domain = wxEmptyString) const;

    // These two methods are for internal use only. First one creates
    // ms_languagesDB if it doesn't already exist, second one destroys
    // it.
    static void CreateLanguagesDB();
    static void DestroyLanguagesDB();

private:
    bool DoInit(const wxString& name,
                const wxString& shortName,
                const wxString& locale);

    // copy default table of languages from global static array to
    // m_langugagesInfo, called by InitLanguagesDB
    static void InitLanguagesDB();

    // initialize the member fields to default values
    void DoCommonInit();

    wxString       m_strLocale,       // this locale name
                   m_strShort;        // short name for the locale
    int            m_language;        // this locale wxLanguage value

    const char  *m_pszOldLocale;      // previous locale from setlocale()
    wxLocale      *m_pOldLocale;      // previous wxLocale

    bool           m_initialized;

    wxTranslations m_translations;

    static wxLanguageInfoArray *ms_languagesDB;

    wxDECLARE_NO_COPY_CLASS(wxLocale);
};

// ----------------------------------------------------------------------------
// global functions
// ----------------------------------------------------------------------------

// get the current locale object (note that it may be NULL!)
extern WXDLLIMPEXP_BASE wxLocale* wxGetLocale();

#endif // wxUSE_INTL

#endif // _WX_INTL_H_
