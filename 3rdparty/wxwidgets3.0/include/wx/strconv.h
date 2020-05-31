///////////////////////////////////////////////////////////////////////////////
// Name:        wx/strconv.h
// Purpose:     conversion routines for char sets any Unicode
// Author:      Ove Kaaven, Robert Roebling, Vadim Zeitlin
// Modified by:
// Created:     29/01/98
// Copyright:   (c) 1998 Ove Kaaven, Robert Roebling
//              (c) 1998-2006 Vadim Zeitlin
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_STRCONV_H_
#define _WX_STRCONV_H_

#include "wx/defs.h"
#include "wx/chartype.h"
#include "wx/buffer.h"

#ifdef __DIGITALMARS__
#include "typeinfo.h"
#endif

#if defined(__VISAGECPP__) && __IBMCPP__ >= 400
#  undef __BSEXCPT__
#endif

#include <stdlib.h>

class WXDLLIMPEXP_FWD_BASE wxString;

// the error value returned by wxMBConv methods
#define wxCONV_FAILED ((size_t)-1)

// ----------------------------------------------------------------------------
// wxMBConv (abstract base class for conversions)
// ----------------------------------------------------------------------------

// When deriving a new class from wxMBConv you must reimplement ToWChar() and
// FromWChar() methods which are not pure virtual only for historical reasons,
// don't let the fact that the existing classes implement MB2WC/WC2MB() instead
// confuse you.
//
// You also have to implement Clone() to allow copying the conversions
// polymorphically.
//
// And you might need to override GetMBNulLen() as well.
class WXDLLIMPEXP_BASE wxMBConv
{
public:
    // The functions doing actual conversion from/to narrow to/from wide
    // character strings.
    //
    // On success, the return value is the length (i.e. the number of
    // characters, not bytes) of the converted string including any trailing
    // L'\0' or (possibly multiple) '\0'(s). If the conversion fails or if
    // there is not enough space for everything, including the trailing NUL
    // character(s), in the output buffer, wxCONV_FAILED is returned.
    //
    // In the special case when dst is NULL (the value of dstLen is ignored
    // then) the return value is the length of the needed buffer but nothing
    // happens otherwise. If srcLen is wxNO_LEN, the entire string, up to and
    // including the trailing NUL(s), is converted, otherwise exactly srcLen
    // bytes are.
    //
    // Typical usage:
    //
    //          size_t dstLen = conv.ToWChar(NULL, 0, src);
    //          if ( dstLen == wxCONV_FAILED )
    //              ... handle error ...
    //          wchar_t *wbuf = new wchar_t[dstLen];
    //          conv.ToWChar(wbuf, dstLen, src);
    //          ... work with wbuf ...
    //          delete [] wbuf;
    //
    virtual size_t ToWChar(wchar_t *dst, size_t dstLen,
                           const char *src, size_t srcLen = wxNO_LEN) const;

    virtual size_t FromWChar(char *dst, size_t dstLen,
                             const wchar_t *src, size_t srcLen = wxNO_LEN) const;


    // Convenience functions for translating NUL-terminated strings: returns
    // the buffer containing the converted string or NULL pointer if the
    // conversion failed.
    const wxWCharBuffer cMB2WC(const char *in) const;
    const wxCharBuffer cWC2MB(const wchar_t *in) const;

    // Convenience functions for converting strings which may contain embedded
    // NULs and don't have to be NUL-terminated.
    //
    // inLen is the length of the buffer including trailing NUL if any or
    // wxNO_LEN if the input is NUL-terminated.
    //
    // outLen receives, if not NULL, the length of the converted string or 0 if
    // the conversion failed (returning 0 and not -1 in this case makes it
    // difficult to distinguish between failed conversion and empty input but
    // this is done for backwards compatibility). Notice that the rules for
    // whether outLen accounts or not for the last NUL are the same as for
    // To/FromWChar() above: if inLen is specified, outLen is exactly the
    // number of characters converted, whether the last one of them was NUL or
    // not. But if inLen == wxNO_LEN then outLen doesn't account for the last
    // NUL even though it is present.
    const wxWCharBuffer
        cMB2WC(const char *in, size_t inLen, size_t *outLen) const;
    const wxCharBuffer
        cWC2MB(const wchar_t *in, size_t inLen, size_t *outLen) const;

    // And yet more convenience functions for converting the entire buffers:
    // these are the simplest and least error-prone as you never need to bother
    // with lengths/sizes directly.
    const wxWCharBuffer cMB2WC(const wxScopedCharBuffer& in) const;
    const wxCharBuffer cWC2MB(const wxScopedWCharBuffer& in) const;

    // convenience functions for converting MB or WC to/from wxWin default
#if wxUSE_UNICODE
    const wxWCharBuffer cMB2WX(const char *psz) const { return cMB2WC(psz); }
    const wxCharBuffer cWX2MB(const wchar_t *psz) const { return cWC2MB(psz); }
    const wchar_t* cWC2WX(const wchar_t *psz) const { return psz; }
    const wchar_t* cWX2WC(const wchar_t *psz) const { return psz; }
#else // ANSI
    const char* cMB2WX(const char *psz) const { return psz; }
    const char* cWX2MB(const char *psz) const { return psz; }
    const wxCharBuffer cWC2WX(const wchar_t *psz) const { return cWC2MB(psz); }
    const wxWCharBuffer cWX2WC(const char *psz) const { return cMB2WC(psz); }
#endif // Unicode/ANSI

    // this function is used in the implementation of cMB2WC() to distinguish
    // between the following cases:
    //
    //      a) var width encoding with strings terminated by a single NUL
    //         (usual multibyte encodings): return 1 in this case
    //      b) fixed width encoding with 2 bytes/char and so terminated by
    //         2 NULs (UTF-16/UCS-2 and variants): return 2 in this case
    //      c) fixed width encoding with 4 bytes/char and so terminated by
    //         4 NULs (UTF-32/UCS-4 and variants): return 4 in this case
    //
    // anything else is not supported currently and -1 should be returned
    virtual size_t GetMBNulLen() const { return 1; }

    // return the maximal value currently returned by GetMBNulLen() for any
    // encoding
    static size_t GetMaxMBNulLen() { return 4 /* for UTF-32 */; }

#if wxUSE_UNICODE_UTF8
    // return true if the converter's charset is UTF-8, i.e. char* strings
    // decoded using this object can be directly copied to wxString's internal
    // storage without converting to WC and than back to UTF-8 MB string
    virtual bool IsUTF8() const { return false; }
#endif

    // The old conversion functions. The existing classes currently mostly
    // implement these ones but we're in transition to using To/FromWChar()
    // instead and any new classes should implement just the new functions.
    // For now, however, we provide default implementation of To/FromWChar() in
    // this base class in terms of MB2WC/WC2MB() to avoid having to rewrite all
    // the conversions at once.
    //
    // On success, the return value is the length (i.e. the number of
    // characters, not bytes) not counting the trailing NUL(s) of the converted
    // string. On failure, (size_t)-1 is returned. In the special case when
    // outputBuf is NULL the return value is the same one but nothing is
    // written to the buffer.
    //
    // Note that outLen is the length of the output buffer, not the length of
    // the input (which is always supposed to be terminated by one or more
    // NULs, as appropriate for the encoding)!
    virtual size_t MB2WC(wchar_t *out, const char *in, size_t outLen) const;
    virtual size_t WC2MB(char *out, const wchar_t *in, size_t outLen) const;


    // make a heap-allocated copy of this object
    virtual wxMBConv *Clone() const = 0;

    // virtual dtor for any base class
    virtual ~wxMBConv();
};

// ----------------------------------------------------------------------------
// wxMBConvLibc uses standard mbstowcs() and wcstombs() functions for
//              conversion (hence it depends on the current locale)
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxMBConvLibc : public wxMBConv
{
public:
    virtual size_t MB2WC(wchar_t *outputBuf, const char *psz, size_t outputSize) const;
    virtual size_t WC2MB(char *outputBuf, const wchar_t *psz, size_t outputSize) const;

    virtual wxMBConv *Clone() const { return new wxMBConvLibc; }

#if wxUSE_UNICODE_UTF8
    virtual bool IsUTF8() const { return wxLocaleIsUtf8; }
#endif
};

#ifdef __UNIX__

// ----------------------------------------------------------------------------
// wxConvBrokenFileNames is made for Unix in Unicode mode when
// files are accidentally written in an encoding which is not
// the system encoding. Typically, the system encoding will be
// UTF8 but there might be files stored in ISO8859-1 on disk.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxConvBrokenFileNames : public wxMBConv
{
public:
    wxConvBrokenFileNames(const wxString& charset);
    wxConvBrokenFileNames(const wxConvBrokenFileNames& conv)
        : wxMBConv(),
          m_conv(conv.m_conv ? conv.m_conv->Clone() : NULL)
    {
    }
    virtual ~wxConvBrokenFileNames() { delete m_conv; }

    virtual size_t MB2WC(wchar_t *out, const char *in, size_t outLen) const
    {
        return m_conv->MB2WC(out, in, outLen);
    }

    virtual size_t WC2MB(char *out, const wchar_t *in, size_t outLen) const
    {
        return m_conv->WC2MB(out, in, outLen);
    }

    virtual size_t GetMBNulLen() const
    {
        // cast needed to call a private function
        return m_conv->GetMBNulLen();
    }

#if wxUSE_UNICODE_UTF8
    virtual bool IsUTF8() const { return m_conv->IsUTF8(); }
#endif

    virtual wxMBConv *Clone() const { return new wxConvBrokenFileNames(*this); }

private:
    // the conversion object we forward to
    wxMBConv *m_conv;

    wxDECLARE_NO_ASSIGN_CLASS(wxConvBrokenFileNames);
};

#endif // __UNIX__

// ----------------------------------------------------------------------------
// wxMBConvUTF7 (for conversion using UTF7 encoding)
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxMBConvUTF7 : public wxMBConv
{
public:
    wxMBConvUTF7() { }

    // compiler-generated copy ctor, assignment operator and dtor are ok
    // (assuming it's ok to copy the shift state -- not really sure about it)

    virtual size_t ToWChar(wchar_t *dst, size_t dstLen,
                           const char *src, size_t srcLen = wxNO_LEN) const;
    virtual size_t FromWChar(char *dst, size_t dstLen,
                             const wchar_t *src, size_t srcLen = wxNO_LEN) const;

    virtual wxMBConv *Clone() const { return new wxMBConvUTF7; }

private:
    // UTF-7 decoder/encoder may be in direct mode or in shifted mode after a
    // '+' (and until the '-' or any other non-base64 character)
    struct StateMode
    {
        enum Mode
        {
            Direct,     // pass through state
            Shifted     // after a '+' (and before '-')
        };
    };

    // the current decoder state: this is only used by ToWChar() if srcLen
    // parameter is not wxNO_LEN, when working on the entire NUL-terminated
    // strings we neither update nor use the state
    class DecoderState : private StateMode
    {
    private:
        // current state: this one is private as we want to enforce the use of
        // ToDirect/ToShifted() methods below
        Mode mode;

    public:
        // the initial state is direct
        DecoderState() { mode = Direct; }

        // switch to/from shifted mode
        void ToDirect() { mode = Direct; }
        void ToShifted() { mode = Shifted; accum = bit = 0; isLSB = false; }

        bool IsDirect() const { return mode == Direct; }
        bool IsShifted() const { return mode == Shifted; }


        // these variables are only used in shifted mode

        unsigned int accum; // accumulator of the bit we've already got
        unsigned int bit;   // the number of bits consumed mod 8
        unsigned char msb;  // the high byte of UTF-16 word
        bool isLSB;         // whether we're decoding LSB or MSB of UTF-16 word
    };

    DecoderState m_stateDecoder;


    // encoder state is simpler as we always receive entire Unicode characters
    // on input
    class EncoderState : private StateMode
    {
    private:
        Mode mode;

    public:
        EncoderState() { mode = Direct; }

        void ToDirect() { mode = Direct; }
        void ToShifted() { mode = Shifted; accum = bit = 0; }

        bool IsDirect() const { return mode == Direct; }
        bool IsShifted() const { return mode == Shifted; }

        unsigned int accum;
        unsigned int bit;
    };

    EncoderState m_stateEncoder;
};

// ----------------------------------------------------------------------------
// wxMBConvUTF8 (for conversion using UTF8 encoding)
// ----------------------------------------------------------------------------

// this is the real UTF-8 conversion class, it has to be called "strict UTF-8"
// for compatibility reasons: the wxMBConvUTF8 class below also supports lossy
// conversions if it is created with non default options
class WXDLLIMPEXP_BASE wxMBConvStrictUTF8 : public wxMBConv
{
public:
    // compiler-generated default ctor and other methods are ok

    virtual size_t ToWChar(wchar_t *dst, size_t dstLen,
                           const char *src, size_t srcLen = wxNO_LEN) const;
    virtual size_t FromWChar(char *dst, size_t dstLen,
                             const wchar_t *src, size_t srcLen = wxNO_LEN) const;

    virtual wxMBConv *Clone() const { return new wxMBConvStrictUTF8(); }

#if wxUSE_UNICODE_UTF8
    // NB: other mapping modes are not, strictly speaking, UTF-8, so we can't
    //     take the shortcut in that case
    virtual bool IsUTF8() const { return true; }
#endif
};

class WXDLLIMPEXP_BASE wxMBConvUTF8 : public wxMBConvStrictUTF8
{
public:
    enum
    {
        MAP_INVALID_UTF8_NOT = 0,
        MAP_INVALID_UTF8_TO_PUA = 1,
        MAP_INVALID_UTF8_TO_OCTAL = 2
    };

    wxMBConvUTF8(int options = MAP_INVALID_UTF8_NOT) : m_options(options) { }

    virtual size_t ToWChar(wchar_t *dst, size_t dstLen,
                           const char *src, size_t srcLen = wxNO_LEN) const;
    virtual size_t FromWChar(char *dst, size_t dstLen,
                             const wchar_t *src, size_t srcLen = wxNO_LEN) const;

    virtual wxMBConv *Clone() const { return new wxMBConvUTF8(m_options); }

#if wxUSE_UNICODE_UTF8
    // NB: other mapping modes are not, strictly speaking, UTF-8, so we can't
    //     take the shortcut in that case
    virtual bool IsUTF8() const { return m_options == MAP_INVALID_UTF8_NOT; }
#endif

private:
    int m_options;
};

// ----------------------------------------------------------------------------
// wxMBConvUTF16Base: for both LE and BE variants
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxMBConvUTF16Base : public wxMBConv
{
public:
    enum { BYTES_PER_CHAR = 2 };

    virtual size_t GetMBNulLen() const { return BYTES_PER_CHAR; }

protected:
    // return the length of the buffer using srcLen if it's not wxNO_LEN and
    // computing the length ourselves if it is; also checks that the length is
    // even if specified as we need an entire number of UTF-16 characters and
    // returns wxNO_LEN which indicates error if it is odd
    static size_t GetLength(const char *src, size_t srcLen);
};

// ----------------------------------------------------------------------------
// wxMBConvUTF16LE (for conversion using UTF16 Little Endian encoding)
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxMBConvUTF16LE : public wxMBConvUTF16Base
{
public:
    virtual size_t ToWChar(wchar_t *dst, size_t dstLen,
                           const char *src, size_t srcLen = wxNO_LEN) const;
    virtual size_t FromWChar(char *dst, size_t dstLen,
                             const wchar_t *src, size_t srcLen = wxNO_LEN) const;
    virtual wxMBConv *Clone() const { return new wxMBConvUTF16LE; }
};

// ----------------------------------------------------------------------------
// wxMBConvUTF16BE (for conversion using UTF16 Big Endian encoding)
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxMBConvUTF16BE : public wxMBConvUTF16Base
{
public:
    virtual size_t ToWChar(wchar_t *dst, size_t dstLen,
                           const char *src, size_t srcLen = wxNO_LEN) const;
    virtual size_t FromWChar(char *dst, size_t dstLen,
                             const wchar_t *src, size_t srcLen = wxNO_LEN) const;
    virtual wxMBConv *Clone() const { return new wxMBConvUTF16BE; }
};

// ----------------------------------------------------------------------------
// wxMBConvUTF32Base: base class for both LE and BE variants
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxMBConvUTF32Base : public wxMBConv
{
public:
    enum { BYTES_PER_CHAR = 4 };

    virtual size_t GetMBNulLen() const { return BYTES_PER_CHAR; }

protected:
    // this is similar to wxMBConvUTF16Base method with the same name except
    // that, of course, it verifies that length is divisible by 4 if given and
    // not by 2
    static size_t GetLength(const char *src, size_t srcLen);
};

// ----------------------------------------------------------------------------
// wxMBConvUTF32LE (for conversion using UTF32 Little Endian encoding)
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxMBConvUTF32LE : public wxMBConvUTF32Base
{
public:
    virtual size_t ToWChar(wchar_t *dst, size_t dstLen,
                           const char *src, size_t srcLen = wxNO_LEN) const;
    virtual size_t FromWChar(char *dst, size_t dstLen,
                             const wchar_t *src, size_t srcLen = wxNO_LEN) const;
    virtual wxMBConv *Clone() const { return new wxMBConvUTF32LE; }
};

// ----------------------------------------------------------------------------
// wxMBConvUTF32BE (for conversion using UTF32 Big Endian encoding)
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_BASE wxMBConvUTF32BE : public wxMBConvUTF32Base
{
public:
    virtual size_t ToWChar(wchar_t *dst, size_t dstLen,
                           const char *src, size_t srcLen = wxNO_LEN) const;
    virtual size_t FromWChar(char *dst, size_t dstLen,
                             const wchar_t *src, size_t srcLen = wxNO_LEN) const;
    virtual wxMBConv *Clone() const { return new wxMBConvUTF32BE; }
};

// ----------------------------------------------------------------------------
// wxCSConv (for conversion based on loadable char sets)
// ----------------------------------------------------------------------------

#include "wx/fontenc.h"

class WXDLLIMPEXP_BASE wxCSConv : public wxMBConv
{
public:
    // we can be created either from charset name or from an encoding constant
    // but we can't have both at once
    wxCSConv(const wxString& charset);
    wxCSConv(wxFontEncoding encoding);

    wxCSConv(const wxCSConv& conv);
    virtual ~wxCSConv();

    wxCSConv& operator=(const wxCSConv& conv);

    virtual size_t ToWChar(wchar_t *dst, size_t dstLen,
                           const char *src, size_t srcLen = wxNO_LEN) const;
    virtual size_t FromWChar(char *dst, size_t dstLen,
                             const wchar_t *src, size_t srcLen = wxNO_LEN) const;
    virtual size_t GetMBNulLen() const;

#if wxUSE_UNICODE_UTF8
    virtual bool IsUTF8() const;
#endif

    virtual wxMBConv *Clone() const { return new wxCSConv(*this); }

    void Clear();

    // return true if the conversion could be initialized successfully
    bool IsOk() const;

private:
    // common part of all ctors
    void Init();

    // Creates the conversion to use, called from all ctors to initialize
    // m_convReal.
    wxMBConv *DoCreate() const;

    // Set the name (may be only called when m_name == NULL), makes copy of
    // the charset string.
    void SetName(const char *charset);

    // Set m_encoding field respecting the rules below, i.e. making sure it has
    // a valid value if m_name == NULL (thus this should be always called after
    // SetName()).
    //
    // Input encoding may be valid or not.
    void SetEncoding(wxFontEncoding encoding);


    // The encoding we use is specified by the two fields below:
    //
    //  1. If m_name != NULL, m_encoding corresponds to it if it's one of
    //     encodings we know about (i.e. member of wxFontEncoding) or is
    //     wxFONTENCODING_SYSTEM otherwise.
    //
    //  2. If m_name == NULL, m_encoding is always valid, i.e. not one of
    //     wxFONTENCODING_{SYSTEM,DEFAULT,MAX}.
    char *m_name;
    wxFontEncoding m_encoding;

    // The conversion object for our encoding or NULL if we failed to create it
    // in which case we fall back to hard-coded ISO8859-1 conversion.
    wxMBConv *m_convReal;
};


// ----------------------------------------------------------------------------
// declare predefined conversion objects
// ----------------------------------------------------------------------------

// Note: this macro is an implementation detail (see the comment in
// strconv.cpp). The wxGet_XXX() and wxGet_XXXPtr() functions shouldn't be
// used by user code and neither should XXXPtr, use the wxConvXXX macro
// instead.
#define WX_DECLARE_GLOBAL_CONV(klass, name)                             \
    extern WXDLLIMPEXP_DATA_BASE(klass*) name##Ptr;                     \
    extern WXDLLIMPEXP_BASE klass* wxGet_##name##Ptr();                 \
    inline klass& wxGet_##name()                                        \
    {                                                                   \
        if ( !name##Ptr )                                               \
            name##Ptr = wxGet_##name##Ptr();                            \
        return *name##Ptr;                                              \
    }


// conversion to be used with all standard functions affected by locale, e.g.
// strtol(), strftime(), ...
WX_DECLARE_GLOBAL_CONV(wxMBConv, wxConvLibc)
#define wxConvLibc wxGet_wxConvLibc()

// conversion ISO-8859-1/UTF-7/UTF-8 <-> wchar_t
WX_DECLARE_GLOBAL_CONV(wxCSConv, wxConvISO8859_1)
#define wxConvISO8859_1 wxGet_wxConvISO8859_1()

WX_DECLARE_GLOBAL_CONV(wxMBConvStrictUTF8, wxConvUTF8)
#define wxConvUTF8 wxGet_wxConvUTF8()

WX_DECLARE_GLOBAL_CONV(wxMBConvUTF7, wxConvUTF7)
#define wxConvUTF7 wxGet_wxConvUTF7()

// conversion used for the file names on the systems where they're not Unicode
// (basically anything except Windows)
//
// this is used by all file functions, can be changed by the application
//
// by default UTF-8 under Mac OS X and wxConvLibc elsewhere (but it's not used
// under Windows normally)
extern WXDLLIMPEXP_DATA_BASE(wxMBConv *) wxConvFileName;

// backwards compatible define
#define wxConvFile (*wxConvFileName)

// the current conversion object, may be set to any conversion, is used by
// default in a couple of places inside wx (initially same as wxConvLibc)
extern WXDLLIMPEXP_DATA_BASE(wxMBConv *) wxConvCurrent;

// the conversion corresponding to the current locale
WX_DECLARE_GLOBAL_CONV(wxCSConv, wxConvLocal)
#define wxConvLocal wxGet_wxConvLocal()

// the conversion corresponding to the encoding of the standard UI elements
//
// by default this is the same as wxConvLocal but may be changed if the program
// needs to use a fixed encoding
extern WXDLLIMPEXP_DATA_BASE(wxMBConv *) wxConvUI;

#undef WX_DECLARE_GLOBAL_CONV

// ----------------------------------------------------------------------------
// endianness-dependent conversions
// ----------------------------------------------------------------------------

#ifdef WORDS_BIGENDIAN
    typedef wxMBConvUTF16BE wxMBConvUTF16;
    typedef wxMBConvUTF32BE wxMBConvUTF32;
#else
    typedef wxMBConvUTF16LE wxMBConvUTF16;
    typedef wxMBConvUTF32LE wxMBConvUTF32;
#endif

// ----------------------------------------------------------------------------
// filename conversion macros
// ----------------------------------------------------------------------------

// filenames are multibyte on Unix and widechar on Windows
#if wxMBFILES && wxUSE_UNICODE
    #define wxFNCONV(name) wxConvFileName->cWX2MB(name)
    #define wxFNSTRINGCAST wxMBSTRINGCAST
#else
#if defined( __WXOSX_OR_COCOA__ ) && wxMBFILES
    #define wxFNCONV(name) wxConvFileName->cWC2MB( wxConvLocal.cWX2WC(name) )
#else
    #define wxFNCONV(name) name
#endif
    #define wxFNSTRINGCAST WXSTRINGCAST
#endif

// ----------------------------------------------------------------------------
// macros for the most common conversions
// ----------------------------------------------------------------------------

#if wxUSE_UNICODE
    #define wxConvertWX2MB(s)   wxConvCurrent->cWX2MB(s)
    #define wxConvertMB2WX(s)   wxConvCurrent->cMB2WX(s)

    // these functions should be used when the conversions really, really have
    // to succeed (usually because we pass their results to a standard C
    // function which would crash if we passed NULL to it), so these functions
    // always return a valid pointer if their argument is non-NULL

    // this function safety is achieved by trying wxConvLibc first, wxConvUTF8
    // next if it fails and, finally, wxConvISO8859_1 which always succeeds
    extern WXDLLIMPEXP_BASE wxWCharBuffer wxSafeConvertMB2WX(const char *s);

    // this function uses wxConvLibc and wxConvUTF8(MAP_INVALID_UTF8_TO_OCTAL)
    // if it fails
    extern WXDLLIMPEXP_BASE wxCharBuffer wxSafeConvertWX2MB(const wchar_t *ws);
#else // ANSI
    // no conversions to do
    #define wxConvertWX2MB(s)   (s)
    #define wxConvertMB2WX(s)   (s)
    #define wxSafeConvertMB2WX(s) (s)
    #define wxSafeConvertWX2MB(s) (s)
#endif // Unicode/ANSI

#endif // _WX_STRCONV_H_

