/////////////////////////////////////////////////////////////////////////////
// Name:        wx/socket.h
// Purpose:     Socket handling classes
// Authors:     Guilhem Lavaux, Guillermo Rodriguez Garcia
// Modified by:
// Created:     April 1997
// Copyright:   (c) Guilhem Lavaux
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SOCKET_H_
#define _WX_SOCKET_H_

#include "wx/defs.h"

#if wxUSE_SOCKETS

// ---------------------------------------------------------------------------
// wxSocket headers
// ---------------------------------------------------------------------------

#include "wx/event.h"
#include "wx/sckaddr.h"
#include "wx/list.h"

class wxSocketImpl;

// ------------------------------------------------------------------------
// Types and constants
// ------------------------------------------------------------------------

// Define the type of native sockets.
#if defined(__WINDOWS__)
    // Although socket descriptors are still 32 bit values, even under Win64,
    // the socket type is 64 bit there.
    typedef wxUIntPtr wxSOCKET_T;
#else
    typedef int wxSOCKET_T;
#endif


// Types of different socket notifications or events.
//
// NB: the values here should be consecutive and start with 0 as they are
//     used to construct the wxSOCKET_XXX_FLAG bit mask values below
enum wxSocketNotify
{
    wxSOCKET_INPUT,
    wxSOCKET_OUTPUT,
    wxSOCKET_CONNECTION,
    wxSOCKET_LOST
};

enum
{
    wxSOCKET_INPUT_FLAG = 1 << wxSOCKET_INPUT,
    wxSOCKET_OUTPUT_FLAG = 1 << wxSOCKET_OUTPUT,
    wxSOCKET_CONNECTION_FLAG = 1 << wxSOCKET_CONNECTION,
    wxSOCKET_LOST_FLAG = 1 << wxSOCKET_LOST
};

// this is a combination of the bit masks defined above
typedef int wxSocketEventFlags;

enum wxSocketError
{
    wxSOCKET_NOERROR = 0,
    wxSOCKET_INVOP,
    wxSOCKET_IOERR,
    wxSOCKET_INVADDR,
    wxSOCKET_INVSOCK,
    wxSOCKET_NOHOST,
    wxSOCKET_INVPORT,
    wxSOCKET_WOULDBLOCK,
    wxSOCKET_TIMEDOUT,
    wxSOCKET_MEMERR,
    wxSOCKET_OPTERR
};

// socket options/flags bit masks
enum
{
    wxSOCKET_NONE           = 0x0000,
    wxSOCKET_NOWAIT_READ    = 0x0001,
    wxSOCKET_NOWAIT_WRITE   = 0x0002,
    wxSOCKET_NOWAIT         = wxSOCKET_NOWAIT_READ | wxSOCKET_NOWAIT_WRITE,
    wxSOCKET_WAITALL_READ   = 0x0004,
    wxSOCKET_WAITALL_WRITE  = 0x0008,
    wxSOCKET_WAITALL        = wxSOCKET_WAITALL_READ | wxSOCKET_WAITALL_WRITE,
    wxSOCKET_BLOCK          = 0x0010,
    wxSOCKET_REUSEADDR      = 0x0020,
    wxSOCKET_BROADCAST      = 0x0040,
    wxSOCKET_NOBIND         = 0x0080
};

typedef int wxSocketFlags;

// socket kind values (badly defined, don't use)
enum wxSocketType
{
    wxSOCKET_UNINIT,
    wxSOCKET_CLIENT,
    wxSOCKET_SERVER,
    wxSOCKET_BASE,
    wxSOCKET_DATAGRAM
};


// event
class WXDLLIMPEXP_FWD_NET wxSocketEvent;
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_NET, wxEVT_SOCKET, wxSocketEvent);

// --------------------------------------------------------------------------
// wxSocketBase
// --------------------------------------------------------------------------

class WXDLLIMPEXP_NET wxSocketBase : public wxObject
{
public:
    // Public interface
    // ----------------

    // ctors and dtors
    wxSocketBase();
    wxSocketBase(wxSocketFlags flags, wxSocketType type);
    virtual ~wxSocketBase();
    void Init();
    bool Destroy();

    // state
    bool Ok() const { return IsOk(); }
    bool IsOk() const { return m_impl != NULL; }
    bool Error() const { return LastError() != wxSOCKET_NOERROR; }
    bool IsClosed() const { return m_closed; }
    bool IsConnected() const { return m_connected; }
    bool IsData() { return WaitForRead(0, 0); }
    bool IsDisconnected() const { return !IsConnected(); }
    wxUint32 LastCount() const { return m_lcount; }
    wxUint32 LastReadCount() const { return m_lcount_read; }
    wxUint32 LastWriteCount() const { return m_lcount_write; }
    wxSocketError LastError() const;
    void SaveState();
    void RestoreState();

    // addresses
    virtual bool GetLocal(wxSockAddress& addr_man) const;
    virtual bool GetPeer(wxSockAddress& addr_man) const;
    virtual bool SetLocal(const wxIPV4address& local);

    // base IO
    virtual bool  Close();
    void ShutdownOutput();
    wxSocketBase& Discard();
    wxSocketBase& Peek(void* buffer, wxUint32 nbytes);
    wxSocketBase& Read(void* buffer, wxUint32 nbytes);
    wxSocketBase& ReadMsg(void *buffer, wxUint32 nbytes);
    wxSocketBase& Unread(const void *buffer, wxUint32 nbytes);
    wxSocketBase& Write(const void *buffer, wxUint32 nbytes);
    wxSocketBase& WriteMsg(const void *buffer, wxUint32 nbytes);

    // all Wait() functions wait until their condition is satisfied or the
    // timeout expires; if seconds == -1 (default) then m_timeout value is used
    //
    // it is also possible to call InterruptWait() to cancel any current Wait()

    // wait for anything at all to happen with this socket
    bool Wait(long seconds = -1, long milliseconds = 0);

    // wait until we can read from or write to the socket without blocking
    // (notice that this does not mean that the operation will succeed but only
    // that it will return immediately)
    bool WaitForRead(long seconds = -1, long milliseconds = 0);
    bool WaitForWrite(long seconds = -1, long milliseconds = 0);

    // wait until the connection is terminated
    bool WaitForLost(long seconds = -1, long milliseconds = 0);

    void InterruptWait() { m_interrupt = true; }


    wxSocketFlags GetFlags() const { return m_flags; }
    void SetFlags(wxSocketFlags flags);
    virtual void SetTimeout(long seconds);
    long GetTimeout() const { return m_timeout; }

    bool GetOption(int level, int optname, void *optval, int *optlen);
    bool SetOption(int level, int optname, const void *optval, int optlen);
    wxUint32 GetLastIOSize() const { return m_lcount; }
    wxUint32 GetLastIOReadSize() const { return m_lcount_read; }
    wxUint32 GetLastIOWriteSize() const { return m_lcount_write; }

    // event handling
    void *GetClientData() const { return m_clientData; }
    void SetClientData(void *data) { m_clientData = data; }
    void SetEventHandler(wxEvtHandler& handler, int id = wxID_ANY);
    void SetNotify(wxSocketEventFlags flags);
    void Notify(bool notify);

    // Get the underlying socket descriptor.
    wxSOCKET_T GetSocket() const;

    // initialize/shutdown the sockets (done automatically so there is no need
    // to call these functions usually)
    //
    // should always be called from the main thread only so one of the cases
    // where they should indeed be called explicitly is when the first wxSocket
    // object in the application is created in a different thread
    static bool Initialize();
    static void Shutdown();

    // check if wxSocket had been already initialized
    //
    // notice that this function should be only called from the main thread as
    // otherwise it is inherently unsafe because Initialize/Shutdown() may be
    // called concurrently with it in the main thread
    static bool IsInitialized();

    // Implementation from now on
    // --------------------------

    // do not use, should be private (called from wxSocketImpl only)
    void OnRequest(wxSocketNotify notify);

    // do not use, not documented nor supported
    bool IsNoWait() const { return ((m_flags & wxSOCKET_NOWAIT) != 0); }
    wxSocketType GetType() const { return m_type; }

private:
    friend class wxSocketClient;
    friend class wxSocketServer;
    friend class wxDatagramSocket;

    // low level IO
    wxUint32 DoRead(void* buffer, wxUint32 nbytes);
    wxUint32 DoWrite(const void *buffer, wxUint32 nbytes);

    // wait until the given flags are set for this socket or the given timeout
    // (or m_timeout) expires
    //
    // notice that wxSOCKET_LOST_FLAG is always taken into account and the
    // function returns -1 if the connection was lost; otherwise it returns
    // true if any of the events specified by flags argument happened or false
    // if the timeout expired
    int DoWait(long timeout, wxSocketEventFlags flags);

    // a helper calling DoWait() using the same convention as the public
    // WaitForXXX() functions use, i.e. use our timeout if seconds == -1 or the
    // specified timeout otherwise
    int DoWait(long seconds, long milliseconds, wxSocketEventFlags flags);

    // another helper calling DoWait() using our m_timeout
    int DoWaitWithTimeout(wxSocketEventFlags flags)
    {
        return DoWait(m_timeout*1000, flags);
    }

    // pushback buffer
    void     Pushback(const void *buffer, wxUint32 size);
    wxUint32 GetPushback(void *buffer, wxUint32 size, bool peek);

    // store the given error as the LastError()
    void SetError(wxSocketError error);

private:
    // socket
    wxSocketImpl *m_impl;             // port-specific implementation
    wxSocketType  m_type;             // wxSocket type

    // state
    wxSocketFlags m_flags;            // wxSocket flags
    bool          m_connected;        // connected?
    bool          m_establishing;     // establishing connection?
    bool          m_reading;          // busy reading?
    bool          m_writing;          // busy writing?
    bool          m_closed;           // was the other end closed?
    wxUint32      m_lcount;           // last IO transaction size
    wxUint32      m_lcount_read;      // last IO transaction size of Read() direction.
    wxUint32      m_lcount_write;     // last IO transaction size of Write() direction.
    unsigned long m_timeout;          // IO timeout value in seconds
                                      // (TODO: remove, wxSocketImpl has it too)
    wxList        m_states;           // stack of states (TODO: remove!)
    bool          m_interrupt;        // interrupt ongoing wait operations?
    bool          m_beingDeleted;     // marked for delayed deletion?
    wxIPV4address m_localAddress;     // bind to local address?

    // pushback buffer
    void         *m_unread;           // pushback buffer
    wxUint32      m_unrd_size;        // pushback buffer size
    wxUint32      m_unrd_cur;         // pushback pointer (index into buffer)

    // events
    int           m_id;               // socket id
    wxEvtHandler *m_handler;          // event handler
    void         *m_clientData;       // client data for events
    bool          m_notify;           // notify events to users?
    wxSocketEventFlags  m_eventmask;  // which events to notify?
    wxSocketEventFlags  m_eventsgot;  // collects events received in OnRequest()


    friend class wxSocketReadGuard;
    friend class wxSocketWriteGuard;

    wxDECLARE_NO_COPY_CLASS(wxSocketBase);
    DECLARE_CLASS(wxSocketBase)
};


// --------------------------------------------------------------------------
// wxSocketServer
// --------------------------------------------------------------------------

class WXDLLIMPEXP_NET wxSocketServer : public wxSocketBase
{
public:
    wxSocketServer(const wxSockAddress& addr,
                   wxSocketFlags flags = wxSOCKET_NONE);

    wxSocketBase* Accept(bool wait = true);
    bool AcceptWith(wxSocketBase& socket, bool wait = true);

    bool WaitForAccept(long seconds = -1, long milliseconds = 0);

    wxDECLARE_NO_COPY_CLASS(wxSocketServer);
    DECLARE_CLASS(wxSocketServer)
};


// --------------------------------------------------------------------------
// wxSocketClient
// --------------------------------------------------------------------------

class WXDLLIMPEXP_NET wxSocketClient : public wxSocketBase
{
public:
    wxSocketClient(wxSocketFlags flags = wxSOCKET_NONE);

    virtual bool Connect(const wxSockAddress& addr, bool wait = true);
    bool Connect(const wxSockAddress& addr,
                 const wxSockAddress& local,
                 bool wait = true);

    bool WaitOnConnect(long seconds = -1, long milliseconds = 0);

    // Sets initial socket buffer sizes using the SO_SNDBUF and SO_RCVBUF
    // options before calling connect (either one can be -1 to leave it
    // unchanged)
    void SetInitialSocketBuffers(int recv, int send)
    {
        m_initialRecvBufferSize = recv;
        m_initialSendBufferSize = send;
    }

private:
    virtual bool DoConnect(const wxSockAddress& addr,
                           const wxSockAddress* local,
                           bool wait = true);

    // buffer sizes, -1 if unset and defaults should be used
    int m_initialRecvBufferSize;
    int m_initialSendBufferSize;

    wxDECLARE_NO_COPY_CLASS(wxSocketClient);
    DECLARE_CLASS(wxSocketClient)
};


// --------------------------------------------------------------------------
// wxDatagramSocket
// --------------------------------------------------------------------------

// WARNING: still in alpha stage

class WXDLLIMPEXP_NET wxDatagramSocket : public wxSocketBase
{
public:
    wxDatagramSocket(const wxSockAddress& addr,
                     wxSocketFlags flags = wxSOCKET_NONE);

    wxDatagramSocket& RecvFrom(wxSockAddress& addr,
                               void *buf,
                               wxUint32 nBytes);
    wxDatagramSocket& SendTo(const wxSockAddress& addr,
                             const void* buf,
                             wxUint32 nBytes);

    /* TODO:
       bool Connect(wxSockAddress& addr);
     */

private:
    wxDECLARE_NO_COPY_CLASS(wxDatagramSocket);
    DECLARE_CLASS(wxDatagramSocket)
};


// --------------------------------------------------------------------------
// wxSocketEvent
// --------------------------------------------------------------------------

class WXDLLIMPEXP_NET wxSocketEvent : public wxEvent
{
public:
    wxSocketEvent(int id = 0)
        : wxEvent(id, wxEVT_SOCKET)
    {
    }

    wxSocketNotify GetSocketEvent() const { return m_event; }
    wxSocketBase *GetSocket() const
        { return (wxSocketBase *) GetEventObject(); }
    void *GetClientData() const { return m_clientData; }

    virtual wxEvent *Clone() const { return new wxSocketEvent(*this); }
    virtual wxEventCategory GetEventCategory() const { return wxEVT_CATEGORY_SOCKET; }

public:
    wxSocketNotify  m_event;
    void           *m_clientData;

    DECLARE_DYNAMIC_CLASS_NO_ASSIGN(wxSocketEvent)
};


typedef void (wxEvtHandler::*wxSocketEventFunction)(wxSocketEvent&);

#define wxSocketEventHandler(func) \
    wxEVENT_HANDLER_CAST(wxSocketEventFunction, func)

#define EVT_SOCKET(id, func) \
    wx__DECLARE_EVT1(wxEVT_SOCKET, id, wxSocketEventHandler(func))

#endif // wxUSE_SOCKETS

#endif // _WX_SOCKET_H_

