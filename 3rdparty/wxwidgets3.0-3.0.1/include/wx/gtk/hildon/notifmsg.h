///////////////////////////////////////////////////////////////////////////////
// Name:        wx/gtk/hildon/notifmsg.h
// Purpose:     Hildon implementation of wxNotificationMessage
// Author:      Vadim Zeitlin
// Created:     2007-11-21
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_GTK_HILDON_NOTIFMSG_H_
#define _WX_GTK_HILDON_NOTIFMSG_H_

typedef struct _HildonBanner HildonBanner;

// ----------------------------------------------------------------------------
// wxNotificationMessage
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_ADV wxNotificationMessage : public wxNotificationMessageBase
{
public:
    wxNotificationMessage() { Init(); }
    wxNotificationMessage(const wxString& title,
                          const wxString& message = wxString(),
                          wxWindow *parent = NULL)
        : wxNotificationMessageBase(title, message, parent)
    {
        Init();
    }

    virtual ~wxNotificationMessage();


    virtual bool Show(int timeout = Timeout_Auto);
    virtual bool Close();

private:
    void Init() { m_banner = NULL; }

    // return the string containing markup for both the title and, if
    // specified, the message
    wxString HildonGetMarkup() const;

    // returns the widget of the parent GtkWindow to use or NULL
    GtkWidget *HildonGetWindow() const;


    // the banner we're showing, only non-NULL if it's an animation or progress
    // banner as the informational dialog times out on its own and we don't
    // need to store it (nor do we have any way to get its widget anyhow)
    GtkWidget *m_banner;


    wxDECLARE_NO_COPY_CLASS(wxNotificationMessage);
};

#endif // _WX_GTK_HILDON_NOTIFMSG_H_

