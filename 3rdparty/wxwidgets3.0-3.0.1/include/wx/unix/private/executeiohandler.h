///////////////////////////////////////////////////////////////////////////////
// Name:        wx/unix/private/executeiohandler.h
// Purpose:     IO handler class for the FD used by wxExecute() under Unix
// Author:      Rob Bresalier, Vadim Zeitlin
// Created:     2013-01-06
// Copyright:   (c) 2013 Rob Bresalier, Vadim Zeitlin
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIX_PRIVATE_EXECUTEIOHANDLER_H_
#define _WX_UNIX_PRIVATE_EXECUTEIOHANDLER_H_

#include "wx/private/streamtempinput.h"

// This class handles IO events on the pipe FD connected to the child process
// stdout/stderr and is used by wxExecute().
//
// Currently it can derive from either wxEventLoopSourceHandler or
// wxFDIOHandler depending on the kind of dispatcher/event loop it is used
// with. In the future, when we get rid of wxFDIOHandler entirely, it will
// derive from wxEventLoopSourceHandler only.
template <class T>
class wxExecuteIOHandlerBase : public T
{
public:
    wxExecuteIOHandlerBase(int fd, wxStreamTempInputBuffer& buf)
        : m_fd(fd),
          m_buf(buf)
    {
        m_callbackDisabled = false;
    }

    // Called when the associated descriptor is available for reading.
    virtual void OnReadWaiting()
    {
        // Sync process, process all data coming at us from the pipe so that
        // the pipe does not get full and cause a deadlock situation.
        m_buf.Update();

        if ( m_buf.Eof() )
            DisableCallback();
    }

    // These methods are never called as we only monitor the associated FD for
    // reading, but we still must implement them as they're pure virtual in the
    // base class.
    virtual void OnWriteWaiting() { }
    virtual void OnExceptionWaiting() { }

    // Disable any future calls to our OnReadWaiting(), can be called when
    // we're sure that no more input is forthcoming.
    void DisableCallback()
    {
        if ( !m_callbackDisabled )
        {
            m_callbackDisabled = true;

            DoDisable();
        }
    }

protected:
    const int m_fd;

private:
    virtual void DoDisable() = 0;

    wxStreamTempInputBuffer& m_buf;

    // If true, DisableCallback() had been already called.
    bool m_callbackDisabled;

    wxDECLARE_NO_COPY_CLASS(wxExecuteIOHandlerBase);
};

// This is the version used with wxFDIODispatcher, which must be passed to the
// ctor in order to register this handler with it.
class wxExecuteFDIOHandler : public wxExecuteIOHandlerBase<wxFDIOHandler>
{
public:
    wxExecuteFDIOHandler(wxFDIODispatcher& dispatcher,
                         int fd,
                         wxStreamTempInputBuffer& buf)
        : wxExecuteIOHandlerBase<wxFDIOHandler>(fd, buf),
          m_dispatcher(dispatcher)
    {
        dispatcher.RegisterFD(fd, this, wxFDIO_INPUT);
    }

    virtual ~wxExecuteFDIOHandler()
    {
        DisableCallback();
    }

private:
    virtual void DoDisable()
    {
        m_dispatcher.UnregisterFD(m_fd);
    }

    wxFDIODispatcher& m_dispatcher;

    wxDECLARE_NO_COPY_CLASS(wxExecuteFDIOHandler);
};

// And this is the version used with an event loop. As AddSourceForFD() is
// static, we don't require passing the event loop to the ctor but an event
// loop must be running to handle our events.
class wxExecuteEventLoopSourceHandler
    : public wxExecuteIOHandlerBase<wxEventLoopSourceHandler>
{
public:
    wxExecuteEventLoopSourceHandler(int fd, wxStreamTempInputBuffer& buf)
        : wxExecuteIOHandlerBase<wxEventLoopSourceHandler>(fd, buf)
    {
        m_source = wxEventLoop::AddSourceForFD(fd, this, wxEVENT_SOURCE_INPUT);
    }

    virtual ~wxExecuteEventLoopSourceHandler()
    {
        DisableCallback();
    }

private:
    virtual void DoDisable()
    {
        delete m_source;
        m_source = NULL;
    }

    wxEventLoopSource* m_source;

    wxDECLARE_NO_COPY_CLASS(wxExecuteEventLoopSourceHandler);
};

#endif // _WX_UNIX_PRIVATE_EXECUTEIOHANDLER_H_
