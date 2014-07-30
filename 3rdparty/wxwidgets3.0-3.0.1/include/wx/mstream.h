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
    virtual wxFileOffset GetLength() const { return m_length; }
    virtual bool IsSeekable() const { return true; }

    virtual char Peek();
    virtual bool CanRead() const;

    wxStreamBuffer *GetInputStreamBuffer() const { return m_i_streambuf; }

#if WXWIN_COMPATIBILITY_2_6
    // deprecated, compatibility only
    wxDEPRECATED( wxStreamBuffer *InputStreamBuffer() const );
#endif // WXWIN_COMPATIBILITY_2_6

protected:
    wxStreamBuffer *m_i_streambuf;

    size_t OnSysRead(void *buffer, size_t nbytes);
    wxFileOffset OnSysSeek(wxFileOffset pos, wxSeekMode mode);
    wxFileOffset OnSysTell() const;

private:
    // common part of ctors taking wxInputStream
    void InitFromStream(wxInputStream& stream, wxFileOffset lenFile);

    size_t m_length;

    // copy ctor is implemented above: it copies the other stream in this one
    DECLARE_ABSTRACT_CLASS(wxMemoryInputStream)
    wxDECLARE_NO_ASSIGN_CLASS(wxMemoryInputStream);
};

class WXDLLIMPEXP_BASE wxMemoryOutputStream : public wxOutputStream
{
public:
    // if data is !NULL it must be allocated with malloc()
    wxMemoryOutputStream(void *data = NULL, size_t length = 0);
    virtual ~wxMemoryOutputStream();
    virtual wxFileOffset GetLength() const { return m_o_streambuf->GetLastAccess(); }
    virtual bool IsSeekable() const { return true; }

    size_t CopyTo(void *buffer, size_t len) const;

    wxStreamBuffer *GetOutputStreamBuffer() const { return m_o_streambuf; }

#if WXWIN_COMPATIBILITY_2_6
    // deprecated, compatibility only
    wxDEPRECATED( wxStreamBuffer *OutputStreamBuffer() const );
#endif // WXWIN_COMPATIBILITY_2_6

protected:
    wxStreamBuffer *m_o_streambuf;

protected:
    size_t OnSysWrite(const void *buffer, size_t nbytes);
    wxFileOffset OnSysSeek(wxFileOffset pos, wxSeekMode mode);
    wxFileOffset OnSysTell() const;

    DECLARE_DYNAMIC_CLASS(wxMemoryOutputStream)
    wxDECLARE_NO_COPY_CLASS(wxMemoryOutputStream);
};

#if WXWIN_COMPATIBILITY_2_6
    inline wxStreamBuffer *wxMemoryInputStream::InputStreamBuffer() const { return m_i_streambuf; }
    inline wxStreamBuffer *wxMemoryOutputStream::OutputStreamBuffer() const { return m_o_streambuf; }
#endif // WXWIN_COMPATIBILITY_2_6

#endif
  // wxUSE_STREAMS

#endif
  // _WX_WXMMSTREAM_H__
