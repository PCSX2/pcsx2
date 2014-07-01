///////////////////////////////////////////////////////////////////////////////
// Name:        wx/aboutdlg.h
// Purpose:     declaration of wxAboutDialog class
// Author:      Vadim Zeitlin
// Created:     2006-10-07
// Copyright:   (c) 2006 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_ABOUTDLG_H_
#define _WX_ABOUTDLG_H_

#include "wx/defs.h"

#if wxUSE_ABOUTDLG

#include "wx/app.h"
#include "wx/icon.h"

// ----------------------------------------------------------------------------
// wxAboutDialogInfo: information shown by the standard "About" dialog
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxAboutDialogInfo
{
public:
    // all fields are initially uninitialized
    wxAboutDialogInfo() { }

    // accessors for various simply fields
    // -----------------------------------

    // name of the program, if not used defaults to wxApp::GetAppDisplayName()
    void SetName(const wxString& name) { m_name = name; }
    wxString GetName() const
        { return m_name.empty() ? wxTheApp->GetAppDisplayName() : m_name; }

    // version should contain program version without "version" word (e.g.,
    // "1.2" or "RC2") while longVersion may contain the full version including
    // "version" word (e.g., "Version 1.2" or "Release Candidate 2")
    //
    // if longVersion is empty, it is automatically constructed from version
    //
    // generic and gtk native: use short version only, as a suffix to the
    // program name msw and osx native: use long version
    void SetVersion(const wxString& version,
                    const wxString& longVersion = wxString());

    bool HasVersion() const { return !m_version.empty(); }
    const wxString& GetVersion() const { return m_version; }
    const wxString& GetLongVersion() const { return m_longVersion; }

    // brief, but possibly multiline, description of the program
    void SetDescription(const wxString& desc) { m_description = desc; }
    bool HasDescription() const { return !m_description.empty(); }
    const wxString& GetDescription() const { return m_description; }

    // short string containing the program copyright information
    void SetCopyright(const wxString& copyright) { m_copyright = copyright; }
    bool HasCopyright() const { return !m_copyright.empty(); }
    const wxString& GetCopyright() const { return m_copyright; }

    // long, multiline string containing the text of the program licence
    void SetLicence(const wxString& licence) { m_licence = licence; }
    void SetLicense(const wxString& licence) { m_licence = licence; }
    bool HasLicence() const { return !m_licence.empty(); }
    const wxString& GetLicence() const { return m_licence; }

    // icon to be shown in the dialog, defaults to the main frame icon
    void SetIcon(const wxIcon& icon) { m_icon = icon; }
    bool HasIcon() const { return m_icon.IsOk(); }
    wxIcon GetIcon() const;

    // web site for the program and its description (defaults to URL itself if
    // empty)
    void SetWebSite(const wxString& url, const wxString& desc = wxEmptyString)
    {
        m_url = url;
        m_urlDesc = desc.empty() ? url : desc;
    }

    bool HasWebSite() const { return !m_url.empty(); }

    const wxString& GetWebSiteURL() const { return m_url; }
    const wxString& GetWebSiteDescription() const { return m_urlDesc; }

    // accessors for the arrays
    // ------------------------

    // the list of developers of the program
    void SetDevelopers(const wxArrayString& developers)
        { m_developers = developers; }
    void AddDeveloper(const wxString& developer)
        { m_developers.push_back(developer); }

    bool HasDevelopers() const { return !m_developers.empty(); }
    const wxArrayString& GetDevelopers() const { return m_developers; }

    // the list of documentation writers
    void SetDocWriters(const wxArrayString& docwriters)
        { m_docwriters = docwriters; }
    void AddDocWriter(const wxString& docwriter)
        { m_docwriters.push_back(docwriter); }

    bool HasDocWriters() const { return !m_docwriters.empty(); }
    const wxArrayString& GetDocWriters() const { return m_docwriters; }

    // the list of artists for the program art
    void SetArtists(const wxArrayString& artists)
        { m_artists = artists; }
    void AddArtist(const wxString& artist)
        { m_artists.push_back(artist); }

    bool HasArtists() const { return !m_artists.empty(); }
    const wxArrayString& GetArtists() const { return m_artists; }

    // the list of translators
    void SetTranslators(const wxArrayString& translators)
        { m_translators = translators; }
    void AddTranslator(const wxString& translator)
        { m_translators.push_back(translator); }

    bool HasTranslators() const { return !m_translators.empty(); }
    const wxArrayString& GetTranslators() const { return m_translators; }


    // implementation only
    // -------------------

    // "simple" about dialog shows only textual information (with possibly
    // default icon but without hyperlink nor any long texts such as the
    // licence text)
    bool IsSimple() const
        { return !HasWebSite() && !HasIcon() && !HasLicence(); }

    // get the description and credits (i.e. all of developers, doc writers,
    // artists and translators) as a one long multiline string
    wxString GetDescriptionAndCredits() const;

    // returns the copyright with the (C) string substituted by the Unicode
    // character U+00A9
    wxString GetCopyrightToDisplay() const;

private:
    wxString m_name,
             m_version,
             m_longVersion,
             m_description,
             m_copyright,
             m_licence;

    wxIcon m_icon;

    wxString m_url,
             m_urlDesc;

    wxArrayString m_developers,
                  m_docwriters,
                  m_artists,
                  m_translators;
};

// functions to show the about dialog box
WXDLLIMPEXP_ADV void wxAboutBox(const wxAboutDialogInfo& info, wxWindow* parent = NULL);

#endif // wxUSE_ABOUTDLG

#endif // _WX_ABOUTDLG_H_

