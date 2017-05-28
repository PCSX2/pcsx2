///////////////////////////////////////////////////////////////////////////////
// Name:        src/common/threadinfo.cpp
// Purpose:     declaration of wxThreadSpecificInfo: thread-specific information
// Author:      Vaclav Slavik
// Created:     2013-09-14
// Copyright:   (c) 2013 Vaclav Slavik <vslavik@fastmail.fm>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if defined(__BORLANDC__)
    #pragma hdrstop
#endif

#include "wx/private/threadinfo.h"

#if wxUSE_THREADS

#include "wx/tls.h"
#include "wx/thread.h"
#include "wx/sharedptr.h"
#include "wx/vector.h"

namespace
{

// All thread info objects are stored in a global list so that they are
// freed when global objects are destroyed and no memory leaks are reported.

// Notice that we must be using accessor functions instead of simple global
// variables here as this code could be executed during global initialization
// time, i.e. before any globals in this module were initialzied.
inline wxCriticalSection& GetAllThreadInfosCS()
{
    static wxCriticalSection s_csAllThreadInfos;

    return s_csAllThreadInfos;
}

typedef wxVector< wxSharedPtr<wxThreadSpecificInfo> > wxAllThreadInfos;
inline wxAllThreadInfos& GetAllThreadInfos()
{
    static wxAllThreadInfos s_allThreadInfos;

    return s_allThreadInfos;
}

// Pointer to the current thread's instance
inline wxTLS_TYPE_REF(wxThreadSpecificInfo*) GetThisThreadInfo()
{
    static wxTLS_TYPE(wxThreadSpecificInfo*) s_thisThreadInfo;

    return s_thisThreadInfo;
}

#define wxTHIS_THREAD_INFO wxTLS_VALUE(GetThisThreadInfo())

} // anonymous namespace


wxThreadSpecificInfo& wxThreadSpecificInfo::Get()
{
    if ( !wxTHIS_THREAD_INFO )
    {
        wxTHIS_THREAD_INFO = new wxThreadSpecificInfo;
        wxCriticalSectionLocker lock(GetAllThreadInfosCS());
        GetAllThreadInfos().push_back(
                wxSharedPtr<wxThreadSpecificInfo>(wxTHIS_THREAD_INFO));
    }
    return *wxTHIS_THREAD_INFO;
}

void wxThreadSpecificInfo::ThreadCleanUp()
{
    if ( !wxTHIS_THREAD_INFO )
        return; // nothing to do, not used by this thread at all

    // find this thread's instance in GetAllThreadInfos() and destroy it
    wxCriticalSectionLocker lock(GetAllThreadInfosCS());
    for ( wxAllThreadInfos::iterator i = GetAllThreadInfos().begin();
          i != GetAllThreadInfos().end();
          ++i )
    {
        if ( i->get() == wxTHIS_THREAD_INFO )
        {
            GetAllThreadInfos().erase(i);
            wxTHIS_THREAD_INFO = NULL;
            break;
        }
    }
}

#else // !wxUSE_THREADS

wxThreadSpecificInfo& wxThreadSpecificInfo::Get()
{
    static wxThreadSpecificInfo s_instance;
    return s_instance;
}

#endif // wxUSE_THREADS/wxUSE_THREADS
