///////////////////////////////////////////////////////////////////////////////
// Name:        src/generic/notifmsgg.cpp
// Purpose:     generic implementation of wxGenericNotificationMessage
// Author:      Vadim Zeitlin
// Created:     2007-11-24
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
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

#ifndef wxUSE_LIBHILDON
    #define wxUSE_LIBHILDON 0
#endif

#ifndef wxUSE_LIBHILDON2
    #define wxUSE_LIBHILDON2 0
#endif

#if wxUSE_NOTIFICATION_MESSAGE && (!wxUSE_LIBHILDON || !wxUSE_LIBHILDON2)

#ifndef WX_PRECOMP
    #include "wx/dialog.h"
    #include "wx/timer.h"
    #include "wx/sizer.h"
    #include "wx/statbmp.h"
#endif //WX_PRECOMP

#include "wx/artprov.h"

// even if the platform has the native implementation, we still normally want
// to use the generic one (unless it's totally unsuitable for the target UI as
// is the case of Hildon) because it may provide more features, so include
// wx/generic/notifmsg.h to get wxGenericNotificationMessage declaration even
// if wx/notifmsg.h only declares wxNotificationMessage itself (if it already
// uses the generic version, the second inclusion will do no harm)
#include "wx/notifmsg.h"
#include "wx/generic/notifmsg.h"

// ----------------------------------------------------------------------------
// wxNotificationMessageDialog
// ----------------------------------------------------------------------------

class wxNotificationMessageDialog : public wxDialog
{
public:
    wxNotificationMessageDialog(wxWindow *parent,
                                const wxString& text,
                                int timeout,
                                int flags);

    void Set(wxWindow *parent,
             const wxString& text,
             int timeout,
             int flags);

    bool IsAutomatic() const { return m_timer.IsRunning(); }
    void SetDeleteOnHide() { m_deleteOnHide = true; }

private:
    void OnClose(wxCloseEvent& event);
    void OnTimer(wxTimerEvent& event);

    // if true, delete the dialog when it should disappear, otherwise just hide
    // it (initially false)
    bool m_deleteOnHide;

    // timer which will hide this dialog when it expires, if it's not running
    // it means we were created without timeout
    wxTimer m_timer;


    DECLARE_EVENT_TABLE()
    wxDECLARE_NO_COPY_CLASS(wxNotificationMessageDialog);
};

// ============================================================================
// wxNotificationMessageDialog implementation
// ============================================================================

BEGIN_EVENT_TABLE(wxNotificationMessageDialog, wxDialog)
    EVT_CLOSE(wxNotificationMessageDialog::OnClose)

    EVT_TIMER(wxID_ANY, wxNotificationMessageDialog::OnTimer)
END_EVENT_TABLE()

wxNotificationMessageDialog::wxNotificationMessageDialog(wxWindow *parent,
                                                         const wxString& text,
                                                         int timeout,
                                                         int flags)
                           : wxDialog(parent, wxID_ANY, _("Notice"),
                                      wxDefaultPosition, wxDefaultSize,
                                      0 /* no caption, no border styles */),
                             m_timer(this)
{
    m_deleteOnHide = false;

    Set(parent, text, timeout, flags);
}

void
wxNotificationMessageDialog::Set(wxWindow * WXUNUSED(parent),
                                 const wxString& text,
                                 int timeout,
                                 int flags)
{
    wxSizer * const sizerTop = new wxBoxSizer(wxHORIZONTAL);
    if ( flags & wxICON_MASK )
    {
        sizerTop->Add(new wxStaticBitmap
                          (
                            this,
                            wxID_ANY,
                            wxArtProvider::GetMessageBoxIcon(flags)
                          ),
                      wxSizerFlags().Centre().Border());
    }

    sizerTop->Add(CreateTextSizer(text), wxSizerFlags(1).Border());
    SetSizerAndFit(sizerTop);

    if ( timeout != wxGenericNotificationMessage::Timeout_Never )
    {
        // wxTimer uses ms, timeout is in seconds
        m_timer.Start(timeout*1000, true /* one shot only */);
    }
    else if ( m_timer.IsRunning() )
    {
        m_timer.Stop();
    }
}

void wxNotificationMessageDialog::OnClose(wxCloseEvent& event)
{
    if ( m_deleteOnHide )
    {
        // we don't need to keep this dialog alive any more
        Destroy();
    }
    else // don't really close, just hide, as we can be shown again later
    {
        event.Veto();

        Hide();
    }
}

void wxNotificationMessageDialog::OnTimer(wxTimerEvent& WXUNUSED(event))
{
    if ( m_deleteOnHide )
        Destroy();
    else
        Hide();
}

// ============================================================================
// wxGenericNotificationMessage implementation
// ============================================================================

int wxGenericNotificationMessage::ms_timeout = 10;

/* static */ void wxGenericNotificationMessage::SetDefaultTimeout(int timeout)
{
    wxASSERT_MSG( timeout > 0,
                "negative or zero default timeout doesn't make sense" );

    ms_timeout = timeout;
}

void wxGenericNotificationMessage::Init()
{
    m_dialog = NULL;
}

wxGenericNotificationMessage::~wxGenericNotificationMessage()
{
    if ( m_dialog->IsAutomatic() )
    {
        // we want to allow the user to create an automatically hidden
        // notification just by creating a local wxGenericNotificationMessage object
        // and so we shouldn't hide the notification when this object goes out
        // of scope
        m_dialog->SetDeleteOnHide();
    }
    else // manual dialog, hide it immediately
    {
        // OTOH for permanently shown dialogs only the code can hide them and
        // if the object is deleted, we must do it now as it won't be
        // accessible programmatically any more
        delete m_dialog;
    }
}

bool wxGenericNotificationMessage::Show(int timeout)
{
    if ( timeout == Timeout_Auto )
    {
        timeout = GetDefaultTimeout();
    }

    if ( !m_dialog )
    {
        m_dialog = new wxNotificationMessageDialog
                       (
                        GetParent(),
                        GetFullMessage(),
                        timeout,
                        GetFlags()
                       );
    }
    else // update the existing dialog
    {
        m_dialog->Set(GetParent(), GetFullMessage(), timeout, GetFlags());
    }

    m_dialog->Show();

    return true;
}

bool wxGenericNotificationMessage::Close()
{
    if ( !m_dialog )
        return false;

    m_dialog->Hide();

    return true;
}

#endif // wxUSE_NOTIFICATION_MESSAGE && (!wxUSE_LIBHILDON || !wxUSE_LIBHILDON2)
