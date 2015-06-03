///////////////////////////////////////////////////////////////////////////////
// Name:        wx/unichar.h
// Purpose:     wxUniChar and wxUniCharRef classes
// Author:      Vaclav Slavik
// Created:     2007-03-19
// Copyright:   (c) 2007 REA Elektronik GmbH
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNICHAR_H_
#define _WX_UNICHAR_H_

#include "wx/defs.h"
#include "wx/chartype.h"
#include "wx/stringimpl.h"

#include <algorithm>        // only for std::swap specialization below

class WXDLLIMPEXP_FWD_BASE wxUniCharRef;
class WXDLLIMPEXP_FWD_BASE wxString;

// This class represents single Unicode character. It can be converted to
// and from char or wchar_t and implements commonly used character operations.
class WXDLLIMPEXP_BASE wxUniChar
{
public:
    // NB: this is not wchar_t on purpose, it needs to represent the entire
    //     Unicode code points range and wchar_t may be too small for that
    //     (e.g. on Win32 where wchar_t* is encoded in UTF-16)
    typedef wxUint32 value_type;

    wxUniChar() : m_value(0) {}

    // Create the character from 8bit character value encoded in the current
    // locale's charset.
    wxUniChar(char c) { m_value = From8bit(c); }
    wxUniChar(unsigned char c) { m_value = From8bit((char)c); }

#define wxUNICHAR_DEFINE_CTOR(type) \
    wxUniChar(type c) { m_value = (value_type)c; }
    wxDO_FOR_INT_TYPES(wxUNICHAR_DEFINE_CTOR)
#undef wxUNICHAR_DEFINE_CTOR

    wxUniChar(const wxUniCharRef& c);

    // Returns Unicode code point value of the character
    value_type GetValue() const { return m_value; }

#if wxUSE_UNICODE_UTF8
    // buffer for single UTF-8 character
    struct Utf8CharBuffer
    {
        char data[5];
        operator const char*() const { return data; }
    };

    // returns the character encoded as UTF-8
    // (NB: implemented in stringops.cpp)
    Utf8CharBuffer AsUTF8() const;
#endif // wxUSE_UNICODE_UTF8

    // Returns true if the character is an ASCII character:
    bool IsAscii() const { return m_value < 0x80; }

    // Returns true if the character is representable as a single byte in the
    // current locale encoding and return this byte in output argument c (which
    // must be non-NULL)
    bool GetAsChar(char *c) const
    {
#if wxUSE_UNICODE
        if ( !IsAscii() )
        {
#if !wxUSE_UTF8_LOCALE_ONLY
            if ( GetAsHi8bit(m_value, c) )
                return true;
#endif // !wxUSE_UTF8_LOCALE_ONLY

            return false;
        }
#endif // wxUSE_UNICODE

        *c = wx_truncate_cast(char, m_value);
        return true;
    }

    // Conversions to char and wchar_t types: all of those are needed to be
    // able to pass wxUniChars to verious standard narrow and wide character
    // functions
    operator char() const { return To8bit(m_value); }
    operator unsigned char() const { return (unsigned char)To8bit(m_value); }

#define wxUNICHAR_DEFINE_OPERATOR_PAREN(type) \
    operator type() const { return (type)m_value; }
    wxDO_FOR_INT_TYPES(wxUNICHAR_DEFINE_OPERATOR_PAREN)
#undef wxUNICHAR_DEFINE_OPERATOR_PAREN

    // We need this operator for the "*p" part of expressions like "for (
    // const_iterator p = begin() + nStart; *p; ++p )". In this case,
    // compilation would fail without it because the conversion to bool would
    // be ambiguous (there are all these int types conversions...). (And adding
    // operator unspecified_bool_type() would only makes the ambiguity worse.)
    operator bool() const { return m_value != 0; }
    bool operator!() const { return !((bool)*this); }

    // And this one is needed by some (not all, but not using ifdefs makes the
    // code easier) compilers to parse "str[0] && *p" successfully
    bool operator&&(bool v) const { return (bool)*this && v; }

    // Assignment operators:
    wxUniChar& operator=(const wxUniChar& c) { if (&c != this) m_value = c.m_value; return *this; }
    wxUniChar& operator=(const wxUniCharRef& c);
    wxUniChar& operator=(char c) { m_value = From8bit(c); return *this; }
    wxUniChar& operator=(unsigned char c) { m_value = From8bit((char)c); return *this; }

#define wxUNICHAR_DEFINE_OPERATOR_EQUAL(type) \
    wxUniChar& operator=(type c) { m_value = (value_type)c; return *this; }
    wxDO_FOR_INT_TYPES(wxUNICHAR_DEFINE_OPERATOR_EQUAL)
#undef wxUNICHAR_DEFINE_OPERATOR_EQUAL

    // Comparison operators:
#define wxDEFINE_UNICHAR_CMP_WITH_INT(T, op) \
    bool operator op(T c) const { return m_value op (value_type)c; }

    // define the given comparison operator for all the types
#define wxDEFINE_UNICHAR_OPERATOR(op)                                         \
    bool operator op(const wxUniChar& c) const { return m_value op c.m_value; }\
    bool operator op(char c) const { return m_value op From8bit(c); }         \
    bool operator op(unsigned char c) const { return m_value op From8bit((char)c); } \
    wxDO_FOR_INT_TYPES_1(wxDEFINE_UNICHAR_CMP_WITH_INT, op)

    wxFOR_ALL_COMPARISONS(wxDEFINE_UNICHAR_OPERATOR)

#undef wxDEFINE_UNICHAR_OPERATOR
#undef wxDEFINE_UNCHAR_CMP_WITH_INT

    // this is needed for expressions like 'Z'-c
    int operator-(const wxUniChar& c) const { return m_value - c.m_value; }
    int operator-(char c) const { return m_value - From8bit(c); }
    int operator-(unsigned char c) const { return m_value - From8bit((char)c); }
    int operator-(wchar_t c) const { return m_value - (value_type)c; }


private:
    // notice that we implement these functions inline for 7-bit ASCII
    // characters purely for performance reasons
    static value_type From8bit(char c)
    {
#if wxUSE_UNICODE
        if ( (unsigned char)c < 0x80 )
            return c;

        return FromHi8bit(c);
#else
        return c;
#endif
    }

    static char To8bit(value_type c)
    {
#if wxUSE_UNICODE
        if ( c < 0x80 )
            return wx_truncate_cast(char, c);

        return ToHi8bit(c);
#else
        return wx_truncate_cast(char, c);
#endif
    }

    // helpers of the functions above called to deal with non-ASCII chars
    static value_type FromHi8bit(char c);
    static char ToHi8bit(value_type v);
    static bool GetAsHi8bit(value_type v, char *c);

private:
    value_type m_value;
};


// Writeable reference to a character in wxString.
//
// This class can be used in the same way wxChar is used, except that changing
// its value updates the underlying string object.
class WXDLLIMPEXP_BASE wxUniCharRef
{
private:
    typedef wxStringImpl::iterator iterator;

    // create the reference
#if wxUSE_UNICODE_UTF8
    wxUniCharRef(wxString& str, iterator pos) : m_str(str), m_pos(pos) {}
#else
    wxUniCharRef(iterator pos) : m_pos(pos) {}
#endif

public:
    // NB: we have to make this public, because we don't have wxString
    //     declaration available here and so can't declare wxString::iterator
    //     as friend; so at least don't use a ctor but a static function
    //     that must be used explicitly (this is more than using 'explicit'
    //     keyword on ctor!):
#if wxUSE_UNICODE_UTF8
    static wxUniCharRef CreateForString(wxString& str, iterator pos)
        { return wxUniCharRef(str, pos); }
#else
    static wxUniCharRef CreateForString(iterator pos)
        { return wxUniCharRef(pos); }
#endif

    wxUniChar::value_type GetValue() const { return UniChar().GetValue(); }

#if wxUSE_UNICODE_UTF8
    wxUniChar::Utf8CharBuffer AsUTF8() const { return UniChar().AsUTF8(); }
#endif // wxUSE_UNICODE_UTF8

    bool IsAscii() const { return UniChar().IsAscii(); }
    bool GetAsChar(char *c) const { return UniChar().GetAsChar(c); }

    // Assignment operators:
#if wxUSE_UNICODE_UTF8
    wxUniCharRef& operator=(const wxUniChar& c);
#else
    wxUniCharRef& operator=(const wxUniChar& c) { *m_pos = c; return *this; }
#endif

    wxUniCharRef& operator=(const wxUniCharRef& c)
        { if (&c != this) *this = c.UniChar(); return *this; }

#define wxUNICHAR_REF_DEFINE_OPERATOR_EQUAL(type) \
    wxUniCharRef& operator=(type c) { return *this = wxUniChar(c); }
    wxDO_FOR_CHAR_INT_TYPES(wxUNICHAR_REF_DEFINE_OPERATOR_EQUAL)
#undef wxUNICHAR_REF_DEFINE_OPERATOR_EQUAL

    // Conversions to the same types as wxUniChar is convertible too:
#define wxUNICHAR_REF_DEFINE_OPERATOR_PAREN(type) \
    operator type() const { return UniChar(); }
    wxDO_FOR_CHAR_INT_TYPES(wxUNICHAR_REF_DEFINE_OPERATOR_PAREN)
#undef wxUNICHAR_REF_DEFINE_OPERATOR_PAREN

    // see wxUniChar::operator bool etc. for explanation
    operator bool() const { return (bool)UniChar(); }
    bool operator!() const { return !UniChar(); }
    bool operator&&(bool v) const { return UniChar() && v; }

#define wxDEFINE_UNICHARREF_CMP_WITH_INT(T, op) \
    bool operator op(T c) const { return UniChar() op c; }

    // Comparison operators:
#define wxDEFINE_UNICHARREF_OPERATOR(op)                                      \
    bool operator op(const wxUniCharRef& c) const { return UniChar() op c.UniChar(); }\
    bool operator op(const wxUniChar& c) const { return UniChar() op c; }     \
    wxDO_FOR_CHAR_INT_TYPES_1(wxDEFINE_UNICHARREF_CMP_WITH_INT, op)

    wxFOR_ALL_COMPARISONS(wxDEFINE_UNICHARREF_OPERATOR)

#undef wxDEFINE_UNICHARREF_OPERATOR
#undef wxDEFINE_UNICHARREF_CMP_WITH_INT

    // for expressions like c-'A':
    int operator-(const wxUniCharRef& c) const { return UniChar() - c.UniChar(); }
    int operator-(const wxUniChar& c) const { return UniChar() - c; }
    int operator-(char c) const { return UniChar() - c; }
    int operator-(unsigned char c) const { return UniChar() - c; }
    int operator-(wchar_t c) const { return UniChar() - c; }

private:
#if wxUSE_UNICODE_UTF8
    wxUniChar UniChar() const;
#else
    wxUniChar UniChar() const { return *m_pos; }
#endif

    friend class WXDLLIMPEXP_FWD_BASE wxUniChar;

private:
    // reference to the string and pointer to the character in string
#if wxUSE_UNICODE_UTF8
    wxString& m_str;
#endif
    iterator m_pos;
};

inline wxUniChar::wxUniChar(const wxUniCharRef& c)
{
    m_value = c.UniChar().m_value;
}

inline wxUniChar& wxUniChar::operator=(const wxUniCharRef& c)
{
    m_value = c.UniChar().m_value;
    return *this;
}

// wxUniCharRef doesn't behave quite like a reference, notably because template
// deduction from wxUniCharRef doesn't yield wxUniChar as would have been the
// case if it were a real reference. This results in a number of problems and
// we can't fix all of them but we can at least provide a working swap() for
// it, instead of the default version which doesn't work because a "wrong" type
// is deduced.
namespace std
{

template <>
inline
void swap<wxUniCharRef>(wxUniCharRef& lhs, wxUniCharRef& rhs)
{
    if ( &lhs != &rhs )
    {
        // The use of wxUniChar here is the crucial difference: in the default
        // implementation, tmp would be wxUniCharRef and so assigning to lhs
        // would modify it too. Here we make a real copy, not affected by
        // changing lhs, instead.
        wxUniChar tmp = lhs;
        lhs = rhs;
        rhs = tmp;
    }
}

} // namespace std


// Comparison operators for the case when wxUniChar(Ref) is the second operand
// implemented in terms of member comparison functions

wxDEFINE_COMPARISONS_BY_REV(char, const wxUniChar&)
wxDEFINE_COMPARISONS_BY_REV(char, const wxUniCharRef&)

wxDEFINE_COMPARISONS_BY_REV(wchar_t, const wxUniChar&)
wxDEFINE_COMPARISONS_BY_REV(wchar_t, const wxUniCharRef&)

wxDEFINE_COMPARISONS_BY_REV(const wxUniChar&, const wxUniCharRef&)

// for expressions like c-'A':
inline int operator-(char c1, const wxUniCharRef& c2) { return -(c2 - c1); }
inline int operator-(const wxUniChar& c1, const wxUniCharRef& c2) { return -(c2 - c1); }
inline int operator-(wchar_t c1, const wxUniCharRef& c2) { return -(c2 - c1); }

#endif /* _WX_UNICHAR_H_ */
