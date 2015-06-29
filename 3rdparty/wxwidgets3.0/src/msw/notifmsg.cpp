///////////////////////////////////////////////////////////////////////////////
// Name:        src/msw/notifmsg.cpp
// Purpose:     implementation of wxNotificationMessage for Windows
// Author:      Vadim Zeitlin
// Created:     2007-12-01
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

// we can only use the native implementation if we have a working
// wxTaskBarIcon::ShowBalloon() method
#if wxUSE_NOTIFICATION_MESSAGE && \
        wxUSE_TASKBARICON && wxUSE_TASKBARICON_BALLOONS

#include "wx/notifmsg.h"

#ifndef WX_PRECOMP
    #include "wx/toplevel.h"
    #include "wx/app.h"
    #include "wx/string.h"
#endif // WX_PRECOMP

#include "wx/generic/notifmsg.h"

#include "wx/taskbar.h"

// ----------------------------------------------------------------------------
// different implementations used by wxNotificationMessage
// ----------------------------------------------------------------------------

// base class for all available implementations
class wxNotifMsgImpl
{
public:
    wxNotifMsgImpl() { }
    virtual ~wxNotifMsgImpl() { }

    virtual bool DoShow(const wxString& title,
                        const wxString& message,
                        int timeout,
                        int flags) = 0;
    virtual bool DoClose() = 0;

private:
    wxDECLARE_NO_COPY_CLASS(wxNotifMsgImpl);
};

// implementation which is simply a bridge to wxGenericNotificationMessage
class wxGenericNotifMsgImpl : public wxNotifMsgImpl
{
public:
    wxGenericNotifMsgImpl() : m_notif(new wxGenericNotificationMessage) { }
    virtual ~wxGenericNotifMsgImpl() { delete m_notif; }

    virtual bool DoShow(const wxString& title,
                        const wxString& message,
                        int timeout,
                        int flags)
    {
        m_notif->SetTitle(title);
        m_notif->SetMessage(message);
        m_notif->SetFlags(flags);
        return m_notif->Show(timeout);
    }

    virtual bool DoClose()
    {
        return m_notif->Close();
    }

private:
    wxGenericNotificationMessage * const m_notif;
};

// common base class for implementations using a taskbar icon and balloons
class wxBalloonNotifMsgImpl : public wxNotifMsgImpl
{
public:
    // Ctor creates the associated taskbar icon (using the icon of the top
    // level parent of the given window) unless UseTaskBarIcon() had been
    // previously called  which can be used to show an attached balloon later
    // by the derived classes.
    wxBalloonNotifMsgImpl(wxWindow *win) { SetUpIcon(win); }

    // implementation of wxNotificationMessage method with the same name
    static wxTaskBarIcon *UseTaskBarIcon(wxTaskBarIcon *icon);

    virtual bool DoShow(const wxString& title,
                        const wxString& message,
                        int timeout,
                        int flags);


    // Returns true if we're using our own icon or false if we're hitching a
    // ride on the application icon provided to us via UseTaskBarIcon().
    static bool IsUsingOwnIcon()
    {
        return ms_refCountIcon != -1;
    }

    // Indicates that the taskbar icon we're using has been hidden and can be
    // deleted.
    //
    // This is only called by wxNotificationIconEvtHandler and should only be
    // called when using our own icon (as opposed to the one passed to us via
    // UseTaskBarIcon()).
    static void ReleaseIcon()
    {
        wxASSERT_MSG( ms_refCountIcon != -1,
                      wxS("Must not be called when not using own icon") );

        if ( !--ms_refCountIcon )
        {
            delete ms_icon;
            ms_icon = NULL;
        }
    }

protected:
    // Creates a new icon if necessary, see the comment below.
    void SetUpIcon(wxWindow *win);


    // We need an icon to show the notification in a balloon attached to it.
    // It may happen that the main application already shows an icon in the
    // taskbar notification area in which case it should call our
    // UseTaskBarIcon() and we just use this icon without ever allocating nor
    // deleting it and ms_refCountIcon is -1 and never changes. Otherwise, we
    // create the icon when we need it the first time but reuse it if we need
    // to show subsequent notifications while this icon is still alive. This is
    // needed in order to avoid 2 or 3 or even more identical icons if a couple
    // of notifications are shown in a row (which happens quite easily in
    // practice because Windows helpfully buffers all the notifications that
    // were generated while the user was away -- i.e. the screensaver was
    // active -- and then shows them all at once when the user comes back). In
    // this case, ms_refCountIcon is used as a normal reference counter, i.e.
    // the icon is only destroyed when it reaches 0.
    static wxTaskBarIcon *ms_icon;
    static int ms_refCountIcon;
};

// implementation for automatically hidden notifications
class wxAutoNotifMsgImpl : public wxBalloonNotifMsgImpl
{
public:
    wxAutoNotifMsgImpl(wxWindow *win);

    virtual bool DoShow(const wxString& title,
                        const wxString& message,
                        int timeout,
                        int flags);

    // can't close automatic notification [currently]
    virtual bool DoClose() { return false; }
};

// implementation for manually closed notifications
class wxManualNotifMsgImpl : public wxBalloonNotifMsgImpl
{
public:
    wxManualNotifMsgImpl(wxWindow *win);
    virtual ~wxManualNotifMsgImpl();

    virtual bool DoShow(const wxString& title,
                        const wxString& message,
                        int timeout,
                        int flags);
    virtual bool DoClose();

private:
    // store ctor parameter as we need it to recreate the icon later if we're
    // closed and shown again
    wxWindow * const m_win;
};

// ----------------------------------------------------------------------------
// custom event handler for task bar icons
// ----------------------------------------------------------------------------

// normally we'd just use a custom taskbar icon class but this is impossible
// because we can be asked to attach the notifications to an existing icon
// which we didn't create, hence we install a special event handler allowing us
// to get the events we need (and, crucially, to delete the icon when it's not
// needed any more) in any case

class wxNotificationIconEvtHandler : public wxEvtHandler
{
public:
    wxNotificationIconEvtHandler(wxTaskBarIcon *icon);

private:
    void OnTimeout(wxTaskBarIconEvent& event);
    void OnClick(wxTaskBarIconEvent& event);

    void OnIconHidden();


    wxTaskBarIcon * const m_icon;

    wxDECLARE_NO_COPY_CLASS(wxNotificationIconEvtHandler);
};

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// wxNotificationIconEvtHandler
// ----------------------------------------------------------------------------

wxNotificationIconEvtHandler::wxNotificationIconEvtHandler(wxTaskBarIcon *icon)
                            : m_icon(icon)
{
    m_icon->Connect
            (
              wxEVT_TASKBAR_BALLOON_TIMEOUT,
              wxTaskBarIconEventHandler(wxNotificationIconEvtHandler::OnTimeout),
              NULL,
              this
            );

    m_icon->Connect
            (
              wxEVT_TASKBAR_BALLOON_CLICK,
              wxTaskBarIconEventHandler(wxNotificationIconEvtHandler::OnClick),
              NULL,
              this
            );
}

void wxNotificationIconEvtHandler::OnIconHidden()
{
    wxBalloonNotifMsgImpl::ReleaseIcon();

    delete this;
}

void
wxNotificationIconEvtHandler::OnTimeout(wxTaskBarIconEvent& WXUNUSED(event))
{
    OnIconHidden();
}

void wxNotificationIconEvtHandler::OnClick(wxTaskBarIconEvent& WXUNUSED(event))
{
    // TODO: generate an event notifying the user code?

    OnIconHidden();
}

// ----------------------------------------------------------------------------
// wxBalloonNotifMsgImpl
// ----------------------------------------------------------------------------

wxTaskBarIcon *wxBalloonNotifMsgImpl::ms_icon = NULL;
int wxBalloonNotifMsgImpl::ms_refCountIcon = 0;

/* static */
wxTaskBarIcon *wxBalloonNotifMsgImpl::UseTaskBarIcon(wxTaskBarIcon *icon)
{
    wxTaskBarIcon * const iconOld = ms_icon;
    ms_icon = icon;

    // Don't use reference counting for the provided icon, we don't own it.
    ms_refCountIcon = icon ? -1 : 0;

    return iconOld;
}

void wxBalloonNotifMsgImpl::SetUpIcon(wxWindow *win)
{
    if ( ms_icon )
    {
        // Increment the reference count if we manage the icon on our own.
        if ( ms_refCountIcon != -1 )
            ms_refCountIcon++;
    }
    else // Create a new icon.
    {
        wxASSERT_MSG( ms_refCountIcon == 0,
                      wxS("Shouldn't reference not existent icon") );

        ms_icon = new wxTaskBarIcon;
        ms_refCountIcon = 1;

        // use the icon of the associated (or main, if none) frame
        wxIcon icon;
        if ( win )
            win = wxGetTopLevelParent(win);
        if ( !win )
            win = wxTheApp->GetTopWindow();
        if ( win )
        {
            const wxTopLevelWindow * const
                tlw = wxDynamicCast(win, wxTopLevelWindow);
            if ( tlw )
                icon = tlw->GetIcon();
        }

        if ( !icon.IsOk() )
        {
            // we really must have some icon
            icon = wxIcon(wxT("wxICON_AAA"));
        }

        ms_icon->SetIcon(icon);
    }
}

bool
wxBalloonNotifMsgImpl::DoShow(const wxString& title,
                              const wxString& message,
                              int timeout,
                              int flags)
{
    if ( !ms_icon->IsIconInstalled() )
    {
        // If we failed to install the icon (which does happen sometimes,
        // although only in unusual circumstances, e.g. it happens regularly,
        // albeit not constantly, if we're used soon after resume from suspend
        // under Windows 7), we should not call ShowBalloon() because it would
        // just assert and return and we must delete the icon ourselves because
        // otherwise its associated wxTaskBarIconWindow would remain alive
        // forever because we're not going to receive a notification about icon
        // disappearance from the system if we failed to install it in the
        // first place.
        delete ms_icon;
        ms_icon = NULL;

        return false;
    }

    timeout *= 1000; // Windows expresses timeout in milliseconds

    return ms_icon->ShowBalloon(title, message, timeout, flags);
}

// ----------------------------------------------------------------------------
// wxManualNotifMsgImpl
// ----------------------------------------------------------------------------

wxManualNotifMsgImpl::wxManualNotifMsgImpl(wxWindow *win)
                    : wxBalloonNotifMsgImpl(win),
                      m_win(win)
{
}

wxManualNotifMsgImpl::~wxManualNotifMsgImpl()
{
    if ( ms_icon )
        DoClose();
}

bool
wxManualNotifMsgImpl::DoShow(const wxString& title,
                             const wxString& message,
                             int WXUNUSED_UNLESS_DEBUG(timeout),
                             int flags)
{
    wxASSERT_MSG( timeout == wxNotificationMessage::Timeout_Never,
                    wxT("shouldn't be used") );

    // base class creates the icon for us initially but we could have destroyed
    // it in DoClose(), recreate it if this was the case
    if ( !ms_icon )
        SetUpIcon(m_win);

    // use maximal (in current Windows versions) timeout (but it will still
    // disappear on its own)
    return wxBalloonNotifMsgImpl::DoShow(title, message, 30, flags);
}

bool wxManualNotifMsgImpl::DoClose()
{
    if ( IsUsingOwnIcon() )
    {
        // we don't need the icon any more
        ReleaseIcon();
    }
    else // using an existing icon
    {
        // just hide the balloon
        ms_icon->ShowBalloon("", "");
    }

    return true;
}

// ----------------------------------------------------------------------------
// wxAutoNotifMsgImpl
// ----------------------------------------------------------------------------

wxAutoNotifMsgImpl::wxAutoNotifMsgImpl(wxWindow *win)
                  : wxBalloonNotifMsgImpl(win)
{
    if ( ms_refCountIcon != -1 )
    {
        // This object will self-destruct and decrease the ref count of the
        // icon when the notification is hidden.
        new wxNotificationIconEvtHandler(ms_icon);
    }
}

bool
wxAutoNotifMsgImpl::DoShow(const wxString& title,
                           const wxString& message,
                           int timeout,
                           int flags)
{
    wxASSERT_MSG( timeout != wxNotificationMessage::Timeout_Never,
                    wxT("shouldn't be used") );

    if ( timeout == wxNotificationMessage::Timeout_Auto )
    {
        // choose a value more or less in the middle of the allowed range
        timeout = 1;
    }

    return wxBalloonNotifMsgImpl::DoShow(title, message, timeout, flags);
}

// ----------------------------------------------------------------------------
// wxNotificationMessage
// ----------------------------------------------------------------------------

/* static */
bool wxNotificationMessage::ms_alwaysUseGeneric = false;

/* static */
wxTaskBarIcon *wxNotificationMessage::UseTaskBarIcon(wxTaskBarIcon *icon)
{
    return wxBalloonNotifMsgImpl::UseTaskBarIcon(icon);
}

bool wxNotificationMessage::Show(int timeout)
{
    if ( !m_impl )
    {
        if ( !ms_alwaysUseGeneric && wxTheApp->GetShell32Version() >= 500 )
        {
            if ( timeout == Timeout_Never )
                m_impl = new wxManualNotifMsgImpl(GetParent());
            else
                m_impl = new wxAutoNotifMsgImpl(GetParent());
        }
        else // no support for balloon tooltips
        {
            m_impl = new wxGenericNotifMsgImpl;
        }
    }
    //else: reuse the same implementation for the subsequent calls, it would
    //      be too confusing if it changed

    return m_impl->DoShow(GetTitle(), GetMessage(), timeout, GetFlags());
}

bool wxNotificationMessage::Close()
{
    wxCHECK_MSG( m_impl, false, "must show the notification first" );

    return m_impl->DoClose();
}

wxNotificationMessage::~wxNotificationMessage()
{
    delete m_impl;
}

#endif // wxUSE_NOTIFICATION_MESSAGE && wxUSE_TASKBARICON
