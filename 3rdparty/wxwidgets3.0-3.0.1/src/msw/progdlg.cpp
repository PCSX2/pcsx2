/////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/progdlg.cpp
// Purpose:     wxProgressDialog
// Author:      Rickard Westerlund
// Created:     2010-07-22
// Copyright:   (c) 2010 wxWidgets team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// Declarations
// ============================================================================

// ----------------------------------------------------------------------------
// Headers
// ----------------------------------------------------------------------------

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_PROGRESSDLG && wxUSE_THREADS

#include "wx/progdlg.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/msgdlg.h"
    #include "wx/stopwatch.h"
    #include "wx/msw/private.h"
#endif

#include "wx/msw/private/msgdlg.h"
#include "wx/evtloop.h"

using namespace wxMSWMessageDialog;

#ifdef wxHAS_MSW_TASKDIALOG

// ----------------------------------------------------------------------------
// Constants
// ----------------------------------------------------------------------------

namespace
{

// Notification values of wxProgressDialogSharedData::m_notifications
const int wxSPDD_VALUE_CHANGED     = 0x0001;
const int wxSPDD_RANGE_CHANGED     = 0x0002;
const int wxSPDD_PBMARQUEE_CHANGED = 0x0004;
const int wxSPDD_TITLE_CHANGED     = 0x0008;
const int wxSPDD_MESSAGE_CHANGED   = 0x0010;
const int wxSPDD_EXPINFO_CHANGED   = 0x0020;
const int wxSPDD_ENABLE_SKIP       = 0x0040;
const int wxSPDD_ENABLE_ABORT      = 0x0080;
const int wxSPDD_DISABLE_SKIP      = 0x0100;
const int wxSPDD_DISABLE_ABORT     = 0x0200;
const int wxSPDD_FINISHED          = 0x0400;
const int wxSPDD_DESTROYED         = 0x0800;

const int Id_SkipBtn = wxID_HIGHEST + 1;

} // anonymous namespace

// ============================================================================
// Helper classes
// ============================================================================

// Class used to share data between the main thread and the task dialog runner.
class wxProgressDialogSharedData
{
public:
    wxProgressDialogSharedData()
    {
        m_hwnd = 0;
        m_value = 0;
        m_progressBarMarquee = false;
        m_skipped = false;
        m_notifications = 0;
        m_parent = NULL;
    }

    wxCriticalSection m_cs;

    wxWindow *m_parent;     // Parent window only used to center us over it.
    HWND m_hwnd;            // Task dialog handler
    long m_style;           // wxProgressDialog style
    int m_value;
    int m_range;
    wxString m_title;
    wxString m_message;
    wxString m_expandedInformation;
    wxString m_labelCancel; // Privately used by callback.
    unsigned long m_timeStop;

    wxProgressDialog::State m_state;
    bool m_progressBarMarquee;
    bool m_skipped;

    // Bit field that indicates fields that have been modified by the
    // main thread so the task dialog runner knows what to update.
    int m_notifications;
};

// Runner thread that takes care of displaying and updating the
// task dialog.
class wxProgressDialogTaskRunner : public wxThread
{
public:
    wxProgressDialogTaskRunner()
        : wxThread(wxTHREAD_JOINABLE)
        { }

    wxProgressDialogSharedData* GetSharedDataObject()
        { return &m_sharedData; }

private:
    wxProgressDialogSharedData m_sharedData;

    virtual void* Entry();

    static HRESULT CALLBACK TaskDialogCallbackProc(HWND hwnd,
                                                   UINT uNotification,
                                                   WPARAM wParam,
                                                   LPARAM lParam,
                                                   LONG_PTR dwRefData);
};

namespace
{

// A custom event loop which runs until the state of the dialog becomes
// "Dismissed".
class wxProgressDialogModalLoop : public wxEventLoop
{
public:
    wxProgressDialogModalLoop(wxProgressDialogSharedData& data)
        : m_data(data)
    {
    }

protected:
    virtual void OnNextIteration()
    {
        wxCriticalSectionLocker locker(m_data.m_cs);

        if ( m_data.m_state == wxProgressDialog::Dismissed )
            Exit();
    }

    wxProgressDialogSharedData& m_data;

    wxDECLARE_NO_COPY_CLASS(wxProgressDialogModalLoop);
};

// ============================================================================
// Helper functions
// ============================================================================

BOOL CALLBACK DisplayCloseButton(HWND hwnd, LPARAM lParam)
{
    wxProgressDialogSharedData *sharedData =
        (wxProgressDialogSharedData *) lParam;

    if ( wxGetWindowText( hwnd ) == sharedData->m_labelCancel )
    {
        sharedData->m_labelCancel = _("Close");
        SendMessage( hwnd, WM_SETTEXT, 0,
                     wxMSW_CONV_LPARAM(sharedData->m_labelCancel) );

        return FALSE;
    }

    return TRUE;
}

void PerformNotificationUpdates(HWND hwnd,
                                wxProgressDialogSharedData *sharedData)
{
    // Update the appropriate dialog fields.
    if ( sharedData->m_notifications & wxSPDD_RANGE_CHANGED )
    {
        ::SendMessage( hwnd,
                       TDM_SET_PROGRESS_BAR_RANGE,
                       0,
                       MAKELPARAM(0, sharedData->m_range) );
    }

    if ( sharedData->m_notifications & wxSPDD_VALUE_CHANGED )
    {
        ::SendMessage( hwnd,
                       TDM_SET_PROGRESS_BAR_POS,
                       sharedData->m_value,
                       0 );
    }

    if ( sharedData->m_notifications & wxSPDD_PBMARQUEE_CHANGED )
    {
        BOOL val = sharedData->m_progressBarMarquee ? TRUE : FALSE;
        ::SendMessage( hwnd,
                       TDM_SET_MARQUEE_PROGRESS_BAR,
                       val,
                       0 );
        ::SendMessage( hwnd,
                       TDM_SET_PROGRESS_BAR_MARQUEE,
                       val,
                       0 );
    }

    if ( sharedData->m_notifications & wxSPDD_TITLE_CHANGED )
        ::SetWindowText( hwnd, sharedData->m_title.t_str() );

    if ( sharedData->m_notifications & wxSPDD_MESSAGE_CHANGED )
    {
        // Split the message in the title string and the rest if it has
        // multiple lines.
        wxString
            title = sharedData->m_message,
            body;

        const size_t posNL = title.find('\n');
        if ( posNL != wxString::npos )
        {
            // There can an extra new line between the first and subsequent
            // lines to separate them as it looks better with the generic
            // version -- but in this one, they're already separated by the use
            // of different dialog elements, so suppress the extra new line.
            int numNLs = 1;
            if ( posNL < title.length() - 1 && title[posNL + 1] == '\n' )
                numNLs++;

            body.assign(title, posNL + numNLs, wxString::npos);
            title.erase(posNL);
        }
        else // A single line
        {
            // Don't use title without the body, this doesn't make sense.
            title.swap(body);
        }

        ::SendMessage( hwnd,
                       TDM_SET_ELEMENT_TEXT,
                       TDE_MAIN_INSTRUCTION,
                       wxMSW_CONV_LPARAM(title) );

        ::SendMessage( hwnd,
                       TDM_SET_ELEMENT_TEXT,
                       TDE_CONTENT,
                       wxMSW_CONV_LPARAM(body) );
    }

    if ( sharedData->m_notifications & wxSPDD_EXPINFO_CHANGED )
    {
        const wxString& expandedInformation =
            sharedData->m_expandedInformation;
        if ( !expandedInformation.empty() )
        {
            ::SendMessage( hwnd,
                           TDM_SET_ELEMENT_TEXT,
                           TDE_EXPANDED_INFORMATION,
                           wxMSW_CONV_LPARAM(expandedInformation) );
        }
    }

    if ( sharedData->m_notifications & wxSPDD_ENABLE_SKIP )
        ::SendMessage( hwnd, TDM_ENABLE_BUTTON, Id_SkipBtn, TRUE );

    if ( sharedData->m_notifications & wxSPDD_ENABLE_ABORT )
        ::SendMessage( hwnd, TDM_ENABLE_BUTTON, IDCANCEL, TRUE );

    if ( sharedData->m_notifications & wxSPDD_DISABLE_SKIP )
        ::SendMessage( hwnd, TDM_ENABLE_BUTTON, Id_SkipBtn, FALSE );

    if ( sharedData->m_notifications & wxSPDD_DISABLE_ABORT )
        ::SendMessage( hwnd, TDM_ENABLE_BUTTON, IDCANCEL, FALSE );

    // Is the progress finished?
    if ( sharedData->m_notifications & wxSPDD_FINISHED )
    {
        sharedData->m_state = wxProgressDialog::Finished;

        if ( !(sharedData->m_style & wxPD_AUTO_HIDE) )
        {
            // Change Cancel into Close and activate the button.
            ::SendMessage( hwnd, TDM_ENABLE_BUTTON, Id_SkipBtn, FALSE );
            ::SendMessage( hwnd, TDM_ENABLE_BUTTON, IDCANCEL, TRUE );
            ::EnumChildWindows( hwnd, DisplayCloseButton,
                                (LPARAM) sharedData );
        }
    }
}

} // anonymous namespace

#endif // wxHAS_MSW_TASKDIALOG

// ============================================================================
// wxProgressDialog implementation
// ============================================================================

wxProgressDialog::wxProgressDialog( const wxString& title,
                                    const wxString& message,
                                    int maximum,
                                    wxWindow *parent,
                                    int style )
    : wxGenericProgressDialog(),
      m_taskDialogRunner(NULL),
      m_sharedData(NULL),
      m_message(message),
      m_title(title)
{
#ifdef wxHAS_MSW_TASKDIALOG
    if ( HasNativeTaskDialog() )
    {
        SetTopParent(parent);
        SetPDStyle(style);
        SetMaximum(maximum);

        Show();
        DisableOtherWindows();

        return;
    }
#endif // wxHAS_MSW_TASKDIALOG

    Create(title, message, maximum, parent, style);
}

wxProgressDialog::~wxProgressDialog()
{
#ifdef wxHAS_MSW_TASKDIALOG
    if ( !m_taskDialogRunner )
        return;

    if ( m_sharedData )
    {
        wxCriticalSectionLocker locker(m_sharedData->m_cs);
        m_sharedData->m_notifications |= wxSPDD_DESTROYED;
    }

    m_taskDialogRunner->Wait();

    delete m_taskDialogRunner;

    ReenableOtherWindows();

    if ( GetTopParent() )
        GetTopParent()->Raise();
#endif // wxHAS_MSW_TASKDIALOG
}

bool wxProgressDialog::Update(int value, const wxString& newmsg, bool *skip)
{
#ifdef wxHAS_MSW_TASKDIALOG
    if ( HasNativeTaskDialog() )
    {
        {
            wxCriticalSectionLocker locker(m_sharedData->m_cs);

            // Do nothing in canceled state.
            if ( !DoNativeBeforeUpdate(skip) )
                return false;

            value /= m_factor;

            wxASSERT_MSG( value <= m_maximum, wxT("invalid progress value") );

            m_sharedData->m_value = value;
            m_sharedData->m_notifications |= wxSPDD_VALUE_CHANGED;

            if ( !newmsg.empty() )
            {
                m_message = newmsg;
                m_sharedData->m_message = newmsg;
                m_sharedData->m_notifications |= wxSPDD_MESSAGE_CHANGED;
            }

            if ( m_sharedData->m_progressBarMarquee )
            {
                m_sharedData->m_progressBarMarquee = false;
                m_sharedData->m_notifications |= wxSPDD_PBMARQUEE_CHANGED;
            }

            UpdateExpandedInformation( value );

            // If we didn't just reach the finish, all we have to do is to
            // return true if the dialog wasn't cancelled and false otherwise.
            if ( value != m_maximum || m_state == Finished )
                return m_sharedData->m_state != Canceled;


            // On finishing, the dialog without wxPD_AUTO_HIDE style becomes a
            // modal one meaning that we must block here until the user
            // dismisses it.
            m_state = Finished;
            m_sharedData->m_state = Finished;
            m_sharedData->m_notifications |= wxSPDD_FINISHED;
            if ( HasPDFlag(wxPD_AUTO_HIDE) )
                return true;

            if ( newmsg.empty() )
            {
                // Provide the finishing message if the application didn't.
                m_message = _("Done.");
                m_sharedData->m_message = m_message;
                m_sharedData->m_notifications |= wxSPDD_MESSAGE_CHANGED;
            }
        } // unlock m_sharedData->m_cs

        // We only get here when we need to wait for the dialog to terminate so
        // do just this by running a custom event loop until the dialog is
        // dismissed.
        wxProgressDialogModalLoop loop(*m_sharedData);
        loop.Run();
        return true;
    }
#endif // wxHAS_MSW_TASKDIALOG

    return wxGenericProgressDialog::Update( value, newmsg, skip );
}

bool wxProgressDialog::Pulse(const wxString& newmsg, bool *skip)
{
#ifdef wxHAS_MSW_TASKDIALOG
    if ( HasNativeTaskDialog() )
    {
        wxCriticalSectionLocker locker(m_sharedData->m_cs);

        // Do nothing in canceled state.
        if ( !DoNativeBeforeUpdate(skip) )
            return false;

        if ( !m_sharedData->m_progressBarMarquee )
        {
            m_sharedData->m_progressBarMarquee = true;
            m_sharedData->m_notifications |= wxSPDD_PBMARQUEE_CHANGED;
        }

        if ( !newmsg.empty() )
        {
            m_message = newmsg;
            m_sharedData->m_message = newmsg;
            m_sharedData->m_notifications |= wxSPDD_MESSAGE_CHANGED;
        }

        // The value passed here doesn't matter, only elapsed time makes sense
        // in indeterminate mode anyhow.
        UpdateExpandedInformation(0);

        return m_sharedData->m_state != Canceled;
    }
#endif // wxHAS_MSW_TASKDIALOG

    return wxGenericProgressDialog::Pulse( newmsg, skip );
}

bool wxProgressDialog::DoNativeBeforeUpdate(bool *skip)
{
#ifdef wxHAS_MSW_TASKDIALOG
    if ( HasNativeTaskDialog() )
    {
        if ( m_sharedData->m_skipped  )
        {
            if ( skip && !*skip )
            {
                *skip = true;
                m_sharedData->m_skipped = false;
                m_sharedData->m_notifications |= wxSPDD_ENABLE_SKIP;
            }
        }

        if ( m_sharedData->m_state == Canceled )
            m_timeStop = m_sharedData->m_timeStop;

        return m_sharedData->m_state != Canceled;
    }
#endif // wxHAS_MSW_TASKDIALOG

    wxUnusedVar(skip);
    wxFAIL_MSG( "unreachable" );

    return false;
}

void wxProgressDialog::Resume()
{
    wxGenericProgressDialog::Resume();

#ifdef wxHAS_MSW_TASKDIALOG
    if ( HasNativeTaskDialog() )
    {
        HWND hwnd;

        {
            wxCriticalSectionLocker locker(m_sharedData->m_cs);
            m_sharedData->m_state = m_state;

            // "Skip" was disabled when "Cancel" had been clicked, so re-enable
            // it now.
            m_sharedData->m_notifications |= wxSPDD_ENABLE_SKIP;

            // Also re-enable "Cancel" itself
            if ( HasPDFlag(wxPD_CAN_ABORT) )
                m_sharedData->m_notifications |= wxSPDD_ENABLE_ABORT;

            hwnd = m_sharedData->m_hwnd;
        } // Unlock m_cs, we can't call any function operating on a dialog with
          // it locked as it can result in a deadlock if the dialog callback is
          // called by Windows.

        // After resuming we need to bring the window on top of the Z-order as
        // it could be hidden by another window shown from the main thread,
        // e.g. a confirmation dialog asking whether the user really wants to
        // abort.
        //
        // Notice that this must be done from the main thread as it owns the
        // currently active window and attempts to do this from the task dialog
        // thread would simply fail.
        ::BringWindowToTop(hwnd);
    }
#endif // wxHAS_MSW_TASKDIALOG
}

WXWidget wxProgressDialog::GetHandle() const 
{ 
#ifdef wxHAS_MSW_TASKDIALOG
    if ( HasNativeTaskDialog() )
    {
        HWND hwnd;
        {
            wxCriticalSectionLocker locker(m_sharedData->m_cs);
            m_sharedData->m_state = m_state;
            hwnd = m_sharedData->m_hwnd;
        }
        return hwnd;
    }
#endif
    return wxGenericProgressDialog::GetHandle();
}

int wxProgressDialog::GetValue() const
{
#ifdef wxHAS_MSW_TASKDIALOG
    if ( HasNativeTaskDialog() )
    {
        wxCriticalSectionLocker locker(m_sharedData->m_cs);
        return m_sharedData->m_value;
    }
#endif // wxHAS_MSW_TASKDIALOG

    return wxGenericProgressDialog::GetValue();
}

wxString wxProgressDialog::GetMessage() const
{
#ifdef wxHAS_MSW_TASKDIALOG
    if ( HasNativeTaskDialog() )
        return m_message;
#endif // wxHAS_MSW_TASKDIALOG

    return wxGenericProgressDialog::GetMessage();
}

void wxProgressDialog::SetRange(int maximum)
{
#ifdef wxHAS_MSW_TASKDIALOG
    if ( HasNativeTaskDialog() )
    {
        SetMaximum(maximum);

        wxCriticalSectionLocker locker(m_sharedData->m_cs);

        m_sharedData->m_range = maximum;
        m_sharedData->m_notifications |= wxSPDD_RANGE_CHANGED;

        return;
    }
#endif // wxHAS_MSW_TASKDIALOG

    wxGenericProgressDialog::SetRange( maximum );
}

bool wxProgressDialog::WasSkipped() const
{
#ifdef wxHAS_MSW_TASKDIALOG
    if ( HasNativeTaskDialog() )
    {
        if ( !m_sharedData )
        {
            // Couldn't be skipped before being shown.
            return false;
        }

        wxCriticalSectionLocker locker(m_sharedData->m_cs);
        return m_sharedData->m_skipped;
    }
#endif // wxHAS_MSW_TASKDIALOG

    return wxGenericProgressDialog::WasSkipped();
}

bool wxProgressDialog::WasCancelled() const
{
#ifdef wxHAS_MSW_TASKDIALOG
    if ( HasNativeTaskDialog() )
    {
        wxCriticalSectionLocker locker(m_sharedData->m_cs);
        return m_sharedData->m_state == Canceled;
    }
#endif // wxHAS_MSW_TASKDIALOG

    return wxGenericProgressDialog::WasCancelled();
}

void wxProgressDialog::SetTitle(const wxString& title)
{
#ifdef wxHAS_MSW_TASKDIALOG
    if ( HasNativeTaskDialog() )
    {
        m_title = title;

        if ( m_sharedData )
        {
            wxCriticalSectionLocker locker(m_sharedData->m_cs);
            m_sharedData->m_title = title;
            m_sharedData->m_notifications = wxSPDD_TITLE_CHANGED;
        }
    }
#endif // wxHAS_MSW_TASKDIALOG

    wxGenericProgressDialog::SetTitle(title);
}

wxString wxProgressDialog::GetTitle() const
{
#ifdef wxHAS_MSW_TASKDIALOG
    if ( HasNativeTaskDialog() )
        return m_title;
#endif // wxHAS_MSW_TASKDIALOG

    return wxGenericProgressDialog::GetTitle();
}

bool wxProgressDialog::Show(bool show)
{
#ifdef wxHAS_MSW_TASKDIALOG
    if ( HasNativeTaskDialog() )
    {
        // The dialog can't be hidden at all and showing it again after it had
        // been shown before doesn't do anything.
        if ( !show || m_taskDialogRunner )
            return false;

        // We're showing the dialog for the first time, create the thread that
        // will manage it.
        m_taskDialogRunner = new wxProgressDialogTaskRunner;
        m_sharedData = m_taskDialogRunner->GetSharedDataObject();

        // Initialize shared data.
        m_sharedData->m_title = m_title;
        m_sharedData->m_message = m_message;
        m_sharedData->m_range = m_maximum;
        m_sharedData->m_state = Uncancelable;
        m_sharedData->m_style = GetPDStyle();
        m_sharedData->m_parent = GetTopParent();

        if ( HasPDFlag(wxPD_CAN_ABORT) )
        {
            m_sharedData->m_state = Continue;
            m_sharedData->m_labelCancel = _("Cancel");
        }
        else // Dialog can't be cancelled.
        {
            // We still must have at least a single button in the dialog so
            // just don't call it "Cancel" in this case.
            m_sharedData->m_labelCancel = _("Close");
        }

        if ( HasPDFlag(wxPD_ELAPSED_TIME |
                         wxPD_ESTIMATED_TIME |
                            wxPD_REMAINING_TIME) )
        {
            // Use a non-empty string just to have the collapsible pane shown.
            m_sharedData->m_expandedInformation = " ";
        }

        // Do launch the thread.
        if ( m_taskDialogRunner->Create() != wxTHREAD_NO_ERROR )
        {
            wxLogError( "Unable to create thread!" );
            return false;
        }

        if ( m_taskDialogRunner->Run() != wxTHREAD_NO_ERROR )
        {
            wxLogError( "Unable to start thread!" );
            return false;
        }

        // Do not show the underlying dialog.
        return false;
    }
#endif // wxHAS_MSW_TASKDIALOG

    return wxGenericProgressDialog::Show( show );
}

void wxProgressDialog::UpdateExpandedInformation(int value)
{
#ifdef wxHAS_MSW_TASKDIALOG
    unsigned long elapsedTime;
    unsigned long estimatedTime;
    unsigned long remainingTime;
    UpdateTimeEstimates(value, elapsedTime, estimatedTime, remainingTime);

    int realEstimatedTime = estimatedTime,
        realRemainingTime = remainingTime;
    if ( m_sharedData->m_progressBarMarquee )
    {
        // In indeterminate mode we don't have any estimation neither for the
        // remaining nor for estimated time.
        realEstimatedTime =
        realRemainingTime = -1;
    }

    wxString expandedInformation;

    // Calculate the three different timing values.
    if ( HasPDFlag(wxPD_ELAPSED_TIME) )
    {
        expandedInformation << GetElapsedLabel()
                            << " "
                            << GetFormattedTime(elapsedTime);
    }

    if ( HasPDFlag(wxPD_ESTIMATED_TIME) )
    {
        if ( !expandedInformation.empty() )
            expandedInformation += "\n";

        expandedInformation << GetEstimatedLabel()
                            << " "
                            << GetFormattedTime(realEstimatedTime);
    }

    if ( HasPDFlag(wxPD_REMAINING_TIME) )
    {
        if ( !expandedInformation.empty() )
            expandedInformation += "\n";

        expandedInformation << GetRemainingLabel()
                            << " "
                            << GetFormattedTime(realRemainingTime);
    }

    // Update with new timing information.
    if ( expandedInformation != m_sharedData->m_expandedInformation )
    {
        m_sharedData->m_expandedInformation = expandedInformation;
        m_sharedData->m_notifications |= wxSPDD_EXPINFO_CHANGED;
    }
#else // !wxHAS_MSW_TASKDIALOG
    wxUnusedVar(value);
#endif // wxHAS_MSW_TASKDIALOG/!wxHAS_MSW_TASKDIALOG
}

// ----------------------------------------------------------------------------
// wxProgressDialogTaskRunner and related methods
// ----------------------------------------------------------------------------

#ifdef wxHAS_MSW_TASKDIALOG

void* wxProgressDialogTaskRunner::Entry()
{
    WinStruct<TASKDIALOGCONFIG> tdc;
    wxMSWTaskDialogConfig wxTdc;

    {
        wxCriticalSectionLocker locker(m_sharedData.m_cs);

        wxTdc.caption = m_sharedData.m_title.wx_str();
        wxTdc.message = m_sharedData.m_message.wx_str();

        // MSWCommonTaskDialogInit() will add an IDCANCEL button but we need to
        // give it the correct label.
        wxTdc.btnOKLabel = m_sharedData.m_labelCancel;
        wxTdc.useCustomLabels = true;

        wxTdc.MSWCommonTaskDialogInit( tdc );
        tdc.pfCallback = TaskDialogCallbackProc;
        tdc.lpCallbackData = (LONG_PTR) &m_sharedData;

        // Undo some of the effects of MSWCommonTaskDialogInit().
        tdc.dwFlags &= ~TDF_EXPAND_FOOTER_AREA; // Expand in content area.
        tdc.dwCommonButtons = 0; // Don't use common buttons.

        if ( m_sharedData.m_style & wxPD_CAN_SKIP )
            wxTdc.AddTaskDialogButton( tdc, Id_SkipBtn, 0, _("Skip") );

        tdc.dwFlags |= TDF_CALLBACK_TIMER | TDF_SHOW_PROGRESS_BAR;

        if ( !m_sharedData.m_expandedInformation.empty() )
        {
            tdc.pszExpandedInformation =
                m_sharedData.m_expandedInformation.t_str();
        }
    }

    TaskDialogIndirect_t taskDialogIndirect = GetTaskDialogIndirectFunc();
    if ( !taskDialogIndirect )
        return NULL;

    int msAns;
    HRESULT hr = taskDialogIndirect(&tdc, &msAns, NULL, NULL);
    if ( FAILED(hr) )
        wxLogApiError( "TaskDialogIndirect", hr );

    // If the main thread is waiting for us to exit inside the event loop in
    // Update(), wake it up so that it checks our status again.
    wxWakeUpIdle();

    return NULL;
}

// static
HRESULT CALLBACK
wxProgressDialogTaskRunner::TaskDialogCallbackProc
                            (
                                HWND hwnd,
                                UINT uNotification,
                                WPARAM wParam,
                                LPARAM WXUNUSED(lParam),
                                LONG_PTR dwRefData
                            )
{
    wxProgressDialogSharedData * const sharedData =
        (wxProgressDialogSharedData *) dwRefData;

    wxCriticalSectionLocker locker(sharedData->m_cs);

    switch ( uNotification )
    {
        case TDN_CREATED:
            // Store the HWND for the main thread use.
            sharedData->m_hwnd = hwnd;

            // Set the maximum value and disable Close button.
            ::SendMessage( hwnd,
                           TDM_SET_PROGRESS_BAR_RANGE,
                           0,
                           MAKELPARAM(0, sharedData->m_range) );

            // We always create this task dialog with NULL parent because our
            // parent in wx sense is a window created from a different thread
            // and so can't be used as our real parent. However we still center
            // this window on the parent one as the task dialogs do with their
            // real parent usually.
            if ( sharedData->m_parent )
            {
                wxRect rect(wxRectFromRECT(wxGetWindowRect(hwnd)));
                rect = rect.CentreIn(sharedData->m_parent->GetRect());
                ::SetWindowPos(hwnd,
                               NULL,
                               rect.x,
                               rect.y,
                               -1,
                               -1,
                               SWP_NOACTIVATE |
                               SWP_NOOWNERZORDER |
                               SWP_NOSIZE |
                               SWP_NOZORDER);
            }

            // If we can't be aborted, the "Close" button will only be enabled
            // when the progress ends (and not even then with wxPD_AUTO_HIDE).
            if ( !(sharedData->m_style & wxPD_CAN_ABORT) )
                ::SendMessage( hwnd, TDM_ENABLE_BUTTON, IDCANCEL, FALSE );
            break;

        case TDN_BUTTON_CLICKED:
            switch ( wParam )
            {
                case Id_SkipBtn:
                    ::SendMessage(hwnd, TDM_ENABLE_BUTTON, Id_SkipBtn, FALSE);
                    sharedData->m_skipped = true;
                    return TRUE;

                case IDCANCEL:
                    if ( sharedData->m_state == wxProgressDialog::Finished )
                    {
                        // If the main thread is waiting for us, tell it that
                        // we're gone (and if it doesn't wait, it's harmless).
                        sharedData->m_state = wxProgressDialog::Dismissed;

                        // Let Windows close the dialog.
                        return FALSE;
                    }

                    // Close button on the window triggers an IDCANCEL press,
                    // don't allow it when it should only be possible to close
                    // a finished dialog.
                    if ( sharedData->m_style & wxPD_CAN_ABORT )
                    {
                        wxCHECK_MSG
                        (
                            sharedData->m_state == wxProgressDialog::Continue,
                            TRUE,
                            "Dialog not in a cancelable state!"
                        );

                        ::SendMessage(hwnd, TDM_ENABLE_BUTTON, Id_SkipBtn, FALSE);
                        ::SendMessage(hwnd, TDM_ENABLE_BUTTON, IDCANCEL, FALSE);

                        sharedData->m_timeStop = wxGetCurrentTime();
                        sharedData->m_state = wxProgressDialog::Canceled;
                    }

                    return TRUE;
            }
            break;

        case TDN_TIMER:
            PerformNotificationUpdates(hwnd, sharedData);

            /*
                Decide whether we should end the dialog. This is done if either
                the dialog object itself was destroyed or if the progress
                finished and we were configured to hide automatically without
                waiting for the user to dismiss us.

                Notice that we do not close the dialog if it was cancelled
                because it's up to the user code in the main thread to decide
                whether it really wants to cancel the dialog.
             */
            if ( (sharedData->m_notifications & wxSPDD_DESTROYED) ||
                    (sharedData->m_state == wxProgressDialog::Finished &&
                        sharedData->m_style & wxPD_AUTO_HIDE) )
            {
                ::EndDialog( hwnd, IDCLOSE );
            }

            sharedData->m_notifications = 0;

            return TRUE;
    }

    // Return anything.
    return 0;
}

#endif // wxHAS_MSW_TASKDIALOG

#endif // wxUSE_PROGRESSDLG && wxUSE_THREADS
