///////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/preferencesg.cpp
// Purpose:     Implementation of wxPreferencesEditor.
// Author:      Vaclav Slavik
// Created:     2013-02-19
// Copyright:   (c) 2013 Vaclav Slavik <vslavik@fastmail.fm>
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

#if wxUSE_PREFERENCES_EDITOR

#include "wx/private/preferences.h"

#ifndef wxHAS_PREF_EDITOR_NATIVE

#include "wx/app.h"
#include "wx/dialog.h"
#include "wx/notebook.h"
#include "wx/sizer.h"
#include "wx/sharedptr.h"
#include "wx/scopedptr.h"
#include "wx/scopeguard.h"
#include "wx/vector.h"

namespace
{

class wxGenericPrefsDialog : public wxDialog
{
public:
    wxGenericPrefsDialog(wxWindow *parent, const wxString& title)
        : wxDialog(parent, wxID_ANY, title,
                   wxDefaultPosition, wxDefaultSize,
                   wxDEFAULT_FRAME_STYLE & ~(wxRESIZE_BORDER | wxMAXIMIZE_BOX | wxMINIMIZE_BOX))
    {
        SetExtraStyle(wxWS_EX_VALIDATE_RECURSIVELY);

        wxSizer *sizer = new wxBoxSizer(wxVERTICAL);

        m_notebook = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_MULTILINE);
        sizer->Add(m_notebook, wxSizerFlags(1).Expand().DoubleBorder());

#ifdef __WXGTK__
        SetEscapeId(wxID_CLOSE);
        sizer->Add(CreateButtonSizer(wxCLOSE), wxSizerFlags().Expand().DoubleBorder(wxBOTTOM));
#else
        sizer->Add(CreateButtonSizer(wxOK | wxCANCEL),
                   wxSizerFlags().Expand().DoubleBorder(wxLEFT|wxRIGHT|wxBOTTOM));
#endif
        SetSizer(sizer);
    }

    void AddPage(wxPreferencesPage *page)
    {
        wxWindow *win = page->CreateWindow(m_notebook);
        m_notebook->AddPage(win, page->GetName());
    }

    int GetSelectedPage() const
    {
        return m_notebook->GetSelection();
    }

    void SelectPage(int page)
    {
        m_notebook->ChangeSelection(page);
    }

    bool ShouldPreventAppExit() const
    {
        return false;
    }

private:
    wxNotebook *m_notebook;
};


class wxGenericPreferencesEditorImplBase : public wxPreferencesEditorImpl
{
public:
    void SetTitle(const wxString& title)
    {
        m_title = title;
    }

    virtual void AddPage(wxPreferencesPage* page)
    {
        m_pages.push_back(wxSharedPtr<wxPreferencesPage>(page));
    }

protected:
    wxGenericPrefsDialog *CreateDialog(wxWindow *parent)
    {
        if ( m_title.empty() )
        {
            // Use the default title, which should include the application name
            // under both MSW and GTK (and OSX uses its own native
            // implementation anyhow).
            m_title.Printf(_("%s Preferences"), wxTheApp->GetAppDisplayName());
        }

        wxGenericPrefsDialog *dlg = new wxGenericPrefsDialog(parent, m_title);

        // TODO: Don't create all pages immediately like this, do it on demand
        //       when a page is selected in the notebook (as is done on OS X).
        //
        //       Currently, creating all pages is necessary so that the notebook
        //       can determine its best size. We'll need to extend
        //       wxPreferencesPage with a GetBestSize() virtual method to make
        //       it possible to defer the creation.
        for ( Pages::const_iterator i = m_pages.begin();
              i != m_pages.end();
              ++i )
        {
            dlg->AddPage(i->get());
        }

        dlg->Fit();

        return dlg;
    }

    typedef wxVector< wxSharedPtr<wxPreferencesPage> > Pages;
    Pages m_pages;

private:
    wxString m_title;
};


#ifdef wxHAS_PREF_EDITOR_MODELESS

class wxModelessPreferencesEditorImpl : public wxGenericPreferencesEditorImplBase
{
public:
    virtual ~wxModelessPreferencesEditorImpl()
    {
        // m_win may already be destroyed if this destructor is called from
        // wxApp's destructor. In that case, all windows -- including this
        // one -- would already be destroyed by now.
        if ( m_win )
            m_win->Destroy();
    }

    virtual void Show(wxWindow* parent)
    {
        if ( !m_win )
        {
            wxWindow *win = CreateDialog(parent);
            win->Show();
            m_win = win;
        }
        else
        {
            // Ideally, we'd reparent the dialog under 'parent', but it's
            // probably not worth the hassle. We know the old parent is still
            // valid, because otherwise Dismiss() would have been called and
            // m_win cleared.
            m_win->Raise();
        }
    }

    virtual void Dismiss()
    {
        if ( m_win )
        {
            m_win->Close(/*force=*/true);
            m_win = NULL;
        }
    }

private:
    wxWeakRef<wxWindow> m_win;
};

inline
wxGenericPreferencesEditorImplBase* NewGenericImpl()
{
    return new wxModelessPreferencesEditorImpl;
}

#else // !wxHAS_PREF_EDITOR_MODELESS

class wxModalPreferencesEditorImpl : public wxGenericPreferencesEditorImplBase
{
public:
    wxModalPreferencesEditorImpl()
    {
        m_dlg = NULL;
        m_currentPage = -1;
    }

    virtual void Show(wxWindow* parent)
    {
        wxScopedPtr<wxGenericPrefsDialog> dlg(CreateDialog(parent));

        // Store it for Dismiss() but ensure that the pointer is reset to NULL
        // when the dialog is destroyed on leaving this function.
        m_dlg = dlg.get();
        wxON_BLOCK_EXIT_NULL(m_dlg);

        // Restore the previously selected page, if any.
        if ( m_currentPage != -1 )
            dlg->SelectPage(m_currentPage);

        // Don't remember the last selected page if the dialog was cancelled.
        if ( dlg->ShowModal() != wxID_CANCEL )
            m_currentPage = dlg->GetSelectedPage();
    }

    virtual void Dismiss()
    {
        if ( m_dlg )
        {
            m_dlg->EndModal(wxID_CANCEL);
            m_dlg = NULL;
        }
    }

private:
    wxGenericPrefsDialog* m_dlg;
    int m_currentPage;

    wxDECLARE_NO_COPY_CLASS(wxModalPreferencesEditorImpl);
};

inline
wxGenericPreferencesEditorImplBase* NewGenericImpl()
{
    return new wxModalPreferencesEditorImpl;
}

#endif // !wxHAS_PREF_EDITOR_MODELESS

} // anonymous namespace

/*static*/
wxPreferencesEditorImpl* wxPreferencesEditorImpl::Create(const wxString& title)
{
    wxGenericPreferencesEditorImplBase* const impl = NewGenericImpl();

    impl->SetTitle(title);

    return impl;
}

#endif // !wxHAS_PREF_EDITOR_NATIVE

#endif // wxUSE_PREFERENCES_EDITOR
