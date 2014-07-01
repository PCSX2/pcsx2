/////////////////////////////////////////////////////////////////////////
// File:        wx/mac/taskbarosx.h
// Purpose:     Defines wxTaskBarIcon class for OSX
// Author:      Ryan Norton
// Modified by:
// Created:     04/04/2003
// Copyright:   (c) Ryan Norton, 2003
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////

#ifndef _TASKBAR_H_
#define _TASKBAR_H_

class WXDLLIMPEXP_FWD_CORE wxIcon;
class WXDLLIMPEXP_FWD_CORE wxMenu;

class WXDLLIMPEXP_ADV wxTaskBarIcon : public wxTaskBarIconBase
{
    DECLARE_DYNAMIC_CLASS_NO_COPY(wxTaskBarIcon)
public:
    wxTaskBarIcon(wxTaskBarIconType iconType = wxTBI_DEFAULT_TYPE);
    virtual ~wxTaskBarIcon();

    // returns true if the taskbaricon is in the global menubar
#if wxOSX_USE_COCOA
    bool OSXIsStatusItem();
#else
    bool OSXIsStatusItem() { return false; }
#endif
    bool IsOk() const { return true; }

    bool IsIconInstalled() const;
    bool SetIcon(const wxIcon& icon, const wxString& tooltip = wxEmptyString);
    bool RemoveIcon();
    bool PopupMenu(wxMenu *menu);

protected:
    wxTaskBarIconType m_type;
    class wxTaskBarIconImpl* m_impl;
    friend class wxTaskBarIconImpl;
};
#endif
    // _TASKBAR_H_
