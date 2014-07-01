/////////////////////////////////////////////////////////////////////////////
// Name:        wx/url.h
// Purpose:     URL parser
// Author:      Guilhem Lavaux
// Modified by: Ryan Norton
// Created:     20/07/1997
// Copyright:   (c) 1997, 1998 Guilhem Lavaux
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_URL_H
#define _WX_URL_H

#include "wx/defs.h"

#if wxUSE_URL

#include "wx/uri.h"
#include "wx/protocol/protocol.h"

#if wxUSE_PROTOCOL_HTTP
  #include "wx/protocol/http.h"
#endif

typedef enum {
  wxURL_NOERR = 0,
  wxURL_SNTXERR,
  wxURL_NOPROTO,
  wxURL_NOHOST,
  wxURL_NOPATH,
  wxURL_CONNERR,
  wxURL_PROTOERR
} wxURLError;

#if wxUSE_URL_NATIVE
class WXDLLIMPEXP_FWD_NET wxURL;

class WXDLLIMPEXP_NET wxURLNativeImp : public wxObject
{
public:
    virtual ~wxURLNativeImp() { }
    virtual wxInputStream *GetInputStream(wxURL *owner) = 0;
};
#endif // wxUSE_URL_NATIVE

class WXDLLIMPEXP_NET wxURL : public wxURI
{
public:
    wxURL(const wxString& sUrl = wxEmptyString);
    wxURL(const wxURI& uri);
    wxURL(const wxURL& url);
    virtual ~wxURL();

    wxURL& operator = (const wxString& url);
    wxURL& operator = (const wxURI& uri);
    wxURL& operator = (const wxURL& url);

    wxProtocol& GetProtocol()        { return *m_protocol; }
    wxURLError GetError() const      { return m_error; }
    wxString GetURL() const          { return m_url; }

    wxURLError SetURL(const wxString &url)
        { *this = url; return m_error; }

    bool IsOk() const
        { return m_error == wxURL_NOERR; }

    wxInputStream *GetInputStream();

#if wxUSE_PROTOCOL_HTTP
    static void SetDefaultProxy(const wxString& url_proxy);
    void SetProxy(const wxString& url_proxy);
#endif // wxUSE_PROTOCOL_HTTP

protected:
    static wxProtoInfo *ms_protocols;

#if wxUSE_PROTOCOL_HTTP
    static wxHTTP *ms_proxyDefault;
    static bool ms_useDefaultProxy;
    wxHTTP *m_proxy;
    bool m_useProxy;
#endif // wxUSE_PROTOCOL_HTTP

#if wxUSE_URL_NATIVE
    friend class wxURLNativeImp;
    // pointer to a native URL implementation object
    wxURLNativeImp *m_nativeImp;
    // Creates on the heap and returns a native
    // implementation object for the current platform.
    static wxURLNativeImp *CreateNativeImpObject();
#endif // wxUSE_URL_NATIVE

    wxProtoInfo *m_protoinfo;
    wxProtocol *m_protocol;

    wxURLError m_error;
    wxString m_url;

    void Init(const wxString&);
    bool ParseURL();
    void CleanData();
    void Free();
    bool FetchProtocol();

    friend class wxProtoInfo;
    friend class wxURLModule;

private:
    DECLARE_DYNAMIC_CLASS(wxURL)
};

#endif // wxUSE_URL

#endif // _WX_URL_H

