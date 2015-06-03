/////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/helpwin.h
// Purpose:     Help system: WinHelp implementation
// Author:      Julian Smart
// Modified by:
// Created:     04/01/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_HELPWIN_H_
#define _WX_HELPWIN_H_

#include "wx/wx.h"

#if wxUSE_HELP

#include "wx/helpbase.h"

class WXDLLIMPEXP_CORE wxWinHelpController: public wxHelpControllerBase
{
    wxDECLARE_DYNAMIC_CLASS(wxWinHelpController);

public:
    wxWinHelpController(wxWindow* parentWindow = NULL): wxHelpControllerBase(parentWindow) {}
    virtual ~wxWinHelpController() {}

    // Must call this to set the filename
    virtual bool Initialize(const wxString& file);
    virtual bool Initialize(const wxString& file, int WXUNUSED(server) ) { return Initialize( file ); }

    // If file is "", reloads file given in Initialize
    virtual bool LoadFile(const wxString& file = wxEmptyString);
    virtual bool DisplayContents();
    virtual bool DisplaySection(int sectionNo);
    virtual bool DisplaySection(const wxString& section) { return KeywordSearch(section); }
    virtual bool DisplayBlock(long blockNo);
    virtual bool DisplayContextPopup(int contextId);
    virtual bool KeywordSearch(const wxString& k,
                               wxHelpSearchMode mode = wxHELP_SEARCH_ALL);
    virtual bool Quit();

    inline wxString GetHelpFile() const { return m_helpFile; }

protected:
    // Append extension if necessary.
    wxString GetValidFilename(const wxString& file) const;

private:
    wxString m_helpFile;
};

#endif // wxUSE_HELP
#endif
// _WX_HELPWIN_H_
