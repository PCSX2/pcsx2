
// Name:        wx/ustring.h
// Purpose:     32-bit string (UCS-4)
// Author:      Robert Roebling
// Copyright:   (c) Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_USTRING_H_
#define _WX_USTRING_H_

#include "wx/defs.h"
#include "wx/string.h"

#include <string>

#if SIZEOF_WCHAR_T == 2
typedef wxWCharBuffer wxU16CharBuffer;
typedef wxScopedWCharBuffer wxScopedU16CharBuffer;
#else
typedef wxCharTypeBuffer<wxChar16> wxU16CharBuffer;
typedef wxScopedCharTypeBuffer<wxChar16> wxScopedU16CharBuffer;
#endif

#if SIZEOF_WCHAR_T == 4
typedef wxWCharBuffer wxU32CharBuffer;
typedef wxScopedWCharBuffer wxScopedU32CharBuffer;
#else
typedef wxCharTypeBuffer<wxChar32> wxU32CharBuffer;
typedef wxScopedCharTypeBuffer<wxChar32> wxScopedU32CharBuffer;
#endif

#ifdef __VISUALC__
    // "non dll-interface class 'std::basic_string<wxChar32>' used as base
    // interface for dll-interface class 'wxString'" -- this is OK in our case
    // (and warning is unavoidable anyhow)
    #pragma warning(push)
    #pragma warning(disable:4275)
#endif

class WXDLLIMPEXP_BASE wxUString: public std::basic_string<wxChar32>
{
public:
    wxUString() { }

    wxUString( const wxChar32 *str )                            { assign(str); }
    wxUString( const wxScopedU32CharBuffer &buf )                     { assign(buf); }

    wxUString( const char *str )                                { assign(str); }
    wxUString( const wxScopedCharBuffer &buf )                        { assign(buf); }
    wxUString( const char *str, const wxMBConv &conv )          { assign(str,conv); }
    wxUString( const wxScopedCharBuffer &buf, const wxMBConv &conv )  { assign(buf,conv); }

    wxUString( const wxChar16 *str )                            { assign(str); }
    wxUString( const wxScopedU16CharBuffer &buf )                     { assign(buf); }

    wxUString( const wxCStrData *cstr )                         { assign(cstr); }
    wxUString( const wxString &str )                            { assign(str); }

    wxUString( char ch )                                        { assign(ch); }
    wxUString( wxChar16 ch )                                    { assign(ch); }
    wxUString( wxChar32 ch )                                    { assign(ch); }
    wxUString( wxUniChar ch )                                   { assign(ch); }
    wxUString( wxUniCharRef ch )                                { assign(ch); }
    wxUString( size_type n, char ch )                           { assign(n,ch); }
    wxUString( size_type n, wxChar16 ch )                       { assign(n,ch); }
    wxUString( size_type n, wxChar32 ch )                       { assign(n,ch); }
    wxUString( size_type n, wxUniChar ch )                      { assign(n,ch); }
    wxUString( size_type n, wxUniCharRef ch )                   { assign(n,ch); }

    // static construction

    static wxUString FromAscii( const char *str, size_type n )
    {
        wxUString ret;
        ret.assignFromAscii( str, n );
        return ret;
    }

    static wxUString FromAscii( const char *str )
    {
        wxUString ret;
        ret.assignFromAscii( str );
        return ret;
    }

    static wxUString FromUTF8( const char *str, size_type n )
    {
        wxUString ret;
        ret.assignFromUTF8( str, n );
        return ret;
    }

    static wxUString FromUTF8( const char *str )
    {
        wxUString ret;
        ret.assignFromUTF8( str );
        return ret;
    }

    static wxUString FromUTF16( const wxChar16 *str, size_type n )
    {
        wxUString ret;
        ret.assignFromUTF16( str, n );
        return ret;
    }

    static wxUString FromUTF16( const wxChar16 *str )
    {
        wxUString ret;
        ret.assignFromUTF16( str );
        return ret;
    }

    // assign from encoding

    wxUString &assignFromAscii( const char *str );
    wxUString &assignFromAscii( const char *str, size_type n );
    wxUString &assignFromUTF8( const char *str );
    wxUString &assignFromUTF8( const char *str, size_type n );
    wxUString &assignFromUTF16( const wxChar16* str );
    wxUString &assignFromUTF16( const wxChar16* str, size_type n );
    wxUString &assignFromCString( const char* str );
    wxUString &assignFromCString( const char* str, const wxMBConv &conv );

    // conversions

    wxScopedCharBuffer utf8_str() const;
    wxScopedU16CharBuffer utf16_str() const;

#if SIZEOF_WCHAR_T == 2
    wxScopedWCharBuffer wc_str() const
    {
       return utf16_str();
    }
#else
    wchar_t *wc_str() const
    {
        return (wchar_t*) c_str();
    }
#endif

    operator wxString() const
    {
#if wxUSE_UNICODE_UTF8
        return wxString::FromUTF8( utf8_str() );
#else
#if SIZEOF_WCHAR_T == 2
        return wxString( utf16_str() );
#else
        return wxString( c_str() );
#endif
#endif
    }

#if wxUSE_UNICODE_UTF8
    wxScopedCharBuffer wx_str()
    {
        return utf8_str();
    }
#else
#if SIZEOF_WCHAR_T == 2
    wxScopedWCharBuffer wx_str()
    {
        return utf16_str();
    }
#else
    const wchar_t* wx_str()
    {
        return c_str();
    }
#endif
#endif

    // assign

    wxUString &assign( const wxChar32* str )
    {
        std::basic_string<wxChar32> *base = this;
        return (wxUString &) base->assign( str );
    }

    wxUString &assign( const wxChar32* str, size_type n )
    {
        std::basic_string<wxChar32> *base = this;
        return (wxUString &) base->assign( str, n );
    }

    wxUString &assign( const wxUString &str )
    {
        std::basic_string<wxChar32> *base = this;
        return (wxUString &) base->assign( str );
    }

    wxUString &assign( const wxUString &str, size_type pos, size_type n )
    {
        std::basic_string<wxChar32> *base = this;
        return (wxUString &) base->assign( str, pos, n );
    }

    // FIXME-VC6:  VC 6.0 stl does not support all types of assign functions
    #ifdef __VISUALC6__
        wxUString &assign( wxChar32 ch )
        {
            wxChar32 chh[1];
            chh[0] = ch;
            std::basic_string<wxChar32> *base = this;
            return (wxUString &)base->assign(chh);
        }

        wxUString &assign( size_type n, wxChar32 ch )
        {
            wxU32CharBuffer buffer(n);
            wxChar32 *p = buffer.data();
            size_type i;
            for (i = 0; i < n; i++)
            {
               *p = ch;
               p++;
            }

            std::basic_string<wxChar32> *base = this;
            return (wxUString &)base->assign(buffer.data());
        }
    #else
        wxUString &assign( wxChar32 ch )
        {
            std::basic_string<wxChar32> *base = this;
            return (wxUString &) base->assign( (size_type) 1, ch );
        }

        wxUString &assign( size_type n, wxChar32 ch )
        {
            std::basic_string<wxChar32> *base = this;
            return (wxUString &) base->assign( n, ch );
        }
    #endif // __VISUALC6__

    wxUString &assign( const wxScopedU32CharBuffer &buf )
    {
        return assign( buf.data() );
    }

    wxUString &assign( const char *str )
    {
        return assignFromCString( str );
    }

    wxUString &assign( const wxScopedCharBuffer &buf )
    {
        return assignFromCString( buf.data() );
    }

    wxUString &assign( const char *str, const wxMBConv &conv )
    {
        return assignFromCString( str, conv );
    }

    wxUString &assign( const wxScopedCharBuffer &buf, const wxMBConv &conv )
    {
        return assignFromCString( buf.data(), conv );
    }

    wxUString &assign( const wxChar16 *str )
    {
        return assignFromUTF16( str );
    }

    wxUString &assign( const wxScopedU16CharBuffer &buf )
    {
        return assignFromUTF16( buf.data() );
    }

    wxUString &assign( const wxCStrData *cstr )
    {
#if SIZEOF_WCHAR_T == 2
        return assignFromUTF16( cstr->AsWChar() );
#else
        return assign( cstr->AsWChar() );
#endif
    }

    wxUString &assign( const wxString &str )
    {
#if wxUSE_UNICODE_UTF8
        return assignFromUTF8( str.wx_str() );
#else
   #if SIZEOF_WCHAR_T == 2
        return assignFromUTF16( str.wc_str() );
   #else
        return assign( str.wc_str() );
   #endif
#endif
    }

    wxUString &assign( char ch )
    {
        char buf[2];
        buf[0] = ch;
        buf[1] = 0;
        return assignFromCString( buf );
    }

    wxUString &assign( size_type n, char ch )
    {
        wxCharBuffer buffer(n);
        char *p = buffer.data();
        size_type i;
        for (i = 0; i < n; i++)
        {
           *p = ch;
           p++;
        }
        return assignFromCString( buffer.data() );
    }

    wxUString &assign( wxChar16 ch )
    {
        wxChar16 buf[2];
        buf[0] = ch;
        buf[1] = 0;
        return assignFromUTF16( buf );
    }

    wxUString &assign( size_type n, wxChar16 ch )
    {
        wxU16CharBuffer buffer(n);
        wxChar16 *p = buffer.data();
        size_type i;
        for (i = 0; i < n; i++)
        {
           *p = ch;
           p++;
        }
        return assignFromUTF16( buffer.data() );
    }

    wxUString &assign( wxUniChar ch )
    {
        return assign( (wxChar32) ch.GetValue() );
    }

    wxUString &assign( size_type n, wxUniChar ch )
    {
        return assign( n, (wxChar32) ch.GetValue() );
    }

    wxUString &assign( wxUniCharRef ch )
    {
        return assign( (wxChar32) ch.GetValue() );
    }

    wxUString &assign( size_type n, wxUniCharRef ch )
    {
        return assign( n, (wxChar32) ch.GetValue() );
    }

    // append [STL overload]

    wxUString &append( const wxUString &s )
    {
        std::basic_string<wxChar32> *base = this;
        return (wxUString &) base->append( s );
    }

    wxUString &append( const wxUString &s, size_type pos, size_type n )
    {
        std::basic_string<wxChar32> *base = this;
        return (wxUString &) base->append( s, pos, n );
    }

    wxUString &append( const wxChar32* s )
    {
        std::basic_string<wxChar32> *base = this;
        return (wxUString &) base->append( s );
    }

    wxUString &append( const wxChar32* s, size_type n )
    {
        std::basic_string<wxChar32> *base = this;
        return (wxUString &) base->append( s, n );
    }

    // FIXME-VC6:  VC 6.0 stl does not support all types of append functions
    #ifdef __VISUALC6__
        wxUString &append( size_type n, wxChar32 c )
        {
            wxU32CharBuffer buffer(n);
            wxChar32 *p = buffer.data();
            size_type i;
            for (i = 0; i < n; i++)
            {
               *p = c;
               p++;
            }

            std::basic_string<wxChar32> *base = this;
            return (wxUString &) base->append(buffer.data());
        }
    #else
        wxUString &append( size_type n, wxChar32 c )
        {
            std::basic_string<wxChar32> *base = this;
            return (wxUString &) base->append( n, c );
        }
    #endif // __VISUALC6__

    wxUString &append( wxChar32 c )
    {
        std::basic_string<wxChar32> *base = this;
        return (wxUString &) base->append( 1, c );
    }

    // append [wx overload]

    wxUString &append( const wxScopedU16CharBuffer &buf )
    {
        return append( buf.data() );
    }

    wxUString &append( const wxScopedU32CharBuffer &buf )
    {
        return append( buf.data() );
    }

    wxUString &append( const char *str )
    {
        return append( wxUString( str ) );
    }

    wxUString &append( const wxScopedCharBuffer &buf )
    {
        return append( wxUString( buf ) );
    }

    wxUString &append( const wxChar16 *str )
    {
        return append( wxUString( str ) );
    }

    wxUString &append( const wxString &str )
    {
        return append( wxUString( str ) );
    }

    wxUString &append( const wxCStrData *cstr )
    {
        return append( wxUString( cstr ) );
    }

    wxUString &append( char ch )
    {
        char buf[2];
        buf[0] = ch;
        buf[1] = 0;
        return append( buf );
    }

    wxUString &append( wxChar16 ch )
    {
        wxChar16 buf[2];
        buf[0] = ch;
        buf[1] = 0;
        return append( buf );
    }

    wxUString &append( wxUniChar ch )
    {
        return append( (size_type) 1, (wxChar32) ch.GetValue() );
    }

    wxUString &append( wxUniCharRef ch )
    {
        return append( (size_type) 1, (wxChar32) ch.GetValue() );
    }


    // insert [STL overloads]

    wxUString &insert( size_type pos, const wxUString &s )
    {
        std::basic_string<wxChar32> *base = this;
        return (wxUString &) base->insert( pos, s );
    }

    wxUString &insert( size_type pos, const wxUString &s, size_type pos1, size_type n )
    {
        std::basic_string<wxChar32> *base = this;
        return (wxUString &) base->insert( pos, s, pos1, n );
    }

    wxUString &insert( size_type pos, const wxChar32 *s )
    {
        std::basic_string<wxChar32> *base = this;
        return (wxUString &) base->insert( pos, s );
    }

    wxUString &insert( size_type pos, const wxChar32 *s, size_type n )
    {
        std::basic_string<wxChar32> *base = this;
        return (wxUString &) base->insert( pos, s, n );
    }

    wxUString &insert( size_type pos, size_type n, wxChar32 c )
    {
        std::basic_string<wxChar32> *base = this;
        return (wxUString &) base->insert( pos, n, c );
    }


    // insert [STL overloads]

    wxUString &insert( size_type n, const char *s )
    {
        return insert( n, wxUString( s ) );
    }

    wxUString &insert( size_type n, const wxChar16 *s )
    {
        return insert( n, wxUString( s ) );
    }

    wxUString &insert( size_type n, const wxScopedCharBuffer &buf )
    {
        return insert( n, wxUString( buf ) );
    }

    wxUString &insert( size_type n, const wxScopedU16CharBuffer &buf )
    {
        return insert( n, wxUString( buf ) );
    }

    wxUString &insert( size_type n, const wxScopedU32CharBuffer &buf )
    {
        return insert( n, buf.data() );
    }

    wxUString &insert( size_type n, const wxString &s )
    {
        return insert( n, wxUString( s ) );
    }

    wxUString &insert( size_type n, const wxCStrData *cstr )
    {
        return insert( n, wxUString( cstr ) );
    }

    wxUString &insert( size_type n, char ch )
    {
        char buf[2];
        buf[0] = ch;
        buf[1] = 0;
        return insert( n, buf );
    }

    wxUString &insert( size_type n, wchar_t ch )
    {
        wchar_t buf[2];
        buf[0] = ch;
        buf[1] = 0;
        return insert( n, buf );
    }

    // insert iterator

    iterator insert( iterator it, wxChar32 ch )
    {
        std::basic_string<wxChar32> *base = this;
        return base->insert( it, ch );
    }

    void insert(iterator it, const_iterator first, const_iterator last)
    {
        std::basic_string<wxChar32> *base = this;
        base->insert( it, first, last );
    }


    // operator =
    wxUString& operator=(const wxString& s)
        { return assign( s ); }
    wxUString& operator=(const wxCStrData* s)
        { return assign( s ); }
    wxUString& operator=(const char *s)
        { return assign( s ); }
    wxUString& operator=(const wxChar16 *s)
        { return assign( s ); }
    wxUString& operator=(const wxChar32 *s)
        { return assign( s ); }
    wxUString& operator=(const wxScopedCharBuffer &s)
        { return assign( s ); }
    wxUString& operator=(const wxScopedU16CharBuffer &s)
        { return assign( s ); }
    wxUString& operator=(const wxScopedU32CharBuffer &s)
        { return assign( s ); }
    wxUString& operator=(const char ch)
        { return assign( ch ); }
    wxUString& operator=(const wxChar16 ch)
        { return assign( ch ); }
    wxUString& operator=(const wxChar32 ch)
        { return assign( ch ); }
    wxUString& operator=(const wxUniChar ch)
        { return assign( ch ); }
    wxUString& operator=(const wxUniCharRef ch)
        { return assign( ch ); }

    // operator +=
    wxUString& operator+=(const wxUString& s)
        { return append( s ); }
    wxUString& operator+=(const wxString& s)
        { return append( s ); }
    wxUString& operator+=(const wxCStrData* s)
        { return append( s ); }
    wxUString& operator+=(const char *s)
        { return append( s ); }
    wxUString& operator+=(const wxChar16 *s)
        { return append( s ); }
    wxUString& operator+=(const wxChar32 *s)
        { return append( s ); }
    wxUString& operator+=(const wxScopedCharBuffer &s)
        { return append( s ); }
    wxUString& operator+=(const wxScopedU16CharBuffer &s)
        { return append( s ); }
    wxUString& operator+=(const wxScopedU32CharBuffer &s)
        { return append( s ); }
    wxUString& operator+=(const char ch)
        { return append( ch ); }
    wxUString& operator+=(const wxChar16 ch)
        { return append( ch ); }
    wxUString& operator+=(const wxChar32 ch)
        { return append( ch ); }
    wxUString& operator+=(const wxUniChar ch)
        { return append( ch ); }
    wxUString& operator+=(const wxUniCharRef ch)
        { return append( ch ); }

};

#ifdef __VISUALC__
    #pragma warning(pop)
#endif

inline wxUString operator+(const wxUString &s1, const wxUString &s2)
    { wxUString ret( s1 ); ret.append( s2 ); return ret; }
inline wxUString operator+(const wxUString &s1, const char *s2)
    { return s1 + wxUString(s2); }
inline wxUString operator+(const wxUString &s1, const wxString &s2)
    { return s1 + wxUString(s2); }
inline wxUString operator+(const wxUString &s1, const wxCStrData *s2)
    { return s1 + wxUString(s2); }
inline wxUString operator+(const wxUString &s1, const wxChar16* s2)
    { return s1 + wxUString(s2); }
inline wxUString operator+(const wxUString &s1, const wxChar32 *s2)
    { return s1 + wxUString(s2); }
inline wxUString operator+(const wxUString &s1, const wxScopedCharBuffer &s2)
    { return s1 + wxUString(s2); }
inline wxUString operator+(const wxUString &s1, const wxScopedU16CharBuffer &s2)
    { return s1 + wxUString(s2); }
inline wxUString operator+(const wxUString &s1, const wxScopedU32CharBuffer &s2)
    { return s1 + wxUString(s2); }
inline wxUString operator+(const wxUString &s1, char s2)
    { return s1 + wxUString(s2); }
inline wxUString operator+(const wxUString &s1, wxChar32 s2)
    { wxUString ret( s1 ); ret.append( s2 ); return ret; }
inline wxUString operator+(const wxUString &s1, wxChar16 s2)
    { wxUString ret( s1 ); ret.append( (wxChar32) s2 ); return ret; }
inline wxUString operator+(const wxUString &s1, wxUniChar s2)
    { wxUString ret( s1 ); ret.append( (wxChar32) s2.GetValue() ); return ret; }
inline wxUString operator+(const wxUString &s1, wxUniCharRef s2)
    { wxUString ret( s1 ); ret.append( (wxChar32) s2.GetValue() ); return ret; }

inline wxUString operator+(const char *s1, const wxUString &s2)
    { return wxUString(s1) + s2; }
inline wxUString operator+(const wxString &s1, const wxUString &s2)
    { return wxUString(s1) + s2; }
inline wxUString operator+(const wxCStrData *s1, const wxUString &s2)
    { return wxUString(s1) + s2; }
inline wxUString operator+(const wxChar16* s1, const wxUString &s2)
    { return wxUString(s1) + s2; }
inline wxUString operator+(const wxChar32 *s1, const wxUString &s2)
    { return wxUString(s1) + s2; }
inline wxUString operator+(const wxScopedCharBuffer &s1, const wxUString &s2)
    { return wxUString(s1) + s2; }
inline wxUString operator+(const wxScopedU16CharBuffer &s1, const wxUString &s2)
    { return wxUString(s1) + s2; }
inline wxUString operator+(const wxScopedU32CharBuffer &s1, const wxUString &s2)
    { return wxUString(s1) + s2; }
inline wxUString operator+(char s1, const wxUString &s2)
    { return wxUString(s1) + s2; }
inline wxUString operator+(wxChar32 s1, const wxUString &s2 )
    { return wxUString(s1) + s2; }
inline wxUString operator+(wxChar16 s1, const wxUString &s2)
    { return wxUString(s1) + s2; }
inline wxUString operator+(wxUniChar s1, const wxUString &s2)
    { return wxUString(s1) + s2; }
inline wxUString operator+(wxUniCharRef s1, const wxUString &s2)
    { return wxUString(s1) + s2; }


inline bool operator==(const wxUString& s1, const wxUString& s2)
    { return s1.compare( s2 ) == 0; }
inline bool operator!=(const wxUString& s1, const wxUString& s2)
    { return s1.compare( s2 ) != 0; }
inline bool operator< (const wxUString& s1, const wxUString& s2)
    { return s1.compare( s2 ) < 0; }
inline bool operator> (const wxUString& s1, const wxUString& s2)
    { return s1.compare( s2 ) > 0; }
inline bool operator<=(const wxUString& s1, const wxUString& s2)
    { return s1.compare( s2 ) <= 0; }
inline bool operator>=(const wxUString& s1, const wxUString& s2)
    { return s1.compare( s2 ) >= 0; }

#define wxUSTRING_COMP_OPERATORS( T )                       \
inline bool operator==(const wxUString& s1, T s2)           \
    { return s1.compare( wxUString(s2) ) == 0; }            \
inline bool operator!=(const wxUString& s1, T s2)           \
    { return s1.compare( wxUString(s2) ) != 0; }            \
inline bool operator< (const wxUString& s1, T s2)           \
    { return s1.compare( wxUString(s2) ) < 0; }             \
inline bool operator> (const wxUString& s1, T s2)           \
    { return s1.compare( wxUString(s2) ) > 0; }             \
inline bool operator<=(const wxUString& s1, T s2)           \
    { return s1.compare( wxUString(s2) ) <= 0; }            \
inline bool operator>=(const wxUString& s1, T s2)           \
    { return s1.compare( wxUString(s2) ) >= 0; }            \
\
inline bool operator==(T s2, const wxUString& s1)           \
    { return s1.compare( wxUString(s2) ) == 0; }            \
inline bool operator!=(T s2, const wxUString& s1)           \
    { return s1.compare( wxUString(s2) ) != 0; }            \
inline bool operator< (T s2, const wxUString& s1)           \
    { return s1.compare( wxUString(s2) ) > 0; }             \
inline bool operator> (T s2, const wxUString& s1)           \
    { return s1.compare( wxUString(s2) ) < 0; }             \
inline bool operator<=(T s2, const wxUString& s1)           \
    { return s1.compare( wxUString(s2) ) >= 0; }            \
inline bool operator>=(T s2, const wxUString& s1)           \
    { return s1.compare( wxUString(s2) ) <= 0; }

wxUSTRING_COMP_OPERATORS( const wxString & )
wxUSTRING_COMP_OPERATORS( const char * )
wxUSTRING_COMP_OPERATORS( const wxChar16 * )
wxUSTRING_COMP_OPERATORS( const wxChar32 * )
wxUSTRING_COMP_OPERATORS( const wxScopedCharBuffer & )
wxUSTRING_COMP_OPERATORS( const wxScopedU16CharBuffer & )
wxUSTRING_COMP_OPERATORS( const wxScopedU32CharBuffer & )
wxUSTRING_COMP_OPERATORS( const wxCStrData * )

#endif // _WX_USTRING_H_
