/////////////////////////////////////////////////////////////////////////////
// Name:        wx/sckstrm.h
// Purpose:     wxSocket*Stream
// Author:      Guilhem Lavaux
// Modified by:
// Created:     17/07/97
// Copyright:   (c)
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////
#ifndef __SCK_STREAM_H__
#define __SCK_STREAM_H__

#include "wx/stream.h"

#if wxUSE_SOCKETS && wxUSE_STREAMS

#include "wx/socket.h"

class WXDLLIMPEXP_NET wxSocketOutputStream : public wxOutputStream
{
public:
    wxSocketOutputStream(wxSocketBase& s);
    virtual ~wxSocketOutputStream();

protected:
    wxSocketBase *m_o_socket;

    size_t OnSysWrite(const void *buffer, size_t bufsize);

    // socket streams are both un-seekable and size-less streams:
    wxFileOffset OnSysTell() const
        { return wxInvalidOffset; }
    wxFileOffset OnSysSeek(wxFileOffset WXUNUSED(pos), wxSeekMode WXUNUSED(mode))
        { return wxInvalidOffset; }

    wxDECLARE_NO_COPY_CLASS(wxSocketOutputStream);
};

class WXDLLIMPEXP_NET wxSocketInputStream : public wxInputStream
{
public:
    wxSocketInputStream(wxSocketBase& s);
    virtual ~wxSocketInputStream();

protected:
    wxSocketBase *m_i_socket;

    size_t OnSysRead(void *buffer, size_t bufsize);

    // socket streams are both un-seekable and size-less streams:

    wxFileOffset OnSysTell() const
        { return wxInvalidOffset; }
    wxFileOffset OnSysSeek(wxFileOffset WXUNUSED(pos), wxSeekMode WXUNUSED(mode))
        { return wxInvalidOffset; }

    wxDECLARE_NO_COPY_CLASS(wxSocketInputStream);
};

class WXDLLIMPEXP_NET wxSocketStream : public wxSocketInputStream,
                   public wxSocketOutputStream
{
public:
    wxSocketStream(wxSocketBase& s);
    virtual ~wxSocketStream();

    wxDECLARE_NO_COPY_CLASS(wxSocketStream);
};

#endif
  // wxUSE_SOCKETS && wxUSE_STREAMS

#endif
  // __SCK_STREAM_H__
