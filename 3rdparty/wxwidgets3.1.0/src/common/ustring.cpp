/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/ustring.cpp
// Purpose:     wxUString class
// Author:      Robert Roebling
// Created:     2008-07-25
// Copyright:   (c) 2008 Robert Roebling
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#include "wx/ustring.h"

#ifndef WX_PRECOMP
    #include "wx/crt.h"
    #include "wx/log.h"
#endif

wxUString &wxUString::assignFromAscii( const char *str )
{
   size_type len = wxStrlen( str );

   wxU32CharBuffer buffer( len );
   wxChar32 *ptr = buffer.data();

   size_type i;
   for (i = 0; i < len; i++)
   {
       *ptr = *str;
       ptr++;
       str++;
   }

   return assign( buffer );
}

wxUString &wxUString::assignFromAscii( const char *str, size_type n )
{
   size_type len = 0;
   const char *s = str;
   while (len < n && *s)
   {
       len++;
       s++;
   }

   wxU32CharBuffer buffer( len );
   wxChar32 *ptr = buffer.data();

   size_type i;
   for (i = 0; i < len; i++)
   {
       *ptr = *str;
       ptr++;
       str++;
   }

   return *this;
}

// ----------------------------------------------------------------------------
// UTF-8
// ----------------------------------------------------------------------------

// this table gives the length of the UTF-8 encoding from its first character:
const unsigned char tableUtf8Lengths[256] = {
    // single-byte sequences (ASCII):
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 00..0F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 10..1F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 20..2F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 30..3F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 40..4F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 50..5F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 60..6F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 70..7F

    // these are invalid:
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 80..8F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 90..9F
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // A0..AF
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // B0..BF
    0, 0,                                            // C0,C1

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
                   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0   // F5..FF
};

wxUString &wxUString::assignFromUTF8( const char *str )
{
    if (!str)
        return assign( wxUString() );

    size_type ucs4_len = 0;
    const char *p = str;
    while (*p)
    {
        unsigned char c = *p;
        size_type len = tableUtf8Lengths[c];
        if (!len)
           return assign( wxUString() );  // don't try to convert invalid UTF-8
        ucs4_len++;
        p += len;
    }

    wxU32CharBuffer buffer( ucs4_len );
    wxChar32 *out = buffer.data();

    p = str;
    while (*p)
    {
        unsigned char c = *p;
        if (c < 0x80)
        {
            *out = c;
            p++;
        }
        else
        {
            size_type len = tableUtf8Lengths[c];  // len == 0 is caught above

            //   Char. number range   |        UTF-8 octet sequence
            //      (hexadecimal)     |              (binary)
            //  ----------------------+----------------------------------------
            //  0000 0000 - 0000 007F | 0xxxxxxx
            //  0000 0080 - 0000 07FF | 110xxxxx 10xxxxxx
            //  0000 0800 - 0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
            //  0001 0000 - 0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            //
            //  Code point value is stored in bits marked with 'x',
            //  lowest-order bit of the value on the right side in the diagram
            //  above.                                         (from RFC 3629)

            // mask to extract lead byte's value ('x' bits above), by sequence
            // length:
            static const unsigned char leadValueMask[] = { 0x7F, 0x1F, 0x0F, 0x07 };

            // mask and value of lead byte's most significant bits, by length:
            static const unsigned char leadMarkerMask[] = { 0x80, 0xE0, 0xF0, 0xF8 };
            static const unsigned char leadMarkerVal[] = { 0x00, 0xC0, 0xE0, 0xF0 };

            len--; // it's more convenient to work with 0-based length here

            // extract the lead byte's value bits:
            if ( (c & leadMarkerMask[len]) != leadMarkerVal[len] )
                break;

            wxChar32 code = c & leadValueMask[len];

            // all remaining bytes, if any, are handled in the same way
            // regardless of sequence's length:
            for ( ; len; --len )
            {
                c = *++p;
                if ( (c & 0xC0) != 0x80 )
                    return assign( wxUString() );  // don't try to convert invalid UTF-8

                code <<= 6;
                code |= c & 0x3F;
            }

            *out = code;
            p++;
        }
        out++;
    }

    return assign( buffer.data() );
}

wxUString &wxUString::assignFromUTF8( const char *str, size_type n )
{
    if (!str)
        return assign( wxUString() );

    size_type ucs4_len = 0;
    size_type utf8_pos = 0;
    const char *p = str;
    while (*p)
    {
        unsigned char c = *p;
        size_type len = tableUtf8Lengths[c];
        if (!len)
           return assign( wxUString() );  // don't try to convert invalid UTF-8
        if (utf8_pos + len > n)
            break;
        utf8_pos += len;
        ucs4_len ++;
        p += len;
    }

    wxU32CharBuffer buffer( ucs4_len );
    wxChar32 *out = buffer.data();

    utf8_pos = 0;
    p = str;
    while (*p)
    {
        unsigned char c = *p;
        if (c < 0x80)
        {
            if (utf8_pos + 1 > n)
                break;
            utf8_pos++;

            *out = c;
            p++;
        }
        else
        {
            size_type len = tableUtf8Lengths[c];  // len == 0 is caught above
            if (utf8_pos + len > n)
                break;
            utf8_pos += len;

            //   Char. number range   |        UTF-8 octet sequence
            //      (hexadecimal)     |              (binary)
            //  ----------------------+----------------------------------------
            //  0000 0000 - 0000 007F | 0xxxxxxx
            //  0000 0080 - 0000 07FF | 110xxxxx 10xxxxxx
            //  0000 0800 - 0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
            //  0001 0000 - 0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            //
            //  Code point value is stored in bits marked with 'x',
            //  lowest-order bit of the value on the right side in the diagram
            //  above.                                         (from RFC 3629)

            // mask to extract lead byte's value ('x' bits above), by sequence
            // length:
            static const unsigned char leadValueMask[] = { 0x7F, 0x1F, 0x0F, 0x07 };

            // mask and value of lead byte's most significant bits, by length:
            static const unsigned char leadMarkerMask[] = { 0x80, 0xE0, 0xF0, 0xF8 };
            static const unsigned char leadMarkerVal[] = { 0x00, 0xC0, 0xE0, 0xF0 };

            len--; // it's more convenient to work with 0-based length here

            // extract the lead byte's value bits:
            if ( (c & leadMarkerMask[len]) != leadMarkerVal[len] )
                break;

            wxChar32 code = c & leadValueMask[len];

            // all remaining bytes, if any, are handled in the same way
            // regardless of sequence's length:
            for ( ; len; --len )
            {
                c = *++p;
                if ( (c & 0xC0) != 0x80 )
                    return assign( wxUString() );  // don't try to convert invalid UTF-8

                code <<= 6;
                code |= c & 0x3F;
            }

            *out = code;
            p++;
        }
        out++;
    }

    *out = 0;

    return assign( buffer.data() );
}

wxUString &wxUString::assignFromUTF16( const wxChar16* str, size_type n )
{
    if (!str)
        return assign( wxUString() );

    size_type ucs4_len = 0;
    size_type utf16_pos = 0;
    const wxChar16 *p = str;
    while (*p)
    {
        size_type len;
        if ((*p < 0xd800) || (*p > 0xdfff))
        {
            len = 1;
        }
        else if ((p[1] < 0xdc00) || (p[1] > 0xdfff))
        {
            return assign( wxUString() );  // don't try to convert invalid UTF-16
        }
        else
        {
           len = 2;
        }

        if (utf16_pos + len > n)
            break;

        ucs4_len++;
        p += len;
        utf16_pos += len;
    }

    wxU32CharBuffer buffer( ucs4_len );
    wxChar32 *out = buffer.data();

    utf16_pos = 0;

    p = str;
    while (*p)
    {
        if ((*p < 0xd800) || (*p > 0xdfff))
        {
            if (utf16_pos + 1 > n)
                break;

            *out = *p;
            p++;
            utf16_pos++;
        }
        else
        {
            if (utf16_pos + 2 > n)
                break;

           *out = ((p[0] - 0xd7c0) << 10) + (p[1] - 0xdc00);
           p += 2;
           utf16_pos += 2;
        }
        out++;
    }

    return assign( buffer.data() );
}

wxUString &wxUString::assignFromUTF16( const wxChar16* str )
{
    if (!str)
        return assign( wxUString() );

    size_type ucs4_len = 0;
    const wxChar16 *p = str;
    while (*p)
    {
        size_type len;
        if ((*p < 0xd800) || (*p > 0xdfff))
        {
            len = 1;
        }
        else if ((p[1] < 0xdc00) || (p[1] > 0xdfff))
        {
            return assign( wxUString() );  // don't try to convert invalid UTF-16
        }
        else
        {
           len = 2;
        }

        ucs4_len++;
        p += len;
    }

    wxU32CharBuffer buffer( ucs4_len );
    wxChar32 *out = buffer.data();

    p = str;
    while (*p)
    {
        if ((*p < 0xd800) || (*p > 0xdfff))
        {
            *out = *p;
            p++;
        }
        else
        {
           *out = ((p[0] - 0xd7c0) << 10) + (p[1] - 0xdc00);
           p += 2;
        }
        out++;
    }

    return assign( buffer.data() );
}

wxUString &wxUString::assignFromCString( const char* str )
{
    if (!str)
        return assign( wxUString() );

    wxScopedWCharBuffer buffer = wxConvLibc.cMB2WC( str );

    return assign( buffer );
}

wxUString &wxUString::assignFromCString( const char* str, const wxMBConv &conv )
{
    if (!str)
        return assign( wxUString() );

    wxScopedWCharBuffer buffer = conv.cMB2WC( str );

    return assign( buffer );
}

wxScopedCharBuffer wxUString::utf8_str() const
{
    size_type utf8_length = 0;
    const wxChar32 *ptr = data();

    while (*ptr)
    {
        wxChar32 code = *ptr;
        ptr++;

        if ( code <= 0x7F )
        {
            utf8_length++;
        }
        else if ( code <= 0x07FF )
        {
            utf8_length += 2;
        }
        else if ( code < 0xFFFF )
        {
            utf8_length += 3;
        }
        else if ( code <= 0x10FFFF )
        {
            utf8_length += 4;
        }
        else
        {
            // invalid range, skip
        }
    }

    wxCharBuffer result( utf8_length );

    char *out = result.data();

    ptr = data();
    while (*ptr)
    {
        wxChar32 code = *ptr;
        ptr++;

        if ( code <= 0x7F )
        {
            out[0] = (char)code;
            out++;
        }
        else if ( code <= 0x07FF )
        {
            out[1] = 0x80 | (code & 0x3F);  code >>= 6;
            out[0] = 0xC0 | code;
            out += 2;
        }
        else if ( code < 0xFFFF )
        {
            out[2] = 0x80 | (code & 0x3F);  code >>= 6;
            out[1] = 0x80 | (code & 0x3F);  code >>= 6;
            out[0] = 0xE0 | code;
            out += 3;
        }
        else if ( code <= 0x10FFFF )
        {
            out[3] = 0x80 | (code & 0x3F);  code >>= 6;
            out[2] = 0x80 | (code & 0x3F);  code >>= 6;
            out[1] = 0x80 | (code & 0x3F);  code >>= 6;
            out[0] = 0xF0 | code;
            out += 4;
        }
        else
        {
            // invalid range, skip
        }
    }

    return result;
}

wxScopedU16CharBuffer wxUString::utf16_str() const
{
    size_type utf16_length = 0;
    const wxChar32 *ptr = data();

    while (*ptr)
    {
        wxChar32 code = *ptr;
        ptr++;

        // TODO: error range checks

        if (code < 0x10000)
           utf16_length++;
        else
           utf16_length += 2;
    }

    wxU16CharBuffer result( utf16_length );
    wxChar16 *out = result.data();

    ptr = data();

    while (*ptr)
    {
        wxChar32 code = *ptr;
        ptr++;

        // TODO: error range checks

        if (code < 0x10000)
        {
           out[0] = code;
           out++;
        }
        else
        {
           out[0] = (code - 0x10000) / 0x400 + 0xd800;
           out[1] = (code - 0x10000) % 0x400 + 0xdc00;
           out += 2;
        }
    }

    return result;
}
