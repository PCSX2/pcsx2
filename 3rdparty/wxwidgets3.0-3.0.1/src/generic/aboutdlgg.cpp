///////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/aboutdlgg.cpp
// Purpose:     implements wxGenericAboutBox() function
// Author:      Vadim Zeitlin
// Created:     2006-10-08
// Copyright:   (c) 2006 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_ABOUTDLG

#ifndef WX_PRECOMP
    #include "wx/sizer.h"
    #include "wx/statbmp.h"
    #include "wx/stattext.h"
    #include "wx/button.h"
#endif //WX_PRECOMP

#include "wx/aboutdlg.h"
#include "wx/generic/aboutdlgg.h"

#include "wx/hyperlink.h"
#include "wx/collpane.h"

// ============================================================================
// implementation
// ============================================================================

// helper function: returns all array elements in a single comma-separated and
// newline-terminated string
static wxString AllAsString(const wxArrayString& a)
{
    wxString s;
    const size_t count = a.size();
    s.reserve(20*count);
    for ( size_t n = 0; n < count; n++ )
    {
        s << a[n] << (n == count - 1 ? wxT("\n") : wxT(", "));
    }

    return s;
}

// ----------------------------------------------------------------------------
// wxAboutDialogInfo
// ----------------------------------------------------------------------------

wxString wxAboutDialogInfo::GetDescriptionAndCredits() const
{
    wxString s = GetDescription();
    if ( !s.empty() )
        s << wxT('\n');

    if ( HasDevelopers() )
        s << wxT('\n') << _("Developed by ") << AllAsString(GetDevelopers());

    if ( HasDocWriters() )
        s << wxT('\n') << _("Documentation by ") << AllAsString(GetDocWriters());

    if ( HasArtists() )
        s << wxT('\n') << _("Graphics art by ") << AllAsString(GetArtists());

    if ( HasTranslators() )
        s << wxT('\n') << _("Translations by ") << AllAsString(GetTranslators());

    return s;
}

wxIcon wxAboutDialogInfo::GetIcon() const
{
    wxIcon icon = m_icon;
    if ( !icon.IsOk() && wxTheApp )
    {
        const wxTopLevelWindow * const
            tlw = wxDynamicCast(wxTheApp->GetTopWindow(), wxTopLevelWindow);
        if ( tlw )
            icon = tlw->GetIcon();
    }

    return icon;
}

wxString wxAboutDialogInfo::GetCopyrightToDisplay() const
{
    wxString ret = m_copyright;

#if wxUSE_UNICODE
    const wxString copyrightSign = wxString::FromUTF8("\xc2\xa9");
    ret.Replace("(c)", copyrightSign);
    ret.Replace("(C)", copyrightSign);
#endif // wxUSE_UNICODE

    return ret;
}

void wxAboutDialogInfo::SetVersion(const wxString& version,
                                   const wxString& longVersion)
{
    if ( version.empty() )
    {
        m_version.clear();

        wxASSERT_MSG( longVersion.empty(),
                      "long version should be empty if version is");

        m_longVersion.clear();
    }
    else // setting valid version
    {
        m_version = version;

        if ( longVersion.empty() )
            m_longVersion = _("Version ") + m_version;
        else
            m_longVersion = longVersion;
    }
}

// ----------------------------------------------------------------------------
// wxGenericAboutDialog
// ----------------------------------------------------------------------------

bool wxGenericAboutDialog::Create(const wxAboutDialogInfo& info, wxWindow* parent)
{
    if ( !wxDialog::Create(parent, wxID_ANY, wxString::Format(_("About %s"), info.GetName()),
                           wxDefaultPosition, wxDefaultSize, wxRESIZE_BORDER|wxDEFAULT_DIALOG_STYLE) )
        return false;

    m_sizerText = new wxBoxSizer(wxVERTICAL);
    wxString nameAndVersion = info.GetName();
    if ( info.HasVersion() )
        nameAndVersion << wxT(' ') << info.GetVersion();
    wxStaticText *label = new wxStaticText(this, wxID_ANY, nameAndVersion);
    wxFont fontBig(*wxNORMAL_FONT);
    fontBig.SetPointSize(fontBig.GetPointSize() + 2);
    fontBig.SetWeight(wxFONTWEIGHT_BOLD);
    label->SetFont(fontBig);

    m_sizerText->Add(label, wxSizerFlags().Centre().Border());
    m_sizerText->AddSpacer(5);

    AddText(info.GetCopyrightToDisplay());
    AddText(info.GetDescription());

    if ( info.HasWebSite() )
    {
#if wxUSE_HYPERLINKCTRL
        AddControl(new wxHyperlinkCtrl(this, wxID_ANY,
                                       info.GetWebSiteDescription(),
                                       info.GetWebSiteURL()));
#else
        AddText(info.GetWebSiteURL());
#endif // wxUSE_HYPERLINKCTRL/!wxUSE_HYPERLINKCTRL
    }

#if wxUSE_COLLPANE
    if ( info.HasLicence() )
        AddCollapsiblePane(_("License"), info.GetLicence());

    if ( info.HasDevelopers() )
        AddCollapsiblePane(_("Developers"),
                           AllAsString(info.GetDevelopers()));

    if ( info.HasDocWriters() )
        AddCollapsiblePane(_("Documentation writers"),
                           AllAsString(info.GetDocWriters()));

    if ( info.HasArtists() )
        AddCollapsiblePane(_("Artists"),
                           AllAsString(info.GetArtists()));

    if ( info.HasTranslators() )
        AddCollapsiblePane(_("Translators"),
                           AllAsString(info.GetTranslators()));
#endif // wxUSE_COLLPANE

    DoAddCustomControls();


    wxSizer *sizerIconAndText = new wxBoxSizer(wxHORIZONTAL);
#if wxUSE_STATBMP
    wxIcon icon = info.GetIcon();
    if ( icon.IsOk() )
    {
        sizerIconAndText->Add(new wxStaticBitmap(this, wxID_ANY, icon),
                                wxSizerFlags().Border(wxRIGHT));
    }
#endif // wxUSE_STATBMP
    sizerIconAndText->Add(m_sizerText, wxSizerFlags(1).Expand());

    wxSizer *sizerTop = new wxBoxSizer(wxVERTICAL);
    sizerTop->Add(sizerIconAndText, wxSizerFlags(1).Expand().Border());

// Mac typically doesn't use OK buttons just for dismissing dialogs.
#if !defined(__WXMAC__)
    wxSizer *sizerBtns = CreateButtonSizer(wxOK);
    if ( sizerBtns )
    {
        sizerTop->Add(sizerBtns, wxSizerFlags().Expand().Border());
    }
#endif

    SetSizerAndFit(sizerTop);

    CentreOnParent();

#if !wxUSE_MODAL_ABOUT_DIALOG
    Connect(wxEVT_CLOSE_WINDOW,
            wxCloseEventHandler(wxGenericAboutDialog::OnCloseWindow));
    Connect(wxID_OK, wxEVT_BUTTON,
            wxCommandEventHandler(wxGenericAboutDialog::OnOK));
#endif // !wxUSE_MODAL_ABOUT_DIALOG

    return true;
}

void wxGenericAboutDialog::AddControl(wxWindow *win, const wxSizerFlags& flags)
{
    wxCHECK_RET( m_sizerText, wxT("can only be called after Create()") );
    wxASSERT_MSG( win, wxT("can't add NULL window to about dialog") );

    m_sizerText->Add(win, flags);
}

void wxGenericAboutDialog::AddControl(wxWindow *win)
{
    AddControl(win, wxSizerFlags().Border(wxDOWN).Centre());
}

void wxGenericAboutDialog::AddText(const wxString& text)
{
    if ( !text.empty() )
        AddControl(new wxStaticText(this, wxID_ANY, text));
}

#if wxUSE_COLLPANE
void wxGenericAboutDialog::AddCollapsiblePane(const wxString& title,
                                              const wxString& text)
{
    wxCollapsiblePane *pane = new wxCollapsiblePane(this, wxID_ANY, title);
    wxWindow * const paneContents = pane->GetPane();
    wxStaticText *txt = new wxStaticText(paneContents, wxID_ANY, text,
                                         wxDefaultPosition, wxDefaultSize,
                                         wxALIGN_CENTRE);

    // don't make the text unreasonably wide
    static const int maxWidth = wxGetDisplaySize().x/3;
    txt->Wrap(maxWidth);


    // we need a sizer to make this text expand to fill the entire pane area
    wxSizer * const sizerPane = new wxBoxSizer(wxHORIZONTAL);
    sizerPane->Add(txt, wxSizerFlags(1).Expand());
    paneContents->SetSizer(sizerPane);

    // NB: all the wxCollapsiblePanes must be added with a null proportion value
    m_sizerText->Add(pane, wxSizerFlags(0).Expand().Border(wxBOTTOM));
}
#endif

#if !wxUSE_MODAL_ABOUT_DIALOG

void wxGenericAboutDialog::OnCloseWindow(wxCloseEvent& event)
{
    // safeguards in case the window is still shown using ShowModal
    if ( !IsModal() )
        Destroy();

    event.Skip();
}

void wxGenericAboutDialog::OnOK(wxCommandEvent& event)
{
    // safeguards in case the window is still shown using ShowModal
    if ( !IsModal() )
    {
        // By default a modeless dialog would be just hidden, destroy this one
        // instead.
        Destroy();
    }
    else
        event.Skip();
}

#endif // !wxUSE_MODAL_ABOUT_DIALOG

// ----------------------------------------------------------------------------
// public functions
// ----------------------------------------------------------------------------

void wxGenericAboutBox(const wxAboutDialogInfo& info, wxWindow* parent)
{
#if wxUSE_MODAL_ABOUT_DIALOG
    wxGenericAboutDialog dlg(info, parent);
    dlg.ShowModal();
#else
    wxGenericAboutDialog* dlg = new wxGenericAboutDialog(info, parent);
    dlg->Show();
#endif
}

// currently wxAboutBox is implemented natively only under these platforms, for
// the others we provide a generic fallback here
#if !defined(__WXMSW__) && !defined(__WXMAC__) && \
        (!defined(__WXGTK20__) || defined(__WXUNIVERSAL__))

void wxAboutBox(const wxAboutDialogInfo& info, wxWindow* parent)
{
    wxGenericAboutBox(info, parent);
}

#endif // platforms without native about dialog


#endif // wxUSE_ABOUTDLG
