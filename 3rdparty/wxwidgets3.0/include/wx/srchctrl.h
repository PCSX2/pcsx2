/////////////////////////////////////////////////////////////////////////////
// Name:        wx/srchctrl.h
// Purpose:     wxSearchCtrlBase class
// Author:      Vince Harron
// Created:     2006-02-18
// Copyright:   (c) Vince Harron
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_SEARCHCTRL_H_BASE_
#define _WX_SEARCHCTRL_H_BASE_

#include "wx/defs.h"

#if wxUSE_SEARCHCTRL

#include "wx/textctrl.h"

#if !defined(__WXUNIVERSAL__) && defined(__WXMAC__)
    // search control was introduced in Mac OS X 10.3 Panther
    #define wxUSE_NATIVE_SEARCH_CONTROL 1

    #define wxSearchCtrlBaseBaseClass wxTextCtrl
#else
    // no native version, use the generic one
    #define wxUSE_NATIVE_SEARCH_CONTROL 0

    #include "wx/compositewin.h"
    #include "wx/containr.h"

    class WXDLLIMPEXP_CORE wxSearchCtrlBaseBaseClass
        : public wxCompositeWindow< wxNavigationEnabled<wxControl> >,
          public wxTextCtrlIface
    {
    };
#endif

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

extern WXDLLIMPEXP_DATA_CORE(const char) wxSearchCtrlNameStr[];

wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SEARCHCTRL_CANCEL_BTN, wxCommandEvent);
wxDECLARE_EXPORTED_EVENT(WXDLLIMPEXP_CORE, wxEVT_SEARCHCTRL_SEARCH_BTN, wxCommandEvent);

// ----------------------------------------------------------------------------
// a search ctrl is a text control with a search button and a cancel button
// it is based on the MacOSX 10.3 control HISearchFieldCreate
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxSearchCtrlBase : public wxSearchCtrlBaseBaseClass
{
public:
    wxSearchCtrlBase() { }
    virtual ~wxSearchCtrlBase() { }

    // search control
#if wxUSE_MENUS
    virtual void SetMenu(wxMenu *menu) = 0;
    virtual wxMenu *GetMenu() = 0;
#endif // wxUSE_MENUS

    // get/set options
    virtual void ShowSearchButton( bool show ) = 0;
    virtual bool IsSearchButtonVisible() const = 0;

    virtual void ShowCancelButton( bool show ) = 0;
    virtual bool IsCancelButtonVisible() const = 0;

private:
    // implement wxTextEntry pure virtual method
    virtual wxWindow *GetEditableWindow() { return this; }
};


// include the platform-dependent class implementation
#if wxUSE_NATIVE_SEARCH_CONTROL
    #if defined(__WXMAC__)
        #include "wx/osx/srchctrl.h"
    #endif
#else
    #include "wx/generic/srchctlg.h"
#endif

// ----------------------------------------------------------------------------
// macros for handling search events
// ----------------------------------------------------------------------------

#define EVT_SEARCHCTRL_CANCEL_BTN(id, fn) \
    wx__DECLARE_EVT1(wxEVT_SEARCHCTRL_CANCEL_BTN, id, wxCommandEventHandler(fn))

#define EVT_SEARCHCTRL_SEARCH_BTN(id, fn) \
    wx__DECLARE_EVT1(wxEVT_SEARCHCTRL_SEARCH_BTN, id, wxCommandEventHandler(fn))

// old wxEVT_COMMAND_* constants
#define wxEVT_COMMAND_SEARCHCTRL_CANCEL_BTN   wxEVT_SEARCHCTRL_CANCEL_BTN
#define wxEVT_COMMAND_SEARCHCTRL_SEARCH_BTN   wxEVT_SEARCHCTRL_SEARCH_BTN

#endif // wxUSE_SEARCHCTRL

#endif // _WX_SEARCHCTRL_H_BASE_
