///////////////////////////////////////////////////////////////////////////////
// Name:        wx/vector.h
// Purpose:     STL vector clone
// Author:      Lindsay Mathieson
// Modified by: Vaclav Slavik - make it a template
// Created:     30.07.2001
// Copyright:   (c) 2001 Lindsay Mathieson <lindsay@mathieson.org>,
//                  2007 Vaclav Slavik <vslavik@fastmail.fm>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_VECTOR_H_
#define _WX_VECTOR_H_

#include "wx/defs.h"

#if wxUSE_STD_CONTAINERS

#include <vector>
#include <algorithm>

#define wxVector std::vector
template<typename T>
inline void wxVectorSort(wxVector<T>& v)
{
    std::sort(v.begin(), v.end());
}

#else // !wxUSE_STD_CONTAINERS

#include "wx/scopeguard.h"
#include "wx/meta/movable.h"
#include "wx/meta/if.h"

#include "wx/beforestd.h"
#include <new> // for placement new
#include "wx/afterstd.h"

// wxQsort is declared in wx/utils.h, but can't include that file here,
// it indirectly includes this file. Just lovely...
typedef int (*wxSortCallback)(const void* pItem1,
                              const void* pItem2,
                              const void* user_data);
WXDLLIMPEXP_BASE void wxQsort(void* pbase, size_t total_elems,
                              size_t size, wxSortCallback cmp,
                              const void* user_data);

namespace wxPrivate
{

// These templates encapsulate memory operations for use by wxVector; there are
// two implementations, both in generic way for any C++ types and as an
// optimized version for "movable" types that uses realloc() and memmove().

// version for movable types:
template<typename T>
struct wxVectorMemOpsMovable
{
    static void Free(T* array)
        { free(array); }

    static T* Realloc(T* old, size_t newCapacity, size_t WXUNUSED(occupiedSize))
        { return (T*)realloc(old, newCapacity * sizeof(T)); }

    static void MemmoveBackward(T* dest, T* source, size_t count)
        { memmove(dest, source, count * sizeof(T)); }

    static void MemmoveForward(T* dest, T* source, size_t count)
        { memmove(dest, source, count * sizeof(T)); }
};

// generic version for non-movable types:
template<typename T>
struct wxVectorMemOpsGeneric
{
    static void Free(T* array)
        { ::operator delete(array); }

    static T* Realloc(T* old, size_t newCapacity, size_t occupiedSize)
    {
        T *mem = (T*)::operator new(newCapacity * sizeof(T));
        for ( size_t i = 0; i < occupiedSize; i++ )
        {
            ::new(mem + i) T(old[i]);
            old[i].~T();
        }
        ::operator delete(old);
        return mem;
    }

    static void MemmoveBackward(T* dest, T* source, size_t count)
    {
        wxASSERT( dest < source );
        T* destptr = dest;
        T* sourceptr = source;
        for ( size_t i = count; i > 0; --i, ++destptr, ++sourceptr )
        {
            ::new(destptr) T(*sourceptr);
            sourceptr->~T();
        }
    }

    static void MemmoveForward(T* dest, T* source, size_t count)
    {
        wxASSERT( dest > source );
        T* destptr = dest + count - 1;
        T* sourceptr = source + count - 1;
        for ( size_t i = count; i > 0; --i, --destptr, --sourceptr )
        {
            ::new(destptr) T(*sourceptr);
            sourceptr->~T();
        }
    }
};


} // namespace wxPrivate

template<typename T>
class wxVector
{
private:
    // This cryptic expression means "typedef Ops to wxVectorMemOpsMovable if
    // type T is movable type, otherwise to wxVectorMemOpsGeneric".
    //
    // Note that we use typedef instead of privately deriving from this (which
    // would allowed us to omit "Ops::" prefixes below) to keep VC6 happy,
    // it can't compile code that derives from wxIf<...>::value.
    //
    // Note that bcc needs the extra parentheses for non-type template
    // arguments to compile this expression.
    typedef typename wxIf< (wxIsMovable<T>::value),
                           wxPrivate::wxVectorMemOpsMovable<T>,
                           wxPrivate::wxVectorMemOpsGeneric<T> >::value
            Ops;

public:
    typedef size_t size_type;
    typedef size_t difference_type;
    typedef T value_type;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef value_type* iterator;
    typedef const value_type* const_iterator;
    typedef value_type& reference;
    typedef const value_type& const_reference;

    class reverse_iterator
    {
    public:
        reverse_iterator() : m_ptr(NULL) { }
        wxEXPLICIT reverse_iterator(iterator it) : m_ptr(it) { }
        reverse_iterator(const reverse_iterator& it) : m_ptr(it.m_ptr) { }

        reference operator*() const { return *m_ptr; }
        pointer operator->() const { return m_ptr; }

        iterator base() const { return m_ptr; }

        reverse_iterator& operator++()
            { --m_ptr; return *this; }
        reverse_iterator operator++(int)
            { reverse_iterator tmp = *this; --m_ptr; return tmp; }
        reverse_iterator& operator--()
            { ++m_ptr; return *this; }
        reverse_iterator operator--(int)
            { reverse_iterator tmp = *this; ++m_ptr; return tmp; }

        reverse_iterator operator+(difference_type n) const
            { return reverse_iterator(m_ptr - n); }
        reverse_iterator& operator+=(difference_type n)
            { m_ptr -= n; return *this; }
        reverse_iterator operator-(difference_type n) const
            { return reverse_iterator(m_ptr + n); }
        reverse_iterator& operator-=(difference_type n)
            { m_ptr += n; return *this; }

        reference operator[](difference_type n) const
            { return *(*this + n); }

        bool operator ==(const reverse_iterator& it) const
            { return m_ptr == it.m_ptr; }
        bool operator !=(const reverse_iterator& it) const
            { return m_ptr != it.m_ptr; }

    private:
        value_type *m_ptr;

        friend class const_reverse_iterator;
    };

    class const_reverse_iterator
    {
    public:
        const_reverse_iterator() : m_ptr(NULL) { }
        wxEXPLICIT const_reverse_iterator(const_iterator it) : m_ptr(it) { }
        const_reverse_iterator(const reverse_iterator& it) : m_ptr(it.m_ptr) { }
        const_reverse_iterator(const const_reverse_iterator& it) : m_ptr(it.m_ptr) { }

        const_reference operator*() const { return *m_ptr; }
        const_pointer operator->() const { return m_ptr; }

        const_iterator base() const { return m_ptr; }

        const_reverse_iterator& operator++()
            { --m_ptr; return *this; }
        const_reverse_iterator operator++(int)
            { const_reverse_iterator tmp = *this; --m_ptr; return tmp; }
        const_reverse_iterator& operator--()
            { ++m_ptr; return *this; }
        const_reverse_iterator operator--(int)
            { const_reverse_iterator tmp = *this; ++m_ptr; return tmp; }

        const_reverse_iterator operator+(difference_type n) const
            { return const_reverse_iterator(m_ptr - n); }
        const_reverse_iterator& operator+=(difference_type n)
            { m_ptr -= n; return *this; }
        const_reverse_iterator operator-(difference_type n) const
            { return const_reverse_iterator(m_ptr + n); }
        const_reverse_iterator& operator-=(difference_type n)
            { m_ptr += n; return *this; }

        const_reference operator[](difference_type n) const
            { return *(*this + n); }

        bool operator ==(const const_reverse_iterator& it) const
            { return m_ptr == it.m_ptr; }
        bool operator !=(const const_reverse_iterator& it) const
            { return m_ptr != it.m_ptr; }

    protected:
        const value_type *m_ptr;
    };

    wxVector() : m_size(0), m_capacity(0), m_values(NULL) {}

    wxVector(size_type p_size)
        : m_size(0), m_capacity(0), m_values(NULL)
    {
        reserve(p_size);
        for ( size_t n = 0; n < p_size; n++ )
            push_back(value_type());
    }

    wxVector(size_type p_size, const value_type& v)
        : m_size(0), m_capacity(0), m_values(NULL)
    {
        reserve(p_size);
        for ( size_t n = 0; n < p_size; n++ )
            push_back(v);
    }

    wxVector(const wxVector& c) : m_size(0), m_capacity(0), m_values(NULL)
    {
        Copy(c);
    }

    template <class InputIterator>
    wxVector(InputIterator first, InputIterator last)
        : m_size(0), m_capacity(0), m_values(NULL)
    {
        assign(first, last);
    }

    ~wxVector()
    {
        clear();
    }

    void assign(size_type p_size, const value_type& v)
    {
        clear();
        reserve(p_size);
        for ( size_t n = 0; n < p_size; n++ )
            push_back(v);
    }

    template <class InputIterator>
    void assign(InputIterator first, InputIterator last)
    {
        clear();

        // Notice that it would be nice to call reserve() here but we can't do
        // it for arbitrary input iterators, we should have a dispatch on
        // iterator type and call it if possible.

        for ( InputIterator it = first; it != last; ++it )
            push_back(*it);
    }

    void swap(wxVector& v)
    {
        wxSwap(m_size, v.m_size);
        wxSwap(m_capacity, v.m_capacity);
        wxSwap(m_values, v.m_values);
    }

    void clear()
    {
        // call destructors of stored objects:
        for ( size_type i = 0; i < m_size; i++ )
        {
            m_values[i].~T();
        }

        Ops::Free(m_values);
        m_values = NULL;
        m_size =
        m_capacity = 0;
    }

    void reserve(size_type n)
    {
        if ( n <= m_capacity )
            return;

        // increase the size twice, unless we're already too big or unless
        // more is requested
        //
        // NB: casts to size_type are needed to suppress warnings about
        //     mixing enumeral and non-enumeral type in conditional expression
        const size_type increment = m_size > 0
                                     ? m_size < ALLOC_MAX_SIZE
                                        ? m_size
                                        : (size_type)ALLOC_MAX_SIZE
                                     : (size_type)ALLOC_INITIAL_SIZE;
        if ( m_capacity + increment > n )
            n = m_capacity + increment;

        m_values = Ops::Realloc(m_values, n, m_size);
        m_capacity = n;
    }

    void resize(size_type n)
    {
        if ( n < m_size )
            Shrink(n);
        else if ( n > m_size )
            Extend(n, value_type());
    }

    void resize(size_type n, const value_type& v)
    {
        if ( n < m_size )
            Shrink(n);
        else if ( n > m_size )
            Extend(n, v);
    }

    size_type size() const
    {
        return m_size;
    }

    size_type capacity() const
    {
        return m_capacity;
    }

    bool empty() const
    {
        return size() == 0;
    }

    wxVector& operator=(const wxVector& vb)
    {
        if (this != &vb)
        {
            clear();
            Copy(vb);
        }
        return *this;
    }

    void push_back(const value_type& v)
    {
        reserve(size() + 1);

        // use placement new to initialize new object in preallocated place in
        // m_values and store 'v' in it:
        void* const place = m_values + m_size;
        ::new(place) value_type(v);

        // only increase m_size if the ctor didn't throw an exception; notice
        // that if it _did_ throw, everything is OK, because we only increased
        // vector's capacity so far and possibly written some data to
        // uninitialized memory at the end of m_values
        m_size++;
    }

    void pop_back()
    {
        erase(end() - 1);
    }

    const value_type& at(size_type idx) const
    {
        wxASSERT(idx < m_size);
        return m_values[idx];
    }

    value_type& at(size_type idx)
    {
        wxASSERT(idx < m_size);
        return m_values[idx];
    }

    const value_type& operator[](size_type idx) const  { return at(idx); }
    value_type& operator[](size_type idx) { return at(idx); }
    const value_type& front() const { return at(0); }
    value_type& front() { return at(0); }
    const value_type& back() const { return at(size() - 1); }
    value_type& back() { return at(size() - 1); }

    const_iterator begin() const { return m_values; }
    iterator begin() { return m_values; }
    const_iterator end() const { return m_values + size(); }
    iterator end() { return m_values + size(); }

    reverse_iterator rbegin() { return reverse_iterator(end() - 1); }
    reverse_iterator rend() { return reverse_iterator(begin() - 1); }

    const_reverse_iterator rbegin() const { return const_reverse_iterator(end() - 1); }
    const_reverse_iterator rend() const { return const_reverse_iterator(begin() - 1); }

    iterator insert(iterator it, const value_type& v = value_type())
    {
        // NB: this must be done before reserve(), because reserve()
        //     invalidates iterators!
        const size_t idx = it - begin();
        const size_t after = end() - it;

        reserve(size() + 1);

        // the place where the new element is going to be inserted
        value_type * const place = m_values + idx;

        // unless we're inserting at the end, move following elements out of
        // the way:
        if ( after > 0 )
            Ops::MemmoveForward(place + 1, place, after);

        // if the ctor called below throws an exception, we need to move all
        // the elements back to their original positions in m_values
        wxScopeGuard moveBack = wxMakeGuard(
                Ops::MemmoveBackward, place, place + 1, after);
        if ( !after )
            moveBack.Dismiss();

        // use placement new to initialize new object in preallocated place in
        // m_values and store 'v' in it:
        ::new(place) value_type(v);

        // now that we did successfully add the new element, increment the size
        // and disable moving the items back
        moveBack.Dismiss();
        m_size++;

        return begin() + idx;
    }

    iterator erase(iterator it)
    {
        return erase(it, it + 1);
    }

    iterator erase(iterator first, iterator last)
    {
        if ( first == last )
            return first;
        wxASSERT( first < end() && last <= end() );

        const size_type idx = first - begin();
        const size_type count = last - first;
        const size_type after = end() - last;

        // erase elements by calling their destructors:
        for ( iterator i = first; i < last; ++i )
            i->~T();

        // once that's done, move following elements over to the freed space:
        if ( after > 0 )
        {
            Ops::MemmoveBackward(m_values + idx, m_values + idx + count, after);
        }

        m_size -= count;

        return begin() + idx;
    }

#if WXWIN_COMPATIBILITY_2_8
    wxDEPRECATED( size_type erase(size_type n) );
#endif // WXWIN_COMPATIBILITY_2_8

private:
    // VC6 can't compile static const int members
    enum { ALLOC_INITIAL_SIZE = 16 };
    enum { ALLOC_MAX_SIZE = 4096 };

    void Copy(const wxVector& vb)
    {
        reserve(vb.size());

        for ( const_iterator i = vb.begin(); i != vb.end(); ++i )
            push_back(*i);
    }

private:
    void Shrink(size_type n)
    {
        for ( size_type i = n; i < m_size; i++ )
            m_values[i].~T();
        m_size = n;
    }

    void Extend(size_type n, const value_type& v)
    {
        reserve(n);
        for ( size_type i = m_size; i < n; i++ )
            push_back(v);
    }

    size_type m_size,
              m_capacity;
    value_type *m_values;
};

#if WXWIN_COMPATIBILITY_2_8
template<typename T>
inline typename wxVector<T>::size_type wxVector<T>::erase(size_type n)
{
    erase(begin() + n);
    return n;
}
#endif // WXWIN_COMPATIBILITY_2_8



namespace wxPrivate
{

// This is a helper for the wxVectorSort function, and should not be used
// directly in user's code.
template<typename T>
struct wxVectorComparator
{
    static int
    Compare(const void* pitem1, const void* pitem2, const void* )
    {
        const T& item1 = *reinterpret_cast<const T*>(pitem1);
        const T& item2 = *reinterpret_cast<const T*>(pitem2);

        if (item1 < item2)
            return -1;
        else if (item2 < item1)
            return 1;
        else
            return 0;
    }
};

}  // namespace wxPrivate



template<typename T>
void wxVectorSort(wxVector<T>& v)
{
    wxQsort(v.begin(), v.size(), sizeof(T),
            wxPrivate::wxVectorComparator<T>::Compare, NULL);
}



#endif // wxUSE_STD_CONTAINERS/!wxUSE_STD_CONTAINERS

#if WXWIN_COMPATIBILITY_2_8
    #define WX_DECLARE_VECTORBASE(obj, cls) typedef wxVector<obj> cls
    #define _WX_DECLARE_VECTOR(obj, cls, exp) WX_DECLARE_VECTORBASE(obj, cls)
    #define WX_DECLARE_VECTOR(obj, cls) WX_DECLARE_VECTORBASE(obj, cls)
#endif // WXWIN_COMPATIBILITY_2_8

#endif // _WX_VECTOR_H_
