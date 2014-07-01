/////////////////////////////////////////////////////////////////////////////
// Name:        wx/sckipc.h
// Purpose:     Interprocess communication implementation (wxSocket version)
// Author:      Julian Smart
// Modified by: Guilhem Lavaux (big rewrite) May 1997, 1998
//              Guillermo Rodriguez (updated for wxSocket v2) Jan 2000
//                                  (callbacks deprecated)    Mar 2000
// Created:     1993
// Copyright:   (c) Julian Smart 1993
//              (c) Guilhem Lavaux 1997, 1998
//              (c) 2000 Guillermo Rodriguez <guille@iies.es>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SCKIPC_H
#define _WX_SCKIPC_H

#include "wx/defs.h"

#if wxUSE_SOCKETS && wxUSE_IPC

#include "wx/ipcbase.h"
#include "wx/socket.h"
#include "wx/sckstrm.h"
#include "wx/datstrm.h"

/*
 * Mini-DDE implementation

   Most transactions involve a topic name and an item name (choose these
   as befits your application).

   A client can:

   - ask the server to execute commands (data) associated with a topic
   - request data from server by topic and item
   - poke data into the server
   - ask the server to start an advice loop on topic/item
   - ask the server to stop an advice loop

   A server can:

   - respond to execute, request, poke and advice start/stop
   - send advise data to client

   Note that this limits the server in the ways it can send data to the
   client, i.e. it can't send unsolicited information.
 *
 */

class WXDLLIMPEXP_FWD_NET wxTCPServer;
class WXDLLIMPEXP_FWD_NET wxTCPClient;

class wxIPCSocketStreams;

class WXDLLIMPEXP_NET wxTCPConnection : public wxConnectionBase
{
public:
    wxTCPConnection() { Init(); }
    wxTCPConnection(void *buffer, size_t size)
        : wxConnectionBase(buffer, size)
    {
        Init();
    }

    virtual ~wxTCPConnection();

    // implement base class pure virtual methods
    virtual const void *Request(const wxString& item,
                                size_t *size = NULL,
                                wxIPCFormat format = wxIPC_TEXT);
    virtual bool StartAdvise(const wxString& item);
    virtual bool StopAdvise(const wxString& item);
    virtual bool Disconnect(void);

    // Will be used in the future to enable the compression but does nothing
    // for now.
    void Compress(bool on);


protected:
    virtual bool DoExecute(const void *data, size_t size, wxIPCFormat format);
    virtual bool DoPoke(const wxString& item, const void *data, size_t size,
                        wxIPCFormat format);
    virtual bool DoAdvise(const wxString& item, const void *data, size_t size,
                          wxIPCFormat format);


    // notice that all the members below are only initialized once the
    // connection is made, i.e. in MakeConnection() for the client objects and
    // after OnAcceptConnection() in the server ones

    // the underlying socket (wxSocketClient for IPC client and wxSocketServer
    // for IPC server)
    wxSocketBase *m_sock;

    // various streams that we use
    wxIPCSocketStreams *m_streams;

    // the topic of this connection
    wxString m_topic;

private:
    // common part of both ctors
    void Init();

    friend class wxTCPServer;
    friend class wxTCPClient;
    friend class wxTCPEventHandler;

    wxDECLARE_NO_COPY_CLASS(wxTCPConnection);
    DECLARE_DYNAMIC_CLASS(wxTCPConnection)
};

class WXDLLIMPEXP_NET wxTCPServer : public wxServerBase
{
public:
    wxTCPServer();
    virtual ~wxTCPServer();

    // Returns false on error (e.g. port number is already in use)
    virtual bool Create(const wxString& serverName);

    virtual wxConnectionBase *OnAcceptConnection(const wxString& topic);

protected:
    wxSocketServer *m_server;

#ifdef __UNIX_LIKE__
    // the name of the file associated to the Unix domain socket, may be empty
    wxString m_filename;
#endif // __UNIX_LIKE__

    wxDECLARE_NO_COPY_CLASS(wxTCPServer);
    DECLARE_DYNAMIC_CLASS(wxTCPServer)
};

class WXDLLIMPEXP_NET wxTCPClient : public wxClientBase
{
public:
    wxTCPClient();

    virtual bool ValidHost(const wxString& host);

    // Call this to make a connection. Returns NULL if cannot.
    virtual wxConnectionBase *MakeConnection(const wxString& host,
                                             const wxString& server,
                                             const wxString& topic);

    // Callbacks to CLIENT - override at will
    virtual wxConnectionBase *OnMakeConnection();

private:
    DECLARE_DYNAMIC_CLASS(wxTCPClient)
};

#endif // wxUSE_SOCKETS && wxUSE_IPC

#endif // _WX_SCKIPC_H
