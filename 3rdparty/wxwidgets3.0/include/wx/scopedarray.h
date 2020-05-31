/////////////////////////////////////////////////////////////////////////////
// Name:        wx/scopedarray.h
// Purpose:     scoped smart pointer class
// Author:      Vadim Zeitlin
// Created:     2009-02-03
// Copyright:   (c) Jesse Lovelace and original Boost authors (see below)
//              (c) 2009 Vadim Zeitlin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SCOPED_ARRAY_H_
#define _WX_SCOPED_ARRAY_H_

#include "wx/defs.h"
#include "wx/checkeddelete.h"

// ----------------------------------------------------------------------------
// wxScopedArray: A scoped array
// ----------------------------------------------------------------------------

template <class T>
class wxScopedArray
{
public:
    typedef T element_type;

    wxEXPLICIT wxScopedArray(T * array = NULL) : m_array(array) { }

    ~wxScopedArray() { delete [] m_array; }

    // test for pointer validity: defining conversion to unspecified_bool_type
    // and not more obvious bool to avoid implicit conversions to integer types
    typedef T *(wxScopedArray<T>::*unspecified_bool_type)() const;
    operator unspecified_bool_type() const
    {
        return m_array ? &wxScopedArray<T>::get : NULL;
    }

    void reset(T *array = NULL)
    {
        if ( array != m_array )
        {
            delete [] m_array;
            m_array = array;
        }
    }

    T& operator[](size_t n) const { return m_array[n]; }

    T *get() const { return m_array; }

    void swap(wxScopedArray &other)
    {
        T * const tmp = other.m_array;
        other.m_array = m_array;
        m_array = tmp;
    }

private:
    T *m_array;

    DECLARE_NO_COPY_TEMPLATE_CLASS(wxScopedArray, T)
};

// ----------------------------------------------------------------------------
// old macro based implementation
// ----------------------------------------------------------------------------

// the same but for arrays instead of simple pointers
#define wxDECLARE_SCOPED_ARRAY(T, name)\
class name                          \
{                                   \
private:                            \
    T * m_ptr;                      \
    name(name const &);             \
    name & operator=(name const &); \
                                    \
public:                             \
    wxEXPLICIT name(T * p = NULL) : m_ptr(p) \
    {}                              \
                                    \
    ~name();                        \
    void reset(T * p = NULL);       \
                                    \
    T & operator[](long int i) const\
    {                               \
        wxASSERT(m_ptr != NULL);    \
        wxASSERT(i >= 0);           \
        return m_ptr[i];            \
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

#define wxDEFINE_SCOPED_ARRAY(T, name)  \
name::~name()                           \
{                                       \
    wxCHECKED_DELETE_ARRAY(m_ptr);      \
}                                       \
void name::reset(T * p){                \
    if (m_ptr != p)                     \
    {                                   \
       wxCHECKED_DELETE_ARRAY(m_ptr);   \
       m_ptr = p;                       \
    }                                   \
}

#endif // _WX_SCOPED_ARRAY_H_

