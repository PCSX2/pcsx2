///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/utilswin.cpp
// Purpose:     Various utility functions only available in Windows GUI
// Author:      Vadim Zeitlin
// Modified by:
// Created:     21.06.2003 (extracted from msw/utils.cpp)
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/utils.h"
#endif //WX_PRECOMP

#include "wx/msw/private.h"     // includes <windows.h>
#include "wx/msw/registry.h"
#include <shellapi.h> // needed for SHELLEXECUTEINFO

// ----------------------------------------------------------------------------
// Launch document with default app
// ----------------------------------------------------------------------------

bool wxLaunchDefaultApplication(const wxString& document, int flags)
{
    wxUnusedVar(flags);

    WinStruct<SHELLEXECUTEINFO> sei;
    sei.lpFile = document.t_str();
#ifdef __WXWINCE__
    sei.nShow = SW_SHOWNORMAL; // SW_SHOWDEFAULT not defined under CE (#10216)
#else
    sei.nShow = SW_SHOWDEFAULT;
#endif

    // avoid Windows message box in case of error for consistency with
    // wxLaunchDefaultBrowser() even if don't show the error ourselves in this
    // function
    sei.fMask = SEE_MASK_FLAG_NO_UI;

    if ( ::ShellExecuteEx(&sei) )
        return true;

    return false;
}

// ----------------------------------------------------------------------------
// Launch default browser
// ----------------------------------------------------------------------------

bool wxDoLaunchDefaultBrowser(const wxString& url, const wxString& scheme, int flags)
{
    wxUnusedVar(flags);

#if wxUSE_IPC
    if ( flags & wxBROWSER_NEW_WINDOW )
    {
        // ShellExecuteEx() opens the URL in an existing window by default so
        // we can't use it if we need a new window
        wxRegKey key(wxRegKey::HKCR, scheme + wxT("\\shell\\open"));
        if ( !key.Exists() )
        {
            // try the default browser, it must be registered at least for http URLs
            key.SetName(wxRegKey::HKCR, wxT("http\\shell\\open"));
        }

        if ( key.Exists() )
        {
            wxRegKey keyDDE(key, wxT("DDEExec"));
            if ( keyDDE.Exists() )
            {
                // we only know the syntax of WWW_OpenURL DDE request for IE,
                // optimistically assume that all other browsers are compatible
                // with it
                static const wxChar *TOPIC_OPEN_URL = wxT("WWW_OpenURL");
                wxString ddeCmd;
                wxRegKey keyTopic(keyDDE, wxT("topic"));
                bool ok = keyTopic.Exists() &&
                            keyTopic.QueryDefaultValue() == TOPIC_OPEN_URL;
                if ( ok )
                {
                    ddeCmd = keyDDE.QueryDefaultValue();
                    ok = !ddeCmd.empty();
                }

                if ( ok )
                {
                    // for WWW_OpenURL, the index of the window to open the URL
                    // in may be -1 (meaning "current") by default, replace it
                    // with 0 which means "new" (see KB article 160957), but
                    // don't fail if there is no -1 as at least for recent
                    // Firefox versions the default value already is 0
                    ddeCmd.Replace(wxT("-1"), wxT("0"),
                                   false /* only first occurrence */);

                    // and also replace the parameters: the topic should
                    // contain a placeholder for the URL and we should fail if
                    // we didn't find it as this would mean that we have no way
                    // of passing the URL to the browser
                    ok = ddeCmd.Replace(wxT("%1"), url, false) == 1;
                }

                if ( ok )
                {
                    // try to send it the DDE request now but ignore the errors
                    wxLogNull noLog;

                    const wxString ddeServer = wxRegKey(keyDDE, wxT("application"));
                    if ( wxExecuteDDE(ddeServer, TOPIC_OPEN_URL, ddeCmd) )
                        return true;

                    // this is not necessarily an error: maybe browser is
                    // simply not running, but no matter, in any case we're
                    // going to launch it using ShellExecuteEx() below now and
                    // we shouldn't try to open a new window if we open a new
                    // browser anyhow
                }
            }
        }
    }
#endif // wxUSE_IPC

    WinStruct<SHELLEXECUTEINFO> sei;
    sei.lpFile = url.c_str();
    sei.lpVerb = wxT("open");
    sei.nShow = SW_SHOWNORMAL;
    sei.fMask = SEE_MASK_FLAG_NO_UI; // we give error message ourselves

    if ( ::ShellExecuteEx(&sei) )
        return true;

    return false;
}
