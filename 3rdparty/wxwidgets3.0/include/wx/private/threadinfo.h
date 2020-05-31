///////////////////////////////////////////////////////////////////////////////
// Name:        wx/private/threadinfo.h
// Purpose:     declaration of wxThreadSpecificInfo: thread-specific information
// Author:      Vadim Zeitlin
// Created:     2009-07-13
// Copyright:   (c) 2009 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PRIVATE_THREADINFO_H_
#define _WX_PRIVATE_THREADINFO_H_

#include "wx/defs.h"

class WXDLLIMPEXP_FWD_BASE wxLog;

#if wxUSE_INTL
#include "wx/hashset.h"
WX_DECLARE_HASH_SET(wxString, wxStringHash, wxStringEqual,
                    wxLocaleUntranslatedStrings);
#endif


// ----------------------------------------------------------------------------
// wxThreadSpecificInfo: contains all thread-specific information used by wx
// ----------------------------------------------------------------------------

// Group all thread-specific information we use (e.g. the active wxLog target)
// a in this class to avoid consuming more TLS slots than necessary as there is
// only a limited number of them.
class wxThreadSpecificInfo
{
public:
    // Return this thread's instance.
    static wxThreadSpecificInfo& Get();

    // the thread-specific logger or NULL if the thread is using the global one
    // (this is not used for the main thread which always uses the global
    // logger)
    wxLog *logger;

    // true if logging is currently disabled for this thread (this is also not
    // used for the main thread which uses wxLog::ms_doLog)
    //
    // NB: we use a counter-intuitive "disabled" flag instead of "enabled" one
    //     because the default, for 0-initialized struct, should be to enable
    //     logging
    bool loggingDisabled;

#if wxUSE_INTL
    // Storage for wxTranslations::GetUntranslatedString()
    wxLocaleUntranslatedStrings untranslatedStrings;
#endif

#if wxUSE_THREADS
    // Cleans up storage for the current thread. Should be called when a thread
    // is being destroyed. If it's not called, the only bad thing that happens
    // is that the memory is deallocated later, on process termination.
    static void ThreadCleanUp();
#endif

private:
    wxThreadSpecificInfo() : logger(NULL), loggingDisabled(false) {}
};

#define wxThreadInfo wxThreadSpecificInfo::Get()

#endif // _WX_PRIVATE_THREADINFO_H_

