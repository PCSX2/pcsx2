///////////////////////////////////////////////////////////////////////////////
// Name:        wx/private/streamtempinput.h
// Purpose:     defines wxStreamTempInputBuffer which is used by Unix and MSW
//              implementations of wxExecute; this file is only used by the
//              library and never by the user code
// Author:      Vadim Zeitlin
// Modified by: Rob Bresalier
// Created:     2013-05-04
// Copyright:   (c) 2002 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_STREAMTEMPINPUT_H
#define _WX_PRIVATE_STREAMTEMPINPUT_H

#include "wx/private/pipestream.h"

// ----------------------------------------------------------------------------
// wxStreamTempInputBuffer
// ----------------------------------------------------------------------------

/*
   wxStreamTempInputBuffer is a hack which we need to solve the problem of
   executing a child process synchronously with IO redirecting: when we do
   this, the child writes to a pipe we open to it but when the pipe buffer
   (which has finite capacity, e.g. commonly just 4Kb) becomes full we have to
   read data from it because the child blocks in its write() until then and if
   it blocks we are never going to return from wxExecute() so we dead lock.

   So here is the fix: we now read the output as soon as it appears into a temp
   buffer (wxStreamTempInputBuffer object) and later just stuff it back into
   the stream when the process terminates. See supporting code in wxExecute()
   itself as well.

   Note that this is horribly inefficient for large amounts of output (count
   the number of times we copy the data around) and so a better API is badly
   needed! However it's not easy to devise a way to do this keeping backwards
   compatibility with the existing wxExecute(wxEXEC_SYNC)...
*/
class wxStreamTempInputBuffer
{
public:
    wxStreamTempInputBuffer()
    {
        m_stream = NULL;
        m_buffer = NULL;
        m_size = 0;
    }

    // call to associate a stream with this buffer, otherwise nothing happens
    // at all
    void Init(wxPipeInputStream *stream)
    {
        wxASSERT_MSG( !m_stream, wxS("Can only initialize once") );

        m_stream = stream;
    }

    // check for input on our stream and cache it in our buffer if any
    //
    // return true if anything was done
    bool Update()
    {
        if ( !m_stream || !m_stream->CanRead() )
            return false;

        // realloc in blocks of 4Kb: this is the default (and minimal) buffer
        // size of the Unix pipes so it should be the optimal step
        //
        // NB: don't use "static int" in this inline function, some compilers
        //     (e.g. IBM xlC) don't like it
        enum { incSize = 4096 };

        void *buf = realloc(m_buffer, m_size + incSize);
        if ( !buf )
            return false;

        m_buffer = buf;
        m_stream->Read((char *)m_buffer + m_size, incSize);
        m_size += m_stream->LastRead();

        return true;
    }

    // check if can continue reading from the stream, this is used to disable
    // the callback once we can't read anything more
    bool Eof() const
    {
        // If we have no stream, always return true as we can't read any more.
        return !m_stream || m_stream->Eof();
    }

    // read everything remaining until the EOF, this should only be called once
    // the child process terminates and we know that no more data is coming
    bool ReadAll()
    {
        while ( !Eof() )
        {
            if ( !Update() )
                return false;
        }

        return true;
    }

    // dtor puts the data buffered during this object lifetime into the
    // associated stream
    ~wxStreamTempInputBuffer()
    {
        if ( m_buffer )
        {
            m_stream->Ungetch(m_buffer, m_size);
            free(m_buffer);
        }
    }

    const void *GetBuffer() const { return m_buffer; }

    size_t GetSize() const { return m_size; }

private:
    // the stream we're buffering, if NULL we don't do anything at all
    wxPipeInputStream *m_stream;

    // the buffer of size m_size (NULL if m_size == 0)
    void *m_buffer;

    // the size of the buffer
    size_t m_size;

    wxDECLARE_NO_COPY_CLASS(wxStreamTempInputBuffer);
};

#endif // _WX_PRIVATE_STREAMTEMPINPUT_H
