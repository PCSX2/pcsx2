/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/ipcbase.cpp
// Purpose:     IPC base classes
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
#endif

#include "wx/ipcbase.h"

wxIMPLEMENT_ABSTRACT_CLASS(wxServerBase, wxObject)
wxIMPLEMENT_ABSTRACT_CLASS(wxClientBase, wxObject)
wxIMPLEMENT_ABSTRACT_CLASS(wxConnectionBase, wxObject)

wxConnectionBase::wxConnectionBase(void *buffer, size_t bytes)
    : m_buffer((char *)buffer),
      m_buffersize(bytes),
      m_deletebufferwhendone(false),
      m_connected(true)
{
  if ( buffer == NULL )
  { // behave like next constructor
    m_buffersize = 0;
    m_deletebufferwhendone = true;
  }
}

wxConnectionBase::wxConnectionBase()
    : m_buffer(NULL),
      m_buffersize(0),
      m_deletebufferwhendone(true),
      m_connected(true)
{
}

wxConnectionBase::wxConnectionBase(const wxConnectionBase& copy)
    : wxObject(),
      m_buffer(copy.m_buffer),
      m_buffersize(copy.m_buffersize),
      m_deletebufferwhendone(false),
      m_connected(copy.m_connected)

{
  // copy constructor would require ref-counted pointer to buffer
  wxFAIL_MSG( wxT("Copy constructor of wxConnectionBase not implemented") );
}


wxConnectionBase::~wxConnectionBase()
{
  if ( m_deletebufferwhendone )
    delete [] m_buffer;
}

/* static */
wxString wxConnectionBase::GetTextFromData(const void* data,
                                           size_t size,
                                           wxIPCFormat fmt)
{
    wxString s;
    switch ( fmt )
    {
        case wxIPC_TEXT:
            // normally the string should be NUL-terminated and size should
            // include the total size of the buffer, including NUL -- but don't
            // crash (by trying to access (size_t)-1 bytes) if it doesn't
            if ( size )
                size--;

            s = wxString(static_cast<const char *>(data), size);
            break;

#if wxUSE_UNICODE
        // TODO: we should handle both wxIPC_UTF16TEXT and wxIPC_UTF32TEXT here
        //       for inter-platform IPC
        case wxIPC_UNICODETEXT:
            wxASSERT_MSG( !(size % sizeof(wchar_t)), "invalid buffer size" );
            if ( size )
            {
                size /= sizeof(wchar_t);
                size--;
            }

            s = wxString(static_cast<const wchar_t *>(data), size);
            break;

        case wxIPC_UTF8TEXT:
            if ( size )
                size--;

            s = wxString::FromUTF8(static_cast<const char *>(data), size);
            break;
#endif // wxUSE_UNICODE

        default:
            wxFAIL_MSG( "non-string IPC format in GetTextFromData()" );
    }

    return s;
}

void *wxConnectionBase::GetBufferAtLeast( size_t bytes )
{
  if ( m_buffersize >= bytes )
    return m_buffer;
  else
  {  // need to resize buffer
    if ( m_deletebufferwhendone )
    { // we're in charge of buffer, increase it
      delete [] m_buffer;
      m_buffer = new char[bytes];
      m_buffersize = bytes;
      return m_buffer;
    } // user-supplied buffer, fail
    else
      return NULL;
  }
}
