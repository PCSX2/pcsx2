//////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/filedlgg.cpp
// Purpose:     wxGenericFileDialog
// Author:      Robert Roebling
// Modified by:
// Created:     12/12/98
// Copyright:   (c) Robert Roebling
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_FILEDLG

// NOTE : it probably also supports MAC, untested
#if !defined(__UNIX__) && !defined(__DOS__) && !defined(__WIN32__) && !defined(__OS2__)
#error wxGenericFileDialog currently only supports Unix, win32 and DOS
#endif

#ifndef WX_PRECOMP
    #ifdef __WXMSW__
        #include "wx/msw/wrapwin.h"
    #endif
    #include "wx/hash.h"
    #include "wx/intl.h"
    #include "wx/settings.h"
    #include "wx/log.h"
    #include "wx/msgdlg.h"
    #include "wx/bmpbuttn.h"
    #include "wx/checkbox.h"
    #include "wx/choice.h"
    #include "wx/stattext.h"
    #include "wx/textctrl.h"
    #include "wx/sizer.h"
    #include "wx/filedlg.h"     // wxFD_OPEN, wxFD_SAVE...
#endif

#include "wx/longlong.h"
#include "wx/config.h"
#include "wx/imaglist.h"
#include "wx/artprov.h"
#include "wx/filefn.h"
#include "wx/filectrl.h"
#include "wx/generic/filedlgg.h"
#include "wx/debug.h"
#include "wx/modalhook.h"

#if wxUSE_TOOLTIPS
    #include "wx/tooltip.h"
#endif
#if wxUSE_CONFIG
    #include "wx/config.h"
#endif

#ifndef __WXWINCE__
    #include <sys/types.h>
    #include <sys/stat.h>
#endif

#ifdef __UNIX__
    #include <dirent.h>
    #include <pwd.h>
    #ifndef __VMS
    # include <grp.h>
    #endif
#endif

#ifdef __WINDOWS__
    #include "wx/msw/mslu.h"
#endif

#ifdef __WATCOMC__
    #include <direct.h>
#endif

#ifndef __WXWINCE__
#include <time.h>
#endif

#if defined(__UNIX__) || defined(__DOS__)
#include <unistd.h>
#endif

#if defined(__WXWINCE__)
#define IsTopMostDir(dir) (dir == wxT("\\") || dir == wxT("/"))
#elif (defined(__DOS__) || defined(__WINDOWS__) || defined (__OS2__))
#define IsTopMostDir(dir)   (dir.empty())
#else
#define IsTopMostDir(dir)   (dir == wxT("/"))
#endif

//-----------------------------------------------------------------------------
// wxGenericFileDialog
//-----------------------------------------------------------------------------

#define  ID_LIST_MODE     (wxID_FILEDLGG    )
#define  ID_REPORT_MODE   (wxID_FILEDLGG + 1)
#define  ID_UP_DIR        (wxID_FILEDLGG + 2)
#define  ID_HOME_DIR      (wxID_FILEDLGG + 3)
#define  ID_NEW_DIR       (wxID_FILEDLGG + 4)
#define  ID_FILE_CTRL     (wxID_FILEDLGG + 5)

IMPLEMENT_DYNAMIC_CLASS(wxGenericFileDialog, wxFileDialogBase)

BEGIN_EVENT_TABLE(wxGenericFileDialog,wxDialog)
    EVT_BUTTON(ID_LIST_MODE, wxGenericFileDialog::OnList)
    EVT_BUTTON(ID_REPORT_MODE, wxGenericFileDialog::OnReport)
    EVT_BUTTON(ID_UP_DIR, wxGenericFileDialog::OnUp)
    EVT_BUTTON(ID_HOME_DIR, wxGenericFileDialog::OnHome)
    EVT_BUTTON(ID_NEW_DIR, wxGenericFileDialog::OnNew)
    EVT_BUTTON(wxID_OK, wxGenericFileDialog::OnOk)
    EVT_FILECTRL_FILEACTIVATED(ID_FILE_CTRL, wxGenericFileDialog::OnFileActivated)

    EVT_UPDATE_UI(ID_UP_DIR, wxGenericFileDialog::OnUpdateButtonsUI)
#if defined(__DOS__) || defined(__WINDOWS__) || defined(__OS2__)
    EVT_UPDATE_UI(ID_NEW_DIR, wxGenericFileDialog::OnUpdateButtonsUI)
#endif // defined(__DOS__) || defined(__WINDOWS__) || defined(__OS2__)
END_EVENT_TABLE()

long wxGenericFileDialog::ms_lastViewStyle = wxLC_LIST;
bool wxGenericFileDialog::ms_lastShowHidden = false;

void wxGenericFileDialog::Init()
{
    m_bypassGenericImpl = false;

    m_filectrl   = NULL;
    m_upDirButton  = NULL;
    m_newDirButton = NULL;
}

wxGenericFileDialog::wxGenericFileDialog(wxWindow *parent,
                           const wxString& message,
                           const wxString& defaultDir,
                           const wxString& defaultFile,
                           const wxString& wildCard,
                           long  style,
                           const wxPoint& pos,
                           const wxSize& sz,
                           const wxString& name,
                           bool  bypassGenericImpl ) : wxFileDialogBase()
{
    Init();
    Create( parent, message, defaultDir, defaultFile, wildCard, style, pos, sz, name, bypassGenericImpl );
}

bool wxGenericFileDialog::Create( wxWindow *parent,
                                  const wxString& message,
                                  const wxString& defaultDir,
                                  const wxString& defaultFile,
                                  const wxString& wildCard,
                                  long  style,
                                  const wxPoint& pos,
                                  const wxSize& sz,
                                  const wxString& name,
                                  bool  bypassGenericImpl )
{
    m_bypassGenericImpl = bypassGenericImpl;

    parent = GetParentForModalDialog(parent, style);

    if (!wxFileDialogBase::Create(parent, message, defaultDir, defaultFile,
                                  wildCard, style, pos, sz, name))
    {
        return false;
    }

    if (m_bypassGenericImpl)
        return true;

    if (!wxDialog::Create( parent, wxID_ANY, message, pos, sz,
                           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | style, name
                           ))
    {
        return false;
    }

#if wxUSE_CONFIG
    if (wxConfig::Get(false))
    {
        wxConfig::Get()->Read(wxT("/wxWindows/wxFileDialog/ViewStyle"),
                              &ms_lastViewStyle);
        wxConfig::Get()->Read(wxT("/wxWindows/wxFileDialog/ShowHidden"),
                              &ms_lastShowHidden);
    }
#endif

    if ((m_dir.empty()) || (m_dir == wxT(".")))
    {
        m_dir = wxGetCwd();
        if (m_dir.empty())
            m_dir = wxFILE_SEP_PATH;
    }

    const size_t len = m_dir.length();
    if ((len > 1) && (wxEndsWithPathSeparator(m_dir)))
        m_dir.Remove( len-1, 1 );

    m_filterExtension = wxEmptyString;

    // layout

    const bool is_pda = (wxSystemSettings::GetScreenType() <= wxSYS_SCREEN_PDA);

    wxBoxSizer *mainsizer = new wxBoxSizer( wxVERTICAL );

    wxBoxSizer *buttonsizer = new wxBoxSizer( wxHORIZONTAL );
    AddBitmapButton( ID_LIST_MODE, wxART_LIST_VIEW,
                     _("View files as a list view"), buttonsizer );
    AddBitmapButton( ID_REPORT_MODE, wxART_REPORT_VIEW,
                     _("View files as a detailed view"), buttonsizer );
    buttonsizer->Add( 30, 5, 1 );
    m_upDirButton = AddBitmapButton( ID_UP_DIR, wxART_GO_DIR_UP,
                                     _("Go to parent directory"), buttonsizer );

#ifndef __DOS__ // VS: Home directory is meaningless in MS-DOS...
    AddBitmapButton( ID_HOME_DIR, wxART_GO_HOME,
                     _("Go to home directory"), buttonsizer );
    buttonsizer->Add( 20, 20 );
#endif //!__DOS__

    m_newDirButton = AddBitmapButton( ID_NEW_DIR, wxART_NEW_DIR,
                                      _("Create new directory"), buttonsizer );

    if (is_pda)
        mainsizer->Add( buttonsizer, wxSizerFlags().Expand() );
    else
        mainsizer->Add( buttonsizer, wxSizerFlags().Expand()
                                                   .Border( wxLEFT | wxRIGHT | wxTOP ) );

    long style2 = 0;
    if ( HasFdFlag(wxFD_MULTIPLE) )
        style2 |= wxFC_MULTIPLE;

    m_filectrl = new wxGenericFileCtrl( this, ID_FILE_CTRL,
                                        m_dir, defaultFile,
                                        wildCard,
                                        style2,
                                        wxDefaultPosition, wxSize(540,200)
                                        );

    m_filectrl->ShowHidden( ms_lastShowHidden );

    if (ms_lastViewStyle == wxLC_LIST)
    {
        m_filectrl->ChangeToListMode();
    }
    else if (ms_lastViewStyle == wxLC_REPORT)
    {
        m_filectrl->ChangeToReportMode();
    }

    mainsizer->Add(m_filectrl, wxSizerFlags(1).Expand().HorzBorder());

    wxSizer *bsizer = CreateButtonSizer(wxOK | wxCANCEL);
    if ( bsizer )
    {
        if (is_pda)
            mainsizer->Add(bsizer, wxSizerFlags().Expand().Border());
        else
            mainsizer->Add(bsizer, wxSizerFlags().Expand().DoubleBorder());
    }

    SetSizer( mainsizer );

    if (!is_pda)
    {
        mainsizer->SetSizeHints( this );

        Centre( wxBOTH );
    }

    return true;
}

wxGenericFileDialog::~wxGenericFileDialog()
{
    if (!m_bypassGenericImpl)
    {
#if wxUSE_CONFIG
        if (wxConfig::Get(false))
        {
            wxConfig::Get()->Write(wxT("/wxWindows/wxFileDialog/ViewStyle"),
                                   ms_lastViewStyle);
            wxConfig::Get()->Write(wxT("/wxWindows/wxFileDialog/ShowHidden"),
                                   ms_lastShowHidden);
        }
#endif
    }
}

wxBitmapButton* wxGenericFileDialog::AddBitmapButton( wxWindowID winId,
                                                      const wxArtID& artId,
                                                      const wxString& tip,
                                                      wxSizer *sizer)
{
    wxBitmapButton *but = new wxBitmapButton(this, winId,
                                         wxArtProvider::GetBitmap(artId, wxART_BUTTON));
    but->SetToolTip(tip);
    sizer->Add(but, wxSizerFlags().Border());
    return but;
}

int wxGenericFileDialog::ShowModal()
{
    WX_HOOK_MODAL_DIALOG();

    if (CreateExtraControl())
    {
        wxSizer *sizer = GetSizer();
        sizer->Insert(2 /* after m_filectrl */, m_extraControl,
                      wxSizerFlags().Expand().HorzBorder());
        sizer->Fit(this);
    }

    m_filectrl->SetDirectory(m_dir);

    return wxDialog::ShowModal();
}

bool wxGenericFileDialog::Show( bool show )
{
    // Called by ShowModal, so don't repeate the update
#ifndef __WIN32__
    if (show)
    {
        m_filectrl->SetDirectory(m_dir);
    }
#endif

    return wxDialog::Show( show );
}

void wxGenericFileDialog::OnOk( wxCommandEvent &WXUNUSED(event) )
{
    wxArrayString selectedFiles;
    m_filectrl->GetPaths(selectedFiles);

    if (selectedFiles.Count() == 0)
        return;

    const wxString& path = selectedFiles[0];

    if (selectedFiles.Count() == 1)
    {
        SetPath(path);
    }

    // check that the file [doesn't] exist if necessary
    if ( HasFdFlag(wxFD_SAVE) && HasFdFlag(wxFD_OVERWRITE_PROMPT) &&
                wxFileExists(path) )
    {
        if ( wxMessageBox
             (
                wxString::Format
                (
                    _("File '%s' already exists, do you really want to overwrite it?"),
                    path
                ),
                _("Confirm"),
                wxYES_NO
             ) != wxYES)
            return;
    }
    else if ( HasFdFlag(wxFD_OPEN) && HasFdFlag(wxFD_FILE_MUST_EXIST) &&
                    !wxFileExists(path) )
    {
        wxMessageBox(_("Please choose an existing file."), _("Error"),
                     wxOK | wxICON_ERROR );
        return;
    }

    EndModal(wxID_OK);
}

void wxGenericFileDialog::OnList( wxCommandEvent &WXUNUSED(event) )
{
    m_filectrl->ChangeToListMode();
    ms_lastViewStyle = wxLC_LIST;
    m_filectrl->GetFileList()->SetFocus();
}

void wxGenericFileDialog::OnReport( wxCommandEvent &WXUNUSED(event) )
{
    m_filectrl->ChangeToReportMode();
    ms_lastViewStyle = wxLC_REPORT;
    m_filectrl->GetFileList()->SetFocus();
}

void wxGenericFileDialog::OnUp( wxCommandEvent &WXUNUSED(event) )
{
    m_filectrl->GoToParentDir();
    m_filectrl->GetFileList()->SetFocus();
}

void wxGenericFileDialog::OnHome( wxCommandEvent &WXUNUSED(event) )
{
    m_filectrl->GoToHomeDir();
    m_filectrl->SetFocus();
}

void wxGenericFileDialog::OnNew( wxCommandEvent &WXUNUSED(event) )
{
    m_filectrl->GetFileList()->MakeDir();
}

void wxGenericFileDialog::OnFileActivated( wxFileCtrlEvent &WXUNUSED(event) )
{
    wxCommandEvent dummy;
    OnOk( dummy );
}

void wxGenericFileDialog::OnUpdateButtonsUI(wxUpdateUIEvent& event)
{
    // surprisingly, we can be called before m_filectrl is set in Create() as
    // wxFileCtrl ctor itself can generate idle events, so we need this test
    if ( m_filectrl )
        event.Enable( !IsTopMostDir(m_filectrl->GetShownDirectory()) );
}

#ifdef wxHAS_GENERIC_FILEDIALOG

IMPLEMENT_DYNAMIC_CLASS(wxFileDialog, wxGenericFileDialog)

#endif // wxHAS_GENERIC_FILEDIALOG

#endif // wxUSE_FILEDLG
