///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/notifmsg.h
// Purpose:     wxNotificationMessage for wxGTK.
// Author:      Vadim Zeitlin
// Created:     2012-07-25
// Copyright:   (c) 2012 Vadim Zeitlin <vadim@wxwidgets.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_NOTIFMSG_H_
#define _WX_GTK_NOTIFMSG_H_

typedef struct _NotifyNotification NotifyNotification;

// ----------------------------------------------------------------------------
// wxNotificationMessage
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxNotificationMessage : public wxNotificationMessageBase
{
public:
    wxNotificationMessage() { Init(); }
    wxNotificationMessage(const wxString& title,
                          const wxString& message = wxString(),
                          wxWindow *parent = NULL,
                          int flags = wxICON_INFORMATION)
        : wxNotificationMessageBase(title, message, parent, flags)
    {
        Init();
    }

    virtual ~wxNotificationMessage();


    virtual bool Show(int timeout = Timeout_Auto);
    virtual bool Close();

    // Set the name of the icon to use, overriding the default icon determined
    // by the flags. Call with empty string to reset custom icon.
    bool GTKSetIconName(const wxString& name);

private:
    void Init() { m_notification = NULL; }

    NotifyNotification* m_notification;
    wxString m_iconName;

    wxDECLARE_NO_COPY_CLASS(wxNotificationMessage);
};

#endif // _WX_GTK_NOTIFMSG_H_
