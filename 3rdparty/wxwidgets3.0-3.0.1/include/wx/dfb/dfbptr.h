/////////////////////////////////////////////////////////////////////////////
// Name:        wx/dfb/dfbptr.h
// Purpose:     wxDfbPtr<T> for holding objects declared in wrapdfb.h
// Author:      Vaclav Slavik
// Created:     2006-08-09
// Copyright:   (c) 2006 REA Elektronik GmbH
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_DFB_DFBPTR_H_
#define _WX_DFB_DFBPTR_H_

//-----------------------------------------------------------------------------
// wxDFB_DECLARE_INTERFACE
//-----------------------------------------------------------------------------

/**
    Forward declares wx wrapper around DirectFB interface @a name.

    Also declares wx##name##Ptr typedef for wxDfbPtr<wx##name> pointer.

    @param name  name of the DirectFB interface
 */
#define wxDFB_DECLARE_INTERFACE(name)            \
    class wx##name;                              \
    typedef wxDfbPtr<wx##name> wx##name##Ptr;


//-----------------------------------------------------------------------------
// wxDfbPtr<T>
//-----------------------------------------------------------------------------

class wxDfbWrapperBase;

class WXDLLIMPEXP_CORE wxDfbPtrBase
{
protected:
    static void DoAddRef(wxDfbWrapperBase *ptr);
    static void DoRelease(wxDfbWrapperBase *ptr);
};

/**
    This template implements smart pointer for keeping pointers to DirectFB
    wrappers (i.e. wxIFoo classes derived from wxDfbWrapper<T>). Interface's
    reference count is increased on copying and the interface is released when
    the pointer is deleted.
 */
template<typename T>
class wxDfbPtr : private wxDfbPtrBase
{
public:
    /**
        Creates the pointer from raw pointer to the wrapper.

        Takes ownership of @a ptr, i.e. AddRef() is @em not called on it.
     */
    wxDfbPtr(T *ptr = NULL) : m_ptr(ptr) {}

    /// Copy ctor
    wxDfbPtr(const wxDfbPtr& ptr) { InitFrom(ptr); }

    /// Dtor. Releases the interface
    ~wxDfbPtr() { Reset(); }

    /// Resets the pointer to NULL, decreasing reference count of the interface.
    void Reset()
    {
        if ( m_ptr )
        {
            this->DoRelease((wxDfbWrapperBase*)m_ptr);
            m_ptr = NULL;
        }
    }

    /// Cast to the wrapper pointer
    operator T*() const { return m_ptr; }

    // standard operators:

    wxDfbPtr& operator=(T *ptr)
    {
        Reset();
        m_ptr = ptr;
        return *this;
    }

    wxDfbPtr& operator=(const wxDfbPtr& ptr)
    {
        Reset();
        InitFrom(ptr);
        return *this;
    }

    T& operator*() const { return *m_ptr; }
    T* operator->() const { return m_ptr; }

private:
    void InitFrom(const wxDfbPtr& ptr)
    {
        m_ptr = ptr.m_ptr;
        if ( m_ptr )
            this->DoAddRef((wxDfbWrapperBase*)m_ptr);
    }

private:
    T *m_ptr;
};

#endif // _WX_DFB_DFBPTR_H_
