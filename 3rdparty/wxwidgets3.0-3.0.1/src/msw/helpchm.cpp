/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/helpchm.cpp
// Purpose:     Help system: MS HTML Help implementation
// Author:      Julian Smart
// Modified by: Vadim Zeitlin at 2008-03-01: refactoring, simplification
// Created:     16/04/2000
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_HELP && wxUSE_MS_HTML_HELP

#include "wx/filename.h"
#include "wx/msw/helpchm.h"

#include "wx/dynload.h"

#ifndef WX_PRECOMP
    #include "wx/intl.h"
    #include "wx/app.h"
#endif

#include "wx/msw/private.h"
#include "wx/msw/htmlhelp.h"

// ----------------------------------------------------------------------------
// utility functions to manage the loading/unloading
// of hhctrl.ocx
// ----------------------------------------------------------------------------

#ifndef UNICODE
    typedef HWND ( WINAPI * HTMLHELP )( HWND, LPCSTR, UINT, DWORD );
    #define HTMLHELP_NAME wxT("HtmlHelpA")
#else // ANSI
    typedef HWND ( WINAPI * HTMLHELP )( HWND, LPCWSTR, UINT, DWORD );
    #define HTMLHELP_NAME wxT("HtmlHelpW")
#endif

HTMLHELP GetHtmlHelpFunction()
{
    static HTMLHELP s_htmlHelp = NULL;

    if ( !s_htmlHelp )
    {
        static wxDynamicLibrary s_dllHtmlHelp(wxT("HHCTRL.OCX"), wxDL_VERBATIM);

        if ( !s_dllHtmlHelp.IsLoaded() )
        {
            wxLogError(_("MS HTML Help functions are unavailable because the MS HTML Help library is not installed on this machine. Please install it."));
        }
        else
        {
            s_htmlHelp = (HTMLHELP)s_dllHtmlHelp.GetSymbol(HTMLHELP_NAME);
            if ( !s_htmlHelp )
            {
                wxLogError(_("Failed to initialize MS HTML Help."));
            }
        }
    }

    return s_htmlHelp;
}

// find the window to use in HtmlHelp() call: use the given one by default but
// fall back to the top level app window and then the desktop if it's NULL
static HWND GetSuitableHWND(wxWindow *win)
{
    if ( !win && wxTheApp )
        win = wxTheApp->GetTopWindow();

    return win ? GetHwndOf(win) : ::GetDesktopWindow();
}


IMPLEMENT_DYNAMIC_CLASS(wxCHMHelpController, wxHelpControllerBase)

bool wxCHMHelpController::Initialize(const wxString& filename)
{
    if ( !GetHtmlHelpFunction() )
        return false;

    m_helpFile = filename;
    return true;
}

bool wxCHMHelpController::LoadFile(const wxString& file)
{
    if (!file.IsEmpty())
        m_helpFile = file;
    return true;
}

/* static */ bool
wxCHMHelpController::CallHtmlHelp(wxWindow *win,
                                  const wxChar *str,
                                  unsigned cmd,
                                  WXWPARAM param)
{
    HTMLHELP htmlHelp = GetHtmlHelpFunction();

    return htmlHelp && htmlHelp(GetSuitableHWND(win), str, cmd, param);
}

bool wxCHMHelpController::DisplayContents()
{
    if (m_helpFile.IsEmpty())
        return false;

    return CallHtmlHelp(HH_DISPLAY_TOPIC);
}

// Use topic or HTML filename
bool wxCHMHelpController::DisplaySection(const wxString& section)
{
    if (m_helpFile.IsEmpty())
        return false;

    // Is this an HTML file or a keyword?
    if ( section.Find(wxT(".htm")) != wxNOT_FOUND )
    {
        // interpret as a file name
        return CallHtmlHelp(HH_DISPLAY_TOPIC, wxMSW_CONV_LPCTSTR(section));
    }

    return KeywordSearch(section);
}

// Use context number
bool wxCHMHelpController::DisplaySection(int section)
{
    if (m_helpFile.IsEmpty())
        return false;

    return CallHtmlHelp(HH_HELP_CONTEXT, section);
}

/* static */
bool
wxCHMHelpController::DoDisplayTextPopup(const wxChar *text,
                                        const wxPoint& pos,
                                        int contextId,
                                        wxWindow *window)
{
    HH_POPUP popup;
    popup.cbStruct = sizeof(popup);
    popup.hinst = (HINSTANCE) wxGetInstance();
    popup.idString = contextId;
    popup.pszText = text;
    popup.pt.x = pos.x;
    popup.pt.y = pos.y;
    popup.clrForeground = ::GetSysColor(COLOR_INFOTEXT);
    popup.clrBackground = ::GetSysColor(COLOR_INFOBK);
    popup.rcMargins.top =
    popup.rcMargins.left =
    popup.rcMargins.right =
    popup.rcMargins.bottom = -1;
    popup.pszFont = NULL;

    return CallHtmlHelp(window, NULL, HH_DISPLAY_TEXT_POPUP, &popup);
}

bool wxCHMHelpController::DisplayContextPopup(int contextId)
{
    return DoDisplayTextPopup(NULL, wxGetMousePosition(), contextId,
                              GetParentWindow());
}

bool
wxCHMHelpController::DisplayTextPopup(const wxString& text, const wxPoint& pos)
{
    return ShowContextHelpPopup(text, pos, GetParentWindow());
}

/* static */
bool wxCHMHelpController::ShowContextHelpPopup(const wxString& text,
                                               const wxPoint& pos,
                                               wxWindow *window)
{
    return DoDisplayTextPopup(text.t_str(), pos, 0, window);
}

bool wxCHMHelpController::DisplayBlock(long block)
{
    return DisplaySection(block);
}

bool wxCHMHelpController::KeywordSearch(const wxString& k,
                                        wxHelpSearchMode WXUNUSED(mode))
{
    if (m_helpFile.IsEmpty())
        return false;

    HH_AKLINK link;
    link.cbStruct =     sizeof(HH_AKLINK);
    link.fReserved =    FALSE;
    link.pszKeywords =  k.t_str();
    link.pszUrl =       NULL;
    link.pszMsgText =   NULL;
    link.pszMsgTitle =  NULL;
    link.pszWindow =    NULL;
    link.fIndexOnFail = TRUE;

    return CallHtmlHelp(HH_KEYWORD_LOOKUP, &link);
}

bool wxCHMHelpController::Quit()
{
    return CallHtmlHelp(NULL, NULL, HH_CLOSE_ALL);
}

wxString wxCHMHelpController::GetValidFilename() const
{
    wxString path, name, ext;
    wxFileName::SplitPath(m_helpFile, &path, &name, &ext);

    wxString fullName;
    if (path.IsEmpty())
        fullName = name + wxT(".chm");
    else if (path.Last() == wxT('\\'))
        fullName = path + name + wxT(".chm");
    else
        fullName = path + wxT("\\") + name + wxT(".chm");
    return fullName;
}

#endif // wxUSE_HELP
