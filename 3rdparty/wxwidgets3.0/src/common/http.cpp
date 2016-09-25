/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/http.cpp
// Purpose:     HTTP protocol
// Author:      Guilhem Lavaux
// Modified by: Simo Virokannas (authentication, Dec 2005)
// Created:     August 1997
// Copyright:   (c) 1997, 1998 Guilhem Lavaux
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_PROTOCOL_HTTP

#include <stdio.h>
#include <stdlib.h>

#ifndef WX_PRECOMP
    #include "wx/string.h"
#endif

#include "wx/tokenzr.h"
#include "wx/socket.h"
#include "wx/protocol/protocol.h"
#include "wx/url.h"
#include "wx/protocol/http.h"
#include "wx/sckstrm.h"
#include "wx/thread.h"


// ----------------------------------------------------------------------------
// wxHTTP
// ----------------------------------------------------------------------------

IMPLEMENT_DYNAMIC_CLASS(wxHTTP, wxProtocol)
IMPLEMENT_PROTOCOL(wxHTTP, wxT("http"), wxT("80"), true)

wxHTTP::wxHTTP()
  : wxProtocol()
{
    m_addr = NULL;
    m_read = false;
    m_proxy_mode = false;
    m_http_response = 0;
}

wxHTTP::~wxHTTP()
{
    ClearHeaders();

    delete m_addr;
}

void wxHTTP::ClearHeaders()
{
    m_headers.clear();
}

void wxHTTP::ClearCookies()
{
    m_cookies.clear();
}

wxString wxHTTP::GetContentType() const
{
    return GetHeader(wxT("Content-Type"));
}

void wxHTTP::SetProxyMode(bool on)
{
    m_proxy_mode = on;
}

wxHTTP::wxHeaderIterator wxHTTP::FindHeader(const wxString& header)
{
    wxHeaderIterator it = m_headers.begin();
    for ( wxHeaderIterator en = m_headers.end(); it != en; ++it )
    {
        if ( header.CmpNoCase(it->first) == 0 )
            break;
    }

    return it;
}

wxHTTP::wxHeaderConstIterator wxHTTP::FindHeader(const wxString& header) const
{
    wxHeaderConstIterator it = m_headers.begin();
    for ( wxHeaderConstIterator en = m_headers.end(); it != en; ++it )
    {
        if ( header.CmpNoCase(it->first) == 0 )
            break;
    }

    return it;
}

wxHTTP::wxCookieIterator wxHTTP::FindCookie(const wxString& cookie)
{
    wxCookieIterator it = m_cookies.begin();
    for ( wxCookieIterator en = m_cookies.end(); it != en; ++it )
    {
        if ( cookie.CmpNoCase(it->first) == 0 )
            break;
    }

    return it;
}

wxHTTP::wxCookieConstIterator wxHTTP::FindCookie(const wxString& cookie) const
{
    wxCookieConstIterator it = m_cookies.begin();
    for ( wxCookieConstIterator en = m_cookies.end(); it != en; ++it )
    {
        if ( cookie.CmpNoCase(it->first) == 0 )
            break;
    }

    return it;
}

void wxHTTP::SetHeader(const wxString& header, const wxString& h_data)
{
    if (m_read) {
        ClearHeaders();
        m_read = false;
    }

    wxHeaderIterator it = FindHeader(header);
    if (it != m_headers.end())
        it->second = h_data;
    else
        m_headers[header] = h_data;
}

wxString wxHTTP::GetHeader(const wxString& header) const
{
    wxHeaderConstIterator it = FindHeader(header);

    return it == m_headers.end() ? wxGetEmptyString() : it->second;
}

wxString wxHTTP::GetCookie(const wxString& cookie) const
{
    wxCookieConstIterator it = FindCookie(cookie);

    return it == m_cookies.end() ? wxGetEmptyString() : it->second;
}

wxString wxHTTP::GenerateAuthString(const wxString& user, const wxString& pass) const
{
    // TODO: Use wxBase64Encode() now that we have it instead of reproducing it

    static const char *base64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    wxString buf;
    wxString toencode;

    buf.Printf(wxT("Basic "));

    toencode.Printf(wxT("%s:%s"),user.c_str(),pass.c_str());

    size_t len = toencode.length();
    const wxChar *from = toencode.c_str();
    while (len >= 3) { // encode full blocks first
        buf << wxString::Format(wxT("%c%c"), base64[(from[0] >> 2) & 0x3f], base64[((from[0] << 4) & 0x30) | ((from[1] >> 4) & 0xf)]);
        buf << wxString::Format(wxT("%c%c"), base64[((from[1] << 2) & 0x3c) | ((from[2] >> 6) & 0x3)], base64[from[2] & 0x3f]);
        from += 3;
        len -= 3;
    }
    if (len > 0) { // pad the remaining characters
        buf << wxString::Format(wxT("%c"), base64[(from[0] >> 2) & 0x3f]);
        if (len == 1) {
            buf << wxString::Format(wxT("%c="), base64[(from[0] << 4) & 0x30]);
        } else {
            buf << wxString::Format(wxT("%c%c"), base64[((from[0] << 4) & 0x30) | ((from[1] >> 4) & 0xf)], base64[(from[1] << 2) & 0x3c]);
        }
        buf << wxT("=");
    }

    return buf;
}

void wxHTTP::SetPostBuffer(const wxString& post_buf)
{
    // Use To8BitData() for backwards compatibility in this deprecated method.
    // The new code should use the other overload or SetPostText() and specify
    // the encoding to use for the text explicitly.
    wxScopedCharBuffer scb = post_buf.To8BitData();
    if ( scb.length() )
    {
        m_postBuffer.Clear();
        m_postBuffer.AppendData(scb.data(), scb.length());
    }
}

bool
wxHTTP::SetPostBuffer(const wxString& contentType,
                      const wxMemoryBuffer& data)
{
    m_postBuffer = data;
    m_contentType = contentType;

    return !m_postBuffer.IsEmpty();
}

bool
wxHTTP::SetPostText(const wxString& contentType,
                    const wxString& data,
                    const wxMBConv& conv)
{
#if wxUSE_UNICODE
    wxScopedCharBuffer scb = data.mb_str(conv);
    const size_t len = scb.length();
    const char* const buf = scb.data();
#else // !wxUSE_UNICODE
    const size_t len = data.length();
    const char* const buf = data.mb_str(conv);
#endif // wxUSE_UNICODE/!wxUSE_UNICODE

    if ( !len )
        return false;

    m_postBuffer.Clear();
    m_postBuffer.AppendData(buf, len);
    m_contentType = contentType;

    return true;
}

void wxHTTP::SendHeaders()
{
    typedef wxStringToStringHashMap::iterator iterator;
    wxString buf;

    for (iterator it = m_headers.begin(), en = m_headers.end(); it != en; ++it )
    {
        buf.Printf(wxT("%s: %s\r\n"), it->first.c_str(), it->second.c_str());

        const wxWX2MBbuf cbuf = buf.mb_str();
        Write(cbuf, strlen(cbuf));
    }
}

bool wxHTTP::ParseHeaders()
{
    wxString line;
    wxStringTokenizer tokenzr;

    ClearHeaders();
    ClearCookies();
    m_read = true;

    for ( ;; )
    {
        m_lastError = ReadLine(this, line);
        if (m_lastError != wxPROTO_NOERR)
            return false;

        if ( line.empty() )
            break;

        wxString left_str = line.BeforeFirst(':');
        if(!left_str.CmpNoCase("Set-Cookie"))
        {
            wxString cookieName = line.AfterFirst(':').Strip(wxString::both).BeforeFirst('=');
            wxString cookieValue = line.AfterFirst(':').Strip(wxString::both).AfterFirst('=').BeforeFirst(';');
            m_cookies[cookieName] = cookieValue;

            // For compatibility
            m_headers[left_str] = line.AfterFirst(':').Strip(wxString::both);
        }
        else
        {
            m_headers[left_str] = line.AfterFirst(':').Strip(wxString::both);
        }
    }
    return true;
}

bool wxHTTP::Connect(const wxString& host, unsigned short port)
{
    wxIPV4address *addr;

    if (m_addr) {
        wxDELETE(m_addr);
        Close();
    }

    m_addr = addr = new wxIPV4address();

    if (!addr->Hostname(host)) {
        wxDELETE(m_addr);
        m_lastError = wxPROTO_NETERR;
        return false;
    }

    if ( port )
        addr->Service(port);
    else if (!addr->Service(wxT("http")))
        addr->Service(80);

    wxString hostHdr = host;
    if ( port && port != 80 )
        hostHdr << wxT(":") << port;
    SetHeader(wxT("Host"), hostHdr);

    m_lastError = wxPROTO_NOERR;
    return true;
}

bool wxHTTP::Connect(const wxSockAddress& addr, bool WXUNUSED(wait))
{
    if (m_addr) {
        delete m_addr;
        Close();
    }

    m_addr = addr.Clone();

    wxIPV4address *ipv4addr = wxDynamicCast(&addr, wxIPV4address);
    if ( ipv4addr )
    {
        wxString hostHdr = ipv4addr->OrigHostname();
        unsigned short port = ipv4addr->Service();
        if ( port && port != 80 )
            hostHdr << wxT(":") << port;
        SetHeader(wxT("Host"), hostHdr);
    }

    m_lastError = wxPROTO_NOERR;
    return true;
}

bool wxHTTP::BuildRequest(const wxString& path, const wxString& method)
{
    // Use the data in the post buffer, if any.
    if ( !m_postBuffer.IsEmpty() )
    {
        wxString len;
        len << m_postBuffer.GetDataLen();

        // Content length must be correct, so always set, possibly
        // overriding the value set explicitly by a previous call to
        // SetHeader("Content-Length").
        SetHeader(wxS("Content-Length"), len);

        // However if the user had explicitly set the content type, don't
        // override it with the content type passed to SetPostText().
        if ( !m_contentType.empty() && GetContentType().empty() )
            SetHeader(wxS("Content-Type"), m_contentType);
    }

    m_http_response = 0;

    // If there is no User-Agent defined, define it.
    if ( GetHeader(wxT("User-Agent")).empty() )
        SetHeader(wxT("User-Agent"), wxT("wxWidgets 2.x"));

    // Send authentication information
    if (!m_username.empty() || !m_password.empty()) {
        SetHeader(wxT("Authorization"), GenerateAuthString(m_username, m_password));
    }

    wxString buf;
    buf.Printf(wxT("%s %s HTTP/1.0\r\n"), method, path);
    const wxWX2MBbuf pathbuf = buf.mb_str();
    Write(pathbuf, strlen(pathbuf));
    SendHeaders();
    Write("\r\n", 2);

    if ( !m_postBuffer.IsEmpty() ) {
        Write(m_postBuffer.GetData(), m_postBuffer.GetDataLen());

        m_postBuffer.Clear();
    }

    wxString tmp_str;
    m_lastError = ReadLine(this, tmp_str);
    if (m_lastError != wxPROTO_NOERR)
        return false;

    if (!tmp_str.Contains(wxT("HTTP/"))) {
        // TODO: support HTTP v0.9 which can have no header.
        // FIXME: tmp_str is not put back in the in-queue of the socket.
        m_lastError = wxPROTO_NOERR;
        SetHeader(wxT("Content-Length"), wxT("-1"));
        SetHeader(wxT("Content-Type"), wxT("none/none"));
        RestoreState();
        return true;
    }

    wxStringTokenizer token(tmp_str,wxT(' '));
    wxString tmp_str2;
    bool ret_value;

    token.NextToken();
    tmp_str2 = token.NextToken();

    m_http_response = wxAtoi(tmp_str2);

    switch ( tmp_str2[0u].GetValue() )
    {
        case wxT('1'):
            /* INFORMATION / SUCCESS */
            break;

        case wxT('2'):
            /* SUCCESS */
            break;

        case wxT('3'):
            /* REDIRECTION */
            break;

        default:
            m_lastError = wxPROTO_NOFILE;
            RestoreState();
            return false;
    }

    m_lastError = wxPROTO_NOERR;
    ret_value = ParseHeaders();

    return ret_value;
}

bool wxHTTP::Abort(void)
{
    return wxSocketClient::Close();
}

// ----------------------------------------------------------------------------
// wxHTTPStream and wxHTTP::GetInputStream
// ----------------------------------------------------------------------------

class wxHTTPStream : public wxSocketInputStream
{
public:
    wxHTTP *m_http;
    size_t m_httpsize;
    unsigned long m_read_bytes;

    wxHTTPStream(wxHTTP *http) : wxSocketInputStream(*http)
    {
        m_http = http;
        m_httpsize = 0;
        m_read_bytes = 0;
    }

    size_t GetSize() const { return m_httpsize; }
    virtual ~wxHTTPStream(void) { m_http->Abort(); }

protected:
    size_t OnSysRead(void *buffer, size_t bufsize);

    wxDECLARE_NO_COPY_CLASS(wxHTTPStream);
};

size_t wxHTTPStream::OnSysRead(void *buffer, size_t bufsize)
{
    if (m_read_bytes >= m_httpsize)
    {
        m_lasterror = wxSTREAM_EOF;
        return 0;
    }

    size_t ret = wxSocketInputStream::OnSysRead(buffer, bufsize);
    m_read_bytes += ret;

    if (m_httpsize==(size_t)-1 && m_lasterror == wxSTREAM_READ_ERROR )
    {
        // if m_httpsize is (size_t) -1 this means read until connection closed
        // which is equivalent to getting a READ_ERROR, for clients however this
        // must be translated into EOF, as it is the expected way of signalling
        // end end of the content
        m_lasterror = wxSTREAM_EOF;
    }

    return ret;
}

wxInputStream *wxHTTP::GetInputStream(const wxString& path)
{
    wxHTTPStream *inp_stream;

    wxString new_path;

    m_lastError = wxPROTO_CONNERR;  // all following returns share this type of error
    if (!m_addr)
        return NULL;

    // We set m_connected back to false so wxSocketBase will know what to do.
#ifdef __WXMAC__
    wxSocketClient::Connect(*m_addr , false );
    wxSocketClient::WaitOnConnect(10);

    if (!wxSocketClient::IsConnected())
        return NULL;
#else
    if (!wxProtocol::Connect(*m_addr))
        return NULL;
#endif

    // Use the user-specified method if any or determine the method to use
    // automatically depending on whether we have anything to post or not.
    wxString method = m_method;
    if (method.empty())
        method = m_postBuffer.IsEmpty() ? wxS("GET"): wxS("POST");

    if (!BuildRequest(path, method))
        return NULL;

    inp_stream = new wxHTTPStream(this);

    if (!GetHeader(wxT("Content-Length")).empty())
        inp_stream->m_httpsize = wxAtoi(GetHeader(wxT("Content-Length")));
    else
        inp_stream->m_httpsize = (size_t)-1;

    inp_stream->m_read_bytes = 0;

    // no error; reset m_lastError
    m_lastError = wxPROTO_NOERR;
    return inp_stream;
}

#endif // wxUSE_PROTOCOL_HTTP
