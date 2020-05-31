/////////////////////////////////////////////////////////////////////////////
// Name:       src/common/socket.cpp
// Purpose:    Socket handler classes
// Authors:    Guilhem Lavaux, Guillermo Rodriguez Garcia
// Created:    April 1997
// Copyright:  (C) 1999-1997, Guilhem Lavaux
//             (C) 1999-2000, Guillermo Rodriguez Garcia
//             (C) 2008 Vadim Zeitlin
// Licence:    wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ==========================================================================
// Declarations
// ==========================================================================

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_SOCKETS

#include "wx/socket.h"

#ifndef WX_PRECOMP
    #include "wx/object.h"
    #include "wx/string.h"
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/event.h"
    #include "wx/app.h"
    #include "wx/utils.h"
    #include "wx/timer.h"
    #include "wx/module.h"
#endif

#include "wx/apptrait.h"
#include "wx/sckaddr.h"
#include "wx/stopwatch.h"
#include "wx/thread.h"
#include "wx/evtloop.h"
#include "wx/link.h"

#include "wx/private/fd.h"
#include "wx/private/socket.h"

#ifdef __UNIX__
    #include <errno.h>
#endif

// we use MSG_NOSIGNAL to avoid getting SIGPIPE when sending data to a remote
// host which closed the connection if it is available, otherwise we rely on
// SO_NOSIGPIPE existency
//
// this should cover all the current Unix systems (Windows never sends any
// signals anyhow) but if we find one that has neither we should explicitly
// ignore SIGPIPE for it
// OpenVMS has neither MSG_NOSIGNAL nor SO_NOSIGPIPE. However the socket sample
// seems to work. Not sure if problems will show up on OpenVMS using sockets.
#ifdef MSG_NOSIGNAL
    #define wxSOCKET_MSG_NOSIGNAL MSG_NOSIGNAL
#else // MSG_NOSIGNAL not available (BSD including OS X)
    // next best possibility is to use SO_NOSIGPIPE socket option, this covers
    // BSD systems (including OS X) -- but if we don't have it neither (AIX and
    // old HP-UX do not), we have to fall back to the old way of simply
    // disabling SIGPIPE temporarily, so define a class to do it in a safe way
    #if defined(__UNIX__) && !defined(SO_NOSIGPIPE)
    extern "C" { typedef void (*wxSigHandler_t)(int); }
    namespace
    {
        class IgnoreSignal
        {
        public:
            // ctor disables the given signal
            IgnoreSignal(int sig)
                : m_handler(signal(sig, SIG_IGN)),
                  m_sig(sig)
            {
            }

            // dtor restores the old handler
            ~IgnoreSignal()
            {
                signal(m_sig, m_handler);
            }

        private:
            const wxSigHandler_t m_handler;
            const int m_sig;

            wxDECLARE_NO_COPY_CLASS(IgnoreSignal);
        };
    } // anonymous namespace

    #define wxNEEDS_IGNORE_SIGPIPE
    #endif // Unix without SO_NOSIGPIPE

    #define wxSOCKET_MSG_NOSIGNAL 0
#endif

// DLL options compatibility check:
#include "wx/build.h"
WX_CHECK_BUILD_OPTIONS("wxNet")

// --------------------------------------------------------------------------
// macros and constants
// --------------------------------------------------------------------------

// event
wxDEFINE_EVENT(wxEVT_SOCKET, wxSocketEvent);

// discard buffer
#define MAX_DISCARD_SIZE (10 * 1024)

#define wxTRACE_Socket wxT("wxSocket")

// --------------------------------------------------------------------------
// wxWin macros
// --------------------------------------------------------------------------

IMPLEMENT_CLASS(wxSocketBase, wxObject)
IMPLEMENT_CLASS(wxSocketServer, wxSocketBase)
IMPLEMENT_CLASS(wxSocketClient, wxSocketBase)
IMPLEMENT_CLASS(wxDatagramSocket, wxSocketBase)
IMPLEMENT_DYNAMIC_CLASS(wxSocketEvent, wxEvent)

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

namespace
{

void SetTimeValFromMS(timeval& tv, unsigned long ms)
{
    tv.tv_sec  = (ms / 1000);
    tv.tv_usec = (ms % 1000) * 1000;
}

} // anonymous namespace

// --------------------------------------------------------------------------
// private classes
// --------------------------------------------------------------------------

class wxSocketState : public wxObject
{
public:
    wxSocketFlags            m_flags;
    wxSocketEventFlags       m_eventmask;
    bool                     m_notify;
    void                    *m_clientData;

public:
    wxSocketState() : wxObject() {}

    wxDECLARE_NO_COPY_CLASS(wxSocketState);
};

// wxSocketWaitModeChanger: temporarily change the socket flags affecting its
// wait mode
class wxSocketWaitModeChanger
{
public:
    // temporarily set the flags to include the flag value which may be either
    // wxSOCKET_NOWAIT or wxSOCKET_WAITALL
    wxSocketWaitModeChanger(wxSocketBase *socket, int flag)
        : m_socket(socket),
          m_oldflags(socket->GetFlags())

    {
        // We can be passed only wxSOCKET_WAITALL{_READ,_WRITE} or
        // wxSOCKET_NOWAIT{_READ,_WRITE} normally.
        wxASSERT_MSG( !(flag & wxSOCKET_WAITALL) || !(flag & wxSOCKET_NOWAIT),
                      "not a wait flag" );

        // preserve wxSOCKET_BLOCK value when switching to wxSOCKET_WAITALL
        // mode but not when switching to wxSOCKET_NOWAIT as the latter is
        // incompatible with wxSOCKET_BLOCK
        if ( flag != wxSOCKET_NOWAIT )
            flag |= m_oldflags & wxSOCKET_BLOCK;

        socket->SetFlags(flag);
    }

    ~wxSocketWaitModeChanger()
    {
        m_socket->SetFlags(m_oldflags);
    }

private:
    wxSocketBase * const m_socket;
    const int m_oldflags;

    wxDECLARE_NO_COPY_CLASS(wxSocketWaitModeChanger);
};

// wxSocketRead/WriteGuard are instantiated before starting reading
// from/writing to the socket
class wxSocketReadGuard
{
public:
    wxSocketReadGuard(wxSocketBase *socket)
        : m_socket(socket)
    {
        wxASSERT_MSG( !m_socket->m_reading, "read reentrancy?" );

        m_socket->m_reading = true;
    }

    ~wxSocketReadGuard()
    {
        m_socket->m_reading = false;

        // connection could have been lost while reading, in this case calling
        // ReenableEvents() would assert and is not necessary anyhow
        wxSocketImpl * const impl = m_socket->m_impl;
        if ( impl && impl->m_fd != INVALID_SOCKET )
            impl->ReenableEvents(wxSOCKET_INPUT_FLAG);
    }

private:
    wxSocketBase * const m_socket;

    wxDECLARE_NO_COPY_CLASS(wxSocketReadGuard);
};

class wxSocketWriteGuard
{
public:
    wxSocketWriteGuard(wxSocketBase *socket)
        : m_socket(socket)
    {
        wxASSERT_MSG( !m_socket->m_writing, "write reentrancy?" );

        m_socket->m_writing = true;
    }

    ~wxSocketWriteGuard()
    {
        m_socket->m_writing = false;

        wxSocketImpl * const impl = m_socket->m_impl;
        if ( impl && impl->m_fd != INVALID_SOCKET )
            impl->ReenableEvents(wxSOCKET_OUTPUT_FLAG);
    }

private:
    wxSocketBase * const m_socket;

    wxDECLARE_NO_COPY_CLASS(wxSocketWriteGuard);
};

// ============================================================================
// wxSocketManager
// ============================================================================

wxSocketManager *wxSocketManager::ms_manager = NULL;

/* static */
void wxSocketManager::Set(wxSocketManager *manager)
{
    wxASSERT_MSG( !ms_manager, "too late to set manager now" );

    ms_manager = manager;
}

/* static */
void wxSocketManager::Init()
{
    wxASSERT_MSG( !ms_manager, "shouldn't be initialized twice" );

    /*
        Details: Initialize() creates a hidden window as a sink for socket
        events, such as 'read completed'. wxMSW has only one message loop
        for the main thread. If Initialize is called in a secondary thread,
        the socket window will be created for the secondary thread, but
        since there is no message loop on this thread, it will never
        receive events and all socket operations will time out.
        BTW, the main thread must not be stopped using sleep or block
        on a semaphore (a bad idea in any case) or socket operations
        will time out.

        On the Mac side, Initialize() stores a pointer to the CFRunLoop for
        the main thread. Because secondary threads do not have run loops,
        adding event notifications to the "Current" loop would have no
        effect at all, events would never fire.
    */
    wxASSERT_MSG( wxIsMainThread(),
                    "sockets must be initialized from the main thread" );

    wxAppConsole * const app = wxAppConsole::GetInstance();
    wxCHECK_RET( app, "sockets can't be initialized without wxApp" );

    ms_manager = app->GetTraits()->GetSocketManager();
}

// ==========================================================================
// wxSocketImpl
// ==========================================================================

wxSocketImpl::wxSocketImpl(wxSocketBase& wxsocket)
    : m_wxsocket(&wxsocket)
{
    m_fd              = INVALID_SOCKET;
    m_error           = wxSOCKET_NOERROR;
    m_server          = false;
    m_stream          = true;

    SetTimeout(wxsocket.GetTimeout() * 1000);

    m_establishing    = false;
    m_reusable        = false;
    m_broadcast       = false;
    m_dobind          = true;
    m_initialRecvBufferSize = -1;
    m_initialSendBufferSize = -1;
}

wxSocketImpl::~wxSocketImpl()
{
    if ( m_fd != INVALID_SOCKET )
        Shutdown();
}

bool wxSocketImpl::PreCreateCheck(const wxSockAddressImpl& addr)
{
    if ( m_fd != INVALID_SOCKET )
    {
        m_error = wxSOCKET_INVSOCK;
        return false;
    }

    if ( !addr.IsOk() )
    {
        m_error = wxSOCKET_INVADDR;
        return false;
    }

    return true;
}

void wxSocketImpl::PostCreation()
{
    // FreeBSD variants can't use MSG_NOSIGNAL, and instead use a socket option
#ifdef SO_NOSIGPIPE
    EnableSocketOption(SO_NOSIGPIPE);
#endif

    if ( m_reusable )
        EnableSocketOption(SO_REUSEADDR);

    if ( m_broadcast )
    {
        wxASSERT_MSG( !m_stream, "broadcasting is for datagram sockets only" );

        EnableSocketOption(SO_BROADCAST);
    }

    if ( m_initialRecvBufferSize >= 0 )
        SetSocketOption(SO_RCVBUF, m_initialRecvBufferSize);
    if ( m_initialSendBufferSize >= 0 )
        SetSocketOption(SO_SNDBUF, m_initialSendBufferSize);

    // we always put our sockets in unblocked mode and handle blocking
    // ourselves in DoRead/Write() if wxSOCKET_WAITALL is specified
    UnblockAndRegisterWithEventLoop();
}

wxSocketError wxSocketImpl::UpdateLocalAddress()
{
    if ( !m_local.IsOk() )
    {
        // ensure that we have a valid object using the correct family: correct
        // being the same one as our peer uses as we have no other way to
        // determine it
        m_local.Create(m_peer.GetFamily());
    }

    WX_SOCKLEN_T lenAddr = m_local.GetLen();
    if ( getsockname(m_fd, m_local.GetWritableAddr(), &lenAddr) != 0 )
    {
        Close();
        m_error = wxSOCKET_IOERR;
        return m_error;
    }

    return wxSOCKET_NOERROR;
}

wxSocketError wxSocketImpl::CreateServer()
{
    if ( !PreCreateCheck(m_local) )
        return m_error;

    m_server = true;
    m_stream = true;

    // do create the socket
    m_fd = socket(m_local.GetFamily(), SOCK_STREAM, 0);

    if ( m_fd == INVALID_SOCKET )
    {
        m_error = wxSOCKET_IOERR;
        return wxSOCKET_IOERR;
    }

    PostCreation();

    // and then bind to and listen on it
    //
    // FIXME: should we test for m_dobind here?
    if ( bind(m_fd, m_local.GetAddr(), m_local.GetLen()) != 0 )
        m_error = wxSOCKET_IOERR;

    if ( IsOk() )
    {
        if ( listen(m_fd, 5) != 0 )
            m_error = wxSOCKET_IOERR;
    }

    if ( !IsOk() )
    {
        Close();
        return m_error;
    }

    // finally retrieve the address we effectively bound to
    return UpdateLocalAddress();
}

wxSocketError wxSocketImpl::CreateClient(bool wait)
{
    if ( !PreCreateCheck(m_peer) )
        return m_error;

    m_fd = socket(m_peer.GetFamily(), SOCK_STREAM, 0);

    if ( m_fd == INVALID_SOCKET )
    {
        m_error = wxSOCKET_IOERR;
        return wxSOCKET_IOERR;
    }

    PostCreation();

    // If a local address has been set, then bind to it before calling connect
    if ( m_local.IsOk() )
    {
        if ( bind(m_fd, m_local.GetAddr(), m_local.GetLen()) != 0 )
        {
            Close();
            m_error = wxSOCKET_IOERR;
            return m_error;
        }
    }

    // Do connect now
    int rc = connect(m_fd, m_peer.GetAddr(), m_peer.GetLen());
    if ( rc == SOCKET_ERROR )
    {
        wxSocketError err = GetLastError();
        if ( err == wxSOCKET_WOULDBLOCK )
        {
            m_establishing = true;

            // block waiting for connection if we should (otherwise just return
            // wxSOCKET_WOULDBLOCK to the caller)
            if ( wait )
            {
                err = SelectWithTimeout(wxSOCKET_CONNECTION_FLAG)
                        ? wxSOCKET_NOERROR
                        : wxSOCKET_TIMEDOUT;
                m_establishing = false;
            }
        }

        m_error = err;
    }
    else // connected
    {
        m_error = wxSOCKET_NOERROR;
    }

    return m_error;
}


wxSocketError wxSocketImpl::CreateUDP()
{
    if ( !PreCreateCheck(m_local) )
        return m_error;

    m_stream = false;
    m_server = false;

    m_fd = socket(m_local.GetFamily(), SOCK_DGRAM, 0);

    if ( m_fd == INVALID_SOCKET )
    {
        m_error = wxSOCKET_IOERR;
        return wxSOCKET_IOERR;
    }

    PostCreation();

    if ( m_dobind )
    {
        if ( bind(m_fd, m_local.GetAddr(), m_local.GetLen()) != 0 )
        {
            Close();
            m_error = wxSOCKET_IOERR;
            return m_error;
        }

        return UpdateLocalAddress();
    }

    return wxSOCKET_NOERROR;
}

wxSocketImpl *wxSocketImpl::Accept(wxSocketBase& wxsocket)
{
    wxSockAddressStorage from;
    WX_SOCKLEN_T fromlen = sizeof(from);
    const wxSOCKET_T fd = accept(m_fd, &from.addr, &fromlen);

    // accepting is similar to reading in the sense that it resets "ready for
    // read" flag on the socket
    ReenableEvents(wxSOCKET_INPUT_FLAG);

    if ( fd == INVALID_SOCKET )
        return NULL;

    wxSocketManager * const manager = wxSocketManager::Get();
    if ( !manager )
        return NULL;

    wxSocketImpl * const sock = manager->CreateSocket(wxsocket);
    if ( !sock )
        return NULL;

    sock->m_fd = fd;
    sock->m_peer = wxSockAddressImpl(from.addr, fromlen);

    sock->UnblockAndRegisterWithEventLoop();

    return sock;
}


void wxSocketImpl::Close()
{
    if ( m_fd != INVALID_SOCKET )
    {
        DoClose();
        m_fd = INVALID_SOCKET;
    }
}

void wxSocketImpl::Shutdown()
{
    if ( m_fd != INVALID_SOCKET )
    {
        shutdown(m_fd, 1 /* SD_SEND */);
        Close();
    }
}

/*
 *  Sets the timeout for blocking calls. Time is expressed in
 *  milliseconds.
 */
void wxSocketImpl::SetTimeout(unsigned long millis)
{
    SetTimeValFromMS(m_timeout, millis);
}

void wxSocketImpl::NotifyOnStateChange(wxSocketNotify event)
{
    m_wxsocket->OnRequest(event);
}

/* Address handling */
wxSocketError wxSocketImpl::SetLocal(const wxSockAddressImpl& local)
{
    /* the socket must be initialized, or it must be a server */
    if (m_fd != INVALID_SOCKET && !m_server)
    {
        m_error = wxSOCKET_INVSOCK;
        return wxSOCKET_INVSOCK;
    }

    if ( !local.IsOk() )
    {
        m_error = wxSOCKET_INVADDR;
        return wxSOCKET_INVADDR;
    }

    m_local = local;

    return wxSOCKET_NOERROR;
}

wxSocketError wxSocketImpl::SetPeer(const wxSockAddressImpl& peer)
{
    if ( !peer.IsOk() )
    {
        m_error = wxSOCKET_INVADDR;
        return wxSOCKET_INVADDR;
    }

    m_peer = peer;

    return wxSOCKET_NOERROR;
}

const wxSockAddressImpl& wxSocketImpl::GetLocal()
{
    if ( !m_local.IsOk() )
        UpdateLocalAddress();

    return m_local;
}

// ----------------------------------------------------------------------------
// wxSocketImpl IO
// ----------------------------------------------------------------------------

// this macro wraps the given expression (normally a syscall) in a loop which
// ignores any interruptions, i.e. reevaluates it again if it failed and errno
// is EINTR
#ifdef __UNIX__
    #define DO_WHILE_EINTR( rc, syscall ) \
        do { \
            rc = (syscall); \
        } \
        while ( rc == -1 && errno == EINTR )
#else
    #define DO_WHILE_EINTR( rc, syscall ) rc = (syscall)
#endif

int wxSocketImpl::RecvStream(void *buffer, int size)
{
    int ret;
    DO_WHILE_EINTR( ret, recv(m_fd, static_cast<char *>(buffer), size, 0) );

    if ( !ret )
    {
        // receiving 0 bytes for a TCP socket indicates that the connection was
        // closed by peer so shut down our end as well (for UDP sockets empty
        // datagrams are also possible)
        m_establishing = false;
        NotifyOnStateChange(wxSOCKET_LOST);

        Shutdown();

        // do not return an error in this case however
    }

    return ret;
}

int wxSocketImpl::SendStream(const void *buffer, int size)
{
#ifdef wxNEEDS_IGNORE_SIGPIPE
    IgnoreSignal ignore(SIGPIPE);
#endif

    int ret;
    DO_WHILE_EINTR( ret, send(m_fd, static_cast<const char *>(buffer), size,
                              wxSOCKET_MSG_NOSIGNAL) );

    return ret;
}

int wxSocketImpl::RecvDgram(void *buffer, int size)
{
    wxSockAddressStorage from;
    WX_SOCKLEN_T fromlen = sizeof(from);

    int ret;
    DO_WHILE_EINTR( ret, recvfrom(m_fd, static_cast<char *>(buffer), size,
                                  0, &from.addr, &fromlen) );

    if ( ret == SOCKET_ERROR )
        return SOCKET_ERROR;

    m_peer = wxSockAddressImpl(from.addr, fromlen);
    if ( !m_peer.IsOk() )
        return -1;

    return ret;
}

int wxSocketImpl::SendDgram(const void *buffer, int size)
{
    if ( !m_peer.IsOk() )
    {
        m_error = wxSOCKET_INVADDR;
        return -1;
    }

    int ret;
    DO_WHILE_EINTR( ret, sendto(m_fd, static_cast<const char *>(buffer), size,
                                0, m_peer.GetAddr(), m_peer.GetLen()) );

    return ret;
}

int wxSocketImpl::Read(void *buffer, int size)
{
    // server sockets can't be used for IO, only to accept new connections
    if ( m_fd == INVALID_SOCKET || m_server )
    {
        m_error = wxSOCKET_INVSOCK;
        return -1;
    }

    int ret = m_stream ? RecvStream(buffer, size)
                       : RecvDgram(buffer, size);

    m_error = ret == SOCKET_ERROR ? GetLastError() : wxSOCKET_NOERROR;

    return ret;
}

int wxSocketImpl::Write(const void *buffer, int size)
{
    if ( m_fd == INVALID_SOCKET || m_server )
    {
        m_error = wxSOCKET_INVSOCK;
        return -1;
    }

    int ret = m_stream ? SendStream(buffer, size)
                       : SendDgram(buffer, size);

    m_error = ret == SOCKET_ERROR ? GetLastError() : wxSOCKET_NOERROR;

    return ret;
}

// ==========================================================================
// wxSocketBase
// ==========================================================================

// --------------------------------------------------------------------------
// Initialization and shutdown
// --------------------------------------------------------------------------

namespace
{

// counts the number of calls to Initialize() minus the number of calls to
// Shutdown(): we don't really need it any more but it was documented that
// Shutdown() must be called the same number of times as Initialize() and using
// a counter helps us to check it
int gs_socketInitCount = 0;

} // anonymous namespace

bool wxSocketBase::IsInitialized()
{
    wxASSERT_MSG( wxIsMainThread(), "unsafe to call from other threads" );

    return gs_socketInitCount != 0;
}

bool wxSocketBase::Initialize()
{
    wxCHECK_MSG( wxIsMainThread(), false,
                 "must be called from the main thread" );

    if ( !gs_socketInitCount )
    {
        wxSocketManager * const manager = wxSocketManager::Get();
        if ( !manager || !manager->OnInit() )
            return false;
    }

    gs_socketInitCount++;

    return true;
}

void wxSocketBase::Shutdown()
{
    wxCHECK_RET( wxIsMainThread(), "must be called from the main thread" );

    wxCHECK_RET( gs_socketInitCount > 0, "too many calls to Shutdown()" );

    if ( !--gs_socketInitCount )
    {
        wxSocketManager * const manager = wxSocketManager::Get();
        wxCHECK_RET( manager, "should have a socket manager" );

        manager->OnExit();
    }
}

// --------------------------------------------------------------------------
// Ctor and dtor
// --------------------------------------------------------------------------

void wxSocketBase::Init()
{
    m_impl         = NULL;
    m_type         = wxSOCKET_UNINIT;

    // state
    m_flags        = 0;
    m_connected    =
    m_establishing =
    m_reading      =
    m_writing      =
    m_closed       = false;
    m_lcount       = 0;
    m_lcount_read  = 0;
    m_lcount_write = 0;
    m_timeout      = 600;
    m_beingDeleted = false;

    // pushback buffer
    m_unread       = NULL;
    m_unrd_size    = 0;
    m_unrd_cur     = 0;

    // events
    m_id           = wxID_ANY;
    m_handler      = NULL;
    m_clientData   = NULL;
    m_notify       = false;
    m_eventmask    =
    m_eventsgot    = 0;

    // when we create the first socket in the main thread we initialize the
    // OS-dependent socket stuff: notice that this means that the user code
    // needs to call wxSocket::Initialize() itself if the first socket it
    // creates is not created in the main thread
    if ( wxIsMainThread() )
    {
        if ( !Initialize() )
        {
            wxLogError(_("Cannot initialize sockets"));
        }
    }
}

wxSocketBase::wxSocketBase()
{
    Init();
}

wxSocketBase::wxSocketBase(wxSocketFlags flags, wxSocketType type)
{
    Init();

    SetFlags(flags);

    m_type = type;
}

wxSocketBase::~wxSocketBase()
{
    // Shutdown and close the socket
    if (!m_beingDeleted)
        Close();

    // Destroy the implementation object
    delete m_impl;

    // Free the pushback buffer
    free(m_unread);
}

bool wxSocketBase::Destroy()
{
    // Delayed destruction: the socket will be deleted during the next idle
    // loop iteration. This ensures that all pending events have been
    // processed.
    m_beingDeleted = true;

    // Shutdown and close the socket
    Close();

    // Suppress events from now on
    Notify(false);

    // Schedule this object for deletion instead of destroying it right now if
    // it can have other events pending for it and we have a way to do it.
    //
    // Notice that sockets used in other threads won't have any events for them
    // and we shouldn't use delayed destruction mechanism for them as it's not
    // MT-safe.
    if ( wxIsMainThread() && wxTheApp )
    {
        wxTheApp->ScheduleForDestruction(this);
    }
    else // no app
    {
        // in wxBase we might have no app object at all, don't leak memory
        delete this;
    }

    return true;
}

// ----------------------------------------------------------------------------
// simple accessors
// ----------------------------------------------------------------------------

void wxSocketBase::SetError(wxSocketError error)
{
    m_impl->m_error = error;
}

wxSocketError wxSocketBase::LastError() const
{
    return m_impl->GetError();
}

// --------------------------------------------------------------------------
// Basic IO calls
// --------------------------------------------------------------------------

// The following IO operations update m_lcount:
// {Read, Write, ReadMsg, WriteMsg, Peek, Unread, Discard}
bool wxSocketBase::Close()
{
    // Interrupt pending waits
    InterruptWait();

    ShutdownOutput();

    m_connected = false;
    m_establishing = false;
    return true;
}

void wxSocketBase::ShutdownOutput()
{
    if ( m_impl )
        m_impl->Shutdown();
}

wxSocketBase& wxSocketBase::Read(void* buffer, wxUint32 nbytes)
{
    wxSocketReadGuard read(this);

    m_lcount_read = DoRead(buffer, nbytes);
    m_lcount = m_lcount_read;

    return *this;
}

wxUint32 wxSocketBase::DoRead(void* buffer_, wxUint32 nbytes)
{
    wxCHECK_MSG( m_impl, 0, "socket must be valid" );

    // We use pointer arithmetic here which doesn't work with void pointers.
    char *buffer = static_cast<char *>(buffer_);
    wxCHECK_MSG( buffer, 0, "NULL buffer" );

    // Try the push back buffer first, even before checking whether the socket
    // is valid to allow reading previously pushed back data from an already
    // closed socket.
    wxUint32 total = GetPushback(buffer, nbytes, false);
    nbytes -= total;
    buffer += total;

    while ( nbytes )
    {
        // our socket is non-blocking so Read() will return immediately if
        // there is nothing to read yet and it's more efficient to try it first
        // before entering DoWait() which is going to start dispatching GUI
        // events and, even more importantly, we must do this under Windows
        // where we're not going to get notifications about socket being ready
        // for reading before we read all the existing data from it
        const int ret = !m_impl->m_stream || m_connected
                            ? m_impl->Read(buffer, nbytes)
                            : 0;
        if ( ret == -1 )
        {
            if ( m_impl->GetLastError() == wxSOCKET_WOULDBLOCK )
            {
                // if we don't want to wait, just return immediately
                if ( m_flags & wxSOCKET_NOWAIT_READ )
                {
                    // this shouldn't be counted as an error in this case
                    SetError(wxSOCKET_NOERROR);
                    break;
                }

                // otherwise wait until the socket becomes ready for reading or
                // an error occurs on it
                if ( !DoWaitWithTimeout(wxSOCKET_INPUT_FLAG) )
                {
                    // and exit if the timeout elapsed before it did
                    SetError(wxSOCKET_TIMEDOUT);
                    break;
                }

                // retry reading
                continue;
            }
            else // "real" error
            {
                SetError(wxSOCKET_IOERR);
                break;
            }
        }
        else if ( ret == 0 )
        {
            // for connection-oriented (e.g. TCP) sockets we can only read
            // 0 bytes if the other end has been closed, and for connectionless
            // ones (UDP) this flag doesn't make sense anyhow so we can set it
            // to true too without doing any harm
            m_closed = true;

            // we're not going to read anything else and so if we haven't read
            // anything (or not everything in wxSOCKET_WAITALL case) already,
            // signal an error
            if ( (m_flags & wxSOCKET_WAITALL_READ) || !total )
                SetError(wxSOCKET_IOERR);
            break;
        }

        total += ret;

        // if we are happy to read something and not the entire nbytes bytes,
        // then we're done
        if ( !(m_flags & wxSOCKET_WAITALL_READ) )
            break;

        nbytes -= ret;
        buffer += ret;
    }

    return total;
}

wxSocketBase& wxSocketBase::ReadMsg(void* buffer, wxUint32 nbytes)
{
    struct
    {
        unsigned char sig[4];
        unsigned char len[4];
    } msg;

    wxSocketReadGuard read(this);

    wxSocketWaitModeChanger changeFlags(this, wxSOCKET_WAITALL_READ);

    bool ok = false;
    if ( DoRead(&msg, sizeof(msg)) == sizeof(msg) )
    {
        wxUint32 sig = (wxUint32)msg.sig[0];
        sig |= (wxUint32)(msg.sig[1] << 8);
        sig |= (wxUint32)(msg.sig[2] << 16);
        sig |= (wxUint32)(msg.sig[3] << 24);

        if ( sig == 0xfeeddead )
        {
            wxUint32 len = (wxUint32)msg.len[0];
            len |= (wxUint32)(msg.len[1] << 8);
            len |= (wxUint32)(msg.len[2] << 16);
            len |= (wxUint32)(msg.len[3] << 24);

            wxUint32 len2;
            if (len > nbytes)
            {
                len2 = len - nbytes;
                len = nbytes;
            }
            else
                len2 = 0;

            // Don't attempt to read if the msg was zero bytes long.
            m_lcount_read = len ? DoRead(buffer, len) : 0;
            m_lcount = m_lcount_read;

            if ( len2 )
            {
                char discard_buffer[MAX_DISCARD_SIZE];
                long discard_len;

                // NOTE: discarded bytes don't add to m_lcount.
                do
                {
                    discard_len = len2 > MAX_DISCARD_SIZE
                                    ? MAX_DISCARD_SIZE
                                    : len2;
                    discard_len = DoRead(discard_buffer, (wxUint32)discard_len);
                    len2 -= (wxUint32)discard_len;
                }
                while ((discard_len > 0) && len2);
            }

            if ( !len2 && DoRead(&msg, sizeof(msg)) == sizeof(msg) )
            {
                sig = (wxUint32)msg.sig[0];
                sig |= (wxUint32)(msg.sig[1] << 8);
                sig |= (wxUint32)(msg.sig[2] << 16);
                sig |= (wxUint32)(msg.sig[3] << 24);

                if ( sig == 0xdeadfeed )
                    ok = true;
            }
        }
    }

    if ( !ok )
        SetError(wxSOCKET_IOERR);

    return *this;
}

wxSocketBase& wxSocketBase::Peek(void* buffer, wxUint32 nbytes)
{
    wxSocketReadGuard read(this);

    // Peek() should never block
    wxSocketWaitModeChanger changeFlags(this, wxSOCKET_NOWAIT);

    m_lcount = DoRead(buffer, nbytes);

    Pushback(buffer, m_lcount);

    return *this;
}

wxSocketBase& wxSocketBase::Write(const void *buffer, wxUint32 nbytes)
{
    wxSocketWriteGuard write(this);

    m_lcount_write = DoWrite(buffer, nbytes);
    m_lcount = m_lcount_write;

    return *this;
}

// This function is a mirror image of DoRead() except that it doesn't use the
// push back buffer and doesn't treat 0 return value specially (normally this
// shouldn't happen at all here), so please see comments there for explanations
wxUint32 wxSocketBase::DoWrite(const void *buffer_, wxUint32 nbytes)
{
    wxCHECK_MSG( m_impl, 0, "socket must be valid" );

    const char *buffer = static_cast<const char *>(buffer_);
    wxCHECK_MSG( buffer, 0, "NULL buffer" );

    wxUint32 total = 0;
    while ( nbytes )
    {
        if ( m_impl->m_stream && !m_connected )
        {
            if ( (m_flags & wxSOCKET_WAITALL_WRITE) || !total )
                SetError(wxSOCKET_IOERR);
            break;
        }

        const int ret = m_impl->Write(buffer, nbytes);
        if ( ret == -1 )
        {
            if ( m_impl->GetLastError() == wxSOCKET_WOULDBLOCK )
            {
                if ( m_flags & wxSOCKET_NOWAIT_WRITE )
                    break;

                if ( !DoWaitWithTimeout(wxSOCKET_OUTPUT_FLAG) )
                {
                    SetError(wxSOCKET_TIMEDOUT);
                    break;
                }

                continue;
            }
            else // "real" error
            {
                SetError(wxSOCKET_IOERR);
                break;
            }
        }

        total += ret;

        if ( !(m_flags & wxSOCKET_WAITALL_WRITE) )
            break;

        nbytes -= ret;
        buffer += ret;
    }

    return total;
}

wxSocketBase& wxSocketBase::WriteMsg(const void *buffer, wxUint32 nbytes)
{
    struct
    {
        unsigned char sig[4];
        unsigned char len[4];
    } msg;

    wxSocketWriteGuard write(this);

    wxSocketWaitModeChanger changeFlags(this, wxSOCKET_WAITALL_WRITE);

    msg.sig[0] = (unsigned char) 0xad;
    msg.sig[1] = (unsigned char) 0xde;
    msg.sig[2] = (unsigned char) 0xed;
    msg.sig[3] = (unsigned char) 0xfe;

    msg.len[0] = (unsigned char) (nbytes & 0xff);
    msg.len[1] = (unsigned char) ((nbytes >> 8) & 0xff);
    msg.len[2] = (unsigned char) ((nbytes >> 16) & 0xff);
    msg.len[3] = (unsigned char) ((nbytes >> 24) & 0xff);

    bool ok = false;
    if ( DoWrite(&msg, sizeof(msg)) == sizeof(msg) )
    {
        m_lcount_write = DoWrite(buffer, nbytes);
        m_lcount = m_lcount_write;
        if ( m_lcount_write == nbytes )
        {
            msg.sig[0] = (unsigned char) 0xed;
            msg.sig[1] = (unsigned char) 0xfe;
            msg.sig[2] = (unsigned char) 0xad;
            msg.sig[3] = (unsigned char) 0xde;
            msg.len[0] =
            msg.len[1] =
            msg.len[2] =
            msg.len[3] = (char) 0;

            if ( DoWrite(&msg, sizeof(msg)) == sizeof(msg))
                ok = true;
        }
    }

    if ( !ok )
        SetError(wxSOCKET_IOERR);

    return *this;
}

wxSocketBase& wxSocketBase::Unread(const void *buffer, wxUint32 nbytes)
{
    if (nbytes != 0)
        Pushback(buffer, nbytes);

    SetError(wxSOCKET_NOERROR);
    m_lcount = nbytes;

    return *this;
}

wxSocketBase& wxSocketBase::Discard()
{
    char *buffer = new char[MAX_DISCARD_SIZE];
    wxUint32 ret;
    wxUint32 total = 0;

    wxSocketReadGuard read(this);

    wxSocketWaitModeChanger changeFlags(this, wxSOCKET_NOWAIT);

    do
    {
        ret = DoRead(buffer, MAX_DISCARD_SIZE);
        total += ret;
    }
    while (ret == MAX_DISCARD_SIZE);

    delete[] buffer;
    m_lcount = total;
    SetError(wxSOCKET_NOERROR);

    return *this;
}

// --------------------------------------------------------------------------
// Wait functions
// --------------------------------------------------------------------------

/*
    This function will check for the events specified in the flags parameter,
    and it will return a mask indicating which operations can be performed.
 */
wxSocketEventFlags wxSocketImpl::Select(wxSocketEventFlags flags,
                                        const timeval *timeout)
{
    if ( m_fd == INVALID_SOCKET )
        return (wxSOCKET_LOST_FLAG & flags);

    struct timeval tv;
    if ( timeout )
        tv = *timeout;
    else
        tv.tv_sec = tv.tv_usec = 0;

    // prepare the FD sets, passing NULL for the one(s) we don't use
    fd_set
        readfds, *preadfds = NULL,
        writefds, *pwritefds = NULL,
        exceptfds;                      // always want to know about errors

    if ( flags & wxSOCKET_INPUT_FLAG )
        preadfds = &readfds;

    if ( flags & wxSOCKET_OUTPUT_FLAG )
        pwritefds = &writefds;

    // When using non-blocking connect() the client socket becomes connected
    // (successfully or not) when it becomes writable but when using
    // non-blocking accept() the server socket becomes connected when it
    // becomes readable.
    if ( flags & wxSOCKET_CONNECTION_FLAG )
    {
        if ( m_server )
            preadfds = &readfds;
        else
            pwritefds = &writefds;
    }

    if ( preadfds )
    {
        wxFD_ZERO(preadfds);
        wxFD_SET(m_fd, preadfds);
    }

    if ( pwritefds )
    {
        wxFD_ZERO(pwritefds);
        wxFD_SET(m_fd, pwritefds);
    }

    wxFD_ZERO(&exceptfds);
    wxFD_SET(m_fd, &exceptfds);

    const int rc = select(m_fd + 1, preadfds, pwritefds, &exceptfds, &tv);

    // check for errors first
    if ( rc == -1 || wxFD_ISSET(m_fd, &exceptfds) )
    {
        m_establishing = false;

        return wxSOCKET_LOST_FLAG & flags;
    }

    if ( rc == 0 )
        return 0;

    wxASSERT_MSG( rc == 1, "unexpected select() return value" );

    wxSocketEventFlags detected = 0;
    if ( preadfds && wxFD_ISSET(m_fd, preadfds) )
    {
        // check for the case of a server socket waiting for connection
        if ( m_server && (flags & wxSOCKET_CONNECTION_FLAG) )
        {
            int error;
            SOCKOPTLEN_T len = sizeof(error);
            m_establishing = false;
            getsockopt(m_fd, SOL_SOCKET, SO_ERROR, (char*)&error, &len);

            if ( error )
                detected = wxSOCKET_LOST_FLAG;
            else
                detected |= wxSOCKET_CONNECTION_FLAG;
        }
        else // not called to get non-blocking accept() status
        {
            detected |= wxSOCKET_INPUT_FLAG;
        }
    }

    if ( pwritefds && wxFD_ISSET(m_fd, pwritefds) )
    {
        // check for the case of non-blocking connect()
        if ( m_establishing && !m_server )
        {
            int error;
            SOCKOPTLEN_T len = sizeof(error);
            m_establishing = false;
            getsockopt(m_fd, SOL_SOCKET, SO_ERROR, (char*)&error, &len);

            if ( error )
                detected = wxSOCKET_LOST_FLAG;
            else
                detected |= wxSOCKET_CONNECTION_FLAG;
        }
        else // not called to get non-blocking connect() status
        {
            detected |= wxSOCKET_OUTPUT_FLAG;
        }
    }

    return detected & flags;
}

int
wxSocketBase::DoWait(long seconds, long milliseconds, wxSocketEventFlags flags)
{
    // Use either the provided timeout or the default timeout value associated
    // with this socket.
    //
    // TODO: allow waiting forever, see #9443
    const long timeout = seconds == -1 ? m_timeout * 1000
                                       : seconds * 1000 + milliseconds;

    return DoWait(timeout, flags);
}

int
wxSocketBase::DoWait(long timeout, wxSocketEventFlags flags)
{
    wxCHECK_MSG( m_impl, -1, "can't wait on invalid socket" );

    // we're never going to become ready in a TCP client if we're not connected
    // any more (OTOH a server can call this to precisely wait for a connection
    // so do wait for it in this case and UDP client is never "connected")
    if ( !m_impl->IsServer() &&
            m_impl->m_stream && !m_connected && !m_establishing )
        return -1;

    // This can be set to true from Interrupt() to exit this function a.s.a.p.
    m_interrupt = false;


    const wxMilliClock_t timeEnd = wxGetLocalTimeMillis() + timeout;

    // Get the active event loop which we'll use for the message dispatching
    // when running in the main thread unless this was explicitly disabled by
    // setting wxSOCKET_BLOCK flag
    wxEventLoopBase *eventLoop;
    if ( !(m_flags & wxSOCKET_BLOCK) && wxIsMainThread() )
    {
        eventLoop = wxEventLoop::GetActive();
    }
    else // in worker thread
    {
        // We never dispatch messages from threads other than the main one.
        eventLoop = NULL;
    }

    // Make sure the events we're interested in are enabled before waiting for
    // them: this is really necessary here as otherwise this could happen:
    //  1. DoRead(wxSOCKET_WAITALL) is called
    //  2. There is nothing to read so DoWait(wxSOCKET_INPUT_FLAG) is called
    //  3. Some, but not all data appears, wxSocketImplUnix::OnReadWaiting()
    //     is called and wxSOCKET_INPUT_FLAG events are disabled in it
    //  4. Because of wxSOCKET_WAITALL we call DoWait() again but the events
    //     are still disabled and we block forever
    //
    // More elegant solution would be nice but for now simply re-enabling the
    // events here will do
    m_impl->ReenableEvents(flags & (wxSOCKET_INPUT_FLAG | wxSOCKET_OUTPUT_FLAG));


    // Wait until we receive the event we're waiting for or the timeout expires
    // (but note that we always execute the loop at least once, even if timeout
    // is 0 as this is used for polling)
    int rc = 0;
    for ( bool firstTime = true; !m_interrupt; firstTime = false )
    {
        long timeLeft = wxMilliClockToLong(timeEnd - wxGetLocalTimeMillis());
        if ( timeLeft < 0 )
        {
            if ( !firstTime )
                break;   // timed out

            timeLeft = 0;
        }

        wxSocketEventFlags events;
        if ( eventLoop )
        {
            // reset them before starting to wait
            m_eventsgot = 0;

            eventLoop->DispatchTimeout(timeLeft);

            events = m_eventsgot;
        }
        else // no event loop or waiting in another thread
        {
            // as explained below, we should always check for wxSOCKET_LOST_FLAG
            timeval tv;
            SetTimeValFromMS(tv, timeLeft);
            events = m_impl->Select(flags | wxSOCKET_LOST_FLAG, &tv);
        }

        // always check for wxSOCKET_LOST_FLAG, even if flags doesn't include
        // it, as continuing to wait for anything else after getting it is
        // pointless
        if ( events & wxSOCKET_LOST_FLAG )
        {
            m_connected = false;
            m_establishing = false;
            rc = -1;
            break;
        }

        // otherwise mask out the bits we're not interested in
        events &= flags;

        // Incoming connection (server) or connection established (client)?
        if ( events & wxSOCKET_CONNECTION_FLAG )
        {
            m_connected = true;
            m_establishing = false;
            rc = true;
            break;
        }

        // Data available or output buffer ready?
        if ( (events & wxSOCKET_INPUT_FLAG) || (events & wxSOCKET_OUTPUT_FLAG) )
        {
            rc = true;
            break;
        }
    }

    return rc;
}

bool wxSocketBase::Wait(long seconds, long milliseconds)
{
    return DoWait(seconds, milliseconds,
                  wxSOCKET_INPUT_FLAG |
                  wxSOCKET_OUTPUT_FLAG |
                  wxSOCKET_CONNECTION_FLAG) != 0;
}

bool wxSocketBase::WaitForRead(long seconds, long milliseconds)
{
    // Check pushback buffer before entering DoWait
    if ( m_unread )
        return true;

    // Check if the socket is not already ready for input, if it is, there is
    // no need to start waiting for it (worse, we'll actually never get a
    // notification about the socket becoming ready if it is already under
    // Windows)
    if ( m_impl->Select(wxSOCKET_INPUT_FLAG) )
        return true;

    return DoWait(seconds, milliseconds, wxSOCKET_INPUT_FLAG) != 0;
}


bool wxSocketBase::WaitForWrite(long seconds, long milliseconds)
{
    if ( m_impl->Select(wxSOCKET_OUTPUT_FLAG) )
        return true;

    return DoWait(seconds, milliseconds, wxSOCKET_OUTPUT_FLAG) != 0;
}

bool wxSocketBase::WaitForLost(long seconds, long milliseconds)
{
    return DoWait(seconds, milliseconds, wxSOCKET_LOST_FLAG) == -1;
}

// --------------------------------------------------------------------------
// Miscellaneous
// --------------------------------------------------------------------------

//
// Get local or peer address
//

bool wxSocketBase::GetPeer(wxSockAddress& addr) const
{
    wxCHECK_MSG( m_impl, false, "invalid socket" );

    const wxSockAddressImpl& peer = m_impl->GetPeer();
    if ( !peer.IsOk() )
        return false;

    addr.SetAddress(peer);

    return true;
}

bool wxSocketBase::GetLocal(wxSockAddress& addr) const
{
    wxCHECK_MSG( m_impl, false, "invalid socket" );

    const wxSockAddressImpl& local = m_impl->GetLocal();
    if ( !local.IsOk() )
        return false;

    addr.SetAddress(local);

    return true;
}

//
// Save and restore socket state
//

void wxSocketBase::SaveState()
{
    wxSocketState *state;

    state = new wxSocketState();

    state->m_flags      = m_flags;
    state->m_notify     = m_notify;
    state->m_eventmask  = m_eventmask;
    state->m_clientData = m_clientData;

    m_states.Append(state);
}

void wxSocketBase::RestoreState()
{
    wxList::compatibility_iterator node;
    wxSocketState *state;

    node = m_states.GetLast();
    if (!node)
        return;

    state = (wxSocketState *)node->GetData();

    m_flags      = state->m_flags;
    m_notify     = state->m_notify;
    m_eventmask  = state->m_eventmask;
    m_clientData = state->m_clientData;

    m_states.Erase(node);
    delete state;
}

//
// Timeout and flags
//

void wxSocketBase::SetTimeout(long seconds)
{
    m_timeout = seconds;

    if (m_impl)
        m_impl->SetTimeout(m_timeout * 1000);
}

void wxSocketBase::SetFlags(wxSocketFlags flags)
{
    // Do some sanity checking on the flags used: not all values can be used
    // together.
    wxASSERT_MSG( !(flags & wxSOCKET_NOWAIT) ||
                  !(flags & (wxSOCKET_WAITALL | wxSOCKET_BLOCK)),
                  "Using wxSOCKET_WAITALL or wxSOCKET_BLOCK with "
                  "wxSOCKET_NOWAIT doesn't make sense" );

    m_flags = flags;
}


// --------------------------------------------------------------------------
// Event handling
// --------------------------------------------------------------------------

void wxSocketBase::OnRequest(wxSocketNotify notification)
{
    wxSocketEventFlags flag = 0;
    switch ( notification )
    {
        case wxSOCKET_INPUT:
            flag = wxSOCKET_INPUT_FLAG;
            break;

        case wxSOCKET_OUTPUT:
            flag = wxSOCKET_OUTPUT_FLAG;
            break;

        case wxSOCKET_CONNECTION:
            flag = wxSOCKET_CONNECTION_FLAG;

            // we're now successfully connected
            m_connected = true;
            m_establishing = false;

            // error was previously set to wxSOCKET_WOULDBLOCK, but this is not
            // the case any longer
            SetError(wxSOCKET_NOERROR);
            break;

        case wxSOCKET_LOST:
            flag = wxSOCKET_LOST_FLAG;

            // if we lost the connection the socket is now closed and not
            // connected any more
            m_connected = false;
            m_closed = true;
            break;

        default:
            wxFAIL_MSG( "unknown wxSocket notification" );
    }

    // remember the events which were generated for this socket, we're going to
    // use this in DoWait()
    m_eventsgot |= flag;

    // send the wx event if enabled and we're interested in it
    if ( m_notify && (m_eventmask & flag) && m_handler )
    {
        // don't generate the events when we're inside DoWait() called from our
        // own code as we are going to consume the data that has just become
        // available ourselves and the user code won't see it at all
        if ( (notification == wxSOCKET_INPUT && m_reading) ||
                (notification == wxSOCKET_OUTPUT && m_writing) )
        {
            return;
        }

        wxSocketEvent event(m_id);
        event.m_event      = notification;
        event.m_clientData = m_clientData;
        event.SetEventObject(this);

        m_handler->AddPendingEvent(event);
    }
}

void wxSocketBase::Notify(bool notify)
{
    m_notify = notify;
}

void wxSocketBase::SetNotify(wxSocketEventFlags flags)
{
    m_eventmask = flags;
}

void wxSocketBase::SetEventHandler(wxEvtHandler& handler, int id)
{
    m_handler = &handler;
    m_id      = id;
}

// --------------------------------------------------------------------------
// Pushback buffer
// --------------------------------------------------------------------------

void wxSocketBase::Pushback(const void *buffer, wxUint32 size)
{
    if (!size) return;

    if (m_unread == NULL)
        m_unread = malloc(size);
    else
    {
        void *tmp;

        tmp = malloc(m_unrd_size + size);
        memcpy((char *)tmp + size, m_unread, m_unrd_size);
        free(m_unread);

        m_unread = tmp;
    }

    m_unrd_size += size;

    memcpy(m_unread, buffer, size);
}

wxUint32 wxSocketBase::GetPushback(void *buffer, wxUint32 size, bool peek)
{
    wxCHECK_MSG( buffer, 0, "NULL buffer" );

    if (!m_unrd_size)
        return 0;

    if (size > (m_unrd_size-m_unrd_cur))
        size = m_unrd_size-m_unrd_cur;

    memcpy(buffer, (char *)m_unread + m_unrd_cur, size);

    if (!peek)
    {
        m_unrd_cur += size;
        if (m_unrd_size == m_unrd_cur)
        {
            free(m_unread);
            m_unread = NULL;
            m_unrd_size = 0;
            m_unrd_cur  = 0;
        }
    }

    return size;
}


// ==========================================================================
// wxSocketServer
// ==========================================================================

// --------------------------------------------------------------------------
// Ctor
// --------------------------------------------------------------------------

wxSocketServer::wxSocketServer(const wxSockAddress& addr,
                               wxSocketFlags flags)
              : wxSocketBase(flags, wxSOCKET_SERVER)
{
    wxLogTrace( wxTRACE_Socket, wxT("Opening wxSocketServer") );

    wxSocketManager * const manager = wxSocketManager::Get();
    m_impl = manager ? manager->CreateSocket(*this) : NULL;

    if (!m_impl)
    {
        wxLogTrace( wxTRACE_Socket, wxT("*** Failed to create m_impl") );
        return;
    }

    // Setup the socket as server
    m_impl->SetLocal(addr.GetAddress());

    if (GetFlags() & wxSOCKET_REUSEADDR) {
        m_impl->SetReusable();
    }
    if (GetFlags() & wxSOCKET_BROADCAST) {
        m_impl->SetBroadcast();
    }
    if (GetFlags() & wxSOCKET_NOBIND) {
        m_impl->DontDoBind();
    }

    if (m_impl->CreateServer() != wxSOCKET_NOERROR)
    {
        wxDELETE(m_impl);

        wxLogTrace( wxTRACE_Socket, wxT("*** CreateServer() failed") );
        return;
    }

    // Notice that we need a cast as wxSOCKET_T is 64 bit under Win64 and that
    // the cast is safe because a wxSOCKET_T is a handle and so limited to 32
    // (or, actually, even 24) bit values anyhow.
    wxLogTrace( wxTRACE_Socket, wxT("wxSocketServer on fd %u"),
                static_cast<unsigned>(m_impl->m_fd) );
}

// --------------------------------------------------------------------------
// Accept
// --------------------------------------------------------------------------

bool wxSocketServer::AcceptWith(wxSocketBase& sock, bool wait)
{
    if ( !m_impl || (m_impl->m_fd == INVALID_SOCKET) || !m_impl->IsServer() )
    {
        wxFAIL_MSG( "can only be called for a valid server socket" );

        SetError(wxSOCKET_INVSOCK);

        return false;
    }

    if ( wait )
    {
        // wait until we get a connection
        if ( !m_impl->SelectWithTimeout(wxSOCKET_INPUT_FLAG) )
        {
            SetError(wxSOCKET_TIMEDOUT);

            return false;
        }
    }

    sock.m_impl = m_impl->Accept(sock);

    if ( !sock.m_impl )
    {
        SetError(m_impl->GetLastError());

        return false;
    }

    sock.m_type = wxSOCKET_BASE;
    sock.m_connected = true;

    return true;
}

wxSocketBase *wxSocketServer::Accept(bool wait)
{
    wxSocketBase* sock = new wxSocketBase();

    sock->SetFlags(m_flags);

    if (!AcceptWith(*sock, wait))
    {
        sock->Destroy();
        sock = NULL;
    }

    return sock;
}

bool wxSocketServer::WaitForAccept(long seconds, long milliseconds)
{
    return DoWait(seconds, milliseconds, wxSOCKET_CONNECTION_FLAG) == 1;
}

wxSOCKET_T wxSocketBase::GetSocket() const
{
    wxASSERT_MSG( m_impl, wxS("Socket not initialised") );

    return m_impl->m_fd;
}


bool wxSocketBase::GetOption(int level, int optname, void *optval, int *optlen)
{
    wxASSERT_MSG( m_impl, wxT("Socket not initialised") );

    SOCKOPTLEN_T lenreal = *optlen;
    if ( getsockopt(m_impl->m_fd, level, optname,
                    static_cast<char *>(optval), &lenreal) != 0 )
        return false;

    *optlen = lenreal;

    return true;
}

bool
wxSocketBase::SetOption(int level, int optname, const void *optval, int optlen)
{
    wxASSERT_MSG( m_impl, wxT("Socket not initialised") );

    return setsockopt(m_impl->m_fd, level, optname,
                      static_cast<const char *>(optval), optlen) == 0;
}

bool wxSocketBase::SetLocal(const wxIPV4address& local)
{
    m_localAddress = local;

    return true;
}

// ==========================================================================
// wxSocketClient
// ==========================================================================

// --------------------------------------------------------------------------
// Ctor and dtor
// --------------------------------------------------------------------------

wxSocketClient::wxSocketClient(wxSocketFlags flags)
              : wxSocketBase(flags, wxSOCKET_CLIENT)
{
    m_initialRecvBufferSize =
    m_initialSendBufferSize = -1;
}

// --------------------------------------------------------------------------
// Connect
// --------------------------------------------------------------------------

bool wxSocketClient::DoConnect(const wxSockAddress& remote,
                               const wxSockAddress* local,
                               bool wait)
{
    if ( m_impl )
    {
        // Shutdown and destroy the old socket
        Close();
        delete m_impl;
    }

    m_connected = false;
    m_establishing = false;

    // Create and set up the new one
    wxSocketManager * const manager = wxSocketManager::Get();
    m_impl = manager ? manager->CreateSocket(*this) : NULL;
    if ( !m_impl )
        return false;

    // Reuse makes sense for clients too, if we are trying to rebind to the same port
    if (GetFlags() & wxSOCKET_REUSEADDR)
        m_impl->SetReusable();
    if (GetFlags() & wxSOCKET_BROADCAST)
        m_impl->SetBroadcast();
    if (GetFlags() & wxSOCKET_NOBIND)
        m_impl->DontDoBind();

    // Bind to the local IP address and port, when provided or if one had been
    // set before
    if ( !local && m_localAddress.GetAddress().IsOk() )
        local = &m_localAddress;

    if ( local )
        m_impl->SetLocal(local->GetAddress());

    m_impl->SetInitialSocketBuffers(m_initialRecvBufferSize, m_initialSendBufferSize);

    m_impl->SetPeer(remote.GetAddress());

    // Finally do create the socket and connect to the peer
    const wxSocketError err = m_impl->CreateClient(wait);

    if ( err != wxSOCKET_NOERROR )
    {
        if ( err == wxSOCKET_WOULDBLOCK )
        {
            wxASSERT_MSG( !wait, "shouldn't get this for blocking connect" );

            m_establishing = true;
        }

        return false;
    }

    m_connected = true;
    return true;
}

bool wxSocketClient::Connect(const wxSockAddress& remote, bool wait)
{
    return DoConnect(remote, NULL, wait);
}

bool wxSocketClient::Connect(const wxSockAddress& remote,
                             const wxSockAddress& local,
                             bool wait)
{
    return DoConnect(remote, &local, wait);
}

bool wxSocketClient::WaitOnConnect(long seconds, long milliseconds)
{
    if ( m_connected )
    {
        // this happens if the initial attempt to connect succeeded without
        // blocking
        return true;
    }

    wxCHECK_MSG( m_establishing && m_impl, false,
                 "No connection establishment attempt in progress" );

    // notice that we return true even if DoWait() returned -1, i.e. if an
    // error occurred and connection was lost: this is intentional as we should
    // return false only if timeout expired without anything happening
    return DoWait(seconds, milliseconds, wxSOCKET_CONNECTION_FLAG) != 0;
}

// ==========================================================================
// wxDatagramSocket
// ==========================================================================

wxDatagramSocket::wxDatagramSocket( const wxSockAddress& addr,
                                    wxSocketFlags flags )
                : wxSocketBase( flags, wxSOCKET_DATAGRAM )
{
    // Create the socket
    wxSocketManager * const manager = wxSocketManager::Get();
    m_impl = manager ? manager->CreateSocket(*this) : NULL;

    if (!m_impl)
        return;

    // Setup the socket as non connection oriented
    m_impl->SetLocal(addr.GetAddress());
    if (flags & wxSOCKET_REUSEADDR)
    {
        m_impl->SetReusable();
    }
    if (GetFlags() & wxSOCKET_BROADCAST)
    {
        m_impl->SetBroadcast();
    }
    if (GetFlags() & wxSOCKET_NOBIND)
    {
        m_impl->DontDoBind();
    }

    if ( m_impl->CreateUDP() != wxSOCKET_NOERROR )
    {
        wxDELETE(m_impl);
        return;
    }

    // Initialize all stuff
    m_connected = false;
    m_establishing = false;
}

wxDatagramSocket& wxDatagramSocket::RecvFrom( wxSockAddress& addr,
                                              void* buf,
                                              wxUint32 nBytes )
{
    Read(buf, nBytes);
    GetPeer(addr);
    return (*this);
}

wxDatagramSocket& wxDatagramSocket::SendTo( const wxSockAddress& addr,
                                            const void* buf,
                                            wxUint32 nBytes )
{
    wxASSERT_MSG( m_impl, wxT("Socket not initialised") );

    m_impl->SetPeer(addr.GetAddress());
    Write(buf, nBytes);
    return (*this);
}

// ==========================================================================
// wxSocketModule
// ==========================================================================

class wxSocketModule : public wxModule
{
public:
    virtual bool OnInit()
    {
        // wxSocketBase will call Initialize() itself only if sockets are
        // really used, don't do it from here
        return true;
    }

    virtual void OnExit()
    {
        if ( wxSocketBase::IsInitialized() )
            wxSocketBase::Shutdown();
    }

private:
    DECLARE_DYNAMIC_CLASS(wxSocketModule)
};

IMPLEMENT_DYNAMIC_CLASS(wxSocketModule, wxModule)

#if defined(wxUSE_SELECT_DISPATCHER) && wxUSE_SELECT_DISPATCHER
// NOTE: we need to force linking against socketiohandler.cpp otherwise in
//       static builds of wxWidgets the ManagerSetter::ManagerSetter ctor
//       contained there wouldn't be ever called
wxFORCE_LINK_MODULE( socketiohandler )
#endif

// same for ManagerSetter in the MSW file
#ifdef __WINDOWS__
    wxFORCE_LINK_MODULE( mswsocket )
#endif

// and for OSXManagerSetter in the OS X one
#ifdef __WXOSX__
    wxFORCE_LINK_MODULE( osxsocket )
#endif

#endif // wxUSE_SOCKETS
