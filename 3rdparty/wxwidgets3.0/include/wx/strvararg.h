///////////////////////////////////////////////////////////////////////////////
// Name:        wx/strvararg.h
// Purpose:     macros for implementing type-safe vararg passing of strings
// Author:      Vaclav Slavik
// Created:     2007-02-19
// Copyright:   (c) 2007 REA Elektronik GmbH
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_STRVARARG_H_
#define _WX_STRVARARG_H_

#include "wx/platform.h"
#if wxONLY_WATCOM_EARLIER_THAN(1,4)
    #error "OpenWatcom version >= 1.4 is required to compile this code"
#endif

#include "wx/cpp.h"
#include "wx/chartype.h"
#include "wx/strconv.h"
#include "wx/buffer.h"
#include "wx/unichar.h"

#if defined(HAVE_TYPE_TRAITS)
    #include <type_traits>
#elif defined(HAVE_TR1_TYPE_TRAITS)
    #ifdef __VISUALC__
        #include <type_traits>
    #else
        #include <tr1/type_traits>
    #endif
#endif

class WXDLLIMPEXP_FWD_BASE wxCStrData;
class WXDLLIMPEXP_FWD_BASE wxString;

// ----------------------------------------------------------------------------
// WX_DEFINE_VARARG_FUNC* macros
// ----------------------------------------------------------------------------

// This macro is used to implement type-safe wrappers for variadic functions
// that accept strings as arguments. This makes it possible to pass char*,
// wchar_t* or even wxString (as opposed to having to use wxString::c_str())
// to e.g. wxPrintf().
//
// This is done by defining a set of N template function taking 1..N arguments
// (currently, N is set to 30 in this header). These functions are just thin
// wrappers around another variadic function ('impl' or 'implUtf8' arguments,
// see below) and the only thing the wrapper does is that it normalizes the
// arguments passed in so that they are of the type expected by variadic
// functions taking string arguments, i.e., char* or wchar_t*, depending on the
// build:
//   * char* in the current locale's charset in ANSI build
//   * char* with UTF-8 encoding if wxUSE_UNICODE_UTF8 and the app is running
//     under an UTF-8 locale
//   * wchar_t* if wxUSE_UNICODE_WCHAR or if wxUSE_UNICODE_UTF8 and the current
//     locale is not UTF-8
//
// Note that wxFormatString *must* be used for the format parameter of these
// functions, otherwise the implementation won't work correctly. Furthermore,
// it must be passed by value, not reference, because it's modified by the
// vararg templates internally.
//
// Parameters:
// [ there are examples in square brackets showing values of the parameters
//   for the wxFprintf() wrapper for fprintf() function with the following
//   prototype:
//   int wxFprintf(FILE *stream, const wxString& format, ...); ]
//
//        rettype   Functions' return type  [int]
//        name      Name of the function  [fprintf]
//        numfixed  The number of leading "fixed" (i.e., not variadic)
//                  arguments of the function (e.g. "stream" and "format"
//                  arguments of fprintf()); their type is _not_ converted
//                  using wxArgNormalizer<T>, unlike the rest of
//                  the function's arguments  [2]
//        fixed     List of types of the leading "fixed" arguments, in
//                  parenthesis  [(FILE*,const wxString&)]
//        impl      Name of the variadic function that implements 'name' for
//                  the native strings representation (wchar_t* if
//                  wxUSE_UNICODE_WCHAR or wxUSE_UNICODE_UTF8 when running under
//                  non-UTF8 locale, char* in ANSI build)  [wxCrt_Fprintf]
//        implUtf8  Like 'impl', but for the UTF-8 char* version to be used
//                  if wxUSE_UNICODE_UTF8 and running under UTF-8 locale
//                  (ignored otherwise)  [fprintf]
//
#define WX_DEFINE_VARARG_FUNC(rettype, name, numfixed, fixed, impl, implUtf8) \
    _WX_VARARG_DEFINE_FUNC_N0(rettype, name, impl, implUtf8, numfixed, fixed) \
    WX_DEFINE_VARARG_FUNC_SANS_N0(rettype, name, numfixed, fixed, impl, implUtf8)

// ditto, but without the version with 0 template/vararg arguments
#define WX_DEFINE_VARARG_FUNC_SANS_N0(rettype, name,                          \
                                       numfixed, fixed, impl, implUtf8)       \
    _WX_VARARG_ITER(_WX_VARARG_MAX_ARGS,                                      \
                    _WX_VARARG_DEFINE_FUNC,                                   \
                    rettype, name, impl, implUtf8, numfixed, fixed)

// Like WX_DEFINE_VARARG_FUNC, but for variadic functions that don't return
// a value.
#define WX_DEFINE_VARARG_FUNC_VOID(name, numfixed, fixed, impl, implUtf8)     \
    _WX_VARARG_DEFINE_FUNC_VOID_N0(name, impl, implUtf8, numfixed, fixed)     \
    _WX_VARARG_ITER(_WX_VARARG_MAX_ARGS,                                      \
                    _WX_VARARG_DEFINE_FUNC_VOID,                              \
                    void, name, impl, implUtf8, numfixed, fixed)

// Like WX_DEFINE_VARARG_FUNC_VOID, but instead of wrapping an implementation
// function, does nothing in defined functions' bodies.
//
// Used to implement wxLogXXX functions if wxUSE_LOG=0.
#define WX_DEFINE_VARARG_FUNC_NOP(name, numfixed, fixed)                      \
        _WX_VARARG_DEFINE_FUNC_NOP_N0(name, numfixed, fixed)                  \
        _WX_VARARG_ITER(_WX_VARARG_MAX_ARGS,                                  \
                        _WX_VARARG_DEFINE_FUNC_NOP,                           \
                        void, name, dummy, dummy, numfixed, fixed)

// Like WX_DEFINE_VARARG_FUNC_CTOR, but for defining template constructors
#define WX_DEFINE_VARARG_FUNC_CTOR(name, numfixed, fixed, impl, implUtf8)     \
    _WX_VARARG_DEFINE_FUNC_CTOR_N0(name, impl, implUtf8, numfixed, fixed)     \
    _WX_VARARG_ITER(_WX_VARARG_MAX_ARGS,                                      \
                    _WX_VARARG_DEFINE_FUNC_CTOR,                              \
                    void, name, impl, implUtf8, numfixed, fixed)


// ----------------------------------------------------------------------------
// wxFormatString
// ----------------------------------------------------------------------------

// This class must be used for format string argument of the functions
// defined using WX_DEFINE_VARARG_FUNC_* macros. It converts the string to
// char* or wchar_t* for passing to implementation function efficiently (i.e.
// without keeping the converted string in memory for longer than necessary,
// like c_str()). It also converts format string to the correct form that
// accounts for string changes done by wxArgNormalizer<>
//
// Note that this class can _only_ be used for function arguments!
class WXDLLIMPEXP_BASE wxFormatString
{
public:
    wxFormatString(const char *str)
        : m_char(wxScopedCharBuffer::CreateNonOwned(str)), m_str(NULL), m_cstr(NULL) {}
    wxFormatString(const wchar_t *str)
        : m_wchar(wxScopedWCharBuffer::CreateNonOwned(str)), m_str(NULL), m_cstr(NULL) {}
    wxFormatString(const wxString& str)
        : m_str(&str), m_cstr(NULL) {}
    wxFormatString(const wxCStrData& str)
        : m_str(NULL), m_cstr(&str) {}
    wxFormatString(const wxScopedCharBuffer& str)
        : m_char(str), m_str(NULL), m_cstr(NULL)  {}
    wxFormatString(const wxScopedWCharBuffer& str)
        : m_wchar(str), m_str(NULL), m_cstr(NULL) {}

    // Possible argument types. These are or-combinable for wxASSERT_ARG_TYPE
    // convenience. Some of the values are or-combined with another value, this
    // expresses "supertypes" for use with wxASSERT_ARG_TYPE masks. For example,
    // a char* string is also a pointer and an integer is also a char.
    enum ArgumentType
    {
#if wxABI_VERSION >= 30001
        Arg_Unused      = 0, // not used at all; the value of 0 is chosen to
                             // conveniently pass wxASSERT_ARG_TYPE's check
#endif

        Arg_Char        = 0x0001,    // character as char %c
        Arg_Pointer     = 0x0002,    // %p
        Arg_String      = 0x0004 | Arg_Pointer, // any form of string (%s and %p too)

        Arg_Int         = 0x0008 | Arg_Char, // (ints can be used with %c)
#if SIZEOF_INT == SIZEOF_LONG
        Arg_LongInt     = Arg_Int,
#else
        Arg_LongInt     = 0x0010,
#endif
#if defined(SIZEOF_LONG_LONG) && SIZEOF_LONG_LONG == SIZEOF_LONG
        Arg_LongLongInt = Arg_LongInt,
#elif defined(wxLongLong_t)
        Arg_LongLongInt = 0x0020,
#endif

        Arg_Double      = 0x0040,
        Arg_LongDouble  = 0x0080,

#if defined(wxSIZE_T_IS_UINT)
        Arg_Size_t      = Arg_Int,
#elif defined(wxSIZE_T_IS_ULONG)
        Arg_Size_t      = Arg_LongInt,
#elif defined(SIZEOF_LONG_LONG) && SIZEOF_SIZE_T == SIZEOF_LONG_LONG
        Arg_Size_t      = Arg_LongLongInt,
#else
        Arg_Size_t      = 0x0100,
#endif

        Arg_IntPtr      = 0x0200,    // %n -- store # of chars written
        Arg_ShortIntPtr = 0x0400,
        Arg_LongIntPtr  = 0x0800,

        Arg_Unknown     = 0x8000     // unrecognized specifier (likely error)
    };

    // returns the type of format specifier for n-th variadic argument (this is
    // not necessarily n-th format specifier if positional specifiers are used);
    // called by wxArgNormalizer<> specializations to get information about
    // n-th variadic argument desired representation
    ArgumentType GetArgumentType(unsigned n) const;

    // returns the value passed to ctor, only converted to wxString, similarly
    // to other InputAsXXX() methods
    wxString InputAsString() const;

#if !wxUSE_UNICODE_WCHAR
    operator const char*() const
        { return const_cast<wxFormatString*>(this)->AsChar(); }
private:
    // InputAsChar() returns the value passed to ctor, only converted
    // to char, while AsChar() takes the string returned by InputAsChar()
    // and does format string conversion on it as well (and similarly for
    // ..AsWChar() below)
    const char* InputAsChar();
    const char* AsChar();
    wxScopedCharBuffer m_convertedChar;
#endif // !wxUSE_UNICODE_WCHAR

#if wxUSE_UNICODE && !wxUSE_UTF8_LOCALE_ONLY
public:
    operator const wchar_t*() const
        { return const_cast<wxFormatString*>(this)->AsWChar(); }
private:
    const wchar_t* InputAsWChar();
    const wchar_t* AsWChar();
    wxScopedWCharBuffer m_convertedWChar;
#endif // wxUSE_UNICODE && !wxUSE_UTF8_LOCALE_ONLY

private:
    wxScopedCharBuffer  m_char;
    wxScopedWCharBuffer m_wchar;

    // NB: we can use a pointer here, because wxFormatString is only used
    //     as function argument, so it has shorter life than the string
    //     passed to the ctor
    const wxString * const m_str;
    const wxCStrData * const m_cstr;

    wxDECLARE_NO_ASSIGN_CLASS(wxFormatString);
};

// these two helper classes are used to find wxFormatString argument among fixed
// arguments passed to a vararg template
struct wxFormatStringArgument
{
    wxFormatStringArgument(const wxFormatString *s = NULL) : m_str(s) {}
    const wxFormatString *m_str;

    // overriding this operator allows us to reuse _WX_VARARG_JOIN macro
    wxFormatStringArgument operator,(const wxFormatStringArgument& a) const
    {
        wxASSERT_MSG( m_str == NULL || a.m_str == NULL,
                      "can't have two format strings in vararg function" );
        return wxFormatStringArgument(m_str ? m_str : a.m_str);
    }

    operator const wxFormatString*() const { return m_str; }
};

template<typename T>
struct wxFormatStringArgumentFinder
{
    static wxFormatStringArgument find(T)
    {
        // by default, arguments are not format strings, so return "not found"
        return wxFormatStringArgument();
    }
};

template<>
struct wxFormatStringArgumentFinder<const wxFormatString&>
{
    static wxFormatStringArgument find(const wxFormatString& arg)
        { return wxFormatStringArgument(&arg); }
};

template<>
struct wxFormatStringArgumentFinder<wxFormatString>
    : public wxFormatStringArgumentFinder<const wxFormatString&> {};

// avoid passing big objects by value to wxFormatStringArgumentFinder::find()
// (and especially wx[W]CharBuffer with its auto_ptr<> style semantics!):
template<>
struct wxFormatStringArgumentFinder<wxString>
    : public wxFormatStringArgumentFinder<const wxString&> {};

template<>
struct wxFormatStringArgumentFinder<wxScopedCharBuffer>
    : public wxFormatStringArgumentFinder<const wxScopedCharBuffer&> {};

template<>
struct wxFormatStringArgumentFinder<wxScopedWCharBuffer>
    : public wxFormatStringArgumentFinder<const wxScopedWCharBuffer&> {};

template<>
struct wxFormatStringArgumentFinder<wxCharBuffer>
    : public wxFormatStringArgumentFinder<const wxCharBuffer&> {};

template<>
struct wxFormatStringArgumentFinder<wxWCharBuffer>
    : public wxFormatStringArgumentFinder<const wxWCharBuffer&> {};


// ----------------------------------------------------------------------------
// wxArgNormalizer*<T> converters
// ----------------------------------------------------------------------------

#if wxDEBUG_LEVEL
    // Check that the format specifier for index-th argument in 'fmt' has
    // the correct type (one of wxFormatString::Arg_XXX or-combination in
    // 'expected_mask').
    #define wxASSERT_ARG_TYPE(fmt, index, expected_mask)                    \
        wxSTATEMENT_MACRO_BEGIN                                             \
            if ( !fmt )                                                     \
                break;                                                      \
            const int argtype = fmt->GetArgumentType(index);                \
            wxASSERT_MSG( (argtype & (expected_mask)) == argtype,           \
                          "format specifier doesn't match argument type" ); \
        wxSTATEMENT_MACRO_END
#else
    // Just define it to suppress "unused parameter" warnings for the
    // parameters which we don't use otherwise
    #define wxASSERT_ARG_TYPE(fmt, index, expected_mask)                      \
        wxUnusedVar(fmt);                                                     \
        wxUnusedVar(index)
#endif // wxDEBUG_LEVEL/!wxDEBUG_LEVEL


#if defined(HAVE_TYPE_TRAITS) || defined(HAVE_TR1_TYPE_TRAITS)

// Note: this type is misnamed, so that the error message is easier to
// understand (no error happens for enums, because the IsEnum=true case is
// specialized).
template<bool IsEnum>
struct wxFormatStringSpecifierNonPodType {};

template<>
struct wxFormatStringSpecifierNonPodType<true>
{
    enum { value = wxFormatString::Arg_Int };
};

template<typename T>
struct wxFormatStringSpecifier
{
#ifdef HAVE_TYPE_TRAITS
    typedef std::is_enum<T> is_enum;
#elif defined HAVE_TR1_TYPE_TRAITS
    typedef std::tr1::is_enum<T> is_enum;
#endif
    enum { value = wxFormatStringSpecifierNonPodType<is_enum::value>::value };
};

#else // !HAVE_(TR1_)TYPE_TRAITS

template<typename T>
struct wxFormatStringSpecifier
{
    // We can't detect enums without is_enum, so the only thing we can
    // do is to accept unknown types. However, the only acceptable unknown
    // types still are enums, which are promoted to ints, so return Arg_Int
    // here. This will at least catch passing of non-POD types through ... at
    // runtime.
    //
    // Furthermore, if the compiler doesn't have partial template
    // specialization, we didn't cover pointers either.
#ifdef HAVE_PARTIAL_SPECIALIZATION
    enum { value = wxFormatString::Arg_Int };
#else
    enum { value = wxFormatString::Arg_Int | wxFormatString::Arg_Pointer };
#endif
};

#endif // HAVE_TR1_TYPE_TRAITS/!HAVE_TR1_TYPE_TRAITS


#ifdef HAVE_PARTIAL_SPECIALIZATION
template<typename T>
struct wxFormatStringSpecifier<T*>
{
    enum { value = wxFormatString::Arg_Pointer };
};

template<typename T>
struct wxFormatStringSpecifier<const T*>
{
    enum { value = wxFormatString::Arg_Pointer };
};
#endif // !HAVE_PARTIAL_SPECIALIZATION


#define wxFORMAT_STRING_SPECIFIER(T, arg)                                   \
    template<> struct wxFormatStringSpecifier<T>                            \
    {                                                                       \
        enum { value = arg };                                               \
    };

wxFORMAT_STRING_SPECIFIER(bool, wxFormatString::Arg_Int)
wxFORMAT_STRING_SPECIFIER(int, wxFormatString::Arg_Int)
wxFORMAT_STRING_SPECIFIER(unsigned int, wxFormatString::Arg_Int)
wxFORMAT_STRING_SPECIFIER(short int, wxFormatString::Arg_Int)
wxFORMAT_STRING_SPECIFIER(short unsigned int, wxFormatString::Arg_Int)
wxFORMAT_STRING_SPECIFIER(long int, wxFormatString::Arg_LongInt)
wxFORMAT_STRING_SPECIFIER(long unsigned int, wxFormatString::Arg_LongInt)
#ifdef wxLongLong_t
wxFORMAT_STRING_SPECIFIER(wxLongLong_t, wxFormatString::Arg_LongLongInt)
wxFORMAT_STRING_SPECIFIER(wxULongLong_t, wxFormatString::Arg_LongLongInt)
#endif
wxFORMAT_STRING_SPECIFIER(float, wxFormatString::Arg_Double)
wxFORMAT_STRING_SPECIFIER(double, wxFormatString::Arg_Double)
wxFORMAT_STRING_SPECIFIER(long double, wxFormatString::Arg_LongDouble)

#if wxWCHAR_T_IS_REAL_TYPE
wxFORMAT_STRING_SPECIFIER(wchar_t, wxFormatString::Arg_Char | wxFormatString::Arg_Int)
#endif

#if !wxUSE_UNICODE
wxFORMAT_STRING_SPECIFIER(char, wxFormatString::Arg_Char | wxFormatString::Arg_Int)
wxFORMAT_STRING_SPECIFIER(signed char, wxFormatString::Arg_Char | wxFormatString::Arg_Int)
wxFORMAT_STRING_SPECIFIER(unsigned char, wxFormatString::Arg_Char | wxFormatString::Arg_Int)
#endif

wxFORMAT_STRING_SPECIFIER(char*, wxFormatString::Arg_String)
wxFORMAT_STRING_SPECIFIER(unsigned char*, wxFormatString::Arg_String)
wxFORMAT_STRING_SPECIFIER(signed char*, wxFormatString::Arg_String)
wxFORMAT_STRING_SPECIFIER(const char*, wxFormatString::Arg_String)
wxFORMAT_STRING_SPECIFIER(const unsigned char*, wxFormatString::Arg_String)
wxFORMAT_STRING_SPECIFIER(const signed char*, wxFormatString::Arg_String)
wxFORMAT_STRING_SPECIFIER(wchar_t*, wxFormatString::Arg_String)
wxFORMAT_STRING_SPECIFIER(const wchar_t*, wxFormatString::Arg_String)

wxFORMAT_STRING_SPECIFIER(int*, wxFormatString::Arg_IntPtr | wxFormatString::Arg_Pointer)
wxFORMAT_STRING_SPECIFIER(short int*, wxFormatString::Arg_ShortIntPtr | wxFormatString::Arg_Pointer)
wxFORMAT_STRING_SPECIFIER(long int*, wxFormatString::Arg_LongIntPtr | wxFormatString::Arg_Pointer)

#undef wxFORMAT_STRING_SPECIFIER


// Converts an argument passed to wxPrint etc. into standard form expected,
// by wxXXX functions, e.g. all strings (wxString, char*, wchar_t*) are
// converted into wchar_t* or char* depending on the build.
template<typename T>
struct wxArgNormalizer
{
    // Ctor. 'value' is the value passed as variadic argument, 'fmt' is pointer
    // to printf-like format string or NULL if the variadic function doesn't
    // use format string and 'index' is index of 'value' in variadic arguments
    // list (starting at 1)
    wxArgNormalizer(T value,
                    const wxFormatString *fmt, unsigned index)
        : m_value(value)
    {
        wxASSERT_ARG_TYPE( fmt, index, wxFormatStringSpecifier<T>::value );
    }

    // Returns the value in a form that can be safely passed to real vararg
    // functions. In case of strings, this is char* in ANSI build and wchar_t*
    // in Unicode build.
    T get() const { return m_value; }

    T m_value;
};

// normalizer for passing arguments to functions working with wchar_t* (and
// until ANSI build is removed, char* in ANSI build as well - FIXME-UTF8)
// string representation
#if !wxUSE_UTF8_LOCALE_ONLY
template<typename T>
struct wxArgNormalizerWchar : public wxArgNormalizer<T>
{
    wxArgNormalizerWchar(T value,
                         const wxFormatString *fmt, unsigned index)
        : wxArgNormalizer<T>(value, fmt, index) {}
};
#endif // !wxUSE_UTF8_LOCALE_ONLY

// normalizer for passing arguments to functions working with UTF-8 encoded
// char* strings
#if wxUSE_UNICODE_UTF8
    template<typename T>
    struct wxArgNormalizerUtf8 : public wxArgNormalizer<T>
    {
        wxArgNormalizerUtf8(T value,
                            const wxFormatString *fmt, unsigned index)
            : wxArgNormalizer<T>(value, fmt, index) {}
    };

    #define wxArgNormalizerNative wxArgNormalizerUtf8
#else // wxUSE_UNICODE_WCHAR
    #define wxArgNormalizerNative wxArgNormalizerWchar
#endif // wxUSE_UNICODE_UTF8 // wxUSE_UNICODE_UTF8



// special cases for converting strings:


// base class for wxArgNormalizer<T> specializations that need to do conversion;
// CharType is either wxStringCharType or wchar_t in UTF-8 build when wrapping
// widechar CRT function
template<typename CharType>
struct wxArgNormalizerWithBuffer
{
    typedef wxScopedCharTypeBuffer<CharType> CharBuffer;

    wxArgNormalizerWithBuffer() {}
    wxArgNormalizerWithBuffer(const CharBuffer& buf,
                              const wxFormatString *fmt,
                              unsigned index)
        : m_value(buf)
    {
        wxASSERT_ARG_TYPE( fmt, index, wxFormatString::Arg_String );
    }

    const CharType *get() const { return m_value; }

    CharBuffer m_value;
};

// string objects:
template<>
struct WXDLLIMPEXP_BASE wxArgNormalizerNative<const wxString&>
{
    wxArgNormalizerNative(const wxString& s,
                          const wxFormatString *fmt,
                          unsigned index)
        : m_value(s)
    {
        wxASSERT_ARG_TYPE( fmt, index, wxFormatString::Arg_String );
    }

    const wxStringCharType *get() const;

    const wxString& m_value;
};

// c_str() values:
template<>
struct WXDLLIMPEXP_BASE wxArgNormalizerNative<const wxCStrData&>
{
    wxArgNormalizerNative(const wxCStrData& value,
                          const wxFormatString *fmt,
                          unsigned index)
        : m_value(value)
    {
        wxASSERT_ARG_TYPE( fmt, index, wxFormatString::Arg_String );
    }

    const wxStringCharType *get() const;

    const wxCStrData& m_value;
};

// wxString/wxCStrData conversion to wchar_t* value
#if wxUSE_UNICODE_UTF8 && !wxUSE_UTF8_LOCALE_ONLY
template<>
struct WXDLLIMPEXP_BASE wxArgNormalizerWchar<const wxString&>
    : public wxArgNormalizerWithBuffer<wchar_t>
{
    wxArgNormalizerWchar(const wxString& s,
                         const wxFormatString *fmt, unsigned index);
};

template<>
struct WXDLLIMPEXP_BASE wxArgNormalizerWchar<const wxCStrData&>
    : public wxArgNormalizerWithBuffer<wchar_t>
{
    wxArgNormalizerWchar(const wxCStrData& s,
                         const wxFormatString *fmt, unsigned index);
};
#endif // wxUSE_UNICODE_UTF8 && !wxUSE_UTF8_LOCALE_ONLY


// C string pointers of the wrong type (wchar_t* for ANSI or UTF8 build,
// char* for wchar_t Unicode build or UTF8):
#if wxUSE_UNICODE_WCHAR

template<>
struct wxArgNormalizerWchar<const char*>
    : public wxArgNormalizerWithBuffer<wchar_t>
{
    wxArgNormalizerWchar(const char* s,
                         const wxFormatString *fmt, unsigned index)
        : wxArgNormalizerWithBuffer<wchar_t>(wxConvLibc.cMB2WC(s), fmt, index) {}
};

#elif wxUSE_UNICODE_UTF8

template<>
struct wxArgNormalizerUtf8<const wchar_t*>
    : public wxArgNormalizerWithBuffer<char>
{
    wxArgNormalizerUtf8(const wchar_t* s,
                        const wxFormatString *fmt, unsigned index)
        : wxArgNormalizerWithBuffer<char>(wxConvUTF8.cWC2MB(s), fmt, index) {}
};

template<>
struct wxArgNormalizerUtf8<const char*>
    : public wxArgNormalizerWithBuffer<char>
{
    wxArgNormalizerUtf8(const char* s,
                        const wxFormatString *fmt,
                        unsigned index)
    {
        wxASSERT_ARG_TYPE( fmt, index, wxFormatString::Arg_String );

        if ( wxLocaleIsUtf8 )
        {
            m_value = wxScopedCharBuffer::CreateNonOwned(s);
        }
        else
        {
            // convert to widechar string first:
            wxScopedWCharBuffer buf(wxConvLibc.cMB2WC(s));

            // then to UTF-8:
            if ( buf )
                m_value = wxConvUTF8.cWC2MB(buf);
        }
    }
};

// UTF-8 build needs conversion to wchar_t* too:
#if !wxUSE_UTF8_LOCALE_ONLY
template<>
struct wxArgNormalizerWchar<const char*>
    : public wxArgNormalizerWithBuffer<wchar_t>
{
    wxArgNormalizerWchar(const char* s,
                         const wxFormatString *fmt, unsigned index)
        : wxArgNormalizerWithBuffer<wchar_t>(wxConvLibc.cMB2WC(s), fmt, index) {}
};
#endif // !wxUSE_UTF8_LOCALE_ONLY

#else // ANSI - FIXME-UTF8

template<>
struct wxArgNormalizerWchar<const wchar_t*>
    : public wxArgNormalizerWithBuffer<char>
{
    wxArgNormalizerWchar(const wchar_t* s,
                         const wxFormatString *fmt, unsigned index)
        : wxArgNormalizerWithBuffer<char>(wxConvLibc.cWC2MB(s), fmt, index) {}
};

#endif // wxUSE_UNICODE_WCHAR/wxUSE_UNICODE_UTF8/ANSI


// this macro is used to implement specialization that are exactly same as
// some other specialization, i.e. to "forward" the implementation (e.g. for
// T=wxString and T=const wxString&). Note that the ctor takes BaseT argument,
// not T!
#if wxUSE_UNICODE_UTF8
    #if wxUSE_UTF8_LOCALE_ONLY
        #define WX_ARG_NORMALIZER_FORWARD(T, BaseT)                         \
          _WX_ARG_NORMALIZER_FORWARD_IMPL(wxArgNormalizerUtf8, T, BaseT)
    #else // possibly non-UTF8 locales
        #define WX_ARG_NORMALIZER_FORWARD(T, BaseT)                         \
          _WX_ARG_NORMALIZER_FORWARD_IMPL(wxArgNormalizerWchar, T, BaseT);  \
          _WX_ARG_NORMALIZER_FORWARD_IMPL(wxArgNormalizerUtf8, T, BaseT)
    #endif
#else // wxUSE_UNICODE_WCHAR
    #define WX_ARG_NORMALIZER_FORWARD(T, BaseT)                             \
        _WX_ARG_NORMALIZER_FORWARD_IMPL(wxArgNormalizerWchar, T, BaseT)
#endif // wxUSE_UNICODE_UTF8/wxUSE_UNICODE_WCHAR

#define _WX_ARG_NORMALIZER_FORWARD_IMPL(Normalizer, T, BaseT)               \
    template<>                                                              \
    struct Normalizer<T> : public Normalizer<BaseT>                         \
    {                                                                       \
        Normalizer(BaseT value,                                             \
                   const wxFormatString *fmt, unsigned index)               \
            : Normalizer<BaseT>(value, fmt, index) {}                       \
    }

// non-reference versions of specializations for string objects
WX_ARG_NORMALIZER_FORWARD(wxString, const wxString&);
WX_ARG_NORMALIZER_FORWARD(wxCStrData, const wxCStrData&);

// versions for passing non-const pointers:
WX_ARG_NORMALIZER_FORWARD(char*, const char*);
WX_ARG_NORMALIZER_FORWARD(wchar_t*, const wchar_t*);

// versions for passing wx[W]CharBuffer:
WX_ARG_NORMALIZER_FORWARD(wxScopedCharBuffer, const char*);
WX_ARG_NORMALIZER_FORWARD(const wxScopedCharBuffer&, const char*);
WX_ARG_NORMALIZER_FORWARD(wxScopedWCharBuffer, const wchar_t*);
WX_ARG_NORMALIZER_FORWARD(const wxScopedWCharBuffer&, const wchar_t*);
WX_ARG_NORMALIZER_FORWARD(wxCharBuffer, const char*);
WX_ARG_NORMALIZER_FORWARD(const wxCharBuffer&, const char*);
WX_ARG_NORMALIZER_FORWARD(wxWCharBuffer, const wchar_t*);
WX_ARG_NORMALIZER_FORWARD(const wxWCharBuffer&, const wchar_t*);

// versions for std::[w]string:
#if wxUSE_STD_STRING

#include "wx/stringimpl.h"

#if !wxUSE_UTF8_LOCALE_ONLY
template<>
struct wxArgNormalizerWchar<const std::string&>
    : public wxArgNormalizerWchar<const char*>
{
    wxArgNormalizerWchar(const std::string& s,
                         const wxFormatString *fmt, unsigned index)
        : wxArgNormalizerWchar<const char*>(s.c_str(), fmt, index) {}
};

template<>
struct wxArgNormalizerWchar<const wxStdWideString&>
    : public wxArgNormalizerWchar<const wchar_t*>
{
    wxArgNormalizerWchar(const wxStdWideString& s,
                         const wxFormatString *fmt, unsigned index)
        : wxArgNormalizerWchar<const wchar_t*>(s.c_str(), fmt, index) {}
};
#endif // !wxUSE_UTF8_LOCALE_ONLY

#if wxUSE_UNICODE_UTF8
template<>
struct wxArgNormalizerUtf8<const std::string&>
    : public wxArgNormalizerUtf8<const char*>
{
    wxArgNormalizerUtf8(const std::string& s,
                        const wxFormatString *fmt, unsigned index)
        : wxArgNormalizerUtf8<const char*>(s.c_str(), fmt, index) {}
};

template<>
struct wxArgNormalizerUtf8<const wxStdWideString&>
    : public wxArgNormalizerUtf8<const wchar_t*>
{
    wxArgNormalizerUtf8(const wxStdWideString& s,
                        const wxFormatString *fmt, unsigned index)
        : wxArgNormalizerUtf8<const wchar_t*>(s.c_str(), fmt, index) {}
};
#endif // wxUSE_UNICODE_UTF8

WX_ARG_NORMALIZER_FORWARD(std::string, const std::string&);
WX_ARG_NORMALIZER_FORWARD(wxStdWideString, const wxStdWideString&);

#endif // wxUSE_STD_STRING


// versions for wxUniChar, wxUniCharRef:
// (this is same for UTF-8 and Wchar builds, we just convert to wchar_t)
template<>
struct wxArgNormalizer<const wxUniChar&> : public wxArgNormalizer<wchar_t>
{
    wxArgNormalizer(const wxUniChar& s,
                    const wxFormatString *fmt, unsigned index)
        : wxArgNormalizer<wchar_t>(wx_truncate_cast(wchar_t, s.GetValue()), fmt, index) {}
};

// for wchar_t, default handler does the right thing

// char has to be treated differently in Unicode builds: a char argument may
// be used either for a character value (which should be converted into
// wxUniChar) or as an integer value (which should be left as-is). We take
// advantage of the fact that both char and wchar_t are converted into int
// in variadic arguments here.
#if wxUSE_UNICODE
template<typename T>
struct wxArgNormalizerNarrowChar
{
    wxArgNormalizerNarrowChar(T value,
                              const wxFormatString *fmt, unsigned index)
    {
        wxASSERT_ARG_TYPE( fmt, index,
                           wxFormatString::Arg_Char | wxFormatString::Arg_Int );

        // FIXME-UTF8: which one is better default in absence of fmt string
        //             (i.e. when used like e.g. Foo("foo", "bar", 'c', NULL)?
        if ( !fmt || fmt->GetArgumentType(index) == wxFormatString::Arg_Char )
            m_value = wx_truncate_cast(T, wxUniChar(value).GetValue());
        else
            m_value = value;
    }

    int get() const { return m_value; }

    T m_value;
};

template<>
struct wxArgNormalizer<char> : public wxArgNormalizerNarrowChar<char>
{
    wxArgNormalizer(char value,
                    const wxFormatString *fmt, unsigned index)
        : wxArgNormalizerNarrowChar<char>(value, fmt, index) {}
};

template<>
struct wxArgNormalizer<unsigned char>
    : public wxArgNormalizerNarrowChar<unsigned char>
{
    wxArgNormalizer(unsigned char value,
                    const wxFormatString *fmt, unsigned index)
        : wxArgNormalizerNarrowChar<unsigned char>(value, fmt, index) {}
};

template<>
struct wxArgNormalizer<signed char>
    : public wxArgNormalizerNarrowChar<signed char>
{
    wxArgNormalizer(signed char value,
                    const wxFormatString *fmt, unsigned index)
        : wxArgNormalizerNarrowChar<signed char>(value, fmt, index) {}
};

#endif // wxUSE_UNICODE

// convert references:
WX_ARG_NORMALIZER_FORWARD(wxUniChar, const wxUniChar&);
WX_ARG_NORMALIZER_FORWARD(const wxUniCharRef&, const wxUniChar&);
WX_ARG_NORMALIZER_FORWARD(wxUniCharRef, const wxUniChar&);
WX_ARG_NORMALIZER_FORWARD(const wchar_t&, wchar_t);

WX_ARG_NORMALIZER_FORWARD(const char&, char);
WX_ARG_NORMALIZER_FORWARD(const unsigned char&, unsigned char);
WX_ARG_NORMALIZER_FORWARD(const signed char&, signed char);


#undef WX_ARG_NORMALIZER_FORWARD
#undef _WX_ARG_NORMALIZER_FORWARD_IMPL

// NB: Don't #undef wxASSERT_ARG_TYPE here as it's also used in wx/longlong.h.

// ----------------------------------------------------------------------------
// WX_VA_ARG_STRING
// ----------------------------------------------------------------------------

// Replacement for va_arg() for use with strings in functions that accept
// strings normalized by wxArgNormalizer<T>:

struct WXDLLIMPEXP_BASE wxArgNormalizedString
{
    wxArgNormalizedString(const void* ptr) : m_ptr(ptr) {}

    // returns true if non-NULL string was passed in
    bool IsValid() const { return m_ptr != NULL; }
    operator bool() const { return IsValid(); }

    // extracts the string, returns empty string if NULL was passed in
    wxString GetString() const;
    operator wxString() const;

private:
    const void *m_ptr;
};

#define WX_VA_ARG_STRING(ap) wxArgNormalizedString(va_arg(ap, const void*))

// ----------------------------------------------------------------------------
// implementation of the WX_DEFINE_VARARG_* macros
// ----------------------------------------------------------------------------

// NB: The vararg emulation code is limited to 30 variadic and 4 fixed
//     arguments at the moment.
//     If you need more variadic arguments, you need to
//        1) increase the value of _WX_VARARG_MAX_ARGS
//        2) add _WX_VARARG_JOIN_* and _WX_VARARG_ITER_* up to the new
//           _WX_VARARG_MAX_ARGS value to the lists below
//     If you need more fixed arguments, you need to
//        1) increase the value of _WX_VARARG_MAX_FIXED_ARGS
//        2) add _WX_VARARG_FIXED_EXPAND_* and _WX_VARARG_FIXED_UNUSED_EXPAND_*
//           macros below
#define _WX_VARARG_MAX_ARGS        30
#define _WX_VARARG_MAX_FIXED_ARGS   4

#define _WX_VARARG_JOIN_1(m)                                 m(1)
#define _WX_VARARG_JOIN_2(m)       _WX_VARARG_JOIN_1(m),     m(2)
#define _WX_VARARG_JOIN_3(m)       _WX_VARARG_JOIN_2(m),     m(3)
#define _WX_VARARG_JOIN_4(m)       _WX_VARARG_JOIN_3(m),     m(4)
#define _WX_VARARG_JOIN_5(m)       _WX_VARARG_JOIN_4(m),     m(5)
#define _WX_VARARG_JOIN_6(m)       _WX_VARARG_JOIN_5(m),     m(6)
#define _WX_VARARG_JOIN_7(m)       _WX_VARARG_JOIN_6(m),     m(7)
#define _WX_VARARG_JOIN_8(m)       _WX_VARARG_JOIN_7(m),     m(8)
#define _WX_VARARG_JOIN_9(m)       _WX_VARARG_JOIN_8(m),     m(9)
#define _WX_VARARG_JOIN_10(m)      _WX_VARARG_JOIN_9(m),     m(10)
#define _WX_VARARG_JOIN_11(m)      _WX_VARARG_JOIN_10(m),    m(11)
#define _WX_VARARG_JOIN_12(m)      _WX_VARARG_JOIN_11(m),    m(12)
#define _WX_VARARG_JOIN_13(m)      _WX_VARARG_JOIN_12(m),    m(13)
#define _WX_VARARG_JOIN_14(m)      _WX_VARARG_JOIN_13(m),    m(14)
#define _WX_VARARG_JOIN_15(m)      _WX_VARARG_JOIN_14(m),    m(15)
#define _WX_VARARG_JOIN_16(m)      _WX_VARARG_JOIN_15(m),    m(16)
#define _WX_VARARG_JOIN_17(m)      _WX_VARARG_JOIN_16(m),    m(17)
#define _WX_VARARG_JOIN_18(m)      _WX_VARARG_JOIN_17(m),    m(18)
#define _WX_VARARG_JOIN_19(m)      _WX_VARARG_JOIN_18(m),    m(19)
#define _WX_VARARG_JOIN_20(m)      _WX_VARARG_JOIN_19(m),    m(20)
#define _WX_VARARG_JOIN_21(m)      _WX_VARARG_JOIN_20(m),    m(21)
#define _WX_VARARG_JOIN_22(m)      _WX_VARARG_JOIN_21(m),    m(22)
#define _WX_VARARG_JOIN_23(m)      _WX_VARARG_JOIN_22(m),    m(23)
#define _WX_VARARG_JOIN_24(m)      _WX_VARARG_JOIN_23(m),    m(24)
#define _WX_VARARG_JOIN_25(m)      _WX_VARARG_JOIN_24(m),    m(25)
#define _WX_VARARG_JOIN_26(m)      _WX_VARARG_JOIN_25(m),    m(26)
#define _WX_VARARG_JOIN_27(m)      _WX_VARARG_JOIN_26(m),    m(27)
#define _WX_VARARG_JOIN_28(m)      _WX_VARARG_JOIN_27(m),    m(28)
#define _WX_VARARG_JOIN_29(m)      _WX_VARARG_JOIN_28(m),    m(29)
#define _WX_VARARG_JOIN_30(m)      _WX_VARARG_JOIN_29(m),    m(30)

#define _WX_VARARG_ITER_1(m,a,b,c,d,e,f)                                    m(1,a,b,c,d,e,f)
#define _WX_VARARG_ITER_2(m,a,b,c,d,e,f)  _WX_VARARG_ITER_1(m,a,b,c,d,e,f)  m(2,a,b,c,d,e,f)
#define _WX_VARARG_ITER_3(m,a,b,c,d,e,f)  _WX_VARARG_ITER_2(m,a,b,c,d,e,f)  m(3,a,b,c,d,e,f)
#define _WX_VARARG_ITER_4(m,a,b,c,d,e,f)  _WX_VARARG_ITER_3(m,a,b,c,d,e,f)  m(4,a,b,c,d,e,f)
#define _WX_VARARG_ITER_5(m,a,b,c,d,e,f)  _WX_VARARG_ITER_4(m,a,b,c,d,e,f)  m(5,a,b,c,d,e,f)
#define _WX_VARARG_ITER_6(m,a,b,c,d,e,f)  _WX_VARARG_ITER_5(m,a,b,c,d,e,f)  m(6,a,b,c,d,e,f)
#define _WX_VARARG_ITER_7(m,a,b,c,d,e,f)  _WX_VARARG_ITER_6(m,a,b,c,d,e,f)  m(7,a,b,c,d,e,f)
#define _WX_VARARG_ITER_8(m,a,b,c,d,e,f)  _WX_VARARG_ITER_7(m,a,b,c,d,e,f)  m(8,a,b,c,d,e,f)
#define _WX_VARARG_ITER_9(m,a,b,c,d,e,f)  _WX_VARARG_ITER_8(m,a,b,c,d,e,f)  m(9,a,b,c,d,e,f)
#define _WX_VARARG_ITER_10(m,a,b,c,d,e,f) _WX_VARARG_ITER_9(m,a,b,c,d,e,f)  m(10,a,b,c,d,e,f)
#define _WX_VARARG_ITER_11(m,a,b,c,d,e,f) _WX_VARARG_ITER_10(m,a,b,c,d,e,f) m(11,a,b,c,d,e,f)
#define _WX_VARARG_ITER_12(m,a,b,c,d,e,f) _WX_VARARG_ITER_11(m,a,b,c,d,e,f) m(12,a,b,c,d,e,f)
#define _WX_VARARG_ITER_13(m,a,b,c,d,e,f) _WX_VARARG_ITER_12(m,a,b,c,d,e,f) m(13,a,b,c,d,e,f)
#define _WX_VARARG_ITER_14(m,a,b,c,d,e,f) _WX_VARARG_ITER_13(m,a,b,c,d,e,f) m(14,a,b,c,d,e,f)
#define _WX_VARARG_ITER_15(m,a,b,c,d,e,f) _WX_VARARG_ITER_14(m,a,b,c,d,e,f) m(15,a,b,c,d,e,f)
#define _WX_VARARG_ITER_16(m,a,b,c,d,e,f) _WX_VARARG_ITER_15(m,a,b,c,d,e,f) m(16,a,b,c,d,e,f)
#define _WX_VARARG_ITER_17(m,a,b,c,d,e,f) _WX_VARARG_ITER_16(m,a,b,c,d,e,f) m(17,a,b,c,d,e,f)
#define _WX_VARARG_ITER_18(m,a,b,c,d,e,f) _WX_VARARG_ITER_17(m,a,b,c,d,e,f) m(18,a,b,c,d,e,f)
#define _WX_VARARG_ITER_19(m,a,b,c,d,e,f) _WX_VARARG_ITER_18(m,a,b,c,d,e,f) m(19,a,b,c,d,e,f)
#define _WX_VARARG_ITER_20(m,a,b,c,d,e,f) _WX_VARARG_ITER_19(m,a,b,c,d,e,f) m(20,a,b,c,d,e,f)
#define _WX_VARARG_ITER_21(m,a,b,c,d,e,f) _WX_VARARG_ITER_20(m,a,b,c,d,e,f) m(21,a,b,c,d,e,f)
#define _WX_VARARG_ITER_22(m,a,b,c,d,e,f) _WX_VARARG_ITER_21(m,a,b,c,d,e,f) m(22,a,b,c,d,e,f)
#define _WX_VARARG_ITER_23(m,a,b,c,d,e,f) _WX_VARARG_ITER_22(m,a,b,c,d,e,f) m(23,a,b,c,d,e,f)
#define _WX_VARARG_ITER_24(m,a,b,c,d,e,f) _WX_VARARG_ITER_23(m,a,b,c,d,e,f) m(24,a,b,c,d,e,f)
#define _WX_VARARG_ITER_25(m,a,b,c,d,e,f) _WX_VARARG_ITER_24(m,a,b,c,d,e,f) m(25,a,b,c,d,e,f)
#define _WX_VARARG_ITER_26(m,a,b,c,d,e,f) _WX_VARARG_ITER_25(m,a,b,c,d,e,f) m(26,a,b,c,d,e,f)
#define _WX_VARARG_ITER_27(m,a,b,c,d,e,f) _WX_VARARG_ITER_26(m,a,b,c,d,e,f) m(27,a,b,c,d,e,f)
#define _WX_VARARG_ITER_28(m,a,b,c,d,e,f) _WX_VARARG_ITER_27(m,a,b,c,d,e,f) m(28,a,b,c,d,e,f)
#define _WX_VARARG_ITER_29(m,a,b,c,d,e,f) _WX_VARARG_ITER_28(m,a,b,c,d,e,f) m(29,a,b,c,d,e,f)
#define _WX_VARARG_ITER_30(m,a,b,c,d,e,f) _WX_VARARG_ITER_29(m,a,b,c,d,e,f) m(30,a,b,c,d,e,f)


#define _WX_VARARG_FIXED_EXPAND_1(t1) \
         t1 f1
#define _WX_VARARG_FIXED_EXPAND_2(t1,t2) \
         t1 f1, t2 f2
#define _WX_VARARG_FIXED_EXPAND_3(t1,t2,t3) \
         t1 f1, t2 f2, t3 f3
#define _WX_VARARG_FIXED_EXPAND_4(t1,t2,t3,t4) \
         t1 f1, t2 f2, t3 f3, t4 f4

#define _WX_VARARG_FIXED_UNUSED_EXPAND_1(t1) \
         t1 WXUNUSED(f1)
#define _WX_VARARG_FIXED_UNUSED_EXPAND_2(t1,t2) \
         t1 WXUNUSED(f1), t2 WXUNUSED(f2)
#define _WX_VARARG_FIXED_UNUSED_EXPAND_3(t1,t2,t3) \
         t1 WXUNUSED(f1), t2 WXUNUSED(f2), t3 WXUNUSED(f3)
#define _WX_VARARG_FIXED_UNUSED_EXPAND_4(t1,t2,t3,t4) \
         t1 WXUNUSED(f1), t2 WXUNUSED(f2), t3 WXUNUSED(f3), t4 WXUNUSED(f4)

#define _WX_VARARG_FIXED_TYPEDEFS_1(t1) \
             typedef t1 TF1
#define _WX_VARARG_FIXED_TYPEDEFS_2(t1,t2) \
             _WX_VARARG_FIXED_TYPEDEFS_1(t1); typedef t2 TF2
#define _WX_VARARG_FIXED_TYPEDEFS_3(t1,t2,t3) \
             _WX_VARARG_FIXED_TYPEDEFS_2(t1,t2); typedef t3 TF3
#define _WX_VARARG_FIXED_TYPEDEFS_4(t1,t2,t3,t4) \
             _WX_VARARG_FIXED_TYPEDEFS_3(t1,t2,t3); typedef t4 TF4

// This macro expands N-items tuple of fixed arguments types into part of
// function's declaration. For example,
// "_WX_VARARG_FIXED_EXPAND(3, (int, char*, int))" expands into
// "int f1, char* f2, int f3".
#define _WX_VARARG_FIXED_EXPAND(N, args) \
                _WX_VARARG_FIXED_EXPAND_IMPL(N, args)
#define _WX_VARARG_FIXED_EXPAND_IMPL(N, args) \
                _WX_VARARG_FIXED_EXPAND_##N args

// Ditto for unused arguments
#define _WX_VARARG_FIXED_UNUSED_EXPAND(N, args) \
                _WX_VARARG_FIXED_UNUSED_EXPAND_IMPL(N, args)
#define _WX_VARARG_FIXED_UNUSED_EXPAND_IMPL(N, args) \
                _WX_VARARG_FIXED_UNUSED_EXPAND_##N args

// Declarates typedefs for fixed arguments types; i-th fixed argument types
// will have TFi typedef.
#define _WX_VARARG_FIXED_TYPEDEFS(N, args) \
                _WX_VARARG_FIXED_TYPEDEFS_IMPL(N, args)
#define _WX_VARARG_FIXED_TYPEDEFS_IMPL(N, args) \
                _WX_VARARG_FIXED_TYPEDEFS_##N args


// This macro calls another macro 'm' passed as second argument 'N' times,
// with its only argument set to 1..N, and concatenates the results using
// comma as separator.
//
// An example:
//     #define foo(i)  x##i
//     // this expands to "x1,x2,x3,x4"
//     _WX_VARARG_JOIN(4, foo)
//
//
// N must not be greater than _WX_VARARG_MAX_ARGS (=30).
#define _WX_VARARG_JOIN(N, m)             _WX_VARARG_JOIN_IMPL(N, m)
#define _WX_VARARG_JOIN_IMPL(N, m)        _WX_VARARG_JOIN_##N(m)


// This macro calls another macro 'm' passed as second argument 'N' times, with
// its first argument set to 1..N and the remaining arguments set to 'a', 'b',
// 'c', 'd', 'e' and 'f'. The results are separated with whitespace in the
// expansion.
//
// An example:
//     // this macro expands to:
//     //     foo(1,a,b,c,d,e,f)
//     //     foo(2,a,b,c,d,e,f)
//     //     foo(3,a,b,c,d,e,f)
//     _WX_VARARG_ITER(3, foo, a, b, c, d, e, f)
//
// N must not be greater than _WX_VARARG_MAX_ARGS (=30).
#define _WX_VARARG_ITER(N,m,a,b,c,d,e,f) \
        _WX_VARARG_ITER_IMPL(N,m,a,b,c,d,e,f)
#define _WX_VARARG_ITER_IMPL(N,m,a,b,c,d,e,f) \
        _WX_VARARG_ITER_##N(m,a,b,c,d,e,f)

// Generates code snippet for i-th "variadic" argument in vararg function's
// prototype:
#define _WX_VARARG_ARG(i)               T##i a##i

// Like _WX_VARARG_ARG_UNUSED, but outputs argument's type with WXUNUSED:
#define _WX_VARARG_ARG_UNUSED(i)        T##i WXUNUSED(a##i)

// Generates code snippet for i-th type in vararg function's template<...>:
#define _WX_VARARG_TEMPL(i)             typename T##i

// Generates code snippet for passing i-th argument of vararg function
// wrapper to its implementation, normalizing it in the process:
#define _WX_VARARG_PASS_WCHAR(i) \
    wxArgNormalizerWchar<T##i>(a##i, fmt, i).get()
#define _WX_VARARG_PASS_UTF8(i) \
    wxArgNormalizerUtf8<T##i>(a##i, fmt, i).get()


// And the same for fixed arguments, _not_ normalizing it:
#define _WX_VARARG_PASS_FIXED(i)        f##i

#define _WX_VARARG_FIND_FMT(i) \
            (wxFormatStringArgumentFinder<TF##i>::find(f##i))

#define _WX_VARARG_FORMAT_STRING(numfixed, fixed)                             \
    _WX_VARARG_FIXED_TYPEDEFS(numfixed, fixed);                               \
    const wxFormatString *fmt =                                               \
            (_WX_VARARG_JOIN(numfixed, _WX_VARARG_FIND_FMT))

#if wxUSE_UNICODE_UTF8
    #define _WX_VARARG_DO_CALL_UTF8(return_kw, impl, implUtf8, N, numfixed)   \
        return_kw implUtf8(_WX_VARARG_JOIN(numfixed, _WX_VARARG_PASS_FIXED),  \
                        _WX_VARARG_JOIN(N, _WX_VARARG_PASS_UTF8))
    #define _WX_VARARG_DO_CALL0_UTF8(return_kw, impl, implUtf8, numfixed)     \
        return_kw implUtf8(_WX_VARARG_JOIN(numfixed, _WX_VARARG_PASS_FIXED))
#endif // wxUSE_UNICODE_UTF8

#define _WX_VARARG_DO_CALL_WCHAR(return_kw, impl, implUtf8, N, numfixed)      \
    return_kw impl(_WX_VARARG_JOIN(numfixed, _WX_VARARG_PASS_FIXED),          \
                    _WX_VARARG_JOIN(N, _WX_VARARG_PASS_WCHAR))
#define _WX_VARARG_DO_CALL0_WCHAR(return_kw, impl, implUtf8, numfixed)        \
    return_kw impl(_WX_VARARG_JOIN(numfixed, _WX_VARARG_PASS_FIXED))

#if wxUSE_UNICODE_UTF8
    #if wxUSE_UTF8_LOCALE_ONLY
        #define _WX_VARARG_DO_CALL _WX_VARARG_DO_CALL_UTF8
        #define _WX_VARARG_DO_CALL0 _WX_VARARG_DO_CALL0_UTF8
    #else // possibly non-UTF8 locales
        #define _WX_VARARG_DO_CALL(return_kw, impl, implUtf8, N, numfixed)    \
            if ( wxLocaleIsUtf8 )                                             \
              _WX_VARARG_DO_CALL_UTF8(return_kw, impl, implUtf8, N, numfixed);\
            else                                                              \
              _WX_VARARG_DO_CALL_WCHAR(return_kw, impl, implUtf8, N, numfixed)

        #define _WX_VARARG_DO_CALL0(return_kw, impl, implUtf8, numfixed)      \
            if ( wxLocaleIsUtf8 )                                             \
              _WX_VARARG_DO_CALL0_UTF8(return_kw, impl, implUtf8, numfixed);  \
            else                                                              \
              _WX_VARARG_DO_CALL0_WCHAR(return_kw, impl, implUtf8, numfixed)
    #endif // wxUSE_UTF8_LOCALE_ONLY or not
#else // wxUSE_UNICODE_WCHAR or ANSI
    #define _WX_VARARG_DO_CALL _WX_VARARG_DO_CALL_WCHAR
    #define _WX_VARARG_DO_CALL0 _WX_VARARG_DO_CALL0_WCHAR
#endif // wxUSE_UNICODE_UTF8 / wxUSE_UNICODE_WCHAR


// Macro to be used with _WX_VARARG_ITER in the implementation of
// WX_DEFINE_VARARG_FUNC (see its documentation for the meaning of arguments)
#define _WX_VARARG_DEFINE_FUNC(N, rettype, name,                              \
                               impl, implUtf8, numfixed, fixed)               \
    template<_WX_VARARG_JOIN(N, _WX_VARARG_TEMPL)>                            \
    rettype name(_WX_VARARG_FIXED_EXPAND(numfixed, fixed),                    \
                 _WX_VARARG_JOIN(N, _WX_VARARG_ARG))                          \
    {                                                                         \
        _WX_VARARG_FORMAT_STRING(numfixed, fixed);                            \
        _WX_VARARG_DO_CALL(return, impl, implUtf8, N, numfixed);              \
    }

#define _WX_VARARG_DEFINE_FUNC_N0(rettype, name,                              \
                                  impl, implUtf8, numfixed, fixed)            \
    inline rettype name(_WX_VARARG_FIXED_EXPAND(numfixed, fixed))             \
    {                                                                         \
        _WX_VARARG_DO_CALL0(return, impl, implUtf8, numfixed);                \
    }

// Macro to be used with _WX_VARARG_ITER in the implementation of
// WX_DEFINE_VARARG_FUNC_VOID (see its documentation for the meaning of
// arguments; rettype is ignored and is used only to satisfy _WX_VARARG_ITER's
// requirements).
#define _WX_VARARG_DEFINE_FUNC_VOID(N, rettype, name,                         \
                                    impl, implUtf8, numfixed, fixed)          \
    template<_WX_VARARG_JOIN(N, _WX_VARARG_TEMPL)>                            \
    void name(_WX_VARARG_FIXED_EXPAND(numfixed, fixed),                       \
                 _WX_VARARG_JOIN(N, _WX_VARARG_ARG))                          \
    {                                                                         \
        _WX_VARARG_FORMAT_STRING(numfixed, fixed);                            \
        _WX_VARARG_DO_CALL(wxEMPTY_PARAMETER_VALUE,                           \
                           impl, implUtf8, N, numfixed);                      \
    }

#define _WX_VARARG_DEFINE_FUNC_VOID_N0(name, impl, implUtf8, numfixed, fixed) \
    inline void name(_WX_VARARG_FIXED_EXPAND(numfixed, fixed))                \
    {                                                                         \
        _WX_VARARG_DO_CALL0(wxEMPTY_PARAMETER_VALUE,                          \
                            impl, implUtf8, numfixed);                        \
    }

// Macro to be used with _WX_VARARG_ITER in the implementation of
// WX_DEFINE_VARARG_FUNC_CTOR (see its documentation for the meaning of
// arguments; rettype is ignored and is used only to satisfy _WX_VARARG_ITER's
// requirements).
#define _WX_VARARG_DEFINE_FUNC_CTOR(N, rettype, name,                         \
                                    impl, implUtf8, numfixed, fixed)          \
    template<_WX_VARARG_JOIN(N, _WX_VARARG_TEMPL)>                            \
    name(_WX_VARARG_FIXED_EXPAND(numfixed, fixed),                            \
                _WX_VARARG_JOIN(N, _WX_VARARG_ARG))                           \
    {                                                                         \
        _WX_VARARG_FORMAT_STRING(numfixed, fixed);                            \
        _WX_VARARG_DO_CALL(wxEMPTY_PARAMETER_VALUE,                           \
                           impl, implUtf8, N, numfixed);                      \
    }

#define _WX_VARARG_DEFINE_FUNC_CTOR_N0(name, impl, implUtf8, numfixed, fixed) \
    inline name(_WX_VARARG_FIXED_EXPAND(numfixed, fixed))                     \
    {                                                                         \
        _WX_VARARG_DO_CALL0(wxEMPTY_PARAMETER_VALUE,                          \
                            impl, implUtf8, numfixed);                        \
    }

// Macro to be used with _WX_VARARG_ITER in the implementation of
// WX_DEFINE_VARARG_FUNC_NOP, i.e. empty stub for a disabled vararg function.
// The rettype and impl arguments are ignored.
#define _WX_VARARG_DEFINE_FUNC_NOP(N, rettype, name,                          \
                                   impl, implUtf8, numfixed, fixed)           \
    template<_WX_VARARG_JOIN(N, _WX_VARARG_TEMPL)>                            \
    void name(_WX_VARARG_FIXED_UNUSED_EXPAND(numfixed, fixed),                \
                 _WX_VARARG_JOIN(N, _WX_VARARG_ARG_UNUSED))                   \
    {}

#define _WX_VARARG_DEFINE_FUNC_NOP_N0(name, numfixed, fixed)                  \
    inline void name(_WX_VARARG_FIXED_UNUSED_EXPAND(numfixed, fixed))         \
    {}


// ----------------------------------------------------------------------------
// workaround for OpenWatcom bug #351
// ----------------------------------------------------------------------------

#ifdef __WATCOMC__
// workaround for http://bugzilla.openwatcom.org/show_bug.cgi?id=351

// This macro can be used to forward a 'vararg' template to another one with
// different fixed arguments types. Parameters are same as for
// WX_DEFINE_VARARG_FUNC (rettype=void can be used here), 'convfixed' is how
// to convert fixed arguments. For example, this is typical code for dealing
// with different forms of format string:
//
// WX_DEFINE_VARARG_FUNC_VOID(Printf, 1, (const wxFormatString&),
//                            DoPrintfWchar, DoPrintfUtf8)
// #ifdef __WATCOMC__
// WX_VARARG_WATCOM_WORKAROUND(void, Printf, 1, (const wxString&),
//                             (wxFormatString(f1)))
// WX_VARARG_WATCOM_WORKAROUND(void, Printf, 1, (const char*),
//                             (wxFormatString(f1)))
// ...
#define WX_VARARG_WATCOM_WORKAROUND(rettype, name, numfixed, fixed, convfixed)\
    _WX_VARARG_ITER(_WX_VARARG_MAX_ARGS,                                      \
                    _WX_VARARG_WATCOM_WORKAROUND,                             \
                    rettype, name, convfixed, dummy, numfixed, fixed)

#define WX_VARARG_WATCOM_WORKAROUND_CTOR(name, numfixed, fixed, convfixed)    \
    _WX_VARARG_ITER(_WX_VARARG_MAX_ARGS,                                      \
                    _WX_VARARG_WATCOM_WORKAROUND_CTOR,                        \
                    dummy, name, convfixed, dummy, numfixed, fixed)

#define _WX_VARARG_WATCOM_UNPACK_1(a1)               a1
#define _WX_VARARG_WATCOM_UNPACK_2(a1, a2)           a1, a2
#define _WX_VARARG_WATCOM_UNPACK_3(a1, a2, a3)       a1, a2, a3
#define _WX_VARARG_WATCOM_UNPACK_4(a1, a2, a3, a4)   a1, a2, a3, a4
#define _WX_VARARG_WATCOM_UNPACK(N, convfixed) \
        _WX_VARARG_WATCOM_UNPACK_##N convfixed

#define _WX_VARARG_PASS_WATCOM(i) a##i

#define _WX_VARARG_WATCOM_WORKAROUND(N, rettype, name,                        \
                                     convfixed, dummy, numfixed, fixed)       \
    template<_WX_VARARG_JOIN(N, _WX_VARARG_TEMPL)>                            \
    rettype name(_WX_VARARG_FIXED_EXPAND(numfixed, fixed),                    \
                 _WX_VARARG_JOIN(N, _WX_VARARG_ARG))                          \
    {                                                                         \
         return name(_WX_VARARG_WATCOM_UNPACK(numfixed, convfixed),           \
                     _WX_VARARG_JOIN(N, _WX_VARARG_PASS_WATCOM));             \
    }

#define _WX_VARARG_WATCOM_WORKAROUND_CTOR(N, dummy1, name,                    \
                                     convfixed, dummy2, numfixed, fixed)      \
    template<_WX_VARARG_JOIN(N, _WX_VARARG_TEMPL)>                            \
    name(_WX_VARARG_FIXED_EXPAND(numfixed, fixed),                            \
                 _WX_VARARG_JOIN(N, _WX_VARARG_ARG))                          \
    {                                                                         \
         name(_WX_VARARG_WATCOM_UNPACK(numfixed, convfixed),                  \
                     _WX_VARARG_JOIN(N, _WX_VARARG_PASS_WATCOM));             \
    }

#endif // __WATCOMC__

#endif // _WX_STRVARARG_H_
