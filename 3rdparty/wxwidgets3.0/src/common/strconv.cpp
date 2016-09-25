/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/strconv.cpp
// Purpose:     Unicode conversion classes
// Author:      Ove Kaaven, Robert Roebling, Vadim Zeitlin, Vaclav Slavik,
//              Ryan Norton, Fredrik Roubert (UTF7)
// Modified by:
// Created:     29/01/98
// Copyright:   (c) 1999 Ove Kaaven, Robert Roebling, Vaclav Slavik
//              (c) 2000-2003 Vadim Zeitlin
//              (c) 2004 Ryan Norton, Fredrik Roubert
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif  //__BORLANDC__

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/utils.h"
    #include "wx/hashmap.h"
#endif

#include "wx/strconv.h"

#ifndef __WXWINCE__
#include <errno.h>
#endif

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#if defined(__WIN32__) && !defined(__WXMICROWIN__)
    #include "wx/msw/private.h"
    #include "wx/msw/missing.h"
    #define wxHAVE_WIN32_MB2WC
#endif

#ifdef HAVE_ICONV
    #include <iconv.h>
    #include "wx/thread.h"
#endif

#include "wx/encconv.h"
#include "wx/fontmap.h"

#ifdef __DARWIN__
#include "wx/osx/core/private/strconv_cf.h"
#endif //def __DARWIN__


#define TRACE_STRCONV wxT("strconv")

// WC_UTF16 is defined only if sizeof(wchar_t) == 2, otherwise it's supposed to
// be 4 bytes
#if SIZEOF_WCHAR_T == 2
    #define WC_UTF16
#endif


// ============================================================================
// implementation
// ============================================================================

// helper function of cMB2WC(): check if n bytes at this location are all NUL
static bool NotAllNULs(const char *p, size_t n)
{
    while ( n && *p++ == '\0' )
        n--;

    return n != 0;
}

// ----------------------------------------------------------------------------
// UTF-16 en/decoding to/from UCS-4 with surrogates handling
// ----------------------------------------------------------------------------

static size_t encode_utf16(wxUint32 input, wxUint16 *output)
{
    if (input <= 0xffff)
    {
        if (output)
            *output = (wxUint16) input;

        return 1;
    }
    else if (input >= 0x110000)
    {
        return wxCONV_FAILED;
    }
    else
    {
        if (output)
        {
            *output++ = (wxUint16) ((input >> 10) + 0xd7c0);
            *output = (wxUint16) ((input & 0x3ff) + 0xdc00);
        }

        return 2;
    }
}

static size_t decode_utf16(const wxUint16* input, wxUint32& output)
{
    if ((*input < 0xd800) || (*input > 0xdfff))
    {
        output = *input;
        return 1;
    }
    else if ((input[1] < 0xdc00) || (input[1] > 0xdfff))
    {
        output = *input;
        return wxCONV_FAILED;
    }
    else
    {
        output = ((input[0] - 0xd7c0) << 10) + (input[1] - 0xdc00);
        return 2;
    }
}

#ifdef WC_UTF16
    typedef wchar_t wxDecodeSurrogate_t;
#else // !WC_UTF16
    typedef wxUint16 wxDecodeSurrogate_t;
#endif // WC_UTF16/!WC_UTF16

// returns the next UTF-32 character from the wchar_t buffer and advances the
// pointer to the character after this one
//
// if an invalid character is found, *pSrc is set to NULL, the caller must
// check for this
static wxUint32 wxDecodeSurrogate(const wxDecodeSurrogate_t **pSrc)
{
    wxUint32 out;
    const size_t
        n = decode_utf16(reinterpret_cast<const wxUint16 *>(*pSrc), out);
    if ( n == wxCONV_FAILED )
        *pSrc = NULL;
    else
        *pSrc += n;

    return out;
}

// ----------------------------------------------------------------------------
// wxMBConv
// ----------------------------------------------------------------------------

size_t
wxMBConv::ToWChar(wchar_t *dst, size_t dstLen,
                  const char *src, size_t srcLen) const
{
    // although new conversion classes are supposed to implement this function
    // directly, the existing ones only implement the old MB2WC() and so, to
    // avoid to have to rewrite all conversion classes at once, we provide a
    // default (but not efficient) implementation of this one in terms of the
    // old function by copying the input to ensure that it's NUL-terminated and
    // then using MB2WC() to convert it
    //
    // moreover, some conversion classes simply can't implement ToWChar()
    // directly, the primary example is wxConvLibc: mbstowcs() only handles
    // NUL-terminated strings

    // the number of chars [which would be] written to dst [if it were not NULL]
    size_t dstWritten = 0;

    // the number of NULs terminating this string
    size_t nulLen = 0;  // not really needed, but just to avoid warnings

    // if we were not given the input size we just have to assume that the
    // string is properly terminated as we have no way of knowing how long it
    // is anyhow, but if we do have the size check whether there are enough
    // NULs at the end
    wxCharBuffer bufTmp;
    const char *srcEnd;
    if ( srcLen != wxNO_LEN )
    {
        // we need to know how to find the end of this string
        nulLen = GetMBNulLen();
        if ( nulLen == wxCONV_FAILED )
            return wxCONV_FAILED;

        // if there are enough NULs we can avoid the copy
        if ( srcLen < nulLen || NotAllNULs(src + srcLen - nulLen, nulLen) )
        {
            // make a copy in order to properly NUL-terminate the string
            bufTmp = wxCharBuffer(srcLen + nulLen - 1 /* 1 will be added */);
            char * const p = bufTmp.data();
            memcpy(p, src, srcLen);
            for ( char *s = p + srcLen; s < p + srcLen + nulLen; s++ )
                *s = '\0';

            src = bufTmp;
        }

        srcEnd = src + srcLen;
    }
    else // quit after the first loop iteration
    {
        srcEnd = NULL;
    }

    // the idea of this code is straightforward: it converts a NUL-terminated
    // chunk of the string during each iteration and updates the output buffer
    // with the result
    //
    // all the complication come from the fact that this function, for
    // historical reasons, must behave in 2 subtly different ways when it's
    // called with a fixed number of characters and when it's called for the
    // entire NUL-terminated string: in the former case (srcEnd != NULL) we
    // must count all characters we convert, NUL or not; but in the latter we
    // do not count the trailing NUL -- but still count all the NULs inside the
    // string
    //
    // so for the (simple) former case we just always count the trailing NUL,
    // but for the latter we need to wait until we see if there is going to be
    // another loop iteration and only count it then
    for ( ;; )
    {
        // try to convert the current chunk
        size_t lenChunk = MB2WC(NULL, src, 0);
        if ( lenChunk == wxCONV_FAILED )
            return wxCONV_FAILED;

        dstWritten += lenChunk;
        if ( !srcEnd )
            dstWritten++;

        if ( dst )
        {
            if ( dstWritten > dstLen )
                return wxCONV_FAILED;

            // +1 is for trailing NUL
            if ( MB2WC(dst, src, lenChunk + 1) == wxCONV_FAILED )
                return wxCONV_FAILED;

            dst += lenChunk;
            if ( !srcEnd )
                dst++;
        }

        if ( !srcEnd )
        {
            // we convert just one chunk in this case as this is the entire
            // string anyhow (and we don't count the trailing NUL in this case)
            break;
        }

        // advance the input pointer past the end of this chunk: notice that we
        // will always stop before srcEnd because we know that the chunk is
        // always properly NUL-terminated
        while ( NotAllNULs(src, nulLen) )
        {
            // notice that we must skip over multiple bytes here as we suppose
            // that if NUL takes 2 or 4 bytes, then all the other characters do
            // too and so if advanced by a single byte we might erroneously
            // detect sequences of NUL bytes in the middle of the input
            src += nulLen;
        }

        // if the buffer ends before this NUL, we shouldn't count it in our
        // output so skip the code below
        if ( src == srcEnd )
            break;

        // do count this terminator as it's inside the buffer we convert
        dstWritten++;
        if ( dst )
            dst++;

        src += nulLen; // skip the terminator itself

        if ( src >= srcEnd )
            break;
    }

    return dstWritten;
}

size_t
wxMBConv::FromWChar(char *dst, size_t dstLen,
                    const wchar_t *src, size_t srcLen) const
{
    // the number of chars [which would be] written to dst [if it were not NULL]
    size_t dstWritten = 0;

    // if we don't know its length we have no choice but to assume that it is
    // NUL-terminated (notice that it can still be NUL-terminated even if
    // explicit length is given but it doesn't change our return value)
    const bool isNulTerminated = srcLen == wxNO_LEN;

    // make a copy of the input string unless it is already properly
    // NUL-terminated
    wxWCharBuffer bufTmp;
    if ( isNulTerminated )
    {
        srcLen = wxWcslen(src) + 1;
    }
    else if ( srcLen != 0 && src[srcLen - 1] != L'\0' )
    {
        // make a copy in order to properly NUL-terminate the string
        bufTmp = wxWCharBuffer(srcLen);
        memcpy(bufTmp.data(), src, srcLen * sizeof(wchar_t));
        src = bufTmp;
    }

    const size_t lenNul = GetMBNulLen();
    for ( const wchar_t * const srcEnd = src + srcLen;
          src < srcEnd;
          src++ /* skip L'\0' too */ )
    {
        // try to convert the current chunk
        size_t lenChunk = WC2MB(NULL, src, 0);
        if ( lenChunk == wxCONV_FAILED )
            return wxCONV_FAILED;

        dstWritten += lenChunk;

        const wchar_t * const
            chunkEnd = isNulTerminated ? srcEnd - 1 : src + wxWcslen(src);

        // our return value accounts for the trailing NUL(s), unlike that of
        // WC2MB(), however don't do it for the last NUL we artificially added
        // ourselves above
        if ( chunkEnd < srcEnd )
            dstWritten += lenNul;

        if ( dst )
        {
            if ( dstWritten > dstLen )
                return wxCONV_FAILED;

            // if we know that there is enough space in the destination buffer
            // (because we accounted for lenNul in dstWritten above), we can
            // convert directly in place -- but otherwise we need another
            // temporary buffer to ensure that we don't overwrite the output
            wxCharBuffer dstBuf;
            char *dstTmp;
            if ( chunkEnd == srcEnd )
            {
                dstBuf = wxCharBuffer(lenChunk + lenNul - 1);
                dstTmp = dstBuf.data();
            }
            else
            {
                dstTmp = dst;
            }

            if ( WC2MB(dstTmp, src, lenChunk + lenNul) == wxCONV_FAILED )
                return wxCONV_FAILED;

            if ( dstTmp != dst )
            {
                // copy everything up to but excluding the terminating NUL(s)
                // into the real output buffer
                memcpy(dst, dstTmp, lenChunk);

                // micro-optimization: if dstTmp != dst it means that chunkEnd
                // == srcEnd and so we're done, no need to update anything below
                break;
            }

            dst += lenChunk;
            if ( chunkEnd < srcEnd )
                dst += lenNul;
        }

        src = chunkEnd;
    }

    return dstWritten;
}

size_t wxMBConv::MB2WC(wchar_t *outBuff, const char *inBuff, size_t outLen) const
{
    size_t rc = ToWChar(outBuff, outLen, inBuff);
    if ( rc != wxCONV_FAILED )
    {
        // ToWChar() returns the buffer length, i.e. including the trailing
        // NUL, while this method doesn't take it into account
        rc--;
    }

    return rc;
}

size_t wxMBConv::WC2MB(char *outBuff, const wchar_t *inBuff, size_t outLen) const
{
    size_t rc = FromWChar(outBuff, outLen, inBuff);
    if ( rc != wxCONV_FAILED )
    {
        rc -= GetMBNulLen();
    }

    return rc;
}

wxMBConv::~wxMBConv()
{
    // nothing to do here (necessary for Darwin linking probably)
}

const wxWCharBuffer wxMBConv::cMB2WC(const char *psz) const
{
    if ( psz )
    {
        // calculate the length of the buffer needed first
        const size_t nLen = ToWChar(NULL, 0, psz);
        if ( nLen != wxCONV_FAILED )
        {
            // now do the actual conversion
            wxWCharBuffer buf(nLen - 1 /* +1 added implicitly */);

            // +1 for the trailing NULL
            if ( ToWChar(buf.data(), nLen, psz) != wxCONV_FAILED )
                return buf;
        }
    }

    return wxWCharBuffer();
}

const wxCharBuffer wxMBConv::cWC2MB(const wchar_t *pwz) const
{
    if ( pwz )
    {
        const size_t nLen = FromWChar(NULL, 0, pwz);
        if ( nLen != wxCONV_FAILED )
        {
            wxCharBuffer buf(nLen - 1);
            if ( FromWChar(buf.data(), nLen, pwz) != wxCONV_FAILED )
                return buf;
        }
    }

    return wxCharBuffer();
}

const wxWCharBuffer
wxMBConv::cMB2WC(const char *inBuff, size_t inLen, size_t *outLen) const
{
    const size_t dstLen = ToWChar(NULL, 0, inBuff, inLen);
    if ( dstLen != wxCONV_FAILED )
    {
        // notice that we allocate space for dstLen+1 wide characters here
        // because we want the buffer to always be NUL-terminated, even if the
        // input isn't (as otherwise the caller has no way to know its length)
        wxWCharBuffer wbuf(dstLen);
        if ( ToWChar(wbuf.data(), dstLen, inBuff, inLen) != wxCONV_FAILED )
        {
            if ( outLen )
            {
                *outLen = dstLen;

                // we also need to handle NUL-terminated input strings
                // specially: for them the output is the length of the string
                // excluding the trailing NUL, however if we're asked to
                // convert a specific number of characters we return the length
                // of the resulting output even if it's NUL-terminated
                if ( inLen == wxNO_LEN )
                    (*outLen)--;
            }

            return wbuf;
        }
    }

    if ( outLen )
        *outLen = 0;

    return wxWCharBuffer();
}

const wxCharBuffer
wxMBConv::cWC2MB(const wchar_t *inBuff, size_t inLen, size_t *outLen) const
{
    size_t dstLen = FromWChar(NULL, 0, inBuff, inLen);
    if ( dstLen != wxCONV_FAILED )
    {
        const size_t nulLen = GetMBNulLen();

        // as above, ensure that the buffer is always NUL-terminated, even if
        // the input is not
        wxCharBuffer buf(dstLen + nulLen - 1);
        memset(buf.data() + dstLen, 0, nulLen);
        if ( FromWChar(buf.data(), dstLen, inBuff, inLen) != wxCONV_FAILED )
        {
            if ( outLen )
            {
                *outLen = dstLen;

                if ( inLen == wxNO_LEN )
                {
                    // in this case both input and output are NUL-terminated
                    // and we're not supposed to count NUL
                    *outLen -= nulLen;
                }
            }

            return buf;
        }
    }

    if ( outLen )
        *outLen = 0;

    return wxCharBuffer();
}

const wxWCharBuffer wxMBConv::cMB2WC(const wxScopedCharBuffer& buf) const
{
    const size_t srcLen = buf.length();
    if ( srcLen )
    {
        const size_t dstLen = ToWChar(NULL, 0, buf, srcLen);
        if ( dstLen != wxCONV_FAILED )
        {
            wxWCharBuffer wbuf(dstLen);
            wbuf.data()[dstLen] = L'\0';
            if ( ToWChar(wbuf.data(), dstLen, buf, srcLen) != wxCONV_FAILED )
                return wbuf;
        }
    }

    return wxScopedWCharBuffer::CreateNonOwned(L"", 0);
}

const wxCharBuffer wxMBConv::cWC2MB(const wxScopedWCharBuffer& wbuf) const
{
    const size_t srcLen = wbuf.length();
    if ( srcLen )
    {
        const size_t dstLen = FromWChar(NULL, 0, wbuf, srcLen);
        if ( dstLen != wxCONV_FAILED )
        {
            wxCharBuffer buf(dstLen);
            buf.data()[dstLen] = '\0';
            if ( FromWChar(buf.data(), dstLen, wbuf, srcLen) != wxCONV_FAILED )
                return buf;
        }
    }

    return wxScopedCharBuffer::CreateNonOwned("", 0);
}

// ----------------------------------------------------------------------------
// wxMBConvLibc
// ----------------------------------------------------------------------------

size_t wxMBConvLibc::MB2WC(wchar_t *buf, const char *psz, size_t n) const
{
    return wxMB2WC(buf, psz, n);
}

size_t wxMBConvLibc::WC2MB(char *buf, const wchar_t *psz, size_t n) const
{
    return wxWC2MB(buf, psz, n);
}

// ----------------------------------------------------------------------------
// wxConvBrokenFileNames
// ----------------------------------------------------------------------------

#ifdef __UNIX__

wxConvBrokenFileNames::wxConvBrokenFileNames(const wxString& charset)
{
    if ( wxStricmp(charset, wxT("UTF-8")) == 0 ||
         wxStricmp(charset, wxT("UTF8")) == 0  )
        m_conv = new wxMBConvUTF8(wxMBConvUTF8::MAP_INVALID_UTF8_TO_PUA);
    else
        m_conv = new wxCSConv(charset);
}

#endif // __UNIX__

// ----------------------------------------------------------------------------
// UTF-7
// ----------------------------------------------------------------------------

// Implementation (C) 2004 Fredrik Roubert
//
// Changes to work in streaming mode (C) 2008 Vadim Zeitlin

//
// BASE64 decoding table
//
static const unsigned char utf7unb64[] =
{
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x3e, 0xff, 0xff, 0xff, 0x3f,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
    0x3c, 0x3d, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
    0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
    0x31, 0x32, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

size_t wxMBConvUTF7::ToWChar(wchar_t *dst, size_t dstLen,
                             const char *src, size_t srcLen) const
{
    DecoderState stateOrig,
                *statePtr;
    if ( srcLen == wxNO_LEN )
    {
        // convert the entire string, up to and including the trailing NUL
        srcLen = strlen(src) + 1;

        // when working on the entire strings we don't update nor use the shift
        // state from the previous call
        statePtr = &stateOrig;
    }
    else // when working with partial strings we do use the shift state
    {
        statePtr = const_cast<DecoderState *>(&m_stateDecoder);

        // also save the old state to be able to rollback to it on error
        stateOrig = m_stateDecoder;
    }

    // but to simplify the code below we use this variable in both cases
    DecoderState& state = *statePtr;


    // number of characters [which would have been] written to dst [if it were
    // not NULL]
    size_t len = 0;

    const char * const srcEnd = src + srcLen;

    while ( (src < srcEnd) && (!dst || (len < dstLen)) )
    {
        const unsigned char cc = *src++;

        if ( state.IsShifted() )
        {
            const unsigned char dc = utf7unb64[cc];
            if ( dc == 0xff )
            {
                // end of encoded part, check that nothing was left: there can
                // be up to 4 bits of 0 padding but nothing else (we also need
                // to check isLSB as we count bits modulo 8 while a valid UTF-7
                // encoded sequence must contain an integral number of UTF-16
                // characters)
                if ( state.isLSB || state.bit > 4 ||
                        (state.accum & ((1 << state.bit) - 1)) )
                {
                    if ( !len )
                        state = stateOrig;

                    return wxCONV_FAILED;
                }

                state.ToDirect();

                // re-parse this character normally below unless it's '-' which
                // is consumed by the decoder
                if ( cc == '-' )
                    continue;
            }
            else // valid encoded character
            {
                // mini base64 decoder: each character is 6 bits
                state.bit += 6;
                state.accum <<= 6;
                state.accum += dc;

                if ( state.bit >= 8 )
                {
                    // got the full byte, consume it
                    state.bit -= 8;
                    unsigned char b = (state.accum >> state.bit) & 0x00ff;

                    if ( state.isLSB )
                    {
                        // we've got the full word, output it
                        if ( dst )
                            *dst++ = (state.msb << 8) | b;
                        len++;
                        state.isLSB = false;
                    }
                    else // MSB
                    {
                        // just store it while we wait for LSB
                        state.msb = b;
                        state.isLSB = true;
                    }
                }
            }
        }

        if ( state.IsDirect() )
        {
            // start of an encoded segment?
            if ( cc == '+' )
            {
                if ( *src == '-' )
                {
                    // just the encoded plus sign, don't switch to shifted mode
                    if ( dst )
                        *dst++ = '+';
                    len++;
                    src++;
                }
                else if ( utf7unb64[(unsigned)*src] == 0xff )
                {
                    // empty encoded chunks are not allowed
                    if ( !len )
                        state = stateOrig;

                    return wxCONV_FAILED;
                }
                else // base-64 encoded chunk follows
                {
                    state.ToShifted();
                }
            }
            else // not '+'
            {
                // only printable 7 bit ASCII characters (with the exception of
                // NUL, TAB, CR and LF) can be used directly
                if ( cc >= 0x7f || (cc < ' ' &&
                      !(cc == '\0' || cc == '\t' || cc == '\r' || cc == '\n')) )
                    return wxCONV_FAILED;

                if ( dst )
                    *dst++ = cc;
                len++;
            }
        }
    }

    if ( !len )
    {
        // as we didn't read any characters we should be called with the same
        // data (followed by some more new data) again later so don't save our
        // state
        state = stateOrig;

        return wxCONV_FAILED;
    }

    return len;
}

//
// BASE64 encoding table
//
static const unsigned char utf7enb64[] =
{
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/'
};

//
// UTF-7 encoding table
//
// 0 - Set D (directly encoded characters)
// 1 - Set O (optional direct characters)
// 2 - whitespace characters (optional)
// 3 - special characters
//
static const unsigned char utf7encode[128] =
{
    0, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 3, 3, 2, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    2, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 3, 0, 0, 0, 3,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 3, 3
};

static inline bool wxIsUTF7Direct(wchar_t wc)
{
    return wc < 0x80 && utf7encode[wc] < 1;
}

size_t wxMBConvUTF7::FromWChar(char *dst, size_t dstLen,
                               const wchar_t *src, size_t srcLen) const
{
    EncoderState stateOrig,
                *statePtr;
    if ( srcLen == wxNO_LEN )
    {
        // we don't apply the stored state when operating on entire strings at
        // once
        statePtr = &stateOrig;

        srcLen = wxWcslen(src) + 1;
    }
    else // do use the mode we left the output in previously
    {
        stateOrig = m_stateEncoder;
        statePtr = const_cast<EncoderState *>(&m_stateEncoder);
    }

    EncoderState& state = *statePtr;


    size_t len = 0;

    const wchar_t * const srcEnd = src + srcLen;
    while ( src < srcEnd && (!dst || len < dstLen) )
    {
        wchar_t cc = *src++;
        if ( wxIsUTF7Direct(cc) )
        {
            if ( state.IsShifted() )
            {
                // pad with zeros the last encoded block if necessary
                if ( state.bit )
                {
                    if ( dst )
                        *dst++ = utf7enb64[((state.accum % 16) << (6 - state.bit)) % 64];
                    len++;
                }

                state.ToDirect();

                if ( dst )
                    *dst++ = '-';
                len++;
            }

            if ( dst )
                *dst++ = (char)cc;
            len++;
        }
        else if ( cc == '+' && state.IsDirect() )
        {
            if ( dst )
            {
                *dst++ = '+';
                *dst++ = '-';
            }

            len += 2;
        }
#ifndef WC_UTF16
        else if (((wxUint32)cc) > 0xffff)
        {
            // no surrogate pair generation (yet?)
            return wxCONV_FAILED;
        }
#endif
        else
        {
            if ( state.IsDirect() )
            {
                state.ToShifted();

                if ( dst )
                    *dst++ = '+';
                len++;
            }

            // BASE64 encode string
            for ( ;; )
            {
                for ( unsigned lsb = 0; lsb < 2; lsb++ )
                {
                    state.accum <<= 8;
                    state.accum += lsb ? cc & 0xff : (cc & 0xff00) >> 8;

                    for (state.bit += 8; state.bit >= 6; )
                    {
                        state.bit -= 6;
                        if ( dst )
                            *dst++ = utf7enb64[(state.accum >> state.bit) % 64];
                        len++;
                    }
                }

                if ( src == srcEnd || wxIsUTF7Direct(cc = *src) )
                    break;

                src++;
            }
        }
    }

    // we need to restore the original encoder state if we were called just to
    // calculate the amount of space needed as we will presumably be called
    // again to really convert the data now
    if ( !dst )
        state = stateOrig;

    return len;
}

// ----------------------------------------------------------------------------
// UTF-8
// ----------------------------------------------------------------------------

static const wxUint32 utf8_max[]=
    { 0x7f, 0x7ff, 0xffff, 0x1fffff, 0x3ffffff, 0x7fffffff, 0xffffffff };

// boundaries of the private use area we use to (temporarily) remap invalid
// characters invalid in a UTF-8 encoded string
const wxUint32 wxUnicodePUA = 0x100000;
const wxUint32 wxUnicodePUAEnd = wxUnicodePUA + 256;

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

size_t
wxMBConvStrictUTF8::ToWChar(wchar_t *dst, size_t dstLen,
                            const char *src, size_t srcLen) const
{
    wchar_t *out = dstLen ? dst : NULL;
    size_t written = 0;

    if ( srcLen == wxNO_LEN )
        srcLen = strlen(src) + 1;

    for ( const char *p = src; ; p++ )
    {
        if ( (srcLen == wxNO_LEN ? !*p : !srcLen) )
        {
            // all done successfully, just add the trailing NULL if we are not
            // using explicit length
            if ( srcLen == wxNO_LEN )
            {
                if ( out )
                {
                    if ( !dstLen )
                        break;

                    *out = L'\0';
                }

                written++;
            }

            return written;
        }

        if ( out && !dstLen-- )
            break;

        wxUint32 code;
        unsigned char c = *p;

        if ( c < 0x80 )
        {
            if ( srcLen == 0 ) // the test works for wxNO_LEN too
                break;

            if ( srcLen != wxNO_LEN )
                srcLen--;

            code = c;
        }
        else
        {
            unsigned len = tableUtf8Lengths[c];
            if ( !len )
                break;

            if ( srcLen < len ) // the test works for wxNO_LEN too
                break;

            if ( srcLen != wxNO_LEN )
                srcLen -= len;

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

            len--; // it's more convenient to work with 0-based length here

            code = c & leadValueMask[len];

            // all remaining bytes, if any, are handled in the same way
            // regardless of sequence's length:
            for ( ; len; --len )
            {
                c = *++p;
                if ( (c & 0xC0) != 0x80 )
                    return wxCONV_FAILED;

                code <<= 6;
                code |= c & 0x3F;
            }
        }

#ifdef WC_UTF16
        // cast is ok because wchar_t == wxUint16 if WC_UTF16
        if ( encode_utf16(code, (wxUint16 *)out) == 2 )
        {
            if ( out )
                out++;
            written++;
        }
#else // !WC_UTF16
        if ( out )
            *out = code;
#endif // WC_UTF16/!WC_UTF16

        if ( out )
            out++;

        written++;
    }

    return wxCONV_FAILED;
}

size_t
wxMBConvStrictUTF8::FromWChar(char *dst, size_t dstLen,
                              const wchar_t *src, size_t srcLen) const
{
    char *out = dstLen ? dst : NULL;
    size_t written = 0;

    for ( const wchar_t *wp = src; ; wp++ )
    {
        if ( (srcLen == wxNO_LEN ? !*wp : !srcLen) )
        {
            // all done successfully, just add the trailing NULL if we are not
            // using explicit length
            if ( srcLen == wxNO_LEN )
            {
                if ( out )
                {
                    if ( !dstLen )
                        break;

                    *out = '\0';
                }

                written++;
            }

            return written;
        }

        if ( srcLen != wxNO_LEN )
            srcLen--;

        wxUint32 code;
#ifdef WC_UTF16
        // cast is ok for WC_UTF16
        if ( decode_utf16((const wxUint16 *)wp, code) == 2 )
        {
            // skip the next char too as we decoded a surrogate
            wp++;
            if ( srcLen != wxNO_LEN )
                srcLen--;
        }
#else // wchar_t is UTF-32
        code = *wp & 0x7fffffff;
#endif

        unsigned len;
        if ( code <= 0x7F )
        {
            len = 1;
            if ( out )
            {
                if ( dstLen < len )
                    break;

                out[0] = (char)code;
            }
        }
        else if ( code <= 0x07FF )
        {
            len = 2;
            if ( out )
            {
                if ( dstLen < len )
                    break;

                // NB: this line takes 6 least significant bits, encodes them as
                // 10xxxxxx and discards them so that the next byte can be encoded:
                out[1] = 0x80 | (code & 0x3F);  code >>= 6;
                out[0] = 0xC0 | code;
            }
        }
        else if ( code < 0xFFFF )
        {
            len = 3;
            if ( out )
            {
                if ( dstLen < len )
                    break;

                out[2] = 0x80 | (code & 0x3F);  code >>= 6;
                out[1] = 0x80 | (code & 0x3F);  code >>= 6;
                out[0] = 0xE0 | code;
            }
        }
        else if ( code <= 0x10FFFF )
        {
            len = 4;
            if ( out )
            {
                if ( dstLen < len )
                    break;

                out[3] = 0x80 | (code & 0x3F);  code >>= 6;
                out[2] = 0x80 | (code & 0x3F);  code >>= 6;
                out[1] = 0x80 | (code & 0x3F);  code >>= 6;
                out[0] = 0xF0 | code;
            }
        }
        else
        {
            wxFAIL_MSG( wxT("trying to encode undefined Unicode character") );
            break;
        }

        if ( out )
        {
            out += len;
            dstLen -= len;
        }

        written += len;
    }

    // we only get here if an error occurs during decoding
    return wxCONV_FAILED;
}

size_t wxMBConvUTF8::ToWChar(wchar_t *buf, size_t n,
                             const char *psz, size_t srcLen) const
{
    if ( m_options == MAP_INVALID_UTF8_NOT )
        return wxMBConvStrictUTF8::ToWChar(buf, n, psz, srcLen);

    size_t len = 0;

    // The length can be either given explicitly or computed implicitly for the
    // NUL-terminated strings.
    const bool isNulTerminated = srcLen == wxNO_LEN;
    while ((isNulTerminated ? *psz : srcLen--) && ((!buf) || (len < n)))
    {
        const char *opsz = psz;
        bool invalid = false;
        unsigned char cc = *psz++, fc = cc;
        unsigned cnt;
        for (cnt = 0; fc & 0x80; cnt++)
            fc <<= 1;

        if (!cnt)
        {
            // plain ASCII char
            if (buf)
                *buf++ = cc;
            len++;

            // escape the escape character for octal escapes
            if ((m_options & MAP_INVALID_UTF8_TO_OCTAL)
                    && cc == '\\' && (!buf || len < n))
            {
                if (buf)
                    *buf++ = cc;
                len++;
            }
        }
        else
        {
            cnt--;
            if (!cnt)
            {
                // invalid UTF-8 sequence
                invalid = true;
            }
            else
            {
                unsigned ocnt = cnt - 1;
                wxUint32 res = cc & (0x3f >> cnt);
                while (cnt--)
                {
                    if (!isNulTerminated && !srcLen)
                    {
                        // invalid UTF-8 sequence ending before the end of code
                        // point.
                        invalid = true;
                        break;
                    }

                    cc = *psz;
                    if ((cc & 0xC0) != 0x80)
                    {
                        // invalid UTF-8 sequence
                        invalid = true;
                        break;
                    }

                    psz++;
                    if (!isNulTerminated)
                        srcLen--;
                    res = (res << 6) | (cc & 0x3f);
                }

                if (invalid || res <= utf8_max[ocnt])
                {
                    // illegal UTF-8 encoding
                    invalid = true;
                }
                else if ((m_options & MAP_INVALID_UTF8_TO_PUA) &&
                        res >= wxUnicodePUA && res < wxUnicodePUAEnd)
                {
                    // if one of our PUA characters turns up externally
                    // it must also be treated as an illegal sequence
                    // (a bit like you have to escape an escape character)
                    invalid = true;
                }
                else
                {
#ifdef WC_UTF16
                    // cast is ok because wchar_t == wxUint16 if WC_UTF16
                    size_t pa = encode_utf16(res, (wxUint16 *)buf);
                    if (pa == wxCONV_FAILED)
                    {
                        invalid = true;
                    }
                    else
                    {
                        if (buf)
                            buf += pa;
                        len += pa;
                    }
#else // !WC_UTF16
                    if (buf)
                        *buf++ = (wchar_t)res;
                    len++;
#endif // WC_UTF16/!WC_UTF16
                }
            }

            if (invalid)
            {
                if (m_options & MAP_INVALID_UTF8_TO_PUA)
                {
                    while (opsz < psz && (!buf || len < n))
                    {
#ifdef WC_UTF16
                        // cast is ok because wchar_t == wxUuint16 if WC_UTF16
                        size_t pa = encode_utf16((unsigned char)*opsz + wxUnicodePUA, (wxUint16 *)buf);
                        wxASSERT(pa != wxCONV_FAILED);
                        if (buf)
                            buf += pa;
                        opsz++;
                        len += pa;
#else
                        if (buf)
                            *buf++ = (wchar_t)(wxUnicodePUA + (unsigned char)*opsz);
                        opsz++;
                        len++;
#endif
                    }
                }
                else if (m_options & MAP_INVALID_UTF8_TO_OCTAL)
                {
                    while (opsz < psz && (!buf || len < n))
                    {
                        if ( buf && len + 3 < n )
                        {
                            unsigned char on = *opsz;
                            *buf++ = L'\\';
                            *buf++ = (wchar_t)( L'0' + on / 0100 );
                            *buf++ = (wchar_t)( L'0' + (on % 0100) / 010 );
                            *buf++ = (wchar_t)( L'0' + on % 010 );
                        }

                        opsz++;
                        len += 4;
                    }
                }
                else // MAP_INVALID_UTF8_NOT
                {
                    return wxCONV_FAILED;
                }
            }
        }
    }

    if ( isNulTerminated )
    {
        // Add the trailing NUL in this case if we have a large enough buffer.
        if ( buf && (len < n) )
            *buf = 0;

        // And count it in any case.
        len++;
    }

    return len;
}

static inline bool isoctal(wchar_t wch)
{
    return L'0' <= wch && wch <= L'7';
}

size_t wxMBConvUTF8::FromWChar(char *buf, size_t n,
                               const wchar_t *psz, size_t srcLen) const
{
    if ( m_options == MAP_INVALID_UTF8_NOT )
        return wxMBConvStrictUTF8::FromWChar(buf, n, psz, srcLen);

    size_t len = 0;

    // The length can be either given explicitly or computed implicitly for the
    // NUL-terminated strings.
    const bool isNulTerminated = srcLen == wxNO_LEN;
    while ((isNulTerminated ? *psz : srcLen--) && ((!buf) || (len < n)))
    {
        wxUint32 cc;

#ifdef WC_UTF16
        // cast is ok for WC_UTF16
        size_t pa = decode_utf16((const wxUint16 *)psz, cc);
        psz += (pa == wxCONV_FAILED) ? 1 : pa;
#else
        cc = (*psz++) & 0x7fffffff;
#endif

        if ( (m_options & MAP_INVALID_UTF8_TO_PUA)
                && cc >= wxUnicodePUA && cc < wxUnicodePUAEnd )
        {
            if (buf)
                *buf++ = (char)(cc - wxUnicodePUA);
            len++;
        }
        else if ( (m_options & MAP_INVALID_UTF8_TO_OCTAL)
                    && cc == L'\\' && psz[0] == L'\\' )
        {
            if (buf)
                *buf++ = (char)cc;
            psz++;
            len++;
        }
        else if ( (m_options & MAP_INVALID_UTF8_TO_OCTAL) &&
                    cc == L'\\' &&
                        isoctal(psz[0]) && isoctal(psz[1]) && isoctal(psz[2]) )
        {
            if (buf)
            {
                *buf++ = (char) ((psz[0] - L'0') * 0100 +
                                 (psz[1] - L'0') * 010 +
                                 (psz[2] - L'0'));
            }

            psz += 3;
            len++;
        }
        else
        {
            unsigned cnt;
            for (cnt = 0; cc > utf8_max[cnt]; cnt++)
            {
            }

            if (!cnt)
            {
                // plain ASCII char
                if (buf)
                    *buf++ = (char) cc;
                len++;
            }
            else
            {
                len += cnt + 1;
                if (buf)
                {
                    *buf++ = (char) ((-128 >> cnt) | ((cc >> (cnt * 6)) & (0x3f >> cnt)));
                    while (cnt--)
                        *buf++ = (char) (0x80 | ((cc >> (cnt * 6)) & 0x3f));
                }
            }
        }
    }

    if ( isNulTerminated )
    {
        // Add the trailing NUL in this case if we have a large enough buffer.
        if ( buf && (len < n) )
            *buf = 0;

        // And count it in any case.
        len++;
    }

    return len;
}

// ============================================================================
// UTF-16
// ============================================================================

#ifdef WORDS_BIGENDIAN
    #define wxMBConvUTF16straight wxMBConvUTF16BE
    #define wxMBConvUTF16swap     wxMBConvUTF16LE
#else
    #define wxMBConvUTF16swap     wxMBConvUTF16BE
    #define wxMBConvUTF16straight wxMBConvUTF16LE
#endif

/* static */
size_t wxMBConvUTF16Base::GetLength(const char *src, size_t srcLen)
{
    if ( srcLen == wxNO_LEN )
    {
        // count the number of bytes in input, including the trailing NULs
        const wxUint16 *inBuff = reinterpret_cast<const wxUint16 *>(src);
        for ( srcLen = 1; *inBuff++; srcLen++ )
            ;

        srcLen *= BYTES_PER_CHAR;
    }
    else // we already have the length
    {
        // we can only convert an entire number of UTF-16 characters
        if ( srcLen % BYTES_PER_CHAR )
            return wxCONV_FAILED;
    }

    return srcLen;
}

// case when in-memory representation is UTF-16 too
#ifdef WC_UTF16

// ----------------------------------------------------------------------------
// conversions without endianness change
// ----------------------------------------------------------------------------

size_t
wxMBConvUTF16straight::ToWChar(wchar_t *dst, size_t dstLen,
                               const char *src, size_t srcLen) const
{
    // set up the scene for using memcpy() (which is presumably more efficient
    // than copying the bytes one by one)
    srcLen = GetLength(src, srcLen);
    if ( srcLen == wxNO_LEN )
        return wxCONV_FAILED;

    const size_t inLen = srcLen / BYTES_PER_CHAR;
    if ( dst )
    {
        if ( dstLen < inLen )
            return wxCONV_FAILED;

        memcpy(dst, src, srcLen);
    }

    return inLen;
}

size_t
wxMBConvUTF16straight::FromWChar(char *dst, size_t dstLen,
                                 const wchar_t *src, size_t srcLen) const
{
    if ( srcLen == wxNO_LEN )
        srcLen = wxWcslen(src) + 1;

    srcLen *= BYTES_PER_CHAR;

    if ( dst )
    {
        if ( dstLen < srcLen )
            return wxCONV_FAILED;

        memcpy(dst, src, srcLen);
    }

    return srcLen;
}

// ----------------------------------------------------------------------------
// endian-reversing conversions
// ----------------------------------------------------------------------------

size_t
wxMBConvUTF16swap::ToWChar(wchar_t *dst, size_t dstLen,
                           const char *src, size_t srcLen) const
{
    srcLen = GetLength(src, srcLen);
    if ( srcLen == wxNO_LEN )
        return wxCONV_FAILED;

    srcLen /= BYTES_PER_CHAR;

    if ( dst )
    {
        if ( dstLen < srcLen )
            return wxCONV_FAILED;

        const wxUint16 *inBuff = reinterpret_cast<const wxUint16 *>(src);
        for ( size_t n = 0; n < srcLen; n++, inBuff++ )
        {
            *dst++ = wxUINT16_SWAP_ALWAYS(*inBuff);
        }
    }

    return srcLen;
}

size_t
wxMBConvUTF16swap::FromWChar(char *dst, size_t dstLen,
                             const wchar_t *src, size_t srcLen) const
{
    if ( srcLen == wxNO_LEN )
        srcLen = wxWcslen(src) + 1;

    srcLen *= BYTES_PER_CHAR;

    if ( dst )
    {
        if ( dstLen < srcLen )
            return wxCONV_FAILED;

        wxUint16 *outBuff = reinterpret_cast<wxUint16 *>(dst);
        for ( size_t n = 0; n < srcLen; n += BYTES_PER_CHAR, src++ )
        {
            *outBuff++ = wxUINT16_SWAP_ALWAYS(*src);
        }
    }

    return srcLen;
}

#else // !WC_UTF16: wchar_t is UTF-32

// ----------------------------------------------------------------------------
// conversions without endianness change
// ----------------------------------------------------------------------------

size_t
wxMBConvUTF16straight::ToWChar(wchar_t *dst, size_t dstLen,
                               const char *src, size_t srcLen) const
{
    srcLen = GetLength(src, srcLen);
    if ( srcLen == wxNO_LEN )
        return wxCONV_FAILED;

    const size_t inLen = srcLen / BYTES_PER_CHAR;
    size_t outLen = 0;
    const wxUint16 *inBuff = reinterpret_cast<const wxUint16 *>(src);
    for ( const wxUint16 * const inEnd = inBuff + inLen; inBuff < inEnd; )
    {
        const wxUint32 ch = wxDecodeSurrogate(&inBuff);
        if ( !inBuff )
            return wxCONV_FAILED;

        outLen++;

        if ( dst )
        {
            if ( outLen > dstLen )
                return wxCONV_FAILED;

            *dst++ = ch;
        }
    }


    return outLen;
}

size_t
wxMBConvUTF16straight::FromWChar(char *dst, size_t dstLen,
                                 const wchar_t *src, size_t srcLen) const
{
    if ( srcLen == wxNO_LEN )
        srcLen = wxWcslen(src) + 1;

    size_t outLen = 0;
    wxUint16 *outBuff = reinterpret_cast<wxUint16 *>(dst);
    for ( size_t n = 0; n < srcLen; n++ )
    {
        wxUint16 cc[2] = { 0 };
        const size_t numChars = encode_utf16(*src++, cc);
        if ( numChars == wxCONV_FAILED )
            return wxCONV_FAILED;

        outLen += numChars * BYTES_PER_CHAR;
        if ( outBuff )
        {
            if ( outLen > dstLen )
                return wxCONV_FAILED;

            *outBuff++ = cc[0];
            if ( numChars == 2 )
            {
                // second character of a surrogate
                *outBuff++ = cc[1];
            }
        }
    }

    return outLen;
}

// ----------------------------------------------------------------------------
// endian-reversing conversions
// ----------------------------------------------------------------------------

size_t
wxMBConvUTF16swap::ToWChar(wchar_t *dst, size_t dstLen,
                           const char *src, size_t srcLen) const
{
    srcLen = GetLength(src, srcLen);
    if ( srcLen == wxNO_LEN )
        return wxCONV_FAILED;

    const size_t inLen = srcLen / BYTES_PER_CHAR;
    size_t outLen = 0;
    const wxUint16 *inBuff = reinterpret_cast<const wxUint16 *>(src);
    for ( const wxUint16 * const inEnd = inBuff + inLen; inBuff < inEnd; )
    {
        wxUint32 ch;
        wxUint16 tmp[2];

        tmp[0] = wxUINT16_SWAP_ALWAYS(*inBuff);
        if ( ++inBuff < inEnd )
        {
            // Normal case, we have a next character to decode.
            tmp[1] = wxUINT16_SWAP_ALWAYS(*inBuff);
        }
        else // End of input.
        {
            // Setting the second character to 0 ensures we correctly return
            // wxCONV_FAILED if the first one is the first half of a surrogate
            // as the second half can't be 0 in this case.
            tmp[1] = 0;
        }

        const size_t numChars = decode_utf16(tmp, ch);
        if ( numChars == wxCONV_FAILED )
            return wxCONV_FAILED;

        if ( numChars == 2 )
            inBuff++;

        outLen++;

        if ( dst )
        {
            if ( outLen > dstLen )
                return wxCONV_FAILED;

            *dst++ = ch;
        }
    }


    return outLen;
}

size_t
wxMBConvUTF16swap::FromWChar(char *dst, size_t dstLen,
                             const wchar_t *src, size_t srcLen) const
{
    if ( srcLen == wxNO_LEN )
        srcLen = wxWcslen(src) + 1;

    size_t outLen = 0;
    wxUint16 *outBuff = reinterpret_cast<wxUint16 *>(dst);
    for ( const wchar_t *srcEnd = src + srcLen; src < srcEnd; src++ )
    {
        wxUint16 cc[2] = { 0 };
        const size_t numChars = encode_utf16(*src, cc);
        if ( numChars == wxCONV_FAILED )
            return wxCONV_FAILED;

        outLen += numChars * BYTES_PER_CHAR;
        if ( outBuff )
        {
            if ( outLen > dstLen )
                return wxCONV_FAILED;

            *outBuff++ = wxUINT16_SWAP_ALWAYS(cc[0]);
            if ( numChars == 2 )
            {
                // second character of a surrogate
                *outBuff++ = wxUINT16_SWAP_ALWAYS(cc[1]);
            }
        }
    }

    return outLen;
}

#endif // WC_UTF16/!WC_UTF16


// ============================================================================
// UTF-32
// ============================================================================

#ifdef WORDS_BIGENDIAN
    #define wxMBConvUTF32straight  wxMBConvUTF32BE
    #define wxMBConvUTF32swap      wxMBConvUTF32LE
#else
    #define wxMBConvUTF32swap      wxMBConvUTF32BE
    #define wxMBConvUTF32straight  wxMBConvUTF32LE
#endif


WXDLLIMPEXP_DATA_BASE(wxMBConvUTF32LE) wxConvUTF32LE;
WXDLLIMPEXP_DATA_BASE(wxMBConvUTF32BE) wxConvUTF32BE;

/* static */
size_t wxMBConvUTF32Base::GetLength(const char *src, size_t srcLen)
{
    if ( srcLen == wxNO_LEN )
    {
        // count the number of bytes in input, including the trailing NULs
        const wxUint32 *inBuff = reinterpret_cast<const wxUint32 *>(src);
        for ( srcLen = 1; *inBuff++; srcLen++ )
            ;

        srcLen *= BYTES_PER_CHAR;
    }
    else // we already have the length
    {
        // we can only convert an entire number of UTF-32 characters
        if ( srcLen % BYTES_PER_CHAR )
            return wxCONV_FAILED;
    }

    return srcLen;
}

// case when in-memory representation is UTF-16
#ifdef WC_UTF16

// ----------------------------------------------------------------------------
// conversions without endianness change
// ----------------------------------------------------------------------------

size_t
wxMBConvUTF32straight::ToWChar(wchar_t *dst, size_t dstLen,
                               const char *src, size_t srcLen) const
{
    srcLen = GetLength(src, srcLen);
    if ( srcLen == wxNO_LEN )
        return wxCONV_FAILED;

    const wxUint32 *inBuff = reinterpret_cast<const wxUint32 *>(src);
    const size_t inLen = srcLen / BYTES_PER_CHAR;
    size_t outLen = 0;
    for ( size_t n = 0; n < inLen; n++ )
    {
        wxUint16 cc[2] = { 0 };
        const size_t numChars = encode_utf16(*inBuff++, cc);
        if ( numChars == wxCONV_FAILED )
            return wxCONV_FAILED;

        outLen += numChars;
        if ( dst )
        {
            if ( outLen > dstLen )
                return wxCONV_FAILED;

            *dst++ = cc[0];
            if ( numChars == 2 )
            {
                // second character of a surrogate
                *dst++ = cc[1];
            }
        }
    }

    return outLen;
}

size_t
wxMBConvUTF32straight::FromWChar(char *dst, size_t dstLen,
                                 const wchar_t *src, size_t srcLen) const
{
    if ( srcLen == wxNO_LEN )
        srcLen = wxWcslen(src) + 1;

    if ( !dst )
    {
        // optimization: return maximal space which could be needed for this
        // string instead of the exact amount which could be less if there are
        // any surrogates in the input
        //
        // we consider that surrogates are rare enough to make it worthwhile to
        // avoid running the loop below at the cost of slightly extra memory
        // consumption
        return srcLen * BYTES_PER_CHAR;
    }

    wxUint32 *outBuff = reinterpret_cast<wxUint32 *>(dst);
    size_t outLen = 0;
    for ( const wchar_t * const srcEnd = src + srcLen; src < srcEnd; )
    {
        const wxUint32 ch = wxDecodeSurrogate(&src);
        if ( !src )
            return wxCONV_FAILED;

        outLen += BYTES_PER_CHAR;

        if ( outLen > dstLen )
            return wxCONV_FAILED;

        *outBuff++ = ch;
    }

    return outLen;
}

// ----------------------------------------------------------------------------
// endian-reversing conversions
// ----------------------------------------------------------------------------

size_t
wxMBConvUTF32swap::ToWChar(wchar_t *dst, size_t dstLen,
                           const char *src, size_t srcLen) const
{
    srcLen = GetLength(src, srcLen);
    if ( srcLen == wxNO_LEN )
        return wxCONV_FAILED;

    const wxUint32 *inBuff = reinterpret_cast<const wxUint32 *>(src);
    const size_t inLen = srcLen / BYTES_PER_CHAR;
    size_t outLen = 0;
    for ( size_t n = 0; n < inLen; n++, inBuff++ )
    {
        wxUint16 cc[2] = { 0 };
        const size_t numChars = encode_utf16(wxUINT32_SWAP_ALWAYS(*inBuff), cc);
        if ( numChars == wxCONV_FAILED )
            return wxCONV_FAILED;

        outLen += numChars;
        if ( dst )
        {
            if ( outLen > dstLen )
                return wxCONV_FAILED;

            *dst++ = cc[0];
            if ( numChars == 2 )
            {
                // second character of a surrogate
                *dst++ = cc[1];
            }
        }
    }

    return outLen;
}

size_t
wxMBConvUTF32swap::FromWChar(char *dst, size_t dstLen,
                             const wchar_t *src, size_t srcLen) const
{
    if ( srcLen == wxNO_LEN )
        srcLen = wxWcslen(src) + 1;

    if ( !dst )
    {
        // optimization: return maximal space which could be needed for this
        // string instead of the exact amount which could be less if there are
        // any surrogates in the input
        //
        // we consider that surrogates are rare enough to make it worthwhile to
        // avoid running the loop below at the cost of slightly extra memory
        // consumption
        return srcLen*BYTES_PER_CHAR;
    }

    wxUint32 *outBuff = reinterpret_cast<wxUint32 *>(dst);
    size_t outLen = 0;
    for ( const wchar_t * const srcEnd = src + srcLen; src < srcEnd; )
    {
        const wxUint32 ch = wxDecodeSurrogate(&src);
        if ( !src )
            return wxCONV_FAILED;

        outLen += BYTES_PER_CHAR;

        if ( outLen > dstLen )
            return wxCONV_FAILED;

        *outBuff++ = wxUINT32_SWAP_ALWAYS(ch);
    }

    return outLen;
}

#else // !WC_UTF16: wchar_t is UTF-32

// ----------------------------------------------------------------------------
// conversions without endianness change
// ----------------------------------------------------------------------------

size_t
wxMBConvUTF32straight::ToWChar(wchar_t *dst, size_t dstLen,
                               const char *src, size_t srcLen) const
{
    // use memcpy() as it should be much faster than hand-written loop
    srcLen = GetLength(src, srcLen);
    if ( srcLen == wxNO_LEN )
        return wxCONV_FAILED;

    const size_t inLen = srcLen/BYTES_PER_CHAR;
    if ( dst )
    {
        if ( dstLen < inLen )
            return wxCONV_FAILED;

        memcpy(dst, src, srcLen);
    }

    return inLen;
}

size_t
wxMBConvUTF32straight::FromWChar(char *dst, size_t dstLen,
                                 const wchar_t *src, size_t srcLen) const
{
    if ( srcLen == wxNO_LEN )
        srcLen = wxWcslen(src) + 1;

    srcLen *= BYTES_PER_CHAR;

    if ( dst )
    {
        if ( dstLen < srcLen )
            return wxCONV_FAILED;

        memcpy(dst, src, srcLen);
    }

    return srcLen;
}

// ----------------------------------------------------------------------------
// endian-reversing conversions
// ----------------------------------------------------------------------------

size_t
wxMBConvUTF32swap::ToWChar(wchar_t *dst, size_t dstLen,
                           const char *src, size_t srcLen) const
{
    srcLen = GetLength(src, srcLen);
    if ( srcLen == wxNO_LEN )
        return wxCONV_FAILED;

    srcLen /= BYTES_PER_CHAR;

    if ( dst )
    {
        if ( dstLen < srcLen )
            return wxCONV_FAILED;

        const wxUint32 *inBuff = reinterpret_cast<const wxUint32 *>(src);
        for ( size_t n = 0; n < srcLen; n++, inBuff++ )
        {
            *dst++ = wxUINT32_SWAP_ALWAYS(*inBuff);
        }
    }

    return srcLen;
}

size_t
wxMBConvUTF32swap::FromWChar(char *dst, size_t dstLen,
                             const wchar_t *src, size_t srcLen) const
{
    if ( srcLen == wxNO_LEN )
        srcLen = wxWcslen(src) + 1;

    srcLen *= BYTES_PER_CHAR;

    if ( dst )
    {
        if ( dstLen < srcLen )
            return wxCONV_FAILED;

        wxUint32 *outBuff = reinterpret_cast<wxUint32 *>(dst);
        for ( size_t n = 0; n < srcLen; n += BYTES_PER_CHAR, src++ )
        {
            *outBuff++ = wxUINT32_SWAP_ALWAYS(*src);
        }
    }

    return srcLen;
}

#endif // WC_UTF16/!WC_UTF16


// ============================================================================
// The classes doing conversion using the iconv_xxx() functions
// ============================================================================

#ifdef HAVE_ICONV

// VS: glibc 2.1.3 is broken in that iconv() conversion to/from UCS4 fails with
//     E2BIG if output buffer is _exactly_ as big as needed. Such case is
//     (unless there's yet another bug in glibc) the only case when iconv()
//     returns with (size_t)-1 (which means error) and says there are 0 bytes
//     left in the input buffer -- when _real_ error occurs,
//     bytes-left-in-input buffer is non-zero. Hence, this alternative test for
//     iconv() failure.
//     [This bug does not appear in glibc 2.2.]
#if defined(__GLIBC__) && __GLIBC__ == 2 && __GLIBC_MINOR__ <= 1
#define ICONV_FAILED(cres, bufLeft) ((cres == (size_t)-1) && \
                                     (errno != E2BIG || bufLeft != 0))
#else
#define ICONV_FAILED(cres, bufLeft)  (cres == (size_t)-1)
#endif

#define ICONV_CHAR_CAST(x)  ((ICONV_CONST char **)(x))

#define ICONV_T_INVALID ((iconv_t)-1)

#if SIZEOF_WCHAR_T == 4
    #define WC_BSWAP    wxUINT32_SWAP_ALWAYS
    #define WC_ENC      wxFONTENCODING_UTF32
#elif SIZEOF_WCHAR_T == 2
    #define WC_BSWAP    wxUINT16_SWAP_ALWAYS
    #define WC_ENC      wxFONTENCODING_UTF16
#else // sizeof(wchar_t) != 2 nor 4
    // does this ever happen?
    #error "Unknown sizeof(wchar_t): please report this to wx-dev@lists.wxwindows.org"
#endif

// ----------------------------------------------------------------------------
// wxMBConv_iconv: encapsulates an iconv character set
// ----------------------------------------------------------------------------

class wxMBConv_iconv : public wxMBConv
{
public:
    wxMBConv_iconv(const char *name);
    virtual ~wxMBConv_iconv();

    // implement base class virtual methods
    virtual size_t ToWChar(wchar_t *dst, size_t dstLen,
                           const char *src, size_t srcLen = wxNO_LEN) const;
    virtual size_t FromWChar(char *dst, size_t dstLen,
                             const wchar_t *src, size_t srcLen = wxNO_LEN) const;
    virtual size_t GetMBNulLen() const;

#if wxUSE_UNICODE_UTF8
    virtual bool IsUTF8() const;
#endif

    virtual wxMBConv *Clone() const
    {
        wxMBConv_iconv *p = new wxMBConv_iconv(m_name);
        p->m_minMBCharWidth = m_minMBCharWidth;
        return p;
    }

    bool IsOk() const
        { return (m2w != ICONV_T_INVALID) && (w2m != ICONV_T_INVALID); }

protected:
    // the iconv handlers used to translate from multibyte
    // to wide char and in the other direction
    iconv_t m2w,
            w2m;

#if wxUSE_THREADS
    // guards access to m2w and w2m objects
    wxMutex m_iconvMutex;
#endif

private:
    // the name (for iconv_open()) of a wide char charset -- if none is
    // available on this machine, it will remain NULL
    static wxString ms_wcCharsetName;

    // true if the wide char encoding we use (i.e. ms_wcCharsetName) has
    // different endian-ness than the native one
    static bool ms_wcNeedsSwap;


    // name of the encoding handled by this conversion
    const char *m_name;

    // cached result of GetMBNulLen(); set to 0 meaning "unknown"
    // initially
    size_t m_minMBCharWidth;
};

// make the constructor available for unit testing
WXDLLIMPEXP_BASE wxMBConv* new_wxMBConv_iconv( const char* name )
{
    wxMBConv_iconv* result = new wxMBConv_iconv( name );
    if ( !result->IsOk() )
    {
        delete result;
        return 0;
    }

    return result;
}

wxString wxMBConv_iconv::ms_wcCharsetName;
bool wxMBConv_iconv::ms_wcNeedsSwap = false;

wxMBConv_iconv::wxMBConv_iconv(const char *name)
              : m_name(wxStrdup(name))
{
    m_minMBCharWidth = 0;

    // check for charset that represents wchar_t:
    if ( ms_wcCharsetName.empty() )
    {
        wxLogTrace(TRACE_STRCONV, wxT("Looking for wide char codeset:"));

#if wxUSE_FONTMAP
        const wxChar *const *names = wxFontMapperBase::GetAllEncodingNames(WC_ENC);
#else // !wxUSE_FONTMAP
        static const wxChar *const names_static[] =
        {
#if SIZEOF_WCHAR_T == 4
            wxT("UCS-4"),
#elif SIZEOF_WCHAR_T == 2
            wxT("UCS-2"),
#endif
            NULL
        };
        const wxChar *const *names = names_static;
#endif // wxUSE_FONTMAP/!wxUSE_FONTMAP

        for ( ; *names && ms_wcCharsetName.empty(); ++names )
        {
            const wxString nameCS(*names);

            // first try charset with explicit bytesex info (e.g. "UCS-4LE"):
            wxString nameXE(nameCS);

#ifdef WORDS_BIGENDIAN
                nameXE += wxT("BE");
#else // little endian
                nameXE += wxT("LE");
#endif

            wxLogTrace(TRACE_STRCONV, wxT("  trying charset \"%s\""),
                       nameXE.c_str());

            m2w = iconv_open(nameXE.ToAscii(), name);
            if ( m2w == ICONV_T_INVALID )
            {
                // try charset w/o bytesex info (e.g. "UCS4")
                wxLogTrace(TRACE_STRCONV, wxT("  trying charset \"%s\""),
                           nameCS.c_str());
                m2w = iconv_open(nameCS.ToAscii(), name);

                // and check for bytesex ourselves:
                if ( m2w != ICONV_T_INVALID )
                {
                    char    buf[2], *bufPtr;
                    wchar_t wbuf[2];
                    size_t  insz, outsz;
                    size_t  res;

                    buf[0] = 'A';
                    buf[1] = 0;
                    wbuf[0] = 0;
                    insz = 2;
                    outsz = SIZEOF_WCHAR_T * 2;
                    char* wbufPtr = (char*)wbuf;
                    bufPtr = buf;

                    res = iconv(
                        m2w, ICONV_CHAR_CAST(&bufPtr), &insz,
                        &wbufPtr, &outsz);

                    if (ICONV_FAILED(res, insz))
                    {
                        wxLogLastError(wxT("iconv"));
                        wxLogError(_("Conversion to charset '%s' doesn't work."),
                                   nameCS.c_str());
                    }
                    else // ok, can convert to this encoding, remember it
                    {
                        ms_wcCharsetName = nameCS;
                        ms_wcNeedsSwap = wbuf[0] != (wchar_t)buf[0];
                    }
                }
            }
            else // use charset not requiring byte swapping
            {
                ms_wcCharsetName = nameXE;
            }
        }

        wxLogTrace(TRACE_STRCONV,
                   wxT("iconv wchar_t charset is \"%s\"%s"),
                   ms_wcCharsetName.empty() ? wxString("<none>")
                                            : ms_wcCharsetName,
                   ms_wcNeedsSwap ? wxT(" (needs swap)")
                                  : wxT(""));
    }
    else // we already have ms_wcCharsetName
    {
        m2w = iconv_open(ms_wcCharsetName.ToAscii(), name);
    }

    if ( ms_wcCharsetName.empty() )
    {
        w2m = ICONV_T_INVALID;
    }
    else
    {
        w2m = iconv_open(name, ms_wcCharsetName.ToAscii());
        if ( w2m == ICONV_T_INVALID )
        {
            wxLogTrace(TRACE_STRCONV,
                       wxT("\"%s\" -> \"%s\" works but not the converse!?"),
                       ms_wcCharsetName.c_str(), name);
        }
    }
}

wxMBConv_iconv::~wxMBConv_iconv()
{
    free(const_cast<char *>(m_name));

    if ( m2w != ICONV_T_INVALID )
        iconv_close(m2w);
    if ( w2m != ICONV_T_INVALID )
        iconv_close(w2m);
}

size_t
wxMBConv_iconv::ToWChar(wchar_t *dst, size_t dstLen,
                        const char *src, size_t srcLen) const
{
    if ( srcLen == wxNO_LEN )
    {
        // find the string length: notice that must be done differently for
        // NUL-terminated strings and UTF-16/32 which are terminated with 2/4
        // consecutive NULs
        const size_t nulLen = GetMBNulLen();
        switch ( nulLen )
        {
            default:
                return wxCONV_FAILED;

            case 1:
                srcLen = strlen(src); // arguably more optimized than our version
                break;

            case 2:
            case 4:
                // for UTF-16/32 not only we need to have 2/4 consecutive NULs
                // but they also have to start at character boundary and not
                // span two adjacent characters
                const char *p;
                for ( p = src; NotAllNULs(p, nulLen); p += nulLen )
                    ;
                srcLen = p - src;
                break;
        }

        // when we're determining the length of the string ourselves we count
        // the terminating NUL(s) as part of it and always NUL-terminate the
        // output
        srcLen += nulLen;
    }

    // we express length in the number of (wide) characters but iconv always
    // counts buffer sizes it in bytes
    dstLen *= SIZEOF_WCHAR_T;

#if wxUSE_THREADS
    // NB: iconv() is MT-safe, but each thread must use its own iconv_t handle.
    //     Unfortunately there are a couple of global wxCSConv objects such as
    //     wxConvLocal that are used all over wx code, so we have to make sure
    //     the handle is used by at most one thread at the time. Otherwise
    //     only a few wx classes would be safe to use from non-main threads
    //     as MB<->WC conversion would fail "randomly".
    wxMutexLocker lock(wxConstCast(this, wxMBConv_iconv)->m_iconvMutex);
#endif // wxUSE_THREADS

    size_t res, cres;
    const char *pszPtr = src;

    if ( dst )
    {
        char* bufPtr = (char*)dst;

        // have destination buffer, convert there
        size_t dstLenOrig = dstLen;
        cres = iconv(m2w,
                     ICONV_CHAR_CAST(&pszPtr), &srcLen,
                     &bufPtr, &dstLen);

        // convert the number of bytes converted as returned by iconv to the
        // number of (wide) characters converted that we need
        res = (dstLenOrig - dstLen) / SIZEOF_WCHAR_T;

        if (ms_wcNeedsSwap)
        {
            // convert to native endianness
            for ( unsigned i = 0; i < res; i++ )
                dst[i] = WC_BSWAP(dst[i]);
        }
    }
    else // no destination buffer
    {
        // convert using temp buffer to calculate the size of the buffer needed
        wchar_t tbuf[256];
        res = 0;

        do
        {
            char* bufPtr = (char*)tbuf;
            dstLen = 8 * SIZEOF_WCHAR_T;

            cres = iconv(m2w,
                         ICONV_CHAR_CAST(&pszPtr), &srcLen,
                         &bufPtr, &dstLen );

            res += 8 - (dstLen / SIZEOF_WCHAR_T);
        }
        while ((cres == (size_t)-1) && (errno == E2BIG));
    }

    if (ICONV_FAILED(cres, srcLen))
    {
        //VS: it is ok if iconv fails, hence trace only
        wxLogTrace(TRACE_STRCONV, wxT("iconv failed: %s"), wxSysErrorMsg(wxSysErrorCode()));
        return wxCONV_FAILED;
    }

    return res;
}

size_t wxMBConv_iconv::FromWChar(char *dst, size_t dstLen,
                                 const wchar_t *src, size_t srcLen) const
{
#if wxUSE_THREADS
    // NB: explained in MB2WC
    wxMutexLocker lock(wxConstCast(this, wxMBConv_iconv)->m_iconvMutex);
#endif

    if ( srcLen == wxNO_LEN )
        srcLen = wxWcslen(src) + 1;

    size_t inbuflen = srcLen * SIZEOF_WCHAR_T;
    size_t outbuflen = dstLen;
    size_t res, cres;

    wchar_t *tmpbuf = 0;

    if (ms_wcNeedsSwap)
    {
        // need to copy to temp buffer to switch endianness
        // (doing WC_BSWAP twice on the original buffer won't work, as it
        //  could be in read-only memory, or be accessed in some other thread)
        tmpbuf = (wchar_t *)malloc(inbuflen);
        for ( size_t i = 0; i < srcLen; i++ )
            tmpbuf[i] = WC_BSWAP(src[i]);

        src = tmpbuf;
    }

    char* inbuf = (char*)src;
    if ( dst )
    {
        // have destination buffer, convert there
        cres = iconv(w2m, ICONV_CHAR_CAST(&inbuf), &inbuflen, &dst, &outbuflen);

        res = dstLen - outbuflen;
    }
    else // no destination buffer
    {
        // convert using temp buffer to calculate the size of the buffer needed
        char tbuf[256];
        res = 0;
        do
        {
            dst = tbuf;
            outbuflen = WXSIZEOF(tbuf);

            cres = iconv(w2m, ICONV_CHAR_CAST(&inbuf), &inbuflen, &dst, &outbuflen);

            res += WXSIZEOF(tbuf) - outbuflen;
        }
        while ((cres == (size_t)-1) && (errno == E2BIG));
    }

    if (ms_wcNeedsSwap)
    {
        free(tmpbuf);
    }

    if (ICONV_FAILED(cres, inbuflen))
    {
        wxLogTrace(TRACE_STRCONV, wxT("iconv failed: %s"), wxSysErrorMsg(wxSysErrorCode()));
        return wxCONV_FAILED;
    }

    return res;
}

size_t wxMBConv_iconv::GetMBNulLen() const
{
    if ( m_minMBCharWidth == 0 )
    {
        wxMBConv_iconv * const self = wxConstCast(this, wxMBConv_iconv);

#if wxUSE_THREADS
        // NB: explained in MB2WC
        wxMutexLocker lock(self->m_iconvMutex);
#endif

        const wchar_t *wnul = L"";
        char buf[8]; // should be enough for NUL in any encoding
        size_t inLen = sizeof(wchar_t),
               outLen = WXSIZEOF(buf);
        char *inBuff = (char *)wnul;
        char *outBuff = buf;
        if ( iconv(w2m, ICONV_CHAR_CAST(&inBuff), &inLen, &outBuff, &outLen) == (size_t)-1 )
        {
            self->m_minMBCharWidth = (size_t)-1;
        }
        else // ok
        {
            self->m_minMBCharWidth = outBuff - buf;
        }
    }

    return m_minMBCharWidth;
}

#if wxUSE_UNICODE_UTF8
bool wxMBConv_iconv::IsUTF8() const
{
    return wxStricmp(m_name, "UTF-8") == 0 ||
           wxStricmp(m_name, "UTF8") == 0;
}
#endif

#endif // HAVE_ICONV


// ============================================================================
// Win32 conversion classes
// ============================================================================

#ifdef wxHAVE_WIN32_MB2WC

// from utils.cpp
#if wxUSE_FONTMAP
extern WXDLLIMPEXP_BASE long wxCharsetToCodepage(const char *charset);
extern WXDLLIMPEXP_BASE long wxEncodingToCodepage(wxFontEncoding encoding);
#endif

class wxMBConv_win32 : public wxMBConv
{
public:
    wxMBConv_win32()
    {
        m_CodePage = CP_ACP;
        m_minMBCharWidth = 0;
    }

    wxMBConv_win32(const wxMBConv_win32& conv)
        : wxMBConv()
    {
        m_CodePage = conv.m_CodePage;
        m_minMBCharWidth = conv.m_minMBCharWidth;
    }

#if wxUSE_FONTMAP
    wxMBConv_win32(const char* name)
    {
        m_CodePage = wxCharsetToCodepage(name);
        m_minMBCharWidth = 0;
    }

    wxMBConv_win32(wxFontEncoding encoding)
    {
        m_CodePage = wxEncodingToCodepage(encoding);
        m_minMBCharWidth = 0;
    }
#endif // wxUSE_FONTMAP

    virtual size_t MB2WC(wchar_t *buf, const char *psz, size_t n) const
    {
        // note that we have to use MB_ERR_INVALID_CHARS flag as it without it
        // the behaviour is not compatible with the Unix version (using iconv)
        // and break the library itself, e.g. wxTextInputStream::NextChar()
        // wouldn't work if reading an incomplete MB char didn't result in an
        // error
        //
        // Moreover, MB_ERR_INVALID_CHARS is only supported on Win 2K SP4 or
        // Win XP or newer and it is not supported for UTF-[78] so we always
        // use our own conversions in this case. See
        //     http://blogs.msdn.com/michkap/archive/2005/04/19/409566.aspx
        //     http://msdn.microsoft.com/library/en-us/intl/unicode_17si.asp
        if ( m_CodePage == CP_UTF8 )
        {
            return wxMBConvUTF8().MB2WC(buf, psz, n);
        }

        if ( m_CodePage == CP_UTF7 )
        {
            return wxMBConvUTF7().MB2WC(buf, psz, n);
        }

        int flags = 0;
        if ( (m_CodePage < 50000 && m_CodePage != CP_SYMBOL) &&
                IsAtLeastWin2kSP4() )
        {
            flags = MB_ERR_INVALID_CHARS;
        }

        const size_t len = ::MultiByteToWideChar
                             (
                                m_CodePage,     // code page
                                flags,          // flags: fall on error
                                psz,            // input string
                                -1,             // its length (NUL-terminated)
                                buf,            // output string
                                buf ? n : 0     // size of output buffer
                             );
        if ( !len )
        {
            // function totally failed
            return wxCONV_FAILED;
        }

        // if we were really converting and didn't use MB_ERR_INVALID_CHARS,
        // check if we succeeded, by doing a double trip:
        if ( !flags && buf )
        {
            const size_t mbLen = strlen(psz);
            wxCharBuffer mbBuf(mbLen);
            if ( ::WideCharToMultiByte
                   (
                      m_CodePage,
                      0,
                      buf,
                      -1,
                      mbBuf.data(),
                      mbLen + 1,        // size in bytes, not length
                      NULL,
                      NULL
                   ) == 0 ||
                  strcmp(mbBuf, psz) != 0 )
            {
                // we didn't obtain the same thing we started from, hence
                // the conversion was lossy and we consider that it failed
                return wxCONV_FAILED;
            }
        }

        // note that it returns count of written chars for buf != NULL and size
        // of the needed buffer for buf == NULL so in either case the length of
        // the string (which never includes the terminating NUL) is one less
        return len - 1;
    }

    virtual size_t WC2MB(char *buf, const wchar_t *pwz, size_t n) const
    {
        /*
            we have a problem here: by default, WideCharToMultiByte() may
            replace characters unrepresentable in the target code page with bad
            quality approximations such as turning "1/2" symbol (U+00BD) into
            "1" for the code pages which don't have it and we, obviously, want
            to avoid this at any price

            the trouble is that this function does it _silently_, i.e. it won't
            even tell us whether it did or not... Win98/2000 and higher provide
            WC_NO_BEST_FIT_CHARS but it doesn't work for the older systems and
            we have to resort to a round trip, i.e. check that converting back
            results in the same string -- this is, of course, expensive but
            otherwise we simply can't be sure to not garble the data.
         */

        // determine if we can rely on WC_NO_BEST_FIT_CHARS: according to MSDN
        // it doesn't work with CJK encodings (which we test for rather roughly
        // here...) nor with UTF-7/8 nor, of course, with Windows versions not
        // supporting it
        BOOL usedDef wxDUMMY_INITIALIZE(false);
        BOOL *pUsedDef;
        int flags;
        if ( CanUseNoBestFit() && m_CodePage < 50000 )
        {
            // it's our lucky day
            flags = WC_NO_BEST_FIT_CHARS;
            pUsedDef = &usedDef;
        }
        else // old system or unsupported encoding
        {
            flags = 0;
            pUsedDef = NULL;
        }

        const size_t len = ::WideCharToMultiByte
                             (
                                m_CodePage,     // code page
                                flags,          // either none or no best fit
                                pwz,            // input string
                                -1,             // it is (wide) NUL-terminated
                                buf,            // output buffer
                                buf ? n : 0,    // and its size
                                NULL,           // default "replacement" char
                                pUsedDef        // [out] was it used?
                             );

        if ( !len )
        {
            // function totally failed
            return wxCONV_FAILED;
        }

        // we did something, check if we really succeeded
        if ( flags )
        {
            // check if the conversion failed, i.e. if any replacements
            // were done
            if ( usedDef )
                return wxCONV_FAILED;
        }
        else // we must resort to double tripping...
        {
            // first we need to ensure that we really have the MB data: this is
            // not the case if we're called with NULL buffer, in which case we
            // need to do the conversion yet again
            wxCharBuffer bufDef;
            if ( !buf )
            {
                bufDef = wxCharBuffer(len);
                buf = bufDef.data();
                if ( !::WideCharToMultiByte(m_CodePage, flags, pwz, -1,
                                            buf, len, NULL, NULL) )
                    return wxCONV_FAILED;
            }

            if ( !n )
                n = wcslen(pwz);
            wxWCharBuffer wcBuf(n);
            if ( MB2WC(wcBuf.data(), buf, n + 1) == wxCONV_FAILED ||
                    wcscmp(wcBuf, pwz) != 0 )
            {
                // we didn't obtain the same thing we started from, hence
                // the conversion was lossy and we consider that it failed
                return wxCONV_FAILED;
            }
        }

        // see the comment above for the reason of "len - 1"
        return len - 1;
    }

    virtual size_t GetMBNulLen() const
    {
        if ( m_minMBCharWidth == 0 )
        {
            int len = ::WideCharToMultiByte
                        (
                            m_CodePage,     // code page
                            0,              // no flags
                            L"",            // input string
                            1,              // translate just the NUL
                            NULL,           // output buffer
                            0,              // and its size
                            NULL,           // no replacement char
                            NULL            // [out] don't care if it was used
                        );

            wxMBConv_win32 * const self = wxConstCast(this, wxMBConv_win32);
            switch ( len )
            {
                default:
                    wxLogDebug(wxT("Unexpected NUL length %d"), len);
                    self->m_minMBCharWidth = (size_t)-1;
                    break;

                case 0:
                    self->m_minMBCharWidth = (size_t)-1;
                    break;

                case 1:
                case 2:
                case 4:
                    self->m_minMBCharWidth = len;
                    break;
            }
        }

        return m_minMBCharWidth;
    }

    virtual wxMBConv *Clone() const { return new wxMBConv_win32(*this); }

    bool IsOk() const { return m_CodePage != -1; }

private:
    static bool CanUseNoBestFit()
    {
        static int s_isWin98Or2k = -1;

        if ( s_isWin98Or2k == -1 )
        {
            int verMaj, verMin;
            switch ( wxGetOsVersion(&verMaj, &verMin) )
            {
                case wxOS_WINDOWS_9X:
                    s_isWin98Or2k = verMaj >= 4 && verMin >= 10;
                    break;

                case wxOS_WINDOWS_NT:
                    s_isWin98Or2k = verMaj >= 5;
                    break;

                default:
                    // unknown: be conservative by default
                    s_isWin98Or2k = 0;
                    break;
            }

            wxASSERT_MSG( s_isWin98Or2k != -1, wxT("should be set above") );
        }

        return s_isWin98Or2k == 1;
    }

    static bool IsAtLeastWin2kSP4()
    {
#ifdef __WXWINCE__
        return false;
#else
        static int s_isAtLeastWin2kSP4 = -1;

        if ( s_isAtLeastWin2kSP4 == -1 )
        {
            OSVERSIONINFOEX ver;

            memset(&ver, 0, sizeof(ver));
            ver.dwOSVersionInfoSize = sizeof(ver);
            GetVersionEx((OSVERSIONINFO*)&ver);

            s_isAtLeastWin2kSP4 =
              ((ver.dwMajorVersion > 5) || // Vista+
               (ver.dwMajorVersion == 5 && ver.dwMinorVersion > 0) || // XP/2003
               (ver.dwMajorVersion == 5 && ver.dwMinorVersion == 0 &&
               ver.wServicePackMajor >= 4)) // 2000 SP4+
              ? 1 : 0;
        }

        return s_isAtLeastWin2kSP4 == 1;
#endif
    }


    // the code page we're working with
    long m_CodePage;

    // cached result of GetMBNulLen(), set to 0 initially meaning
    // "unknown"
    size_t m_minMBCharWidth;
};

#endif // wxHAVE_WIN32_MB2WC


// ============================================================================
// wxEncodingConverter based conversion classes
// ============================================================================

#if wxUSE_FONTMAP

class wxMBConv_wxwin : public wxMBConv
{
private:
    void Init()
    {
        // Refuse to use broken wxEncodingConverter code for Mac-specific encodings.
        // The wxMBConv_cf class does a better job.
        m_ok = (m_enc < wxFONTENCODING_MACMIN || m_enc > wxFONTENCODING_MACMAX) &&
               m2w.Init(m_enc, wxFONTENCODING_UNICODE) &&
               w2m.Init(wxFONTENCODING_UNICODE, m_enc);
    }

public:
    // temporarily just use wxEncodingConverter stuff,
    // so that it works while a better implementation is built
    wxMBConv_wxwin(const char* name)
    {
        if (name)
            m_enc = wxFontMapperBase::Get()->CharsetToEncoding(name, false);
        else
            m_enc = wxFONTENCODING_SYSTEM;

        Init();
    }

    wxMBConv_wxwin(wxFontEncoding enc)
    {
        m_enc = enc;

        Init();
    }

    size_t MB2WC(wchar_t *buf, const char *psz, size_t WXUNUSED(n)) const
    {
        size_t inbuf = strlen(psz);
        if (buf)
        {
            if (!m2w.Convert(psz, buf))
                return wxCONV_FAILED;
        }
        return inbuf;
    }

    size_t WC2MB(char *buf, const wchar_t *psz, size_t WXUNUSED(n)) const
    {
        const size_t inbuf = wxWcslen(psz);
        if (buf)
        {
            if (!w2m.Convert(psz, buf))
                return wxCONV_FAILED;
        }

        return inbuf;
    }

    virtual size_t GetMBNulLen() const
    {
        switch ( m_enc )
        {
            case wxFONTENCODING_UTF16BE:
            case wxFONTENCODING_UTF16LE:
                return 2;

            case wxFONTENCODING_UTF32BE:
            case wxFONTENCODING_UTF32LE:
                return 4;

            default:
                return 1;
        }
    }

    virtual wxMBConv *Clone() const { return new wxMBConv_wxwin(m_enc); }

    bool IsOk() const { return m_ok; }

public:
    wxFontEncoding m_enc;
    wxEncodingConverter m2w, w2m;

private:
    // were we initialized successfully?
    bool m_ok;

    wxDECLARE_NO_COPY_CLASS(wxMBConv_wxwin);
};

// make the constructors available for unit testing
WXDLLIMPEXP_BASE wxMBConv* new_wxMBConv_wxwin( const char* name )
{
    wxMBConv_wxwin* result = new wxMBConv_wxwin( name );
    if ( !result->IsOk() )
    {
        delete result;
        return 0;
    }

    return result;
}

#endif // wxUSE_FONTMAP

// ============================================================================
// wxCSConv implementation
// ============================================================================

void wxCSConv::Init()
{
    m_name = NULL;
    m_convReal =  NULL;
}

void wxCSConv::SetEncoding(wxFontEncoding encoding)
{
    switch ( encoding )
    {
        case wxFONTENCODING_MAX:
        case wxFONTENCODING_SYSTEM:
            if ( m_name )
            {
                // It's ok to not have encoding value if we have a name for it.
                m_encoding = wxFONTENCODING_SYSTEM;
            }
            else // No name neither.
            {
                // Fall back to the system default encoding in this case (not
                // sure how much sense does this make but this is how the old
                // code used to behave).
#if wxUSE_INTL
                m_encoding = wxLocale::GetSystemEncoding();
                if ( m_encoding == wxFONTENCODING_SYSTEM )
#endif // wxUSE_INTL
                    m_encoding = wxFONTENCODING_ISO8859_1;
            }
            break;

        case wxFONTENCODING_DEFAULT:
            // wxFONTENCODING_DEFAULT is same as US-ASCII in this context
            m_encoding = wxFONTENCODING_ISO8859_1;
            break;

        default:
            // Just use the provided encoding.
            m_encoding = encoding;
    }
}

wxCSConv::wxCSConv(const wxString& charset)
{
    Init();

    if ( !charset.empty() )
    {
        SetName(charset.ToAscii());
    }

#if wxUSE_FONTMAP
    SetEncoding(wxFontMapperBase::GetEncodingFromName(charset));
#else
    SetEncoding(wxFONTENCODING_SYSTEM);
#endif

    m_convReal = DoCreate();
}

wxCSConv::wxCSConv(wxFontEncoding encoding)
{
    if ( encoding == wxFONTENCODING_MAX || encoding == wxFONTENCODING_DEFAULT )
    {
        wxFAIL_MSG( wxT("invalid encoding value in wxCSConv ctor") );

        encoding = wxFONTENCODING_SYSTEM;
    }

    Init();

    SetEncoding(encoding);

    m_convReal = DoCreate();
}

wxCSConv::~wxCSConv()
{
    Clear();
}

wxCSConv::wxCSConv(const wxCSConv& conv)
        : wxMBConv()
{
    Init();

    SetName(conv.m_name);
    SetEncoding(conv.m_encoding);

    m_convReal = DoCreate();
}

wxCSConv& wxCSConv::operator=(const wxCSConv& conv)
{
    Clear();

    SetName(conv.m_name);
    SetEncoding(conv.m_encoding);

    m_convReal = DoCreate();

    return *this;
}

void wxCSConv::Clear()
{
    free(m_name);
    m_name = NULL;

    wxDELETE(m_convReal);
}

void wxCSConv::SetName(const char *charset)
{
    if ( charset )
        m_name = wxStrdup(charset);
}

#if wxUSE_FONTMAP

WX_DECLARE_HASH_MAP( wxFontEncoding, wxString, wxIntegerHash, wxIntegerEqual,
                     wxEncodingNameCache );

static wxEncodingNameCache gs_nameCache;
#endif

wxMBConv *wxCSConv::DoCreate() const
{
#if wxUSE_FONTMAP
    wxLogTrace(TRACE_STRCONV,
               wxT("creating conversion for %s"),
               (m_name ? m_name
                       : (const char*)wxFontMapperBase::GetEncodingName(m_encoding).mb_str()));
#endif // wxUSE_FONTMAP

    // check for the special case of ASCII or ISO8859-1 charset: as we have
    // special knowledge of it anyhow, we don't need to create a special
    // conversion object
    if ( m_encoding == wxFONTENCODING_ISO8859_1 )
    {
        // don't convert at all
        return NULL;
    }

    // we trust OS to do conversion better than we can so try external
    // conversion methods first
    //
    // the full order is:
    //      1. OS conversion (iconv() under Unix or Win32 API)
    //      2. hard coded conversions for UTF
    //      3. wxEncodingConverter as fall back

    // step (1)
#ifdef HAVE_ICONV
#if !wxUSE_FONTMAP
    if ( m_name )
#endif // !wxUSE_FONTMAP
    {
#if wxUSE_FONTMAP
        wxFontEncoding encoding(m_encoding);
#endif

        if ( m_name )
        {
            wxMBConv_iconv *conv = new wxMBConv_iconv(m_name);
            if ( conv->IsOk() )
                return conv;

            delete conv;

#if wxUSE_FONTMAP
            encoding =
                wxFontMapperBase::Get()->CharsetToEncoding(m_name, false);
#endif // wxUSE_FONTMAP
        }
#if wxUSE_FONTMAP
        {
            const wxEncodingNameCache::iterator it = gs_nameCache.find(encoding);
            if ( it != gs_nameCache.end() )
            {
                if ( it->second.empty() )
                    return NULL;

                wxMBConv_iconv *conv = new wxMBConv_iconv(it->second.ToAscii());
                if ( conv->IsOk() )
                    return conv;

                delete conv;
            }

            const wxChar* const* names = wxFontMapperBase::GetAllEncodingNames(encoding);
            // CS : in case this does not return valid names (eg for MacRoman)
            // encoding got a 'failure' entry in the cache all the same,
            // although it just has to be created using a different method, so
            // only store failed iconv creation attempts (or perhaps we
            // shoulnd't do this at all ?)
            if ( names[0] != NULL )
            {
                for ( ; *names; ++names )
                {
                    // FIXME-UTF8: wxFontMapperBase::GetAllEncodingNames()
                    //             will need changes that will obsolete this
                    wxString name(*names);
                    wxMBConv_iconv *conv = new wxMBConv_iconv(name.ToAscii());
                    if ( conv->IsOk() )
                    {
                        gs_nameCache[encoding] = *names;
                        return conv;
                    }

                    delete conv;
                }

                gs_nameCache[encoding] = wxT(""); // cache the failure
            }
        }
#endif // wxUSE_FONTMAP
    }
#endif // HAVE_ICONV

#ifdef wxHAVE_WIN32_MB2WC
    {
#if wxUSE_FONTMAP
        wxMBConv_win32 *conv = m_name ? new wxMBConv_win32(m_name)
                                      : new wxMBConv_win32(m_encoding);
        if ( conv->IsOk() )
            return conv;

        delete conv;
#else
        return NULL;
#endif
    }
#endif // wxHAVE_WIN32_MB2WC

#ifdef __DARWIN__
    {
        // leave UTF16 and UTF32 to the built-ins of wx
        if ( m_name || ( m_encoding < wxFONTENCODING_UTF16BE ||
            ( m_encoding >= wxFONTENCODING_MACMIN && m_encoding <= wxFONTENCODING_MACMAX ) ) )
        {
#if wxUSE_FONTMAP
            wxMBConv_cf *conv = m_name ? new wxMBConv_cf(m_name)
                                          : new wxMBConv_cf(m_encoding);
#else
            wxMBConv_cf *conv = new wxMBConv_cf(m_encoding);
#endif

            if ( conv->IsOk() )
                 return conv;

            delete conv;
        }
    }
#endif // __DARWIN__

    // step (2)
    wxFontEncoding enc = m_encoding;
#if wxUSE_FONTMAP
    if ( enc == wxFONTENCODING_SYSTEM && m_name )
    {
        // use "false" to suppress interactive dialogs -- we can be called from
        // anywhere and popping up a dialog from here is the last thing we want to
        // do
        enc = wxFontMapperBase::Get()->CharsetToEncoding(m_name, false);
    }
#endif // wxUSE_FONTMAP

    switch ( enc )
    {
        case wxFONTENCODING_UTF7:
             return new wxMBConvUTF7;

        case wxFONTENCODING_UTF8:
             return new wxMBConvUTF8;

        case wxFONTENCODING_UTF16BE:
             return new wxMBConvUTF16BE;

        case wxFONTENCODING_UTF16LE:
             return new wxMBConvUTF16LE;

        case wxFONTENCODING_UTF32BE:
             return new wxMBConvUTF32BE;

        case wxFONTENCODING_UTF32LE:
             return new wxMBConvUTF32LE;

        default:
             // nothing to do but put here to suppress gcc warnings
             break;
    }

    // step (3)
#if wxUSE_FONTMAP
    {
        wxMBConv_wxwin *conv = m_name ? new wxMBConv_wxwin(m_name)
                                      : new wxMBConv_wxwin(m_encoding);
        if ( conv->IsOk() )
            return conv;

        delete conv;
    }

    wxLogTrace(TRACE_STRCONV,
               wxT("encoding \"%s\" is not supported by this system"),
               (m_name ? wxString(m_name)
                       : wxFontMapperBase::GetEncodingName(m_encoding)));
#endif // wxUSE_FONTMAP

    return NULL;
}

bool wxCSConv::IsOk() const
{
    // special case: no convReal created for wxFONTENCODING_ISO8859_1
    if ( m_encoding == wxFONTENCODING_ISO8859_1 )
        return true; // always ok as we do it ourselves

    // m_convReal->IsOk() is called at its own creation, so we know it must
    // be ok if m_convReal is non-NULL
    return m_convReal != NULL;
}

size_t wxCSConv::ToWChar(wchar_t *dst, size_t dstLen,
                         const char *src, size_t srcLen) const
{
    if (m_convReal)
        return m_convReal->ToWChar(dst, dstLen, src, srcLen);

    // latin-1 (direct)
    if ( srcLen == wxNO_LEN )
        srcLen = strlen(src) + 1; // take trailing NUL too

    if ( dst )
    {
        if ( dstLen < srcLen )
            return wxCONV_FAILED;

        for ( size_t n = 0; n < srcLen; n++ )
            dst[n] = (unsigned char)(src[n]);
    }

    return srcLen;
}

size_t wxCSConv::FromWChar(char *dst, size_t dstLen,
                           const wchar_t *src, size_t srcLen) const
{
    if (m_convReal)
        return m_convReal->FromWChar(dst, dstLen, src, srcLen);

    // latin-1 (direct)
    if ( srcLen == wxNO_LEN )
        srcLen = wxWcslen(src) + 1;

    if ( dst )
    {
        if ( dstLen < srcLen )
            return wxCONV_FAILED;

        for ( size_t n = 0; n < srcLen; n++ )
        {
            if ( src[n] > 0xFF )
                return wxCONV_FAILED;

            dst[n] = (char)src[n];
        }

    }
    else // still need to check the input validity
    {
        for ( size_t n = 0; n < srcLen; n++ )
        {
            if ( src[n] > 0xFF )
                return wxCONV_FAILED;
        }
    }

    return srcLen;
}

size_t wxCSConv::GetMBNulLen() const
{
    if ( m_convReal )
        return m_convReal->GetMBNulLen();

    // otherwise, we are ISO-8859-1
    return 1;
}

#if wxUSE_UNICODE_UTF8
bool wxCSConv::IsUTF8() const
{
    if ( m_convReal )
        return m_convReal->IsUTF8();

    // otherwise, we are ISO-8859-1
    return false;
}
#endif


#if wxUSE_UNICODE

wxWCharBuffer wxSafeConvertMB2WX(const char *s)
{
    if ( !s )
        return wxWCharBuffer();

    wxWCharBuffer wbuf(wxConvLibc.cMB2WX(s));
    if ( !wbuf )
        wbuf = wxMBConvUTF8().cMB2WX(s);
    if ( !wbuf )
        wbuf = wxConvISO8859_1.cMB2WX(s);

    return wbuf;
}

wxCharBuffer wxSafeConvertWX2MB(const wchar_t *ws)
{
    if ( !ws )
        return wxCharBuffer();

    wxCharBuffer buf(wxConvLibc.cWX2MB(ws));
    if ( !buf )
        buf = wxMBConvUTF8(wxMBConvUTF8::MAP_INVALID_UTF8_TO_OCTAL).cWX2MB(ws);

    return buf;
}

#endif // wxUSE_UNICODE

// ----------------------------------------------------------------------------
// globals
// ----------------------------------------------------------------------------

// NB: The reason why we create converted objects in this convoluted way,
//     using a factory function instead of global variable, is that they
//     may be used at static initialization time (some of them are used by
//     wxString ctors and there may be a global wxString object). In other
//     words, possibly _before_ the converter global object would be
//     initialized.

#undef wxConvLibc
#undef wxConvUTF8
#undef wxConvUTF7
#undef wxConvLocal
#undef wxConvISO8859_1

#define WX_DEFINE_GLOBAL_CONV2(klass, impl_klass, name, ctor_args)      \
    WXDLLIMPEXP_DATA_BASE(klass*) name##Ptr = NULL;                     \
    WXDLLIMPEXP_BASE klass* wxGet_##name##Ptr()                         \
    {                                                                   \
        static impl_klass name##Obj ctor_args;                          \
        return &name##Obj;                                              \
    }                                                                   \
    /* this ensures that all global converter objects are created */    \
    /* by the time static initialization is done, i.e. before any */    \
    /* thread is launched: */                                           \
    static klass* gs_##name##instance = wxGet_##name##Ptr()

#define WX_DEFINE_GLOBAL_CONV(klass, name, ctor_args) \
    WX_DEFINE_GLOBAL_CONV2(klass, klass, name, ctor_args)

#ifdef __INTELC__
    // disable warning "variable 'xxx' was declared but never referenced"
    #pragma warning(disable: 177)
#endif // Intel C++

#ifdef __WINDOWS__
    WX_DEFINE_GLOBAL_CONV2(wxMBConv, wxMBConv_win32, wxConvLibc, wxEMPTY_PARAMETER_VALUE);
#elif 0 // defined(__WXOSX__)
    WX_DEFINE_GLOBAL_CONV2(wxMBConv, wxMBConv_cf, wxConvLibc,  (wxFONTENCODING_UTF8));
#else
    WX_DEFINE_GLOBAL_CONV2(wxMBConv, wxMBConvLibc, wxConvLibc, wxEMPTY_PARAMETER_VALUE);
#endif

// NB: we can't use wxEMPTY_PARAMETER_VALUE as final argument here because it's
//     passed to WX_DEFINE_GLOBAL_CONV2 after a macro expansion and so still
//     provokes an error message about "not enough macro parameters"; and we
//     can't use "()" here as the name##Obj declaration would be parsed as a
//     function declaration then, so use a semicolon and live with an extra
//     empty statement (and hope that no compilers warns about this)
WX_DEFINE_GLOBAL_CONV(wxMBConvStrictUTF8, wxConvUTF8, ;);
WX_DEFINE_GLOBAL_CONV(wxMBConvUTF7, wxConvUTF7, ;);

WX_DEFINE_GLOBAL_CONV(wxCSConv, wxConvLocal, (wxFONTENCODING_SYSTEM));
WX_DEFINE_GLOBAL_CONV(wxCSConv, wxConvISO8859_1, (wxFONTENCODING_ISO8859_1));

WXDLLIMPEXP_DATA_BASE(wxMBConv *) wxConvCurrent = wxGet_wxConvLibcPtr();
WXDLLIMPEXP_DATA_BASE(wxMBConv *) wxConvUI = wxGet_wxConvLocalPtr();

#ifdef __DARWIN__
// It is important to use this conversion object under Darwin as it ensures
// that Unicode strings are (re)composed correctly even though xnu kernel uses
// decomposed form internally (at least for the file names).
static wxMBConv_cf wxConvMacUTF8DObj(wxFONTENCODING_UTF8);
#endif

WXDLLIMPEXP_DATA_BASE(wxMBConv *) wxConvFileName =
#ifdef __DARWIN__
                                    &wxConvMacUTF8DObj;
#else // !__DARWIN__
                                    wxGet_wxConvLibcPtr();
#endif // __DARWIN__/!__DARWIN__
