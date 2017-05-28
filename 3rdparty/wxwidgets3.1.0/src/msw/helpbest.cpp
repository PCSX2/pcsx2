/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/helpbest.cpp
// Purpose:     Tries to load MS HTML Help, falls back to wxHTML upon failure
// Author:      Mattia Barbon
// Modified by:
// Created:     02/04/2001
// Copyright:   (c) Mattia Barbon
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/log.h"
#endif

#include "wx/filename.h"

#if wxUSE_HELP && wxUSE_MS_HTML_HELP \
    && wxUSE_WXHTML_HELP && !defined(__WXUNIVERSAL__)

#include "wx/msw/helpchm.h"
#include "wx/html/helpctrl.h"
#include "wx/msw/helpbest.h"

wxIMPLEMENT_DYNAMIC_CLASS(wxBestHelpController, wxHelpControllerBase);

bool wxBestHelpController::Initialize( const wxString& filename )
{
    // try wxCHMHelpController
    wxCHMHelpController* chm = new wxCHMHelpController(m_parentWindow);

    m_helpControllerType = wxUseChmHelp;
    // do not warn upon failure
    wxLogNull dontWarnOnFailure;

    if( chm->Initialize( GetValidFilename( filename ) ) )
    {
        m_helpController = chm;
        m_parentWindow = NULL;
        return true;
    }

    // failed
    delete chm;

    // try wxHtmlHelpController
    wxHtmlHelpController *
        html = new wxHtmlHelpController(m_style, m_parentWindow);

    m_helpControllerType = wxUseHtmlHelp;
    if( html->Initialize( GetValidFilename( filename ) ) )
    {
        m_helpController = html;
        m_parentWindow = NULL;
        return true;
    }

    // failed
    delete html;

    return false;
}

wxString wxBestHelpController::GetValidFilename( const wxString& filename ) const
{
    wxFileName fn(filename);

    switch( m_helpControllerType )
    {
        case wxUseChmHelp:
            fn.SetExt("chm");
            if( fn.FileExists() )
                return fn.GetFullPath();

            return filename;

        case wxUseHtmlHelp:
            fn.SetExt("htb");
            if( fn.FileExists() )
                return fn.GetFullPath();

            fn.SetExt("zip");
            if( fn.FileExists() )
                return fn.GetFullPath();

            fn.SetExt("hhp");
            if( fn.FileExists() )
                return fn.GetFullPath();

            return filename;

        default:
            // we CAN'T get here
            wxFAIL_MSG( wxT("wxBestHelpController: Must call Initialize, first!") );
    }

    return wxEmptyString;
}

#endif
    // wxUSE_HELP && wxUSE_MS_HTML_HELP && wxUSE_WXHTML_HELP
