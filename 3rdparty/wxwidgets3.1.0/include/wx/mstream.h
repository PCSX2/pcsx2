/////////////////////////////////////////////////////////////////////////////
// Name:        wx/mstream.h
// Purpose:     Memory stream classes
// Author:      Guilhem Lavaux
// Modified by:
// Created:     11/07/98
// Copyright:   (c) Guilhem Lavaux
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_WXMMSTREAM_H__
#define _WX_WXMMSTREAM_H__

#include "wx/defs.h"

#if wxUSE_STREAMS

#include "wx/stream.h"

class WXDLLIMPEXP_FWD_BASE wxMemoryOutputStream;

class WXDLLIMPEXP_BASE wxMemoryInputStream : public wxInputStream
{
public:
    wxMemoryInputStream(const void *data, size_t length);
    wxMemoryInputStream(const wxMemoryOutputStream& stream);
    wxMemoryInputStream(wxInputStream& stream,
                        wxFileOffset lenFile = wxInvalidOffset)
    {
        InitFromStream(stream, lenFile);
    }
    wxMemoryInputStream(wxMemoryInputStream& stream)
        : wxInputStream()
    {
        InitFromStream(stream, wxInvalidOffset);
    }

    virtual ~wxMemoryInputStream();
    virtual wxFileOffset GetLength() const wxOVERRIDE { return m_length; }
    virtual bool IsSeekable() const wxOVERRIDE { return true; }

    virtual char Peek() wxOVERRIDE;
    virtual bool CanRead() const wxOVERRIDE;

    wxStreamBuffer *GetInputStreamBuffer() const { return m_i_streambuf; }

protected:
    wxStreamBuffer *m_i_streambuf;

    size_t OnSysRead(void *buffer, size_t nbytes) wxOVERRIDE;
    wxFileOffset OnSysSeek(wxFileOffset pos, wxSeekMode mode) wxOVERRIDE;
    wxFileOffset OnSysTell() const wxOVERRIDE;

private:
    // common part of ctors taking wxInputStream
    void InitFromStream(wxInputStream& stream, wxFileOffset lenFile);

    size_t m_length;

    // copy ctor is implemented above: it copies the other stream in this one
    wxDECLARE_ABSTRACT_CLASS(wxMemoryInputStream);
    wxDECLARE_NO_ASSIGN_CLASS(wxMemoryInputStream);
};

class WXDLLIMPEXP_BASE wxMemoryOutputStream : public wxOutputStream
{
public:
    // if data is !NULL it must be allocated with malloc()
    wxMemoryOutputStream(void *data = NULL, size_t length = 0);
    virtual ~wxMemoryOutputStream();
    virtual wxFileOffset GetLength() const wxOVERRIDE { return m_o_streambuf->GetLastAccess(); }
    virtual bool IsSeekable() const wxOVERRIDE { return true; }

    size_t CopyTo(void *buffer, size_t len) const;

    wxStreamBuffer *GetOutputStreamBuffer() const { return m_o_streambuf; }

protected:
    wxStreamBuffer *m_o_streambuf;

protected:
    size_t OnSysWrite(const void *buffer, size_t nbytes) wxOVERRIDE;
    wxFileOffset OnSysSeek(wxFileOffset pos, wxSeekMode mode) wxOVERRIDE;
    wxFileOffset OnSysTell() const wxOVERRIDE;

    wxDECLARE_DYNAMIC_CLASS(wxMemoryOutputStream);
    wxDECLARE_NO_COPY_CLASS(wxMemoryOutputStream);
};

#endif
  // wxUSE_STREAMS

#endif
  // _WX_WXMMSTREAM_H__
