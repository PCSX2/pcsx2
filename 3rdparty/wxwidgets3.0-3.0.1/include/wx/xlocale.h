//////////////////////////////////////////////////////////////////////////////
// Name:        wx/xlocale.h
// Purpose:     Header to provide some xlocale wrappers
// Author:      Brian Vanderburg II, Vadim Zeitlin
// Created:     2008-01-07
// Copyright:   (c) 2008 Brian Vanderburg II
//                  2008 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

/*
    This header defines portable wrappers around xlocale foo_l() functions or
    their MSVC proprietary _foo_l() equivalents when they are available and
    implements these functions for the "C" locale [only] if they are not. This
    allows the program running under the default user locale to still use "C"
    locale for operations such as reading data from files where they are stored
    using decimal point &c.

    TODO: Currently only the character classification and transformation
          functions and number <-> string functions, are implemented,
          we also need at least
            - formatted IO: scanf_l(), printf_l() &c
            - time: strftime_l(), strptime_l()
 */

#ifndef _WX_XLOCALE_H_
#define _WX_XLOCALE_H_

#include "wx/defs.h"    // wxUSE_XLOCALE

#if wxUSE_XLOCALE

#include "wx/crt.h"     // Includes wx/chartype.h, wx/wxcrt.h(wx/string.h)
#include "wx/intl.h"    // wxLanguage

// The platform-specific locale type
// If wxXLocale_t is not defined, then only "C" locale support is provided
#ifdef wxHAS_XLOCALE_SUPPORT
    #if wxCHECK_VISUALC_VERSION(8) && !defined(__WXWINCE__)
        typedef _locale_t wxXLocale_t;
        #define wxXLOCALE_IDENT(name) _ ## name
    #elif defined(HAVE_LOCALE_T)
        #include <locale.h>
        #include <xlocale.h>
        #include <ctype.h>
        #include <stdlib.h>

        #if wxUSE_UNICODE
            #include <wctype.h>
        #endif

        // Locale type and identifier name
        typedef locale_t wxXLocale_t;

        #define wxXLOCALE_IDENT(name) name
    #else
        #error "Unknown xlocale support"
    #endif
#endif // wxHAS_XLOCALE_SUPPORT


// wxXLocale is a wrapper around the native type representing a locale.
//
// It is not to be confused with wxLocale, which handles actually changing the
// locale, loading message catalogs, etc.  This just stores a locale value.
// The similarity of names is unfortunate, but there doesn't seem to be any
// better alternative right now. Perhaps by wxWidgets 4.0 better naming could
// be used, or this class could become wxLocale (a wrapper for the value), and
// some other class could be used to load the language catalogs or something
// that would be clearer
#ifdef wxHAS_XLOCALE_SUPPORT

class WXDLLIMPEXP_BASE wxXLocale
{
public:
    // Construct an uninitialized locale
    wxXLocale() { m_locale = NULL; }

    // Construct from a symbolic language constant
    wxXLocale(wxLanguage lang);

    // Construct from the given language string
    wxXLocale(const char *loc) { Init(loc); }

    // Destroy the locale
    ~wxXLocale() { Free(); }


    // Get the global "C" locale object
    static wxXLocale& GetCLocale();

    // Check if the object represents a valid locale (notice that without
    // wxHAS_XLOCALE_SUPPORT the only valid locale is the "C" one)
    bool IsOk() const { return m_locale != NULL; }

    // Get the type
    wxXLocale_t Get() const { return m_locale; }

    bool operator== (const wxXLocale& loc) const
        { return m_locale == loc.m_locale; }

private:
    // Special ctor for the "C" locale, it's only used internally as the user
    // code is supposed to use GetCLocale()
    wxXLocale(struct wxXLocaleCTag * WXUNUSED(dummy)) { Init("C"); }

    // Create from the given language string (called from ctors)
    void Init(const char *loc);

    // Free the locale if it's non-NULL
    void Free();


    // The corresponding locale handle, NULL if invalid
    wxXLocale_t m_locale;


    // POSIX xlocale API provides a duplocale() function but MSVC locale API
    // doesn't give us any means to copy a _locale_t object so we reduce the
    // functionality to least common denominator here -- it shouldn't be a
    // problem as copying the locale objects shouldn't be often needed
    wxDECLARE_NO_COPY_CLASS(wxXLocale);
};

#else // !wxHAS_XLOCALE_SUPPORT

// Skeleton version supporting only the "C" locale for the systems without
// xlocale support
class WXDLLIMPEXP_BASE wxXLocale
{
public:
    // Construct an uninitialized locale
    wxXLocale() { m_isC = false; }

    // Construct from a symbolic language constant: unless the language is
    // wxLANGUAGE_ENGLISH_US (which we suppose to be the same as "C" locale)
    // the object will be invalid
    wxXLocale(wxLanguage lang)
    {
        m_isC = lang == wxLANGUAGE_ENGLISH_US;
    }

    // Construct from the given language string: unless the string is "C" or
    // "POSIX" the object will be invalid
    wxXLocale(const char *loc)
    {
        m_isC = loc && (strcmp(loc, "C") == 0 || strcmp(loc, "POSIX") == 0);
    }

    // Default copy ctor, assignment operator and dtor are ok (or would be if
    // we didn't use DECLARE_NO_COPY_CLASS() for consistency with the xlocale
    // version)


    // Get the global "C" locale object
    static wxXLocale& GetCLocale();

    // Check if the object represents a valid locale (notice that without
    // wxHAS_XLOCALE_SUPPORT the only valid locale is the "C" one)
    bool IsOk() const { return m_isC; }

private:
    // Special ctor for the "C" locale, it's only used internally as the user
    // code is supposed to use GetCLocale()
    wxXLocale(struct wxXLocaleCTag * WXUNUSED(dummy)) { m_isC = true; }

    // Without xlocale support this class can only represent "C" locale, if
    // this is false the object is invalid
    bool m_isC;


    // although it's not a problem to copy the objects of this class, we use
    // this macro in this implementation for consistency with the xlocale-based
    // one which can't be copied when using MSVC locale API
    wxDECLARE_NO_COPY_CLASS(wxXLocale);
};

#endif // wxHAS_XLOCALE_SUPPORT/!wxHAS_XLOCALE_SUPPORT


// A shorter synonym for the most commonly used locale object
#define wxCLocale (wxXLocale::GetCLocale())
extern WXDLLIMPEXP_DATA_BASE(wxXLocale) wxNullXLocale;

// Wrappers for various functions:
#ifdef wxHAS_XLOCALE_SUPPORT

    // ctype functions
    #define wxCRT_Isalnum_lA wxXLOCALE_IDENT(isalnum_l)
    #define wxCRT_Isalpha_lA wxXLOCALE_IDENT(isalpha_l)
    #define wxCRT_Iscntrl_lA wxXLOCALE_IDENT(iscntrl_l)
    #define wxCRT_Isdigit_lA wxXLOCALE_IDENT(isdigit_l)
    #define wxCRT_Isgraph_lA wxXLOCALE_IDENT(isgraph_l)
    #define wxCRT_Islower_lA wxXLOCALE_IDENT(islower_l)
    #define wxCRT_Isprint_lA wxXLOCALE_IDENT(isprint_l)
    #define wxCRT_Ispunct_lA wxXLOCALE_IDENT(ispunct_l)
    #define wxCRT_Isspace_lA wxXLOCALE_IDENT(isspace_l)
    #define wxCRT_Isupper_lA wxXLOCALE_IDENT(isupper_l)
    #define wxCRT_Isxdigit_lA wxXLOCALE_IDENT(isxdigit_l)
    #define wxCRT_Tolower_lA wxXLOCALE_IDENT(tolower_l)
    #define wxCRT_Toupper_lA wxXLOCALE_IDENT(toupper_l)

    inline int wxIsalnum_l(char c, const wxXLocale& loc)
        { return wxCRT_Isalnum_lA(static_cast<unsigned char>(c), loc.Get()); }
    inline int wxIsalpha_l(char c, const wxXLocale& loc)
        { return wxCRT_Isalpha_lA(static_cast<unsigned char>(c), loc.Get()); }
    inline int wxIscntrl_l(char c, const wxXLocale& loc)
        { return wxCRT_Iscntrl_lA(static_cast<unsigned char>(c), loc.Get()); }
    inline int wxIsdigit_l(char c, const wxXLocale& loc)
        { return wxCRT_Isdigit_lA(static_cast<unsigned char>(c), loc.Get()); }
    inline int wxIsgraph_l(char c, const wxXLocale& loc)
        { return wxCRT_Isgraph_lA(static_cast<unsigned char>(c), loc.Get()); }
    inline int wxIslower_l(char c, const wxXLocale& loc)
        { return wxCRT_Islower_lA(static_cast<unsigned char>(c), loc.Get()); }
    inline int wxIsprint_l(char c, const wxXLocale& loc)
        { return wxCRT_Isprint_lA(static_cast<unsigned char>(c), loc.Get()); }
    inline int wxIspunct_l(char c, const wxXLocale& loc)
        { return wxCRT_Ispunct_lA(static_cast<unsigned char>(c), loc.Get()); }
    inline int wxIsspace_l(char c, const wxXLocale& loc)
        { return wxCRT_Isspace_lA(static_cast<unsigned char>(c), loc.Get()); }
    inline int wxIsupper_l(char c, const wxXLocale& loc)
        { return wxCRT_Isupper_lA(static_cast<unsigned char>(c), loc.Get()); }
    inline int wxIsxdigit_l(char c, const wxXLocale& loc)
        { return wxCRT_Isxdigit_lA(static_cast<unsigned char>(c), loc.Get()); }
    inline int wxTolower_l(char c, const wxXLocale& loc)
        { return wxCRT_Tolower_lA(static_cast<unsigned char>(c), loc.Get()); }
    inline int wxToupper_l(char c, const wxXLocale& loc)
        { return wxCRT_Toupper_lA(static_cast<unsigned char>(c), loc.Get()); }


    // stdlib functions for numeric <-> string conversion
    // NOTE: GNU libc does not have ato[fil]_l functions;
    //       MSVC++8 does not have _strto[u]ll_l functions;
    //       thus we take the minimal set of functions provided in both environments:

    #define wxCRT_Strtod_lA wxXLOCALE_IDENT(strtod_l)
    #define wxCRT_Strtol_lA wxXLOCALE_IDENT(strtol_l)
    #define wxCRT_Strtoul_lA wxXLOCALE_IDENT(strtoul_l)

    inline double wxStrtod_lA(const char *c, char **endptr, const wxXLocale& loc)
        { return wxCRT_Strtod_lA(c, endptr, loc.Get()); }
    inline long wxStrtol_lA(const char *c, char **endptr, int base, const wxXLocale& loc)
        { return wxCRT_Strtol_lA(c, endptr, base, loc.Get()); }
    inline unsigned long wxStrtoul_lA(const char *c, char **endptr, int base, const wxXLocale& loc)
        { return wxCRT_Strtoul_lA(c, endptr, base, loc.Get()); }

    #if wxUSE_UNICODE

        // ctype functions
        #define wxCRT_Isalnum_lW wxXLOCALE_IDENT(iswalnum_l)
        #define wxCRT_Isalpha_lW wxXLOCALE_IDENT(iswalpha_l)
        #define wxCRT_Iscntrl_lW wxXLOCALE_IDENT(iswcntrl_l)
        #define wxCRT_Isdigit_lW wxXLOCALE_IDENT(iswdigit_l)
        #define wxCRT_Isgraph_lW wxXLOCALE_IDENT(iswgraph_l)
        #define wxCRT_Islower_lW wxXLOCALE_IDENT(iswlower_l)
        #define wxCRT_Isprint_lW wxXLOCALE_IDENT(iswprint_l)
        #define wxCRT_Ispunct_lW wxXLOCALE_IDENT(iswpunct_l)
        #define wxCRT_Isspace_lW wxXLOCALE_IDENT(iswspace_l)
        #define wxCRT_Isupper_lW wxXLOCALE_IDENT(iswupper_l)
        #define wxCRT_Isxdigit_lW wxXLOCALE_IDENT(iswxdigit_l)
        #define wxCRT_Tolower_lW wxXLOCALE_IDENT(towlower_l)
        #define wxCRT_Toupper_lW wxXLOCALE_IDENT(towupper_l)

        inline int wxIsalnum_l(wchar_t c, const wxXLocale& loc)
            { return wxCRT_Isalnum_lW(c, loc.Get()); }
        inline int wxIsalpha_l(wchar_t c, const wxXLocale& loc)
            { return wxCRT_Isalpha_lW(c, loc.Get()); }
        inline int wxIscntrl_l(wchar_t c, const wxXLocale& loc)
            { return wxCRT_Iscntrl_lW(c, loc.Get()); }
        inline int wxIsdigit_l(wchar_t c, const wxXLocale& loc)
            { return wxCRT_Isdigit_lW(c, loc.Get()); }
        inline int wxIsgraph_l(wchar_t c, const wxXLocale& loc)
            { return wxCRT_Isgraph_lW(c, loc.Get()); }
        inline int wxIslower_l(wchar_t c, const wxXLocale& loc)
            { return wxCRT_Islower_lW(c, loc.Get()); }
        inline int wxIsprint_l(wchar_t c, const wxXLocale& loc)
            { return wxCRT_Isprint_lW(c, loc.Get()); }
        inline int wxIspunct_l(wchar_t c, const wxXLocale& loc)
            { return wxCRT_Ispunct_lW(c, loc.Get()); }
        inline int wxIsspace_l(wchar_t c, const wxXLocale& loc)
            { return wxCRT_Isspace_lW(c, loc.Get()); }
        inline int wxIsupper_l(wchar_t c, const wxXLocale& loc)
            { return wxCRT_Isupper_lW(c, loc.Get()); }
        inline int wxIsxdigit_l(wchar_t c, const wxXLocale& loc)
            { return wxCRT_Isxdigit_lW(c, loc.Get()); }
        inline wchar_t wxTolower_l(wchar_t c, const wxXLocale& loc)
            { return wxCRT_Tolower_lW(c, loc.Get()); }
        inline wchar_t wxToupper_l(wchar_t c, const wxXLocale& loc)
            { return wxCRT_Toupper_lW(c, loc.Get()); }


        // stdlib functions for numeric <-> string conversion
        // (see notes above about missing functions)
        #define wxCRT_Strtod_lW wxXLOCALE_IDENT(wcstod_l)
        #define wxCRT_Strtol_lW wxXLOCALE_IDENT(wcstol_l)
        #define wxCRT_Strtoul_lW wxXLOCALE_IDENT(wcstoul_l)

        inline double wxStrtod_l(const wchar_t *c, wchar_t **endptr, const wxXLocale& loc)
            { return wxCRT_Strtod_lW(c, endptr, loc.Get()); }
        inline long wxStrtol_l(const wchar_t *c, wchar_t **endptr, int base, const wxXLocale& loc)
            { return wxCRT_Strtol_lW(c, endptr, base, loc.Get()); }
        inline unsigned long wxStrtoul_l(const wchar_t *c, wchar_t **endptr, int base, const wxXLocale& loc)
            { return wxCRT_Strtoul_lW(c, endptr, base, loc.Get()); }
    #else // !wxUSE_UNICODE
        inline double wxStrtod_l(const char *c, char **endptr, const wxXLocale& loc)
            { return wxCRT_Strtod_lA(c, endptr, loc.Get()); }
        inline long wxStrtol_l(const char *c, char **endptr, int base, const wxXLocale& loc)
            { return wxCRT_Strtol_lA(c, endptr, base, loc.Get()); }
        inline unsigned long wxStrtoul_l(const char *c, char **endptr, int base, const wxXLocale& loc)
            { return wxCRT_Strtoul_lA(c, endptr, base, loc.Get()); }
    #endif // wxUSE_UNICODE
#else // !wxHAS_XLOCALE_SUPPORT
    // ctype functions
    int WXDLLIMPEXP_BASE wxIsalnum_l(const wxUniChar& c, const wxXLocale& loc);
    int WXDLLIMPEXP_BASE wxIsalpha_l(const wxUniChar& c, const wxXLocale& loc);
    int WXDLLIMPEXP_BASE wxIscntrl_l(const wxUniChar& c, const wxXLocale& loc);
    int WXDLLIMPEXP_BASE wxIsdigit_l(const wxUniChar& c, const wxXLocale& loc);
    int WXDLLIMPEXP_BASE wxIsgraph_l(const wxUniChar& c, const wxXLocale& loc);
    int WXDLLIMPEXP_BASE wxIslower_l(const wxUniChar& c, const wxXLocale& loc);
    int WXDLLIMPEXP_BASE wxIsprint_l(const wxUniChar& c, const wxXLocale& loc);
    int WXDLLIMPEXP_BASE wxIspunct_l(const wxUniChar& c, const wxXLocale& loc);
    int WXDLLIMPEXP_BASE wxIsspace_l(const wxUniChar& c, const wxXLocale& loc);
    int WXDLLIMPEXP_BASE wxIsupper_l(const wxUniChar& c, const wxXLocale& loc);
    int WXDLLIMPEXP_BASE wxIsxdigit_l(const wxUniChar& c, const wxXLocale& loc);
    int WXDLLIMPEXP_BASE wxTolower_l(const wxUniChar& c, const wxXLocale& loc);
    int WXDLLIMPEXP_BASE wxToupper_l(const wxUniChar& c, const wxXLocale& loc);

    // stdlib functions
    double WXDLLIMPEXP_BASE wxStrtod_l(const wchar_t* str, wchar_t **endptr, const wxXLocale& loc);
    double WXDLLIMPEXP_BASE wxStrtod_l(const char* str, char **endptr, const wxXLocale& loc);
    long WXDLLIMPEXP_BASE wxStrtol_l(const wchar_t* str, wchar_t **endptr, int base, const wxXLocale& loc);
    long WXDLLIMPEXP_BASE wxStrtol_l(const char* str, char **endptr, int base, const wxXLocale& loc);
    unsigned long WXDLLIMPEXP_BASE wxStrtoul_l(const wchar_t* str, wchar_t **endptr, int base, const wxXLocale& loc);
    unsigned long WXDLLIMPEXP_BASE wxStrtoul_l(const char* str, char **endptr, int base, const wxXLocale& loc);

#endif // wxHAS_XLOCALE_SUPPORT/!wxHAS_XLOCALE_SUPPORT

#endif // wxUSE_XLOCALE

#endif // _WX_XLOCALE_H_
