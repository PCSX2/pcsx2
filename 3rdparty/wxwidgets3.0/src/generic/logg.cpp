/////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/logg.cpp
// Purpose:     wxLog-derived classes which need GUI support (the rest is in
//              src/common/log.cpp)
// Author:      Vadim Zeitlin
// Modified by:
// Created:     20.09.99 (extracted from src/common/log.cpp)
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
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

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/button.h"
    #include "wx/intl.h"
    #include "wx/log.h"
    #include "wx/menu.h"
    #include "wx/frame.h"
    #include "wx/filedlg.h"
    #include "wx/msgdlg.h"
    #include "wx/textctrl.h"
    #include "wx/sizer.h"
    #include "wx/statbmp.h"
    #include "wx/settings.h"
    #include "wx/wxcrtvararg.h"
#endif // WX_PRECOMP

#if wxUSE_LOGGUI || wxUSE_LOGWINDOW

#include "wx/file.h"
#include "wx/clipbrd.h"
#include "wx/dataobj.h"
#include "wx/textfile.h"
#include "wx/statline.h"
#include "wx/artprov.h"
#include "wx/collpane.h"
#include "wx/arrstr.h"
#include "wx/msgout.h"

#ifdef  __WXMSW__
    // for OutputDebugString()
    #include  "wx/msw/private.h"
#endif // Windows


#ifdef  __WXPM__
    #include <time.h>
#endif

#if wxUSE_LOG_DIALOG
    #include "wx/listctrl.h"
    #include "wx/imaglist.h"
    #include "wx/image.h"
#endif // wxUSE_LOG_DIALOG/!wxUSE_LOG_DIALOG

#include "wx/time.h"

// the suffix we add to the button to show that the dialog can be expanded
#define EXPAND_SUFFIX wxT(" >>")

#define CAN_SAVE_FILES (wxUSE_FILE && wxUSE_FILEDLG)

// ----------------------------------------------------------------------------
// private classes
// ----------------------------------------------------------------------------

#if wxUSE_LOG_DIALOG

// this function is a wrapper around strftime(3)
// allows to exclude the usage of wxDateTime
static wxString TimeStamp(const wxString& format, time_t t)
{
    wxChar buf[4096];
    struct tm tm;
    if ( !wxStrftime(buf, WXSIZEOF(buf), format, wxLocaltime_r(&t, &tm)) )
    {
        // buffer is too small?
        wxFAIL_MSG(wxT("strftime() failed"));
    }
    return wxString(buf);
}


class wxLogDialog : public wxDialog
{
public:
    wxLogDialog(wxWindow *parent,
                const wxArrayString& messages,
                const wxArrayInt& severity,
                const wxArrayLong& timess,
                const wxString& caption,
                long style);
    virtual ~wxLogDialog();

    // event handlers
    void OnOk(wxCommandEvent& event);
#if wxUSE_CLIPBOARD
    void OnCopy(wxCommandEvent& event);
#endif // wxUSE_CLIPBOARD
#if CAN_SAVE_FILES
    void OnSave(wxCommandEvent& event);
#endif // CAN_SAVE_FILES
    void OnListItemActivated(wxListEvent& event);

private:
    // create controls needed for the details display
    void CreateDetailsControls(wxWindow *);

    // if necessary truncates the given string and adds an ellipsis
    wxString EllipsizeString(const wxString &text)
    {
        if (ms_maxLength > 0 &&
            text.length() > ms_maxLength)
        {
            wxString ret(text);
            ret.Truncate(ms_maxLength);
            ret << "...";
            return ret;
        }

        return text;
    }

#if CAN_SAVE_FILES || wxUSE_CLIPBOARD
    // return the contents of the dialog as a multiline string
    wxString GetLogMessages() const;
#endif // CAN_SAVE_FILES || wxUSE_CLIPBOARD


    // the data for the listctrl
    wxArrayString m_messages;
    wxArrayInt m_severity;
    wxArrayLong m_times;

    // the controls which are not shown initially (but only when details
    // button is pressed)
    wxListCtrl *m_listctrl;

    // the translated "Details" string
    static wxString ms_details;

    // the maximum length of the log message
    static size_t ms_maxLength;

    DECLARE_EVENT_TABLE()
    wxDECLARE_NO_COPY_CLASS(wxLogDialog);
};

BEGIN_EVENT_TABLE(wxLogDialog, wxDialog)
    EVT_BUTTON(wxID_OK, wxLogDialog::OnOk)
#if wxUSE_CLIPBOARD
    EVT_BUTTON(wxID_COPY,   wxLogDialog::OnCopy)
#endif // wxUSE_CLIPBOARD
#if CAN_SAVE_FILES
    EVT_BUTTON(wxID_SAVE,   wxLogDialog::OnSave)
#endif // CAN_SAVE_FILES
    EVT_LIST_ITEM_ACTIVATED(wxID_ANY, wxLogDialog::OnListItemActivated)
END_EVENT_TABLE()

#endif // wxUSE_LOG_DIALOG

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

#if CAN_SAVE_FILES

// pass an uninitialized file object, the function will ask the user for the
// filename and try to open it, returns true on success (file was opened),
// false if file couldn't be opened/created and -1 if the file selection
// dialog was cancelled
static int OpenLogFile(wxFile& file, wxString *filename = NULL, wxWindow *parent = NULL);

#endif // CAN_SAVE_FILES

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxLogGui implementation (FIXME MT-unsafe)
// ----------------------------------------------------------------------------

#if wxUSE_LOGGUI

wxLogGui::wxLogGui()
{
    Clear();
}

void wxLogGui::Clear()
{
    m_bErrors =
    m_bWarnings =
    m_bHasMessages = false;

    m_aMessages.Empty();
    m_aSeverity.Empty();
    m_aTimes.Empty();
}

int wxLogGui::GetSeverityIcon() const
{
    return m_bErrors ? wxICON_STOP
                     : m_bWarnings ? wxICON_EXCLAMATION
                                   : wxICON_INFORMATION;
}

wxString wxLogGui::GetTitle() const
{
    wxString titleFormat;
    switch ( GetSeverityIcon() )
    {
        case wxICON_STOP:
            titleFormat = _("%s Error");
            break;

        case wxICON_EXCLAMATION:
            titleFormat = _("%s Warning");
            break;

        default:
            wxFAIL_MSG( "unexpected icon severity" );
            // fall through

        case wxICON_INFORMATION:
            titleFormat = _("%s Information");
    }

    return wxString::Format(titleFormat, wxTheApp->GetAppDisplayName());
}

void
wxLogGui::DoShowSingleLogMessage(const wxString& message,
                                 const wxString& title,
                                 int style)
{
    wxMessageBox(message, title, wxOK | style);
}

void
wxLogGui::DoShowMultipleLogMessages(const wxArrayString& messages,
                                    const wxArrayInt& severities,
                                    const wxArrayLong& times,
                                    const wxString& title,
                                    int style)
{
#if wxUSE_LOG_DIALOG
    wxLogDialog dlg(NULL,
                    messages, severities, times,
                    title, style);

    // clear the message list before showing the dialog because while it's
    // shown some new messages may appear
    Clear();

    (void)dlg.ShowModal();
#else // !wxUSE_LOG_DIALOG
    // start from the most recent message
    wxString message;
    const size_t nMsgCount = messages.size();
    message.reserve(nMsgCount*100);
    for ( size_t n = nMsgCount; n > 0; n-- ) {
        message << m_aMessages[n - 1] << wxT("\n");
    }

    DoShowSingleLogMessage(message, title, style);
#endif // wxUSE_LOG_DIALOG/!wxUSE_LOG_DIALOG
}

void wxLogGui::Flush()
{
    wxLog::Flush();

    if ( !m_bHasMessages )
        return;

    // do it right now to block any new calls to Flush() while we're here
    m_bHasMessages = false;

    // note that this must be done before examining m_aMessages as it may log
    // yet another message
    const unsigned repeatCount = LogLastRepeatIfNeeded();

    const size_t nMsgCount = m_aMessages.size();

    if ( repeatCount > 0 )
    {
        m_aMessages[nMsgCount - 1] << " (" << m_aMessages[nMsgCount - 2] << ")";
    }

    const wxString title = GetTitle();
    const int style = GetSeverityIcon();

    // avoid showing other log dialogs until we're done with the dialog we're
    // showing right now: nested modal dialogs make for really bad UI!
    Suspend();

    if ( nMsgCount == 1 )
    {
        // make a copy before calling Clear()
        const wxString message(m_aMessages[0]);
        Clear();

        DoShowSingleLogMessage(message, title, style);
    }
    else // more than one message
    {
        wxArrayString messages;
        wxArrayInt severities;
        wxArrayLong times;

        messages.swap(m_aMessages);
        severities.swap(m_aSeverity);
        times.swap(m_aTimes);

        Clear();

        DoShowMultipleLogMessages(messages, severities, times, title, style);
    }

    // allow flushing the logs again
    Resume();
}

// log all kinds of messages
void wxLogGui::DoLogRecord(wxLogLevel level,
                           const wxString& msg,
                           const wxLogRecordInfo& info)
{
    switch ( level )
    {
        case wxLOG_Info:
            if ( GetVerbose() )
        case wxLOG_Message:
            {
                m_aMessages.Add(msg);
                m_aSeverity.Add(wxLOG_Message);
                m_aTimes.Add((long)info.timestamp);
                m_bHasMessages = true;
            }
            break;

        case wxLOG_Status:
#if wxUSE_STATUSBAR
            {
                wxFrame *pFrame = NULL;

                // check if the frame was passed to us explicitly
                wxUIntPtr ptr = 0;
                if ( info.GetNumValue(wxLOG_KEY_FRAME, &ptr) )
                {
                    pFrame = static_cast<wxFrame *>(wxUIntToPtr(ptr));
                }

                // find the top window and set it's status text if it has any
                if ( pFrame == NULL ) {
                    wxWindow *pWin = wxTheApp->GetTopWindow();
                    if ( wxDynamicCast(pWin, wxFrame) ) {
                        pFrame = (wxFrame *)pWin;
                    }
                }

                if ( pFrame && pFrame->GetStatusBar() )
                    pFrame->SetStatusText(msg);
            }
#endif // wxUSE_STATUSBAR
            break;

        case wxLOG_Error:
            if ( !m_bErrors ) {
#if !wxUSE_LOG_DIALOG
                // discard earlier informational messages if this is the 1st
                // error because they might not make sense any more and showing
                // them in a message box might be confusing
                m_aMessages.Empty();
                m_aSeverity.Empty();
                m_aTimes.Empty();
#endif // wxUSE_LOG_DIALOG
                m_bErrors = true;
            }
            // fall through

        case wxLOG_Warning:
            if ( !m_bErrors ) {
                // for the warning we don't discard the info messages
                m_bWarnings = true;
            }

            m_aMessages.Add(msg);
            m_aSeverity.Add((int)level);
            m_aTimes.Add((long)info.timestamp);
            m_bHasMessages = true;
            break;

        case wxLOG_Debug:
        case wxLOG_Trace:
            // let the base class deal with debug/trace messages
            wxLog::DoLogRecord(level, msg, info);
            break;

        case wxLOG_FatalError:
        case wxLOG_Max:
            // fatal errors are shown immediately and terminate the program so
            // we should never see them here
            wxFAIL_MSG("unexpected log level");
            break;

        case wxLOG_Progress:
        case wxLOG_User:
            // just ignore those: passing them to the base class would result
            // in asserts from DoLogText() because DoLogTextAtLevel() would
            // call it as it doesn't know how to handle these levels otherwise
            break;
    }
}

#endif   // wxUSE_LOGGUI

// ----------------------------------------------------------------------------
// wxLogWindow and wxLogFrame implementation
// ----------------------------------------------------------------------------

#if wxUSE_LOGWINDOW

// log frame class
// ---------------
class wxLogFrame : public wxFrame
{
public:
    // ctor & dtor
    wxLogFrame(wxWindow *pParent, wxLogWindow *log, const wxString& szTitle);
    virtual ~wxLogFrame();

    // Don't prevent the application from exiting if just this frame remains.
    virtual bool ShouldPreventAppExit() const { return false; }

    // menu callbacks
    void OnClose(wxCommandEvent& event);
    void OnCloseWindow(wxCloseEvent& event);
#if CAN_SAVE_FILES
    void OnSave(wxCommandEvent& event);
#endif // CAN_SAVE_FILES
    void OnClear(wxCommandEvent& event);

    // do show the message in the text control
    void ShowLogMessage(const wxString& message)
    {
        m_pTextCtrl->AppendText(message + wxS('\n'));
    }

private:
    // use standard ids for our commands!
    enum
    {
        Menu_Close = wxID_CLOSE,
        Menu_Save  = wxID_SAVE,
        Menu_Clear = wxID_CLEAR
    };

    // common part of OnClose() and OnCloseWindow()
    void DoClose();

    wxTextCtrl  *m_pTextCtrl;
    wxLogWindow *m_log;

    DECLARE_EVENT_TABLE()
    wxDECLARE_NO_COPY_CLASS(wxLogFrame);
};

BEGIN_EVENT_TABLE(wxLogFrame, wxFrame)
    // wxLogWindow menu events
    EVT_MENU(Menu_Close, wxLogFrame::OnClose)
#if CAN_SAVE_FILES
    EVT_MENU(Menu_Save,  wxLogFrame::OnSave)
#endif // CAN_SAVE_FILES
    EVT_MENU(Menu_Clear, wxLogFrame::OnClear)

    EVT_CLOSE(wxLogFrame::OnCloseWindow)
END_EVENT_TABLE()

wxLogFrame::wxLogFrame(wxWindow *pParent, wxLogWindow *log, const wxString& szTitle)
          : wxFrame(pParent, wxID_ANY, szTitle)
{
    m_log = log;

    m_pTextCtrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
            wxDefaultSize,
            wxTE_MULTILINE  |
            wxHSCROLL       |
            // needed for Win32 to avoid 65Kb limit but it doesn't work well
            // when using RichEdit 2.0 which we always do in the Unicode build
#if !wxUSE_UNICODE
            wxTE_RICH       |
#endif // !wxUSE_UNICODE
            wxTE_READONLY);

#if wxUSE_MENUS
    // create menu
    wxMenuBar *pMenuBar = new wxMenuBar;
    wxMenu *pMenu = new wxMenu;
#if CAN_SAVE_FILES
    pMenu->Append(Menu_Save,  _("Save &As..."), _("Save log contents to file"));
#endif // CAN_SAVE_FILES
    pMenu->Append(Menu_Clear, _("C&lear"), _("Clear the log contents"));
    pMenu->AppendSeparator();
    pMenu->Append(Menu_Close, _("&Close"), _("Close this window"));
    pMenuBar->Append(pMenu, _("&Log"));
    SetMenuBar(pMenuBar);
#endif // wxUSE_MENUS

#if wxUSE_STATUSBAR
    // status bar for menu prompts
    CreateStatusBar();
#endif // wxUSE_STATUSBAR
}

void wxLogFrame::DoClose()
{
    if ( m_log->OnFrameClose(this) )
    {
        // instead of closing just hide the window to be able to Show() it
        // later
        Show(false);
    }
}

void wxLogFrame::OnClose(wxCommandEvent& WXUNUSED(event))
{
    DoClose();
}

void wxLogFrame::OnCloseWindow(wxCloseEvent& WXUNUSED(event))
{
    DoClose();
}

#if CAN_SAVE_FILES
void wxLogFrame::OnSave(wxCommandEvent& WXUNUSED(event))
{
    wxString filename;
    wxFile file;
    int rc = OpenLogFile(file, &filename, this);
    if ( rc == -1 )
    {
        // cancelled
        return;
    }

    bool bOk = rc != 0;

    // retrieve text and save it
    // -------------------------
    int nLines = m_pTextCtrl->GetNumberOfLines();
    for ( int nLine = 0; bOk && nLine < nLines; nLine++ ) {
        bOk = file.Write(m_pTextCtrl->GetLineText(nLine) +
                         wxTextFile::GetEOL());
    }

    if ( bOk )
        bOk = file.Close();

    if ( !bOk ) {
        wxLogError(_("Can't save log contents to file."));
    }
    else {
        wxLogStatus((wxFrame*)this, _("Log saved to the file '%s'."), filename.c_str());
    }
}
#endif // CAN_SAVE_FILES

void wxLogFrame::OnClear(wxCommandEvent& WXUNUSED(event))
{
    m_pTextCtrl->Clear();
}

wxLogFrame::~wxLogFrame()
{
    m_log->OnFrameDelete(this);
}

// wxLogWindow
// -----------

wxLogWindow::wxLogWindow(wxWindow *pParent,
                         const wxString& szTitle,
                         bool bShow,
                         bool bDoPass)
{
    // Initialize it to NULL to ensure that we don't crash if any log messages
    // are generated before the frame is fully created (while this doesn't
    // happen normally, it might, in principle).
    m_pLogFrame = NULL;

    PassMessages(bDoPass);

    m_pLogFrame = new wxLogFrame(pParent, this, szTitle);

    if ( bShow )
        m_pLogFrame->Show();
}

void wxLogWindow::Show(bool bShow)
{
    m_pLogFrame->Show(bShow);
}

void wxLogWindow::DoLogTextAtLevel(wxLogLevel level, const wxString& msg)
{
    if ( !m_pLogFrame )
        return;

    // don't put trace messages in the text window for 2 reasons:
    // 1) there are too many of them
    // 2) they may provoke other trace messages (e.g. wxMSW code uses
    //    wxLogTrace to log Windows messages and adding text to the control
    //    sends more of them) thus sending a program into an infinite loop
    if ( level == wxLOG_Trace )
        return;

    m_pLogFrame->ShowLogMessage(msg);
}

wxFrame *wxLogWindow::GetFrame() const
{
    return m_pLogFrame;
}

bool wxLogWindow::OnFrameClose(wxFrame * WXUNUSED(frame))
{
    // allow to close
    return true;
}

void wxLogWindow::OnFrameDelete(wxFrame * WXUNUSED(frame))
{
    m_pLogFrame = NULL;
}

wxLogWindow::~wxLogWindow()
{
    // may be NULL if log frame already auto destroyed itself
    delete m_pLogFrame;
}

#endif // wxUSE_LOGWINDOW

// ----------------------------------------------------------------------------
// wxLogDialog
// ----------------------------------------------------------------------------

#if wxUSE_LOG_DIALOG

wxString wxLogDialog::ms_details;
size_t wxLogDialog::ms_maxLength = 0;

wxLogDialog::wxLogDialog(wxWindow *parent,
                         const wxArrayString& messages,
                         const wxArrayInt& severity,
                         const wxArrayLong& times,
                         const wxString& caption,
                         long style)
           : wxDialog(parent, wxID_ANY, caption,
                      wxDefaultPosition, wxDefaultSize,
                      wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    // init the static variables:

    if ( ms_details.empty() )
    {
        // ensure that we won't loop here if wxGetTranslation()
        // happens to pop up a Log message while translating this :-)
        ms_details = wxTRANSLATE("&Details");
        ms_details = wxGetTranslation(ms_details);
#ifdef __SMARTPHONE__
        ms_details = wxStripMenuCodes(ms_details);
#endif
    }

    if ( ms_maxLength == 0 )
    {
        ms_maxLength = (2 * wxGetDisplaySize().x/3) / GetCharWidth();
    }

    size_t count = messages.GetCount();
    m_messages.Alloc(count);
    m_severity.Alloc(count);
    m_times.Alloc(count);

    for ( size_t n = 0; n < count; n++ )
    {
        m_messages.Add(messages[n]);
        m_severity.Add(severity[n]);
        m_times.Add(times[n]);
    }

    m_listctrl = NULL;

    bool isPda = (wxSystemSettings::GetScreenType() <= wxSYS_SCREEN_PDA);

    // create the controls which are always shown and layout them: we use
    // sizers even though our window is not resizable to calculate the size of
    // the dialog properly
    wxBoxSizer *sizerTop = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *sizerAll = new wxBoxSizer(isPda ? wxVERTICAL : wxHORIZONTAL);

    if (!isPda)
    {
        wxStaticBitmap *icon = new wxStaticBitmap
                                   (
                                    this,
                                    wxID_ANY,
                                    wxArtProvider::GetMessageBoxIcon(style)
                                   );
        sizerAll->Add(icon, wxSizerFlags().Centre());
    }

    // create the text sizer with a minimal size so that we are sure it won't be too small
    wxString message = EllipsizeString(messages.Last());
    wxSizer *szText = CreateTextSizer(message);
    szText->SetMinSize(wxMin(300, wxGetDisplaySize().x / 3), -1);

    sizerAll->Add(szText, wxSizerFlags(1).Centre().Border(wxLEFT | wxRIGHT));

    wxButton *btnOk = new wxButton(this, wxID_OK);
    sizerAll->Add(btnOk, wxSizerFlags().Centre());

    sizerTop->Add(sizerAll, wxSizerFlags().Expand().Border());


    // add the details pane
#ifndef __SMARTPHONE__

#if wxUSE_COLLPANE
    wxCollapsiblePane * const
        collpane = new wxCollapsiblePane(this, wxID_ANY, ms_details);
    sizerTop->Add(collpane, wxSizerFlags(1).Expand().Border());

    wxWindow *win = collpane->GetPane();
#else
    wxPanel* win = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                               wxBORDER_NONE);
#endif
    wxSizer * const paneSz = new wxBoxSizer(wxVERTICAL);

    CreateDetailsControls(win);

    paneSz->Add(m_listctrl, wxSizerFlags(1).Expand().Border(wxTOP));

#if wxUSE_CLIPBOARD || CAN_SAVE_FILES
    wxBoxSizer * const btnSizer = new wxBoxSizer(wxHORIZONTAL);

    wxSizerFlags flagsBtn;
    flagsBtn.Border(wxLEFT);

#if wxUSE_CLIPBOARD
    btnSizer->Add(new wxButton(win, wxID_COPY), flagsBtn);
#endif // wxUSE_CLIPBOARD

#if CAN_SAVE_FILES
    btnSizer->Add(new wxButton(win, wxID_SAVE), flagsBtn);
#endif // CAN_SAVE_FILES

    paneSz->Add(btnSizer, wxSizerFlags().Right().Border(wxTOP|wxBOTTOM));
#endif // wxUSE_CLIPBOARD || CAN_SAVE_FILES

    win->SetSizer(paneSz);
    paneSz->SetSizeHints(win);
#else // __SMARTPHONE__
    SetLeftMenu(wxID_OK);
    SetRightMenu(wxID_MORE, ms_details + EXPAND_SUFFIX);
#endif // __SMARTPHONE__/!__SMARTPHONE__

    SetSizerAndFit(sizerTop);

    Centre();

    if (isPda)
    {
        // Move up the screen so that when we expand the dialog,
        // there's enough space.
        Move(wxPoint(GetPosition().x, GetPosition().y / 2));
    }
}

void wxLogDialog::CreateDetailsControls(wxWindow *parent)
{
    wxString fmt = wxLog::GetTimestamp();
    bool hasTimeStamp = !fmt.IsEmpty();

    // create the list ctrl now
    m_listctrl = new wxListCtrl(parent, wxID_ANY,
                                wxDefaultPosition, wxDefaultSize,
                                wxBORDER_SIMPLE |
                                wxLC_REPORT |
                                wxLC_NO_HEADER |
                                wxLC_SINGLE_SEL);
#ifdef __WXWINCE__
    // This makes a big aesthetic difference on WinCE but I
    // don't want to risk problems on other platforms
    m_listctrl->Hide();
#endif

    // no need to translate these strings as they're not shown to the
    // user anyhow (we use wxLC_NO_HEADER style)
    m_listctrl->InsertColumn(0, wxT("Message"));

    if (hasTimeStamp)
        m_listctrl->InsertColumn(1, wxT("Time"));

    // prepare the imagelist
    static const int ICON_SIZE = 16;
    wxImageList *imageList = new wxImageList(ICON_SIZE, ICON_SIZE);

    // order should be the same as in the switch below!
    static const char* const icons[] =
    {
        wxART_ERROR,
        wxART_WARNING,
        wxART_INFORMATION
    };

    bool loadedIcons = true;

    for ( size_t icon = 0; icon < WXSIZEOF(icons); icon++ )
    {
        wxBitmap bmp = wxArtProvider::GetBitmap(icons[icon], wxART_MESSAGE_BOX,
                                                wxSize(ICON_SIZE, ICON_SIZE));

        // This may very well fail if there are insufficient colours available.
        // Degrade gracefully.
        if ( !bmp.IsOk() )
        {
            loadedIcons = false;

            break;
        }

        imageList->Add(bmp);
    }

    m_listctrl->SetImageList(imageList, wxIMAGE_LIST_SMALL);

    // fill the listctrl
    size_t count = m_messages.GetCount();
    for ( size_t n = 0; n < count; n++ )
    {
        int image;

        if ( loadedIcons )
        {
            switch ( m_severity[n] )
            {
                case wxLOG_Error:
                    image = 0;
                    break;

                case wxLOG_Warning:
                    image = 1;
                    break;

                default:
                    image = 2;
            }
        }
        else // failed to load images
        {
            image = -1;
        }

        wxString msg = m_messages[n];
        msg.Replace(wxT("\n"), wxT(" "));
        msg = EllipsizeString(msg);

        m_listctrl->InsertItem(n, msg, image);

        if (hasTimeStamp)
            m_listctrl->SetItem(n, 1, TimeStamp(fmt, (time_t)m_times[n]));
    }

    // let the columns size themselves
    m_listctrl->SetColumnWidth(0, wxLIST_AUTOSIZE);
    if (hasTimeStamp)
        m_listctrl->SetColumnWidth(1, wxLIST_AUTOSIZE);

    // calculate an approximately nice height for the listctrl
    int height = GetCharHeight()*(count + 4);

    // but check that the dialog won't fall fown from the screen
    //
    // we use GetMinHeight() to get the height of the dialog part without the
    // details and we consider that the "Save" button below and the separator
    // line (and the margins around it) take about as much, hence double it
    int heightMax = wxGetDisplaySize().y - GetPosition().y - 2*GetMinHeight();

    // we should leave a margin
    heightMax *= 9;
    heightMax /= 10;

    m_listctrl->SetSize(wxDefaultCoord, wxMin(height, heightMax));
}

void wxLogDialog::OnListItemActivated(wxListEvent& event)
{
    // show the activated item in a message box
    // This allow the user to correctly display the logs which are longer
    // than the listctrl and thus gets truncated or those which contains
    // newlines.

    // NB: don't do:
    //    wxString str = m_listctrl->GetItemText(event.GetIndex());
    // as there's a 260 chars limit on the items inside a wxListCtrl in wxMSW.
    wxString str = m_messages[event.GetIndex()];

    // wxMessageBox will nicely handle the '\n' in the string (if any)
    // and supports long strings
    wxMessageBox(str, wxT("Log message"), wxOK, this);
}

void wxLogDialog::OnOk(wxCommandEvent& WXUNUSED(event))
{
    EndModal(wxID_OK);
}

#if CAN_SAVE_FILES || wxUSE_CLIPBOARD

wxString wxLogDialog::GetLogMessages() const
{
    wxString fmt = wxLog::GetTimestamp();
    if ( fmt.empty() )
    {
        // use the default format
        fmt = "%c";
    }

    const size_t count = m_messages.GetCount();

    wxString text;
    text.reserve(count*m_messages[0].length());
    for ( size_t n = 0; n < count; n++ )
    {
        text << TimeStamp(fmt, (time_t)m_times[n])
             << ": "
             << m_messages[n]
             << wxTextFile::GetEOL();
    }

    return text;
}

#endif // CAN_SAVE_FILES || wxUSE_CLIPBOARD

#if wxUSE_CLIPBOARD

void wxLogDialog::OnCopy(wxCommandEvent& WXUNUSED(event))
{
    wxClipboardLocker clip;
    if ( !clip ||
            !wxTheClipboard->AddData(new wxTextDataObject(GetLogMessages())) )
    {
        wxLogError(_("Failed to copy dialog contents to the clipboard."));
    }
}

#endif // wxUSE_CLIPBOARD

#if CAN_SAVE_FILES

void wxLogDialog::OnSave(wxCommandEvent& WXUNUSED(event))
{
    wxFile file;
    int rc = OpenLogFile(file, NULL, this);
    if ( rc == -1 )
    {
        // cancelled
        return;
    }

    if ( !rc || !file.Write(GetLogMessages()) || !file.Close() )
    {
        wxLogError(_("Can't save log contents to file."));
    }
}

#endif // CAN_SAVE_FILES

wxLogDialog::~wxLogDialog()
{
    if ( m_listctrl )
    {
        delete m_listctrl->GetImageList(wxIMAGE_LIST_SMALL);
    }
}

#endif // wxUSE_LOG_DIALOG

#if CAN_SAVE_FILES

// pass an uninitialized file object, the function will ask the user for the
// filename and try to open it, returns true on success (file was opened),
// false if file couldn't be opened/created and -1 if the file selection
// dialog was cancelled
static int OpenLogFile(wxFile& file, wxString *pFilename, wxWindow *parent)
{
    // get the file name
    // -----------------
    wxString filename = wxSaveFileSelector(wxT("log"), wxT("txt"), wxT("log.txt"), parent);
    if ( !filename ) {
        // cancelled
        return -1;
    }

    // open file
    // ---------
    bool bOk = true; // suppress warning about it being possible uninitialized
    if ( wxFile::Exists(filename) ) {
        bool bAppend = false;
        wxString strMsg;
        strMsg.Printf(_("Append log to file '%s' (choosing [No] will overwrite it)?"),
                      filename.c_str());
        switch ( wxMessageBox(strMsg, _("Question"),
                              wxICON_QUESTION | wxYES_NO | wxCANCEL) ) {
            case wxYES:
                bAppend = true;
                break;

            case wxNO:
                bAppend = false;
                break;

            case wxCANCEL:
                return -1;

            default:
                wxFAIL_MSG(_("invalid message box return value"));
        }

        if ( bAppend ) {
            bOk = file.Open(filename, wxFile::write_append);
        }
        else {
            bOk = file.Create(filename, true /* overwrite */);
        }
    }
    else {
        bOk = file.Create(filename);
    }

    if ( pFilename )
        *pFilename = filename;

    return bOk;
}

#endif // CAN_SAVE_FILES

#endif // !(wxUSE_LOGGUI || wxUSE_LOGWINDOW)

#if wxUSE_LOG && wxUSE_TEXTCTRL

// ----------------------------------------------------------------------------
// wxLogTextCtrl implementation
// ----------------------------------------------------------------------------

wxLogTextCtrl::wxLogTextCtrl(wxTextCtrl *pTextCtrl)
{
    m_pTextCtrl = pTextCtrl;
}

void wxLogTextCtrl::DoLogText(const wxString& msg)
{
    m_pTextCtrl->AppendText(msg + wxS('\n'));
}

#endif // wxUSE_LOG && wxUSE_TEXTCTRL
