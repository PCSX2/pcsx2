///////////////////////////////////////////////////////////////////////////////
// Name:        wx/msw/notifmsg.h
// Purpose:     implementation of wxNotificationMessage for Windows
// Author:      Vadim Zeitlin
// Created:     2007-12-01
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_MSW_NOTIFMSG_H_
#define _WX_MSW_NOTIFMSG_H_

class WXDLLIMPEXP_FWD_ADV wxTaskBarIcon;

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

    // MSW implementation-specific methods

    // by default, wxNotificationMessage under MSW creates a temporary taskbar
    // icon to which it attaches the notification, if there is an existing
    // taskbar icon object in the application you may want to call this method
    // to attach the notification to it instead (we won't take ownership of it
    // and you can also pass NULL to not use the icon for notifications any
    // more)
    //
    // returns the task bar icon which was used previously (may be NULL)
    static wxTaskBarIcon *UseTaskBarIcon(wxTaskBarIcon *icon);

    // call this to always use the generic implementation, even if the system
    // supports the balloon tooltips used by the native one
    static void AlwaysUseGeneric(bool alwaysUseGeneric)
    {
        ms_alwaysUseGeneric = alwaysUseGeneric;
    }

private:
    // common part of all ctors
    void Init() { m_impl = NULL; }


    // flag indicating whether we should always use generic implementation
    static bool ms_alwaysUseGeneric;

    // the real implementation of this class (selected during run-time because
    // the balloon task bar icons are not available in all Windows versions)
    class wxNotifMsgImpl *m_impl;


    wxDECLARE_NO_COPY_CLASS(wxNotificationMessage);
};

#endif // _WX_MSW_NOTIFMSG_H_

