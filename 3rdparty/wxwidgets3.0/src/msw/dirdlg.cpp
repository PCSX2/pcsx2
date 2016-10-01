/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/dirdlg.cpp
// Purpose:     wxDirDialog
// Author:      Julian Smart
// Modified by:
// Created:     01/02/97
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_DIRDLG

#if wxUSE_OLE && !defined(__GNUWIN32_OLD__) && (!defined(__WXWINCE__) || \
    (defined(__HANDHELDPC__) && (_WIN32_WCE >= 500)))

#include "wx/dirdlg.h"
#include "wx/modalhook.h"

#ifndef WX_PRECOMP
    #include "wx/utils.h"
    #include "wx/dialog.h"
    #include "wx/log.h"
    #include "wx/app.h"     // for GetComCtl32Version()
#endif

#include "wx/msw/private.h"
#include "wx/msw/wrapshl.h"
#include "wx/msw/private/comptr.h"
#include "wx/dynlib.h"

#include <initguid.h>

// We can only use IFileDialog under desktop Windows and we need
// wxDynamicLibrary for it.
#if wxUSE_DYNLIB_CLASS && !defined(__WXWINCE__)
    #define wxUSE_IFILEDIALOG 1
#else
    #define wxUSE_IFILEDIALOG 0
#endif

#if wxUSE_IFILEDIALOG
// IFileDialog related declarations missing from some compilers headers.

// IShellItem
#ifndef __IShellItem_INTERFACE_DEFINED__

#ifndef SIGDN_FILESYSPATH
    #define SIGDN_FILESYSPATH 0x80058000
#endif

struct IShellItem : public IUnknown
{
    virtual HRESULT wxSTDCALL BindToHandler(IBindCtx*, REFGUID, REFIID, void**) = 0;
    virtual HRESULT wxSTDCALL GetParent(IShellItem**) = 0;
    virtual HRESULT wxSTDCALL GetDisplayName(DWORD, LPWSTR*) = 0;
    virtual HRESULT wxSTDCALL GetAttributes(ULONG, ULONG*) = 0;
    virtual HRESULT wxSTDCALL Compare(IShellItem*, DWORD, int*) = 0;
};

#endif // #ifndef __IShellItem_INTERFACE_DEFINED__

// Define this GUID in any case, even when __IShellItem_INTERFACE_DEFINED__ is
// defined in the headers we might still not have it in the actual uuid.lib,
// this happens with at least VC7 used with its original (i.e. not updated) SDK
// and there is no harm in defining the GUID unconditionally.
DEFINE_GUID(IID_IShellItem,
    0x43826D1E, 0xE718, 0x42EE, 0xBC, 0x55, 0xA1, 0xE2, 0x61, 0xC3, 0x7B, 0xFE);

struct IShellItemFilter;
struct IFileDialogEvents;

// IModalWindow
#ifndef __IModalWindow_INTERFACE_DEFINED__

struct IModalWindow : public IUnknown
{
    virtual HRESULT wxSTDCALL Show(HWND) = 0;
};

#endif // #ifndef __IModalWindow_INTERFACE_DEFINED__

// IFileDialog
#ifndef __IFileDialog_INTERFACE_DEFINED__

#ifndef FOS_PICKFOLDERS
    #define FOS_PICKFOLDERS 0x20
#endif

#ifndef FOS_FORCEFILESYSTEM
    #define FOS_FORCEFILESYSTEM 0x40
#endif

struct _COMDLG_FILTERSPEC;

struct IFileDialog : public IModalWindow
{
    virtual HRESULT wxSTDCALL SetFileTypes(UINT, const _COMDLG_FILTERSPEC*) = 0;
    virtual HRESULT wxSTDCALL SetFileTypeIndex(UINT) = 0;
    virtual HRESULT wxSTDCALL GetFileTypeIndex(UINT*) = 0;
    virtual HRESULT wxSTDCALL Advise(IFileDialogEvents*, DWORD*) = 0;
    virtual HRESULT wxSTDCALL Unadvise(DWORD) = 0;
    virtual HRESULT wxSTDCALL SetOptions(DWORD) = 0;
    virtual HRESULT wxSTDCALL GetOptions(DWORD*) = 0;
    virtual HRESULT wxSTDCALL SetDefaultFolder(IShellItem*) = 0;
    virtual HRESULT wxSTDCALL SetFolder(IShellItem*) = 0;
    virtual HRESULT wxSTDCALL GetFolder(IShellItem**) = 0;
    virtual HRESULT wxSTDCALL GetCurrentSelection(IShellItem**) = 0;
    virtual HRESULT wxSTDCALL SetFileName(LPCWSTR) = 0;
    virtual HRESULT wxSTDCALL GetFileName(LPWSTR*) = 0;
    virtual HRESULT wxSTDCALL SetTitle(LPCWSTR) = 0;
    virtual HRESULT wxSTDCALL SetOkButtonLabel(LPCWSTR) = 0;
    virtual HRESULT wxSTDCALL SetFileNameLabel(LPCWSTR) = 0;
    virtual HRESULT wxSTDCALL GetResult(IShellItem**) = 0;
    virtual HRESULT wxSTDCALL AddPlace(IShellItem*, DWORD) = 0;
    virtual HRESULT wxSTDCALL SetDefaultExtension(LPCWSTR) = 0;
    virtual HRESULT wxSTDCALL Close(HRESULT) = 0;
    virtual HRESULT wxSTDCALL SetClientGuid(REFGUID) = 0;
    virtual HRESULT wxSTDCALL ClearClientData() = 0;
    virtual HRESULT wxSTDCALL SetFilter(IShellItemFilter*) = 0;
};

DEFINE_GUID(CLSID_FileOpenDialog,
    0xDC1C5A9C, 0xE88A, 0x4dde, 0xA5, 0xA1, 0x60, 0xF8, 0x2A, 0x20, 0xAE, 0xF7);

DEFINE_GUID(IID_IFileDialog,
    0x42F85136, 0xDB7E, 0x439C, 0x85, 0xF1, 0xE4, 0x07, 0x5D, 0x13, 0x5F, 0xC8);

#endif // #ifndef __IFileDialog_INTERFACE_DEFINED__

#endif // wxUSE_IFILEDIALOG

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

#ifndef BIF_NEWDIALOGSTYLE
    #define BIF_NEWDIALOGSTYLE 0x0040
#endif

#ifndef BIF_NONEWFOLDERBUTTON
    #define BIF_NONEWFOLDERBUTTON  0x0200
#endif

#ifndef BIF_EDITBOX
    #define BIF_EDITBOX 16
#endif

// ----------------------------------------------------------------------------
// wxWidgets macros
// ----------------------------------------------------------------------------

IMPLEMENT_CLASS(wxDirDialog, wxDialog)

// ----------------------------------------------------------------------------
// private functions prototypes
// ----------------------------------------------------------------------------

// the callback proc for the dir dlg
static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lp,
                                       LPARAM pData);


// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxDirDialog
// ----------------------------------------------------------------------------

wxDirDialog::wxDirDialog(wxWindow *parent,
                         const wxString& message,
                         const wxString& defaultPath,
                         long style,
                         const wxPoint& WXUNUSED(pos),
                         const wxSize& WXUNUSED(size),
                         const wxString& WXUNUSED(name))
{
    m_message = message;
    m_parent = parent;

    SetWindowStyle(style);
    SetPath(defaultPath);
}

void wxDirDialog::SetPath(const wxString& path)
{
    m_path = path;

    // SHBrowseForFolder doesn't like '/'s nor the trailing backslashes
    m_path.Replace(wxT("/"), wxT("\\"));

    while ( !m_path.empty() && (*(m_path.end() - 1) == wxT('\\')) )
    {
        m_path.erase(m_path.length() - 1);
    }

    // but the root drive should have a trailing slash (again, this is just
    // the way the native dialog works)
    if ( !m_path.empty() && (*(m_path.end() - 1) == wxT(':')) )
    {
        m_path += wxT('\\');
    }
}

int wxDirDialog::ShowModal()
{
    WX_HOOK_MODAL_DIALOG();

    wxWindow* const parent = GetParentForModalDialog();
    WXHWND hWndParent = parent ? GetHwndOf(parent) : NULL;

    // Use IFileDialog under new enough Windows, it's more user-friendly.
    int rc;
#if wxUSE_IFILEDIALOG
    // While the new dialog is available under Vista, it may return a wrong
    // path there (see http://support.microsoft.com/kb/969885/en-us), so we
    // don't use it there by default. We could improve the version test to
    // allow its use if the comdlg32.dll version is greater than 6.0.6002.22125
    // as this means that the hotfix correcting this bug is installed.
    if ( wxGetWinVersion() > wxWinVersion_Vista )
    {
        rc = ShowIFileDialog(hWndParent);
    }
    else
    {
        rc = wxID_NONE;
    }

    if ( rc == wxID_NONE )
#endif // wxUSE_IFILEDIALOG
    {
        rc = ShowSHBrowseForFolder(hWndParent);
    }

    // change current working directory if asked so
    if ( rc == wxID_OK && HasFlag(wxDD_CHANGE_DIR) )
        wxSetWorkingDirectory(m_path);

    return rc;
}

int wxDirDialog::ShowSHBrowseForFolder(WXHWND owner)
{
    BROWSEINFO bi;
    bi.hwndOwner      = owner;
    bi.pidlRoot       = NULL;
    bi.pszDisplayName = NULL;
    // Please don't change this without checking it compiles
    // with eVC++ first.
#if defined(__POCKETPC__) || defined(__SMARTPHONE__)
    bi.lpszTitle      = m_message.mb_str();
#else
    bi.lpszTitle      = m_message.c_str();
#endif
    bi.ulFlags        = BIF_RETURNONLYFSDIRS | BIF_STATUSTEXT;
    bi.lpfn           = BrowseCallbackProc;
    bi.lParam         = wxMSW_CONV_LPARAM(m_path); // param for the callback

    static const int verComCtl32 = wxApp::GetComCtl32Version();

    // we always add the edit box (it doesn't hurt anybody, does it?) if it is
    // supported by the system
    if ( verComCtl32 >= 471 )
    {
        bi.ulFlags |= BIF_EDITBOX;
    }

    // to have the "New Folder" button we must use the "new" dialog style which
    // is also the only way to have a resizable dialog
    //
    // "new" style is only available in the version 5.0+ of comctl32.dll
    const bool needNewDir = !HasFlag(wxDD_DIR_MUST_EXIST);
    if ( (needNewDir || HasFlag(wxRESIZE_BORDER)) && (verComCtl32 >= 500) )
    {
        if (needNewDir)
        {
            bi.ulFlags |= BIF_NEWDIALOGSTYLE;
        }
        else
        {
            // Versions < 600 doesn't support BIF_NONEWFOLDERBUTTON
            // The only way to get rid of the Make New Folder button is use
            // the old dialog style which doesn't have the button thus we
            // simply don't set the New Dialog Style for such comctl versions.
            if (verComCtl32 >= 600)
            {
                bi.ulFlags |= BIF_NEWDIALOGSTYLE;
                bi.ulFlags |= BIF_NONEWFOLDERBUTTON;
            }
        }
    }

    // do show the dialog
    wxItemIdList pidl(SHBrowseForFolder(&bi));

    wxItemIdList::Free((LPITEMIDLIST)bi.pidlRoot);

    if ( !pidl )
    {
        // Cancel button pressed
        return wxID_CANCEL;
    }

    m_path = pidl.GetPath();

    return m_path.empty() ? wxID_CANCEL : wxID_OK;
}

// Function for obtaining folder name on Vista and newer.
//
// Returns wxID_OK on success, wxID_CANCEL if cancelled by user or wxID_NONE if
// an error occurred and we should fall back onto the old dialog.
#if wxUSE_IFILEDIALOG

int wxDirDialog::ShowIFileDialog(WXHWND owner)
{
    HRESULT hr;
    wxCOMPtr<IFileDialog> fileDialog;

    hr = ::CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                            wxIID_PPV_ARGS(IFileDialog, &fileDialog));
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("CoCreateInstance(CLSID_FileOpenDialog)"), hr);
        return wxID_NONE;
    }

    // allow user to select only a file system folder
    hr = fileDialog->SetOptions(FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    if ( FAILED(hr) )
    {
        wxLogApiError(wxS("IFileDialog::SetOptions"), hr);
        return wxID_NONE;
    }

    hr = fileDialog->SetTitle(m_message.wc_str());
    if ( FAILED(hr) )
    {
        // This error is not serious, let's just log it and continue even
        // without the title set.
        wxLogApiError(wxS("IFileDialog::SetTitle"), hr);
    }

    // set the initial path
    if ( !m_path.empty() )
    {
        // We need to link SHCreateItemFromParsingName() dynamically as it's
        // not available on pre-Vista systems.
        typedef HRESULT
        (WINAPI *SHCreateItemFromParsingName_t)(PCWSTR,
                                                IBindCtx*,
                                                REFIID,
                                                void**);

        SHCreateItemFromParsingName_t pfnSHCreateItemFromParsingName = NULL;
        wxDynamicLibrary dllShell32;
        if ( dllShell32.Load(wxS("shell32.dll"), wxDL_VERBATIM | wxDL_QUIET) )
        {
            wxDL_INIT_FUNC(pfn, SHCreateItemFromParsingName, dllShell32);
        }

        if ( !pfnSHCreateItemFromParsingName )
        {
            wxLogLastError(wxS("SHCreateItemFromParsingName() not found"));
            return wxID_NONE;
        }

        wxCOMPtr<IShellItem> folder;
        hr = pfnSHCreateItemFromParsingName(m_path.wc_str(),
                                              NULL,
                                              wxIID_PPV_ARGS(IShellItem,
                                                             &folder));

        // Failing to parse the folder name is not really an error, we'll just
        // ignore the initial directory in this case, but we should still show
        // the dialog.
        if ( FAILED(hr) )
        {
            if ( hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) )
            {
                wxLogApiError(wxS("SHCreateItemFromParsingName"), hr);
                return wxID_NONE;
            }
        }
        else // The folder was parsed correctly.
        {
            hr = fileDialog->SetFolder(folder);
            if ( FAILED(hr) )
            {
                wxLogApiError(wxS("IFileDialog::SetFolder"), hr);
                return wxID_NONE;
            }
        }
    }


    wxString path;

    hr = fileDialog->Show(owner);
    if ( SUCCEEDED(hr) )
    {
        wxCOMPtr<IShellItem> folder;

        hr = fileDialog->GetResult(&folder);
        if ( SUCCEEDED(hr) )
        {
            LPOLESTR pathOLE = NULL;

            hr = folder->GetDisplayName(SIGDN_FILESYSPATH, &pathOLE);
            if ( SUCCEEDED(hr) )
            {
                path = pathOLE;
                CoTaskMemFree(pathOLE);
            }
            else
            {
                wxLogApiError(wxS("IShellItem::GetDisplayName"), hr);
            }
        }
        else
        {
            wxLogApiError(wxS("IFileDialog::GetResult"), hr);
        }
    }
    else if ( hr == HRESULT_FROM_WIN32(ERROR_CANCELLED) )
    {
        return wxID_CANCEL; // the user cancelled the dialog
    }
    else
    {
        wxLogApiError(wxS("IFileDialog::Show"), hr);
    }

    if ( path.empty() )
    {
        // the user didn't cancel the dialog and yet the path is empty
        // it means there was an error, already logged by wxLogApiError()
        // now report the error to the user and return
        wxLogSysError(_("Couldn't obtain folder name"), hr);
        return wxID_CANCEL;
    }

    m_path = path;
    return wxID_OK;
}

#endif // wxUSE_IFILEDIALOG

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

static int CALLBACK
BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lp, LPARAM pData)
{
    switch(uMsg)
    {
#ifdef BFFM_SETSELECTION
        case BFFM_INITIALIZED:
            // sent immediately after initialisation and so we may set the
            // initial selection here
            //
            // wParam = TRUE => lParam is a string and not a PIDL
            ::SendMessage(hwnd, BFFM_SETSELECTION, TRUE, pData);
            break;
#endif // BFFM_SETSELECTION


        case BFFM_SELCHANGED:
            // note that this doesn't work with the new style UI (MSDN doesn't
            // say anything about it, but the comments in shlobj.h do!) but we
            // still execute this code in case it starts working again with the
            // "new new UI" (or would it be "NewUIEx" according to tradition?)
            {
                // Set the status window to the currently selected path.
                wxString strDir;
                if ( SHGetPathFromIDList((LPITEMIDLIST)lp,
                                         wxStringBuffer(strDir, MAX_PATH)) )
                {
                    // NB: this shouldn't be necessary with the new style box
                    //     (which is resizable), but as for now it doesn't work
                    //     anyhow (see the comment above) no harm in doing it

                    // need to truncate or it displays incorrectly
                    static const size_t maxChars = 37;
                    if ( strDir.length() > maxChars )
                    {
                        strDir = strDir.Right(maxChars);
                        strDir = wxString(wxT("...")) + strDir;
                    }

                    SendMessage(hwnd, BFFM_SETSTATUSTEXT,
                                0, wxMSW_CONV_LPARAM(strDir));
                }
            }
            break;

        //case BFFM_VALIDATEFAILED: -- might be used to provide custom message
        //                             if the user types in invalid dir name
    }

    return 0;
}

#endif // compiler/platform on which the code here compiles

#endif // wxUSE_DIRDLG
