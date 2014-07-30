///////////////////////////////////////////////////////////////////////////////
// Name:        wx/os2/tls.h
// Purpose:     OS/2 implementation of wxTlsValue<>
// Author:      Stefan Neis
// Created:     2008-08-30
// Copyright:   (c) 2008 Stefan Neis
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_OS2_TLS_H_
#define _WX_OS2_TLS_H_

#include "wx/os2/private.h"
#include "wx/thread.h"
#include "wx/vector.h"

// ----------------------------------------------------------------------------
// wxTlsKey is a helper class encapsulating a TLS slot
// ----------------------------------------------------------------------------

class wxTlsKey
{
public:
    // ctor allocates a new key
    wxTlsKey(wxTlsDestructorFunction destructor)
    {
        m_destructor = destructor;
        APIRET rc = ::DosAllocThreadLocalMemory(1, &m_slot);
        if (rc != NO_ERROR)
            m_slot = NULL;
    }

    // return true if the key was successfully allocated
    bool IsOk() const { return m_slot != NULL; }

    // get the key value, there is no error return
    void *Get() const
    {
        return (void *)m_slot;
    }

    // change the key value, return true if ok
    bool Set(void *value)
    {
        void *old = Get();

        m_slot = (ULONG*)value;

        if ( old )
            m_destructor(old);

        // update m_allValues list of all values - remove old, add new
        wxCriticalSectionLocker lock(m_csAllValues);
        if ( old )
        {
            for ( wxVector<void*>::iterator i = m_allValues.begin();
                  i != m_allValues.end();
                  ++i )
            {
                if ( *i == old )
                {
                    if ( value )
                        *i = value;
                    else
                        m_allValues.erase(i);
                    return true;
                }
            }
            wxFAIL_MSG( "previous wxTlsKey value not recorded in m_allValues" );
        }

        if ( value )
            m_allValues.push_back(value);

        return true;
    }

    // free the key
    ~wxTlsKey()
    {
        if ( !IsOk() )
            return;

        // Win32 and OS/2 API doesn't have the equivalent of pthread's
        // destructor, so we have to keep track of all allocated values and
        // destroy them manually; ideally we'd do that at thread exit time, but
        // since we could only do that with wxThread and not otherwise created
        // threads, we do it here.
        //
        // TODO: We should still call destructors for wxTlsKey used in the
        //       thread from wxThread's thread shutdown code, *in addition*
        //       to doing it in ~wxTlsKey.
        //
        // NB: No need to lock m_csAllValues, by the time this code is called,
        //     no other thread can be using this key.
        for ( wxVector<void*>::iterator i = m_allValues.begin();
              i != m_allValues.end();
              ++i )
        {
            m_destructor(*i);
        }

        ::DosFreeThreadLocalMemory(m_slot);
    }

private:
    wxTlsDestructorFunction m_destructor;
    ULONG* m_slot;

    wxVector<void*> m_allValues;
    wxCriticalSection m_csAllValues;

    wxDECLARE_NO_COPY_CLASS(wxTlsKey);
};

#endif // _WX_OS2_TLS_H_

