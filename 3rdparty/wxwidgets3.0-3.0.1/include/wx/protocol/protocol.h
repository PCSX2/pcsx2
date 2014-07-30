/////////////////////////////////////////////////////////////////////////////
// Name:        wx/protocol/protocol.h
// Purpose:     Protocol base class
// Author:      Guilhem Lavaux
// Modified by:
// Created:     10/07/1997
// Copyright:   (c) 1997, 1998 Guilhem Lavaux
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PROTOCOL_PROTOCOL_H
#define _WX_PROTOCOL_PROTOCOL_H

#include "wx/defs.h"

#if wxUSE_PROTOCOL

#include "wx/object.h"
#include "wx/string.h"
#include "wx/stream.h"

#if wxUSE_SOCKETS
    #include "wx/socket.h"
#endif

class WXDLLIMPEXP_FWD_NET wxProtocolLog;

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

typedef enum
{
    wxPROTO_NOERR = 0,
    wxPROTO_NETERR,
    wxPROTO_PROTERR,
    wxPROTO_CONNERR,
    wxPROTO_INVVAL,
    wxPROTO_NOHNDLR,
    wxPROTO_NOFILE,
    wxPROTO_ABRT,
    wxPROTO_RCNCT,
    wxPROTO_STREAMING
} wxProtocolError;

// ----------------------------------------------------------------------------
// wxProtocol: abstract base class for all protocols
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_NET wxProtocol
#if wxUSE_SOCKETS
 : public wxSocketClient
#else
 : public wxObject
#endif
{
public:
    wxProtocol();
    virtual ~wxProtocol();

#if wxUSE_SOCKETS
    bool Reconnect();
    virtual bool Connect( const wxString& WXUNUSED(host) ) { return false; }
    virtual bool Connect( const wxSockAddress& addr, bool WXUNUSED(wait) = true)
        { return wxSocketClient::Connect(addr); }

    // read a '\r\n' terminated line from the given socket and put it in
    // result (without the terminators)
    static wxProtocolError ReadLine(wxSocketBase *socket, wxString& result);

    // read a line from this socket - this one can be overridden in the
    // derived classes if different line termination convention is to be used
    virtual wxProtocolError ReadLine(wxString& result);
#endif // wxUSE_SOCKETS

    virtual bool Abort() = 0;
    virtual wxInputStream *GetInputStream(const wxString& path) = 0;
    virtual wxString GetContentType() const = 0;

    // the error code
    virtual wxProtocolError GetError() const { return m_lastError; }

    void SetUser(const wxString& user) { m_username = user; }
    void SetPassword(const wxString& passwd) { m_password = passwd; }

    virtual void SetDefaultTimeout(wxUint32 Value);

    // override wxSocketBase::SetTimeout function to avoid that the internal
    // m_uiDefaultTimeout goes out-of-sync:
    virtual void SetTimeout(long seconds)
        { SetDefaultTimeout(seconds); }


    // logging support: each wxProtocol object may have the associated logger
    // (by default there is none) which is used to log network requests and
    // responses

    // set the logger, deleting the old one and taking ownership of this one
    void SetLog(wxProtocolLog *log);

    // return the current logger, may be NULL
    wxProtocolLog *GetLog() const { return m_log; }

    // detach the existing logger without deleting it, the caller is
    // responsible for deleting the returned pointer if it's non-NULL
    wxProtocolLog *DetachLog()
    {
        wxProtocolLog * const log = m_log;
        m_log = NULL;
        return log;
    }

    // these functions forward to the same functions with the same names in
    // wxProtocolLog if we have a valid logger and do nothing otherwise
    void LogRequest(const wxString& str);
    void LogResponse(const wxString& str);

protected:
    // the timeout associated with the protocol:
    wxUint32        m_uiDefaultTimeout;

    wxString        m_username;
    wxString        m_password;

    // this must be always updated by the derived classes!
    wxProtocolError m_lastError;

private:
    wxProtocolLog *m_log;

    DECLARE_DYNAMIC_CLASS_NO_COPY(wxProtocol)
};

// ----------------------------------------------------------------------------
// macros for protocol classes
// ----------------------------------------------------------------------------

#define DECLARE_PROTOCOL(class) \
public: \
  static wxProtoInfo g_proto_##class;

#define IMPLEMENT_PROTOCOL(class, name, serv, host) \
wxProtoInfo class::g_proto_##class(name, serv, host, wxCLASSINFO(class)); \
bool wxProtocolUse##class = true;

#define USE_PROTOCOL(class) \
    extern bool wxProtocolUse##class ; \
    static struct wxProtocolUserFor##class \
    { \
        wxProtocolUserFor##class() { wxProtocolUse##class = true; } \
    } wxProtocolDoUse##class;

class WXDLLIMPEXP_NET wxProtoInfo : public wxObject
{
public:
    wxProtoInfo(const wxChar *name,
                const wxChar *serv_name,
                const bool need_host1,
                wxClassInfo *info);

protected:
    wxProtoInfo *next;
    wxString m_protoname;
    wxString prefix;
    wxString m_servname;
    wxClassInfo *m_cinfo;
    bool m_needhost;

    friend class wxURL;

    DECLARE_DYNAMIC_CLASS(wxProtoInfo)
    wxDECLARE_NO_COPY_CLASS(wxProtoInfo);
};

#endif // wxUSE_PROTOCOL

#endif // _WX_PROTOCOL_PROTOCOL_H
