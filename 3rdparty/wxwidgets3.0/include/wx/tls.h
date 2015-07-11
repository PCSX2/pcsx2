///////////////////////////////////////////////////////////////////////////////
// Name:        wx/tls.h
// Purpose:     Implementation of thread local storage
// Author:      Vadim Zeitlin
// Created:     2008-08-08
// Copyright:   (c) 2008 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_TLS_H_
#define _WX_TLS_H_

#include "wx/defs.h"

// ----------------------------------------------------------------------------
// check for compiler support of thread-specific variables
// ----------------------------------------------------------------------------

// when not using threads at all, there is no need for thread-specific
// values to be really thread-specific
#if !wxUSE_THREADS
    #define wxHAS_COMPILER_TLS
    #define wxTHREAD_SPECIFIC_DECL
// otherwise try to find the compiler-specific way to handle TLS unless
// explicitly disabled by setting wxUSE_COMPILER_TLS to 0 (it is 1 by default).
#elif wxUSE_COMPILER_TLS
// __thread keyword is not supported correctly by MinGW, at least in some
// configurations, see http://sourceforge.net/support/tracker.php?aid=2837047
// and when in doubt we prefer to not use it at all.
#if defined(HAVE___THREAD_KEYWORD) && !defined(__MINGW32__)
    #define wxHAS_COMPILER_TLS
    #define wxTHREAD_SPECIFIC_DECL __thread
// MSVC has its own version which might be supported by some other Windows
// compilers, to be tested
#elif wxCHECK_VISUALC_VERSION(7)
    #define wxHAS_COMPILER_TLS
    #define wxTHREAD_SPECIFIC_DECL __declspec(thread)
#endif // compilers
#endif // wxUSE_COMPILER_TLS

// ----------------------------------------------------------------------------
// define wxTLS_TYPE()
// ----------------------------------------------------------------------------

#ifdef wxHAS_COMPILER_TLS
    #define wxTLS_TYPE(T) wxTHREAD_SPECIFIC_DECL T
    #define wxTLS_TYPE_REF(T) T&
    #define wxTLS_PTR(var) (&(var))
    #define wxTLS_VALUE(var) (var)
#else // !wxHAS_COMPILER_TLS

    extern "C"
    {
        typedef void (*wxTlsDestructorFunction)(void*);
    }

    #if defined(__WINDOWS__)
        #include "wx/msw/tls.h"
    #elif defined(__OS2__)
        #include "wx/os2/tls.h"
    #elif defined(__UNIX__)
        #include "wx/unix/tls.h"
    #else
        // TODO: we could emulate TLS for such platforms...
        #error Neither compiler nor OS support thread-specific variables.
    #endif

    #include <stdlib.h> // for calloc()

    // wxTlsValue<T> represents a thread-specific value of type T but, unlike
    // with native compiler thread-specific variables, it behaves like a
    // (never NULL) pointer to T and so needs to be dereferenced before use
    //
    // Note: T must be a POD!
    //
    // Note: On Unix, thread-specific T value is freed when the thread exits.
    //       On Windows, thread-specific values are freed later, when given
    //       wxTlsValue<T> is destroyed.  The only exception to this is the
    //       value for the main thread, which is always freed when
    //       wxTlsValue<T> is destroyed.
    template <typename T>
    class wxTlsValue
    {
    public:
        typedef T ValueType;

        // ctor doesn't do anything, the object is created on first access
        wxTlsValue() : m_key(free) {}

        // dtor is only called in the main thread context and so is not enough
        // to free memory allocated by us for the other threads, we use
        // destructor function when using Pthreads for this (which is not
        // called for the main thread as it doesn't call pthread_exit() but
        // just to be safe we also reset the key anyhow)
        ~wxTlsValue()
        {
            if ( m_key.Get() )
                m_key.Set(NULL); // this deletes the value
        }

        // access the object creating it on demand
        ValueType *Get()
        {
            void *value = m_key.Get();
            if ( !value )
            {
                // ValueType must be POD to be used in wxHAS_COMPILER_TLS case
                // anyhow (at least gcc doesn't accept non-POD values being
                // declared with __thread) so initialize it as a POD too
                value = calloc(1, sizeof(ValueType));

                if ( !m_key.Set(value) )
                {
                    free(value);

                    // this will probably result in a crash in the caller but
                    // it's arguably better to crash immediately instead of
                    // slowly dying from out-of-memory errors which would
                    // happen as the next access to this object would allocate
                    // another ValueType instance and so on forever
                    value = NULL;
                }
            }

            return static_cast<ValueType *>(value);
        }

        // pointer-like accessors
        ValueType *operator->() { return Get(); }
        ValueType& operator*() { return *Get(); }

    private:
        wxTlsKey m_key;

        DECLARE_NO_COPY_TEMPLATE_CLASS(wxTlsValue, T)
    };

    #define wxTLS_TYPE(T) wxTlsValue<T>
    #define wxTLS_TYPE_REF(T) wxTLS_TYPE(T)&
    #define wxTLS_PTR(var) ((var).Get())
    #define wxTLS_VALUE(var) (*(var))
#endif // wxHAS_COMPILER_TLS/!wxHAS_COMPILER_TLS

#endif // _WX_TLS_H_

