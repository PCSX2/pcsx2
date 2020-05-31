/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/sysopt.cpp
// Purpose:     wxSystemOptions
// Author:      Julian Smart
// Modified by:
// Created:     2001-07-10
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ---------------------------------------------------------------------------
// headers
// ---------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if defined(__BORLANDC__)
    #pragma hdrstop
#endif

#if wxUSE_SYSTEM_OPTIONS

#include "wx/sysopt.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/list.h"
    #include "wx/string.h"
    #include "wx/arrstr.h"
#endif

// ----------------------------------------------------------------------------
// private globals
// ----------------------------------------------------------------------------

static wxArrayString gs_optionNames,
                     gs_optionValues;

// ============================================================================
// wxSystemOptions implementation
// ============================================================================

// Option functions (arbitrary name/value mapping)
void wxSystemOptions::SetOption(const wxString& name, const wxString& value)
{
    int idx = gs_optionNames.Index(name, false);
    if (idx == wxNOT_FOUND)
    {
        gs_optionNames.Add(name);
        gs_optionValues.Add(value);
    }
    else
    {
        gs_optionNames[idx] = name;
        gs_optionValues[idx] = value;
    }
}

void wxSystemOptions::SetOption(const wxString& name, int value)
{
    SetOption(name, wxString::Format(wxT("%d"), value));
}

wxString wxSystemOptions::GetOption(const wxString& name)
{
    wxString val;

    int idx = gs_optionNames.Index(name, false);
    if ( idx != wxNOT_FOUND )
    {
        val = gs_optionValues[idx];
    }
    else // not set explicitly
    {
        // look in the environment: first for a variable named "wx_appname_name"
        // which can be set to affect the behaviour or just this application
        // and then for "wx_name" which can be set to change the option globally
        wxString var(name);
        var.Replace(wxT("."), wxT("_"));  // '.'s not allowed in env var names
        var.Replace(wxT("-"), wxT("_"));  // and neither are '-'s

        wxString appname;
        if ( wxTheApp )
            appname = wxTheApp->GetAppName();

        if ( !appname.empty() )
            val = wxGetenv(wxT("wx_") + appname + wxT('_') + var);

        if ( val.empty() )
            val = wxGetenv(wxT("wx_") + var);
    }

    return val;
}

int wxSystemOptions::GetOptionInt(const wxString& name)
{
    return wxAtoi (GetOption(name));
}

bool wxSystemOptions::HasOption(const wxString& name)
{
    return !GetOption(name).empty();
}

#endif // wxUSE_SYSTEM_OPTIONS
