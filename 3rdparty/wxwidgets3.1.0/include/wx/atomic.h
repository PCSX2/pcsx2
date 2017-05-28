/////////////////////////////////////////////////////////////////////////////
// Name:        wx/atomic.h
// Purpose:     functions to manipulate atomically integers and pointers
// Author:      Armel Asselin
// Created:     12/13/2006
// Copyright:   (c) Armel Asselin
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_ATOMIC_H_
#define _WX_ATOMIC_H_

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// get the value of wxUSE_THREADS configuration flag
#include "wx/defs.h"

// constraints on the various functions:
//  - wxAtomicDec must return a zero value if the value is zero once
//  decremented else it must return any non-zero value (the true value is OK
//  but not necessary).

#if wxUSE_THREADS

#if defined(HAVE_GCC_ATOMIC_BUILTINS)

// NB: we intentionally don't use Linux's asm/atomic.h header, because it's
//     an internal kernel header that doesn't always work in userspace:
//     http://bugs.mysql.com/bug.php?id=28456
//     http://golubenco.org/blog/atomic-operations/

inline void wxAtomicInc (wxUint32 &value)
{
    __sync_fetch_and_add(&value, 1);
}

inline wxUint32 wxAtomicDec (wxUint32 &value)
{
    return __sync_sub_and_fetch(&value, 1);
}


#elif defined(__WINDOWS__)

// include standard Windows headers
#include "wx/msw/wrapwin.h"

inline void wxAtomicInc (wxUint32 &value)
{
    InterlockedIncrement ((LONG*)&value);
}

inline wxUint32 wxAtomicDec (wxUint32 &value)
{
    return InterlockedDecrement ((LONG*)&value);
}

#elif defined(__DARWIN__)

#include "libkern/OSAtomic.h"
inline void wxAtomicInc (wxUint32 &value)
{
    OSAtomicIncrement32 ((int32_t*)&value);
}

inline wxUint32 wxAtomicDec (wxUint32 &value)
{
    return OSAtomicDecrement32 ((int32_t*)&value);
}

#elif defined (__SOLARIS__)

#include <atomic.h>

inline void wxAtomicInc (wxUint32 &value)
{
    atomic_add_32 ((uint32_t*)&value, 1);
}

inline wxUint32 wxAtomicDec (wxUint32 &value)
{
    return atomic_add_32_nv ((uint32_t*)&value, (uint32_t)-1);
}

#else // unknown platform

// it will result in inclusion if the generic implementation code a bit later in this page
#define wxNEEDS_GENERIC_ATOMIC_OPS

#endif // unknown platform

#else // else of wxUSE_THREADS
// if no threads are used we can safely use simple ++/--

inline void wxAtomicInc (wxUint32 &value) { ++value; }
inline wxUint32 wxAtomicDec (wxUint32 &value) { return --value; }

#endif // !wxUSE_THREADS

// ----------------------------------------------------------------------------
// proxies to actual implementations, but for various other types with same
//  behaviour
// ----------------------------------------------------------------------------

#ifdef wxNEEDS_GENERIC_ATOMIC_OPS

#include "wx/thread.h" // for wxCriticalSection

class wxAtomicInt32
{
public:
    wxAtomicInt32() { } // non initialized for consistency with basic int type
    wxAtomicInt32(wxInt32 v) : m_value(v) { }
    wxAtomicInt32(const wxAtomicInt32& a) : m_value(a.m_value) {}

    operator wxInt32() const { return m_value; }
    operator volatile wxInt32&() { return m_value; }

    wxAtomicInt32& operator=(wxInt32 v) { m_value = v; return *this; }

    void Inc()
    {
        wxCriticalSectionLocker lock(m_locker);
        ++m_value;
    }

    wxInt32 Dec()
    {
        wxCriticalSectionLocker lock(m_locker);
        return --m_value;
    }

private:
    volatile wxInt32  m_value;
    wxCriticalSection m_locker;
};

inline void wxAtomicInc(wxAtomicInt32 &value) { value.Inc(); }
inline wxInt32 wxAtomicDec(wxAtomicInt32 &value) { return value.Dec(); }

#else // !wxNEEDS_GENERIC_ATOMIC_OPS

#define wxHAS_ATOMIC_OPS

inline void wxAtomicInc(wxInt32 &value) { wxAtomicInc((wxUint32&)value); }
inline wxInt32 wxAtomicDec(wxInt32 &value) { return wxAtomicDec((wxUint32&)value); }

typedef wxInt32 wxAtomicInt32;

#endif // wxNEEDS_GENERIC_ATOMIC_OPS

// all the native implementations use 32 bits currently
// for a 64 bits implementation we could use (a future) wxAtomicInt64 as
// default type
typedef wxAtomicInt32 wxAtomicInt;

#endif // _WX_ATOMIC_H_
