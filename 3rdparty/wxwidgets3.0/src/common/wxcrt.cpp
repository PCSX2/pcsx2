/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/wxcrt.cpp
// Purpose:     wxChar CRT wrappers implementation
// Author:      Ove Kaven
// Modified by: Ron Lee, Francesco Montorsi
// Created:     09/04/99
// Copyright:   (c) wxWidgets copyright
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ===========================================================================
// headers, declarations, constants
// ===========================================================================

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/crt.h"
#include "wx/strconv.h" // wxMBConv::cWC2MB()

#define _ISOC9X_SOURCE 1 // to get vsscanf()
#define _BSD_SOURCE    1 // to still get strdup()

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifdef __SGI__
    // wide character functions are declared in std namespace under IRIX
    using namespace std;

    // and this one is only declared if __c99 is defined which is not the case
    // for C++ builds, so declare it ourselves
    extern "C" int vswscanf(const wchar_t *, const wchar_t *, va_list);
#endif

#ifndef __WXWINCE__
    #include <time.h>
    #include <locale.h>
#else
    #include "wx/msw/wince/time.h"
#endif

#ifndef WX_PRECOMP
    #include "wx/string.h"
    #include "wx/hash.h"
    #include "wx/utils.h"     // for wxMin and wxMax
    #include "wx/log.h"
#endif

#ifdef HAVE_LANGINFO_H
    #include <langinfo.h>
#endif

#ifdef __WXWINCE__
    // there is no errno.h under CE apparently
    #define wxSET_ERRNO(value)
#else
    #include <errno.h>

    #define wxSET_ERRNO(value) errno = value
#endif

#if defined(__DARWIN__)
    #include "wx/osx/core/cfref.h"
    #include <CoreFoundation/CFLocale.h>
    #include "wx/osx/core/cfstring.h"
    #include <xlocale.h>
#endif

wxDECL_FOR_STRICT_MINGW32(int, vswprintf, (wchar_t*, const wchar_t*, __VALIST));
wxDECL_FOR_STRICT_MINGW32(int, _putws, (const wchar_t*));
wxDECL_FOR_STRICT_MINGW32(void, _wperror, (const wchar_t*));

WXDLLIMPEXP_BASE size_t wxMB2WC(wchar_t *buf, const char *psz, size_t n)
{
  // assume that we have mbsrtowcs() too if we have wcsrtombs()
#ifdef HAVE_WCSRTOMBS
  mbstate_t mbstate;
  memset(&mbstate, 0, sizeof(mbstate_t));
#endif

  if (buf) {
    if (!n || !*psz) {
      if (n) *buf = wxT('\0');
      return 0;
    }
#ifdef HAVE_WCSRTOMBS
    return mbsrtowcs(buf, &psz, n, &mbstate);
#else
    return mbstowcs(buf, psz, n);
#endif
  }

  // Note that we rely on common (and required by Unix98 but unfortunately not
  // C99) extension which allows to call mbs(r)towcs() with NULL output pointer
  // to just get the size of the needed buffer -- this is needed as otherwise
  // we have no idea about how much space we need. Currently all supported
  // compilers do provide it and if they don't, HAVE_WCSRTOMBS shouldn't be
  // defined at all.
#ifdef HAVE_WCSRTOMBS
  return mbsrtowcs(NULL, &psz, 0, &mbstate);
#else
  return mbstowcs(NULL, psz, 0);
#endif
}

WXDLLIMPEXP_BASE size_t wxWC2MB(char *buf, const wchar_t *pwz, size_t n)
{
#ifdef HAVE_WCSRTOMBS
  mbstate_t mbstate;
  memset(&mbstate, 0, sizeof(mbstate_t));
#endif

  if (buf) {
    if (!n || !*pwz) {
      // glibc2.1 chokes on null input
      if (n) *buf = '\0';
      return 0;
    }
#ifdef HAVE_WCSRTOMBS
    return wcsrtombs(buf, &pwz, n, &mbstate);
#else
    return wcstombs(buf, pwz, n);
#endif
  }

#ifdef HAVE_WCSRTOMBS
  return wcsrtombs(NULL, &pwz, 0, &mbstate);
#else
  return wcstombs(NULL, pwz, 0);
#endif
}

char* wxSetlocale(int category, const char *locale)
{
#ifdef __WXWINCE__
    // FIXME-CE: there is no setlocale() in CE CRT, use SetThreadLocale()?
    wxUnusedVar(category);
    wxUnusedVar(locale);

    return NULL;
#else // !__WXWINCE__
#ifdef __WXMAC__
    char *rv = NULL ;
    if ( locale != NULL && locale[0] == 0 )
    {
        // the attempt to use newlocale(LC_ALL_MASK, "", NULL);
        // here in order to deduce the language along the environment vars rules
        // lead to strange crashes later...

        // we have to emulate the behaviour under OS X
        wxCFRef<CFLocaleRef> userLocaleRef(CFLocaleCopyCurrent());
        wxCFStringRef str(wxCFRetain((CFStringRef)CFLocaleGetValue(userLocaleRef, kCFLocaleLanguageCode)));
        wxString langFull = str.AsString()+"_";
        str.reset(wxCFRetain((CFStringRef)CFLocaleGetValue(userLocaleRef, kCFLocaleCountryCode)));
        langFull += str.AsString();
        rv = setlocale(category, langFull.c_str());
    }
    else
        rv = setlocale(category, locale);
#else
    char *rv = setlocale(category, locale);
#endif
    if ( locale != NULL /* setting locale, not querying */ &&
         rv /* call was successful */ )
    {
        wxUpdateLocaleIsUtf8();
    }
    return rv;
#endif // __WXWINCE__/!__WXWINCE__
}

// ============================================================================
// printf() functions business
// ============================================================================

// special test mode: define all functions below even if we don't really need
// them to be able to test them
#ifdef wxTEST_PRINTF
    #undef wxFprintf
    #undef wxPrintf
    #undef wxSprintf
    #undef wxVfprintf
    #undef wxVsprintf
    #undef wxVprintf
    #undef wxVsnprintf_

    #define wxNEED_WPRINTF

    int wxCRT_VfprintfW( FILE *stream, const wchar_t *format, va_list argptr );
#endif

#if defined(__DMC__)
/* Digital Mars adds count to _stprintf (C99) so convert */
int wxCRT_SprintfW (wchar_t * __RESTRICT s, const wchar_t * __RESTRICT format, ... )
{
    va_list arglist;

    va_start( arglist, format );
    int iLen = swprintf ( s, -1, format, arglist );
    va_end( arglist );
    return iLen ;
}
#endif //__DMC__

// ----------------------------------------------------------------------------
// implement the standard IO functions for wide char if libc doesn't have them
// ----------------------------------------------------------------------------

#ifndef wxCRT_FputsW
int wxCRT_FputsW(const wchar_t *ws, FILE *stream)
{
    wxCharBuffer buf(wxConvLibc.cWC2MB(ws));
    if ( !buf )
        return -1;

    // counting the number of wide characters written isn't worth the trouble,
    // simply distinguish between ok and error
    return wxCRT_FputsA(buf, stream) == -1 ? -1 : 0;
}
#endif // !wxCRT_FputsW

#ifndef wxCRT_PutsW
int wxCRT_PutsW(const wchar_t *ws)
{
    int rc = wxCRT_FputsW(ws, stdout);
    if ( rc != -1 )
    {
        if ( wxCRT_FputsW(L"\n", stdout) == -1 )
            return -1;

        rc++;
    }

    return rc;
}
#endif // !wxCRT_PutsW

#ifndef wxCRT_FputcW
int /* not wint_t */ wxCRT_FputcW(wchar_t wc, FILE *stream)
{
    wchar_t ws[2] = { wc, L'\0' };

    return wxCRT_FputsW(ws, stream);
}
#endif // !wxCRT_FputcW

// NB: we only implement va_list functions here, the ones taking ... are
//     defined below for wxNEED_PRINTF_CONVERSION case anyhow and we reuse
//     the definitions there to avoid duplicating them here
#ifdef wxNEED_WPRINTF

// TODO: implement the scanf() functions
static int vwscanf(const wchar_t *format, va_list argptr)
{
    wxFAIL_MSG( wxT("TODO") );

    return -1;
}

static int vfwscanf(FILE *stream, const wchar_t *format, va_list argptr)
{
    wxFAIL_MSG( wxT("TODO") );

    return -1;
}

#define vswprintf wxCRT_VsnprintfW

static int vfwprintf(FILE *stream, const wchar_t *format, va_list argptr)
{
    wxString s;
    int rc = s.PrintfV(format, argptr);

    if ( rc != -1 )
    {
        // we can't do much better without Unicode support in libc...
        if ( fprintf(stream, "%s", (const char*)s.mb_str() ) == -1 )
            return -1;
    }

    return rc;
}

static int vwprintf(const wchar_t *format, va_list argptr)
{
    return wxCRT_VfprintfW(stdout, format, argptr);
}

#endif // wxNEED_WPRINTF

#ifdef wxNEED_VSWSCANF
static int vswscanf(const wchar_t *ws, const wchar_t *format, va_list argptr)
{
    // The best we can do without proper Unicode support in glibc is to
    // convert the strings into MB representation and run ANSI version
    // of the function. This doesn't work with %c and %s because of difference
    // in size of char and wchar_t, though.

    wxCHECK_MSG( wxStrstr(format, L"%s") == NULL, -1,
                 wxT("incomplete vswscanf implementation doesn't allow %s") );
    wxCHECK_MSG( wxStrstr(format, L"%c") == NULL, -1,
                 wxT("incomplete vswscanf implementation doesn't allow %c") );

    return wxCRT_VsscanfA(static_cast<const char*>(wxConvLibc.cWC2MB(ws)),
        wxConvLibc.cWC2MB(format), argptr);
}
#endif

// ----------------------------------------------------------------------------
// wxPrintf(), wxScanf() and relatives
// ----------------------------------------------------------------------------

// FIXME-UTF8: do format conversion using (modified) wxFormatConverter in
//             template wrappers, not here; note that it will needed to
//             translate all forms of string specifiers to %(l)s for wxPrintf(),
//             but it only should do what it did in 2.8 for wxScanf()!

#ifndef wxCRT_PrintfW
int wxCRT_PrintfW( const wchar_t *format, ... )
{
    va_list argptr;
    va_start(argptr, format);

    int ret = vwprintf( format, argptr );

    va_end(argptr);

    return ret;
}
#endif

#ifndef wxCRT_FprintfW
int wxCRT_FprintfW( FILE *stream, const wchar_t *format, ... )
{
    va_list argptr;
    va_start( argptr, format );

    int ret = vfwprintf( stream, format, argptr );

    va_end(argptr);

    return ret;
}
#endif

#ifndef wxCRT_VfprintfW
int wxCRT_VfprintfW( FILE *stream, const wchar_t *format, va_list argptr )
{
    return vfwprintf( stream, format, argptr );
}
#endif

#ifndef wxCRT_VprintfW
int wxCRT_VprintfW( const wchar_t *format, va_list argptr )
{
    return vwprintf( format, argptr );
}
#endif

#ifndef wxCRT_VsprintfW
int wxCRT_VsprintfW( wchar_t *str, const wchar_t *format, va_list argptr )
{
    // same as for wxSprintf()
    return vswprintf(str, INT_MAX / 4, format, argptr);
}
#endif

#ifndef wxCRT_ScanfW
int wxCRT_ScanfW(const wchar_t *format, ...)
{
    va_list argptr;
    va_start(argptr, format);

#ifdef __VMS
#if (__DECCXX_VER >= 70100000) && !defined(__STD_CFRONT) && !defined( __NONAMESPACE_STD )
   int ret = std::vwscanf(format, argptr);
#else
   int ret = vwscanf(format, argptr);
#endif
#else
   int ret = vwscanf(format, argptr);
#endif

    va_end(argptr);

    return ret;
}
#endif

#ifndef wxCRT_SscanfW
int wxCRT_SscanfW(const wchar_t *str, const wchar_t *format, ...)
{
    va_list argptr;
    va_start(argptr, format);

#ifdef __VMS
#if (__DECCXX_VER >= 70100000) && !defined(__STD_CFRONT) && !defined( __NONAMESPACE_STD )
   int ret = std::vswscanf(str, format, argptr);
#else
   int ret = vswscanf(str, format, argptr);
#endif
#else
   int ret = vswscanf(str, format, argptr);
#endif

    va_end(argptr);

    return ret;
}
#endif

#ifndef wxCRT_FscanfW
int wxCRT_FscanfW(FILE *stream, const wchar_t *format, ...)
{
    va_list argptr;
    va_start(argptr, format);
#ifdef __VMS
#if (__DECCXX_VER >= 70100000) && !defined(__STD_CFRONT) && !defined( __NONAMESPACE_STD )
   int ret = std::vfwscanf(stream, format, argptr);
#else
   int ret = vfwscanf(stream, format, argptr);
#endif
#else
   int ret = vfwscanf(stream, format, argptr);
#endif

    va_end(argptr);

    return ret;
}
#endif

#ifndef wxCRT_VsscanfW
int wxCRT_VsscanfW(const wchar_t *str, const wchar_t *format, va_list argptr)
{
#ifdef __VMS
#if (__DECCXX_VER >= 70100000) && !defined(__STD_CFRONT) && !defined( __NONAMESPACE_STD )
   return std::vswscanf(str, format, argptr);
#else
   return vswscanf(str, format, argptr);
#endif
#else
   return vswscanf(str, format, argptr);
#endif
}
#endif


// ----------------------------------------------------------------------------
// wrappers to printf and scanf function families
// ----------------------------------------------------------------------------

#if !wxUSE_UTF8_LOCALE_ONLY
int wxDoSprintfWchar(char *str, const wxChar *format, ...)
{
    va_list argptr;
    va_start(argptr, format);

    int rv = wxVsprintf(str, format, argptr);

    va_end(argptr);
    return rv;
}
#endif // !wxUSE_UTF8_LOCALE_ONLY

#if wxUSE_UNICODE_UTF8
int wxDoSprintfUtf8(char *str, const char *format, ...)
{
    va_list argptr;
    va_start(argptr, format);

    int rv = wxVsprintf(str, format, argptr);

    va_end(argptr);
    return rv;
}
#endif // wxUSE_UNICODE_UTF8

#if wxUSE_UNICODE

#if !wxUSE_UTF8_LOCALE_ONLY
int wxDoSprintfWchar(wchar_t *str, const wxChar *format, ...)
{
    va_list argptr;
    va_start(argptr, format);

    int rv = wxVsprintf(str, format, argptr);

    va_end(argptr);
    return rv;
}
#endif // !wxUSE_UTF8_LOCALE_ONLY

#if wxUSE_UNICODE_UTF8
int wxDoSprintfUtf8(wchar_t *str, const char *format, ...)
{
    va_list argptr;
    va_start(argptr, format);

    int rv = wxVsprintf(str, format, argptr);

    va_end(argptr);
    return rv;
}
#endif // wxUSE_UNICODE_UTF8

#endif // wxUSE_UNICODE

#if !wxUSE_UTF8_LOCALE_ONLY
int wxDoSnprintfWchar(char *str, size_t size, const wxChar *format, ...)
{
    va_list argptr;
    va_start(argptr, format);

    int rv = wxVsnprintf(str, size, format, argptr);

    va_end(argptr);
    return rv;
}
#endif // !wxUSE_UTF8_LOCALE_ONLY

#if wxUSE_UNICODE_UTF8
int wxDoSnprintfUtf8(char *str, size_t size, const char *format, ...)
{
    va_list argptr;
    va_start(argptr, format);

    int rv = wxVsnprintf(str, size, format, argptr);

    va_end(argptr);
    return rv;
}
#endif // wxUSE_UNICODE_UTF8

#if wxUSE_UNICODE

#if !wxUSE_UTF8_LOCALE_ONLY
int wxDoSnprintfWchar(wchar_t *str, size_t size, const wxChar *format, ...)
{
    va_list argptr;
    va_start(argptr, format);

    int rv = wxVsnprintf(str, size, format, argptr);

    va_end(argptr);
    return rv;
}
#endif // !wxUSE_UTF8_LOCALE_ONLY

#if wxUSE_UNICODE_UTF8
int wxDoSnprintfUtf8(wchar_t *str, size_t size, const char *format, ...)
{
    va_list argptr;
    va_start(argptr, format);

    int rv = wxVsnprintf(str, size, format, argptr);

    va_end(argptr);
    return rv;
}
#endif // wxUSE_UNICODE_UTF8

#endif // wxUSE_UNICODE


#ifdef HAVE_BROKEN_VSNPRINTF_DECL
    #define vsnprintf wx_fixed_vsnprintf
#endif

#if wxUSE_UNICODE

namespace
{

#if !wxUSE_UTF8_LOCALE_ONLY
int ConvertStringToBuf(const wxString& s, char *out, size_t outsize)
{
    const wxCharBuffer buf(s.mb_str());

    const size_t len = buf.length();
    if ( outsize > len )
    {
        memcpy(out, buf, len+1);
    }
    else // not enough space
    {
        memcpy(out, buf, outsize-1);
        out[outsize-1] = '\0';
    }

    return len;
}
#endif // !wxUSE_UTF8_LOCALE_ONLY

#if wxUSE_UNICODE_UTF8
int ConvertStringToBuf(const wxString& s, wchar_t *out, size_t outsize)
{
    const wxWX2WCbuf buf(s.wc_str());
    size_t len = s.length(); // same as buf length for wchar_t*
    if ( outsize > len )
    {
        memcpy(out, buf, (len+1) * sizeof(wchar_t));
    }
    else // not enough space
    {
        memcpy(out, buf, (outsize-1) * sizeof(wchar_t));
        out[outsize-1] = 0;
    }
    return len;
}
#endif // wxUSE_UNICODE_UTF8

} // anonymous namespace

template<typename T>
static size_t PrintfViaString(T *out, size_t outsize,
                              const wxString& format, va_list argptr)
{
    wxString s;
    s.PrintfV(format, argptr);

    return ConvertStringToBuf(s, out, outsize);
}
#endif // wxUSE_UNICODE

int wxVsprintf(char *str, const wxString& format, va_list argptr)
{
#if wxUSE_UTF8_LOCALE_ONLY
    return wxCRT_VsprintfA(str, format.wx_str(), argptr);
#else
    #if wxUSE_UNICODE_UTF8
    if ( wxLocaleIsUtf8 )
        return wxCRT_VsprintfA(str, format.wx_str(), argptr);
    else
    #endif
    #if wxUSE_UNICODE
    return PrintfViaString(str, wxNO_LEN, format, argptr);
    #else
    return wxCRT_VsprintfA(str, format.mb_str(), argptr);
    #endif
#endif
}

#if wxUSE_UNICODE
int wxVsprintf(wchar_t *str, const wxString& format, va_list argptr)
{
#if wxUSE_UNICODE_WCHAR
#ifdef __DMC__
/*
This fails with a bug similar to
http://www.digitalmars.com/pnews/read.php?server=news.digitalmars.com&group=c++.beta&artnum=680
in DMC 8.49 and 8.50
I don't see it being used in the wxWidgets sources at present (oct 2007) CE
*/
#pragma message ( "warning ::::: wxVsprintf(wchar_t *str, const wxString& format, va_list argptr) not yet implemented" )
    wxFAIL_MSG( wxT("TODO") );

    return -1;
#else
    return wxCRT_VsprintfW(str, format.wc_str(), argptr);
#endif //DMC
#else // wxUSE_UNICODE_UTF8
    #if !wxUSE_UTF8_LOCALE_ONLY
    if ( !wxLocaleIsUtf8 )
        return wxCRT_VsprintfW(str, format.wc_str(), argptr);
    else
    #endif
        return PrintfViaString(str, wxNO_LEN, format, argptr);
#endif // wxUSE_UNICODE_UTF8
}
#endif // wxUSE_UNICODE

int wxVsnprintf(char *str, size_t size, const wxString& format, va_list argptr)
{
    int rv;
#if wxUSE_UTF8_LOCALE_ONLY
    rv = wxCRT_VsnprintfA(str, size, format.wx_str(), argptr);
#else
    #if wxUSE_UNICODE_UTF8
    if ( wxLocaleIsUtf8 )
        rv = wxCRT_VsnprintfA(str, size, format.wx_str(), argptr);
    else
    #endif
    #if wxUSE_UNICODE
    {
        // NB: if this code is called, then wxString::PrintV() would use the
        //     wchar_t* version of wxVsnprintf(), so it's safe to use PrintV()
        //     from here
        rv = PrintfViaString(str, size, format, argptr);
    }
    #else
    rv = wxCRT_VsnprintfA(str, size, format.mb_str(), argptr);
    #endif
#endif

    // VsnprintfTestCase reveals that glibc's implementation of vswprintf
    // doesn't nul terminate on truncation.
    str[size - 1] = 0;

    return rv;
}

#if wxUSE_UNICODE
int wxVsnprintf(wchar_t *str, size_t size, const wxString& format, va_list argptr)
{
    int rv;

#if wxUSE_UNICODE_WCHAR
    rv = wxCRT_VsnprintfW(str, size, format.wc_str(), argptr);
#else // wxUSE_UNICODE_UTF8
    #if !wxUSE_UTF8_LOCALE_ONLY
    if ( !wxLocaleIsUtf8 )
        rv = wxCRT_VsnprintfW(str, size, format.wc_str(), argptr);
    else
    #endif
    {
        // NB: if this code is called, then wxString::PrintV() would use the
        //     char* version of wxVsnprintf(), so it's safe to use PrintV()
        //     from here
        rv = PrintfViaString(str, size, format, argptr);
    }
#endif // wxUSE_UNICODE_UTF8

    // VsnprintfTestCase reveals that glibc's implementation of vswprintf
    // doesn't nul terminate on truncation.
    str[size - 1] = 0;

    return rv;
}
#endif // wxUSE_UNICODE


// ----------------------------------------------------------------------------
// ctype.h stuff (currently unused)
// ----------------------------------------------------------------------------

#ifndef wxCRT_StrdupA
WXDLLIMPEXP_BASE char *wxCRT_StrdupA(const char *s)
{
    return strcpy((char *)malloc(strlen(s) + 1), s);
}
#endif // wxCRT_StrdupA

#ifndef wxCRT_StrdupW
WXDLLIMPEXP_BASE wchar_t * wxCRT_StrdupW(const wchar_t *pwz)
{
  size_t size = (wxWcslen(pwz) + 1) * sizeof(wchar_t);
  wchar_t *ret = (wchar_t *) malloc(size);
  memcpy(ret, pwz, size);
  return ret;
}
#endif // wxCRT_StrdupW

#ifndef wxWCHAR_T_IS_WXCHAR16
size_t wxStrlen(const wxChar16 *s )
{
    if (!s) return 0;
    size_t i=0;
    while (*s!=0) { ++i; ++s; };
    return i;
}

wxChar16* wxStrdup(const wxChar16* s)
{
  size_t size = (wxStrlen(s) + 1) * sizeof(wxChar16);
  wxChar16 *ret = (wxChar16*) malloc(size);
  memcpy(ret, s, size);
  return ret;
}
#endif

#ifndef wxWCHAR_T_IS_WXCHAR32
size_t wxStrlen(const wxChar32 *s )
{
    if (!s) return 0;
    size_t i=0;
    while (*s!=0) { ++i; ++s; };
    return i;
}

wxChar32* wxStrdup(const wxChar32* s)
{
  size_t size = (wxStrlen(s) + 1) * sizeof(wxChar32);
  wxChar32 *ret = (wxChar32*) malloc(size);
  if ( ret )
      memcpy(ret, s, size);
  return ret;
}
#endif

#ifndef wxCRT_StricmpA
WXDLLIMPEXP_BASE int wxCRT_StricmpA(const char *psz1, const char *psz2)
{
  char c1, c2;
  do {
    c1 = wxTolower(*psz1++);
    c2 = wxTolower(*psz2++);
  } while ( c1 && (c1 == c2) );
  return c1 - c2;
}
#endif // !defined(wxCRT_StricmpA)

#ifndef wxCRT_StricmpW
WXDLLIMPEXP_BASE int wxCRT_StricmpW(const wchar_t *psz1, const wchar_t *psz2)
{
  wchar_t c1, c2;
  do {
    c1 = wxTolower(*psz1++);
    c2 = wxTolower(*psz2++);
  } while ( c1 && (c1 == c2) );
  return c1 - c2;
}
#endif // !defined(wxCRT_StricmpW)

#ifndef wxCRT_StrnicmpA
WXDLLIMPEXP_BASE int wxCRT_StrnicmpA(const char *s1, const char *s2, size_t n)
{
  // initialize the variables just to suppress stupid gcc warning
  char c1 = 0, c2 = 0;
  while (n && ((c1 = wxTolower(*s1)) == (c2 = wxTolower(*s2)) ) && c1) n--, s1++, s2++;
  if (n) {
    if (c1 < c2) return -1;
    if (c1 > c2) return 1;
  }
  return 0;
}
#endif // !defined(wxCRT_StrnicmpA)

#ifndef wxCRT_StrnicmpW
WXDLLIMPEXP_BASE int wxCRT_StrnicmpW(const wchar_t *s1, const wchar_t *s2, size_t n)
{
  // initialize the variables just to suppress stupid gcc warning
  wchar_t c1 = 0, c2 = 0;
  while (n && ((c1 = wxTolower(*s1)) == (c2 = wxTolower(*s2)) ) && c1) n--, s1++, s2++;
  if (n) {
    if (c1 < c2) return -1;
    if (c1 > c2) return 1;
  }
  return 0;
}
#endif // !defined(wxCRT_StrnicmpW)

// ----------------------------------------------------------------------------
// string.h functions
// ----------------------------------------------------------------------------

// this (and wxCRT_StrncmpW below) are extern "C" because they are needed
// by regex code, the rest isn't needed, so it's not declared as extern "C"
#ifndef wxCRT_StrlenW
extern "C" WXDLLIMPEXP_BASE size_t wxCRT_StrlenW(const wchar_t *s)
{
    size_t n = 0;
    while ( *s++ )
        n++;

    return n;
}
#endif

// ----------------------------------------------------------------------------
// stdlib.h functions
// ----------------------------------------------------------------------------

#ifndef wxCRT_GetenvW
WXDLLIMPEXP_BASE wchar_t* wxCRT_GetenvW(const wchar_t *name)
{
    // NB: buffer returned by getenv() is allowed to be overwritten next
    //     time getenv() is called, so it is OK to use static string
    //     buffer to hold the data.
    static wxWCharBuffer value;
    value = wxConvLibc.cMB2WC(getenv(wxConvLibc.cWC2MB(name)));
    return value.data();
}
#endif // !wxCRT_GetenvW

#ifndef wxCRT_StrftimeW
WXDLLIMPEXP_BASE size_t
wxCRT_StrftimeW(wchar_t *s, size_t maxsize, const wchar_t *fmt, const struct tm *tm)
{
    if ( !maxsize )
        return 0;

    wxCharBuffer buf(maxsize);

    wxCharBuffer bufFmt(wxConvLibc.cWX2MB(fmt));
    if ( !bufFmt )
        return 0;

    size_t ret = strftime(buf.data(), maxsize, bufFmt, tm);
    if  ( !ret )
        return 0;

    wxWCharBuffer wbuf = wxConvLibc.cMB2WX(buf);
    if ( !wbuf )
        return 0;

    wxCRT_StrncpyW(s, wbuf, maxsize);
    return wxCRT_StrlenW(s);
}
#endif // !wxCRT_StrftimeW

#ifdef wxLongLong_t
template<typename T>
static wxULongLong_t
wxCRT_StrtoullBase(const T* nptr, T** endptr, int base, T* sign)
{
    wxULongLong_t sum = 0;
    wxString wxstr(nptr);
    wxString::const_iterator i = wxstr.begin();
    wxString::const_iterator end = wxstr.end();

    // Skip spaces
    while ( i != end && wxIsspace(*i) ) ++i;

    // Starts with sign?
    *sign = wxT(' ');
    if ( i != end )
    {
        T c = *i;
        if ( c == wxT('+') || c == wxT('-') )
        {
            *sign = c;
            ++i;
        }
    }

    // Starts with octal or hexadecimal prefix?
    if ( i != end && *i == wxT('0') )
    {
        ++i;
        if ( i != end )
        {
            if ( (*i == wxT('x')) || (*i == wxT('X')) )
            {
                // Hexadecimal prefix: use base 16 if auto-detecting.
                if ( base == 0 )
                    base = 16;

                // If we do use base 16, just skip "x" as well.
                if ( base == 16 )
                {
                    ++i;
                }
                else // Not using base 16
                {
                    // Then it's an error.
                    if ( endptr )
                        *endptr = (T*) nptr;
                    wxSET_ERRNO(EINVAL);
                    return sum;
                }
            }
            else if ( base == 0 )
            {
                base = 8;
            }
        }
        else
            --i;
    }

    if ( base == 0 )
        base = 10;

    for ( ; i != end; ++i )
    {
        unsigned int n;

        T c = *i;
        if ( c >= '0' )
        {
            if ( c <= '9' )
                n = c - wxT('0');
            else
                n = wxTolower(c) - wxT('a') + 10;
        }
        else
            break;

        if ( n >= (unsigned int)base )
            // Invalid character (for this base)
            break;

        wxULongLong_t prevsum = sum;
        sum = (sum * base) + n;

        if ( sum < prevsum )
        {
            wxSET_ERRNO(ERANGE);
            break;
        }
    }

    if ( endptr )
    {
        *endptr = (T*)(nptr + (i - wxstr.begin()));
    }

    return sum;
}

template<typename T>
static wxULongLong_t wxCRT_DoStrtoull(const T* nptr, T** endptr, int base)
{
    T sign;
    wxULongLong_t uval = ::wxCRT_StrtoullBase(nptr, endptr, base, &sign);

    if ( sign == wxT('-') )
    {
        wxSET_ERRNO(ERANGE);
        uval = 0;
    }

    return uval;
}

template<typename T>
static wxLongLong_t wxCRT_DoStrtoll(const T* nptr, T** endptr, int base)
{
    T sign;
    wxULongLong_t uval = ::wxCRT_StrtoullBase(nptr, endptr, base, &sign);
    wxLongLong_t val = 0;

    if ( sign == wxT('-') )
    {
        if (uval <= (wxULongLong_t)wxINT64_MAX + 1)
        {
            val = -(wxLongLong_t)uval;
        }
        else
        {
            wxSET_ERRNO(ERANGE);
        }
    }
    else if ( uval <= wxINT64_MAX )
    {
        val = uval;
    }
    else
    {
        wxSET_ERRNO(ERANGE);
    }

    return val;
}

#ifndef wxCRT_StrtollA
wxLongLong_t wxCRT_StrtollA(const char* nptr, char** endptr, int base)
    { return wxCRT_DoStrtoll(nptr, endptr, base); }
#endif
#ifndef wxCRT_StrtollW
wxLongLong_t wxCRT_StrtollW(const wchar_t* nptr, wchar_t** endptr, int base)
    { return wxCRT_DoStrtoll(nptr, endptr, base); }
#endif

#ifndef wxCRT_StrtoullA
wxULongLong_t wxCRT_StrtoullA(const char* nptr, char** endptr, int base)
    { return wxCRT_DoStrtoull(nptr, endptr, base); }
#endif
#ifndef wxCRT_StrtoullW
wxULongLong_t wxCRT_StrtoullW(const wchar_t* nptr, wchar_t** endptr, int base)
    { return wxCRT_DoStrtoull(nptr, endptr, base); }
#endif

#endif // wxLongLong_t

// ----------------------------------------------------------------------------
// strtok() functions
// ----------------------------------------------------------------------------

template<typename T>
static T *wxCRT_DoStrtok(T *psz, const T *delim, T **save_ptr)
{
    if (!psz)
    {
        psz = *save_ptr;
        if ( !psz )
            return NULL;
    }

    psz += wxStrspn(psz, delim);
    if (!*psz)
    {
        *save_ptr = NULL;
        return NULL;
    }

    T *ret = psz;
    psz = wxStrpbrk(psz, delim);
    if (!psz)
    {
        *save_ptr = NULL;
    }
    else
    {
        *psz = wxT('\0');
        *save_ptr = psz + 1;
    }

    return ret;
}

#ifndef wxCRT_StrtokA
char *wxCRT_StrtokA(char *psz, const char *delim, char **save_ptr)
    { return wxCRT_DoStrtok(psz, delim, save_ptr); }
#endif
#ifndef wxCRT_StrtokW
wchar_t *wxCRT_StrtokW(wchar_t *psz, const wchar_t *delim, wchar_t **save_ptr)
    { return wxCRT_DoStrtok(psz, delim, save_ptr); }
#endif

// ----------------------------------------------------------------------------
// missing C RTL functions
// ----------------------------------------------------------------------------

#ifdef wxNEED_STRDUP

char *strdup(const char *s)
{
    char *dest = (char*) malloc( strlen( s ) + 1 ) ;
    if ( dest )
        strcpy( dest , s ) ;
    return dest ;
}
#endif // wxNEED_STRDUP

#if defined(__WXWINCE__) && (_WIN32_WCE <= 211)

void *calloc( size_t num, size_t size )
{
    void** ptr = (void **)malloc(num * size);
    memset( ptr, 0, num * size);
    return ptr;
}

#endif // __WXWINCE__ <= 211

// ============================================================================
// wxLocaleIsUtf8
// ============================================================================

#if wxUSE_UNICODE_UTF8

#if !wxUSE_UTF8_LOCALE_ONLY
bool wxLocaleIsUtf8 = false; // the safer setting if not known
#endif

static bool wxIsLocaleUtf8()
{
    // NB: we intentionally don't use wxLocale::GetSystemEncodingName(),
    //     because a) it may be unavailable in some builds and b) has slightly
    //     different semantics (default locale instead of current)

#if defined(HAVE_LANGINFO_H) && defined(CODESET)
    // GNU libc provides current character set this way (this conforms to
    // Unix98)
    const char *charset = nl_langinfo(CODESET);
    if ( charset )
    {
        // "UTF-8" is used by modern glibc versions, but test other variants
        // as well, just in case:
        if ( strcmp(charset, "UTF-8") == 0 ||
             strcmp(charset, "utf-8") == 0 ||
             strcmp(charset, "UTF8") == 0 ||
             strcmp(charset, "utf8") == 0 )
        {
            return true;
        }
    }
#endif // HAVE_LANGINFO_H

    // check if we're running under the "C" locale: it is 7bit subset
    // of UTF-8, so it can be safely used with the UTF-8 build:
    const char *lc_ctype = setlocale(LC_CTYPE, NULL);
    if ( lc_ctype &&
         (strcmp(lc_ctype, "C") == 0 || strcmp(lc_ctype, "POSIX") == 0) )
    {
        return true;
    }

    // we don't know what charset libc is using, so assume the worst
    // to be safe:
    return false;
}

void wxUpdateLocaleIsUtf8()
{
#if wxUSE_UTF8_LOCALE_ONLY
    if ( !wxIsLocaleUtf8() )
    {
        wxLogFatalError(wxT("This program requires UTF-8 locale to run."));
    }
#else // !wxUSE_UTF8_LOCALE_ONLY
    wxLocaleIsUtf8 = wxIsLocaleUtf8();
#endif
}

#endif // wxUSE_UNICODE_UTF8

// ============================================================================
// wx wrappers for CRT functions
// ============================================================================

#if wxUSE_UNICODE_WCHAR
    #define CALL_ANSI_OR_UNICODE(return_kw, callA, callW)  return_kw callW
#elif wxUSE_UNICODE_UTF8 && !wxUSE_UTF8_LOCALE_ONLY
    #define CALL_ANSI_OR_UNICODE(return_kw, callA, callW) \
            return_kw wxLocaleIsUtf8 ? callA : callW
#else // ANSI or UTF8 only
    #define CALL_ANSI_OR_UNICODE(return_kw, callA, callW)  return_kw callA
#endif

int wxPuts(const wxString& s)
{
    // under IRIX putws() takes a non-const argument so use wchar_str() instead
    // of wc_str()
    CALL_ANSI_OR_UNICODE(return,
                         wxCRT_PutsA(s.mb_str()),
                         wxCRT_PutsW(s.wchar_str()));
}

int wxFputs(const wxString& s, FILE *stream)
{
    CALL_ANSI_OR_UNICODE(return,
                         wxCRT_FputsA(s.mb_str(), stream),
                         wxCRT_FputsW(s.wc_str(), stream));
}

int wxFputc(const wxUniChar& c, FILE *stream)
{
#if !wxUSE_UNICODE // FIXME-UTF8: temporary, remove this with ANSI build
    return wxCRT_FputcA((char)c, stream);
#else
    CALL_ANSI_OR_UNICODE(return,
                         wxCRT_FputsA(c.AsUTF8(), stream),
                         wxCRT_FputcW((wchar_t)c, stream));
#endif
}

#ifdef wxCRT_PerrorA

void wxPerror(const wxString& s)
{
#ifdef wxCRT_PerrorW
    CALL_ANSI_OR_UNICODE(wxEMPTY_PARAMETER_VALUE,
                         wxCRT_PerrorA(s.mb_str()),
                         wxCRT_PerrorW(s.wc_str()));
#else
    wxCRT_PerrorA(s.mb_str());
#endif
}

#endif // wxCRT_PerrorA

wchar_t *wxFgets(wchar_t *s, int size, FILE *stream)
{
    wxCHECK_MSG( s, NULL, "empty buffer passed to wxFgets()" );

    wxCharBuffer buf(size - 1);
    // FIXME: this reads too little data if wxConvLibc uses UTF-8 ('size' wide
    //        characters may be encoded by up to 'size'*4 bytes), but what
    //        else can we do?
    if ( wxFgets(buf.data(), size, stream) == NULL )
        return NULL;

    if ( wxConvLibc.ToWChar(s, size, buf, wxNO_LEN) == wxCONV_FAILED )
        return NULL;

    return s;
}

// ----------------------------------------------------------------------------
// wxScanf() and friends
// ----------------------------------------------------------------------------

#ifdef HAVE_VSSCANF // __VISUALC__ and __DMC__ see wx/crt.h
int wxVsscanf(const char *str, const char *format, va_list ap)
    { return wxCRT_VsscanfA(str, format, ap); }
int wxVsscanf(const wchar_t *str, const wchar_t *format, va_list ap)
    { return wxCRT_VsscanfW(str, format, ap); }
int wxVsscanf(const wxCharBuffer& str, const char *format, va_list ap)
    { return wxCRT_VsscanfA(static_cast<const char*>(str), format, ap); }
int wxVsscanf(const wxWCharBuffer& str, const wchar_t *format, va_list ap)
    { return wxCRT_VsscanfW(str, format, ap); }
int wxVsscanf(const wxString& str, const char *format, va_list ap)
    { return wxCRT_VsscanfA(static_cast<const char*>(str.mb_str()), format, ap); }
int wxVsscanf(const wxString& str, const wchar_t *format, va_list ap)
    { return wxCRT_VsscanfW(str.wc_str(), format, ap); }
int wxVsscanf(const wxCStrData& str, const char *format, va_list ap)
    { return wxCRT_VsscanfA(static_cast<const char*>(str.AsCharBuf()), format, ap); }
int wxVsscanf(const wxCStrData& str, const wchar_t *format, va_list ap)
    { return wxCRT_VsscanfW(str.AsWCharBuf(), format, ap); }
#endif // HAVE_NO_VSSCANF
