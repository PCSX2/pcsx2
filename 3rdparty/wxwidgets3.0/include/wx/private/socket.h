/////////////////////////////////////////////////////////////////////////////
// Name:        wx/private/socket.h
// Purpose:     wxSocketImpl and related declarations
// Authors:     Guilhem Lavaux, Vadim Zeitlin
// Created:     April 1997
// Copyright:   (c) 1997 Guilhem Lavaux
//              (c) 2008 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

/*
    Brief overview of different socket classes:

    - wxSocketBase is the public class representing a socket ("Base" here
      refers to the fact that wxSocketClient and wxSocketServer are derived
      from it and predates the convention of using "Base" for common base
      classes for platform-specific classes in wxWidgets) with implementation
      common to all platforms and forwarding methods whose implementation
      differs between platforms to wxSocketImpl which it contains.

    - wxSocketImpl is actually just an abstract base class having only code
      common to all platforms, the concrete implementation classes derive from
      it and are created by wxSocketImpl::Create().

    - Some socket operations have different implementations in console-mode and
      GUI applications. wxSocketManager class exists to abstract this in such
      way that console applications (using wxBase) don't depend on wxNet. An
      object of this class is made available via wxApp and GUI applications set
      up a different kind of global socket manager from console ones.

      TODO: it looks like wxSocketManager could be eliminated by providing
            methods for registering/unregistering sockets directly in
            wxEventLoop.
 */

#ifndef _WX_PRIVATE_SOCKET_H_
#define _WX_PRIVATE_SOCKET_H_

#include "wx/defs.h"

#if wxUSE_SOCKETS

#include "wx/socket.h"
#include "wx/private/sckaddr.h"

#include <stddef.h>

/*
   Including sys/types.h under Cygwin results in the warnings about "fd_set
   having been defined in sys/types.h" when winsock.h is included later and
   doesn't seem to be necessary anyhow. It's not needed under Mac neither.
 */
#if !defined(__WXMAC__) && !defined(__WXMSW__) && !defined(__WXWINCE__)
#include <sys/types.h>
#endif

#ifdef __WXWINCE__
#include <stdlib.h>
#endif

// include the header defining timeval: under Windows this struct is used only
// with sockets so we need to include winsock.h which we do via windows.h
#ifdef __WINDOWS__
    #include "wx/msw/wrapwin.h"
#else
    #include <sys/time.h>   // for timeval
#endif

// these definitions are for MSW when we don't use configure, otherwise these
// symbols are defined by configure
#ifndef WX_SOCKLEN_T
    #define WX_SOCKLEN_T int
#endif

#ifndef SOCKOPTLEN_T
    #define SOCKOPTLEN_T int
#endif

// define some symbols which winsock.h defines but traditional BSD headers
// don't
#ifndef INVALID_SOCKET
    #define INVALID_SOCKET (-1)
#endif

#ifndef SOCKET_ERROR
    #define SOCKET_ERROR (-1)
#endif

typedef int wxSocketEventFlags;

class wxSocketImpl;

/*
   Class providing hooks abstracting the differences between console and GUI
   applications for socket code.

   We also have different implementations of this class for different platforms
   allowing us to keep more things in the common code but the main reason for
   its existence is that we want the same socket code work differently
   depending on whether it's used from a console or a GUI program. This is
   achieved by implementing the virtual methods of this class differently in
   the objects returned by wxConsoleAppTraits::GetSocketManager() and the same
   method in wxGUIAppTraits.
 */
class wxSocketManager
{
public:
    // set the manager to use, we don't take ownership of it
    //
    // this should be called before creating the first wxSocket object,
    // otherwise the manager returned by wxAppTraits::GetSocketManager() will
    // be used
    static void Set(wxSocketManager *manager);

    // return the manager to use
    //
    // this initializes the manager at first use
    static wxSocketManager *Get()
    {
        if ( !ms_manager )
            Init();

        return ms_manager;
    }

    // called before the first wxSocket is created and should do the
    // initializations needed in order to use the network
    //
    // return true if initialized successfully; if this returns false sockets
    // can't be used at all
    virtual bool OnInit() = 0;

    // undo the initializations of OnInit()
    virtual void OnExit() = 0;


    // create the socket implementation object matching this manager
    virtual wxSocketImpl *CreateSocket(wxSocketBase& wxsocket) = 0;

    // these functions enable or disable monitoring of the given socket for the
    // specified events inside the currently running event loop (but notice
    // that both BSD and Winsock implementations actually use socket->m_server
    // value to determine what exactly should be monitored so it needs to be
    // set before calling these functions)
    //
    // the default event value is used just for the convenience of wxMSW
    // implementation which doesn't use this parameter anyhow, it doesn't make
    // sense to pass wxSOCKET_LOST for the Unix implementation which does use
    // this parameter
    virtual void Install_Callback(wxSocketImpl *socket,
                                  wxSocketNotify event = wxSOCKET_LOST) = 0;
    virtual void Uninstall_Callback(wxSocketImpl *socket,
                                    wxSocketNotify event = wxSOCKET_LOST) = 0;

    virtual ~wxSocketManager() { }

private:
    // get the manager to use if we don't have it yet
    static void Init();

    static wxSocketManager *ms_manager;
};

/*
    Base class for all socket implementations providing functionality common to
    BSD and Winsock sockets.

    Objects of this class are not created directly but only via the factory
    function wxSocketManager::CreateSocket().
 */
class wxSocketImpl
{
public:
    virtual ~wxSocketImpl();

    // set various socket properties: all of those can only be called before
    // creating the socket
    void SetTimeout(unsigned long millisec);
    void SetReusable() { m_reusable = true; }
    void SetBroadcast() { m_broadcast = true; }
    void DontDoBind() { m_dobind = false; }
    void SetInitialSocketBuffers(int recv, int send)
    {
        m_initialRecvBufferSize = recv;
        m_initialSendBufferSize = send;
    }

    wxSocketError SetLocal(const wxSockAddressImpl& address);
    wxSocketError SetPeer(const wxSockAddressImpl& address);

    // accessors
    // ---------

    bool IsServer() const { return m_server; }

    const wxSockAddressImpl& GetLocal(); // non const as may update m_local
    const wxSockAddressImpl& GetPeer() const { return m_peer; }

    wxSocketError GetError() const { return m_error; }
    bool IsOk() const { return m_error == wxSOCKET_NOERROR; }

    // get the error code corresponding to the last operation
    virtual wxSocketError GetLastError() const = 0;


    // creating/closing the socket
    // --------------------------

    // notice that SetLocal() must be called before creating the socket using
    // any of the functions below
    //
    // all of Create() functions return wxSOCKET_NOERROR if the operation
    // completed successfully or one of:
    //  wxSOCKET_INVSOCK - the socket is in use.
    //  wxSOCKET_INVADDR - the local (server) or peer (client) address has not
    //                     been set.
    //  wxSOCKET_IOERR   - any other error.

    // create a socket listening on the local address specified by SetLocal()
    // (notice that DontDoBind() is ignored by this function)
    wxSocketError CreateServer();

    // create a socket connected to the peer address specified by SetPeer()
    // (notice that DontDoBind() is ignored by this function)
    //
    // this function may return wxSOCKET_WOULDBLOCK in addition to the return
    // values listed above if wait is false
    wxSocketError CreateClient(bool wait);

    // create (and bind unless DontDoBind() had been called) an UDP socket
    // associated with the given local address
    wxSocketError CreateUDP();

    // may be called whether the socket was created or not, calls DoClose() if
    // it was indeed created
    void Close();

    // shuts down the writing end of the socket and closes it, this is a more
    // graceful way to close
    //
    // does nothing if the socket wasn't created
    void Shutdown();


    // IO operations
    // -------------

    // basic IO, work for both TCP and UDP sockets
    //
    // return the number of bytes read/written (possibly 0) or -1 on error
    int Read(void *buffer, int size);
    int Write(const void *buffer, int size);

    // basically a wrapper for select(): returns the condition of the socket,
    // blocking for not longer than timeout if it is specified (otherwise just
    // poll without blocking at all)
    //
    // flags defines what kind of conditions we're interested in, the return
    // value is composed of a (possibly empty) subset of the bits set in flags
    wxSocketEventFlags Select(wxSocketEventFlags flags,
                              const timeval *timeout = NULL);

    // convenient wrapper calling Select() with our default timeout
    wxSocketEventFlags SelectWithTimeout(wxSocketEventFlags flags)
    {
        return Select(flags, &m_timeout);
    }

    // just a wrapper for accept(): it is called to create a new wxSocketImpl
    // corresponding to a new server connection represented by the given
    // wxSocketBase, returns NULL on error (including immediately if there are
    // no pending connections as our sockets are non-blocking)
    wxSocketImpl *Accept(wxSocketBase& wxsocket);


    // notifications
    // -------------

    // notify m_wxsocket about the given socket event by calling its (inaptly
    // named) OnRequest() method
    void NotifyOnStateChange(wxSocketNotify event);

    // called after reading/writing the data from/to the socket and should
    // enable back the wxSOCKET_INPUT/OUTPUT_FLAG notifications if they were
    // turned off when this data was first detected
    virtual void ReenableEvents(wxSocketEventFlags flags) = 0;

    // TODO: make these fields protected and provide accessors for those of
    //       them that wxSocketBase really needs
//protected:
    wxSOCKET_T m_fd;

    int m_initialRecvBufferSize;
    int m_initialSendBufferSize;

    wxSockAddressImpl m_local,
                      m_peer;
    wxSocketError m_error;

    bool m_stream;
    bool m_establishing;
    bool m_reusable;
    bool m_broadcast;
    bool m_dobind;

    struct timeval m_timeout;

protected:
    wxSocketImpl(wxSocketBase& wxsocket);

    // get the associated socket flags
    wxSocketFlags GetSocketFlags() const { return m_wxsocket->GetFlags(); }

    // true if we're a listening stream socket
    bool m_server;

private:
    // called by Close() if we have a valid m_fd
    virtual void DoClose() = 0;

    // put this socket into non-blocking mode and enable monitoring this socket
    // as part of the event loop
    virtual void UnblockAndRegisterWithEventLoop() = 0;

    // check that the socket wasn't created yet and that the given address
    // (either m_local or m_peer depending on the socket kind) is valid and
    // set m_error and return false if this is not the case
    bool PreCreateCheck(const wxSockAddressImpl& addr);

    // set the given socket option: this just wraps setsockopt(SOL_SOCKET)
    int SetSocketOption(int optname, int optval)
    {
        // although modern Unix systems use "const void *" for the 4th
        // parameter here, old systems and Winsock still use "const char *"
        return setsockopt(m_fd, SOL_SOCKET, optname,
                          reinterpret_cast<const char *>(&optval),
                          sizeof(optval));
    }

    // set the given socket option to true value: this is an even simpler
    // wrapper for setsockopt(SOL_SOCKET) for boolean options
    int EnableSocketOption(int optname)
    {
        return SetSocketOption(optname, 1);
    }

    // apply the options to the (just created) socket and register it with the
    // event loop by calling UnblockAndRegisterWithEventLoop()
    void PostCreation();

    // update local address after binding/connecting
    wxSocketError UpdateLocalAddress();

    // functions used to implement Read/Write()
    int RecvStream(void *buffer, int size);
    int RecvDgram(void *buffer, int size);
    int SendStream(const void *buffer, int size);
    int SendDgram(const void *buffer, int size);


    // set in ctor and never changed except that it's reset to NULL when the
    // socket is shut down
    wxSocketBase *m_wxsocket;

    wxDECLARE_NO_COPY_CLASS(wxSocketImpl);
};

#if defined(__WINDOWS__)
    #include "wx/msw/private/sockmsw.h"
#else
    #include "wx/unix/private/sockunix.h"
#endif

#endif /* wxUSE_SOCKETS */

#endif /* _WX_PRIVATE_SOCKET_H_ */
