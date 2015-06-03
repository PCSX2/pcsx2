/////////////////////////////////////////////////////////////////////////////
// Name:        src/common/appcmn.cpp
// Purpose:     wxAppBase methods common to all platforms
// Author:      Vadim Zeitlin
// Modified by:
// Created:     18.10.99
// Copyright:   (c) Vadim Zeitlin
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

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/window.h"
    #include "wx/bitmap.h"
    #include "wx/log.h"
    #include "wx/msgdlg.h"
    #include "wx/confbase.h"
    #include "wx/utils.h"
    #include "wx/wxcrtvararg.h"
#endif

#include "wx/apptrait.h"
#include "wx/cmdline.h"
#include "wx/msgout.h"
#include "wx/thread.h"
#include "wx/vidmode.h"
#include "wx/evtloop.h"

#if wxUSE_FONTMAP
    #include "wx/fontmap.h"
#endif // wxUSE_FONTMAP

// DLL options compatibility check:
#include "wx/build.h"
WX_CHECK_BUILD_OPTIONS("wxCore")

// ============================================================================
// wxAppBase implementation
// ============================================================================

// ----------------------------------------------------------------------------
// initialization
// ----------------------------------------------------------------------------

wxAppBase::wxAppBase()
{
    m_topWindow = NULL;

    m_useBestVisual = false;
    m_forceTrueColour = false;

    m_isActive = true;

    // We don't want to exit the app if the user code shows a dialog from its
    // OnInit() -- but this is what would happen if we set m_exitOnFrameDelete
    // to Yes initially as this dialog would be the last top level window.
    // OTOH, if we set it to No initially we'll have to overwrite it with Yes
    // when we enter our OnRun() because we do want the default behaviour from
    // then on. But this would be a problem if the user code calls
    // SetExitOnFrameDelete(false) from OnInit().
    //
    // So we use the special "Later" value which is such that
    // GetExitOnFrameDelete() returns false for it but which we know we can
    // safely (i.e. without losing the effect of the users SetExitOnFrameDelete
    // call) overwrite in OnRun()
    m_exitOnFrameDelete = Later;
}

bool wxAppBase::Initialize(int& argcOrig, wxChar **argvOrig)
{
#ifdef __WXOSX__
    // Mac OS X passes a process serial number command line argument when
    // the application is launched from the Finder. This argument must be
    // removed from the command line arguments before being handled by the
    // application (otherwise applications would need to handle it)
    //
    // Notice that this has to be done for all ports that can be used under OS
    // X (e.g. wxGTK) and not just wxOSX itself, hence this code is here and
    // not in a port-specific file.
    if ( argcOrig > 1 )
    {
        static const wxChar *ARG_PSN = wxT("-psn_");
        if ( wxStrncmp(argvOrig[1], ARG_PSN, wxStrlen(ARG_PSN)) == 0 )
        {
            // remove this argument
            --argcOrig;
            memmove(argvOrig + 1, argvOrig + 2, argcOrig * sizeof(wxChar*));
        }
    }
#endif // __WXOSX__

    if ( !wxAppConsole::Initialize(argcOrig, argvOrig) )
        return false;

    wxInitializeStockLists();

    wxBitmap::InitStandardHandlers();

    // for compatibility call the old initialization function too
    if ( !OnInitGui() )
        return false;

    return true;
}

// ----------------------------------------------------------------------------
// cleanup
// ----------------------------------------------------------------------------

wxAppBase::~wxAppBase()
{
    // this destructor is required for Darwin
}

void wxAppBase::CleanUp()
{
    // Clean up any still pending objects. Normally there shouldn't any as we
    // already do this in OnExit(), but this could happen if the user code has
    // somehow managed to create more of them since then or just forgot to call
    // the base class OnExit().
    DeletePendingObjects();

    // and any remaining TLWs (they remove themselves from wxTopLevelWindows
    // when destroyed, so iterate until none are left)
    while ( !wxTopLevelWindows.empty() )
    {
        // do not use Destroy() here as it only puts the TLW in pending list
        // but we want to delete them now
        delete wxTopLevelWindows.GetFirst()->GetData();
    }

    // undo everything we did in Initialize() above
    wxBitmap::CleanUpHandlers();

    wxStockGDI::DeleteAll();

    wxDeleteStockLists();

    wxDELETE(wxTheColourDatabase);

    wxAppConsole::CleanUp();
}

// ----------------------------------------------------------------------------
// various accessors
// ----------------------------------------------------------------------------

wxWindow* wxAppBase::GetTopWindow() const
{
    wxWindow* window = m_topWindow;
    if (window == NULL && wxTopLevelWindows.GetCount() > 0)
        window = wxTopLevelWindows.GetFirst()->GetData();
    return window;
}

wxVideoMode wxAppBase::GetDisplayMode() const
{
    return wxVideoMode();
}

wxLayoutDirection wxAppBase::GetLayoutDirection() const
{
#if wxUSE_INTL
    const wxLocale *const locale = wxGetLocale();
    if ( locale )
    {
        const wxLanguageInfo *const
            info = wxLocale::GetLanguageInfo(locale->GetLanguage());

        if ( info )
            return info->LayoutDirection;
    }
#endif // wxUSE_INTL

    // we don't know
    return wxLayout_Default;
}

#if wxUSE_CMDLINE_PARSER

// ----------------------------------------------------------------------------
// GUI-specific command line options handling
// ----------------------------------------------------------------------------

#ifdef __WXUNIVERSAL__
#define OPTION_THEME   "theme"
#endif
#if defined(__WXDFB__)
#define OPTION_MODE    "mode"
#endif

void wxAppBase::OnInitCmdLine(wxCmdLineParser& parser)
{
    // first add the standard non GUI options
    wxAppConsole::OnInitCmdLine(parser);

    // the standard command line options
    static const wxCmdLineEntryDesc cmdLineGUIDesc[] =
    {
#ifdef __WXUNIVERSAL__
        {
            wxCMD_LINE_OPTION,
            NULL,
            OPTION_THEME,
            gettext_noop("specify the theme to use"),
            wxCMD_LINE_VAL_STRING,
            0x0
        },
#endif // __WXUNIVERSAL__

#if defined(__WXDFB__)
        // VS: this is not specific to wxDFB, all fullscreen (framebuffer) ports
        //     should provide this option. That's why it is in common/appcmn.cpp
        //     and not dfb/app.cpp
        {
            wxCMD_LINE_OPTION,
            NULL,
            OPTION_MODE,
            gettext_noop("specify display mode to use (e.g. 640x480-16)"),
            wxCMD_LINE_VAL_STRING,
            0x0
        },
#endif // __WXDFB__

        // terminator
        wxCMD_LINE_DESC_END
    };

    parser.SetDesc(cmdLineGUIDesc);
}

bool wxAppBase::OnCmdLineParsed(wxCmdLineParser& parser)
{
#ifdef __WXUNIVERSAL__
    wxString themeName;
    if ( parser.Found(OPTION_THEME, &themeName) )
    {
        wxTheme *theme = wxTheme::Create(themeName);
        if ( !theme )
        {
            wxLogError(_("Unsupported theme '%s'."), themeName.c_str());
            return false;
        }

        // Delete the defaultly created theme and set the new theme.
        delete wxTheme::Get();
        wxTheme::Set(theme);
    }
#endif // __WXUNIVERSAL__

#if defined(__WXDFB__)
    wxString modeDesc;
    if ( parser.Found(OPTION_MODE, &modeDesc) )
    {
        unsigned w, h, bpp;
        if ( wxSscanf(modeDesc.c_str(), wxT("%ux%u-%u"), &w, &h, &bpp) != 3 )
        {
            wxLogError(_("Invalid display mode specification '%s'."), modeDesc.c_str());
            return false;
        }

        if ( !SetDisplayMode(wxVideoMode(w, h, bpp)) )
            return false;
    }
#endif // __WXDFB__

    return wxAppConsole::OnCmdLineParsed(parser);
}

#endif // wxUSE_CMDLINE_PARSER

// ----------------------------------------------------------------------------
// OnXXX() hooks
// ----------------------------------------------------------------------------

bool wxAppBase::OnInitGui()
{
#ifdef __WXUNIVERSAL__
    if ( !wxTheme::Get() && !wxTheme::CreateDefault() )
        return false;
#endif // __WXUNIVERSAL__

    return true;
}

int wxAppBase::OnRun()
{
    // see the comment in ctor: if the initial value hasn't been changed, use
    // the default Yes from now on
    if ( m_exitOnFrameDelete == Later )
    {
        m_exitOnFrameDelete = Yes;
    }
    //else: it has been changed, assume the user knows what he is doing

    return wxAppConsole::OnRun();
}

int wxAppBase::OnExit()
{
#ifdef __WXUNIVERSAL__
    delete wxTheme::Set(NULL);
#endif // __WXUNIVERSAL__

    return wxAppConsole::OnExit();
}

wxAppTraits *wxAppBase::CreateTraits()
{
    return new wxGUIAppTraits;
}

// ----------------------------------------------------------------------------
// misc
// ----------------------------------------------------------------------------

void wxAppBase::SetActive(bool active, wxWindow * WXUNUSED(lastFocus))
{
    if ( active == m_isActive )
        return;

    m_isActive = active;

    wxActivateEvent event(wxEVT_ACTIVATE_APP, active);
    event.SetEventObject(this);

    (void)ProcessEvent(event);
}

bool wxAppBase::SafeYield(wxWindow *win, bool onlyIfNeeded)
{
    wxWindowDisabler wd(win);

    wxEventLoopBase * const loop = wxEventLoopBase::GetActive();

    return loop && loop->Yield(onlyIfNeeded);
}

bool wxAppBase::SafeYieldFor(wxWindow *win, long eventsToProcess)
{
    wxWindowDisabler wd(win);

    wxEventLoopBase * const loop = wxEventLoopBase::GetActive();

    return loop && loop->YieldFor(eventsToProcess);
}


// ----------------------------------------------------------------------------
// idle handling
// ----------------------------------------------------------------------------

// Returns true if more time is needed.
bool wxAppBase::ProcessIdle()
{
    // call the base class version first to send the idle event to wxTheApp
    // itself
    bool needMore = wxAppConsoleBase::ProcessIdle();
    wxIdleEvent event;
    wxWindowList::compatibility_iterator node = wxTopLevelWindows.GetFirst();
    while (node)
    {
        wxWindow* win = node->GetData();

        // Don't send idle events to the windows that are about to be destroyed
        // anyhow, this is wasteful and unexpected.
        if ( !wxPendingDelete.Member(win) && win->SendIdleEvents(event) )
            needMore = true;
        node = node->GetNext();
    }

    wxUpdateUIEvent::ResetUpdateTime();

    return needMore;
}

// ----------------------------------------------------------------------------
// wxGUIAppTraitsBase
// ----------------------------------------------------------------------------

#if wxUSE_LOG

wxLog *wxGUIAppTraitsBase::CreateLogTarget()
{
#if wxUSE_LOGGUI
#ifndef __WXOSX_IPHONE__
    return new wxLogGui;
#else
    return new wxLogStderr;
#endif
#else
    // we must have something!
    return new wxLogStderr;
#endif
}

#endif // wxUSE_LOG

wxMessageOutput *wxGUIAppTraitsBase::CreateMessageOutput()
{
    // The standard way of printing help on command line arguments (app --help)
    // is (according to common practice):
    //     - console apps: to stderr (on any platform)
    //     - GUI apps: stderr on Unix platforms (!)
    //                 stderr if available and message box otherwise on others
    //                 (currently stderr only Windows if app running from console)
#ifdef __UNIX__
    return new wxMessageOutputStderr;
#else // !__UNIX__
    // wxMessageOutputMessageBox doesn't work under Motif
    #ifdef __WXMOTIF__
        return new wxMessageOutputLog;
    #elif wxUSE_MSGDLG
        return new wxMessageOutputBest(wxMSGOUT_PREFER_STDERR);
    #else
        return new wxMessageOutputStderr;
    #endif
#endif // __UNIX__/!__UNIX__
}

#if wxUSE_FONTMAP

wxFontMapper *wxGUIAppTraitsBase::CreateFontMapper()
{
    return new wxFontMapper;
}

#endif // wxUSE_FONTMAP

wxRendererNative *wxGUIAppTraitsBase::CreateRenderer()
{
    // use the default native renderer by default
    return NULL;
}

bool wxGUIAppTraitsBase::ShowAssertDialog(const wxString& msg)
{
#if wxDEBUG_LEVEL
    // under MSW we prefer to use the base class version using ::MessageBox()
    // even if wxMessageBox() is available because it has less chances to
    // double fault our app than our wxMessageBox()
    //
    // under DFB the message dialog is not always functional right now
    //
    // and finally we can't use wxMessageBox() if it wasn't compiled in, of
    // course
#if !defined(__WXMSW__) && !defined(__WXDFB__) && wxUSE_MSGDLG

    // we can't (safely) show the GUI dialog from another thread, only do it
    // for the asserts in the main thread
    if ( wxIsMainThread() )
    {
        wxString msgDlg = msg;

#if wxUSE_STACKWALKER
        const wxString stackTrace = GetAssertStackTrace();
        if ( !stackTrace.empty() )
            msgDlg << wxT("\n\nCall stack:\n") << stackTrace;
#endif // wxUSE_STACKWALKER

        // this message is intentionally not translated -- it is for
        // developpers only
        msgDlg += wxT("\nDo you want to stop the program?\n")
                  wxT("You can also choose [Cancel] to suppress ")
                  wxT("further warnings.");

        switch ( wxMessageBox(msgDlg, wxT("wxWidgets Debug Alert"),
                              wxYES_NO | wxCANCEL | wxICON_STOP ) )
        {
            case wxYES:
                wxTrap();
                break;

            case wxCANCEL:
                // no more asserts
                return true;

            //case wxNO: nothing to do
        }

        return false;
    }
#endif // wxUSE_MSGDLG
#endif // wxDEBUG_LEVEL

    return wxAppTraitsBase::ShowAssertDialog(msg);
}

bool wxGUIAppTraitsBase::HasStderr()
{
    // we consider that under Unix stderr always goes somewhere, even if the
    // user doesn't always see it under GUI desktops
#ifdef __UNIX__
    return true;
#else
    return false;
#endif
}

