///////////////////////////////////////////////////////////////////////////////
// Name:        wx/private/sckaddr.h
// Purpose:     wxSockAddressImpl
// Author:      Vadim Zeitlin
// Created:     2008-12-28
// Copyright:   (c) 2008 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_SOCKADDR_H_
#define _WX_PRIVATE_SOCKADDR_H_

#ifdef __WINDOWS__
    #include "wx/msw/wrapwin.h"

    #if wxUSE_IPV6
        #include <ws2tcpip.h>
    #endif
#elif defined(__VMS__)
    #include <socket.h>

    struct sockaddr_un
    {
        u_char  sun_len;        /* sockaddr len including null */
        u_char  sun_family;     /* AF_UNIX */
        char    sun_path[108];  /* path name (gag) */
    };
    #include <in.h>
#else // generic Unix
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <sys/un.h>
#endif // platform

#include <stdlib.h> // for calloc()

// this is a wrapper for sockaddr_storage if it's available or just sockaddr
// otherwise
union wxSockAddressStorage
{
#if wxUSE_IPV6
    sockaddr_storage addr_storage;
#endif
    sockaddr addr;
};

// ----------------------------------------------------------------------------
// helpers for wxSockAddressImpl
// ----------------------------------------------------------------------------

// helper class mapping sockaddr_xxx types to corresponding AF_XXX values
template <class T> struct AddressFamily;

template <> struct AddressFamily<sockaddr_in> { enum { value = AF_INET }; };

#if wxUSE_IPV6
template <> struct AddressFamily<sockaddr_in6> { enum { value = AF_INET6 }; };
#endif // wxUSE_IPV6

#ifdef wxHAS_UNIX_DOMAIN_SOCKETS
template <> struct AddressFamily<sockaddr_un> { enum { value = AF_UNIX }; };
#endif // wxHAS_UNIX_DOMAIN_SOCKETS

// ----------------------------------------------------------------------------
// wxSockAddressImpl
// ----------------------------------------------------------------------------

// Represents a socket endpoint, e.g. an (address, port) pair for PF_INET
// sockets. It can be initialized from an existing sockaddr struct and also
// provides access to sockaddr stored internally so that it can be easily used
// with e.g. connect(2).
//
// This class also performs (synchronous, hence potentially long) name lookups
// if necessary, i.e. if the host name strings don't contain addresses in
// numerical form (quad dotted for IPv4 or standard hexadecimal for IPv6).
// Notice that internally the potentially Unicode host names are encoded as
// UTF-8 before being passed to the lookup function but the host names should
// really be ASCII anyhow.
class wxSockAddressImpl
{
public:
    // as this is passed to socket() it should be a PF_XXX and not AF_XXX (even
    // though they're the same in practice)
    enum Family
    {
        FAMILY_INET = PF_INET,
#if wxUSE_IPV6
        FAMILY_INET6 = PF_INET6,
#endif
#ifdef wxHAS_UNIX_DOMAIN_SOCKETS
        FAMILY_UNIX = PF_UNIX,
#endif
        FAMILY_UNSPEC = PF_UNSPEC
    };

    // default ctor creates uninitialized object, use one of CreateXXX() below
    wxSockAddressImpl()
    {
        InitUnspec();
    }

    // ctor from an existing sockaddr
    wxSockAddressImpl(const sockaddr& addr, int len)
    {
        switch ( addr.sa_family )
        {
            case PF_INET:
#if wxUSE_IPV6
            case PF_INET6:
#endif
#ifdef wxHAS_UNIX_DOMAIN_SOCKETS
            case PF_UNIX:
#endif
                m_family = static_cast<Family>(addr.sa_family);
                break;

            default:
                wxFAIL_MSG( "unsupported socket address family" );
                InitUnspec();
                return;
        }

        InitFromSockaddr(addr, len);
    }

    // copy ctor and assignment operators
    wxSockAddressImpl(const wxSockAddressImpl& other)
    {
        InitFromOther(other);
    }

    wxSockAddressImpl& operator=(const wxSockAddressImpl& other)
    {
        if (this != &other)
        {
            free(m_addr);
            InitFromOther(other);
        }
        return *this;
    }

    // dtor frees the memory used by m_addr
    ~wxSockAddressImpl()
    {
        free(m_addr);
    }


    // reset the address to the initial uninitialized state
    void Clear()
    {
        free(m_addr);

        InitUnspec();
    }

    // initialize the address to be of specific address family, it must be
    // currently uninitialized (you may call Clear() to achieve this)
    void CreateINET();
    void CreateINET6();
#ifdef wxHAS_UNIX_DOMAIN_SOCKETS
    void CreateUnix();
#endif // wxHAS_UNIX_DOMAIN_SOCKETS
    void Create(Family family)
    {
        switch ( family )
        {
            case FAMILY_INET:
                CreateINET();
                break;

#if wxUSE_IPV6
            case FAMILY_INET6:
                CreateINET6();
                break;
#endif // wxUSE_IPV6

#ifdef wxHAS_UNIX_DOMAIN_SOCKETS
            case FAMILY_UNIX:
                CreateUnix();
                break;
#endif // wxHAS_UNIX_DOMAIN_SOCKETS

            default:
                wxFAIL_MSG( "unsupported socket address family" );
        }
    }

    // simple accessors
    Family GetFamily() const { return m_family; }
    bool Is(Family family) const { return m_family == family; }
    bool IsOk() const { return m_family != FAMILY_UNSPEC; }
    const sockaddr *GetAddr() const { return m_addr; }
    sockaddr *GetWritableAddr() { return m_addr; }
    int GetLen() const { return m_len; }

    // accessors for INET or INET6 address families
#if wxUSE_IPV6
    #define CALL_IPV4_OR_6(func, args) \
        Is(FAMILY_INET6) ? func##6(args) : func##4(args)
    #define CALL_IPV4_OR_6_VOID(func) \
        Is(FAMILY_INET6) ? func##6() : func##4()
#else
    #define CALL_IPV4_OR_6(func, args) func##4(args)
    #define CALL_IPV4_OR_6_VOID(func) func##4()
#endif // IPv6 support on/off

    wxString GetHostName() const;
    bool SetHostName(const wxString& name)
    {
        return CALL_IPV4_OR_6(SetHostName, (name));
    }

    wxUint16 GetPort() const { return CALL_IPV4_OR_6_VOID(GetPort); }
    bool SetPort(wxUint16 port) { return CALL_IPV4_OR_6(SetPort, (port)); }
    bool SetPortName(const wxString& name, const char *protocol);

    bool SetToAnyAddress() { return CALL_IPV4_OR_6_VOID(SetToAnyAddress); }

#undef CALL_IPV4_OR_6

    // accessors for INET addresses only
    bool GetHostAddress(wxUint32 *address) const;
    bool SetHostAddress(wxUint32 address);

    bool SetToBroadcastAddress() { return SetHostAddress(INADDR_BROADCAST); }

    // accessors for INET6 addresses only
#if wxUSE_IPV6
    bool GetHostAddress(in6_addr *address) const;
    bool SetHostAddress(const in6_addr& address);
#endif // wxUSE_IPV6

#ifdef wxHAS_UNIX_DOMAIN_SOCKETS
    // methods valid for Unix address family addresses only
    bool SetPath(const wxString& path);
    wxString GetPath() const;
#endif // wxHAS_UNIX_DOMAIN_SOCKETS

private:
    void DoAlloc(int len)
    {
        m_addr = static_cast<sockaddr *>(calloc(1, len));
        m_len = len;
    }

    template <class T>
    T *Alloc()
    {
        DoAlloc(sizeof(T));

        return reinterpret_cast<T *>(m_addr);
    }

    template <class T>
    T *Get() const
    {
        wxCHECK_MSG( static_cast<int>(m_family) == AddressFamily<T>::value,
                     NULL,
                     "socket address family mismatch" );

        return reinterpret_cast<T *>(m_addr);
    }

    void InitUnspec()
    {
        m_family = FAMILY_UNSPEC;
        m_addr = NULL;
        m_len = 0;
    }

    void InitFromSockaddr(const sockaddr& addr, int len)
    {
        DoAlloc(len);
        memcpy(m_addr, &addr, len);
    }

    void InitFromOther(const wxSockAddressImpl& other)
    {
        m_family = other.m_family;

        if ( other.m_addr )
        {
            InitFromSockaddr(*other.m_addr, other.m_len);
        }
        else // no address to copy
        {
            m_addr = NULL;
            m_len = 0;
        }
    }

    // IPv4/6 implementations of public functions
    bool SetHostName4(const wxString& name);

    bool SetPort4(wxUint16 port);
    wxUint16 GetPort4() const;

    bool SetToAnyAddress4() { return SetHostAddress(INADDR_ANY); }

#if wxUSE_IPV6
    bool SetHostName6(const wxString& name);

    bool SetPort6(wxUint16 port);
    wxUint16 GetPort6() const;

    bool SetToAnyAddress6();
#endif // wxUSE_IPV6

    Family m_family;
    sockaddr *m_addr;
    int m_len;
};

#endif // _WX_PRIVATE_SOCKADDR_H_
