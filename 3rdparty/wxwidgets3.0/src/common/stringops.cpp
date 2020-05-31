/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/stringops.cpp
// Purpose:     implementation of wxString primitive operations
// Author:      Vaclav Slavik
// Modified by:
// Created:     2007-04-16
// Copyright:   (c) 2007 REA Elektronik GmbH
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ===========================================================================
// headers
// ===========================================================================

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/stringops.h"
#endif

// ===========================================================================
// implementation
// ===========================================================================

#if wxUSE_UNICODE_UTF8

// ---------------------------------------------------------------------------
// UTF-8 sequences lengths
// ---------------------------------------------------------------------------

const unsigned char wxStringOperationsUtf8::ms_utf8IterTable[256] = {
    // single-byte sequences (ASCII):
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 00..0F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 10..1F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 20..2F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 30..3F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 40..4F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 50..5F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 60..6F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 70..7F

    // these are invalid, we use step 1 to skip
    // over them (should never happen):
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 80..8F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 90..9F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // A0..AF
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // B0..BF
    1, 1,                                            // C0,C1

    // two-byte sequences:
          2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  // C2..CF
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  // D0..DF

    // three-byte sequences:
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,  // E0..EF

    // four-byte sequences:
    4, 4, 4, 4, 4,                                   // F0..F4

    // these are invalid again (5- or 6-byte
    // sequences and sequences for code points
    // above U+10FFFF, as restricted by RFC 3629):
                   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1   // F5..FF
};

// ---------------------------------------------------------------------------
// UTF-8 operations
// ---------------------------------------------------------------------------

//
// Table 3.1B from Unicode spec: Legal UTF-8 Byte Sequences
//
//     Code Points    | 1st Byte | 2nd Byte | 3rd Byte | 4th Byte |
// -------------------+----------+----------+----------+----------+
//   U+0000..U+007F   |  00..7F  |          |          |          |
//   U+0080..U+07FF   |  C2..DF  |  80..BF  |          |          |
//   U+0800..U+0FFF   |  E0      |  A0..BF  |  80..BF  |          |
//   U+1000..U+FFFF   |  E1..EF  |  80..BF  |  80..BF  |          |
//  U+10000..U+3FFFF  |  F0      |  90..BF  |  80..BF  |  80..BF  |
//  U+40000..U+FFFFF  |  F1..F3  |  80..BF  |  80..BF  |  80..BF  |
// U+100000..U+10FFFF |  F4      |  80..8F  |  80..BF  |  80..BF  |
// -------------------+----------+----------+----------+----------+

bool wxStringOperationsUtf8::IsValidUtf8String(const char *str, size_t len)
{
    if ( !str )
        return true; // empty string is UTF8 string

    const unsigned char *c = (const unsigned char*)str;
    const unsigned char * const end = (len == wxStringImpl::npos) ? NULL : c + len;

    for ( ; c != end && *c; ++c )
    {
        unsigned char b = *c;

        if ( end != NULL )
        {
            // if the string is not NULL-terminated, verify we have enough
            // bytes in it left for current character's encoding:
            if ( c + ms_utf8IterTable[*c] > end )
                return false;
        }

        if ( b <= 0x7F ) // 00..7F
            continue;

        else if ( b < 0xC2 ) // invalid lead bytes: 80..C1
            return false;

        // two-byte sequences:
        else if ( b <= 0xDF ) // C2..DF
        {
            b = *(++c);
            if ( !(b >= 0x80 && b <= 0xBF ) )
                return false;
        }

        // three-byte sequences:
        else if ( b == 0xE0 )
        {
            b = *(++c);
            if ( !(b >= 0xA0 && b <= 0xBF ) )
                return false;
            b = *(++c);
            if ( !(b >= 0x80 && b <= 0xBF ) )
                return false;
        }
        else if ( b == 0xED )
        {
            b = *(++c);
            if ( !(b >= 0x80 && b <= 0x9F ) )
                return false;
            b = *(++c);
            if ( !(b >= 0x80 && b <= 0xBF ) )
                return false;
        }
        else if ( b <= 0xEF ) // E1..EC EE..EF
        {
            for ( int i = 0; i < 2; ++i )
            {
                b = *(++c);
                if ( !(b >= 0x80 && b <= 0xBF ) )
                    return false;
            }
        }

        // four-byte sequences:
        else if ( b == 0xF0 )
        {
            b = *(++c);
            if ( !(b >= 0x90 && b <= 0xBF ) )
                return false;
            for ( int i = 0; i < 2; ++i )
            {
                b = *(++c);
                if ( !(b >= 0x80 && b <= 0xBF ) )
                    return false;
            }
        }
        else if ( b <= 0xF3 ) // F1..F3
        {
            for ( int i = 0; i < 3; ++i )
            {
                b = *(++c);
                if ( !(b >= 0x80 && b <= 0xBF ) )
                    return false;
            }
        }
        else if ( b == 0xF4 )
        {
            b = *(++c);
            if ( !(b >= 0x80 && b <= 0x8F ) )
                return false;
            for ( int i = 0; i < 2; ++i )
            {
                b = *(++c);
                if ( !(b >= 0x80 && b <= 0xBF ) )
                    return false;
            }
        }
        else // otherwise, it's invalid lead byte
            return false;
    }

    return true;
}

// NB: this is in this file and not unichar.cpp to keep all UTF-8 encoding
//     code in single place
wxUniChar::Utf8CharBuffer wxUniChar::AsUTF8() const
{
    Utf8CharBuffer buf = { "" }; // init to avoid g++ 4.1 warning with -O2
    char *out = buf.data;

    value_type code = GetValue();

    //    Char. number range   |        UTF-8 octet sequence
    //       (hexadecimal)     |              (binary)
    //   ----------------------+---------------------------------------------
    //   0000 0000 - 0000 007F | 0xxxxxxx
    //   0000 0080 - 0000 07FF | 110xxxxx 10xxxxxx
    //   0000 0800 - 0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
    //   0001 0000 - 0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    //
    //   Code point value is stored in bits marked with 'x', lowest-order bit
    //   of the value on the right side in the diagram above.
    //                                                        (from RFC 3629)

    if ( code <= 0x7F )
    {
        out[1] = 0;
        out[0] = (char)code;
    }
    else if ( code <= 0x07FF )
    {
        out[2] = 0;
        // NB: this line takes 6 least significant bits, encodes them as
        // 10xxxxxx and discards them so that the next byte can be encoded:
        out[1] = 0x80 | (code & 0x3F);  code >>= 6;
        out[0] = 0xC0 | code;
    }
    else if ( code < 0xFFFF )
    {
        out[3] = 0;
        out[2] = 0x80 | (code & 0x3F);  code >>= 6;
        out[1] = 0x80 | (code & 0x3F);  code >>= 6;
        out[0] = 0xE0 | code;
    }
    else if ( code <= 0x10FFFF )
    {
        out[4] = 0;
        out[3] = 0x80 | (code & 0x3F);  code >>= 6;
        out[2] = 0x80 | (code & 0x3F);  code >>= 6;
        out[1] = 0x80 | (code & 0x3F);  code >>= 6;
        out[0] = 0xF0 | code;
    }
    else
    {
        wxFAIL_MSG( wxT("trying to encode undefined Unicode character") );
        out[0] = 0;
    }

    return buf;
}

wxUniChar
wxStringOperationsUtf8::DecodeNonAsciiChar(wxStringImpl::const_iterator i)
{
    wxASSERT( IsValidUtf8LeadByte(*i) );

    size_t len = GetUtf8CharLength(*i);
    wxASSERT_MSG( len <= 4, wxT("invalid UTF-8 sequence length") );

    //    Char. number range   |        UTF-8 octet sequence
    //       (hexadecimal)     |              (binary)
    //   ----------------------+---------------------------------------------
    //   0000 0000 - 0000 007F | 0xxxxxxx
    //   0000 0080 - 0000 07FF | 110xxxxx 10xxxxxx
    //   0000 0800 - 0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
    //   0001 0000 - 0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    //
    //   Code point value is stored in bits marked with 'x', lowest-order bit
    //   of the value on the right side in the diagram above.
    //                                                        (from RFC 3629)

    // mask to extract lead byte's value ('x' bits above), by sequence's length:
    static const unsigned char s_leadValueMask[4] =  { 0x7F, 0x1F, 0x0F, 0x07 };
#if wxDEBUG_LEVEL
    // mask and value of lead byte's most significant bits, by length:
    static const unsigned char s_leadMarkerMask[4] = { 0x80, 0xE0, 0xF0, 0xF8 };
    static const unsigned char s_leadMarkerVal[4] =  { 0x00, 0xC0, 0xE0, 0xF0 };
#endif

    // extract the lead byte's value bits:
    wxASSERT_MSG( ((unsigned char)*i & s_leadMarkerMask[len-1]) ==
                  s_leadMarkerVal[len-1],
                  wxT("invalid UTF-8 lead byte") );
    wxUniChar::value_type code = (unsigned char)*i & s_leadValueMask[len-1];

    // all remaining bytes, if any, are handled in the same way regardless of
    // sequence's length:
    for ( ++i ; len > 1; --len, ++i )
    {
        wxASSERT_MSG( ((unsigned char)*i & 0xC0) == 0x80,
                      wxT("invalid UTF-8 byte") );

        code <<= 6;
        code |= (unsigned char)*i & 0x3F;
    }

    return wxUniChar(code);
}

wxCharBuffer wxStringOperationsUtf8::EncodeNChars(size_t n, const wxUniChar& ch)
{
    Utf8CharBuffer once(EncodeChar(ch));
    // the IncIter() table can be used to determine the length of ch's encoding:
    size_t len = ms_utf8IterTable[(unsigned char)once.data[0]];

    wxCharBuffer buf(n * len);
    char *ptr = buf.data();
    for ( size_t i = 0; i < n; i++, ptr += len )
    {
        memcpy(ptr, once.data, len);
    }

    return buf;
}

#endif // wxUSE_UNICODE_UTF8
