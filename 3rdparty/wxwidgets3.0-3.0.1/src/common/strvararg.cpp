///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/strvararg.cpp
// Purpose:     macros for implementing type-safe vararg passing of strings
// Author:      Vaclav Slavik
// Created:     2007-02-19
// Copyright:   (c) 2007 REA Elektronik GmbH
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

#include "wx/strvararg.h"
#include "wx/string.h"
#include "wx/crt.h"
#include "wx/private/wxprintf.h"

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxArgNormalizer<>
// ----------------------------------------------------------------------------

const wxStringCharType *wxArgNormalizerNative<const wxString&>::get() const
{
    return m_value.wx_str();
}

const wxStringCharType *wxArgNormalizerNative<const wxCStrData&>::get() const
{
    return m_value.AsInternal();
}

#if wxUSE_UNICODE_UTF8 && !wxUSE_UTF8_LOCALE_ONLY
wxArgNormalizerWchar<const wxString&>::wxArgNormalizerWchar(
                            const wxString& s,
                            const wxFormatString *fmt, unsigned index)
    : wxArgNormalizerWithBuffer<wchar_t>(s.wc_str(), fmt, index)
{
}

wxArgNormalizerWchar<const wxCStrData&>::wxArgNormalizerWchar(
                            const wxCStrData& s,
                            const wxFormatString *fmt, unsigned index)
    : wxArgNormalizerWithBuffer<wchar_t>(s.AsWCharBuf(), fmt, index)
{
}
#endif // wxUSE_UNICODE_UTF8 && !wxUSE_UTF8_LOCALE_ONLY

// ----------------------------------------------------------------------------
// wxArgNormalizedString
// ----------------------------------------------------------------------------

wxString wxArgNormalizedString::GetString() const
{
    if ( !IsValid() )
        return wxEmptyString;

#if wxUSE_UTF8_LOCALE_ONLY
    return wxString(reinterpret_cast<const char*>(m_ptr));
#else
    #if wxUSE_UNICODE_UTF8
        if ( wxLocaleIsUtf8 )
            return wxString(reinterpret_cast<const char*>(m_ptr));
        else
    #endif
        return wxString(reinterpret_cast<const wxChar*>(m_ptr));
#endif // !wxUSE_UTF8_LOCALE_ONLY
}

wxArgNormalizedString::operator wxString() const
{
    return GetString();
}

// ----------------------------------------------------------------------------
// wxFormatConverter: class doing the "%s" and "%c" normalization
// ----------------------------------------------------------------------------

/*
   There are four problems with wxPrintf() etc. format strings:

   1) The printf vararg macros convert all forms of strings into
      wxStringCharType* representation. This may make the format string
      incorrect: for example, if %ls was used together with a wchar_t*
      variadic argument, this would no longer work, because the templates
      would change wchar_t* argument to wxStringCharType* and %ls would now
      be incorrect in e.g. UTF-8 build. We need make sure only one specifier
      form is used.

   2) To complicate matters further, the meaning of %s and %c is different
      under Windows and on Unix. The Windows/MS convention is as follows:

       In ANSI mode:

       format specifier         results in
       -----------------------------------
       %s, %hs, %hS             char*
       %ls, %S, %lS             wchar_t*

       In Unicode mode:

       format specifier         results in
       -----------------------------------
       %hs, %S, %hS             char*
       %s, %ls, %lS             wchar_t*

       (While on POSIX systems we have %C identical to %lc and %c always means
       char (in any mode) while %lc always means wchar_t.)

      In other words, we should _only_ use %s on Windows and %ls on Unix for
      wxUSE_UNICODE_WCHAR build.

   3) To make things even worse, we need two forms in UTF-8 build: one for
      passing strings to ANSI functions under UTF-8 locales (this one should
      use %s) and one for widechar functions used under non-UTF-8 locales
      (this one should use %ls).

   And, of course, the same should be done for %c as well.


   wxScanf() family of functions is simpler, because we don't normalize their
   variadic arguments and we only have to handle 2) above and only for widechar
   versions.
*/

template<typename T>
class wxFormatConverterBase
{
public:
    typedef T CharType;

    wxFormatConverterBase()
    {
        m_fmtOrig = NULL;
        m_fmtLast = NULL;
        m_nCopied = 0;
    }

    wxScopedCharTypeBuffer<CharType> Convert(const CharType *format)
    {
        // this is reset to NULL if we modify the format string
        m_fmtOrig = format;

        while ( *format )
        {
            if ( CopyFmtChar(*format++) == wxT('%') )
            {
#if wxUSE_PRINTF_POS_PARAMS
                if ( *format >= '0' && *format <= '9' )
                {
                    SkipDigits(&format);
                    if ( *format == '$' )
                    {
                        // It was a positional argument specification.
                        CopyFmtChar(*format++);
                    }
                    //else: it was a width specification, nothing else to do.
                }
#endif // wxUSE_PRINTF_POS_PARAMS

                // skip any flags
                while ( IsFlagChar(*format) )
                    CopyFmtChar(*format++);

                // and possible width
                if ( *format == wxT('*') )
                    CopyFmtChar(*format++);
                else
                    SkipDigits(&format);

                // precision?
                if ( *format == wxT('.') )
                {
                    CopyFmtChar(*format++);
                    if ( *format == wxT('*') )
                        CopyFmtChar(*format++);
                    else
                        SkipDigits(&format);
                }

                // next we can have a size modifier
                SizeModifier size;

                switch ( *format )
                {
                    case 'h':
                        size = Size_Short;
                        format++;
                        break;

                    case 'l':
                        // "ll" has a different meaning!
                        if ( format[1] != 'l' )
                        {
                            size = Size_Long;
                            format++;
                            break;
                        }
                        //else: fall through

                    default:
                        size = Size_Default;
                }

                CharType outConv = *format;
                SizeModifier outSize = size;

                // and finally we should have the type
                switch ( *format )
                {
                    case wxT('S'):
                    case wxT('s'):
                        // all strings were converted into the same form by
                        // wxArgNormalizer<T>, this form depends on the context
                        // in which the value is used (scanf/printf/wprintf):
                        HandleString(*format, size, outConv, outSize);
                        break;

                    case wxT('C'):
                    case wxT('c'):
                        HandleChar(*format, size, outConv, outSize);
                        break;

                    default:
                        // nothing special to do
                        break;
                }

                if ( outConv == *format && outSize == size ) // no change
                {
                    if ( size != Size_Default )
                        CopyFmtChar(*(format - 1));
                    CopyFmtChar(*format);
                }
                else // something changed
                {
                    switch ( outSize )
                    {
                        case Size_Long:
                            InsertFmtChar(wxT('l'));
                            break;

                        case Size_Short:
                            InsertFmtChar(wxT('h'));
                            break;

                        case Size_Default:
                            // nothing to do
                            break;
                    }
                    InsertFmtChar(outConv);
                }

                format++;
            }
        }

        // notice that we only translated the string if m_fmtOrig == NULL (as
        // set by CopyAllBefore()), otherwise we should simply use the original
        // format
        if ( m_fmtOrig )
        {
            return wxScopedCharTypeBuffer<CharType>::CreateNonOwned(m_fmtOrig);
        }
        else
        {
            // shrink converted format string to actual size (instead of
            // over-sized allocation from CopyAllBefore()) and NUL-terminate
            // it:
            m_fmt.shrink(m_fmtLast - m_fmt.data());
            return m_fmt;
        }
    }

    virtual ~wxFormatConverterBase() {}

protected:
    enum SizeModifier
    {
        Size_Default,
        Size_Short,
        Size_Long
    };

    // called to handle %S or %s; 'conv' is conversion specifier ('S' or 's'
    // respectively), 'size' is the preceding size modifier; the new values of
    // conversion and size specifiers must be written to outConv and outSize
    virtual void HandleString(CharType conv, SizeModifier size,
                              CharType& outConv, SizeModifier& outSize) = 0;

    // ditto for %C or %c
    virtual void HandleChar(CharType conv, SizeModifier size,
                            CharType& outConv, SizeModifier& outSize) = 0;

private:
    // copy another character to the translated format: this function does the
    // copy if we are translating but doesn't do anything at all if we don't,
    // so we don't create the translated format string at all unless we really
    // need to (i.e. InsertFmtChar() is called)
    CharType CopyFmtChar(CharType ch)
    {
        if ( !m_fmtOrig )
        {
            // we're translating, do copy
            *(m_fmtLast++) = ch;
        }
        else
        {
            // simply increase the count which should be copied by
            // CopyAllBefore() later if needed
            m_nCopied++;
        }

        return ch;
    }

    // insert an extra character
    void InsertFmtChar(CharType ch)
    {
        if ( m_fmtOrig )
        {
            // so far we haven't translated anything yet
            CopyAllBefore();
        }

        *(m_fmtLast++) = ch;
    }

    void CopyAllBefore()
    {
        wxASSERT_MSG( m_fmtOrig && m_fmt.data() == NULL, "logic error" );

        // the modified format string is guaranteed to be no longer than
        // 3/2 of the original (worst case: the entire format string consists
        // of "%s" repeated and is expanded to "%ls" on Unix), so we can
        // allocate the buffer now and not worry about running out of space if
        // we over-allocate a bit:
        size_t fmtLen = wxStrlen(m_fmtOrig);
        // worst case is of even length, so there's no rounding error in *3/2:
        m_fmt.extend(fmtLen * 3 / 2);

        if ( m_nCopied > 0 )
            wxStrncpy(m_fmt.data(), m_fmtOrig, m_nCopied);
        m_fmtLast = m_fmt.data() + m_nCopied;

        // we won't need it any longer and resetting it also indicates that we
        // modified the format
        m_fmtOrig = NULL;
    }

    static bool IsFlagChar(CharType ch)
    {
        return ch == wxT('-') || ch == wxT('+') ||
               ch == wxT('0') || ch == wxT(' ') || ch == wxT('#');
    }

    void SkipDigits(const CharType **ptpc)
    {
        while ( **ptpc >= wxT('0') && **ptpc <= wxT('9') )
            CopyFmtChar(*(*ptpc)++);
    }

    // the translated format
    wxCharTypeBuffer<CharType> m_fmt;
    CharType *m_fmtLast;

    // the original format
    const CharType *m_fmtOrig;

    // the number of characters already copied (i.e. already parsed, but left
    // unmodified)
    size_t m_nCopied;
};

#if defined(__WINDOWS__) && !defined(__CYGWIN__)

// on Windows, we should use %s and %c regardless of the build:
class wxPrintfFormatConverterWchar : public wxFormatConverterBase<wchar_t>
{
    virtual void HandleString(CharType WXUNUSED(conv),
                              SizeModifier WXUNUSED(size),
                              CharType& outConv, SizeModifier& outSize)
    {
        outConv = 's';
        outSize = Size_Default;
    }

    virtual void HandleChar(CharType WXUNUSED(conv),
                            SizeModifier WXUNUSED(size),
                            CharType& outConv, SizeModifier& outSize)
    {
        outConv = 'c';
        outSize = Size_Default;
    }
};

#else // !__WINDOWS__

// on Unix, it's %s for ANSI functions and %ls for widechar:

#if !wxUSE_UTF8_LOCALE_ONLY
class wxPrintfFormatConverterWchar : public wxFormatConverterBase<wchar_t>
{
    virtual void HandleString(CharType WXUNUSED(conv),
                              SizeModifier WXUNUSED(size),
                              CharType& outConv, SizeModifier& outSize)
    {
        outConv = 's';
        outSize = Size_Long;
    }

    virtual void HandleChar(CharType WXUNUSED(conv),
                            SizeModifier WXUNUSED(size),
                            CharType& outConv, SizeModifier& outSize)
    {
        outConv = 'c';
        outSize = Size_Long;
    }
};
#endif // !wxUSE_UTF8_LOCALE_ONLY

#endif // __WINDOWS__/!__WINDOWS__

#if wxUSE_UNICODE_UTF8
class wxPrintfFormatConverterUtf8 : public wxFormatConverterBase<char>
{
    virtual void HandleString(CharType WXUNUSED(conv),
                              SizeModifier WXUNUSED(size),
                              CharType& outConv, SizeModifier& outSize)
    {
        outConv = 's';
        outSize = Size_Default;
    }

    virtual void HandleChar(CharType WXUNUSED(conv),
                            SizeModifier WXUNUSED(size),
                            CharType& outConv, SizeModifier& outSize)
    {
        // chars are represented using wchar_t in both builds, so this is
        // the same as above
        outConv = 'c';
        outSize = Size_Long;
    }
};
#endif // wxUSE_UNICODE_UTF8

#if !wxUSE_UNICODE // FIXME-UTF8: remove
class wxPrintfFormatConverterANSI : public wxFormatConverterBase<char>
{
    virtual void HandleString(CharType WXUNUSED(conv),
                              SizeModifier WXUNUSED(size),
                              CharType& outConv, SizeModifier& outSize)
    {
        outConv = 's';
        outSize = Size_Default;
    }

    virtual void HandleChar(CharType WXUNUSED(conv),
                            SizeModifier WXUNUSED(size),
                            CharType& outConv, SizeModifier& outSize)
    {
        outConv = 'c';
        outSize = Size_Default;
    }
};
#endif // ANSI

#ifndef __WINDOWS__
/*

   wxScanf() format translation is different, we need to translate %s to %ls
   and %c to %lc on Unix (but not Windows and for widechar functions only!).

   So to use native functions in order to get our semantics we must do the
   following translations in Unicode mode:

   wxWidgets specifier      POSIX specifier
   ----------------------------------------

   %hc, %C, %hC             %c
   %c                       %lc

 */
class wxScanfFormatConverterWchar : public wxFormatConverterBase<wchar_t>
{
    virtual void HandleString(CharType conv, SizeModifier size,
                              CharType& outConv, SizeModifier& outSize)
    {
        outConv = 's';
        outSize = GetOutSize(conv == 'S', size);
    }

    virtual void HandleChar(CharType conv, SizeModifier size,
                            CharType& outConv, SizeModifier& outSize)
    {
        outConv = 'c';
        outSize = GetOutSize(conv == 'C', size);
    }

    SizeModifier GetOutSize(bool convIsUpper, SizeModifier size)
    {
        // %S and %hS -> %s and %lS -> %ls
        if ( convIsUpper )
        {
            if ( size == Size_Long )
                return Size_Long;
            else
                return Size_Default;
        }
        else // %s or %c
        {
            if ( size == Size_Default )
                return Size_Long;
            else
                return size;
        }
    }
};

const wxScopedWCharBuffer wxScanfConvertFormatW(const wchar_t *format)
{
    return wxScanfFormatConverterWchar().Convert(format);
}
#endif // !__WINDOWS__


// ----------------------------------------------------------------------------
// wxFormatString
// ----------------------------------------------------------------------------

#if !wxUSE_UNICODE_WCHAR
const char* wxFormatString::InputAsChar()
{
    if ( m_char )
        return m_char.data();

    // in ANSI build, wx_str() returns char*, in UTF-8 build, this function
    // is only called under UTF-8 locales, so we should return UTF-8 string,
    // which is, again, what wx_str() returns:
    if ( m_str )
        return m_str->wx_str();

    // ditto wxCStrData:
    if ( m_cstr )
        return m_cstr->AsInternal();

    // the last case is that wide string was passed in: in that case, we need
    // to convert it:
    wxASSERT( m_wchar );

    m_char = wxConvLibc.cWC2MB(m_wchar.data());

    return m_char.data();
}

const char* wxFormatString::AsChar()
{
    if ( !m_convertedChar )
#if !wxUSE_UNICODE // FIXME-UTF8: remove this
        m_convertedChar = wxPrintfFormatConverterANSI().Convert(InputAsChar());
#else
        m_convertedChar = wxPrintfFormatConverterUtf8().Convert(InputAsChar());
#endif

    return m_convertedChar.data();
}
#endif // !wxUSE_UNICODE_WCHAR

#if wxUSE_UNICODE && !wxUSE_UTF8_LOCALE_ONLY
const wchar_t* wxFormatString::InputAsWChar()
{
    if ( m_wchar )
        return m_wchar.data();

#if wxUSE_UNICODE_WCHAR
    if ( m_str )
        return m_str->wc_str();
    if ( m_cstr )
        return m_cstr->AsInternal();
#else // wxUSE_UNICODE_UTF8
    if ( m_str )
    {
        m_wchar = m_str->wc_str();
        return m_wchar.data();
    }
    if ( m_cstr )
    {
        m_wchar = m_cstr->AsWCharBuf();
        return m_wchar.data();
    }
#endif // wxUSE_UNICODE_WCHAR/UTF8

    // the last case is that narrow string was passed in: in that case, we need
    // to convert it:
    wxASSERT( m_char );

    m_wchar = wxConvLibc.cMB2WC(m_char.data());

    return m_wchar.data();
}

const wchar_t* wxFormatString::AsWChar()
{
    if ( !m_convertedWChar )
        m_convertedWChar = wxPrintfFormatConverterWchar().Convert(InputAsWChar());

    return m_convertedWChar.data();
}
#endif // wxUSE_UNICODE && !wxUSE_UTF8_LOCALE_ONLY

wxString wxFormatString::InputAsString() const
{
    if ( m_str )
        return *m_str;
    if ( m_cstr )
        return m_cstr->AsString();
    if ( m_wchar )
        return wxString(m_wchar);
    if ( m_char )
        return wxString(m_char);

    wxFAIL_MSG( "invalid wxFormatString - not initialized?" );
    return wxString();
}

// ----------------------------------------------------------------------------
// wxFormatString::GetArgumentType()
// ----------------------------------------------------------------------------

namespace
{

template<typename CharType>
wxFormatString::ArgumentType DoGetArgumentType(const CharType *format,
                                               unsigned n)
{
    wxCHECK_MSG( format, wxFormatString::Arg_Unknown,
                 "empty format string not allowed here" );

    wxPrintfConvSpecParser<CharType> parser(format);

    if ( n > parser.nargs )
    {
        // The n-th argument doesn't appear in the format string and is unused.
        // This can happen e.g. if a translation of the format string is used
        // and the translation language tends to avoid numbers in singular forms.
        // The translator would then typically replace "%d" with "One" (e.g. in
        // Hebrew). Passing too many vararg arguments does not harm, so its
        // better to be more permissive here and allow legitimate uses in favour
        // of catching harmless errors.
        return wxFormatString::Arg_Unused;
    }

    wxCHECK_MSG( parser.pspec[n-1] != NULL, wxFormatString::Arg_Unknown,
                 "requested argument not found - invalid format string?" );

    switch ( parser.pspec[n-1]->m_type )
    {
        case wxPAT_CHAR:
        case wxPAT_WCHAR:
            return wxFormatString::Arg_Char;

        case wxPAT_PCHAR:
        case wxPAT_PWCHAR:
            return wxFormatString::Arg_String;

        case wxPAT_INT:
            return wxFormatString::Arg_Int;
        case wxPAT_LONGINT:
            return wxFormatString::Arg_LongInt;
#ifdef wxLongLong_t
        case wxPAT_LONGLONGINT:
            return wxFormatString::Arg_LongLongInt;
#endif
        case wxPAT_SIZET:
            return wxFormatString::Arg_Size_t;

        case wxPAT_DOUBLE:
            return wxFormatString::Arg_Double;
        case wxPAT_LONGDOUBLE:
            return wxFormatString::Arg_LongDouble;

        case wxPAT_POINTER:
            return wxFormatString::Arg_Pointer;

        case wxPAT_NINT:
            return wxFormatString::Arg_IntPtr;
        case wxPAT_NSHORTINT:
            return wxFormatString::Arg_ShortIntPtr;
        case wxPAT_NLONGINT:
            return wxFormatString::Arg_LongIntPtr;

        case wxPAT_STAR:
            // "*" requires argument of type int
            return wxFormatString::Arg_Int;

        case wxPAT_INVALID:
            // (handled after the switch statement)
            break;
    }

    // silence warning
    wxFAIL_MSG( "unexpected argument type" );
    return wxFormatString::Arg_Unknown;
}

} // anonymous namespace

wxFormatString::ArgumentType wxFormatString::GetArgumentType(unsigned n) const
{
    if ( m_char )
        return DoGetArgumentType(m_char.data(), n);
    else if ( m_wchar )
        return DoGetArgumentType(m_wchar.data(), n);
    else if ( m_str )
        return DoGetArgumentType(m_str->wx_str(), n);
    else if ( m_cstr )
        return DoGetArgumentType(m_cstr->AsInternal(), n);

    wxFAIL_MSG( "unreachable code" );
    return Arg_Unknown;
}
