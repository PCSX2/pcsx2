/////////////////////////////////////////////////////////////////////////////
// Name:        wx/uri.h
// Purpose:     wxURI - Class for parsing URIs
// Author:      Ryan Norton
//              Vadim Zeitlin (UTF-8 URI support, many other changes)
// Created:     07/01/2004
// Copyright:   (c) 2004 Ryan Norton
//                  2008 Vadim Zeitlin
// Licence:     wxWindows Licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_URI_H_
#define _WX_URI_H_

#include "wx/defs.h"
#include "wx/object.h"
#include "wx/string.h"
#include "wx/arrstr.h"

// Host Type that the server component can be
enum wxURIHostType
{
    wxURI_REGNAME,      // Host is a normal register name (www.mysite.com etc.)
    wxURI_IPV4ADDRESS,  // Host is a version 4 ip address (192.168.1.100)
    wxURI_IPV6ADDRESS,  // Host is a version 6 ip address [aa:aa:aa:aa::aa:aa]:5050
    wxURI_IPVFUTURE     // Host is a future ip address (wxURI is unsure what kind)
};

// Component Flags
enum wxURIFieldType
{
    wxURI_SCHEME = 1,
    wxURI_USERINFO = 2,
    wxURI_SERVER = 4,
    wxURI_PORT = 8,
    wxURI_PATH = 16,
    wxURI_QUERY = 32,
    wxURI_FRAGMENT = 64
};

// Miscellaneous other flags
enum wxURIFlags
{
    wxURI_STRICT = 1
};


// Generic class for parsing URIs.
//
// See RFC 3986
class WXDLLIMPEXP_BASE wxURI : public wxObject
{
public:
    wxURI();
    wxURI(const wxString& uri);

    // default copy ctor, assignment operator and dtor are ok

    bool Create(const wxString& uri);

    wxURI& operator=(const wxString& string)
    {
        Create(string);
        return *this;
    }

    bool operator==(const wxURI& uri) const;

    // various accessors

    bool HasScheme() const      { return (m_fields & wxURI_SCHEME) != 0;   }
    bool HasUserInfo() const    { return (m_fields & wxURI_USERINFO) != 0; }
    bool HasServer() const      { return (m_fields & wxURI_SERVER) != 0;   }
    bool HasPort() const        { return (m_fields & wxURI_PORT) != 0;     }
    bool HasPath() const        { return (m_fields & wxURI_PATH) != 0;     }
    bool HasQuery() const       { return (m_fields & wxURI_QUERY) != 0;    }
    bool HasFragment() const    { return (m_fields & wxURI_FRAGMENT) != 0; }

    const wxString& GetScheme() const    { return m_scheme;   }
    const wxString& GetPath() const      { return m_path;     }
    const wxString& GetQuery() const     { return m_query;    }
    const wxString& GetFragment() const  { return m_fragment; }
    const wxString& GetPort() const      { return m_port;     }
    const wxString& GetUserInfo() const  { return m_userinfo; }
    const wxString& GetServer() const    { return m_server;   }
    wxURIHostType GetHostType() const    { return m_hostType; }

    // these functions only work if the user information part of the URI is in
    // the usual (but insecure and hence explicitly recommended against by the
    // RFC) "user:password" form
    wxString GetUser() const;
    wxString GetPassword() const;


    // combine all URI components into a single string
    //
    // BuildURI() returns the real URI suitable for use with network libraries,
    // for example, while BuildUnescapedURI() returns a string suitable to be
    // shown to the user.
    wxString BuildURI() const { return DoBuildURI(&wxURI::Nothing); }
    wxString BuildUnescapedURI() const { return DoBuildURI(&wxURI::Unescape); }

    // the escaped URI should contain only ASCII characters, including possible
    // escape sequences
    static wxString Unescape(const wxString& escapedURI);


    void Resolve(const wxURI& base, int flags = wxURI_STRICT);
    bool IsReference() const;

protected:
    void Clear();

    // common part of BuildURI() and BuildUnescapedURI()
    wxString DoBuildURI(wxString (*funcDecode)(const wxString&)) const;

    // function which returns its argument unmodified, this is used by
    // BuildURI() to tell DoBuildURI() that nothing needs to be done with the
    // URI components
    static wxString Nothing(const wxString& value) { return value; }

    bool Parse(const char* uri);

    const char* ParseAuthority (const char* uri);
    const char* ParseScheme    (const char* uri);
    const char* ParseUserInfo  (const char* uri);
    const char* ParseServer    (const char* uri);
    const char* ParsePort      (const char* uri);
    const char* ParsePath      (const char* uri);
    const char* ParseQuery     (const char* uri);
    const char* ParseFragment  (const char* uri);


    static bool ParseH16(const char*& uri);
    static bool ParseIPv4address(const char*& uri);
    static bool ParseIPv6address(const char*& uri);
    static bool ParseIPvFuture(const char*& uri);

    // should be called with i pointing to '%', returns the encoded character
    // following it or -1 if invalid and advances i past it (so that it points
    // to the last character consumed on return)
    static int DecodeEscape(wxString::const_iterator& i);

    // append next character pointer to by p to the string in an escaped form
    // and advance p past it
    //
    // if the next character is '%' and it's followed by 2 hex digits, they are
    // not escaped (again) by this function, this allows to keep (backwards-
    // compatible) ambiguity about the input format to wxURI::Create(): it can
    // be either already escaped or not
    void AppendNextEscaped(wxString& s, const char *& p);

    // convert hexadecimal digit to its value; return -1 if c isn't valid
    static int CharToHex(char c);

    // split an URI path string in its component segments (including empty and
    // "." ones, no post-processing is done)
    static wxArrayString SplitInSegments(const wxString& path);

    // various URI grammar helpers
    static bool IsUnreserved(char c);
    static bool IsReserved(char c);
    static bool IsGenDelim(char c);
    static bool IsSubDelim(char c);
    static bool IsHex(char c);
    static bool IsAlpha(char c);
    static bool IsDigit(char c);
    static bool IsEndPath(char c);

    wxString m_scheme;
    wxString m_path;
    wxString m_query;
    wxString m_fragment;

    wxString m_userinfo;
    wxString m_server;
    wxString m_port;

    wxURIHostType m_hostType;

    size_t m_fields;

    DECLARE_DYNAMIC_CLASS(wxURI)
};

#endif // _WX_URI_H_

