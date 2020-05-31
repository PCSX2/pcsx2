/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/sckaddr.cpp
// Purpose:     Network address manager
// Author:      Guilhem Lavaux
// Created:     26/04/97
// Modified by: Vadim Zeitlin to use wxSockAddressImpl on 2008-12-28
// Copyright:   (c) 1997, 1998 Guilhem Lavaux
//              (c) 2008 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_SOCKETS

#ifndef WX_PRECOMP
    #include "wx/object.h"
    #include "wx/log.h"
    #include "wx/intl.h"
    #include "wx/thread.h"

    #include <stdio.h>
    #include <stdlib.h>
    #include <ctype.h>

    #include <memory.h>
#endif // !WX_PRECOMP

#include "wx/socket.h"
#include "wx/sckaddr.h"
#include "wx/private/socket.h"
#include "wx/private/sckaddr.h"

#include <errno.h>

#if defined(__UNIX__) && !defined(__WINDOWS__)
    #include <netdb.h>
    #include <arpa/inet.h>
#endif // __UNIX__

#ifndef INADDR_NONE
    #define INADDR_NONE INADDR_ANY
#endif

// ----------------------------------------------------------------------------
// wxRTTI macros
// ----------------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(wxSockAddress, wxObject)
IMPLEMENT_ABSTRACT_CLASS(wxIPaddress, wxSockAddress)
IMPLEMENT_DYNAMIC_CLASS(wxIPV4address, wxIPaddress)
#if wxUSE_IPV6
IMPLEMENT_DYNAMIC_CLASS(wxIPV6address, wxIPaddress)
#endif
#if defined(__UNIX__) && !defined(__WINDOWS__) && !defined(__WINE__)
IMPLEMENT_DYNAMIC_CLASS(wxUNIXaddress, wxSockAddress)
#endif

// ============================================================================
// implementation of thread-safe/reentrant functions if they're missing
// ============================================================================

// TODO: use POSIX getaddrinfo() (also available in Winsock 2) for simplicity
//       and to use the same code for IPv4 and IPv6 support

#ifdef __WINDOWS__
    #define HAVE_INET_ADDR

    #ifndef HAVE_GETHOSTBYNAME
    #define HAVE_GETHOSTBYNAME
    #endif
    #ifndef HAVE_GETSERVBYNAME
    #define HAVE_GETSERVBYNAME
    #endif

    // under MSW getxxxbyname() functions are MT-safe (but not reentrant) so
    // we don't need to serialize calls to them
    #define wxHAS_MT_SAFE_GETBY_FUNCS

    #if wxUSE_IPV6
        #ifdef __VISUALC__
            // this header does dynamic dispatching of getaddrinfo/freeaddrinfo()
            // by implementing them in its own code if the system versions are
            // not available (as is the case for anything < XP)
            #pragma warning(push)
            #pragma warning(disable:4706)
            #include <wspiapi.h>
            #pragma warning(pop)
        #else
            // TODO: Use wxDynamicLibrary to bind to these functions
            //       dynamically on older Windows systems, currently a program
            //       built with wxUSE_IPV6==1 won't even start there, even if
            //       it doesn't actually use the socket stuff.
            #include <ws2tcpip.h>
        #endif
    #endif
#endif // __WINDOWS__

// we assume that we have gethostbyaddr_r() if and only if we have
// gethostbyname_r() and that it uses the similar conventions to it (see
// comment in configure)
#define HAVE_GETHOSTBYADDR HAVE_GETHOSTBYNAME
#ifdef HAVE_FUNC_GETHOSTBYNAME_R_3
    #define HAVE_FUNC_GETHOSTBYADDR_R_3
#endif
#ifdef HAVE_FUNC_GETHOSTBYNAME_R_5
    #define HAVE_FUNC_GETHOSTBYADDR_R_5
#endif
#ifdef HAVE_FUNC_GETHOSTBYNAME_R_6
    #define HAVE_FUNC_GETHOSTBYADDR_R_6
#endif

// the _r functions need the extra buffer parameter but unfortunately its type
// differs between different systems and for the systems which use opaque
// structs for it (at least AIX and OpenBSD) it must be zero-filled before
// being passed to the system functions
#ifdef HAVE_FUNC_GETHOSTBYNAME_R_3
    struct wxGethostBuf : hostent_data
    {
        wxGethostBuf()
        {
            memset(this, 0, sizeof(hostent_data));
        }
    };
#else
    typedef char wxGethostBuf[4096];
#endif

#ifdef HAVE_FUNC_GETSERVBYNAME_R_4
    struct wxGetservBuf : servent_data
    {
        wxGetservBuf()
        {
            memset(this, 0, sizeof(servent_data));
        }
    };
#else
    typedef char wxGetservBuf[4096];
#endif

#if defined(wxHAS_MT_SAFE_GETBY_FUNCS) || !wxUSE_THREADS
    #define wxLOCK_GETBY_MUTEX(name)
#else // may need mutexes to protect getxxxbyxxx() calls
    #if defined(HAVE_GETHOSTBYNAME) || \
        defined(HAVE_GETHOSTBYADDR) || \
        defined(HAVE_GETSERVBYNAME)
        #include "wx/thread.h"

        namespace
        {
            // these mutexes are used to serialize
            wxMutex nameLock,   // gethostbyname()
                    addrLock,   // gethostbyaddr()
                    servLock;   // getservbyname()
        }

        #define wxLOCK_GETBY_MUTEX(name) wxMutexLocker locker(name ## Lock)
    #endif // we don't have _r functions
#endif // wxUSE_THREADS

namespace
{

#if defined(HAVE_GETHOSTBYNAME)
hostent *deepCopyHostent(hostent *h,
                         const hostent *he,
                         char *buffer,
                         int size,
                         int *err)
{
    /* copy old structure */
    memcpy(h, he, sizeof(hostent));

    /* copy name */
    int len = strlen(h->h_name);
    if (len > size)
    {
        *err = ENOMEM;
        return NULL;
    }
    memcpy(buffer, h->h_name, len);
    buffer[len] = '\0';
    h->h_name = buffer;

    /* track position in the buffer */
    int pos = len + 1;

    /* reuse len to store address length */
    len = h->h_length;

    /* ensure pointer alignment */
    unsigned int misalign = sizeof(char *) - pos%sizeof(char *);
    if(misalign < sizeof(char *))
        pos += misalign;

    /* leave space for pointer list */
    char **p = h->h_addr_list, **q;
    char **h_addr_list = (char **)(buffer + pos);
    while(*(p++) != 0)
        pos += sizeof(char *);

    /* copy addresses and fill new pointer list */
    for (p = h->h_addr_list, q = h_addr_list; *p != 0; p++, q++)
    {
        if (size < pos + len)
        {
            *err = ENOMEM;
            return NULL;
        }
        memcpy(buffer + pos, *p, len); /* copy content */
        *q = buffer + pos; /* set copied pointer to copied content */
        pos += len;
    }
    *++q = 0; /* null terminate the pointer list */
    h->h_addr_list = h_addr_list; /* copy pointer to pointers */

    /* ensure word alignment of pointers */
    misalign = sizeof(char *) - pos%sizeof(char *);
    if(misalign < sizeof(char *))
        pos += misalign;

    /* leave space for pointer list */
    p = h->h_aliases;
    char **h_aliases = (char **)(buffer + pos);
    while(*(p++) != 0)
        pos += sizeof(char *);

    /* copy aliases and fill new pointer list */
    for (p = h->h_aliases, q = h_aliases; *p != 0; p++, q++)
    {
        len = strlen(*p);
        if (size <= pos + len)
        {
            *err = ENOMEM;
            return NULL;
        }
        memcpy(buffer + pos, *p, len); /* copy content */
        buffer[pos + len] = '\0';
        *q = buffer + pos; /* set copied pointer to copied content */
        pos += len + 1;
    }
    *++q = 0; /* null terminate the pointer list */
    h->h_aliases = h_aliases; /* copy pointer to pointers */

    return h;
}
#endif // HAVE_GETHOSTBYNAME

hostent *wxGethostbyname_r(const char *hostname,
                           hostent *h,
                           wxGethostBuf buffer,
                           int size,
                           int *err)
{
    hostent *he;
#if defined(HAVE_FUNC_GETHOSTBYNAME_R_6)
    gethostbyname_r(hostname, h, buffer, size, &he, err);
#elif defined(HAVE_FUNC_GETHOSTBYNAME_R_5)
    he = gethostbyname_r(hostname, h, buffer, size, err);
#elif defined(HAVE_FUNC_GETHOSTBYNAME_R_3)
    wxUnusedVar(var);
    *err = gethostbyname_r(hostname, h,  &buffer);
    he = h;
#elif defined(HAVE_GETHOSTBYNAME)
    wxLOCK_GETBY_MUTEX(name);

    he = gethostbyname(hostname);
    *err = h_errno;

    if ( he )
        he = deepCopyHostent(h, he, buffer, size, err);
#else
    #error "No gethostbyname[_r]()"
#endif

    return he;
}

hostent *wxGethostbyaddr_r(const char *addr_buf,
                           int buf_size,
                           int proto,
                           hostent *h,
                           wxGethostBuf buffer,
                           int size,
                           int *err)
{
    hostent *he;
#if defined(HAVE_FUNC_GETHOSTBYADDR_R_6)
    gethostbyaddr_r(addr_buf, buf_size, proto, h, buffer, size, &he, err);
#elif defined(HAVE_FUNC_GETHOSTBYADDR_R_5)
    he = gethostbyaddr_r(addr_buf, buf_size, proto, h, buffer, size, err);
#elif defined(HAVE_FUNC_GETHOSTBYADDR_R_3)
    wxUnusedVar(size);
    *err = gethostbyaddr_r(addr_buf, buf_size, proto, h, &buffer);
    he = h;
#elif defined(HAVE_GETHOSTBYADDR)
    wxLOCK_GETBY_MUTEX(addr);

    he = gethostbyaddr(addr_buf, buf_size, proto);
    *err = h_errno;

    if ( he )
        he = deepCopyHostent(h, he, buffer, size, err);
#else
    #error "No gethostbyaddr[_r]()"
#endif

    return he;
}

#if defined(HAVE_GETSERVBYNAME)
servent *deepCopyServent(servent *s,
                         servent *se,
                         char *buffer,
                         int size)
{
    /* copy plain old structure */
    memcpy(s, se, sizeof(servent));

    /* copy name */
    int len = strlen(s->s_name);
    if (len >= size)
    {
        return NULL;
    }
    memcpy(buffer, s->s_name, len);
    buffer[len] = '\0';
    s->s_name = buffer;

    /* track position in the buffer */
    int pos = len + 1;

    /* copy protocol */
    len = strlen(s->s_proto);
    if (pos + len >= size)
    {
        return NULL;
    }
    memcpy(buffer + pos, s->s_proto, len);
    buffer[pos + len] = '\0';
    s->s_proto = buffer + pos;

    /* track position in the buffer */
    pos += len + 1;

    /* ensure pointer alignment */
    unsigned int misalign = sizeof(char *) - pos%sizeof(char *);
    if(misalign < sizeof(char *))
        pos += misalign;

    /* leave space for pointer list */
    char **p = s->s_aliases, **q;
    char **s_aliases = (char **)(buffer + pos);
    while(*(p++) != 0)
        pos += sizeof(char *);

    /* copy addresses and fill new pointer list */
    for (p = s->s_aliases, q = s_aliases; *p != 0; p++, q++){
        len = strlen(*p);
        if (size <= pos + len)
        {
            return NULL;
        }
        memcpy(buffer + pos, *p, len); /* copy content */
        buffer[pos + len] = '\0';
        *q = buffer + pos; /* set copied pointer to copied content */
        pos += len + 1;
    }
    *++q = 0; /* null terminate the pointer list */
    s->s_aliases = s_aliases; /* copy pointer to pointers */
    return s;
}
#endif // HAVE_GETSERVBYNAME

servent *wxGetservbyname_r(const char *port,
                           const char *protocol,
                           servent *serv,
                           wxGetservBuf& buffer,
                           int size)
{
    servent *se;
#if defined(HAVE_FUNC_GETSERVBYNAME_R_6)
    getservbyname_r(port, protocol, serv, buffer, size, &se);
#elif defined(HAVE_FUNC_GETSERVBYNAME_R_5)
    se = getservbyname_r(port, protocol, serv, buffer, size);
#elif defined(HAVE_FUNC_GETSERVBYNAME_R_4)
    wxUnusedVar(size);
    if ( getservbyname_r(port, protocol, serv, &buffer) != 0 )
        return NULL;
#elif defined(HAVE_GETSERVBYNAME)
    wxLOCK_GETBY_MUTEX(serv);

    se = getservbyname(port, protocol);
    if ( se )
        se = deepCopyServent(serv, se, buffer, size);
#else
    #error "No getservbyname[_r]()"
#endif
    return se;
}

} // anonymous namespace

// ============================================================================
// wxSockAddressImpl implementation
// ============================================================================

// FIXME-VC6: helper macros to call Alloc/Get() hiding the ugly dummy argument
#define ALLOC(T) Alloc(static_cast<T *>(NULL))
#define GET(T) Get(static_cast<T *>(NULL))

// ----------------------------------------------------------------------------
// INET or INET6 address family
// ----------------------------------------------------------------------------

wxString wxSockAddressImpl::GetHostName() const
{
    const void *addrbuf;
    int addrbuflen;

#if wxUSE_IPV6
    if ( m_family == FAMILY_INET6 )
    {
        sockaddr_in6 * const addr6 = GET(sockaddr_in6);
        addrbuf = &addr6->sin6_addr;
        addrbuflen = sizeof(addr6->sin6_addr);
    }
    else
#endif // wxUSE_IPV6
    {
        sockaddr_in * const addr = GET(sockaddr_in);
        if ( !addr )
            return wxString();

        addrbuf = &addr->sin_addr;
        addrbuflen = sizeof(addr->sin_addr);
    }

    hostent he;
    wxGethostBuf buffer;
    int err;
    if ( !wxGethostbyaddr_r
          (
            static_cast<const char *>(addrbuf),
            addrbuflen,
            m_family,
            &he,
            buffer,
            sizeof(buffer),
            &err
          ) )
    {
        return wxString();
    }

    return wxString::FromUTF8(he.h_name);
}

bool wxSockAddressImpl::SetPortName(const wxString& name, const char *protocol)
{
    // test whether it's a number first
    unsigned long port;
    if ( name.ToULong(&port) )
    {
        if ( port > 65535 )
            return false;
    }
    else // it's a service name
    {
        wxGetservBuf buffer;
        servent se;
        if ( !wxGetservbyname_r(name.utf8_str(), protocol, &se,
                                buffer, sizeof(buffer)) )
            return false;

        // s_port is in network byte order and SetPort() uses the host byte
        // order and we prefer to reuse it from here instead of assigning to
        // sin_port directly
        port = ntohs(se.s_port);
    }

    return SetPort(port);
}

// ----------------------------------------------------------------------------
// INET address family
// ----------------------------------------------------------------------------

void wxSockAddressImpl::CreateINET()
{
    wxASSERT_MSG( Is(FAMILY_UNSPEC), "recreating address as different type?" );

    m_family = FAMILY_INET;
    sockaddr_in * const addr = ALLOC(sockaddr_in);
    addr->sin_family = FAMILY_INET;
}

bool wxSockAddressImpl::SetHostName4(const wxString& name)
{
    sockaddr_in * const addr = GET(sockaddr_in);
    if ( !addr )
        return false;

    const wxScopedCharBuffer namebuf(name.utf8_str());

    // first check if this is an address in quad dotted notation
#if defined(HAVE_INET_ATON)
    if ( inet_aton(namebuf, &addr->sin_addr) )
        return true;
#elif defined(HAVE_INET_ADDR)
    addr->sin_addr.s_addr = inet_addr(namebuf);
    if ( addr->sin_addr.s_addr != INADDR_NONE )
        return true;
#else
    #error "Neither inet_aton() nor inet_addr() is available?"
#endif

    // it's a host name, resolve it
    hostent he;
    wxGethostBuf buffer;
    int err;
    if ( !wxGethostbyname_r(namebuf, &he, buffer, sizeof(buffer), &err) )
        return false;

    addr->sin_addr.s_addr = ((in_addr *)he.h_addr)->s_addr;
    return true;
}

bool wxSockAddressImpl::GetHostAddress(wxUint32 *address) const
{
    sockaddr_in * const addr = GET(sockaddr_in);
    if ( !addr )
        return false;

    *address = ntohl(addr->sin_addr.s_addr);

    return true;
}

bool wxSockAddressImpl::SetHostAddress(wxUint32 address)
{
    sockaddr_in * const addr = GET(sockaddr_in);
    if ( !addr )
        return false;

    addr->sin_addr.s_addr = htonl(address);

    return true;
}

wxUint16 wxSockAddressImpl::GetPort4() const
{
    sockaddr_in * const addr = GET(sockaddr_in);
    if ( !addr )
        return 0;

    return ntohs(addr->sin_port);
}

bool wxSockAddressImpl::SetPort4(wxUint16 port)
{
    sockaddr_in * const addr = GET(sockaddr_in);
    if ( !addr )
        return false;

    addr->sin_port = htons(port);

    return true;
}

#if wxUSE_IPV6

// ----------------------------------------------------------------------------
// INET6 address family
// ----------------------------------------------------------------------------

void wxSockAddressImpl::CreateINET6()
{
    wxASSERT_MSG( Is(FAMILY_UNSPEC), "recreating address as different type?" );

    m_family = FAMILY_INET6;
    sockaddr_in6 * const addr = ALLOC(sockaddr_in6);
    addr->sin6_family = FAMILY_INET6;
}

bool wxSockAddressImpl::SetHostName6(const wxString& hostname)
{
    sockaddr_in6 * const addr = GET(sockaddr_in6);
    if ( !addr )
        return false;

    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;

    addrinfo *info = NULL;
    int rc = getaddrinfo(hostname.utf8_str(), NULL, &hints, &info);
    if ( rc )
    {
        // use gai_strerror()?
        return false;
    }

    wxCHECK_MSG( info, false, "should have info on success" );

    wxASSERT_MSG( int(info->ai_addrlen) == m_len, "unexpected address length" );

    memcpy(addr, info->ai_addr, info->ai_addrlen);
    freeaddrinfo(info);

    return true;
}

bool wxSockAddressImpl::GetHostAddress(in6_addr *address) const
{
    sockaddr_in6 * const addr = GET(sockaddr_in6);
    if ( !addr )
        return false;

    *address = addr->sin6_addr;

    return true;
}

bool wxSockAddressImpl::SetHostAddress(const in6_addr& address)
{
    sockaddr_in6 * const addr = GET(sockaddr_in6);
    if ( !addr )
        return false;

    addr->sin6_addr = address;

    return true;
}

wxUint16 wxSockAddressImpl::GetPort6() const
{
    sockaddr_in6 * const addr = GET(sockaddr_in6);
    if ( !addr )
        return 0;

    return ntohs(addr->sin6_port);
}

bool wxSockAddressImpl::SetPort6(wxUint16 port)
{
    sockaddr_in6 * const addr = GET(sockaddr_in6);
    if ( !addr )
        return false;

    addr->sin6_port = htons(port);

    return true;
}

bool wxSockAddressImpl::SetToAnyAddress6()
{
    static const in6_addr any = IN6ADDR_ANY_INIT;

    return SetHostAddress(any);
}

#endif // wxUSE_IPV6

#ifdef wxHAS_UNIX_DOMAIN_SOCKETS

// ----------------------------------------------------------------------------
// Unix address family
// ----------------------------------------------------------------------------

#ifndef UNIX_PATH_MAX
    #define UNIX_PATH_MAX (WXSIZEOF(((sockaddr_un *)NULL)->sun_path))
#endif

void wxSockAddressImpl::CreateUnix()
{
    wxASSERT_MSG( Is(FAMILY_UNSPEC), "recreating address as different type?" );

    m_family = FAMILY_UNIX;
    sockaddr_un * const addr = ALLOC(sockaddr_un);
    addr->sun_family = FAMILY_UNIX;
    addr->sun_path[0] = '\0';
}

bool wxSockAddressImpl::SetPath(const wxString& path)
{
    sockaddr_un * const addr = GET(sockaddr_un);
    if ( !addr )
        return false;

    const wxScopedCharBuffer buf(path.utf8_str());
    if ( strlen(buf) >= UNIX_PATH_MAX )
        return false;

    wxStrlcpy(addr->sun_path, buf, UNIX_PATH_MAX);

    return true;
}

wxString wxSockAddressImpl::GetPath() const
{
    sockaddr_un * const addr = GET(sockaddr_un);
    if ( !addr )
        return wxString();

    return wxString::FromUTF8(addr->sun_path);
}

#endif // wxHAS_UNIX_DOMAIN_SOCKETS

#undef GET
#undef ALLOC

// ----------------------------------------------------------------------------
// wxSockAddress
// ----------------------------------------------------------------------------

const sockaddr *wxSockAddress::GetAddressData() const
{
    return GetAddress().GetAddr();
}

int wxSockAddress::GetAddressDataLen() const
{
    return GetAddress().GetLen();
}

void wxSockAddress::Init()
{
    if ( wxIsMainThread() && !wxSocketBase::IsInitialized() )
    {
        // we must do it before using any socket functions
        (void)wxSocketBase::Initialize();
    }
}

wxSockAddress::wxSockAddress()
{
    Init();

    m_impl = new wxSockAddressImpl();
}

wxSockAddress::wxSockAddress(const wxSockAddress& other)
    : wxObject()
{
    Init();

    m_impl = new wxSockAddressImpl(*other.m_impl);
}

wxSockAddress::~wxSockAddress()
{
    delete m_impl;
}

void wxSockAddress::SetAddress(const wxSockAddressImpl& address)
{
    if ( &address != m_impl )
    {
        delete m_impl;
        m_impl = new wxSockAddressImpl(address);
    }
}

wxSockAddress& wxSockAddress::operator=(const wxSockAddress& addr)
{
    SetAddress(addr.GetAddress());

    return *this;
}

void wxSockAddress::Clear()
{
    m_impl->Clear();
}

// ----------------------------------------------------------------------------
// wxIPaddress
// ----------------------------------------------------------------------------

wxSockAddressImpl& wxIPaddress::GetImpl()
{
    if ( m_impl->GetFamily() == wxSockAddressImpl::FAMILY_UNSPEC )
        m_impl->CreateINET();

    return *m_impl;
}

bool wxIPaddress::Hostname(const wxString& name)
{
    wxCHECK_MSG( !name.empty(), false, "empty host name is invalid" );

    m_origHostname = name;

    return GetImpl().SetHostName(name);
}

bool wxIPaddress::Service(const wxString& name)
{
    return GetImpl().SetPortName(name, "tcp");
}

bool wxIPaddress::Service(unsigned short port)
{
    return GetImpl().SetPort(port);
}

bool wxIPaddress::LocalHost()
{
    return Hostname("localhost");
}

wxString wxIPaddress::Hostname() const
{
    return GetImpl().GetHostName();
}

unsigned short wxIPaddress::Service() const
{
    return GetImpl().GetPort();
}

bool wxIPaddress::operator==(const wxIPaddress& addr) const
{
    return Hostname().Cmp(addr.Hostname()) == 0 &&
           Service() == addr.Service();
}

bool wxIPaddress::AnyAddress()
{
    return GetImpl().SetToAnyAddress();
}

// ----------------------------------------------------------------------------
// wxIPV4address
// ----------------------------------------------------------------------------

void wxIPV4address::DoInitImpl()
{
    m_impl->CreateINET();
}

bool wxIPV4address::Hostname(unsigned long addr)
{
    if ( !GetImpl().SetHostAddress(addr) )
    {
        m_origHostname.clear();
        return false;
    }

    m_origHostname = Hostname();
    return true;
}

bool wxIPV4address::IsLocalHost() const
{
    return Hostname() == "localhost" || IPAddress() == "127.0.0.1";
}

wxString wxIPV4address::IPAddress() const
{
    wxUint32 addr;
    if ( !GetImpl().GetHostAddress(&addr) )
        return wxString();

    return wxString::Format
           (
            "%u.%u.%u.%u",
            (addr >> 24) & 0xff,
            (addr >> 16) & 0xff,
            (addr >> 8) & 0xff,
            addr & 0xff
           );
}

bool wxIPV4address::BroadcastAddress()
{
    return GetImpl().SetToBroadcastAddress();
}

#if wxUSE_IPV6

// ---------------------------------------------------------------------------
// wxIPV6address
// ---------------------------------------------------------------------------

void wxIPV6address::DoInitImpl()
{
    m_impl->CreateINET6();
}

bool wxIPV6address::Hostname(unsigned char addr[16])
{
    unsigned short wk[8];
    for ( int i = 0; i < 8; ++i )
    {
        wk[i] = addr[2*i];
        wk[i] <<= 8;
        wk[i] |= addr[2*i+1];
    }

    return Hostname
           (
                wxString::Format
                (
                 "%x:%x:%x:%x:%x:%x:%x:%x",
                 wk[0], wk[1], wk[2], wk[3], wk[4], wk[5], wk[6], wk[7]
                )
           );
}

bool wxIPV6address::IsLocalHost() const
{
    if ( Hostname() == "localhost" )
        return true;

    wxString addr = IPAddress();
    return addr == wxT("::1") ||
                addr == wxT("0:0:0:0:0:0:0:1") ||
                    addr == wxT("::ffff:127.0.0.1");
}

wxString wxIPV6address::IPAddress() const
{
    union
    {
        in6_addr addr6;
        wxUint8 bytes[16];
    } u;

    if ( !GetImpl().GetHostAddress(&u.addr6) )
        return wxString();

    const wxUint8 * const addr = u.bytes;

    wxUint16 words[8];
    int i,
        prefix_zero_count = 0;
    for ( i = 0; i < 8; ++i )
    {
        words[i] = addr[i*2];
        words[i] <<= 8;
        words[i] |= addr[i*2+1];
        if ( i == prefix_zero_count && words[i] == 0 )
            ++prefix_zero_count;
    }

    wxString result;
    if ( prefix_zero_count == 8 )
    {
        result = wxT( "::" );
    }
    else if ( prefix_zero_count == 6 && words[5] == 0xFFFF )
    {
        // IPv4 mapped
        result.Printf("::ffff:%d.%d.%d.%d",
                      addr[12], addr[13], addr[14], addr[15]);
    }
    else // general case
    {
        result = ":";
        for ( i = prefix_zero_count; i < 8; ++i )
        {
            result += wxString::Format(":%x", words[i]);
        }
    }

    return result;
}

#endif // wxUSE_IPV6

#ifdef wxHAS_UNIX_DOMAIN_SOCKETS

// ---------------------------------------------------------------------------
// wxUNIXaddress
// ---------------------------------------------------------------------------

wxSockAddressImpl& wxUNIXaddress::GetUNIX()
{
    if ( m_impl->GetFamily() == wxSockAddressImpl::FAMILY_UNSPEC )
        m_impl->CreateUnix();

    return *m_impl;
}

void wxUNIXaddress::Filename(const wxString& fname)
{
    GetUNIX().SetPath(fname);
}

wxString wxUNIXaddress::Filename() const
{
    return GetUNIX().GetPath();
}

#endif // wxHAS_UNIX_DOMAIN_SOCKETS

#endif // wxUSE_SOCKETS
