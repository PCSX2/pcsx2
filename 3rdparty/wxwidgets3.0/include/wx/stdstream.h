/////////////////////////////////////////////////////////////////////////////
// Name:        wx/stdstream.h
// Purpose:     Header of std::istream and std::ostream derived wrappers for
//              wxInputStream and wxOutputStream
// Author:      Jonathan Liu <net147@gmail.com>
// Created:     2009-05-02
// Copyright:   (c) 2009 Jonathan Liu
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_STDSTREAM_H_
#define _WX_STDSTREAM_H_

#include "wx/defs.h"    // wxUSE_STD_IOSTREAM

#if wxUSE_STREAMS && wxUSE_STD_IOSTREAM

#include "wx/defs.h"
#include "wx/stream.h"
#include "wx/ioswrap.h"

// ==========================================================================
// wxStdInputStreamBuffer
// ==========================================================================

class WXDLLIMPEXP_BASE wxStdInputStreamBuffer : public std::streambuf
{
public:
    wxStdInputStreamBuffer(wxInputStream& stream);
    virtual ~wxStdInputStreamBuffer() { }

protected:
    virtual std::streambuf *setbuf(char *s, std::streamsize n);
    virtual std::streampos seekoff(std::streamoff off,
                                   std::ios_base::seekdir way,
                                   std::ios_base::openmode which =
                                       std::ios_base::in |
                                       std::ios_base::out);
    virtual std::streampos seekpos(std::streampos sp,
                                   std::ios_base::openmode which =
                                       std::ios_base::in |
                                       std::ios_base::out);
    virtual std::streamsize showmanyc();
    virtual std::streamsize xsgetn(char *s, std::streamsize n);
    virtual int underflow();
    virtual int uflow();
    virtual int pbackfail(int c = EOF);

    // Special work around for VC8/9 (this bug was fixed in VC10 and later):
    // these versions have non-standard _Xsgetn_s() that it being called from
    // the stream code instead of xsgetn() and so our overridden implementation
    // never actually gets used. To work around this, forward to it explicitly.
#if defined(__VISUALC8__) || defined(__VISUALC9__)
    virtual std::streamsize
    _Xsgetn_s(char *s, size_t WXUNUSED(size), std::streamsize n)
    {
        return xsgetn(s, n);
    }
#endif // VC8 or VC9

    wxInputStream& m_stream;
    int m_lastChar;
};

// ==========================================================================
// wxStdInputStream
// ==========================================================================

class WXDLLIMPEXP_BASE wxStdInputStream : public std::istream
{
public:
    wxStdInputStream(wxInputStream& stream);
    virtual ~wxStdInputStream() { }

protected:
    wxStdInputStreamBuffer m_streamBuffer;
};

// ==========================================================================
// wxStdOutputStreamBuffer
// ==========================================================================

class WXDLLIMPEXP_BASE wxStdOutputStreamBuffer : public std::streambuf
{
public:
    wxStdOutputStreamBuffer(wxOutputStream& stream);
    virtual ~wxStdOutputStreamBuffer() { }

protected:
    virtual std::streambuf *setbuf(char *s, std::streamsize n);
    virtual std::streampos seekoff(std::streamoff off,
                                   std::ios_base::seekdir way,
                                   std::ios_base::openmode which =
                                       std::ios_base::in |
                                       std::ios_base::out);
    virtual std::streampos seekpos(std::streampos sp,
                                   std::ios_base::openmode which =
                                       std::ios_base::in |
                                       std::ios_base::out);
    virtual std::streamsize xsputn(const char *s, std::streamsize n);
    virtual int overflow(int c);

    wxOutputStream& m_stream;
};

// ==========================================================================
// wxStdOutputStream
// ==========================================================================

class WXDLLIMPEXP_BASE wxStdOutputStream : public std::ostream
{
public:
    wxStdOutputStream(wxOutputStream& stream);
    virtual ~wxStdOutputStream() { }

protected:
    wxStdOutputStreamBuffer m_streamBuffer;
};

#endif // wxUSE_STREAMS && wxUSE_STD_IOSTREAM

#endif // _WX_STDSTREAM_H_
