/////////////////////////////////////////////////////////////////////////////
// Name:        wx/scopedptr.h
// Purpose:     scoped smart pointer class
// Author:      Jesse Lovelace <jllovela@eos.ncsu.edu>
// Created:     06/01/02
// Copyright:   (c) Jesse Lovelace and original Boost authors (see below)
//              (c) 2009 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

//  This class closely follows the implementation of the boost
//  library scoped_ptr and is an adaption for c++ macro's in
//  the wxWidgets project. The original authors of the boost
//  scoped_ptr are given below with their respective copyrights.

//  (C) Copyright Greg Colvin and Beman Dawes 1998, 1999.
//  Copyright (c) 2001, 2002 Peter Dimov
//
//  Permission to copy, use, modify, sell and distribute this software
//  is granted provided this copyright notice appears in all copies.
//  This software is provided "as is" without express or implied
//  warranty, and with no claim as to its suitability for any purpose.
//
//  See http://www.boost.org/libs/smart_ptr/scoped_ptr.htm for documentation.
//

#ifndef _WX_SCOPED_PTR_H_
#define _WX_SCOPED_PTR_H_

#include "wx/defs.h"
#include "wx/checkeddelete.h"

// ----------------------------------------------------------------------------
// wxScopedPtr: A scoped pointer
// ----------------------------------------------------------------------------

template <class T>
class wxScopedPtr
{
public:
    typedef T element_type;

    wxEXPLICIT wxScopedPtr(T * ptr = NULL) : m_ptr(ptr) { }

    ~wxScopedPtr() { wxCHECKED_DELETE(m_ptr); }

    // test for pointer validity: defining conversion to unspecified_bool_type
    // and not more obvious bool to avoid implicit conversions to integer types
#ifdef __BORLANDC__
    // this compiler is too dumb to use unspecified_bool_type operator in tests
    // of the form "if ( !ptr )"
    typedef bool unspecified_bool_type;
#else
    typedef T *(wxScopedPtr<T>::*unspecified_bool_type)() const;
#endif // __BORLANDC__
    operator unspecified_bool_type() const
    {
        return m_ptr ? &wxScopedPtr<T>::get : NULL;
    }

    void reset(T * ptr = NULL)
    {
        if ( ptr != m_ptr )
        {
            wxCHECKED_DELETE(m_ptr);
            m_ptr = ptr;
        }
    }

    T *release()
    {
        T *ptr = m_ptr;
        m_ptr = NULL;
        return ptr;
    }

    T & operator*() const
    {
        wxASSERT(m_ptr != NULL);
        return *m_ptr;
    }

    T * operator->() const
    {
        wxASSERT(m_ptr != NULL);
        return m_ptr;
    }

    T * get() const
    {
        return m_ptr;
    }

    void swap(wxScopedPtr& other)
    {
        T * const tmp = other.m_ptr;
        other.m_ptr = m_ptr;
        m_ptr = tmp;
    }

private:
    T * m_ptr;

    DECLARE_NO_COPY_TEMPLATE_CLASS(wxScopedPtr, T)
};

// ----------------------------------------------------------------------------
// old macro based implementation
// ----------------------------------------------------------------------------

/* The type being used *must* be complete at the time
   that wxDEFINE_SCOPED_* is called or a compiler error will result.
   This is because the class checks for the completeness of the type
   being used. */

#define wxDECLARE_SCOPED_PTR(T, name) \
class name                          \
{                                   \
private:                            \
    T * m_ptr;                      \
                                    \
    name(name const &);             \
    name & operator=(name const &); \
                                    \
public:                             \
    wxEXPLICIT name(T * ptr = NULL) \
    : m_ptr(ptr) { }                \
                                    \
    ~name();                        \
                                    \
    void reset(T * ptr = NULL);     \
                                    \
    T *release()                    \
    {                               \
        T *ptr = m_ptr;             \
        m_ptr = NULL;               \
        return ptr;                 \
    }                               \
                                    \
    T & operator*() const           \
    {                               \
        wxASSERT(m_ptr != NULL);    \
        return *m_ptr;              \
    }                               \
                                    \
    T * operator->() const          \
    {                               \
        wxASSERT(m_ptr != NULL);    \
        return m_ptr;               \
    }                               \
                                    \
    T * get() const                 \
    {                               \
        return m_ptr;               \
    }                               \
                                    \
    void swap(name & ot)            \
    {                               \
        T * tmp = ot.m_ptr;         \
        ot.m_ptr = m_ptr;           \
        m_ptr = tmp;                \
    }                               \
};

#define wxDEFINE_SCOPED_PTR(T, name)\
void name::reset(T * ptr)           \
{                                   \
    if (m_ptr != ptr)               \
    {                               \
        wxCHECKED_DELETE(m_ptr);    \
        m_ptr = ptr;                \
    }                               \
}                                   \
name::~name()                       \
{                                   \
    wxCHECKED_DELETE(m_ptr);        \
}

// this macro can be used for the most common case when you want to declare and
// define the scoped pointer at the same time and want to use the standard
// naming convention: auto pointer to Foo is called FooPtr
#define wxDEFINE_SCOPED_PTR_TYPE(T)    \
    wxDECLARE_SCOPED_PTR(T, T ## Ptr)  \
    wxDEFINE_SCOPED_PTR(T, T ## Ptr)

// ----------------------------------------------------------------------------
// "Tied" scoped pointer: same as normal one but also sets the value of
//                        some other variable to the pointer value
// ----------------------------------------------------------------------------

#define wxDEFINE_TIED_SCOPED_PTR_TYPE(T)                                      \
    wxDEFINE_SCOPED_PTR_TYPE(T)                                               \
    class T ## TiedPtr : public T ## Ptr                                      \
    {                                                                         \
    public:                                                                   \
        T ## TiedPtr(T **pp, T *p)                                            \
            : T ## Ptr(p), m_pp(pp)                                           \
        {                                                                     \
            m_pOld = *pp;                                                     \
            *pp = p;                                                          \
        }                                                                     \
                                                                              \
        ~ T ## TiedPtr()                                                      \
        {                                                                     \
            *m_pp = m_pOld;                                                   \
        }                                                                     \
                                                                              \
    private:                                                                  \
        T **m_pp;                                                             \
        T *m_pOld;                                                            \
    };

#endif // _WX_SCOPED_PTR_H_

