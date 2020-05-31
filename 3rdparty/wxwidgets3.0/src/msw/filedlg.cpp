/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/filedlg.cpp
// Purpose:     wxFileDialog
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

#if wxUSE_FILEDLG && !(defined(__SMARTPHONE__) && defined(__WXWINCE__))

#include "wx/filedlg.h"

#ifndef WX_PRECOMP
    #include "wx/msw/wrapcdlg.h"
    #include "wx/msw/missing.h"
    #include "wx/utils.h"
    #include "wx/msgdlg.h"
    #include "wx/filefn.h"
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/app.h"
    #include "wx/math.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "wx/dynlib.h"
#include "wx/filename.h"
#include "wx/scopeguard.h"
#include "wx/tokenzr.h"
#include "wx/modalhook.h"

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

#ifdef __WIN32__
# define wxMAXPATH   65534
#else
# define wxMAXPATH   1024
#endif

# define wxMAXFILE   1024

# define wxMAXEXT    5

// ----------------------------------------------------------------------------
// globals
// ----------------------------------------------------------------------------

// standard dialog size for the old Windows systems where the dialog wasn't
// resizable
static wxRect gs_rectDialog(0, 0, 428, 266);

// ============================================================================
// implementation
// ============================================================================

IMPLEMENT_CLASS(wxFileDialog, wxFileDialogBase)

// ----------------------------------------------------------------------------

namespace
{

#if wxUSE_DYNLIB_CLASS

typedef BOOL (WINAPI *GetProcessUserModeExceptionPolicy_t)(LPDWORD);
typedef BOOL (WINAPI *SetProcessUserModeExceptionPolicy_t)(DWORD);

GetProcessUserModeExceptionPolicy_t gs_pfnGetProcessUserModeExceptionPolicy
    = (GetProcessUserModeExceptionPolicy_t) -1;

SetProcessUserModeExceptionPolicy_t gs_pfnSetProcessUserModeExceptionPolicy
    = (SetProcessUserModeExceptionPolicy_t) -1;

DWORD gs_oldExceptionPolicyFlags = 0;

bool gs_changedPolicy = false;

#endif // #if wxUSE_DYNLIB_CLASS

/*
Since Windows 7 by default (callback) exceptions aren't swallowed anymore
with native x64 applications. Exceptions can occur in a file dialog when
using the hook procedure in combination with third-party utilities.
Since Windows 7 SP1 the swallowing of exceptions can be enabled again
by using SetProcessUserModeExceptionPolicy.
*/
void ChangeExceptionPolicy()
{
#if wxUSE_DYNLIB_CLASS
    gs_changedPolicy = false;

    wxLoadedDLL dllKernel32(wxT("kernel32.dll"));

    if ( gs_pfnGetProcessUserModeExceptionPolicy
        == (GetProcessUserModeExceptionPolicy_t) -1)
    {
        wxDL_INIT_FUNC(gs_pfn, GetProcessUserModeExceptionPolicy, dllKernel32);
        wxDL_INIT_FUNC(gs_pfn, SetProcessUserModeExceptionPolicy, dllKernel32);
    }

    if ( !gs_pfnGetProcessUserModeExceptionPolicy
        || !gs_pfnSetProcessUserModeExceptionPolicy
        || !gs_pfnGetProcessUserModeExceptionPolicy(&gs_oldExceptionPolicyFlags) )
    {
        return;
    }

    if ( gs_pfnSetProcessUserModeExceptionPolicy(gs_oldExceptionPolicyFlags
        | 0x1 /* PROCESS_CALLBACK_FILTER_ENABLED */ ) )
    {
        gs_changedPolicy = true;
    }

#endif // wxUSE_DYNLIB_CLASS
}

void RestoreExceptionPolicy()
{
#if wxUSE_DYNLIB_CLASS
    if (gs_changedPolicy)
    {
        gs_changedPolicy = false;
        (void) gs_pfnSetProcessUserModeExceptionPolicy(gs_oldExceptionPolicyFlags);
    }
#endif // wxUSE_DYNLIB_CLASS
}

} // unnamed namespace

// ----------------------------------------------------------------------------
// hook function for moving the dialog
// ----------------------------------------------------------------------------

UINT_PTR APIENTRY
wxFileDialogHookFunction(HWND      hDlg,
                         UINT      iMsg,
                         WPARAM    WXUNUSED(wParam),
                         LPARAM    lParam)
{
    switch ( iMsg )
    {
#ifndef __WXWINCE__
        case WM_INITDIALOG:
            {
                OPENFILENAME* ofn = reinterpret_cast<OPENFILENAME *>(lParam);
                reinterpret_cast<wxFileDialog *>(ofn->lCustData)
                    ->MSWOnInitDialogHook((WXHWND)hDlg);
            }
            break;
#endif // __WXWINCE__

        case WM_NOTIFY:
            {
                NMHDR* const pNM = reinterpret_cast<NMHDR*>(lParam);
                if ( pNM->code > CDN_LAST && pNM->code <= CDN_FIRST )
                {
                    OFNOTIFY* const
                        pNotifyCode = reinterpret_cast<OFNOTIFY *>(lParam);
                    wxFileDialog* const
                        dialog = reinterpret_cast<wxFileDialog *>(
                                        pNotifyCode->lpOFN->lCustData
                                    );

                    switch ( pNotifyCode->hdr.code )
                    {
                        case CDN_INITDONE:
                            dialog->MSWOnInitDone((WXHWND)hDlg);
                            break;

                        case CDN_SELCHANGE:
                            dialog->MSWOnSelChange((WXHWND)hDlg);
                            break;
                    }
                }
            }
            break;

        case WM_DESTROY:
            // reuse the position used for the dialog the next time by default
            //
            // NB: at least under Windows 2003 this is useless as after the
            //     first time it's shown the dialog always remembers its size
            //     and position itself and ignores any later SetWindowPos calls
            wxCopyRECTToRect(wxGetWindowRect(::GetParent(hDlg)), gs_rectDialog);
            break;
    }

    // do the default processing
    return 0;
}

// ----------------------------------------------------------------------------
// wxFileDialog
// ----------------------------------------------------------------------------

wxFileDialog::wxFileDialog(wxWindow *parent,
                           const wxString& message,
                           const wxString& defaultDir,
                           const wxString& defaultFileName,
                           const wxString& wildCard,
                           long style,
                           const wxPoint& pos,
                           const wxSize& sz,
                           const wxString& name)
            : wxFileDialogBase(parent, message, defaultDir, defaultFileName,
                               wildCard, style, pos, sz, name)

{
    // NB: all style checks are done by wxFileDialogBase::Create

    m_bMovedWindow = false;
    m_centreDir = 0;

    // Must set to zero, otherwise the wx routines won't size the window
    // the second time you call the file dialog, because it thinks it is
    // already at the requested size.. (when centering)
    gs_rectDialog.x =
    gs_rectDialog.y = 0;
}

void wxFileDialog::GetPaths(wxArrayString& paths) const
{
    paths.Empty();

    wxString dir(m_dir);
    if ( m_dir.empty() || m_dir.Last() != wxT('\\') )
        dir += wxT('\\');

    size_t count = m_fileNames.GetCount();
    for ( size_t n = 0; n < count; n++ )
    {
        if (wxFileName(m_fileNames[n]).IsAbsolute())
            paths.Add(m_fileNames[n]);
        else
            paths.Add(dir + m_fileNames[n]);
    }
}

void wxFileDialog::GetFilenames(wxArrayString& files) const
{
    files = m_fileNames;
}

void wxFileDialog::DoGetPosition(int *x, int *y) const
{
    if ( x )
        *x = gs_rectDialog.x;
    if ( y )
        *y = gs_rectDialog.y;
}

void wxFileDialog::DoGetSize(int *width, int *height) const
{
    if ( width )
        *width = gs_rectDialog.width;
    if ( height )
        *height = gs_rectDialog.height;
}

void wxFileDialog::DoMoveWindow(int x, int y, int WXUNUSED(w), int WXUNUSED(h))
{
    gs_rectDialog.x = x;
    gs_rectDialog.y = y;

    // our HWND is only set when we're called from MSWOnInitDone(), test if
    // this is the case
    HWND hwnd = GetHwnd();
    if ( hwnd )
    {
        // size of the dialog can't be changed because the controls are not
        // laid out correctly then
       ::SetWindowPos(hwnd, HWND_TOP, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
    }
    else // just remember that we were requested to move the window
    {
        m_bMovedWindow = true;

        // if Centre() had been called before, it shouldn't be taken into
        // account now
        m_centreDir = 0;
    }
}

void wxFileDialog::DoCentre(int dir)
{
    m_centreDir = dir;
    m_bMovedWindow = true;

    // it's unnecessary to do anything else at this stage as we'll redo it in
    // MSWOnInitDone() anyhow
}

void wxFileDialog::MSWOnInitDone(WXHWND hDlg)
{
    // note the dialog is the parent window: hDlg is a child of it when
    // OFN_EXPLORER is used
    HWND hFileDlg = ::GetParent((HWND)hDlg);

    // set HWND so that our DoMoveWindow() works correctly
    SetHWND((WXHWND)hFileDlg);

    if ( m_centreDir )
    {
        // now we have the real dialog size, remember it
        RECT rect;
        GetWindowRect(hFileDlg, &rect);
        gs_rectDialog = wxRectFromRECT(rect);

        // and position the window correctly: notice that we must use the base
        // class version as our own doesn't do anything except setting flags
        wxFileDialogBase::DoCentre(m_centreDir);
    }
    else // need to just move it to the correct place
    {
        SetPosition(gs_rectDialog.GetPosition());
    }

    // Call selection change handler so that update handler will be
    // called once with no selection.
    MSWOnSelChange(hDlg);

    // we shouldn't destroy this HWND
    SetHWND(NULL);
}

void wxFileDialog::MSWOnSelChange(WXHWND hDlg)
{
    TCHAR buf[MAX_PATH];
    LRESULT len = SendMessage(::GetParent(hDlg), CDM_GETFILEPATH,
                              MAX_PATH, reinterpret_cast<LPARAM>(buf));

    if ( len > 0 )
        m_currentlySelectedFilename = buf;
    else
        m_currentlySelectedFilename.clear();

    if ( m_extraControl )
        m_extraControl->UpdateWindowUI(wxUPDATE_UI_RECURSE);
}

// helper used below in ShowCommFileDialog(): style is used to determine
// whether to show the "Save file" dialog (if it contains wxFD_SAVE bit) or
// "Open file" one; returns true on success or false on failure in which case
// err is filled with the CDERR_XXX constant
static bool DoShowCommFileDialog(OPENFILENAME *of, long style, DWORD *err)
{
    if ( style & wxFD_SAVE ? GetSaveFileName(of) : GetOpenFileName(of) )
        return true;

    if ( err )
    {
#ifdef __WXWINCE__
        // according to MSDN, CommDlgExtendedError() should work under CE as
        // well but apparently in practice it doesn't (anybody has more
        // details?)
        *err = GetLastError();
#else
        *err = CommDlgExtendedError();
#endif
    }

    return false;
}

// We want to use OPENFILENAME struct version 5 (Windows 2000/XP) but we don't
// know if the OPENFILENAME declared in the currently used headers is a V5 or
// V4 (smaller) one so we try to manually extend the struct in case it is the
// old one.
//
// We don't do this on Windows CE nor under Win64, however, as there are no
// compilers with old headers for these architectures
#if defined(__WXWINCE__) || defined(__WIN64__)
    typedef OPENFILENAME wxOPENFILENAME;

    static const DWORD gs_ofStructSize = sizeof(OPENFILENAME);
#else // !__WXWINCE__ || __WIN64__
    #define wxTRY_SMALLER_OPENFILENAME

    struct wxOPENFILENAME : public OPENFILENAME
    {
        // fields added in Windows 2000/XP comdlg32.dll version
        void *pVoid;
        DWORD dw1;
        DWORD dw2;
    };

    // hardcoded sizeof(OPENFILENAME) in the Platform SDK: we have to do it
    // because sizeof(OPENFILENAME) in the headers we use when compiling the
    // library could be less if _WIN32_WINNT is not >= 0x500
    static const DWORD wxOPENFILENAME_V5_SIZE = 88;

    // this is hardcoded sizeof(OPENFILENAME_NT4) from Platform SDK
    static const DWORD wxOPENFILENAME_V4_SIZE = 76;

    // always try the new one first
    static DWORD gs_ofStructSize = wxOPENFILENAME_V5_SIZE;
#endif // __WXWINCE__ || __WIN64__/!...

static bool ShowCommFileDialog(OPENFILENAME *of, long style)
{
    DWORD errCode;
    bool success = DoShowCommFileDialog(of, style, &errCode);

#ifdef wxTRY_SMALLER_OPENFILENAME
    // the system might be too old to support the new version file dialog
    // boxes, try with the old size
    if ( !success && errCode == CDERR_STRUCTSIZE &&
            of->lStructSize != wxOPENFILENAME_V4_SIZE )
    {
        of->lStructSize = wxOPENFILENAME_V4_SIZE;

        success = DoShowCommFileDialog(of, style, &errCode);

        if ( success || !errCode )
        {
            // use this struct size for subsequent dialogs
            gs_ofStructSize = of->lStructSize;
        }
    }
#endif // wxTRY_SMALLER_OPENFILENAME

    if ( !success &&
            // FNERR_INVALIDFILENAME is not defined under CE (besides we don't
            // use CommDlgExtendedError() there anyhow)
#ifndef __WXWINCE__
            errCode == FNERR_INVALIDFILENAME &&
#endif // !__WXWINCE__
                of->lpstrFile[0] )
    {
        // this can happen if the default file name is invalid, try without it
        // now
        of->lpstrFile[0] = wxT('\0');
        success = DoShowCommFileDialog(of, style, &errCode);
    }

    if ( !success )
    {
        // common dialog failed - why?
        if ( errCode != 0 )
        {
            wxLogError(_("File dialog failed with error code %0lx."), errCode);
        }
        //else: it was just cancelled

        return false;
    }

    return true;
}

#ifndef __WXWINCE__
void wxFileDialog::MSWOnInitDialogHook(WXHWND hwnd)
{
   SetHWND(hwnd);

   CreateExtraControl();

   SetHWND(NULL);
}
#endif // __WXWINCE__

int wxFileDialog::ShowModal()
{
    WX_HOOK_MODAL_DIALOG();

    HWND hWnd = 0;
    if (m_parent) hWnd = (HWND) m_parent->GetHWND();
    if (!hWnd && wxTheApp->GetTopWindow())
        hWnd = (HWND) wxTheApp->GetTopWindow()->GetHWND();

    static wxChar fileNameBuffer [ wxMAXPATH ];           // the file-name
    wxChar        titleBuffer    [ wxMAXFILE+1+wxMAXEXT ];  // the file-name, without path

    *fileNameBuffer = wxT('\0');
    *titleBuffer    = wxT('\0');

    long msw_flags = OFN_HIDEREADONLY;

    if ( HasFdFlag(wxFD_FILE_MUST_EXIST) )
        msw_flags |= OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    /*
        If the window has been moved the programmer is probably
        trying to center or position it.  Thus we set the callback
        or hook function so that we can actually adjust the position.
        Without moving or centering the dlg, it will just stay
        in the upper left of the frame, it does not center
        automatically.
    */
    if (m_bMovedWindow || HasExtraControlCreator()) // we need these flags.
    {
        ChangeExceptionPolicy();
        msw_flags |= OFN_EXPLORER|OFN_ENABLEHOOK;
#ifndef __WXWINCE__
        msw_flags |= OFN_ENABLESIZING;
#endif
    }

    wxON_BLOCK_EXIT0(RestoreExceptionPolicy);

    if ( HasFdFlag(wxFD_MULTIPLE) )
    {
        // OFN_EXPLORER must always be specified with OFN_ALLOWMULTISELECT
        msw_flags |= OFN_EXPLORER | OFN_ALLOWMULTISELECT;
    }

    // if wxFD_CHANGE_DIR flag is not given we shouldn't change the CWD which the
    // standard dialog does by default (notice that under NT it does it anyhow,
    // OFN_NOCHANGEDIR or not, see below)
    if ( !HasFdFlag(wxFD_CHANGE_DIR) )
    {
        msw_flags |= OFN_NOCHANGEDIR;
    }

    if ( HasFdFlag(wxFD_OVERWRITE_PROMPT) )
    {
        msw_flags |= OFN_OVERWRITEPROMPT;
    }

    wxOPENFILENAME of;
    wxZeroMemory(of);

    of.lStructSize       = gs_ofStructSize;
    of.hwndOwner         = hWnd;
    of.lpstrTitle        = m_message.t_str();
    of.lpstrFileTitle    = titleBuffer;
    of.nMaxFileTitle     = wxMAXFILE + 1 + wxMAXEXT;

#ifndef __WXWINCE__
    GlobalPtr hgbl;
    if ( HasExtraControlCreator() )
    {
        msw_flags |= OFN_ENABLETEMPLATEHANDLE;

        hgbl.Init(256, GMEM_ZEROINIT);
        GlobalPtrLock hgblLock(hgbl);
        LPDLGTEMPLATE lpdt = static_cast<LPDLGTEMPLATE>(hgblLock.Get());

        // Define a dialog box.

        lpdt->style = DS_CONTROL | WS_CHILD | WS_CLIPSIBLINGS;
        lpdt->cdit = 0;         // Number of controls
        lpdt->x = 0;
        lpdt->y = 0;

        // convert the size of the extra controls to the dialog units
        const wxSize extraSize = GetExtraControlSize();
        const LONG baseUnits = ::GetDialogBaseUnits();
        lpdt->cx = ::MulDiv(extraSize.x, 4, LOWORD(baseUnits));
        lpdt->cy = ::MulDiv(extraSize.y, 8, HIWORD(baseUnits));

        // after the DLGTEMPLATE there are 3 additional WORDs for dialog menu,
        // class and title, all three set to zeros.

        of.hInstance = (HINSTANCE)lpdt;
    }
#endif // __WXWINCE__

    // Convert forward slashes to backslashes (file selector doesn't like
    // forward slashes) and also squeeze multiple consecutive slashes into one
    // as it doesn't like two backslashes in a row neither

    wxString  dir;
    size_t    i, len = m_dir.length();
    dir.reserve(len);
    for ( i = 0; i < len; i++ )
    {
        wxChar ch = m_dir[i];
        switch ( ch )
        {
            case wxT('/'):
                // convert to backslash
                ch = wxT('\\');

                // fall through

            case wxT('\\'):
                while ( i < len - 1 )
                {
                    wxChar chNext = m_dir[i + 1];
                    if ( chNext != wxT('\\') && chNext != wxT('/') )
                        break;

                    // ignore the next one, unless it is at the start of a UNC path
                    if (i > 0)
                        i++;
                    else
                        break;
                }
                // fall through

            default:
                // normal char
                dir += ch;
        }
    }

    of.lpstrInitialDir   = dir.c_str();

    of.Flags             = msw_flags;
    of.lpfnHook          = wxFileDialogHookFunction;
    of.lCustData         = (LPARAM)this;

    wxArrayString wildDescriptions, wildFilters;

    size_t items = wxParseCommonDialogsFilter(m_wildCard, wildDescriptions, wildFilters);

    wxASSERT_MSG( items > 0 , wxT("empty wildcard list") );

    wxString filterBuffer;

    for (i = 0; i < items ; i++)
    {
        filterBuffer += wildDescriptions[i];
        filterBuffer += wxT("|");
        filterBuffer += wildFilters[i];
        filterBuffer += wxT("|");
    }

    // Replace | with \0
    for (i = 0; i < filterBuffer.length(); i++ ) {
        if ( filterBuffer.GetChar(i) == wxT('|') ) {
            filterBuffer[i] = wxT('\0');
        }
    }

    of.lpstrFilter  = filterBuffer.t_str();
    of.nFilterIndex = m_filterIndex + 1;

    //=== Setting defaultFileName >>=========================================

    wxStrlcpy(fileNameBuffer, m_fileName.c_str(), WXSIZEOF(fileNameBuffer));

    of.lpstrFile = fileNameBuffer;  // holds returned filename
    of.nMaxFile  = wxMAXPATH;

    // we must set the default extension because otherwise Windows would check
    // for the existing of a wrong file with wxFD_OVERWRITE_PROMPT (i.e. if the
    // user types "foo" and the default extension is ".bar" we should force it
    // to check for "foo.bar" existence and not "foo")
    wxString defextBuffer; // we need it to be alive until GetSaveFileName()!
    if (HasFdFlag(wxFD_SAVE))
    {
        const wxChar* extension = filterBuffer.t_str();
        int maxFilter = (int)(of.nFilterIndex*2L) - 1;

        for( int j = 0; j < maxFilter; j++ )           // get extension
            extension = extension + wxStrlen( extension ) + 1;

        // use dummy name a to avoid assert in AppendExtension
        defextBuffer = AppendExtension(wxT("a"), extension);
        if (defextBuffer.StartsWith(wxT("a.")))
        {
            defextBuffer = defextBuffer.Mid(2); // remove "a."
            of.lpstrDefExt = defextBuffer.c_str();
        }
    }

    // Create a temporary struct to restore the CWD when we exit this function
    // store off before the standard windows dialog can possibly change it
    struct CwdRestore
    {
        wxString value;
        ~CwdRestore()
        {
            if (!value.empty())
                wxSetWorkingDirectory(value);
        }
    } cwdOrig;

    // GetOpenFileName will always change the current working directory on
    // (according to MSDN) "Windows NT 4.0/2000/XP" because the flag
    // OFN_NOCHANGEDIR has no effect.  If the user did not specify
    // wxFD_CHANGE_DIR let's restore the current working directory to what it
    // was before the dialog was shown.
    if (msw_flags & OFN_NOCHANGEDIR)
        cwdOrig.value = wxGetCwd();

    //== Execute FileDialog >>=================================================

    if ( !ShowCommFileDialog(&of, m_windowStyle) )
        return wxID_CANCEL;

    m_fileNames.Empty();

    if ( ( HasFdFlag(wxFD_MULTIPLE) ) &&
#if defined(OFN_EXPLORER)
         ( fileNameBuffer[of.nFileOffset-1] == wxT('\0') )
#else
         ( fileNameBuffer[of.nFileOffset-1] == wxT(' ') )
#endif // OFN_EXPLORER
       )
    {
#if defined(OFN_EXPLORER)
        m_dir = fileNameBuffer;
        i = of.nFileOffset;
        m_fileName = &fileNameBuffer[i];
        m_fileNames.Add(m_fileName);
        i += m_fileName.length() + 1;

        while (fileNameBuffer[i] != wxT('\0'))
        {
            m_fileNames.Add(&fileNameBuffer[i]);
            i += wxStrlen(&fileNameBuffer[i]) + 1;
        }
#else
        wxStringTokenizer toke(fileNameBuffer, wxT(" \t\r\n"));
        m_dir = toke.GetNextToken();
        m_fileName = toke.GetNextToken();
        m_fileNames.Add(m_fileName);

        while (toke.HasMoreTokens())
            m_fileNames.Add(toke.GetNextToken());
#endif // OFN_EXPLORER

        m_path = m_dir;
        if ( m_dir.Last() != wxT('\\') )
            m_path += wxT('\\');

        m_path += m_fileName;
        m_filterIndex = (int)of.nFilterIndex - 1;
    }
    else
    {
        //=== Adding the correct extension >>=================================

        m_filterIndex = (int)of.nFilterIndex - 1;

        if ( !of.nFileExtension ||
             (of.nFileExtension && fileNameBuffer[of.nFileExtension] == wxT('\0')) )
        {
            // User has typed a filename without an extension:
            const wxChar* extension = filterBuffer.t_str();
            int   maxFilter = (int)(of.nFilterIndex*2L) - 1;

            for( int j = 0; j < maxFilter; j++ )           // get extension
                extension = extension + wxStrlen( extension ) + 1;

            m_fileName = AppendExtension(fileNameBuffer, extension);
            wxStrlcpy(fileNameBuffer, m_fileName.c_str(), WXSIZEOF(fileNameBuffer));
        }

        m_path = fileNameBuffer;
        m_fileName = wxFileNameFromPath(fileNameBuffer);
        m_fileNames.Add(m_fileName);
        m_dir = wxPathOnly(fileNameBuffer);
        }

    return wxID_OK;

}

#endif // wxUSE_FILEDLG && !(__SMARTPHONE__ && __WXWINCE__)
