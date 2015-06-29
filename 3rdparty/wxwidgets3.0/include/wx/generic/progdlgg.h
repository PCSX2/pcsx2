///////////////////////////////////////////////////////////////////////////////
// Name:        wx/generic/progdlgg.h
// Purpose:     wxGenericProgressDialog class
// Author:      Karsten Ballueder
// Modified by: Francesco Montorsi
// Created:     09.05.1999
// Copyright:   (c) Karsten Ballueder
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef __PROGDLGH_G__
#define __PROGDLGH_G__

#include "wx/dialog.h"

class WXDLLIMPEXP_FWD_CORE wxButton;
class WXDLLIMPEXP_FWD_CORE wxEventLoop;
class WXDLLIMPEXP_FWD_CORE wxGauge;
class WXDLLIMPEXP_FWD_CORE wxStaticText;
class WXDLLIMPEXP_FWD_CORE wxWindowDisabler;

/*
    Progress dialog which shows a moving progress bar.
    Taken from the Mahogany project.
*/
class WXDLLIMPEXP_CORE wxGenericProgressDialog : public wxDialog
{
public:
    wxGenericProgressDialog();
    wxGenericProgressDialog(const wxString& title, const wxString& message,
                            int maximum = 100,
                            wxWindow *parent = NULL,
                            int style = wxPD_APP_MODAL | wxPD_AUTO_HIDE);

    virtual ~wxGenericProgressDialog();

    bool Create(const wxString& title,
                const wxString& message,
                int maximum = 100,
                wxWindow *parent = NULL,
                int style = wxPD_APP_MODAL | wxPD_AUTO_HIDE);

    virtual bool Update(int value, const wxString& newmsg = wxEmptyString, bool *skip = NULL);
    virtual bool Pulse(const wxString& newmsg = wxEmptyString, bool *skip = NULL);

    void Resume();

    int GetValue() const;
    int GetRange() const;
    wxString GetMessage() const;

    void SetRange(int maximum);

    // Return whether "Cancel" or "Skip" button was pressed, always return
    // false if the corresponding button is not shown.
    bool WasCancelled() const;
    bool WasSkipped() const;

    // Must provide overload to avoid hiding it (and warnings about it)
    virtual void Update() { wxDialog::Update(); }

    virtual bool Show( bool show = true );

    // This enum is an implementation detail and should not be used
    // by user code.
    enum State
    {
        Uncancelable = -1,   // dialog can't be canceled
        Canceled,            // can be cancelled and, in fact, was
        Continue,            // can be cancelled but wasn't
        Finished,            // finished, waiting to be removed from screen
        Dismissed            // was closed by user after finishing
    };

protected:
    // Update just the m_maximum field, this is used by public SetRange() but,
    // unlike it, doesn't update the controls state. This makes it useful for
    // both this class and its derived classes that don't use m_gauge to
    // display progress.
    void SetMaximum(int maximum);

    // Return the labels to use for showing the elapsed/estimated/remaining
    // times respectively.
    static wxString GetElapsedLabel() { return wxGetTranslation("Elapsed time:"); }
    static wxString GetEstimatedLabel() { return wxGetTranslation("Estimated time:"); }
    static wxString GetRemainingLabel() { return wxGetTranslation("Remaining time:"); }


    // Similar to wxWindow::HasFlag() but tests for a presence of a wxPD_XXX
    // flag in our (separate) flags instead of using m_windowStyle.
    bool HasPDFlag(int flag) const { return (m_pdStyle & flag) != 0; }

    // Return the progress dialog style. Prefer to use HasPDFlag() if possible.
    int GetPDStyle() const { return m_pdStyle; }
    void SetPDStyle(int pdStyle) { m_pdStyle = pdStyle; }

    // Updates estimated times from a given progress bar value and stores the
    // results in provided arguments.
    void UpdateTimeEstimates(int value,
                             unsigned long &elapsedTime,
                             unsigned long &estimatedTime,
                             unsigned long &remainingTime);

    // Converts seconds to HH:mm:ss format.
    static wxString GetFormattedTime(unsigned long timeInSec);

    // callback for optional abort button
    void OnCancel(wxCommandEvent&);

    // callback for optional skip button
    void OnSkip(wxCommandEvent&);

    // callback to disable "hard" window closing
    void OnClose(wxCloseEvent&);

    // called to disable the other windows while this dialog is shown
    void DisableOtherWindows();

    // must be called to reenable the other windows temporarily disabled while
    // the dialog was shown
    void ReenableOtherWindows();

    // Set the top level parent we store from the parent window provided when
    // creating the dialog.
    void SetTopParent(wxWindow* parent);

    // return the top level parent window of this dialog (may be NULL)
    wxWindow *GetTopParent() const { return m_parentTop; }


    // continue processing or not (return value for Update())
    State m_state;

    // the maximum value
    int m_maximum;

#if defined(__WXMSW__ ) || defined(__WXPM__)
    // the factor we use to always keep the value in 16 bit range as the native
    // control only supports ranges from 0 to 65,535
    size_t m_factor;
#endif // __WXMSW__

    // time when the dialog was created
    unsigned long m_timeStart;
    // time when the dialog was closed or cancelled
    unsigned long m_timeStop;
    // time between the moment the dialog was closed/cancelled and resume
    unsigned long m_break;

private:
    // update the label to show the given time (in seconds)
    static void SetTimeLabel(unsigned long val, wxStaticText *label);

    // common part of all ctors
    void Init();

    // create the label with given text and another one to show the time nearby
    // as the next windows in the sizer, returns the created control
    wxStaticText *CreateLabel(const wxString& text, wxSizer *sizer);

    // updates the label message
    void UpdateMessage(const wxString &newmsg);

    // common part of Update() and Pulse(), returns true if not cancelled
    bool DoBeforeUpdate(bool *skip);

    // common part of Update() and Pulse()
    void DoAfterUpdate();

    // shortcuts for enabling buttons
    void EnableClose();
    void EnableSkip(bool enable = true);
    void EnableAbort(bool enable = true);
    void DisableSkip() { EnableSkip(false); }
    void DisableAbort() { EnableAbort(false); }

    // the widget displaying current status (may be NULL)
    wxGauge *m_gauge;
    // the message displayed
    wxStaticText *m_msg;
    // displayed elapsed, estimated, remaining time
    wxStaticText *m_elapsed,
                 *m_estimated,
                 *m_remaining;

    // parent top level window (may be NULL)
    wxWindow *m_parentTop;

    // Progress dialog styles: this is not the same as m_windowStyle because
    // wxPD_XXX constants clash with the existing TLW styles so to be sure we
    // don't have any conflicts we just use a separate variable for storing
    // them.
    int m_pdStyle;

    // skip some portion
    bool m_skip;

#if !defined(__SMARTPHONE__)
    // the abort and skip buttons (or NULL if none)
    wxButton *m_btnAbort;
    wxButton *m_btnSkip;
#endif

    // saves the time when elapsed time was updated so there is only one
    // update per second
    unsigned long m_last_timeupdate;

    // tells how often a change of the estimated time has to be confirmed
    // before it is actually displayed - this reduces the frequency of updates
    // of estimated and remaining time
    int m_delay;

    // counts the confirmations
    int m_ctdelay;
    unsigned long m_display_estimated;

    // for wxPD_APP_MODAL case
    wxWindowDisabler *m_winDisabler;

    // Temporary event loop created by the dialog itself if there is no
    // currently active loop when it is created.
    wxEventLoop *m_tempEventLoop;


    DECLARE_EVENT_TABLE()
    wxDECLARE_NO_COPY_CLASS(wxGenericProgressDialog);
};

#endif // __PROGDLGH_G__
