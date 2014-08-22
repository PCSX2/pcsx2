///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/private/comptr.h
// Purpose:     Smart pointer for COM interfaces.
// Author:      PB
// Created:     2012-04-16
// Copyright:   (c) 2012 wxWidgets team
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_PRIVATE_COMPTR_H_
#define _WX_MSW_PRIVATE_COMPTR_H_

// ----------------------------------------------------------------------------
// wxCOMPtr: A minimalistic smart pointer for use with COM interfaces.
// ----------------------------------------------------------------------------

template <class T>
class wxCOMPtr
{
public:
    typedef T element_type;

    wxCOMPtr()
        : m_ptr(NULL)
    {
    }

    wxEXPLICIT wxCOMPtr(T* ptr)
        : m_ptr(ptr)
    {
        if ( m_ptr )
            m_ptr->AddRef();
    }

    wxCOMPtr(const wxCOMPtr& ptr)
        : m_ptr(ptr.get())
    {
        if ( m_ptr )
            m_ptr->AddRef();
    }

    ~wxCOMPtr()
    {
        if ( m_ptr )
            m_ptr->Release();
    }

    void reset(T* ptr = NULL)
    {
        if ( m_ptr != ptr)
        {
            if ( ptr )
                ptr->AddRef();
            if ( m_ptr )
                m_ptr->Release();
            m_ptr = ptr;
        }
    }

    wxCOMPtr& operator=(const wxCOMPtr& ptr)
    {
        reset(ptr.get());
        return *this;
    }

    wxCOMPtr& operator=(T* ptr)
    {
        reset(ptr);
        return *this;
    }

    operator T*() const
    {
        return m_ptr;
    }

    T& operator*() const
    {
        return *m_ptr;
    }

    T* operator->() const
    {
        return m_ptr;
    }

    // It would be better to forbid direct access completely but we do need
    // for QueryInterface() and similar functions, so provide it but it can
    // only be used to initialize the pointer, not to modify an existing one.
    T** operator&()
    {
        wxASSERT_MSG(!m_ptr,
                     wxS("Can't get direct access to initialized pointer"));

        return &m_ptr;
    }

    T* get() const
    {
        return m_ptr;
    }

    bool operator<(T* ptr) const
    {
        return get() < ptr;
    }

private:
    T* m_ptr;
};

// Define a helper for the macro below: we just need a function taking a
// pointer and not returning anything to avoid warnings about unused return
// value of the cast in the macro itself.
namespace wxPrivate { inline void PPV_ARGS_CHECK(void*) { } }

// Takes the interface name and a pointer to a pointer of the interface type
// and expands into the IID of this interface and the same pointer but after a
// type-safety check.
//
// This is similar to the standard IID_PPV_ARGS macro but takes the pointer
// type instead of relying on the non-standard Microsoft __uuidof().
#define wxIID_PPV_ARGS(IType, pType) \
    IID_##IType, \
    (wxPrivate::PPV_ARGS_CHECK(static_cast<IType*>(*pType)), \
     reinterpret_cast<void**>(pType))

#endif // _WX_MSW_PRIVATE_COMPTR_H_

