///////////////////////////////////////////////////////////////////////////////
// Name:        wx/windowid.h
// Purpose:     wxWindowID class - a class for managing window ids
// Author:      Brian Vanderburg II
// Created:     2007-09-21
// Copyright:   (c) 2007 Brian Vanderburg II
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_WINDOWID_H_
#define _WX_WINDOWID_H_

// NB: do not include defs.h as we are included from it

typedef int wxWindowID;

// ----------------------------------------------------------------------------
// wxWindowIDRef: reference counted id value
// ----------------------------------------------------------------------------

// A wxWindowIDRef object wraps an id value and marks it as (un)used as
// necessary. All ids returned from wxWindow::NewControlId() should be assigned
// to an instance of this class to ensure that the id is marked as being in
// use.
//
// This class is always defined but it is trivial if wxUSE_AUTOID_MANAGEMENT is
// off.
class WXDLLIMPEXP_CORE wxWindowIDRef
{
public:
    // default ctor
    wxWindowIDRef()
    {
        m_id = wxID_NONE;
    }

    // ctor taking id values
    wxWindowIDRef(int id)
    {
        Init(id);
    }

    wxWindowIDRef(long id)
    {
        Init(wxWindowID(id));
    }

    wxWindowIDRef(const wxWindowIDRef& id)
    {
        Init(id.m_id);
    }

    // dtor
    ~wxWindowIDRef()
    {
        Assign(wxID_NONE);
    }

    // assignment
    wxWindowIDRef& operator=(int id)
    {
        Assign(id);
        return *this;
    }

    wxWindowIDRef& operator=(long id)
    {
        Assign(wxWindowID(id));
        return *this;
    }

    wxWindowIDRef& operator=(const wxWindowIDRef& id)
    {
        if (&id != this)
            Assign(id.m_id);
        return *this;
    }

    // access to the stored id value
    wxWindowID GetValue() const
    {
        return m_id;
    }

    operator wxWindowID() const
    {
        return m_id;
    }

private:
#if wxUSE_AUTOID_MANAGEMENT
    // common part of all ctors: call Assign() for our new id
    void Init(wxWindowID id)
    {
        // m_id must be initialized before calling Assign()
        m_id = wxID_NONE;
        Assign(id);
    }

    // increase reference count of id, decrease the one of m_id
    void Assign(wxWindowID id);
#else // !wxUSE_AUTOID_MANAGEMENT
    // trivial stubs for the functions above
    void Init(wxWindowID id)
    {
        m_id = id;
    }

    void Assign(wxWindowID id)
    {
        m_id = id;
    }
#endif // wxUSE_AUTOID_MANAGEMENT/!wxUSE_AUTOID_MANAGEMENT


    wxWindowID m_id;
};

// comparison operators
inline bool operator==(const wxWindowIDRef& lhs, const wxWindowIDRef& rhs)
{
    return lhs.GetValue() == rhs.GetValue();
}

inline bool operator==(const wxWindowIDRef& lhs, int rhs)
{
    return lhs.GetValue() == rhs;
}

inline bool operator==(const wxWindowIDRef& lhs, long rhs)
{
    return lhs.GetValue() == rhs;
}

inline bool operator==(int lhs, const wxWindowIDRef& rhs)
{
    return rhs == lhs;
}

inline bool operator==(long lhs, const wxWindowIDRef& rhs)
{
    return rhs == lhs;
}

inline bool operator!=(const wxWindowIDRef& lhs, const wxWindowIDRef& rhs)
{
    return !(lhs == rhs);
}

inline bool operator!=(const wxWindowIDRef& lhs, int rhs)
{
    return !(lhs == rhs);
}

inline bool operator!=(const wxWindowIDRef& lhs, long rhs)
{
    return !(lhs == rhs);
}

inline bool operator!=(int lhs, const wxWindowIDRef& rhs)
{
    return !(lhs == rhs);
}

inline bool operator!=(long lhs, const wxWindowIDRef& rhs)
{
    return !(lhs == rhs);
}

// ----------------------------------------------------------------------------
// wxIdManager
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxIdManager
{
public:
    // This returns an id value and not an wxWindowIDRef.  The returned value
    // should be assigned a.s.a.p to a wxWindowIDRef.  The IDs are marked as
    // reserved so that another call to ReserveId before assigning the id to a
    // wxWindowIDRef will not use the same ID
    static wxWindowID ReserveId(int count = 1);

    // This will release an unused reserved ID.  This should only be called
    // if the ID returned by ReserveId was NOT assigned to a wxWindowIDRef
    // for some purpose, maybe an early return from a function
    static void UnreserveId(wxWindowID id, int count = 1);
};

#endif // _WX_WINDOWID_H_
