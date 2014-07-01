///////////////////////////////////////////////////////////////////////////////
// Name:        wx/wxcrtvararg.h
// Purpose:     Type-safe ANSI and Unicode builds compatible wrappers for
//              printf(), scanf() and related CRT functions
// Author:      Joel Farley, Ove Kåven
// Modified by: Vadim Zeitlin, Robert Roebling, Ron Lee
// Created:     2007-02-19
// Copyright:   (c) 2007 REA Elektronik GmbH
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_WXCRTVARARG_H_
#define _WX_WXCRTVARARG_H_

// NB: User code should include wx/crt.h instead of including this
//     header directly.

#include "wx/wxcrt.h"
#include "wx/strvararg.h"

#include "wx/string.h"

// ----------------------------------------------------------------------------
// CRT functions aliases
// ----------------------------------------------------------------------------

/* Required for wxPrintf() etc */
#include <stdarg.h>

/* printf() family saga */

/*
   For many old Unix systems [v]snprintf()/vsscanf() exists in the system
   libraries but not in the headers, so we need to declare it ourselves to be
   able to use it.
 */
#ifdef __UNIX__

#if defined(HAVE_VSNPRINTF) && !defined(HAVE_VSNPRINTF_DECL)
#ifdef __cplusplus
    extern "C"
#else
    extern
#endif
    int vsnprintf(char *str, size_t size, const char *format, va_list ap);
#endif /* !HAVE_VSNPRINTF_DECL */

#if defined(HAVE_SNPRINTF) && !defined(HAVE_SNPRINTF_DECL)
#ifdef __cplusplus
    extern "C"
#else
    extern
#endif
    int snprintf(char *str, size_t size, const char *format, ...);
#endif /* !HAVE_SNPRINTF_DECL */

#if defined(HAVE_VSSCANF) && !defined(HAVE_VSSCANF_DECL)
#ifdef __cplusplus
    extern "C"
#else
    extern
#endif
    int vsscanf(const char *str, const char *format, va_list ap);
#endif /* !HAVE_VSSCANF_DECL */

/* Wrapper for vsnprintf if it's 3rd parameter is non-const. Note: the
 * same isn't done for snprintf below, the builtin wxSnprintf_ is used
 * instead since it's already a simple wrapper */
#if defined __cplusplus && defined HAVE_BROKEN_VSNPRINTF_DECL
    inline int wx_fixed_vsnprintf(char *str, size_t size, const char *format, va_list ap)
    {
        return vsnprintf(str, size, (char*)format, ap);
    }
#endif

#endif /* __UNIX__ */

/*
   mingw32 normally uses MSVCRT which has non-standard vswprintf() and so
   normally _vsnwprintf() is used instead, the only exception is when mingw32
   is used with STLPort which does have a standard vswprintf() starting from
   version 5.1 which we can use.
 */
#ifdef __MINGW32__
    #if defined(_STLPORT_VERSION) && _STLPORT_VERSION >= 0x510
        #ifndef HAVE_VSWPRINTF
            #define HAVE_VSWPRINTF
        #endif
    #elif defined(HAVE_VSWPRINTF)
        /* can't use non-standard vswprintf() */
        #undef HAVE_VSWPRINTF
    #endif
#endif /* __MINGW32__ */

#if defined(__WATCOMC__)
    #define HAVE_VSWPRINTF 1
#endif

#if wxUSE_PRINTF_POS_PARAMS
    /*
        The systems where vsnprintf() supports positional parameters should
        define the HAVE_UNIX98_PRINTF symbol.

        On systems which don't (e.g. Windows) we are forced to use
        our wxVsnprintf() implementation.
    */
    #if defined(HAVE_UNIX98_PRINTF)
        #ifdef HAVE_VSWPRINTF
            #define wxCRT_VsnprintfW   vswprintf
        #endif
        #ifdef HAVE_BROKEN_VSNPRINTF_DECL
            #define wxCRT_VsnprintfA    wx_fixed_vsnprintf
        #else
            #define wxCRT_VsnprintfA    vsnprintf
        #endif
    #else /* !HAVE_UNIX98_PRINTF */
        /*
            The only compiler with positional parameters support under Windows
            is VC++ 8.0 which provides a new xxprintf_p() functions family.
            The 2003 PSDK includes a slightly earlier version of VC8 than the
            main release and does not have the printf_p functions.
         */
        #if defined _MSC_FULL_VER && _MSC_FULL_VER >= 140050727 && !defined __WXWINCE__
            #define wxCRT_VsnprintfA    _vsprintf_p
            #define wxCRT_VsnprintfW    _vswprintf_p
        #endif
    #endif /* HAVE_UNIX98_PRINTF/!HAVE_UNIX98_PRINTF */
#else /* !wxUSE_PRINTF_POS_PARAMS */
    /*
       We always want to define safe snprintf() function to be used instead of
       sprintf(). Some compilers already have it (or rather vsnprintf() which
       we really need...), otherwise we implement it using our own printf()
       code.

       We define function with a trailing underscore here because the real one
       is a wrapper around it as explained below
     */

    #if defined(__VISUALC__) || \
            (defined(__BORLANDC__) && __BORLANDC__ >= 0x540)
        #define wxCRT_VsnprintfA    _vsnprintf
        #define wxCRT_VsnprintfW    _vsnwprintf
    #else
        #if defined(HAVE__VSNWPRINTF)
            #define wxCRT_VsnprintfW    _vsnwprintf
        #elif defined(HAVE_VSWPRINTF)
            #define wxCRT_VsnprintfW     vswprintf
        #elif defined(__WATCOMC__)
            #define wxCRT_VsnprintfW    _vsnwprintf
        #endif

        #if defined(HAVE_VSNPRINTF) \
            || defined(__WATCOMC__)
            #ifdef HAVE_BROKEN_VSNPRINTF_DECL
                #define wxCRT_VsnprintfA    wx_fixed_vsnprintf
            #else
                #define wxCRT_VsnprintfA    vsnprintf
            #endif
        #endif
    #endif
#endif /* wxUSE_PRINTF_POS_PARAMS/!wxUSE_PRINTF_POS_PARAMS */

#ifndef wxCRT_VsnprintfW
    /* no (suitable) vsnprintf(), cook our own */
    WXDLLIMPEXP_BASE int
    wxCRT_VsnprintfW(wchar_t *buf, size_t len, const wchar_t *format, va_list argptr);
    #define wxUSE_WXVSNPRINTFW 1
#else
    #define wxUSE_WXVSNPRINTFW 0
#endif

#ifndef wxCRT_VsnprintfA
        /* no (suitable) vsnprintf(), cook our own */
        WXDLLIMPEXP_BASE int
        wxCRT_VsnprintfA(char *buf, size_t len, const char *format, va_list argptr);
        #define wxUSE_WXVSNPRINTFA 1
#else
        #define wxUSE_WXVSNPRINTFA 0
#endif

// for wxString code, define wxUSE_WXVSNPRINTF to indicate that wx
// implementation is used no matter what (in UTF-8 build, either *A or *W
// version may be called):
#if !wxUSE_UNICODE
    #define wxUSE_WXVSNPRINTF wxUSE_WXVSNPRINTFA
#elif wxUSE_UNICODE_WCHAR
    #define wxUSE_WXVSNPRINTF wxUSE_WXVSNPRINTFW
#elif wxUSE_UTF8_LOCALE_ONLY
    #define wxUSE_WXVSNPRINTF wxUSE_WXVSNPRINTFA
#else // UTF-8 under any locale
    #define wxUSE_WXVSNPRINTF (wxUSE_WXVSNPRINTFA && wxUSE_WXVSNPRINTFW)
#endif

#define wxCRT_FprintfA       fprintf
#define wxCRT_PrintfA        printf
#define wxCRT_VfprintfA      vfprintf
#define wxCRT_VprintfA       vprintf
#define wxCRT_VsprintfA      vsprintf

/*
   In Unicode mode we need to have all standard functions such as wprintf() and
   so on but not all systems have them so use our own implementations in this
   case.
 */
#if !defined(wxHAVE_TCHAR_SUPPORT) && !defined(HAVE_WPRINTF)
    #define wxNEED_WPRINTF
#endif
#if !defined(wxHAVE_TCHAR_SUPPORT) && !defined(HAVE_VSWSCANF) && defined(HAVE_VSSCANF)
    #define wxNEED_VSWSCANF
#endif


#if defined(wxNEED_WPRINTF)
    /*
        we need to implement all wide character printf functions either because
        we don't have them at all or because they don't have the semantics we
        need
     */
    int wxCRT_PrintfW( const wchar_t *format, ... );
    int wxCRT_FprintfW( FILE *stream, const wchar_t *format, ... );
    int wxCRT_VfprintfW( FILE *stream, const wchar_t *format, va_list ap );
    int wxCRT_VprintfW( const wchar_t *format, va_list ap );
    int wxCRT_VsprintfW( wchar_t *str, const wchar_t *format, va_list ap );
#else /* !wxNEED_WPRINTF */
    #define wxCRT_FprintfW      fwprintf
    #define wxCRT_PrintfW       wprintf
    #define wxCRT_VfprintfW     vfwprintf
    #define wxCRT_VprintfW      vwprintf

    #if defined(__WINDOWS__) && !defined(HAVE_VSWPRINTF)
        // only non-standard vswprintf() without buffer size argument can be used here
        #define  wxCRT_VsprintfW     vswprintf
    #endif
#endif /* wxNEED_WPRINTF */


/* Required for wxScanf() etc. */
#define wxCRT_ScanfA     scanf
#define wxCRT_SscanfA    sscanf
#define wxCRT_FscanfA    fscanf

/* vsscanf() may have a wrong declaration with non-const first parameter, fix
 * this by wrapping it if necessary. */
#if defined __cplusplus && defined HAVE_BROKEN_VSSCANF_DECL
    inline int wxCRT_VsscanfA(const char *str, const char *format, va_list ap)
    {
        return vsscanf(const_cast<char *>(str), format, ap);
    }
#else
    #define wxCRT_VsscanfA   vsscanf
#endif

#if defined(wxNEED_WPRINTF)
    int wxCRT_ScanfW(const wchar_t *format, ...);
    int wxCRT_SscanfW(const wchar_t *str, const wchar_t *format, ...);
    int wxCRT_FscanfW(FILE *stream, const wchar_t *format, ...);
#else
    #define wxCRT_ScanfW     wxVMS_USE_STD wscanf
    #define wxCRT_SscanfW    wxVMS_USE_STD swscanf
    #define wxCRT_FscanfW    wxVMS_USE_STD fwscanf
#endif
#ifdef wxNEED_VSWSCANF
    int wxCRT_VsscanfW(const wchar_t *str, const wchar_t *format, va_list ap);
#else
    #define wxCRT_VsscanfW   wxVMS_USE_STD vswscanf
#endif

// ----------------------------------------------------------------------------
// user-friendly wrappers to CRT functions
// ----------------------------------------------------------------------------

#ifdef __WATCOMC__
    // workaround for http://bugzilla.openwatcom.org/show_bug.cgi?id=351
    #define wxPrintf    wxPrintf_Impl
    #define wxFprintf   wxFprintf_Impl
    #define wxSprintf   wxSprintf_Impl
    #define wxSnprintf  wxSnprintf_Impl
#endif

    // FIXME-UTF8: remove this
#if wxUSE_UNICODE
    #define wxCRT_PrintfNative wxCRT_PrintfW
    #define wxCRT_FprintfNative wxCRT_FprintfW
#else
    #define wxCRT_PrintfNative wxCRT_PrintfA
    #define wxCRT_FprintfNative wxCRT_FprintfA
#endif


WX_DEFINE_VARARG_FUNC_SANS_N0(int, wxPrintf, 1, (const wxFormatString&),
                              wxCRT_PrintfNative, wxCRT_PrintfA)
inline int wxPrintf(const wxFormatString& s)
{
    return wxPrintf("%s", s.InputAsString());
}

WX_DEFINE_VARARG_FUNC_SANS_N0(int, wxFprintf, 2, (FILE*, const wxFormatString&),
                              wxCRT_FprintfNative, wxCRT_FprintfA)
inline int wxFprintf(FILE *f, const wxFormatString& s)
{
    return wxFprintf(f, "%s", s.InputAsString());
}

// va_list versions of printf functions simply forward to the respective
// CRT function; note that they assume that va_list was created using
// wxArgNormalizer<T>!
#if wxUSE_UNICODE_UTF8
    #if wxUSE_UTF8_LOCALE_ONLY
        #define WX_VARARG_VFOO_IMPL(args, implW, implA)              \
            return implA args
    #else
        #define WX_VARARG_VFOO_IMPL(args, implW, implA)              \
            if ( wxLocaleIsUtf8 ) return implA args;                 \
            else return implW args
    #endif
#elif wxUSE_UNICODE_WCHAR
    #define WX_VARARG_VFOO_IMPL(args, implW, implA)                  \
        return implW args
#else // ANSI
    #define WX_VARARG_VFOO_IMPL(args, implW, implA)                  \
        return implA args
#endif

inline int
wxVprintf(const wxString& format, va_list ap)
{
    WX_VARARG_VFOO_IMPL((wxFormatString(format), ap),
                        wxCRT_VprintfW, wxCRT_VprintfA);
}

inline int
wxVfprintf(FILE *f, const wxString& format, va_list ap)
{
    WX_VARARG_VFOO_IMPL((f, wxFormatString(format), ap),
                        wxCRT_VfprintfW, wxCRT_VfprintfA);
}

#undef WX_VARARG_VFOO_IMPL


// wxSprintf() and friends have to be implemented in two forms, one for
// writing to char* buffer and one for writing to wchar_t*:

#if !wxUSE_UTF8_LOCALE_ONLY
int WXDLLIMPEXP_BASE wxDoSprintfWchar(char *str, const wxChar *format, ...);
#endif
#if wxUSE_UNICODE_UTF8
int WXDLLIMPEXP_BASE wxDoSprintfUtf8(char *str, const char *format, ...);
#endif
WX_DEFINE_VARARG_FUNC(int, wxSprintf, 2, (char*, const wxFormatString&),
                      wxDoSprintfWchar, wxDoSprintfUtf8)

int WXDLLIMPEXP_BASE
wxVsprintf(char *str, const wxString& format, va_list argptr);

#if !wxUSE_UTF8_LOCALE_ONLY
int WXDLLIMPEXP_BASE wxDoSnprintfWchar(char *str, size_t size, const wxChar *format, ...);
#endif
#if wxUSE_UNICODE_UTF8
int WXDLLIMPEXP_BASE wxDoSnprintfUtf8(char *str, size_t size, const char *format, ...);
#endif
WX_DEFINE_VARARG_FUNC(int, wxSnprintf, 3, (char*, size_t, const wxFormatString&),
                      wxDoSnprintfWchar, wxDoSnprintfUtf8)

int WXDLLIMPEXP_BASE
wxVsnprintf(char *str, size_t size, const wxString& format, va_list argptr);

#if wxUSE_UNICODE

#if !wxUSE_UTF8_LOCALE_ONLY
int WXDLLIMPEXP_BASE wxDoSprintfWchar(wchar_t *str, const wxChar *format, ...);
#endif
#if wxUSE_UNICODE_UTF8
int WXDLLIMPEXP_BASE wxDoSprintfUtf8(wchar_t *str, const char *format, ...);
#endif
WX_DEFINE_VARARG_FUNC(int, wxSprintf, 2, (wchar_t*, const wxFormatString&),
                      wxDoSprintfWchar, wxDoSprintfUtf8)

int WXDLLIMPEXP_BASE
wxVsprintf(wchar_t *str, const wxString& format, va_list argptr);

#if !wxUSE_UTF8_LOCALE_ONLY
int WXDLLIMPEXP_BASE wxDoSnprintfWchar(wchar_t *str, size_t size, const wxChar *format, ...);
#endif
#if wxUSE_UNICODE_UTF8
int WXDLLIMPEXP_BASE wxDoSnprintfUtf8(wchar_t *str, size_t size, const char *format, ...);
#endif
WX_DEFINE_VARARG_FUNC(int, wxSnprintf, 3, (wchar_t*, size_t, const wxFormatString&),
                      wxDoSnprintfWchar, wxDoSnprintfUtf8)

int WXDLLIMPEXP_BASE
wxVsnprintf(wchar_t *str, size_t size, const wxString& format, va_list argptr);

#endif // wxUSE_UNICODE

#ifdef __WATCOMC__
    // workaround for http://bugzilla.openwatcom.org/show_bug.cgi?id=351
    //
    // fortunately, OpenWatcom implements __VA_ARGS__, so we can provide macros
    // that cast the format argument to wxString:
    #undef wxPrintf
    #undef wxFprintf
    #undef wxSprintf
    #undef wxSnprintf

    #define wxPrintf(fmt, ...) \
            wxPrintf_Impl(wxFormatString(fmt), __VA_ARGS__)
    #define wxFprintf(f, fmt, ...) \
            wxFprintf_Impl(f, wxFormatString(fmt), __VA_ARGS__)
    #define wxSprintf(s, fmt, ...) \
            wxSprintf_Impl(s, wxFormatString(fmt), __VA_ARGS__)
    #define wxSnprintf(s, n, fmt, ...) \
            wxSnprintf_Impl(s, n, wxFormatString(fmt), __VA_ARGS__)
#endif // __WATCOMC__


// We can't use wxArgNormalizer<T> for variadic arguments to wxScanf() etc.
// because they are writable, so instead of providing friendly template
// vararg-like functions, we just provide both char* and wchar_t* variants
// of these functions. The type of output variadic arguments for %s must match
// the type of 'str' and 'format' arguments.
//
// For compatibility with earlier wx versions, we also provide wxSscanf()
// version with the first argument (input string) wxString; for this version,
// the type of output string values is determined by the type of format string
// only.

#define _WX_SCANFUNC_EXTRACT_ARGS_1(x)   x
#define _WX_SCANFUNC_EXTRACT_ARGS_2(x,y) x, y
#define _WX_SCANFUNC_EXTRACT_ARGS(N, args) _WX_SCANFUNC_EXTRACT_ARGS_##N args

#define _WX_VARARG_PASS_WRITABLE(i) a##i

#define _WX_DEFINE_SCANFUNC(N, dummy1, name, impl, passfixed, numfixed, fixed)\
    template<_WX_VARARG_JOIN(N, _WX_VARARG_TEMPL)>                            \
    int name(_WX_SCANFUNC_EXTRACT_ARGS(numfixed, fixed),                      \
             _WX_VARARG_JOIN(N, _WX_VARARG_ARG))                              \
    {                                                                         \
        return  impl(_WX_SCANFUNC_EXTRACT_ARGS(numfixed, passfixed),          \
                     _WX_VARARG_JOIN(N, _WX_VARARG_PASS_WRITABLE));           \
    }

#define WX_DEFINE_SCANFUNC(name, numfixed, fixed, impl, passfixed)            \
    _WX_VARARG_ITER(_WX_VARARG_MAX_ARGS,                                      \
                    _WX_DEFINE_SCANFUNC,                                      \
                    dummy1, name, impl, passfixed, numfixed, fixed)

// this is needed to normalize the format string, see src/common/strvararg.cpp
// for more details
#ifdef __WINDOWS__
    #define wxScanfConvertFormatW(fmt) fmt
#else
    const wxScopedWCharBuffer
    WXDLLIMPEXP_BASE wxScanfConvertFormatW(const wchar_t *format);
#endif

WX_DEFINE_SCANFUNC(wxScanf, 1, (const char *format),
                   wxCRT_ScanfA, (format))
WX_DEFINE_SCANFUNC(wxScanf, 1, (const wchar_t *format),
                   wxCRT_ScanfW, (wxScanfConvertFormatW(format)))

WX_DEFINE_SCANFUNC(wxFscanf, 2, (FILE *stream, const char *format),
                   wxCRT_FscanfA, (stream, format))
WX_DEFINE_SCANFUNC(wxFscanf, 2, (FILE *stream, const wchar_t *format),
                   wxCRT_FscanfW, (stream, wxScanfConvertFormatW(format)))

WX_DEFINE_SCANFUNC(wxSscanf, 2, (const char *str, const char *format),
                   wxCRT_SscanfA, (str, format))
WX_DEFINE_SCANFUNC(wxSscanf, 2, (const wchar_t *str, const wchar_t *format),
                   wxCRT_SscanfW, (str, wxScanfConvertFormatW(format)))
WX_DEFINE_SCANFUNC(wxSscanf, 2, (const wxScopedCharBuffer& str, const char *format),
                   wxCRT_SscanfA, (str.data(), format))
WX_DEFINE_SCANFUNC(wxSscanf, 2, (const wxScopedWCharBuffer& str, const wchar_t *format),
                   wxCRT_SscanfW, (str.data(), wxScanfConvertFormatW(format)))
WX_DEFINE_SCANFUNC(wxSscanf, 2, (const wxString& str, const char *format),
                   wxCRT_SscanfA, (str.mb_str(), format))
WX_DEFINE_SCANFUNC(wxSscanf, 2, (const wxString& str, const wchar_t *format),
                   wxCRT_SscanfW, (str.wc_str(), wxScanfConvertFormatW(format)))
WX_DEFINE_SCANFUNC(wxSscanf, 2, (const wxCStrData& str, const char *format),
                   wxCRT_SscanfA, (str.AsCharBuf(), format))
WX_DEFINE_SCANFUNC(wxSscanf, 2, (const wxCStrData& str, const wchar_t *format),
                   wxCRT_SscanfW, (str.AsWCharBuf(), wxScanfConvertFormatW(format)))

// Visual C++ doesn't provide vsscanf()
#ifndef __VISUALC___
int WXDLLIMPEXP_BASE wxVsscanf(const char *str, const char *format, va_list ap);
int WXDLLIMPEXP_BASE wxVsscanf(const wchar_t *str, const wchar_t *format, va_list ap);
int WXDLLIMPEXP_BASE wxVsscanf(const wxScopedCharBuffer& str, const char *format, va_list ap);
int WXDLLIMPEXP_BASE wxVsscanf(const wxScopedWCharBuffer& str, const wchar_t *format, va_list ap);
int WXDLLIMPEXP_BASE wxVsscanf(const wxString& str, const char *format, va_list ap);
int WXDLLIMPEXP_BASE wxVsscanf(const wxString& str, const wchar_t *format, va_list ap);
int WXDLLIMPEXP_BASE wxVsscanf(const wxCStrData& str, const char *format, va_list ap);
int WXDLLIMPEXP_BASE wxVsscanf(const wxCStrData& str, const wchar_t *format, va_list ap);
#endif // !__VISUALC__

#endif /* _WX_WXCRTVARARG_H_ */
