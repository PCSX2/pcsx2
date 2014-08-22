/////////////////////////////////////////////////////////////////////////////
// Name:        wx/unix/private/sockunix.h
// Purpose:     wxSocketImpl implementation for Unix systems
// Authors:     Guilhem Lavaux, Vadim Zeitlin
// Created:     April 1997
// Copyright:   (c) 1997 Guilhem Lavaux
//              (c) 2008 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_UNIX_GSOCKUNX_H_
#define _WX_UNIX_GSOCKUNX_H_

#include <unistd.h>
#include <sys/ioctl.h>

// Under older (Open)Solaris versions FIONBIO is declared in this header only.
// In the newer versions it's included by sys/ioctl.h but it's simpler to just
// include it always instead of testing for whether it is or not.
#ifdef __SOLARIS__
    #include <sys/filio.h>
#endif

#include "wx/private/fdiomanager.h"

class wxSocketImplUnix : public wxSocketImpl,
                         public wxFDIOHandler
{
public:
    wxSocketImplUnix(wxSocketBase& wxsocket)
        : wxSocketImpl(wxsocket)
    {
        m_fds[0] =
        m_fds[1] = -1;
    }

    virtual wxSocketError GetLastError() const;

    virtual void ReenableEvents(wxSocketEventFlags flags)
    {
        // enable the notifications about input/output being available again in
        // case they were disabled by OnRead/WriteWaiting()
        //
        // notice that we'd like to enable the events here only if there is
        // nothing more left on the socket right now as otherwise we're going
        // to get a "ready for whatever" notification immediately (well, during
        // the next event loop iteration) and disable the event back again
        // which is rather inefficient but unfortunately doing it like this
        // doesn't work because the existing code (e.g. src/common/sckipc.cpp)
        // expects to keep getting notifications about the data available from
        // the socket even if it didn't read all the data the last time, so we
        // absolutely have to continue generating them
        EnableEvents(flags);
    }

    // wxFDIOHandler methods
    virtual void OnReadWaiting();
    virtual void OnWriteWaiting();
    virtual void OnExceptionWaiting();
    virtual bool IsOk() const { return m_fd != INVALID_SOCKET; }

private:
    virtual void DoClose()
    {
        DisableEvents();

        close(m_fd);
    }

    virtual void UnblockAndRegisterWithEventLoop()
    {
        int trueArg = 1;
        ioctl(m_fd, FIONBIO, &trueArg);

        EnableEvents();
    }

    // enable or disable notifications for socket input/output events
    void EnableEvents(int flags = wxSOCKET_INPUT_FLAG | wxSOCKET_OUTPUT_FLAG)
        { DoEnableEvents(flags, true); }
    void DisableEvents(int flags = wxSOCKET_INPUT_FLAG | wxSOCKET_OUTPUT_FLAG)
        { DoEnableEvents(flags, false); }

    // really enable or disable socket input/output events
    void DoEnableEvents(int flags, bool enable);

protected:
    // descriptors for input and output event notification channels associated
    // with the socket
    int m_fds[2];

private:
    // notify the associated wxSocket about a change in socket state and shut
    // down the socket if the event is wxSOCKET_LOST
    void OnStateChange(wxSocketNotify event);

    // check if there is any input available, return 1 if yes, 0 if no or -1 on
    // error
    int CheckForInput();


    // give it access to our m_fds
    friend class wxSocketFDBasedManager;
};

// A version of wxSocketManager which uses FDs for socket IO: it is used by
// Unix console applications and some X11-like ports (wxGTK and wxMotif but not
// wxX11 currently) which implement their own port-specific wxFDIOManagers
class wxSocketFDBasedManager : public wxSocketManager
{
public:
    wxSocketFDBasedManager()
    {
        m_fdioManager = NULL;
    }

    virtual bool OnInit();
    virtual void OnExit() { }

    virtual wxSocketImpl *CreateSocket(wxSocketBase& wxsocket)
    {
        return new wxSocketImplUnix(wxsocket);
    }

    virtual void Install_Callback(wxSocketImpl *socket_, wxSocketNotify event);
    virtual void Uninstall_Callback(wxSocketImpl *socket_, wxSocketNotify event);

protected:
    // get the FD index corresponding to the given wxSocketNotify
    wxFDIOManager::Direction
    GetDirForEvent(wxSocketImpl *socket, wxSocketNotify event);

    // access the FDs we store
    int& FD(wxSocketImplUnix *socket, wxFDIOManager::Direction d)
    {
        return socket->m_fds[d];
    }

    wxFDIOManager *m_fdioManager;

    wxDECLARE_NO_COPY_CLASS(wxSocketFDBasedManager);
};

#endif  /* _WX_UNIX_GSOCKUNX_H_ */
