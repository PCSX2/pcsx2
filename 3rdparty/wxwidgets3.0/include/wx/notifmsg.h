///////////////////////////////////////////////////////////////////////////////
// Name:        wx/notifmsg.h
// Purpose:     class allowing to show notification messages to the user
// Author:      Vadim Zeitlin
// Created:     2007-11-19
// Copyright:   (c) 2007 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

#ifndef _WX_NOTIFMSG_H_
#define _WX_NOTIFMSG_H_

#include "wx/event.h"

#if wxUSE_NOTIFICATION_MESSAGE

// ----------------------------------------------------------------------------
// wxNotificationMessage: allows to show the user a message non intrusively
// ----------------------------------------------------------------------------

// notice that this class is not a window and so doesn't derive from wxWindow

class WXDLLIMPEXP_ADV wxNotificationMessageBase : public wxEvtHandler
{
public:
    // ctors and initializers
    // ----------------------

    // default ctor, use setters below to initialize it later
    wxNotificationMessageBase()
    {
        m_parent = NULL;
        m_flags = wxICON_INFORMATION;
    }

    // create a notification object with the given title and message (the
    // latter may be empty in which case only the title will be shown)
    wxNotificationMessageBase(const wxString& title,
                              const wxString& message = wxEmptyString,
                              wxWindow *parent = NULL,
                              int flags = wxICON_INFORMATION)
        : m_title(title),
          m_message(message),
          m_parent(parent)
    {
        SetFlags(flags);
    }

    // note that the setters must be called before Show()

    // set the title: short string, markup not allowed
    void SetTitle(const wxString& title) { m_title = title; }

    // set the text of the message: this is a longer string than the title and
    // some platforms allow simple HTML-like markup in it
    void SetMessage(const wxString& message) { m_message = message; }

    // set the parent for this notification: we'll be associated with the top
    // level parent of this window or, if this method is not called, with the
    // main application window by default
    void SetParent(wxWindow *parent) { m_parent = parent; }

    // this method can currently be used to choose a standard icon to use: the
    // parameter may be one of wxICON_INFORMATION, wxICON_WARNING or
    // wxICON_ERROR only (but not wxICON_QUESTION)
    void SetFlags(int flags)
    {
        wxASSERT_MSG( flags == wxICON_INFORMATION ||
                        flags == wxICON_WARNING || flags == wxICON_ERROR,
                            "Invalid icon flags specified" );

        m_flags = flags;
    }


    // showing and hiding
    // ------------------

    // possible values for Show() timeout
    enum
    {
        Timeout_Auto = -1,  // notification will be hidden automatically
        Timeout_Never = 0   // notification will never time out
    };

    // show the notification to the user and hides it after timeout seconds
    // pass (special values Timeout_Auto and Timeout_Never can be used)
    //
    // returns false if an error occurred
    virtual bool Show(int timeout = Timeout_Auto) = 0;

    // hide the notification, returns true if it was hidden or false if it
    // couldn't be done (e.g. on some systems automatically hidden
    // notifications can't be hidden manually)
    virtual bool Close() = 0;

protected:
    // accessors for the derived classes
    const wxString& GetTitle() const { return m_title; }
    const wxString& GetMessage() const { return m_message; }
    wxWindow *GetParent() const { return m_parent; }
    int GetFlags() const { return m_flags; }

    // return the concatenation of title and message separated by a new line,
    // this is suitable for simple implementation which have no support for
    // separate title and message parts of the notification
    wxString GetFullMessage() const
    {
        wxString text(m_title);
        if ( !m_message.empty() )
        {
            text << "\n\n" << m_message;
        }

        return text;
    }

private:
    wxString m_title,
             m_message;

    wxWindow *m_parent;

    int m_flags;

    wxDECLARE_NO_COPY_CLASS(wxNotificationMessageBase);
};

/*
    TODO: Implement under OS X using notification centre (10.8+) or
          Growl (http://growl.info/) for the previous versions.
 */
#if defined(__WXGTK__) && wxUSE_LIBNOTIFY
    #include "wx/gtk/notifmsg.h"
#elif defined(__WXGTK__) && (wxUSE_LIBHILDON || wxUSE_LIBHILDON2)
    #include "wx/gtk/hildon/notifmsg.h"
#elif defined(__WXMSW__) && wxUSE_TASKBARICON && wxUSE_TASKBARICON_BALLOONS
    #include "wx/msw/notifmsg.h"
#else
    #include "wx/generic/notifmsg.h"

    class wxNotificationMessage : public wxGenericNotificationMessage
    {
    public:
        wxNotificationMessage() { }
        wxNotificationMessage(const wxString& title,
                              const wxString& message = wxEmptyString,
                              wxWindow *parent = NULL,
                              int flags = wxICON_INFORMATION)
            : wxGenericNotificationMessage(title, message, parent, flags)
        {
        }
    };
#endif

#endif // wxUSE_NOTIFICATION_MESSAGE

#endif // _WX_NOTIFMSG_H_

