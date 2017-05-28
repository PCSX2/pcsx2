/////////////////////////////////////////////////////////////////////////////
// Name:        wx/private/wxprintf.h
// Purpose:     wxWidgets wxPrintf() implementation
// Author:      Ove Kaven
// Modified by: Ron Lee, Francesco Montorsi
// Created:     09/04/99
// Copyright:   (c) wxWidgets copyright
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_WXPRINTF_H_
#define _WX_PRIVATE_WXPRINTF_H_

// ---------------------------------------------------------------------------
// headers and macros
// ---------------------------------------------------------------------------

#include "wx/crt.h"
#include "wx/log.h"
#include "wx/utils.h"

#include <string.h>

// prefer snprintf over sprintf
#if defined(__VISUALC__) || \
        (defined(__BORLANDC__) && __BORLANDC__ >= 0x540)
    #define system_sprintf(buff, max, flags, data)      \
        ::_snprintf(buff, max, flags, data)
#elif defined(HAVE_SNPRINTF)
    #define system_sprintf(buff, max, flags, data)      \
        ::snprintf(buff, max, flags, data)
#else       // NB: at least sprintf() should always be available
    // since 'max' is not used in this case, wxVsnprintf() should always
    // ensure that 'buff' is big enough for all common needs
    // (see wxMAX_SVNPRINTF_FLAGBUFFER_LEN and wxMAX_SVNPRINTF_SCRATCHBUFFER_LEN)
    #define system_sprintf(buff, max, flags, data)      \
        ::sprintf(buff, flags, data)

    #define SYSTEM_SPRINTF_IS_UNSAFE
#endif

// ---------------------------------------------------------------------------
// printf format string parsing
// ---------------------------------------------------------------------------

// some limits of our implementation
#define wxMAX_SVNPRINTF_ARGUMENTS         64
#define wxMAX_SVNPRINTF_FLAGBUFFER_LEN    32
#define wxMAX_SVNPRINTF_SCRATCHBUFFER_LEN   512


// the conversion specifiers accepted by wxCRT_VsnprintfW
enum wxPrintfArgType
{
    wxPAT_INVALID = -1,

    wxPAT_INT,          // %d, %i, %o, %u, %x, %X
    wxPAT_LONGINT,      // %ld, etc
#ifdef wxLongLong_t
    wxPAT_LONGLONGINT,  // %Ld, etc
#endif
    wxPAT_SIZET,        // %zd, etc

    wxPAT_DOUBLE,       // %e, %E, %f, %g, %G
    wxPAT_LONGDOUBLE,   // %le, etc

    wxPAT_POINTER,      // %p

    wxPAT_CHAR,         // %hc  (in ANSI mode: %c, too)
    wxPAT_WCHAR,        // %lc  (in Unicode mode: %c, too)

    wxPAT_PCHAR,        // %s   (related to a char *)
    wxPAT_PWCHAR,       // %s   (related to a wchar_t *)

    wxPAT_NINT,         // %n
    wxPAT_NSHORTINT,    // %hn
    wxPAT_NLONGINT,     // %ln

    wxPAT_STAR          // '*' used for width or precision
};

// an argument passed to wxCRT_VsnprintfW
union wxPrintfArg
{
    int pad_int;                        //  %d, %i, %o, %u, %x, %X
    long int pad_longint;               // %ld, etc
#ifdef wxLongLong_t
    wxLongLong_t pad_longlongint;       // %Ld, etc
#endif
    size_t pad_sizet;                   // %zd, etc

    double pad_double;                  // %e, %E, %f, %g, %G
    long double pad_longdouble;         // %le, etc

    void *pad_pointer;                  // %p

    char pad_char;                      // %hc  (in ANSI mode: %c, too)
    wchar_t pad_wchar;                  // %lc  (in Unicode mode: %c, too)

    void *pad_str;                      // %s

    int *pad_nint;                      // %n
    short int *pad_nshortint;           // %hn
    long int *pad_nlongint;             // %ln
};

// helper for converting string into either char* or wchar_t* depending
// on the type of wxPrintfConvSpec<T> instantiation:
template<typename CharType> struct wxPrintfStringHelper {};

template<> struct wxPrintfStringHelper<char>
{
    typedef const wxWX2MBbuf ConvertedType;
    static ConvertedType Convert(const wxString& s) { return s.mb_str(); }
};

template<> struct wxPrintfStringHelper<wchar_t>
{
    typedef const wxWX2WCbuf ConvertedType;
    static ConvertedType Convert(const wxString& s) { return s.wc_str(); }
};


// Contains parsed data relative to a conversion specifier given to
// wxCRT_VsnprintfW and parsed from the format string
// NOTE: in C++ there is almost no difference between struct & classes thus
//       there is no performance gain by using a struct here...
template<typename CharType>
class wxPrintfConvSpec
{
public:

    // the position of the argument relative to this conversion specifier
    size_t m_pos;

    // the type of this conversion specifier
    wxPrintfArgType m_type;

    // the minimum and maximum width
    // when one of this var is set to -1 it means: use the following argument
    // in the stack as minimum/maximum width for this conversion specifier
    int m_nMinWidth, m_nMaxWidth;

    // does the argument need to the be aligned to left ?
    bool m_bAlignLeft;

    // pointer to the '%' of this conversion specifier in the format string
    // NOTE: this points somewhere in the string given to the Parse() function -
    //       it's task of the caller ensure that memory is still valid !
    const CharType *m_pArgPos;

    // pointer to the last character of this conversion specifier in the
    // format string
    // NOTE: this points somewhere in the string given to the Parse() function -
    //       it's task of the caller ensure that memory is still valid !
    const CharType *m_pArgEnd;

    // a little buffer where formatting flags like #+\.hlqLz are stored by Parse()
    // for use in Process()
    char m_szFlags[wxMAX_SVNPRINTF_FLAGBUFFER_LEN];


public:

    // we don't declare this as a constructor otherwise it would be called
    // automatically and we don't want this: to be optimized, wxCRT_VsnprintfW
    // calls this function only on really-used instances of this class.
    void Init();

    // Parses the first conversion specifier in the given string, which must
    // begin with a '%'. Returns false if the first '%' does not introduce a
    // (valid) conversion specifier and thus should be ignored.
    bool Parse(const CharType *format);

    // Process this conversion specifier and puts the result in the given
    // buffer. Returns the number of characters written in 'buf' or -1 if
    // there's not enough space.
    int Process(CharType *buf, size_t lenMax, wxPrintfArg *p, size_t written);

    // Loads the argument of this conversion specifier from given va_list.
    bool LoadArg(wxPrintfArg *p, va_list &argptr);

private:
    // An helper function of LoadArg() which is used to handle the '*' flag
    void ReplaceAsteriskWith(int w);
};

template<typename CharType>
void wxPrintfConvSpec<CharType>::Init()
{
    m_nMinWidth = 0;
    m_nMaxWidth = 0xFFFF;
    m_pos = 0;
    m_bAlignLeft = false;
    m_pArgPos = m_pArgEnd = NULL;
    m_type = wxPAT_INVALID;

    memset(m_szFlags, 0, sizeof(m_szFlags));
    // this character will never be removed from m_szFlags array and
    // is important when calling sprintf() in wxPrintfConvSpec::Process() !
    m_szFlags[0] = '%';
}

template<typename CharType>
bool wxPrintfConvSpec<CharType>::Parse(const CharType *format)
{
    bool done = false;

    // temporary parse data
    size_t flagofs = 1;
    bool in_prec,       // true if we found the dot in some previous iteration
         prec_dot;      // true if the dot has been already added to m_szFlags
    int ilen = 0;

    m_bAlignLeft = in_prec = prec_dot = false;
    m_pArgPos = m_pArgEnd = format;
    do
    {
#define CHECK_PREC \
        if (in_prec && !prec_dot) \
        { \
            m_szFlags[flagofs++] = '.'; \
            prec_dot = true; \
        }

        // what follows '%'?
        const CharType ch = *(++m_pArgEnd);
        switch ( ch )
        {
            case wxT('\0'):
                return false;       // not really an argument

            case wxT('%'):
                return false;       // not really an argument

            case wxT('#'):
            case wxT('0'):
            case wxT(' '):
            case wxT('+'):
            case wxT('\''):
                CHECK_PREC
                m_szFlags[flagofs++] = char(ch);
                break;

            case wxT('-'):
                CHECK_PREC
                m_bAlignLeft = true;
                m_szFlags[flagofs++] = char(ch);
                break;

            case wxT('.'):
                // don't use CHECK_PREC here to avoid warning about the value
                // assigned to prec_dot inside it being never used (because
                // overwritten just below) from Borland in release build
                if (in_prec && !prec_dot)
                    m_szFlags[flagofs++] = '.';
                in_prec = true;
                prec_dot = false;
                m_nMaxWidth = 0;
                // dot will be auto-added to m_szFlags if non-negative
                // number follows
                break;

            case wxT('h'):
                ilen = -1;
                CHECK_PREC
                m_szFlags[flagofs++] = char(ch);
                break;

            case wxT('l'):
                // NB: it's safe to use flagofs-1 as flagofs always start from 1
                if (m_szFlags[flagofs-1] == 'l')       // 'll' modifier is the same as 'L' or 'q'
                    ilen = 2;
                else
                ilen = 1;
                CHECK_PREC
                m_szFlags[flagofs++] = char(ch);
                break;

            case wxT('q'):
            case wxT('L'):
                ilen = 2;
                CHECK_PREC
                m_szFlags[flagofs++] = char(ch);
                break;
#ifdef __WINDOWS__
            // under Windows we support the special '%I64' notation as longlong
            // integer conversion specifier for MSVC compatibility
            // (it behaves exactly as '%lli' or '%Li' or '%qi')
            case wxT('I'):
                if (*(m_pArgEnd+1) == wxT('6') &&
                    *(m_pArgEnd+2) == wxT('4'))
                {
                    m_pArgEnd++;
                    m_pArgEnd++;

                    ilen = 2;
                    CHECK_PREC
                    m_szFlags[flagofs++] = char(ch);
                    m_szFlags[flagofs++] = '6';
                    m_szFlags[flagofs++] = '4';
                    break;
                }
                // else: fall-through, 'I' is MSVC equivalent of C99 'z'
#endif      // __WINDOWS__

            case wxT('z'):
            case wxT('Z'):
                // 'z' is C99 standard for size_t and ptrdiff_t, 'Z' was used
                // for this purpose in libc5 and by wx <= 2.8
                ilen = 3;
                CHECK_PREC
                m_szFlags[flagofs++] = char(ch);
                break;

            case wxT('*'):
                if (in_prec)
                {
                    CHECK_PREC

                    // tell Process() to use the next argument
                    // in the stack as maxwidth...
                    m_nMaxWidth = -1;
                }
                else
                {
                    // tell Process() to use the next argument
                    // in the stack as minwidth...
                    m_nMinWidth = -1;
                }

                // save the * in our formatting buffer...
                // will be replaced later by Process()
                m_szFlags[flagofs++] = char(ch);
                break;

            case wxT('1'): case wxT('2'): case wxT('3'):
            case wxT('4'): case wxT('5'): case wxT('6'):
            case wxT('7'): case wxT('8'): case wxT('9'):
                {
                    int len = 0;
                    CHECK_PREC
                    while ( (*m_pArgEnd >= CharType('0')) &&
                            (*m_pArgEnd <= CharType('9')) )
                    {
                        m_szFlags[flagofs++] = char(*m_pArgEnd);
                        len = len*10 + (*m_pArgEnd - wxT('0'));
                        m_pArgEnd++;
                    }

                    if (in_prec)
                        m_nMaxWidth = len;
                    else
                        m_nMinWidth = len;

                    m_pArgEnd--; // the main loop pre-increments n again
                }
                break;

            case wxT('$'):      // a positional parameter (e.g. %2$s) ?
                {
                    if (m_nMinWidth <= 0)
                        break;      // ignore this formatting flag as no
                                    // numbers are preceding it

                    // remove from m_szFlags all digits previously added
                    do {
                        flagofs--;
                    } while (m_szFlags[flagofs] >= '1' &&
                             m_szFlags[flagofs] <= '9');

                    // re-adjust the offset making it point to the
                    // next free char of m_szFlags
                    flagofs++;

                    m_pos = m_nMinWidth;
                    m_nMinWidth = 0;
                }
                break;

            case wxT('d'):
            case wxT('i'):
            case wxT('o'):
            case wxT('u'):
            case wxT('x'):
            case wxT('X'):
                CHECK_PREC
                m_szFlags[flagofs++] = char(ch);
                if (ilen == 0)
                    m_type = wxPAT_INT;
                else if (ilen == -1)
                    // NB: 'short int' value passed through '...'
                    //      is promoted to 'int', so we have to get
                    //      an int from stack even if we need a short
                    m_type = wxPAT_INT;
                else if (ilen == 1)
                    m_type = wxPAT_LONGINT;
                else if (ilen == 2)
#ifdef wxLongLong_t
                    m_type = wxPAT_LONGLONGINT;
#else // !wxLongLong_t
                    m_type = wxPAT_LONGINT;
#endif // wxLongLong_t/!wxLongLong_t
                else if (ilen == 3)
                    m_type = wxPAT_SIZET;
                done = true;
                break;

            case wxT('e'):
            case wxT('E'):
            case wxT('f'):
            case wxT('g'):
            case wxT('G'):
                CHECK_PREC
                m_szFlags[flagofs++] = char(ch);
                if (ilen == 2)
                    m_type = wxPAT_LONGDOUBLE;
                else
                    m_type = wxPAT_DOUBLE;
                done = true;
                break;

            case wxT('p'):
                m_type = wxPAT_POINTER;
                m_szFlags[flagofs++] = char(ch);
                done = true;
                break;

            case wxT('c'):
                if (ilen == -1)
                {
                    // in Unicode mode %hc == ANSI character
                    // and in ANSI mode, %hc == %c == ANSI...
                    m_type = wxPAT_CHAR;
                }
                else if (ilen == 1)
                {
                    // in ANSI mode %lc == Unicode character
                    // and in Unicode mode, %lc == %c == Unicode...
                    m_type = wxPAT_WCHAR;
                }
                else
                {
#if wxUSE_UNICODE
                    // in Unicode mode, %c == Unicode character
                    m_type = wxPAT_WCHAR;
#else
                    // in ANSI mode, %c == ANSI character
                    m_type = wxPAT_CHAR;
#endif
                }
                done = true;
                break;

            case wxT('s'):
                if (ilen == -1)
                {
                    // Unicode mode wx extension: we'll let %hs mean non-Unicode
                    // strings (when in ANSI mode, %s == %hs == ANSI string)
                    m_type = wxPAT_PCHAR;
                }
                else if (ilen == 1)
                {
                    // in Unicode mode, %ls == %s == Unicode string
                    // in ANSI mode, %ls == Unicode string
                    m_type = wxPAT_PWCHAR;
                }
                else
                {
#if wxUSE_UNICODE
                    m_type = wxPAT_PWCHAR;
#else
                    m_type = wxPAT_PCHAR;
#endif
                }
                done = true;
                break;

            case wxT('n'):
                if (ilen == 0)
                    m_type = wxPAT_NINT;
                else if (ilen == -1)
                    m_type = wxPAT_NSHORTINT;
                else if (ilen >= 1)
                    m_type = wxPAT_NLONGINT;
                done = true;
                break;

            default:
                // bad format, don't consider this an argument;
                // leave it unchanged
                return false;
        }

        if (flagofs == wxMAX_SVNPRINTF_FLAGBUFFER_LEN)
        {
            wxLogDebug(wxT("Too many flags specified for a single conversion specifier!"));
            return false;
        }
    }
    while (!done);

    return true;        // parsing was successful
}

template<typename CharType>
void wxPrintfConvSpec<CharType>::ReplaceAsteriskWith(int width)
{
    char temp[wxMAX_SVNPRINTF_FLAGBUFFER_LEN];

    // find the first * in our flag buffer
    char *pwidth = strchr(m_szFlags, '*');
    wxCHECK_RET(pwidth, wxT("field width must be specified"));

    // save what follows the * (the +1 is to skip the asterisk itself!)
    strcpy(temp, pwidth+1);
    if (width < 0)
    {
        pwidth[0] = wxT('-');
        pwidth++;
    }

    // replace * with the actual integer given as width
#ifndef SYSTEM_SPRINTF_IS_UNSAFE
    int maxlen = (m_szFlags + wxMAX_SVNPRINTF_FLAGBUFFER_LEN - pwidth) /
                        sizeof(*m_szFlags);
#endif
    int offset = system_sprintf(pwidth, maxlen, "%d", abs(width));

    // restore after the expanded * what was following it
    strcpy(pwidth+offset, temp);
}

template<typename CharType>
bool wxPrintfConvSpec<CharType>::LoadArg(wxPrintfArg *p, va_list &argptr)
{
    // did the '*' width/precision specifier was used ?
    if (m_nMaxWidth == -1)
    {
        // take the maxwidth specifier from the stack
        m_nMaxWidth = va_arg(argptr, int);
        if (m_nMaxWidth < 0)
            m_nMaxWidth = 0;
        else
            ReplaceAsteriskWith(m_nMaxWidth);
    }

    if (m_nMinWidth == -1)
    {
        // take the minwidth specifier from the stack
        m_nMinWidth = va_arg(argptr, int);

        ReplaceAsteriskWith(m_nMinWidth);
        if (m_nMinWidth < 0)
        {
            m_bAlignLeft = !m_bAlignLeft;
            m_nMinWidth = -m_nMinWidth;
        }
    }

    switch (m_type) {
        case wxPAT_INT:
            p->pad_int = va_arg(argptr, int);
            break;
        case wxPAT_LONGINT:
            p->pad_longint = va_arg(argptr, long int);
            break;
#ifdef wxLongLong_t
        case wxPAT_LONGLONGINT:
            p->pad_longlongint = va_arg(argptr, wxLongLong_t);
            break;
#endif // wxLongLong_t
        case wxPAT_SIZET:
            p->pad_sizet = va_arg(argptr, size_t);
            break;
        case wxPAT_DOUBLE:
            p->pad_double = va_arg(argptr, double);
            break;
        case wxPAT_LONGDOUBLE:
            p->pad_longdouble = va_arg(argptr, long double);
            break;
        case wxPAT_POINTER:
            p->pad_pointer = va_arg(argptr, void *);
            break;

        case wxPAT_CHAR:
            p->pad_char = (char)va_arg(argptr, int);  // char is promoted to int when passed through '...'
            break;
        case wxPAT_WCHAR:
            p->pad_wchar = (wchar_t)va_arg(argptr, int);  // char is promoted to int when passed through '...'
            break;

        case wxPAT_PCHAR:
        case wxPAT_PWCHAR:
            p->pad_str = va_arg(argptr, void *);
            break;

        case wxPAT_NINT:
            p->pad_nint = va_arg(argptr, int *);
            break;
        case wxPAT_NSHORTINT:
            p->pad_nshortint = va_arg(argptr, short int *);
            break;
        case wxPAT_NLONGINT:
            p->pad_nlongint = va_arg(argptr, long int *);
            break;

        case wxPAT_STAR:
            // this will be handled as part of the next argument
            return true;

        case wxPAT_INVALID:
        default:
            return false;
    }

    return true;    // loading was successful
}

template<typename CharType>
int wxPrintfConvSpec<CharType>::Process(CharType *buf, size_t lenMax, wxPrintfArg *p, size_t written)
{
    // buffer to avoid dynamic memory allocation each time for small strings;
    // note that this buffer is used only to hold results of number formatting,
    // %s directly writes user's string in buf, without using szScratch
    char szScratch[wxMAX_SVNPRINTF_SCRATCHBUFFER_LEN];
    size_t lenScratch = 0, lenCur = 0;

#define APPEND_CH(ch) \
                { \
                    if ( lenCur == lenMax ) \
                        return -1; \
                    \
                    buf[lenCur++] = ch; \
                }

    switch ( m_type )
    {
        case wxPAT_INT:
            lenScratch = system_sprintf(szScratch, wxMAX_SVNPRINTF_SCRATCHBUFFER_LEN, m_szFlags, p->pad_int);
            break;

        case wxPAT_LONGINT:
            lenScratch = system_sprintf(szScratch, wxMAX_SVNPRINTF_SCRATCHBUFFER_LEN, m_szFlags, p->pad_longint);
            break;

#ifdef wxLongLong_t
        case wxPAT_LONGLONGINT:
            lenScratch = system_sprintf(szScratch, wxMAX_SVNPRINTF_SCRATCHBUFFER_LEN, m_szFlags, p->pad_longlongint);
            break;
#endif // SIZEOF_LONG_LONG

        case wxPAT_SIZET:
            lenScratch = system_sprintf(szScratch, wxMAX_SVNPRINTF_SCRATCHBUFFER_LEN, m_szFlags, p->pad_sizet);
            break;

        case wxPAT_LONGDOUBLE:
            lenScratch = system_sprintf(szScratch, wxMAX_SVNPRINTF_SCRATCHBUFFER_LEN, m_szFlags, p->pad_longdouble);
            break;

        case wxPAT_DOUBLE:
            lenScratch = system_sprintf(szScratch, wxMAX_SVNPRINTF_SCRATCHBUFFER_LEN, m_szFlags, p->pad_double);
            break;

        case wxPAT_POINTER:
            lenScratch = system_sprintf(szScratch, wxMAX_SVNPRINTF_SCRATCHBUFFER_LEN, m_szFlags, p->pad_pointer);
            break;

        case wxPAT_CHAR:
        case wxPAT_WCHAR:
            {
                wxUniChar ch;
                if (m_type == wxPAT_CHAR)
                    ch = p->pad_char;
                else // m_type == wxPAT_WCHAR
                    ch = p->pad_wchar;

                CharType val = ch;

                size_t i;

                if (!m_bAlignLeft)
                    for (i = 1; i < (size_t)m_nMinWidth; i++)
                        APPEND_CH(wxT(' '));

                APPEND_CH(val);

                if (m_bAlignLeft)
                    for (i = 1; i < (size_t)m_nMinWidth; i++)
                        APPEND_CH(wxT(' '));
            }
            break;

        case wxPAT_PCHAR:
        case wxPAT_PWCHAR:
            {
                wxString s;
                if ( !p->pad_str )
                {
                    if ( m_nMaxWidth >= 6 )
                        s = wxT("(null)");
                }
                else if (m_type == wxPAT_PCHAR)
                    s.assign(static_cast<const char *>(p->pad_str));
                else // m_type == wxPAT_PWCHAR
                    s.assign(static_cast<const wchar_t *>(p->pad_str));

                typename wxPrintfStringHelper<CharType>::ConvertedType strbuf(
                        wxPrintfStringHelper<CharType>::Convert(s));

                // at this point we are sure that m_nMaxWidth is positive or
                // null (see top of wxPrintfConvSpec::LoadArg)
                int len = wxMin((unsigned int)m_nMaxWidth, wxStrlen(strbuf));

                int i;

                if (!m_bAlignLeft)
                {
                    for (i = len; i < m_nMinWidth; i++)
                        APPEND_CH(wxT(' '));
                }

                len = wxMin((unsigned int)len, lenMax-lenCur);
                wxStrncpy(buf+lenCur, strbuf, len);
                lenCur += len;

                if (m_bAlignLeft)
                {
                    for (i = len; i < m_nMinWidth; i++)
                        APPEND_CH(wxT(' '));
                }
            }
            break;

        case wxPAT_NINT:
            *p->pad_nint = written;
            break;

        case wxPAT_NSHORTINT:
            *p->pad_nshortint = (short int)written;
            break;

        case wxPAT_NLONGINT:
            *p->pad_nlongint = written;
            break;

        case wxPAT_INVALID:
        default:
            return -1;
    }

    // if we used system's sprintf() then we now need to append the s_szScratch
    // buffer to the given one...
    switch (m_type)
    {
        case wxPAT_INT:
        case wxPAT_LONGINT:
#ifdef wxLongLong_t
        case wxPAT_LONGLONGINT:
#endif
        case wxPAT_SIZET:
        case wxPAT_LONGDOUBLE:
        case wxPAT_DOUBLE:
        case wxPAT_POINTER:
            wxASSERT(lenScratch < wxMAX_SVNPRINTF_SCRATCHBUFFER_LEN);
            // NB: 1) we can compare lenMax (for CharType*, i.e. possibly
            //        wchar_t*) with lenScratch (char*) because this code is
            //        formatting integers and that will have the same length
            //        even in UTF-8 (the only case when char* length may be
            //        more than wchar_t* length of the same string)
            //     2) wxStrncpy converts the 2nd argument to 1st argument's
            //        type transparently if their types differ, so this code
            //        works for both instantiations
            if (lenMax < lenScratch)
            {
                // fill output buffer and then return -1
                wxStrncpy(buf, szScratch, lenMax);
                return -1;
            }
            wxStrncpy(buf, szScratch, lenScratch);
            lenCur += lenScratch;
            break;

        default:
            break;      // all other cases were completed previously
    }

    return lenCur;
}


// helper that parses format string
template<typename CharType>
struct wxPrintfConvSpecParser
{
    typedef wxPrintfConvSpec<CharType> ConvSpec;

    wxPrintfConvSpecParser(const CharType *fmt)
    {
        nargs = 0;
        posarg_present =
        nonposarg_present = false;

        memset(pspec, 0, sizeof(pspec));

        // parse the format string
        for ( const CharType *toparse = fmt; *toparse != wxT('\0'); toparse++ )
        {
            // skip everything except format specifications
            if ( *toparse != '%' )
                continue;

            // also skip escaped percent signs
            if ( toparse[1] == '%' )
            {
                toparse++;
                continue;
            }

            ConvSpec *spec = &specs[nargs];
            spec->Init();

            // attempt to parse this format specification
            if ( !spec->Parse(toparse) )
                continue;

            // advance to the end of this specifier
            toparse = spec->m_pArgEnd;

            // special handling for specifications including asterisks: we need
            // to reserve an extra slot (or two if asterisks were used for both
            // width and precision) in specs array in this case
            if ( const char *f = strchr(spec->m_szFlags, '*') )
            {
                unsigned numAsterisks = 1;
                if ( strchr(++f, '*') )
                    numAsterisks++;

                for ( unsigned n = 0; n < numAsterisks; n++ )
                {
                    if ( nargs++ == wxMAX_SVNPRINTF_ARGUMENTS )
                        break;

                    // TODO: we need to support specifiers of the form "%2$*1$s"
                    // (this is the same as "%*s") as if any positional arguments
                    // are used all asterisks must be positional as well but this
                    // requires a lot of changes in this code (basically we'd need
                    // to rewrite Parse() to return "*" and conversion itself as
                    // separate entries)
                    if ( posarg_present )
                    {
                        wxFAIL_MSG
                        (
                            wxString::Format
                            (
                                "Format string \"%s\" uses both positional "
                                "parameters and '*' but this is not currently "
                                "supported by this implementation, sorry.",
                                fmt
                            )
                        );
                    }

                    specs[nargs] = *spec;

                    // make an entry for '*' and point to it from pspec
                    spec->Init();
                    spec->m_type = wxPAT_STAR;
                    pspec[nargs - 1] = spec;

                    spec = &specs[nargs];
                }
            }


            // check if this is a positional or normal argument
            if ( spec->m_pos > 0 )
            {
                // the positional arguments start from number 1 so we need
                // to adjust the index
                spec->m_pos--;
                posarg_present = true;
            }
            else // not a positional argument...
            {
                spec->m_pos = nargs;
                nonposarg_present = true;
            }

            // this conversion specifier is tied to the pos-th argument...
            pspec[spec->m_pos] = spec;

            if ( nargs++ == wxMAX_SVNPRINTF_ARGUMENTS )
                break;
        }


        // warn if we lost any arguments (the program probably will crash
        // anyhow because of stack corruption...)
        if ( nargs == wxMAX_SVNPRINTF_ARGUMENTS )
        {
            wxFAIL_MSG
            (
                wxString::Format
                (
                    "wxVsnprintf() currently supports only %d arguments, "
                    "but format string \"%s\" defines more of them.\n"
                    "You need to change wxMAX_SVNPRINTF_ARGUMENTS and "
                    "recompile if more are really needed.",
                    fmt, wxMAX_SVNPRINTF_ARGUMENTS
                )
            );
        }
    }

    // total number of valid elements in specs
    unsigned nargs;

    // all format specifications in this format string in order of their
    // appearance (which may be different from arguments order)
    ConvSpec specs[wxMAX_SVNPRINTF_ARGUMENTS];

    // pointer to specs array element for the N-th argument
    ConvSpec *pspec[wxMAX_SVNPRINTF_ARGUMENTS];

    // true if any positional/non-positional parameters are used
    bool posarg_present,
         nonposarg_present;
};

#undef APPEND_CH
#undef CHECK_PREC

#endif // _WX_PRIVATE_WXPRINTF_H_
