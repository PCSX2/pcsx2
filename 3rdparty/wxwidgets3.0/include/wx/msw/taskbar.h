/////////////////////////////////////////////////////////////////////////
// File:        wx/msw/taskbar.h
// Purpose:     Defines wxTaskBarIcon class for manipulating icons on the
//              Windows task bar.
// Author:      Julian Smart
// Modified by: Vaclav Slavik
// Created:     24/3/98
// Copyright:   (c) Julian Smart
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////

#ifndef _WX_TASKBAR_H_
#define _WX_TASKBAR_H_

#include "wx/icon.h"

// private helper class:
class WXDLLIMPEXP_FWD_ADV wxTaskBarIconWindow;

class WXDLLIMPEXP_ADV wxTaskBarIcon : public wxTaskBarIconBase
{
public:
    wxTaskBarIcon(wxTaskBarIconType iconType = wxTBI_DEFAULT_TYPE);
    virtual ~wxTaskBarIcon();

    // Accessors
    bool IsOk() const { return true; }
    bool IsIconInstalled() const { return m_iconAdded; }

    // Operations
    bool SetIcon(const wxIcon& icon, const wxString& tooltip = wxEmptyString);
    bool RemoveIcon(void);
    bool PopupMenu(wxMenu *menu);

    // MSW-specific class methods

#if wxUSE_TASKBARICON_BALLOONS
    // show a balloon notification (the icon must have been already initialized
    // using SetIcon)
    //
    // title and text are limited to 63 and 255 characters respectively, msec
    // is the timeout, in milliseconds, before the balloon disappears (will be
    // clamped down to the allowed 10-30s range by Windows if it's outside it)
    // and flags can include wxICON_ERROR/INFO/WARNING to show a corresponding
    // icon
    //
    // return true if balloon was shown, false on error (incorrect parameters
    // or function unsupported by OS)
    bool ShowBalloon(const wxString& title,
                     const wxString& text,
                     unsigned msec = 0,
                     int flags = 0);
#endif // wxUSE_TASKBARICON_BALLOONS

protected:
    friend class wxTaskBarIconWindow;

    long WindowProc(unsigned int msg, unsigned int wParam, long lParam);
    void RegisterWindowMessages();


    wxTaskBarIconWindow *m_win;
    bool                 m_iconAdded;
    wxIcon               m_icon;
    wxString             m_strTooltip;

    DECLARE_DYNAMIC_CLASS_NO_COPY(wxTaskBarIcon)
};

#endif // _WX_TASKBAR_H_
