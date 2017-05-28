/////////////////////////////////////////////////////////////////////////
// File:        src/common/taskbarcmn.cpp
// Purpose:     Common parts of wxTaskBarIcon class
// Author:      Julian Smart
// Modified by:
// Created:     04/04/2003
// Copyright:   (c) Julian Smart, 2003
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_TASKBARICON

#include "wx/taskbar.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/list.h"
    #include "wx/menu.h"
#endif

extern WXDLLIMPEXP_DATA_CORE(wxList) wxPendingDelete;

// DLL options compatibility check:
WX_CHECK_BUILD_OPTIONS("wxAdvanced")

wxDEFINE_EVENT( wxEVT_TASKBAR_MOVE, wxTaskBarIconEvent );
wxDEFINE_EVENT( wxEVT_TASKBAR_LEFT_DOWN, wxTaskBarIconEvent );
wxDEFINE_EVENT( wxEVT_TASKBAR_LEFT_UP, wxTaskBarIconEvent );
wxDEFINE_EVENT( wxEVT_TASKBAR_RIGHT_DOWN, wxTaskBarIconEvent );
wxDEFINE_EVENT( wxEVT_TASKBAR_RIGHT_UP, wxTaskBarIconEvent );
wxDEFINE_EVENT( wxEVT_TASKBAR_LEFT_DCLICK, wxTaskBarIconEvent );
wxDEFINE_EVENT( wxEVT_TASKBAR_RIGHT_DCLICK, wxTaskBarIconEvent );
wxDEFINE_EVENT( wxEVT_TASKBAR_BALLOON_TIMEOUT, wxTaskBarIconEvent );
wxDEFINE_EVENT( wxEVT_TASKBAR_BALLOON_CLICK, wxTaskBarIconEvent );


wxBEGIN_EVENT_TABLE(wxTaskBarIconBase, wxEvtHandler)
    EVT_TASKBAR_CLICK(wxTaskBarIconBase::OnRightButtonDown)
wxEND_EVENT_TABLE()

void wxTaskBarIconBase::OnRightButtonDown(wxTaskBarIconEvent& WXUNUSED(event))
{
    wxMenu *menu = CreatePopupMenu();
    if (menu)
    {
        PopupMenu(menu);
        delete menu;
    }
}

void wxTaskBarIconBase::Destroy()
{
    wxPendingDelete.Append(this);
}

#endif // wxUSE_TASKBARICON
