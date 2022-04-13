///////////////////////////////////////////////////////////////////////////////
// Name:        wx/wxcrt.h
// Purpose:     Type-safe ANSI and Unicode builds compatible wrappers for
//              CRT functions
// Author:      Joel Farley, Ove Kaaven
// Modified by: Vadim Zeitlin, Robert Roebling, Ron Lee, Vaclav Slavik
// Created:     1998/06/12
// Copyright:   (c) 1998-2006 wxWidgets dev team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_WXCRT_H_
#define _WX_WXCRT_H_

#include "wx/wxcrtbase.h"
#include "wx/string.h"

#ifndef __WX_SETUP_H__
// For non-configure builds assume vsscanf is available, if not Visual C or DMC
#if !defined (__VISUALC__) && !defined (__DMC__)
    #define HAVE_VSSCANF 1
#endif
#endif

// ============================================================================
//                              misc functions
// ============================================================================

/* checks whether the passed in pointer is NULL and if the string is empty */
inline bool wxIsEmpty(const char *s) { return !s || !*s; }
inline bool wxIsEmpty(const wchar_t *s) { return !s || !*s; }
inline bool wxIsEmpty(const wxScopedCharBuffer& s) { return wxIsEmpty(s.data()); }
inline bool wxIsEmpty(const wxScopedWCharBuffer& s) { return wxIsEmpty(s.data()); }
inline bool wxIsEmpty(const wxString& s) { return s.empty(); }
inline bool wxIsEmpty(const wxCStrData& s) { return s.AsString().empty(); }



/* multibyte to wide char conversion functions and macros */

/* multibyte<->widechar conversion */
WXDLLIMPEXP_BASE size_t wxMB2WC(wchar_t *buf, const char *psz, size_t n);
WXDLLIMPEXP_BASE size_t wxWC2MB(char *buf, const wchar_t *psz, size_t n);

#if wxUSE_UNICODE
    #define wxMB2WX wxMB2WC
    #define wxWX2MB wxWC2MB
    #define wxWC2WX wxStrncpy
    #define wxWX2WC wxStrncpy
#else
    #define wxMB2WX wxStrncpy
    #define wxWX2MB wxStrncpy
    #define wxWC2WX wxWC2MB
    #define wxWX2WC wxMB2WC
#endif


//  RN: We could do the usual tricky compiler detection here,
//  and use their variant (such as wmemchr, etc.).  The problem
//  is that these functions are quite rare, even though they are
//  part of the current POSIX standard.  In addition, most compilers
//  (including even MSC) inline them just like we do right in their
//  headers.
//
#include <string.h>

#if wxUSE_UNICODE
    //implement our own wmem variants
    inline wxChar* wxTmemchr(const wxChar* s, wxChar c, size_t l)
    {
        for(;l && *s != c;--l, ++s) {}

        if(l)
            return const_cast<wxChar*>(s);
        return NULL;
    }

    inline int wxTmemcmp(const wxChar* sz1, const wxChar* sz2, size_t len)
    {
        for(; *sz1 == *sz2 && len; --len, ++sz1, ++sz2) {}

        if(len)
            return *sz1 < *sz2 ? -1 : *sz1 > *sz2;
        else
            return 0;
    }

    inline wxChar* wxTmemcpy(wxChar* szOut, const wxChar* szIn, size_t len)
    {
        return (wxChar*) memcpy(szOut, szIn, len * sizeof(wxChar));
    }

    inline wxChar* wxTmemmove(wxChar* szOut, const wxChar* szIn, size_t len)
    {
        return (wxChar*) memmove(szOut, szIn, len * sizeof(wxChar));
    }

    inline wxChar* wxTmemset(wxChar* szOut, const wxChar cIn, size_t len)
    {
        wxChar* szRet = szOut;

        while (len--)
            *szOut++ = cIn;

        return szRet;
    }
#endif /* wxUSE_UNICODE */

// provide trivial wrappers for char* versions for both ANSI and Unicode builds
// (notice that these intentionally return "char *" and not "void *" unlike the
// standard memxxx() for symmetry with the wide char versions):
inline char* wxTmemchr(const char* s, char c, size_t len)
    { return (char*)memchr(s, c, len); }
inline int wxTmemcmp(const char* sz1, const char* sz2, size_t len)
    { return memcmp(sz1, sz2, len); }
inline char* wxTmemcpy(char* szOut, const char* szIn, size_t len)
    { return (char*)memcpy(szOut, szIn, len); }
inline char* wxTmemmove(char* szOut, const char* szIn, size_t len)
    { return (char*)memmove(szOut, szIn, len); }
inline char* wxTmemset(char* szOut, const char cIn, size_t len)
    { return (char*)memset(szOut, cIn, len); }


// ============================================================================
//     wx wrappers for CRT functions in both char* and wchar_t* versions
// ============================================================================

// A few notes on implementation of these wrappers:
//
// We need both char* and wchar_t* versions of functions like wxStrlen() for
// compatibility with both ANSI and Unicode builds.
//
// This makes passing wxString or c_str()/mb_str()/wc_str() result to them
// ambiguous, so we need to provide overrides for that as well (in cases where
// it makes sense).
//
// We can do this without problems for some functions (wxStrlen()), but in some
// cases, we can't stay compatible with both ANSI and Unicode builds, e.g. for
// wxStrcpy(const wxString&), which can only return either char* or wchar_t*.
// In these cases, we preserve ANSI build compatibility by returning char*.

// ----------------------------------------------------------------------------
//                              locale functions
// ----------------------------------------------------------------------------

// NB: we can't provide const wchar_t* (= wxChar*) overload, because calling
//     wxSetlocale(category, NULL) -- which is a common thing to do -- would be
//     ambiguous
WXDLLIMPEXP_BASE char* wxSetlocale(int category, const char *locale);
inline char* wxSetlocale(int category, const wxScopedCharBuffer& locale)
    { return wxSetlocale(category, locale.data()); }
inline char* wxSetlocale(int category, const wxString& locale)
    { return wxSetlocale(category, locale.mb_str()); }
inline char* wxSetlocale(int category, const wxCStrData& locale)
    { return wxSetlocale(category, locale.AsCharBuf()); }

// ----------------------------------------------------------------------------
//                              string functions
// ----------------------------------------------------------------------------

/* safe version of strlen() (returns 0 if passed NULL pointer) */
// NB: these are defined in wxcrtbase.h, see the comment there
// inline size_t wxStrlen(const char *s) { return s ? strlen(s) : 0; }
// inline size_t wxStrlen(const wchar_t *s) { return s ? wxCRT_Strlen_(s) : 0; }
inline size_t wxStrlen(const wxScopedCharBuffer& s) { return wxStrlen(s.data()); }
inline size_t wxStrlen(const wxScopedWCharBuffer& s) { return wxStrlen(s.data()); }
inline size_t wxStrlen(const wxString& s) { return s.length(); }
inline size_t wxStrlen(const wxCStrData& s) { return s.AsString().length(); }

// this is a function new in 2.9 so we don't care about backwards compatibility and
// so don't need to support wxScopedCharBuffer/wxScopedWCharBuffer overloads
#if defined(wxCRT_StrnlenA)
inline size_t wxStrnlen(const char *str, size_t maxlen) { return wxCRT_StrnlenA(str, maxlen); }
#else
inline size_t wxStrnlen(const char *str, size_t maxlen)
{
    size_t n;
    for ( n = 0; n < maxlen; n++ )
        if ( !str[n] )
            break;

    return n;
}
#endif

#if defined(wxCRT_StrnlenW)
inline size_t wxStrnlen(const wchar_t *str, size_t maxlen) { return wxCRT_StrnlenW(str, maxlen); }
#else
inline size_t wxStrnlen(const wchar_t *str, size_t maxlen)
{
    size_t n;
    for ( n = 0; n < maxlen; n++ )
        if ( !str[n] )
            break;

    return n;
}
#endif

// NB: these are defined in wxcrtbase.h, see the comment there
// inline char* wxStrdup(const char *s) { return wxStrdupA(s); }
// inline wchar_t* wxStrdup(const wchar_t *s) { return wxStrdupW(s); }
inline char* wxStrdup(const wxScopedCharBuffer& s) { return wxStrdup(s.data()); }
inline wchar_t* wxStrdup(const wxScopedWCharBuffer& s) { return wxStrdup(s.data()); }
inline char* wxStrdup(const wxString& s) { return wxStrdup(s.mb_str()); }
inline char* wxStrdup(const wxCStrData& s) { return wxStrdup(s.AsCharBuf()); }

inline char *wxStrcpy(char *dest, const char *src)
    { return wxCRT_StrcpyA(dest, src); }
inline wchar_t *wxStrcpy(wchar_t *dest, const wchar_t *src)
    { return wxCRT_StrcpyW(dest, src); }
inline char *wxStrcpy(char *dest, const wxString& src)
    { return wxCRT_StrcpyA(dest, src.mb_str()); }
inline char *wxStrcpy(char *dest, const wxCStrData& src)
    { return wxCRT_StrcpyA(dest, src.AsCharBuf()); }
inline char *wxStrcpy(char *dest, const wxScopedCharBuffer& src)
    { return wxCRT_StrcpyA(dest, src.data()); }
inline wchar_t *wxStrcpy(wchar_t *dest, const wxString& src)
    { return wxCRT_StrcpyW(dest, src.wc_str()); }
inline wchar_t *wxStrcpy(wchar_t *dest, const wxCStrData& src)
    { return wxCRT_StrcpyW(dest, src.AsWCharBuf()); }
inline wchar_t *wxStrcpy(wchar_t *dest, const wxScopedWCharBuffer& src)
    { return wxCRT_StrcpyW(dest, src.data()); }
inline char *wxStrcpy(char *dest, const wchar_t *src)
    { return wxCRT_StrcpyA(dest, wxConvLibc.cWC2MB(src)); }
inline wchar_t *wxStrcpy(wchar_t *dest, const char *src)
    { return wxCRT_StrcpyW(dest, wxConvLibc.cMB2WC(src)); }

inline char *wxStrncpy(char *dest, const char *src, size_t n)
    { return wxCRT_StrncpyA(dest, src, n); }
inline wchar_t *wxStrncpy(wchar_t *dest, const wchar_t *src, size_t n)
    { return wxCRT_StrncpyW(dest, src, n); }
inline char *wxStrncpy(char *dest, const wxString& src, size_t n)
    { return wxCRT_StrncpyA(dest, src.mb_str(), n); }
inline char *wxStrncpy(char *dest, const wxCStrData& src, size_t n)
    { return wxCRT_StrncpyA(dest, src.AsCharBuf(), n); }
inline char *wxStrncpy(char *dest, const wxScopedCharBuffer& src, size_t n)
    { return wxCRT_StrncpyA(dest, src.data(), n); }
inline wchar_t *wxStrncpy(wchar_t *dest, const wxString& src, size_t n)
    { return wxCRT_StrncpyW(dest, src.wc_str(), n); }
inline wchar_t *wxStrncpy(wchar_t *dest, const wxCStrData& src, size_t n)
    { return wxCRT_StrncpyW(dest, src.AsWCharBuf(), n); }
inline wchar_t *wxStrncpy(wchar_t *dest, const wxScopedWCharBuffer& src, size_t n)
    { return wxCRT_StrncpyW(dest, src.data(), n); }
inline char *wxStrncpy(char *dest, const wchar_t *src, size_t n)
    { return wxCRT_StrncpyA(dest, wxConvLibc.cWC2MB(src), n); }
inline wchar_t *wxStrncpy(wchar_t *dest, const char *src, size_t n)
    { return wxCRT_StrncpyW(dest, wxConvLibc.cMB2WC(src), n); }

// this is a function new in 2.9 so we don't care about backwards compatibility and
// so don't need to support wchar_t/char overloads
inline size_t wxStrlcpy(char *dest, const char *src, size_t n)
{
    const size_t len = wxCRT_StrlenA(src);

    if ( n )
    {
        if ( n-- > len )
            n = len;
        wxCRT_StrncpyA(dest, src, n);
        dest[n] = '\0';
    }

    return len;
}
inline size_t wxStrlcpy(wchar_t *dest, const wchar_t *src, size_t n)
{
    const size_t len = wxCRT_StrlenW(src);
    if ( n )
    {
        if ( n-- > len )
            n = len;
        wxCRT_StrncpyW(dest, src, n);
        dest[n] = L'\0';
    }

    return len;
}

inline char *wxStrcat(char *dest, const char *src)
    { return wxCRT_StrcatA(dest, src); }
inline wchar_t *wxStrcat(wchar_t *dest, const wchar_t *src)
    { return wxCRT_StrcatW(dest, src); }
inline char *wxStrcat(char *dest, const wxString& src)
    { return wxCRT_StrcatA(dest, src.mb_str()); }
inline char *wxStrcat(char *dest, const wxCStrData& src)
    { return wxCRT_StrcatA(dest, src.AsCharBuf()); }
inline char *wxStrcat(char *dest, const wxScopedCharBuffer& src)
    { return wxCRT_StrcatA(dest, src.data()); }
inline wchar_t *wxStrcat(wchar_t *dest, const wxString& src)
    { return wxCRT_StrcatW(dest, src.wc_str()); }
inline wchar_t *wxStrcat(wchar_t *dest, const wxCStrData& src)
    { return wxCRT_StrcatW(dest, src.AsWCharBuf()); }
inline wchar_t *wxStrcat(wchar_t *dest, const wxScopedWCharBuffer& src)
    { return wxCRT_StrcatW(dest, src.data()); }
inline char *wxStrcat(char *dest, const wchar_t *src)
    { return wxCRT_StrcatA(dest, wxConvLibc.cWC2MB(src)); }
inline wchar_t *wxStrcat(wchar_t *dest, const char *src)
    { return wxCRT_StrcatW(dest, wxConvLibc.cMB2WC(src)); }

inline char *wxStrncat(char *dest, const char *src, size_t n)
    { return wxCRT_StrncatA(dest, src, n); }
inline wchar_t *wxStrncat(wchar_t *dest, const wchar_t *src, size_t n)
    { return wxCRT_StrncatW(dest, src, n); }
inline char *wxStrncat(char *dest, const wxString& src, size_t n)
    { return wxCRT_StrncatA(dest, src.mb_str(), n); }
inline char *wxStrncat(char *dest, const wxCStrData& src, size_t n)
    { return wxCRT_StrncatA(dest, src.AsCharBuf(), n); }
inline char *wxStrncat(char *dest, const wxScopedCharBuffer& src, size_t n)
    { return wxCRT_StrncatA(dest, src.data(), n); }
inline wchar_t *wxStrncat(wchar_t *dest, const wxString& src, size_t n)
    { return wxCRT_StrncatW(dest, src.wc_str(), n); }
inline wchar_t *wxStrncat(wchar_t *dest, const wxCStrData& src, size_t n)
    { return wxCRT_StrncatW(dest, src.AsWCharBuf(), n); }
inline wchar_t *wxStrncat(wchar_t *dest, const wxScopedWCharBuffer& src, size_t n)
    { return wxCRT_StrncatW(dest, src.data(), n); }
inline char *wxStrncat(char *dest, const wchar_t *src, size_t n)
    { return wxCRT_StrncatA(dest, wxConvLibc.cWC2MB(src), n); }
inline wchar_t *wxStrncat(wchar_t *dest, const char *src, size_t n)
    { return wxCRT_StrncatW(dest, wxConvLibc.cMB2WC(src), n); }


#define WX_STR_DECL(name, T1, T2)  name(T1 s1, T2 s2)
#define WX_STR_CALL(func, a1, a2)  func(a1, a2)

// This macro defines string function for all possible variants of arguments,
// except for those taking wxString or wxCStrData as second argument.
// Parameters:
//   rettype   - return type
//   name      - name of the (overloaded) function to define
//   crtA      - function to call for char* versions (takes two arguments)
//   crtW      - ditto for wchar_t* function
//   forString - function to call when the *first* argument is wxString;
//               the second argument can be any string type, so this is
//               typically a template
#define WX_STR_FUNC_NO_INVERT(rettype, name, crtA, crtW, forString)           \
    inline rettype WX_STR_DECL(name, const char *, const char *)              \
        { return WX_STR_CALL(crtA, s1, s2); }                                 \
    inline rettype WX_STR_DECL(name, const char *, const wchar_t *)           \
        { return WX_STR_CALL(forString, wxString(s1), wxString(s2)); }        \
    inline rettype WX_STR_DECL(name, const char *, const wxScopedCharBuffer&)       \
        { return WX_STR_CALL(crtA, s1, s2.data()); }                          \
    inline rettype WX_STR_DECL(name, const char *, const wxScopedWCharBuffer&)      \
        { return WX_STR_CALL(forString, wxString(s1), s2.data()); }           \
                                                                              \
    inline rettype WX_STR_DECL(name, const wchar_t *, const wchar_t *)        \
        { return WX_STR_CALL(crtW, s1, s2); }                                 \
    inline rettype WX_STR_DECL(name, const wchar_t *, const char *)           \
        { return WX_STR_CALL(forString, wxString(s1), wxString(s2)); }        \
    inline rettype WX_STR_DECL(name, const wchar_t *, const wxScopedWCharBuffer&)   \
        { return WX_STR_CALL(crtW, s1, s2.data()); }                          \
    inline rettype WX_STR_DECL(name, const wchar_t *, const wxScopedCharBuffer&)    \
        { return WX_STR_CALL(forString, wxString(s1), s2.data()); }           \
                                                                              \
    inline rettype WX_STR_DECL(name, const wxScopedCharBuffer&, const char *)       \
        { return WX_STR_CALL(crtA, s1.data(), s2); }                          \
    inline rettype WX_STR_DECL(name, const wxScopedCharBuffer&, const wchar_t *)    \
        { return WX_STR_CALL(forString, wxString(s1), wxString(s2)); }        \
    inline rettype WX_STR_DECL(name, const wxScopedCharBuffer&, const wxScopedCharBuffer&)\
        { return WX_STR_CALL(crtA, s1.data(), s2.data()); }                   \
    inline rettype WX_STR_DECL(name, const wxScopedCharBuffer&, const wxScopedWCharBuffer&)   \
        { return WX_STR_CALL(forString, wxString(s1), wxString(s2)); }        \
                                                                              \
    inline rettype WX_STR_DECL(name, const wxScopedWCharBuffer&, const wchar_t *)   \
        { return WX_STR_CALL(crtW, s1.data(), s2); }                          \
    inline rettype WX_STR_DECL(name, const wxScopedWCharBuffer&, const char *)      \
        { return WX_STR_CALL(forString, wxString(s1), wxString(s2)); }        \
    inline rettype WX_STR_DECL(name, const wxScopedWCharBuffer&, const wxScopedWCharBuffer&)  \
        { return WX_STR_CALL(crtW, s1.data(), s2.data()); }                   \
    inline rettype WX_STR_DECL(name, const wxScopedWCharBuffer&, const wxScopedCharBuffer&)   \
        { return WX_STR_CALL(forString, wxString(s1), wxString(s2)); }        \
                                                                              \
    inline rettype WX_STR_DECL(name, const wxString&, const char*)            \
        { return WX_STR_CALL(forString, s1, s2); }                            \
    inline rettype WX_STR_DECL(name, const wxString&, const wchar_t*)         \
        { return WX_STR_CALL(forString, s1, s2); }                            \
    inline rettype WX_STR_DECL(name, const wxString&, const wxScopedCharBuffer&)    \
        { return WX_STR_CALL(forString, s1, s2); }                            \
    inline rettype WX_STR_DECL(name, const wxString&, const wxScopedWCharBuffer&)   \
        { return WX_STR_CALL(forString, s1, s2); }                            \
    inline rettype WX_STR_DECL(name, const wxString&, const wxString&)        \
        { return WX_STR_CALL(forString, s1, s2); }                            \
    inline rettype WX_STR_DECL(name, const wxString&, const wxCStrData&)      \
        { return WX_STR_CALL(forString, s1, s2); }                            \
                                                                              \
    inline rettype WX_STR_DECL(name, const wxCStrData&, const char*)          \
        { return WX_STR_CALL(forString, s1.AsString(), s2); }                 \
    inline rettype WX_STR_DECL(name, const wxCStrData&, const wchar_t*)       \
        { return WX_STR_CALL(forString, s1.AsString(), s2); }                 \
    inline rettype WX_STR_DECL(name, const wxCStrData&, const wxScopedCharBuffer&)  \
        { return WX_STR_CALL(forString, s1.AsString(), s2); }                 \
    inline rettype WX_STR_DECL(name, const wxCStrData&, const wxScopedWCharBuffer&) \
        { return WX_STR_CALL(forString, s1.AsString(), s2); }                 \
    inline rettype WX_STR_DECL(name, const wxCStrData&, const wxString&)      \
        { return WX_STR_CALL(forString, s1.AsString(), s2); }                 \
    inline rettype WX_STR_DECL(name, const wxCStrData&, const wxCStrData&)    \
        { return WX_STR_CALL(forString, s1.AsString(), s2); }

// This defines strcmp-like function, i.e. one returning the result of
// comparison; see WX_STR_FUNC_NO_INVERT for explanation of the arguments
#define WX_STRCMP_FUNC(name, crtA, crtW, forString)                           \
    WX_STR_FUNC_NO_INVERT(int, name, crtA, crtW, forString)                   \
                                                                              \
    inline int WX_STR_DECL(name, const char *, const wxCStrData&)             \
        { return -WX_STR_CALL(forString, s2.AsString(), s1); }                \
    inline int WX_STR_DECL(name, const char *, const wxString&)               \
        { return -WX_STR_CALL(forString, s2, s1); }                           \
                                                                              \
    inline int WX_STR_DECL(name, const wchar_t *, const wxCStrData&)          \
        { return -WX_STR_CALL(forString, s2.AsString(), s1); }                \
    inline int WX_STR_DECL(name, const wchar_t *, const wxString&)            \
        { return -WX_STR_CALL(forString, s2, s1); }                           \
                                                                              \
    inline int WX_STR_DECL(name, const wxScopedCharBuffer&, const wxCStrData&)      \
        { return -WX_STR_CALL(forString, s2.AsString(), s1.data()); }         \
    inline int WX_STR_DECL(name, const wxScopedCharBuffer&, const wxString&)        \
        { return -WX_STR_CALL(forString, s2, s1.data()); }                    \
                                                                              \
    inline int WX_STR_DECL(name, const wxScopedWCharBuffer&, const wxCStrData&)     \
        { return -WX_STR_CALL(forString, s2.AsString(), s1.data()); }         \
    inline int WX_STR_DECL(name, const wxScopedWCharBuffer&, const wxString&)       \
        { return -WX_STR_CALL(forString, s2, s1.data()); }


// This defines a string function that is *not* strcmp-like, i.e. doesn't
// return the result of comparison and so if the second argument is a string,
// it has to be converted to char* or wchar_t*
#define WX_STR_FUNC(rettype, name, crtA, crtW, forString)                     \
    WX_STR_FUNC_NO_INVERT(rettype, name, crtA, crtW, forString)               \
                                                                              \
    inline rettype WX_STR_DECL(name, const char *, const wxCStrData&)         \
        { return WX_STR_CALL(crtA, s1, s2.AsCharBuf()); }                     \
    inline rettype WX_STR_DECL(name, const char *, const wxString&)           \
        { return WX_STR_CALL(crtA, s1, s2.mb_str()); }                        \
                                                                              \
    inline rettype WX_STR_DECL(name, const wchar_t *, const wxCStrData&)      \
        { return WX_STR_CALL(crtW, s1, s2.AsWCharBuf()); }                    \
    inline rettype WX_STR_DECL(name, const wchar_t *, const wxString&)        \
        { return WX_STR_CALL(crtW, s1, s2.wc_str()); }                        \
                                                                              \
    inline rettype WX_STR_DECL(name, const wxScopedCharBuffer&, const wxCStrData&)  \
        { return WX_STR_CALL(crtA, s1.data(), s2.AsCharBuf()); }              \
    inline rettype WX_STR_DECL(name, const wxScopedCharBuffer&, const wxString&)    \
        { return WX_STR_CALL(crtA, s1.data(), s2.mb_str()); }                 \
                                                                              \
    inline rettype WX_STR_DECL(name, const wxScopedWCharBuffer&, const wxCStrData&) \
        { return WX_STR_CALL(crtW, s1.data(), s2.AsWCharBuf()); }             \
    inline rettype WX_STR_DECL(name, const wxScopedWCharBuffer&, const wxString&)   \
        { return WX_STR_CALL(crtW, s1.data(), s2.wc_str()); }

template<typename T>
inline int wxStrcmp_String(const wxString& s1, const T& s2)
    { return s1.compare(s2); }
WX_STRCMP_FUNC(wxStrcmp, wxCRT_StrcmpA, wxCRT_StrcmpW, wxStrcmp_String)

template<typename T>
inline int wxStricmp_String(const wxString& s1, const T& s2)
    { return s1.CmpNoCase(s2); }
WX_STRCMP_FUNC(wxStricmp, wxCRT_StricmpA, wxCRT_StricmpW, wxStricmp_String)

#if defined(wxCRT_StrcollA) && defined(wxCRT_StrcollW)

// GCC 3.4 and other compilers have a bug that causes it to fail compilation if
// the template's implementation uses overloaded function declared later (see
// the wxStrcoll() call in wxStrcoll_String<T>()), so we have to
// forward-declare the template and implement it below WX_STRCMP_FUNC. OTOH,
// this fails to compile with VC6, so don't do it for VC. It also causes
// problems with GCC visibility in newer GCC versions.
// PCSX2: MSVC in conformance mode also requires declaration before use.
#if (wxCHECK_GCC_VERSION(3,5) && !wxCHECK_GCC_VERSION(4,7)) || defined(__clang__) || defined(__VISUALC__)
    #define wxNEEDS_DECL_BEFORE_TEMPLATE
#endif

#ifdef wxNEEDS_DECL_BEFORE_TEMPLATE
template<typename T>
inline int wxStrcoll_String(const wxString& s1, const T& s2);
WX_STRCMP_FUNC(wxStrcoll, wxCRT_StrcollA, wxCRT_StrcollW, wxStrcoll_String)
#endif // wxNEEDS_DECL_BEFORE_TEMPLATE

template<typename T>
inline int wxStrcoll_String(const wxString& s1, const T& s2)
{
#if wxUSE_UNICODE
    // NB: strcoll() doesn't work correctly on UTF-8 strings, so we have to use
    //     wc_str() even if wxUSE_UNICODE_UTF8; the (const wchar_t*) cast is
    //     there just as optimization to avoid going through
    //     wxStrcoll<wxScopedWCharBuffer>:
    return wxStrcoll((const wchar_t*)s1.wc_str(), s2);
#else
    return wxStrcoll((const char*)s1.mb_str(), s2);
#endif
}

#ifndef wxNEEDS_DECL_BEFORE_TEMPLATE
// this is exactly the same WX_STRCMP_FUNC line as above, insde the
// wxNEEDS_DECL_BEFORE_TEMPLATE case
WX_STRCMP_FUNC(wxStrcoll, wxCRT_StrcollA, wxCRT_StrcollW, wxStrcoll_String)
#endif

#endif // defined(wxCRT_Strcoll[AW])

template<typename T>
inline size_t wxStrspn_String(const wxString& s1, const T& s2)
{
    size_t pos = s1.find_first_not_of(s2);
    return pos == wxString::npos ? s1.length() : pos;
}
WX_STR_FUNC(size_t, wxStrspn, wxCRT_StrspnA, wxCRT_StrspnW, wxStrspn_String)

template<typename T>
inline size_t wxStrcspn_String(const wxString& s1, const T& s2)
{
    size_t pos = s1.find_first_of(s2);
    return pos == wxString::npos ? s1.length() : pos;
}
WX_STR_FUNC(size_t, wxStrcspn, wxCRT_StrcspnA, wxCRT_StrcspnW, wxStrcspn_String)

#undef WX_STR_DECL
#undef WX_STR_CALL
#define WX_STR_DECL(name, T1, T2)  name(T1 s1, T2 s2, size_t n)
#define WX_STR_CALL(func, a1, a2)  func(a1, a2, n)

template<typename T>
inline int wxStrncmp_String(const wxString& s1, const T& s2, size_t n)
    { return s1.compare(0, n, s2, 0, n); }
WX_STRCMP_FUNC(wxStrncmp, wxCRT_StrncmpA, wxCRT_StrncmpW, wxStrncmp_String)

template<typename T>
inline int wxStrnicmp_String(const wxString& s1, const T& s2, size_t n)
    { return s1.substr(0, n).CmpNoCase(wxString(s2).substr(0, n)); }
WX_STRCMP_FUNC(wxStrnicmp, wxCRT_StrnicmpA, wxCRT_StrnicmpW, wxStrnicmp_String)

#undef WX_STR_DECL
#undef WX_STR_CALL
#undef WX_STRCMP_FUNC
#undef WX_STR_FUNC
#undef WX_STR_FUNC_NO_INVERT

#if defined(wxCRT_StrxfrmA) && defined(wxCRT_StrxfrmW)

inline size_t wxStrxfrm(char *dest, const char *src, size_t n)
    { return wxCRT_StrxfrmA(dest, src, n); }
inline size_t wxStrxfrm(wchar_t *dest, const wchar_t *src, size_t n)
    { return wxCRT_StrxfrmW(dest, src, n); }
template<typename T>
inline size_t wxStrxfrm(T *dest, const wxScopedCharTypeBuffer<T>& src, size_t n)
    { return wxStrxfrm(dest, src.data(), n); }
inline size_t wxStrxfrm(char *dest, const wxString& src, size_t n)
    { return wxCRT_StrxfrmA(dest, src.mb_str(), n); }
inline size_t wxStrxfrm(wchar_t *dest, const wxString& src, size_t n)
    { return wxCRT_StrxfrmW(dest, src.wc_str(), n); }
inline size_t wxStrxfrm(char *dest, const wxCStrData& src, size_t n)
    { return wxCRT_StrxfrmA(dest, src.AsCharBuf(), n); }
inline size_t wxStrxfrm(wchar_t *dest, const wxCStrData& src, size_t n)
    { return wxCRT_StrxfrmW(dest, src.AsWCharBuf(), n); }

#endif // defined(wxCRT_Strxfrm[AW])

inline char *wxStrtok(char *str, const char *delim, char **saveptr)
    { return wxCRT_StrtokA(str, delim, saveptr); }
inline wchar_t *wxStrtok(wchar_t *str, const wchar_t *delim, wchar_t **saveptr)
    { return wxCRT_StrtokW(str, delim, saveptr); }
template<typename T>
inline T *wxStrtok(T *str, const wxScopedCharTypeBuffer<T>& delim, T **saveptr)
    { return wxStrtok(str, delim.data(), saveptr); }
inline char *wxStrtok(char *str, const wxCStrData& delim, char **saveptr)
    { return wxCRT_StrtokA(str, delim.AsCharBuf(), saveptr); }
inline wchar_t *wxStrtok(wchar_t *str, const wxCStrData& delim, wchar_t **saveptr)
    { return wxCRT_StrtokW(str, delim.AsWCharBuf(), saveptr); }
inline char *wxStrtok(char *str, const wxString& delim, char **saveptr)
    { return wxCRT_StrtokA(str, delim.mb_str(), saveptr); }
inline wchar_t *wxStrtok(wchar_t *str, const wxString& delim, wchar_t **saveptr)
    { return wxCRT_StrtokW(str, delim.wc_str(), saveptr); }

inline const char *wxStrstr(const char *haystack, const char *needle)
    { return wxCRT_StrstrA(haystack, needle); }
inline const wchar_t *wxStrstr(const wchar_t *haystack, const wchar_t *needle)
    { return wxCRT_StrstrW(haystack, needle); }
inline const char *wxStrstr(const char *haystack, const wxString& needle)
    { return wxCRT_StrstrA(haystack, needle.mb_str()); }
inline const wchar_t *wxStrstr(const wchar_t *haystack, const wxString& needle)
    { return wxCRT_StrstrW(haystack, needle.wc_str()); }
// these functions return char* pointer into the non-temporary conversion buffer
// used by c_str()'s implicit conversion to char*, for ANSI build compatibility
inline const char *wxStrstr(const wxString& haystack, const wxString& needle)
    { return wxCRT_StrstrA(haystack.c_str(), needle.mb_str()); }
inline const char *wxStrstr(const wxCStrData& haystack, const wxString& needle)
    { return wxCRT_StrstrA(haystack, needle.mb_str()); }
inline const char *wxStrstr(const wxCStrData& haystack, const wxCStrData& needle)
    { return wxCRT_StrstrA(haystack, needle.AsCharBuf()); }
// if 'needle' is char/wchar_t, then the same is probably wanted as return value
inline const char *wxStrstr(const wxString& haystack, const char *needle)
    { return wxCRT_StrstrA(haystack.c_str(), needle); }
inline const char *wxStrstr(const wxCStrData& haystack, const char *needle)
    { return wxCRT_StrstrA(haystack, needle); }
inline const wchar_t *wxStrstr(const wxString& haystack, const wchar_t *needle)
    { return wxCRT_StrstrW(haystack.c_str(), needle); }
inline const wchar_t *wxStrstr(const wxCStrData& haystack, const wchar_t *needle)
    { return wxCRT_StrstrW(haystack, needle); }

inline const char *wxStrchr(const char *s, char c)
    { return wxCRT_StrchrA(s, c); }
inline const wchar_t *wxStrchr(const wchar_t *s, wchar_t c)
    { return wxCRT_StrchrW(s, c); }
inline const char *wxStrrchr(const char *s, char c)
    { return wxCRT_StrrchrA(s, c); }
inline const wchar_t *wxStrrchr(const wchar_t *s, wchar_t c)
    { return wxCRT_StrrchrW(s, c); }
inline const char *wxStrchr(const char *s, const wxUniChar& uc)
    { char c; return uc.GetAsChar(&c) ? wxCRT_StrchrA(s, c) : NULL; }
inline const wchar_t *wxStrchr(const wchar_t *s, const wxUniChar& c)
    { return wxCRT_StrchrW(s, (wchar_t)c); }
inline const char *wxStrrchr(const char *s, const wxUniChar& uc)
    { char c; return uc.GetAsChar(&c) ? wxCRT_StrrchrA(s, c) : NULL; }
inline const wchar_t *wxStrrchr(const wchar_t *s, const wxUniChar& c)
    { return wxCRT_StrrchrW(s, (wchar_t)c); }
inline const char *wxStrchr(const char *s, const wxUniCharRef& uc)
    { char c; return uc.GetAsChar(&c) ? wxCRT_StrchrA(s, c) : NULL; }
inline const wchar_t *wxStrchr(const wchar_t *s, const wxUniCharRef& c)
    { return wxCRT_StrchrW(s, (wchar_t)c); }
inline const char *wxStrrchr(const char *s, const wxUniCharRef& uc)
    { char c; return uc.GetAsChar(&c) ? wxCRT_StrrchrA(s, c) : NULL; }
inline const wchar_t *wxStrrchr(const wchar_t *s, const wxUniCharRef& c)
    { return wxCRT_StrrchrW(s, (wchar_t)c); }
template<typename T>
inline const T* wxStrchr(const wxScopedCharTypeBuffer<T>& s, T c)
    { return wxStrchr(s.data(), c); }
template<typename T>
inline const T* wxStrrchr(const wxScopedCharTypeBuffer<T>& s, T c)
    { return wxStrrchr(s.data(), c); }
template<typename T>
inline const T* wxStrchr(const wxScopedCharTypeBuffer<T>& s, const wxUniChar& c)
    { return wxStrchr(s.data(), (T)c); }
template<typename T>
inline const T* wxStrrchr(const wxScopedCharTypeBuffer<T>& s, const wxUniChar& c)
    { return wxStrrchr(s.data(), (T)c); }
template<typename T>
inline const T* wxStrchr(const wxScopedCharTypeBuffer<T>& s, const wxUniCharRef& c)
    { return wxStrchr(s.data(), (T)c); }
template<typename T>
inline const T* wxStrrchr(const wxScopedCharTypeBuffer<T>& s, const wxUniCharRef& c)
    { return wxStrrchr(s.data(), (T)c); }
// these functions return char* pointer into the non-temporary conversion buffer
// used by c_str()'s implicit conversion to char*, for ANSI build compatibility
inline const char* wxStrchr(const wxString& s, char c)
    { return wxCRT_StrchrA((const char*)s.c_str(), c); }
inline const char* wxStrrchr(const wxString& s, char c)
    { return wxCRT_StrrchrA((const char*)s.c_str(), c); }
inline const char* wxStrchr(const wxString& s, int c)
    { return wxCRT_StrchrA((const char*)s.c_str(), c); }
inline const char* wxStrrchr(const wxString& s, int c)
    { return wxCRT_StrrchrA((const char*)s.c_str(), c); }
inline const char* wxStrchr(const wxString& s, const wxUniChar& uc)
    { char c; return uc.GetAsChar(&c) ? wxCRT_StrchrA(s.c_str(), c) : NULL; }
inline const char* wxStrrchr(const wxString& s, const wxUniChar& uc)
    { char c; return uc.GetAsChar(&c) ? wxCRT_StrrchrA(s.c_str(), c) : NULL; }
inline const char* wxStrchr(const wxString& s, const wxUniCharRef& uc)
    { char c; return uc.GetAsChar(&c) ? wxCRT_StrchrA(s.c_str(), c) : NULL; }
inline const char* wxStrrchr(const wxString& s, const wxUniCharRef& uc)
    { char c; return uc.GetAsChar(&c) ? wxCRT_StrrchrA(s.c_str(), c) : NULL; }
inline const wchar_t* wxStrchr(const wxString& s, wchar_t c)
    { return wxCRT_StrchrW((const wchar_t*)s.c_str(), c); }
inline const wchar_t* wxStrrchr(const wxString& s, wchar_t c)
    { return wxCRT_StrrchrW((const wchar_t*)s.c_str(), c); }
inline const char* wxStrchr(const wxCStrData& s, char c)
    { return wxCRT_StrchrA(s.AsChar(), c); }
inline const char* wxStrrchr(const wxCStrData& s, char c)
    { return wxCRT_StrrchrA(s.AsChar(), c); }
inline const char* wxStrchr(const wxCStrData& s, int c)
    { return wxCRT_StrchrA(s.AsChar(), c); }
inline const char* wxStrrchr(const wxCStrData& s, int c)
    { return wxCRT_StrrchrA(s.AsChar(), c); }
inline const char* wxStrchr(const wxCStrData& s, const wxUniChar& uc)
    { char c; return uc.GetAsChar(&c) ? wxCRT_StrchrA(s, c) : NULL; }
inline const char* wxStrrchr(const wxCStrData& s, const wxUniChar& uc)
    { char c; return uc.GetAsChar(&c) ? wxCRT_StrrchrA(s, c) : NULL; }
inline const char* wxStrchr(const wxCStrData& s, const wxUniCharRef& uc)
    { char c; return uc.GetAsChar(&c) ? wxCRT_StrchrA(s, c) : NULL; }
inline const char* wxStrrchr(const wxCStrData& s, const wxUniCharRef& uc)
    { char c; return uc.GetAsChar(&c) ? wxCRT_StrrchrA(s, c) : NULL; }
inline const wchar_t* wxStrchr(const wxCStrData& s, wchar_t c)
    { return wxCRT_StrchrW(s.AsWChar(), c); }
inline const wchar_t* wxStrrchr(const wxCStrData& s, wchar_t c)
    { return wxCRT_StrrchrW(s.AsWChar(), c); }

inline const char *wxStrpbrk(const char *s, const char *accept)
    { return wxCRT_StrpbrkA(s, accept); }
inline const wchar_t *wxStrpbrk(const wchar_t *s, const wchar_t *accept)
    { return wxCRT_StrpbrkW(s, accept); }
inline const char *wxStrpbrk(const char *s, const wxString& accept)
    { return wxCRT_StrpbrkA(s, accept.mb_str()); }
inline const char *wxStrpbrk(const char *s, const wxCStrData& accept)
    { return wxCRT_StrpbrkA(s, accept.AsCharBuf()); }
inline const wchar_t *wxStrpbrk(const wchar_t *s, const wxString& accept)
    { return wxCRT_StrpbrkW(s, accept.wc_str()); }
inline const wchar_t *wxStrpbrk(const wchar_t *s, const wxCStrData& accept)
    { return wxCRT_StrpbrkW(s, accept.AsWCharBuf()); }
inline const char *wxStrpbrk(const wxString& s, const wxString& accept)
    { return wxCRT_StrpbrkA(s.c_str(), accept.mb_str()); }
inline const char *wxStrpbrk(const wxString& s, const char *accept)
    { return wxCRT_StrpbrkA(s.c_str(), accept); }
inline const wchar_t *wxStrpbrk(const wxString& s, const wchar_t *accept)
    { return wxCRT_StrpbrkW(s.wc_str(), accept); }
inline const char *wxStrpbrk(const wxString& s, const wxCStrData& accept)
    { return wxCRT_StrpbrkA(s.c_str(), accept.AsCharBuf()); }
inline const char *wxStrpbrk(const wxCStrData& s, const wxString& accept)
    { return wxCRT_StrpbrkA(s.AsChar(), accept.mb_str()); }
inline const char *wxStrpbrk(const wxCStrData& s, const char *accept)
    { return wxCRT_StrpbrkA(s.AsChar(), accept); }
inline const wchar_t *wxStrpbrk(const wxCStrData& s, const wchar_t *accept)
    { return wxCRT_StrpbrkW(s.AsWChar(), accept); }
inline const char *wxStrpbrk(const wxCStrData& s, const wxCStrData& accept)
    { return wxCRT_StrpbrkA(s.AsChar(), accept.AsCharBuf()); }
template <typename S, typename T>
inline const T *wxStrpbrk(const S& s, const wxScopedCharTypeBuffer<T>& accept)
    { return wxStrpbrk(s, accept.data()); }


/* inlined non-const versions */
template <typename T>
inline char *wxStrstr(char *haystack, T needle)
    { return const_cast<char*>(wxStrstr(const_cast<const char*>(haystack), needle)); }
template <typename T>
inline wchar_t *wxStrstr(wchar_t *haystack, T needle)
    { return const_cast<wchar_t*>(wxStrstr(const_cast<const wchar_t*>(haystack), needle)); }

template <typename T>
inline char * wxStrchr(char *s, T c)
    { return const_cast<char*>(wxStrchr(const_cast<const char*>(s), c)); }
template <typename T>
inline wchar_t * wxStrchr(wchar_t *s, T c)
    { return (wchar_t *)wxStrchr((const wchar_t *)s, c); }
template <typename T>
inline char * wxStrrchr(char *s, T c)
    { return const_cast<char*>(wxStrrchr(const_cast<const char*>(s), c)); }
template <typename T>
inline wchar_t * wxStrrchr(wchar_t *s, T c)
    { return const_cast<wchar_t*>(wxStrrchr(const_cast<const wchar_t*>(s), c)); }

template <typename T>
inline char * wxStrpbrk(char *s, T accept)
    { return const_cast<char*>(wxStrpbrk(const_cast<const char*>(s), accept)); }
template <typename T>
inline wchar_t * wxStrpbrk(wchar_t *s, T accept)
    { return const_cast<wchar_t*>(wxStrpbrk(const_cast<const wchar_t*>(s), accept)); }


// ----------------------------------------------------------------------------
//                              stdio.h functions
// ----------------------------------------------------------------------------

// NB: using fn_str() for mode is a hack to get the same type (char*/wchar_t*)
//     as needed, the conversion itself doesn't matter, it's ASCII
inline FILE *wxFopen(const wxString& path, const wxString& mode)
    { return wxCRT_Fopen(path.fn_str(), mode.fn_str()); }
inline FILE *wxFreopen(const wxString& path, const wxString& mode, FILE *stream)
    { return wxCRT_Freopen(path.fn_str(), mode.fn_str(), stream); }
inline int wxRemove(const wxString& path)
    { return wxCRT_Remove(path.fn_str()); }
inline int wxRename(const wxString& oldpath, const wxString& newpath)
    { return wxCRT_Rename(oldpath.fn_str(), newpath.fn_str()); }

extern WXDLLIMPEXP_BASE int wxPuts(const wxString& s);
extern WXDLLIMPEXP_BASE int wxFputs(const wxString& s, FILE *stream);
extern WXDLLIMPEXP_BASE void wxPerror(const wxString& s);

extern WXDLLIMPEXP_BASE int wxFputc(const wxUniChar& c, FILE *stream);

#define wxPutc(c, stream)  wxFputc(c, stream)
#define wxPutchar(c)       wxFputc(c, stdout)
#define wxFputchar(c)      wxPutchar(c)

// NB: We only provide ANSI version of fgets() because fgetws() interprets the
//     stream according to current locale, which is rarely what is desired.
inline char *wxFgets(char *s, int size, FILE *stream)
    { return wxCRT_FgetsA(s, size, stream); }
// This version calls ANSI version and converts the string using wxConvLibc
extern WXDLLIMPEXP_BASE wchar_t *wxFgets(wchar_t *s, int size, FILE *stream);

#define wxGets(s) wxGets_is_insecure_and_dangerous_use_wxFgets_instead

// NB: We only provide ANSI versions of this for the same reasons as in the
//     case of wxFgets() above
inline int wxFgetc(FILE *stream) { return wxCRT_FgetcA(stream); }
inline int wxUngetc(int c, FILE *stream) { return wxCRT_UngetcA(c, stream); }

#define wxGetc(stream)     wxFgetc(stream)
#define wxGetchar()        wxFgetc(stdin)
#define wxFgetchar()       wxGetchar()

// ----------------------------------------------------------------------------
//                             stdlib.h functions
// ----------------------------------------------------------------------------

#ifdef wxCRT_AtoiW
inline int wxAtoi(const wxString& str) { return wxCRT_AtoiW(str.wc_str()); }
#else
inline int wxAtoi(const wxString& str) { return wxCRT_AtoiA(str.mb_str()); }
#endif

#ifdef wxCRT_AtolW
inline long wxAtol(const wxString& str) { return wxCRT_AtolW(str.wc_str()); }
#else
inline long wxAtol(const wxString& str) { return wxCRT_AtolA(str.mb_str()); }
#endif

#ifdef wxCRT_AtofW
inline double wxAtof(const wxString& str) { return wxCRT_AtofW(str.wc_str()); }
#else
inline double wxAtof(const wxString& str) { return wxCRT_AtofA(str.mb_str()); }
#endif

inline double wxStrtod(const char *nptr, char **endptr)
    { return wxCRT_StrtodA(nptr, endptr); }
inline double wxStrtod(const wchar_t *nptr, wchar_t **endptr)
    { return wxCRT_StrtodW(nptr, endptr); }
template<typename T>
inline double wxStrtod(const wxScopedCharTypeBuffer<T>& nptr, T **endptr)
    { return wxStrtod(nptr.data(), endptr); }

// We implement wxStrto*() like this so that the code compiles when NULL is
// passed in - - if we had just char** and wchar_t** overloads for 'endptr', it
// would be ambiguous. The solution is to use a template so that endptr can be
// any type: when NULL constant is used, the type will be int and we can handle
// that case specially. Otherwise, we infer the type that 'nptr' should be
// converted to from the type of 'endptr'. We need wxStrtoxCharType<T> template
// to make the code compile even for T=int (that's the case when it's not going
// to be ever used, but it still has to compile).
template<typename T> struct wxStrtoxCharType {};
template<> struct wxStrtoxCharType<char**>
{
    typedef const char* Type;
    static char** AsPointer(char **p) { return p; }
};
template<> struct wxStrtoxCharType<wchar_t**>
{
    typedef const wchar_t* Type;
    static wchar_t** AsPointer(wchar_t **p) { return p; }
};
template<> struct wxStrtoxCharType<int>
{
    typedef const char* Type; /* this one is never used */
    static char** AsPointer(int WXUNUSED_UNLESS_DEBUG(p))
    {
        wxASSERT_MSG( p == 0, "passing non-NULL int is invalid" );
        return NULL;
    }
};

template<typename T>
inline double wxStrtod(const wxString& nptr, T endptr)
{
    if ( endptr == 0 )
    {
        // when we don't care about endptr, use the string representation that
        // doesn't require any conversion (it doesn't matter for this function
        // even if its UTF-8):
        return wxStrtod(nptr.wx_str(), (wxStringCharType**)NULL);
    }
    else
    {
        // note that it is important to use c_str() here and not mb_str() or
        // wc_str(), because we store the pointer into (possibly converted)
        // buffer in endptr and so it must be valid even when wxStrtod() returns
        typedef typename wxStrtoxCharType<T>::Type CharType;
        return wxStrtod((CharType)nptr.c_str(),
                        wxStrtoxCharType<T>::AsPointer(endptr));
    }
}
template<typename T>
inline double wxStrtod(const wxCStrData& nptr, T endptr)
    { return wxStrtod(nptr.AsString(), endptr); }


#define WX_STRTOX_FUNC(rettype, name, implA, implW)                           \
    /* see wxStrtod() above for explanation of this code: */                  \
    inline rettype name(const char *nptr, char **endptr, int base)            \
        { return implA(nptr, endptr, base); }                                 \
    inline rettype name(const wchar_t *nptr, wchar_t **endptr, int base)      \
        { return implW(nptr, endptr, base); }                                 \
    template<typename T>                                                      \
    inline rettype name(const wxScopedCharTypeBuffer<T>& nptr, T **endptr, int)\
        { return name(nptr.data(), endptr); }                                 \
    template<typename T>                                                      \
    inline rettype name(const wxString& nptr, T endptr, int base)             \
    {                                                                         \
        if ( endptr == 0 )                                                    \
            return name(nptr.wx_str(), (wxStringCharType**)NULL, base);       \
        else                                                                  \
        {                                                                     \
            typedef typename wxStrtoxCharType<T>::Type CharType;              \
            return name((CharType)nptr.c_str(),                               \
                        wxStrtoxCharType<T>::AsPointer(endptr),               \
                        base);                                                \
        }                                                                     \
    }                                                                         \
    template<typename T>                                                      \
    inline rettype name(const wxCStrData& nptr, T endptr, int base)           \
        { return name(nptr.AsString(), endptr, base); }

WX_STRTOX_FUNC(long, wxStrtol, wxCRT_StrtolA, wxCRT_StrtolW)
WX_STRTOX_FUNC(unsigned long, wxStrtoul, wxCRT_StrtoulA, wxCRT_StrtoulW)
#ifdef wxLongLong_t
WX_STRTOX_FUNC(wxLongLong_t, wxStrtoll, wxCRT_StrtollA, wxCRT_StrtollW)
WX_STRTOX_FUNC(wxULongLong_t, wxStrtoull, wxCRT_StrtoullA, wxCRT_StrtoullW)
#endif // wxLongLong_t

#undef WX_STRTOX_FUNC


// there is no command interpreter under CE, hence no system()
#ifndef __WXWINCE__

// mingw32 doesn't provide _tsystem() even though it provides other stdlib.h
// functions in their wide versions
#ifdef wxCRT_SystemW
inline int wxSystem(const wxString& str) { return wxCRT_SystemW(str.wc_str()); }
#else
inline int wxSystem(const wxString& str) { return wxCRT_SystemA(str.mb_str()); }
#endif

#endif // !__WXWINCE__/__WXWINCE__

inline char* wxGetenv(const char *name) { return wxCRT_GetenvA(name); }
inline wchar_t* wxGetenv(const wchar_t *name) { return wxCRT_GetenvW(name); }
inline char* wxGetenv(const wxString& name) { return wxCRT_GetenvA(name.mb_str()); }
inline char* wxGetenv(const wxCStrData& name) { return wxCRT_GetenvA(name.AsCharBuf()); }
inline char* wxGetenv(const wxScopedCharBuffer& name) { return wxCRT_GetenvA(name.data()); }
inline wchar_t* wxGetenv(const wxScopedWCharBuffer& name) { return wxCRT_GetenvW(name.data()); }

// ----------------------------------------------------------------------------
//                            time.h functions
// ----------------------------------------------------------------------------

inline size_t wxStrftime(char *s, size_t max,
                         const wxString& format, const struct tm *tm)
    { return wxCRT_StrftimeA(s, max, format.mb_str(), tm); }

inline size_t wxStrftime(wchar_t *s, size_t max,
                         const wxString& format, const struct tm *tm)
    { return wxCRT_StrftimeW(s, max, format.wc_str(), tm); }

// NB: we can't provide both char* and wchar_t* versions for obvious reasons
//     and returning wxString wouldn't work either (it would be immediately
//     destroyed and if assigned to char*/wchar_t*, the pointer would be
//     invalid), so we only keep ASCII version, because the returned value
//     is always ASCII anyway
#define  wxAsctime   asctime
#define  wxCtime     ctime


// ----------------------------------------------------------------------------
//                             ctype.h functions
// ----------------------------------------------------------------------------

// FIXME-UTF8: we'd be better off implementing these ourselves, as the CRT
//             version is locale-dependent
// FIXME-UTF8: these don't work when EOF is passed in because of wxUniChar,
//             is this OK or not?

inline bool wxIsalnum(const wxUniChar& c)  { return wxCRT_IsalnumW(c) != 0; }
inline bool wxIsalpha(const wxUniChar& c)  { return wxCRT_IsalphaW(c) != 0; }
inline bool wxIscntrl(const wxUniChar& c)  { return wxCRT_IscntrlW(c) != 0; }
inline bool wxIsdigit(const wxUniChar& c)  { return wxCRT_IsdigitW(c) != 0; }
inline bool wxIsgraph(const wxUniChar& c)  { return wxCRT_IsgraphW(c) != 0; }
inline bool wxIslower(const wxUniChar& c)  { return wxCRT_IslowerW(c) != 0; }
inline bool wxIsprint(const wxUniChar& c)  { return wxCRT_IsprintW(c) != 0; }
inline bool wxIspunct(const wxUniChar& c)  { return wxCRT_IspunctW(c) != 0; }
inline bool wxIsspace(const wxUniChar& c)  { return wxCRT_IsspaceW(c) != 0; }
inline bool wxIsupper(const wxUniChar& c)  { return wxCRT_IsupperW(c) != 0; }
inline bool wxIsxdigit(const wxUniChar& c) { return wxCRT_IsxdigitW(c) != 0; }

inline wxUniChar wxTolower(const wxUniChar& c) { return wxCRT_TolowerW(c); }
inline wxUniChar wxToupper(const wxUniChar& c) { return wxCRT_ToupperW(c); }

#if WXWIN_COMPATIBILITY_2_8
// we had goofed and defined wxIsctrl() instead of (correct) wxIscntrl() in the
// initial versions of this header -- now it is too late to remove it so
// although we fixed the function/macro name above, still provide the
// backwards-compatible synonym.
wxDEPRECATED( inline int wxIsctrl(const wxUniChar& c) );
inline int wxIsctrl(const wxUniChar& c) { return wxIscntrl(c); }
#endif // WXWIN_COMPATIBILITY_2_8

inline bool wxIsascii(const wxUniChar& c) { return c.IsAscii(); }

#endif /* _WX_WXCRT_H_ */
