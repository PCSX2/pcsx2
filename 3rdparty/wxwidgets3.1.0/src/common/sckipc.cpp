/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/sckipc.cpp
// Purpose:     Interprocess communication implementation (wxSocket version)
// Author:      Julian Smart
// Modified by: Guilhem Lavaux (big rewrite) May 1997, 1998
//              Guillermo Rodriguez (updated for wxSocket v2) Jan 2000
//                                  (callbacks deprecated)    Mar 2000
//              Vadim Zeitlin (added support for Unix sockets) Apr 2002
//                            (use buffering, many fixes/cleanup) Oct 2008
// Created:     1993
// Copyright:   (c) Julian Smart 1993
//              (c) Guilhem Lavaux 1997, 1998
//              (c) 2000 Guillermo Rodriguez <guille@iies.es>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ==========================================================================
// declarations
// ==========================================================================

// --------------------------------------------------------------------------
// headers
// --------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_SOCKETS && wxUSE_IPC && wxUSE_STREAMS

#include "wx/sckipc.h"

#ifndef WX_PRECOMP
    #include "wx/log.h"
    #include "wx/event.h"
    #include "wx/module.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "wx/socket.h"

// --------------------------------------------------------------------------
// macros and constants
// --------------------------------------------------------------------------

namespace
{

// Message codes (don't change them to avoid breaking the existing code using
// wxIPC protocol!)
enum IPCCode
{
    IPC_EXECUTE         = 1,
    IPC_REQUEST         = 2,
    IPC_POKE            = 3,
    IPC_ADVISE_START    = 4,
    IPC_ADVISE_REQUEST  = 5,
    IPC_ADVISE          = 6,
    IPC_ADVISE_STOP     = 7,
    IPC_REQUEST_REPLY   = 8,
    IPC_FAIL            = 9,
    IPC_CONNECT         = 10,
    IPC_DISCONNECT      = 11,
    IPC_MAX
};

} // anonymous namespace

// headers needed for umask()
#ifdef __UNIX_LIKE__
    #include <sys/types.h>
    #include <sys/stat.h>
#endif // __UNIX_LIKE__

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

// get the address object for the given server name, the caller must delete it
static wxSockAddress *
GetAddressFromName(const wxString& serverName,
                   const wxString& host = wxEmptyString)
{
    // we always use INET sockets under non-Unix systems
#if defined(__UNIX__) && !defined(__WINDOWS__) && !defined(__WINE__)
    // under Unix, if the server name looks like a path, create a AF_UNIX
    // socket instead of AF_INET one
    if ( serverName.Find(wxT('/')) != wxNOT_FOUND )
    {
        wxUNIXaddress *addr = new wxUNIXaddress;
        addr->Filename(serverName);

        return addr;
    }
#endif // Unix/!Unix
    {
        wxIPV4address *addr = new wxIPV4address;
        addr->Service(serverName);
        if ( !host.empty() )
        {
            addr->Hostname(host);
        }

        return addr;
    }
}

// --------------------------------------------------------------------------
// wxTCPEventHandler stuff (private class)
// --------------------------------------------------------------------------

class wxTCPEventHandler : public wxEvtHandler
{
public:
    wxTCPEventHandler() : wxEvtHandler() { }

    void Client_OnRequest(wxSocketEvent& event);
    void Server_OnRequest(wxSocketEvent& event);

private:
    void HandleDisconnect(wxTCPConnection *connection);

    wxDECLARE_EVENT_TABLE();
    wxDECLARE_NO_COPY_CLASS(wxTCPEventHandler);
};

enum
{
    _CLIENT_ONREQUEST_ID = 1000,
    _SERVER_ONREQUEST_ID
};

// --------------------------------------------------------------------------
// wxTCPEventHandlerModule (private class)
// --------------------------------------------------------------------------

class wxTCPEventHandlerModule : public wxModule
{
public:
    wxTCPEventHandlerModule() : wxModule() { }

    // get the global wxTCPEventHandler creating it if necessary
    static wxTCPEventHandler& GetHandler()
    {
        if ( !ms_handler )
            ms_handler = new wxTCPEventHandler;

        return *ms_handler;
    }

    // as ms_handler is initialized on demand, don't do anything in OnInit()
    virtual bool OnInit() wxOVERRIDE { return true; }
    virtual void OnExit() wxOVERRIDE { wxDELETE(ms_handler); }

private:
    static wxTCPEventHandler *ms_handler;

    wxDECLARE_DYNAMIC_CLASS(wxTCPEventHandlerModule);
    wxDECLARE_NO_COPY_CLASS(wxTCPEventHandlerModule);
};

wxIMPLEMENT_DYNAMIC_CLASS(wxTCPEventHandlerModule, wxModule);

wxTCPEventHandler *wxTCPEventHandlerModule::ms_handler = NULL;

// --------------------------------------------------------------------------
// wxIPCSocketStreams
// --------------------------------------------------------------------------

#define USE_BUFFER

// this class contains the various (related) streams used by wxTCPConnection
// and also provides a way to read from the socket stream directly
//
// for writing to the stream use the IPCOutput class below
class wxIPCSocketStreams
{
public:
    // ctor initializes all the streams on top of the given socket
    //
    // note that we use a bigger than default buffer size which matches the
    // typical Ethernet MTU (minus TCP header overhead)
    wxIPCSocketStreams(wxSocketBase& sock)
        : m_socketStream(sock),
#ifdef USE_BUFFER
          m_bufferedOut(m_socketStream, 1448),
#else
          m_bufferedOut(m_socketStream),
#endif
          m_dataIn(m_socketStream),
          m_dataOut(m_bufferedOut)
    {
    }

    // expose the IO methods needed by IPC code (notice that writing is only
    // done via IPCOutput)

    // flush output
    void Flush()
    {
#ifdef USE_BUFFER
        m_bufferedOut.Sync();
#endif
    }

    // simple wrappers around the functions with the same name in
    // wxDataInputStream
    wxUint8 Read8()
    {
        Flush();
        return m_dataIn.Read8();
    }

    wxUint32 Read32()
    {
        Flush();
        return m_dataIn.Read32();
    }

    wxString ReadString()
    {
        Flush();
        return m_dataIn.ReadString();
    }

    // read arbitrary (size-prepended) data
    //
    // connection parameter is needed to call its GetBufferAtLeast() method
    void *ReadData(wxConnectionBase *conn, size_t *size)
    {
        Flush();

        wxCHECK_MSG( conn, NULL, "NULL connection parameter" );
        wxCHECK_MSG( size, NULL, "NULL size parameter" );

        *size = Read32();

        void * const data = conn->GetBufferAtLeast(*size);
        wxCHECK_MSG( data, NULL, "IPC buffer allocation failed" );

        m_socketStream.Read(data, *size);

        return data;
    }

    // same as above but for data preceded by the format
    void *
    ReadFormatData(wxConnectionBase *conn, wxIPCFormat *format, size_t *size)
    {
        wxCHECK_MSG( format, NULL, "NULL format parameter" );

        *format = static_cast<wxIPCFormat>(Read8());

        return ReadData(conn, size);
    }


    // these methods are only used by IPCOutput and not directly
    wxDataOutputStream& GetDataOut() { return m_dataOut; }
    wxOutputStream& GetUnformattedOut() { return m_bufferedOut; }

private:
    // this is the low-level underlying stream using the connection socket
    wxSocketStream m_socketStream;

    // the buffered stream is used to avoid writing all pieces of an IPC
    // request to the socket one by one but to instead do it all at once when
    // we're done with it
#ifdef USE_BUFFER
    wxBufferedOutputStream m_bufferedOut;
#else
    wxOutputStream& m_bufferedOut;
#endif

    // finally the data streams are used to be able to write typed data into
    // the above streams easily
    wxDataInputStream  m_dataIn;
    wxDataOutputStream m_dataOut;

    wxDECLARE_NO_COPY_CLASS(wxIPCSocketStreams);
};

namespace
{

// an object of this class should be instantiated on the stack to write to the
// underlying socket stream
//
// this class is intentionally separated from wxIPCSocketStreams to ensure that
// Flush() is always called
class IPCOutput
{
public:
    // construct an object associated with the given streams (which must have
    // life time greater than ours as we keep a reference to it)
    IPCOutput(wxIPCSocketStreams *streams)
        : m_streams(*streams)
    {
        wxASSERT_MSG( streams, "NULL streams pointer" );
    }

    // dtor calls Flush() really sending the IPC data to the network
    ~IPCOutput() { m_streams.Flush(); }


    // write a byte
    void Write8(wxUint8 i)
    {
        m_streams.GetDataOut().Write8(i);
    }

    // write the reply code and a string
    void Write(IPCCode code, const wxString& str)
    {
        Write8(code);
        m_streams.GetDataOut().WriteString(str);
    }

    // write the reply code, a string and a format in this order
    void Write(IPCCode code, const wxString& str, wxIPCFormat format)
    {
        Write(code, str);
        Write8(format);
    }

    // write arbitrary data
    void WriteData(const void *data, size_t size)
    {
        m_streams.GetDataOut().Write32(size);
        m_streams.GetUnformattedOut().Write(data, size);
    }


private:
    wxIPCSocketStreams& m_streams;

    wxDECLARE_NO_COPY_CLASS(IPCOutput);
};

} // anonymous namespace

// ==========================================================================
// implementation
// ==========================================================================

wxIMPLEMENT_DYNAMIC_CLASS(wxTCPServer, wxServerBase);
wxIMPLEMENT_DYNAMIC_CLASS(wxTCPClient, wxClientBase);
wxIMPLEMENT_CLASS(wxTCPConnection, wxConnectionBase);

// --------------------------------------------------------------------------
// wxTCPClient
// --------------------------------------------------------------------------

wxTCPClient::wxTCPClient()
           : wxClientBase()
{
}

bool wxTCPClient::ValidHost(const wxString& host)
{
    wxIPV4address addr;

    return addr.Hostname(host);
}

wxConnectionBase *wxTCPClient::MakeConnection(const wxString& host,
                                              const wxString& serverName,
                                              const wxString& topic)
{
    wxSockAddress *addr = GetAddressFromName(serverName, host);
    if ( !addr )
        return NULL;

    wxSocketClient * const client = new wxSocketClient(wxSOCKET_WAITALL);
    wxIPCSocketStreams * const streams = new wxIPCSocketStreams(*client);

    bool ok = client->Connect(*addr);
    delete addr;

    if ( ok )
    {
        // Send topic name, and enquire whether this has succeeded
        IPCOutput(streams).Write(IPC_CONNECT, topic);

        unsigned char msg = streams->Read8();

        // OK! Confirmation.
        if (msg == IPC_CONNECT)
        {
            wxTCPConnection *
                connection = (wxTCPConnection *)OnMakeConnection ();

            if (connection)
            {
                if (wxDynamicCast(connection, wxTCPConnection))
                {
                    connection->m_topic = topic;
                    connection->m_sock  = client;
                    connection->m_streams = streams;
                    client->SetEventHandler(wxTCPEventHandlerModule::GetHandler(),
                                            _CLIENT_ONREQUEST_ID);
                    client->SetClientData(connection);
                    client->SetNotify(wxSOCKET_INPUT_FLAG | wxSOCKET_LOST_FLAG);
                    client->Notify(true);
                    return connection;
                }
                else
                {
                    delete connection;
                    // and fall through to delete everything else
                }
            }
        }
    }

    // Something went wrong, delete everything
    delete streams;
    client->Destroy();

    return NULL;
}

wxConnectionBase *wxTCPClient::OnMakeConnection()
{
    return new wxTCPConnection();
}

// --------------------------------------------------------------------------
// wxTCPServer
// --------------------------------------------------------------------------

wxTCPServer::wxTCPServer()
           : wxServerBase()
{
    m_server = NULL;
}

bool wxTCPServer::Create(const wxString& serverName)
{
    // Destroy previous server, if any
    if (m_server)
    {
        m_server->SetClientData(NULL);
        m_server->Destroy();
        m_server = NULL;
    }

    wxSockAddress *addr = GetAddressFromName(serverName);
    if ( !addr )
        return false;

#ifdef __UNIX_LIKE__
    mode_t umaskOld;
    if ( addr->Type() == wxSockAddress::UNIX )
    {
        // ensure that the file doesn't exist as otherwise calling socket()
        // would fail
        int rc = remove(serverName.fn_str());
        if ( rc < 0 && errno != ENOENT )
        {
            delete addr;

            return false;
        }

        // also set the umask to prevent the others from reading our file
        umaskOld = umask(077);
    }
    else
    {
        // unused anyhow but shut down the compiler warnings
        umaskOld = 0;
    }
#endif // __UNIX_LIKE__

    // Create a socket listening on the specified port (reusing it to allow
    // restarting the server listening on the same port as was used by the
    // previous instance of this server)
    m_server = new wxSocketServer(*addr, wxSOCKET_WAITALL | wxSOCKET_REUSEADDR);

#ifdef __UNIX_LIKE__
    if ( addr->Type() == wxSockAddress::UNIX )
    {
        // restore the umask
        umask(umaskOld);

        // save the file name to remove it later
        m_filename = serverName;
    }
#endif // __UNIX_LIKE__

    delete addr;

    if (!m_server->IsOk())
    {
        m_server->Destroy();
        m_server = NULL;

        return false;
    }

    m_server->SetEventHandler(wxTCPEventHandlerModule::GetHandler(),
                              _SERVER_ONREQUEST_ID);
    m_server->SetClientData(this);
    m_server->SetNotify(wxSOCKET_CONNECTION_FLAG);
    m_server->Notify(true);

    return true;
}

wxTCPServer::~wxTCPServer()
{
    if ( m_server )
    {
        m_server->SetClientData(NULL);
        m_server->Destroy();
    }

#ifdef __UNIX_LIKE__
    if ( !m_filename.empty() )
    {
        if ( remove(m_filename.fn_str()) != 0 )
        {
            wxLogDebug(wxT("Stale AF_UNIX file '%s' left."), m_filename.c_str());
        }
    }
#endif // __UNIX_LIKE__
}

wxConnectionBase *
wxTCPServer::OnAcceptConnection(const wxString& WXUNUSED(topic))
{
    return new wxTCPConnection();
}

// --------------------------------------------------------------------------
// wxTCPConnection
// --------------------------------------------------------------------------

void wxTCPConnection::Init()
{
    m_sock = NULL;
    m_streams = NULL;
}

wxTCPConnection::~wxTCPConnection()
{
    Disconnect();

    if ( m_sock )
    {
        m_sock->SetClientData(NULL);
        m_sock->Destroy();
    }

    delete m_streams;
}

void wxTCPConnection::Compress(bool WXUNUSED(on))
{
    // TODO
}

// Calls that CLIENT can make.
bool wxTCPConnection::Disconnect()
{
    if ( !GetConnected() )
        return true;

    // Send the disconnect message to the peer.
    IPCOutput(m_streams).Write8(IPC_DISCONNECT);

    if ( m_sock )
    {
        m_sock->Notify(false);
        m_sock->Close();
    }

    SetConnected(false);

    return true;
}

bool wxTCPConnection::DoExecute(const void *data,
                                size_t size,
                                wxIPCFormat format)
{
    if ( !m_sock->IsConnected() )
        return false;

    // Prepare EXECUTE message
    IPCOutput out(m_streams);
    out.Write8(IPC_EXECUTE);
    out.Write8(format);

    out.WriteData(data, size);

    return true;
}

const void *wxTCPConnection::Request(const wxString& item,
                                     size_t *size,
                                     wxIPCFormat format)
{
    if ( !m_sock->IsConnected() )
        return NULL;

    IPCOutput(m_streams).Write(IPC_REQUEST, item, format);

    const int ret = m_streams->Read8();
    if ( ret != IPC_REQUEST_REPLY )
        return NULL;

    // ReadData() needs a non-NULL size pointer but the client code can call us
    // with NULL pointer (this makes sense if it knows that it always works
    // with NUL-terminated strings)
    size_t sizeFallback;
    return m_streams->ReadData(this, size ? size : &sizeFallback);
}

bool wxTCPConnection::DoPoke(const wxString& item,
                             const void *data,
                             size_t size,
                             wxIPCFormat format)
{
    if ( !m_sock->IsConnected() )
        return false;

    IPCOutput out(m_streams);
    out.Write(IPC_POKE, item, format);
    out.WriteData(data, size);

    return true;
}

bool wxTCPConnection::StartAdvise(const wxString& item)
{
    if ( !m_sock->IsConnected() )
        return false;

    IPCOutput(m_streams).Write(IPC_ADVISE_START, item);

    const int ret = m_streams->Read8();

    return ret == IPC_ADVISE_START;
}

bool wxTCPConnection::StopAdvise (const wxString& item)
{
    if ( !m_sock->IsConnected() )
        return false;

    IPCOutput(m_streams).Write(IPC_ADVISE_STOP, item);

    const int ret = m_streams->Read8();

    return ret == IPC_ADVISE_STOP;
}

// Calls that SERVER can make
bool wxTCPConnection::DoAdvise(const wxString& item,
                               const void *data,
                               size_t size,
                               wxIPCFormat format)
{
    if ( !m_sock->IsConnected() )
        return false;

    IPCOutput out(m_streams);
    out.Write(IPC_ADVISE, item, format);
    out.WriteData(data, size);

    return true;
}

// --------------------------------------------------------------------------
// wxTCPEventHandler (private class)
// --------------------------------------------------------------------------

wxBEGIN_EVENT_TABLE(wxTCPEventHandler, wxEvtHandler)
    EVT_SOCKET(_CLIENT_ONREQUEST_ID, wxTCPEventHandler::Client_OnRequest)
    EVT_SOCKET(_SERVER_ONREQUEST_ID, wxTCPEventHandler::Server_OnRequest)
wxEND_EVENT_TABLE()

void wxTCPEventHandler::HandleDisconnect(wxTCPConnection *connection)
{
    // connection was closed (either gracefully or not): destroy everything
    connection->m_sock->Notify(false);
    connection->m_sock->Close();

    // don't leave references to this soon-to-be-dangling connection in the
    // socket as it won't be destroyed immediately as its destruction will be
    // delayed in case there are more events pending for it
    connection->m_sock->SetClientData(NULL);

    connection->SetConnected(false);
    connection->OnDisconnect();
}

void wxTCPEventHandler::Client_OnRequest(wxSocketEvent &event)
{
    wxSocketBase *sock = event.GetSocket();
    if (!sock)
        return;

    wxSocketNotify evt = event.GetSocketEvent();
    wxTCPConnection * const
        connection = static_cast<wxTCPConnection *>(sock->GetClientData());

    // This socket is being deleted; skip this event
    if (!connection)
        return;

    if ( evt == wxSOCKET_LOST )
    {
        HandleDisconnect(connection);
        return;
    }

    // Receive message number.
    wxIPCSocketStreams * const streams = connection->m_streams;

    const wxString topic = connection->m_topic;
    wxString item;

    bool error = false;

    const int msg = streams->Read8();
    switch ( msg )
    {
        case IPC_EXECUTE:
            {
                wxIPCFormat format;
                size_t size wxDUMMY_INITIALIZE(0);
                void * const
                    data = streams->ReadFormatData(connection, &format, &size);
                if ( data )
                    connection->OnExecute(topic, data, size, format);
                else
                    error = true;
            }
            break;

        case IPC_ADVISE:
            {
                item = streams->ReadString();

                wxIPCFormat format;
                size_t size wxDUMMY_INITIALIZE(0);
                void * const
                    data = streams->ReadFormatData(connection, &format, &size);

                if ( data )
                    connection->OnAdvise(topic, item, data, size, format);
                else
                    error = true;
            }
            break;

        case IPC_ADVISE_START:
            {
                item = streams->ReadString();

                IPCOutput(streams).Write8(connection->OnStartAdvise(topic, item)
                                            ? IPC_ADVISE_START
                                            : IPC_FAIL);
            }
            break;

        case IPC_ADVISE_STOP:
            {
                item = streams->ReadString();

                IPCOutput(streams).Write8(connection->OnStopAdvise(topic, item)
                                             ? IPC_ADVISE_STOP
                                             : IPC_FAIL);
            }
            break;

        case IPC_POKE:
            {
                item = streams->ReadString();
                wxIPCFormat format = (wxIPCFormat)streams->Read8();

                size_t size wxDUMMY_INITIALIZE(0);
                void * const data = streams->ReadData(connection, &size);

                if ( data )
                    connection->OnPoke(topic, item, data, size, format);
                else
                    error = true;
            }
            break;

        case IPC_REQUEST:
            {
                item = streams->ReadString();

                wxIPCFormat format = (wxIPCFormat)streams->Read8();

                size_t user_size = wxNO_LEN;
                const void *user_data = connection->OnRequest(topic,
                                                              item,
                                                              &user_size,
                                                              format);

                if ( !user_data )
                {
                    IPCOutput(streams).Write8(IPC_FAIL);
                    break;
                }

                IPCOutput out(streams);
                out.Write8(IPC_REQUEST_REPLY);

                if ( user_size == wxNO_LEN )
                {
                    switch ( format )
                    {
                        case wxIPC_TEXT:
                        case wxIPC_UTF8TEXT:
                            user_size = strlen((const char *)user_data) + 1;  // includes final NUL
                            break;
                        case wxIPC_UNICODETEXT:
                            user_size = (wcslen((const wchar_t *)user_data) + 1) * sizeof(wchar_t);  // includes final NUL
                            break;
                        default:
                            user_size = 0;
                    }
                }

                out.WriteData(user_data, user_size);
            }
            break;

        case IPC_DISCONNECT:
            HandleDisconnect(connection);
            break;

        case IPC_FAIL:
            wxLogDebug("Unexpected IPC_FAIL received");
            error = true;
            break;

        default:
            wxLogDebug("Unknown message code %d received.", msg);
            error = true;
            break;
    }

    if ( error )
        IPCOutput(streams).Write8(IPC_FAIL);
}

void wxTCPEventHandler::Server_OnRequest(wxSocketEvent &event)
{
    wxSocketServer *server = (wxSocketServer *) event.GetSocket();
    if (!server)
        return;
    wxTCPServer *ipcserv = (wxTCPServer *) server->GetClientData();

    // This socket is being deleted; skip this event
    if (!ipcserv)
        return;

    if (event.GetSocketEvent() != wxSOCKET_CONNECTION)
        return;

    // Accept the connection, getting a new socket
    wxSocketBase *sock = server->Accept();
    if (!sock)
        return;
    if (!sock->IsOk())
    {
        sock->Destroy();
        return;
    }

    wxIPCSocketStreams *streams = new wxIPCSocketStreams(*sock);

    {
        IPCOutput out(streams);

        const int msg = streams->Read8();
        if ( msg == IPC_CONNECT )
        {
            const wxString topic = streams->ReadString();

            wxTCPConnection *new_connection =
                (wxTCPConnection *)ipcserv->OnAcceptConnection (topic);

            if (new_connection)
            {
                if (wxDynamicCast(new_connection, wxTCPConnection))
                {
                    // Acknowledge success
                    out.Write8(IPC_CONNECT);

                    new_connection->m_sock = sock;
                    new_connection->m_streams = streams;
                    new_connection->m_topic = topic;
                    sock->SetEventHandler(wxTCPEventHandlerModule::GetHandler(),
                                          _CLIENT_ONREQUEST_ID);
                    sock->SetClientData(new_connection);
                    sock->SetNotify(wxSOCKET_INPUT_FLAG | wxSOCKET_LOST_FLAG);
                    sock->Notify(true);
                    return;
                }
                else
                {
                    delete new_connection;
                    // and fall through to delete everything else
                }
            }
        }

        // Something went wrong, send failure message and delete everything
        out.Write8(IPC_FAIL);
    } // IPCOutput object is destroyed here, before destroying stream

    delete streams;
    sock->Destroy();
}

#endif // wxUSE_SOCKETS && wxUSE_IPC && wxUSE_STREAMS
