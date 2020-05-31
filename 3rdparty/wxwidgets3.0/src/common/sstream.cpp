///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/sstream.cpp
// Purpose:     string-based streams implementation
// Author:      Vadim Zeitlin
// Modified by: Ryan Norton (UTF8 UNICODE)
// Created:     2004-09-19
// Copyright:   (c) 2004 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_STREAMS

#include "wx/sstream.h"

// ============================================================================
// wxStringInputStream implementation
// ============================================================================

// ----------------------------------------------------------------------------
// construction/destruction
// ----------------------------------------------------------------------------

// TODO:  Do we want to include the null char in the stream?  If so then
// just add +1 to m_len in the ctor
wxStringInputStream::wxStringInputStream(const wxString& s)
#if wxUSE_UNICODE
    // FIXME-UTF8: use wxCharBufferWithLength if we have it
    : m_str(s), m_buf(s.utf8_str()), m_len(strlen(m_buf))
#else
    : m_str(s), m_buf(s.mb_str()), m_len(s.length())
#endif
{
#if wxUSE_UNICODE
    wxASSERT_MSG(m_buf.data() != NULL, wxT("Could not convert string to UTF8!"));
#endif
    m_pos = 0;
}

// ----------------------------------------------------------------------------
// getlength
// ----------------------------------------------------------------------------

wxFileOffset wxStringInputStream::GetLength() const
{
    return m_len;
}

// ----------------------------------------------------------------------------
// seek/tell
// ----------------------------------------------------------------------------

wxFileOffset wxStringInputStream::OnSysSeek(wxFileOffset ofs, wxSeekMode mode)
{
    switch ( mode )
    {
        case wxFromStart:
            // nothing to do, ofs already ok
            break;

        case wxFromEnd:
            ofs += m_len;
            break;

        case wxFromCurrent:
            ofs += m_pos;
            break;

        default:
            wxFAIL_MSG( wxT("invalid seek mode") );
            return wxInvalidOffset;
    }

    if ( ofs < 0 || ofs > static_cast<wxFileOffset>(m_len) )
        return wxInvalidOffset;

    // FIXME: this can't be right
    m_pos = wx_truncate_cast(size_t, ofs);

    return ofs;
}

wxFileOffset wxStringInputStream::OnSysTell() const
{
    return static_cast<wxFileOffset>(m_pos);
}

// ----------------------------------------------------------------------------
// actual IO
// ----------------------------------------------------------------------------

size_t wxStringInputStream::OnSysRead(void *buffer, size_t size)
{
    const size_t sizeMax = m_len - m_pos;

    if ( size >= sizeMax )
    {
        if ( sizeMax == 0 )
        {
            m_lasterror = wxSTREAM_EOF;
            return 0;
        }

        size = sizeMax;
    }

    memcpy(buffer, m_buf.data() + m_pos, size);
    m_pos += size;

    return size;
}

// ============================================================================
// wxStringOutputStream implementation
// ============================================================================

// ----------------------------------------------------------------------------
// seek/tell
// ----------------------------------------------------------------------------

wxFileOffset wxStringOutputStream::OnSysTell() const
{
    return static_cast<wxFileOffset>(m_pos);
}

// ----------------------------------------------------------------------------
// actual IO
// ----------------------------------------------------------------------------

size_t wxStringOutputStream::OnSysWrite(const void *buffer, size_t size)
{
    const char *p = static_cast<const char *>(buffer);

#if wxUSE_UNICODE
    // the part of the string we have here may be incomplete, i.e. it can stop
    // in the middle of an UTF-8 character and so converting it would fail; if
    // this is the case, accumulate the part which we failed to convert until
    // we get the rest (and also take into account the part which we might have
    // left unconverted before)
    const char *src;
    size_t srcLen;
    if ( m_unconv.GetDataLen() )
    {
        // append the new data to the data remaining since the last time
        m_unconv.AppendData(p, size);
        src = m_unconv;
        srcLen = m_unconv.GetDataLen();
    }
    else // no unconverted data left, avoid extra copy
    {
        src = p;
        srcLen = size;
    }

    size_t wlen;
    wxWCharBuffer wbuf(m_conv.cMB2WC(src, srcLen, &wlen));
    if ( wbuf )
    {
        // conversion succeeded, clear the unconverted buffer
        m_unconv = wxMemoryBuffer(0);

        m_str->append(wbuf, wlen);
    }
    else // conversion failed
    {
        // remember unconverted data if there had been none before (otherwise
        // we've already got it in the buffer)
        if ( src == p )
            m_unconv.AppendData(src, srcLen);

        // pretend that we wrote the data anyhow, otherwise the caller would
        // believe there was an error and this might not be the case, but do
        // not update m_pos as m_str hasn't changed
        return size;
    }
#else // !wxUSE_UNICODE
    // no recoding necessary
    m_str->append(p, size);
#endif // wxUSE_UNICODE/!wxUSE_UNICODE

    // update position
    m_pos += size;

    // return number of bytes actually written
    return size;
}

#endif // wxUSE_STREAMS

